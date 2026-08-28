/*
 *  SDIO core routines
 *
 *  Copyright (c) 2017 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _HAL_INTF_XMIT_C_

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#endif

#include "./8192cd.h"
#include "./8192cd_headers.h"
#include "./8192cd_debug.h"

#if defined(CONFIG_TRIBAND_MESH)
#define mesh_debug_tx4(priv, txcfg) {do {} while(0);}
#endif

#ifdef WMM_APSD
const unsigned char wmm_apsd_bitmask[MAX_STA_TX_SERV_QUEUE] = {
	0,		// MGNT_QUEUE
	0x04,	// BK_QUEUE
	0x08,	// BE_QUEUE
	0x02,	// VI_QUEUE
	0x01	// VO_QUEUE
};
#endif

#ifdef TX_SHORTCUT
#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
int get_tx_sc_index_usb(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *hdr, unsigned char pktpri)
#else
int get_tx_sc_index(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *hdr, unsigned char pktpri)
#endif
{
	struct tx_sc_entry *ptxsc_entry;
	int i;

	ptxsc_entry = pstat->TX_SC_ENT[pktpri];
	
	for (i=0; i<TX_SC_ENTRY_NUM; i++) {
#ifdef MCAST2UI_REFINE          
		if  ((OPMODE & WIFI_AP_STATE)
#ifdef WDS			
				&& !(pstat->state & WIFI_WDS)
#endif				
			) {		
			if (!memcmp(hdr+6, &ptxsc_entry[i].ethhdr.saddr, sizeof(struct wlan_ethhdr_t)-6)) 
				return i;							
		}
		else				
#endif
		{
			if (!memcmp(hdr, &ptxsc_entry[i].ethhdr, sizeof(struct wlan_ethhdr_t)))
				return i;
		}
	}

	return -1;
}

#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
int get_tx_sc_free_entry_usb(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *hdr, unsigned char pktpri)
#else
int get_tx_sc_free_entry(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *hdr, unsigned char pktpri)
#endif
{
	struct tx_sc_entry *ptxsc_entry;
	int i;

#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
	i = get_tx_sc_index_usb(priv, pstat, hdr, pktpri);
#else
	i = get_tx_sc_index(priv, pstat, hdr, pktpri);
#endif
	if (i >= 0)
		return i;
	
	ptxsc_entry = pstat->TX_SC_ENT[pktpri];
	
	for (i=0; i<TX_SC_ENTRY_NUM; i++) {
		if (ptxsc_entry[i].txcfg.fr_len == 0)
			return i;
	}
	
	// no free entry
	i = pstat->TX_SC_REPLACE_IDX[pktpri];
	pstat->TX_SC_REPLACE_IDX[pktpri] = (i+1) % TX_SC_ENTRY_NUM;
	return i;
}
#endif // TX_SHORTCUT

u32 rtw_is_tx_queue_empty(struct rtl8192cd_priv *priv, struct tx_insn *txcfg)
{
	int empty = TRUE;
	
	if (txcfg->pstat) {
		if (tx_servq_len(&txcfg->pstat->tx_queue[txcfg->q_num]) > 0)
			empty = FALSE;
	} else {	// txcfg->pstat == NULL
		if (MGNT_QUEUE == txcfg->q_num) {
			if (tx_servq_len(&priv->tx_mgnt_queue) > 0)
				empty = FALSE;
		} else if (tx_servq_len(&priv->tx_mc_queue) > 0)
			empty = FALSE;
	}
	
	return empty;
}

// Note. update_txinsn_stage1() must be fulfilled first before calling
int rtw_xmit_enqueue(struct rtl8192cd_priv *priv, struct tx_insn *txcfg)
{
	struct xmit_frame *pxmitframe;
	int ret;
	
#ifdef CONFIG_SDIO_TX_FILTER_BY_PRI
	if (OPMODE & WIFI_AP_STATE) {
		if (priv->pshare->rf_ft_var.tx_filter_enable && !priv->pshare->iot_mode_enable) {
			int qlen = priv->pshare->free_xmit_queue.qlen;
			switch (txcfg->q_num) {
				case VO_QUEUE:
				if (1 > qlen) {
					priv->pshare->nr_tx_filter_vo++;
						return FALSE;
				}
					break;
				case VI_QUEUE:
				if (41 > qlen) {
					priv->pshare->nr_tx_filter_vi++;
						return FALSE;
				}
					break;
				case BE_QUEUE:
				if (81 > qlen) {
					priv->pshare->nr_tx_filter_be++;
						return FALSE;
				}
					break;
				case BK_QUEUE:
				if (121 > qlen) {
					priv->pshare->nr_tx_filter_bk++;
						return FALSE;
				}
					break;
			}
		}
	}
#endif

	if (NULL == (pxmitframe = rtw_alloc_xmitframe(priv))) {
		DEBUG_WARN("No more xmitframe\n");
		return FALSE;
	}
	
	memcpy(&pxmitframe->txinsn, txcfg, sizeof(struct tx_insn));
	pxmitframe->priv = priv;
	
	if (txcfg->is_urgent)
		ret = rtw_enqueue_urg_xmitframe(priv, pxmitframe, ENQUEUE_TO_TAIL);
	else
		ret = rtw_enqueue_xmitframe(priv, pxmitframe, ENQUEUE_TO_TAIL);
	if (FALSE == ret) {
		priv->ext_stats.tx_drops++;
		DEBUG_ERR("TX DROP: %s failed!\n", __func__);
		rtw_free_xmitframe(priv, pxmitframe);
		return FALSE;
	}
	
	return TRUE;
}

// Note. for __rtl8192cd_firetx(),  it not release any xmitbuf resource no matter return value is success or fail
void rtw_handle_xmit_fail(struct rtl8192cd_priv *priv, struct tx_insn *txcfg)
{
	struct xmit_buf *pxmitbuf = txcfg->pxmitbuf;
	
	if (pxmitbuf) {
		// handle error case for the first user that hold xmitbuf
		if ((txcfg == pxmitbuf->agg_start_with) && pxmitbuf->use_hw_queue) {
			txcfg->pxmitbuf = NULL;
			pxmitbuf->use_hw_queue = 0;
			
#ifdef CONFIG_SDIO_TX_AGGREGATION
			if (pxmitbuf->flags & XMIT_BUF_FLAG_REUSE) {
				unsigned long flags;
				
				xmit_preempt_disable(flags);
				
				// Release the ownership of the HW TX queue
				clear_bit(pxmitbuf->q_num, &priv->pshare->use_hw_queue_bitmap);
				/* Enqueue the pxmitbuf, and it will be dequeued by a xmit thread later */
				enqueue_pending_xmitbuf(priv, pxmitbuf);
				
				xmit_preempt_enable(flags);
			} else
