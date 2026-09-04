/* Render the Xray page outside a browser, with fixtures instead of a router.
 *
 * The page is client-side JavaScript, so nothing in the firmware build ever executes
 * it: a typo in a rarely opened dialog reaches the device and shows up as a blank
 * page with an error in a console nobody has open.  This harness runs render() and
 * every dialog against a fake DOM and reports what came out, so that class of defect
 * fails on the host instead.
 *
 * It is deliberately a small hand-written DOM rather than jsdom: no dependency to
 * install, and the page only needs elements, attributes, children and events.
 *
 * Usage:  node xray-view-harness.js <view main.js> <hh71vm/xray.js>
 *         stdout: JSON summary of what rendered
 */
'use strict';

const fs = require('fs');

/* ------------------------------------------------------------- a very small DOM */

let created = 0;

function Node(tag) {
	this.tagName = String(tag || 'div').toUpperCase();
	this.attrs = {};
	this.children = [];
	this.style = {};
	this.classList = { add() {}, remove() {}, contains() { return false; } };
	this.checked = false;
	this.value = '';
	this.listeners = {};
	created++;
}
Node.prototype.appendChild = function (n) {
	if (n != null) this.children.push(n);
	return n;
};
Node.prototype.setAttribute = function (k, v) { this.attrs[k] = v; };
Node.prototype.removeAttribute = function (k) { delete this.attrs[k]; };
Node.prototype.getAttribute = function (k) { return this.attrs[k]; };
Node.prototype.addEventListener = function (ev, fn) {
	(this.listeners[ev] = this.listeners[ev] || []).push(fn);
};
Node.prototype.focus = function () {};
Node.prototype.select = function () {};
Node.prototype.remove = function () {};
Object.defineProperty(Node.prototype, 'className', {
	get() { return this.attrs['class'] || ''; },
	set(v) { this.attrs['class'] = v; }
});

function text(node, out) {
	out = out || [];
	if (node == null) return out;
	if (typeof node === 'string' || typeof node === 'number') { out.push(String(node)); return out; }
	if (Array.isArray(node)) { node.forEach((n) => text(n, out)); return out; }
	(node.children || []).forEach((n) => text(n, out));
	return out;
}

function E(tag, attrs, kids) {
	if (attrs && typeof attrs !== 'object') { kids = attrs; attrs = null; }
	const n = new Node(tag);
	for (const k in (attrs || {})) {
		const v = attrs[k];
		if (v == null) continue;
		if (typeof v === 'function') n.addEventListener(k, v);
		else if (k === 'value') n.value = String(v);
		else n.attrs[k] = v;
	}
	const list = Array.isArray(kids) ? kids : (kids != null ? [kids] : []);
	list.forEach((k) => n.appendChild(k));
	return n;
}

/* ------------------------------------------------------------------- LuCI stubs */

if (!String.prototype.format) {
	String.prototype.format = function () {
		const args = Array.prototype.slice.call(arguments);
		let i = 0;
		return this.replace(/%[sdx]/g, () => String(args[i++]));
	};
}

const modals = [];
const notifications = [];
const polls = [];

const stubs = {
	view: { extend: (o) => o },
	baseclass: { extend: (o) => o },
	dom: {
		content(node, kids) {
			node.children = [];
			(Array.isArray(kids) ? kids : [kids]).forEach((k) => node.appendChild(k));
		}
	},
	ui: {
		showModal(title, kids) { modals.push({ title, kids }); return kids; },
		hideModal() {},
		addNotification(_t, msg, kind) { notifications.push({ kind, text: text(msg).join(' ') }); },
		createHandlerFn(ctx, fn) { return fn; }
	},
	poll: { add(fn, s) { polls.push(s); } },
	rpc: { declare: () => () => Promise.resolve({}), getBaseURL: () => '/ubus' },
	request: { post: () => Promise.resolve({}) },
	L: { isObject: (v) => v !== null && typeof v === 'object', env: {} },
	_: (s) => s
};

global.document = { querySelector: () => null };
global.window = { HH71: null, setTimeout: (fn) => fn && 0,
                  location: { host: '192.168.1.1' } };
global.E = E;
global.atob = (s) => Buffer.from(s, 'base64').toString('binary');
global.btoa = (s) => Buffer.from(s, 'binary').toString('base64');

function load(path, extraNames, extraValues) {
	let src = fs.readFileSync(path, 'utf8')
		.replace(/^'use strict';\s*$/m, '')
		.replace(/^'require [^']*';\s*$/gm, '');
	const names = ['view', 'baseclass', 'dom', 'ui', 'poll', 'rpc', 'request', 'L', 'E', '_']
		.concat(extraNames || []);
	const values = [stubs.view, stubs.baseclass, stubs.dom, stubs.ui, stubs.poll,
	                stubs.rpc, stubs.request, stubs.L, E, stubs._].concat(extraValues || []);
	return new Function(...names, src)(...values);
}

