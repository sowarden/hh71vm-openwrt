/*
 *  TX handle routines
 *
 *  Copyright (c) 2017 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _8192CD_TX_C_

#ifdef __KERNEL__
#include <linux/if_arp.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/tcp.h>
#elif defined(__ECOS)
#include <cyg/io/eth/rltk/819x/wrapper/sys_support.h>
#include <cyg/io/eth/rltk/819x/wrapper/skbuff.h>
#include <cyg/io/eth/rltk/819x/wrapper/timer.h>
#include <cyg/io/eth/rltk/819x/wrapper/wrapper.h>
#endif

#ifdef __DRAYTEK_OS__
#include <draytek/wl_dev.h>
#endif

#include "./8192cd_cfg.h"
#include "./8192cd.h"
#include "./8192cd_hw.h"
#include "./8192cd_headers.h"
#include "./8192cd_debug.h"
#ifndef __OSK__
#include "./osdep_service.h"
#endif
#include "WlanHAL/HalPrecomp.h"

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

#ifdef PERF_DUMP_1074K
#include "m1074kctrl.c"
#endif

#ifdef __OSK__
#include "ifport.h"
#endif

#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
#include "./8192cd_util.h"
#endif /* defined(CONFIG_RTL_TRIBAND_SUPPORT) */

#include "./core/8192cd_core_tx.h"

struct rtl_arphdr
{
	//for corss platform
    __be16          ar_hrd;         /* format of hardware address   */
    __be16          ar_pro;         /* format of protocol address   */
    unsigned char   ar_hln;         /* length of hardware address   */
    unsigned char   ar_pln;         /* length of protocol address   */
    __be16          ar_op;          /* ARP opcode (command)         */
};

#define AMSDU_TX_DESC_TH		2	// A-MSDU tx desc threshold, A-MSDU will be
									// triggered when more than this threshold packet in hw queue

#define RET_AGGRE_BYPASS		0
#define RET_AGGRE_ENQUE			1
#define RET_AGGRE_DESC_FULL		2

#ifdef SW_TX_QUEUE
#define SWQ_DEQ_ALL			1
#define SWQ_DEQ_LIMIT		0
#endif



#if 0/*def CONFIG_RTL_STP*/
extern unsigned char STPmac[6];
#endif

#ifdef CONFIG_RTL_VLAN_8021Q
extern int linux_vlan_enable;
extern linux_vlan_ctl_t *vlan_ctl_p;
#endif



#if defined(CONFIG_RTL8672) && defined(__OSK__)
extern int enable_port_mapping;
extern int enable_IGMP_SNP;
extern int check_IGMP_report(unsigned char *datap);
extern int check_wlan_mcast_tx(struct sk_buff *skb, int port);
#ifdef INET6
extern int enable_MLD_SNP;
extern int check_mld_report(unsigned char *datap);
extern int check_wlan_ipv6_mcast_tx(struct sk_buff *skb,int port);
#endif

#ifdef CONFIG_RTL_92D_DMDP
extern struct port_map wlanDev[(RTL8192CD_NUM_VWLAN+2)*2];
#else
extern struct port_map wlanDev[RTL8192CD_NUM_VWLAN+2];		// Root(1)+vxd(1)+VAPs(4)
#endif
#elif !defined(CONFIG_RTL_8676HWNAT) && defined(CONFIG_RTL8672) && !defined(CONFIG_RTL8686) && !defined(CONFIG_RTL8685) && !defined(CONFIG_RTL8685S) && !defined(CONFIG_RTL8685SB) && !defined(CONFIG_RTL9607C)
extern int check_IGMP_report(struct sk_buff *skb);
extern int check_wlan_mcast_tx(struct sk_buff *skb);
#endif



static int tkip_mic_padding(struct rtl8192cd_priv *priv,
			unsigned char *da, unsigned char *sa, unsigned char priority,
			unsigned char *llc,struct sk_buff *pskb, struct tx_insn* txcfg);

#ifdef CONFIG_PCI_HCI
//static int rtl8192cd_tx_queueDsr(struct rtl8192cd_priv *priv, unsigned int txRingIdx);
static void rtl8192cd_tx_restartQueue(struct rtl8192cd_priv *priv);
#endif

#ifdef TX_SCATTER
extern struct sk_buff *copy_skb(struct sk_buff *skb);
#endif






unsigned int get_lowest_tx_rate(struct rtl8192cd_priv *priv, struct stat_info *pstat,
				unsigned int tx_rate)
{
	unsigned int lowest_tx_rate;

	if (is_auto_rate(priv , pstat))
	{
			lowest_tx_rate = find_rate(priv, pstat, 0, 0);
	}
	else
		lowest_tx_rate = tx_rate;

	return lowest_tx_rate;
}






#ifdef CONFIG_PCI_HCI
#ifdef AP_SWPS_OFFLOAD
static int SWPS_dz_queue_insert(struct rtl8192cd_priv *priv, struct tx_desc_info *pdescinfo, void *pskb, int* insert_position
)
{
	unsigned int ret;
    //struct sk_buff* dzpskb_old;
    //pskb = (struct sk_buff *)(pdescinfo->pframe);
    int dz_position;
    int pkt_Qos;
    pkt_Qos = pdescinfo->SWPS_pkt_Qos;
    
#if defined(WIFI_WMM) && defined(WMM_APSD)
		if ((QOS_ENABLE) && (APSD_ENABLE) && (pdescinfo->swps_pstat->QosEnabled) && (pdescinfo->swps_pstat->apsd_bitmap & 0x0f)) {
			
            if(pkt_Qos < 0){
                printk("Error Qos when reprepare insert to dzqueue.\n");
                return FALSE;
            }
            
			if (pkt_Qos == 3) {
                
				ret = enque_before_Queuehead(priv, pdescinfo, pdescinfo->swps_pstat->VO_dz_queue, pdescinfo->swps_pstat->SWPS_last_seq[pkt_Qos],
					NUM_APSD_TXPKT_QUEUE, (void *)pskb, &pdescinfo->swps_pstat->prepare_done[pkt_Qos], insert_position);
				if (ret)
					DEBUG_INFO("reprepare enque VO pkt to dzqueue\n");
			}
			else if (pkt_Qos == 2) {
				ret = enque_before_Queuehead(priv, pdescinfo, pdescinfo->swps_pstat->VI_dz_queue, pdescinfo->swps_pstat->SWPS_last_seq[pkt_Qos],
					NUM_APSD_TXPKT_QUEUE, (void *)pskb, &pdescinfo->swps_pstat->prepare_done[pkt_Qos],insert_position);
				if (ret)
					DEBUG_INFO("reprepare enque VI pkt to dzqueue\n");
			}
			else if (pkt_Qos == 1) {
				ret = enque_before_Queuehead(priv, pdescinfo, pdescinfo->swps_pstat->BE_dz_queue, pdescinfo->swps_pstat->SWPS_last_seq[pkt_Qos],
					NUM_APSD_TXPKT_QUEUE, (void *)pskb, &pdescinfo->swps_pstat->prepare_done[pkt_Qos],insert_position);
				if (ret)
					DEBUG_INFO("reprepare enque BE pkt to dzqueue\n");
			}
			else if (pkt_Qos == 0) {
				ret = enque_before_Queuehead(priv, pdescinfo, pdescinfo->swps_pstat->BK_dz_queue, pdescinfo->swps_pstat->SWPS_last_seq[pkt_Qos],
					NUM_APSD_TXPKT_QUEUE, (void *)pskb, &pdescinfo->swps_pstat->prepare_done[pkt_Qos],insert_position);
				if (ret)
					DEBUG_INFO("reprepare enque BK pkt to dzqueue\n");
			}
			else
				goto legacy_ps;

			if (!pdescinfo->swps_pstat->apsd_pkt_buffering)
				pdescinfo->swps_pstat->apsd_pkt_buffering = 1;

			if (ret == FALSE) {
				printk("sleep Q full for Qos = %d!\n",pkt_Qos);
				return CONGESTED;
			}
			return TRUE;
		}
		else
legacy_ps:
#endif
        if (pdescinfo->swps_pstat->dz_queue.qlen<NUM_TXPKT_QUEUE){
            
            dz_position = find_dzposition(&pdescinfo->swps_pstat->dz_list_head, pdescinfo);
            *insert_position = dz_position;
            if(dz_position == -1)
                printk("find_dzposition failed.\n");
            else{
                insert_dzposition(pdescinfo,pdescinfo->swps_pstat,dz_position,pdescinfo->swps_pstat->SWPS_last_seq[pkt_Qos],&pdescinfo->swps_pstat->prepare_done[pkt_Qos],(struct sk_buff *)pskb); //BE
			    //skb_queue_tail(&pdescinfo->pstat->dz_queue, (struct sk_buff *)pdescinfo->pframe);
			    return TRUE;
            }
            return CONGESTED;
		}
	
	return FALSE;
}
static int SWPS_reprepare_pktinfo_enqueue(struct rtl8192cd_priv *priv, struct tx_desc_info *pdescinfo, int pkt_Qos, int insert_position, u1Byte AMSDU_flag)
{
    struct apsd_pkt_queue* AC_dz_queue;
    int amsdu_i = 0;
        
#if defined(WIFI_WMM) && defined(WMM_APSD)
    if ((QOS_ENABLE) && (APSD_ENABLE) && (pdescinfo->swps_pstat->QosEnabled) && (pdescinfo->swps_pstat->apsd_bitmap & 0x0f)) {
        
        if(pkt_Qos < 0){
            printk("Error Qos when reprepare pktinfo enqueue.\n");
            return FALSE;
        }
        if(insert_position == -1)
            return FALSE;
        
        if (pkt_Qos == 3) {
            AC_dz_queue = pdescinfo->swps_pstat->VO_dz_queue;
        }else if (pkt_Qos == 2) {
            AC_dz_queue = pdescinfo->swps_pstat->VI_dz_queue;
        }else if (pkt_Qos == 1) {
            AC_dz_queue = pdescinfo->swps_pstat->BE_dz_queue;
        }else if (pkt_Qos == 0) {
            AC_dz_queue = pdescinfo->swps_pstat->BK_dz_queue;
        }else
    	    goto legacy_ps;

        switch(pdescinfo->type){
            case _PRE_ALLOCHDR_:
                AC_dz_queue->hdr[insert_position] = pdescinfo->pframe;
                AC_dz_queue->hdr_len[insert_position] = pdescinfo->hdr_len;
            break;
            case  _PRE_ALLOCLLCHDR_:
                AC_dz_queue->llchdr[insert_position] = pdescinfo->pframe;
                AC_dz_queue->hdr_len[insert_position] = pdescinfo->hdr_len;
            break;          
            case _PRE_ALLOCICVHDR_:
                AC_dz_queue->icv[insert_position] = pdescinfo->pframe;
                AC_dz_queue->mic_icv_len[insert_position] = pdescinfo->mic_icv_len;
                break;
            case _PRE_ALLOCMICHDR_:
                AC_dz_queue->mic[insert_position] = pdescinfo->pframe;
                AC_dz_queue->mic_icv_len[insert_position] = pdescinfo->mic_icv_len;
                break;
            case _SKB_FRAME_TYPE_: //header conversion case
                AC_dz_queue->hdr_len[insert_position] = pdescinfo->hdr_len;
                break;
            default:
                break;
        }
        int cnt;
        for (cnt =0; cnt<TXBD_ELE_NUM-2; cnt++) {
            switch(pdescinfo->buf_type[cnt]){
                case _PRE_ALLOCHDR_:
                    AC_dz_queue->hdr[insert_position] = pdescinfo->buf_pframe[cnt];
                    AC_dz_queue->hdr_len[insert_position] = pdescinfo->hdr_len;
                break;
                case  _PRE_ALLOCLLCHDR_:
                    AC_dz_queue->llchdr[insert_position] = pdescinfo->buf_pframe[cnt];
                    AC_dz_queue->hdr_len[insert_position] = pdescinfo->hdr_len;
                break;          
                case _PRE_ALLOCICVHDR_:
                    AC_dz_queue->icv[insert_position] = pdescinfo->buf_pframe[cnt];
                    AC_dz_queue->mic_icv_len[insert_position] = pdescinfo->mic_icv_len;
                    break;
                case _PRE_ALLOCMICHDR_:
                    AC_dz_queue->mic[insert_position] = pdescinfo->buf_pframe[cnt];
                    AC_dz_queue->mic_icv_len[insert_position] = pdescinfo->mic_icv_len;
                    break;
                default:
                    break;
            }
        }
        AC_dz_queue->amsdu_flag[insert_position] = AMSDU_flag;
        AC_dz_queue->skbbuf_len[insert_position] = pdescinfo->skbbuf_len;
        AC_dz_queue->PSB_len[insert_position] = pdescinfo->PSB_len;
        AC_dz_queue->ptxdesc[insert_position] = pdescinfo->ptxdesc;
        AC_dz_queue->isWiFiHdr[insert_position] = pdescinfo->isWiFiHdr;
#if CFG_HAL_SUPPORT_TXDESC_IE
        AC_dz_queue->IE_bitmap[insert_position] = pdescinfo->IE_bitmap;
#endif
        if(AC_dz_queue->amsdu_flag[insert_position]){
#ifdef WLAN_HAL_TX_AMSDU            
            AC_dz_queue->amsdu_num[insert_position] = pdescinfo->amsdu_num;
            for(amsdu_i = 0;amsdu_i<AC_dz_queue->amsdu_num[insert_position];amsdu_i++)
                AC_dz_queue->amsdubuf_len[insert_position][amsdu_i] = pdescinfo->amsdubuf_len[amsdu_i];
#endif            
        }

        //reset to 0
        pdescinfo->hdr_len = 0;
        pdescinfo->mic_icv_len = 0;
        pdescinfo->skbbuf_len = 0;
        pdescinfo->PSB_len = 0;
        pdescinfo->ptxdesc = NULL;
        pdescinfo->isWiFiHdr = 0;
        pdescinfo->SWPS_sequence = 0;
#if CFG_HAL_SUPPORT_TXDESC_IE
        pdescinfo->IE_bitmap = 0;
#endif
#ifdef WLAN_HAL_TX_AMSDU            
        pdescinfo->amsdu_num = 0;
        for(amsdu_i = 0;amsdu_i<WLAN_HAL_TX_AMSDU_MAX_NUM;amsdu_i++)
            pdescinfo->amsdubuf_len[amsdu_i] = 0;
#endif                
        return TRUE;
        
    }else
legacy_ps:
#endif
    if (pdescinfo->swps_pstat->dz_queue.qlen<NUM_TXPKT_QUEUE){
        struct list_head *iterator;
        struct list_head* dzhead = &pdescinfo->swps_pstat->dz_list_head;
        int search_count = 0;
        struct dz_seq_node* insert_node= NULL;

        list_for_each(iterator, dzhead){
            
            if(search_count == insert_position){
                insert_node = list_entry(iterator, struct dz_seq_node, node);
                break;
            }
            search_count++;
        }

        if(insert_node != NULL){
            
            switch(pdescinfo->type){
                case _PRE_ALLOCHDR_:
                    insert_node->hdr = pdescinfo->pframe;
                    insert_node->hdr_len = pdescinfo->hdr_len;
                    insert_node->llchdr =NULL;
                    insert_node->icv = NULL;
                    insert_node->mic = NULL;
                    insert_node->mic_icv_len = 0;
                break;
                case  _PRE_ALLOCLLCHDR_:
                    insert_node->llchdr = pdescinfo->pframe;
                    insert_node->hdr_len = pdescinfo->hdr_len;
                    insert_node->hdr = NULL;
                    insert_node->icv = NULL;
                    insert_node->mic = NULL;
                    insert_node->mic_icv_len = 0;
                break;          
                case _PRE_ALLOCICVHDR_:
                    insert_node->icv = pdescinfo->pframe;
                    insert_node->mic_icv_len = pdescinfo->mic_icv_len;
                    insert_node->hdr = NULL;
                    insert_node->llchdr =NULL;
                    insert_node->mic =NULL;
                    insert_node->hdr_len = 0;
                    break;
                case _PRE_ALLOCMICHDR_:
                    insert_node->mic = pdescinfo->pframe;
                    insert_node->mic_icv_len = pdescinfo->mic_icv_len;
                    insert_node->hdr = NULL;
                    insert_node->llchdr = NULL;
                    insert_node->icv = NULL;
                    insert_node->hdr_len = 0;
                    break;
                case _SKB_FRAME_TYPE_: //header conversion case
                    insert_node->hdr_len = pdescinfo->hdr_len;
                    //need check which parar need to rst to 0
                    break;
                default:
                    break;
            }
            int cnt;
            for (cnt =0; cnt<TXBD_ELE_NUM-2; cnt++) {
                
                switch(pdescinfo->buf_type[cnt]){
                    case _PRE_ALLOCHDR_:
                        insert_node->hdr = pdescinfo->buf_pframe[cnt];
                        insert_node->hdr_len = pdescinfo->hdr_len;
                    break;
                    case  _PRE_ALLOCLLCHDR_:
                        insert_node->llchdr = pdescinfo->buf_pframe[cnt];
                        insert_node->hdr_len = pdescinfo->hdr_len;
                    break;          
                    case _PRE_ALLOCICVHDR_:
                        insert_node->icv = pdescinfo->buf_pframe[cnt];
                        insert_node->mic_icv_len = pdescinfo->mic_icv_len;
                        break;
                    case _PRE_ALLOCMICHDR_:
                        insert_node->mic = pdescinfo->buf_pframe[cnt];
                        insert_node->mic_icv_len = pdescinfo->mic_icv_len;
                        break;
                    default:
                        break;
                }
            }
            insert_node->queueIndex = pkt_Qos+1;
            if((pkt_Qos > 3) || (pkt_Qos < 0))
                printk("insert reprepare pkt, compute queue index error.\n");
            insert_node->amsdu_flag = AMSDU_flag;
            insert_node->skbbuf_len = pdescinfo->skbbuf_len;
            insert_node->PSB_len = pdescinfo->PSB_len;
            insert_node->ptxdesc = pdescinfo->ptxdesc;
            insert_node->isWiFiHdr = pdescinfo->isWiFiHdr;
#if CFG_HAL_SUPPORT_TXDESC_IE
            insert_node->IE_bitmap = pdescinfo->IE_bitmap;
#endif            
            if(insert_node->amsdu_flag){
#ifdef WLAN_HAL_TX_AMSDU                
                insert_node->amsdu_num = pdescinfo->amsdu_num;
                for(amsdu_i = 0;amsdu_i<insert_node->amsdu_num;amsdu_i++)
                    insert_node->amsdubuf_len[amsdu_i] = pdescinfo->amsdubuf_len[amsdu_i];
#endif                
            }

            //reset to 0
            pdescinfo->hdr_len = 0;
            pdescinfo->mic_icv_len = 0;
            pdescinfo->skbbuf_len = 0;
            pdescinfo->PSB_len = 0;
            pdescinfo->ptxdesc = NULL;
            pdescinfo->isWiFiHdr = 0;
            pdescinfo->SWPS_sequence = 0;
            pdescinfo->pframe = NULL;
#if CFG_HAL_SUPPORT_TXDESC_IE
            pdescinfo->IE_bitmap = 0;
#endif            
#ifdef WLAN_HAL_TX_AMSDU            
            pdescinfo->amsdu_num = 0;
            for(amsdu_i = 0;amsdu_i<WLAN_HAL_TX_AMSDU_MAX_NUM;amsdu_i++)
                pdescinfo->amsdubuf_len[amsdu_i] = 0;
#endif                
            return TRUE;
        }else
            printk("[%s][%s] insert node = NULL!!!\n",__FUNCTION__,__LINE__); //debug
            
		
	}

    return FALSE;
}
#endif


