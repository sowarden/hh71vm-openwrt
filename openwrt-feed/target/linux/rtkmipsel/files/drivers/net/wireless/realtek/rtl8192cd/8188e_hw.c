/*
 *  Routines to access hardware
 *
 *  Copyright (c) 2017 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifdef __ECOS
#include <cyg/io/eth/rltk/819x/wrapper/sys_support.h>
#include <cyg/io/eth/rltk/819x/wrapper/skbuff.h>
#include <cyg/io/eth/rltk/819x/wrapper/timer.h>
#include <cyg/io/eth/rltk/819x/wrapper/wrapper.h>
#endif


#define _8188E_HW_C_

#include "8192cd_cfg.h"
#include "8192cd.h"
#include "8192cd_util.h"

#include "8192c_reg.h"
#include "8188e_reg.h"

#ifdef CONFIG_RTL_88E_SUPPORT

#ifdef TXREPORT
#ifdef __KERNEL__
#include <linux/kernel.h>
#endif
#include "8192cd_debug.h"
#include "8192cd_headers.h"



void RTL8188E_EnableTxReport(struct rtl8192cd_priv *priv)
{
	DEBUG_INFO("%s %d\n", __FUNCTION__, __LINE__);
	RTL_W16(REG_88E_TXRPT_TIM, 0xffff); /* unit: 32us */
	RTL_W32(REG_88E_TXRPT_CTRL, TXRPT_CTRL_88E_TXRPT_EN);
}


void RTL8188E_DisableTxReport(struct rtl8192cd_priv *priv)
{
	DEBUG_INFO("%s %d\n", __FUNCTION__, __LINE__);
	RTL_W32(REG_88E_TXRPT_CTRL, 0);
	RTL_W16(REG_88E_TXRPT_TIM, 0);
}


void RTL8188E_ResumeTxReport(struct rtl8192cd_priv *priv)
{
	DEBUG_INFO("%s %d\n", __FUNCTION__, __LINE__);
	RTL_W32(REG_88E_TXRPT_CTRL,  RTL_R32(REG_88E_TXRPT_CTRL) | TXRPT_CTRL_88E_TXRPT_TIM_EN);
}


void RTL8188E_SuspendTxReport(struct rtl8192cd_priv *priv)
{
	DEBUG_INFO("%s %d\n", __FUNCTION__, __LINE__);
	RTL_W32(REG_88E_TXRPT_CTRL,  RTL_R32(REG_88E_TXRPT_CTRL) & ~TXRPT_CTRL_88E_TXRPT_TIM_EN);
}


static unsigned int findout_max_macid(struct rtl8192cd_priv *priv)
{
	struct stat_info	*pstat;
	struct list_head	*phead, *plist;
	unsigned int max_macid = 0;
#if defined(SMP_SYNC)
	unsigned long flags = 0;
#endif

	phead = &priv->asoc_list;
#if defined(SMP_SYNC)
	SMP_LOCK_ASOC_LIST(flags);
#endif
	plist = phead->next;
	while(plist != phead) {
		pstat = list_entry(plist, struct stat_info, asoc_list);
        plist = plist->next;

		update_remapAid(priv, pstat);
        if (pstat->cmn_info.ra_info.disable_ra)
            continue;

        if (REMAP_AID(pstat) > max_macid)
			max_macid = REMAP_AID(pstat);
	}
#if defined(SMP_SYNC)
	SMP_UNLOCK_ASOC_LIST(flags);
#endif
	return max_macid;
}

#ifdef RATEADAPTIVE_BY_ODM
void rtl8188e_set_station_tx_rate_info(struct dm_struct*	p_dm_odm, struct _odm_ra_info_* pRAInfo, int MacID)
{
	struct rtl8192cd_priv *priv = p_dm_odm->priv;
	struct sta_info* pstat = p_dm_odm->p_odm_sta_info[MacID];
#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
	struct aid_obj *aidobj;
#endif
	
	if( !MacID || !pstat)
		return;
	
#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
	aidobj = container_of(pstat, struct aid_obj, station);
	priv = aidobj->priv;
#endif
	
	if (priv->pmib->dot11StationConfigEntry.autoRate) {
		if (pRAInfo->rate_sgi)
			pstat->ht_current_tx_info |= TX_USE_SHORT_GI;
		else
			pstat->ht_current_tx_info &= ~TX_USE_SHORT_GI;

		if (priv->pshare->is_40m_bw && (pstat->tx_bw == CHANNEL_WIDTH_40))
			pstat->ht_current_tx_info |= TX_USE_40M_MODE;
		else
			pstat->ht_current_tx_info &= ~TX_USE_40M_MODE;
		
		if ((pRAInfo->decision_rate&0x3f) < 12)
			pstat->current_tx_rate = dot11_rate_table[pRAInfo->decision_rate&0x3f];
		else if ((pRAInfo->decision_rate&0x3f) <= 27)
			pstat->current_tx_rate = HT_RATE_ID|((pRAInfo->decision_rate&0x3f) -12);
		else
			DEBUG_WARN("%s %d, DecisionRate mismatched as 0x%02x\n", __FUNCTION__, __LINE__, pRAInfo->decision_rate);
	}
}
#else
static void rtl8188e_set_station_tx_rate_info(struct rtl8192cd_priv *priv, PSTATION_RA_INFO pRaInfo)
{
	if (pRaInfo->RateSGI)
		pRaInfo->pstat->ht_current_tx_info |= TX_USE_SHORT_GI;
	else
		pRaInfo->pstat->ht_current_tx_info &= ~TX_USE_SHORT_GI;

	if (priv->pshare->is_40m_bw && (pRaInfo->pstat->tx_bw == CHANNEL_WIDTH_40))
		pRaInfo->pstat->ht_current_tx_info |= TX_USE_40M_MODE;
	else
		pRaInfo->pstat->ht_current_tx_info &= ~TX_USE_40M_MODE;

	if ((pRaInfo->DecisionRate&0x3f) < 12)
		pRaInfo->pstat->current_tx_rate = dot11_rate_table[pRaInfo->DecisionRate];
	else if ((pRaInfo->DecisionRate&0x3f) <= 27)
		pRaInfo->pstat->current_tx_rate = HT_RATE_ID|((pRaInfo->DecisionRate&0x3f) -12);
	else
		DEBUG_WARN("%s %d, DecisionRate mismatched as 0x%02x\n", __FUNCTION__, __LINE__, pRaInfo->DecisionRate);
}

#endif

void RTL8188E_AssignTxReportMacId(struct rtl8192cd_priv *priv)
{
	struct rtl8192cd_priv *tmp_root_priv = GET_ROOT(priv);
	unsigned int max_macid = 0;
#ifdef MBSSID
	unsigned int i = 0;
#endif
#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
	unsigned int temp_macid = 0;
#endif

	/* find out the largest macid */
	max_macid = findout_max_macid(tmp_root_priv);
#ifdef UNIVERSAL_REPEATER
	if (GET_VXD_PRIV(tmp_root_priv) && netif_running(GET_VXD_PRIV(tmp_root_priv)->dev)) {
		temp_macid = findout_max_macid(GET_VXD_PRIV(tmp_root_priv));
		if (temp_macid > max_macid)
			max_macid = temp_macid;
	}
#endif
#ifdef MBSSID
	if (tmp_root_priv->pmib->miscEntry.vap_enable) {
		for (i=0; i<RTL8192CD_NUM_VWLAN; i++) {
			if (IS_DRV_OPEN(tmp_root_priv->pvap_priv[i])) {
				temp_macid = findout_max_macid(tmp_root_priv->pvap_priv[i]);
				if (temp_macid > max_macid)
					max_macid = temp_macid;
			}
		}
	}
#endif

	/* assign new macid for tx report setting iff it is different from the previous one */
	if (max_macid && (priv->pshare->txRptMacid != max_macid)) {
		int val = 0;
		val = (RTL_R32(REG_88E_TXRPT_CTRL)
			& (~(TXRPT_CTRL_88E_RPT_MACID_Mask << TXRPT_CTRL_88E_RPT_MACID_SHIFT)))
			| (((max_macid+1) & TXRPT_CTRL_88E_RPT_MACID_Mask) << TXRPT_CTRL_88E_RPT_MACID_SHIFT);
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
		if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
			val	|= TXRPT_CTRL_88E_TXRPT_TIM_EN;
		}
#endif
		RTL_W32(REG_88E_TXRPT_CTRL, val);

		DEBUG_INFO("%s %d, preTxRptMacid: %d, newTxRptMacid: %d\n", __FUNCTION__, __LINE__, priv->pshare->txRptMacid, max_macid);
		priv->pshare->txRptMacid = max_macid;
	}
}

#ifdef DETECT_STA_EXISTANCE
void RTL8188E_DetectSTAExistance(struct rtl8192cd_priv *priv, struct tx_rpt *report, struct stat_info *pstat)
{
    const unsigned int  txFailSecThr= 3;            // threshold of Tx Fail Time (in second)
	const unsigned char	TFRL = 7;					// New Retry Limit value
	const unsigned char	TFRL_FailCnt = 2;			// Tx Fail Count threshold to set Retry Limit
	const unsigned char	TFRL_SetTime = 2;			// Time to set Retry Limit (in second)
	const unsigned char	TFRL_RcvTime = 10;			// Time to recover Retry Limit (in second)	

	if(OPMODE & WIFI_STATION_STATE)
		return;

    if (report->txok)
    { // Reset Counter
        pstat->tx_conti_fail_cnt = 0;
        pstat->tx_last_good_time = priv->up_time;
        if (pstat->leave!= 0) {
            pstat->leave = 0;
            RTL8188E_MACID_NOLINK(priv, 0, REMAP_AID(pstat));
        }
    }
    else if (report->txfail)
    {
		if (pstat->leave)
			return;

#ifdef CONFIG_PCI_HCI
		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
			if((priv->up_time >= (pstat->tx_last_good_time+TFRL_SetTime)) &&
				pstat->tx_conti_fail_cnt >= TFRL_FailCnt && 
				!priv->pshare->bRLShortened )
			{ // Shorten retry limit, because AP spending too much time to send out g mode STA pending packets in HW queue.
				RTL_W16(RL, (TFRL&SRL_Mask)<<SRL_SHIFT|(TFRL&LRL_Mask)<<LRL_SHIFT);
				priv->pshare->bRLShortened = TRUE;
				DEBUG_WARN( "== Shorten RetryLimit to 0x%04X ==\n", RTL_R16(RL) );
			}
		}
#endif
        pstat->tx_conti_fail_cnt += report->txfail;
        if((pstat->tx_conti_fail_cnt >= priv->pshare->rf_ft_var.max_pkt_fail) ||
            (pstat->tx_conti_fail_cnt >= priv->pshare->rf_ft_var.min_pkt_fail && priv->up_time >= (pstat->tx_last_good_time+txFailSecThr) )
            ) {
            DEBUG_INFO("sta aid:%d leave\n", pstat->cmn_info.aid);
            ++(pstat->leave);
            RTL8188E_MACID_NOLINK(priv, 1, REMAP_AID(pstat));
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
			if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
            	free_sta_tx_skb(priv, pstat);
			}
#endif

            // Reset Counter
            pstat->tx_conti_fail_cnt = 0;
            pstat->tx_last_good_time = priv->up_time;
#if defined(BR_SHORTCUT) && defined(WDS)
			if (pstat->state & WIFI_WDS) {
				extern struct net_device *cached_wds_dev;
				extern unsigned char cached_wds_mac[MACADDRLEN];
				cached_wds_dev = NULL;
				memset(cached_wds_mac, 0, MACADDRLEN);
			}
#endif
        }
    }
}
#endif
#ifndef RATEADAPTIVE_BY_ODM

