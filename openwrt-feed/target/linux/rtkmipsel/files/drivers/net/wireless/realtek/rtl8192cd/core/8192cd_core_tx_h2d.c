/**
  *  @file 8192cd_h2d_tx.c of H2D Tx process from HostCPU to Data-CPU  (Use 8814A to simulation)
  *  @brief Handle Tx-processes in HostCPU
  *
  *  Packet Offload Engine will help RTK WiFi Chip to decrease host platform CPU utilization.
  *  This functon will handle Tx-processes in Host-CPU
  *
  *  Copyright (c) 2015 Realtek Semiconductor Corp.
  *
  *  @author Peter Yu
  *  @date 2015/10/21
 **/

#define _8192CD_CORE_TX_H2D_C_

#ifdef __KERNEL__
#include <linux/if_arp.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/tcp.h>
#endif

#include "./8192cd_cfg.h"
#include "./8192cd.h"
#include "./8192cd_hw.h"
#include "./8192cd_headers.h"
#include "./8192cd_debug.h"

#if !defined(__KERNEL__) && !defined(__ECOS)
#include "./sys-support.h"
#endif

#ifdef RTL8192CD_VARIABLE_USED_DMEM
#include "./8192cd_dmem.h"
#endif

#if defined(CONFIG_RTL_WAPI_SUPPORT)
#include "wapiCrypto.h"
#endif
#if defined (CONFIG_RTL_VLAN_8021Q) || defined (CONFIG_PUMA_VLAN_8021Q)
#include <linux/if_vlan.h>
#endif

#ifdef CONFIG_RTL867X_VLAN_MAPPING
#include "../../re_vlan.h"
#endif

#ifdef PERF_DUMP
#include "romeperf.h"
#endif

#ifdef TAROKO_0
//no HalComTXDesc.h
#else
#include "HalHeader/HalComTXDesc.h"
#endif
#include "WlanHAL/HalPrecomp.h"


#if defined(__ECOS)
#include "./WlanHAL/RTL88XX/RTL8814B/Hal8814HRxDesc_core.h"
#include "./WlanHAL/RTL88XX/RTL8814B/Hal8814HTxDesc_core.h"
#define EXTERN
#endif

extern void rtl88XX_fill_fwinfo(
    struct rtl8192cd_priv   *priv, 
    struct tx_insn          *txcfg, 
    unsigned int            frag_idx, 
    PTX_DESC_DATA_88XX      pdesc_data
);

/** @brief --get length of the packet for icv, hdr, length
  *
  * @param param_out None
  * @param param_in  priv   : which interface.
  *                  pstat  : which station
  *                  skb    : packets content
  *                  txcfg  : tx config for singin
  * @return 0: handle successfully, others: error
 **/