#endif // CONFIG_PCI_HCI


/*        Function to process different situations in TX flow             */
/* ====================================================================== */
#define TX_PROCEDURE_CTRL_STOP			0
#define TX_PROCEDURE_CTRL_CONTINUE		1
#define TX_PROCEDURE_CTRL_SUCCESS		2


#ifdef WDS
int rtl8192cd_tx_wdsDevProc(struct rtl8192cd_priv *priv, struct sk_buff *skb, struct net_device **dev_p,
				struct net_device **wdsDev_p, struct tx_insn *txcfg)
{
	struct stat_info *pstat;

	txcfg->wdsIdx = getWdsIdxByDev(priv, *dev_p);
	if (txcfg->wdsIdx < 0) {
		priv->ext_stats.tx_drops++;
		DEBUG_ERR("TX DROP: getWdsIdxByDev() fail!\n");
		goto free_and_stop;
	}

	if (!netif_running(priv->dev)) {
		priv->ext_stats.tx_drops++;
		DEBUG_ERR("TX DROP: Can't send WDS packet due to wlan interface is down!\n");
		goto free_and_stop;
	}
	pstat = get_stainfo(priv, priv->pmib->dot11WdsInfo.entry[txcfg->wdsIdx].macAddr);
	if (NULL == pstat) {
		priv->ext_stats.tx_drops++;
		DEBUG_ERR("TX DROP: %s: get_stainfo() for wds failed [%d]!\n", (char *)__FUNCTION__, txcfg->wdsIdx);
		goto free_and_stop;
	}
	if (pstat->current_tx_rate==0) {
		priv->ext_stats.tx_drops++;
		DEBUG_ERR("TX DROP: Can't send packet due to tx rate is not supported in peer WDS AP!\n");
		goto free_and_stop;
	}
	*wdsDev_p = *dev_p;
	*dev_p = priv->dev;

	/* Reply caller function : Continue process */
	return TX_PROCEDURE_CTRL_CONTINUE;

free_and_stop:		/* Free current packet and stop TX process */

	rtl_kfree_skb(priv,skb,_SKB_TX_);

	/* Reply caller function : STOP process */
	return TX_PROCEDURE_CTRL_STOP;
}
#endif