#endif // CONFIG_SDIO_TX_AGGREGATION
			{
				// Release the ownership of the HW TX queue
				clear_bit(pxmitbuf->q_num, &priv->pshare->use_hw_queue_bitmap);
				
				rtw_free_xmitbuf(priv, pxmitbuf);
				
				// schedule xmit_tasklet to avoid the same source TX queue not be handled
				// because this queue had enqueued packet during this processing
				if (FALSE == priv->pshare->bDriverStopped)
					tasklet_hi_schedule(&priv->pshare->xmit_tasklet);
			}
		}
	}
}

static int _rtw_xmit_decision(struct rtl8192cd_priv *priv, struct tx_insn *txcfg)
{
	if (test_and_set_bit(txcfg->q_num, &priv->pshare->use_hw_queue_bitmap) == 0)
	{
		// Got the ownership of the corresponding HW TX queue
		if (NULL == txcfg->pxmitbuf) {
			struct xmit_buf *pxmitbuf;
			
			if (txcfg->is_urgent) {
				pxmitbuf = rtw_alloc_urg_xmitbuf(priv, (u8)txcfg->q_num);
			} else if (txcfg->fr_type == _SKB_FRAME_TYPE_) {
#if 0//def CONFIG_SDIO_TX_AGGREGATION
				pxmitbuf = get_usable_pending_xmitbuf(priv, txcfg);
				if (NULL == pxmitbuf)
#endif
					pxmitbuf = rtw_alloc_xmitbuf(priv, (u8)txcfg->q_num);
			} else {
				pxmitbuf = rtw_alloc_xmitbuf_ext(priv, (u8)txcfg->q_num);
			}
			
			if (NULL == pxmitbuf) {
				// Release the ownership of the HW TX queue
				clear_bit(txcfg->q_num, &priv->pshare->use_hw_queue_bitmap);
				
				if (rtw_xmit_enqueue(priv, txcfg) == FALSE) {
					return XMIT_DECISION_STOP;
				}
				return XMIT_DECISION_ENQUEUE;
			}
			
			pxmitbuf->agg_start_with = txcfg;
			txcfg->pxmitbuf = pxmitbuf;
		} else {
			BUG_ON((txcfg->pxmitbuf->q_num != txcfg->q_num) ||txcfg->pxmitbuf->use_hw_queue);
		}
		
		txcfg->pxmitbuf->use_hw_queue = 1;
	}
	else if (NULL == txcfg->pxmitbuf) {
		if (rtw_xmit_enqueue(priv, txcfg) == FALSE) {
			return XMIT_DECISION_STOP;
		}
		return XMIT_DECISION_ENQUEUE;
	}
	else {
		BUG_ON(0 == txcfg->pxmitbuf->use_hw_queue);
	}
	
	return XMIT_DECISION_CONTINUE;
}

int rtw_xmit_decision(struct rtl8192cd_priv *priv, struct tx_insn *txcfg)
{
	unsigned long flags;
	int res;
	
	xmit_preempt_disable(flags);
	res = _rtw_xmit_decision(priv, txcfg);
	xmit_preempt_enable(flags);
	
	return res;
}

int dz_queue_usb(struct rtl8192cd_priv *priv, struct stat_info *pstat, struct tx_insn *txcfg)
{
	if (pstat)
	{
		if (0 == pstat->expire_to)
			return FALSE;
		
		if (rtw_xmit_enqueue(priv, txcfg) == TRUE) {
		return TRUE;
	}
	}
	else {	// Multicast or Broadcast or class 1 frame
		if (rtw_xmit_enqueue(priv, txcfg) == TRUE) {
			priv->pkt_in_dtimQ = 1;
			return TRUE;
		}
	}

	return FALSE;
}

