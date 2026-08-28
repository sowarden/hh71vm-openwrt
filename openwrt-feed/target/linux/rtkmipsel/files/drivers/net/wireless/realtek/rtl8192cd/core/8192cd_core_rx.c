#define _8192CD_CORE_RX_C_

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <net/ip.h>
#include <linux/ipv6.h>
#include <linux/icmpv6.h>
#include <net/addrconf.h>   //__in6_dev_get


#elif defined(__ECOS)
#include <cyg/io/eth/rltk/819x/wrapper/sys_support.h>
#include <cyg/io/eth/rltk/819x/wrapper/skbuff.h>
#include <cyg/io/eth/rltk/819x/wrapper/timer.h>
#include <cyg/io/eth/rltk/819x/wrapper/wrapper.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#endif

#include "../8192cd_cfg.h"
#include "../8192cd.h"
#include "../8192cd_hw.h"
#include "../8192cd_headers.h"
#include "../8192cd_rx.h"
#include "./8192cd_core_rx.h"

#include "../8192cd_debug.h"

#if defined(__ECOS)
#if defined(CONFIG_RTL_OFFLOAD_DRIVER) && defined(CONFIG_WLAN_HAL_8814BE)
#include "./WlanHAL/RTL88XX/RTL8814B/Hal8814HRxDesc_core.h"
#endif
#endif


#ifdef __LINUX_2_6__
#ifdef CONFIG_RTL8672
#if !defined(CONFIG_OPENWRT_SDK)
#include "../romeperf.h"
#else
#include "../rtl_types.h"
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
#undef __MIPS16
#define __MIPS16
#endif
#else
#if !defined(NOT_RTK_BSP)
#ifdef CONFIG_OPENWRT_SDK
#include "../rtl_types.h" //mark_wrt
#else
#ifdef CONFIG_RTL_8198F
#include "../rtl_types.h"  //Charlie
#else
#include <net/rtl/rtl_types.h>
#endif
#undef __MIPS16
#if CONFIG_RTL_DISABLE_WLAN_MIPS16
#define __MIPS16			
#else
#ifdef __ECOS
#if defined(RTLPKG_DEVS_ETH_RLTK_819X_USE_MIPS16) || (!defined(CONFIG_RTL_8198C) && !defined(CONFIG_RTL_8197F))
#define __MIPS16			__attribute__ ((mips16))
#else
#define __MIPS16
#endif
#else
#if defined(CONFIG_WIRELESS_LAN_MODULE) || defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)   || (defined(CONFIG_RTL8672) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0)))
#define __MIPS16
#else
#define __MIPS16			__attribute__ ((mips16))
#endif
#endif
#endif
#endif
#endif
#endif
#endif

#if !defined(__ECOS) && !defined(CPTCFG_CFG80211_MODULE)
#include "HalPrecomp.h"
#else
#include "../HalPrecomp.h"
#endif


#ifdef _BROADLIGHT_FASTPATH_
extern int (*send_packet_to_upper_layer)(struct sk_buff *skb);
#endif

#ifdef PREVENT_ARP_SPOOFING
#include <linux/inetdevice.h>
#include <linux/if_arp.h>
static int check_arp_spoofing(struct rtl8192cd_priv *priv, struct sk_buff *pskb);
#endif


#if defined(CONFIG_RTL_WAPI_SUPPORT)
#include "wapiCrypto.h"
#endif
#if defined(CONFIG_RTL_FASTBRIDGE)
#include <net/rtl/features/fast_bridge.h>
#endif

#ifdef CONFIG_RTL867X_VLAN_MAPPING
#include "../../re_vlan.h"
#endif
#if defined (CONFIG_RTL_VLAN_8021Q) || defined (CONFIG_PUMA_VLAN_8021Q)
#include <linux/if_vlan.h>
#endif

#ifdef CONFIG_PUMA_UDMA_SUPPORT
#include <linux/udma_api.h>
#endif

#ifdef PERF_DUMP
#include "romeperf.h"
#endif

#ifdef PERF_DUMP_1074K
#include "m1074kctrl.c"
#endif

#ifdef __OSK__
#include "ifport.h"
#endif





#ifdef __ECOS
#define uint32 unsigned int
#define uint16 unsigned short
#define int16 short
#endif


//for 8671 IGMP snooping
#if defined(__OSK__) && defined(CONFIG_RTL8672)
extern int wlan_igmp_tag;
extern int enable_IGMP_SNP;
extern void check_IGMP_snoop_rx(CCB *ccb, int tag);
#ifdef CONFIG_RTL_92D_DMDP
extern struct port_map wlanDev[(RTL8192CD_NUM_VWLAN+2)*2];
#else
extern struct port_map wlanDev[RTL8192CD_NUM_VWLAN+2];		// Root(1)+vxd(1)+VAPs(4)
#endif

extern int enable_port_mapping;
#ifdef INET6
extern int enable_MLD_SNP;
extern void check_mld_snoop_rx(CCB * ccb, int tag);
#if defined(CONFIG_RTL8672)
extern unsigned int AvailableMemSize;  // 20100818 defined in oskmem.c,  bad declaration, but still here for bad include on OSK.
#endif
#else
#if defined(CONFIG_RTL8672)
extern unsigned int AvailableMemSize;  // 20100818 defined in oskmem.c,  bad declaration, but still here for bad include on OSK.
#endif
#endif

#elif defined(CONFIG_RTL8672)
#define wlan_igmp_tag 0x1f
extern int enable_IGMP_SNP;
#ifdef CONFIG_EXT_SWITCH
extern void check_IGMP_snoop_rx(struct sk_buff *skb, int tag);
#endif

#endif //CONFIG_RTL8672

#if 0/*def CONFIG_RTL_STP*/
unsigned char STPmac[6] = { 1, 0x80, 0xc2, 0,0,0};
static struct net_device* wlan_pseudo_dev;
#define WLAN_INTERFACE_NAME			"wlan0"
#endif

#if defined(__LINUX_2_6__) && defined(CONFIG_RTK_VLAN_SUPPORT)
extern int rtk_vlan_support_enable;
#endif
#ifdef CONFIG_RTL_VLAN_8021Q
extern int linux_vlan_enable;
extern linux_vlan_ctl_t *vlan_ctl_p;
#endif

#if defined(CONFIG_RTL_EAP_RELAY) || defined(CONFIG_RTK_INBAND_HOST_HACK)
extern unsigned char inband_Hostmac[]; //it's from br.c
#endif

#if defined(CONFIG_RTL_819X_ECOS)
extern unsigned char freebsd_Hostmac[];
#endif

#ifdef CONFIG_RTK_MESH
__DRAM_IN_865X extern unsigned char cached_mesh_mac[2][MACADDRLEN];
__DRAM_IN_865X extern struct net_device *cached_mesh_dev[2];
#endif

#if (defined(__ECOS) ||defined(__LINUX_2_6__)) && defined(RX_TASKLET)
#define	RTL_WLAN_RX_ATOMIC_PROTECT_ENTER	do {SAVE_INT_AND_CLI(x);} while(0)
#define	RTL_WLAN_RX_ATOMIC_PROTECT_EXIT		do {RESTORE_INT(x);} while(0)
#else
#define	RTL_WLAN_RX_ATOMIC_PROTECT_ENTER
#define	RTL_WLAN_RX_ATOMIC_PROTECT_EXIT
#endif

#ifdef DROP_RXPKT
/* For not occupyping CPU for too long so that other users (espeically those having real-time requirement) can obtain some CPU to run on-time */
volatile unsigned long G24_rx_bytes = 0;
volatile unsigned long G24_tx_bytes = 0;
#endif


#ifdef RX_BUFFER_GATHER
void flush_gathered_buff_list(struct rtl8192cd_priv *priv, struct list_head *phead)
{
	struct list_head *plist;
	struct rx_frinfo *pfrinfo;

	/*phead is in first skb's pfrinfo when called from flush_rx_list. so free in reverse*/
	plist = phead->prev;
	while (plist != phead) {
		pfrinfo = list_entry(plist, struct rx_frinfo, rx_list);
		plist = plist->prev;
		if (pfrinfo->pskb)
			rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
	}
	
	return;
}

void flush_rx_list(struct rtl8192cd_priv *priv)
{
	struct rx_frinfo *pfrinfo;
	struct list_head *phead, *plist;
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);

#ifdef RX_BUFFER_GATHER_REORDER
	phead = priv->pshare->gather_list_p;
	if(NULL == phead) {
		printk("error no buffer to flush\n");		
		RESTORE_INT(flags);
		return;
	}		
#else
	phead = &priv->pshare->gather_list;
#endif

	flush_gathered_buff_list(priv,phead);

#ifdef RX_BUFFER_GATHER_REORDER
	priv->pshare->gather_list_p=NULL;
#else
	INIT_LIST_HEAD(&priv->pshare->gather_list);
#endif

	RESTORE_INT(flags);
}

#ifdef RX_BUFFER_GATHER_REORDER
void flush_rx_list_inq(struct rtl8192cd_priv *priv,struct rx_frinfo *pfrinfo)
{
	unsigned long flags;
	SAVE_INT_AND_CLI(flags);
	flush_gathered_buff_list(priv,&pfrinfo->gather_list_head);
	RESTORE_INT(flags);
}
#endif

static int search_first_segment(struct rtl8192cd_priv *priv, struct rx_frinfo **found)
{
	struct rx_frinfo *pfrinfo, *first = NULL;
	struct list_head *phead, *plist;
	int len = 0, look_for = GATHER_FIRST, ok = 0;
#ifndef SMP_SYNC
	unsigned long flags;
#endif
	unsigned char *pframe;
	u8 qosControl[4];

	SAVE_INT_AND_CLI(flags);

#ifdef RX_BUFFER_GATHER_REORDER
	phead = priv->pshare->gather_list_p;
#else
	phead = &priv->pshare->gather_list;
#endif
	plist = phead->next;

	while (phead && plist != phead) {
		pfrinfo = list_entry(plist, struct rx_frinfo, rx_list);
		plist = plist->next;

		if (pfrinfo->gather_flag & look_for) {
			len += pfrinfo->pktlen;
			if (pfrinfo->gather_flag & GATHER_FIRST) {
				first = pfrinfo;
				list_del_init(&pfrinfo->rx_list);

				if (pfrinfo->pskb) {
					pframe = get_pframe(pfrinfo);
					memcpy(qosControl, GetQosControl(pframe), 2);
					if (!is_qos_data(pframe)  || !(qosControl[0] & BIT(7))) {	/* not AMSDU */
						rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);			
						pfrinfo->pskb = NULL;
					} else {
						look_for = GATHER_MIDDLE | GATHER_LAST;
					}
				}
			}
			else if (pfrinfo->gather_flag & GATHER_LAST) {
#ifdef CONFIG_WLAN_HAL
                if (IS_HAL_CHIP(priv)) {
                    first->gather_len = len - _CRCLNG_;
                } else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif
                {//not HAL
                    first->gather_len = len;
                }
				*found = first;
				ok = 1;
				break;
			}
		}
		else {
			list_del_init(&pfrinfo->rx_list);
			if (pfrinfo->pskb)
				rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
		}
	}

	if (first && !ok) {
		if (first->pskb)
			rtl_kfree_skb(priv, first->pskb, _SKB_RX_);
	}

	RESTORE_INT(flags);
	return ok;
}

#endif /* RX_BUFFER_GATHER */





void rx_pkt_exception(struct rtl8192cd_priv *priv, unsigned int cmd)
{
	struct net_device_stats *pnet_stats = &(priv->net_stats);

	if (cmd & RX_CRC32)
	{
		pnet_stats->rx_crc_errors++;
		pnet_stats->rx_errors++;
		SNMP_MIB_INC(dot11FCSErrorCount, 1);
	}
	else if (cmd & RX_ICVERR)
	{
#ifdef RX_CRC_EXPTIMER
        priv->ext_stats.rx_crc_exptimer++;
#endif
		pnet_stats->rx_errors++;
		SNMP_MIB_ASSIGN(dot11WEPICVErrorCount, 1);
	}
	else
	{
	}
}


#ifdef  CONFIG_WLAN_HAL
__IRAM_IN_865X
static void
rx_pkt_exception_88XX(
    struct rtl8192cd_priv   *priv, 
    BOOLEAN                 bRX_CRC32,
    BOOLEAN                 bRX_ICVERR,
    u1Byte					RX_RATE
)
{
	struct net_device_stats *pnet_stats = &(priv->net_stats);
#ifdef RX_CRC_EXPTIMER
	u1Byte rate;
#endif

    priv->ext_stats.rx_reuse++;
	//printk("%s(%d), reuse(0x%x)\n", __FUNCTION__, __LINE__, (unsigned int)priv->ext_stats.rx_reuse);

	if ( bRX_CRC32 )
	{
#ifdef RX_CRC_EXPTIMER
        if (RX_RATE < 12) {
            rate = dot11_rate_table[RX_RATE];
        } else if(RX_RATE < 44) {
            rate = HT_RATE_ID + (RX_RATE - 12);
        } else{
            rate = VHT_RATE_ID + (RX_RATE - 44);
        }
        priv->ext_stats.rx_crc_exptimer++;
		priv->ext_stats.rx_crc_by_rate[rate]++;
#endif
		pnet_stats->rx_crc_errors++;
		pnet_stats->rx_errors++;
		SNMP_MIB_INC(dot11FCSErrorCount, 1);
	}
	else if ( bRX_ICVERR )
	{
		pnet_stats->rx_errors++;
		SNMP_MIB_ASSIGN(dot11WEPICVErrorCount, 1);
	}
	else
	{
	}
}
#endif  //CONFIG_WLAN_HAL

BOOLEAN
CheckRxPktLenReuse(
    struct rtl8192cd_priv   *priv,
	struct rx_frinfo        *pfrinfo,
	PRX_DESC_STATUS_88XX     prx_desc_status
)
{
    // TODO: 0x2d00 is 11520, but MAX AMSDU is 11454 (0x2CBE)
    //3 Check Upper bound length
    // 1.) Check older than 11AC
    if (!(priv->pmib->dot11BssType.net_work_type & WIRELESS_11AC)) {
        if (pfrinfo->pktlen > 0x2000) {
            return TRUE;
        }
    }

    // 2.) Check 11AC    
    if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11AC) {
        if (pfrinfo->pktlen > 0x2d00) {
            return TRUE;
        }
    }

    //3 Check Low bound length
    if (prx_desc_status->C2HPkt) {
        // TODO: check if there is low bound length for C2H packet length
    }
    else {
        // Normal RX packet
        if (pfrinfo->pktlen < 16) {

#if defined(RX_BUFFER_GATHER)
            if (pfrinfo->gather_flag != GATHER_LAST) {
                return TRUE;
            }
#else
            return TRUE;
#endif //defined(RX_BUFFER_GATHER)
        }
    }

    return FALSE;
}


#ifdef __OSK__
__IRAM_WIFI_PRI3
#else
__MIPS16
__IRAM_IN_865X
#endif
void
validate_C2HPkt(
    struct rtl8192cd_priv   *priv, 
    struct rx_frinfo        *pfrinfo
)
{
#ifdef CONFIG_WLAN_HAL_8881A
    if ((GET_CHIP_VER(priv) == VERSION_8881A)) {
        C2HPacketHandler_8881A(priv, get_pframe(pfrinfo), pfrinfo->pktlen); // no CRC in C2H_PACKET
    }
    else
#endif
    GET_HAL_INTERFACE(priv)->C2HPacketHandler(priv,get_pframe(pfrinfo), pfrinfo->pktlen); // no CRC in C2H_PACKET
    // TODO: Parsing C2H Packet
}



static int rtl8192cd_rx_procNullPkt(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo
#ifdef MBSSID
				,int vap_idx
#endif
				)
{
	unsigned char *sa = pfrinfo->sa;
	struct stat_info *pstat = NULL;
	unsigned char *pframe = get_pframe(pfrinfo);
#ifdef SMP_SYNC
	unsigned long x;
#endif

#ifdef HW_FILL_MACID
    pstat = get_stainfo_fast(priv,sa,pfrinfo->macid);
#else
    pstat = get_stainfo(priv,sa);
#endif //#ifdef HW_FILL_MACID

    

#ifdef UNIVERSAL_REPEATER
	if ((pstat == NULL) && IS_DRV_OPEN(GET_VXD_PRIV(priv))) {

#ifdef HW_FILL_MACID
        pstat = get_stainfo_fast(GET_VXD_PRIV(priv),sa,pfrinfo->macid);
#else
        pstat = get_stainfo(GET_VXD_PRIV(priv),sa);
#endif //#ifdef HW_FILL_MACID

		if (pstat)
			priv = GET_VXD_PRIV(priv);
	}
#endif

#ifdef MBSSID
	if ((pstat == NULL)
		&& GET_ROOT(priv)->pmib->miscEntry.vap_enable
		&& (vap_idx >= 0)) {

#ifdef HW_FILL_MACID
        pstat = get_stainfo_fast(priv->pvap_priv[vap_idx], sa,pfrinfo->macid);
#else
        pstat = get_stainfo(priv->pvap_priv[vap_idx],sa);
#endif //#ifdef HW_FILL_MACID



		if (pstat)
			priv = priv->pvap_priv[vap_idx];
	}
#endif

	if (pstat && (pstat->state & WIFI_ASOC_STATE)
			&& (!(OPMODE & WIFI_AP_STATE) || IS_BSSID(priv, GetAddr1Ptr(pframe)))) {
#ifdef DRVMAC_LB
		rx_sum_up(priv, pstat, pfrinfo);
#else
		rx_sum_up(NULL, pstat, pfrinfo);
#endif
		update_sta_rssi(priv, pstat, pfrinfo);

#ifdef DETECT_STA_EXISTANCE
#ifdef CONFIG_RTL_88E_SUPPORT
        if (GET_CHIP_VER(priv)==VERSION_8188E) {
            if (pstat->leave!= 0)
                RTL8188E_MACID_NOLINK(priv, 0, REMAP_AID(pstat));
        }
#endif
#ifdef CONFIG_WLAN_HAL
        if(IS_HAL_CHIP(priv))
        {
            if (pstat->leave!= 0)
            {
#if defined(CONFIG_PCI_HCI)
				if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
#if defined(TAROKO_0)
                    HAL_H2C_MSRRPT_PARA MSRRPT_para;
                    MSRRPT_para.macid = pstat->cmn_info.mac_id;
                    MSRRPT_para.opmode = INCREASE;

                    GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, &MSRRPT_para);
#else
    				GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, pstat, INCREASE);
#endif                   
	                pstat->bDrop = 0;
				}
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
				if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
                	notify_update_sta_msr(priv, pstat, INCREASE);
				}
#endif
                pstat->cmn_info.dm_ctrl |= STA_DM_CTRL_ACTIVE;
            }
            pstat->rx_last_good_time = priv->up_time;
        }
#endif //#ifdef CONFIG_WLAN_HAL

		pstat->leave = 0;
#endif

		// check power save state
#ifndef DRVMAC_LB
		if (OPMODE & WIFI_AP_STATE) {
			pwr_state(priv, pfrinfo);
		}
#endif
	}

#ifdef MULTI_MAC_CLONE
	int idx = mclone_find_address(priv, pfrinfo->sa, NULL, MAC_CLONE_SA_FIND);
	if (idx > 0)
	{
		struct rtl8192cd_priv *priv_backup;
		struct stat_info *pstat_backup;
		priv_backup = priv;
		pstat_backup = pstat;
		priv = priv->pshare->mclone_sta[idx-1].priv;
		pstat = get_stainfo(priv, BSSID);
		if (pstat)
		{
			ACTIVE_ID = idx;
			issue_NullData(priv, pstat->cmn_info.mac_addr);
		}
		priv = priv_backup;
		pstat = pstat_backup;
	}
#endif

#if defined(WIFI_WMM) && defined(WMM_APSD)
	if ((QOS_ENABLE)
#ifndef DRVMAC_LB
		&& (APSD_ENABLE)
#endif
		&& (OPMODE & WIFI_AP_STATE) && pstat
#ifndef DRVMAC_LB
		&& (pstat->state & WIFI_SLEEP_STATE)
#endif
		&& (pstat->QosEnabled)
#ifndef DRVMAC_LB
		&& (pstat->apsd_bitmap & 0x0f)
#endif
		&& ((GetFrameSubType(pframe)) == (WIFI_DATA_NULL | BIT(7)))) {
		unsigned char *bssid = GetAddr1Ptr(pframe);
		if (IS_BSSID(priv, bssid))
		{
#ifdef RTL8190_DIRECT_RX
			rtl8192cd_rx_dataframe(priv, NULL, pfrinfo);
#else
			SMP_LOCK_RX_DATA(x);
			list_add_tail(&(pfrinfo->rx_list), &(priv->rx_datalist));
			SMP_UNLOCK_RX_DATA(x);
			rtl_atomic_inc(&priv->pshare->rx_enqueue_cnt);
#endif
			return 0;
		}
	}
#endif

        /*special case for mobile wifi. the mobile insleep  will send 
         null data to AP after AP's reboot. if not deauthed, the client will not reconnect*/
        if(pstat == NULL) {
#if defined(CONFIG_RTL_SIMPLE_CONFIG)
	if ((priv->pmib->dot11StationConfigEntry.sc_enabled == 1) && (priv->simple_config_status !=0) && IS_MCAST(pfrinfo->da))
	{
		if (priv->pmib->dot11StationConfigEntry.sc_status <=4) {
			if (get_sc_smac(pfrinfo) == 1) {
				return 1;
			}
		}
	}
#endif
		if (OPMODE & WIFI_AP_STATE) {
#if defined(CONFIG_RTL_SIMPLE_CONFIG)
			if (get_sc_smac(pfrinfo) == 1) {
				//panic_printk("D %02X:%02X:%02X:%02X:%02X:%02X\n",pfrinfo->da[0], pfrinfo->da[1], pfrinfo->da[2], pfrinfo->da[3], pfrinfo->da[4], pfrinfo->da[5]);
				//panic_printk("S %02X:%02X:%02X:%02X:%02X:%02X\n",pfrinfo->sa[0], pfrinfo->sa[1], pfrinfo->sa[2], pfrinfo->sa[3], pfrinfo->sa[4], pfrinfo->sa[5]);
				return 1;
			}
#endif			
			if (IS_BSSID(priv, GetAddr1Ptr(pframe)) && deauth_filter(priv, GetAddr2Ptr(pframe))) {
				DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_SERIOUS, "issue_deauth - %02X:%02X:%02X:%02X:%02X:%02X, Reason: _RSON_CLS3_\n",
								pfrinfo->sa[0],pfrinfo->sa[1],pfrinfo->sa[2],pfrinfo->sa[3],pfrinfo->sa[4],pfrinfo->sa[5]);
				issue_deauth(priv, GetAddr2Ptr(pframe), _RSON_CLS3_);
                //issue_disassoc(priv, GetAddr2Ptr(pframe), _RSON_CLS3_);
            }
        }
        DEBUG_ERR("RX DROP: class 3 error (null packet)!\n");
		return 1;
	}
	else if(pstat->prepare_to_free) {
		DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_SERIOUS, "issue_deauth - %02X:%02X:%02X:%02X:%02X:%02X, Reason: _RSON_DISAOC_STA_LEAVING_\n",
								pfrinfo->sa[0],pfrinfo->sa[1],pfrinfo->sa[2],pfrinfo->sa[3],pfrinfo->sa[4],pfrinfo->sa[5]);
		issue_deauth(priv,GetAddr2Ptr(pframe), _RSON_DISAOC_STA_LEAVING_);
		return 1;
	}

	// for AR5007 IOT ISSUE
	if ((!GetPwrMgt(pframe)) && (GetTupleCache(pframe) == 0) // because this is special case for AR5007, so use GetTupleCache with Seq-Num and Frag-Num, GetSequenceis also ok
		 && (OPMODE & WIFI_AP_STATE) && (IS_BSSID(priv, GetAddr1Ptr(pframe))) 
		 && (((GetFrameSubType(pframe)) == WIFI_DATA_NULL)
#ifdef WIFI_WMM
		 || ((QOS_ENABLE) && ((GetFrameSubType(pframe)) == (WIFI_DATA_NULL | BIT(7))))
#endif 
		 )
	) {
#ifdef RTL8190_DIRECT_RX
		rtl8192cd_rx_dataframe(priv, NULL, pfrinfo);
#else
		SMP_LOCK_RX_DATA(x);
		list_add_tail(&(pfrinfo->rx_list), &(priv->rx_datalist));
		SMP_UNLOCK_RX_DATA(x);
		rtl_atomic_inc(&priv->pshare->rx_enqueue_cnt);
#endif
		DEBUG_INFO("special Null Data in%d \n", pfrinfo->tid);
		return 0;
	}

	return 1;
}

// for AR5007 IOT ISSUE
static void rtl8192cd_rx_handle_Spec_Null_Data(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
	unsigned char *sa;
	struct stat_info *pstat = NULL;
	unsigned char *pframe = get_pframe(pfrinfo);
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);
	sa = pfrinfo->sa;

#ifdef HW_FILL_MACID
        pstat = get_stainfo_fast(priv,sa,pfrinfo->macid);
#else
        pstat = get_stainfo(priv,sa);
#endif //#ifdef HW_FILL_MACID	

	if (pstat==NULL) {
		goto out;
	}

	pframe = get_pframe(pfrinfo);
	if ((!GetPwrMgt(pframe)) && (GetTupleCache(pframe) == 0) // because this is special case for AR5007, so use GetTupleCache with Seq-Num and Frag-Num, GetSequenceis also ok
		 && (OPMODE & WIFI_AP_STATE) && (IS_BSSID(priv, GetAddr1Ptr(pframe))))
	{
		int i,j;

		DEBUG_INFO("tpcache should be reset: %d\n", pfrinfo->tid);
		for (i=0; i<8; i++)
			for (j=0; j<TUPLE_WINDOW; j++)
				pstat->tpcache[i][j] = 0xffff;
	}
out:
	RESTORE_INT(flags);
}


#if defined(WIFI_WMM) && defined(WMM_APSD)
static void process_qos_null(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
	unsigned char *pframe;
	struct stat_info *pstat = NULL;

	pframe  = get_pframe(pfrinfo);

#ifdef HW_FILL_MACID
        pstat = get_stainfo_fast(priv,get_sa(pframe),pfrinfo->macid);
#else
        pstat = get_stainfo(priv,get_sa(pframe));
#endif //#ifdef HW_FILL_MACID



	if ((!(OPMODE & WIFI_AP_STATE)) || (pstat == NULL)) {
		return;
	}

	if (pstat->apsd_bitmap & 0x0f)
		process_APSD_dz_queue(priv, pstat, pfrinfo->tid);
}
#endif


#ifdef RX_BUFFER_GATHER
static struct sk_buff *get_next_skb(struct rtl8192cd_priv *priv, int remove, int *is_last
#ifdef RX_BUFFER_GATHER_REORDER
, struct list_head *phead_list
#endif
)
{
	struct rx_frinfo *pfrinfo = NULL;
	struct sk_buff *pskb = NULL;
	struct list_head *phead, *plist;
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);

#if defined(RX_BUFFER_GATHER_REORDER)
	phead = phead_list;
#else
	phead = &priv->pshare->gather_list;
#endif
	plist = phead->next;

	if (plist != phead) {
		pfrinfo = list_entry(plist, struct rx_frinfo, rx_list);
		pskb = get_pskb(pfrinfo);
		if (pskb) {
			skb_set_tail_pointer(pskb, pfrinfo->pktlen);
			pskb->len = pfrinfo->pktlen;
			pskb->dev = priv->dev;
			if (remove)
				list_del_init(plist);
		}
	}

	if (is_last && pskb && pfrinfo->gather_flag == GATHER_LAST)
		*is_last = 1;

	RESTORE_INT(flags);
	return pskb;
}

static struct sk_buff *shift_padding_len(struct rtl8192cd_priv *priv, struct sk_buff *skb, int len, int *is_last
#ifdef RX_BUFFER_GATHER_REORDER
, struct list_head *phead
#endif
	)
{
	struct sk_buff *nskb= skb ;

	if (skb->len < len) {
		if (*is_last)
			return NULL;

#ifdef RX_BUFFER_GATHER_REORDER		
		nskb = get_next_skb(priv, 1, is_last, phead);
#else
		nskb = get_next_skb(priv, 1, is_last);
#endif

		if (nskb)
			skb_pull(nskb, len - skb->len);
		else
			DEBUG_ERR("Shift len error (%d, %d)!\n", skb->len, len);
	}
	else
		skb_pull(nskb, len);

	return nskb;
}

static int get_subframe_len(struct rtl8192cd_priv *priv, struct sk_buff *skb, int is_last
#ifdef RX_BUFFER_GATHER_REORDER
, struct list_head*	phead_list
#endif
	)
{
	u8 sub_len[2];
	struct sk_buff *nskb;
	int offset;
	u16 u16_len;

	if (skb->len < WLAN_ETHHDR_LEN) {
		if (is_last)
			return 0;

		if (skb->len == WLAN_ETHHDR_LEN -1) {
			sub_len[0] = skb->data[MACADDRLEN*2];
			offset = 1;
		}
		else
			offset = WLAN_ETHHDR_LEN -2 - skb->len;
#ifdef RX_BUFFER_GATHER_REORDER		
		nskb = get_next_skb(priv, 0, NULL, phead_list);
#else
		nskb = get_next_skb(priv, 0, NULL);
#endif
		if (nskb == NULL)
			return -1;

		if (offset == 1)
			sub_len[1] = nskb->data[0];
		else
			memcpy(sub_len, nskb->data + offset, 2);
	}
	else
		memcpy(sub_len, &skb->data[MACADDRLEN*2], 2);

	u16_len = ntohs(*((u16 *)sub_len));
	return ((int)u16_len);
}

static struct sk_buff *get_subframe(struct rtl8192cd_priv *priv, struct sk_buff *skb, struct sk_buff **orgskb, int len, int *is_last
#ifdef RX_BUFFER_GATHER_REORDER
, struct list_head *phead
#endif
)
{
	struct sk_buff *nextskb=NULL, *joinskb;
	int offset, copy_len;

	if (skb->len < len+WLAN_ETHHDR_LEN) {
		int rest_len = len + WLAN_ETHHDR_LEN - skb->len;

		if (*is_last)
			return NULL;

		joinskb = dev_alloc_skb(len + WLAN_ETHHDR_LEN);
		if (joinskb == NULL) {
			DEBUG_ERR("dev_alloc_skb() failed!\n");
			return NULL;
		}
		memcpy(joinskb->data, skb->data, skb->len);
		offset = skb->len;

		do {
			if (nextskb)
				rtl_kfree_skb(priv, nextskb, _SKB_RX_);

#ifdef RX_BUFFER_GATHER_REORDER
			nextskb = get_next_skb(priv, 1, is_last, phead);
#else
			nextskb = get_next_skb(priv, 1, is_last);
#endif
			if (nextskb == NULL) {
				dev_kfree_skb_any(joinskb);
				return NULL;
			}

			if (nextskb->len < rest_len && *is_last) {
				dev_kfree_skb_any(joinskb);
				rtl_kfree_skb(priv, nextskb, _SKB_RX_);
				return NULL;
			}
			if (nextskb->len < rest_len)
				copy_len = nextskb->len;
			else
				copy_len = rest_len;

			memcpy(joinskb->data+offset, nextskb->data, copy_len);
			rest_len -= copy_len;
			offset += copy_len;
			skb_pull(nextskb, copy_len);
		}while (rest_len > 0);
		rtl_kfree_skb(priv, *orgskb, _SKB_RX_);
		*orgskb = nextskb;
		skb = joinskb;
	}
	else
		skb_pull(*orgskb, len+WLAN_ETHHDR_LEN);

	return skb;
}
#endif /* RX_BUFFER_GATHER */


#if defined(BR_SHORTCUT)
int rtl_IsMcastIP(struct sk_buff *pskb)
{
	int ret=0;
	unsigned short L3_protocol;
	L3_protocol = *(unsigned short *)(pskb->data+ETH_ALEN*2);
	
	if( L3_protocol == __constant_htons(0x0800) )
	{
		unsigned int DaIpAddr;
		struct iphdr* iph = (struct iphdr *)(pskb->data + ETH_HLEN);
		
		DaIpAddr = ntohl(iph->daddr);
		//panic_printk("[%s]:[%d],pskb:%x.\n",__FUNCTION__,__LINE__,DaIpAddr);

		if((DaIpAddr & 0xF0000000) == 0xE0000000)  
		{
			ret=1;
		}	
	}
	else if(L3_protocol == __constant_htons(0x86dd)) 
	{	
	#ifdef __ECOS
		struct ip6_hdr *iph;
		iph  = (struct ip6_hdr *)(pskb->data + ETH_HLEN);
	#else
		struct ipv6hdr *iph;
		iph  = (struct ipv6hdr *)(pskb->data + ETH_HLEN);
	#endif
		/*ip(v6) format is  multicast ip*/	
	#ifdef __ECOS
		if (iph->ip6_dst.s6_addr[0] == 0xff)
	#else		
		if (iph->daddr.s6_addr[0] == 0xff)
	#endif		
		{
			ret=1;
		}
	}
	
	//panic_printk("[%s]:[%d],ret:%x,pskb:%x-%x-%x-%x-%x-%x.\n",__FUNCTION__,__LINE__,ret,pskb->data[0],pskb->data[1],pskb->data[2],pskb->data[3],pskb->data[4],pskb->data[5]);
	return ret;
}
#endif

#if defined(CONFIG_RTL_EXT_PORT_SUPPORT)
extern int rtl_check_ext_port_napt_entry(uint32 sip, uint16 sport, uint32 dip, uint16 dport, uint8 protocol);
extern unsigned int _br0_ip;
extern unsigned int _br0_mask;
extern int gHwNatEnabled;
extern int extPortEnabled;
extern unsigned int statistic_wlan_xmit_to_eth;
extern unsigned int statistic_wlan_out_napt_exist;
extern unsigned int statistic_wlan_rx_unicast_pkt;
extern struct iphdr * rtl_get_ipv4_header(uint8 *macFrame);
extern int rtl_wlan_frame_use_hw_nat(uint8 *data);
#endif


#if defined(CONFIG_RTL_8196E) && (!defined(__OSK__)) || (defined(__OSK__) && !defined(CONFIG_RTL6028))
__MIPS16
#endif
__IRAM_IN_865X
void rtl_netif_rx(struct rtl8192cd_priv *priv, struct sk_buff *pskb, struct stat_info *pstat)
{
#ifdef CLIENT_MODE
#ifdef SUPPORT_RX_UNI2MCAST
	unsigned short L3_protocol;
	unsigned char *DA_START;
#endif
#ifdef VIDEO_STREAMING_REFINE
	// for video streaming refine
	extern struct net_device *is_eth_streaming_only(struct sk_buff *skb);
	struct net_device *dev;
#endif
#endif
	struct vlan_info *vlan=NULL;
#ifdef CONFIG_RTL_VLAN_8021Q
        int index;
#endif

#if defined(CONFIG_RTL_CUSTOM_PASSTHRU)
	unsigned char dest_mac[6];
#endif

#if defined(__KERNEL__) || defined(__OSK__)
	struct net_bridge_port *br_port=NULL;
	br_port = GET_BR_PORT(priv->dev);
#endif

#ifdef CONFIG_POWER_SAVE
		rtw_ap_ps_recv_monitor(priv);
#endif

#if defined(SUPPORT_RX_AMSDU_AMPDU)
		pskb->truesize = pskb->len + 432;
#endif	
#ifdef PREVENT_BROADCAST_STORM
    if ((OPMODE & WIFI_AP_STATE) && ((unsigned char)pskb->data[0] == 0xff) && pstat) {
        if (pstat->rx_pkts_bc > BROADCAST_STORM_THRESHOLD) {
            priv->ext_stats.rx_data_drops++;
            DEBUG_ERR("RX DROP: Broadcast storm happened!\n");
            rtl_kfree_skb(priv, pskb, _SKB_RX_);
            return;
        }
    }
#endif

	if(priv->pshare->rf_ft_var.drop_multicast && IS_MCAST(pskb->data) && ((unsigned char)pskb->data[0] != 0xff) ) {
		priv->ext_stats.rx_data_drops++;
		rtl_kfree_skb(priv, pskb, _SKB_RX_);
		return;
	}
#ifdef CONFIG_RECORD_CLIENT_HOST
		extern void client_host_snooping_bydhcp(struct sk_buff *pskb, struct rtl8192cd_priv *priv); 
	
		if(priv->pmib->miscEntry.client_host_sniffer_enable && isDHCPpkt(pskb)) {
			client_host_snooping_bydhcp(pskb, priv);
		}
#endif

#ifdef TDLS_SUPPORT
	unsigned char *pDa = pskb->data;
	unsigned char *pSa = pskb->data + ETH_ALEN;
	unsigned short ether_type = *(unsigned short *)(pSa +ETH_ALEN);
	
	if(ether_type == __constant_htons(ETHER_TDLS)) {
		unsigned char *pdata = pskb->data + ETH_HLEN; // DA+SA+Type
		if(!priv->pmib->dot11OperationEntry.tdls_prohibited) {	
			unsigned int pkt_len = pskb->len; 
			//priv->tdls_disabled = priv->pmib->dot11OperationEntry.tdls_prohibited;
			//int rval = tdls_on_rx_action(priv, pdata, pDa, pSa, pkt_len);
			//panic_printk("TDLS: rval=%d\n", rval);
        }
		rtl_kfree_skb(priv, pskb, _SKB_RX_);
		return;		
	}
#endif
	
#ifdef CONFIG_RTK_VLAN_WAN_TAG
	extern int rtl865x_same_root(struct net_device *dev1,struct net_device *dev2);
#endif

#if defined(CONFIG_RTL_CUSTOM_PASSTHRU)
	memcpy(dest_mac, pskb->data, 6);
#endif

#if defined(CONFIG_RTL_CUSTOM_PASSTHRU)
#ifdef __ECOS
	if (SUCCESS==rtl_isWlanPassthruFrame(pskb->data))
#else
	if (SUCCESS==rtl_isPassthruFrame(pskb->data))
#endif
	{
#ifdef CLIENT_MODE
#if defined(CONFIG_RTL_CUSTOM_PASSTHRU)
	if(priv &&(((GET_MIB(priv))->dot11OperationEntry.opmode)& WIFI_STATION_STATE)&&
		(compare_ether_addr((char* )priv->dev->dev_addr, (char*)dest_mac)))
#else
	if(priv &&((GET_MIB(priv))->dot11OperationEntry.opmode)& WIFI_STATION_STATE)
#endif
	{
           // #if defined(CONFIG_RTL_92D_SUPPORT)||defined (CONFIG_RTL_8881A)
	//dual band
			#if (defined (CONFIG_USE_PCIE_SLOT_0)&& (defined (CONFIG_USE_PCIE_SLOT_1) || defined (CONFIG_RTL_8197F)))||( defined(CONFIG_RTL_8881A)&& !defined(CONFIG_RTL_8881A_SELECTIVE))|| defined(CONFIG_WLAN_HAL_8814AE)
			unsigned int wispWlanIndex=(passThruStatusWlan&WISP_WLAN_IDX_MASK)>>WISP_WLAN_IDX_RIGHT_SHIFT;
            if(
				#ifdef SMART_REPEATER_MODE
				(priv==(GET_VXD_PRIV((wlan_device[wispWlanIndex].priv))))||
				#endif
                (priv == wlan_device[wispWlanIndex].priv))
            {
                pskb->dev = wlan_device[passThruWanIdx].priv->pWlanDev;
            }
            #else
            pskb->dev = wlan_device[passThruWanIdx].priv->pWlanDev;
            #endif

        }		
#endif
	}
#endif

#if defined(CONFIG_RTL8672) && defined(__OSK__)
	{
		int k;
		// Modofied by Mason Yu
		// MBSSID Port Mapping
		if( enable_port_mapping )
		{
			for (k=0; k<RTL8192CD_NUM_VWLAN+1; k++) {
				if ( priv->dev == wlanDev[k].dev_pointer ) {
					pskb->vlan_member = wlanDev[k].dev_ifgrp_member;
					break;
				}
			}
		}
	}
#endif

#ifdef GBWC
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
		if (priv->pmib->gbwcEntry.GBWCMode && pstat) {
			if (((priv->pmib->gbwcEntry.GBWCMode == GBWC_MODE_LIMIT_MAC_INNER) && (pstat->GBWC_in_group)) ||
				((priv->pmib->gbwcEntry.GBWCMode == GBWC_MODE_LIMIT_MAC_OUTTER) && !(pstat->GBWC_in_group)) ||
				(priv->pmib->gbwcEntry.GBWCMode == GBWC_MODE_LIMIT_IF_RX) ||
				(priv->pmib->gbwcEntry.GBWCMode == GBWC_MODE_LIMIT_IF_TRX)) {
				if ((priv->GBWC_rx_count + pskb->len) > ((priv->pmib->gbwcEntry.GBWCThrd_rx * 1024 / 8) / (1000/GBWC_TO_MILISECS))) {
					// over the bandwidth
					if (priv->GBWC_consuming_Q) {
						// in rtl8192cd_GBWC_timer context
						priv->ext_stats.rx_data_drops++;
						DEBUG_ERR("RX DROP: BWC bandwidth over!\n");
						rtl_kfree_skb(priv, pskb, _SKB_RX_);
					}
					else {
						// normal Rx path
						int ret = enque(priv, 
	#if defined(AP_SWPS_OFFLOAD)
	                            NULL,
	#endif					
	        					&(priv->GBWC_rx_queue.head), &(priv->GBWC_rx_queue.tail),
								(unsigned long)(priv->GBWC_rx_queue.pSkb), NUM_TXPKT_QUEUE, (void *)pskb);
						if (ret == FALSE) {
							priv->ext_stats.rx_data_drops++;
							DEBUG_ERR("RX DROP: BWC rx queue full!\n");
							rtl_kfree_skb(priv, pskb, _SKB_RX_);
						}
						else
							*(unsigned int *)&(pskb->cb[4]) = (unsigned int)pstat;	// backup pstat pointer
					}
					return;
				}
				else {
					// not over the bandwidth
					if (CIRC_CNT(priv->GBWC_rx_queue.head, priv->GBWC_rx_queue.tail, NUM_TXPKT_QUEUE) &&
							!priv->GBWC_consuming_Q) {
						// there are already packets in queue, put in queue too for order
						int ret = enque(priv, 
	#if defined(AP_SWPS_OFFLOAD)
	                            NULL,
	#endif						
						        &(priv->GBWC_rx_queue.head), &(priv->GBWC_rx_queue.tail),
								(unsigned long)(priv->GBWC_rx_queue.pSkb), NUM_TXPKT_QUEUE, (void *)pskb);
						if (ret == FALSE) {
							priv->ext_stats.rx_data_drops++;
							DEBUG_ERR("RX DROP: BWC rx queue full!\n");
							rtl_kfree_skb(priv, pskb, _SKB_RX_);
						}
						else
							*(unsigned int *)&(pskb->cb[4]) = (unsigned int)pstat;	// backup pstat pointer
						return;
					}
					else {
						// can pass up directly
						priv->GBWC_rx_count += pskb->len;
					}
				}
			}
		}
	}
#endif

#ifdef SBWC
	if (pstat && (OPMODE & WIFI_AP_STATE)) 	{
		if (pstat->SBWC_mode & SBWC_MODE_LIMIT_STA_RX) {
			unsigned long flags=0;
			SMP_LOCK_SBWC(flags);
			if ((pstat->SBWC_rx_count + pskb->len) > pstat->SBWC_rx_limit_byte) {
				// over the bandwidth
				if (pstat->SBWC_consuming_q) {
					// in rtl8192cd_SBWC_timer context
					priv->ext_stats.rx_data_drops++;
					DEBUG_ERR("RX DROP: SBWC bandwidth over!\n");
					rtl_kfree_skb(priv, pskb, _SKB_RX_);
				}
				else {
					// normal Rx path
					int ret = enque(priv, 
#if defined(AP_SWPS_OFFLOAD)
                            NULL,
#endif						
					        &(pstat->SBWC_rxq.head), &(pstat->SBWC_rxq.tail),
							(unsigned long)(pstat->SBWC_rxq.pSkb), NUM_TXPKT_QUEUE, (void *)pskb);
					if (ret == FALSE) {
						priv->ext_stats.rx_data_drops++;
						DEBUG_ERR("RX DROP: SBWC rx queue full!\n");
						rtl_kfree_skb(priv, pskb, _SKB_RX_);
					}
					else
						*(unsigned int *)&(pskb->cb[4]) = (unsigned int)pstat;	// backup pstat pointer
				}
				SMP_UNLOCK_SBWC(flags);
				return;
			}
			else {
				// not over the bandwidth
				if (CIRC_CNT(pstat->SBWC_rxq.head, pstat->SBWC_rxq.tail, NUM_TXPKT_QUEUE) &&
						!pstat->SBWC_consuming_q) {
					// there are already packets in queue, put in queue too for order
					int ret = enque(priv, 
#if defined(AP_SWPS_OFFLOAD)
                            NULL,
#endif	
					        &(pstat->SBWC_rxq.head), &(pstat->SBWC_rxq.tail),
							(unsigned long)(pstat->SBWC_rxq.pSkb), NUM_TXPKT_QUEUE, (void *)pskb);
					if (ret == FALSE) {
						priv->ext_stats.rx_data_drops++;
						DEBUG_ERR("RX DROP: SBWC rx queue full!\n");
						rtl_kfree_skb(priv, pskb, _SKB_RX_);
					}
					else
						*(unsigned int *)&(pskb->cb[4]) = (unsigned int)pstat;	// backup pstat pointer

					SMP_UNLOCK_SBWC(flags);
					return;
				}
				else {
					// can pass up directly
					pstat->SBWC_rx_count+= pskb->len;
				}
			}
			SMP_UNLOCK_SBWC(flags);
		}
	}
#endif

#ifdef ENABLE_RTL_SKB_STATS
	rtl_atomic_dec(&priv->rtl_rx_skb_cnt);
#endif

#ifdef CONFIG_SPECIAL_ENV_TEST
	if (pskb->len == priv->pshare->rf_ft_var.spec_pktsz-4)
		priv->ext_stats.spec_netif_rx_pkt++;
	priv->ext_stats.netif_rx_pkt++;
#endif

#if defined(MULTI_MAC_CLONE)
	if ((OPMODE & WIFI_AP_STATE) && pskb->dev && (pskb->dev->base_addr != 0)) {
#if !defined(__ECOS)		
		*(unsigned int *)&(pskb->cb[40]) = 0x86518192;	// means from wlan interface
#else
		*(unsigned int *)&(pskb->cb[8]) = 0x86518190;
#endif
	}
#endif

#if defined(RTK_ATM) && !defined(HS2_SUPPORT)
	if(priv->pshare->rf_ft_var.atm_en && priv->pshare->rf_ft_var.atm_mode==4)
	{
		if (IS_MCAST(pskb->data) && pstat)	
			staip_snooping_byarp(pskb, pstat);		
	}
#endif

#ifdef HS2_SUPPORT
/* Hotspot 2.0 Release 1 */
	if ((priv->proxy_arp) && (IS_MCAST(pskb->data)) && pstat)
	{
		if (ICMPV6_MCAST_SOLI_MAC(pskb->data)){
            _HS2DEBUG("\n");
			HS2DEBUG("=>stav6ip_snooping_bynsolic from[%02X%02X%02X:%02X%02X%02X]\n",pskb->data[6],pskb->data[7],pskb->data[8]
                ,pskb->data[9],pskb->data[10],pskb->data[11]);
			stav6ip_snooping_bynsolic(priv,pskb, pstat);
            panic_printk("\n");            
		}
		//else if (ICMPV6_MCAST_MAC(pskb->data))
		//	stav6ip_snooping_bynadvert(pskb, pstat);
		else{	
			//HS2DEBUG("\n");			
			staip_snooping_byarp(pskb, pstat);
		}
	}    
#endif

#ifdef UNIVERSAL_REPEATER
    if((OPMODE & WIFI_STATION_STATE)
#if defined(__KERNEL__) || defined(__OSK__)
        && (br_port)
        #endif
    ) {
        #ifdef A4_STA
        if(pstat && !(pstat->state & WIFI_A4_STA))
        #endif
        {

            unsigned char *brmac;

            brmac = priv->br_mac;
            if(isDHCPpkt(pskb) && memcmp(pskb->data+MACADDRLEN,brmac,MACADDRLEN)) {
			HS2_DEBUG_INFO("%s %d invoke snoop_STA_IP\n",__func__,__LINE__);
                snoop_STA_IP(pskb, priv);
            }
        }
    }
#endif

#if 0/*def CONFIG_RTL_STP*/
	if (((unsigned char)pskb->data[12]) < 0x06)
	{
		if (!memcmp(pskb->data, STPmac, 5) && !(((unsigned char )pskb->data[5])& 0xF0))
		{
			if (memcmp(pskb->dev->name, WLAN_INTERFACE_NAME, sizeof(WLAN_INTERFACE_NAME)) == 0)
			{
				if (wlan_pseudo_dev != NULL)
					pskb->dev = wlan_pseudo_dev;
				pskb->protocol = eth_type_trans(pskb, priv->dev);
			#if defined(_BROADLIGHT_FASTPATH_)
				send_packet_to_upper_layer(pskb);
			#elif defined(__LINUX_2_6__) && defined(RX_TASKLET) && !defined(CONFIG_RTL8672) && !defined(NOT_RTK_BSP) && !(defined(__LINUX_3_10__) || defined(__LINUX_3_2__))
				netif_receive_skb(pskb);
			#else
				netif_rx(pskb);
			#endif
			}
		}
	}
	else
#endif
	{
#ifdef CONFIG_RTK_VLAN_SUPPORT
#if defined(CONFIG_RTK_MESH)
		if(pskb->dev == priv->mesh_dev) {
			vlan = (struct vlan_info *)&priv->mesh_vlan;
		} else
#endif
		{
			vlan = (struct vlan_info *)&priv->pmib->vlan;
		}

        //if (rtk_vlan_support_enable && vlan->global_vlan) {
		  if (vlan->global_vlan) {
#if defined(CONFIG_RTK_BRIDGE_VLAN_SUPPORT)|| defined(CONFIG_RTL_HW_VLAN_SUPPORT)
                if (rx_vlan_process(priv->dev, vlan, pskb, NULL))
#else
                if (rx_vlan_process(priv->dev, vlan, pskb))
#endif
				{
                        priv->ext_stats.rx_data_drops++;
                        DEBUG_ERR("RX DROP: by vlan!\n");
                        dev_kfree_skb_any(pskb);
                        return;
                }

#if defined(CONFIG_RTK_VLAN_FOR_CABLE_MODEM)
                if(rtk_vlan_support_enable == 2 && pskb->tag.f.tpid ==  htons(ETH_P_8021Q))
                {
                        struct net_device *toDev;

                        toDev = re865x_get_netdev_by_name("eth1");

                        //printk("===%s(%d),vid(%d),from(%s),todev(%s),skb->tag.vid(%d)\n",__FUNCTION__,__LINE__,pskb->tag.f.pci & 0xfff, pskb->dev->name,
                                //toDev?toDev->name:NULL,pskb->tag.f.pci & 0xfff);

                        if(toDev)
                        {
                                pskb->dev = toDev;
                                toDev->netdev_ops->ndo_start_xmit(pskb,toDev);
                                return;
                        }

                }
#endif
        }
#endif

#if defined(CONFIG_RTL_VLAN_PASSTHROUGH_SUPPORT)	
		if (*((uint16*)(pskb->data+(ETH_ALEN<<1))) == __constant_htons(ETH_P_8021Q)) 
		{	
			extern int rtl_vlan_passthrough_enable;
			
			if(rtl_vlan_passthrough_enable){
				memcpy(pskb->vlan_passthrough_saved_tag, pskb->data+ETH_ALEN*2, 4);
		
				memmove(pskb->data + 4, pskb->data, ETH_ALEN<<1);
				skb_pull(pskb, 4);
			}
		}
#endif

#if defined(CONFIG_RTL_819X_ECOS)&&defined(CONFIG_RTL_VLAN_SUPPORT)&&defined(CONFIG_RTL_819X_SWCORE)
	if (rtl_vlan_support_enable) {
        #ifdef CONFIG_RTL_BRIDGE_VLAN_SUPPORT
		if (rtl_vlanIngressProcess(pskb, priv->dev->name, NULL) < 0)
        #else
		if (rtl_vlanIngressProcess(pskb, priv->dev->name) < 0)
        #endif
		{
			priv->ext_stats.rx_data_drops++;
			DEBUG_ERR("RX DROP: by vlan!\n");
			dev_kfree_skb_any(pskb);
			return;
		}
	}
#endif	//CONFIG_RTL_VLAN_SUPPORT

#ifdef CONFIG_RTL_VLAN_8021Q
#if 1
	if(linux_vlan_enable){
		uint16 vid = 0;
		uint8 vlan_type = 0;
		/*add vlan tag if pkt is untagged*/
		if(*((uint16*)(pskb->data+(ETH_ALEN<<1))) != __constant_htons(ETH_P_8021Q)){
			/*mapping dev to pvid array's index*/
			index = priv->dev->vlan_member_map;	
			if(index>=WLAN0_MASK_BIT && index<=WLAN1_VXD_MASK_BIT){
				vid = vlan_ctl_p->pvid[index];
				if(vid){
					memmove(pskb->data-VLAN_HLEN, pskb->data, ETH_ALEN<<1);
					skb_push(pskb,VLAN_HLEN);
					*((uint16*)(pskb->data+(ETH_ALEN<<1))) = __constant_htons(ETH_P_8021Q);		
					*((uint16*)(pskb->data+(ETH_ALEN<<1)+2)) = __constant_htons(vid);

					//For iptables vlanid match.
					pskb->srcVlanId = vid;

					*(uint16 *)(pskb->linux_vlan_src_tag) = __constant_htons(ETH_P_8021Q);
					*(uint16 *)(pskb->linux_vlan_src_tag+2) = __constant_htons(vid);
				}
			}		
		}
	}
#endif    
#endif

#if defined(CONFIG_RTL_ISP_MULTI_WAN_SUPPORT)
	if (*((uint16*)(pskb->data+(ETH_ALEN<<1))) == __constant_htons(ETH_P_8021Q)) 
	{	
		pskb->srcVlanId = ntohs(*(uint16*)(pskb->data+(ETH_ALEN<<1)+2)) & 0x0fff; 
	}
#endif

#ifdef CONFIG_PUMA_VLAN_8021Q
	if (priv->pmib->vlan.vlan_enable) {
		//mem_dump("8021Q,RX", pskb->data, 18);
		if(*((UINT16*)(pskb->data+(ETH_ALEN<<1))) != __constant_htons(ETH_P_8021Q)){
			memmove(pskb->data-VLAN_HLEN, pskb->data, ETH_ALEN<<1);
			skb_push(pskb,VLAN_HLEN);
			*((UINT16*)(pskb->data+(ETH_ALEN<<1))) = __constant_htons(ETH_P_8021Q);		
			*((UINT16*)(pskb->data+(ETH_ALEN<<1)+2)) = __constant_htons(priv->pmib->vlan.vlan_id);
		}
	}
#endif

#if defined(BR_SHORTCUT)
	if ((!priv->pmib->dot11OperationEntry.disable_brsc) &&
		(pskb->dev) &&
		(rtl_IsMcastIP(pskb) == 0) &&
		!(pskb->data[0] & 0x01)
#if defined(__KERNEL__) || defined(__OSK__)
		&& (br_port)
#endif
		) {
		if (gothrough_brsrc(priv, pskb, pstat)) {
			//pstat->rx_pkts2++;
			return;
		}	
	}
#endif // BR_SHORTCUT

	 #if defined(CONFIG_RTL_FASTBRIDGE)
	 	if (br_port) {
			if (RTL_FB_RETURN_SUCCESS==rtl_fb_process_in_nic(pskb, pskb->dev))
				return;
	 	}
	#endif

#if defined(CONFIG_RTL_EXT_PORT_SUPPORT)
	if (extPortEnabled&&(rtl_wlan_frame_use_hw_nat(pskb->data)==SUCCESS)) {
		struct net_device *eth_dev = NULL;
		struct net_device *ori_wlan_dev = NULL;
		//panic_printk("%s:%d\n",__FUNCTION__,__LINE__);
		if ((eth_dev = __dev_get_by_name(&init_net, "eth0")) != NULL) {
			if (pskb->dev)
				ori_wlan_dev = pskb->dev;
			else
				ori_wlan_dev = __dev_get_by_name(&init_net, "wlan0");

			pskb->dev = eth_dev;
			#if !defined(__LINUX_2_6__) || defined(CONFIG_COMPAT_NET_DEV_OPS)
			eth_dev->hard_start_xmit(pskb, ori_wlan_dev);
			#else
			eth_dev->netdev_ops->ndo_start_xmit(pskb, ori_wlan_dev);
			#endif
			statistic_wlan_xmit_to_eth++;

			return;
		}
	}
#endif

#ifdef WLAN_VDEV_SUPPORT
	if((OPMODE & WIFI_AP_STATE) &&
	   (priv->vdev_enabled) &&
	   (priv->vdev)
	)
	{
		int tov = 1;
		int offset=(ETH_ALEN << 1);
		int proc_val=priv->vdev_enabled, is_dhcp=0, is_br0_ip=0;
		enum ip_proto{PROTO_UNKNOWN, PROTO_IPV4, PROTO_IPV6} pkt_proto = PROTO_UNKNOWN;
		uint16 proto_tmp;
		extern unsigned int _br0_ip;
		struct net_device *br_dev = __dev_get_by_name(&init_net, "br0");
#ifdef CONFIG_IPV6
		struct inet6_dev *br_idev = br_dev?__in6_dev_get(br_dev):NULL;
		struct inet6_ifaddr *ifp = NULL;
		extern void* rtl_get_ipv6_skip_exthdr(struct sk_buff *skb,uint8_t protocol);
#endif
		//printk("[%s:%d] proc_val:0x%x\n", __FUNCTION__, __LINE__, proc_val);

		//parse packet IP version and is DHCP
		{
			if(*(uint16 *)&pskb->data[offset] == __constant_htons(ETH_P_8021Q))
				offset += VLAN_HLEN;

			//check pkt ip proto
			proto_tmp = *(uint16 *)&pskb->data[offset];
			if(proto_tmp == __constant_htons(ETH_P_IP) 
				|| proto_tmp == __constant_htons(ETH_P_ARP)) 
				pkt_proto = PROTO_IPV4;
			else if(proto_tmp == __constant_htons(ETH_P_IPV6)) 
				pkt_proto = PROTO_IPV6;
			else if(proto_tmp == __constant_htons(ETH_P_PPP_SES))
			{
				#define PPP_IP		0x21
				#define PPP_IPCP	0x8021
				#define PPP_IPV6	0x57
				#define PPP_IPV6CP	0x8057
			
				offset += 8;	//pppoe header length
				proto_tmp = *(uint16 *)&pskb->data[offset];
				if(proto_tmp == __constant_htons(PPP_IP) || proto_tmp == __constant_htons(PPP_IPCP))
					pkt_proto = PROTO_IPV4;
				else if(proto_tmp == __constant_htons(PPP_IPV6) || proto_tmp == __constant_htons(PPP_IPV6CP))
					pkt_proto = PROTO_IPV6;
			}
			//printk("[%s:%d] packet proto_tmp:%x, pkt_proto:0x%x\n", __FUNCTION__, __LINE__, proto_tmp, pkt_proto);

			if(proto_tmp == __constant_htons(ETH_P_IP))
			{
				struct iphdr *iph = (struct iphdr *)(&pskb->data[offset+2]);	//move in 2-byte protocal type

				//check is dhcp or not
		        if (iph && (iph->protocol == IPPROTO_UDP))
				{
		            struct udphdr *udph = (uint8 *)iph + iph->ihl*4;
		            if (udph && (udph->source == htons(68) || udph->source == htons(67)))// DHCP packet
		                is_dhcp = 1;
		        }else if (iph->daddr == _br0_ip)
					is_br0_ip = 1;	//check DIP==br0 ip
			}
#ifdef CONFIG_IPV6
			else if(proto_tmp == __constant_htons(ETH_P_IPV6)){
				struct ipv6hdr *ipv6h = (struct iphdr *)(&pskb->data[offset+2]);	//move in 2-byte protocal type

				if(br_idev && ipv6h)
				{
					struct udphdr *udpv6hdr = (struct udphdr *)rtl_get_ipv6_skip_exthdr(pskb, NEXTHDR_UDP);
					//printk("packet V6 DIP:%x %x %x %x\n", ipv6h->daddr.s6_addr32[0], ipv6h->daddr.s6_addr32[1], ipv6h->daddr.s6_addr32[2], ipv6h->daddr.s6_addr32[3]);

					//check is DHCPv6 or not
					if (udpv6hdr && (udpv6hdr->source == htons(546) || udpv6hdr->source == htons(547)))// DHCPv6 packet
		                is_dhcp = 1;
					else{
						//check DIP6==br0 ip6
						list_for_each_entry(ifp, &br_idev->addr_list, if_list) 
						{
							//printk("br0 V6 IP:%x %x %x %x\n", ifp->addr.s6_addr32[0], ifp->addr.s6_addr32[1], ifp->addr.s6_addr32[2], ifp->addr.s6_addr32[3]);
					    	if((ifp->addr.s6_addr32[0]== ipv6h->daddr.s6_addr32[0])&&
							   (ifp->addr.s6_addr32[1]== ipv6h->daddr.s6_addr32[1])&&
							   (ifp->addr.s6_addr32[2]== ipv6h->daddr.s6_addr32[2])&&
							   (ifp->addr.s6_addr32[3]== ipv6h->daddr.s6_addr32[3]))
					   		{
					       		is_br0_ip = 1;
					      		break;
					    	}
					 	}
					}
					//printk("v6 is_dhcp:%d, is_br0_ip:%d\n", is_dhcp, is_br0_ip);
				}
			}
#endif
			//printk("[%s:%d]is_br_ip:%d, is_dhcp:%d\n", __FUNCTION__, __LINE__, is_br0_ip, is_dhcp);
		}
		
		//handle allowed ip version check
		if( !ip_proto_is_bothv4v6(proc_val) && is_opt_ipdrop_set(proc_val) )
		{
			if( (pkt_proto==PROTO_IPV4&&ip_proto_is_v6only(proc_val)) ||
				(pkt_proto==PROTO_IPV6&&ip_proto_is_v4only(proc_val))
			  )
			{
				//ip version mismatch
				//printk("[%s:%d] mismatch ip version, drop!!!\n", __FUNCTION__, __LINE__);

				priv->ext_stats.rx_data_drops++;
	            DEBUG_ERR("RX DROP: packet ip version mismatch with lan-wan bridge setting.\n");
	            rtl_kfree_skb(priv, pskb, _SKB_RX_);
	            return ;
			}
		}

		//handle bridge proto type
		if(pskb->data[0]==0xFF)
		{
			//broadcast pkt
			int dst_ip=0;

			if(*(uint16 *)&pskb->data[offset] == __constant_htons(ETH_P_ARP))
			{
				//ARP
				memcpy((void *)&dst_ip, pskb->data + offset + 26, 4);
				if(dst_ip == _br0_ip)
				{
					//printk("[%s:%d] ARP to br0 --> no need to vdev!\n", __FUNCTION__, __LINE__);
					tov = 0;
				}
			}else if(is_dhcp && is_opt_trapDHCP_set(proc_val))
			{
				//DHCP packet Trap to br0
				//printk("[%s:%d] Trap DHCP packet!!!\n", __FUNCTION__, __LINE__);
				tov = 0;
			}else{
				//if vdev type is V6 only, broadcast packet is seen as V4 and should not go to vdev.
				if(br_proto_is_v6only(proc_val))
					tov = 0;
			}
		}else if((pskb->data[0] == 0x33)&&(pskb->data[1] == 0x33))
		{
#ifdef CONFIG_IPV6
		   	//v6 multicast pkt
			if(*(uint16 *)&pskb->data[offset] == __constant_htons(ETH_P_IPV6))
		    {
		    	if((pskb->data[2] == 0x00) && is_dhcp && is_opt_trapDHCP_set(proc_val))
				{
		    		//Trap DHCPv6 to br0 if required
		    		//printk("[%s:%d] Trap DHCPV6 to br0", __FUNCTION__, __LINE__);
					tov = 0;
		    	}else if(pskb->data[2] == 0xff)
				{
					//Trap ICMPv6 NEIGH SOLICATION to br0 if its target IP is br0 IP
			        struct nd_msg *nd_msg_hdr = rtl_get_ipv6_skip_exthdr(pskb, NEXTHDR_ICMP);
					if(nd_msg_hdr && (nd_msg_hdr->icmph.icmp6_type == NDISC_NEIGHBOUR_SOLICITATION) && br_idev)
					{
                     	list_for_each_entry(ifp, &br_idev->addr_list, if_list) 
                    	{
                        	if((ifp->addr.s6_addr32[0]== nd_msg_hdr->target.s6_addr32[0])&&
						   	(ifp->addr.s6_addr32[1]== nd_msg_hdr->target.s6_addr32[1])&&
						  	(ifp->addr.s6_addr32[2]== nd_msg_hdr->target.s6_addr32[2])&&
						  	(ifp->addr.s6_addr32[3]== nd_msg_hdr->target.s6_addr32[3]))
                        	{
                   				//allow to br0
                           		tov = 0;
		                   		break;
                        	}
                     	}
					}
	    		}
		    }
#endif
		}else{
			//unicast packet
			if(br_dev && memcmp(pskb->data, br_dev->dev_addr, MACADDRLEN)==0)
			{
				//pkt DMAC==GMAC, to real device do NAT/Routing
				//printk("[%s:%d] non-broadcast packet to br0 --> no need to vdev!\n", __FUNCTION__, __LINE__);
				if( proto_tmp==__constant_htons(ETH_P_IP) || proto_tmp==__constant_htons(ETH_P_IPV6) )
				{
					//IP packet
					if(is_br0_ip)
					{
						if( (br_proto_is_bothv4v6(proc_val)) ||
							(br_proto_is_v4only(proc_val) && pkt_proto==PROTO_IPV4) ||
							(br_proto_is_v6only(proc_val) && pkt_proto==PROTO_IPV6)
						)
						//packet proto matches br proto, and packet DIP is br0 IP: allow to br0
						tov = 0;
					}else{
						//drop
						priv->ext_stats.rx_data_drops++;
			            DEBUG_ERR("RX DROP: bridged STA should not try to do NAT/Routing\n");
			            rtl_kfree_skb(priv, pskb, _SKB_RX_);
			            return ;
					}
				}else if(proto_tmp == __constant_htons(ETH_P_ARP))
				{
					//ARP packet, allow to br0 for learning
					tov = 0;
				}else{
					//drop
					priv->ext_stats.rx_data_drops++;
		            DEBUG_ERR("RX DROP: this is a bridge device, should not do NAT/Routing!\n");
		            rtl_kfree_skb(priv, pskb, _SKB_RX_);
		            return ;
				}
			}else{
				//pkt DMAC!=GMAC, should to virtual device do bridging.
				if( (pkt_proto==PROTO_IPV4 && br_proto_is_v6only(proc_val)) || (pkt_proto==PROTO_IPV6 && br_proto_is_v4only(proc_val))){
					// packet type mismatch bridge type, DROP!
					priv->ext_stats.rx_data_drops++;
		            DEBUG_ERR("RX DROP: packet ip version mismatch with bridge type.\n");
		            rtl_kfree_skb(priv, pskb, _SKB_RX_);
		            return ;
				}else{
					//default policy: bridging.
					tov = 1;
				}
			}
		}

		if(tov)
		{
			pskb->dev = priv->vdev;
			//printk("[%s:%d] change to vdev: %s\n", __FUNCTION__, __LINE__, pskb->dev->name);
			
			//correct stat
			struct rtl8192cd_priv *vdp = (struct rtl8192cd_priv *)netdev_priv(priv->vdev);
			vdp->net_stats.rx_packets++;
			vdp->net_stats.rx_bytes += pskb->len;
		}else{
			//printk("[%s:%d] remain to dev: %s\n", __FUNCTION__, __LINE__, pskb->dev->name);
		}
	}
#endif /*WLAN_VDEV_SUPPORT*/

#ifdef MBSSID
	if ((OPMODE & WIFI_AP_STATE) && pskb->dev && (pskb->dev->base_addr != 0)) {
		*(unsigned int *)&(pskb->cb[20]) = 0x86518190;						// means from wlan interface
	#if !defined(__ECOS)
		*(unsigned int *)&(pskb->cb[24]) = priv->pmib->miscEntry.groupID;	// remember group ID
	#else
		*(unsigned int *)&(pskb->cb[12]) = priv->pmib->miscEntry.groupID;	// remember group ID
	#endif
	}
#endif

#ifdef CONFIG_RTL867X_VLAN_MAPPING
	if (re_vlan_loaded()) {
		re_vlan_untag(pskb);
	}
#endif

#if defined(__KERNEL__) || defined(__OSK__)
	if (pskb->dev)
#ifdef __LINUX_2_6__
		pskb->protocol = eth_type_trans(pskb, pskb->dev);
	else
#endif
		pskb->protocol = eth_type_trans(pskb, priv->dev);
#endif

#ifdef _FULLY_WIFI_IGMP_SNOOPING_SUPPORT_
	check_igmp_snooping_pkt(pskb);
#endif //_FULLY_WIFI_IGMP_SNOOPING_SUPPORT_

#ifdef _FULLY_WIFI_MLD_SNOOPING_SUPPORT_
	check_mld_snooping_pkt(pskb);
#endif //_FULLY_WIFI_MLD_SNOOPING_SUPPORT_

#if defined(__OSK__) && defined(CONFIG_RTL8672)
	int i, num, idx;
	num = TOTAL_WLAN_PORT_NUM-1;
	idx = wlan_find_idx_by_linux_name(priv->dev->name);
	if(enable_IGMP_SNP){
		//IGMPSNOOPDEBUG(("%s: line %d, num=%d\n",__FUNCTION__, __LINE__, num));
		for (i=0; i<=num; i++) {
			//IGMPSNOOPDEBUG(("%s: line %d, len=%d, i=%d\n",__FUNCTION__, __LINE__, sizeof(wlanDev)/sizeof(struct port_map), i));			
			if ( (priv->dev == wlanDev[i].dev_pointer) && isWLANInterfaceIdx(idx)) {
				//IGMPSNOOPDEBUG(("%s: line %d, i=%d, idx=%d\n",__FUNCTION__, __LINE__, i, idx));
				check_IGMP_snoop_rx((CCB *)skb2ccb(pskb), wlan_igmp_tag+idx);
				break;
			}
		}
	}	
#ifdef INET6
	if (enable_MLD_SNP)
	{
		for (i=0; i<=num; i++) {
			if ( priv->dev == wlanDev[i].dev_pointer && isWLANInterfaceIdx(idx)) {
				check_mld_snoop_rx((CCB *)skb2ccb(pskb), wlan_igmp_tag+idx);
			}
		}
	}
#endif
#elif defined(CONFIG_RTL8672) ||defined(CONFIG_RTL_ISP_MULTI_WAN_SUPPORT)
#ifdef CONFIG_EXT_SWITCH
	if(enable_IGMP_SNP) {
		check_IGMP_snoop_rx(pskb, wlan_igmp_tag);
	}
#endif
	//pskb->switch_port = priv->dev->name;
	pskb->from_dev = priv->dev;
#elif defined(CONFIG_RTL_ISP_MULTI_WAN_SUPPORT)
	pskb->from_dev = pskb->dev;	
#endif
#if defined(CONFIG_RTL_IP_POLICY_ROUTING_SUPPORT)
	pskb->switch_port = priv->dev->name;
#endif

#ifdef HS2_SUPPORT
/* Hotspot 2.0 Release 1 */
#ifdef __ECOS
	if (pskb->data[12] == 0x89 && pskb->data[13] == 0x0d )
#else
	if (pskb->protocol == __constant_htons(ETHER_TDLS))
#endif
	{
		priv->ext_stats.rx_data_drops++;
        HS2_DEBUG_INFO("RX DROP: TDLS!\n");
        dev_kfree_skb_any(pskb);

		return;
	} 
#endif

#ifdef  SUPPORT_RX_UNI2MCAST
	/* under sta mode for check UDP type packet that L3 IP is multicast but L2 mac is not */
	if (((OPMODE & WIFI_STATION_STATE) == WIFI_STATION_STATE)
#ifdef MULTI_MAC_CLONE
		&& !GET_MIB(priv)->ethBrExtInfo.macclone_enable
#endif
	)
	{
		#ifdef __ECOS
		//diag_printf("[%s]:[%d],pskb:%x-%x-%x-%x-%x-%x.\n",__FUNCTION__,__LINE__,pskb->data[0],pskb->data[1],pskb->data[2],pskb->data[3],pskb->data[4],pskb->data[5]);
		L3_protocol = *(unsigned short *)(pskb->data+ MACADDRLEN * 2);
		DA_START = pskb->data;
		#else
		L3_protocol = *(unsigned short *)(SKB_MAC_HEADER(pskb) + MACADDRLEN * 2);
		DA_START = SKB_MAC_HEADER(pskb);
		#endif
		if( L3_protocol == __constant_htons(0x0800) )
		//&&(*(unsigned char *)(SKB_MAC_HEADER(pskb) + 23)) == 0x11) { /*added by qinjunjie,warning:unicast to multicast conversion should not only limited to udp*/
		{
			CheckUDPandU2M(pskb);
		}else if(L3_protocol == __constant_htons(0x86dd) &&
		#ifdef __ECOS
			*(unsigned char *)(pskb->data + 20) == 0x11 )
		#else
			*(unsigned char *)(SKB_MAC_HEADER(pskb) + 20) == 0x11 )
		#endif	
		{
			CheckV6UDPandU2M(pskb);
		}
	}

#ifdef BR_SHORTCUT
#ifdef VIDEO_STREAMING_REFINE
	// for video streaming refine
	if ((OPMODE & WIFI_STATION_STATE) &&
		(*((unsigned char *)SKB_MAC_HEADER(pskb))) & 0x01) &&
		!priv->pmib->dot11OperationEntry.disable_brsc &&
		(br_port) &&
		((dev = is_eth_streaming_only(pskb)) != NULL)) {
			skb_push(pskb, 14);
#if !defined(__LINUX_2_6__) || defined(CONFIG_COMPAT_NET_DEV_OPS)
			dev->hard_start_xmit(pskb, dev);
#else
			dev->netdev_ops->ndo_start_xmit(pskb, dev);
#endif
			return;
		}
#endif // VIDEO_STREAMING_REFINE
#endif // BR_SHORTCUT
#endif // SUPPORT_RX_UNI2MCAST

#ifdef CONFIG_RTL_NETSNIPER_SUPPORT
        pskb->wanorlan = rtl_check_wanorlan(pskb->dev->name); /* wan:1 lan:2 default:0 */
#endif

#if defined(SHORTCUT_STATISTIC) /*defined(__ECOS) && defined(_DEBUG_RTL8192CD_)*/
		priv->ext_stats.br_cnt_nosc++;
#endif
	#if defined(_BROADLIGHT_FASTPATH_)
		send_packet_to_upper_layer(pskb);
	#elif defined(CONFIG_PUMA_UDMA_SUPPORT)
		skb_push(pskb, ETH_HLEN);
		#ifdef CONCURRENT_MODE
			udma_xmit_skb(priv->pshare->wlandev_idx, pskb);
		#else
			udma_xmit_skb(0, pskb);
		#endif
	#elif defined(__LINUX_2_6__) && defined(RX_TASKLET) && !defined(CONFIG_RTL8672)&& !defined(NOT_RTK_BSP)&& !(defined(__LINUX_3_10__) || defined(__LINUX_3_2__))
		netif_receive_skb(pskb);
	#elif defined(CONFIG_RG_WLAN_HWNAT_ACCELERATION) && !defined(CONFIG_ARCH_LUNA_SLAVE)
    {
#ifdef CONFIG_RTK_MESH
		// Check stp state here
		struct net_bridge_port *mesh_br_port=NULL;
		mesh_br_port = GET_BR_PORT(pskb->dev);

		if(GET_MIB(priv)->dot1180211sInfo.mesh_enable && 
			mesh_br_port && mesh_br_port->br->stp_enabled && mesh_br_port->state != BR_STATE_FORWARDING)
		{
			netif_rx(pskb);
		}
		else
#endif
		{
#ifdef CONFIG_RG_NETIF_RX_QUEUE_SUPPORT
    #if defined(CONFIG_RTL_TRIBAND_SUPPORT)    
            if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE)
                skb_queue_tail(&priv->pshare->rg_netif_rx_queue, pskb);
            else
                rtk_rg_netif_rx(pskb);
    #else
			skb_queue_tail(&priv->pshare->rg_netif_rx_queue, pskb);
    #endif
#else
			rtk_rg_netif_rx(pskb);
#endif
		}
    }
    #else
#ifdef __OSK__
		toswcore_cnt++;
#endif
		netif_rx(pskb);
    #endif
	}
	pstat->rx_pkts2++;
}


#ifdef CONFIG_RTL_KERNEL_MIPS16_WLAN
__NOMIPS16
#endif
#ifdef __OSK__
__IRAM_WIFI_PRI3
#endif
void process_amsdu(struct rtl8192cd_priv *priv, struct stat_info *pstat, struct rx_frinfo *pfrinfo)
{
    unsigned char	*pframe, *da;
    struct stat_info	*dst_pstat = NULL;
    struct sk_buff	*pskb  = NULL, *pnewskb = NULL;
    unsigned char	*next_head;
    int				rest, agg_pkt_num=0, i, privacy;
    unsigned int	subfr_len, padding;
    const unsigned char rfc1042_ip_header[8]={0xaa,0xaa,0x03,00,00,00,0x08,0x00};
#ifdef RX_BUFFER_GATHER
    struct sk_buff *nskb;
    int rx_gather = 0;
    int is_last = 0;
#ifdef RX_BUFFER_GATHER_REORDER
	struct list_head *phead;
	struct sk_buff	*pfisrtskb = NULL; 
#endif
#endif

#ifdef CONFIG_RTK_MESH
    struct  MESH_HDR* mhdr;
#endif
    pframe = get_pframe(pfrinfo);
    pskb = get_pskb(pfrinfo);

	rest = pfrinfo->pktlen - pfrinfo->hdr_len;
	next_head = pframe + pfrinfo->hdr_len;

	if(priv->pshare->rf_ft_var.debug8822 == 9)
		printk("process_amsdu +++ \n");

#ifdef RX_BUFFER_GATHER
    if (pfrinfo->gather_flag == GATHER_FIRST) {
        skb_pull(pskb, pfrinfo->hdr_len);
        rest = pfrinfo->gather_len - pfrinfo->hdr_len;
        rx_gather = 1;
#ifdef 	RX_BUFFER_GATHER_REORDER
		phead=&pfrinfo->gather_list_head;
		/*not to release it until amsdu handled over since head is in the first skb*/
		pfisrtskb=skb_clone(pskb, GFP_ATOMIC);
		if(pfisrtskb == NULL){
			printk("not skb buff\n");	
			flush_rx_list_inq(priv, pfrinfo);
			rtl_kfree_skb(priv, pskb, _SKB_RX_);
			return;
		}
#endif
    }
#endif

    if (GetPrivacy(pframe)) {
        #ifdef WDS
        if (pfrinfo->to_fr_ds==3 && priv->pmib->dot11WdsInfo.wdsEnabled)
            privacy = priv->pmib->dot11WdsInfo.wdsPrivacy;
        else
        #endif
            privacy = get_sta_encrypt_algthm(priv, pstat);
        if ((privacy == _CCMP_PRIVACY_) || (privacy == _TKIP_PRIVACY_)) {
            rest -= 8;
            next_head += 8;
            #ifdef RX_BUFFER_GATHER
            pskb->data += 8;
            pskb->len -= 8;
            #endif

        }
        else {	// WEP
            rest -= 4;
            next_head += 4;
            #ifdef RX_BUFFER_GATHER
            pskb->data += 4;
            pskb->len -= 4;
            #endif

        }
    }


	//Wifi Logo FFD 4.2.1
	if (isEqualMACAddr(next_head, rfc1042_ip_header))
	{
		DEBUG_ERR("RX DROP: LLC header within an A-MSDU subframe!\n");
#ifdef RX_BUFFER_GATHER_REORDER
		if(pfrinfo->gather_flag == GATHER_FIRST)
			flush_rx_list_inq(priv, pfrinfo);
#endif
		priv->ext_stats.rx_data_drops++;
		rtl_kfree_skb(priv, pskb, _SKB_RX_);
		return;
	}

    while (rest > WLAN_ETHHDR_LEN) {
#if defined(__OSK__) && !defined(CONFIG_SKB_CLONE)
		pnewskb = dev_alloc_skb((pskb->len< RX_BUF_LEN) ? RX_BUF_LEN : pskb->len);
		if (NULL == pnewskb)
		{
			dev_kfree_skb(pskb);
			return;
		}

		memcpy(pnewskb->data, next_head, pskb->len);
		pnewskb->dev = pskb->dev;
#else
        pnewskb = skb_clone(pskb, GFP_ATOMIC);
#endif

        if (pnewskb) {
#if !defined(__OSK__) || defined(CONFIG_SKB_CLONE)
            pnewskb->data = next_head;
#endif

#ifdef RX_BUFFER_GATHER
            if (rx_gather) {
#ifdef RX_BUFFER_GATHER_REORDER
				subfr_len = get_subframe_len(priv, pnewskb, is_last, phead);
#else
                subfr_len = get_subframe_len(priv, pnewskb, is_last);
#endif

                if (subfr_len <= 0) {
                    DEBUG_ERR("invalid subfr_len=%d, discard AMSDU!\n", subfr_len);
                    dev_kfree_skb_any(pnewskb);
                    break;
                }
#ifdef RX_BUFFER_GATHER_REORDER
				nskb = get_subframe(priv, pnewskb, &pskb, subfr_len, &is_last, phead);
#else				
                nskb = get_subframe(priv, pnewskb, &pskb, subfr_len, &is_last);
#endif
                if (nskb == NULL) {
                    DEBUG_ERR("get_subframe() failed, discard AMSDU!\n");
                    dev_kfree_skb_any(pnewskb);
                    priv->ext_stats.rx_data_drops++;
                    break;
                }
                if (nskb != pnewskb) {
                    nskb->dev = pnewskb->dev;
                    dev_kfree_skb_any(pnewskb);
                    pnewskb = nskb;
                }
            }
            else
#endif
                subfr_len = (*(next_head + MACADDRLEN*2) << 8) + (*(next_head + MACADDRLEN*2 + 1));

            pnewskb->len = WLAN_ETHHDR_LEN + subfr_len;
            skb_set_tail_pointer(pnewskb, pnewskb->len);
#ifdef ENABLE_RTL_SKB_STATS
            rtl_atomic_inc(&priv->rtl_rx_skb_cnt);
#endif

            if(pnewskb->tail > pnewskb->end) {
                rtl_kfree_skb(priv, pnewskb, _SKB_RX_);
                priv->ext_stats.rx_data_drops++;
                DEBUG_ERR("RX DROP: sub-frame length too large!\n");
                break;
            }

            if (!memcmp(rfc1042_ip_header, pnewskb->data+WLAN_ETHHDR_LEN, 8)) {
                for (i=0; i<MACADDRLEN*2; i++)
                    pnewskb->data[19-i] = pnewskb->data[11-i];
                pnewskb->data += 8;
                pnewskb->len -= 8;
            }
            else
                strip_amsdu_llc(priv, pnewskb, pstat);
            agg_pkt_num++;


            #ifdef CONFIG_RTK_MESH
            if(pfrinfo->is_11s) {               

                mhdr = (struct MESH_HDR*) (pnewskb->data+sizeof(struct wlan_ethhdr_t));
                pfrinfo->is_11s = 8;
                pfrinfo->pktlen = pnewskb->len;
                pfrinfo->da = (pnewskb->data);
                pfrinfo->sa = (pnewskb->data+MACADDRLEN);
                pfrinfo->pskb = pnewskb;

                 
                if (__process_11s_datafrme(priv, pfrinfo, pstat, mhdr) == FAIL) 
                    rtl_kfree_skb(priv, pnewskb, _SKB_RX_);
            }
            else
            #endif
            if (OPMODE & WIFI_AP_STATE)
            {
                da = pnewskb->data;
                dst_pstat = get_stainfo(priv, da);

#ifdef A4_STA
                if (priv->pmib->miscEntry.a4_enable && (dst_pstat == NULL))
                    dst_pstat = a4_sta_lookup(priv, da);
#endif

                if ((dst_pstat == NULL) || (!(dst_pstat->state & WIFI_ASOC_STATE)))
                    rtl_netif_rx(priv, pnewskb, pstat);
                else
                {
                    if (priv->pmib->dot11OperationEntry.block_relay == 1) {
                        priv->ext_stats.rx_data_drops++;
                        DEBUG_ERR("RX DROP: Relay unicast packet is blocked!\n");
#ifdef RX_SHORTCUT
                        for (i=0; i<RX_SC_ENTRY_NUM; i++) { // shortcut data saved, clear it
                            dst_pstat->rx_sc_ent[i].rx_payload_offset = 0;
                        }
#endif
                        rtl_kfree_skb(priv, pnewskb, _SKB_RX_);
                    }
                    else if (priv->pmib->dot11OperationEntry.block_relay == 2) {
                        DEBUG_INFO("Relay unicast packet is blocked! Indicate to bridge.\n");
                        rtl_netif_rx(priv, pnewskb, pstat);
                    }
                    else {
                        #ifdef ENABLE_RTL_SKB_STATS
                        rtl_atomic_dec(&priv->rtl_rx_skb_cnt);
                        #endif
						if ( 0 
							#ifdef GBWC
							|| (GET_HCI_TYPE(priv) == RTL_HCI_PCIE && GBWC_forward_check(priv, pnewskb, pstat))
							#endif
							#ifdef SBWC
							|| SBWC_forward_check(priv, pnewskb, pstat)
							#endif
							) 
						{
							// packet is queued, nothing to do
						}
						else
                        {
                            #ifdef TX_SCATTER
                            pnewskb->list_num = 0;
                            #endif

                            #ifdef CONFIG_RTK_MESH
                            if(isMeshPoint(dst_pstat)) {      
                                if(mesh_start_xmit(pnewskb, priv->mesh_dev))
                                    rtl_kfree_skb(priv, pnewskb, _SKB_TX_);
                            }
                            else
                            #endif  
                            {
                                if (rtl8192cd_start_xmit(pnewskb, priv->dev))
                                    rtl_kfree_skb(priv, pnewskb, _SKB_TX_);
                            }
                        }
                    }
                }
            }
#ifdef CLIENT_MODE
            else if (OPMODE & (WIFI_STATION_STATE | WIFI_ADHOC_STATE))
            {
                unsigned char to_netif_rx = 1;
#ifdef RTK_BR_EXT
#ifdef A4_STA
                if(pstat && !(pstat->state & WIFI_A4_STA))
#endif
                {                
                    if(nat25_handle_frame(priv, pnewskb) == -1) {
                        priv->ext_stats.rx_data_drops++;
                        DEBUG_ERR("RX DROP: nat25_handle_frame fail!\n");
                        rtl_kfree_skb(priv, pnewskb, _SKB_RX_);
                        to_netif_rx = 0;
                    }
                }
#endif
                if(to_netif_rx == 1)                   
                    rtl_netif_rx(priv, pnewskb, pstat);
            }
#endif

            padding = 4 - ((WLAN_ETHHDR_LEN + subfr_len) % 4);
            if (padding == 4)
                padding = 0;
            rest -= (WLAN_ETHHDR_LEN + subfr_len + padding);

#ifdef RX_BUFFER_GATHER
            if (rx_gather) {
                if ((rest <= WLAN_ETHHDR_LEN) && is_last)
                    break;
#ifdef RX_BUFFER_GATHER_REORDER
                nskb = shift_padding_len(priv, pskb, padding, &is_last, phead);
#else
				nskb = shift_padding_len(priv, pskb, padding, &is_last);
#endif
                if (nskb == NULL) {
                    DEBUG_ERR("shift AMSDU padding len error!\n");
                    break;
                }
                if (nskb != pskb) {
                    rtl_kfree_skb(priv, pskb, _SKB_RX_);
                    pskb = nskb;
                }
                next_head = pskb->data;
            }
            else
#endif
                next_head += (WLAN_ETHHDR_LEN + subfr_len + padding);
        }
        else {
            // Can't get new skb header, drop this packet
            break;
        }
    }

	// clear saved shortcut data
#ifdef RX_SHORTCUT
    for (i=0; i<RX_SC_ENTRY_NUM; i++) {
        pstat->rx_sc_ent[i].rx_payload_offset = 0;
    }
#endif

#ifdef _AMPSDU_AMSDU_DEBUG_//_DEBUG_RTL8192CD_
    switch (agg_pkt_num) {
        case 0:
            pstat->rx_amsdu_err++;
            break;
        case 1:
            pstat->rx_amsdu_1pkt++;
            break;
        case 2:
            pstat->rx_amsdu_2pkt++;
            break;
        case 3:
            pstat->rx_amsdu_3pkt++;
            break;
        case 4:
            pstat->rx_amsdu_4pkt++;
            break;
        case 5:
            pstat->rx_amsdu_5pkt++;
            break;
        default:
            pstat->rx_amsdu_gt5pkt++;
            break;
    }
#endif

    rtl_kfree_skb(priv, pskb, _SKB_RX_);
#ifdef 	RX_BUFFER_GATHER_REORDER
	if (rx_gather) {
		flush_rx_list_inq(priv, pfrinfo);
		if(pfisrtskb)
			rtl_kfree_skb(priv, pfisrtskb, _SKB_RX_);
	}
#endif
}




#if (!defined(__OSK__)) || (defined(__OSK__) && !defined(CONFIG_RTL6028))
__MIPS16
#endif
#ifndef WIFI_MIN_IMEM_USAGE
__IRAM_IN_865X
#endif
static void reorder_ctrl_pktout(struct rtl8192cd_priv *priv, struct sk_buff *pskb, struct stat_info *pstat, struct rx_frinfo *pfrinfo)
{
    struct stat_info *dst_pstat = (struct stat_info*)(*(unsigned int *)&(pskb->cb[4]));
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
    struct sk_buff *skb_next;
#endif

    if (dst_pstat == 0) {
#ifdef SUPPORT_RX_AMSDU_AMPDU	
        if(pskb->cb[3]) {			
            process_amsdu(priv, pstat, pfrinfo);
        } else 
#endif        
        {
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
        	if (pskb->next) { /* msdu list */
        		while (pskb){
        			skb_next = pskb->next;
        			pskb->next = pskb->prev = NULL;
        			rtl_netif_rx(priv, pskb, pstat);
        			pskb = skb_next;
        		}
        	} else
#endif
	            rtl_netif_rx(priv, pskb, pstat);
        }	
    }
    else
    {
#ifdef CONFIG_RTL_OFFLOAD_DRIVER	
    	while (pskb)
		{
			skb_next = pskb->next;
			pskb->next = pskb->prev = NULL;		
#endif			
	        #ifdef ENABLE_RTL_SKB_STATS
	        rtl_atomic_dec(&priv->rtl_rx_skb_cnt);
	        #endif
	        #ifdef TX_SCATTER
	        pskb->list_num = 0;
	        #endif
	        #if defined(CONFIG_RTK_MESH) && defined(RX_RL_SHORTCUT)
	        if(pskb->cb[3] == RELAY_11S) {
	            struct tx_insn txcfg;

	            #ifdef NETDEV_NO_PRIV
	            txcfg.priv = ((struct rtl8192cd_priv *)netdev_priv(pskb->dev))->wlan_priv;
	            #else
	            txcfg.priv = pskb->dev->priv;
	            #endif
	            
	            txcfg.is_11s = RELAY_11S;
	            pskb->dev = priv->mesh_dev;

	            priv = txcfg.priv;
	            SMP_LOCK_XMIT(x); 
	            __rtl8192cd_start_xmit_out(pskb, dst_pstat, &txcfg);
	            SMP_UNLOCK_XMIT(x); 
	        }
	        else if(pskb->cb[3] == XMIT_11S) {
	            if(mesh_start_xmit(pskb, priv->mesh_dev))
	                rtl_kfree_skb(priv, pskb, _SKB_TX_);
	        }
	        else
	        #endif
	        if (rtl8192cd_start_xmit(pskb, priv->dev))
	            rtl_kfree_skb(priv, pskb, _SKB_TX_);

#ifdef CONFIG_RTL_OFFLOAD_DRIVER
     		pskb = skb_next;			
    	}
#endif		
    }
}



#if (!defined(__OSK__)) || (defined(__OSK__) && !defined(CONFIG_RTL6028))
__MIPS16
#endif
#ifndef WIFI_MIN_IMEM_USAGE
__IRAM_IN_865X
#endif
void reorder_ctrl_consumeQ(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char tid, int seg)
{
	int win_start, win_size;
	struct reorder_ctrl_entry *rc_entry;

	rc_entry  = &pstat->rc_entry[tid & 7];		// Avoid core dump, we only process TC of 0~7
	win_start = rc_entry->win_start;
	win_size  = priv->pmib->reorderCtrlEntry.ReorderCtrlWinSz;

	while (SN_LESS(win_start, rc_entry->last_seq) || (win_start == rc_entry->last_seq)) {
		if (rc_entry->packet_q[win_start & (win_size - 1)]) {
			reorder_ctrl_pktout(priv, rc_entry->packet_q[win_start & (win_size - 1)], pstat, 
				rc_entry->frinfo_q[win_start & (win_size - 1)]);
			rc_entry->packet_q[win_start & (win_size - 1)] = NULL;
#ifdef SUPPORT_RX_AMSDU_AMPDU			
			rc_entry->frinfo_q[win_start & (win_size - 1)] = NULL;
#endif			
#ifdef _DEBUG_RTL8192CD_
			if (seg == 0)
				pstat->rx_rc_passupi++;
			else if (seg == 2)
				pstat->rx_rc_passup2++;
			else if (seg == 3)
				pstat->rx_rc_passup3++;
			else if (seg == 4)
				pstat->rx_rc_passup4++;
#endif
		}
		win_start = SN_NEXT(win_start);
	}
	rc_entry->start_rcv = FALSE;
}

#if (!defined(__OSK__)) || (defined(__OSK__) && !defined(CONFIG_RTL6028))
__MIPS16
#endif
#ifndef WIFI_MIN_IMEM_USAGE
__IRAM_IN_865X
#endif
void reorder_ctrl_timer_del(struct reorder_ctrl_timer* rc_timer)
{
    rc_timer->priv = NULL;
    rc_timer->pstat = NULL;
    list_del_init(&rc_timer->list);
}

#if (!defined(__OSK__)) || (defined(__OSK__) && !defined(CONFIG_RTL6028))
__MIPS16
#endif
#ifndef WIFI_MIN_IMEM_USAGE
__IRAM_IN_865X
#endif
static void reorder_ctrl_timer_add(struct rtl8192cd_priv *priv, struct stat_info *pstat, int tid, int from_timeout)
{
    struct reorder_ctrl_timer *prc_timer;
    struct reorder_ctrl_entry *rc_entry;
    struct list_head *phead, *plist;
    int idx, sys_tick;
    unsigned long next_timeout = 0;

    // update the rc timer list
    if (!from_timeout) {
        if (timer_pending(&priv->pshare->rc_sys_timer)) {
            del_timer(&priv->pshare->rc_sys_timer);
		}
        reorder_ctrl_timeout((unsigned long)priv);
	}

	for (idx=0; idx<RC_TIMER_NUM; idx++)
		if (priv->pshare->rc_timer_pool[idx].pstat == NULL)
			break;

	if (idx >= RC_TIMER_NUM) {
		pstat->rx_rc_timer_ovf++;
		return;
	}

    rc_entry = &pstat->rc_entry[tid]; 
	if (rc_entry->rc_timer != NULL) {
        reorder_ctrl_timer_del(rc_entry->rc_timer);
        rc_entry->rc_timer = NULL;
	}

    prc_timer = &priv->pshare->rc_timer_pool[idx];
    prc_timer->priv = priv;
    prc_timer->pstat = pstat;
    prc_timer->tid = (unsigned char)tid;
    prc_timer->timeout = jiffies + ((OPMODE & WIFI_STATION_STATE) ? priv->pshare->rc_timer_tick_cli : priv->pshare->rc_timer_tick);
	if (!prc_timer->timeout)
		prc_timer->timeout = 1;

    list_add_tail(&(prc_timer->list), &priv->pshare->rc_timer_list);
    rc_entry->rc_timer = prc_timer;

    // schedule the next timeout
	if (!from_timeout) {
        phead = &priv->pshare->rc_timer_list;
        plist = phead->next;
        while (phead && (plist != phead)) {
            prc_timer = list_entry(plist, struct reorder_ctrl_timer, list);
            plist = plist->next;

            if (next_timeout == 0 || time_before(prc_timer->timeout, next_timeout))
                next_timeout = prc_timer->timeout;
		}

        if (next_timeout > 0) {
            if (time_before_eq(next_timeout, jiffies)) 
                next_timeout = jiffies + 1;
            mod_timer(&priv->pshare->rc_sys_timer, next_timeout);
		}
	}
}

#ifdef __OSK__
__IRAM_WIFI_PRI6
#elif !defined(WIFI_MIN_IMEM_USAGE)
__IRAM_IN_865X
#endif
void reorder_ctrl_timeout(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	unsigned long current_time, next_timeout = 0;
	struct reorder_ctrl_entry *rc_entry=NULL;
	struct rtl8192cd_priv *priv_this=NULL;
	struct stat_info *pstat;
    struct list_head *phead, *plist; 
    struct reorder_ctrl_timer *prc_timer;
	int win_start=0, win_size, tid=0, sys_tick, i;
	unsigned long flags = 0;
#ifdef SMP_SYNC
	int locked = 0;
#endif

	if (!(priv->drv_state & DRV_STATE_OPEN))
		return;

#ifdef PCIE_POWER_SAVING
	if ((priv->pwr_state == L2) || (priv->pwr_state == L1))
		return ;
#endif
#ifdef PCIE_POWER_SAVING_TEST //yllin
    if((priv->pwr_state >= L2) || (priv->pwr_state == L1)) {
            return;
    }
#endif

	SAVE_INT_AND_CLI(flags);
	SMP_TRY_LOCK_REORDER_CTRL(flags, locked);

	current_time = jiffies;
	win_size = priv->pmib->reorderCtrlEntry.ReorderCtrlWinSz;

    phead = &priv->pshare->rc_timer_list;
    plist = phead->next;
    while (phead && (plist != phead) && plist) {
        prc_timer = list_entry(plist, struct reorder_ctrl_timer, list);
        plist = plist->next;

        if (time_after_eq(current_time, prc_timer->timeout)) {
            priv_this = prc_timer->priv;
            tid       = prc_timer->tid;
            pstat     = prc_timer->pstat;
            rc_entry  = &prc_timer->pstat->rc_entry[tid];
            win_start = rc_entry->win_start;

			i=0;
			while (!(rc_entry->packet_q[win_start & (win_size - 1)]) && i<RC_ENTRY_NUM){
				priv->ext_stats.rx_fail++;
				win_start = SN_NEXT(win_start);
				i++;
			}
			while (rc_entry->packet_q[win_start & (win_size - 1)]) {
				reorder_ctrl_pktout(priv_this, rc_entry->packet_q[win_start & (win_size - 1)], pstat,
					rc_entry->frinfo_q[win_start & (win_size - 1)]);
				rc_entry->packet_q[win_start & (win_size - 1)] = NULL;
#ifdef SUPPORT_RX_AMSDU_AMPDU
				rc_entry->frinfo_q[win_start & (win_size - 1)] = NULL;
#endif				
#ifdef _DEBUG_RTL8192CD_
				pstat->rx_rc_passupi++;
#endif
				win_start = SN_NEXT(win_start);
			}

			rc_entry->win_start = win_start;
			if (rc_entry->rc_timer != NULL) {
                reorder_ctrl_timer_del(rc_entry->rc_timer);
                rc_entry->rc_timer = NULL;
			}

			if (SN_LESS(win_start, rc_entry->last_seq) || (win_start == rc_entry->last_seq)) {
                reorder_ctrl_timer_add(priv_this, pstat, tid, 1);
                if (rc_entry->rc_timer == NULL)
					reorder_ctrl_consumeQ(priv_this, pstat, tid, 0);
				else if (next_timeout == 0 || time_before(prc_timer->timeout, next_timeout))
					next_timeout = prc_timer->timeout;
            } else {
				rc_entry->start_rcv = FALSE;
			}
        } else {
            if (next_timeout == 0 || time_before(prc_timer->timeout, next_timeout))
                next_timeout = prc_timer->timeout;
		}
	}

    // schedule the next timeout
    if (next_timeout > 0) {
        if (time_before_eq(next_timeout, jiffies)) 
            next_timeout = jiffies + 1;
        mod_timer(&priv->pshare->rc_sys_timer, next_timeout);
	}

exit:
	RESTORE_INT(flags);
#ifdef SMP_SYNC
	if (locked) {
        SMP_UNLOCK_REORDER_CTRL(flags);
#ifdef CONFIG_RG_NETIF_RX_QUEUE_SUPPORT
        process_rg_netif_rx_queue(priv);
#endif
	}
#endif
}

/* ====================================================================================
   segment   1      2               3                         4
   -----------------+--------------------------------+-------------------------
                  win_start                       win_end
                    +--------------------------------+
                                  win_size

   segment 1: drop this packet
   segment 2: indicate this packet, then indicate the following packets until a hole
   segment 3: queue this packet in corrosponding position
   segment 4: indicate queued packets until SN_DIFF(seq, win_start)<win_size, then
              queue this packet
====================================================================================*/
#ifdef __OSK__
__IRAM_WIFI_PRI4
#else
__MIPS16
__IRAM_IN_865X
#endif
int check_win_seqment(unsigned short win_start, unsigned short win_end, unsigned short seq)
{
	if (SN_LESS(seq, win_start))
		return 1;
	else if (seq == win_start)
		return 2;
	else if (SN_LESS(win_start, seq) && SN_LESS(seq, win_end))
		return 3;
	else
		return 4;
}

#ifdef __OSK__
__IRAM_WIFI_PRI4
#else
__MIPS16
#ifndef WIFI_MIN_IMEM_USAGE
__IRAM_IN_865X
#endif
#endif
#ifndef CONFIG_RTK_MESH
static 
#endif
int reorder_ctrl_check(struct rtl8192cd_priv *priv, struct stat_info *pstat, struct rx_frinfo *pfrinfo)
{
	unsigned short	seq;
	unsigned char	tid;
	int index, segment;
	int win_start, win_size, win_end;
	struct reorder_ctrl_entry *rc_entry;
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif

	SMP_LOCK_REORDER_CTRL(flags);

	seq       = pfrinfo->seq;
	tid       = pfrinfo->tid;
	rc_entry  = &pstat->rc_entry[tid];
	win_start = rc_entry->win_start;
	win_size  = priv->pmib->reorderCtrlEntry.ReorderCtrlWinSz;
	win_end   = (win_start + win_size) & 0xfff;

	if (rc_entry->start_rcv == FALSE)
	{
		if(pfrinfo->paggr) {
			rc_entry->start_rcv = TRUE;
			rc_entry->win_start = SN_NEXT(seq);
			rc_entry->last_seq = seq;
		}
		SMP_UNLOCK_REORDER_CTRL(flags);
		return TRUE;
	}
	else
	{
		segment = check_win_seqment(win_start, win_end, seq);
		if (segment == 1) {
			SMP_UNLOCK_REORDER_CTRL(flags);
#ifdef _DEBUG_RTL8192CD_
			pstat->rx_rc_drop1++;
#endif
			//priv->ext_stats.rx_data_drops++;
			//DEBUG_ERR("RX DROP: skb behind the window\n");
			//rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
			//return FALSE;
			return TRUE;
		}
		else if (segment == 2) {
			reorder_ctrl_pktout(priv, pfrinfo->pskb, pstat, pfrinfo);
#ifdef _DEBUG_RTL8192CD_
			pstat->rx_rc_passup2++;
#endif
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
			if (!pfrinfo->amsdu_cut || pfrinfo->last_msdu)
#endif
			{
				win_start = SN_NEXT(win_start);
				while (rc_entry->packet_q[win_start & (win_size - 1)]) {
					reorder_ctrl_pktout(priv, rc_entry->packet_q[win_start & (win_size - 1)], pstat,
						rc_entry->frinfo_q[win_start & (win_size - 1)]);
					rc_entry->packet_q[win_start & (win_size - 1)] = NULL;
#ifdef SUPPORT_RX_AMSDU_AMPDU				
					rc_entry->frinfo_q[win_start & (win_size - 1)] = NULL;
#endif				
					win_start = SN_NEXT(win_start);
				}
				rc_entry->win_start = win_start;
				if (SN_LESS(rc_entry->last_seq, seq))
					rc_entry->last_seq = seq;

				if (rc_entry->rc_timer != NULL) {
	                reorder_ctrl_timer_del(rc_entry->rc_timer);
	                rc_entry->rc_timer = NULL;
				}
				if (!SN_LESS(rc_entry->last_seq, win_start)) {
	                reorder_ctrl_timer_add(priv, pstat, tid, 0);
	                if (rc_entry->rc_timer == NULL)
						reorder_ctrl_consumeQ(priv, pstat, tid, 2);
				}
			}
			SMP_UNLOCK_REORDER_CTRL(flags);
			return FALSE;
		}
		else if (segment == 3) {
			index = seq & (win_size - 1);
			if (rc_entry->packet_q[index]
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
 				&& (!pfrinfo->amsdu_cut || rc_entry->packet_q[index]->prev==0xBEEFBEEF)
#endif
			) {
				SMP_UNLOCK_REORDER_CTRL(flags);
#ifdef _DEBUG_RTL8192CD_
				pstat->rx_rc_drop3++;
#endif
				priv->ext_stats.rx_data_drops++;
				DEBUG_ERR("RX DROP: skb already in rc queue\n");
#if defined(SUPPORT_RX_AMSDU_AMPDU) && defined(RX_BUFFER_GATHER_REORDER)
				if(pfrinfo && pfrinfo->gather_flag == GATHER_FIRST) {
					flush_rx_list_inq(priv,pfrinfo);
				}
#endif				
				rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
				return FALSE;
			}
			else {
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
				if (pfrinfo->amsdu_cut) {
					if (rc_entry->packet_q[index]) {
						struct sk_buff *msdu = rc_entry->packet_q[index];
						while (msdu->next)
							msdu = msdu->next;
						msdu->next = pfrinfo->pskb;
					} else {
						rc_entry->packet_q[index] = pfrinfo->pskb;
#ifdef SUPPORT_RX_AMSDU_AMPDU
						rc_entry->frinfo_q[index] = pfrinfo;
#endif				
#ifdef _DEBUG_RTL8192CD_
						pstat->rx_rc_reorder3++;
#endif
					}
					if (pfrinfo->last_msdu) {
						rc_entry->packet_q[index]->prev = 0xBEEFBEEF; /* got last_msdu */
					}
				} else
#endif
				{
					rc_entry->packet_q[index] = pfrinfo->pskb;
#ifdef SUPPORT_RX_AMSDU_AMPDU
					rc_entry->frinfo_q[index] = pfrinfo;
#endif				
#ifdef _DEBUG_RTL8192CD_
					pstat->rx_rc_reorder3++;
#endif
				}
			}
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
			if (!pfrinfo->amsdu_cut || pfrinfo->last_msdu)
#endif
			{
				if (SN_LESS(rc_entry->last_seq, seq))
					rc_entry->last_seq = seq;
				if (rc_entry->rc_timer == NULL) {
	                reorder_ctrl_timer_add(priv, pstat, tid, 0);
					if (rc_entry->rc_timer == NULL)
						reorder_ctrl_consumeQ(priv, pstat, tid, 3);
				}
			}
			SMP_UNLOCK_REORDER_CTRL(flags);
			return FALSE;
		}
		else {	// (segment == 4)
			while ((SN_DIFF(seq, win_start) >= win_size) || (rc_entry->packet_q[win_start & (win_size - 1)])) {
				if (rc_entry->packet_q[win_start & (win_size - 1)]) {
					reorder_ctrl_pktout(priv, rc_entry->packet_q[win_start & (win_size - 1)], pstat,
						rc_entry->frinfo_q[win_start & (win_size - 1)]);
					rc_entry->packet_q[win_start & (win_size - 1)] = NULL;
#ifdef SUPPORT_RX_AMSDU_AMPDU					
					rc_entry->frinfo_q[win_start & (win_size - 1)] = NULL;
#endif					
#ifdef _DEBUG_RTL8192CD_
					pstat->rx_rc_passup4++;
#endif
				}
				win_start = SN_NEXT(win_start);
			}
			rc_entry->win_start = win_start;

			index = seq & (win_size - 1);
			if (rc_entry->packet_q[index]) {
#ifdef _DEBUG_RTL8192CD_
				pstat->rx_rc_drop4++;
#endif
				priv->ext_stats.rx_data_drops++;
				DEBUG_ERR("RX DROP: skb already in rc queue\n");
#if defined(SUPPORT_RX_AMSDU_AMPDU) && defined(RX_BUFFER_GATHER_REORDER)
				if(rc_entry->frinfo_q[index] && rc_entry->frinfo_q[index]->gather_flag == GATHER_FIRST) {
					flush_rx_list_inq(priv,rc_entry->frinfo_q[index]);
				}
#endif				
				rtl_kfree_skb(priv, rc_entry->packet_q[index], _SKB_RX_);
			}

			rc_entry->packet_q[index] = pfrinfo->pskb;
#ifdef SUPPORT_RX_AMSDU_AMPDU
			rc_entry->frinfo_q[index] = pfrinfo;
#endif			
			pstat->rx_rc4_count++;
#ifdef _DEBUG_RTL8192CD_
			pstat->rx_rc_reorder4++;
#endif
			rc_entry->last_seq = seq;
            if (rc_entry->rc_timer != NULL) {
                reorder_ctrl_timer_del(rc_entry->rc_timer);
                rc_entry->rc_timer = NULL;
			}
            reorder_ctrl_timer_add(priv, pstat, tid, 0);
            if (rc_entry->rc_timer == NULL)
				reorder_ctrl_consumeQ(priv, pstat, tid, 4);

			SMP_UNLOCK_REORDER_CTRL(flags);
			return FALSE;
		}
	}
}


#ifdef RX_SHORTCUT
/*---------------------------------------------------------------
	return value:
	TRUE:	MIC ok
	FALSE:	MIC error
----------------------------------------------------------------*/
static int wait_mic_done_and_compare(unsigned char *org_mic, unsigned char *tkipmic)
{
#ifdef NOT_RTK_BSP
	return TRUE;
#else
	register unsigned long int l,r;
	int delay = 20;

	while ((*(volatile unsigned int *)GDMAISR & GDMA_COMPIP) == 0) {
		delay_us(delay);
		delay = delay / 2;
	}

	l = *(volatile unsigned int *)GDMAICVL;
	r = *(volatile unsigned int *)GDMAICVR;

	tkipmic[0] = (unsigned char)(l & 0xff);
	tkipmic[1] = (unsigned char)((l >> 8) & 0xff);
	tkipmic[2] = (unsigned char)((l >> 16) & 0xff);
	tkipmic[3] = (unsigned char)((l >> 24) & 0xff);
	tkipmic[4] = (unsigned char)(r & 0xff);
	tkipmic[5] = (unsigned char)((r >> 8) & 0xff);
	tkipmic[6] = (unsigned char)((r >> 16) & 0xff);
	tkipmic[7] = (unsigned char)((r >> 24) & 0xff);

	return (memcmp(org_mic, tkipmic, 8) ? FALSE : TRUE);
#endif // NOT_RTK_BSP
}


#if defined(CONFIG_WLAN_HAL_8197F) 
static __inline__  int get_rx_sc_index(struct stat_info *pstat, unsigned char *pframe)
#else
#ifdef __OSK__
__IRAM_WIFI_PRI4
#else
__MIPS16
__IRAM_IN_865X
#endif
int get_rx_sc_index(struct stat_info *pstat, unsigned char *pframe)
#endif
{
	struct rx_sc_entry *prxsc_entry;
	int i;
	unsigned char *addr;
	prxsc_entry = pstat->rx_sc_ent;

	addr = GetAddr1Ptr(pframe);
	for (i=0; i<RX_SC_ENTRY_NUM; i++) {
#if 1
		if(isEqualMACAddr(addr, prxsc_entry[i].rx_wlanhdr.addr1) &&
			isEqualMACAddr(addr+6, prxsc_entry[i].rx_wlanhdr.addr2) &&
			isEqualMACAddr(addr+12, prxsc_entry[i].rx_wlanhdr.addr3)			
		)
			return i;
#else
		if (!rtk_memcmp(addr prxsc_entry[i].rx_wlanhdr.addr1, 18))
			return i;
#endif
	}

	return -1;
}

#ifdef __OSK__
__IRAM_WIFI_PRI4
#else
__MIPS16
__IRAM_IN_865X
#endif
int get_rx_sc_free_entry(struct stat_info *pstat, unsigned char *pframe)
{
	struct rx_sc_entry *prxsc_entry;
	int i;

	i = get_rx_sc_index(pstat, pframe);
	if (i >= 0)
		return i;
	
	prxsc_entry = pstat->rx_sc_ent;
	
	for (i=0; i<RX_SC_ENTRY_NUM; i++) {
		if (prxsc_entry[i].rx_payload_offset == 0)
			return i;
	}
	
	// no free entry
	i = pstat->rx_sc_replace_idx;
	pstat->rx_sc_replace_idx = (pstat->rx_sc_replace_idx + 1) % RX_SC_ENTRY_NUM;
	return i;
}

#ifdef CLIENT_MODE
inline unsigned char mcast_packet_number_check(struct rtl8192cd_priv *priv, unsigned char *pframe, unsigned int hdr_len)
{
	int ret = 1;
	if(get_mcast_encrypt_algthm(priv) == _CCMP_PRIVACY_) {
		unsigned char keyIdx;
		union PN48 rxPN;
		union PN48 *pCurRxPN;

		if(!GetPrivacy(pframe))	{
			return 0;
		}
		
		rxPN.val48 = 0;
		rxPN._byte_.TSC0 = pframe[hdr_len];
		rxPN._byte_.TSC1 = pframe[hdr_len+1];
		rxPN._byte_.TSC2 = pframe[hdr_len+4];
		rxPN._byte_.TSC3 = pframe[hdr_len+5];
		rxPN._byte_.TSC4 = pframe[hdr_len+6];
		rxPN._byte_.TSC5 = pframe[hdr_len+7];

		keyIdx = (pframe[hdr_len+3] >> 6) & 0x3;
#ifdef GKEY_ID_SECOND
		if(keyIdx == GKEY_ID_SECOND) 
			pCurRxPN = &priv->pmib->dot11GroupKeysTable.dot11EncryptKey2.dot11RXPN48;
		else
#endif
			pCurRxPN = &priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11RXPN48;		
		
		if(rxPN.val48 <= pCurRxPN->val48) {
			priv->ext_stats.rx_mc_pn_drops++;
			ret = 0;
		} else {
			pCurRxPN->val48 = rxPN.val48;
		}			
	}
	return ret;
}
#endif


inline unsigned char sta_packet_number_check(struct rtl8192cd_priv *priv, union PN48 *last_RxPN, unsigned char *pframe, unsigned int hdr_len)
{
	int ret = SUCCESS;
	union PN48 rxPN;

	if(!GetPrivacy(pframe))	{
		return FAIL;
	}

	rxPN.val48 = 0;
	rxPN._byte_.TSC0 = pframe[hdr_len];
	rxPN._byte_.TSC1 = pframe[hdr_len+1];
	rxPN._byte_.TSC2 = pframe[hdr_len+4];
	rxPN._byte_.TSC3 = pframe[hdr_len+5];
	rxPN._byte_.TSC4 = pframe[hdr_len+6];
	rxPN._byte_.TSC5 = pframe[hdr_len+7];

	if((last_RxPN->val48 + 1) != rxPN.val48) {
		ret = FAIL;
	} else {
		last_RxPN->val48 = rxPN.val48;
	}

	return ret;
}


#ifndef RTK_NL80211
static const unsigned char rfc1042_header[WLAN_LLC_HEADER_SIZE]={0xaa,0xaa,0x03,00,00,00}; //mark_wrt
#else
//For OpenWRT, rfc1042_header is defined at compat-wireless/net/wireless/util.c
extern const unsigned char rfc1042_header[];
#endif
/*---------------------------------------------------------------
	return value:
	0:	shortcut ok, rx data has passup
	1:	discard this packet
	-1:	can't do shortcut, data path should be continued
 ---------------------------------------------------------------*/
#ifdef __OSK__
__IRAM_WIFI_PRI4
#else
__MIPS16
#ifndef WIFI_MIN_IMEM_USAGE
__IRAM_IN_865X
#endif
#endif
int rx_shortcut(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
    //	unsigned long flags=0;
    struct stat_info *pstat, *dst_pstat;
    int privacy, tkip_mic_ret=0;
    int payload_length, offset, pos=0, do_rc=0, do_process, idx;
    struct wlan_ethhdr_t *e_hdr;
    unsigned char rxmic[8], tkipmic[8];
    struct sk_buff skb_copy2;
    unsigned char wlanhdr_copy[sizeof(struct wlanllc_hdr)];
    unsigned char *pframe = get_pframe(pfrinfo);
    unsigned char da[MACADDRLEN];
    unsigned short tpcache=0;
	unsigned int hdr_len = pfrinfo->hdr_len;

#if defined(CONFIG_RTK_MESH) && defined(RX_RL_SHORTCUT)
    int mesh_sc_ret;
    unsigned char to_mesh = 0;
    struct MESH_HDR *meshHdrPt;
    unsigned char meshDest[MACADDRLEN];
    unsigned char meshNextHop[MACADDRLEN];
    struct rtl8192cd_priv *dest_priv;
#endif
    struct rx_sc_entry *prxsc_entry = NULL;
    unsigned char is_qos_datafrm=0;

#ifdef HW_FILL_MACID
    pstat = get_stainfo_fast(priv,GetAddr2Ptr(pframe),pfrinfo->macid);
#else
    pstat = get_stainfo(priv, GetAddr2Ptr(pframe));
#endif //#ifdef HW_FILL_MACID

    is_qos_datafrm = is_qos_data(pframe);


#if defined(CONFIG_RTK_MESH) && !defined(RX_RL_SHORTCUT)
	if (GET_MIB(priv)->dot1180211sInfo.mesh_enable && pfrinfo->is_11s)
        goto shouldnot_rxsc;
#endif

#if defined(CONFIG_RTK_MESH) && defined(RX_RL_SHORTCUT)
	if (GET_MIB(priv)->dot1180211sInfo.mesh_enable && pfrinfo->is_11s && memcmp(pfrinfo->da, GET_MY_HWADDR, MACADDRLEN))
		goto shouldnot_rxsc;
#endif

#if defined(RX_BUFFER_GATHER)
	if(pfrinfo->gather_flag)
		goto shouldnot_rxsc;
#endif

#if 0	// already flush cache in rtl8192cd_rx_isr()
#ifndef RTL8190_CACHABLE_CLUSTER
#ifdef __MIPSEB__
	pframe = (UINT8 *)((unsigned long)pframe | 0x20000000);
#endif
#endif
#endif

    if (priv->pmib->dot11OperationEntry.guest_access)
        goto shouldnot_rxsc;

    if (pstat && ((idx = get_rx_sc_index(pstat, pframe)) >= 0))
        prxsc_entry = &pstat->rx_sc_ent[idx];

    if ((NULL != prxsc_entry) &&
        prxsc_entry->rx_payload_offset &&
        (GetFragNum(pframe) == 0) && (GetMFrag(pframe) == 0))
    {
        privacy = GetPrivacy(pframe);
		//memcpy(da, pfrinfo->da, MACADDRLEN);
		copyMACAddr(da, pfrinfo->da);

        tpcache = GetTupleCache(pframe);
#ifdef CLIENT_MODE
		if (IS_MCAST(da) && (OPMODE & WIFI_STATION_STATE))
        {
            if (tpcache == pstat->tpcache_mcast)
            {
                priv->ext_stats.rx_decache++;
                rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
                SNMP_MIB_INC(dot11FrameDuplicateCount, 1);
                return 0;
            }

			if(!mcast_packet_number_check(priv, pframe, hdr_len)) {
				rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
				return 0;
			}				
        }
        else
#endif
        if (is_qos_datafrm) {
            pos = GetSequence(pframe) & (TUPLE_WINDOW - 1);
            if (tpcache == pstat->tpcache[pfrinfo->tid][pos]) {
                priv->ext_stats.rx_decache++;
                rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
                SNMP_MIB_INC(dot11FrameDuplicateCount, 1);
                return 0;
            }
        }
        else {
            if (GetRetry(pframe)) {
                if (tpcache == pstat->tpcache_mgt) {
                    priv->ext_stats.rx_decache++;
                    rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
                    SNMP_MIB_INC(dot11FrameDuplicateCount, 1);
                    return 0;
                }
            }
        }

        // check wlan header
        if (pstat->rx_privacy == privacy)
        {

#if defined(CONFIG_RTK_MESH) && defined(RX_RL_SHORTCUT)
            if (pfrinfo->is_11s) {
                mesh_sc_ret = mesh_shortcut_update(priv, pfrinfo, prxsc_entry, &dest_priv, meshNextHop, &to_mesh, &meshHdrPt);
                if(mesh_sc_ret == -1) {
                    goto shouldnot_rxsc;
                }
                else if(mesh_sc_ret == 1) {
                    rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
                    return 1;
                }

                if(to_mesh == 0) {
                    memcpy(meshDest, meshHdrPt->DestMACAddr, MACADDRLEN);/*backup mesh addr5 */
                }
                pfrinfo->pskb->dev = priv->mesh_dev;
            }
            else
#endif //CONFIG_RTK_MESH

#ifdef WDS
            if (pfrinfo->to_fr_ds == 3 && priv->pmib->dot11WdsInfo.wdsEnabled) {
                if (memcmp(GetAddr4Ptr(pframe), prxsc_entry->rx_wlanhdr.addr4, 6))
                    goto shouldnot_rxsc;
                pfrinfo->pskb->dev = getWdsDevByAddr(priv, GetAddr2Ptr(pframe));
            }
            else
#endif

#ifdef A4_STA
            if (pfrinfo->to_fr_ds == 3 && priv->pmib->miscEntry.a4_enable) {
                if(!(pstat->state & WIFI_A4_STA))
                    goto shouldnot_rxsc;
                if (memcmp(GetAddr4Ptr(pframe), prxsc_entry->rx_wlanhdr.addr4, 6))
                    goto shouldnot_rxsc;
                a4_sta_add(priv, pstat, GetAddr4Ptr(pframe));
                pfrinfo->pskb->dev = priv->dev;
            }
            else
#endif
                pfrinfo->pskb->dev = priv->dev;

            offset = prxsc_entry->rx_payload_offset + sizeof(rfc1042_header);

            /*
                    printk("pstat->rx_payload_offset = %02x, 0xaa, cmp = %d \n",pframe[prxsc_entry->rx_payload_offset],
                    memcmp(&pframe[prxsc_entry->rx_payload_offset], rfc1042_header, 6));
                    printk("pstat->rx_ethhdr.type = %02x, pframe[offset] = %02x, cmp= %d \n", prxsc_entry->rx_ethhdr.type, pframe[offset],
                    memcmp(&prxsc_entry->rx_ethhdr.type,&pframe[offset], 2));
                    */
            // check snap header
            //if (memcmp(&pframe[prxsc_entry->rx_payload_offset], rfc1042_header, 6) ||
            //	memcmp(&prxsc_entry->rx_ethhdr.type,&pframe[offset], 2))
            if (!isEqualMACAddr(&pframe[prxsc_entry->rx_payload_offset], rfc1042_header) ||
                *(unsigned short *)(&prxsc_entry->rx_ethhdr.type) != *(unsigned short *)(&pframe[offset])
            )
                goto shouldnot_rxsc;

            payload_length = pfrinfo->pktlen - offset - prxsc_entry->rx_trim_pad - 2;
            if (payload_length < WLAN_ETHHDR_LEN)
                goto shouldnot_rxsc;

            if (privacy)
            {

#ifdef WDS
                if (pfrinfo->to_fr_ds == 3 && priv->pmib->dot11WdsInfo.wdsEnabled )
                    privacy = priv->pmib->dot11WdsInfo.wdsPrivacy;

                else
#endif
                    privacy = get_sta_encrypt_algthm(priv, pstat);

#ifdef CONFIG_RTL_WAPI_SUPPORT
                if (privacy==_WAPI_SMS4_)
                {
                    /*	Decryption	*/
                    if (SecSWSMS4Decryption(priv, pstat, pfrinfo) == FAIL)
                    {
                        priv->ext_stats.rx_data_drops++;
                        DEBUG_ERR("RX DROP: WAPI decrpt error!\n");
                        rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
                        return 1;
                    }
                    pframe = get_pframe(pfrinfo);
                }
                else
#endif
                {
                    if (privacy == _TKIP_PRIVACY_)
                    {
                        memcpy((void *)rxmic, (void *)(pframe + pfrinfo->pktlen - 8 - 4), 8); // 8 michael, 4 icv
                        //SAVE_INT_AND_CLI(flags);
                        tkip_mic_ret = tkip_rx_mic(priv, pframe, pfrinfo->da, pfrinfo->sa,
                        pfrinfo->tid, pframe + pfrinfo->hdr_len + 8,
                        pfrinfo->pktlen - pfrinfo->hdr_len - 8 - 8 - 4, tkipmic, 1); // 8 IV, 8 Mic, 4 ICV
                        if (tkip_mic_ret) { // MIC completed
                            //RESTORE_INT(flags);
                            if (memcmp(rxmic, tkipmic, 8)) {
                                goto shouldnot_rxsc;
                            }
                        }
                        else {
                            memcpy(&skb_copy2, pfrinfo->pskb, sizeof(skb_copy2));
                            memcpy(wlanhdr_copy, pframe, sizeof(wlanhdr_copy));
                        }
                    }
                }
            }

            if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) &&
                 priv->pmib->reorderCtrlEntry.ReorderCtrlEnable) {
                if (!IS_MCAST(GetAddr1Ptr(pframe)))
                    do_rc = 1;
            }


            rx_sum_up(NULL, pstat, pfrinfo);

            pstat->rx_sc_pkts++;

            priv->pshare->NumRxBytesUnicast += pfrinfo->pktlen;

            update_sta_rssi(priv, pstat, pfrinfo);

#ifdef DETECT_STA_EXISTANCE
#ifdef CONFIG_RTL_88E_SUPPORT
            if (GET_CHIP_VER(priv)==VERSION_8188E) {
                if (pstat->leave!= 0)
                    RTL8188E_MACID_NOLINK(priv, 0, REMAP_AID(pstat));
            }
#endif

#ifdef CONFIG_WLAN_HAL
            if(IS_HAL_CHIP(priv))
            {
                if (pstat->leave!= 0)
                {
#if defined(CONFIG_PCI_HCI)
					if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
#if defined(TAROKO_0)
                        HAL_H2C_MSRRPT_PARA MSRRPT_para;
                        MSRRPT_para.macid = pstat->cmn_info.mac_id;
                        MSRRPT_para.opmode = INCREASE;
        
                        GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, &MSRRPT_para);
#else
                        GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, pstat, INCREASE);
#endif
	                    pstat->bDrop = 0;
					}
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
					if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
                    	notify_update_sta_msr(priv, pstat, INCREASE);
					}
#endif
                    pstat->cmn_info.dm_ctrl |= STA_DM_CTRL_ACTIVE;
                }
                pstat->rx_last_good_time = priv->up_time;
            }
#endif //#ifdef CONFIG_WLAN_HAL

            pstat->leave = 0;
#endif

            //printk("RXSC\n");

#ifdef SUPPORT_SNMP_MIB
            if (IS_MCAST(da))
                SNMP_MIB_INC(dot11MulticastReceivedFrameCount, 1);
#endif

            /* chop 802.11 header from skb. */
            //skb_put(pfrinfo->pskb, pfrinfo->pktlen);	// pskb->tail will be wrong
            skb_set_tail_pointer(pfrinfo->pskb, pfrinfo->pktlen);
            pfrinfo->pskb->len = pfrinfo->pktlen;

            skb_pull(pfrinfo->pskb, offset+2);

#if defined(CONFIG_RTK_MESH) && defined(RX_RL_SHORTCUT)
            if (pfrinfo->is_11s && to_mesh == 0) {
                skb_pull(pfrinfo->pskb, 16); //because we guarantee only 6 address frame enters shortcut
                payload_length -= 16;
            }
#endif

            e_hdr = (struct wlan_ethhdr_t *)skb_push(pfrinfo->pskb, WLAN_ETHHDR_LEN);
            memcpy((unsigned char *)e_hdr, (unsigned char *)&prxsc_entry->rx_ethhdr, sizeof(struct wlan_ethhdr_t));
            /* chop off the 802.11 CRC */

            skb_trim(pfrinfo->pskb, payload_length + WLAN_ETHHDR_LEN);


            //printk("RXSC skb_pull offset+2 = %d\n", offset+2);
            //printk("RXSC pstat->rx_ethhdr dest= %02x\n", e_hdr->daddr[5]);
            //printk("RXSC pstat->rx_ethhdr src = %02x\n", e_hdr->saddr[5]);

            if ((privacy == _TKIP_PRIVACY_) && (tkip_mic_ret == FALSE)) {
                if (wait_mic_done_and_compare(rxmic, tkipmic) == FALSE) {
                    //					RESTORE_INT(flags);
                    memcpy(pfrinfo->pskb, &skb_copy2, sizeof(skb_copy2));
                    memcpy(pframe, wlanhdr_copy, sizeof(wlanhdr_copy));
                    goto shouldnot_rxsc;
                }
                //				RESTORE_INT(flags);
            }

#ifdef CLIENT_MODE
			if (IS_MCAST(da) && (OPMODE & WIFI_STATION_STATE))
                pstat->tpcache_mcast = tpcache;
            else
#endif
            if (is_qos_datafrm)
                pstat->tpcache[pfrinfo->tid][pos] = tpcache;
            else
                pstat->tpcache_mgt = tpcache;

            if (OPMODE & WIFI_AP_STATE)
            {

#if defined(CONFIG_RTK_MESH) && defined(RX_RL_SHORTCUT)
                pfrinfo->pskb->cb[3] = 0;
                if (pfrinfo->is_11s ) { /* from wlan0-msh0 */
                    if(to_mesh) { /* to wlan0-msh0 */
                        meshHdrPt->TTL--;
                        if(1 > meshHdrPt->TTL) {
                            priv->ext_stats.rx_data_drops++;
                            DEBUG_ERR("RX DROP: TTL=0\n");
                            rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
                            return 1;
                        }

                        dst_pstat = get_stainfo(dest_priv, meshNextHop);
                        if(dst_pstat) {
                            do_process = 0;
                            if (do_rc) {
                                *(unsigned int *)&(pfrinfo->pskb->cb[4]) = dst_pstat;
                                pfrinfo->pskb->cb[3] = RELAY_11S;
                                pfrinfo->pskb->dev = dest_priv->dev; /* temprarily save the priv to nexthop */
                                if (reorder_ctrl_check(priv, pstat, pfrinfo)) {
                                    do_process = 1;
                                }
                            } else {
                                do_process = 1;                        
                            }

                            if(do_process) {                                
                                struct tx_insn txcfg;

                                #ifdef ENABLE_RTL_SKB_STATS
                                rtl_atomic_dec(&priv->rtl_rx_skb_cnt);
                                rtl_atomic_inc(&priv->rtl_tx_skb_cnt);
                                #endif

                                txcfg.priv = dest_priv;
                                txcfg.is_11s = RELAY_11S;
                                pfrinfo->pskb->dev = priv->mesh_dev;         
                                priv = dest_priv;
                                SMP_LOCK_XMIT(x);                                 
                                __rtl8192cd_start_xmit_out(pfrinfo->pskb, dst_pstat, &txcfg); 
                                SMP_UNLOCK_XMIT(x);                                
                            }
                            return 0;
                        }
                    }
                    else if(priv->mesh_priv_first != NULL && priv->mesh_priv_first->pmib->dot1180211sInfo.mesh_portal_enable){ /* to wlan0 or eth0 */
                        dst_pstat = get_stainfo(priv, meshDest); /* mesh addr5 */
                    }
                    else
                        dst_pstat = NULL;
                }
                else { /* from wlan0 */
                    dst_pstat = get_stainfo(priv, da);
#if defined(CONFIG_RTL_MESH_SINGLE_IFACE)
                    if(dst_pstat) {
                        if(isMeshPoint(dst_pstat) && priv->mesh_priv_first !=NULL && priv->mesh_priv_first->pmib->dot1180211sInfo.mesh_portal_enable == 0)
                            dst_pstat = NULL;
                    }
#endif /* defined(CONFIG_RTL_MESH_SINGLE_IFACE) */
                    #ifdef BR_SHORTCUT
                    else if(dst_pstat == NULL     
                    #if defined(CONFIG_RTL_MESH_SINGLE_IFACE)
                        && priv->mesh_priv_first != NULL && priv->mesh_priv_first->pmib->dot1180211sInfo.mesh_portal_enable
                    #endif
                        ) {
                        int index = 0;
                        #ifdef CONFIG_RTL_MESH_CROSSBAND
                        index = (priv->dev->name[4] == '0')?1:0;
                        #endif                        
                        if(cached_mesh_dev[index] && isEqualMACAddr(da, cached_mesh_mac[index])) { /* to wlan0-msh0 */
                            #ifdef CONFIG_RTL_MESH_CROSSBAND
                            pfrinfo->pskb->dev = priv->mesh_priv_sc->dev;
                            #elif defined(CONFIG_RTL_MESH_SINGLE_IFACE)
                            pfrinfo->pskb->dev = priv->mesh_priv_first->dev;
                            #endif
                            do_process = 0;
                            if (do_rc) {
                                pfrinfo->pskb->cb[4] = 1;
                                pfrinfo->pskb->cb[3] = XMIT_11S;
                                if (reorder_ctrl_check(priv, pstat, pfrinfo)) {
                                    do_process = 1;
                                }
                            }
                            else
                                do_process = 1;

                            if(do_process) {
                                if(mesh_start_xmit(pfrinfo->pskb, priv->mesh_dev))
                                    rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_TX_);
                            }
                            return 0;
                        }
                    }
                    #endif // #ifdef BR_SHORTCUT
                }
#else //defined(CONFIG_RTK_MESH) && defined(RX_RL_SHORTCUT)                        
                dst_pstat = get_stainfo(priv, da);
#endif         



#ifdef A4_STA
                if (priv->pmib->miscEntry.a4_enable && (dst_pstat == NULL))
                    dst_pstat = a4_sta_lookup(priv, da);
#endif

                if ((dst_pstat == NULL) || (!(dst_pstat->state & WIFI_ASOC_STATE))
                    #if defined(WDS)
                    || (pfrinfo->to_fr_ds==3 && priv->pmib->dot11WdsInfo.wdsEnabled)
                    #endif
                )
				{
                   
#ifdef A4_STA
	                if((GET_BR_PORT(priv->dev) && priv->pmib->miscEntry.a4_enable == 2 && fdb_find_for_driver(GET_BR_PORT(priv->dev)->br, da) == NULL) || (priv->pmib->miscEntry.a4_enable == 1 && IS_MCAST(da))) {
	                    struct sk_buff  *pnewskb = NULL;
	                    pnewskb = skb_copy(pfrinfo->pskb, GFP_ATOMIC);
	                    if (pnewskb) {

	                        if ( 0
								#ifdef GBWC
								|| (GET_HCI_TYPE(priv) == RTL_HCI_PCIE && GBWC_forward_check(priv, pnewskb, pstat))
								#endif
								#ifdef SBWC
								|| SBWC_forward_check(priv, pnewskb, pstat)
								#endif
								) 
							{
								// packet is queued, nothing to do
							}
							else
	                        {
#ifdef TX_SCATTER
	                            pnewskb->list_num = 0;
#endif                            
	                            if (rtl8192cd_start_xmit(pnewskb, priv->dev))
	                                rtl_kfree_skb(priv, pnewskb, _SKB_TX_);
	                        }
	                    }
	                }
#endif //A4_STA
                    if (do_rc) {
                        *(unsigned int *)&(pfrinfo->pskb->cb[4]) = 0;
                        if (reorder_ctrl_check(priv, pstat, pfrinfo)) {
                            rtl_netif_rx(priv, pfrinfo->pskb, pstat);
                        }
                    }
                    else
                        rtl_netif_rx(priv, pfrinfo->pskb, pstat);
				}

                else
                {
                    if (priv->pmib->dot11OperationEntry.block_relay == 1) {
                        priv->ext_stats.rx_data_drops++;
                        DEBUG_ERR("RX DROP: Relay unicast packet is blocked in shortcut!\n");
                        rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
                        return 1;
                    }
                    else if (priv->pmib->dot11OperationEntry.block_relay == 2) {
                        DEBUG_INFO("Relay unicast packet is blocked! Indicate to bridge in shortcut\n");
                        rtl_netif_rx(priv, pfrinfo->pskb, pstat);
                    }
                    #ifdef HS2_SUPPORT
                    else if (priv->pmib->dot11OperationEntry.block_relay == 3  /*l2_inspect , only for 4.7-B*/
                            &&(IS_ICMPV4_PROTO(pfrinfo->pskb->data) && IS_ICMPV4_ECHO_TYPE(pfrinfo->pskb->data))
                            &&(priv->pmib->hs2Entry.ICMPv4ECHO != 1) 
                    ) { 

                        HS2DEBUG("block_relay=3\n");
                        //unsigned short protocol;

                        if(priv->pmib->hs2Entry.ICMPv4ECHO == 2) {
                            HS2DEBUG("redirect ICMPv4 Packet to connected portal\n");
                            rtl_netif_rx(priv, pfrinfo->pskb, pstat);
                        }
                        else if(priv->pmib->hs2Entry.ICMPv4ECHO == 0) {
                            HS2DEBUG("Drop ICMPv4 Packet\n");
                            rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);		
                        }
                        #if 0 /*2014 1127 sgs log test*/
                        else if(priv->pmib->hs2Entry.ICMPv4ECHO == 1) {
                            HS2DEBUG("Allow ICMPv4 Echo Requests\n");

                            #if defined(CONFIG_RTK_MESH) && defined(RX_RL_SHORTCUT)
                            if (rtl8192cd_start_xmit(pfrinfo->pskb, isMeshPoint(dst_pstat)? priv->mesh_dev: priv->dev))
                            #else
                            if (rtl8192cd_start_xmit(pfrinfo->pskb, priv->dev))
                            #endif
                                rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
                        }
                        #endif					
                        return 1;
                    	
                    }
                    #endif	/*end of HS2_SUPPORT*/				
                    else {
                        do_process = 0;
                        #ifdef ENABLE_RTL_SKB_STATS
                        rtl_atomic_dec(&priv->rtl_rx_skb_cnt);
                        #endif
                        if ( 0 
							#ifdef GBWC
							|| (GET_HCI_TYPE(priv) == RTL_HCI_PCIE && GBWC_forward_check(priv, pfrinfo->pskb, pstat))
							#endif
							#ifdef SBWC
							|| SBWC_forward_check(priv, pfrinfo->pskb, pstat)
							#endif
						) {
							// packet is queued, nothing to do
						}
						else if (do_rc) {
                            #ifdef CONFIG_RTK_MESH
                            if(isMeshPoint(dst_pstat)) {
                                pfrinfo->pskb->cb[4] = 1;
                                pfrinfo->pskb->cb[3] = XMIT_11S;
                            }
                            else                  
                            #endif                            
                                *(unsigned int *)&(pfrinfo->pskb->cb[4]) = (unsigned int)dst_pstat;	// backup pstat pointer
                                
#ifdef ENABLE_RTL_SKB_STATS
                            rtl_atomic_inc(&priv->rtl_rx_skb_cnt);
#endif
                            if (reorder_ctrl_check(priv, pstat, pfrinfo)) {
#ifdef ENABLE_RTL_SKB_STATS
                                rtl_atomic_dec(&priv->rtl_rx_skb_cnt);
#endif
                                do_process = 1;
                            }
                        }
                        else {
                            do_process = 1;
                        }

                        if(do_process) {

                            #ifdef TX_SCATTER
                            pfrinfo->pskb->list_num = 0;
                            #endif

                            #ifdef CONFIG_RTK_MESH
                            if(isMeshPoint(dst_pstat)) {                            
                                if(mesh_start_xmit(pfrinfo->pskb, priv->mesh_dev))
                                    rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_TX_);
                            }
                            else
                            #endif 
                            {
                                if (rtl8192cd_start_xmit(pfrinfo->pskb, priv->dev))
                                    rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_TX_);
                            }
                        }

                    }
                }
                return 0;
			}
#ifdef CLIENT_MODE
            else if (OPMODE & (WIFI_STATION_STATE | WIFI_ADHOC_STATE)) {
                priv->rxDataNumInPeriod++;
                if (IS_MCAST(pfrinfo->pskb->data)) {
                    priv->rxMlcstDataNumInPeriod++;
                } else if ((OPMODE & WIFI_STATION_STATE) && (priv->ps_state)) {
                    if ((GetFrameSubType(pframe) == WIFI_DATA)
#ifdef WIFI_WMM
                        ||(QOS_ENABLE && pstat->QosEnabled && (GetFrameSubType(pframe) == WIFI_QOS_DATA))
#endif
                    ) {
                        if (GetMData(pframe)) {
#if defined(WIFI_WMM) && defined(WMM_APSD)
                            if (QOS_ENABLE && APSD_ENABLE && priv->uapsd_assoc) {
                                if (!((priv->pmib->dot11QosEntry.UAPSD_AC_BE && ((pfrinfo->tid == 0) || (pfrinfo->tid == 3))) ||
                                        (priv->pmib->dot11QosEntry.UAPSD_AC_BK && ((pfrinfo->tid == 1) || (pfrinfo->tid == 2))) ||
                                        (priv->pmib->dot11QosEntry.UAPSD_AC_VI && ((pfrinfo->tid == 4) || (pfrinfo->tid == 5))) ||
                                        (priv->pmib->dot11QosEntry.UAPSD_AC_VO && ((pfrinfo->tid == 6) || (pfrinfo->tid == 7)))))
                                    {
#ifdef CONFIG_PE_ENABLE
                                        if(priv->pshare->rf_ft_var.manual_pe_enable){
                                            //do nothing
                                        }else
#endif                                            
                                        issue_PsPoll(priv);

                                    }
                            } else
#endif
                            {
#ifdef CONFIG_PE_ENABLE
                                if(priv->pshare->rf_ft_var.manual_pe_enable){
                                    //do nothing
                                }else
#endif                                            
                                issue_PsPoll(priv);

                            }
                        }
                    }
                }

#ifdef RTK_BR_EXT
#ifdef A4_STA
                if(!(pstat->state & WIFI_A4_STA))
#endif
                {
                    if (!priv->pmib->ethBrExtInfo.nat25sc_disable &&
                        !(pfrinfo->pskb->data[0] & 1) &&
                        *((unsigned short *)(pfrinfo->pskb->data+ETH_ALEN*2)) == __constant_htons(ETH_P_IP) &&
                        isEqualIPAddr(priv->scdb_ip, pfrinfo->pskb->data+ETH_HLEN+16)) {
                        memcpy(pfrinfo->pskb->data, priv->scdb_mac, ETH_ALEN);
#ifdef MULTI_MAC_CLONE
						mclone_dhcp_caddr(priv, pfrinfo->pskb);
#endif
					}
                    else
                    if(nat25_handle_frame(priv, pfrinfo->pskb) == -1) {
                        priv->ext_stats.rx_data_drops++;
                        DEBUG_ERR("RX DROP: nat25_handle_frame fail in shortcut!\n");
                        rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
                        return 1;
                    }
                }
#endif
                if (do_rc) {
                    *(unsigned int *)&(pfrinfo->pskb->cb[4]) = 0;
                    if (reorder_ctrl_check(priv, pstat, pfrinfo))
                        rtl_netif_rx(priv, pfrinfo->pskb, pstat);
                }
                else
                    rtl_netif_rx(priv, pfrinfo->pskb, pstat);
                return 0;
            }
#endif
            else
            {
                priv->ext_stats.rx_data_drops++;
                DEBUG_ERR("RX DROP: Non supported mode in process_datafrme in shortcut\n");
                rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
                return 1;
            }
        }
    }

shouldnot_rxsc:

    if (pstat)
        pstat->rx_sc_pkts_slow++;
    return -1;
}
#endif // RX_SHORTCUT

// The purpose of reassemble is to assemble the frag into a complete one.
static struct rx_frinfo *reassemble(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	struct list_head	*phead, *plist;
	unsigned short		seq=0;
	unsigned char		tofr_ds=0;
	unsigned int		iv, icv, mic, privacy=0, offset;
	struct sk_buff		*pskb;
	struct rx_frinfo	*pfrinfo, *pfirstfrinfo=NULL;
	unsigned char		*pframe=NULL, *pfirstframe=NULL, tail[16]; //4, 12, and 8 for WEP/TKIP/AES
	unsigned long		flags;
	int			i;
	union PN48	last_PN;
	
	struct list_head frag_list;
	
	INIT_LIST_HEAD(&frag_list);
	
	DEFRAG_LOCK(flags);
	list_del_init(&pstat->defrag_list);
	list_splice_init(&pstat->frag_list, &frag_list);
	pstat->frag_count = 0;
	DEFRAG_UNLOCK(flags);
	
	phead = &frag_list;

	// checking  all the seq should be the same, and the frg should monotically increase
	for(i=0, plist=phead->next; plist!=phead; plist=plist->next, i++)
	{
		pfrinfo = list_entry(plist, struct rx_frinfo, mpdu_list);
		pframe = get_pframe(pfrinfo);

		if ((GetFragNum(pframe)) != i)
		{
			DEBUG_ERR("RX DROP: FragNum did not match, FragNum=%d, GetFragNum(pframe)=%d\n",
				i, GetFragNum(pframe));
			goto unchainned_all;
		}

		if (i == 0)
		{
			seq = GetSequence(pframe);
			privacy = GetPrivacy(pframe);
			tofr_ds = pfrinfo->to_fr_ds;
			if (get_sta_encrypt_algthm(priv, pstat) == _CCMP_PRIVACY_)
			{
				last_PN.val48 = 0;
				last_PN._byte_.TSC0 = pframe[pfrinfo->hdr_len];
				last_PN._byte_.TSC1 = pframe[pfrinfo->hdr_len+1];
				last_PN._byte_.TSC2 = pframe[pfrinfo->hdr_len+4];
				last_PN._byte_.TSC3 = pframe[pfrinfo->hdr_len+5];
				last_PN._byte_.TSC4 = pframe[pfrinfo->hdr_len+6];
				last_PN._byte_.TSC5 = pframe[pfrinfo->hdr_len+7];
			}
		}
		else
		{
			if (get_sta_encrypt_algthm(priv, pstat) == _CCMP_PRIVACY_ &&
				sta_packet_number_check(priv, &last_PN, pframe, pfrinfo->hdr_len) != SUCCESS)
			{
				DEBUG_ERR("RX DROP: Non-consecutive Packet Number\n");
				goto unchainned_all;
			}

			if (GetSequence(pframe) != seq)
			{
				DEBUG_ERR("RX DROP: Seq is not correct, seq=%d, GetSequence(pframe)=%d\n",
					seq, GetSequence(pframe));
				goto unchainned_all;
			}

			if (GetPrivacy(pframe) != privacy)
			{
				DEBUG_ERR("RX DROP: Privacy is not correct, privacy=%d, GetPrivacy(pframe)=%d\n",
					privacy, GetPrivacy(pframe));
				goto unchainned_all;
			}

			if (pfrinfo->to_fr_ds != tofr_ds)
			{
				DEBUG_ERR("RX DROP: to_fr_ds did not match, tofr_ds=%d, pfrinfo->to_fr_ds=%d\n",
					tofr_ds, pfrinfo->to_fr_ds);
				goto unchainned_all;
			}
		}
	}

	privacy = get_privacy(priv, pstat, &iv, &icv, &mic);

	offset = iv;
	offset += get_hdrlen(priv, pframe);

	// below we are going to re-assemble the whole pkts...
	for(i=0, plist=phead->next; plist!=phead; plist=plist->next, i++)
	{
		pfrinfo = list_entry(plist, struct rx_frinfo, mpdu_list);

		if (pfrinfo->pktlen <= (offset + icv + mic))
		{
			DEBUG_ERR("RX DROP: Frame length bad (%d)\n", pfrinfo->pktlen);
			pfirstfrinfo = NULL;
			goto unchainned_all;
		}

		if (i == 0)
		{
			pfirstfrinfo = pfrinfo;
			pfirstframe  = get_pframe(pfrinfo);
			pfirstframe += pfrinfo->pktlen - (icv + mic);

			if (icv + mic)
			{
				if ((icv + mic) <= sizeof(tail))
					memcpy((void *)tail, (void *)(pfirstframe), (icv + mic));
				pfirstfrinfo->pktlen -= (icv + mic);
			}
			continue;
		}

		// check if too many frags...
		pfirstfrinfo->pktlen += (pfrinfo->pktlen - offset - icv - mic);
		{
			int run = 0;
#if defined(CONFIG_PCI_HCI)
			if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
				if (pfirstfrinfo->pktlen >= (RX_BUF_LEN - offset - icv - mic - 200)) {
					run = 1;
				}
			}
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
			if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
				if ((pfirstframe + (pfrinfo->pktlen - offset)) >=
					((unsigned char *)((unsigned long)skb_end_pointer(pfirstfrinfo->pskb)))) {
					run = 1;
				}
			}
#endif

			if (run)
			{
				DEBUG_ERR("RX DROP: over rx buf size after reassemble...\n");
				pfirstfrinfo = NULL;
				goto unchainned_all;
			}
		}

		// here we should check if all these frags exceeds the buf size
		memcpy(pfirstframe, get_pframe(pfrinfo) + offset, pfrinfo->pktlen - offset - icv - mic);
		pfirstframe += (pfrinfo->pktlen - offset - icv - mic);
	}

	if (icv + mic)
	{
		memcpy((void *)pfirstframe, (void *)tail, (icv + mic));
		pfirstfrinfo->pktlen += (icv + mic);
	}

	// take the first frame out of fragment list
	plist = phead->next;
	list_del_init(plist);

unchainned_all:	// dequeue all the queued-up frag, free skb, and init_list_head again...
	
	while (!list_empty(phead)) {
		plist = phead->next;
		list_del(plist);
		
		pfrinfo = list_entry(plist, struct rx_frinfo, mpdu_list);
		pskb = get_pskb(pfrinfo);
		rtl_kfree_skb(priv, pskb, _SKB_RX_IRQ_);
		
		if (pfirstfrinfo == NULL)
			priv->ext_stats.rx_data_drops++;
	}

	return pfirstfrinfo;
}

static int auth_filter(struct rtl8192cd_priv *priv, struct stat_info *pstat,
				struct rx_frinfo *pfrinfo)
{
	unsigned int	hdr_len;
	unsigned char	*pframe, *pbuf;
	unsigned short	proto;

//	hdr_len = pfrinfo->hdr_len;
//	pframe = get_pframe(pfrinfo);
//	pbuf = pframe + hdr_len + sizeof(struct wlan_llc_t) + 3;
//	proto = *(unsigned short *)pbuf;

	if(IEEE8021X_FUN) {
		if (pstat) {
			if (pstat->ieee8021x_ctrlport) // controlled port is enable...
				return SUCCESS;
			else {
				//only 802.1x frame can pass...
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
				if (OFFLOAD_ENABLE(priv) && pfrinfo->rxpkt_info->pkt_type==RXPKTINFO_TYPE_8023) {
					pframe = get_pframe(pfrinfo);
					pbuf = pframe + (2 * MACADDRLEN);
				}
				else
#endif // CONFIG_RTL_OFFLOAD_DRIVER
				{
				hdr_len = pfrinfo->hdr_len;
				pframe = get_pframe(pfrinfo);
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
				if ((GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO)
					&& (GetPrivacy(pframe)))
					pbuf = pframe + hdr_len + sizeof(struct wlan_llc_t) + 3 + 8;	// 8 bytes IV
				else
#endif
#if defined(RTK_NL80211)
					if (GetPrivacy(pframe) && (
#ifdef CONFIG_IEEE80211W  
						UseSwCrypto(priv, pstat, IS_MCAST(GetAddr1Ptr(pframe)), pstat->isPMF)
#else
						UseSwCrypto(priv, pstat, IS_MCAST(GetAddr1Ptr(pframe))) 
#endif
						  || get_sta_encrypt_algthm(priv, pstat)==_TKIP_PRIVACY_
						  || get_sta_encrypt_algthm(priv, pstat)==_CCMP_PRIVACY_)
					)
						pbuf = pframe + hdr_len + sizeof(struct wlan_llc_t) + 3 + 8;	// 8 bytes IV
					else
#endif
						pbuf = pframe + hdr_len + sizeof(struct wlan_llc_t) + 3;
				}

				proto = *(unsigned short *)pbuf;
				if (proto == __constant_htons(0x888e))
					return SUCCESS;
				else {
					return FAIL;
				}
			}
		}
		else {
			DEBUG_ERR("pstat == NULL in auth_filter\n");
			return FAIL;
		}
	}

	return SUCCESS;
}

// Keep the last time of issuing deauth and avoid to issue another deauth within 2000 ms interval
int deauth_filter(struct rtl8192cd_priv *priv, unsigned char *sa)
{
    unsigned int i, ret = SUCCESS;
    unsigned long now = jiffies;
    struct deauth_sta_info *pdeauth_pool = &priv->pshare->deauth_sta_pool;

    // delete the old sta while the last deauth issue time > 2000 ms
    while (pdeauth_pool->head != pdeauth_pool->tail)
    {
        if (time_before_eq(now, pdeauth_pool->issue_time[pdeauth_pool->head] + RTL_MILISECONDS_TO_JIFFIES(2000)))
            break;

        DEBUG_INFO("[%s %d] Delete STA (%02X:%02X:%02X:%02X:%02X:%02X) with %lu\n", __FUNCTION__, __LINE__, 
            pdeauth_pool->mac_addr[pdeauth_pool->head][0], pdeauth_pool->mac_addr[pdeauth_pool->head][1], 
            pdeauth_pool->mac_addr[pdeauth_pool->head][2], pdeauth_pool->mac_addr[pdeauth_pool->head][3], 
            pdeauth_pool->mac_addr[pdeauth_pool->head][4], pdeauth_pool->mac_addr[pdeauth_pool->head][5], 
            pdeauth_pool->issue_time[pdeauth_pool->head]);
        pdeauth_pool->head = (pdeauth_pool->head + 1) % MAX_DEAUTH_STA_NUM;
    }

    // search the deauth sta by mac address
    i = pdeauth_pool->head;
    while (i != pdeauth_pool->tail)
    {
        if (!memcmp(pdeauth_pool->mac_addr[i], sa, MACADDRLEN))
        {
            ret = FAIL;
            break;
        }
        
        i = (i + 1) % MAX_DEAUTH_STA_NUM;
    }

    if (ret == SUCCESS)
    {
        if (pdeauth_pool->tail != ((pdeauth_pool->head - 1 + MAX_DEAUTH_STA_NUM) % MAX_DEAUTH_STA_NUM))
        {
            memcpy(pdeauth_pool->mac_addr[pdeauth_pool->tail], sa, MACADDRLEN);
            pdeauth_pool->issue_time[pdeauth_pool->tail] = now;

            DEBUG_INFO("[%s %d] Add STA (%02X:%02X:%02X:%02X:%02X:%02X) with %lu\n", __FUNCTION__, __LINE__, 
                pdeauth_pool->mac_addr[pdeauth_pool->tail][0], pdeauth_pool->mac_addr[pdeauth_pool->tail][1], 
                pdeauth_pool->mac_addr[pdeauth_pool->tail][2], pdeauth_pool->mac_addr[pdeauth_pool->tail][3], 
                pdeauth_pool->mac_addr[pdeauth_pool->tail][4], pdeauth_pool->mac_addr[pdeauth_pool->tail][5], 
                pdeauth_pool->issue_time[pdeauth_pool->tail]);
            
            pdeauth_pool->tail = (pdeauth_pool->tail + 1) % MAX_DEAUTH_STA_NUM;
        }        
    }

    return ret;
}



/*----------------------------------------------------------------------------------------
	So far, only data pkt will be defragmented.
-----------------------------------------------------------------------------------------*/
static struct rx_frinfo *defrag_frame_main(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
    unsigned char *da, *sa;
    struct stat_info *pstat=NULL;
    unsigned int  res, hdr_len, len;
    int status, privacy=0, pos;
    unsigned long flags;
    unsigned char tkipmic[8], rxmic[8];
    unsigned char *pframe;
    unsigned char hw_didnt_decrypt=0;
    unsigned char amsdu=0;

    pframe = get_pframe(pfrinfo);
    hdr_len = pfrinfo->hdr_len;
    da	= pfrinfo->da;
    sa  = pfrinfo->sa;
    len = pfrinfo->pktlen;
    amsdu = pfrinfo->amsdu;
#if defined(CONFIG_RTK_MESH) || defined(WDS) || defined(A4_STA)
    if(pfrinfo->to_fr_ds == 3) {       
        da = GetAddr1Ptr(pframe);
        sa = GetAddr2Ptr(pframe);
    }
#endif

    /*---------first of all check if sa is assocated---------*/
    if (OPMODE & WIFI_AP_STATE) {
        #ifdef HW_FILL_MACID
        pstat = get_stainfo_fast(priv,sa,pfrinfo->macid);               
        #else
        pstat = get_stainfo(priv,sa);               
        #endif //#ifdef HW_FILL_MACID

        #ifdef A4_STA
        if (priv->pmib->miscEntry.a4_enable && pstat) {
            pstat = a4_rx_check_reuse_ap(priv, pstat, pfrinfo);
        }
        #endif        
    }
    #ifdef CLIENT_MODE
    else if (OPMODE & WIFI_STATION_STATE) {
        unsigned char *bssid = GetAddr2Ptr(pframe);
        pstat = get_stainfo(priv, bssid);
    }
    else	// Ad-hoc
        pstat = get_stainfo(priv, sa);
    #endif

#if defined(CONFIG_RTK_MESH) || defined(WDS)
    if(pfrinfo->to_fr_ds == 3 && (priv->pmib->dot11WdsInfo.wdsEnabled || priv->pmib->dot1180211sInfo.mesh_enable)) {
        if (pstat == NULL){
            priv->ext_stats.rx_data_drops++;
            panic_printk("RX Drop: WDS/MESH rx data with pstat == NULL\n");
            goto free_skb_in_defrag;
        } 
    } else
#endif
	if (pstat == NULL)
	{
		status = _RSON_CLS3_;
		priv->ext_stats.rx_data_drops++;
		DEBUG_WARN("RX DROP: class 3 error (STA info is empty)!\n");
		goto data_defrag_error;
	}
	else if (!(pstat->state & WIFI_ASOC_STATE))
	{   
		status = _RSON_CLS3_;
		priv->ext_stats.rx_data_drops++;
		DEBUG_ERR("RX DROP: class 3 error!\n");
		goto data_defrag_error;
	}
	else {}

    /*-------------------get privacy-------------------*/
    if (OPMODE & WIFI_AP_STATE) {
        #if defined(CONFIG_RTK_MESH) || defined(WDS) || defined(A4_STA)
        if(pfrinfo->to_fr_ds == 3) {
            #ifdef CONFIG_RTK_MESH
            if(pfrinfo->is_11s)
                privacy = IS_MCAST(da) ? _NO_PRIVACY_ : get_sta_encrypt_algthm(priv, pstat);
            else
            #endif
            #ifdef WDS
            if (priv->pmib->dot11WdsInfo.wdsEnabled)
                privacy = priv->pmib->dot11WdsInfo.wdsPrivacy;
            else
            #endif
                privacy = get_sta_encrypt_algthm(priv, pstat);
        }
        else
        #endif
            privacy = get_sta_encrypt_algthm(priv, pstat);
    }
#ifdef CLIENT_MODE
    else {
		if (IS_MCAST(da)) 
		{
			privacy = get_mcast_encrypt_algthm(priv);
		}
		else
		{
			privacy = get_sta_encrypt_algthm(priv, pstat);
		}
    }

	if (IS_MCAST(da))
	{
		if (GetTupleCache(pframe) == pstat->tpcache_mcast)
		{
			priv->ext_stats.rx_decache++;
			SNMP_MIB_INC(dot11FrameDuplicateCount, 1);
			goto free_skb_in_defrag;
		}
		else
			pstat->tpcache_mcast = GetTupleCache(pframe);

		if((OPMODE & WIFI_STATION_STATE) && !mcast_packet_number_check(priv, pframe, hdr_len)) {
			goto free_skb_in_defrag;
		}
		
	}
	else
#endif
	/*-------------------check retry-------------------*/
	if (is_qos_data(pframe)) {
		pos = GetSequence(pframe) & (TUPLE_WINDOW - 1);
		if (IS_MCAST(da)) {
			// ignore check of multicast packet to workaround Veriware test.
		}
		else if (GetTupleCache(pframe) == pstat->tpcache[pfrinfo->tid][pos]) {
			priv->ext_stats.rx_decache++;
			SNMP_MIB_INC(dot11FrameDuplicateCount, 1);
			goto free_skb_in_defrag;
		}
		else
			pstat->tpcache[pfrinfo->tid][pos] = GetTupleCache(pframe);
        }
	else {
		if (GetRetry(pframe)) {
			if (GetTupleCache(pframe) == pstat->tpcache_mgt) {
				priv->ext_stats.rx_decache++;
				SNMP_MIB_INC(dot11FrameDuplicateCount, 1);
				goto free_skb_in_defrag;
			}
		}
		pstat->tpcache_mgt = GetTupleCache(pframe);
	}

	/*-------------------------------------------------------*/
	/*-----------insert MPDU-based decrypt below-------------*/
	/*-------------------------------------------------------*/

#ifdef SUPPORT_SNMP_MIB
	if (GetPrivacy(pframe) && privacy == _NO_PRIVACY_)
		SNMP_MIB_INC(dot11WEPUndecryptableCount, 1);

	if (!GetPrivacy(pframe) && privacy != _NO_PRIVACY_)
		SNMP_MIB_INC(dot11WEPExcludedCount, 1);
#endif

#ifdef CLIENT_MODE
	if((OPMODE & WIFI_STATION_STATE) && (priv->pmib->wscEntry.wsc_enable==1) && (privacy == _WEP_40_PRIVACY_ || privacy == _WEP_104_PRIVACY_))
	{
		DEBUG_TRACE("RX : Recv unencrypted packet when encryption is WEP!, Don't drop the EAPOL packet when doing WPS!\n");
	}
	else
#endif
#if defined(CONFIG_RTL_WAPI_SUPPORT)
	if (privacy==_WAPI_SMS4_)
	{
		//WAPI authentication packet is not encryption, but privacy==_WAPI_SMS4_. This can avoid WAPI authentication packet is droped!!!
	}
	else
#endif
        //if (!GetPrivacy(pframe) && privacy != _NO_PRIVACY_)
        if (!GetPrivacy(pframe) && privacy != _NO_PRIVACY_  && privacy != _WAPI_SMS4_ )
        {
				unsigned short proto;
			proto = (pframe[hdr_len + sizeof(struct wlan_llc_t) + 3] <<8)
				+ pframe[hdr_len + sizeof(struct wlan_llc_t) + 4];
			DEBUG_INFO("proto:%04x\n", proto);
			if (amsdu || proto != 0x888e) {
                priv->ext_stats.rx_data_drops++;
                DEBUG_ERR("RX DROP: Recv unencrypted packet!\n");
                goto free_skb_in_defrag;
        	}
        }

#ifdef CONFIG_IEEE80211W
	if (GetPrivacy(pframe) && !UseSwCrypto(priv, pstat, IS_MCAST(GetAddr1Ptr(pframe)), 0) && pfrinfo->sw_dec)
#else
	if (GetPrivacy(pframe) && !UseSwCrypto(priv, pstat, IS_MCAST(GetAddr1Ptr(pframe))) && pfrinfo->sw_dec)
#endif
	{	
		hw_didnt_decrypt = 1;
	}

	// check whether WEP bit is set in mac header and sw encryption
#ifdef CONFIG_IEEE80211W
		if (GetPrivacy(pframe) && (UseSwCrypto(priv, pstat, IS_MCAST(GetAddr1Ptr(pframe)), 0)||hw_didnt_decrypt)) // 0: PMF
#else
		if (GetPrivacy(pframe) && (UseSwCrypto(priv, pstat, IS_MCAST(GetAddr1Ptr(pframe)))||hw_didnt_decrypt))
#endif
	{
#if defined(CONFIG_RTL_WAPI_SUPPORT)
		if (privacy==_WAPI_SMS4_)
		{
			/*	Decryption	*/
//			SAVE_INT_AND_CLI(flags);
			res = SecSWSMS4Decryption(priv, pstat, pfrinfo);
//			RESTORE_INT(flags);
			if (res == FAIL)
			{
				priv->ext_stats.rx_data_drops++;
				DEBUG_ERR("RX DROP: WAPI decrpt error!\n");
				goto free_skb_in_defrag;
			}
			pframe = get_pframe(pfrinfo);
		} else
#endif
		if (privacy == _TKIP_PRIVACY_)
		{
			res = tkip_decrypt(priv, pfrinfo, pfrinfo->pktlen);
			if (res == FAIL)
			{
				priv->ext_stats.rx_data_drops++;
				DEBUG_ERR("RX DROP: Tkip decrpt error!\n");
				goto free_skb_in_defrag;
			}
		}
		else if (privacy == _CCMP_PRIVACY_)
		{
			res = aesccmp_decrypt(priv, pfrinfo
#ifdef CONFIG_IEEE80211W				
				, 0
#endif
			);
			if (res == FALSE)
			{
				priv->ext_stats.rx_data_drops++;
				DEBUG_ERR("RX DROP: AES decrpt error!\n");
				goto free_skb_in_defrag;
			}
		}
		else if (privacy == _WEP_40_PRIVACY_ || privacy == _WEP_104_PRIVACY_)
		{
			res = wep_decrypt(priv, pfrinfo, pfrinfo->pktlen, privacy, 0);
			if (res == FAIL)
			{
				priv->ext_stats.rx_data_drops++;
				DEBUG_ERR("RX DROP: WEP decrpt error!\n");
				goto free_skb_in_defrag;
			}
		}
		else
		{
			DEBUG_ERR("RX DROP: encrypted packet but no key in sta or wrong enc type!\n");
			goto free_skb_in_defrag;
		}

	}
	/*----------------End of MPDU-based decrypt--------------*/

	if (GetMFrag(pframe))
	{
		if (pstat->frag_count > MAX_FRAG_COUNT)
		{
			priv->ext_stats.rx_data_drops++;
			DEBUG_ERR("RX DROP: station has received too many frags!\n");
			goto free_skb_in_defrag;
		}
		
		DEFRAG_LOCK(flags);

		if (pstat->frag_count == 0) // the first frag...
			pstat->frag_to = priv->frag_to;

		if (list_empty(&pstat->defrag_list))
			list_add_tail(&pstat->defrag_list, &priv->defrag_list);

		list_add_tail(&pfrinfo->mpdu_list, &pstat->frag_list);
		pstat->frag_count++;
		
		DEFRAG_UNLOCK(flags);
		
		return (struct rx_frinfo *)NULL;
	}
	else
	{
		if(GetFragNum(pframe))
		{
			DEFRAG_LOCK(flags);
			list_add_tail(&pfrinfo->mpdu_list, &pstat->frag_list);
			DEFRAG_UNLOCK(flags);
			
			pfrinfo = reassemble(priv, pstat);
			if (pfrinfo == NULL)
				return (struct rx_frinfo *)NULL;
		}
	}

	/*-----discard non-authorized packet before MIC check----*/
	if (OPMODE & WIFI_AP_STATE) {
#if defined(CONFIG_RTK_MESH) || defined(WDS)
		if (pfrinfo->to_fr_ds != 3)
#endif
		if (auth_filter(priv, pstat, pfrinfo) == FAIL) {
			priv->ext_stats.rx_data_drops++;
			DEBUG_ERR("RX DROP: due to auth_filter fails\n");
			goto free_skb_in_defrag;
		}
	}
#ifdef CLIENT_MODE
	else if (OPMODE & WIFI_STATION_STATE) {
		if (auth_filter(priv, pstat, pfrinfo) == FAIL) {
			priv->ext_stats.rx_data_drops++;
			DEBUG_ERR("RX DROP: due to auth_filter fails\n");
			goto free_skb_in_defrag;
		}
	}
#endif

	/*------------------------------------------------------------------------*/
						//insert MSDU-based digest here!
	/*------------------------------------------------------------------------*/
	if (GetPrivacy(pframe) && (privacy == _TKIP_PRIVACY_))
	{
		pframe = get_pframe(pfrinfo);
		len = pfrinfo->pktlen;

		//truncate Michael...
		memcpy((void *)rxmic, (void *)(pframe + len - 8 - 4), 8); // 8 michael, 4 icv
		SAVE_INT_AND_CLI(flags);
		tkip_rx_mic(priv, pframe, pfrinfo->da,  pfrinfo->sa,
			pfrinfo->tid, pframe + hdr_len + 8,
			len - hdr_len - 8 - 8 - 4, tkipmic, 0); // 8 IV, 8 Mic, 4 ICV
		RESTORE_INT(flags);

		if(memcmp(rxmic, tkipmic, 8))
		{
			priv->ext_stats.rx_data_drops++;
#ifdef _SINUX_
			printk("RX DROP: MIC error! Indicate to protection mechanism\n");
			mic_error_report(0);
#else
			DEBUG_ERR("RX DROP: MIC error! Indicate to protection mechanism\n");
#endif
			if (OPMODE & WIFI_AP_STATE) {
#ifdef RTL_WPA2
#ifdef _SINUX_
				printk("%s: DOT11_Indicate_MIC_Failure %02X:%02X:%02X:%02X:%02X:%02X \n", (char *)__FUNCTION__,pstat->cmn_info.mac_addr[0],pstat->cmn_info.mac_addr[1],pstat->cmn_info.mac_addr[2],pstat->cmn_info.mac_addr[3],pstat->cmn_info.mac_addr[4],pstat->cmn_info.mac_addr[5]);
#else
				PRINT_INFO("%s: DOT11_Indicate_MIC_Failure %02X:%02X:%02X:%02X:%02X:%02X \n", (char *)__FUNCTION__,pstat->cmn_info.mac_addr[0],pstat->cmn_info.mac_addr[1],pstat->cmn_info.mac_addr[2],pstat->cmn_info.mac_addr[3],pstat->cmn_info.mac_addr[4],pstat->cmn_info.mac_addr[5]);
#endif

#endif
#ifdef WDS
				if ((pfrinfo->to_fr_ds == 3) &&
						pstat && (pstat->state & WIFI_WDS))
					goto free_skb_in_defrag;
#endif

#if defined(TAROKO_0)
//TODO, translate to IPC				DOT11_Indicate_MIC_Failure(priv->dev, pstat);
#else
				DOT11_Indicate_MIC_Failure(priv->dev, pstat);
#endif

			}
#ifdef CLIENT_MODE
			else if (OPMODE & WIFI_STATION_STATE)
				DOT11_Indicate_MIC_Failure_Clnt(priv, pfrinfo->sa);
#endif
			goto free_skb_in_defrag;
		}
	}

	return pfrinfo;

data_defrag_error:

	if (OPMODE & WIFI_AP_STATE){
        if (deauth_filter(priv, sa)) {
			DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_SERIOUS, "issue_deauth - %02X:%02X:%02X:%02X:%02X:%02X, Reason: %d\n",
								sa[0],sa[1],sa[2],sa[3],sa[4],sa[5], status);
            issue_deauth(priv, sa, status);            
            //issue_disassoc(priv, sa, status);
        }
		if (pstat != NULL){
#if defined(CONFIG_PCI_HCI)
			if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
				free_stainfo(priv, pstat);
			}
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
			if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
				notify_del_sta(priv, pstat);
			}
#endif
		}
	}
#ifdef CLIENT_MODE
	else {
		if (pstat == NULL) {
			DEBUG_ERR("rx data with pstat == NULL\n");
		}
		else if (!(pstat->state & WIFI_ASOC_STATE)) {
			DEBUG_ERR("rx data with pstat not associated\n");
		}
	}
#endif

free_skb_in_defrag:

#ifdef RX_BUFFER_GATHER_REORDER
	if(pfrinfo->gather_flag == GATHER_FIRST) {
			flush_rx_list_inq(priv,pfrinfo);
	}		
#endif		
	rtl_kfree_skb(priv, get_pskb(pfrinfo), _SKB_RX_);

	return (struct rx_frinfo *)NULL;
}


static struct rx_frinfo *defrag_frame(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
    struct sk_buff *pskb = get_pskb(pfrinfo);
    struct rx_frinfo *prx_frinfo = NULL;
    unsigned int encrypt;
    unsigned char *pframe;
    int drop = 0;

    
    pframe = get_pframe(pfrinfo);
    encrypt = GetPrivacy(pframe);

    /* rx a encrypt packet but encryption is not enabled in local mib, discard it*/
    /* rx a non-encrypt packet but encryption is not enabled in local mib, discard it*/
    
    #if defined(CONFIG_RTK_MESH) || defined(WDS) || defined(A4_STA)
    if(pfrinfo->to_fr_ds==3) {
        #ifdef CONFIG_RTK_MESH
        if(GET_MIB(priv)->dot1180211sInfo.mesh_enable) {
            if((encrypt && GET_MIB(priv)->dot11sKeysTable.dot11Privacy ==_NO_PRIVACY_) ||
               (!encrypt && GET_MIB(priv)->dot11sKeysTable.dot11Privacy !=_NO_PRIVACY_))
            drop = 1;
        } else
        #endif
        #ifdef WDS
        if(GET_MIB(priv)->dot11WdsInfo.wdsEnabled) {
            if((encrypt && GET_MIB(priv)->dot11WdsInfo.wdsPrivacy ==_NO_PRIVACY_) ||
               (!encrypt && GET_MIB(priv)->dot11WdsInfo.wdsPrivacy !=_NO_PRIVACY_))
                drop = 1;
        } else
        #endif
        #ifdef A4_STA
        if(priv->pmib->miscEntry.a4_enable == 1) {
#ifdef RTK_MULTI_AP
            if((encrypt && priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _NO_PRIVACY_))
#else
            if((encrypt && priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _NO_PRIVACY_) ||
               (!encrypt && priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm != _NO_PRIVACY_))
#endif
                drop = 1;
	} else if(priv->pmib->miscEntry.a4_enable == 2) {
		if((encrypt && priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _NO_PRIVACY_))
			drop = 1;
	}
	else
        #endif
        {;}
    } else       
    #endif
    {
        if(encrypt && priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _NO_PRIVACY_
            #if defined(CONFIG_RTL_WAPI_SUPPORT)
            && priv->pmib->wapiInfo.wapiType==wapiDisable
            #endif
        )
        {
            drop = 1;
        }
    }

    if(drop) {
        priv->ext_stats.rx_data_drops++;
        if(encrypt) {
            DEBUG_ERR("RX DROP: Discard a encrypted packet!\n");
        }
        else {           
            SNMP_MIB_INC(dot11WEPExcludedCount, 1);
            DEBUG_ERR("RX DROP: Discard a un-encrypted 4-address(WDS/MESH/A4_STA) packet!\n");
	}
#ifdef RX_BUFFER_GATHER_REORDER
		if(pfrinfo->gather_flag == GATHER_FIRST)
			flush_rx_list_inq(priv,pfrinfo);
#endif	
		rtl_kfree_skb(priv, pskb, _SKB_RX_);
		return (struct rx_frinfo *)NULL;
	}

    prx_frinfo = defrag_frame_main(priv, pfrinfo);
    return prx_frinfo;
}


static int process_datafrme(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
	unsigned char 	*pframe, da[MACADDRLEN];
	unsigned int  	privacy;
	unsigned int	res;
	struct stat_info *pstat = NULL, *dst_pstat = NULL;
	struct sk_buff	 *pskb  = NULL, *pnewskb = NULL;
	unsigned char	qosControl[2];
	int dontBcast2otherSta = 0, do_rc = 0, do_process;
	struct net_bridge_port *br_port=NULL;
#ifdef RX_SHORTCUT
	int i;
#endif

#ifdef CLIENT_MODE
    unsigned char * dest;
#endif

	pframe = get_pframe(pfrinfo);

	pskb = get_pskb(pfrinfo);
	//skb_put(pskb, pfrinfo->pktlen);	// pskb->tail will be wrong
	skb_set_tail_pointer(pskb, pfrinfo->pktlen);
	pskb->len = pfrinfo->pktlen;
	pskb->dev = priv->dev;

	if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) &&
		priv->pmib->reorderCtrlEntry.ReorderCtrlEnable) {
		if (!IS_MCAST(GetAddr1Ptr(pframe)))
			do_rc = 1;
	}

	if (OPMODE & WIFI_AP_STATE)
	{
		memcpy(da, pfrinfo->da, MACADDRLEN);

#ifdef CONFIG_RTK_MESH
		if (pfrinfo->is_11s)
		{
			pstat = get_stainfo(priv, GetAddr2Ptr(pframe));

			rx_sum_up(NULL, pstat, pfrinfo);

			priv->pshare->NumRxBytesUnicast += pfrinfo->pktlen;	
			update_sta_rssi(priv, pstat, pfrinfo);
			return process_11s_datafrme(priv,pfrinfo, pstat);
		}
		else
#endif
#ifdef WDS
		if (pfrinfo->to_fr_ds == 3 && priv->pmib->dot11WdsInfo.wdsEnabled) {
			pstat = get_stainfo(priv, GetAddr2Ptr(pframe));
			pskb->dev = getWdsDevByAddr(priv, GetAddr2Ptr(pframe));
		}
		else
#endif
#ifdef A4_STA
		if (pfrinfo->to_fr_ds == 3 &&  priv->pmib->miscEntry.a4_enable) {
			pstat = get_stainfo(priv, GetAddr2Ptr(pframe));
		}
		else
#endif
        {
            #ifdef HW_FILL_MACID
            pstat = get_stainfo_fast(priv,pfrinfo->sa,pfrinfo->macid);              
            #else
            pstat = get_stainfo(priv,pfrinfo->sa);               
            #endif //#ifdef HW_FILL_MACID
		}

		if (NULL == pstat) {
			priv->ext_stats.rx_data_drops++;
			DEBUG_ERR("RX DROP: pstat is NULL\n");
			return FAIL;
		}

#if defined(CONFIG_RTL_WAPI_SUPPORT)
#if defined(CONFIG_PCI_HCI)
		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
			if (wapiHandleRecvPacket(pfrinfo, pstat)==SUCCESS)
			{
				return SUCCESS;
			}
		}
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
		if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
			if (pstat->wapiInfo && (wapiDisable != pstat->wapiInfo->wapiType))
			{
				int waiOffset;
				if ((SecIsWAIPacket(pskb, &waiOffset) == SUCCESS) ||
						(pstat->wapiInfo->wapiState != ST_WAPI_AE_MSKA_ESTABLISH))
				{
					notify_recv_wai_frame(priv, pfrinfo);
					return SUCCESS;
				}
			}
		}
#endif
#endif // CONFIG_RTL_WAPI_SUPPORT

		// log rx statistics...
#ifdef  WDS
		if (pfrinfo->to_fr_ds == 3 && priv->pmib->dot11WdsInfo.wdsEnabled) {
			privacy = priv->pmib->dot11WdsInfo.wdsPrivacy;
		}
		else
#endif
		{
			privacy = get_sta_encrypt_algthm(priv, pstat);
		}

		rx_sum_up(NULL, pstat, pfrinfo);

		priv->pshare->NumRxBytesUnicast += pfrinfo->pktlen;	
		update_sta_rssi(priv, pstat, pfrinfo);

#ifdef DETECT_STA_EXISTANCE
#ifdef CONFIG_RTL_88E_SUPPORT
        if (GET_CHIP_VER(priv)==VERSION_8188E) {
            if (pstat->leave!= 0)
                RTL8188E_MACID_NOLINK(priv, 0, REMAP_AID(pstat));
        }
#endif
#ifdef CONFIG_WLAN_HAL
        if(IS_HAL_CHIP(priv))
        {
            if (pstat->leave!= 0)
            {
#if defined(CONFIG_PCI_HCI)
				if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
#if defined(TAROKO_0)
                    HAL_H2C_MSRRPT_PARA MSRRPT_para;
                    MSRRPT_para.macid = pstat->cmn_info.mac_id;
                    MSRRPT_para.opmode = INCREASE;
    
                    GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, &MSRRPT_para);
#else
                    GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, pstat, INCREASE);
#endif
	                pstat->bDrop = 0;
				}
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
				if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
                	notify_update_sta_msr(priv, pstat, INCREASE);
				}
#endif
                pstat->cmn_info.dm_ctrl |= STA_DM_CTRL_ACTIVE;
            }
            pstat->rx_last_good_time = priv->up_time;
        }
#endif //#ifdef CONFIG_WLAN_HAL

		pstat->leave = 0;
#endif

#ifdef SUPPORT_SNMP_MIB
		if (IS_MCAST(da))
			SNMP_MIB_INC(dot11MulticastReceivedFrameCount, 1);
#endif

#if defined(WIFI_WMM) && defined(WMM_APSD)
		if ((QOS_ENABLE) && (APSD_ENABLE) && (pstat->QosEnabled) && (pstat->apsd_bitmap & 0x0f) &&
			((pstat->state & (WIFI_ASOC_STATE|WIFI_SLEEP_STATE)) == (WIFI_ASOC_STATE|WIFI_SLEEP_STATE)) &&
			(GetFrameSubType(get_pframe(pfrinfo)) == (WIFI_QOS_DATA))) {
#ifdef CONFIG_WLAN_HAL
			if(IS_HAL_CHIP(priv)){
#if defined(TAROKO_0)
                HAL_H2C_MSRRPT_PARA MSRRPT_para;
                MSRRPT_para.macid = pstat->cmn_info.mac_id;
                MSRRPT_para.opmode = INCREASE;

                GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, &MSRRPT_para);
#else
                GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, pstat, INCREASE);
#endif
				pstat->txpdrop_flag = 0;

				GET_HAL_INTERFACE(priv)->SetMACIDSleepHandler(priv, 0 , REMAP_AID(pstat));                
				if(priv->pshare->paused_sta_num && pstat->txpause_flag) {
					priv->pshare->paused_sta_num--;	
					pstat->txpause_flag =0;
			    	}
			}
#endif  
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
			if(GET_CHIP_VER(priv)== VERSION_8812E || GET_CHIP_VER(priv) == VERSION_8723B){
				UpdateHalMSRRPT8812(priv, pstat, INCREASE);
				pstat->txpdrop_flag = 0;

				RTL8812_MACID_PAUSE(priv, 0, REMAP_AID(pstat));
				if(priv->pshare->paused_sta_num && pstat->txpause_flag) {
					priv->pshare->paused_sta_num--;	
					pstat->txpause_flag =0;
			    	}
			}
#endif
#ifdef CONFIG_RTL_88E_SUPPORT
			if(GET_CHIP_VER(priv)== VERSION_8188E){
				RTL8188E_MACID_NOLINK(priv, 0, REAP_AID(pstat));
				pstat->txpdrop_flag = 0;
				
				RTL8188E_MACID_PAUSE(priv, 0, REMAP_AID(pstat));
				if(priv->pshare->paused_sta_num && pstat->txpause_flag) {
					priv->pshare->paused_sta_num--;	
					pstat->txpause_flag =0;
			    	}
			}
#endif
				process_APSD_dz_queue(priv, pstat, pfrinfo->tid);
		}
#endif

		// Process A-MSDU
		if (is_qos_data(pframe))
		{
			if (pfrinfo->amsdu)	// A-MSDU present
			{
#ifdef RTK_AC_SUPPORT
				if(pstat->vht_cap_len ==0)
#endif			
				if (!pstat->is_realtek_sta && (pstat->IOTPeer!=HT_IOT_PEER_RALINK) && (pstat->IOTPeer!=HT_IOT_PEER_MARVELL)) {
					pstat->IOTPeer=HT_IOT_PEER_MARVELL;
					if (priv->pshare->is_40m_bw) {
						setSTABitMap(&priv->pshare->marvellMapBit, pstat->cmn_info.aid);
					}
				}

#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL_8881A) || defined(CONFIG_RTL_8723B_SUPPORT) || defined(CONFIG_WLAN_HAL_8198F)
				if ((GET_CHIP_VER(priv)== VERSION_8812E) || (GET_CHIP_VER(priv)== VERSION_8881A) || (GET_CHIP_VER(priv) == VERSION_8723B)|| (GET_CHIP_VER(priv) == VERSION_8198F)) {
					//Do Nothing
				}
				else
#endif
				if (priv->pshare->is_40m_bw && (pstat->IOTPeer==HT_IOT_PEER_MARVELL) && (priv->pshare->Reg_RRSR_2 == 0) && (priv->pshare->Reg_81b == 0)) {
#if defined(CONFIG_PCI_HCI)
					if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
						priv->pshare->Reg_RRSR_2 = RTL_R8(RRSR+2);
						priv->pshare->Reg_81b = RTL_R8(0x81b);
						RTL_W8(RRSR+2, priv->pshare->Reg_RRSR_2 | 0x60);
						RTL_W8(0x81b, priv->pshare->Reg_81b | 0x0E);
					}
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
					if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
						notify_40M_RRSR_SC_change(priv);
					}
#endif
				}
#ifndef SUPPORT_RX_AMSDU_AMPDU
				process_amsdu(priv, pstat, pfrinfo);
#else
				// do rc check when ampdu_amsdu
				if (do_rc) {
					*(unsigned int *)&(pfrinfo->pskb->cb[4]) = 0;
					(pfrinfo->pskb->cb[3]) = 1;
					if (reorder_ctrl_check(priv, pstat, pfrinfo))
						process_amsdu(priv, pstat, pfrinfo);
				}
				else
					process_amsdu(priv, pstat, pfrinfo);

#endif
				return SUCCESS;
			}
#ifdef RX_BUFFER_GATHER
#ifdef RX_BUFFER_GATHER_REORDER
#else
			else if (!list_empty(&priv->pshare->gather_list))
				flush_rx_list(priv);
#endif			
#endif
		}

#ifdef PREVENT_BROADCAST_STORM
//			if (get_free_memory() < FREE_MEM_LOWER_BOUND) {
			if (da[0] == 0xff) {
				pstat->rx_pkts_bc++;
				#if 0
				if (pstat->rx_pkts_bc > BROADCAST_STORM_THRESHOLD) {
					priv->ext_stats.rx_data_drops++;
					DEBUG_ERR("RX DROP: Broadcast storm happened!\n");
					return FAIL;
				}
				#endif
			}
#endif

		// AP always receive unicast frame only
#if defined(WDS) || defined(A4_STA)
		if (pfrinfo->to_fr_ds!=3 && IS_MCAST(da))
#else
		if (IS_MCAST(da))
#endif
		{
#ifdef DRVMAC_LB
			if (priv->pmib->miscEntry.drvmac_lb) {
				priv->ext_stats.rx_data_drops++;
				DEBUG_ERR("RX DROP: drop br/ml packet in loop-back mode!\n");
				return FAIL;
			}
#endif

#if 0 /*def PREVENT_BROADCAST_STORM*/
//			if (get_free_memory() < FREE_MEM_LOWER_BOUND) {
			if (da[0] == 0xff) {
				pstat->rx_pkts_bc++;
				if (pstat->rx_pkts_bc > BROADCAST_STORM_THRESHOLD) {
					priv->ext_stats.rx_data_drops++;
					DEBUG_ERR("RX DROP: Broadcast storm happened!\n");
					return FAIL;
				}
			}
#endif

#if defined(TAROKO_0)
			res = 1;
#else
			// This is a legal frame, convert it to skb
			res = skb_p80211_to_ether(priv->dev, privacy, pfrinfo);
#endif

			if (res == FAIL) {
				priv->ext_stats.rx_data_drops++;
				DEBUG_ERR("RX DROP: skb_p80211_to_ether fail!\n");
				return FAIL;
			}

#ifdef PREVENT_ARP_SPOOFING
			if (check_arp_spoofing(priv, pskb)) {
				priv->ext_stats.rx_data_drops++;
				DEBUG_ERR("RX DROP: Detected ARP spoofing!\n");
				return FAIL;
			}
#endif

#ifdef  SUPPORT_TX_MCAST2UNI
			if ((IP_MCAST_MAC(pskb->data) && IS_IGMP_PROTO(pskb->data)) || IS_1905_MCAST(pskb->data))
			{
				dontBcast2otherSta = 1;
			}
#endif
#if defined(__LINUX_2_6__) || defined(__ECOS)
#ifdef CONFIG_RTL_IGMP_SNOOPING
			/* added by qinjunjie, to avoid igmpv1/v2 report suppress */
			if (rtl8192cd_isIgmpV1V2Report(pskb->data))
			{
				//printk("%s:%d,receive igmpv1/v2 report\n",__FUNCTION__,__LINE__);
				goto mcast_netif_rx;
			}
#endif
#ifdef CONFIG_RTL_MLD_SNOOPING
			if(rtl8192cd_isMldV1V2Report(pskb->data))
			{
				goto mcast_netif_rx;
			}
#endif
#endif

#if !defined(NOT_RTK_BSP) && !defined(CONFIG_OPENWRT_SDK) && !(defined(CONFIG_RTL8672) && defined(__LINUX_3_18__))
			if (priv->pmib->dot11OperationEntry.guest_access && priv->pmib->dot11OperationEntry.guest_access!=2)
				pskb->__unused = 0xe5;
#endif

#ifdef __KERNEL__
#ifndef CONFIG_RTL8196C_KLD
			br_port = GET_BR_PORT(priv->dev);

			// if we are STP aware, don't broadcast received BPDU
			if (!(br_port &&
				 br_port->br->stp_enabled &&
				 !memcmp(pskb->data, "\x01\x80\xc2\x00\x00\x00", 6)))
#endif
#endif
			{
				//if (!priv->pmib->dot11OperationEntry.block_relay)
				if (priv->pmib->dot11OperationEntry.block_relay==0 
				|| (priv->pmib->dot11OperationEntry.block_relay==3 && priv->pmib->hs2Entry.ICMPv4ECHO == 1))
				{
#if defined(_SINUX_) && defined(CONFIG_RTL865X_ETH_PRIV_SKB)
					extern struct sk_buff *priv_skb_copy(struct sk_buff *skb);
					pnewskb = priv_skb_copy(pskb);
#else
					pnewskb = skb_copy(pskb, GFP_ATOMIC);
#endif
					if (pnewskb) {

						if ( 0 
							#ifdef GBWC
							|| (GET_HCI_TYPE(priv) == RTL_HCI_PCIE && GBWC_forward_check(priv, pnewskb, pstat))
							#endif
							#ifdef SBWC
							|| SBWC_forward_check(priv, pnewskb, pstat)
							#endif
						) {
							// packet is queued, nothing to do
						}
						else
						{
#ifdef TX_SCATTER
							pnewskb->list_num = 0;
#endif
							if(dontBcast2otherSta){
								dev_kfree_skb_any(pnewskb);
							}else{
#ifdef PREVENT_BROADCAST_STORM
	                            if (da[0] == 0xff) {
		                            if (pstat->rx_pkts_bc > BROADCAST_STORM_THRESHOLD) {
			                            priv->ext_stats.rx_data_drops++;
				                        DEBUG_ERR("RX DROP: Broadcast storm happened!\n");
					                    dev_kfree_skb_any(pnewskb);
						            }
							        else {
								        if (rtl8192cd_start_xmit(pnewskb, priv->dev))
									        rtl_kfree_skb(priv, pnewskb, _SKB_TX_);
	                                }
		                        }
			                    else
#endif
								{
									if (rtl8192cd_start_xmit(pnewskb, priv->dev))
										rtl_kfree_skb(priv, pnewskb, _SKB_TX_);
								}
							}

						}
					}
				}
			}

#if (defined(__LINUX_2_6__) || defined(__ECOS)) && (defined(CONFIG_RTL_IGMP_SNOOPING) || defined(CONFIG_RTL_MLD_SNOOPING))
mcast_netif_rx:
#endif

			if (do_rc) {
				*(unsigned int *)&(pfrinfo->pskb->cb[4]) = 0;
				if (reorder_ctrl_check(priv, pstat, pfrinfo))
					rtl_netif_rx(priv, pfrinfo->pskb, pstat);
			}
			else
				rtl_netif_rx(priv, pskb, pstat);
		}
		else
		{

#if defined(TAROKO_0)
			res =1;
#else
			// unicast.. the status of sa has been checked in defrag_frame.
			// however, we should check if the da is in the WDS to see if we should
			res = skb_p80211_to_ether(pskb->dev, privacy, pfrinfo);
#endif

			if (res == FAIL) {
				priv->ext_stats.rx_data_drops++;
				DEBUG_ERR("RX DROP: skb_p80211_to_ether fail!\n");
				return FAIL;
			}

#ifdef PREVENT_ARP_SPOOFING
			if (check_arp_spoofing(priv, pskb)) {
				priv->ext_stats.rx_data_drops++;
				DEBUG_ERR("RX DROP: Detected ARP spoofing!\n");
				return FAIL;
			}
#endif

			dst_pstat = get_stainfo(priv, da);
#ifdef CONFIG_RTK_MESH
            {
                struct rtl8192cd_priv *orig_priv=priv;
#if defined(CONFIG_RTL_MESH_SINGLE_IFACE)
                priv = priv->mesh_priv_first;
#endif
                if(dst_pstat && isMeshPoint(dst_pstat) && priv->pmib->dot1180211sInfo.mesh_portal_enable == 0)
                    dst_pstat = NULL;

                priv = orig_priv;
            }
#endif

#ifdef A4_STA
			if (priv->pmib->miscEntry.a4_enable && (dst_pstat == NULL))
				dst_pstat = a4_sta_lookup(priv, da);
#endif

			if ((dst_pstat == NULL) || (!(dst_pstat->state & WIFI_ASOC_STATE))
#ifdef WDS
				|| (pfrinfo->to_fr_ds==3 && priv->pmib->dot11WdsInfo.wdsEnabled)
#endif
			)
			{
#if  defined(__KERNEL__)
				if (priv->pmib->dot11OperationEntry.guest_access) {
					br_port = GET_BR_PORT(priv->dev);
					
					if (
						(*(unsigned short *)(SKB_MAC_HEADER(pskb) + MACADDRLEN*2) != __constant_htons(0x888e)) &&
						(*(unsigned short *)(SKB_MAC_HEADER(pskb) + MACADDRLEN*2) != __constant_htons(0x86dd))
#ifndef CONFIG_RTL8196C_KLD
						&& br_port
#if defined(__LINUX_2_6__) || defined(__OSK__)
	#if defined(CONFIG_RTL_EAP_RELAY) || defined(CONFIG_RTK_INBAND_HOST_HACK)
						&& memcmp(da, inband_Hostmac, MACADDRLEN)
	#else
						&& memcmp(da, br_port->br->dev->dev_addr, MACADDRLEN)
	#endif
#else
						&& memcmp(da, priv->dev->br_port->br->dev.dev_addr, MACADDRLEN)
#endif
#endif
						) {
						if(!(priv->pmib->dot11OperationEntry.guest_access==2 &&
							!memcmp(da, priv->pmib->dot11OperationEntry.dfgw_mac, MACADDRLEN)))
						{
							priv->ext_stats.rx_data_drops++;
							DEBUG_ERR("RX DROP: guest access fail!\n");													
							return FAIL;
						}
					}
#if !defined(NOT_RTK_BSP) && !defined(CONFIG_OPENWRT_SDK) && !(defined(CONFIG_RTL8672) && defined(__LINUX_3_18__))
					pskb->__unused = 0xe5;
#endif
					//printk("guest packet, addr: %0x2:%02x:%02x:%02x:%02x:%02x\n",da[0],da[1],da[2],da[3],da[4],da[5]);
				}
#if !defined(NOT_RTK_BSP) && !defined(CONFIG_OPENWRT_SDK) && !(defined(CONFIG_RTL8672) && defined(__LINUX_3_18__))
				else
					pskb->__unused = 0;
#endif

#ifdef __LINUX_2_6__
				if(pskb->dev)
					pskb->protocol = eth_type_trans(pskb,pskb->dev);
				else
#endif
					pskb->protocol = eth_type_trans(pskb, priv->dev);

#if defined(HS2_SUPPORT)
                /* Hotspot 2.0 Release 1 */
#ifdef __ECOS
				if (pskb->data[12] == 0x89 && pskb->data[13] == 0x0d )
#else
				if(pskb->protocol == __constant_htons(ETHER_TDLS))
#endif
				{
					priv->ext_stats.rx_data_drops++;
					HS2_DEBUG_INFO("RX DROP: TDLS!\n");
					return FAIL;
				}
#endif

#ifdef EAPOLSTART_BY_QUEUE
				if (*(unsigned short *)(SKB_MAC_HEADER(pskb) + MACADDRLEN*2) == __constant_htons(0x888e))
				{
					unsigned char		szEAPOL[] = {0x01, 0x01, 0x00, 0x00};
					DOT11_EAPOL_START	Eapol_Start;

					if (!memcmp(pskb->data, szEAPOL, sizeof(szEAPOL)))
					{
						Eapol_Start.EventId = DOT11_EVENT_EAPOLSTART;
						Eapol_Start.IsMoreEvent = FALSE;
						memcpy(&Eapol_Start.MACAddr, SKB_MAC_HEADER(pskb) + MACADDRLEN, WLAN_ETHHDR_LEN);
						DOT11_EnQueue((unsigned long)priv, priv->pevent_queue, (unsigned char*)&Eapol_Start, sizeof(DOT11_EAPOL_START));
						event_indicate(priv, SKB_MAC_HEADER(pskb) + MACADDRLEN, 4);
						return FAIL;	// let dsr free this skb
					}
				}
#endif
#else
#ifdef CONFIG_RTL_819X_ECOS
				if (priv->pmib->dot11OperationEntry.guest_access)
				{
					if((*(unsigned short *)(SKB_MAC_HEADER(pskb) + MACADDRLEN*2) != __constant_htons(0x888e)) &&
						(*(unsigned short *)(SKB_MAC_HEADER(pskb) + MACADDRLEN*2) != __constant_htons(0x86dd)) &&
						memcmp(da, freebsd_Hostmac, MACADDRLEN)
						)
					{
						//wlan sta can't access LAN when guest access is enabled
						priv->ext_stats.rx_data_drops++;
						DEBUG_ERR("RX DROP: guest access fail!\n");
						return FAIL;
					}
#if !defined(NOT_RTK_BSP) && !defined(CONFIG_OPENWRT_SDK) && !(defined(CONFIG_RTL8672) && defined(__LINUX_3_18__))					
					else
						pskb->__unused = 0xe5;
#endif					
				}
#endif
#endif /* __ECOS */

#if 0 /*rtk_nl80211 rx eap packets (for debug)*/
				if ((*(unsigned short *)(SKB_MAC_HEADER(pskb) + MACADDRLEN*2) == __constant_htons(0x888e))
#ifdef WDS
					&& (pfrinfo->to_fr_ds != 3)
#endif
					)
				{
					printk("[%s][%d]RX EAP PACKETS len = %d!!\n\n", __FUNCTION__, __LINE__, pskb->len);
				}
#endif

#if (defined(EAP_BY_QUEUE) || defined(INCLUDE_WPA_PSK)) && (!defined(WIFI_HAPD) || defined(HAPD_DRV_PSK_WPS)) && (!defined(RTK_NL80211))
#ifdef WDS
				if (pfrinfo->to_fr_ds == 3 && priv->pmib->dot11WdsInfo.wdsEnabled) {
                    ;
                } else
#endif
				if (*(unsigned short *)(SKB_MAC_HEADER(pskb) + MACADDRLEN*2) == __constant_htons(0x888e))
				{
					if (IEEE8021X_FUN
#ifdef INCLUDE_WPA_PSK
							|| (priv->pmib->dot1180211AuthEntry.dot11EnablePSK &&
									((priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _TKIP_PRIVACY_) ||
									 (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _CCMP_PRIVACY_)))
#endif
#ifdef WIFI_SIMPLE_CONFIG
							|| priv->pmib->wscEntry.wsc_enable
#endif
					) {
						unsigned short		pkt_len;

#ifdef __ECOS
						pkt_len = pskb->len;
#else
						pkt_len = WLAN_ETHHDR_LEN + pskb->len;
#endif

#if defined(TAROKO_0)
//TODO, handle eap frame to upper layer
#else
						if (pkt_len <= sizeof(priv->Eap_packet->packet))
						{
						priv->Eap_packet->EventId = DOT11_EVENT_EAP_PACKET;
						priv->Eap_packet->IsMoreEvent = FALSE;
						memcpy(&(priv->Eap_packet->packet_len), &pkt_len, sizeof(unsigned short));
#ifdef __ECOS
						memcpy(&priv->Eap_packet->packet, pskb->data, pskb->len);
#else
						memcpy(&(priv->Eap_packet->packet[0]), SKB_MAC_HEADER(pskb), WLAN_ETHHDR_LEN);
						memcpy(&(priv->Eap_packet->packet[WLAN_ETHHDR_LEN]), pskb->data, pskb->len);
#endif
#ifdef EAP_BY_QUEUE

#ifdef INCLUDE_WPS

						wps_NonQueue_indicate_evt(priv ,
							(char *)priv->Eap_packet, sizeof(DOT11_EAP_PACKET));
#else
						DOT11_EnQueue((unsigned long)priv, priv->pevent_queue,
							(unsigned char*)priv->Eap_packet,sizeof(DOT11_EAP_PACKET));

						event_indicate(priv, NULL, -1);
#endif

#endif
#ifdef INCLUDE_WPA_PSK
#ifdef __ECOS
						psk_indicate_evt(priv, DOT11_EVENT_EAP_PACKET, (unsigned char*)&(priv->Eap_packet->packet[6]),
							(unsigned char*)priv->Eap_packet->packet, pskb->len);
#else
						psk_indicate_evt(priv, DOT11_EVENT_EAP_PACKET, (unsigned char*)&(priv->Eap_packet->packet[6]),
							(unsigned char*)priv->Eap_packet->packet, WLAN_ETHHDR_LEN+pskb->len);
#endif
#endif
						}
#endif

#if (!defined(WIFI_HAPD)) && (!defined(RTK_NL80211))
						return FAIL;	// let dsr free this skb
#endif
					}
				}
#endif

#ifndef __ECOS
				skb_push(pskb, WLAN_ETHHDR_LEN);	// push back due to be pulled by eth_type_trans()
#endif


#ifdef A4_STA
					if((GET_BR_PORT(priv->dev) && priv->pmib->miscEntry.a4_enable == 2 && fdb_find_for_driver(GET_BR_PORT(priv->dev)->br, da) == NULL) || (priv->pmib->miscEntry.a4_enable == 1 && IS_MCAST(da))) {
						if (!IS_1905_MCAST(da)) {
                    	pnewskb = skb_copy(pskb, GFP_ATOMIC);
						if (pnewskb) {

							if ( 0
								#ifdef GBWC
								|| (GET_HCI_TYPE(priv) == RTL_HCI_PCIE && GBWC_forward_check(priv, pnewskb, pstat))
								#endif
								#ifdef SBWC
								|| SBWC_forward_check(priv, pnewskb, pstat)
								#endif
							) {
							    // packet is queued, nothing to do
							}
							else
                        {
#ifdef TX_SCATTER
                            pnewskb->list_num = 0;
#endif                          
                            if (rtl8192cd_start_xmit(pnewskb, priv->dev))
                                rtl_kfree_skb(priv, pnewskb, _SKB_TX_);
                        }
                    }
                }
                	}
#endif //A4_STA

				if (do_rc) {
					*(unsigned int *)&(pfrinfo->pskb->cb[4]) = 0;
					if (reorder_ctrl_check(priv, pstat, pfrinfo))
						rtl_netif_rx(priv, pfrinfo->pskb, pstat);
				}
				else
					rtl_netif_rx(priv, pskb, pstat);
			}
			else
			{
				if (priv->pmib->dot11OperationEntry.block_relay == 1) {
					priv->ext_stats.rx_data_drops++;
					DEBUG_ERR("RX DROP: Relay unicast packet is blocked!\n");
#ifdef RX_SHORTCUT
					for (i=0; i<RX_SC_ENTRY_NUM; i++) { // shortcut data saved, clear it
						dst_pstat->rx_sc_ent[i].rx_payload_offset = 0;
					}
#endif
					return FAIL;
				}
				else if (priv->pmib->dot11OperationEntry.block_relay == 2) {
					DEBUG_INFO("Relay unicast packet is blocked! Indicate to bridge.\n");
					rtl_netif_rx(priv, pskb, pstat);
				}
#ifdef HS2_SUPPORT
				/* Hotspot 2.0 Release 1 */
				else if (priv->pmib->dot11OperationEntry.block_relay == 3 
                    && (IS_ICMPV4_PROTO(pfrinfo->pskb->data) && IS_ICMPV4_ECHO_TYPE(pfrinfo->pskb->data))
                    && (priv->pmib->hs2Entry.ICMPv4ECHO != 1) ){
                    
					HS2DEBUG("block_relay=3\n");
		
					if(priv->pmib->hs2Entry.ICMPv4ECHO == 2) {
						//HS2_CHECK
						//memcpy(&pfrinfo->pskb->data[0], priv->pmib->hs2Entry.redir_mac, 6);
						HS2DEBUG("redirect ICMPv4 Packet to portal\n");
						rtl_netif_rx(priv, pskb, pstat);
						return SUCCESS;
					}
					else if(priv->pmib->hs2Entry.ICMPv4ECHO == 0) {
						HS2DEBUG("Drop ICMPv4 Packet\n");
						rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);       
						return SUCCESS;
					}
		            #if 0   /*2014 1127 sgs log test*/ 
					else if(priv->pmib->hs2Entry.ICMPv4ECHO == 1) {
						HS2DEBUG("Allow ICMPv4 Echo Requests\n");
						*(unsigned int *)&(pfrinfo->pskb->cb[4]) = (unsigned int)dst_pstat;   // backup pstat pointer

                        #ifdef ENABLE_RTL_SKB_STATS
						rtl_atomic_dec(&priv->rtl_rx_skb_cnt);
                        #endif
                        #if defined(CONFIG_RTK_MESH) && defined(RX_RL_SHORTCUT)
						if (rtl8192cd_start_xmit(pfrinfo->pskb, isMeshPoint(dst_pstat)? priv->mesh_dev: priv->dev))
                        #else
						if (rtl8192cd_start_xmit(pfrinfo->pskb, priv->dev))
                        #endif
							rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_TX_);
					}
		            #endif
					return FAIL;
				}
#endif
                else {
                    do_process = 0;
                    #ifdef ENABLE_RTL_SKB_STATS
                    rtl_atomic_dec(&priv->rtl_rx_skb_cnt);
                    #endif
					if ( 0 
						#ifdef GBWC
						|| (GET_HCI_TYPE(priv) == RTL_HCI_PCIE && GBWC_forward_check(priv, pfrinfo->pskb, pstat))
						#endif
						#ifdef SBWC
						|| SBWC_forward_check(priv, pfrinfo->pskb, pstat)
						#endif
						) 
					{
						// packet is queued, nothing to do
					}
					else if (do_rc) {
                        #ifdef CONFIG_RTK_MESH
                        if(isMeshPoint(dst_pstat)) {
                            pfrinfo->pskb->cb[4] = 1;
                            pfrinfo->pskb->cb[3] = XMIT_11S;
                        }
                        else                  
                        #endif
                            *(unsigned int *)&(pfrinfo->pskb->cb[4]) = (unsigned int)dst_pstat;	// backup pstat pointer

#ifdef ENABLE_RTL_SKB_STATS
                        rtl_atomic_inc(&priv->rtl_rx_skb_cnt);
#endif
                        if (reorder_ctrl_check(priv, pstat, pfrinfo)) {
#ifdef ENABLE_RTL_SKB_STATS
                            rtl_atomic_dec(&priv->rtl_rx_skb_cnt);
#endif
                            do_process = 1;
                        }
                    } else {
                        do_process = 1;
                    }

                    if(do_process) {
                        #ifdef TX_SCATTER
                        pskb->list_num = 0;
                        #endif
                        #ifdef CONFIG_RTK_MESH
                        if(isMeshPoint(dst_pstat)) {                            
                            if(mesh_start_xmit(pfrinfo->pskb, priv->mesh_dev))
                                rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_TX_);
                        }
                        else
                        #endif  
                        {
                            if (rtl8192cd_start_xmit(pskb, priv->dev))
                                rtl_kfree_skb(priv, pskb, _SKB_TX_);
                        }

                    }
                }
			}
		}
	}
#ifdef CLIENT_MODE
	else if (OPMODE & (WIFI_STATION_STATE | WIFI_ADHOC_STATE)) {
		// I am station, and just report every frame I received to protocol statck
		if (OPMODE & WIFI_STATION_STATE)
			pstat = get_stainfo(priv, BSSID);
		else	// Ad-hoc
			pstat = get_stainfo(priv, pfrinfo->sa);
		
		if (NULL == pstat) {
			priv->ext_stats.rx_data_drops++;
			DEBUG_ERR("RX DROP: pstat is NULL\n");
			return FAIL;
		}
    
#ifdef A4_STA
        if(priv->pmib->miscEntry.a4_enable && pfrinfo->to_fr_ds == 3) {
            dest = GetAddr1Ptr(pframe);
        }
        else
#endif
            dest = pfrinfo->da;

		if (IS_MCAST(dest)) {
			// iv, icv and mic are not be used below. Don't care them!
			privacy = get_mcast_encrypt_algthm(priv);
		} else {
			privacy = get_sta_encrypt_algthm(priv, pstat);
		}

		rx_sum_up(NULL, pstat, pfrinfo);

		priv->pshare->NumRxBytesUnicast += pfrinfo->pktlen;
		update_sta_rssi(priv, pstat, pfrinfo);
		priv->rxDataNumInPeriod++;
      
		if (IS_MCAST(dest)) {
			priv->rxMlcstDataNumInPeriod++;
#ifdef SUPPORT_SNMP_MIB
			SNMP_MIB_INC(dot11MulticastReceivedFrameCount, 1);
#endif
		} else if ((OPMODE & WIFI_STATION_STATE) && (priv->ps_state)) {
			if ((GetFrameSubType(get_pframe(pfrinfo)) == WIFI_DATA)
#ifdef WIFI_WMM
				|| (QOS_ENABLE && pstat->QosEnabled && (GetFrameSubType(get_pframe(pfrinfo)) == WIFI_QOS_DATA))
#endif
			) {
				if (GetMData(pframe)) {
#if defined(WIFI_WMM) && defined(WMM_APSD)
					if (QOS_ENABLE && APSD_ENABLE && priv->uapsd_assoc) {
						if (!((priv->pmib->dot11QosEntry.UAPSD_AC_BE && ((pfrinfo->tid == 0) || (pfrinfo->tid == 3))) ||
							(priv->pmib->dot11QosEntry.UAPSD_AC_BK && ((pfrinfo->tid == 1) || (pfrinfo->tid == 2))) ||
							(priv->pmib->dot11QosEntry.UAPSD_AC_VI && ((pfrinfo->tid == 4) || (pfrinfo->tid == 5))) ||
							(priv->pmib->dot11QosEntry.UAPSD_AC_VO && ((pfrinfo->tid == 6) || (pfrinfo->tid == 7)))))
							issue_PsPoll(priv);
					} else
#endif
					{
						issue_PsPoll(priv);
					}
				}
			}
		}

		#if defined(CONFIG_RTL_WAPI_SUPPORT)
		if (privacy==_WAPI_SMS4_) {
#if defined(CONFIG_PCI_HCI)
			if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
				if (wapiHandleRecvPacket(pfrinfo, pstat)==SUCCESS)
				{
					return SUCCESS;
				}
			}
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
			if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
				if (pstat->wapiInfo && (wapiDisable != pstat->wapiInfo->wapiType))
				{
					int waiOffset;
					if ((SecIsWAIPacket(pskb, &waiOffset) == SUCCESS) ||
							(pstat->wapiInfo->wapiState != ST_WAPI_AE_MSKA_ESTABLISH))
					{
						notify_recv_wai_frame(priv, pfrinfo);
						return SUCCESS;
					}
				}
			}
#endif
		}
		#endif
		// Process A-MSDU
		if (is_qos_data(pframe)) {
			if (pfrinfo->amsdu)	// A-MSDU present
			{
#ifndef SUPPORT_RX_AMSDU_AMPDU
				process_amsdu(priv, pstat, pfrinfo);
#else
				//do rc check when ampdu_amsdu
				if (do_rc) {
					*(unsigned int *)&(pfrinfo->pskb->cb[4]) = 0;
					(pfrinfo->pskb->cb[3]) = 1;
					if (reorder_ctrl_check(priv, pstat, pfrinfo))
				process_amsdu(priv, pstat, pfrinfo);
				}
				else
					process_amsdu(priv, pstat, pfrinfo);
#endif				
				return SUCCESS;
			}
#ifdef RX_BUFFER_GATHER
#ifdef RX_BUFFER_GATHER_REORDER
#else
			else if (!list_empty(&priv->pshare->gather_list))
				flush_rx_list(priv);
#endif			
#endif
		}

		res = skb_p80211_to_ether(priv->dev, privacy, pfrinfo);
		if (res == FAIL) {
			priv->ext_stats.rx_data_drops++;
			DEBUG_ERR("RX DROP: skb_p80211_to_ether fail!\n");
			return FAIL;
		}

#ifdef RTK_BR_EXT
#ifdef A4_STA
        if(pstat && !(pstat->state & WIFI_A4_STA))
#endif
        {
    		if(nat25_handle_frame(priv, pskb) == -1) {
    			priv->ext_stats.rx_data_drops++;
    			DEBUG_ERR("RX DROP: nat25_handle_frame fail!\n");
    			return FAIL;
    		}
        }
#endif

#if 0 /*rtk_nl80211 (for debug)*/
		if (*(unsigned short *)(SKB_MAC_HEADER(pskb) + MACADDRLEN*2) == __constant_htons(0x888e))
		{
			printk("[%s][%d]rx eap packets len = %d!!\n\n", __FUNCTION__, __LINE__, pskb->len);
		}
#endif

#if defined(__KERNEL__) || defined(__OSK__)
		pskb->protocol = eth_type_trans(pskb, priv->dev);
#endif

#ifdef WIFI_WPAS_CLI
		if (!(OPMODE & WIFI_STATION_STATE))
#endif
		{
#if (defined(EAP_BY_QUEUE) || defined(INCLUDE_WPA_PSK)) && (!defined(WIFI_HAPD) || defined(HAPD_DRV_PSK_WPS)) &&(!defined(RTK_NL80211))
		if (*(unsigned short *)(SKB_MAC_HEADER(pskb) + MACADDRLEN*2) == __constant_htons(0x888e))
		{
			unsigned short		pkt_len;

#ifdef __ECOS
			pkt_len = pskb->len;
#else
			pkt_len = WLAN_ETHHDR_LEN + pskb->len;
#endif

#if defined(TAROKO_0)
//TODO, handle eap frame to upper layer
#else

			if (pkt_len <= sizeof(priv->Eap_packet->packet))
			{
			priv->Eap_packet->EventId = DOT11_EVENT_EAP_PACKET;
			priv->Eap_packet->IsMoreEvent = FALSE;
			memcpy(&(priv->Eap_packet->packet_len), &pkt_len, sizeof(unsigned short));
#ifdef __ECOS
			memcpy(&priv->Eap_packet->packet, pskb->data, pskb->len);
#else
			memcpy(&(priv->Eap_packet->packet[0]), SKB_MAC_HEADER(pskb), WLAN_ETHHDR_LEN);
			memcpy(&(priv->Eap_packet->packet[WLAN_ETHHDR_LEN]), pskb->data, pskb->len);
#endif
#ifdef EAP_BY_QUEUE

#ifdef INCLUDE_WPS

			wps_NonQueue_indicate_evt(priv ,
				(char *)priv->Eap_packet, sizeof(DOT11_EAP_PACKET));
#else
			DOT11_EnQueue((unsigned long)priv, priv->pevent_queue,
				(unsigned char*)priv->Eap_packet, sizeof(DOT11_EAP_PACKET));
			event_indicate(priv, NULL, -1);
#endif
#endif
#ifdef INCLUDE_WPA_PSK
#ifdef __ECOS
			psk_indicate_evt(priv, DOT11_EVENT_EAP_PACKET, (unsigned char*)&(priv->Eap_packet->packet[6]),
				(unsigned char*)priv->Eap_packet->packet, pskb->len);
#else
			psk_indicate_evt(priv, DOT11_EVENT_EAP_PACKET, (unsigned char*)&(priv->Eap_packet->packet[6]),
				(unsigned char*)priv->Eap_packet->packet, WLAN_ETHHDR_LEN+pskb->len);
#endif
#endif
			}
#endif

#if (!defined(WIFI_HAPD)) && (!defined(RTK_NL80211))
			return FAIL;	// let dsr free this skb
#endif
		}
#endif
		} // WIFI_WPAS_CLI

#if defined(__KERNEL__) || defined(__OSK__)
		skb_push(pskb, WLAN_ETHHDR_LEN);	// push back due to be pulled by eth_type_trans()
#endif
		if (do_rc) {
			*(unsigned int *)&(pfrinfo->pskb->cb[4]) = 0;
			if (reorder_ctrl_check(priv, pstat, pfrinfo))
				rtl_netif_rx(priv, pskb, pstat);
		}
		else
			rtl_netif_rx(priv, pskb, pstat);
	}
#endif // CLIENT_MODE
	else
	{
		priv->ext_stats.rx_data_drops++;
		DEBUG_ERR("RX DROP: Non supported mode in process_datafrme\n");
		return FAIL;
	}

	return SUCCESS;
}

#if defined(DRVMAC_LB) && defined(WIFI_WMM)
static void process_lb_qos_null(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
//	unsigned char *pframe;
//	unsigned int aid;
//	struct stat_info *pstat = NULL;

//	pframe  = get_pframe(pfrinfo);
//	aid = GetAid(pframe);
//	pstat = get_aidinfo(priv, aid);

//	if ((!(OPMODE & WIFI_AP_STATE)) || (pstat == NULL) || (memcmp(pstat->cmn_info.mac_addr, get_sa(pframe), MACADDRLEN))) {
//		rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
//		return;
//	}
//	process_APSD_dz_queue(priv, pstat, pfrinfo->tid);

#if 0
	if (pfrinfo->pskb && pfrinfo->pskb->data) {
		unsigned int *p_skb_int = (unsigned int *)pfrinfo->pskb->data;
		printk("LB RX FRAME =====>>\n");
		printk("0x%08x 0x%08x 0x%08x 0x%08x\n", *p_skb_int, *(p_skb_int+1), *(p_skb_int+2), *(p_skb_int+3));
		printk("0x%08x 0x%08x 0x%08x 0x%08x\n", *(p_skb_int+4), *(p_skb_int+5), *(p_skb_int+6), *(p_skb_int+7));
		printk("LB RX FRAME <<=====\n");
	}
#endif
	rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
}


static void process_lb_qos(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
//	unsigned char *pframe;
//	unsigned int aid;
//	struct stat_info *pstat = NULL;

//	pframe  = get_pframe(pfrinfo);
//	aid = GetAid(pframe);
//	pstat = get_aidinfo(priv, aid);

//	if ((!(OPMODE & WIFI_AP_STATE)) || (pstat == NULL) || (memcmp(pstat->cmn_info.mac_addr, get_sa(pframe), MACADDRLEN))) {
//		rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
//		return;
//	}
//	process_APSD_dz_queue(priv, pstat, pfrinfo->tid);

#if 1
	if (pfrinfo->pskb && pfrinfo->pskb->data) {
		unsigned char *p_skb_int = (unsigned char *)pfrinfo->pskb->data;
		unsigned int payload_length = 0, i = 0, mismatch = 0;
		unsigned char matching = 0;

		if (pfrinfo->pktlen && pfrinfo->hdr_len && pfrinfo->pktlen > pfrinfo->hdr_len) {
			payload_length = pfrinfo->pktlen - pfrinfo->hdr_len;
			if (payload_length >= 2048)
				printk("LB Qos RX, payload max hit!\n");
//			if (payload_length > 32)
//				payload_length = 32;
		}
		else {
			if (!pfrinfo->pktlen)
				printk("LB Qos RX, zero pktlen!!!\n");
			else if (!pfrinfo->hdr_len)
				printk("LB Qos RX, zero hdr_len!!!\n");
			else if (pfrinfo->pktlen < pfrinfo->hdr_len)
				printk("LB Qos RX, pktlen < hdr_len!!!\n");
			else
				printk("LB Qos RX, empty payload.\n");
			goto out;
		}

		p_skb_int += pfrinfo->hdr_len;

//		printk("LB RX >> ");
//		for (i = 0; i < payload_length; i++) {
//			if (i>0 && !(i%4))
//				printk(" ");
//			if (!(i%4))
//				printk("0x");
//			printk("%02x", *(p_skb_int+i));
//		}
//		printk(" <<\n");

		for (i = 0; i < payload_length; i++) {
			if (priv->pmib->miscEntry.lb_mlmp == 1) {
				matching = 0;
				if (memcmp((p_skb_int+i), &matching, 1)) {
					mismatch++;
					break;
				}
			}
			else if (priv->pmib->miscEntry.lb_mlmp == 2) {
				matching = 0xff;
				if (memcmp((p_skb_int+i), &matching, 1)) {
					mismatch++;
					break;
				}
			}
			else if ((priv->pmib->miscEntry.lb_mlmp == 3) || (priv->pmib->miscEntry.lb_mlmp == 4)) {
				matching = i%0x100;
				if (memcmp((p_skb_int+i), &matching, 1)) {
					mismatch++;
					break;
				}
			}
			else {
				printk("LB Qos RX, wrong mlmp setting!\n");
				goto out;
			}
		}

		if (mismatch) {
			printk("LB Qos RX, rx pattern mismatch!!\n");
			priv->pmib->miscEntry.drvmac_lb = 0;	// stop the test
		}
	}
#endif

out:
	rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
}
#endif
#ifdef DRVMAC_LB
void lb_convert(struct rtl8192cd_priv *priv, unsigned char *pframe)
{
	unsigned char *addr1, *addr2, *addr3;

	if (get_tofr_ds(pframe) == 0x01)
	{
		SetToDs(pframe);
		ClearFrDs(pframe);
		addr1 = GetAddr1Ptr(pframe);
		addr2 = GetAddr2Ptr(pframe);
		addr3 = GetAddr3Ptr(pframe);
		if (addr1[0] & 0x01) {
			memcpy(addr3, addr1, 6);
			memcpy(addr1, addr2, 6);
			memcpy(addr2, priv->pmib->miscEntry.lb_da, 6);
		}
		else {
			memcpy(addr1, addr2, 6);
			memcpy(addr2, priv->pmib->miscEntry.lb_da, 6);
		}
	}
}
#endif


/*
	Actually process RX data frame:

		Process data frame stored in "inputPfrinfo" or gotten from "list",
		only one of them is used to get Frame information.

			Note that:
			1. If frame information is gotten from "list", "inputPfrinfo" MUST be NULL.
			2. If frame information is gotten from "inputPfrinfo", "list" MUST be NULL
*/
#ifdef __OSK__
__IRAM_WIFI_PRI3
#else
__MIPS16
__IRAM_IN_865X
#endif
 void rtl8192cd_rx_dataframe(struct rtl8192cd_priv *priv, struct list_head *list,
				struct rx_frinfo *inputPfrinfo)
{
	struct rx_frinfo *pfrinfo = NULL;
	unsigned char *pframe;
#ifdef CONFIG_RTK_MESH
	unsigned char is_mesh_amsdu = 0;
#endif
	/* ============== Do releted process for Packet RX ============== */
	// for SW LED
	priv->pshare->LED_rx_cnt++;

	// for Rx dynamic tasklet
	priv->pshare->rxInt_data_delta++;

	/* Get RX frame info */
	if (list) {
		/* Indicate the frame information can be gotten from "list" */
		pfrinfo = list_entry(list, struct rx_frinfo, rx_list);
	}
	else {
		/* Indicate the frame information is stored in "inputPfrinfo " */
		pfrinfo = inputPfrinfo;
	}

	if (pfrinfo == NULL) {
		printk("pfrinfo == NULL\n");
		goto out;
	}

	pframe = get_pframe(pfrinfo);
#ifdef CONFIG_RTK_MESH
	if (is_qos_data(pframe)) {
		if((*GetQosControl(pframe) & BIT(7)) && pfrinfo->is_11s) // A-MSDU present
			is_mesh_amsdu = 1;
	}
#endif
#ifdef DROP_RXPKT
	static volatile unsigned long prev_jiffies  = 0;
	static volatile unsigned long prev_rx_bytes = 0;
	static volatile unsigned long prev_tx_bytes = 0;
	unsigned long j, diff;
	unsigned long rxb = 0; /* Current rx bytes */
	unsigned long txb = 0; /* Current tx bytes */
	unsigned long totalb = 0; /* Total tx/rx bytes */
	struct net_device_stats *ns;
	if(priv->pshare->use_hal)  { //This is 8192ER 2.4GHz traffic
		ns = &(priv->net_stats);
		if(ns != NULL) {
			G24_rx_bytes = ns->rx_bytes;
			G24_tx_bytes = ns->tx_bytes;
		} else {
			printk("Warning: Invalid net_stats!!!\n");
		}
	} else { //This is 8812 5GHz traffic
		j = jiffies;
		diff = j - prev_jiffies; 
		if(diff) {
			rxb = G24_rx_bytes;
			txb = G24_tx_bytes;
			totalb =  (rxb - prev_rx_bytes);
			totalb += (txb - prev_tx_bytes);
		
			/* 
			diff_sec = diff/HZ; //time diff --> convert unit from jiffies to seconds
			totalb = (totalb<<3)/diff_sec; //2.4GHz rate --> bits per second
                               = (totalb<<3)/(diff/HZ)
                               = (totalb<<3)*(HZ/diff)
			*/
			totalb = ((totalb<<3) * HZ) /diff; /* 2.4GHz rate --> bits per second */
//#define G24_THRESHOLD (50<<20) /* Unit=bits per second */
#define G24_THRESHOLD (20<<20) /* Unit=bits per second */
			if(totalb >= G24_THRESHOLD) {
				unsigned long tmp1, tmp2;
				tmp1 = priv->pmib->miscEntry.drop_rxpkt_rate;
				tmp2 = priv->pmib->miscEntry.G5G24_drop_rxpkt_rate; 
				if(tmp1 != tmp2) {
					priv->pmib->miscEntry.drop_rxpkt_rate = tmp2;
					printk("priv->pmib->miscEntry.drop_rxpkt_rate=%lu\n", tmp2);
				}
#if 0
				//printk("24G TX/RX(%u) is running over %u Mbps\n", totalb, (G24_THRESHOLD>>20));
				if (!(priv->pmib->miscEntry.drop_rxpkt_en)) {
					//Enable 5G rx drop
					priv->pmib->miscEntry.drop_rxpkt_en = 1;
					//priv->pmib->miscEntry.drop_rxpkt_rate = 1000; 
	
					printk("24G TX/RX(%u) is running over %u Mbps\n", totalb, (G24_THRESHOLD>>20));
					printk("Turned on 5G rx drop\n");
				}
#endif
				//Activate a timer to disable rx drop
extern struct timer_list G5_rx_drop_timer;
				mod_timer(&G5_rx_drop_timer, jiffies+HZ);	
			}
			prev_jiffies = j;
			prev_rx_bytes = rxb;
			prev_tx_bytes = txb;
		}
	}

	if (OPMODE & WIFI_AP_STATE) {
		if (priv->pmib->miscEntry.drop_rxpkt_en) {
			priv->rxpkt_count++;
			if (pfrinfo->pskb != NULL &&
				pfrinfo->pktlen > priv->pmib->miscEntry.drop_rxpkt_len &&
				priv->rxpkt_count >= priv->pmib->miscEntry.drop_rxpkt_rate) {
				priv->rxpkt_count = 0;
#ifdef RX_BUFFER_GATHER_REORDER
				if(pfrinfo->gather_flag == GATHER_FIRST)
					flush_rx_list_inq(priv,pfrinfo);
#endif					
				rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
				//printk("\nDROP_RXPKT\n");
				goto out;
			}
		}
	}
#endif

#ifdef WIFI_WMM
	if (is_qos_data(pframe)) {
		if ((OPMODE & WIFI_AP_STATE) && (QOS_ENABLE)) {
			if ((pfrinfo->tid == 7) || (pfrinfo->tid == 6)) {
				priv->pshare->phw->VO_pkt_count++;
			} else if ((pfrinfo->tid == 5) || (pfrinfo->tid == 4)) {
				priv->pshare->phw->VI_pkt_count++;
				if (priv->pshare->rf_ft_var.wifi_beq_iot)
					priv->pshare->phw->VI_rx_pkt_count++;
			} else if ((pfrinfo->tid == 2) || (pfrinfo->tid == 1)) {
				priv->pshare->phw->BK_pkt_count++;
			}
		}
	}
#endif

	/* ============== Start to process RX dataframe ============== */
#ifndef __ECOS
#if (defined(CONFIG_RTK_VOIP_QOS)|| defined(CONFIG_RTK_VLAN_WAN_TAG_SUPPORT) ||defined(CONFIG_RTL_CUSTOM_PASSTHRU)|| defined(CONFIG_RTL_MULTI_LAN_DEV) || defined(CONFIG_RTL_EN_UNKNOWNUC_ONLY_TO_CPU) || defined(CONFIG_RTL_NIC_QUEUE)) && (!(defined(CONFIG_RTL8672) && defined(__LINUX_3_18__))) && (!defined(NOT_RTK_BSP))
#ifdef MBSSID
	#ifndef CONFIG_OPENWRT_SDK
        if(IS_VAP_INTERFACE(priv))
                pfrinfo->pskb->srcPhyPort += (priv->vap_id+1);
    #endif
#endif
#endif
#endif

#if defined(CROSSBAND_REPEATER)
		if(IS_VXD_INTERFACE(priv)) //drop possible looping packets
		{
				if(!memcmp(pfrinfo->sa,GET_ROOT(priv->crossband.crossband_priv_sc)->pmib->dot11OperationEntry.hwaddr,MACADDRLEN)
					|| !memcmp(pfrinfo->sa,GET_ROOT(priv)->pmib->dot11OperationEntry.hwaddr,MACADDRLEN))
				{
					rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
					goto out;
				}
		}			
#endif

#if defined(DRVMAC_LB) && defined(WIFI_WMM)
	if(priv->pmib->miscEntry.drvmac_lb /*&& priv->pmib->miscEntry.lb_tps*/) {
		if ((QOS_ENABLE) && (GetFrameSubType(get_pframe(pfrinfo)) == (BIT(7)|WIFI_DATA_NULL)))
			process_lb_qos_null(priv, pfrinfo);
		else if ((QOS_ENABLE) && (GetFrameSubType(get_pframe(pfrinfo)) == (WIFI_QOS_DATA)))
			process_lb_qos(priv, pfrinfo);
		else {
#if 0
			if (!QOS_ENABLE)
				printk("wifi qos not enabled, ");
			printk("cannot match loopback pkt pattern!!!\n");
			if (pfrinfo->pskb && pfrinfo->pskb->data) {
				unsigned int *p_skb_int = (unsigned int *)pfrinfo->pskb->data;
				printk("ERROR PKT===>> 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x <<===\n",
					*p_skb_int, *(p_skb_int+1), *(p_skb_int+2), *(p_skb_int+3), *(p_skb_int+4), *(p_skb_int+5),
					*(p_skb_int+6), *(p_skb_int+7));
			}
#endif
			rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
		}
		goto out;
	}
#endif

	if (OPMODE & WIFI_AP_STATE) {
		// Intel 6205 IOT issue
		if (GetFrameSubType(pframe) == (BIT(7) | WIFI_DATA_NULL)) {
#if defined(WIFI_WMM) && defined(WMM_APSD)
			if (QOS_ENABLE) {
				rtl8192cd_rx_handle_Spec_Null_Data(priv, pfrinfo);
				process_qos_null(priv, pfrinfo);
			}
#endif
			rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
			goto out;
		}

		// AR5007 IOT ISSUE
		if (GetFrameSubType(pframe) == WIFI_DATA_NULL) {
			rtl8192cd_rx_handle_Spec_Null_Data(priv, pfrinfo);
			rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
			goto out;
		}
	}

#ifdef __OSK__
	to_cnt++;
#endif

	pfrinfo->pskb->cb[3] = 0; /* clear amsdu indicate */

#ifdef RX_SHORTCUT
	if (!priv->pmib->dot11OperationEntry.disable_rxsc &&
#ifdef CONFIG_RTK_MESH
		!is_mesh_amsdu &&
#endif
		!IS_MCAST(pfrinfo->da)
#if defined(WIFI_WMM) && defined(WMM_APSD)
		&& (!(
#ifdef CLIENT_MODE
			(OPMODE & WIFI_AP_STATE) &&
#endif
			(APSD_ENABLE) && (GetFrameSubType(pframe) == (WIFI_QOS_DATA)) && (GetPwrMgt(pframe))))
#endif
		) {
		if (rx_shortcut(priv, pfrinfo) >= 0) {
#if defined(SHORTCUT_STATISTIC)  /*defined(__ECOS) && defined(_DEBUG_RTL8192CD_)*/
			priv->ext_stats.rx_cnt_sc++;
#endif
			goto out;
		}
	}
#endif

#if 0/*rtk_nl80211 (for debug)*/
{
	struct sk_buff *pskb = get_pskb(pfrinfo);
	unsigned int encrypt;
	unsigned char *pframe;
	unsigned short	proto;

	unsigned int	hdr_len;
	unsigned char	*pbuf;
	
	// rx a encrypt packet but encryption is not enabled in local mib, discard it
	hdr_len = pfrinfo->hdr_len;
	pframe = get_pframe(pfrinfo);
	pbuf = pframe + hdr_len + sizeof(struct wlan_llc_t) + 3;
	proto = *(unsigned short *)pbuf;

	if (proto == __constant_htons(0x888e))
		printk("[rx eap] from 0x%02x 0x%02x 0x%02x\n", pfrinfo->sa[3], pfrinfo->sa[4], pfrinfo->sa[5]);
}
#endif

#if defined(SHORTCUT_STATISTIC)  /*defined(__ECOS) && defined(_DEBUG_RTL8192CD_)*/
	priv->ext_stats.rx_cnt_nosc++; 
#endif
	pfrinfo = defrag_frame(priv, pfrinfo);

	if (pfrinfo == NULL)
		goto out;

	if (process_datafrme(priv, pfrinfo) == FAIL) {
		rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
	}

out:
	return;
}



/*
	Check the "to_fr_ds" field:

						FromDS = 0
						ToDS != 0
*/
static inline int rtl8192cd_rx_dispatch_toDs(struct rtl8192cd_priv **priv_p, 	struct rx_frinfo *pfrinfo
#ifdef MBSSID
				,int vap_idx
#endif
				)
{
	int reuse = 1;
	struct rtl8192cd_priv *priv = *priv_p;
	unsigned int opmode = OPMODE;
	unsigned char *pframe = get_pframe(pfrinfo);
	unsigned int frtype = GetFrameType(pframe);
	unsigned char *bssid = GetAddr1Ptr(pframe);


#if defined(SMP_SYNC)
	unsigned long x;
#endif
#ifdef CONFIG_RTK_MESH
    if (GET_MIB(priv)->dot1180211sInfo.mesh_enable == 1) {    /* For MBSSID enable and STA connect to Virtual-AP can't use problem */
		struct stat_info *pstat = get_stainfo(priv, pfrinfo->sa);

		if (((pstat == NULL) || isPossibleNeighbor(pstat))
#ifdef MBSSID
			&&(vap_idx < 0)
#endif
		) {
			goto out;
		}
    }
#endif	// CONFIG_RTK_MESH


	if (frtype == WIFI_MGT_TYPE)
		goto out;
#ifdef BSS_MONITOR_SUPPORT
//	collect_monitor_info(priv, pfrinfo);
#endif
#ifdef MBSSID
	if (GET_ROOT(priv)->pmib->miscEntry.vap_enable && (vap_idx >= 0))
	{
		priv = priv->pvap_priv[vap_idx];
		opmode = OPMODE;
	}
#endif

	if (opmode & WIFI_AP_STATE)
	{
#if 0
#ifdef MBSSID
		if (GET_ROOT(priv)->pmib->miscEntry.vap_enable && (vap_idx >= 0))
		{
			priv = priv->pvap_priv[vap_idx];
#ifdef RTL8190_DIRECT_RX
			rtl8192cd_rx_dataframe(priv, NULL, pfrinfo);
#else
#if defined(__OSK__) && defined(RX_BUFFER_GATHER)
			if (pfrinfo->gather_flag & GATHER_FIRST){
    			rtl8192cd_rx_dataframe(priv, NULL, pfrinfo);
    		}else
#endif
			{
			SMP_LOCK_RX_DATA(x);
			list_add_tail(&(pfrinfo->rx_list), &(priv->rx_datalist));
			SMP_UNLOCK_RX_DATA(x);
			rtl_atomic_inc(&priv->pshare->rx_enqueue_cnt);
			}
#endif
			reuse = 0;
		}
		else
#endif
#endif
		// For AP mode, the data frame should have bssid==BSSID. (sa state checked laterly)
		if (IS_BSSID(priv, bssid))
		{
#ifndef DRVMAC_LB
			// check power save state
			struct stat_info *pstat = get_stainfo(priv, GetAddr2Ptr(pframe));
			if (pstat && (pstat->state & WIFI_ASOC_STATE) &&
				(GetPwrMgt(pframe) != ((pstat->state & WIFI_SLEEP_STATE ? 1 : 0)))) {
				pwr_state(priv, pfrinfo);
			}
#endif
			// we only receive mgt frame when we are in SITE_MONITOR or link_loss
			// please consider the case: re-auth and re-assoc
#ifdef RTL8190_DIRECT_RX
			rtl8192cd_rx_dataframe(priv, NULL, pfrinfo);
#else
#if defined(__OSK__) && defined(RX_BUFFER_GATHER)
			if (pfrinfo->gather_flag & GATHER_FIRST){
    			rtl8192cd_rx_dataframe(priv, NULL, pfrinfo);
    		}else
#endif
			{
			SMP_LOCK_RX_DATA(x);
			list_add_tail(&(pfrinfo->rx_list), &(priv->rx_datalist));
			SMP_UNLOCK_RX_DATA(x);
			rtl_atomic_inc(&priv->pshare->rx_enqueue_cnt);
			}
#endif
			reuse = 0;
		}
	}
#ifdef UNIVERSAL_REPEATER
	else if (IS_ROOT_INTERFACE(priv) && (opmode & WIFI_STATION_STATE) && IS_DRV_OPEN(GET_VXD_PRIV(priv))
			&& (GET_VXD_PRIV(priv)->drv_state & DRV_STATE_VXD_AP_STARTED)) {
		if (IS_BSSID(GET_VXD_PRIV(priv), bssid))
		{
			priv = GET_VXD_PRIV(priv);
#ifndef DRVMAC_LB
			// check power save state
			struct stat_info *pstat = get_stainfo(priv, GetAddr2Ptr(pframe));
			if (pstat && (pstat->state & WIFI_ASOC_STATE) &&
				(GetPwrMgt(pframe) != ((pstat->state & WIFI_SLEEP_STATE ? 1 : 0)))) {
				pwr_state(priv, pfrinfo);
			}
#endif
#ifdef RTL8190_DIRECT_RX
			rtl8192cd_rx_dataframe(priv, NULL, pfrinfo);
#else
#if defined(__OSK__) && defined(RX_BUFFER_GATHER)
			if (pfrinfo->gather_flag & GATHER_FIRST){
    			rtl8192cd_rx_dataframe(priv, NULL, pfrinfo);
    		}else
#endif
			{
			SMP_LOCK_RX_DATA(x);
			list_add_tail(&(pfrinfo->rx_list), &priv->rx_datalist);
			SMP_UNLOCK_RX_DATA(x);
			rtl_atomic_inc(&priv->pshare->rx_enqueue_cnt);
			}
#endif
			reuse = 0;
		}
	}
#endif
	else
		reuse = 1;

out:

	/* update priv's point */
	*priv_p = priv;
	rx_sum_up(priv, NULL, pfrinfo);

	return reuse;
}

#ifdef CONFIG_RTL_OFFLOAD_DRIVER
static struct rx_frinfo *reassemble_offload(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	struct d2h_rxpkt_info *rxpkt_info;
	struct list_head	*phead, *plist;
	unsigned short		seq=0;
	unsigned char		tofr_ds=0;
	unsigned int		iv, icv, mic, privacy=0, offset;
	struct sk_buff		*pskb;
	struct rx_frinfo	*pfrinfo, *pfirstfrinfo=NULL;
	unsigned char		*pframe=NULL, *pfirstframe=NULL, tail[16]; //4, 12, and 8 for WEP/TKIP/AES
	unsigned long		flags;
	int			i;
	
	struct list_head frag_list;
	
	INIT_LIST_HEAD(&frag_list);
	
	DEFRAG_LOCK(flags);
	list_del_init(&pstat->defrag_list);
	list_splice_init(&pstat->frag_list, &frag_list);
	pstat->frag_count = 0;
	DEFRAG_UNLOCK(flags);
	
	phead = &frag_list;

	// checking  all the seq should be the same, and the frg should monotically increase
	for(i=0, plist=phead->next; plist!=phead; plist=plist->next, i++)
	{
		pfrinfo = list_entry(plist, struct rx_frinfo, mpdu_list);
		rxpkt_info = pfrinfo->rxpkt_info;
		pframe = get_pframe(pfrinfo);

		if (pfrinfo->frag != i)
		{
			DEBUG_ERR("RX DROP: FragNum did not match, FragNum=%d, GetFragNum(pframe)=%d\n",
				i, pfrinfo->frag);
			goto unchainned_all;
		}

		if (i == 0)
		{
			seq = pfrinfo->seq;
			privacy = GetPrivacy(rxpkt_info->frame_ctrl);
			tofr_ds = pfrinfo->to_fr_ds;
		}
		else
		{
			if (pfrinfo->seq != seq)
			{
				DEBUG_ERR("RX DROP: Seq is not correct, seq=%d, GetSequence(pframe)=%d\n",
					seq, pfrinfo->seq);
				goto unchainned_all;
			}

			if (GetPrivacy(rxpkt_info->frame_ctrl) != privacy)
			{
				DEBUG_ERR("RX DROP: Privacy is not correct, privacy=%d, GetPrivacy(pframe)=%d\n",
					privacy, GetPrivacy(rxpkt_info->frame_ctrl));
				goto unchainned_all;
			}

			if (pfrinfo->to_fr_ds != tofr_ds)
			{
				DEBUG_ERR("RX DROP: to_fr_ds did not match, tofr_ds=%d, pfrinfo->to_fr_ds=%d\n",
					tofr_ds, pfrinfo->to_fr_ds);
				goto unchainned_all;
			}
		}
	}

	privacy = get_privacy(priv, pstat, &iv, &icv, &mic);

	offset = iv;
	//offset += get_hdrlen(priv, pframe);

	// below we are going to re-assemble the whole pkts...
	for(i=0, plist=phead->next; plist!=phead; plist=plist->next, i++)
	{
		pfrinfo = list_entry(plist, struct rx_frinfo, mpdu_list);

		if (pfrinfo->pktlen <= (offset + icv + mic))
		{
			DEBUG_ERR("RX DROP: Frame length bad (%d)\n", pfrinfo->pktlen);
			pfirstfrinfo = NULL;
			goto unchainned_all;
		}

		if (i == 0)
		{
			pfirstfrinfo = pfrinfo;
			pfirstframe  = get_pframe(pfrinfo);
			pfirstframe += pfrinfo->pktlen - (icv + mic);

			if (icv + mic)
			{
				memcpy((void *)tail, (void *)(pfirstframe), (icv + mic));
				pfirstfrinfo->pktlen -= (icv + mic);
			}
			continue;
		}

		// check if too many frags...
		pfirstfrinfo->pktlen += (pfrinfo->pktlen - offset - icv - mic);
#if defined(CONFIG_PCI_HCI)
		if (pfirstfrinfo->pktlen >= (RX_BUF_LEN - offset - icv - mic - 200))
#elif defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
		if ((pfirstframe + (pfrinfo->pktlen - offset)) >=
			((unsigned char *)((unsigned long)(pfirstfrinfo->pskb->end))))
#endif
		{
			DEBUG_ERR("RX DROP: over rx buf size after reassemble...\n");
			pfirstfrinfo = NULL;
			goto unchainned_all;
		}

		// here we should check if all these frags exceeds the buf size
		memcpy(pfirstframe, get_pframe(pfrinfo) + offset, pfrinfo->pktlen - offset - icv - mic);
		pfirstframe += (pfrinfo->pktlen - offset - icv - mic);
	}

	if (icv + mic)
	{
		memcpy((void *)pfirstframe, (void *)tail, (icv + mic));
		pfirstfrinfo->pktlen += (icv + mic);
	}

	// take the first frame out of fragment list
	plist = phead->next;
	list_del_init(plist);

unchainned_all:	// dequeue all the queued-up frag, free skb, and init_list_head again...
	
	while (!list_empty(phead)) {
		plist = phead->next;
		list_del(plist);
		
		pfrinfo = list_entry(plist, struct rx_frinfo, mpdu_list);
		pskb = get_pskb(pfrinfo);
		rtl_kfree_skb(priv, pskb, _SKB_RX_IRQ_);
		
		if (pfirstfrinfo == NULL)
			priv->ext_stats.rx_data_drops++;
	}

	return pfirstfrinfo;
}

static struct rx_frinfo *defrag_frame_main_offload(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
	struct d2h_rxpkt_info *rxpkt_info = pfrinfo->rxpkt_info;
	unsigned char *da, *sa;
	struct stat_info *pstat=NULL;
	unsigned int  res, hdr_len, len;
	int status, privacy=0, pos;
	unsigned long flags;
	unsigned char tkipmic[8], rxmic[8];
	unsigned char *pframe;
	unsigned char hw_didnt_decrypt=0;

	pframe = get_pframe(pfrinfo);
	hdr_len = pfrinfo->hdr_len;
	da	= pfrinfo->da;
	sa  = pfrinfo->sa;
	len = pfrinfo->pktlen;

#ifdef CLIENT_MODE
	if (OPMODE & WIFI_STATION_STATE) {
		sa = BSSID;
	}
#endif

#ifdef A4_STA
    if(pfrinfo->to_fr_ds == 3 && priv->pmib->miscEntry.a4_enable) {
        da = GetAddr1Ptr(pframe);
        sa = GetAddr2Ptr(pframe);
    }
#endif

	/*---------first of all check if sa is assocated---------*/
	if (OPMODE & WIFI_AP_STATE) {
#if defined(CONFIG_RTK_MESH) || defined(WDS)
		// for 802.11s case, pstat will not be NULL, because we have check it in validate-mpdu
		if (pfrinfo->to_fr_ds == 3) {
			pstat = get_stainfo(priv, sa);
			if (pstat == NULL){
				priv->ext_stats.rx_data_drops++;
				DEBUG_ERR("RX Drop: WDS rx data with pstat == NULL\n");
				goto free_skb_in_defrag;
			} else {
				goto check_privacy;
			}
		}
		else
#endif
		{
        #ifdef HW_FILL_MACID
            pstat = get_stainfo_fast(priv,sa,pfrinfo->macid);               
        #else
            pstat = get_stainfo(priv,sa);               
        #endif //#ifdef HW_FILL_MACID

		#ifdef A4_STA
            if (priv->pmib->miscEntry.a4_enable && pstat) {
                pstat = a4_rx_check_reuse_ap(priv, pstat, pfrinfo);
            }
        #endif

        }
	}
#ifdef CLIENT_MODE
	else if (OPMODE & WIFI_STATION_STATE) {
		pstat = get_stainfo(priv, sa);
	}
	else	// Ad-hoc
		pstat = get_stainfo(priv, sa);
#endif

	if (pstat == NULL)
	{
		status = _RSON_CLS2_;
		priv->ext_stats.rx_data_drops++;
		DEBUG_ERR("RX DROP: class 2 error!\n");
		goto data_defrag_error;
	}
	else if (!(pstat->state & WIFI_ASOC_STATE))
	{
		status = _RSON_CLS3_;
		priv->ext_stats.rx_data_drops++;
		DEBUG_ERR("RX DROP: class 3 error!\n");
		goto data_defrag_error;
	}
	else {}

	/*-------------------get privacy-------------------*/
#if defined(CONFIG_RTK_MESH) || defined(WDS)
check_privacy:
#endif

	if (OPMODE & WIFI_AP_STATE) {
#ifdef CONFIG_RTK_MESH
		//modify by Joule for SECURITY
		if(pfrinfo->is_11s)
			privacy = IS_MCAST(da) ? _NO_PRIVACY_ : get_sta_encrypt_algthm(priv, pstat);
		else
#endif
#ifdef WDS
		if (pfrinfo->to_fr_ds == 3)
			privacy = priv->pmib->dot11WdsInfo.wdsPrivacy;
		else
#endif
		privacy = get_sta_encrypt_algthm(priv, pstat);
	}
#ifdef CLIENT_MODE
	else {
		if (IS_MCAST(da)) {
#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
			if (!IS_ROOT_INTERFACE(priv) && !IEEE8021X_FUN &&
				((priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _WEP_40_PRIVACY_) ||
				 (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _WEP_104_PRIVACY_))){
				 privacy = priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm;
			}else
#endif
#if defined(CONFIG_RTL_WAPI_SUPPORT)
			if (pstat&&pstat->wapiInfo&&pstat->wapiInfo->wapiType!=wapiDisable){
				privacy = _WAPI_SMS4_;
			}else
#endif
			{
    			// iv, icv and mic are not be used below. Don't care them!
				privacy = priv->pmib->dot11GroupKeysTable.dot11Privacy;
			}
		}
		else
		{
			privacy = get_sta_encrypt_algthm(priv, pstat);
		}
	}

	if (IS_MCAST(da))
	{
		if (pfrinfo->tpcache == pstat->tpcache_mcast)
		{
			priv->ext_stats.rx_decache++;
			SNMP_MIB_INC(dot11FrameDuplicateCount, 1);
			goto free_skb_in_defrag;
		}
		else
			pstat->tpcache_mcast = pfrinfo->tpcache;
	}
	else
#endif
	/*-------------------check retry-------------------*/
	if (is_qos_data(rxpkt_info->frame_ctrl)) {
		pos = pfrinfo->seq & (TUPLE_WINDOW - 1);
		if (IS_MCAST(da)) {
			// ignore check of multicast packet to workaround Veriware test.
		}
		else if (pfrinfo->tpcache == pstat->tpcache[pfrinfo->tid][pos]) {
			if (!pfrinfo->amsdu_cut || pfrinfo->last_msdu) {
				priv->ext_stats.rx_decache++;
				SNMP_MIB_INC(dot11FrameDuplicateCount, 1);
			}
			goto free_skb_in_defrag;
		}
		else if (!pfrinfo->amsdu_cut || pfrinfo->last_msdu)
			pstat->tpcache[pfrinfo->tid][pos] = pfrinfo->tpcache;
	}
	else {
		if (GetRetry(rxpkt_info->frame_ctrl) && (!pfrinfo->amsdu_cut || pfrinfo->last_msdu)) {
			if (pfrinfo->tpcache == pstat->tpcache_mgt) {
				priv->ext_stats.rx_decache++;
				SNMP_MIB_INC(dot11FrameDuplicateCount, 1);
				goto free_skb_in_defrag;
			}
		}
		pstat->tpcache_mgt = pfrinfo->tpcache;
	}

	/*-------------------------------------------------------*/
	/*-----------insert MPDU-based decrypt below-------------*/
	/*-------------------------------------------------------*/

#ifdef SUPPORT_SNMP_MIB
	if (GetPrivacy(rxpkt_info->frame_ctrl) && privacy == _NO_PRIVACY_)
		SNMP_MIB_INC(dot11WEPUndecryptableCount, 1);

	if (!GetPrivacy(rxpkt_info->frame_ctrl) && privacy != _NO_PRIVACY_)
		SNMP_MIB_INC(dot11WEPExcludedCount, 1);
#endif

        if (!GetPrivacy(rxpkt_info->frame_ctrl) && privacy != _NO_PRIVACY_)
        {
                priv->ext_stats.rx_data_drops++;
                DEBUG_ERR("RX DROP: Recv unencrypted packet!\n");
                goto free_skb_in_defrag;
        }

#ifdef CONFIG_IEEE80211W
	if (GetPrivacy(rxpkt_info->frame_ctrl) && !UseSwCrypto(priv, pstat, IS_MCAST(GetAddr1Ptr(pframe)), 0) && pfrinfo->sw_dec)
#else
	if (GetPrivacy(rxpkt_info->frame_ctrl) && !UseSwCrypto(priv, pstat, IS_MCAST(GetAddr1Ptr(pframe))) && pfrinfo->sw_dec)
#endif
	{	
		hw_didnt_decrypt = 1;
	}
#if 0
	// check whether WEP bit is set in mac header and sw encryption
#ifdef CONFIG_IEEE80211W
		if (GetPrivacy(rxpkt_info->frame_ctrl) && (UseSwCrypto(priv, pstat, IS_MCAST(GetAddr1Ptr(pframe)), 0)||hw_didnt_decrypt)) // 0: PMF
#else
		if (GetPrivacy(rxpkt_info->frame_ctrl) && (UseSwCrypto(priv, pstat, IS_MCAST(GetAddr1Ptr(pframe)))||hw_didnt_decrypt))
#endif
	{
#if defined(CONFIG_RTL_WAPI_SUPPORT)
		if (privacy==_WAPI_SMS4_)
		{
			/*	Decryption	*/
//			SAVE_INT_AND_CLI(flags);
			res = SecSWSMS4Decryption(priv, pstat, pfrinfo);
//			RESTORE_INT(flags);
			if (res == FAIL)
			{
				priv->ext_stats.rx_data_drops++;
				DEBUG_ERR("RX DROP: WAPI decrpt error!\n");
				goto free_skb_in_defrag;
			}
			pframe = get_pframe(pfrinfo);
		} else
#endif
		if (privacy == _TKIP_PRIVACY_)
		{
			res = tkip_decrypt(priv, pfrinfo, pfrinfo->pktlen);
			if (res == FAIL)
			{
				priv->ext_stats.rx_data_drops++;
				DEBUG_ERR("RX DROP: Tkip decrpt error!\n");
				goto free_skb_in_defrag;
			}
		}
		else if (privacy == _CCMP_PRIVACY_)
		{
			res = aesccmp_decrypt(priv, pfrinfo
#ifdef CONFIG_IEEE80211W				
				, 0
#endif
			);
			if (res == FAIL)
			{
				priv->ext_stats.rx_data_drops++;
				DEBUG_ERR("RX DROP: AES decrpt error!\n");
				goto free_skb_in_defrag;
			}
		}
		else if (privacy == _WEP_40_PRIVACY_ || privacy == _WEP_104_PRIVACY_)
		{
			res = wep_decrypt(priv, pfrinfo, pfrinfo->pktlen, privacy, 0);
			if (res == FAIL)
			{
				priv->ext_stats.rx_data_drops++;
				DEBUG_ERR("RX DROP: WEP decrpt error!\n");
				goto free_skb_in_defrag;
			}
		}
		else
		{
			DEBUG_ERR("RX DROP: encrypted packet but no key in sta or wrong enc type!\n");
			goto free_skb_in_defrag;
		}

	}
	/*----------------End of MPDU-based decrypt--------------*/
#endif
	if (GetMFrag(rxpkt_info->frame_ctrl))
	{
		if (pstat->frag_count > MAX_FRAG_COUNT)
		{
			priv->ext_stats.rx_data_drops++;
			DEBUG_ERR("RX DROP: station has received too many frags!\n");
			goto free_skb_in_defrag;
		}
		
		DEFRAG_LOCK(flags);

		if (pstat->frag_count == 0) // the first frag...
			pstat->frag_to = priv->frag_to;

		if (list_empty(&pstat->defrag_list))
			list_add_tail(&pstat->defrag_list, &priv->defrag_list);

		list_add_tail(&pfrinfo->mpdu_list, &pstat->frag_list);
		pstat->frag_count++;
		
		DEFRAG_UNLOCK(flags);
		
		return (struct rx_frinfo *)NULL;
	}
	else
	{
		if(pfrinfo->frag)
		{
			DEFRAG_LOCK(flags);
			list_add_tail(&pfrinfo->mpdu_list, &pstat->frag_list);
			DEFRAG_UNLOCK(flags);
			
			pfrinfo = reassemble_offload(priv, pstat);
			if (pfrinfo == NULL)
				return (struct rx_frinfo *)NULL;
		}
	}

	/*-----discard non-authorized packet before MIC check----*/
	if (OPMODE & WIFI_AP_STATE) {
#if defined(CONFIG_RTK_MESH) || defined(WDS)
		if (pfrinfo->to_fr_ds != 3)
#endif
		if (auth_filter(priv, pstat, pfrinfo) == FAIL) {
			priv->ext_stats.rx_data_drops++;
			DEBUG_ERR("RX DROP: due to auth_filter fails\n");
			goto free_skb_in_defrag;
		}
	}
#ifdef CLIENT_MODE
	else if (OPMODE & WIFI_STATION_STATE) {
		if (auth_filter(priv, pstat, pfrinfo) == FAIL) {
			priv->ext_stats.rx_data_drops++;
			DEBUG_ERR("RX DROP: due to auth_filter fails\n");
			goto free_skb_in_defrag;
		}
	}
#endif

	/*------------------------------------------------------------------------*/
						//insert MSDU-based digest here!
	/*------------------------------------------------------------------------*/
#if 0	
	if (GetPrivacy(rxpkt_info->frame_ctrl) && (privacy == _TKIP_PRIVACY_))
	{
		pframe = get_pframe(pfrinfo);
		len = pfrinfo->pktlen;

		//truncate Michael...
		memcpy((void *)rxmic, (void *)(pframe + len - 8 - 4), 8); // 8 michael, 4 icv
		SAVE_INT_AND_CLI(flags);
		tkip_rx_mic(priv, pframe, pfrinfo->da,  pfrinfo->sa,
			pfrinfo->tid, pframe + hdr_len + 8,
			len - hdr_len - 8 - 8 - 4, tkipmic, 0); // 8 IV, 8 Mic, 4 ICV
		RESTORE_INT(flags);

		if(memcmp(rxmic, tkipmic, 8))
		{
			priv->ext_stats.rx_data_drops++;
#ifdef _SINUX_
			printk("RX DROP: MIC error! Indicate to protection mechanism\n");
			mic_error_report(0);
#else
			DEBUG_ERR("RX DROP: MIC error! Indicate to protection mechanism\n");
#endif
			if (OPMODE & WIFI_AP_STATE) {
#ifdef RTL_WPA2
#ifdef _SINUX_
				printk("%s: DOT11_Indicate_MIC_Failure %02X:%02X:%02X:%02X:%02X:%02X \n", (char *)__FUNCTION__,pstat->cmn_info.mac_addr[0],pstat->cmn_info.mac_addr[1],pstat->cmn_info.mac_addr[2],pstat->cmn_info.mac_addr[3],pstat->cmn_info.mac_addr[4],pstat->cmn_info.mac_addr[5]);
#else
				PRINT_INFO("%s: DOT11_Indicate_MIC_Failure %02X:%02X:%02X:%02X:%02X:%02X \n", (char *)__FUNCTION__,pstat->cmn_info.mac_addr[0],pstat->cmn_info.mac_addr[1],pstat->cmn_info.mac_addr[2],pstat->cmn_info.mac_addr[3],pstat->cmn_info.mac_addr[4],pstat->cmn_info.mac_addr[5]);
#endif

#endif
#ifdef WDS
				if ((pfrinfo->to_fr_ds == 3) &&
						pstat && (pstat->state & WIFI_WDS))
					goto free_skb_in_defrag;
#endif

#if defined(TAROKO_0)
//TODO, translate to IPC				DOT11_Indicate_MIC_Failure(priv->dev, pstat);
#else
				DOT11_Indicate_MIC_Failure(priv->dev, pstat);
#endif
			}
#ifdef CLIENT_MODE
			else if (OPMODE & WIFI_STATION_STATE)
				DOT11_Indicate_MIC_Failure_Clnt(priv, pfrinfo->sa);
#endif
			goto free_skb_in_defrag;
		}
	}
#endif
	return pfrinfo;

data_defrag_error:

	if (OPMODE & WIFI_AP_STATE){
		issue_deauth(priv,sa,status);
		if (pstat != NULL){
#if defined(CONFIG_PCI_HCI)
			free_stainfo(priv, pstat);
#elif defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
			notify_del_sta(priv, pstat);
#endif
		}
	}
#ifdef CLIENT_MODE
	else {
		if (pstat == NULL) {
			DEBUG_ERR("rx data with pstat == NULL\n");
		}
		else if (!(pstat->state & WIFI_ASOC_STATE)) {
			DEBUG_ERR("rx data with pstat not associated\n");
		}
	}
#endif

free_skb_in_defrag:

	rtl_kfree_skb(priv, get_pskb(pfrinfo), _SKB_RX_);

	return (struct rx_frinfo *)NULL;
}


static struct rx_frinfo *defrag_frame_offload(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
	struct sk_buff *pskb = get_pskb(pfrinfo);
	struct rx_frinfo *prx_frinfo = NULL;
	struct d2h_rxpkt_info *rxpkt_info = pfrinfo->rxpkt_info;
	unsigned int encrypt;
	unsigned char *pframe;

	// rx a encrypt packet but encryption is not enabled in local mib, discard it
	pframe = get_pframe(pfrinfo);
	encrypt = GetPrivacy(rxpkt_info->frame_ctrl);

//modify by Joule for SECURITY
// here maybe need do some tune; plus
#ifdef CONFIG_RTK_MESH	/*-------*/
	if (encrypt && (
		(pfrinfo->to_fr_ds==3 && (
#ifdef WDS
		GET_MIB(priv)->dot1180211sInfo.mesh_enable ==0	?
		priv->pmib->dot11WdsInfo.wdsPrivacy==_NO_PRIVACY_ :
#endif
		priv->pmib->dot11sKeysTable.dot11Privacy ==_NO_PRIVACY_	)) ||
		(pfrinfo->to_fr_ds!=3 && priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm==_NO_PRIVACY_
#if defined(CONFIG_RTL_WAPI_SUPPORT)
			&& priv->pmib->wapiInfo.wapiType==wapiDisable
#endif
		)))
#else	/*-------*/
// origin
#ifdef WDS
	if (encrypt && (
			(pfrinfo->to_fr_ds==3 && priv->pmib->dot11WdsInfo.wdsPrivacy==_NO_PRIVACY_) ||
			(pfrinfo->to_fr_ds!=3 && priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm==_NO_PRIVACY_))
#if defined(CONFIG_RTL_WAPI_SUPPORT)
			&& priv->pmib->wapiInfo.wapiType==wapiDisable
#endif
	)
#else
	if (encrypt && priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _NO_PRIVACY_
#if defined(CONFIG_RTL_WAPI_SUPPORT)
			&& priv->pmib->wapiInfo.wapiType==wapiDisable
#endif
	)
#endif
#endif/*-------*/
	{
		priv->ext_stats.rx_data_drops++;
		DEBUG_ERR("RX DROP: Discard a encrypted packet!\n");
		rtl_kfree_skb(priv, pskb, _SKB_RX_);
		return (struct rx_frinfo *)NULL;
	}

#ifdef CONFIG_RTK_MESH
        if (pfrinfo->to_fr_ds==3 && !encrypt && (
#ifdef WDS
		GET_MIB(priv)->dot1180211sInfo.mesh_enable ==0	?
		priv->pmib->dot11WdsInfo.wdsPrivacy!=_NO_PRIVACY_ :
#endif
		(priv->pmib->dot11sKeysTable.dot11Privacy != _NO_PRIVACY_ && !IS_MCAST(pfrinfo->da)))) {
		priv->ext_stats.rx_data_drops++;
		DEBUG_ERR("RX DROP: Discard a un-encrypted WDS/MESH packet!\n");
		rtl_kfree_skb(priv, pskb, _SKB_RX_);
		SNMP_MIB_INC(dot11WEPExcludedCount, 1);
		return (struct rx_frinfo *)NULL;
	}
#else
//origin
#ifdef WDS
	if (pfrinfo->to_fr_ds==3 && !encrypt && priv->pmib->dot11WdsInfo.wdsPrivacy!=_NO_PRIVACY_) {
		priv->ext_stats.rx_data_drops++;
		DEBUG_ERR("RX DROP: Discard a un-encrypted WDS packet!\n");
		rtl_kfree_skb(priv, pskb, _SKB_RX_);
		SNMP_MIB_INC(dot11WEPExcludedCount, 1);
		return (struct rx_frinfo *)NULL;
	}
#endif
#endif
	prx_frinfo = defrag_frame_main_offload(priv, pfrinfo);

	return prx_frinfo;
}

int rtl8192cd_rx_dispatch_offload(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
	unsigned char *pframe;
	struct d2h_rxpkt_info *rxpkt_info;
	struct stat_info *pstat, *dst_pstat;
	struct sk_buff *pskb=NULL, *pnewskb=NULL;
	int reuse=0;
	int dontBcast2otherSta=0, do_rc=0, do_process, i;
#if  defined(__KERNEL__)
	struct net_bridge_port *br_port=NULL;
#endif


#ifdef CLIENT_MODE
	unsigned char *dest;
#endif

	pfrinfo->to_fr_ds = get_tofr_ds(pfrinfo->rxpkt_info->frame_ctrl);
	if (pfrinfo->to_fr_ds == WIFI_FRMCTL_FROM_DS) {
		if ((OPMODE & (WIFI_STATION_STATE | WIFI_ASOC_STATE)) ==
				(WIFI_STATION_STATE | WIFI_ASOC_STATE))
			; /* go ahead to process data frame */
#ifdef UNIVERSAL_REPEATER
		else if ((OPMODE & WIFI_AP_STATE) && IS_DRV_OPEN(GET_VXD_PRIV(priv))) {
			/* do something for Repeater mode */
			priv = GET_VXD_PRIV(priv);
		}
#endif
		else
			return FAIL;
	} else if (pfrinfo->to_fr_ds == WIFI_FRMCTL_TO_DS) {
		if (OPMODE & WIFI_AP_STATE)
			; /* go ahead to process data frame */
#ifdef UNIVERSAL_REPEATER
		else if (IS_ROOT_INTERFACE(priv) && (OPMODE & WIFI_STATION_STATE) &&
				IS_DRV_OPEN(GET_VXD_PRIV(priv)) &&
				(GET_VXD_PRIV(priv)->drv_state & DRV_STATE_VXD_AP_STARTED)) {
			/* do something for Repeater mode */
			priv = GET_VXD_PRIV(priv);
		}
#endif
		else
			return FAIL;
	}

	rxpkt_info = pfrinfo->rxpkt_info;
	pskb = pfrinfo->pskb;
	pframe = get_pframe(pfrinfo);

	pfrinfo->da	= pskb->data;
	pfrinfo->sa	= pskb->data + MACADDRLEN;
	INIT_LIST_HEAD(&(pfrinfo->mpdu_list));
	INIT_LIST_HEAD(&(pfrinfo->rx_list));
	pskb->len = pfrinfo->pktlen;
	pskb->tail = pskb->data + pskb->len;

#ifdef CLIENT_MODE
	if (OPMODE & WIFI_STATION_STATE)
		pstat = get_stainfo(priv, BSSID);
	else
#endif
	pstat = get_stainfo(priv, pfrinfo->sa);

	// for SW LED
	priv->pshare->LED_rx_cnt++;

#ifdef WIFI_WMM
	if (is_qos_data(rxpkt_info->frame_ctrl)) {
		if ((OPMODE & WIFI_AP_STATE) && (QOS_ENABLE)) {
			if ((pfrinfo->tid == 7) || (pfrinfo->tid == 6)) {
				priv->pshare->phw->VO_pkt_count++;
			} else if ((pfrinfo->tid == 5) || (pfrinfo->tid == 4)) {
				priv->pshare->phw->VI_pkt_count++;
				if (priv->pshare->rf_ft_var.wifi_beq_iot)
					priv->pshare->phw->VI_rx_pkt_count++;
			} else if ((pfrinfo->tid == 2) || (pfrinfo->tid == 1)) {
				priv->pshare->phw->BK_pkt_count++;
			}
		}
	}
#endif

//TODO, power saving
#if defined(TAROKO_0)
#else
	// check power save state
	if (OPMODE & WIFI_AP_STATE) {
		if (pstat && (pstat->state & WIFI_ASOC_STATE) &&
			(GetPwrMgt(rxpkt_info->frame_ctrl) != ((pstat->state & WIFI_SLEEP_STATE ? 1 : 0))))
			pwr_state(priv, pfrinfo);
	}
#endif

	/* defrag */
	if ((pfrinfo = defrag_frame_offload(priv, pfrinfo)) == NULL)
		return SUCCESS;
    pskb = pfrinfo->pskb;

	if (pstat == NULL) {
		rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
		return SUCCESS;
	}

	if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) &&
		priv->pmib->reorderCtrlEntry.ReorderCtrlEnable) {
		if ((OPMODE & WIFI_AP_STATE) || !IS_MCAST(pfrinfo->da))
			do_rc = 1;
	}

	if (OPMODE & WIFI_AP_STATE)
	{
		// log rx statistics...
		rx_sum_up(priv, pstat, pfrinfo);
#ifdef RX_CRC_EXPTIMER
		priv->ext_stats.rx_crc_exptimer++;
#endif
		priv->pshare->NumRxBytesUnicast += pfrinfo->pktlen;
		update_sta_rssi(priv, pstat, pfrinfo);

#ifdef DETECT_STA_EXISTANCE
#ifdef CONFIG_WLAN_HAL
		if (IS_HAL_CHIP(priv))
		{
			if (pstat->leave != 0)
			{
#if defined(CONFIG_PCI_HCI)
#if defined(TAROKO_0)
                HAL_H2C_MSRRPT_PARA MSRRPT_para;
                MSRRPT_para.macid = pstat->cmn_info.mac_id;
                MSRRPT_para.opmode = INCREASE;

                GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, &MSRRPT_para);
#else
				GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, pstat, INCREASE);
#endif
				pstat->bDrop = 0;
#elif defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
				notify_update_sta_msr(priv, pstat, INCREASE);
#endif
			}
			pstat->rx_last_good_time = priv->up_time;
		}
#endif
		pstat->leave = 0;
#endif

#ifdef SUPPORT_SNMP_MIB
		if (IS_MCAST(pfrinfo->da))
			SNMP_MIB_INC(dot11MulticastReceivedFrameCount, 1);
#endif

#if defined(WIFI_WMM) && defined(WMM_APSD)
		if ((QOS_ENABLE) && (APSD_ENABLE) && (pstat->QosEnabled) && (pstat->apsd_bitmap & 0x0f) &&
			((pstat->state & (WIFI_ASOC_STATE|WIFI_SLEEP_STATE)) == (WIFI_ASOC_STATE|WIFI_SLEEP_STATE)) &&
			(GetFrameSubType(rxpkt_info->frame_ctrl) == (WIFI_QOS_DATA))) {
			process_APSD_dz_queue(priv, pstat, pfrinfo->tid);
		}
#endif

#ifdef PREVENT_BROADCAST_STORM
		if (pfrinfo->da[0] == 0xff)
			pstat->rx_pkts_bc++;
#endif

		// AP always receive unicast frame only
#ifdef WDS
		if (pfrinfo->to_fr_ds!=3 && IS_MCAST(pfrinfo->da))
#else
		if (IS_MCAST(pfrinfo->da))
#endif
		{
#ifdef PREVENT_ARP_SPOOFING
			if (check_arp_spoofing(priv, pskb)) {
				priv->ext_stats.rx_data_drops++;
				DEBUG_ERR("RX DROP: Detected ARP spoofing!\n");
				return FAIL;
			}
#endif

#ifdef SUPPORT_TX_MCAST2UNI
			if (IP_MCAST_MAC(pskb->data) && IS_IGMP_PROTO(pskb->data))
			{
				dontBcast2otherSta = 1;
			}
#endif

#if (defined(__LINUX_2_6__) || defined(__ECOS)) && defined(CONFIG_RTL_IGMP_SNOOPING)
			/* added by qinjunjie, to avoid igmpv1/v2 report suppress */
			if (rtl8192cd_isIgmpV1V2Report(pskb->data))
			{
				//printk("%s:%d,receive igmpv1/v2 report\n",__FUNCTION__,__LINE__);
				goto mcast_netif_rx_go;
			}
#if defined (CONFIG_RTL_MLD_SNOOPING)
			if(rtl8192cd_isMldV1V2Report(pskb->data))
			{
				goto mcast_netif_rx_go;
			}
#endif
#endif

#if !defined(NOT_RTK_BSP) && !defined(CONFIG_OPENWRT_SDK) && !(defined(CONFIG_RTL8672) && defined(__LINUX_3_18__))
			if (priv->pmib->dot11OperationEntry.guest_access)
				pskb->__unused = 0xe5;
#endif

#ifdef __KERNEL__
			br_port = GET_BR_PORT(priv->dev);

			// if we are STP aware, don't broadcast received BPDU
			if (!(br_port &&
				br_port->br->stp_enabled &&
				!memcmp(pskb->data, "\x01\x80\xc2\x00\x00\x00", 6)))
#endif
			{
				if ((priv->pmib->dot11OperationEntry.block_relay==0) ||
					(priv->pmib->dot11OperationEntry.block_relay==3 && priv->pmib->hs2Entry.ICMPv4ECHO == 1))
				{
#if defined(_SINUX_) && defined(CONFIG_RTL865X_ETH_PRIV_SKB)
					extern struct sk_buff *priv_skb_copy(struct sk_buff *skb);
					pnewskb = priv_skb_copy(pskb);
#else
					pnewskb = skb_copy(pskb, GFP_ATOMIC);
#endif
					if (pnewskb) {
#ifdef GBWC
						if (GBWC_forward_check(priv, pnewskb, pstat)) {
							// packet is queued, nothing to do
						}
						else
#endif
						{
#ifdef TX_SCATTER
							pnewskb->list_num = 0;
#endif
							if (dontBcast2otherSta) {
								dev_kfree_skb_any(pnewskb);
							} else {
#ifdef PREVENT_BROADCAST_STORM
								if (pfrinfo->da[0] == 0xff) {
									if (pstat->rx_pkts_bc > BROADCAST_STORM_THRESHOLD) {
										priv->ext_stats.rx_data_drops++;
										DEBUG_ERR("RX DROP: Broadcast storm happened!\n");
										dev_kfree_skb_any(pnewskb);
									}
									else {
#if defined(TAROKO_0) && defined(PE_RX_BCMC_PKT_RECV)
										//Copy Broadcast frame to A17 through IPC
										if (1) {
											//unsigned int shift_length = pfrinfo->rxbuffsize - pnewskb->len;
											//printk("BC PKT SEND %d, leng = %d, pfrinfo->rxbuffsize = %d\r\n", __LINE__, pnewskb->len, pfrinfo->rxbuffsize);
											//dev_kfree_skb_any(pnewskb);
											taroko_t2h_ipc_pkt_send(pnewskb, (unsigned int)pnewskb->data, pnewskb->len);
										}else
#endif
										if (rtl8192cd_start_xmit(pnewskb, priv->dev))
											rtl_kfree_skb(priv, pnewskb, _SKB_TX_);
									}
								}
								else
#endif
								{
									if (rtl8192cd_start_xmit(pnewskb, priv->dev))
										rtl_kfree_skb(priv, pnewskb, _SKB_TX_);
								}
							}
						}
					}
				}
			}

#if (defined(__LINUX_2_6__) || defined(__ECOS)) && defined(CONFIG_RTL_IGMP_SNOOPING)
mcast_netif_rx_go:
#endif

			if (do_rc) {
				*(unsigned int *)&(pfrinfo->pskb->cb[4]) = 0;
				if (reorder_ctrl_check(priv, pstat, pfrinfo))
					rtl_netif_rx(priv, pfrinfo->pskb, pstat);
			}
			else
				rtl_netif_rx(priv, pskb, pstat);
		}
		else
		{
			// unicast.. the status of sa has been checked in defrag_frame.
			// however, we should check if the da is in the WDS to see if we should

#ifdef PREVENT_ARP_SPOOFING
			if (check_arp_spoofing(priv, pskb)) {
				priv->ext_stats.rx_data_drops++;
				DEBUG_ERR("RX DROP: Detected ARP spoofing!\n");
				return FAIL;
			}
#endif

			dst_pstat = get_stainfo(priv, pfrinfo->da);

#ifdef A4_STA
			if (priv->pmib->miscEntry.a4_enable && (dst_pstat == NULL))
				dst_pstat = a4_sta_lookup(priv, pfrinfo->da);
#endif

#ifdef WDS
			if ((pfrinfo->to_fr_ds==3) ||
				(dst_pstat == NULL) || !(dst_pstat->state & WIFI_ASOC_STATE))
#else
			if ((dst_pstat == NULL) || (!(dst_pstat->state & WIFI_ASOC_STATE)))
#endif
			{
#if  defined(__KERNEL__)
				if (priv->pmib->dot11OperationEntry.guest_access) {
					br_port = GET_BR_PORT(priv->dev);

					if ((*(unsigned short *)(SKB_MAC_HEADER(pskb) + MACADDRLEN*2) != __constant_htons(0x888e)) &&
						(*(unsigned short *)(SKB_MAC_HEADER(pskb) + MACADDRLEN*2) != __constant_htons(0x86dd))
#ifndef CONFIG_RTL8196C_KLD
						&& br_port
#ifdef __LINUX_2_6__
#if defined(CONFIG_RTL_EAP_RELAY) || defined(CONFIG_RTK_INBAND_HOST_HACK)
						&& memcmp(pfrinfo->da, inband_Hostmac, MACADDRLEN)
#else
						&& memcmp(pfrinfo->da, br_port->br->dev->dev_addr, MACADDRLEN)
#endif
#else
						&& memcmp(pfrinfo->da, priv->dev->br_port->br->dev.dev_addr, MACADDRLEN)
#endif
#endif
					) {
						priv->ext_stats.rx_data_drops++;
						DEBUG_ERR("RX DROP: guest access fail!\n");
						return FAIL;
					}
#if !defined(NOT_RTK_BSP) && !defined(CONFIG_OPENWRT_SDK) && !(defined(CONFIG_RTL8672) && defined(__LINUX_3_18__))
					pskb->__unused = 0xe5;
#endif
					//printk("guest packet, addr: %0x2:%02x:%02x:%02x:%02x:%02x\n",da[0],da[1],da[2],da[3],da[4],da[5]);
				}
#if !defined(NOT_RTK_BSP) && !defined(CONFIG_OPENWRT_SDK) && !(defined(CONFIG_RTL8672) && defined(__LINUX_3_18__))
				else
					pskb->__unused = 0;
#endif

#ifdef __LINUX_2_6__
				if (pskb->dev)
					pskb->protocol = eth_type_trans(pskb,pskb->dev);
				else
#endif
					pskb->protocol = eth_type_trans(pskb, priv->dev);

#if defined(HS2_SUPPORT)
				/* Hotspot 2.0 Release 1 */
#ifdef __ECOS
				if (pskb->data[12] == 0x89 && pskb->data[13] == 0x0d)
#else
				if (pskb->protocol == __constant_htons(ETHER_TDLS))
#endif
				{
					priv->ext_stats.rx_data_drops++;
					HS2_DEBUG_INFO("RX DROP: TDLS!\n");
					return FAIL;
				}
#endif

#ifdef EAPOLSTART_BY_QUEUE
				if (*(unsigned short *)(SKB_MAC_HEADER(pskb) + MACADDRLEN*2) == __constant_htons(0x888e))
				{
					unsigned char szEAPOL[] = {0x01, 0x01, 0x00, 0x00};
					DOT11_EAPOL_START Eapol_Start;

					if (!memcmp(pskb->data, szEAPOL, sizeof(szEAPOL)))
					{
						Eapol_Start.EventId = DOT11_EVENT_EAPOLSTART;
						Eapol_Start.IsMoreEvent = FALSE;
						memcpy(&Eapol_Start.MACAddr, SKB_MAC_HEADER(pskb) + MACADDRLEN, WLAN_ETHHDR_LEN);
						DOT11_EnQueue((unsigned long)priv, priv->pevent_queue, (unsigned char*)&Eapol_Start, sizeof(DOT11_EAPOL_START));
						event_indicate(priv, SKB_MAC_HEADER(pskb) + MACADDRLEN, 4);
						return FAIL;	// let dsr free this skb
					}
				}
#endif
#else
#ifdef CONFIG_RTL_819X_ECOS
				if (priv->pmib->dot11OperationEntry.guest_access)
				{
					if ((*(unsigned short *)(SKB_MAC_HEADER(pskb) + MACADDRLEN*2) != __constant_htons(0x888e)) &&
						(*(unsigned short *)(SKB_MAC_HEADER(pskb) + MACADDRLEN*2) != __constant_htons(0x86dd)) &&
						memcmp(pfrinfo->da, freebsd_Hostmac, MACADDRLEN)
					) {
						//wlan sta can't access LAN when guest access is enabled
						priv->ext_stats.rx_data_drops++;
						DEBUG_ERR("RX DROP: guest access fail!\n");
						return FAIL;
					}
#if !defined(NOT_RTK_BSP) && !defined(CONFIG_OPENWRT_SDK) && !(defined(CONFIG_RTL8672) && defined(__LINUX_3_18__))					
					else
						pskb->__unused = 0xe5;
#endif					
				}
#endif
#endif /* __ECOS */

#if (defined(EAP_BY_QUEUE) || defined(INCLUDE_WPA_PSK)) && (!defined(WIFI_HAPD) || defined(HAPD_DRV_PSK_WPS)) && (!defined(RTK_NL80211))
#ifdef WDS
				if ((pfrinfo->to_fr_ds != 3) && (*(unsigned short *)(SKB_MAC_HEADER(pskb) + MACADDRLEN*2) == __constant_htons(0x888e)))
#else
				if (*(unsigned short *)(SKB_MAC_HEADER(pskb) + MACADDRLEN*2) == __constant_htons(0x888e))
#endif
				{
					if (IEEE8021X_FUN
#ifdef INCLUDE_WPA_PSK
						|| (priv->pmib->dot1180211AuthEntry.dot11EnablePSK &&
							((priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _TKIP_PRIVACY_) ||
							 (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _CCMP_PRIVACY_)))
#endif
#ifdef WIFI_SIMPLE_CONFIG
							|| priv->pmib->wscEntry.wsc_enable
#endif
					) {
#if defined(TAROKO_0)
#if defined(PE_RX_EAPOL_PKT_RECV)
						//Translate EAPOL frame to A17 through IPC
						unsigned int shift_length = pfrinfo->rxbuffsize - pskb->len;
						taroko_t2h_ipc_pkt_recv(pskb, pskb->data - shift_length, pfrinfo->rxbuffsize);	
						//ca_lib_dump_mem(pskb->data - shift_length, pfrinfo->rxbuffsize);
						return SUCCESS;
						
#endif
#else
						unsigned short		pkt_len;

#ifdef __ECOS
						pkt_len = pskb->len;
#else
						pkt_len = WLAN_ETHHDR_LEN + pskb->len;
#endif
						priv->Eap_packet->EventId = DOT11_EVENT_EAP_PACKET;
						priv->Eap_packet->IsMoreEvent = FALSE;
						memcpy(&(priv->Eap_packet->packet_len), &pkt_len, sizeof(unsigned short));
#ifdef __ECOS
						memcpy(&priv->Eap_packet->packet, pskb->data, pskb->len);
#else
						memcpy(&(priv->Eap_packet->packet[0]), SKB_MAC_HEADER(pskb), WLAN_ETHHDR_LEN);
						memcpy(&(priv->Eap_packet->packet[WLAN_ETHHDR_LEN]), pskb->data, pskb->len);
#endif
#ifdef EAP_BY_QUEUE

#ifdef INCLUDE_WPS

						wps_NonQueue_indicate_evt(priv ,
							(char *)priv->Eap_packet, sizeof(DOT11_EAP_PACKET));
#else
						DOT11_EnQueue((unsigned long)priv, priv->pevent_queue,
							(unsigned char*)priv->Eap_packet,sizeof(DOT11_EAP_PACKET));

						event_indicate(priv, NULL, -1);
#endif

#endif
#ifdef INCLUDE_WPA_PSK
#ifdef __ECOS
						psk_indicate_evt(priv, DOT11_EVENT_EAP_PACKET, (unsigned char*)&(priv->Eap_packet->packet[6]),
							(unsigned char*)priv->Eap_packet->packet, pskb->len);
#else
						psk_indicate_evt(priv, DOT11_EVENT_EAP_PACKET, (unsigned char*)&(priv->Eap_packet->packet[6]),
							(unsigned char*)priv->Eap_packet->packet, WLAN_ETHHDR_LEN+pskb->len);
#endif
#endif


#if (!defined(WIFI_HAPD)) && (!defined(RTK_NL80211))
						return FAIL;	// let dsr free this skb
#endif
#endif

					}
				}
#endif

#if !defined(__ECOS) && !defined(TAROKO_0)
				skb_push(pskb, WLAN_ETHHDR_LEN);	// push back due to be pulled by eth_type_trans()
#endif


#ifdef A4_STA
               if((GET_BR_PORT(priv->dev) && priv->pmib->miscEntry.a4_enable == 2 && fdb_find_for_driver(GET_BR_PORT(priv->dev)->br, da) == NULL) || (priv->pmib->miscEntry.a4_enable == 1 && IS_MCAST(da))) {
					pnewskb = skb_copy(pskb, GFP_ATOMIC);
                    if (pnewskb) {
#ifdef GBWC
                        if (GBWC_forward_check(priv, pnewskb, pstat)) {
                            // packet is queued, nothing to do
                        }
                        else
#endif
                        {
#ifdef TX_SCATTER
                            pnewskb->list_num = 0;
#endif                          
                            if (rtl8192cd_start_xmit(pnewskb, priv->dev))
                                rtl_kfree_skb(priv, pnewskb, _SKB_TX_);
                        }
                    }
                }
#endif //A4_STA

				if (do_rc) {
					*(unsigned int *)&(pfrinfo->pskb->cb[4]) = 0;
					if (reorder_ctrl_check(priv, pstat, pfrinfo))
						rtl_netif_rx(priv, pfrinfo->pskb, pstat);
				}
				else
					rtl_netif_rx(priv, pskb, pstat);
			}
			else
			{
				if (priv->pmib->dot11OperationEntry.block_relay == 1) {
					priv->ext_stats.rx_data_drops++;
					DEBUG_ERR("RX DROP: Relay unicast packet is blocked!\n");
					return FAIL;
				}
				else if (priv->pmib->dot11OperationEntry.block_relay == 2) {
					DEBUG_INFO("Relay unicast packet is blocked! Indicate to bridge.\n");
					rtl_netif_rx(priv, pskb, pstat);
				}
#ifdef HS2_SUPPORT
				/* Hotspot 2.0 Release 1 */
				else if (priv->pmib->dot11OperationEntry.block_relay == 3 
                    && (IS_ICMPV4_PROTO(pfrinfo->pskb->data) && IS_ICMPV4_ECHO_TYPE(pfrinfo->pskb->data))
                    && (priv->pmib->hs2Entry.ICMPv4ECHO != 1) ) {
                    
					HS2DEBUG("block_relay=3\n");
		
					if (priv->pmib->hs2Entry.ICMPv4ECHO == 2) {
						//HS2_CHECK
						//memcpy(&pfrinfo->pskb->data[0], priv->pmib->hs2Entry.redir_mac, 6);
						HS2DEBUG("redirect ICMPv4 Packet to portal\n");
						rtl_netif_rx(priv, pskb, pstat);
						return SUCCESS;
					}
					else if (priv->pmib->hs2Entry.ICMPv4ECHO == 0) {
						HS2DEBUG("Drop ICMPv4 Packet\n");
						rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);       
						return SUCCESS;
					}
					return FAIL;
				}
#endif
				else {
					do_process = 0;
#ifdef ENABLE_RTL_SKB_STATS
					rtl_atomic_dec(&priv->rtl_rx_skb_cnt);
#endif
#ifdef GBWC
					if (GBWC_forward_check(priv, pfrinfo->pskb, pstat)) {
						// packet is queued, nothing to do
					}
					else
#endif
					if (do_rc) {
#ifdef CONFIG_RTK_MESH
						if (isMeshPoint(dst_pstat)) {
							pfrinfo->pskb->cb[4] = 1;
							pfrinfo->pskb->cb[3] = XMIT_11S;
						}
						else                  
#endif
							*(unsigned int *)&(pfrinfo->pskb->cb[4]) = (unsigned int)dst_pstat;	// backup pstat pointer

#ifdef ENABLE_RTL_SKB_STATS
						rtl_atomic_inc(&priv->rtl_rx_skb_cnt);
#endif
						if (reorder_ctrl_check(priv, pstat, pfrinfo)) {
#ifdef ENABLE_RTL_SKB_STATS
							rtl_atomic_dec(&priv->rtl_rx_skb_cnt);
#endif
							do_process = 1;
						}
					} else {
						do_process = 1;
					}

					if (do_process) {
#ifdef TX_SCATTER
						pskb->list_num = 0;
#endif
#ifdef CONFIG_RTK_MESH
						if (isMeshPoint(dst_pstat)) {
							if (mesh_start_xmit(pfrinfo->pskb, priv->mesh_dev))
								rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_TX_);
						}
						else
#endif  
						{
							if (rtl8192cd_start_xmit(pskb, priv->dev))
								rtl_kfree_skb(priv, pskb, _SKB_TX_);
						}
					}
				}
			}
		}
	}
#ifdef CLIENT_MODE
	else if (OPMODE & (WIFI_STATION_STATE | WIFI_ADHOC_STATE))
	{
		// I am station, and just report every frame I received to protocol statck

		if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) &&
			priv->pmib->reorderCtrlEntry.ReorderCtrlEnable) {
			if (IS_MCAST(pfrinfo->da))
				do_rc = 0;
		}

		if (OPMODE & WIFI_STATION_STATE)
			pstat = get_stainfo(priv, BSSID);
		else	// Ad-hoc
			pstat = get_stainfo(priv, pfrinfo->sa);

#ifdef A4_STA
		if (priv->pmib->miscEntry.a4_enable && pfrinfo->to_fr_ds == 3) {
			dest = GetAddr1Ptr(pframe);
		}
		else
#endif
			dest = pfrinfo->da;

		rx_sum_up(priv, pstat, pfrinfo);
#ifdef RX_CRC_EXPTIMER
		priv->ext_stats.rx_crc_exptimer++;
#endif
		priv->pshare->NumRxBytesUnicast += pfrinfo->pktlen;
		update_sta_rssi(priv, pstat, pfrinfo);
		priv->rxDataNumInPeriod++;

		if (IS_MCAST(dest)) {
			priv->rxMlcstDataNumInPeriod++;
#ifdef SUPPORT_SNMP_MIB
			SNMP_MIB_INC(dot11MulticastReceivedFrameCount, 1);
#endif
		} else if ((OPMODE & WIFI_STATION_STATE) && (priv->ps_state)) {
			if ((GetFrameSubType(get_pframe(pfrinfo)) == WIFI_DATA)
#ifdef WIFI_WMM
				|| (QOS_ENABLE && pstat->QosEnabled && (GetFrameSubType(get_pframe(pfrinfo)) == WIFI_QOS_DATA))
#endif
			) {
				if (GetMData(rxpkt_info->frame_ctrl)) {
#if defined(WIFI_WMM) && defined(WMM_APSD)
					if (QOS_ENABLE && APSD_ENABLE && priv->uapsd_assoc) {
						if (!((priv->pmib->dot11QosEntry.UAPSD_AC_BE && ((pfrinfo->tid == 0) || (pfrinfo->tid == 3))) ||
							(priv->pmib->dot11QosEntry.UAPSD_AC_BK && ((pfrinfo->tid == 1) || (pfrinfo->tid == 2))) ||
							(priv->pmib->dot11QosEntry.UAPSD_AC_VI && ((pfrinfo->tid == 4) || (pfrinfo->tid == 5))) ||
							(priv->pmib->dot11QosEntry.UAPSD_AC_VO && ((pfrinfo->tid == 6) || (pfrinfo->tid == 7)))))
							issue_PsPoll(priv);
					} else
#endif
					{
						issue_PsPoll(priv);
					}
				}
			}
		}

#ifdef RTK_BR_EXT
#ifdef A4_STA
        if(pstat && !(pstat->state & WIFI_A4_STA))
#endif
        {
    		if(nat25_handle_frame(priv, pskb) == -1) {
    			priv->ext_stats.rx_data_drops++;
    			DEBUG_ERR("RX DROP: nat25_handle_frame fail!\n");
    			return FAIL;
    		}
        }
#endif

#ifdef __KERNEL__
		pskb->protocol = eth_type_trans(pskb, priv->dev);
#endif

#if defined(TAROKO_0)
//TODO, handle eap frame to upper layer
#else


#ifdef WIFI_WPAS_CLI
		if (!(OPMODE & WIFI_STATION_STATE))
#endif
		{
#if (defined(EAP_BY_QUEUE) || defined(INCLUDE_WPA_PSK)) && (!defined(WIFI_HAPD) || defined(HAPD_DRV_PSK_WPS)) &&(!defined(RTK_NL80211))
		if (*(unsigned short *)(SKB_MAC_HEADER(pskb) + MACADDRLEN*2) == __constant_htons(0x888e))
		{
			unsigned short		pkt_len;

#ifdef __ECOS
			pkt_len = pskb->len;
#else
			pkt_len = WLAN_ETHHDR_LEN + pskb->len;
#endif
			priv->Eap_packet->EventId = DOT11_EVENT_EAP_PACKET;
			priv->Eap_packet->IsMoreEvent = FALSE;
			memcpy(&(priv->Eap_packet->packet_len), &pkt_len, sizeof(unsigned short));
#ifdef __ECOS
			memcpy(&priv->Eap_packet->packet, pskb->data, pskb->len);
#else
			memcpy(&(priv->Eap_packet->packet[0]), SKB_MAC_HEADER(pskb), WLAN_ETHHDR_LEN);
			memcpy(&(priv->Eap_packet->packet[WLAN_ETHHDR_LEN]), pskb->data, pskb->len);
#endif
#ifdef EAP_BY_QUEUE

#ifdef INCLUDE_WPS

			wps_NonQueue_indicate_evt(priv ,
				(char *)priv->Eap_packet, sizeof(DOT11_EAP_PACKET));
#else
			DOT11_EnQueue((unsigned long)priv, priv->pevent_queue,
				(unsigned char*)priv->Eap_packet, sizeof(DOT11_EAP_PACKET));
			event_indicate(priv, NULL, -1);
#endif
#endif
#ifdef INCLUDE_WPA_PSK
#ifdef __ECOS
			psk_indicate_evt(priv, DOT11_EVENT_EAP_PACKET, (unsigned char*)&(priv->Eap_packet->packet[6]),
				(unsigned char*)priv->Eap_packet->packet, pskb->len);
#else
			psk_indicate_evt(priv, DOT11_EVENT_EAP_PACKET, (unsigned char*)&(priv->Eap_packet->packet[6]),
				(unsigned char*)priv->Eap_packet->packet, WLAN_ETHHDR_LEN+pskb->len);
#endif
#endif

#if (!defined(WIFI_HAPD)) && (!defined(RTK_NL80211))
			return FAIL;	// let dsr free this skb
#endif
		}
#endif
		} // WIFI_WPAS_CLI
#endif
#ifdef __KERNEL__
		skb_push(pskb, WLAN_ETHHDR_LEN);	// push back due to be pulled by eth_type_trans()
#endif
		if (do_rc) {
			*(unsigned int *)&(pfrinfo->pskb->cb[4]) = 0;
			if (reorder_ctrl_check(priv, pstat, pfrinfo))
				rtl_netif_rx(priv, pskb, pstat);
		}
		else
			rtl_netif_rx(priv, pskb, pstat);
	}
#endif // CLIENT_MODE

	return SUCCESS;
}
#endif // CONFIG_RTL_OFFLOAD_DRIVER



/*---------------------------------------------------------------
	return value:
	0:	no reuse, allocate new skb due to the current is queued.
	1:	reuse! due to error pkt or short pkt.
----------------------------------------------------------------*/
#ifdef __OSK__
__IRAM_WIFI_PRI2
#else
__MIPS16
__IRAM_IN_865X
#endif
int validate_mpdu(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
	unsigned char	*sa, *da, *myhwaddr, *pframe;
	unsigned int	frtype;


	int reuse = 1;
#ifdef MBSSID
	unsigned int opmode = OPMODE;
	int i, vap_idx=-1;
#endif
#if defined(CONFIG_WLAN_HAL_8814BE) && defined(CONFIG_RTL9607C)
#ifdef SMP_SYNC
	unsigned long x;
#endif
#endif

#if defined(__LINUX_2_6__) || defined(__ECOS) || defined(__OSK__)
	if(pfrinfo == NULL || pfrinfo->pskb==NULL || pfrinfo->pskb->data == NULL)
		return 1;
#endif

	pframe = get_pframe(pfrinfo);

#ifdef CONFIG_RTL_OFFLOAD_DRIVER
	if (OFFLOAD_ENABLE(priv)) {
		if (pfrinfo->rxpkt_info->pkt_type == RXPKTINFO_TYPE_8023
				|| pfrinfo->rxpkt_info->pkt_type == RXPKTINFO_TYPE_AMSDU) {
			struct rtl8192cd_priv *priv_intf = priv;
			int ifid = pfrinfo->rxpkt_info->ifid;
			int ifid_offset = 1;
			struct sk_buff *pskb = pfrinfo->pskb;
			
			sa = pskb->data + MACADDRLEN;
			myhwaddr = priv->pmib->dot11OperationEntry.hwaddr;
			if (isEqualMACAddr(myhwaddr, sa))
				return 1;

			if (ifid > 0) {
#ifdef UNIVERSAL_REPEATER
				ifid_offset = 2;
				if (ifid == 1)
					priv_intf = priv->pvxd_priv;
#endif
#ifdef MBSSID
				if (ifid >= ifid_offset)
					priv_intf = priv->pvap_priv[ifid - ifid_offset];
#endif
			}
			
			if (!IS_DRV_OPEN(priv_intf)) {
				DEBUG_ERR("%s: intf is not open, if_id: %d, priv: %p\n", __FUNCTION__, ifid, priv_intf);
				return 1;
			}
			
#if defined(CONFIG_WLAN_HAL_8814BE) && defined(CONFIG_RTL9607C)
			SMP_LOCK_RX_DATA(x);
			list_add_tail(&(pfrinfo->rx_list), &(priv_intf->rx_datalist));
			SMP_UNLOCK_RX_DATA(x);
			rtl_atomic_inc(&priv_intf->pshare->rx_enqueue_cnt);
			return 0;
#else	
			if (rtl8192cd_rx_dispatch_offload(priv_intf, pfrinfo) == FAIL)
				return 1; /* reuse */
			else
				return 0;
#endif	
		}
	}
#endif // CONFIG_RTL_OFFLOAD_DRIVER

#if 1   /*Filen_Debug*/
    if ((unsigned long)pframe & BIT0) {
        panic_printk("Error: pframe(0x%lx), pfrinfo->pskb(0x%lx), pfrinfo(0x%lx)\n", (unsigned long)pframe, (unsigned long)pfrinfo->pskb, (unsigned long)pfrinfo);
        panic_printk("Error: rxbuf_shift(0x%x), driver_info_size(0x%x)\n", pfrinfo->rxbuf_shift, pfrinfo->driver_info_size);
        return 1;
    }
#endif

#if 0	/* already flush cache in rtl8192cd_rx_isr()*/
#ifndef RTL8190_CACHABLE_CLUSTER
#ifdef __MIPSEB__
	pframe = (UINT8 *)((unsigned long)pframe | 0x20000000);
#endif
#endif
#endif

#ifdef DRVMAC_LB
	if (priv->pmib->miscEntry.drvmac_lb)
		lb_convert(priv, pframe);
#endif

	pfrinfo->hdr_len =  get_hdrlen(priv, pframe);
	if (pfrinfo->hdr_len == 0)	// unallowed packet type
	{
//		printk("pfrinfo->hdr_len == 0\n");
		return 1;
	}

#ifdef CONFIG_8814_AP_MAC_VERI  
    RX_MAC_Verify_8814(priv,pframe,pfrinfo);
#endif //#ifdef CONFIG_8814_AP_MAC_VERI

	frtype = GetFrameType(pframe);
	pfrinfo->to_fr_ds = get_tofr_ds(pframe);
	pfrinfo->da	= da = 	get_da(pframe);
	pfrinfo->sa	= sa =  get_sa(pframe);
	pfrinfo->retry = GetRetry(pframe);
	pfrinfo->amsdu = is_qos_data(pframe) && (*GetQosControl(pframe) & BIT7);
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
	if (OFFLOAD_ENABLE(priv))
		; // already got from RxDESC
	else
#endif // CONFIG_RTL_OFFLOAD_DRIVER
		pfrinfo->seq = GetSequence(pframe);

#ifdef TRX_DATA_LOG
	pfrinfo->bdata = (frtype == WIFI_DATA_TYPE);
#endif
#if defined(CONFIG_RTL8672) || defined(CONFIG_WLAN_STATS_EXTENTION)
	pfrinfo->bcast = IS_BCAST2(da);
	pfrinfo->mcast = IS_MCAST(da);
#endif

#if defined(SUPPORT_MONITOR) && !defined(__ECOS)
		if(priv->is_monitor_mode == TRUE) {
			if (frtype == WIFI_MGT_TYPE || frtype == WIFI_DATA_TYPE) {
				unsigned int sub_frame_type = GetFrameSubType(pframe);
				unsigned char chan = getPktChanInfo(priv, pfrinfo, sub_frame_type);
	#ifdef CONFIG_PKT_FILTER
				if(sub_frame_type == WIFI_BEACON || sub_frame_type == WIFI_PROBEREQ || sub_frame_type == WIFI_PROBERSP) { //only 2.4g has dsss para set ie
					if(!chan && (chan != priv->pmib->dot11RFEntry.dot11channel))
						return reuse = 1;
				}
	#endif
				struct sk_buff* pskb = pfrinfo->pskb;
				rtl_ieee80211_radiotap_header(priv, pfrinfo, frtype, sub_frame_type, chan);
				pskb->dev = priv->dev;
				skb_reset_mac_header(pskb);
				pskb->ip_summed = CHECKSUM_UNNECESSARY;
				pskb->pkt_type = PACKET_OTHERHOST;
				pskb->protocol = htons(ETH_P_802_2);
				netif_receive_skb(pskb);
				return reuse = 0;
			} else 
				return reuse = 1;
		}
#endif

#ifdef CONFIG_RTK_MESH
	pfrinfo->is_11s = 0;

	// WIFI_11S_MESH_ACTION use QoS bit, but it's not a QoS data (8186 compatible)
	if (is_qos_data(pframe) && GetFrameSubType(pframe) != WIFI_11S_MESH_ACTION)
#else
	if (is_qos_data(pframe))
#endif
	{
		pfrinfo->tid = GetTid(pframe) & 0x07;	// we only process TC of 0~7
	}
	else {
		pfrinfo->tid = 0;
	}
#ifdef MULTI_MAC_CLONE
	if (MCLONE_NUM > 0 && !IS_MCAST(GetAddr1Ptr(pframe))) {
		int id;
		id = mclone_find_address(priv, GetAddr1Ptr(pframe), pfrinfo->pskb, MAC_CLONE_DA_FIND);
		if (id > 0)
			ACTIVE_ID = id;
		else
			ACTIVE_ID = 0;
	} else
		ACTIVE_ID = 0;
	myhwaddr = GET_MY_HWADDR;
#else
	myhwaddr = priv->pmib->dot11OperationEntry.hwaddr;
#endif

#ifdef	PCIE_POWER_SAVING
	if (!memcmp(myhwaddr, GetAddr1Ptr(pframe), MACADDRLEN))
		mod_timer(&priv->ps_timer, jiffies + POWER_DOWN_T0);
#endif

	// filter packets that SA is myself
#ifdef MULTI_MAC_CLONE
	if ((OPMODE & WIFI_STATION_STATE) && IS_MCAST(GetAddr1Ptr(pframe)) && 
		priv->pmib->ethBrExtInfo.macclone_enable && (mclone_find_address(priv, sa, NULL, MAC_CLONE_NOCARE_FIND) >= 0)) {
		return 1;	
	}
	else if ((OPMODE & WIFI_AP_STATE) && IS_MCAST(GetAddr1Ptr(pframe)) && 
		/*priv->pmib->ethBrExtInfo.macclone_enable &&*/ (mclone_find_address(priv, sa, NULL, MAC_CLONE_MSA_FIND) >= 0)) {
		return 1;
	}
	else
#endif
	if (isEqualMACAddr(myhwaddr, sa)
#ifdef CONFIG_RTK_MESH
        // for e.g.,  MPP1 - THIS_DEVICE - ROOT ;
        //            THIS_DEVICE use ROOT first, and then ROOT dispatch the packet to MPP1
		&& (priv->pmib->dot1180211sInfo.mesh_enable ? ((pfrinfo->to_fr_ds != 0x03) && GetFrameSubType(pframe) != WIFI_11S_MESH) : TRUE)
#endif
	)
		return 1;
#ifdef MULTI_MAC_CLONE
	else if ((ACTIVE_ID == 0) && isEqualMACAddr(GET_VXD_PRIV(priv)->pmib->dot11OperationEntry.hwaddr, sa))
		return 1;
#endif

#ifdef MBSSID
	if (GET_ROOT(priv)->pmib->miscEntry.vap_enable) {
		for (i=0; i<RTL8192CD_NUM_VWLAN; i++) {
			if (IS_DRV_OPEN(priv->pvap_priv[i]))
			{
				unsigned char *addr = priv->pvap_priv[i]->pmib->dot11OperationEntry.hwaddr;
				unsigned char *addr2 = priv->pvap_priv[i]->pmib->dot11StationConfigEntry.dot11Bssid;
				
				if ( (((pfrinfo->to_fr_ds == 0x00) || (pfrinfo->to_fr_ds == 0x02) || (pfrinfo->to_fr_ds == 0x03)) && isEqualMACAddr(addr, GetAddr1Ptr(pframe)))
				  || (((pfrinfo->to_fr_ds == 0x00) || (pfrinfo->to_fr_ds == 0x01)) && isEqualMACAddr(addr2, GetAddr2Ptr(pframe))) ) {
					vap_idx = i;
					if ((priv->pvap_priv[i]->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE) && (pfrinfo->to_fr_ds == 0x01) && isEqualMACAddr(addr, sa))
						return 1;
					break;
				}
			}
		}
	}
#endif


// filter packets that DA is not myself
#ifdef BSS_MONITOR_SUPPORT
if(priv->pshare->rf_ft_var.monitor_enable)
{
	int Addr_match=0;
	if(IS_MCAST(GetAddr1Ptr(pframe)) || isEqualMACAddr(GET_MY_HWADDR, GetAddr1Ptr(pframe))) {
		Addr_match = 1;
	} else {
#ifdef MBSSID
	if (GET_ROOT(priv)->pmib->miscEntry.vap_enable) {
		for (i=0; i<RTL8192CD_NUM_VWLAN; i++) {
			if (IS_DRV_OPEN(priv->pvap_priv[i])){
				if(isEqualMACAddr(priv->pvap_priv[i]->pmib->dot11OperationEntry.hwaddr, GetAddr1Ptr(pframe))) {
					Addr_match = 1;
					break;
				}
			}
		}
	}
#endif
#ifdef UNIVERSAL_REPEATER
#endif		
#ifdef MULTI_MAC_CLONE
	if (Addr_match==0 && MCLONE_NUM > 0) {
		int i;
		for (i=0; i<priv->pshare->mclone_num_max; i++) {
			if ( isEqualMACAddr(GetAddr1Ptr(pframe), priv->pshare->mclone_sta[i].hwaddr)) {
				Addr_match = 1;
				break;
			}
		}
	}				
#endif
	}
	collect_monitor_info(priv, pfrinfo);
	if(Addr_match ==0) 
		return 1;	
}
#endif

#if defined(CONFIG_RTL_MONITOR_STA_INFO)
	if(priv->pmib->dot11StationConfigEntry.monitor_sta_enabled)
		rtk_monitor_sta_info(priv, pfrinfo);
#endif

#if defined(RTK_SMART_ROAMING)  || defined(RTK_MULTI_AP)
	if (pfrinfo->rf_info.rssi && memcmp(GET_MY_HWADDR, GetAddr1Ptr(pframe), MACADDRLEN)) {
		if (pfrinfo->to_fr_ds == 0x02) {
#ifdef RTK_MULTI_AP
			if(priv->multiap_unassocStaEntryOccupied)
				update_unassoc_MAC_RSSI_Entry(priv, GetAddr2Ptr(pframe), pfrinfo->rf_info.rssi, 0, priv->multiap_unassoc_sta);
#else
			add_neighbor_unicast_sta(priv, GetAddr2Ptr(pframe), pfrinfo->rf_info.rssi);
#endif
		}

		if (!IS_MCAST(GetAddr1Ptr(pframe))) {
#if (defined(MULTI_MAC_CLONE) || defined(VXD_DIFF_MAC)) && defined(UNIVERSAL_REPEATER)
			struct rtl8192cd_priv *vxd_priv = GET_VXD_PRIV(priv);					
#endif

			#ifdef MBSSID
			if(GET_ROOT(priv)->pmib->miscEntry.vap_enable) {
				if(vap_idx == -1){
#if (defined(MULTI_MAC_CLONE) || defined(VXD_DIFF_MAC)) && defined(UNIVERSAL_REPEATER)
					if (!IS_DRV_OPEN(vxd_priv) || memcmp((GET_MIB(vxd_priv))->dot11OperationEntry.hwaddr, GetAddr1Ptr(pframe), MACADDRLEN))
#endif
					return 1;
				}
			}
			else
			#endif
			{
#if (defined(MULTI_MAC_CLONE) || defined(VXD_DIFF_MAC)) && defined(UNIVERSAL_REPEATER)
				if (!IS_DRV_OPEN(vxd_priv) || memcmp((GET_MIB(vxd_priv))->dot11OperationEntry.hwaddr, GetAddr1Ptr(pframe), MACADDRLEN))
#endif
				return 1;
			}
		}
	}	
#endif

#if 0
	// check power save state
	if ((opmode & WIFI_AP_STATE)
#ifdef UNIVERSAL_REPEATER
		|| ((opmode & WIFI_STATION_STATE) && IS_DRV_OPEN(GET_VXD_PRIV(priv)))
#endif
		)
	{
		bssid = GetAddr1Ptr(pframe);

#ifdef MBSSID
		if (vap_idx >= 0) {
			if (IS_BSSID(priv->pvap_priv[vap_idx], bssid))
				pwr_state(priv->pvap_priv[vap_idx], pfrinfo);
		}
		else
#endif
#ifdef UNIVERSAL_REPEATER
		if (opmode & WIFI_AP_STATE) {
			if (IS_BSSID(priv, bssid))
				pwr_state(priv, pfrinfo);
		}
		else {
			if (IS_BSSID(GET_VXD_PRIV(priv), bssid))
				pwr_state(GET_VXD_PRIV(priv), pfrinfo);
		}
#else
		if (IS_BSSID(priv, bssid))
			pwr_state(priv, pfrinfo);
#endif
	}
#endif

	// if receiving control frames, we just handle PS-Poll only
	if (frtype == WIFI_CTRL_TYPE)
	{
#if defined(TAROKO_0)
//TODO SKIP power saving 
		return 1;
#else	
		return rtl8192cd_rx_procCtrlPkt(priv, pfrinfo
#ifdef MBSSID
									  ,vap_idx
#endif
									  );
#endif
	}

	// for ap, we have handled; for station/ad-hoc, we reject null_data frame
	if (((GetFrameSubType(pframe)) == WIFI_DATA_NULL) ||
		(((GetFrameSubType(pframe)) == WIFI_DATA) && pfrinfo->pktlen <= 24)
#ifdef WIFI_WMM
		||((QOS_ENABLE) && ((GetFrameSubType(pframe)) == (WIFI_DATA_NULL | BIT(7))))
#endif
	)
	{
#if defined(TAROKO_0)
		//TODO SKIP power saving 
		return 1;
#else			
		return rtl8192cd_rx_procNullPkt(priv, pfrinfo
#ifdef MBSSID
									  ,vap_idx
#endif
									  );
#endif
	}

	// david, move rx statistics from rtl8192cd_rx_isr() to here because repeater issue
	//qinjunjie:move rx_sum_up to rtl8192cd_rx_dispatch_mgmt_adhoc/rtl8192cd_rx_dispatch_fromDs/rtl8192cd_rx_dispatch_toDs/rtl8192cd_rx_dispatch_wds
	//	rx_sum_up(priv, NULL, pfrinfo->pktlen, GetRetry(pframe));

	// enqueue data frames and management frames
#if defined(CONFIG_RTL_SIMPLE_CONFIG)
		if((priv->pmib->dot11StationConfigEntry.sc_enabled == 1) && (priv->simple_config_status !=0) && IS_MCAST(pfrinfo->da))
		{
			rtk_sc_start_parse_packet(priv, pfrinfo);
			if (priv->pmib->dot11StationConfigEntry.sc_status <=4) {
				if (get_sc_smac(pfrinfo) == 1) {
					return 1;
				}
			}
		}
#ifdef UNIVERSAL_REPEATER
		if((GET_VXD_PRIV(priv)->pmib->dot11StationConfigEntry.sc_enabled == 1) && (GET_VXD_PRIV(priv)->simple_config_status !=0) && IS_MCAST(pfrinfo->da))
		{
				rtk_sc_start_parse_packet(GET_VXD_PRIV(priv), pfrinfo);
				if (priv->pmib->dot11StationConfigEntry.sc_status <=4) {
					if (get_sc_smac(pfrinfo) == 1) {
						return 1;
					}
				}
		}
#endif
#endif

#ifdef CONFIG_ADAPTIVE_SOML
    if (frtype == WIFI_DATA_TYPE)
    {
		//panic_printk(" rate %d, len %d\n", pfrinfo->rx_rate, pfrinfo->pktlen);

		int rate = 0;    
		if (pfrinfo->rx_rate < 12)
		{
			rate = pfrinfo->rx_rate;
		}
		else if (pfrinfo->rx_rate >= 0xA0)
		{
			rate = pfrinfo->rx_rate - 0xA0 + 12 + 32;
		}
		else if (pfrinfo->rx_rate >= 0x80)
		{
			rate = pfrinfo->rx_rate - 0x80 + 12;
		}
	
		phydm_soml_bytes_acq(ODMPTR, rate, pfrinfo->pktlen);
    }
#endif

    switch (pfrinfo->to_fr_ds)
    {
        case 0x00:	// ToDs=0, FromDs=0
			reuse = rtl8192cd_rx_dispatch_mgmt_adhoc(&priv, pfrinfo
                #ifdef MBSSID
                ,vap_idx
                #endif
                );
  		
            break;

        case 0x01:	// ToDs=0, FromDs=1
            #if defined(CLIENT_MODE) || defined(CONFIG_RTK_MESH)
            reuse = rtl8192cd_rx_dispatch_fromDs(&priv, pfrinfo
                #ifdef MBSSID
                ,vap_idx
                #endif
                );
            #endif
            break;

        case 0x02:	// ToDs=1, FromDs=0
            reuse = rtl8192cd_rx_dispatch_toDs(&priv, pfrinfo
                #ifdef MBSSID
                ,vap_idx
                #endif
                );
            break;

        case 0x03:	// ToDs=1, FromDs=1
            #ifdef CONFIG_RTK_MESH
            if( 1 == GET_MIB(priv)->dot1180211sInfo.mesh_enable)
            {
                reuse = rx_dispatch_mesh(priv, pfrinfo);
            } else
            #endif

            #ifdef A4_STA
            if (priv->pmib->miscEntry.a4_enable) {
                reuse = a4_rx_dispatch(priv, pfrinfo
                #ifdef MBSSID
                    ,vap_idx
                    #endif
                    );
            } else
            #endif

            #ifdef WDS
            if (priv->pmib->dot11WdsInfo.wdsEnabled) {
                reuse = rtl8192cd_rx_dispatch_wds(priv, pfrinfo);
            } else
            #endif
            {;}
            break;
    }

    return reuse;
}

#ifdef __OSK__
__IRAM_WIFI_PRI3
#else
#if !defined(__LINUX_2_6__) && !defined(__ECOS) 
__MIPS16
#endif
__IRAM_IN_865X
#endif
void rtl88XX_ipc_rx_isr(struct rtl8192cd_priv *priv, struct sk_buff *pskb)
{
			struct rtl8192cd_hw	*phw;
			struct rx_frinfo	*pfrinfo;
			unsigned int		reuse;
			unsigned int		MPreuse;
			unsigned int		rtl8192cd_ICV, privacy;
			struct stat_info	*pstat = NULL;
#if (defined(__ECOS) || defined(__LINUX_2_6__)) && defined(RX_TASKLET)
			unsigned long		x;
#if defined(SMP_SYNC)
			unsigned long		y = 0;
#endif
#endif

			u2Byte			rxbdOkCnt = 0;

#if defined(CONFIG_RTL8190_PRIV_SKB)
#ifdef CONCURRENT_MODE
			extern int		skb_free_num[];
#else
			extern int		skb_free_num;
#endif
#endif
#if defined(RX_BUFFER_GATHER)
			int			pfrinfo_update;
#endif
			unsigned int			q_num;
			PHCI_RX_DMA_MANAGER_88XX	prx_dma;
			PHCI_RX_DMA_QUEUE_STRUCT_88XX	cur_q;
			RX_DESC_STATUS_88XX		rx_desc_status;
		
#if 1/*(defined (CONFIG_RTK_VOIP_QOS) && !defined (CONFIG_RTK_VOIP_ETHERNET_DSP_IS_HOST)) || defined (CHK_RX_ISR_TAKES_TOO_LONG)*/
			unsigned long			start_time = jiffies;
			int				n = 0;
#endif
			int				update = 0;
			u4Byte				skb_shift_size = 0;
			u4Byte				skb_used_size;
		
			void *init_rxdesc_callback;
			int cur_host_idx, hw_idx;
		
		
			if (!(priv->drv_state & DRV_STATE_OPEN))
				return;

			SMP_LOCK_RECV(y);
		
			phw = GET_HW(priv);
		
			RTL_WLAN_RX_ATOMIC_PROTECT_ENTER;
		
			// Currently, only one queue for rx...
			q_num	= 0;	
		
					
				pfrinfo 	= get_pfrinfo(pskb);
				reuse		= 1;
				MPreuse 	= 1;
				skb_shift_size	= 0;
		
				memset(&rx_desc_status, 0, sizeof(struct _RX_DESC_STATUS_88XX_));

				if (RT_STATUS_SUCCESS != GET_HAL_INTERFACE(priv)->QueryRxDescHandler(priv, q_num, pskb->data, &rx_desc_status)) {

#if defined(CONFIG_NET_PCI) && !defined(USE_RTL8186_SDK)
					if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
						if (IS_PCIBIOS_TYPE) {
							pci_unmap_single(priv->pshare->pdev, phw->rx_infoL[cur_host_idx].paddr, (RX_BUF_LEN - sizeof(struct rx_frinfo)), PCI_DMA_FROMDEVICE);
						}
					}
#endif
					pfrinfo = NULL;
					priv->ext_stats.rx_reuse++;
#ifdef __ECOS
					panic_printk("QueryRxDesc:%s(%d), reuse1(0x%x)\n", __FUNCTION__, __LINE__, (unsigned int)priv->ext_stats.rx_reuse);
#else
					printk("%s(%d), reuse(0x%x)\n", __FUNCTION__, __LINE__, (unsigned int)priv->ext_stats.rx_reuse);
#endif
					goto rx_reuse;
				}


		
				if (rx_desc_status.CRC32) {
					priv->pshare->HW_CRC_error_cnt++;
					if (rx_desc_status.FCS_OK) {
						priv->pshare->HW_CRC_err_FCS_ok_cnt++;
						rx_desc_status.CRC32 = 0;
					}
				}
		
				if (rx_desc_status.CRC32) {
					rx_pkt_exception_88XX(priv, rx_desc_status.CRC32, rx_desc_status.ICVERR, rx_desc_status.RX_RATE);
					goto rx_reuse;
				}
		
#if !defined(RX_BUFFER_GATHER)
				else if (!(rx_desc_status.FS == 0x01 && rx_desc_status.LS == 0x01)) {
					// h/w use more than 1 rx descriptor to receive a packet
					// that means this packet is too large
					// drop such kind of packet
					priv->ext_stats.rx_reuse++;
					printk("%s(%d), reuse(0x%x)\n", __FUNCTION__, __LINE__, (unsigned int)priv->ext_stats.rx_reuse);
					goto rx_reuse;
				}
#endif // !defined(RX_BUFFER_GATHER)
				else if (!IS_DRV_OPEN(priv)) {
					priv->ext_stats.rx_reuse++;
					printk("%s(%d), reuse(0x%x)\n", __FUNCTION__, __LINE__, (unsigned int)priv->ext_stats.rx_reuse);
					goto rx_reuse;
				}
				else {
					pfrinfo->pskb = pskb;
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
					if (OFFLOAD_ENABLE(priv))
						pfrinfo->pktlen = rx_desc_status.PKT_LEN;
					else
#endif // CONFIG_RTL_OFFLOAD_DRIVER
						if (rx_desc_status.C2HPkt)
							pfrinfo->pktlen = rx_desc_status.PKT_LEN;// If is C2H packet, there is no CRC length Kevin for txbf
						else
							pfrinfo->pktlen = rx_desc_status.PKT_LEN - _CRCLNG_;
		
					{
						pfrinfo->driver_info_size = rx_desc_status.DRV_INFO_SIZE;
						pfrinfo->rxbuf_shift = rx_desc_status.SHIFT;
					}

					pfrinfo->sw_dec = rx_desc_status.SWDEC;

					
					if (CheckRxPktLenReuse(priv, pfrinfo, &rx_desc_status)) {
						//printk("pktlen out of range, pfrinfo->pktlen=0x%x, goto rx_reuse\n", pfrinfo->pktlen);
						priv->ext_stats.rx_reuse++;
						//printk("%s(%d), reuse(0x%x)\n", __FUNCTION__, __LINE__, (unsigned int)priv->ext_stats.rx_reuse);
						goto rx_reuse;
					}

#if defined(CONFIG_RTL865X_CMO)
					if (pfrinfo->pskb == NULL) {
						pfrinfo->pskb = pskb;
						priv->ext_stats.rx_reuse++;
						printk("%s(%d), reuse(0x%x)\n", __FUNCTION__, __LINE__, (unsigned int)priv->ext_stats.rx_reuse);
						goto rx_reuse;
					}
#endif

#if defined(RX_BUFFER_GATHER)
            if (priv->pshare->gather_state != GATHER_STATE_MIDDLE && priv->pshare->gather_state != GATHER_STATE_LAST)
#endif
            {
                //3 Shift SKB pointer before we use skb->data
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
				if (OFFLOAD_ENABLE(priv)) {
					int buf_len = ((*(u32 *)&pskb->data[0]) & 0x3fff) + ((*(u32 *)&pskb->data[4]) & 0x3fff);
					//mem_dump("pskb", pskb->data, min(buf_len, 128)); /* FIXME: for debug */

					/* skip rx_dma_info */
	    			pfrinfo->rxdma_info = (struct d2h_rxdma_info *)get_pframe(pfrinfo);
	                skb_reserve(pskb, sizeof(struct d2h_rxdma_info));
	                skb_shift_size += sizeof(struct d2h_rxdma_info);
	                
					/* skip rx_pkt_info */
					pfrinfo->rxpkt_info = (struct d2h_rxpkt_info *)get_pframe(pfrinfo);
	                skb_reserve(pskb, sizeof(struct d2h_rxpkt_info));
	                skb_shift_size += sizeof(struct d2h_rxpkt_info);

	                skb_reserve(pskb, pfrinfo->rxpkt_info->dummy_len);
	                skb_shift_size += pfrinfo->rxpkt_info->dummy_len;
                }
#endif // CONFIG_RTL_OFFLOAD_DRIVER
                skb_reserve(pskb, sizeof(RX_DESC_88XX));
                skb_shift_size += sizeof(RX_DESC_88XX);
            }

#ifdef CONFIG_RTL_OFFLOAD_DRIVER
			if (OFFLOAD_ENABLE(priv)) {
				if ((pfrinfo->rxpkt_info->pkt_type == RXPKTINFO_TYPE_80211_PROTO) ||
					(pfrinfo->rxpkt_info->pkt_type == RXPKTINFO_TYPE_80211)) {
					pfrinfo->pktlen -= _CRCLNG_;
				}

				if (pfrinfo->rxpkt_info->pkt_type == RXPKTINFO_TYPE_D2H) {
					pfrinfo->pktlen -= skb_shift_size;
					GET_HAL_INTERFACE(priv)->D2HPacketHandler(priv, get_pframe(pfrinfo), pfrinfo->pktlen);

					pskb->data -= skb_shift_size;
					skb_reset_tail_pointer(pskb);
					goto rx_reuse;
				}
			}
#endif

            //3 1.) Check if C2H packet is received
            if (rx_desc_status.C2HPkt) {

                validate_C2HPkt(priv, pfrinfo);

                pskb->data -= skb_shift_size; // skb_shift_size == sizeof(RX_DESC_88XX)
                skb_reset_tail_pointer(pskb); // skb_shift_size == sizeof(RX_DESC_88XX)
                goto rx_reuse;
            }
#if defined(AP_SWPS_OFFLOAD)
            if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv)) {
                if(rx_desc_status.SWPS_RPT){
					//GDEBUG("rcv swps pkt,report_size = %x,NUM_STAT=%d\n",rx_desc_status.PKT_LEN,NUM_STAT);
                    if((rx_desc_status.FS == 0x00 && rx_desc_status.LS == 0x01) || (rx_desc_status.FS == 0x00 && rx_desc_status.LS == 0x00)){
						  #if 1    
                        pu1Byte	pBuf = get_pframe(pfrinfo);
                        u4Byte dump_i=0;
                        printk("dump rpt:");
                        //for(dump_i=0;dump_i<2138;dump_i++)
                        for(dump_i=0;dump_i<rx_desc_status.PKT_LEN;dump_i++)
                        {
                        if(dump_i%16==0)
                            printk("\n");
                        printk("%x ",*(pBuf+dump_i));

                        }
                        printk("\n");
                        #endif

					}else{
						SWPS_RPT_Handler(priv, pfrinfo);
					}	
					
                    pskb->data -= skb_shift_size; // skb_shift_size == sizeof(RX_DESC_88XX)
                    pskb->tail -= skb_shift_size; // skb_shift_size == sizeof(RX_DESC_88XX)
                    goto rx_reuse;
                }
            }
#elif defined(AP_SWPS_OFFLOAD_WITH_AMPDU_VERI)
            if(rx_desc_status.SWPS_RPT){
					//printk("rcv swps pkt,report_size = %x,NUM_STAT=%d\n",rx_desc_status.PKT_LEN,NUM_STAT);
                    if((rx_desc_status.FS == 0x00 && rx_desc_status.LS == 0x01) || (rx_desc_status.FS == 0x00 && rx_desc_status.LS == 0x00)){
						  #if 1    
                        pu1Byte	pBuf = get_pframe(pfrinfo);
                        u4Byte dump_i=0;
                        printk("dump rpt:");
                        //for(dump_i=0;dump_i<2138;dump_i++)
                        for(dump_i=0;dump_i<rx_desc_status.PKT_LEN;dump_i++)
                        {
                        if(dump_i%16==0)
                            printk("\n");
                        printk("%x ",*(pBuf+dump_i));

                        }
                        printk("\n");
                        #endif

					}else{
						SWPS_RPT_AMPDU_VERI_Handler(priv, pfrinfo);
					}	
					
                    pskb->data -= skb_shift_size; // skb_shift_size == sizeof(RX_DESC_88XX)
                    pskb->tail -= skb_shift_size; // skb_shift_size == sizeof(RX_DESC_88XX)
                    goto rx_reuse;
                }
#endif    

#if defined(RX_BUFFER_GATHER)
            if (TRUE == rx_desc_status.PHYST && ((priv->pshare->gather_state == GATHER_STATE_FIRST) || (priv->pshare->gather_state == GATHER_STATE_NO)))
#else
            if (TRUE == rx_desc_status.PHYST)
#endif
            {
    			pfrinfo->driver_info = (struct RxFWInfo *)(get_pframe(pfrinfo));
            }
            else {
                //printk("%s(%d): driver_info pointer error\n", __FUNCTION__, __LINE__);
            }

            pfrinfo->physt    = rx_desc_status.PHYST;
			pfrinfo->paggr    = rx_desc_status.PAGGR;
            pfrinfo->faggr    = 0;
            pfrinfo->rx_splcp = 0;
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
			if (OFFLOAD_ENABLE(priv)) {
	            pfrinfo->tid      = rx_desc_status.TID;
	            pfrinfo->seq      = rx_desc_status.SEQ;
	            pfrinfo->frag     = rx_desc_status.FRAG;
	            pfrinfo->tpcache  = (pfrinfo->seq << 4) | (pfrinfo->frag & 0xf);
                pfrinfo->amsdu_cut = rx_desc_status.AMSDU_CUT;
			    pfrinfo->last_msdu = rx_desc_status.LAST_MSDU;
			    pfrinfo->a1_match = rx_desc_status.A1_MATCH;
			}
#endif // CONFIG_RTL_OFFLOAD_DRIVER
#ifdef HW_FILL_MACID
	if (IS_SUPPORT_HW_FILL_MACID(priv)) {
            pfrinfo->macid    = rx_desc_status.rxMACID;
	}
#endif //HW_FILL_MACID
			pfrinfo->rx_bw	  = 0;

            if(priv->pmib->dot11BssType.net_work_type & WIRELESS_11AC) {
                if (rx_desc_status.RX_RATE < 12) {
                    pfrinfo->rx_rate = dot11_rate_table[rx_desc_status.RX_RATE];
                } else if(rx_desc_status.RX_RATE < 44) {
                    pfrinfo->rx_rate = HT_RATE_ID + (rx_desc_status.RX_RATE - 12);
                } else{
                    pfrinfo->rx_rate = VHT_RATE_ID + (rx_desc_status.RX_RATE - 44);
                }
            } else if (rx_desc_status.RX_RATE < 12) {
                pfrinfo->rx_rate = dot11_rate_table[rx_desc_status.RX_RATE];
            } else {
                pfrinfo->rx_rate = HT_RATE_ID + (rx_desc_status.RX_RATE - 12);
            }

			if (!pfrinfo->physt) {
				pfrinfo->rf_info.rssi = 0;
			} else {
#if defined(RX_BUFFER_GATHER)
				if (pfrinfo->driver_info_size > 0)
#endif
				{

#ifdef USE_OUT_SRC
#ifdef _OUTSRC_COEXIST
					if(IS_OUTSRC_CHIP(priv))
#endif
					{
#ifdef CONFIG_PE_ENABLE 						
                        if(!priv->pshare->rf_ft_var.manual_pe_enable)
#endif
						translate_rssi_sq_outsrc(priv, pfrinfo, rx_desc_status.RX_RATE);
	
#ifdef CONFIG_WLAN_HAL_8192EE
							//translate_CRC32_outsrc(priv, pfrinfo, rx_desc_status.CRC32);
						if(!rx_desc_status.C2HPkt)
						{
							translate_CRC32_outsrc(priv, pfrinfo, rx_desc_status.CRC32,rx_desc_status.PKT_LEN);
						}
#endif
					
					}
#endif
				
#if !defined(USE_OUT_SRC) || defined(_OUTSRC_COEXIST)
#ifdef _OUTSRC_COEXIST
					if(!IS_OUTSRC_CHIP(priv))
#endif
					{
						translate_rssi_sq(priv, pfrinfo);
					}
#endif
		
						}
					}

					/*-----------------------------------------------------
					 validate_mpdu will check if we still can reuse the skb
					------------------------------------------------------*/
#ifndef __ECOS
#if (defined(CONFIG_RTL_QOS_PATCH) || defined(CONFIG_RTK_VOIP_QOS) || defined(CONFIG_RTL_CUSTOM_PASSTHRU)|| defined(CONFIG_RTK_VLAN_WAN_TAG_SUPPORT) || defined(CONFIG_RTL_MULTI_LAN_DEV) || defined(CONFIG_RTL_EN_UNKNOWNUC_ONLY_TO_CPU) || defined(CONFIG_RTL_NIC_QUEUE)) && (!(defined(CONFIG_RTL8672) && defined(__LINUX_3_18__))) && (!defined(NOT_RTK_BSP))
	#ifndef CONFIG_OPENWRT_SDK
					pskb->srcPhyPort = 7;  //QOS_PATCH_RX_FROM_WIRELESS
	#endif
#endif
#endif
		
					{
		
						// 64 is the value in init_rxdesc_88XX: *pBufLen = RX_BUF_LEN - sizeof(struct rx_frinfo) - 64;
						// Actually, the precise value is the offset, but the value is 64 in old version code (rtl8192cd).

						skb_used_size = 64 + sizeof(struct rx_frinfo) + SIZE_RXDESC_88XX +
								pfrinfo->driver_info_size + pfrinfo->rxbuf_shift + pfrinfo->pktlen + _CRCLNG_;

		
						if (skb_used_size <= RX_BUF_LEN) {
							skb_reserve(pskb, (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size));
							skb_shift_size += (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size);
		
							// 8822B igoore AES ICV Error when TxBF MU mapping exist
		
							if (rx_desc_status.ICVERR) {
								rx_pkt_exception_88XX(priv, rx_desc_status.CRC32, rx_desc_status.ICVERR, rx_desc_status.RX_RATE);
								pskb->data -= skb_shift_size; // skb_shift_size is (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size + sizeof(RX_DESC_88XX))
								skb_reset_tail_pointer(pskb); // skb_shift_size is (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size + sizeof(RX_DESC_88XX))
								priv->ext_stats.rx_reuse++;
								//printk("%s(%d), reuse(0x%x)\n", __FUNCTION__, __LINE__, (unsigned int)priv->ext_stats.rx_reuse);
								goto rx_reuse;
							}
		
							SNMP_MIB_INC(dot11ReceivedFragmentCount, 1);
		
		
							{	
								reuse = validate_mpdu(priv, pfrinfo);
							}

							if (reuse) {
								priv->ext_stats.rx_reuse++;

								rtl_kfree_skb(priv, pskb, _SKB_RX_);
								reuse = 0;
							} else {
								//free skb by upper layer, i.e., validate_mpdu
								//So, we don't need to control skb data/tail pointer
							}
						} else {
							pskb->data -= skb_shift_size; // skb_shift_size is sizeof(RX_DESC_88XX)
							skb_reset_tail_pointer(pskb); // skb_shift_size is sizeof(RX_DESC_88XX)
						}
					}
				}	/* if (cmd&XXXX) */
	
			if (!reuse) {
				goto rx_done;
			}	/* if (!reuse) */
		
		rx_reuse:
		
				rtl_kfree_skb(priv, pskb, _SKB_RX_);
		
		rx_done:

			RTL_WLAN_RX_ATOMIC_PROTECT_EXIT;
		
			SMP_UNLOCK_RECV(y);
		
			if (!IS_DRV_OPEN(priv))
				return;
}


#ifdef CONFIG_PCI_HCI
#ifdef CONFIG_RTK_VOIP_QOS
extern int ( *check_voip_channel_loading )( void );
#endif
#endif


/*
*
* Notice:
*
* Becasue PCIE/USB/SDIO interface use different ISR/RX tasklet, 
* some global modification of rx flow may cause some failures in the other interfaces.
* Please make sure you have add releated code to each hci_type interface,
*
* or you can use hci_type to separate the code for each host controller type.
*   if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {}
*   if (GET_HCI_TYPE(priv) == RTL_HCI_USB) {}
*   if (GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {}
*
*============================================================================================
* PCIE:
*   ISR:
*   rtl8192cd_interrupt() -->
*   __rtl_wlan_interrupt() -->
*   InterruptRxHandle() {
*	    tasklet_hi_schedule(&priv->pshare->rx_tasklet);
*   }
*
*   RX_TASKLET:
*   rtl8192cd_rx_dsr() -->
*   rtl88XX_rx_dsr() {
*   }
*
*
*=============================================================================================
* USB:
*   ISR:
*   usb_read_port_complete () {
*       ......
*       skb_queue_tail(&precvpriv->rx_skb_queue, precvbuf->pskb);
*       if (skb_queue_len(&precvpriv->rx_skb_queue) > 0) {
*           tasklet_schedule(&precvpriv->recv_tasklet);
*       }
*       ......
*   }
*
*   RX_TASKLET:
*   rtw_recv_tasklet() {
*   }
*
*
*=============================================================================================
* SDIO:
*   ISR:
*   sd_rxhandler() {
*   	......
*   	tasklet_schedule(&precvpriv->recv_tasklet);
*   }
*
*   RX_TASKLET by IC:
*   rtl8192es_recv_tasklet() {
*   }
*   rtl8188es_recv_tasklet() {
*   }
*   rtl8822bs_recv_tasklet() {
*   }
*   rtl8821cs_recv_tasklet() {
*   }
*
*/
#ifdef __OSK__
__IRAM_WIFI_PRI3
#else
#if !defined(__LINUX_2_6__) && !defined(__ECOS) 
__MIPS16
#endif
__IRAM_IN_865X
#endif
void rtl88XX_rx_isr(struct rtl8192cd_priv *priv)
{
	struct rtl8192cd_hw     *phw;
	struct sk_buff          *pskb, *new_skb;
	struct rx_frinfo        *pfrinfo;
	unsigned int            reuse;
	unsigned int            MPreuse;
#if defined(DELAY_REFILL_RX_BUF)
	unsigned int            cmp_flags;
#endif
	unsigned int            rtl8192cd_ICV, privacy;
	struct stat_info        *pstat = NULL;
#if (defined(__ECOS) || defined(__LINUX_2_6__)) && defined(RX_TASKLET)
    unsigned long           x;
#if defined(SMP_SYNC)
	unsigned long           y = 0;
#endif
#endif
#if defined(DELAY_REFILL_RX_BUF)
    int                     refill;
#else
    u2Byte                  rxbdOkCnt = 0;
#endif
#if defined(MP_TEST)
	unsigned char           *sa,*da,*bssid;
	char                    *pframe;
	unsigned int            find_flag;
#endif
#if defined(CONFIG_RTL8190_PRIV_SKB)
#ifdef CONCURRENT_MODE
	extern int              skb_free_num[];
#else
	extern int              skb_free_num;
#endif
#endif
#if defined(RX_BUFFER_GATHER)
	int                     pfrinfo_update;
#endif
    unsigned int                    q_num;    
    PHCI_RX_DMA_MANAGER_88XX        prx_dma;
    PHCI_RX_DMA_QUEUE_STRUCT_88XX   cur_q;
    RX_DESC_STATUS_88XX             rx_desc_status;

#if 1/*(defined (CONFIG_RTK_VOIP_QOS) && !defined (CONFIG_RTK_VOIP_ETHERNET_DSP_IS_HOST)) || defined (CHK_RX_ISR_TAKES_TOO_LONG)*/
    unsigned long                   start_time = jiffies;
    int                             n = 0;
#endif
	int                             update = 0;
    u4Byte                          skb_shift_size = 0;
    u4Byte                          skb_used_size;
	unsigned long		max_time;
	int max_cnt;

    int cur_host_idx, hw_idx;

	if (!(priv->drv_state & DRV_STATE_OPEN))
		return;

    SMP_LOCK_RECV(y);

	phw = GET_HW(priv);
    
	if (priv->pshare->rf_ft_var.limit_rxloop_time)
		max_time = RTL_MILISECONDS_TO_JIFFIES(priv->pshare->rf_ft_var.limit_rxloop_time);
	else
		max_time = RTL_MILISECONDS_TO_JIFFIES(20);
	if (priv->pshare->rf_ft_var.limit_rxloop)
		max_cnt = priv->pshare->rf_ft_var.limit_rxloop;
	else
		max_cnt = (NUM_RX_DESC_IF(priv) *2/3);
    
    RTL_WLAN_RX_ATOMIC_PROTECT_ENTER;

    // Currently, only one queue for rx...    
    q_num   = 0;
    GET_HAL_INTERFACE(priv)->UpdateRXBDHWIdxHandler(priv, q_num);

	prx_dma = (PHCI_RX_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PRxDMA88XX);
	cur_q   = &(prx_dma->rx_queue[q_num]);
	cur_host_idx = cur_q->cur_host_idx;
	hw_idx       = cur_q->hw_idx;

	while (cur_host_idx != hw_idx)
	{
		n++;
		if (n > max_cnt)
		{
			priv->ext_stats.rx_break_by_pkt++;
			break;
		}
		if ((jiffies - start_time) >= max_time)
		{
			priv->ext_stats.rx_break_by_time++;
			break;
		}
#if defined (CONFIG_RTK_VOIP_QOS) && !defined (CONFIG_RTK_VOIP_ETHERNET_DSP_IS_HOST)	
		else if ((n > 100 || (jiffies - start_time) >= 1) && (check_voip_channel_loading() && (check_voip_channel_loading() > 0)))
		{
			break;
		}
#endif		
#if defined (CHK_RX_ISR_TAKES_TOO_LONG)
		/* for small packet RX test(ex. VeryWave UDP small pkt size RX test), there may comes too many packets from the air and 
		   cause our RX ISR while(1) loop run too long and then cause the system watch dog timer timeout. 
		   So we break the while loop when it takes to long */
		else if ((n > 1000) || ((jiffies - start_time) >= RTL_MILISECONDS_TO_JIFFIES(1000)))
		{
			break;
		}
#endif
#ifdef CONFIG_WLAN_HAL_8192EE
    	if (GET_CHIP_VER(priv) == VERSION_8192E) {
    		if(((priv->pshare->RxTagPollingCount >20))) 
    			break;
    	}
#endif

#if defined(DELAY_REFILL_RX_BUF)
		refill = 1;
#endif // DELAY_REFILL_RX_BUF

#if defined(RX_BUFFER_GATHER)
        pfrinfo_update = 0;
#endif

        pskb            = (struct sk_buff *)(phw->rx_infoL[cur_host_idx].pbuf);
        pfrinfo         = get_pfrinfo(pskb);
        reuse           = 1;
		MPreuse		= 1;
        skb_shift_size  = 0;

		memset(&rx_desc_status,0,sizeof(struct _RX_DESC_STATUS_88XX_));

#ifdef TRXBD_CACHABLE_REGION
#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
        // With CCI400 ,Do nothing for cache coherent code
#else
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
        if(OFFLOAD_ENABLE(priv)){
            //invalidate in QueryRxDescHandler
        }
        else
#endif
        {
        // Invalidate SKB->data
        rtl_cache_sync_wback(priv,(unsigned long)bus_to_virt(phw->rx_infoL[cur_host_idx].paddr), sizeof(RX_DESC_88XX), HAL_PCI_DMA_FROMDEVICE); 
        }
#endif  //CONFIG_ENABLE_CCI400
#endif //CONFIG_RTL_8198F
#endif //#ifdef TRXBD_CACHABLE_REGION


        if ( RT_STATUS_SUCCESS != GET_HAL_INTERFACE(priv)->QueryRxDescHandler(priv, q_num, pskb->data, &rx_desc_status) ) {
#if defined(CONFIG_NET_PCI) && !defined(USE_RTL8186_SDK)
			if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
				if (IS_PCIBIOS_TYPE) {
					pci_unmap_single(priv->pshare->pdev, phw->rx_infoL[cur_host_idx].paddr, (RX_BUF_LEN - sizeof(struct rx_frinfo)), PCI_DMA_FROMDEVICE);
				}
			}
#endif
            pfrinfo = NULL;
            priv->ext_stats.rx_reuse++;
#ifdef __ECOS
            panic_printk("QueryRxDesc:%s(%d), reuse1(0x%x)\n", __FUNCTION__, __LINE__, (unsigned int)priv->ext_stats.rx_reuse);
#else
            printk("%s(%d), reuse(0x%x)\n", __FUNCTION__, __LINE__, (unsigned int)priv->ext_stats.rx_reuse);
#endif
            goto rx_reuse;
        }

#if defined(CONFIG_NET_PCI) && !defined(USE_RTL8186_SDK)
		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
			if (IS_PCIBIOS_TYPE) {
				pci_unmap_single(priv->pshare->pdev, phw->rx_infoL[cur_host_idx].paddr, rx_desc_status.PKT_LEN, PCI_DMA_FROMDEVICE);
			}
		}
#endif

#ifdef TRXBD_CACHABLE_REGION
#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
        // With CCI400 ,Do nothing for cache coherent code
#else
    if(rx_desc_status.FS == 0x01)
    {
        // If first packet invalidate actually packet according RXDESC packet size
        rtl_cache_sync_wback(priv,(unsigned long)bus_to_virt(phw->rx_infoL[cur_host_idx].paddr + sizeof(RX_DESC_88XX)), rx_desc_status.PKT_LEN, HAL_PCI_DMA_FROMDEVICE); 
    }else
    {
        // If not first packet, invalidate total packet
        rtl_cache_sync_wback(priv,(unsigned long)bus_to_virt(phw->rx_infoL[cur_host_idx].paddr), (RX_BUF_LEN - sizeof(struct rx_frinfo)-64), HAL_PCI_DMA_FROMDEVICE); 
    }      
#endif  //CONFIG_ENABLE_CCI400
#endif //CONFIG_RTL_8198F
#endif //#ifdef TRXBD_CACHABLE_REGION
   

#if	0 /*eric-8822 check csi*/
{
	unsigned char	*pframe_tmp = get_pframe(pfrinfo);
	unsigned int	frtype_tmp = GetFrameSubType(pframe_tmp);

	if(frtype_tmp == Type_Action_No_Ack) {
		if(rx_desc_status.CRC32)
			panic_printk("[%s][%d] RX Action No ACK with CRC32 !!\n", __FUNCTION__, __LINE__);
		else
			panic_printk("[%s][%d] RX Action No ACK\n", __FUNCTION__, __LINE__);
	}
}
#endif

#ifdef CONFIG_ADAPTIVE_SOML
                //soml new feature
	        phydm_soml_crc_acq(ODMPTR, rx_desc_status.RX_RATE, rx_desc_status.CRC32, rx_desc_status.PKT_LEN);
#endif
		if (rx_desc_status.CRC32) {
			priv->pshare->HW_CRC_error_cnt++;
			if(rx_desc_status.FCS_OK) {
				priv->pshare->HW_CRC_err_FCS_ok_cnt++;
				rx_desc_status.CRC32 = 0;
			}
		}

        if (rx_desc_status.CRC32) {		
            rx_pkt_exception_88XX(priv, rx_desc_status.CRC32, rx_desc_status.ICVERR, rx_desc_status.RX_RATE);
            goto rx_reuse;
        }

#if !defined(RX_BUFFER_GATHER)
        else if (!(rx_desc_status.FS == 0x01 && rx_desc_status.LS == 0x01)) {
			// h/w use more than 1 rx descriptor to receive a packet
			// that means this packet is too large
			// drop such kind of packet
			priv->ext_stats.rx_reuse++;
			printk("%s(%d), reuse(0x%x)\n", __FUNCTION__, __LINE__, (unsigned int)priv->ext_stats.rx_reuse);
			goto rx_reuse;
		}
#endif // !defined(RX_BUFFER_GATHER)
		else if (!IS_DRV_OPEN(priv)) {
            priv->ext_stats.rx_reuse++;
			printk("%s(%d), reuse(0x%x)\n", __FUNCTION__, __LINE__, (unsigned int)priv->ext_stats.rx_reuse);
			goto rx_reuse;
		} 
#if defined(__OSK__) && defined(CONFIG_RTL8672)
		else if(AvailableMemSize < (100*1024) && (!rx_desc_status.C2HPkt))
		{	// 20100818 this case should not happened, but we still add here to avoid some potential queuing more buffer in wifi driver
			printk("wifirx:mem(%d)\n", AvailableMemSize);
			goto rx_reuse;
		}
#endif
        else {
			pfrinfo->pskb = pskb;
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
			if (OFFLOAD_ENABLE(priv))
				pfrinfo->pktlen = rx_desc_status.PKT_LEN;
			else
#endif // CONFIG_RTL_OFFLOAD_DRIVER
			if(rx_desc_status.C2HPkt)
				pfrinfo->pktlen = rx_desc_status.PKT_LEN;// If is C2H packet, there is no CRC length Kevin for txbf
        	else 
            	pfrinfo->pktlen = rx_desc_status.PKT_LEN - _CRCLNG_;

#if defined(RX_BUFFER_GATHER)
            if (!(rx_desc_status.FS == 0x01 && rx_desc_status.LS == 0x01)) {
				if (rx_desc_status.FS == 0x01 && priv->pshare->gather_state == GATHER_STATE_NO) {
#if defined(RX_BUFFER_GATHER_REORDER)				
					if(priv->pshare->gather_list_p) {

						flush_rx_list(priv);
					}	
					priv->pshare->gather_list_p=&pfrinfo->gather_list_head;
#endif					
					priv->pshare->gather_state = GATHER_STATE_FIRST;
                    pfrinfo->pktlen = rx_desc_status.RXBuffSize - SIZE_RXDESC_88XX - rx_desc_status.DRV_INFO_SIZE - rx_desc_status.SHIFT;
                    priv->pshare->gather_len = pfrinfo->pktlen;
                    priv->pshare->pkt_total_len = rx_desc_status.PKT_LEN;                    
					pfrinfo->gather_flag = GATHER_FIRST;
                } else if ((rx_desc_status.FS == 0x00 && rx_desc_status.LS == 0x00) &&
                        (priv->pshare->gather_state == GATHER_STATE_FIRST || priv->pshare->gather_state == GATHER_STATE_MIDDLE)) {
                    priv->pshare->gather_state = GATHER_STATE_MIDDLE;
                    pfrinfo->pktlen = rx_desc_status.RXBuffSize;
                    priv->pshare->gather_len += rx_desc_status.RXBuffSize;
                    pfrinfo->gather_flag = GATHER_MIDDLE;
				} else if (rx_desc_status.LS == 0x01 &&
						(priv->pshare->gather_state == GATHER_STATE_FIRST || priv->pshare->gather_state == GATHER_STATE_MIDDLE)) {
					priv->pshare->gather_state = GATHER_STATE_LAST;
                    pfrinfo->pktlen = priv->pshare->pkt_total_len - priv->pshare->gather_len;
					pfrinfo->gather_flag = GATHER_LAST;
				} else {
					MPreuse = 0;
					if (priv->pshare->gather_state != GATHER_STATE_NO) {
						flush_rx_list(priv);
						priv->pshare->gather_state = GATHER_STATE_NO;
					}

                    priv->ext_stats.rx_reuse++;
					//printk("%s(%d), reuse(0x%x)\n", __FUNCTION__, __LINE__, (unsigned int)priv->ext_stats.rx_reuse);
					goto rx_reuse;
				}

			}
			else {
				if (priv->pshare->gather_state != GATHER_STATE_NO) {
					DEBUG_ERR("Rx pkt not in valid gather state [%x]!\n", priv->pshare->gather_state);
					flush_rx_list(priv);
					priv->pshare->gather_state = GATHER_STATE_NO;
					MPreuse = 0;
				}
			}
            
			if (pfrinfo->gather_flag && pfrinfo->gather_flag != GATHER_FIRST) {
				pfrinfo->driver_info_size = 0;
				pfrinfo->rxbuf_shift = 0;
			}
			else
#endif /* RX_BUFFER_GATHER */
			{
                pfrinfo->driver_info_size = rx_desc_status.DRV_INFO_SIZE;
                pfrinfo->rxbuf_shift = rx_desc_status.SHIFT;
			}

            pfrinfo->sw_dec = rx_desc_status.SWDEC;

            if (CheckRxPktLenReuse(priv, pfrinfo, &rx_desc_status)) {
                //printk("pktlen out of range, pfrinfo->pktlen=0x%x, goto rx_reuse\n", pfrinfo->pktlen);
                priv->ext_stats.rx_reuse++;
                //printk("%s(%d), reuse(0x%x)\n", __FUNCTION__, __LINE__, (unsigned int)priv->ext_stats.rx_reuse);
                goto rx_reuse;
            }

#if defined(CONFIG_RTL865X_CMO)
			if (pfrinfo->pskb == NULL) {
				pfrinfo->pskb = pskb;
                priv->ext_stats.rx_reuse++;
				printk("%s(%d), reuse(0x%x)\n", __FUNCTION__, __LINE__, (unsigned int)priv->ext_stats.rx_reuse);
				goto rx_reuse;
			}
#endif

#if defined(RX_BUFFER_GATHER)
            if (priv->pshare->gather_state != GATHER_STATE_MIDDLE && priv->pshare->gather_state != GATHER_STATE_LAST)
#endif
            {
                //3 Shift SKB pointer before we use skb->data
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
				if (OFFLOAD_ENABLE(priv)) {
					int buf_len = ((*(u32 *)&pskb->data[0]) & 0x3fff) + ((*(u32 *)&pskb->data[4]) & 0x3fff);
					//mem_dump("pskb", pskb->data, min(buf_len, 128)); /* FIXME: for debug */

					/* skip rx_dma_info */
	    			pfrinfo->rxdma_info = (struct d2h_rxdma_info *)get_pframe(pfrinfo);
	                skb_reserve(pskb, sizeof(struct d2h_rxdma_info));
	                skb_shift_size += sizeof(struct d2h_rxdma_info);
	                
					/* skip rx_pkt_info */
					pfrinfo->rxpkt_info = (struct d2h_rxpkt_info *)get_pframe(pfrinfo);
	                skb_reserve(pskb, sizeof(struct d2h_rxpkt_info));
	                skb_shift_size += sizeof(struct d2h_rxpkt_info);

	                skb_reserve(pskb, pfrinfo->rxpkt_info->dummy_len);
	                skb_shift_size += pfrinfo->rxpkt_info->dummy_len;

					if (pfrinfo->rxpkt_info->pkt_type == RXPKTINFO_TYPE_RAW) {
	                    skb_reserve(pskb, sizeof(RX_DESC_88XX));
	                    skb_shift_size += sizeof(RX_DESC_88XX);
	                }
				}
#endif // CONFIG_RTL_OFFLOAD_DRIVER
                skb_reserve(pskb, sizeof(RX_DESC_88XX));
                skb_shift_size += sizeof(RX_DESC_88XX);
            }

#ifdef CONFIG_RTL_OFFLOAD_DRIVER
			if (OFFLOAD_ENABLE(priv)) {
				if(pfrinfo->rxpkt_info == NULL) {
					//panic_printk("gather_state=%d, FS=%d, LS:%d, C2HPkt:%d, RXBuffSize:%d, PKT_LEN:%d\n", 
					//	priv->pshare->gather_state, rx_desc_status.FS, rx_desc_status.LS, rx_desc_status.C2HPkt, rx_desc_status.RXBuffSize, rx_desc_status.PKT_LEN);
  				    pskb->data -= skb_shift_size; // skb_shift_size == sizeof(RX_DESC_88XX)
	                skb_reset_tail_pointer(pskb); // skb_shift_size == sizeof(RX_DESC_88XX)
	                goto rx_reuse;
				} else {
					if (pfrinfo->rxpkt_info->pkt_type == RXPKTINFO_TYPE_D2H) {
						#if (defined(TAROKO_0) && defined(PE_RX_D2H_PKT_RECV)) 
						taroko_t2h_ipc_pkt_recv(pskb, ((unsigned int)pfrinfo + sizeof(*pfrinfo)), rx_desc_status.RXBuffSize);
						new_skb = rtl_dev_alloc_skb(priv, RX_BUF_LEN, _SKB_RX_, 0);
						if (new_skb == NULL) {
							//DEBUG_WARN("out of skb_buff\n");
			                priv->ext_stats.rx_reuse++;
							DEBUG_WARN("%s(%d), reuse(0x%x)\n", __FUNCTION__, __LINE__, (unsigned int)priv->ext_stats.rx_reuse);
							priv->ext_stats.reused_skb++;
			                pskb->data -= skb_shift_size; // skb_shift_size is sizeof(RX_DESC_88XX)
			                skb_reset_tail_pointer(pskb); // skb_shift_size is sizeof(RX_DESC_88XX)
							goto rx_reuse;
						}
						pskb = new_skb;
						#else
						pfrinfo->pktlen -= skb_shift_size;
						GET_HAL_INTERFACE(priv)->D2HPacketHandler(priv, get_pframe(pfrinfo), pfrinfo->pktlen);

						pskb->data -= skb_shift_size;
						skb_reset_tail_pointer(pskb);
						#endif
						goto rx_reuse;
					}
				}	
			}
#endif

            //3 1.) Check if C2H packet is received
            if (rx_desc_status.C2HPkt) {

                validate_C2HPkt(priv, pfrinfo);

                pskb->data -= skb_shift_size; // skb_shift_size == sizeof(RX_DESC_88XX)
                skb_reset_tail_pointer(pskb); // skb_shift_size == sizeof(RX_DESC_88XX)
                goto rx_reuse;
            }
#if defined(AP_SWPS_OFFLOAD)
            if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv)) {
                if(rx_desc_status.SWPS_RPT){
					//GDEBUG("rcv swps pkt,report_size = %x,NUM_STAT=%d\n",rx_desc_status.PKT_LEN,NUM_STAT);
                    if((rx_desc_status.FS == 0x00 && rx_desc_status.LS == 0x01) || (rx_desc_status.FS == 0x00 && rx_desc_status.LS == 0x00)){
						  #if 1    
                        pu1Byte	pBuf = get_pframe(pfrinfo);
                        u4Byte dump_i=0;
                        printk("dump rpt:");
                        //for(dump_i=0;dump_i<2138;dump_i++)
                        for(dump_i=0;dump_i<rx_desc_status.PKT_LEN;dump_i++)
                        {
                        if(dump_i%16==0)
                            printk("\n");
                        printk("%x ",*(pBuf+dump_i));

                        }
                        printk("\n");
                        #endif

					}else{
						SWPS_RPT_Handler(priv, pfrinfo);
					}	
					
                    pskb->data -= skb_shift_size; // skb_shift_size == sizeof(RX_DESC_88XX)
                    pskb->tail -= skb_shift_size; // skb_shift_size == sizeof(RX_DESC_88XX)
                    goto rx_reuse;
                }
            }
#elif defined(AP_SWPS_OFFLOAD_WITH_AMPDU_VERI)
            if(rx_desc_status.SWPS_RPT){
					//printk("rcv swps pkt,report_size = %x,NUM_STAT=%d\n",rx_desc_status.PKT_LEN,NUM_STAT);
                    if((rx_desc_status.FS == 0x00 && rx_desc_status.LS == 0x01) || (rx_desc_status.FS == 0x00 && rx_desc_status.LS == 0x00)){
						  #if 1    
                        pu1Byte	pBuf = get_pframe(pfrinfo);
                        u4Byte dump_i=0;
                        printk("dump rpt:");
                        //for(dump_i=0;dump_i<2138;dump_i++)
                        for(dump_i=0;dump_i<rx_desc_status.PKT_LEN;dump_i++)
                        {
                        if(dump_i%16==0)
                            printk("\n");
                        printk("%x ",*(pBuf+dump_i));

                        }
                        printk("\n");
                        #endif

					}else{
						SWPS_RPT_AMPDU_VERI_Handler(priv, pfrinfo);
					}	
					
                    pskb->data -= skb_shift_size; // skb_shift_size == sizeof(RX_DESC_88XX)
                    pskb->tail -= skb_shift_size; // skb_shift_size == sizeof(RX_DESC_88XX)
                    goto rx_reuse;
                }
#endif    

#if defined(RX_BUFFER_GATHER)
            if (TRUE == rx_desc_status.PHYST && ((priv->pshare->gather_state == GATHER_STATE_FIRST) || (priv->pshare->gather_state == GATHER_STATE_NO)))
#else
            if (TRUE == rx_desc_status.PHYST)
#endif
            {
    			pfrinfo->driver_info = (struct RxFWInfo *)(get_pframe(pfrinfo));
            }
            else {
                //printk("%s(%d): driver_info pointer error\n", __FUNCTION__, __LINE__);
            }

            pfrinfo->physt    = rx_desc_status.PHYST;
			pfrinfo->paggr    = rx_desc_status.PAGGR;
            pfrinfo->faggr    = 0;
            pfrinfo->rx_splcp = 0;
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
			if (OFFLOAD_ENABLE(priv)) {
	            pfrinfo->tid      = rx_desc_status.TID;
	            pfrinfo->seq      = rx_desc_status.SEQ;
	            pfrinfo->frag     = rx_desc_status.FRAG;
	            pfrinfo->tpcache  = (pfrinfo->seq << 4) | (pfrinfo->frag & 0xf);
                pfrinfo->amsdu_cut = rx_desc_status.AMSDU_CUT;
			    pfrinfo->last_msdu = rx_desc_status.LAST_MSDU;
			    pfrinfo->a1_match = rx_desc_status.A1_MATCH;
			}
#endif // CONFIG_RTL_OFFLOAD_DRIVER
#if defined(TAROKO_0)
			pfrinfo->rxbuffsize = rx_desc_status.RXBuffSize;
#endif
#ifdef HW_FILL_MACID
	if (IS_SUPPORT_HW_FILL_MACID(priv)) {
            pfrinfo->macid    = rx_desc_status.rxMACID;
	}
#endif //HW_FILL_MACID
			pfrinfo->rx_bw	  = 0;

            if(priv->pmib->dot11BssType.net_work_type & WIRELESS_11AC) {
                if (rx_desc_status.RX_RATE < 12) {
                    pfrinfo->rx_rate = dot11_rate_table[rx_desc_status.RX_RATE];
                } else if(rx_desc_status.RX_RATE < 44) {
                    pfrinfo->rx_rate = HT_RATE_ID + (rx_desc_status.RX_RATE - 12);
                } else{
                    pfrinfo->rx_rate = VHT_RATE_ID + (rx_desc_status.RX_RATE - 44);
                }
            } else if (rx_desc_status.RX_RATE < 12) {
                pfrinfo->rx_rate = dot11_rate_table[rx_desc_status.RX_RATE];
            } else {
                pfrinfo->rx_rate = HT_RATE_ID + (rx_desc_status.RX_RATE - 12);
            }

			if (!pfrinfo->physt) {
				pfrinfo->rf_info.rssi = 0;
			} else {
#if defined(RX_BUFFER_GATHER)
				if (pfrinfo->driver_info_size > 0)
#endif
				{

#ifdef USE_OUT_SRC
#ifdef _OUTSRC_COEXIST
					if(IS_OUTSRC_CHIP(priv))
#endif
					{
#ifdef CONFIG_PE_ENABLE 						
                        if(!priv->pshare->rf_ft_var.manual_pe_enable)
#endif
						translate_rssi_sq_outsrc(priv, pfrinfo, rx_desc_status.RX_RATE);
	
#ifdef CONFIG_WLAN_HAL_8192EE
							//translate_CRC32_outsrc(priv, pfrinfo, rx_desc_status.CRC32);
						if(!rx_desc_status.C2HPkt)
						{
							translate_CRC32_outsrc(priv, pfrinfo, rx_desc_status.CRC32,rx_desc_status.PKT_LEN);
						}
#endif
					
					}
#endif
				
#if !defined(USE_OUT_SRC) || defined(_OUTSRC_COEXIST)
#ifdef _OUTSRC_COEXIST
					if(!IS_OUTSRC_CHIP(priv))
#endif
					{
						translate_rssi_sq(priv, pfrinfo);
					}
#endif

				}
			}

#if defined(CONFIG_RTL8190_PRIV_SKB)
			{
#if defined(DELAY_REFILL_RX_BUF)
                // Use REFILL_THRESHOLD-2 because we should keep at least 2 skb to trigger movement of host_idx
                cmp_flags = (CIRC_CNT_RTK(cur_q->cur_host_idx,
                                (cur_q->host_idx+cur_q->rxbd_ok_cnt)%cur_q->total_rxbd_num, NUM_RX_DESC_IF(priv)) >= REFILL_THRESHOLD_IF(priv)-2);

				if (cmp_flags) {
					DEBUG_WARN("out of skb_buff\n");
                    priv->ext_stats.rx_reuse++;
				    DEBUG_WARN("%s(%d), reuse(0x%x)\n", __FUNCTION__, __LINE__, (unsigned int)priv->ext_stats.rx_reuse);
					priv->ext_stats.reused_skb++;
                    pskb->data -= skb_shift_size; // skb_shift_size is sizeof(RX_DESC_88XX)
                    skb_reset_tail_pointer(pskb); // skb_shift_size is sizeof(RX_DESC_88XX)
					goto rx_reuse;
				}
				new_skb = NULL;
#else
				new_skb = rtl_dev_alloc_skb(priv, RX_BUF_LEN, _SKB_RX_, 0);
				if (new_skb == NULL) {
					DEBUG_WARN("out of skb_buff\n");
                    priv->ext_stats.rx_reuse++;
				    //printk("%s(%d), reuse(0x%x)\n", __FUNCTION__, __LINE__, (unsigned int)priv->ext_stats.rx_reuse);
					priv->ext_stats.reused_skb++;
                    pskb->data -= skb_shift_size; // skb_shift_size is sizeof(RX_DESC_88XX)
                    skb_reset_tail_pointer(pskb); // skb_shift_size is sizeof(RX_DESC_88XX)
					goto rx_reuse;
				}
#endif
			}
#else	/* defined(CONFIG_RTL8190_PRIV_SKB) */

#if defined(DELAY_REFILL_RX_BUF)
			// TODO:  REFILL_THRESHOLD or REFILL_THRESHOLD-1?
			cmp_flags = (CIRC_CNT_RTK(cur_q->cur_host_idx,
							(cur_q->host_idx+cur_q->rxbd_ok_cnt)%cur_q->total_rxbd_num, NUM_RX_DESC_IF(priv)) >= (REFILL_THRESHOLD_IF(priv)-2));

			if (cmp_flags) {
				DEBUG_WARN("out of skb_buff\n");
				priv->ext_stats.rx_reuse++;
				printk("%s(%d), reuse(0x%x)\n", __FUNCTION__, __LINE__, (unsigned int)priv->ext_stats.rx_reuse);
				priv->ext_stats.reused_skb++;
				pskb->data -= skb_shift_size; // skb_shift_size is sizeof(RX_DESC_88XX)
				skb_reset_tail_pointer(pskb); // skb_shift_size is sizeof(RX_DESC_88XX)
				goto rx_reuse;
			}
			new_skb = NULL;
#else

			// allocate new one in advance
#ifdef _CORTINA_
			unsigned long free_page;
#define K(x) ((x) << (PAGE_SHIFT - 10))
			free_page = si_mem_available();
			if ( K(free_page) < priv->pshare->rf_ft_var.rx_min_rsvd_mem)
				new_skb = NULL;
			else
#undef K
#endif
			new_skb = rtl_dev_alloc_skb(priv, RX_BUF_LEN, _SKB_RX_, 0);
			if (new_skb == NULL) {
				//DEBUG_WARN("out of skb_buff\n");
                priv->ext_stats.rx_reuse++;
				DEBUG_WARN("%s(%d), reuse(0x%x)\n", __FUNCTION__, __LINE__, (unsigned int)priv->ext_stats.rx_reuse);
				priv->ext_stats.reused_skb++;
                pskb->data -= skb_shift_size; // skb_shift_size is sizeof(RX_DESC_88XX)
                skb_reset_tail_pointer(pskb); // skb_shift_size is sizeof(RX_DESC_88XX)
				goto rx_reuse;
			}
#endif
#endif

			/*-----------------------------------------------------
			 validate_mpdu will check if we still can reuse the skb
			------------------------------------------------------*/
#ifndef __ECOS
#if (defined(CONFIG_RTL_QOS_PATCH) || defined(CONFIG_RTK_VOIP_QOS) || defined(CONFIG_RTL_CUSTOM_PASSTHRU) || defined(CONFIG_RTK_VLAN_WAN_TAG_SUPPORT) || defined(CONFIG_RTL_MULTI_LAN_DEV) || defined(CONFIG_RTL_EN_UNKNOWNUC_ONLY_TO_CPU) || defined(CONFIG_RTL_NIC_QUEUE)) && (!(defined(CONFIG_RTL8672) && defined(__LINUX_3_18__))) && (!defined(NOT_RTK_BSP))
	#ifndef CONFIG_OPENWRT_SDK
			pskb->srcPhyPort = 7;  //QOS_PATCH_RX_FROM_WIRELESS
	#endif
#endif
#endif

#if defined(MP_TEST)
			if (OPMODE & WIFI_MP_STATE) {
				skb_reserve(pskb, (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size));
                skb_shift_size += (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size);
				reuse = 1;
				find_flag=1;
				//-------------------------------------------------------------------------------------------
				if((OPMODE & WIFI_MP_ARX_FILTER ) && (OPMODE & WIFI_MP_RX ) )
				{
					pframe = get_pframe(pfrinfo);
					sa = 	get_sa(pframe);
					da = 	get_da(pframe);
					bssid =get_bssid_mp(pframe);
					if(priv->pshare->mp_filter_flag & 0x1)
					{
						//sa = 	get_sa(pframe);
						if(memcmp(priv->pshare->mp_filter_SA,sa,MACADDRLEN))
						{
							find_flag=0;
						}
					}
					if(find_flag)
					{
						if(priv->pshare->mp_filter_flag & 0x2)
						{
							//da = 	get_da(pframe);
							if(memcmp(priv->pshare->mp_filter_DA,da,MACADDRLEN))
							{
								find_flag=0;
							}
						}
					}
					if(find_flag)
					{
						if(priv->pshare->mp_filter_flag & 0x4)
						{
							//bssid =get_bssid_mp(pframe);
							if(memcmp(priv->pshare->mp_filter_BSSID,bssid,MACADDRLEN))
							{
								find_flag=0;
							}
						}
					}
					#if 0
					if(find_flag)
					{
						printk("flag: %x\nSA: %02x:%02x:%02x:%02x:%02x:%02x\nDA: %02x:%02x:%02x:%02x:%02x:%02x\nBSSID: %02x:%02x:%02x:%02x:%02x:%02x\n",priv->pshare->mp_filter_flag,
                            sa[0], sa[1], sa[2], sa[3], sa[4], sa[5],
                            da[0], da[1], da[2], da[3], da[4], da[5],
                            bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
					}
					#endif
				}
	//-------------------------------------------------------------------------------------------
				if(find_flag)
				{
					#if defined(B2B_TEST)
					mp_validate_rx_packet(priv, pskb->data, pfrinfo->pktlen);
					#endif
#if defined(CONFIG_RTL8672) || defined(CONFIG_WLAN_STATS_EXTENTION)
					pfrinfo->bcast = IS_BCAST2(get_da(get_pframe(pfrinfo)));
					pfrinfo->mcast = IS_MCAST(get_da(get_pframe(pfrinfo)));
#endif
					pfrinfo->retry = GetRetry(get_pframe(pfrinfo));
#if defined(RX_BUFFER_GATHER)
					if (priv->pshare->gather_state != GATHER_STATE_NO){
						if(priv->pshare->gather_state == GATHER_STATE_LAST)
							rx_sum_up(priv, NULL, pfrinfo);
					}
					if (priv->pshare->gather_state == GATHER_STATE_NO && MPreuse){
						rx_sum_up(priv, NULL, pfrinfo);
					}
#else
					rx_sum_up(priv, NULL, pfrinfo);
#endif

					//if (priv->pshare->rf_ft_var.rssi_dump)
						update_sta_rssi(priv, NULL, pfrinfo);
				}

                pskb->data -= skb_shift_size; // skb_shift_size is (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size + sizeof(RX_DESC_88XX))
                skb_reset_tail_pointer(pskb); // skb_shift_size is (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size + sizeof(RX_DESC_88XX))

#if defined(RX_BUFFER_GATHER)
				if (priv->pshare->gather_state == GATHER_STATE_LAST){
					priv->pshare->gather_state = GATHER_STATE_NO;
				}
#endif				
			}
			else
#endif // defined(MP_TEST)
			{

                // 64 is the value in init_rxdesc_88XX: *pBufLen = RX_BUF_LEN - sizeof(struct rx_frinfo) - 64;
                // Actually, the precise value is the offset, but the value is 64 in old version code (rtl8192cd).
#if defined(RX_BUFFER_GATHER)
                if (rx_desc_status.FS == 0x01 && rx_desc_status.LS == 0x01) {
                    skb_used_size = 64 + sizeof(struct rx_frinfo) + SIZE_RXDESC_88XX + 
                        pfrinfo->driver_info_size + pfrinfo->rxbuf_shift + pfrinfo->pktlen + _CRCLNG_;
                } else if (rx_desc_status.FS == 0x01 && rx_desc_status.LS == 0x0) {
                    skb_used_size = 64 + sizeof(struct rx_frinfo) + SIZE_RXDESC_88XX +
                        pfrinfo->driver_info_size + pfrinfo->rxbuf_shift + pfrinfo->pktlen;
                } else { // (FS,LS) = (0,0) or (0,1)
                    skb_used_size = 64 + sizeof(struct rx_frinfo) +
                        pfrinfo->driver_info_size + pfrinfo->rxbuf_shift + pfrinfo->pktlen;
                }
#else
                skb_used_size = 64 + sizeof(struct rx_frinfo) + SIZE_RXDESC_88XX + 
                    pfrinfo->driver_info_size + pfrinfo->rxbuf_shift + pfrinfo->pktlen + _CRCLNG_;
#endif

                if (skb_used_size <= RX_BUF_LEN) {
					skb_reserve(pskb, (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size));
                    skb_shift_size += (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size);

					// 8822B igoore AES ICV Error when TxBF MU mapping exist
					#if (MU_BEAMFORMING_SUPPORT == 1)
					#ifdef CONFIG_WLAN_HAL_8822BE
					if (GET_CHIP_VER(priv) == VERSION_8822B) {						
						if(priv->pshare->BeamformingInfo.beamformee_mu_reg_maping)
							rx_desc_status.ICVERR =0;							
					}
					#endif
					#endif	
					
                    if (rx_desc_status.ICVERR) {
						rx_pkt_exception_88XX(priv, rx_desc_status.CRC32, rx_desc_status.ICVERR, rx_desc_status.RX_RATE);
						pskb->data -= skb_shift_size; // skb_shift_size is (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size + sizeof(RX_DESC_88XX))
						skb_reset_tail_pointer(pskb); // skb_shift_size is (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size + sizeof(RX_DESC_88XX))
#if !defined(DELAY_REFILL_RX_BUF) || !defined(CONFIG_RTL8190_PRIV_SKB) /*we create this, but we do not free it!*/
						if (new_skb != NULL)
							rtl_kfree_skb(priv, new_skb, _SKB_RX_);
#endif
						priv->ext_stats.rx_reuse++;
						//printk("%s(%d), reuse(0x%x)\n", __FUNCTION__, __LINE__, (unsigned int)priv->ext_stats.rx_reuse);
						goto rx_reuse;
					}

					SNMP_MIB_INC(dot11ReceivedFragmentCount, 1);

                    #if defined(SW_ANT_SWITCH)
					if(priv->pshare->rf_ft_var.antSw_enable) {
						dm_SWAW_RSSI_Check(priv, pfrinfo);
					}
                    #endif

#if defined(RX_BUFFER_GATHER)
					if (priv->pshare->gather_state != GATHER_STATE_NO) {
#if defined(RX_BUFFER_GATHER_REORDER)
						list_add_tail(&pfrinfo->rx_list, priv->pshare->gather_list_p);
#else
						list_add_tail(&pfrinfo->rx_list, &priv->pshare->gather_list);
#endif

						if (priv->pshare->gather_state == GATHER_STATE_LAST) {
							if (!search_first_segment(priv, &pfrinfo)) {
								reuse = 0;
							} else {
								pskb = pfrinfo->pskb;
								pfrinfo_update = 1;
#if defined(RX_BUFFER_GATHER_REORDER)
								priv->pshare->gather_list_p = NULL;
#endif
							}
							priv->pshare->gather_state = GATHER_STATE_NO;
						} else {
							reuse = 0;
						}
					}
					if (priv->pshare->gather_state == GATHER_STATE_NO && reuse)
#endif
                    {
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
                        if (OFFLOAD_ENABLE(priv) && 
                            ((pfrinfo->rxpkt_info->pkt_type == RXPKTINFO_TYPE_8023) || (pfrinfo->rxpkt_info->pkt_type == RXPKTINFO_TYPE_AMSDU))
                        )
                            reuse = validate_mpdu(priv, pfrinfo);
                        else
#endif
                        {
#if (BEAMFORMING_SUPPORT == 1) 
							unsigned char	 *pframe = get_pframe(pfrinfo);
							unsigned int	frtype = GetFrameSubType(pframe);
							if( frtype== Type_Action_No_Ack || frtype == Type_NDPA ) {
								if( frtype== Type_Action_No_Ack) {
									 priv->pshare->rf_ft_var.csi_counter++;
									 priv->pshare->rf_ft_var.csi_counter %= priv->pshare->rf_ft_var.dumpcsi;
									 if( priv->pshare->rf_ft_var.dumpcsi &&
										priv->pshare->rf_ft_var.csi_counter==1)
									  {
										if ((pfrinfo->physt)&& (pfrinfo->driver_info_size > 0))  {
										}
									 }
#if 1 //eric-txbf, rx CSI & work around
									if(pstat && (GET_CHIP_VER(priv)== VERSION_8822B) && (GET_CHIP_VER_8822(priv) <= 0x2)){
										set_csi_report_info(priv, pframe, pfrinfo->pktlen);
									}
#endif								 
								}
								else /*if(frtype == Type_NDPA) */{
								}
								reuse = 1;
							}else
#endif
                        reuse = validate_mpdu(priv, pfrinfo);

                        }
                    }

					if (reuse) {
						priv->ext_stats.rx_reuse++;
#if 1	/*Fix: bridge_handle_frame core dump, epc = 0x00000006*/
						rtl_kfree_skb(priv, pskb, _SKB_RX_);
						reuse = 0;
#else
                        pskb->data -= skb_shift_size; // skb_shift_size is (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size + sizeof(RX_DESC_88XX))
                        skb_reset_tail_pointer(pskb); // skb_shift_size is (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size + sizeof(RX_DESC_88XX))

#ifdef RX_BUFFER_GATHER
                        if (pfrinfo->gather_flag & GATHER_FIRST){
                            //flush_rx_list(priv);
                            rtl_kfree_skb(priv, pskb, _SKB_RX_);
                            reuse = 0;
                 //           printk("%s(%d), Gather-First packet error, free skb\n", __FUNCTION__, __LINE__);
                            DEBUG_WARN("Gather-First packet error, free skb\n");
                        }
#endif
#endif
					}
                    else {
                        //free skb by upper layer, i.e., validate_mpdu
                        //So, we don't need to control skb data/tail pointer
                    }
				}
                else {
                    pskb->data -= skb_shift_size; // skb_shift_size is sizeof(RX_DESC_88XX)
                    skb_reset_tail_pointer(pskb); // skb_shift_size is sizeof(RX_DESC_88XX)
                }
			}
		}	/* if (cmd&XXXX) */

		if (!reuse) {
            phw->rx_infoL[cur_host_idx].pbuf = NULL;

#if defined(CONFIG_NET_PCI) && !defined(USE_RTL8186_SDK)
			if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
				if (IS_PCIBIOS_TYPE) {
//					pci_unmap_single(priv->pshare->pdev, phw->rx_infoL[cur_host_idx].paddr, (RX_BUF_LEN - sizeof(struct rx_frinfo)), PCI_DMA_FROMDEVICE);
				}
			}
#endif

#if defined(DELAY_REFILL_RX_BUF)
#if defined(CONFIG_RTL8190_PRIV_SKB)
#ifdef CONCURRENT_MODE
			if (skb_free_num[priv->pshare->wlandev_idx] == 0 && priv->pshare->skb_queue.qlen == 0)
#else
			if (skb_free_num == 0 && priv->pshare->skb_queue.qlen == 0)
#endif
			{
				refill = 0;
				goto rx_done;
			}
#endif
			new_skb = rtl_dev_alloc_skb(priv, RX_BUF_LEN, _SKB_RX_, 0);
			ASSERT(new_skb);
			if(new_skb == NULL) {
				printk("not enough memory...\n");
				refill = 0;
				goto rx_done;
			}

#endif
			pskb = new_skb;

#if defined(DELAY_REFILL_RX_BUF)
            GET_HAL_INTERFACE(priv)->UpdateRXBDInfoHandler(priv, q_num,
                (cur_q->host_idx+cur_q->rxbd_ok_cnt)%cur_q->total_rxbd_num, (pu1Byte)pskb, init_rxdesc_88XX, _FALSE);
#else
			GET_HAL_INTERFACE(priv)->UpdateRXBDInfoHandler(priv, q_num, cur_host_idx, (pu1Byte)pskb, init_rxdesc_88XX, _FALSE);
#endif
			goto rx_done;

		}	/* if (!reuse) */
#if !defined(DELAY_REFILL_RX_BUF)
		else {
			rtl_kfree_skb(priv, new_skb, _SKB_RX_);
		}
#endif

rx_reuse:
    #if 0   /*Filen: for debug*/
    {
        static unsigned int rx_reuse_cnt = 0;
        
        rx_reuse_cnt++;
        printk("%s(%d): reuse(0x%x), rx_reuse_cnt(0x%x), pkt_len: 0x%x\n", 
                    __FUNCTION__, __LINE__, reuse, rx_reuse_cnt, rx_desc_status.PKT_LEN);
    }
    #endif
    
#if defined(DELAY_REFILL_RX_BUF)
#if defined(RX_BUFFER_GATHER)
        cmp_flags = (cur_q->cur_host_idx != (cur_q->host_idx+cur_q->rxbd_ok_cnt)%cur_q->total_rxbd_num) || pfrinfo_update;
#else
        cmp_flags = (cur_q->cur_host_idx != (cur_q->host_idx+cur_q->rxbd_ok_cnt)%cur_q->total_rxbd_num);
#endif // RX_BUFFER_GATHER

#if defined(RX_BUFFER_GATHER)
		if(pfrinfo && (priv->pshare->gather_state != GATHER_STATE_NO)) {
					flush_rx_list(priv);
					priv->pshare->gather_state = GATHER_STATE_NO;
		}	
#endif		
		if (cmp_flags)
		{
            phw->rx_infoL[cur_q->cur_host_idx].pbuf = NULL;
			pskb->data = pskb->head;
			skb_reset_tail_pointer(pskb);

			// TODO: why skb_reserve 128? 
			// 1. priv_skb: dev_alloc_8190_skb: reserve 128 
			// 2. normal: reserve max(32, NET_SKB_PAD) 
 #ifdef __ECOS
			skb_reserve(pskb, 32);
 #else
			skb_reserve(pskb, 128);
 #endif
#if defined(CONFIG_RTL8196_RTL8366)
			skb_reserve(pskb, 8);
#endif
#if defined(CONFIG_RTK_VOIP_VLAN_ID)
			skb_reserve(pskb, 4);
#endif

            //refill_rx_ring_88XX(priv, pskb, NULL, q_num, cur_q);
            GET_HAL_INTERFACE(priv)->UpdateRXBDInfoHandler(priv, q_num,
                (cur_q->host_idx+cur_q->rxbd_ok_cnt)%cur_q->total_rxbd_num, (pu1Byte)pskb, init_rxdesc_88XX, _FALSE);
			//refill = 0;
		} else
#endif	/*	defined(DELAY_REFILL_RX_BUF)	*/
		{
			u4Byte	offset;
#if defined(RX_BUFFER_GATHER)
            if (pfrinfo != NULL) {
    			pfrinfo->gather_flag = 0;
            }
#endif
			offset = GetOffsetStartToRXDESC(priv, pskb);
			offset += sizeof(struct rx_frinfo);
#if defined(CONFIG_NET_PCI) && !defined(USE_RTL8186_SDK)
//			phw->rx_infoL[cur_host_idx].paddr = get_physical_addr(priv, pskb->data, (RX_BUF_LEN - offset), PCI_DMA_FROMDEVICE);

			pskb->data -= offset;
			skb_reset_tail_pointer(pskb);
 
            GET_HAL_INTERFACE(priv)->UpdateRXBDInfoHandler(priv, q_num, cur_host_idx, (pu1Byte)pskb, init_rxdesc_88XX, _FALSE);
			// Remove it because pci_map_single() in get_physical_addr() already performed memory sync.
            //rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(phw->rx_infoL[cur_host_idx].paddr),
            //    RX_BUF_LEN - offset,
            //    PCI_DMA_FROMDEVICE);
#else
#ifdef TRXBD_CACHABLE_REGION
            memset(bus_to_virt(phw->rx_infoL[cur_host_idx].paddr), 0, RX_BUF_LEN - offset);
#ifdef CONFIG_RTL_8198F
     
#ifdef CONFIG_ENABLE_CCI400
        // With CCI400 ,Do nothing for cache coherent code
#else
        rtl_cache_sync_wback(priv, (unsigned long)(bus_to_virt(phw->rx_infoL[cur_host_idx].paddr)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), 
            RX_BUF_LEN - offset, PCI_DMA_FROMDEVICE);
#endif  //CONFIG_ENABLE_CCI400
#else

            _dma_cache_wback((unsigned long)(bus_to_virt(phw->rx_infoL[cur_host_idx].paddr)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), 
                RX_BUF_LEN - offset);
#endif //CONFIG_RTL_8198F
//            _dma_cache_inv((unsigned long)(bus_to_virt(phw->rx_infoL[cur_host_idx].paddr)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), 
//                RX_BUF_LEN - offset);
#else
            rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(phw->rx_infoL[cur_host_idx].paddr),
                RX_BUF_LEN - offset,
                PCI_DMA_FROMDEVICE);
#endif //#ifdef TRXBD_CACHABLE_REGION

#endif

#if defined(TAROKO_0)
#if defined(DELAY_REFILL_RX_BUF)
			GET_HAL_INTERFACE(priv)->UpdateRXBDInfoHandler(priv, q_num,
				(cur_q->host_idx+cur_q->rxbd_ok_cnt)%cur_q->total_rxbd_num, (pu1Byte)pskb, init_rxdesc_88XX, _FALSE);
#else
			GET_HAL_INTERFACE(priv)->UpdateRXBDInfoHandler(priv, q_num, cur_host_idx, (pu1Byte)pskb, init_rxdesc_88XX, _FALSE);
#endif
#endif

		}

rx_done:

		{
			cur_host_idx = (cur_q->cur_host_idx + 1) % cur_q->total_rxbd_num;
			cur_q->cur_host_idx = cur_host_idx;
		}

#if CFG_UPDATE_RX_SWBD_IDX_EARLY
    	update = 0;
        if(cur_host_idx == hw_idx) {
            GET_HAL_INTERFACE(priv)->UpdateRXBDHWIdxHandler(priv, q_num); 
	        update = 1;
	    }
#endif
		
	if((priv->pmib->miscEntry.update_rxbd_th != 0) && (n % priv->pmib->miscEntry.update_rxbd_th == 0)) {
	        update = 1;
	}
		
#if defined(DELAY_REFILL_RX_BUF)
		if (refill) {
            cur_q->rxbd_ok_cnt++;
		}
#else
        rxbdOkCnt++;
#endif // DELAY_REFILL_RX_BUF

#if CFG_UPDATE_RX_SWBD_IDX_EARLY
		if (update) {
#if defined(DELAY_REFILL_RX_BUF)
			GET_HAL_INTERFACE(priv)->UpdateRXBDHostIdxHandler(priv, q_num, cur_q->rxbd_ok_cnt);
			cur_q->rxbd_ok_cnt = 0;
#else
			GET_HAL_INTERFACE(priv)->UpdateRXBDHostIdxHandler(priv, q_num, rxbdOkCnt);
            rxbdOkCnt = 0;
#endif
		}
#endif //CFG_UPDATE_RX_SWBD_IDX_EARLY
	}	/*	while(1)	*/

//#if !CFG_UPDATE_RX_SWBD_IDX_EARLY
#if defined(DELAY_REFILL_RX_BUF)
    GET_HAL_INTERFACE(priv)->UpdateRXBDHostIdxHandler(priv, q_num, cur_q->rxbd_ok_cnt);
    cur_q->rxbd_ok_cnt = 0;
#else
    GET_HAL_INTERFACE(priv)->UpdateRXBDHostIdxHandler(priv, q_num, rxbdOkCnt);
#endif
//#endif //!CFG_UPDATE_RX_SWBD_IDX_EARLY

    RTL_WLAN_RX_ATOMIC_PROTECT_EXIT;

    SMP_UNLOCK_RECV(y);

	if (!IS_DRV_OPEN(priv))
		return;

//TODO, power saving
#if defined(TAROKO_0)
#else
#if defined(RTL8190_ISR_RX) && defined(RTL8190_DIRECT_RX)
	if (OPMODE & WIFI_AP_STATE) {
		if (!list_empty(&priv->wakeup_list))
			process_dzqueue(priv);

#if defined(MBSSID)
		if (priv->pmib->miscEntry.vap_enable) {
			int i;
			struct rtl8192cd_priv *priv_vap;

			for (i=0; i<RTL8192CD_NUM_VWLAN; i++) {
				if (IS_DRV_OPEN(priv->pvap_priv[i])) {
					priv_vap = priv->pvap_priv[i];
					if (!list_empty(&priv_vap->wakeup_list))
						process_dzqueue(priv_vap);
				}
			}
		}
#endif
	}
#ifdef UNIVERSAL_REPEATER
	else {
		if (IS_DRV_OPEN(GET_VXD_PRIV(priv)))
			if (!list_empty(&GET_VXD_PRIV(priv)->wakeup_list))
				process_dzqueue(GET_VXD_PRIV(priv));
	}
#endif
#endif	/*	defined(RTL8190_ISR_RX) && defined(RTL8190_DIRECT_RX)	*/
#endif

	if(priv->pshare->skip_mic_chk)
		--priv->pshare->skip_mic_chk;
}



#ifdef __OSK__
__IRAM_WIFI_PRI3
#else
#ifndef __ECOS
#ifndef __LINUX_2_6__
__MIPS16
#endif
#endif
__IRAM_IN_865X
#endif
void rtl8192cd_rx_isr(struct rtl8192cd_priv *priv)
{
	struct rx_desc *pdesc, *prxdesc;
	struct rtl8192cd_hw *phw;
	struct sk_buff *pskb, *new_skb = NULL;
	struct rx_frinfo *pfrinfo;
	unsigned int tail;
	unsigned int cmd, reuse;
#ifdef DELAY_REFILL_RX_BUF
	unsigned int cmp_flags;
#endif
	unsigned int rtl8192cd_ICV, privacy;
	struct stat_info *pstat;
#if  (defined(__ECOS) ||defined(__LINUX_2_6__)) && defined(RX_TASKLET)
	unsigned long x;
#if defined(SMP_SYNC)
	unsigned long y = 0;
#endif
#endif
#if defined(DELAY_REFILL_RX_BUF)
	int refill;
#endif
#if defined(MP_TEST)
	unsigned char *sa,*da,*bssid;
	char *pframe;
	unsigned int  find_flag;
#endif
#if defined(CONFIG_RTL8190_PRIV_SKB)
#ifdef CONCURRENT_MODE
	extern int skb_free_num[];
#else
	extern int skb_free_num;
#endif
#endif
#if defined(RX_BUFFER_GATHER)
	int pfrinfo_update;
#endif

	unsigned int rxdescDW1;	
	unsigned int rxdescDW2;
	unsigned int rxdescDW3;
	unsigned int rxdescDW4;
#if /*defined(__ECOS) ||*/ (defined (CONFIG_RTK_VOIP_QOS) && !defined (CONFIG_RTK_VOIP_ETHERNET_DSP_IS_HOST))
	unsigned long start_time = jiffies;
#endif
	int n = 0;

#ifdef CONFIG_WLAN_HAL
	if(IS_HAL_CHIP(priv)) {
		rtl88XX_rx_isr(priv);
		return;
    } else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif //CONFIG_WLAN_HAL
	{
		if (!(priv->drv_state & DRV_STATE_OPEN))
			return;

		SMP_LOCK_RECV(y);

		phw = GET_HW(priv);
		tail = phw->cur_rx;

#if defined(RTL8190_CACHABLE_DESC)
		prxdesc = (struct rx_desc *)(phw->rx_descL);
		rtl_cache_sync_wback(priv, (unsigned long)prxdesc, sizeof(struct rx_desc), PCI_DMA_FROMDEVICE);
#else
#if defined(__MIPSEB__)
		prxdesc = (struct rx_desc *)((unsigned long)(phw->rx_descL) | 0x20000000);
#else
		prxdesc = (struct rx_desc *)(phw->rx_descL);
#endif
#endif	/* RTL8190_CACHABLE_DESC */

		while (1)
		{
			n++;
			if (priv->pshare->rf_ft_var.limit_rxloop && (n > priv->pshare->rf_ft_var.limit_rxloop)) {
				priv->ext_stats.rx_break_by_pkt++;
				break;
			}
#if defined (CONFIG_RTK_VOIP_QOS) && !defined (CONFIG_RTK_VOIP_ETHERNET_DSP_IS_HOST)	
			if ( (n > 100 || (jiffies - start_time) >= 1 )&& (check_voip_channel_loading() && (check_voip_channel_loading() > 0)) )
			{
				break;		
			}
#endif
#if defined(DELAY_REFILL_RX_BUF)
			refill = 1;

			/*
			if (((tail+1) % NUM_RX_DESC) == phw->cur_rx_refill) {
				break;
			}*/
			RTL_WLAN_RX_ATOMIC_PROTECT_ENTER;
			cmp_flags = ( ((tail+1)==NUM_RX_DESC_IF(priv)?0:tail+1) == phw->cur_rx_refill );
			RTL_WLAN_RX_ATOMIC_PROTECT_EXIT;

			if (cmp_flags) {
				break;
			}
#endif

#if defined(RX_BUFFER_GATHER)
			pfrinfo_update = 0;
#endif

			pdesc = prxdesc + tail;
			cmd = get_desc(pdesc->Dword0);
			reuse = 1;

			if (cmd & RX_OWN)
				break;
			#if defined(CONFIG_NET_PCI) && !defined(USE_RTL8186_SDK)
			if (IS_PCIBIOS_TYPE) {
				pci_unmap_single(priv->pshare->pdev, phw->rx_infoL[tail].paddr, (cmd & RX_PktLenMask), PCI_DMA_FROMDEVICE);
			}
			#endif

			pskb = (struct sk_buff *)(phw->rx_infoL[tail].pbuf);
			pfrinfo = get_pfrinfo(pskb);

#ifdef MP_SWITCH_LNA
			if((GET_CHIP_VER(priv) == VERSION_8192D) && priv->pshare->rf_ft_var.mp_specific)
				dynamic_switch_lna(priv);
#endif

			if (priv->pshare->rf_ft_var.rcr_val & RCR_ACRC32)
			{
				if (cmd & RX_CRC32) {
					/*printk("CRC32 happens~!!\n");*/
					rx_pkt_exception(priv, cmd);
					goto rx_reuse;
				}
			}
#if defined(CONFIG_RTL_88E_SUPPORT) && defined(TXREPORT)
			else if ((GET_CHIP_VER(priv)==VERSION_8188E) && 
				(((get_desc(pdesc->Dword3) >> RXdesc_88E_RptSelSHIFT) & RXdesc_88E_RptSelMask) == 2)) {
				pfrinfo->pktlen = (cmd & RX_PktLenMask);
				if (get_desc(pdesc->Dword4) || get_desc(pdesc->Dword5))
#ifdef RATEADAPTIVE_BY_ODM
					odm_ra_tx_rpt2_handle_8188e(ODMPTR, pskb->data, pfrinfo->pktlen, get_desc(pdesc->Dword4), get_desc(pdesc->Dword5));
#else
					RTL8188E_TxReportHandler(priv, pskb, get_desc(pdesc->Dword4), get_desc(pdesc->Dword5), pdesc);
#endif
				goto rx_reuse;
			} else if ((GET_CHIP_VER(priv)==VERSION_8188E) && 
				((get_desc(pdesc->Dword3) >> RXdesc_88E_RptSelSHIFT) & RXdesc_88E_RptSelMask)) {
				printk("%s %d, Rx report select mismatch, val:%d\n", __FUNCTION__, __LINE__, 
					((get_desc(pdesc->Dword3) >> RXdesc_88E_RptSelSHIFT) & RXdesc_88E_RptSelMask));
				goto rx_reuse;
			}
#endif
#if !defined(RX_BUFFER_GATHER)
			else if ((cmd & (RX_FirstSeg | RX_LastSeg)) != (RX_FirstSeg | RX_LastSeg)) {
				// h/w use more than 1 rx descriptor to receive a packet
				// that means this packet is too large
				// drop such kind of packet
				goto rx_reuse;
			}
#endif
			else if (!IS_DRV_OPEN(priv)) {
				goto rx_reuse;
			}
#if defined(__OSK__) && defined(CONFIG_RTL8672)
			else if(AvailableMemSize < (100*1024))
			{	// 20100818 this case should not happened, but we still add here to avoid some potential queuing more buffer in wifi driver
				printk("wifirx:mem(%d)\n", AvailableMemSize);
				goto rx_reuse;
			}
#endif
			else {
				pfrinfo->pskb = pskb;
				pfrinfo->pktlen = (cmd & RX_PktLenMask) - _CRCLNG_;

#if defined(RX_BUFFER_GATHER)
				if ((cmd & (RX_FirstSeg | RX_LastSeg)) != (RX_FirstSeg | RX_LastSeg)) {
					if ((cmd & RX_FirstSeg) && priv->pshare->gather_state == GATHER_STATE_NO) {

#if defined(RX_BUFFER_GATHER_REORDER)				
						if(priv->pshare->gather_list_p)
							flush_rx_list(priv);
						INIT_LIST_HEAD(&pfrinfo->gather_list_head);
						priv->pshare->gather_list_p=&pfrinfo->gather_list_head;	
#endif	
						priv->pshare->gather_state = GATHER_STATE_FIRST;
						priv->pshare->gather_len = pfrinfo->pktlen;
						pfrinfo->gather_flag = GATHER_FIRST;
					} else if (!(cmd & (RX_FirstSeg | RX_LastSeg)) &&
							(priv->pshare->gather_state == GATHER_STATE_FIRST || priv->pshare->gather_state == GATHER_STATE_MIDDLE)) {
						priv->pshare->gather_state = GATHER_STATE_MIDDLE;
						priv->pshare->gather_len += pfrinfo->pktlen;
						pfrinfo->gather_flag = GATHER_MIDDLE;
					} else if ((cmd & RX_LastSeg) &&
							(priv->pshare->gather_state == GATHER_STATE_FIRST || priv->pshare->gather_state == GATHER_STATE_MIDDLE)) {
						priv->pshare->gather_state = GATHER_STATE_LAST;
						pfrinfo->pktlen -= priv->pshare->gather_len;
						pfrinfo->gather_flag = GATHER_LAST;
					} else {
						if (priv->pshare->gather_state != GATHER_STATE_NO) {
							DEBUG_ERR("Rx pkt not in sequence [%x, %x]!\n", (cmd & (RX_FirstSeg | RX_LastSeg)), priv->pshare->gather_state);
							flush_rx_list(priv);
							priv->pshare->gather_state = GATHER_STATE_NO;
						}
						goto rx_reuse;
					}
				} else {
					if (priv->pshare->gather_state != GATHER_STATE_NO) {
						DEBUG_ERR("Rx pkt not in valid gather state [%x]!\n", priv->pshare->gather_state);
						flush_rx_list(priv);
						priv->pshare->gather_state = GATHER_STATE_NO;
					}
				}
				if (pfrinfo->gather_flag && pfrinfo->gather_flag != GATHER_FIRST) {
					pfrinfo->driver_info_size = 0;
					pfrinfo->rxbuf_shift = 0;
				}
				else
#endif /* RX_BUFFER_GATHER */
				{
					pfrinfo->driver_info_size = ((cmd >> RX_DrvInfoSizeSHIFT) & RX_DrvInfoSizeMask)<<3;
					pfrinfo->rxbuf_shift = (cmd & (RX_ShiftMask << RX_ShiftSHIFT)) >> RX_ShiftSHIFT;
				}

#if defined(RX_BUFFER_GATHER)
				if (pfrinfo->gather_flag) {
					pfrinfo->pktlen -= (pfrinfo->driver_info_size - 4);
					priv->pshare->gather_len -= (pfrinfo->driver_info_size - 4);
				}
#endif
				pfrinfo->sw_dec = (cmd & RX_SwDec) >> 27;
				pfrinfo->pktlen -= pfrinfo->rxbuf_shift;
#if	defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)	
				if ( ((GET_CHIP_VER(priv)!= VERSION_8812E && GET_CHIP_VER(priv)!= VERSION_8723B) && (pfrinfo->pktlen > 0x2000))
					|| ((GET_CHIP_VER(priv)== VERSION_8812E || GET_CHIP_VER(priv)== VERSION_8723B) && (pfrinfo->pktlen > 0x2d00))
					|| (pfrinfo->pktlen < 16))
#else
				if ((pfrinfo->pktlen > 0x2000) || (pfrinfo->pktlen < 16))
#endif
				{
#if defined(RX_BUFFER_GATHER)
					if (!(pfrinfo->gather_flag && (pfrinfo->pktlen < 16)))
#endif
					{
						DEBUG_INFO("pfrinfo->pktlen=%d, goto rx_reuse\n",pfrinfo->pktlen);
						if( get_desc(pdesc->Dword2) & BIT(28))	{
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)				
							if(GET_CHIP_VER(priv)== VERSION_8812E || GET_CHIP_VER(priv)== VERSION_8723B)
								C2HPacketHandler_8812(priv, get_pframe(pfrinfo), pfrinfo->pktlen+4); // no CRC in C2H_PACKET
#endif												
						} else {
							DEBUG_INFO("pfrinfo->pktlen=%d, goto rx_reuse\n",pfrinfo->pktlen);
						}
						goto rx_reuse;
					}
				}

	//#ifdef PCIE_POWER_SAVING_DEBUG
#if 0
				if(priv->firstPkt)	{
					unsigned char *pp= pdesc ;
					printk("rx isr, first pkt: len=%d\n[",  pfrinfo->pktlen);
				//	printHex(pp, sizeof(struct rx_frinfo));
				//	printk("------------------\n");
					printHex(pskb->data+32,pfrinfo->pktlen);
					priv->firstPkt=0;
					printk("]\n\n");
				}
#endif

#if defined(CONFIG_RTL865X_CMO)
				if (pfrinfo->pskb == NULL) {
					panic_printk("  rtl8192cd_rx_isr(): pfrinfo->pskb = NULL.\n");
					pfrinfo->pskb = pskb;
					goto rx_reuse;
				}
#endif

				pfrinfo->driver_info = (struct RxFWInfo *)(get_pframe(pfrinfo));
				rxdescDW1 = get_desc(pdesc->Dword1);
				rxdescDW4 = get_desc(pdesc->Dword4);
				pfrinfo->physt = (cmd & RX_PHYST)? 1:0;
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)/*eric_8812 ??*/
				if (GET_CHIP_VER(priv)==VERSION_8812E || GET_CHIP_VER(priv) == VERSION_8723B)
				{
					pfrinfo->paggr    = (rxdescDW1 & RXdesc_92E_PAGGR)? 1:0;
					pfrinfo->faggr    = 0;
					if (IS_C_CUT_8812(priv)) {
						pfrinfo->rx_bw	  = (rxdescDW4>>4)&0x3;
						pfrinfo->rx_splcp = (rxdescDW4)&0x01;
					} else {				
						pfrinfo->rx_bw	  = 0;
						pfrinfo->rx_splcp = 0;
					}				
				}
				else
#endif			
				{
					rxdescDW3 = get_desc(pdesc->Dword3);
					pfrinfo->faggr    = (rxdescDW1 & RX_FAGGR)? 1:0;
					pfrinfo->paggr    = (rxdescDW1 & RX_PAGGR)? 1:0;
					pfrinfo->rx_bw    = (rxdescDW3 & RX_BW)? 1:0;
					pfrinfo->rx_splcp = (rxdescDW3 & RX_SPLCP)? 1:0;
				}

			if (!pfrinfo->physt) {
				pfrinfo->rf_info.rssi = 0;
			} else {
				unsigned int rxdesc_rate = (get_desc(pdesc->Dword3)& RX_RxMcsMask);
				if (rxdesc_rate < 12) {
					pfrinfo->rx_rate = dot11_rate_table[rxdesc_rate];
				} else if(rxdesc_rate < 44) {
					pfrinfo->rx_rate = HT_RATE_ID +(rxdesc_rate - 12);
				} else{
					pfrinfo->rx_rate = VHT_RATE_ID +(rxdesc_rate - 44);
				}
				
#if defined(RX_BUFFER_GATHER)
					if (pfrinfo->driver_info_size > 0)
#endif
					{
#ifdef USE_OUT_SRC				
#ifdef _OUTSRC_COEXIST
						if(IS_OUTSRC_CHIP(priv))
#endif
						{
#ifdef CONFIG_PE_ENABLE      							
                        if(!priv->pshare->rf_ft_var.manual_pe_enable)
#endif
						translate_rssi_sq_outsrc(priv, pfrinfo, rxdesc_rate);

						}
#endif
					
#if !defined(USE_OUT_SRC) || defined(_OUTSRC_COEXIST)
#ifdef _OUTSRC_COEXIST
						if(!IS_OUTSRC_CHIP(priv))
#endif
						{
						translate_rssi_sq(priv, pfrinfo);
						}
#endif
					}
				}

#if defined(CONFIG_RTL8190_PRIV_SKB)
				{
#if defined(DELAY_REFILL_RX_BUF)
					RTL_WLAN_RX_ATOMIC_PROTECT_ENTER;
					cmp_flags = (CIRC_CNT_RTK(tail, phw->cur_rx_refill, NUM_RX_DESC_IF(priv)) > REFILL_THRESHOLD_IF(priv));
					RTL_WLAN_RX_ATOMIC_PROTECT_EXIT;
					if (cmp_flags) {
						DEBUG_WARN("out of skb_buff\n");
						priv->ext_stats.reused_skb++;
						goto rx_reuse;
					}
					new_skb = NULL;
#else
	//				if (skb_free_num== 0 && priv->pshare->skb_queue.qlen == 0) {
					new_skb = rtl_dev_alloc_skb(priv, RX_BUF_LEN, _SKB_RX_, 0);
					if (new_skb == NULL) {
						DEBUG_WARN("out of skb_buff\n");
						priv->ext_stats.reused_skb++;
						goto rx_reuse;
					}
#endif
				}
#else	/* defined(CONFIG_RTL8190_PRIV_SKB) */
#if defined(DELAY_REFILL_RX_BUF)
				RTL_WLAN_RX_ATOMIC_PROTECT_ENTER;
				cmp_flags = (CIRC_CNT_RTK(tail, phw->cur_rx_refill, NUM_RX_DESC_IF(priv)) > REFILL_THRESHOLD_IF(priv));
				RTL_WLAN_RX_ATOMIC_PROTECT_EXIT;
				if (cmp_flags) {
					DEBUG_WARN("out of skb_buff\n");
					priv->ext_stats.reused_skb++;
					goto rx_reuse;
				}
				new_skb = NULL;
#else
				// allocate new one in advance
				new_skb = rtl_dev_alloc_skb(priv, RX_BUF_LEN, _SKB_RX_, 0);
				if (new_skb == NULL) {
					DEBUG_WARN("out of skb_buff\n");
					priv->ext_stats.reused_skb++;
					goto rx_reuse;
				}
#endif
#endif

				/*-----------------------------------------------------
				 validate_mpdu will check if we still can reuse the skb
				------------------------------------------------------*/
#ifndef __ECOS				
#if (defined(CONFIG_RTL_QOS_PATCH) || defined(CONFIG_RTK_VOIP_QOS) || defined(CONFIG_RTL_CUSTOM_PASSTHRU) ||  defined(CONFIG_RTK_VLAN_WAN_TAG_SUPPORT) || defined(CONFIG_RTL_MULTI_LAN_DEV) || defined(CONFIG_RTL_EN_UNKNOWNUC_ONLY_TO_CPU) || defined(CONFIG_RTL_NIC_QUEUE)) && (!(defined(CONFIG_RTL8672) && defined(__LINUX_3_18__))) && (!defined(NOT_RTK_BSP))
	#ifndef CONFIG_OPENWRT_SDK
				pskb->srcPhyPort = 7;  //QOS_PATCH_RX_FROM_WIRELESS
	#endif
#endif
#endif

#if defined(MP_TEST)
				if (OPMODE & WIFI_MP_STATE) {
					skb_reserve(pskb, (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size));
					reuse = 1;
					find_flag=1;
					//-------------------------------------------------------------------------------------------
					if((OPMODE & WIFI_MP_ARX_FILTER ) && (OPMODE & WIFI_MP_RX ) )
					{
						pframe = get_pframe(pfrinfo);
						sa = 	get_sa(pframe);
						da = 	get_da(pframe);
						bssid =get_bssid_mp(pframe);
						if(priv->pshare->mp_filter_flag & 0x1)
						{
							//sa = 	get_sa(pframe);
							if(memcmp(priv->pshare->mp_filter_SA,sa,MACADDRLEN))
							{
								find_flag=0;
							}
						}
						if(find_flag)
						{
							if(priv->pshare->mp_filter_flag & 0x2)
							{
								//da = 	get_da(pframe);
								if(memcmp(priv->pshare->mp_filter_DA,da,MACADDRLEN))
								{
									find_flag=0;
								}
							}
						}
						if(find_flag)
						{
							if(priv->pshare->mp_filter_flag & 0x4)
							{
								//bssid =get_bssid_mp(pframe);
								if(memcmp(priv->pshare->mp_filter_BSSID,bssid,MACADDRLEN))
								{
									find_flag=0;
								}
							}
						}
#if 0
						if(find_flag)
						{
							printk("flag: %x\nSA: %02x:%02x:%02x:%02x:%02x:%02x\nDA: %02x:%02x:%02x:%02x:%02x:%02x\nBSSID: %02x:%02x:%02x:%02x:%02x:%02x\n",priv->pshare->mp_filter_flag,
																sa[0],
																sa[1],
																sa[2],
																sa[3],
																sa[4],
																sa[5],
																da[0],
																da[1],
																da[2],
																da[3],
																da[4],
																da[5],
																bssid[0],
																bssid[1],
																bssid[2],
																bssid[3],
																bssid[4],
																bssid[5]);
						}
#endif
					}
		//-------------------------------------------------------------------------------------------
					if(find_flag)
					{
#if defined(B2B_TEST)
						mp_validate_rx_packet(priv, pskb->data, pfrinfo->pktlen);
#endif
#if defined(CONFIG_RTL8672) || defined(CONFIG_WLAN_STATS_EXTENTION)
						pfrinfo->bcast = IS_BCAST2(get_da(get_pframe(pfrinfo)));
						pfrinfo->mcast = IS_MCAST(get_da(get_pframe(pfrinfo)));
#endif
						pfrinfo->retry = GetRetry(get_pframe(pfrinfo));
						rx_sum_up(priv, NULL, pfrinfo);

						if (priv->pshare->rf_ft_var.rssi_dump)
							update_sta_rssi(priv, NULL, pfrinfo);
					}
						pskb->data -= (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size);
						skb_reset_tail_pointer(pskb);
				}
				else
#endif // defined(MP_TEST)
				{
#if defined(RX_BUFFER_GATHER)
					#define	RTL_WLAN_DRV_RX_GATHER_GAP_THRESHOLD	32
#else
					#define	RTL_WLAN_DRV_RX_GATHER_GAP_THRESHOLD	64
#endif

					if (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size + pfrinfo->pktlen + _CRCLNG_ <= (RX_BUF_LEN- sizeof(struct rx_frinfo)-RTL_WLAN_DRV_RX_GATHER_GAP_THRESHOLD) ) {
						skb_reserve(pskb, (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size));
                        if (cmd & RX_ICVERR) {
                            rtl8192cd_ICV = privacy = 0;
                            pstat = NULL;

                            #if defined(WDS) || defined(CONFIG_RTK_MESH) || defined(A4_STA)
                            if (get_tofr_ds((unsigned char *)get_pframe(pfrinfo)) == 3) {
                                pstat = get_stainfo(priv, (unsigned char *)GetAddr2Ptr((unsigned char *)get_pframe(pfrinfo)));
                            } else
                            #endif
                            {
                                #ifdef HW_FILL_MACID
                                pstat = get_stainfo_fast(priv,(unsigned char *)get_sa((unsigned char *)get_pframe(pfrinfo)),pfrinfo->macid);    
                                #else
                                pstat = get_stainfo(priv,(unsigned char *)get_sa((unsigned char *)get_pframe(pfrinfo)));
                                #endif //#ifdef HW_FILL_MACID	                        						
                            }

                            if (!pstat) {
                                rtl8192cd_ICV++;
                            } else {
                                if (OPMODE & WIFI_AP_STATE) {
                                    #if defined(WDS) || defined(CONFIG_RTK_MESH) || defined(A4_STA)
                                    if (get_tofr_ds((unsigned char *)get_pframe(pfrinfo)) == 3) {
                                        #if defined(CONFIG_RTK_MESH)
                                        if(priv->pmib->dot1180211sInfo.mesh_enable) {
                                            privacy = (IS_MCAST(GetAddr1Ptr((unsigned char *)get_pframe(pfrinfo)))) ? _NO_PRIVACY_ : priv->pmib->dot11sKeysTable.dot11Privacy;
                                        } else
                                        #endif
                                        #if defined(WDS)
                                        if(priv->pmib->dot11WdsInfo.wdsEnabled) {
                                            privacy = priv->pmib->dot11WdsInfo.wdsPrivacy;
                                        } else
                                        #endif
                                        #if defined(A4_STA)
                                        if (priv->pmib->miscEntry.a4_enable) {
                                            privacy = get_sta_encrypt_algthm(priv, pstat);
                                        } else
                                        #endif
                                        {;}
                                    }
                                    else
                                    #endif /* defined(WDS) || defined(CONFIG_RTK_MESH) || defined(A4_STA) */
                                        {privacy = get_sta_encrypt_algthm(priv, pstat);}
                                }
                                #if defined(CLIENT_MODE)
                                else {
                                    privacy = get_sta_encrypt_algthm(priv, pstat);
                                }
                                #endif

                                if (privacy != _CCMP_PRIVACY_)
                                    rtl8192cd_ICV++;
                            }

                            if (rtl8192cd_ICV) {
                                rx_pkt_exception(priv, cmd);
                                pskb->data -= (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size);
                                skb_reset_tail_pointer(pskb);
                                #if !defined(DELAY_REFILL_RX_BUF) || !defined(CONFIG_RTL8190_PRIV_SKB) /*we create this, but we do not free it!*/
                                if (new_skb != NULL)
                                    rtl_kfree_skb(priv, new_skb, _SKB_RX_);
                                #endif
                                goto rx_reuse;
                            }

                        }

	//					printk("pkt in\n");
						SNMP_MIB_INC(dot11ReceivedFragmentCount, 1);

#if defined(SW_ANT_SWITCH)
						if(priv->pshare->rf_ft_var.antSw_enable) {
							dm_SWAW_RSSI_Check(priv, pfrinfo);
						}
#endif

#if defined(RX_BUFFER_GATHER)
						if (priv->pshare->gather_state != GATHER_STATE_NO) {
#if defined(RX_BUFFER_GATHER_REORDER)							
							list_add_tail(&pfrinfo->rx_list, priv->pshare->gather_list_p);
#else							
							list_add_tail(&pfrinfo->rx_list, &priv->pshare->gather_list);
#endif

							if (priv->pshare->gather_state == GATHER_STATE_LAST) {
								if (!search_first_segment(priv, &pfrinfo))
									reuse = 0;
								else {
									pskb = pfrinfo->pskb;
									pfrinfo_update = 1;
#if defined(RX_BUFFER_GATHER_REORDER)
									priv->pshare->gather_list_p = NULL;
#endif				
								}
								priv->pshare->gather_state = GATHER_STATE_NO;
							} else {
								reuse = 0;
							}
						}
						if (priv->pshare->gather_state == GATHER_STATE_NO && reuse)
#endif
						{
#if (BEAMFORMING_SUPPORT == 1) 
							unsigned char	 *pframe = get_pframe(pfrinfo);
							unsigned int	frtype = GetFrameSubType(pframe);

								if( frtype== Type_Action_No_Ack) {
									 priv->pshare->rf_ft_var.csi_counter++;
									 if(priv->pshare->rf_ft_var.dumpcsi)
										 priv->pshare->rf_ft_var.csi_counter %= priv->pshare->rf_ft_var.dumpcsi;
									 if( priv->pshare->rf_ft_var.dumpcsi &&
										priv->pshare->rf_ft_var.csi_counter==1)
									  {
#if 0									 
										 panic_printk("Action_no_Ack: %x %x %x %x\n",
										 	get_desc(pdesc->Dword0),
										 	get_desc(pdesc->Dword1),
										 	get_desc(pdesc->Dword2),
										 	get_desc(pdesc->Dword3)					 );
#endif										
										if ((pfrinfo->physt)&& (pfrinfo->driver_info_size > 0))	 {
											/* HH71VM: cast added. driver_info is declared as
											 * `struct RxFWInfo *` (8192cd_hw.h:775). The vendor himself
											 * core/8192cd_core_rx.h:110 writes exactly the same:
											 * `(unsigned char*)pfrinfo->driver_info`. Variable
											 * Moreover, it is not used here. */
											unsigned char *p __maybe_unused = (unsigned char *)pfrinfo->driver_info;
										}
									 }
								 reuse = 1;
								}
								else if(frtype == Type_NDPA) {
#if 0								 
									 panic_printk("Type_NDPA: %x %x %x %x\n",
									 	get_desc(pdesc->Dword0),
									 	get_desc(pdesc->Dword1),
									 	get_desc(pdesc->Dword2),
									 	get_desc(pdesc->Dword3)
									 );	
#endif
									Beamforming_GetNDPAFrame(priv, pframe);
#if 0								 
									 if ((pfrinfo->physt)&& (pfrinfo->driver_info_size > 0)) {
										panic_printk("pRtRfdStatus:(%d)\n", pfrinfo->driver_info_size);
										printHex(pfrinfo->driver_info, pfrinfo->driver_info_size);				
										panic_printk("\n");
									 }
#endif								 
								reuse = 1;
							} else
#endif
								reuse = validate_mpdu(priv, pfrinfo);
						}

						if (reuse) {
#if 1	/*Fix: bridge_handle_frame core dump, epc = 0x00000006*/
                        rtl_kfree_skb(priv, pskb, _SKB_RX_);
                        reuse = 0;
#else
							pskb->data -= (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size);
							skb_reset_tail_pointer(pskb);

#ifdef RX_BUFFER_GATHER
	                        if (pfrinfo->gather_flag & GATHER_FIRST){
	                            //flush_rx_list(priv);
	                            rtl_kfree_skb(priv, pskb, _SKB_RX_);
	                            reuse = 0;
	                            DEBUG_WARN("Gather-First packet error, free skb\n");
	                        }
#endif
#endif
						}
					}
					#undef	RTL_WLAN_DRV_RX_GATHER_GAP_THRESHOLD
				}
			}	/* if (cmd&XXXX) */

			if (!reuse) {
				phw->rx_infoL[tail].pbuf = NULL;        // clear pointer for not being accidently freed

#if 0
				// allocate new one
				pskb = rtl_dev_alloc_skb(priv, RX_BUF_LEN, _SKB_RX_);

				if (pskb == (struct sk_buff *)NULL)
				{
					DEBUG_WARN("out of skb_buff\n");
					list_del(&pfrinfo->rx_list);
					pskb = (struct sk_buff *)(phw->rx_infoL[tail].pbuf);
					pskb->data -= (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size);
					skb_reset_tail_pointer(pskb);
					goto rx_reuse;
				}
#endif

#if  defined(DELAY_REFILL_RX_BUF)
#if defined(CONFIG_RTL8190_PRIV_SKB)
#ifdef CONCURRENT_MODE
				if (skb_free_num[priv->pshare->wlandev_idx] == 0 && priv->pshare->skb_queue.qlen == 0)
#else
				if (skb_free_num == 0 && priv->pshare->skb_queue.qlen == 0)
#endif
				{
					refill = 0;
					goto rx_done;
				}
#endif
				new_skb = rtl_dev_alloc_skb(priv, RX_BUF_LEN, _SKB_RX_, 0);
				ASSERT(new_skb);
				if(new_skb == NULL) {
					printk("not enough memory...\n");
					refill = 0;
					goto rx_done;
				}
#endif
				pskb = new_skb;


#if defined(DELAY_REFILL_RX_BUF)
				RTL_WLAN_RX_ATOMIC_PROTECT_ENTER;
				init_rxdesc(pskb, phw->cur_rx_refill, priv);
				RTL_WLAN_RX_ATOMIC_PROTECT_EXIT;
#else
				init_rxdesc(pskb, tail, priv);
#endif

				goto rx_done;
			}	/* if (!reuse) */
#if !defined(DELAY_REFILL_RX_BUF)
			else {
				rtl_kfree_skb(priv, new_skb, _SKB_RX_);
			}
#endif

	rx_reuse:
#if defined(DELAY_REFILL_RX_BUF)
			RTL_WLAN_RX_ATOMIC_PROTECT_ENTER;
#if defined(RX_BUFFER_GATHER)
			cmp_flags = (tail != phw->cur_rx_refill) || (pfrinfo_update);
#else
			cmp_flags = (tail != phw->cur_rx_refill);
#endif
			RTL_WLAN_RX_ATOMIC_PROTECT_EXIT;

#if defined(RX_BUFFER_GATHER)
		if(pfrinfo && (priv->pshare->gather_state != GATHER_STATE_NO)) {
					flush_rx_list(priv);
					priv->pshare->gather_state = GATHER_STATE_NO;
		}	
#endif

			if (cmp_flags)
			{
				phw->rx_infoL[tail].pbuf = NULL;        // clear pointer for not being accidently freed
				pskb->data = pskb->head;
				skb_reset_tail_pointer(pskb);
				skb_reserve(pskb, NET_SKB_PAD);
#if defined(CONFIG_RTL8196_RTL8366)
				skb_reserve(pskb, 8);
#endif
#if defined(CONFIG_RTK_VOIP_VLAN_ID)
				skb_reserve(pskb, 4);
#endif
				refill_rx_ring(priv, pskb, NULL);
				refill = 0;
			} else
#endif	/*	defined(DELAY_REFILL_RX_BUF)	*/
			{
#if defined(RX_BUFFER_GATHER)
				pfrinfo->gather_flag = 0;
#endif
				//SMP_LOCK_SKB(x);
#if defined(CONFIG_NET_PCI) && !defined(USE_RTL8186_SDK)
				if (IS_PCIBIOS_TYPE) {
					phw->rx_infoL[tail].paddr = get_physical_addr(priv, pskb->data, (RX_BUF_LEN - sizeof(struct rx_frinfo)), PCI_DMA_FROMDEVICE);
				}
#endif
				pdesc->Dword6 = set_desc(phw->rx_infoL[tail].paddr);
#if defined(CONFIG_NET_PCI) && !defined(USE_RTL8186_SDK)
				// Remove it because pci_map_single() in get_physical_addr() already performed memory sync.
				//rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(phw->rx_infoL[tail].paddr), RX_BUF_LEN - sizeof(struct rx_frinfo)-64, PCI_DMA_FROMDEVICE);
#else
				rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(phw->rx_infoL[tail].paddr-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), RX_BUF_LEN - sizeof(struct rx_frinfo)-64, PCI_DMA_FROMDEVICE);
#endif
				pdesc->Dword0 = set_desc((tail == (NUM_RX_DESC_IF(priv) - 1)? RX_EOR : 0) | RX_OWN | (RX_BUF_LEN - sizeof(struct rx_frinfo)-64));
				//SMP_UNLOCK_SKB(x);
			}

	rx_done:
			RTL_WLAN_RX_ATOMIC_PROTECT_ENTER;
			//tail = (tail + 1) % NUM_RX_DESC;
			tail = ( ((tail+1)==NUM_RX_DESC_IF(priv))?0:tail+1);
			phw->cur_rx = tail;

#if defined(DELAY_REFILL_RX_BUF)
			if (refill) {
				//phw->cur_rx_refill = (phw->cur_rx_refill + 1) % NUM_RX_DESC;
				phw->cur_rx_refill = ( ((phw->cur_rx_refill+1)==NUM_RX_DESC_IF(priv))?0:phw->cur_rx_refill+1);
			}
#endif
			RTL_WLAN_RX_ATOMIC_PROTECT_EXIT;
		}	/*	while(1)	*/
		SMP_UNLOCK_RECV(y);

		if (!IS_DRV_OPEN(priv))
			return;

#if defined(RTL8190_ISR_RX) && defined(RTL8190_DIRECT_RX)
		if (OPMODE & WIFI_AP_STATE) {
			if (!list_empty(&priv->wakeup_list))
				process_dzqueue(priv);

#if defined(MBSSID)
			if (priv->pmib->miscEntry.vap_enable) {
				int i;
				struct rtl8192cd_priv *priv_vap;

				for (i=0; i<RTL8192CD_NUM_VWLAN; i++) {
					if (IS_DRV_OPEN(priv->pvap_priv[i])) {
						priv_vap = priv->pvap_priv[i];
						if (!list_empty(&priv_vap->wakeup_list))
							process_dzqueue(priv_vap);
					}
				}
			}
#endif
		}
#ifdef UNIVERSAL_REPEATER
		else {
			if (IS_DRV_OPEN(GET_VXD_PRIV(priv)))
				if (!list_empty(&GET_VXD_PRIV(priv)->wakeup_list))
					process_dzqueue(GET_VXD_PRIV(priv));
		}
#endif
#endif	/*	defined(RTL8190_ISR_RX) && defined(RTL8190_DIRECT_RX)	*/

		if(priv->pshare->skip_mic_chk)
			--priv->pshare->skip_mic_chk;
	}
}

