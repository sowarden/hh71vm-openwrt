-- SPDX-License-Identifier: Apache-2.0
-- Restore-only workflow for the device owner's original IMEI in Qualcomm NV 550.
local c, fs = require 'common', require 'nixio.fs'
local I={}
local backup=c.directory .. '/imei-before-restore.json'
local pending=c.directory .. '/imei-pending.json'
local activation=c.directory .. '/imei-activation-pending.json'
local cache=c.runtime .. '/imei.json'

function I.valid(value)
  if type(value)~='string' or not value:match('^%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d$')
    or not value:find('[1-9]') then return false end
  local sum=0
  for position=1,15 do
    local digit=tonumber(value:sub(position,position))
    if position<15 and position%2==0 then
      digit=digit*2; if digit>=10 then digit=digit-9 end
    end
    sum=sum+digit
  end
  return sum%10==0
end
local function raw_valid(raw)
  return type(raw)=='string' and #raw==256 and not raw:find('[^0-9a-f]')
end
function I.decode(raw)
  c.need(raw_valid(raw),'invalid NV 550 reply')
  c.need(raw:sub(1,2)=='08' and raw:sub(4,4)=='a','NV 550 does not contain a standard IMEI record')
  local digits={raw:sub(3,3)}
  for offset=5,18,2 do
    digits[#digits+1]=raw:sub(offset+1,offset+1)
    digits[#digits+1]=raw:sub(offset,offset)
  end
  local value=table.concat(digits)
  c.need(value:match('^%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d$'),'NV 550 IMEI encoding is invalid')
  return value
end
local function read(q)
  local raw=q:run(q.helper .. ' read')
  c.need(raw_valid(raw),'invalid NV 550 helper reply')
  return raw
end
local function checked_backup(value)
  c.need(type(value)=='table' and value.schema==1 and value.board==c.board()
    and raw_valid(value.raw),'missing, corrupt or foreign IMEI safety backup')
  return value
end
local function describe(raw)
  local decoded_ok,value=pcall(I.decode,raw)
  local state={ok=true,refreshed=os.time(),unread=false,backup_present=fs.access(backup) and true or false,
    pending=fs.access(pending) and true or false,backend='qualcomm-dci-nv550'}
  if decoded_ok then
    state.current_imei=value; state.current_valid=I.valid(value)
  else
    state.current_imei='unreadable'; state.current_valid=false; state.current_error=tostring(value)
  end
  local journal=c.json(pending)
  if journal and type(journal)=='table' and I.valid(journal.target) then
    state.pending_target=journal.target
    state.pending_target_matches=decoded_ok and value==journal.target or false
  end
  local marker=c.json(activation)
  if marker and type(marker)=='table' and marker.schema==1 and marker.board==c.board()
    and I.valid(marker.target) then
    state.activation_pending=true
    state.activation_required='full-power-cycle'
    state.activation_target_matches_nv=decoded_ok and value==marker.target or false
  else
    state.activation_pending=false
  end
  return state
end
function I.cached()
  local state=c.json(cache) or {ok=true,unread=true,backup_present=fs.access(backup) and true or false,
    pending=fs.access(pending) and true or false,backend='qualcomm-dci-nv550'}
  state.backup_present=fs.access(backup) and true or false
  state.pending=fs.access(pending) and true or false
  local marker=c.json(activation)
  state.activation_pending=marker and type(marker)=='table' and marker.schema==1
    and marker.board==c.board() and I.valid(marker.target) or false
  if state.activation_pending then state.activation_required='full-power-cycle' end
  return state
end
function I.execute(operation,target,confirmed)
  c.board()
  if operation=='restore' then
    c.need(confirmed==true,'explicit confirmation of the original device IMEI is required')
    c.need(I.valid(target),'IMEI must be 15 digits with a valid Luhn check digit')
  end
  local lock=c.lock('imei')
  fs.mkdirr(c.directory)
  local q
  local ok,result=pcall(function()
    c.need(not fs.access(pending) or operation=='show' or operation=='recover',
      'an interrupted IMEI restore exists; run imei show, then imei recover')
    q=require('qualcomm').open('imei')
    local before=read(q)
    if operation=='show' then return describe(before) end
    if operation=='restore' then
      if fs.access(backup) then checked_backup(c.json(backup))
      else
        local decoded_ok,decoded=pcall(I.decode,before)
        c.atomic(backup,{schema=1,board=c.board(),created=os.time(),raw=before,
          decoded=decoded_ok and decoded or false})
      end
      c.atomic(pending,{schema=1,board=c.board(),created=os.time(),target=target,before_raw=before})
    elseif operation=='recover' then
      local journal=c.need(c.json(pending),'no pending IMEI restore')
      c.need(type(journal)=='table' and journal.schema==1 and journal.board==c.board()
        and I.valid(journal.target) and raw_valid(journal.before_raw),'corrupt IMEI restore journal')
      target=journal.target
    else error('unknown IMEI operation',0) end
    local after=q:run(q.helper .. ' restore ' .. target,12)
    c.need(raw_valid(after),'invalid NV 550 restore reply')
    c.need(I.decode(after)==target,'IMEI readback mismatch')
    c.atomic(activation,{schema=1,board=c.board(),created=os.time(),target=target})
    fs.unlink(pending); c.need(c.exec('sync'),'filesystem sync failed')
    local before_ok,before_imei=pcall(I.decode,before)
    local state=describe(after); state.changed=not before_ok or before_imei~=target
    state.nv_readback_verified=true
    return state
  end)
  if q then q:close() end
  lock:close(); c.need(ok,result)
  local reported=c.refresh_modem_identity()
  result.identity_cache_refreshed=reported and true or false
  if reported and I.valid(reported.imei) then
    result.reported_imei=reported.imei
    result.reported_matches_nv=result.current_valid and reported.imei==result.current_imei or false
  end
  c.atomic(cache,result); return result
end
return I
