'use strict';
// SPDX-License-Identifier: Apache-2.0
'require view';
'require rpc';
'require ui';
'require dom';

var statusCall = rpc.declare({ object: 'sms-to-telegram', method: 'status' });
var configGet = rpc.declare({ object: 'sms-to-telegram', method: 'config_get' });
var configSet = rpc.declare({ object: 'sms-to-telegram', method: 'config_set',
	params: [ 'token', 'chat_id', 'remove_after_send' ] });
var discoverChat = rpc.declare({ object: 'sms-to-telegram', method: 'discover_chat', params: [ 'token' ] });

function checked(result) {
	if (!result || result.ok !== true) {
		var error = new Error(result && result.error || 'invalid_response');
		error.retryAfter = result && result.retry_after;
		throw error;
	}
	return result;
}

function message(code, retryAfter) {
	var messages = {
		invalid_token: _('The bot token is invalid. Copy the complete token from BotFather and try again.'),
		invalid_chat_id: _('Enter a positive numeric Telegram private chat ID. Usernames, links, spaces, zero and negative values are not accepted.'),
		no_private_chat: _('No recent private chats were found. Send your bot a new private message, then try detection again.'),
		too_many_private_chats: _('Too many private chats were returned. Send the bot a fresh message from the intended account and try again later.'),
		telegram_rate_limited: retryAfter ?
			_('Telegram asked this router to wait %d seconds. Try again after that delay.').format(retryAfter) :
			_('Telegram rate-limited this request. Wait briefly, then try again.'),
		telegram_http_error: _('Telegram returned an HTTP error. Check the token and try again later.'),
		telegram_api_error: _('Telegram rejected the request. Check the token and try again.'),
		telegram_transport_failed: _('The router could not reach Telegram before the request timed out. Check internet access and try again.'),
		telegram_invalid_response: _('Telegram returned data that could not be handled safely. Send a fresh private message and try again.'),
		config_write_failed: _('The configuration could not be saved.'),
		modem_unavailable: _('The modem SMS service is temporarily unavailable.'),
		sim_delete_failed: _('A delivered SMS is waiting for another SIM deletion attempt.'),
		sim_delete_unconfirmed: _('SIM deletion could not be confirmed and will be retried safely.'),
		internal_error: _('The service encountered an internal error. Its delivery state was preserved.')
	};
	return messages[code] || _('The operation failed safely.');
}

function notify(error) {
	ui.addNotification(null, E('p', {}, message(String(error && (error.message || error.error) || error),
		error && error.retryAfter)), 'error');
}

function field(label, input, description) {
	return E('div', { 'class': 'cbi-value' }, [
		E('div', { 'class': 'cbi-value-title' }, label),
		E('div', { 'class': 'cbi-value-field' }, [
			input,
			E('div', { 'class': 'cbi-value-description' }, description)
		])
	]);
}

function step(number, title, children) {
	return E('section', { 'class': 'cbi-section', 'style': 'margin-bottom:1em' }, [
		E('h3', { 'style': 'margin-top:0' }, _('Step %d — %s').format(number, title))
	].concat(children));
}

function statusPill(text, good) {
	return E('span', {
		'class': good ? 'label success' : 'label warning',
		'style': 'display:inline-block;margin:0 .5em .35em 0'
	}, text);
}

function candidateLabel(candidate) {
	var parts = [ E('strong', {}, _('Chat ID: %s').format(candidate.chat_id)) ];
	if (candidate.username) {
		parts.push(document.createTextNode(' — '));
		parts.push(E('bdi', { 'dir': 'auto' }, document.createTextNode('@' + candidate.username)));
	}
	else {
		var names = [];
		if (candidate.first_name) names.push(candidate.first_name);
		if (candidate.last_name) names.push(candidate.last_name);
		if (names.length) {
			parts.push(document.createTextNode(' — '));
			parts.push(E('bdi', { 'dir': 'auto' }, document.createTextNode(names.join(' '))));
		}
	}
	return parts;
}