int get_privacy_frag_thrshld_len(struct rtl8192cd_priv *priv, struct stat_info *pstat, struct sk_buff *pskb, struct tx_insn *txcfg)
{
    unsigned char *da= pskb->data;
    unsigned short protocol = ntohs(*((UINT16 *)((UINT8 *)pskb->data + ETH_ALEN*2)));

    txcfg->privacy = txcfg->iv = txcfg->icv = txcfg->mic = 0;
    txcfg->frag_thrshld = FRAGTHRSLD - _CRCLNG_;

#ifdef A4_STA
    if(priv->pmib->miscEntry.a4_enable && (pstat && (pstat->state & WIFI_A4_STA))) {
        da = pstat->cmn_info.mac_addr;
        txcfg->hdr_len += WLAN_HDR_A4_LEN;
    }
#endif
#ifdef WDS
    if (txcfg->wdsIdx >= 0) {
        txcfg->hdr_len = WLAN_HDR_A4_LEN;
        txcfg->privacy = priv->pmib->dot11WdsInfo.wdsPrivacy;
        switch (txcfg->privacy)
        {
        case _WEP_40_PRIVACY_:
        case _WEP_104_PRIVACY_:
            txcfg->iv = 4;
            txcfg->icv = 4;
            break;
        case _TKIP_PRIVACY_:
            txcfg->iv = 8;
            txcfg->icv = 4;
            txcfg->mic = 0;
            txcfg->fr_len += 8; // for Michael padding
            break;
        case _CCMP_PRIVACY_:
            txcfg->iv = 8;
            txcfg->icv = 0;
            txcfg->mic = 8;
            break;
        }
        return 1;
    }
#endif

    if (!IS_MCAST(da)
#ifdef CLIENT_MODE
      || (OPMODE & WIFI_STATION_STATE)
#endif
    ) {
        if ((((protocol == 0x888E) && ((GET_UNICAST_ENCRYP_KEYLEN == 0)
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
        )) {
            txcfg->privacy = 0;
        } else {
                txcfg->privacy = get_privacy(priv, pstat, &txcfg->iv, &txcfg->icv, &txcfg->mic);
        }
    } else { //IS_MCAST and in ap mode
        txcfg->privacy = get_mcast_privacy(priv, &txcfg->iv, &txcfg->icv, &txcfg->mic);
    }

    txcfg->frag_thrshld -= (txcfg->mic + txcfg->iv + txcfg->icv + txcfg->hdr_len);

    return 1;
}



/** @brief --Check the packet could offload to Data-CPU or not.
  * consider exception case :
  *      SW encryption/decryption
  *      TKIP MIC
  *      Frame for fragmentation/Defragmentation        : Fragment could do in data cpu
  *      Station Sleep: Enqueue frame per sleeping STA  : dequeue should call rtl8192cd_xmit_offload_check
  *      AMSDU is enable or not
  *      MAC-layer NAT translation (STA/Ad-hoc mode)
  * @param param_out None
  * @param param_in  skb : packets content
  *                  priv: to which interface.
  * @return 1: offload, 0: not offload, others: error
 **/
__IRAM_IN_865X
int rtl8192cd_xmit_offload_check(struct rtl8192cd_priv *priv, struct stat_info *pstat, struct sk_buff *pskb, struct tx_insn *txcfg, unsigned int flags)
{
    int ret = 1;

    if ((pstat) && (!IS_MCAST(pskb->data))) {
		
	    if (flags == 1)
	        if (get_privacy_frag_thrshld_len(priv, pstat, pskb, txcfg) == 0)
	            return 0;

        //if ((ntohs(*((UINT16 *)((UINT8 *)pskb->data + ETH_ALEN*2))) + WLAN_ETHHDR_LEN) > WLAN_MAX_ETHFRM_LEN)
        //    iLen += sizeof(struct llc_snap);
        /* check station is exist in Data-CPU?
         * check station is sleep mode or not
         * check AMSDU for station or not?  // this maybe offload in the future
         * check encryption: SW or TKIP
         * check fragment or not
         */
         //MAC_PRINT(pstat->cmn_info.mac_addr);
        /*printk("peteryu2: aid:%d sta_in_dc:%d, 0x%x, len:%d,%d, aggr:%d, %d, %d, %d\n", 
            pstat->cmn_info.aid, pstat->sta_in_dc, pstat->state, 
            pskb->len, txcfg->frag_thrshld, pstat->aggre_mthd,
            get_sta_encrypt_algthm(priv, pstat),txcfg->privacy, (UseSwCrypto(priv, pstat, IS_MCAST(pskb->data))));*/
        if ((!pstat->sta_in_dc)
          /*|| (pstat->state & WIFI_SLEEP_STATE)*/
          /*|| (pstat->aggre_mthd & AGGRE_MTHD_MSDU)*/
          /*|| (!(pstat->aggre_mthd & AGGRE_MTHD_MPDU))*/
          || (get_sta_encrypt_algthm(priv, pstat) == _TKIP_PRIVACY_)
          || (pskb && pskb->len > (txcfg->frag_thrshld))                //keep this at first, maybe could remove
#ifdef A4_STA
          || ((priv->pmib->miscEntry.a4_enable) && (pstat && (pstat->state & WIFI_A4_STA)))
#endif
		  || (txcfg->privacy 
#ifdef CONFIG_IEEE80211W
          && (UseSwCrypto(priv, pstat, pskb? IS_MCAST(pskb->data):0, 0))
#else
          && (UseSwCrypto(priv, pstat, pskb? IS_MCAST(pskb->data):0))
#endif
        )) {
            #ifdef A4_STA
            if ((priv->pmib->miscEntry.a4_enable) && (pstat && (pstat->state & WIFI_A4_STA))) {
                if (txcfg->aggre_en >= 5)
                    txcfg->aggre_en = 1;
            }
            #endif
            ret = 0;
        }
    } else {
        ret = 0;
    }
    
	return ret;
}


#ifdef TX_SHORTCUT
EXTERN int get_tx_sc_index(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *hdr, int aggre_en);
EXTERN int get_tx_sc_free_entry(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *hdr, int aggre_en);

inline BOOLEAN check_txsc_condition(struct rtl8192cd_priv *priv, struct tx_insn *txcfg, struct sk_buff *skb, unsigned long *txsc_entry_addr, unsigned int pri) 
{

	struct stat_info *pstat = txcfg->pstat;
	struct tx_insn		*txsc_txcfg;
	struct tx_sc_entry *ptxsc_entry;
	
	int idx=0, shortcut = 0;
	int k;

	if (!priv->pmib->dot11OperationEntry.disable_txsc && pstat && !IS_MCAST(((struct sk_buff *)txcfg->pframe)->data)
			&& (((k = get_tx_sc_index(priv, pstat, ((struct sk_buff *)txcfg->pframe)->data, txcfg->aggre_en)) >= 0)))
	{

		ptxsc_entry = &pstat->tx_sc_ent[k];
		txsc_txcfg = &(ptxsc_entry->txcfg);
		if(
#if defined(NOT_RTK_BSP) && !defined(__ECOS)
			// go slow path to avoid TX drop when no enough tail room to put TKIP MIC.
			((_TKIP_PRIVACY_ != txsc_txcfg->privacy) || ((skb->tail + 8) <= skb->end)) &&
#endif
			((txsc_txcfg->privacy == 0) ||
#ifdef CONFIG_RTL_WAPI_SUPPORT
			  (txsc_txcfg->privacy == _WAPI_SMS4_) ||
#endif
			  (!txcfg->use_sw_enc)
			) &&
			(txsc_txcfg->fr_len > 0) &&
			(pri/*get_skb_priority(priv, skb, pstat, txcfg->is_11s)*/ == ptxsc_entry->pktpri) &&
			(FRAGTHRSLD > 1500) &&
#if 0//def SUPPORT_TX_AMSDU_SHORTCUT
			 ((txsc_txcfg->aggre_en == FG_AGGRE_MSDU_FIRST && txcfg->aggre_en >=FG_AGGRE_MSDU_FIRST) || (txsc_txcfg->aggre_en != FG_AGGRE_MSDU_FIRST && txcfg->aggre_en < FG_AGGRE_MSDU_FIRST))
#else
			1
#endif
			)
			{

#if 1
				PH2D_TXDMA_INFO_DATA dma = (PH2D_TXDMA_INFO_DATA)ptxsc_entry->hal_hw_desc;

				if (
						/*((GET_HAL_INTERFACE(priv)->GetShortCutTxBuffSizeHandler(priv, ptxsc_entry->hal_hw_desc)) > 0) &&*/
				/*dma->dw2_txdesc_checksum &&*/
						(
			            #ifdef CONFIG_RTL_WAPI_SUPPORT
							// Note: for sw wapi, txcfg->mic=16; for hw wapi, txcfg->mic=0.
							(txcfg->privacy==_WAPI_SMS4_) ? ((skb->len+txcfg->mic)==(get_desc(ptxsc_entry->hwdesc2.Dword7)&TX_TxBufSizeMask)) :
			            #endif
							1/*( skb->len == (GET_HAL_INTERFACE(priv)->GetShortCutTxBuffSizeHandler(priv, ptxsc_entry->hal_hw_desc)))*/
				/*(skb->len == dma->dw2_txdesc_checksum)*/
						) &&
						/*(txcfg->tx_rate == ptxsc_entry->txcfg.tx_rate) && */
						(txcfg->q_num == ptxsc_entry->txcfg.q_num)&&			
						(pstat->protection == priv->pmib->dot11ErpInfo.protection) &&
						(pstat->ht_protection == priv->ht_protection) &&
	                #if defined(WIFI_WMM) && defined(WMM_APSD)
						(!(
	                #ifdef CLIENT_MODE
									(OPMODE & WIFI_AP_STATE) &&
	                #endif
									(APSD_ENABLE) && (pstat->state & WIFI_SLEEP_STATE)
						   )
						)
					#else
							1
	                #endif
					)
#endif
					{
						*txsc_entry_addr = (unsigned long) ptxsc_entry;
						return 1;								
					}
			}
	}
	return 0;
	


}
#endif


/** @brief --transmit the packet to Data-CPU
  *
  * @param param_out None
  * @param param_in  priv   : which interface.
  *                  pstat  : which station
  *                  skb    : packets content
  *                  txcfg  : tx config for singin
  * @return 0: handle successfully, others: error
 **/
__IRAM_IN_865X
int rtl8192cd_xmit_offload(struct stat_info *pstat, struct sk_buff *pskb, struct tx_insn* txcfg)
{
    int ret = SUCCESS;
    unsigned int q_num, pri;
    unsigned char *da = NULL;
    unsigned short protocol;
    unsigned int is_dhcp=0;
    struct net_device *dev = pskb->dev;
    struct rtl8192cd_priv *priv = GET_DEV_PRIV(dev);
#ifdef TX_SHORTCUT
    unsigned long	ptr;
    unsigned int fit_sc = 0;
    unsigned char *bak_hdr, bak_aggre_en, bak_h2d_type;

#endif

	if (rtl8192cd_xmit_offload_check(priv, pstat, pskb, txcfg, 1) != 1) {
		ret = FAIL;
		goto stop_proc;
	}
	
	/* 802.3 packet process */
	pri = pskb->cb[_SKB_CB_PRIORITY];
	q_num = pskb->cb[_SKB_CB_QNUM];

#ifdef MCAST2UI_REFINE
    if (txcfg->isMC2UC)
        memcpy(pskb->data, &pskb->cb[10], WLAN_ADDR_LEN);
#endif

  if (getAvailableTXBD(priv, q_num) < NUM_TX_DESC/4) {
		rtl8192cd_tx_queueDsr(priv, q_num);
		if(getAvailableTXBD(priv, q_num) < 4) {
			priv->ext_stats.tx_drops++;
			DEBUG_ERR("%s: no available TXBD\n", __FUNCTION__);
       goto free_and_stop;
		}
  }

    txcfg->phdr = (UINT8 *)get_wlanllchdr_from_poll(priv);
    if (txcfg->phdr == NULL) {
		priv->ext_stats.tx_drops++;
        DEBUG_WARN("Can't alloc wlan header!\n");
        goto free_and_stop;
    }

#ifdef TX_SHORTCUT	
		txcfg->pstat = pstat;
		txcfg->pframe = pskb;
		txcfg->q_num = q_num;
		if(check_txsc_condition(priv, txcfg, pskb, &ptr, pri)
			&& ((pskb->cb[_SKB_CB_FLAGS] & _SKB_CB_FLAGS_DHCP) == 0)) {
			struct tx_sc_entry *sc = (struct tx_sc_entry *)ptr;

			bak_hdr = txcfg->phdr;
			bak_aggre_en = txcfg->aggre_en;
			bak_h2d_type = txcfg->h2d_type;

			
			memcpy(txcfg, &sc->txcfg, sizeof(struct tx_insn));
			txcfg->pframe = pskb;
			txcfg->phdr = bak_hdr;
			txcfg->hdr_len = 0;
			txcfg->fr_len = pskb->len;
			txcfg->aggre_en = bak_aggre_en;
			txcfg->h2d_type = bak_h2d_type;

			
			if (txcfg->h2d_type != TXPKTINFO_TYPE_AMSDU)
				txcfg->h2d_type = TXPKTINFO_TYPE_8023;

            txcfg->tx_rate = get_tx_rate(priv, pstat);
#ifndef TX_LOWESTRATE				
            txcfg->lowest_tx_rate = get_lowest_tx_rate(priv, pstat, txcfg->tx_rate);
#endif

					if (txcfg->aggre_en == 0
#ifdef SUPPORT_TX_MCAST2UNI
							&& (priv->pshare->rf_ft_var.mc2u_disable || (pskb->cb[2] != (char)0xff))
#endif
					) {
						if (pstat->aggre_mthd & AGGRE_MTHD_MPDU)
							txcfg->aggre_en = FG_AGGRE_MPDU;
					}

	        //send add BA
	        if (!pstat->ADDBA_ready[pri]) {
	            if ((pstat->ADDBA_req_num[pri] < 5) && !pstat->ADDBA_sent[pri]) {
	                pstat->ADDBA_req_num[pri]++;
	                SMP_UNLOCK_XMIT(flags);
	                issue_ADDBAreq(priv, pstat, pri);
	                SMP_LOCK_XMIT(flags);
	                pstat->ADDBA_sent[pri]++;
	            }
	        }
			
			fit_sc = 1;
			goto enter_txsc;
		}
#endif

    //set txcfg.  On current time, do not need support AMSDU, so just do this
    txcfg->pstat = pstat;
    txcfg->fr_type = _SKB_FRAME_TYPE_;
    txcfg->pframe = pskb;
    txcfg->hdr_len = 0;
    txcfg->q_num = q_num;
    if (txcfg->h2d_type != TXPKTINFO_TYPE_AMSDU)
        txcfg->h2d_type = TXPKTINFO_TYPE_8023;

    da = pskb->data;

    // simple content of check_desc
    //txcfg->privacy = txcfg->iv = txcfg->icv = txcfg->mic = 0;
    txcfg->frg_num = 1;

#ifdef TX_SCATTER
    if (pskb->list_num > 0)
        txcfg->fr_len = pskb->total_len;
    else
#endif
        txcfg->fr_len = pskb->len;

    if (!IS_MCAST(da)
#ifdef CLIENT_MODE
      || (OPMODE & WIFI_STATION_STATE)
#endif
    ) {
        txcfg->need_ack = 1;
        txcfg->rts_thrshld = RTSTHRSLD;

        protocol = ntohs(*((UINT16 *)((UINT8 *)pskb->data + ETH_ALEN*2)));
        if ((OPMODE & WIFI_AP_STATE) && (pskb->cb[_SKB_CB_FLAGS] & _SKB_CB_FLAGS_DHCP))
            is_dhcp++;

        if (txcfg->aggre_en == 0
#ifdef SUPPORT_TX_MCAST2UNI
                && (priv->pshare->rf_ft_var.mc2u_disable || (pskb->cb[2] != (char)0xff))
#endif
        ) {
            if ((pstat->aggre_mthd & AGGRE_MTHD_MPDU) && 
                    (protocol != 0x888E) &&
                    !is_dhcp && pstat->ADDBA_ready[pri])
                txcfg->aggre_en = FG_AGGRE_MPDU;
        }

        if ((protocol == 0x888E)
#ifdef CONFIG_RTL_WAPI_SUPPORT
            ||(protocol == ETH_P_WAPI)
#endif
            || is_dhcp) {
        // use basic rate to send EAP packet for sure
            txcfg->tx_rate = find_rate(priv, NULL, 0, 1);
#ifndef TX_LOWESTRATE
            txcfg->lowest_tx_rate = txcfg->tx_rate;
#endif				
            txcfg->fixed_rate = 1;
            txcfg->retry = 0x20;

#ifdef TX_SHORTCUT
            txcfg->skip_txsc = 1;
#endif
        }
        else {
            txcfg->tx_rate = get_tx_rate(priv, pstat);
#ifndef TX_LOWESTRATE				
            txcfg->lowest_tx_rate = get_lowest_tx_rate(priv, pstat, txcfg->tx_rate);
#endif				
            if (!is_auto_rate(priv, pstat) &&
                !(should_restrict_Nrate(priv, pstat) && is_fixedMCSTxRate(priv, pstat)))
            {            
#ifdef RTK_AC_SUPPORT
                if (!is_fixedVHTTxRate(priv, pstat) || (pstat->vht_cap_len))
#endif
                {
                    txcfg->fixed_rate = 1;
                }
            }

            if (pskb->len > priv->pmib->dot11OperationEntry.dot11RTSThreshold)
                txcfg->retry = priv->pmib->dot11OperationEntry.dot11LongRetryLimit;
            else
                txcfg->retry = priv->pmib->dot11OperationEntry.dot11ShortRetryLimit;
        }

        //send add BA
        if (!pstat->ADDBA_ready[pri]) {
            if ((pstat->ADDBA_req_num[pri] < 5) && !pstat->ADDBA_sent[pri]) {
                pstat->ADDBA_req_num[pri]++;
                SMP_UNLOCK_XMIT(flags);
                issue_ADDBAreq(priv, pstat, pri);
                SMP_LOCK_XMIT(flags);
                pstat->ADDBA_sent[pri]++;
            }
        }
    } else {
        txcfg->need_ack = 0;
        txcfg->rts_thrshld = 10000;
        txcfg->aggre_en = 0;

        if ((*da) == 0xff)  // broadcast
            txcfg->tx_rate = find_rate(priv, NULL, 0, 1);
        else {              // multicast
            if (priv->pmib->dot11StationConfigEntry.lowestMlcstRate)
                txcfg->tx_rate = get_rate_from_bit_value(priv->pmib->dot11StationConfigEntry.lowestMlcstRate);
            else
                txcfg->tx_rate = find_rate(priv, NULL, 1, 1);
        }
#ifndef TX_LOWESTRATE
        txcfg->lowest_tx_rate = txcfg->tx_rate;
#endif			
        txcfg->fixed_rate = 1;

        // if group key not set yet, don't let unencrypted multicast go to air
        if (priv->pmib->dot11GroupKeysTable.dot11Privacy) {
            if (GET_GROUP_ENCRYP_KEYLEN == 0) {
				priv->ext_stats.tx_drops++;
                DEBUG_ERR("TX DROP: group key not set yet!\n");
                ret = SUCCESS;
                goto free_and_stop;
            }

            txcfg->privacy = get_mcast_privacy(priv, &txcfg->iv, &txcfg->icv, &txcfg->mic);
            if (OPMODE & WIFI_AP_STATE) {
#ifdef MBSSID
                if (IS_ROOT_INTERFACE(priv))
					txcfg->q_num = HIGH_QUEUE_V2;
				else
					txcfg->q_num = HIGH_QUEUE_V2 + priv->vap_init_seq;
#else
				txcfg->q_num = HIGH_QUEUE_V2;
#endif

            } else {
                // to do: sta mode
                txcfg->q_num = BE_QUEUE;
                pskb->cb[1] = 0;
            }

        }
    }

#if (BEAMFORMING_SUPPORT == 1)
    if((priv->pmib->dot11RFEntry.txbf == 1) && (pstat) &&
        ((pstat->ht_cap_len && (pstat->ht_cap_buf.txbf_cap))
#ifdef RTK_AC_SUPPORT
        ||(pstat->vht_cap_len && (cpu_to_le32(pstat->vht_cap_buf.vht_cap_info) & BIT(SU_BFEE_S)))
#endif
        ))
        Beamforming_GidPAid(priv, pstat);
#endif
enter_txsc:
    // for SW LED
    priv->pshare->LED_tx_cnt++;

    tx_sum_up(priv, txcfg->pstat, txcfg);

#ifdef TX_SHORTCUT
    if (fit_sc)
        rtl88XX_signin_offload_htxdesc_shortcut(priv, txcfg, ptr);
    else
#endif
    rtl88XX_signin_offload_htxdesc(priv, txcfg);

    goto stop_proc;

free_and_stop:        /* Free current packet and stop TX process */
    if (txcfg->phdr) {
        release_wlanllchdr_to_poll(priv, txcfg->phdr);
    }
    
    if (pskb)
        rtl_kfree_skb(priv, pskb, _SKB_TX_);

stop_proc:
    return ret;
}

/** @brief -- MappingChannelDMA
  * 
 **/
static int MappingChannelDMA(int TxQNum)
{
	switch (TxQNum) {
	case BK_QUEUE:
		return HCI_TXDMA_CHAN_03;
	case BE_QUEUE:
		return HCI_TXDMA_CHAN_02;
	case VI_QUEUE:
		return HCI_TXDMA_CHAN_01;
	case VO_QUEUE:
		return HCI_TXDMA_CHAN_00;
	case MGNT_QUEUE:
		return 14;
	case HIGH_QUEUE:
	case HIGH_QUEUE1:
	case HIGH_QUEUE2:
	case HIGH_QUEUE3:
	case HIGH_QUEUE4:
	case HIGH_QUEUE5:
	case HIGH_QUEUE6:
	case HIGH_QUEUE7:
		return 15;
	case CMD_QUEUE_V2:
		return 16;
	case BEACON_QUEUE:
		return 19;
	default: 
        printk("%s: Unknown ChannelDMA Mapping (%d)\n", __func__, TxQNum);
		return HCI_TXDMA_CHAN_00;
	}
}


/** @brief -- MappingQSel
  * 
 **/
static int MappingQSel(HAL_PADAPTER Adapter, int TxQNum, int Tid)
{
	switch (TxQNum) {
	case HIGH_QUEUE:
	case HIGH_QUEUE1:
	case HIGH_QUEUE2:
	case HIGH_QUEUE3:
	case HIGH_QUEUE4:
	case HIGH_QUEUE5:
	case HIGH_QUEUE6:
	case HIGH_QUEUE7:
		return TXDESC_QSEL_HIGH;
	case MGNT_QUEUE: 
		return TXDESC_QSEL_MGT;
	case CMD_QUEUE_V2: 
		return 20;
	case BEACON_QUEUE: 
		return TXDESC_QSEL_BCN;
	default: 
#if CFG_HAL_RTL_MANUAL_EDCA
    	if (HAL_VAR_MANUAL_EDCA) {
	    	switch (TxQNum) {
	    	case VO_QUEUE:
	    		return TXDESC_QSEL_TID6;
	    	case VI_QUEUE:
	    		return TXDESC_QSEL_TID4;
	    	case BE_QUEUE:
	    		return TXDESC_QSEL_TID0;
	    	default:
	    		return TXDESC_QSEL_TID1;
	    	}
    	} else
    		return Tid;
#else
        return Tid;
#endif
	}
}




static __IRAM_IN_865X void update_txie(struct rtl8192cd_priv *priv, struct tx_insn *txcfg)
{
    struct h2d_txie_t *txie[MAX_TXIE_NUMBER];
    TX_DESC_DATA_88XX desc_data;
    int i;
    unsigned char *da;

    if (txcfg->pstat) {
        for(i=0; i<MAX_TXIE_NUMBER; i++)
        txie[i] = (struct h2d_txie_t *)(&txcfg->pstat->txie[i][0]);
    }
    else {
        for(i=0; i<MAX_TXIE_NUMBER; i++)
        txie[i] = (struct h2d_txie_t *)(&priv->txie[i][0]);
    }

    memset(&desc_data, 0, sizeof(TX_DESC_DATA_88XX));

#ifdef CLIENT_MODE
    if (OPMODE & WIFI_STATION_STATE) {
        if (GetFrameSubType(txcfg->phdr) == WIFI_PSPOLL)
            desc_data.navUseHdr = _TRUE;
    }
#endif

#if (BEAMFORMING_SUPPORT == 1)
    if (txcfg->ndpa)
        desc_data.navUseHdr = _TRUE;
#endif

    desc_data.frLen  = txcfg->fr_len;

#if 0
//#ifdef CONFIG_PHYDM_ANTENNA_DIVERSITY
    if (txcfg->pstat)
    {
        ODM_SetTxAntByTxInfo_HAL(priv, (PVOID)&desc_data, txcfg->pstat->cmn_info.aid);   //HAL Set TX antenna
    }
#endif

    rtl88XX_fill_fwinfo(priv, txcfg, 0, &desc_data);    // FIXME: frag_idx?

#if (BEAMFORMING_SUPPORT == 1)
    if (txcfg->pstat) {
        if ((priv->pmib->dot11RFEntry.txbf == 1) &&
            ((txcfg->pstat->ht_cap_len && (txcfg->pstat->ht_cap_buf.txbf_cap)) 
#ifdef RTK_AC_SUPPORT
             ||(txcfg->pstat->vht_cap_len && (cpu_to_le32(txcfg->pstat->vht_cap_buf.vht_cap_info) & BIT(SU_BFEE_S)))
#endif
        )) {
            desc_data.p_aid = txcfg->pstat->cmn_info.bf_info.p_aid;
            desc_data.g_id  = txcfg->pstat->cmn_info.bf_info.g_id;
        }
    }
#endif

    if (txcfg->hdr_len)
        da = txcfg->phdr;
    else
        da = (unsigned char *)txcfg->pframe;

    if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G) {
        desc_data.dataRateFBLmt = 4;
    } else {
        desc_data.dataRateFBLmt = 0;
    }

    if (txcfg->fixed_rate || (priv->pshare->rf_ft_var.txforce != 0xff)) {
        desc_data.disDataFB = _TRUE;
        desc_data.disRTSFB  = _TRUE;
        desc_data.useRate   = _TRUE;
    }

    if(txcfg->pstat && txcfg->pstat->cmn_info.ra_info.disable_ra)
        desc_data.useRate = _TRUE;

    if(txcfg->privacy )  {              
#ifdef CONFIG_IEEE80211W			
        txcfg->use_sw_enc = UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE), txcfg->isPMF);
#else		
        txcfg->use_sw_enc = UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE));
