#define _8192CD_CORE_UTILS_C_

#ifdef __KERNEL__

#include <linux/version.h>

#include <linux/circ_buf.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
#include <linux/sched/signal.h>
#else
#include <linux/sched.h>
#endif
#include <linux/if_arp.h>
#include <net/ipv6.h>
#include <net/protocol.h>
#include <net/ndisc.h>
#include <linux/icmpv6.h>
#include <linux/vmalloc.h>


#elif defined(__ECOS)
#include <pkgconf/system.h>
#include <pkgconf/devs_eth_rltk_819x_wlan.h>
#include <cyg/io/eth/rltk/819x/wrapper/sys_support.h>
#include <cyg/io/eth/rltk/819x/wrapper/skbuff.h>
#include <cyg/io/eth/rltk/819x/wrapper/timer.h>
#include <cyg/io/eth/rltk/819x/wrapper/wrapper.h>
#endif

#ifdef __KERNEL__
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
#define ipv6_addr_copy(a1,a2) memcpy(a1, a2, sizeof(struct in6_addr))
#endif
#endif

#include "../8192cd_cfg.h"
#include "../8192cd.h"
#include "../8192cd_util.h"
#include "./8192cd_core_util.h"

#include "../8192cd_headers.h"
#include "../8192cd_debug.h"
#if defined(RTK_NL80211)
#include "../8192cd_cfg80211.h"
#endif
#ifdef CONFIG_FON
#include "fonmain.h"
#endif

#ifdef RTL8192CD_VARIABLE_USED_DMEM
#include "./8192cd_dmem.h"
#endif
#if defined(CONFIG_RTL_CUSTOM_PASSTHRU)
#ifdef __KERNEL__
#include <linux/if_vlan.h>
#endif
#endif
#if defined(CONFIG_RTL_FASTBRIDGE)
#include <net/rtl/features/fast_bridge.h>
#endif

#ifdef __OSK__
#include "ccb.h"
#endif

#if defined(CONFIG_RTL_SIMPLE_CONFIG)
#if !defined(__ECOS)
#include <linux/netdevice.h>
#endif
#include "./8192cd_profile.h"
extern unsigned char g_sc_ifname[32];
#endif
#ifdef CONFIG_DUAL_CPU
#include "dual_cpu.h"
#endif

#include "WlanHAL/HalPrecomp.h"



#ifdef DELAY_REFILL_RX_BUF
#ifdef CONFIG_WLAN_HAL
extern int refill_rx_ring_88XX(struct rtl8192cd_priv * priv, struct sk_buff * skb, unsigned char * data, unsigned int q_num, PHCI_RX_DMA_QUEUE_STRUCT_88XX cur_q);
#endif
extern int refill_rx_ring(struct rtl8192cd_priv *priv, struct sk_buff *skb, unsigned char *data);
#endif



unsigned char dot11_rate_table[] = {2,4,11,22,12,18,24,36,48,72,96,108,0}; // last element must be zero!!

#ifdef CONFIG_RECORD_CLIENT_HOST
void client_host_snooping_bydhcp(struct sk_buff *pskb, struct rtl8192cd_priv *priv)
{
#define DHCP_MAGIC 0x63825363

	struct iphdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
		__u8    ihl: 4,
			version: 4;
#elif defined (__BIG_ENDIAN_BITFIELD)
		__u8    version: 4,
			ihl: 4;
#else
#error  "Please fix <asm/byteorder.h>"
#endif
		__u8    tos;
		__u16   tot_len;
		__u16   id;
		__u16   frag_off;
		__u8    ttl;
		__u8    protocol;
#if 0
		__u16   check;
		__u32   saddr;
		__u32   daddr;
#endif
	};

	struct udphdr {
		__u16   source;
		__u16   dest;
		__u16   len;
		__u16   check;
	};

	struct dhcpMessage {
		u_int8_t op;
		u_int8_t htype;
		u_int8_t hlen;
		u_int8_t hops;
		u_int32_t xid;
		u_int16_t secs;
		u_int16_t flags;
		u_int32_t ciaddr;
		u_int32_t yiaddr;
		u_int32_t siaddr;
		u_int32_t giaddr;
		u_int8_t chaddr[16];
		u_int8_t sname[64];
		u_int8_t file[128];
		u_int32_t cookie;
#if 1
		u_int8_t options[308]; /* 312 - cookie */
#endif
	};

	struct stat_info *pstat;
	struct iphdr *iph;
	struct udphdr *udph;
	struct dhcpMessage *dhcph;
	struct list_head *phead, *plist;
	char find_host_name = 0;
	char find_host_ip = 0;
	iph = (struct iphdr *)(pskb->data + ETH_HLEN);
	udph = (struct udphdr *)((unsigned int)iph + (iph->ihl << 2));
	dhcph = (struct dhcpMessage *)((unsigned int)udph + sizeof(struct udphdr));

	phead = &priv->asoc_list;
	plist = phead->next;

	while (phead && (plist != phead)) {
		pstat = list_entry(plist, struct stat_info, asoc_list);
		plist = plist->next;

		if (isEqualMACAddr(pstat->cmn_info.mac_addr, &dhcph->chaddr[0]) && (dhcph->op == 1)) {//dhcp request
			int i, option_len, op_val_addr;
			i = 0;
			option_len = dhcph->options[1];
			op_val_addr = 2;

			if (dhcph->options[i] != 53 || dhcph->options[op_val_addr] != 3) /*message request type*/
				return;

			i = op_val_addr + option_len;
			while (dhcph->options[i] != 255) { /*DHCP end option*/
				option_len = dhcph->options[i + 1];
				op_val_addr = i + 2;

				if (dhcph->options[i] == 12) { /*Host name*/
					int hostnamelen = option_len > 255 ? 255 : option_len;
					find_host_name = 1;
					memcpy(pstat->client_host_name, dhcph->options + op_val_addr, hostnamelen);
					pstat->client_host_name[hostnamelen] = 0;
				} else if (dhcph->options[i] == 50) { /*Requested IP*/
					find_host_ip = 1;
					memcpy(pstat->client_host_ip, dhcph->options + op_val_addr, 4);
				}

				if (find_host_name && find_host_ip)
					return;

				i = op_val_addr + option_len;
			}

			if (find_host_ip && !find_host_name) {
				memcpy(pstat->client_host_name, &dhcph->chaddr[0], 6);
				pstat->client_host_name[6] = 0;
			}

			return;
		}
	}
}
#endif

#ifdef SW_TX_QUEUE
#define VW_2G_TURBO_SWQ_AGG_NUM 30	//VeriWave 2G setting
#define VW_5G_TURBO_SWQ_AGG_NUM 50	//VeriWave 5G setting
#define TRUBO_SWQ_AGG_NUM_2G 16		//normal setting
#define TRUBO_SWQ_AGG_NUM_5G 32		//normal setting


inline int get_turbo_swq_agg_num(struct rtl8192cd_priv *priv)
{
	int result;

	if (priv->pshare->rf_ft_var.manual_swqturboaggnum) {
		result = priv->pshare->rf_ft_var.manual_swqturboaggnum;
	} else {
		if (is_veriwave_testing(priv)) {
			if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_2G) {
				result = VW_2G_TURBO_SWQ_AGG_NUM;
			} else if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G) {
				result = VW_5G_TURBO_SWQ_AGG_NUM;
			} else {
				printk("error!! Unknown band \n");
				result = VW_5G_TURBO_SWQ_AGG_NUM;
			}
		} else {
			if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_2G) {
				result = TRUBO_SWQ_AGG_NUM_2G;
			} else if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G) {
				result = TRUBO_SWQ_AGG_NUM_5G;
			} else {
				printk("error!! Unknown band \n");
				result = TRUBO_SWQ_AGG_NUM_2G;
			} 
		}
	}

	return result;
}

