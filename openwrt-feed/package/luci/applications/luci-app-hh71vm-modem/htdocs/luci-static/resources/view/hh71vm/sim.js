'use strict';
'require view';
'require ui';
'require dom';
'require hh71vm.modem as m';

/* SIM and PIN.
 *
 * Every wrong PIN costs one of three attempts, and three wrong ones lock the card until
 * a PUK is entered; ten wrong PUKs destroy it for good.  So each dialog says plainly
 * what it is about to do, and nothing here retries by itself.
 */

function pinField(placeholder) {
	return E('input', { 'type': 'password', 'inputmode': 'numeric',
	                    'autocomplete': 'off', 'placeholder': placeholder || '····' });
}

function row(title, field, descr) {
	return E('div', { 'class': 'cbi-value' }, [
		E('label', { 'class': 'cbi-value-title' }, title),
		E('div', { 'class': 'cbi-value-field' }, descr
			? [field, E('div', { 'class': 'cbi-value-description' }, descr)]
			: field)
	]);
}

return view.extend({
	handleSave: null,
	handleSaveApply: null,
	handleReset: null,

	load: function () {
		return m.api.status();
	},

	render: function (st) {
		var body = E('div', {});

		function reload() {
			return m.api.simInfo().then(function () {
				return m.api.status().then(draw);
			});
		}

		function dialog(title, fields, warnText, submit) {
			ui.showModal(title, [
				warnText ? E('div', { 'class': 'alert-message warning' },
				             E('p', {}, warnText)) : E([])
			].concat(fields).concat([
				E('div', { 'class': 'cbi-page-actions' }, [
					E('button', { 'class': 'cbi-button', 'click': ui.hideModal }, _('Cancel')),
					E('button', { 'class': 'cbi-button cbi-button-action',
						'click': ui.createHandlerFn(this, function () {
							return Promise.resolve(submit())
								.then(function () { ui.hideModal(); return reload(); })
								.catch(function (e) {
									ui.addNotification(null,
										E('p', {}, String(e.message || e)), 'error');
								});
						}) }, _('Apply'))
				])
			]));
		}

		function unlockPin() {
			var pin = pinField();
			dialog(_('Unlock the SIM'), [row(_('PIN'), pin)],
			       _('Three wrong PINs lock the card until you enter the PUK.'),
			       function () {
					return m.checked(m.api.pinUnlock(pin.value.trim()),
					                 _('SIM unlocked.'));
			       });
			pin.focus();
		}

		function unlockPuk() {
			var puk = pinField('········'), pin = pinField();
			dialog(_('Unlock with PUK'), [
				row(_('PUK'), puk, _('Eight digits, printed on the card holder or given \
by the operator.')),
				row(_('New PIN'), pin, _('Four to eight digits. This becomes the card\'s \
new PIN.'))
			], _('Ten wrong PUK attempts destroy the SIM permanently. Be certain before \
you press Apply.'), function () {
				return m.checked(m.api.pukUnlock(puk.value.trim(), pin.value.trim()),
				                 _('SIM unlocked and the new PIN set.'));
			});
			puk.focus();
		}

		function togglePin(enable) {
			var pin = pinField();
			dialog(enable ? _('Ask for a PIN at power-on') : _('Stop asking for a PIN'),
			       [row(_('Current PIN'), pin)],
			       enable ? null : _('Without a PIN, anyone who takes the card out can use \
it in another device.'),
			       function () {
					return m.checked(m.api.pinEnable(pin.value.trim(), enable),
						enable ? _('PIN protection enabled.')
						       : _('PIN protection disabled.'));
			       });
			pin.focus();
		}

		function changePin() {
			var oldp = pinField(), newp = pinField();
			dialog(_('Change the PIN'), [
				row(_('Current PIN'), oldp),
				row(_('New PIN'), newp, _('Four to eight digits.'))
			], null, function () {
				return m.checked(m.api.pinChange(oldp.value.trim(), newp.value.trim()),
				                 _('PIN changed.'));
			});
			oldp.focus();
		}

		function draw(st) {
			st = st || {};
			var sim = st.sim || {}, net = st.net || {};
			var kids = [];
			var warn = m.linkState(st);
			if (warn) kids.push(warn);

			if (sim.pin_required)
				kids.push(E('div', { 'class': 'alert-message warning' }, [
					E('h4', {}, _('The SIM is waiting for its PIN')),
					E('p', {}, _('No mobile connection is possible until it is unlocked.')),
					E('div', { 'class': 'mactions' },
					  [m.action(_('Enter PIN'), 'action', unlockPin)])
				]));

			if (sim.puk_required)
				kids.push(E('div', { 'class': 'alert-message error' }, [
					E('h4', {}, _('The SIM is locked and needs the PUK')),
					E('p', {}, _('Too many wrong PINs were entered. The PUK comes from your \
operator, usually printed on the plastic the card came in.')),
					E('div', { 'class': 'mactions' },
					  [m.action(_('Enter PUK'), 'action', unlockPuk)])
				]));

			kids.push(m.section(_('Card'), [ m.facts([
				[_('State'), sim.ready ? m.label(sim.status || _('ready'), 'success')
					: m.label(sim.status || _('unknown'), 'warning'), { raw: true }],
				[_('Initialisation'), sim.init],
				[_('IMSI'), sim.imsi, { copy: true, mono: true }],
				[_('ICCID'), sim.iccid, { copy: true, mono: true }],
				[_('Own number'), sim.number, { copy: true }],
				[_('Home network'), sim.mcc
					? _('MCC %s').format(sim.mcc) : null],
				[_('Registered on'), net.operator_numeric
					? net.operator + ' (' + net.operator_numeric + ')' : net.operator],
				[_('Lock states (raw)'), sim.pinstat, { mono: true, copy: true }]
			]) ]));

			kids.push(m.section(_('PIN'), [
				m.facts([
					[_('Asked for at power-on'), sim.pin_enabled
						? m.label(_('yes'), 'notice') : m.label(_('no')), { raw: true }]
				]),
				E('div', { 'class': 'mactions' }, [
					sim.pin_enabled
						? m.action(_('Turn PIN off'), 'negative',
						           function () { togglePin(false); })
						: m.action(_('Turn PIN on'), 'action',
						           function () { togglePin(true); }),
					m.action(_('Change PIN'), 'neutral', changePin),
					m.action(_('Unlock with PIN'), 'neutral', unlockPin),
					m.action(_('Unlock with PUK'), 'neutral', unlockPuk)
				]),
				E('div', { 'class': 'cbi-value-description' },
				  _('The modem does not report how many attempts are left. Three wrong \
PINs lock the card until the PUK is entered; ten wrong PUKs destroy it.'))
			]));

			dom.content(body, kids);
			if (window.HH71) window.HH71.decorate(body);
		}

		draw(st);
		return body;
	}
});
