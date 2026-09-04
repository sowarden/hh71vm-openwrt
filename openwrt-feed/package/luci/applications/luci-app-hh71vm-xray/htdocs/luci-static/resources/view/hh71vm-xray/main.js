'use strict';
'require view';
'require ui';
'require dom';
'require poll';
'require hh71vm.xray as x';

/* The Xray page.
 *
 * One page: the state and the Connect button at the top, the two switches that decide
 * what happens without anyone watching, the profiles below, and the diagnostics last.
 * Everything that changes anything goes through the hh71vm-xray ubus object.
 *
 * The one thing worth reading before changing this file: connecting is a job, not a
 * call.  LuCI's /admin/ubus proxy gives every RPC 30 seconds and a connection that is
 * about to fail routinely takes longer, so the page starts the job and polls it. That
 * is also what makes the failure legible -- each step reports as it happens, and the
 * step that fails is half the diagnosis.
 */

var PROTOCOLS = [
	['vless', 'VLESS'], ['vmess', 'VMess'], ['trojan', 'Trojan'],
	['shadowsocks', 'Shadowsocks']
];
var TRANSPORTS = [
	['tcp', 'TCP'], ['ws', 'WebSocket'], ['grpc', 'gRPC'],
	['http', 'HTTP/2'], ['httpupgrade', 'HTTPUpgrade'], ['kcp', 'mKCP']
];
var TLSMODES = [
	['none', _('none (plain)')], ['tls', 'TLS'], ['reality', 'REALITY']
];
var FLOWS = [
	['', _('none')], ['xtls-rprx-vision', 'xtls-rprx-vision']
];
var FINGERPRINTS = ['chrome', 'firefox', 'safari', 'ios', 'android', 'edge', 'random'];
var SS_METHODS = [
	'aes-128-gcm', 'aes-256-gcm', 'chacha20-ietf-poly1305',
	'xchacha20-ietf-poly1305', '2022-blake3-aes-128-gcm', '2022-blake3-aes-256-gcm'
];
var VMESS_SEC = ['auto', 'aes-128-gcm', 'chacha20-poly1305', 'none', 'zero'];

/* The page's own styling lives here rather than in the theme on purpose.  The theme is
 * a settled behavioural contract, and the image build runs its CSS through csstidy,
 * which has silently broken it before; a style element created by the view goes through
 * neither. Everything here is a variable the theme already defines. */
var CSS = [
	'.xray-hero{display:flex;flex-wrap:wrap;gap:1rem;align-items:center;',
	'  justify-content:space-between;padding:.6rem 0}',
	'.xray-hero-state{display:flex;flex-direction:column;gap:.35rem}',
	'.xray-hero-state>.label{align-self:flex-start}',
	'.xray-hero-profile{font-size:1.15rem;font-weight:600}',
	'.xray-hero-actions{display:flex;flex-wrap:wrap;gap:.4rem}',
	'.xray-switches{display:flex;flex-direction:column;gap:.5rem;margin-top:.8rem}',
	'.xray-switch{display:flex;gap:.6rem;align-items:flex-start;cursor:pointer}',
	'.xray-switch input{margin-top:.25rem}',
	'.xray-steps{display:flex;flex-direction:column;gap:.25rem;margin:.6rem 0}',
	'.xray-step{display:flex;gap:.5rem;align-items:baseline;font-size:.95em}',
	'.xray-step-mark{width:1.2em;text-align:center;font-weight:700}',
	'.xray-step.ok .xray-step-mark{color:var(--ok,#2e7d32)}',
	'.xray-step.bad .xray-step-mark{color:var(--bad,#c62828)}',
	'.xray-step-name{min-width:6.5em;font-weight:600;text-transform:capitalize}',
	'.xray-step-text{opacity:.85}',
	'.xray-log{max-height:22em;overflow:auto;white-space:pre-wrap;word-break:break-word;',
	'  font-size:.85em;padding:.5rem;border-radius:.3rem;background:rgba(127,127,127,.10);',
	'  margin-top:14px}',
	'.xray-form .cbi-value-title{min-width:14em}',
	'.xray-dialog .xray-form .cbi-value-title{grid-column:1;min-width:0;width:auto}',
	'.xray-dialog .xray-form .cbi-value-field{grid-column:2;min-width:0;width:auto}',
	/* the section headings sat directly on the separator above them */
	'.xray-form h4{margin:1.4rem 0 .6rem;padding-top:.2rem}',
	'.xray-form h4:first-child{margin-top:.2rem}',
	'#modal_overlay>.modal.xray-dialog{position:relative;display:grid;',
	'  grid-template-columns:minmax(0,1fr);',
	'  width:calc(100% - 32px);max-width:720px;max-height:calc(100vh - 48px);',
	'  overflow:auto;box-sizing:border-box}',
	'#modal_overlay>.modal.xray-settings-modal,',
	'#modal_overlay>.modal.xray-profile-modal{max-width:1040px;overflow:hidden;',
	'  grid-template-rows:auto minmax(0,1fr) auto}',
	'.xray-settings-modal>.xray-form,.xray-profile-modal>.xray-form{',
	'  min-height:0;overflow-y:auto;padding-right:.7rem;overscroll-behavior:contain}',
	'.xray-dialog>.xray-modal-title{display:flex;align-items:center;gap:1rem;',
	'  margin:0 0 .5rem}',
	'.xray-modal-close{display:inline-flex;align-items:center;justify-content:center;',
	'  flex:none;width:2rem;height:2rem;min-height:0;margin-left:auto;padding:0;',
	'  border:1px solid var(--border);border-radius:50%;background:transparent;',
	'  color:var(--muted);font:600 1.35rem/1 sans-serif;cursor:pointer}',
	'.xray-modal-close:hover,.xray-modal-close:focus-visible{background:var(--surface-2);',
	'  border-color:var(--border-strong);color:var(--text)}',
	'.xray-settings-modal>.cbi-page-actions,.xray-profile-modal>.cbi-page-actions{',
	'  position:relative;z-index:1;margin:0;padding-top:.75rem;',
	'  border-top:1px solid var(--border);background:var(--surface)}',
	'.xray-token{word-break:break-all}',
	'.xray-capture code{word-break:break-all}',
	'.xray-examples{display:flex;flex-direction:column;gap:.5rem}',
	'.xray-example-what{font-size:.85em;opacity:.75;margin-bottom:.1rem}',
	'.xray-examples code{display:block;padding:.35rem .5rem;border-radius:.3rem;',
	'  background:rgba(127,127,127,.10);font-size:.82em;overflow-x:auto;',
	'  white-space:pre;word-break:normal}',
	'.xcol-act{width:4.5em}',

	/* Below this width the two-column form rows and the button row stop working.
	   Labels go above their fields, controls go full width, and the buttons become a
	   grid instead of a ragged wrap. Tables keep the theme's horizontal scroll - side
	   scrolling is the settled answer on this interface, not cards. */
	'@media (max-width:980px){',
	'  .xray-hero{flex-direction:column;align-items:stretch;gap:.75rem}',
	'  .xray-hero-actions{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));',
	'    gap:.4rem}',
	'  .xray-hero-actions>.cbi-button{width:100%;margin:0}',
	'}',
	'@media (max-width:700px){',
	'  .xray-form .cbi-value{display:block}',
	'  .xray-dialog .xray-form .cbi-value-title{grid-column:1;min-width:0;',
	'    width:auto;display:block;',
	'    padding:0 0 .3rem;font-weight:600}',
	'  .xray-dialog .xray-form .cbi-value-field{grid-column:1;width:100%;display:block}',
	'  .xray-form input[type=text],.xray-form input[type=number],',
	'  .xray-form input[type=password],.xray-form select,.xray-form textarea{',
	'    width:100%;box-sizing:border-box;max-width:none}',
	'  .xray-form input[type=checkbox]{width:auto}',
	'  .mactions{flex-wrap:wrap;gap:.4rem}',
	'  .mactions>.cbi-button{flex:1 1 12em;margin:0}',
	'  #modal_overlay>.modal.xray-dialog{width:100%;',
	'    max-height:calc(100vh - 16px);',
	'    padding:16px}',
	'  .xray-settings-modal>.xray-form,.xray-profile-modal>.xray-form{padding-right:.35rem}',
	'}',
	'@media (max-width:560px){',
	'  .xray-hero-actions{grid-template-columns:1fr}',
	'  .xray-step{flex-wrap:wrap}',
	'  .xray-step-name{min-width:0}',
	'  .xray-step-text{flex-basis:100%;padding-left:1.7em}',
	'  .mactions>.cbi-button{flex:1 1 100%}',
	'  .xray-hero-profile{font-size:1.05rem}',
	'}'
].join('');

