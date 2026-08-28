/*
 *  Header file defines some common inline funtions
 *
 *  Copyright (c) 2017 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef _8192CD_UTIL_H_
#define _8192CD_UTIL_H_

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/circ_buf.h>
#include <asm/io.h>
#endif
#ifdef __KERNEL__
#include <asm/cacheflush.h>
#endif
#include "./8192cd_cfg.h"
#include "./8192cd.h"
#include "./wifi.h"
#include "./8192cd_hw.h"

#if !defined(NOT_RTK_BSP)
#if defined(__LINUX_2_6__)
//#include <bsp/bspchip.h>
#else
#if !defined(__ECOS) && !defined(__OSK__)
	#include <asm/rtl865x/platform.h>
#endif	
#endif
#endif

#ifdef CONFIG_RTK_MESH
#include "./mesh_ext/mesh_util.h"
#endif

#ifdef CONFIG_USB_HCI
#ifdef CONFIG_RTL_88E_SUPPORT
#include "./usb/8188eu/8192cd_usb.h"
#endif

#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
  #if defined(CONFIG_RTL_92E_SUPPORT)
    #include "./usb/8192eu/8192cd_usb.h"
  #elif defined(CONFIG_RTL_92F_SUPPORT)
    #include "./usb/8192fu/8192cd_usb.h"
  #else
    #error "triband undefined!!"
  #endif
#endif /* defined(CONFIG_RTL_TRIBAND_SUPPORT) */
#endif

#ifdef CONFIG_SDIO_HCI
#ifdef CONFIG_RTL_88E_SUPPORT
#include "./sdio/8189es/8188e_sdio.h"
#endif
#ifdef CONFIG_WLAN_HAL_8192EE
#include "./sdio/8192es/8192e_sdio.h"
#endif
#endif

#ifdef RTK_129X_PLATFORM
#ifdef CONFIG_RTK_SW_LOCK_API
#include <soc/realtek/rtd129x_lockapi.h>
#endif
#endif

#include "./core/8192cd_core_util.h"

#define CTRL_LEN_CHECK(__x__,__y__) \
	do { \
		if((__x__ < __y__) || (__y__ < 0)) { \
			panic_printk("!!! error [%s][%d] len=%d \n",__FUNCTION__, __LINE__, __y__); \
		} \
	} while(0)

#if defined(CONFIG_RTL_TRIBAND_SUPPORT) && defined(SMP_SYNC)
#define _8192CD_TRIBNAD_UTIL_LOCK_H_
#include "8192cd_triband.h"
#undef _8192CD_TRIBNAD_UTIL_LOCK_H_

#else //!CONFIG_RTL_TRIBAND_SUPPORT


#ifdef GREEN_HILL
#define	SAVE_INT_AND_CLI(x)		{ x = save_and_cli(); }
#define RESTORE_INT(x)			restore_flags(x)
#define SMP_LOCK(__x__)	
#define SMP_UNLOCK(__x__)
#define SMP_TRY_LOCK(__x__,__y__)
#define SMP_LOCK_XMIT(__x__)		
#define SMP_UNLOCK_XMIT(__x__)
#define SMP_LOCK_GLOBAL(__x__)
#define SMP_UNLOCK_GLOBAL(__x__)		
#define SMP_LOCK_ACL(__x__)
#define SMP_UNLOCK_ACL(__x__)
#define SMP_LOCK_HASH_LIST(__x__)
#define SMP_UNLOCK_HASH_LIST(__x__)
#define SMP_LOCK_ASOC_LIST(__x__)
#define SMP_UNLOCK_ASOC_LIST(__x__)
#define SMP_TRY_LOCK_ASOC_LIST(__x__, __y__)
#define SMP_LOCK_SLEEP_LIST(__x__)
#define SMP_UNLOCK_SLEEP_LIST(__x__)
#define SMP_LOCK_AUTH_LIST(__x__)
#define SMP_UNLOCK_AUTH_LIST(__x__)
#define SMP_LOCK_WAKEUP_LIST(__x__)
#define SMP_UNLOCK_WAKEUP_LIST(__x__)
#define SMP_LOCK_STACONTROL_LIST(__X__)
#define SMP_UNLOCK_STACONTROL_LIST(__X__)
#ifdef AP_NEIGHBOR_INFO
#define SMP_LOCK_AP_NEIGHBOR_LIST(__x__)  
#define SMP_UNLOCK_AP_NEIGHBOR_LIST(__x__)
#endif
#define SMP_LOCK_TRANSITION_LIST(__X__)
#define SMP_UNLOCK_TRANSITION_LIST(__X__)
#define SMP_LOCK_WAITING_LIST(__X__);
#define SMP_UNLOCK_WAITING_LIST(__X__);
#define SMP_LOCK_BLOCK_LIST(__X__);
#define SMP_UNLOCK_BLOCK_LIST(__X__);
#define SMP_LOCK_SKB(__x__)			
#define SMP_UNLOCK_SKB(__x__)
#define SMP_LOCK_BUF(__x__)			
#define SMP_UNLOCK_BUF(__x__)	
#define SMP_LOCK_RECV(__x__)
#define SMP_UNLOCK_RECV(__x__)
#ifdef RTK_129X_PLATFORM
#define SMP_LOCK_IO_129X(__x__)
#define SMP_UNLOCK_IO_129X(__x__)
#endif
#define SMP_LOCK_RX_DATA(__x__)
#define SMP_UNLOCK_RX_DATA(__x__)
#define SMP_LOCK_RX_MGT(__x__)
#define SMP_UNLOCK_RX_MGT(__x__)
#define SMP_LOCK_RX_CTRL(__x__)
#define SMP_UNLOCK_RX_CTRL(__x__)
#define SMP_LOCK_REORDER_CTRL(__x__)
#define SMP_UNLOCK_REORDER_CTRL(__x__)
#define SMP_TRY_LOCK_REORDER_CTRL(__x__,__y__)
#define DEFRAG_LOCK(__x__)			{ x = save_and_cli(); }
#define DEFRAG_UNLOCK(__x__)		restore_flags(x)
#define SMP_LOCK_PSK_RESEND(__x__)
#define SMP_UNLOCK_PSK_RESEND(__x__)
#define SMP_LOCK_PSK_GKREKEY(__x__)
#define SMP_UNLOCK_PSK_GKREKEY(__x__)
#define SMP_LOCK_ASSERT()

#ifdef CONFIG_IEEE80211R
#define SMP_LOCK_FT_R0KH(__x__)	
#define SMP_UNLOCK_FT_R0KH(__x__)
#define SMP_TRY_LOCK_FT_R0KH(__x__,__y__)	

#define SMP_LOCK_FT_R1KH(__x__)
#define SMP_UNLOCK_FT_R1KH(__x__)
#define SMP_TRY_LOCK_FT_R1KH(__x__,__y__)
#endif

#ifdef SBWC
#define SMP_LOCK_SBWC(__x__)
#define SMP_UNLOCK_SBWC(__x__)
#define SMP_TRY_LOCK_SBWC(__x__,__y__)
#endif

#elif defined(__OSK__)
#define SAVE_INT_AND_CLI(x)		{ x=lx4180_ReadStatus();lx4180_WriteStatus(x&(~1)); }
#define RESTORE_INT(x)			{ lx4180_WriteStatus(x); }

#define SMP_LOCK(__x__)	
#define SMP_UNLOCK(__x__)
#define SMP_TRY_LOCK(__x__,__y__)
#define SMP_LOCK_XMIT(__x__)		
#define SMP_UNLOCK_XMIT(__x__)	
#define SMP_LOCK_GLOBAL(__x__)
#define SMP_UNLOCK_GLOBAL(__x__)	
#define SMP_LOCK_ACL(__x__)
#define SMP_UNLOCK_ACL(__x__)
#define SMP_LOCK_HASH_LIST(__x__)
#define SMP_UNLOCK_HASH_LIST(__x__)
#define SMP_LOCK_ASOC_LIST(__x__)
#define SMP_UNLOCK_ASOC_LIST(__x__)
#define SMP_TRY_LOCK_ASOC_LIST(__x__, __y__)
#define SMP_LOCK_SLEEP_LIST(__x__)
#define SMP_UNLOCK_SLEEP_LIST(__x__)
#define SMP_LOCK_AUTH_LIST(__x__)
#define SMP_UNLOCK_AUTH_LIST(__x__)
#define SMP_LOCK_WAKEUP_LIST(__x__)
#define SMP_UNLOCK_WAKEUP_LIST(__x__)
#define SMP_LOCK_MESH_ACL(__x__)
#define SMP_UNLOCK_MESH_ACL(__x__)
#define SMP_LOCK_MESH_MP_HDR(__X__)
#define SMP_UNLOCK_MESH_MP_HDR(__X__)
#define SMP_LOCK_SKB(__x__)			
#define SMP_UNLOCK_SKB(__x__)
#define SMP_LOCK_BUF(__x__)			
#define SMP_UNLOCK_BUF(__x__)	
#define SMP_LOCK_RECV(__x__)
#define SMP_UNLOCK_RECV(__x__)
#define SMP_LOCK_RX_DATA(__x__)
#define SMP_UNLOCK_RX_DATA(__x__)
#define SMP_LOCK_RX_MGT(__x__)
#define SMP_UNLOCK_RX_MGT(__x__)
#define SMP_LOCK_RX_CTRL(__x__)
#define SMP_UNLOCK_RX_CTRL(__x__)
#define SMP_LOCK_REORDER_CTRL(__x__)
#define SMP_UNLOCK_REORDER_CTRL(__x__)
#define SMP_TRY_LOCK_REORDER_CTRL(__x__,__y__)
#define DEFRAG_LOCK(x)			{ x=lx4180_ReadStatus();lx4180_WriteStatus(x&(~1)); }
#define DEFRAG_UNLOCK(x)		{ lx4180_WriteStatus(x); }
#define SMP_LOCK_PSK_RESEND(__x__)
#define SMP_UNLOCK_PSK_RESEND(__x__)
#define SMP_LOCK_PSK_GKREKEY(__x__)
#define SMP_UNLOCK_PSK_GKREKEY(__x__)
#define SMP_LOCK_ASSERT()

#ifdef CONFIG_IEEE80211R
#define SMP_LOCK_FT_R0KH(__x__)	
#define SMP_UNLOCK_FT_R0KH(__x__)
#define SMP_TRY_LOCK_FT_R0KH(__x__,__y__)

#define SMP_LOCK_FT_R1KH(__x__)
#define SMP_UNLOCK_FT_R1KH(__x__)
#define SMP_TRY_LOCK_FT_R1KH(__x__,__y__)
#endif

#ifdef SBWC
#define SMP_LOCK_SBWC(__x__)
#define SMP_UNLOCK_SBWC(__x__)
#define SMP_TRY_LOCK_SBWC(__x__,__y__)
#endif

