'use strict';
'require baseclass';
'require rpc';

/* Modem block for the LuCI status dashboard.
 *
 * luci-mod-status renders every file it finds in this directory, so dropping this in is
 * all that is needed.  Returning null hides the block, which is what happens when the
 * modem daemon is not there -- the app has to survive being installed without it.
 */

var callModemStatus = rpc.declare({ object: 'hh71vm-modem', method: 'status' });

function bars(n) {
	var cls = n >= 4 ? '' : (n >= 2 ? 'weak' : 'bad');
	var el = E('span', { 'class': 'bars ' + cls });
	for (var i = 1; i <= 5; i++)
		el.appendChild(E('i', i <= n ? { 'class': 'on' } : {}));
	return el;
}

/* The link used to be a table row with an empty label cell, which parked it a third
   of the way across the card.  It is not data, so it lives under the table. */
function more(text) {
	return E('div', { 'class': 'card-more' },
	         E('a', { 'href': L.url('admin/modem/overview') }, text + ' »'));
}

function row(label, value) {
	return E('div', { 'class': 'tr' }, [
		E('div', { 'class': 'td left', 'style': 'width:33%' }, label),
		E('div', { 'class': 'td left' }, value)
	]);
}

return baseclass.extend({
	title: _('Mobile network'),

	load: function () {
		return L.resolveDefault(callModemStatus(), null);
	},

	render: function (st) {
		if (!L.isObject(st) || !L.isObject(st.link)) return null;

		var link = st.link || {}, net = st.net || {}, sig = st.signal || {},
		    data = st.data || {}, sim = st.sim || {}, radio = st.radio || {},
		    usage = st.usage || {};

		if (link.state !== 'ready')
			return E('div', { 'class': 'table' }, [
				row(_('Control channel'),
				    E('span', { 'class': 'label warning' }, link.state || 'down')),
				row(_('Detail'), link.error || '–')
			]);

		if (radio.on === false)
			return E([], [
				E('div', { 'class': 'table' }, [
					row(_('Radio'), E('span', { 'class': 'label warning' }, _('disabled')))
				]),
				more(_('Modem settings'))
			]);

		var level = (sig.rsrp != null) ? sig.rsrp + ' dBm RSRP'
		          : (sig.rssi_dbm != null ? sig.rssi_dbm + ' dBm RSSI' : '–');

		return E([], [ E('div', { 'class': 'table' }, [
			row(_('Operator'), '%s%s'.format(net.operator || _('not registered'),
			    (net.roaming && net.registered) ? ' (' + _('roaming') + ')' : '')),
			row(_('Technology'), '%s%s'.format(net.act_name || net.sysmode || '–',
			    sig.band ? ', ' + _('band') + ' ' + sig.band : '')),
			row(_('Signal'), E('span', { 'class': 'mtile-sig' },
			    [bars(sig.bars || 0), E('span', {}, level)])),
			row(_('SIM'), sim.status || '–'),
			row(_('Address'), data.ipv4 || '–'),
			row(_('Traffic'), '%s %s / %s %s'.format(
				'↓', String.format('%1024.2mB', usage.rx || 0),
				'↑', String.format('%1024.2mB', usage.tx || 0)))
		]), more(_('Modem details')) ]);
	}
});
