-- luci-app-hh71vm-xray: the menu entry for the Xray page.
-- Licensed to the public under the Apache License 2.0.
--
-- One client-side view; every answer comes from the hh71vm-xray ubus object
-- (/usr/libexec/rpcd/hh71vm-xray), so there is nothing to do server side beyond
-- putting the entry in the tree.

module("luci.controller.hh71vm_xray", package.seeall)

function index()
	entry({"admin", "services", "hh71vm-xray"}, view("hh71vm-xray/main"),
	      _("VPN (Xray)"), 20).acl_depends = { "luci-app-hh71vm-xray" }
end
