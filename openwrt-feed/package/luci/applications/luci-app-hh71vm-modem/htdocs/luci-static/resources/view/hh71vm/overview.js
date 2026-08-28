'use strict';
'require view';
'require ui';
'require poll';
'require dom';
'require hh71vm.modem as m';

/* Modem overview: the whole radio state on one page.  Nothing is hidden behind a
 * "details" toggle -- the technical values are the point of the page.
 */

function tile(caption, value, sub, extra) {
	return E('div', { 'class': 'mtile' }, [
		E('div', { 'class': 'mtile-cap' }, caption),
		E('div', { 'class': 'mtile-val' }, value),
		E('div', { 'class': 'mtile-sub' }, sub || ' '),
		extra || E([])
	]);
}

function tiles(st) {
	var sig = st.signal || {}, net = st.net || {}, data = st.data || {},
	    use = st.usage || {}, sms = st.sms || {};

	var lvl = (sig.rsrp != null) ? sig.rsrp + ' dBm' :
	          (sig.rssi_dbm != null ? sig.rssi_dbm + ' dBm' : '–');

	return E('div', { 'class': 'mtiles' }, [
		tile(_('Signal'), E('span', { 'class': 'mtile-sig' }, [
			m.signalBars(sig.bars), E('span', {}, lvl)
		]), sig.rsrp != null ? 'RSRP · RSRQ ' + (sig.rsrq != null ? sig.rsrq + ' dB' : '–')
		                     : _('RSSI')),
		tile(_('Operator'), net.operator || _('not registered'),
		     [net.act_name || net.sysmode || '', net.operator_numeric || '']
		         .filter(Boolean).join(' · ') || ' '),
		tile(_('WAN address'), data.ipv4 || '–',
		     data.apn ? _('APN') + ' ' + data.apn : ' '),
		tile(_('Traffic'), '↓ ' + m.bytes(use.rx) + '  ↑ ' + m.bytes(use.tx),
		     use.since ? _('since %s').format(new Date(use.since * 1000).toLocaleString())
		               : ' '),
		/* sms.count is messages; sms.used is storage slots, and a long text takes
		   several of them -- showing the slot count here read as "6 messages" when
		   there were two. */
		tile(_('Messages'), (sms.count != null) ? String(sms.count) : '–',
		     sms.unread ? _('%d unread').format(sms.unread)
		                : (sms.used != null
		                   ? _('%d of %d storage slots used').format(sms.used, sms.total || 0)
		                   : _('no new messages')))
	]);
}

/* The modem reports a different set of neighbours on nearly every poll, and the
   table shrinking mid-refresh made everything below it jump.  Keep the block as tall
   as the most rows seen so far and pad the rest, so only the values move. */
var neighbourRows = 0;

function neighbourTable(list) {
	list = list || [];
	if (list.length > neighbourRows)
		neighbourRows = list.length;
	if (!neighbourRows)
		return E('div', { 'class': 'cbi-value-description' }, _('No neighbour cells reported.'));
	var rows = [E('div', { 'class': 'tr table-titles' }, [
		E('div', { 'class': 'th' }, _('PCI')),
		E('div', { 'class': 'th' }, _('EARFCN')),
		E('div', { 'class': 'th' }, _('Band')),
		E('div', { 'class': 'th' }, _('RSRP'))
	])];
	for (var i = 0; i < neighbourRows; i++) {
		var n = list[i];
		rows.push(E('div', { 'class': 'tr' }, [
			E('div', { 'class': 'td' }, n ? String(n.pci) : '–'),
			E('div', { 'class': 'td' }, n ? String(n.earfcn) : '–'),
			E('div', { 'class': 'td' }, (n && n.band) ? String(n.band) : '–'),
			E('div', { 'class': 'td' }, n ? (n.rsrp + ' dBm') : '–')
		]));
	}
	return E('div', { 'class': 'table' }, rows);
}