void RTL8188E_TxReportHandler(struct rtl8192cd_priv *priv, struct sk_buff *pskb, unsigned int bitmapLowByte,
	unsigned int bitmapHighByte, struct rx_desc* pdesc)
{
	PSTATION_RA_INFO pRaInfo = NULL;
	unsigned char*	Buffer= NULL;
	unsigned char		i = 0;
	unsigned int		offset = 0;
	unsigned short	minRptTime = 0xffff /*0x927c*/;
	struct rx_frinfo *pfrinfo = get_pfrinfo(pskb);

	struct rtl8192cd_priv *priv_root = priv;
	struct stat_info *pstat;
	struct tx_rpt rpt1;

	for(i = 1; i <= priv->pshare->txRptMacid; i++) {
		if (i >= 32) {
			if (!(bitmapHighByte & BIT(i-32)))
				continue;
		} else { 
			if (!(bitmapLowByte & BIT(i)))
				continue;
		}

		do {
			pstat = get_macidinfo(priv_root, i);
			if (pstat) {
				priv = priv_root;
				break;
			}
#ifdef UNIVERSAL_REPEATER
			if (IS_DRV_OPEN(GET_VXD_PRIV(priv_root))) {
				pstat = get_macidinfo(GET_VXD_PRIV(priv_root), i);
				if (pstat) {
					priv = GET_VXD_PRIV(priv_root);
					break;
				}
			}
#endif
#ifdef MBSSID
			if (priv_root->pmib->miscEntry.vap_enable) {
				int idx;
				for (idx=0; idx<RTL8192CD_NUM_VWLAN; idx++) {
					if (IS_DRV_OPEN(priv_root->pvap_priv[idx])) {
						pstat = get_macidinfo(priv_root->pvap_priv[idx], i);
						if (pstat) {
							priv = priv_root->pvap_priv[idx];
							break;
						}
					}
				}
			}
#endif
		} while (0);
		
		if (NULL == pstat)
			continue;
		
		pRaInfo = &(priv_root->pshare->RaInfo[pstat->cmn_info.aid]);
		if (!pRaInfo->pstat)
			continue;

		offset = i << 3; // Start of entry RA data
		DEBUG_INFO("%s %d, MacId=%d offset=%d\n", __FUNCTION__, __LINE__, i, offset);

		if(offset + 8 > pfrinfo->pktlen) {
			DEBUG_WARN("%s %d, offset + 8 > pfrinfo->pktlen, break!!\n", __FUNCTION__, __LINE__);
			break;
		} else {
			Buffer = pskb->data + offset;
			if (!Buffer[0] && !Buffer[1] && !Buffer[2] && !Buffer[3] && !Buffer[4] && !Buffer[5] && !Buffer[6] && !Buffer[7]) {
				DEBUG_INFO("\n%s %d, rpt but not update for aid:%d\n", __FUNCTION__, __LINE__, i);
#ifdef CONFIG_PCI_HCI
				if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
					RTL_W8(0x3a, 0xff);	/* for CATC triggering only, meaningless */
				}
#endif
				DEBUG_INFO("=====rxdesc=====\n");
				DEBUG_INFO("D0:0x%08x, D1:0x%08x, D2:0x%08x, D3:0x%08x, \nD4:0x%08x, D5:0x%08x",
					get_desc(pdesc->Dword0), get_desc(pdesc->Dword1), get_desc(pdesc->Dword2), get_desc(pdesc->Dword3), 
					get_desc(pdesc->Dword4), get_desc(pdesc->Dword5));
#ifdef CONFIG_PCI_HCI
				if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
					DEBUG_INFO(", D6:0x%08x, D7:0x%08x", get_desc(pdesc->Dword6), get_desc(pdesc->Dword7));
				}
#endif
				DEBUG_INFO("\n=====rxdesc=====\n");
				DEBUG_INFO("=====buffer=====\n");
	/*			printk("B0:0x%08x, B1:0x%08x, B2:0x%08x, B3:0x%08x\n",
					(unsigned int)(pskb->data[0]), (unsigned int)(pskb->data[4]), (unsigned int)(pskb->data[8]), (unsigned int)(pskb->data[12]));*/
				DEBUG_INFO("B0:0x%02x, B1:0x%02x, B2:0x%02x, B3:0x%02x, B4:0x%02x, B5:0x%02x, B6:0x%02x, B7:0x%02x, B8:0x%02x, B9:0x%02x, B10:0x%02x, B11:0x%02x, B12:0x%02x, B13:0x%02x, B14:0x%02x, B15:0x%02x\n",
					pskb->data[0], pskb->data[1], pskb->data[2], pskb->data[3], 
					pskb->data[4], pskb->data[5], pskb->data[6], pskb->data[7], 
					pskb->data[8], pskb->data[9], pskb->data[10], pskb->data[11], 
					pskb->data[12], pskb->data[13], pskb->data[14], pskb->data[15]);
				DEBUG_INFO("=====buffer=====\n\n");
				continue;
			}
				
			pRaInfo->RTY[0] = (unsigned short)(Buffer[1] << 8 | Buffer[0]);
			pRaInfo->RTY[1] = Buffer[2];
			pRaInfo->RTY[2] = Buffer[3];
			pRaInfo->RTY[3] = Buffer[4];
			pRaInfo->RTY[4] = Buffer[5];
			pRaInfo->DROP = Buffer[6];

			pRaInfo->TOTAL = pRaInfo->RTY[0] + pRaInfo->RTY[1] + pRaInfo->RTY[2] + pRaInfo->RTY[3]
							+ pRaInfo->RTY[4] + pRaInfo->DROP;

			DEBUG_INFO("%s %d, macid %d R0=%d R1=%d R2=%d R3=%d R4=%d D0=%d Total=%d, HB: 0x%08x LB: 0x%08x vld_hi:0x%08x, vld_lo:0x%08x\n", 
						__FUNCTION__, __LINE__, i,
						pRaInfo->RTY[0],
						pRaInfo->RTY[1],
						pRaInfo->RTY[2],
						pRaInfo->RTY[3],
						pRaInfo->RTY[4],
						pRaInfo->DROP,
						pRaInfo->TOTAL,
						(unsigned int)(Buffer[7] << 24 | Buffer[6]<<16|Buffer[5] << 8 | Buffer[4]), 
						(unsigned int)(Buffer[3] << 24 | Buffer[2]<<16|Buffer[1] << 8 | Buffer[0]),
						bitmapHighByte, bitmapLowByte);

			if (priv->pmib->dot11StationConfigEntry.autoRate) {
				RateDecision(priv, pRaInfo);
				rtl8188e_set_station_tx_rate_info(priv, pRaInfo);
			}

			rpt1.macid = i;
			rpt1.txok = pRaInfo->RTY[0] + pRaInfo->RTY[1] + pRaInfo->RTY[2] + pRaInfo->RTY[3] + pRaInfo->RTY[4];
			rpt1.txfail = pRaInfo->DROP;
			_txrpt_handler(priv, pstat, &rpt1);
		}

		if(minRptTime > pRaInfo->RptTime)
			minRptTime = pRaInfo->RptTime;
	}	

	if (minRptTime != 0xffff) {
#if defined(CONFIG_PCI_HCI)
		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
			RTL_W16(REG_88E_TXRPT_TIM, minRptTime);
		}
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
		if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
			notify_tx_report_interval_change(priv, minRptTime);
		}
#endif
	}
}


void RTL8188E_SetTxReportTimeByRA(struct rtl8192cd_priv *priv, int extend)
{
//	extern unsigned short DynamicTxRPTTiming[];
	extern unsigned char TxRPTTiming_idx;

//	unsigned short WriteTxRPTTiming;
	unsigned char idx;

	idx=TxRPTTiming_idx;
	if (extend==0) { // back to default timing
		idx=1;  //400ms
	} else if (extend==1) {// increase the timing
		idx+=1;
		if (idx>5)
			idx=5;
	} else if (extend==2) {// decrease the timing
		if(idx!=0)
			idx-=1;
	}
//	WriteTxRPTTiming=DynamicTxRPTTiming[idx];  
	TxRPTTiming_idx=idx;
}
#endif
#endif

#ifdef CONFIG_PCI_HCI
static inline
#endif
void __RTL8188E_MACID_NOLINK(struct rtl8192cd_priv *priv, unsigned int nolink, unsigned int aid)
{
	if (nolink) {
		if (aid > 31)
			RTL_W32(REG_88E_MACID_NOLINK+4, RTL_R32(REG_88E_MACID_NOLINK+4) | BIT(aid -31));
		else
			RTL_W32(REG_88E_MACID_NOLINK, RTL_R32(REG_88E_MACID_NOLINK) | BIT(aid));
	} else {
		if (aid > 31)
			RTL_W32(REG_88E_MACID_NOLINK+4, RTL_R32(REG_88E_MACID_NOLINK+4) & ~BIT(aid-31));
		else
			RTL_W32(REG_88E_MACID_NOLINK, RTL_R32(REG_88E_MACID_NOLINK) & ~BIT(aid));
	}
}

void RTL8188E_MACID_NOLINK(struct rtl8192cd_priv *priv, unsigned int nolink, unsigned int aid)
{
	if (!priv->pshare->rf_ft_var.disable_pkt_nolink) {
#if defined(CONFIG_PCI_HCI)
		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
			__RTL8188E_MACID_NOLINK(priv, nolink, aid);
		}
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
		if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
			notify_macid_no_link_change(priv, aid, nolink);
		}
#endif
	}
}

#ifdef CONFIG_PCI_HCI
static inline
#endif
void __RTL8188E_MACID_PAUSE(struct rtl8192cd_priv *priv, unsigned int pause, unsigned int aid)
{
	if (pause) {
		if (aid > 31)
			RTL_W32(REG_88E_MACID_PAUSE+4, RTL_R32(REG_88E_MACID_PAUSE+4) | BIT(aid -31));
		else
			RTL_W32(REG_88E_MACID_PAUSE, RTL_R32(REG_88E_MACID_PAUSE) | BIT(aid));
	} else {
		if (aid > 31)
			RTL_W32(REG_88E_MACID_PAUSE+4, RTL_R32(REG_88E_MACID_PAUSE+4) & ~BIT(aid -31));
		else
			RTL_W32(REG_88E_MACID_PAUSE, RTL_R32(REG_88E_MACID_PAUSE) & ~BIT(aid));
	}
}

