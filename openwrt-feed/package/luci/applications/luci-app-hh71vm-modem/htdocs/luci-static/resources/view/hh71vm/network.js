'use strict';
'require view';
'require ui';
'require dom';
'require hh71vm.modem as m';

/* Network: which technologies the modem may use, which operator it is on, and USSD.
 *
 * Scanning for operators is a deliberate button press and nothing else -- AT+COPS=?
 * takes up to a minute and a half and drops the data connection while it runs.
 */

return view.extend({
	handleSave: null,
	handleSaveApply: null,
	handleReset: null,

	load: function () {
		return Promise.all([m.api.status(), m.api.netMode()]);
	},

	render: function (data) {
		var body = E('div', {});

		function reload() {
			return Promise.all([m.api.status(), m.api.netMode()]).then(function (d) {
				draw(d[0] || {}, d[1] || {});
			});
		}

		/* The scan is a job in the daemon, not a long call: AT+COPS=? runs for over a
		   minute and LuCI's ubus proxy gives up at thirty seconds, so the page starts
		   it and then asks for the result until it is there. */
		function scanDialog() {
			var results = E('div', { 'class': 'spinning' }, _('Starting the scan…'));
			var poller = null, stopped = false;

			function stop() { stopped = true; if (poller) { clearTimeout(poller); poller = null; } }

			ui.showModal(_('Available networks'), [
				E('p', {}, _('The modem is asking every band it supports who is there. This takes up to about a minute and a half, and the data connection is unavailable while it runs.')),
				results,
				E('div', { 'class': 'cbi-page-actions' }, [
					E('button', { 'class': 'cbi-button', 'click': function () {
						stop(); ui.hideModal();
					} }, _('Close'))
				])
			]);

			function showNetworks(nets) {
				var STAT = { 0: _('unknown'), 1: _('available'), 2: _('current'),
				             3: _('forbidden') };
				var rows = [E('div', { 'class': 'tr table-titles' }, [
					E('div', { 'class': 'th' }, _('Operator')),
					E('div', { 'class': 'th' }, _('Code')),
					E('div', { 'class': 'th' }, _('Technology')),
					E('div', { 'class': 'th' }, _('State')),
					E('div', { 'class': 'th' }, '')
				])];
				nets.forEach(function (n) {
					rows.push(E('div', { 'class': 'tr' }, [
						E('div', { 'class': 'td' }, n.long || n.short || '?'),
						E('div', { 'class': 'td mono' }, n.numeric || '?'),
						E('div', { 'class': 'td' }, n.act_name || '–'),
						E('div', { 'class': 'td' }, STAT[n.stat] || String(n.stat)),
						E('div', { 'class': 'td cbi-section-actions' }, n.stat === 3 ? E([]) :
							m.action(_('Register'), 'action', function () {
								return m.checked(
									m.api.netRegister(n.numeric, n.act, false),
									_('Registering on %s…').format(n.long || n.numeric))
									.then(function () { stop(); ui.hideModal(); return reload(); });
							}))
					]));
				});
				results.className = '';
				dom.content(results, E('div', { 'class': 'table' }, rows));
			}

			function poll() {
				if (stopped) return;
				m.api.netScanResult().then(function (res) {
					res = res || {};
					if (stopped) return;
					if (res.state === 'running') {
						dom.content(results, E('em', { 'class': 'spinning' },
							_('Scanning for networks… (%ds)').format(res.elapsed || 0)));
						poller = setTimeout(poll, 3000);
						return;
					}
					if (res.state === 'failed' || res.error) {
						results.className = '';
						dom.content(results, E('p', { 'class': 'cbi-value-description' },
							res.error || _('The scan failed.')));
						return;
					}
					var nets = res.networks || [];
					if (!nets.length) {
						results.className = '';
						dom.content(results, E('p', { 'class': 'cbi-value-description' },
							_('The modem returned no networks.')));
						return;
					}
					showNetworks(nets);
				}).catch(function (e) {
					if (stopped) return;
					results.className = '';
					dom.content(results, E('p', { 'class': 'cbi-value-description' },
						String(e.message || e)));
				});
			}

			m.api.netScan().then(function () {
				if (!stopped) poller = setTimeout(poll, 1500);
			}).catch(function (e) {
				results.className = '';
				dom.content(results, E('p', { 'class': 'cbi-value-description' },
					String(e.message || e)));
			});
		}

		function ussdDialog() {
			var code = E('input', { 'type': 'text', 'placeholder': '*111#' });
			var out = E('div', { 'class': 'at-out', 'style': 'display:none' });
			ui.showModal(_('USSD request'), [
				E('div', { 'class': 'cbi-value' }, [
					E('label', { 'class': 'cbi-value-title' }, _('Code')),
					E('div', { 'class': 'cbi-value-field' }, [code,
						E('div', { 'class': 'cbi-value-description' },
						  _('Service codes such as *111# — exactly as your operator \
publishes them. Some codes cost money.'))])
				]),
				out,
				E('div', { 'class': 'cbi-page-actions' }, [
					E('button', { 'class': 'cbi-button', 'click': ui.hideModal }, _('Close')),
					E('button', { 'class': 'cbi-button cbi-button-action',
						'click': ui.createHandlerFn(this, function () {
							var c = code.value.trim();
							if (!c) return;
							out.style.display = '';
							out.textContent = _('Waiting for the network…');
							/* The daemon runs this as a job and we poll it, so a slow network no
							   longer ends as a timeout at the 30 s /admin/ubus ceiling. */
							return m.api.ussdRun(c, function (sec) {
								out.textContent = sec > 3
									? _('Waiting for the network… %ds').format(sec)
									: _('Waiting for the network…');
							}).then(function (res) {
								res = res || {};
								out.textContent = res.text || res.error ||
									_('No answer from the network.');
							}).catch(function (e) {
								out.textContent = String(e.message || e);
							});
						}) }, _('Send'))
				])
			]);
			code.focus();
		}

		function draw(st, mode) {
			var net = st.net || {}, sig = st.signal || {};
			var kids = [];
			var warn = m.linkState(st);
			if (warn) kids.push(warn);

			kids.push(m.section(_('Registration'), [ m.facts([
				[_('Operator'), net.operator, { copy: true }],
				[_('Operator code'), net.operator_numeric, { copy: true, mono: true }],
				[_('State'), net.reg_name],
				/* Roaming has no meaning while the modem is not registered on a
				   network: the flag there is whatever it held last. */
				[_('Roaming'), net.registered
					? (net.roaming ? m.label(_('yes'), 'warning')
					               : m.label(_('no'), 'success'))
					: E('span', {}, '–'), { raw: true }],
				[_('Technology'), net.act_name || net.sysmode],
				[_('Band'), sig.band ? 'LTE B' + sig.band : null],
				[_('Packet service'), net.attached ? _('attached') : _('detached')],
				[_('Cell / tracking area'), net.ci ? net.ci + ' / ' + net.tac : null,
					{ mono: true }]
			]) ]));

			/* --- allowed technologies (kcap SetNetworkSettings) --- */
			var modes = (mode.modes && mode.modes.length) ? mode.modes : (net.modes || []);
			var sel = E('select', {});
			modes.forEach(function (o) {
				sel.appendChild(E('option', {
					'value': String(o.value),
					'selected': (o.value === (mode.mode != null ? mode.mode : net.mode))
						? 'selected' : null
				}, o.name));
			});
			if (!modes.length)
				sel.appendChild(E('option', {}, _('the modem reported no choices')));

			kids.push(m.section(_('Allowed technologies'), [
				E('div', { 'class': 'cbi-value' }, [
					E('label', { 'class': 'cbi-value-title' }, _('Mode')),
					E('div', { 'class': 'cbi-value-field' }, [ sel,
						E('div', { 'class': 'cbi-value-description' },
						  _('Restricting this can make the connection more stable in a \
weak-signal spot, at the cost of speed. "Automatic" lets the modem choose.')),
						E('div', { 'class': 'cbi-value-description' },
						  _('Applying a mode makes the modem re-register, so the connection drops for a \
few seconds. The mode that actually took effect is read back afterwards and \
shown here.'))])
				]),
				E('div', { 'class': 'mactions' }, [
					m.action(_('Apply'), 'action', function () {
						/* Redraw either way: on a refusal the dropdown is still showing
						   the mode that was asked for, and leaving it there next to an
						   error reads as though it had been applied. */
						return m.checked(m.api.netModeSet(parseInt(sel.value, 10)),
						                 _('Mode applied.'))
							.then(reload, function (e) { reload(); throw e; });
					})
				])
			]));

			/* --- operator selection --- */
			kids.push(m.section(_('Operator selection'), [
				m.facts([
					[_('Current mode'), (net.auto === false) ? _('manual') : _('automatic')]
				]),
				E('div', { 'class': 'mactions' }, [
					m.action(_('Search for networks'), 'neutral', scanDialog),
					m.action(_('Return to automatic'), 'action', function () {
						return m.checked(m.api.netRegister('', 0, true),
						                 _('Automatic selection restored.')).then(reload);
					})
				]),
				E('div', { 'class': 'cbi-value-description' },
				  _('A manual search takes up to about a minute and a half and interrupts \
the mobile data connection while it runs.'))
			]));

			/* --- USSD --- */
			kids.push(m.section(_('Service codes (USSD)'), [
				E('div', { 'class': 'mactions' }, [
					m.action(_('Send a USSD code'), 'neutral', ussdDialog)
				]),
				E('div', { 'class': 'cbi-value-description' },
				  _('Balance enquiries and similar operator codes.'))
			]));

			/* --- what the modem says it supports --- */
			if (mode.bands && mode.bands.length)
				kids.push(m.section(_('Band preferences reported by the modem'), [
					E('div', { 'class': 'at-out' }, mode.bands.join('\n')),
					E('div', { 'class': 'cbi-value-description' },
					  _('Read-only: this firmware exposes no command to change the band \
list. The serving LTE band is shown under Registration.'))
				]));

			dom.content(body, kids);
			if (window.HH71) window.HH71.decorate(body);
		}

		draw(data[0] || {}, data[1] || {});
		return body;
	}
});
