#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <asm/unistd.h>
#include <linux/gpio.h>
#elif defined(__ECOS)
#include <cyg/io/eth/rltk/819x/wrapper/sys_support.h>
#include <cyg/io/eth/rltk/819x/wrapper/skbuff.h>
#include <cyg/io/eth/rltk/819x/wrapper/timer.h>
#include <cyg/io/eth/rltk/819x/wrapper/wrapper.h>
#endif

#include "./8192cd_cfg.h"
#include "./8192cd.h"
#include "./8192cd_hw.h"
#include "./8192cd_headers.h"
#include "./8192cd_debug.h"
#include "./8192cd_pwrctrl.h"
#ifdef CONFIG_RTL_88E_SUPPORT
#include "Hal8188EPwrSeq.h"
#endif
#ifdef CONFIG_RTL_8723B_SUPPORT
#include "Hal8723BPwrSeq.h"
#endif

#ifdef CONFIG_RTL_8812_SUPPORT
#include "Hal8812PwrSeq.h"
#endif

#ifdef CONFIG_WLAN_HAL_8814AE
#include "./phydm/rtl8814a/phydm_rtl8814a.h"
#endif

#if defined(CONFIG_WLAN_HAL_8197F) && defined(CONFIG_SOC_ENABLE_EFUSE)
#include "./efuse.h"
#endif

#ifdef __KERNEL__
#ifdef __LINUX_2_6__
#include <linux/syscalls.h>
#else
#include <linux/fs.h>
#endif
#endif

#if defined(USE_RTL8186_SDK) && !defined(__OSK__)
#ifdef CONFIG_RTL8672
	#ifdef USE_RLX_BSP
	#include <bspchip.h>	
	#ifdef __LINUX_3_10__
	#include <bspgpio.h>
	#else
	#include <gpio.h>
	#endif

	#ifdef CONFIG_RTL_8196C
	#undef CONFIG_RTL_8196C
	#endif
	#ifdef CONFIG_RTL8196C_REVISION_B
	#undef CONFIG_RTL8196C_REVISION_B
	#endif

	#else
	#include <platform.h>
	#include "../../../arch/mips/realtek/rtl8672/gpio.h"
	#endif
#else

#if defined(__LINUX_2_6__)
#if (defined(CONFIG_OPENWRT_SDK) && !defined(CONFIG_ARCH_CPU_RLX)) || defined(CONFIG_RTL_8197F)
#include <bspchip.h>
#elif defined(CONFIG_RTL_8198F) 
#else
#include <bsp/bspchip.h>
#endif //CONFIG_OPENWRT_SDK
#else
#ifndef __ECOS
#include <asm/rtl865x/platform.h>
#endif
#endif
#endif

#endif // USE_RTL8186_SDK

#if defined(TAROKO_0)
#include "WlanHAL/HalPrecomp.h"
#endif



#if defined(CONFIG_PCI_HCI)
#if defined(CONFIG_WLAN_HAL)
static inline u32 __getAvailableTXBD_HAL(
	struct rtl8192cd_priv   *priv,
	u32						qNum)
{
	PHCI_TX_DMA_MANAGER_88XX    ptx_dma = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PTxDMA88XX);
	u32			                halQnum = GET_HAL_INTERFACE(priv)->MappingTxQueueHandler(priv, qNum);

	return ptx_dma->tx_queue[halQnum].avail_txbd_num;
}
#endif /* CONFIG_WLAN_HAL */

static inline u32 __getAvailableTXBD_nonHAL(
	struct rtl8192cd_priv	*priv,
	u32            			qNum)
{
	int *tx_head, *tx_tail;
	u32 total_size;

	tx_head = get_txhead_addr(priv->pshare->phw, qNum);
	tx_tail = get_txtail_addr(priv->pshare->phw, qNum);

#ifdef OSK_LOW_TX_DESC
	total_size = (qNum == BE_QUEUE || qNum == HIGH_QUEUE) ? BE_TXDESC : NONBE_TXDESC;
#else
	total_size = CURRENT_NUM_TX_DESC;
#endif

	return CIRC_SPACE_RTK(*tx_head, *tx_tail, total_size);
}

