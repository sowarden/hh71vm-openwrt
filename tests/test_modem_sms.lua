-- Focused SMS parser/state regressions. This loads the production daemon source with
-- inert module stubs and stops before its CLI, so every assertion exercises its real
-- local functions without opening a modem or control socket.

local daemon_path = assert(arg[1], "daemon path required")

package.preload["nixio"] = function()
	return {
		const = { EINPROGRESS = 115, EAGAIN = 11 },
		poll_flags = function() return 0 end,
		gettimeofday = function() return os.time(), 0 end,
	}
end
package.preload["nixio.fs"] = function()
	return {
		mkdir = function() return true end,
		mkdirr = function() return true end,
		move = function() return true end,
		unlink = function() return true end,
		chmod = function() return true end,
		readfile = function() return nil end,
		writefile = function() return true end,
	}
end
package.preload["luci.jsonc"] = function()
	return {
		parse = function() return nil end,
		stringify = function() return "{}" end,
	}
end

local file = assert(io.open(daemon_path, "r"))
local source = file:read("*a")
file:close()
source = source:gsub("^#![^\n]*\n", "", 1)
local marker = "--=========================================================================== CLI"
local cut = assert(source:find(marker, 1, true), "CLI marker missing")
source = source:sub(1, cut - 1) .. [[
return {
	pdu_decode = pdu_decode,
	parse_sms_status = parse_sms_status,
	parse_sms_header = parse_sms_header,
	parse_cmgl = parse_cmgl,
	assemble = assemble,
	sms_key = sms_key,
	sms_slot = sms_slot,
	sms_active_stores = sms_active_stores,
	sms_known_stores = sms_known_stores,
	sms_preferred_store = sms_preferred_store,
	sms_store_has_room = sms_store_has_room,
	sms_receive_target = sms_receive_target,
	sms_aim_steps = sms_aim_steps,
	sms_storage_mode_get = sms_storage_mode_get,
	sms_storage_mode_set = sms_storage_mode_set,
	sms_storage_for = sms_storage_for,
	sms_pending_add = sms_pending_add,
	sms_pending_forget = sms_pending_forget,
	sms_pending_reconcile = sms_pending_reconcile,
	sms_schedule_retry = sms_schedule_retry,
	sms_list_job = sms_list_job,
	SMS = SMS,
	M = M,
}
]]
local chunk, load_error = loadstring(source, "@" .. daemon_path)
assert(chunk, load_error)
local sms = chunk()

local assertions = 0
local function equal(actual, expected, label)
	assertions = assertions + 1
	if actual ~= expected then
		error((label or "value") .. ": expected " .. tostring(expected) ..
		      ", got " .. tostring(actual), 2)
	end
end
local function truthy(value, label)
	assertions = assertions + 1
	if not value then error(label or "expected truthy value", 2) end
end

--- A step's `cmd` is a plain string almost everywhere, and a zero-argument function for
--- the storage-aim step alone -- resolved lazily by M.step_send, at the moment it is
--- actually sent, because it depends on occupancy counts earlier steps in the same
--- request just read.  This harness drives steps by hand without going through
--- M.step_send, so it has to do that resolution itself wherever it inspects step.cmd.
local function step_cmd(step)
	if type(step.cmd) == "function" then return step.cmd() end
	return step.cmd
end

local pdu_a = "00000B912120550501F10000629050100010001CD3E614C44ECFE920723A7C76BFE7F4F41814042D6F4D593407"
local pdu_b = "00000B912120550501F20000629050100020001CD3E614C44ECFE920723A7C76BFE7F4F418240459694E1C3406"
local pdu_multi_1 = "00400B912120550501F3000062905010003000A0050003020201A6CD29889D9ED341ED3A9B9E8687E57410391D3EBBDF737A7A0C1A82A436EAEC8A03C162B219AD66BBE172A0A070482C1A8FC8A412048BC966B49AED86CB8182C221B1683C22934A102C269BD16AB61B2E070A0A87C4A2F1884C2A41B0986C46ABD96EB81C28281C128BC62332A904C162B219AD66BBE172A0A070482C1A8FC8A412048BC966"
local pdu_multi_2 = "00400B912120550501F30000629050100030002E0500030202026835DB0D970305854362D17844269520584C36A3D56C375C0E14140E8945E311995402"
local pdu_ucs2 = "00000B912120550501F4000862905010004000080054006500730074"
local pdu_8bit = "00000B912120550501F5000462905010005000030102FF"

