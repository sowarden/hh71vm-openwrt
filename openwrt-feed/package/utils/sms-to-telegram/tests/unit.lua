-- Mocked regression tests. No Telegram or modem connection is opened.

local files = assert(arg[1], 'files directory required')
package.path = files .. '/?.lua;' .. package.path
local core = require 'sms_to_telegram'

local assertions = 0
local function equal(actual, expected, label)
	assertions = assertions + 1
	if actual ~= expected then error((label or 'value') .. ': expected ' .. tostring(expected) .. ', got ' .. tostring(actual), 0) end
end
local function truth(value, label) equal(not not value, true, label) end
local function first_record(state) for _, record in pairs(state.records) do return record end end

local TOKEN = '123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghi'
local CHAT = '123456789'
local CYRILLIC = string.char(0xD0, 0x9F, 0xD1, 0x80, 0xD0, 0xB8, 0xD0, 0xB2, 0xD0, 0xB5, 0xD1, 0x82)
local SMILE = string.char(0xF0, 0x9F, 0x99, 0x82)
local MESSAGE = { index = 7, indexes = { 7 }, sender = '+15550001111',
	ts = '2026-08-30T10:11:12+03:00', text = CYRILLIC .. ', "JSON" \\ newline\n' .. SMILE, unread = true }

local function harness(options)
	options = options or {}
	local clock, sends, deletes, saved = 1000, 0, 0, nil
	local current_messages = options.messages or { core.copy(MESSAGE) }
	local send_result = options.send_result or { ok = true }
	local delete_result = options.delete_result or { ok = true }
	local readback = options.readback or { ok = true, messages = {} }
	local env = {
		now = function() return clock end,
		fingerprint = function(material) return material end,
		save_state = function(state) saved = core.copy(state) end,
		snapshot = function() return { ok = true, messages = current_messages } end,
		send = function(token, chat, text)
			sends = sends + 1
			equal(token, TOKEN, 'mock token')
			equal(chat, CHAT, 'mock chat')
			truth(text:find(MESSAGE.text, 1, true), 'Unicode body preserved')
			return send_result
		end,
		delete_sms = function(indexes)
			deletes = deletes + 1
			equal(table.concat(indexes, ','), options.expected_indexes or '7', 'delete indexes')
			return delete_result
		end,
		readback = function() return readback end,
	}
	return {
		env = env,
		engine = core.new_engine(env, options.state),
		config = { token = TOKEN, chat_id = CHAT, remove_after_send = options.remove == true },
		sends = function() return sends end,
		deletes = function() return deletes end,
		saved = function() return saved end,
		set_delete = function(value) delete_result = value end,
		set_readback = function(value) readback = value end,
		advance = function(seconds) clock = clock + seconds end,
	}
end

-- 1-3: one delivery, no repeat, and persistent deduplication after restart.
do
	local h = harness()
	truth(h.engine:step(h.config), 'first SMS delivered')
	equal(h.sends(), 1, 'first send count')
	equal(first_record(h.engine.state).state, 'completed', 'completed without delete')
	equal(h.engine:step(h.config), false, 'repeat poll idle')
	equal(h.sends(), 1, 'repeat poll does not resend')
	local restarted = harness({ state = h.saved() })
	equal(restarted.engine:step(restarted.config), false, 'restart poll idle')
	equal(restarted.sends(), 0, 'restart does not resend')
end

-- 4: Telegram HTTP/API failure preserves the SIM message and remains pending.
do
	local h = harness({ remove = true, send_result = { ok = false, error = 'telegram_api_error' } })
	equal(h.engine:step(h.config), false, 'API failure')
	equal(h.deletes(), 0, 'API failure never deletes')
	equal(first_record(h.engine.state).state, 'pending', 'API failure stays pending')
	equal(h.engine.state.last_error, 'telegram_api_error', 'safe API error')
end

-- 5: confirmed delivery with removal disabled leaves the SIM message in place.
do
	local h = harness({ remove = false })
	truth(h.engine:step(h.config), 'confirmed without removal')
	equal(h.deletes(), 0, 'removal disabled')
end