function row(title, field, descr) {
	return E('div', { 'class': 'cbi-value' }, [
		E('label', { 'class': 'cbi-value-title' }, title),
		E('div', { 'class': 'cbi-value-field' }, descr
			? [field, E('div', { 'class': 'cbi-value-description' }, descr)]
			: field)
	]);
}

function select(options, value) {
	var s = E('select', { 'class': 'cbi-input-select' });
	options.forEach(function (o) {
		var v = Array.isArray(o) ? o[0] : o, t = Array.isArray(o) ? o[1] : o;
		s.appendChild(E('option', {
			'value': v, 'selected': (String(value) === String(v)) ? 'selected' : null
		}, t));
	});
	return s;
}

function input(value, type, attrs) {
	var a = { 'type': type || 'text', 'class': 'cbi-input-text',
	          'value': (value == null) ? '' : String(value) };
	for (var k in (attrs || {})) a[k] = attrs[k];
	return E('input', a);
}

function checkbox(checked, onchange) {
	var b = E('input', { 'type': 'checkbox', 'class': 'cbi-input-checkbox' });
	b.checked = !!checked;
	if (onchange) b.addEventListener('change', onchange);
	return b;
}

/* A checkbox with its own label, saved the moment it is clicked.  These two are the
 * ones the owner asked for by name, so they sit at the top and are not buried in a
 * settings dialog behind a Save button. */
function switchRow(labelText, descr, checked, onSet) {
	var box = checkbox(checked);
	var busy = E('span', { 'class': 'label', 'style': 'display:none' }, _('saving…'));
	box.addEventListener('change', function () {
		busy.style.display = '';
		Promise.resolve(onSet(box.checked))
			.catch(function (e) {
				box.checked = !box.checked;
				ui.addNotification(null, E('p', {}, String(e.message || e)), 'error');
			})
			.then(function () { busy.style.display = 'none'; });
	});
	return E('label', { 'class': 'xray-switch' }, [
		box, E('span', {}, [E('strong', {}, labelText),
		                    descr ? E('div', { 'class': 'cbi-value-description' }, descr) : '']),
		busy
	]);
}