void turbo_swq_setting(struct rtl8192cd_priv *priv)
{
    struct stat_info *pstat;
    int i, j;
	
    for(i= 0; i<NUM_STAT; i++) {
        if (priv->pshare->aidarray[i] && priv->pshare->aidarray[i]->used == TRUE) {
            pstat = &(priv->pshare->aidarray[i]->station);
            for(j=BK_QUEUE;j<HIGH_QUEUE;j++)
                pstat->swq.q_aggnum[j] = get_turbo_swq_agg_num(priv);
        }
    }
}
void adjust_swq_setting(struct rtl8192cd_priv *priv, struct stat_info *pstat, int i, int mode)
{
    int thd, step; 
    int maxAggNum;
	unsigned int swq_turbo_thd;

	swq_turbo_thd = get_swq_turbo_thd(priv);
	
    if(pstat->swq.q_used[i]) {
#if (MU_BEAMFORMING_SUPPORT == 1)
		if(pstat->muPartner_num)
			pstat->swq.q_aggnum[i] = priv->pshare->rf_ft_var.qlmt;
		else
#endif			
        if(priv->pshare->swq_use_hw_timer && priv->pshare->swq_numActiveSTA > swq_turbo_thd && 
           (priv->up_time - priv->pshare->swq_turbo_time) < priv->pshare->rf_ft_var.swqmaxturbotime) {
            pstat->swq.q_aggnum[i] = get_turbo_swq_agg_num(priv);
        }
        else if (mode == CHECK_DEC_AGGN) {
#if (MU_BEAMFORMING_SUPPORT == 1)
		if(pstat->swq.q_aggnum[i] == priv->pshare->rf_ft_var.qlmt)
	  		pstat->swq.q_aggnum[i] = priv->pshare->rf_ft_var.swq_aggnum;	  
#endif
            if(priv->pshare->swq_use_hw_timer) {
                if (pstat->swq.swq_prev_timeout[i] == 0)
                    return;
                thd = SWQ_TIMEOUT_THD / pstat->swq.swq_prev_timeout[i];    
                if(thd > 100)
                    thd = 100;
                else if (thd < 1)
                    thd = 1;
                if ((pstat->swq.q_TOCount[i] >= thd)&& ((pstat->swq.q_TOCount[i] % thd) == 0)) {                   
                    if (pstat->swq.q_aggnum[i] > 1) {
                        --(pstat->swq.q_aggnum[i]); 
                        pstat->swq.swq_keeptime[i] = priv->up_time + 3;
                    }
                    pstat->swq.swq_timeout_change[i] = 0;                    
                }                
            }
            else {
                if (pstat->swq.q_aggnum[i] <= 2)
                    thd = priv->pshare->rf_ft_var.timeout_thd;
                else if (pstat->swq.q_aggnum[i] <= 4)
                    thd = priv->pshare->rf_ft_var.timeout_thd2;
                else
                    thd = priv->pshare->rf_ft_var.timeout_thd3;

                if ((pstat->swq.q_TOCount[i] >= thd)&& ((pstat->swq.q_TOCount[i] % thd) == 0)) {
                    --(pstat->swq.q_aggnum[i]); 
                    if (pstat->swq.q_aggnum[i] <= 2)
                        pstat->swq.q_aggnum[i] = 2;
                    if (++pstat->swq.q_aggnumIncSlow[i] >= MAX_BACKOFF_CNT)
                        pstat->swq.q_aggnumIncSlow[i] = MAX_BACKOFF_CNT;
                    DEBUG_INFO("dec,aid:%d,cnt:%d\n", pstat->cmn_info.aid, pstat->swq.q_TOCount[i]);
                }
            }
        }
        else {
#if (MU_BEAMFORMING_SUPPORT == 1)
	   if(pstat->swq.q_aggnum[i] == priv->pshare->rf_ft_var.qlmt)
	  	 pstat->swq.q_aggnum[i] = priv->pshare->rf_ft_var.swq_aggnum;
#endif
            if(priv->pshare->swq_use_hw_timer) {
                if(priv->pshare->swq_numActiveSTA > swq_turbo_thd)
                    maxAggNum = get_turbo_swq_agg_num(priv);
                else
                    maxAggNum = priv->pshare->rf_ft_var.swq_aggnum;

                if(pstat->swq.q_aggnum[i] < maxAggNum && 
                   pstat->swq.q_TOCount[i] < SWQ_TIMEOUT_THD_LOWER) {
                    if(pstat->swq.swq_timeout_change[i]) {
                        step = (maxAggNum - pstat->swq.q_aggnum[i] + 1)/2;
                        if(step < 2)
                            step = 2;
                        pstat->swq.q_aggnum[i] += step;
                        if (pstat->swq.q_aggnum[i] > maxAggNum) {                    
                            pstat->swq.q_aggnum[i] = maxAggNum;
                        }        
                    }
                    else {
                        if(priv->up_time > pstat->swq.swq_keeptime[i])
                            pstat->swq.q_aggnum[i]++;                        
                    }
                }			
            }
            else {
                if (pstat->swq.q_aggnum[i] <= 2)
                    thd = priv->pshare->rf_ft_var.timeout_thd-10;
                else if (pstat->swq.q_aggnum[i] <= 4)
                    thd = priv->pshare->rf_ft_var.timeout_thd2-30;
                else
                    thd = priv->pshare->rf_ft_var.timeout_thd3-50;

                if(pstat->swq.q_TOCount[i]< thd) {
                    step = 1;
                if(pstat->swq.q_TOCount[i]< thd/5)
                    step = 5;
                else if(pstat->swq.q_TOCount[i]< thd/3)
                    step = 3;

                pstat->swq.q_aggnum[i] += step; 

                if (pstat->swq.q_aggnum[i] > priv->pshare->rf_ft_var.swq_aggnum)
                    pstat->swq.q_aggnum[i] = priv->pshare->rf_ft_var.swq_aggnum;
                    //panic_printk("inc,aid:%d,cnt:%d,%d\n", pstat->cmn_info.aid, pstat->swq.q_TOCount[i], pstat->swq.q_aggnum[BE_QUEUE]);
                }
            }
        }
    }
}

void init_STA_SWQAggNum(struct rtl8192cd_priv *priv)
{
    struct stat_info    *pstat;
    int i,j;
    unsigned long x = 0;
	
    for(i= 0; i<NUM_STAT; i++) {
        if (priv->pshare->aidarray[i] && priv->pshare->aidarray[i]->used == TRUE) {
            pstat = &(priv->pshare->aidarray[i]->station);
           SMP_LOCK_XMIT(x);
            for(j=BK_QUEUE;j<HIGH_QUEUE;j++) {
                if(priv->pshare->swq_numActiveSTA > 4)
                    pstat->swq.q_aggnum[j] = 16;
                if(pstat->tx_avarage> (1<<16)) {
                    pstat->swq.q_aggnum[j] = priv->pshare->rf_ft_var.swq_aggnum>>1;                               
                }
                if(priv->pshare->swq_use_hw_timer) {
                    pstat->swq.swq_en[j] = 1;
                    pstat->swq.swq_prev_timeout[j] = 1;
                    pstat->swq.swq_timeout_change[j] = 0;
                    pstat->swq.swq_keeptime[j] = 0;         
                }
            }
           SMP_UNLOCK_XMIT(x);
        }
    }
}
#endif //SW_TX_QUEUE



void eth_2_llc(struct wlan_ethhdr_t *pethhdr, struct llc_snap *pllc_snap)
{
	pllc_snap->llc_hdr.dsap=pllc_snap->llc_hdr.ssap=0xAA;
	pllc_snap->llc_hdr.ctl=0x03;

	if (p80211_stt_findproto(ntohs(pethhdr->type))) {
		memcpy((void *)pllc_snap->snap_hdr.oui, oui_8021h, WLAN_IEEE_OUI_LEN);
	}
	else {
		memcpy((void *)pllc_snap->snap_hdr.oui, oui_rfc1042, WLAN_IEEE_OUI_LEN);
	}
	pllc_snap->snap_hdr.type = pethhdr->type;
}


void eth2_2_wlanhdr(struct rtl8192cd_priv *priv, struct wlan_ethhdr_t *pethhdr, struct tx_insn *txcfg)
{
	unsigned char *pframe = txcfg->phdr;
	unsigned int to_fr_ds = get_tofr_ds(pframe);

	switch (to_fr_ds)
	{
		case 0x00:
			memcpy(GetAddr1Ptr(pframe), (const void *)pethhdr->daddr, WLAN_ADDR_LEN);
			memcpy(GetAddr2Ptr(pframe), (const void *)pethhdr->saddr, WLAN_ADDR_LEN);
			memcpy(GetAddr3Ptr(pframe), BSSID, WLAN_ADDR_LEN);
			break;
		case 0x01:
			{
#ifdef MCAST2UI_REFINE
                                if (txcfg->fr_type == _SKB_FRAME_TYPE_)
					memcpy(GetAddr1Ptr(pframe), (const void *) &((struct sk_buff *)txcfg->pframe)->cb[10], WLAN_ADDR_LEN);
                                else
#endif
				memcpy(GetAddr1Ptr(pframe), (const void *)pethhdr->daddr, WLAN_ADDR_LEN);
				memcpy(GetAddr2Ptr(pframe), BSSID, WLAN_ADDR_LEN);
				memcpy(GetAddr3Ptr(pframe), (const void *)pethhdr->saddr, WLAN_ADDR_LEN);
			}
			break;
		case 0x02:
			{
				memcpy(GetAddr1Ptr(pframe), BSSID, WLAN_ADDR_LEN);
				memcpy(GetAddr2Ptr(pframe), (const void *)pethhdr->saddr, WLAN_ADDR_LEN);
				memcpy(GetAddr3Ptr(pframe), (const void *)pethhdr->daddr, WLAN_ADDR_LEN);
			}
			break;
		case 0x03:

#ifdef CONFIG_RTK_MESH
            if(txcfg->is_11s) {
                memcpy(GetAddr1Ptr(pframe), txcfg->nhop_11s, WLAN_ADDR_LEN);
                memcpy(GetAddr2Ptr(pframe), GET_MY_HWADDR, WLAN_ADDR_LEN);
                memcpy(GetAddr3Ptr(pframe), (const void *)pethhdr->daddr, WLAN_ADDR_LEN);
                memcpy(GetAddr4Ptr(pframe), (const void *)pethhdr->saddr, WLAN_ADDR_LEN);
            } else
#endif // CONFIG_RTK_MESH
#ifdef A4_STA
            if (priv->pmib->miscEntry.a4_enable && txcfg->pstat && (txcfg->pstat->state & WIFI_A4_STA)) {
                memcpy(GetAddr1Ptr(pframe), txcfg->pstat->cmn_info.mac_addr, WLAN_ADDR_LEN);
                memcpy(GetAddr2Ptr(pframe), GET_MY_HWADDR, WLAN_ADDR_LEN);
								if(txcfg->aggre_en == FG_AGGRE_MSDU_FIRST) {
									memcpy(GetAddr3Ptr(pframe), (const void *)GET_MY_HWADDR, WLAN_ADDR_LEN);
									memcpy(GetAddr4Ptr(pframe), (const void *)GET_MY_HWADDR, WLAN_ADDR_LEN);
								} else {
						                	memcpy(GetAddr3Ptr(pframe), (const void *)pethhdr->daddr, WLAN_ADDR_LEN);
							                memcpy(GetAddr4Ptr(pframe), (const void *)pethhdr->saddr, WLAN_ADDR_LEN);
								}
            }
            else
#endif
            {            
#ifdef WDS
                #ifdef MP_TEST
    			if (OPMODE & WIFI_MP_STATE)
    				memcpy(GetAddr1Ptr(pframe), (const void *)pethhdr->daddr, WLAN_ADDR_LEN);
    			else
                #endif
    				memcpy(GetAddr1Ptr(pframe), priv->pmib->dot11WdsInfo.entry[txcfg->wdsIdx].macAddr, WLAN_ADDR_LEN);

                #ifdef MP_TEST
    			if (OPMODE & WIFI_MP_STATE)
    				memcpy(GetAddr2Ptr(pframe), priv->dev->dev_addr, WLAN_ADDR_LEN);
    			else
                #endif
                #ifdef __DRAYTEK_OS__
    				memcpy(GetAddr2Ptr(pframe), priv->dev->dev_addr, WLAN_ADDR_LEN);
                #else
    				memcpy(GetAddr2Ptr(pframe), priv->wds_dev[txcfg->wdsIdx]->dev_addr , WLAN_ADDR_LEN);
                #endif
    			memcpy(GetAddr3Ptr(pframe), (const void *)pethhdr->daddr, WLAN_ADDR_LEN);
    			memcpy(GetAddr4Ptr(pframe), (const void *)pethhdr->saddr, WLAN_ADDR_LEN);
#else // not WDS   			
    			DEBUG_ERR("no support for WDS!\n");
    			memcpy(GetAddr1Ptr(pframe), (const void *)pethhdr->daddr, WLAN_ADDR_LEN);
    			memcpy(GetAddr2Ptr(pframe), (const void *)BSSID, WLAN_ADDR_LEN);
    			memcpy(GetAddr3Ptr(pframe), (const void *)pethhdr->saddr, WLAN_ADDR_LEN);
#endif	// WDS
            }
			break;
	}
}