void RTL8188E_MACID_PAUSE(struct rtl8192cd_priv *priv, unsigned int pause, unsigned int aid)
{
	if (!priv->pshare->rf_ft_var.disable_pkt_pause) {
#if defined(CONFIG_PCI_HCI)
		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
			__RTL8188E_MACID_PAUSE(priv, pause, aid);
		}
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
		if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
			notify_macid_pause_change(priv, aid, pause);
		}
#endif
	}
}


#ifdef SUPPORT_RTL8188E_TC
void check_RTL8188E_testChip(struct rtl8192cd_priv *priv)
{
	if (!priv->pshare->rtl8188e_testchip_checked) {
		if (RTL_R8(AFE_XTAL_CTRL+3) == 0x3F)
			priv->pshare->version_id |= 0x100;

		priv->pshare->rtl8188e_testchip_checked++;
	}
}
#endif

#ifndef CALIBRATE_BY_ODM


#define	IQK_DELAY_TIME_88E	10

#define bRFRegOffsetMask	0xfffff	


void _phy_path_a_stand_by(struct rtl8192cd_priv *priv)
{
	phy_set_bb_reg(priv, 0xe28, 0xffffff00, 0x0);
	phy_set_bb_reg(priv, 0x840, bMaskDWord, 0x00010000);
	phy_set_bb_reg(priv, 0xe28, 0xffffff00, 0x808000);
}

void _phy_path_a_fill_iqk_matrix(struct rtl8192cd_priv *priv, char bIQKOK, int	result[][8], unsigned char final_candidate, char bTxOnly)
{
	int	oldval_0, X, TX0_A, reg;
	int	Y, TX0_C;

	if(final_candidate == 0xFF)
		return;

	else if(bIQKOK) 	{
		oldval_0 = (phy_query_bb_reg(priv, REG_OFDM_0_XA_TX_IQ_IMBALANCE, bMaskDWord) >> 22) & 0x3FF;

		X = result[final_candidate][0];
		if ((X & 0x00000200) != 0)
			X = X | 0xFFFFFC00;
		TX0_A = (X * oldval_0) >> 8;
		phy_set_bb_reg(priv, REG_OFDM_0_XA_TX_IQ_IMBALANCE, 0x3FF, TX0_A);
		phy_set_bb_reg(priv, REG_OFDM_0_ECCA_THRESHOLD, BIT(31), ((X* oldval_0>>7) & 0x1));

		Y = result[final_candidate][1];
		if ((Y & 0x00000200) != 0)
			Y = Y | 0xFFFFFC00;
		TX0_C = (Y * oldval_0) >> 8;
		phy_set_bb_reg(priv, REG_OFDM_0_XC_TX_AFE, 0xF0000000, ((TX0_C&0x3C0)>>6));
		phy_set_bb_reg(priv, REG_OFDM_0_XA_TX_IQ_IMBALANCE, 0x003F0000, (TX0_C&0x3F));
		phy_set_bb_reg(priv, REG_OFDM_0_ECCA_THRESHOLD, BIT(29), ((Y* oldval_0>>7) & 0x1));

		if(bTxOnly)		{
//			RTPRINT(FINIT, INIT_IQK, ("_phy_path_a_fill_iqk_matrix only Tx OK\n"));
			return;
		}

		reg = result[final_candidate][2];
		if( RTL_ABS(reg ,0x100) >= 16) 
			reg = 0x100;
		phy_set_bb_reg(priv, REG_OFDM_0_XA_RX_IQ_IMBALANCE, 0x3FF, reg);

		reg = result[final_candidate][3] & 0x3F;
		phy_set_bb_reg(priv, REG_OFDM_0_XA_RX_IQ_IMBALANCE, 0xFC00, reg);

		reg = (result[final_candidate][3] >> 6) & 0xF;
		phy_set_bb_reg(priv, 0xca0, 0xF0000000, reg);
	}
}


void _phy_pi_mode_switch(struct rtl8192cd_priv *priv, char pi_mode)
{
	unsigned int mode;

	DEBUG_INFO("BB Switch to %s mode!\n", (pi_mode ? "PI" : "SI"));

	mode = pi_mode ? 0x01000100 : 0x01000000;
	phy_set_bb_reg(priv, REG_FPGA0_XA_HSSI_PARAMETER1, bMaskDWord, mode);
	phy_set_bb_reg(priv, REG_FPGA0_XB_HSSI_PARAMETER1, bMaskDWord, mode);
}


//bit0 = 1 => Tx OK, bit1 = 1 => Rx OK
unsigned char phy_path_a_iqk_8188e(struct rtl8192cd_priv *priv, unsigned char configPathB)
{
	unsigned int reg_eac, reg_e94, reg_e9c, reg_ea4;
	unsigned char result = 0x00;

	DEBUG_INFO("Path A IQK!\n");

	//path-A IQK setting
	DEBUG_INFO("Path-A IQK setting!\n");
	phy_set_bb_reg(priv, REG_TX_IQK_TONE_A, bMaskDWord, 0x10008c1c);
	phy_set_bb_reg(priv, REG_RX_IQK_TONE_A, bMaskDWord, 0x30008c1c);
	phy_set_bb_reg(priv, REG_TX_IQK_PI_A, bMaskDWord, 0x8214032a);
	phy_set_bb_reg(priv, REG_RX_IQK_PI_A, bMaskDWord, 0x28160000);

	//LO calibration setting
	DEBUG_INFO("LO calibration setting!\n");
	phy_set_bb_reg(priv, REG_IQK_AGC_RSP, bMaskDWord, 0x00462911); //0x001028d1

	//One shot, path A LOK & IQK
	DEBUG_INFO("One shot, path A LOK & IQK!\n");
	phy_set_bb_reg(priv, REG_IQK_AGC_PTS, bMaskDWord, 0xf9000000);
	phy_set_bb_reg(priv, REG_IQK_AGC_PTS, bMaskDWord, 0xf8000000);
	
	// delay x ms
	DEBUG_INFO("Delay %d ms for One shot, path A LOK & IQK.\n", IQK_DELAY_TIME_88E);
	delay_ms(IQK_DELAY_TIME_88E);

	// Check failed
	reg_eac = phy_query_bb_reg(priv, REG_RX_POWER_AFTER_IQK_A_2, bMaskDWord);
	DEBUG_INFO("0xeac = 0x%x\n", reg_eac);
	reg_e94 = phy_query_bb_reg(priv, REG_TX_POWER_BEFORE_IQK_A, bMaskDWord);
	DEBUG_INFO("0xe94 = 0x%x\n", reg_e94);
	reg_e9c= phy_query_bb_reg(priv, REG_TX_POWER_AFTER_IQK_A, bMaskDWord);
	DEBUG_INFO("0xe9c = 0x%x\n", reg_e9c);
	reg_ea4= phy_query_bb_reg(priv, REG_RX_POWER_BEFORE_IQK_A_2, bMaskDWord);
	DEBUG_INFO("0xea4 = 0x%x\n", reg_ea4);

	if(!(reg_eac & BIT(28)) &&		
		(((reg_e94 & 0x03FF0000)>>16) != 0x142) &&
		(((reg_e9c & 0x03FF0000)>>16) != 0x42) )
		result |= 0x01;
	else							//if Tx not OK, ignore Rx
		return result;

#if 0
	if(!(reg_eac & BIT27) &&		//if Tx is OK, check whether Rx is OK
		(((reg_ea4 & 0x03FF0000)>>16) != 0x132) &&
		(((reg_eac & 0x03FF0000)>>16) != 0x36))
		result |= 0x02;
	else
		RTPRINT(FINIT, INIT_IQK, ("Path A Rx IQK fail!!\n"));
#endif	

	return result;

}


