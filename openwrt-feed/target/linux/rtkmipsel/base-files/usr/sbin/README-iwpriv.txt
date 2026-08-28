iwpriv is shipped as a prebuilt binary, an explicit bring-up compromise.

Why prebuilt: rtl8192cd is configured through private ioctls rather than cfg80211,
and the required client is iwpriv from wireless_tools. The OpenWrt base tree does not
contain it; the package is in the `packages` feed, which was not enabled for this build.
Adding a feed for one 35-kilobyte utility during bring-up was considered disproportionate.

Provenance: compiled from the vendor wireless_tools.29 source, which is in the
extracted Realtek SDK (users/wireless_tools.29 in ax12-sdk-v3.6.0), with the same toolchain
used to build the image.

Rebuild it from that source tree with the matching OpenWrt toolchain.

The preferred long-term solution is to package this utility for OpenWrt or enable
the `packages` feed and select wireless-tools.
