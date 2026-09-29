'use strict';
'require baseclass';
'require ui';
'require dom';
'require poll';
'require hh71vm.modem as m';

/* Carrier (network) SIM lock, on the modem overview page.
 *
 * This lives in the base image rather than in an add-on package for one reason: a router
 * that refuses the SIM in it cannot install a package first.  The page therefore has to
 * be able to say what is wrong and undo it with nothing else installed.
 *
 * Reading the lock is not free -- it opens a session to the Qualcomm side and reads the
 * modem's personalisation list -- so it is never part of the five second page poll.  The
 * cheap signal that something is wrong comes from the modem daemon's own SIM status
 * ("PH-NET PIN"), which the page already polls; the expensive read happens once when that
 * says a lock is being enforced, or when the user asks for it.
 */

var style =
	'#msl .msl-state{display:flex;gap:12px;align-items:flex-start;border:1px solid var(--border,#ccc);' +
		'border-left-width:5px;border-radius:8px;padding:14px 16px}' +
	'#msl .msl-state.ok{border-left-color:var(--ok,#2e7d32)}' +
	'#msl .msl-state.warn{border-left-color:var(--warn,#ef6c00)}' +
	'#msl .msl-state.info{border-left-color:var(--accent,#2979ff)}' +
	'#msl .msl-state.idle{border-left-color:var(--border,#ccc)}' +
	'#msl .msl-state h4{margin:0;font-size:16px}' +
	'#msl .msl-state p{margin:6px 0 0;color:var(--muted,#777);line-height:1.5}' +
	'#msl .msl-body{flex:1 1 auto;min-width:0}' +
	'#msl .msl-state > button{flex:0 0 auto}' +
	'#msl .msl-step{margin-top:18px;padding-top:18px;border-top:1px solid var(--border,#ccc)}' +
	'#msl .msl-step h4{margin:0;font-size:15px}' +
	'#msl .msl-step > p{margin:4px 0 0;color:var(--muted,#777);line-height:1.5}' +
	'#msl .msl-note{color:var(--muted,#777);font-size:12.5px;line-height:1.5;margin:8px 0 0}' +
	'#msl .msl-row{display:flex;align-items:flex-end;gap:10px;flex-wrap:wrap;margin-top:12px}' +
	'#msl .msl-row > label{display:flex;flex-direction:column;gap:6px;flex:1 1 220px;min-width:0;font-weight:600}' +
	'#msl .msl-row input{width:100%;max-width:100%;box-sizing:border-box}' +
	'#msl .msl-row > button{flex:0 0 auto}' +
	'#msl button.cbi-button{min-height:36px;padding:6px 14px;line-height:22px;white-space:normal;' +
		'box-sizing:border-box;margin:0}' +
	'#msl .msl-codes{border:1px solid var(--border,#ccc);border-radius:8px;' +
		'background:var(--surface-2,rgba(127,127,127,.06));padding:12px 14px;margin-top:14px}' +
	'#msl .msl-code{margin-top:14px}#msl .msl-code:first-child{margin-top:0}' +
	'#msl .msl-code > span{display:block;font-weight:600;margin-bottom:6px}' +
	'#msl .msl-code > span em{font-style:normal;font-weight:400;color:var(--muted,#777)}' +
	'#msl .msl-code input{font-size:1.1em;letter-spacing:.04em}' +
	'#msl .msl-code.msl-want input{border:2px solid var(--accent,#2979ff)}' +
	'#msl .msl-codes > p{margin:6px 0 0}' +
	'#msl details{margin-top:14px}#msl summary{cursor:pointer;font-weight:600}' +
	'@media(max-width:600px){' +
		'#msl .msl-state{flex-direction:column;align-items:stretch}' +
		'#msl .msl-state > button{width:100%}' +
		'#msl .msl-row{flex-direction:column;align-items:stretch}' +
		/* In a column the label's flex basis becomes a height, which opens a large gap. */
		'#msl .msl-row > label{flex:0 0 auto}' +
		'#msl .msl-row > button{width:100%}' +
	'}';