#endif        
    }

    if (!txcfg->use_sw_enc) {
    if (txcfg->privacy == _WEP_40_PRIVACY_ ||
		txcfg->privacy == _WEP_104_PRIVACY_ ||
		txcfg->privacy == _TKIP_PRIVACY_)
        desc_data.secType = 1;
	else if (txcfg->privacy == _WAPI_SMS4_)
		desc_data.secType = 2;
	else if (txcfg->privacy == _CCMP_PRIVACY_)
		desc_data.secType = 3;
    }

    if (txcfg->q_num >= HCI_TX_DMA_QUEUE_HI0)
        desc_data.moreData = 1;

    /******************** Fill TXIEs ********************/
    /* IE 0 word 0 */
    IE0_SET_TX_DESC_HW_RTS_EN(txie[0], desc_data.HWRTSEn);
    IE0_SET_TX_DESC_RTS_EN(txie[0], desc_data.RTSEn);
    IE0_SET_TX_DESC_CTS2SELF(txie[0], desc_data.CTS2Self);
    IE0_SET_TX_DESC_RTY_LMT_EN(txie[0], desc_data.rtyLmtEn);
    IE0_SET_TX_DESC_RTS_SHORT(txie[0], desc_data.RTSShort);
    IE0_SET_TX_DESC_DISDATAFB(txie[0], desc_data.disDataFB);
    IE0_SET_TX_DESC_DISRTSFB(txie[0], desc_data.disRTSFB);
    IE0_SET_TX_DESC_DATA_SHORT(txie[0], desc_data.dataShort);
    IE0_SET_TX_DESC_USERATE(txie[0], desc_data.useRate);

    /* IE 0 word 1 */
    IE0_SET_TX_DESC_RTS_RTY_LOWEST_RATE(txie[0], desc_data.RTSRateFBLmt);
    IE0_SET_TX_DESC_DATA_RTY_LOWEST_RATE(txie[0], desc_data.dataRateFBLmt);
    IE0_SET_TX_DESC_RTS_DATA_RTY_LMT(txie[0], desc_data.dataRtyLmt);
    IE0_SET_TX_DESC_DATA_BW(txie[0], desc_data.dataBW);
    IE0_SET_TX_DESC_RTSRATE(txie[0], desc_data.RTSRate);
    IE0_SET_TX_DESC_DATARATE(txie[0], desc_data.dataRate);

    /* IE 1 word 0 */
    IE1_SET_TX_DESC_AMPDU_DENSITY(txie[1], desc_data.ampduDensity);

	if(priv->pshare->rf_ft_var.manual_ampdu)
   	 	IE1_SET_TX_DESC_MAX_AGG_NUM(txie[1], 0);
	else{
		IE1_SET_TX_DESC_MAX_AGG_NUM(txie[1], desc_data.maxAggNum);
	}
	
    IE1_SET_TX_DESC_SECTYPE(txie[1], desc_data.secType);
    IE1_SET_TX_DESC_MOREFRAG(txie[1], desc_data.frag);
    IE1_SET_TX_DESC_NAVUSEHDR(txie[1], desc_data.navUseHdr);
    IE1_SET_TX_DESC_BMC(txie[1], HAL_IS_MCAST(da)?1:0);
	if (txcfg->h2d_type == TXPKTINFO_TYPE_80211_PROTO) {
		//IE1_SET_TX_DESC_EN_HWSEQ(txie[1], 1);
	}

    /* IE 1 word 1 */
    IE1_SET_TX_DESC_P_AID(txie[1], desc_data.p_aid);
    IE1_SET_TX_DESC_MOREDATA(txie[1], desc_data.moreData);

    /* IE 2 word 0 */

    /* IE 2 word 1 */
