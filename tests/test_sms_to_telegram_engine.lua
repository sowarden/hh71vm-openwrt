-- Functional cover for the forwarder engine's delete path.
--
-- The rest of the forwarder's checks are static text contracts.  This one runs the
-- engine against a fake modem, because the thing being protected is behavioural: the
-- modem keeps two message stores whose slot numbers both start at zero, so a delete
-- that carries only an index can remove a different message from the other store.

local core_path = assert(arg[1], "sms_to_telegram.lua path required")

local chunk = assert(loadfile(core_path))
local core = chunk()

local assertions = 0
local function equal(actual, expected, label)
	assertions = assertions + 1
	if actual ~= expected then
		error(("%s: expected %s, got %s"):format(label, tostring(expected), tostring(actual)), 2)
	end
end
local function truthy(value, label)
	assertions = assertions + 1
	if not value then error(label, 2) end
end

local CONFIG = {
	token = "1234567890:" .. ("b"):rep(35),
	chat_id = "123456",
	remove_after_send = true,
}
truthy(core.valid_token(CONFIG.token), "fixture token is accepted")
truthy(core.valid_chat_id(CONFIG.chat_id), "fixture chat id is accepted")

--- A modem holding the same slot number in both stores, which is the shape measured on
--- the stand: ME indexes 0-7 and SM indexes 0-9 on one modem.
local function harness(messages)
	local log = { deletes = {}, sent = 0 }
	local store = {}
	for _, m in ipairs(messages) do store[#store + 1] = m end
	local env = {
		now = function() return 1000 end,
		save_state = function() end,
		fingerprint = function(material)
			-- deterministic and collision-free enough for a fixture
			local sum = 0
			for i = 1, #material do sum = (sum * 131 + material:byte(i)) % 4294967296 end
			return ("%08x"):format(sum) .. ("0"):rep(56)
		end,
		snapshot = function() return { ok = true, messages = store } end,
		readback = function() return { ok = true, messages = store } end,
		delete_sms = function(indexes, storage)
			log.deletes[#log.deletes + 1] = { indexes = indexes, storage = storage }
			local kept = {}
			for _, m in ipairs(store) do
				local drop = false
				-- A real modem deletes by slot *within the selected store*.
				if not storage or m.storage == storage then
					for _, index in ipairs(indexes) do
						if m.index == index then drop = true end
					end
				end
				if not drop then kept[#kept + 1] = m end
			end
			store = kept
			return { ok = true }
		end,
		send = function() log.sent = log.sent + 1; return { ok = true } end,
	}
	return core.new_engine(env, nil), log, function() return store end
end

do
	-- Two different messages sitting at slot 0 in each store.  Forwarding and deleting
	-- one must not touch the other.
	local engine, log, current = harness({
		{ index = 0, indexes = { 0 }, storage = "ME", sender = "A", text = "in modem",
		  ts = "26/09/07,01:00:00+00", unread = true },
		{ index = 0, indexes = { 0 }, storage = "SM", sender = "B", text = "on sim",
		  ts = "26/09/07,01:00:01+00", unread = false },
	})

	engine:step(CONFIG)
	equal(log.sent, 1, "only the unread message is forwarded")
	equal(#log.deletes, 1, "one delete issued")
	equal(log.deletes[1].storage, "ME", "the delete names the store the message came from")

	local left = current()
	equal(#left, 1, "one message remains")
	equal(left[1].storage, "SM", "the colliding slot in the other store survived")
	equal(left[1].text, "on sim", "and it is the message that was never forwarded")

	local record
	for _, value in pairs(engine.state.records) do record = value end
	equal(record.state, "completed", "delete confirmed")
	equal(record.storage, "ME", "the record remembers the store")
	equal(engine.state.last_error, nil, "no error reported")
end

do
	-- A same-numbered slot left behind in the *other* store must not be mistaken for
	-- a failed delete, or the forwarder retries for ever against the wrong store.
	equal(core.message_storage({ storage = "sm" }), "SM", "storage name normalised")
	equal(core.message_storage({ storage = "" }), nil, "empty storage is not a store")
	equal(core.message_storage({}), nil, "absent storage is not a store")
end

do
	-- A snapshot from a daemon that does not report stores at all still works: the
	-- delete simply goes out unqualified, exactly as it did before.
	local engine, log, current = harness({
		{ index = 3, indexes = { 3 }, sender = "A", text = "no store reported",
		  ts = "26/09/07,01:00:02+00", unread = true },
	})
	engine:step(CONFIG)
	equal(log.sent, 1, "message forwarded without a store")
	equal(log.deletes[1].storage, nil, "no store is claimed when none is known")
	equal(#current(), 0, "message deleted")
	equal(engine.state.last_error, nil, "unqualified delete is not an error")
end

--- A clock the test moves, a save counter, and a snapshot it can swap.
local function timed(messages, config_ok)
	local t = { now = 1000, saves = 0, sent = {}, modes = {}, answers = {},
		store = messages, modem_ok = true }
	local env = {
		now = function() return t.now end,
		save_state = function() t.saves = t.saves + 1 end,
		fingerprint = function(material)
			local sum = 0
			for i = 1, #material do sum = (sum * 131 + material:byte(i)) % 4294967296 end
			return ("%08x"):format(sum) .. ("0"):rep(56)
		end,
		snapshot = function()
			if not t.modem_ok then return { ok = false } end
			return { ok = true, messages = t.store }
		end,
		readback = function() return { ok = true, messages = t.store } end,
		delete_sms = function() return { ok = true } end,
		receiver = function() t.receiver_reads = (t.receiver_reads or 0) + 1; return "+380990000000" end,
		hostname = function() return "hh71vm" end,
		local_time = function() return "2026-09-29 02:00:00" end,
		send = function(_, _, text, _, parse_mode)
			t.sent[#t.sent + 1] = text
			t.modes[#t.modes + 1] = parse_mode
			local answer = t.answers[#t.sent]
			return answer or { ok = true }
		end,
	}
	return core.new_engine(env, nil), t
end
local KEEP = { token = CONFIG.token, chat_id = CONFIG.chat_id, remove_after_send = false }

do
	-- A long message whose later parts are still on their way is held, then sent
	-- once, whole, when they arrive.
	local engine, t = timed({
		{ index = 1, indexes = { 1 }, storage = "SM", sender = "A", text = "part one ",
		  ts = "26/09/07,01:00:00+00", unread = true, parts = 3, missing = 2 },
	})
	engine:step(KEEP)
	equal(#t.sent, 0, "an incomplete message is not forwarded at once")
	t.now = t.now + 30
	t.store = { { index = 1, indexes = { 1, 2, 3 }, storage = "SM", sender = "A",
	              text = "part one part two part three", ts = "26/09/07,01:00:00+00",
	              unread = true, parts = 3 } }
	engine:step(KEEP)
	equal(#t.sent, 1, "the completed message is forwarded")
	truthy(t.sent[1]:find("part three", 1, true), "it is forwarded whole")
	truthy(not t.sent[1]:find("incomplete", 1, true), "and is not marked incomplete")
	engine:step(KEEP)
	equal(#t.sent, 1, "and only once")
end

do
	-- A part that never arrives: after the grace period the message goes out marked.
	local engine, t = timed({
		{ index = 4, indexes = { 4 }, storage = "SM", sender = "B", text = "only half",
		  ts = "26/09/07,01:00:05+00", unread = true, parts = 2, missing = 1 },
	})
	engine:step(KEEP)
	equal(#t.sent, 0, "held while the grace period runs")
	t.now = t.now + core.INCOMPLETE_GRACE + 1
	engine:step(KEEP)
	equal(#t.sent, 1, "forwarded once the grace period is over")
	truthy(t.sent[1]:find("[incomplete: 1 of 2 parts arrived]", 1, true), "marked incomplete")
end

do
	-- The same error on every poll must not rewrite the flash every poll.
	local engine, t = timed({})
	t.modem_ok = false
	for _ = 1, 20 do engine:step(KEEP); t.now = t.now + 15 end
	equal(t.saves, 1, "a repeating error is saved once")
	equal(engine.state.last_error, "modem_unavailable", "the error is still reported")
	t.modem_ok = true
	engine:step(KEEP)
	equal(t.saves, 1, "an idle poll with nothing to do writes nothing")
end

do
	-- A forwarded message that has left the modem is forgotten after a while.
	local engine, t = timed({
		{ index = 7, indexes = { 7 }, storage = "SM", sender = "C", text = "old",
		  ts = "26/09/07,01:00:09+00", unread = true },
	})
	engine:step(KEEP)
	equal(#t.sent, 1, "forwarded")
	t.store = {}
	engine:step(KEEP)
	local count = 0
	for _ in pairs(engine.state.records) do count = count + 1 end
	equal(count, 1, "kept while recently seen")
	t.now = t.now + core.FORGET_COMPLETED + 1
	engine:step(KEEP)
	count = 0
	for _ in pairs(engine.state.records) do count = count + 1 end
	equal(count, 0, "dropped after the retention period")
end

do
	-- The message template.  What the user types is markup on purpose; what the
	-- network supplies is text and must never be able to act as markup.
	local MSG = { index = 1, indexes = { 1 }, storage = "ME", sender = "+380501112233",
		text = "balance is <low> & falling", ts = "26/09/29,01:23:45+08", unread = true }

	equal(core.compose(MSG, nil),
		"<b>SMS from +380501112233</b>\n2026-09-29 01:23:45\n\nbalance is &lt;low&gt; &amp; falling",
		"the default template escapes the message for HTML and drops the empty line")

	equal(core.compose({ sender = "A", text = "b", parts = 3, missing = 2 },
		{ parse_mode = "none", template = "%incomplete%|%parts%|%sms_text%" }),
		"[incomplete: 1 of 3 parts arrived]|3|b", "an incomplete long message says so")

	equal(core.compose({ sender = "x*y", text = "a.b" },
		{ parse_mode = "MarkdownV2", template = "*%sender%* %sms_text%" }),
		"*x\\*y* a\\.b", "MarkdownV2 escapes the values but not the template")

	equal(core.compose({ sender = "A", text = "t" },
		{ parse_mode = "none", template = "%sendr% %sender% 100% off" }),
		"%sendr% A 100% off", "an unknown name and a bare percent are left as typed")

	equal(core.compose({ sender = "A", text = "fallback" },
		{ parse_mode = "none", template = "%incomplete%" }),
		"fallback", "a template that renders to nothing falls back to the message")

	-- An SMS with no text at all would otherwise render empty, and Telegram refuses an
	-- empty message, so the send would be retried for ever.
	equal(core.compose({ sender = "A", text = "" },
		{ parse_mode = "none", template = "%sms_text%" }),
		"(empty message)", "an empty message still produces something to send")
	equal(core.compose({ sender = "A", text = "" },
		{ parse_mode = "MarkdownV2", template = "%sms_text%" }),
		"\\(empty message\\)", "and it is escaped for the mode in use")

	truthy(not core.valid_template(""), "an empty template is refused")
	truthy(not core.valid_template("   "), "a whitespace-only template is refused")
	truthy(not core.valid_template("a\1b"), "a control character is refused")
	truthy(not core.valid_template(("a"):rep(core.TEMPLATE_MAX + 1)), "an oversized template is refused")
	truthy(core.valid_template("ok\nstill ok"), "newlines are allowed")
	truthy(core.valid_parse_mode("HTML") and core.valid_parse_mode("MarkdownV2") and
		core.valid_parse_mode("none"), "the three modes are accepted")
	truthy(not core.valid_parse_mode("Markdown"), "legacy Markdown is not offered")

	-- The costly lookups happen only when the template names them.
	local engine, t = timed({ core.copy(MSG) })
	engine:step({ token = CONFIG.token, chat_id = CONFIG.chat_id,
		template = "%sender%: %sms_text%", parse_mode = "none" })
	equal(t.sent[1], "+380501112233: balance is <low> & falling", "plain text is sent unescaped")
	equal(t.modes[1], "none", "the chosen mode reaches the transport")
	equal(t.receiver_reads, nil, "the modem is not asked for a number the template never uses")

	engine, t = timed({ core.copy(MSG) })
	engine:step({ token = CONFIG.token, chat_id = CONFIG.chat_id,
		template = "%receiver% %hostname% %router_time%", parse_mode = "none" })
	equal(t.sent[1], "+380990000000 hh71vm 2026-09-29 02:00:00", "the router's own facts are filled in")
	equal(t.receiver_reads, 1, "and the number is read once")

	-- Telegram refuses the whole request when the markup does not parse.  The message
	-- still has to arrive, or one bad template stops every SMS getting through.
	engine, t = timed({ core.copy(MSG) })
	t.answers[1] = { ok = false, error = "telegram_http_error", http_status = 400 }
	engine:step({ token = CONFIG.token, chat_id = CONFIG.chat_id,
		template = "<b>%sms_text%", parse_mode = "HTML" })
	equal(#t.sent, 2, "the rejected message is sent a second time")
	equal(t.modes[2], "none", "the retry carries no parse_mode")
	equal(t.sent[2], "<b>balance is <low> & falling", "and is the unescaped plain text")
	truthy(engine.state.template_rejected, "the page is told the formatting was refused")
	equal(engine.state.last_error, nil, "but the delivery itself counts as a success")

	-- A 500 is an ordinary transport failure and must still be retried, not downgraded.
	engine, t = timed({ core.copy(MSG) })
	t.answers[1] = { ok = false, error = "telegram_http_error", http_status = 500 }
	engine:step({ token = CONFIG.token, chat_id = CONFIG.chat_id, parse_mode = "HTML" })
	equal(#t.sent, 1, "a server error is not retried as plain text")
	equal(engine.state.last_error, "telegram_http_error", "and is reported")
end

print(("sms-to-telegram engine tests: %d assertions passed"):format(assertions))
