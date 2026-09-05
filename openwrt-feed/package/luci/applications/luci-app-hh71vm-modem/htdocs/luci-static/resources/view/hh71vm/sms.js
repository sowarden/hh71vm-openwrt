'use strict';
'require view';
'require ui';
'require dom';
'require hh71vm.modem as m';

/* Messages.  The list is fetched on demand, never on a timer: AT+CMGL clears the
 * modem's own unread flag on everything it returns, so polling it would destroy the
 * very state the page is showing.  (The daemon keeps its own read state; new arrivals
 * announce themselves through +CMTI and show up in the header counter.)
 */

/* A GSM-7 message fits 160 characters, 153 per segment once it is split; UCS2 -- which
 * anything outside the GSM alphabet needs -- fits 70, or 67 per segment. */
function segments(text) {
	var ucs2 = /[^\x00-\x7F]/.test(text);
	var lim = ucs2 ? 70 : 160, seg = ucs2 ? 67 : 153;
	var n = text.length <= lim ? (text.length ? 1 : 0) : Math.ceil(text.length / seg);
	return { ucs2: ucs2, count: n, limit: lim, used: text.length };
}

/* A failed modem read must never be rendered as a successful empty inbox.  Keep the
 * last cache visible when possible, but label it stale and preserve the real error. */
function loadMessages() {
	function fallback(error) {
		var detail = String((error && (error.error || error.message)) ||
		                    _('The message store could not be read.'));
		return L.resolveDefault(m.api.smsSnapshot(), {}).then(function (snapshot) {
			snapshot = snapshot || {};
			return {
				ok: false,
				error: detail,
				stale: true,
				messages: Array.isArray(snapshot.messages) ? snapshot.messages : [],
				generation: snapshot.generation,
				pending: snapshot.pending
			};
		});
	}

	return m.api.smsList().then(function (list) {
		list = list || {};
		return (list.ok === true && !list.error) ? list : fallback(list);
	}, fallback);
}

function loadPage() {
	/* Read status after the list operation so the page count and the global indicator
	 * describe the same cache generation rather than two sides of a CMGL refresh. */
	return loadMessages().then(function (list) {
		return m.api.status().then(function (status) { return [status || {}, list]; });
	});
}

