'use strict';
// SPDX-License-Identifier: Apache-2.0
'require view';
'require rpc';
'require ui';
'require dom';
'require poll';

var status = rpc.declare({ object: 'modem-extra-tools', method: 'status' });
var job = rpc.declare({ object: 'modem-extra-tools', method: 'job' });
var ttlSet = rpc.declare({ object: 'modem-extra-tools', method: 'ttl_set',
	params: [ 'ipv4', 'ipv6', 'ipv6_enabled', 'network' ] });
var ttlDisable = rpc.declare({ object: 'modem-extra-tools', method: 'ttl_disable' });
var bandRead = rpc.declare({ object: 'modem-extra-tools', method: 'bands_refresh' });
var bandSet = rpc.declare({ object: 'modem-extra-tools', method: 'bands_set', params: [ 'bands' ] });
var bandRestore = rpc.declare({ object: 'modem-extra-tools', method: 'bands_restore' });
var bandRecover = rpc.declare({ object: 'modem-extra-tools', method: 'bands_recover' });
var imeiRead = rpc.declare({ object: 'modem-extra-tools', method: 'imei_refresh' });
var imeiRestore = rpc.declare({ object: 'modem-extra-tools', method: 'imei_restore',
	params: [ 'imei', 'confirmation' ] });
var imeiRecover = rpc.declare({ object: 'modem-extra-tools', method: 'imei_recover' });

function check(result) {
	if (!result || result.ok === false) throw new Error(result && result.error || _('No valid response'));
	return result;
}
function notify(error) {
	ui.addNotification(null, E('p', {}, String(error.message || error.error || error)), 'error');
}
function names(values) { return (values || []).map(function(b) { return 'B' + b; }).join(', ') || '-'; }
function section(title, children) {
	return E('div', { 'class': 'cbi-section' }, [ E('h3', {}, title) ].concat(children));
}
function row(label, input, description) {
	return E('div', { 'class': 'cbi-value' }, [
		E('div', { 'class': 'cbi-value-title' }, label),
		E('div', { 'class': 'cbi-value-field' }, [ input,
			E('div', { 'class': 'cbi-value-description' }, description || '') ])
	]);
}
function integer(input) {
	return /^\d+$/.test(input.value) && Number(input.value) >= 1 && Number(input.value) <= 255;
}
function validImei(value) {
	if (!/^\d{15}$/.test(value) || !/[1-9]/.test(value)) return false;
	var sum = 0;
	for (var i = 0; i < 15; i++) {
		var digit = Number(value.charAt(i));
		if (i < 14 && i % 2 === 1) { digit *= 2; if (digit >= 10) digit -= 9; }
		sum += digit;
	}
	return sum % 10 === 0;
}

