-- Run with Lua 5.1: lua tests/unit.lua files ../hh71vm-simlock/files
-- Pure mocks: never access a modem, filesystem or firewall.
-- The SIM lock module lives in the base-image package now, so the second path is where
-- `simlock` and `kcap` come from; everything else still comes from this package.
package.path=(arg[1] or 'files') .. '/?.lua;' ..
  (arg[2] or '../hh71vm-simlock/files') .. '/?.lua;' .. package.path
local stored,remote,capability,band_fault,opens,writes={},'',nil,nil,0,0
local imei_raw,imei_fault,imei_writes,reported_imei='',nil,0,nil
local original='c500088820000000'
local regional='c700088820000000' -- Same modem plus LTE B2 capability.
local six_band='c500080800000000' -- B1/B3/B7/B8/B20/B28 only.
local label_imei='490154203237518'
local donor_imei='356938035643809'
local fs={access=function(p) return stored[p]~=nil end,
  mkdirr=function() return true end, unlink=function(p) stored[p]=nil; return true end}
package.preload['nixio.fs']=function() return fs end
-- simlock takes its own operation lock through nixio so that both packages that can
-- drive it share one lock file; the mock only has to hand back something lockable.
package.preload['nixio']=function() return {
  open=function() return {lock=function() return true end, close=function() end} end
} end
package.preload['luci.jsonc']=function() return {} end
package.preload['uci']=function() return {} end
local c=require 'common'
c.board=function() return 'hh71vm' end
c.model=function() return 'Alcatel LINKHUB HH71VM' end
c.lock=function() return {close=function() end} end
c.atomic=function(p,v) stored[p]=v end
c.read=function(p) return stored[p] end
c.json=c.read
c.exec=function() return true end
c.refresh_modem_identity=function()
  return reported_imei and {imei=reported_imei} or nil
end