#define MAX_FRAG_NUM		16
int update_txinsn_stage1(struct rtl8192cd_priv *priv, struct tx_insn* txcfg)
{
	struct sk_buff 	*pskb=NULL;
	unsigned short  protocol;
	unsigned char   *da=NULL;
	struct stat_info	*pstat=NULL;
	int priority=0;
	
	if (txcfg->aggre_en == FG_AGGRE_MSDU_MIDDLE || txcfg->aggre_en == FG_AGGRE_MSDU_LAST)
		return TRUE;

	/*---frag_thrshld setting---plus tune---0115*/
#ifdef	WDS
	if (txcfg->wdsIdx >= 0){
		txcfg->frag_thrshld = 2346; // if wds, disable fragment
	}else
#endif
#ifdef CONFIG_RTK_MESH
	if(txcfg->is_11s) {
		txcfg->frag_thrshld = 2346; // if Mesh case, disable fragment
	} else
#endif
	{
		txcfg->frag_thrshld = FRAGTHRSLD - _CRCLNG_;
	}
	/*---frag_thrshld setting---end*/

	txcfg->rts_thrshld  = RTSTHRSLD;
	
	txcfg->privacy = txcfg->iv = txcfg->icv = txcfg->mic = 0;
	txcfg->frg_num = 0;
	txcfg->need_ack = 1;

	if (txcfg->fr_type == _SKB_FRAME_TYPE_)
	{
		pskb = ((struct sk_buff *)txcfg->pframe);
		txcfg->fr_len = pskb->len - WLAN_ETHHDR_LEN;
		
		protocol = ntohs(*((UINT16 *)((UINT8 *)pskb->data + ETH_ALEN*2)));
		if ((protocol + WLAN_ETHHDR_LEN) > WLAN_MAX_ETHFRM_LEN)
			txcfg->llc = sizeof(struct llc_snap);
		
#ifdef MP_TEST
		if (OPMODE & WIFI_MP_STATE) {
			txcfg->hdr_len = WLAN_HDR_A3_LEN;
			txcfg->frg_num = 1;
			if (IS_MCAST(pskb->data))
				txcfg->need_ack = 0;
			return TRUE;
		}
#endif

#ifdef WDS
		if (txcfg->wdsIdx >= 0) {
			pstat = get_stainfo(priv, priv->pmib->dot11WdsInfo.entry[txcfg->wdsIdx].macAddr);
			if (pstat == NULL) {
				DEBUG_ERR("TX DROP: %s: get_stainfo() for wds failed [%d]!\n", (char *)__FUNCTION__, txcfg->wdsIdx);
				return FALSE;
			}

			txcfg->privacy = priv->pmib->dot11WdsInfo.wdsPrivacy;
			switch (txcfg->privacy) {
				case _WEP_40_PRIVACY_:
				case _WEP_104_PRIVACY_:
					txcfg->iv = 4;
					txcfg->icv = 4;
					break;
				case _TKIP_PRIVACY_:
					txcfg->iv = 8;
					txcfg->icv = 4;
					txcfg->mic = 0;
					break;
				case _CCMP_PRIVACY_:
					txcfg->iv = 8;
					txcfg->icv = 0;
					txcfg->mic = 8;
					break;
			}
			txcfg->frg_num = 1;
			if (txcfg->aggre_en < FG_AGGRE_MSDU_FIRST) {
				priority = get_skb_priority(priv, pskb, pstat, 0);
				txcfg->q_num = pri_to_qnum(priv, priority);
			}
			
			txcfg->need_ack = 1;
			txcfg->pstat = pstat;
#ifdef WIFI_WMM
			if ((QOS_ENABLE) && (pstat->QosEnabled))
				txcfg->hdr_len = WLAN_HDR_A4_QOS_LEN;
			else
#endif
			txcfg->hdr_len = WLAN_HDR_A4_LEN;
		
			return TRUE;
		}
#endif // WDS

		if (OPMODE & WIFI_AP_STATE) {
#ifdef MCAST2UI_REFINE
			pstat = get_stainfo(priv, &pskb->cb[10]);
#else
			pstat = get_stainfo(priv, pskb->data);
#endif
#ifdef A4_STA
			if (pstat == NULL) {
				if (txcfg->pstat && (txcfg->pstat->state & WIFI_A4_STA)) 
					pstat = txcfg->pstat;
				else if (!IS_MCAST(pskb->data) &&  priv->pmib->miscEntry.a4_enable)
					pstat = a4_sta_lookup(priv, pskb->data);
				if (pstat) 
					da = pstat->cmn_info.mac_addr;
			}
#endif
		}
#ifdef CLIENT_MODE
		else if (OPMODE & WIFI_STATION_STATE)
			pstat = get_stainfo(priv, BSSID);
		else if (OPMODE & WIFI_ADHOC_STATE)
			pstat = get_stainfo(priv, pskb->data);
#endif

#ifdef WIFI_WMM
		if ((pstat) && (QOS_ENABLE) && (pstat->QosEnabled)) {
			txcfg->hdr_len = WLAN_HDR_A3_QOS_LEN;
		}
		else
#endif
		{
			txcfg->hdr_len = WLAN_HDR_A3_LEN;
		}

#ifdef CONFIG_RTK_MESH
		if(txcfg->is_11s)
		{
			txcfg->hdr_len = WLAN_HDR_A4_QOS_LEN ;
			da = txcfg->nhop_11s;
		}
		else
#endif

#ifdef A4_STA
		if (!da)
#endif
#ifdef MCAST2UI_REFINE
			da = &pskb->cb[10];
#else
			da = pskb->data;
#endif

		//check if da is associated, if not, just drop and return false
		if (!IS_MCAST(da)
#ifdef CLIENT_MODE
			|| (OPMODE & WIFI_STATION_STATE)
#endif
#ifdef A4_STA
			|| (pstat && (pstat->state & WIFI_A4_STA))
#endif
			)
		{
#if defined(CONFIG_TRIBAND_MESH)
		#ifdef CLIENT_MODE
			if (OPMODE & WIFI_STATION_STATE)
				pstat = get_stainfo(priv, BSSID);
			else
		#endif
			{
				if( !txcfg->pstat ) 
				    pstat = get_stainfo(priv, da);
				else
				    pstat = txcfg->pstat;
			}
#endif /* defined(CONFIG_TRIBAND_MESH) */

			if ((pstat == NULL) || (!(pstat->state & WIFI_ASOC_STATE)))
			{
				DEBUG_INFO("TX DROP: Non asoc tx request!\n");
				return FALSE;
			}
#ifdef A4_STA
			if (pstat->state & WIFI_A4_STA)
				txcfg->hdr_len += WLAN_ADDR_LEN;
#endif

			if (((protocol == 0x888E) && ((GET_UNICAST_ENCRYP_KEYLEN == 0)
#ifdef WIFI_SIMPLE_CONFIG
				|| (pstat->state & WIFI_WPS_JOIN)
#endif
				))
#ifdef CONFIG_RTL_WAPI_SUPPORT
				 || (protocol == ETH_P_WAPI)
#endif
#if (BEAMFORMING_SUPPORT == 1)
				|| (txcfg->ndpa) 
#endif
				)
				txcfg->privacy = 0;
			else
				txcfg->privacy = get_privacy(priv, pstat, &txcfg->iv, &txcfg->icv, &txcfg->mic);
			
			if ((OPMODE & WIFI_AP_STATE) && !IS_MCAST(da) && (isDHCPpkt(pskb) == TRUE))
				txcfg->is_dhcp = 1;
			
			if ((protocol == 0x888E)
#ifdef CONFIG_RTL_WAPI_SUPPORT
				|| (protocol == ETH_P_WAPI)
#endif
				|| txcfg->is_dhcp) {
				txcfg->is_urgent = 1;
			}
			
			if (txcfg->aggre_en < FG_AGGRE_MSDU_FIRST) {
				priority = get_skb_priority(priv, pskb, pstat, txcfg->is_11s);
				txcfg->q_num = pri_to_qnum(priv, priority);
			}
		}
		else
		{
			// if group key not set yet, don't let unencrypted multicast go to air
			if (priv->pmib->dot11GroupKeysTable.dot11Privacy) {
				if (GET_GROUP_ENCRYP_KEYLEN == 0) {
					DEBUG_ERR("TX DROP: group key not set yet!\n");
					return FALSE;
				}
			}

			txcfg->privacy = get_mcast_privacy(priv, &txcfg->iv, &txcfg->icv, &txcfg->mic);
			
			txcfg->q_num = priv->tx_mc_queue.q_num;
			pskb->cb[1] = 0;

			if ((*da) == 0xff)	// broadcast
				txcfg->tx_rate = find_rate(priv, NULL, 0, 1);
			else {				// multicast
				if (priv->pmib->dot11StationConfigEntry.lowestMlcstRate)
					txcfg->tx_rate = get_rate_from_bit_value(priv->pmib->dot11StationConfigEntry.lowestMlcstRate);
				else
					txcfg->tx_rate = find_rate(priv, NULL, 1, 1);
			}

#ifdef CONFIG_RTK_MESH
			mesh_debug_tx4(priv, txcfg);
#endif

			txcfg->lowest_tx_rate = txcfg->tx_rate;
			txcfg->fixed_rate = 1;
		}
	}
#ifdef _11s_TEST_MODE_	/*---11s mgt frame---*/
	else if (txcfg->is_11s)
		mesh_debug_tx6(priv, txcfg);
#endif
	
	if (!da)
	{
		// This is non data frame, no need to frag.
#ifdef CONFIG_RTK_MESH
		if(txcfg->is_11s)
			da = GetAddr1Ptr(txcfg->phdr);
		else
#endif
			da = get_da((unsigned char *) (txcfg->phdr));
		
#ifdef CLIENT_MODE
		if ((OPMODE & WIFI_AP_STATE) || (OPMODE & WIFI_ADHOC_STATE))
#else
		if (OPMODE & WIFI_AP_STATE)
#endif
		{
			if (!IS_MCAST(da) && (GetFrameSubType(txcfg->phdr) != WIFI_PROBERSP)) {
				pstat = get_stainfo(priv, da);
			}
		}
#ifdef CLIENT_MODE
		else if (OPMODE & WIFI_STATION_STATE)
		{
			pstat = get_stainfo(priv, BSSID);
		}
#endif

		txcfg->frg_num = 1;

		if (IS_MCAST(da))
			txcfg->need_ack = 0;
		else
			txcfg->need_ack = 1;

		if (GetPrivacy(txcfg->phdr))
		{
#ifdef CONFIG_IEEE80211W 
			if (txcfg->isPMF) {
				txcfg->privacy = _CCMP_PRIVACY_;
				txcfg->iv = 8;
				txcfg->icv = 0;
				txcfg->mic = 8;
			} else		
#endif
			{
				// only auth with legacy wep...
				txcfg->iv = 4;
				txcfg->icv = 4;
				txcfg->privacy = priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm;
			}
		}

#ifdef DRVMAC_LB
		if (GetFrameType(txcfg->phdr) == WIFI_MGT_TYPE)
#endif
		if (txcfg->fr_len != 0)	//for mgt frame
			txcfg->hdr_len += WLAN_HDR_A3_LEN;
	}
	
	txcfg->is_mcast = IS_MCAST(da);
	
#ifdef CLIENT_MODE
	if ((OPMODE & WIFI_AP_STATE) || (OPMODE & WIFI_ADHOC_STATE))
#else
	if (OPMODE & WIFI_AP_STATE)
#endif
	{
		if (IS_MCAST(da))
		{
			txcfg->frg_num = 1;
			txcfg->need_ack = 0;
			txcfg->rts_thrshld = DEFAULT_RTS_THRESHOLD;	// disable RTS protection for mcast frames
		}
		else
		{
			txcfg->pstat = pstat;
		}
	}
#ifdef CLIENT_MODE
	else if (OPMODE & WIFI_STATION_STATE)
	{
		txcfg->pstat = pstat;
	}
#endif

#if (BEAMFORMING_SUPPORT == 1)
	if((priv->pmib->dot11RFEntry.txbf == 1) && (pstat) &&
		((pstat->ht_cap_len && (pstat->ht_cap_buf.txbf_cap)) 
#ifdef RTK_AC_SUPPORT		
		||(pstat->vht_cap_len && (cpu_to_le32(pstat->vht_cap_buf.vht_cap_info) & BIT(SU_BFEE_S)))
#endif		
		))
		Beamforming_GidPAid(priv, pstat);
#endif

	txcfg->frag_thrshld -= (txcfg->mic + txcfg->iv + txcfg->icv + txcfg->hdr_len);

	if (txcfg->frg_num == 0)
	{
		if (txcfg->aggre_en > 0)
			txcfg->frg_num = 1;
		else {
			// how many mpdu we need...
			int size;
			
			size = txcfg->fr_len + txcfg->llc + ((_TKIP_PRIVACY_ == txcfg->privacy) ? 8 : 0);
			txcfg->frg_num = (size + txcfg->frag_thrshld -1) / txcfg->frag_thrshld;
			if (unlikely(txcfg->frg_num > MAX_FRAG_NUM)) {
				txcfg->frag_thrshld = 2346;
				txcfg->frg_num = 1;
			}
		}
	}

#ifdef RTK_SMART_ROAMING
	if (pstat && pstat->prepare_to_free) {
		txcfg->is_urgent = 0;
	}
#endif

	return TRUE;
}

