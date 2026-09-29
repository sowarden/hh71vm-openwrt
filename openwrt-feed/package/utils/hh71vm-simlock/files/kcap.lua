-- SPDX-License-Identifier: Apache-2.0
-- Private transport for the Qualcomm control channel. Callers in this package pass
-- fixed method names; no RPC or CLI path supplies one, and nothing here is a generic
-- JSON-RPC proxy. Framing follows hh71vm-modemd, which is the proven implementation:
--     0x00 4  "kcap"                0x1c 4  zero
--     0x04 4  total length          0x20 4  caller's context handle, echoed back
--     0x08 4  message number        0x24 4  reply: JSON length without the NUL
--     0x0c 4  1 = call, 2 = event   0x28 4  reply: 0x00000101
--     0x10 4  zero                  0x2c .. JSON, NUL terminated
--     0x14 4  0 in a call, 1 in a reply
--     0x18 4  zero
-- An event frame stops after 28 bytes and carries no JSON, so a fixed 44-byte unpack
-- throws on a perfectly valid frame.
local n, c, json = require 'nixio', require 'common', require 'luci.jsonc'
local K = {host='192.168.225.1', port=2016, timeout=6, seq=0}
local HEADER, CONTEXT, LIMIT = 44, 0x77000001, 262144

function K.le32(value)
  value=value % 4294967296
  return string.char(value % 256, math.floor(value/256) % 256,
    math.floor(value/65536) % 256, math.floor(value/16777216) % 256)
end
function K.rd32(text, position)
  local a,b,d,e=text:byte(position, position+3)
  if not e then return nil end
  return a + b*256 + d*65536 + e*16777216
end

-- An empty Lua table has no type of its own and an encoder is free to write it as [];
-- core_app expects an object there, so spell the empty case out.
function K.body(method, params)
  local encoded=(type(params)=='table' and next(params)~=nil) and json.stringify(params) or '{}'
  return '{"id": "12", "jsonrpc": "2.0", "method": "' .. method .. '", "params": ' .. encoded .. '}\0'
end
function K.frame(sequence, body)
  return 'kcap' .. K.le32(HEADER + #body) .. K.le32(sequence) .. K.le32(1)
    .. K.le32(0) .. K.le32(0) .. K.le32(0) .. K.le32(0)
    .. K.le32(CONTEXT) .. K.le32(0) .. K.le32(0) .. body
end

--- Pull the first reply belonging to `sequence` out of `buffer`. Event frames and a slow
--- answer to an earlier call are skipped, not mistaken for ours. Returns the JSON text
--- (or nil when more bytes are needed), the unconsumed buffer, and a fatal error.
function K.take(buffer, sequence)
  while #buffer >= 28 do
    if buffer:sub(1,4) ~= 'kcap' then return nil, '', 'frame out of sync' end
    local total=K.rd32(buffer, 5)
    if not total or total < 28 or total > LIMIT then return nil, '', 'bad frame length' end
    if #buffer < total then return nil, buffer, nil end
    local frame=buffer:sub(1, total)
    buffer=buffer:sub(total + 1)
    if total > HEADER and K.rd32(frame, 13) == 1 and K.rd32(frame, 9) == sequence then
      return (frame:sub(HEADER + 1, total):gsub('%z+$','')), buffer, nil
    end
  end
  return nil, buffer, nil
end

function K.result(text)
  local message=json.parse(text)
  if type(message) ~= 'table' then return nil, 'unparsable core_app answer' end
  if message.error ~= nil then
    local detail=type(message.error)=='table' and message.error or {}
    return nil, detail.message or ('core_app error ' .. tostring(detail.code or message.error))
  end
  return message.result or {}
end

--- One call, one answer, one connection. Returns result, error, sent. `sent` is the
--- caller's only way to tell "never left the box" from "the modem may have acted on it":
--- a state-changing method must never be retried once this is true.
function K.call(method, params, timeout)
  local deadline=n.sysinfo().uptime + (timeout or K.timeout)
  local socket=c.need(n.socket('inet','stream'),'cannot create Qualcomm socket')
  socket:setblocking(false)
  local sent=false
  local function fail(message) pcall(function() socket:close() end); return nil, message, sent end
  local function left() return math.floor(math.max(0, deadline - n.sysinfo().uptime) * 1000) end

  local ok,code=socket:connect(K.host, K.port)
  if not ok and code ~= n.const.EINPROGRESS then return fail('cannot reach core_app: ' .. tostring(code)) end
  if not ok then
    local ready=n.poll({{fd=socket, events=n.poll_flags('out')}}, left())
    if not ready or ready < 1 then return fail('core_app connect timed out') end
    if socket:getopt('socket','error') ~= 0 then return fail('core_app refused the connection') end
  end

  K.seq=(K.seq % 65000) + 1
  local sequence=K.seq
  local out=K.frame(sequence, K.body(method, params))
  while #out > 0 do
    if n.sysinfo().uptime > deadline then return fail('core_app send timed out') end
    local ready=n.poll({{fd=socket, events=n.poll_flags('out')}}, 200)
    if ready and ready > 0 then
      local count=socket:write(out)
      if not count then return fail('core_app write failed') end
      out=out:sub(count + 1)
      sent=true -- Any byte on the wire makes an unanswered write indeterminate.
    end
  end

  local buffer=''
  while true do
    local text,rest,broken=K.take(buffer, sequence)
    if broken then return fail(broken) end
    buffer=rest
    if text then
      pcall(function() socket:close() end)
      local value,message=K.result(text)
      if not value then return nil, message, sent end
      return value, nil, sent
    end
    if n.sysinfo().uptime > deadline then return fail('no answer from core_app') end
    local ready=n.poll({{fd=socket, events=n.poll_flags('in')}}, 200)
    if ready and ready > 0 then
      local data,err=socket:read(65536)
      if data == nil then
        if err ~= n.const.EAGAIN then return fail('core_app read failed') end
      elseif data == '' then return fail('core_app closed the connection')
      else
        buffer=buffer .. data
        if #buffer > LIMIT then return fail('core_app reply too large') end
      end
    end
  end
end
return K
