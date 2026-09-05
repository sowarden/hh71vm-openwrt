'use strict';

const fs = require('fs');
const source = fs.readFileSync(process.argv[2], 'utf8');
let assertions = 0;

function equal(actual, expected, label) {
	assertions++;
	if (actual !== expected)
		throw new Error(`${label}: expected ${expected}, got ${actual}`);
}
function truthy(value, label) {
	assertions++;
	if (!value) throw new Error(label);
}

String.prototype.format = function () {
	const args = Array.from(arguments);
	let index = 0;
	return this.replace(/%[sd]/g, () => String(args[index++]));
};

function E(tag, attrs, children) {
	if (Array.isArray(tag)) return { tag: 'fragment', attrs: {}, children: tag };
	const node = { tag, attrs: attrs || {}, children: [] };
	if (children != null) node.children = Array.isArray(children) ? children : [children];
	node.appendChild = child => node.children.push(child);
	return node;
}

function textOf(value) {
	if (value == null) return '';
	if (Array.isArray(value)) return value.map(textOf).join(' ');
	if (typeof value === 'object') return textOf(value.children);
	return String(value);
}

function findNode(value, predicate) {
	if (value == null) return null;
	if (Array.isArray(value)) {
		for (const child of value) {
			const found = findNode(child, predicate);
			if (found) return found;
		}
		return null;
	}
	if (typeof value !== 'object') return null;
	if (predicate(value)) return value;
	return findNode(value.children, predicate);
}

const view = { extend: value => value };
const dom = { content: (node, children) => { node.children = children; } };
const ui = {};
const L = { resolveDefault: (promise, fallback) => Promise.resolve(promise).catch(() => fallback) };
const translate = value => value;
const windowStub = {};
const confirmStub = () => true;

let calls = [];
let listResponse = { ok: true, messages: [] };
let snapshotResponse = { ok: true, messages: [] };
let statusResponse = { sms: { used: 0, total: 100, unread: 0 } };

const modem = {
	api: {
		status: () => { calls.push('status'); return Promise.resolve(statusResponse); },
		smsList: () => { calls.push('list'); return Promise.resolve(listResponse); },
		smsSnapshot: () => { calls.push('snapshot'); return Promise.resolve(snapshotResponse); },
		smsMark: (index, ts, read) => {
			calls.push(`mark:${index}:${ts}:${read}`);
			return Promise.resolve({ ok: true });
		},
		smsDelete: (index, indexes) => {
			calls.push(`delete:${index}:${indexes.join('+')}`);
			return Promise.resolve({ ok: true });
		},
		smsDeleteAll: () => Promise.resolve({ ok: true }),
		smsSettings: () => Promise.resolve({ ok: true, sms: {} }),
		smsSettingsSet: () => Promise.resolve({ ok: true }),
		smsSend: () => Promise.resolve({ ok: true }),
		smsSave: () => Promise.resolve({ ok: true })
	},
	action: (text, kind, fn) => E('button', { kind, handler: fn }, text),
	checked: promise => Promise.resolve(promise).then(result => {
		if (!result || result.ok === false || result.error) throw new Error((result || {}).error || 'failed');
		return result;
	}),
	facts: rows => E('facts', {}, rows.map(row => E('row', {}, [row[0], row[1]]))),
	label: text => E('label', {}, text),
	linkState: () => null,
	smsTime: value => value,
	copyText: () => true
};

const page = new Function('view', 'ui', 'dom', 'm', 'E', '_', 'L', 'window', 'confirm',
	source)(view, ui, dom, modem, E, translate, L, windowStub, confirmStub);

async function main() {
	listResponse = {
		ok: true,
		messages: [
			{ index: 4, indexes: [4], sender: 'SENDER-A', text: 'first body',
			  ts: '26/09/05,01:00:01+00', unread: true, parts: 1, status: 'REC UNREAD' },
			{ index: 5, indexes: [5, 6], sender: 'SENDER-B', text: 'second body',
			  ts: '26/09/05,01:00:02+00', unread: true, parts: 2, status: 'REC UNREAD' }
		]
	};
	statusResponse = { sms: { storage: 'SM', used: 3, total: 50, unread: 2, count: 2 } };
	calls = [];
	const loaded = await page.load();
	equal(calls.join(','), 'list,status', 'list/status ordering');
	equal(loaded[1].messages.length, 2, 'multiple messages loaded');
	const renderedTree = page.render(loaded);
	const rendered = textOf(renderedTree);
	truthy(rendered.includes('first body'), 'first message rendered');
	truthy(rendered.includes('second body'), 'second message rendered');
	truthy(rendered.includes('2 parts'), 'multipart badge rendered');
	truthy(!rendered.includes('temporarily out of sync'), 'consistent unread count');
	const mark = findNode(renderedTree, node => node.tag === 'button' && textOf(node) === 'Mark read');
	truthy(mark && typeof mark.attrs.handler === 'function', 'mark action rendered');
	await mark.attrs.handler();
	truthy(calls.includes('mark:5:26/09/05,01:00:02+00:true'), 'mark action arguments');
	const remove = findNode(renderedTree, node => node.tag === 'button' && textOf(node) === 'Delete');
	truthy(remove && typeof remove.attrs.handler === 'function', 'delete action rendered');
	await remove.attrs.handler();
	truthy(calls.includes('delete:null:5+6'), 'multipart delete action indexes');

	listResponse = { ok: false, error: 'CMGL failed', messages: [] };
	snapshotResponse = { ok: false, stale: true, generation: 9, messages: loaded[1].messages };
	calls = [];
	const failed = await page.load();
	equal(calls.join(','), 'list,snapshot,status', 'error fallback ordering');
	equal(failed[1].ok, false, 'failed list remains failed');
	equal(failed[1].messages.length, 2, 'stale cache retained');
	const stale = textOf(page.render(failed));
	truthy(stale.includes('Messages could not be refreshed'), 'error shown');
	truthy(stale.includes('last cached messages'), 'stale cache labelled');
	truthy(!stale.includes('No messages are stored on the modem'), 'error is not empty success');

	const malformed = page.render([statusResponse, {
		ok: true,
		decode_errors: 1,
		messages: [{ index: 8, indexes: [8], status: 'REC UNREAD', unread: true,
		             parts: 1, decode_error: 'short pdu' }]
	}]);
	const malformedText = textOf(malformed);
	truthy(malformedText.includes('decode error'), 'decode placeholder badge');
	truthy(malformedText.includes('remains available for marking or deletion'),
	       'decode placeholder actions explained');

	console.log(`sms LuCI tests: ${assertions} assertions passed`);
}

main().catch(error => {
	console.error(error.stack || error);
	process.exit(1);
});
