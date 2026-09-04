-- SPDX-License-Identifier: Apache-2.0

local M = {}

local function copy(value)
	if type(value) ~= 'table' then return value end
	local result = {}
	for key, item in pairs(value) do result[copy(key)] = copy(item) end
	return result
end

function M.valid_token(value)
	return type(value) == 'string' and #value >= 30 and #value <= 128 and
		value:match('^[1-9][0-9]+:[A-Za-z0-9_-]+$') ~= nil
end

function M.valid_chat_id(value)
	if type(value) == 'number' then value = ('%.0f'):format(value) end
	return type(value) == 'string' and #value >= 5 and #value <= 20 and
		value:match('^[1-9][0-9]+$') ~= nil
end

function M.valid_proxy_type(value)
	return value == 'none' or value == 'http' or value == 'socks5'
end

function M.valid_proxy_host(value)
	return type(value) == 'string' and #value >= 1 and #value <= 253 and
		value:match('^[A-Za-z0-9_.:%%%-]+$') ~= nil
end

function M.valid_proxy_port(value)
	local number = tonumber(value)
	return number and number == math.floor(number) and number >= 1 and number <= 65535
end

function M.valid_proxy_credential(value)
	return type(value) == 'string' and #value <= 256 and value:find('[%c]') == nil
end

function M.proxy_config(config)
	config = config or {}
	local proxy = {
		type = config.proxy_type or config.type or 'none',
		host = config.proxy_host or config.host or '',
		port = tostring(config.proxy_port or config.port or ''),
		username = config.proxy_username or config.username or '',
		password = config.proxy_password or config.password or '',
	}
	if not M.valid_proxy_type(proxy.type) then return nil, 'invalid_proxy_type' end
	if proxy.host ~= '' and not M.valid_proxy_host(proxy.host) then return nil, 'invalid_proxy_host' end
	if proxy.port ~= '' and not M.valid_proxy_port(proxy.port) then return nil, 'invalid_proxy_port' end
	if not M.valid_proxy_credential(proxy.username) or not M.valid_proxy_credential(proxy.password) then
		return nil, 'invalid_proxy_credentials'
	end
	if proxy.type ~= 'none' then
		if proxy.host == '' then return nil, 'invalid_proxy_host' end
		if proxy.port == '' then return nil, 'invalid_proxy_port' end
	end
	return proxy
end