UINT8 get_rate_from_bit_value(int bit_val)
{
	int i;

	if (bit_val == 0)
		return 0;
	
#ifdef RTK_AC_SUPPORT 	//vht rate 
	if(bit_val & BIT(31)) {
		i = bit_val - BIT(31);

		if(i < VHT_RATE_NUM)
			return (VHT_RATE_ID + i);
		else
			return _NSS1_MCS0_RATE_; //unknown rate value 
	}
#endif

	if(bit_val & BIT(28)) {
		i = bit_val - BIT(28);

		if((i+16) < HT_RATE_NUM)
			return (_MCS16_RATE_ + i);
		else
			return _MCS0_RATE_; //unknown rate value 
	}
	
	i = 0;
	while ((bit_val & BIT(i)) == 0)
		i++;

	if (i < 12)
		return dot11_rate_table[i];
	else if (i < 28)
		return ((i - 12) + HT_RATE_ID);
	
	return 0;
}



unsigned int get_mcast_privacy(struct rtl8192cd_priv *priv, unsigned int *iv, unsigned int *icv,
				unsigned int *mic)
{
	unsigned int privacy;
	*iv  = 0;
	*icv = 0;
	*mic = 0;

	privacy = get_mcast_encrypt_algthm(priv);

	switch (privacy)
	{
#ifdef CONFIG_RTL_WAPI_SUPPORT
	case _WAPI_SMS4_:
		*iv = WAPI_PN_LEN+2;
		*icv = 0;
#if defined(CONFIG_RTL_HW_WAPI_SUPPORT)
#ifdef CONFIG_IEEE80211W
	if(!(UseSwCrypto(priv, NULL, TRUE, 0)))
#else
 	if(!(UseSwCrypto(priv, NULL, TRUE)))
#endif
		*mic = 0;	//HW will take care of the mic
	else
		*mic = SMS4_MIC_LEN;
#else
		*mic = SMS4_MIC_LEN;
#endif
		break;
#endif
	case _NO_PRIVACY_:
		*iv = 0;
		*icv = 0;
		*mic = 0;
		break;
	case _WEP_40_PRIVACY_:
	case _WEP_104_PRIVACY_:
		*iv = 4;
		*icv = 4;
		*mic = 0;
		break;
	case _TKIP_PRIVACY_:
		*iv = 8;
		*icv = 4;
		*mic = 0; // mic of TKIP is msdu based
		break;
	case _CCMP_PRIVACY_:
		*iv = 8;
		*icv = 0;
		*mic = 8;
		break;
	default:
		DEBUG_WARN("un-awared encrypted type %d\n", privacy);
		*iv = 0;
		*icv = 0;
		*mic = 0;
		break;
	}

	return privacy;
}



#ifdef RTK_AC_SUPPORT //for 11ac logo
int is_mixed_mode(struct rtl8192cd_priv *priv)
{
	if((priv->pmib->dot1180211AuthEntry.dot11EnablePSK == 3)
		&& (priv->pmib->dot1180211AuthEntry.dot11WPACipher & BIT(1))
		&& (priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher & BIT(1))
		&& (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == 2))
	{
		return 1;
	}
	else
		return 0;
}
#endif

int should_restrict_Nrate(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	if (OPMODE & WIFI_AP_STATE)
	{
		if (pstat->is_legacy_encrpt == 1) {
			if (priv->pmib->dot11nConfigEntry.dot11nLgyEncRstrct & BIT(1)) {
				if (!pstat->is_realtek_sta || (priv->pmib->dot11nConfigEntry.dot11nLgyEncRstrct & BIT(2)))
					return 1;
			}
		}
		else if (pstat->is_legacy_encrpt == 2) {
			if (priv->pmib->dot11nConfigEntry.dot11nLgyEncRstrct & BIT(0)) {
				if (!pstat->is_realtek_sta || (priv->pmib->dot11nConfigEntry.dot11nLgyEncRstrct & BIT(2)))
					return 1;
			}
		}

#ifdef RTK_AC_SUPPORT //for 11ac logo  // Cheat for mixed mode
		if (AC_SIGMA_MODE != AC_SIGMA_NONE) {
			if(is_mixed_mode(priv))
				return 1;
		}
#endif
		
#ifdef WDS
		else if (pstat->state & WIFI_WDS) {
			if ((priv->pmib->dot11WdsInfo.wdsPrivacy == _WEP_40_PRIVACY_) ||
				(priv->pmib->dot11WdsInfo.wdsPrivacy == _WEP_104_PRIVACY_) ||
				(priv->pmib->dot11WdsInfo.wdsPrivacy == _TKIP_PRIVACY_))
				return 1;
		}
#endif
	}
// Client mode IOT issue, Button 2009.07.17
#ifdef CLIENT_MODE
	else if(OPMODE & WIFI_STATION_STATE)
	{

		if(!pstat->is_realtek_sta && (pstat->IOTPeer != HT_IOT_PEER_MARVELL) && pstat->is_legacy_encrpt)


		return 1;
	}
#endif

	return 0;
}



// rateset: is the rateset for searching
// mode: 0: find the lowest rate, 1: find the highest rate
// isBasicRate: bit0-1: find from basic rate set, bit0-0: find from supported rate set. bit1-1: find CCK only
unsigned int find_rate(struct rtl8192cd_priv *priv, struct stat_info *pstat, int mode, int isBasicRate)
{
	unsigned int len, i, hirate, lowrate, rate_limit, OFDM_only=0;
	unsigned char *rateset, *p;
#ifdef CLIENT_MODE
	unsigned char totalrateset[32];
#endif

	if ((get_rf_mimo_mode(priv)== RF_1T2R) || (get_rf_mimo_mode(priv)== RF_1T1R)) //eric-8814 ?? 3t3r ??
		rate_limit = 8;
	else if (get_rf_mimo_mode(priv)== RF_2T2R)
		rate_limit = 16;
	else if (get_rf_mimo_mode(priv)== RF_3T3R)
		rate_limit = 24; 
	else if (get_rf_mimo_mode(priv)== RF_4T4R)
		rate_limit = 32;
	else
		rate_limit = 16;

	if (pstat) {
		rateset = pstat->bssrateset;
		len = pstat->bssratelen;
	} else {
#ifdef CLIENT_MODE
		if ((OPMODE & WIFI_STATION_STATE) && priv->pmib->dot11Bss.supportrate) {
			int i=0;
			len = 0;
			for (i=0; dot11_rate_table[i]; i++) {
				if (priv->pmib->dot11Bss.supportrate & BIT(i)) {
					totalrateset[len] = dot11_rate_table[i];
					if (priv->pmib->dot11Bss.basicrate & BIT(i))
						totalrateset[len] |= 0x80;
					len++;
				}
			}
			rateset = totalrateset;
		} else
#endif
		{
			rateset = AP_BSSRATE;
			len = AP_BSSRATE_LEN;
		}
	}

	hirate = _1M_RATE_;
	lowrate = _54M_RATE_;
	if (priv->pshare->curr_band == BAND_5G
#if defined(RTK_5G_SUPPORT) 
		|| priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G
#endif
		)
		OFDM_only = 1;

	for(i=0,p=rateset; i<len; i++,p++)
	{
		if (*p == 0x00)
			break;

		if ((isBasicRate & 1) && !(*p & 0x80))
			continue;

		if ((isBasicRate & 2) && !is_CCK_rate(*p & 0x7f))
			continue;

		if ((*p & 0x7f) > hirate)
			if (!OFDM_only || !is_CCK_rate(*p & 0x7f))
				hirate = (*p & 0x7f);

		if ((*p & 0x7f) < lowrate)
			if (!OFDM_only || !is_CCK_rate(*p & 0x7f))
				lowrate = (*p & 0x7f);
	}

	if (pstat) {
		if ((mode == 1) && (isBasicRate == 0) && (!should_restrict_Nrate(priv, pstat))) {
            if (pstat->ht_cap_len) {
                for (i=0; i<rate_limit; i++)
                {
                    if (pstat->ht_cap_buf.support_mcs[i/8] & BIT(i&0x7)) {
                        hirate = i;
                        hirate += HT_RATE_ID;
                    }
                }                
            }
#ifdef RTK_AC_SUPPORT
            if (pstat->vht_cap_len) {
                rate_limit = cpu_to_le32(pstat->vht_cap_buf.vht_support_mcs[0]);
                for (i=0; i<4; i++)
                {
                    if ((rate_limit & 0x3) == 0x3)
                        break;
                    else if ((i >= 2) && (pstat->tx_bw == CHANNEL_WIDTH_80_80))
                        break;
                    else
                        hirate = VHT_RATE_ID + 7 + (rate_limit & 0x3) + i*10;

                    rate_limit >>= 2;
                }
            }
#endif            
		}
	}
	else {
		if ((mode == 1) && (isBasicRate == 0)) {
            if (priv->ht_cap_len) {
    			for (i=0; i<rate_limit; i++)
    			{
    				if (priv->ht_cap_buf.support_mcs[i/8] & BIT(i%8)) {
    					hirate = i;
    					hirate += HT_RATE_ID;
    				}
    			}
            }
#ifdef RTK_AC_SUPPORT
            if (priv->vht_cap_len) {
                rate_limit = cpu_to_le32(priv->vht_cap_buf.vht_support_mcs[0]);
                for (i=0; i<4; i++)
                {
                    if ((rate_limit & 0x3) == 0x3)
                        break;
                    else if ((i >= 2) && (priv->pshare->CurrentChannelBW == CHANNEL_WIDTH_80_80))
                        break;
                    else
                        hirate = VHT_RATE_ID + 7 + (rate_limit & 0x3) + i*10;

                    rate_limit >>= 2;
                }
            }
#endif            
		}
	}

	if (mode == 0)
		return lowrate;
	else
		return hirate;
}



BOOLEAN CheckCts2SelfEnable(UINT8 rtsTxRate)
{
	return (rtsTxRate <= _11M_RATE_) ? 1 :0;
}

