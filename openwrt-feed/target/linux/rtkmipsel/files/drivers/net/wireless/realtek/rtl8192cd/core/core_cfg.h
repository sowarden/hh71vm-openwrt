#ifndef	_CORE_CFG_H_
#define _CORE_CFG_H_

#if defined(CONFIG_RTL_ULINKER_BRSC)
#include "linux/ulinker_brsc.h"
#endif

#ifdef CONFIG_ARCH_RTL8198F
#define USE_RTL8186_SDK
// CN2 also use the exact name "CONFIG_RTL_MULTI_LAN_DEV"
#define CONFIG_RTL_8198F
#if defined(TAROKO_0)
#else
#define CONFIG_ENABLE_CCI400
#endif
#undef CONFIG_RTL_MULTI_LAN_DEV
#endif

#ifdef __OSK__
	#if !defined(CONFIG_RTL_819X) && !defined(CONFIG_RTL8672)
		#define CONFIG_RTL8672
	#endif
	#define USE_RTL8186_SDK
	#define CONFIG_RTL8196C
	#define CONFIG_RTL_8196C
#endif

//#define _LITTLE_ENDIAN_
//#define _BIG_ENDIAN_

//this is for WLAN HAL driver coexist with not HAL driver for code size reduce
#ifdef CONFIG_RTL_WLAN_HAL_NOT_EXIST
#define CONFIG_WLAN_NOT_HAL_EXIST 1
#else
#define CONFIG_WLAN_NOT_HAL_EXIST 0//96e_92e, 8881a_92e, 8881a_only, 96d_92er, is only HAL driver
#endif

#ifdef __ECOS
//#define CONFIG_RTL_WLAN_CONF_TXT_NOT_EXIST
#endif

#ifdef CONFIG_RTL_WLAN_CONF_TXT_NOT_EXIST
#define CONFIG_WLAN_CONF_TXT_EXIST 0
#else
#define CONFIG_WLAN_CONF_TXT_EXIST 1
#endif

#ifdef __MIPSEB__

#ifndef _BIG_ENDIAN_
	#define _BIG_ENDIAN_
#endif

#ifdef _LITTLE_ENDIAN_
#undef _LITTLE_ENDIAN_
#endif
//### add by sen_liu 2011.4.14 CONFIG_NET_PCI defined in V2.4 and CONFIG_PCI
// define now to replace it. However,some modules still use CONFIG_NET_PCI
#ifdef CONFIG_PCI
#define CONFIG_NET_PCI
#endif

//### end
#endif	//__MIPSEB__

#if defined(__MIPSEL__) || defined(__ARMEL__)
#ifndef _LITTLE_ENDIAN_
	#define _LITTLE_ENDIAN_
#endif

#ifdef _BIG_ENDIAN_
#undef _BIG_ENDIAN_
#endif

#endif //__MIPSEL__ or __ARMEL__

#ifdef __KERNEL__
#include <linux/version.h>

#if LINUX_VERSION_CODE >= 0x020614 // linux 2.6.20
	#define LINUX_2_6_20_
#endif

#if LINUX_VERSION_CODE >= 0x020615 // linux 2.6.21
	#define LINUX_2_6_21_
#endif

#if LINUX_VERSION_CODE >= 0x020616 // linux 2.6.22
	#define LINUX_2_6_22_
#endif

#if LINUX_VERSION_CODE >= 0x020618 // linux 2.6.24
	#define LINUX_2_6_24_
#endif

#if LINUX_VERSION_CODE >= 0x02061B // linux 2.6.27
	#define LINUX_2_6_27_
#endif

#if LINUX_VERSION_CODE >= 0x02061D // linux 2.6.29
	#define LINUX_2_6_29_
#endif

#if LINUX_VERSION_CODE > 0x020600
	#define __LINUX_2_6__
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
	#define __LINUX_3_2__
	#ifndef CONFIG_RTL_PROC_NEW
	#define CONFIG_RTL_PROC_NEW
	#endif
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0))
	#define __LINUX_3_4__
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
	#define __LINUX_3_10__
	#ifndef CONFIG_RTL_PROC_NEW
	#define CONFIG_RTL_PROC_NEW 
	#endif
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0)
	#define __LINUX_3_18__
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,21)
	#define __LINUX_3_18_21__
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,23)
	#define __LINUX_3_18_23__
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
	#define __LINUX_4_4__
#endif

#if defined(LINUX_2_6_20_) || defined(__LINUX_3_4__)
#ifdef CPTCFG_CFG80211_MODULE
#ifndef RTK_129X_PLATFORM
#ifdef __LINUX_3_18_23__
	#ifdef CONFIG_OPENWRT_SDK
#include "../../../../../linux-4.4.3/include/generated/autoconf.h"
	#else
#include "../../../../../linux-3.18.23/include/generated/autoconf.h"
	#endif