#elif defined(SMP_SYNC) /*Add these spin locks to avoid deadlock under SMP platforms.*/
#define SAVE_INT_AND_CLI(__x__)		do { } while (0)
#define RESTORE_INT(__x__)			do { } while (0)
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
#define SMP_LOCK(__x__)				do { } while (0)
#define SMP_UNLOCK(__x__)			do { } while (0)
#define SMP_TRY_LOCK(__x__,__y__)	do { } while (0)
#define SMP_LOCK_ASSERT()			do { } while (0)
#define SMP_LOCK_XMIT(__x__)			do { } while (0)
#define SMP_UNLOCK_XMIT(__x__)			do { } while (0)
#define SMP_LOCK_GLOBAL(__x__)
#define SMP_UNLOCK_GLOBAL(__x__)
#define SMP_LOCK_STACONTROL_LIST(__x__)  do { } while (0)
#define SMP_UNLOCK_STACONTROL_LIST(__x__) do { } while (0)
#ifdef AP_NEIGHBOR_INFO
#define SMP_LOCK_AP_NEIGHBOR_LIST(__x__)  do { } while (0)
#define SMP_UNLOCK_AP_NEIGHBOR_LIST(__x__)	do { } while (0)
#endif
#define SMP_LOCK_TRANSITION_LIST(__x__)  do { } while (0)
#define SMP_UNLOCK_TRANSITION_LIST(__x__)  do { } while (0)
#define SMP_LOCK_WAITING_LIST(__X__);  do { } while (0)
#define SMP_UNLOCK_WAITING_LIST(__X__);  do { } while (0)
#define SMP_LOCK_BLOCK_LIST(__X__);  do { } while (0)
#define SMP_UNLOCK_BLOCK_LIST(__X__);  do { } while (0)
#define SMP_LOCK_MBSSID(__x__)			_enter_critical_mutex(&priv->pshare->mbssid_lock, (__x__))
#define SMP_UNLOCK_MBSSID(__x__)		_exit_critical_mutex(&priv->pshare->mbssid_lock, (__x__))
#define SMP_LOCK_ACL(__x__)				do { spin_lock(&priv->wlan_acl_list_lock); (void)(__x__); } while (0)
#define SMP_UNLOCK_ACL(__x__)			do { spin_unlock(&priv->wlan_acl_list_lock); (void)(__x__); } while (0)
#ifdef __ECOS
#define SMP_LOCK_HASH_LIST(__x__)		_enter_critical_mutex(&priv->hash_list_lock, (__x__))
#define SMP_UNLOCK_HASH_LIST(__x__)		_exit_critical_mutex(&priv->hash_list_lock, (__x__))
#define SMP_LOCK_ASOC_LIST(__x__)		_enter_critical_mutex(&priv->asoc_list_lock, (__x__))
#define SMP_UNLOCK_ASOC_LIST(__x__)		_exit_critical_mutex(&priv->asoc_list_lock, (__x__))
#define SMP_TRY_LOCK_ASOC_LIST(__x__, __y__)	_enter_critical_mutex(&priv->asoc_list_lock, (__x__))
#define SMP_LOCK_AUTH_LIST(__x__)		_enter_critical_mutex(&priv->auth_list_lock, (__x__))
#define SMP_UNLOCK_AUTH_LIST(__x__)		_exit_critical_mutex(&priv->auth_list_lock, (__x__))
#define SMP_LOCK_SLEEP_LIST(__x__)		_enter_critical_mutex(&priv->sleep_list_lock, (__x__))
#define SMP_UNLOCK_SLEEP_LIST(__x__)		_exit_critical_mutex(&priv->sleep_list_lock, (__x__))
#define SMP_LOCK_WAKEUP_LIST(__x__)		_enter_critical_mutex(&priv->wakeup_list_lock, (__x__))
#define SMP_UNLOCK_WAKEUP_LIST(__x__)	_exit_critical_mutex(&priv->wakeup_list_lock, (__x__))
#else
#define SMP_LOCK_HASH_LIST(__x__)		do { spin_lock_bh(&priv->hash_list_lock); (void)(__x__); } while (0)
#define SMP_UNLOCK_HASH_LIST(__x__)	do { spin_unlock_bh(&priv->hash_list_lock); (void)(__x__); } while (0)
#define SMP_LOCK_ASOC_LIST(__x__)		do { spin_lock_bh(&priv->asoc_list_lock); (void)(__x__); } while (0)
#define SMP_UNLOCK_ASOC_LIST(__x__)	do { spin_unlock_bh(&priv->asoc_list_lock); (void)(__x__); } while (0)
#define SMP_TRY_LOCK_ASOC_LIST(__x__, __y__)	do { spin_lock_bh(&priv->asoc_list_lock); (void)(__x__); } while (0)
#define SMP_LOCK_AUTH_LIST(__x__)			do { spin_lock_bh(&priv->auth_list_lock); (void)(__x__); } while (0)
#define SMP_UNLOCK_AUTH_LIST(__x__)			do { spin_unlock_bh(&priv->auth_list_lock); (void)(__x__); } while (0)
#define SMP_LOCK_SLEEP_LIST(__x__)			do { spin_lock_bh(&priv->sleep_list_lock); (void)(__x__); } while (0)
#define SMP_UNLOCK_SLEEP_LIST(__x__)			do { spin_unlock_bh(&priv->sleep_list_lock); (void)(__x__); } while (0)
#define SMP_LOCK_WAKEUP_LIST(__x__)			do { spin_lock_bh(&priv->wakeup_list_lock); (void)(__x__); } while (0)
#define SMP_UNLOCK_WAKEUP_LIST(__x__)			do { spin_unlock_bh(&priv->wakeup_list_lock); (void)(__x__); } while (0)
#endif
#define SMP_LOCK_MESH_MP_HDR(__X__)			do { spin_lock_bh(&priv->mesh_mp_hdr_lock); (void)(__x__); } while (0)
#define SMP_UNLOCK_MESH_MP_HDR(__X__)			do { spin_unlock_bh(&priv->mesh_mp_hdr_lock); (void)(__x__); } while (0)
#define SMP_LOCK_MESH_ACL(__x__)		do { spin_lock(&priv->mesh_acl_list_lock); (void)(__x__); } while (0)
#define SMP_UNLOCK_MESH_ACL(__x__)		do { spin_unlock(&priv->mesh_acl_list_lock); (void)(__x__); } while (0)
#elif defined(CONFIG_PCI_HCI)
#if 0   // 0: show debugging message while recursion is detected. 1: disable debugging message
#define SMP_LOCK(__x__) spin_lock_irqsave(&priv->pshare->lock, priv->pshare->lock_flags);
#define SMP_UNLOCK(__x__) spin_unlock_irqrestore(&priv->pshare->lock, priv->pshare->lock_flags);
#define SMP_TRY_LOCK(__x__,__y__) SMP_LOCK(__x__);__y__ = 1;
#define SMP_LOCK_ASSERT()
#define SMP_LOCK_XMIT(__x__) spin_lock_irqsave(&priv->pshare->lock_xmit, priv->pshare->lock_xmit_flags);
#define SMP_UNLOCK_XMIT(__x__) spin_unlock_irqrestore(&priv->pshare->lock_xmit, priv->pshare->lock_xmit_flags);
#define SMP_TRY_LOCK_XMIT(__x__,__y__) SMP_LOCK_XMIT(__x__);__y__ = 1;
#define SMP_LOCK_GLOBAL(__x__) spin_lock_irqsave(&global_lock, global_lock_flag);
#define SMP_UNLOCK_GLOBAL(__x__) spin_unlock_irqrestore(&global_lock, global_lock_flag);
#else
#define SMP_LOCK(__x__)	\
	do { \
		__u32 _cpu_id = get_cpu(); \
		if(priv->pshare->lock_owner != _cpu_id) \
			spin_lock_irqsave(&priv->pshare->lock, priv->pshare->lock_flags); \
		else {\
			panic_printk("[%s %d] recursion detection, caller=%p\n",__FUNCTION__,__LINE__,__builtin_return_address(0)); \
			panic_printk("Previous Lock Function is %s\n",priv->pshare->lock_func); \
		} \
		strcpy(priv->pshare->lock_func, __FUNCTION__);\
		priv->pshare->lock_owner = _cpu_id;\
		put_cpu(); \
	}while(0)
#define SMP_UNLOCK(__x__)				do {priv->pshare->lock_owner=-1;spin_unlock_irqrestore(&priv->pshare->lock, priv->pshare->lock_flags);}while(0)
#define SMP_TRY_LOCK(__x__,__y__) \
	do { \
		__u32 _cpu_id = get_cpu(); \
		if(priv->pshare->lock_owner != _cpu_id) { \
			SMP_LOCK(__x__); \
			__y__ = 1; \
		} else \
			__y__ = 0; \
		put_cpu();\
	} while(0)
#define SMP_LOCK_ASSERT() \
	do { \
		__u32 _cpu_id = get_cpu(); \
		if(priv->pshare->lock_owner != _cpu_id) { \
				panic_printk("ERROR: Without obtaining SMP_LOCK(). Please calling SMP_LOCK() before entering into %s()\n\n\n",__FUNCTION__); \
				put_cpu(); \
				return; \
		} \
		put_cpu(); \
	}while(0)
//#define SMP_LOCK_XMIT(__x__)			spin_lock_irqsave(&priv->pshare->lock_xmit, (__x__))
//#define SMP_UNLOCK_XMIT(__x__)			spin_unlock_irqrestore(&priv->pshare->lock_xmit, (__x__))
//#define SMP_LOCK_XMIT(__x__)			spin_lock_irqsave(&priv->pshare->lock_xmit, priv->pshare->lock_xmit_flags)
//#define SMP_UNLOCK_XMIT(__x__)			spin_unlock_irqrestore(&priv->pshare->lock_xmit, priv->pshare->lock_xmit_flags)
#define SMP_LOCK_XMIT(__x__)	\
    do { \
        __u32 _cpu_id = get_cpu(); \
        if(priv->pshare->lock_xmit_owner != _cpu_id) \
            spin_lock_irqsave(&priv->pshare->lock_xmit, priv->pshare->lock_xmit_flags); \
        else {\
            panic_printk("[%s %d] recursion detection, caller=%p\n",__FUNCTION__,__LINE__,__builtin_return_address(0)); \
            panic_printk("Previous Lock Function is %s\n",priv->pshare->lock_xmit_func); \
        }\
        strcpy(priv->pshare->lock_xmit_func, __FUNCTION__);\
        priv->pshare->lock_xmit_owner = _cpu_id;\
        put_cpu(); \
    }while(0)
