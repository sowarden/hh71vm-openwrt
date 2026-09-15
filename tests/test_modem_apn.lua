-- Focused APN-reconciler regressions: SIM-identity scoping, PDP-type tolerance, the
-- rate limit and the give-up cap (fixed 2026-09-14, reported: a different SIM had no
-- internet after the previous SIM's APN was forced onto it, and the router
-- generally became unreliable after this reconciler ran repeatedly). Loads the
-- production daemon source with inert module stubs and stops before its CLI, so
-- every assertion exercises the real local functions without opening a modem or
-- control socket.

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
	apn_reconcile = apn_reconcile,
	apn_state_get = apn_state_get,
	apn_state_set = apn_state_set,
	apn_state_clear = apn_state_clear,
	current_sim_id = current_sim_id,
	M = M,
}
]]
local chunk, load_error = loadstring(source, "@" .. daemon_path)
assert(chunk, load_error)
local apn = chunk()

-- apn_reconcile's rate limit and give-up cap compare against real os.time(); a
-- controllable fake clock is what lets both be exercised without an actual
-- ten-minute test. os.time is a plain field on the shared global `os` table, so
-- overwriting it here reaches every call inside the already-loaded daemon chunk
-- too.
local fake_now = 2000000000
os.time = function() return fake_now end

local assertions = 0
local function equal(actual, expected, label)
	assertions = assertions + 1
	if actual ~= expected then
		error(("%s: expected %s, got %s"):format(label, tostring(expected), tostring(actual)), 2)
	end
end

local M = apn.M
local S = M.state

--- One context, as if freshly measured by the slow poll -- what apn_reconcile
--- compares the saved state against.
local function set_live(cid, apn_name, pdp_type, auth)
	S.apn = { count = 1, list = { { cid = cid, apn = apn_name, pdp_type = pdp_type,
	                                active = true } } }
	M.cgauth = { [tostring(cid)] = auth or 0 }
end

local function set_sim(iccid)
	S.sim = { iccid = iccid }
end

--- apn_state_set persists to /etc/hh71vm-modem/, which this environment does not
--- have writable -- confirmed by test_modem_sms.lua's own check of the same path.
--- The in-memory apn_state upvalue is set unconditionally before that write is
--- attempted, so apn_state_get() reflects it either way; nothing here depends on
--- apn_state_set's own return value.

