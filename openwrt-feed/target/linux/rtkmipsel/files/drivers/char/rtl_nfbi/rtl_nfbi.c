/*
 *  RTL8197B NFBI char driver
 *
 *	Copyright (C)2008, Realtek Semiconductor Corp. All rights reserved.
 *
 */
/*================================================================*/
/* Include Files */


#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/circ_buf.h>
/* This used to include <asm/uaccess.h>. On 4.14 the MIPS architecture header itself uses
   uaccess_kernel() from <linux/uaccess.h> will not build without it. */
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include "rtl_nfbi.h"
#include <linux/gpio.h>
/*
 * The vendor's register map was included only with CONFIG_RTL819X_GPIO, although
 * REG32() is used everywhere in this file (line 115 onwards) - that is, without this
 * The symbol file was not assembled in principle. The path is the same as in rtl_nic.c: in the tree
 * The OpenWrt header is in arch/mips/include/asm/mach-rtl8197f/, not in bsp/.
 */
#if defined(CONFIG_SOC_RTL8198C) && defined(CONFIG_OPENWRT_SDK)
#include <asm/mach-realtek/bspchip.h>
#elif defined(CONFIG_SOC_RTL8197F) && defined(CONFIG_OPENWRT_SDK)
#include <asm/mach-rtl8197f/bspchip.h>
#elif defined(CONFIG_RTL819X_GPIO)
#include <../bsp/bspchip.h>
#endif

/*================================================================*/
/* RTL8651C MDC/MDIO Control Register, @Pana_TBD */
#define SWMACCR_BASE                (0xBB804000)
#define MDCIOCR                     (0x004+SWMACCR_BASE) /* MDC/MDIO Command Register */
#define MDCIOSR                     (0x008+SWMACCR_BASE) /* MDC/MDIO Status Register */

/* GPIO Register Set */
#define GPIO_BASE                   (0xB8003500)
#define PABCDCNR_REG				(0x000 + GPIO_BASE) /* Port ABCD control */
#define PABCDPTYPE_REG				(0x004 + GPIO_BASE) /* Port ABCD type */
#define PABCDDIR_REG				(0x008 + GPIO_BASE) /* Port ABCD direction*/
#define PABCDDAT_REG				(0x00C + GPIO_BASE) /* Port ABCD data */
#define PABCDISR_REG				(0x010 + GPIO_BASE) /* Port ABCD interrupt status */
#define PABIMR_REG				    (0x014 + GPIO_BASE) /* Port AB interrupt mask */
#define PCDIMR_REG				    (0x018 + GPIO_BASE) /* Port CD interrupt mask */
#define PEFGHCNR_REG				(0x01C + GPIO_BASE) /* Port EFGH control */
#define PEFGHPTYPE_REG				(0x020 + GPIO_BASE) /* Port EFGH type */
#define PEFGHDIR_REG				(0x024 + GPIO_BASE) /* Port EFGH direction*/
#define PEFGHDAT_REG				(0x028 + GPIO_BASE) /* Port EFGH data */
#define PEFGHISR_REG				(0x02C + GPIO_BASE) /* Port EFGH interrupt status */
#define PEFIMR_REG				    (0x030 + GPIO_BASE) /* Port EF interrupt mask */
#define PGHIMR_REG				    (0x034 + GPIO_BASE) /* Port GH interrupt mask */

#define MDIO_TIMEOUT 2000000
#define DEFAULT_MDIO_PHYAD 16  /* selected by the hardware strapping pin of external Host CPU, @Pana_TBD */

//#define REG32(reg) 			(*((volatile unsigned int *)(reg)))

#ifdef MDCIO_GPIO_SIMULATION
//RTL8651C - F0: MDC, F1: MDIO
//RTL8196  - E5: MDC, E6: MDIO
//RTL8954C V200 EVB - G1: MDC, G0: MDIO, E2: RESET
//RTL8954C V400 EVB - G1: MDC, F2: MDIO, E2: RESET
//#define RTL8651C_FAMILY
#define MDC_PIN		BSP_GPIO_PIN_G6
#define MDIO_PIN	BSP_GPIO_PIN_C3

static int mdc_pin;
static int mdio_pin;
static unsigned long mdcdir_reg = PABCDDIR_REG;
static unsigned long mdiodir_reg = PABCDDIR_REG;
static unsigned long mdcdat_reg = PABCDDAT_REG;
static unsigned long mdiodat_reg = PABCDDAT_REG;
#endif

/*================================================================*/
/* Local Variables */
static DEFINE_SPINLOCK(mdio_lock);
static struct nfbi_dev_priv *dev_priv=NULL;

/*----------------------------------------------------------------------------*/
#ifdef MDCIO_GPIO_SIMULATION

#define DELAY		50

