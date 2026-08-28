/*
 *  RG handle routines
 *
 *  Copyright (c) 2018 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _8192CD_RG_C_

#include "rtk_rg_wlan.h"

#if defined(CONFIG_RG_WLAN_HWNAT_ACCELERATION) && !defined(CONFIG_ARCH_LUNA_SLAVE)
#ifdef CONFIG_RG_NETIF_RX_QUEUE_SUPPORT
void process_rg_netif_rx_queue(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	struct sk_buff *pskb;

	while (skb_queue_len(&priv->pshare->rg_netif_rx_queue) > 0) {
		pskb = skb_dequeue(&priv->pshare->rg_netif_rx_queue);
		if (pskb == NULL) {
			break;
		}
        rtk_rg_netif_rx(pskb);
	}
}
#endif // end of CONFIG_RG_NETIF_RX_QUEUE_SUPPORT

#if defined(CONFIG_RG_G3_SERIES) || defined(CONFIG_ARCH_RTL8198F)
	// directly call "rtk_rg_wlan_netDevice_set" in drivers/net/ethernet/realtek/rtl86900/romeDriver/rtk_rg_apollo_liteRomeDriver.c
#else // !defined(CONFIG_RG_G3_SERIES) && !defined(CONFIG_ARCH_RTL8198F)
// wlan0
struct net_device *wlan_root_netdev=NULL;
struct net_device *wlan_vap_netdev[RTL8192CD_NUM_VWLAN]={0};
struct net_device *wlan_wds_netdev[8]={0};
struct net_device *wlan_vxd_netdev=NULL;
#if !defined(CONFIG_RTK_L34_FC_KERNEL_MODULE) && defined(CONFIG_RTK_MESH) && (RG_RET_MBSSID_MASTER_MESH_INTF != RG_RET_MBSSID_MASTER_VAP3_INTF)
struct net_device *wlan_mesh_netdev=NULL;
#endif
// wlan1
struct net_device *wlan1_root_netdev=NULL;
struct net_device *wlan1_vap_netdev[RTL8192CD_NUM_VWLAN]={0};
struct net_device *wlan1_wds_netdev[8]={0};
struct net_device *wlan1_vxd_netdev=NULL;
#if 1//defined(CONFIG_RTL_USE_FC_GMAC2)
// wlan2
struct net_device *wlan2_root_netdev=NULL;
struct net_device *wlan2_vap_netdev[RTL8192CD_NUM_VWLAN]={0};
struct net_device *wlan2_wds_netdev[8]={0};
struct net_device *wlan2_vxd_netdev=NULL;
#endif

/* Function Name:
 *      rtk_rg_wlan_netDevice_set
 * Description:
 *      Set address of net device of wlan
 *      Refer to: drivers/net/ethernet/realtek/rtl86900/romeDriver/rtk_rg_apollo_liteRomeDriver.c
 * Input:
 *      wlanDevIdx  - wlan device index(refer to rtk_rg_mbssidDev_t)
 *      pDev        - address of net device of wlan
 * Output:
 *      None
 * Return:
 *      RT_ERR_RG_OK
 *      RT_ERR_RG_INDEX_OUT_OF_RANGE
 *      RT_ERR_RG_NULL_POINTER
 */