return view.extend({
	handleSave: null,
	handleSaveApply: null,
	handleReset: null,

	load: function () {
		return m.api.status();
	},

	render: function (st) {
		st = st || {};
		var self = this;

		var body = E('div', { 'id': 'modem-overview' });

		function draw(st) {
			var sig = st.signal || {}, net = st.net || {}, data = st.data || {},
			    sim = st.sim || {}, dev = st.device || {}, radio = st.radio || {},
			    link = st.link || {}, use = st.usage || {};

			var kids = [];
			var warn = m.linkState(st);
			if (warn) kids.push(warn);

			kids.push(tiles(st));

			kids.push(E('div', { 'class': 'cbi-section fade-in' }, [
				E('h3', {}, _('Control')),
				E('div', { 'class': 'mactions' }, [
					m.action(_('Refresh now'), 'action', function () {
						return m.checked(m.api.refresh()).then(function (res) {
							draw(res);
						});
					}),
					m.action(radio.on === false ? _('Enable radio') : _('Disable radio'),
					         radio.on === false ? 'positive' : 'negative',
						function () {
							return m.checked(m.api.radioSet(radio.on === false))
								.then(refresh);
						},
						radio.on === false ? null
						  : _('Turning the radio off drops the mobile connection. Continue?')),
					m.action(data.connected ? _('Disconnect data') : _('Connect data'),
					         data.connected ? 'negative' : 'positive',
						function () {
							var fn = data.connected ? m.api.dataDisconnect : m.api.dataConnect;
							return m.checked(fn(data.profile_cid || 1)).then(refresh);
						},
						data.connected
						  ? _('This ends the data session. Continue?') : null),
					m.action(_('Reconnect control channel'), 'neutral', function () {
						return m.checked(m.api.reconnect()).then(function () {
							ui.addNotification(null, E('p', {},
								_('Reconnecting; this takes a few seconds.')), 'info');
						});
					})
				])
			]));

			kids.push(m.section(_('Radio'), [ m.facts([
				[_('Signal quality'), E('span', { 'class': 'mtile-sig' },
					[m.signalBars(sig.bars), E('span', {},
						_('%d of 5').format(sig.bars || 0))]), { raw: true }],
				[_('RSRP'), sig.rsrp != null ? sig.rsrp + ' dBm' : null],
				[_('RSRQ'), sig.rsrq != null ? sig.rsrq + ' dB' : null],
				[_('RSSI'), sig.rssi_dbm != null ? sig.rssi_dbm + ' dBm' : null],
				[_('CSQ'), sig.csq != null ? sig.csq + ' / 31' : null],
				[_('Band'), sig.band ? 'LTE B' + sig.band : null],
				[_('EARFCN'), sig.earfcn],
				[_('Physical cell id'), sig.pci],
				[_('Cell id'), net.ci, { copy: true, mono: true }],
				[_('eNodeB / sector'), (net.enb != null)
					? net.enb + ' / ' + net.sector : null],
				[_('Tracking area'), net.tac, { mono: true }]
			]), E('h4', { 'style': 'margin-top:16px' }, _('Neighbour cells')),
			   neighbourTable(sig.neighbours) ]));

			kids.push(m.section(_('Network'), [ m.facts([
				[_('Operator'), net.operator, { copy: true }],
				[_('Operator code'), net.operator_numeric
					? net.operator_numeric + '  (MCC ' + net.mcc + ', MNC ' + net.mnc + ')'
					: null],
				[_('Registration'), net.reg_name],
				[_('Roaming'), net.registered ? (net.roaming ? _('yes') : _('no')) : null],
				[_('Access technology'), net.act_name || net.sysmode],
				[_('Selection'), net.auto === false ? _('manual') : _('automatic')],
				[_('Allowed technologies'), net.mode_name],
				[_('Packet service'), net.attached ? _('attached') : _('detached')],
				[_('Network time'), m.smsTime(net.time)]
			]) ]));

			kids.push(m.section(_('Data connection'), [ m.facts([
				[_('State'), data.connected
					? m.label(_('connected'), 'success') : m.label(_('down'), 'warning'),
					{ raw: true }],
				[_('APN'), data.apn, { copy: true }],
				[_('PDP type'), data.pdp_type],
				[_('Profile'), data.profile_cid],
				[_('IPv4 address'), data.ipv4, { copy: true, mono: true }],
				[_('IPv6 address'), data.ipv6, { copy: true, mono: true }],
				[_('Gateway'), data.gw, { copy: true, mono: true }],
				[_('DNS servers'), (data.dns || []).concat(data.dns6 || []).join(', ')
					|| null, { copy: true, mono: true }]
			]) ]));

			kids.push(m.section(_('Traffic'), [
				m.facts([
					[_('Received'), m.bytes(use.rx) + '  (' + _('total') + ' ' +
						m.bytes(use.rx_total) + ')'],
					[_('Sent'), m.bytes(use.tx) + '  (' + _('total') + ' ' +
						m.bytes(use.tx_total) + ')'],
					[_('Counting since'), use.since
						? new Date(use.since * 1000).toLocaleString() : null],
					[_('Interface'), use.iface, { mono: true }]
				]),
				E('div', { 'class': 'mactions' }, [
					m.action(_('Reset counter'), 'neutral', function () {
						return m.checked(m.api.usageReset()).then(refresh);
					})
				]),
				E('div', { 'class': 'cbi-value-description' },
				  _('Counted from the WAN interface of the router, not from the network: \
the operator may count differently.'))
			]));

			kids.push(m.section(_('SIM'), [ m.facts([
				[_('State'), sim.status],
				[_('PIN protection'), sim.pin_enabled ? _('enabled') : _('disabled')],
				[_('IMSI'), sim.imsi, { copy: true, mono: true }],
				[_('ICCID'), sim.iccid, { copy: true, mono: true }],
				[_('Own number'), sim.number, { copy: true }],
				[_('Initialisation'), sim.init]
			]) ]));

			kids.push(m.section(_('Modem'), [ m.facts([
				[_('Manufacturer'), dev.vendor],
				[_('Model'), dev.model],
				[_('Firmware'), dev.revision, { copy: true, mono: true }],
				[_('IMEI'), dev.imei, { copy: true, mono: true }],
				[_('Radio'), radio.on === false
					? m.label(_('off'), 'warning') : m.label(_('on'), 'success'),
					{ raw: true }],
				[_('Control channel'), link.state === 'ready'
					? m.label(_('ready'), 'success')
					: m.label(link.state || '?', 'danger'), { raw: true }],
				[_('Channel up since'), link.since
					? new Date(link.since * 1000).toLocaleString() : null]
			]) ]));

			dom.content(body, kids);
			if (window.HH71) window.HH71.decorate(body);
		}

		function refresh() {
			return m.api.status().then(draw);
		}

		draw(st);
		poll.add(refresh, 5);

		return body;
	}
});
