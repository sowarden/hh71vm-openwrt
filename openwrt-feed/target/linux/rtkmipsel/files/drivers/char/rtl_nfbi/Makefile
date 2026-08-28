# NFBI driver (MDIO access to an external PHY through the switch core).
#
# Source - vendor kernel 3.10.90 from GPL archive DIR-842E,
# kernel/drivers/char/rtl_nfbi/. The extracted AX12 SDK v3.6.0 does not have a nuclear version:
# there rtl_nfbi.c is only in bootcode/, that is, in the bootloader sources.
#
# Why: rtl_mdio_read()/rtl_mdio_write()/nfbi_init() are needed by the branch
# CONFIG_RTL_8211F_SUPPORT driver rtl819x - through them it goes to the external
# gigabit PHY at MDIO address 6 (PORT0_RGMII_PHYID).
#
# The vendor's Makefile contained an unconditional `obj-y += rtl_nfbi.o`; we have assembly
# tied to the external PHY symbol so that the file does not end up in the image unnecessarily.
obj-$(CONFIG_RTL_8211F_SUPPORT) += rtl_nfbi.o