#ifdef CLIENT_MODE
int rtl8192cd_tx_clientMode(struct rtl8192cd_priv *priv, struct sk_buff **pskb)
{
	struct sk_buff *skb=*pskb;
	int DontEnterNat25=0;

	{
#ifdef RTK_BR_EXT
		int res, is_vlan_tag=0, i, do_nat25=1;
		unsigned short vlan_hdr=0;
		int lltd_flag=0;

		if (!priv->pmib->wscEntry.wsc_enable)
#ifdef MULTI_MAC_CLONE			
			if (mac_clone_handle_frame(priv, skb)) {
				/*if STAs number exceed max macclone support number then let it run nat25 path*/
                ACTIVE_ID = 0;
				//priv->ext_stats.tx_drops++;
				//DEBUG_ERR("TX DROP: exceed max clone address!\n");
				//goto free_and_stop;
			}
#else
			mac_clone_handle_frame(priv, skb);
		if(priv->pmib->ethBrExtInfo.macclone_enable && priv->macclone_completed){
			if(!memcmp(skb->data+ETH_ALEN, GET_MY_HWADDR, ETH_ALEN))	{
				DontEnterNat25=1;
			}
		}
#endif
		if ((!priv->pmib->ethBrExtInfo.nat25_disable && DontEnterNat25==0) 
#ifdef MULTI_MAC_CLONE
			&& ((ACTIVE_ID == 0) || (ACTIVE_ID > 0 && priv->pshare->mclone_sta[ACTIVE_ID-1].usedStaAddrId != 0xff))//(ACTIVE_ID == 0)//change for wifi-sta
#endif		
			)
		{
			if (*((unsigned short *)(skb->data+MACADDRLEN*2)) == __constant_htons(ETH_P_8021Q)) {
				is_vlan_tag = 1;
				vlan_hdr = *((unsigned short *)(skb->data+MACADDRLEN*2+2));
				for (i=0; i<6; i++)
					*((unsigned short *)(skb->data+MACADDRLEN*2+2-i*2)) = *((unsigned short *)(skb->data+MACADDRLEN*2-2-i*2));
				skb_pull(skb, 4);
			}

            if(priv->nat25_filter) {
                if(nat25_filter(priv, skb)== 1) {
                    priv->ext_stats.tx_drops++;
                    DEBUG_ERR("TX DROP: nat25 filter out!\n");
                    goto free_and_stop;
                }
            }


			if ((*((unsigned short *)(skb->data+MACADDRLEN*2)) == __constant_htons(ETH_P_IP)) && !IS_MCAST(skb->data)) {
				if (memcmp(priv->scdb_mac, skb->data+MACADDRLEN, MACADDRLEN)) {
					if ((priv->scdb_entry = (struct nat25_network_db_entry *)scdb_findEntry(priv,
								skb->data+MACADDRLEN, skb->data+WLAN_ETHHDR_LEN+12)) != NULL) {
						memcpy(priv->scdb_mac, skb->data+MACADDRLEN, MACADDRLEN);
						memcpy(priv->scdb_ip, skb->data+WLAN_ETHHDR_LEN+12, 4);
						priv->scdb_entry->ageing_timer = jiffies;
						do_nat25 = 0;
					}
				}
				else {
					if (priv->scdb_entry) {
						priv->scdb_entry->ageing_timer = jiffies;
						do_nat25 = 0;
					}
					else {
						memset(priv->scdb_mac, 0, MACADDRLEN);
						memset(priv->scdb_ip, 0, 4);
					}
				}
			}

			if (*((unsigned short *)(skb->data+MACADDRLEN*2)) == __constant_htons(0x88d9)) {
				if(skb->data[0] & 0x1)
				{
					do_nat25=0;
					lltd_flag=1;
				}
			}

			if (do_nat25)
			{
				if (nat25_db_handle(priv, skb, NAT25_CHECK) == 0) {
					struct sk_buff *newskb;

					if (is_vlan_tag) {
						skb_push(skb, 4);
						for (i=0; i<6; i++)
							*((unsigned short *)(skb->data+i*2)) = *((unsigned short *)(skb->data+4+i*2));
						*((unsigned short *)(skb->data+MACADDRLEN*2)) = __constant_htons(ETH_P_8021Q);
						*((unsigned short *)(skb->data+MACADDRLEN*2+2)) = vlan_hdr;
					}

					newskb = skb_copy(skb, GFP_ATOMIC);
					if (newskb == NULL) {
						priv->ext_stats.tx_drops++;
						DEBUG_ERR("TX DROP: skb_copy fail!\n");
						goto free_and_stop;
					}
					dev_kfree_skb_any(skb);
					*pskb = skb = newskb;
					if (is_vlan_tag) {
						vlan_hdr = *((unsigned short *)(skb->data+MACADDRLEN*2+2));
						for (i=0; i<6; i++)
							*((unsigned short *)(skb->data+MACADDRLEN*2+2-i*2)) = *((unsigned short *)(skb->data+MACADDRLEN*2-2-i*2));
						skb_pull(skb, 4);
					}
				}

				res = nat25_db_handle(priv, skb, NAT25_INSERT);
				if (res < 0) {
					if (res == -2) {
						priv->ext_stats.tx_drops++;
						DEBUG_ERR("TX DROP: nat25_db_handle fail!\n");
						goto free_and_stop;
					}
					// we just print warning message and let it go
					DEBUG_WARN("nat25_db_handle INSERT fail!\n");
				}
			}

			if(lltd_flag != 1)
			{
				memcpy(skb->data+MACADDRLEN, GET_MY_HWADDR, MACADDRLEN);
			}

			if (is_vlan_tag) {
				skb_push(skb, 4);
				for (i=0; i<6; i++)
					*((unsigned short *)(skb->data+i*2)) = *((unsigned short *)(skb->data+4+i*2));
				*((unsigned short *)(skb->data+MACADDRLEN*2)) = __constant_htons(ETH_P_8021Q);
				*((unsigned short *)(skb->data+MACADDRLEN*2+2)) = vlan_hdr;
			}
		}
		else{

            /*even nat25 is disabled, we still do nat25 for bridge's ip/mac*/
            if (!memcmp(skb->data+MACADDRLEN, priv->br_mac, MACADDRLEN)) {
                unsigned short ethtype = *((unsigned short *)(skb->data+MACADDRLEN*2));
                i = 0;
                if(ethtype == __constant_htons(ETH_P_IP)) {
                    i = 12;
                }
                else if(ethtype == __constant_htons(ETH_P_ARP)) {
                    i= 14;
                }

                if(i) {                           
                    memcpy(priv->br_ip, skb->data+WLAN_ETHHDR_LEN+i, 4);                    
                    res = nat25_db_handle(priv, skb, NAT25_INSERT);
                    if (res < 0) {
                        if (res == -2) {
                            priv->ext_stats.tx_drops++;
                            DEBUG_ERR("TX DROP: nat25_db_handle fail!\n");
                            goto free_and_stop;
                        }
                        // we just print warning message and let it go
                        DEBUG_WARN("nat25_db_handle INSERT fail!\n");
                    }    
                    memcpy(skb->data+MACADDRLEN, GET_MY_HWADDR, MACADDRLEN);
                }
            }

#ifdef TX_SUPPORT_IPV6_MCAST2UNI            
			if (*((unsigned short *)(skb->data+MACADDRLEN*2)) == __constant_htons(ETH_P_8021Q)) {
				is_vlan_tag = 1;
			}

			if(is_vlan_tag){
				if(ICMPV6_MCAST_MAC(skb->data) && ICMPV6_PROTO1A_VALN(skb->data)){
					memcpy(skb->data+MACADDRLEN, GET_MY_HWADDR, MACADDRLEN);
				}
			}else
			{
				if(ICMPV6_MCAST_MAC(skb->data) && ICMPV6_PROTO1A(skb->data)){
					memcpy(skb->data+MACADDRLEN, GET_MY_HWADDR, MACADDRLEN);
				}
			}
#endif            
		}



		// check if SA is equal to our MAC
		if (memcmp(skb->data+MACADDRLEN, GET_MY_HWADDR, MACADDRLEN)) {
			priv->ext_stats.tx_drops++;
			DEBUG_ERR("TX DROP: untransformed frame SA:%02X%02X%02X%02X%02X%02X!\n",
				skb->data[6],skb->data[7],skb->data[8],skb->data[9],skb->data[10],skb->data[11]);
			goto free_and_stop;
		}
#endif // RTK_BR_EXT
	}

	/* Reply caller function : Continue process */
	return TX_PROCEDURE_CTRL_CONTINUE;

#ifdef RTK_BR_EXT
free_and_stop:		/* Free current packet and stop TX process */

	rtl_kfree_skb(priv, skb, _SKB_TX_);

//stop_proc:
	/* Reply caller function : STOP process */
	return TX_PROCEDURE_CTRL_STOP;
#endif

}
#endif // CLIENT_MODE


 
#ifdef SBWC
/*static*/ int rtl8192cd_tx_sbwc(struct rtl8192cd_priv *priv, struct stat_info	*pstat, struct sk_buff *skb)
{
	SBWC_MODE mode = pstat->SBWC_mode;
	unsigned int tx_count, tx_limit;
	unsigned char is_consuming_q = pstat->SBWC_consuming_q;
	int *head, *tail;
	struct sk_buff *pSkb;
	BOOLEAN ret;
	int ret_val=TX_PROCEDURE_CTRL_CONTINUE;
	unsigned long flags=0;
	int locked=0;

	if (!(OPMODE & WIFI_AP_STATE))
		return ret_val;

	if (!(mode & SBWC_MODE_LIMIT_STA_TX))
		return ret_val;

	SMP_TRY_LOCK_SBWC(flags, locked);

	tx_count = (pstat->SBWC_tx_count + skb->len);
	tx_limit = pstat->SBWC_tx_limit_byte;

	if (tx_count > tx_limit)
	{
		// over the bandwidth
		if (is_consuming_q)
		{
			// in rtl8192cd_SBWC_timer context
			priv->ext_stats.tx_drops++;
			DEBUG_ERR("TX DROP: SBWC bandwidth over!\n");
			rtl_kfree_skb(priv, skb, _SKB_TX_);
		}
		else
		{
			ret = enque(priv, &(pstat->SBWC_txq.head), &(pstat->SBWC_txq.tail),
						(unsigned long)(pstat->SBWC_txq.pSkb), NUM_TXPKT_QUEUE, (void *)skb);
			if (ret == FALSE) 
			{
				priv->ext_stats.tx_drops++;
				DEBUG_ERR("TX DROP: SBWC tx queue full!\n");
				rtl_kfree_skb(priv, skb, _SKB_TX_);
			}
		}
		ret_val = TX_PROCEDURE_CTRL_STOP;
	}
	else
	{
		// not over the bandwidth
		if (CIRC_CNT(pstat->SBWC_txq.head, pstat->SBWC_txq.tail, NUM_TXPKT_QUEUE) && !is_consuming_q) 
		{
			// there are already packets in queue, put in queue too for order
			ret = enque(priv, &(pstat->SBWC_txq.head), &(pstat->SBWC_txq.tail),
						(unsigned long)(pstat->SBWC_txq.pSkb), NUM_TXPKT_QUEUE, (void *)skb);

			if (ret == FALSE) 
			{
				priv->ext_stats.tx_drops++;
				DEBUG_WARN("TX DROP: SBWC tx queue full!\n");
				rtl_kfree_skb(priv, skb, _SKB_TX_);
			}
			ret_val = TX_PROCEDURE_CTRL_STOP;
		}
		else 
		{
			// can transmit directly
			pstat->SBWC_tx_count = tx_count;
		}
	}

	if (locked)
		SMP_UNLOCK_SBWC(flags);
	
	return ret_val;
}
#endif

#ifdef SMP_LOAD_BALANCE_SUPPORT
void rtl8192cd_xmit_dsr(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	unsigned long x;
	struct sk_buff *skb;
	int count = 0;
	unsigned long start_time, time = 0;
	int i, idx;

	if (rtl_atomic_read(&priv->pshare->skb_xmit_queue_len) <= 0)
		return;

	start_time = jiffies;
	time = start_time+RTL_MILISECONDS_TO_JIFFIES(priv->pshare->rf_ft_var.xmit_dsr_time);
	idx = priv->pshare->skb_xmit_queue_idx;

#if (MAX_SKB_XMIT_QUEUE > 1)
	do {
		for (i = 0; i < priv->pshare->skb_xmit_queue_num; i++)
#endif
	{
#if (MAX_SKB_XMIT_QUEUE > 1)
		idx = (idx == priv->pshare->skb_xmit_queue_num-1) ? 0 : (idx+1);
		if (skb_queue_len(&priv->pshare->skb_xmit_queue[idx]) == 0)
			continue;
#endif
		while ((skb = skb_dequeue(&priv->pshare->skb_xmit_queue[idx])) != NULL) {
			rtl_atomic_dec(&priv->pshare->skb_xmit_queue_len);
			
			SAVE_INT_AND_CLI(x);
			SMP_LOCK_XMIT(x);

			__rtl8192cd_start_xmit(skb, skb->dev, TX_NORMAL);

			RESTORE_INT(x);
			SMP_UNLOCK_XMIT(x);
			
			count++;

			if (time_after(jiffies, time))
				goto out;

#ifdef WIFI_LOGO_11N_4_2_47
			if (priv->is_ssid_ps == 1)
				goto out;
#endif
		}
	}
#if (MAX_SKB_XMIT_QUEUE > 1)
		if (rtl_atomic_read(&priv->pshare->skb_xmit_queue_len) <= 0)
			goto out;

		if (count >= priv->pshare->rf_ft_var.xmit_dsr_num)
			goto out;
		
		priv->pshare->xmit_dsr_redo++;
	} while (1);
#endif

out:
	priv->pshare->skb_xmit_queue_idx = idx;

	if (count > priv->pshare->max_xmit_dsr_skb)
		priv->pshare->max_xmit_dsr_skb = count;

	time = jiffies - start_time;
	if (time > priv->pshare->max_xmit_dsr_time)
		priv->pshare->max_xmit_dsr_time = time;
}

void rtl8192cd_xmit_smp(void *_dev)
{
	struct net_device *dev = _dev;
	struct rtl8192cd_priv *priv = GET_DEV_PRIV(dev);

	tasklet_schedule(&priv->pshare->xmit_tasklet);
}
#endif



#if defined(CONFIG_ARCH_RTL8198F) && defined(CONFIG_HWNAT_FLEETCONNTRACK) && defined(CONFIG_RTK_BR_SHORTCUT)
extern int (*wlantxptr)(struct sk_buff *, struct net_device *);
void init_ca_rx_brsc_wlan_tx_ptr(void)
{
	wlantxptr = rtl8192cd_start_xmit;
}
#endif

#ifdef SUPPORT_TX_MCAST2UNI

__IRAM_IN_865X
int rtl8192cd_start_xmit_noM2U(struct sk_buff *skb, struct net_device *dev)
{
	int ret;
	struct rtl8192cd_priv *priv;
	priv = GET_DEV_PRIV(dev);

#if (!defined(CONFIG_USB_HCI) && !defined(CONFIG_SDIO_HCI)) || defined(CONFIG_RTL_TRIBAND_SUPPORT)
	unsigned long x;

	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
		SAVE_INT_AND_CLI(x);
		SMP_LOCK_XMIT(x);
	}
#endif

	ret = __rtl8192cd_start_xmit(skb, dev, TX_NO_MUL2UNI);

#if (!defined(CONFIG_USB_HCI) && !defined(CONFIG_SDIO_HCI)) || defined(CONFIG_RTL_TRIBAND_SUPPORT)
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
		RESTORE_INT(x);
		SMP_UNLOCK_XMIT(x);
	}
#endif

	return ret;
}

#endif

 


#ifdef RESERVE_TXDESC_FOR_EACH_IF
int check_rsv_txdesc(struct rtl8192cd_priv *priv, int q_num, int txdesc_need)
{
	struct rtl8192cd_priv *root_priv = NULL;
	unsigned int lower_limit = priv->pshare->num_txdesc_lower_limit;
	unsigned int avail_cnt = priv->pshare->num_txdesc_cnt;
	unsigned int used, accu = 0, i;

	if (!IS_RSVQ(q_num))
		return 0;
	
	used = priv->use_txdesc_cnt[RSVQ(q_num)];

	if (IS_ROOT_INTERFACE(priv))
		root_priv = priv;
	else
		root_priv = GET_ROOT(priv);
	
	if (IS_ROOT_INTERFACE(priv)) {
		if (IS_DRV_OPEN(priv))
			accu += used;
	} else {
		if (IS_DRV_OPEN(root_priv))
			accu += MAX_NUM(root_priv->use_txdesc_cnt[RSVQ(q_num)], lower_limit);
	}
	
#ifdef UNIVERSAL_REPEATER
	if (IS_VXD_INTERFACE(priv)) {
		if (IS_DRV_OPEN(priv))
			accu += used;
	} else {
		if (IS_DRV_OPEN(root_priv->pvxd_priv))
			accu += MAX_NUM(root_priv->pvxd_priv->use_txdesc_cnt[RSVQ(q_num)], lower_limit);
	}
#endif

#ifdef MBSSID
	for (i=0; i<RTL8192CD_NUM_VWLAN; i++) {
		if (IS_DRV_OPEN(root_priv->pvap_priv[i])) {
			if (root_priv->pvap_priv[i] == priv)
				accu += used;
	else
				accu += MAX_NUM(root_priv->pvap_priv[i]->use_txdesc_cnt[RSVQ(q_num)], lower_limit);
		}
	}
#endif
	
	if (accu + txdesc_need <= avail_cnt)
		return 0;
	
	return -1;
}
#endif


#ifdef TX_EARLY_MODE
__MIPS16
__IRAM_IN_865X
static void insert_emcontent(struct rtl8192cd_priv *priv, struct tx_insn* txcfg, unsigned char *buf)
{
	struct stat_info *pstat = txcfg->pstat;
	unsigned int dw[2];

	dw[0] = set_desc((pstat->empkt_num & 0xf) |
							(((pstat->empkt_len[0]+pstat->emextra_len) << 4) & 0xfff0) |
							(((pstat->empkt_len[1]+pstat->emextra_len) << 16) & 0xfff0000) |
							(((pstat->empkt_len[2]+pstat->emextra_len) << 28) & 0xf0000000) 	
							);
	dw[1] = set_desc((((pstat->empkt_len[2]+pstat->emextra_len) >> 4) & 0xff) |
							(((pstat->empkt_len[3]+pstat->emextra_len) << 8) & 0xfff00) |							
							(((pstat->empkt_len[4]+pstat->emextra_len) << 20) & 0xfff00000) 
							);
	memcpy(buf, dw, 8);
}
#endif