int rtk_rg_wlan_netDevice_set(rtk_rg_mbssidDev_t wlanDevIdx, struct net_device *pDev)
{
#if defined(CONFIG_RTL_USE_FC_GMAC2)
    if(!(RG_RET_MBSSID_MASTER_ROOT_INTF<=wlanDevIdx && wlanDevIdx<WLAN_DEVICE_NUM*3))
        return -1;
#else
    if(!(RG_RET_MBSSID_MASTER_ROOT_INTF<=wlanDevIdx && wlanDevIdx<WLAN_DEVICE_NUM*2))
        return -1;
#endif

    if(pDev==NULL)
        return -2;

    switch(wlanDevIdx)
    {
        // wlan0
        case RG_RET_MBSSID_MASTER_ROOT_INTF:
            wlan_root_netdev = pDev;
            //printk("Set net_device of wlan0 root");
            break;
        case RG_RET_MBSSID_MASTER_VAP0_INTF:
        case RG_RET_MBSSID_MASTER_VAP1_INTF:
        case RG_RET_MBSSID_MASTER_VAP2_INTF:
        case RG_RET_MBSSID_MASTER_VAP3_INTF:
#if defined(CONFIG_WLAN_MBSSID_NUM) && (CONFIG_WLAN_MBSSID_NUM==7)
        case RG_RET_MBSSID_MASTER_VAP4_INTF:
        case RG_RET_MBSSID_MASTER_VAP5_INTF:
        case RG_RET_MBSSID_MASTER_VAP6_INTF:
#endif
            wlan_vap_netdev[wlanDevIdx-RG_RET_MBSSID_MASTER_VAP0_INTF] = pDev;
            //printk("Set net_device of wlan0 vap%d", wlanDevIdx-RG_RET_MBSSID_MASTER_VAP0_INTF);
            break;
        case RG_RET_MBSSID_MASTER_WDS0_INTF:
        case RG_RET_MBSSID_MASTER_WDS1_INTF:
        case RG_RET_MBSSID_MASTER_WDS2_INTF:
        case RG_RET_MBSSID_MASTER_WDS3_INTF:
        case RG_RET_MBSSID_MASTER_WDS4_INTF:
        case RG_RET_MBSSID_MASTER_WDS5_INTF:
        case RG_RET_MBSSID_MASTER_WDS6_INTF:
#if !defined(CONFIG_RTK_L34_FC_KERNEL_MODULE) && defined(CONFIG_RTK_MESH) && (RG_RET_MBSSID_MASTER_MESH_INTF != RG_RET_MBSSID_MASTER_VAP3_INTF)
            wlan_wds_netdev[wlanDevIdx-RG_RET_MBSSID_MASTER_WDS0_INTF] = pDev;
            break;
        case RG_RET_MBSSID_MASTER_MESH_INTF:
            wlan_mesh_netdev = pDev;
			break;
#else
        case RG_RET_MBSSID_MASTER_WDS7_INTF:
            wlan_wds_netdev[wlanDevIdx-RG_RET_MBSSID_MASTER_WDS0_INTF] = pDev;
            //printk("Set net_device of wlan0 wds%d", wlanDevIdx-RG_RET_MBSSID_MASTER_WDS0_INTF);
            break;
#endif
#ifdef CONFIG_RTL_REPEATER_MODE_SUPPORT
        case RG_RET_MBSSID_MASTER_CLIENT_INTF:
            wlan_vxd_netdev = pDev;
            //printk("Set net_device of wlan0 vxd");
            break;
#endif
        // wlan1
        case RG_RET_MBSSID_SLAVE_ROOT_INTF:
            wlan1_root_netdev = pDev;
            //printk("Set net_device of wlan1 root");
            break;
        case RG_RET_MBSSID_SLAVE_VAP0_INTF:
        case RG_RET_MBSSID_SLAVE_VAP1_INTF:
        case RG_RET_MBSSID_SLAVE_VAP2_INTF:
        case RG_RET_MBSSID_SLAVE_VAP3_INTF:
#if defined(CONFIG_WLAN_MBSSID_NUM) && (CONFIG_WLAN_MBSSID_NUM==7)
        case RG_RET_MBSSID_SLAVE_VAP4_INTF:
        case RG_RET_MBSSID_SLAVE_VAP5_INTF:
        case RG_RET_MBSSID_SLAVE_VAP6_INTF:
#endif
            wlan1_vap_netdev[wlanDevIdx-RG_RET_MBSSID_SLAVE_VAP0_INTF] = pDev;
            //printk("Set net_device of wlan1 vap%d", wlanDevIdx-RG_RET_MBSSID_SLAVE_VAP0_INTF);
            break;
        case RG_RET_MBSSID_SLAVE_WDS0_INTF:
        case RG_RET_MBSSID_SLAVE_WDS1_INTF:
        case RG_RET_MBSSID_SLAVE_WDS2_INTF:
        case RG_RET_MBSSID_SLAVE_WDS3_INTF:
        case RG_RET_MBSSID_SLAVE_WDS4_INTF:
        case RG_RET_MBSSID_SLAVE_WDS5_INTF:
        case RG_RET_MBSSID_SLAVE_WDS6_INTF:
        case RG_RET_MBSSID_SLAVE_WDS7_INTF:
            wlan1_wds_netdev[wlanDevIdx-RG_RET_MBSSID_SLAVE_WDS0_INTF] = pDev;
            //printk("Set net_device of wlan1 wds%d", wlanDevIdx-RG_RET_MBSSID_SLAVE_WDS0_INTF);
            break;
#ifdef CONFIG_RTL_REPEATER_MODE_SUPPORT
        case RG_RET_MBSSID_SLAVE_CLIENT_INTF:
            wlan1_vxd_netdev = pDev;
            //printk("Set net_device of wlan1 vxd");
            break;
#endif
#if defined(CONFIG_RTL_USE_FC_GMAC2)
        // wlan2
        case RG_RET_MBSSID_THIRD_ROOT_INTF:
            wlan2_root_netdev = pDev;
            //printk("Set net_device of wlan2 root");
            break;
        case RG_RET_MBSSID_THIRD_VAP0_INTF:
        case RG_RET_MBSSID_THIRD_VAP1_INTF:
        case RG_RET_MBSSID_THIRD_VAP2_INTF:
        case RG_RET_MBSSID_THIRD_VAP3_INTF:
#if defined(CONFIG_WLAN_MBSSID_NUM) && (CONFIG_WLAN_MBSSID_NUM==7)
        case RG_RET_MBSSID_THIRD_VAP4_INTF:
        case RG_RET_MBSSID_THIRD_VAP5_INTF:
        case RG_RET_MBSSID_THIRD_VAP6_INTF:
#endif
            wlan2_vap_netdev[wlanDevIdx-RG_RET_MBSSID_THIRD_VAP0_INTF] = pDev;
            //printk("Set net_device of wlan2 vap%d", wlanDevIdx-RG_RET_MBSSID_THIRD_VAP0_INTF);
            break;
        case RG_RET_MBSSID_THIRD_WDS0_INTF:
        case RG_RET_MBSSID_THIRD_WDS1_INTF:
        case RG_RET_MBSSID_THIRD_WDS2_INTF:
        case RG_RET_MBSSID_THIRD_WDS3_INTF:
        case RG_RET_MBSSID_THIRD_WDS4_INTF:
        case RG_RET_MBSSID_THIRD_WDS5_INTF:
        case RG_RET_MBSSID_THIRD_WDS6_INTF:
        case RG_RET_MBSSID_THIRD_WDS7_INTF:
            wlan2_wds_netdev[wlanDevIdx-RG_RET_MBSSID_THIRD_WDS0_INTF] = pDev;
            //printk("Set net_device of wlan2 wds%d", wlanDevIdx-RG_RET_MBSSID_THIRD_WDS0_INTF);
            break;
#ifdef CONFIG_RTL_REPEATER_MODE_SUPPORT
        case RG_RET_MBSSID_THIRD_CLIENT_INTF:
            wlan2_vxd_netdev = pDev;
            //printk("Set net_device of wlan2 vxd");
            break;
#endif
#endif /* defined(CONFIG_RTL_USE_FC_GMAC2) */
        default:
            break;
    }

    return 0;
}
#endif // end of CONFIG_RG_G3_SERIES