int update_txinsn_stage2(struct rtl8192cd_priv *priv, struct tx_insn* txcfg)
{
	struct sk_buff 	*pskb=NULL;
	unsigned short  protocol;
	struct stat_info	*pstat=NULL;
	int priority=0;

	if (txcfg->aggre_en == FG_AGGRE_MSDU_MIDDLE || txcfg->aggre_en == FG_AGGRE_MSDU_LAST)
		return TRUE;
	
	if (txcfg->fr_type == _SKB_FRAME_TYPE_)
	{
#ifdef MP_TEST
		if (OPMODE & WIFI_MP_STATE) {
			return TRUE;
		}
#endif
		pskb = ((struct sk_buff *)txcfg->pframe);
		pstat = txcfg->pstat;

#ifdef WDS
		if (txcfg->wdsIdx >= 0) {
			if (txcfg->privacy == _TKIP_PRIVACY_)
				txcfg->fr_len += 8;	// for Michael padding.
				
			txcfg->tx_rate = get_tx_rate(priv, pstat);
			txcfg->lowest_tx_rate = get_lowest_tx_rate(priv, pstat, txcfg->tx_rate);
			if (priv->pmib->dot11WdsInfo.entry[pstat->wds_idx].txRate)
				txcfg->fixed_rate = 1;

			if (txcfg->aggre_en == 0) {
				if ((pstat->aggre_mthd == AGGRE_MTHD_MPDU) && is_MCS_rate(txcfg->tx_rate))
					txcfg->aggre_en = FG_AGGRE_MPDU;
			}

			return TRUE;
		}
#endif

		//check if da is associated, if not, just drop and return false
		if (!txcfg->is_mcast
#ifdef CLIENT_MODE
			|| (OPMODE & WIFI_STATION_STATE)
#endif
#ifdef A4_STA
			|| (pstat && (pstat->state & WIFI_A4_STA))
#endif		
			)
		{
			protocol = ntohs(*((UINT16 *)((UINT8 *)pskb->data + ETH_ALEN*2)));

			if ((protocol == 0x888E)
#ifdef CONFIG_RTL_WAPI_SUPPORT
				||(protocol == ETH_P_WAPI)
#endif
				|| txcfg->is_dhcp) {
				// use basic rate to send EAP packet for sure
				txcfg->tx_rate = find_rate(priv, NULL, 0, 1);
				txcfg->lowest_tx_rate = txcfg->tx_rate;
				txcfg->fixed_rate = 1;
			} else {
				txcfg->tx_rate = get_tx_rate(priv, pstat);
				txcfg->lowest_tx_rate = get_lowest_tx_rate(priv, pstat, txcfg->tx_rate);
				if (!is_auto_rate(priv, pstat)&&
					!(should_restrict_Nrate(priv, pstat) && is_fixedMCSTxRate(priv, pstat)))
					txcfg->fixed_rate = 1;
			}

			if (txcfg->aggre_en == 0
#ifdef SUPPORT_TX_MCAST2UNI
					&& (priv->pshare->rf_ft_var.mc2u_disable || (pskb->cb[2] != (char)0xff))
#endif
				) {
				if ((pstat->aggre_mthd == AGGRE_MTHD_MPDU) &&
				/*	is_MCS_rate(txcfg->tx_rate) &&*/ (protocol != 0x888E)
#ifdef CONFIG_RTL_WAPI_SUPPORT
					&& (protocol != ETH_P_WAPI)
#endif
					&& !txcfg->is_dhcp)
					txcfg->aggre_en = FG_AGGRE_MPDU;
			}

			if (
#ifdef RTK_AC_SUPPORT //FOR_VHT5G_PF
				(txcfg->aggre_en && pstat && 
				((pstat->aggre_mthd == AGGRE_MTHD_MPDU_AMSDU) || (pstat->aggre_mthd == AGGRE_MTHD_MPDU))) ||
#endif
				(txcfg->aggre_en >= FG_AGGRE_MPDU && txcfg->aggre_en <= FG_AGGRE_MPDU_BUFFER_LAST))
			{
				priority = pskb->cb[1];
				if (!pstat->ADDBA_ready[priority]) {
					if ((pstat->ADDBA_req_num[priority] < 5) && !pstat->ADDBA_sent[priority]) {
						pstat->ADDBA_req_num[priority]++;
						issue_ADDBAreq(priv, pstat, priority);
						pstat->ADDBA_sent[priority]++;
					}
				}
			}

			if (txcfg->is_pspoll && (tx_servq_len(&pstat->tx_queue[BE_QUEUE]) > 0)) {
				SetMData(txcfg->phdr);
			}
			
#ifdef WMM_APSD
			if (pstat->apsd_trigger) {
				if (((pstat->apsd_bitmap & 0x01) && tx_servq_len(&pstat->tx_queue[VO_QUEUE]))
						|| ((pstat->apsd_bitmap & 0x02) && tx_servq_len(&pstat->tx_queue[VI_QUEUE]))
						|| ((pstat->apsd_bitmap & 0x04) && tx_servq_len(&pstat->tx_queue[BK_QUEUE]))
						|| ((pstat->apsd_bitmap & 0x08) && tx_servq_len(&pstat->tx_queue[BE_QUEUE]))) {
					SetMData(txcfg->phdr);
				} else {
					pstat->apsd_trigger = 0;
				}
			}
#endif
		} else {
			if ((OPMODE & WIFI_AP_STATE) && !list_empty(&priv->sleep_list)
					&& (tx_servq_len(&priv->tx_mc_queue) > 0)) {
				SetMData(txcfg->phdr);
			}
		}
	}

	if (txcfg->privacy == _TKIP_PRIVACY_)
		txcfg->fr_len += 8;	// for Michael padding.

	if (txcfg->aggre_en > 0) {
		txcfg->frg_num = 1;
		txcfg->frag_thrshld = 2346;
	}
	
	return TRUE;
}

