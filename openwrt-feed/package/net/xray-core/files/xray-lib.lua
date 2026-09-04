--[[
hh71vm.xray -- everything the Xray page needs that has to run on the router.

  * the profile store (/etc/xray/profiles.json) and the service settings (UCI xray.main)
  * turning one profile into a working /etc/xray/config.json, in either mode
  * the connectivity probe, which is what tells "the process started" from "it works"
  * the error table, which turns Xray's own wording into something actionable

Kept in one place because the rpcd plugin, the command line tool, the watchdog and the
HTTP API all need the same answers, and three copies of the config generator would drift
apart within a week.

Lua 5.1 (OpenWrt 19.07). luci.jsonc and nixio are in the image; the uci Lua binding is
not, so settings are read through the `uci` command line tool.
]]

local json  = require "luci.jsonc"
local nixio = require "nixio"

local M = {}

M.PROFILES   = "/etc/xray/profiles.json"
M.CONFIG     = "/etc/xray/config.json"
M.LOGFILE    = "/var/log/xray.log"
M.STATEFILE  = "/var/run/xray.state.json"
M.STATE_SH   = "/var/run/xray.state.sh"
M.PIDFILE    = "/var/run/xray.pid"
M.LAUNCHER   = "/usr/sbin/hh71vm-xray"

-- Private destinations never go through the tunnel.  geoip:private would be the usual
-- way to say this and it is not available here: geoip.dat is 20 MB and the share has
-- 21 MB free, so the list is written out instead.  It has to stay in step with the one
-- in hh71vm-xray-fw.
M.PRIVATE_V4 = {
	"0.0.0.0/8", "10.0.0.0/8", "100.64.0.0/10", "127.0.0.0/8", "169.254.0.0/16",
	"172.16.0.0/12", "192.0.0.0/24", "192.168.0.0/16", "198.18.0.0/15",
	"224.0.0.0/4", "240.0.0.0/4"
}
M.PRIVATE_V6 = { "::1/128", "fc00::/7", "fe80::/10" }

M.DEFAULTS = {
	enabled          = "0",
	autostart        = "0",
	watchdog         = "0",
	watchdog_period  = "60",
	watchdog_fails   = "2",
	mode             = "vpn",
	binary           = "",
	config           = M.CONFIG,
	assets           = "/mnt/extern/xray",
	wait             = "120",
	socks_port       = "1080",
	http_port        = "1081",
	redirect_port    = "1082",
	tproxy_port      = "1083",
	dns_port         = "1053",
	dns_server       = "1.1.1.1",
	-- empty means "work it out": every interface except the way out
	lan_ifaces       = "",
	router_traffic   = "1",
	block_quic       = "1",
	capture_udp      = "0",
	block_ipv6       = "1",
	sniffing         = "1",
	loglevel         = "warning",
	probe_url        = "http://cp.cloudflare.com/generate_204",
	probe_timeout    = "12",
	fwmark           = "255",
	tproxy_mark      = "1",
	tproxy_table     = "100",
	api_enabled      = "0",
	api_token        = "",
	set_clock        = "1"
}

-- ------------------------------------------------------------------ small helpers

local function trim(s)
	return (tostring(s or ""):gsub("^%s+", ""):gsub("%s+$", ""))
end
M.trim = trim

local function shq(s)             -- single-quote for /bin/sh
	return "'" .. tostring(s or ""):gsub("'", "'\\''") .. "'"
end
M.shq = shq

local function readfile(path)
	local f = io.open(path, "r")
	if not f then return nil end
	local d = f:read("*a")
	f:close()
	return d
end
M.readfile = readfile

local function writefile(path, data)
	local tmp = path .. ".tmp"
	local f, err = io.open(tmp, "w")
	if not f then return nil, err end
	f:write(data)
	f:close()
	local ok, e = os.rename(tmp, path)
	if not ok then return nil, e end
	return true
end
M.writefile = writefile

local function popen(cmd)
	local p = io.popen(cmd .. " 2>&1", "r")
	if not p then return "" end
	local out = p:read("*a") or ""
	p:close()
	return out
end
M.popen = popen

--- Wall clock in milliseconds.  os.clock() is CPU time and measures nothing useful
--- for a network round trip.
function M.now_ms()
	local ok, s, us = pcall(nixio.gettimeofday)
	if ok and type(s) == "table" then
		return s.sec * 1000 + math.floor(s.usec / 1000)
	elseif ok and type(s) == "number" then
		return s * 1000 + math.floor((us or 0) / 1000)
	end
	return os.time() * 1000
end

function M.isnum(v, lo, hi)
	local n = tonumber(v)
	if not n then return nil end
	n = math.floor(n)
	if lo and n < lo then return nil end
	if hi and n > hi then return nil end
	return n
end