//bit0 = 1 => Tx OK, bit1 = 1 => Rx OK
unsigned char phy_path_a_rx_iqk(struct rtl8192cd_priv *priv, char configPathB)
{
	int reg_eac, reg_e94, reg_e9c, reg_ea4, u4tmp;
	unsigned char result = 0x00;

	DEBUG_INFO("Path A Rx IQK!\n");

	//get TXIMR setting
	//modify RXIQK mode table
	DEBUG_INFO("Path-A Rx IQK modify RXIQK mode table!\n");
	phy_set_bb_reg(priv, REG_FPGA0_IQK, 0xffffff00, 0);	
	phy_set_rf_reg(priv, RF_PATH_A, RF_WE_LUT, bRFRegOffsetMask, 0x800a0 );
	phy_set_rf_reg(priv, RF_PATH_A, RF_RCK_OS, bRFRegOffsetMask, 0x30000 );
	phy_set_rf_reg(priv, RF_PATH_A, RF_TXPA_G1, bRFRegOffsetMask, 0x0000f );
	phy_set_rf_reg(priv, RF_PATH_A, RF_TXPA_G2, bRFRegOffsetMask, 0xf117B );
	phy_set_bb_reg(priv, REG_FPGA0_IQK, 0xffffff00, 0x808000);

	//IQK setting
	phy_set_bb_reg(priv, REG_TX_IQK, bMaskDWord, 0x01007c00);
	phy_set_bb_reg(priv, REG_RX_IQK, bMaskDWord, 0x81004800);

	//path-A IQK setting
	phy_set_bb_reg(priv, REG_TX_IQK_TONE_A, bMaskDWord, 0x10008c1c);
	phy_set_bb_reg(priv, REG_RX_IQK_TONE_A, bMaskDWord, 0x30008c1c);
	phy_set_bb_reg(priv, REG_TX_IQK_PI_A, bMaskDWord, 0x82160804);
	phy_set_bb_reg(priv, REG_RX_IQK_PI_A, bMaskDWord, 0x28160000);	

	//LO calibration setting
	DEBUG_INFO("LO calibration setting!\n");
	phy_set_bb_reg(priv, REG_IQK_AGC_RSP, bMaskDWord, 0x0046a911); //0x0010a8d1

	//One shot, path A LOK & IQK
	DEBUG_INFO("One shot, path A LOK & IQK!\n");
	phy_set_bb_reg(priv, REG_IQK_AGC_PTS, bMaskDWord, 0xf9000000);
	phy_set_bb_reg(priv, REG_IQK_AGC_PTS, bMaskDWord, 0xf8000000);
	
	// delay x ms
	DEBUG_INFO("Delay %d ms for One shot, path A LOK & IQK.\n", IQK_DELAY_TIME_88E);
	delay_ms(IQK_DELAY_TIME_88E);

	// Check failed
	reg_eac = phy_query_bb_reg(priv, REG_RX_POWER_AFTER_IQK_A_2, bMaskDWord);
	DEBUG_INFO("0xeac = 0x%x\n", reg_eac);	
	reg_e94 = phy_query_bb_reg(priv, REG_TX_POWER_BEFORE_IQK_A, bMaskDWord);
	DEBUG_INFO("0xe94 = 0x%x\n", reg_e94);
	reg_e9c = phy_query_bb_reg(priv, REG_TX_POWER_AFTER_IQK_A, bMaskDWord);
	DEBUG_INFO("0xe9c = 0x%x\n", reg_e9c);

	if(!(reg_eac & BIT(28)) &&		
		(((reg_e94 & 0x03FF0000)>>16) != 0x142) &&
		(((reg_e9c & 0x03FF0000)>>16) != 0x42) )
		result |= 0x01;
	else							//if Tx not OK, ignore Rx
		return result;

	u4tmp = 0x80007C00 | (reg_e94&0x3FF0000)  | ((reg_e9c&0x3FF0000) >> 16);	
	phy_set_bb_reg(priv, REG_TX_IQK, bMaskDWord, u4tmp);
	DEBUG_INFO("0xe40 = 0x%x u4tmp = 0x%x \n", phy_query_bb_reg(priv, REG_TX_IQK, bMaskDWord), u4tmp);	
	

	//RX IQK
	//modify RXIQK mode table
	DEBUG_INFO("Path-A Rx IQK modify RXIQK mode table 2!\n");
	phy_set_bb_reg(priv, REG_FPGA0_IQK, 0xffffff00, 0);		
	phy_set_rf_reg(priv, RF_PATH_A, RF_WE_LUT, bRFRegOffsetMask, 0x800a0 );
	phy_set_rf_reg(priv, RF_PATH_A, RF_RCK_OS, bRFRegOffsetMask, 0x30000 );
	phy_set_rf_reg(priv, RF_PATH_A, RF_TXPA_G1, bRFRegOffsetMask, 0x0000f );
	phy_set_rf_reg(priv, RF_PATH_A, RF_TXPA_G2, bRFRegOffsetMask, 0xf7ffa );
	phy_set_bb_reg(priv, REG_FPGA0_IQK, 0xffffff00, 0x808000);

	//IQK setting
	phy_set_bb_reg(priv, REG_RX_IQK, bMaskDWord, 0x01004800);

	//path-A IQK setting
	phy_set_bb_reg(priv, REG_TX_IQK_TONE_A, bMaskDWord, 0x30008c1c);
	phy_set_bb_reg(priv, REG_RX_IQK_TONE_A, bMaskDWord, 0x10008c1c);
	phy_set_bb_reg(priv, REG_TX_IQK_PI_A, bMaskDWord, 0x82160c05);
	phy_set_bb_reg(priv, REG_RX_IQK_PI_A, bMaskDWord, 0x28160c05);	

	//LO calibration setting
	DEBUG_INFO("LO calibration setting!\n");
	phy_set_bb_reg(priv, REG_IQK_AGC_RSP, bMaskDWord, 0x0046a911); //0x0010a8d1

	//One shot, path A LOK & IQK
	DEBUG_INFO("One shot, path A LOK & IQK!\n");
	phy_set_bb_reg(priv, REG_IQK_AGC_PTS, bMaskDWord, 0xf9000000);
	phy_set_bb_reg(priv, REG_IQK_AGC_PTS, bMaskDWord, 0xf8000000);
	
	// delay x ms
	DEBUG_INFO("Delay %d ms for One shot, path A LOK & IQK.\n", IQK_DELAY_TIME_88E);
	delay_ms(IQK_DELAY_TIME_88E);

	// Check failed
	reg_eac = phy_query_bb_reg(priv, REG_RX_POWER_AFTER_IQK_A_2, bMaskDWord);
	DEBUG_INFO("0xeac = 0x%x\n", reg_eac);
	reg_e94 = phy_query_bb_reg(priv, REG_TX_POWER_BEFORE_IQK_A, bMaskDWord);
	DEBUG_INFO("0xe94 = 0x%x\n", reg_e94);
	reg_e9c= phy_query_bb_reg(priv, REG_TX_POWER_AFTER_IQK_A, bMaskDWord);
	DEBUG_INFO("0xe9c = 0x%x\n", reg_e9c);
	reg_ea4= phy_query_bb_reg(priv, REG_RX_POWER_BEFORE_IQK_A_2, bMaskDWord);
	DEBUG_INFO("0xea4 = 0x%x\n", reg_ea4);

#if 0	
	if(!(reg_eac & BIT28) &&		
		(((reg_e94 & 0x03FF0000)>>16) != 0x142) &&
		(((reg_e9c & 0x03FF0000)>>16) != 0x42) )
		result |= 0x01;
	else							//if Tx not OK, ignore Rx
		return result;
#endif	

	if(!(reg_eac & BIT(27)) &&		//if Tx is OK, check whether Rx is OK
		(((reg_ea4 & 0x03FF0000)>>16) != 0x132) &&
		(((reg_eac & 0x03FF0000)>>16) != 0x36))
		result |= 0x02;
	else
		DEBUG_INFO("Path A Rx IQK fail!!\n");
	
	return result;


}

//bit0 = 1 => Tx OK, bit1 = 1 => Rx OK
unsigned char phy_path_b_iqk_8188e(struct rtl8192cd_priv *priv)
{
	unsigned int reg_eac, reg_eb4, reg_ebc, reg_ec4, reg_ecc;
	unsigned char	result = 0x00;
	DEBUG_INFO("Path B IQK!\n");

	//One shot, path B LOK & IQK
	DEBUG_INFO("One shot, path A LOK & IQK!\n");
	phy_set_bb_reg(priv, REG_IQK_AGC_CONT, bMaskDWord, 0x00000002);
	phy_set_bb_reg(priv, REG_IQK_AGC_CONT, bMaskDWord, 0x00000000);

	// delay x ms
	DEBUG_INFO("Delay %d ms for One shot, path B LOK & IQK.\n", IQK_DELAY_TIME_88E);
	delay_ms(IQK_DELAY_TIME_88E);

	// Check failed
	reg_eac = phy_query_bb_reg(priv, REG_RX_POWER_AFTER_IQK_A_2, bMaskDWord);
	DEBUG_INFO("0xeac = 0x%x\n", reg_eac);
	reg_eb4 = phy_query_bb_reg(priv, REG_TX_POWER_BEFORE_IQK_B, bMaskDWord);
	DEBUG_INFO("0xeb4 = 0x%x\n", reg_eb4);
	reg_ebc= phy_query_bb_reg(priv, REG_TX_POWER_AFTER_IQK_B, bMaskDWord);
	DEBUG_INFO("0xebc = 0x%x\n", reg_ebc);
	reg_ec4= phy_query_bb_reg(priv, REG_RX_POWER_BEFORE_IQK_B_2, bMaskDWord);
	DEBUG_INFO("0xec4 = 0x%x\n", reg_ec4);
	reg_ecc= phy_query_bb_reg(priv, REG_RX_POWER_AFTER_IQK_B_2, bMaskDWord);
	DEBUG_INFO("0xecc = 0x%x\n", reg_ecc);

	if(!(reg_eac & BIT(31)) &&
		(((reg_eb4 & 0x03FF0000)>>16) != 0x142) &&
		(((reg_ebc & 0x03FF0000)>>16) != 0x42))
		result |= 0x01;
	else
		return result;

	if(!(reg_eac & BIT(30)) &&
		(((reg_ec4 & 0x03FF0000)>>16) != 0x132) &&
		(((reg_ecc & 0x03FF0000)>>16) != 0x36))
		result |= 0x02;
	else
		DEBUG_INFO("Path B Rx IQK fail!!\n");
	

	return result;

}


