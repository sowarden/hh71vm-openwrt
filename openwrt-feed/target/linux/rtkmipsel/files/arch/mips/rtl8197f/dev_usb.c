/*
 *  Realte rtl819x USB host device support
 *
 *  Copyright (C) 2008-2009 Gabor Juhos <juhosg@openwrt.org>
 *  Copyright (C) 2008 Imre Kaloz <kaloz@openwrt.org>
 *
 *  Parts of this file are based on Atheros' 2.6.15 BSP
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 */
/*
 * HH71VM: turn the vendor USB diagnostics back on.
 *
 * The AC10 fork replaced every printk here with pr_debug, that is, silenced the output. Stock
 * does print these lines and we have its UART log - so with DEBUG on our output can be compared
 * against the stock one line by line: PHY register values (`port N org 0xe...`), the patch branch
 * taken (40 MHz against 25 MHz), the system registers (`system reg b8000180=...` and its neighbours).
 *
 * That is cheaper than any guessing: a divergence shows up at once, and a match clears the BSP
 * of suspicion entirely.
 */
#define DEBUG 1

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>

#include "bspchip.h"
#include "dev_usb.h"

#define CONFIG_USB_ARCH_HAS_OHCI 1 //mark_cc
#define CONFIG_USB_ARCH_HAS_EHCI 1 //mark_cc
/*
 * OHCI (USB full speed host controller)
 */