UINT8 find_rts_rate(struct rtl8192cd_priv *priv, UINT8 TxRate, BOOLEAN bErpProtect)
{
	UINT8 rtsTxRate = _6M_RATE_;

#if 0//def CONFIG_VERIWAVE_CHECK
	if((priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_2G) && (priv->pshare->rf_ft_var.rts_intf_mode || is_veriwave_testing(priv)))
	{
		rtsTxRate = _1M_RATE_;
	}
	else
#endif
	if(bErpProtect) // use CCK rate as RTS
	{
		rtsTxRate = _1M_RATE_;
	}
	else
	{
		switch (TxRate) 
		{
			case _NSS3_MCS9_RATE_:
			case _NSS3_MCS8_RATE_:
			case _NSS3_MCS7_RATE_:
			case _NSS3_MCS6_RATE_:
			case _NSS3_MCS5_RATE_:
			case _NSS3_MCS4_RATE_:
			case _NSS3_MCS3_RATE_:
			case _NSS2_MCS9_RATE_:
			case _NSS2_MCS8_RATE_:
			case _NSS2_MCS7_RATE_:
			case _NSS2_MCS6_RATE_:
			case _NSS2_MCS5_RATE_:
			case _NSS2_MCS4_RATE_:
			case _NSS2_MCS3_RATE_:
			case _NSS1_MCS9_RATE_:
			case _NSS1_MCS8_RATE_:
			case _NSS1_MCS7_RATE_:
			case _NSS1_MCS6_RATE_:
			case _NSS1_MCS5_RATE_:
			case _NSS1_MCS4_RATE_:
			case _NSS1_MCS3_RATE_:
			case _MCS23_RATE_:
			case _MCS22_RATE_:
			case _MCS21_RATE_:
			case _MCS20_RATE_:
			case _MCS19_RATE_:
			case _MCS15_RATE_:
			case _MCS14_RATE_:
			case _MCS13_RATE_:
			case _MCS12_RATE_:
			case _MCS11_RATE_:
			case _MCS7_RATE_:
			case _MCS6_RATE_:
			case _MCS5_RATE_:
			case _MCS4_RATE_:
			case _MCS3_RATE_:
			case _54M_RATE_:
			case _48M_RATE_:
			case _36M_RATE_:
			case _24M_RATE_:		
				rtsTxRate = _24M_RATE_;
				break;
			case _NSS3_MCS2_RATE_:
			case _NSS3_MCS1_RATE_:				
			case _NSS2_MCS2_RATE_:
			case _NSS2_MCS1_RATE_:
			case _NSS1_MCS2_RATE_:
			case _NSS1_MCS1_RATE_:
			case _MCS18_RATE_:
			case _MCS17_RATE_:				
			case _MCS10_RATE_:
			case _MCS9_RATE_:
			case _MCS2_RATE_:
			case _MCS1_RATE_:
			case _18M_RATE_:
			case _12M_RATE_:
				rtsTxRate = _12M_RATE_;
				break;
			case _NSS3_MCS0_RATE_:
			case _NSS2_MCS0_RATE_:
			case _NSS1_MCS0_RATE_:
			case _MCS16_RATE_:
			case _MCS8_RATE_:
			case _MCS0_RATE_:
			case _9M_RATE_:
			case _6M_RATE_:
				rtsTxRate = _6M_RATE_;
				break;
			case _11M_RATE_:
			case _5M_RATE_:
			case _2M_RATE_:
			case _1M_RATE_:
				rtsTxRate = _1M_RATE_;
				break;
			default:
				rtsTxRate = _6M_RATE_;
				break;
		}
	}

	if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G) {
	           if(rtsTxRate < _6M_RATE_)
	                     rtsTxRate = _6M_RATE_;
	}

	return rtsTxRate;

}




int get_rate_index_from_ieee_value(UINT8 val)
{
	int i;
	for (i=0; dot11_rate_table[i]; i++) {
		if (val == dot11_rate_table[i]) {
			return i;
		}
	}
	_DEBUG_ERR("Local error, invalid input rate for get_rate_index_from_ieee_value() [%d]!!\n", val);
	return 0;
}


__inline__ unsigned int orForce20_Switch20Map(struct rtl8192cd_priv *priv)
{
    return (orSTABitMap(&priv->force_20_sta) || orSTABitMap(&priv->switch_20_sta));
}


#ifdef RTK_QUE
void rtk_queue_init(struct ring_que *que)
{
	memset(que, '\0', sizeof(struct ring_que));
	que->qmax = MAX_PRE_ALLOC_SKB_NUM;
}

int rtk_queue_tail(struct rtl8192cd_priv *priv, struct ring_que *que, struct sk_buff *skb)
{
	int next;
	unsigned long x;

	SAVE_INT_AND_CLI(x);
	SMP_LOCK_SKB(x);

	if (que->head == que->qmax)
		next = 0;
	else
		next = que->head + 1;

	if (que->qlen >= que->qmax || next == que->tail) {
		printk("%s: ring-queue full!\n", __FUNCTION__);
		RESTORE_INT(x);
		SMP_UNLOCK_SKB(x);
		return 0;
	}

	que->ring[que->head] = skb;
	que->head = next;
	que->qlen++;

	RESTORE_INT(x);
	SMP_UNLOCK_SKB(x);
	return 1;
}


__IRAM_IN_865X
static struct sk_buff *rtk_dequeue(struct rtl8192cd_priv *priv, struct ring_que *que)
{
	struct sk_buff *skb;
	unsigned long x;

	SAVE_INT_AND_CLI(x);
	SMP_LOCK_SKB(x);

	if (que->qlen <= 0 || que->tail == que->head) {
		RESTORE_INT(x);
		SMP_UNLOCK_SKB(x);
		return NULL;
	}

	skb = que->ring[que->tail];

	if (que->tail == que->qmax)
		que->tail  = 0;
	else
		que->tail++;

	que->qlen--;

	RESTORE_INT(x);
	SMP_UNLOCK_SKB(x);
	return (struct sk_buff *)skb;
}


void free_rtk_queue(struct rtl8192cd_priv *priv, struct ring_que *skb_que)
{
	struct sk_buff *skb;

	while (skb_que->qlen > 0) {
		skb = rtk_dequeue(priv, skb_que);
		if (skb == NULL)
			break;
		dev_kfree_skb_any(skb);
	}
}
#endif // RTK_QUE



#if !(defined(__ECOS) && defined(CONFIG_SDIO_HCI))
void refill_skb_queue(struct rtl8192cd_priv *priv)
{
	struct sk_buff *skb;
#ifdef DELAY_REFILL_RX_BUF
 	struct rtl8192cd_hw *phw=GET_HW(priv);

#ifdef CONFIG_WLAN_HAL
    unsigned int                    q_num;
    PHCI_RX_DMA_MANAGER_88XX        prx_dma;
    PHCI_RX_DMA_QUEUE_STRUCT_88XX   cur_q;

    if (IS_HAL_CHIP(priv)) {
        q_num   = 0;
        prx_dma = (PHCI_RX_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PRxDMA88XX);
        cur_q   = &(prx_dma->rx_queue[q_num]);
    }
#endif // CONFIG_WLAN_HAL
#endif

#ifdef NOT_RTK_BSP
	while (skb_queue_len(&priv->pshare->skb_queue) < MAX_PRE_ALLOC_SKB_NUM) 
#else
	while (priv->pshare->skb_queue.qlen < MAX_PRE_ALLOC_SKB_NUM) 
#endif
	{

	#ifdef CONFIG_RTL8190_PRIV_SKB
			skb = dev_alloc_skb_priv(priv, RX_BUF_LEN);
	#else
			skb = dev_alloc_skb(RX_BUF_LEN);
	#endif

		if (skb == NULL) {
//			DEBUG_ERR("dev_alloc_skb() failed!\n");
			return;
		}
#ifdef DELAY_REFILL_RX_BUF
#ifdef CONFIG_WLAN_HAL
		if (IS_HAL_CHIP(priv)) {
	        if (cur_q->cur_host_idx != ((cur_q->host_idx + cur_q->rxbd_ok_cnt)%cur_q->total_rxbd_num)) {
	            refill_rx_ring_88XX(priv, skb, NULL, q_num, cur_q);
				continue;
		  	}
		} else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif // CONFIG_WLAN_HAL
		{//not HAL
			if (phw->cur_rx_refill != phw->cur_rx) {
				refill_rx_ring(priv, skb, NULL); 
				continue;
			}
		}
#endif

#ifdef RTK_QUE
		rtk_queue_tail(priv, &priv->pshare->skb_queue, skb);
#else
#ifdef __ECOS
		skb_queue_tail(&priv->pshare->skb_queue, skb);
#else
		__skb_queue_tail(&priv->pshare->skb_queue, skb);
#endif
#endif
	}
#ifdef DELAY_REFILL_RX_BUF
#ifdef CONFIG_WLAN_HAL
	if (IS_HAL_CHIP(priv)) {
        GET_HAL_INTERFACE(priv)->UpdateRXBDHostIdxHandler(priv, q_num, cur_q->rxbd_ok_cnt);
        cur_q->rxbd_ok_cnt = 0;
	}
#endif // CONFIG_WLAN_HAL
#endif 
}

#if (!defined(__OSK__)) || (defined(__OSK__) && !defined(CONFIG_RTL6028))
__MIPS16
#endif
__IRAM_IN_865X
struct sk_buff *alloc_skb_from_queue(struct rtl8192cd_priv *priv)
{
	struct sk_buff *skb=NULL;

#ifdef NOT_RTK_BSP
	if (skb_queue_len(&priv->pshare->skb_queue) < 2)
#else
	if (priv->pshare->skb_queue.qlen == 0) 
#endif
	{
//		struct sk_buff *skb;
#ifdef CONFIG_RTL8190_PRIV_SKB
#ifdef CONCURRENT_MODE
		skb = dev_alloc_skb_priv(priv, RX_BUF_LEN);
#else
		skb = dev_alloc_skb_priv(priv, RX_BUF_LEN);
#endif
#else
		skb = dev_alloc_skb(RX_BUF_LEN);
#endif
		if (skb == NULL) {
			DEBUG_INFO("dev_alloc_skb() failed!\n");
		}

		return skb;
	}
#ifdef RTK_QUE
	skb = rtk_dequeue(priv, &priv->pshare->skb_queue);
#else
#ifdef __ECOS
	skb = skb_dequeue(&priv->pshare->skb_queue);
#else
	skb = __skb_dequeue(&priv->pshare->skb_queue);
#endif
#endif
	if (skb == NULL) {
		DEBUG_ERR("skb_dequeue() failed!\n");
	}

	return skb;
}


void free_skb_queue(struct rtl8192cd_priv *priv, struct sk_buff_head	*skb_que)
{
	struct sk_buff *skb;
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	while (skb_que->qlen > 0) {
// 2009.09.08
		SAVE_INT_AND_CLI(flags);
		skb = (struct sk_buff *)__skb_dequeue(skb_que);
		RESTORE_INT(flags);
		if (skb == NULL)
			break;
		dev_kfree_skb_any(skb);
	}
}
#endif // !(__ECOS && CONFIG_SDIO_HCI)