void _phy_iq_calibrate_8188e(struct rtl8192cd_priv *priv, int result[][8], unsigned char t, char is2T)
{
	unsigned int	i;
	unsigned char path_aok=0, path_bok=0;
	unsigned int  ADDA_REG[IQK_ADDA_REG_NUM] = {	
						REG_FPGA0_XCD_SWITCH_CONTROL, 	REG_BLUE_TOOTH, 	
						REG_RX_WAIT_CCA, 		REG_TX_CCK_RFON,
						REG_TX_CCK_BBON, 	REG_TX_OFDM_RFON, 	
						REG_TX_OFDM_BBON, 	REG_TX_TO_RX,
						REG_TX_TO_TX, 		REG_RX_CCK, 	
						REG_RX_OFDM, 		REG_RX_WAIT_RIFS,
						REG_RX_TO_RX, 		REG_STANDBY, 	
						REG_SLEEP, 			REG_PMPD_ANAEN };
						
	unsigned int  IQK_MAC_REG[IQK_MAC_REG_NUM] = {
						REG_TXPAUSE, 		REG_BCN_CTRL,	
						REG_BCN_CTRL_1,	REG_GPIO_MUXCFG};
					
	//since 92C & 92D have the different define in IQK_BB_REG	
	unsigned int  IQK_BB_REG_92C[IQK_BB_REG_NUM] = {
							REG_OFDM_0_TRX_PATH_ENABLE, 		REG_OFDM_0_TR_MUX_PAR,	
							REG_FPGA0_XCD_RF_INTERFACE_SW,	REG_CONFIG_ANT_A,	REG_CONFIG_ANT_B,
							REG_FPGA0_XAB_RF_INTERFACE_SW,	REG_FPGA0_XA_RF_INTERFACE_OE,	
							REG_FPGA0_XB_RF_INTERFACE_OE,	/*REG_FPGA0_RFMOD*/ REG_CCK_0_AFE_SETTING	
							};	

	unsigned int	retryCount = 0;

//	unsigned int 	bbvalue;

#ifdef MP_TEST
	if(priv->pshare->rf_ft_var.mp_specific)
		retryCount = 9; //original 9 
	else
		retryCount = 2;
#else
	retryCount = 2;
#endif

	// Note: IQ calibration must be performed after loading 
	// 		PHY_REG.txt , and radio_a, radio_b.txt	
	
	if(t == 0)
	{
//	 	 bbvalue = phy_query_bb_reg(pAdapter, REG_FPGA0_RFMOD, bMaskDWord);
//			RTPRINT(FINIT, INIT_IQK, ("_phy_iq_calibrate_8188e()==>0x%08x\n",bbvalue));

		//RTPRINT(FINIT, INIT_IQK, ("IQ Calibration for %s for %d times\n", (is2T ? "2T2R" : "1T1R"), t));
		DEBUG_INFO("IQ Calibration for %s for %d times\n", (is2T ? "2T2R" : "1T1R"), t);
	
	 	// Save ADDA parameters, turn Path A ADDA on
	 	_phy_save_adda_registers(priv, ADDA_REG, priv->pshare->ADDA_backup, IQK_ADDA_REG_NUM);
		_phy_save_mac_registers(priv, IQK_MAC_REG, priv->pshare->IQK_MAC_backup);
	 	_phy_save_adda_registers(priv, IQK_BB_REG_92C, priv->pshare->IQK_BB_backup, IQK_BB_REG_NUM);	

	}
	DEBUG_INFO("IQ Calibration for %s for %d times\n", (is2T ? "2T2R" : "1T1R"), t);

		
 	_phy_path_adda_on(priv, ADDA_REG, TRUE, is2T);
		
	
	if(t==0)
	{
		priv->pshare->bRfPiEnable = (unsigned char)phy_query_bb_reg(priv, REG_FPGA0_XA_HSSI_PARAMETER1, BIT(8));
	}
	
	if(!priv->pshare->bRfPiEnable){
		// Switch BB to PI mode to do IQ Calibration.
		_phy_pi_mode_switch(priv, TRUE);
	}
	
	//MAC settings
	_phy_mac_setting_calibration(priv, IQK_MAC_REG, priv->pshare->IQK_MAC_backup);
	
	//BB setting
	//phy_set_bb_reg(priv, REG_FPGA0_RFMOD, BIT(24), 0x00);
	phy_set_bb_reg(priv, REG_CCK_0_AFE_SETTING, bMaskDWord, (0x0f000000 | (phy_query_bb_reg(priv, REG_CCK_0_AFE_SETTING, bMaskDWord))) );	
	phy_set_bb_reg(priv, REG_OFDM_0_TRX_PATH_ENABLE, bMaskDWord, 0x03a05600);
	phy_set_bb_reg(priv, REG_OFDM_0_TR_MUX_PAR, bMaskDWord, 0x000800e4);
	phy_set_bb_reg(priv, REG_FPGA0_XCD_RF_INTERFACE_SW, bMaskDWord, 0x22204000);

	phy_set_bb_reg(priv, REG_FPGA0_XAB_RF_INTERFACE_SW, BIT(10), 0x01);
	phy_set_bb_reg(priv, REG_FPGA0_XAB_RF_INTERFACE_SW, BIT(26), 0x01);	
	phy_set_bb_reg(priv, REG_FPGA0_XA_RF_INTERFACE_OE, BIT(10), 0x00);
	phy_set_bb_reg(priv, REG_FPGA0_XB_RF_INTERFACE_OE, BIT(10), 0x00);	
	

	if(is2T)
	{
		phy_set_bb_reg(priv, REG_FPGA0_XA_LSSI_PARAMETER, bMaskDWord, 0x00010000);
		phy_set_bb_reg(priv, REG_FPGA0_XB_LSSI_PARAMETER, bMaskDWord, 0x00010000);
	}

	//Page B init
	//AP or IQK
	phy_set_bb_reg(priv, REG_CONFIG_ANT_A, bMaskDWord, 0x0f600000);
	
	if(is2T)
	{
		phy_set_bb_reg(priv, REG_CONFIG_ANT_B, bMaskDWord, 0x0f600000);
	}

	// IQ calibration setting
	DEBUG_INFO("IQK setting!\n");		
	phy_set_bb_reg(priv, REG_FPGA0_IQK, 0xffffff00, 0x808000);
	phy_set_bb_reg(priv, REG_TX_IQK, bMaskDWord, 0x01007c00);
	phy_set_bb_reg(priv, REG_RX_IQK, bMaskDWord, 0x81004800);

	for(i = 0 ; i < retryCount ; i++){
		path_aok = phy_path_a_iqk_8188e(priv, is2T);
//		if(path_aok == 0x03){
		if(path_aok == 0x01){
			DEBUG_INFO("Path A Tx IQK Success!!\n");
				result[t][0] = (phy_query_bb_reg(priv, REG_TX_POWER_BEFORE_IQK_A, bMaskDWord)&0x3FF0000)>>16;
				result[t][1] = (phy_query_bb_reg(priv, REG_TX_POWER_AFTER_IQK_A, bMaskDWord)&0x3FF0000)>>16;
			break;
		}
#if 0		
		else if (i == (retryCount-1) && path_aok == 0x01)	//Tx IQK OK
		{
			DEBUG_INFO("Path A IQK Only  Tx Success!!\n");
			
			result[t][0] = (phy_query_bb_reg(priv, REG_TX_POWER_BEFORE_IQK_A, bMaskDWord)&0x3FF0000)>>16;
			result[t][1] = (phy_query_bb_reg(priv, REG_TX_POWER_AFTER_IQK_A, bMaskDWord)&0x3FF0000)>>16;			
		}
#endif
	}

	for(i = 0 ; i < retryCount ; i++){
		path_aok = phy_path_a_rx_iqk(priv, is2T);

		//printk("\n\nPathAOK = 0x%x\n\n", PathAOK);
		
		if(path_aok == 0x03){
			DEBUG_INFO("Path A Rx IQK Success!!\n");
//				result[t][0] = (phy_query_bb_reg(pAdapter, REG_TX_POWER_BEFORE_IQK_A, bMaskDWord)&0x3FF0000)>>16;
//				result[t][1] = (phy_query_bb_reg(pAdapter, REG_TX_POWER_AFTER_IQK_A, bMaskDWord)&0x3FF0000)>>16;
				result[t][2] = (phy_query_bb_reg(priv, REG_RX_POWER_BEFORE_IQK_A_2, bMaskDWord)&0x3FF0000)>>16;
				result[t][3] = (phy_query_bb_reg(priv, REG_RX_POWER_AFTER_IQK_A_2, bMaskDWord)&0x3FF0000)>>16;
			break;
		}
		else
		{
			DEBUG_INFO("Path A Rx IQK Fail!!\n");		
		}
	}

	if(0x00 == path_aok){		
		DEBUG_INFO("Path A IQK failed!!\n");		
	}

	if(is2T){
		_phy_path_a_stand_by(priv);

		// Turn Path B ADDA on
		_phy_path_adda_on(priv, ADDA_REG, FALSE, is2T);

		for(i = 0 ; i < retryCount ; i++){
			path_bok = phy_path_b_iqk_8188e(priv);
			if(path_bok == 0x03){
				DEBUG_INFO("Path B IQK Success!!\n");
				result[t][4] = (phy_query_bb_reg(priv, REG_TX_POWER_BEFORE_IQK_B, bMaskDWord)&0x3FF0000)>>16;
				result[t][5] = (phy_query_bb_reg(priv, REG_TX_POWER_AFTER_IQK_B, bMaskDWord)&0x3FF0000)>>16;
				result[t][6] = (phy_query_bb_reg(priv, REG_RX_POWER_BEFORE_IQK_B_2, bMaskDWord)&0x3FF0000)>>16;
				result[t][7] = (phy_query_bb_reg(priv, REG_RX_POWER_AFTER_IQK_B_2, bMaskDWord)&0x3FF0000)>>16;
				break;
			}
			else if (i == (retryCount - 1) && path_bok == 0x01)	//Tx IQK OK
			{
				DEBUG_INFO("Path B Only Tx IQK Success!!\n");
				result[t][4] = (phy_query_bb_reg(priv, REG_TX_POWER_BEFORE_IQK_B, bMaskDWord)&0x3FF0000)>>16;
				result[t][5] = (phy_query_bb_reg(priv, REG_TX_POWER_AFTER_IQK_B, bMaskDWord)&0x3FF0000)>>16;				
			}
		}

		if(0x00 == path_bok){		
			DEBUG_INFO("Path B IQK failed!!\n");		
		}
	}

	//Back to BB mode, load original value
	DEBUG_INFO("IQK:Back to BB mode, load original value!\n");
	phy_set_bb_reg(priv, REG_FPGA0_IQK, 0xffffff00, 0);

	if(t != 0)
	{
		if(!priv->pshare->bRfPiEnable){
			// Switch back BB to SI mode after finish IQ Calibration.
			_phy_pi_mode_switch(priv, FALSE);
		}

	 	// Reload ADDA power saving parameters
	 	_phy_reload_adda_registers(priv, ADDA_REG, priv->pshare->ADDA_backup, IQK_ADDA_REG_NUM);

		// Reload MAC parameters
		_phy_reload_mac_registers(priv, IQK_MAC_REG, priv->pshare->IQK_MAC_backup);
		
	 	_phy_reload_adda_registers(priv, IQK_BB_REG_92C, priv->pshare->IQK_BB_backup, IQK_BB_REG_NUM);

		// Restore RX initial gain
		phy_set_bb_reg(priv, REG_FPGA0_XA_LSSI_PARAMETER, bMaskDWord, 0x00032ed3);
		if(is2T){
			phy_set_bb_reg(priv, REG_FPGA0_XB_LSSI_PARAMETER, bMaskDWord, 0x00032ed3);
		}
		
		//load 0xe30 IQC default value
		phy_set_bb_reg(priv, REG_TX_IQK_TONE_A, bMaskDWord, 0x01008c00);		
		phy_set_bb_reg(priv, REG_RX_IQK_TONE_A, bMaskDWord, 0x01008c00);				
		
	}
	DEBUG_INFO("_phy_iq_calibrate_8188e() <==\n");
	
}


unsigned char phy_simularity_compare_8188e(struct rtl8192cd_priv *priv, int result[][8], unsigned char c1, unsigned char c2)
{
	unsigned int		i, j, diff, SimularityBitMap, bound = 0;	
	unsigned char		final_candidate[2] = {0xFF, 0xFF};	//for path A and path B
	unsigned char		bResult = TRUE, is2T = FALSE; 
	
	if(is2T)
		bound = 8;
	else
		bound = 4;

	DEBUG_INFO("===> IQK:phy_simularity_compare_8188e c1 %d c2 %d!!!\n", c1, c2);


	SimularityBitMap = 0;
	
	for( i = 0; i < bound; i++ )
	{
		diff = (result[c1][i] > result[c2][i]) ? (result[c1][i] - result[c2][i]) : (result[c2][i] - result[c1][i]);
		if (diff > MAX_TOLERANCE)
		{
			DEBUG_INFO("IQK:phy_simularity_compare_8188e differnece overflow index %d compare1 0x%x compare2 0x%x!!!\n",  i, result[c1][i], result[c2][i]);
		
			if((i == 2 || i == 6) && !SimularityBitMap)
			{
				if(result[c1][i]+result[c1][i+1] == 0)
					final_candidate[(i/4)] = c2;
				else if (result[c2][i]+result[c2][i+1] == 0)
					final_candidate[(i/4)] = c1;
				else
					SimularityBitMap = SimularityBitMap|(1<<i);					
			}
			else
				SimularityBitMap = SimularityBitMap|(1<<i);
		}
	}
	
	DEBUG_INFO("IQK:phy_simularity_compare_8188e SimularityBitMap   %d !!!\n", SimularityBitMap);
	
	if ( SimularityBitMap == 0)
	{
		for( i = 0; i < (bound/4); i++ )
		{
			if(final_candidate[i] != 0xFF)
			{
				for( j = i*4; j < (i+1)*4-2; j++)
					result[3][j] = result[final_candidate[i]][j];
				bResult = FALSE;
			}
		}
		return bResult;
	}
	else if (!(SimularityBitMap & 0x0F))			//path A OK
	{
		for(i = 0; i < 4; i++)
			result[3][i] = result[c1][i];
		return FALSE;
	}
	else if (!(SimularityBitMap & 0xF0) && is2T)	//path B OK
	{
		for(i = 4; i < 8; i++)
			result[3][i] = result[c1][i];
		return FALSE;
	}	
	else		
		return FALSE;
	
}