/*
 * MDC/MDIO API: use two GPIO pin to simulate MDC/MDIO signal
 *
 */
void _smiGpioInit(void)
{
	printk("Init GPIO for MDC/MDIO.\n");

	#ifdef CONFIG_RTK_VOIP_GPIO_8954C_V400	//G1: MDC, F2: MDIO, E2: RESET
	// PIN Mux select (register PIN_MUX_SEL bit 14, 8, 9)
	*((volatile unsigned int *)0xB8000040) = 
		*((volatile unsigned int *)0xB8000040) | 0x4300;
	//config as GPIO pin
	REG32(PEFGHCNR_REG) = REG32(PEFGHCNR_REG) & (~0x00020400);
	printk("PEFGHCNR_REG=%X\n", REG32(PEFGHCNR_REG));
	//Disable G1 interrupt
	REG32(PGHIMR_REG) = REG32(PGHIMR_REG) & (~0x0000000C);
	printk("PGHIMR_REG=%X\n", REG32(PGHIMR_REG));
	//Disable F2 interrupt
	REG32(PEFIMR_REG) = REG32(PEFIMR_REG) & (~0x00300000);
	printk("PEFIMR_REG=%X\n", REG32(PEFIMR_REG));
	//set G0, F2 output pin
	REG32(PEFGHDIR_REG) = REG32(PEFGHDIR_REG) | 0x00020400;
	printk("PEFGHDIR_REG=%X\n", REG32(PEFGHDIR_REG));
	#else 									//G1: MDC, G0: MDIO, E2: RESET

	// PIN Mux select (register PIN_MUX_SEL bit 14, 8, 9)
	//config as GPIO pin
	REG32(PABCDCNR_REG) = REG32(PABCDCNR_REG) & (~0x00080000);
	REG32(PEFGHCNR_REG) = REG32(PEFGHCNR_REG) & (~0x00400000);
	printk("PABCDCNR_REG=%X\n", REG32(PABCDCNR_REG));
	//set G0, G1 output pin
	REG32(PABCDDIR_REG) = REG32(PABCDDIR_REG) | 0x00080000;
	REG32(PEFGHDIR_REG) = REG32(PEFGHDIR_REG) | 0x00400000;
	printk("PABCDDIR_REG=%X\n", REG32(PABCDDIR_REG));
	
	#endif
}

/*
static void _smiGenReadClk(void) 
{
	unsigned short i;
#ifdef RTL8651C_FAMILY
	REG32(PEFGHDIR_REG) = (REG32(PEFGHDIR_REG)& 0xFFFFFCFF) | 0x00000100;	// MDC=OUT, MDIO=IN
	REG32(PEFGHDAT_REG) = (REG32(PEFGHDAT_REG) & 0xFFFFFCFF) | 0x00000100;	// MDC=1, MDIO=0
	for(i=0; i< DELAY; i++);
	REG32(PEFGHDAT_REG) = (REG32(PEFGHDAT_REG) & 0xFFFFFCFF);	// MDC=0
#else
#error "Need implement for _smiGenReadClk"
#endif
}
*/

static void _smiGenWriteClk(void) 
{
	unsigned short i;

	#ifdef CONFIG_RTK_VOIP_GPIO_8954C_V400	//G1: MDC, F2: MDIO
	for(i=0; i< DELAY; i++);
	REG32(PABCDDAT_REG) = REG32(PABCDDAT_REG) & 0xFFFDFBFF;// MDC=0, MDIO=0
	for(i=0; i< DELAY; i++);
	REG32(PABCDDAT_REG) = REG32(PABCDDAT_REG) | 0x00020000;	// MDC=1
	#else									//C2: MDC, C3: MDIO

	for(i=0; i< DELAY; i++);
	REG32(mdiodat_reg) = REG32(mdiodat_reg) & ~(1<<mdio_pin);// MDIO=0 
	REG32(mdcdat_reg) = REG32(mdcdat_reg) & ~(1<<mdc_pin);// MDC=0

	for(i=0; i< DELAY; i++);
	REG32(mdcdat_reg) = REG32(mdcdat_reg) | (1<<mdc_pin);	// MDC=1

	#endif

}

/* Change clock to 1 */
static void _smiZBit(void) 
{
	unsigned short i;

	//MDC=output MDIO = input
	REG32(mdiodir_reg) = REG32(mdiodir_reg) & ~(1<<mdio_pin);// MDIO=0, 
	REG32(mdcdir_reg) = REG32(mdcdir_reg) | (1<<mdc_pin);// MDC=1
	
	//MDC = 0
	REG32(mdcdat_reg) = (REG32(mdcdat_reg) & ~(1<<mdc_pin)); //MDC=0
	for(i=0; i< DELAY; i++);
}

/* Generate  1 -> 0 transition and sampled at 1 to 0 transition time,
should not sample at 0->1 transition because some chips stop outputing
at the last bit at rising edge*/