void event_indicate(struct rtl8192cd_priv *priv, unsigned char *mac, int event)
{
#ifdef __KERNEL__
#ifdef USE_CHAR_DEV
	if (priv->pshare->chr_priv && priv->pshare->chr_priv->asoc_fasync)
		kill_fasync(&priv->pshare->chr_priv->asoc_fasync, SIGIO, POLL_IN);
#endif
#ifdef USE_PID_NOTIFY
	if (priv->pshare->wlanapp_pid > 0)
#ifdef LINUX_2_6_27_
	{
		kill_pid(_wlanapp_pid, SIGIO, 1);
	}
#else
		kill_proc(priv->pshare->wlanapp_pid, SIGIO, 1);
#endif
#endif
#endif

#ifdef __DRAYTEK_OS__
	if (event == 2)
		cb_disassoc_indicate(priv->dev, mac);
#endif

#ifdef GREEN_HILL
	extern void indicate_to_upper(int reason, unsigned char *addr);
	if (event > 0)
		indicate_to_upper(event, mac);
#endif

#ifdef __ECOS
#ifdef RTLPKG_DEVS_ETH_RLTK_819X_IWCONTROL
    extern cyg_flag_t iw_flag;
    cyg_flag_setbits(&iw_flag, 0x1);

#else
#ifdef RTLPKG_DEVS_ETH_RLTK_819X_WLAN_WPS
	extern cyg_flag_t wsc_flag;
	cyg_flag_setbits(&wsc_flag, 0x1);
#endif
#endif
#endif

#ifdef __OSK__
	extern void rtk_wlan_event(int event, void *);
	rtk_wlan_event(event, priv->dev);
#endif
}



#if (!defined(__OSK__)) || (defined(__OSK__) && !defined(CONFIG_RTL6028)) 
__MIPS16
#endif
__IRAM_IN_865X
unsigned char get_hdrlen(struct rtl8192cd_priv *priv, UINT8 *pframe)
{
    if (GetFrameType(pframe) == WIFI_DATA_TYPE)
    {
#ifdef CONFIG_RTK_MESH
		if ((get_tofr_ds(pframe) == 0x03) && ((GetFrameSubType(pframe) == WIFI_11S_MESH) || (GetFrameSubType(pframe) == WIFI_11S_MESH_ACTION))) {
			if(GetFrameSubType(pframe) == WIFI_11S_MESH) { /* DATA frame, qos might be on (TRUE on 8186) */
				return WLAN_HDR_A4_QOS_LEN;
			} /* WIFI_11S_MESH */
			else {/* WIFI_11S_MESH_ACTION frame, although qos flag is on, the qos field(2bytes) is not used for 8186 */
				if(is_mesh_6addr_format_without_qos(pframe)) {
					return WLAN_HDR_A6_MESH_DATA_LEN;
				} else {
					return WLAN_HDR_A4_MESH_DATA_LEN;
				}
			}
		} /* end of get_tofr_ds == 0x03 & (MESH DATA or MESH ACTION) */
		else
#endif // CONFIG_RTK_MESH
        if (is_qos_data(pframe)) {            
            if (get_tofr_ds(pframe) == 0x03) {
                if(GetOrder(pframe))                    
                    return WLAN_HDR_A4_QOS_HT_LEN;
                else
                    return WLAN_HDR_A4_QOS_LEN;
            }
            else {
                if(GetOrder(pframe))                    
                    return WLAN_HDR_A3_QOS_HT_LEN;
                else
                    return WLAN_HDR_A3_QOS_LEN;
            }             
        }
        else {
            if (get_tofr_ds(pframe) == 0x03)
                return WLAN_HDR_A4_LEN;
            else
                return WLAN_HDR_A3_LEN;
        }
    }
    else if (GetFrameType(pframe) == WIFI_MGT_TYPE) {
        if(GetOrder(pframe))
            return 	WLAN_HDR_A3_HT_LEN;
        else
            return 	WLAN_HDR_A3_LEN;
    }
    else if (GetFrameType(pframe) == WIFI_CTRL_TYPE)
    {
        if (GetFrameSubType(pframe) == WIFI_PSPOLL)
            return 16;
        else if (GetFrameSubType(pframe) == WIFI_BLOCKACK_REQ)
            return 16;
        else if (GetFrameSubType(pframe) == WIFI_BLOCKACK)
            return 16;
        else
        {
            DEBUG_INFO("unallowed control pkt type! 0x%04X\n", GetFrameSubType(pframe));
            return 0;
        }
    }
    else
    {
        DEBUG_INFO("unallowed pkt type! 0x%04X\n", GetFrameType(pframe));
        return 0;
    }
}


unsigned int get_privacy(struct rtl8192cd_priv *priv, struct stat_info *pstat,
				unsigned int *iv, unsigned int *icv, unsigned int *mic)
{
	unsigned int privacy;
	*iv = 0;
	*icv = 0;
	*mic = 0;

	privacy = get_sta_encrypt_algthm(priv, pstat);

	switch (privacy)
	{
#ifdef CONFIG_RTL_WAPI_SUPPORT
	case _WAPI_SMS4_:
		*iv = WAPI_PN_LEN+2;
		*icv = 0;
#if defined(CONFIG_RTL_HW_WAPI_SUPPORT)		
	#ifdef CONFIG_IEEE80211W
			if(!(UseSwCrypto(priv, pstat, (pstat ? FALSE : TRUE), 0)))
	#else
			if(!(UseSwCrypto(priv, pstat, (pstat ? FALSE : TRUE))))
	#endif
				*mic = 0;	//HW will take care of the mic
			else
				*mic = SMS4_MIC_LEN;
#else
		*mic = SMS4_MIC_LEN;
#endif
		break;
#endif
	case _NO_PRIVACY_:
		*iv  = 0;
		*icv = 0;
		*mic = 0;
		break;
	case _WEP_40_PRIVACY_:
	case _WEP_104_PRIVACY_:
		*iv = 4;
		*icv = 4;
		*mic = 0;
		break;
	case _TKIP_PRIVACY_:
		*iv = 8;
		*icv = 4;
		*mic = 0;	// mic of TKIP is msdu based
		break;
	case _CCMP_PRIVACY_:
		*iv = 8;
		*icv = 0;
		*mic = 8;
		break;
	default:
		DEBUG_WARN("un-awared encrypted type %d\n", privacy);
		*iv = *icv = *mic = 0;
		break;
	}

	return privacy;
}


unsigned char * get_da(unsigned char *pframe)
{
	unsigned char 	*da;
	unsigned int	to_fr_ds	= (GetToDs(pframe) << 1) | GetFrDs(pframe);

	switch (to_fr_ds) {
		case 0x00:	// ToDs=0, FromDs=0
			da = GetAddr1Ptr(pframe);
			break;
		case 0x01:	// ToDs=0, FromDs=1
			da = GetAddr1Ptr(pframe);
			break;
		case 0x02:	// ToDs=1, FromDs=0
			da = GetAddr3Ptr(pframe);
			break;
		default:	// ToDs=1, FromDs=1
			da = GetAddr3Ptr(pframe);
			break;
	}

	return da;
}


unsigned char * get_sa(unsigned char *pframe)
{
	unsigned char 	*sa;
	unsigned int	to_fr_ds	= (GetToDs(pframe) << 1) | GetFrDs(pframe);

	switch (to_fr_ds) {
		case 0x00:	// ToDs=0, FromDs=0
			sa = GetAddr2Ptr(pframe);
			break;
		case 0x01:	// ToDs=0, FromDs=1
			sa = GetAddr3Ptr(pframe);
			break;
		case 0x02:	// ToDs=1, FromDs=0
			sa = GetAddr2Ptr(pframe);
			break;
		default:	// ToDs=1, FromDs=1
			{
				unsigned char *qosCtrl=GetQosControl(pframe);

				if((qosCtrl[0] & BIT(7)))	//AMSDU, 8021.11ac Table 8-19 Address field
					sa = GetAddr2Ptr(pframe);
				else
					sa = GetAddr4Ptr(pframe);
			}
			break;
	}

	return sa;
}


