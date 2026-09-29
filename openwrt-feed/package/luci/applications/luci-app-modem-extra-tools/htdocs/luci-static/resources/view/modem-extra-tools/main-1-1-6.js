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
var simlockStatus = rpc.declare({ object: 'modem-extra-tools', method: 'simlock_status' });
var simlockLock = rpc.declare({ object: 'modem-extra-tools', method: 'simlock_lock',
	params: [ 'plmn', 'confirmation' ] });

function check(result) {
	if (!result || result.ok === false) throw new Error(result && result.error || _('No valid response'));
	return result;
}
/* A lone string child of E() is parsed as HTML; modem answers and backend errors are
   passed as text. */
function txt(v) { return v == null ? [] : (typeof v === 'object' ? v : [ String(v) ]); }
function notify(error) {
	ui.addNotification(null, E('p', {}, [ String(error.message || error.error || error) ]), 'error');
}
function names(values) { return (values || []).map(function(b) { return 'B' + b; }).join(', ') || '-'; }
function section(title, children) {
	return E('div', { 'class': 'cbi-section' }, [ E('h3', {}, txt(title)) ].concat(children));
}
function row(label, input, description) {
	return E('div', { 'class': 'cbi-value' }, [
		E('div', { 'class': 'cbi-value-title' }, txt(label)),
		E('div', { 'class': 'cbi-value-field' }, [ input,
			E('div', { 'class': 'cbi-value-description' }, txt(description || '')) ])
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

// One stylesheet for the whole page. The LuCI themes size form rows for a desktop table
// layout, so inputs keep a fixed width and button rows stay horizontal until they overflow
// their container on a phone. Everything below is scoped to this page and does two things:
// keep every control inside its box, and collapse rows to a single column on small screens.
var pageStyle =
	'#met-page input,#met-page select,#met-page textarea{max-width:100%;box-sizing:border-box}' +
	'#met-page .cbi-value-field input[type=text]{max-width:min(100%,30rem)}' +
	// Themes give the button variants different padding, which made two buttons standing
	// side by side end up different heights. Pin the box model for all of them.
	'#met-page button.cbi-button{min-height:36px;padding:6px 14px;line-height:22px;' +
		'white-space:normal;box-sizing:border-box;vertical-align:middle;margin:0}' +
	'#met-page .cbi-page-actions{display:flex;flex-wrap:wrap;gap:8px;align-items:stretch;justify-content:flex-end}' +
	'@media(max-width:600px){' +
		'#met-page .cbi-page-actions{flex-direction:column;align-items:stretch}' +
		'#met-page .cbi-page-actions > button{width:100%}' +
		'#met-page .cbi-value{display:block}' +
		'#met-page .cbi-value-title{width:auto;min-width:0;padding:0 0 4px;text-align:left}' +
		'#met-page .cbi-value-field{width:auto;margin-left:0}' +
		// Beats the inline widths the desktop layout sets on these fields.
		'#met-page input[type=number],#met-page input[type=text]{width:100%!important;max-width:100%}' +
	'}' +
	// Confirmation dialogs are rendered outside this page's container by LuCI, so their
	// button row needs its own rule to stop two buttons sharing one cramped line.
	'@media(max-width:600px){' +
		'#modal_overlay .cbi-page-actions{display:flex;flex-direction:column;align-items:stretch;gap:8px}' +
		'#modal_overlay .cbi-page-actions > button{width:100%;margin:0}' +
	'}' +
	'#met-simlock .met-group + .met-group{border-top:1px solid var(--border,#ccc);margin-top:20px;padding-top:20px}' +
	'#met-simlock .met-header h4{margin:0;font-size:15px}' +
	'#met-simlock .met-header p{margin:4px 0 0;color:var(--muted,#777)}' +
	'#met-simlock .met-control{display:flex;align-items:flex-end;gap:10px;flex-wrap:wrap;margin-top:12px}' +
	'#met-simlock .met-control > label{display:flex;flex-direction:column;gap:6px;flex:1 1 220px;min-width:0;font-weight:600}' +
	'#met-simlock .met-control input{width:100%}' +
	'#met-simlock .met-control > button{flex:0 0 auto}' +
	'#met-simlock .met-actions{display:flex;align-items:center;gap:8px;flex-wrap:wrap;margin-top:12px}' +
	'#met-simlock .met-note{color:var(--muted,#777);font-size:12.5px;line-height:1.5;margin:8px 0 0}' +
	'#met-simlock .met-result{border:1px solid var(--border,#ccc);border-radius:8px;' +
		'background:var(--bg,rgba(127,127,127,.06));padding:12px 14px;margin-top:14px}' +
	'#met-simlock .met-result > *:first-child{margin-top:0}' +
	'#met-simlock .met-result p{margin:6px 0 0}' +
	'#met-simlock .met-code{margin-top:14px}#met-simlock .met-code:first-child{margin-top:0}' +
	'#met-simlock .met-code > span{display:block;font-weight:600;margin-bottom:6px}' +
	'#met-simlock .met-code > span em{font-style:normal;font-weight:400;color:var(--muted,#777)}' +
	'#met-simlock .met-code input{font-size:1.1em;letter-spacing:.04em}' +
	'#met-simlock .met-code.met-required input{border:2px solid var(--primary,#2979ff)}' +
	'#met-simlock .met-meta{display:flex;flex-wrap:wrap;gap:6px;margin:10px 0 0}' +
	'#met-simlock .met-meta span{border:1px solid var(--border,#ccc);border-radius:5px;padding:4px 8px;font-size:12.5px}' +
	'#met-simlock details{margin-top:14px}#met-simlock summary{cursor:pointer;font-weight:600}' +
	'#met-simlock details > p{margin:10px 0 0}' +
	'#met-simlock .met-state{display:flex;gap:12px;align-items:flex-start;border:1px solid var(--border,#ccc);' +
		'border-left-width:5px;border-radius:8px;padding:14px 16px;margin:0 0 4px}' +
	'#met-simlock .met-state.ok{border-left-color:#2e7d32}' +
	'#met-simlock .met-state.warn{border-left-color:#ef6c00}' +
	'#met-simlock .met-state.info{border-left-color:#2979ff}' +
	'#met-simlock .met-state.none{border-left-color:var(--border,#ccc)}' +
	'#met-simlock .met-state h4{margin:0;font-size:16px}' +
	'#met-simlock .met-state p{margin:6px 0 0;color:var(--muted,#777);line-height:1.5}' +
	'#met-simlock .met-state .met-state-body{flex:1 1 auto;min-width:0}' +
	'#met-simlock .met-state > button{flex:0 0 auto}' +
	'#met-simlock .met-step{margin-top:18px}' +
	'#met-simlock dl.met-facts{display:grid;grid-template-columns:auto 1fr;gap:4px 14px;margin:10px 0 0;font-size:12.5px}' +
	'#met-simlock dl.met-facts dt{color:var(--muted,#777)}#met-simlock dl.met-facts dd{margin:0}' +
	// Phones: every row becomes one column, and anything clickable spans the full width so
	// no label is squeezed into a sliver next to a button.
	'@media(max-width:600px){' +
		'#met-simlock .met-control,#met-simlock .met-actions{flex-direction:column;align-items:stretch}' +
		// The row layout gives the label a 220px flex basis. Once the container becomes a
		// column that basis is a *height*, which opens a large gap under every field.
		'#met-simlock .met-control > label{flex:0 0 auto}' +
		'#met-simlock .met-control > button,#met-simlock .met-actions > button{width:100%}' +
		'#met-simlock .met-state{flex-direction:column;align-items:stretch}' +
		'#met-simlock .met-state > button{width:100%}' +
		'#met-simlock dl.met-facts{grid-template-columns:1fr}' +
		'#met-simlock dl.met-facts dd{margin:0 0 6px;font-weight:600}' +
	'}';

return view.extend({
	handleSave: null, handleSaveApply: null, handleReset: null,
	load: function() { return Promise.all([ status(), job() ]); },
	render: function(data) {
		var body = E('div', { 'id': 'met-page' }), state = check(data[0]), currentJob = data[1] || {}, busy = false;
		// SIM lock state is never polled: each read costs a round trip to core_app, so it
		// is held here and only replaced by an explicit read or by an unlock result.
		// Cached results may describe a different SIM. Never offer actions until a fresh read.
		var lock = state.simlock && state.simlock.pending ? { unread: true, pending: true } : { unread: true };
	var simlockJobId = null;
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
			ui.showModal(title, [ E('p', {}, txt(message)),
				E('p', { 'class': 'alert-message warning' },
					_('Changing bands asks the modem to reselect the mobile network and may interrupt mobile service. No router reboot is requested. A selection without local coverage may leave the modem offline; restore the saved bands through the LAN.')),
				E('div', { 'class': 'cbi-page-actions' }, [
					E('button', { 'class': 'cbi-button', 'click': ui.hideModal }, _('Cancel')),
					E('button', { 'class': 'cbi-button cbi-button-action', 'click': function() {
						ui.hideModal(); action(call, true);
					} }, applyLabel || _('Apply bands')) ]) ]);
		}
		function lockBusy(on) {
			busy = on;
			if (on) Array.prototype.forEach.call(body.querySelectorAll('button'), function(b) { b.disabled = true; });
		}
		function readLock() {
			if (busy || currentJob.state === 'running') return;
			lockBusy(true);
			return simlockStatus().then(check).then(function(result) {
				lock = result;
			}).catch(function(error) {
				lock = { unread: true, pending: lock.pending === true,
					error: String(error.message || error) };
				notify(error);
			}).then(function() { lockBusy(false); draw(); });
		}
		function sendLockAction(call) {
			lockBusy(true);
			return call().then(check).then(function(result) {
				currentJob = result;
				simlockJobId = result.id;
				lock = { unread: true, pending: true };
			}).catch(function(error) {
				lock = { unread: true, pending: true, error: String(error.message || error) };
				notify(error);
				return simlockStatus().then(check).then(function(result) { lock = result; })
					.catch(function() {});
			}).then(function() { lockBusy(false); draw(); });
		}
		function sendLock(plmn) {
			return sendLockAction(function() { return simlockLock(plmn, true); });
		}
		function button(text, style, disabled, click) {
			return E('button', { 'class': 'cbi-button ' + (style || ''),
				'disabled': disabled || busy || currentJob.state === 'running' ? 'disabled' : null,
				'click': click }, txt(text));
		}
		function draw() {
			var t = state.ttl || {}, b = state.bands || {}, im = state.imei || {};
			var content = [ E('h2', {}, _('Extra modem tools')),
				E('p', {}, _('Optional mobile TTL / Hop Limit normalization, LTE band selection, guarded restoration of the device owner\'s original IMEI, and the carrier SIM lock.')) ];
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
			if (t.warning) ttlChildren.push(E('p', { 'class': 'alert-message warning' }, txt(t.warning)));
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
			var unconfirmed = {};
			(b.unconfirmed_bands || []).forEach(function(band) { unconfirmed[band] = true; });
			(b.selectable_bands || b.supported_bands || []).forEach(function(band) {
				choices.appendChild(E('label', { 'style': 'display:inline-block;min-width:6em;margin:.5em 1em .5em 0' }, [
					E('input', { 'type': 'checkbox', 'value': band, 'checked': selected[band] ? 'checked' : null,
						'disabled': b.unread || b.pending ? 'disabled' : null }), ' LTE B' + band,
					unconfirmed[band] ? E('small', {}, ' ' + _('(current only)')) : '' ]));
			});
			if (!(b.selectable_bands || b.supported_bands || []).length) choices.appendChild(E('em', {}, _('Read bands to query this modem.')));
			var bandsChildren = [
				E('p', {}, _('Restrict the LTE bands the modem may use. This does not lock a cell tower or force carrier aggregation. The fastest choice depends on local coverage and congestion.')),
				E('p', {}, _('The available checkboxes are discovered from the Qualcomm modem. Bands already present in the current preference remain visible even when the capability response omits them. No fixed router or operator band list is used.')),
				E('p', {}, txt(b.managed ? _('Maintained selection: ') + names(b.desired_bands) : _('Automatic maintenance is off.'))),
				E('p', { 'class': 'cbi-value-description' }, _('OpenWrt stores your selection and checks it once per minute, restoring it after modem startup or stock software changes. No write is sent while it already matches. Restore original also turns maintenance off.')),
				E('p', {}, txt(b.unread ? _('No modem reading yet. Read bands to query capabilities and the current preference without restarting the modem.') :
					_('Last read preference: ') + names(b.current_bands) + ' | ' + new Date((b.refreshed || 0) * 1000).toLocaleString())),
				row(_('Available LTE bands'), choices, _('Bands marked current only are enabled in the modem preference but are not confirmed by its capability response. B32 is supplementary downlink and cannot be selected alone.')),
				E('p', { 'class': 'cbi-value-description' }, txt(b.backup_present ?
					_('Original restore point: ') + names(b.backup_bands) :
					_('The original LTE preference is saved before the first change and is never overwritten. Band control uses QMI and does not edit radio calibration NV items.')))
			];
			if (!b.unread && b.capability_mismatch) bandsChildren.push(E('p', { 'class': 'alert-message warning' },
				txt(_('The current preference contains bands omitted by the modem capability response: ') + names(b.unconfirmed_bands) +
				_('. They can be preserved or explicitly removed, but no new unreported band can be added.'))));
			if (!b.unread && b.editable === false) bandsChildren.push(E('p', { 'class': 'alert-message error' },
				_('The modem returned an invalid LTE preference. Changes remain blocked.')));
			if (b.backup_error) bandsChildren.push(E('p', { 'class': 'alert-message error' }, txt(b.backup_error)));
			if (b.desired_error) bandsChildren.push(E('p', { 'class': 'alert-message error' }, txt(b.desired_error)));
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
					var kept = {};
					values.forEach(function(band) { kept[band] = true; });
					var removed = (b.unconfirmed_bands || []).filter(function(band) { return !kept[band]; });
					var message = _('Allow only ') + names(values) + '?';
					if (removed.length) message += ' ' + _('This explicitly removes current-only bands: ') + names(removed) + '.';
					confirmBands(_('Apply LTE bands'), message, function() { return bandSet(values); });
				}) ]));
			content.push(section(_('LTE band selection'), bandsChildren));

			var imeiOne = E('input', { 'type': 'text', 'inputmode': 'numeric', 'pattern': '[0-9]{15}',
				'maxlength': 15, 'autocomplete': 'off', 'style': 'width:17em', 'aria-label': _('Original IMEI') });
			var imeiTwo = E('input', { 'type': 'text', 'inputmode': 'numeric', 'pattern': '[0-9]{15}',
				'maxlength': 15, 'autocomplete': 'off', 'style': 'width:17em', 'aria-label': _('Repeat original IMEI') });
			var ownership = E('input', { 'type': 'checkbox' });
			var imeiChildren = [
				E('p', { 'class': 'alert-message warning' }, _('Use this only to restore the original IMEI printed on the label or box of this exact router. Do not enter an invented value or an IMEI from another device.')),
				E('p', { 'class': 'alert-message warning' }, _('After a successful restore, fully shut down the router, disconnect its power, and then power it on again. A normal OpenWrt reboot restarts the Realtek/OpenWrt side but does not fully restart the separate Qualcomm modem subsystem, so the restored value may not take effect until this cold power cycle.')),
				E('p', {}, txt(im.unread ? _('Current IMEI has not been read yet.') :
					_('Current IMEI: ') + (im.current_imei || _('unreadable')) + (im.current_valid ? '' : _(' (missing, damaged or not checksum-valid)')))),
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
			if (im.activation_pending) imeiChildren.push(E('p', { 'class': 'alert-message warning' },
				_('NV 550 readback completed, but activation by the running modem and the mobile network is not verified. Fully shut down the router, disconnect its power, then power it on again. An OpenWrt reboot or a matching ATI readback is not proof of network acceptance.')));
			if (im.identity_cache_refreshed === false) imeiChildren.push(E('p', { 'class': 'alert-message notice' },
				_('The main Modem overview cache could not be refreshed. Its IMEI may remain stale until the modem channel reconnects or the router is fully power-cycled.')));
			else if (im.reported_imei) imeiChildren.push(E('p', { 'class': 'cbi-value-description' },
				_('Fresh ATI readback: ') + (im.reported_matches_nv ? _('matches NV 550') : _('differs from NV 550')) +
				_('. This still does not verify the identity accepted by the mobile network.')));
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
						E('p', {}, [ _('Current value: ') + (im.current_imei || _('unreadable')) ]),
						E('p', {}, [ _('Restore the label value ') + target + '?' ]),
						E('p', { 'class': 'alert-message warning' }, _('This writes only Qualcomm NV item 550, then reads it back for exact verification. Mobile service may reconnect. Do not power off the router during the operation. After success, fully disconnect router power before relying on the restored value.')),
						E('div', { 'class': 'cbi-page-actions' }, [
							E('button', { 'class': 'cbi-button', 'click': ui.hideModal }, _('Cancel')),
							E('button', { 'class': 'cbi-button cbi-button-negative', 'click': function() {
								ui.hideModal(); action(function() { return imeiRestore(target, true); }, true);
							} }, _('Restore and verify')) ]) ]);
				}) ]));
			content.push(section(_('Restore original IMEI'), imeiChildren));

			// One question drives this section: is a carrier lock armed in the modem, and does
			// it accept the SIM that is in the router right now? The backend answers that from
			// the modem itself; core_app's cached status only appears under Technical details,
			// because it keeps describing the old state for a few seconds after every write.
			// Removing a lock is not here: it belongs where a router whose SIM is being
			// refused can still reach it, which is the modem page in the base image. What
			// is left on this page is creating one, which nobody needs to recover a device.
			var known = !lock.unread && !lock.pending && !lock.error;
			var lockState = known ? lock.lock_state : null;
			var challenged = lockState === 'challenged';
			var lockedOn = lockState === 'allowed';
			var noLock = lockState === 'none';
			var attempts = typeof lock.attempts_left === 'number' ? lock.attempts_left : null;
			var network = lock.locked_plmn ? String(lock.locked_plmn) : null;
			var homeNetwork = lock.home_plmn ? String(lock.home_plmn) : null;

			// Backend messages are sentence fragments by convention; give them a capital and a
			// full stop so they read as sentences wherever the page shows one.
			function sentence(text) {
				var value = String(text || '').trim();
				if (!value) return '';
				value = value.charAt(0).toUpperCase() + value.slice(1);
				return /[.!?]$/.test(value) ? value : value + '.';
			}
			var banner;
			if (lock.error) banner = [ 'warn', _('The lock status could not be read'),
				sentence(lock.error) + ' ' + _('Nothing is offered until the modem answers again.') ];
			else if (lock.pending) banner = [ 'info', _('Checking how it went'),
				_('The last request was sent to the modem. Read the status again in a moment; do not repeat the request.') ];
			else if (lock.unread) banner = [ 'none', _('Lock status has not been read yet'),
				_('Reading it asks the modem directly and changes nothing.') ];
			else if (challenged) banner = [ 'warn', _('This SIM is blocked by a carrier lock'),
				_('The modem will not use the SIM that is in the router until the lock is removed.') +
					(attempts !== null ? ' ' + _('Code attempts left: ') + String(attempts) + '.' : '') ];
			else if (lockedOn) banner = [ 'ok',
				network ? _('Locked to network ') + network : _('A carrier lock is active'),
				(lock.locked_to_home === true ?
					_('The SIM in the router belongs to this network, so it keeps working normally.') :
					_('The SIM in the router is accepted by this lock.')) + ' ' +
					_('A SIM from any other network would be refused until the lock is removed.') ];
			else if (noLock) banner = [ 'ok', _('No carrier lock'),
				_('This router accepts a SIM card from any mobile network.') ];
			else banner = [ 'warn', _('The lock state is not clear'),
				sentence(lock.lock_state_reason) || _('The modem did not give a complete answer. Read the status again.') ];

			var lockChildren = [ E('div', { 'class': 'met-state ' + banner[0] }, [
				E('div', { 'class': 'met-state-body' }, [ E('h4', {}, txt(banner[1])), E('p', {}, txt(banner[2])) ]),
				button(lock.unread && !lock.error ? _('Read lock status') : _('Refresh'), '', false, readLock)
			]) ];
			if (lock.read_error) lockChildren.push(E('p', { 'class': 'met-note' },
				txt(_('Part of the reading failed: ') + sentence(lock.read_error))));
			if (lock.status_settling) lockChildren.push(E('p', { 'class': 'met-note' },
				_('The modem and its control service still disagree; this settles a few seconds after a change.')));

			if (challenged || lockedOn) lockChildren.push(E('div', { 'class': 'met-group met-step' }, [
				E('div', { 'class': 'met-header' }, E('div', {}, [
					E('h4', {}, _('Removing this lock')),
					E('p', {}, _('Unlocking lives on the modem page, so a router that refuses its SIM can still be recovered without installing anything.'))
				])),
				E('div', { 'class': 'met-actions' },
					E('a', { 'class': 'cbi-button cbi-button-action',
						'href': L.url('admin/modem/overview') }, _('Go to Modem → Overview')))
			]));

			if (noLock) {
				var lockGroup = [ E('div', { 'class': 'met-header' }, E('div', {}, [
					E('h4', {}, _('Lock this router to one network')),
					E('p', {}, _('Optional, and for people who know they want it. After this the router refuses SIM cards from other networks until the lock is removed on the modem page.'))
				])) ];
				if (lock.can_lock && homeNetwork) {
					lockGroup.push(E('p', { 'class': 'met-note' },
						txt(_('The lock will be set to the network of the SIM that is in the router now, code ') +
							homeNetwork + _('. That SIM keeps working.'))));
					lockGroup.push(E('div', { 'class': 'met-actions' },
						button(_('Lock to network ') + homeNetwork, 'cbi-button-negative', false, function() {
							var target = homeNetwork;
							var acknowledge = E('input', { 'type': 'checkbox' });
							ui.showModal(_('Lock to network ') + target, [
								E('p', {}, txt(_('This router will then only work with SIM cards from network ') + target +
									_('. The SIM that is in it now belongs to that network and keeps working.'))),
								E('p', { 'class': 'alert-message warning' },
									_('The modem control service restarts, so mobile service drops for up to a minute. You can undo this at any time from Modem → Overview.')),
								E('label', {}, [ acknowledge, ' ',
									_('I understand other networks will be refused until I remove the lock.') ]),
								E('div', { 'class': 'cbi-page-actions' }, [
									E('button', { 'class': 'cbi-button', 'click': ui.hideModal }, _('Cancel')),
									E('button', { 'class': 'cbi-button cbi-button-negative', 'click': function() {
										if (!acknowledge.checked) {
											notify(new Error(_('Tick the box to confirm.'))); return;
										}
										ui.hideModal(); sendLock(target);
									} }, _('Create the lock')) ]) ]);
						})));
				} else {
					lockGroup.push(E('p', { 'class': 'met-note' },
						txt(sentence(_('Not available right now: ') + (lock.lock_refusal || _('the inserted SIM could not be read'))))));
				}
				lockChildren.push(E('div', { 'class': 'met-group met-step' }, lockGroup));
			}

			if (known) {
				var facts = [];
				function fact(label, value) {
					if (value === null || value === undefined || value === '') return;
					facts.push(E('dt', {}, txt(label)), E('dd', {}, txt(value)));
				}
				fact(_('Carrier lock'), challenged ? _('on, this SIM refused') :
					lockedOn ? _('on, this SIM allowed') : noLock ? _('off') : _('unclear'));
				if (network) fact(_('Locked network code'), network);
				if (homeNetwork) fact(_('Network of the inserted SIM'), homeNetwork);
				if (attempts !== null) fact(_('Code attempts left'), attempts);
				if (lock.code_length) fact(_('Code length this modem wants'), lock.code_length + _(' digits'));
				if (lock.configured_attempts != null) fact(_('Attempts a new lock starts with'), lock.configured_attempts);
				if (lock.card) {
					fact(_('SIM'), lock.card.cpin + (lock.card.readable === true ? _(', readable') : '') +
						(lock.card.registered === true ? _(', registered') : lock.card.registered === false ? _(', not registered') : ''));
					if (lock.card.pn != null) fact(_('Modem lock facility (PN)'), lock.card.pn === 1 ? _('enabled') : _('disabled'));
				}
				if (lock.uim) fact(_('Personalisation entries in the modem'), lock.uim.feature_count);
				if (lock.sim_state != null) fact(_('Control service card state'),
					String(lock.sim_state) + (lock.sim_state_name ? ' (' + lock.sim_state_name + ')' : ''));
				fact(_('Saved carrier setting'), lock.lock_provisioned === true ?
					_('present') + (lock.saved_network_code ? ' (' + lock.saved_network_code + ')' : '') :
					lock.lock_provisioned === false ? _('cleared') : null);
				if (lock.kcap_error) fact(_('Control service'), lock.kcap_error);
				lockChildren.push(E('details', {}, [
					E('summary', {}, _('Technical details')),
					E('dl', { 'class': 'met-facts' }, facts),
					E('p', { 'class': 'met-note' },
						_('“Carrier lock” is read from the modem itself. The control service card state can lag it by a few seconds after a change, and the saved carrier setting only describes what a future lock would use.'))
				]));
			}

			lockChildren.push(E('details', {}, [
				E('summary', {}, _('Which devices this was tested on')),
				E('p', { 'class': 'met-note' }, _('Carrier lock and unlock were tested on one HH71VM GK unit (HH71_GK_02.00_04, Qualcomm MPSS.TH.2.0.1.c8-00028-M9645LAAAANAZM-2.147565.5). Other firmware may behave differently.')),
				E('p', { 'class': 'met-note' }, _('Use only on a router you own. When reporting results, leave out IMEIs, unlock codes and SIM identifiers.'))
			]));

			var simSection = section(_('Carrier SIM lock'), lockChildren);
			simSection.id = 'met-simlock';
			content.push(E('style', {}, pageStyle), simSection);
			content.push(section(_('Command line'), [ E('pre', {},
				'modem-extra-tools status --json\nmodem-extra-tools ttl set 65 off wan\n' +
				'modem-extra-tools ttl disable\nmodem-extra-tools bands show\n' +
				'modem-extra-tools bands set 3,7\nmodem-extra-tools bands restore\n' +
				'modem-extra-tools imei show\n' +
				'modem-extra-tools imei restore ORIGINAL_15_DIGIT_IMEI --confirm-original-imei\n' +
				'modem-extra-tools simlock status\n' +
				'modem-extra-tools simlock imei\n' +
				'modem-extra-tools simlock nck\n' +
				'modem-extra-tools simlock unlock --expected-state 0 --confirm-sim-unlock\n' +
				'modem-extra-tools simlock erase --confirm-sim-erase\n' +
				'modem-extra-tools simlock lock 5_OR_6_DIGIT_NETWORK_CODE --confirm-sim-lock'),
				E('p', { 'class': 'cbi-value-description' }, _('IMEI restore accepts only the original 15-digit value printed for your router and is never automatic.')),
				E('p', { 'class': 'cbi-value-description' }, _('The NCK and unlock commands read the IMEI or NCK from standard input. Avoid supplying sensitive values as shell arguments: those can remain in history or appear in the process list.')) ]));
			dom.content(body, content);
			if (window.HH71) window.HH71.decorate(body);
		}
		poll.add(function() {
			if (currentJob.state !== 'running' || busy) return Promise.resolve();
			return job().then(function(result) {
				if (result.state === 'running') return;
				currentJob = result;
				var simlockOperation = result.state_before != null || result.id === simlockJobId;
				simlockJobId = null;
				if (simlockOperation) {
					if (result.ok === false) {
						lock = { unread: true, pending: true, error: String(result.error || _('Modem operation failed')) };
						notify(result);
						return simlockStatus().then(check).then(function(latest) { lock = latest; })
							.catch(notify).then(refresh);
					}
					lock = result;
					// The backend already settled and verified the outcome against the modem,
					// so the message states what happened instead of hedging about it.
					var done = result.erased === true || result.unlocked === true || result.applied === true;
					var message;
					if (result.applied === true) message = _('This router is now locked to network ') +
						String(result.locked_plmn || '') + _('. The SIM in it keeps working.');
					else if (done) message = _('The carrier lock has been removed. This router now accepts a SIM card from any network.');
					else if (result.wrong_code === true) message = _('That unlock code was not accepted.') +
						(typeof result.attempts_left === 'number' ?
							' ' + _('Attempts left: ') + String(result.attempts_left) + '.' : '');
					else message = result.note ? String(result.note) :
						_('The result could not be confirmed. Read the status again before trying anything else.');
					ui.addNotification(null, E('p', {}, txt(message)), done ? 'info' : 'warning');
				} else if (result.ok === false) notify(result);
				else ui.addNotification(null, E('p', {}, _('Modem operation completed.')), 'info');
				return refresh();
			}).catch(notify);
		}, 2);
		draw();
		if (currentJob.state !== 'running') readLock();
		return body;
	}
});
