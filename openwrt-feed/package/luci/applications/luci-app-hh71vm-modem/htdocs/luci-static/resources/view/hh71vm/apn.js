'use strict';
'require view';
'require ui';
'require dom';
'require hh71vm.modem as m';

/* Connection profiles (PDP contexts).  The modem keeps up to 24; profile 1 is the one
 * the router's data connection normally runs on, so editing it is possible but flagged.
 */

var PDP_TYPES = ['IPV4V6', 'IP', 'IPV6'];
var AUTH_TYPES = [[0, _('none')], [1, 'PAP'], [2, 'CHAP'], [3, _('PAP or CHAP')]];

return view.extend({
	handleSave: null,
	handleSaveApply: null,
	handleReset: null,

	load: function () {
		return Promise.all([m.api.status(), m.api.apnList()]);
	},

	render: function (data) {
		var body = E('div', {});

		function reload() {
			return Promise.all([m.api.status(), m.api.apnList()]).then(function (d) {
				draw(d[0] || {}, d[1] || {});
			});
		}

		function editDialog(entry, usedCids) {
			var isNew = !entry;
			entry = entry || {};

			var freeCid = 2;
			while (usedCids[freeCid] && freeCid < 24) freeCid++;

			var cid = E('input', { 'type': 'number', 'min': 1, 'max': 24,
			                       'value': String(entry.cid || freeCid),
			                       'disabled': isNew ? null : 'disabled' });
			var apn = E('input', { 'type': 'text', 'value': entry.apn || '' });
			var pdp = E('select', {});
			PDP_TYPES.forEach(function (t) {
				pdp.appendChild(E('option', { 'value': t,
					'selected': (entry.pdp_type === t) ? 'selected' : null }, t));
			});
			var auth = E('select', {});
			AUTH_TYPES.forEach(function (a) {
				auth.appendChild(E('option', { 'value': String(a[0]),
					'selected': (entry.auth === a[0]) ? 'selected' : null }, a[1]));
			});
			var user = E('input', { 'type': 'text', 'value': '' });
			var pass = E('input', { 'type': 'password', 'value': '' });

			function row(title, field, descr) {
				return E('div', { 'class': 'cbi-value' }, [
					E('label', { 'class': 'cbi-value-title' }, title),
					E('div', { 'class': 'cbi-value-field' }, descr
						? [field, E('div', { 'class': 'cbi-value-description' }, descr)]
						: field)
				]);
			}

			ui.showModal(isNew ? _('New profile') : _('Edit profile %d').format(entry.cid), [
				row(_('Profile number'), cid,
				    _('The modem calls this the context id (cid). Profile 1 is the one the \
router uses for its own connection.')),
				row(_('APN'), apn, _('The access point name your operator gave you. \
Leaving it empty asks the network for its default.')),
				row(_('IP version'), pdp),
				row(_('Authentication'), auth),
				row(_('Username'), user),
				row(_('Password'), pass,
				    _('Credentials are write-only: the modem never reports them back, so \
these two fields always start empty.')),
				E('div', { 'class': 'cbi-page-actions' }, [
					E('button', { 'class': 'cbi-button', 'click': ui.hideModal }, _('Cancel')),
					E('button', { 'class': 'cbi-button cbi-button-action',
						'click': ui.createHandlerFn(this, function () {
							var n = parseInt(cid.value, 10);
							if (!(n >= 1 && n <= 24))
								return ui.addNotification(null,
									E('p', {}, _('Profile number must be 1 to 24.')),
									'warning');
							return m.checked(m.api.apnSet(n, apn.value.trim(), pdp.value,
							                              parseInt(auth.value, 10),
							                              user.value, pass.value),
							                 _('Profile saved.'))
								.then(function () { ui.hideModal(); return reload(); })
								.catch(function (e) {
									ui.addNotification(null,
										E('p', {}, String(e.message || e)), 'error');
								});
						}) }, _('Save'))
				])
			]);
			apn.focus();
		}

		function draw(st, list) {
			var entries = list.list || (st.apn || {}).list || [];
			var data = st.data || {};
			var used = {};
			entries.forEach(function (e) { used[e.cid] = true; });

			var kids = [];
			var warn = m.linkState(st);
			if (warn) kids.push(warn);

			var rows = [E('div', { 'class': 'tr table-titles' }, [
				E('div', { 'class': 'th mcol-idx' }, _('Profile')),
				E('div', { 'class': 'th' }, _('APN')),
				E('div', { 'class': 'th mcol-sm' }, _('IP version')),
				E('div', { 'class': 'th mcol-sm' }, _('Auth')),
				E('div', { 'class': 'th mcol-sm' }, _('State')),
				E('div', { 'class': 'th right mcol-act' }, '')
			])];

			entries.forEach(function (e) {
				var acts = [
					m.action(_('Edit'), 'edit', function () { editDialog(e, used); })
				];
				if (e.active)
					acts.push(m.action(_('Disconnect'), 'negative', function () {
						return m.checked(m.api.dataDisconnect(e.cid),
						                 _('Context %d deactivated.').format(e.cid))
							.then(reload);
					}, _('This ends the data session on profile %d. Continue?').format(e.cid)));
				else if (e.apn)
					acts.push(m.action(_('Connect'), 'positive', function () {
						return m.checked(m.api.dataConnect(e.cid),
						                 _('Context %d activated.').format(e.cid))
							.then(reload);
					}));
				if (e.cid !== 1)
					acts.push(m.action(_('Delete'), 'remove', function () {
						return m.checked(m.api.apnDelete(e.cid), _('Profile cleared.'))
							.then(reload);
					}, _('Clear profile %d?').format(e.cid)));

				rows.push(E('div', { 'class': 'tr' }, [
					E('div', { 'class': 'td mono' }, String(e.cid)),
					E('div', { 'class': 'td' }, e.apn
						? m.copyable(e.apn)
						: E('em', { 'style': 'color:var(--muted)' }, _('(from the network)'))),
					E('div', { 'class': 'td' }, e.pdp_type || '–'),
					E('div', { 'class': 'td' }, e.auth_name || '–'),
					E('div', { 'class': 'td' }, e.active
						? m.label(_('active'), 'success') : m.label(_('idle'))),
					E('div', { 'class': 'td right' },
					  E('div', { 'class': 'mactions', 'style': 'justify-content:flex-end' },
					    acts))
				]));
			});

			kids.push(E('div', { 'class': 'cbi-section fade-in' }, [
				E('h3', {}, _('Connection profiles')),
				E('div', { 'class': 'cbi-section-descr' },
				  _('The modem stores these itself, so they survive a reboot of the router. \
The profile carrying the current connection is marked active.')),
				E('div', { 'class': 'table' }, rows),
				E('div', { 'class': 'mactions' }, [
					m.action(_('Add profile'), 'add', function () { editDialog(null, used); }),
					m.action(_('Reload'), 'neutral', reload)
				])
			]));

			kids.push(m.section(_('Current connection'), [ m.facts([
				[_('Profile in use'), data.profile_cid],
				[_('APN'), data.apn, { copy: true }],
				[_('IP version'), data.pdp_type],
				[_('IPv4 address'), data.ipv4, { copy: true, mono: true }],
				[_('IPv6 address'), data.ipv6, { copy: true, mono: true }],
				[_('Gateway'), data.gw, { copy: true, mono: true }],
				[_('DNS servers'), (data.dns || []).concat(data.dns6 || []).join(', ') || null,
					{ copy: true, mono: true }]
			]) ]));

			dom.content(body, kids);
			if (window.HH71) window.HH71.decorate(body);
		}

		draw(data[0] || {}, data[1] || {});
		return body;
	}
});
