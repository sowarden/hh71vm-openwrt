'use strict';
'require view';
'require rpc';
'require hh71vm.modem as m';

/* About / legal.  Linked from the footer of every page. */

var SOURCE_URL = 'https://github.com/sowarden/hh71vm-openwrt';

var COMPONENTS = [
	['Linux kernel', 'GPL-2.0-only', 'https://kernel.org'],
	['OpenWrt', 'GPL-2.0-only and others, per package', 'https://openwrt.org'],
	['LuCI web interface', 'Apache-2.0', 'https://github.com/openwrt/luci'],
	['BusyBox', 'GPL-2.0-only', 'https://busybox.net'],
	['musl libc', 'MIT', 'https://musl.libc.org'],
	['dnsmasq', 'GPL-2.0-only or GPL-3.0-only', 'https://thekelleys.org.uk/dnsmasq/doc.html'],
	['Dropbear SSH', 'MIT-style', 'https://matt.ucc.asn.au/dropbear/dropbear.html'],
	['uhttpd, ubus, libubox, netifd, procd', 'ISC, LGPL-2.1 or GPL-2.0 per component',
	 'https://git.openwrt.org'],
	['Realtek RTL8192CD / RTL8812F wireless driver',
	 'GPL-2.0-only, from the vendor source releases', null],
	['This port: board support, hh71vm-modemd, LuCI app and theme',
	 'Apache-2.0', SOURCE_URL]
];

var boardInfo = rpc.declare({ object: 'system', method: 'board' });

return view.extend({
	handleSave: null,
	handleSaveApply: null,
	handleReset: null,

	load: function () {
		return Promise.all([
			boardInfo().catch(function () { return {}; }),
			m.api.status().catch(function () { return {}; })
		]);
	},

	render: function (data) {
		var board = data[0] || {}, st = data[1] || {};
		var rel = board.release || {};
		var dev = st.device || {};

		var rows = [E('div', { 'class': 'tr table-titles' }, [
			E('div', { 'class': 'th' }, _('Component')),
			E('div', { 'class': 'th' }, _('Licence')),
			E('div', { 'class': 'th' }, _('Home'))
		])];
		COMPONENTS.forEach(function (c) {
			rows.push(E('div', { 'class': 'tr' }, [
				E('div', { 'class': 'td' }, c[0]),
				E('div', { 'class': 'td' }, c[1]),
				E('div', { 'class': 'td' }, c[2]
					? E('a', { 'href': c[2], 'target': '_blank', 'rel': 'noopener' },
					    c[2].replace(/^https?:\/\//, ''))
					: '–')
			]));
		});

		return E('div', {}, [
			E('div', { 'class': 'cbi-section fade-in' }, [
				E('h3', {}, _('About this port')),
				E('p', {}, _('OpenWrt %s, ported by sowarden for the Alcatel LINKHUB \
HH71VM.').format('<strong>' + (rel.version || '19.07') + '</strong>')),
				m.facts([
					[_('OpenWrt release'), (rel.distribution || 'OpenWrt') + ' ' +
						(rel.version || '') + ' ' + (rel.revision || ''), { copy: true }],
					[_('Target'), rel.target, { mono: true }],
					[_('Board'), board.model],
					[_('SoC'), board.system],
					[_('Kernel'), board.kernel, { mono: true }],
					[_('Modem firmware'), dev.revision, { copy: true, mono: true }],
					[_('Source code'), E('a', { 'href': SOURCE_URL, 'target': '_blank',
					                            'rel': 'noopener' }, SOURCE_URL),
						{ raw: true }]
				])
			]),

			E('div', { 'class': 'cbi-section fade-in' }, [
				E('h3', {}, _('Special Thanks')),
				m.facts([
					['2rt', _('For helping with testing and debugging on their own router')]
				])
			]),

			E('div', { 'class': 'cbi-section fade-in' }, [
				E('h3', {}, _('Source code and the GPL')),
				E('p', {}, _('This firmware contains software licensed under the GNU \
General Public License, version 2. You are entitled to the complete corresponding source \
code for those parts, including the scripts used to configure and build them.')),
				E('p', {}, _('The sources for this port, together with the patches applied \
to the upstream projects, are published at %s.')
					.format('<a href="' + SOURCE_URL + '" target="_blank" rel="noopener">' +
					        SOURCE_URL + '</a>')),
				E('p', {}, _('The upstream projects hold the copyright in their own code; \
nothing here changes their licences.'))
			]),

			E('div', { 'class': 'cbi-section fade-in' }, [
				E('h3', {}, _('Components and licences')),
				E('div', { 'class': 'cbi-section-descr' },
				  _('The main pieces this firmware is built from. Every installed package \
carries its own licence; the full per-package list is in the source repository and in the \
build output.')),
				E('div', { 'class': 'table' }, rows)
			]),

			E('div', { 'class': 'cbi-section fade-in' }, [
				E('h3', {}, _('No warranty')),
				E('div', { 'class': 'alert-message warning' }, [
					E('p', {}, _('This is an unofficial, community-made firmware. It is \
provided as is, without warranty of any kind, express or implied. The author is not \
responsible for any damage to your device, loss of data, loss of service, or any other \
consequence of installing or running it.')),
					E('p', {}, _('Installing third-party firmware normally voids the \
manufacturer\'s warranty, and a failed installation can leave the device unusable.'))
				]),
				E('p', {}, _('Alcatel and LINKHUB are trademarks of their respective \
owners. This project is not affiliated with, endorsed by, or supported by the \
manufacturer of the device, its brand owners, or Qualcomm.'))
			])
		]);
	}
});
