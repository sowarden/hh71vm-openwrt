/*
 * Realtek Semiconductor Corp.
 *
 * bsp/prom.c
 *     bsp early initialization code
 *
 * Copyright (C) 2006-2012 Tony Wu (tonywu@realtek.com)
 *
 * Build two versions:
 * - the basis is vendor one from Realtek SDK v3.6.0 (core 4.4). Taken from it
 * determining the 8197F/8197F_VG chip variant for an early console and parsing it
 * RAM volume by register bond option;
 * - command line parsing - in the style of OpenWrt (arguments from the loader
 * lzma-loader), as in the fork openwrt-AC10.
 *
 * Why the variant definition is important: 8197F and 8197F_VG have receive and
 * UART0 transfers are at different offsets (+0x024 vs +0x000). Vendor
 * the code selects them at runtime, version AC10 is hardcoded for non-VG. Error here
 * doesn't show up as a build failure - it shows up as completely silent
 * the console on the first boot, that is, exactly at the moment when the console
 * is the only source of diagnosis.
 */
#include <linux/version.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>

#include <asm/bootinfo.h>
#include <asm/addrspace.h>

#include "bspcpu.h"
#include "bspchip.h"

/*
 * The default values ​​are for non-VG. Specified in bsp_serial_init() before the first
 * symbol output.
 */
unsigned int bsp_uart0_rbr = BSP_UART0_RBR_8197F;
unsigned int bsp_uart0_thr = BSP_UART0_THR_8197F;
EXPORT_SYMBOL(bsp_uart0_rbr);
EXPORT_SYMBOL(bsp_uart0_thr);

extern char arcs_cmdline[];

#ifdef CONFIG_EARLY_PRINTK
static int promcons_output __initdata = 0;

/*
 * __init was added here by us, the vendor did not have it. Without it, modpost gives
 * section mismatch: a normal function touches a variable from .init.data, that is
 * after freeing init memory, it would access the freed page.
 *
 * The mark is safe: there is no one to call this pair in 4.14. Hook disable_early_printk()
 * there is no 4.14 in the kernel at all (not a single mention of arch/mips and include),
 * and unregister_prom_console() is not called either in BSP or in vendor sources,
 * nor in the AC10 fork. This is an inheritance from kernels where such a hook still existed.
 */
void __init unregister_prom_console(void)
{
	if (promcons_output)
		promcons_output = 0;
}

void __init disable_early_printk(void)
    __attribute__ ((alias("unregister_prom_console")));

void prom_putchar(char c)
{
	unsigned int busy_cnt = 0;

	do
	{
		/* Prevent Hanging */
		if (busy_cnt++ >= 30000)
		{
			/* Reset Tx FIFO */
			REG8(BSP_UART0_FCR) = BSP_TXRST | BSP_CHAR_TRIGGER_14;
			return;
		}
	} while ((REG8(BSP_UART0_LSR) & BSP_LSR_THRE) == BSP_TxCHAR_AVAIL);

	/* Send Character */
	REG8(BSP_UART0_THR) = c;
	return;
}

static int bsp_serial_init(void)
{
	if (IS_8197F_VG()) {
		bsp_uart0_rbr = BSP_UART0_RBR_8197F_VG;
		bsp_uart0_thr = BSP_UART0_THR_8197F_VG;
	}

	REG32(BSP_UART0_IER) = 0;

	REG32(BSP_UART0_LCR) = BSP_LCR_DLAB;
	REG32(BSP_UART0_DLL) = BSP_UART0_BAUD_DIVISOR & 0x00ff;
	REG32(BSP_UART0_DLM) = (BSP_UART0_BAUD_DIVISOR & 0xff00) >> 8;
	REG32(BSP_UART0_LCR) = BSP_CHAR_LEN_8;
	return 0;
}
#else
static int bsp_serial_init(void)
{
	return 0;
}
#endif

const char *get_system_type(void)
{
	return "RTL8197F";
}

void __init prom_free_prom_memory(void)
{
}

/*
 * The lzma-loader loader passes the command line as argc/argv to fw_arg0/1.
 * If you didn’t transmit anything, we leave a line that is enough to see
 * loading: console at the same speed at which the drain is running.
 */
static __init void prom_init_cmdline(int argc, char **argv)
{
	int i;

	if (argc > 0 && argv) {
		for (i = 0; i < argc; i++) {
			if (!argv[i])
				continue;
			strlcat(arcs_cmdline, " ", sizeof(arcs_cmdline));
			strlcat(arcs_cmdline, argv[i], sizeof(arcs_cmdline));
		}
	} else {
		strcpy(arcs_cmdline, "console=ttyS0,38400");
	}
}

/* The amount of RAM is from the bond option register, as the vendor bootloader does */
static __init u_long prom_detect_memsize(void)
{
	switch (REG32(BSP_BOND_OPTION) & 0x0F) {
	case 0x06:
	case 0x0C:
		return 32 << 20;
	case 0x04:
	case 0x0A:
		return 64 << 20;
	case 0x05:
	case 0x0B:
		return 128 << 20;
	default:
		return REG32(0xB8000F00) << 20;
	}
}

/* Do basic initialization */
void __init prom_init(void)
{
	u_long mem_size;

	bsp_serial_init();
	prom_init_cmdline(fw_arg0, (char **)fw_arg1);

	mem_size = prom_detect_memsize();
	add_memory_region(0, mem_size, BOOT_MEM_RAM);

#ifndef CONFIG_RTL_819X_SWCORE
	/*
	 * The switch core and router mode includes the vendor Ethernet driver.
	 * While it is not in the assembly, we turn off the clock, as the vendor’s BSP does.
	 */
#define SYS_CLK_MAG		(0xB8000000 + 0x0010)
#define CM_ACTIVE_SWCORE	(1 << 11)
#define EPHY_CONTROL		(0xB8000000 + 0x01E0)
#define EN_ROUTER_MODE		(1 << 12)
	REG32(SYS_CLK_MAG) &= ~CM_ACTIVE_SWCORE;
	REG32(EPHY_CONTROL) &= ~EN_ROUTER_MODE;
#endif
}
