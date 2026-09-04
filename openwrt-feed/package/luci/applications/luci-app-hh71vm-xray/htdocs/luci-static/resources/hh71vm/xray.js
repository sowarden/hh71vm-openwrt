'use strict';
'require baseclass';
'require rpc';
'require request';
'require ui';

/* Shared plumbing for the Xray page: the ubus calls, the URI parsers, and the few
 * widgets the page repeats.  Everything talks to the hh71vm-xray rpcd object, so the
 * session ACL in luci-app-hh71vm-xray.json is what actually grants access.
 *
 * The URI parsers live here rather than on the router on purpose: pasting a link is a
 * browser-side operation, the shapes change with fashion rather than with the router,
 * and a parser that runs in the page can be fixed without touching the firmware.
 */

function decl(method, params) {
	return rpc.declare({ object: 'hh71vm-xray', method: method, params: params });
}

/* rpc.js hard-codes a 20 second browser timeout.  Above it there is a ceiling we do
 * not control: LuCI's /admin/ubus proxy calls libubus with a 30 second timeout, so no
 * single RPC can outlive that.  Connecting is routinely slower than 30 s when it goes
 * wrong -- which is the case that matters -- so connect is a job and this is only for
 * the calls that are merely slow. */
function callLong(method, params, timeoutSec) {
	var url = rpc.getBaseURL() + '/hh71vm-xray.' + method;
	var msg = {
		jsonrpc: '2.0', id: Date.now() % 100000, method: 'call',
		params: [L.env.sessionid || '00000000000000000000000000000000',
		         'hh71vm-xray', method, params || {}]
	};
	return request.post(url, msg, {
		timeout: (timeoutSec || 40) * 1000, nobatch: true, credentials: true
	}).then(function (res) {
		if (!res.ok)
			throw new Error(_('The router answered with HTTP %d').format(res.status));
		var body = res.json();
		if (!L.isObject(body)) throw new Error(_('Malformed answer from the router'));
		if (L.isObject(body.error)) throw new Error(body.error.message || _('RPC error'));
		var r = body.result;
		if (Array.isArray(r)) {
			if (r[0] !== 0 && r.length < 2)
				throw new Error(_('ubus refused the call (code %d)').format(r[0]));
			return (r.length > 1) ? r[1] : {};
		}
		return r || {};
	});
}

var api = {
	status:      decl('status',   ['refresh']),
	settings:    decl('settings', ['all']),
	profiles:    decl('profiles', ['all']),
	job:         decl('job',      ['all']),
	fwStatus:    decl('fw_status', ['all']),
	log:         decl('log',      ['lines']),

	profileSave:     decl('profile_save',     ['json']),
	profileDelete:   decl('profile_delete',   ['id']),
	profileActivate: decl('profile_activate', ['id']),
	settingsSet:     decl('settings_set',     ['json']),
	connectStart:    decl('connect',          ['start']),
	apiTokenNew:     decl('api_token_new',    ['now']),

	/* these two can sit for a while: a probe waits out its own timeout and
	   disconnect waits for procd */
	probe:      function () { return callLong('probe', { now: true }, 28); },
	disconnect: function () { return callLong('disconnect', { now: true }, 28); },
	generate:   function () { return callLong('generate', { now: true }, 28); },
	test:       function () { return callLong('test', { now: true }, 28); },

	/* Start a connect job and poll it to the end.  onStep is called with the job
	   state so the dialog can show each step as it happens; that is most of what
	   makes a failure legible -- the step that failed is half the answer. */
	connect: function (onStep) {
		var self = this, deadline = Date.now() + 180000;

		function poll() {
			return new Promise(function (r) { window.setTimeout(r, 1200); })
				.then(function () { return self.job(true); })
				.then(function (j) {
					j = j || {};
					if (onStep) onStep(j);
					if (j.state !== 'running') return j;
					if (Date.now() > deadline)
						return { state: 'done', ok: false,
						         error: _('The router did not finish connecting in three minutes.') };
					return poll();
				});
		}

		return self.connectStart(true).then(function () { return poll(); });
	}
};