return view.extend({
	handleSave: null,
	handleSaveApply: null,
	handleReset: null,

	load: function () {
		return Promise.all([
			x.api.status(true).catch(function () { return {}; }),
			x.api.profiles(true).catch(function () { return { profiles: [] }; }),
			x.api.settings(true).catch(function () { return {}; })
		]);
	},

	render: function (data) {
		var body = E('div', { 'class': 'xray-page' });
		var st = data[0] || {}, store = data[1] || {}, cfg = data[2] || {};
		var lastResult = null;      /* the outcome of the last Connect, kept on screen */

		function fetch() {
			return Promise.all([
				x.api.status(true).catch(function () { return {}; }),
				x.api.profiles(true).catch(function () { return { profiles: [] }; }),
				x.api.settings(true).catch(function () { return {}; })
			]).then(function (d) {
				st = d[0] || {}; store = d[1] || {}; cfg = d[2] || {};
			});
		}

		function reload() { return fetch().then(draw); }

		/* Form dialogs stay inside the viewport. The title and actions remain visible
		   while the form itself scrolls, and every non-destructive dialog has the two
		   conventional escape routes missing from LuCI 19.07: a close button and the
		   backdrop. The connecting progress dialog is deliberately excluded. */
		var modalCleanup = null;

		function hideFormModal() {
			if (modalCleanup) {
				modalCleanup();
				modalCleanup = null;
			}
			ui.hideModal();
		}

		function showFormModal(title, children, kind) {
			if (modalCleanup) modalCleanup();
			var dlg = ui.showModal(title, children);
			if (!dlg || !dlg.classList) return dlg;

			dlg.classList.add('xray-dialog');
			if (kind) dlg.classList.add(kind);
			var heading = dlg.firstElementChild;
			if (heading) {
				heading.classList.add('xray-modal-title');
				heading.appendChild(E('button', {
					'type': 'button', 'class': 'xray-modal-close',
					'aria-label': _('Close'), 'title': _('Close'),
					'click': hideFormModal
				}, '\u00d7'));
			}

			var overlay = dlg.parentNode;
			function backdrop(ev) { if (ev.target === overlay) hideFormModal(); }
			function escape(ev) { if (ev.key === 'Escape') hideFormModal(); }
			if (overlay) overlay.addEventListener('click', backdrop);
			document.addEventListener('keydown', escape);
			modalCleanup = function () {
				if (overlay) overlay.removeEventListener('click', backdrop);
				document.removeEventListener('keydown', escape);
			};
			return dlg;
		}

		/* ----------------------------------------------------------- connecting */

		function stepList(job) {
			var kids = [];
			(job.steps || []).forEach(function (s) {
				kids.push(E('div', { 'class': 'xray-step ' + (s.ok ? 'ok' : 'bad') }, [
					E('span', { 'class': 'xray-step-mark' }, s.ok ? '✓' : '✗'),
					E('span', { 'class': 'xray-step-name' }, s.step),
					E('span', { 'class': 'xray-step-text' }, s.text || s.error || '')
				]));
			});
			if (job.state === 'running')
				kids.push(E('div', { 'class': 'xray-step run' }, [
					E('span', { 'class': 'xray-step-mark' }, '…'),
					E('span', { 'class': 'xray-step-name' }, job.stage || _('working')),
					E('span', { 'class': 'xray-step-text' }, _('in progress'))
				]));
			return E('div', { 'class': 'xray-steps' }, kids);
		}

		function resultBox(job) {
			if (!job) return null;
			if (job.ok) {
				return E('div', { 'class': 'alert-message success' }, [
					E('h4', {}, _('Connected')),
					E('p', {}, _('The tunnel carries traffic: the probe came back in %d ms.')
					            .format(job.ms || 0))
				]);
			}
			var e = job.explain || {};
			var kids = [E('h4', {}, e.title || _('The connection failed'))];
			if (job.stage)
				kids.push(E('p', {}, E('em', {}, _('It failed at the "%s" step.').format(job.stage))));
			if (e.detail) kids.push(E('p', {}, e.detail));
			if (job.error && job.error !== e.detail)
				kids.push(E('p', {}, E('code', {}, String(job.error))));
			if (e.hint) kids.push(E('p', {}, [E('strong', {}, _('What to do: ')), e.hint]));
			if (job.log)
				kids.push(E('details', {}, [
					E('summary', {}, _('What Xray itself said')),
					E('pre', { 'class': 'xray-log' }, String(job.log))
				]));
			return E('div', { 'class': 'alert-message warning' }, kids);
		}

		function connect() {
			var stepsBox = E('div', {});
			var closeBtn = E('button', {
				'class': 'cbi-button', 'disabled': 'disabled',
				'click': function () { ui.hideModal(); reload(); }
			}, _('Close'));

			ui.showModal(_('Connecting'), [
				E('p', {}, _('Each step reports as it happens. The whole sequence has to \
finish, including a request through the tunnel — a process that started is not a \
connection that works.')),
				stepsBox,
				E('div', { 'class': 'cbi-page-actions' }, [closeBtn])
			]);

			return x.api.connect(function (job) {
				dom.content(stepsBox, [stepList(job)]);
			}).then(function (job) {
				lastResult = job;
				dom.content(stepsBox, [stepList(job), resultBox(job)]);
				closeBtn.removeAttribute('disabled');
				closeBtn.className = 'cbi-button cbi-button-action';
				return fetch().then(draw);
			}).catch(function (e) {
				lastResult = { ok: false, error: String(e.message || e) };
				dom.content(stepsBox, [resultBox(lastResult)]);
				closeBtn.removeAttribute('disabled');
			});
		}

		function disconnect() {
			return x.checked(x.api.disconnect(), _('Disconnected.')).then(function () {
				lastResult = null;
				return reload();
			});
		}

		/* ------------------------------------------------------- profile editing */

		function profileDialog(profile, titleText) {
			var p = profile || x.emptyProfile();
			var f = {};

			f.name = input(p.name);
			f.protocol = select(PROTOCOLS, p.protocol);
			f.address = input(p.address);
			f.port = input(p.port, 'number', { min: 1, max: 65535 });
			f.uuid = input(p.uuid);
			f.password = input(p.password);
			f.method = select(SS_METHODS, p.method);
			f.security = select(VMESS_SEC, p.security);
			f.alterId = input(p.alterId || 0, 'number', { min: 0, max: 65535 });
			f.flow = select(FLOWS, p.flow);
			f.transport = select(TRANSPORTS, p.transport);
			f.path = input(p.path);
			f.host = input(p.host);
			f.serviceName = input(p.serviceName);
			f.grpcMode = select([['gun', 'gun'], ['multi', 'multi']], p.grpcMode);
			f.headerType = select([['none', 'none'], ['http', 'http']], p.headerType);
			f.tls = select(TLSMODES, p.tls);
			f.sni = input(p.sni);
			f.alpn = input(p.alpn);
			f.fingerprint = select(FINGERPRINTS, p.fingerprint);
			f.allowInsecure = checkbox(p.allowInsecure);
			f.publicKey = input(p.publicKey);
			f.shortId = input(p.shortId);
			f.spiderX = input(p.spiderX);
			f.mux = checkbox(p.mux);
			f.note = input(p.note);

			var rows = {
				uuid: row(_('User id (UUID)'), f.uuid),
				password: row(_('Password'), f.password),
				method: row(_('Encryption method'), f.method),
				security: row(_('Encryption'), f.security,
				              _('VMess only. "auto" is what every current client sends.')),
				alterId: row(_('alterId'), f.alterId,
				             _('Legacy VMess. Leave at 0 unless the server is old.')),
				flow: row(_('Flow'), f.flow,
				          _('VLESS only. It has to match the server exactly: setting it when \
the server does not pin it fails, and leaving it empty when the server does pin it also \
fails. Both errors blame something else.')),
				path: row(_('Path'), f.path),
				host: row(_('Host header'), f.host),
				serviceName: row(_('gRPC service name'), f.serviceName),
				grpcMode: row(_('gRPC mode'), f.grpcMode),
				headerType: row(_('Header camouflage'), f.headerType),
				sni: row(_('Server name (SNI)'), f.sni,
				         _('For REALITY this is the site the handshake pretends to be, not \
your server.')),
				alpn: row(_('ALPN'), f.alpn, _('Comma separated, usually left empty.')),
				fingerprint: row(_('TLS fingerprint'), f.fingerprint),
				allowInsecure: row(_('Accept an untrusted certificate'), f.allowInsecure,
				                   _('Only for a self-signed certificate on a server you own.')),
				publicKey: row(_('REALITY public key (pbk)'), f.publicKey),
				shortId: row(_('REALITY short id (sid)'), f.shortId),
				spiderX: row(_('REALITY spiderX (spx)'), f.spiderX)
			};

			function refresh() {
				var proto = f.protocol.value, tr = f.transport.value, tls = f.tls.value;
				function show(node, on) { node.style.display = on ? '' : 'none'; }

				show(rows.uuid, proto === 'vless' || proto === 'vmess');
				show(rows.password, proto === 'trojan' || proto === 'shadowsocks');
				show(rows.method, proto === 'shadowsocks');
				show(rows.security, proto === 'vmess');
				show(rows.alterId, proto === 'vmess');
				show(rows.flow, proto === 'vless');

				show(rows.path, tr === 'ws' || tr === 'http' || tr === 'httpupgrade' ||
				                (tr === 'tcp' && f.headerType.value === 'http'));
				show(rows.host, tr === 'ws' || tr === 'http' || tr === 'httpupgrade' ||
				                (tr === 'tcp' && f.headerType.value === 'http'));
				show(rows.serviceName, tr === 'grpc');
				show(rows.grpcMode, tr === 'grpc');
				show(rows.headerType, tr === 'tcp' || tr === 'kcp');

				show(rows.sni, tls !== 'none');
				show(rows.alpn, tls === 'tls');
				show(rows.fingerprint, tls !== 'none');
				show(rows.allowInsecure, tls === 'tls');
				show(rows.publicKey, tls === 'reality');
				show(rows.shortId, tls === 'reality');
				show(rows.spiderX, tls === 'reality');
			}
			f.protocol.addEventListener('change', refresh);
			f.transport.addEventListener('change', refresh);
			f.tls.addEventListener('change', refresh);
			f.headerType.addEventListener('change', refresh);

			function collect() {
				return {
					id: p.id || '',
					name: f.name.value.trim(),
					protocol: f.protocol.value,
					address: f.address.value.trim(),
					port: parseInt(f.port.value, 10) || 0,
					uuid: f.uuid.value.trim(),
					password: f.password.value,
					method: f.method.value,
					security: f.security.value,
					alterId: parseInt(f.alterId.value, 10) || 0,
					flow: f.flow.value,
					encryption: 'none',
					transport: f.transport.value,
					path: f.path.value,
					host: f.host.value.trim(),
					serviceName: f.serviceName.value.trim(),
					grpcMode: f.grpcMode.value,
					headerType: f.headerType.value,
					tls: f.tls.value,
					sni: f.sni.value.trim(),
					alpn: f.alpn.value.trim(),
					fingerprint: f.fingerprint.value,
					allowInsecure: f.allowInsecure.checked,
					publicKey: f.publicKey.value.trim(),
					shortId: f.shortId.value.trim(),
					spiderX: f.spiderX.value.trim(),
					mux: f.mux.checked,
					muxConcurrency: 8,
					note: f.note.value
				};
			}

			showFormModal(titleText || (p.id ? _('Edit profile') : _('New profile')), [
				E('div', { 'class': 'xray-form' }, [
					row(_('Name'), f.name, _('Yours, not the server\'s. It is what the list shows.')),
					row(_('Protocol'), f.protocol),
					row(_('Server address'), f.address,
					    _('A name or an IP address. A name is resolved once when the connection \
comes up, and the address it resolves to is what the firewall exempts.')),
					row(_('Port'), f.port),
					rows.uuid, rows.password, rows.method, rows.security, rows.alterId,
					rows.flow,
					row(_('Transport'), f.transport),
					rows.headerType, rows.path, rows.host, rows.serviceName, rows.grpcMode,
					row(_('Security'), f.tls),
					rows.sni, rows.alpn, rows.fingerprint, rows.allowInsecure,
					rows.publicKey, rows.shortId, rows.spiderX,
					row(_('Multiplexing (mux)'), f.mux,
					    _('Off unless the server asks for it. On this CPU it usually costs more \
than it saves.')),
					row(_('Note'), f.note)
				]),
				E('div', { 'class': 'cbi-page-actions' }, [
					E('button', { 'class': 'cbi-button', 'click': hideFormModal }, _('Cancel')),
					E('button', {
						'class': 'cbi-button cbi-button-action',
						'click': ui.createHandlerFn(this, function () {
							var prof = collect();
							return x.checked(x.api.profileSave(JSON.stringify(prof)),
							                 _('Profile saved.'))
								.then(function () { hideFormModal(); return reload(); })
								.catch(function (e) {
									ui.addNotification(null,
										E('p', {}, String(e.message || e)), 'error');
								});
						})
					}, _('Save'))
				])
			], 'xray-profile-modal');
			refresh();
			f.name.focus();
		}

		function importDialog() {
			var ta = E('textarea', {
				'class': 'cbi-input-textarea', 'rows': 4, 'style': 'width:100%',
				'placeholder': 'vless://…  vmess://…  trojan://…  ss://…'
			});
			var err = E('div', { 'style': 'display:none' });

			showFormModal(_('Add from a link'), [
				E('p', {}, _('Paste the link your server gave you. The fields are filled in \
from it and you can correct anything before saving.')),
				ta, err,
				E('div', { 'class': 'cbi-page-actions' }, [
					E('button', { 'class': 'cbi-button', 'click': hideFormModal }, _('Cancel')),
					E('button', {
						'class': 'cbi-button cbi-button-action',
						'click': function () {
							var p;
							try { p = x.parseUri(ta.value); }
							catch (e) {
								err.style.display = '';
								dom.content(err, E('div', { 'class': 'alert-message warning' },
								                   String(e.message || e)));
								return;
							}
							hideFormModal();
							profileDialog(p, _('Check the imported profile'));
						}
					}, _('Read the link'))
				])
			]);
			ta.focus();
		}

		function showUri(p) {
			var uri;
			try { uri = x.toUri(p); }
			catch (e) { uri = String(e.message || e); }
			var ta = E('textarea', { 'class': 'cbi-input-textarea', 'rows': 4,
			                         'style': 'width:100%' }, uri);
			showFormModal(_('Link for "%s"').format(p.name), [
				E('p', {}, _('This is the same profile written back out as a link, so it can \
be moved to a phone or another router.')),
				ta,
				E('div', { 'class': 'cbi-page-actions' }, [
					E('button', { 'class': 'cbi-button cbi-button-action',
					              'click': hideFormModal }, _('Close'))
				])
			]);
			ta.select();
		}

		/* -------------------------------------------------------------- settings */

		function settingsDialog() {
			var f = {};
			f.mode = select([['vpn', _('VPN mode — everything goes through the tunnel')],
			                 ['proxy', _('Proxy mode — clients point at SOCKS/HTTP themselves')]],
			                cfg.mode);
			/* The interface list is normally worked out on the router, so this offers
			   the choice rather than a box to type into, and shows what the automatic
			   answer came out as. */
			var cap = st.capture || {};
			var manual = String(cfg.lan_ifaces || '').trim() !== '';
			f.capture_mode = select([
				['auto', _('Automatic — everything except the way out to the internet')],
				['manual', _('Only the interfaces I list')]
			], manual ? 'manual' : 'auto');
			f.lan_ifaces = input(cfg.lan_ifaces);

			var capNow = E('div', { 'class': 'cbi-value-description xray-capture' }, [
				E('div', {}, [
					E('strong', {}, _('Capturing right now: ')),
					E('code', {}, cap.ifaces || _('(nothing)')),
					cap.wireless
						? E('span', {}, ' — ' + _('Wi-Fi included, it is bridged into %s')
						                        .format(String(cap.wireless).split(':')[0]))
						: ''
				]),
				E('div', {}, [
					_('The way out is currently '), E('code', {}, cap.uplink || _('(none)')),
					_('. That is the modem, or the combined port when a cable is plugged \
into it — either way it is excluded, and the tunnel does not care which one it is.')
				])
			]);

			var manualRow = row(_('Interfaces to capture'), f.lan_ifaces,
			                    _('Space separated interface names, for example "br-lan". \
Only what you list is captured; everything else leaves the router unproxied.'));

			function refreshCapture() {
				manualRow.style.display = (f.capture_mode.value === 'manual') ? '' : 'none';
			}
			f.capture_mode.addEventListener('change', refreshCapture);
			f.router_traffic = checkbox(cfg.router_traffic === '1');
			f.block_quic = checkbox(cfg.block_quic === '1');
			f.capture_udp = checkbox(cfg.capture_udp === '1');
			f.block_ipv6 = checkbox(cfg.block_ipv6 === '1');
			f.sniffing = checkbox(cfg.sniffing === '1');
			f.set_clock = checkbox(cfg.set_clock === '1');
			f.dns_server = input(cfg.dns_server);
			f.socks_port = input(cfg.socks_port, 'number');
			f.http_port = input(cfg.http_port, 'number');
			f.redirect_port = input(cfg.redirect_port, 'number');
			f.tproxy_port = input(cfg.tproxy_port, 'number');
			f.dns_port = input(cfg.dns_port, 'number');
			f.probe_url = input(cfg.probe_url);
			f.probe_timeout = input(cfg.probe_timeout, 'number');
			f.watchdog_period = input(cfg.watchdog_period, 'number');
			f.watchdog_fails = input(cfg.watchdog_fails, 'number');
			f.loglevel = select(['debug', 'info', 'warning', 'error', 'none'], cfg.loglevel);
			f.api_enabled = checkbox(cfg.api_enabled === '1');

			var tokenField = E('code', { 'class': 'xray-token' },
			                   cfg.api_token || _('(none yet)'));

			/* An API section that only says "there is an API" is no use: these are the
			   real commands for this router, with this router's address and this
			   token already in them, ready to paste. */
			function exampleLines(token) {
				var host = (window.location && window.location.host) || '192.168.1.1';
				var base = 'http://' + host + '/cgi-bin/xray-api';
				var t = token || '<token>';
				var rows = [
					[_('list the profiles'), 'action=profiles'],
					[_('make profile p2 the active one'), 'action=activate&id=p2'],
					[_('connect (starts the same job the button does)'), 'action=connect'],
					[_('disconnect'), 'action=disconnect'],
					[_('how the connect is going'), 'action=job'],
					[_('everything the page shows'), 'action=status']
				];
				var kids = rows.map(function (r) {
					return E('div', { 'class': 'xray-example' }, [
						E('div', { 'class': 'xray-example-what' }, r[0]),
						E('code', {}, "curl -s '" + base + '?' + r[1] + '&token=' + t + "'")
					]);
				});
				kids.push(E('div', { 'class': 'xray-example' }, [
					E('div', { 'class': 'xray-example-what' },
					  _('the token can go in a header instead, to keep it out of the log')),
					E('code', {}, "curl -s -H 'X-Xray-Token: " + t + "' '" +
					              base + "?action=status'")
				]));
				return kids;
			}

			var examples = E('div', { 'class': 'xray-examples' },
			                 exampleLines(cfg.api_token));

			function collect() {
				return {
					mode: f.mode.value,
					/* empty means "work it out", which is what the router's own
					   default is; the manual list is only sent when it is chosen */
					lan_ifaces: (f.capture_mode.value === 'manual')
						? f.lan_ifaces.value.trim() : '',
					router_traffic: f.router_traffic.checked ? '1' : '0',
					block_quic: f.block_quic.checked ? '1' : '0',
					capture_udp: f.capture_udp.checked ? '1' : '0',
					block_ipv6: f.block_ipv6.checked ? '1' : '0',
					sniffing: f.sniffing.checked ? '1' : '0',
					set_clock: f.set_clock.checked ? '1' : '0',
					dns_server: f.dns_server.value.trim(),
					socks_port: f.socks_port.value,
					http_port: f.http_port.value,
					redirect_port: f.redirect_port.value,
					tproxy_port: f.tproxy_port.value,
					dns_port: f.dns_port.value,
					probe_url: f.probe_url.value.trim(),
					probe_timeout: f.probe_timeout.value,
					watchdog_period: f.watchdog_period.value,
					watchdog_fails: f.watchdog_fails.value,
					loglevel: f.loglevel.value,
					api_enabled: f.api_enabled.checked ? '1' : '0'
				};
			}

			showFormModal(_('Settings'), [
				E('div', { 'class': 'xray-form' }, [
					E('h4', {}, _('What "connected" means')),
					row(_('Mode'), f.mode,
					    _('VPN mode redirects the LAN\'s traffic into the tunnel with firewall \
rules, so nothing has to be set on any client. Proxy mode only opens the SOCKS and HTTP \
ports on this router.')),
					row(_('Whose traffic is captured'), f.capture_mode,
					    _('This is about the clients, not about where the internet comes \
from. Left automatic, every network on this router is captured — the LAN ports, both \
Wi-Fi radios, and the router itself — and whichever interface currently reaches the \
internet is excluded, so it works the same on the SIM or on a cable in the combined \
port.')),
					E('div', { 'class': 'cbi-value' }, [
						E('label', { 'class': 'cbi-value-title' }, ''),
						E('div', { 'class': 'cbi-value-field' }, capNow)
					]),
					manualRow,
					row(_('Also capture the router\'s own traffic'), f.router_traffic,
					    _('Updates, opkg and anything else the router itself starts. Xray\'s \
own connection to the server is exempted by its socket mark, which is what keeps this \
from looping.')),
					row(_('Capture UDP as well'), f.capture_udp,
					    _('Off on purpose, and not because the kernel module is missing. \
Measured on this board: Xray reads the captured packet, tunnels it, the far end answers, \
and the reply is never written back — so the traffic disappears instead of going out \
unproxied, which is worse. TCP and DNS are unaffected. Turn it on only to test it.')),
					row(_('Reject QUIC while UDP is not captured'), f.block_quic,
					    _('QUIC is the one thing that would quietly leave unproxied. \
Rejecting it makes browsers fall back to TCP, which does go through the tunnel.')),
					row(_('Reject outbound IPv6'), f.block_ipv6,
					    _('There is no IPv6 tunnel here, so IPv6 must not have a way out either.')),
					row(_('Recover the destination host name (sniffing)'), f.sniffing,
					    _('Redirected traffic only carries an IP address. Sniffing reads the \
host name back out of the first packet, which is what the server needs for TLS.')),

					E('h4', {}, _('DNS and the clock')),
					row(_('Resolver used through the tunnel'), f.dns_server,
					    _('Redirected DNS queries are answered by this server, through the \
tunnel. dnsmasq is left alone, so nothing has to be put back when the tunnel goes down.')),
					row(_('Set the clock from the connection'), f.set_clock,
					    _('This board has no working NTP. VMess refuses any handshake more \
than 90 seconds out and blames the user id when it does, which is the single most \
misleading error in this stack.')),

					E('h4', {}, _('Ports')),
					row('SOCKS', f.socks_port),
					row('HTTP', f.http_port),
					row(_('Transparent TCP'), f.redirect_port),
					row(_('Transparent UDP'), f.tproxy_port),
					row(_('DNS'), f.dns_port),

					E('h4', {}, _('Checking and reconnecting')),
					row(_('Probe URL'), f.probe_url,
					    _('Requested through the router\'s own HTTP port to prove the tunnel \
carries traffic. It has to be plain HTTP: the answer\'s Date header is also what sets the \
clock.')),
					row(_('Probe timeout (s)'), f.probe_timeout),
					row(_('Check every (s)'), f.watchdog_period),
					row(_('Reconnect after this many failed checks'), f.watchdog_fails),
					row(_('Xray log level'), f.loglevel),

					E('h4', {}, _('HTTP API')),
					row(_('Enable the API'), f.api_enabled,
					    _('Four operations for automation: list profiles, activate one, \
connect, disconnect. There is no TLS on this router\'s web server, so the token is only \
as private as the LAN it crosses.')),
					row(_('Token'), E('div', {}, [
						tokenField, ' ',
						x.action(_('Generate a new one'), 'neutral', function () {
							return x.checked(x.api.apiTokenNew(true)).then(function (r) {
								dom.content(tokenField, r.token || '');
								dom.content(examples, exampleLines(r.token || ''));
							});
						})
					])),
					row(_('How to use it'), examples)
				]),
				E('div', { 'class': 'cbi-page-actions' }, [
					E('button', { 'class': 'cbi-button', 'click': hideFormModal }, _('Cancel')),
					E('button', {
						'class': 'cbi-button cbi-button-action',
						'click': ui.createHandlerFn(this, function () {
							return x.checked(x.api.settingsSet(JSON.stringify(collect())),
							                 _('Settings saved.'))
								.then(function () {
									hideFormModal();
									return reload();
								})
								.catch(function (e) {
									ui.addNotification(null,
										E('p', {}, String(e.message || e)), 'error');
								});
						})
					}, _('Save'))
				])
			], 'xray-settings-modal');
			refreshCapture();
		}

		/* ------------------------------------------------------------- rendering */

		function header() {
			var connected = st.running && st.enabled;
			var kids = [];

			var badge = connected
				? x.state(_('Connected'), 'on')
				: x.state(_('Not connected'), 'off');

			var modeLabel = (cfg.mode === 'proxy')
				? x.label(_('Proxy mode'), '')
				: x.label(_('VPN mode'), 'success');

			var active = st.profile_name || _('no profile');

			kids.push(E('div', { 'class': 'xray-hero' }, [
				E('div', { 'class': 'xray-hero-state' }, [
					badge, ' ', modeLabel,
					E('div', { 'class': 'xray-hero-profile' }, active)
				]),
				E('div', { 'class': 'xray-hero-actions' }, [
					connected
						? x.action(_('Disconnect'), 'negative', disconnect)
						: x.action(_('Connect'), 'positive', connect),
					x.action(_('Check now'), 'neutral', function () {
						return x.api.probe().then(function (r) {
							ui.addNotification(null, E('p', {}, r.ok
								? _('The tunnel answered in %d ms (HTTP %d).')
								  .format(r.ms || 0, r.status || 0)
								: _('The check failed: %s').format(r.error || '?')),
								r.ok ? 'info' : 'warning');
						});
					}),
					x.action(_('Settings'), 'neutral', settingsDialog)
				])
			]));

			if (st.profile_count === 0)
				kids.push(E('div', { 'class': 'alert-message warning' }, [
					E('h4', {}, _('There is no profile yet')),
					E('p', {}, _('Add one from a link or by hand, below. Nothing can connect \
until there is one.'))
				]));

			if (!st.binary)
				kids.push(E('div', { 'class': 'alert-message warning' }, [
					E('h4', {}, _('The Xray binary is not installed')),
					E('p', {}, st.share
						? _('The share is mounted but there is no binary on it. Install it with \
"hh71vm-extern-pkg install xray-core" — it is 34 MB and cannot live in the router\'s own \
6 MiB overlay.')
						: _('The /mnt/extern share is not mounted, and the binary lives on it. \
Run "hh71vm-extern-mount status" to see why.'))
				]));

			if (cfg.mode !== 'proxy' && cfg.capture_udp !== '1')
				kids.push(E('div', { 'class': 'alert-message' }, [
					E('h4', {}, _('TCP and DNS go through the tunnel; other UDP does not')),
					E('p', {}, _('UDP capture is switched off on purpose, not for want of a \
kernel module: on this board Xray reads the captured packet and never writes the answer \
back, so that traffic would disappear rather than go out unproxied. QUIC is rejected \
instead, which makes browsers fall back to TCP. DNS is tunnelled either way.'))
				]));

			var res = resultBox(lastResult);
			if (res) kids.push(res);

			kids.push(E('div', { 'class': 'xray-switches' }, [
				switchRow(_('Connect automatically on router power on'),
				          _('The connection comes up by itself after a power cut, before \
anyone logs in.'),
				          st.autostart, function (on) {
					return x.checked(x.api.settingsSet(JSON.stringify(
						{ autostart: on ? '1' : '0' }))).then(fetch);
				}),
				switchRow(_('Reconnect automatically if the connection drops'),
				          _('Checks the tunnel every %s seconds and rebuilds it when it stops \
carrying traffic. This catches the case a process watchdog cannot see: Xray still running, \
port still open, nothing getting through.').format(cfg.watchdog_period || '60'),
				          st.watchdog, function (on) {
					return x.checked(x.api.settingsSet(JSON.stringify(
						{ watchdog: on ? '1' : '0' })))
						.then(function () {
							/* the watchdog is an instance of the same service, so it only
							   appears or goes away on a restart */
							if (st.running) return x.api.connect(function () {});
						})
						.then(fetch);
				})
			]));

			return x.section(_('Connection'), kids);
		}

		function profileTable() {
			var rows = [E('div', { 'class': 'tr table-titles' }, [
				E('div', { 'class': 'th xcol-act' }, _('Active')),
				E('div', { 'class': 'th' }, _('Name')),
				E('div', { 'class': 'th' }, _('Server')),
				E('div', { 'class': 'th' }, _('Kind')),
				E('div', { 'class': 'th right xcol-btn' }, '')
			])];

			(store.profiles || []).forEach(function (p) {
				var radio = E('input', { 'type': 'radio', 'name': 'xray-active' });
				radio.checked = (p.id === store.active);
				radio.addEventListener('change', function () {
					x.checked(x.api.profileActivate(p.id), _('Active profile changed.'))
						.then(function (r) {
							if (r.reconnecting)
								ui.addNotification(null, E('p', {},
									_('Reconnecting through the new profile…')), 'info');
							return reload();
						})
						.catch(function (e) {
							ui.addNotification(null, E('p', {}, String(e.message || e)), 'error');
						});
				});

				rows.push(E('div', { 'class': 'tr' }, [
					E('div', { 'class': 'td xcol-act' }, radio),
					E('div', { 'class': 'td' }, [
						E('strong', {}, p.name || p.id),
						p.note ? E('div', { 'class': 'cbi-value-description' }, p.note) : ''
					]),
					E('div', { 'class': 'td mono' }, String(p.address || '') + ':' + String(p.port || '')),
					E('div', { 'class': 'td' }, x.summary(p)),
					E('div', { 'class': 'td right' },
					  E('div', { 'class': 'mactions', 'style': 'justify-content:flex-end' }, [
						x.action(_('Edit'), 'edit', function () { profileDialog(p); }),
						x.action(_('Link'), 'neutral', function () { showUri(p); }),
						x.action(_('Copy'), 'neutral', function () {
							var c = JSON.parse(JSON.stringify(p));
							c.id = ''; c.name = (p.name || '') + ' (copy)';
							return x.checked(x.api.profileSave(JSON.stringify(c)),
							                 _('Profile duplicated.')).then(reload);
						}),
						x.action(_('Delete'), 'remove', function () {
							return x.checked(x.api.profileDelete(p.id), _('Profile deleted.'))
								.then(reload);
						}, _('Delete the profile "%s"?').format(p.name || p.id))
					]))
				]));
			});

			if ((store.profiles || []).length === 0)
				rows.push(E('div', { 'class': 'tr placeholder' },
				            E('div', { 'class': 'td' }, _('No profiles yet.'))));

			return x.section(_('Profiles'), [
				E('div', { 'class': 'table' }, rows),
				E('div', { 'class': 'mactions' }, [
					x.action(_('Add from a link'), 'add', importDialog),
					x.action(_('Add by hand'), 'add', function () { profileDialog(null); }),
					x.action(_('Reload'), 'neutral', reload)
				])
			], _('Click a profile to make it the active one. If the tunnel is up it moves to \
the new profile immediately.'));
		}

		function diagnostics() {
			var logBox = E('pre', { 'class': 'xray-log' }, _('Not loaded.'));
			var ports = st.ports || {};
			var listening = st.listening || {};
			var cap = st.capture || {};

			function fmtPort(name, port, on) {
				return E('span', {}, [
					String(name) + ' ' + String(port || '–') + ' ',
					on ? x.label(_('listening'), 'success') : x.label(_('closed'))
				]);
			}

			return x.section(_('Diagnostics'), [
				x.facts([
					[_('Service'), st.running
						? _('running, pid %d, %d s').format(st.pid || 0, st.uptime || 0)
						: _('stopped')],
					[_('Memory'), st.rss ? (Math.round(st.rss / 1024) + ' KiB') : null],
					[_('Xray version'), st.version, { mono: true }],
					[_('Binary'), st.binary || _('not found'), { mono: true }],
					[_('Share /mnt/extern'), st.share ? _('mounted') : _('not mounted')],
					[_('Geo assets'), st.assets ? _('present')
						: _('missing — geoip: and geosite: rules would fail')],
					[_('Server address in use'), st.resolved, { mono: true }],
					[_('Traffic captured from'), (cap.ifaces || '–') +
						(cap.wireless ? ' ' + _('(Wi-Fi included)') : '') +
						(cap.mode ? ' — ' + cap.mode : ''), { mono: false }],
					[_('Way out to the internet'), cap.uplink || _('none right now'),
						{ mono: true }],
					[_('Redirection rules'), st.rules ? _('installed') : _('not installed')],
					[_('UDP capture (TPROXY)'), st.tproxy ? _('available') : _('not available')],
					[_('Router clock (UTC)'), st.clock, { mono: true }],
					[_('SOCKS'), null, { raw: fmtPort('', ports.socks, listening.socks) }],
					[_('HTTP'), null, { raw: fmtPort('', ports.http, listening.http) }],
					[_('Transparent TCP'), null,
						{ raw: fmtPort('', ports.redirect, listening.redirect) }]
				]),
				E('div', { 'class': 'mactions' }, [
					x.action(_('Show the log'), 'neutral', function () {
						return x.api.log(80).then(function (r) {
							dom.content(logBox, String((r || {}).log || _('(empty)')));
						});
					}),
					x.action(_('Validate the configuration'), 'neutral', function () {
						return x.api.test().then(function (r) {
							ui.addNotification(null, E('p', {}, r.ok
								? _('The generated configuration is valid.')
								: _('Xray rejected the configuration: %s').format(r.output || '')),
								r.ok ? 'info' : 'warning');
						});
					}),
					x.action(_('Show the firewall rules'), 'neutral', function () {
						return x.api.fwStatus(true).then(function (r) {
							dom.content(logBox, String((r || {}).output || ''));
						});
					})
				]),
				logBox
			], _('What the router itself reports. The log is Xray\'s own error log, which is \
where a failure explains itself when the page cannot.'));
		}

		function draw() {
			dom.content(body, [E('style', { 'type': 'text/css' }, CSS),
			                   header(), profileTable(), diagnostics()]);
			if (window.HH71) window.HH71.decorate(body);
		}

		draw();
		/* A slow refresh only: everything that changes here is changed from this page,
		   and the status call walks /proc. */
		poll.add(function () {
			return fetch().then(function () {
				/* redrawing under an open dialog would close it */
				if (!document.querySelector('.modal')) draw();
			});
		}, 10);

		return body;
	}
});