#ifdef MBSSID
    IE2_SET_TX_DESC_MBSSID(txie[2], priv->vap_init_seq);
#endif

    /* IE 3 word 0 */
    IE3_SET_TX_DESC_DATA_SC(txie[3], desc_data.dataSC);

    /* IE 3 word 1 */
    IE3_SET_TX_DESC_G_ID(txie[3], desc_data.g_id);
    IE3_SET_TX_DESC_CCA_RTS(txie[3], desc_data.cca_rts);

    /* IE 4 word 0 */
    IE4_SET_TX_DESC_DATA_STBC(txie[4], desc_data.dataStbc);
    IE4_SET_TX_DESC_DATA_LDPC(txie[4], desc_data.dataLdpc);

    /* IE 4 word 1 */

    /* IE 5 word 0 */

    /* IE 5 word 1 */

	IE0_SET_TX_DESC_IE_UP(txie[0],1);
	IE0_SET_TX_DESC_IE_NUM(txie[0],0);
	IE1_SET_TX_DESC_IE_UP(txie[1],1);
	IE1_SET_TX_DESC_IE_NUM(txie[1],1);
	IE2_SET_TX_DESC_IE_UP(txie[2],1);
	IE2_SET_TX_DESC_IE_NUM(txie[2],2);
	IE3_SET_TX_DESC_IE_UP(txie[3],1);
	IE3_SET_TX_DESC_IE_NUM(txie[3],3);
	IE4_SET_TX_DESC_IE_UP(txie[4],1);
	IE4_SET_TX_DESC_IE_NUM(txie[4],4); 
	IE4_SET_TX_DESC_IE_END(txie[4],1); 


}


