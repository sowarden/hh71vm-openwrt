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

print(("sms-to-telegram engine tests: %d assertions passed"):format(assertions))