void phy_iq_calibrate_8188e(struct rtl8192cd_priv *priv, char bReCovery)
{
	int				result[4][8];	//last is final result
	unsigned char	i, final_candidate, Indexforchannel;
	char			bPathAOK, bPathBOK;
	int				RegE94, RegE9C, RegEA4, RegEAC, RegEB4, RegEBC, RegEC4, RegECC, RegTmp = 0;
	char			is12simular, is13simular, is23simular;	
//	char 			bStartContTx = FALSE, bSingleTone = FALSE;
	int				IQK_BB_REG_92C[IQK_BB_REG_NUM] = {
					REG_OFDM_0_XA_RX_IQ_IMBALANCE, 	REG_OFDM_0_XB_RX_IQ_IMBALANCE, 
					REG_OFDM_0_ECCA_THRESHOLD, 	REG_OFDM_0_AGC_RSSI_TABLE,
					REG_OFDM_0_XA_TX_IQ_IMBALANCE, 	REG_OFDM_0_XB_TX_IQ_IMBALANCE, 
					REG_OFDM_0_XC_TX_AFE, 			REG_OFDM_0_XD_TX_AFE, 
					REG_OFDM_0_RX_IQ_EXT_ANTA};

//default disable
//	return;					


#ifdef MP_TEST
	if(priv->pshare->rf_ft_var.mp_specific)
	{
		if((OPMODE & WIFI_MP_CTX_PACKET) || (OPMODE & WIFI_MP_CTX_ST))
			return;
	}
#endif

#if 1 
	if(priv->pshare->IQK_88E_done)
	{
#ifdef __ECOS
		_phy_reload_adda_registers(priv, (unsigned int*)IQK_BB_REG_92C, priv->pshare->IQK_BB_backup_recover, 9);
#else
		_phy_reload_adda_registers(priv, IQK_BB_REG_92C, priv->pshare->IQK_BB_backup_recover, 9);
#endif
		return;
	}

	priv->pshare->IQK_88E_done = 1;
#endif

#if 0
	if(bReCovery)
	{
		_phy_reload_adda_registers(priv, IQK_BB_REG_92C, priv->pshare->IQK_BB_backup_recover, 9);
		return;		
	}
#endif

	printk("DO 8188E IQK !!!\n");
	DEBUG_INFO("IQK:Start!!!\n");

	for(i = 0; i < 8; i++)
	{
		result[0][i] = 0;
		result[1][i] = 0;
		result[2][i] = 0;
		result[3][i] = 0;
	}
	final_candidate = 0xff;
	bPathAOK = FALSE;
	bPathBOK = FALSE;
	is12simular = FALSE;
	is23simular = FALSE;
	is13simular = FALSE;


	//RTPRINT(("IQK !!!interface %d currentband %d ishardwareD %d \n", pAdapter->interfaceIndex, pHalData->CurrentBandType92D, IS_HARDWARE_TYPE_8192D(pAdapter)));
	//RT_TRACE(COMP_INIT,DBG_LOUD,("Acquire Mutex in IQCalibrate \n"));
	
	for (i=0; i<3; i++)
	{

		// For 88C 1T1R  //_eric ?? 
		_phy_iq_calibrate_8188e(priv, result, i, FALSE); 

		if(i == 1)
		{
			is12simular = phy_simularity_compare_8188e(priv, result, 0, 1);
			if(is12simular)
			{
				final_candidate = 0;
				DEBUG_INFO("IQK: is12simular final_candidate is %x\n",final_candidate);				
				break;
			}
		}
		
		if(i == 2)
		{
			is13simular = phy_simularity_compare_8188e(priv, result, 0, 2);
			if(is13simular)
			{
				final_candidate = 0;			
				DEBUG_INFO("IQK: is13simular final_candidate is %x\n",final_candidate);
				
				break;
			}
			
			is23simular = phy_simularity_compare_8188e(priv, result, 1, 2);
			if(is23simular)
			{
				final_candidate = 1;
				DEBUG_INFO("IQK: is23simular final_candidate is %x\n",final_candidate);				
			}
			else
			{
				for(i = 0; i < 8; i++)
					RegTmp += result[3][i];

				if(RegTmp != 0)
					final_candidate = 3;			
				else
					final_candidate = 0xFF;
			}
		}
	}
//	RT_TRACE(COMP_INIT,DBG_LOUD,("Release Mutex in IQCalibrate \n"));

	for (i=0; i<4; i++)
	{
		RegE94 = result[i][0];
		RegE9C = result[i][1];
		RegEA4 = result[i][2];
		RegEAC = result[i][3];
		RegEB4 = result[i][4];
		RegEBC = result[i][5];
		RegEC4 = result[i][6];
		RegECC = result[i][7];
		DEBUG_INFO("IQK: RegE94=%x RegE9C=%x RegEA4=%x RegEAC=%x RegEB4=%x RegEBC=%x RegEC4=%x RegECC=%x\n ", RegE94, RegE9C, RegEA4, RegEAC, RegEB4, RegEBC, RegEC4, RegECC);
	}
	
	if(final_candidate != 0xff)
	{
		priv->pshare->RegE94 = RegE94 = result[final_candidate][0];
		priv->pshare->RegE9C = RegE9C = result[final_candidate][1];
		RegEA4 = result[final_candidate][2];
		RegEAC = result[final_candidate][3];
		priv->pshare->RegEB4 = RegEB4 = result[final_candidate][4];
		priv->pshare->RegEBC = RegEBC = result[final_candidate][5];
		RegEC4 = result[final_candidate][6];
		RegECC = result[final_candidate][7];
		DEBUG_INFO("IQK: final_candidate is %x\n",final_candidate);
		DEBUG_INFO("IQK: RegE94=%x RegE9C=%x RegEA4=%x RegEAC=%x RegEB4=%x RegEBC=%x RegEC4=%x RegECC=%x\n ", RegE94, RegE9C, RegEA4, RegEAC, RegEB4, RegEBC, RegEC4, RegECC);
		bPathAOK = bPathBOK = TRUE;
	}
	else
	{
		DEBUG_INFO("IQK: FAIL use default value\n");
	
		priv->pshare->RegE94 = priv->pshare->RegEB4 = 0x100;	//X default value
		priv->pshare->RegE9C = priv->pshare->RegEBC = 0x0;		//Y default value
	}


	
	if((RegE94 != 0)/*&&(RegEA4 != 0)*/)
	{
		_phy_path_a_fill_iqk_matrix(priv, bPathAOK, result, final_candidate, (RegEA4 == 0));
	}

#if 0
	if (IS_92C_SERIAL(pHalData->VersionID))
	{
		if((RegEB4 != 0)/*&&(RegEC4 != 0)*/)
		{
			_phy_path_b_fill_iqk_matrix(pAdapter, bPathBOK, result, final_candidate, (RegEC4 == 0));
		}
	}
#endif
	 Indexforchannel = 0;

	 for(i = 0; i < IQK_Matrix_REG_NUM; i++)
		 priv->pshare->IQKMatrixRegSetting[Indexforchannel].Value[0][i] = result[final_candidate][i];
	  
	 priv->pshare->IQKMatrixRegSetting[Indexforchannel].bIQKDone = TRUE;
#ifdef __ECOS
	_phy_save_adda_registers(priv, (unsigned int*)IQK_BB_REG_92C, priv->pshare->IQK_BB_backup_recover, 9);
#else
	_phy_save_adda_registers(priv, IQK_BB_REG_92C, priv->pshare->IQK_BB_backup_recover, 9);
#endif

}

void odm_reset_iqk_result(struct rtl8192cd_priv *priv)
{
/*
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN || DM_ODM_SUPPORT_TYPE == ODM_CE)
	PADAPTER	Adapter = p_dm_odm->Adapter;
	u1Byte		i;

	if (!IS_HARDWARE_TYPE_8192D(Adapter))
		return;
#endif
*/

	unsigned char i;

	//printk("PHY_ResetIQKResult:: settings regs %d default regs %d\n", sizeof(priv->pshare->IQKMatrixRegSetting)/sizeof(struct _IQK_MATRIX_REGS_SETTING), IQK_MATRIX_SETTINGS_NUM);
	//0xe94, 0xe9c, 0xea4, 0xeac, 0xeb4, 0xebc, 0xec4, 0xecc

	for(i = 0; i < IQK_MATRIX_SETTINGS_NUM; i++)
	{
		{
			priv->pshare->IQKMatrixRegSetting[i].Value[0][0] = 
				priv->pshare->IQKMatrixRegSetting[i].Value[0][2] = 
				priv->pshare->IQKMatrixRegSetting[i].Value[0][4] = 
				priv->pshare->IQKMatrixRegSetting[i].Value[0][6] = 0x100;

			priv->pshare->IQKMatrixRegSetting[i].Value[0][1] = 
				priv->pshare->IQKMatrixRegSetting[i].Value[0][3] = 
				priv->pshare->IQKMatrixRegSetting[i].Value[0][5] = 
				priv->pshare->IQKMatrixRegSetting[i].Value[0][7] = 0x0;

			priv->pshare->IQKMatrixRegSetting[i].bIQKDone = FALSE;
			
		}
	}

}

#define	OFDM_TABLE_SIZE_92D 	43

#define bRFRegOffsetMask	0xfffff	

extern unsigned int ofdm_swing_table[];
extern const int OFDM_TABLE_SIZE;
extern const int CCK_TABLE_SIZE;
extern unsigned char cck_swing_table_ch14 [][8];
extern unsigned char cck_swing_table_ch1_ch13[][8];


