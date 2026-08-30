-- SPDX-License-Identifier: Apache-2.0

module("luci.controller.modem_extra_tools", package.seeall)

function index()
	local page = entry({"admin", "modem", "extra-tools"},
		view("modem-extra-tools/main-1-1-2"), _("Extra tools"), 80)
	page.dependent = true
	page.acl_depends = { "luci-app-modem-extra-tools" }
end