#define SMP_UNLOCK_XMIT(__x__) 			do {priv->pshare->lock_xmit_owner=-1;spin_unlock_irqrestore(&priv->pshare->lock_xmit, priv->pshare->lock_xmit_flags);}while(0)
#define SMP_TRY_LOCK_XMIT(__x__,__y__) \
	do { \
		__u32 _cpu_id = get_cpu(); \
		if(priv->pshare->lock_xmit_owner != _cpu_id) { \
			SMP_LOCK_XMIT(__x__); \
			__y__ = 1; \
		} else \
			__y__ = 0; \
		put_cpu(); \
	} while(0)
#define SMP_LOCK_GLOBAL(__x__)	\
	do { \
        __u32 _cpu_id = get_cpu(); \
		if(global_lock_owner != _cpu_id) \
			spin_lock_irqsave(&global_lock, global_lock_flag); \
		else {\
			panic_printk("[%s %d] recursion detection, caller=%p\n",__FUNCTION__,__LINE__,__builtin_return_address(0)); \
			panic_printk("Previous Lock Function is %s\n",global_lock_func); \
		} \
		strcpy(global_lock_func, __FUNCTION__);\
		global_lock_owner = _cpu_id;\
		put_cpu(); \
	}while(0)
#define SMP_UNLOCK_GLOBAL(__x__)				do {global_lock_owner=-1;spin_unlock_irqrestore(&global_lock, global_lock_flag);}while(0)
#endif

#define SMP_LOCK_HASH_LIST(__x__)		spin_lock_irqsave(&priv->hash_list_lock, (__x__))
#define SMP_UNLOCK_HASH_LIST(__x__)		spin_unlock_irqrestore(&priv->hash_list_lock, (__x__))

#define SMP_LOCK_STACONTROL_LIST(__x__)  spin_lock_irqsave(&priv->stactrl.stactrl_lock, (__x__))
#define SMP_UNLOCK_STACONTROL_LIST(__x__) spin_unlock_irqrestore(&priv->stactrl.stactrl_lock, (__x__))

#ifdef AP_NEIGHBOR_INFO
#define SMP_LOCK_AP_NEIGHBOR_LIST(__x__)  spin_lock_irqsave(&priv->pshare->ap_neighbor.ap_neighbor_lock, (__x__))
#define SMP_UNLOCK_AP_NEIGHBOR_LIST(__x__) spin_unlock_irqrestore(&priv->pshare->ap_neighbor.ap_neighbor_lock, (__x__))
#endif

#define SMP_LOCK_TRANSITION_LIST(__x__)  spin_lock_irqsave(&priv->transition_list_lock, (__x__))
#define SMP_UNLOCK_TRANSITION_LIST(__x__) spin_unlock_irqrestore(&priv->transition_list_lock, (__x__))

#define SMP_LOCK_WAITING_LIST(__x__)  spin_lock_irqsave(&priv->waiting_list_lock, (__x__))
#define SMP_UNLOCK_WAITING_LIST(__x__) spin_unlock_irqrestore(&priv->waiting_list_lock, (__x__))

#define SMP_LOCK_BLOCK_LIST(__x__)  spin_lock_irqsave(&priv->block_list_lock, (__x__))
#define SMP_UNLOCK_BLOCK_LIST(__x__) spin_unlock_irqrestore(&priv->block_list_lock, (__x__))

#ifdef RTK_SMART_ROAMING
#define SMP_LOCK_SR_BLOCK_LIST(__x__)		spin_lock_irqsave(&priv->sr_block.sr_block_lock, (__x__))
#define SMP_UNLOCK_SR_BLOCK_LIST(__x__)		spin_unlock_irqrestore(&priv->sr_block.sr_block_lock, (__x__))
#endif

#define SMP_LOCK_ACL(__x__)
#define SMP_UNLOCK_ACL(__x__)

//#define SMP_LOCK_ASOC_LIST(__x__)		spin_lock_irqsave(&priv->asoc_list_lock, (__x__))
//#define SMP_UNLOCK_ASOC_LIST(__x__)		spin_unlock_irqrestore(&priv->asoc_list_lock, (__x__))
#define SMP_LOCK_ASOC_LIST(__x__) \
	do { \
		__u32 _cpu_id = get_cpu(); \
		if (priv->asoc_list_lock_owner != _cpu_id) \
			spin_lock_irqsave(&priv->asoc_list_lock, __x__); \
		else { \
			panic_printk("[%s %d] recursion detection, caller=%p\n",__FUNCTION__,__LINE__,__builtin_return_address(0)); \
			panic_printk("Previous Lock Function is %s\n",priv->asoc_list_lock_func); \
		} \
		strcpy(priv->asoc_list_lock_func, __FUNCTION__); \
		priv->asoc_list_lock_owner = _cpu_id; \
		put_cpu(); \
	} while(0)
#define SMP_UNLOCK_ASOC_LIST(__x__)		do {priv->asoc_list_lock_owner = -1; spin_unlock_irqrestore(&priv->asoc_list_lock, __x__);} while(0)
#define SMP_TRY_LOCK_ASOC_LIST(__x__, __y__) \
	do { \
		__u32 _cpu_id = get_cpu(); \
		if(priv->asoc_list_lock_owner != _cpu_id) { \
			SMP_LOCK_ASOC_LIST(__x__); \
			__y__ = 1; \
		} else \
			__y__ = 0; \
		put_cpu(); \
	} while(0)
	
#define SMP_LOCK_SLEEP_LIST(__x__)		spin_lock_irqsave(&priv->sleep_list_lock, (__x__))
#define SMP_UNLOCK_SLEEP_LIST(__x__)	spin_unlock_irqrestore(&priv->sleep_list_lock, (__x__))
#define SMP_LOCK_AUTH_LIST(__x__)		spin_lock_irqsave(&priv->auth_list_lock, (__x__))
#define SMP_UNLOCK_AUTH_LIST(__x__)		spin_unlock_irqrestore(&priv->auth_list_lock, (__x__))
#define SMP_LOCK_WAKEUP_LIST(__x__)		spin_lock_irqsave(&priv->wakeup_list_lock, (__x__))
#define SMP_UNLOCK_WAKEUP_LIST(__x__)	spin_unlock_irqrestore(&priv->wakeup_list_lock, (__x__))

#endif
#ifdef __KERNEL__
#define SMP_LOCK_SKB(__x__)				spin_lock_irqsave(&priv->pshare->lock_skb, (__x__))
#define SMP_UNLOCK_SKB(__x__)			spin_unlock_irqrestore(&priv->pshare->lock_skb, (__x__))
#if defined(CONFIG_SDIO_HCI)
#define SMP_LOCK_BUF(__x__)				do { spin_lock_bh(&priv->pshare->lock_buf); (void)(__x__); } while (0)
#define SMP_UNLOCK_BUF(__x__)			do { spin_unlock_bh(&priv->pshare->lock_buf); (void)(__x__); } while (0)
#elif defined(CONFIG_PCI_HCI) || defined(CONFIG_USB_HCI)
#define SMP_LOCK_BUF(__x__)				spin_lock_irqsave(&priv->pshare->lock_buf, (__x__))
#define SMP_UNLOCK_BUF(__x__)			spin_unlock_irqrestore(&priv->pshare->lock_buf, (__x__))
#endif
#if defined(CONFIG_SDIO_HCI)
#define SMP_LOCK_RECV(__x__) do { } while (0)
#define SMP_UNLOCK_RECV(__x__) do { } while (0)
#define SMP_TRY_LOCK_RECV(__x__,__y__) while (0)
#else
#if 0 // 0: show debugging message while recursion is detected. 1: disable debugging message
#define SMP_LOCK_RECV(__x__) spin_lock_irqsave(&priv->pshare->lock_recv, (__x__));
#define SMP_UNLOCK_RECV(__x__) spin_unlock_irqrestore(&priv->pshare->lock_recv, (__x__));
#define SMP_TRY_LOCK_RECV(__x__,__y__) SMP_LOCK_RECV(__x__);__y__ = 1;
#ifdef RTK_129X_PLATFORM
#define SMP_LOCK_IO_129X(__x__) spin_lock_irqsave(&priv->pshare->lock_io_129x, (__x__));
#define SMP_UNLOCK_IO_129X(__x__) spin_unlock_irqrestore(&priv->pshare->lock_io_129x, (__x__));
#endif
#else
#define SMP_LOCK_RECV(__x__)	\
	do { \
		__u32 _cpu_id = get_cpu(); \
		if (priv->pshare->lock_recv_owner != _cpu_id) \
			spin_lock_irqsave(&priv->pshare->lock_recv, (__x__)); \
		else \
			panic_printk("[%s %d] recursion detection\n",__FUNCTION__,__LINE__); \
		priv->pshare->lock_recv_owner = _cpu_id;\
		put_cpu(); \
	}while(0)
#define SMP_UNLOCK_RECV(__x__)				do {priv->pshare->lock_recv_owner=-1;spin_unlock_irqrestore(&priv->pshare->lock_recv, (__x__));}while(0)
#define SMP_TRY_LOCK_RECV(__x__,__y__) \
	do { \
		__u32 _cpu_id = get_cpu(); \
		if (priv->pshare->lock_recv_owner != _cpu_id) { \
			SMP_LOCK_RECV(__x__); \
			__y__ = 1; \
		} else \
			__y__ = 0; \
		put_cpu(); \
	} while(0)

#ifdef RTK_129X_PLATFORM
#define SMP_LOCK_IO_129X(__x__)	\
	do { \
        __u32 _cpu_id = get_cpu(); \
		if(priv->pshare->lock_io_129x_owner != _cpu_id) \
			spin_lock_irqsave(&priv->pshare->lock_io_129x, (__x__)); \
		else \
			panic_printk("[%s %d] recursion detection\n",__FUNCTION__,__LINE__); \
		priv->pshare->lock_io_129x_owner = _cpu_id;\
		put_cpu(); \
	}while(0)
#define SMP_UNLOCK_IO_129X(__x__)				do {priv->pshare->lock_io_129x_owner=-1;spin_unlock_irqrestore(&priv->pshare->lock_io_129x, (__x__));}while(0)
#endif //RTK_129X_PLATFORM
#endif
#endif //CONFIG_SDIO_HCI

//#define SMP_LOCK_RECV(__x__)			spin_lock_irqsave(&priv->pshare->lock_recv, (__x__))
//#define SMP_UNLOCK_RECV(__x__)			spin_unlock_irqrestore(&priv->pshare->lock_recv, (__x__))
#define SMP_LOCK_RX_DATA(__x__)			spin_lock_irqsave(&priv->rx_datalist_lock, (__x__))
#define SMP_UNLOCK_RX_DATA(__x__)		spin_unlock_irqrestore(&priv->rx_datalist_lock, (__x__))
#define SMP_LOCK_RX_MGT(__x__)			spin_lock_irqsave(&priv->rx_mgtlist_lock, (__x__))
#define SMP_UNLOCK_RX_MGT(__x__)		spin_unlock_irqrestore(&priv->rx_mgtlist_lock, (__x__))
#define SMP_LOCK_RX_CTRL(__x__)			spin_lock_irqsave(&priv->rx_ctrllist_lock, (__x__))
#define SMP_UNLOCK_RX_CTRL(__x__)		spin_unlock_irqrestore(&priv->rx_ctrllist_lock, (__x__))

