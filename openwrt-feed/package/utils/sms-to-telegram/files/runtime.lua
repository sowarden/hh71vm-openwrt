-- SPDX-License-Identifier: Apache-2.0

local n, fs, json, uci, ubus = require 'nixio', require 'nixio.fs', require 'luci.jsonc', require 'uci', require 'ubus'
local core = require 'sms_to_telegram'
local R = {}

local STATE_DIR = '/etc/sms-to-telegram'
local STATE_FILE = STATE_DIR .. '/state.json'
local RUN_DIR = '/var/run/sms-to-telegram'
local PID_FILE = RUN_DIR .. '/pid'
local HTTP_HELPER = '/usr/libexec/sms-to-telegram/http'
local sequence = 0

local function quote(value)
	return "'" .. tostring(value):gsub("'", "'\\''") .. "'"
end

local function mkdir(path, mode)
	fs.mkdirr(path)
	fs.chmod(path, mode)
end

local function atomic(path, value)
	local temporary = path .. '.new.' .. n.getpid()
	local fd = assert(n.open(temporary, 'w', 600), 'state_write_failed')
	local body = type(value) == 'table' and json.stringify(value) or tostring(value)
	local ok = fd:writeall(body .. '\n') and fd:sync()
	fd:close()
	if not ok then fs.unlink(temporary); error('state_write_failed', 0) end
	assert(fs.rename(temporary, path), 'state_replace_failed')
	fs.chmod(path, 600)
end

local function temporary_file(body)
	mkdir(RUN_DIR, 700)
	sequence = sequence + 1
	local path = ('%s/request-%d-%d'):format(RUN_DIR, n.getpid(), sequence)
	local fd = assert(n.open(path, 'w', 600), 'temporary_file_failed')
	local ok = fd:writeall(body) and fd:sync()
	fd:close()
	if not ok then fs.unlink(path); error('temporary_file_failed', 0) end
	return path
end

local function read_state()
	local body = fs.readfile(STATE_FILE)
	if not body then return { schema = 1, records = {} } end
	local ok, value = pcall(json.parse, body)
	if ok and type(value) == 'table' then return value end
	return { schema = 1, records = {} }
end

local function write_state(value)
	mkdir(STATE_DIR, 700)
	atomic(STATE_FILE, value)
end

function R.read_config()
	local cursor = uci.cursor()
	local result = {
		token = cursor:get('sms-to-telegram', 'main', 'token') or '',
		chat_id = cursor:get('sms-to-telegram', 'main', 'chat_id') or '',
		remove_after_send = cursor:get('sms-to-telegram', 'main', 'remove_after_send') == '1',
		poll_interval = tonumber(cursor:get('sms-to-telegram', 'main', 'poll_interval')) or 15,
	}
	fs.chmod('/etc/config/sms-to-telegram', 600)
	return result
end

function R.write_config(update)
	local current = R.read_config()
	local merged, err = core.merge_config(current, update)
	if not merged then return { ok = false, error = err } end
	local cursor = uci.cursor()
	cursor:set('sms-to-telegram', 'main', 'sms_to_telegram')
	cursor:set('sms-to-telegram', 'main', 'token', merged.token)
	cursor:set('sms-to-telegram', 'main', 'chat_id', merged.chat_id)
	cursor:set('sms-to-telegram', 'main', 'remove_after_send', merged.remove_after_send and '1' or '0')
	if not cursor:commit('sms-to-telegram') then return { ok = false, error = 'config_write_failed' } end
	fs.chmod('/etc/config/sms-to-telegram', 600)
	return { ok = true, configured = merged.token ~= '' and merged.chat_id ~= '' }
end

local function modem_call(method, params)
	local connection = ubus.connect()
	if not connection then return { ok = false } end
	local ok, result = pcall(connection.call, connection, 'hh71vm-modem', method, params or {})
	connection:close()
	if not ok or type(result) ~= 'table' then return { ok = false } end
	return result