#ifdef CONFIG_PCI_HCI
#if defined(AP_SWPS_OFFLOAD)
static int GetPktQoStype(struct rtl8192cd_priv *priv, struct stat_info *pstat, struct sk_buff *pskb)
{
	int ret = -1;
    int pri = 0;
    
    pri = get_skb_priority(priv, pskb, pstat, 0);
    ret = pri_to_qnum(priv, pri);

#if 0
	if (pstat)
	{
		
	GDEBUG("QOS_ENABLE=%x, APSD_ENABLE=%x, pstat->QosEnabled=%x, pstat->apsd_bitmap=%x \n",QOS_ENABLE,APSD_ENABLE,pstat->QosEnabled,pstat->apsd_bitmap);
#if defined(WIFI_WMM) && defined(WMM_APSD)
		if ((QOS_ENABLE) && (APSD_ENABLE) && (pstat->QosEnabled) && (pstat->apsd_bitmap & 0x0f)) {
			int pri = 0;

			pri = get_skb_priority(priv, pskb, pstat, 0);

			if (((pri == 7) || (pri == 6)) && (pstat->apsd_bitmap & 0x01)) {
                
				ret = 3; //VO
			}
			else if (((pri == 5) || (pri == 4)) && (pstat->apsd_bitmap & 0x02)) {
				ret = 2; //VI
			}
			else if (((pri == 0) || (pri == 3)) && (pstat->apsd_bitmap & 0x08)) {
				ret = 1; //BE
			}
			else if (pstat->apsd_bitmap & 0x04) {
				ret = 0; //BK
			}
			else
				goto legacy_ps;

        }
		else
legacy_ps:
#endif
		ret = 1; //BE
	}
	else {	// Multicast or Broadcast
		ret = -2;
	}
#endif
	return ret;
}
#endif
#endif // CONFIG_PCI_HCI


 

#if 0

#ifdef CONFIG_PCI_HCI
//-----------------------------------------------------------------------------
// Procedure:    Fill Tx Command Packet Descriptor
//
// Description:   This routine fill command packet descriptor. We assum that command packet
//				require only one descriptor.
//
// Arguments:   This function is only for Firmware downloading in current stage
//
// Returns:
//-----------------------------------------------------------------------------
int rtl8192cd_SetupOneCmdPacket(struct rtl8192cd_priv *priv, unsigned char *dat_content, unsigned short txLength, unsigned char LastPkt)
/*
	IN	PADAPTER		Adapter,
	IN	PRT_TCB			pTcb,
	IN	u1Byte			QueueIndex,
	IN	u2Byte			index,
	IN	BOOLEAN			bFirstSeg,
	IN	BOOLEAN			bLastSeg,
	IN	pu1Byte			VirtualAddress,
	IN	u4Byte			PhyAddressLow,
	IN	u4Byte			BufferLen,
	IN	BOOLEAN     		bSetOwnBit,
	IN	BOOLEAN			bLastInitPacket,
	IN    u4Byte			DescPacketType,
	IN	u4Byte			PktLen
	)
*/
{

	unsigned char	ih=0;
	unsigned char	DescNum;
	unsigned short	DebugTimerCount;

	struct tx_desc	*pdesc;
	struct tx_desc	*phdesc;
	volatile unsigned int *ppdesc  ; //= (unsigned int *)pdesc;
	int	*tx_head, *tx_tail;
	struct rtl8192cd_hw	*phw = GET_HW(priv);

	tx_head	= (int *)&phw->txcmdhead;
	tx_tail = (int *)&phw->txcmdtail;
	phdesc = phw->txcmd_desc;

	DebugTimerCount = 0; // initialize debug counter to exit loop
	DescNum = 1;

//TODO: Fill the dma check here

//	printk("data lens: %d\n", txLength );

	for (ih=0; ih<DescNum; ih++) {
		pdesc      = (phdesc + (*tx_head));
		ppdesc = (unsigned int *)pdesc;
		// Clear all status
		memset(pdesc, 0, 36);
//		rtl_cache_sync_wback(priv, phw->txcmd_desc_dma_addr[*tx_head], 32, PCI_DMA_TODEVICE);
		// For firmware downlaod we only need to set LINIP
		if (LastPkt)
			pdesc->Dword0 |= set_desc(TX_LINIP);

		// From Scott --> 92SE must set as 1 for firmware download HW DMA error
		pdesc->Dword0 |= set_desc(TX_FirstSeg);;//bFirstSeg;
		pdesc->Dword0 |= set_desc(TX_LastSeg);;//bLastSeg;

		// 92SE need not to set TX packet size when firmware download
		pdesc->Dword7 |=  (set_desc((unsigned short)(txLength) << TX_TxBufSizeSHIFT));

		memcpy(priv->pshare->txcmd_buf, dat_content, txLength);

		rtl_cache_sync_wback(priv, (unsigned long)priv->pshare->txcmd_buf, txLength, PCI_DMA_TODEVICE);

#ifdef CONFIG_RTL_8812_SUPPORT
		if(GET_CHIP_VER(priv)== VERSION_8812E)
			pdesc->Dword10 =  set_desc(priv->pshare->cmdbuf_phyaddr);
		else
#endif
		pdesc->Dword8 =  set_desc(priv->pshare->cmdbuf_phyaddr);


//		pdesc->Dword0	|= set_desc((unsigned short)(txLength) << TX_PktIDSHIFT);
		pdesc->Dword0	|= set_desc((unsigned short)(txLength) << TX_PktSizeSHIFT);
		//if (bSetOwnBit)
		{
			pdesc->Dword0 |= set_desc(TX_OWN);
//			*(ppdesc) |= set_desc(BIT(31));
		}

#ifndef USE_RTL8186_SDK
		rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(phw->txcmd_desc_dma_addr[*tx_head]-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(struct tx_desc), PCI_DMA_TODEVICE);
#endif
		*tx_head = (*tx_head + 1) & (NUM_CMD_DESC - 1);
	}

	return TRUE;
}
#endif // CONFIG_PCI_HCI
#endif

#ifdef CONFIG_RTK_MESH
#ifndef __LINUX_2_6__
__MIPS16
#endif
int reuse_meshhdr(struct rtl8192cd_priv *priv, struct tx_insn *txcfg)
{
	const short meshhdrlen= (txcfg->mesh_header.mesh_flag & 0x01) ? 16 : 4;
	struct sk_buff *pskb = (struct sk_buff *)txcfg->pframe;
	if (skb_cloned(pskb))
	{
		struct sk_buff	*newskb = skb_copy(pskb, GFP_ATOMIC);
		rtl_kfree_skb(priv, pskb, _SKB_TX_);
		if (newskb == NULL) {
			priv->ext_stats.tx_drops++;
			release_wlanllchdr_to_poll(priv, txcfg->phdr);
			DEBUG_ERR("TX DROP: Can't copy the skb!\n");
			return 0;
		}
		txcfg->pframe = pskb = newskb;
#ifdef ENABLE_RTL_SKB_STATS
		rtl_atomic_inc(&priv->rtl_tx_skb_cnt);
#endif
	}
	memcpy(skb_push(pskb,meshhdrlen), &(txcfg->mesh_header), meshhdrlen);
	txcfg->fr_len += meshhdrlen;
	return 1;
}
#endif //CONFIG_RTK_MESH


#if defined(WIFI_WMM) && defined(DRVMAC_LB)
void SendLbQosNullData(struct rtl8192cd_priv *priv)
{
	struct wifi_mib *pmib;
	unsigned char *hwaddr;
	unsigned char tempQosControl[2];
	DECLARE_TXINSN(txinsn);


	pmib = GET_MIB(priv);
	hwaddr = pmib->dot11OperationEntry.hwaddr;

	if(!pmib->miscEntry.drvmac_lb) {
		printk("LB mode disabled, cannot SendLbQosNullData!!!\n");
		return;
	}

	if(!memcmp(pmib->miscEntry.lb_da, "\x0\x0\x0\x0\x0\x0", 6)) {
		printk("LB addr is NULL, cannot SendLbQosNullData!!!\n");
		return;
	}

	txinsn.retry = priv->pmib->dot11OperationEntry.dot11ShortRetryLimit;
	txinsn.q_num = BE_QUEUE;
//	txinsn.q_num = MANAGE_QUE_NUM;
	txinsn.tx_rate = find_rate(priv, NULL, 0, 1);
#ifndef TX_LOWESTRATE	
	txinsn.lowest_tx_rate = txinsn.tx_rate;
#endif	
	txinsn.fixed_rate = 1;
	txinsn.fr_type = _PRE_ALLOCHDR_;
	txinsn.phdr = get_wlanhdr_from_poll(priv);
	txinsn.pframe = NULL;

	if (txinsn.phdr == NULL)
		goto send_qos_null_fail;

	memset((void *)(txinsn.phdr), 0, sizeof (struct	wlan_hdr));

	SetFrameSubType(txinsn.phdr, BIT(7) | WIFI_DATA_NULL);
	SetFrDs(txinsn.phdr);
	memcpy((void *)GetAddr1Ptr((txinsn.phdr)), pmib->miscEntry.lb_da, MACADDRLEN);
	memcpy((void *)GetAddr2Ptr((txinsn.phdr)), hwaddr, MACADDRLEN);
	memcpy((void *)GetAddr3Ptr((txinsn.phdr)), hwaddr, MACADDRLEN);
	txinsn.hdr_len = WLAN_HDR_A3_QOS_LEN;

	memset(tempQosControl, 0, 2);
//	tempQosControl[0] = 0x07;		//set priority to VO
//	tempQosControl[0] |= BIT(4);	//set EOSP
	memcpy((void *)GetQosControl((txinsn.phdr)), tempQosControl, 2);

	if ((rtl8192cd_firetx(priv, &txinsn)) == SUCCESS)
		return;

send_qos_null_fail:

	if (txinsn.phdr)
		release_wlanhdr_to_poll(priv, txinsn.phdr);
}


void SendLbQosData(struct rtl8192cd_priv *priv)
{
	struct wifi_mib *pmib;
	unsigned char *hwaddr;
	unsigned char tempQosControl[2];
	unsigned char	*pbuf;
	static unsigned int pkt_length = 1;
	unsigned int max_len = 600; //if want to change to larger size, need to modify buf allocate size too
	DECLARE_TXINSN(txinsn);

	pmib = GET_MIB(priv);
	hwaddr = pmib->dot11OperationEntry.hwaddr;

	if(!pmib->miscEntry.drvmac_lb) {
		printk("LB mode disabled, cannot SendLbQosData!!!\n");
		return;
	}

	if(!memcmp(pmib->miscEntry.lb_da, "\x0\x0\x0\x0\x0\x0", 6)) {
		printk("LB addr is NULL, cannot SendLbQosData!!!\n");
		return;
	}

	txinsn.retry = priv->pmib->dot11OperationEntry.dot11ShortRetryLimit;
	txinsn.q_num = BE_QUEUE;
	txinsn.fr_type = _PRE_ALLOCMEM_;
	txinsn.tx_rate = find_rate(priv, NULL, 0, 1);
#ifndef TX_LOWESTRATE
	txinsn.lowest_tx_rate = txinsn.tx_rate;
#endif	
	txinsn.fixed_rate = 1;
//	txinsn.pframe = NULL;

	pbuf = txinsn.pframe = get_mgtbuf_from_poll(priv);
	if (pbuf == NULL)
		goto send_qos_fail;

	txinsn.phdr = get_wlanhdr_from_poll(priv);
	if (txinsn.phdr == NULL)
		goto send_qos_fail;

	memset((void *)(txinsn.phdr), 0, sizeof (struct	wlan_hdr));
	memset((void *)pbuf, 0, PRE_ALLOCATED_BUFSIZE);

	SetFrameSubType(txinsn.phdr, WIFI_QOS_DATA);
	SetFrDs(txinsn.phdr);
	memcpy((void *)GetAddr1Ptr((txinsn.phdr)), pmib->miscEntry.lb_da, MACADDRLEN);
	memcpy((void *)GetAddr2Ptr((txinsn.phdr)), hwaddr, MACADDRLEN);
	memcpy((void *)GetAddr3Ptr((txinsn.phdr)), hwaddr, MACADDRLEN);
	txinsn.hdr_len = WLAN_HDR_A3_QOS_LEN;

	memset(tempQosControl, 0, 2);
	memcpy((void *)GetQosControl((txinsn.phdr)), tempQosControl, 2);

//	printk("--0x%02x%02x%02x%02x 0x%02x%02x%02x%02x--\n", *pbuf, *(pbuf+1), *(pbuf+2), *(pbuf+3), *(pbuf+4), *(pbuf+5), *(pbuf+6), *(pbuf+7));

	if (pmib->miscEntry.lb_mlmp == 1) {
		// all zero in payload
		memset((void *)pbuf, 0x00, pkt_length);
		pbuf += pkt_length;
		txinsn.fr_len += pkt_length;
	}
	else if (pmib->miscEntry.lb_mlmp == 2) {
		// all 0xff in payload
		memset((void *)pbuf, 0xff, pkt_length);
//		printk("~~0x%02x%02x%02x%02x 0x%02x%02x%02x%02x~~\n", *pbuf, *(pbuf+1), *(pbuf+2), *(pbuf+3), *(pbuf+4), *(pbuf+5), *(pbuf+6), *(pbuf+7));
		pbuf += pkt_length;
		txinsn.fr_len += pkt_length;
	}
	else if ((pmib->miscEntry.lb_mlmp == 3) || (pmib->miscEntry.lb_mlmp == 4)) {
		// all different value in payload, 0x00~0xff
		unsigned int i = 0;
//		printk("~~");
		for (i = 0; i <pkt_length; i++) {
			memset((void *)pbuf, i%0x100, 1);
//			printk("%02x", *pbuf);
			pbuf += 1;
			txinsn.fr_len += 1;
		}
//		printk("~~\n");
	}
	else {
		printk("wrong LB muli-length-multi-packet setting!!\n");
		goto send_qos_fail;
	}

//	if (pkt_length++ >= 600)
//	if (pkt_length++ >= 2048)
	if (pkt_length++ >= max_len)	
		pkt_length = 1;

	if ((rtl8192cd_firetx(priv, &txinsn)) == SUCCESS)
		return;

send_qos_fail:

	if (txinsn.phdr)
		release_wlanhdr_to_poll(priv, txinsn.phdr);
	if (txinsn.pframe)
		release_mgtbuf_to_poll(priv, txinsn.pframe);
}
#endif