#ifdef CONFIG_IEEE80211R
#define SMP_LOCK_FT_R0KH(__x__) \
	do { \
		__u32 _cpu_id = get_cpu(); \
		if (priv->ft_r0kh_lock_owner != _cpu_id) \
			spin_lock_irqsave(&priv->ft_r0kh_lock, __x__); \
		else { \
			panic_printk("[%s %d] recursion detection, caller=%p\n",__FUNCTION__,__LINE__,__builtin_return_address(0)); \
			panic_printk("Previous Lock Function is %s\n",priv->ft_r0kh_lock_func); \
		} \
		strcpy(priv->ft_r0kh_lock_func, __FUNCTION__); \
		priv->ft_r0kh_lock_owner = _cpu_id; \
		put_cpu(); \
	} while(0)
#define SMP_UNLOCK_FT_R0KH(__x__)		do {priv->ft_r0kh_lock_owner = -1; spin_unlock_irqrestore(&priv->ft_r0kh_lock, __x__);} while(0)
#define SMP_TRY_LOCK_FT_R0KH(__x__,__y__) \
	do { \
		__u32 _cpu_id = get_cpu(); \
		if (priv->ft_r0kh_lock_owner != _cpu_id) { \
			SMP_LOCK_FT_R0KH(__x__); \
			__y__ = 1; \
		} else \
			__y__ = 0; \
		put_cpu(); \
	} while(0)

#define SMP_LOCK_FT_R1KH(__x__) \
	do { \
		__u32 _cpu_id = get_cpu(); \
		if (priv->ft_r1kh_lock_owner != _cpu_id) \
			spin_lock_irqsave(&priv->ft_r1kh_lock, __x__); \
		else { \
			panic_printk("[%s %d] recursion detection, caller=%p\n",__FUNCTION__,__LINE__,__builtin_return_address(0)); \
			panic_printk("Previous Lock Function is %s\n",priv->ft_r1kh_lock_func); \
		} \
		strcpy(priv->ft_r1kh_lock_func, __FUNCTION__); \
		priv->ft_r1kh_lock_owner = _cpu_id; \
		put_cpu(); \
	} while(0)
#define SMP_UNLOCK_FT_R1KH(__x__)		do {priv->ft_r1kh_lock_owner = -1; spin_unlock_irqrestore(&priv->ft_r1kh_lock, __x__);} while(0)
#define SMP_TRY_LOCK_FT_R1KH(__x__,__y__) \
	do { \
		__u32 _cpu_id = get_cpu(); \
		if (priv->ft_r1kh_lock_owner != _cpu_id) { \
			SMP_LOCK_FT_R1KH(__x__); \
			__y__ = 1; \
		} else \
			__y__ = 0; \
		put_cpu(); \
	} while(0)
#endif

#ifdef SBWC
#define SMP_LOCK_SBWC(__x__)	\
	do { \
		__u32 _cpu_id = get_cpu(); \
		if(priv->pshare->lock_sbwc_owner != _cpu_id) \
			spin_lock_irqsave(&priv->pshare->lock_sbwc, (__x__)); \
		else \
			panic_printk("[%s %d] recursion detection\n",__FUNCTION__,__LINE__); \
		priv->pshare->lock_sbwc_owner = _cpu_id;\
		put_cpu(); \
	} while(0)
#define SMP_UNLOCK_SBWC(__x__)				do {priv->pshare->lock_sbwc_owner=-1;spin_unlock_irqrestore(&priv->pshare->lock_sbwc, (__x__));}while(0)
#define SMP_TRY_LOCK_SBWC(__x__,__y__) \
	do { \
		__u32 _cpu_id = get_cpu(); \
		if (priv->pshare->lock_sbwc_owner != _cpu_id) { \
			SMP_LOCK_SBWC(__x__); \
			__y__ = 1; \
		} else \
			__y__ = 0; \
		put_cpu(); \
	} while(0)
#endif /* SBWC */

#ifdef RTK_NL80211
#define SMP_LOCK_CFG80211(__x__)		spin_lock_irqsave(&priv->cfg80211_lock, (__x__))
#define SMP_UNLOCK_CFG80211(__x__)		spin_unlock_irqrestore(&priv->cfg80211_lock, (__x__))
#endif
#define SMP_LOCK_IQK(__x__)				spin_lock_irqsave(&priv->pshare->lock_iqk, (__x__))
#define SMP_UNLOCK_IQK(__x__)			spin_unlock_irqrestore(&priv->pshare->lock_iqk, (__x__))
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
#ifdef __ECOS
#define SMP_LOCK_BUF(__x__)			_enter_critical_mutex(&(priv->pshare->lock_buf), (__x__))
#define SMP_UNLOCK_BUF(__x__)			_exit_critical_mutex(&(priv->pshare->lock_buf), (__x__))
#define SMP_LOCK_REORDER_CTRL(__x__)		_enter_critical_mutex(&priv->pshare->rc_packet_q_lock, (__x__))
#define SMP_UNLOCK_REORDER_CTRL(__x__)	_exit_critical_mutex(&priv->pshare->rc_packet_q_lock, (__x__))
#else
#define SMP_LOCK_REORDER_CTRL(__x__)	do { spin_lock_bh(&priv->pshare->rc_packet_q_lock); (void)(__x__); } while (0)
#define SMP_UNLOCK_REORDER_CTRL(__x__)	do { spin_unlock_bh(&priv->pshare->rc_packet_q_lock); (void)(__x__); } while (0)
#endif
#define SMP_TRY_LOCK_REORDER_CTRL(__x__,__y__)

#define DEFRAG_LOCK(__x__)				do { spin_lock_bh(&priv->defrag_lock); (void)(__x__); } while (0)
#define DEFRAG_UNLOCK(__x__)			do { spin_unlock_bh(&priv->defrag_lock); (void)(__x__); } while (0)
#define SMP_LOCK_PSK_RESEND(__x__)		do { } while (0)
#define SMP_UNLOCK_PSK_RESEND(__x__)		do { } while (0)
#define SMP_LOCK_PSK_GKREKEY(__x__)		do { } while (0)
#define SMP_UNLOCK_PSK_GKREKEY(__x__)		do { } while (0)
#elif defined(CONFIG_PCI_HCI)

#define SMP_LOCK_REORDER_CTRL(__x__)	\
	do { \
		__u32 _cpu_id = get_cpu(); \
		if (priv->pshare->rc_packet_q_lock_owner != _cpu_id) \
			spin_lock_irqsave(&priv->pshare->rc_packet_q_lock, (__x__)); \
		else \
			panic_printk("[%s %d] recursion detection\n",__FUNCTION__,__LINE__); \
		priv->pshare->rc_packet_q_lock_owner = _cpu_id;\
		put_cpu(); \
	} while(0)
#define SMP_UNLOCK_REORDER_CTRL(__x__)				do {priv->pshare->rc_packet_q_lock_owner=-1;spin_unlock_irqrestore(&priv->pshare->rc_packet_q_lock, (__x__));}while(0)
#define SMP_TRY_LOCK_REORDER_CTRL(__x__,__y__) \
	do { \
		__u32 _cpu_id = get_cpu(); \
		if (priv->pshare->rc_packet_q_lock_owner != _cpu_id) { \
			SMP_LOCK_REORDER_CTRL(__x__); \
			__y__ = 1; \
		} else \
			__y__ = 0; \
		put_cpu(); \
	} while(0)

#define DEFRAG_LOCK(__x__)				spin_lock_irqsave(&priv->defrag_lock, (__x__))
#define DEFRAG_UNLOCK(__x__)			spin_unlock_irqrestore(&priv->defrag_lock, (__x__))
#define SMP_LOCK_PSK_RESEND(__x__)		spin_lock_irqsave(&priv->psk_resend_lock, (__x__))
#define SMP_UNLOCK_PSK_RESEND(__x__)	spin_unlock_irqrestore(&priv->psk_resend_lock, (__x__))
#define SMP_LOCK_PSK_GKREKEY(__x__)		spin_lock_irqsave(&priv->psk_gkrekey_lock, (__x__))
#define SMP_UNLOCK_PSK_GKREKEY(__x__)	spin_unlock_irqrestore(&priv->psk_gkrekey_lock, (__x__))
#endif

#else
#define SAVE_INT_AND_CLI(__x__)		spin_lock_irqsave(&priv->pshare->lock, (__x__))
#define RESTORE_INT(__x__)			spin_unlock_irqrestore(&priv->pshare->lock, (__x__))
#ifndef __ECOS
#define SMP_LOCK(__x__)	
#define SMP_UNLOCK(__x__)
#endif
#define SMP_LOCK_XMIT(__x__)		
#define SMP_UNLOCK_XMIT(__x__)
#define SMP_LOCK_GLOBAL(__x__)
#define SMP_UNLOCK_GLOBAL(__x__)		
#define SMP_LOCK_ACL(__x__)
#define SMP_UNLOCK_ACL(__x__)
#define SMP_LOCK_HASH_LIST(__x__)
#define SMP_UNLOCK_HASH_LIST(__x__)
#define SMP_LOCK_ASOC_LIST(__x__)
#define SMP_UNLOCK_ASOC_LIST(__x__)
#define SMP_TRY_LOCK_ASOC_LIST(__x__, __y__)
#define SMP_LOCK_SLEEP_LIST(__x__)
#define SMP_UNLOCK_SLEEP_LIST(__x__)
#define SMP_LOCK_AUTH_LIST(__x__)
#define SMP_UNLOCK_AUTH_LIST(__x__)
#define SMP_LOCK_WAKEUP_LIST(__x__)
#define SMP_UNLOCK_WAKEUP_LIST(__x__)
#define SMP_LOCK_STACONTROL_LIST(__X__)
#define SMP_UNLOCK_STACONTROL_LIST(__X__)
#ifdef AP_NEIGHBOR_INFO
#define SMP_LOCK_AP_NEIGHBOR_LIST(__x__)  
#define SMP_UNLOCK_AP_NEIGHBOR_LIST(__x__) 
#endif
#define SMP_LOCK_TRANSITION_LIST(__X__)
#define SMP_UNLOCK_TRANSITION_LIST(__X__)
#define SMP_LOCK_WAITING_LIST(__X__)
#define SMP_UNLOCK_WAITING_LIST(__X__)
#define SMP_LOCK_BLOCK_LIST(__X__)
#define SMP_UNLOCK_BLOCK_LIST(__X__)
#define SMP_LOCK_SKB(__x__)			
#define SMP_UNLOCK_SKB(__x__)		
#define SMP_LOCK_BUF(__x__)			
#define SMP_UNLOCK_BUF(__x__)	
#define SMP_LOCK_RECV(__x__)
#define SMP_UNLOCK_RECV(__x__)
#ifdef RTK_129X_PLATFORM
#define SMP_LOCK_IO_129X(__x__)
#define SMP_UNLOCK_IO_129X(__x__)
#endif
#define SMP_LOCK_RX_DATA(__x__)
#define SMP_UNLOCK_RX_DATA(__x__)
#define SMP_LOCK_RX_MGT(__x__)
#define SMP_UNLOCK_RX_MGT(__x__)
#define SMP_LOCK_RX_CTRL(__x__)
#define SMP_UNLOCK_RX_CTRL(__x__)
#define SMP_LOCK_REORDER_CTRL(__x__)
#define SMP_UNLOCK_REORDER_CTRL(__x__)
#define SMP_TRY_LOCK_REORDER_CTRL(__x__,__y__)