do
	local message = assert(sms.pdu_decode(pdu_a))
	equal(message.sender, "+12025550101", "GSM-7 sender")
	equal(message.text, "SMS list diagnostic A K7M2Q9", "GSM-7 text")
	equal(message.ts, "26/09/05,01:00:01+00", "timestamp")
	local ucs2 = assert(sms.pdu_decode(pdu_ucs2))
	equal(ucs2.text, "Test", "UCS2 text")
	local binary = assert(sms.pdu_decode(pdu_8bit))
	equal(binary.text, "0102FF", "8-bit payload")
end

do
	local numeric, numeric_name = sms.parse_sms_status("0")
	equal(numeric, 0, "numeric status")
	equal(numeric_name, "REC UNREAD", "numeric status name")
	local textual = sms.parse_sms_status('"REC READ"')
	equal(textual, 1, "quoted textual status")
	local underscored = sms.parse_sms_status(' "rec_unread" ')
	equal(underscored, 0, "normalized textual status")
	local index, code = sms.parse_sms_header('+CMGL: 7, "REC UNREAD", "", 44', "CMGL")
	equal(index, 7, "spaced header index")
	equal(code, 0, "spaced header status")
	local _, read_code = sms.parse_sms_header('+CMGR: "REC READ",,44', "CMGR")
	equal(read_code, 1, "CMGR textual status")
end