function sentence(text) {
	var v = String(text || '').trim();
	if (!v) return '';
	v = v.charAt(0).toUpperCase() + v.slice(1);
	return /[.!?]$/.test(v) ? v : v + '.';
}

/* True when the modem daemon's own SIM status says a carrier lock is holding the card.
   Cheap: it comes from the status the page already polls. */
function refusedBySimlock(st) {
	var sim = (st || {}).sim || {};
	return String(sim.status || '').toUpperCase().indexOf('PH-NET') === 0;
}

return baseclass.extend({
	refusedBySimlock: refusedBySimlock,

	/* Returns a node that manages itself.  The caller keeps the same node across its own
	   redraws, so a typed code and a finished read are not thrown away every poll. */
	widget: function () {
		var root = E('div', { 'id': 'msl' });
		var lock = { unread: true }, job = { state: 'idle' }, busy = false;
		var liveImei = null, imeiValue = '', generated = null, identityError = null;
		var code = { 10: '', 16: '' }, autoRead = false;

		function clearCodes() { code = { 10: '', 16: '' }; generated = null; }
		function setBusy(on) {
			busy = on;
			if (on) Array.prototype.forEach.call(root.querySelectorAll('button'),
				function (b) { b.disabled = true; });
		}
		function isCalculated(len) {
			return generated !== null && generated.imei === imeiValue &&
				code[len] === generated['nck' + len];
		}
		function button(text, kind, disabled, fn) {
			return E('button', {
				'class': 'cbi-button cbi-button-' + (kind || 'neutral'),
				'type': 'button',
				'disabled': (disabled || busy || job.state === 'running') ? 'disabled' : null,
				'click': ui.createHandlerFn(this, fn)
			}, m.text(text));
		}
		function fail(e) {
			ui.addNotification(null, E('p', {}, [ String(e.message || e) ]), 'error');
		}

		function read() {
			if (busy || job.state === 'running') return Promise.resolve();
			setBusy(true);
			return m.api.simlockStatus().then(function (res) {
				res = res || {};
				if (res.ok === false) throw new Error(res.error || _('No valid response'));
				lock = res;
				if (lock.lock_state !== 'challenged') { clearCodes(); imeiValue = ''; liveImei = null; }
			}).catch(function (e) {
				lock = { unread: true, error: String(e.message || e) };
				clearCodes(); imeiValue = ''; liveImei = null;
				fail(e);
			}).then(function () {
				setBusy(false); draw();
				if (lock.lock_state === 'challenged' && !liveImei) return readImei();
			});
		}
		function readImei() {
			if (busy || job.state === 'running') return Promise.resolve();
			setBusy(true);
			return m.api.simlockImei().then(function (res) {
				res = res || {};
				if (res.ok === false) throw new Error(res.error || _('No valid response'));
				var previous = liveImei;
				if (isCalculated(10)) code[10] = '';
				if (isCalculated(16)) code[16] = '';
				liveImei = res.imei;
				identityError = null;
				if (!imeiValue || imeiValue === previous) imeiValue = liveImei;
				generated = null;
			}).catch(function () {
				identityError = _('The modem did not report its IMEI; type the 15 digits yourself.');
			}).then(function () { setBusy(false); draw(); });
		}
		function startJob(call) {
			setBusy(true);
			return call().then(function (res) {
				res = res || {};
				if (res.ok === false) throw new Error(res.error || _('No valid response'));
				job = res; lock = { unread: true, pending: true };
			}).catch(function (e) {
				lock = { unread: true, pending: true, error: String(e.message || e) };
				fail(e);
			}).then(function () { setBusy(false); draw(); });
		}

		/* Removing the lock without a code is the path that always works: no code, no
		   attempt spent, and it works with the counter already at zero.  It is offered
		   first for that reason, with the code path kept in full underneath. */
		function removalStep(heading, explanation, label) {
			var kids = [ E('h4', {}, heading), E('p', {}, explanation) ];
			/* The build is advisory: it is offered on every modem, because trying costs no
			   code and no attempt, and a report from an untested build is the only way the
			   known-good list grows. */
			if (lock.can_erase && lock.tested_build !== true)
				kids.push(E('p', { 'class': 'alert-message warning' }, m.text(sentence(
					lock.tested_build === false
						? _('This modem runs a Qualcomm firmware build that keyless removal has not been tested on, so it may simply do nothing. Trying is safe: no unlock code is sent and no attempt is used. Please report whether it worked, with your firmware version.')
						: _('The Qualcomm firmware build could not be read, so it is not known whether keyless removal works here. Trying is safe: no unlock code is sent and no attempt is used. Please report whether it worked, with your firmware version.')))));
			if (lock.can_erase)
				kids.push(E('div', { 'class': 'msl-row' }, button(label, 'negative', false, function () {
					var ack = E('input', { 'type': 'checkbox' });
					ui.showModal(label, [
						E('p', {}, _('The carrier lock will be removed from this modem. No unlock code is needed and no attempt is used.')),
						E('p', { 'class': 'alert-message warning' },
							_('Mobile service drops for a few seconds while the modem re-reads the SIM. The request is sent once and never repeated automatically.')),
						lock.tested_build === true ? '' : E('p', {},
							_('This has not been tested on your modem firmware. If nothing changes, the unlock code below still works.')),
						E('label', {}, [ ack, ' ', _('I own this router and want its carrier lock removed.') ]),
						E('div', { 'class': 'cbi-page-actions' }, [
							E('button', { 'class': 'cbi-button', 'click': ui.hideModal }, _('Cancel')),
							E('button', { 'class': 'cbi-button cbi-button-negative', 'click': function () {
								if (!ack.checked) { fail(new Error(_('Tick the box to confirm.'))); return; }
								ui.hideModal(); clearCodes();
								startJob(function () { return m.api.simlockErase(true); });
							} }, _('Remove the lock')) ]) ]);
				})));
			else kids.push(E('p', { 'class': 'alert-message warning' },
				m.text(sentence(_('Not available right now: ') + (lock.erase_refusal || _('the modem could not be checked'))))));
			return E('div', { 'class': 'msl-step' }, kids);
		}

		function codeBox(len, attempts) {
			var want = lock.code_length === len;
			var re = new RegExp('^\\d{' + len + '}$');
			var field = E('input', { 'type': 'text', 'inputmode': 'numeric',
				'pattern': '[0-9]{' + len + '}', 'maxlength': len, 'autocomplete': 'off',
				'value': code[len], 'aria-label': len + _('-digit unlock code'),
				'input': function () { code[len] = field.value; send.disabled = !re.test(field.value) || busy; } });
			var send = button(_('Unlock with this code'), want ? 'negative' : 'neutral',
				!re.test(code[len]), function () {
				var value = field.value;
				if (!re.test(value)) { fail(new Error(_('This code must be exactly ') + len + _(' digits.'))); return; }
				var mine = isCalculated(len) && liveImei === imeiValue;
				var needAck = !mine || !want;
				var ack = E('input', { 'type': 'checkbox' });
				var expected = typeof lock.state === 'number' ? lock.state : 0;
				ui.showModal(_('Send the ') + len + _('-digit code'), [
					E('p', {}, m.text(_('The code is sent once.') +
						(attempts !== null ? ' ' + _('Attempts left: ') + String(attempts) + '.' : ''))),
					E('p', { 'class': 'alert-message warning' },
						m.text((!want && lock.code_length) ?
							_('This modem asked for a ') + String(lock.code_length) +
								_('-digit code, so this one will almost certainly be refused and will use one attempt.') :
						mine ? _('This code was calculated for this modem. A wrong code still uses one attempt.') :
						isCalculated(len) ? _('This code was calculated for a different IMEI than the modem reports. Do not send it unless you know it belongs to this modem.') :
						_('This code was typed or pasted. A wrong code uses one attempt.'))),
					needAck ? E('label', {}, [ ack, ' ',
						_('I understand this may use one of the remaining attempts.') ]) : '',
					E('div', { 'class': 'cbi-page-actions' }, [
						E('button', { 'class': 'cbi-button', 'click': ui.hideModal }, _('Cancel')),
						E('button', { 'class': 'cbi-button cbi-button-negative', 'click': function () {
							if (needAck && !ack.checked) { fail(new Error(_('Tick the box to confirm.'))); return; }
							ui.hideModal(); clearCodes();
							startJob(function () { return m.api.simlockUnlock(value, expected, true); });
						} }, _('Send the code')) ]) ]);
			});
			return E('div', { 'class': 'msl-code' + (want ? ' msl-want' : '') }, [
				E('span', {}, [ len + _('-digit code'), ' ',
					E('em', {}, want ? _('— your modem asked for this length') :
						lock.code_length ? _('— not what this modem asked for') : '') ]),
				E('div', { 'class': 'msl-row' }, [ E('label', {}, field), send ])
			]);
		}

		function draw() {
			var known = !lock.unread && !lock.pending && !lock.error;
			var state = known ? lock.lock_state : null;
			var challenged = state === 'challenged';
			var lockedOn = state === 'allowed';
			var attempts = typeof lock.attempts_left === 'number' ? lock.attempts_left : null;
			var network = lock.locked_plmn ? String(lock.locked_plmn) : null;
			var banner;

			if (lock.error) banner = [ 'warn', _('The carrier lock could not be read'),
				sentence(lock.error) ];
			else if (lock.pending) banner = [ 'info', _('Checking how it went'),
				_('The last request was sent to the modem. Read the status again in a moment; do not repeat it.') ];
			else if (lock.unread) banner = [ 'idle', _('Carrier lock not checked yet'),
				_('Checking asks the modem directly and changes nothing.') ];
			else if (challenged) banner = [ 'warn', _('This SIM is blocked by a carrier lock'),
				_('The modem will not use the SIM in this router until the lock is removed or the right unlock code is entered.') +
					(attempts !== null ? ' ' + _('Code attempts left: ') + String(attempts) + '.' : '') ];
			else if (lockedOn) banner = [ 'ok',
				network ? _('Locked to network ') + network : _('A carrier lock is active'),
				(lock.locked_to_home === true ?
					_('The SIM in this router belongs to that network, so it keeps working normally.') :
					_('The SIM in this router is accepted by this lock.')) + ' ' +
					_('A SIM from any other network would be refused until the lock is removed.') ];
			else if (state === 'none') banner = [ 'ok', _('No carrier lock'),
				_('This router accepts a SIM card from any mobile network.') ];
			else if (lock.settling_after_removal) banner = [ 'info', _('The lock is being removed'),
				_('The modem no longer holds a carrier lock, but the SIM is still restarting. Give it a few seconds and press Refresh.') ];
			else if (lock.sim_failure) banner = [ 'warn', _('The modem lost the SIM'),
				_('The modem reports a SIM failure, so the carrier lock cannot be read right now. This usually clears on its own within a minute; if it does not, use Restart modem below.') ];
			else banner = [ 'warn', _('The carrier lock state is not clear'),
				sentence(lock.lock_state_reason) || _('The modem did not give a complete answer.') ];

			var kids = [ E('div', { 'class': 'msl-state ' + banner[0] }, [
				E('div', { 'class': 'msl-body' }, [ E('h4', {}, m.text(banner[1])), E('p', {}, m.text(banner[2])) ]),
				button(lock.unread && !lock.error ? _('Check now') : _('Refresh'), 'neutral', false, read)
			]) ];
			if (lock.read_error) kids.push(E('p', { 'class': 'msl-note' },
				m.text(_('Part of the reading failed: ') + sentence(lock.read_error))));
			if (known && lock.cleanup && lock.cleanup.ok === false)
				kids.push(E('p', { 'class': 'alert-message warning' },
					m.text(_('The lock was removed, but the saved carrier setting could not be cleared: ') +
						sentence(lock.cleanup.error))));

			if (challenged) {
				kids.push(removalStep(_('Easiest: remove the lock, no code needed'),
					_('Clears the carrier lock from the modem so this SIM and any other SIM work again. It does not use an unlock attempt and works even when no attempts are left.'),
					_('Remove the lock without a code')));

				var codeKids = [ E('h4', {}, _('Or: enter the unlock code (NCK)')),
					E('p', {}, m.text(lock.code_length ?
						_('Your modem asked for a ') + String(lock.code_length) +
							_('-digit code, so use that one. Calculate it from the modem\'s IMEI, or paste a code your carrier gave you. A wrong code uses one of the remaining attempts.') :
						_('Paste the code your carrier gave you. A wrong code uses one of the remaining attempts.'))) ];
				var imeiField = E('input', { 'type': 'text', 'inputmode': 'numeric',
					'pattern': '[0-9]{15}', 'maxlength': 15, 'autocomplete': 'off',
					'value': imeiValue, 'aria-label': _('IMEI used to calculate the code'),
					'input': function () {
						imeiValue = imeiField.value; generated = null;
						calc.disabled = !/^\d{15}$/.test(imeiValue) || busy;
					} });
				var calc = button(_('Calculate the code'), 'action', !/^\d{15}$/.test(imeiValue), function () {
					var typed = imeiField.value;
					if (!/^\d{15}$/.test(typed)) { fail(new Error(_('Enter exactly 15 IMEI digits.'))); return; }
					setBusy(true);
					return m.api.simlockNck(typed).then(function (res) {
						res = res || {};
						if (res.ok === false) throw new Error(res.error || _('No valid response'));
						generated = { imei: typed, nck10: res.nck10, nck16: res.nck16 };
						imeiValue = typed; code[10] = res.nck10; code[16] = res.nck16;
					}).catch(fail).then(function () { setBusy(false); draw(); });
				});
				codeKids.push(E('div', { 'class': 'msl-row' }, [
					E('label', {}, [ _('IMEI of this modem'), imeiField ]),
					button(_('Use this modem\'s IMEI'), 'neutral', false, readImei), calc
				]));
				codeKids.push(E('p', { 'class': 'msl-note' }, liveImei && imeiValue === liveImei ?
					_('Filled in from the modem itself. Calculating a code sends nothing and is always safe.') :
					_('Use the IMEI the modem reports, which can differ from the sticker. Calculating a code sends nothing and is always safe.')));
				if (liveImei && imeiValue && imeiValue !== liveImei)
					codeKids.push(E('p', { 'class': 'alert-message warning' },
						_('This IMEI is not the one this modem reports. Calculating a code is harmless, but sending a code made from the wrong IMEI will use up one attempt.')));
				if (identityError) codeKids.push(E('p', { 'class': 'alert-message warning' }, m.text(identityError)));
				if (lock.can_unlock) {
					var lengths = [ lock.code_length ];
					[ 10, 16 ].forEach(function (l) { if (lengths.indexOf(l) < 0) lengths.push(l); });
					codeKids.push(E('div', { 'class': 'msl-codes' },
						lengths.map(function (l) { return codeBox(l, attempts); })
							.concat([ E('p', { 'class': 'msl-note' },
								_('Edit a box or paste a code over it before sending. Nothing reaches the modem until you press a button.')) ])));
				} else if (lock.unlock_refusal) {
					codeKids.push(E('p', { 'class': 'alert-message warning' },
						m.text(sentence(_('The code cannot be sent right now: ') + lock.unlock_refusal))));
				}
				kids.push(E('div', { 'class': 'msl-step' }, codeKids));
			}

			/* A removal whose readback was cut short by the card dropping out clears the
			   lock but not the saved rule, and then nothing retries it.  Offer it here so
			   the status stops reporting a carrier setting that nothing is using. */
			if (state === 'none' && lock.lock_provisioned === true) kids.push(
				E('div', { 'class': 'msl-step' }, [
					E('h4', {}, _('Leftover carrier setting')),
					E('p', {}, m.text(_('The lock is gone, but this router still has a saved setting for network ') +
						String(lock.saved_network_code || '') +
						_('. Nothing is using it and it cannot lock the modem by itself, but it can be cleared.'))),
					E('div', { 'class': 'msl-row' }, button(_('Clear the saved setting'), 'neutral', false, function () {
						setBusy(true);
						return m.api.simlockClearRule(true).then(function (res) {
							res = res || {};
							if (res.ok === false) throw new Error(res.error || _('No valid response'));
							lock = res;
							ui.addNotification(null, E('p', {}, m.text(res.note ? String(res.note) :
								_('The saved carrier setting is cleared.'))), res.cleared ? 'info' : 'warning');
						}).catch(fail).then(function () { setBusy(false); draw(); });
					}))
				]));

			if (lockedOn) kids.push(removalStep(_('Remove the carrier lock'),
				_('Clears the lock so this router accepts a SIM card from any network. No unlock code is needed.'),
				_('Remove the carrier lock')));

			if (known) {
				var rows = [
					[_('Carrier lock'), challenged ? _('on, this SIM refused') :
						lockedOn ? _('on, this SIM allowed') :
						state === 'none' ? _('off') : _('unclear')],
					[_('Locked network code'), network],
					[_('Network of the inserted SIM'), lock.home_plmn],
					[_('Code attempts left'), attempts],
					[_('Code length this modem wants'), lock.code_length
						? lock.code_length + _(' digits') : null],
					[_('SIM'), lock.card ? lock.card.cpin : null],
					[_('Modem lock facility (PN)'), lock.card && lock.card.pn != null
						? (lock.card.pn === 1 ? _('enabled') : _('disabled')) : null],
					[_('Personalisation entries in the modem'), lock.uim ? lock.uim.feature_count : null],
					[_('Saved carrier setting'), lock.lock_provisioned === true
						? _('present') + (lock.saved_network_code ? ' (' + lock.saved_network_code + ')' : '')
						: lock.lock_provisioned === false ? _('cleared') : null]
				];
				kids.push(E('details', {}, [ E('summary', {}, _('Technical details')), m.facts(rows),
					E('p', { 'class': 'msl-note' },
						_('The carrier lock is read from the modem itself. The saved carrier setting only describes what a future lock would use.')) ]));
			}

			dom.content(root, [ E('style', {}, style),
				m.section(_('Carrier SIM lock'), kids) ]);
			if (window.HH71) window.HH71.decorate(root);
		}

		/* The expensive read happens by itself only when the cheap signal already says the
		   card is being refused, and only once per visit. */
		root.hh71Notice = function (st) {
			if (autoRead || !refusedBySimlock(st)) return;
			autoRead = true;
			read();
		};

		poll.add(function () {
			if (job.state !== 'running' || busy) return Promise.resolve();
			return m.api.simlockJob().then(function (res) {
				res = res || {};
				if (res.state === 'running') return;
				job = res;
				if (res.ok === false) {
					lock = { unread: true, error: String(res.error || _('The operation failed')) };
					ui.addNotification(null, E('p', {}, [ String(res.error || _('The operation failed')) ]), 'error');
					draw();
					return read();
				}
				lock = res;
				var done = res.erased === true || res.unlocked === true;
				var text;
				if (done) text = _('The carrier lock has been removed. This router now accepts a SIM card from any network.');
				else if (res.wrong_code === true) text = _('That unlock code was not accepted.') +
					(typeof res.attempts_left === 'number' ?
						' ' + _('Attempts left: ') + String(res.attempts_left) + '.' : '');
				else text = res.note ? String(res.note) :
					_('The result could not be confirmed. Check the status again before trying anything else.');
				ui.addNotification(null, E('p', {}, m.text(text)), done ? 'info' : 'warning');
				draw();
			}).catch(function () {});
		}, 2);

		draw();
		return root;
	}
});
