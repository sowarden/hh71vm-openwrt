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
	if token ~= '' and not M.valid_token(token) then return nil, 'invalid_token' end
	if chat_id ~= '' and not M.valid_chat_id(chat_id) then return nil, 'invalid_chat_id' end
	return { token = token, chat_id = chat_id, remove_after_send = remove == true }
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
		return { ok = false, error = 'telegram_rate_limited',
			retry_after = type(parsed.parameters) == 'table' and tonumber(parsed.parameters.retry_after) or nil }
	end
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
			local result = self.env.send(config.token, config.chat_id, M.compose(item.message))
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

function M.unique_private_chat(updates)
	local found = {}
	for _, update in ipairs(updates or {}) do
		local message = update.message
		local chat = type(message) == 'table' and message.chat or nil
		if type(chat) == 'table' and chat.type == 'private' and chat.id ~= nil then
			local id = type(chat.id) == 'number' and ('%.0f'):format(chat.id) or tostring(chat.id)
			if M.valid_chat_id(id) then found[id] = true end
		end
	end
	local only, count
	count = 0
	for id in pairs(found) do only, count = id, count + 1 end
	if count == 1 then return only end
	return nil, count == 0 and 'no_private_chat' or 'multiple_private_chats'
end

M.copy = copy
return M