/** @brief -- fill pktinfo.
  *
  * @param param_out None
  * @param param_in  priv:  which Wi-Fi interface.
  *                  pstat: which station.
  * @return 0: OK, 1: others
 **/
__IRAM_IN_865X int rtl88XX_fill_txPktInfo(struct rtl8192cd_priv *priv, struct tx_insn *txcfg, unsigned char pktType)
{
    struct h2d_txpkt_info *pTxPktInfo = (struct h2d_txpkt_info *)(txcfg->phdr - sizeof(struct h2d_txpkt_info));
    
    memset((char *)pTxPktInfo, 0, sizeof(struct h2d_txpkt_info));

	pTxPktInfo->ver = 0;
	pTxPktInfo->dummy_len = 0;

    pTxPktInfo->pkt_type = pktType;
    pTxPktInfo->ifid = priv->if_id;
    /* use macid0 if h2d */
    if (txcfg->h2d_type == TXPKTINFO_TYPE_H2D)
        pTxPktInfo->macid = 0;
    else
        pTxPktInfo->macid = (txcfg->pstat ? REMAP_AID(txcfg->pstat) : 0);
    if (txcfg->aggre_en == FG_AGGRE_MSDU_LAST)
        pTxPktInfo->last_msdu = 1;
    /* FIXME: fix correct macid for each interface */
    pTxPktInfo->tid = txcfg->h2d_tid;
#ifdef _BIG_ENDIAN_
{
    int *pswap = (int *)pTxPktInfo;
    pswap[0] = cpu_to_le32(pswap[0]);
    pswap[1] = cpu_to_le32(pswap[1]);
    pswap[2] = cpu_to_le32(pswap[2]);
    pswap[3] = cpu_to_le32(pswap[3]);
}	
#endif	

    return 0;
}

#ifdef TX_SHORTCUT
static inline __IRAM_IN_865X int rtl88XX_fill_txDMAInfo_shortcut(struct rtl8192cd_priv *priv, struct tx_insn *txcfg, struct tx_sc_entry *ptxsc_entry, PH2D_TXDMA_INFO_DATA pTxdma_Info)
{
	int i;
	struct h2d_txie_t *txie[6];
	TX_DESC_DATA_88XX desc_data;
	unsigned char txRate;

	//memcpy(pTxdma_Info, ptxsc_entry->hal_hw_desc, 32);
   	//memcpy(pTxdma_Info, ptxsc_entry->hal_hw_desc, sizeof(H2D_TXDMA_INFO_DATA) - 4 * sizeof(struct h2d_txie_t));
	memcpy(pTxdma_Info, ptxsc_entry->hal_hw_desc, sizeof(H2D_TXDMA_INFO_DATA));
	
	txie[0] = &(pTxdma_Info->txie[0]);
	txie[1] = &(pTxdma_Info->txie[1]);
	
	if (txcfg->fixed_rate || (priv->pshare->rf_ft_var.txforce != 0xff)) {
		desc_data.disDataFB = _TRUE;
		desc_data.disRTSFB	= _TRUE;
		desc_data.useRate	= _TRUE;
		desc_data.dataRate = priv->pshare->rf_ft_var.txforce; 
		/* IE 0 word 0 */
		IE0_SET_TX_DESC_DISDATAFB(txie[0], desc_data.disDataFB);
		IE0_SET_TX_DESC_DISRTSFB(txie[0], desc_data.disRTSFB);
		IE0_SET_TX_DESC_USERATE(txie[0], desc_data.useRate);

		/* IE 0 word 1 */
		IE0_SET_TX_DESC_DATARATE(txie[0], desc_data.dataRate);
	}
	else {
		if (is_MCS_rate(txcfg->tx_rate))	// HT rates
		{
#ifdef RTK_AC_SUPPORT
			if(is_VHT_rate(txcfg->tx_rate))
			{
				txRate = (txcfg->tx_rate - VHT_RATE_ID) + 44;
			}
			else
#endif
			{
				txRate = (txcfg->tx_rate - HT_RATE_ID) + 12;
			}
		}
		else
			txRate = get_rate_index_from_ieee_value((UINT8)txcfg->tx_rate);

		desc_data.dataRate = txRate;

		/* IE 0 word 1 */
		IE0_SET_TX_DESC_DATARATE(txie[0], desc_data.dataRate);
	}


	if (txcfg->privacy)
		if (txcfg->pstat->IOTPeer == HT_IOT_PEER_NETGEAR
			&& txcfg->pstat->ht_cap_buf.support_mcs[3])
			desc_data.ampduDensity = _HTCAP_AMPDU_SPC_2_US_;
		else
			desc_data.ampduDensity = ((txcfg->pstat->ht_cap_buf.ampdu_para & _HTCAP_AMPDU_SPC_MASK_) >> _HTCAP_AMPDU_SPC_SHIFT_);
	else {
		if (txcfg->pstat->IOTPeer == HT_IOT_PEER_NETGEAR
			&& txcfg->pstat->ht_cap_buf.support_mcs[3])
			desc_data.ampduDensity = _HTCAP_AMPDU_SPC_QUAR_US_;
		else
			desc_data.ampduDensity = ((txcfg->pstat->ht_cap_buf.ampdu_para & _HTCAP_AMPDU_SPC_MASK_) >> _HTCAP_AMPDU_SPC_SHIFT_);
	}


	if(priv->pshare->rf_ft_var.manual_ampdu)
   	 	IE1_SET_TX_DESC_MAX_AGG_NUM(txie[1], 0);
	else{
		desc_data.maxAggNum = txcfg->pstat->maxAggNum >> 1;	// half of the max agg num	
		IE1_SET_TX_DESC_MAX_AGG_NUM(txie[1], desc_data.maxAggNum);
	}
	
	IE1_SET_TX_DESC_AMPDU_DENSITY(txie[1], desc_data.ampduDensity);
	//IE0_SET_TX_DESC_IE_END(txie[0], 1);
	//IE0_SET_TX_DESC_IE_UP(txie[0], 1);
	//IE1_SET_TX_DESC_IE_END(txie[1], 1);
	//IE1_SET_TX_DESC_IE_UP(txie[1], 1);
	//pTxdma_Info->dw0_pkt_offset = 2; 
	//pTxdma_Info->dw0_offset = 32;

	pTxdma_Info->dw0_agg_en = !!txcfg->aggre_en;
	
	if (txcfg->h2d_type == TXPKTINFO_TYPE_8023 || txcfg->h2d_type == TXPKTINFO_TYPE_AMSDU) {
       pTxdma_Info->dw0_txpktsize = txcfg->fr_len + sizeof(struct h2d_txpkt_info)
                                   + txcfg->hdr_len;
    } else {
        pTxdma_Info->dw0_txpktsize = txcfg->fr_len + sizeof(struct h2d_txpkt_info)
                                   + txcfg->hdr_len + txcfg->llc;
        if (txcfg->privacy) {
            if (txcfg->use_sw_enc)
                pTxdma_Info->dw0_txpktsize += txcfg->iv + txcfg->icv + txcfg->mic;
            else
                pTxdma_Info->dw0_txpktsize += txcfg->iv;
        }
    }

    if (txcfg->h2d_type == TXPKTINFO_TYPE_AMSDU)
        pTxdma_Info->dw2_dma_pri = 1;
	else
		pTxdma_Info->dw2_dma_pri = 0;

	pTxdma_Info->dw3_offload_size = pTxdma_Info->dw0_offset + pTxdma_Info->dw0_txpktsize; //full load //txdesc+txpktinfo+payload

	if((priv->pmib->miscEntry.auto_rts_mode == 1)&&(txcfg->rts_thrshld <= get_mpdu_len(txcfg, txcfg->fr_len))){
	    /* IE 0 word 0 */
		IE0_SET_TX_DESC_HW_RTS_EN(txie[0], FALSE);
		IE0_SET_TX_DESC_RTS_EN(txie[0], TRUE);
		IE0_SET_TX_DESC_CTS2SELF(txie[0], FALSE);
	}else if (priv->pmib->miscEntry.auto_rts_mode == 2){
	
		IE0_SET_TX_DESC_HW_RTS_EN(txie[0], FALSE);
		IE0_SET_TX_DESC_RTS_EN(txie[0], FALSE);
		IE0_SET_TX_DESC_CTS2SELF(txie[0], FALSE);
	}
	else if (priv->pmib->miscEntry.auto_rts_mode == 3){
	
		IE0_SET_TX_DESC_HW_RTS_EN(txie[0], FALSE);
		IE0_SET_TX_DESC_RTS_EN(txie[0], TRUE);
		IE0_SET_TX_DESC_CTS2SELF(txie[0], FALSE);
	}

	if (txcfg->pstat){
		if(txcfg->pstat->cmn_info.aid == priv->pshare->rf_ft_var.print_sta_txdesc){

			panic_printk("%s %d IE body:\n", __FUNCTION__, __LINE__);
			panic_printk("0x");	
			for (i=15;i>=0;i--){
				panic_printk("%02x",*((unsigned char *)pTxdma_Info+i));
			}
			panic_printk("\n");
			for (i=0;i<=5;i++){
				panic_printk("%s %d txie[%d]->dw0 0x%x\n", __FUNCTION__, __LINE__,i,pTxdma_Info->txie[i].dw0);
				panic_printk("%s %d txie[%d]->dw1 0x%x\n", __FUNCTION__, __LINE__,i,pTxdma_Info->txie[i].dw1);
			}
			priv->pshare->rf_ft_var.print_sta_txdesc = 0xff;
		}
	}
}
#endif

