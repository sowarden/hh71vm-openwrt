/*
 *  Header file define some tx inline functions
 *
 *  Copyright (c) 2017 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef _8192CD_TX_H_
#define _8192CD_TX_H_

#ifndef WLAN_HAL_INTERNAL_USED


#include "./8192cd_cfg.h"
#include "./8192cd.h"
#include "./8192cd_util.h"
#include "./core/8192cd_core_tx.h"
extern int rtl8192cd_tx_clientMode(struct rtl8192cd_priv *priv, struct sk_buff **pskb);
extern int rtl8192cd_tx_wdsDevProc(struct rtl8192cd_priv *priv, struct sk_buff *skb, struct net_device **dev_p,
				struct net_device **wdsDev_p, struct tx_insn *txcfg);

enum _RECYCLE_CONDITION_{
    ERROR_STATE = 0,
    RECYCLE = 1,
    REPREPARE = 2,
    STOP_RECYCLE = 3
};



enum _TX_QUEUE_V1_ {
	MGNT_QUEUE_V1   = 0,
	BK_QUEUE_V1		= 1,
	BE_QUEUE_V1		= 2,
	VI_QUEUE_V1		= 3,
	VO_QUEUE_V1		= 4,
	HIGH_QUEUE_V1	= 5,
#if defined(CONFIG_WLAN_HAL)
	HIGH_QUEUE1_V1	= 6,
	HIGH_QUEUE2_V1	= 7,
	HIGH_QUEUE3_V1	= 8,
	HIGH_QUEUE4_V1	= 9,
	HIGH_QUEUE5_V1	= 10,
	HIGH_QUEUE6_V1	= 11,
	HIGH_QUEUE7_V1	= 12,
    HIGH_QUEUE8		= 13,
  	HIGH_QUEUE9		= 14,
	HIGH_QUEUE10	= 15,
    HIGH_QUEUE11	= 16,
    HIGH_QUEUE12	= 17,
    HIGH_QUEUE13	= 18,
    HIGH_QUEUE14	= 19,
    HIGH_QUEUE15	= 20,
	CMD_QUEUE_V1	= 21,	
	BEACON_QUEUE_V1	= 22
#endif
};

#if defined(CONFIG_NETDEV_MULTI_TX_QUEUE) || defined(CONFIG_SDIO_TX_FILTER_BY_PRI)
enum _NETDEV_TX_QUEUE_ {
	_NETDEV_TX_QUEUE_VO = 0,
	_NETDEV_TX_QUEUE_VI = 1,
	_NETDEV_TX_QUEUE_BE = 2,
	_NETDEV_TX_QUEUE_BK = 3,
	_NETDEV_TX_QUEUE_ALL
};
#endif





#ifdef CONFIG_PCI_HCI
// the purpose if actually just to link up all the desc in the same q
static __inline__ void init_txdesc(struct rtl8192cd_priv *priv, struct tx_desc *pdesc,
				unsigned long ringaddr, unsigned int i)
{
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
	if(GET_CHIP_VER(priv)== VERSION_8812E || GET_CHIP_VER(priv) == VERSION_8723B){
		if (i == (CURRENT_NUM_TX_DESC - 1))
			(pdesc + i)->Dword12 = set_desc(ringaddr); // NextDescAddress
		else
			(pdesc + i)->Dword12 = set_desc(ringaddr + (i+1) * sizeof(struct tx_desc)); // NextDescAddress
	} else
#endif
	{
	if (i == (CURRENT_NUM_TX_DESC - 1))
		(pdesc + i)->Dword10 = set_desc(ringaddr); // NextDescAddress
	else
		(pdesc + i)->Dword10 = set_desc(ringaddr + (i+1) * sizeof(struct tx_desc)); // NextDescAddress
	}

}



#endif // CONFIG_PCI_HCI



#endif //#ifndef WLAN_HAL_INTERNAL_USED



#endif // _8192CD_TX_H_