return view.extend({
	load: function() {
		return Promise.all([ statusCall(), configGet() ]);
	},

	validate: function() {
		var token = this.form.token.value;
		var chat = this.form.chat.value;
		if (token && !/^[1-9][0-9]+:[A-Za-z0-9_-]+$/.test(token))
			throw new Error('invalid_token');
		if (chat && !/^[1-9][0-9]{4,19}$/.test(chat))
			throw new Error('invalid_chat_id');
	},

	setBusy: function(value) {
		this.busy = value;
		if (this.form && this.form.detect) this.form.detect.disabled = value;
	},

	drawStatus: function(state) {
		var children = [
			statusPill(state.configured ? _('Configured') : _('Not configured'), state.configured),
			statusPill(state.running ? _('Running') : _('Stopped'), state.running),
			statusPill(_('Pending: %d').format(state.pending || 0), (state.pending || 0) === 0),
			statusPill(_('Pending SIM deletion: %d').format(state.pending_delete || 0), (state.pending_delete || 0) === 0)
		];
		if (state.last_error)
			children.push(E('div', { 'class': 'cbi-value-description' }, [
				E('strong', {}, _('Last issue: ')), message(String(state.last_error))
			]));
		return children;
	},

	handleSave: null,

	saveConfiguration: function() {
		var self = this;
		if (!this.form || this.busy) return Promise.resolve();
		try { this.validate(); } catch (error) { notify(error); return Promise.resolve(); }
		this.setBusy(true);
		return configSet(this.form.token.value, this.form.chat.value, this.form.remove.checked)
			.then(checked)
			.then(function() {
				self.form.token.value = '';
				ui.addNotification(null, E('p', {}, _('SMS to Telegram settings were saved. The service will use them on its next polling cycle.')), 'info');
				return Promise.all([ statusCall(), configGet() ]);
			})
			.then(function(values) {
				var state = checked(values[0]);
				var config = checked(values[1]);
				self.form.token.placeholder = config.token_set ?
					_('Configured — leave blank to keep the current token') : _('Paste the bot token');
				dom.content(self.form.status, self.drawStatus(state));
				return state;
			})
			.catch(function(error) { notify(error); })
			.then(function(value) { self.setBusy(false); return value; });
	},

	handleSaveApply: function() {
		return this.saveConfiguration();
	},

	handleReset: function() {
		var self = this;
		if (!this.form || this.busy) return Promise.resolve();
		this.setBusy(true);
		return Promise.all([ statusCall(), configGet() ]).then(function(values) {
			var state = checked(values[0]);
			var config = checked(values[1]);
			self.form.token.value = '';
			self.form.token.placeholder = config.token_set ?
				_('Configured — leave blank to keep the current token') : _('Paste the bot token');
			self.form.chat.value = config.chat_id || '';
			self.form.remove.checked = config.remove_after_send === true;
			dom.content(self.form.candidates, '');
			dom.content(self.form.status, self.drawStatus(state));
		}).catch(notify).then(function() { self.setBusy(false); });
	},

	render: function(data) {
		var self = this;
		var state = checked(data[0]);
		var config = checked(data[1]);
		var body = E('div');
		var token = E('input', {
			'type': 'password',
			'autocomplete': 'new-password',
			'value': '',
			'placeholder': config.token_set ? _('Configured — leave blank to keep the current token') : _('Paste the bot token'),
			'style': 'width:32em;max-width:100%',
			'aria-label': _('Telegram Bot Token')
		});
		var chat = E('input', {
			'type': 'text',
			'inputmode': 'numeric',
			'pattern': '[0-9]+',
			'maxlength': 20,
			'value': config.chat_id || '',
			'style': 'width:22em;max-width:100%',
			'aria-label': _('Destination Chat ID')
		});
		var remove = E('input', { 'type': 'checkbox', 'checked': config.remove_after_send ? 'checked' : null });
		var candidates = E('div', { 'style': 'margin-top:.75em' });
		var status = E('div', { 'class': 'alert-message', 'style': 'margin-bottom:1em' }, this.drawStatus(state));
		var detect = E('button', {
			'class': 'cbi-button cbi-button-action',
			'type': 'button',
			'click': function() {
				if (self.busy) return;
				self.setBusy(true);
				dom.content(candidates, E('p', {}, _('Checking recent Telegram updates…')));
				discoverChat(token.value).then(checked).then(function(result) {
					var radios = [];
					result.candidates.forEach(function(candidate) {
						var radio = E('input', {
							'type': 'radio',
							'name': 'sms-to-telegram-recipient',
							'value': candidate.chat_id,
							'change': function() { if (radio.checked) chat.value = candidate.chat_id; }
						});
						radios.push(E('label', {
							'style': 'display:flex;align-items:flex-start;gap:.55em;padding:.45em 0;overflow-wrap:anywhere'
						}, [ radio, E('span', {}, candidateLabel(candidate)) ]));
					});
					dom.content(candidates, [
						E('p', {}, result.candidates.length === 1 ?
							_('One recent private chat was found. Select it below.') :
							_('%d recent private chats were found. Select the intended recipient.').format(result.candidates.length)),
						E('div', { 'class': 'cbi-value-description' }, radios)
					]);
				}).catch(function(error) {
					dom.content(candidates, '');
					notify(error);
				}).then(function() { self.setBusy(false); });
			}
		}, _('Detect Chat IDs'));

		this.busy = false;
		this.form = { token: token, chat: chat, remove: remove, candidates: candidates, status: status, detect: detect };

		dom.content(body, [
			E('h2', {}, _('SMS to Telegram')),
			E('p', {}, _('Forward SMS messages received by this router to one private Telegram chat. Complete the six short steps below.')),
			status,
			step(1, _('Create a Telegram bot'), [
				E('ol', {}, [
					E('li', {}, [ _('Open Telegram and start a chat with '), E('strong', {}, '@BotFather'), '.' ]),
					E('li', {}, _('Create a bot and copy its bot token.')),
					E('li', {}, _('Paste the token below.'))
				]),
				field(_('Telegram Bot Token'), token,
					_('Leave this field blank to keep the currently configured token. The saved token is never shown on this page.'))
			]),
			step(2, _('Message the bot'), [
				E('p', { 'class': 'alert-message warning' }, [
					E('strong', {}, _('Telegram bots cannot start a private conversation. ')),
					_('Open your new bot and send it any fresh message immediately before detection.')
				]),
				E('p', {}, _('Then select Detect Chat IDs. If the token above has not been saved yet, it is used only for this detection request. A blank field uses the saved token.')),
				detect
			]),
			step(3, _('Select a detected recipient'), [
				E('p', {}, _('Only private chats from the currently available Telegram updates are shown. Groups and channels are excluded. Detection is not a permanent address book and updates may already have been consumed.')),
				candidates
			]),
			step(4, _('Confirm or edit the recipient'), [
				field(_('Destination Chat ID'), chat,
					_('A positive numeric Telegram private chat ID. A normal @username, URL, zero or negative value cannot be used.'))
			]),
			step(5, _('Optional SIM deletion'), [
				field(_('After successful delivery'), E('label', {}, [ remove, ' ',
					_('Remove messages from the SIM after successful Telegram delivery') ]),
					_('An SMS is removed only after Telegram returns HTTP 200 with JSON ok: true. A deletion failure retries deletion without sending the Telegram message again. When disabled, the SMS stays on the SIM and is not forwarded again.'))
			]),
			step(6, _('Apply configuration'), [
				E('p', {}, _('Select Save & Apply below. The service will use the new configuration on its next polling cycle. If the token or destination is missing, forwarding remains not configured.'))
			]),
			E('details', { 'style': 'margin-top:1em' }, [
				E('summary', {}, _('Technical details')),
				E('p', {}, _('SMS text is sent without parse_mode. Delivery and SIM deletion use persistent state and bounded retries. A network timeout can be ambiguous, so Telegram delivery may occasionally be duplicated, but a SIM message is never deleted before a confirmed Telegram response.'))
			])
		]);
		if (window.HH71) window.HH71.decorate(body);
		return body;
	}
});