#elif defined(__LINUX_3_18_21__)
#include "../../../../../linux-3.18.21/include/generated/autoconf.h"
#elif (LINUX_VERSION_CODE == KERNEL_VERSION(3,10,49))
#include "../../../../../linux-3.10.49/include/generated/autoconf.h"
#endif
#endif
#else //CPTCFG_CFG80211_MODULE
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
#include <linux/kconfig.h>
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
#include <linux/autoconf.h>
#else
#include <generated/autoconf.h>
#endif
#endif //CPTCFG_CFG80211_MODULE
#include <linux/jiffies.h>
#include <asm/param.h>
#else
#include <linux/config.h>
#endif
#endif // __KERNEL__

//-------------------------------------------------------------
// Type definition
//-------------------------------------------------------------
#include "typedef.h"

#ifdef __ECOS
	#include <pkgconf/system.h>
	#include <pkgconf/devs_eth_rltk_819x_wrapper.h>
	#include <pkgconf/devs_eth_rltk_819x_wlan.h>
	#include <sys/param.h>
#ifndef RTLPKG_DEVS_ETH_RLTK_819X_RX_ZERO_COPY
    #define CONFIG_RTL8190_PRIV_SKB
#endif
#endif
#ifdef LINUX_2_6_29_ // linux 2.6.29
#define NETDEV_NO_PRIV
#endif

#define SWQ_TIMER_NUM	(NUM_STAT*4)
#define SWQ_HWTIMER_TOLERANCE	500 // us
#define SWQ_HWTIMER_MINIMUN	1000 // us
#define SWQ_HWTIMER_MAXIMUN	500000 // us
#define SWQ_HWTIMER_DELAY	34 // us
#define SWQ_TIMEOUT_THD  300 // ms
#define SWQ_TIMEOUT_THD_LOWER  3 // count

#define RC_ENTRY_NUM		128
#define RC_TIMER_NUM		64

#define GKEY_ID_SECOND 		2
#define AMSDU_TIMER_NUM		64
#define TUPLE_WINDOW		128
#define NUM_TXPKT_QUEUE		64
#define NUM_APSD_TXPKT_QUEUE	32
#define GBWC_TO_MILISECS	250
#define TX_SC_ENTRY_NUM		4
#define RX_SC_ENTRY_NUM		4
#define MAX_FRAG_COUNT		16

// for counting association number
#define INCREASE		1
#define DECREASE		0

#define NUM_RX_DESC_IF(priv)  (priv->pshare->wlandev_idx == (0^WLANIDX)?NUM_RX_DESC:NUM_RX_DESC_2G)
#define CURRENT_NUM_TX_DESC	priv->pshare->current_num_tx_desc


#ifdef OSK_LOW_TX_DESC
	#define BE_TXDESC			(NUM_TX_DESC)
	#define NONBE_TXDESC		(BE_TXDESC>>2)
	#define NUM_TX_DESC_HQ		(NUM_TX_DESC>>3)
#else
	#define BE_TXDESC			(NUM_TX_DESC)
	#define NONBE_TXDESC		(NUM_TX_DESC)
	#define NUM_TX_DESC_HQ		(NUM_TX_DESC>>3)
#endif

//This is for LUNA SDK - Apollo to config 8812 in slave CPU and shift mem 33M
#if defined(CONFIG_ARCH_LUNA_SLAVE) && !defined(CONFIG_WLAN_HAL)
#define CONFIG_LUNA_SLAVE_PHYMEM_OFFSET CONFIG_RTL8686_DSP_MEM_BASE
#else
#define CONFIG_LUNA_SLAVE_PHYMEM_OFFSET 0x0
#endif

//-------------------------------------------------------------
// Refine for multi-station
//-------------------------------------------------------------
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL) 
#define MULTI_STA_REFINE
#endif
#ifdef MULTI_STA_REFINE
#define PKTAGINGTIME 1200
#define MAXPAUSEDSTA 24
#define MAXPAUSEDQUEUE 7
#define LOWAGGRESTA	8
#define STA_TX_LOWRATE ((txcfg->pstat->ht_cap_buf.support_mcs[2])? _MCS8_RATE_:_MCS4_RATE_)
#endif


//-------------------------------------------------------------
// to prevent broadcast storm attacks
//-------------------------------------------------------------
#define PREVENT_BROADCAST_STORM	1

//-------------------------------------------------------------
// to prevent ARP spoofing attacks
//-------------------------------------------------------------
#ifdef PREVENT_BROADCAST_STORM
/*
 *	NOTE: The driver will skip the other broadcast packets if the system free memory is less than FREE_MEM_LOWER_BOUND 
 *		   and the broadcast packet amount is larger than BROADCAST_STORM_THRESHOLD in one second period.
 */

#define BROADCAST_STORM_THRESHOLD		50 //16
#define FREE_MEM_LOWER_BOUND			800 //uint: KBytes
#endif

#endif
