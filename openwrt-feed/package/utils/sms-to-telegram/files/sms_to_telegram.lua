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

--- Which message store a message lives in, when the daemon says.  Slot numbers restart
--- in every store -- ME held 0-7 while SM held 0-9 on the same modem -- so a delete
--- that carries only an index can land on the wrong message entirely.
function M.message_storage(message)
	local storage = message and message.storage
	if type(storage) ~= 'string' then return nil end
	storage = storage:upper()
	return storage:match('^%u%u$') and storage or nil
end

--- Segments still missing from a long message, as the daemon reports them.
function M.missing_parts(message)
	local missing = tonumber(message and message.missing)
	return (missing and missing > 0) and math.floor(missing) or 0
end

--- Telegram's own markup dialects.  'none' sends the text with no parse_mode at all,
--- which is what this package did before templates existed.
M.PARSE_MODES = { none = true, HTML = true, MarkdownV2 = true }
M.DEFAULT_PARSE_MODE = 'HTML'

--- Every name the template may use.  An unknown %name% is left exactly as typed, so a
--- literal percent sign in a template is never mangled and a typo is visible.
M.PLACEHOLDERS = { 'sender', 'receiver', 'sms_text', 'receive_time', 'router_time',
	'parts', 'incomplete', 'storage', 'hostname' }

M.DEFAULT_TEMPLATE = table.concat({
	'<b>SMS from %sender%</b>',
	'%incomplete%',
	'%receive_time%',
	'',
	'%sms_text%',
}, '\n')

M.TEMPLATE_MAX = 2000

function M.valid_parse_mode(value)
	return type(value) == 'string' and M.PARSE_MODES[value] == true
end

function M.valid_template(value)
	if type(value) ~= 'string' or #value < 1 or #value > M.TEMPLATE_MAX then return false end
	if value:find('[^\n\t\32-\255]') then return false end
	return value:find('%S') ~= nil
end

--- A value dropped into a template is somebody else's text -- an SMS body, a sender ID
--- the network chose -- so it is escaped for the markup dialect in use.  The template
--- itself is not: its markup is what the user typed on purpose.
function M.escape_value(value, parse_mode)
	value = tostring(value or '')
	if parse_mode == 'HTML' then
		value = value:gsub('&', '&amp;'):gsub('<', '&lt;'):gsub('>', '&gt;')
	elseif parse_mode == 'MarkdownV2' then
		value = value:gsub('[_*%[%]()~`>#+%-=|{}.!\\]', '\\%0')
	end
	return value
end

--- The modem reports 'YY/MM/DD,HH:MM:SS+ZZ'; show it the way the Messages page does.
function M.format_ts(ts)
	if type(ts) ~= 'string' or ts == '' then return '' end
	local y, mo, d, h, mi, sec = ts:match('^(%d+)/(%d+)/(%d+),(%d+):(%d+):(%d+)')
	if not y then return ts end
	return ('20%s-%s-%s %s:%s:%s'):format(y, mo, d, h, mi, sec)
end

function M.incomplete_note(message)
	local missing = M.missing_parts(message)
	if missing <= 0 then return '' end
	local parts = tonumber(message.parts) or 0
	return parts > 0
		and ('[incomplete: %d of %d parts arrived]'):format(parts - missing, parts)
		or ('[incomplete: %d part(s) missing]'):format(missing)
end

--- `extra` carries the things that are not in the message itself: the router's own
--- number, its hostname and its clock.  They are looked up only when the template asks
--- for them, because the first two cost a call to the modem daemon.
function M.placeholder_values(message, extra)
	message, extra = message or {}, extra or {}
	return {
		sender = type(message.sender) == 'string' and message.sender or 'unknown',
		receiver = extra.receiver and extra.receiver ~= '' and extra.receiver or 'unknown',
		sms_text = type(message.text) == 'string' and message.text or '',
		receive_time = M.format_ts(message.ts),
		router_time = extra.router_time or '',
		parts = tostring(tonumber(message.parts) or 1),
		incomplete = M.incomplete_note(message),
		storage = M.message_storage(message) or '',
		hostname = extra.hostname or '',
	}
end