static void _smiReadBit(unsigned short * pdata) 
{
	unsigned short i;

	//MDC = output, MDIO = input
	REG32(mdiodir_reg) = REG32(mdiodir_reg) & ~(1<<mdio_pin); //MDIO=input
	REG32(mdcdir_reg) = REG32(mdcdir_reg)| (1<<mdc_pin); //MDC=outut:1

	//MDC = 1
	REG32(mdcdat_reg) = REG32(mdcdat_reg) | (1<<mdc_pin); //MDC=1
	for(i=0; i< DELAY; i++);
	//MDC = 0 MDC = output
	REG32(mdcdat_reg) = REG32(mdcdat_reg) & ~(1<<mdc_pin); //MDC=0
	//MDIO read data
	*pdata = (REG32(mdiodat_reg) & (1<<mdio_pin))?1:0;  //check MDIO data
}

/* Generate  0 -> 1 transition and put data ready during 0 to 1 whole period */
static void _smiWriteBit(unsigned short data) 
{
	unsigned short i;

	//MDC = output, MDIO = output
	REG32(mdiodir_reg) = REG32(mdiodir_reg) | (1<<mdio_pin);	//output pin; // MDIO = 1
	REG32(mdcdir_reg) = REG32(mdcdir_reg) | (1<<mdc_pin);	//output pin  // MDC=1

	if(data) {/* Write 1 */
		//MDIO = 1, MDC=0
		REG32(mdiodat_reg) = REG32(mdiodat_reg) | (1<<mdio_pin); //MDIO = 1, MDC=0
		REG32(mdcdat_reg) = REG32(mdcdat_reg) & ~(1<<mdc_pin); //MDC=0

		for(i=0; i< DELAY; i++);
		//MDIO=1 MDC=1
		REG32(mdiodat_reg) = REG32(mdiodat_reg) | (1<<mdio_pin); //MDIO=1
		REG32(mdcdat_reg) = REG32(mdcdat_reg) | (1<<mdc_pin); //MDC=1

	} else {
		//MDC = 0, MDIO = 0
		REG32(mdiodat_reg) = (REG32(mdiodat_reg) & ~(1<<mdio_pin)); //MDIO = 0
		REG32(mdcdat_reg) = (REG32(mdcdat_reg) & ~(1<<mdc_pin)); //MDC=0
				
		for(i=0; i< DELAY; i++);
		//MDC = 1
		REG32(mdiodat_reg) = (REG32(mdiodat_reg) & ~(1<<mdio_pin)); //MDIO=0
		REG32(mdcdat_reg) = REG32(mdcdat_reg) | (1<<mdc_pin); //MDC=1
	}
}

int smiRead(unsigned int phyad, unsigned int regad, unsigned int * data) 
{
	int i;
	unsigned short readBit;

	/* 32 continuous 1 as preamble*/
	for(i=0; i<32; i++)
		_smiWriteBit(1);
	/* ST: Start of Frame, <01>*/
	_smiWriteBit(0);
	_smiWriteBit(1);
	/* OP: Operation code, read is <10> */
	_smiWriteBit(1);
	_smiWriteBit(0);
	/* PHY Address */
	for(i=4; i>=0; i--) 
		_smiWriteBit((phyad>>i)&0x1);
	/* Register Address */
	for(i=4; i>=0; i--) 
		_smiWriteBit((regad>>i)&0x1);
	/* TA: Turnaround <z0> */
	_smiZBit();
	_smiReadBit(&readBit);
	/* Data */
	*data = 0;
	for(i=15; i>=0; i--) {
		_smiReadBit(&readBit);
		*data = (*data<<1) | readBit;
	}
        /*add  an extra clock cycles for robust reading , ensure partner stop output signal
        and not affect the next read operation, because TA steal a clock*/
	_smiWriteBit(1);
	_smiZBit();
    
	return 0;
}


int smiWrite(unsigned char phyad, unsigned int regad, unsigned int data) 
{
	int i;

	/* 32 continuous 1 as preamble*/
	for(i=0; i<32; i++)
		_smiWriteBit(1);
	/* ST: Start of Frame, <01>*/
	_smiWriteBit(0);
	_smiWriteBit(1);
	/* OP: Operation code, write is <01> */
	_smiWriteBit(0);
	_smiWriteBit(1);
	/* PHY Address */
	for(i=4; i>=0; i--) 
		_smiWriteBit((phyad>>i)&0x1);
	/* Register Address */
	for(i=4; i>=0; i--) 
		_smiWriteBit((regad>>i)&0x1);
	/* TA: Turnaround <10> */
	_smiWriteBit(1);
	_smiWriteBit(0);
	/* Data */
	for(i=15; i>=0; i--) 
		_smiWriteBit((data>>i)&0x1);
	_smiGenWriteClk();
	_smiZBit();

	return 0;
}
#endif //MDCIO_GPIO_SIMULATION
/*----------------------------------------------------------------------------*/


