'use strict';
'require view';
'require ui';
'require dom';
'require hh71vm.modem as m';

/* The contact list stored on the SIM itself.  Names are read and written in UCS2, so
 * non-Latin entries survive the round trip.
 */

return view.extend({
	handleSave: null,
	handleSaveApply: null,
	handleReset: null,

	load: function () {
		return Promise.all([m.api.status(), m.api.phonebookList(1, 0)]);
	},

	render: function (data) {
		var body = E('div', {});

		function reload() {
			return Promise.all([m.api.status(), m.api.phonebookList(1, 0)])
				.then(function (d) { draw(d[0] || {}, d[1] || {}); });
		}

		function editDialog(entry, freeIndex) {
			var isNew = !entry;
			entry = entry || {};
			var name = E('input', { 'type': 'text', 'value': entry.name || '' });
			var num = E('input', { 'type': 'text', 'value': entry.number || '',
			                       'placeholder': '+380…' });

			function row(t, f, d) {
				return E('div', { 'class': 'cbi-value' }, [
					E('label', { 'class': 'cbi-value-title' }, t),
					E('div', { 'class': 'cbi-value-field' },
					  d ? [f, E('div', { 'class': 'cbi-value-description' }, d)] : f)
				]);
			}

			ui.showModal(isNew ? _('New contact') : _('Edit contact'), [
				row(_('Name'), name),
				row(_('Number'), num, _('Digits, optionally starting with +. \
The SIM limits how long a name and number may be — the modem will refuse an entry that \
does not fit.')),
				E('div', { 'class': 'cbi-page-actions' }, [
					E('button', { 'class': 'cbi-button', 'click': ui.hideModal }, _('Cancel')),
					E('button', { 'class': 'cbi-button cbi-button-action',
						'click': ui.createHandlerFn(this, function () {
							var n = num.value.trim();
							if (!n) return ui.addNotification(null,
								E('p', {}, _('Enter a number.')), 'warning');
							return m.checked(
								m.api.phonebookAdd(isNew ? (freeIndex || 0) : entry.index,
								                   n, name.value.trim()),
								_('Contact saved.'))
								.then(function () { ui.hideModal(); return reload(); })
								.catch(function (e) {
									ui.addNotification(null,
										E('p', {}, String(e.message || e)), 'error');
								});
						}) }, _('Save'))
				])
			]);
			name.focus();
		}

		function draw(st, pb) {
			var entries = pb.entries || [];
			var store = pb.storage || st.phonebook || {};
			var kids = [];
			var warn = m.linkState(st);
			if (warn) kids.push(warn);

			var taken = {};
			entries.forEach(function (e) { taken[e.index] = true; });
			var free = 1;
			while (taken[free] && free < (store.total || 250)) free++;

			var rows = [E('div', { 'class': 'tr table-titles' }, [
				E('div', { 'class': 'th mcol-idx' }, '#'),
				E('div', { 'class': 'th' }, _('Name')),
				E('div', { 'class': 'th' }, _('Number')),
				E('div', { 'class': 'th right mcol-act' }, '')
			])];

			if (!entries.length)
				rows.push(E('div', { 'class': 'tr placeholder' },
				            E('div', { 'class': 'td' }, _('The SIM holds no contacts.'))));

			entries.forEach(function (e) {
				rows.push(E('div', { 'class': 'tr' }, [
					E('div', { 'class': 'td mono' }, String(e.index)),
					E('div', { 'class': 'td' }, e.name || '–'),
					E('div', { 'class': 'td' }, m.copyable(e.number)),
					E('div', { 'class': 'td right' },
					  E('div', { 'class': 'mactions', 'style': 'justify-content:flex-end' }, [
						m.action(_('Edit'), 'edit', function () { editDialog(e); }),
						m.action(_('Delete'), 'remove', function () {
							return m.checked(m.api.phonebookDelete(e.index),
							                 _('Contact deleted.')).then(reload);
						}, _('Delete "%s"?').format(e.name || e.number))
					]))
				]));
			});

			kids.push(E('div', { 'class': 'cbi-section fade-in' }, [
				E('h3', {}, _('SIM contacts')),
				E('div', { 'class': 'cbi-section-descr' },
				  _('Stored on the card, not on the router: they travel with the SIM.')),
				E('div', { 'class': 'table' }, rows),
				E('div', { 'class': 'mactions' }, [
					m.action(_('Add contact'), 'add',
					         function () { editDialog(null, free); }),
					m.action(_('Reload'), 'neutral', reload)
				]),
				m.facts([
					[_('Storage'), store.storage],
					[_('Used'), (store.used != null)
						? store.used + ' / ' + store.total : null]
				])
			]));

			dom.content(body, kids);
			if (window.HH71) window.HH71.decorate(body);
		}

		draw(data[0] || {}, data[1] || {});
		return body;
	}
});
