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
      ' (install iptables-mod-ipopt and kmod-ipt-ipopt from the release feed)',0)
  end
  if not active then c.exec(command .. ' -t mangle -X ' .. chain .. ' 2>/dev/null') end
end
function T.apply(s)
  T.validate(s)
  if s.enabled then c.need(not T.offload(),'disable firewall flow offloading before enabling TTL Fix') end
  local device=s.enabled and T.device(s.wan_network) or nil
  apply_family('iptables','MET_TTL',4,s,device)
  apply_family('ip6tables','MET_HL',6,s,device)
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
    local restored=pcall(function() T.apply(old); T.save(old) end)
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
function T.status()
  local s=T.config(); s.ok=true
  s.flow_offload_detected=T.offload()
  local ok,device=pcall(T.device,s.wan_network)
  if ok then s.wan_device=device else s.warning=device end
  local function active(command,chain,target,value)
    if not ok then return false end
    local jump=output(command .. ' -t mangle -S POSTROUTING 2>/dev/null')
    local rules=output(command .. ' -t mangle -S ' .. chain .. ' 2>/dev/null')
    return jump:find('-A POSTROUTING -j ' .. chain .. '\n',1,true)~=nil
      and rules:find('-A ' .. chain .. ' -o ' .. device .. ' -j ' .. target .. value .. '\n',1,true)~=nil
  end
  s.ipv4_active=active('iptables','MET_TTL','TTL --ttl-set ',s.ipv4_value)
  s.ipv6_active=active('ip6tables','MET_HL','HL --hl-set ',s.ipv6_value)
  return s
end
return T