int strip_amsdu_llc(struct rtl8192cd_priv *priv, struct sk_buff *skb, struct stat_info *pstat)
{
	INT		payload_length;
	INT		payload_offset;
	UINT8	daddr[WLAN_ETHADDR_LEN];
	UINT8	saddr[WLAN_ETHADDR_LEN];
	struct wlan_ethhdr_t	*e_hdr;
	struct wlan_llc_t		*e_llc;
	struct wlan_snap_t		*e_snap;
	int		pkt_format;

	memcpy(daddr, skb->data, MACADDRLEN);
	memcpy(saddr, skb->data+MACADDRLEN, MACADDRLEN);
	payload_length = skb->len - WLAN_ETHHDR_LEN;
	payload_offset = WLAN_ETHHDR_LEN;

	e_hdr = (struct wlan_ethhdr_t *) (skb->data + payload_offset);
	e_llc = (struct wlan_llc_t *) (skb->data + payload_offset);
	e_snap = (struct wlan_snap_t *) (skb->data + payload_offset + sizeof(struct wlan_llc_t));

	if ((e_llc->dsap==0xaa) && (e_llc->ssap==0xaa) && (e_llc->ctl==0x03))
	{
		if (!memcmp(e_snap->oui, oui_rfc1042, WLAN_IEEE_OUI_LEN)) {
			pkt_format = WLAN_PKT_FORMAT_SNAP_RFC1042;
			if(!memcmp(&e_snap->type, SNAP_ETH_TYPE_IPX, 2))
				pkt_format = WLAN_PKT_FORMAT_IPX_TYPE4;
			else if(!memcmp(&e_snap->type, SNAP_ETH_TYPE_APPLETALK_AARP, 2))
				pkt_format = WLAN_PKT_FORMAT_APPLETALK;
		}
		else if (!memcmp(e_snap->oui, SNAP_HDR_APPLETALK_DDP, WLAN_IEEE_OUI_LEN) &&
				 !memcmp(&e_snap->type, SNAP_ETH_TYPE_APPLETALK_DDP, 2))
			pkt_format = WLAN_PKT_FORMAT_APPLETALK;
		else if (!memcmp(e_snap->oui, oui_8021h, WLAN_IEEE_OUI_LEN))
			pkt_format = WLAN_PKT_FORMAT_SNAP_TUNNEL;
		else if (!memcmp(e_snap->oui, oui_cisco, WLAN_IEEE_OUI_LEN))
			pkt_format = WLAN_PKT_FORMAT_CDP;
		else {
			DEBUG_ERR("drop pkt due to invalid frame format!\n");
			return FAIL;
		}
	}
	else if ((memcmp(daddr, e_hdr->daddr, WLAN_ETHADDR_LEN) == 0) &&
			 (memcmp(saddr, e_hdr->saddr, WLAN_ETHADDR_LEN) == 0))
		pkt_format = WLAN_PKT_FORMAT_ENCAPSULATED;
	else
		pkt_format = WLAN_PKT_FORMAT_OTHERS;

	DEBUG_INFO("Convert 802.11 to 802.3 in format %d\n", pkt_format);

	if ((pkt_format == WLAN_PKT_FORMAT_SNAP_RFC1042) ||
		(pkt_format == WLAN_PKT_FORMAT_SNAP_TUNNEL) ||
		(pkt_format == WLAN_PKT_FORMAT_CDP)) {
		/* Test for an overlength frame */
		payload_length = payload_length - sizeof(struct wlan_llc_t) - sizeof(struct wlan_snap_t);

		if ((payload_length+WLAN_ETHHDR_LEN) > WLAN_MAX_ETHFRM_LEN) {
			/* A bogus length ethfrm has been sent. */
			/* Is someone trying an oflow attack? */
			DEBUG_WARN("SNAP frame too large (%d>%d)\n",
				(payload_length+WLAN_ETHHDR_LEN), WLAN_MAX_ETHFRM_LEN);
		}

		/* chop 802.11 header from skb. */
		skb_pull(skb, payload_offset);

		if ((pkt_format == WLAN_PKT_FORMAT_SNAP_RFC1042) ||
			(pkt_format == WLAN_PKT_FORMAT_SNAP_TUNNEL))
		{
			/* chop llc header from skb. */
			skb_pull(skb, sizeof(struct wlan_llc_t));

			/* chop snap header from skb. */
			skb_pull(skb, sizeof(struct wlan_snap_t));
		}

		/* create 802.3 header at beginning of skb. */
		e_hdr = (struct wlan_ethhdr_t *)skb_push(skb, WLAN_ETHHDR_LEN);
		if (pkt_format == WLAN_PKT_FORMAT_CDP)
			e_hdr->type = payload_length;
		else
			e_hdr->type = e_snap->type;
		memcpy((void *)e_hdr->daddr, daddr, WLAN_ETHADDR_LEN);
		memcpy((void *)e_hdr->saddr, saddr, WLAN_ETHADDR_LEN);

		/* chop off the 802.11 CRC */
		skb_trim(skb, payload_length + WLAN_ETHHDR_LEN);
	}
	else if ((pkt_format == WLAN_PKT_FORMAT_OTHERS) ||
			 (pkt_format == WLAN_PKT_FORMAT_APPLETALK) ||
			 (pkt_format == WLAN_PKT_FORMAT_IPX_TYPE4)) {

		/* Test for an overlength frame */
		if ( (payload_length + WLAN_ETHHDR_LEN) > WLAN_MAX_ETHFRM_LEN ) {
			/* A bogus length ethfrm has been sent. */
			/* Is someone trying an oflow attack? */
			DEBUG_WARN("IPX/AppleTalk frame too large (%d>%d)\n",
				(payload_length + WLAN_ETHHDR_LEN), WLAN_MAX_ETHFRM_LEN);
		}

		/* chop 802.11 header from skb. */
		skb_pull(skb, payload_offset);

		/* create 802.3 header at beginning of skb. */
		e_hdr = (struct wlan_ethhdr_t *)skb_push(skb, WLAN_ETHHDR_LEN);
		memcpy((void *)e_hdr->daddr, daddr, WLAN_ETHADDR_LEN);
		memcpy((void *)e_hdr->saddr, saddr, WLAN_ETHADDR_LEN);
		e_hdr->type = htons(payload_length);

		/* chop off the 802.11 CRC */
		skb_trim(skb, payload_length+WLAN_ETHHDR_LEN);
	}
	else if (pkt_format == WLAN_PKT_FORMAT_ENCAPSULATED) {

		if ( payload_length > WLAN_MAX_ETHFRM_LEN ) {
			/* A bogus length ethfrm has been sent. */
			/* Is someone trying an oflow attack? */
			DEBUG_WARN("Encapsulated frame too large (%d>%d)\n",
				payload_length, WLAN_MAX_ETHFRM_LEN);
		}

		/* Chop off the 802.11 header. */
		skb_pull(skb, payload_offset);

		/* chop off the 802.11 CRC */
		skb_trim(skb, payload_length);
	}

#if defined(__KERNEL__) || defined(__OSK__)
#ifdef LINUX_2_6_22_
	skb_reset_mac_header(skb);
#else
	skb->mac.raw = (unsigned char *) skb->data; /* new MAC header */
#endif
#endif

	return SUCCESS;
}


#ifdef CONFIG_RTL_OFFLOAD_DRIVER
void rtl_cfg_offload_enable(struct rtl8192cd_priv *priv)
{
	switch (GET_CHIP_VER(priv)) {
		case VERSION_8814B:
			priv->pshare->offload_en = 1;
			break;
		
		default:
			priv->pshare->offload_en = 0;
			break;
	}
}


void dc_if_update(struct rtl8192cd_priv *priv)
{
	if (!OFFLOAD_ENABLE(priv))
		return;

    //printk("[%s] priv->if_in_dc:%d \n", __func__, priv->if_in_dc);
    if (priv->if_in_dc == 0){//H2D cmd: add interface 
        rtl88XX_signin_h2d_cmd(priv, NULL, h2d_intf_add);
        priv->if_in_dc = 1;  // correct me: or wait d2h command??
    } else {//H2D cmd: update interface 
        rtl88XX_signin_h2d_cmd(priv, NULL, h2d_intf_update);
    }
}

void dc_if_remove(struct rtl8192cd_priv *priv)
{
	if (!OFFLOAD_ENABLE(priv))
		return;

    //printk("[%s] priv->if_in_dc:%d \n", __func__, priv->if_in_dc);
    if (priv->if_in_dc == 1) {
        rtl88XX_signin_h2d_cmd(priv, NULL, h2d_intf_remove);
        priv->if_in_dc = 0; // correct me: or wait d2h command??
    }
}


void dc_if_bcn_mode(struct rtl8192cd_priv *priv, int mode)
{
    unsigned char buf[20];
    
    if (priv->if_in_dc == 1) {
        mode = !!mode;
        sprintf(buf, "bcn,%d", mode);
        rtl88XX_signin_h2d_cmd(priv, buf, h2d_pe_debug);
    }
}


void dc_sta_update(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	if (!OFFLOAD_ENABLE(priv))
		return;
	//printk("[%s] pstat->sta_in_dc:%d \n", __func__, pstat->sta_in_dc);
    if(pstat->sta_in_dc == 0)
        return; /*not exists in data-CPU, just return*/

    //H2D cmd: update station     
    rtl88XX_signin_h2d_cmd(priv, pstat, h2d_sta_update);
}

void dc_sta_add(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	if (!OFFLOAD_ENABLE(priv))
		return;

	//printk("[%s] pstat->sta_in_dc:%d \n", __func__, pstat->sta_in_dc);
    if(pstat->sta_in_dc == 1) {
        dc_sta_update(priv,pstat);
        return; /*already exists, call update or just return*/
    }

    if(priv->pshare->dc_free_sta_space) {
        //H2D cmd: add station     
        rtl88XX_signin_h2d_cmd(priv, pstat, h2d_sta_add);

        pstat->sta_in_dc = 1; // correct me: or wait d2h command??
        priv->pshare->dc_free_sta_space --;
    } else { /* free space run out*/
        pstat->sta_in_dc = 0;
        panic_printk("%s %d Due to no free space in datacpu, dc_sta_add() fail!\n", __FUNCTION__, __LINE__);
    }
}

void dc_sta_remove(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	if (!OFFLOAD_ENABLE(priv))
		return;

	//printk("[%s] pstat->sta_in_dc:%d \n", __func__, pstat->sta_in_dc);
    if(pstat->sta_in_dc == 0)
        return; /*not exists in data-CPU, just return*/

    //H2D cmd: remove station    
    rtl88XX_signin_h2d_cmd(priv, pstat, h2d_sta_remove); 

    priv->pshare->dc_free_sta_space ++;
    pstat->sta_in_dc = 0; // correct me: or wait d2h command??
}


int realloc_dc_sta(struct rtl8192cd_priv *priv)
{
	struct stat_info *pstat =NULL, *pstat_chosen = NULL;
	int i=0,j=0;
	unsigned int max_through_put = 0, temp_through_put = 0;
	unsigned int have_chosen = 0;

	if (!OFFLOAD_ENABLE(priv))
		return;

    for(i= 0; i<NUM_STAT; i++) {
        if (priv->pshare->aidarray[i] && priv->pshare->aidarray[i]->used == TRUE) {
            pstat = &(priv->pshare->aidarray[i]->station);
            if (pstat->sta_in_dc == 1)
                continue;

            if (pstat->expire_to == 0 || pstat->leave == 1) //expired STA or leave station
                continue;
			    
            temp_through_put =  pstat->tx_avarage + pstat->rx_avarage;
            if (temp_through_put >= max_through_put){
                pstat_chosen = pstat;
                max_through_put = temp_through_put;
                have_chosen = 1;
                j = i;
            }   
        }
    }
	
	if (have_chosen == 0)
		return 0;

	dc_sta_add(priv->pshare->aidarray[j]->priv, pstat_chosen);

	return 1;
}
#endif