-- 6: confirmed delivery with removal enabled deletes every composite slot.
do
	local composite = core.copy(MESSAGE)
	composite.index, composite.indexes = 3, { 9, 3, 4 }
	local h = harness({ remove = true, messages = { composite }, expected_indexes = '3,4,9' })
	truth(h.engine:step(h.config), 'confirmed and deleted')
	equal(h.sends(), 1, 'composite sent once')
	equal(h.deletes(), 1, 'composite delete called once')
	equal(first_record(h.engine.state).state, 'completed', 'delete readback completed')
end

-- 7-8: failed deletion becomes pending-delete; retry deletes only and completes.
do
	local h = harness({ remove = true, delete_result = { ok = false } })
	equal(h.engine:step(h.config), false, 'delete failure')
	equal(h.sends(), 1, 'sent before delete failure')
	equal(first_record(h.engine.state).state, 'pending_delete', 'pending delete persisted')
	h.set_delete({ ok = true })
	h.set_readback({ ok = true, messages = {} })
	h.advance(15)
	truth(h.engine:step(h.config), 'delete retry success')
	equal(h.sends(), 1, 'delete retry never resends')
	equal(h.deletes(), 2, 'delete retried')
	equal(first_record(h.engine.state).state, 'completed', 'delete retry completed')
end

-- Pending delivery survives a daemon/reboot refresh that reports the SIM message as read.
do
	local h = harness({ send_result = { ok = false, error = 'telegram_transport_failed' } })
	equal(h.engine:step(h.config), false, 'initial transport failure')
	local read_message = core.copy(MESSAGE)
	read_message.unread = false
	local restarted = harness({ state = h.saved(), messages = { read_message } })
	restarted.advance(15)
	truth(restarted.engine:step(restarted.config), 'pending read message retried after restart')
	equal(restarted.sends(), 1, 'restart retries Telegram once')
end

-- If confirmed deletion already removed the original, recovery completes by readback
-- and does not delete a different message that reused the same SIM slot.
do
	local state = { schema = 1, records = {} }
	local fingerprint = core.fingerprint_material(MESSAGE)
	state.records[fingerprint] = { state = 'pending_delete', indexes = { 7 }, next_delete_attempt = 0 }
	local replacement = core.copy(MESSAGE)
	replacement.sender, replacement.text, replacement.unread = '+15550002222', 'replacement', true
	local h = harness({ state = state, messages = { replacement }, readback = { ok = true, messages = { replacement } } })
	truth(h.engine:step(h.config), 'missing original reconciled')
	equal(h.deletes(), 0, 'reused slot is never deleted')
	equal(state.records[fingerprint].state, 'completed', 'missing original completed')
end

-- 9: HTTP 200 is insufficient, while ok:true and 429 retry_after are recognized.
do
	equal(core.telegram_response(200, { ok = false }).error, 'telegram_api_error', 'HTTP 200 API failure')
	truth(core.telegram_response(200, { ok = true }).ok, 'HTTP 200 ok true')
	local limited = core.telegram_response(429, { ok = false, parameters = { retry_after = 37 } })
	equal(limited.error, 'telegram_rate_limited', '429 class')
	equal(limited.retry_after, 37, '429 retry_after')
	equal(core.telegram_response(429, { parameters = { retry_after = 99999 } }).retry_after, 3600,
		'429 retry_after bounded')
	equal(core.telegram_response(401, {}).error, 'invalid_token', '401 is invalid token')
	equal(core.telegram_response(404, {}).error, 'invalid_token', '404 is invalid token')
end

-- 10: blank token updates preserve the stored secret; status contains no secret fields.
do
	local merged = assert(core.merge_config({ token = TOKEN, chat_id = CHAT, remove_after_send = false },
		{ token = '', chat_id = CHAT, remove_after_send = true }))
	equal(merged.token, TOKEN, 'blank token preserves secret')
	truth(merged.remove_after_send, 'checkbox saved')
	local status = core.safe_status({ records = {}, last_error = 'telegram_api_error' }, true, true)
	equal(status.token, nil, 'status has no token')
	equal(status.chat_id, nil, 'status has no chat ID')
	equal(status.text, nil, 'status has no SMS text')
