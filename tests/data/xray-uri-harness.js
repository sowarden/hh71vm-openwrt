/* Load the page's URI parser outside a browser and answer questions about it.
 *
 * The parser is the one piece of this feature that has to understand what other
 * people's servers emit, so it is worth testing against a table of real link shapes
 * rather than only by hand in the browser.  The module is written for LuCI's loader,
 * which means a few 'require' pragmas and a bare `return` at the end; both are handled
 * here rather than by contorting the module itself.
 *
 * Usage:  node xray-uri-harness.js <path to xray.js>      (reads JSON on stdin)
 *   stdin:  { "uris": ["vless://...", ...] }  ->  stdout: { "results": [...] }
 */
'use strict';

const fs = require('fs');

const modulePath = process.argv[2];
let src = fs.readFileSync(modulePath, 'utf8');

/* the pragmas LuCI's loader reads, and the strict marker, are not JavaScript we can
   run here */
src = src.replace(/^'use strict';\s*$/m, '')
         .replace(/^'require [^']*';\s*$/gm, '');

const stubs = {
	baseclass: { extend: (o) => o },
	rpc: { declare: () => () => Promise.resolve({}), getBaseURL: () => '/ubus' },
	request: { post: () => Promise.resolve({}) },
	ui: { createHandlerFn: (ctx, fn) => fn, addNotification: () => {} },
	L: { isObject: (v) => (v !== null && typeof v === 'object'), env: {} },
	E: function () { return {}; },
	_: (s) => s
};

if (!String.prototype.format) {
	String.prototype.format = function () {
		const args = Array.prototype.slice.call(arguments);
		let i = 0;
		return this.replace(/%[sdx]/g, () => String(args[i++]));
	};
}

const factory = new Function('baseclass', 'rpc', 'request', 'ui', 'L', 'E', '_', src);
const mod = factory(stubs.baseclass, stubs.rpc, stubs.request, stubs.ui,
                    stubs.L, stubs.E, stubs._);

let input = '';
process.stdin.on('data', (d) => { input += d; });
process.stdin.on('end', () => {
	const req = JSON.parse(input || '{}');
	const results = (req.uris || []).map((uri) => {
		try {
			const p = mod.parseUri(uri);
			let round = null;
			try { round = mod.toUri(p); } catch (e) { round = 'ERROR: ' + e.message; }
			return { ok: true, profile: p, uri: round };
		} catch (e) {
			return { ok: false, error: String(e.message || e) };
		}
	});
	process.stdout.write(JSON.stringify({ results }, null, 1));
});