-- ---------------------------------------------------------------------- settings

--- Read xray.main into a plain table, filling in every default.
function M.settings()
	local s = {}
	for k, v in pairs(M.DEFAULTS) do s[k] = v end
	local out = popen("/sbin/uci -q show xray.main")
	for line in out:gmatch("[^\n]+") do
		local key, val = line:match("^xray%.main%.([%w_]+)=(.*)$")
		if key then
			val = val:gsub("^'", ""):gsub("'$", ""):gsub("'\\''", "'")
			s[key] = val
		end
	end
	return s
end

function M.set_settings(kv)
	local parts = {}
	for k, v in pairs(kv) do
		if M.DEFAULTS[k] == nil then return nil, "unknown setting: " .. tostring(k) end
		parts[#parts + 1] = "/sbin/uci -q set xray.main." .. k .. "=" .. shq(v)
	end
	if #parts == 0 then return true end
	parts[#parts + 1] = "/sbin/uci -q commit xray"
	local out = popen(table.concat(parts, " && "))
	return true, out
end

function M.bool(v)
	v = tostring(v or "")
	return v == "1" or v == "true" or v == "yes" or v == "on"
end

-- ---------------------------------------------------------------------- profiles

local EMPTY_STORE = { version = 1, active = "", profiles = {} }

function M.load_profiles()
	local raw = readfile(M.PROFILES)
	if not raw or trim(raw) == "" then return { version = 1, active = "", profiles = {} } end
	local t = json.parse(raw)
	if type(t) ~= "table" then return { version = 1, active = "", profiles = {} } end
	if type(t.profiles) ~= "table" then t.profiles = {} end
	t.active = t.active or ""
	t.version = t.version or 1
	return t
end

function M.save_profiles(store)
	store.version = 1
	if type(store.profiles) ~= "table" then store.profiles = {} end
	-- an empty profile list has to stay an array, and luci.jsonc writes an empty table
	-- as {}, which the next load would still accept but which reads wrong in a file a
	-- human may open.
	local body = json.stringify(store, true)
	if #store.profiles == 0 then
		body = body:gsub('"profiles"%s*:%s*{%s*}', '"profiles": []')
	end
	return writefile(M.PROFILES, body .. "\n")
end

function M.find_profile(store, id)
	for i, p in ipairs(store.profiles or {}) do
		if p.id == id then return p, i end
	end
	return nil
end

function M.active_profile(store)
	store = store or M.load_profiles()
	local p = M.find_profile(store, store.active)
	if p then return p end
	return (store.profiles or {})[1]
end

--- Profile ids are generated here, not in the browser: two tabs open at once would
--- otherwise be able to produce the same one.
function M.new_id(store)
	local n = 1
	while true do
		local id = "p" .. n
		if not M.find_profile(store, id) then return id end
		n = n + 1
	end
end

-- The fields a profile may carry.  Anything else is dropped on the way in, so a broken
-- or hostile client cannot smuggle arbitrary keys into the generated configuration.
M.FIELDS = {
	id = "s", name = "s", protocol = "s", address = "s", port = "n",
	uuid = "s", password = "s", method = "s", security = "s", alterId = "n",
	flow = "s", encryption = "s",
	transport = "s", path = "s", host = "s", serviceName = "s", grpcMode = "s",
	headerType = "s", seed = "s", quicSecurity = "s", quicKey = "s", authority = "s",
	tls = "s", sni = "s", alpn = "s", fingerprint = "s", allowInsecure = "b",
	publicKey = "s", shortId = "s", spiderX = "s",
	mux = "b", muxConcurrency = "n",
	note = "s"
}

function M.clean_profile(p)
	local out = {}
	if type(p) ~= "table" then return out end
	for k, kind in pairs(M.FIELDS) do
		local v = p[k]
		if v ~= nil then
			if kind == "s" then
				out[k] = tostring(v)
			elseif kind == "n" then
				out[k] = tonumber(v) or 0
			else
				out[k] = M.bool(v) and true or false
			end
		end
	end
	return out
end

-- What a profile has to have before it can possibly connect.  Returned as a list of
-- human sentences, because "invalid profile" helps nobody.
function M.validate_profile(p)
	local errs = {}
	local proto = p.protocol or ""
	if trim(p.name or "") == "" then errs[#errs + 1] = "The profile needs a name." end
	if trim(p.address or "") == "" then errs[#errs + 1] = "The server address is empty." end
	if not M.isnum(p.port, 1, 65535) then errs[#errs + 1] = "The port must be 1 to 65535." end

	if proto == "vless" or proto == "vmess" then
		if trim(p.uuid or "") == "" then
			errs[#errs + 1] = "This protocol needs a user id (UUID)."
		end
	elseif proto == "trojan" then
		if trim(p.password or "") == "" then errs[#errs + 1] = "Trojan needs a password." end
	elseif proto == "shadowsocks" then
		if trim(p.password or "") == "" then errs[#errs + 1] = "Shadowsocks needs a password." end
		if trim(p.method or "") == "" then errs[#errs + 1] = "Shadowsocks needs an encryption method." end
	else
		errs[#errs + 1] = "Unknown protocol: " .. tostring(proto)
	end

	if p.tls == "reality" then
		if trim(p.publicKey or "") == "" then
			errs[#errs + 1] = "REALITY needs the server's public key (pbk)."
		end
		if trim(p.sni or "") == "" then
			errs[#errs + 1] = "REALITY needs a server name (sni), the site the handshake pretends to be."
		end
		if proto ~= "vless" then
			errs[#errs + 1] = "REALITY only exists for VLESS."
		end
	end
	if trim(p.flow or "") ~= "" and proto ~= "vless" then
		errs[#errs + 1] = "The flow field only exists for VLESS."
	end
	if p.transport == "grpc" and trim(p.serviceName or "") == "" then
		errs[#errs + 1] = "gRPC needs a service name."
	end
	return errs
end

-- ---------------------------------------------------------------- name resolution

--- Resolve a host name to an IPv4 address.
--- Xray is given the address literally in the generated config, which keeps it from
--- needing DNS at all -- and that matters here, because in VPN mode the router's own
--- DNS is redirected *into* Xray.  A server named by domain would otherwise deadlock
--- at startup: Xray asks dnsmasq, dnsmasq's upstream query is redirected into Xray,
--- and Xray is not up yet.
function M.resolve(host)
	if not host or host == "" then return nil, "empty address" end
	if host:match("^%d+%.%d+%.%d+%.%d+$") then return host end
	if host:match("^%[?[%x:]+%]?$") and host:find(":") then
		return (host:gsub("[%[%]]", ""))
	end
	local ok, res = pcall(nixio.getaddrinfo, host, "inet")
	if ok and type(res) == "table" then
		for _, e in ipairs(res) do
			if e.address then return e.address end
		end
	end
	-- getaddrinfo goes through the C library, which on this image reads only
	-- /etc/resolv.conf; nslookup is the second opinion and costs nothing here.
	local out = popen("/usr/bin/nslookup " .. shq(host))
	local last
	for line in out:gmatch("[^\n]+") do
		local a = line:match("^Address%s+%d*:?%s*(%d+%.%d+%.%d+%.%d+)")
		if a and a ~= "127.0.0.1" then last = a end
	end
	if last then return last end
	return nil, "cannot resolve " .. host
end

-- ------------------------------------------------------------ config generation

local function stream_settings(p, mark)
	local st = {}
	local net = p.transport or "tcp"
	if net == "h2" then net = "http" end
	st.network = net

	local sec = p.tls or "none"
	st.security = (sec == "none") and "none" or sec

	if sec == "tls" then
		local t = {}
		if trim(p.sni or "") ~= "" then t.serverName = p.sni end
		if trim(p.fingerprint or "") ~= "" then t.fingerprint = p.fingerprint end
		if trim(p.alpn or "") ~= "" then
			local list = {}
			for a in tostring(p.alpn):gmatch("[^,%s]+") do list[#list + 1] = a end
			if #list > 0 then t.alpn = list end
		end
		if p.allowInsecure then t.allowInsecure = true end
		st.tlsSettings = t
	elseif sec == "reality" then
		local r = {
			serverName = p.sni or "",
			publicKey  = p.publicKey or "",
			shortId    = p.shortId or "",
			spiderX    = (trim(p.spiderX or "") ~= "") and p.spiderX or "/"
		}
		if trim(p.fingerprint or "") ~= "" then r.fingerprint = p.fingerprint
		else r.fingerprint = "chrome" end
		st.realitySettings = r
	end

	if net == "ws" then
		local w = { path = (trim(p.path or "") ~= "") and p.path or "/" }
		if trim(p.host or "") ~= "" then w.headers = { Host = p.host } end
		st.wsSettings = w
	elseif net == "httpupgrade" then
		local w = { path = (trim(p.path or "") ~= "") and p.path or "/" }
		if trim(p.host or "") ~= "" then w.host = p.host end
		st.httpupgradeSettings = w
	elseif net == "grpc" then
		local g = { serviceName = p.serviceName or "" }
		if p.grpcMode == "multi" then g.multiMode = true end
		if trim(p.authority or "") ~= "" then g.authority = p.authority end
		st.grpcSettings = g
	elseif net == "http" then
		local h = { path = (trim(p.path or "") ~= "") and p.path or "/" }
		if trim(p.host or "") ~= "" then
			local hosts = {}
			for a in tostring(p.host):gmatch("[^,%s]+") do hosts[#hosts + 1] = a end
			if #hosts > 0 then h.host = hosts end
		end
		st.httpSettings = h
	elseif net == "kcp" then
		local k = { header = { type = (trim(p.headerType or "") ~= "") and p.headerType or "none" } }
		if trim(p.seed or "") ~= "" then k.seed = p.seed end
		st.kcpSettings = k
	elseif net == "tcp" and p.headerType == "http" then
		local req = { version = "1.1", method = "GET",
		              path = { (trim(p.path or "") ~= "") and p.path or "/" },
		              headers = {} }
		if trim(p.host or "") ~= "" then
			local hosts = {}
			for a in tostring(p.host):gmatch("[^,%s]+") do hosts[#hosts + 1] = a end
			req.headers.Host = hosts
		end
		st.tcpSettings = { header = { type = "http", request = req } }
	end

	-- The socket mark is what keeps VPN mode from eating its own tail: the firewall
	-- returns anything carrying it before the redirect rules can see it.
	st.sockopt = { mark = mark }
	return st
end

--- Build the proxy outbound for one profile.
--- `addr` overrides the address (used to pass the resolved IP), `mark` is the socket
--- mark.  Returns the outbound table.
function M.outbound(p, addr, mark)
	local address = addr or p.address
	local port = M.isnum(p.port, 1, 65535) or 443
	local o = { tag = "proxy", protocol = p.protocol, settings = {} }

	if p.protocol == "vless" then
		local user = { id = p.uuid, encryption = "none", level = 0 }
		if trim(p.flow or "") ~= "" then user.flow = p.flow end
		o.settings.vnext = { { address = address, port = port, users = { user } } }
	elseif p.protocol == "vmess" then
		local user = {
			id = p.uuid, level = 0,
			alterId = M.isnum(p.alterId, 0, 65535) or 0,
			security = (trim(p.security or "") ~= "") and p.security or "auto"
		}
		o.settings.vnext = { { address = address, port = port, users = { user } } }
	elseif p.protocol == "trojan" then
		local s = { address = address, port = port, password = p.password, level = 0 }
		if trim(p.flow or "") ~= "" then s.flow = p.flow end
		o.settings.servers = { s }
	elseif p.protocol == "shadowsocks" then
		o.settings.servers = { {
			address = address, port = port, method = p.method,
			password = p.password, level = 0, uot = false
		} }
	else
		return nil, "unknown protocol " .. tostring(p.protocol)
	end

	o.streamSettings = stream_settings(p, mark)
	if p.mux then
		o.mux = { enabled = true, concurrency = M.isnum(p.muxConcurrency, 1, 1024) or 8 }
	end
	return o
end

--- Build the whole configuration for one profile.
--- Returns config table, meta table.  meta.resolved is the server IP the firewall has
--- to exempt; meta.warnings is a list of sentences worth showing on the page.
function M.build_config(p, s, opts)
	s = s or M.settings()
	opts = opts or {}
	local warnings = {}

	local mark = M.isnum(s.fwmark, 1, 4294967295) or 255
	local resolved, rerr = M.resolve(p.address)
	if not resolved then
		if opts.allow_unresolved then
			warnings[#warnings + 1] =
				"The server address could not be resolved (" .. tostring(rerr) ..
				"); the name is passed to Xray as it is."
		else
			return nil, nil, rerr
		end
	end

	local pp = {}
	for k, v in pairs(p) do pp[k] = v end
	-- If the address is a name and we resolved it, keep the name as the TLS server
	-- name unless the profile already sets one: substituting the IP would otherwise
	-- change what the handshake claims to be.
	if resolved and resolved ~= p.address then
		if trim(pp.sni or "") == "" and (pp.tls == "tls") then pp.sni = p.address end
	end

	local out, oerr = M.outbound(pp, resolved, mark)
	if not out then return nil, nil, oerr end

	local cfg = {
		log = {
			loglevel = s.loglevel or "warning",
			error = M.LOGFILE
		},
		inbounds = {},
		outbounds = {
			out,
			{ tag = "direct", protocol = "freedom", settings = { domainStrategy = "UseIP" },
			  streamSettings = { sockopt = { mark = mark } } },
			{ tag = "block", protocol = "blackhole",
			  settings = { response = { type = "none" } } }
		},
		routing = { domainStrategy = "AsIs", rules = {} }
	}

	-- A *new* table each time, not one shared reference: luci.jsonc writes the second
	-- and every later appearance of the same table as null, and an inbound whose
	-- sniffing is null loses the host name -- which in a transparent setup is the
	-- difference between working TLS and a handshake to a bare IP address.
	local function sniff()
		if not M.bool(s.sniffing) then return nil end
		return { enabled = true, destOverride = { "http", "tls", "quic" },
		         routeOnly = false }
	end

	local function add_in(t) cfg.inbounds[#cfg.inbounds + 1] = t end

	-- SOCKS and HTTP exist in both modes.  They are the whole of proxy mode, and in
	-- VPN mode they are what the probe and the API talk to, and what a client that
	-- wants to bypass the transparent path can still use.
	add_in({
		tag = "socks-in", protocol = "socks", listen = "0.0.0.0",
		port = M.isnum(s.socks_port, 1, 65535) or 1080,
		settings = { auth = "noauth", udp = true, ip = "127.0.0.1" },
		sniffing = sniff()
	})
	add_in({
		tag = "http-in", protocol = "http", listen = "0.0.0.0",
		port = M.isnum(s.http_port, 1, 65535) or 1081,
		settings = { allowTransparent = false },
		sniffing = sniff()
	})

	local mode = s.mode or "vpn"
	if mode == "vpn" then
		add_in({
			tag = "redirect-in", protocol = "dokodemo-door", listen = "0.0.0.0",
			port = M.isnum(s.redirect_port, 1, 65535) or 1082,
			settings = { network = "tcp", followRedirect = true },
			sniffing = sniff()
		})
		if opts.tproxy ~= false then
			add_in({
				tag = "tproxy-in", protocol = "dokodemo-door", listen = "0.0.0.0",
				port = M.isnum(s.tproxy_port, 1, 65535) or 1083,
				settings = { network = "udp", followRedirect = true },
				streamSettings = { sockopt = { tproxy = "tproxy" } },
				sniffing = sniff()
			})
		end
		add_in({
			tag = "dns-in", protocol = "dokodemo-door", listen = "0.0.0.0",
			port = M.isnum(s.dns_port, 1, 65535) or 1053,
			settings = {
				network = "tcp,udp",
				address = (trim(s.dns_server or "") ~= "") and s.dns_server or "1.1.1.1",
				port = 53
			}
		})
	end

	-- Routing.  Order matters: the server's own address and every private
	-- destination leave through `direct`, everything else through `proxy`.
	local rules = cfg.routing.rules
	if resolved then
		rules[#rules + 1] = { type = "field", ip = { resolved .. "/32" }, outboundTag = "direct" }
	end
	if trim(p.address or "") ~= "" and not p.address:match("^%d+%.%d+%.%d+%.%d+$") then
		rules[#rules + 1] = { type = "field", domain = { "full:" .. p.address }, outboundTag = "direct" }
	end
	local priv = {}
	for _, c in ipairs(M.PRIVATE_V4) do priv[#priv + 1] = c end
	for _, c in ipairs(M.PRIVATE_V6) do priv[#priv + 1] = c end
	rules[#rules + 1] = { type = "field", ip = priv, outboundTag = "direct" }
	rules[#rules + 1] = { type = "field", network = "tcp,udp", outboundTag = "proxy" }

	local meta = {
		resolved = resolved,
		warnings = warnings,
		mode = mode,
		ports = {
			socks = M.isnum(s.socks_port) or 1080,
			http = M.isnum(s.http_port) or 1081,
			redirect = M.isnum(s.redirect_port) or 1082,
			tproxy = M.isnum(s.tproxy_port) or 1083,
			dns = M.isnum(s.dns_port) or 1053
		}
	}
	return cfg, meta
end

--- Generate and write /etc/xray/config.json for the active profile.
function M.generate(opts)
	opts = opts or {}
	local s = M.settings()
	local store = M.load_profiles()
	local p = opts.profile or M.active_profile(store)
	if not p then return nil, "no profile is configured" end

	local errs = M.validate_profile(p)
	if #errs > 0 then return nil, table.concat(errs, " ") end

	local cfg, meta, err = M.build_config(p, s, opts)
	if not cfg then return nil, err end

	local ok, werr = writefile(s.config or M.CONFIG, json.stringify(cfg, true) .. "\n")
	if not ok then return nil, "cannot write the configuration: " .. tostring(werr) end

	writefile(M.STATEFILE, json.stringify({
		profile = p.id, name = p.name, resolved = meta.resolved,
		mode = meta.mode, ports = meta.ports, generated = os.time()
	}) .. "\n")
	-- The firewall script is shell and has no JSON parser; it needs one value from
	-- here, and getting it wrong means the tunnel eats its own traffic.
	writefile(M.STATE_SH, string.format(
		"XRAY_SERVER_IP=%s\nXRAY_PROFILE=%s\nXRAY_MODE=%s\n",
		shq(meta.resolved or ""), shq(p.id or ""), shq(meta.mode or "")))
	return meta
end

function M.state()
	local raw = readfile(M.STATEFILE)
	if not raw then return {} end
	local t = json.parse(raw)
	return (type(t) == "table") and t or {}
end

-- ------------------------------------------------------------------ error table

-- Xray names the wrong thing often enough that a plain "failed" is worthless.  Each
-- entry is { pattern, title, what it really is, what to do }.
M.ERRORS = {
	{ "invalid user.*timestamp",
	  "The router's clock is wrong",
	  "VMess authenticates with a timestamp and refuses anything more than 90 seconds out. It reports that as an invalid user, which is the wrong thing to blame.",
	  "Set the clock (System -> System, or `date -u -s`). This board has no working NTP of its own, so the clock has to come from somewhere every boot." },
	{ "invalid user",
	  "The server rejected the user id",
	  "Either the id really is wrong, or the router's clock is far enough out that VMess refuses the handshake and blames the user.",
	  "Check the id against the server, then check the clock." },
	{ "not able to use the flow",
	  "The server does not want the flow this profile sets",
	  "The profile asks for xtls-rprx-vision and the server's user is not pinned to it.",
	  "Clear the Flow field in the profile." },
	{ "client flow is empty",
	  "The server requires xtls-rprx-vision and this profile does not set it",
	  "The server pins the flow on its user; a client that does not set the same flow is rejected.",
	  "Set Flow to xtls-rprx-vision in the profile." },
	{ "REALITY",
	  "The REALITY handshake failed",
	  "One of the four REALITY fields does not match the server, or the site the server borrows its certificate from is unreachable from the server.",
	  "Check the public key (pbk), the short id (sid) and the server name (sni) against the server's own configuration." },
	{ "x509: certificate signed by unknown authority",
	  "The server's certificate is not trusted",
	  "The TLS certificate does not chain to any CA the router knows.",
	  "For a self-signed certificate, tick 'Accept an untrusted certificate' in the profile. Otherwise the server name is probably wrong." },
	{ "certificate is valid for",
	  "The certificate does not match the server name",
	  "The TLS certificate is for a different host than the one the profile asks for.",
	  "Correct the SNI field." },
	{ "handshake failure",
	  "The TLS handshake failed",
	  "The server answered but refused the TLS parameters -- usually a wrong server name, a wrong ALPN or the wrong security type for this port.",
	  "Check TLS type, SNI and ALPN against the server." },
	{ "connection refused",
	  "The server refused the connection",
	  "Something answered at that address and closed the port. Usually the wrong port, or the service is not running.",
	  "Check the port." },
	{ "i/o timeout",
	  "The server did not answer",
	  "The packets left the router and nothing came back within the timeout.",
	  "Check that the router itself has working internet, and that the address and port are right. On this bench the modem has no data, so nothing outside the LAN answers." },
	{ "context deadline exceeded",
	  "The connection timed out",
	  "The tunnel was still being set up when the deadline ran out.",
	  "Usually the same as no answer at all: check the address, the port, and the router's own connectivity." },
	{ "no such host",
	  "The server name could not be resolved",
	  "DNS did not answer for the address in the profile.",
	  "Check the address for a typo, and that the router can resolve names at all." },
	{ "failed to load geoip",
	  "A routing rule needs geoip.dat and it is not installed",
	  "The share has about 21 MB free and the two geo files are about 20 MB together, so they are deliberately not shipped.",
	  "Do not use geoip: or geosite: rules on this board, or put the files on the share yourself." },
	{ "failed to load geosite",
	  "A routing rule needs geosite.dat and it is not installed",
	  "The share has about 21 MB free and the two geo files are about 20 MB together, so they are deliberately not shipped.",
	  "Do not use geoip: or geosite: rules on this board, or put the files on the share yourself." },
	{ "address already in use",
	  "One of the local ports is taken",
	  "Another program on the router is already listening on a port this configuration wants.",
	  "Change the port in Settings, or stop whatever holds it." },
	{ "no such file or directory",
	  "Something the configuration names is missing",
	  "Usually the binary or the asset directory.",
	  "Check that xray-core is installed on the share and that the share is mounted." },
	{ "closed pipe",
	  "The server closed the connection",
	  "The tunnel was established and the server then dropped it without saying why. From the client side that is what a rejected user looks like: a flow that does not match, a wrong id, or REALITY fields the server does not recognise all end this way.",
	  "Check the Flow field first - it has to match the server exactly, and both setting it when the server does not pin it and leaving it empty when the server does are rejected. Then the id, then the REALITY public key and short id." },
	{ "unexpected EOF",
	  "The server closed the connection",
	  "The tunnel was established and the server then dropped it without saying why. From the client side that is what a rejected user looks like: a flow that does not match, a wrong id, or REALITY fields the server does not recognise all end this way.",
	  "Check the Flow field first - it has to match the server exactly, and both setting it when the server does not pin it and leaving it empty when the server does are rejected. Then the id, then the REALITY public key and short id." },
	{ "operation not permitted",
	  "The kernel refused a socket option",
	  "TPROXY needs the xt_TPROXY and xt_socket modules and root; without them the UDP inbound cannot start.",
	  "Install kmod-ipt-tproxy, or switch UDP interception off in Settings." }
}

--- The few lines of a log worth showing someone.
--- Xray at debug level writes a padding trace for every block it moves, so the last
--- forty lines are usually forty lines of nothing. Warnings and errors are what a
--- failure looks like, newest last, and five of them is plenty.
function M.significant_log(text, keep)
	keep = keep or 5
	local picked = {}
	for line in tostring(text or ""):gmatch("[^\n]+") do
		local l = line:lower()
		if (l:find("%[warning%]") or l:find("%[error%]") or l:find("failed") or
		    l:find("rejected") or l:find("invalid")) and not l:find("core: xray .* started") then
			picked[#picked + 1] = line
		end
	end
	if #picked == 0 then return trim(text) end
	local first = math.max(1, #picked - keep + 1)
	return table.concat(picked, "\n", first, #picked)
end

--- Turn a lump of Xray output into { title, detail, hint, raw }.
--- The match runs over the significant lines rather than the whole log: a debug trace
--- that happens to contain the word "failed" three hundred lines ago is not what this
--- failure is about.
function M.explain(text)
	text = tostring(text or "")
	local meat = M.significant_log(text)
	for _, e in ipairs(M.ERRORS) do
		if meat:lower():find(e[1]:lower()) or text:lower():find(e[1]:lower()) then
			return { title = e[2], detail = e[3], hint = e[4], raw = trim(meat) }
		end
	end
	if trim(meat) == "" then return nil end
	-- Nothing recognised: show the newest significant line, not the whole log. The
	-- full text is still one click away on the page.
	local last = meat:match("([^\n]+)$") or meat
	return { title = "Xray reported an error", detail = trim(last), hint = "",
	         raw = trim(meat) }
end

--- The last few lines of the Xray error log.
function M.log_tail(lines)
	lines = lines or 40
	local out = popen("tail -n " .. tonumber(lines) .. " " .. shq(M.LOGFILE))
	if trim(out) == "" or out:find("No such file") then
		out = popen("logread -e xray 2>/dev/null | tail -n " .. tonumber(lines))
	end
	return out
end

-- ------------------------------------------------------------------------ probe

local function http_through_proxy(host, port, url, timeout)
	local u_host, u_path = url:match("^https?://([^/]+)(.*)$")
	if not u_host then return nil, "the probe URL is not a URL: " .. tostring(url) end
	if u_path == "" then u_path = "/" end

	local sock = nixio.socket("inet", "stream")
	if not sock then return nil, "no socket" end
	sock:setopt("socket", "sndtimeo", timeout)
	sock:setopt("socket", "rcvtimeo", timeout)
	local ok, err = sock:connect(host, port)
	if not ok then
		sock:close()
		return nil, "the local proxy port " .. port .. " did not accept a connection (" ..
		            tostring(err) .. ")"
	end
	local req = ("GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: hh71vm-xray-probe\r\n" ..
	             "Connection: close\r\nCache-Control: no-cache\r\n\r\n")
	            :format(url, u_host)
	sock:write(req)
	local buf, deadline = "", os.time() + timeout
	while #buf < 8192 do
		local chunk = sock:read(2048)
		if not chunk or chunk == "" then break end
		buf = buf .. chunk
		if os.time() > deadline then break end
	end
	sock:close()
	if buf == "" then
		return nil, "the proxy accepted the connection and returned nothing"
	end
	return buf
end

--- The same request without the proxy.  Used only to learn the time before the tunnel
--- exists, which is the one thing that has to happen before VMess can work at all.
function M.http_direct(url, timeout)
	local host, port, path = url:match("^https?://([^/:]+):?(%d*)(.*)$")
	if not host then return nil, "the probe URL is not a URL" end
	port = tonumber(port) or 80
	if path == "" then path = "/" end
	local ip, err = M.resolve(host)
	if not ip then return nil, err end
	local sock = nixio.socket("inet", "stream")
	sock:setopt("socket", "sndtimeo", timeout or 8)
	sock:setopt("socket", "rcvtimeo", timeout or 8)
	local ok, cerr = sock:connect(ip, port)
	if not ok then sock:close(); return nil, tostring(cerr) end
	sock:write(("HEAD %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n")
	           :format(path, host))
	local buf = sock:read(2048) or ""
	sock:close()
	if buf == "" then return nil, "no answer" end
	return buf
end

--- Ask for the probe URL through the router's own HTTP inbound.
--- This is the difference between "the process is running" and "the tunnel works".
--- Returns { ok, status, ms, date, error, body }.
function M.probe(s)
	s = s or M.settings()
	local url = trim(s.probe_url or "")
	if url == "" then return { ok = false, error = "no probe URL is configured" } end
	local port = M.isnum(s.http_port, 1, 65535) or 1081
	local timeout = M.isnum(s.probe_timeout, 1, 60) or 12

	local t0 = M.now_ms()
	local resp, err = http_through_proxy("127.0.0.1", port, url, timeout)
	local ms = M.now_ms() - t0
	if not resp then
		return { ok = false, error = err, ms = ms }
	end
	local code = tonumber(resp:match("^HTTP/%d%.%d (%d%d%d)") or "")
	local date = resp:match("[Dd]ate: ([^\r\n]+)")
	if not code then
		return { ok = false, error = "the answer was not HTTP", ms = ms,
		         body = resp:sub(1, 200) }
	end
	-- Xray's HTTP inbound answers 502/503 itself when the tunnel cannot carry the
	-- request, so a code alone is not proof; anything 2xx/3xx is.
	local ok = (code >= 200 and code < 400)
	local r = { ok = ok, status = code, ms = ms, date = date }
	if not ok then
		r.error = "the proxy answered HTTP " .. code
		r.body = resp:sub(1, 400)
	end
	return r
end

-- ------------------------------------------------------------------------ clock

local MONTHS = { Jan = 1, Feb = 2, Mar = 3, Apr = 4, May = 5, Jun = 6,
                 Jul = 7, Aug = 8, Sep = 9, Oct = 10, Nov = 11, Dec = 12 }

--- Parse an HTTP Date header into a UTC timestamp.
function M.http_date(str)
	if not str then return nil end
	local d, mon, y, hh, mm, ss =
		str:match("(%d+) (%a+) (%d+) (%d+):(%d+):(%d+)")
	if not d then return nil end
	local m = MONTHS[mon]
	if not m then return nil end
	-- os.time interprets the table as local time; the board runs UTC, and the
	-- difference is corrected by comparing os.date("!*t") with os.date("*t").
	local t = os.time({ year = tonumber(y), month = m, day = tonumber(d),
	                    hour = tonumber(hh), min = tonumber(mm), sec = tonumber(ss),
	                    isdst = false })
	if not t then return nil end
	local now = os.time()
	local utc = os.time(os.date("!*t", now))
	return t + (now - utc)
end

--- If the probe came back with a Date header and the clock is more than 60 s out,
--- set it.  This is the one thing that makes VMess usable on a board with no NTP.
function M.sync_clock(date_header)
	local ts = M.http_date(date_header)
	if not ts then return nil, "no usable Date header" end
	local drift = ts - os.time()
	if math.abs(drift) < 60 then return false, drift end
	os.execute("/bin/date -u -s " .. shq(os.date("!%Y-%m-%d %H:%M:%S", ts)) .. " >/dev/null 2>&1")
	return true, drift
end

-- ------------------------------------------------------------------ service view

function M.pid()
	local raw = readfile(M.PIDFILE)
	local pid = tonumber(trim(raw or ""))
	if not pid then return nil end
	if nixio.fs.access("/proc/" .. pid .. "/stat") then return pid end
	return nil
end

function M.binary()
	local out = trim(popen(M.LAUNCHER .. " binary"))
	if out == "" or out:find("not found") then return nil end
	return out
end

function M.share_mounted()
	local m = readfile("/proc/mounts") or ""
	return m:find(" /mnt/extern cifs ") ~= nil
end

--- Is anything listening on this TCP port?
--- Both tables have to be read.  Go opens a dual-stack socket, so an inbound written
--- as `"listen": "0.0.0.0"` appears in /proc/net/tcp6 as [::]:port and **not** in
--- /proc/net/tcp at all -- which made a working Xray look like one that never opened
--- its ports.
function M.listening(port)
	local hex = string.format("%04X", port)
	for _, path in ipairs({ "/proc/net/tcp", "/proc/net/tcp6" }) do
		local out = readfile(path) or ""
		for line in out:gmatch("[^\n]+") do
			local laddr, st = line:match("^%s*%d+:%s+(%x+:%x+)%s+%x+:%x+%s+(%x%x)")
			if laddr and st == "0A" and laddr:sub(-4) == hex then return true end
		end
	end
	return false
end

function M.tproxy_available()
	-- Not `ls | find "xt_TPROXY"`: with stderr folded in, ls's own "No such file"
	-- message contains the very string being looked for, and the answer is then
	-- always yes.  Ask the shell for a verdict instead of reading its complaint.
	local out = popen("ls /lib/modules/*/xt_TPROXY.ko >/dev/null 2>&1 && echo present")
	if out:find("present") then return true end
	local mods = readfile("/proc/modules") or ""
	return mods:find("xt_TPROXY") ~= nil
end

return M