const xrayModule = load(process.argv[3]);
const view = load(process.argv[2], ['x'], [xrayModule]);

/* ------------------------------------------------------------------- fixtures */

const status = {
	mode: 'vpn', enabled: true, autostart: true, watchdog: false, running: true,
	pid: 1234, binary: '/mnt/extern/opkg/usr/bin/xray', version: 'Xray 26.3.27',
	share: true, assets: false, config: '/etc/xray/config.json', config_exists: true,
	tproxy: false, profile: 'p1', profile_name: 'bench', profile_count: 2,
	resolved: '192.0.2.10', rules: true, uptime: 42, rss: 33000000,
	clock: '2026-09-04 03:00:00 UTC',
	ports: { socks: 1080, http: 1081, redirect: 1082, tproxy: 1083, dns: 1053 },
	listening: { socks: true, http: true, redirect: true },
	capture: { mode: 'auto', ifaces: 'br-lan', uplink: 'eth2',
	           wireless: 'br-lan:wlan0 br-lan:wlan1' }
};
const store = {
	version: 1, active: 'p1',
	profiles: [
		{ id: 'p1', name: 'bench', protocol: 'vless', address: '192.0.2.10', port: 8443,
		  uuid: 'a8c0ee5d-e852-4b7d-a28f-bf0db2e21336', flow: 'xtls-rprx-vision',
		  transport: 'tcp', tls: 'reality', sni: 'www.example.com',
		  publicKey: 'k', shortId: 's', spiderX: '/', fingerprint: 'chrome' },
		{ id: 'p2', name: 'ws server', protocol: 'vmess', address: 'example.com',
		  port: 443, uuid: 'cb53631d-0323-4fe9-9448-b0ec534c43f1', transport: 'ws',
		  tls: 'tls', path: '/x', host: 'example.com', security: 'auto' }
	]
};
const settings = {
	mode: 'vpn', lan_ifaces: 'br-lan', router_traffic: '1', block_quic: '1',
	block_ipv6: '1', sniffing: '1', set_clock: '1', dns_server: '1.1.1.1',
	socks_port: '1080', http_port: '1081', redirect_port: '1082',
	tproxy_port: '1083', dns_port: '1053',
	probe_url: 'http://cp.cloudflare.com/generate_204', probe_timeout: '12',
	watchdog_period: '60', watchdog_fails: '2', loglevel: 'warning',
	api_enabled: '0', api_token: '', capture_udp: '0'
};

const body = view.render([status, store, settings]);
const rendered = text(body).join(' ');

/* every dialog, opened through the stubbed ui.showModal */
function openDialogs() {
	const opened = [];
	function walkForHandlers(node, want, fn) {
		if (!node || typeof node !== 'object') return;
		const label = text(node).join(' ');
		if (node.listeners && node.listeners.click && want.test(label)) fn(node, label);
		(node.children || []).forEach((c) => walkForHandlers(c, want, fn));
	}
	walkForHandlers(body, /^(Add from a link|Add by hand|Settings|Edit|Link)$/, (node, label) => {
		const before = modals.length;
		try { node.listeners.click[0]({}); } catch (e) { opened.push([label, 'ERROR: ' + e.message]); return; }
		opened.push([label, modals.length > before ? 'opened' : 'no modal']);
	});
	return opened;
}

const dialogs = openDialogs();

process.stdout.write(JSON.stringify({
	elements: created,
	poll_intervals: polls,
	notifications: notifications,
	dialogs: dialogs,
	modal_titles: modals.map((m) => m.title),
	has_connect_button: /Disconnect|Connect/.test(rendered),
	has_autostart_switch: /Connect automatically on router power on/.test(rendered),
	has_reconnect_switch: /Reconnect automatically if the connection drops/.test(rendered),
	mentions_profiles: /bench/.test(rendered) && /ws server/.test(rendered),
	warns_about_udp: /other UDP does not/.test(rendered),
	warns_about_assets: /geoip/.test(rendered),
	shows_capture_set: /br-lan/.test(rendered) && /Wi-Fi included/.test(rendered),
	shows_uplink: /eth2/.test(rendered),
	api_examples: modals.filter((m) => m.title === 'Settings')
		.map((m) => text(m.kids).join(' '))
		.some((t) => /curl -s/.test(t) && /action=connect/.test(t) &&
		             /action=disconnect/.test(t) && /action=profiles/.test(t) &&
		             /action=activate/.test(t)),
	settings_offers_automatic_capture: modals.filter((m) => m.title === 'Settings')
		.map((m) => text(m.kids).join(' '))
		.some((t) => /Automatic/.test(t) && /Capturing right now/.test(t)),
	text_length: rendered.length
}, null, 1));