/** @brief -- fill txDMAinfo.
  *
  * @param param_out None
  * @param param_in  priv:  which Wi-Fi interface.
  *                  pstat: which station.
  *                  txcfg : tx-config
  * @return 0: OK, 1: others
 **/
static __IRAM_IN_865X int rtl88XX_fill_txDMAInfo(struct rtl8192cd_priv *priv, struct tx_insn *txcfg, PH2D_TXDMA_INFO_DATA pTxdma_Info)
{
    unsigned char *pbuf = (unsigned char *)(txcfg->phdr - sizeof(struct h2d_txpkt_info));
    struct stat_info *pstat = txcfg->pstat;
	unsigned short protocol;

#ifdef TX_SHORTCUT
			int idx=0;
#endif
int i;
    if (!pTxdma_Info)
        return 1;

    if (txcfg->h2d_type == TXPKTINFO_TYPE_H2D) {
        pTxdma_Info->dw0_ie_end = 1;
        pTxdma_Info->dw0_agg_en = 0;
        pTxdma_Info->dw0_pkt_offset = 0;
        pTxdma_Info->dw0_offset = offsetof(H2D_TXDMA_INFO_DATA, txie);
    }
    else {
        update_txie(priv, txcfg);
        if (pstat)
            memcpy(pTxdma_Info->txie, pstat->txie, sizeof(struct h2d_txie_t) * MAX_TXIE_NUMBER);
        else
            memcpy(pTxdma_Info->txie, priv->txie, sizeof(struct h2d_txie_t) * MAX_TXIE_NUMBER);

        pTxdma_Info->dw0_ie_end = 0;
        pTxdma_Info->dw0_agg_en = !!txcfg->aggre_en;  //Aggregate this MPDU if possible.
        //pTxdma_Info->dw0_bk = 0;      //Aggregation lasting till previous MPDU.
        pTxdma_Info->dw0_pkt_offset = ((MAX_TXIE_NUMBER - 1) * sizeof(struct h2d_txie_t)) >> 3; /* Skip IE5 at this moment */
        pTxdma_Info->dw0_offset = sizeof(H2D_TXDMA_INFO) - sizeof(struct h2d_txie_t); /* Skip IE5 at this moment */
			pTxdma_Info->dw2_txdesc_checksum = txcfg->fr_len;
    }

    if (txcfg->h2d_type == TXPKTINFO_TYPE_8023 || txcfg->h2d_type == TXPKTINFO_TYPE_AMSDU) {
        pTxdma_Info->dw0_txpktsize = txcfg->fr_len + sizeof(struct h2d_txpkt_info)
                                   + txcfg->hdr_len;
    } else {
        pTxdma_Info->dw0_txpktsize = txcfg->fr_len + sizeof(struct h2d_txpkt_info)
                                   + txcfg->hdr_len + txcfg->llc;
        if (txcfg->privacy) {
            if (txcfg->use_sw_enc)
                pTxdma_Info->dw0_txpktsize += txcfg->iv + txcfg->icv + txcfg->mic;
            else
                pTxdma_Info->dw0_txpktsize += txcfg->iv;
        }
    }
    pTxdma_Info->dw1_qsel = MappingQSel(priv, txcfg->q_num, txcfg->h2d_tid);
    pTxdma_Info->dw1_macid = (txcfg->pstat ? REMAP_AID(txcfg->pstat) : 0);

    if (txcfg->h2d_type == TXPKTINFO_TYPE_AMSDU)
        pTxdma_Info->dw2_dma_pri = 1;

    pTxdma_Info->dw3_dma_channel = MappingChannelDMA(txcfg->q_num);
    pTxdma_Info->dw3_offload_size = pTxdma_Info->dw0_offset + pTxdma_Info->dw0_txpktsize; //full load //txdesc+txpktinfo+payload

	
	
#ifdef TX_SHORTCUT
	if (txcfg->fr_type == _SKB_FRAME_TYPE_)
		protocol = ntohs(*((UINT16 *)((UINT8 *)(((struct sk_buff *)txcfg->pframe)->data) + ETH_ALEN*2)));

			if (!priv->pmib->dot11OperationEntry.disable_txsc && txcfg->pstat &&
						(txcfg->fr_type == _SKB_FRAME_TYPE_) && 
						(!IS_MCAST(((struct sk_buff *)txcfg->pframe)->data)) &&
						(protocol == 0x0800) &&
						(txcfg->frg_num == 1) &&
						((txcfg->privacy == 0)
					#ifdef CONFIG_RTL_WAPI_SUPPORT
						|| (txcfg->privacy == _WAPI_SMS4_)
					#endif
						|| (!txcfg->use_sw_enc)
						) &&
#if 0						
						(
					#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
						(isWiFiHdr == FALSE) ? TRUE : 
					#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
						(!GetMData(txcfg->phdr))
						) &&
#endif
					#ifdef SUPPORT_TX_AMSDU_SHORTCUT				
						txcfg->aggre_en <= FG_AGGRE_MSDU_FIRST
					#else
						txcfg->aggre_en < FG_AGGRE_MSDU_FIRST
					#endif
						&& !txcfg->skip_txsc
				) {

					if(priv->pmib->dot118021xAuthEntry.dot118021xAlgrthm && txcfg->privacy == 0) {
						return 0;	
					}
				#ifdef SUPPORT_TX_AMSDU_SHORTCUT
					struct wlan_ethhdr_t ethhdr;
				#endif
				#ifdef CONFIG_RTK_MESH
					if(txcfg->is_11s) {
						panic_printk("No implemented in this path\n");
						idx = get_tx_sc_free_entry(priv, txcfg->pstat, &priv->ethhdr, txcfg->aggre_en);
					} else
				#endif
					{
					#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
						if (isWiFiHdr == FALSE) {
							panic_printk("No implemented in this path\n");
							idx = get_tx_sc_free_entry(priv, txcfg->pstat, pbuf, txcfg->aggre_en);
						} else
					#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
						{
						#if 0//def SUPPORT_TX_AMSDU_SHORTCUT
							if(txcfg->aggre_en == FG_AGGRE_MSDU_FIRST){
								panic_printk("No implemented in this path\n");
								memcpy((void *)&ethhdr, pbuf, 12);
								memcpy((void *)&(ethhdr.type), pbuf+20, 2);
								idx = get_tx_sc_free_entry(priv, txcfg->pstat, (unsigned char *)(((struct sk_buff *)txcfg->pframe)->data), txcfg->aggre_en);
							}
							else
						#endif
							idx = get_tx_sc_free_entry(priv, txcfg->pstat, (unsigned char *)(((struct sk_buff *)txcfg->pframe)->data), txcfg->aggre_en);
						}
					}
				#ifdef CONFIG_RTK_MESH
					if(txcfg->is_11s) {
						memcpy((void *)&txcfg->pstat->tx_sc_ent[idx].ethhdr, &priv->ethhdr, sizeof(struct wlan_ethhdr_t));
					} else
				#endif
					{
					#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
						if (isWiFiHdr == FALSE) 
						{
							memcpy((void *)&txcfg->pstat->tx_sc_ent[idx].ethhdr, pbuf, sizeof(struct wlan_ethhdr_t));
						} else
					#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
						{
						#if 0//def SUPPORT_TX_AMSDU_SHORTCUT
							if(txcfg->aggre_en == FG_AGGRE_MSDU_FIRST){
								memcpy((void *)&txcfg->pstat->tx_sc_ent[idx].amsdullcsnaphdr, pbuf, sizeof(struct wlan_amsdullcsnaphdr_t));
								memcpy((void *)&txcfg->pstat->tx_sc_ent[idx].ethhdr, &ethhdr, sizeof(struct wlan_ethhdr_t));
							}
							else
						#endif
							memcpy((void *)&txcfg->pstat->tx_sc_ent[idx].ethhdr, (unsigned char *)(((struct sk_buff *)txcfg->pframe)->data), sizeof(struct wlan_ethhdr_t));
						}
					}
	
					txcfg->pstat->protection = priv->pmib->dot11ErpInfo.protection;
					txcfg->pstat->ht_protection = priv->ht_protection;
					txcfg->pstat->tx_sc_ent[idx].pktpri = ((struct sk_buff *)txcfg->pframe)->cb[1];
	
				#if 0
				#if defined(WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC)
					if(IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC(priv))	{
						desc_data.txwifiCp = TRUE;
						txcfg->pstat->tx_sc_hw_idx = idx;
					}
				#elif defined(SUPPORT_TXDESC_IE)
					if(IS_SUPPORT_TXDESC_IE(priv))	 {
						txcfg->pstat->tx_sc_hw_idx = idx;
					}
				#endif // WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC
				#endif
					
				#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
					if(IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(priv))	{
						if (convHdr == HW_TX_SC_BACKUP_HEADER) {
							desc_data.macCp = TRUE; 	// backup 802.11 header info.
							desc_data.smhEn = FALSE;
	                #ifdef SUPPORT_TX_HW_AMSDU
							desc_data.HW_AMSDU = FALSE;
	                #endif
	                #if IS_RTL88XX_MAC_V4              
						if(_GET_HAL_DATA(priv)->MacVersion.is_MAC_v4){
							desc_data.smhCamIdx = GET_HAL_INTERFACE(priv)->GetSmhCamHandler(priv, txcfg->pstat);
							if(desc_data.smhCamIdx == SMHCAM_SEARCH_FAIL){
								desc_data.smhCamIdx = GET_HAL_INTERFACE(priv)->FindFreeSmhCamHandler(priv, txcfg->pstat);
								if(desc_data.smhCamIdx == SMHCAM_IS_FULL){
									DEBUG_WARN("SMH CAM is fill.\n");
								}
							}
						}
	                #endif                    
						} else if (convHdr == HW_TX_SC_HEADER_CONV) {
							desc_data.macCp = FALSE;
							desc_data.smhEn = TRUE; 	// auto conv hdr  (802.3 -> 802.11) 
				    #ifdef SUPPORT_TX_HW_AMSDU
							desc_data.HW_AMSDU = TRUE;
	                #endif
							// TODO: consider AES for HW_TX_SHORTCUT_HDR_CONV
							// Eth hdr 14 bytes => gen llc 8 bytes
							//								gen iv 8 bytes
							// skb: ethHdr + data
							// ethHdr + (iv + llc)	+ data
							// BD[0]: txDesc, BD[1]: ethHdr, BD[2]: iv+llc, BD[3]: data
						#if IS_RTL88XX_MAC_V4 
							if(_GET_HAL_DATA(priv)->MacVersion.is_MAC_v4){
								desc_data.smhCamIdx = GET_HAL_INTERFACE(priv)->GetSmhCamHandler(priv, txcfg->pstat);
								if(desc_data.smhCamIdx == SMHCAM_SEARCH_FAIL){
									DEBUG_WARN("SMH CAM search fail.\n");
								}
							}
	                    #endif 
						}
					}
				#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
	
					{
					#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
						if (isWiFiHdr == FALSE) {
							desc_data.pHdr	 = txcfg->phdr;
							desc_data.hdrLen = txcfg->fr_len;
							desc_data.llcLen = 0;
							desc_data.frLen  = 0;
						}
					#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
						memcpy(txcfg->pstat->tx_sc_ent[idx].hal_hw_desc, pTxdma_Info, sizeof(H2D_TXDMA_INFO_DATA));
						memcpy(&txcfg->pstat->tx_sc_ent[idx].txcfg, txcfg, sizeof(struct tx_insn));
						memcpy(&txcfg->pstat->tx_sc_ent[idx].txpkt_info, txcfg->phdr - sizeof(struct h2d_txpkt_info), sizeof(struct h2d_txpkt_info));
					}
			}
			else {
				#if 0
				if (txcfg->pstat) {
					for (idx=0; idx<MAX_TXSC_ENTRY; idx++) {
						GET_HAL_INTERFACE(priv)->SetShortCutTxBuffSizeHandler(priv, txcfg->pstat->tx_sc_ent[idx].hal_hw_desc, 0);
					}
				}
				#endif
			}

#endif
		
	if (txcfg->pstat){
		if(txcfg->pstat->cmn_info.aid == priv->pshare->rf_ft_var.print_sta_txdesc){

			panic_printk("%s %d IE body:\n", __FUNCTION__, __LINE__);
			panic_printk("0x");	
			for (i=15;i>=0;i--){
				panic_printk("%02x",*((unsigned char *)pTxdma_Info+i));
			}
			panic_printk("\n");
			for (i=0;i<=5;i++){
				panic_printk("%s %d txie[%d]->dw0 0x%x\n", __FUNCTION__, __LINE__,i,pTxdma_Info->txie[i].dw0);
				panic_printk("%s %d txie[%d]->dw1 0x%x\n", __FUNCTION__, __LINE__,i,pTxdma_Info->txie[i].dw1);
			}
			priv->pshare->rf_ft_var.print_sta_txdesc = 0xff;
		}
	}

    return 0;
}