local function encode_imei(value)
  local result={'08',value:sub(1,1)..'a'}
  for offset=2,14,2 do result[#result+1]=value:sub(offset+1,offset+1)..value:sub(offset,offset) end
  return table.concat(result) .. string.rep('00',119)
end
local qnas={helper='/tmp/test/nas',close=function() end}
function qnas:run(command)
  if command==self.helper .. ' get' then
    if band_fault=='unreachable' then error('connection lost',0) end
    return remote
  end
  if command==self.helper .. ' capabilities' then return capability end
  local operation,mask,expected=command:match(' ([a-z]+) ([a-f0-9]+) ([a-f0-9]+)$')
  if operation=='apply' or operation=='restore' then
    if band_fault=='race' then remote='4000000000000000'; band_fault=nil end
    if remote~=expected then error('LTE preference changed concurrently; no write performed',0) end
    writes=writes+1
    if band_fault=='unreachable' then error('connection lost',0) end
    if band_fault=='reject' and mask~=original then error('QMI rejected',0) end
    remote=mask
    if band_fault=='lost-ack' then band_fault='unreachable'; error('ack lost',0) end
    return ''
  end
  error('unexpected NAS mock command: ' .. command)
end
local qimei={helper='/tmp/test/imei',close=function() end}
function qimei:run(command)
  if command==self.helper .. ' read' then return imei_raw end
  local target=command:match(' restore (%d+)$')
  if target then
    imei_writes=imei_writes+1
    if imei_fault=='unreachable' then error('connection lost',0) end
    imei_raw=encode_imei(target)
    if imei_fault=='lost-ack' then imei_fault='unreachable'; error('ack lost',0) end
    return imei_raw
  end
  error('unexpected IMEI mock command: ' .. command)
end
local qshell={close=function() end, commands={}}
local shell_reply,shell_fault='CKLength=16\nSimLockMode=1\nNckUnlockTimes=100\nNetworkCode=00000',nil
local markers_present=true
-- The Qualcomm channel now carries four fixed commands: the read-only configuration
-- query, the configuration write that arms or clears a rule, the marker cleanup and the
-- supervised core_app restart. The mock answers each and keeps the configuration
-- consistent between them, because the code reads a rule back after writing it.
local diag_command
local ap_commands={}
local function ap_command(command)
  ap_commands[#ap_commands+1]=command
  if command:find('mode=ro',1,true) then
    if shell_fault then error(shell_fault,0) end
    return shell_reply
  end
  if command:find('update sim_config',1,true) then
    if shell_fault then error(shell_fault,0) end
    -- The statements arrive shell-quoted, so the pattern tolerates the quoting.
    for value,item in command:gmatch('set value=[^%w]*(%w+)[^%w]*where items=[^%w]*(%w+)') do
      local replaced=false
      shell_reply=shell_reply:gsub(item .. '=[^\n]*', function()
        replaced=true; return item .. '=' .. value
      end)
      if not replaced then shell_reply=shell_reply .. '\n' .. item .. '=' .. value end
    end
    return ''
  end
  if command:find('rm -f',1,true) then markers_present=false; return '' end
  if command:find('killall core_app',1,true) then return '' end
  if command:find('diagcmd',1,true) then return diag_command(command) end
  return nil
end
function qshell:run(command)
  self.commands[#self.commands+1]=command
  local answer=ap_command(command)
  if answer then return answer end
  error('unexpected Qualcomm shell command: ' .. command,0)
end
local sim_raw
local uim_count,uim_verify,erase_writes,erase_fault=1,5,0,nil
local mock_card_state,mock_pn,mock_imsi='PH-NET PIN',0,'255060000000000'
local build='MPSS.TH.2.0.1.c8-00028-M9645LAAAANAZM-2.147565.5'
local build_hex={}
for i=1,#build do build_hex[#build_hex+1]=('%02x'):format(build:byte(i)) end
local quim={helper='/tmp/test/uim',close=function() end}
function quim:run(command)
  local answer=ap_command(command)
  if answer then return answer end
  if command==self.helper then
    if uim_count==0 then return 'slot=00 feature_count=00' end
    return ('slot=00 feature_count=01\nslot=00 feature=00 verify=%02x unblock=00')
      :format(uim_verify)
  end
  error('unexpected UIM mock command: ' .. command)
end
local build_silent=false
-- After one erase the modem stops answering DIAG queries for the rest of its boot, while
-- erase requests themselves keep working. Both halves of that are modelled here.
diag_command=function(command)
  if command:find('diagcmd 7c',1,true) then
    if build_silent then return '' end
    return table.concat(build_hex,' ')
  end
  if command:find('diagcmd fe 00 02 01 02',1,true) then
    erase_writes=erase_writes+1
    build_silent=true
    if erase_fault=='lost-reply' then error('no DIAG reply',0) end
    uim_count=0; mock_pn=0
    mock_card_state=erase_fault=='sim-failure' and 'SIM failure' or 'READY'
    sim_raw={SIMLockState=-1,SIMLockRemainingTimes=0,
      SIMState=erase_fault=='sim-failure' and 0 or 7}
    return ''
  end
  error('unexpected DIAG mock command: ' .. command)
end
package.preload.qualcomm=function() return {open=function(name)
  opens=opens+1
  if name=='imei' then return qimei end
  if name=='shell' then return qshell end
  if name=='uim' then return quim end
  return qnas
end} end
local B,T,I=require 'bands',require 'ttl',require 'imei'
local count=0
local function test(value,message) assert(value,message); count=count+1 end
local function rejects(fn,pattern)
  local ok,err=pcall(fn)
  test(not ok and tostring(err):find(pattern,1,true),'expected rejection: ' .. pattern .. '; got ' .. tostring(err))
end
local function reset()
  stored={}; remote=original; capability=regional; band_fault=nil; opens=0; writes=0
  imei_raw=encode_imei(donor_imei); imei_fault=nil; imei_writes=0; reported_imei=donor_imei
end

reset()
test(B.hex(original),'full LTE mask accepted')
test(not B.hex(original:sub(2)),'short mask rejected')
test(not B.hex(original:upper()),'noncanonical hex rejected')
test(table.concat(B.list(original),',')=='1,3,7,8,20,28,32,38','little-endian 64-bit mask')
test(table.concat(B.list(regional),',')=='1,2,3,7,8,20,28,32,38','dynamic regional capability')
test(B.mask({3,7})=='4400000000000000','mask conversion')
test(table.concat(B.parse('38,3,7',regional),',')=='3,7,38','sort bands')
test(table.concat(B.parse('2,3',regional),',')=='2,3','accept modem-reported regional band')
for _,bad in ipairs({'','3,','3,,7','3;reboot','3 7','0','65','5','3,3','32','3.0','-3'}) do
  test(not pcall(B.parse,bad,regional),'reject malformed or unsupported bands ' .. bad)
end
for _,value in ipairs({1,64,65,128,255}) do test(c.uint(value,1,255,'TTL')==value,'accept TTL') end
for _,value in ipairs({0,256,-1,1.5,'1;reboot','1.0',false,math.huge}) do
  test(not pcall(c.uint,value,1,255,'TTL'),'reject TTL')
end
test(T.validate({ipv4_value=65,ipv6_value=66,wan_network='wan_6'}).ipv4_value==65,'validate mobile WAN')
for _,name in ipairs({'wan;reboot','wan.x','',string.rep('x',33)}) do
  test(not pcall(T.validate,{ipv4_value=65,ipv6_value=65,wan_network=name}),'reject WAN name')
end

-- TTL rule convergence. Reported 2026-09-12..14: after a power cycle there was no
-- internet until the TTL Fix was switched off and back on by hand -- the saved setting
-- read enabled while nothing was rewriting anything. The rules are installed only by
-- the fw3 include and by `ifup` of the mobile WAN, and both can run while that WAN has
-- no L3 device yet, which used to throw and leave exactly that state. What is asserted
-- here is the decision, not an iptables simulation: a scheduled caller defers instead
-- of failing, never rewrites the user's setting, and reconcile is what converges.
local jsonc,uci_mock=require 'luci.jsonc',require 'uci'
local ttl_uci,wan_up,ttl_applied={},true,0
jsonc.parse=function(text) return text~='' and {l3_device='eth2'} or nil end
uci_mock.cursor=function() return {
  get=function(_,_,section,key)
    if section~='ttl' then return nil end
    if not key then return 'ttl' end
    return ttl_uci[key]
  end,
  set=function(_,_,_,key,value) ttl_uci[key]=value; return true end,
  section=function(_,_,_,name) return name end,
  commit=function() return true end,
  foreach=function() end,
} end
local real_popen,real_exec=io.popen,c.exec
c.exec=function(command)
  if command:find('-restore',1,true) then ttl_applied=ttl_applied+1 end
  return true
end
io.popen=function(command)
  local text=''
  if command:find('ubus call',1,true) then text=wan_up and '{"l3_device":"eth2"}' or '' end
  return {read=function() return text end,close=function() end}
end
local function ttl_reset(enabled)
  ttl_uci={enabled=enabled and '1' or '0',ipv4_value='65',ipv6_enabled='0',
    ipv6_value='65',wan_network='wan'}
  wan_up=true; ttl_applied=0
end

ttl_reset(true); wan_up=false
test(T.apply(T.config(),true)==false,'a scheduled apply defers a WAN with no L3 device')
test(ttl_uci.enabled=='1','a deferred apply never rewrites the saved setting')
rejects(function() T.apply(T.config()) end,'WAN network is down')

ttl_reset(true); wan_up=false
local deferred=T.reconcile()
test(deferred.changed==false and deferred.deferred,'reconcile defers while the WAN is down')
test(ttl_uci.enabled=='1','a deferred reconcile leaves the fix enabled')

ttl_reset(true)
test(T.reconcile().changed,'reconcile reinstalls rules a firewall reload took away')
test(ttl_applied>0,'reconcile actually reached iptables-restore')

ttl_reset(true)
-- Nothing to do is the common case, once a minute: it must not rewrite the ruleset.
io.popen=function(command)
  local text=''
  if command:find('ubus call',1,true) then text=wan_up and '{"l3_device":"eth2"}' or ''
  elseif command:find('-S POSTROUTING',1,true) then text='-A POSTROUTING -j MET_TTL\n'
  elseif command:find('-S MET_TTL',1,true) then text='-A MET_TTL -o eth2 -j TTL --ttl-set 65\n' end
  return {read=function() return text end,close=function() end}
end
test(T.reconcile().changed==false and ttl_applied==0,'reconcile is a no-op when the rules are already there')

ttl_reset(false)
test(T.reconcile().enabled==false,'reconcile does nothing while the fix is off')

-- The interactive path must keep the user's choice when the only fault is a WAN that
-- is momentarily down: rolling back to "off" turned that into a silently disabled fix.
ttl_reset(true); wan_up=false
rejects(function() T.change({enabled=true,ipv4_value=64,ipv6_enabled=false,
  ipv6_value=65,wan_network='wan'}) end,'WAN network is down')
test(ttl_uci.enabled=='1','a failed change never switches the TTL Fix off by itself')

io.popen=real_popen; c.exec=real_exec; uci_mock.cursor=nil
test(B.cached().unread and opens==0 and #B.cached().supported_bands==0,'cached status does not invent capabilities')
local shown=B.execute('show')
test(shown.editable and writes==0,'band read is write-free')
test(table.concat(shown.supported_bands,',')=='1,2,3,7,8,20,28,32,38','QMI capability returned to UI')
B.execute('set','2,3')
test(remote=='0600000000000000','regional band mask written')
test(stored[c.directory .. '/band-original.json'].schema==3,'new board-based backup schema')
test(stored[c.directory .. '/band-original.json'].mask==original,'initial band backup saved')
test(stored[c.directory .. '/band-desired.json'].mask==remote,'desired selection persisted')
test(not stored[c.directory .. '/band-pending.json'],'band journal cleared after verified success')
local prior_writes=writes
B.execute('set','2,3')
test(writes==prior_writes,'same selection avoids permanent writes')
rejects(function() B.execute('backup') end,'already exists')
B.execute('restore')
test(remote==original,'restore original bands')
test(not stored[c.directory .. '/band-desired.json'],'restore disables maintenance')
B.execute('undo')
test(remote=='0600000000000000','undo previous preference')
reset(); band_fault='reject'
rejects(function() B.execute('set','3') end,'previous preference restored')
test(not stored[c.directory .. '/band-pending.json'],'confirmed rollback clears band journal')
reset(); band_fault='lost-ack'
rejects(function() B.execute('set','3') end,'recovery required')
test(stored[c.directory .. '/band-pending.json'].before.mask==original,'interruption keeps original band journal')
rejects(function() B.execute('set','7') end,'interrupted transaction')
rejects(function() B.execute('recover') end,'connection lost')
band_fault=nil; B.execute('recover')
test(remote==original and not stored[c.directory .. '/band-pending.json'],'band recovery restores original')
reset(); capability=six_band; remote=original
shown=B.execute('show')
test(shown.editable and shown.capability_mismatch,'capability mismatch remains explicitly editable')
test(table.concat(shown.supported_bands,',')=='1,3,7,8,20,28','DMS bands remain authoritative capabilities')
test(table.concat(shown.unconfirmed_bands,',')=='32,38','current-only bands are discovered dynamically')
test(table.concat(shown.selectable_bands,',')=='1,3,7,8,20,28,32,38','current-only bands stay visible')
B.execute('set','3,32,38')
test(remote==B.mask({3,32,38}),'current-only bands may be preserved while changing confirmed bands')
rejects(function() B.execute('set','3,5,32,38') end,'not supported by this modem: B5')
test(remote==B.mask({3,32,38}),'a new unreported band is never written')
B.execute('set','3,7')
test(remote==B.mask({3,7}),'current-only bands may be removed by an explicit selection')
B.execute('restore')
test(remote==original,'exact original mismatch mask remains restorable')
reset(); capability=B.mask({3,7}); remote=B.mask({3,7,40})
shown=B.execute('show')
test(table.concat(shown.unconfirmed_bands,',')=='40','compatibility behavior is not hardcoded to B32 or B38')
test(table.concat(shown.selectable_bands,',')=='3,7,40','dynamic current-only B40 is selectable')
reset(); band_fault='race'
rejects(function() B.execute('set','3') end,'changed concurrently')
test(writes==0,'helper-side race gate prevents a stale write')
reset(); stored[c.directory .. '/band-original.json']={schema=3,board='another-board',mask=original}
rejects(function() B.execute('restore') end,'different OpenWrt board')
test(writes==0,'wrong-board backup never applied')
reset(); stored[c.directory .. '/band-original.json']={schema=2,model=c.model(),mask=original}
B.execute('restore')
test(remote==original,'legacy version 1.0 restore point remains usable')
reset(); B.execute('set','7'); remote=original
B.execute('reconcile')
test(remote=='4000000000000000','reconcile repairs stock overwrite')
local stable=writes; B.execute('reconcile')
test(writes==stable,'reconcile never writes an unchanged preference')
B.execute('restore'); local read_count=opens; B.execute('reconcile')
test(opens==read_count,'disabled maintenance does not contact modem')

reset()
test(I.valid(label_imei),'valid label IMEI accepted')
test(I.valid(donor_imei),'valid donor IMEI recognized but not trusted as original')
for _,bad in ipairs({'','000000000000000','490154203237517','49015420323751','4901542032375180','49015420323751x'}) do
  test(not I.valid(bad),'reject invalid IMEI ' .. bad)
end
test(I.decode(encode_imei(label_imei))==label_imei,'NV 550 swapped BCD decode')
test(I.cached().unread and opens==0,'cached IMEI status does not contact modem')
local imei_shown=I.execute('show')
test(imei_shown.current_imei==donor_imei and imei_writes==0,'show reports valid current donor value without writing')
rejects(function() I.execute('restore',label_imei,false) end,'explicit confirmation')
test(imei_writes==0,'missing confirmation never writes IMEI')
rejects(function() I.execute('restore','000000000000000',true) end,'valid Luhn')
test(imei_writes==0,'invalid target never writes IMEI')
local restored=I.execute('restore',label_imei,true)
test(I.decode(imei_raw)==label_imei,'confirmed original IMEI restored despite valid foreign current value')
test(stored[c.directory .. '/imei-before-restore.json'].decoded==donor_imei,'pre-restore NV 550 safety backup saved')
test(not stored[c.directory .. '/imei-pending.json'],'IMEI journal cleared after verified success')
test(restored.nv_readback_verified and restored.activation_pending,'verified NV write records pending activation')
test(restored.activation_required=='full-power-cycle','activation state requires a full power cycle')
test(restored.identity_cache_refreshed and not restored.reported_matches_nv,'fresh ATI can still report the pre-restore identity')
test(stored[c.directory .. '/imei-activation-pending.json'].target==label_imei,'activation warning survives an OpenWrt reboot')
reported_imei=label_imei
local reread=I.execute('show')
test(reread.reported_matches_nv and reread.activation_pending,'matching ATI refreshes the cache but does not claim network activation')
local safety=stored[c.directory .. '/imei-before-restore.json'].raw
I.execute('restore',donor_imei,true)
test(stored[c.directory .. '/imei-before-restore.json'].raw==safety,'first IMEI safety backup is never overwritten')
reset(); imei_raw=string.rep('00',128)
I.execute('restore',label_imei,true)
test(I.decode(imei_raw)==label_imei,'missing or damaged current IMEI does not block restore')
reset(); imei_fault='lost-ack'
rejects(function() I.execute('restore',label_imei,true) end,'ack lost')
test(stored[c.directory .. '/imei-pending.json'].target==label_imei,'interrupted IMEI restore keeps confirmed target')
rejects(function() I.execute('restore',donor_imei,true) end,'interrupted IMEI restore')
imei_fault=nil; I.execute('recover')
test(I.decode(imei_raw)==label_imei and not stored[c.directory .. '/imei-pending.json'],'IMEI recovery finishes confirmed target')

-- Carrier SIM lock. Two things make this different from the other operations: a wrong
-- control key can cost one of a finite number of attempts, and the key itself must not
-- survive anywhere. So the assertions below are mostly about refusing to write.
local K,SL=require 'kcap',require 'simlock'
local function encode(value)
  if type(value)~='table' then return '"' .. tostring(value) .. '"' end
  local keys={}
  for key in pairs(value) do keys[#keys+1]=key end
  table.sort(keys)
  local parts={}
  for i,key in ipairs(keys) do parts[i]='"' .. key .. '":' .. encode(value[key]) end
  return '{' .. table.concat(parts,',') .. '}'
end
jsonc.stringify=encode
jsonc.parse=function(text)
  if text:sub(1,3)=='AT:' then
    -- The card reader sends one batch and matches answers by position, so the mock answers
    -- every command in the order it was asked, not just the first one.
    local results={}
    local index=1
    while true do
      local from=text:find('"' .. index .. '":',1,true)
      if not from then break end
      local stop=text:find('"' .. (index+1) .. '":',from,true) or (#text+1)
      local command=text:sub(from,stop-1)
      local line,final='','OK'
      if command:find('AT+GSN',1,true) then line='012345678901234'
      elseif command:find('AT+CPIN?',1,true) then
        if mock_card_state=='SIM failure' then final='+CME ERROR: SIM failure'
        else line='+CPIN: ' .. mock_card_state end
      elseif command:find('AT+CIMI',1,true) then line=mock_imsi
      elseif command:find('28589',1,true) then line='+CRSM: 144,0,"00000002"'
      elseif command:find('AT+CRSM',1,true) then line='+CRSM: 144,0,"00000000000000000000"'
      elseif command:find('AT+CLCK',1,true) then
        if mock_card_state=='READY' then line='+CLCK: ' .. tostring(mock_pn) else final='ERROR' end
      elseif command:find('AT+CEREG',1,true) then line='+CEREG: 2,1' end
      results[#results+1]={final=final,lines={line}}
      index=index+1
    end
    return {ok=true,results=results}
  end
  if text=='{"result":{"SIMLockState":0}}' then return {result={SIMLockState=0}} end
  if text=='{"error":{"code":-32000,"message":"unknown error"}}' then
    return {error={code=-32000,message='unknown error'}}
  end
  if text=='not json' then return nil end
  return {result={}}
end
io.popen=function(command)
  test(command:find('/bin/ubus call hh71vm-modem at',1,true),
    'SIM lock uses only the modem daemon AT bridge')
  return {read=function() return 'AT:' .. command end,close=function() end}
end

local function rd(text,position) return K.rd32(text,position) end
local body=K.body('GetSimStatus',nil)
local frame=K.frame(7,body)
test(body:sub(-1)=='\0' and body:find('"params": {}',1,true),'empty params encode as an object, not []')
test(#frame==44+#body,'call frame is a 44-byte header plus JSON and NUL')
test(frame:sub(1,4)=='kcap','call frame magic')
test(rd(frame,5)==#frame and rd(frame,9)==7 and rd(frame,13)==1,'length, sequence and call type are little-endian')
test(rd(frame,33)==0x77000001,'context handle is echoed back by core_app')
test(rd(frame,17)==0 and rd(frame,21)==0 and rd(frame,25)==0 and rd(frame,29)==0
  and rd(frame,37)==0 and rd(frame,41)==0,'reserved header words are zero in a call')
test(K.body('UnlockSimlock',{SIMLockCode='1',SIMLockState=0}):find('"SIMLockCode":"1"',1,true),
  'parameters are sent as an object')

local reply=function(sequence,text,kind)
  return 'kcap' .. K.le32(44+#text+1) .. K.le32(sequence) .. K.le32(kind or 1)
    .. string.rep(K.le32(0),4) .. K.le32(0x77000001) .. K.le32(#text) .. K.le32(0x101)
    .. text .. '\0'
end
local good=reply(3,'{"result":{}}')
local text,rest,broken=K.take(good,3)
test(text=='{"result":{}}' and rest=='' and not broken,'reply JSON is returned without its NUL')
test(select(1,K.take(good:sub(1,20),3))==nil and select(2,K.take(good:sub(1,20),3))==good:sub(1,20),
  'a partial frame is kept for the next read')
test(select(1,K.take(reply(9,'{"result":{}}'),3))==nil,'a stale reply for another call is ignored')
-- An event frame stops after 28 bytes and carries no JSON; a fixed 44-byte unpack throws
-- on it. Reported twice during the KCAP work, so it is asserted here.
local event='kcap' .. K.le32(28) .. K.le32(0) .. K.le32(2) .. string.rep(K.le32(0),3)
test(K.take(event .. good,3)=='{"result":{}}','an event frame is skipped, not misparsed')
test(select(3,K.take('xxxx' .. string.rep('\0',40),3))=='frame out of sync','bad magic is fatal')
test(select(3,K.take('kcap' .. K.le32(999999) .. string.rep('\0',30),3))=='bad frame length','oversized frame is refused')
test(select(3,K.take('kcap' .. K.le32(8) .. string.rep('\0',30),3))=='bad frame length','undersized frame is refused')
test(select(2,K.result('{"error":{"code":-32000,"message":"unknown error"}}'))=='unknown error','vendor error is surfaced verbatim')
test(select(2,K.result('not json'))=='unparsable core_app answer','unparsable answer is an error, not an empty result')
test(K.result('{"result":{"SIMLockState":0}}').SIMLockState==0,'result object is unwrapped')

test(SL.classify(-1)=='unlocked','state -1 is no personalisation')
for state=0,4 do test(SL.classify(state)=='locked','state ' .. state .. ' is a control-key state') end
for state=15,19 do test(SL.classify(state)=='unblock','state ' .. state .. ' needs the reset key') end
test(SL.classify(30)=='forbidden','state 30 is reset-key forbidden')
for _,state in ipairs({5,14,20,99}) do test(SL.classify(state)=='unknown','state ' .. state .. ' is unknown') end
test(SL.classify(nil)=='unknown' and SL.classify('0')=='unknown','a missing or non-numeric state is unknown')
for state=0,4 do
  local mapped=SL.normalize({SIMLockState=state,SIMLockRemainingTimes=5})
  test(mapped.facility==({[0]='PN',[1]='PU',[2]='PP',[3]='PC',[4]='PF'})[state],'facility for state ' .. state)
end
-- The owner's unlocked GK unit reports state -1 with a zero counter. Reading that as
-- "blocked" would hide a working modem behind a scary refusal.
local unlocked=SL.normalize({SIMLockState=-1,SIMLockRemainingTimes=0,SIMState=7,PLMN='25506'})
test(not unlocked.locked and unlocked.plmn=='25506','unlocked unit is not reported as locked')
-- The refusal is decided from the modem: the authoritative lock state, the personalisation
-- list and the attempt count. core_app's socket drops often enough that refusing whenever
-- it is silent would block a perfectly submittable code, so its fields are a cross-check.
-- `drop` names fields to remove: a nil in a Lua table constructor is simply absent, so
-- "this field could not be read" needs to be spelled out separately.
local function pn(extra,drop)
  local status={lock_state='challenged',attempts_left=3,code_length=16,
    uim={feature_count=1,features={{id=0,verify=3,unblock=0}}}}
  for key,value in pairs(extra or {}) do status[key]=value end
  for _,key in ipairs(drop or {}) do status[key]=nil end
  return status
end
test(SL.refusal(pn())==nil,'a PN challenge with attempts and a known length allows a code')
test(SL.refusal(pn({state=0,category='locked',sim_state=4}))==nil,
  'core_app agreeing with the modem changes nothing')
test(SL.refusal(pn())==nil,'a silent core_app does not block a submittable code')
test(SL.refusal(pn(nil,{'attempts_left'})):find('unknown',1,true),'absent attempt count refuses')
test(SL.refusal(pn({attempts_left=0})):find('without a code',1,true),
  'zero attempts points at keyless removal instead')
test(SL.refusal(pn(nil,{'code_length'})):find('length',1,true),'unknown code length refuses')
test(SL.refusal(pn(nil,{'uim'})):find('could not be read',1,true),'an unreadable feature list refuses')
test(SL.refusal(pn({uim={feature_count=2,features={{id=0,verify=3},{id=4,verify=3}}}})):find('refusing to guess',1,true),
  'more than one personalisation refuses')
test(SL.refusal(pn({uim={feature_count=1,features={{id=4,verify=3}}}})):find('not a network',1,true),
  'a non-network personalisation refuses an NCK')
test(SL.refusal(pn({state=17,category='unblock'})):find('RCK',1,true),'unblock state refuses an NCK')
test(SL.refusal(pn({state=30,category='forbidden'})):find('forbidden',1,true),'forbidden state refuses')
test(SL.refusal(pn({state=2,category='locked'})):find('not a PN',1,true),
  'an NCK cannot be sent to another lock facility')
test(SL.refusal({lock_state='none'})=='this modem is not challenging the inserted SIM',
  'a free modem has nothing to unlock')
test(SL.refusal({lock_state='allowed'}):find('nothing to unlock',1,true),
  'an accepted SIM has nothing to unlock with a code')
test(SL.refusal({lock_state='unknown',lock_state_reason='the SIM needs SIM PIN'}):find('blocked',1,true),
  'an unreadable card blocks code submission')

-- core_app reports SIMLockState from the card only while the card state is 4, and its
-- unlock handler refuses to build a request in any other state. A ready card therefore
-- always reads -1, and a PIN-locked or absent card reads -1 too - which is not the same
-- statement and must not be reported as one.
local ready=SL.normalize({SIMLockState=-1,SIMLockRemainingTimes=0,SIMState=7})
test(ready.sim_state_name=='ready' and not ready.enforcing,'a ready card is not enforcing a carrier lock')
local holding=SL.normalize({SIMLockState=0,SIMLockRemainingTimes=5,SIMState=4})
test(holding.sim_state_name=='carrier-locked' and holding.enforcing,'card state 4 is the enforcing state')
for _,pair in ipairs({{0,'no-sim'},{2,'pin-required'},{3,'puk-required'},{6,'invalid'}}) do
  local blocked=SL.normalize({SIMLockState=-1,SIMLockRemainingTimes=0,SIMState=pair[1]})
  test(blocked.sim_state_name==pair[2],'SIM state ' .. pair[1] .. ' is named ' .. pair[2])
end

test(SL.valid_length(16)==16 and SL.valid_length('10')==10,'ETSI lengths are accepted')
for _,bad in ipairs({0,4,7,17,99,'x',10.5,'16x'}) do test(not SL.valid_length(bad),'reject key length ' .. tostring(bad)) end
test(not SL.valid_length(nil),'a missing key length is not a length')
test(SL.valid_code('0123456789012345',16),'leading zeros are preserved, not parsed as a number')
for _,bad in ipairs({'123456789012345','12345678901234567','abcdefghijklmnop','1234 56789012345','','123456789012345 '}) do
  test(not SL.valid_code(bad,16),'reject malformed key ' .. bad)
end
test(not SL.valid_code(1234567890123456,16),'a number is not a control key')

local generated=SL.generate('012345678901234')
test(generated.nck10=='6318552905' and generated.nck16=='6318552905478883',
  'synthetic NCK vector, including all six control digits')
rejects(function() SL.generate('01234567890123') end,'exactly 15 decimal digits')
test(SL.live_imei().imei=='012345678901234','fresh AT+GSN prefill uses the modem daemon')
test(SL.tested_build(table.concat(build_hex,' ')),'tested MPSS build is recognized from DIAG hex')
test(not SL.tested_build('7c 00'),'unrecognized MPSS cannot enable erase')
test(SL.parse_uim('slot=00 feature_count=00').feature_count==0,'empty UIM feature list')
rejects(function() SL.parse_uim('slot=00 feature_count=01') end,'incomplete UIM feature list')

test(SL.parse_config('CKLength=16\nSimLockMode=1\nNckUnlockTimes=100').code_length==16,'CKLength is read from sim_config')
test(SL.parse_config('CKLength=16\nSimLockMode=1\n').lock_provisioned==true,'SimLockMode 1 marks a provisioned lock')
-- false and nil must stay distinct: "not provisioned" is not "could not be read".
test(SL.parse_config('SimLockMode=0\n').lock_provisioned==false,'SimLockMode 0 is reported as false, not dropped')
test(SL.parse_config('CKLength=10\n').lock_provisioned==nil,'an unread SimLockMode stays absent')
test(SL.parse_config('CKLength=4\n').code_length==nil,'an out-of-range CKLength is treated as unknown')
test(SL.parse_config('').code_length==nil,'an empty query result is treated as unknown')

-- Pure state classification. These are the decisions the page shows the user, so they are
-- worth pinning down without a modem: "a lock is armed" and "this card is refused" are
-- different answers, and core_app's card state can report neither of them correctly.
test(SL.evaluate({cpin='PH-NET PIN'},{feature_count=1,features={{id=0,verify=9}}})=='challenged',
  'a personalisation challenge is the challenged state')
test(SL.evaluate({cpin='READY',pn=1},{feature_count=1,features={{id=0,verify=10}}})=='allowed',
  'an armed lock with an accepted card is the allowed state, not "unlocked"')
test(SL.evaluate({cpin='READY',pn=0},{feature_count=0,features={}})=='none','no feature and PN off is no lock')
-- An inactive but readable card reports card state 6, and a successful removal is still a
-- removal on such a card; the verdict must not depend on core_app reporting 7.
test(SL.evaluate({cpin='READY',pn=0},{feature_count=0,features={}},{sim_state=6})=='none',
  'card state 6 does not turn a removed lock into a failure')
test(SL.evaluate({cpin='SIM PIN'},{feature_count=0,features={}})=='unknown','a PIN-locked card cannot report the carrier lock')
-- A removal clears the lock record before the card session notices. Calling that window a
-- challenge produced a refusal that read like a fault; it is settling, and it says so.
local settling,why=SL.evaluate({cpin='PH-NET PIN'},{feature_count=0,features={}})
test(settling=='unknown' and why:find('re-initialising',1,true),
  'no feature left while the card still shows a challenge is settling, not a lock')
test(SL.evaluate({cpin='SIM failure'},{feature_count=0,features={}})=='unknown','a failing SIM is unknown, not unlocked')
test(SL.evaluate({cpin='READY',pn=0})=='unknown','a missing UIM list is unknown, not unlocked')
test(SL.evaluate(nil,nil)=='unknown','no card reading at all is unknown')

test(SL.home_plmn('255060000000000','00000002')=='25506','a two-digit MNC gives a five-digit home network')
test(SL.home_plmn('310260123456789','00000003')=='310260','EF_AD selects a three-digit MNC')
test(SL.home_plmn('255060000000000')=='25506','two digits is the fallback when EF_AD is unreadable')
test(SL.home_plmn('25506')==nil and SL.home_plmn(nil)==nil,'a short or missing IMSI yields no home network')
test(SL.valid_plmn('25506') and SL.valid_plmn('310260'),'five and six digit network codes are accepted')
for _,bad in ipairs({'00000','000000','2550','2550600','abcde','',nil}) do
  test(not SL.valid_plmn(bad),'reject network code ' .. tostring(bad))
end

test(SL.verified_removed({lock_state='none',card={cpin='READY',pn=0},uim={feature_count=0}}),
  'removal is verified from the card and the modem feature list')
test(not SL.verified_removed({lock_state='none',card={cpin='READY',pn=1},uim={feature_count=0}}),
  'PN still enabled is not a removal')
test(not SL.verified_removed({lock_state='allowed',card={cpin='READY',pn=1},uim={feature_count=1}}),
  'an armed lock is never a removal')

-- Keyless removal is the path that always works, so it must stay available on an armed
-- lock whether or not the card is challenged, and with no attempts left.
test(SL.erase_refusal({lock_state='challenged',tested_build=true})==nil,'a challenge can be cleared without a code')
test(SL.erase_refusal({lock_state='allowed',tested_build=true})==nil,'an accepted card can still have its lock removed')
test(SL.erase_refusal({lock_state='none',tested_build=true}):find('no carrier lock',1,true),
  'there is nothing to remove without a lock')
test(SL.erase_refusal({lock_state='challenged',tested_build=false}):find('verified on',1,true),
  'an unverified firmware build refuses keyless removal')
test(SL.erase_refusal({lock_state='challenged'}):find('could not be confirmed',1,true),
  'an unknown firmware build refuses keyless removal')

test(SL.lock_refusal({lock_state='none',card={cpin='READY'},home_plmn='25506',lock_provisioned=false})==nil,
  'a free modem with a readable card can be locked')
test(SL.lock_refusal({lock_state='allowed'}):find('already has a carrier lock',1,true),
  'a locked modem is not locked again')
test(SL.lock_refusal({lock_state='none',card={cpin='READY'},lock_provisioned=false}):find('could not be read',1,true),
  'locking needs the card home network, never the serving network')
test(SL.lock_refusal({lock_state='none',card={cpin='SIM PIN'},home_plmn='25506'}):find('working SIM',1,true),
  'locking needs a usable card in the router')

local kcap_log,answer_fault={},nil
local key='0123456789012345'
K.call=function(method,params,timeout)
  kcap_log[#kcap_log+1]={method=method,params=params}
  if method=='GetSimStatus' then
    if answer_fault=='no-status' then return nil,'core_app refused the connection',false end
    return sim_raw
  end
  if answer_fault=='indeterminate' then return nil,'no answer from core_app',true end
  if answer_fault=='unreachable' then return nil,'cannot reach core_app',false end
  if method=='UnlockSimlock' then
    if params and params.SIMLockCode==key then
      sim_raw={SIMLockState=-1,SIMLockRemainingTimes=0,SIMState=7}
      uim_count=0; mock_card_state='READY'; mock_pn=0
      return {}
    end
    -- A wrong code answers with a bare error and still spends one attempt.
    uim_verify=uim_verify-1
    sim_raw.SIMLockRemainingTimes=uim_verify
    return nil,'unknown error',true
  end
  if method=='ActiveSimlock' then
    if shell_reply:find('SimLockMode=1',1,true) then
      uim_count=1; uim_verify=10
      if shell_reply:match('NetworkCode=(%d+)')==mock_imsi:sub(1,5) then
        mock_card_state='READY'; mock_pn=1
        sim_raw={SIMLockState=-1,SIMLockRemainingTimes=0,SIMState=7}
      else
        mock_card_state='PH-NET PIN'; mock_pn=0
        sim_raw={SIMLockState=0,SIMLockRemainingTimes=10,SIMState=4}
      end
    end
    return {}
  end
  return {}
end
SL.pause=function() end -- the settle loop must not really sleep in a mock run

local function sim_reset(state,attempts)
  stored={}; kcap_log={}; answer_fault=nil; shell_fault=nil; qshell.commands={}; ap_commands={}
  shell_reply='CKLength=16\nSimLockMode=1\nNckUnlockTimes=100\nNetworkCode=25501'
  sim_raw={SIMLockState=state,SIMLockRemainingTimes=attempts,SIMState=4,PLMN='25002'}
  uim_count=1; uim_verify=attempts; erase_writes=0; erase_fault=nil
  mock_card_state='PH-NET PIN'; mock_pn=0; build_silent=false; markers_present=true
end
local function free_reset()
  sim_reset(-1,0)
  shell_reply='CKLength=16\nSimLockMode=0\nNckUnlockTimes=100\nNetworkCode=00000'
  sim_raw={SIMLockState=-1,SIMLockRemainingTimes=0,SIMState=7,PLMN='25506'}
  uim_count=0; mock_card_state='READY'; mock_pn=0
end
local function writes()
  local total=0
  for _,entry in ipairs(kcap_log) do if entry.method=='UnlockSimlock' then total=total+1 end end
  return total
end
local function sent_key()
  for _,entry in ipairs(kcap_log) do if entry.method=='UnlockSimlock' then return entry.params end end
end

sim_reset(0,5)
local shown=SL.inspect()
test(shown.state==0 and shown.facility=='PN' and shown.remaining_attempts==5,'status reports the live lock state')
test(shown.code_length==16 and shown.configured_attempts==100,'status detects the configured key length')
test(shown.can_unlock and not shown.unlock_refusal,'a locked unit with attempts and a known length can be unlocked')
-- The modem's SQLite is 3.7.17 and has no -readonly switch; the URI form is what that
-- build accepts, confirmed on the device before this was written.
local read_query
for _,command in ipairs(ap_commands) do
  if command:find('sim_config',1,true) and command:find('mode=ro',1,true) then read_query=command end
end
test(read_query and not read_query:find('update',1,true),'key length is read with a fixed read-only query')
test(writes()==0,'reading status never writes')

sim_reset(0,5)
local challenged=SL.inspect()
test(challenged.lock_state=='challenged' and challenged.locked and challenged.challenged,
  'a refused card is reported as a challenge')
test(challenged.attempts_left==5,'live attempts come from the modem feature list')
test(challenged.locked_plmn=='25501' and challenged.locked_to_home~=true,
  'the locked network is reported while a lock is armed')
test(challenged.can_erase and not challenged.can_lock,'a challenge offers removal but not a new lock')

free_reset()
local free=SL.inspect()
test(free.lock_state=='none' and not free.locked,'a free modem is reported as free')
test(free.home_plmn=='25506','the inserted card home network comes from its own IMSI')
test(free.can_lock and not free.can_erase,'a free modem offers a lock and nothing to remove')
test(free.locked_plmn==nil,'a cleared rule is not presented as a locked carrier')

sim_reset(2,5)
test(not SL.inspect().can_unlock,'a non-PN challenge does not accept an NCK')
rejects(function() SL.unlock(key,2,true) end,'not a PN')
test(writes()==0,'non-PN challenge never receives an NCK')

sim_reset(0,5)
rejects(function() SL.unlock(key,0,false) end,'explicit carrier-unlock confirmation')
test(writes()==0,'missing confirmation never writes')
rejects(function() SL.unlock(key,nil,true) end,'expected lock state is required')
test(writes()==0,'missing expected state never writes')
rejects(function() SL.unlock(key,2,true) end,'lock state changed')
test(writes()==0,'a stale expected state never writes')
rejects(function() SL.unlock('123456789',0,true) end,'exactly 16 digits')
rejects(function() SL.unlock('abcdefghijklmnop',0,true) end,'exactly 16 digits')
test(writes()==0,'a malformed key never reaches the modem')
free_reset()
rejects(function() SL.unlock(key,-1,true) end,'not challenging the inserted SIM')
sim_reset(17,3)
rejects(function() SL.unlock(key,17,true) end,'RCK')
sim_reset(0,0)
rejects(function() SL.unlock(key,0,true) end,'no attempts remain')
sim_reset(0,5); shell_fault='sqlite3: not found'
rejects(function() SL.unlock(key,0,true) end,'refusing to guess')
test(writes()==0,'no refusal path reaches the modem')

sim_reset(0,5)
local cleared=SL.unlock(key,0,true)
test(writes()==1,'a successful unlock sends exactly one request')
test(sent_key().SIMLockCode==key and sent_key().SIMLockState==0,
  'the key is sent with the state that was just read back, not a remembered one')
test(cleared.unlocked and cleared.verified_unlocked and cleared.lock_state=='none' and
  cleared.state_before=='challenged','a keyed unlock is confirmed from the modem, not from the reply')
test(cleared.card.cpin=='READY' and cleared.card.readable and cleared.card.pn==0 and
  cleared.uim.feature_count==0,'verified unlock is not inferred from KCAP success alone')
-- The saved rule is what would let the carrier lock come back later, so a verified
-- removal clears it and says so.
test(cleared.cleanup and cleared.cleanup.ok and cleared.lock_provisioned==false and
  cleared.saved_network_code=='00000','a verified unlock clears the saved carrier rule')
test(not markers_present,'the arming marker is removed once the lock is gone')
test(not encode(cleared):find(key,1,true),'the control key is not echoed in the result')
test(not encode(stored['/var/run/hh71vm-simlock/simlock.json'] or {}):find(key,1,true),'the control key is not persisted')

-- A wrong code is an ordinary outcome, not an error: the modem answers with a bare
-- failure and spends one attempt, and the user needs to be told exactly that.
sim_reset(0,5)
local refused=SL.unlock('9999999999999999',0,true)
test(writes()==1 and refused.wrong_code and not refused.unlocked,'a wrong code is reported as a wrong code')
test(refused.attempts_before==5 and refused.attempts_after==4 and refused.attempt_consumed,
  'a wrong code reports the attempt it spent')
test(refused.note:find('4 attempt',1,true),'the remaining attempts are stated plainly')
test(refused.lock_provisioned~=false,'a failed unlock never clears the saved carrier rule')

-- A reply lost after the request went out must never be retried: core_app may already
-- have spent the attempt. Unlike a wrong code, that is not a verdict.
sim_reset(0,5); answer_fault='indeterminate'
local lost=SL.unlock(key,0,true)
test(writes()==1,'an indeterminate write is not repeated')
test(not lost.unlocked,'an indeterminate write is not reported as success')
sim_reset(0,5); answer_fault='unreachable'
rejects(function() SL.unlock(key,0,true) end,'cannot reach core_app')
test(writes()==1,'a request that never left the box is reported as such')

sim_reset(0,0)
rejects(function() SL.erase(false) end,'explicit unlock confirmation')
test(erase_writes==0,'unconfirmed keyless removal never writes')
local exhausted=SL.inspect()
test(not exhausted.can_unlock,'zero attempts block a keyed unlock')
test(exhausted.can_erase,'zero attempts still permit keyless removal')
local removed=SL.erase(true)
test(erase_writes==1 and removed.erased and removed.card.readable and removed.uim.feature_count==0,
  'one DIAG erase is confirmed by independent readback, not by its exit status')
test(removed.lock_provisioned==false,'a verified keyless removal also clears the saved carrier rule')

-- After an erase the modem stops answering DIAG queries until it reboots, but erase
-- requests still work. A build confirmed earlier must keep the action available.
sim_reset(0,10)
test(build_silent==false,'the fixture starts with DIAG answering')
local first=SL.inspect()
test(first.tested_build==true and stored['/etc/hh71vm-simlock/diag-build'],'a confirmed build is remembered')
SL.erase(true)
test(build_silent,'an erase silences DIAG queries for the rest of that Qualcomm boot')
-- Put a second challenge up without clearing what the device already learned.
local function rearm()
  shell_reply='CKLength=16\nSimLockMode=1\nNckUnlockTimes=100\nNetworkCode=25501'
  sim_raw={SIMLockState=0,SIMLockRemainingTimes=10,SIMState=4,PLMN='25002'}
  uim_count=1; uim_verify=10; mock_card_state='PH-NET PIN'; mock_pn=0; erase_fault=nil
end
rearm()
local second=SL.inspect()
test(second.tested_build==true,'a silent DIAG falls back to the build confirmed on this device')
test(second.can_erase,'a second removal is not blocked by DIAG going quiet')
stored={}; rearm()
local unknown_build=SL.inspect()
test(unknown_build.tested_build==nil and not unknown_build.can_erase,
  'without any confirmed build, keyless removal stays unavailable')

sim_reset(0,0)
erase_fault='lost-reply'
local pending=SL.erase(true)
test(erase_writes==1 and not pending.erased,'a lost DIAG reply does not trigger a second erase or claim success')
test(pending.lock_provisioned~=false,'an unconfirmed removal never clears the saved carrier rule')

sim_reset(0,0)
erase_fault='sim-failure'
pending=SL.erase(true)
test(erase_writes==1 and not pending.erased,'a SIM failure leaves removal unverified despite an empty feature list')
mock_card_state='READY'; mock_pn=0; sim_raw.SIMState=7
local recovered=SL.inspect()
test(recovered.verified_unlocked and erase_writes==1,
  'a fresh readable card can confirm the removal without resending it')

-- A removal whose readback was cut short leaves the saved rule behind, because cleanup
-- only runs on a verified result. Clearing it later is allowed only once the modem itself
-- says no lock is armed, which is the same condition that makes dropping the markers safe.
sim_reset(0,5)
rejects(function() SL.forget_saved_rule() end,'no carrier lock')
free_reset()
rejects(function() SL.forget_saved_rule() end,'no saved carrier setting')
free_reset()
shell_reply='CKLength=16\nSimLockMode=1\nNckUnlockTimes=100\nNetworkCode=25501'
markers_present=true
local forgotten=SL.forget_saved_rule()
test(forgotten.cleared and forgotten.lock_provisioned==false,'a leftover saved rule can be cleared')
test(not markers_present,'clearing the leftover rule also drops the arming marker')
test(shell_reply:find('SimLockMode=0',1,true),'the saved mode is written back to zero')

-- Creating a lock. The only offer is the network of the card in the router, so the
-- router cannot lock itself away from the SIM it is using.
free_reset()
rejects(function() SL.lock('25506',false) end,'explicit carrier-lock confirmation')
rejects(function() SL.lock('123',true) end,'five or six digit network code')
rejects(function() SL.lock('25501',true) end,'not 25501')
test(shell_reply:find('SimLockMode=0',1,true),'a refused lock never writes the configuration')
sim_reset(0,5)
rejects(function() SL.lock('25506',true) end,'already has a carrier lock')

free_reset()
local locked=SL.lock('25506',true)
test(locked.applied and locked.lock_state=='allowed','a lock to the inserted card network is verified before it is reported')
test(locked.card.pn==1 and locked.uim.feature_count==1,'the lock is confirmed from the modem itself')
test(locked.locked_plmn=='25506' and locked.locked_to_home,'the locked network is the one that was asked for')
test(shell_reply:find('SimLockMode=1',1,true) and shell_reply:find('NetworkCode=25506',1,true),
  'the saved rule records the network that was locked')
local restarted=false
for _,command in ipairs(ap_commands) do
  if command:find('killall core_app',1,true) then restarted=true end
end
test(restarted,'the mode is cached at start-up, so the lock restarts core_app before provisioning')
test(not locked.can_lock and locked.can_erase,'a fresh lock offers removal, not another lock')

print('PASS ' .. count .. ' assertions (mock unit tests, not hardware write evidence)')
