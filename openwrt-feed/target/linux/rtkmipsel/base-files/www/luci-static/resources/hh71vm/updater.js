'use strict';
'require dom';
'require fs';
'require ui';

function parseReply(reply) {
	if (!reply || reply.code !== 0)
		L.raise('Error', (reply && reply.stderr || _('Firmware updater failed.')).trim());

	try {
		return JSON.parse(reply.stdout);
	}
	catch (error) {
		L.raise('Error', _('Firmware updater returned invalid data.'));
	}
}

function formatDate(value) {
	if (!value)
		return _('release date unavailable');

	var date = new Date(value);
	return isNaN(date.getTime()) ? _('release date unavailable') : date.toLocaleString();
}

return {
	load: function() {
		return fs.exec('/usr/sbin/autosysupgrade', [ '--status-json' ])
			.then(parseReply)
			.catch(function(error) {
				return { error: error.message || String(error), checked: false };
			});
	},

	renderRelease: function(release) {
		var changes = Array.isArray(release.changes) ? release.changes : [];

		return E('div', { 'class': 'cbi-section' }, [
			E('h4', {}, [ release.tag, ' — ', formatDate(release.published_at) ]),
			changes.length
				? E('ul', {}, changes.map(function(change) { return E('li', {}, change); }))
				: E('p', {}, _('No signed changelog was published for this build.'))
		]);
	},

	updateView: function() {
		var state = this.state || {},
		    currentDate = formatDate(state.current_published_at),
		    latest = state.checked
				? E('p', {}, [ E('strong', {}, _('Latest available build:')), ' ', state.latest || _('unknown'), ' — ', formatDate(state.latest_published_at) ])
				: E('p', {}, _('Select “Check Updates” to query published releases.')),
		    result;

		if (state.error)
			result = E('p', { 'class': 'alert-message warning' }, state.error);
		else if (state.installed_newer)
			result = E('p', { 'class': 'alert-message warning' }, _('The installed build is newer than the latest public release. Automatic downgrade is disabled.'));
		else if (state.checked && state.update_available)
			result = E('p', { 'class': 'alert-message success' }, _('A newer signed firmware build is available.'));
		else if (state.checked)
			result = E('p', { 'class': 'alert-message success' }, _('The latest published firmware is already installed.'));
		else
			result = '';

		dom.content(this.summaryNode, [
			E('p', {}, [ E('strong', {}, _('Current firmware:')), ' ', state.current || _('unknown'), ' — ', currentDate ]),
			latest,
			result
		]);

		this.upgradeButton.disabled = !(state.checked && state.update_available && !state.installed_newer);

		var releases = Array.isArray(state.releases) ? state.releases.slice(0, 5) : [];
		dom.content(this.historyNode, state.checked ? [
			E('h4', {}, _('Recent release history')),
			E('p', {}, _('The newest release is included. Each signed list describes changes introduced by that build.')),
			state.history_complete ? '' : E('p', { 'class': 'alert-message warning' }, _('Some older release metadata could not be verified and was omitted.'))
		].concat(releases.map(this.renderRelease.bind(this))) : []);
	},

	handleCheck: function(ev) {
		var button = ev.currentTarget;
		button.disabled = true;
		button.firstChild.data = _('Checking…');

		return fs.exec('/usr/sbin/autosysupgrade', [ '--check-json' ])
			.then(parseReply)
			.then(L.bind(function(state) {
				this.state = state;
				this.updateView();
			}, this))
			.catch(L.bind(function(error) {
				this.state = this.state || {};
				this.state.error = error.message || String(error);
				this.state.checked = false;
				this.updateView();
			}, this))
			.finally(function() {
				button.disabled = false;
				button.firstChild.data = _('Check Updates');
			});
	},

	handleUpgrade: function() {
		if (!this.state || !this.state.checked || !this.state.update_available || this.state.installed_newer)
			return;

		ui.showModal(_('Upgrade Firmware?'), [
			E('p', {}, _('The router will download the exact build shown below, verify its signature, SHA-256 checksum, and platform compatibility, then install it while preserving settings.')),
			E('p', {}, [ E('strong', {}, this.state.latest) ]),
			E('p', { 'class': 'alert-message warning' }, _('Do not disconnect power during the upgrade. The router will be unavailable for approximately 5–7 minutes.')),
			E('div', { 'class': 'right' }, [
				E('button', { 'class': 'btn', 'click': ui.hideModal }, _('Cancel')), ' ',
				E('button', {
					'class': 'btn cbi-button-action important',
					'click': ui.createHandlerFn(this, 'handleUpgradeConfirm')
				}, _('Upgrade Firmware'))
			])
		]);
	},

	handleUpgradeConfirm: function() {
		var expected = this.state.latest;
		ui.showModal(_('Flashing…'), [
			E('p', { 'class': 'spinning' }, _('The signed image is being downloaded, verified, and installed. DO NOT POWER OFF THE DEVICE.'))
		]);

		/* Start reconnect polling only after sysupgrade terminates the RPC connection. */
		return fs.exec('/usr/sbin/autosysupgrade', [ '--yes', '--expected', expected ])
			.then(function(reply) {
				if (reply.code !== 0) {
					ui.hideModal();
					ui.addNotification(null, E('p', {}, (reply.stderr || _('Firmware upgrade failed.')).trim()));
					return;
				}
				ui.awaitReconnect(window.location.host);
			})
			.catch(function() {
				ui.awaitReconnect(window.location.host);
			});
	},

	render: function(state) {
		this.state = state || {};
		this.summaryNode = E('div');
		this.historyNode = E('div');
		this.upgradeButton = E('button', {
			'class': 'btn cbi-button-action important',
			'click': ui.createHandlerFn(this, 'handleUpgrade'),
			'disabled': true
		}, _('Upgrade Firmware'));

		var node = E('div', { 'class': 'cbi-section' }, [
			E('h3', {}, _('Firmware Updates & Upgrade')),
			E('p', {}, _('Check GitHub Releases for a newer HH71VM build. Only release descriptors signed by the firmware trust key are accepted.')),
			this.summaryNode,
			E('div', { 'class': 'cbi-page-actions' }, [
				E('button', {
					'class': 'btn cbi-button-action',
					'click': ui.createHandlerFn(this, 'handleCheck')
				}, _('Check Updates')), ' ',
				this.upgradeButton
			]),
			this.historyNode
		]);

		this.updateView();
		return node;
	}
};