#ifdef CONFIG_IEEE80211R
#define SMP_LOCK_FT_R0KH(__x__)	
#define SMP_UNLOCK_FT_R0KH(__x__)
#define SMP_TRY_LOCK_FT_R0KH(__x__,__y__)

#define SMP_LOCK_FT_R1KH(__x__)
#define SMP_UNLOCK_FT_R1KH(__x__)
#define SMP_TRY_LOCK_FT_R1KH(__x__,__y__)
#endif

#ifdef SBWC
#define SMP_LOCK_SBWC(__x__)
#define SMP_UNLOCK_SBWC(__x__)
#define SMP_TRY_LOCK_SBWC(__x__,__y__)
#endif

//#define DEFRAG_LOCK(__x__)			spin_lock_irqsave(&priv->defrag_lock, (__x__))
//#define DEFRAG_UNLOCK(__x__)		spin_unlock_irqrestore(&priv->defrag_lock, (__x__))
#define SMP_LOCK_PSK_RESEND(__x__)
#define SMP_UNLOCK_PSK_RESEND(__x__)
#define SMP_LOCK_PSK_GKREKEY(__x__)
#define SMP_UNLOCK_PSK_GKREKEY(__x__)
#define SMP_LOCK_ASSERT()
#ifdef RTK_NL80211
#define SMP_LOCK_CFG80211(__x__)
#define SMP_UNLOCK_CFG80211(__x__)
#endif
#define SMP_LOCK_IQK(__x__)
#define SMP_UNLOCK_IQK(__x__)
#endif

#define SMP_LOCK_MBSSID(__x__)
#define SMP_UNLOCK_MBSSID(__x__)

#endif //CONFIG_RTL_TRIBAND_SUPPORT






/*NOTE if 1.5 seconds should be RTL_SECONDS_TO_JIFFIES(15)/10 
  *RTL_MILISECONDS_TO_JIFFIES shoud consider the HZ value
  *for example HZ=100, x should large than 10
  */


#define RTL_10MILISECONDS_TO_JIFFIES(x) (((x)*HZ)/100)
#define RTL_JIFFIES_TO_MICROSECOND ((1000*1000)/HZ)
#define RTL_JIFFIES_TO_MILISECONDS(x) (((x)*1000)/HZ)
#define RTL_JIFFIES_TO_SECOND(x)  ((x)/HZ)


#define CHIP_VER_92X_SERIES(priv)		( (priv->pshare->version_id&0xf00f) < 0x1003)
#ifndef RTL_MAX
#define RTL_MAX(X, Y) ((X) > (Y) ? (X) : (Y))
#endif

//#if defined(CONFIG_RTL_92C_SUPPORT) || defined(SUPPORT_RTL8188E_TC)
#define IS_TEST_CHIP(priv)		((priv->pshare->version_id&0x100))
//#endif

#if defined(USE_OUT_SRC)
#if _OUTSRC_COEXIST
#define IS_OUTSRC_CHIP(priv)	(priv->pshare->use_outsrc)
#else
#define IS_OUTSRC_CHIP(priv)	1
#endif
#endif

#if defined(CONFIG_PCI_HCI) || defined(CONFIG_RTL_TRIBAND_SUPPORT)
#if (CONFIG_WLAN_NOT_HAL_EXIST==0)
#define IS_HAL_CHIP(priv)	(1)
#else
#define IS_HAL_CHIP(priv)	(priv->pshare->use_hal)
#endif

#define IS_MACHAL_CHIP(priv)	(priv->pshare->use_macHalAPI)
#define GET_MACHAL_API(priv)	(priv->pHalmac_api)


#elif  defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
#if defined(CONFIG_RTL_92C_SUPPORT) || defined(CONFIG_RTL_92D_SUPPORT) || defined(CONFIG_RTL_88E_SUPPORT)
#define IS_HAL_CHIP(priv)			0
#define IS_MACHAL_CHIP(priv)		0
#define GET_MACHAL_API(priv)		0
#elif defined(CONFIG_WLAN_HAL_8192EE)
#define IS_HAL_CHIP(priv)			1
#define IS_MACHAL_CHIP(priv)		0
#define GET_MACHAL_API(priv)		0
#else
#define IS_HAL_CHIP(priv)			1
#define IS_MACHAL_CHIP(priv)		1
#define GET_MACHAL_API(priv)		1
#endif
#endif

//#ifdef CONFIG_RTL_92C_SUPPORT /*tonyWifi 20150211 comment*/
#define IS_88RE(priv)			((priv->pshare->version_id&0x200))
//#endif
#define IS_UMC_A_CUT(priv)		((priv->pshare->version_id&0x4f0)==0x400)
#define IS_UMC_B_CUT(priv)		((priv->pshare->version_id&0x4f0)==0x410)
#ifdef CONFIG_RTL_92C_SUPPORT
#define IS_UMC_A_CUT_88C(priv)	(IS_UMC_A_CUT(priv) && (GET_CHIP_VER(priv) == VERSION_8188C))
#define IS_UMC_B_CUT_88C(priv)	(IS_UMC_B_CUT(priv) && (GET_CHIP_VER(priv) == VERSION_8188C))
#endif
//#ifdef CONFIG_RTL_8812_SUPPORT
#define IS_B_CUT_8812(priv)	((GET_CHIP_VER(priv) == VERSION_8812E) && ((priv->pshare->version_id&0xf0)==0))
#define IS_C_CUT_8812(priv)	((GET_CHIP_VER(priv) == VERSION_8812E) && ((priv->pshare->version_id&0xf0)==0x10))
//#endif

#define IS_A_CUT_8881A(priv)	((GET_CHIP_VER(priv) == VERSION_8881A) && ((priv->pshare->version_id&0xf0)==0))
#define IS_C_CUT_8192E(priv)	((GET_CHIP_VER(priv) == VERSION_8192E) && ((priv->pshare->version_id&0xf0)>>4 == 0x2))
#define IS_D_CUT_8192E(priv)	((GET_CHIP_VER(priv) == VERSION_8192E) && ((priv->pshare->version_id&0xf0)>>4 == 0x3))



                                   
#define RTL_SET_MASK(reg,mask,val,shift) (((reg)&(~(mask)))|((val)<<(shift)))


#if defined(CONFIG_USB_HCI) && !defined(CONFIG_RTL_TRIBAND_SUPPORT)
#define RTL_R8(reg)			usb_read8(priv, reg)
#define RTL_R16(reg)			usb_read16(priv, reg)
#define RTL_R32(reg)			usb_read32(priv, reg)

#define RTL_W8(reg, val8)		usb_write8(priv, reg, val8)
#define RTL_W16(reg, val16)	usb_write16(priv, reg, val16)
#define RTL_W32(reg, val32)	usb_write32(priv, reg, val32)
#define RTL_Wn(reg, len, val)	usb_writeN(priv, reg, len, val)

#define get_desc(val)	le32_to_cpu(val)
#define set_desc(val)	cpu_to_le32(val)
#endif // CONFIG_USB_HCI

#if defined(CONFIG_SDIO_HCI) && !defined(CONFIG_RTL_TRIBAND_SUPPORT)
#define RTL_R8(reg)			sdio_read8(priv, reg, NULL)
#define RTL_R16(reg)			sdio_read16(priv, reg, NULL)
#define RTL_R32(reg)			sdio_read32(priv, reg, NULL)

#define RTL_W8(reg, val8)		sdio_write8(priv, reg, val8)
#define RTL_W16(reg, val16)	sdio_write16(priv, reg, val16)
#define RTL_W32(reg, val32)	sdio_write32(priv, reg, val32)
#define RTL_Wn(reg, len, val)	sdio_writeN(priv, reg, len, val)

#define get_desc(val)	le32_to_cpu(val)
#define set_desc(val)	cpu_to_le32(val)
#endif // CONFIG_SDIO_HCI







#ifdef __OSK__
// defined as normal function jim 20100611
extern __IRAM_WIFI_PRI1 struct list_head *dequeue_frame(struct rtl8192cd_priv *priv, struct list_head *head);
#else
static __inline__ struct list_head *dequeue_frame(struct rtl8192cd_priv *priv, struct list_head *head)
{
#ifndef SMP_SYNC
	unsigned long flags=0;
#endif
	struct list_head *pnext;

	SAVE_INT_AND_CLI(flags);
	if (list_empty(head)) {
		RESTORE_INT(flags);
		return (void *)NULL;
	}

	pnext = head->next;
	list_del_init(pnext);

	RESTORE_INT(flags);

	return pnext;
}
#endif

static __inline__ int wifi_mac_hash(unsigned char *mac)
{
	unsigned long x;

	x = mac[0];
	x = (x << 2) ^ mac[1];
	x = (x << 2) ^ mac[2];
	x = (x << 2) ^ mac[3];
	x = (x << 2) ^ mac[4];
	x = (x << 2) ^ mac[5];

	x ^= x >> 8;

	return x & (NUM_STAT - 1);
}

static __inline__ int is_OFDM_rate(unsigned char rate)
{
	if ((rate == _6M_RATE_) || (rate == _9M_RATE_) || (rate == _12M_RATE_) || (rate == _18M_RATE_) || (rate == _24M_RATE_) || (rate == _36M_RATE_) || (rate == _48M_RATE_) || (rate == _54M_RATE_))
		return TRUE;
	else
		return FALSE;
}

static __inline__ int is_HT_rate(unsigned char rate)
{
	if (rate >= HT_RATE_ID && rate < VHT_RATE_ID)
		return TRUE;
	else
		return FALSE;
}




static __inline__ int is_3T_rate(unsigned char rate)
{
#ifdef RTK_AC_SUPPORT
	if ((rate >= _NSS3_MCS0_RATE_) && (rate <= _NSS3_MCS9_RATE_)) 
		return TRUE;
	else
#endif
		return ((rate >= _MCS16_RATE_) && (rate <= _MCS23_RATE_)) ? TRUE : FALSE;
}

