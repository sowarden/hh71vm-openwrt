-- SPDX-License-Identifier: Apache-2.0
local c, uci, json = require 'common', require 'uci', require 'luci.jsonc'
local T={}
local config='modem-extra-tools'
local function output(command)
  local p=c.need(io.popen(command,'r'),'cannot run local status command')
  local text=p:read('*a') or ''; p:close(); return text
end
function T.config()
  local u=uci.cursor()
  return {enabled=u:get(config,'ttl','enabled')=='1',
    ipv4_value=tonumber(u:get(config,'ttl','ipv4_value')) or 65,
    ipv6_enabled=u:get(config,'ttl','ipv6_enabled')=='1',
    ipv6_value=tonumber(u:get(config,'ttl','ipv6_value')) or 65,
    wan_network=u:get(config,'ttl','wan_network') or 'wan'}
end
function T.validate(s)
  s.ipv4_value=c.uint(s.ipv4_value,1,255,'IPv4 TTL')
  s.ipv6_value=c.uint(s.ipv6_value,1,255,'IPv6 Hop Limit')
  c.need(type(s.wan_network)=='string' and #s.wan_network<=32 and
    s.wan_network:match('^[%w_]+$'),'invalid WAN network name')
  return s
end
function T.offload()
  local u=uci.cursor(); local enabled=false
  u:foreach('firewall','defaults',function(s)
    if s.flow_offloading=='1' or s.flow_offloading_hw=='1' then enabled=true end
  end)
  return enabled or output('iptables-save 2>/dev/null'):find('-j FLOWOFFLOAD',1,true)~=nil
    or output('ip6tables-save 2>/dev/null'):find('-j FLOWOFFLOAD',1,true)~=nil
end
function T.device(network)
  local s=json.parse(output('ubus call network.interface.' .. c.quote(network) .. ' status 2>/dev/null'))
  local device=type(s)=='table' and s.l3_device
  c.need(type(device)=='string' and #device<=15 and device:match('^[%w_.:-]+$'),
    'WAN network is down or has no L3 device: ' .. network)
  c.need(device~='br-lan' and device~='lo','refusing to normalize the LAN or loopback interface')
  return device