--- A line that held nothing but placeholders and came out empty is dropped, so
--- `%incomplete%` on its own line leaves no blank gap for an ordinary message.
function M.render(template, values, parse_mode)
	local out = {}
	for line in (template .. '\n'):gmatch('([^\n]*)\n') do
		local had = line:find('%%[a-z_]+%%') ~= nil
		local only = had and line:gsub('%%[a-z_]+%%', ''):find('%S') == nil
		-- gsub with a function uses the returned string verbatim, and keeps the match
		-- when the function returns nil; an unknown name therefore stays as typed.
		local rendered = line:gsub('%%([a-z_]+)%%', function(name)
			if values[name] == nil then return nil end
			return M.escape_value(values[name], parse_mode)
		end)
		if not (only and rendered:find('%S') == nil) then out[#out + 1] = rendered end
	end
	return (table.concat(out, '\n'):gsub('%s+$', ''))
end

function M.template_of(config)
	config = config or {}
	return M.valid_template(config.template) and config.template or M.DEFAULT_TEMPLATE
end

function M.parse_mode_of(config)
	config = config or {}
	return M.valid_parse_mode(config.parse_mode) and config.parse_mode or M.DEFAULT_PARSE_MODE
end

--- Which of the costly extras this template actually needs.
function M.template_needs(config)
	local template = M.template_of(config)
	return {
		receiver = template:find('%%receiver%%') ~= nil,
		hostname = template:find('%%hostname%%') ~= nil,
		router_time = template:find('%%router_time%%') ~= nil,
	}
end

function M.compose(message, config, extra)
	local mode = M.parse_mode_of(config)
	local values = M.placeholder_values(message, extra)
	local text = M.render(M.template_of(config), values, mode)
	-- Telegram refuses an empty message, and the send would then be retried for ever.  A
	-- template that renders to nothing falls back to the message itself, and a message
	-- with no text at all -- which does happen -- falls back to saying so.
	if text == '' then text = values.sms_text end
	if text == '' then text = M.escape_value('(empty message)', mode) end
	return text
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
	local template = update.template ~= nil and update.template or current.template
	if template == nil or template == '' then template = M.DEFAULT_TEMPLATE end
	local parse_mode = update.parse_mode ~= nil and update.parse_mode or current.parse_mode
	if parse_mode == nil or parse_mode == '' then parse_mode = M.DEFAULT_PARSE_MODE end
	local merged = {
		token = token,
		chat_id = chat_id,
		template = template,
		parse_mode = parse_mode,
		remove_after_send = remove == true,
		proxy_type = update.proxy_type ~= nil and update.proxy_type or current.proxy_type or 'none',
		proxy_host = update.proxy_host ~= nil and update.proxy_host or current.proxy_host or '',
		proxy_port = tostring(update.proxy_port ~= nil and update.proxy_port or current.proxy_port or '8080'),
		proxy_username = update.proxy_username ~= nil and update.proxy_username or current.proxy_username or '',
		proxy_password = proxy_password,
	}
	if token ~= '' and not M.valid_token(token) then return nil, 'invalid_token' end
	if chat_id ~= '' and not M.valid_chat_id(chat_id) then return nil, 'invalid_chat_id' end
	if not M.valid_template(merged.template) then return nil, 'invalid_template' end
	if not M.valid_parse_mode(merged.parse_mode) then return nil, 'invalid_parse_mode' end
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
		template_rejected = tonumber((state or {}).template_rejected) or 0,
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
	-- No status at all means the helper never got an HTTP answer: it prints nothing and
	-- exits non-zero when curl fails, and Lua 5.1's pipe:close() reports success whatever
	-- the child's exit code, so the empty output arrives here rather than being caught as
	-- a transport error.  Calling that an HTTP error points the reader at the bot token,
	-- when the cause is a connection that was never made -- usually no route to Telegram,
	-- or a router clock still years behind, which makes the certificate invalid.
	if not status then return { ok = false, error = 'telegram_transport_failed' } end
	-- The status comes back too: a 400 with a parse_mode set means Telegram could not
	-- parse the template's markup, which is worth handling differently from a 500.
	if status ~= 200 then return { ok = false, error = 'telegram_http_error', http_status = status } end
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

--- The state lives on the flash overlay, so it is written when something in it changes
--- and not on every poll.  The same error repeating (the modem away for an hour, say)
--- only moves last_error_time, which is kept in memory until the next real change.
function Engine:error(code, changed)
	local repeated = self.state.last_error == code
	self.state.last_error = code
	if not repeated then self.state.last_error_time = self.env.now() end
	if changed or not repeated then self:save() end
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

--- Is any of `indexes` still present in `storage`?  A message at the same slot number
--- in a *different* store is a different message and must not be read as proof that
--- the delete failed -- that would retry the delete for ever against the wrong store.
local function overlaps(indexes, storage, messages)
	local wanted = {}
	for _, index in ipairs(indexes) do wanted[index] = true end
	for _, message in ipairs(messages or {}) do
		local where = M.message_storage(message)
		if not storage or not where or where == storage then
			for _, index in ipairs(M.message_indexes(message) or {}) do
				if wanted[index] then return true end
			end
		end
	end
	return false
end

function Engine:retry_delete(record)
	local deleted = self.env.delete_sms(record.indexes, record.storage)
	if not deleted or deleted.ok ~= true then
		record.delete_attempts = (tonumber(record.delete_attempts) or 0) + 1
		record.next_delete_attempt = self.env.now() + retry_delay(record.delete_attempts)
		record.updated = self.env.now()
		self:error('sim_delete_failed', true)
		return false
	end
	local readback = self.env.readback()
	if not readback or readback.ok ~= true
	   or overlaps(record.indexes, record.storage, readback.messages) then
		record.delete_attempts = (tonumber(record.delete_attempts) or 0) + 1
		record.next_delete_attempt = self.env.now() + retry_delay(record.delete_attempts)
		record.updated = self.env.now()
		self:error('sim_delete_unconfirmed', true)
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
		self:error('sim_delete_unconfirmed', true)
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

--- A long message whose segments are still arriving is held back this long before it
--- is forwarded as it is, marked incomplete.  Forwarding it at once sent the first part
--- alone and then the whole message again once the rest arrived; with deletion on, the
--- first slots were gone by then and the late parts went out as a fragment.
M.INCOMPLETE_GRACE = 600
--- A record whose message has left the modem is kept this long, then dropped, so the
--- state file does not grow with every message ever received.
M.FORGET_COMPLETED = 30 * 86400
M.FORGET_PENDING = 86400

--- Records whose message is no longer in the snapshot.  Returns true when anything was
--- changed, so the caller saves once.  pending_delete records are never dropped here:
--- reconcile_missing_delete owns them.
function Engine:age_out(current)
	local now, changed = self.env.now(), false
	for fingerprint, record in pairs(self.state.records) do
		if current[fingerprint] then
			if record.gone_since then record.gone_since = nil; changed = true end
		elseif record.state ~= 'pending_delete' then
			if not record.gone_since then
				record.gone_since = now
				changed = true
			else
				local keep = record.state == 'completed' and M.FORGET_COMPLETED or M.FORGET_PENDING
				if now - (tonumber(record.gone_since) or now) >= keep then
					self.state.records[fingerprint] = nil
					changed = true
				end
			end
		end
	end
	return changed
end

--- The parts of a message that are not in the message: looked up once per poll, and
--- only when the template uses them.
function Engine:extras(config)
	local needs, extra = M.template_needs(config), {}
	if needs.receiver and self.env.receiver then extra.receiver = self.env.receiver() end
	if needs.hostname and self.env.hostname then extra.hostname = self.env.hostname() end
	if needs.router_time and self.env.local_time then extra.router_time = self.env.local_time() end
	return extra
end

--- Send one message.  Telegram rejects the whole request with HTTP 400 when the
--- template's markup does not parse, and retrying it would stop every SMS getting
--- through until the template is fixed.  Deliver it once as plain text instead and
--- record that it happened, so the page can say why the formatting is missing.
function Engine:deliver(config, message)
	local extra = self:extras(config)
	local mode = M.parse_mode_of(config)
	local proxy = M.proxy_config(config)
	local result = self.env.send(config.token, config.chat_id,
		M.compose(message, config, extra), proxy, mode)
	if result and result.ok ~= true and mode ~= 'none' and tonumber(result.http_status) == 400 then
		local plain = M.compose(message, { template = config.template, parse_mode = 'none' }, extra)
		local retry = self.env.send(config.token, config.chat_id, plain, proxy, 'none')
		if retry and retry.ok == true then
			self.state.template_rejected = self.env.now()
			return retry
		end
	end
	if result and result.ok == true and self.state.template_rejected then
		self.state.template_rejected = nil
	end
	return result
end

function Engine:step(config)
	if not config or not M.valid_token(config.token) or not M.valid_chat_id(config.chat_id) then return false end
	local snapshot = self.env.snapshot()
	if not snapshot or snapshot.ok ~= true or type(snapshot.messages) ~= 'table' then
		self:error('modem_unavailable')
		return false
	end
	local messages, current, dirty = {}, {}, false
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
						storage = M.message_storage(message),
						first_seen = self.env.now(), updated = self.env.now(), next_attempt = 0 }
					self.state.records[fingerprint] = record
					current[fingerprint].record = record
					dirty = true
				end
				-- A record written before stores were tracked has no storage; adopt the
				-- one the daemon now reports so its delete cannot go to the wrong store.
				if record and not record.storage then
					record.storage = M.message_storage(message)
				end
				messages[#messages + 1] = { fingerprint = fingerprint, message = message, record = record }
			end
		end
	end
	if self:age_out(current) then dirty = true end
	if dirty then self:save() end

	for fingerprint, record in pairs(self.state.records) do
		if record.state == 'pending_delete' and
		   self.env.now() >= (tonumber(record.next_delete_attempt) or 0) then
			if current[fingerprint] then return self:retry_delete(record) end
			return self:reconcile_missing_delete(fingerprint, record)
		end
	end

	for _, item in ipairs(messages) do
		local record = item.record
		local held = M.missing_parts(item.message) > 0 and
			self.env.now() < (tonumber(record.first_seen) or 0) + M.INCOMPLETE_GRACE
		if record.state == 'pending' and not held and
		   self.env.now() >= (tonumber(record.next_attempt) or 0) then
			local result = self:deliver(config, item.message)
			if not result or result.ok ~= true then
				record.attempts = (tonumber(record.attempts) or 0) + 1
				record.next_attempt = self.env.now() + retry_delay(record.attempts, result and result.retry_after)
				record.updated = self.env.now()
				self:error(result and result.error or 'telegram_transport_failed', true)
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