void rtk_rg_netif_rx(struct sk_buff *pskb)
{
    extern int fwdEngine_wifi_rx(struct sk_buff *skb);
    enum {
        RE8670_RX_STOP=0,
        RE8670_RX_CONTINUE,
        RE8670_RX_STOP_SKBNOFREE,
        RE8670_RX_END
    };
    int ret;

#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
    if (pskb->dev->netdev_ops->ndo_start_xmit == rtl8192cd_start_xmit) {
        netif_rx(pskb);
        return;
    }
#endif

#if defined(CONFIG_RTL_USE_FC_MESH)
    if(pskb->dev->name[7]=='a' && pskb->dev->name[9]=='3') //use extension port of wlanX-vap3 for mesh FC, therefore wlanX-vap3 go to ps directly.
    {
        netif_rx(pskb);
        return;
    }
#endif

    pskb->data-=14;
    pskb->len+=14;
    //printk("[%s]\n",pskb->dev->name);
    ret=fwdEngine_wifi_rx(pskb);
    
    if(ret==RE8670_RX_CONTINUE)
    {
        pskb->data+=14;
        pskb->len-=14;
        //printk("WLAN0 rx, fwdEngine is handled, trap to netif_rx\n");
        netif_rx(pskb); 
    }
    else if(ret==RE8670_RX_STOP)
    {
        kfree_skb(pskb);
    }
}

#if defined(CONFIG_RTL_USE_FC_MESH)
#define WLAN_VAP_HWLOOKUP_SUPPORT_NUM 4
int is_mesh_dev(struct net_device *dev)
{
    int ret = 0;

#if defined(CONFIG_RTL_MESH_SINGLE_IFACE)
    if (dev == wlan_vap_netdev[WLAN_VAP_HWLOOKUP_SUPPORT_NUM-1])
        ret = 1;
#else
    if (dev == wlan_vap_netdev[WLAN_VAP_HWLOOKUP_SUPPORT_NUM-1])
        ret = 1;
    else if (dev == wlan1_vap_netdev[WLAN_VAP_HWLOOKUP_SUPPORT_NUM-1])
        ret = 1;
    #if defined(CONFIG_RTL_USE_FC_GMAC2)
    else if (dev == wlan2_vap_netdev[WLAN_VAP_HWLOOKUP_SUPPORT_NUM-1])
        ret = 1;
    #endif /* defined(CONFIG_RTL_USE_FC_GMAC2) */
#endif /* defined(CONFIG_RTL_MESH_SINGLE_IFACE) */
    return ret;
}
#endif /* defined(CONFIG_RTL_USE_FC_MESH) */

#endif  // end of defined(CONFIG_RG_WLAN_HWNAT_ACCELERATION) && !defined(CONFIG_ARCH_LUNA_SLAVE)

