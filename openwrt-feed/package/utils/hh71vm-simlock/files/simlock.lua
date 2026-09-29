-- SPDX-License-Identifier: Apache-2.0
-- Carrier (network) lock and unlock over the Qualcomm control channel.
--
-- This is not the SIM PIN or PUK. The lock lives in the modem's own personalisation
-- store, and stock Realtek is only a client of the same core_app API, so no stock image
-- is needed to read, remove or create one.
--
-- Three different things are easy to confuse, and every earlier revision of this file got
-- at least one of them wrong:
--
--   * the *saved rule* in the Qualcomm AP's sim_config table - what a future provisioning
--     call would apply, and nothing about the present;
--   * the *armed lock* in the modem - AT+CLCK="PN",2 and the UIM personalisation feature
--     list, which is the only authoritative answer to "is this modem carrier locked";
--   * the *current card* - core_app's GetSimStatus, which says whether this particular SIM
--     is accepted or challenged and, while challenged, how many attempts are left.
--
-- GetSimStatus reads identically whether or not a lock is armed, and it lags the modem by
-- a few seconds after any write. So the rule throughout is: decide from AT and UIM, let
-- the status settle before judging a write, and never retry a request that was sent.
local c, fs, nixio = require 'common', require 'nixio.fs', require 'nixio'
local S={}

-- State paths are absolute rather than taken from whichever `common` happened to be
-- loaded. Both the base backend and the add-on package can drive this module, and they
-- must share one operation lock and one cache: two writers to the modem at once is the
-- one thing that must not happen.
local STATE='/var/run/hh71vm-simlock'
local SETTINGS='/etc/hh71vm-simlock'
local cache=STATE .. '/simlock.json'

--- Exclusive across every caller of this module. The kernel releases it on a crash too,
--- so the file is never unlinked.
local function guard()
  fs.mkdirr(STATE)
  local fd=c.need(nixio.open(STATE .. '/simlock.lock','w',600),'cannot open the operation lock')
  if not fd:lock('tlock') then
    fd:close(); error('another SIM lock operation is running; retry when it finishes',0)
  end
  return fd
end

-- Vendor state numbering, taken from the SIMLOCK_PERSO_* constants in the stock web
-- bundle: -1 none, 0..4 control key, 15..19 reset key, 30 reset key forbidden. -1 is "no
-- personalisation required": on an unlocked unit the remaining-attempt counter reads 0
-- alongside it, so a zero counter is only meaningful once the state itself says a lock is
-- present.
local FACILITY={[0]='PN',[1]='PU',[2]='PP',[3]='PC',[4]='PF'}
local CATEGORY={[0]='network',[1]='network-subset',[2]='service-provider',
  [3]='corporate',[4]='uim'}

-- SIM_STATE_* from the same bundle. Only state 4 matters here: core_app fills SIMLockState
-- from the card only when the card state is 4, and its unlock handler refuses to build a
-- request in any other state.
local SIM_LOCKED, SIM_READY = 4, 7
local SIM_STATE={[0]='no-sim',[1]='detected',[2]='pin-required',[3]='puk-required',
  [4]='carrier-locked',[5]='puk-attempts-exhausted',[6]='invalid',[7]='ready',
  [11]='initialising'}

function S.classify(state)
  if type(state) ~= 'number' then return 'unknown' end
  if state == -1 then return 'unlocked' end
  if state >= 0 and state <= 4 then return 'locked' end
  if state >= 15 and state <= 19 then return 'unblock' end
  if state == 30 then return 'forbidden' end
  return 'unknown'
end

-- ETSI TS 122 022 allows 8..16 decimal digits for NCK and the other non-PCK control
-- keys. Anything outside that is a misread, not a device that wants a 3-digit key.
function S.valid_length(value)
  local length=tonumber(value)
  if not length or length ~= math.floor(length) or length < 8 or length > 16 then return nil end
  return length
end

function S.valid_code(code, length)
  return type(code) == 'string' and type(length) == 'number'
    and #code == length and code:match('^%d+$') ~= nil
end

function S.valid_plmn(value)
  return type(value)=='string' and value:match('^%d%d%d%d%d%d?$')~=nil and value~='00000'
    and value~='000000'
end

local function band(a,b)
  local value,power=0,1
  for _=1,32 do
    if a%2 == 1 and b%2 == 1 then value=value+power end
    a=math.floor(a/2); b=math.floor(b/2); power=power*2
  end
  return value
end