do
	-- An entry from before this fix, or one saved before the SIM had been read
	-- yet, has no identity: the first SIM seen claims it, and reconciliation
	-- then proceeds normally against that SIM.
	fake_now = fake_now + 700
	M.queue = {}
	S.apn_reconcile = nil
	set_sim("8900000000000000001")
	set_live(1, "old-apn", "IPV4V6", 0)
	apn.apn_state_set({ cid = 1, apn = "good-apn", pdp_type = "IPV4V6", auth = 0 })
	equal(apn.apn_state_get().sim_id, nil, "a freshly-saved entry starts with no identity")

	apn.apn_reconcile()
	equal(apn.apn_state_get().sim_id, "8900000000000000001",
	      "the first SIM seen claims an identity-less entry")
	equal(#M.queue, 1, "a genuine APN mismatch on the (now-claimed) SIM is re-applied")
	equal(S.apn_reconcile.state, "reapplying", "the published state says so")
end

do
	-- A different SIM in the slot must not have the previous SIM's APN forced
	-- onto it. Reported 2026-09-14: a second SIM, on a different operator with
	-- no TTL-related block of its own, still had no internet -- consistent with
	-- this having been forced back onto it.
	fake_now = fake_now + 700
	M.queue = {}
	S.apn_reconcile = nil
	apn.apn_state_set({ cid = 1, apn = "good-apn", pdp_type = "IPV4V6", auth = 0,
	                     sim_id = "8900000000000000001" })
	set_sim("8900000000000000002")
	set_live(1, "operator-default", "IPV4V6", 0)

	apn.apn_reconcile()
	equal(#M.queue, 0, "a different SIM's context is left alone")
	equal(S.apn_reconcile.state, "different_sim", "the published state says why")
	equal(apn.apn_state_get().sim_id, "8900000000000000001",
	      "the saved identity is not overwritten by the SIM that was rejected")
end

do
	-- APN comparison is case-insensitive: the network echoing the same APN back
	-- in a different case must not look like drift.
	fake_now = fake_now + 700
	M.queue = {}
	S.apn_reconcile = nil
	apn.apn_state_set({ cid = 1, apn = "Internet", pdp_type = "IPV4V6", auth = 0,
	                     sim_id = "8900000000000000009" })
	set_sim("8900000000000000009")
	set_live(1, "INTERNET", "IPV4V6", 0)

	apn.apn_reconcile()
	equal(#M.queue, 0, "a case difference alone is not drift")
	equal(S.apn_reconcile, nil, "and nothing is reported as an active reconcile")
end

do
	-- A PDP-type-only difference is the network's own answer -- narrowing an
	-- attach context -- not the modem forgetting the APN, and is no longer
	-- force-corrected; only logged.
	fake_now = fake_now + 700
	M.queue = {}
	S.apn_reconcile = nil
	apn.apn_state_set({ cid = 1, apn = "internet", pdp_type = "IPV4V6", auth = 0,
	                     sim_id = "8900000000000000009" })
	set_sim("8900000000000000009")
	set_live(1, "internet", "IPV4", 0)

	apn.apn_reconcile()
	equal(#M.queue, 0, "a PDP-type-only difference is not re-applied")
	equal(S.apn_reconcile, nil, "and is not reported as an active reconcile either")
end

do
	-- A genuine mismatch is re-applied once, then rate-limited: two calls in
	-- quick succession (well under the 10-minute floor) must not both fire. Past
	-- the floor it may retry again, up to the cap (3) -- and once there, it
	-- stops and says so instead of hammering the modem forever.
	fake_now = fake_now + 700
	M.queue = {}
	S.apn_reconcile = nil
	apn.apn_state_set({ cid = 1, apn = "good-apn", pdp_type = "IPV4V6", auth = 0,
	                     sim_id = "8900000000000000005" })
	set_sim("8900000000000000005")
	set_live(1, "operator-default", "IPV4V6", 0)

	apn.apn_reconcile()
	equal(#M.queue, 1, "the first genuine mismatch is re-applied")
	apn.apn_reconcile()
	equal(#M.queue, 1, "a second call seconds later is rate-limited, not re-applied again")

	fake_now = fake_now + 601
	apn.apn_reconcile()
	equal(#M.queue, 2, "past the 10-minute floor, a persistent mismatch retries")
	fake_now = fake_now + 601
	apn.apn_reconcile()
	equal(#M.queue, 3, "and a third time, at the cap")
	equal(S.apn_reconcile.tries, 3, "three tries recorded")
	fake_now = fake_now + 601
	apn.apn_reconcile()
	equal(#M.queue, 3, "a fourth attempt is refused: the cap is reached")
	equal(S.apn_reconcile.state, "given_up", "and reported as given up, not silently ignored")

	-- Convergence from elsewhere (a later user save, the network itself) still
	-- clears the given-up state -- it must not stay reported as failed forever.
	set_live(1, "good-apn", "IPV4V6", 0)
	apn.apn_reconcile()
	equal(S.apn_reconcile, nil, "convergence after giving up still clears the state")
end

do
	-- The context itself being gone (deleted, or simply not this cid) is left
	-- alone rather than compared against nothing.
	fake_now = fake_now + 700
	M.queue = {}
	S.apn_reconcile = nil
	apn.apn_state_set({ cid = 9, apn = "good-apn", pdp_type = "IPV4V6", auth = 0,
	                     sim_id = "8900000000000000005" })
	set_sim("8900000000000000005")
	S.apn = { count = 0, list = {} }

	apn.apn_reconcile()
	equal(#M.queue, 0, "a context that no longer exists is left alone")
end

print(("apn reconciler tests: %d assertions passed"):format(assertions))