int rtl_mdio_write(unsigned int mdio_phyaddr,unsigned int reg, unsigned int data)
{
    unsigned long flags;

#ifdef MDCIO_GPIO_SIMULATION
    int ret;

    if (!dev_priv)
	    return 1;

    del_timer(&dev_priv->mdc_timer);
    spin_lock_irqsave(&mdio_lock, flags);
    ret = smiWrite(mdio_phyaddr, reg, data);
    spin_unlock_irqrestore(&mdio_lock, flags);
    mod_timer(&dev_priv->mdc_timer, jiffies + 1);
    return ret;
#endif
}

#if 0
int rtl_mdio_mask_write(unsigned short reg, unsigned short mask, unsigned short data)
{
    unsigned long flags;
#ifdef MDCIO_GPIO_SIMULATION
    unsigned short regval;
    int ret;

    del_timer(&dev_priv->mdc_timer);
    spin_lock_irqsave(&mdio_lock, flags);
    ret = smiRead(mdio_phyaddr, reg, &regval);
    if (ret == 0) {
    	data = (regval&(~mask)) | (data&mask);
        ret = smiWrite(mdio_phyaddr, reg, data);
    }
    spin_unlock_irqrestore(&mdio_lock, flags);
    mod_timer(&dev_priv->mdc_timer, jiffies + 1);
    return ret;
#endif
}
#endif

int rtl_mdio_read(unsigned int mdio_phyaddr, unsigned int reg, unsigned int *pdata)
{
    unsigned long flags;

#ifdef MDCIO_GPIO_SIMULATION
    int ret;

    if (!dev_priv)
	    return 1;

    del_timer(&dev_priv->mdc_timer);
    spin_lock_irqsave(&mdio_lock, flags);
    ret = smiRead(mdio_phyaddr, reg, pdata);
    spin_unlock_irqrestore(&mdio_lock, flags);
    mod_timer(&dev_priv->mdc_timer, jiffies + 1);
    return ret;
#endif
}

#ifdef MDCIO_GPIO_SIMULATION
static void mdc_timer_fn(unsigned long arg)
{
    _smiWriteBit(1); // generate MDC clock
    mod_timer(&dev_priv->mdc_timer, jiffies + 1);
}
#endif


int nfbi_init(void)
{
#ifdef MDCIO_GPIO_SIMULATION
//    _smiGpioInit();
	gpio_request_one(MDC_PIN, GPIOF_DIR_OUT | GPIOF_EXPORT_DIR_CHANGEABLE, "mdc pin"); 
	gpio_request_one(MDIO_PIN, GPIOF_DIR_OUT | GPIOF_EXPORT_DIR_CHANGEABLE, "mdio pin");

	mdc_pin = MDC_PIN;
	mdio_pin = MDIO_PIN;

	if(mdc_pin >= BSP_GPIO_2ND_REG){
		mdc_pin -= BSP_GPIO_2ND_REG;
		mdcdir_reg = PEFGHDIR_REG;
		mdcdat_reg = PEFGHDAT_REG;
	}
	
	if(mdio_pin >= BSP_GPIO_2ND_REG){
		mdio_pin -= BSP_GPIO_2ND_REG;
		mdiodir_reg = PEFGHDIR_REG;
		mdiodat_reg = PEFGHDAT_REG;
	}
#endif

	dev_priv = (struct nfbi_dev_priv *)kmalloc(sizeof (struct nfbi_dev_priv), GFP_KERNEL);
	if(!dev_priv) {
	    printk(KERN_ERR DRIVER_NAME": unable to kmalloc for nfbi_dev_priv\n");
		return -ENOMEM;
	}
	memset((void *)dev_priv, 0, sizeof (struct nfbi_dev_priv));

#ifdef MDCIO_GPIO_SIMULATION
    init_timer(&dev_priv->mdc_timer);
    dev_priv->mdc_timer.data = 0;
    dev_priv->mdc_timer.function = mdc_timer_fn;
    dev_priv->mdc_timer.expires = jiffies + 1; /* in jiffies */
    add_timer(&dev_priv->mdc_timer);
#endif
	return 0;
}

int nfbi_close(void)
{
        if(!dev_priv) {
                return 1;
        }

        del_timer(&dev_priv->mdc_timer);
        kfree(dev_priv);
        dev_priv = NULL;
        gpio_free(MDC_PIN);
        gpio_free(MDIO_PIN);

        return 0;
}


MODULE_AUTHOR("Michael Lo");
MODULE_DESCRIPTION("Driver for RTL8197B NFBI");
MODULE_LICENSE("none-GPL");