#ifdef __OSK__
__IRAM_WIFI_PRI5
#endif
#ifdef CONFIG_IEEE80211W
int UseSwCrypto(struct rtl8192cd_priv *priv, struct stat_info *pstat, int isMulticast, int isPMF)
#else
int UseSwCrypto(struct rtl8192cd_priv *priv, struct stat_info *pstat, int isMulticast)
#endif
{
#ifdef CONFIG_IEEE80211W
		if(isPMF)
			return 1;
#endif

	if (SWCRYPTO)
		return 1;
	else // hw crypto
	{
#ifdef CONFIG_RTK_MESH
	if(isMeshPoint(pstat))
		return 0;
		// return (pstat->dot11KeyMapping.keyInCam || isMulticast) ? 0 : 1;
#endif

#ifdef WDS
		if (pstat && (pstat->state & WIFI_WDS) && !(pstat->state & WIFI_ASOC_STATE)) {
			if (!pstat->dot11KeyMapping.keyInCam)
				return 1;
			else
				return 0;
		}
#endif

			if (priv->pmib->dot118021xAuthEntry.dot118021xAlgrthm 
#ifdef CONFIG_RTL_WAPI_SUPPORT
			|| priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _WAPI_SMS4_
#endif
				) {
				if (isMulticast) { // multicast
					if (!priv->pmib->dot11GroupKeysTable.keyInCam)
						return 1;
					else
						return 0;
				}
				else {
				if (!pstat->dot11KeyMapping.keyInCam)
					return 1;
				else // key is in CAM
					return 0;
				}
			}
			else { // legacy 802.11 auth (wep40 || wep104)
#ifdef MBSSID
				if (GET_ROOT(priv)->pmib->miscEntry.vap_enable)
				{
					if (GET_ROOT(priv)->pmib->dot11OperationEntry.opmode & WIFI_AP_STATE) {
						if (isMulticast)
							return 1;
						else {
						if (!pstat->dot11KeyMapping.keyInCam)
							return 1;
						else // key is in CAM
							return 0;
						}
					}
				}
#endif

#ifdef USE_WEP_DEFAULT_KEY
				if (GET_ROOT(priv)->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE)
                {
                    if (pstat && (pstat->state & WIFI_ASOC_STATE))
                        return 0;
                }

				if (isMulticast && 	!priv->pmib->dot11GroupKeysTable.keyInCam)
					return 1;
#else			
				if (isMulticast) {
					if (!priv->pmib->dot11GroupKeysTable.keyInCam)
						return 1;
				}
				else {
					if (!pstat->dot11KeyMapping.keyInCam)
						return 1;				
				}			
#endif			
				return 0;
			}
		}
	}



unsigned char *get_buf_from_poll(struct rtl8192cd_priv *priv, struct list_head *phead, unsigned int *count)
{
	unsigned char *buf;
	struct list_head *plist;
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	if(priv)
	SMP_LOCK_BUF(flags);

	if (list_empty(phead)) {
		if(priv)
		SMP_UNLOCK_BUF(flags);
//		_DEBUG_ERR("phead=%lX buf is empty now!\n", (unsigned long)phead);
		return NULL;
	}

	if (*count == 0) {
		if(priv)
		SMP_UNLOCK_BUF(flags);
		_DEBUG_ERR("phead=%lX under-run!\n", (unsigned long)phead);
		return NULL;
	}

	*count = *count - 1;
	plist = phead->next;
	list_del_init(plist);

	if(priv)
	SMP_UNLOCK_BUF(flags);
	
	buf = (UINT8 *)((unsigned long)plist + sizeof (struct list_head));
	return buf;
}

void release_buf_to_poll(struct rtl8192cd_priv *priv, unsigned char *pbuf, struct list_head	*phead, unsigned int *count)
{
	struct list_head *plist;
#ifdef SMP_SYNC
	unsigned long flags;
#endif

	if (pbuf == NULL)
	{
		DEBUG_ERR("Release Null Buf!\n");
		return;
	}

#if 0
	if (*count >= PRE_ALLOCATED_HDR) {
		_DEBUG_ERR("over size free buf phead=%lX, *count=%d\n", (unsigned long)phead, *count);
		return;
	}
#endif

	plist = (struct list_head *)((unsigned long)pbuf - sizeof(struct list_head));

	SMP_LOCK_BUF(flags);
	
	*count = *count + 1;
	list_add_tail(plist, phead);
	
	SMP_UNLOCK_BUF(flags);
}

unsigned char *get_wlanhdr_from_poll(struct rtl8192cd_priv *priv)
{
	unsigned char *pbuf;
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);

	pbuf = get_buf_from_poll(priv, &priv->pshare->wlan_hdrlist, (unsigned int *)&priv->pshare->pwlan_hdr_poll->count);
	if (pbuf) {
#ifdef TX_EARLY_MODE
		pbuf += 8;
#endif
#if defined(CONFIG_RTL_OFFLOAD_DRIVER)
		if (OFFLOAD_ENABLE(priv))
		    pbuf += sizeof(struct h2d_txpkt_info);
#endif
	}

	RESTORE_INT(flags);
	return pbuf;
}


void release_wlanhdr_to_poll(struct rtl8192cd_priv *priv, unsigned char *pbuf)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	if (pbuf == NULL)
	{
		DEBUG_ERR("Err: Free Null Buf!\n");
		return;
	}
	
	SAVE_INT_AND_CLI(flags);

#ifdef TX_EARLY_MODE
	pbuf -= 8;
#endif
#if defined(CONFIG_RTL_OFFLOAD_DRIVER)
	if (OFFLOAD_ENABLE(priv))
	    pbuf -= sizeof(struct h2d_txpkt_info);
#endif
	release_buf_to_poll(priv, pbuf, &priv->pshare->wlan_hdrlist, (unsigned int *)&priv->pshare->pwlan_hdr_poll->count);

	RESTORE_INT(flags);
}

//__MIPS16
__IRAM_IN_865X
unsigned char *get_wlanllchdr_from_poll(struct rtl8192cd_priv *priv)
{
	unsigned char *pbuf;
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);

	pbuf = get_buf_from_poll(priv, &priv->pshare->wlanllc_hdrlist, (unsigned int *)&priv->pshare->pwlanllc_hdr_poll->count);
	if (pbuf) {
#ifdef TX_EARLY_MODE
		pbuf += 8;
#endif
#if defined(CONFIG_RTL_OFFLOAD_DRIVER)
		if (OFFLOAD_ENABLE(priv))
		    pbuf += sizeof(struct h2d_txpkt_info);
#endif
	}

	RESTORE_INT(flags);
	return pbuf;
}

#ifdef __OSK__
__IRAM_WIFI_PRI5
#endif
void release_wlanllchdr_to_poll(struct rtl8192cd_priv *priv, unsigned char *pbuf)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);

#ifdef TX_EARLY_MODE
	pbuf -= 8;
#endif
#if defined(CONFIG_RTL_OFFLOAD_DRIVER)
	if (OFFLOAD_ENABLE(priv))
	    pbuf -= sizeof(struct h2d_txpkt_info);
#endif
	release_buf_to_poll(priv, pbuf, &priv->pshare->wlanllc_hdrlist, (unsigned int *)&priv->pshare->pwlanllc_hdr_poll->count);

	RESTORE_INT(flags);
}


unsigned char *get_icv_from_poll(struct rtl8192cd_priv *priv)
{
	unsigned char *ret;
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);

	ret = get_buf_from_poll(priv, &priv->pshare->wlanicv_list, (unsigned int *)&priv->pshare->pwlanicv_poll->count);

	RESTORE_INT(flags);
	return ret;
}

void release_icv_to_poll(struct rtl8192cd_priv *priv, unsigned char *pbuf)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);

	release_buf_to_poll(priv, pbuf, &priv->pshare->wlanicv_list, (unsigned int *)&priv->pshare->pwlanicv_poll->count);

	RESTORE_INT(flags);
}


unsigned char *get_mic_from_poll(struct rtl8192cd_priv *priv)
{
	unsigned char *ret;
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);

	ret = get_buf_from_poll(priv, &priv->pshare->wlanmic_list, (unsigned int *)&priv->pshare->pwlanmic_poll->count);

	RESTORE_INT(flags);
	return ret;
}


void release_mic_to_poll(struct rtl8192cd_priv *priv, unsigned char *pbuf)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);

	release_buf_to_poll(priv, pbuf, &priv->pshare->wlanmic_list, (unsigned int *)&priv->pshare->pwlanmic_poll->count);

	RESTORE_INT(flags);
}

unsigned char *get_mgtbuf_from_poll(struct rtl8192cd_priv *priv)
{
	unsigned char *ret;
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);
	
	ret = get_buf_from_poll(priv, &priv->pshare->wlanbuf_list, (unsigned int *)&priv->pshare->pwlanbuf_poll->count);

	RESTORE_INT(flags);
	return ret;
}


void release_mgtbuf_to_poll(struct rtl8192cd_priv *priv, unsigned char *pbuf)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);

	release_buf_to_poll(priv, pbuf, &priv->pshare->wlanbuf_list, (unsigned int *)&priv->pshare->pwlanbuf_poll->count);

	RESTORE_INT(flags);
}



unsigned int get_sta_encrypt_algthm(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	unsigned int privacy = 0;

#ifdef CONFIG_RTK_MESH
	if(isMeshPoint(pstat))
		return priv->pmib->dot11sKeysTable.dot11Privacy;
#endif

#ifdef CONFIG_RTL_WAPI_SUPPORT
	if (pstat&&pstat->wapiInfo&&pstat->wapiInfo->wapiType!=wapiDisable)
	{
		return _WAPI_SMS4_;
	}
	else
#endif
	{
	if (priv->pmib->dot118021xAuthEntry.dot118021xAlgrthm) {
		if (pstat)
			privacy = pstat->dot11KeyMapping.dot11Privacy;
		else
			DEBUG_ERR("pstat == NULL\n");
	}
		else
		{
		// legacy system
		privacy = priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm; //could be wep40 or wep104
	}
	}

	return privacy;
}