#ifdef TX_EARLY_MODE
__MIPS16
__IRAM_IN_865X
static void get_tx_early_info(struct rtl8192cd_priv *priv, struct stat_info  *pstat,  struct sk_buff_head *pqueue)
{	
	struct sk_buff *next_skb;

	pstat->empkt_num = 0;
	memset(pstat->empkt_len, '\0', sizeof(pstat->empkt_len));
	
#ifdef CONFIG_RTL_88E_SUPPORT
	if (GET_CHIP_VER(priv) == VERSION_8188E) {
		int tmplen = 0, extra_len = 0;
		
		if (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _CCMP_PRIVACY_) {
			extra_len = 16;
		} else if (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _TKIP_PRIVACY_) {
			extra_len = 20;
		} else if (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _WEP_40_PRIVACY_ ||
			priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _WEP_104_PRIVACY_) {
			extra_len = 8;
		}
		
		skb_queue_walk(pqueue, next_skb) {
			if ((pstat->empkt_num %2) == 0) {
				pstat->empkt_len[pstat->empkt_num/2] = next_skb->len+WLAN_HDR_A3_LEN+WLAN_CRC_LEN+extra_len;
			} else {
				tmplen = pstat->empkt_len[(pstat->empkt_num-1)/2];
				tmplen += ((tmplen%4)?(4-tmplen%4):0)+4;
				tmplen += next_skb->len+WLAN_HDR_A3_LEN+WLAN_CRC_LEN+extra_len;	
				pstat->empkt_len[(pstat->empkt_num-1)/2] = tmplen;
			}
		
			pstat->empkt_num++;
			if (skb_queue_is_last(pqueue, next_skb))
				break;

			if (pstat->empkt_num >= 10)
				break;
		}	
	} else 
#endif	
	skb_queue_walk(pqueue, next_skb) {
		pstat->empkt_len[pstat->empkt_num++] = next_skb->len;

		if (skb_queue_is_last(pqueue, next_skb))
			break;

		if (pstat->empkt_num >= 5)
			break;
	}	
}
#endif

#ifdef RTK_ATM
__inline__ static void rtl8192cd_atm_swq_settimer(struct rtl8192cd_priv *priv, UINT32 timer_timeout)
{
    UINT32 set_timeout, current_time;    
    unsigned char set_timer = 0;

	//delete timer when timeout == 0
    if(timer_timeout == 0) {
		if(priv->pshare->atm_swq_current_timeout) {
            priv->pshare->atm_swq_current_timeout = 0;
	    	if(priv->pshare->atm_swq_use_hw_timer) {
#ifndef SMP_LOAD_BALANCE_SUPPORT
                RTL_W32(TC4_CTRL, 0);
#endif
	        } else {      
	        	if (timer_pending(&priv->pshare->atm_swq_sw_timer))
					del_timer_sync(&priv->pshare->atm_swq_sw_timer);
	        }
		}
		priv->pshare->atm_timer_init = 0;//reset timer
    }
    else {
        if(priv->pshare->atm_swq_use_hw_timer) {
#ifdef SMP_LOAD_BALANCE_SUPPORT
            priv->pshare->atm_swq_current_timeout = timer_timeout;
#else			
#ifdef CONFIG_WLAN_HAL
            if (IS_HAL_CHIP(priv)) {
            	GET_HAL_INTERFACE(priv)->GetHwRegHandler(priv, HW_VAR_TSF_TIMER, (pu1Byte)&current_time);
            } else
#endif
            {
            	current_time = RTL_R32(TSFTR1);   
            }			
            set_timeout = current_time + SWQ_HWTIMER_MINIMUN;
            if(timer_timeout - set_timeout > SWQ_HWTIMER_MAXIMUN) {                
                timer_timeout = set_timeout;
                set_timeout = SWQ_HWTIMER_MINIMUN;                
            }            
            else {
                set_timeout = timer_timeout - current_time;
            }

            if(priv->pshare->atm_swq_current_timeout) {
                if((timer_timeout + SWQ_HWTIMER_TOLERANCE - priv->pshare->atm_swq_current_timeout) > SWQ_HWTIMER_MAXIMUN) {
                    set_timer = 1;
                }
            } else {
                set_timer = 1;
            }

            if(set_timer) {                         
                set_timeout = RTL_MICROSECONDS_TO_GTIMERCOUNTER(set_timeout);                
                set_timeout = BIT26 | BIT24 | (set_timeout & 0x00FFFFFF);  //TC40INT_EN | TC4EN | TC4Data
                RTL_W32(TC4_CTRL, set_timeout);           
				priv->pshare->atm_swq_current_timeout = (timer_timeout == 0?1:timer_timeout);
            }      
#endif			
        } else {
        	if(priv->pshare->atm_swq_current_timeout) {                          
                if(timer_timeout < priv->pshare->atm_swq_current_timeout)               
                    set_timer = 1;
            } else {
                set_timer = 1;
            }

            if(set_timer){
				priv->pshare->atm_swq_current_timeout = timer_timeout;
        		mod_timer(&priv->pshare->atm_swq_sw_timer, (unsigned long)timer_timeout);
            }
        }
    }
}

void rtl8192cd_atm_swq_inittimer(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	UINT32 tri_time = 0;

	if (priv->pshare->atm_swq_use_hw_timer) {
#ifdef CONFIG_WLAN_HAL
		if (IS_HAL_CHIP(priv)) {
			GET_HAL_INTERFACE(priv)->GetHwRegHandler(priv, HW_VAR_TSF_TIMER, (pu1Byte)&tri_time);
			tri_time += priv->pshare->atm_timer * 1000;
		} else
#endif
		{
			tri_time = RTL_R32(TSFTR1) + priv->pshare->atm_timer * 1000;
		}
	} else {
		tri_time = jiffies + RTL_MILISECONDS_TO_JIFFIES(priv->pshare->atm_timer);
	}

	priv->pshare->atm_swq_current_timeout = 0;
	rtl8192cd_atm_swq_settimer(priv, tri_time);	
}

__inline__ static int rtl8192cd_atm_swq_dequeue(struct rtl8192cd_priv *priv);


void rtl8192cd_atm_swq_timeout(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	UINT32 tri_time = 0;
	unsigned long x = 0;

	priv->pshare->atm_timeout_count++;
#ifdef SMP_LOAD_BALANCE_SUPPORT
	rtl8192cd_xmit_dsr(task_priv);
#endif

    SMP_LOCK_XMIT(x);
    SAVE_INT_AND_CLI(x);
	
	//atm swq dequeue
	rtl8192cd_atm_swq_dequeue(priv);

	//set next timer
	if (priv->pshare->atm_swq_use_hw_timer) {
#ifdef CONFIG_WLAN_HAL
		if (IS_HAL_CHIP(priv)) {
			GET_HAL_INTERFACE(priv)->GetHwRegHandler(priv, HW_VAR_TSF_TIMER, (pu1Byte)&tri_time);
			tri_time += priv->pshare->atm_timer * 1000;
		} else
#endif
		{
			tri_time = RTL_R32(TSFTR1) + priv->pshare->atm_timer * 1000;
		}
	} else {
		tri_time = jiffies + RTL_MILISECONDS_TO_JIFFIES(priv->pshare->atm_timer);
	}
	
	priv->pshare->atm_swq_current_timeout = 0;
	if(priv->pshare->atm_swq_en)
		rtl8192cd_atm_swq_settimer(priv, tri_time);	
	else
		rtl8192cd_atm_swq_settimer(priv, 0);

#ifdef SMP_LOAD_BALANCE_SUPPORT
	RTL_W32(TC4_CTRL, BIT26 | BIT25 | BIT24 | (RTL_MICROSECONDS_TO_GTIMERCOUNTER(1000) & 0x00FFFFFF));
#endif

	RESTORE_INT(x);		
    SMP_UNLOCK_XMIT(x); 
}

void atm_check_refill(struct rtl8192cd_priv *priv)
{
	int i;
	int is_all_burst=1, min_burst_sent=0xffff;
	struct stat_info *pstat=NULL;
	
	for(i=0; i<NUM_STAT; i++) {
		if (priv->pshare->aidarray[i] && (priv->pshare->aidarray[i]->used == TRUE)){
			pstat = &(priv->pshare->aidarray[i]->station);
			//its means someone has quota to send
			if(pstat->atm_burst_sent<pstat->atm_burst_num && is_all_burst==1){
				if((pstat->atm_burst_sent > pstat->atm_burst_num/1000) && pstat->atm_is_maxsta)
					is_all_burst = 0;//not all sta is burst out
				else
					;//sta sent only 1/1000 packets, so others keep sending burst packet
			}		
		}
	}

	//if all other client is burst
	if(is_all_burst==1){	
		for(i=0; i<NUM_STAT; i++) {
			if (priv->pshare->aidarray[i] && (priv->pshare->aidarray[i]->used == TRUE)){
				pstat = &(priv->pshare->aidarray[i]->station);

				if(pstat->atm_burst_sent >= pstat->atm_burst_num)
					pstat->atm_burst_sent -= pstat->atm_burst_num;
				else
					pstat->atm_burst_sent = 0;
			}
		}
	}
}

int atm_skb_dequeue(struct rtl8192cd_priv *priv, struct stat_info *pstat, int deq_size, int qnum)
{	
	int count = 0, flush_all = 0;
	struct sk_buff *tmpskb;
    
	if(deq_size == priv->pshare->rf_ft_var.atm_aggmax)
		deq_size = 0xff;
    
    while (1)
    {        
        tmpskb = skb_dequeue(&pstat->atm_swq.swq_queue[qnum]);
        if (tmpskb == NULL)
            break;

#ifdef TX_EARLY_MODE
        if (GET_TX_EARLY_MODE && GET_EM_SWQ_ENABLE) 
            get_tx_early_info(priv, pstat, &pstat->atm_swq.swq_queue[qnum]);
#endif

        __rtl8192cd_start_xmit_out(tmpskb, pstat, NULL);
        count++;
		pstat->atm_burst_sent++;

		if(deq_size == 0xff)
			continue;

		//if full, sent 1 packet
		if(pstat->atm_burst_sent >= pstat->atm_burst_num){
			pstat->atm_full_sent_cnt++;
			break;
		}

		if(count == deq_size)			
			break;
	}

	pstat->atm_swq_sent_cnt += count;

    if(skb_queue_len(&pstat->atm_swq.swq_queue[qnum]) == 0)
        pstat->atm_swq.swq_empty[qnum] = 0;	
	
    return count;	
}

#define MAX_DEQUEUE_NUM 16

#if defined(SW_TX_QUEUE) || defined(RTK_ATM)
__inline__ static int rtl8192cd_swq_bdfull(struct rtl8192cd_priv *priv, unsigned int num,  unsigned char qnum)
{
    if (getAvailableTXBD(priv, qnum) < num)
        return 1;
    else
    	return 0;
}
#endif

unsigned int sta_idx[4]={0,0,0,0};