static __inline__ int is_4T_rate(unsigned char rate)
{
#ifdef RTK_AC_SUPPORT
	if ((rate >= _NSS4_MCS0_RATE_) && (rate <= _NSS4_MCS9_RATE_)) 
		return TRUE;
	else
#endif
		return ((rate >= _MCS24_RATE_) && (rate <= _MCS31_RATE_)) ? TRUE : FALSE;
}







static __inline__ int is_MCS_1SS_rate(unsigned char rate)
{
	if ((rate >= _MCS0_RATE_) && (rate <= _MCS7_RATE_))
		return TRUE;
	else
		return FALSE;
}

static __inline__ int is_MCS_2SS_rate(unsigned char rate)
{
	if ((rate >= _MCS8_RATE_) && (rate <= _MCS15_RATE_))
		return TRUE;
	else
		return FALSE;
}

static __inline__ int is_MCS_3SS_rate(unsigned char rate)
{
	if ((rate >= _MCS16_RATE_) && (rate <= _MCS23_RATE_))
		return TRUE;
	else
		return FALSE;
}

static __inline__ int is_MCS_4SS_rate(unsigned char rate)
{
	if ((rate >= _MCS24_RATE_) && (rate <= _MCS31_RATE_))
		return TRUE;
	else
		return FALSE;
}

static __inline__ int is_1SS_rate(unsigned char rate)
{
#ifdef RTK_AC_SUPPORT
	if ((rate >= _NSS1_MCS0_RATE_) && (rate <= _NSS1_MCS9_RATE_)) 
		return TRUE;
	else
#endif
		return (((rate >= _MCS0_RATE_) && (rate <= _MCS7_RATE_)) || rate <= _54M_RATE_) ? TRUE : FALSE;
}

static __inline__ int is_2SS_rate(unsigned char rate)
{
#ifdef RTK_AC_SUPPORT
	if ((rate >= _NSS2_MCS0_RATE_) && (rate <= _NSS2_MCS9_RATE_)) 
		return TRUE;
	else
#endif
		return ((rate >= _MCS8_RATE_) && (rate <= _MCS15_RATE_)) ? TRUE : FALSE;
}

static __inline__ int is_3SS_rate(unsigned char rate)
{
#ifdef RTK_AC_SUPPORT
	if ((rate >= _NSS3_MCS0_RATE_) && (rate <= _NSS3_MCS9_RATE_)) 
		return TRUE;
	else
#endif
		return ((rate >= _MCS16_RATE_) && (rate <= _MCS23_RATE_)) ? TRUE : FALSE;
}

static __inline__ int is_4SS_rate(unsigned char rate)
{
#ifdef RTK_AC_SUPPORT
	if ((rate >= _NSS4_MCS0_RATE_) && (rate <= _NSS4_MCS9_RATE_)) 
		return TRUE;
	else
#endif
		return ((rate >= _MCS23_RATE_) && (rate <= _MCS31_RATE_)) ? TRUE : FALSE;
}


#if (defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)) && !defined(CONFIG_RTL_TRIBAND_SUPPORT)
/* The following definitions were useless in USB/SDIO I/F. The only purpose is to avoid compiler issue */
#define PCI_DMA_BIDIRECTIONAL	0
#define PCI_DMA_TODEVICE		1
#define PCI_DMA_FROMDEVICE		2
#define PCI_DMA_NONE			3

static __inline__ void rtl_cache_sync_wback(struct rtl8192cd_priv *priv, unsigned long start,
				unsigned int size, int direction)
{
}

static __inline__ unsigned long get_physical_addr(struct rtl8192cd_priv *priv, void *ptr,
				unsigned int size, int direction)
{
	return (unsigned long)ptr;
}

#endif // CONFIG_USB_HCI || CONFIG_SDIO_HCI

static __inline__ int can_enable_rx_ldpc(struct rtl8192cd_priv *priv)
{
	if(priv->pmib->dot11nConfigEntry.dot11nLDPC == 3) //force enable rx ldpc
		return 1;
		
	if(GET_CHIP_VER(priv) == VERSION_8197G || GET_CHIP_VER(priv) == VERSION_8197F || GET_CHIP_VER(priv) == VERSION_8192F || GET_CHIP_VER(priv) == VERSION_8821C)
		return 0;
	else if(GET_CHIP_VER(priv) >= VERSION_8814A)
		return 1;
	else
#ifdef RTK_AC_SUPPORT //for 11ac logo
	if(AC_SIGMA_MODE != AC_SIGMA_NONE)
		return 1;
	else
#endif
		return 0;
}


/*
 *  find a token in a string. If succes, return pointer of token next. If fail, return null
 */
static __inline__ char *get_value_by_token(char *data, char *token)
{
		int idx=0, src_len=strlen(data), token_len=strlen(token);

		while (src_len >= token_len) {
			if (!memcmp(&data[idx], token, token_len))
				return (&data[idx+token_len]);
			src_len--;
			idx++;
		}
		return NULL;
}
static __inline__ int get_rf_NTx(unsigned char mimo_mode)
{
	u1Byte			Ntx = 0;

	if(mimo_mode == RF_4T4R)
		Ntx = 4;
	else if(mimo_mode== RF_3T3R)
		Ntx = 3;
	else if(mimo_mode == RF_2T4R)
		Ntx = 2;
	else if(mimo_mode == RF_2T2R)
		Ntx = 2;
	else
		Ntx = 1;

	return Ntx;

}

#ifdef RTK_AC_SUPPORT	
static __inline__ int get_sta_vht_mimo_mode(struct stat_info *pstat) {

	u1Byte		sta_mimo_mode = -1;


	if(pstat->vht_cap_len) {
		if(((le32_to_cpu(pstat->vht_cap_buf.vht_support_mcs[0])>>8)&3)==3) // no support RX 5ss
			sta_mimo_mode = RF_4T4R;
		if(((le32_to_cpu(pstat->vht_cap_buf.vht_support_mcs[0])>>6)&3)==3) // no support RX 4ss
			sta_mimo_mode = RF_3T3R;
		if(((le32_to_cpu(pstat->vht_cap_buf.vht_support_mcs[0])>>4)&3)==3) // no support RX 3ss
			sta_mimo_mode = RF_2T2R;
		if(((le32_to_cpu(pstat->vht_cap_buf.vht_support_mcs[0])>>2)&3)==3) // no support RX 2ss
			sta_mimo_mode = RF_1T1R; 	
	}
	return sta_mimo_mode;
}
#endif


static __inline__ unsigned int get_supported_mcs(struct rtl8192cd_priv *priv)
{
	if (get_rf_mimo_mode(priv) == RF_1T1R)
		return (priv->pmib->dot11nConfigEntry.dot11nSupportedMCS & 0x00ff);
	else if(get_rf_mimo_mode(priv) == RF_2T2R)
		return (priv->pmib->dot11nConfigEntry.dot11nSupportedMCS & 0xffff);
	else if(get_rf_mimo_mode(priv) == RF_3T3R)
		return (priv->pmib->dot11nConfigEntry.dot11nSupportedMCS & 0xffffff);
	else if(get_rf_mimo_mode(priv) == RF_4T4R)
		return (priv->pmib->dot11nConfigEntry.dot11nSupportedMCS & 0xffffffff);
	
	return (priv->pmib->dot11nConfigEntry.dot11nSupportedMCS & 0xffff); //2ss as default
}





static __inline__ unsigned char get_cck_swing_idx(unsigned int bandwidth, unsigned char ofdm_swing_idx)
{
	unsigned char cck_swing_idx;

	if (bandwidth == CHANNEL_WIDTH_20) {
		if (ofdm_swing_idx >= TxPwrTrk_CCK_SwingTbl_Len)
			cck_swing_idx = TxPwrTrk_CCK_SwingTbl_Len - 1;
		else
			cck_swing_idx = ofdm_swing_idx;
	}
	else {	// 40M bw
		if (ofdm_swing_idx < 12)
			cck_swing_idx = 0;
		else if (ofdm_swing_idx > (TxPwrTrk_CCK_SwingTbl_Len - 1 + 12))
			cck_swing_idx = TxPwrTrk_CCK_SwingTbl_Len - 1;
		else
			cck_swing_idx = ofdm_swing_idx - 12;
	}

	return cck_swing_idx;
}

//Apollo slave cpu do not use watch dog
#ifdef CONFIG_ARCH_LUNA_SLAVE
#undef CONFIG_RTL_WTDOG
#endif

#if (defined(__ECOS)&&defined(CONFIG_RTL_819X)) || defined(CONFIG_RTL865X_WTDOG) || defined(CONFIG_RTL_WTDOG)

#if !defined(CONFIG_RTL_8198B) && !defined(__OSK__)
#ifndef BSP_WDTCNR
	#define BSP_WDTCNR 0xB800311C
#endif
#endif

#if defined(__LINUX_2_6__) || defined(__ECOS)
#if defined(CONFIG_RTL_8198B)
#define _WDTCNR_			BSP_WDTCNTRR
#else
#define _WDTCNR_			BSP_WDTCNR
#endif
#else
#define _WDTCNR_			WDTCNR
#endif

#define _WDTKICK_			(1 << 23)
#ifdef __ECOS
#define _WDTSTOP_			(0xA5000000)
#else
#define _WDTSTOP_			(0xA5f00000)
#endif

static __inline__ void watchdog_stop(struct rtl8192cd_priv *priv)
{
	*((volatile unsigned long *)_WDTCNR_) |= _WDTKICK_;
	priv->pshare->wtval = *((volatile unsigned long *)_WDTCNR_);
#ifdef __ECOS
	*((volatile unsigned long *)_WDTCNR_) |= _WDTSTOP_;
#else
	*((volatile unsigned long *)_WDTCNR_) = _WDTSTOP_;
#endif
}

static __inline__ void watchdog_resume(struct rtl8192cd_priv *priv)
{
	*((volatile unsigned long *)_WDTCNR_) = priv->pshare->wtval;
	*((volatile unsigned long *)_WDTCNR_) |= _WDTKICK_;
}


static __inline__ void watchdog_kick(void)
{
	*((volatile unsigned long *)_WDTCNR_) |= _WDTKICK_;
}

#else // no watchdog support

static __inline__ void watchdog_stop(struct rtl8192cd_priv *priv)
{}

static __inline__ void watchdog_resume(struct rtl8192cd_priv *priv)
{}

static __inline__ void watchdog_kick(void)
{}

#endif

#ifndef _WDTCNR_
	#define	_WDTCNR_ 0xB800311C
#endif