function M.valid_indexes(value)
	if type(value) ~= 'table' or #value < 1 or #value > 16 then return nil end
	local result, seen = {}, {}
	for _, item in ipairs(value) do
		local number = tonumber(item)
		if not number or number ~= math.floor(number) or number < 0 or number > 65535 or seen[number] then
			return nil
		end
		seen[number] = true
		result[#result + 1] = number
	end
	table.sort(result)
	return result
end

function M.message_indexes(message)
	local indexes = message and message.indexes
	if type(indexes) ~= 'table' and message and message.index ~= nil then indexes = { message.index } end
	return M.valid_indexes(indexes)
end

function M.compose(message)
	local sender = type(message.sender) == 'string' and message.sender or 'unknown'
	local text = type(message.text) == 'string' and message.text or ''
	local lines = { 'SMS from: ' .. sender }
	if type(message.ts) == 'string' and message.ts ~= '' then
		lines[#lines + 1] = 'Time: ' .. message.ts
	end
	lines[#lines + 1] = ''
	lines[#lines + 1] = text
	return table.concat(lines, '\n')
end

function M.fingerprint_material(message)
	local indexes = M.message_indexes(message)
	if not indexes or type(message.sender) ~= 'string' or type(message.text) ~= 'string' then return nil end
	return table.concat({ table.concat(indexes, ','), message.sender,
		type(message.ts) == 'string' and message.ts or '', message.text }, '\0')
end

function M.merge_config(current, update)
	current, update = current or {}, update or {}
	local token = current.token or ''
	if update.token ~= nil and update.token ~= '' then token = update.token end
	local chat_id = update.chat_id ~= nil and tostring(update.chat_id) or tostring(current.chat_id or '')
	local remove = update.remove_after_send
	if remove == nil then remove = current.remove_after_send == true or current.remove_after_send == '1' end
	local proxy_password = current.proxy_password or ''
	if update.clear_proxy_password == true then proxy_password = ''
	elseif update.proxy_password ~= nil and update.proxy_password ~= '' then proxy_password = update.proxy_password end
	local merged = {
		token = token,
		chat_id = chat_id,
		remove_after_send = remove == true,
		proxy_type = update.proxy_type ~= nil and update.proxy_type or current.proxy_type or 'none',
		proxy_host = update.proxy_host ~= nil and update.proxy_host or current.proxy_host or '',
		proxy_port = tostring(update.proxy_port ~= nil and update.proxy_port or current.proxy_port or '8080'),
		proxy_username = update.proxy_username ~= nil and update.proxy_username or current.proxy_username or '',
		proxy_password = proxy_password,
	}
	if token ~= '' and not M.valid_token(token) then return nil, 'invalid_token' end
	if chat_id ~= '' and not M.valid_chat_id(chat_id) then return nil, 'invalid_chat_id' end
	local proxy, err = M.proxy_config(merged)
	if not proxy then return nil, err end
	return merged
end

function M.safe_status(state, configured, running)
	local counts = { pending = 0, pending_delete = 0, completed = 0 }
	for _, record in pairs((state or {}).records or {}) do
		if counts[record.state] ~= nil then counts[record.state] = counts[record.state] + 1 end
	end
	return {
		ok = true,
		configured = configured == true,
		running = running == true,
		last_success = tonumber((state or {}).last_success) or 0,
		last_error_time = tonumber((state or {}).last_error_time) or 0,
		last_error = (state or {}).last_error,
		pending = counts.pending,
		pending_delete = counts.pending_delete,
		completed = counts.completed,
	}
end

function M.telegram_response(status, parsed)
	status = tonumber(status)
	if status == 429 and type(parsed) == 'table' then
		local retry_after = type(parsed.parameters) == 'table' and tonumber(parsed.parameters.retry_after) or nil
		if retry_after then retry_after = math.max(1, math.min(3600, math.floor(retry_after))) end
		return { ok = false, error = 'telegram_rate_limited', retry_after = retry_after }
	end
	if status == 401 or status == 404 then return { ok = false, error = 'invalid_token' } end
	if status ~= 200 then return { ok = false, error = 'telegram_http_error' } end
	if type(parsed) ~= 'table' then return { ok = false, error = 'telegram_invalid_response' } end
	if parsed.ok ~= true then return { ok = false, error = 'telegram_api_error' } end
	return { ok = true, result = parsed.result }
end

local Engine = {}
Engine.__index = Engine

function M.new_engine(environment, state)
	state = type(state) == 'table' and state or {}
	if state.schema ~= 1 or type(state.records) ~= 'table' then
		state = { schema = 1, records = {} }
	end
	return setmetatable({ env = environment, state = state }, Engine)
end

function Engine:save()
	self.env.save_state(self.state)
end

function Engine:error(code)
	self.state.last_error = code
	self.state.last_error_time = self.env.now()
	self:save()
end

function Engine:success()
	self.state.last_error = nil
	self.state.last_success = self.env.now()
	self:save()
end

local function retry_delay(attempt, requested)
	if tonumber(requested) then return math.max(15, math.min(3600, tonumber(requested))) end
	return math.min(3600, 15 * (2 ^ math.min(8, math.max(0, attempt - 1))))
end

local function overlaps(indexes, messages)
	local wanted = {}
	for _, index in ipairs(indexes) do wanted[index] = true end
	for _, message in ipairs(messages or {}) do
		for _, index in ipairs(M.message_indexes(message) or {}) do
			if wanted[index] then return true end
		end
	end
	return false
end

function Engine:retry_delete(record)
	local deleted = self.env.delete_sms(record.indexes)
	if not deleted or deleted.ok ~= true then
		record.delete_attempts = (tonumber(record.delete_attempts) or 0) + 1
		record.next_delete_attempt = self.env.now() + retry_delay(record.delete_attempts)
		record.updated = self.env.now()
		self:error('sim_delete_failed')
		return false
	end
	local readback = self.env.readback()
	if not readback or readback.ok ~= true or overlaps(record.indexes, readback.messages) then
		record.delete_attempts = (tonumber(record.delete_attempts) or 0) + 1
		record.next_delete_attempt = self.env.now() + retry_delay(record.delete_attempts)
		record.updated = self.env.now()
		self:error('sim_delete_unconfirmed')
		return false
	end
	record.state = 'completed'
	record.next_delete_attempt = nil
	record.updated = self.env.now()
	self:success()
	return true
end

function Engine:reconcile_missing_delete(fingerprint, record)
	local readback = self.env.readback()
	if not readback or readback.ok ~= true or type(readback.messages) ~= 'table' then
		record.delete_attempts = (tonumber(record.delete_attempts) or 0) + 1
		record.next_delete_attempt = self.env.now() + retry_delay(record.delete_attempts)
		record.updated = self.env.now()
		self:error('sim_delete_unconfirmed')
		return false
	end
	for _, message in ipairs(readback.messages) do
		local material = M.fingerprint_material(message)
		if material and self.env.fingerprint(material) == fingerprint then
			return self:retry_delete(record)
		end
	end
	-- The original is no longer present.  This also safely handles a reused slot:
	-- never delete a different message merely because its numeric index overlaps.
	record.state = 'completed'
	record.next_delete_attempt = nil
	record.updated = self.env.now()
	self:success()
	return true
end

function Engine:step(config)
	if not config or not M.valid_token(config.token) or not M.valid_chat_id(config.chat_id) then return false end
	local snapshot = self.env.snapshot()
	if not snapshot or snapshot.ok ~= true or type(snapshot.messages) ~= 'table' then
		self:error('modem_unavailable')
		return false
	end
	local messages, current = {}, {}
	for _, message in ipairs(snapshot.messages) do
		local material = M.fingerprint_material(message)
		local indexes = M.message_indexes(message)
		if material and indexes then
			local fingerprint = self.env.fingerprint(material)
			local record = self.state.records[fingerprint]
			current[fingerprint] = { fingerprint = fingerprint, message = message, record = record }
			if message.unread == true or record then
				if not record and message.unread == true then
					record = { state = 'pending', indexes = indexes, attempts = 0,
						first_seen = self.env.now(), updated = self.env.now(), next_attempt = 0 }
					self.state.records[fingerprint] = record
					current[fingerprint].record = record
					self:save()
				end
				messages[#messages + 1] = { fingerprint = fingerprint, message = message, record = record }
			end
		end
	end

	for fingerprint, record in pairs(self.state.records) do
		if record.state == 'pending_delete' and
		   self.env.now() >= (tonumber(record.next_delete_attempt) or 0) then
			if current[fingerprint] then return self:retry_delete(record) end
			return self:reconcile_missing_delete(fingerprint, record)
		end
	end

	for _, item in ipairs(messages) do
		local record = item.record
		if record.state == 'pending' and self.env.now() >= (tonumber(record.next_attempt) or 0) then
			local result = self.env.send(config.token, config.chat_id, M.compose(item.message),
				M.proxy_config(config))
			if not result or result.ok ~= true then
				record.attempts = (tonumber(record.attempts) or 0) + 1
				record.next_attempt = self.env.now() + retry_delay(record.attempts, result and result.retry_after)
				record.updated = self.env.now()
				self:error(result and result.error or 'telegram_transport_failed')
				return false
			end
			record.telegram_confirmed = self.env.now()
			record.delete_after_send = config.remove_after_send == true
			record.state = record.delete_after_send and 'pending_delete' or 'completed'
			record.delete_attempts = record.delete_after_send and 0 or nil
			record.next_delete_attempt = record.delete_after_send and 0 or nil
			record.updated = self.env.now()
			self:success() -- Persist confirmation before any SIM deletion attempt.
			if record.state == 'pending_delete' then return self:retry_delete(record) end
			return true
		end
	end
	return false
end

local function optional_name(value, maximum)
	if value == nil or value == '' then return nil end
	if type(value) ~= 'string' or #value > maximum or value:find('%c') then return false end
	return value
end

local function merge_identity(candidate, conflicts, key, value)
	if value == nil or conflicts[key] then return end
	if candidate[key] == nil then candidate[key] = value
	elseif candidate[key] ~= value then candidate[key], conflicts[key] = nil, true end
end

function M.private_chat_candidates(updates)
	if type(updates) ~= 'table' or #updates > 100 then return nil, 'telegram_invalid_response' end
	local found, order, conflicts = {}, {}, {}
	for _, update in ipairs(updates) do
		if type(update) ~= 'table' then return nil, 'telegram_invalid_response' end
		local message = update.message
		if message ~= nil then
			if type(message) ~= 'table' or type(message.chat) ~= 'table' or
			   type(message.chat.type) ~= 'string' then
				return nil, 'telegram_invalid_response'
			end
			local chat = message.chat
			if chat.type == 'private' then
				local id = type(chat.id) == 'number' and ('%.0f'):format(chat.id) or chat.id
				local username = optional_name(chat.username, 64)
				local first_name = optional_name(chat.first_name, 128)
				local last_name = optional_name(chat.last_name, 128)
				if not M.valid_chat_id(id) or username == false or first_name == false or last_name == false or
				   (username and not username:match('^[A-Za-z0-9_]+$')) then
					return nil, 'telegram_invalid_response'
				end
				if not found[id] then
					if #order >= 20 then return nil, 'too_many_private_chats' end
					found[id] = { chat_id = id }
					conflicts[id] = {}
					order[#order + 1] = id
				end
				merge_identity(found[id], conflicts[id], 'username', username)
				merge_identity(found[id], conflicts[id], 'first_name', first_name)
				merge_identity(found[id], conflicts[id], 'last_name', last_name)
			end
		end
	end
	if #order == 0 then return nil, 'no_private_chat' end
	local result = {}
	for _, id in ipairs(order) do result[#result + 1] = found[id] end
	return result
end

function M.discovery_result(response)
	if type(response) ~= 'table' then return { ok = false, error = 'telegram_invalid_response' } end
	if response.ok ~= true then
		local allowed = {
			invalid_token = true, telegram_rate_limited = true, telegram_http_error = true,
			telegram_api_error = true, telegram_transport_failed = true,
			telegram_invalid_response = true,
		}
		local error_code = allowed[response.error] and response.error or 'telegram_invalid_response'
		local result = { ok = false, error = error_code }
		if error_code == 'telegram_rate_limited' and tonumber(response.retry_after) then
			result.retry_after = math.max(1, math.min(3600, math.floor(tonumber(response.retry_after))))
		end
		return result
	end
	local candidates, err = M.private_chat_candidates(response.result)
	if not candidates then return { ok = false, error = err } end
	return { ok = true, candidates = candidates }
end

M.copy = copy
return M