int rtl8192cd_firetx_usb(struct rtl8192cd_priv *priv, struct tx_insn* txcfg)
{
	unsigned long flags;
	int retval;
#ifdef CONFIG_POWER_SAVE
	unsigned short fr_type;
#endif

#ifdef DFS
	if (!priv->pmib->dot11DFSEntry.disable_DFS &&
		GET_ROOT(priv)->pmib->dot11DFSEntry.disable_tx) {
		DEBUG_ERR("TX DROP: DFS probation period\n");
		return CONGESTED;
	}
#endif

#ifdef SDIO_AP_OFFLOAD
	if (priv->pshare->offload_function_ctrl)
		return CONGESTED;
	
#ifdef CONFIG_POWER_SAVE
	fr_type = GetFrameSubType(txcfg->phdr);

	if ((WIFI_PROBEREQ != fr_type) && (WIFI_PROBERSP != fr_type))
		rtw_lock_suspend_timeout(priv, 2000);
#endif
#endif

	if (update_txinsn_stage1(priv, txcfg) == FALSE) {
		return CONGESTED;
	}
	
	txcfg->next_txpath = TXPATH_FIRETX;
	xmit_preempt_disable(flags);
	if (rtw_is_tx_queue_empty(priv, txcfg) == FALSE) {
		if (rtw_xmit_enqueue(priv, txcfg) == FALSE) {
			xmit_preempt_enable(flags);
			return CONGESTED;
		}
		xmit_preempt_enable(flags);
		return SUCCESS;
	}
	xmit_preempt_enable(flags);
	
	retval = __rtl8192cd_firetx(priv, txcfg);
	
	rtw_handle_xmit_fail(priv, txcfg);
	
	return retval;
}