end

-- 11: malformed credentials are rejected; @username is not a private chat_id.
do
	equal(core.valid_token('bad-token'), false, 'bad token')
	equal(core.valid_chat_id('@example'), false, 'username rejected')
	equal(core.valid_chat_id('-100123456789'), false, 'group chat rejected')
	truth(core.valid_token(TOKEN), 'synthetic token accepted')
	truth(core.valid_chat_id(CHAT), 'numeric private chat accepted')
end

-- 12: Cyrillic/Unicode survives composition and composite identity includes all slots.
do
	local text = core.compose(MESSAGE)
	truth(text:find(CYRILLIC, 1, true), 'Cyrillic composed')
	truth(text:find(SMILE, 1, true), 'Unicode composed')
	local composite = core.copy(MESSAGE)
	composite.indexes = { 4, 2, 3 }
	truth(core.fingerprint_material(composite):find('2,3,4', 1, true), 'composite slots normalized')
end

-- Safe getUpdates discovery returns normalized private candidates.
do
	local candidates = assert(core.private_chat_candidates({
		{ message = { text = 'must not escape', chat = { id = 123456789, type = 'private',
			username = 'example_user', first_name = CYRILLIC } } },
	}))
	equal(#candidates, 1, 'single private chat')
	equal(candidates[1].chat_id, CHAT, 'single numeric chat ID')
	equal(candidates[1].username, 'example_user', 'username retained')
	equal(candidates[1].first_name, CYRILLIC, 'Unicode name retained')
	equal(candidates[1].text, nil, 'message text omitted')

	candidates = assert(core.private_chat_candidates({
		{ message = { chat = { id = 123456789, type = 'private', first_name = 'Alice' } } },
		{ message = { chat = { id = 987654321, type = 'private', first_name = 'Bob' } } },
		{ message = { chat = { id = 123456789, type = 'private', first_name = 'Alice' } } },
		{ message = { chat = { id = -100123456789, type = 'supergroup', title = 'ignored' } } },
		{ message = { chat = { id = -100987654321, type = 'channel', title = 'ignored' } } },
	}))
	equal(#candidates, 2, 'multiple private chats returned')
	equal(candidates[1].chat_id, CHAT, 'duplicate private chat merged')
	equal(candidates[2].chat_id, '987654321', 'second private chat retained')
	equal(candidates[2].username, nil, 'username is optional')

	local value, err = core.private_chat_candidates({
		{ message = { chat = { id = -100123456789, type = 'group' } } },
	})
	equal(value, nil, 'group-only result omitted')
	equal(err, 'no_private_chat', 'empty private result classified')

	value, err = core.private_chat_candidates({
		{ message = { chat = { id = '@invalid', type = 'private' } } },
	})
	equal(value, nil, 'malformed private chat refused')
	equal(err, 'telegram_invalid_response', 'malformed result classified')

	candidates = assert(core.private_chat_candidates({
		{ message = { chat = { id = 123456789, type = 'private', username = 'old_name' } } },
		{ message = { chat = { id = 123456789, type = 'private', username = 'new_name' } } },
	}))
	equal(candidates[1].username, nil, 'conflicting duplicate metadata omitted')

	local normalized = core.discovery_result({ ok = true, result = {
		{ message = { text = 'private body', chat = { id = 123456789, type = 'private' } } },
	} })
	truth(normalized.ok, 'mock discovery response accepted')
	equal(normalized.candidates[1].chat_id, CHAT, 'mock discovery candidate normalized')
	equal(normalized.candidates[1].text, nil, 'mock discovery body omitted')
	normalized = core.discovery_result({ ok = false, error = 'telegram_transport_failed',
		description = 'raw upstream detail' })
	equal(normalized.error, 'telegram_transport_failed', 'mock timeout classified')
	equal(normalized.description, nil, 'mock upstream error detail omitted')
	normalized = core.discovery_result({ ok = false, error = 'telegram_rate_limited', retry_after = 99999 })
	equal(normalized.retry_after, 3600, 'mock discovery rate limit bounded')
end

print(('sms-to-telegram tests: PASS (%d assertions)'):format(assertions))
