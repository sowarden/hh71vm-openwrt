-- SPDX-License-Identifier: Apache-2.0
-- Helpers shared by the SIM lock backend. Deliberately small: this package exists so a
-- carrier-locked router can be unlocked from its own web interface without installing
-- anything first, so it carries only what that needs.
local n, fs, json = require 'nixio', require 'nixio.fs', require 'luci.jsonc'
local M = { directory = '/etc/hh71vm-simlock', runtime = '/var/run/hh71vm-simlock' }

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
  fs.mkdirr(fs.dirname(path))
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
return M