do
	local empty, empty_report = sms.parse_cmgl({})
	equal(#empty, 0, "empty SIM")
	equal(empty_report.headers, 0, "empty headers")
	local list, report = sms.parse_cmgl({
		'+CMGL: 0, "REC UNREAD", "", 44', pdu_a,
		"+CMGL:   1 , 1 , , 44", pdu_b,
	})
	equal(#list, 2, "multiple messages")
	equal(report.decode_errors, 0, "valid decode count")
	equal(list[1].unread, true, "unread text status")
	equal(list[2].unread, false, "read numeric status")

	local resilient, bad_report = sms.parse_cmgl({
		"+CMGL: 4,0,,1", "NOT-A-PDU",
		"+CMGL: 5,1,,44", pdu_b,
		"+CMGL: 6,0,,1",
		"+CMGL: 7,0,,3", "0040008100000000000000000003080804",
		"+CMGL: 8,0,,44", pdu_a,
	})
	equal(#resilient, 5, "malformed entries remain visible")
	equal(bad_report.decode_errors, 3, "malformed count")
	truthy(resilient[1].decode_error, "malformed placeholder")
	equal(resilient[2].text, "SMS list diagnostic B V4N8P1", "valid after malformed")
	truthy(resilient[4].decode_error, "truncated UDH placeholder")
	equal(resilient[5].text, "SMS list diagnostic A K7M2Q9", "missing body does not eat header")
end

do
	local segments = sms.parse_cmgl({
		"+CMGL: 3,0,,60", pdu_multi_2,
		"+CMGL: 2,0,,159", pdu_multi_1,
	})
	local assembled = sms.assemble(segments)
	equal(#assembled, 1, "multipart count")
	equal(assembled[1].parts, 2, "multipart parts")
	equal(assembled[1].indexes[1], 2, "sorted first index")
	equal(assembled[1].indexes[2], 3, "sorted second index")
	truthy(assembled[1].text:match("SMS list multipart diagnostic C"), "multipart text")

	local one_part = sms.parse_cmgl({ "+CMGL: 2,0,,159", pdu_multi_1 })
	local incomplete = sms.assemble(one_part)
	equal(incomplete[1].missing, 1, "incomplete multipart")
end

do
	local state = sms.M.state
	state.sms = {}
	sms.M.parsers.cpms({ '+CPMS: "ME",4,100,"ME",4,100,"SM",10,10' })
	equal(state.sms.storage, "ME", "quoted CPMS storage")
	equal(state.sms.used, 4, "quoted CPMS used")
	equal(state.sms.write_storage, "ME", "quoted CPMS write storage")
	equal(state.sms.receive_storage, "SM", "quoted CPMS receive storage")
	equal(state.sms.receive_used, 10, "quoted CPMS receive used")
	sms.M.parsers.cpms({ '+CPMS: "SM" , 3 , 15 , "ME" , 0 , 100 , "ME" , 2 , 100' })
	equal(state.sms.storage, "SM", "spaced CPMS read storage")
	equal(state.sms.write_storage, "ME", "spaced CPMS write storage")
	equal(state.sms.receive_storage, "ME", "spaced CPMS receive storage")
	equal(state.sms.receive_used, 2, "spaced CPMS receive used")
	sms.M.parsers.cpms({ "+CPMS:  SM , 2 , 10" })
	equal(state.sms.storage, "SM", "unquoted CPMS storage")
	equal(state.sms.total, 10, "unquoted CPMS total")
	sms.M.parsers.cpms({ "+CPMS: 3, 50" })
	equal(state.sms.storage, "SM", "short CPMS preserves storage")
	equal(state.sms.used, 3, "short CPMS used")
	equal(state.sms.total, 50, "short CPMS total")
	state.sms.write_storage, state.sms.receive_storage = "ME", "SM"
	sms.M.parsers.cpms({ "+CPMS: 4,50,5,60,6,70" })
	equal(state.sms.storage, "SM", "numeric CPMS preserves read storage")
	equal(state.sms.write_storage, "ME", "numeric CPMS preserves write storage")
	equal(state.sms.receive_storage, "SM", "numeric CPMS preserves receive storage")
	equal(state.sms.used, 4, "numeric CPMS read used")
	equal(state.sms.write_used, 5, "numeric CPMS write used")
	equal(state.sms.receive_used, 6, "numeric CPMS receive used")
end

do
	truthy(sms.sms_key(9, "26/09/05,01:00:01+00") ~=
	       sms.sms_key(9, "26/09/05,01:00:02+00"), "reused index timestamp")
	truthy(sms.sms_key(9, nil, { sender = "A", text = "one" }) ~=
	       sms.sms_key(9, nil, { sender = "A", text = "two" }), "missing timestamp fingerprint")

	-- Slot numbers repeat across stores: measured on the stand, ME held 0-7 while SM
	-- held 0-9 on the same modem, so an index alone cannot identify a message.
	equal(sms.sms_slot("ME", 3), "ME:3", "store-qualified slot")
	equal(sms.sms_slot(nil, 3), "3", "slot without a store keeps the bare index")
	truthy(sms.sms_key(sms.sms_slot("ME", 3), "T") ~= sms.sms_key(sms.sms_slot("SM", 3), "T"),
	       "same index in two stores is two keys")

	-- A message with no timestamp is keyed by a fingerprint, and a stored draft never
	-- has one.  Lua 5.1's "%x" casts through a signed 32-bit integer on the target, so
	-- a hash with the top bit set used to abort the whole listing with
	-- "bad argument #1 to 'format'".  Half of all hashes are that large.
	local high = 0
	for i = 1, 400 do
		local key = sms.sms_key("ME:0", nil, { sender = "S", text = "draft " .. i })
		local digest = key:match("|~(.*)$")
		assertions = assertions + 1
		if not digest or not digest:match("^%x%x%x%x%x%x%x%x$") then
			error("fingerprint is not eight hex digits: " .. tostring(key), 2)
		end
		if digest:byte(1) >= ("8"):byte() then high = high + 1 end
	end
	truthy(high > 0, "fingerprints above 2^31 are produced and survive formatting")
end

do
	-- Read state written before messages carried a store name must survive the
	-- upgrade: AT+CMGL has already cleared the modem's own flag by then, so losing it
	-- would silently mark every stored message unread again.
	local SMS = sms.SMS
	SMS.loaded, SMS.seen = true, { ["4|26/09/05,01:00:01+00"] = "unread" }
	local merged = { { index = 4, ts = "26/09/05,01:00:01+00", storage = "SM", unread = false } }
	equal(SMS.merge(merged), 1, "legacy read state adopted")
	equal(merged[1].unread, true, "legacy unread flag preserved")
	truthy(SMS.seen["SM:4|26/09/05,01:00:01+00"] == "unread", "adopted under the store key")
	truthy(SMS.seen["4|26/09/05,01:00:01+00"] == nil, "legacy key retired after one merge")

	-- The same slot number in the other store is a different message.
	SMS.loaded, SMS.seen = true, {}
	SMS.merge({ { index = 4, ts = "T", storage = "ME", unread = true },
	            { index = 4, ts = "T", storage = "SM", unread = false } })
	SMS.mark("ME", 4, "T", "read")
	equal(SMS.seen["ME:4|T"], "read", "mark hits the named store")
	equal(SMS.seen["SM:4|T"], "read", "same index in the other store is untouched")
	SMS.mark("SM", 4, "T", "unread")
	equal(SMS.seen["ME:4|T"], "read", "marking one store does not reach the other")
	equal(SMS.seen["SM:4|T"], "unread", "second store marked independently")
	SMS.forget("ME", 4)
	equal(SMS.seen["ME:4|T"], nil, "forget removes the named store")
	equal(SMS.seen["SM:4|T"], "unread", "forget leaves the other store alone")
end

do
	local M = sms.M
	M.sms_pending_entries, M.sms_pending, M.sms_sync_attempts = {}, 0, 0
	M.sms_generation, M.sms_messages, M.sms_last_error = 7, { { index = 99 } }, nil
	sms.sms_pending_add("SM", 12)
	equal(M.sms_pending, 1, "CMTI pending count")
	equal(M.sms_sync_storage, "SM", "CMTI storage selection")
	equal(sms.sms_pending_reconcile({}, { "SM" }), 1, "delayed storage entry")
	truthy(sms.sms_schedule_retry("delayed"), "retry scheduled")
	truthy(M.sms_sync_due ~= nil, "retry due time")
	equal(M.sms_generation, 7, "retry does not replace cache")
	equal(M.sms_last_error, nil, "scheduled retry is not a terminal snapshot error")
	equal(sms.sms_pending_reconcile({ { index = 12, storage = "SM" } }, { "SM" }), 0,
	      "delayed entry found")
	equal(M.sms_pending, 0, "pending cleared after read")

	-- A notification for a store the user chose not to read is not "still on its way".
	-- Holding it pending would turn a deliberate setting into a permanent error.
	M.sms_pending_entries, M.sms_pending, M.sms_sync_attempts = {}, 0, 0
	sms.sms_pending_add("SM", 3)
	equal(sms.sms_pending_reconcile({}, { "ME" }), 0, "notification outside the read set is dropped")

	-- The same slot number in the store that was not read must not satisfy it.
	M.sms_pending_entries, M.sms_pending, M.sms_sync_attempts = {}, 0, 0
	sms.sms_pending_add("SM", 5)
	equal(sms.sms_pending_reconcile({ { index = 5, storage = "ME" } }, { "ME", "SM" }), 1,
	      "same index in the wrong store does not reconcile")
	equal(sms.sms_pending_reconcile({ { index = 5, storage = "SM" } }, { "ME", "SM" }), 0,
	      "matching store reconciles")
end

do
	-- Store selection.  This is the regression the whole change exists for: preferring
	-- CPMS's receive memory made every refresh read SM alone, and the eight messages
	-- sitting in ME on the owner's modem stopped being listed at all.
	local M = sms.M
	M.state.sms = { storage = "ME", receive_storage = "SM", used = 0 }

	local function commands(job)
		local out = {}
		for _, step in ipairs(job.steps) do out[#out + 1] = step_cmd(step) end
		return table.concat(out, " ")
	end

	-- The aim step's target depends on M.sms_cpms3 (3-argument capability, learned per
	-- channel) and on store_counts (occupancy). Reset to the "nothing known yet" state
	-- so these assertions do not depend on order against later do-blocks.
	M.sms_cpms3 = nil
	M.state.sms.store_counts = nil

	local both = sms.sms_list_job(function() end)
	equal(commands(both),
	      'AT+CMGF=0 AT+CPMS="ME" AT+CMGL=4 AT+CPMS="SM" AT+CMGL=4 AT+CPMS="ME","ME","ME" AT+CPMS?',
	      "default reads every store, receive memory does not narrow it, and the tail " ..
	      "aims at ME (the preferred store) rather than wherever the scan last read")

	local named = sms.sms_list_job(function() end, "SM")
	equal(commands(named), 'AT+CMGF=0 AT+CPMS="SM" AT+CMGL=4 AT+CPMS="ME","ME","ME" AT+CPMS?',
	      "an explicit store is read alone, but the receive aim still follows the " ..
	      "general preference, not this one-off request")

	-- The listing deadline is shared out, never handed to each store: two stores must
	-- not be able to outlast the 28 s rpcd call waiting for them.
	local budget = sms.sms_list_job(function() end, nil, 20)
	local listings = {}
	for _, step in ipairs(budget.steps) do
		if step.cmd == "AT+CMGL=4" then listings[#listings + 1] = step end
	end
	equal(#listings, 2, "one listing per store")
	equal(listings[1].timeout, 10, "shared deadline per store")
	truthy(listings[1].timeout * 2 < 28, "both stores fit inside the rpcd timeout")

	-- Only stores the modem actually reported are read, and the receive aim follows:
	-- a modem that only ever advertised SM must never be aimed at an ME it does not
	-- have.
	M.state.sms.stores = { "SM" }
	equal(commands(sms.sms_list_job(function() end)),
	      'AT+CMGF=0 AT+CPMS="SM" AT+CMGL=4 AT+CPMS="SM","SM","SM" AT+CPMS?',
	      "a modem with one message store is read once and aims only at that store")
	M.state.sms.stores = nil

	sms.M.parsers.cpms_range({ '+CPMS: ("ME","MT","SM","SR"),("ME","MT","SM","SR"),("ME","SM")' })
	equal(table.concat(M.state.sms.stores, ","), "ME,SM",
	      "MT and SR are not message stores to list")
end

do
	-- Where an incoming message should land: this is gap 3 from session 2, the
	-- regression that motivated this whole change. Every store-scan job used to end on
	-- a bare AT+CPMS?, which left mem1/mem2/mem3 wherever the scan's last per-store
	-- select happened to leave them -- SM on both known units -- and three real
	-- messages sent while SM was full and selected were refused outright.
	local M = sms.M
	M.state.sms = { storage_mode = nil, stores = { "ME", "SM" }, store_counts = nil }
	sms.sms_storage_mode_set("both")

	equal(sms.sms_preferred_store(), "ME",
	      "\"both\" prefers ME -- the 100-slot store -- over the 10-15 slot SIM")

	M.state.sms.store_counts = { ME = { used = 100, total = 100 } }
	equal(sms.sms_receive_target(), "SM",
	      "a full preferred store falls back to the other known store")
	equal(M.state.sms.stores_full, false, "the fallback store has room, so nothing is full")

	M.state.sms.store_counts = { ME = { used = 100, total = 100 }, SM = { used = 10, total = 10 } }
	equal(sms.sms_receive_target(), "ME",
	      "with nowhere to go, the target stays the preferred store")
	equal(M.state.sms.stores_full, true, "and stores_full is raised so the page can say so")

	M.state.sms.store_counts = nil
	equal(sms.sms_receive_target(), "ME",
	      "a store never yet measured this session is assumed to have room, not refused")
	equal(M.state.sms.stores_full, false, "an unmeasured store is not reported full")

	-- An explicit mode is itself the preference, but never for a store the modem does
	-- not actually have -- untested on real hardware, but not to be assumed impossible.
	-- sms_storage_mode_set persists to /etc/hh71vm-modem/, which a dev/CI host may not
	-- have writable; skip rather than fail the suite over an unrelated environment gap.
	local set_ok = sms.sms_storage_mode_set("SM")
	if set_ok then
		M.state.sms.store_counts = nil
		equal(sms.sms_preferred_store(), "SM", "an explicit mode is honoured when available")
		M.state.sms.stores = { "ME" }
		equal(sms.sms_preferred_store(), "ME",
		      "an explicit mode naming a store the modem never advertised falls back to one it has")
		M.state.sms.stores = { "ME", "SM" }
		sms.sms_storage_mode_set("both")
	else
		print("sms modem tests: skipped explicit-mode-fallback assertions " ..
		      "(/etc/hh71vm-modem is not writable in this environment)")
	end

	-- The aim command itself: three arguments while the modem has not refused that
	-- form, two once M.sms_cpms3 remembers a refusal -- proved on the stand
	-- 2026-09-08, where AT+CPMS="ME","ME","ME" moved mem3 and survived a daemon
	-- restart, so it is the default rather than an opt-in.
	M.sms_cpms3 = nil
	M.state.sms.store_counts = nil
	local steps = sms.sms_aim_steps(sms.sms_receive_target)
	equal(#steps, 2, "one aim command plus one confirming read")
	equal(step_cmd(steps[1]), 'AT+CPMS="ME","ME","ME"',
	      "three-argument form by default -- moves mem1, mem2 and mem3 together")
	steps[1].parse({}, true)
	equal(M.sms_cpms3, true, "a successful three-argument aim is remembered as supported")

	M.sms_cpms3 = nil
	local refused = sms.sms_aim_steps(sms.sms_receive_target)
	refused[1].parse({ "+CME ERROR: 4" }, false)
	equal(M.sms_cpms3, false, "a refused three-argument aim is remembered as unsupported")
	local after_refusal = sms.sms_aim_steps(sms.sms_receive_target)
	equal(step_cmd(after_refusal[1]), 'AT+CPMS="ME","ME"',
	      "every later aim falls back to the two-argument form without retrying the refusal")
end

do
	local M = sms.M
	M.sms_pending_entries, M.sms_pending, M.sms_sync_attempts = {}, 0, 0
	M.sms_last_error = nil
	sms.sms_pending_add("SM", 13)
	for _ = 1, 4 do truthy(sms.sms_schedule_retry("still missing"), "bounded retry") end
	equal(sms.sms_schedule_retry("still missing"), false, "retry exhaustion")
	equal(M.sms_pending, 1, "exhausted notification remains visible")
	equal(M.sms_last_error, "still missing", "retry exhaustion is explicit")
end

do
	-- Empty success, explicit failure and restart-style repopulation keep distinct
	-- outcomes and generation changes.
	local M, SMS = sms.M, sms.SMS

	--- Drive a job the way the AT engine would: each store's select answers with its
	--- own slot counts, each listing answers with that store's +CMGL block.
	--- `plan[store] = { used = n, lines = {...}, select_ok = false }`
	local function run(job, plan)
		local store
		for _, step in ipairs(job.steps) do
			local cmd = step_cmd(step)
			local named = cmd:match('^AT%+CPMS="(%u%u)"$')
			if named then
				store = named
				local p = plan[store] or {}
				local ok = p.select_ok ~= false
				step.parse({ ("+CPMS: %d,100,0,100,0,10"):format(p.used or 0) }, ok)
			elseif cmd == "AT+CMGL=4" then
				local p = plan[store] or {}
				step.parse(p.lines or {}, p.list_ok ~= false)
			end
		end
	end

	local function reset(state)
		SMS.loaded, SMS.seen = true, {}
		M.sms_pending_entries, M.sms_pending, M.sms_sync_storage = {}, 0, nil
		M.sms_sync_attempts = 0
		M.state.sms = state
		M.sms_generation, M.sms_messages, M.sms_last_error = 0, nil, nil
	end

	reset({ used = 0, storage = "ME", stores = { "ME" } })
	local outcome
	local empty_job = sms.sms_list_job(function(ok, list, err)
		outcome = { ok = ok, count = #list, err = err }
	end)
	run(empty_job, { ME = { used = 0 } })
	empty_job.cb(true, {}, nil)
	equal(outcome.ok, true, "genuine empty success")
	equal(outcome.count, 0, "genuine empty count")
	equal(M.sms_generation, 1, "empty refresh generation")

	local failed_job = sms.sms_list_job(function(ok, list, err)
		outcome = { ok = ok, count = #list, err = err }
	end)
	failed_job.cb(false, {}, "ERROR")
	equal(outcome.ok, false, "list failure")
	equal(outcome.err, "ERROR", "list failure detail")
	equal(M.sms_generation, 1, "failure preserves generation")

	local missing_headers = sms.sms_list_job(function(ok, list, err)
		outcome = { ok = ok, count = #list, err = err }
	end)
	run(missing_headers, { ME = { used = 4 } })
	missing_headers.cb(true, {}, nil)
	equal(outcome.ok, false, "occupied store without headers fails")
	truthy(outcome.err:match("no parseable entries"), "occupied store diagnostic")
	equal(M.sms_generation, 1, "unparseable store preserves generation")

	M.sms_pending_entries, M.sms_pending, M.sms_sync_attempts = {}, 0, 0
	sms.sms_pending_add("ME", 7)
	local delayed_job = sms.sms_list_job(function(ok, list, err)
		outcome = { ok = ok, count = #list, err = err }
	end, "ME")
	run(delayed_job, { ME = { used = 0 } })
	delayed_job.cb(true, {}, nil)
	equal(outcome.ok, false, "CMTI before storage availability")
	equal(M.sms_pending, 1, "delayed CMTI remains pending")
	equal(M.sms_generation, 1, "delayed empty list preserves cache")

	local arrived_job = sms.sms_list_job(function(ok, list)
		outcome = { ok = ok, count = #list }
	end, "ME")
	run(arrived_job, { ME = { used = 1, lines = { "+CMGL: 7,0,,44", pdu_a } } })
	arrived_job.cb(true, {}, nil)
	equal(outcome.ok, true, "delayed CMTI retry success")
	equal(M.sms_pending, 0, "delayed CMTI reconciled")
	equal(M.sms_generation, 2, "delayed retry generation")

	local restart_job = sms.sms_list_job(function(ok, list)
		outcome = { ok = ok, count = #list }
	end)
	run(restart_job, { ME = { used = 1, lines = { "+CMGL: 0,0,,44", pdu_a } } })
	restart_job.cb(true, {}, nil)
	equal(outcome.ok, true, "restart refresh")
	equal(outcome.count, 1, "restart existing message")
	equal(M.sms_generation, 3, "restart generation")
end

do
	-- Two stores in one list: this is the shape the owner's modem actually has, with
	-- eight messages in ME and ten in SM, and slot numbers that collide.
	local M, SMS = sms.M, sms.SMS
	SMS.loaded, SMS.seen = true, {}
	M.sms_pending_entries, M.sms_pending, M.sms_sync_attempts = {}, 0, 0
	M.state.sms = { storage = "SM", receive_storage = "SM", stores = { "ME", "SM" } }
	M.sms_generation, M.sms_messages, M.sms_last_error = 0, nil, nil

	local outcome
	local job = sms.sms_list_job(function(ok, list, err)
		outcome = { ok = ok, list = list, err = err }
	end)
	local store
	for _, step in ipairs(job.steps) do
		local cmd = step_cmd(step)
		local named = cmd:match('^AT%+CPMS="(%u%u)"$')
		if named then
			store = named
			step.parse({ "+CPMS: 1,100,0,100,0,10" }, true)
		elseif cmd == "AT+CMGL=4" then
			-- the same slot number in both stores, which is why storage has to travel
			-- with the message
			step.parse({ "+CMGL: 0,0,,44", store == "ME" and pdu_a or pdu_b }, true)
		end
	end
	job.cb(true, {}, nil)
	equal(outcome.ok, true, "both stores listed")
	equal(#outcome.list, 2, "a colliding slot number in each store is two messages")
	local seen = {}
	for _, m in ipairs(outcome.list) do seen[m.storage] = m.index end
	equal(seen.ME, 0, "message kept its ME slot")
	equal(seen.SM, 0, "message kept its SM slot")
	equal(M.state.sms.read_stores[1] .. "," .. M.state.sms.read_stores[2], "ME,SM",
	      "the list reports which stores it read")

	-- The cached list is what tells a later delete which store a slot belongs to.
	M.sms_messages = outcome.list
	equal(sms.sms_storage_for({ 0 }), outcome.list[1].storage, "slot resolved from the cache")

	-- One store refusing to be selected must not discard the store that answered --
	-- on a modem with a single message store that would hide every message there is --
	-- and must not let the answering store's messages be filed under its name.
	M.sms_generation, M.sms_last_error = 0, nil
	local partial = sms.sms_list_job(function(ok, list, err, _, report)
		outcome = { ok = ok, list = list, err = err, report = report }
	end)
	store = nil
	for _, step in ipairs(partial.steps) do
		local cmd = step_cmd(step)
		local named = cmd:match('^AT%+CPMS="(%u%u)"$')
		if named then
			store = named
			step.parse({ "+CPMS: 1,100,0,100,0,10" }, store ~= "SM")
		elseif cmd == "AT+CMGL=4" then
			step.parse({ "+CMGL: 0,0,,44", pdu_a }, true)
		end
	end
	partial.cb(true, {}, nil)
	equal(outcome.ok, true, "the store that answered still produces a list")
	truthy(outcome.report.store_error:match("SM"), "the refused store is named")
	equal(#outcome.list, 1, "only the store that was selected contributed messages")
	equal(outcome.list[1].storage, "ME", "no message filed under the refused store")
	equal(M.state.sms.read_stores[1], "ME", "only the store that answered counts as read")
	equal(#M.state.sms.read_stores, 1, "the refused store is not reported as read")
	equal(M.sms_generation, 1, "a partial list is still a list")

	-- Every store failing is a real failure, not a quiet empty inbox.
	M.sms_generation = 0
	local dead = sms.sms_list_job(function(ok, list, err)
		outcome = { ok = ok, list = list, err = err }
	end)
	for _, step in ipairs(dead.steps) do
		if step_cmd(step):match('^AT%+CPMS="%u%u"$') then step.parse({ "+CMS ERROR: 321" }, false) end
	end
	dead.cb(true, {}, nil)
	equal(outcome.ok, false, "no store readable is a failure")
	truthy(outcome.err:match("ME") and outcome.err:match("SM"), "both refused stores named")
	equal(M.sms_generation, 0, "a total failure preserves the cache")
end

do
	-- Store-scan order must not leak into display order. Found 2026-09-08 on real
	-- hardware: the owner sent 3 fresh messages that landed in ME, and LuCI showed
	-- them *below* two-week-old SM messages, because assemble() used to preserve
	-- whatever order the per-store scan produced (ME's slots, then SM's) with no
	-- notion of when a message actually arrived. LuCI reverses this list to show
	-- newest first, so assemble() has to return real chronological order, oldest
	-- first, regardless of which store answered when.
	local old_sm  = { index = 0, storage = "SM", sender = "OLD",  ts = "26/08/06,11:45:11+12" }
	local mid_sm  = { index = 1, storage = "SM", sender = "MID",  ts = "26/08/18,05:09:03+12" }
	local new_me1 = { index = 4, storage = "ME", sender = "NEW1", ts = "26/09/08,11:38:33+00" }
	local new_me2 = { index = 5, storage = "ME", sender = "NEW2", ts = "26/09/08,11:39:03+00" }
	local draft   = { index = 8, storage = "ME", sender = "DRAFT", ts = "" }

	local function order_of(list)
		local out = sms.assemble(list)
		local senders = {}
		for _, m in ipairs(out) do senders[#senders + 1] = m.sender end
		return table.concat(senders, ",")
	end

	equal(order_of({ new_me1, new_me2, old_sm, mid_sm, draft }),
	      "OLD,MID,NEW1,NEW2,DRAFT",
	      "chronological order survives even when ME's fresh messages are fed first")
	equal(order_of({ mid_sm, draft, new_me2, old_sm, new_me1 }),
	      "OLD,MID,NEW1,NEW2,DRAFT",
	      "the sort does not depend on input order at all")
	equal(order_of({ draft }), "DRAFT", "a lone undated draft does not crash the sort")
end

print(("sms modem tests: %d assertions passed"):format(assertions))
