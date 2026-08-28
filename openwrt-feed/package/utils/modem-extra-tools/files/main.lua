#!/usr/bin/lua
-- SPDX-License-Identifier: Apache-2.0
package.path='/usr/libexec/modem-extra-tools/?.lua;' .. package.path
local c, json = require 'common', require 'luci.jsonc'
local arguments, json_output, imei_confirmation={},false,false
for _,value in ipairs(arg) do
  if value=='--json' then json_output=true
  elseif value=='--confirm-original-imei' then imei_confirmation=true
  else arguments[#arguments+1]=value end
end
local function main()
  local command,sub=arguments[1],arguments[2]
  if command=='status' then
    return {ok=true,ttl=require('ttl').status(),bands=require('bands').cached(),
      imei=require('imei').cached()}
  elseif command=='ttl' or command=='firewall' then
    local t=require 'ttl'
    if command=='ttl' and (not sub or sub=='show') then return t.status() end
    local lock=c.lock('ttl'); local state=t.config()
    local ok,result=pcall(function()
      if command=='firewall' then
        c.need(not sub or sub=='disable','unexpected firewall argument')
        if sub=='disable' then state.enabled=false end
        t.apply(state); return {ok=true}
      end
      if sub=='disable' then
        c.need(#arguments==2,'usage: ttl disable'); state.enabled=false
      elseif sub=='set' then
        c.need(#arguments>=3 and #arguments<=5,'usage: ttl set IPV4 [IPV6|off] [WAN_NETWORK]')
        state.enabled=true; state.ipv4_value=c.uint(arguments[3],1,255,'IPv4 TTL')
        state.ipv6_enabled=arguments[4]~=nil and arguments[4]~='off'
        if state.ipv6_enabled then state.ipv6_value=c.uint(arguments[4],1,255,'IPv6 Hop Limit') end
        state.wan_network=arguments[5] or state.wan_network
      else error('unknown TTL command',0) end
      return t.change(state)
    end)
    lock:close(); c.need(ok,result); return result
  elseif command=='bands' then
    local allowed={show=true,backup=true,set=true,restore=true,undo=true,recover=true,reconcile=true}
    sub=sub or 'show'; c.need(allowed[sub],'unknown band command')
    local text
    if sub=='set' then
      c.need(arguments[3],'usage: bands set 3,7')
      text=arguments[3]
      c.need(#arguments==3,'unexpected band argument (QMI applies without a reboot)')
    else
      c.need(#arguments<=2,'unexpected band argument (QMI applies without a reboot)')
    end
    return require('bands').execute(sub,text)
  elseif command=='imei' then
    sub=sub or 'show'
    c.need(sub=='show' or sub=='restore' or sub=='recover','unknown IMEI command')
    if sub=='restore' then
      c.need(#arguments==3,'usage: imei restore 15_DIGIT_ORIGINAL_IMEI --confirm-original-imei')
      return require('imei').execute('restore',arguments[3],imei_confirmation)
    end
    c.need(#arguments<=2 and not imei_confirmation,'unexpected IMEI argument')
    return require('imei').execute(sub)
  end
  error('Usage:\n  modem-extra-tools status [--json]\n  modem-extra-tools ttl show|disable\n  modem-extra-tools ttl set IPV4 [IPV6|off] [MOBILE_WAN_NETWORK]\n  modem-extra-tools bands show|backup\n  modem-extra-tools bands set 3,7\n  modem-extra-tools bands restore|undo|recover\n  modem-extra-tools imei show|recover\n  modem-extra-tools imei restore 15_DIGIT_ORIGINAL_IMEI --confirm-original-imei',0)
end
local ok,result=pcall(main)
if not ok then result={ok=false,error=tostring(result)} end
if json_output then print(json.stringify(result))
elseif not ok then io.stderr:write(result.error,'\n')
else
  -- Same schema in terminal and RPC. Pretty JSON is unambiguous and scriptable.
  print(json.stringify(result,true))
end
os.exit(ok and 0 or 1)
