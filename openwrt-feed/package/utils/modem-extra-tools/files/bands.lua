-- SPDX-License-Identifier: Apache-2.0
-- QMI DMS discovers LTE capabilities; QMI NAS changes only LTE preference.
local c, fs = require 'common', require 'nixio.fs'
local B={}
local backup=c.directory .. '/band-original.json'
local pending=c.directory .. '/band-pending.json'
local last=c.directory .. '/band-previous.json'
local desired=c.directory .. '/band-desired.json'
local cache=c.runtime .. '/bands.json'

function B.hex(value)
  return type(value)=='string' and #value==16 and not value:find('[^0-9a-f]')
end
function B.list(value)
  c.need(B.hex(value),'invalid 64-bit LTE mask')
  local result={}
  for band=1,64 do
    local offset=math.floor((band-1)/8)*2+1
    local byte=tonumber(value:sub(offset,offset+1),16)
    if math.floor(byte/2^((band-1)%8))%2==1 then result[#result+1]=band end
  end
  return result
end
function B.mask(bands)
  local bytes={0,0,0,0,0,0,0,0}; local result={}
  for _,band in ipairs(bands) do
    local i=math.floor((band-1)/8)+1
    bytes[i]=bytes[i]+2^((band-1)%8)
  end
  for i=1,8 do result[i]=('%02x'):format(bytes[i]) end
  return table.concat(result)
end
local function bit(mask,band)
  local offset=math.floor((band-1)/8)*2+1
  local byte=tonumber(mask:sub(offset,offset+1),16)
  return math.floor(byte/2^((band-1)%8))%2==1
end
local function subset(mask,capability)
  for _,band in ipairs(B.list(mask)) do if not bit(capability,band) then return false end end
  return true
end
function B.parse(text,capability)
  c.need(type(text)=='string' and text:match('^%d[%d,]*$') and not text:find(',,',1,true)
    and text:sub(-1)~=',' ,'expected a comma-separated LTE band list')
  if capability then c.need(B.hex(capability),'invalid LTE capability mask') end
  local result,seen={},{}
  for token in text:gmatch('[^,]+') do
    local band=c.uint(token,1,64,'LTE band')
    c.need(not capability or bit(capability,band),'band is not supported by this modem: B' .. band)
    c.need(not seen[band],'duplicate band: B' .. band)
    result[#result+1]=band; seen[band]=true
  end
  c.need(#result>0,'select at least one LTE band')
  c.need(not (#result==1 and result[1]==32),'B32 is supplementary downlink; select an anchor band too')
  table.sort(result); return result
end
local function known(mask,capability)
  c.need(B.hex(mask),'invalid 64-bit LTE preference')
  B.parse(table.concat(B.list(mask),','),capability)
  return mask
end
local function record(mask)
  return {schema=3,board=c.board(),mask=known(mask)}
end
local function checked(value,capability)
  c.need(type(value)=='table' and B.hex(value.mask),'missing or corrupt LTE restore point')
  if value.schema==3 then
    c.need(value.board==c.board(),'restore point belongs to a different OpenWrt board')
  elseif value.schema==2 then
    c.need(value.model==c.model(),'legacy restore point belongs to a different router model')
  else error('unsupported LTE restore point schema',0) end
  return known(value.mask,capability)
end
local function read(q)
  local mask=q:run(q.helper .. ' get')
  c.need(B.hex(mask),'invalid QMI LTE preference reply')
  return mask
end
local function capabilities(q)
  local mask=q:run(q.helper .. ' capabilities')
  c.need(B.hex(mask) and #B.list(mask)>0,'invalid QMI LTE capability reply')
  return mask
end
local function managed(state,capability)
  local saved=c.json(desired)
  state.managed=saved~=nil
  if saved then
    local ok,value=pcall(checked,saved,capability)
    if ok then state.desired_bands=B.list(value) else state.desired_error=tostring(value) end
  end
  return state
end
local function describe(mask,capability)
  local state={ok=true,refreshed=os.time(),current_bands=B.list(mask),
    supported_bands=B.list(capability),capability_source='qmi-dms',backend='qmi-nas',
    backup_present=false,pending=fs.access(pending) and true or false}
  state.editable=pcall(known,mask,capability)
  local saved=c.json(backup)
  if saved then
    local ok,value=pcall(checked,saved,capability)
    state.backup_present=ok
    if ok then state.backup_bands=B.list(value) else state.backup_error=tostring(value) end
  end
  return managed(state,capability)
end
function B.cached()
  local state=c.json(cache) or {ok=true,unread=true,supported_bands={},
    capability_source='unread',backend='qmi-nas'}
  state.pending=fs.access(pending) and true or false
  return managed(state)
end
local function save_desired(value)
  if value then c.atomic(desired,value) else fs.unlink(desired); c.need(c.exec('sync'),'sync failed') end
end
local function transaction(q,before,target,recovering,next_desired)
  if not recovering then
    c.atomic(pending,{schema=3,before=record(before),target=record(target),
      desired_before=c.json(desired) or false,desired_after=next_desired or false})
  end
  if read(q)~=before then
    if not recovering then fs.unlink(pending); c.exec('sync') end
    error('LTE preference changed concurrently; no write performed',0)
  end
  local ok,err=pcall(function()
    q:run(q.helper .. ' set ' .. target,12)
    c.need(read(q)==target,'QMI readback mismatch')
  end)
  if not ok then
    local restored=pcall(function()
      q:run(q.helper .. ' set ' .. before,12)
      c.need(read(q)==before,'rollback readback mismatch')
    end)
    if restored and not recovering then fs.unlink(pending); c.exec('sync') end
    error(tostring(err) .. (restored and '; previous preference restored' or '; recovery required'),0)
  end
  c.atomic(last,record(before))
  save_desired(next_desired)
  fs.unlink(pending); c.exec('sync')
end
function B.execute(operation,text)
  c.board()
  local lock=c.lock('bands')
  fs.mkdirr(c.directory)
  local q
  local ok,result=pcall(function()
    c.need(not fs.access(pending) or operation=='show' or operation=='recover',
      'an interrupted transaction exists; run bands show, then bands recover')
    if operation=='reconcile' and not fs.access(desired) then return B.cached() end
    q=require('qualcomm').open('nas')
    local capability=capabilities(q)
    local selection=operation=='set' and B.parse(text,capability) or nil
    local before=read(q)
    if operation=='show' then return describe(before,capability) end
    known(before,capability)
    if operation=='backup' then
      c.need(not fs.access(backup),'restore point already exists; it will not be overwritten')
      c.atomic(backup,record(before)); return describe(before,capability)
    end
    local target,next_desired
    if operation=='set' then
      if not fs.access(backup) then c.atomic(backup,record(before)) else checked(c.json(backup),capability) end
      target=B.mask(selection); next_desired=record(target)
    elseif operation=='restore' then target=checked(c.json(backup),capability)
    elseif operation=='undo' then target=checked(c.json(last),capability); next_desired=record(target)
    elseif operation=='reconcile' then
      checked(c.json(backup),capability)
      next_desired=c.json(desired); target=checked(next_desired,capability)
    elseif operation=='recover' then
      local journal=c.need(c.json(pending),'no pending transaction')
      c.need(journal.schema==2 or journal.schema==3,'unsupported pending transaction schema')
      target=checked(journal.before,capability); next_desired=journal.desired_before
      if next_desired then checked(next_desired,capability) end
    else error('unknown band operation',0) end
    c.need(subset(target,capability),'target LTE preference exceeds modem capabilities')
    if before~=target then transaction(q,before,target,operation=='recover',next_desired)
    else
      if operation~='reconcile' then save_desired(next_desired) end
      if operation=='recover' then fs.unlink(pending); c.exec('sync') end
    end
    local state=describe(target,capability)
    state.changed=before~=target; state.reselection_expected=state.changed
    return state
  end)
  if q then q:close() end
  lock:close(); c.need(ok,result)
  c.atomic(cache,result); return result
end
return B