local function bxor(a,b)
  local value,power=0,1
  for _=1,32 do
    if a%2 ~= b%2 then value=value+power end
    a=math.floor(a/2); b=math.floor(b/2); power=power*2
  end
  return value
end

local function rol(value,bits)
  local limit=2^(32-bits)
  return (value%limit)*2^bits + math.floor(value/limit)
end

-- The NCK input is always 32 bytes, so SHA-1 needs exactly one padded block.
-- This avoids depending on crypto modules or hash utilities absent from this build.
local function sha1_32(message)
  local words={}
  for i=0,7 do
    local a,b,c,d=message:byte(i*4+1,i*4+4)
    words[i]=((a*256+b)*256+c)*256+d
  end
  words[8]=0x80000000
  for i=9,14 do words[i]=0 end
  words[15]=256
  for i=16,79 do
    words[i]=rol(bxor(bxor(words[i-3],words[i-8]),
      bxor(words[i-14],words[i-16])),1)
  end
  local initial={0x67452301,0xefcdab89,0x98badcfe,0x10325476,0xc3d2e1f0}
  local a,b,c,d,e=unpack(initial)
  for i=0,79 do
    local f,k
    if i<20 then
      f=band(b,c)+band(0xffffffff-b,d); k=0x5a827999
    elseif i<40 then
      f=bxor(bxor(b,c),d); k=0x6ed9eba1
    elseif i<60 then
      f=band(b,c)+band(b,d)+band(c,d)-2*band(band(b,c),d); k=0x8f1bbcdc
    else
      f=bxor(bxor(b,c),d); k=0xca62c1d6
    end
    a,b,c,d,e=(rol(a,5)+f+e+k+words[i])%0x100000000,a,rol(b,30),c,d
  end
  for i,value in ipairs({a,b,c,d,e}) do
    initial[i]=(initial[i]+value)%0x100000000
  end
  return initial
end