static __inline__ void watchdog_reboot(void)
{
	#ifdef __ECOS //avoid R_mips_26 problem
		cyg_interrupt_disable();
	#else
		local_irq_disable();
	#endif
	*((volatile unsigned long *)_WDTCNR_) = 0;
	while (1);
}

#if defined(CONFIG_RTL_WAPI_SUPPORT)
void wapi_event_indicate(struct rtl8192cd_priv *priv);
#endif

#ifdef CONFIG_RTL8190_PRIV_SKB
#ifdef CONCURRENT_MODE
struct sk_buff *dev_alloc_skb_priv(struct rtl8192cd_priv *priv, unsigned int size);
#else
struct sk_buff *dev_alloc_skb_priv(struct rtl8192cd_priv *priv, unsigned int size);
#endif
#endif


#define CIRC_SPACE_RTK(head,tail,size)	CIRC_CNT_RTK((tail),((head)+1),(size))

#ifdef CONFIG_USE_VMALLOC
#define rtw_vmalloc(sz)			_rtw_vmalloc((sz))
#define rtw_zvmalloc(sz)			_rtw_zvmalloc((sz))
#define rtw_vmalloc_flag(sz,gfp)	_rtw_vmalloc((sz))
#define rtw_vmfree(pbuf, sz)		_rtw_vmfree((pbuf), (sz))
#else // !CONFIG_USE_VMALLOC
#define rtw_vmalloc(sz)			_rtw_malloc((sz))
#define rtw_vmalloc_flag(sz,gfp)	_rtw_malloc_flag((sz), (gfp))
#define rtw_zvmalloc(sz)			_rtw_zmalloc((sz))
#define rtw_vmfree(pbuf, sz)		_rtw_mfree((pbuf), (sz))
#endif // CONFIG_USE_VMALLOC
#define rtw_malloc(sz)			_rtw_malloc((sz))
#define rtw_zmalloc(sz)			_rtw_zmalloc((sz))
#define rtw_mfree(pbuf, sz)		_rtw_mfree((pbuf), (sz))

u8* _rtw_vmalloc(u32 sz);
u8* _rtw_zvmalloc(u32 sz);
void _rtw_vmfree(const void *pbuf, u32 sz);
u8* _rtw_zmalloc(u32 sz);
#if defined(__ECOS)
u8* _rtw_malloc_flag(u32 sz, int gfp);
#else
u8* _rtw_malloc_flag(u32 sz, gfp_t gfp);
#endif
u8* _rtw_malloc(u32 sz);
void _rtw_mfree(const void *pbuf, u32 sz);

void* rtw_malloc2d(int h, int w, int size);
void rtw_mfree2d(void *pbuf, int h, int w, int size);

#if defined(USE_PID_NOTIFY) && defined(LINUX_2_6_27_)
extern struct pid *_wlanapp_pid;
extern struct pid *_wlanwapi_pid;
#ifdef CONFIG_IEEE80211R
extern struct pid *_wlanft_pid;
#endif
#endif

#if defined(GENERAL_EVENT)
extern struct pid *_wlangeneral_pid;
#endif

#if defined(CONFIG_RTL_CUSTOM_PASSTHRU)
#ifdef __ECOS
INT32 rtl_isWlanPassthruFrame(UINT8 *data);
#else
INT32 rtl_isPassthruFrame(UINT8 *data);
#endif
#endif


#ifndef __OSK__
static inline long timeval_to_us(const struct timeval *tv)
{
	return (tv->tv_sec*1000000L)+ tv->tv_usec;
}
#endif

// Get the N-bytes aligment offset from the current length
#define N_BYTE_ALIGMENT(__Value, __Aligment) ((__Aligment == 1) ? (__Value) : (((__Value + __Aligment - 1) / __Aligment) * __Aligment))

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
static inline int tx_servq_len(struct tx_servq *ptxservq)
{
	return ptxservq->xframe_queue.qlen;
}
#endif

#ifdef CONFIG_USB_HCI
/*
* Increase and check if the continual_urb_error of this @param dvobjprive is larger than MAX_CONTINUAL_URB_ERR
* @return _TRUE:
* @return _FALSE:
*/
static inline int rtw_inc_and_chk_continual_urb_error(struct rtl8192cd_priv *priv)
{
	int ret = FALSE;
	int value;
	if( (value=atomic_inc_return(&priv->pshare->continual_urb_error)) > MAX_CONTINUAL_URB_ERR) {
		printk("[ERROR] continual_urb_error:%d > %d\n", value, MAX_CONTINUAL_URB_ERR);
		ret = TRUE;
	} else {
		//printk("continual_urb_error:%d\n", value);
	}
	return ret;
}

/*
* Set the continual_urb_error of this @param dvobjprive to 0
*/
static inline void rtw_reset_continual_urb_error(struct rtl8192cd_priv *priv)
{
	atomic_set(&priv->pshare->continual_urb_error, 0);	
}

#endif // CONFIG_USB_HCI

#if defined(RTK_ATM) && !defined(HS2_SUPPORT)
void staip_snooping_byarp(struct sk_buff *pskb, struct stat_info *pstat);
#endif
#ifdef HS2_SUPPORT
unsigned int getDSCP2UP(struct rtl8192cd_priv *priv, unsigned char DSCP); //after pf4
void setQoSMapConf(struct rtl8192cd_priv *priv);//after pf4
void staip_snooping_byarp(struct sk_buff *pskb, struct stat_info *pstat);
void staip_snooping_bydhcp(struct sk_buff *pskb, struct rtl8192cd_priv *priv);
void stav6ip_snooping_bynadvert(struct sk_buff *pskb, struct stat_info *pstat);
void stav6ip_snooping_bynsolic(struct rtl8192cd_priv *priv, struct sk_buff *pskb, struct stat_info *pstat);// modify after pf4
int proxy_arp_handle(struct rtl8192cd_priv *priv, struct sk_buff *skb);
int proxy_icmpv6_ndisc(struct rtl8192cd_priv *priv, struct sk_buff *skb);
void calcu_sta_v6ip(struct stat_info *pstat);
int check_nei_advt(struct rtl8192cd_priv *priv, struct sk_buff *skb);
#endif
#ifdef SUPPORT_MONITOR
void start_monitor_mode(struct rtl8192cd_priv * priv);
void stop_monitor_mode(struct rtl8192cd_priv * priv);
#endif


#if defined(CONFIG_PCI_HCI) || defined(CONFIG_RTL_TRIBAND_SUPPORT)
static inline int hash_list_add(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	unsigned int index;
	struct list_head *plist;
	int ret = 0;

#ifdef SMP_SYNC
	SMP_LOCK_HASH_LIST(flags);
#endif

	index = wifi_mac_hash(pstat->cmn_info.mac_addr);
	plist = priv->stat_hash;
	plist += index;

	if (list_empty(&pstat->hash_list)) {
		list_add_tail(&pstat->hash_list, plist);
		ret = 1;
	}

#ifdef SMP_SYNC
	SMP_UNLOCK_HASH_LIST(flags);
#endif

	return ret;
}

static inline int hash_list_del(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	int ret = 0;

#ifdef SMP_SYNC
	SMP_LOCK_HASH_LIST(flags);
#endif

	if (!list_empty(&pstat->hash_list)) {
		list_del_init(&pstat->hash_list);
		ret = 1;
	}

#ifdef SMP_SYNC
	SMP_UNLOCK_HASH_LIST(flags);
#endif

	return ret;
}


#if 1//defined(CONFIG_RTL_TRIBAND_SUPPORT)
#define STATIC_INLINE static inline
#else
#define STATIC_INLINE 
#endif





static inline int asoc_list_del(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	int ret = 0;

#if defined(CONFIG_PCI_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
#ifdef SMP_SYNC
		unsigned long flags = 0;
		int locked = 0;
	
		SMP_TRY_LOCK_ASOC_LIST(flags, locked);
#endif
		
		if (!list_empty(&pstat->asoc_list)) {
			list_del_init(&pstat->asoc_list);
			ret = 1;
		}
	
#ifdef SMP_SYNC
		if (locked)
			SMP_UNLOCK_ASOC_LIST(flags);
#endif
	}
#endif //CONFIG_PCI_HCI

#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	// The function returns whether it had linked to asoc_list before removing.
	if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
#ifdef SMP_SYNC
		unsigned long flags = 0;
#endif
		int wait = 0;
		
#ifdef __KERNEL__
		might_sleep();
#endif

#ifdef SMP_SYNC
		SMP_LOCK_ASOC_LIST(flags);
#endif
		
		if (0 != pstat->asoc_list_refcnt) {
			pstat->asoc_list_refcnt--;
			if (0 ==pstat->asoc_list_refcnt) {
	//			if (!list_empty(&pstat->asoc_list)) {
					list_del_init(&pstat->asoc_list);
					ret = 1;
	//			}
			} else {
				wait = 1;
			}
		}

#ifdef SMP_SYNC
		SMP_UNLOCK_ASOC_LIST(flags);
#endif
		
		if (wait) {
#ifdef __ECOS
			cyg_flag_wait(&pstat->asoc_unref_done, 0x01, CYG_FLAG_WAITMODE_OR | CYG_FLAG_WAITMODE_CLR);
#else
			wait_for_completion(&pstat->asoc_unref_done);
#endif
			ret = 1;
		}
}
#endif //CONFIG_USB_HCI || CONFIG_SDIO_HCI
#endif //CONFIG_RTL_TRIBAND_SUPPORT

	return ret;
}

STATIC_INLINE int asoc_list_add(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	int ret = 0;

#ifdef SMP_SYNC
	SMP_LOCK_ASOC_LIST(flags);
#endif
	
	if (list_empty(&pstat->asoc_list)) {
		list_add_tail(&pstat->asoc_list, &priv->asoc_list);
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
		if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
			pstat->asoc_list_refcnt = 1;
		}
#endif
		ret = 1;
	}

#ifdef SMP_SYNC
	SMP_UNLOCK_ASOC_LIST(flags);
#endif
	
	return ret;
}

static inline int auth_list_del(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	int ret = 0;

#ifdef SMP_SYNC
	SMP_LOCK_AUTH_LIST(flags);
#endif

	if (!list_empty(&pstat->auth_list)) {
		list_del_init(&pstat->auth_list);
		ret = 1;
	}

#ifdef SMP_SYNC
	SMP_UNLOCK_AUTH_LIST(flags);
#endif
	return ret;
}

static inline int auth_list_add(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	int ret = 0;

#ifdef SMP_SYNC
	SMP_LOCK_AUTH_LIST(flags);
#endif
	if (list_empty(&pstat->auth_list)) {
		list_add_tail(&pstat->auth_list, &priv->auth_list);
		ret = 1;
	}

#ifdef SMP_SYNC
	SMP_UNLOCK_AUTH_LIST(flags);
#endif

	return ret;
}

