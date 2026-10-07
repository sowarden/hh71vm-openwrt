# Linux 4.14 provides nft_queue, but the pinned OpenWrt lacks its package.

define KernelPackage/nft-queue
  SUBMENU:=$(NF_MENU)
  TITLE:=Netfilter nf_tables queue support
  DEPENDS:=+kmod-nft-core +kmod-nfnetlink-queue
  KCONFIG:=CONFIG_NFT_QUEUE
  FILES:=$(LINUX_DIR)/net/netfilter/nft_queue.ko
  AUTOLOAD:=$(call AutoProbe,nft_queue)
endef

define KernelPackage/nft-queue/description
 Kernel module support for queueing packets with nftables.
endef

$(eval $(call KernelPackage,nft-queue))