VOID
SetTxBufferDesc88XX (
    IN      HAL_PADAPTER    Adapter,
    IN      u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN      PVOID           pDescData,
    IN      u1Byte          setTxbdSource
);

/** @brief -- prepare HTXBD/HTXDesc/pktinfo for offload path.
  *
  * @param param_out None
  * @param param_in  priv  : which Wi-Fi interface.
  *                  pstat : which station.
  *                  txcfg : tx-config
  * @return 0: OK, 1: others
 **/
__IRAM_IN_865X int rtl88XX_signin_offload_htxdesc(struct rtl8192cd_priv *priv, struct tx_insn *txcfg)
{
    struct tx_desc_info     *pswdescinfo, *pdescinfo;
    u2Byte                  *tx_head;
    u4Byte                  q_num = txcfg->q_num;
    unsigned char           *pbuf, *pwlhdr, *picv, *pmic;
    struct rtl8192cd_hw     *phw = GET_HW(priv);
    u32 halQNum = GET_HAL_INTERFACE(priv)->MappingTxQueueHandler(priv, q_num);

    PHCI_TX_DMA_MANAGER_88XX ph2d_tx_dma = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PTxDMA88XX);
    PHCI_TX_DMA_QUEUE_STRUCT_88XX cur_q = &(ph2d_tx_dma->tx_queue[halQNum]);
    H2D_TXDMA_INFO             h2d_txdma_info;
    TX_DESC_DATA_88XX      h2d_desc_data;
    int txpktinfo_len = sizeof(struct h2d_txpkt_info);

    memset(&h2d_desc_data, 0, sizeof(TX_DESC_DATA_88XX));
    memset(&h2d_txdma_info, 0, sizeof(H2D_TXDMA_INFO));

    tx_head     = (u2Byte *)(&(cur_q->host_idx));
    pswdescinfo = get_txdesc_info(priv, priv->pshare->pdesc_info, q_num);
    pdescinfo = pswdescinfo + *tx_head;

	if (txcfg->fr_type == _SKB_FRAME_TYPE_)
		pbuf = ((struct sk_buff *)txcfg->pframe)->data;
	else
		pbuf = (unsigned char*)txcfg->pframe;

	if(txcfg->fr_type == _SKB_FRAME_TYPE_) {
		txcfg->h2d_tid = ((struct sk_buff *)txcfg->pframe)->cb[1];
#ifdef WMM_DSCP_C42
		{
			unsigned int 	   q_num;
			q_num = pri_to_qnum(priv, txcfg->h2d_tid);

			if(q_num ^ txcfg->q_num) 
				txcfg->h2d_tid = 0x04;
		}
#endif
	}

    priv->cnt_offload_nosc++;
    rtl88XX_fill_txPktInfo(priv, txcfg, txcfg->h2d_type);
    rtl88XX_fill_txDMAInfo(priv, txcfg, (PH2D_TXDMA_INFO_DATA)(&h2d_txdma_info));

    h2d_desc_data.pHdr = (unsigned char *)(txcfg->phdr - txpktinfo_len);
    h2d_desc_data.hdrLen = txcfg->hdr_len + txpktinfo_len;

    if (txcfg->fr_type == _SKB_FRAME_TYPE_)
        h2d_desc_data.pBuf = ((struct sk_buff *)txcfg->pframe)->data;
    else
        h2d_desc_data.pBuf = (unsigned char*)txcfg->pframe;

    h2d_desc_data.frLen = txcfg->fr_len;
    h2d_desc_data.ph2d_txdma_info = (PVOID)&h2d_txdma_info;


    GET_HAL_INTERFACE(priv)->FillTxHwCtrlHandler(priv, halQNum, (void *)&h2d_desc_data);

    /* for tx-recycle using */
    if ((txcfg->fr_type == _SKB_FRAME_TYPE_))
        pdescinfo->type = _PRE_ALLOCLLCHDR_;
    else
        pdescinfo->type = _PRE_ALLOCHDR_;
    pdescinfo->pframe = txcfg->phdr;
    pdescinfo->len = h2d_desc_data.hdrLen;
	#if defined(TAROKO_0)	//chestre: tbd with yl
    pdescinfo->paddr = HAL_VIRT_TO_BUS1(priv, h2d_desc_data.pHdr, h2d_desc_data.hdrLen, PCI_DMA_TODEVICE);
    #else
    pdescinfo->paddr = get_physical_addr(priv, h2d_desc_data.pHdr, h2d_desc_data.hdrLen, PCI_DMA_TODEVICE);
	#endif

    if (txcfg->fr_len != 0) {
        pdescinfo->buf_type[0]      = txcfg->fr_type;
        pdescinfo->buf_pframe[0]    = txcfg->pframe;
        pdescinfo->buf_len[0]       = txcfg->fr_len;
		#if defined(TAROKO_0) //chestre: tbd with yl
		pdescinfo->buf_paddr[0]     = HAL_VIRT_TO_BUS1(priv, h2d_desc_data.pBuf, txcfg->fr_len, PCI_DMA_TODEVICE);
		#else
        pdescinfo->buf_paddr[0]     = get_physical_addr(priv, h2d_desc_data.pBuf, txcfg->fr_len, PCI_DMA_TODEVICE);
		#endif
    }


    GET_HAL_INTERFACE(priv)->SyncSWTXBDHostIdxToHWHandler(priv, halQNum);

    return SUCCESS;
}

