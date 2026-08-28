-- SPDX-License-Identifier: Apache-2.0
-- Private transport. No RPC/CLI method accepts shell text or arbitrary NV items.
local n = require 'nixio'
local c = require 'common'
local Q = {}; Q.__index = Q
local function now() return n.sysinfo().uptime end
function Q:send(text)
  local end_at = now() + 5
  while #text > 0 do
    c.need(now() < end_at, 'Qualcomm send timeout')
    local count = self.socket:write(text)
    if count then text = text:sub(count + 1)
    else n.poll({{fd=self.socket, events=n.poll_flags('out')}}, 100) end
  end
end
function Q:receive()
  local ready = n.poll({{fd=self.socket, events=n.poll_flags('in')}}, 100)
  if not ready or ready == 0 then return '' end
  local data, err = self.socket:read(8192)
  if not data and err == n.const.EAGAIN then return '' end
  c.need(data and data ~= '', 'Qualcomm shell disconnected')
  local output = {}
  for i=1,#data do
    local b = data:byte(i)
    if self.telnet == 'iac' then
      if b == 255 then output[#output+1]=string.char(b); self.telnet=nil
      elseif b == 251 or b == 252 or b == 253 or b == 254 then self.option=b; self.telnet='option'
      elseif b == 250 then self.telnet='sub'
      else self.telnet=nil end
    elseif self.telnet == 'option' then
      if self.option == 251 then self:send(string.char(255,254,b)) end
      if self.option == 253 then self:send(string.char(255,252,b)) end
      self.telnet=nil
    elseif self.telnet == 'sub' then if b == 255 then self.telnet='sub-iac' end
    elseif self.telnet == 'sub-iac' then
      if b == 240 then self.telnet=nil else self.telnet='sub' end
    elseif b == 255 then self.telnet='iac'
    else output[#output+1]=string.char(b) end
  end
  return table.concat(output):gsub('\r','')
end
function Q:run(command, timeout)
  c.need(#command < 900 and not command:find('\n',1,true), 'internal shell line exceeds safe length')
  self.sequence=self.sequence+1
  local marker='MET_DONE_' .. n.getpid() .. '_' .. self.sequence
  -- Keep the acknowledgement on the same shell line: stty may flush queued
  -- input, discarding a second line during initial terminal setup.
  self:send(command .. "; printf '\\n" .. marker .. ":%s\\n' \"$?\"\n")
  local buffer, end_at = '', now() + (timeout or 8)
  while now() < end_at do
    buffer=buffer .. self:receive()
    c.need(#buffer < 65536, 'excessive Qualcomm reply')
    local body, code=buffer:match('^(.-)\n' .. marker .. ':(%d+)\n')
    if body then
      c.need(tonumber(code)==0, 'Qualcomm operation failed: ' .. body:gsub('^%s+',''):gsub('%s+$',''))
      return (body:gsub('^%s+',''):gsub('%s+$',''))
    end
  end
  error('Qualcomm operation ' .. self.sequence .. ' timed out; inspect pending transaction before retrying',0)
end
function Q:upload(name, content, expected_hash)
  c.need(name:match('^[a-z0-9-]+$'), 'invalid internal filename')
  local path=self.directory .. '/' .. name
  self:run(': > ' .. path)
  for offset=1,#content,160 do
    local escaped=content:sub(offset,offset+159):gsub('.',function(b) return ('\\%03o'):format(b:byte()) end)
    self:run("printf '" .. escaped .. "' >> " .. path)
  end
  self:run('test "$(wc -c < ' .. path .. ')" -eq ' .. #content)
  if expected_hash then
    self:run('test "$(sha256sum ' .. path .. " | cut -d ' ' -f1)\" = '" .. expected_hash .. "'")
  end
  return path
end
function Q.open(helper_name)
  helper_name=helper_name or 'nas'
  c.need(helper_name=='nas' or helper_name=='imei','unknown internal Qualcomm helper')
  local self=setmetatable({sequence=0,directory='/tmp/modem-extra-' .. n.getpid()},Q)
  local success,result=pcall(function()
  self.socket=c.need(n.socket('inet','stream'),'cannot create modem socket')
  self.socket:setblocking(false)
  local ok,err=self.socket:connect('192.168.225.1',23)
  if not ok then
    c.need(err==n.const.EINPROGRESS,'cannot connect to Qualcomm')
    local ready=n.poll({{fd=self.socket,events=n.poll_flags('out')}},3000)
    c.need(ready and ready>0 and self.socket:getopt('socket','error')==0,'Qualcomm connect timeout')
  end
  local end_at=now()+1
  while now()<end_at do self:receive() end
  self:run('stty -echo 2>/dev/null; PS1=')
  self:run('umask 077; mkdir ' .. self.directory)
  local helper='/usr/libexec/modem-extra-tools/hh71-' .. helper_name .. '-arm'
  self.helper=self:upload(helper_name,c.need(c.read(helper),'missing Qualcomm helper'),c.hash(helper))
  self:run('chmod 700 ' .. self.helper)
  return self
  end)
  if not success then
    if self.socket then self.socket:close() end
    error(result,0)
  end
  return result
end
function Q:close()
  pcall(function() self:run('rm -f ' .. self.directory .. '/nas ' .. self.directory .. '/imei; rmdir ' .. self.directory) end)
  self.socket:close()
end
return Q