u32 getAvailableTXBD(
	struct rtl8192cd_priv   *priv,
	u32						qNum)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif
	u32 ret = 0;
	SAVE_INT_AND_CLI(flags);

#ifdef CONFIG_WLAN_HAL
	if (IS_HAL_CHIP(priv)) {
		ret = __getAvailableTXBD_HAL(priv, qNum);
	}
	else 
#endif
	{
		ret = __getAvailableTXBD_nonHAL(priv, qNum);
	}

	RESTORE_INT(flags);
	return ret;
}
#endif /* CONFIG_PCI_HCI */



#if defined(SUPPORT_TX_AMSDU) || defined (P2P_SUPPORT)
#if defined(TAROKO_0)
//TODO , port HW timer to SW timer
void setup_timer2(struct rtl8192cd_priv *priv, unsigned int timeout)
{

}
void cancel_timer2(struct rtl8192cd_priv *priv)
{

}

#else
void setup_timer2(struct rtl8192cd_priv *priv, unsigned int timeout)
{
	unsigned int current_value;

	if(!priv->pshare->amsdu_use_hw_timer) {
		mod_timer(&priv->pshare->amsdu_sw_timer, (unsigned long)timeout);	
		return;
	}

#ifdef CONFIG_WLAN_HAL
    if ( IS_HAL_CHIP(priv) ) {
        GET_HAL_INTERFACE(priv)->GetHwRegHandler(priv, HW_VAR_TSF_TIMER, (pu1Byte)&current_value);
    } else
#endif
    {
        current_value = RTL_R32(TSFTR1);
    }

	if (TSF_LESS(timeout, current_value))
		timeout = current_value + 20;

	if (TSF_DIFF(current_value,priv->pshare->timer2_last_tsf) <= TIMER2_HWTIMER_DELAY)
    {
		DEBUG_WARN("TIMER2 do not set too quickly (%d,%d)\n", current_value, priv->pshare->timer2_last_tsf);
	}
	else
	{
		priv->pshare->timer2_last_tsf = current_value;

#ifdef CONFIG_WLAN_HAL
		if ( IS_HAL_CHIP(priv) ) {
			GET_HAL_INTERFACE(priv)->SetHwRegHandler(priv, HW_VAR_PS_TIMER, (pu1Byte)&timeout);
		} else 
#endif
		{
			RTL_W32(TIMER1, timeout);
		}
	}

#ifdef CONFIG_WLAN_HAL
	if ( IS_HAL_CHIP(priv) ) {
		RTL_W8((REG_HIMR0+3), RTL_R8(REG_HIMR0+3) | (BIT_PSTIMEOUT>>24));
	} else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif //CONFIG_WLAN_HAL
	{//not HAL
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
		if (GET_CHIP_VER(priv) == VERSION_8812E || GET_CHIP_VER(priv) == VERSION_8723B)
			RTL_W32(REG_HIMR0_8812,	RTL_R32(REG_HIMR0_8812) | IMR_TIMER2_8812);
		else
#endif
			RTL_W32(HIMR, RTL_R32(HIMR) | HIMR_TIMEOUT2);
	}
}


void cancel_timer2(struct rtl8192cd_priv *priv)
{
	if(!priv->pshare->amsdu_use_hw_timer) {
		if (timer_pending(&priv->pshare->amsdu_sw_timer))
			del_timer(&priv->pshare->amsdu_sw_timer);
		return;
	}

#ifdef CONFIG_WLAN_HAL
	if ( IS_HAL_CHIP(priv) ) {
		RTL_W8(REG_HIMR0+3, RTL_R8(REG_HIMR0+3) & ~ (BIT_PSTIMEOUT>>24));
	} else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif //CONFIG_WLAN_HAL    
	{//not HAL
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
		if (GET_CHIP_VER(priv) == VERSION_8812E || GET_CHIP_VER(priv) == VERSION_8723B)
			RTL_W32(REG_HIMR0_8812, RTL_R32(REG_HIMR0_8812) & ~IMR_TIMER2_8812);
		else
#endif
			RTL_W32(HIMR, RTL_R32(HIMR) & ~HIMR_TIMEOUT2);
	}
}
#endif
#endif