__inline__ static int rtl8192cd_atm_swq_dequeue(struct rtl8192cd_priv *priv)
{
	int i, qnum, q_len, deq_num=0, *pempty, burst_num=0;
	struct stat_info *pstat;
	int sta_cnt = 0, sta_used_cnt = 0, bd_full = 0;

	if(priv->pshare->rf_ft_var.atm_en==0 || priv->pshare->atm_swq_en==0)
		return 0;
	
	if(priv->pshare->atm_ttl_stanum==0)
		return 0;		
#if 0
	//VO dequeue -> VI -> BE -> BK
	for(qnum = VO_QUEUE; qnum >= BK_QUEUE ; qnum--){
		for(i=0; i<NUM_STAT; i++) {
			if (priv->pshare->aidarray[i] && (priv->pshare->aidarray[i]->used == TRUE)){
				pstat = &(priv->pshare->aidarray[i]->station);

				if(rtl8192cd_swq_bdfull(priv, pstat, qnum))
					pstat->atm_txbd_full[qnum-1]++;

				//burst_num = (priv->pshare->rf_ft_var.atm_aggmax*pstat->atm_burst_num)/priv->pshare->rf_ft_var.atm_quota;
				burst_num = (priv->pshare->rf_ft_var.atm_aggmax*pstat->atm_burst_size)/priv->pshare->atm_max_burstsize;

				if(burst_num < priv->pshare->rf_ft_var.atm_aggmin)
				burst_num = priv->pshare->rf_ft_var.atm_aggmin;
						
				pstat->atm_burst_unit = burst_num;
				deq_num = atm_skb_dequeue(priv, pstat, burst_num, qnum);
			}
		}
	}
#else
	for(qnum = VO_QUEUE; qnum >= BK_QUEUE ; qnum--){
		sta_cnt = 0;//0~63
		sta_used_cnt = 0;//used count

		i = sta_idx[qnum-1];
		
		while(sta_cnt < NUM_STAT){
			
			if (priv->pshare->aidarray[(i%NUM_STAT)] && (priv->pshare->aidarray[(i%NUM_STAT)]->used == TRUE)){
				pstat = &(priv->pshare->aidarray[(i%NUM_STAT)]->station);

				bd_full = rtl8192cd_swq_bdfull(priv, 0, qnum);
				if(!bd_full){					
					burst_num = (priv->pshare->rf_ft_var.atm_aggmax*pstat->atm_burst_size)/priv->pshare->atm_max_burstsize;
					if(burst_num < priv->pshare->rf_ft_var.atm_aggmin)
						burst_num = priv->pshare->rf_ft_var.atm_aggmin;
							
					pstat->atm_burst_unit = burst_num;
					deq_num = atm_skb_dequeue(priv, pstat, burst_num, qnum);					
				}else
					pstat->atm_txbd_full[qnum-1]++;

				//find 2nd sta
				if(sta_used_cnt == 1)
					sta_idx[qnum-1] = i%NUM_STAT;
								
				//printk("sta[%d][%d] send.\n", qnum, i%NUM_STAT);

				sta_used_cnt++;
			}

			//next
			i++;
			sta_cnt++;//0~63
		}
		//printk("\n");
	}

	//printk("-----------------------------------------\n");
#endif	
	return 0;
}
#endif//RTK_ATM

#ifdef CONFIG_PCI_HCI