return view.extend({
	handleSave: null,
	handleSaveApply: null,
	handleReset: null,

	load: function () {
		return loadPage();
	},

	render: function (data) {
		var st = data[0] || {}, list = data[1] || {};
		var body = E('div', {});
		var self = this;

		function reload() {
			return loadPage().then(function (d) {
				draw(d[0] || {}, d[1] || {});
			});
		}

		function compose(preset) {
			var to = E('input', { 'type': 'text', 'placeholder': '+380…',
			                      'value': (preset && preset.to) || '' });
			var text = E('textarea', { 'rows': 6, 'style': 'width:100%;max-width:none' });
			var counter = E('div', { 'class': 'cbi-value-description' }, ' ');

			function recount() {
				var s = segments(text.value);
				counter.textContent = _('%d characters · %d message(s) · %s alphabet')
					.format(s.used, s.count, s.ucs2 ? 'UCS2' : 'GSM-7');
			}
			text.addEventListener('input', recount);
			recount();

			function submit(send) {
				var dst = to.value.trim(), msg = text.value;
				if (!dst) return ui.addNotification(null,
					E('p', {}, _('Enter a destination number.')), 'warning');
				if (!msg) return ui.addNotification(null,
					E('p', {}, _('The message is empty.')), 'warning');
				if (send && !confirm(_('Send this message to %s? Your operator will charge \
for it.').format(dst))) return;
				var fn = send ? m.api.smsSend : m.api.smsSave;
				return m.checked(fn(dst, msg), send ? _('Message sent.')
				                                    : _('Message stored on the SIM/modem.'))
					.then(function () { ui.hideModal(); return reload(); })
					.catch(function (e) {
						ui.addNotification(null, E('p', {}, String(e.message || e)), 'error');
					});
			}

			ui.showModal(_('New message'), [
				E('div', { 'class': 'cbi-value' }, [
					E('label', { 'class': 'cbi-value-title' }, _('To')),
					E('div', { 'class': 'cbi-value-field' }, to)
				]),
				E('div', { 'class': 'cbi-value' }, [
					E('label', { 'class': 'cbi-value-title' }, _('Message')),
					E('div', { 'class': 'cbi-value-field' }, [text, counter])
				]),
				E('div', { 'class': 'cbi-page-actions' }, [
					E('button', { 'class': 'cbi-button', 'click': ui.hideModal },
					  _('Cancel')),
					E('button', { 'class': 'cbi-button cbi-button-neutral',
					              'click': function () { return submit(false); } },
					  _('Save without sending')),
					E('button', { 'class': 'cbi-button cbi-button-action',
					              'click': function () { return submit(true); } },
					  _('Send'))
				])
			]);
			text.focus();
		}

		function settingsDialog() {
			m.api.smsSettings().then(function (res) {
				res = res || {};
				var sca = E('input', { 'type': 'text',
				                       'value': (res.sms || {}).sca || '' });
				ui.showModal(_('Message settings'), [
					E('div', { 'class': 'cbi-value' }, [
						E('label', { 'class': 'cbi-value-title' }, _('Service centre')),
						E('div', { 'class': 'cbi-value-field' }, [sca,
							E('div', { 'class': 'cbi-value-description' },
							  _('The operator number that relays your messages. Change it \
only if your operator told you to.'))])
					]),
					m.facts([
						[_('Storage'), (res.sms || {}).storage],
						[_('Slots used'), ((res.sms || {}).used != null)
							? (res.sms).used + ' / ' + (res.sms).total : null],
						[_('Text parameters (CSMP)'), res.csmp, { mono: true }]
					]),
					E('div', { 'class': 'cbi-page-actions' }, [
						E('button', { 'class': 'cbi-button', 'click': ui.hideModal },
						  _('Cancel')),
						E('button', { 'class': 'cbi-button cbi-button-action',
							'click': function () {
								return m.checked(m.api.smsSettingsSet(sca.value.trim()),
								                 _('Service centre saved.'))
									.then(ui.hideModal)
									.catch(function (e) {
										ui.addNotification(null,
											E('p', {}, String(e.message || e)), 'error');
									});
							} }, _('Save'))
					])
				]);
			});
		}

		function messageCard(msg) {
			var acts = E('div', { 'class': 'msg-acts' }, [
				// the `read` argument says what to set it to, so the message being
				// unread right now is exactly the value we want to send
				m.action(msg.unread ? _('Mark read') : _('Mark unread'), 'neutral',
					function () {
						return m.checked(m.api.smsMark(msg.index, msg.ts,
						                               msg.unread === true))
							.then(reload);
					}),
				m.action(_('Copy'), 'neutral', function () {
					m.copyText(msg.text || '');
				}),
				m.action(_('Delete'), 'negative', function () {
					return m.checked(m.api.smsDelete(null, msg.indexes || [msg.index]),
					                 _('Message deleted.')).then(reload);
				}, _('Delete this message?'))
			]);

			return E('div', { 'class': 'msg' + (msg.unread ? ' unread' : '') }, [
				E('div', { 'class': 'msg-head' }, [
					E('span', { 'class': 'msg-from' }, msg.sender || '?'),
					E('span', { 'class': 'msg-time' }, m.smsTime(msg.ts)),
					msg.parts > 1 ? m.label(_('%d parts').format(msg.parts)) : E([]),
					/* a segment can still be on its way, or one slot of several may
					   have been deleted -- say so instead of showing a hole */
					msg.missing ? m.label(msg.missing === 1
					                      ? _('1 part missing')
					                      : _('%d parts missing').format(msg.missing),
					                      'warning') : E([]),
					msg.unread ? m.label(_('new'), 'notice') : E([]),
					msg.decode_error ? m.label(_('decode error'), 'warning') : E([]),
					(msg.status && msg.status.indexOf('STO') === 0)
						? m.label(_('draft'), 'warning') : E([]),
					acts
				]),
				E('div', { 'class': 'msg-body' }, msg.decode_error
					? _('This stored message could not be decoded. It remains available for marking or deletion by slot.')
					: (msg.text || ''))
			]);
		}

		function draw(st, list) {
			var sms = st.sms || {}, msgs = list.messages || [];
			var kids = [];
			var warn = m.linkState(st);
			if (warn) kids.push(warn);
			if (list.ok !== true) kids.push(E('div', { 'class': 'alert-message error' }, [
				E('h4', {}, _('Messages could not be refreshed')),
				E('p', {}, String(list.error || _('The message store could not be read.'))),
				msgs.length ? E('p', {}, _('The last cached messages are shown below.')) : E([])
			]));
			if ((list.decode_errors || 0) > 0) kids.push(E('div', {
				'class': 'alert-message warning'
			}, _('One or more stored messages could not be decoded. Other messages are still shown.')));

			var pct = (sms.total ? Math.round(100 * (sms.used || 0) / sms.total) : 0);
			var visibleUnread = msgs.filter(function (msg) { return msg.unread === true; }).length;
			if (list.ok === true && !list.pending && sms.unread !== visibleUnread)
				kids.push(E('div', { 'class': 'alert-message warning' },
					_('The unread indicator and the visible message list are temporarily out of sync.')));

			kids.push(E('div', { 'class': 'cbi-section fade-in' }, [
				E('h3', {}, _('Messages')),
				E('div', { 'class': 'cbi-section-descr' },
				  _('The modem stores a message in as many slots as it has parts, so the slot count below is normally higher than the number of messages.')),
				E('div', { 'class': 'mactions' }, [
					m.action(_('New message'), 'action', function () { compose(); }),
					m.action(_('Reload'), 'neutral', reload),
					m.action(_('Settings'), 'neutral', settingsDialog),
					m.action(_('Delete all'), 'negative', function () {
						return m.checked(m.api.smsDeleteAll(), _('All messages deleted.'))
							.then(reload);
					}, _('Delete every message stored on the modem? This cannot be undone.'))
				]),
				m.facts([
					[_('Messages'), String(msgs.length) +
						(sms.unread ? '  (' + _('%d unread').format(sms.unread) + ')' : '')],
					[_('Storage'), sms.storage],
					[_('Slots used'), E('div', {
							'class': 'cbi-progressbar',
							'title': '%d / %d (%d%%)'.format(sms.used || 0, sms.total || 0, pct)
						}, E('div', { 'style': 'width:%d%%'.format(pct) })), { raw: true }],
					[_('Service centre'), sms.sca, { copy: true }]
				])
			]));

			if (!msgs.length && list.ok !== true) {
				kids.push(E('div', { 'class': 'cbi-section fade-in' }, [
					E('h3', {}, _('Inbox')),
					E('p', { 'class': 'cbi-value-description' },
					  _('No message rows can be shown until the message store refresh succeeds.'))
				]));
			} else if (!msgs.length) {
				kids.push(E('div', { 'class': 'cbi-section fade-in' }, [
					E('h3', {}, _('Inbox')),
					E('p', { 'class': 'cbi-value-description' },
					  _('No messages are stored on the modem. Incoming messages appear here on their own; use "New message" to write one.'))
				]));
			} else {
				var cards = [E('h3', {}, _('Inbox') + ' (' + msgs.length + ')')];
				for (var i = msgs.length - 1; i >= 0; i--)
					cards.push(messageCard(msgs[i]));
				kids.push(E('div', { 'class': 'cbi-section fade-in' }, cards));
			}

			dom.content(body, kids);
		}

		draw(st, list);
		return body;
	}
});