function S.generate(imei)
  c.need(type(imei)=='string' and imei:match('^%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d$'),
    'IMEI must be exactly 15 decimal digits')
  local digits='0' .. imei
  local bcd={}
  for i=1,16,2 do bcd[#bcd+1]=string.char(tonumber(digits:sub(i,i+1),16)) end
  bcd=table.concat(bcd)
  local words=sha1_32(bcd .. string.char(0xc0,0,0,0) .. bcd ..
    string.char(0xcd,0,0,0) .. bcd)
  local code={}
  for i=1,4 do
    for shift=24,0,-8 do
      local byte=math.floor(words[i]/2^shift)%256
      code[#code+1]=tostring(bxor(math.floor(byte/16),byte%16)%10)
    end
  end
  local full=table.concat(code)
  return {ok=true,nck10=full:sub(1,10),nck16=full}
end

local json=require 'luci.jsonc'
local function at_many(commands, timeout)
  local params=json.stringify({cmds=commands,timeout=timeout or 8})
  local pipe=c.need(io.popen('/bin/ubus call hh71vm-modem at ' .. c.quote(params) ..
    ' 2>/dev/null','r'),'cannot reach modem daemon')
  local reply=json.parse(pipe:read('*a') or '')
  pipe:close()
  if not reply or reply.ok ~= true or type(reply.results) ~= 'table' then return nil end
  return reply.results
end

local function at(command)
  local results=at_many({command})
  if not results or #results ~= 1 then return nil end
  return results[1]
end

local function line_of(result, pattern)
  if not result or result.final~='OK' or type(result.lines)~='table' then return nil end
  for _,line in ipairs(result.lines) do
    local first,second=line:match(pattern)
    if first then return first,second end
  end
end

function S.live_imei()
  c.board()
  local result=at('AT+GSN')
  c.need(result and result.final=='OK' and type(result.lines)=='table',
    'live modem IMEI could not be read; enter it manually')
  local imei
  for _,line in ipairs(result.lines) do
    local value=line:match('^%s*(%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d)%s*$') or
      line:match('^%s*%+GSN:%s*(%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d)%s*$')
    if value then
      c.need(not imei,'ambiguous live modem IMEI; enter it manually')
      imei=value
    end
  end
  c.need(imei,'live modem IMEI could not be read; enter it manually')
  return {ok=true,imei=imei,source='AT+GSN'}
end

function S.normalize(raw)
  c.need(type(raw) == 'table', 'core_app returned no SIM status')
  local state=tonumber(raw.SIMLockState)
  local remaining=tonumber(raw.SIMLockRemainingTimes)
  local sim_state=tonumber(raw.SIMState)
  local category=S.classify(state)
  return {ok=true, state=state, category=category,
    facility=state and FACILITY[state] or nil,
    facility_name=state and CATEGORY[state] or nil,
    locked=category == 'locked',
    remaining_attempts=remaining,
    sim_state=sim_state,
    sim_state_name=sim_state and SIM_STATE[sim_state] or nil,
    -- core_app is holding the card on personalisation right now.
    enforcing=sim_state == SIM_LOCKED,
    plmn=raw.PLMN and tostring(raw.PLMN) or nil,
    refreshed=os.time(), backend='qualcomm-kcap-simlock'}
end

--- The home PLMN of the inserted card, from its own IMSI and EF_AD, never from the
--- network it happens to be camped on. EF_AD byte 4 carries the MNC digit count; two is
--- the common case and the fallback when the file cannot be read.
function S.home_plmn(imsi, ad_hex)
  if type(imsi)~='string' or not imsi:match('^%d%d%d%d%d%d+$') then return nil end
  local width=2
  if type(ad_hex)=='string' and #ad_hex>=8 then
    local digits=tonumber(ad_hex:sub(7,8),16)
    if digits==2 or digits==3 then width=digits end
  end
  return imsi:sub(1,3+width)
end

--- Everything the modem itself says about the card and the lock. This, not core_app's
--- cached status, is what decides whether a lock is armed.
function S.card()
  local results=at_many({'AT+CPIN?','AT+CLCK="PN",2','AT+CEREG?',
    'AT+CRSM=176,12258,0,0,10','AT+CIMI','AT+CRSM=176,28589,0,0,4'},10)
  if not results or #results < 3 then return {cpin='unknown'} end
  local state=line_of(results[1],'^%+CPIN:%s*([%w%- ]+)%s*$')
  local card={cpin=state or 'unknown'}
  if results[1] and results[1].final and results[1].final:find('SIM failure',1,true) then
    card.cpin='SIM failure'
  end
  -- While a personalisation challenge is up, this query answers ERROR. That is not "PN
  -- off", so the field stays absent rather than becoming a misleading zero.
  local pn=line_of(results[2],'^%+CLCK:%s*([01])%s*$')
  card.pn=pn and tonumber(pn) or nil
  local registration=line_of(results[3],'^%+CEREG:%s*%d+,(%d+)')
  if registration then card.registered=registration=='1' or registration=='5' end
  if card.cpin~='READY' then return card end
  local sw1,sw2=line_of(results[4],'^%+CRSM:%s*(%d+),(%d+)')
  card.readable=sw1=='144' and sw2=='0'
  local imsi=line_of(results[5],'^%s*(%d%d%d%d%d%d+)%s*$')
  local ad=line_of(results[6],'^%+CRSM:%s*144,0,"(%x+)"')
  card.home_plmn=S.home_plmn(imsi,ad)
  return card
end

-- CKLength, SimLockMode, NetworkCode and NckUnlockTimes are not compiled into core_app: it
-- binds them from this database (16 digits on the OU/N1RU build, 10 on GK, which is why
-- the GK stock UI validated ten). Reading them costs no unlock attempt. The modem ships
-- SQLite 3.7.17 (2013), which has no -readonly switch; the URI form is how this build
-- opens a database read-only, and it was verified on the device.
local DATABASE='/jrd-resource/resource/sqlite3/user_info.db3'
local QUERY="sqlite3 \"file:" .. DATABASE .. "?mode=ro\"" ..
  " \"select items||'='||value from sim_config where items in" ..
  " ('CKLength','SimLockMode','NckUnlockTimes','NetworkCode');\""
-- core_app recreates these when it arms a lock and deletes them when the lock goes away.
-- The first one is what makes core_app re-apply the saved rule at its next start, so a
-- rule left behind with no marker cannot come back on its own.
local MARKERS='/cache/simlock_active_flag /jrd-resource/resource/simlock_active_sucess_flag'

function S.parse_config(text)
  local values={}
  for line in tostring(text):gmatch('[^\n]+') do
    local key,value=line:match('^(%w+)=(%-?%d+)%s*$')
    if key then values[key]=value end
  end
  -- Report SimLockMode as a real false rather than dropping the field: "provisioned for a
  -- lock" and "could not be read" have to stay distinguishable in the status output.
  local provisioned
  if values.SimLockMode ~= nil then provisioned=(tonumber(values.SimLockMode) == 1) end
  return {code_length=S.valid_length(values.CKLength),
    lock_provisioned=provisioned,
    network_code=values.NetworkCode,
    configured_attempts=tonumber(values.NckUnlockTimes)}
end

function S.parse_uim(text)
  local count,features
  for line in tostring(text):gmatch('[^\n]+') do
    local total=line:match('^slot=00 feature_count=(%x%x)$')
    local id,verify,unblock=line:match('^slot=00 feature=(%x%x) verify=(%x%x) unblock=(%x%x)$')
    if total then
      c.need(not count,'duplicate UIM feature list')
      count=tonumber(total,16); features={}
    elseif id and features then
      features[#features+1]={id=tonumber(id,16),verify=tonumber(verify,16),
        unblock=tonumber(unblock,16)}
    else
      error('invalid UIM status',0)
    end
  end
  c.need(count and #features==count,'incomplete UIM feature list')
  return {feature_count=count,features=features}
end

local TESTED_BUILD='MPSS.TH.2.0.1.c8-00028-M9645LAAAANAZM-2.147565.5'
local BUILD_QUERY='timeout -t 5 /usr/bin/diagcmd 7c'
local ERASE='timeout -t 10 /usr/bin/diagcmd fe 00 02 01 02 00 00 00 00 00 00 00'
local BUILD_CACHE=SETTINGS .. '/diag-build'

function S.tested_build(text)
  local bytes={}
  for word in tostring(text):gmatch('%S+') do
    if not word:match('^%x%x$') then return false end
    bytes[#bytes+1]=string.char(tonumber(word,16))
  end
  return table.concat(bytes):find(TESTED_BUILD,1,true) ~= nil
end

--- Is this the Qualcomm firmware the keyless removal was verified against?
--- After any erase, DIAG stops answering queries for the rest of that Qualcomm boot -
--- `diagcmd 7c` then prints nothing and still exits 0 - while erase requests themselves
--- keep working. Refusing on a silent DIAG would make the second removal impossible for
--- no reason, so a build confirmed earlier on this same device is remembered.
function S.build_verified(q)
  local ok,text=pcall(function() return q:run(BUILD_QUERY,8) end)
  if ok and type(text)=='string' and text:match('%S') then
    local matched=S.tested_build(text)
    if matched then
      pcall(function()
        fs.mkdirr(SETTINGS)
        c.atomic(BUILD_CACHE,TESTED_BUILD)
      end)
    end
    return matched
  end
  if (c.read(BUILD_CACHE) or ''):find(TESTED_BUILD,1,true) then return true end
  return nil
end

--- The authoritative lock state, decided from the modem's own answers.
---   none        no carrier lock is armed
---   allowed     a lock is armed and the inserted SIM is one it accepts
---   challenged  a lock is armed and the inserted SIM is refused; an NCK is expected
---   unknown     the card or the modem could not be read well enough to say
function S.evaluate(card, uim, kcap)
  if type(card)~='table' then return 'unknown','the modem did not answer' end
  if card.cpin=='PH-NET PIN' then
    -- A removal clears the lock record before the card session notices: the modem lists no
    -- personalisation any more while the card is still holding the old challenge. That is a
    -- few seconds of settling, not a lock, and it must not be reported as either.
    if type(uim)=='table' and uim.feature_count==0 then
      return 'unknown','the modem no longer lists a carrier lock, but the SIM has not finished re-initialising yet'
    end
    return 'challenged'
  end
  if kcap and kcap.sim_state==SIM_LOCKED and kcap.state==0 and card.cpin~='READY' then
    return 'challenged'
  end
  if card.cpin=='SIM failure' then return 'unknown','the modem reported a SIM failure' end
  if card.cpin=='unknown' then return 'unknown','the SIM state could not be read' end
  if card.cpin~='READY' then
    return 'unknown','the SIM needs '  .. tostring(card.cpin) .. ' before the carrier lock can be read'
  end
  if type(uim)~='table' or type(uim.feature_count)~='number' then
    return 'unknown','the modem personalisation list could not be read'
  end
  if uim.feature_count > 0 then return 'allowed' end
  if card.pn==0 then return 'none' end
  return 'unknown','the modem accepted this SIM but did not report its lock facility'
end

--- Why an NCK cannot be submitted. Decided from the modem's own answers: the card state and
--- the UIM personalisation list say what is being challenged and how many attempts are left,
--- and core_app's view is only a cross-check, because its socket drops often enough that
--- refusing on its silence would block a perfectly submittable code.
function S.refusal(status)
  if status.lock_state == 'none' then return 'this modem is not challenging the inserted SIM' end
  if status.lock_state == 'allowed' then
    return 'the inserted SIM is accepted; there is nothing to unlock with a code'
  end
  if status.lock_state ~= 'challenged' then
    return 'the card is not confirmed to be on a network-lock challenge (' ..
      tostring(status.lock_state_reason or 'unknown') .. '); code submission is blocked'
  end
  local uim=status.uim
  if type(uim) ~= 'table' or type(uim.feature_count) ~= 'number' then
    return 'the modem personalisation list could not be read; refusing to write'
  end
  if uim.feature_count ~= 1 then
    return 'the modem lists ' .. tostring(uim.feature_count) ..
      ' personalisations; refusing to guess which code this is'
  end
  if uim.features[1].id ~= 0 then
    return 'this is not a network (PN) challenge; an NCK is not applicable'
  end
  -- When core_app does answer, disagreeing with the modem is a reason to stop.
  if status.state ~= nil and status.state ~= 0 then
    if status.category == 'unblock' then
      return 'the modem is in an unblock state and needs the reset key (RCK), not an NCK'
    end
    if status.category == 'forbidden' then return 'the modem reports the reset key as forbidden' end
    return 'this is not a PN network-lock challenge; an NCK is not applicable'
  end
  if type(status.attempts_left) ~= 'number' then
    return 'the remaining attempt count is unknown; refusing to write'
  end
  if status.attempts_left <= 0 then
    return 'no attempts remain; remove the lock without a code instead'
  end
  if not status.code_length then
    return 'the unlock code length could not be read from the modem; refusing to guess it'
  end
  return nil
end

--- Keyless removal works on an armed lock whether or not the inserted SIM is challenged,
--- and it works with the attempt count already at zero. It needs the firmware it was
--- verified against and a lock that is actually there.
--- The Qualcomm firmware build is advisory here, not a gate.  Keyless removal was verified
--- on one build, but these units ship with many, and refusing every other one meant nobody
--- else could try it or report back -- which is the only way the set of builds known to work
--- ever grows.  Trying costs nothing: no unlock code is sent and no attempt is consumed, so
--- a build where it does not work is a wasted click rather than a loss.  `tested_build` stays
--- in the status and the page warns when it is not the verified build, so the choice, and the
--- report that follows, belong to whoever runs the modem.
function S.erase_refusal(status)
  if status.lock_state=='challenged' or status.lock_state=='allowed' then return nil end
  if status.lock_state=='none' then return 'this modem has no carrier lock to remove' end
  return 'the lock state could not be read; removal is unavailable'
end

--- A new lock may only be created when none is armed and the inserted card's own home
--- network is known, so the router cannot lock itself away from the SIM that is in it.
function S.lock_refusal(status)
  if status.lock_state=='challenged' or status.lock_state=='allowed' then
    return 'this modem already has a carrier lock; remove it before creating a new one'
  end
  if status.lock_state~='none' then
    return 'the lock state could not be read; locking is unavailable'
  end
  if not status.card or status.card.cpin~='READY' then
    return 'a working SIM must be inserted to lock the modem to its network'
  end
  if not S.valid_plmn(status.home_plmn) then
    return 'the network of the inserted SIM could not be read from the card'
  end
  if status.lock_provisioned==nil then
    return 'the modem configuration could not be read; locking is unavailable'
  end
  return nil
end

--- A removal is only finished when the card is usable again and the modem lists no
--- personalisation feature. core_app's cached SIM state is deliberately not part of this:
--- it still reports the old challenge for a few seconds after a successful unlock, and it
--- reports 6 rather than 7 for a readable card that has no service.
function S.verified_removed(status)
  return status and status.lock_state=='none' and status.card and
    status.card.cpin=='READY' and status.card.pn==0 and
    status.uim and status.uim.feature_count==0 or false
end

local function open_ap(helper)
  return require('qualcomm').open(helper or 'uim')
end

-- Each reading stands on its own: an unreadable configuration must not also cost the
-- personalisation list, which is what actually decides the lock state.
local function read_ap(q, status, want_build)
  local errors={}
  local ok,config=pcall(function() return S.parse_config(q:run(QUERY,10)) end)
  if ok then
    status.code_length=config.code_length
    status.lock_provisioned=config.lock_provisioned
    status.configured_attempts=config.configured_attempts
    status.saved_network_code=config.network_code
    if not status.code_length then status.code_length_error='CKLength is missing or out of range' end
  else
    status.code_length_error=tostring(config)
    errors[#errors+1]=tostring(config)
  end
  local read,uim=pcall(function() return S.parse_uim(q:run(q.helper,10)) end)
  if read then status.uim=uim else errors[#errors+1]=tostring(uim) end
  if want_build then status.tested_build=S.build_verified(q) end
  if #errors > 0 then error(table.concat(errors,'; '),0) end
end

--- One complete picture: the modem's own answers first, core_app's view alongside them.
function S.inspect()
  c.board()
  local status
  local raw,kcap_error=require('kcap').call('GetSimStatus')
  if raw then
    status=S.normalize(raw)
  else
    -- A KCAP transport failure must not hide the lock: AT and UIM still answer, and they
    -- are the authoritative sources anyway.
    status={ok=true,refreshed=os.time(),backend='qualcomm-kcap-simlock',
      kcap_error=kcap_error or 'no answer from core_app'}
  end
  local card_ok,card=pcall(S.card)
  if card_ok then status.card=card else status.read_error=tostring(card) end
  local opened,q=pcall(open_ap,'uim')
  if opened then
    local ok,result=pcall(read_ap,q,status,true)
    pcall(function() q:close() end)
    if not ok then status.read_error=tostring(result) end
  else
    status.read_error=tostring(q)
  end
  local state,reason=S.evaluate(status.card,status.uim,status)
  status.lock_state=state
  status.lock_state_reason=reason
  -- Two shapes of "unknown" that the page should word differently from a plain unreadable
  -- state: the lock is already gone and the card is restarting, or the card itself failed.
  if state=='unknown' and status.card then
    if status.card.cpin=='PH-NET PIN' and status.uim and status.uim.feature_count==0 then
      status.settling_after_removal=true
    elseif status.card.cpin=='SIM failure' then
      status.sim_failure=true
    end
  end
  status.locked=state=='allowed' or state=='challenged'
  status.challenged=state=='challenged'
  status.home_plmn=status.card and status.card.home_plmn or nil
  -- The saved rule only describes the armed lock while one is armed; on a free modem it
  -- is leftover configuration and is reported as such instead of as a carrier.
  if status.locked and S.valid_plmn(status.saved_network_code) then
    status.locked_plmn=status.saved_network_code
    status.locked_to_home=status.home_plmn~=nil and status.home_plmn==status.locked_plmn
  end
  if state=='challenged' then
    status.attempts_left=status.remaining_attempts
    if status.uim and status.uim.feature_count==1 then
      status.attempts_left=status.uim.features[1].verify
    end
  end
  -- core_app keeps reporting the old challenge for a few seconds after the modem has
  -- already let the card go. Flag the disagreement instead of believing the stale side.
  status.status_settling=(state=='none' or state=='allowed') and status.enforcing==true or
    (state=='challenged' and status.sim_state==SIM_READY) or false
  status.verified_unlocked=S.verified_removed(status)
  status.unlock_refusal=S.refusal(status)
  status.can_unlock=status.unlock_refusal==nil
  status.erase_refusal=S.erase_refusal(status)
  status.can_erase=status.erase_refusal==nil
  status.lock_refusal=S.lock_refusal(status)
  status.can_lock=status.lock_refusal==nil
  return status
end

function S.pause(seconds) os.execute('sleep ' .. tostring(seconds)) end

--- Read until the modem and core_app agree, or give up after a bounded number of tries.
--- Without this a finished unlock is reported as a failure, because GetSimStatus still
--- describes the challenge the modem has already released for a few seconds.
function S.settle(attempts)
  local tries=attempts or 9
  local last
  for i=1,tries do
    local ok,status=pcall(S.inspect)
    if ok then
      last=status
      if status.lock_state~='unknown' and not status.status_settling then return status end
    end
    if i<tries then S.pause(3) end
  end
  return last
end

--- Fold a completed saved-rule cleanup back into the status that is about to be shown, so
--- the result never reports a carrier setting that this same operation has just removed.
local function apply_cleanup(status, cleanup)
  status.cleanup=cleanup
  if cleanup and cleanup.ok then
    status.lock_provisioned=false
    status.saved_network_code=cleanup.network_code or '00000'
    status.locked_plmn=nil
    status.locked_to_home=nil
    status.lock_refusal=S.lock_refusal(status)
    status.can_lock=status.lock_refusal==nil
  end
  return status
end

local function finish(before, action, note)
  local after=S.settle()
  if not after then
    after={ok=true,backend='qualcomm-kcap-simlock',refreshed=os.time(),lock_state='unknown',
      read_error='the request was sent but the modem could not be read back'}
  end
  after.attempted=true
  after.action=action
  after.state_before=before and before.lock_state or nil
  after.attempts_before=before and before.attempts_left or nil
  if note then after.note=note end
  c.atomic(cache,after)
  return after
end

--- One attempt, one request. A wrong code costs one of a finite number of attempts, and
--- the modem answers a wrong code with a bare JSON-RPC error - sometimes with no answer at
--- all - so the verdict comes from the readback, never from the reply.
function S.unlock(code, expected_state, confirmed)
  c.board()
  c.need(confirmed == true, 'explicit carrier-unlock confirmation is required')
  c.need(type(expected_state) == 'number', 'the expected lock state is required')
  local lock=guard()
  local ok,result=pcall(function()
    local before=S.inspect()
    c.need(before.lock_state=='challenged',
      before.unlock_refusal or 'this modem is not asking for an unlock code')
    -- The expected state guards against the challenge moving to another facility between
    -- the status the caller was shown and this request. core_app is the only source for it,
    -- so the comparison only applies when core_app answered; the modem's own PN check in
    -- S.refusal covers the same ground when it did not.
    if before.state ~= nil then
      c.need(before.state == expected_state,
        'the lock state changed since it was read (now ' .. tostring(before.state) ..
        ', expected ' .. tostring(expected_state) .. '); read the status again')
    end
    c.need(before.can_unlock, before.unlock_refusal or 'unlock is not permitted in this state')
    c.need(S.valid_code(code, before.code_length),
      'the unlock code must be exactly ' .. tostring(before.code_length) .. ' digits')

    -- SIMLockState is echoed back purely for parity with the stock client. The Qualcomm
    -- handler never reads it: it takes the facility from sim_config.SimLockMode and the
    -- key from SIMLockCode, and nothing else out of this object.
    local answer,err,sent=require('kcap').call('UnlockSimlock',
      {SIMLockCode=code, SIMLockState=before.state or 0}, 25)
    code=nil
    if not answer and not sent then error(err or 'could not reach core_app', 0) end

    local after=finish(before,'unlock')
    after.unlocked=after.verified_unlocked==true
    after.attempts_after=after.attempts_left
    after.attempt_consumed=type(before.attempts_left)=='number' and
      type(after.attempts_left)=='number' and after.attempts_left < before.attempts_left or false
    if after.unlocked then
      after.note='the carrier lock is gone: the SIM is readable and the modem lists no lock'
      apply_cleanup(after,S.clear_saved_rule())
    elseif after.lock_state=='challenged' then
      after.wrong_code=true
      after.note=type(after.attempts_left)=='number' and
        ('that code was not accepted; ' .. tostring(after.attempts_left) ..
         ' attempt(s) left') or 'that code was not accepted'
    else
      after.note='the result could not be confirmed; read the status again before trying anything else'
    end
    return after
  end)
  lock:close()
  c.need(ok, result)
  return result
end

--- Whole-lock removal without a code. One vendor request, never repeated: the dispatcher
--- always reports a zero status byte and usually returns no reply at all, so only the
--- AT/UIM readback decides.
function S.erase(confirmed)
  c.board()
  c.need(confirmed==true,'explicit unlock confirmation is required')
  local lock=guard()
  local ok,result=pcall(function()
    local before=S.inspect()
    c.need(before.can_erase, before.erase_refusal or 'removal is not permitted in this state')
    local q=open_ap('shell')
    local sent=false
    local accepted,reason=pcall(function()
      sent=true
      q:run(ERASE,14)
    end)
    pcall(function() q:close() end)
    if not accepted and not sent then error(reason,0) end
    local after=finish(before,'erase')
    after.erased=after.verified_unlocked==true
    if after.erased then
      after.note='the carrier lock is gone: the SIM is readable and the modem lists no lock'
      apply_cleanup(after,S.clear_saved_rule())
    elseif after.uim and after.uim.feature_count==0 then
      -- The lock record is already gone; what is missing is a usable card to confirm it on.
      after.removed_pending_card=true
      after.note='the modem no longer lists a carrier lock, but the SIM has not come back yet; ' ..
        'read the status again in a moment'
    else
      after.note='removal could not be confirmed; wait for the SIM to settle and read the status again'
    end
    return after
  end)
  lock:close()
  c.need(ok,result)
  return result
end

--- Drop the leftover provisioning rule once the modem is demonstrably free, so the saved
--- carrier cannot be re-applied later and the status stops claiming a lock is configured.
--- Only ever called after an independently verified removal.
function S.clear_saved_rule(remove_markers)
  local ok,result=pcall(function()
    local q=open_ap('shell')
    local done,reason=pcall(function()
      q:run('sqlite3 ' .. DATABASE .. ' ' .. c.quote(
        "update sim_config set value='0' where items='SimLockMode';" ..
        "update sim_config set value='00000' where items='NetworkCode';"),12)
      -- Safe only while no lock is armed: removing the marker while one is armed makes
      -- core_app provision it again at its next start.
      if remove_markers ~= false then q:run('rm -f ' .. MARKERS,10) end
      return S.parse_config(q:run(QUERY,10))
    end)
    pcall(function() q:close() end)
    c.need(done,reason)
    return reason
  end)
  if not ok then return {ok=false,error=tostring(result)} end
  return {ok=result.lock_provisioned==false,
    lock_provisioned=result.lock_provisioned,
    network_code=result.network_code}
end

--- Clear a saved rule that outlived its lock. Only allowed once the modem itself says no
--- lock is armed, which is the same condition that makes removing the markers safe; a
--- removal whose readback was interrupted leaves this behind, and nothing else retries it.
function S.forget_saved_rule()
  c.board()
  local held=guard()
  local ok,result=pcall(function()
    local before=S.inspect()
    c.need(before.lock_state=='none',
      'the saved setting can only be cleared once the modem reports no carrier lock')
    c.need(before.lock_provisioned==true,'there is no saved carrier setting to clear')
    local cleanup=S.clear_saved_rule()
    c.need(cleanup.ok,cleanup.error or 'the saved carrier setting could not be cleared')
    -- The cleanup already read the configuration back, so fold that into the status the
    -- caller was shown rather than paying for another full round trip to the modem.
    apply_cleanup(before,cleanup)
    before.action='clear_rule'
    before.attempted=true
    before.cleared=before.lock_provisioned==false
    before.note=before.cleared and 'the saved carrier setting is cleared' or
      'the saved carrier setting is still present; read the status again'
    c.atomic(cache,before)
    return before
  end)
  held:close()
  c.need(ok,result)
  return result
end

--- Lock the modem to the network of the SIM that is in it. The rule is written first,
--- core_app is restarted so it picks up the mode, and the provisioning call is made; each
--- of those was measured to be necessary. If the lock does not come up, the saved rule is
--- put back the way it was rather than left half applied.
function S.lock(plmn, confirmed)
  c.board()
  c.need(confirmed==true,'explicit carrier-lock confirmation is required')
  c.need(S.valid_plmn(plmn),'a five or six digit network code is required')
  local held=guard()
  local ok,result=pcall(function()
    local before=S.inspect()
    c.need(before.can_lock, before.lock_refusal or 'locking is not permitted in this state')
    c.need(before.home_plmn==plmn,
      'the inserted SIM belongs to network ' .. tostring(before.home_plmn) ..
      ', not ' .. plmn .. '; read the status again')
    local q=open_ap('shell')
    local applied,reason=pcall(function()
      q:run('sqlite3 ' .. DATABASE .. ' ' .. c.quote(
        "update sim_config set value='1' where items='SimLockMode';" ..
        "update sim_config set value='" .. plmn .. "' where items='NetworkCode';"),12)
      -- SimLockMode is cached by core_app at start-up, so the restart is what makes the
      -- new mode take effect; NetworkCode is read fresh when the lock is applied.
      q:run('killall core_app; sleep 18; pidof core_app >/dev/null',30)
    end)
    pcall(function() q:close() end)
    c.need(applied,reason)
    local answered
    for _=1,10 do
      if require("kcap").call("GetSimStatus",nil,6) then answered=true; break end
      S.pause(3)
    end
    c.need(answered,'the modem control service did not come back after the restart')
    require('kcap').call('ActiveSimlock',nil,25)
    local after=finish(before,'lock')
    after.locked_plmn=plmn
    if after.lock_state=='allowed' then
      after.applied=true
      after.note='this modem now accepts only SIM cards from network ' .. plmn
    else
      after.applied=false
      apply_cleanup(after,S.clear_saved_rule(after.lock_state=='none'))
      after.rollback=after.cleanup
      after.note='the lock could not be confirmed, so the saved setting was cleared again; read the status before trying again'
    end
    return after
  end)
  held:close()
  c.need(ok,result)
  return result
end

function S.cached()
  return c.json(cache) or {ok=true, unread=true, backend='qualcomm-kcap-simlock'}
end
return S
