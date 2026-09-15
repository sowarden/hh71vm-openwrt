-- Regression guard for API.modem_restart (fixed 2026-09-15): must run the flight-mode
-- bounce (AT+CFUN=0, pause, AT+CFUN=1, full session re-setup) and must never again
-- send AT+CFUN=1,1, which was confirmed live to take the whole Realtek board down
-- instead of just the modem. Drives the real daemon state machine (API.modem_restart,
-- run_for, request/M.enqueue/M.start_next/M.step_send, M.on_line, M.after) with only
-- the transport (M.write_raw) stubbed, so the assertions exercise the actual command
-- sequence and ordering, not a paraphrase of it. Same harness shape as
-- test_modem_sms.lua/test_modem_apn.lua/test_modem_calls.lua.

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
	M = M,
	API = API,
}
]]
local chunk, load_error = loadstring(source, "@" .. daemon_path)
assert(chunk, load_error)
local d = chunk()
local M, API = d.M, d.API

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

-- The only real I/O boundary in this whole chain: capture what would have been sent
-- to the modem instead of touching a socket. Everything else (queueing, dispatch,
-- final-response handling, the M.after pause, the follow-up request) is the daemon's
-- own real code.
local sent = {}
function M.write_raw(payload)
	sent[#sent + 1] = (payload:gsub("\r$", ""))
	return true
end

M.link.state = "ready"

-- confirm=false must refuse before touching the radio at all.
do
	local dead_cli = { dead = true }
	API.modem_restart(dead_cli, {})
	equal(#sent, 0, "no confirm means no AT command is sent")
end

local cli = { dead = true }
API.modem_restart(cli, { confirm = true })

-- run_for enqueues and calls M.start_next() itself, so the first step (AT+CFUN=0)
-- should already be in flight.
equal(#sent, 1, "one command sent so far")
equal(sent[1], "AT+CFUN=0", "first command parks the radio, not a full reset")
truthy(M.cur ~= nil, "the CFUN=0 request is in flight")

-- Simulate the modem answering OK: this finishes the request, whose callback (the
-- body of API.modem_restart) schedules the CFUN=1 step via M.after(8, ...) instead of
-- sending it immediately -- the radio needs the real pause.
M.on_line("OK")
truthy(M.cur == nil, "CFUN=0 request finished")
equal(#sent, 1, "CFUN=1 is not sent immediately -- it is behind the M.after(8, ...) pause")
equal(#M.timers, 1, "the pause is scheduled as a timer")

-- Fire the timer by hand (this test does not run the real main loop's clock) --
-- mirrors how M.check_session()'s own sim-reprobe pair uses M.after the same way.
local due = table.remove(M.timers, 1)
due.fn()
M.start_next()

equal(#sent, 2, "the timer firing sends the second command")
equal(sent[2], "AT+CFUN=1", "second command re-enables the radio")
truthy(M.cur ~= nil, "the CFUN=1 request is in flight")

-- Simulate the modem answering OK again: this must trigger a full session re-setup,
-- exactly like M.check_session()'s sim-reprobe-on path does, and must record the
-- restart as acknowledged.
M.on_line("OK")
truthy(M.cur == nil, "CFUN=1 request finished")
equal(M.state.modem_restart.acknowledged, true, "restart recorded as acknowledged")

truthy(#M.queue >= 1, "a follow-up request was enqueued after the radio came back")
equal(M.queue[1].name, "setup", "the follow-up request is a full session setup")

-- The regression this whole test exists to prevent: AT+CFUN=1,1 (a full baseband
-- reset) must never be sent by this code path again.
for _, cmd in ipairs(sent) do
	truthy(cmd ~= "AT+CFUN=1,1", "AT+CFUN=1,1 must never be sent by modem_restart")
end
equal(#sent, 2, "exactly two AT commands for the whole restart, both flight-mode")

print(("modem restart tests: %d assertions passed"):format(assertions))
