#!/usr/bin/lua
--[[
A four-operation HTTP API for the Xray connection, for automation and for a phone app
that does not want to drive a web page.

    GET /cgi-bin/xray-api?action=profiles&token=...
    GET /cgi-bin/xray-api?action=activate&id=p2&token=...
    GET /cgi-bin/xray-api?action=connect&token=...      (starts a job)
    GET /cgi-bin/xray-api?action=disconnect&token=...

and two that cost nothing to add because the answers already exist:

    GET /cgi-bin/xray-api?action=status&token=...
    GET /cgi-bin/xray-api?action=job&token=...          (how the connect is going)

POST works too, with the same parameters in the body.  The token may also be sent as
an `X-Xray-Token:` header, which keeps it out of the web server's access log.

It answers 403 until `api_enabled` is set and a token exists -- both are one switch on
the page.  There is no TLS on this router's web server, so the token is only as private
as the LAN it crosses; that is stated on the page rather than hidden here.
]]

package.path = "/usr/lib/lua/?.lua;" .. package.path

local json = require "luci.jsonc"
local X = require "hh71vm.xray"

-- uhttpd passes the Status line through verbatim, and it needs the reason phrase:
-- "Status: 403" alone comes out of the server as 200, which would make a rejected
-- request look like an accepted one to anything that checks the code.
local REASON = { [200] = "OK", [400] = "Bad Request", [403] = "Forbidden",
                 [404] = "Not Found", [500] = "Internal Server Error" }

local function reply(code, body)
	io.write("Status: " .. code .. " " .. (REASON[code] or "OK") .. "\r\n")
	io.write("Content-Type: application/json\r\n")
	io.write("Cache-Control: no-store\r\n\r\n")
	io.write(json.stringify(body), "\n")
	os.exit(0)
end

local function urldecode(s)
	s = tostring(s or ""):gsub("+", " ")
	return (s:gsub("%%(%x%x)", function (h) return string.char(tonumber(h, 16)) end))
end

local function parse_query(q)
	local t = {}
	for pair in tostring(q or ""):gmatch("[^&]+") do
		local k, v = pair:match("^([^=]+)=?(.*)$")
		if k then t[urldecode(k)] = urldecode(v) end
	end
	return t
end

local args = parse_query(os.getenv("QUERY_STRING"))
if (os.getenv("REQUEST_METHOD") or "") == "POST" then
	local len = tonumber(os.getenv("CONTENT_LENGTH") or "0") or 0
	if len > 0 and len < 65536 then
		local body = io.read(len) or ""
		for k, v in pairs(parse_query(body)) do args[k] = v end
	end
end

local s = X.settings()
local token = args.token or os.getenv("HTTP_X_XRAY_TOKEN") or ""

if not X.bool(s.api_enabled) then
	reply(403, { ok = false, error = "the HTTP API is switched off" })
end
if X.trim(s.api_token) == "" then
	reply(403, { ok = false, error = "no API token is set" })
end
if token ~= s.api_token then
	reply(403, { ok = false, error = "bad token" })
end

local action = args.action or "status"

if action == "profiles" then
	local store = X.load_profiles()
	local list = {}
	for _, p in ipairs(store.profiles or {}) do
		list[#list + 1] = {
			id = p.id, name = p.name, protocol = p.protocol,
			address = p.address, port = p.port,
			tls = p.tls, transport = p.transport,
			active = (p.id == store.active)
		}
	end
	reply(200, { ok = true, active = store.active, profiles = list })

elseif action == "activate" then
	local store = X.load_profiles()
	local p = X.find_profile(store, args.id or "")
	if not p then reply(404, { ok = false, error = "no such profile" }) end
	store.active = p.id
	X.save_profiles(store)
	-- If the connection is up, move it to the new profile rather than leaving the
	-- page and the tunnel disagreeing about which server is in use.
	if X.bool(s.enabled) then
		os.execute("/usr/sbin/hh71vm-xrayctl connect --job >/dev/null 2>&1 &")
		reply(200, { ok = true, active = p.id, reconnecting = true })
	end
	reply(200, { ok = true, active = p.id, reconnecting = false })

elseif action == "connect" then
	local out = X.popen("/usr/sbin/hh71vm-xrayctl connect --job")
	reply(200, { ok = true, started = true, raw = X.trim(out) })

elseif action == "disconnect" then
	local out = X.popen("/usr/sbin/hh71vm-xrayctl disconnect")
	local t = json.parse(out) or {}
	reply(200, { ok = t.ok ~= false, result = t })

elseif action == "job" then
	local out = X.popen("/usr/sbin/hh71vm-xrayctl job")
	reply(200, json.parse(out) or { state = "unknown" })

elseif action == "status" then
	local out = X.popen("/usr/sbin/hh71vm-xrayctl status")
	reply(200, json.parse(out) or { ok = false, error = "no status" })

else
	reply(400, { ok = false, error = "unknown action",
	             actions = { "profiles", "activate", "connect", "disconnect",
	                         "status", "job" } })
end
