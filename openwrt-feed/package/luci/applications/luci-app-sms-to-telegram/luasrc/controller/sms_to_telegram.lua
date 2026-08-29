-- SPDX-License-Identifier: Apache-2.0

module("luci.controller.sms_to_telegram", package.seeall)

function index()
	local page = entry({"admin", "modem", "sms-to-telegram"},
		view("sms-to-telegram/main-1-0-0"), _("SMS to Telegram"), 25)
	page.dependent = true
	page.acl_depends = { "luci-app-sms-to-telegram" }
end