end

local function http_request(token, method, payload)
	if not core.valid_token(token) then return { ok = false, error = 'invalid_token' } end
	local input = temporary_file(token .. '\n' .. method .. '\n' .. json.stringify(payload) .. '\n')
	local pipe = io.popen(HTTP_HELPER .. ' ' .. quote(input) .. ' 2>/dev/null', 'r')
	if not pipe then fs.unlink(input); return { ok = false, error = 'telegram_transport_failed' } end
	local output = pipe:read('*a') or ''
	local closed = pipe:close()
	fs.unlink(input) -- The helper normally unlinks it immediately after opening.
	if not closed then return { ok = false, error = 'telegram_transport_failed' } end
	local status, body = output:match('^(%d+)\n(.*)$')
	status = tonumber(status)
	local parsed = body and json.parse(body) or nil
	return core.telegram_response(status, parsed)
end

local function fingerprint(material)
	local path = temporary_file(material)
	local pipe = io.popen('sha256sum ' .. quote(path) .. ' 2>/dev/null', 'r')
	if not pipe then fs.unlink(path); error('fingerprint_failed', 0) end
	local digest = (pipe:read('*a') or ''):match('^([0-9a-f]+)')
	pipe:close()
	fs.unlink(path)
	if not digest or #digest ~= 64 then error('fingerprint_failed', 0) end
	return digest
end

local function environment()
	return {
		now = os.time,
		save_state = write_state,
		fingerprint = fingerprint,
		snapshot = function() return modem_call('sms_snapshot') end,
		delete_sms = function(indexes) return modem_call('sms_delete', { indexes = indexes }) end,
		readback = function()
			local result = modem_call('sms_list')
			return { ok = result.ok == true, messages = result.messages or {} }
		end,
		send = function(token, chat_id, text)
			return http_request(token, 'sendMessage', { chat_id = chat_id, text = text })
		end,
	}
end

local function running()
	local pid = tonumber((fs.readfile(PID_FILE) or ''):match('%d+'))
	return pid and n.kill(pid, 0) == true or false
end

function R.status()
	local config = R.read_config()
	return core.safe_status(read_state(), core.valid_token(config.token) and core.valid_chat_id(config.chat_id), running())
end

function R.config_get()
	local config = R.read_config()
	return { ok = true, token_set = core.valid_token(config.token), chat_id = config.chat_id,
		remove_after_send = config.remove_after_send }
end

function R.discover_chat(token)
	local config = R.read_config()
	token = token ~= '' and token or config.token
	if not core.valid_token(token) then return { ok = false, error = 'invalid_token' } end
	local response = http_request(token, 'getUpdates', { limit = 50, timeout = 0, allowed_updates = { 'message' } })
	return core.discovery_result(response)
end

function R.run(once)
	mkdir(STATE_DIR, 700)
	mkdir(RUN_DIR, 700)
	local lock = assert(n.open(RUN_DIR .. '/service.lock', 'w', 600), 'service_lock_failed')
	if not lock:lock('tlock') then lock:close(); error('service_already_running', 0) end
	atomic(PID_FILE, tostring(n.getpid()))
	local active = true
	pcall(function() n.signal(15, function() active = false end) end)
	pcall(function() n.signal(2, function() active = false end) end)
	local engine = core.new_engine(environment(), read_state())
	repeat
		local config = R.read_config()
		local ok, err = pcall(engine.step, engine, config)
		if not ok then
			engine.state.last_error = 'internal_error'
			engine.state.last_error_time = os.time()
			pcall(write_state, engine.state)
			n.syslog('err', 'sms-to-telegram: internal_error')
		end
		if not once and active then
			local delay = math.max(10, math.min(300, tonumber(config.poll_interval) or 15))
			n.nanosleep(delay)
		end
	until once or not active
	fs.unlink(PID_FILE)
	lock:close()
	return true
end

return R