#ifndef CONFIG_NETDEV_MULTI_TX_QUEUE
void rtl8192cd_tx_restartQueue(struct rtl8192cd_priv *priv)
{
	priv = GET_ROOT(priv);

	if (IS_DRV_OPEN(priv)) {
		netif_wake_queue(priv->dev);
	}

#ifdef UNIVERSAL_REPEATER
	if (IS_DRV_OPEN(GET_VXD_PRIV(priv))) {
		netif_wake_queue(GET_VXD_PRIV(priv)->dev);
	}
#endif

#ifdef MBSSID
	if (priv->pmib->miscEntry.vap_enable) {
		int bssidIdx;
		for (bssidIdx=0; bssidIdx<RTL8192CD_NUM_VWLAN; bssidIdx++) {
			if (IS_DRV_OPEN(priv->pvap_priv[bssidIdx])) {
				netif_wake_queue(priv->pvap_priv[bssidIdx]->dev);
			}
		}
	}
#endif

#ifdef CONFIG_RTK_MESH
	if (priv->pmib->dot1180211sInfo.mesh_enable) {
		if (netif_running(priv->mesh_dev)) {
			netif_wake_queue(priv->mesh_dev);
		}
	}
#endif
#ifdef WDS
	if (priv->pmib->dot11WdsInfo.wdsEnabled) {
		int i;
		for (i=0; i<priv->pmib->dot11WdsInfo.wdsNum; i++) {
			if (netif_running(priv->wds_dev[i])) {
				netif_wake_queue(priv->wds_dev[i]);
			}
		}
	}
#endif
}

void rtl8192cd_tx_stopQueue(struct rtl8192cd_priv *priv)
{
	priv = GET_ROOT(priv);
	++priv->pshare->nr_stop_netif_tx_queue;

	if (IS_DRV_OPEN(priv)) {
		netif_stop_queue(priv->dev);
	}

#ifdef UNIVERSAL_REPEATER
	if (IS_DRV_OPEN(GET_VXD_PRIV(priv))) {
		netif_stop_queue(GET_VXD_PRIV(priv)->dev);
	}
#endif

#ifdef MBSSID
	if (priv->pmib->miscEntry.vap_enable) {
		int bssidIdx;
		for (bssidIdx=0; bssidIdx<RTL8192CD_NUM_VWLAN; bssidIdx++) {
			if (IS_DRV_OPEN(priv->pvap_priv[bssidIdx])) {
				netif_stop_queue(priv->pvap_priv[bssidIdx]->dev);
			}
		}
	}
#endif

#ifdef CONFIG_RTK_MESH
	if (priv->pmib->dot1180211sInfo.mesh_enable) {
		if (netif_running(priv->mesh_dev)) {
			netif_stop_queue(priv->mesh_dev);
		}
	}
#endif
#ifdef WDS
	if (priv->pmib->dot11WdsInfo.wdsEnabled) {
		int i;
		for (i=0; i<priv->pmib->dot11WdsInfo.wdsNum; i++) {
			if (netif_running(priv->wds_dev[i])) {
				netif_stop_queue(priv->wds_dev[i]);
			}
		}
	}
#endif
}

#else // CONFIG_NETDEV_MULTI_TX_QUEUE
#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
void rtl8192cd_tx_restartQueue_usb(struct rtl8192cd_priv *priv, unsigned int index)
#else
void rtl8192cd_tx_restartQueue(struct rtl8192cd_priv *priv, unsigned int index)
#endif
{
	priv = GET_ROOT(priv);

	if (IS_DRV_OPEN(priv)) {
		if (unlikely(_NETDEV_TX_QUEUE_ALL == index))
			netif_tx_wake_all_queues(priv->dev);
		else
			netif_tx_wake_queue(netdev_get_tx_queue(priv->dev, index));
	}

#ifdef UNIVERSAL_REPEATER
	if (IS_DRV_OPEN(GET_VXD_PRIV(priv))) {
		if (unlikely(_NETDEV_TX_QUEUE_ALL == index))
			netif_tx_wake_all_queues(GET_VXD_PRIV(priv)->dev);
		else
			netif_tx_wake_queue(netdev_get_tx_queue(GET_VXD_PRIV(priv)->dev, index));
	}
#endif

#ifdef MBSSID
	if (priv->pmib->miscEntry.vap_enable) {
		int bssidIdx;
		for (bssidIdx=0; bssidIdx<RTL8192CD_NUM_VWLAN; bssidIdx++) {
			if (IS_DRV_OPEN(priv->pvap_priv[bssidIdx])) {
				if (unlikely(_NETDEV_TX_QUEUE_ALL == index))
					netif_tx_wake_all_queues(priv->pvap_priv[bssidIdx]->dev);
				else
					netif_tx_wake_queue(netdev_get_tx_queue(priv->pvap_priv[bssidIdx]->dev, index));
			}
		}
	}
#endif

	if ((_NETDEV_TX_QUEUE_ALL != index) && (_NETDEV_TX_QUEUE_BE != index))
		return;

#ifdef CONFIG_RTK_MESH
	if (priv->pmib->dot1180211sInfo.mesh_enable) {
		if (netif_running(priv->mesh_dev)) {
			netif_wake_queue(priv->mesh_dev);
		}
	}
#endif
#ifdef WDS
	if (priv->pmib->dot11WdsInfo.wdsEnabled) {
		int i;
		for (i=0; i<priv->pmib->dot11WdsInfo.wdsNum; i++) {
			if (netif_running(priv->wds_dev[i])) {
				netif_wake_queue(priv->wds_dev[i]);
			}
		}
	}
#endif
}

