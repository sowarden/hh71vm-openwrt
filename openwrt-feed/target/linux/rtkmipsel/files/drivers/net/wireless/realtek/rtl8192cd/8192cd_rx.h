/*
 *  Header files defines some RX inline routines
 *
 *  Copyright (c) 2017 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef _8192CD_RX_H_
#define _8192CD_RX_H_

#include "./8192cd_cfg.h"
#include "./8192cd.h"
#include "./8192cd_util.h"
#ifdef CONFIG_DUAL_CPU
#include "dual_cpu.h"
#endif

#if defined(CONFIG_RG_WLAN_HWNAT_ACCELERATION) && !defined(CONFIG_ARCH_LUNA_SLAVE)
#include "rtk_rg_wlan.h"
#endif

#include "./core/8192cd_core_rx.h"



extern int rtl8192cd_rx_procCtrlPkt(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo
#ifdef MBSSID
				,int vap_idx
#endif
				);

extern int rtl8192cd_rx_dispatch_mgmt_adhoc(struct rtl8192cd_priv **priv_p, struct rx_frinfo *pfrinfo
#ifdef MBSSID
				,int vap_idx
#endif
				);

extern int rtl8192cd_rx_dispatch_fromDs(struct rtl8192cd_priv **priv_p, struct rx_frinfo *pfrinfo
#ifdef MBSSID
				,int vap_idx
#endif
);

extern void set_csi_report_info(struct rtl8192cd_priv *priv, unsigned char *pframe, unsigned int pktlen);

extern int deauth_filter(struct rtl8192cd_priv *priv, unsigned char *sa);

extern int GBWC_forward_check(struct rtl8192cd_priv *priv, struct sk_buff *pskb, struct stat_info *pstat);
extern int SBWC_forward_check(struct rtl8192cd_priv *priv, struct sk_buff *pskb, struct stat_info *pstat);
extern unsigned compare_ether_addr(const u8 *addr1, const u8 *addr2);
extern int rtl_IsMcastIP(struct sk_buff *pskb);
extern int gothrough_brsrc(struct rtl8192cd_priv *priv, struct sk_buff *pskb, struct stat_info *pstat);
extern void check_igmp_snooping_pkt( struct sk_buff *pskb );
extern void CheckUDPandU2M(struct sk_buff *pskb);
extern void CheckV6UDPandU2M(struct sk_buff *pskb);

#if (defined(__LINUX_2_6__) || defined(__ECOS)) && defined(CONFIG_RTL_IGMP_SNOOPING)
	/*added by qinjunjie,to avoid igmpv1/v2 report suppress*/
extern int rtl8192cd_isIgmpV1V2Report(unsigned char *macFrame);
extern int rtl8192cd_isMldV1V2Report(unsigned char *macFrame);
#endif
#ifdef WDS
extern int rtl8192cd_rx_dispatch_wds(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo);
#endif
#ifdef DELAY_REFILL_RX_BUF
#ifdef CONFIG_WLAN_HAL
extern int refill_rx_ring_88XX(struct rtl8192cd_priv * priv, struct sk_buff * skb, unsigned char * data, unsigned int q_num, PHCI_RX_DMA_QUEUE_STRUCT_88XX cur_q);
#endif
extern int refill_rx_ring(struct rtl8192cd_priv *priv, struct sk_buff *skb, unsigned char *data);
#endif

void check_mld_snooping_pkt(struct sk_buff *pskb);

#ifdef CONFIG_PCI_HCI

static __inline__ void init_rxdesc(struct sk_buff *pskb, int i, struct rtl8192cd_priv *priv)
{
	struct rtl8192cd_hw	*phw;
	struct rx_frinfo	*pfrinfo;
	int offset;

	phw = GET_HW(priv);

	offset = 0x20 - ((((unsigned long)pskb->data) + sizeof(struct rx_frinfo)) & 0x1f);	// need 32 byte aligned
	skb_reserve(pskb, sizeof(struct rx_frinfo) + offset);
	pfrinfo = get_pfrinfo(pskb);

	init_frinfo(pfrinfo);
#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
	pfrinfo->is_br_mgnt = 0;
#endif
#ifdef RX_BUFFER_GATHER
	pfrinfo->gather_flag = 0;
#endif

#ifdef __ECOS
#ifdef DELAY_REFILL_RX_BUF
	pskb->priv = priv;
#endif
#endif

	phw->rx_infoL[i].pbuf  = (void *)pskb;
	phw->rx_infoL[i].paddr = get_physical_addr(priv, pskb->data, (RX_BUF_LEN - sizeof(struct rx_frinfo) - 64), PCI_DMA_FROMDEVICE);
	phw->rx_descL[i].Dword6 = set_desc(phw->rx_infoL[i].paddr);
#if defined(CONFIG_NET_PCI) && !defined(USE_RTL8186_SDK)
	// Remove it because pci_map_single() in get_physical_addr() already performed memory sync.
	//rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(phw->rx_infoL[i].paddr), RX_BUF_LEN - sizeof(struct rx_frinfo)-64, PCI_DMA_FROMDEVICE);
#else
#ifdef CONFIG_ENABLE_NCBUFF
        if(skb_ncbuff_tag(pskb)->tag != NUBUFF_CACHE_TAG)
#endif
	rtl_cache_sync_wback(priv, (unsigned long)(phw->rx_infoL[i].paddr), RX_BUF_LEN - sizeof(struct rx_frinfo)-64, PCI_DMA_FROMDEVICE);
#endif
	phw->rx_descL[i].Dword0 = set_desc((i == (NUM_RX_DESC_IF(priv) - 1)? RX_EOR : 0) | RX_OWN |((RX_BUF_LEN - sizeof(struct rx_frinfo)-64) & RX_PktLenMask));	//32 for alignment, 32 for TKIP MIC

}
#endif // CONFIG_PCI_HCI




#endif // _8192CD_RX_H_