end
local function apply_family(command, chain, version, s, device)
  local active=s.enabled and (version==4 or s.ipv6_enabled)
  local existing=output(command .. ' -t mangle -S POSTROUTING 2>/dev/null')
  local lines={'*mangle',':' .. chain .. ' - [0:0]','-F ' .. chain}
  -- Only the exact jump owned by this package may be deleted.
  for line in existing:gmatch('[^\n]+') do
    if line=='-A POSTROUTING -j ' .. chain then lines[#lines+1]='-D POSTROUTING -j ' .. chain end
  end
  if active then
    if version==4 then
      lines[#lines+1]='-A ' .. chain .. ' -d 192.168.225.0/24 -j RETURN'
      lines[#lines+1]='-A ' .. chain .. ' -o ' .. device .. ' -j TTL --ttl-set ' .. s.ipv4_value
    else
      lines[#lines+1]='-A ' .. chain .. ' -d fe80::/10 -j RETURN'
      lines[#lines+1]='-A ' .. chain .. ' -d ff00::/8 -j RETURN'
      lines[#lines+1]='-A ' .. chain .. ' -o ' .. device .. ' -j HL --hl-set ' .. s.ipv6_value
    end
    lines[#lines+1]='-I POSTROUTING 1 -j ' .. chain
  end
  lines[#lines+1]='COMMIT'
  local path=c.runtime .. '/rules' .. version
  c.atomic(path,table.concat(lines,'\n'))
  -- Keep what iptables actually said. The old wording lumped an absent kernel target
  -- together with an absent userspace extension, and those need different packages.
  local report=path .. '.error'
  if not c.exec(command .. '-restore -w 5 --noflush < ' .. c.quote(path) ..
      ' 2>' .. c.quote(report)) then
    local detail=(c.read(report) or ''):match('^[^\n]*') or ''
    error(command .. ' rejected the TTL/HL rules: ' ..
      (detail~='' and detail or 'no diagnostic') ..
      ' (needs iptables-mod-ipopt and kmod-hh71vm-ipt-ipopt; both ship in the image' ..
      ' since 2026-09-16 -- on an older one, opkg install them from the release feed)',0)
  end
  if not active then c.exec(command .. ' -t mangle -X ' .. chain .. ' 2>/dev/null') end
end
-- Remove our own rules without touching the saved setting. Used when the mobile WAN
-- has no L3 device yet: a rule pinned to a device that is not there is worse than no
-- rule, and leaving whatever the previous device was named behind is worse still.
local function take_down(s)
  local off={}; for key,value in pairs(s) do off[key]=value end; off.enabled=false
  apply_family('iptables','MET_TTL',4,off,nil)
  apply_family('ip6tables','MET_HL',6,off,nil)
end
--- `deferrable` is for the callers that run on their own schedule rather than because
--- the user asked for something right now -- the fw3 include and the reconciler. For
--- them a mobile WAN with no L3 device is the ordinary state, not an error: at boot
--- fw3 starts long before the Qualcomm side has enumerated its RNDIS gadget, and any
--- fw3 reload afterwards (a firewall save, Xray installing or removing its own rules)
--- re-runs the include at whatever moment it happens to land in.
---
--- It used to throw there, which left the rules simply absent with the saved setting
--- still reading enabled -- the TTL Fix showing as on in LuCI while nothing was
--- rewriting anything, until the user toggled it off and on again by hand. That is
--- the reported symptom (2026-09-12..14: no internet after a power cycle, fixed by
--- disabling and re-enabling the TTL Fix). Returns false instead, having taken our
--- own rules down, and T.reconcile() puts them back when the device appears.
function T.apply(s,deferrable)
  T.validate(s)
  if s.enabled then c.need(not T.offload(),'disable firewall flow offloading before enabling TTL Fix') end
  local device=nil
  if s.enabled then
    local found,result=pcall(T.device,s.wan_network)
    if not found then
      if not deferrable then error(tostring(result),0) end
      take_down(s)
      return false,tostring(result)
    end
    device=result
  end
  apply_family('iptables','MET_TTL',4,s,device)
  apply_family('ip6tables','MET_HL',6,s,device)
  return true
end
function T.save(s)
  local u=uci.cursor()
  if not u:get(config,'ttl') then u:section(config,'ttl','ttl',{}) end
  for _,key in ipairs({'enabled','ipv6_enabled','ipv4_value','ipv6_value','wan_network'}) do
    local value=s[key]
    if type(value)=='boolean' then value=value and '1' or '0' end
    c.need(u:set(config,'ttl',key,tostring(value)),'cannot stage TTL configuration')
  end
  c.need(u:commit(config),'cannot persist TTL configuration')
end
function T.change(s)
  local old=T.config()
  T.validate(s)
  if s.enabled then c.need(not T.offload(),'disable firewall flow offloading before enabling TTL Fix') end
  local ok,err=pcall(function() T.apply(s); T.save(s) end)
  if not ok then
    -- Roll back deferrably: if the previous settings had the fix on and the mobile WAN
    -- is simply not up at this instant, that is not a reason to rewrite the user's
    -- choice. Keep it saved, leave no rules behind, and let T.reconcile() install them
    -- when the device appears -- switching the feature off here used to turn a
    -- momentary "WAN is down" into a silently disabled TTL Fix.
    local restored=pcall(function() T.apply(old,true); T.save(old) end)
    if restored then error(tostring(err) .. '; previous settings restored',0) end
    -- Rolling back into a state the board cannot reapply turns one failure into a permanent
    -- one: every later save, and every firewall reload, retries the same broken rules and
    -- fails the same way. Switching the feature off is always applicable, so fall back to it.
    local off={}; for key,value in pairs(old) do off[key]=value end; off.enabled=false
    local cleared,clear_error=pcall(function() T.apply(off); T.save(off) end)
    error(tostring(err) .. (cleared and '; TTL Fix switched off'
      or '; rollback failed: ' .. tostring(clear_error)),0)
  end
  return T.status()
end
local function family_active(command,chain,target,value,device)
  if not device then return false end
  local jump=output(command .. ' -t mangle -S POSTROUTING 2>/dev/null')
  local rules=output(command .. ' -t mangle -S ' .. chain .. ' 2>/dev/null')
  return jump:find('-A POSTROUTING -j ' .. chain .. '\n',1,true)~=nil
    and rules:find('-A ' .. chain .. ' -o ' .. device .. ' -j ' .. target .. value .. '\n',1,true)~=nil
end
function T.status()
  local s=T.config(); s.ok=true
  s.flow_offload_detected=T.offload()
  local ok,device=pcall(T.device,s.wan_network)
  if ok then s.wan_device=device else s.warning=device; device=nil end
  s.ipv4_active=family_active('iptables','MET_TTL','TTL --ttl-set ',s.ipv4_value,device)
  s.ipv6_active=family_active('ip6tables','MET_HL','HL --hl-set ',s.ipv6_value,device)
  return s
end
--- Put the rules back if the saved setting says they should be there and they are not.
---
--- Nothing else on this board converges them. The rules are installed from exactly two
--- events -- the fw3 include, and `ifup` of the mobile WAN -- and both are one-shots
--- that can miss: `wan` is `proto static` on eth2, so netifd raises it once when the
--- RNDIS gadget enumerates and never again, however often the data session behind it
--- drops and returns; and any fw3 reload after that point flushes mangle POSTROUTING,
--- taking our jump with it, and re-runs the include at a moment the WAN device may not
--- be resolvable. Either miss left the TTL Fix reading enabled with no rule behind it
--- until the user toggled it by hand. Called once a minute from `maintain`.
---
--- Deliberately cheap when there is nothing to do: two `-S` reads per family and no
--- iptables-save, so the offload scan only happens on the path that actually applies.
function T.reconcile()
  local s=T.config()
  if not s.enabled then return {ok=true,enabled=false,changed=false} end
  local found,device=pcall(T.device,s.wan_network)
  if found and family_active('iptables','MET_TTL','TTL --ttl-set ',s.ipv4_value,device)
    and (not s.ipv6_enabled
      or family_active('ip6tables','MET_HL','HL --hl-set ',s.ipv6_value,device)) then
    return {ok=true,enabled=true,changed=false}
  end
  local applied,why=T.apply(s,true)
  return {ok=true,enabled=true,changed=applied and true or false,
    deferred=(not applied) or nil,reason=why}
end
return T
