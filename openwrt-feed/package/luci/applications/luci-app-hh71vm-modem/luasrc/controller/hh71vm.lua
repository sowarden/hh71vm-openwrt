-- luci-app-hh71vm-modem: menu entries for the modem pages.
-- Licensed to the public under the Apache License 2.0.
--
-- Every page is a client-side view; all data comes from the hh71vm-modem ubus object
-- (see /usr/libexec/rpcd/hh71vm-modem), so there is nothing to do server side beyond
-- putting the entries in the tree.

module("luci.controller.hh71vm", package.seeall)

function index()
	local page = entry({"admin", "modem"}, firstchild(), _("Modem"), 25)
	page.dependent = false
	page.acl_depends = { "luci-app-hh71vm-modem" }

	entry({"admin", "modem", "overview"},  view("hh71vm/overview"),  _("Overview"),    10)
	entry({"admin", "modem", "sms"},       view("hh71vm/sms"),       _("Messages"),    20)
	entry({"admin", "modem", "network"},   view("hh71vm/network"),   _("Network"),     30)
	entry({"admin", "modem", "apn"},       view("hh71vm/apn"),       _("Profiles"),    40)
	entry({"admin", "modem", "sim"},       view("hh71vm/sim"),       _("SIM and PIN"), 50)
	entry({"admin", "modem", "phonebook"}, view("hh71vm/phonebook"), _("Phonebook"),   60)
	entry({"admin", "modem", "console"},   view("hh71vm/console"),   _("AT console"),  70)

	-- linked from the footer of every page
	entry({"admin", "system", "hh71vm-about"}, view("hh71vm/about"),
	      _("About this port"), 90)
end