#ifdef CONFIG_USB_ARCH_HAS_OHCI
static struct resource rtl819x_ohci_resources[] = {
	[0] = {
		.start	= PADDR(OHCI_RTL819x_USB_BASE),
		.end	= PADDR(OHCI_RTL819x_USB_BASE) + OHCI_RTL819x_USB_REG_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= RTL819x_USB_IRQ,
		.end	= RTL819x_USB_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static u64 rtl819x_ohci_dmamask = RTL819x_DMA_MASK;
static struct platform_device rtl819x_ohci_device = {
	/* HH71VM: this used to be "rtl819x-ohci" - see the note at the EHCI device below. */
	.name			= "ohci-platform",
	.id				= -1,
	.resource		= rtl819x_ohci_resources,
	.num_resources	= ARRAY_SIZE(rtl819x_ohci_resources),
	.dev = {
		.dma_mask			= &rtl819x_ohci_dmamask,
		.coherent_dma_mask	= RTL819x_DMA_MASK,
	},
};
#endif

/*
 * EHCI (USB high/full speed host controller)
 */
#ifdef CONFIG_USB_ARCH_HAS_EHCI
static struct resource rtl819x_ehci_resources[] = {
	[0] = {
		.start	= PADDR(EHCI_RTL819x_USB_BASE),
		.end	= PADDR(EHCI_RTL819x_USB_BASE) + EHCI_RTL819x_USB_REG_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= RTL819x_USB_IRQ,
		.end	= RTL819x_USB_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static u64 rtl819x_ehci_dmamask = RTL819x_DMA_MASK;

/*
 * HH71VM: struct rtl819x_ehci_platform_data and the variable rtl819x_ehci_data were removed -
 * they were only needed by the vendor shim, and would now be left unused
 * (a -Wunused-variable warning). The only field was `u8 is_rtl819x`.
 */

/*
 * HH71VM: the devices were renamed to use the STOCK kernel drivers
 * ehci-platform / ohci-platform instead of the vendor shim.
 *
 * WHY. The vendor names "rtl819x-ehci"/"rtl819x-ohci" require the vendor drivers
 * (`drivers/usb/host/ehci-rtl819x.c`), which only the AC10 fork has and which are written for
 * kernel 3.18. Porting them to 4.14 runs into four places: `IRQF_DISABLED` is gone from the
 * kernel, `ehci_port_power()` changed its signature to a per-port one, the `.hub_suspend`/`.hub_resume`
 * fields of `struct hc_driver` are called `.bus_suspend`/`.bus_resume`, and a hand-written
 * callback table risks diverging from 4.14 in something else as well.
 *
 * Meanwhile the shim itself does NOTHING SoC-specific: `synopsys_usb_setup()` is declared in it
 * and never called, the big_endian flags are ignored on our mipsel (the `ehci_big_endian_*`
 * macros give 0 without CONFIG_USB_EHCI_BIG_ENDIAN_MMIO), and everything genuinely
 * platform-specific - enabling the PHY, the clocks and the choice of ports - lives right here,
 * in `rtl819x_usb_setup()` above, and does not depend on the choice of driver.
 *
 * The stock ehci-platform/ohci-platform in 4.14 bind by name, need neither a device tree nor
 * clocks (every `devm_clk_get`/`reset_control_get` is optional), and fall back to sensible
 * defaults when there is no `platform_data`.
 *
 * `platform_data` has to be NULL. The previous value pointed at a
 * `struct rtl819x_ehci_platform_data` (one field, `is_rtl819x`), and `ehci_platform_probe()`
 * reads a non-NULL pointer as a `struct usb_ehci_pdata` - that is, it would read somebody
 * else's structure as its own.
 */
static struct platform_device rtl819x_ehci_device = {
	.name			= "ehci-platform",
	.id				= -1,
	.resource		= rtl819x_ehci_resources,
	.num_resources	= ARRAY_SIZE(rtl819x_ehci_resources),
	.dev = {
		.dma_mask			= &rtl819x_ehci_dmamask,
		.coherent_dma_mask	= RTL819x_DMA_MASK,
		.platform_data		= NULL,
	},
};
#endif

/*-------------------------------------------------------------------------------------*/
void SetUSBPhy(unsigned char reg, unsigned char val,unsigned char portnum)
{
	unsigned char reg_h;
	unsigned char reg_l;

#if 0
	printk("set usb phy\n");
	#define	USB2_PHY_DELAY	{mdelay(5);}
	//8196C demo board: 0xE0:99, 0xE1:A8, 0xE2:98, 0xE3:C1,  0xE5:91,
#if !CONFIG_RTL_819XD //8198
//	REG32(0xb8000034) = (0x1f00 | val); USB2_PHY_DELAY;
#else  //8196D
	printk("8196 only set usb phy\n");
	//#define SYS_USB_SIE 0xb8000034
	//#define SYS_USB_PHY 0xb8000090
	#define SYS_USB_PHY 0xb800016c
	//vicadd int oneportsel=(REG32(SYS_USB_SIE) & (1<<18))>>18;

//	unsigned int tmp = REG32(SYS_USB_PHY);  //8672 only
//	tmp = tmp & ~((0xff<<11)|(0xff<<0));


	//if(oneportsel==0)
	//{
	//REG32(SYS_USB_PHY) = (val << 0) | tmp;   //phy 0
	//}
	//else
	//{	REG32(SYS_USB_PHY) = (val << 11) | tmp;  //phy1
	//}

	//USB2_PHY_DELAY;
#endif
	//printk("0xb8000034=%08x\n", REG32(0xb8000034));

	unsigned char reg_h=(reg &0xf0)>>4;
	unsigned char reg_l=(reg &0x0f);

	printk("regh=%x regl=%x\n", reg_h,reg_l);

	REG32(0xb814020c)=val;// vicadd
	mdelay(100);
	//REG32(0xb80210A4) = (0x00300000 | (reg_l<<16)); USB2_PHY_DELAY;
	REG32(0xb80210A4) = (0x00003000 | (reg_l<<8)); USB2_PHY_DELAY;
	//REG32(0xb80210A4) = (0x00200000 | (reg_l<<16)); USB2_PHY_DELAY;
	REG32(0xb80210A4) = (0x00002000 | (reg_l<<8)); USB2_PHY_DELAY;

	//REG32(0xb80210A4) = (0x00300000 | (reg_l<<16)); USB2_PHY_DELAY;
	REG32(0xb80210A4) = (0x00003000 | (reg_l<<8)); USB2_PHY_DELAY;

	//REG32(0xb80210A4) = (0x00300000 | (reg_h<<16)); USB2_PHY_DELAY;
	REG32(0xb80210A4) = (0x00003000 | (reg_h<<8)); USB2_PHY_DELAY;

	//REG32(0xb80210A4) = (0x00200000 | (reg_h<<16)); USB2_PHY_DELAY;
	REG32(0xb80210A4) = (0x00002000 | (reg_h<<8)); USB2_PHY_DELAY;

	//REG32(0xb80210A4) = (0x00300000 | (reg_h<<16)); USB2_PHY_DELAY;
	REG32(0xb80210A4) = (0x00003000 | (reg_h<<8)); USB2_PHY_DELAY;



#endif
#define	USB2_PHY_DELAY	{mdelay(10);}

	//for 8196F port0 set usb phy
	//printk("set usb phy\n");
	if (portnum==0)
		REG32(0xb814020c) =0x00340000|val;
	else
		REG32(0xb814020c) =0x00340000|(val<<8);//port 1

	//printk("0xb814020c=%x\n", REG32(0xb814020c));

	 reg_h=(reg &0xf0)>>4;
	 reg_l=(reg &0x0f);
	if (portnum==0) {
		REG32(0xb80210A4) = (0x00003000 | (reg_l<<8)); USB2_PHY_DELAY;
		//printk("(0x003000 | (reg_l<<8))=%x\n", (0x003000 | (reg_l<<8)) );
		REG32(0xb80210A4) = (0x00002000 | (reg_l<<8)); USB2_PHY_DELAY;
		//printk("(0x002000 | (reg_l<<8))=%x\n", (0x002000 | (reg_l<<8)) );
		REG32(0xb80210A4) = (0x00003000 | (reg_l<<8)); USB2_PHY_DELAY;
		//printk("(0x003000 | (reg_l<<8))=%x\n", (0x003000 | (reg_l<<8)) );
		REG32(0xb80210A4) = (0x00003000 | (reg_h<<8)); USB2_PHY_DELAY;
		//printk("(0x003000 | (reg_h<<8))=%x\n", (0x003000 | (reg_h<<8)) );
		REG32(0xb80210A4) = (0x00002000 | (reg_h<<8)); USB2_PHY_DELAY;
		//printk("(0x003000 | (reg_h<<8))=%x\n", (0x002000 | (reg_h<<8)) );
		REG32(0xb80210A4) = (0x00003000 | (reg_h<<8)); USB2_PHY_DELAY;
		//printk("(0x003000 | (reg_h<<8))=%x\n", (0x003000 | (reg_h<<8)) );
	}
	else {   //port 1
		REG32(0xb80210A4) = (0x00005000 | (reg_l<<8)); USB2_PHY_DELAY;
		//printk("(0x003000 | (reg_l<<8))=%x\n", (0x003000 | (reg_l<<8)) );
		REG32(0xb80210A4) = (0x00004000 | (reg_l<<8)); USB2_PHY_DELAY;
		//printk("(0x002000 | (reg_l<<8))=%x\n", (0x002000 | (reg_l<<8)) );
		REG32(0xb80210A4) = (0x00005000 | (reg_l<<8)); USB2_PHY_DELAY;
		//printk("(0x003000 | (reg_l<<8))=%x\n", (0x003000 | (reg_l<<8)) );
		REG32(0xb80210A4) = (0x00005000 | (reg_h<<8)); USB2_PHY_DELAY;
		//printk("(0x003000 | (reg_h<<8))=%x\n", (0x003000 | (reg_h<<8)) );
		REG32(0xb80210A4) = (0x00004000 | (reg_h<<8)); USB2_PHY_DELAY;
		//printk("(0x003000 | (reg_h<<8))=%x\n", (0x002000 | (reg_h<<8)) );
		REG32(0xb80210A4) = (0x00005000 | (reg_h<<8)); USB2_PHY_DELAY;
	}
}

unsigned char GetUSBPhy(unsigned char reg)
{
	#define	USB2_PHY_DELAY	{mdelay(10);}
	unsigned char val;
	unsigned char reg_h=((reg &0xf0)>>4)-2;
	unsigned char reg_l=(reg &0x0f);

	//printk("GetUSBPHY\n");
	//REG32(0xb80210A4) = (0x00300000 | (reg_l<<16)); USB2_PHY_DELAY;
	//REG32(0xb80210A4) = (0x00200000 | (reg_l<<16)); USB2_PHY_DELAY;
	//REG32(0xb80210A4) = (0x00300000 | (reg_l<<16)); USB2_PHY_DELAY;
	//REG32(0xb80210A4) = (0x00300000 | (reg_h<<16)); USB2_PHY_DELAY;
	//REG32(0xb80210A4) = (0x00200000 | (reg_h<<16)); USB2_PHY_DELAY;
	//REG32(0xb80210A4) = (0x00300000 | (reg_h<<16)); USB2_PHY_DELAY;

	REG32(0xb80210A4) = (0x00003000 | (reg_l<<8)); USB2_PHY_DELAY;
	//printk("(0x003000 | (reg_l<<8))=%x\n", (0x003000 | (reg_l<<8)) );
	REG32(0xb80210A4) = (0x00002000 | (reg_l<<8)); USB2_PHY_DELAY;
	//printk("(0x002000 | (reg_l<<8))=%x\n", (0x002000 | (reg_l<<8)) );
	REG32(0xb80210A4) = (0x00003000 | (reg_l<<8)); USB2_PHY_DELAY;
	//printk("(0x003000 | (reg_l<<8))=%x\n", (0x003000 | (reg_l<<8)) );
	REG32(0xb80210A4) = (0x00003000 | (reg_h<<8)); USB2_PHY_DELAY;
	//printk("(0x003000 | (reg_h<<8))=%x\n", (0x003000 | (reg_h<<8)) );
	REG32(0xb80210A4) = (0x00002000 | (reg_h<<8)); USB2_PHY_DELAY;
	//printk("(0x003000 | (reg_h<<8))=%x\n", (0x002000 | (reg_h<<8)) );
	REG32(0xb80210A4) = (0x00003000 | (reg_h<<8)); USB2_PHY_DELAY;
	//printk("(0x003000 | (reg_h<<8))=%x\n", (0x003000 | (reg_h<<8)) );

	//unsigned char val;
	val=REG32(0xb80210A4);USB2_PHY_DELAY;
	//printk("Get USB PHY 0xB80210a4 l=%x\n",REG32(0xb80210A4) );
	return val;

}

unsigned char GetUSBPhy_1(unsigned char reg)
{
	//#define	USB2_PHY_DELAY	{mdelay(5);}
	unsigned char val;
	unsigned char reg_h=((reg &0xf0)>>4)-2;
	unsigned char reg_l=(reg &0x0f);

	//REG32(0xb80210A4) = (0x00300000 | (reg_l<<16)); USB2_PHY_DELAY;
	//REG32(0xb80210A4) = (0x00200000 | (reg_l<<16)); USB2_PHY_DELAY;
	//REG32(0xb80210A4) = (0x00300000 | (reg_l<<16)); USB2_PHY_DELAY;
	//REG32(0xb80210A4) = (0x00300000 | (reg_h<<16)); USB2_PHY_DELAY;
	//REG32(0xb80210A4) = (0x00200000 | (reg_h<<16)); USB2_PHY_DELAY;
	//REG32(0xb80210A4) = (0x00300000 | (reg_h<<16)); USB2_PHY_DELAY;
	//printk("Begin to read %x\n",reg);
	//REG32(0xb80210A4) = (0x00003000 | (reg_l<<8)); USB2_PHY_DELAY;
	REG32(0xb80210A4) = (0x00005000 | (reg_l<<8)); USB2_PHY_DELAY;
	//printk("0xb80210a4=%x\n", REG32(0xb80210a4));
	//REG32(0xb80210A4) = (0x00002000 | (reg_l<<8)); USB2_PHY_DELAY;
	REG32(0xb80210A4) = (0x00004000 | (reg_l<<8)); USB2_PHY_DELAY;
	//printk("0xb80210a4=%x\n", REG32(0xb80210a4));
       //REG32(0xb80210A4) = (0x00003000 | (reg_l<<8)); USB2_PHY_DELAY;
	REG32(0xb80210A4) = (0x00005000 | (reg_l<<8)); USB2_PHY_DELAY;
	//printk("0xb80210a4=%x\n", REG32(0xb80210a4));
	//REG32(0xb80210A4) = (0x00003000 | (reg_h<<8)); USB2_PHY_DELAY;
	REG32(0xb80210A4) = (0x00005000 | (reg_h<<8)); USB2_PHY_DELAY;
	//printk("0xb80210a4=%x\n", REG32(0xb80210a4));
	//REG32(0xb80210A4) = (0x00002000 | (reg_h<<8)); USB2_PHY_DELAY;
	REG32(0xb80210A4) = (0x00004000 | (reg_h<<8)); USB2_PHY_DELAY;
	//printk("0xb80210a4=%x\n", REG32(0xb80210a4));
	//REG32(0xb80210A4) = (0x00003000 | (reg_h<<8)); USB2_PHY_DELAY;
	REG32(0xb80210A4) = (0x00005000 | (reg_h<<8)); USB2_PHY_DELAY;
	//printk("0xb80210a4=%x\n", REG32(0xb80210a4));

	//USB2_PHY_DELAY;
	//unsigned char val;
	val=REG32(0xb80210A4);USB2_PHY_DELAY;
	//printk("Get USB PHY reg=%x val=%x\n",reg, val);
	//printk("return 0xb80210a4=%x \n\n", REG32(0xb80210A4) );
	//printk("end of read\n");
	return val;
}

#if 0
static void synopsys_usb_patch(void)
{
	//printk("synopsys_usb_patch\n");

//#ifndef REG32(reg)
//#define REG32(reg)	(*(volatile unsigned int *)(reg))
//#endif

//#define	USB2_PHY_DELAY	{int i=100; while(i>0) {i--;}}
	/* Patch: for USB 2.0 PHY */
#if !defined(CONFIG_RTL_8196C)
#if 0
	/* For Port-0 */
	writel(0x0000000E,0xb8003314) ;	USB2_PHY_DELAY;
	writel(0x00340000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x00240000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x00340000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x003E0000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x002E0000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x003E0000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x000000D8,0xb8003314) ;	USB2_PHY_DELAY;
	writel(0x00360000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x00260000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x00360000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x003E0000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x002E0000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x003E0000,0xb80210A4) ;	USB2_PHY_DELAY;

	/* For Port-1 */
	writel(0x000E0000,0xb8003314) ;	USB2_PHY_DELAY;
	writel(0x00540000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x00440000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x00540000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x005E0000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x004E0000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x005E0000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x00D80000,0xb8003314) ;	USB2_PHY_DELAY;
	writel(0x00560000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x00460000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x00560000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x005E0000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x004E0000,0xb80210A4) ;	USB2_PHY_DELAY;
	writel(0x005E0000,0xb80210A4) ;	USB2_PHY_DELAY;
#else
	/* For Port-0 */
	printk("synopsys_usb_patch\n");
	//REG32(0xb8003314) = 0x0000000E;	USB2_PHY_DELAY;
	REG32(0xb814020c) = 0x0000000E;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x00340000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x00240000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x00340000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x003E0000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x002E0000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x003E0000;	USB2_PHY_DELAY;
	//REG32(0xb8003314) = 0x000000D8;	USB2_PHY_DELAY;
	REG32(0xb814020c) = 0x000000D8;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x00360000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x00260000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x00360000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x003E0000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x002E0000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x003E0000;	USB2_PHY_DELAY;

	/* For Port-1 */
	//REG32(0xb8003314) = 0x000E0000;	USB2_PHY_DELAY;
	REG32(0xb814020c) = 0x000E0000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x00540000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x00440000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x00540000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x005E0000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x004E0000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x005E0000;	USB2_PHY_DELAY;
	//REG32(0xb8003314) = 0x00D80000;	USB2_PHY_DELAY;
	REG32(0xb814020c) = 0x00D80000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x00560000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x00460000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x00560000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x005E0000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x004E0000;	USB2_PHY_DELAY;
	REG32(0xb80210A4) = 0x005E0000;	USB2_PHY_DELAY;

	printk("USB 2.0 Phy Patch(D): &0xb80210A4 = %08x\n", REG32(0xb80210A4));	/* A85E0000 */
#endif
#elif defined(CONFIG_RTL_8196C)

	//disable Host chirp J-K
	SetUSBPhy(0xf4,0xe3);	GetUSBPhy(0xf4);
	//8196C demo board: 0xE0:99, 0xE1:A8, 0xE2:98, 0xE3:C1,  0xE5:91,
	SetUSBPhy(0xe0,0x99);   if(GetUSBPhy(0xe0)!=0x99) printk("reg 0xe0 not correct\n");
	SetUSBPhy(0xe1,0xa8);	if(GetUSBPhy(0xe1)!=0xa8) printk("reg 0xe1 not correct\n");
	SetUSBPhy(0xe2,0x98);	if(GetUSBPhy(0xe2)!=0x98) printk("reg 0xe2 not correct\n");
	SetUSBPhy(0xe3,0xc1);	if(GetUSBPhy(0xe3)!=0xc1) printk("reg 0xe3 not correct\n");
	SetUSBPhy(0xe5,0x91);	if(GetUSBPhy(0xe5)!=0x91) printk("reg 0xe5 not correct\n");

	//test packet.
	/*
	REG32(0xb8021054)=0x85100000;
	REG32(0xb8021010)=0x08000100;
	REG32(0xb8021054)=0x85180400;
	*/
	printk("USB 2.0 PHY Patch Done.\n");

#else
	printk("========================== NO PATCH USB 2.0 PHY =====================\n");
#endif
	return;
}
#endif

//--------------------------------------------
void EnableUSBPHY(int portnum)
{
	//printk("enable usb PHY %x\n",portnum);
	if (portnum==0) {
		//phy0
		REG32(0xb800016c) |= (1<<8);	 //USBPHY_EN=1
		REG32(0xb800016c) |=   (1<<9);	 //usbphy_reset=1, active high
		REG32(0xb800016c) &= ~(1<<9);	 //usbphy_reset=0, active high
		//REG32(0xb800016c) &= ~(1<<9);	 //usbphy_reset=0, active high
		//REG32(0xb800016c) |=   (1<<9);	 //usbphy_reset=1, active high

		REG32(0xb800016c) |= (1<<10);	 //active_usbphyt=1

		REG32(0xb8140210) |= (1<<7);
		REG32(0xb8140210) &= ~(1<<5);	 //disable force mode
	}
	else {
		//phy1
		REG32(0xb800016c) |= (1<<19);	 //USBPHY_EN=1
		REG32(0xb800016c) |=   (1<<20);	 //usbphy_reset=1, active high
		REG32(0xb800016c) &= ~(1<<20);	 //usbphy_reset=0, active high
		//REG32(0xb800016c) &= ~(1<<20);	 //usbphy_reset=0, active high
		//REG32(0xb800016c) |=   (1<<20);	 //usbphy_reset=1, active high

		REG32(0xb800016c) |= (1<<21);	 //active_usbphyt=1

		REG32(0xb8140210) |= (1<<23);	 //
		REG32(0xb8140210) &= ~(1<<21);	 //disable force mode
	}
}

static int __init rtl819x_usb_setup(void)
{
	u32 uphy_port0=0,uphy_port1=0;
	int i;//for debug

	/*
	 * HH71VM: SWITCHING THE USB MULTIPLEXER WITH GPIO 24.
	 *
	 * WHY. The USB port of the SoC is routed through a hardware multiplexer: one position is
	 * the internal link to the Qualcomm modem (RNDIS), the other is the external microUSB
	 * connector. After a reset the pin stays in peripheral mode, nothing drives the switch
	 * line, and the modem never reaches the bus - with a perfectly working USB stack.
	 * That is what looked, for two rounds, like "the controller came up, the ports are alive,
	 * and the port does not even see a physical connection".
	 *
	 * WHERE THE CODE COMES FROM. The stock bootloader, function 0x8000BCA0 (disassembled
	 * byte by byte), which prints "usb switch:0" into the log. The bootloader only calls it on
	 * the branch that boots an image from flash (0x8000C0B4), so our "TFTP plus Jump" path went
	 * past it and the switch stayed where it was.
	 *
	 * The addresses are checked against bspchip.h: 0xB8000840 = BSP_PIN_MUX_SEL16, 0xB8003500/08/0C =
	 * BSP_PABCD_CNR/DIR/DAT. Bit 24 is port D bit 0, that is GPIO 24. The semantics of CNR are
	 * confirmed by gpio.c: rtl819x_gpio_pin_enable() clears the bit ("0 as BSP_GPIO
	 * pin"), and ..._disable() sets it ("1 as peripheral pin").
	 *
	 * Confirmed live: the same four writes, made by hand with EW commands in <RealTek>
	 * before a RAM boot, give "usb 1-1: new high-speed USB device" and rndis_host with
	 * the same MAC as stock.
	 *
	 *
	 * Done BEFORE the USB clocks are enabled, as in the bootloader - before the controller starts.
	 */
	REG32(BSP_PIN_MUX_SEL16) = (REG32(BSP_PIN_MUX_SEL16) & ~0xF) | 5;
	REG32(BSP_PABCD_CNR) &= ~(1 << 24);	/* 0 = the pin in GPIO mode */
	REG32(BSP_PABCD_DIR) |=  (1 << 24);	/* 1 = output */
	REG32(BSP_PABCD_DAT) &= ~(1 << 24);	/* drive 0 = link to the modem */
	printk("HH71VM usb switch:%d\n",
	       (REG32(BSP_PABCD_DAT) & (1 << 24)) ? 1 : 0);

	REG32(0xb8000014) |= (1<<3);	 //enable usb clock
	mdelay(50);
	//printk("enable USB host\n");

	REG32(0xb8140210) &= 0xfdfffdff;	 //20141216 hanyi FPGA fix suspend drop issue

	//enable port 0 two port
	 REG32(0xb8000180) =0;

#if 1 //port 0
	pr_debug("enable port 0 two port enable\n");

       REG32(0xb8000180) |= (1<<17);	 //two port usb 0: one port 1: two port
	   REG32(0xb8000180) |= (1<<18);	 //otg usb host select port 1 host


	//REG32(0xb8000180)=0;
//enable port 1,port 0 will fail   one port and two port
#else    //port 1	should verify bit 17=1 and 0

//#define one_port_select 1

#ifdef one_port_select
	pr_debug("enable port 1 only ,verify port 1\n");
	//REG32(0xb8000180) |= (1<<17);	 //otg usb host selec
	REG32(0xb8000180) |= (1<<18);	 //otg usb host select
#endif

#ifndef one_port_select

//enable port one and port 0
       pr_debug("enable port 0 and port 1 verify port one \n");

	REG32(0xb8000180) |= (1<<17);	 //otg usb host selec 0: one port 1: two port
	REG32(0xb8000180) |= (1<<18);	 //otg usb host select // 0: otg 1; ehci
#endif

#endif

	REG32(0xb8140208)=0x02000000;// USB debug port select

	REG32(0xb8000160) |= (1);	 //enable usb host  /20141216 hanyi FPGA fix suspend drop issue
	 //REG32(0xb8000014) |= (1<<3);	 //enable usb



		//printk("enable usb phy 0 \n");
              EnableUSBPHY(0);
		//printk("enable usb phy 1 \n");
		EnableUSBPHY(1);
//#ifdef CONFIG_RTL_OTGCTRL
//	  TurnOn_OTGCtrl_Interrupt(old);
//#endif
	// }

	// printk("set IP clock \n");
	//ip clock mgr
#if 1
	// REG32(0xb8000010) |= (1<<12)|(1<<13)|(1<<19)|(1<<20);	 //enable lx1, lx2
	 //REG32(0xb8000010) |= (1<<21);	 //enable host ip usb3

	 //printk("enable usb host on system register 0x14 bit3 \n");
	 REG32(0xb8000014) |= (1<<3);	 //enable usb
#endif

	mdelay(100);

	 /*	b8021000=10000001
		b8021054=200000
	*/


//#endif

	/*register platform device*/
#ifdef CONFIG_USB_ARCH_HAS_EHCI
	platform_device_register(&rtl819x_ehci_device);
#endif
#ifdef CONFIG_USB_ARCH_HAS_OHCI
	platform_device_register(&rtl819x_ohci_device);
#endif

#if defined(CONFIG_RTL_8196C)
	synopsys_usb_patch();
#endif

	//auto detect usb phy
#if 1
	pr_debug("port 0 org 0xe0=%x\n",GetUSBPhy(0xe0) );
	pr_debug("port 0 org 0xe1=%x\n",GetUSBPhy(0xe1) );
	pr_debug("port 0 org 0xe2=%x\n",GetUSBPhy(0xe2) );
	pr_debug("port 0 org 0xe4=%x\n",GetUSBPhy(0xe4) );
	pr_debug("port 0 org 0xe6=%x\n",GetUSBPhy(0xe6) );

	pr_debug("port 1 org 0xe0=%x\n",GetUSBPhy_1(0xe0) );
	pr_debug("port 1 org 0xe1=%x\n",GetUSBPhy_1(0xe1) );
	pr_debug("port 1 org 0xe2=%x\n",GetUSBPhy_1(0xe2) );
	pr_debug("port 1 org 0xe4=%x\n",GetUSBPhy_1(0xe4) );
	pr_debug("port 1 org 0xe6=%x\n",GetUSBPhy_1(0xe6) );
#endif

#if 1 //auto detect PHY
	if ((GetUSBPhy(0xe0)==0x9c) && (GetUSBPhy(0xe1)==0xac)) {
		pr_debug("port 0  is U55 0287B \n");
		uphy_port0=1;
	}

	if ((GetUSBPhy(0xe0)==0xe0) && (GetUSBPhy(0xe1)==0x2f))	{
		pr_debug("port0 is u28 0599 PHY\n");
		uphy_port0=2;
	}

	if ((GetUSBPhy_1(0xe0)==0x9c) && (GetUSBPhy_1(0xe1)==0xac)) {
		pr_debug("port1 is U55 0287B \n");
		uphy_port1=1;
	}

	if ((GetUSBPhy_1(0xe0)==0xe1) && (GetUSBPhy_1(0xe1)==0x2f)) {
		pr_debug("port1 is u28 0599 PHY\n");
		uphy_port1=2;
	}

	if ((GetUSBPhy_1(0xe0)==0xe0) & (GetUSBPhy_1(0xe1)==0x2e)) {
		pr_debug("port1 is u28 0599 PHY\n");
		uphy_port1=2;
	}
#endif


//config port 1 u28phy
#if 0
	if (uphy_port1==2) {
		SetUSBPhy(0xf4,0x9b,1);//switch back to page 0
		mdelay(10);//it seems need delay for stitch page

		SetUSBPhy(0xe0,0xe1,1);
		SetUSBPhy(0xe1,0x2e,1);
		SetUSBPhy(0xe2,0xb5,1);
		SetUSBPhy(0xe4,0x9b,1);
		SetUSBPhy(0xe6,0xca,1);
		//SetUSBPhy(0xe0,0xe1,1);

		if (GetUSBPhy_1(0xf4)!=0x9b) printk("reg 0xf4 not correct!!!!!!!!!\n");
		if(GetUSBPhy_1(0xe0)!=0xe1) printk("reg 0xe0 not correct!!!!!!!!!!!\n");
		if(GetUSBPhy_1(0xe1)!=0x2e) printk("reg 0xe1 not correct!!!!!!!!!!\n");
		if(GetUSBPhy_1(0xe2)!=0xb5) printk("reg 0xe2 not correct!!!!!!!!!!\n");
		if(GetUSBPhy_1(0xe4)!=0x9b) printk("reg 0xe4 not correct!!!!!!!!!!\n");
		if(GetUSBPhy_1(0xe6)!=0xca) printk("reg 0xca not correct!!!!!!!!\n");

		/*printk("U28 port 1 page 0 reg %x=%x should 0x9b\n",0xf4,GetUSBPhy_1(0xf4) );
		printk("U28 port 1 page 0 reg %x=%x should 0xe1\n",0xe0,GetUSBPhy_1(0xe0) );
		printk("U28 port 1 page 0 reg %x=%x should 0x2e\n",0xe1,GetUSBPhy_1(0xe1) );
		printk("port 1 reg page 0 %x=%x should 0xb5\n",0xe2,GetUSBPhy_1(0xe2) );
		printk("port 1 reg page 0 %x=%x should 0x9b\n",0xe4,GetUSBPhy_1(0xe4) );
		printk("port 1 reg page 0 %x=%x should 0xca\n",0xe6,GetUSBPhy_1(0xe6) );*/

		printk("port 1 page1\n");
		SetUSBPhy(0xf4,0xbb,1);
		SetUSBPhy(0xe6,0x18,1);
		SetUSBPhy(0xe7,0xe3,1);
		//if(GetUSBPhy_1(0xf4)!=0xbb) printk("reg 0xf4 not correct !!!!!!!!!!!!!!\n");
		//if(GetUSBPhy_1(0xe6)!=0x18) printk("reg 0xe6 not correct!!!!!!!!!!!!!!!\n");
		//if(GetUSBPhy_1(0xe7)!=0xe3) printk("reg 0xe7 not correct!!!!!!!!!!!!!!!\n");
		SetUSBPhy(0xe0,0x25,1);
		SetUSBPhy(0xe1,0xef,1);
		SetUSBPhy(0xe5,0x0f,1);
		SetUSBPhy(0xe6,0x18,1);
		SetUSBPhy(0xe7,0xe3,1);

		if(GetUSBPhy_1(0xf4)!=0xbb) printk("port 1 reg 0xf4  not correct!!!!!!!!!!!!!!!!!!!\n");
		if(GetUSBPhy_1(0xe0)!=0x25) printk("p1 reg 0xe0 not correct!!!!!!!!!!!!!!!!\n");
		if(GetUSBPhy_1(0xe1)!=0xef) printk("p1 reg 0xe1 not correct!!!!!!!!!!!!!!!!!!\n");
		if(GetUSBPhy_1(0xe5)!=0x0f) printk("p1 reg 0xe5 not correct!!!!!!!!!!!!!!!!!\n");
		if(GetUSBPhy_1(0xe6)!=0x18) printk("p1 reg 0xe6 not correct!!!!!!!!!!!!!!!!!!!!\n");
		if(GetUSBPhy_1(0xe7)!=0xe3) printk("p1 reg 0xe7 not correct!!!!!!!!!!!!!!!!!!!!!!\n");

		SetUSBPhy(0xf4,0x9b,1);
	}
#endif

//#define U28

#if 0 //defined U28 port 0
	SetUSBPhy(0xf4,0x9b,0);
	SetUSBPhy(0xe0,0xe1,0);
	SetUSBPhy(0xe1,0x2e,0);
	SetUSBPhy(0xe2,0xb5,0);
	SetUSBPhy(0xe4,0x9b,0);
	SetUSBPhy(0xe6,0xca,0);

	printk("U28 port 0 page 0 reg %x should 0xe1=%x\n",0xe0,GetUSBPhy(0xe0) );
	printk("U28 port 0 page 0 reg %x=%x should 0x2e\n",0xe1,GetUSBPhy(0xe1) );
	printk("U28 port 0 reg page 0 %x=%x should 0xb5\n",0xe2,GetUSBPhy(0xe2) );
	printk("port 0 reg page 0 %x=%x should 0x9b\n",0xe4,GetUSBPhy(0xe4) );
	printk("port 0 reg page 0 %x=%x should 0xca\n",0xe6,GetUSBPhy(0xe6) );


	printk("port 0 page1\n");
	SetUSBPhy(0xf4,0xbb,0);
	SetUSBPhy(0xe0,0x25,0);
	SetUSBPhy(0xe1,0xef,0);
	SetUSBPhy(0xe5,0x0f,0);
	SetUSBPhy(0xe6,0x18,0);
	SetUSBPhy(0xe7,0xe3,0);

	printk("U28 port 0 reg %x=%x should 0xbb\n",0xf4,GetUSBPhy(0xf4) );
	printk("U28 port 0 reg %x=%x should 0x25\n",0xe0,GetUSBPhy(0xe0) );
	printk("U28 port 0 reg %x=%x should 0xef\n",0xe1,GetUSBPhy(0xe1) );
	printk("U28 port 0 reg %x=%x should 0x0f\n",0xe5,GetUSBPhy(0xe5) );
	printk("U28 port 0 reg %x=%x should 0x18\n",0xe6,GetUSBPhy(0xe6) );
	printk("U28 port 0 reg %x=%x should 0xe3\n",0xe7,GetUSBPhy(0xe7) );

#endif

//u28 phy port 1
#if 0
	SetUSBPhy(0xf4,0x9b,1);
	SetUSBPhy(0xe0,0xe1,1);
	SetUSBPhy(0xe1,0x2e,1);
	SetUSBPhy(0xe2,0xb5,1);
	SetUSBPhy(0xe4,0x9b,1);
	SetUSBPhy(0xe6,0xca,1);

	if(GetUSBPhy_1(0xf4)!=0x9b) printk("reg 0xf4 not correct!!!!!!!!!\n");
	if(GetUSBPhy_1(0xe0)!=0xe1) printk("reg 0xe0 not correct!!!!!!!!!!!\n");
	if(GetUSBPhy_1(0xe1)!=0x2e) printk("reg 0xe1 not correct!!!!!!!!!!\n");
	if(GetUSBPhy_1(0xe2)!=0xb5) printk("reg 0xe2 not correct!!!!!!!!!!\n");
	if(GetUSBPhy_1(0xe4)!=0x9b) printk("reg 0xe4 not correct!!!!!!!!!!\n");
	if(GetUSBPhy_1(0xe6)!=0xca) printk("reg 0xca not correct!!!!!!!!\n");

	printk("port 1 page1\n");
	SetUSBPhy(0xf4,0xbb,1);
	SetUSBPhy(0xe6,0x18,1);
	SetUSBPhy(0xe7,0xe3,1);
	//if(GetUSBPhy_1(0xf4)!=0xbb) printk("reg 0xf4 not correct !!!!!!!!!!!!!!\n");
	//if(GetUSBPhy_1(0xe6)!=0x18) printk("reg 0xe6 not correct!!!!!!!!!!!!!!!\n");
	//if(GetUSBPhy_1(0xe7)!=0xe3) printk("reg 0xe7 not correct!!!!!!!!!!!!!!!\n");
	SetUSBPhy(0xe0,0x25,1);
	SetUSBPhy(0xe1,0xef,1);
	SetUSBPhy(0xe5,0x0f,1);
	SetUSBPhy(0xe6,0x18,1);
	SetUSBPhy(0xe7,0xe3,1);

	if(GetUSBPhy_1(0xf4)!=0xbb) printk("port 1 reg 0xf4  not correct!!!!!!!!!!!!!!!!!!!\n");
	if(GetUSBPhy_1(0xe0)!=0x25) printk("p1 reg 0xe0 not correct!!!!!!!!!!!!!!!!\n");
	if(GetUSBPhy_1(0xe1)!=0xef) printk("p1 reg 0xe1 not correct!!!!!!!!!!!!!!!!!!\n");
	if(GetUSBPhy_1(0xe5)!=0x0f) printk("p1 reg 0xe5 not correct!!!!!!!!!!!!!!!!!\n");
	if(GetUSBPhy_1(0xe6)!=0x18) printk("p1 reg 0xe6 not correct!!!!!!!!!!!!!!!!!!!!\n");
	if(GetUSBPhy_1(0xe7)!=0xe3) printk("p1 reg 0xe7 not correct!!!!!!!!!!!!!!!!!!!!!!\n");
	//}
	//else
	//	printk("U55 PHY \n");
#endif

	SetUSBPhy(0xf4,0x9b,1);

#if 0
	for(t=0;t<10;t++) {
		/*printk("U28 port 1 page 0 reg %x=%x should 0xbb\n",0xf4,GetUSBPhy_1(0xf4) );
		printk("port 1 reg page 1 %x=%x should 0x18\n",0xe6,GetUSBPhy_1(0xe6) );
		printk("port 1 reg page 1 %x=%x should 0xe3\n",0xe7,GetUSBPhy_1(0xe7) );*/
		printk("0xf4=%x\n",GetUSBPhy_1(0xf4));
		if(GetUSBPhy_1(0xe6)!=0x18) SetUSBPhy(0xe6,0x18,1);
		else { printk("set page 1 0xe6 t=%x\n",t); break;}
	}

	for(t=0;t<10;t++) {
		/*printk("U28 port 1 page 0 reg %x=%x should 0xbb\n",0xf4,GetUSBPhy_1(0xf4) );
		printk("port 1 reg page 1 %x=%x should 0x18\n",0xe6,GetUSBPhy_1(0xe6) );
		printk("port 1 reg page 1 %x=%x should 0xe3\n",0xe7,GetUSBPhy_1(0xe7) );*/
		printk("0xf4=%x\n",GetUSBPhy_1(0xf4));
		if(GetUSBPhy_1(0xe7)!=0xe3) SetUSBPhy(0xe7,0xe3,1);
		else { printk("set page 1 0xe7 t=%x\n",t); break;}
	}

	//SetUSBPhy(0xe6,0x18,1);
	//SetUSBPhy(0xe7,0xe3,1);

	printk("port 1 reg page 1 %x=%x should 0x18\n",0xe6,GetUSBPhy_1(0xe6) );
	printk("port 1 reg page 1 %x=%x should 0xe3\n",0xe7,GetUSBPhy_1(0xe7) );
#endif


#if 1
//#define 97F_B cut ASIC 40Mhz
if(REG32(0xb8000008)&(1<<24))
{
	pr_debug("patch new usb phy para for 40M OSC\n");
//printk("  PHY reg 0xf4=%x\n",GetUSBPhy(0xf4) );

//port 0
		SetUSBPhy(0xf4,0x9b,0);		//back to page 0
		SetUSBPhy(0xe2,0x33,0);
		//SetUSBPhy(0xe4,0x99,0);
		SetUSBPhy(0xe4,0xc9,0);		//for disconnect
		SetUSBPhy(0xe6,0xc1,0);
		SetUSBPhy(0xf4,0xbb,0);		//page 1
		SetUSBPhy(0xe6,0x00,0);
		SetUSBPhy(0xe7,0x00,0);
		SetUSBPhy(0xf4,0x9b,0);		//back to page 0
//port 1
		SetUSBPhy(0xf4,0x9b,1);		//back to page 0
		SetUSBPhy(0xe2,0x33,1);
		//SetUSBPhy(0xe4,0x99,1);		//for disconnect
		SetUSBPhy(0xe4,0xc9,1);
		SetUSBPhy(0xe6,0xc1,1);
		SetUSBPhy(0xf4,0xbb,1);		//page 1
		SetUSBPhy(0xe6,0x00,1);
		SetUSBPhy(0xe7,0x00,1);
		SetUSBPhy(0xf4,0x9b,1);		//back to page 0

}
else
{
pr_debug("patch new usb phy para for 25M OSC\n");
printk("  PHY reg 0xf4=%x\n",GetUSBPhy(0xf4) );

//port 0
		//switch to page 0 for sure
		SetUSBPhy(0xf4,0x9b,0);
		SetUSBPhy(0xe0,0xe3,0);
		SetUSBPhy(0xe1,0x30,0);
		SetUSBPhy(0xe2,0xd5,0);
		//SetUSBPhy(0xe4,0x99,0);
		SetUSBPhy(0xe4,0xc9,0);	//for disconnect level
		SetUSBPhy(0xe6,0xc1,0);
		//switch to page 1
		SetUSBPhy(0xf4,0xbb,0);
		SetUSBPhy(0xe5,0x11,0);
		SetUSBPhy(0xe6,0x06,0);
		SetUSBPhy(0xe7,0x66,0);
		SetUSBPhy(0xf4,0x9b,0);
//port 1
		//switch to page 0 for sure
		SetUSBPhy(0xf4,0x9b,1);
		SetUSBPhy(0xe0,0xe3,1);
		SetUSBPhy(0xe1,0x30,1);
		SetUSBPhy(0xe2,0xd5,1);
		//SetUSBPhy(0xe4,0x99,1);
		SetUSBPhy(0xe4,0xc9,1);	//for disconnect level
		SetUSBPhy(0xe6,0xc1,1);
		//switch to page 1
		SetUSBPhy(0xf4,0xbb,1);
		SetUSBPhy(0xe5,0x11,1);
		SetUSBPhy(0xe6,0x06,1);
		SetUSBPhy(0xe7,0x66,1);
		SetUSBPhy(0xf4,0x9b,1);




}
#else
//#define 97F_B cut ASIC 40Mhz OK

pr_debug("patch new usb phy para for 40M OSC\n");
//printk("  PHY reg 0xf4=%x\n",GetUSBPhy(0xf4) );

//port 0
//	SetUSBPhy(0xe1,0x30,0);
	//switch to page 1
	SetUSBPhy(0xf4,0xbb,0);
	//adjust page 1 para

	SetUSBPhy(0xe6,0x00,0);
	SetUSBPhy(0xe7,0x00,0);
	//switch back to page 0
	SetUSBPhy(0xf4,0x9b,0);

//port 1
	//SetUSBPhy(0xe1,0x30,1);
	//switch to page 1
	SetUSBPhy(0xf4,0xbb,1);
	//adjust page 1 para

	SetUSBPhy(0xe6,0x00,1);
	SetUSBPhy(0xe7,0x00,1);
	//switch back to page 0
	SetUSBPhy(0xf4,0x9b,1);




#endif


SetUSBPhy(0xf4,0x9b,1);









	pr_debug("system  reg %x=0x%x\n",0xb8000010,REG32(0xb8000010));
	pr_debug("system  reg %x=0x%x\n",0xb8000014,REG32(0xb8000014));
	pr_debug("system  reg %x=0x%x\n",0xb8000160,REG32(0xb8000160));
	pr_debug("system  reg %x=0x%x\n",0xb8000164,REG32(0xb8000164));
	pr_debug("system  reg %x=0x%x\n",0xb8000168,REG32(0xb8000168));
	pr_debug("system  reg %x=0x%x\n",0xb800016c,REG32(0xb800016c));
	pr_debug("system  reg %x=0x%x\n",0xb8000180,REG32(0xb8000180));
	pr_debug("system  reg %x=0x%x\n",0xb8021094,REG32(0xb8021094));
	pr_debug("system  reg %x=0x%x\n",0xb8140200,REG32(0xb8140200));
	pr_debug("system  reg %x=0x%x\n",0xb8140204,REG32(0xb8140204));
	pr_debug("system  reg %x=0x%x\n",0xb8140208,REG32(0xb8140208));
	pr_debug("system  reg %x=0x%x\n",0xb814020c,REG32(0xb814020c));
	pr_debug("system  reg %x=0x%x\n",0xb8140210,REG32(0xb8140210));

	pr_debug("\n\nEHCI  reg %x=0x%x\n",0xb8021050,REG32(0xb8021050));
	pr_debug("EHCI  reg %x=0x%x\n",0xb8021054,REG32(0xb8021054));
	pr_debug("EHCI  reg %x=0x%x\n",0xb8021058,REG32(0xb8021058));
#ifdef CONFIG_USB_ARCH_HAS_OHCI
	pr_debug("OHCI  reg %x=0x%x\n",0xb8020000,REG32(0xb8020000));
	pr_debug("OHCI  reg %x=0x%x\n",0xb8020004,REG32(0xb8020004));
#endif

#if 1 //wei add
	//dump
	//int i;
	for(i=0xe0;i<=0xe7; i++)
		pr_debug("port 0 reg %x=%x\n", i,GetUSBPhy(i) );
	for(i=0xf0;i<=0xf6; i++)
		pr_debug("port 0 reg %x=%x\n", i,GetUSBPhy(i) );

    for(i=0xe0;i<=0xe7; i++)
		pr_debug("port 1 reg %x=%x\n", i,GetUSBPhy_1(i) );
	for(i=0xf0;i<=0xf6; i++)
		pr_debug("port 1 reg %x=%x\n", i,GetUSBPhy_1(i) );
#endif

	/*
	 * HH71VM: A SECOND UTMI RESET of both PHYs after they have been programmed.
	 *
	 * WHY. The Qualcomm modem is attached to the Realtek side over USB (RNDIS) and did not
	 * appear on the bus at all in our build: the EHCI controller came up properly, both ports of
	 * the root hub were alive, but the port did not even see a physical connection.
	 *
	 * Every system register and every USB PHY register MATCHES stock bit for bit
	 * (checked against the stock UART log: b8000010/14/160/164/168/16c/180, b8021094,
	 * b8140200/204/208/20c/210, plus PHY e0,e1,e2,e4,e6 on both ports). So the configuration
	 * does not diverge - the SEQUENCE does.
	 *
	 * Stock does it in this order (from the log, the stage after EHCI registration):
	 *     Otg act as Host mode
	 *     =>do UTMI reset, r=1 / r=0      <- the reset
	 *     RTL8197F u2 phy 40MHz patch     <- programming the PHY
	 *     ... a dump of pages 0 and 1 ...
	 *     =>do UTMI reset, r=1 / r=0      <- the reset AGAIN, this time after programming
	 * and only then `usb 1-1: new high-speed USB device`.
	 *
	 * We do reset in EnableUSBPHY() - but BEFORE the patch, and nobody repeats it afterwards.
	 * A UTMI reset restarts connection detection on the line, so without it a device that is
	 * already attached can stay unnoticed.
	 *
	 * The code of the stock OTG stage is not available to us (the GPL archive has no
	 * drivers/usb/dwc_otg/ directory at all), and the version from the AC10 fork does not fit: it
	 * touches register 0xb8000090, whereas on the 8197F that is 0xb800016c. So we reproduce the
	 * observed behaviour rather than the code - with the same bits EnableUSBPHY() uses above:
	 * bit 9 is usbphy_reset of port 0, bit 20 is that of port 1.
	 *
	 * This is what made the modem appear on the bus.
	 *
	 *
	 */
	pr_debug("HH71VM: second UTMI reset of both PHYs after programming\n");

	REG32(0xb800016c) |=  (1<<9);		/* port 0: usbphy_reset = 1 */
	mdelay(2);
	REG32(0xb800016c) &= ~(1<<9);		/* port 0: usbphy_reset = 0 */

	REG32(0xb800016c) |=  (1<<20);		/* port 1: usbphy_reset = 1 */
	mdelay(2);
	REG32(0xb800016c) &= ~(1<<20);		/* port 1: usbphy_reset = 0 */

	mdelay(20);

	return 0;
}

static int __init bsp_usb_init(void)
{
	printk("INFO: initializing USB devices ...\n");
	rtl819x_usb_setup();
	return 0;
}

arch_initcall(bsp_usb_init);