//091212 chiyokolin
void odm_txpowertracking_callback_thermal_meter_8188e(struct rtl8192cd_priv *priv)
{

	//HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	
	unsigned char			ThermalValue = 0, delta, delta_LCK, delta_IQK, offset;
	unsigned char			ThermalValue_AVG_count = 0;
	unsigned int			ThermalValue_AVG = 0;	
	int						ele_A=0, ele_D, /* TempCCk,*/ X, value32;
	int						Y, ele_C=0;
	s1Byte				OFDM_index[2]={0}, CCK_index=0, OFDM_index_old[2]={0}, CCK_index_old=0, index;
	unsigned int			i = 0, j = 0;
	char 					is2T = FALSE;
//	char 					bInteralPA = FALSE;

	unsigned char			OFDM_min_index = 6, rf; //OFDM BB Swing should be less than +3.0dB, which is required by Arthur
	unsigned char			Indexforchannel = 0/*GetRightChnlPlaceforIQK(pHalData->CurrentChannel)*/;
	s1Byte				OFDM_index_mapping[2][index_mapping_NUM_88E] = { 
							{0,	0,	2,	3,	4,	4,			//2.4G, decrease power 
							5, 	6, 	7, 	7,	8,	9,					
							10,	10,	11}, // For lower temperature, 20120220 updated on 20120220.	
							{0,	0,	-1,	-2,	-3,	-4,			//2.4G, increase power 
							-4, 	-4, 	-4, 	-5,	-7,	-8,					
							-9,	-9,	-10},					
							};	
	unsigned char			Thermal_mapping[2][index_mapping_NUM_88E] = { 
							{0,	2,	4,	6,	8,	10,			//2.4G, decrease power 
							12, 	14, 	16, 	18,	20,	22,					
							24,	26,	27},	
							{0, 	2,	4,	6,	8,	10, 			//2.4G,, increase power 
							12, 	14, 	16, 	18, 	20, 	22, 
							25,	25,	25},					
							};	
	
	priv->pshare->TXPowerTrackingCallbackCnt++; //cosa add for debug
	priv->pshare->bTXPowerTrackingInit = TRUE;
    
#if 1 //(MP_DRIVER == 1)      //_eric_??
    priv->pshare->TxPowerTrackControl = 1; //priv->pshare->TxPowerTrackControl; //_eric_?? // <Kordan> We should keep updating the control variable according to HalData.
    // <Kordan> pshare->RegA24 will be initialized when ODM HW configuring, but MP configures with para files.
    priv->pshare->RegA24 = 0x090e1317; 
#endif


#ifdef MP_TEST
		if ((OPMODE & WIFI_MP_STATE) || priv->pshare->rf_ft_var.mp_specific) {
			if(priv->pshare->mp_txpwr_tracking == FALSE)
				return;
		}
#endif
#if 0
	if(priv->pshare->Power_tracking_on_88E == 0)
	{
		priv->pshare->Power_tracking_on_88E = 1;
		phy_set_rf_reg(priv, RF_PATH_A, 0x42, (BIT(17) | BIT(16)), 0x03);
		return; 
	}
	else
#endif
	{

//	priv->pshare->Power_tracking_on_88E = 0;
	
		//printk("===>dm_TXPowerTrackingCallback_ThermalMeter_8188E txpowercontrol %d\n",  priv->pshare->TxPowerTrackControl);

		ThermalValue = (unsigned char)phy_query_rf_reg(priv, RF_PATH_A, RF_T_METER_88E, 0xfc00, 1);	//0x42: RF Reg[15:10] 88E

		//printk("\nReadback Thermal Meter = 0x%x pre thermal meter 0x%x EEPROMthermalmeter 0x%x\n", ThermalValue, priv->pshare->thermal_value, priv->pmib->dot11RFEntry.ther);

	}

	
	if(is2T)
		rf = 2;
	else
		rf = 1;
	
	//if(ThermalValue)
	{
//		if(!pHalData->thermal_value)
		{
			//Query OFDM path A default setting 		
			ele_D = phy_query_bb_reg(priv, REG_OFDM_0_XA_TX_IQ_IMBALANCE, bMaskDWord)&bMaskOFDM_D;
	
			for(i=0; i<OFDM_TABLE_SIZE_92D; i++)	//find the index
			{
				if(ele_D == (ofdm_swing_table[i]&bMaskOFDM_D))
				{
					OFDM_index_old[0] = (unsigned char)i;
					printk("Initial pathA ele_D reg0x%x = 0x%x, OFDM_index=0x%x\n", 
						REG_OFDM_0_XA_TX_IQ_IMBALANCE, ele_D, OFDM_index_old[0]);
					break;
				}
			}

			//Query OFDM path B default setting 
			if(is2T)
			{
				ele_D = phy_query_bb_reg(priv, REG_OFDM_0_XB_TX_IQ_IMBALANCE, bMaskDWord)&bMaskOFDM_D;
				for(i=0; i<OFDM_TABLE_SIZE_92D; i++)	//find the index
				{
					if(ele_D == (ofdm_swing_table[i]&bMaskOFDM_D))
					{
						OFDM_index_old[1] = (unsigned char)i;
						printk("Initial pathB ele_D reg0x%x = 0x%x, OFDM_index=0x%x\n", 
							REG_OFDM_0_XB_TX_IQ_IMBALANCE, ele_D, OFDM_index_old[1]);
						break;
					}
				}
			}
			
			{
				//Query CCK default setting From 0xa24
#if 0
				TempCCk = priv->pshare->RegA24;

				for(i=0 ; i<CCK_TABLE_SIZE ; i++)
				{
					if(priv->pshare->bCCKinCH14)
					{
						if(memcmp((void*)&TempCCk, (void*)&cck_swing_table_ch14[i][2], 4)==0)
						{
							CCK_index_old =(unsigned char) i;
							//printk("Initial reg0x%x = 0x%x, CCK_index=0x%x, ch 14 %d\n", 
								//rCCK0_TxFilter2, TempCCk, CCK_index_old, priv->pshare->bCCKinCH14);
							break;
						}
					}
					else
					{
                        //printk("RegA24: 0x%X, cck_swing_table_ch1_ch13[%d][2]: cck_swing_table_ch1_ch13[i][2]: 0x%X\n", TempCCk, i, cck_swing_table_ch1_ch13[i][2]);
						if(memcmp((void*)&TempCCk, (void*)&cck_swing_table_ch1_ch13[i][2], 4)==0)
						{
							CCK_index_old =(unsigned char) i;
							//printk("Initial reg0x%x = 0x%x, CCK_index=0x%x, ch14 %d\n", 
								//rCCK0_TxFilter2, TempCCk, CCK_index_old, priv->pshare->bCCKinCH14);
							break;
						}			
					}
				}
#endif
			}

			if(!priv->pshare->thermal_value)
			{
				CCK_index_old = get_CCK_swing_index(priv);
				priv->pshare->thermal_value = priv->pmib->dot11RFEntry.ther;
				priv->pshare->thermal_value_lck = ThermalValue;				
				priv->pshare->thermal_value_iqk = ThermalValue;								
				
				for(i = 0; i < rf; i++)
					priv->pshare->OFDM_index[i] = OFDM_index_old[i];
				priv->pshare->CCK_index = CCK_index_old;
			}			

			if(priv->pshare->bReloadtxpowerindex)
			{
				printk("reload ofdm index for band switch\n");				
			}

			//calculate average thermal meter
			{
				priv->pshare->thermal_value_avg[priv->pshare->thermal_value_avg_index] = ThermalValue;
				priv->pshare->thermal_value_avg_index++;
				if(priv->pshare->thermal_value_avg_index == AVG_THERMAL_NUM_88E)
					priv->pshare->thermal_value_avg_index = 0;

				for(i = 0; i < AVG_THERMAL_NUM_88E; i++)
				{
					if(priv->pshare->thermal_value_avg[i])
					{
						ThermalValue_AVG += priv->pshare->thermal_value_avg[i];
						ThermalValue_AVG_count++;
					}
				}

				if(ThermalValue_AVG_count)
				{
					ThermalValue = (unsigned char)(ThermalValue_AVG / ThermalValue_AVG_count);
					//printk("AVG Thermal Meter = 0x%x \n", ThermalValue);					
				}
			}			
		}

		if(priv->pshare->bReloadtxpowerindex)
		{
			delta = ThermalValue > priv->pmib->dot11RFEntry.ther?(ThermalValue - priv->pmib->dot11RFEntry.ther):(priv->pmib->dot11RFEntry.ther - ThermalValue);				
			priv->pshare->bReloadtxpowerindex = FALSE;	
			priv->pshare->bDoneTxpower = FALSE;
		}
		else if(priv->pshare->bDoneTxpower)
		{
			delta = (ThermalValue > priv->pshare->thermal_value)?(ThermalValue - priv->pshare->thermal_value):(priv->pshare->thermal_value - ThermalValue);
		}
		else
		{
			delta = ThermalValue > priv->pmib->dot11RFEntry.ther?(ThermalValue - priv->pmib->dot11RFEntry.ther):(priv->pmib->dot11RFEntry.ther - ThermalValue);		
		}
		delta_LCK = (ThermalValue > priv->pshare->thermal_value_lck)?(ThermalValue - priv->pshare->thermal_value_lck):(priv->pshare->thermal_value_lck - ThermalValue);
		delta_IQK = (ThermalValue > priv->pshare->thermal_value_iqk)?(ThermalValue - priv->pshare->thermal_value_iqk):(priv->pshare->thermal_value_iqk - ThermalValue);

		//printk("Readback Thermal Meter = 0x%x \npre thermal meter 0x%x EEPROMthermalmeter 0x%x delta 0x%x \ndelta_LCK 0x%x delta_IQK 0x%x \n",   ThermalValue, priv->pshare->thermal_value, priv->pshare->EEPROMThermalMeter, delta, delta_LCK, delta_IQK);
		//printk("pre thermal meter LCK 0x%x \npre thermal meter IQK 0x%x \ndelta_LCK_bound 0x%x delta_IQK_bound 0x%x\n",   priv->pshare->thermal_value_lck, priv->pshare->thermal_value_iqk, priv->pshare->delta_lck, priv->pshare->delta_iqk);


		//if((delta_LCK > pHalData->delta_lck) && (pHalData->delta_lck != 0))
        if (delta_LCK >= 8) // Delta temperature is equal to or larger than 20 centigrade.
		{
            priv->pshare->thermal_value_lck = ThermalValue;
			phy_lc_calibrate(priv);
		}

		
		if(delta > 0 && priv->pshare->TxPowerTrackControl)
		{
			delta = ThermalValue > priv->pmib->dot11RFEntry.ther?(ThermalValue - priv->pmib->dot11RFEntry.ther):(priv->pmib->dot11RFEntry.ther - ThermalValue);		

			//calculate new OFDM / CCK offset	
			{
				{							
					if(ThermalValue > priv->pmib->dot11RFEntry.ther)
						j = 1;
					else
						j = 0;

					for(offset = 0; offset < index_mapping_NUM_88E; offset++)
					{
						if(delta < Thermal_mapping[j][offset])
						{
							if(offset != 0)
								offset--;
							break;
						}
					}			
					if(offset >= index_mapping_NUM_88E)
						offset = index_mapping_NUM_88E-1;
					
					index = OFDM_index_mapping[j][offset];	

					printk("\nj = %d delta = %d, index = %d\n\n", j, delta, index);
					
					for(i = 0; i < rf; i++) 		
						OFDM_index[i] = priv->pshare->OFDM_index[i] + OFDM_index_mapping[j][offset];
						CCK_index = priv->pshare->CCK_index + OFDM_index_mapping[j][offset];					
				}				
				
				if(is2T)
				{
					printk("temp OFDM_A_index=0x%x, OFDM_B_index=0x%x, CCK_index=0x%x\n", 
						priv->pshare->OFDM_index[0], priv->pshare->OFDM_index[1], priv->pshare->CCK_index);			
				}
				else
				{
					printk("temp OFDM_A_index=0x%x, CCK_index=0x%x\n", 
						priv->pshare->OFDM_index[0], priv->pshare->CCK_index); 		
				}
				
				for(i = 0; i < rf; i++)
				{
					if(OFDM_index[i] > OFDM_TABLE_SIZE_92D-1)
					{
						OFDM_index[i] = OFDM_TABLE_SIZE_92D-1;
					}
					else if (OFDM_index[i] < OFDM_min_index)
					{
						OFDM_index[i] = OFDM_min_index;
					}
				}

				{
					if(CCK_index > CCK_TABLE_SIZE-1)
						CCK_index = CCK_TABLE_SIZE-1;
					else if (CCK_index < 0)
						CCK_index = 0;
				}

				if(is2T)
				{
					printk("new OFDM_A_index=0x%x, OFDM_B_index=0x%x, CCK_index=0x%x\n", 
						OFDM_index[0], OFDM_index[1], CCK_index);
				}
				else
				{
					printk("new OFDM_A_index=0x%x, CCK_index=0x%x\n", 
						OFDM_index[0], CCK_index); 
				}
			}

			//2 temporarily remove bNOPG
			//Config by SwingTable
			if(priv->pshare->TxPowerTrackControl /*&& !pHalData->bNOPG*/)			
			{
				priv->pshare->bDoneTxpower = TRUE;			

				//Adujst OFDM Ant_A according to IQK result
				ele_D = (ofdm_swing_table[(unsigned char)OFDM_index[0]] & 0xFFC00000)>>22;		
				X = priv->pshare->IQKMatrixRegSetting[Indexforchannel].Value[0][0];
				Y = priv->pshare->IQKMatrixRegSetting[Indexforchannel].Value[0][1];

				if(X != 0)
				{
					if ((X & 0x00000200) != 0)
						X = X | 0xFFFFFC00;
					ele_A = ((X * ele_D)>>8)&0x000003FF;
						
					//new element C = element D x Y
					if ((Y & 0x00000200) != 0)
						Y = Y | 0xFFFFFC00;
					ele_C = ((Y * ele_D)>>8)&0x000003FF;
					
					//wirte new elements A, C, D to regC80 and regC94, element B is always 0
					value32 = (ele_D<<22)|((ele_C&0x3F)<<16)|ele_A;
					phy_set_bb_reg(priv, REG_OFDM_0_XA_TX_IQ_IMBALANCE, bMaskDWord, value32);

					value32 = (ele_C&0x000003C0)>>6;
					phy_set_bb_reg(priv, REG_OFDM_0_XC_TX_AFE, bMaskH4Bits, value32);

					value32 = ((X * ele_D)>>7)&0x01;
					phy_set_bb_reg(priv, REG_OFDM_0_ECCA_THRESHOLD, BIT(24), value32);
					
				}
				else
				{
					phy_set_bb_reg(priv, REG_OFDM_0_XA_TX_IQ_IMBALANCE, bMaskDWord, ofdm_swing_table[(unsigned char)OFDM_index[0]]);				
					phy_set_bb_reg(priv, REG_OFDM_0_XC_TX_AFE, bMaskH4Bits, 0x00);
					phy_set_bb_reg(priv, REG_OFDM_0_ECCA_THRESHOLD, BIT(24), 0x00);			
				}

				//printk("TxPwrTracking for path A: X = 0x%x, Y = 0x%x ele_A = 0x%x ele_C = 0x%x ele_D = 0x%x 0xe94 = 0x%x 0xe9c = 0x%x\n", 
					//(unsigned int)X, (unsigned int)Y, (unsigned int)ele_A, (unsigned int)ele_C, (unsigned int)ele_D, (unsigned int)X, (unsigned int)Y); 	
				
				{
					//Adjust CCK according to IQK result
					if(!priv->pshare->bCCKinCH14){
						RTL_W8(0xa22, cck_swing_table_ch1_ch13[(unsigned char)CCK_index][0]);
						RTL_W8(0xa23, cck_swing_table_ch1_ch13[(unsigned char)CCK_index][1]);
						RTL_W8(0xa24, cck_swing_table_ch1_ch13[(unsigned char)CCK_index][2]);
						RTL_W8(0xa25, cck_swing_table_ch1_ch13[(unsigned char)CCK_index][3]);
						RTL_W8(0xa26, cck_swing_table_ch1_ch13[(unsigned char)CCK_index][4]);
						RTL_W8(0xa27, cck_swing_table_ch1_ch13[(unsigned char)CCK_index][5]);
						RTL_W8(0xa28, cck_swing_table_ch1_ch13[(unsigned char)CCK_index][6]);
						RTL_W8(0xa29, cck_swing_table_ch1_ch13[(unsigned char)CCK_index][7]);		
					}
					else{
						RTL_W8(0xa22, cck_swing_table_ch14[(unsigned char)CCK_index][0]);
						RTL_W8(0xa23, cck_swing_table_ch14[(unsigned char)CCK_index][1]);
						RTL_W8(0xa24, cck_swing_table_ch14[(unsigned char)CCK_index][2]);
						RTL_W8(0xa25, cck_swing_table_ch14[(unsigned char)CCK_index][3]);
						RTL_W8(0xa26, cck_swing_table_ch14[(unsigned char)CCK_index][4]);
						RTL_W8(0xa27, cck_swing_table_ch14[(unsigned char)CCK_index][5]);
						RTL_W8(0xa28, cck_swing_table_ch14[(unsigned char)CCK_index][6]);
						RTL_W8(0xa29, cck_swing_table_ch14[(unsigned char)CCK_index][7]);	
					}		
				}
				
				if(is2T)
				{						
					ele_D = (ofdm_swing_table[(unsigned char)OFDM_index[1]] & 0xFFC00000)>>22;
					
					//new element A = element D x X
					X = priv->pshare->IQKMatrixRegSetting[Indexforchannel].Value[0][4];
					Y = priv->pshare->IQKMatrixRegSetting[Indexforchannel].Value[0][5];
					
					//if(X != 0 && pHalData->CurrentBandType92D == ODM_BAND_ON_2_4G)
					if((X != 0) && (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_2G))
						
					{
						if ((X & 0x00000200) != 0)	//consider minus
							X = X | 0xFFFFFC00;
						ele_A = ((X * ele_D)>>8)&0x000003FF;
						
						//new element C = element D x Y
						if ((Y & 0x00000200) != 0)
							Y = Y | 0xFFFFFC00;
						ele_C = ((Y * ele_D)>>8)&0x00003FF;
						
						//wirte new elements A, C, D to regC88 and regC9C, element B is always 0
						value32=(ele_D<<22)|((ele_C&0x3F)<<16) |ele_A;
						phy_set_bb_reg(priv, REG_OFDM_0_XB_TX_IQ_IMBALANCE, bMaskDWord, value32);

						value32 = (ele_C&0x000003C0)>>6;
						phy_set_bb_reg(priv, REG_OFDM_0_XD_TX_AFE, bMaskH4Bits, value32);	
						
						value32 = ((X * ele_D)>>7)&0x01;
						phy_set_bb_reg(priv, REG_OFDM_0_ECCA_THRESHOLD, BIT(28), value32);

					}
					else
					{
						phy_set_bb_reg(priv, REG_OFDM_0_XB_TX_IQ_IMBALANCE, bMaskDWord, ofdm_swing_table[(unsigned char)OFDM_index[1]]);										
						phy_set_bb_reg(priv, REG_OFDM_0_XD_TX_AFE, bMaskH4Bits, 0x00);	
						phy_set_bb_reg(priv, REG_OFDM_0_ECCA_THRESHOLD, BIT(28), 0x00);				
					}

					//printk("TxPwrTracking path B: X = 0x%x, Y = 0x%x ele_A = 0x%x ele_C = 0x%x ele_D = 0x%x 0xeb4 = 0x%x 0xebc = 0x%x\n", 
						//(unsigned int)X, (unsigned int)Y, (unsigned int)ele_A, (unsigned int)ele_C, (unsigned int)ele_D, (unsigned int)X, (unsigned int)Y);			
				}
				
				printk("TxPwrTracking 0xc80 = 0x%x, 0xc94 = 0x%x RF 0x24 = 0x%x\n\n", phy_query_bb_reg(priv, 0xc80, bMaskDWord), phy_query_bb_reg(priv, 0xc94, bMaskDWord), phy_query_rf_reg(priv, RF_PATH_A, 0x24, bRFRegOffsetMask, 1));
			}			
		}
		
#if 0 //DO NOT do IQK during 88E power tracking	
		// if((delta_IQK > pHalData->delta_iqk) && (pHalData->delta_iqk != 0))
        if (delta_IQK >= 8) // Delta temperature is equal to or larger than 20 centigrade.
		{
			odm_reset_iqk_result(priv);		

/*
#if(DM_ODM_SUPPORT_TYPE  & ODM_WIN)
#if (DEV_BUS_TYPE == RT_PCI_INTERFACE)	
			if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
#if USE_WORKITEM
				platform_acquire_mutex(&pHalData->mxChnlBwControl);
#else
				platform_acquire_spin_lock(Adapter, RT_CHANNEL_AND_BANDWIDTH_SPINLOCK);
#endif
#endif
			}
#if((DEV_BUS_TYPE == RT_USB_INTERFACE) || (DEV_BUS_TYPE == RT_SDIO_INTERFACE))
			if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
				platform_acquire_mutex(&pHalData->mxChnlBwControl);
			}
#endif
#endif			
*/
			priv->pshare->thermal_value_iqk= ThermalValue;
			phy_iq_calibrate_8188e(priv, FALSE);

/*
#if(DM_ODM_SUPPORT_TYPE  & ODM_WIN)
#if (DEV_BUS_TYPE == RT_PCI_INTERFACE)	
			if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
#if USE_WORKITEM
				platform_release_mutex(&pHalData->mxChnlBwControl);
#else
				platform_release_spin_lock(Adapter, RT_CHANNEL_AND_BANDWIDTH_SPINLOCK);
#endif
#endif
			}
#if((DEV_BUS_TYPE == RT_USB_INTERFACE) || (DEV_BUS_TYPE == RT_SDIO_INTERFACE))
			if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
				platform_release_mutex(&pHalData->mxChnlBwControl);
			}
#endif
#endif
*/
		}
#endif
		//update thermal meter value
		if(priv->pshare->TxPowerTrackControl)
		{
			//Adapter->HalFunc.SetHalDefVarHandler(Adapter, HAL_DEF_THERMAL_VALUE, &ThermalValue);
			priv->pshare->thermal_value = ThermalValue;
		}
			
	}

	//printk("<===dm_TXPowerTrackingCallback_ThermalMeter_8188E\n");
	
	priv->pshare->TXPowercount = 0;

}


#endif // CALIBRATE_BY_ODM


#endif // CONFIG_RTL_88E_SUPPORT