/* ------------------------------------------------------------------ URI parsing */

function b64decode(s) {
	s = String(s || '').replace(/-/g, '+').replace(/_/g, '/').replace(/\s+/g, '');
	while (s.length % 4) s += '=';
	var raw = atob(s);
	/* the remark in a vmess link is UTF-8 inside the base64 */
	try { return decodeURIComponent(escape(raw)); } catch (e) { return raw; }
}

function b64encode(s) {
	try { return btoa(unescape(encodeURIComponent(s))); } catch (e) { return btoa(s); }
}

function parseQuery(q) {
	var out = {};
	String(q || '').replace(/^\?/, '').split('&').forEach(function (kv) {
		if (!kv) return;
		var i = kv.indexOf('='), k, v;
		if (i < 0) { k = kv; v = ''; } else { k = kv.slice(0, i); v = kv.slice(i + 1); }
		try { out[decodeURIComponent(k)] = decodeURIComponent(v.replace(/\+/g, ' ')); }
		catch (e) { out[k] = v; }
	});
	return out;
}

function emptyProfile() {
	return {
		id: '', name: '', protocol: 'vless', address: '', port: 443,
		uuid: '', password: '', method: 'aes-256-gcm', security: 'auto', alterId: 0,
		flow: '', encryption: 'none',
		transport: 'tcp', path: '/', host: '', serviceName: '', grpcMode: 'gun',
		headerType: 'none', seed: '', authority: '',
		tls: 'none', sni: '', alpn: '', fingerprint: 'chrome', allowInsecure: false,
		publicKey: '', shortId: '', spiderX: '/',
		mux: false, muxConcurrency: 8, note: ''
	};
}

/* map the query parameters shared by vless, trojan and the URL form of vmess */
function applyStreamParams(p, q) {
	p.transport = (q.type || q.net || 'tcp').toLowerCase();
	if (p.transport === 'h2') p.transport = 'http';
	if (p.transport === 'raw') p.transport = 'tcp';

	var sec = (q.security || '').toLowerCase();
	if (sec === 'reality') p.tls = 'reality';
	else if (sec === 'tls' || sec === 'xtls') p.tls = 'tls';
	else p.tls = 'none';

	if (q.sni) p.sni = q.sni;
	else if (q.peer) p.sni = q.peer;
	if (q.alpn) p.alpn = q.alpn;
	if (q.fp) p.fingerprint = q.fp;
	if (q.flow) p.flow = q.flow;
	if (q.encryption) p.encryption = q.encryption;
	if (q.pbk) p.publicKey = q.pbk;
	if (q.sid) p.shortId = q.sid;
	if (q.spx) p.spiderX = q.spx;
	if (q.allowInsecure === '1' || q.allowInsecure === 'true' ||
	    q.insecure === '1' || q.insecure === 'true') p.allowInsecure = true;

	if (p.transport === 'ws' || p.transport === 'httpupgrade') {
		p.path = q.path || '/';
		p.host = q.host || '';
	} else if (p.transport === 'grpc') {
		p.serviceName = q.serviceName || q.servicename || '';
		p.grpcMode = (q.mode === 'multi') ? 'multi' : 'gun';
		if (q.authority) p.authority = q.authority;
	} else if (p.transport === 'http') {
		p.path = q.path || '/';
		p.host = q.host || '';
	} else if (p.transport === 'kcp') {
		p.headerType = q.headerType || 'none';
		p.seed = q.seed || '';
	} else if (p.transport === 'tcp') {
		p.headerType = q.headerType || 'none';
		if (p.headerType === 'http') {
			p.path = q.path || '/';
			p.host = q.host || '';
		}
	}
	return p;
}

