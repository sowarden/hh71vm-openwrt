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

function check(result) {
	if (!result || result.ok !== true) throw new Error(result && result.error || 'invalid_response');
	return result;
}

function message(code) {
	var messages = {
		invalid_token: _('The Telegram bot token is invalid.'),
		invalid_chat_id: _('Send to User must contain a positive numeric private chat ID.'),
		no_private_chat: _('No private chat was found. Send any message to your bot, then try again.'),
		multiple_private_chats: _('More than one private chat was found. Enter the intended numeric chat ID manually.'),
		telegram_rate_limited: _('Telegram rate-limited this request. Wait and try again.'),
		telegram_http_error: _('Telegram returned an HTTP error.'),
		telegram_api_error: _('Telegram rejected the request.'),
		telegram_transport_failed: _('The HTTPS request to Telegram failed.'),
		telegram_invalid_response: _('Telegram returned an invalid response.'),
		config_write_failed: _('The configuration could not be saved.')
	};
	return messages[code] || _('The operation failed safely.');
}

function notify(error) {
	ui.addNotification(null, E('p', {}, message(String(error.message || error.error || error))), 'error');
}

function row(label, input, description) {
	return E('div', { 'class': 'cbi-value' }, [
		E('div', { 'class': 'cbi-value-title' }, label),
		E('div', { 'class': 'cbi-value-field' }, [ input,
			E('div', { 'class': 'cbi-value-description' }, description || '') ])
	]);
}

return view.extend({
	handleSave: null, handleSaveApply: null, handleReset: null,
	load: function() { return Promise.all([ statusCall(), configGet() ]); },
	render: function(data) {
		var body = E('div'), state = check(data[0]), config = check(data[1]), busy = false;
		var token = E('input', { 'type': 'password', 'autocomplete': 'new-password', 'value': '',
			'placeholder': config.token_set ? _('Configured - leave blank to keep') : _('Telegram bot token'),
			'style': 'width:32em;max-width:100%', 'aria-label': _('Telegram Bot Token') });
		var chat = E('input', { 'type': 'text', 'inputmode': 'numeric', 'pattern': '[0-9]+',
			'maxlength': 20, 'value': config.chat_id || '', 'style': 'width:22em;max-width:100%',
			'aria-label': _('Send to User') });
		var remove = E('input', { 'type': 'checkbox', 'checked': config.remove_after_send ? 'checked' : null });

		function setBusy(value) {
			busy = value;
			Array.prototype.forEach.call(body.querySelectorAll('button'), function(button) { button.disabled = value; });
		}
		function validate() {
			if (token.value && !/^[1-9][0-9]+:[A-Za-z0-9_-]+$/.test(token.value)) throw new Error('invalid_token');
			if (!/^[1-9][0-9]{4,19}$/.test(chat.value)) throw new Error('invalid_chat_id');
		}
		function drawStatus(value) {
			state = value;
			var text = state.configured ? _('Configured') : _('Not configured');
			text += ' | ' + (state.running ? _('Running') : _('Stopped'));
			text += ' | ' + _('Pending: %d').format(state.pending || 0);
			text += ' | ' + _('Pending SIM deletion: %d').format(state.pending_delete || 0);
			return text;
		}

		var statusLine = E('p', {}, drawStatus(state));
		var content = [
			E('h2', {}, _('SMS to Telegram')),
			E('p', { 'class': 'alert-message warning' }, [
				E('strong', {}, _('Telegram bots cannot start a private conversation. ')),
				_('You must send any message to your bot at least once. Only after that can the bot send messages to you.')
			]),
			E('p', {}, _('Send to User requires the positive numeric private chat_id, not an @username. After messaging the bot, use Detect chat ID. Detection succeeds only when exactly one private chat is present; otherwise it fails instead of choosing a recipient ambiguously.')),
			statusLine,
			row(_('Telegram Bot Token'), token, _('The token is stored with restricted permissions. Leave this field blank when saving to keep the existing token. It is never returned by the status API.')),
			row(_('Send to User'), chat, _('Positive numeric private chat_id. A normal Telegram @username is not accepted.')),
			row(_('Delete after delivery'), E('label', {}, [ remove, ' ',
				_('Remove Messages from SIM after sending to Telegram') ]),
				_('Deletion is attempted only after Telegram returns HTTP 200 with JSON ok: true. Failed deletion is retried without sending the SMS again.')),
			E('p', { 'class': 'cbi-value-description' }, _('SMS text is sent without parse_mode, so Telegram does not interpret it as Markdown or HTML. A timeout can be ambiguous: the service retries delivery but never deletes the SIM message until a confirmed response is observed.')),
			E('div', { 'class': 'cbi-page-actions' }, [
				E('button', { 'class': 'cbi-button', 'click': function() {
					if (busy) return;
					setBusy(true);
					discoverChat(token.value).then(check).then(function(result) {
						chat.value = result.chat_id;
						ui.addNotification(null, E('p', {}, _('A single private chat ID was detected. Review it and save the configuration.')), 'info');
					}).catch(notify).then(function() { setBusy(false); });
				} }, _('Detect chat ID')),
				E('button', { 'class': 'cbi-button cbi-button-action', 'click': function() {
					if (busy) return;
					try { validate(); } catch (error) { notify(error); return; }
					setBusy(true);
					configSet(token.value, chat.value, remove.checked).then(check).then(function() {
						token.value = '';
						ui.addNotification(null, E('p', {}, _('SMS to Telegram settings saved.')), 'info');
						return Promise.all([ statusCall(), configGet() ]);
					}).then(function(values) {
						state = check(values[0]); config = check(values[1]);
						token.placeholder = config.token_set ? _('Configured - leave blank to keep') : _('Telegram bot token');
						dom.content(statusLine, drawStatus(state));
					}).catch(notify).then(function() { setBusy(false); });
				} }, _('Save'))
			])
		];
		dom.content(body, content);
		if (window.HH71) window.HH71.decorate(body);
		return body;
	}
});
