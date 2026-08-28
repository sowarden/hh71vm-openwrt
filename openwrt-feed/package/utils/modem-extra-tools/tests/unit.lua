-- Run with Lua 5.1: lua tests/unit.lua files
-- Pure mocks: never access a modem, filesystem or firewall.
package.path=(arg[1] or 'files') .. '/?.lua;' .. package.path
local stored,remote,capability,band_fault,opens,writes={},'',nil,nil,0,0
local imei_raw,imei_fault,imei_writes='',nil,0
local original='c500088820000000'
local regional='c700088820000000' -- Same modem plus LTE B2 capability.
local label_imei='490154203237518'
local donor_imei='356938035643809'
local fs={access=function(p) return stored[p]~=nil end,
  mkdirr=function() return true end, unlink=function(p) stored[p]=nil; return true end}
package.preload['nixio.fs']=function() return fs end
package.preload['nixio']=function() return {} end
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

local function encode_imei(value)
  local result={'08',value:sub(1,1)..'a'}
  for offset=2,14,2 do result[#result+1]=value:sub(offset+1,offset+1)..value:sub(offset,offset) end
  return table.concat(result) .. string.rep('00',119)
end
local qnas={helper='/tmp/test/nas',close=function() end}
function qnas:run(command)
  if command==self.helper .. ' get' then return remote end
  if command==self.helper .. ' capabilities' then return capability end
  local mask=command:match(' set ([a-f0-9]+)$')
  if mask then
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
package.preload.qualcomm=function() return {open=function(name)
  opens=opens+1
  if name=='imei' then return qimei end
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
  imei_raw=encode_imei(donor_imei); imei_fault=nil; imei_writes=0
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
rejects(function() B.execute('recover') end,'recovery required')
band_fault=nil; B.execute('recover')
test(remote==original and not stored[c.directory .. '/band-pending.json'],'band recovery restores original')
reset(); remote='d500088820000000' -- B5 current preference, absent in capabilities.
rejects(function() B.execute('set','3') end,'not supported by this modem: B5')
test(writes==0,'capability mismatch never written')
test(not B.execute('show').editable,'capability mismatch still readable')
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
I.execute('restore',label_imei,true)
test(I.decode(imei_raw)==label_imei,'confirmed original IMEI restored despite valid foreign current value')
test(stored[c.directory .. '/imei-before-restore.json'].decoded==donor_imei,'pre-restore NV 550 safety backup saved')
test(not stored[c.directory .. '/imei-pending.json'],'IMEI journal cleared after verified success')
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

print('PASS ' .. count .. ' assertions (mock unit tests, not hardware write evidence)')
