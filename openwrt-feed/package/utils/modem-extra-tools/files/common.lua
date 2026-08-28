-- SPDX-License-Identifier: Apache-2.0
local n, fs, json = require 'nixio', require 'nixio.fs', require 'luci.jsonc'
local M = { directory = '/etc/modem-extra-tools', runtime = '/var/run/modem-extra-tools' }

function M.need(ok, message)
  if not ok then error(message, 0) end
  return ok
end
function M.quote(value)
  return "'" .. tostring(value):gsub("'", "'\\''") .. "'"
end
function M.exec(command) return os.execute(command) == 0 end
function M.read(path) return fs.readfile(path) end
function M.json(path)
  local text = M.read(path)
  return text and json.parse(text) or nil
end
function M.atomic(path, value)
  local temporary = path .. '.new.' .. n.getpid()
  local fd = M.need(n.open(temporary, 'w', 600), 'cannot create ' .. temporary)
  local content = type(value) == 'table' and json.stringify(value) or value
  local ok = fd:writeall(content .. '\n')
  local synced = fd:sync()
  fd:close()
  if not ok or not synced then fs.unlink(temporary); error('cannot persist ' .. path, 0) end
  M.need(fs.rename(temporary, path), 'cannot replace ' .. path)
  M.need(M.exec('sync'), 'filesystem sync failed')
end
function M.lock(name)
  fs.mkdirr(M.runtime)
  local fd = M.need(n.open(M.runtime .. '/' .. name .. '.lock', 'w', 600), 'cannot open operation lock')
  if not fd:lock('tlock') then fd:close(); error('another operation is running; retry when it finishes', 0) end
  return fd -- Kernel releases this lock after crashes too. Never unlink lock files.
end
function M.uint(value, low, high, label)
  M.need(type(value) == 'number' or type(value) == 'string', label .. ': integer required')
  if type(value) == 'string' then M.need(value:match('^%d+$'), label .. ': integer required') end
  local number = tonumber(value)
  M.need(number and number == math.floor(number) and number >= low and number <= high,
    label .. ': expected ' .. low .. '..' .. high)
  return number
end
function M.hash(path)
  local pipe = M.need(io.popen('sha256sum ' .. M.quote(path), 'r'), 'cannot calculate SHA256')
  local digest = (pipe:read('*a') or ''):match('^([0-9a-f]+)')
  pipe:close()
  M.need(digest and #digest == 64, 'SHA256 failed')
  return digest
end
function M.board()
  local board = (M.read('/tmp/sysinfo/board_name') or ''):gsub('%s+$', '')
  M.need(board == 'hh71vm', 'unsupported OpenWrt board: ' .. board)
  return board
end
function M.model()
  M.board()
  local model = (M.read('/tmp/sysinfo/model') or ''):gsub('%s+$', '')
  M.need(model ~= '', 'router model is unavailable')
  return model
end
return M