static inline int sleep_list_del(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	int ret = 0;

#ifdef SMP_SYNC
	SMP_LOCK_SLEEP_LIST(flags);
#endif

	if (!list_empty(&pstat->sleep_list)) {
		list_del_init(&pstat->sleep_list);
		ret = 1;
	}

#ifdef SMP_SYNC
	SMP_UNLOCK_SLEEP_LIST(flags);
#endif

	return ret;
}

static inline int sleep_list_add(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	int ret = 0;

#ifdef SMP_SYNC
	SMP_LOCK_SLEEP_LIST(flags);
#endif

	if (list_empty(&pstat->sleep_list)) {
		list_add_tail(&pstat->sleep_list, &priv->sleep_list);
		ret = 1;
	}

#ifdef SMP_SYNC
	SMP_UNLOCK_SLEEP_LIST(flags);
#endif

	return ret;
}

static inline int wakeup_list_del(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	int ret = 0;

#ifdef SMP_SYNC
	SMP_LOCK_WAKEUP_LIST(flags);
#endif

	if (!list_empty(&pstat->wakeup_list)) {
		list_del_init(&pstat->wakeup_list);
		ret = 1;
	}

#ifdef SMP_SYNC
	SMP_UNLOCK_WAKEUP_LIST(flags);
#endif

	return ret;
}

static inline int wakeup_list_add(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	int ret = 0;

#ifdef SMP_SYNC
	SMP_LOCK_WAKEUP_LIST(flags);
#endif

	if (list_empty(&pstat->wakeup_list)) {
		list_add_tail(&pstat->wakeup_list, &priv->wakeup_list);
		ret = 1;
	}

#ifdef SMP_SYNC
	SMP_UNLOCK_WAKEUP_LIST(flags);
#endif

	return ret;
}
#endif // CONFIG_PCI_HCI

#if (defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)) && !defined(CONFIG_RTL_TRIBAND_SUPPORT)
static inline int hash_list_add(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	unsigned int index;
	struct list_head *plist;
	int ret = 0;

#ifdef SMP_SYNC
	SMP_LOCK_HASH_LIST(flags);
#endif
	index = wifi_mac_hash(pstat->cmn_info.mac_addr);
	plist = priv->stat_hash;
	plist += index;

	if (list_empty(&pstat->hash_list)) {
		list_add_tail(&pstat->hash_list, plist);
		ret = 1;
	}

#ifdef SMP_SYNC
	SMP_UNLOCK_HASH_LIST(flags);
#endif

	return ret;
}

static inline int hash_list_del(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	int ret = 0;

#ifdef SMP_SYNC
	SMP_LOCK_HASH_LIST(flags);
#endif

	if (!list_empty(&pstat->hash_list)) {
		list_del_init(&pstat->hash_list);
		ret = 1;
	}

#ifdef SMP_SYNC
	SMP_UNLOCK_HASH_LIST(flags);
#endif

	return ret;
}

static inline int auth_list_del(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	int ret = 0;

#ifdef SMP_SYNC
	SMP_LOCK_AUTH_LIST(flags);
#endif

	if (!list_empty(&pstat->auth_list)) {
		list_del_init(&pstat->auth_list);
		ret = 1;
	}

#ifdef SMP_SYNC
	SMP_UNLOCK_AUTH_LIST(flags);
#endif

	return ret;
}

static inline int auth_list_add(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	int ret = 0;

#ifdef SMP_SYNC
	SMP_LOCK_AUTH_LIST(flags);
#endif

	if (list_empty(&pstat->auth_list)) {
		list_add_tail(&pstat->auth_list, &priv->auth_list);
		ret = 1;
	}

#ifdef SMP_SYNC
	SMP_UNLOCK_AUTH_LIST(flags);
#endif

	return ret;
}

static inline int sleep_list_del(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	int ret = 0;

#ifdef SMP_SYNC
	SMP_LOCK_SLEEP_LIST(flags);
#endif

	if (!list_empty(&pstat->sleep_list)) {
		list_del_init(&pstat->sleep_list);
		ret = 1;
	}

#ifdef SMP_SYNC
	SMP_UNLOCK_SLEEP_LIST(flags);
#endif

	return ret;
}

static inline int sleep_list_add(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	int ret = 0;

#ifdef SMP_SYNC
	SMP_LOCK_SLEEP_LIST(flags);
#endif

	if (list_empty(&pstat->sleep_list)) {
		list_add_tail(&pstat->sleep_list, &priv->sleep_list);
		ret = 1;
	}

#ifdef SMP_SYNC
	SMP_UNLOCK_SLEEP_LIST(flags);
#endif

	return ret;
}

static inline int wakeup_list_del(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	int ret = 0;

#ifdef SMP_SYNC
	SMP_LOCK_WAKEUP_LIST(flags);
#endif

	if (!list_empty(&pstat->wakeup_list)) {
		list_del_init(&pstat->wakeup_list);
		ret = 1;
	}

#ifdef SMP_SYNC
	SMP_UNLOCK_WAKEUP_LIST(flags);
#endif

	return ret;
}

static inline int wakeup_list_add(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	int ret = 0;

#ifdef SMP_SYNC
	SMP_LOCK_WAKEUP_LIST(flags);
#endif

	if (list_empty(&pstat->wakeup_list)) {
		list_add_tail(&pstat->wakeup_list, &priv->wakeup_list);
		ret = 1;
	}

#ifdef SMP_SYNC
	SMP_UNLOCK_WAKEUP_LIST(flags);
#endif

	return ret;
}
#endif // CONFIG_USB_HCI || CONFIG_SDIO_HCI


#define SET_PSEUDO_RANDOM_NUMBER(target)	{ \
	get_random_bytes(&(target), sizeof(target)); \
	target += (GET_MY_HWADDR[4] + GET_MY_HWADDR[5] + jiffies - priv->net_stats.rx_bytes \
	+ priv->net_stats.tx_bytes + priv->net_stats.rx_errors - priv->ext_stats.beacon_ok); \
}


#ifdef __ECOS
typedef void pr_fun(char *fmt, ...);
extern pr_fun *ecos_pr_fun;

#define PRINT_ONE(val, format, line_end) { \
	ecos_pr_fun(format, val); \
	if (line_end) 	\
		ecos_pr_fun("\n"); \
}

#define PRINT_ARRAY(val, format, len, line_end) { \
	int index; 	\
	for (index=0; index<len; index++) \
		ecos_pr_fun(format, val[index]); \
	if (line_end) 	\
		ecos_pr_fun("\n"); \
}
#else
#ifdef CONFIG_RTL_PROC_NEW
#define PRINT_ONE(val, format, line_end) { 		\
	seq_printf(s, format, val); \
	if (line_end) \
		seq_printf(s, "\n");		\
}

#define PRINT_ARRAY(val, format, len, line_end) { 	\
	int index;					\
	for (index=0; index<len; index++)		\
		seq_printf(s, format, val[index]); \
	if (line_end)					\
		seq_printf(s, "\n");		\
							\
}
#else
#define PRINT_ONE(val, format, line_end) { 		\
	pos += sprintf(&buf[pos], format, val);		\
	if (line_end)					\
		strcat(&buf[pos++], "\n");		\
}

#define PRINT_ARRAY(val, format, len, line_end) { 	\
	int index;					\
	for (index=0; index<len; index++)		\
		pos += sprintf(&buf[pos], format, val[index]); \
	if (line_end)					\
		strcat(&buf[pos++], "\n");		\
							\
}
#endif //LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
#endif

#define PRINT_SINGL_ARG(name, para, format) { \
	PRINT_ONE(name, "%s", 0); \
	PRINT_ONE(para, format, 1); \
}

#define PRINT_ARRAY_ARG(name, para, format, len) { \
	PRINT_ONE(name, "%s", 0); \
	PRINT_ARRAY(para, format, len, 1); \
}

#if (NUM_STAT >64)
#define PRINT_BITMAP_ARG(name, para) { \
    PRINT_ONE(name, "%s: ", 0); \
    PRINT_ONE(para._staMap_, "0x%08x", 1); \
    PRINT_ONE(name, "%s ext 1: ", 0); \
    PRINT_ONE(para._staMap_ext_1, "0x%08x", 1); \
    PRINT_ONE(name, "%s ext 2: ", 0); \
    PRINT_ONE(para._staMap_ext_2, "0x%08x", 1); \
    PRINT_ONE(name, "%s ext 3: ", 0); \
    PRINT_ONE(para._staMap_ext_3, "0x%08x", 1); \
}

#elif (NUM_STAT >32)
#define PRINT_BITMAP_ARG(name, para) { \
    PRINT_ONE(name, "%s: ", 0); \
    PRINT_ONE(para._staMap_, "0x%08x", 1); \
    PRINT_ONE(name, "%s ext 1: ", 0); \
    PRINT_ONE(para._staMap_ext_1, "0x%08x", 1); \
}
#else
#define PRINT_BITMAP_ARG(name, para) { \
    PRINT_ONE(name, "%s: ", 0); \
    PRINT_ONE(para._staMap_, "0x%08x", 1); \
}
#endif

#if defined(__ECOS) || defined(CONFIG_RTL_PROC_NEW)
#define CHECK_LEN do {} while(0)
#define CHECK_LEN_E do {} while(0)
#else
#define CHECK_LEN { \
	len += size; \
	pos = begin + len; \
	if (pos < offset) { \
		len = 0; \
		begin = pos; \
	} else { \
		if (len == size) { \
			len -= (offset-begin); \
			memcpy(buf, &buf[offset-begin], len); \
			begin = offset; \
		} \
		if (len > length) \
			goto _ret; \
	} \
}

#define CHECK_LEN_E { \
	len += size; \
	pos = begin + len; \
	if (pos < offset) { \
		len = 0; \
		begin = pos; \
	} else { \
		if (len == size) { \
			len -= (offset-begin); \
			memcpy(buf, &pTmp[offset-begin], len>length? length : len); \
			begin = offset; \
		} \
		else { \
			if (len > length) \
				memcpy(&buf[len-size], pTmp, size-(len-length)); \
			else \
				memcpy(&buf[len-size], pTmp, size); \
		} \
		if (len >= length) \
			goto _ret; \
	} \
}
#endif

#if defined(STA_ASSOC_STATISTIC) 
#define SECSPERMIN  		60
#define MINSPERHOUR 		60
#define SECSPERHOUR (SECSPERMIN * MINSPERHOUR)
#define SECSPERDAY  (SECSPERHOUR * HOURSPERDAY)
#define TM_YEAR_BASE    	1900
#endif

#define MOV_AVG(_tar, _val, _fac, _rat) \
	do { \
		if (_tar) \
			_tar = (((_tar)*((_fac)-(_rat))) + ((_val)*(_rat)))/(_fac); \
		else \
			_tar = _val; \
	} while (0)
		
#endif // _8192CD_UTIL_H_

