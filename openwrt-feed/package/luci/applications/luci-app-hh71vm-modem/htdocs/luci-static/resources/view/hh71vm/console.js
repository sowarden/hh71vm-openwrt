'use strict';
'require view';
'require ui';
'require dom';
'require hh71vm.modem as m';

/* Raw AT console.
 *
 * Deliberately unfiltered: the point of the page is to see exactly what the modem says.
 * Commands run through the same queue as everything else, so nothing here can collide
 * with the background polling.
 */

var PRESETS = [
	['AT+CSQ',        _('Signal strength')],
	['AT+COPS?',      _('Current operator')],
	['AT+CEREG?',     _('LTE registration and cell')],
	['AT+CGDCONT?',   _('Defined profiles')],
	['AT+CGCONTRDP',  _('Address, gateway and DNS')],
	['AT+CPIN?',      _('SIM state')],
	['AT+CPMS?',      _('Message storage')],
	['AT+CCLK?',      _('Network time')],
	['ATI',           _('Modem identification')],
	['AT+CGATT?',     _('Packet service attached')],
	['AT$QCSYSMODE?', _('System mode')]
];

/* AT+CGMR, AT+CGMM, AT+CGMI and AT+CGSN get no answer from this firmware at all:
 * the modem takes the line and never produces a final result, so the command sits
 * there until the deadline and blocks everything queued behind it.  ATI returns the
 * same three facts -- manufacturer, model, revision -- at once, so that is what the
 * presets offer.  Typed by hand they still run, and now report the timeout instead
 * of printing a blank line. */
var NO_ANSWER = /^AT\+CG(MR|MM|MI|SN)/i;

/* AT+CLAC (list every supported command) never returns on this modem: it was left
 * running for 150 seconds without a final result, and the only thing it achieves is
 * blocking the command queue until the deadline.  It is not offered as a preset;
 * typing it by hand still works, and now fails cleanly instead of hanging the page. */
var NEVER_RETURNS = /^AT\+CLAC/i;

/* Commands that take a long time or interrupt the connection.  Not forbidden -- this is
 * a console -- but worth a word before they run. */
var SLOW = /^AT\+COPS=\?/i;
var DISRUPTIVE = /^AT(\+CFUN=0|\+CGACT=0|\$QCRMCALL|\+CMGD=.*,4)/i;

