'use strict';
'require baseclass';
'require rpc';
'require request';
'require ui';

/* Shared plumbing for the modem pages: the ubus calls, the formatting, and the few
 * widgets every page repeats.  Everything talks to hh71vm-modemd through rpcd, so the
 * session ACL in luci-app-hh71vm-modem.json is what actually grants access.
 */

function decl(method, params) {
	return rpc.declare({ object: 'hh71vm-modem', method: method, params: params });
}

/* Slow calls need their own request, because rpc.js hard-codes a 20 second browser
 * timeout and several modem operations legitimately take longer.  Same envelope
 * rpc.js builds, only the deadline differs.
 *
 * There is a hard ceiling above this that we do not control: LuCI's /admin/ubus
 * proxy calls libubus with its default 30 second timeout, so no single RPC can take
 * longer than that no matter what the browser and rpcd allow.  Anything slower has
 * to run as a job in the daemon and be polled -- see net_scan / net_scan_result. */
function callLong(method, params, timeoutSec) {
	var url = rpc.getBaseURL() + '/hh71vm-modem.' + method;
	var msg = {
		jsonrpc: '2.0', id: Date.now() % 100000, method: 'call',
		params: [L.env.sessionid || '00000000000000000000000000000000',
		         'hh71vm-modem', method, params || {}]
	};
	return request.post(url, msg, {
		timeout: (timeoutSec || 60) * 1000,
		nobatch: true,
		credentials: true
	}).then(function (res) {
		if (!res.ok)
			throw new Error(_('The router answered with HTTP %d').format(res.status));
		var body = res.json();
		if (!L.isObject(body))
			throw new Error(_('Malformed answer from the router'));
		if (L.isObject(body.error))
			throw new Error(body.error.message || _('RPC error'));
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
	status:          decl('status'),
	refresh:         decl('refresh'),
	usage:           decl('usage'),
	usageReset:      decl('usage_reset'),
	reconnect:       decl('reconnect'),
	cellInfo:        decl('cell_info'),
	simInfo:         decl('sim_info'),
	pinStatus:       decl('pin_status'),
	pinUnlock:       decl('pin_unlock',   ['pin']),
	pukUnlock:       decl('puk_unlock',   ['puk', 'pin']),
	pinEnable:       decl('pin_enable',   ['pin', 'enable']),
	pinChange:       decl('pin_change',   ['old', 'new']),
	netMode:         decl('net_mode'),
	netModeSet:      decl('net_mode_set', ['mode']),
	netRegister:     decl('net_register', ['numeric', 'act', 'auto']),
	radioSet:        decl('radio_set',    ['on']),
	dataConnect:     decl('data_connect', ['cid']),
	dataDisconnect:  decl('data_disconnect', ['cid']),
	apnList:         decl('apn_list'),
	apnSet:          decl('apn_set',      ['cid', 'apn', 'pdp_type', 'auth', 'user', 'pass']),
	apnDelete:       decl('apn_delete',   ['cid']),
	smsRead:         decl('sms_read',     ['index']),
	smsMark:         decl('sms_mark',     ['index', 'ts', 'read']),
	smsSend:         decl('sms_send',     ['to', 'text']),
	smsSave:         decl('sms_save',     ['to', 'text']),
	smsSettings:     decl('sms_settings'),
	smsSettingsSet:  decl('sms_settings_set', ['sca']),
	phonebookList:   decl('phonebook_list', ['first', 'count']),
	phonebookAdd:    decl('phonebook_add',  ['index', 'number', 'name']),
	phonebookDelete: decl('phonebook_delete', ['index']),

	/* the operator scan is a job: start it, then poll */
	netScan:       decl('net_scan'),
	netScanResult: decl('net_scan_result'),

	/* USSD is a job for the same reason: AT+CUSD returns OK at once and the text
	   arrives later as a +CUSD URC, on the network's schedule.  A single RPC cannot
	   outlive the 30 s /admin/ubus ceiling, so start it and poll it. */
	ussdStart:     decl('ussd_start', ['code']),
	ussdResult:    decl('ussd_result'),

	/* slower than rpc.js's own 20 s, but still inside the 30 s ubus ceiling */
	smsList:      function ()        { return callLong('sms_list', {}, 28); },
	smsDelete:    function (i, list) { return callLong('sms_delete',
	                                       { index: i, indexes: list }, 28); },
	smsDeleteAll: function ()        { return callLong('sms_delete_all', {}, 28); },
	at:           function (cmds, t) { return callLong('at',
	                                       { cmds: cmds, timeout: t || 22 }, 28); },

	/* The blocking form, kept only so this page still works against a daemon that
	   has no ussd_start.  Prefer ussdRun. */
	ussd:         function (code)    { return callLong('ussd', { code: code }, 28); },

	/* Start a USSD job and poll it to the end.  onProgress, when given, is called
	   with the seconds elapsed, so the dialog can say something while the network
	   thinks.  Falls back to the blocking call against a daemon with no job API. */
	ussdRun: function (code, onProgress) {
		var self = this;
		var deadline = Date.now() + 120000;

		function poll() {
			return new Promise(function (r) { window.setTimeout(r, 1500); })
				.then(function () { return self.ussdResult(); })
				.then(function (r) {
					r = r || {};
					if (r.state !== 'running') return r;
					if (Date.now() > deadline)
						return { ok: false, state: 'failed', error: _('The network did not answer.') };
					if (onProgress) onProgress(r.elapsed || 0);
					return poll();
				});
		}

		return self.ussdStart(code).then(function (res) {
			res = res || {};
			/* A daemon without ussd_start answers nothing recognisable, so fall back
			   to the call it does have rather than reporting a bogus failure. */
			if (res.state == null && res.error == null) return self.ussd(code);
			if (res.state !== 'running') return res;
			if (onProgress) onProgress(res.elapsed || 0);
			return poll();
		});
	}
};

/* ------------------------------------------------------------------ format */

function bytes(n) {
	if (n == null) return '–';
	var u = ['B', 'KiB', 'MiB', 'GiB', 'TiB'], i = 0;
	n = Number(n);
	while (n >= 1024 && i < u.length - 1) { n /= 1024; i++; }
	return (i === 0 ? n : n.toFixed(n < 10 ? 2 : 1)) + ' ' + u[i];
}

function duration(sec) {
	if (sec == null) return '–';
	sec = Math.max(0, Math.floor(sec));
	var d = Math.floor(sec / 86400), h = Math.floor(sec % 86400 / 3600),
	    m = Math.floor(sec % 3600 / 60), s = sec % 60, out = [];
	if (d) out.push(d + 'd');
	if (d || h) out.push(h + 'h');
	if (d || h || m) out.push(m + 'm');
	out.push(s + 's');
	return out.join(' ');
}

/* "26/08/18,00:10:05+12" -- the trailing number is the offset in quarter hours */
function smsTime(ts) {
	if (!ts) return '–';
	var m = String(ts).match(/^(\d+)\/(\d+)\/(\d+),(\d+):(\d+):(\d+)/);
	if (!m) return String(ts);
	return '20%s-%s-%s %s:%s:%s'.format(m[1], m[2], m[3], m[4], m[5], m[6]);
}

/* ----------------------------------------------------------------- widgets */

function copyText(text) {
	var ok = window.HH71 ? window.HH71.copy(String(text)) : false;
	if (ok && window.HH71) window.HH71.toast('Copied: ' +
		(text.length > 42 ? String(text).slice(0, 42) + '…' : text));
	return ok;
}

/* The copy button itself comes from the theme, so the icon and the feedback are the
 * same here as everywhere else in the interface. */
function copyable(value, extraClass) {
	if (value == null || value === '') return E('span', {}, '–');
	var kids = [E('span', {}, String(value))];
	if (window.HH71 && window.HH71.copyButton)
		kids.push(window.HH71.copyButton(function () { return String(value); }));
	return E('span', { 'class': 'copyable ' + (extraClass || '') }, kids);
}

/* A key/value list.  rows: [label, value] or [label, value, {copy, mono, raw}].
 * Renders as a grid of pairs, so a wide screen gets two columns instead of one
 * narrow column with half the page empty. */
function facts(rows) {
	var out = [];
	for (var i = 0; i < rows.length; i++) {
		var r = rows[i];
		if (!r) continue;
		var o = r[2] || {}, v = r[1], cell;
		if (o.raw) cell = v;
		else if (o.copy && v != null && v !== '' && v !== '–') cell = copyable(v);
		else cell = (v == null || v === '') ? '–' : String(v);
		out.push(E('div', { 'class': 'fact' }, [
			E('div', { 'class': 'fact-k' }, r[0]),
			E('div', { 'class': 'fact-v' + (o.mono ? ' mono' : '') }, cell)
		]));
	}
	return E('div', { 'class': 'facts' }, out);
}

function section(title, children, descr) {
	var kids = [E('h3', {}, title)];
	if (descr) kids.push(E('div', { 'class': 'cbi-section-descr' }, descr));
	return E('div', { 'class': 'cbi-section fade-in' }, kids.concat(children));
}

/* five bars, coloured by how good the level is */
function signalBars(n) {
	n = n || 0;
	var cls = n >= 4 ? '' : (n >= 2 ? 'weak' : 'bad');
	var e = E('span', { 'class': 'bars ' + cls });
	for (var i = 1; i <= 5; i++)
		e.appendChild(E('i', i <= n ? { 'class': 'on' } : {}));
	return e;
}

function label(text, kind) {
	return E('span', { 'class': 'label ' + (kind || '') }, text);
}

/* State that does not lean on colour alone: a dot plus the word. */
function state(text, kind) {
	return E('span', { 'class': 'dotlabel ' + (kind || 'off') }, text);
}

/* Buttons that call the daemon: disable while in flight, report either way. */
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

/* Uniform error reporting: the daemon answers { ok: false, error: "..." } */
function checked(promise, okMsg) {
	return Promise.resolve(promise).then(function (res) {
		res = res || {};
		if (res.error || res.ok === false)
			throw new Error(res.error || res.detail || _('The modem rejected the request'));
		if (okMsg) ui.addNotification(null, E('p', {}, okMsg), 'info');
		return res;
	});
}

function linkState(st) {
	var l = (st || {}).link || {};
	if (l.state === 'ready') return null;
	return E('div', { 'class': 'alert-message warning' }, [
		E('h4', {}, _('No connection to the modem')),
		E('p', {}, [
			_('The control channel to the Qualcomm side is'), ' ',
			E('strong', {}, String(l.state || 'down')), '. ',
			l.error ? String(l.error) : ''
		]),
		E('p', {}, _('Nothing on this page can be read or changed until it comes back.'))
	]);
}

return baseclass.extend({
	api: api,
	callLong: callLong,
	bytes: bytes,
	duration: duration,
	smsTime: smsTime,
	copyText: copyText,
	copyable: copyable,
	facts: facts,
	section: section,
	signalBars: signalBars,
	label: label,
	state: state,
	action: action,
	checked: checked,
	linkState: linkState
});
