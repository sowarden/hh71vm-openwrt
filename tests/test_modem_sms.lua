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
	sms_pending_add = sms_pending_add,
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
end

do
	local M = sms.M
	M.sms_pending_entries, M.sms_pending, M.sms_sync_attempts = {}, 0, 0
	M.sms_generation, M.sms_messages, M.sms_last_error = 7, { { index = 99 } }, nil
	sms.sms_pending_add("SM", 12)
	equal(M.sms_pending, 1, "CMTI pending count")
	equal(M.sms_sync_storage, "SM", "CMTI storage selection")
	equal(sms.sms_pending_reconcile({}, "SM"), 1, "delayed storage entry")
	truthy(sms.sms_schedule_retry("delayed"), "retry scheduled")
	truthy(M.sms_sync_due ~= nil, "retry due time")
	equal(M.sms_generation, 7, "retry does not replace cache")
	equal(M.sms_last_error, nil, "scheduled retry is not a terminal snapshot error")
	equal(sms.sms_pending_reconcile({ { index = 12 } }, "SM"), 0, "delayed entry found")
	equal(M.sms_pending, 0, "pending cleared after read")

	local job = sms.sms_list_job(function() end, "SM")
	equal(job.steps[1].cmd, "AT+CMGF=0", "PDU mode command")
	equal(job.steps[2].cmd, 'AT+CPMS="SM"', "CMTI storage command")
	equal(job.steps[3].cmd, "AT+CPMS?", "storage query")
	equal(job.steps[4].cmd, "AT+CMGL=4", "list command")
	equal(job.steps[4].timeout, 24, "rpc-bounded list timeout")
	equal(job.steps[4].tolerate, nil, "list errors are not tolerated")

	M.state.sms = { storage = "ME", receive_storage = "SM" }
	local receive_job = sms.sms_list_job(function() end)
	equal(receive_job.steps[2].cmd, 'AT+CPMS="SM"', "boot uses CPMS receive storage")
	local explicit_job = sms.sms_list_job(function() end, "ME")
	equal(explicit_job.steps[2].cmd, 'AT+CPMS="ME"', "CMTI storage overrides CPMS receive storage")
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
	SMS.loaded, SMS.seen = true, {}
	M.sms_pending_entries, M.sms_pending, M.sms_sync_storage = {}, 0, nil
	M.state.sms = { used = 0, storage = "ME" }
	M.sms_generation, M.sms_messages, M.sms_last_error = 0, nil, nil
	local outcome
	local empty_job = sms.sms_list_job(function(ok, list, err)
		outcome = { ok = ok, count = #list, err = err }
	end)
	empty_job.steps[#empty_job.steps].parse({})
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

	M.state.sms.used = 4
	local missing_headers = sms.sms_list_job(function(ok, list, err)
		outcome = { ok = ok, count = #list, err = err }
	end)
	missing_headers.steps[#missing_headers.steps].parse({})
	missing_headers.cb(true, {}, nil)
	equal(outcome.ok, false, "occupied store without headers fails")
	truthy(outcome.err:match("no parseable entries"), "occupied store diagnostic")
	equal(M.sms_generation, 1, "unparseable store preserves generation")

	M.state.sms.used = 0
	M.sms_pending_entries, M.sms_pending, M.sms_sync_attempts = {}, 0, 0
	sms.sms_pending_add("ME", 7)
	local delayed_job = sms.sms_list_job(function(ok, list, err)
		outcome = { ok = ok, count = #list, err = err }
	end, "ME")
	delayed_job.steps[#delayed_job.steps].parse({})
	delayed_job.cb(true, {}, nil)
	equal(outcome.ok, false, "CMTI before storage availability")
	equal(M.sms_pending, 1, "delayed CMTI remains pending")
	equal(M.sms_generation, 1, "delayed empty list preserves cache")

	local arrived_job = sms.sms_list_job(function(ok, list)
		outcome = { ok = ok, count = #list }
	end, "ME")
	arrived_job.steps[#arrived_job.steps].parse({ "+CMGL: 7,0,,44", pdu_a })
	arrived_job.cb(true, {}, nil)
	equal(outcome.ok, true, "delayed CMTI retry success")
	equal(M.sms_pending, 0, "delayed CMTI reconciled")
	equal(M.sms_generation, 2, "delayed retry generation")

	local restart_job = sms.sms_list_job(function(ok, list)
		outcome = { ok = ok, count = #list }
	end)
	restart_job.steps[#restart_job.steps].parse({ "+CMGL: 0,0,,44", pdu_a })
	restart_job.cb(true, {}, nil)
	equal(outcome.ok, true, "restart refresh")
	equal(outcome.count, 1, "restart existing message")
	equal(M.sms_generation, 3, "restart generation")
end

print(("sms modem tests: %d assertions passed"):format(assertions))