return view.extend({
	handleSave: null,
	handleSaveApply: null,
	handleReset: null,

	load: function () {
		return m.api.status();
	},

	render: function (st) {
		var out = E('div', { 'class': 'at-out' },
		            E('span', { 'class': 'at-hint' }, _('Output appears here.')));
		var input = E('input', {
			'type': 'text',
			'class': 'at-cmd',
			'placeholder': 'AT+CSQ',
			'autocomplete': 'off',
			'spellcheck': 'false',
			'aria-label': _('Command')
		});
		var history = [], histPos = -1;

		function append(nodes) {
			if (out.firstChild && out.firstChild.nodeName === 'SPAN'
			    && out.children.length === 1 && !out.dataset.used)
				dom.content(out, []);
			out.dataset.used = '1';
			nodes.forEach(function (n) { out.appendChild(n); });
			out.scrollTop = out.scrollHeight;
		}

		function run(cmdText) {
			var cmds = String(cmdText).split('\n')
				.map(function (s) { return s.trim(); })
				.filter(function (s) { return s.length > 0; });
			if (!cmds.length) return;

			for (var i = 0; i < cmds.length; i++) {
				if (NEVER_RETURNS.test(cmds[i]) && !confirm(
					_('%s does not answer on this modem -- it has been left running for two and a half minutes without a result, and it blocks every other command until it gives up. Run it anyway?').format(cmds[i]))) return;
				if (NO_ANSWER.test(cmds[i]) && !confirm(
					_('%s is not answered by this modem: it never returns a final result and blocks the command queue until the deadline. ATI reports the same identification instantly. Run it anyway?').format(cmds[i]))) return;
				if (SLOW.test(cmds[i]) && !confirm(
					_('%s scans every band and can take a minute and a half, during which \
mobile data is unavailable. Run it?').format(cmds[i]))) return;
				if (DISRUPTIVE.test(cmds[i]) && !confirm(
					_('%s changes the modem state and may drop the connection. Run it?')
						.format(cmds[i]))) return;
			}

			history.push(cmdText);
			histPos = history.length;

			append([E('div', {}, E('span', { 'class': 'cmd' }, '> ' + cmds.join(' ; ')))]);
			var busy = E('div', { 'class': 'spinning' }, _('waiting for the modem…'));
			append([busy]);
			var done = function () { if (busy.parentNode) busy.parentNode.removeChild(busy); };

			return m.api.at(cmds, 22).then(function (res) {
				done();
				res = res || {};
				/* A timeout comes back as { error: "timeout", results: [] }.  An empty
				   array is truthy, so the old guard fell through to the loop below,
				   which had nothing to print -- the command looked like it silently
				   did nothing. */
				if (res.error && !(res.results && res.results.length))
					return append([E('div', { 'class': 'err' },
					                 _('no answer from the modem: %s').format(String(res.error)))]);
				(res.results || []).forEach(function (r) {
					(r.lines || []).forEach(function (l) {
						append([E('div', {}, l)]);
					});
					var final = r.final || '';
					append([E('div', { 'class': final === 'OK' ? 'ok' : 'err' },
					          final || '(no final result)')]);
				});
				append([E('div', {}, ' ')]);
			}).catch(function (e) {
				done();
				append([E('div', { 'class': 'err' }, String(e.message || e))]);
			});
		}

		input.addEventListener('keydown', function (ev) {
			if (ev.key === 'Enter') {
				ev.preventDefault();
				var v = input.value;
				input.value = '';
				run(v);
			} else if (ev.key === 'ArrowUp' && history.length) {
				ev.preventDefault();
				histPos = Math.max(0, histPos - 1);
				input.value = history[histPos] || '';
			} else if (ev.key === 'ArrowDown' && history.length) {
				ev.preventDefault();
				histPos = Math.min(history.length, histPos + 1);
				input.value = history[histPos] || '';
			}
		});

		var presetButtons = PRESETS.map(function (p) {
			return E('button', {
				'class': 'cbi-button cbi-button-neutral',
				'type': 'button',
				'title': p[1],
				'click': function () { input.value = p[0]; run(p[0]); }
			}, p[0]);
		});

		/* One terminal: the log and the line you type on live in the same frame.
		   The command box used to be a form row at the top of the card with the
		   output all the way at the bottom, so reading a reply meant looking
		   somewhere other than where you had just typed. */
		var term = E('div', { 'class': 'at-term' }, [
			E('div', { 'class': 'at-bar' }, [
				E('span', { 'class': 'at-title' }, _('Modem console')),
				E('button', {
					'class': 'cbi-button', 'type': 'button',
					'click': function () { m.copyText(out.innerText || out.textContent || ''); }
				}, _('Copy output')),
				E('button', {
					'class': 'cbi-button', 'type': 'button',
					'click': function () {
						delete out.dataset.used;
						dom.content(out, E('span', { 'class': 'at-hint' },
						                   _('Output appears here.')));
					}
				}, _('Clear'))
			]),
			out,
			E('div', { 'class': 'at-line' }, [
				E('span', { 'class': 'at-prompt', 'aria-hidden': 'true' }, '>'),
				input,
				E('button', {
					'class': 'cbi-button cbi-button-action', 'type': 'button',
					'click': function () { var v = input.value; input.value = ''; run(v); }
				}, _('Run'))
			])
		]);

		/* Clicking the prompt row anywhere puts the caret in the field; clicking the
		   log above it does not, because selecting text out of the log is the other
		   thing people do on this page. */
		term.lastElementChild.addEventListener('click', function (ev) {
			if (ev.target.tagName !== 'BUTTON') input.focus();
		});

		var body = E('div', {}, [
			m.linkState(st) || E([]),

			E('div', { 'class': 'cbi-section' }, [
				E('h3', {}, _('AT console')),
				E('div', { 'class': 'cbi-section-descr' },
				  _('Commands go straight to the modem on the Qualcomm side. Answers are \
shown exactly as they come back, unfiltered.')),

				E('div', { 'class': 'alert-message warning' }, [
					E('p', {}, _('This is the real modem, not a simulator. Commands that \
write settings take effect immediately, and some of them can drop your connection or \
cost money. If you are not sure what a command does, do not send it.'))
				]),

				E('div', { 'class': 'at-presets' }, presetButtons),

				term,

				E('div', { 'class': 'cbi-value-description' },
				  _('Enter runs the line. Arrow up and down walk back through what you \
have already sent. Answers are printed exactly as the modem returns them, with the \
final result on its own line.'))
			])
		]);
		return body;
	}
});