#endif // CONFIG_PCI_HCI

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
int __rtl8192cd_start_xmit_usb(struct sk_buff *skb, struct net_device *dev, int tx_flag)
{
	struct rtl8192cd_priv *priv;
	struct stat_info	*pstat=NULL;
	unsigned char		*da;
#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
	struct sk_buff *newskb = NULL;
#else /* !defined(CONFIG_RTL_TRIBAND_SUPPORT) */
#ifdef __KERNEL__
#if defined(HS2_SUPPORT) || !defined(CONFIG_PCI_HCI)
	struct sk_buff *newskb = NULL;
#endif
#endif
#ifdef TX_SCATTER
	struct sk_buff *newskb = NULL;
#endif
#endif /* defined(CONFIG_RTL_TRIBAND_SUPPORT) */
	struct net_device *wdsDev = NULL;
	struct tx_insn tx_insn;
    int temp;
#ifdef SMP_SYNC
	unsigned long flags;
#endif


	DECLARE_TXCFG(txcfg, tx_insn);

	priv = GET_DEV_PRIV(dev);

#ifdef ENABLE_RTL_SKB_STATS
	rtl_atomic_inc(&priv->rtl_tx_skb_cnt);
#endif

	if (skb->len < 15)
    {
        _DEBUG_ERR("TX DROP: SKB len small:%d\n", skb->len);
        goto free_and_stop;
    }

        skb->dev = dev;

#ifdef WDS
	if (!dev->base_addr && skb_cloned(skb)
		&& (priv->pmib->dot11WdsInfo.wdsPrivacy == _TKIP_PRIVACY_)
		) {
		struct sk_buff *mcast_skb = NULL;
		mcast_skb = skb_copy(skb, GFP_ATOMIC);
		if (mcast_skb == NULL) {
			priv->ext_stats.tx_drops++;
			DEBUG_ERR("TX DROP: Can't copy the skb!\n");
			goto free_and_stop;
		}
		dev_kfree_skb_any(skb);
		skb = mcast_skb;
	}
#endif

#ifdef HS2_SUPPORT
	if (priv->proxy_arp)	{

		unsigned short protocol;

		if(isDHCPpkt(skb))	{
			staip_snooping_bydhcp(skb, priv);
		}

		if (IS_MCAST(skb->data))	{
			protocol = *((unsigned short *)(skb->data + 2 * ETH_ALEN));
	
			//proxy arp	
			if (protocol == __constant_htons(ETH_P_ARP))	{
				if (proxy_arp_handle(priv, skb))	{
					//reply ok and return
			        	goto stop_proc;
				}
				//drop packets
		        	goto free_and_stop;
			}
			//icmpv6 
			if (ICMPV6_MCAST_SOLI_MAC(skb->data))	{
			//if (ICMPV6_MCAST_MAC(skb->data))
				if (proxy_icmpv6_ndisc(priv,skb))	{
					goto stop_proc;
				}
				//drop packets
				goto free_and_stop;
			} 
			//drop unsolicited neighbor advertisement when proxy arp=1
			if (ICMPV6_MCAST_MAC(skb->data))	{
				if (check_nei_advt(priv,skb))	{
					 //drop packets
			             goto free_and_stop;
				}
			}
		}
	} 
	
	if(priv->dgaf_disable)
	{
		//dhcp m2u check
		if (IS_MCAST(skb->data))
		{
			if (isDHCPpkt(skb))
			{
				struct list_head *phead, *plist;
                HS2_DEBUG_INFO("DHCP multicast to unicast\n");
				phead = &priv->asoc_list;
				plist = phead->next;
				while (phead && (plist != phead)) 
				{
					pstat = list_entry(plist, struct stat_info, asoc_list);
					plist = plist->next;

					/* avoid   come from STA1 and send back STA1 */
			        if (!memcmp(pstat->cmn_info.mac_addr, &skb->data[6], 6))
					{
						continue;
					}
#if defined(CONFIG_RTL865X_ETH_PRIV_SKB) || defined(CONFIG_RTL_ETH_PRIV_SKB)
					newskb = priv_skb_copy(skb);
#else
					newskb = skb_copy(skb, GFP_ATOMIC);
#endif
	                if (newskb) 
					{
						memcpy(newskb->data, pstat->cmn_info.mac_addr, 6);
				        newskb->cb[2] = (char)0xff;         // not do aggregation
				        memcpy(newskb->cb+10,newskb->data,6);
                        #if 0   // for debug
						printk("send m2u, da=");						
						MAC_PRINT(pstat->cmn_info.mac_addr);
						printk("\n");
                        #endif
						__rtl8192cd_start_xmit(newskb, priv->dev, TX_NO_MUL2UNI);
					}
				}
			}
	        goto free_and_stop;
		}
	}
#endif

#ifdef SUPPORT_TX_MCAST2UNI
#ifdef WDS			// when interface is WDS don't enter multi2uni path
	if (dev->base_addr && !priv->pmib->dot11WdsInfo.wdsPure)
#endif
	if (!priv->pshare->rf_ft_var.mc2u_disable) {
		if ((OPMODE & WIFI_AP_STATE) &&	IS_MCAST(skb->data))
		{
			if ((IP_MCAST_MAC(skb->data)
				#ifdef	TX_SUPPORT_IPV6_MCAST2UNI
				|| IPV6_MCAST_MAC(skb->data)
				#endif
				) && (tx_flag != TX_NO_MUL2UNI))
			{
				if (mlcst2unicst(priv, skb)){
					return 0;
				}
			}
		}

		skb->cb[2] = 0;	// allow aggregation
	}
#endif

#ifdef SUPPORT_TX_MCAST2UNI
	skb->cb[16] = tx_flag;
#endif

#ifdef DFS
	if (!priv->pmib->dot11DFSEntry.disable_DFS &&
		GET_ROOT(priv)->pmib->dot11DFSEntry.disable_tx 
		&& (priv->site_survey)
		&& (priv->site_survey->hidden_ap_found != HIDE_AP_FOUND_DO_ACTIVE_SSAN)) {
		priv->ext_stats.tx_drops++;
		DEBUG_ERR("TX DROP: DFS probation period\n");
		goto free_and_stop;
	}
#endif

	if (!IS_DRV_OPEN(priv))
		goto free_and_stop;

#ifdef MP_TEST
	if (OPMODE & WIFI_MP_STATE) {
		goto free_and_stop;
	}
#endif

#ifdef MULTI_MAC_CLONE
	if (priv->pmib->ethBrExtInfo.macclone_enable) {
		int id;
		if (!(skb->data[ETH_ALEN] & 1) && MCLONE_NUM > 0) {
			id = mclone_find_address(priv, skb->data+ETH_ALEN, skb, MAC_CLONE_SA_FIND);
			if (id > 0)
				ACTIVE_ID = id;
			else
				ACTIVE_ID = 0;
		}  else
			ACTIVE_ID = 0;
	}
#endif

	// strip VLAN tag to avoid packet drop on peer which not support VLAN tag
	if (*((u16*)(skb->data+(ETH_ALEN<<1))) == __constant_htons(ETH_P_8021Q)) {
		if (skb_cloned(skb)) {
			newskb = skb_copy(skb, GFP_ATOMIC);
			if (newskb == NULL) {
				priv->ext_stats.tx_drops++;
				DEBUG_ERR("TX DROP: Can't copy the skb!\n");
				goto free_and_stop;
			}
			dev_kfree_skb_any(skb);
			skb = newskb;
		}
		memmove(skb->data + 4, skb->data, ETH_ALEN<<1);
		skb_pull(skb, 4);
	}

#ifdef WDS
	if (dev->base_addr) {
		// normal packets
		if (priv->pmib->dot11WdsInfo.wdsPure) {
			priv->ext_stats.tx_drops++;
			DEBUG_ERR("TX DROP: Sent normal pkt in Pure WDS mode!\n");
			goto free_and_stop;
		}
	}
	else {
		// WDS process
		if (rtl8192cd_tx_wdsDevProc(priv, skb, &dev, &wdsDev, txcfg) == TX_PROCEDURE_CTRL_STOP) {
			goto stop_proc;
		}
	}
#endif // WDS

	if (priv->pmib->miscEntry.func_off)
		goto free_and_stop;

	// drop packets if link status is null
#ifdef WDS
	if (!wdsDev)
#endif
	{
		if (priv->assoc_num == 0) {
#ifdef RTK_BR_EXT
			if(!((OPMODE & WIFI_STATION_STATE) && priv->pmib->ethBrExtInfo.macclone_enable && !priv->macclone_completed)){
#endif
				priv->ext_stats.tx_drops++;
				DEBUG_WARN("TX DROP: Non asoc tx request!\n");
				goto free_and_stop;
#ifdef RTK_BR_EXT
			}
#endif
		}
	}

#ifdef CLIENT_MODE
	// nat2.5 translation, mac clone, dhcp broadcast flag add.
	if (OPMODE & (WIFI_STATION_STATE|WIFI_ADHOC_STATE) ) {

#ifdef A4_STA
        pstat = get_stainfo(priv, BSSID);
        if(pstat && !(pstat->state & WIFI_A4_STA))
#endif
        {            
#ifdef RTK_BR_EXT
			if (!priv->pmib->ethBrExtInfo.nat25_disable &&
					!(skb->data[0] & 1) &&
#ifdef MULTI_MAC_CLONE
					(ACTIVE_ID == 0) &&
#endif		
#ifdef __KERNEL__
					GET_BR_PORT(priv->dev) &&
#endif
					memcmp(skb->data+MACADDRLEN, priv->br_mac, MACADDRLEN) &&
//					*((unsigned short *)(skb->data+MACADDRLEN*2)) != __constant_htons(ETH_P_8021Q) &&
					*((unsigned short *)(skb->data+MACADDRLEN*2)) == __constant_htons(ETH_P_IP) &&
					!memcmp(priv->scdb_mac, skb->data+MACADDRLEN, MACADDRLEN) && priv->scdb_entry) {
                if(priv->nat25_filter) {
                    if(nat25_filter(priv, skb)== 1) {
                        priv->ext_stats.tx_drops++;
                        DEBUG_ERR("TX DROP: nat25 filter out!\n");
                        goto free_and_stop;
                    }
                }

				memcpy(skb->data+MACADDRLEN, GET_MY_HWADDR, MACADDRLEN);
				priv->scdb_entry->ageing_timer = jiffies;
			}
			else
#endif		
			{
			if((OPMODE & WIFI_STATION_STATE) && memcmp(skb->data+ETH_ALEN, GET_MY_HWADDR, ETH_ALEN)) {	// ignore tx from root br to vap(client mode) if vap is not connected to remote ap successfully
				struct stat_info *pstat = get_stainfo(priv,BSSID);
				if (!((NULL != pstat) && (pstat->state & (WIFI_AUTH_SUCCESS | WIFI_ASOC_STATE))
					   && (((0 != pstat->dot11KeyMapping.dot11EncryptKey.dot11TTKeyLen) && (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm != _NO_PRIVACY_))
						   || (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _NO_PRIVACY_)))) {
					goto free_and_stop;
				}
			}
				SMP_UNLOCK_XMIT(flags);
				if (rtl8192cd_tx_clientMode(priv, &skb) == TX_PROCEDURE_CTRL_STOP) {
					SMP_LOCK_XMIT(flags);
					goto stop_proc;
				}
				SMP_LOCK_XMIT(flags);
			}
    		
#ifdef RTK_BR_EXT //8812_client
#ifdef MULTI_MAC_CLONE
			if (ACTIVE_ID >= 0)
				mclone_dhcp_caddr(priv, skb);		
			if (ACTIVE_ID == 0)				//not a mac clone sta
#endif
			dhcp_flag_bcast(priv, skb);
#endif
        }

	}
#endif // CLIENT_MODE

#ifdef MBSSID
	if (GET_ROOT(priv)->pmib->miscEntry.vap_enable)
	{
		if ((OPMODE & WIFI_AP_STATE) && !wdsDev) {
			if ((*(unsigned int *)&(skb->cb[20]) == 0x86518190) &&						// come from wlan interface
				#if !defined(__ECOS)
				(*(unsigned int *)&(skb->cb[24]) != priv->pmib->miscEntry.groupID)      // check group ID
				#else
				(*(unsigned int *)&(skb->cb[12]) != priv->pmib->miscEntry.groupID)      // check group ID				
				#endif
				)		
			{
				priv->ext_stats.tx_drops++;
				DEBUG_ERR("TX DROP: not the same group!\n");
				goto free_and_stop;
			}
		}
	}
#endif

#ifdef WDS
	if (wdsDev)
		da = priv->pmib->dot11WdsInfo.entry[txcfg->wdsIdx].macAddr;
	else
#endif
	{

#ifdef MCAST2UI_REFINE
        da = &skb->cb[10];
#else
		da = skb->data;
#endif
	}

#ifdef CLIENT_MODE
	if ((OPMODE & (WIFI_STATION_STATE | WIFI_ASOC_STATE)) == (WIFI_STATION_STATE | WIFI_ASOC_STATE))
		pstat = get_stainfo(priv, BSSID);
	else
#endif
	{
		pstat = get_stainfo(priv, da);
#ifdef A4_STA
        if (tx_flag == TX_NORMAL && pstat == NULL && (OPMODE & WIFI_AP_STATE)) {
            if(priv->pmib->miscEntry.a4_enable == 2) {
                if(IS_MCAST(da)) { 
                    /*duplicate the packet and use 4-address to send to every a4 client, 
                                      and send one 3-address packet  to other a3 clients*/
                    if( a4_tx_mcast_to_unicast(priv, skb) == 1) {
                        goto free_and_stop;
                    }
                }
                else {
                    pstat = a4_sta_lookup(priv, da);	
                    if(pstat == NULL) {
                        /*Unknown unicast, transmit to every A4 sta*/
                        a4_tx_unknown_unicast(priv, skb);                
                        goto free_and_stop;
                    }
                }

            }
            else if(priv->pmib->miscEntry.a4_enable == 1){
                if(!IS_MCAST(da)) {/*just send one 3-address multi/broadcast  for all clients*/
                    pstat = a4_sta_lookup(priv, da);
                }
            }
        }
#endif		
	}
#ifdef DETECT_STA_EXISTANCE
	if(pstat && pstat->leave)	{
		priv->ext_stats.tx_drops++;
		DEBUG_WARN("TX DROP: sta may leave! %02x%02x%02x%02x%02x%02x\n", pstat->cmn_info.mac_addr[0],pstat->cmn_info.mac_addr[1],pstat->cmn_info.mac_addr[2],pstat->cmn_info.mac_addr[3],pstat->cmn_info.mac_addr[4],pstat->cmn_info.mac_addr[5]);
		goto free_and_stop;
	}
#endif

#ifdef WIFI_SIMPLE_CONFIG
	if (pstat && (*((u16*)(skb->data+ETH_ALEN*2)) == __constant_htons(0x888e))
			&& (skb->data[15] == 0) && (skb->data[18] == 4)) {		// EAP-Failure
		wsc_disconn_list_add(priv, da);
	}
#endif

#ifdef TX_SCATTER
#ifdef CONFIG_IEEE80211W
		if (skb->list_num > 0 && (UseSwCrypto(priv, pstat, (pstat ? FALSE : TRUE), 0) ||
#else
		if (skb->list_num > 0 && (UseSwCrypto(priv, pstat, (pstat ? FALSE : TRUE)) ||
#endif
		(pstat && (get_sta_encrypt_algthm(priv, pstat) == _TKIP_PRIVACY_)))) {
		newskb = copy_skb(skb);
		if (newskb == NULL) {
			priv->ext_stats.tx_drops++;
			DEBUG_ERR("TX DROP: Can't copy the skb for list buffer!\n");
			goto free_and_stop;
		}
		dev_kfree_skb_any(skb);
		skb = newskb;
	}
#endif

	txcfg->fr_type = _SKB_FRAME_TYPE_;
	txcfg->pframe = skb;
	txcfg->pstat = pstat;
	
	if (update_txinsn_stage1(priv, txcfg) == FALSE) {
		priv->ext_stats.tx_drops++;
		goto free_and_stop;
	}

#if !defined(SUPPORT_TX_AMSDU)
	if ((txcfg->privacy) && (
#ifdef CONFIG_IEEE80211W
		UseSwCrypto(priv, pstat, (pstat ? FALSE : TRUE), 0)	// sw enc will modify content
#else
		UseSwCrypto(priv, pstat, (pstat ? FALSE : TRUE))	// sw enc will modify content
#endif
		|| (pstat && (_TKIP_PRIVACY_ == txcfg->privacy))))		// need append MIC
#endif
	{
		if (skb_cloned(skb)
#ifdef MCAST2UI_REFINE
                        && !memcmp(skb->data, &skb->cb[10], 6)
#endif
			)
		{
			newskb = skb_copy(skb, GFP_ATOMIC);
			if (newskb == NULL) {
				priv->ext_stats.tx_drops++;
				DEBUG_ERR("TX DROP: Can't copy the skb!\n");
				goto free_and_stop;
			}
			dev_kfree_skb_any(skb);
			skb = newskb;
		}
	}

	txcfg->pframe = skb;
	txcfg->next_txpath = TXPATH_HARD_START_XMIT;
	
#ifdef CONFIG_TCP_ACK_TXAGG
	if (priv->pshare->rf_ft_var.tcpack_agg && pstat && (pstat->state & WIFI_ASOC_STATE)) {
		if (*(u16 *)(skb->data + ETH_ALEN*2) == htons(ETH_P_IP)) {
			struct iphdr *iph = (struct iphdr *)(skb->data + ETH_HLEN);
			if (6 == iph->protocol) {	// IPPROTO_TCP
				struct tcphdr *tcph = (struct tcphdr *)((u8*)iph + iph->ihl*4);
				// only handle TCP ACK w/o TCP data packet
				if (iph->ihl*4+tcph->doff*4 == ntohs(iph->tot_len)) {
					int tcp_flag = tcp_flag_word(tcph) & __cpu_to_be32(0xff0000);
					if (TCP_FLAG_ACK == tcp_flag) {
					if (rtw_xmit_enqueue_tcpack(priv, txcfg) == FALSE) {
						goto free_and_stop;
					}
					goto stop_proc;
				}
			}
		}
	}
	}
#endif // CONFIG_TCP_ACK_TXAGG

		if (rtw_xmit_enqueue(priv, txcfg) == FALSE) {
			goto free_and_stop;
		}
		goto stop_proc;

free_and_stop:		/* Free current packet and stop TX process */

	rtl_kfree_skb(priv, skb, _SKB_TX_);

stop_proc:			/* Stop process and assume the TX-ed packet is already "processed" (freed or TXed) in previous code. */

	return 0;
}

int __rtl8192cd_usb_start_xmit(struct rtl8192cd_priv *priv, struct tx_insn* txcfg)
{
	struct net_device *dev;
	struct net_device *wdsDev = NULL;
	struct stat_info *pstat;
	struct sk_buff *skb;

	#ifdef CONFIG_RTK_MESH
	int block_mesh = 0;
	#endif    
#ifdef TX_SHORTCUT
	int k;
	struct tx_sc_entry *ptxsc_entry = NULL;
	unsigned char pktpri;
#endif
//	int priority;
#if defined(CONFIG_SDIO_HCI) && defined(TX_SCATTER)
	struct wlan_ethhdr_t	*pethhdr=NULL;
#endif
	
	skb = (struct sk_buff *)txcfg->pframe;

	if (!IS_DRV_OPEN(priv))
		goto free_and_stop;

	if (priv->pmib->miscEntry.func_off || priv->pmib->miscEntry.raku_only)
		goto free_and_stop;
	
	switch( rtw_xmit_decision(priv, txcfg) ) {
	case XMIT_DECISION_ENQUEUE:
		goto stop_proc;
	case XMIT_DECISION_STOP:
		priv->ext_stats.tx_drops++;
		goto free_and_stop;
	case XMIT_DECISION_CONTINUE:
		break;
	}
	
#ifdef WDS
	if (txcfg->wdsIdx >= 0) {
		wdsDev = priv->wds_dev[txcfg->wdsIdx];
	}
#endif
	
	dev = priv->dev;
	pstat = txcfg->pstat;
	
	if (pstat)
	{
		#ifdef GBWC
		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
			if (priv->pmib->gbwcEntry.GBWCMode)
				if (rtl8192cd_tx_gbwc(priv, pstat, skb) == TX_PROCEDURE_CTRL_STOP) 
					goto stop_proc;
		}
		#endif

		#ifdef SBWC
		if (rtl8192cd_tx_sbwc(priv, pstat, skb) == TX_PROCEDURE_CTRL_STOP) 
			goto stop_proc;
		#endif
	}

#ifdef SUPPORT_TX_AMSDU
	#if defined(CONFIG_RTK_MESH) && !defined(MESH_AMSDU)
	if(dev == priv->mesh_dev){
		block_mesh = 1;
	}
    #endif

	if (IS_SUPPORT_TX_AMSDU(priv)
#if defined(CONFIG_RTK_MESH)
		&& !block_mesh
#endif
		&& pstat && (pstat->aggre_mthd & AGGRE_MTHD_MSDU) && (pstat->amsdu_level > 0)
#ifdef SUPPORT_TX_MCAST2UNI
		&& (priv->pshare->rf_ft_var.mc2u_disable || (skb->cb[2] != (char)0xff))
#endif
		) {
		int ret = amsdu_check(priv, skb, pstat, txcfg);

		if (ret == RET_AGGRE_ENQUE)
			goto stop_proc;

		if (ret == RET_AGGRE_DESC_FULL)
			goto free_and_stop;
	}
#endif

#ifdef TX_SHORTCUT
	if (!priv->pmib->dot11OperationEntry.disable_txsc && pstat) {
		pktpri = get_skb_priority(priv, skb, pstat, 0);	
	#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
		if ((k = get_tx_sc_index_usb(priv, pstat, skb->data, pktpri)) >= 0)
	#else
		if ((k = get_tx_sc_index(priv, pstat, skb->data, pktpri)) >= 0)
	#endif
		{
			ptxsc_entry = &pstat->TX_SC_ENT[pktpri][k];
		}
	}
	
	if ((NULL != ptxsc_entry) &&
#if !defined(__ECOS)
		// go slow path to avoid TX drop when no enough tail room to put TKIP MIC.
		((_TKIP_PRIVACY_ != ptxsc_entry->txcfg.privacy) || ((skb->tail + 8) <= skb->end)) &&
#endif
		((ptxsc_entry->txcfg.privacy == 0) ||
#ifdef CONFIG_RTL_WAPI_SUPPORT
		  (ptxsc_entry->txcfg.privacy == _WAPI_SMS4_) ||
#endif
#ifdef CONFIG_IEEE80211W
		!UseSwCrypto(priv, pstat, FALSE, 0) ) &&
#else
		!UseSwCrypto(priv, pstat, FALSE) ) &&
#endif
		(ptxsc_entry->txcfg.fr_len > 0) &&
		(FRAGTHRSLD > 1500))
	{
#if defined(MESH_TX_SHORTCUT)
			if(txcfg->is_11s) {
                if(!mesh_txsc_decision(txcfg, &ptxsc_entry->txcfg)) { /*compare addr 5 and 6*/
                    goto just_skip;
                }

                if(memcmp(skb->data, GetAddr3Ptr(&ptxsc_entry->wlanhdr), MACADDRLEN)) {/*compare addr 3*/
                    goto just_skip;
                }

                ptxsc_entry->txcfg.mesh_header.segNum = txcfg->mesh_header.segNum;
                ptxsc_entry->txcfg.mesh_header.TTL = txcfg->mesh_header.TTL;
                ptxsc_entry->txcfg.is_11s = txcfg->is_11s;
			}
#endif

#ifdef A4_STA
            if(pstat->state & WIFI_A4_STA) {
                if(memcmp(skb->data, ptxsc_entry->ethhdr.daddr, MACADDRLEN)) {/*compare addr 3*/
                    goto just_skip;
                }
            }
#endif

			BUG_ON(txcfg->q_num != ptxsc_entry->txcfg.q_num);
			memcpy(txcfg, &ptxsc_entry->txcfg, FIELD_OFFSET(struct tx_insn, pxmitframe));

			{
			txcfg->phdr = (UINT8 *)get_wlanllchdr_from_poll(priv);
			if (txcfg->phdr == NULL) {
				DEBUG_ERR("Can't alloc wlan header!\n");
				rtl8192cd_tx_xmitSkbFail(priv, skb, dev, wdsDev, txcfg);
				goto stop_proc;
			}
			memcpy(txcfg->phdr, (const void *)&ptxsc_entry->wlanhdr, sizeof(struct wlanllc_hdr));
			txcfg->pframe = skb;
#ifdef WDS
			/*Correct the aggre_mthd*/
			if(pstat->state & WIFI_WDS)
				if(txcfg->aggre_en != pstat->aggre_mthd) {
					/*Invalid the tx_sc entry*/
					ptxsc_entry->txcfg.fr_len=0;
					txcfg->aggre_en = pstat->aggre_mthd;
				}	
#endif

				txcfg->fr_len = skb->len - WLAN_ETHHDR_LEN;
#if defined(CONFIG_SDIO_HCI) && defined(TX_SCATTER)
			// in case 1st buffer len is 14, we get ether header pointer first and then ajust the skb
			pethhdr = (struct wlan_ethhdr_t *)(skb->data);
#endif
			skb_pull(skb, WLAN_ETHHDR_LEN);
            }

#ifdef CONFIG_RTL_WAPI_SUPPORT
#ifdef CONFIG_RTL_HW_WAPI_SUPPORT
#ifdef CONFIG_IEEE80211W
			if((txcfg->privacy==_WAPI_SMS4_)&&(txcfg->llc>0)&&(UseSwCrypto(priv, pstat, FALSE, 0)))
#else
			if((txcfg->privacy==_WAPI_SMS4_)&&(txcfg->llc>0)&&(UseSwCrypto(priv, pstat, FALSE)))
#endif				
#else
			if((txcfg->privacy==_WAPI_SMS4_)&&(txcfg->llc>0))
#endif
			{				
				//To restore not-encrypted llc in wlan hdr
				//because llc in wlan hdr has been sms4encrypted to deliver at SecSWSMS4Encryption()
				eth_2_llc(&ptxsc_entry->ethhdr, (struct llc_snap *)(txcfg->phdr+txcfg->hdr_len + txcfg->iv));
			}
#endif

			if (txcfg->privacy == _TKIP_PRIVACY_) {
				if (rtl8192cd_tx_tkip(priv, skb, pstat, txcfg) == TX_PROCEDURE_CTRL_STOP) {
					goto stop_proc;
				}
			}

#ifdef MESH_TX_SHORTCUT
			if ( (txcfg->is_11s&1) && (GetFrameSubType(txcfg->phdr) == WIFI_11S_MESH))
                    if( !reuse_meshhdr(priv, txcfg) ) {
					        goto stop_proc;
					}
#endif
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
					if ((GetFrameSubType(txcfg->phdr) & (BIT2 | BIT3 | BIT6 | BIT7)) == (WIFI_DATA_TYPE | BIT7)) {
						if (pstat->state & WIFI_SLEEP_STATE) {
							unsigned char *qosctrl = GetQosControl(txcfg->phdr);
							qosctrl[0] |= BIT4;
						}
					}
					pstat->apsd_trigger = 0;
				}
			}