/* "vless://uuid@host:port?params#name" and the trojan link, which has the same shape */
function parseUserinfoUri(uri, protocol) {
	var m = uri.match(/^[a-z]+:\/\/([^@]+)@([^\/?#]+)(\?[^#]*)?(#.*)?$/i);
	if (!m) throw new Error(_('The link is not in the expected form.'));
	var userinfo = m[1], hostport = m[2];
	var q = parseQuery(m[3] || '');
	var name = m[4] ? decodeURIComponent(m[4].slice(1)) : '';

	/* The port is optional and often left out: a REALITY link straight from a panel
	   is usually "vless://id@host?…" with 443 implied.  Rejecting those as malformed
	   was the first thing this parser got wrong. */
	var host, port;
	var v6 = hostport.match(/^\[([^\]]+)\](?::(\d+))?$/);
	if (v6) {
		host = v6[1];
		port = v6[2] ? parseInt(v6[2], 10) : 443;
	} else if (/:\d+$/.test(hostport)) {
		var hp = hostport.split(':');
		port = parseInt(hp.pop(), 10);
		host = hp.join(':');
	} else {
		host = hostport;
		port = 443;
	}
	if (!host || !(port > 0 && port < 65536))
		throw new Error(_('The link has no usable address and port.'));

	var p = emptyProfile();
	p.protocol = protocol;
	p.address = host;
	p.port = port;
	p.name = name || (host + ':' + port);

	try { userinfo = decodeURIComponent(userinfo); } catch (e) { /* leave as is */ }
	if (protocol === 'trojan') p.password = userinfo;
	else p.uuid = userinfo;

	applyStreamParams(p, q);

	/* A REALITY link without a public key is not a REALITY link; say so here rather
	   than let Xray fail later with something less obvious. */
	if (p.tls === 'reality' && !p.publicKey)
		throw new Error(_('The link says REALITY but carries no public key (pbk).'));
	return p;
}

/* "vmess://" is base64 of a JSON object in the v2rayN dialect; a few generators emit
   the URL form instead, so both are accepted. */
function parseVmessUri(uri) {
	var body = uri.replace(/^vmess:\/\//i, '');
	if (body.indexOf('@') >= 0 && body.indexOf('?') >= 0)
		return parseUserinfoUri(uri, 'vmess');

	var text;
	try { text = b64decode(body.split('#')[0]); }
	catch (e) { throw new Error(_('The vmess link is not valid base64.')); }
	var o;
	try { o = JSON.parse(text); }
	catch (e) { throw new Error(_('The vmess link does not contain a configuration.')); }

	var p = emptyProfile();
	p.protocol = 'vmess';
	p.address = o.add || '';
	p.port = parseInt(o.port, 10) || 443;
	p.uuid = o.id || '';
	p.alterId = parseInt(o.aid, 10) || 0;
	p.security = o.scy || 'auto';
	p.name = o.ps || (p.address + ':' + p.port);
	p.transport = (o.net || 'tcp').toLowerCase();
	if (p.transport === 'h2') p.transport = 'http';
	if (p.transport === 'raw') p.transport = 'tcp';
	p.headerType = o.type || 'none';
	p.host = o.host || '';
	p.path = o.path || '/';
	if (p.transport === 'grpc') p.serviceName = o.path || '';
	p.tls = (String(o.tls || '').toLowerCase() === 'tls') ? 'tls' : 'none';
	p.sni = o.sni || o.host || '';
	p.alpn = o.alpn || '';
	if (o.fp) p.fingerprint = o.fp;
	return p;
}

/* SIP002 "ss://base64(method:password)@host:port#name" and the older
   "ss://base64(method:password@host:port)#name" */
function parseSsUri(uri) {
	var body = uri.replace(/^ss:\/\//i, '');
	var name = '';
	var h = body.indexOf('#');
	if (h >= 0) {
		try { name = decodeURIComponent(body.slice(h + 1)); } catch (e) { name = body.slice(h + 1); }
		body = body.slice(0, h);
	}
	var query = '';
	var qi = body.indexOf('?');
	if (qi >= 0) { query = body.slice(qi); body = body.slice(0, qi); }

	var method, password, host, port;
	if (body.indexOf('@') >= 0) {
		var parts = body.split('@');
		var creds = parts[0], hostport = parts.slice(1).join('@');
		/* SIP002 says base64url(method:password), and clients percent-encode it often
		   enough that the padding arrives as %3D.  Decode the percent-encoding first,
		   then decide: base64 of "method:password" never contains a colon, so the
		   colon is what tells the two shapes apart. */
		var dec;
		try { dec = decodeURIComponent(creds); } catch (e) { dec = creds; }
		if (dec.indexOf(':') < 0) {
			try { dec = b64decode(dec); } catch (e) { /* leave it and fail below */ }
		}
		var ci = dec.indexOf(':');
		if (ci < 0) throw new Error(_('The shadowsocks link has no method and password.'));
		method = dec.slice(0, ci);
		password = dec.slice(ci + 1);
		var hp = hostport.split(':');
		port = parseInt(hp.pop(), 10);
		host = hp.join(':').replace(/^\[|\]$/g, '');
	} else {
		var whole = b64decode(body);
		var m = whole.match(/^([^:]+):(.*)@([^:]+):(\d+)$/);
		if (!m) throw new Error(_('The shadowsocks link could not be read.'));
		method = m[1]; password = m[2]; host = m[3]; port = parseInt(m[4], 10);
	}
	if (!host || !(port > 0 && port < 65536))
		throw new Error(_('The shadowsocks link has no usable address and port.'));

	var q = parseQuery(query);
	if (q.plugin)
		throw new Error(_('This link needs the "%s" plugin, which this router does not have.')
		                .format(String(q.plugin).split(';')[0]));

	var p = emptyProfile();
	p.protocol = 'shadowsocks';
	p.address = host; p.port = port;
	p.method = method; p.password = password;
	p.name = name || (host + ':' + port);
	p.transport = 'tcp'; p.tls = 'none';
	return p;
}

function parseUri(uri) {
	uri = String(uri || '').trim();
	if (!uri) throw new Error(_('Paste a link first.'));
	/* people paste with the scheme mangled by chat apps often enough to be worth it */
	uri = uri.replace(/^\s*([a-z]+):\/*/i, function (_m, s) { return s.toLowerCase() + '://'; });

	if (/^vless:\/\//i.test(uri))  return parseUserinfoUri(uri, 'vless');
	if (/^trojan:\/\//i.test(uri)) return parseUserinfoUri(uri, 'trojan');
	if (/^vmess:\/\//i.test(uri))  return parseVmessUri(uri);
	if (/^ss:\/\//i.test(uri))     return parseSsUri(uri);
	if (/^(ssr|hysteria2?|tuic|wireguard|juicity):\/\//i.test(uri))
		throw new Error(_('%s links are a different protocol; Xray does not speak it.')
		                .format(uri.split(':')[0].toUpperCase()));
	throw new Error(_('Unknown link type. Expected vless://, vmess://, trojan:// or ss://'));
}

/* The other direction, for copying a profile back out. */
function toUri(p) {
	var q = [], name = encodeURIComponent(p.name || '');
	function add(k, v) { if (v !== '' && v != null) q.push(k + '=' + encodeURIComponent(v)); }

	if (p.protocol === 'shadowsocks') {
		return 'ss://' + encodeURIComponent(b64encode((p.method || '') + ':' + (p.password || '')))
		     + '@' + p.address + ':' + p.port + '#' + name;
	}
	if (p.protocol === 'vmess') {
		var o = {
			v: '2', ps: p.name || '', add: p.address, port: String(p.port),
			id: p.uuid, aid: String(p.alterId || 0), scy: p.security || 'auto',
			net: p.transport || 'tcp', type: p.headerType || 'none',
			host: p.host || '', path: (p.transport === 'grpc') ? (p.serviceName || '') : (p.path || ''),
			tls: (p.tls === 'none') ? '' : 'tls', sni: p.sni || '', alpn: p.alpn || '',
			fp: p.fingerprint || ''
		};
		return 'vmess://' + b64encode(JSON.stringify(o));
	}

	add('type', p.transport || 'tcp');
	add('security', (p.tls === 'none') ? 'none' : p.tls);
	if (p.protocol === 'vless') add('encryption', p.encryption || 'none');
	add('flow', p.flow);
	add('sni', p.sni);
	add('alpn', p.alpn);
	add('fp', p.fingerprint);
	add('pbk', p.publicKey);
	add('sid', p.shortId);
	add('spx', p.spiderX);
	if (p.transport === 'ws' || p.transport === 'httpupgrade' || p.transport === 'http') {
		add('path', p.path); add('host', p.host);
	} else if (p.transport === 'grpc') {
		add('serviceName', p.serviceName);
		if (p.grpcMode === 'multi') add('mode', 'multi');
	} else if (p.transport === 'kcp') {
		add('headerType', p.headerType); add('seed', p.seed);
	} else if (p.transport === 'tcp' && p.headerType === 'http') {
		add('headerType', 'http'); add('path', p.path); add('host', p.host);
	}
	if (p.allowInsecure) add('allowInsecure', '1');

	var user = (p.protocol === 'trojan') ? encodeURIComponent(p.password || '') : (p.uuid || '');
	var host = (String(p.address).indexOf(':') >= 0) ? '[' + p.address + ']' : p.address;
	return p.protocol + '://' + user + '@' + host + ':' + p.port +
	       (q.length ? '?' + q.join('&') : '') + '#' + name;
}

/* --------------------------------------------------------------------- widgets */

function summary(p) {
	var bits = [String(p.protocol || '').toUpperCase()];
	if (p.tls === 'reality') bits.push('REALITY');
	else if (p.tls === 'tls') bits.push('TLS');
	bits.push(String(p.transport || 'tcp').toUpperCase());
	if (p.flow) bits.push(p.flow);
	return bits.join(' · ');
}

function section(title, children, descr) {
	var kids = [E('h3', {}, title)];
	if (descr) kids.push(E('div', { 'class': 'cbi-section-descr' }, descr));
	return E('div', { 'class': 'cbi-section fade-in' }, kids.concat(children));
}

function facts(rows) {
	var out = [];
	for (var i = 0; i < rows.length; i++) {
		var r = rows[i];
		if (!r) continue;
		var o = r[2] || {}, v = r[1], cell;
		if (o.raw) cell = v;
		else cell = (v == null || v === '') ? '–' : String(v);
		out.push(E('div', { 'class': 'fact' }, [
			E('div', { 'class': 'fact-k' }, r[0]),
			E('div', { 'class': 'fact-v' + (o.mono ? ' mono' : '') }, cell)
		]));
	}
	return E('div', { 'class': 'facts' }, out);
}

function label(text, kind) {
	return E('span', { 'class': 'label ' + (kind || '') }, text);
}

function state(text, kind) {
	return E('span', { 'class': 'dotlabel ' + (kind || 'off') }, text);
}

function action(text, kind, fn, confirmText) {
	return E('button', {
		'class': 'cbi-button cbi-button-' + (kind || 'neutral'),
		'type': 'button',
		'click': ui.createHandlerFn(this, function (ev) {
			if (confirmText && !confirm(confirmText)) return;
			return Promise.resolve(fn(ev)).catch(function (e) {
				ui.addNotification(null, E('p', {}, String(e.message || e)), 'error');
			});
		})
	}, text);
}

/* The backend answers { error: "..." } rather than failing the RPC, so that the page
 * can show the sentence instead of a stack trace. */
function checked(promise, okMsg) {
	return Promise.resolve(promise).then(function (res) {
		res = res || {};
		if (res.error) throw new Error(res.error);
		if (okMsg) ui.addNotification(null, E('p', {}, okMsg), 'info');
		return res;
	});
}

return baseclass.extend({
	api: api,
	callLong: callLong,
	parseUri: parseUri,
	toUri: toUri,
	emptyProfile: emptyProfile,
	summary: summary,
	section: section,
	facts: facts,
	label: label,
	state: state,
	action: action,
	checked: checked
});