unsigned int isDHCPpkt(struct sk_buff *pskb)
{
#define DHCP_MAGIC 0x63825363

	struct iphdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	        __u8    ihl:4,
	                version:4;
#elif defined (__BIG_ENDIAN_BITFIELD)
	        __u8    version:4,
	                ihl:4;
#else
#error  "Please fix <asm/byteorder.h>"
#endif
	        __u8    tos;
	        __u16   tot_len;
	        __u16   id;
	        __u16   frag_off;
	        __u8    ttl;
	        __u8    protocol;
#if 0
	        __u16   check;
	        __u32   saddr;
	        __u32   daddr;
#endif
	};

	struct udphdr {
	        __u16   source;
	        __u16   dest;
	        __u16   len;
	        __u16   check;
	};

	struct dhcpMessage {
		u_int8_t op;
		u_int8_t htype;
		u_int8_t hlen;
		u_int8_t hops;
		u_int32_t xid;
		u_int16_t secs;
		u_int16_t flags;
		u_int32_t ciaddr;
		u_int32_t yiaddr;
		u_int32_t siaddr;
		u_int32_t giaddr;
		u_int8_t chaddr[16];
		u_int8_t sname[64];
		u_int8_t file[128];
		u_int32_t cookie;
#if 0
		u_int8_t options[308]; /* 312 - cookie */
#endif
	};

	unsigned short protocol = 0;
	struct iphdr* iph;
	struct udphdr *udph;
	struct dhcpMessage *dhcph;

	protocol = *((unsigned short *)(pskb->data + 2 * ETH_ALEN));

	if(protocol == __constant_htons(ETH_P_IP)) { /* IP */
		iph = (struct iphdr *)(pskb->data + ETH_HLEN);

		if(iph->protocol == 17) { /* UDP */
			udph = (struct udphdr *)((unsigned long)iph + (iph->ihl << 2));
			dhcph = (struct dhcpMessage *)((unsigned long)udph + sizeof(struct udphdr));

			if (((udph->source != __constant_htons(0x43)) || (udph->dest != __constant_htons(0x44))) 
				&& ((udph->source != __constant_htons(0x44)) || (udph->dest != __constant_htons(0x43))))
				return FALSE;

			if ((unsigned long)dhcph & 0x03) { //not 4-byte alignment
				u_int32_t cookie;
				char *pdhcphcookie;
				char *pcookie = (char *)&cookie;

				pdhcphcookie = (char *)&dhcph->cookie;
				pcookie[0] = pdhcphcookie[0];
				pcookie[1] = pdhcphcookie[1];
				pcookie[2] = pdhcphcookie[2];
				pcookie[3] = pdhcphcookie[3];
				if(cookie == htonl(DHCP_MAGIC))
					return TRUE;
			}
			else {
				if(dhcph->cookie == htonl(DHCP_MAGIC))
					return TRUE;
			}
		}
	}
#if !defined(__ECOS) && !defined(TAROKO_0)
	else if (protocol == __constant_htons(ETH_P_IPV6)) { /* IPv6 */
		struct ipv6hdr *ipv6h;
		ipv6h = (struct ipv6hdr *)(pskb->data + ETH_HLEN);

		if (ipv6h->nexthdr == 17) { /* UDP */
			udph = (struct udphdr *)((unsigned long)ipv6h + sizeof(struct ipv6hdr));

			if ((udph->source == __constant_htons(546) && udph->dest == __constant_htons(547))
				|| (udph->source == __constant_htons(547) && udph->dest == __constant_htons(546))) {
				return TRUE;
			}
		}
	}
#endif
	return FALSE;
}


/* any station allocated can be searched by hash list */
#ifdef CONFIG_WLAN_HAL_8197F
#ifdef __OSK__
__IRAM_WIFI_PRI1
#else
__MIPS16
__IRAM_IN_865X
#endif
struct stat_info *get_stainfo_hash(struct rtl8192cd_priv *priv, unsigned char *hwaddr)
{
	struct list_head	*phead, *plist;
	struct stat_info	*pstat;
	unsigned int	index;
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif

	SMP_LOCK_HASH_LIST(flags);

	index = wifi_mac_hash(hwaddr);
	phead = &priv->stat_hash[index];
	
	plist = phead->next;
	//check whether plist and phead is null
	while (plist && phead && plist != phead)
	{
		pstat = list_entry(plist, struct stat_info ,hash_list);
		plist = plist->next;
		
		if (isEqualMACAddr(pstat->cmn_info.mac_addr, hwaddr)) { // if found the matched address
#ifdef MULTI_MAC_CLONE
			if (!(priv->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE) ||
				((priv->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE) &&
										!priv->pmib->ethBrExtInfo.macclone_enable) ||
					((priv->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE) &&
						priv->pmib->ethBrExtInfo.macclone_enable & (pstat->mclone_id == ACTIVE_ID))) 
#endif		
			{
				priv->pstat_cache = pstat;
				goto exit;
			}
		}
#ifdef CONFIG_PCI_HCI
		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
			if (plist == plist->next)
				break;
		}
#endif
	}
	
	pstat = NULL;
	
exit:
	SMP_UNLOCK_HASH_LIST(flags);
	
	return pstat;
}

#if defined(__ECOS) && defined(WLAN_REG_FW_RAM_REFINE)
struct stat_info *get_stainfo(struct rtl8192cd_priv *priv, unsigned char *hwaddr)
{
	struct stat_info	*pstat;

#ifdef RTK_NL80211
	if(hwaddr == NULL)
		return (struct stat_info *)NULL;
#endif

	//if (!memcmp(hwaddr, priv->stainfo_cache.hwaddr, MACADDRLEN) &&  priv->stainfo_cache.pstat)
	pstat = priv->pstat_cache;

#ifdef MULTI_MAC_CLONE
	if ((priv->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE) && MCLONE_NUM > 0) {
	    if (pstat && !memcmp(hwaddr, pstat->hwaddr, MACADDRLEN) && pstat->mclone_id == ACTIVE_ID)
		return pstat;
	}
	else
#endif
	{
	    if(pstat && isEqualMACAddr(hwaddr, pstat->hwaddr))
			return pstat;
	}
	
	return get_stainfo_hash(priv, hwaddr);
}
#endif

#else
#ifdef __OSK__
__IRAM_WIFI_PRI1
#else
__MIPS16
__IRAM_IN_865X
#endif
struct stat_info *get_stainfo(struct rtl8192cd_priv *priv, unsigned char *hwaddr)
{
	struct list_head	*phead, *plist;
	struct stat_info	*pstat;
	unsigned int	index, idx;
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif

//#ifdef RTK_NL80211
	if(hwaddr == NULL){
		return (struct stat_info *)NULL;
		}
//#endif

	//if (!memcmp(hwaddr, priv->stainfo_cache.hwaddr, MACADDRLEN) &&  priv->stainfo_cache.pstat)
	pstat = priv->pstat_cache;

#ifdef MULTI_MAC_CLONE
	if ((priv->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE) && MCLONE_NUM > 0) {
	    if (pstat && !memcmp(hwaddr, pstat->cmn_info.mac_addr, MACADDRLEN) && pstat->mclone_id == ACTIVE_ID)
		return pstat;
	}
	else
#endif
	{
    if(pstat && isEqualMACAddr(hwaddr, pstat->cmn_info.mac_addr))
		return pstat;
	}

    /* fast comare with sta array */
#if !defined(TAROKO_0)
    idx = hwaddr[5];
	pstat = priv->sta_ary[idx].pstat;
#ifdef MULTI_MAC_CLONE
	if ((priv->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE) && MCLONE_NUM > 0) {
	    if (pstat && isEqualMACAddr(hwaddr, pstat->cmn_info.mac_addr) && pstat->mclone_id == ACTIVE_ID)
		return pstat;
	}
	else
#endif
	{
	    if(pstat && isEqualMACAddr(hwaddr, pstat->cmn_info.mac_addr))
			return pstat;
	}
#endif
	SMP_LOCK_HASH_LIST(flags);

	index = wifi_mac_hash(hwaddr);
	phead = &priv->stat_hash[index];
	
	plist = phead->next;
	while (plist && phead && plist != phead)
	{
		pstat = list_entry(plist, struct stat_info ,hash_list);
		plist = plist->next;
		
		if (!(memcmp((void *)pstat->cmn_info.mac_addr, (void *)hwaddr, MACADDRLEN))) { // if found the matched address
#ifdef MULTI_MAC_CLONE
			if (!(priv->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE) ||
				((priv->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE) &&
										!priv->pmib->ethBrExtInfo.macclone_enable) ||
					((priv->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE) &&
						priv->pmib->ethBrExtInfo.macclone_enable & (pstat->mclone_id == ACTIVE_ID))) 
#endif		
			{
				priv->pstat_cache = pstat;
				goto exit;
			}
		}
#ifdef CONFIG_PCI_HCI
		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
			if (plist == plist->next)
				break;
		}
#endif
	}
	
	pstat = NULL;
	
exit:
	SMP_UNLOCK_HASH_LIST(flags);
	
	return pstat;
}
#endif


#if 0	// Disable this function because it has heavy CPU load in while loop
/*
 * Use this function to get the number of associated station, no matter
 * it is expired or not. And don't count WDS peers in.
 */
int get_assoc_sta_num(struct rtl8192cd_priv *priv, int mode)
{
	int sta_num = priv->assoc_num;
#ifdef CONFIG_RTL8672
#ifdef CONFIG_RTK_MESH
	if((mode == 1) && priv->pmib->dot1180211sInfo.mesh_enable)
		sta_num -= (GET_MIB(priv)->dot1180211sInfo.mesh_max_neightbor - MESH_PEER_LINK_CAP_NUM(priv));
#endif

	if (sta_num < 0) {
	    panic_printk("%s: error sta_num:%d\n", __func__, sta_num);
	    sta_num = 0;
	}
	
#elif defined(NOT_RTK_BSP) && !defined(CONFIG_RTK_MESH)
#else
	struct list_head *phead, *plist;
	struct stat_info *pstat;

#if defined(SMP_SYNC) /* && (defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)) */
	unsigned long flags = 0;
	int locked = 0;
#endif

	sta_num = 0;
	phead = &priv->asoc_list;
	
#ifdef SMP_SYNC
	SMP_TRY_LOCK_ASOC_LIST(flags, locked);
#endif
	
	plist = phead->next;
	while (plist != phead) {
		pstat = list_entry(plist, struct stat_info, asoc_list);
		plist = plist->next;

#ifdef CONFIG_RTK_MESH        
		if(mode == 1) {
			if(isPossibleNeighbor(pstat))
			continue;
		}
#endif

		if ((pstat->state & WIFI_ASOC_STATE)
#ifdef NOT_RTK_BSP
			&& pstat->expire_to
#endif
			)
		{ 
			sta_num++;
		} 
	}
	
#ifdef SMP_SYNC
	if(locked)
		SMP_UNLOCK_ASOC_LIST(flags);
#endif

#endif
	return sta_num;
}
#endif


#if !(defined(CONFIG_WLAN_HAL_8197F)) || defined(CONFIG_WLAN_STATS_EXTENTION)
int IS_BSSID(struct rtl8192cd_priv *priv, unsigned char *da)
{
	unsigned char *bssid;
	bssid = priv->pmib->dot11StationConfigEntry.dot11Bssid;

	//if (!memcmp(da, bssid, 6))
	if (isEqualMACAddr(da, bssid))
		return TRUE;
	else
		return FALSE;
}



int IS_MCAST(unsigned char *da)
{
	if ((*da) & 0x01)
		return TRUE;
	else
		return FALSE;
}

int IS_BCAST2(unsigned char *da)
{
     if ((*da) == 0xff)
         return TRUE;
     else
         return FALSE;
}
#endif