void rtl8192cd_tx_stopQueue(struct rtl8192cd_priv *priv)
{
	priv = GET_ROOT(priv);
	++priv->pshare->nr_stop_netif_tx_queue;

	if (IS_DRV_OPEN(priv)) {
		netif_tx_stop_all_queues(priv->dev);
	}

#ifdef UNIVERSAL_REPEATER
	if (IS_DRV_OPEN(GET_VXD_PRIV(priv))) {
		netif_tx_stop_all_queues(GET_VXD_PRIV(priv)->dev);
	}
#endif

#ifdef MBSSID
	if (priv->pmib->miscEntry.vap_enable) {
		int bssidIdx;
		for (bssidIdx=0; bssidIdx<RTL8192CD_NUM_VWLAN; bssidIdx++) {
			if (IS_DRV_OPEN(priv->pvap_priv[bssidIdx])) {
				netif_tx_stop_all_queues(priv->pvap_priv[bssidIdx]->dev);
			}
		}
	}
#endif

#ifdef CONFIG_RTK_MESH
	if (priv->pmib->dot1180211sInfo.mesh_enable) {
		if (netif_running(priv->mesh_dev)) {
			netif_stop_queue(priv->mesh_dev);
		}
	}
#endif
#ifdef WDS
	if (priv->pmib->dot11WdsInfo.wdsEnabled) {
		int i;
		for (i=0; i<priv->pmib->dot11WdsInfo.wdsNum; i++) {
			if (netif_running(priv->wds_dev[i])) {
				netif_stop_queue(priv->wds_dev[i]);
			}
		}
	}
#endif
}
#endif // !CONFIG_NETDEV_MULTI_TX_QUEUE


static int activate_mc_queue_xmit(struct rtl8192cd_priv *priv, u8 force_tx)
{
	_irqL irqL;
	struct tx_servq *ptxservq;
	_queue *xframe_queue, *sta_queue;
	int active = 0;
	
	ptxservq = &priv->tx_mc_queue;
	xframe_queue = &ptxservq->xframe_queue;
	
	xmit_lock(&xframe_queue->lock, &irqL);
	
	if (_rtw_queue_empty(xframe_queue) == FALSE) {
		sta_queue = &priv->pshare->tx_pending_sta_queue[ptxservq->q_num];
		
		_rtw_spinlock(&sta_queue->lock);
		
		if (rtw_is_list_empty(&ptxservq->tx_pending) == TRUE) {
			rtw_list_insert_head(&ptxservq->tx_pending, &sta_queue->queue);
			++sta_queue->qlen;
			active = 1;
		}
		priv->release_mcast = force_tx;
		
		_rtw_spinunlock(&sta_queue->lock);
	}
	
	xmit_unlock(&xframe_queue->lock, &irqL);
	
	return active;
}

void stop_sta_xmit(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	int q_num, deactive;
	
	struct tx_servq *ptxservq;
	_queue *sta_queue;
	_irqL irqL;
	
#ifdef WMM_APSD
	if (pstat->apsd_trigger)
		return;
#endif

	for (q_num = 0; q_num < MAX_STA_TX_SERV_QUEUE; ++q_num) {
		ptxservq = &pstat->tx_queue[q_num];
		sta_queue = &priv->pshare->tx_pending_sta_queue[q_num];
		deactive = 0;
		
		xmit_lock(&sta_queue->lock, &irqL);
		
		if (rtw_is_list_empty(&ptxservq->tx_pending) == FALSE) {
			rtw_list_delete(&ptxservq->tx_pending);
			--sta_queue->qlen;
			deactive = 1;
		}
		
		xmit_unlock(&sta_queue->lock, &irqL);
		
		if (deactive)
			need_sched_xmit_for_dequeue(priv, q_num);
	}
	
	// for BC/MC frames
	ptxservq = &priv->tx_mc_queue;
	sta_queue = &priv->pshare->tx_pending_sta_queue[ptxservq->q_num];
	
	xmit_lock(&sta_queue->lock, &irqL);
	
	if (rtw_is_list_empty(&ptxservq->tx_pending) == FALSE) {
		rtw_list_delete(&ptxservq->tx_pending);
		--sta_queue->qlen;
	}
	ptxservq->q_num = MCAST_QNUM;
	
	xmit_unlock(&sta_queue->lock, &irqL);
	
	if (priv->release_mcast) {
		activate_mc_queue_xmit(priv, priv->release_mcast);
	}
}

void wakeup_sta_xmit(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	int q_num, active;
	struct tx_servq *ptxservq;
	_queue *xframe_queue, *sta_queue;
	_irqL irqL;
	
	for (q_num = 0; q_num < MAX_STA_TX_SERV_QUEUE; ++q_num) {
		ptxservq = &pstat->tx_queue[q_num];
		xframe_queue = &ptxservq->xframe_queue;
		
		xmit_lock(&xframe_queue->lock, &irqL);
		
		if (_rtw_queue_empty(xframe_queue) == FALSE) {
			sta_queue = &priv->pshare->tx_pending_sta_queue[q_num];
			active = 0;
			
			_rtw_spinlock(&sta_queue->lock);
			
			if (rtw_is_list_empty(&ptxservq->tx_pending) == TRUE) {
				rtw_list_insert_head(&ptxservq->tx_pending, &sta_queue->queue);
				++sta_queue->qlen;
#ifdef CONFIG_SDIO_HCI
				ptxservq->ts_used = 0;
#endif
				active = 1;
			}
			
			_rtw_spinunlock(&sta_queue->lock);
			
			if (active)
				need_sched_xmit_for_enqueue(priv, q_num);
		}
		
		xmit_unlock(&xframe_queue->lock, &irqL);
	}
	
#ifdef WMM_APSD
	pstat->apsd_trigger = 0;
#endif
	
	// for BC/MC frames
	if (list_empty(&priv->sleep_list)) {
		activate_mc_queue_xmit(priv, 0);
	}

#ifdef __ECOS
	triggered_wlan_tx_tasklet(priv);
#else
	tasklet_hi_schedule(&priv->pshare->xmit_tasklet);
#endif
}

