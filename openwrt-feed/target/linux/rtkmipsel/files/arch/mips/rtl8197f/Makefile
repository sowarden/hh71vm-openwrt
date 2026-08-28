# Realtek Semiconductor Corp.
#
# bsp/Makefile
#     bsp Makefile
#
# Copyright (C) 2006-2012 Tony Wu (tonywu@realtek.com)
#
# Edits for HH71VM (kernel 4.14):
#   * AC10 (kinkan/komikan) and generic boards have been removed from the list of machines,
#     added our mach-hh71vm.o;
#   * irq-ictl.c and irq-gpio.c are intentionally not listed here: they are connected
#     via #include itself irq.c (lines 49-55), and CONFIG_IRQ_ICTL/CONFIG_IRQ_GPIO
#     defines not Kconfig, but bspchip.h;
#   * mtd.o is assembled together with the controller driver spi-sheipa
#     (drivers/spi/sheipa/; the symbol CONFIG_SPI_SHEIPA starts the patch
#     203-rtl8197f-spi-sheipa.patch). The file registers the platform device
#     controller and spi_board_info for m25p80 - from this we get /proc/mtd,
#     /dev/mtdblockN and loading from flash.

obj-y := prom.o setup.o irq.o timer.o serial.o clk.o platform.o
obj-$(CONFIG_PCI) += pci-fixup.o pci-ops.o
obj-$(CONFIG_RTL_819X) += pci.o
obj-$(CONFIG_USB) += dev_usb.o
obj-$(CONFIG_SPI_SHEIPA) += mtd.o
obj-$(CONFIG_GPIOLIB) += gpio.o dev-gpio-buttons.o
obj-$(CONFIG_LEDS_GPIO) += dev_leds_gpio.o
obj-$(CONFIG_RTL819X_DW_SPI)  += spi.o
obj-$(CONFIG_I2C_DESIGNWARE_PLATFORM)  += i2c.o

#
# Machines
#
obj-$(CONFIG_RTL8197_MACH_HH71VM)	+= mach-hh71vm.o