return view.extend({
	handleSave: null, handleSaveApply: null, handleReset: null,
	load: function() { return Promise.all([ status(), job() ]); },
	render: function(data) {
		var body = E('div'), state = check(data[0]), currentJob = data[1] || {}, busy = false;
		function refresh() { return status().then(function(s) { state = check(s); draw(); }); }
		function action(call, async) {
			if (busy || currentJob.state === 'running') return;
			busy = true;
			Array.prototype.forEach.call(body.querySelectorAll('button'), function(b) { b.disabled = true; });
			return call().then(check).then(function(result) {
				if (async) currentJob = result;
				else ui.addNotification(null, E('p', {}, _('Settings applied and saved.')), 'info');
			}).catch(notify).then(function() { busy = false; return refresh(); }).catch(notify);
		}
		function confirmBands(title, message, call, applyLabel) {
			ui.showModal(title, [ E('p', {}, message),
				E('p', { 'class': 'alert-message warning' },
					_('Changing bands asks the modem to reselect the mobile network and may interrupt mobile service. No router reboot is requested. A selection without local coverage may leave the modem offline; restore the saved bands through the LAN.')),
				E('div', { 'class': 'cbi-page-actions' }, [
					E('button', { 'class': 'cbi-button', 'click': ui.hideModal }, _('Cancel')),
					E('button', { 'class': 'cbi-button cbi-button-action', 'click': function() {
						ui.hideModal(); action(call, true);
					} }, applyLabel || _('Apply bands')) ]) ]);
		}
		function button(text, style, disabled, click) {
			return E('button', { 'class': 'cbi-button ' + (style || ''),
				'disabled': disabled || busy || currentJob.state === 'running' ? 'disabled' : null,
				'click': click }, text);
		}
		function draw() {
			var t = state.ttl || {}, b = state.bands || {}, im = state.imei || {};
			var content = [ E('h2', {}, _('Extra modem tools')),
				E('p', {}, _('Optional mobile TTL / Hop Limit normalization, LTE band selection and guarded restoration of the device owner\'s original IMEI.')) ];
			if (currentJob.state === 'running') content.push(E('p', { 'class': 'alert-message notice spinning' },
				_('Modem operation in progress. You can leave this page; the operation continues in the background.')));

			var v4 = E('input', { 'type': 'number', 'min': 1, 'max': 255, 'step': 1,
				'value': t.ipv4_value || 65, 'style': 'width:8em', 'aria-label': _('IPv4 TTL') });
			var v6 = E('input', { 'type': 'number', 'min': 1, 'max': 255, 'step': 1,
				'value': t.ipv6_value || 65, 'style': 'width:8em', 'aria-label': _('IPv6 Hop Limit'),
				'disabled': t.ipv6_enabled ? null : 'disabled' });
			var use6 = E('input', { 'type': 'checkbox', 'checked': t.ipv6_enabled ? 'checked' : null,
				'change': function() { v6.disabled = !use6.checked; } });
			var network = E('input', { 'type': 'text', 'value': t.wan_network || 'wan',
				'maxlength': 32, 'style': 'width:12em', 'aria-label': _('Mobile WAN network') });
			var ttlChildren = [
				E('p', {}, [ E('strong', {}, t.enabled ? _('Enabled') : _('Disabled')), ' | ',
					_('IPv4 rule: ') + (t.ipv4_active ? _('active') : _('inactive')), ' | ',
					_('IPv6 rule: ') + (t.ipv6_active ? _('active') : _('inactive')) ]),
				E('p', {}, _('TTL (IPv4) and Hop Limit (IPv6) decrease at each routed hop. These values are set on packets leaving the OpenWrt interface toward the Qualcomm mobile modem, including traffic from the router itself. No hidden offset is added.')),
				row(_('IPv4 TTL'), v4, _('Range: 1-255. On the normal two-router HH71VM path, 65 is expected to become 64 after Qualcomm routing; this is not a guarantee of carrier acceptance.')),
				row(_('IPv6 normalization'), E('label', {}, [ use6, ' ', _('Enable IPv6 Hop Limit rewriting') ]),
					_('Only use this when IPv6 traffic leaves through the same mobile WAN device. Link-local and multicast traffic is excluded.')),
				row(_('IPv6 Hop Limit'), v6),
				row(_('Mobile WAN network'), network, _('Logical OpenWrt network that leads to the Qualcomm modem, normally wan. Current device: ') + (t.wan_device || '-'))
			];
			if (t.warning) ttlChildren.push(E('p', { 'class': 'alert-message warning' }, t.warning));
			if (t.flow_offload_detected) ttlChildren.push(E('p', { 'class': 'alert-message warning' },
				_('Flow offloading is enabled. Disable software and hardware flow offloading in Firewall before enabling TTL Fix.')));
			if (t.enabled && (!t.ipv4_active || (t.ipv6_enabled && !t.ipv6_active))) ttlChildren.push(
				E('p', { 'class': 'alert-message warning' }, _('Saved settings are not fully active. Check the mobile WAN device and system log, then apply again.')));
			ttlChildren.push(E('div', { 'class': 'cbi-page-actions' }, [
				button(_('Disable'), 'cbi-button-negative', !t.enabled, function() { action(ttlDisable, false); }),
				button(_('Apply TTL Fix'), 'cbi-button-action', t.flow_offload_detected, function() {
					if (!integer(v4) || (use6.checked && !integer(v6)) || !/^[A-Za-z0-9_]{1,32}$/.test(network.value)) {
						notify(new Error(_('Use integer values from 1 to 255 and a valid logical mobile WAN network name.'))); return;
					}
					action(function() { return ttlSet(Number(v4.value), Number(v6.value) || 65, use6.checked, network.value); }, false);
				}) ]));
			content.push(section(_('TTL Fix'), ttlChildren));

			var selected = {}, choices = E('div', { 'class': 'cbi-checkboxes' });
			(b.current_bands || []).forEach(function(band) { selected[band] = true; });
			(b.supported_bands || []).forEach(function(band) {
				choices.appendChild(E('label', { 'style': 'display:inline-block;min-width:6em;margin:.5em 1em .5em 0' }, [
					E('input', { 'type': 'checkbox', 'value': band, 'checked': selected[band] ? 'checked' : null,
						'disabled': b.unread || b.pending ? 'disabled' : null }), ' LTE B' + band ]));
			});
			if (!(b.supported_bands || []).length) choices.appendChild(E('em', {}, _('Read bands to query this modem.')));
			var bandsChildren = [
				E('p', {}, _('Restrict the LTE bands the modem may use. This does not lock a cell tower or force carrier aggregation. The fastest choice depends on local coverage and congestion.')),
				E('p', {}, _('The available checkboxes come from the Qualcomm modem\'s QMI capability response, not from a fixed router or operator list.')),
				E('p', {}, b.managed ? _('Maintained selection: ') + names(b.desired_bands) : _('Automatic maintenance is off.')),
				E('p', { 'class': 'cbi-value-description' }, _('OpenWrt stores your selection and checks it once per minute, restoring it after modem startup or stock software changes. No write is sent while it already matches. Restore original also turns maintenance off.')),
				E('p', {}, b.unread ? _('No modem reading yet. Read bands to query capabilities and the current preference without restarting the modem.') :
					_('Last read preference: ') + names(b.current_bands) + ' | ' + new Date((b.refreshed || 0) * 1000).toLocaleString()),
				row(_('Supported LTE bands'), choices, _('B32 is supplementary downlink and cannot be selected alone.')),
				E('p', { 'class': 'cbi-value-description' }, b.backup_present ?
					_('Original restore point: ') + names(b.backup_bands) :
					_('The original LTE preference is saved before the first change and is never overwritten. Band control uses QMI and does not edit radio calibration NV items.'))
			];
			if (!b.unread && b.editable === false) bandsChildren.push(E('p', { 'class': 'alert-message warning' },
				_('The current preference is inconsistent with the modem capability response. Changes are blocked instead of dropping unknown bits.')));
			if (b.backup_error) bandsChildren.push(E('p', { 'class': 'alert-message error' }, b.backup_error));
			if (b.desired_error) bandsChildren.push(E('p', { 'class': 'alert-message error' }, b.desired_error));
			if (b.pending) bandsChildren.push(E('p', { 'class': 'alert-message error' },
				_('An interrupted band transaction requires recovery. Further band changes are blocked until the pre-transaction values are restored.')));
			bandsChildren.push(E('div', { 'class': 'cbi-page-actions' }, [
				button(_('Read bands'), '', false, function() { action(bandRead, true); }),
				button(_('Recover interrupted change'), 'cbi-button-negative', !b.pending, function() {
					confirmBands(_('Recover bands'), _('Restore the values saved immediately before the interrupted transaction?'), bandRecover, _('Recover'));
				}),
				button(_('Restore original'), '', !b.backup_present || b.pending, function() {
					confirmBands(_('Restore original bands'), _('Restore the original LTE preference saved before the first change?'), bandRestore, _('Restore'));
				}),
				button(_('Apply bands'), 'cbi-button-action', b.unread || !b.editable || b.pending, function() {
					var values = [];
					Array.prototype.forEach.call(choices.querySelectorAll('input:checked'), function(input) { values.push(Number(input.value)); });
					if (!values.length || (values.length === 1 && values[0] === 32)) {
						notify(new Error(_('Select at least one anchor band; B32 alone is not valid.'))); return;
					}
					confirmBands(_('Apply LTE bands'), _('Allow only ') + names(values) + '?', function() { return bandSet(values); });
				}) ]));
			content.push(section(_('LTE band selection'), bandsChildren));

			var imeiOne = E('input', { 'type': 'text', 'inputmode': 'numeric', 'pattern': '[0-9]{15}',
				'maxlength': 15, 'autocomplete': 'off', 'style': 'width:17em', 'aria-label': _('Original IMEI') });
			var imeiTwo = E('input', { 'type': 'text', 'inputmode': 'numeric', 'pattern': '[0-9]{15}',
				'maxlength': 15, 'autocomplete': 'off', 'style': 'width:17em', 'aria-label': _('Repeat original IMEI') });
			var ownership = E('input', { 'type': 'checkbox' });
			var imeiChildren = [
				E('p', { 'class': 'alert-message warning' }, _('Use this only to restore the original IMEI printed on the label or box of this exact router. Do not enter an invented value or an IMEI from another device.')),
				E('p', {}, im.unread ? _('Current IMEI has not been read yet.') :
					_('Current IMEI: ') + (im.current_imei || _('unreadable')) + (im.current_valid ? '' : _(' (missing, damaged or not checksum-valid)'))),
				E('p', { 'class': 'cbi-value-description' }, im.backup_present ?
					_('A private safety copy of NV 550 made before the first restore is stored on this router and survives reboot/sysupgrade with settings kept.') :
					_('Before the first restore, the current NV 550 record is saved privately and is never overwritten.')),
				row(_('Original IMEI'), imeiOne, _('Exactly 15 digits from the device label or original box.')),
				row(_('Repeat original IMEI'), imeiTwo, _('Enter it again to catch typing mistakes.')),
				row(_('Confirmation'), E('label', {}, [ ownership, ' ',
					_('This is the original IMEI printed for this exact router, and I am restoring it only on that router.') ]))
			];
			if (im.pending) imeiChildren.push(E('p', { 'class': 'alert-message error' },
				_('An interrupted IMEI restore is recorded. Use recovery to finish writing and verifying the already confirmed target.')));
			imeiChildren.push(E('div', { 'class': 'cbi-page-actions' }, [
				button(_('Read current IMEI'), '', false, function() { action(imeiRead, true); }),
				button(_('Recover interrupted restore'), 'cbi-button-negative', !im.pending, function() {
					ui.showModal(_('Recover original IMEI restore'), [
						E('p', {}, _('Retry the previously confirmed restore and verify NV 550 by reading it back?')),
						E('div', { 'class': 'cbi-page-actions' }, [
							E('button', { 'class': 'cbi-button', 'click': ui.hideModal }, _('Cancel')),
							E('button', { 'class': 'cbi-button cbi-button-negative', 'click': function() {
								ui.hideModal(); action(imeiRecover, true);
							} }, _('Recover')) ]) ]);
				}),
				button(_('Restore original IMEI'), 'cbi-button-negative', im.unread || im.pending, function() {
					var target = imeiOne.value;
					if (target !== imeiTwo.value) { notify(new Error(_('The two IMEI entries do not match.'))); return; }
					if (!validImei(target)) { notify(new Error(_('IMEI must contain 15 digits and have a valid check digit.'))); return; }
					if (!ownership.checked) { notify(new Error(_('Confirm that this is the original IMEI of this exact router.'))); return; }
					ui.showModal(_('Restore original IMEI'), [
						E('p', {}, _('Current value: ') + (im.current_imei || _('unreadable'))),
						E('p', {}, _('Restore the label value ') + target + '?'),
						E('p', { 'class': 'alert-message warning' }, _('This writes only Qualcomm NV item 550, then reads it back for exact verification. Mobile service may reconnect. Do not power off the router during the operation.')),
						E('div', { 'class': 'cbi-page-actions' }, [
							E('button', { 'class': 'cbi-button', 'click': ui.hideModal }, _('Cancel')),
							E('button', { 'class': 'cbi-button cbi-button-negative', 'click': function() {
								ui.hideModal(); action(function() { return imeiRestore(target, true); }, true);
							} }, _('Restore and verify')) ]) ]);
				}) ]));
			content.push(section(_('Restore original IMEI'), imeiChildren));

			content.push(section(_('Command line'), [ E('pre', {},
				'modem-extra-tools status --json\nmodem-extra-tools ttl set 65 off wan\n' +
				'modem-extra-tools ttl disable\nmodem-extra-tools bands show\n' +
				'modem-extra-tools bands set 3,7\nmodem-extra-tools bands restore\n' +
				'modem-extra-tools imei show\n' +
				'modem-extra-tools imei restore ORIGINAL_15_DIGIT_IMEI --confirm-original-imei'),
				E('p', { 'class': 'cbi-value-description' }, _('Replace the placeholder only with the original 15-digit value printed for your router. IMEI restore is never automatic.')) ]));
			dom.content(body, content);
			if (window.HH71) window.HH71.decorate(body);
		}
		poll.add(function() {
			if (currentJob.state !== 'running' || busy) return Promise.resolve();
			return job().then(function(result) {
				if (result.state === 'running') return;
				currentJob = result;
				if (result.ok === false) notify(result);
				else ui.addNotification(null, E('p', {}, _('Modem operation completed.')), 'info');
				return refresh();
			}).catch(notify);
		}, 2);
		draw(); return body;
	}
});