#endif

			txcfg->tx_rate = get_tx_rate(priv, pstat);
#ifndef TX_LOWESTRATE
			txcfg->lowest_tx_rate = get_lowest_tx_rate(priv, pstat, txcfg->tx_rate);
#endif			
			// log tx statistics...
			tx_sum_up(priv, pstat, txcfg);

			// for SW LED
			priv->pshare->LED_tx_cnt++;

			if (
#if defined(RTK_AC_SUPPORT) || defined(CONFIG_WLAN_HAL_8192EE) //FOR_VHT5G_PF
			( ((pstat->aggre_mthd == AGGRE_MTHD_MPDU_AMSDU) || (pstat->aggre_mthd == AGGRE_MTHD_MPDU))
			&& txcfg->aggre_en ) ||
#endif
			((txcfg->aggre_en >= FG_AGGRE_MPDU) && (txcfg->aggre_en <= FG_AGGRE_MPDU_BUFFER_LAST))
			) {
				if (!pstat->ADDBA_ready[(int)skb->cb[1]]) {
					if ((pstat->ADDBA_req_num[(int)skb->cb[1]] < 5) && !pstat->ADDBA_sent[(int)skb->cb[1]]) {
						pstat->ADDBA_req_num[(int)skb->cb[1]]++;
						SMP_UNLOCK_XMIT(flags);
						issue_ADDBAreq(priv, pstat, (int)skb->cb[1]);
						SMP_LOCK_XMIT(flags);
						pstat->ADDBA_sent[(int)skb->cb[1]]++;
					}
				}
			}

			// check if we could re-use tx descriptor
			if (
				((get_desc(ptxsc_entry->hwdesc1.Dword7)&TX_TxBufSizeMask) > 0) &&
				(skb->len == (get_desc(ptxsc_entry->hwdesc1.Dword7)&TX_TxBufSizeMask)) &&
#if 1
                        ((GET_CHIP_VER(priv)== VERSION_8812E) ? 1 : (txcfg->tx_rate == ptxsc_entry->txcfg.tx_rate)) &&
#else
				(txcfg->tx_rate == ptxsc_entry->txcfg.tx_rate) &&
#endif	

				(pstat->protection == priv->pmib->dot11ErpInfo.protection) &&
				(pstat->ht_protection == priv->ht_protection)
#if 0 //defined(WIFI_WMM) && defined(WMM_APSD)
				&& (!(
#ifdef CLIENT_MODE
				(OPMODE & WIFI_AP_STATE) &&
#endif
				(APSD_ENABLE) && (pstat->state & WIFI_SLEEP_STATE)))
#endif
				) {

				pstat->tx_sc_pkts_lv2++;

				if (txcfg->privacy) {
					switch (txcfg->privacy) {
					case _WEP_104_PRIVACY_:
					case _WEP_40_PRIVACY_:
						wep_fill_iv(priv, txcfg->phdr, txcfg->hdr_len, ptxsc_entry->sc_keyid);
						break;

					case _TKIP_PRIVACY_:
						tkip_fill_encheader(priv, txcfg->phdr, txcfg->hdr_len, ptxsc_entry->sc_keyid);
						break;

					case _CCMP_PRIVACY_:
						aes_fill_encheader(priv, txcfg->phdr, txcfg->hdr_len, ptxsc_entry->sc_keyid);
						break;
					}
				}
#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
                rtl8192cd_signin_txdesc_shortcut_usb(priv, txcfg, ptxsc_entry);
#else
				rtl8192cd_signin_txdesc_shortcut(priv, txcfg, ptxsc_entry);
#endif
#if defined(SHORTCUT_STATISTIC) /*defined(__ECOS) && defined(_DEBUG_RTL8192CD_)*/
				priv->ext_stats.tx_cnt_sc2++;
#endif
				goto stop_proc;
			}

			pstat->tx_sc_pkts_lv1++;
#ifdef CONFIG_RTK_MESH
            if(txcfg->is_11s) {
                memcpy(&priv->ethhdr, &(ptxsc_entry->ethhdr), sizeof(struct wlan_ethhdr_t));
            }
#endif

#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
#if defined(__ECOS) && defined(TX_SCATTER)
			if (skb->key)
				rtl8192cd_signin_txdesc_usb(priv, txcfg, pethhdr); //TBD, ethhdr might be overwrite by WAPI
			else
				rtl8192cd_signin_txdesc_usb(priv, txcfg, NULL);
#else
			rtl8192cd_signin_txdesc_usb(priv, txcfg, NULL);
#endif
#else /* !defined(CONFIG_RTL_TRIBAND_SUPPORT) */
#if defined(__ECOS) && defined(TX_SCATTER)
			if (skb->key)
				rtl8192cd_signin_txdesc(priv, txcfg, pethhdr); //TBD, ethhdr might be overwrite by WAPI
			else
				rtl8192cd_signin_txdesc(priv, txcfg, NULL);
#else
			rtl8192cd_signin_txdesc(priv, txcfg, NULL);
#endif
#endif /* defined(CONFIG_RTL_TRIBAND_SUPPORT) */
#if defined(SHORTCUT_STATISTIC) /*defined(__ECOS) && defined(_DEBUG_RTL8192CD_)*/
			priv->ext_stats.tx_cnt_sc1++; 
#endif
			goto stop_proc;
	}
	if (!priv->pmib->dot11OperationEntry.disable_txsc && pstat)
		pstat->tx_sc_pkts_slow++;
#endif // TX_SHORTCUT
#if defined(CONFIG_RTK_MESH) || defined(A4_STA)
just_skip:
#endif

#if defined(SHORTCUT_STATISTIC) /*defined(__ECOS) && defined(_DEBUG_RTL8192CD_)*/
	priv->ext_stats.tx_cnt_nosc++;
#endif

	/* ==================== Slow path of packet TX process ==================== */
	SMP_UNLOCK_XMIT(flags);
	if (rtl8192cd_tx_slowPath(priv, skb, pstat, dev, wdsDev, txcfg) == TX_PROCEDURE_CTRL_STOP) {
		SMP_LOCK_XMIT(flags);
		goto stop_proc;
	}
	SMP_LOCK_XMIT(flags);

#ifdef __KERNEL__
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,13,0)
	dev->trans_start = jiffies;
#endif
#endif

	goto stop_proc;

free_and_stop:		/* Free current packet and stop TX process */

	rtl_kfree_skb(priv, skb, _SKB_TX_);

stop_proc:			/* Stop process and assume the TX-ed packet is already "processed" (freed or TXed) in previous code. */
	
	rtw_handle_xmit_fail(priv, txcfg);
	
	return 0;
}
#endif // CONFIG_USB_HCI || CONFIG_SDIO_HCI


/*-------------------------------------------------------------------------------
tx flow:
	Please refer to design spec for detail flow

rtl8192cd_firetx: check if hw desc is available for tx
				hdr_len, iv,icv, tx_rate info are available

signin_txdesc: fillin the desc and txpoll is necessary
--------------------------------------------------------------------------------*/
 


#ifdef CONFIG_PCI_HCI
#ifdef CONFIG_WLAN_HAL
#ifdef AP_SWPS_OFFLOAD
void SWPS_force_recycle_packet(
    unsigned long task_priv
)
{
#ifndef SMP_SYNC
    unsigned long flags;
#endif
    int recycleCnt,recycle_idx;
    int restart_queue = 0;
    
    struct rtl8192cd_priv   *priv = (struct rtl8192cd_priv *)task_priv;

    SAVE_INT_AND_CLI(flags);

    for(recycle_idx = 0; recycle_idx <4; recycle_idx++){
        if(priv->swps_recycle_queue[recycle_idx]){
            if (rtl8192cd_tx_recycle(priv, recycle_idx+1, &recycleCnt) == 1){//recycle idx=0 is MGT, so need +1
                restart_queue = 1;
            }
            priv->swps_recycle_queue[recycle_idx] = 0;
        }
    }

    if(restart_queue)
        rtl8192cd_tx_restartQueue(priv);

    priv->pshare->has_triggered_forcerecycle_tasklet = 0;
    
    RESTORE_INT(flags);

}
int SWPS_check_recycle_condition(
    struct rtl8192cd_priv   *priv, 
    unsigned short SWPS_sequence,
    unsigned short RPT_TxDone_SEQ,
    unsigned short RPT_reprepare_SEQ,
    unsigned short head, //the latest tx swseq
    unsigned short tail //the oldest tx swseq
)
{
    int result = 0; //1:recycle, 2: reprepare, 3:no action
#ifndef SMP_SYNC
    //unsigned long flags;
#endif

    //SAVE_INT_AND_CLI(flags); //already disable interrupt in rtl88xx_tx_dsr

    printk("head seq=%x, tail seq=%x, RPT_reprepare_SEQ=%x, RPT_TxDone_SEQ=%x\n",head,tail,RPT_reprepare_SEQ,RPT_TxDone_SEQ);
    if(head >= tail){
        if((SWPS_sequence <= RPT_TxDone_SEQ) && ((RPT_TxDone_SEQ-SWPS_sequence) < (MAX_SWPS_SEQ/2)))//to prevent get old report txdone, if not drop done when wakeup, will get old info
            result=1;  //RECYCLE
        else if((SWPS_sequence <= RPT_TxDone_SEQ) && ((RPT_TxDone_SEQ-SWPS_sequence) >= (MAX_SWPS_SEQ/2)))
            result=3;//STOP_RECYCLE //wait for new report
        else if(((RPT_reprepare_SEQ!=0) && (SWPS_sequence >= RPT_reprepare_SEQ)) && (RPT_reprepare_SEQ <=head))//to prevent get old RPT_reprepare_SEQ, if not drop done when wakeup, will get old info
            result=2; //REPREPARE
        else
            result=3;//STOP_RECYCLE
    }
    else if(tail > head){
        if((RPT_TxDone_SEQ <= head) && (RPT_TxDone_SEQ <= tail)){
            if(SWPS_sequence <= RPT_TxDone_SEQ)
                result = 1;
            else if((SWPS_sequence >= tail))
                result = 1;
            else if (((RPT_reprepare_SEQ!=0) && (SWPS_sequence >= RPT_reprepare_SEQ)) && (RPT_reprepare_SEQ <=head))//to prevent get old RPT_reprepare_SEQ,
                result = 2;
            else
                result = 3;
        }
        else if((RPT_TxDone_SEQ >= head) && (RPT_TxDone_SEQ >= tail)){
            if( (SWPS_sequence <=RPT_TxDone_SEQ) && (SWPS_sequence >= tail))
                result = 1;
            else if((RPT_reprepare_SEQ!=0) && (((SWPS_sequence >= RPT_reprepare_SEQ) && (RPT_reprepare_SEQ <=head )) || (SWPS_sequence <=head)))
                result = 2;
            else
                result = 3;
        }
        else
            result = 3;
    }
    else{
        //printk("[%s][%d]head == tail,wrong condition! \n",__FUNCTION__,__LINE__);
    }
    //RESTORE_INT(flags);
    printk("[%s] result=%x\n",__FUNCTION__,result);
    return result;
}

#endif



#endif // CONFIG_WLAN_HAL












#endif // CONFIG_PCI_HCI


 