#ifdef TX_SHORTCUT
inline /*__IRAM_IN_865X*/ int rtl88XX_signin_offload_htxdesc_shortcut(struct rtl8192cd_priv *priv, struct tx_insn *txcfg, struct tx_sc_entry *sc)
{
    struct tx_desc_info     *pswdescinfo, *pdescinfo;
    u2Byte                  *tx_head;
    u4Byte                  q_num = txcfg->q_num;
    unsigned char           *pbuf, *pwlhdr, *picv, *pmic;
    struct rtl8192cd_hw     *phw = GET_HW(priv);
    u32 halQNum = GET_HAL_INTERFACE(priv)->MappingTxQueueHandler(priv, q_num);

    PHCI_TX_DMA_MANAGER_88XX ph2d_tx_dma = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PTxDMA88XX);
    PHCI_TX_DMA_QUEUE_STRUCT_88XX cur_q = &(ph2d_tx_dma->tx_queue[halQNum]);
    TX_DESC_DATA_88XX      h2d_desc_data;
    int txpktinfo_len = sizeof(struct h2d_txpkt_info);
#ifdef _BIG_ENDIAN_
	u32 pktinfo_dw1;
#endif

    memset(&h2d_desc_data, 0, sizeof(TX_DESC_DATA_88XX));

    tx_head     = (u2Byte *)(&(cur_q->host_idx));
    pswdescinfo = get_txdesc_info(priv, priv->pshare->pdesc_info, q_num);
    pdescinfo = pswdescinfo + *tx_head;

#if 0
    rtl88XX_fill_txPktInfo(priv, txcfg, txcfg->h2d_type);
#else
	memcpy(txcfg->phdr - txpktinfo_len, &sc->txpkt_info, txpktinfo_len);

#ifdef CONFIG_RTL9607C
	pktinfo_dw1 = *((u32 *)(txcfg->phdr - txpktinfo_len)+1);

	if ((txcfg->aggre_en == FG_AGGRE_MSDU_LAST) || (txcfg->last_msdu == 1))
		pktinfo_dw1 |= BIT(13);

	if (txcfg->h2d_type == TXPKTINFO_TYPE_AMSDU)
		pktinfo_dw1 = (pktinfo_dw1 & ~BIT(24)) | (BIT(25)|BIT(26));
	else
		pktinfo_dw1 = (pktinfo_dw1 & ~BIT(26)) | (BIT(24)|BIT(25));

	*((u32 *)(txcfg->phdr - txpktinfo_len)+1) = pktinfo_dw1;
#else
#ifdef _BIG_ENDIAN_
	pktinfo_dw1 = le32_to_cpu(*((u32 *)(txcfg->phdr - txpktinfo_len)+1));
	if ((txcfg->aggre_en == FG_AGGRE_MSDU_LAST) || (txcfg->last_msdu == 1))
		pktinfo_dw1 |= BIT(21);

	pktinfo_dw1 = (pktinfo_dw1 & ~0x7) | (txcfg->h2d_type);
	*((u32 *)(txcfg->phdr - txpktinfo_len)+1) = cpu_to_le32(pktinfo_dw1);
	
#else
	if(txcfg->aggre_en == FG_AGGRE_MSDU_LAST) 
		((struct h2d_txpkt_info *)(txcfg->phdr - txpktinfo_len))->last_msdu = 1;
	else
		((struct h2d_txpkt_info *)(txcfg->phdr - txpktinfo_len))->last_msdu = 0;

	((struct h2d_txpkt_info *)(txcfg->phdr - txpktinfo_len))->pkt_type = txcfg->h2d_type;
#endif
#endif
#endif
		{
			PHCI_TX_DMA_MANAGER_88XX		 ph2d_tx_dma = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PTxDMA88XX);
			PHCI_TX_DMA_QUEUE_STRUCT_88XX	cur_q = &(ph2d_tx_dma->tx_queue[halQNum]);
			PTX_DESC_88XX			  ptx_desc = ((PTX_DESC_88XX)(cur_q->ptx_desc_head)) + cur_q->host_idx;
			
			rtl88XX_fill_txDMAInfo_shortcut(priv, txcfg, (struct tx_sc_entry *)(sc), (PH2D_TXDMA_INFO_DATA)(ptx_desc));		
	
			h2d_desc_data.pHdr = (unsigned char *)(txcfg->phdr - txpktinfo_len);
			h2d_desc_data.hdrLen = txcfg->hdr_len + txpktinfo_len;
	
			if (txcfg->fr_type == _SKB_FRAME_TYPE_)
				h2d_desc_data.pBuf = ((struct sk_buff *)txcfg->pframe)->data;
			else
				h2d_desc_data.pBuf = (unsigned char*)txcfg->pframe;
	
			h2d_desc_data.frLen = txcfg->fr_len;
			h2d_desc_data.ph2d_txdma_info = ptx_desc;

#ifdef _BIG_ENDIAN_
			{
				int i, *src, *dst;
				u8 tmp[16];
				memcpy(tmp, ptx_desc, 16);
				src = (int *)tmp;
				dst = (int *)ptx_desc;
				for (i=0; i<4; i++) /* TxDMAInfo */
					*dst++ = cpu_to_le32(*src++);
				//for (i=0; i<12; i++) /* TXIEs */
				//	*dst++ = *src++;
			}
#endif

#if defined(TAROKO_0)
			GET_HAL_INTERFACE(priv)->SetTxBufferDescHandler(priv, halQNum, (void *)&h2d_desc_data,SET_TXBD_SOURCE_SHORTCUT);
#else
			SetTxBufferDesc88XX(priv, halQNum, (void *)&h2d_desc_data,SET_TXBD_SOURCE_SHORTCUT);
#endif			
					priv->cnt_offload_sc++;
		} 


    /* for tx-recycle using */
    if ((txcfg->fr_type == _SKB_FRAME_TYPE_))
        pdescinfo->type = _PRE_ALLOCLLCHDR_;
    else
        pdescinfo->type = _PRE_ALLOCHDR_;
    pdescinfo->pframe = txcfg->phdr;
    pdescinfo->len = h2d_desc_data.hdrLen;
    pdescinfo->paddr = get_physical_addr(priv, h2d_desc_data.pHdr, h2d_desc_data.hdrLen, PCI_DMA_TODEVICE);

    if (txcfg->fr_len != 0) {
        pdescinfo->buf_type[0]      = txcfg->fr_type;
        pdescinfo->buf_pframe[0]    = txcfg->pframe;
        pdescinfo->buf_len[0]       = txcfg->fr_len;
        pdescinfo->buf_paddr[0]     = get_physical_addr(priv, h2d_desc_data.pBuf, txcfg->fr_len, PCI_DMA_TODEVICE);
    }


    GET_HAL_INTERFACE(priv)->SyncSWTXBDHostIdxToHWHandler(priv, halQNum);

    return SUCCESS;
}
#endif	