void process_dzqueue_usb(struct rtl8192cd_priv *priv)
{
	struct stat_info *pstat;
	struct list_head *phead = &priv->wakeup_list;
	struct list_head *plist;
	unsigned long flags;
	
	while(1)
	{
		plist = NULL;
		
		SAVE_INT_AND_CLI(flags);
		SMP_LOCK_WAKEUP_LIST(flags);

		if (!list_empty(phead)) {
			plist = phead->next;
			list_del_init(plist);
		}

		SMP_UNLOCK_WAKEUP_LIST(flags);
		RESTORE_INT(flags);
		
		if (NULL == plist) break;
		
		pstat = list_entry(plist, struct stat_info, wakeup_list);
		
		DEBUG_INFO("Del fr wakeup_list %02X%02X%02X%02X%02X%02X\n",
			pstat->cmn_info.mac_addr[0],pstat->cmn_info.mac_addr[1],pstat->cmn_info.mac_addr[2],pstat->cmn_info.mac_addr[3],pstat->cmn_info.mac_addr[4],pstat->cmn_info.mac_addr[5]);
		
		wakeup_sta_xmit(priv, pstat);
	}
}

void process_mcast_dzqueue_usb(struct rtl8192cd_priv *priv)
{
	priv->tx_mc_pkt_num = 0;
	
	if (activate_mc_queue_xmit(priv, 1)) {
		need_sched_xmit_for_enqueue(priv, priv->tx_mc_queue.q_num);
	}
}

#ifdef WMM_APSD
void process_APSD_dz_queue_usb(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned short tid)
{
	int q_num;
	int active, need_sched;
	
	struct tx_servq *ptxservq;
	_queue *xframe_queue, *sta_queue;
	_irqL irqL;

	if ((((tid == 7) || (tid == 6)) && !(pstat->apsd_bitmap & 0x01))
			|| (((tid == 5) || (tid == 4)) && !(pstat->apsd_bitmap & 0x02))
			|| (((tid == 3) || (tid == 0)) && !(pstat->apsd_bitmap & 0x08))
			|| (((tid == 2) || (tid == 1)) && !(pstat->apsd_bitmap & 0x04))) {
		DEBUG_INFO("RcvQosNull legacy ps tid=%d", tid);
		return;
	}

	if (pstat->apsd_trigger)
		return;
	
	pstat->sleep_time = jiffies;
	
	if ((!(pstat->apsd_bitmap & 0x01) || (tx_servq_len(&pstat->tx_queue[VO_QUEUE]) == 0))
			&& (!(pstat->apsd_bitmap & 0x02) || (tx_servq_len(&pstat->tx_queue[VI_QUEUE]) == 0))
			&& (!(pstat->apsd_bitmap & 0x04) || (tx_servq_len(&pstat->tx_queue[BK_QUEUE]) == 0))
			&& (!(pstat->apsd_bitmap & 0x08) || (tx_servq_len(&pstat->tx_queue[BE_QUEUE]) == 0))) {
		//send QoS Null packet
		SendQosNullData(priv, pstat->cmn_info.mac_addr);
		DEBUG_INFO("sendQosNull  tid=%d\n", tid);
		return;
	}

	pstat->apsd_trigger = 1;

	need_sched = 0;
	for (q_num = VO_QUEUE; q_num >= BK_QUEUE; --q_num) {
		if (pstat->apsd_bitmap & wmm_apsd_bitmask[q_num]) {
			ptxservq = &pstat->tx_queue[q_num];
			xframe_queue = &ptxservq->xframe_queue;
			
			xmit_lock(&xframe_queue->lock, &irqL);
			
			if (_rtw_queue_empty(xframe_queue) == FALSE) {
				sta_queue = &priv->pshare->tx_pending_sta_queue[q_num];
				active = 0;
				
				_rtw_spinlock(&sta_queue->lock);
				
				if (rtw_is_list_empty(&ptxservq->tx_pending) == TRUE) {
					rtw_list_insert_head(&ptxservq->tx_pending, &sta_queue->queue);
					++sta_queue->qlen;
#ifdef CONFIG_SDIO_HCI
					ptxservq->ts_used = 0;
#endif
					active = 1;
					need_sched = 1;
				}
				
				_rtw_spinunlock(&sta_queue->lock);
				
				if (active)
					need_sched_xmit_for_enqueue(priv, q_num);
			}
			
			xmit_unlock(&xframe_queue->lock, &irqL);
		}
	}
	
	if (need_sched) {
		if (pstat->txpause_flag) {
#ifdef CONFIG_WLAN_HAL
			if (IS_HAL_CHIP(priv)) {
				GET_HAL_INTERFACE(priv)->SetMACIDSleepHandler(priv, 0, REMAP_AID(pstat));
			} else
#endif
#ifdef CONFIG_RTL_8812_SUPPORT
			if (GET_CHIP_VER(priv) == VERSION_8812E) {
				RTL8812_MACID_PAUSE(priv, 0, REMAP_AID(pstat));
			} else
#endif
#ifdef CONFIG_RTL_88E_SUPPORT
			if (GET_CHIP_VER(priv) == VERSION_8188E) {
				RTL8188E_MACID_PAUSE(priv, 0, REMAP_AID(pstat));
			} else
#endif
			{
#if defined(CONFIG_RTL_92D_SUPPORT) || defined(CONFIG_RTL_92C_SUPPORT)				
				add_update_ps(priv, pstat);
#endif
			}
			pstat->txpause_flag = 0;
		}
	}
}
#endif // WMM_APSD

