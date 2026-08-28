	#define _8192CD_CORE_TX_C_

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

#include "../8192cd_cfg.h"
#include "../8192cd.h"
#include "../8192cd_hw.h"
#include "../8192cd_headers.h"
#include "../8192cd_debug.h"
#include "../osdep_service.h"
#include "../WlanHAL/HalPrecomp.h"

#if !defined(__KERNEL__) && !defined(__ECOS)
#include "../sys-support.h"
#endif

#ifdef RTL8192CD_VARIABLE_USED_DMEM
#include "../8192cd_dmem.h"
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
#include "../romeperf.h"
#endif

#ifdef PERF_DUMP_1074K
#include "m1074kctrl.c"
#endif

#ifdef __OSK__
#include "ifport.h"
#endif

#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
#include "../8192cd_util.h"
#endif /* defined(CONFIG_RTL_TRIBAND_SUPPORT) */



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
void rtl8192cd_tx_restartQueue(struct rtl8192cd_priv *priv);
#endif

#ifdef TX_SCATTER
extern struct sk_buff *copy_skb(struct sk_buff *skb);
#endif



#define TX_NORMAL			0
#define TX_NO_MUL2UNI			1
#define TX_NO_TCPACK_ACC		6

#define TX_PROCEDURE_CTRL_STOP		0
#define TX_PROCEDURE_CTRL_CONTINUE	1
#define TX_PROCEDURE_CTRL_SUCCESS	2

#define RET_AGGRE_BYPASS		0
#define RET_AGGRE_ENQUE			1
#define RET_AGGRE_DESC_FULL		2

#define dz_queue(PRIV, PSTAT, PSKB) \
        priv->hci_ops->dz_queue(PRIV, PSTAT, PSKB)

#define BG_TABLE_SIZE 21
#define MCS_40M_TABLE_SIZE 18
#define MCS_20M_TABLE_SIZE 22
#define MAX_SUPPORT_ANT 3
	
unsigned short BG_TABLE[2][BG_TABLE_SIZE] = {{73,68,63,57,52,47,42,37,31,30,27,25,23,22,20,18,16,14,11,9,8},
				{108,108,108,108,108,108,108,108,108,108,108,108,72,72,48,48,36,12,11,11,4}};
unsigned short MCS_40M_TABLE[2][MCS_40M_TABLE_SIZE] = {{73,68,63,57,53,45,39,37,31,29,28,26,25,23,21,19,17,12},
					{7,7,7,7,7,7,7,7,5,4,4,4,2,2,2,0,0,0}};
unsigned short MCS_20M_TABLE[2][MCS_20M_TABLE_SIZE]={{73,68,65,60,55,50,45,40,31,32,27,26,24,22,21,19,18,16,14,11,9,8},
					{7,7,7,7,7,7,7,7,7,6,5,5,5,4,3,3,3,2,3,1,0,0}};

void wep_fill_iv(struct rtl8192cd_priv *priv,
				unsigned char *pwlhdr, unsigned int hdrlen, unsigned long keyid)
{
	unsigned char *iv = pwlhdr + hdrlen;
	union PN48 *ptsc48 = NULL;
	union PN48 auth_pn48;
	unsigned char *da;
	struct stat_info *pstat = NULL;

	memset(&auth_pn48, 0, sizeof(union PN48));
	da = get_da(pwlhdr);

#if defined(WDS) || defined(A4_STA)
	if (get_tofr_ds(pwlhdr) == 3)
		da = GetAddr1Ptr(pwlhdr);
#endif

	if (OPMODE & WIFI_AP_STATE)
	{
		if (IS_MCAST(da))
		{
			ptsc48 = GET_GROUP_ENCRYP_PN;
		}
		else
		{
			pstat = get_stainfo(priv, da);
			if (NULL == pstat) {
				DEBUG_ERR("no such station (da:%02x:%02x:%02x:%02x:%02x:%02x)\n",
					da[0],da[1],da[2],da[3],da[4],da[5]);
				return;
			}
			ptsc48 = GET_UNICAST_ENCRYP_PN;
		}
	}
#ifdef CLIENT_MODE
	else if (OPMODE & WIFI_STATION_STATE)
	{
		pstat = get_stainfo(priv, BSSID);
		if (pstat != NULL)
			ptsc48 = GET_UNICAST_ENCRYP_PN;
		else
			ptsc48 = &auth_pn48;
	}
	else if (OPMODE & WIFI_ADHOC_STATE)
		ptsc48 = GET_GROUP_ENCRYP_PN;
#endif

	if (ptsc48 == NULL)
	{
		DEBUG_ERR("no TSC for WEP due to ptsc48=NULL\n");
		return;
	}

	iv[0] = ptsc48->_byte_.TSC0;
	iv[1] = ptsc48->_byte_.TSC1;
	iv[2] = ptsc48->_byte_.TSC2;
	iv[3] = 0x0 | (keyid << 6);

	if (ptsc48->val48 == 0xffffffffffffULL)
		ptsc48->val48 = 0;
	else
		ptsc48->val48++;
}

void tkip_fill_encheader(struct rtl8192cd_priv *priv,
				unsigned char *pwlhdr, unsigned int hdrlen, unsigned long keyid_out)
{
	unsigned char *iv = pwlhdr + hdrlen;
	union PN48 *ptsc48 = NULL;
	unsigned int keyid = 0;
	unsigned char *da;
	struct stat_info *pstat = NULL;

	da = get_da(pwlhdr);

	if (OPMODE & WIFI_AP_STATE)
	{
#if defined(WDS) || defined(CONFIG_RTK_MESH) || defined(A4_STA)
		unsigned int to_fr_ds = (GetToDs(pwlhdr) << 1) | GetFrDs(pwlhdr);
		if (to_fr_ds == 3)
			da = GetAddr1Ptr(pwlhdr);
#endif

		if (IS_MCAST(da))
		{
			ptsc48 = GET_GROUP_ENCRYP_PN;
			keyid = priv->pmib->dot11GroupKeysTable.keyid;
		}
		else
		{
			pstat = get_stainfo(priv, da);
			if (NULL == pstat) {
				DEBUG_ERR("no such station (da:%02x:%02x:%02x:%02x:%02x:%02x)\n",
					da[0],da[1],da[2],da[3],da[4],da[5]);
				return;
			}
			ptsc48 = GET_UNICAST_ENCRYP_PN;
			keyid = 0;
		}
	}
#ifdef CLIENT_MODE
	else if (OPMODE & WIFI_STATION_STATE)
	{
		pstat = get_stainfo(priv, BSSID);
		if (NULL == pstat) {
			DEBUG_ERR("no such station\n");
			return;
		}
		ptsc48 = GET_UNICAST_ENCRYP_PN;
		keyid = 0;
	}
	else if (OPMODE & WIFI_ADHOC_STATE)
	{
		ptsc48 = GET_GROUP_ENCRYP_PN;
		keyid = 0;
	}
#endif

#ifdef __DRAYTEK_OS__
	keyid = keyid_out;
#endif

	if (ptsc48 == NULL)
	{
		DEBUG_ERR("no TSC for TKIP due to ptsc48=NULL\n");
		return;
	}

	iv[0] = ptsc48->_byte_.TSC1;
	iv[1] = (iv[0] | 0x20) & 0x7f;
	iv[2] = ptsc48->_byte_.TSC0;
	iv[3] = 0x20 | (keyid << 6);
	iv[4] = ptsc48->_byte_.TSC2;
	iv[5] = ptsc48->_byte_.TSC3;
	iv[6] = ptsc48->_byte_.TSC4;
	iv[7] = ptsc48->_byte_.TSC5;

	if (ptsc48->val48 == 0xffffffffffffULL)
		ptsc48->val48 = 0;
	else
		ptsc48->val48++;
}


#if defined(SUPPORT_TX_AMSDU) || defined (P2P_SUPPORT)
__IRAM_IN_865X void rtl8192cd_amsdu_timeout(unsigned long task_priv)
{
	struct rtl8192cd_priv        *priv = (struct rtl8192cd_priv *)task_priv;
	unsigned long flags;
	SMP_LOCK_XMIT(flags);
	amsdu_timeout(priv, jiffies);
	SMP_UNLOCK_XMIT(flags);
	priv->pshare->amsdu_cnt_timeout++;
}
#endif


#if (!defined(__OSK__)) || (defined(__OSK__) && !defined(CONFIG_RTL6028))
__MIPS16
#endif
#ifndef WIFI_MIN_IMEM_USAGE
__IRAM_IN_865X
#endif
void aes_fill_encheader(struct rtl8192cd_priv *priv,
				unsigned char *pwlhdr, unsigned int hdrlen, unsigned long keyid)
{
	unsigned char *da;
	struct stat_info *pstat = NULL;
	union PN48 *pn48 = NULL;

#ifdef __DRAYTEK_OS__
	int	keyid_input = keyid;
#endif

	da = get_da(pwlhdr);

	if (OPMODE & WIFI_AP_STATE)
	{
#if defined(WDS) || defined(CONFIG_RTK_MESH) || defined(A4_STA)
		unsigned int to_fr_ds = (GetToDs(pwlhdr) << 1) | GetFrDs(pwlhdr);
		if (to_fr_ds == 3)
			da = GetAddr1Ptr(pwlhdr);
#endif


		if (IS_MCAST(da))
		{
			pn48 = GET_GROUP_ENCRYP_PN;
			keyid = priv->pmib->dot11GroupKeysTable.keyid;
		}
		else
		{
			pstat = get_stainfo(priv, da);
			if(NULL == pstat) {
				DEBUG_ERR("no such station (da:%02x:%02x:%02x:%02x:%02x:%02x)\n",
					da[0],da[1],da[2],da[3],da[4],da[5]);
				return;
			}
			pn48 = GET_UNICAST_ENCRYP_PN;
			keyid = 0;
		}
	}
#ifdef CLIENT_MODE
	else if (OPMODE & WIFI_STATION_STATE)
	{
		pstat = get_stainfo(priv, BSSID);
		if(NULL == pstat) {
			DEBUG_ERR("no such station\n");
			return;
		}
		pn48 = GET_UNICAST_ENCRYP_PN;
		keyid = 0;
	}
	else if (OPMODE & WIFI_ADHOC_STATE)
	{
		pn48 = GET_GROUP_ENCRYP_PN;
		keyid = 0;
	}
#endif

	if (pn48 == NULL)
	{
		DEBUG_ERR("no TSC for AES due to pn48=NULL\n");
		return;
	}

#ifdef __DRAYTEK_OS__
	keyid = keyid_input;
#endif

	pwlhdr[hdrlen]   = pn48->_byte_.TSC0;
	pwlhdr[hdrlen+1] = pn48->_byte_.TSC1;
	pwlhdr[hdrlen+2] =  0x00;
	pwlhdr[hdrlen+3] = (0x20 | (keyid << 6));
	pwlhdr[hdrlen+4] = pn48->_byte_.TSC2;
	pwlhdr[hdrlen+5] = pn48->_byte_.TSC3;
	pwlhdr[hdrlen+6] = pn48->_byte_.TSC4;
	pwlhdr[hdrlen+7] = pn48->_byte_.TSC5;

   	if (pn48->val48 == 0xffffffffffffULL)
		pn48->val48 = 0;
	else
		pn48->val48++;
}


unsigned int get_tx_rate(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
    if (!pstat->cmn_info.ra_info.disable_ra) {
#if defined(CONFIG_RTL_92D_SUPPORT) || defined(CONFIG_RTL_92C_SUPPORT)
		if (CHIP_VER_92X_SERIES(priv) && (priv->pmib->dot11RFEntry.txbf == 1))
			check_txrate_by_reg(priv, pstat);
#endif		
		return pstat->current_tx_rate;
	} else {
			// firmware does not keep the aid ...
			//use default rate instead
			// eric_8814 ?? VHT rates ?? 1SS ?? 2SS ?? 3SS ??
			if (pstat->ht_cap_len) {	// is N client
				if (pstat->tx_bw == CHANNEL_WIDTH_40) {//(pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SUPPORT_CH_WDTH_))){ //40Mhz
					int i = 0;
					pstat->current_tx_rate = MCS_40M_TABLE[1][MCS_40M_TABLE_SIZE-1] + HT_RATE_ID;
					for (i=1; i < MCS_40M_TABLE_SIZE; i++) {
						if (pstat->rssi > MCS_40M_TABLE[0][i]){
							pstat->current_tx_rate = MCS_40M_TABLE[1][i-1] + HT_RATE_ID;
							break;
						}
					}
					return pstat->current_tx_rate;
				} else { // 20Mhz
					int i = 0;
					pstat->current_tx_rate = MCS_20M_TABLE[1][MCS_20M_TABLE_SIZE-1] + HT_RATE_ID;
					for (i=1; i < MCS_20M_TABLE_SIZE; i++) {
						if (pstat->rssi > MCS_20M_TABLE[0][i]){
							pstat->current_tx_rate = MCS_20M_TABLE[1][i-1] + HT_RATE_ID;
							break;
						}
					}
					return pstat->current_tx_rate;
				}
			} 
#ifdef RTK_AC_SUPPORT
			else if (pstat->vht_cap_len) {	// is N client
				if (pstat->tx_bw == CHANNEL_WIDTH_80) {//(pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SUPPORT_CH_WDTH_))){ //40Mhz
					int i = 0;
					pstat->current_tx_rate = MCS_40M_TABLE[1][MCS_40M_TABLE_SIZE-1] + VHT_RATE_ID;
					for (i=1; i < MCS_40M_TABLE_SIZE; i++) {
						if (pstat->rssi > MCS_40M_TABLE[0][i]){
							pstat->current_tx_rate = MCS_40M_TABLE[1][i-1] + VHT_RATE_ID;
							break;
						}
					}
					return pstat->current_tx_rate;
				} else { // 20Mhz
					int i = 0;
					pstat->current_tx_rate = MCS_20M_TABLE[1][MCS_20M_TABLE_SIZE-1] + VHT_RATE_ID;
					for (i=1; i < MCS_20M_TABLE_SIZE; i++) {
						if (pstat->rssi > MCS_20M_TABLE[0][i]){
							pstat->current_tx_rate = MCS_20M_TABLE[1][i-1] + VHT_RATE_ID;
							break;
						}
					}
					return pstat->current_tx_rate;
				}
				return pstat->current_tx_rate;

			} 
#endif
			else { // is BG client
				int i = 0;
				pstat->current_tx_rate = BG_TABLE[1][BG_TABLE_SIZE-1]; // eric_8814 ?? conflict with HT RATES ??
				for (i = 0; i < BG_TABLE_SIZE; i++) {
					if (pstat->rssi > BG_TABLE[0][i]){
						pstat->current_tx_rate = BG_TABLE[1][i-1];
						break;
				    }
				}
				return pstat->current_tx_rate;
			}

	}

}


static int tkip_mic_padding(struct rtl8192cd_priv *priv,
				unsigned char *da, unsigned char *sa, unsigned char priority,
				unsigned char *llc, struct sk_buff *pskb, struct tx_insn* txcfg)
{
	// now check what's the mic key we should apply...

	unsigned char	*mickey = NULL;
	unsigned int	keylen = 0;
	struct stat_info	*pstat = NULL;
	unsigned char	*hdr, hdr_buf[16];
	unsigned int	num_blocks;
	unsigned char	tkipmic[8];
	unsigned char	*pbuf=pskb->data;
	unsigned int	len=pskb->len;
	unsigned int	llc_len = 0;

	// check if the mic/tkip key is valid at this moment.

	if (((txcfg->aggre_en < FG_AGGRE_MSDU_FIRST) || (txcfg->aggre_en == FG_AGGRE_MSDU_LAST)) && (pskb->tail + 8) > (pskb->end))
	{
		DEBUG_ERR("pskb have no extra room for TKIP Michael padding\n");
		return FALSE;
	}

#ifdef WDS
	if (txcfg->wdsIdx >= 0) {
		pstat = get_stainfo(priv, priv->pmib->dot11WdsInfo.entry[txcfg->wdsIdx].macAddr);
		if (pstat) {
			keylen = GET_UNICAST_MIC_KEYLEN;
			mickey = GET_UNICAST_TKIP_MIC1_KEY;
		}
	}
	else
#endif
	if (OPMODE & WIFI_AP_STATE)
	{

#ifdef CONFIG_RTK_MESH
		if(txcfg->is_11s)
			da = txcfg->nhop_11s;
#endif
		if (IS_MCAST(da))
		{
			if(priv->pmib->dot11GroupKeysTable.keyid == GKEY_ID_SECOND) {
				keylen = GET_GROUP_IDX2_MIC_KEYLEN;
				mickey = GET_GROUP_TKIP_IDX2_MIC1_KEY;
			} else {
				keylen = GET_GROUP_MIC_KEYLEN;
				mickey = GET_GROUP_TKIP_MIC1_KEY;
			}
#ifdef A4_STA			
			if (txcfg->pstat && (txcfg->pstat->state & WIFI_A4_STA)) {
				pstat = txcfg->pstat;
				keylen = GET_UNICAST_MIC_KEYLEN;
				mickey = GET_UNICAST_TKIP_MIC1_KEY;
			}
#endif			
		}
		else
		{
			pstat = get_stainfo (priv, da);
#ifdef A4_STA			
			if (priv->pmib->miscEntry.a4_enable && (pstat == NULL)) 
				pstat = a4_sta_lookup(priv, da);
#endif		
			if (pstat == NULL) {
				DEBUG_ERR("tx mic pstat == NULL\n");
				return FALSE;
			}

			keylen = GET_UNICAST_MIC_KEYLEN;
			mickey = GET_UNICAST_TKIP_MIC1_KEY;
		}
	}
#ifdef CLIENT_MODE
	else if (OPMODE & WIFI_STATION_STATE)
	{
		pstat = get_stainfo (priv, BSSID);
		if (pstat == NULL) {
			DEBUG_ERR("tx mic pstat == NULL\n");
			return FALSE;
		}

		keylen = GET_UNICAST_MIC_KEYLEN;
		mickey = GET_UNICAST_TKIP_MIC2_KEY;
	}
	else if (OPMODE & WIFI_ADHOC_STATE)
	{
		keylen = GET_GROUP_MIC_KEYLEN;
		mickey = GET_GROUP_TKIP_MIC1_KEY;
	}
#endif

	if ((txcfg->aggre_en == FG_AGGRE_MSDU_MIDDLE) || (txcfg->aggre_en == FG_AGGRE_MSDU_LAST))
		mickey = pstat->tmp_mic_key;

	if (keylen == 0)
	{
		DEBUG_ERR("no mic padding for TKIP due to keylen=0\n");
		return FALSE;
	}

	if (txcfg->aggre_en <= FG_AGGRE_MSDU_FIRST) {
		hdr = hdr_buf;
		memcpy((void *)hdr, (void *)da, WLAN_ADDR_LEN);
		memcpy((void *)(hdr + WLAN_ADDR_LEN), (void *)sa, WLAN_ADDR_LEN);
		hdr[12] = priority;
		hdr[13] = hdr[14] = hdr[15] = 0;
	}
	else
		hdr = NULL;

	pbuf[len] = 0x5a;   /* Insert padding */
	pbuf[len+1] = 0x00;
	pbuf[len+2] = 0x00;
	pbuf[len+3] = 0x00;
	pbuf[len+4] = 0x00;
	pbuf[len+5] = 0x00;
	pbuf[len+6] = 0x00;
	pbuf[len+7] = 0x00;

	if (llc)
		llc_len = 8;
	num_blocks = (16 + llc_len + len + 5) / 4;
	if ((16 + llc_len + len + 5) & (4-1))
		num_blocks++;

	if (txcfg->aggre_en >= FG_AGGRE_MSDU_FIRST) {
		if (txcfg->aggre_en == FG_AGGRE_MSDU_FIRST) {
			num_blocks = (16 + len) / 4;
			if ((16 + len) & (4-1))
				num_blocks++;
		}
		else if (txcfg->aggre_en == FG_AGGRE_MSDU_MIDDLE) {
			num_blocks = len / 4;
			if (len & (4-1))
				num_blocks++;
		}
		else if (txcfg->aggre_en == FG_AGGRE_MSDU_LAST) {
			num_blocks = (len + 5) / 4;
			if ((len + 5) & (4-1))
				num_blocks++;
		}
	}

	michael(priv, mickey, hdr, llc, pbuf, (num_blocks << 2), tkipmic, 1);

	//tkip mic is MSDU-based, before filled-in descriptor, already finished.
#ifdef SUPPORT_TKIP_OSK_GDMA
	//always get tkipmic in michael function
#else
	if (!(priv->pshare->have_hw_mic) ||
		(priv->pmib->dot11StationConfigEntry.swTkipMic))
#endif
	{
#ifdef MICERR_TEST
		if (priv->micerr_flag) {
			tkipmic[7] ^= tkipmic[7];
			priv->micerr_flag = 0;
		}
#endif
		if ((txcfg->aggre_en == FG_AGGRE_MSDU_FIRST) || (txcfg->aggre_en == FG_AGGRE_MSDU_MIDDLE)) {
			memcpy((void *)pstat->tmp_mic_key, (void *)tkipmic, 8);
		}
		else
			memcpy((void *)(pbuf + len), (void *)tkipmic, 8);
	}

	return TRUE;
}



/*
	Always STOP process after calling this Procedure.
*/
#ifdef CONFIG_RTL_KERNEL_MIPS16_WLAN
__NOMIPS16
#endif
/*static*/ void rtl8192cd_tx_xmitSkbFail(struct rtl8192cd_priv *priv, struct sk_buff *skb, struct net_device *dev,
				struct net_device *wdsDev, struct tx_insn *txcfg)
{
/*
 * Comment-out the following lines to disable flow-control in any case to fix
 * WDS interface may be blocked sometimes during root interface is up/down
 * continously.
 */
#if 0
#ifdef WDS
	if (wdsDev)
		netif_stop_queue(wdsDev);
	else
#endif
	{
#ifdef WIFI_WMM
		if (!QOS_ENABLE)
#endif
			netif_stop_queue(dev);
	}
#endif

#if 1
if(txcfg->pstat)
{
	int q_num= txcfg->q_num;
		if(q_num == VI_QUEUE) 
			priv->pshare->phw->VI_droppkt_count++;
		else if(q_num == VO_QUEUE) 
			priv->pshare->phw->VO_droppkt_count++;
		else if(q_num == BK_QUEUE) 
			priv->pshare->phw->BK_droppkt_count++;
		else if(q_num == BE_QUEUE) 
			priv->pshare->phw->BE_droppkt_count++;
}
#endif
	priv->ext_stats.tx_drops++;
	DEBUG_WARN("TX DROP: Congested!\n");
	if (txcfg->phdr)
		release_wlanllchdr_to_poll(priv, txcfg->phdr);
	if (skb)
		rtl_kfree_skb(priv, skb, _SKB_TX_);

	return;
}


int rtl8192cd_tx_slowPath(struct rtl8192cd_priv *priv, struct sk_buff *skb, struct stat_info *pstat,
				struct net_device *dev, struct net_device *wdsDev, struct tx_insn *txcfg)
{
	int run = 0;

#ifdef A4_STA
    int use4Addr = 0;
#endif

    //SMP_LOCK_XMIT(flags);
#ifdef CONFIG_RTL_WAPI_SUPPORT
    if ((pstat && pstat->wapiInfo
        && (pstat->wapiInfo->wapiType!=wapiDisable)
        && skb->protocol != __constant_htons(ETH_P_WAPI)
        && (!pstat->wapiInfo->wapiUCastTxEnable))
    )
    {
        rtl8192cd_tx_xmitSkbFail(priv, skb, dev, wdsDev, txcfg);
        goto stop_proc;
    }
#endif

    if (IEEE8021X_FUN && pstat && (pstat->ieee8021x_ctrlport == DOT11_PortStatus_Unauthorized) &&
        #ifdef WDS
        (! (pstat->state & WIFI_WDS)) &&
        #endif
        #ifdef CONFIG_RTK_MESH
        (txcfg->is_11s == 0) && /* is not mesh packet*/
        #endif
        (*((UINT16 *)((UINT8 *)skb->data + ETH_ALEN*2)) != htons(0x888e)) &&
        (!(OPMODE & WIFI_ADHOC_STATE))
    )
    {
        DEBUG_WARN("TX DROP: control port not authorized!\n");
        rtl8192cd_tx_xmitSkbFail(priv, skb, dev, wdsDev, txcfg);
        goto stop_proc;
    }

    // Note: Don't change txcfg->q_num for USB. If do it, you may hold two ownerships, but only release one.
#ifdef CONFIG_PCI_HCI
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
	    if (txcfg->aggre_en < FG_AGGRE_MSDU_FIRST) {
	        txcfg->q_num = BE_QUEUE;	// using low queue for data queue
#ifdef SSID_PRIORITY_SUPPORT
			if (priv->pmib->miscEntry.manual_priority && (OPMODE & WIFI_AP_STATE))
				skb->cb[1] = priv->pmib->miscEntry.manual_priority;
			else
#endif
	        	skb->cb[1] = 0;
	    }
		if (skb->cb[_SKB_CB_FLAGS] & _SKB_CB_FLAGS_URGENT) {
			txcfg->q_num = MGNT_QUEUE;
		}
	}
#endif
    txcfg->fr_type = _SKB_FRAME_TYPE_;
    txcfg->pframe = skb;

    if ((txcfg->aggre_en == FG_AGGRE_MSDU_MIDDLE) || (txcfg->aggre_en == FG_AGGRE_MSDU_LAST))
        txcfg->phdr = NULL;
    else
    {
        txcfg->phdr = (UINT8 *)get_wlanllchdr_from_poll(priv);

        if (txcfg->phdr == NULL) {
            DEBUG_ERR("Can't alloc wlan header!\n");
            rtl8192cd_tx_xmitSkbFail(priv, skb, dev, wdsDev, txcfg);
            goto stop_proc;
        }

        if (skb->len > priv->pmib->dot11OperationEntry.dot11RTSThreshold)
            txcfg->retry = priv->pmib->dot11OperationEntry.dot11LongRetryLimit;
        else
            txcfg->retry = priv->pmib->dot11OperationEntry.dot11ShortRetryLimit;

        if(*((UINT16 *)((UINT8 *)skb->data + ETH_ALEN*2)) == htons(0x888e))
            txcfg->retry = 0x20;

        memset((void *)txcfg->phdr, 0, sizeof(struct wlanllc_hdr));

#ifdef CONFIG_RTK_MESH
        if(txcfg->is_11s)
        {
            SetFrameSubType(txcfg->phdr, WIFI_11S_MESH);
            SetToDs(txcfg->phdr);
        }
        else
#endif
#ifdef WIFI_WMM
        if ((pstat) && (QOS_ENABLE) && (pstat->QosEnabled))
            SetFrameSubType(txcfg->phdr, WIFI_QOS_DATA);
        else
#endif
            SetFrameType(txcfg->phdr, WIFI_DATA);


        #ifdef A4_STA
        if (pstat && (pstat->state & WIFI_A4_STA)) {
            if(priv->pmib->miscEntry.a4_enable == 2) {
                use4Addr = 1;
            }
            else { /*when a4_enable = 1, use 3-address to send EAP packet*/
                if(*((UINT16 *)((UINT8 *)skb->data + ETH_ALEN*2)) != htons(0x888e))
                    use4Addr = 1;
            }                
            txcfg->pstat = pstat;
        }
        #endif

        if (OPMODE & WIFI_AP_STATE) {
            SetFrDs(txcfg->phdr);
            #ifdef WDS
            if (wdsDev)
                SetToDs(txcfg->phdr);
            #endif
            #ifdef A4_STA
            if(use4Addr)
                SetToDs(txcfg->phdr);
            #endif                     
        }
#ifdef CLIENT_MODE
        else if (OPMODE & WIFI_STATION_STATE) {
            SetToDs(txcfg->phdr);
            #ifdef A4_STA
            if(use4Addr)
                SetFrDs(txcfg->phdr);
            #endif         
        }
        else if (OPMODE & WIFI_ADHOC_STATE)
        /* toDS=0, frDS=0 */;
#endif
		else
			DEBUG_WARN("non supported mode yet!\n");
	}

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
    	txcfg->next_txpath = TXPATH_FIRETX;
	}
#endif
	
	//SMP_UNLOCK_XMIT(flags);
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
	    if (__rtl8192cd_firetx(priv, txcfg) == CONGESTED) {
			run = 1;
		}
	}
#endif
#if defined(CONFIG_PCI_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
		if (rtl8192cd_wlantx(priv, txcfg) == CONGESTED) {
			run = 1;
		}
	}
#endif
	if (run)
    {
        //SMP_LOCK_XMIT(flags);
        rtl8192cd_tx_xmitSkbFail(priv, skb, dev, wdsDev, txcfg);
        goto stop_proc;
    }
    //SMP_LOCK_XMIT(flags);

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
    	rtw_handle_xmit_fail(priv, txcfg);
	}
#endif

    /* Reply caller function : process done successfully */
    //SMP_UNLOCK_XMIT(flags);
    return TX_PROCEDURE_CTRL_SUCCESS;

stop_proc:
	
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
    	rtw_handle_xmit_fail(priv, txcfg);
	}
#endif
    //SMP_UNLOCK_XMIT(flags);

    /* Reply caller function : STOP process */
    return TX_PROCEDURE_CTRL_STOP;
}




#ifdef SUPPORT_TX_AMSDU
#ifdef CONFIG_WLAN_HAL
int rtl88XX_signin_txdesc_amsdu(struct rtl8192cd_priv *priv, struct tx_insn* txcfg)
{
    struct tx_desc_info *pswdescinfo, *pdescinfo;
    unsigned int  tx_len;
	u2Byte  *tx_head;
    u4Byte  q_num;
    unsigned long   tmpphyaddr;
    unsigned char *pbuf;
    struct rtl8192cd_hw *phw;
    unsigned long *dma_txhead;

    PHCI_TX_DMA_MANAGER_88XX        ptx_dma;
    u4Byte                          halQNum;
    PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q;
    PTX_BUFFER_DESCRIPTOR           cur_txbd;
#ifdef WLAN_HAL_TX_AMSDU
    PHCI_TX_AMSDU_DMA_MANAGER_88XX  ptx_dma_amsdu;
    PTX_BUFFER_DESCRIPTOR_AMSDU     cur_txbd_amsdu;
#endif
    TX_DESC_DATA_88XX               desc_data;

    q_num = txcfg->q_num;
    phw = GET_HW(priv);

    halQNum     = GET_HAL_INTERFACE(priv)->MappingTxQueueHandler(priv, (u4Byte)q_num);
    ptx_dma     = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PTxDMA88XX);
    cur_q       = &(ptx_dma->tx_queue[halQNum]);
    cur_txbd    = cur_q->pTXBD_head + cur_q->host_idx;
#ifdef WLAN_HAL_TX_AMSDU
    ptx_dma_amsdu  = (PHCI_TX_AMSDU_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PTxDMAAMSDU88XX);
    cur_txbd_amsdu = ptx_dma_amsdu->tx_amsdu_queue[halQNum-1].pTXBD_head_amsdu + cur_q->host_idx;
#endif
    memset(&desc_data, 0, sizeof(TX_DESC_DATA_88XX));
    
    tx_head     = &(ptx_dma->tx_queue[halQNum].host_idx);
    pswdescinfo = get_txdesc_info(priv,priv->pshare->pdesc_info, q_num);
       
    pbuf = ((struct sk_buff *)txcfg->pframe)->data;    
    tx_len = ((struct sk_buff *)txcfg->pframe)->len;

    pdescinfo = pswdescinfo + *tx_head;

    //pdescinfo->type = _SKB_FRAME_TYPE_;


#ifdef WLAN_HAL_TX_AMSDU
    pdescinfo->buf_type_amsdu[pdescinfo->amsdu_num] = _SKB_FRAME_TYPE_;
    pdescinfo->buf_pframe_amsdu[pdescinfo->amsdu_num] = txcfg->pframe;
#endif
#if defined(CONFIG_NET_PCI)
    pdescinfo->len = txcfg->fr_len;
#endif
#if defined(ENABLE_RTL_SKB_STATS) || defined(RESERVE_TXDESC_FOR_EACH_IF)
    pdescinfo->priv = priv;
#endif

    priv->amsdu_len += tx_len;

#ifdef WLAN_HAL_TX_AMSDU
    desc_data.aggreEn   = txcfg->aggre_en;
#endif
    desc_data.pBuf      = pbuf;
    desc_data.frLen     = tx_len;

#ifdef WLAN_HAL_TX_AMSDU
    if (txcfg->aggre_en == FG_AGGRE_MSDU_LAST) {
        desc_data.amsduLen  = priv->amsdu_len;
    }
#endif
#if 0//defined(AP_SWPS_OFFLOAD)
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv)) {

        if(txcfg->pstat->EnSWPS){
            int pkt_Qos = txcfg->pstat->SWPS_pkt_Qos;
            if(pkt_Qos < 0)
            {
                pdescinfo->SWPS_pkt_Qos = pkt_Qos;
            }
            else
            {
                desc_data.SWPS_sequence = txcfg->pstat->SWPS_seq[pkt_Qos];
                pdescinfo->SWPS_sequence = txcfg->pstat->SWPS_seq[pkt_Qos];
                pdescinfo->SWPS_pktQ = halQNum;
                pdescinfo->SWPS_pkt_Qos = pkt_Qos;
            #ifndef SMP_SYNC
                    unsigned long flags;
            #endif

                SAVE_INT_AND_CLI(flags);
                txcfg->pstat->SWPS_seq_head[pkt_Qos] = pdescinfo->SWPS_sequence;
                    
                
                if(txcfg->pstat->SWPS_seq[pkt_Qos] == 0xFFF)
                    txcfg->pstat->SWPS_seq[pkt_Qos] = 1;
                else        
                    txcfg->pstat->SWPS_seq[pkt_Qos]++;
                RESTORE_INT(flags);
            }
        }
        else{
            desc_data.SWPS_sequence = 0;
            pdescinfo->SWPS_sequence = 0;
            pdescinfo->SWPS_pktQ = 0; 
            pdescinfo->SWPS_pkt_Qos = 0; // no use, just to prevent get no qos situation.            
        }
            
        desc_data.DropID = txcfg->pstat->Drop_ID;
        pdescinfo->Drop_ID = txcfg->pstat->Drop_ID;
    }
#endif

    GET_HAL_INTERFACE(priv)->FillTxHwCtrlHandler(priv, halQNum, (void *)&desc_data);

#ifdef RESERVE_TXDESC_FOR_EACH_IF
	if (RSVQ_ENABLE && IS_RSVQ(q_num))
		priv->use_txdesc_cnt[RSVQ(q_num)] ++;
#endif

#if defined(CONFIG_NET_PCI) && !defined(USE_RTL8186_SDK)
    pdescinfo->buf_paddr_amsdu[pdescinfo->amsdu_num]=get_desc(cur_txbd_amsdu->TXBD_ELE[pdescinfo->amsdu_num].Dword1);
    pdescinfo->buf_len_amsdu[pdescinfo->amsdu_num]=get_desc(cur_txbd_amsdu->TXBD_ELE[pdescinfo->amsdu_num].Dword0) & 0xffff;
#endif
	pdescinfo->amsdu_num++;
	
    if (txcfg->aggre_en == FG_AGGRE_MSDU_LAST) {
        GET_HAL_INTERFACE(priv)->SyncSWTXBDHostIdxToHWHandler(priv, halQNum);
    }

    return 0;
}
#endif // CONFIG_WLAN_HAL
int rtl8192cd_signin_txdesc_amsdu(struct rtl8192cd_priv *priv, struct tx_insn* txcfg)
{
	struct tx_desc *phdesc, *pdesc, *pfrstdesc;
	struct tx_desc_info *pswdescinfo, *pdescinfo;
	unsigned int  tx_len;
	int *tx_head, q_num;
	unsigned long	tmpphyaddr;
	unsigned char *pbuf;
	struct rtl8192cd_hw *phw;
#if !defined(USE_RTL8186_SDK) && CONFIG_WLAN_NOT_HAL_EXIST	
	unsigned long *dma_txhead;
#endif
	q_num = txcfg->q_num;
	phw	= GET_HW(priv);
#if !defined(USE_RTL8186_SDK) && CONFIG_WLAN_NOT_HAL_EXIST
	dma_txhead	= get_txdma_addr(phw, q_num);
#endif
	tx_head		= get_txhead_addr(phw, q_num);
	phdesc   	= get_txdesc(phw, q_num);
	pswdescinfo = get_txdesc_info(priv,priv->pshare->pdesc_info, q_num);

	pbuf = ((struct sk_buff *)txcfg->pframe)->data;
	tx_len = ((struct sk_buff *)txcfg->pframe)->len;
	tmpphyaddr = get_physical_addr(priv, pbuf, tx_len, PCI_DMA_TODEVICE);
#ifdef USE_RTL8186_SDK
	rtl_cache_sync_wback(priv, (unsigned long)pbuf, tx_len, PCI_DMA_TODEVICE);
#endif

	pdesc     = phdesc + (*tx_head);
	pdescinfo = pswdescinfo + *tx_head;

	//clear all bits
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
	if(GET_CHIP_VER(priv)== VERSION_8812E || GET_CHIP_VER(priv) == VERSION_8723B)
	{
		memset(pdesc, 0, 40);					//clear all bits
		pdesc->Dword10 = set_desc(tmpphyaddr); // TXBufferAddr
		pdesc->Dword7 |= (set_desc(tx_len & TX_TxBufSizeMask));
		pdesc->Dword0 |= set_desc(40 << TX_OffsetSHIFT); // tx_desc size
	}
	else
#endif
	{
		memset(pdesc, 0, 32);
		pdesc->Dword8 = set_desc(tmpphyaddr); // TXBufferAddr
		pdesc->Dword7 |= (set_desc(tx_len & TX_TxBufSizeMask));
		pdesc->Dword0 |= set_desc(32 << TX_OffsetSHIFT); // tx_desc size
	}

	if (txcfg->aggre_en == FG_AGGRE_MSDU_LAST){
		pdesc->Dword0 = set_desc(TX_OWN | TX_LastSeg);
	}
	else
		pdesc->Dword0 = set_desc(TX_OWN);

#if !defined(USE_RTL8186_SDK) && CONFIG_WLAN_NOT_HAL_EXIST
	rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(dma_txhead[*tx_head]-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(struct tx_desc), PCI_DMA_TODEVICE);
#endif

	pdescinfo->type = _SKB_FRAME_TYPE_;

//#ifdef CONFIG_RTL_8812_SUPPORT
//	if(GET_CHIP_VER(priv)== VERSION_8812E)
//	pdescinfo->paddr = get_desc(pdesc->Dword10); // TXBufferAddr
//	else
//#endif
//	pdescinfo->paddr = get_desc(pdesc->Dword8); // TXBufferAddr

	pdescinfo->pframe = txcfg->pframe;
//	pdescinfo->len = txcfg->fr_len;
#if defined(ENABLE_RTL_SKB_STATS) || defined(RESERVE_TXDESC_FOR_EACH_IF)
	pdescinfo->priv = priv;
#endif
	txdesc_rollover(pdesc, (unsigned int *)tx_head);

	priv->amsdu_len += tx_len;

	if (txcfg->aggre_en == FG_AGGRE_MSDU_LAST) {
		pfrstdesc = priv->amsdu_first_desc;
		pfrstdesc->Dword0 = set_desc((get_desc(pfrstdesc->Dword0) &0xff0000) | priv->amsdu_len | TX_FirstSeg | TX_OWN);
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
		if(GET_CHIP_VER(priv)== VERSION_8812E || GET_CHIP_VER(priv)== VERSION_8723B)
			pfrstdesc->Dword2 |= set_desc(TXdesc_92E_BK);
#endif

#ifdef RESERVE_TXDESC_FOR_EACH_IF
	if (RSVQ_ENABLE && IS_RSVQ(q_num))
		priv->use_txdesc_cnt[RSVQ(q_num)] ++;
#endif

#ifndef USE_RTL8186_SDK
		rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(priv->amsdu_first_dma_desc-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(struct tx_desc), PCI_DMA_TODEVICE);
#endif
		tx_poll(priv, q_num);
	}

	return 0;
}
#endif // SUPPORT_TX_AMSDU

#ifdef __OSK__
__IRAM_WIFI_PRI5
#else
__MIPS16 __IRAM_IN_865X 
#endif
void assign_wlanseq(struct rtl8192cd_hw *phw, unsigned char *pframe, struct stat_info *pstat, struct wifi_mib *pmib
#ifdef CONFIG_RTK_MESH	// For broadcast data frame via mesh (ex:ARP requst)
	, unsigned char is_11s
#endif
	)
{
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	u16 seq;
#endif

#ifdef WIFI_WMM
	unsigned char qosControl[2];
	int tid;

	if (is_qos_data(pframe)) {
		memcpy(qosControl, GetQosControl(pframe), 2);
		tid = qosControl[0] & 0x07;

		if (pstat) {
			SetSeqNum(pframe, pstat->AC_seq[tid]);
			pstat->AC_seq[tid] = (pstat->AC_seq[tid] + 1) & 0xfff;
		}
		else {
//			SetSeqNum(pframe, phw->AC_seq[tid]);
//			phw->AC_seq[tid] = (phw->AC_seq[tid] + 1) & 0xfff;
#ifdef CONFIG_RTK_MESH	// For broadcast data frame via mesh (ex:ARP requst)
			if (is_11s)
			{
				SetSeqNum(pframe, phw->seq);
				phw->seq = (phw->seq + 1) & 0xfff;
			}
			else
#endif
				printk("Invalid seq num setting for Multicast or Broadcast pkt!!\n");
		}

		{
			if ((tid == 7) || (tid == 6))
				phw->VO_pkt_count++;
			else if ((tid == 5) || (tid == 4))
				phw->VI_pkt_count++;
			else if ((tid == 2) || (tid == 1))
				phw->BK_pkt_count++;
#ifdef WMM_VIBE_PRI
			else
				phw->BE_pkt_count++;
#endif
		}
	}
	else
#endif
	{
#if defined(CONFIG_PCI_HCI) || defined(CONFIG_RTL_TRIBAND_SUPPORT)
		SetSeqNum(pframe, phw->seq);
		phw->seq = (phw->seq + 1) & 0xfff;
#elif defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
		seq = atomic_inc_return(&phw->seq);
		seq = seq & 0x0fff;
		SetSeqNum(pframe, seq);
#endif
	}
}




#ifdef TX_SHORTCUT

#ifdef CONFIG_WLAN_HAL
#if (!defined(__OSK__)) || (defined(__OSK__) && !defined(CONFIG_RTL6028))
__MIPS16
#endif
__IRAM_IN_865X
void rtl88XX_signin_txdesc_shortcut(struct rtl8192cd_priv *priv, struct tx_insn *txcfg, int idx)
{
    struct tx_desc_info *pswdescinfo, *pdescinfo;
    struct rtl8192cd_hw *phw;
    int q_num;
    u2Byte *tx_head;
    struct stat_info *pstat;
    struct sk_buff *pskb;

    PHCI_TX_DMA_MANAGER_88XX        ptx_dma;
    u32                             halQNum;
    PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q;
    PTX_BUFFER_DESCRIPTOR           cur_txbd;
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
    PHCI_TX_AMSDU_DMA_MANAGER_88XX  ptx_dma_amsdu;
    PTX_BUFFER_DESCRIPTOR_AMSDU     cur_txbd_amsdu;
#endif

    TX_DESC_DATA_88XX               desc_data;
    BOOLEAN                         use_hwtxsc = FALSE;

    pstat = txcfg->pstat;
    pskb = (struct sk_buff *)txcfg->pframe;

    phw = GET_HW(priv);
    q_num = txcfg->q_num;

    halQNum     = GET_HAL_INTERFACE(priv)->MappingTxQueueHandler(priv, (u32)q_num);
    ptx_dma     = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PTxDMA88XX);
    cur_q       = &(ptx_dma->tx_queue[halQNum]);
    cur_txbd    = cur_q->pTXBD_head + cur_q->host_idx;
    memset(&desc_data, 0, sizeof(TX_DESC_DATA_88XX));

    tx_head     = &(cur_q->host_idx);

    pswdescinfo = get_txdesc_info(priv,priv->pshare->pdesc_info, q_num);    

    pdescinfo = pswdescinfo + *tx_head;

	// TODO: if enable HW_SEQ , it should be disabled here...
#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
    if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(priv)) {
        // Don't assign seq num in this case.....
    } else
#endif
    {
    assign_wlanseq(GET_HW(priv), txcfg->phdr, pstat, GET_MIB(priv)
#ifdef CONFIG_RTK_MESH	// For broadcast data frame via mesh (ex:ARP requst)
    , txcfg->is_11s
#endif
        );
    }

#if defined(WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC)
    // Hw_tx_shortcut just support 1 entry for one STA
    // if enable HW TX shortcut, the following TXDESC information no need modify
    if((!IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC(priv))|| pstat->tx_sc_hw_idx != idx)
#elif defined(SUPPORT_TXDESC_IE)      
    if((!IS_SUPPORT_TXDESC_IE(priv))|| pstat->tx_sc_hw_idx != idx)
#endif        
    {

        if ((pstat->IOTPeer==HT_IOT_PEER_INTEL) && (pstat->retry_inc)) {
            if (is_MCS_rate(pstat->current_tx_rate) && !(pstat->leave)
                && priv->pshare->intel_rty_lmt) {
                desc_data.rtyLmtEn   = _TRUE;
                desc_data.dataRtyLmt = priv->pshare->intel_rty_lmt; 
            } else {
                desc_data.rtyLmtEn   = _FALSE;
                desc_data.dataRtyLmt = 1;
            }
        }
	if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) &&
		(txcfg->pstat) && (txcfg->pstat->MIMO_ps & _HT_MIMO_PS_DYNAMIC_) &&
		(is_2T_rate(txcfg->tx_rate) || (txcfg->pstat->tx_avarage>=(3<<17))))
		{	// when HT MIMO Dynamic power save is set and rate > MCS7, RTS is needed
			desc_data.RTSEn = _TRUE;
		    desc_data.HWRTSEn = _FALSE;
		    desc_data.CTS2Self= _FALSE;	
		}

        if((priv->pshare->rf_ft_var.txforce != 0xff || (txcfg->pstat && txcfg->pstat->force_rate))
            #if (BEAMFORMING_SUPPORT == 1)
				&& (!txcfg->ndpa) 
            #endif
			){
				if(txcfg->pstat && txcfg->pstat->force_rate)
					desc_data.dataRate	= txcfg->pstat->force_rate;
				else
					desc_data.dataRate	= priv->pshare->rf_ft_var.txforce;
				desc_data.disDataFB = _TRUE;
				desc_data.disRTSFB	= _TRUE;
				desc_data.useRate	= _TRUE;
		}

#if (BEAMFORMING_SUPPORT == 1)
		if (txcfg->pstat)
		{
			if ((txcfg->pstat->cmn_info.stbc_en & HT_STBC_EN)
#ifdef RTK_AC_SUPPORT
			|| (txcfg->pstat->cmn_info.stbc_en & VHT_STBC_EN)
#endif
			) {
				u1Byte					Idx = 0;

				PRT_BEAMFORMING_ENTRY	pEntry;
				pEntry = Beamforming_GetEntryByMacId(priv, txcfg->pstat->cmn_info.aid, &Idx);

				if (((get_rf_mimo_mode(priv) != RF_1T1R) && (get_rf_mimo_mode(priv) != RF_1T2R)) && (pEntry == NULL))
					desc_data.dataStbc = 1;
			}
		}
#endif
    }

    desc_data.iv = txcfg->iv;
    desc_data.secType = txcfg->privacy;

#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))
		if(txcfg->pstat)
		{
			odm_set_tx_ant_by_tx_info_hal(priv, (PVOID)&desc_data, txcfg->pstat->cmn_info.aid);   //HAL Set TX antenna	
		}
#endif	//#if (defined(CONFIG_HW_ANTENNA_DIVERSITY))

#ifdef CLIENT_MODE
    if (OPMODE & WIFI_STATION_STATE) {
        if (GetFrameSubType(txcfg->phdr) == WIFI_PSPOLL) {
            desc_data.navUseHdr = _TRUE;
        }

        if (priv->ps_state)
            SetPwrMgt(txcfg->phdr);
        else
            ClearPwrMgt(txcfg->phdr);
    }
#endif

    // TODO: have_hw_mic
//one_txdesc:

    //descinfo_copy(pdescinfo, &pstat->tx_sc_ent[idx].swdesc1);
    pdescinfo->type = pstat->tx_sc_ent[idx].swdesc1.type;
#ifndef TXDESC_INFO	
    pdescinfo->len = pstat->tx_sc_ent[idx].swdesc1.len;
    pdescinfo->rate = pstat->tx_sc_ent[idx].swdesc1.rate;  
#endif	

    //??? pdescinfo->buf_type[0] = pstat->tx_sc_ent[idx].swdesc1.buf_type[0];
    pdescinfo->buf_type[0] = txcfg->fr_type;
    
    pdescinfo->pframe = txcfg->phdr;
    //pdescinfo->buf_pframe[0] = txcfg->pframe;
#if defined(WIFI_WMM) && defined(WMM_APSD)
#if defined(ENABLE_RTL_SKB_STATS) || defined(RESERVE_TXDESC_FOR_EACH_IF)
    pdescinfo->priv = priv;
#endif
#ifndef TXDESC_INFO		
    pdescinfo->pstat = pstat;
#endif	
#endif
    
    desc_data.pHdr   = txcfg->phdr;    
    desc_data.hdrLen = txcfg->hdr_len;
    desc_data.llcLen = txcfg->llc;
    desc_data.frLen  = txcfg->fr_len;   // pskb->len ??
    //if (pskb->len != txcfg->fr_len) {
    //}
    desc_data.pBuf   = pskb->data;

#if defined(WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC)
    if(IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC(priv) && pstat->tx_sc_hw_idx == idx){
        desc_data.stwEn = TRUE;
    }
#endif  //defined(WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC) 
#if defined(WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC) || defined(SUPPORT_TXDESC_IE)
    if(IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC(priv) || IS_SUPPORT_TXDESC_IE(priv))   {

        if(pstat->tx_sc_hw_idx == idx) {
            //desc_data.stwEn = TRUE;
            if (txcfg->pstat->cmn_info.aid != MANAGEMENT_AID) {
                desc_data.macId = REMAP_AID(txcfg->pstat);
                desc_data.rateId = txcfg->pstat->cmn_info.ra_info.rate_id;
				if (txcfg->pstat->tx_bw == CHANNEL_WIDTH_80_80)
					desc_data.dataBW = CHANNEL_WIDTH_160;
				else
                desc_data.dataBW = txcfg->pstat->tx_bw;
                desc_data.tid = pskb->cb[1];
#ifdef WMM_DSCP_C42
				{
					unsigned int	   q_num;
					q_num = pri_to_qnum(priv, desc_data.tid);

					if(q_num ^ txcfg->q_num)
						desc_data.tid = 0x04;
				}
#elif defined(CONFIG_WLAN_HAL_8197F)
				{
					unsigned int	   q_num;
					q_num = pri_to_qnum(priv, desc_data.tid);
				}
#endif
				
            }      
	#if (BEAMFORMING_SUPPORT == 1)
	if((priv->pmib->dot11RFEntry.txbf == 1) &&
			((txcfg->pstat->ht_cap_len && (txcfg->pstat->ht_cap_buf.txbf_cap)) 
#ifdef RTK_AC_SUPPORT
			||(txcfg->pstat->vht_cap_len && (cpu_to_le32(txcfg->pstat->vht_cap_buf.vht_cap_info) & BIT(SU_BFEE_S)))
#endif
	)){
			desc_data.p_aid = txcfg->pstat->cmn_info.bf_info.p_aid;
			desc_data.g_id  = txcfg->pstat->cmn_info.bf_info.g_id;
		}
#if (MU_BEAMFORMING_SUPPORT == 1)
		if (txcfg->pstat->muPartner_num) {
			desc_data.p_aid = txcfg->pstat->cmn_info.bf_info.p_aid;
			//desc_data.g_id  = txcfg->pstat->cmn_info.bf_info.g_id;
			//printk("assign P_AID:%d into tx desc\n", desc_data.p_aid);
		}
#endif
#endif
            use_hwtxsc = TRUE;
        }
        else {
            pstat->tx_sc_hw_idx = idx;
        }        
    }
#endif //  defined(WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC) || defined(SUPPORT_TXDESC_IE)
#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
    if(IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(priv))   {   

        desc_data.swCrypt = FALSE;
        desc_data.icv     = 0;
        desc_data.mic     = 0;

#ifdef WLAN_HAL_HW_AES_IV
        if (txcfg->privacy == _CCMP_PRIVACY_) {
            desc_data.iv = 0;
            desc_data.hwAESIv = TRUE;
        }
#endif
		desc_data.smhEn = TRUE;
#ifdef SUPPORT_TX_HW_AMSDU
        desc_data.HW_AMSDU = TRUE;
#endif

		desc_data.pHdr	   = txcfg->phdr; // 802.3 hdr len + payload len;
		desc_data.hdrLen   = txcfg->fr_len; // 802.3 hdr + payload

		desc_data.llcLen   = 0;

		desc_data.frLen    = 0;
		desc_data.pBuf	   = 0; // useless....because frLen is zero
		#if IS_RTL88XX_MAC_V4       
        if(_GET_HAL_DATA(priv)->MacVersion.is_MAC_v4){
            desc_data.smhCamIdx = GET_HAL_INTERFACE(priv)->GetSmhCamHandler(priv, txcfg->pstat);
            if(desc_data.smhCamIdx == SMHCAM_SEARCH_FAIL){
                DEBUG_WARN("SMH CAM search fail.\n");
            }
        }
        #endif 
		// TODO: recycle problem for sw desc..
    }
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV

#if defined(AP_SWPS_OFFLOAD)
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv)) {

        if(txcfg->pstat == NULL){
            printk("pstat is NULL\n");
            desc_data.SWPS_sequence = 0;
            pdescinfo->SWPS_sequence = 0;
            //pdescinfo->SWPS_pktQ = 0; 
            pdescinfo->SWPS_pkt_Qos = -2; //no pstat, pkt is multicast/broadcast
        }else{

            if(txcfg->pstat->EnSWPS){
                pdescinfo->swps_pstat = pstat; //for recycle and reprepare pkt use
                int pkt_Qos = halQNum-1;//txcfg->pstat->SWPS_pkt_Qos;
                if((pkt_Qos > 3) || ( pkt_Qos < 0))
                {
                    //not 4 ac packet, no need to reprepare, so no need to record related info
                    pdescinfo->SWPS_pkt_Qos = -2; //direct reycle
                }
                else
                {
                    desc_data.SWPS_sequence = txcfg->pstat->SWPS_seq[pkt_Qos];
                    pdescinfo->SWPS_sequence = txcfg->pstat->SWPS_seq[pkt_Qos];
                    //pdescinfo->SWPS_pktQ = halQNum;
                    pdescinfo->SWPS_pkt_Qos = pkt_Qos;
                    #ifndef SMP_SYNC
                	    //unsigned long flags;
                    #endif

                	//SAVE_INT_AND_CLI(flags); //may not need
                    txcfg->pstat->SWPS_seq_head[pkt_Qos] = pdescinfo->SWPS_sequence;
                        
                    
                    if(txcfg->pstat->SWPS_seq[pkt_Qos] == 0xFFF)
                        txcfg->pstat->SWPS_seq[pkt_Qos] = 1;
                    else        
                        txcfg->pstat->SWPS_seq[pkt_Qos]++;

                    //RESTORE_INT(flags);
                }
            }
            else{
                desc_data.SWPS_sequence = 0;
                pdescinfo->SWPS_sequence = 0;
                //pdescinfo->SWPS_pktQ = 0; 
                pdescinfo->SWPS_pkt_Qos = 0; // no use, just to prevent get no qos situation.
            }
                
            desc_data.DropID = txcfg->pstat->Drop_ID;
            pdescinfo->Drop_ID = txcfg->pstat->Drop_ID;
        }
    }
#endif


#ifdef MERGE_HEADER_PAYLOAD
//    if (txcfg->one_txdesc) {
		desc_data.pHdr	   = txcfg->phdr; // 802.3 hdr + payload
		desc_data.hdrLen   = txcfg->fr_len; // len(802.3 hdr + payload)

		desc_data.llcLen   = 0;

		desc_data.frLen    = 0;
		desc_data.pBuf	   = 0;
//    }
#endif

    if (txcfg->privacy) {
#ifdef CONFIG_IEEE80211W
       txcfg->use_sw_enc = UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE), txcfg->isPMF);
#else
       txcfg->use_sw_enc = UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE));
#endif		
    }
	
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
    desc_data.aggreEn = txcfg->aggre_en;
    if (txcfg->aggre_en == FG_AGGRE_MSDU_FIRST) {
        priv->amsdu_len = desc_data.hdrLen + desc_data.llcLen + desc_data.frLen;

        if (txcfg->use_sw_enc == FALSE)
        {
            priv->amsdu_len += (desc_data.icv + desc_data.mic + desc_data.iv);
        }
    }
//	if(txcfg->aggre_en == FG_AGGRE_MSDU_FIRST)
//		AMSDU_DBG_PRINT("%s %d desc_data.frLen %d txcfg->fr_len %d priv->amsdu_len %d desc_data.aggreEn %d\n", __func__, __LINE__, desc_data.frLen, txcfg->fr_len, priv->amsdu_len, desc_data.aggreEn);
#endif

#if defined(DRVMAC_LB) && defined(SUPPORT_TXDESC_IE) && defined(CONFIG_8814_AP_MAC_VERI)//yllin8814B, test txdesc shortcut
    desc_data.macId = priv->macID_temp;
    //printk("shortcut use hw = 0x%x, idx=0x%x\n",use_hwtxsc,idx);

#endif

    GET_HAL_INTERFACE(priv)->FillShortCutTxHwCtrlHandler(
        priv, halQNum, (void *)&desc_data, pstat->tx_sc_ent[idx].hal_hw_desc, 0x02, use_hwtxsc);

	// TODO: check the sw desc here for HW_TX_SHORTCUT
#ifndef TXDESC_INFO	
    pdescinfo->paddr         = get_desc(cur_txbd->TXBD_ELE[1].Dword1);//header address
#endif	
//    pdescinfo->len           = get_desc(cur_txbd->TXBD_ELE[1].Dword0) & 0xffff;
#if defined(CONFIG_NET_PCI)
    pdescinfo->len           = get_desc(cur_txbd->TXBD_ELE[1].Dword0) & 0xffff;
    pdescinfo->buf_paddr[0]  = get_desc(cur_txbd->TXBD_ELE[2].Dword1);//payload address
    pdescinfo->buf_len[0]    = txcfg->fr_len; // pskb->len;
#endif    
    pdescinfo->buf_pframe[0] = pskb;
//    pdescinfo->buf_len[0]    = txcfg->fr_len; // pskb->len;

#ifdef SUPPORT_TX_AMSDU_SHORTCUT
            pdescinfo->amsdu_num = 0;
            if (txcfg->aggre_en == FG_AGGRE_MSDU_FIRST) {
                ptx_dma_amsdu  = (PHCI_TX_AMSDU_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PTxDMAAMSDU88XX);
                cur_txbd_amsdu = ptx_dma_amsdu->tx_amsdu_queue[halQNum-1].pTXBD_head_amsdu + cur_q->host_idx;

                pdescinfo->buf_type_amsdu[0]    = txcfg->fr_type;
//		AMSDU_DBG_PRINT("%s %d pframe %p\n", __func__, __LINE__, txcfg->pframe);
                pdescinfo->buf_pframe_amsdu[0]  = txcfg->pframe;
#if defined(CONFIG_NET_PCI) && !defined(USE_RTL8186_SDK)
				pdescinfo->buf_paddr_amsdu[0]=get_desc(cur_txbd_amsdu->TXBD_ELE[0].Dword1);
                pdescinfo->buf_len_amsdu[0]=get_desc(cur_txbd_amsdu->TXBD_ELE[0].Dword0) & 0xffff;
#endif                 
                pdescinfo->buf_type[0] = _RESERVED_FRAME_TYPE_;
                pdescinfo->buf_pframe[0] = NULL;
                pdescinfo->amsdu_num = 1;
            }
#endif // WLAN_HAL_TX_AMSDU


#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
    if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(priv)) {
        pdescinfo->type          = _SKB_FRAME_TYPE_; // 802.3 header + payload
        pdescinfo->pframe        = txcfg->pframe; // skb
        
        pdescinfo->buf_type[0]   = 0;
        pdescinfo->buf_pframe[0] = 0; // no packet payload        
#ifdef AP_SWPS_OFFLOAD        
        pdescinfo->isWiFiHdr = FALSE;
#endif
    }
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV

#ifdef MERGE_HEADER_PAYLOAD
    pdescinfo->type          = _SKB_FRAME_TYPE_; // 802.3 header + payload
    pdescinfo->pframe        = txcfg->phdr; // skb

    pdescinfo->buf_type[0]   = 0;
    pdescinfo->buf_pframe[0] = 0; // no packet payload     
#endif


#ifdef MERGE_TXDESC_HEADER_PAYLOAD
//    if (q_num == BE_QUEUE) {
    pdescinfo->type             = _SKB_FRAME_TYPE_; // txdesc + 802.3 header + payload
    pdescinfo->pframe           = txcfg->pframe;    // skb

    pdescinfo->buf_type[0]      = 0;
    pdescinfo->buf_pframe[0]    = 0;

    release_wlanllchdr_to_poll(priv, txcfg->phdr);
//        }
#endif

#ifdef RESERVE_TXDESC_FOR_EACH_IF
	if (RSVQ_ENABLE && IS_RSVQ(q_num))
		priv->use_txdesc_cnt[RSVQ(q_num)] ++;
#endif

#ifdef SUPPORT_TX_AMSDU_SHORTCUT
    if (txcfg->aggre_en == FG_AGGRE_MSDU_FIRST) {
        return;
    }
#endif

    GET_HAL_INTERFACE(priv)->SyncSWTXBDHostIdxToHWHandler(priv, halQNum);

	return;
}
#endif // CONFIG_WLAN_HAL


/*static*/ int rtl8192cd_tx_tkip(struct rtl8192cd_priv *priv, struct sk_buff *skb, struct stat_info*pstat, struct tx_insn *txcfg)
{
    struct wlan_ethhdr_t *pethhdr;
    struct llc_snap	*pllc_snap = NULL;
    unsigned char * da;
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
   if(txcfg->aggre_en >= FG_AGGRE_MSDU_FIRST)
   	pethhdr = (struct wlan_ethhdr_t *)(skb->data);
   else
#endif
    pethhdr = (struct wlan_ethhdr_t *)(skb->data - WLAN_ETHHDR_LEN);

    da = pethhdr->daddr;
#ifdef MCAST2UI_REFINE
    memcpy(pethhdr->daddr, &skb->cb[10], 6);
#endif

#ifdef A4_STA
    if(pstat && (pstat->state & WIFI_A4_STA)) {
        da = GetAddr3Ptr(txcfg->phdr);
    }
#endif

    if(txcfg->llc) {
        pllc_snap = (struct llc_snap *)((UINT8 *)(txcfg->phdr) + txcfg->hdr_len + txcfg->iv);
    }
    
#ifdef WIFI_WMM
    if ((tkip_mic_padding(priv, da, pethhdr->saddr, ((QOS_ENABLE) && (pstat) && (pstat->QosEnabled))?skb->cb[1]:0, (UINT8 *)pllc_snap,
            skb, txcfg)) == FALSE)
#else
    if ((tkip_mic_padding(priv, da, pethhdr->saddr, 0, (UINT8 *)pllc_snap,
            skb, txcfg)) == FALSE)
#endif
    {
        priv->ext_stats.tx_drops++;
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
	if(txcfg->aggre_en == FG_AGGRE_MSDU_LAST)
		printk("%s(%d) impossible failed on AMSDU packets\n", __func__, __LINE__);
#endif
        DEBUG_ERR("TX DROP: Tkip mic padding fail!\n");
        rtl_kfree_skb(priv, skb, _SKB_TX_);
        release_wlanllchdr_to_poll(priv, txcfg->phdr);
        goto stop_proc;
    }
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
	if ((txcfg->aggre_en < FG_AGGRE_MSDU_FIRST) || (txcfg->aggre_en == FG_AGGRE_MSDU_LAST))
#endif
	{
    skb_put((struct sk_buff *)txcfg->pframe, 8);
    txcfg->fr_len += 8;	// for Michael padding.
	}

    /* Reply caller function : Continue process */
    return TX_PROCEDURE_CTRL_CONTINUE;

stop_proc:
    /* Reply caller function : STOP process */
    return TX_PROCEDURE_CTRL_STOP;
}


#ifdef CONFIG_PCI_HCI
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
#if (!defined(__OSK__)) || (defined(__OSK__) && !defined(CONFIG_RTL6028))
__MIPS16
#endif
__IRAM_IN_865X
void rtl8192cd_signin_txdesc_shortcut_8812(struct rtl8192cd_priv *priv, struct tx_insn *txcfg, int idx)
{
	struct tx_desc *phdesc, *pdesc, *pfrstdesc;
	struct tx_desc_info *pswdescinfo, *pdescinfo;
	struct rtl8192cd_hw	*phw;
	int *tx_head, q_num;
	struct stat_info *pstat;
	struct sk_buff *pskb;
	unsigned long pfrst_dma_desc;
	unsigned long *dma_txhead;
/*
#if defined(CONFIG_RTL_WAPI_SUPPORT)
	uint8				*wapiMic2;
	struct tx_desc		*pmicdesc;
	struct tx_desc_info	*pmicdescinfo;
#endif
*/
	pstat = txcfg->pstat;
	pskb = (struct sk_buff *)txcfg->pframe;
	pfrst_dma_desc=0;

	phw	= GET_HW(priv);
	q_num = txcfg->q_num;

	dma_txhead	= get_txdma_addr(phw, q_num);
	tx_head		= get_txhead_addr(phw, q_num);
	phdesc		= get_txdesc(phw, q_num);
	pswdescinfo	= get_txdesc_info(priv,priv->pshare->pdesc_info, q_num);

	/*------------------------------------------------------------*/
	/*           fill descriptor of header + iv + llc             */
	/*------------------------------------------------------------*/
	pfrstdesc = pdesc = phdesc + *tx_head;
	pdescinfo = pswdescinfo + *tx_head;

	memcpy(pdesc, &pstat->tx_sc_ent[idx].hwdesc1, 40);

	assign_wlanseq(GET_HW(priv), txcfg->phdr, pstat, GET_MIB(priv)
#ifdef CONFIG_RTK_MESH	// For broadcast data frame via mesh (ex:ARP requst)
	, txcfg->is_11s
#endif
		);

	pdesc->Dword9 = 0;
	pdesc->Dword9 |= set_desc((GetSequence(txcfg->phdr) & TXdesc_92E_TX_SeqMask)  << TXdesc_92E_TX_SeqSHIFT);	

	if((priv->pshare->rf_ft_var.txforce != 0xff)
#if (BEAMFORMING_SUPPORT == 1)
		&& (!txcfg->ndpa) 
#endif
	){
		pdesc->Dword4 &= set_desc(~(TXdesc_92E_DataRateMask << TXdesc_92E_DataRateSHIFT));
		pdesc->Dword3 |= set_desc(TXdesc_92E_DisDataFB|TXdesc_92E_DisRtsFB|TXdesc_92E_UseRate);
		pdesc->Dword4 |= set_desc((priv->pshare->rf_ft_var.txforce & TXdesc_92E_DataRateMask) << TXdesc_92E_DataRateSHIFT);
	}

	if (txcfg->one_txdesc) {
#ifdef TX_EARLY_MODE
		if (GET_TX_EARLY_MODE) {
			pdesc->Dword0 = set_desc(((get_desc(pdesc->Dword0) & 0xff00ffff) |(0x28 << TX_OffsetSHIFT)) |
									TX_LastSeg | 	(txcfg->hdr_len + txcfg->llc + txcfg->iv + txcfg->fr_len));
			pdesc->Dword1 = set_desc(get_desc(pdesc->Dword1) | (1 << TX_PktOffsetSHIFT) );
			pdesc->Dword7 = set_desc((get_desc(pdesc->Dword7) & 0xffff0000) |
						(txcfg->hdr_len + txcfg->llc + txcfg->iv + txcfg->fr_len+8));
			memset(txcfg->phdr-8, '\0', 8);			
			if (pstat->empkt_num > 0) 				
				insert_emcontent(priv, txcfg, txcfg->phdr-8);
			pdesc->Dword10 = set_desc(get_physical_addr(priv, txcfg->phdr-8,
				(get_desc(pdesc->Dword7)&TX_TxBufSizeMask), PCI_DMA_TODEVICE));
					
		}
		else
#endif
		{	
			pdesc->Dword0 = set_desc((get_desc(pdesc->Dword0) & 0xffff0000) |
				TX_LastSeg | (txcfg->hdr_len + txcfg->llc + txcfg->iv + txcfg->fr_len));
			pdesc->Dword7 = set_desc((get_desc(pdesc->Dword7) & 0xffff0000) |
				(txcfg->hdr_len + txcfg->llc + txcfg->iv + txcfg->fr_len));
		}
	}

#ifdef TXSC_SKBLEN
    else {
		pdesc->Dword0 = set_desc((get_desc(pdesc->Dword0) & 0xffff0000) |
						 (txcfg->hdr_len + txcfg->llc + txcfg->iv + txcfg->fr_len));
		pdesc->Dword7 = set_desc((get_desc(pdesc->Dword7) & 0xffff0000) |
						  (txcfg->hdr_len + txcfg->llc + txcfg->iv ));
	}
#endif

#if (BEAMFORMING_SUPPORT == 1)
	if (priv->pmib->dot11RFEntry.txbf == 1)
	{
		if ((pstat->cmn_info.stbc_en & HT_STBC_EN)
#ifdef RTK_AC_SUPPORT
			|| (pstat->cmn_info.stbc_en & VHT_STBC_EN)
#endif
		) {
			u1Byte					Idx = 0;
			PRT_BEAMFORMING_ENTRY	pEntry;
			pEntry = Beamforming_GetEntryByMacId(priv, pstat->cmn_info.aid, &Idx);
			if (pEntry)
				pdesc->Dword5 &= set_desc(~ (BIT(TXdesc_92E_DataStbcSHIFT)));
		}
	}
#endif
#ifdef TX_EARLY_MODE
	if (GET_TX_EARLY_MODE) 
		pdesc->Dword10 = set_desc(get_physical_addr(priv, txcfg->phdr-8,
			(get_desc(pdesc->Dword7)& TX_TxBufSizeMask), PCI_DMA_TODEVICE));
	else
#endif
	pdesc->Dword10 = set_desc(get_physical_addr(priv, txcfg->phdr,
		(get_desc(pdesc->Dword7)& TX_TxBufSizeMask), PCI_DMA_TODEVICE));

	descinfo_copy(pdescinfo, &pstat->tx_sc_ent[idx].swdesc1);
#ifndef TXDESC_INFO
	pdescinfo->paddr  = get_desc(pdesc->Dword10); // buffer addr
#endif	
	if (txcfg->one_txdesc) {
		pdescinfo->type = _SKB_FRAME_TYPE_;
		pdescinfo->pframe = pskb;
#if defined(ENABLE_RTL_SKB_STATS) || defined(RESERVE_TXDESC_FOR_EACH_IF)
		pdescinfo->priv = priv;
#endif
#if defined(WIFI_WMM) && defined(WMM_APSD)
#ifndef TXDESC_INFO
		pdescinfo->pstat = pstat;
#endif		
#endif
	}
	else {
		pdescinfo->pframe = txcfg->phdr;
#if defined(WIFI_WMM) && defined(WMM_APSD)
#if defined(ENABLE_RTL_SKB_STATS) || defined(RESERVE_TXDESC_FOR_EACH_IF)
		pdescinfo->priv = priv;
#endif
#ifndef TXDESC_INFO		
		pdescinfo->pstat = pstat;
#endif		
#endif
	}

#ifdef CLIENT_MODE
	if (OPMODE & WIFI_STATION_STATE) {
		if (GetFrameSubType(pdescinfo->pframe) == WIFI_PSPOLL)
			pdesc->Dword1 |= set_desc(TX_NAVUSEHDR);

		if (priv->ps_state)
			SetPwrMgt(pdescinfo->pframe);
		else
			ClearPwrMgt(pdescinfo->pframe);
	}
#endif

	pfrst_dma_desc = dma_txhead[*tx_head];
/*
#ifdef USE_RTL8186_SDK
	rtl_cache_sync_wback(priv, get_desc(pdesc->Dword8), (get_desc(pdesc->Dword7)&TX_TxBufferSizeMask), PCI_DMA_TODEVICE);
#endif
*/

#ifdef OSK_LOW_TX_DESC
	if (q_num!=BE_QUEUE && q_num!=HIGH_QUEUE)
		txdesc_rollover_nonbe(pdesc, (unsigned int *)tx_head);
	else
#endif
	txdesc_rollover(pdesc, (unsigned int *)tx_head);

	if (txcfg->one_txdesc)
		goto one_txdesc;

	/*------------------------------------------------------------*/
	/*              fill descriptor of frame body                 */
	/*------------------------------------------------------------*/
	pdesc = phdesc + *tx_head;
	pdescinfo = pswdescinfo + *tx_head;
    memcpy(pdesc, &pstat->tx_sc_ent[idx].hwdesc2, 40);

#ifdef TXSC_SKBLEN
	pdesc->Dword7 = set_desc((get_desc(pdesc->Dword7) & 0xffff0000) |
				  ( txcfg->fr_len));
#endif

	pdesc->Dword10 = set_desc(get_physical_addr(priv, pskb->data,
		(get_desc(pdesc->Dword7)&0x0fff), PCI_DMA_TODEVICE));

	descinfo_copy(pdescinfo, &pstat->tx_sc_ent[idx].swdesc2);
#ifndef TXDESC_INFO
	pdescinfo->paddr  = get_desc(pdesc->Dword10);
#endif	
	pdescinfo->pframe = pskb;
#if defined(ENABLE_RTL_SKB_STATS) || defined(RESERVE_TXDESC_FOR_EACH_IF)
	pdescinfo->priv = priv;
#endif
/*
#ifndef USE_RTL8186_SDK
	rtl_cache_sync_wback(priv, dma_txhead[*tx_head], sizeof(struct tx_desc), PCI_DMA_TODEVICE);
#else
	rtl_cache_sync_wback(priv, get_desc(pdesc->Dword8), (get_desc(pdesc->Dword7)&TX_TxBufferSizeMask), PCI_DMA_TODEVICE);
#endif
*/

#if defined(CONFIG_RTL_WAPI_SUPPORT)
	if (txcfg->privacy == _WAPI_SMS4_)
	{
		SecSWSMS4Encryption(priv, txcfg);
	}
#endif

#ifndef NOT_RTK_BSP
	if ((txcfg->privacy == _TKIP_PRIVACY_) &&
		(priv->pshare->have_hw_mic) &&
		!(priv->pmib->dot11StationConfigEntry.swTkipMic))
	{
		register unsigned long int l,r;
		unsigned char *mic;
		int delay = 18;

		while ((*(volatile unsigned int *)GDMAISR & GDMA_COMPIP) == 0) {
			delay_us(delay);
			delay = delay / 2;
		}

		l = *(volatile unsigned int *)GDMAICVL;
		r = *(volatile unsigned int *)GDMAICVR;

		mic = ((struct sk_buff *)txcfg->pframe)->data + txcfg->fr_len - 8;
		mic[0] = (unsigned char)(l & 0xff);
		mic[1] = (unsigned char)((l >> 8) & 0xff);
		mic[2] = (unsigned char)((l >> 16) & 0xff);
		mic[3] = (unsigned char)((l >> 24) & 0xff);
		mic[4] = (unsigned char)(r & 0xff);
		mic[5] = (unsigned char)((r >> 8) & 0xff);
		mic[6] = (unsigned char)((r >> 16) & 0xff);
		mic[7] = (unsigned char)((r >> 24) & 0xff);

#ifdef MICERR_TEST
		if (priv->micerr_flag) {
			mic[7] ^= mic[7];
			priv->micerr_flag = 0;
		}
#endif
	}
#endif // NOT_RTK_BSP

#ifndef USE_RTL8186_SDK
	rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(dma_txhead[*tx_head]-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(struct tx_desc), PCI_DMA_TODEVICE);
#endif
	rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(get_desc(pdesc->Dword10)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), (get_desc(pdesc->Dword7)&TX_TxBufSizeMask), PCI_DMA_TODEVICE);

#ifdef OSK_LOW_TX_DESC
	if (q_num!=BE_QUEUE && q_num!=HIGH_QUEUE)
		txdesc_rollover_nonbe(pdesc, (unsigned int *)tx_head);
	else
#endif
	txdesc_rollover(pdesc, (unsigned int *)tx_head);
one_txdesc:

	rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(get_desc(pfrstdesc->Dword10)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), (get_desc(pfrstdesc->Dword7)&TX_TxBufSizeMask), PCI_DMA_TODEVICE);

#ifdef SUPPORT_SNMP_MIB
	if (txcfg->rts_thrshld <= get_mpdu_len(txcfg, txcfg->fr_len))
		SNMP_MIB_INC(dot11RTSSuccessCount, 1);
#endif

	pfrstdesc->Dword0 |= set_desc(TX_OWN);

#ifndef USE_RTL8186_SDK
	rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(pfrst_dma_desc-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(struct tx_desc), PCI_DMA_TODEVICE);
#endif

#ifdef RESERVE_TXDESC_FOR_EACH_IF
	if (RSVQ_ENABLE && IS_RSVQ(q_num))
		priv->use_txdesc_cnt[RSVQ(q_num)] += (txcfg->one_txdesc)? 1 : 2;
#endif

	if (q_num == HIGH_QUEUE) {
		DEBUG_WARN("signin shortcut for DTIM pkt?\n");
		return;
	} else {
		tx_poll(priv, q_num);
	}

	return;
}
#endif

#if (!defined(__OSK__)) || (defined(__OSK__) && !defined(CONFIG_RTL6028))
__MIPS16
#endif
__IRAM_IN_865X
#if defined(CONFIG_WLAN_HAL_8197F) 
static inline int get_tx_sc_index(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *hdr, int aggre_en)
#else
int get_tx_sc_index(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *hdr, int aggre_en)
#endif
{
	struct tx_sc_entry *ptxsc_entry;
	int i, idx;
	int offset = 0;
	
#ifdef MCAST2UI_REFINE
	if  ((OPMODE & WIFI_AP_STATE)
#ifdef WDS
		&& !(pstat->state & WIFI_WDS)
#endif
		)
		offset = 6;
#endif

	ptxsc_entry = pstat->tx_sc_ent;

#ifdef SUPPORT_TX_AMSDU_SHORTCUT
	if (aggre_en >= FG_AGGRE_MSDU_FIRST)
	{
		idx = pstat->tx_sc_amsdu_idx_cache;
		for (i = TX_SC_ENTRY_NUM; i < MAX_TXSC_ENTRY; i++) {
			if (!rtk_memcmp(hdr+offset, (unsigned char *)(&ptxsc_entry[idx].ethhdr)+offset, sizeof(struct wlan_ethhdr_t)-offset)) {
				pstat->tx_sc_amsdu_idx_cache = idx;
				return idx;
			}
			idx = (idx == MAX_TXSC_ENTRY-1) ? TX_SC_ENTRY_NUM : (idx+1);
		}
	}
	else
#endif
	{
		idx = pstat->tx_sc_idx_cache;
		for (i = 0; i < TX_SC_ENTRY_NUM; i++) {
			if (!rtk_memcmp(hdr+offset, (unsigned char *)(&ptxsc_entry[idx].ethhdr)+offset, sizeof(struct wlan_ethhdr_t)-offset)) {
				pstat->tx_sc_idx_cache = idx;
				return idx;
			}
			idx = ((idx+1) % TX_SC_ENTRY_NUM);
		}
	}

	return -1;
} 


#ifdef CONFIG_RTL8672
#if (!defined(__OSK__)) || (defined(__OSK__) && !defined(CONFIG_RTL6028))
__MIPS16
#endif
__IRAM_IN_865X
#endif
#if defined(CONFIG_WLAN_HAL_8197F) 
static inline int get_tx_sc_free_entry(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *hdr, int aggre_en)
#else
int get_tx_sc_free_entry(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *hdr, int aggre_en)
#endif
{
	struct tx_sc_entry *ptxsc_entry;
	int i;

	i = get_tx_sc_index(priv, pstat, hdr, aggre_en);
	if (i >= 0)
		return i;
	
	ptxsc_entry = pstat->tx_sc_ent;

#ifdef SUPPORT_TX_AMSDU_SHORTCUT
	if (aggre_en == FG_AGGRE_MSDU_FIRST) {
		for (i = TX_SC_ENTRY_NUM; i < MAX_TXSC_ENTRY; i++) {
			if (ptxsc_entry[i].txcfg.fr_len == 0)
				return i;
		}
		
		// no free entry
		i = pstat->tx_sc_amsdu_replace_idx;
		if (pstat->tx_sc_amsdu_replace_idx == MAX_TXSC_ENTRY-1)
			pstat->tx_sc_amsdu_replace_idx = TX_SC_ENTRY_NUM ;
		else
			pstat->tx_sc_amsdu_replace_idx++;
	}
	else
#endif
	{
		for (i=0; i<TX_SC_ENTRY_NUM; i++) {
			if (ptxsc_entry[i].txcfg.fr_len == 0)
				return i;
		}
		
		// no free entry
		i = pstat->tx_sc_replace_idx;
		pstat->tx_sc_replace_idx = (pstat->tx_sc_replace_idx + 1) % TX_SC_ENTRY_NUM;
	}

	return i;
}
#endif // CONFIG_PCI_HCI
#endif // TX_SHORTCUT

#if (BEAMFORMING_SUPPORT == 1)
BOOLEAN
IsMgntNDPA(
	pu1Byte		pdu
)
{
	BOOLEAN ret = 0;
	if(IsMgntActionNoAck(pdu) && GET_80211_HDR_ORDER(pdu))
	{
		if(GET_HT_CTRL_NDP_ANNOUNCEMENT(pdu+sMacHdrLng) == 1)
			ret = 1;
	}
	return ret;
}
#endif

static int 
rtl88XX_tx_recycle(
    struct rtl8192cd_priv   *priv, 
    unsigned int            txRingIdx, 
    int                     *recycleCnt_p
)
{
	int	                        cnt = 0;
	struct tx_desc_info         *pdescinfoH, *pdescinfo;
    // TODO: int or u2Byte ?
	//volatile int	            head, tail;
    volatile u2Byte             head, tail;
	int				            needRestartQueue = 0;
	int				            recycleCnt = 0;
#ifdef AP_SWPS_OFFLOAD    
    int                         pkt_Qos;
    u1Byte                      recycle_condition = 0;
    int                         pkt_insert_position = -1;
    u1Byte                      AMSDU_flag=0;
    int                         pkt_info_result;
    unsigned char               amsdu_i;
#endif
#ifdef PCIE_POWER_SAVING_TEST // yllin
    if((priv->pwr_state >= L2) || (priv->pwr_state == L1)) {
        return 0; 
    }
#endif
#ifndef SMP_SYNC
        unsigned long flags;
#endif
    
    unsigned int                halQnum = GET_HAL_INTERFACE(priv)->MappingTxQueueHandler(priv, txRingIdx);
    PHCI_TX_DMA_MANAGER_88XX    ptx_dma;

    ptx_dma = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PTxDMA88XX);
    head = GET_HAL_INTERFACE(priv)->GetTxQueueHWIdxHandler(priv, txRingIdx);
    tail = ptx_dma->tx_queue[halQnum].hw_idx;
	pdescinfoH	= get_txdesc_info(priv,priv->pshare->pdesc_info, txRingIdx);
#if 0    
    if((strcmp(priv->dev->name, "wlan0")==0))
        printk("this is interface wlan0\n");
    else
        printk("this is interface wlan1\n");
#endif

	while (tail != head)
	{
		pdescinfo = pdescinfoH + (tail);

#if defined(CONFIG_NET_PCI) && !defined(USE_RTL8186_SDK)
		if (IS_PCIBIOS_TYPE) {
			if (_RESERVED_FRAME_TYPE_ != pdescinfo->type) {
				pci_unmap_single(priv->pshare->pdev, pdescinfo->paddr, pdescinfo->len, PCI_DMA_TODEVICE);
			}
		}
#endif

#ifdef AP_SWPS_OFFLOAD
        if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv)) {
            recycle_condition=ERROR_STATE; //init

            //SAVE_INT_AND_CLI(flags); //already disbale interrupt in rtl88XX_tx_dsr
            pkt_Qos = pdescinfo->SWPS_pkt_Qos;
            printk("[%s]pdescinfo->SWPS_sequence=%x, Pkt_Qos=%x\n",__FUNCTION__,pdescinfo->SWPS_sequence,pkt_Qos);  //yllinSWPSprint

            if(pkt_Qos == -2)
                recycle_condition = RECYCLE;
            else if(pdescinfo->swps_pstat == NULL){
                //printk("ERROR, pdescinfo->swps_pstat is NULL.\n");//debug
                {
                    printk("ERROR, recycle break, this queue type will never recycle!! pkt_Qos=%x\n",pkt_Qos);
                    //none-AC packet may not have swps_pstat
                    break; //to avoid core dump
                }
            }else if(pdescinfo->SWPS_sequence!=0){
                recycle_condition = SWPS_check_recycle_condition(priv,pdescinfo->SWPS_sequence,pdescinfo->swps_pstat->RPT_TxDone_SEQ[pkt_Qos],pdescinfo->swps_pstat->RPT_reprepare_SEQ[pkt_Qos],pdescinfo->swps_pstat->SWPS_seq_head[pkt_Qos],pdescinfo->swps_pstat->SWPS_seq_tail[pkt_Qos]);
                if((recycle_condition==RECYCLE) || (recycle_condition==REPREPARE)) //recycle
                {

                    if(pdescinfo->SWPS_sequence==MAX_SWPS_SEQ)
                        pdescinfo->swps_pstat->SWPS_seq_tail[pkt_Qos]=1;
                    else
                        pdescinfo->swps_pstat->SWPS_seq_tail[pkt_Qos]=pdescinfo->SWPS_sequence+1;
                    
                }
                else if(recycle_condition==STOP_RECYCLE)
                    break;
                else if(recycle_condition==ERROR_STATE)//error state,debug
                    printk("recycle condition=0...ERROR!\n");
                
            }else if(pdescinfo->SWPS_sequence == 0){
                recycle_condition==RECYCLE;
            }
            else{
                printk("ERROR: recycle_condition is ERROR_STATE\n");//yllinSWPSprint
            }
            //RESTORE_INT(flags);
        }
#endif

		if (pdescinfo->type == _SKB_FRAME_TYPE_)
		{
#ifdef MP_TEST 
			if (OPMODE & WIFI_MP_CTX_BACKGROUND) {
                #if 0
                printk("skb_tail: 0x%x, skb_head: 0x%x\n", 
                                priv->pshare->skb_tail,
                                priv->pshare->skb_head
                                );
                #endif
                
				struct sk_buff *skb = (struct sk_buff *)(pdescinfo->pframe);
				skb->data = skb->head;
				skb_reset_tail_pointer(skb);
				skb->len = 0;
				priv->pshare->skb_tail = (priv->pshare->skb_tail + 1) & (NUM_MP_SKB - 1);
			}
			else
#endif
			{
#if defined(__LINUX_2_6__) || defined(__OSK__) || defined(TAROKO_0)
#ifdef AP_SWPS_OFFLOAD     
                if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv)) {
                    //SAVE_INT_AND_CLI(flags); //already disbale interrupt in rtl88XX_tx_dsr
                    
                    
                    if(pkt_Qos < 0){
                        printk("[%s][%d]recycle not AC packet, packet Qos =%d\n",__FUNCTION__,__LINE__, pkt_Qos);
                        rtl_kfree_skb(pdescinfo->priv, (struct sk_buff *)(pdescinfo->pframe), _SKB_TX_IRQ_);
                    }
                    
                    if(pkt_Qos < 0){
                        //no action, already recycled
                    }
                    else if ((pdescinfo->pframe && (pdescinfo->SWPS_sequence==0)) || (pdescinfo->pframe && recycle_condition==RECYCLE))
                        rtl_kfree_skb(pdescinfo->priv, (struct sk_buff *)(pdescinfo->pframe), _SKB_TX_IRQ_);
                    else if(pdescinfo->pframe && recycle_condition==REPREPARE){ //4 
                        if(SWPS_dz_queue_insert(priv,pdescinfo,pdescinfo->pframe,&pkt_insert_position)!=TRUE)
                            printk("[%s][%d]insert packet at AC dz_queue head fail.\n",__FUNCTION__,__LINE__);
                        else
                            pdescinfo->pframe = NULL;
                    }
                    else if(pdescinfo->swps_pstat->SWPS_last_seq[pkt_Qos]==0 && pdescinfo->swps_pstat->PS_get_SWPSRPT==1){
                        pdescinfo->swps_pstat->prepare_done[pkt_Qos]=1;
                    }
                    else
                        priv->ext_stats.freeskb_err++;
                    //RESTORE_INT(flags);
                }else
#endif               
				if (pdescinfo->pframe)
#ifdef ENABLE_RTL_SKB_STATS	
					rtl_kfree_skb(pdescinfo->priv, (struct sk_buff *)(pdescinfo->pframe), _SKB_TX_IRQ_);
#else
					rtl_kfree_skb(priv, (struct sk_buff *)(pdescinfo->pframe), _SKB_TX_IRQ_);
#endif
				else
					priv->ext_stats.freeskb_err++;
#else
// for debug ------------
//				rtl_kfree_skb(pdescinfo->priv, (struct sk_buff *)(pdescinfo->pframe), _SKB_TX_IRQ_);
				if (pdescinfo->pframe) {
    				if (((struct sk_buff *)pdescinfo->pframe)->list) {
    					DEBUG_ERR("Free tx skb error, skip it!\n");
    					priv->ext_stats.freeskb_err++;
    				}
    				else {
#ifdef ENABLE_RTL_SKB_STATS	
    					rtl_kfree_skb(pdescinfo->priv, (struct sk_buff *)(pdescinfo->pframe), _SKB_TX_IRQ_);
#else
						rtl_kfree_skb(priv, (struct sk_buff *)(pdescinfo->pframe), _SKB_TX_IRQ_);
#endif

    				}
				}
#endif
				needRestartQueue = 1;
			}
		}
		else if (pdescinfo->type == _PRE_ALLOCMEM_)
		{
			release_mgtbuf_to_poll(priv, (UINT8 *)(pdescinfo->pframe));
		}
		else if (pdescinfo->type == _PRE_ALLOCHDR_)
		{
#ifdef AP_SWPS_OFFLOAD		
		    if(recycle_condition != REPREPARE)
#endif                
			release_wlanhdr_to_poll(priv, (UINT8 *)(pdescinfo->pframe));
		}
		else if (pdescinfo->type == _PRE_ALLOCLLCHDR_)
		{
#ifdef AP_SWPS_OFFLOAD		
		    if(recycle_condition != REPREPARE)
#endif                
			release_wlanllchdr_to_poll(priv, (UINT8 *)(pdescinfo->pframe));
		}
		else if (pdescinfo->type == _PRE_ALLOCICVHDR_)
		{
#ifdef AP_SWPS_OFFLOAD		
    		if(recycle_condition != REPREPARE)
#endif                
			release_icv_to_poll(priv, (UINT8 *)(pdescinfo->pframe));
		}
		else if (pdescinfo->type == _PRE_ALLOCMICHDR_)
		{
#ifdef AP_SWPS_OFFLOAD		
    		if(recycle_condition != REPREPARE)
#endif                
			release_mic_to_poll(priv, (UINT8 *)(pdescinfo->pframe));
		}
		else if (pdescinfo->type == _RESERVED_FRAME_TYPE_)
		{
			// the chained skb, no need to release memory
		}
		else
		{
			DEBUG_ERR("Unknown tx frame type %d\n", pdescinfo->type);
		}
#ifdef AP_SWPS_OFFLOAD
        if(recycle_condition != REPREPARE)
#endif            
        {
        //Reset to default value
        pdescinfo->type     = _RESERVED_FRAME_TYPE_;
		// for skb buffer free
		pdescinfo->pframe   = NULL;
        recycleCnt++;
        }


		for (cnt =0; cnt<TXBD_ELE_NUM-2; cnt++) {

#if defined(CONFIG_NET_PCI) && !defined(USE_RTL8186_SDK)
			if (IS_PCIBIOS_TYPE) {
				if (_RESERVED_FRAME_TYPE_ != pdescinfo->buf_type[cnt]) {
					//use the paddr and flen of pdesc field for icv, mic case which doesn't fill the pdescinfo
					pci_unmap_single(priv->pshare->pdev,
									 pdescinfo->buf_paddr[cnt],//payload
									 pdescinfo->buf_len[cnt],
									 PCI_DMA_TODEVICE);
				}
			}
#endif
			//if (txRingIdx == 2)
			//panic_printk("buf type[%d]=%d\n", cnt, pdescinfo->buf_type[cnt]);

			if (pdescinfo->buf_type[cnt] == _SKB_FRAME_TYPE_)
			{
#ifdef MP_TEST
				if (OPMODE & WIFI_MP_CTX_BACKGROUND) {
					struct sk_buff *skb = (struct sk_buff *)(pdescinfo->buf_pframe[cnt]);
					skb->data = skb->head;
					skb_reset_tail_pointer(skb);
					skb->len = 0;
					priv->pshare->skb_tail = (priv->pshare->skb_tail + 1) & (NUM_MP_SKB - 1);
				}
				else
#endif
				{
#if defined(__LINUX_2_6__) || defined(__OSK__) || defined(TAROKO_0)
#ifdef AP_SWPS_OFFLOAD 
                    if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv)) {
                        //SAVE_INT_AND_CLI(flags); //already disbale interrupt in rtl88XX_tx_dsr
                        
                        if(pkt_Qos < 0){
                            printk("[%s][%d]recycle not AC packet, packet Qos =%d\n",__FUNCTION__,__LINE__, pkt_Qos);
                            rtl_kfree_skb(pdescinfo->priv, (struct sk_buff *)(pdescinfo->buf_pframe[cnt]), _SKB_TX_IRQ_);
                        }
                        
                        if(pkt_Qos < 0){
                            //no action, already recycled
                        }
                        else if ((pdescinfo->buf_pframe[cnt] && (pdescinfo->SWPS_sequence==0)) || (pdescinfo->buf_pframe[cnt] && recycle_condition==RECYCLE))
                            rtl_kfree_skb(pdescinfo->priv, (struct sk_buff *)(pdescinfo->buf_pframe[cnt]), _SKB_TX_IRQ_);
                        else if(pdescinfo->buf_pframe[cnt] && recycle_condition==REPREPARE){ //4 
                            if(SWPS_dz_queue_insert(priv,pdescinfo,pdescinfo->buf_pframe[cnt],&pkt_insert_position)!=TRUE)
                                printk("[%s][%d]insert packet at AC dz_queue head fail.\n",__FUNCTION__,__LINE__);
                            else
                                pdescinfo->buf_pframe[cnt]=NULL;
                        }else if(pdescinfo->swps_pstat->SWPS_last_seq[pkt_Qos]==0 && pdescinfo->swps_pstat->PS_get_SWPSRPT==1){
                            pdescinfo->swps_pstat->prepare_done[pkt_Qos]=1;
                            //printk("no packet to prepare, set prepare_done[%x]=1\n",pkt_Qos);
                        }
                        else 
                            priv->ext_stats.freeskb_err++;
                        //RESTORE_INT(flags);
                    }else
#endif                    
					if (pdescinfo->buf_pframe[cnt])
#ifdef ENABLE_RTL_SKB_STATS	
						rtl_kfree_skb(pdescinfo->priv, (struct sk_buff *)(pdescinfo->buf_pframe[cnt]), _SKB_TX_IRQ_);
#else
						rtl_kfree_skb(priv, (struct sk_buff *)(pdescinfo->buf_pframe[cnt]), _SKB_TX_IRQ_);
#endif
					else
						priv->ext_stats.freeskb_err++;
#else
// for debug ------------
//					rtl_kfree_skb(pdescinfo->priv, (struct sk_buff *)(pdescinfo->pframe), _SKB_TX_IRQ_);
					if (pdescinfo->buf_pframe[cnt]) {
					if (((struct sk_buff *)pdescinfo->buf_pframe[cnt])->list) {
						DEBUG_ERR("Free tx skb error, skip it!\n");
						priv->ext_stats.freeskb_err++;
					}
					else
#ifdef ENABLE_RTL_SKB_STATS	
						rtl_kfree_skb(pdescinfo->priv, (struct sk_buff *)(pdescinfo->buf_pframe[cnt]), _SKB_TX_IRQ_);
#else
						rtl_kfree_skb(priv, (struct sk_buff *)(pdescinfo->buf_pframe[cnt]), _SKB_TX_IRQ_);
#endif
					}
#endif
					needRestartQueue = 1;
				}
			}
			else if (pdescinfo->buf_type[cnt] == _PRE_ALLOCMEM_)
			{
				release_mgtbuf_to_poll(priv, (UINT8 *)(pdescinfo->buf_pframe[cnt]));
			}
			else if (pdescinfo->buf_type[cnt] == _PRE_ALLOCHDR_)
			{
#ifdef AP_SWPS_OFFLOAD			
			    if(recycle_condition != REPREPARE)
#endif                    
				release_wlanhdr_to_poll(priv, (UINT8 *)(pdescinfo->buf_pframe[cnt]));
			}
			else if (pdescinfo->buf_type[cnt] == _PRE_ALLOCLLCHDR_)
			{
#ifdef AP_SWPS_OFFLOAD			
    			if(recycle_condition != REPREPARE)
#endif                    
				release_wlanllchdr_to_poll(priv, (UINT8 *)(pdescinfo->buf_pframe[cnt]));
			}
			else if (pdescinfo->buf_type[cnt] == _PRE_ALLOCICVHDR_)
			{
#ifdef AP_SWPS_OFFLOAD			
    			if(recycle_condition != REPREPARE)
#endif                    
				release_icv_to_poll(priv, (UINT8 *)(pdescinfo->buf_pframe[cnt]));
			}
			else if (pdescinfo->buf_type[cnt] == _PRE_ALLOCMICHDR_)
			{
#ifdef AP_SWPS_OFFLOAD			
    			if(recycle_condition != REPREPARE)
#endif                    
				release_mic_to_poll(priv, (UINT8 *)(pdescinfo->buf_pframe[cnt]));
			}
			else if (pdescinfo->buf_type[cnt] == _RESERVED_FRAME_TYPE_)
			{
				// the chained skb, no need to release memory
			}
			else
			{
				DEBUG_ERR("Unknown tx frame type %d\n", pdescinfo->buf_type[cnt]);
			}
#ifdef AP_SWPS_OFFLOAD            
            if(recycle_condition != REPREPARE || pdescinfo->buf_type[cnt] != _SKB_FRAME_TYPE_)
#endif                
            {
            //Reset to default value
            pdescinfo->buf_type[cnt]    = _RESERVED_FRAME_TYPE_;
			// for skb buffer free
            pdescinfo->buf_pframe[cnt]  = NULL;
            recycleCnt ++;
            }

		}

#ifdef WLAN_HAL_TX_AMSDU
		{
			for (cnt =0; cnt<pdescinfo->amsdu_num; cnt++) {
				//if (txRingIdx == 2)
				//panic_printk("buf type[%d]=%d\n", cnt, pdescinfo->buf_type[cnt]);

				if (pdescinfo->buf_type_amsdu[cnt] == _SKB_FRAME_TYPE_)
				{

#if defined(CONFIG_NET_PCI) && !defined(USE_RTL8186_SDK)
					if (IS_PCIBIOS_TYPE) {
						pci_unmap_single(priv->pshare->pdev, pdescinfo->buf_paddr_amsdu[cnt], pdescinfo->buf_len_amsdu[cnt], PCI_DMA_TODEVICE);
					}
#endif

#if 0 /*def MP_TEST*/
					if (OPMODE & WIFI_MP_CTX_BACKGROUND) {
						struct sk_buff *skb = (struct sk_buff *)(pdescinfo->buf_pframe[cnt]);
						skb->data = skb->head;
						skb_reset_tail_pointer(skb);
						skb->len = 0;
						priv->pshare->skb_tail = (priv->pshare->skb_tail + 1) & (NUM_MP_SKB - 1);
					}
					else
#endif
					{
#if defined(__LINUX_2_6__) || defined(__OSK__) || defined(TAROKO_0)
#ifdef AP_SWPS_OFFLOAD 
                    if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv)) {
                        //SAVE_INT_AND_CLI(flags); //already disbale interrupt in rtl88XX_tx_dsr
                        if(pkt_Qos < 0){
                            printk("[%s][%d]recycle not AC packet, packet Qos =%d\n",__FUNCTION__,__LINE__, pkt_Qos);
                            rtl_kfree_skb(pdescinfo->priv, (struct sk_buff *)(pdescinfo->buf_pframe_amsdu[cnt]), _SKB_TX_IRQ_);
                        }
                        
                        if(pkt_Qos < 0){
                            //no action, already recycled
                        }
                        else if ((pdescinfo->buf_pframe_amsdu[cnt] && (pdescinfo->SWPS_sequence==0)) || (pdescinfo->buf_pframe_amsdu[cnt] && recycle_condition==RECYCLE))
                            rtl_kfree_skb(pdescinfo->priv, (struct sk_buff *)(pdescinfo->buf_pframe_amsdu[cnt]), _SKB_TX_IRQ_);
                        else if(pdescinfo->buf_pframe_amsdu[cnt] && recycle_condition==REPREPARE){ //4 
                            AMSDU_flag = 1;
                            if(cnt == 0){
                                if(SWPS_dz_queue_insert(priv,pdescinfo,&pdescinfo->buf_pframe_amsdu[0],&pkt_insert_position)!=TRUE)
                                    printk("[%s][%d]insert packet at AC dz_queue head fail.\n",__FUNCTION__,__LINE__);
                                else
                                    pdescinfo->buf_pframe_amsdu[0] = pdescinfo->buf_pframe_amsdu[1] = pdescinfo->buf_pframe_amsdu[2] = pdescinfo->buf_pframe_amsdu[3] = NULL;
                                
                            }
                        }else if(pdescinfo->swps_pstat->SWPS_last_seq[pkt_Qos]==0 && pdescinfo->swps_pstat->PS_get_SWPSRPT==1){
                            pdescinfo->swps_pstat->prepare_done[pkt_Qos]=1;
                        }
                        else 
                            priv->ext_stats.freeskb_err++;
                        //RESTORE_INT(flags);
                    }else
                    
#endif                    
						if(pdescinfo->buf_pframe_amsdu[cnt])
#ifdef ENABLE_RTL_SKB_STATS						
							rtl_kfree_skb(pdescinfo->priv, (struct sk_buff *)(pdescinfo->buf_pframe_amsdu[cnt]), _SKB_TX_IRQ_);
#else
							rtl_kfree_skb(priv, (struct sk_buff *)(pdescinfo->buf_pframe_amsdu[cnt]), _SKB_TX_IRQ_);					
#endif
						else
							priv->ext_stats.freeskb_err++;
#else
	// for debug ------------
	//					rtl_kfree_skb(pdescinfo->priv, (struct sk_buff *)(pdescinfo->pframe), _SKB_TX_IRQ_);
						if (pdescinfo->buf_pframe_amsdu[cnt]) {
						if (((struct sk_buff *)pdescinfo->buf_pframe_amsdu[cnt])->list) {
							DEBUG_ERR("Free tx skb error, skip it!\n");
							priv->ext_stats.freeskb_err++;
						}
						else
#ifdef ENABLE_RTL_SKB_STATS		
							rtl_kfree_skb(pdescinfo->priv, (struct sk_buff *)(pdescinfo->buf_pframe_amsdu[cnt]), _SKB_TX_IRQ_);
#else
							rtl_kfree_skb(priv, (struct sk_buff *)(pdescinfo->buf_pframe_amsdu[cnt]), _SKB_TX_IRQ_);
#endif

						}
#endif
						needRestartQueue = 1;
					}
				}
#if 0
				else if (pdescinfo->buf_type_amsdu[cnt] == _PRE_ALLOCMEM_)
				{
					release_mgtbuf_to_poll(priv, (UINT8 *)(pdescinfo->buf_pframe_amsdu[cnt]));
				}
				else if (pdescinfo->buf_type_amsdu[cnt] == _PRE_ALLOCHDR_)
				{
					release_wlanhdr_to_poll(priv, (UINT8 *)(pdescinfo->buf_pframe_amsdu[cnt]));
				}
				else if (pdescinfo->buf_type_amsdu[cnt] == _PRE_ALLOCLLCHDR_)
				{
					release_wlanllchdr_to_poll(priv, (UINT8 *)(pdescinfo->buf_pframe_amsdu[cnt]));
				}
				else if (pdescinfo->buf_type_amsdu[cnt] == _PRE_ALLOCICVHDR_)
				{
					release_icv_to_poll(priv, (UINT8 *)(pdescinfo->buf_pframe_amsdu[cnt]));
				}
				else if (pdescinfo->buf_type_amsdu[cnt] == _PRE_ALLOCMICHDR_)
				{
					release_mic_to_poll(priv, (UINT8 *)(pdescinfo->buf_pframe_amsdu[cnt]));
				}
#endif
				else if (pdescinfo->buf_type_amsdu[cnt] == _RESERVED_FRAME_TYPE_)
				{
					// the chained skb, no need to release memory
				}
				else
				{
				    printk("%s(%d): Unknow tx frame type for AMSDU: %d \n", __FUNCTION__, __LINE__, pdescinfo->buf_type_amsdu[cnt]);
					DEBUG_ERR("Unknown tx frame type %d\n", pdescinfo->buf_type_amsdu[cnt]);
				}
#ifdef AP_SWPS_OFFLOAD            
            if(recycle_condition != REPREPARE || pdescinfo->buf_type[cnt] != _SKB_FRAME_TYPE_)
#endif                
            {

	            //Reset to default value
	            pdescinfo->buf_type_amsdu[cnt]    = _RESERVED_FRAME_TYPE_;
				// for skb buffer free
	            pdescinfo->buf_pframe_amsdu[cnt]  = NULL;

    			recycleCnt ++;
}
			}
		}
#endif // WLAN_HAL_TX_AMSDU

#ifdef AP_SWPS_OFFLOAD 
        if(recycle_condition == REPREPARE){
            printk("pkt_insert_position=%d\n",pkt_insert_position);  //yllinSWPSprint
            pkt_info_result = SWPS_reprepare_pktinfo_enqueue(priv,pdescinfo,pkt_Qos,pkt_insert_position,AMSDU_flag);
            if(!pkt_info_result)
                printk("Reprepare pkt info fail.......\n");//debug
        }else if(recycle_condition == RECYCLE || recycle_condition == ERROR_STATE){
            //reset to 0
            pdescinfo->hdr_len = 0;
            pdescinfo->mic_icv_len = 0;
            pdescinfo->skbbuf_len = 0;
            pdescinfo->PSB_len = 0;
            //pdescinfo->ptxdesc = NULL;
            pdescinfo->isWiFiHdr = 0;
            pdescinfo->SWPS_sequence = 0;
            pdescinfo->SWPS_pkt_Qos = 0;
            pdescinfo->swps_pstat = NULL;
#if CFG_HAL_SUPPORT_TXDESC_IE
            pdescinfo->IE_bitmap = 0;
#endif            
#ifdef WLAN_HAL_TX_AMSDU            
            pdescinfo->amsdu_num = 0;
            for(amsdu_i = 0;amsdu_i<WLAN_HAL_TX_AMSDU_MAX_NUM;amsdu_i++)
                pdescinfo->amsdubuf_len[amsdu_i] = 0;
#endif                
            //if(pdescinfo->ptxdesc)
                //kfree(pdescinfo->ptxdesc);
        }
#endif
    	tail = (tail + 1) % (ptx_dma->tx_queue[halQnum].total_txbd_num);
        ptx_dma->tx_queue[halQnum].avail_txbd_num++;

#ifdef RESERVE_TXDESC_FOR_EACH_IF
		if (RSVQ_ENABLE && IS_RSVQ(txRingIdx))
			pdescinfo->priv->use_txdesc_cnt[RSVQ(txRingIdx)]--;
#endif

		if (head == tail)
			head = GET_HAL_INTERFACE(priv)->GetTxQueueHWIdxHandler(priv, txRingIdx);
	}


    ptx_dma->tx_queue[halQnum].hw_idx = tail;

	if (recycleCnt_p)
		*recycleCnt_p = recycleCnt;

	return needRestartQueue;
}

#ifdef TAROKO_0
__PE_IMEM__
void rtl88xx_tx_recycle_pe(struct rtl8192cd_priv   *priv)
{
	int i;
	{
		for(i = 1;i < 5;i++)
			rtl88XX_tx_recycle(priv, i, NULL);
	}

}
#endif


/*
	Procedure to re-cycle TXed packet in Queue index "txRingIdx"

	=> Return value means if system need restart-TX-queue or not.

		1: Need Re-start Queue
		0: Don't Need Re-start Queue
*/

static int rtl8192cd_tx_recycle(struct rtl8192cd_priv *priv, unsigned int txRingIdx, int *recycleCnt_p)
{
	struct tx_desc	*pdescH, *pdesc;
	struct tx_desc_info *pdescinfoH, *pdescinfo;
	volatile int	head, tail;
	struct rtl8192cd_hw	*phw=GET_HW(priv);
	int				needRestartQueue=0;
	int				recycleCnt=0;
#ifdef CONFIG_WLAN_HAL
    if (IS_HAL_CHIP(priv)) {
        return rtl88XX_tx_recycle(priv, txRingIdx, recycleCnt_p);
    } else if (CONFIG_WLAN_NOT_HAL_EXIST)
#endif //CONFIG_WLAN_HAL
	{//not HAL
	head		= get_txhead(phw, txRingIdx);
	tail		= get_txtail(phw, txRingIdx);
	pdescH		= get_txdesc(phw, txRingIdx);
	pdescinfoH	= get_txdesc_info(priv,priv->pshare->pdesc_info, txRingIdx);

	while (tail != head)
	{
		pdesc = pdescH + (tail);
		pdescinfo = pdescinfoH + (tail);

#ifdef __MIPSEB__
		pdesc = (struct tx_desc *)KSEG1ADDR(pdesc);
#endif

		if (!pdesc || (get_desc(pdesc->Dword0) & TX_OWN))
			break;
		if (pdescinfo->pframe == NULL) {
			DEBUG_ERR("Free Null Buf: head=%d tail=%d recycleCnt=%d RingIdx=%d!\n", head, tail, recycleCnt, txRingIdx);
		}

#ifdef CONFIG_PCI_HCI
		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
#if defined(CONFIG_NET_PCI) && !defined(USE_RTL8186_SDK)
			if (IS_PCIBIOS_TYPE) {
				//use the paddr and flen of pdesc field for icv, mic case which doesn't fill the pdescinfo
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
				if(GET_CHIP_VER(priv)== VERSION_8812E || GET_CHIP_VER(priv) == VERSION_8723B)	
				pci_unmap_single(priv->pshare->pdev,
								 get_desc(pdesc->Dword10),
								 (get_desc(pdesc->Dword7)&0xffff),
								 PCI_DMA_TODEVICE);
				else
#endif
				pci_unmap_single(priv->pshare->pdev,
								 get_desc(pdesc->Dword8),
								 (get_desc(pdesc->Dword7)&0xffff),
								 PCI_DMA_TODEVICE);
			}
#endif
		}
#endif // CONFIG_PCI_HCI

		if (pdescinfo->type == _SKB_FRAME_TYPE_)
		{
#ifdef MP_TEST
			if (OPMODE & WIFI_MP_CTX_BACKGROUND) {
				struct sk_buff *skb = (struct sk_buff *)(pdescinfo->pframe);
				skb->data = skb->head;
				skb_reset_tail_pointer(skb);
				skb->len = 0;
				priv->pshare->skb_tail = (priv->pshare->skb_tail + 1) & (NUM_MP_SKB - 1);
			}
			else
#endif
			{
#if defined(__LINUX_2_6__) || defined(__OSK__)  || defined(TAROKO_0)
#ifdef CONFIG_ENABLE_NCBUFF
				rtl_kfree_skb_spec(pdescinfo->priv, (struct sk_buff *)(pdescinfo->pframe), _SKB_TX_IRQ_);
#else 
				if (pdescinfo->pframe)
#ifdef ENABLE_RTL_SKB_STATS					
					rtl_kfree_skb(pdescinfo->priv, (struct sk_buff *)(pdescinfo->pframe), _SKB_TX_IRQ_);
#else
					rtl_kfree_skb(priv, (struct sk_buff *)(pdescinfo->pframe), _SKB_TX_IRQ_);				
#endif
				else
					priv->ext_stats.freeskb_err++;
#endif
#else
// for debug ------------
//				rtl_kfree_skb(pdescinfo->priv, (struct sk_buff *)(pdescinfo->pframe), _SKB_TX_IRQ_);
				if (pdescinfo->pframe) {
				if (((struct sk_buff *)pdescinfo->pframe)->list) {
					DEBUG_ERR("Free tx skb error, skip it!\n");
					priv->ext_stats.freeskb_err++;
				}
				else
					rtl_kfree_skb(pdescinfo->priv, (struct sk_buff *)(pdescinfo->pframe), _SKB_TX_IRQ_);
				}
#endif
				needRestartQueue = 1;
			}
		}
		else if (pdescinfo->type == _PRE_ALLOCMEM_)
		{
			release_mgtbuf_to_poll(priv, (UINT8 *)(pdescinfo->pframe));
		}
		else if (pdescinfo->type == _PRE_ALLOCHDR_)
		{
			release_wlanhdr_to_poll(priv, (UINT8 *)(pdescinfo->pframe));
		}
		else if (pdescinfo->type == _PRE_ALLOCLLCHDR_)
		{
			release_wlanllchdr_to_poll(priv, (UINT8 *)(pdescinfo->pframe));
		}
		else if (pdescinfo->type == _PRE_ALLOCICVHDR_)
		{
			release_icv_to_poll(priv, (UINT8 *)(pdescinfo->pframe));
		}
		else if (pdescinfo->type == _PRE_ALLOCMICHDR_)
		{
			release_mic_to_poll(priv, (UINT8 *)(pdescinfo->pframe));
		}
		else if (pdescinfo->type == _RESERVED_FRAME_TYPE_)
		{
			// the chained skb, no need to release memory
		}
		else
		{
			DEBUG_ERR("Unknown tx frame type %d\n", pdescinfo->type);
		}

		// for skb buffer free
		pdescinfo->type = _RESERVED_FRAME_TYPE_;
		pdescinfo->pframe = NULL;

		recycleCnt ++;

#ifdef OSK_LOW_TX_DESC
		tail = (tail + 1) % ((txRingIdx==BE_QUEUE || txRingIdx==HIGH_QUEUE)?BE_TXDESC:NONBE_TXDESC);
#else
		tail = (tail + 1) % CURRENT_NUM_TX_DESC;
#endif

#ifdef RESERVE_TXDESC_FOR_EACH_IF
		if (RSVQ_ENABLE && IS_RSVQ(txRingIdx))
			pdescinfo->priv->use_txdesc_cnt[RSVQ(txRingIdx)]--;
#endif
	}

	*get_txtail_addr(phw, txRingIdx) = tail;

	if (recycleCnt_p)
		*recycleCnt_p = recycleCnt;

	return needRestartQueue;
	} //not HAL
#ifdef CONFIG_WLAN_HAL
	else {
		panic_printk("Shouldn't come here %s()\n", __FUNCTION__);
		return 0;
	}
#endif
}







/*
	Procedure to restart TX Queue
*/
void rtl8192cd_tx_restartQueue(struct rtl8192cd_priv *priv)
{
#if defined(__KERNEL__) || defined(__OSK__)
	priv = GET_ROOT(priv);

	if (IS_DRV_OPEN(priv) && netif_queue_stopped(priv->dev)) {
		DEBUG_INFO("wake-up Root queue\n");
		netif_wake_queue(priv->dev);
	}

#ifdef UNIVERSAL_REPEATER
	if (IS_DRV_OPEN(GET_VXD_PRIV(priv)) && netif_queue_stopped(GET_VXD_PRIV(priv)->dev)) {
		DEBUG_INFO("wake-up VXD queue\n");
		netif_wake_queue(GET_VXD_PRIV(priv)->dev);
	}
#endif

#ifdef MBSSID
	if (priv->pmib->miscEntry.vap_enable) {
		int bssidIdx;
		for (bssidIdx=0; bssidIdx<RTL8192CD_NUM_VWLAN; bssidIdx++) {
			if (IS_DRV_OPEN(priv->pvap_priv[bssidIdx]) && netif_queue_stopped(priv->pvap_priv[bssidIdx]->dev)) {
				DEBUG_INFO("wake-up Vap%d queue\n", bssidIdx);
				netif_wake_queue(priv->pvap_priv[bssidIdx]->dev);
			}
		}
	}
#endif

#ifdef CONFIG_RTK_MESH
    if(priv->mesh_dev) {
		if (netif_running(priv->mesh_dev) && netif_queue_stopped(priv->mesh_dev)) {
			netif_wake_queue(priv->mesh_dev);
		}
    }
#endif
#ifdef WDS
	if (priv->pmib->dot11WdsInfo.wdsEnabled) {
		int i;
		for (i=0; i<priv->pmib->dot11WdsInfo.wdsNum; i++) {
			if (netif_running(priv->wds_dev[i]) &&
				netif_queue_stopped(priv->wds_dev[i])) {
				DEBUG_INFO("wake-up wds[%d] queue\n", i);
				netif_wake_queue(priv->wds_dev[i]);
			}
		}
	}
#endif
#endif
}



/*
	Try to do TX-DSR for only ONE TX-queue ( rtl8192cd_tx_dsr would check for ALL TX queue )
*/
int rtl8192cd_tx_queueDsr(struct rtl8192cd_priv *priv, unsigned int txRingIdx)
{
	int recycleCnt;
#ifndef SMP_SYNC
	unsigned long flags;
#endif
	SAVE_INT_AND_CLI(flags);

	if (rtl8192cd_tx_recycle(priv, txRingIdx, &recycleCnt) == 1)
		rtl8192cd_tx_restartQueue(priv);

	RESTORE_INT(flags);
	return recycleCnt;
}

#ifdef  CONFIG_WLAN_HAL
static void rtl88XX_tx_dsr(unsigned long task_priv)
{
	struct rtl8192cd_priv	*priv = (struct rtl8192cd_priv *)task_priv;
	unsigned int	        j=0;
	unsigned int	        restart_queue=0;
	struct rtl8192cd_hw	    *phw=GET_HW(priv);
	int                     needRestartQueue;
    int                     Queue_max;
	unsigned long           flags;

#if IS_RTL88XX_MAC_V4
    if(_GET_HAL_DATA(priv)->MacVersion.is_MAC_v4)
        Queue_max = HIGH_QUEUE_V2;
#endif
#if IS_RTL88XX_MAC_V1_V2_V3
    if(_GET_HAL_DATA(priv)->MacVersion.is_MAC_v1_v2_v3)
        Queue_max = HIGH_QUEUE;
#endif

	if (!phw)
		return;
    
#ifdef PCIE_POWER_SAVING_TEST //yllin
    if((priv->pwr_state >= L2) || (priv->pwr_state == L1)) {
        priv->pshare->has_triggered_tx_tasklet = 0;
            return;
    }
#endif

#ifdef PCIE_POWER_SAVING
	if ((priv->pwr_state == L2) || (priv->pwr_state == L1)) {
		priv->pshare->has_triggered_tx_tasklet = 0;
		return;
	}
#endif

#ifdef MBSSID
    if (GET_ROOT(priv)->pmib->miscEntry.vap_enable) {
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(priv)) {             
            Queue_max = HIGH_QUEUE15;
        }              
#endif  
#if (IS_RTL88XX_MAC_V1_V2 || IS_RTL8197G_SERIES)
        if (_GET_HAL_DATA(priv)->MacVersion.is_MAC_v1_v2 || IS_HARDWARE_TYPE_8197G(priv)){
        Queue_max = HIGH_QUEUE7;      
        }
#endif

#if IS_RTL88XX_MAC_V4
        if( _GET_HAL_DATA(priv)->MacVersion.is_MAC_v4){
            Queue_max = HIGH_QUEUE15_V1;
        }
#endif
    }
#endif

#ifdef CONFIG_RTL_OFFLOAD_DRIVER
    if(_GET_HAL_DATA(priv)->MacVersion.is_MAC_v4)
        Queue_max = CMD_QUEUE_V2;
#endif

	SAVE_INT_AND_CLI(flags);
	SMP_LOCK_XMIT(flags);

	for(j=0; j<=Queue_max; j++)
	{
#if defined(CONFIG_PE_ENABLE)
		if(priv->pshare->rf_ft_var.manual_pe_enable)
		{
			if((j >= BK_QUEUE) && (j <= VO_QUEUE))
				continue;
		}
#endif    
		needRestartQueue = rtl8192cd_tx_recycle(priv, j, NULL);
		/* If anyone of queue report the TX queue need to be restart : we would set "restart_queue" to process ALL queues */
		if (needRestartQueue == 1)
			restart_queue = 1;
	}

	if (restart_queue)
		rtl8192cd_tx_restartQueue(priv);

#ifdef MP_TEST
#if 1/*def CONFIG_RTL8672*/
	if ((OPMODE & (WIFI_MP_STATE|WIFI_MP_CTX_BACKGROUND|WIFI_MP_CTX_BACKGROUND_STOPPING))
			==(WIFI_MP_STATE|WIFI_MP_CTX_BACKGROUND))
#else //CONFIG_RTL8672
	if ((OPMODE & (WIFI_MP_STATE|WIFI_MP_CTX_BACKGROUND))==(WIFI_MP_STATE|WIFI_MP_CTX_BACKGROUND)) 
#endif //CONFIG_RTL8672
	{
		RESTORE_INT(flags);

		if (getAvailableTXBD(priv, BE_QUEUE) > (CURRENT_NUM_TX_DESC/2)) {
			SMP_UNLOCK_XMIT(flags);
			mp_ctx(priv, (unsigned char *)"tx-isr");
			SMP_LOCK_XMIT(flags);
		}

		SAVE_INT_AND_CLI(flags);
	}
#endif

	SMP_UNLOCK_XMIT(flags);

#if defined(SMP_SYNC) && defined(DELAY_REFILL_RX_BUF)
	SMP_LOCK_RECV(flags);
#endif	
	refill_skb_queue(priv);
#if defined(SMP_SYNC) && defined(DELAY_REFILL_RX_BUF)
	SMP_UNLOCK_RECV(flags);
#endif

	priv->pshare->has_triggered_tx_tasklet = 0;

	RESTORE_INT(flags);
}
#endif  //CONFIG_WLAN_HAL
/*-----------------------------------------------------------------------------
Purpose of tx_dsr:

	For ALL TX queues
		1. Free Allocated Buf
		2. Update tx_tail
		3. Update tx related counters
		4. Restart tx queue if needed
------------------------------------------------------------------------------*/
#ifdef __OSK__
__IRAM_WIFI_PRI3
#endif
void rtl8192cd_tx_dsr(unsigned long task_priv)
{
	struct rtl8192cd_priv	*priv = (struct rtl8192cd_priv *)task_priv;
	unsigned int	j=0;
	unsigned int	restart_queue=0;
	struct rtl8192cd_hw	*phw=GET_HW(priv);
	int needRestartQueue;
	unsigned long flags;

#ifdef CONFIG_WLAN_HAL
    if(IS_HAL_CHIP(priv)){
        rtl88XX_tx_dsr((unsigned long)priv);
        return;
    }else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif //CONFIG_WLAN_HAL
	{//not HAL
	if (!phw)
		return;

#ifdef PCIE_POWER_SAVING
	if ((priv->pwr_state == L2) || (priv->pwr_state == L1)) {
		priv->pshare->has_triggered_tx_tasklet = 0;
		return;
	}
#endif
#ifdef PCIE_POWER_SAVING_TEST
    if ((priv->pwr_state >= L2) || (priv->pwr_state == L1)) {
        priv->pshare->has_triggered_tx_tasklet = 0;
        return;
    }
#endif

	SAVE_INT_AND_CLI(flags);
	SMP_LOCK_XMIT(flags);
	for(j=0; j<=HIGH_QUEUE; j++)
	{
		needRestartQueue = rtl8192cd_tx_recycle(priv, j, NULL);
		/* If anyone of queue report the TX queue need to be restart : we would set "restart_queue" to process ALL queues */
		if (needRestartQueue == 1)
			restart_queue = 1;
	}

	if (restart_queue)
		rtl8192cd_tx_restartQueue(priv);

#ifdef MP_TEST
#if 1/*def CONFIG_RTL8672*/
	if ((OPMODE & (WIFI_MP_STATE|WIFI_MP_CTX_BACKGROUND|WIFI_MP_CTX_BACKGROUND_STOPPING))
			==(WIFI_MP_STATE|WIFI_MP_CTX_BACKGROUND))
#else //CONFIG_RTL8672
	if ((OPMODE & (WIFI_MP_STATE|WIFI_MP_CTX_BACKGROUND))==(WIFI_MP_STATE|WIFI_MP_CTX_BACKGROUND)) 
#endif //CONFIG_RTL8672
	{
		int *tx_head, *tx_tail;
		RESTORE_INT(flags);
		tx_head = get_txhead_addr(phw, BE_QUEUE);
		tx_tail = get_txtail_addr(phw, BE_QUEUE);
		// OSK: because BE_Queue = CURRENT_NUM_TX_DESC, so do not need to change
		if (CIRC_SPACE_RTK(*tx_head, *tx_tail, CURRENT_NUM_TX_DESC) > (CURRENT_NUM_TX_DESC/2))
		{
			SMP_UNLOCK_XMIT(flags);
			mp_ctx(priv, (unsigned char *)"tx-isr");
		    SMP_LOCK_XMIT(flags);
	    }
		SAVE_INT_AND_CLI(flags);
	}
#endif

	SMP_UNLOCK_XMIT(flags);

#if defined(SMP_SYNC) && defined(DELAY_REFILL_RX_BUF)
	SMP_LOCK_RECV(flags);
#endif

	refill_skb_queue(priv);

#if defined(SMP_SYNC) && defined(DELAY_REFILL_RX_BUF)
	SMP_UNLOCK_RECV(flags);
#endif

	priv->pshare->has_triggered_tx_tasklet = 0;

	RESTORE_INT(flags);
}
}


void RtsCheck(struct rtl8192cd_priv *priv, struct tx_insn* txcfg, u4Byte fr_len,
	BOOLEAN *bRtsEnable, BOOLEAN *bCts2SelfEnable, BOOLEAN *bHwRts, BOOLEAN *bErpProtect, BOOLEAN *bNProtect, BOOLEAN *bRtsThreshold)
{
	*bRtsEnable = FALSE;
	*bCts2SelfEnable = FALSE;
	*bHwRts = FALSE;
	*bErpProtect = FALSE;
	*bNProtect = FALSE;
	*bRtsThreshold = FALSE;		// set to TRUE when MPDU > rts_thrshld

	if(!txcfg->pstat)
		return; 

#if (MU_BEAMFORMING_SUPPORT == 1)
	if( priv->pmib->dot11RFEntry.txbf_mu && txcfg->pstat->muPartner_num){
		*bRtsEnable = FALSE;
		*bCts2SelfEnable = FALSE;
		*bHwRts = FALSE;
		return;
	}
#endif

	if ((txcfg->rts_thrshld <= get_mpdu_len(txcfg, fr_len)) ||
		(txcfg->pstat && txcfg->pstat->is_forced_rts))
	{
		*bRtsEnable = TRUE;
		*bRtsThreshold = TRUE;
	}
	else {
		if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) &&
			is_MCS_rate(txcfg->tx_rate) &&
			(priv->ht_protection /*|| txcfg->pstat->is_rtl8190_sta*/))
		{
			*bNProtect = 1;
			if (priv->pmib->dot11ErpInfo.protection)
				*bErpProtect = 1;
#ifdef MULTI_STA_REFINE 					
			if (priv->pshare->total_assoc_num > 10 )	{
				*bRtsEnable = FALSE;
				*bRtsEnable = TRUE;
			} else			
#endif			
			if (priv->pmib->dot11ErpInfo.ctsToSelf)
				*bCts2SelfEnable = TRUE;
			else					
				*bRtsEnable = TRUE;
				
		}
		else if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11G) &&
			(!is_CCK_rate(txcfg->tx_rate)) && // OFDM mode
			priv->pmib->dot11ErpInfo.protection)
		{
			*bErpProtect = 1;
			if (priv->pmib->dot11ErpInfo.ctsToSelf)
				*bCts2SelfEnable = TRUE;
			else						
				*bRtsEnable = TRUE;
			
		}
		else if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) &&
			(txcfg->pstat) && (txcfg->pstat->MIMO_ps & _HT_MIMO_PS_DYNAMIC_) &&
			(is_2T_rate(txcfg->tx_rate) || (txcfg->pstat->tx_avarage>=(3<<17))))
		{	// when HT MIMO Dynamic power save is set, RTS is needed
			*bRtsEnable = TRUE;
		
		} else {
			/*
			 * Auto rts mode, use rts depends on packet length and packet tx time
			 * This feature is enabled when dot11RTSThreshold is set with default value (2347).
			 */

			if(is_MCS_rate(txcfg->tx_rate) && (txcfg->pstat->IOTPeer==HT_IOT_PEER_INTEL) && priv->pshare->rf_ft_var.rts_iot_th)
			{

				*bNProtect = 1;
				if(!txcfg->pstat->no_rts) {
					*bHwRts = TRUE;
					*bRtsEnable = TRUE;
				}
				if(txcfg->pstat->useCts2self)
					*bCts2SelfEnable = TRUE;
			} else
			if (is_MCS_rate(txcfg->tx_rate) /*&& ((txcfg->pstat->IOTPeer!=HT_IOT_PEER_INTEL) || !txcfg->pstat->no_rts)*/) 
			{
#ifdef COCHANNEL_RTS
				if (priv->pmib->miscEntry.auto_rts_mode == 1 && priv->cochannel_to && (txcfg->rts_thrshld == DEFAULT_RTS_THRESHOLD))
				{
					*bNProtect = 1;
					*bRtsEnable = TRUE;
					*bCts2SelfEnable = FALSE;
					*bHwRts = FALSE;
				}
				else
#endif
				{
					*bNProtect = 1;
					*bHwRts = TRUE;
					if(txcfg->pstat->useCts2self)
						*bCts2SelfEnable = TRUE;
					else
						*bRtsEnable = TRUE;
				}
			}
		}
	}
}
#ifdef RTK_AC_SUPPORT
void RtsCheckAC(struct rtl8192cd_priv *priv, struct tx_insn* txcfg, BOOLEAN *bRtsEnable, BOOLEAN *bCts2SelfEnable, BOOLEAN *bHwRts)
{
	if((priv->pshare->rf_ft_var.cca_rts) && (txcfg->pstat->vht_cap_len > 0)) {
		*bHwRts = FALSE;
		*bCts2SelfEnable = FALSE;
		*bRtsEnable = TRUE;
	}
}
#endif

#ifdef CONFIG_PCI_HCI
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
static void rtl8192cd_fill_fwinfo_8812(struct rtl8192cd_priv *priv, struct tx_insn* txcfg, struct tx_desc  *pdesc, unsigned int frag_idx)
{
	char n_txshort = 0, bg_txshort = 0;
	//int erp_protection = 0, n_protection = 0;
	//unsigned char rate;
	unsigned char txRate = 0;
#ifdef DRVMAC_LB
	static unsigned int rate_select = 0;
#endif
	BOOLEAN			bRtsEnable;
	BOOLEAN			bErpProtect;
	BOOLEAN			bNProtect;
	BOOLEAN			bHwRts;
	BOOLEAN			bCts2SelfEnable;
	BOOLEAN			bRtsThreshold;
	unsigned char RtsRate;
	unsigned int	reduction_level = 0;

#ifdef MP_TEST
	if (OPMODE & WIFI_MP_STATE) {
		#ifdef RTK_AC_SUPPORT
		if (is_VHT_rate(txcfg->tx_rate)) {
			txRate = txcfg->tx_rate - VHT_RATE_ID;
			txRate += 44;
		} else 
		#endif
		if (is_MCS_rate(txcfg->tx_rate)) {	// HT rates
			txRate = txcfg->tx_rate - HT_RATE_ID;
			txRate += 12;
		} else{
			txRate = get_rate_index_from_ieee_value((UINT8)txcfg->tx_rate);
		}

		if (priv->pshare->is_40m_bw == 2) {
			pdesc->Dword5 |= set_desc((0 << TXdesc_92E_DataScSHIFT) | (0 << TXdesc_92E_RtsScSHIFT));
			pdesc->Dword5 |= set_desc(0x2 << TXdesc_92E_DataBwSHIFT);

			if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor80M)
				n_txshort = 1;
		} else if (priv->pshare->is_40m_bw == 1) {	
			pdesc->Dword5 |= set_desc((0 << TXdesc_92E_DataScSHIFT) | (0 << TXdesc_92E_RtsScSHIFT));
			pdesc->Dword5 |= set_desc(0x1 << TXdesc_92E_DataBwSHIFT);

			if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor40M)
				n_txshort = 1;
		} else {
			pdesc->Dword5 |= set_desc((0 << TXdesc_92E_DataScSHIFT) | (0 << TXdesc_92E_RtsScSHIFT));
			pdesc->Dword5 |= set_desc(0 << TXdesc_92E_DataBwSHIFT);
			if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor20M)
				n_txshort = 1;
		}

		if (txcfg->retry) {	
			pdesc->Dword4 |= set_desc(TXdesc_92E_RtyLmtEn);
            		pdesc->Dword4 |= set_desc((txcfg->retry  & TXdesc_92E_DataRtyLmtMask) << TXdesc_92E_DataRtyLmtSHIFT);
		}

		pdesc->Dword4 |= set_desc((txRate & TXdesc_92E_DataRateMask) << TXdesc_92E_DataRateSHIFT);
		
		
		if (priv->pmib->dot11RFEntry.phyBandSelect & PHY_BAND_5G)
			pdesc->Dword4 |= set_desc((4 & TX_RtsRateMask_8812) << TX_RtsRateSHIFT_8812);

		if (n_txshort == 1)
			pdesc->Dword5 |= set_desc(TXdesc_92E_DataShort);	

		return;
	}
#endif


	if (is_MCS_rate(txcfg->tx_rate))	// HT rates
	{
		if (txcfg->tx_rate >= VHT_RATE_ID)
		{
			txRate = (txcfg->tx_rate - VHT_RATE_ID) + 44;
		}
		else
		{
			txRate = (txcfg->tx_rate - HT_RATE_ID) + 12;
		}

		if (priv->pshare->is_40m_bw==2)
		{
				
			#ifdef RTK_AC_SUPPORT 
			if (txcfg->pstat && is_VHT_rate(txcfg->tx_rate) && (txcfg->pstat->tx_bw == CHANNEL_WIDTH_80 
				&& (is_auto_rate(priv, txcfg->pstat) || is_fixedVHTTxRate(priv, txcfg->pstat))
			) )
			{
				pdesc->Dword5 |= set_desc((0 << TXdesc_92E_DataScSHIFT) | (0 << TXdesc_92E_RtsScSHIFT));
				pdesc->Dword5 |= set_desc(0x2 << TXdesc_92E_DataBwSHIFT);

				if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor80M &&
					txcfg->pstat && (txcfg->pstat->vht_cap_buf.vht_cap_info & cpu_to_le32(BIT(5))))
						n_txshort = 1;
			}
			else
			#endif
			if (txcfg->pstat && is_MCS_rate(txcfg->tx_rate) && (txcfg->pstat->tx_bw >= CHANNEL_WIDTH_40
					&& (is_auto_rate(priv, txcfg->pstat) || is_fixedMCSTxRate(priv, txcfg->pstat))		
				) )
			{
				#ifdef RTK_AC_SUPPORT
				if(GET_CHIP_VER(priv) != VERSION_8723B)
					pdesc->Dword5 |= set_desc((priv->pshare->txsc_40 << TXdesc_92E_DataScSHIFT) | (priv->pshare->txsc_40 << TXdesc_92E_RtsScSHIFT));
				#endif
				pdesc->Dword5 |= set_desc(0x1 << TXdesc_92E_DataBwSHIFT);

				if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor40M &&
					txcfg->pstat && (txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_40M_)))
						n_txshort = 1;
			}
			else
			{
				#ifdef RTK_AC_SUPPORT
				if(GET_CHIP_VER(priv) != VERSION_8723B)
					pdesc->Dword5 |= set_desc((priv->pshare->txsc_20 << TXdesc_92E_DataScSHIFT) | (priv->pshare->txsc_20 << TXdesc_92E_RtsScSHIFT));
				#endif
				if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor20M &&
					txcfg->pstat && (txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_20M_)))
						n_txshort = 1;
			}
		}
		else
		if (priv->pshare->is_40m_bw) {
			if (txcfg->pstat && (txcfg->pstat->tx_bw == CHANNEL_WIDTH_40)
#ifdef WIFI_11N_2040_COEXIST
				&& !(COEXIST_ENABLE && (((OPMODE & WIFI_AP_STATE) && 
				(priv->bg_ap_timeout || orForce20_Switch20Map(priv)
				))
#ifdef CLIENT_MODE
				|| ((OPMODE & WIFI_STATION_STATE) && priv->coexist_connection && 
				(txcfg->pstat->ht_ie_len) && !(txcfg->pstat->ht_ie_buf.info0 & _HTIE_STA_CH_WDTH_))
#endif
				))
#endif

				) {
					
				pdesc->Dword5 |= set_desc((1 << TXdesc_92E_DataBwSHIFT) | (3 << TXdesc_92E_DataScSHIFT));

				{
					if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor40M &&
						txcfg->pstat && (txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_40M_)))
						n_txshort = 1;
				}
			}
			else {
				if (priv->pshare->offset_2nd_chan == HT_2NDCH_OFFSET_BELOW)
					pdesc->Dword5 |= set_desc((2 << TXdesc_92E_DataScSHIFT) | (2 << TXdesc_92E_RtsScSHIFT));
				else
					pdesc->Dword5 |= set_desc((1 << TXdesc_92E_DataScSHIFT) | (1 << TXdesc_92E_RtsScSHIFT));

				{
					if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor20M &&
						txcfg->pstat && (txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_20M_)))
						n_txshort = 1;
				}
			}
		} else {
			{
				if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor20M &&
					txcfg->pstat && (txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_20M_)))
					n_txshort = 1;
			}
		}

		
		if (
			( txcfg->pstat && 
			((txcfg->pstat->aggre_mthd == AGGRE_MTHD_MPDU_AMSDU) || (txcfg->pstat->aggre_mthd == AGGRE_MTHD_MPDU))
			&& txcfg->aggre_en )	|| 
			((txcfg->aggre_en >= FG_AGGRE_MPDU) && (txcfg->aggre_en <= FG_AGGRE_MPDU_BUFFER_LAST)) 
		){
			int TID = ((struct sk_buff *)txcfg->pframe)->cb[1];
			if (txcfg->pstat->ADDBA_ready[TID] && !txcfg->pstat->low_tp_disable_ampdu) {


					pdesc->Dword2 |= set_desc(TXdesc_92E_AggEn);


				/*
				 * assign aggr size
				 */

				// assign aggr density
				if (txcfg->privacy) {
					//8812_11n_iot, set TxAmpduDsty=7 for 20M WPA2
					if ((priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G) && (!priv->pshare->is_40m_bw))
						pdesc->Dword2 |= set_desc(7 << TX_AmpduDstySHIFT);	// according to DWA-160 A2
					else
						pdesc->Dword2 |= set_desc(5 << TX_AmpduDstySHIFT);	// according to WN111v2
				}
				else {
					pdesc->Dword2 |= set_desc(((txcfg->pstat->ht_cap_buf.ampdu_para & _HTCAP_AMPDU_SPC_MASK_) >> _HTCAP_AMPDU_SPC_SHIFT_) << TX_AmpduDstySHIFT);
				}

				pdesc->Dword3 |= set_desc( ((txcfg->pstat->maxAggNum >> 1) & TXdesc_92E_MAX_AGG_NUMMask)<< TXdesc_92E_MAX_AGG_NUMSHIFT);      // MAX_AGG_NUM=4
                
				if (((txcfg->pstat->current_tx_rate >= _MCS0_RATE_) && (txcfg->pstat->current_tx_rate <= _MCS2_RATE_)) 
				||((txcfg->pstat->current_tx_rate >= _MCS8_RATE_) && (txcfg->pstat->current_tx_rate <= _MCS10_RATE_)) 
#ifdef RTK_AC_SUPPORT					
				||((txcfg->pstat->current_tx_rate >= _NSS1_MCS0_RATE_) && (txcfg->pstat->current_tx_rate <= _NSS1_MCS2_RATE_)) 
				||((txcfg->pstat->current_tx_rate >= _NSS2_MCS0_RATE_) && (txcfg->pstat->current_tx_rate <= _NSS2_MCS2_RATE_))
#endif					
				)
					pdesc->Dword3 |= set_desc( (0x02 & TXdesc_92E_MAX_AGG_NUMMask)<< TXdesc_92E_MAX_AGG_NUMSHIFT);		// MAX_AGG_NUM=4
#ifdef MULTI_STA_REFINE 					
				else if (priv->pshare->total_assoc_num > 10 )
					pdesc->Dword3 |= set_desc( (0x08 & TXdesc_92E_MAX_AGG_NUMMask)<< TXdesc_92E_MAX_AGG_NUMSHIFT);		// MAX_AGG_NUM=16
#endif
			}
		}

		// for STBC
		if (txcfg->pstat)	// 2012 10 31  for test
		{
			if ((txcfg->pstat->cmn_info.stbc_en & HT_STBC_EN)
#ifdef RTK_AC_SUPPORT
				|| (txcfg->pstat->cmn_info.stbc_en & VHT_STBC_EN)
#endif
			) {
#if (BEAMFORMING_SUPPORT == 1)
				u1Byte					Idx = 0;
				PRT_BEAMFORMING_ENTRY	pEntry;
				pEntry = Beamforming_GetEntryByMacId(priv, txcfg->pstat->cmn_info.aid, &Idx);
				if ((pEntry == NULL) || (!pEntry->Sounding_En))
#endif 
				pdesc->Dword5 |= set_desc(1 << TXdesc_92E_DataStbcSHIFT);
			}
		}
	}
	else	// legacy rate
	{
		txRate = get_rate_index_from_ieee_value((UINT8)txcfg->tx_rate);
		if (is_CCK_rate(txcfg->tx_rate) && (txcfg->tx_rate != 2)) {
			if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11G) &&
					(priv->pmib->dot11ErpInfo.longPreambleStaNum > 0))
				; // txfw->txshort = 0
			else {
				if (txcfg->pstat)
					bg_txshort = (priv->pmib->dot11RFEntry.shortpreamble) &&
									(txcfg->pstat->useShortPreamble);
				else
					bg_txshort = priv->pmib->dot11RFEntry.shortpreamble;
			}
		}
		#ifdef RTK_AC_SUPPORT	
		if (priv->pshare->is_40m_bw==2 && (GET_CHIP_VER(priv) != VERSION_8723B)) {
			pdesc->Dword5 |= set_desc((priv->pshare->txsc_20 << TXdesc_92E_DataScSHIFT) | (priv->pshare->txsc_20  << TXdesc_92E_RtsScSHIFT));
		}
		else
		#endif
		if (priv->pshare->is_40m_bw) {
			if (priv->pshare->offset_2nd_chan == HT_2NDCH_OFFSET_BELOW)
				pdesc->Dword5 |= set_desc((2 << TXdesc_92E_DataScSHIFT) | (2 << TXdesc_92E_RtsScSHIFT));
			else
				pdesc->Dword5 |= set_desc((1 << TXdesc_92E_DataScSHIFT) | (1 << TXdesc_92E_RtsScSHIFT));	
		}

		if (bg_txshort)
			pdesc->Dword5 |= set_desc(TXdesc_92E_DataShort);
			
	}
	#ifdef RTK_AC_SUPPORT
	if (AC_SIGMA_MODE != AC_SIGMA_NONE) //for 11ac logo
	if (txcfg->pstat)
	{
		txcfg->pstat->no_rts=0;
	}
	#endif
	if (txcfg->need_ack) { // unicast
		if (frag_idx == 0) {
			u4Byte fr_len;
#if defined(CONFIG_PCI_HCI)
			if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
				fr_len = (get_desc(pdesc->Dword0)&TX_PktSizeMask) - (get_desc(pdesc->Dword7)&TX_TxBufSizeMask);
			}
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
			if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
				fr_len = txcfg->fr_len;	// for backward compatibile
			}
#endif
			RtsCheck(priv, txcfg, fr_len, &bRtsEnable, &bCts2SelfEnable, &bHwRts, &bErpProtect, &bNProtect, &bRtsThreshold);
		}
	}

	RtsRate = find_rts_rate(priv, txcfg->tx_rate, bErpProtect);
	// Use Cts2Self instead when RtsRate < 11M && MPDU <= rts_thrshld
	if (bRtsEnable && CheckCts2SelfEnable(RtsRate) && !bRtsThreshold)
	{
		bRtsEnable = FALSE;
		bCts2SelfEnable = TRUE;
	}

#ifdef COCHANNEL_RTS
	if (priv->pmib->miscEntry.auto_rts_mode == 2)
	{
		bRtsEnable = FALSE;
		bCts2SelfEnable = FALSE;
		bHwRts = FALSE;
	}
	else if (priv->pmib->miscEntry.auto_rts_mode == 3)
	{
		bRtsEnable = TRUE;
		bCts2SelfEnable = FALSE;
		bHwRts = FALSE;
	}
#endif

#ifdef RTK_AC_SUPPORT
	if (txcfg->pstat && (GET_CHIP_VER(priv) != VERSION_8723B)) //8812_11n_iot, only vht clnt support cca_rts, //for 11ac logo
		RtsCheckAC(priv, txcfg, &bRtsEnable, &bCts2SelfEnable, &bHwRts);
#endif
	if (bRtsEnable)
		pdesc->Dword3 |= set_desc(TX_RtsEn);	
	if (bCts2SelfEnable)
		pdesc->Dword3 |= set_desc(TX_CTS2Self);	
	if (bHwRts)
		pdesc->Dword3 |= set_desc(TX_HwRtsEn);	

	if (bRtsEnable || bCts2SelfEnable ) {

			unsigned int rtsTxRateIdx   = get_rate_index_from_ieee_value(RtsRate);
			if (bErpProtect) {
				unsigned char  rtsShort = 0;
				if (is_CCK_rate(RtsRate) && (RtsRate != 2)) {
					if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11G) &&
							(priv->pmib->dot11ErpInfo.longPreambleStaNum > 0))
						rtsShort = 0; // do nothing
					else {
						if (txcfg->pstat)
							rtsShort = (priv->pmib->dot11RFEntry.shortpreamble) &&
											(txcfg->pstat->useShortPreamble);
						else
							rtsShort = priv->pmib->dot11RFEntry.shortpreamble;
					}
				}
				pdesc->Dword5 |= (rtsShort == 1)? set_desc(TXdesc_92E_RtsShort): 0;
				
			} 
			pdesc->Dword4 |= set_desc((rtsTxRateIdx & TXdesc_92E_RtsRateMask) << TXdesc_92E_RtsRateSHIFT);
			pdesc->Dword4 |= set_desc((0xf & TXdesc_92E_RtsRateFBLmtMask) << TXdesc_92E_RtsRateFBLmtSHIFT);
				
	}

	if (txcfg->pstat && priv->pshare->rf_ft_var.txforce != 0xFF)
	{
		if(priv->pshare->rf_ft_var.sgiforce == 1)
			n_txshort = 1;
		else if(priv->pshare->rf_ft_var.sgiforce == 0)
			n_txshort = 0;
	}

	if (n_txshort == 1)
	if ((!txcfg->pstat->cmn_info.ra_info.disable_ra) || (priv->pshare->rf_ft_var.sgiforce != 0xFF))
		pdesc->Dword5 |= set_desc(TXdesc_92E_DataShort);

#ifdef DRVMAC_LB
	if (priv->pmib->miscEntry.drvmac_lb && (priv->pmib->miscEntry.lb_mlmp == 4)) {
		txRate = rate_select;
		if (rate_select++ > 0x1b)
			rate_select = 0;

		pdesc->Dword3 |= set_desc(TXdesc_92E_DisDataFB|TXdesc_92E_DisRtsFB|TXdesc_92E_UseRate);
	}
#endif

	if ((priv->pshare->rf_ft_var.txforce != 0xff)
#if (BEAMFORMING_SUPPORT == 1)
		&& (!txcfg->ndpa) 
#endif
	) {
				pdesc->Dword3 |= set_desc(TXdesc_92E_DisDataFB|TXdesc_92E_DisRtsFB|TXdesc_92E_UseRate);
				pdesc->Dword4 |= set_desc((priv->pshare->rf_ft_var.txforce & TXdesc_92E_DataRateMask) << TXdesc_92E_DataRateSHIFT);
	}
	else
		pdesc->Dword4 |= set_desc((txRate & TXdesc_92E_DataRateMask) << TXdesc_92E_DataRateSHIFT);


#if 1
	if (priv->pshare->rf_ft_var.rts_init_rate) {
		pdesc->Dword4 &= set_desc(~(TX_RtsRateMask_8812 << TX_RtsRateSHIFT_8812));
		pdesc->Dword4 |= set_desc(((priv->pshare->rf_ft_var.rts_init_rate) & TX_RtsRateMask_8812) << TX_RtsRateSHIFT_8812);
	}		
	if ((priv->pmib->dot11RFEntry.phyBandSelect & PHY_BAND_5G) && 
		(TX_RtsRateMask_8812&(get_desc(pdesc->Dword4)>>TX_RtsRateSHIFT_8812)) <4	)
		pdesc->Dword4 |= set_desc((4 & TX_RtsRateMask_8812) << TX_RtsRateSHIFT_8812);
#else		
	if (priv->pmib->dot11RFEntry.phyBandSelect & PHY_BAND_5G)
		pdesc->Dword4 |= set_desc((4 & TX_RtsRateMask_8812) << TX_RtsRateSHIFT_8812);

#endif

	if (txcfg->need_ack) {
		// give retry limit to management frame
		if (txcfg->q_num == MANAGE_QUE_NUM) {
			pdesc->Dword4 |= set_desc(TXdesc_92E_RtyLmtEn);
			
			if (GetFrameSubType(txcfg->phdr) == WIFI_PROBERSP) {
				;	// 0 no need to set
			}
#ifdef WDS
			else if ((GetFrameSubType(txcfg->phdr) == WIFI_PROBEREQ) && txcfg->pstat && (txcfg->pstat->state & WIFI_WDS)) {
				pdesc->Dword4 |= set_desc((2 & TX_DataRtyLmtMask) << TX_DataRtyLmtSHIFT);
			}
#endif
#ifdef CONFIG_IEEE80211W
            else if (txcfg->isPMF == 1 && 
                (GetFrameSubType(txcfg->phdr) == WIFI_DEAUTH || GetFrameSubType(txcfg->phdr) == WIFI_DISASSOC)) {
                pdesc->Dword4 |= set_desc((16 & TX_DataRtyLmtMask) << TX_DataRtyLmtSHIFT);
            }
#endif
			else {
				pdesc->Dword4 |= set_desc((6 & TXdesc_92E_DataRtyLmtMask) << TXdesc_92E_DataRtyLmtSHIFT);
			}
		}
#ifdef WDS
		else if (txcfg->wdsIdx >= 0) {
			if (txcfg->pstat->rx_avarage == 0) {
				pdesc->Dword4 |= set_desc(TX_RtyLmtEn);
				pdesc->Dword4 |= set_desc((3 & TX_DataRtyLmtMask) << TX_DataRtyLmtSHIFT);
			}
		}
#endif
#ifdef MULTI_STA_REFINE
		else if ( priv->pshare->total_assoc_num > 30) {
		        pdesc->Dword4 |= set_desc(TXdesc_92E_RtyLmtEn);
                	pdesc->Dword4 |= set_desc((0x0a & TXdesc_92E_DataRtyLmtMask) << TXdesc_92E_DataRtyLmtSHIFT);
		}
#endif
		else if (is_MCS_rate(txcfg->pstat->current_tx_rate) && (txcfg->pstat->IOTPeer==HT_IOT_PEER_INTEL) && (txcfg->pstat->retry_inc)
			&& !(txcfg->pstat->leave) && priv->pshare->intel_rty_lmt) {			
			pdesc->Dword4 |= set_desc(TXdesc_92E_RtyLmtEn);
			pdesc->Dword4 |= set_desc((priv->pshare->intel_rty_lmt & TXdesc_92E_DataRtyLmtMask) << TXdesc_92E_DataRtyLmtSHIFT);
		}

		else if ((txcfg->pstat->IOTPeer==HT_IOT_PEER_BROADCOM) && (txcfg->pstat->retry_inc) && !(txcfg->pstat->leave)) {
			pdesc->Dword4 |= set_desc(TXdesc_92E_RtyLmtEn);
    		pdesc->Dword4 |= set_desc((0x20 & TXdesc_92E_DataRtyLmtMask) << TXdesc_92E_DataRtyLmtSHIFT);
        }

		if (priv->pshare->rf_ft_var.tx_pwr_ctrl && txcfg->pstat && (txcfg->fr_type == _SKB_FRAME_TYPE_)) {
			if (txcfg->pstat->hp_level == 1) {
				pdesc->Dword5 |= set_desc((priv->pshare->phw->TXPowerOffset & TXdesc_8812_TxPwrOffetMask) << TXdesc_8812_TxPwrOffetSHIFT);
			}
		}	

#if (BEAMFORMING_SUPPORT == 1)
		if (txcfg->ndpa) {
			unsigned char *pFrame = (unsigned char*)txcfg->phdr;
			if (IsCtrlNDPA(pFrame) || IsMgntNDPA(pFrame)) {
				//SET_TX_DESC_DATA_RETRY_LIMIT_8812(pDesc, 5);
				//SET_TX_DESC_RETRY_LIMIT_ENABLE_8812(pDesc, 1);
				pdesc->Dword4 &= set_desc(~(TXdesc_92E_DataRtyLmtMask << TXdesc_92E_DataRtyLmtSHIFT));									
	            pdesc->Dword4 |= set_desc(TXdesc_92E_RtyLmtEn|((0x05 & TXdesc_92E_DataRtyLmtMask) << TXdesc_92E_DataRtyLmtSHIFT));	

					if(IsMgntNDPA(pFrame))		//0xe0
					{
						pdesc->Dword3 |= set_desc((1 &TXdesc_92E_NDPAMASK)<<TXdesc_92E_NDPASHIFT);
					}	
					else		// 0x54
					{
						if(!IS_TEST_CHIP(priv))
						{
							if ((txcfg->pstat->wireless_mode & WIRELESS_MODE_AC_5G) && (txcfg->pstat->IOTPeer!=HT_IOT_PEER_BROADCOM))
								pdesc->Dword3 |= set_desc((2 &TXdesc_92E_NDPAMASK)<<TXdesc_92E_NDPASHIFT);
							else
								pdesc->Dword3 |= set_desc((1 &TXdesc_92E_NDPAMASK)<<TXdesc_92E_NDPASHIFT);
						}
						else
						{
							pdesc->Dword3 |= set_desc((1 &TXdesc_92E_NDPAMASK)<<TXdesc_92E_NDPASHIFT);				
						}
					}	
	//				panic_printk("LINE:%d, %x\n", __LINE__, get_desc(pdesc->Dword3));
			}
		} else
#endif

		{
			if ((txcfg->pstat) &&
				((txcfg->pstat->cmn_info.ldpc_en & HT_LDPC_EN)
#ifdef RTK_AC_SUPPORT
				|| (txcfg->pstat->cmn_info.ldpc_en & VHT_LDPC_EN)
#endif
			)) {
				pdesc->Dword5 |= set_desc(TXdesc_92E_DataLDPC);	
			}
		}
	}

	if ((txcfg->q_num == MANAGE_QUE_NUM) && (txcfg->fr_type == _PRE_ALLOCMEM_) && (GetFrameSubType(txcfg->phdr) == WIFI_PROBERSP))
	{
		if (priv->pmib->dot11RFEntry.bcnagc==1)  {
			if (priv->pshare->rf_ft_var.bcn_pwr_idex+6 <= priv->pshare->rf_ft_var.bcn_pwr_max)
				pdesc->Dword5 |= set_desc((4 & TXdesc_8812_TxPwrOffetMask) << TXdesc_8812_TxPwrOffetSHIFT);
		}
		else if ( priv->pmib->dot11RFEntry.bcnagc==2) {
			if (priv->pshare->rf_ft_var.bcn_pwr_idex+12 <= priv->pshare->rf_ft_var.bcn_pwr_max)
				pdesc->Dword5 |= set_desc((5 & TXdesc_8812_TxPwrOffetMask) << TXdesc_8812_TxPwrOffetSHIFT);
			else if (priv->pshare->rf_ft_var.bcn_pwr_idex+6 <=priv->pshare->rf_ft_var.bcn_pwr_max)
				pdesc->Dword5 |= set_desc((4 & TXdesc_8812_TxPwrOffetMask) << TXdesc_8812_TxPwrOffetSHIFT);				
		}
	} 
	if (priv->pmib->dot11RFEntry.txpwr_reduction) {
		if (priv->pmib->dot11RFEntry.txpwr_reduction <= 3) {
			if (reduction_level < priv->pmib->dot11RFEntry.txpwr_reduction) {
				reduction_level = priv->pmib->dot11RFEntry.txpwr_reduction;
				pdesc->Dword5 &= set_desc(~((TXdesc_8812_TxPwrOffetMask) << TXdesc_8812_TxPwrOffetSHIFT));
				pdesc->Dword5 |= set_desc((reduction_level & TXdesc_8812_TxPwrOffetMask) << TXdesc_8812_TxPwrOffetSHIFT);
			}
		}
	}
}

void rtl8192cd_signin_txdesc_8812(struct rtl8192cd_priv *priv, struct tx_insn* txcfg)
{
	struct tx_desc		*phdesc, *pdesc, *pndesc, *picvdesc, *pmicdesc, *pfrstdesc;
	struct tx_desc_info	*pswdescinfo, *pdescinfo, *pndescinfo, *picvdescinfo, *pmicdescinfo;
	unsigned int 		fr_len, tx_len, i, keyid;
	int					*tx_head, q_num;
	unsigned long		tmpphyaddr;
	unsigned char		*da, *pbuf, *pwlhdr, *pmic, *picv;
	struct rtl8192cd_hw	*phw;
	unsigned char		 q_select;
#ifdef TX_SHORTCUT
	int					fit_shortcut=0, idx=0;
#endif
	unsigned long		pfrst_dma_desc=0;
	unsigned long		*dma_txhead;

	unsigned long		flush_addr[20];
	int					flush_len[20];
	int					flush_num=0;
#ifdef CONFIG_IEEE80211W
	unsigned int	isBIP = 0;
#endif

	picvdesc=NULL;
	keyid=0;
	pmic=NULL;
	picv=NULL;
	q_select=0;

	if (txcfg->tx_rate == 0) {
		DEBUG_ERR("tx_rate=0!\n");
		txcfg->tx_rate = find_rate(priv, NULL, 0, 1);
	}

	q_num = txcfg->q_num;

	phw	= GET_HW(priv);

	dma_txhead	= get_txdma_addr(phw, q_num);
	tx_head		= get_txhead_addr(phw, q_num);
	phdesc   	= get_txdesc(phw, q_num);
	pswdescinfo = get_txdesc_info(priv,priv->pshare->pdesc_info, q_num);

	da = get_da((unsigned char *)txcfg->phdr);
	
#ifdef CONFIG_IEEE80211W
	if(txcfg->isPMF && IS_MCAST(da)) 
	{
		isBIP = 1;
		txcfg->iv = 0;
		txcfg->fr_len += 10; // 10: MMIE length
	}
#endif
	tx_len = txcfg->fr_len;

	if (txcfg->fr_type == _SKB_FRAME_TYPE_)
		pbuf = ((struct sk_buff *)txcfg->pframe)->data;
	else
		pbuf = (unsigned char*)txcfg->pframe;

	tmpphyaddr = get_physical_addr(priv, pbuf, tx_len, PCI_DMA_TODEVICE);

	// in case of default key, then find the key id
	if (GetPrivacy((txcfg->phdr)))
	{
#ifdef WDS
		if (txcfg->wdsIdx >= 0) {
			if (txcfg->pstat)
				keyid = txcfg->pstat->keyid;
			else
				keyid = 0;
		}
		else
#endif

#ifdef __DRAYTEK_OS__
		if (!IEEE8021X_FUN)
			keyid = priv->pmib->dot1180211AuthEntry.dot11PrivacyKeyIndex;
		else {
			if (IS_MCAST(GetAddr1Ptr ((unsigned char *)txcfg->phdr)) || !txcfg->pstat)
				keyid = priv->pmib->dot11GroupKeysTable.keyid;
			else
				keyid = txcfg->pstat->keyid;
		}
#else

		if (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm==_WEP_40_PRIVACY_ ||
			priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm==_WEP_104_PRIVACY_) {
			if(IEEE8021X_FUN && txcfg->pstat) {
#ifdef A4_STA
				if (IS_MCAST(da) && !(txcfg->pstat->state & WIFI_A4_STA))
#else
				if(IS_MCAST(da))
#endif					
					keyid = 0;
				else
					keyid = txcfg->pstat->keyid;
			}
			else
				keyid = priv->pmib->dot1180211AuthEntry.dot11PrivacyKeyIndex;
		}
#endif
	}

	for (i=0, pfrstdesc= phdesc + (*tx_head); i < txcfg->frg_num; i++)
	{
		/*------------------------------------------------------------*/
		/*           fill descriptor of header + iv + llc             */
		/*------------------------------------------------------------*/
		pdesc     = phdesc + (*tx_head);
		pdescinfo = pswdescinfo + *tx_head;

		//clear all bits

		memset(pdesc, 0, 40);

		if (i != 0)
		{
			// we have to allocate wlan_hdr
			pwlhdr = (UINT8 *)get_wlanhdr_from_poll(priv);
			if (pwlhdr == (UINT8 *)NULL)
			{
				DEBUG_ERR("System-bug... should have enough wlan_hdr\n");
				return;
			}
			// other MPDU will share the same seq with the first MPDU
			if (txcfg->hdr_len <= sizeof(struct wlan_hdr))
				memcpy((void *)pwlhdr, (void *)(txcfg->phdr), txcfg->hdr_len); // data pkt has 24 bytes wlan_hdr
			else {
				DEBUG_ERR("System-bug... should have enough wlan_hdr, hdr_len=%d\n", txcfg->hdr_len);
				return (txcfg->frg_num - i);
			}
			pdesc->Dword9 |= set_desc((GetSequence(txcfg->phdr) & TXdesc_92E_TX_SeqMask)  << TXdesc_92E_TX_SeqSHIFT);
		}
		else
		{
#ifdef WIFI_WMM
			if (txcfg->pstat && (is_qos_data(txcfg->phdr))) {
				if ((GetFrameSubType(txcfg->phdr) & (WIFI_DATA_TYPE | BIT(6) | BIT(7))) == (WIFI_DATA_TYPE | BIT(7))) {
					unsigned char tempQosControl[2];
					memset(tempQosControl, 0, 2);
					tempQosControl[0] = ((struct sk_buff *)txcfg->pframe)->cb[1];
#ifdef WMM_APSD
					if (
#ifdef CLIENT_MODE
						(OPMODE & WIFI_AP_STATE) &&
#endif
						(APSD_ENABLE) && (txcfg->pstat) && (txcfg->pstat->state & WIFI_SLEEP_STATE) &&
						(!GetMData(txcfg->phdr)) &&
						((((tempQosControl[0] == 7) || (tempQosControl[0] == 6)) && (txcfg->pstat->apsd_bitmap & 0x01)) ||
						 (((tempQosControl[0] == 5) || (tempQosControl[0] == 4)) && (txcfg->pstat->apsd_bitmap & 0x02)) ||
						 (((tempQosControl[0] == 3) || (tempQosControl[0] == 0)) && (txcfg->pstat->apsd_bitmap & 0x08)) ||
						 (((tempQosControl[0] == 2) || (tempQosControl[0] == 1)) && (txcfg->pstat->apsd_bitmap & 0x04))))
						tempQosControl[0] |= BIT(4);
#endif
					if (txcfg->aggre_en == FG_AGGRE_MSDU_FIRST)
						tempQosControl[0] |= BIT(7);

					if (priv->pmib->dot11nConfigEntry.dot11nTxNoAck)
						tempQosControl[0] |= BIT(5);

					memcpy((void *)GetQosControl((txcfg->phdr)), tempQosControl, 2);
				}
			}
#endif

#if (BEAMFORMING_SUPPORT == 1)
			if(!txcfg->ndpa)
#endif	
			assign_wlanseq(GET_HW(priv), txcfg->phdr, txcfg->pstat, GET_MIB(priv)
#ifdef CONFIG_RTK_MESH	// For broadcast data frame via mesh (ex:ARP requst)
				, txcfg->is_11s
#endif
				);

			pdesc->Dword9 |= set_desc((GetSequence(txcfg->phdr) & TXdesc_92E_TX_SeqMask)  << TXdesc_92E_TX_SeqSHIFT);
			pwlhdr = txcfg->phdr;
		}
#if (BEAMFORMING_SUPPORT == 1)
		if(!txcfg->ndpa)
#endif			
		SetDuration(pwlhdr, 0);		

		if (priv->pmib->dot11nConfigEntry.dot11nTxNoAck){
			pdesc->Dword0 |= set_desc(TX_BMC);
			pdesc->Dword2 &= set_desc(~(TXdesc_92E_AggEn));
		}

		pdesc->Dword0 |= set_desc(40 << TX_OffsetSHIFT); // tx_desc size
		
		if (IS_MCAST(GetAddr1Ptr((unsigned char *)txcfg->phdr)))
			pdesc->Dword0 |= set_desc(TX_BMC);

#ifdef CLIENT_MODE
		if (OPMODE & WIFI_STATION_STATE) {
			if (GetFrameSubType(txcfg->phdr) == WIFI_PSPOLL)
				pdesc->Dword3 |= set_desc(TXdesc_92E_NAVUSEHDR);


			if (priv->ps_state)
				SetPwrMgt(pwlhdr);
			else
				ClearPwrMgt(pwlhdr);
		}
#endif
#if (BEAMFORMING_SUPPORT == 1)
		if (txcfg->ndpa)
			pdesc->Dword3 |= set_desc(TXdesc_92E_NAVUSEHDR);
#endif		

		if (i != (txcfg->frg_num - 1))
		{
			SetMFrag(pwlhdr);
			if (i == 0) {
				fr_len = (txcfg->frag_thrshld - txcfg->llc);
				tx_len -= (txcfg->frag_thrshld - txcfg->llc);
			}
			else {
				fr_len = txcfg->frag_thrshld;
				tx_len -= txcfg->frag_thrshld;
			}
		}
		else	// last seg, or the only seg (frg_num == 1)
		{
			fr_len = tx_len;
			ClearMFrag(pwlhdr);
		}
		SetFragNum((pwlhdr), i);

		if ((i == 0) && (txcfg->fr_type == _SKB_FRAME_TYPE_))
		{
			pdesc->Dword7 |= set_desc((txcfg->hdr_len + txcfg->llc) << TX_TxBufSizeSHIFT);
			pdesc->Dword0 |= set_desc((fr_len + (get_desc(pdesc->Dword7) & TX_TxBufSizeMask)) << TX_PktSizeSHIFT);
			pdesc->Dword0 |= set_desc(TX_FirstSeg);
			pdescinfo->type = _PRE_ALLOCLLCHDR_;
		}
		else
		{
			pdesc->Dword7 |= set_desc(txcfg->hdr_len << TX_TxBufSizeSHIFT);
			pdesc->Dword0 |= set_desc((fr_len + (get_desc(pdesc->Dword7) & TX_TxBufSizeMask)) << TX_PktSizeSHIFT);
			pdesc->Dword0 |= set_desc(TX_FirstSeg);

#if (BEAMFORMING_SUPPORT == 1)
			if(txcfg->ndpa)
				pdescinfo->type = _PRE_ALLOCLLCHDR_;
			else
#endif				
			pdescinfo->type = _PRE_ALLOCHDR_;
		}

		rtl8192cd_fill_fwinfo_8812(priv, txcfg, pdesc, i);
		//Disable all RTS/CTS //for 11ac logo
		if(priv->pshare->rf_ft_var.no_rtscts) {
			pdesc->Dword3 &= set_desc(~(TXdesc_92E_HwRtsEn|TXdesc_92E_RtsEn|TXdesc_92E_CTS2Self));
		} else {
			if(txcfg->pstat) //8812_11n_iot, only vht clnt support cca_rts
				if((priv->pshare->rf_ft_var.cca_rts)
				#ifdef RTK_AC_SUPPORT
					&& (txcfg->pstat->vht_cap_len > 0)
				#endif
				)
					pdesc->Dword2 |= set_desc((priv->pshare->rf_ft_var.cca_rts & 0x03) << TX_8812_CcaRtsSHIFT);	//10b:  RTS support dynamic mode CCA secondary
		}

		switch (q_num) {
			case HIGH_QUEUE:
				q_select = 0x11;// High Queue
				break;
			case MGNT_QUEUE:
				q_select = 0x12;
				break;
#if defined(DRVMAC_LB) && defined(WIFI_WMM)
			case BE_QUEUE:
				q_select = 0;
				break;
#endif
			default:
				// data packet
#ifdef RTL_MANUAL_EDCA
				if (priv->pmib->dot11QosEntry.ManualEDCA) {
					switch (q_num) {
					case VO_QUEUE:
						q_select = 6;
						break;
					case VI_QUEUE:
						q_select = 4;
						break;
					case BE_QUEUE:
						q_select = 0;
						break;
					default:
						q_select = 1;
						break;
					}
				}
				else
#endif
					q_select = ((struct sk_buff *)txcfg->pframe)->cb[1];
				break;
		}
		pdesc->Dword1 |= set_desc((q_select & TX_QSelMask)<< TX_QSelSHIFT);

		if (i != (txcfg->frg_num - 1))
			pdesc->Dword2 |= set_desc(TX_MoreFrag);

// Set RateID
		if (txcfg->pstat) {
			if (txcfg->pstat->cmn_info.aid != MANAGEMENT_AID) {
				u8 ratid;

				ratid = txcfg->pstat->cmn_info.ra_info.rate_id;
								
				pdesc->Dword1 |= set_desc((ratid & TXdesc_92E_RateIDMask) << TX_RateIDSHIFT);
				
	// Set MacID
				pdesc->Dword1 |= set_desc(REMAP_AID(txcfg->pstat) & TXdesc_92E_MacIdMask);

#if (BEAMFORMING_SUPPORT == 1)
				if ((priv->pmib->dot11RFEntry.txbf == 1) &&
					((txcfg->pstat->ht_cap_len && (txcfg->pstat->ht_cap_buf.txbf_cap)) ||
					(txcfg->pstat->vht_cap_len && (cpu_to_le32(txcfg->pstat->vht_cap_buf.vht_cap_info) & BIT(SU_BFEE_S))))
				) {
					pdesc->Dword2 |= set_desc((txcfg->pstat->cmn_info.bf_info.p_aid & TX_8812_PAIDMask) << TX_8812_PAIDSHIFT);
					pdesc->Dword2 |= set_desc((txcfg->pstat->cmn_info.bf_info.g_id & TX_8812_GIDMask) << TX_8812_GIDSHIFT);
				}
#endif
			}
		} else {
#if (defined(MBSSID) || defined(UNIVERSAL_REPEATER)) && defined(HW_ENC_FOR_GROUP_CIPHER)
			if ((txcfg->fr_type == _SKB_FRAME_TYPE_) && IS_MCAST(GetAddr1Ptr ((unsigned char *)txcfg->phdr)) && 
				((txcfg->privacy == _TKIP_PRIVACY_) || (txcfg->privacy == _CCMP_PRIVACY_))) {
				pdesc->Dword1 |= set_desc(TX_EnDescID);
				pdesc->Dword1 |= set_desc(((priv->vap_init_seq+HW_ENC_GROUP_CIPHER_OFFSET) & TX_MacIdMask) << TX_MacIdSHIFT);
			}
#endif
			pdesc->Dword1 |= set_desc((ARFR_BMC &TXdesc_92E_RateIDMask)<<TX_RateIDSHIFT);

#if (BEAMFORMING_SUPPORT == 1)
				if (priv->pmib->dot11RFEntry.txbf == 1) 
					pdesc->Dword2 |= set_desc((TX_8812_GIDMask) << TX_8812_GIDSHIFT);
#endif				

		}
#if defined(MULTI_STA_REFINE)
		if ((priv->pshare->total_assoc_num > 10) && (txcfg->pstat) && ((txcfg->pstat->tx_ra_bitmap & 0xff00000) == 0))
			pdesc->Dword4 |= set_desc((0x03 & TXdesc_92E_DataRateFBLmtMask) << TXdesc_92E_DataRateFBLmtSHIFT);
		else
#endif			
		pdesc->Dword4 |= set_desc((0x1f & TXdesc_92E_DataRateFBLmtMask) << TXdesc_92E_DataRateFBLmtSHIFT);
		
		if (txcfg->fixed_rate) {
			pdesc->Dword3 |= set_desc(TXdesc_92E_DisDataFB|TXdesc_92E_DisRtsFB|TXdesc_92E_UseRate);
		}

		if (txcfg->pstat && txcfg->pstat->cmn_info.ra_info.disable_ra)
			pdesc->Dword3 |= set_desc(TXdesc_92E_UseRate);

#ifdef CONFIG_IEEE80211W
		if (!txcfg->need_ack && txcfg->privacy && UseSwCrypto(priv, NULL, TRUE, txcfg->isPMF))
#else
		if (!txcfg->need_ack && txcfg->privacy && UseSwCrypto(priv, NULL, TRUE))
#endif // CONFIG_IEEE80211W
			pdesc->Dword1 &= set_desc( ~(TX_SecTypeMask<< TX_SecTypeSHIFT));

		if (txcfg->privacy) {
#ifdef CONFIG_IEEE80211W
			if (UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE), txcfg->isPMF))
#else
			if (UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE)))
#endif // CONFIG_IEEE80211W
			{
				pdesc->Dword0 = set_desc(get_desc(pdesc->Dword0)+ txcfg->icv + txcfg->mic + txcfg->iv);
				pdesc->Dword7 = set_desc(get_desc(pdesc->Dword7)+ txcfg->iv);
			} else {
				// hw encrypt
				switch(txcfg->privacy) {
					case _WEP_104_PRIVACY_:
					case _WEP_40_PRIVACY_:
						pdesc->Dword0 = set_desc(get_desc(pdesc->Dword0) + txcfg->iv);
						pdesc->Dword7 = set_desc(get_desc(pdesc->Dword7) + txcfg->iv);
						wep_fill_iv(priv, pwlhdr, txcfg->hdr_len, keyid);
						pdesc->Dword1 |= set_desc(0x1 << TX_SecTypeSHIFT);					
						break;

					case _TKIP_PRIVACY_:
						pdesc->Dword0 = set_desc(get_desc(pdesc->Dword0) + txcfg->iv + txcfg->mic);
						pdesc->Dword7 = set_desc(get_desc(pdesc->Dword7) + txcfg->iv);
						tkip_fill_encheader(priv, pwlhdr, txcfg->hdr_len, keyid);
						pdesc->Dword1 |= set_desc(0x1 << TX_SecTypeSHIFT);
						break;
				#if defined(CONFIG_RTL_HW_WAPI_SUPPORT)
					case _WAPI_SMS4_:
						pdesc->Dword0 = set_desc(get_desc(pdesc->Dword0) + txcfg->iv);
						pdesc->Dword7 = set_desc(get_desc(pdesc->Dword7) + txcfg->iv);			
						pdesc->Dword1 |= set_desc(0x2 << TX_SecTypeSHIFT);
						break;
				#endif
					case _CCMP_PRIVACY_:
						//michal also hardware...
						pdesc->Dword0 = set_desc(get_desc(pdesc->Dword0) + txcfg->iv);
						pdesc->Dword7 = set_desc(get_desc(pdesc->Dword7) + txcfg->iv);
						aes_fill_encheader(priv, pwlhdr, txcfg->hdr_len, keyid);
						pdesc->Dword1 |= set_desc(0x3 << TX_SecTypeSHIFT);
						break;

					default:
						DEBUG_ERR("Unknow privacy\n");
						break;
				}
			}
		}

#ifdef TX_EARLY_MODE
		if (GET_TX_EARLY_MODE && pwlhdr && i == 0) {
			pdesc->Dword0 = set_desc((get_desc(pdesc->Dword0) & 0xff00ffff) | (0x28 << TX_OffsetSHIFT));
			pdesc->Dword1 = set_desc(get_desc(pdesc->Dword1) | (1 << TX_PktOffsetSHIFT));
			pdesc->Dword7 = set_desc(get_desc(pdesc->Dword7) + 8);

			memset(pwlhdr-8, '\0', 8);			
			if (txcfg->pstat && txcfg->pstat->empkt_num > 0) 			
				insert_emcontent(priv, txcfg, pwlhdr-8);

			pdesc->Dword10 = set_desc(get_physical_addr(priv, pwlhdr-8,
				get_desc(pdesc->Dword7)&TX_TxBufSizeMask, PCI_DMA_TODEVICE));	
		}
		else
#endif
		{
			pdesc->Dword10 = set_desc(get_physical_addr(priv, pwlhdr,
				(get_desc(pdesc->Dword7)&TX_TxBufSizeMask), PCI_DMA_TODEVICE));

		}

		// below is for sw desc info
#ifndef TXDESC_INFO		
		pdescinfo->paddr  = get_desc(pdesc->Dword10);
#endif		
		pdescinfo->pframe = pwlhdr;
#if defined(WIFI_WMM) && defined(WMM_APSD)
#if defined(ENABLE_RTL_SKB_STATS) || defined(RESERVE_TXDESC_FOR_EACH_IF)
		pdescinfo->priv = priv;
#endif
#ifndef TXDESC_INFO		
		pdescinfo->pstat = txcfg->pstat;
#endif		
#endif

#ifdef TX_SHORTCUT
		if (!priv->pmib->dot11OperationEntry.disable_txsc && txcfg->pstat &&
				(txcfg->fr_type == _SKB_FRAME_TYPE_) &&
				(txcfg->frg_num == 1) &&
				((txcfg->privacy == 0)
#ifdef CONFIG_RTL_WAPI_SUPPORT
				|| (txcfg->privacy == _WAPI_SMS4_)
#endif
#ifdef CONFIG_IEEE80211W
				|| !UseSwCrypto(priv, txcfg->pstat, FALSE, txcfg->isPMF)
#else
				|| !UseSwCrypto(priv, txcfg->pstat, FALSE)
#endif // CONFIG_IEEE80211W
				) && !GetMData(txcfg->phdr) ) {

#ifdef CONFIG_RTK_MESH
			if (txcfg->is_11s) {
				idx = get_tx_sc_free_entry(priv, txcfg->pstat, &priv->ethhdr, txcfg->aggre_en);
			} else
#endif
				idx = get_tx_sc_free_entry(priv, txcfg->pstat, pbuf - sizeof(struct wlan_ethhdr_t), txcfg->aggre_en);

#ifdef CONFIG_RTK_MESH
			if (txcfg->is_11s) {
				memcpy((void *)&txcfg->pstat->tx_sc_ent[idx].ethhdr, &priv->ethhdr, sizeof(struct wlan_ethhdr_t));
			} else
#endif
				memcpy((void *)&txcfg->pstat->tx_sc_ent[idx].ethhdr, pbuf - sizeof(struct wlan_ethhdr_t), sizeof(struct wlan_ethhdr_t));

			memcpy(&txcfg->pstat->tx_sc_ent[idx].hwdesc1, pdesc, 40);
			descinfo_copy(&txcfg->pstat->tx_sc_ent[idx].swdesc1, pdescinfo);
			txcfg->pstat->protection = priv->pmib->dot11ErpInfo.protection;
			txcfg->pstat->ht_protection = priv->ht_protection;
			txcfg->pstat->tx_sc_ent[idx].sc_keyid = keyid;
			txcfg->pstat->tx_sc_ent[idx].pktpri = ((struct sk_buff *)txcfg->pframe)->cb[1];
			fit_shortcut = 1;
		}
		else {
			if (txcfg->pstat) {
				for (idx=0; idx<MAX_TXSC_ENTRY; idx++)
					txcfg->pstat->tx_sc_ent[idx].hwdesc1.Dword7 &= set_desc(~TX_TxBufSizeMask);
			}
		}
#endif

		pfrst_dma_desc = dma_txhead[*tx_head];

		if (i != 0) {
			pdesc->Dword0 |= set_desc(TX_OWN);
#ifndef USE_RTL8186_SDK
			rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(dma_txhead[*tx_head]-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(struct tx_desc), PCI_DMA_TODEVICE);
#endif
		}

		flush_addr[flush_num]  = (unsigned long)bus_to_virt(get_desc(pdesc->Dword10));
		flush_len[flush_num++]= (get_desc(pdesc->Dword7) & TX_TxBufSizeMask);

/*
		//printk desc content
		{
			unsigned int *ppdesc = (unsigned int *)pdesc;
			printk("%08x    %08x    %08x \n", get_desc(*(ppdesc)), get_desc(*(ppdesc+1)), get_desc(*(ppdesc+2)));
			printk("%08x    %08x    %08x \n", get_desc(*(ppdesc+3)), get_desc(*(ppdesc+4)), get_desc(*(ppdesc+5)));
			printk("%08x    %08x    %08x \n", get_desc(*(ppdesc+6)), get_desc(*(ppdesc+7)), get_desc(*(ppdesc+8)));
			printk("%08x\n", *(ppdesc+9));
			printk("===================================================\n");
		}
*/

#ifdef OSK_LOW_TX_DESC
		if (q_num!=BE_QUEUE && q_num!=HIGH_QUEUE)
			txdesc_rollover_nonbe(pdesc, (unsigned int *)tx_head);
		else
#endif
		txdesc_rollover(pdesc, (unsigned int *)tx_head);

		if (txcfg->fr_len == 0)
		{
			if (txcfg->aggre_en != FG_AGGRE_MSDU_FIRST)
				pdesc->Dword0 |= set_desc(TX_LastSeg);
			goto init_deschead;
		}

		/*------------------------------------------------------------*/
		/*              fill descriptor of frame body                 */
		/*------------------------------------------------------------*/
		pndesc     = phdesc + *tx_head;
		pndescinfo = pswdescinfo + *tx_head;
		//clear all bits

		memset(pndesc, 0,40);
		pndesc->Dword0 = set_desc((get_desc(pdesc->Dword0) & (~TX_FirstSeg)) | (TX_OWN));



#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))
	if (txcfg->pstat)
	{
		odm_set_tx_ant_by_tx_info(priv, pdesc, txcfg->pstat->cmn_info.aid);  //AC-series Set TX antenna
	}
#endif


		if (txcfg->privacy)
		{
			if (txcfg->privacy == _WAPI_SMS4_)
			{
				if (txcfg->aggre_en != FG_AGGRE_MSDU_FIRST)
					pndesc->Dword0 |= set_desc(TX_LastSeg);
#ifndef TXDESC_INFO						
				pndescinfo->pstat = txcfg->pstat;
				pndescinfo->rate = txcfg->tx_rate;
#endif				
			}
#ifdef CONFIG_IEEE80211W			
			else if (!UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE),txcfg->isPMF))
#else
			else if (!UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE)))
#endif // CONFIG_IEEE80211W
			{
				if (txcfg->aggre_en != FG_AGGRE_MSDU_FIRST)
					pndesc->Dword0 |= set_desc(TX_LastSeg);
#ifndef TXDESC_INFO						
				pndescinfo->pstat = txcfg->pstat;
				pndescinfo->rate = txcfg->tx_rate;
#endif				
			}
		}
		else
		{
			if (txcfg->aggre_en != FG_AGGRE_MSDU_FIRST)
				pndesc->Dword0 |= set_desc(TX_LastSeg);
#ifndef TXDESC_INFO					
			pndescinfo->pstat = txcfg->pstat;
			pndescinfo->rate = txcfg->tx_rate;
#endif			
		}

#ifdef CONFIG_RTL_WAPI_SUPPORT
#if defined(CONFIG_RTL_HW_WAPI_SUPPORT)
#ifdef CONFIG_IEEE80211W
		if ((txcfg->privacy == _WAPI_SMS4_)&&(UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE), txcfg->isPMF)))
#else
		if ((txcfg->privacy == _WAPI_SMS4_)&&(UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE))))
#endif
#else
		if (txcfg->privacy == _WAPI_SMS4_)
#endif
		{
			pndesc->Dword7 |= set_desc( (fr_len+SMS4_MIC_LEN) & TX_TxBufSizeMask);
		}
		else
#endif
		pndesc->Dword7 |= set_desc(fr_len & TX_TxBufSizeMask);

		if (i == 0)
			pndescinfo->type = txcfg->fr_type;
		else
			pndescinfo->type = _RESERVED_FRAME_TYPE_;

#if defined(CONFIG_RTK_MESH) && defined(MESH_USE_METRICOP)
		if ((txcfg->fr_type == _PRE_ALLOCMEM_) && (txcfg->is_11s & 128)) // for 11s link measurement frame
			pndescinfo->type =_RESERVED_FRAME_TYPE_;
#endif

		pndesc->Dword10 = set_desc(tmpphyaddr); //TxBufferAddr
#ifndef TXDESC_INFO		
		pndescinfo->paddr = get_desc(pndesc->Dword10);
#endif		
		pndescinfo->pframe = txcfg->pframe;
#ifndef TXDESC_INFO		
		pndescinfo->len = txcfg->fr_len;	// for pci_unmap_single
#endif		
		pndescinfo->priv = priv;

		pbuf += fr_len;
		tmpphyaddr += fr_len;

#ifdef TX_SHORTCUT
		if (fit_shortcut) {
			memcpy(&txcfg->pstat->tx_sc_ent[idx].hwdesc2, pndesc, 40);
			descinfo_copy(&txcfg->pstat->tx_sc_ent[idx].swdesc2, pndescinfo);
#ifdef TXSC_CACHE_FRLEN			
			txcfg->pstat->tx_sc_ent[idx].frlen = fr_len;
#endif			
		}
#endif

#ifndef USE_RTL8186_SDK
		rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(dma_txhead[*tx_head]-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(struct tx_desc), PCI_DMA_TODEVICE);
#endif

		flush_addr[flush_num]=(unsigned long)bus_to_virt(get_desc(pndesc->Dword10));
		flush_len[flush_num++] = get_desc(pndesc->Dword7) & TX_TxBufSizeMask;

#ifdef OSK_LOW_TX_DESC
		if (q_num!=BE_QUEUE && q_num!=HIGH_QUEUE)
			txdesc_rollover_nonbe(pndesc, (unsigned int *)tx_head);
		else
#endif
		txdesc_rollover(pndesc, (unsigned int *)tx_head);

		// retrieve H/W MIC and put in payload
#ifdef CONFIG_RTL_WAPI_SUPPORT
		if (txcfg->privacy == _WAPI_SMS4_)
		{
			SecSWSMS4Encryption(priv, txcfg);
#if 0
			if (txcfg->mic>0)
			{
				pndesc->Dword7 &= set_desc(~TX_TxBufSizeMask);
				pndesc->Dword7 |= set_desc((fr_len+txcfg->mic)& TX_TxBufSizeMask);
				flush_len[flush_num-1]= (get_desc(pndesc->Dword7) & TX_TxBufSizeMask);
			}
			else
			{
				txcfg->mic = SMS4_MIC_LEN;
			}
#endif
		
		}
#endif

#ifndef NOT_RTK_BSP
		if ((txcfg->privacy == _TKIP_PRIVACY_) &&
			(priv->pshare->have_hw_mic) &&
			!(priv->pmib->dot11StationConfigEntry.swTkipMic) &&
			(i == (txcfg->frg_num-1)) )	// last segment
		{
			register unsigned long int l,r;
			unsigned char *mic;
			volatile int i;

			while ((*(volatile unsigned int *)GDMAISR & GDMA_COMPIP) == 0)
				for (i=0; i<10; i++)
					;

			l = *(volatile unsigned int *)GDMAICVL;
			r = *(volatile unsigned int *)GDMAICVR;

			mic = ((struct sk_buff *)txcfg->pframe)->data + txcfg->fr_len - 8;
			mic[0] = (unsigned char)(l & 0xff);
			mic[1] = (unsigned char)((l >> 8) & 0xff);
			mic[2] = (unsigned char)((l >> 16) & 0xff);
			mic[3] = (unsigned char)((l >> 24) & 0xff);
			mic[4] = (unsigned char)(r & 0xff);
			mic[5] = (unsigned char)((r >> 8) & 0xff);
			mic[6] = (unsigned char)((r >> 16) & 0xff);
			mic[7] = (unsigned char)((r >> 24) & 0xff);

#ifdef MICERR_TEST
			if (priv->micerr_flag) {
				mic[7] ^= mic[7];
				priv->micerr_flag = 0;
			}
#endif
		}
#endif // !NOT_RTK_BSP

		/*------------------------------------------------------------*/
		/*                insert sw encrypt here!                     */
		/*------------------------------------------------------------*/
#ifdef CONFIG_IEEE80211W
		if (txcfg->privacy && UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE), txcfg->isPMF))
#else
		if (txcfg->privacy && UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE)))
#endif			
		{
			if (txcfg->privacy == _TKIP_PRIVACY_ ||
				txcfg->privacy == _WEP_40_PRIVACY_ ||
				txcfg->privacy == _WEP_104_PRIVACY_)
			{
				picvdesc     = phdesc + *tx_head;
				picvdescinfo = pswdescinfo + *tx_head;
				//clear all bits
				memset(picvdesc, 0,32);

				if (txcfg->aggre_en == FG_AGGRE_MSDU_FIRST) {
					picvdesc->Dword0 = set_desc((get_desc(pdesc->Dword0) & (~TX_FirstSeg)) | TX_OWN);
				}
				else {
					picvdesc->Dword0   = set_desc((get_desc(pdesc->Dword0) & (~TX_FirstSeg)) | TX_OWN | TX_LastSeg);
				}

				picvdesc->Dword7 |= (set_desc(txcfg->icv & TX_TxBufSizeMask)); //TxBufferSize

				// append ICV first...
				picv = get_icv_from_poll(priv);
				if (picv == NULL)
				{
					DEBUG_ERR("System-Buf! can't alloc picv\n");
					BUG();
				}

				picvdescinfo->type = _PRE_ALLOCICVHDR_;
				picvdescinfo->pframe = picv;
#ifndef TXDESC_INFO					
				picvdescinfo->pstat = txcfg->pstat;
				picvdescinfo->rate = txcfg->tx_rate;
#endif				
				picvdescinfo->priv = priv;
				//TxBufferAddr

				picvdesc->Dword10 = set_desc(get_physical_addr(priv, picv, txcfg->icv, PCI_DMA_TODEVICE));

				if (i == 0)
					tkip_icv(picv,
						pwlhdr + txcfg->hdr_len + txcfg->iv, txcfg->llc,
						pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask));
				else
					tkip_icv(picv,
						NULL, 0,
						pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask));

				if ((i == 0) && (txcfg->llc != 0)) {
					if (txcfg->privacy == _TKIP_PRIVACY_)
						tkip_encrypt(priv, pwlhdr, txcfg->hdr_len,
							pwlhdr + txcfg->hdr_len + 8, sizeof(struct llc_snap),
							pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), picv, txcfg->icv);
					else
						wep_encrypt(priv, pwlhdr, txcfg->hdr_len,
							pwlhdr + txcfg->hdr_len + 4, sizeof(struct llc_snap),
							pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), picv, txcfg->icv,
							txcfg->privacy);
				}
				else { // not first segment or no snap header
					if (txcfg->privacy == _TKIP_PRIVACY_)
						tkip_encrypt(priv, pwlhdr, txcfg->hdr_len, NULL, 0,
							pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), picv, txcfg->icv);
					else
						wep_encrypt(priv, pwlhdr, txcfg->hdr_len, NULL, 0,
							pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), picv, txcfg->icv,
							txcfg->privacy);
				}
#ifndef USE_RTL8186_SDK
				rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(dma_txhead[*tx_head]-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(struct tx_desc), PCI_DMA_TODEVICE);
#endif

				flush_addr[flush_num]  = (unsigned long)bus_to_virt(get_desc(picvdesc->Dword10)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET);
				flush_len[flush_num++]=(get_desc(picvdesc->Dword7) & TX_TxBufSizeMask);

#ifdef OSK_LOW_TX_DESC
				if (q_num!=BE_QUEUE && q_num!=HIGH_QUEUE)
					txdesc_rollover_nonbe(picvdesc, (unsigned int *)tx_head);
				else
#endif
				txdesc_rollover(picvdesc, (unsigned int *)tx_head);
			}

			else if (txcfg->privacy == _CCMP_PRIVACY_)
			{
				pmicdesc = phdesc + *tx_head;
				pmicdescinfo = pswdescinfo + *tx_head;

				//clear all bits
				memset(pmicdesc, 0,32);
				
				if (txcfg->aggre_en == FG_AGGRE_MSDU_FIRST)
					pmicdesc->Dword0 = set_desc((get_desc(pdesc->Dword0) & (~TX_FirstSeg)) | TX_OWN);
				else
				  pmicdesc->Dword0   = set_desc((get_desc(pdesc->Dword0) & (~TX_FirstSeg)) | TX_OWN | TX_LastSeg);

				// set TxBufferSize
				pmicdesc->Dword7 = set_desc(txcfg->mic & TX_TxBufSizeMask);

				// append MIC first...
				pmic = get_mic_from_poll(priv);
				if (pmic == NULL)
				{
					DEBUG_ERR("System-Buf! can't alloc pmic\n");
					BUG();
				}

				pmicdescinfo->type = _PRE_ALLOCMICHDR_;
				pmicdescinfo->pframe = pmic;
#ifndef TXDESC_INFO					
				pmicdescinfo->pstat = txcfg->pstat;
				pmicdescinfo->rate = txcfg->tx_rate;
#endif				
				pmicdescinfo->priv = priv;
				// set TxBufferAddr
				pmicdesc->Dword10= set_desc(get_physical_addr(priv, pmic, txcfg->mic, PCI_DMA_TODEVICE));

				// then encrypt all (including ICV) by AES
				if ((i == 0)&&(txcfg->llc != 0)) // encrypt 3 segments ==> llc, mpdu, and mic
				{
				
#ifdef CONFIG_IEEE80211W
					if(isBIP) {
						BIP_encrypt(priv, pwlhdr, pwlhdr + txcfg->hdr_len + 8,
									pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), pmic,txcfg->isPMF);
					} 
					else {
						aesccmp_encrypt(priv, pwlhdr, pwlhdr + txcfg->hdr_len + 8,
										pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), pmic,txcfg->isPMF);
					}
#else
					aesccmp_encrypt(priv, pwlhdr, pwlhdr + txcfg->hdr_len + 8,
									pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), pmic);
#endif
				
				}
				else {// encrypt 2 segments ==> mpdu and mic
#ifdef CONFIG_IEEE80211W
					if(isBIP) 
					{
						BIP_encrypt(priv, pwlhdr, NULL,
									pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), pmic,txcfg->isPMF);
					}
					else
					{
						aesccmp_encrypt(priv, pwlhdr, NULL,
										pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), pmic,txcfg->isPMF);
					}
#else // !defined(CONFIG_IEEE80211W)
					aesccmp_encrypt(priv, pwlhdr, NULL,
									pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), pmic);
#endif // CONFIG_IEEE80211W
				}
#ifndef USE_RTL8186_SDK
				rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(dma_txhead[*tx_head]-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(struct tx_desc), PCI_DMA_TODEVICE);
#endif

				flush_addr[flush_num]=(unsigned long)bus_to_virt(get_desc(pmicdesc->Dword10)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET);
				flush_len[flush_num++]= (get_desc(pmicdesc->Dword7) & TX_TxBufSizeMask);

#ifdef OSK_LOW_TX_DESC
				if (q_num!=BE_QUEUE && q_num!=HIGH_QUEUE)
					txdesc_rollover_nonbe(pmicdesc, (unsigned int *)tx_head);
				else
#endif
				txdesc_rollover(pmicdesc, (unsigned int *)tx_head);
			}
		}

#ifdef RESERVE_TXDESC_FOR_EACH_IF
		if (RSVQ_ENABLE && IS_RSVQ(q_num)) {
			if (txcfg->privacy
#if defined(CONFIG_RTL_WAPI_SUPPORT)
				&& (_WAPI_SMS4_ != txcfg->privacy)
#endif		
#ifdef CONFIG_IEEE80211W
				&& UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE), txcfg->isPMF) )
#else
				&& UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE)) )
#endif						
			{						
				priv->use_txdesc_cnt[RSVQ(q_num)] += 3;
			}
			else
				priv->use_txdesc_cnt[RSVQ(q_num)] += 2;
		}
#endif

	}


init_deschead:
#if 0
	switch (q_select) {
	case 0:
	case 3:
	   if (q_num != BE_QUEUE)
    		printk("%s %d error : q_select[%d], q_num[%d]\n", __FUNCTION__, __LINE__, q_select, q_num);
	   break;
	case 1:
	case 2:
		if (q_num != BK_QUEUE)
		    printk("%s %d error : q_select[%d], q_num[%d]\n", __FUNCTION__, __LINE__, q_select, q_num);
	   break;
	case 4:
	case 5:
		if (q_num != VI_QUEUE)
		    printk("%s %d error : q_select[%d], q_num[%d]\n", __FUNCTION__, __LINE__, q_select, q_num);
		break;
	case 6:
	case 7:
		if (q_num != VO_QUEUE)
			printk("%s %d error : q_select[%d], q_num[%d]\n", __FUNCTION__, __LINE__, q_select, q_num);
		break;
	case 0x11 :
		 if (q_num != HIGH_QUEUE)
			printk("%s %d error : q_select[%d], q_num[%d]\n", __FUNCTION__, __LINE__, q_select, q_num);
		break;
	case 0x12 :
		if (q_num != MGNT_QUEUE)
			printk("%s %d error : q_select[%d], q_num[%d]\n", __FUNCTION__, __LINE__, q_select, q_num);
		break;
	default :
		printk("%s %d warning : q_select[%d], q_num[%d]\n", __FUNCTION__, __LINE__, q_select, q_num);
	break;
	}
#endif

	for (i=0; i<flush_num; i++)
		rtl_cache_sync_wback(priv, flush_addr[i], flush_len[i], PCI_DMA_TODEVICE);

	if (txcfg->aggre_en == FG_AGGRE_MSDU_FIRST) {
		priv->amsdu_first_desc = pfrstdesc;
#ifndef USE_RTL8186_SDK
		priv->amsdu_first_dma_desc = pfrst_dma_desc;
#endif
		priv->amsdu_len = get_desc(pfrstdesc->Dword0) & 0xffff; // get pktSize
		return;
	}

	pfrstdesc->Dword0 |= set_desc(TX_OWN);
#ifndef USE_RTL8186_SDK
	rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(pfrst_dma_desc-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(struct tx_desc), PCI_DMA_TODEVICE);
#endif

	if (q_num == HIGH_QUEUE) {
//		priv->pshare->pkt_in_hiQ = 1;
		priv->pkt_in_hiQ = 1;

		return;
	} else {
		tx_poll(priv, q_num);
	}

	return;
}
#endif // CONFIG_RTL_8812_SUPPORT || CONFIG_RTL_8723B_SUPPORT
#endif // CONFIG_PCI_HCI

 
#ifdef CONFIG_WLAN_HAL

inline void set_ampdu_aggregation_density(struct rtl8192cd_priv *priv, struct tx_insn *txcfg, PTX_DESC_DATA_88XX pdesc_data)
{
	u1Byte sta_density=0, privacy_density=7;
#if defined(CONFIG_WLAN_HAL_8822BE) || defined(CONFIG_WLAN_HAL_8198F) || defined(CONFIG_WLAN_HAL_8822CE) || defined(CONFIG_WLAN_HAL_8812FE) || defined(CONFIG_WLAN_HAL_8814BE)		
	if ((GET_CHIP_VER(priv) == VERSION_8822B)||(GET_CHIP_VER(priv) == VERSION_8198F)||(GET_CHIP_VER(priv) == VERSION_8822C)||(GET_CHIP_VER(priv) == VERSION_8812F)||(GET_CHIP_VER(priv)==VERSION_8814B))
		privacy_density = _HTCAP_AMPDU_SPC_2_US_;
#endif	
	sta_density = ((txcfg->pstat->ht_cap_buf.ampdu_para & _HTCAP_AMPDU_SPC_MASK_) >> _HTCAP_AMPDU_SPC_SHIFT_);

	if (txcfg->privacy && ! is_veriwave_testing(priv)) {
		// check sta's ability	
		if(privacy_density < sta_density)
			privacy_density = sta_density;
		pdesc_data->ampduDensity = privacy_density;
	}else{
		pdesc_data->ampduDensity = sta_density;
	}
}




void 
rtl88XX_fill_fwinfo(
    struct rtl8192cd_priv   *priv, 
    struct tx_insn          *txcfg, 
    unsigned int            frag_idx, 
    PTX_DESC_DATA_88XX      pdesc_data
)
{
    char n_txshort = 0, bg_txshort = 0;
//    int erp_protection = 0, n_protection = 0;
//    unsigned char rate;
    unsigned char txRate = 0;
#ifdef DRVMAC_LB
    static unsigned int rate_select = 0;
#endif
//	BOOLEAN			bRtsEnable;
	BOOLEAN			bErpProtect;
	BOOLEAN			bNProtect;
	BOOLEAN			bRtsThreshold;
//	BOOLEAN			bHwRts;
//	BOOLEAN			bCts2SelfEnable;
	unsigned char RtsRate;

#ifdef MP_TEST
    if (OPMODE & WIFI_MP_STATE) {
    #ifdef RTK_AC_SUPPORT
        if (is_VHT_rate(txcfg->tx_rate)) {
            txRate = (txcfg->tx_rate - VHT_RATE_ID) + 44;
        } 
        else 
    #endif            
        if (is_MCS_rate(txcfg->tx_rate)) {  // HT rates
            txRate = (txcfg->tx_rate - HT_RATE_ID) + 12;
        } 
        else{
            txRate = get_rate_index_from_ieee_value((UINT8)txcfg->tx_rate);
        }

		if (priv->pshare->is_40m_bw == 3 || priv->pshare->is_40m_bw == 4) {
            pdesc_data->dataSC  = 0x0;
            pdesc_data->RTSSC   = 0x0;
            pdesc_data->dataBW  = 0x3;	

            if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor160M)
                n_txshort = 1;
		}
        else if (priv->pshare->is_40m_bw == 2) {
            pdesc_data->dataSC  = 0x0;
            pdesc_data->RTSSC   = 0x0;
            pdesc_data->dataBW  = 0x2;

            if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor80M)
                n_txshort = 1;
        } 
        else if (priv->pshare->is_40m_bw == 1) {
            pdesc_data->dataSC  = 0x0;
            pdesc_data->RTSSC   = 0x0;
            pdesc_data->dataBW  = 0x1;

            if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor40M)
                n_txshort = 1;
        } 
        else {
            pdesc_data->dataSC  = 0x0;
            pdesc_data->RTSSC   = 0x0;
            pdesc_data->dataBW  = 0x0;
            
            if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor20M)
                n_txshort = 1;
        }

        if (txcfg->retry) { 
            pdesc_data->rtyLmtEn    = TRUE;
            pdesc_data->dataRtyLmt  = txcfg->retry;
        }

        pdesc_data->dataRate = txRate;
        if (priv->pmib->dot11RFEntry.phyBandSelect & PHY_BAND_5G)
            pdesc_data->RTSRate = 4;

		if(n_txshort == 1)
			pdesc_data->dataShort = 1;
		
        return;
    }
#endif

    if (is_MCS_rate(txcfg->tx_rate))    // HT rates
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

#ifdef RTK_AC_SUPPORT
		if ((priv->pshare->is_40m_bw == 3) || (priv->pshare->is_40m_bw == 4)) {
			if (txcfg->pstat && ((txcfg->pstat->tx_bw == CHANNEL_WIDTH_160 || txcfg->pstat->tx_bw == CHANNEL_WIDTH_80_80)
				&& (is_auto_rate(priv, txcfg->pstat) || is_fixedVHTTxRate(priv, txcfg->pstat))
			) )
			{
                pdesc_data->dataSC = 0;
                pdesc_data->RTSSC = 0;
                pdesc_data->dataBW = 3;                
                if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor160M &&
                    (txcfg->pstat->vht_cap_buf.vht_cap_info & cpu_to_le32(BIT(6))))
                        n_txshort = 1;				
			}
            else if(txcfg->pstat && (txcfg->pstat->tx_bw == CHANNEL_WIDTH_80
                && (is_auto_rate(priv, txcfg->pstat) || is_fixedVHTTxRate(priv, txcfg->pstat))
            ) )
            {
                pdesc_data->dataSC = priv->pshare->txsc_80;
                pdesc_data->RTSSC = priv->pshare->txsc_80;
                pdesc_data->dataBW = 2;                
                if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor80M &&
                    (txcfg->pstat->vht_cap_buf.vht_cap_info & cpu_to_le32(BIT(5))))
                        n_txshort = 1;
            } // TODO: Pedro, in 8812: is_MCS_rate(txcfg->tx_rate)
            else if(txcfg->pstat && (txcfg->pstat->tx_bw >= CHANNEL_WIDTH_40
                    && (is_auto_rate(priv, txcfg->pstat) || is_fixedMCSTxRate(priv, txcfg->pstat))        
                ) )
            {
                pdesc_data->dataSC = priv->pshare->txsc_40;
                pdesc_data->RTSSC  = priv->pshare->txsc_40;
                pdesc_data->dataBW = 1;
                if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor40M &&
                    (txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_40M_)))
                        n_txshort = 1;
            }
            else
            {
                pdesc_data->dataSC = priv->pshare->txsc_20;
                pdesc_data->RTSSC  = priv->pshare->txsc_20;
                if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor20M &&
                    txcfg->pstat && (txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_20M_)))
                        n_txshort = 1;
            }
		}
        else if (priv->pshare->is_40m_bw==2)
        {
            if(txcfg->pstat && (txcfg->pstat->tx_bw == CHANNEL_WIDTH_80
                && (is_auto_rate(priv, txcfg->pstat) || is_fixedVHTTxRate(priv, txcfg->pstat))
            ) )
            {
                pdesc_data->dataSC = 0;
                pdesc_data->RTSSC = 0;
                pdesc_data->dataBW = 2;                
                if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor80M &&
                    (txcfg->pstat->vht_cap_buf.vht_cap_info & cpu_to_le32(BIT(5))))
                        n_txshort = 1;
            } // TODO: Pedro, in 8812: is_MCS_rate(txcfg->tx_rate)
            else if(txcfg->pstat && (txcfg->pstat->tx_bw >= CHANNEL_WIDTH_40
                    && (is_auto_rate(priv, txcfg->pstat) || is_fixedMCSTxRate(priv, txcfg->pstat))        
                ) )
            {
                pdesc_data->dataSC = priv->pshare->txsc_40;
                pdesc_data->RTSSC  = priv->pshare->txsc_40;
                pdesc_data->dataBW = 1;
                if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor40M &&
                    (txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_40M_)))
                        n_txshort = 1;
            }
            else
            {
                pdesc_data->dataSC = priv->pshare->txsc_20;
                pdesc_data->RTSSC  = priv->pshare->txsc_20;
                if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor20M &&
                    txcfg->pstat && (txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_20M_)))
                        n_txshort = 1;
            }
        }
        else
#endif
        if (priv->pshare->is_40m_bw) {
            if (txcfg->pstat && (txcfg->pstat->tx_bw == CHANNEL_WIDTH_40)
#ifdef WIFI_11N_2040_COEXIST
                && !(COEXIST_ENABLE && (((OPMODE & WIFI_AP_STATE) && 
                (priv->bg_ap_timeout || orForce20_Switch20Map(priv)
                ))
#ifdef CLIENT_MODE
                || ((OPMODE & WIFI_STATION_STATE) && priv->coexist_connection && 
                (txcfg->pstat->ht_ie_len) && !(txcfg->pstat->ht_ie_buf.info0 & _HTIE_STA_CH_WDTH_))
#endif
                ))
#endif

                ) {
                pdesc_data->dataBW = 1;
                pdesc_data->dataSC = 3;
                {
                    if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor40M &&
                        (txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_40M_)))
                        n_txshort = 1;
                }
            }
            else {
                if (priv->pshare->offset_2nd_chan == HT_2NDCH_OFFSET_BELOW) {
                    pdesc_data->dataSC = 2;
                    pdesc_data->RTSSC  = 2;
                } else {
                    pdesc_data->dataSC = 1;
                    pdesc_data->RTSSC  = 1;
                }

                {
                    if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor20M &&
                        txcfg->pstat && (txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_20M_)))
                        n_txshort = 1;
                }
            }
        } else {
            {
                if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor20M &&
                    txcfg->pstat && (txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_20M_)))
                    n_txshort = 1;
            }
        }

        
        if ((txcfg->pstat && (txcfg->pstat->aggre_mthd & AGGRE_MTHD_MPDU) && txcfg->aggre_en)
            || ((txcfg->aggre_en >= FG_AGGRE_MPDU) && (txcfg->aggre_en <= FG_AGGRE_MPDU_BUFFER_LAST))) {
            int TID = ((struct sk_buff *)txcfg->pframe)->cb[1];
            if (txcfg->pstat && txcfg->pstat->ADDBA_ready[TID] && !txcfg->pstat->low_tp_disable_ampdu) {
                pdesc_data->aggEn = TRUE;

                /*
                 * assign aggr size
                 */
#ifdef CONFIG_WLAN_HAL
                // TODO: Is this for non-AC chip ???
                // TODO: check whether aggr size should be assigned at other location
#else
                if(GET_CHIP_VER(priv) != VERSION_8812E)
                {
                     if (priv->pshare->rf_ft_var.diffAmpduSz) {
                        pdesc->Dword6 |= set_desc((txcfg->pstat->diffAmpduSz & 0xffff) << TX_MCS1gMaxSHIFT | TX_UseMaxLen);
                        
                        if (GET_CHIP_VER(priv)!=VERSION_8812E)     
                        pdesc->Dword7 |= set_desc(txcfg->pstat->diffAmpduSz & 0xffff0000);
                     }  
                }
#endif // CONFIG_WLAN_HAL

				// assign aggr density
				set_ampdu_aggregation_density(priv, txcfg, pdesc_data);

                if (GET_CHIP_VER(priv) == VERSION_8814B)
                	pdesc_data->maxAggNum = 0; /* decided by reg MAX_AGG_NUM */
                else
					pdesc_data->maxAggNum = txcfg->pstat->maxAggNum >> 1;	// half of the max agg num	

				if (priv->pshare->rf_ft_var.txforce == 0xff && (
					((txcfg->pstat->current_tx_rate >= _MCS0_RATE_) && (txcfg->pstat->current_tx_rate <= _MCS2_RATE_)) 
					|| ((txcfg->pstat->current_tx_rate >= _MCS8_RATE_) && (txcfg->pstat->current_tx_rate <= _MCS10_RATE_)) 
#if 0/*def RTK_AC_SUPPORT*/
					||((txcfg->pstat->current_tx_rate >= _NSS1_MCS0_RATE_) && (txcfg->pstat->current_tx_rate <= _NSS1_MCS2_RATE_)) 
					||((txcfg->pstat->current_tx_rate >= _NSS2_MCS0_RATE_) && (txcfg->pstat->current_tx_rate <= _NSS2_MCS2_RATE_))
#endif				
				))
					pdesc_data->maxAggNum = 2;

#if defined(CONFIG_WLAN_HAL_8814AE) 	/* MAC HW TxRPT lost for 8814A, truncate aggregation number to 30 in small RSSI*/
				if (txcfg->pstat->rssi < 33 && pdesc_data->maxAggNum > 15)
					pdesc_data->maxAggNum = 15;
#endif		
#if 1 /*eric-8822 tp*/
				if(priv->pshare->rf_ft_var.aggforce)
					pdesc_data->maxAggNum = priv->pshare->rf_ft_var.aggforce;
#endif

#ifdef MULTI_STA_REFINE 					
				if(GET_CHIP_VER(priv) == VERSION_8192E) {
					if (txcfg->tx_rate < STA_TX_LOWRATE && pdesc_data->maxAggNum > 2)
						pdesc_data->maxAggNum = 2;
				}		
#ifdef SW_TX_QUEUE
				if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
					if (priv->pshare->swq_numActiveSTA > LOWAGGRESTA && pdesc_data->maxAggNum > 4
#ifdef CONFIG_SPECIAL_ENV_TEST
						&& !(priv->pshare->veriwave_sta_num)
#endif
					) {
						if (priv->pmib->dot11RFEntry.phyBandSelect & PHY_BAND_5G)
							pdesc_data->maxAggNum = 8;			// max agg# 16	
						else
							pdesc_data->maxAggNum = 4;			// max agg# 8
					}
				}
#endif						
#endif	
#if defined(CONFIG_WLAN_HAL_8814AE)
				if (GET_CHIP_VER(priv) == VERSION_8814A) {
					if (priv->pmib->dot11RFEntry.phyBandSelect & PHY_BAND_2G) {
						// Fix AMSDU over AMPDU throughput issue in Veriwave throughput testing
						// If aggregation number greater than 0x14, Veriwave wil not response BA such that TX rate drop down
						// For AC-88, if aggregation number > 0x14, there are many retransmissions.
						if ((txcfg->pstat->aggre_mthd == AGGRE_MTHD_MPDU_AMSDU) && (pdesc_data->maxAggNum > 10))
							pdesc_data->maxAggNum = 10;
					}
				}
#endif
            }
        }

        // for STBC
        if (txcfg->pstat)
		{
			if ((txcfg->pstat->cmn_info.stbc_en & HT_STBC_EN)
#ifdef RTK_AC_SUPPORT
				|| (txcfg->pstat->cmn_info.stbc_en & VHT_STBC_EN)
#endif
			) {
#if (BEAMFORMING_SUPPORT == 1)
				u1Byte					Idx = 0;	

				PRT_BEAMFORMING_ENTRY	pEntry;
				pEntry = Beamforming_GetEntryByMacId(priv, txcfg->pstat->cmn_info.aid, &Idx);
				if (pEntry == NULL || pEntry->Sounding_En == 0)
#endif
				if ((get_rf_mimo_mode(priv) != RF_1T1R) && (get_rf_mimo_mode(priv) != RF_1T2R))
    	       		pdesc_data->dataStbc = 1;
        	}
        }
		// LDPC
		
#if (BEAMFORMING_SUPPORT == 1)
		PRT_BEAMFORMING_INFO 		pBeamInfo = &(priv->pshare->BeamformingInfo);

		if(!txcfg->ndpa) 
#endif		
		if((txcfg->pstat) &&
			((txcfg->pstat->cmn_info.ldpc_en & HT_LDPC_EN)
#ifdef RTK_AC_SUPPORT
			|| (txcfg->pstat->cmn_info.ldpc_en & VHT_LDPC_EN)
#endif
		)
		) {
#if ((MU_BEAMFORMING_SUPPORT == 1) && defined(CONFIG_WLAN_HAL_8812FE) )
			if (GET_CHIP_VER(priv)==VERSION_8812F && (pBeamInfo->beamformee_mu_cnt >1 && txcfg->pstat->isMUCandidate)) {
				txcfg->pstat->descLDPCCnt++;
				pdesc_data->dataLdpc = 0;
			}
			else
#endif
			{
				pdesc_data->dataLdpc = 1;
			}
		}
    }
    else    // legacy rate
    {
        txRate = get_rate_index_from_ieee_value((UINT8)txcfg->tx_rate);
        if (is_CCK_rate(txcfg->tx_rate) && (txcfg->tx_rate != 2)) {
            if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11G) &&
                    (priv->pmib->dot11ErpInfo.longPreambleStaNum > 0))
                ; // txfw->txshort = 0
            else {
                if (txcfg->pstat)
                    bg_txshort = (priv->pmib->dot11RFEntry.shortpreamble) &&
                                    (txcfg->pstat->useShortPreamble);
                else
                    bg_txshort = priv->pmib->dot11RFEntry.shortpreamble;
            }
        }

#ifdef RTK_AC_SUPPORT
        if (priv->pshare->is_40m_bw==2 || priv->pshare->is_40m_bw==4) {
            pdesc_data->dataSC = priv->pshare->txsc_20;
            pdesc_data->RTSSC  = priv->pshare->txsc_20;
        }
        else
#endif
        if (priv->pshare->is_40m_bw) {
            if (priv->pshare->offset_2nd_chan == HT_2NDCH_OFFSET_BELOW) {
                pdesc_data->dataSC = 2;
                pdesc_data->RTSSC  = 2;
            } else {
                pdesc_data->dataSC = 1;
                pdesc_data->RTSSC  = 1;
            }
        }

        if (bg_txshort) {
            pdesc_data->dataShort = 1;
        }
            
    }
	
#if defined(CONFIG_WLAN_HAL_8192EE)
	if (GET_CHIP_VER(priv)==VERSION_8192E) {
		if( priv->pmib->dot11RFEntry.bcn2path )
			pdesc_data->TXAnt = 1; // use path A for 1ss CCK rate tx
		else if( (txcfg->pstat) && (txcfg->pstat->IOTPeer==HT_IOT_PEER_BROADCOM)&& (txcfg->pstat->cmn_info.ra_info.rate_id == 1))
			pdesc_data->TXAnt = 1;
		else if( priv->pmib->dot11RFEntry.tx2path )
			pdesc_data->TXAnt = 3;
		else
			pdesc_data->TXAnt = 1;
	}
#endif

    if (txcfg->need_ack) { // unicast
        if (frag_idx == 0) { // TODO: ..........how to do for hw tx shortcut (8814)
			u4Byte fr_len = pdesc_data->frLen;
			RtsCheck(priv, txcfg, fr_len, &pdesc_data->RTSEn, &pdesc_data->CTS2Self, &pdesc_data->HWRTSEn, &bErpProtect, &bNProtect, &bRtsThreshold);
		}
	}

#ifdef RTK_AC_SUPPORT 
	if(txcfg->pstat) //8812_11n_iot, only vht clnt support cca_rts, //for 11ac logo
		RtsCheckAC(priv, txcfg, &pdesc_data->RTSEn, &pdesc_data->CTS2Self, &pdesc_data->HWRTSEn);

	pdesc_data->cca_rts = priv->pshare->rf_ft_var.cca_rts; 

	if(priv->pshare->rf_ft_var.no_rtscts){
		pdesc_data->RTSEn		= 0;
		pdesc_data->CTS2Self    = 0;
		pdesc_data->HWRTSEn     = 0;
	}	
#endif
	
	RtsRate = find_rts_rate(priv, txcfg->tx_rate, bErpProtect);
	// Use Cts2Self instead when RtsRate < 11M && MPDU <= rts_thrshld
	if(pdesc_data->RTSEn && CheckCts2SelfEnable(RtsRate) && !bRtsThreshold)
	{
		pdesc_data->RTSEn = FALSE;
		pdesc_data->CTS2Self = TRUE;
	}
#ifdef CONFIG_VERIWAVE_CHECK
	if (is_veriwave_testing(priv)) {
		if (priv->pshare->veriwave_noisy_mode) {
			pdesc_data->RTSEn = TRUE;
			pdesc_data->CTS2Self = FALSE;
			pdesc_data->HWRTSEn = FALSE;
		} else {
			pdesc_data->RTSEn = FALSE;
			pdesc_data->CTS2Self = FALSE;
			pdesc_data->HWRTSEn = FALSE;
		}
	} else
#endif
#ifdef COCHANNEL_RTS
	if (priv->pmib->miscEntry.auto_rts_mode == 2)
	{
		pdesc_data->RTSEn = FALSE;
		pdesc_data->CTS2Self = FALSE;
		pdesc_data->HWRTSEn = FALSE;
	}
	else if (priv->pmib->miscEntry.auto_rts_mode == 3)
	{
		pdesc_data->RTSEn = TRUE;
		pdesc_data->CTS2Self = FALSE;
		pdesc_data->HWRTSEn = FALSE;
	}
#endif

    if (pdesc_data->CTS2Self || pdesc_data->RTSEn
#if (MU_BEAMFORMING_SUPPORT == 1)
		|| (priv->pmib->dot11RFEntry.txbf_mu && txcfg->pstat && txcfg->pstat->muPartner_num)
#endif
	) 
    {
            unsigned int rtsTxRateIdx   = get_rate_index_from_ieee_value(RtsRate);
            if (bErpProtect) {
                unsigned char  rtsShort = 0;
                if (is_CCK_rate(RtsRate) && (RtsRate != 2)) {
                    if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11G) &&
                        (priv->pmib->dot11ErpInfo.longPreambleStaNum > 0))
                        rtsShort = 0; // do nothing
                    else {
                        if (txcfg->pstat)
                            rtsShort = (priv->pmib->dot11RFEntry.shortpreamble) &&
                                            (txcfg->pstat->useShortPreamble);
                        else
                            rtsShort = priv->pmib->dot11RFEntry.shortpreamble;
                    }
                }
                pdesc_data->RTSShort = (rtsShort == 1) ? TRUE : FALSE;                
            } 

            pdesc_data->RTSRate = rtsTxRateIdx;
            if (GET_CHIP_VER(priv) >= VERSION_8814A && GET_CHIP_VER(priv) != VERSION_8192F) {
                if(priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G) {
                    pdesc_data->RTSRateFBLmt = 4;
                } else {
                    pdesc_data->RTSRateFBLmt = 0;
                }
            } else            
                pdesc_data->RTSRateFBLmt = 0xf;

    }

	if(txcfg->pstat && priv->pshare->rf_ft_var.txforce != 0xFF)
	{
		if(priv->pshare->rf_ft_var.sgiforce == 1)
			n_txshort = 1;
		else if(priv->pshare->rf_ft_var.sgiforce == 0)
			n_txshort = 0;
	}
    
	if(n_txshort == 1)
	if((!txcfg->pstat->cmn_info.ra_info.disable_ra) || (priv->pshare->rf_ft_var.sgiforce != 0xFF))    
        pdesc_data->dataShort = 1;
    

#ifdef DRVMAC_LB
    if (priv->pmib->miscEntry.drvmac_lb && (priv->pmib->miscEntry.lb_mlmp == 4)) {
        txRate = rate_select;
        if (rate_select++ > 0x1b)
            rate_select = 0;

        pdesc_data->disDataFB = TRUE;
        pdesc_data->disRTSFB  = TRUE;
        pdesc_data->useRate   = TRUE;
    }
#endif

	if((priv->pshare->rf_ft_var.txforce != 0xff || (txcfg->pstat && txcfg->pstat->force_rate)) 
#if (BEAMFORMING_SUPPORT == 1)
		&& (!txcfg->ndpa) 
#endif
	){
        pdesc_data->useRate  = TRUE;
		if(txcfg->pstat && txcfg->pstat->force_rate)
			pdesc_data->dataRate = 	txcfg->pstat->force_rate;
		else
	        pdesc_data->dataRate = priv->pshare->rf_ft_var.txforce;       
	} else {
        pdesc_data->dataRate = txRate;
    }


#if 1
    if (priv->pshare->rf_ft_var.rts_init_rate) {
        pdesc_data->RTSRate = priv->pshare->rf_ft_var.rts_init_rate;
    }      
    if ((priv->pmib->dot11RFEntry.phyBandSelect & PHY_BAND_5G) && 
        (pdesc_data->RTSRate)<4) {
        pdesc_data->RTSRate = 4;
    }
#else		
    if (priv->pmib->dot11RFEntry.phyBandSelect & PHY_BAND_5G)
        pdesc->Dword4 |= set_desc((4 & TX_RtsRateMask_8812) << TX_RtsRateSHIFT_8812);

#endif

    if (txcfg->need_ack) {
        // give retry limit to management frame
        if (txcfg->q_num == MANAGE_QUE_NUM) {
            pdesc_data->rtyLmtEn = TRUE;
            // TODO: ???? for hw tx sc... how to do
            if (GetFrameSubType(txcfg->phdr) == WIFI_PROBERSP) {
                ;   // 0 no need to set
                pdesc_data->dataRtyLmt = priv->pshare->rf_ft_var.probersp_retry;
            }
            #ifdef WDS
            else if ((GetFrameSubType(txcfg->phdr) == WIFI_PROBEREQ) && txcfg->pstat && (txcfg->pstat->state & WIFI_WDS)) {
                pdesc_data->dataRtyLmt = 2;
            }
            #endif
#ifdef CONFIG_IEEE80211W
            else if (txcfg->isPMF == 1 && 
                (GetFrameSubType(txcfg->phdr) == WIFI_DEAUTH || GetFrameSubType(txcfg->phdr) == WIFI_DISASSOC)) {
                pdesc_data->dataRtyLmt = 16;
            }
#endif
            else {
                pdesc_data->dataRtyLmt = 6;
            }
        }
        #ifdef WDS
        else if (txcfg->wdsIdx >= 0) {
            if (txcfg->pstat->rx_avarage == 0) {
                pdesc_data->rtyLmtEn = TRUE;
                pdesc_data->dataRtyLmt = 3;
            }
        }
        #endif
		else if (txcfg->pstat && !txcfg->fixed_rate)
        {
			int run = 0;
#if defined(CONFIG_PCI_HCI)
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
			if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
				run = 1;
			}
            if (run && (priv->pshare->rf_ft_var.low_retry_rssi_thd && txcfg->pstat->rssi
                && (txcfg->pstat->rssi < priv->pshare->rf_ft_var.low_retry_rssi_thd))) {
				if ((txcfg->tx_rate<= _9M_RATE_) ||
					((_MCS0_RATE_ == txcfg->tx_rate) || (_MCS1_RATE_ == txcfg->tx_rate))) {
                    pdesc_data->rtyLmtEn = TRUE;
                    pdesc_data->dataRtyLmt = 0x04;
                } else {
                    pdesc_data->rtyLmtEn = TRUE;
                    pdesc_data->dataRtyLmt = 0x0c;
                }
            } else if (run && (priv->pshare->rf_ft_var.low_rate_low_retry
					&& ((txcfg->tx_rate <= _9M_RATE_)
					|| (_MCS0_RATE_ == txcfg->tx_rate) || (_MCS1_RATE_ == txcfg->tx_rate)))) {
                pdesc_data->rtyLmtEn = TRUE;
                pdesc_data->dataRtyLmt = 0x08;
            } else
#endif
            if (is_MCS_rate(txcfg->tx_rate) && (txcfg->pstat->IOTPeer==HT_IOT_PEER_INTEL) && (txcfg->pstat->retry_inc)
                && !(txcfg->pstat->leave) && priv->pshare->intel_rty_lmt) {        
                pdesc_data->rtyLmtEn = TRUE;
                pdesc_data->dataRtyLmt = priv->pshare->intel_rty_lmt;
            } else if ((txcfg->pstat->IOTPeer==HT_IOT_PEER_BROADCOM) && (txcfg->pstat->retry_inc) && !(txcfg->pstat->leave)) {
                pdesc_data->rtyLmtEn = TRUE;
                pdesc_data->dataRtyLmt = 0x20;
            }
            #if defined(MULTI_STA_REFINE)
            else if((priv->pmib->dot11RFEntry.phyBandSelect & PHY_BAND_2G) && (priv->pshare->total_assoc_num > 30)) {
                pdesc_data->rtyLmtEn = TRUE;
                pdesc_data->dataRtyLmt = 0x0a;				 
            }
            #endif
        }

#ifdef THERMAL_CONTROL	
		/* fill in descriptor with -11dB if low_poewr = 1*/
		if(priv->pshare->rf_ft_var.low_power == 1){ /* decided by STA*/
			if(txcfg->pstat && txcfg->fr_type == _SKB_FRAME_TYPE_ && txcfg->pstat->power <= 3){
				pdesc_data->TXPowerOffset = txcfg->pstat->power;
			}
		}else if(priv->pshare->rf_ft_var.low_power == 2){ /* decided by power_desc*/		
			if(txcfg->pstat && txcfg->fr_type == _SKB_FRAME_TYPE_ && priv->pshare->rf_ft_var.current_power_desc <= 3){
				pdesc_data->TXPowerOffset = priv->pshare->rf_ft_var.current_power_desc;
			}
		}else
#endif		
		{		

        // High power
        if (priv->pshare->rf_ft_var.tx_pwr_ctrl && txcfg->pstat && (txcfg->fr_type == _SKB_FRAME_TYPE_)) {
            if (txcfg->pstat->hp_level == 1)
                pdesc_data->TXPowerOffset = priv->pshare->phw->TXPowerOffset;
        }
    }
#if (BEAMFORMING_SUPPORT == 1)
#if (MU_BEAMFORMING_SUPPORT == 1)
		pdesc_data->is_GID = txcfg->is_GID;
#endif		

        if(txcfg->ndpa) {
            unsigned char *pFrame = (unsigned char*)txcfg->phdr;
            if (1) { //(IsCtrlNDPA(pFrame) || IsMgntNDPA(pFrame)) { //eric-8822 tx hang
                pdesc_data->dataRtyLmt = 5;
                pdesc_data->rtyLmtEn   = TRUE;	
                if(IsMgntNDPA(pFrame))		//0xe0
                {
                    pdesc_data->ndpa = 1;
                }	
                else		// 0x54
                {
                    if(!IS_TEST_CHIP(priv) && (txcfg->pstat)) //eric-8822
                    {
                        if ((txcfg->pstat->wireless_mode & WIRELESS_MODE_AC_5G) && (txcfg->pstat->IOTPeer!=HT_IOT_PEER_BROADCOM))
                            pdesc_data->ndpa= 1; //eric-8822					
                        else
                            pdesc_data->ndpa= 1;

                    }
                    else
                    {
                        pdesc_data->ndpa = 1;
                    }
                }	
            }
            pdesc_data->SND_pkt_sel = txcfg->SND_pkt_sel; //eric-8822
#ifdef CONFIG_WLAN_HAL_8814BE
            pdesc_data->SND_target = txcfg->SND_target;
#endif
        }
#endif

    }

	if((txcfg->q_num == MANAGE_QUE_NUM) && (txcfg->fr_type == _PRE_ALLOCMEM_) && (GetFrameSubType(txcfg->phdr) == WIFI_PROBERSP))
	{
		if(priv->pmib->dot11RFEntry.bcnagc==1) {
			if(priv->pshare->rf_ft_var.bcn_pwr_idex+6 <= priv->pshare->rf_ft_var.bcn_pwr_max)
				pdesc_data->TXPowerOffset = 4; // +3 dB
		}
		else if ( priv->pmib->dot11RFEntry.bcnagc==2) {
			if(priv->pshare->rf_ft_var.bcn_pwr_idex+12 <= priv->pshare->rf_ft_var.bcn_pwr_max)
				pdesc_data->TXPowerOffset = 5; // +6 dB
			else if(priv->pshare->rf_ft_var.bcn_pwr_idex+6 <= priv->pshare->rf_ft_var.bcn_pwr_max)
				pdesc_data->TXPowerOffset = 4; // +3 dB
		}
#ifdef RF_MIMO_SWITCH
		if(priv->pshare->rf_ft_var.idle_ps_ext && priv->pshare->idle_txpwr_reduction) {
			if(priv->pshare->idle_txpwr_reduction <= 3) {
				pdesc_data->TXPowerOffset = priv->pshare->idle_txpwr_reduction;
			}
		}
#endif
	} 

	if (priv->pmib->dot11RFEntry.txpwr_reduction) {
		if (priv->pmib->dot11RFEntry.txpwr_reduction <= 3) {
			if (pdesc_data->TXPowerOffset < priv->pmib->dot11RFEntry.txpwr_reduction) {
				pdesc_data->TXPowerOffset = priv->pmib->dot11RFEntry.txpwr_reduction;
			}
		}
	}
}
 

// I AM not sure that if our Buffersize and PKTSize is right,
// If there are any problem, fix this first
#if(CONFIG_WLAN_NOT_HAL_EXIST==1)


void rtl8192cd_fill_fwinfo(struct rtl8192cd_priv *priv, struct tx_insn* txcfg, struct tx_desc  *pdesc, unsigned int frag_idx)
{
	char n_txshort = 0, bg_txshort = 0;
	//int erp_protection = 0, n_protection = 0;
	//unsigned char rate;
	unsigned char txRate = 0;
#ifdef DRVMAC_LB
	static unsigned int rate_select = 0;
#endif
	BOOLEAN			bRtsEnable = FALSE;
	BOOLEAN			bErpProtect = FALSE;
	BOOLEAN			bNProtect = FALSE;
	BOOLEAN			bHwRts = FALSE;
	BOOLEAN			bCts2SelfEnable = FALSE;
	BOOLEAN			bRtsThreshold = FALSE;
	unsigned char RtsRate;

#ifdef MP_TEST
	if (OPMODE & WIFI_MP_STATE) {
		if (is_MCS_rate(txcfg->tx_rate)) {	// HT rates
			txRate = (txcfg->tx_rate - HT_RATE_ID) + 12;
		}
		else{
			txRate = get_rate_index_from_ieee_value((UINT8)txcfg->tx_rate);
		}

		if (priv->pshare->CurrentChannelBW) {
			pdesc->Dword4 |= set_desc(TX_DataBw | (3&TX_DataScMask) << TX_DataScSHIFT);
			if (priv->pshare->CurrentChannelBW == CHANNEL_WIDTH_40 && priv->pmib->dot11nConfigEntry.dot11nShortGIfor40M)
				n_txshort = 1;
			else if (priv->pshare->CurrentChannelBW == CHANNEL_WIDTH_80 && priv->pmib->dot11nConfigEntry.dot11nShortGIfor80M)
				n_txshort = 1;
		}
		else {
			if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor20M)
				n_txshort = 1;
		}

		if (txcfg->retry)
			pdesc->Dword5 |= set_desc((txcfg->retry & TX_DataRtyLmtMask) << TX_DataRtyLmtSHIFT | TX_RtyLmtEn);

		if(n_txshort == 1)
			pdesc->Dword5 |= set_desc(TX_SGI);

		pdesc->Dword5 |= set_desc((txRate & TX_DataRateMask) << TX_DataRateSHIFT);
		
#ifdef CONFIG_RTL_92D_SUPPORT
		if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G) {
			pdesc->Dword4 |= set_desc((4 & TX_RtsRateMask) << TX_RtsRateSHIFT);
			//if (is_CCK_rate(txRate))
				//pdesc->Dword5 |= set_desc((4 & TX_DataRateMask) << TX_DataRateSHIFT);
		}
#endif

		return;
	}
#endif

	if (priv->pmib->dot11RFEntry.txbf == 1) {
		pdesc->Dword2 &= set_desc(0x03ffffff); // clear related bits
		pdesc->Dword2 |= set_desc(1 << TX_TxAntCckSHIFT); 	// Set Default CCK rate with 1T
		pdesc->Dword2 |= set_desc(1 << TX_TxAntlSHIFT); 	// Set Default Legacy rate with 1T
		pdesc->Dword2 |= set_desc(1 << TX_TxAntHtSHIFT); 	// Set Default Ht rate		
	}
	if(priv->pmib->dot11RFEntry.bcn2path){
		pdesc->Dword2 &= set_desc(0x03ffffff); // clear related bits
		pdesc->Dword2 |= set_desc(1 << TX_TxAntCckSHIFT);	// Set Default CCK rate with 1T
	}

	if (is_MCS_rate(txcfg->tx_rate))	// HT rates
	{
		txRate = (txcfg->tx_rate - HT_RATE_ID) + 12;
		
		if (priv->pmib->dot11RFEntry.txbf == 1) {
			if (txRate <= 0x12) {
				pdesc->Dword2 |= set_desc(3 << TX_TxAntHtSHIFT); // Set Ht rate < MCS6 with 2T
			}
		}

		if (priv->pshare->is_40m_bw) {
			if (txcfg->pstat && (txcfg->pstat->tx_bw == CHANNEL_WIDTH_40)
#ifdef WIFI_11N_2040_COEXIST
				&& !(COEXIST_ENABLE && (((OPMODE & WIFI_AP_STATE) && 
				(priv->bg_ap_timeout || orForce20_Switch20Map(priv)
				))
#ifdef CLIENT_MODE
				|| ((OPMODE & WIFI_STATION_STATE) && priv->coexist_connection && 
				(txcfg->pstat->ht_ie_len) && !(txcfg->pstat->ht_ie_buf.info0 & _HTIE_STA_CH_WDTH_))
#endif
				))
#endif

				) {
				pdesc->Dword4 |= set_desc(TX_DataBw | (3 << TX_DataScSHIFT));

#if defined(CONFIG_RTL_88E_SUPPORT) && defined(TXREPORT)
				if ((txcfg->fixed_rate) || (GET_CHIP_VER(priv)!=VERSION_8188E))
#endif
				{
					if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor40M &&
						txcfg->pstat && (txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_40M_)))
						n_txshort = 1;
				}
			}
			else {
				if (priv->pshare->offset_2nd_chan == HT_2NDCH_OFFSET_BELOW)
					pdesc->Dword4 |= set_desc((2 << TX_DataScSHIFT) | (2 << TX_RtsScSHIFT));
				else
					pdesc->Dword4 |= set_desc((1 << TX_DataScSHIFT) | (1 << TX_RtsScSHIFT));

#if defined(CONFIG_RTL_88E_SUPPORT) && defined(TXREPORT)
				if ((txcfg->fixed_rate) || (GET_CHIP_VER(priv)!=VERSION_8188E))
#endif
				{
					if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor20M &&
						txcfg->pstat && (txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_20M_)))
						n_txshort = 1;
				}
			}
		} else {
#if defined(CONFIG_RTL_88E_SUPPORT) && defined(TXREPORT)
			if ((txcfg->fixed_rate) || (GET_CHIP_VER(priv)!=VERSION_8188E))
#endif
			{
				if (priv->pmib->dot11nConfigEntry.dot11nShortGIfor20M &&
					txcfg->pstat && (txcfg->pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_20M_)))
					n_txshort = 1;
			}
		}

#if defined(CONFIG_RTL_88E_SUPPORT) && defined(TXREPORT)
		if ((GET_CHIP_VER(priv)==VERSION_8188E) && !(txcfg->fixed_rate)) {
			if (txcfg->pstat->ht_current_tx_info & TX_USE_SHORT_GI)
				n_txshort = 1;
		}
#endif

		if ((txcfg->aggre_en >= FG_AGGRE_MPDU) && (txcfg->aggre_en <= FG_AGGRE_MPDU_BUFFER_LAST)) {
			int TID = ((struct sk_buff *)txcfg->pframe)->cb[1];
			if (txcfg->pstat->ADDBA_ready[TID] && !txcfg->pstat->low_tp_disable_ampdu) {
#ifdef CONFIG_RTL_88E_SUPPORT
				if (GET_CHIP_VER(priv)==VERSION_8188E)
					pdesc->Dword2 |= set_desc(TXdesc_88E_AggEn);
				else
#endif
					pdesc->Dword1 |= set_desc(TX_AggEn);

				/*
				 * assign aggr size
				 */
				 if (priv->pshare->rf_ft_var.diffAmpduSz) {
					pdesc->Dword6 |= set_desc((txcfg->pstat->diffAmpduSz & 0xffff) << TX_MCS1gMaxSHIFT | TX_UseMaxLen);
					
#ifdef CONFIG_RTL_88E_SUPPORT
					if (GET_CHIP_VER(priv)!=VERSION_8188E)
#endif						
					pdesc->Dword7 |= set_desc(txcfg->pstat->diffAmpduSz & 0xffff0000);
				 } else if(	((txcfg->pstat->current_tx_rate >= _MCS0_RATE_) && (txcfg->pstat->current_tx_rate <= _MCS2_RATE_)) 
						||((txcfg->pstat->current_tx_rate >= _MCS8_RATE_) && (txcfg->pstat->current_tx_rate <= _MCS10_RATE_)) )				 	
				 {
					pdesc->Dword6 |= set_desc((0x4444) << TX_MCS1gMaxSHIFT | TX_UseMaxLen);					
#ifdef CONFIG_RTL_88E_SUPPORT
					if (GET_CHIP_VER(priv)!=VERSION_8188E)
#endif						
					pdesc->Dword7 |= set_desc(0x44440000);									 
				 }
				// assign aggr density
				if (txcfg->privacy) {
#ifdef CONFIG_RTL_92D_SUPPORT
					if ((priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G) && (!priv->pshare->is_40m_bw))
						pdesc->Dword2 |= set_desc(7 << TX_AmpduDstySHIFT);	// according to DWA-160 A2
					else
#endif
						pdesc->Dword2 |= set_desc(5 << TX_AmpduDstySHIFT);	// according to WN111v2
				}
				else {
					pdesc->Dword2 |= set_desc(((txcfg->pstat->ht_cap_buf.ampdu_para & _HTCAP_AMPDU_SPC_MASK_) >> _HTCAP_AMPDU_SPC_SHIFT_) << TX_AmpduDstySHIFT);
				}
			}
			//set Break
			if((txcfg->q_num >=1 && txcfg->q_num <=4)){
				if((txcfg->pstat != priv->pshare->CurPstat[txcfg->q_num-1])) {
#ifdef CONFIG_RTL_88E_SUPPORT
					if (GET_CHIP_VER(priv)==VERSION_8188E)
						pdesc->Dword2 |= set_desc(TXdesc_88E_BK);
					else
#endif
						pdesc->Dword1 |= set_desc(TX_BK);
					priv->pshare->CurPstat[txcfg->q_num-1] = txcfg->pstat;
				}				
			} else {
#ifdef CONFIG_RTL_88E_SUPPORT
				if (GET_CHIP_VER(priv)==VERSION_8188E)
					pdesc->Dword2 |= set_desc(TXdesc_88E_BK);
				else
#endif
					pdesc->Dword1 |= set_desc(TX_BK);
			}
		}

		// for STBC
#ifdef CONFIG_RTL_92C_SUPPORT
		if (GET_CHIP_VER(priv) != VERSION_8192C)
#endif			
		if (txcfg->pstat && (txcfg->pstat->cmn_info.stbc_en & HT_STBC_EN) &&
			((get_rf_mimo_mode(priv) == RF_2T2R) || (get_rf_mimo_mode(priv) == RF_3T3R)))
			pdesc->Dword4 |= set_desc(1 << TX_DataStbcSHIFT);
	}
	else	// legacy rate
	{
		txRate = get_rate_index_from_ieee_value((UINT8)txcfg->tx_rate);
		if (is_CCK_rate(txcfg->tx_rate) && (txcfg->tx_rate != 2)) {
			if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11G) &&
					(priv->pmib->dot11ErpInfo.longPreambleStaNum > 0))
				; // txfw->txshort = 0
			else {
				if (txcfg->pstat)
					bg_txshort = (priv->pmib->dot11RFEntry.shortpreamble) &&
									(txcfg->pstat->useShortPreamble);
				else
					bg_txshort = priv->pmib->dot11RFEntry.shortpreamble;
			}
		}
		if (priv->pshare->is_40m_bw) {
			if (priv->pshare->offset_2nd_chan == HT_2NDCH_OFFSET_BELOW)
				pdesc->Dword4 |= set_desc((2 << TX_DataScSHIFT) | (2 << TX_RtsScSHIFT));
			else
				pdesc->Dword4 |= set_desc((1 << TX_DataScSHIFT) | (1 << TX_RtsScSHIFT));	
		}

		if (bg_txshort)
			pdesc->Dword4 |= set_desc(TX_DataShort);
	}

	if (txcfg->need_ack) { // unicast
		if (frag_idx == 0) {
			u4Byte fr_len;
#if defined(CONFIG_PCI_HCI)
			if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
				fr_len = (get_desc(pdesc->Dword0)&TX_PktSizeMask) - (get_desc(pdesc->Dword7)&TX_TxBufSizeMask);
			}
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
			if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
				fr_len = txcfg->fr_len;	// for backward compatibile
			}
#endif
			RtsCheck(priv, txcfg, fr_len, &bRtsEnable, &bCts2SelfEnable, &bHwRts, &bErpProtect, &bNProtect, &bRtsThreshold);
		}
	}

	RtsRate = find_rts_rate(priv, txcfg->tx_rate, bErpProtect);
	// Use Cts2Self instead when RtsRate < 11M && MPDU <= rts_thrshld
	if(bRtsEnable && CheckCts2SelfEnable(RtsRate) && !bRtsThreshold)
	{
		bRtsEnable = FALSE;
		bCts2SelfEnable = TRUE;
	}

//	if(bErpProtect)
//		priv->pshare->phw->bErpProtection = TRUE;

	if(bRtsEnable)
		pdesc->Dword4 |= set_desc(TX_RtsEn);
	if(bCts2SelfEnable)
		pdesc->Dword4 |= set_desc(TX_CTS2Self);
	if(bHwRts)
		pdesc->Dword4 |=set_desc(TX_HwRtsEn);
		
	if (bRtsEnable || bCts2SelfEnable) {
			unsigned int rtsTxRateIdx  = get_rate_index_from_ieee_value(RtsRate);
			if (bErpProtect) {
				unsigned char  rtsShort = 0;
				if (is_CCK_rate(RtsRate) && (RtsRate != 2)) {
					if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11G) &&
							(priv->pmib->dot11ErpInfo.longPreambleStaNum > 0))
						rtsShort = 0; // do nothing
					else {
						if (txcfg->pstat)
							rtsShort = (priv->pmib->dot11RFEntry.shortpreamble) &&
											(txcfg->pstat->useShortPreamble);
						else
							rtsShort = priv->pmib->dot11RFEntry.shortpreamble;
					}
				}
				pdesc->Dword4 |= (rtsShort == 1)? set_desc(TX_RtsShort): 0;
			} 
			
			pdesc->Dword4 |= set_desc((rtsTxRateIdx & TX_RtsRateMask) << TX_RtsRateSHIFT);
			pdesc->Dword5 |= set_desc((0xf & TX_RtsRateFBLmtMask) << TX_RtsRateFBLmtSHIFT);
			//8192SE Must specified BW mode while sending RTS ...
			if (priv->pshare->is_40m_bw)
				pdesc->Dword4 |= set_desc(TX_RtsBw);

	}

	if(txcfg->pstat && priv->pshare->rf_ft_var.txforce != 0xFF)
	{
		if(priv->pshare->rf_ft_var.sgiforce == 1)
			n_txshort = 1;
		else if(priv->pshare->rf_ft_var.sgiforce == 0)
			n_txshort = 0;
	}	
	
	if(n_txshort == 1)
	if((!txcfg->pstat->cmn_info.ra_info.disable_ra) || (priv->pshare->rf_ft_var.sgiforce != 0xFF))
		pdesc->Dword5 |= set_desc(TX_SGI);


#ifdef DRVMAC_LB
	if (priv->pmib->miscEntry.drvmac_lb && (priv->pmib->miscEntry.lb_mlmp == 4)) {
		txRate = rate_select;
		if (rate_select++ > 0x1b)
			rate_select = 0;

		pdesc->Dword4 |= set_desc(TX_UseRate);
		pdesc->Dword4 |= set_desc(TX_DisDataFB);
		pdesc->Dword4 |= set_desc(TX_DisRtsFB);// disable RTS fall back
	}
#endif

	if(priv->pshare->rf_ft_var.txforce != 0xff)	{
		pdesc->Dword4 |= set_desc(TX_UseRate);
		pdesc->Dword5 |= set_desc((priv->pshare->rf_ft_var.txforce & TX_DataRateMask) << TX_DataRateSHIFT);
	} else {
		pdesc->Dword5 |= set_desc((txRate & TX_DataRateMask) << TX_DataRateSHIFT);
	}

#ifdef CONFIG_RTL_92D_SUPPORT
	if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G) {
		pdesc->Dword4 |= set_desc((4 & TX_RtsRateMask) << TX_RtsRateSHIFT);
		//if (is_CCK_rate(txRate))
			//pdesc->Dword5 |= set_desc((4 & TX_DataRateMask) << TX_DataRateSHIFT);
	}
#endif

	if (txcfg->need_ack) {
		int run = 0;

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
		if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
			run = 1;
		}
#endif

		// give retry limit to management frame
#ifndef DRVMAC_LB		
		if (txcfg->q_num == MANAGE_QUE_NUM) {
			pdesc->Dword5 |= set_desc(TX_RtyLmtEn);
			if (GetFrameSubType(txcfg->phdr) == WIFI_PROBERSP) {
				;	// 0 no need to set
			}
#ifdef WDS
			else if ((GetFrameSubType(txcfg->phdr) == WIFI_PROBEREQ) && txcfg->pstat && (txcfg->pstat->state & WIFI_WDS)) {
				pdesc->Dword5 |= set_desc((2 & TX_DataRtyLmtMask) << TX_DataRtyLmtSHIFT);
			}
#endif
#ifdef CONFIG_IEEE80211W
            else if (txcfg->isPMF == 1 && 
                (GetFrameSubType(txcfg->phdr) == WIFI_DEAUTH || GetFrameSubType(txcfg->phdr) == WIFI_DISASSOC)) {
                pdesc->Dword5 |= set_desc((16 & TX_DataRtyLmtMask) << TX_DataRtyLmtSHIFT);
            }
#endif
			else {
				pdesc->Dword5 |= set_desc((6 & TX_DataRtyLmtMask) << TX_DataRtyLmtSHIFT);
			}
		}
#ifdef WDS
		else if (txcfg->wdsIdx >= 0) {
			if (txcfg->pstat->rx_avarage == 0) {
				pdesc->Dword5 |= set_desc(TX_RtyLmtEn);
				pdesc->Dword5 |= set_desc((3 & TX_DataRtyLmtMask) << TX_DataRtyLmtSHIFT);
			}
		}
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
		else if (run && (priv->pshare->rf_ft_var.low_retry_rssi_thd && !txcfg->fixed_rate && txcfg->pstat->rssi
				&& (txcfg->pstat->rssi < priv->pshare->rf_ft_var.low_retry_rssi_thd))) {
			if ((txcfg->tx_rate <= _9M_RATE_) ||
				((_MCS0_RATE_ == txcfg->tx_rate) || (_MCS1_RATE_ == txcfg->tx_rate))) {
				pdesc->Dword5 |= set_desc(TX_RtyLmtEn);
				pdesc->Dword5 |= set_desc((0x04 & TX_DataRtyLmtMask) << TX_DataRtyLmtSHIFT);
			} else {
				pdesc->Dword5 |= set_desc(TX_RtyLmtEn);
				pdesc->Dword5 |= set_desc((0x08 & TX_DataRtyLmtMask) << TX_DataRtyLmtSHIFT);
			}
		} else if (run && (priv->pshare->rf_ft_var.low_rate_low_retry && !txcfg->fixed_rate
				&& ((txcfg->tx_rate <= _9M_RATE_)
				|| (_MCS0_RATE_ == txcfg->tx_rate) || (_MCS1_RATE_ == txcfg->tx_rate)))) {
			pdesc->Dword5 |= set_desc(TX_RtyLmtEn);
			pdesc->Dword5 |= set_desc((0x08 & TX_DataRtyLmtMask) << TX_DataRtyLmtSHIFT);
		}
#endif
		else if (is_MCS_rate(txcfg->tx_rate) && (txcfg->pstat->IOTPeer==HT_IOT_PEER_INTEL) && (txcfg->pstat->retry_inc)
			&& !(txcfg->pstat->leave) && priv->pshare->intel_rty_lmt) {
			pdesc->Dword5 |= set_desc(TX_RtyLmtEn);
			pdesc->Dword5 |= set_desc((priv->pshare->intel_rty_lmt & TX_DataRtyLmtMask) << TX_DataRtyLmtSHIFT);
		}

		else if ((txcfg->pstat->IOTPeer==HT_IOT_PEER_BROADCOM) && (txcfg->pstat->retry_inc) && !(txcfg->pstat->leave)) {
                pdesc->Dword5 |= set_desc(TX_RtyLmtEn);
                pdesc->Dword5 |= set_desc((0x20 & TX_DataRtyLmtMask) << TX_DataRtyLmtSHIFT);
        }
#endif //end DRVMAC_LB

		// High power mechanism
		//if ((GET_CHIP_VER(priv) == VERSION_8192C)||(GET_CHIP_VER(priv) == VERSION_8188C))

		{
			if (priv->pshare->rf_ft_var.tx_pwr_ctrl && txcfg->pstat && (txcfg->fr_type == _SKB_FRAME_TYPE_)) {
				if ((txcfg->pstat->hp_level == 1)
#if defined(CONFIG_RTL_92D_SUPPORT) && defined(CONFIG_RTL_NOISE_CONTROL)
					|| ((priv->pshare->DNC_on) && (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G)) 
#endif
					) {
						int pwr = (priv->pshare->rf_ft_var.min_pwr_idex > 16) ? 16: priv->pshare->rf_ft_var.min_pwr_idex;
						pwr &= 0x1e;
						pdesc->Dword6 |= set_desc(((-pwr) & TX_TxAgcAMask) << TX_TxAgcASHIFT);
						pdesc->Dword6 |= set_desc(((-pwr) & TX_TxAgcBMask) << TX_TxAgcBSHIFT);
					}
			}
		}
	}
}



void rtl8192cd_signin_txdesc(struct rtl8192cd_priv *priv, struct tx_insn* txcfg)
{
	struct tx_desc		*phdesc, *pdesc, *pndesc, *picvdesc, *pmicdesc, *pfrstdesc;
	struct tx_desc_info	*pswdescinfo, *pdescinfo, *pndescinfo, *picvdescinfo, *pmicdescinfo;
	unsigned int 		fr_len, tx_len, i, keyid;
	int					*tx_head, q_num;
	unsigned long		tmpphyaddr;
	unsigned char		*da, *pbuf, *pwlhdr, *pmic, *picv;
	struct rtl8192cd_hw	*phw;
	unsigned char		 q_select;
#ifdef TX_SHORTCUT
	int					fit_shortcut=0, idx=0;
#endif
	unsigned long		pfrst_dma_desc=0;
	unsigned long		*dma_txhead;

	unsigned long		flush_addr[20];
	int					flush_len[20];
	int					flush_num=0;

#ifdef TX_SCATTER
	int				actual_size = 0;
	struct sk_buff	*skb=NULL;
#endif
#ifdef CONFIG_IEEE80211W
	unsigned int	isBIP = 0;
#endif

#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT) 
	if (GET_CHIP_VER(priv)== VERSION_8812E || GET_CHIP_VER(priv)== VERSION_8723B) {
		rtl8192cd_signin_txdesc_8812(priv, txcfg);
		return;
	}
#endif

	picvdesc=NULL;
	keyid=0;
	pmic=NULL;
	picv=NULL;
	q_select=0;

	if (txcfg->tx_rate == 0) {
		DEBUG_ERR("tx_rate=0!\n");
		txcfg->tx_rate = find_rate(priv, NULL, 0, 1);
	}

	q_num = txcfg->q_num;

	phw	= GET_HW(priv);

	dma_txhead	= get_txdma_addr(phw, q_num);
	tx_head		= get_txhead_addr(phw, q_num);
	phdesc   	= get_txdesc(phw, q_num);
	pswdescinfo = get_txdesc_info(priv,priv->pshare->pdesc_info, q_num);

	da = get_da((unsigned char *)txcfg->phdr);
	
#ifdef CONFIG_IEEE80211W
	if(txcfg->isPMF && IS_MCAST(da)) 
	{
		isBIP = 1;
		txcfg->iv = 0;
		txcfg->fr_len += 10; // 10: MMIE length
	}
#endif
	tx_len = txcfg->fr_len;

	if (txcfg->fr_type == _SKB_FRAME_TYPE_)
		pbuf = ((struct sk_buff *)txcfg->pframe)->data;
	else
		pbuf = (unsigned char*)txcfg->pframe;

#ifdef TX_SCATTER
	if (txcfg->fr_type == _SKB_FRAME_TYPE_ &&
			((struct sk_buff *)txcfg->pframe)->list_num > 0) {
		skb = (struct sk_buff *)txcfg->pframe;
		actual_size = skb->len;
	} else {
		actual_size = tx_len;
	}

	tmpphyaddr = get_physical_addr(priv, pbuf, actual_size, PCI_DMA_TODEVICE);
#else
	tmpphyaddr = get_physical_addr(priv, pbuf, tx_len, PCI_DMA_TODEVICE);
#endif

	// in case of default key, then find the key id
	if (GetPrivacy((txcfg->phdr)))
	{
#ifdef WDS
		if (txcfg->wdsIdx >= 0) {
			if (txcfg->pstat)
				keyid = txcfg->pstat->keyid;
			else
				keyid = 0;
		}
		else
#endif

#ifdef __DRAYTEK_OS__
		if (!IEEE8021X_FUN)
			keyid = priv->pmib->dot1180211AuthEntry.dot11PrivacyKeyIndex;
		else {
			if (IS_MCAST(GetAddr1Ptr ((unsigned char *)txcfg->phdr)) || !txcfg->pstat)
				keyid = priv->pmib->dot11GroupKeysTable.keyid;
			else
				keyid = txcfg->pstat->keyid;
		}
#else

		if (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm==_WEP_40_PRIVACY_ ||
			priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm==_WEP_104_PRIVACY_) {
			if(IEEE8021X_FUN && txcfg->pstat) {
#ifdef A4_STA
				if (IS_MCAST(da) && !(txcfg->pstat->state & WIFI_A4_STA))
#else
				if(IS_MCAST(da))
#endif					
					keyid = 0;
				else
					keyid = txcfg->pstat->keyid;
			}
			else
				keyid = priv->pmib->dot1180211AuthEntry.dot11PrivacyKeyIndex;
		}
#endif


	}

	for(i=0, pfrstdesc= phdesc + (*tx_head); i < txcfg->frg_num; i++)
	{
		/*------------------------------------------------------------*/
		/*           fill descriptor of header + iv + llc             */
		/*------------------------------------------------------------*/
		pdesc     = phdesc + (*tx_head);
		pdescinfo = pswdescinfo + *tx_head;

		//clear all bits
		memset(pdesc, 0, 32);

		if (i != 0)
		{
			// we have to allocate wlan_hdr
			pwlhdr = (UINT8 *)get_wlanhdr_from_poll(priv);
			if (pwlhdr == (UINT8 *)NULL)
			{
				DEBUG_ERR("System-bug... should have enough wlan_hdr\n");
				return;
			}
			// other MPDU will share the same seq with the first MPDU
			if (txcfg->hdr_len <= sizeof(struct wlan_hdr))
				memcpy((void *)pwlhdr, (void *)(txcfg->phdr), txcfg->hdr_len); // data pkt has 24 bytes wlan_hdr
			else 
			{
				DEBUG_ERR("System-bug... should have enough wlan_hdr, hdr_len=%d\n", txcfg->hdr_len);
				return;
			}
			pdesc->Dword3 |= set_desc((GetSequence(txcfg->phdr) & TX_SeqMask)  << TX_SeqSHIFT);
		}
		else
		{
#ifdef WIFI_WMM
			if (txcfg->pstat && (is_qos_data(txcfg->phdr))) {
				if ((GetFrameSubType(txcfg->phdr) & (WIFI_DATA_TYPE | BIT(6) | BIT(7))) == (WIFI_DATA_TYPE | BIT(7))) {
					unsigned char tempQosControl[2];
					memset(tempQosControl, 0, 2);
					tempQosControl[0] = ((struct sk_buff *)txcfg->pframe)->cb[1];
#ifdef WMM_APSD
					if (
#ifdef CLIENT_MODE
						(OPMODE & WIFI_AP_STATE) &&
#endif
						(APSD_ENABLE) && (txcfg->pstat) && (txcfg->pstat->state & WIFI_SLEEP_STATE) &&
						(!GetMData(txcfg->phdr)) &&
						((((tempQosControl[0] == 7) || (tempQosControl[0] == 6)) && (txcfg->pstat->apsd_bitmap & 0x01)) ||
						 (((tempQosControl[0] == 5) || (tempQosControl[0] == 4)) && (txcfg->pstat->apsd_bitmap & 0x02)) ||
						 (((tempQosControl[0] == 3) || (tempQosControl[0] == 0)) && (txcfg->pstat->apsd_bitmap & 0x08)) ||
						 (((tempQosControl[0] == 2) || (tempQosControl[0] == 1)) && (txcfg->pstat->apsd_bitmap & 0x04))))
						tempQosControl[0] |= BIT(4);
#endif
					if (txcfg->aggre_en == FG_AGGRE_MSDU_FIRST)
						tempQosControl[0] |= BIT(7);

					if (priv->pmib->dot11nConfigEntry.dot11nTxNoAck)
						tempQosControl[0] |= BIT(5);

					memcpy((void *)GetQosControl((txcfg->phdr)), tempQosControl, 2);
				}
			}
#endif

			assign_wlanseq(GET_HW(priv), txcfg->phdr, txcfg->pstat, GET_MIB(priv)
#ifdef CONFIG_RTK_MESH	// For broadcast data frame via mesh (ex:ARP requst)
				, txcfg->is_11s
#endif
				);
			pdesc->Dword3 |= set_desc((GetSequence(txcfg->phdr) & TX_SeqMask)  << TX_SeqSHIFT);
			pwlhdr = txcfg->phdr;
		}
		SetDuration(pwlhdr, 0);

#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))
		if(txcfg->pstat)
		{
			odm_set_tx_ant_by_tx_info(priv, pdesc,txcfg->pstat->cmn_info.aid);   //N-series Set TX antenna	
		}
#endif
#if defined (HW_ANT_SWITCH) && (defined(CONFIG_RTL_92C_SUPPORT) || defined(CONFIG_RTL_92D_SUPPORT))
		pdesc->Dword2 &= set_desc(~ BIT(24));
		pdesc->Dword2 &= set_desc(~ BIT(25));
		if(!(priv->pshare->rf_ft_var.CurAntenna & 0x80) && (txcfg->pstat)) {
			pdesc->Dword2 |= set_desc(((txcfg->pstat->CurAntenna^priv->pshare->rf_ft_var.CurAntenna)&1)<<24);
			pdesc->Dword2 |= set_desc(((txcfg->pstat->CurAntenna^priv->pshare->rf_ft_var.CurAntenna)&1)<<25);
		}
#endif

		pdesc->Dword0 |= set_desc(32 << TX_OffsetSHIFT); // tx_desc size

		if (IS_MCAST(GetAddr1Ptr((unsigned char *)txcfg->phdr)))
			pdesc->Dword0 |= set_desc(TX_BMC);

#ifdef CLIENT_MODE
		if (OPMODE & WIFI_STATION_STATE) {
			if (GetFrameSubType(txcfg->phdr) == WIFI_PSPOLL)
				pdesc->Dword1 |= set_desc(TX_NAVUSEHDR);

			if (priv->ps_state)
				SetPwrMgt(pwlhdr);
			else
				ClearPwrMgt(pwlhdr);
		}
#endif

		if (i != (txcfg->frg_num - 1))
		{
			SetMFrag(pwlhdr);
			if (i == 0) {
				fr_len = (txcfg->frag_thrshld - txcfg->llc);
				tx_len -= (txcfg->frag_thrshld - txcfg->llc);
			}
			else {
				fr_len = txcfg->frag_thrshld;
				tx_len -= txcfg->frag_thrshld;
			}
		}
		else	// last seg, or the only seg (frg_num == 1)
		{
			fr_len = tx_len;
			ClearMFrag(pwlhdr);
		}
		SetFragNum((pwlhdr), i);

		if ((i == 0) && (txcfg->fr_type == _SKB_FRAME_TYPE_))
		{
			pdesc->Dword7 |= set_desc((txcfg->hdr_len + txcfg->llc) << TX_TxBufSizeSHIFT);
			pdesc->Dword0 |= set_desc((fr_len + (get_desc(pdesc->Dword7) & TX_TxBufSizeMask)) << TX_PktSizeSHIFT);
			pdesc->Dword0 |= set_desc(TX_FirstSeg);
			pdescinfo->type = _PRE_ALLOCLLCHDR_;
		}
		else
		{
			pdesc->Dword7 |= set_desc(txcfg->hdr_len << TX_TxBufSizeSHIFT);
			pdesc->Dword0 |= set_desc((fr_len + (get_desc(pdesc->Dword7) & TX_TxBufSizeMask)) << TX_PktSizeSHIFT);
			pdesc->Dword0 |= set_desc(TX_FirstSeg);
			pdescinfo->type = _PRE_ALLOCHDR_;
		}

		rtl8192cd_fill_fwinfo(priv, txcfg, pdesc, i);

		switch (q_num) {
		case HIGH_QUEUE:
			q_select = 0x11;// High Queue
			break;
		case MGNT_QUEUE:
			q_select = 0x12;
			break;
#if defined(DRVMAC_LB) && defined(WIFI_WMM)
		case BE_QUEUE:
			q_select = 0;
			break;
#endif
		default:
			// data packet
#ifdef RTL_MANUAL_EDCA
			if (priv->pmib->dot11QosEntry.ManualEDCA) {
				switch (q_num) {
				case VO_QUEUE:
					q_select = 6;
					break;
				case VI_QUEUE:
					q_select = 4;
					break;
				case BE_QUEUE:
					q_select = 0;
					break;
				default:
					q_select = 1;
					break;
				}
			}
			else
#endif
			q_select = ((struct sk_buff *)txcfg->pframe)->cb[1];
				break;
		}
		pdesc->Dword1 |= set_desc((q_select & TX_QSelMask)<< TX_QSelSHIFT);

		if (i != (txcfg->frg_num - 1))
			pdesc->Dword2 |= set_desc(TX_MoreFrag);

// Set RateID
		if (txcfg->pstat) {
			if (txcfg->pstat->cmn_info.aid != MANAGEMENT_AID) {
				u8 ratid;

#ifdef CONFIG_RTL_92D_SUPPORT
				if (priv->pmib->dot11RFEntry.phyBandSelect & PHY_BAND_5G){
					if (txcfg->pstat->tx_ra_bitmap & 0xffff000) {
						if (priv->pshare->is_40m_bw)
							ratid = ARFR_2T_Band_A_40M;
						else
							ratid = ARFR_2T_Band_A_20M;
					} else {
						ratid = ARFR_G_ONLY;
					}
				} else 
#endif
				{			
					if (txcfg->pstat->tx_ra_bitmap & 0xff00000) {
						if (priv->pshare->is_40m_bw)
							ratid = ARFR_2T_40M;
						else
							ratid = ARFR_2T_20M;
					} else if (txcfg->pstat->tx_ra_bitmap & 0xff000) {
						if (priv->pshare->is_40m_bw)
							ratid = ARFR_1T_40M;
						else
							ratid = ARFR_1T_20M;
					} else if (txcfg->pstat->tx_ra_bitmap & 0xff0) {
						ratid = ARFR_BG_MIX;
					} else {
						ratid = ARFR_B_ONLY;
					}


#ifdef P2P_SUPPORT	/*tx to GC no user B rate*/
					if(txcfg->pstat->is_p2p_client){
						switch(ratid) {
							case  ARFR_BG_MIX :
								ratid = ARFR_G_ONLY;
								break;
							default:
								ratid = ARFR_2T_Band_A_40M;		
						}
					}
#endif				
				}
				pdesc->Dword1 |= set_desc((ratid & TX_RateIDMask) << TX_RateIDSHIFT);
	// Set MacID
#ifdef CONFIG_RTL_88E_SUPPORT
				if (GET_CHIP_VER(priv)==VERSION_8188E)
					pdesc->Dword1 |= set_desc(REMAP_AID(txcfg->pstat) & TXdesc_88E_MacIdMask);
				else
#endif
					pdesc->Dword1 |= set_desc(REMAP_AID(txcfg->pstat) & TX_MacIdMask);
			}
		} else {
	
#ifdef CONFIG_RTL_92D_SUPPORT
		if (priv->pmib->dot11RFEntry.phyBandSelect & PHY_BAND_5G)
			pdesc->Dword1 |= set_desc((ARFR_Band_A_BMC &TX_RateIDMask)<<TX_RateIDSHIFT);
		else
#endif
			pdesc->Dword1 |= set_desc((ARFR_BMC &TX_RateIDMask)<<TX_RateIDSHIFT);
	}

		pdesc->Dword5 |= set_desc((0x1f & TX_DataRateFBLmtMask) << TX_DataRateFBLmtSHIFT);
		if (txcfg->fixed_rate)
			pdesc->Dword4 |= set_desc(TX_DisDataFB|TX_DisRtsFB|TX_UseRate);
#ifdef CONFIG_RTL_88E_SUPPORT
		else if (GET_CHIP_VER(priv)==VERSION_8188E)
			pdesc->Dword4 |= set_desc(TX_UseRate);
#endif

		if(txcfg->pstat &&  txcfg->pstat->cmn_info.ra_info.disable_ra)
			pdesc->Dword4 |= set_desc(TX_UseRate);

		if (!txcfg->need_ack && txcfg->privacy && 
#ifdef CONFIG_IEEE80211W
			UseSwCrypto(priv, NULL, TRUE, txcfg->isPMF)
#else
			UseSwCrypto(priv, NULL, TRUE)
#endif		
	 	)
			pdesc->Dword1 &= set_desc( ~(TX_SecTypeMask<< TX_SecTypeSHIFT));

		if (txcfg->privacy) {
#ifdef CONFIG_IEEE80211W
			if (UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE), txcfg->isPMF
#else
			if (UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE)
#endif				
			)){
				pdesc->Dword0 = set_desc(get_desc(pdesc->Dword0)+ txcfg->icv + txcfg->mic + txcfg->iv);
				pdesc->Dword7 = set_desc(get_desc(pdesc->Dword7)+ txcfg->iv);
			} else {
				// hw encrypt
				switch(txcfg->privacy) {
				case _WEP_104_PRIVACY_:
				case _WEP_40_PRIVACY_:
					pdesc->Dword0 = set_desc(get_desc(pdesc->Dword0) + txcfg->iv);
					pdesc->Dword7 = set_desc(get_desc(pdesc->Dword7) + txcfg->iv);
					wep_fill_iv(priv, pwlhdr, txcfg->hdr_len, keyid);
					pdesc->Dword1 |= set_desc(0x1 << TX_SecTypeSHIFT);
					break;

				case _TKIP_PRIVACY_:
					pdesc->Dword0 = set_desc(get_desc(pdesc->Dword0) + txcfg->iv + txcfg->mic);
					pdesc->Dword7 = set_desc(get_desc(pdesc->Dword7) + txcfg->iv);
					tkip_fill_encheader(priv, pwlhdr, txcfg->hdr_len, keyid);
					pdesc->Dword1 |= set_desc(0x1 << TX_SecTypeSHIFT);
					break;
				#if defined(CONFIG_RTL_HW_WAPI_SUPPORT)
				case _WAPI_SMS4_:
					pdesc->Dword0 = set_desc(get_desc(pdesc->Dword0) + txcfg->iv);
					pdesc->Dword7 = set_desc(get_desc(pdesc->Dword7) + txcfg->iv);
					
					pdesc->Dword1 |= set_desc(0x2 << TX_SecTypeSHIFT);
					break;
				#endif
				case _CCMP_PRIVACY_:
					//michal also hardware...
					pdesc->Dword0 = set_desc(get_desc(pdesc->Dword0) + txcfg->iv);
					pdesc->Dword7 = set_desc(get_desc(pdesc->Dword7) + txcfg->iv);
					aes_fill_encheader(priv, pwlhdr, txcfg->hdr_len, keyid);
					pdesc->Dword1 |= set_desc(0x3 << TX_SecTypeSHIFT);
					break;

				default:
					DEBUG_ERR("Unknow privacy\n");
					break;
				}
			}
		}

#ifdef TX_EARLY_MODE
		if (GET_TX_EARLY_MODE && pwlhdr && i == 0) {
			pdesc->Dword0 = set_desc((get_desc(pdesc->Dword0) & 0xff00ffff) | (0x28 << TX_OffsetSHIFT));
			pdesc->Dword1 = set_desc(get_desc(pdesc->Dword1) | (1 << TX_PktOffsetSHIFT));
#ifdef CONFIG_RTL_88E_SUPPORT
			if (GET_CHIP_VER(priv) == VERSION_8188E) {
				pdesc->Dword6 &= set_desc(~ (0xf << TX_MaxAggNumSHIFT));
				pdesc->Dword6 |= set_desc(0xf << TX_MaxAggNumSHIFT);
			}
#endif
			pdesc->Dword7 = set_desc(get_desc(pdesc->Dword7) + 8);

			memset(pwlhdr-8, '\0', 8);			
			if (txcfg->pstat && txcfg->pstat->empkt_num > 0) 			
				insert_emcontent(priv, txcfg, pwlhdr-8);
			pdesc->Dword8 = set_desc(get_physical_addr(priv, pwlhdr-8,
				get_desc(pdesc->Dword7)&TX_TxBufSizeMask, PCI_DMA_TODEVICE));	
		}
		else
#endif
		{
#if defined(TX_EARLY_MODE) && defined(CONFIG_RTL_88E_SUPPORT)
			if (GET_CHIP_VER(priv) == VERSION_8188E)
				pdesc->Dword6 &= set_desc(~ (0xf << TX_MaxAggNumSHIFT));
#endif
			pdesc->Dword8 = set_desc(get_physical_addr(priv, pwlhdr,
				(get_desc(pdesc->Dword7)&TX_TxBufSizeMask), PCI_DMA_TODEVICE));
		}

		// below is for sw desc info
#ifndef TXDESC_INFO
		pdescinfo->paddr  = get_desc(pdesc->Dword8);
#endif
		pdescinfo->pframe = pwlhdr;
#if defined(WIFI_WMM) && defined(WMM_APSD)
#if defined(ENABLE_RTL_SKB_STATS) || defined(RESERVE_TXDESC_FOR_EACH_IF)
		pdescinfo->priv = priv;
#endif
#ifndef TXDESC_INFO		
		pdescinfo->pstat = txcfg->pstat;
#endif		
#endif

#ifdef TX_SHORTCUT
		if (!priv->pmib->dot11OperationEntry.disable_txsc && txcfg->pstat &&
				(txcfg->fr_type == _SKB_FRAME_TYPE_) &&
				(txcfg->frg_num == 1) &&
				((txcfg->privacy == 0)
#ifdef CONFIG_RTL_WAPI_SUPPORT
				|| (txcfg->privacy == _WAPI_SMS4_)
#endif
#ifdef CONFIG_IEEE80211W
				|| !UseSwCrypto(priv, txcfg->pstat, FALSE,txcfg->isPMF)) &&
#else
				|| !UseSwCrypto(priv, txcfg->pstat, FALSE)) &&
#endif // CONFIG_IEEE80211W
				!GetMData(txcfg->phdr) ) {

#ifdef CONFIG_RTK_MESH
			if( txcfg->is_11s) {
				idx = get_tx_sc_free_entry(priv, txcfg->pstat, &priv->ethhdr, txcfg->aggre_en);
			} else
#endif
				idx = get_tx_sc_free_entry(priv, txcfg->pstat, pbuf - sizeof(struct wlan_ethhdr_t), txcfg->aggre_en);

#ifdef CONFIG_RTK_MESH
			if( txcfg->is_11s) {
				memcpy((void *)&txcfg->pstat->tx_sc_ent[idx].ethhdr, &priv->ethhdr, sizeof(struct wlan_ethhdr_t));
			} else
#endif
#ifdef TX_SCATTER
			if (((struct sk_buff *)txcfg->pframe)->list_num > 0)
				memcpy((void *)&txcfg->pstat->tx_sc_ent[idx].ethhdr, ((struct sk_buff *)txcfg->pframe)->list_buf[0].buf, sizeof(struct wlan_ethhdr_t));
			else	
#endif
				memcpy((void *)&txcfg->pstat->tx_sc_ent[idx].ethhdr, pbuf - sizeof(struct wlan_ethhdr_t), sizeof(struct wlan_ethhdr_t));

			desc_copy(&txcfg->pstat->tx_sc_ent[idx].hwdesc1, pdesc);
			descinfo_copy(&txcfg->pstat->tx_sc_ent[idx].swdesc1, pdescinfo);
			txcfg->pstat->protection = priv->pmib->dot11ErpInfo.protection;
			txcfg->pstat->ht_protection = priv->ht_protection;
			txcfg->pstat->tx_sc_ent[idx].sc_keyid = keyid;
			txcfg->pstat->tx_sc_ent[idx].pktpri = ((struct sk_buff *)txcfg->pframe)->cb[1];
			fit_shortcut = 1;
		}
		else {
			if (txcfg->pstat) {
				for (idx=0; idx<MAX_TXSC_ENTRY; idx++) {
					txcfg->pstat->tx_sc_ent[idx].hwdesc1.Dword7 &= set_desc(~TX_TxBufSizeMask);
#ifdef TX_SCATTER
					txcfg->pstat->tx_sc_ent[idx].has_desc3 = 0;
#endif
				}
			}
		}
#endif

		pfrst_dma_desc = dma_txhead[*tx_head];

		if (i != 0) {
			pdesc->Dword0 |= set_desc(TX_OWN);
#ifndef USE_RTL8186_SDK
			rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(dma_txhead[*tx_head]-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(struct tx_desc), PCI_DMA_TODEVICE);
#endif
		}

		flush_addr[flush_num]  = (unsigned long)bus_to_virt(get_desc(pdesc->Dword8)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET);
		flush_len[flush_num++]= (get_desc(pdesc->Dword7) & TX_TxBufSizeMask);

/*
		//printk desc content
		{
			unsigned int *ppdesc = (unsigned int *)pdesc;
			printk("%08x    %08x    %08x \n", get_desc(*(ppdesc)), get_desc(*(ppdesc+1)), get_desc(*(ppdesc+2)));
			printk("%08x    %08x    %08x \n", get_desc(*(ppdesc+3)), get_desc(*(ppdesc+4)), get_desc(*(ppdesc+5)));
			printk("%08x    %08x    %08x \n", get_desc(*(ppdesc+6)), get_desc(*(ppdesc+7)), get_desc(*(ppdesc+8)));
			printk("%08x\n", *(ppdesc+9));
			printk("===================================================\n");
		}
*/

#ifdef OSK_LOW_TX_DESC
		if (q_num!=BE_QUEUE && q_num!=HIGH_QUEUE)
			txdesc_rollover_nonbe(pdesc, (unsigned int *)tx_head);
		else
#endif
		txdesc_rollover(pdesc, (unsigned int *)tx_head);

		if (txcfg->fr_len == 0)
		{
			if (txcfg->aggre_en != FG_AGGRE_MSDU_FIRST)
				pdesc->Dword0 |= set_desc(TX_LastSeg);
			goto init_deschead;
		}

#ifdef TX_SCATTER
fill_body:
#endif
		/*------------------------------------------------------------*/
		/*              fill descriptor of frame body                 */
		/*------------------------------------------------------------*/
		pndesc     = phdesc + *tx_head;
		pndescinfo = pswdescinfo + *tx_head;
		//clear all bits
		memset(pndesc, 0,32);
		pndesc->Dword0 = set_desc((get_desc(pdesc->Dword0) & (~TX_FirstSeg)) | (TX_OWN));


#if defined (HW_ANT_SWITCH) && (defined(CONFIG_RTL_92C_SUPPORT) || defined(CONFIG_RTL_92D_SUPPORT))
		pndesc->Dword2 &= set_desc(~ BIT(24));
		pndesc->Dword2 &= set_desc(~ BIT(25));
		if(!(priv->pshare->rf_ft_var.CurAntenna & 0x80) && (txcfg->pstat)) {
			pndesc->Dword2 |= set_desc(((txcfg->pstat->CurAntenna^priv->pshare->rf_ft_var.CurAntenna)&1)<<24);
			pndesc->Dword2 |= set_desc(((txcfg->pstat->CurAntenna^priv->pshare->rf_ft_var.CurAntenna)&1)<<25);
		}
#endif


		if (txcfg->privacy)
		{
			if (txcfg->privacy == _WAPI_SMS4_)
			{
				if (txcfg->aggre_en != FG_AGGRE_MSDU_FIRST)
					pndesc->Dword0 |= set_desc(TX_LastSeg);
#ifndef TXDESC_INFO	
				pndescinfo->pstat = txcfg->pstat;
				pndescinfo->rate = txcfg->tx_rate;
#endif				
			}


#ifdef CONFIG_IEEE80211W
			else if (!UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE), txcfg->isPMF))
#else
			else if (!UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE)))
#endif		
			{
				if (txcfg->aggre_en != FG_AGGRE_MSDU_FIRST) {
#ifdef TX_SCATTER
					if (!skb || (skb && ((skb->list_idx+1) == skb->list_num))) 
#endif
						pndesc->Dword0 |= set_desc(TX_LastSeg);
				}
#ifndef TXDESC_INFO					
				pndescinfo->pstat = txcfg->pstat;
				pndescinfo->rate = txcfg->tx_rate;
#endif				
			}
		}
		else
		{
			if (txcfg->aggre_en != FG_AGGRE_MSDU_FIRST) {
#ifdef TX_SCATTER
				if (!skb || (skb && ((skb->list_idx+1) == skb->list_num)))
#endif
					pndesc->Dword0 |= set_desc(TX_LastSeg);
			}
#ifndef TXDESC_INFO				
			pndescinfo->pstat = txcfg->pstat;
			pndescinfo->rate = txcfg->tx_rate;
#endif			
		}
#ifdef TX_SCATTER
		if (skb != NULL)
			fr_len = actual_size;
#endif		

#ifdef CONFIG_RTL_WAPI_SUPPORT
#if defined(CONFIG_RTL_HW_WAPI_SUPPORT)
#ifdef CONFIG_IEEE80211W
		if ((txcfg->privacy == _WAPI_SMS4_)&&(UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE), txcfg->isPMF)))
#else
		if ((txcfg->privacy == _WAPI_SMS4_)&&(UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE))))
#endif	// CONFIG_IEEE80211W
#else
		if (txcfg->privacy == _WAPI_SMS4_)
#endif
		{
			pndesc->Dword7 |= set_desc( (fr_len+SMS4_MIC_LEN) & TX_TxBufSizeMask);
		}
		else
#endif
		pndesc->Dword7 |= set_desc(fr_len & TX_TxBufSizeMask);

#ifdef TX_SCATTER
		if (skb && (skb->list_num > 0)) {
			if (get_desc(pndesc->Dword0) & TX_LastSeg)
				pndescinfo->type = txcfg->fr_type;
			else
				pndescinfo->type = _RESERVED_FRAME_TYPE_;
		} else
#endif
		{
			if (i == 0)
				pndescinfo->type = txcfg->fr_type;
			else
				pndescinfo->type = _RESERVED_FRAME_TYPE_;
		}

#if defined(CONFIG_RTK_MESH) && defined(MESH_USE_METRICOP)
		if( (txcfg->fr_type == _PRE_ALLOCMEM_) && (txcfg->is_11s & 128)) // for 11s link measurement frame
			pndescinfo->type =_RESERVED_FRAME_TYPE_;
#endif

		pndesc->Dword8 = set_desc(tmpphyaddr); //TxBufferAddr
#ifndef TXDESC_INFO
		pndescinfo->paddr = get_desc(pndesc->Dword8);
#endif		
		pndescinfo->pframe = txcfg->pframe;
#ifndef TXDESC_INFO		
		pndescinfo->len = txcfg->fr_len;	// for pci_unmap_single
#endif
#if defined(ENABLE_RTL_SKB_STATS) || defined(RESERVE_TXDESC_FOR_EACH_IF)
		pndescinfo->priv = priv;
#endif

		pbuf += fr_len;
		tmpphyaddr += fr_len;

#ifdef TX_SHORTCUT
		if (fit_shortcut) {
#ifdef TX_SCATTER
			if (txcfg->pstat->tx_sc_ent[idx].has_desc3) {
				desc_copy(&txcfg->pstat->tx_sc_ent[idx].hwdesc3, pndesc);
				descinfo_copy(&txcfg->pstat->tx_sc_ent[idx].swdesc3, pndescinfo);
			} else
#endif
			{
				desc_copy(&txcfg->pstat->tx_sc_ent[idx].hwdesc2, pndesc);
				descinfo_copy(&txcfg->pstat->tx_sc_ent[idx].swdesc2, pndescinfo);
#ifdef TXSC_CACHE_FRLEN				
				txcfg->pstat->tx_sc_ent[idx].frlen = fr_len;
#endif				
			}
		}
#endif

#ifndef USE_RTL8186_SDK
		rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(dma_txhead[*tx_head]-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(struct tx_desc), PCI_DMA_TODEVICE);
#endif

		flush_addr[flush_num]=(unsigned long)bus_to_virt(get_desc(pndesc->Dword8)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET);
		flush_len[flush_num++] = get_desc(pndesc->Dword7) & TX_TxBufSizeMask;

#ifdef OSK_LOW_TX_DESC
		if (q_num!=BE_QUEUE && q_num!=HIGH_QUEUE)
			txdesc_rollover_nonbe(pdesc, (unsigned int *)tx_head);
		else
#endif
		txdesc_rollover(pndesc, (unsigned int *)tx_head);

		// retrieve H/W MIC and put in payload
#ifdef CONFIG_RTL_WAPI_SUPPORT
		if (txcfg->privacy == _WAPI_SMS4_)
		{
			SecSWSMS4Encryption(priv, txcfg);
#if 0
			if (txcfg->mic>0)
			{
				pndesc->Dword7 &= set_desc(~TX_TxBufSizeMask);
				pndesc->Dword7 |= set_desc((fr_len+txcfg->mic)& TX_TxBufSizeMask);
				flush_len[flush_num-1]= (get_desc(pndesc->Dword7) & TX_TxBufSizeMask);
			}
			else
			{
				txcfg->mic = SMS4_MIC_LEN;
			}
#endif
		
		}
#endif

#if !defined(NOT_RTK_BSP) && !defined(SUPPORT_TKIP_OSK_GDMA)
		if ((txcfg->privacy == _TKIP_PRIVACY_) &&
			(priv->pshare->have_hw_mic) &&
			!(priv->pmib->dot11StationConfigEntry.swTkipMic) &&
			(i == (txcfg->frg_num-1)) )	// last segment
		{
			register unsigned long int l,r;
			unsigned char *mic;
			volatile int i;

			while ((*(volatile unsigned int *)GDMAISR & GDMA_COMPIP) == 0)
				for (i=0; i<10; i++)
					;

			l = *(volatile unsigned int *)GDMAICVL;
			r = *(volatile unsigned int *)GDMAICVR;

			mic = ((struct sk_buff *)txcfg->pframe)->data + txcfg->fr_len - 8;
			mic[0] = (unsigned char)(l & 0xff);
			mic[1] = (unsigned char)((l >> 8) & 0xff);
			mic[2] = (unsigned char)((l >> 16) & 0xff);
			mic[3] = (unsigned char)((l >> 24) & 0xff);
			mic[4] = (unsigned char)(r & 0xff);
			mic[5] = (unsigned char)((r >> 8) & 0xff);
			mic[6] = (unsigned char)((r >> 16) & 0xff);
			mic[7] = (unsigned char)((r >> 24) & 0xff);

#ifdef MICERR_TEST
			if (priv->micerr_flag) {
				mic[7] ^= mic[7];
				priv->micerr_flag = 0;
			}
#endif
		}
#endif // !NOT_RTK_BSP && !SUPPORT_TKIP_OSK_GDMA

		/*------------------------------------------------------------*/
		/*                insert sw encrypt here!                     */
		/*------------------------------------------------------------*/
		if (txcfg->privacy &&
#ifdef CONFIG_IEEE80211W
			UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE), txcfg->isPMF))
#else
			UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE)))
#endif
		{
			if (txcfg->privacy == _TKIP_PRIVACY_ ||
				txcfg->privacy == _WEP_40_PRIVACY_ ||
				txcfg->privacy == _WEP_104_PRIVACY_)
			{
				picvdesc     = phdesc + *tx_head;
				picvdescinfo = pswdescinfo + *tx_head;
				//clear all bits
				memset(picvdesc, 0,32);

				if (txcfg->aggre_en == FG_AGGRE_MSDU_FIRST){
					picvdesc->Dword0 = set_desc((get_desc(pdesc->Dword0) & (~TX_FirstSeg)) | TX_OWN);
				}
				else{
					picvdesc->Dword0   = set_desc((get_desc(pdesc->Dword0) & (~TX_FirstSeg)) | TX_OWN | TX_LastSeg);
				}

				picvdesc->Dword7 |= (set_desc(txcfg->icv & TX_TxBufSizeMask)); //TxBufferSize

				// append ICV first...
				picv = get_icv_from_poll(priv);
				if (picv == NULL)
				{
					DEBUG_ERR("System-Buf! can't alloc picv\n");
					BUG();
				}

				picvdescinfo->type = _PRE_ALLOCICVHDR_;
				picvdescinfo->pframe = picv;
#ifndef TXDESC_INFO					
				picvdescinfo->pstat = txcfg->pstat;
				picvdescinfo->rate = txcfg->tx_rate;
#endif
#if defined(ENABLE_RTL_SKB_STATS) || defined(RESERVE_TXDESC_FOR_EACH_IF)
				picvdescinfo->priv = priv;
#endif
				//TxBufferAddr
				picvdesc->Dword8 = set_desc(get_physical_addr(priv, picv, txcfg->icv, PCI_DMA_TODEVICE));

				if (i == 0)
					tkip_icv(picv,
						pwlhdr + txcfg->hdr_len + txcfg->iv, txcfg->llc,
						pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask));
				else
					tkip_icv(picv,
						NULL, 0,
						pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask));

				if ((i == 0) && (txcfg->llc != 0)) {
					if (txcfg->privacy == _TKIP_PRIVACY_)
						tkip_encrypt(priv, pwlhdr, txcfg->hdr_len,
							pwlhdr + txcfg->hdr_len + 8, sizeof(struct llc_snap),
							pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), picv, txcfg->icv);
					else
						wep_encrypt(priv, pwlhdr, txcfg->hdr_len,
							pwlhdr + txcfg->hdr_len + 4, sizeof(struct llc_snap),
							pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), picv, txcfg->icv,
							txcfg->privacy);
				}
				else { // not first segment or no snap header
					if (txcfg->privacy == _TKIP_PRIVACY_)
						tkip_encrypt(priv, pwlhdr, txcfg->hdr_len, NULL, 0,
							pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), picv, txcfg->icv);
					else
						wep_encrypt(priv, pwlhdr, txcfg->hdr_len, NULL, 0,
							pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), picv, txcfg->icv,
							txcfg->privacy);
				}
#ifndef USE_RTL8186_SDK
				rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(dma_txhead[*tx_head]-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(struct tx_desc), PCI_DMA_TODEVICE);
#endif

				flush_addr[flush_num]  = (unsigned long)bus_to_virt(get_desc(picvdesc->Dword8)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET);
				flush_len[flush_num++]=(get_desc(picvdesc->Dword7) & TX_TxBufSizeMask);
#ifdef OSK_LOW_TX_DESC
				if (q_num!=BE_QUEUE && q_num!=HIGH_QUEUE)
					txdesc_rollover_nonbe(picvdesc, (unsigned int *)tx_head);
				else
#endif
				txdesc_rollover(picvdesc, (unsigned int *)tx_head);
			}

			else if (txcfg->privacy == _CCMP_PRIVACY_)
			{
				pmicdesc = phdesc + *tx_head;
				pmicdescinfo = pswdescinfo + *tx_head;

				//clear all bits
				memset(pmicdesc, 0,32);
				
				if (txcfg->aggre_en == FG_AGGRE_MSDU_FIRST)
					pmicdesc->Dword0 = set_desc((get_desc(pdesc->Dword0) & (~TX_FirstSeg)) | TX_OWN);
				else
				  pmicdesc->Dword0   = set_desc((get_desc(pdesc->Dword0) & (~TX_FirstSeg)) | TX_OWN | TX_LastSeg);

				// set TxBufferSize
				pmicdesc->Dword7 = set_desc(txcfg->mic & TX_TxBufSizeMask);

				// append MIC first...
				pmic = get_mic_from_poll(priv);
				if (pmic == NULL)
				{
					DEBUG_ERR("System-Buf! can't alloc pmic\n");
					BUG();
				}

				pmicdescinfo->type = _PRE_ALLOCMICHDR_;
				pmicdescinfo->pframe = pmic;
#ifndef TXDESC_INFO					
				pmicdescinfo->pstat = txcfg->pstat;
				pmicdescinfo->rate = txcfg->tx_rate;
#endif			
#if defined(ENABLE_RTL_SKB_STATS) || defined(RESERVE_TXDESC_FOR_EACH_IF)
				pmicdescinfo->priv = priv;
#endif
				// set TxBufferAddr
				pmicdesc->Dword8= set_desc(get_physical_addr(priv, pmic, txcfg->mic, PCI_DMA_TODEVICE));

				// then encrypt all (including ICV) by AES
				if ((i == 0)&&(txcfg->llc != 0)) // encrypt 3 segments ==> llc, mpdu, and mic
				{
#ifdef CONFIG_IEEE80211W
					if(isBIP) 
					{
						BIP_encrypt(priv, pwlhdr, pwlhdr + txcfg->hdr_len + 8,
									pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), pmic,txcfg->isPMF);
					}
					else {
						aesccmp_encrypt(priv, pwlhdr, pwlhdr + txcfg->hdr_len + 8,
										pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), pmic,txcfg->isPMF);
					}
#else
					aesccmp_encrypt(priv, pwlhdr, pwlhdr + txcfg->hdr_len + 8,
									pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), pmic);
#endif	
				}
				else { // encrypt 2 segments ==> mpdu and mic
#ifdef CONFIG_IEEE80211W
					if(isBIP) 
					{
						BIP_encrypt(priv, pwlhdr, NULL,
							pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), pmic,txcfg->isPMF);
					}
					else {
						aesccmp_encrypt(priv, pwlhdr, NULL,
							pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), pmic,txcfg->isPMF);
					}
#else // !defined(CONFIG_IEEE80211W)
					aesccmp_encrypt(priv, pwlhdr, NULL,
						pbuf - (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), (get_desc(pndesc->Dword7) & TX_TxBufSizeMask), pmic);
#endif // CONFIG_IEEE80211W
				}
#ifndef USE_RTL8186_SDK
				rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(dma_txhead[*tx_head]-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(struct tx_desc), PCI_DMA_TODEVICE);
#endif
				flush_addr[flush_num]=(unsigned long)bus_to_virt(get_desc(pmicdesc->Dword8)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET);
				flush_len[flush_num++]= (get_desc(pmicdesc->Dword7) & TX_TxBufSizeMask);
#ifdef OSK_LOW_TX_DESC
				if (q_num!=BE_QUEUE && q_num!=HIGH_QUEUE)
					txdesc_rollover_nonbe(pmicdesc, (unsigned int *)tx_head);
				else
#endif
				txdesc_rollover(pmicdesc, (unsigned int *)tx_head);
			}
		}

#ifdef RESERVE_TXDESC_FOR_EACH_IF
		if (RSVQ_ENABLE && IS_RSVQ(q_num)) {
			if ( txcfg->privacy
#if defined(CONFIG_RTL_WAPI_SUPPORT)
				&& (_WAPI_SMS4_ != txcfg->privacy)
#endif		
#ifdef CONFIG_IEEE80211W
				&& UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE), txcfg->isPMF) )
#else
				&& UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE)) )
#endif						
			{
				priv->use_txdesc_cnt[RSVQ(q_num)] += 3;
			}
			else
				priv->use_txdesc_cnt[RSVQ(q_num)] += 2;
		}
#endif

#ifdef TX_SCATTER
		if (skb && ++skb->list_idx < skb->list_num) {
			skb_assign_buf(skb, skb->list_buf[skb->list_idx].buf, skb->list_buf[skb->list_idx].len);
			skb->len = skb->list_buf[skb->list_idx].len;
			pbuf = skb->data;
			actual_size = skb->len;
			tmpphyaddr = get_physical_addr(priv, pbuf, actual_size, PCI_DMA_TODEVICE);
#ifdef TX_SHORTCUT
			if (txcfg->pstat) {
				if (txcfg->pstat->tx_sc_ent[idx].has_desc3) {
					fit_shortcut = 0;
					txcfg->pstat->tx_sc_ent[idx].has_desc3 = 0;
					for (idx=0; idx<MAX_TXSC_ENTRY; idx++)
						txcfg->pstat->tx_sc_ent[idx].hwdesc1.Dword7 &= set_desc(~TX_TxBufSizeMask);
				} else {
					txcfg->pstat->tx_sc_ent[idx].has_desc3 = 1;
				}
			}
#endif
			goto fill_body;
		}
#endif
	}


init_deschead:
#if 0
	switch (q_select) {
	case 0:
	case 3:
	   if (q_num != BE_QUEUE)
    		printk("%s %d error : q_select[%d], q_num[%d]\n", __FUNCTION__, __LINE__, q_select, q_num);
	   break;
	case 1:
	case 2:
		if (q_num != BK_QUEUE)
		    printk("%s %d error : q_select[%d], q_num[%d]\n", __FUNCTION__, __LINE__, q_select, q_num);
	   break;
	case 4:
	case 5:
		if (q_num != VI_QUEUE)
		    printk("%s %d error : q_select[%d], q_num[%d]\n", __FUNCTION__, __LINE__, q_select, q_num);
		break;
	case 6:
	case 7:
		if (q_num != VO_QUEUE)
			printk("%s %d error : q_select[%d], q_num[%d]\n", __FUNCTION__, __LINE__, q_select, q_num);
		break;
	case 0x11 :
		 if (q_num != HIGH_QUEUE)
			printk("%s %d error : q_select[%d], q_num[%d]\n", __FUNCTION__, __LINE__, q_select, q_num);
		break;
	case 0x12 :
		if (q_num != MGNT_QUEUE)
			printk("%s %d error : q_select[%d], q_num[%d]\n", __FUNCTION__, __LINE__, q_select, q_num);
		break;
	default :
		printk("%s %d warning : q_select[%d], q_num[%d]\n", __FUNCTION__, __LINE__, q_select, q_num);
	break;
	}
#endif

	for (i=0; i<flush_num; i++)
		rtl_cache_sync_wback(priv, flush_addr[i], flush_len[i], PCI_DMA_TODEVICE);

	if (txcfg->aggre_en == FG_AGGRE_MSDU_FIRST) {
		priv->amsdu_first_desc = pfrstdesc;
#ifndef USE_RTL8186_SDK
		priv->amsdu_first_dma_desc = pfrst_dma_desc;
#endif
		priv->amsdu_len = get_desc(pfrstdesc->Dword0) & 0xffff; // get pktSize
		return;
	}

	pfrstdesc->Dword0 |= set_desc(TX_OWN);
#ifndef USE_RTL8186_SDK
	rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(pfrst_dma_desc-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(struct tx_desc), PCI_DMA_TODEVICE);
#endif

	if (q_num == HIGH_QUEUE) {
//		priv->pshare->pkt_in_hiQ = 1;
		priv->pkt_in_hiQ = 1;

		return;
	} else {
		tx_poll(priv, q_num);
	}

	return;
}
#else
void rtl8192cd_signin_txdesc(struct rtl8192cd_priv *priv, struct tx_insn* txcfg)
{
	return;
}
#endif//CONFIG_WLAN_NOT_HAL_EXIST

void rtl88XX_signin_txdesc(struct rtl8192cd_priv *priv, struct tx_insn* txcfg, unsigned char convHdr)
{
	struct tx_desc_info	*pswdescinfo, *pdescinfo;
	unsigned int 		fr_len, tx_len, i, keyid;
	u2Byte              *tx_head;
    u4Byte              q_num;
	unsigned char		*da, *pbuf, *pwlhdr, *pmic, *picv;
	struct rtl8192cd_hw	*phw;
#ifdef TX_SHORTCUT
	int					fit_shortcut=0, idx=0;
#endif


    PHCI_TX_DMA_MANAGER_88XX        ptx_dma;
    u32                             halQNum;
    PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q;
    PTX_BUFFER_DESCRIPTOR           cur_txbd;    
#ifdef WLAN_HAL_TX_AMSDU
    PHCI_TX_AMSDU_DMA_MANAGER_88XX  ptx_dma_amsdu;
    PTX_BUFFER_DESCRIPTOR_AMSDU     cur_txbd_amsdu;
#endif
    TX_DESC_DATA_88XX               desc_data;
    BOOLEAN use_hwtxsc = FALSE;

	unsigned int FA_total_cnt=0; // cliff test
	
#ifdef CONFIG_IEEE80211W
	unsigned int	isBIP = 0;
#endif
#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
    BOOLEAN isWiFiHdr = TRUE;
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV

	keyid=0;
	pmic=NULL;
	picv=NULL;

	if (txcfg->tx_rate == 0) {
		DEBUG_ERR("tx_rate=0!\n");
		txcfg->tx_rate = find_rate(priv, NULL, 0, 1);
	}

	q_num = txcfg->q_num;

	phw	= GET_HW(priv);
	
    halQNum     = GET_HAL_INTERFACE(priv)->MappingTxQueueHandler(priv, (u32)q_num);
    ptx_dma     = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PTxDMA88XX);
    cur_q       = &(ptx_dma->tx_queue[halQNum]);
    cur_txbd    = cur_q->pTXBD_head + cur_q->host_idx;
#if 0/*def WLAN_HAL_TX_AMSDU*/
    if(IS_SUPPORT_TX_AMSDU(priv)){
    	ptx_dma_amsdu  = (PHCI_TX_AMSDU_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PTxDMAAMSDU88XX);
    	cur_txbd_amsdu = ptx_dma_amsdu->tx_amsdu_queue[halQNum-1].pTXBD_head_amsdu + cur_q->host_idx;
    }
#endif
    memset(&desc_data, 0, sizeof(TX_DESC_DATA_88XX));

    tx_head     = &(cur_q->host_idx);

	pswdescinfo = get_txdesc_info(priv,priv->pshare->pdesc_info, q_num);

#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
	if (isWiFiHdr)
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
	{
		da = get_da((unsigned char *)txcfg->phdr);
	}

#ifdef CONFIG_IEEE80211W
	if(txcfg->isPMF && IS_MCAST(da)) 
	{
		isBIP = 1;
		txcfg->iv = 0;
		txcfg->fr_len += 10; // 10: MMIE length
	}
#endif

	tx_len = txcfg->fr_len;

	if (txcfg->fr_type == _SKB_FRAME_TYPE_)
		pbuf = ((struct sk_buff *)txcfg->pframe)->data;
	else
		pbuf = (unsigned char*)txcfg->pframe;

#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
	// this condition is 802.3 header
    if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(priv) && (convHdr==HW_TX_SC_HEADER_CONV)) {
        isWiFiHdr = FALSE;
    }
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV

	// TODO: hw tx shorcut, reuse txdesc only support AES...
#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
	if (isWiFiHdr)
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
 
	// in case of default key, then find the key id
	if (GetPrivacy((txcfg->phdr)))
	{
#ifdef WDS
		if (txcfg->wdsIdx >= 0) {
			if (txcfg->pstat)
			keyid = txcfg->pstat->keyid;
			else
				keyid = 0;
		}
		else
#endif

#ifdef __DRAYTEK_OS__
		if (!IEEE8021X_FUN)
			keyid = priv->pmib->dot1180211AuthEntry.dot11PrivacyKeyIndex;
		else {
			if (IS_MCAST(GetAddr1Ptr ((unsigned char *)txcfg->phdr)) || !txcfg->pstat)
				keyid = priv->pmib->dot11GroupKeysTable.keyid;
			else
				keyid = txcfg->pstat->keyid;
		}
#else

		if (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm==_WEP_40_PRIVACY_ ||
			priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm==_WEP_104_PRIVACY_) {
			if(IEEE8021X_FUN && txcfg->pstat) {
#ifdef A4_STA
				if (IS_MCAST(da) && !(txcfg->pstat->state & WIFI_A4_STA))
#else
				if(IS_MCAST(da))
#endif					
					keyid = 0;
				else
					keyid = txcfg->pstat->keyid;
			}
			else {
				keyid = priv->pmib->dot1180211AuthEntry.dot11PrivacyKeyIndex;
		    }
		}
#endif
	}


	for (i=0; i < txcfg->frg_num; i++)
	{
        pdescinfo = pswdescinfo + *tx_head;

#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
#ifdef AP_SWPS_OFFLOAD
        pdescinfo->isWiFiHdr = isWiFiHdr;
#endif
		// TODO: hw tx shortcut no support fragment ? Qos Control bit ??
		if (isWiFiHdr)
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
		if (i != 0)
		{
			memset(&desc_data, 0, sizeof(TX_DESC_DATA_88XX));
			
			// we have to allocate wlan_hdr
			pwlhdr = (UINT8 *)get_wlanhdr_from_poll(priv);
			if (pwlhdr == (UINT8 *)NULL)
			{
				DEBUG_ERR("System-bug... should have enough wlan_hdr\n");
				return;
			}
			// other MPDU will share the same seq with the first MPDU
			if (txcfg->hdr_len <= sizeof(struct wlan_hdr))
				memcpy((void *)pwlhdr, (void *)(txcfg->phdr), txcfg->hdr_len); // data pkt has 24 bytes wlan_hdr
			else {
				DEBUG_ERR("System-bug... should have enough wlan_hdr, hdr_len=%d\n", txcfg->hdr_len);
				return;
			}
		}
		else
		{
#ifdef WIFI_WMM
			if (txcfg->pstat /*&& (is_qos_data(txcfg->phdr))*/) {
				if ((GetFrameSubType(txcfg->phdr) & (WIFI_DATA_TYPE | BIT(6) | BIT(7))) == (WIFI_DATA_TYPE | BIT(7))) {
					unsigned char tempQosControl[2];
					memset(tempQosControl, 0, 2);
					tempQosControl[0] = ((struct sk_buff *)txcfg->pframe)->cb[1];
#ifdef WMM_APSD
					if (
#ifdef CLIENT_MODE
						(OPMODE & WIFI_AP_STATE) &&
#endif
						(APSD_ENABLE) && (txcfg->pstat) && (txcfg->pstat->state & WIFI_SLEEP_STATE) &&
						(!GetMData(txcfg->phdr)) &&
						((((tempQosControl[0] == 7) || (tempQosControl[0] == 6)) && (txcfg->pstat->apsd_bitmap & 0x01)) ||
						 (((tempQosControl[0] == 5) || (tempQosControl[0] == 4)) && (txcfg->pstat->apsd_bitmap & 0x02)) ||
						 (((tempQosControl[0] == 3) || (tempQosControl[0] == 0)) && (txcfg->pstat->apsd_bitmap & 0x08)) ||
						 (((tempQosControl[0] == 2) || (tempQosControl[0] == 1)) && (txcfg->pstat->apsd_bitmap & 0x04))))
						tempQosControl[0] |= BIT(4);
#endif
					if (txcfg->aggre_en == FG_AGGRE_MSDU_FIRST)
						tempQosControl[0] |= BIT(7);

					if (priv->pmib->dot11nConfigEntry.dot11nTxNoAck)
						tempQosControl[0] |= BIT(5);

					memcpy((void *)GetQosControl((txcfg->phdr)), tempQosControl, 2);
				}
			}
#endif

#if (BEAMFORMING_SUPPORT == 1)
			if(!txcfg->ndpa)
#endif	
			assign_wlanseq(GET_HW(priv), txcfg->phdr, txcfg->pstat, GET_MIB(priv)
#ifdef CONFIG_RTK_MESH	// For broadcast data frame via mesh (ex:ARP requst)
				, txcfg->is_11s
#endif
				);
			pwlhdr = txcfg->phdr;
		}
#if (BEAMFORMING_SUPPORT == 1)
		if(!txcfg->ndpa)
#endif	
        {
#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
 	       	if (isWiFiHdr)
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
            {
		SetDuration(pwlhdr, 0);
            }
		}

#ifdef CLIENT_MODE
		if (OPMODE & WIFI_STATION_STATE) {
			if (GetFrameSubType(txcfg->phdr) == WIFI_PSPOLL)
        desc_data.navUseHdr = _TRUE;

			if (priv->ps_state)
				SetPwrMgt(pwlhdr);
			else
				ClearPwrMgt(pwlhdr);
		}
#endif
#if (BEAMFORMING_SUPPORT == 1)
		if(txcfg->ndpa)
			desc_data.navUseHdr = _TRUE;
#endif

if((priv->pshare->rf_ft_var.txforce != 0xff || (txcfg->pstat && txcfg->pstat->force_rate))
            #if (BEAMFORMING_SUPPORT == 1)
            && (!txcfg->ndpa) 
            #endif
        ){
        	if(txcfg->pstat && txcfg->pstat->force_rate)
				desc_data.dataRate  = txcfg->pstat->force_rate;
			else
	            desc_data.dataRate  = priv->pshare->rf_ft_var.txforce;
            desc_data.disDataFB = _TRUE;
            desc_data.disRTSFB  = _TRUE;
            desc_data.useRate   = _TRUE;
        }
			

#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
		if (isWiFiHdr)
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
		if (i != (txcfg->frg_num - 1))
		{
			SetMFrag(pwlhdr);
			if (i == 0) {
				fr_len = (txcfg->frag_thrshld - txcfg->llc);
				tx_len -= (txcfg->frag_thrshld - txcfg->llc);
			}
			else {
				fr_len = txcfg->frag_thrshld;
				tx_len -= txcfg->frag_thrshld;
			}
		}
		else	// last seg, or the only seg (frg_num == 1)
		{
			fr_len = tx_len;
			ClearMFrag(pwlhdr);
		}

		desc_data.frLen  = fr_len;

#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))
		if(txcfg->pstat)
		{
			odm_set_tx_ant_by_tx_info_hal(priv, (PVOID)&desc_data, txcfg->pstat->cmn_info.aid);   //HAL Set TX antenna	
		}
#endif	//#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))
		// TODO: how to fill some field in rtl88XX_fill_fwinfo
		rtl88XX_fill_fwinfo(priv, txcfg, i, &desc_data);
        
#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
        if (isWiFiHdr)
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
        {
		SetFragNum((pwlhdr), i);
        }

		if (((i == 0) && (txcfg->fr_type == _SKB_FRAME_TYPE_))
#if (BEAMFORMING_SUPPORT == 1)
			|| txcfg->ndpa
#endif
		) {
			pdescinfo->type = _PRE_ALLOCLLCHDR_;
		}
		else {
			pdescinfo->type = _PRE_ALLOCHDR_;
		}

		if(txcfg->fr_type == _SKB_FRAME_TYPE_) {
	       desc_data.tid = ((struct sk_buff *)txcfg->pframe)->cb[1];
#ifdef WMM_DSCP_C42
			{
				unsigned int 	   q_num;
				q_num = pri_to_qnum(priv, desc_data.tid);

				if(q_num ^ txcfg->q_num)
					desc_data.tid = 0x04;
			}
#elif defined(CONFIG_WLAN_HAL_8197F)
			{
				unsigned int	   q_num;
				q_num = pri_to_qnum(priv, desc_data.tid);
			}
#endif
		}

#ifdef CONFIG_RTL_OFFLOAD_DRIVER
		if (OFFLOAD_ENABLE(priv)) {
			if (GetFrameType(pwlhdr) == WIFI_DATA) {
				txcfg->h2d_type = TXPKTINFO_TYPE_80211;
                if (txcfg->q_num >= HIGH_QUEUE_V2 && txcfg->q_num <= HIGH_QUEUE19_V1)
                    txcfg->h2d_tid = TXDESC_DMA_CHANNEL15;
                else
                    txcfg->h2d_tid = ((struct sk_buff *)txcfg->pframe)->cb[1];
			}
			else
				txcfg->h2d_type = TXPKTINFO_TYPE_80211_PROTO;
						
			rtl88XX_fill_txPktInfo(priv, txcfg, txcfg->h2d_type);
		}
#endif

        if (i != (txcfg->frg_num - 1)) {
            desc_data.frag = _TRUE;
        }

		if (txcfg->pstat) {
			if (txcfg->pstat->cmn_info.aid != MANAGEMENT_AID) {
                desc_data.rateId = txcfg->pstat->cmn_info.ra_info.rate_id;
                desc_data.macId = REMAP_AID(txcfg->pstat);
			}
#if (BEAMFORMING_SUPPORT == 1)
			if((priv->pmib->dot11RFEntry.txbf == 1) &&
				((txcfg->pstat->ht_cap_len && (txcfg->pstat->ht_cap_buf.txbf_cap)) 
#ifdef RTK_AC_SUPPORT
				||(txcfg->pstat->vht_cap_len && (cpu_to_le32(txcfg->pstat->vht_cap_buf.vht_cap_info) & BIT(SU_BFEE_S)))
#endif
			)){
				desc_data.p_aid = txcfg->pstat->cmn_info.bf_info.p_aid;
				desc_data.g_id  = txcfg->pstat->cmn_info.bf_info.g_id;
			}
#if (MU_BEAMFORMING_SUPPORT == 1)
			if (txcfg->pstat->muPartner_num) {
				desc_data.p_aid = txcfg->pstat->cmn_info.bf_info.p_aid;
			}
#endif
#endif
		} else {
#if (defined(MBSSID) || defined(UNIVERSAL_REPEATER)) && defined(HW_ENC_FOR_GROUP_CIPHER)
			if ((txcfg->fr_type == _SKB_FRAME_TYPE_) && IS_MCAST(GetAddr1Ptr ((unsigned char *)txcfg->phdr)) && 
				((txcfg->privacy == _TKIP_PRIVACY_) || (txcfg->privacy == _CCMP_PRIVACY_))) {
				desc_data.enDescId = 1;
				desc_data.macId = (priv->vap_init_seq+HW_ENC_GROUP_CIPHER_OFFSET);
			}
#endif
            desc_data.rateId = ARFR_BMC;
			
        }

#if 1 /* eric-8822 ?? assign gid of txdesc = 0x3f */
#if (BEAMFORMING_SUPPORT == 1)
		if((GET_CHIP_VER(priv)==VERSION_8822B || GET_CHIP_VER(priv)==VERSION_8814B || GET_CHIP_VER(priv)==VERSION_8812F ) && txcfg->ndpa){
			if(txcfg->pstat){
				Beamforming_GidPAid(priv, txcfg->pstat);
				desc_data.p_aid = txcfg->pstat->cmn_info.bf_info.p_aid;
			}
		}
#endif

		desc_data.g_id = 0x3f;
#endif	

        if (GET_CHIP_VER(priv) >= VERSION_8814A && GET_CHIP_VER(priv) != VERSION_8192F) {
            if(priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G) {
                desc_data.dataRateFBLmt = 4;
            } else {
                desc_data.dataRateFBLmt = 0;
            }
        } else
#if defined(MULTI_STA_REFINE)
        if((priv->pmib->dot11RFEntry.phyBandSelect & PHY_BAND_2G) && (priv->pshare->total_assoc_num > 10) 
            && (txcfg->pstat) && ((txcfg->pstat->tx_ra_bitmap & 0xff00000) == 0)) {
            desc_data.dataRateFBLmt = 3;
        } else
#endif
        {
            desc_data.dataRateFBLmt = 0x1F;
        }

        if (txcfg->fixed_rate) {
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

#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
		if (isWiFiHdr)
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
		if (txcfg->privacy) {
            desc_data.secType = txcfg->privacy;
			if ( txcfg->use_sw_enc ) {				
                desc_data.swCrypt = TRUE;
                desc_data.icv = txcfg->icv;
                desc_data.mic = txcfg->mic;
                desc_data.iv  = txcfg->iv;
			} else {
				// hw encrypt
				desc_data.swCrypt = FALSE;
				switch(txcfg->privacy) {
				case _WEP_104_PRIVACY_:
				case _WEP_40_PRIVACY_:
                    desc_data.icv = 0;
                    desc_data.mic = 0;
                    desc_data.iv  = txcfg->iv;
					wep_fill_iv(priv, pwlhdr, txcfg->hdr_len, keyid);
					break;

				case _TKIP_PRIVACY_:
                    desc_data.icv = 0;
                    desc_data.mic = txcfg->mic;
                    desc_data.iv  = txcfg->iv;
					tkip_fill_encheader(priv, pwlhdr, txcfg->hdr_len, keyid);
					break;

				#if defined(CONFIG_RTL_HW_WAPI_SUPPORT)
				case _WAPI_SMS4_:
                    desc_data.icv = 0;
                    desc_data.mic = 0;
                    desc_data.iv  = txcfg->iv;
					break;
				#endif
                
				case _CCMP_PRIVACY_:
					//michal also hardware...
                    desc_data.icv = 0;
                    desc_data.mic = 0;
                    desc_data.iv  = txcfg->iv;
					aes_fill_encheader(priv, pwlhdr, txcfg->hdr_len, keyid);
					break;

				default:
					DEBUG_ERR("Unknow privacy\n");
					break;
				}
			}
		}

#ifdef WLAN_HAL_HW_AES_IV
        // TODO: add this mechanism in rtl88XX_signin_txdesc_shortcut !!!
        if ((txcfg->privacy == _CCMP_PRIVACY_) && 
            (
#ifdef CONFIG_IEEE80211W
            (UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE), txcfg->isPMF)) 
#else
            (UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE)))
#endif
                == FALSE)) {
            desc_data.secType = txcfg->privacy;
            desc_data.swCrypt = FALSE;
            desc_data.icv     = 0;
            desc_data.mic     = 0;
            desc_data.iv      = 0;
            desc_data.hwAESIv = TRUE;
			desc_data.iv = txcfg->iv;

            #if 0
            // temp mark by Eric, no need check isWifiHdr
            if (isWiFiHdr == TRUE) {
                desc_data.iv = txcfg->iv;
                // when desc_data.hwAESIv == TRUE, hw auto fill AES iv
                // aes_fill_encheader(priv, pwlhdr, txcfg->hdr_len, keyid);
            }
            #endif 
        }
#endif // WLAN_HAL_HW_AES_IV

		// below is for sw desc info
		// TODO: hw tx shortcut recycle....sw desc.
#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
		if (isWiFiHdr == FALSE) {
			pdescinfo->pframe = NULL;
		} else
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
		{
		pdescinfo->pframe = pwlhdr;
		}

#if defined(WIFI_WMM) && defined(WMM_APSD)
#if defined(ENABLE_RTL_SKB_STATS) || defined(RESERVE_TXDESC_FOR_EACH_IF)
		pdescinfo->priv = priv;
#endif
#ifndef TXDESC_INFO
		pdescinfo->pstat = txcfg->pstat;
#endif		
#endif

#ifdef TX_SHORTCUT
#ifdef CONFIG_WLAN_HAL_8814BE
		if (GET_CHIP_VER(priv) == VERSION_8814B)
			;
		else 
#endif
		if (!priv->pmib->dot11OperationEntry.disable_txsc && txcfg->pstat &&
				(txcfg->fr_type == _SKB_FRAME_TYPE_) &&
				(txcfg->frg_num == 1) &&
				((txcfg->privacy == 0)
#ifdef CONFIG_RTL_WAPI_SUPPORT
				|| (txcfg->privacy == _WAPI_SMS4_)
#endif
				|| (!txcfg->use_sw_enc)
				) &&
				(
#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
				(isWiFiHdr == FALSE) ? TRUE : 
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
				(!GetMData(txcfg->phdr))
				) &&
#ifdef SUPPORT_TX_AMSDU_SHORTCUT				
				txcfg->aggre_en <= FG_AGGRE_MSDU_FIRST
#else
				txcfg->aggre_en < FG_AGGRE_MSDU_FIRST
#endif
				) {
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
				struct wlan_ethhdr_t ethhdr;
#endif
#ifdef CONFIG_RTK_MESH
			if(txcfg->is_11s) {
				idx = get_tx_sc_free_entry(priv, txcfg->pstat, &priv->ethhdr, txcfg->aggre_en);
			} else
#endif
			{
#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
				if (isWiFiHdr == FALSE) {
					idx = get_tx_sc_free_entry(priv, txcfg->pstat, pbuf, txcfg->aggre_en);
				} else
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
                {
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
        			if(txcfg->aggre_en == FG_AGGRE_MSDU_FIRST){
						memcpy((void *)&ethhdr, pbuf, 12);
						memcpy((void *)&(ethhdr.type), pbuf+20, 2);
						idx = get_tx_sc_free_entry(priv, txcfg->pstat, (unsigned char *)&ethhdr, txcfg->aggre_en);
        			}
					else
#endif
					idx = get_tx_sc_free_entry(priv, txcfg->pstat, pbuf - sizeof(struct wlan_ethhdr_t), txcfg->aggre_en);
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
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
//					txcfg->pstat->tx_sc_ent[idx].isamsdu = (txcfg->aggre_en == FG_AGGRE_MSDU_FIRST);
        			if(txcfg->aggre_en == FG_AGGRE_MSDU_FIRST){
						memcpy((void *)&txcfg->pstat->tx_sc_ent[idx].amsdullcsnaphdr, pbuf, sizeof(struct wlan_amsdullcsnaphdr_t));
						memcpy((void *)&txcfg->pstat->tx_sc_ent[idx].ethhdr, &ethhdr, sizeof(struct wlan_ethhdr_t));
        			}
					else
#endif
					memcpy((void *)&txcfg->pstat->tx_sc_ent[idx].ethhdr, pbuf - sizeof(struct wlan_ethhdr_t), sizeof(struct wlan_ethhdr_t));
				}
			}

			txcfg->pstat->protection = priv->pmib->dot11ErpInfo.protection;
			txcfg->pstat->ht_protection = priv->ht_protection;
			txcfg->pstat->tx_sc_ent[idx].sc_keyid = keyid;
			txcfg->pstat->tx_sc_ent[idx].pktpri = ((struct sk_buff *)txcfg->pframe)->cb[1];
			fit_shortcut = 1;

#if defined(WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC)
            if(IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC(priv))   {
                desc_data.txwifiCp = TRUE;
                txcfg->pstat->tx_sc_hw_idx = idx;
            }
#elif defined(SUPPORT_TXDESC_IE)
            if(IS_SUPPORT_TXDESC_IE(priv))   {
                txcfg->pstat->tx_sc_hw_idx = idx;
            }
#endif // WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC
#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
            if(IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(priv))   {
				if (convHdr == HW_TX_SC_BACKUP_HEADER) {
					desc_data.macCp = TRUE;		// backup 802.11 header info.
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
					desc_data.smhEn = TRUE;		// auto conv hdr  (802.3 -> 802.11)	
			    #ifdef SUPPORT_TX_HW_AMSDU
                    desc_data.HW_AMSDU = TRUE;
                #endif
					// TODO: consider AES for HW_TX_SHORTCUT_HDR_CONV
					// Eth hdr 14 bytes => gen llc 8 bytes
					//                              gen iv 8 bytes
					// skb: ethHdr + data
					// ethHdr + (iv + llc)  + data
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
		}
		else {
			if (txcfg->pstat) {
				for (idx=0; idx<MAX_TXSC_ENTRY; idx++) {
                    GET_HAL_INTERFACE(priv)->SetShortCutTxBuffSizeHandler(priv, txcfg->pstat->tx_sc_ent[idx].hal_hw_desc, 0);
				}
			}
		}
#endif

#ifndef TXDESC_INFO			
		if (txcfg->privacy)
		{
			if (txcfg->privacy == _WAPI_SMS4_)
			{
				pdescinfo->pstat = txcfg->pstat;
				pdescinfo->rate = txcfg->tx_rate;
			}
            else if (!txcfg->use_sw_enc)
			{
				pdescinfo->pstat = txcfg->pstat;
				pdescinfo->rate = txcfg->tx_rate;
			}
            // TODO: why swCrypto doesn't set these two swDesc ?
		}
		else
		{
			pdescinfo->pstat = txcfg->pstat;
			pdescinfo->rate = txcfg->tx_rate;
		}
#endif    
	if (0 == txcfg->fr_len)
            goto swenc_end;
	
	#ifdef CONFIG_RTL_WAPI_SUPPORT
	//panic_printk("%s:%d privacy=%d\n", __FUNCTION__, __LINE__,txcfg->privacy);
	if (txcfg->privacy == _WAPI_SMS4_)
	{
		//panic_printk("%s:%d\n", __FUNCTION__, __LINE__);
		SecSWSMS4Encryption(priv, txcfg);
		desc_data.pMic = ((struct sk_buff *)txcfg->pframe)->data+txcfg->fr_len;
	} 
	#endif    
        /*** start sw encryption ***/
		if (txcfg->privacy && txcfg->use_sw_enc)
		{
            if (txcfg->privacy == _TKIP_PRIVACY_ ||
                txcfg->privacy == _WEP_40_PRIVACY_ ||
                txcfg->privacy == _WEP_104_PRIVACY_) {
                
                //picvdescinfo = pswdescinfo + *tx_head;
        
                // append ICV first...
                picv = get_icv_from_poll(priv);
                if (picv == NULL) {
                    DEBUG_ERR("System-Buf! can't alloc picv\n");
                    //BUG();
                }
                
                pdescinfo->buf_type[1]   = _PRE_ALLOCICVHDR_;
                pdescinfo->buf_pframe[1] = picv;
#if defined(CONFIG_NET_PCI)
                pdescinfo->buf_len[1] = txcfg->icv;
#endif
                desc_data.pIcv = picv;
                
                if (i == 0) {
                    tkip_icv(picv,
                          pwlhdr + txcfg->hdr_len + txcfg->iv, txcfg->llc,
                          pbuf,                                txcfg->fr_len);
                } else {                
                    tkip_icv(picv,
                        NULL, 0,
                        pbuf, txcfg->fr_len);
                }
        
                if ((i == 0) && (txcfg->llc != 0)) {
                    if (txcfg->privacy == _TKIP_PRIVACY_) {
                        tkip_encrypt(priv, pwlhdr, txcfg->hdr_len,
                            pwlhdr + txcfg->hdr_len + 8, sizeof(struct llc_snap),
                            pbuf,                        txcfg->fr_len, 
                            picv,                        txcfg->icv);
                    } else {
                        wep_encrypt(priv, pwlhdr, txcfg->hdr_len,
                            pwlhdr + txcfg->hdr_len + 4, sizeof(struct llc_snap),
                            pbuf,                        txcfg->fr_len, 
                            picv,                        txcfg->icv,
                            txcfg->privacy);
                    }
                } else { // not first segment or no snap header
                    if (txcfg->privacy == _TKIP_PRIVACY_) {
                        tkip_encrypt(priv, pwlhdr, txcfg->hdr_len, 
                            NULL, 0,
                            pbuf, txcfg->fr_len, 
                            picv, txcfg->icv);
                    } else {
                        wep_encrypt(priv, pwlhdr, txcfg->hdr_len, 
                            NULL, 0,
                            pbuf, txcfg->fr_len,
                            picv, txcfg->icv,
                            txcfg->privacy);
                    }
                }
                        
            } else if (txcfg->privacy == _CCMP_PRIVACY_) {
                //pmicdescinfo = pswdescinfo + *tx_head;

                // append MIC first...
                pmic = get_mic_from_poll(priv);
                if (pmic == NULL) {
                    DEBUG_ERR("System-Buf! can't alloc pmic\n");
                    //BUG();
                }

                pdescinfo->buf_type[1]   = _PRE_ALLOCMICHDR_;
                pdescinfo->buf_pframe[1] = pmic;
#if defined(CONFIG_NET_PCI)
                pdescinfo->buf_len[1] = txcfg->mic;
#endif
                desc_data.pMic = pmic;
                
                // then encrypt all (including ICV) by AES
                if ((i == 0)&&(txcfg->llc != 0)) { // encrypt 3 segments ==> llc, mpdu, and mic
#ifdef CONFIG_IEEE80211W
					if(isBIP) {
						BIP_encrypt(priv, pwlhdr, 
									pwlhdr + txcfg->hdr_len + 8,
									pbuf, txcfg->fr_len, 
									pmic,txcfg->isPMF);
					} 
					else {
						aesccmp_encrypt(priv, pwlhdr, 
										pwlhdr + txcfg->hdr_len + 8,
										pbuf, txcfg->fr_len, 
										pmic,txcfg->isPMF);
					}
#else
						aesccmp_encrypt(priv, pwlhdr, 
										pwlhdr + txcfg->hdr_len + 8,
										pbuf, txcfg->fr_len, 
										pmic);
#endif            
                } else { // encrypt 2 segments ==> mpdu and mic
#ifdef CONFIG_IEEE80211W
					if(isBIP) {
						BIP_encrypt(priv, pwlhdr, 
									NULL,
									pbuf, txcfg->fr_len, 
									pmic,txcfg->isPMF);
					} 
					else {
						aesccmp_encrypt(priv, pwlhdr, 
										NULL,
										pbuf, txcfg->fr_len, 
										pmic,txcfg->isPMF);
					}
#else
					aesccmp_encrypt(priv, pwlhdr, 
									NULL,
									pbuf, txcfg->fr_len, 
									pmic);
#endif                    
               }       
            }
        }
        /*** end sw encryption ***/
swenc_end:
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
		if (OFFLOAD_ENABLE(priv)) {
			desc_data.pHdr = pwlhdr - sizeof(struct h2d_txpkt_info);
			desc_data.hdrLen = txcfg->hdr_len + sizeof(struct h2d_txpkt_info);
		}else
#endif
		{
        desc_data.pHdr   = pwlhdr;
        desc_data.hdrLen = txcfg->hdr_len;
		}
        desc_data.llcLen = (i==0 ? txcfg->llc : 0);
        
        if (fr_len != 0) {
            desc_data.pBuf = pbuf;
        }

#if defined(TAROKO_0)
    //do nothing
#else
        switch (halQNum) {
        case HCI_TX_DMA_QUEUE_HI0:
#if CFG_HAL_SUPPORT_MBSSID
        case HCI_TX_DMA_QUEUE_HI1:
        case HCI_TX_DMA_QUEUE_HI2:
        case HCI_TX_DMA_QUEUE_HI3:
        case HCI_TX_DMA_QUEUE_HI4:
        case HCI_TX_DMA_QUEUE_HI5:
        case HCI_TX_DMA_QUEUE_HI6:
        case HCI_TX_DMA_QUEUE_HI7:
#if IS_RTL8198F_SERIES
        case HCI_TX_DMA_QUEUE_HI8:
        case HCI_TX_DMA_QUEUE_HI9:
        case HCI_TX_DMA_QUEUE_HI10:
        case HCI_TX_DMA_QUEUE_HI11:
        case HCI_TX_DMA_QUEUE_HI12:
        case HCI_TX_DMA_QUEUE_HI13:
        case HCI_TX_DMA_QUEUE_HI14:
        case HCI_TX_DMA_QUEUE_HI15:   
#endif  //#if IS_RTL8198F_SERIES
#endif //#if CFG_HAL_SUPPORT_MBSSID
            desc_data.moreData = 1;
            break;
        default:
            break;
        }
#endif

#if 0//#ifdef CONFIG_8814_AP_MAC_VERI //old
    if (txcfg->fr_len != 0)	//for mgt frame
    {
        if((desc_data.pBuf[0]==0x55) && (desc_data.pBuf[1]==0x55))
        {
            desc_data.macId = priv->macID_temp;
            desc_data.disDataFB = _FALSE;
            desc_data.disRTSFB  = _FALSE;
            desc_data.useRate= _TRUE;  

            desc_data.dataRate = 0x13; // HT MCS6
            desc_data.RTSRate = 0x13;
            desc_data.rateId = 5;

            if(priv->lowestRate_TXDESCen)
            {
                desc_data.moreData = 1;                
                desc_data.dataRateFBLmt = priv->lowestRate;
                desc_data.RTSRateFBLmt = priv->lowestRate;                
                desc_data.rtyLmtEn = 1;                                         
                desc_data.dataRtyLmt = 16;    
                desc_data.RTSEn = true;
            }
            else
            {
                desc_data.rtyLmtEn = 0;
            }

            if(priv->ProtectionType == 0){
                desc_data.HWRTSEn = 0;
                desc_data.RTSEn = 0;                
                desc_data.CTS2Self = 0;                
            }else if(priv->ProtectionType == 1) {
                desc_data.HWRTSEn = 0;
                desc_data.RTSEn = 1;                              
                desc_data.CTS2Self = 0;       
                desc_data.rtyLmtEn = 1;         
                desc_data.dataRtyLmt = 16;                 
            }else if(priv->ProtectionType == 2) {
                desc_data.HWRTSEn = 0;
                desc_data.RTSEn = 0;                
                desc_data.CTS2Self = 1;                          
            }

            desc_data.BMCRtyLmt = priv->BMC_RTY_LMT;

#ifdef VERIFY_AP_FAST_EDCA
			desc_data.aggEn = 0;	// no agg.
			desc_data.dataRate = 4;	// OFDM rate
			desc_data.RTSEn = FALSE;
#endif
        }
    }
#endif 


#ifdef CONFIG_8814_AP_MAC_VERI
    if (txcfg->fr_len != 0)	//for mgt frame
    {
        desc_data.dataRate = txcfg->tx_rate; // HT MCS6
#ifdef AP_SWPS_OFFLOAD_WITH_AMPDU_VERI        
        if(txcfg->test_pkt==1) //to avoid other packet enter , only allow test packet
#endif            
        if((desc_data.pBuf[0]==0x55) && (desc_data.pBuf[1]==0x55))
        {
            #ifdef AP_SWPS_OFFLOAD
            if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv))
                pdescinfo->test_pkt = 1;
            #endif        
            desc_data.macId = priv->macID_temp;
            //desc_data.disDataFB = _FALSE;
            //desc_data.disRTSFB  = _FALSE;
            desc_data.useRate= _TRUE;  

            desc_data.dataRate = txcfg->tx_rate; // HT MCS6
            desc_data.hdrLen = WLAN_HDR_A3_QOS_LEN;
            desc_data.llcLen = 0;
            //pdescinfo->type = _PRE_ALLOCMEM_;
            //pdescinfo->pframe = txcfg->pframe; // skb


            //pdescinfo->type = _PRE_ALLOCHDR_;
            //pdescinfo->pframe = txcfg->phdr; // skb            


            //pdescinfo->buf_type[1]   = _PRE_ALLOCMEM_;
            //pdescinfo->buf_pframe[1] = txcfg->pframe;
            //pdescinfo->buf_type[1]   = _PRE_ALLOCHDR_;
            //pdescinfo->buf_pframe[1] = txcfg->phdr;

            //desc_data.RTSRate = 0x1;
            //desc_data.rateId = 0;
            if(priv->lowestRate_TXDESCen)
            {
                desc_data.moreData = 1;                
                desc_data.dataRateFBLmt = priv->lowestRate;
                desc_data.RTSRateFBLmt = priv->lowestRate;                
                desc_data.rtyLmtEn = 1;                                         
                desc_data.dataRtyLmt = 16;    
                desc_data.RTSEn = true;
            }
            else
            {
                desc_data.rtyLmtEn = 0;
                desc_data.dataRtyLmt = 16;    
            }

            if(priv->ProtectionType == 0){
                desc_data.HWRTSEn = 0;
                desc_data.RTSEn = 0;                
                desc_data.CTS2Self = 0;                
            }else if(priv->ProtectionType == 1) {
                desc_data.HWRTSEn = 0;
                desc_data.RTSEn = 1;                              
                desc_data.CTS2Self = 0;       
                desc_data.rtyLmtEn = 1;         
                desc_data.dataRtyLmt = 16;                 
            }else if(priv->ProtectionType == 2) {
                desc_data.HWRTSEn = 0;
                desc_data.RTSEn = 0;                
                desc_data.CTS2Self = 1;                          
            }

            //desc_data.BMCRtyLmt = priv->BMC_RTY_LMT;

#ifdef AP_SWPS_OFFLOAD_WITH_AMPDU_VERI
            desc_data.aggEn = 1; //need to agg
            desc_data.rtyLmtEn = 1;
            desc_data.dataRtyLmt = 0x3F;
#endif
#ifdef VERIFY_AP_FAST_EDCA
			desc_data.aggEn = 0;	// no agg.
			desc_data.dataRate = 4;	// OFDM rate
			desc_data.RTSEn = FALSE;
#endif
        }
    }
#endif 


#if 0
#if defined(TAROKO_0)
//TODO, can't reference ODMPTR->false_alm_cnt.cnt_all
#else
    /* cliff test */
	if ( 
#ifdef MP_TEST
		!(OPMODE & WIFI_MP_STATE) && 
#endif
		priv->assoc_num > 4 ) 
	{
#ifdef USE_OUT_SRC
#ifdef _OUTSRC_COEXIST
       if(IS_OUTSRC_CHIP(priv))
#endif
       {
    	   FA_total_cnt = ODMPTR->false_alm_cnt.cnt_all;
       }
#endif

#if !defined(USE_OUT_SRC) || defined(_OUTSRC_COEXIST)
#ifdef _OUTSRC_COEXIST
       if(!IS_OUTSRC_CHIP(priv))
#endif
       {
      	   FA_total_cnt = priv->pshare->FA_total_cnt;
       }
#endif	
 
       if ( FA_total_cnt < 1000 )  {
           desc_data.HWRTSEn = 0;
           desc_data.RTSEn = 0;                
           desc_data.CTS2Self = 0;		
	   }
    }

#endif
#endif

#ifdef WLAN_HAL_TX_AMSDU
    desc_data.aggreEn = txcfg->aggre_en;
    if (txcfg->aggre_en == FG_AGGRE_MSDU_FIRST) {
        priv->amsdu_len = desc_data.hdrLen + desc_data.llcLen + desc_data.frLen;

#ifdef CONFIG_IEEE80211W
        if (txcfg->privacy && (UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE), txcfg->isPMF)==FALSE))
#else
        if (txcfg->privacy && (UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE))==FALSE))
#endif
        {
            priv->amsdu_len += (desc_data.icv + desc_data.mic + desc_data.iv);
        }
    }
#endif

#if defined(AP_SWPS_OFFLOAD)
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv)) {

        pdescinfo->swps_pstat = txcfg->pstat; //for recycle and reprepare pkt use
        if(txcfg->pstat == NULL){
            printk("[%s][%d]pstat is NULL\n",__FUNCTION__,__LINE__);
            desc_data.SWPS_sequence = 0;
            pdescinfo->SWPS_sequence = 0;
            //pdescinfo->SWPS_pktQ = 0; 
            pdescinfo->SWPS_pkt_Qos = -2; //no pstat, pkt is multicast/broadcast
        }
        else{
            if(txcfg->pstat->EnSWPS){

            int pkt_Qos = halQNum-1;//txcfg->pstat->SWPS_pkt_Qos;
            
            if((pkt_Qos > 3) || (pkt_Qos < 0))
            {
                //not 4 ac packet, no need to reprepare, so no need to record related info
                pdescinfo->SWPS_pkt_Qos = -2; //direct reycle
            }
            else
            {

                desc_data.SWPS_sequence = txcfg->pstat->SWPS_seq[pkt_Qos];
                pdescinfo->SWPS_sequence = txcfg->pstat->SWPS_seq[pkt_Qos];
                //pdescinfo->SWPS_pktQ = halQNum;
                pdescinfo->SWPS_pkt_Qos = pkt_Qos;
                #ifndef SMP_SYNC
                	//unsigned long flags;
                #endif

            	//SAVE_INT_AND_CLI(flags); //may not need
                txcfg->pstat->SWPS_seq_head[pkt_Qos] = pdescinfo->SWPS_sequence;

                    
                if(txcfg->pstat->SWPS_seq[pkt_Qos] == 0xFFF)
                    txcfg->pstat->SWPS_seq[pkt_Qos] = 1;
                else        
                    txcfg->pstat->SWPS_seq[pkt_Qos]++;
                //RESTORE_INT(flags);

            }
        }
        else{
            desc_data.SWPS_sequence = 0;
            pdescinfo->SWPS_sequence = 0;
            //pdescinfo->SWPS_pktQ = 0; 
            pdescinfo->SWPS_pkt_Qos = 0; // no use, just to prevent get no qos situation.
        }
            
        desc_data.DropID = txcfg->pstat->Drop_ID;
        pdescinfo->Drop_ID = txcfg->pstat->Drop_ID;
        printk("[%s]macid =%x ,aid = %x,tx pkt swps seq=%x, drop id=%x\n",__FUNCTION__, txcfg->pstat->cmn_info.mac_id,txcfg->pstat->cmn_info.aid,pdescinfo->SWPS_sequence,pdescinfo->Drop_ID);
        }
    }
        
#endif

#ifdef TX_SHORTCUT
        if (fit_shortcut) {
#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
			if (isWiFiHdr == FALSE) {
				desc_data.pHdr	 = txcfg->phdr;
				desc_data.hdrLen = txcfg->fr_len;
				desc_data.llcLen = 0;
				desc_data.frLen  = 0;
			}
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
            GET_HAL_INTERFACE(priv)->FillShortCutTxHwCtrlHandler(
                priv, halQNum, (void *)&desc_data, txcfg->pstat->tx_sc_ent[idx].hal_hw_desc, 0x01, use_hwtxsc);
#ifdef TXSC_CACHE_FRLEN					
			txcfg->pstat->tx_sc_ent[idx].frlen = fr_len;
#endif
        } else
#endif
        {
            GET_HAL_INTERFACE(priv)->FillTxHwCtrlHandler(priv, halQNum, (void *)&desc_data);
        }

        if (txcfg->fr_len != 0) {
            if (i == 0) {
                pdescinfo->buf_type[0] = txcfg->fr_type;
            } else {
                pdescinfo->buf_type[0] = _RESERVED_FRAME_TYPE_;
            }

            pdescinfo->buf_pframe[0]   = txcfg->pframe;
#if defined(CONFIG_NET_PCI)
            pdescinfo->buf_len[0]      = txcfg->fr_len;
#endif
#ifdef WLAN_HAL_TX_AMSDU
            pdescinfo->amsdu_num = 0;
            if ((txcfg->aggre_en == FG_AGGRE_MSDU_FIRST) && (halQNum-1 < HCI_TX_AMSDU_DMA_QUEUE_MAX_NUM)) {
                ptx_dma_amsdu  = (PHCI_TX_AMSDU_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PTxDMAAMSDU88XX);
                cur_txbd_amsdu = ptx_dma_amsdu->tx_amsdu_queue[halQNum-1].pTXBD_head_amsdu + cur_q->host_idx;

				pdescinfo->buf_type_amsdu[0]    = txcfg->fr_type;
                pdescinfo->buf_pframe_amsdu[0]  = txcfg->pframe;
#if defined(CONFIG_NET_PCI) && !defined(USE_RTL8186_SDK)	
				pdescinfo->buf_paddr_amsdu[0]=get_desc(cur_txbd_amsdu->TXBD_ELE[0].Dword1);
				pdescinfo->buf_len_amsdu[0]=get_desc(cur_txbd_amsdu->TXBD_ELE[0].Dword0) & 0xffff;
#endif                
                pdescinfo->buf_type[0] = _RESERVED_FRAME_TYPE_;
                pdescinfo->buf_pframe[0] = NULL;
                pdescinfo->amsdu_num = 1;
            } else
#endif // WLAN_HAL_TX_AMSDU
            {
#ifndef TXDESC_INFO
                pdescinfo->buf_paddr[0] = get_desc(cur_txbd->TXBD_ELE[2].Dword1);//payload
#endif
            }
#if defined(CONFIG_NET_PCI)
            if (desc_data.pIcv || desc_data.pMic)
                pdescinfo->buf_paddr[1] = get_desc(cur_txbd->TXBD_ELE[3].Dword1);// icv/mic
#endif                
        }

#ifndef TXDESC_INFO        
            pdescinfo->paddr = get_desc(cur_txbd->TXBD_ELE[1].Dword1);//header address
#endif
#if defined(CONFIG_NET_PCI)
        pdescinfo->len = get_desc(cur_txbd->TXBD_ELE[1].Dword0) & 0xffff; // header len
#endif


#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
		// TODO: temp for swdesc...should check it..
		if (isWiFiHdr == FALSE) {
			pdescinfo->type				= _SKB_FRAME_TYPE_;	// 802.3 header + payload
			pdescinfo->pframe 			= txcfg->pframe; // skb

			pdescinfo->buf_type[0]	 	= 0;
			pdescinfo->buf_pframe[0] 	= 0; // no packet payload
		}
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV


#ifdef MERGE_TXDESC_HEADER_PAYLOAD
//if (q_num == BE_QUEUE) {
        pdescinfo->type             = _SKB_FRAME_TYPE_; // txdesc + 802.3 header + payload
        pdescinfo->pframe           = txcfg->pframe;    // skb

        pdescinfo->buf_type[0]      = 0;
        pdescinfo->buf_pframe[0]    = 0;

        if (((i == 0) && (txcfg->fr_type == _SKB_FRAME_TYPE_))) {
            //pdescinfo->type = _PRE_ALLOCLLCHDR_;
            release_wlanllchdr_to_poll(priv, txcfg->phdr);
        } else {
            //pdescinfo->type = _PRE_ALLOCHDR_;
            release_wlanhdr_to_poll(priv, txcfg->phdr);
        }
//}
#endif

#ifdef TX_SHORTCUT
        if (fit_shortcut) {
			//descinfo_copy(&txcfg->pstat->tx_sc_ent[idx].swdesc1, pdescinfo);
    		txcfg->pstat->tx_sc_ent[idx].swdesc1.type = pdescinfo->type;
#ifndef TXDESC_INFO			
	    	txcfg->pstat->tx_sc_ent[idx].swdesc1.len  = pdescinfo->len;
    		txcfg->pstat->tx_sc_ent[idx].swdesc1.rate = pdescinfo->rate;
#endif			
            //txcfg->pstat->tx_sc_ent[idx].swdesc1.buf_type[0] = pdescinfo->buf_type[0]
        }
#endif

		if (txcfg->fr_len == 0)
		{
//            printk("%s(%d): fr_len == 0 !!! \n", __FUNCTION__, __LINE__);
			goto init_deschead;
		}

		pbuf += fr_len;

#ifdef RESERVE_TXDESC_FOR_EACH_IF
		if (RSVQ_ENABLE && IS_RSVQ(q_num))
			priv->use_txdesc_cnt[RSVQ(q_num)] ++;
#endif
	}


init_deschead:

#ifdef WLAN_HAL_TX_AMSDU
    if (txcfg->aggre_en == FG_AGGRE_MSDU_FIRST) {
        return;
    }
#endif

    if(desc_data.SND_pkt_sel != 1) //SND_pkt_sel = 1 = send VHTNDPA_MU, needs Tx together with Report Poll
    GET_HAL_INTERFACE(priv)->SyncSWTXBDHostIdxToHWHandler(priv, halQNum);
	return;
}
#endif // CONFIG_WLAN_HAL

#if (!defined(__OSK__)) || (defined(__OSK__) && !defined(CONFIG_RTL6028))
__MIPS16
#endif
__IRAM_IN_865X unsigned int get_skb_priority(struct rtl8192cd_priv *priv, struct sk_buff *skb, struct stat_info *pstat, int is_11s)
{
	unsigned int pri=0, parsing=0;
	unsigned char protocol[2];

#ifdef WIFI_WMM
    if (QOS_ENABLE) {
        parsing = 1;
#ifdef CONFIG_PCI_HCI
		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
	        if(pstat && !pstat->QosEnabled)
	            parsing = 0;
		}
#endif
    }
#endif

	if (parsing) {
#ifdef TX2_BK_QUEUE
		if (pstat && pstat->tx2bk_on)
		{
			pri =1;

			skb->cb[1] = pri;
			return pri;
		}
#endif
#if defined(SSID_PRIORITY_SUPPORT)
		if (priv->pmib->miscEntry.manual_priority && (OPMODE & WIFI_AP_STATE))
			pri = priv->pmib->miscEntry.manual_priority;
		else
#endif

#if defined(CONFIG_RTK_VLAN_SUPPORT) ||defined(CONFIG_RTL_VLAN_SUPPORT) 
		if (skb->cb[0])
			pri =  skb->cb[0];
		else
#endif
		{
			protocol[0] = skb->data[12];
			protocol[1] = skb->data[13];

			if ((protocol[0] == 0x08) && (protocol[1] == 0x00))
			{
#ifdef CONFIG_RTK_MESH
				if(is_11s & RELAY_11S)
				{
					pri = (skb->data[31] & 0xe0) >> 5;
				}
				else
#endif
#ifdef HS2_SUPPORT
				if(priv->pmib->hs2Entry.QoSMap_ielen[priv->pmib->hs2Entry.curQoSMap]!=0) {
					pri=getDSCP2UP(priv, (skb->data[15] & 0xFC) >> 2);
				}
				else
#endif
				{
#if defined(CONFIG_SDIO_HCI) && defined(TX_SCATTER)
					pri = (get_skb_data_u8(skb, 15) & 0xe0) >> 5;
#else
					pri = (skb->data[15] & 0xe0) >> 5;
#endif
				}
			}
#ifndef NOT_RTK_BSP
			else if ((skb->cb[0]>0) && (skb->cb[0]<8))	// Ethernet driver will parse priority and put in cb[0]
				pri = skb->cb[0];
#endif
			else if (protocol[0] == 0x88 && protocol[1] == 0x8e)
				pri = 7;
			else
				pri = 0;
		}

#ifdef CLIENT_MODE
		if ((OPMODE & (WIFI_STATION_STATE | WIFI_ASOC_STATE)) == (WIFI_STATION_STATE | WIFI_ASOC_STATE)) {
			if (GET_STA_AC_VO_PARA.ACM) {
				if (!GET_STA_AC_VI_PARA.ACM) 
					pri = 5;
				else if (!GET_STA_AC_BE_PARA.ACM)
					pri = 0;
				else
					pri = 1;
			} else if (GET_STA_AC_VI_PARA.ACM) {
				if (!GET_STA_AC_BE_PARA.ACM)
					pri = 0;
				else
					pri = 1;
			} else if (GET_STA_AC_BE_PARA.ACM) {
				pri = 1;	// DSCP_BK tag = 1;
			}
		}
#endif	
#if defined(WIFI_WMM) && defined(WIFI_QOS_ENHANCE)
		pstat->qosenhance_old_tid = pstat->qosenhance_new_tid = pri;
		if(1 == priv->pshare->rf_ft_var.qos_enhance_active){
			if(qos_enhance_query_sta(priv, (unsigned char *)&(skb->data[0]))){
				if((pri == 0) || (pri == 3) || (pri == 5) || (pri == 4)){ //re-queue BE/VI
					pri = 7; //VO
					skb->cb[1] = pri;
				}
			} else {
				//not qos_enhance sta
				if(priv->pshare->rf_ft_var.qos_edge_enable == 1){
					if(!pstat->is_qosenhance_farsta){ //Near STA 
						pri = 5; //VI
						skb->cb[1] = pri;
					} else { //Far STA
						pri = 3; //BE
						skb->cb[1] = pri;
					}
				}
			}
			pstat->qosenhance_new_tid = pri;
		}
#endif	
	}
	
	skb->cb[_SKB_CB_PRIORITY] = pri;
	return pri;
}


#ifdef SUPPORT_TX_AMSDU
#ifdef __OSK__
__IRAM_WIFI_PRI3
#endif
static int amsdu_xmit(struct rtl8192cd_priv *priv, struct stat_info *pstat, struct tx_insn *txcfg, int tid,
				int from_isr, struct net_device *wdsDev, struct net_device *dev)
{
	int q_num, max_size, is_first=1, total_len=0, total_num=0;
	struct sk_buff *pskb;
	unsigned long	flags;

	txcfg->pstat = pstat;
	q_num = txcfg->q_num;

	max_size = pstat->amsdu_level;

#ifdef _AMPSDU_AMSDU_DEBUG_
	if(from_isr)
		pstat->tx_amsdu_to++;
#endif

	// start to transmit queued skb
	SAVE_INT_AND_CLI(flags);
	while (skb_queue_len(&pstat->amsdu_tx_que[tid]) > 0) {
		pskb = __skb_dequeue(&pstat->amsdu_tx_que[tid]);
		if (pskb == NULL)
			break;
		total_len += (pskb->len + sizeof(struct llc_snap) + 3);
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
		pskb->cb[_SKB_CB_AMSDU_TXSC] = q_num;
#endif
		if (is_first) {
			if (skb_queue_len(&pstat->amsdu_tx_que[tid]) > 0) {
                if (getAvailableTXBD(priv, q_num) < 10) {
#ifdef SMP_SYNC
					if (!priv->pshare->has_triggered_tx_tasklet) {
						tasklet_schedule(&priv->pshare->tx_tasklet);
						priv->pshare->has_triggered_tx_tasklet = 1;
					}
#else
					rtl8192cd_tx_dsr((unsigned long)priv);
#endif

                    if (getAvailableTXBD(priv, q_num) < 10) {
						// printk("Tx desc not enough for A-MSDU!\n");
						__skb_queue_head(&pstat->amsdu_tx_que[tid], pskb);
						RESTORE_INT(flags);
#ifdef _AMPSDU_AMSDU_DEBUG_
						pstat->tx_amsdu_buf_overflow++;
#endif						
						return 0;
					}
				}
				txcfg->aggre_en = FG_AGGRE_MSDU_FIRST;
#ifdef SUPPORT_TX_AMSDU_SHORTCUT	
				pskb->cb[_SKB_CB_AMSDU_TXSC] |= (FG_AGGRE_MSDU_FIRST << 4);
#endif
				is_first = 0;
				total_num++;
			}
			else {
				if (!from_isr) {
					__skb_queue_head(&pstat->amsdu_tx_que[tid], pskb);
					RESTORE_INT(flags);
#ifdef _AMPSDU_AMSDU_DEBUG_
					pstat->tx_amsdu_buf_overflow++;
#endif
					return 0;
				}
#if 0//def CONFIG_RTL_OFFLOAD_DRIVER
				else
					txcfg->last_msdu = 1;
#endif
				//Normal packet without AMSDU
				txcfg->aggre_en = 0;
			}
		}
		else if ((skb_queue_len(&pstat->amsdu_tx_que[tid]) == 0) ||
				((total_len + pstat->amsdu_tx_que[tid].next->len + sizeof(struct llc_snap) + 3) > max_size) ||
				(
#ifdef CONFIG_WLAN_HAL
                (IS_HAL_CHIP(priv)) ? (getAvailableTXBD(priv, q_num) < 1) : 
#endif
				(total_num >= 4)) || // 1 for header, 1 for ICV when sw encrypt, 2 for spare 
				(!pstat->is_realtek_sta && (total_num >= (priv->pmib->dot11nConfigEntry.dot11nAMSDUSendNum-1)))
#ifdef CONFIG_WLAN_HAL
                 || (total_num >= (priv->pmib->dot11nConfigEntry.dot11nCurAMSDUSendNum-1))
#endif
        ) {
			txcfg->aggre_en = FG_AGGRE_MSDU_LAST;
#ifdef SUPPORT_TX_AMSDU_SHORTCUT				
			pskb->cb[_SKB_CB_AMSDU_TXSC] |= (FG_AGGRE_MSDU_LAST << 4);
#endif
			total_len = 0;
			is_first = 1;
#ifdef _AMPSDU_AMSDU_DEBUG_
			switch(total_num){
				case 0:
					pstat->tx_amsdu_1pkt++;
					break;
				case 1:
					pstat->tx_amsdu_2pkt++;
					break;
				case 2:
					pstat->tx_amsdu_3pkt++;
					break;
				case 3:
					pstat->tx_amsdu_4pkt++;
					break;
				case 4:
					pstat->tx_amsdu_5pkt++;
					break;
				default:
					pstat->tx_amsdu_gt5pkt++;
					break;
			}
#endif
			total_num = 0;
		}
		else {
			txcfg->aggre_en = FG_AGGRE_MSDU_MIDDLE;
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
			pskb->cb[_SKB_CB_AMSDU_TXSC] |= (FG_AGGRE_MSDU_MIDDLE << 4);
#endif
			total_num++;
		}

		pstat->amsdu_size[tid] -= (pskb->len + sizeof(struct llc_snap));
#ifdef MESH_AMSDU
		if(isMeshPoint(pstat))
		{
			txcfg->is_11s = 8;
			dev = priv->mesh_dev;
			memcpy(txcfg->nhop_11s, pstat->cmn_info.mac_addr, MACADDRLEN);
		}
		else
			txcfg->is_11s = 0;

#endif
		//SMP_UNLOCK_XMIT(flags);
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
		if (OFFLOAD_ENABLE(priv)) {
			if (txcfg->aggre_en == 0)
				txcfg->h2d_type = TXPKTINFO_TYPE_8023;
			else
			txcfg->h2d_type = TXPKTINFO_TYPE_AMSDU;
			rtl8192cd_xmit_offload(pstat, pskb, txcfg);
		} else
#endif
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
		if(!priv->pmib->dot11OperationEntry.disable_amsdu_txsc)
		{
			__rtl8192cd_start_xmit_out(pskb, pstat, txcfg);
		}
		else
		{
			pstat->tx_sc_amsdu_pkts_slow++;
#endif
		rtl8192cd_tx_slowPath(priv, pskb, pstat, dev, wdsDev, txcfg);
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
		}
#endif
		//SMP_LOCK_XMIT(flags);
		if(from_isr)
			priv->pshare->amsdu_to_pkt++;
		else
			priv->pshare->amsdu_chk_pkt++;		
	}
	RESTORE_INT(flags);

	return 1;
}


int amsdu_timer_add(struct rtl8192cd_priv *priv, struct stat_info *pstat, int tid, int from_timeout)
{
	unsigned int now, timeout, new_timer=0;
	int setup_timer;
	int current_idx, next_idx;

	current_idx = priv->pshare->amsdu_timer_head;

	while (CIRC_CNT(current_idx, priv->pshare->amsdu_timer_tail, AMSDU_TIMER_NUM)) {
		if (priv->pshare->amsdu_timer[priv->pshare->amsdu_timer_tail].pstat == NULL) {
			priv->pshare->amsdu_timer_tail = (priv->pshare->amsdu_timer_tail + 1) & (AMSDU_TIMER_NUM - 1);
			new_timer = 1;
		}
		else
			break;
	}

	if (CIRC_CNT(current_idx, priv->pshare->amsdu_timer_tail, AMSDU_TIMER_NUM) == 0) {
		setup_timer = 1;
	}
	else if (CIRC_SPACE(current_idx, priv->pshare->amsdu_timer_tail, AMSDU_TIMER_NUM) == 0) {
		//printk("%s: %s, amsdu timer overflow!\n", priv->dev->name, __FUNCTION__ );
		pstat->tx_amsdu_timer_ovf++;
		return -1;
	}
	else {	// some items in timer queue
		setup_timer = 0;
		if (new_timer)
			new_timer = priv->pshare->amsdu_timer[priv->pshare->amsdu_timer_tail].timeout;
	}

	next_idx = (current_idx + 1) & (AMSDU_TIMER_NUM - 1);

	priv->pshare->amsdu_timer[current_idx].priv = priv;
	priv->pshare->amsdu_timer[current_idx].pstat = pstat;
	priv->pshare->amsdu_timer[current_idx].tid = (unsigned char)tid;
	priv->pshare->amsdu_timer_head = next_idx;

	if (!priv->pshare->amsdu_use_hw_timer) {
		now = jiffies;
	} else
#ifdef CONFIG_WLAN_HAL
    if ( IS_HAL_CHIP(priv) ) {
        GET_HAL_INTERFACE(priv)->GetHwRegHandler(priv, HW_VAR_TSF_TIMER, (pu1Byte)&now);
    } else
#endif
    {
	    now = RTL_R32(TSFTR1);
    }

	if (!priv->pshare->amsdu_use_hw_timer) {
			timeout = now + RTL_MILISECONDS_TO_JIFFIES(10);
	} else if (priv->pmib->dot11nConfigEntry.dot11nAMSDUSendTimeout) {
		timeout = now + priv->pmib->dot11nConfigEntry.dot11nAMSDUSendTimeout;
	} else {
#ifdef CONFIG_SPECIAL_ENV_TEST	
		if (priv->pshare->in_spec_env_test_to) {
			timeout = now + 15000;
		} else
#endif
#ifdef SUPPORT_TX_AMSDU_SMALL_PKTS_ONLY			
		if (pstat->amsdu_dropdupack_en) {
			timeout = now + 400;
		} else
#endif
		{
			timeout = now + 15000;
		}
	}
	
	priv->pshare->amsdu_timer[current_idx].timeout = timeout;

	if (!from_timeout) {
		if (setup_timer)
			setup_timer2(priv, timeout);
		else if (new_timer) {
			if (TSF_LESS(new_timer, now))
				setup_timer2(priv, timeout);
			else
				setup_timer2(priv, new_timer);
		}
	}

	return current_idx;
}


void amsdu_timeout(struct rtl8192cd_priv *priv, unsigned int current_time)
{
	struct tx_insn tx_insn;
	struct stat_info *pstat;
	struct net_device *wdsDev=NULL;
	struct rtl8192cd_priv *priv_this=NULL;
	int tid=0, head;
	//DECLARE_TXCFG(txcfg, tx_insn);
	unsigned long flags;
	
	SMP_LOCK_XMIT(flags);
	SAVE_INT_AND_CLI(flags);
	
	head = priv->pshare->amsdu_timer_head;
	while (CIRC_CNT(head, priv->pshare->amsdu_timer_tail, AMSDU_TIMER_NUM))
	{
		DECLARE_TXCFG(txcfg, tx_insn);	// will be reused in this while loop

		pstat = priv->pshare->amsdu_timer[priv->pshare->amsdu_timer_tail].pstat;
		if (pstat) {
			tid = priv->pshare->amsdu_timer[priv->pshare->amsdu_timer_tail].tid;
			priv_this = priv->pshare->amsdu_timer[priv->pshare->amsdu_timer_tail].priv;
			priv->pshare->amsdu_timer[priv->pshare->amsdu_timer_tail].pstat = NULL;
		}

		priv->pshare->amsdu_timer_tail = (priv->pshare->amsdu_timer_tail + 1) & (AMSDU_TIMER_NUM - 1);

		if (pstat) {
#ifdef WDS
			wdsDev = NULL;
			if (pstat->state & WIFI_WDS) {
				wdsDev = getWdsDevByAddr(priv, pstat->cmn_info.mac_addr);
				txcfg->wdsIdx = getWdsIdxByDev(priv, wdsDev);
			}
#endif
			txcfg->q_num = pri_to_qnum(priv_this, tid);

			if (pstat->state & WIFI_SLEEP_STATE)
				pstat->amsdu_timer_id[tid] = amsdu_timer_add(priv_this, pstat, tid, 1) + 1;
			else
			{
				int ret;
				ret = amsdu_xmit(priv_this, pstat, txcfg, tid, 1, wdsDev, priv->dev);
				
				if (ret == 0) // not finish
					pstat->amsdu_timer_id[tid] = amsdu_timer_add(priv_this, pstat, tid, 1) + 1;
				else
					pstat->amsdu_timer_id[tid] = 0;
		    }
	    }
	}

	if (CIRC_CNT(priv->pshare->amsdu_timer_head, priv->pshare->amsdu_timer_tail, AMSDU_TIMER_NUM)) {
		setup_timer2(priv, priv->pshare->amsdu_timer[priv->pshare->amsdu_timer_tail].timeout);
		if (TSF_LESS(priv->pshare->amsdu_timer[priv->pshare->amsdu_timer_tail].timeout, current_time))
			printk("Setup timer2 %d too late (now %d)\n", priv->pshare->amsdu_timer[priv->pshare->amsdu_timer_tail].timeout, current_time);
	} else {
		cancel_timer2(priv);
	}

	RESTORE_INT(flags);
	SMP_UNLOCK_XMIT(flags);	
}


 int amsdu_check(struct rtl8192cd_priv *priv, struct sk_buff *skb, struct stat_info *pstat, struct tx_insn *txcfg)
{
	int q_num;
	unsigned int priority;
	unsigned short protocol;
	int *tx_head, *tx_tail, cnt, add_timer=1;
	struct rtl8192cd_hw	*phw;
#ifndef SMP_SYNC
	unsigned long flags;
#endif
	struct net_device *wdsDev=NULL;

#if defined(CONFIG_WLAN_HAL_8814BE) && defined(CONFIG_RTL9607C)
	if (GET_CHIP_VER(priv) == VERSION_8814B)
	{
		if (priv->pmib->dot11nConfigEntry.dot11nAMSDURestrict)
		{
			if (skb->len > 1000)
				return RET_AGGRE_BYPASS;	
		}
	}
#endif

#ifdef TX_SKB_REFINE
	if (!txcfg->is_test)
#endif
	{
		if (skb->cb[_SKB_CB_FLAGS] & _SKB_CB_FLAGS_URGENT)
			return RET_AGGRE_BYPASS;

#ifdef CONFIG_RTL_OFFLOAD_DRIVER
		if (!OFFLOAD_ENABLE(priv))
#endif
		{
			protocol = ntohs(*((UINT16 *)((UINT8 *)skb->data + ETH_ALEN*2)));
			if (((protocol + WLAN_ETHHDR_LEN) > WLAN_MAX_ETHFRM_LEN) &&
					(skb_headroom(skb) < sizeof(struct llc_snap))) {
				return RET_AGGRE_BYPASS;
			}
		}
    } 
//----------------------

	priority = skb->cb[_SKB_CB_PRIORITY];
	q_num = skb->cb[_SKB_CB_QNUM];

    if (getAvailableTXBD(priv, q_num) < 10)
		return RET_AGGRE_DESC_FULL;

#ifdef MESH_AMSDU
	if (txcfg->is_11s & 1)
	{
		short j, popen =  ((txcfg->mesh_header.mesh_flag &1) ? 16 : 4);
		if (skb_headroom(skb) < popen || skb_cloned(skb)) {
			struct sk_buff *skb2 = dev_alloc_skb(skb->len);
			if (skb2 == NULL) {
				printk("%s: %s, dev_alloc_skb() failed!\n", priv->mesh_dev->name, __FUNCTION__);
				return RET_AGGRE_BYPASS;
			}
         	memcpy(skb_put(skb2, skb->len), skb->data, skb->len);
			dev_kfree_skb_any(skb);
			skb = skb2;
			txcfg->pframe = (void *)skb;
		}
		skb_push(skb, popen);
		for(j=0; j<sizeof(struct wlan_ethhdr_t); j++)
			skb->data[j]= skb->data[j+popen];
		memcpy(skb->data+j, &(txcfg->mesh_header), popen);
	}
#endif // MESH_AMSDU


	SAVE_INT_AND_CLI(flags);

#ifdef SUPPORT_TX_AMSDU_SMALL_PKTS_ONLY
	// dot11nAMSDUSmallPktLen 0 means queue TCP Ack only
	if(priv->pmib->dot11nConfigEntry.dot11nAMSDUSmallPkts && 
		!priv->pmib->dot11nConfigEntry.dot11nAMSDUSmallPktLen && 
		priv->pmib->dot11nConfigEntry.dot11nAMSDUTCPAckDropDup) {
		if (skb->cb[_SKB_CB_FLAGS] & _SKB_CB_FLAGS_TCPACK) {
			int i, exist = 0;
			for(i = 0; i < TCPACK_SESSION_MAX_ENTERY; i++) {
				if(!memcmp(pstat->amsdu_tcpack_ses[i].ip_port, skb->data+26, 12) && pstat->amsdu_tcpack_ses[i].used) {
					pstat->amsdu_tcpack_ses[i].ack_cnt++;
					exist = 1;
					break;
				}
			}
			if(exist) {
				if(pstat->amsdu_tcpack_ses[i].used == 1 && pstat->amsdu_tcpack_ses[i].ack_cnt > 99)
					pstat->amsdu_tcpack_ses[i].used = 2;
				pstat->amsdu_tcpack_ses[i].flag = 1;
			}
			else {
				for(i = 0; i < TCPACK_SESSION_MAX_ENTERY; i++) {
					if(pstat->amsdu_tcpack_ses[i].used == 0)
						break;
				}
				if(i == TCPACK_SESSION_MAX_ENTERY)
					i = i - 1;
				memcpy(pstat->amsdu_tcpack_ses[i].ip_port, skb->data+26, 12);
				pstat->amsdu_tcpack_ses[i].used = 1;
				pstat->amsdu_tcpack_ses[i].timeout = TCPACK_SESSION_TIMEOUT;
			}
			if(pstat->amsdu_dropdupack_en) {
				struct sk_buff *curr, *tmp;
				struct sk_buff_head *queue = &pstat->amsdu_tx_que[priority];
				
				skb_queue_reverse_walk_safe(queue, curr, tmp) {
					/* Drop duplicate TCP Ack if it's at the same tcp connection */
					if (!memcmp(curr->data+26, skb->data+26, 12)) {						
						skb_unlink(curr, queue);
						pstat->amsdu_size[priority] -= (curr->len + sizeof(struct llc_snap));
						rtl_kfree_skb(priv, curr, _SKB_TX_);
						break;
					}
				}
			}
		}
	}
#endif
	__skb_queue_tail(&pstat->amsdu_tx_que[priority], skb);
	pstat->amsdu_size[priority] += (skb->len + sizeof(struct llc_snap));
	// queue more packets for station of realtek even if amsdu_level is 3895, helpful on TX throughput
	if ((!(pstat->state & WIFI_SLEEP_STATE)) && ((!pstat->is_realtek_sta && pstat->amsdu_size[priority] >= pstat->amsdu_level) ||
		(pstat->is_realtek_sta && pstat->amsdu_size[priority] >= 7991) ||
		(skb_queue_len(&pstat->amsdu_tx_que[priority]) >= priv->pmib->dot11nConfigEntry.dot11nAMSDUSendNum)))
	{
#ifdef WDS
		wdsDev = NULL;
		if (pstat->state & WIFI_WDS) {
			wdsDev = getWdsDevByAddr(priv, pstat->cmn_info.mac_addr);
			txcfg->wdsIdx = getWdsIdxByDev(priv, wdsDev);
		}
#endif
		// delete timer entry
		if (pstat->amsdu_timer_id[priority] > 0) {
			priv->pshare->amsdu_timer[pstat->amsdu_timer_id[priority] - 1].pstat = NULL;
			pstat->amsdu_timer_id[priority] = 0;
		}
		txcfg->q_num = q_num;
		if (amsdu_xmit(priv, pstat, txcfg, priority, 0, wdsDev, priv->dev) == 0) // not finish
			pstat->amsdu_timer_id[priority] = amsdu_timer_add(priv, pstat, priority, 0) + 1;
		else
			add_timer = 0;
	}

	if (add_timer) {
		if (pstat->amsdu_timer_id[priority] == 0)
			pstat->amsdu_timer_id[priority] = amsdu_timer_add(priv, pstat, priority, 0) + 1;
	}

	RESTORE_INT(flags);

	return RET_AGGRE_ENQUE;
}
#endif // SUPPORT_TX_AMSDU

#ifdef CONFIG_PCI_HCI
/* This sub-routine is gonna to check how many tx desc we need */
static int check_txdesc(struct rtl8192cd_priv *priv, struct tx_insn* txcfg)
{
	struct sk_buff 	*pskb=NULL;
	unsigned short  protocol;
	unsigned char   *da=NULL;
	struct stat_info	*pstat=NULL;
	int priority=0;
	unsigned int is_dhcp = 0;

	if (txcfg->aggre_en == FG_AGGRE_MSDU_MIDDLE || txcfg->aggre_en == FG_AGGRE_MSDU_LAST)
		return TRUE;

	txcfg->privacy = txcfg->iv = txcfg->icv = txcfg->mic = 0;
	txcfg->frg_num = 0;
	txcfg->need_ack = 1;

	if (txcfg->fr_type == _SKB_FRAME_TYPE_)
	{
		pskb = ((struct sk_buff *)txcfg->pframe);
#ifdef TX_SCATTER
		if (pskb->list_num > 0)
			txcfg->fr_len = pskb->total_len - WLAN_ETHHDR_LEN;
		else
#endif
			txcfg->fr_len = pskb->len - WLAN_ETHHDR_LEN;

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
			txcfg->hdr_len = WLAN_HDR_A4_LEN;
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
					txcfg->fr_len += 8; // for Michael padding
					break;
				case _CCMP_PRIVACY_:
					txcfg->iv = 8;
					txcfg->icv = 0;
					txcfg->mic = 8;
					break;
			}
			txcfg->frg_num = 1;
			if (txcfg->aggre_en < FG_AGGRE_MSDU_FIRST) {
				priority = get_skb_priority(priv, (struct sk_buff *)txcfg->pframe, pstat, 0);
				txcfg->q_num = pri_to_qnum(priv, priority);
			}

			txcfg->tx_rate = get_tx_rate(priv, pstat);
#ifndef TX_LOWESTRATE
			txcfg->lowest_tx_rate = get_lowest_tx_rate(priv, pstat, txcfg->tx_rate);
#endif
			if (priv->pmib->dot11WdsInfo.entry[pstat->wds_idx].txRate)
				txcfg->fixed_rate = 1;
			txcfg->need_ack = 1;
			txcfg->pstat = pstat;
#ifdef WIFI_WMM
			if ((pstat) && (QOS_ENABLE) && (pstat->QosEnabled) && (is_qos_data(txcfg->phdr)))
				txcfg->hdr_len = WLAN_HDR_A4_QOS_LEN;
#endif

			if (txcfg->aggre_en == 0) {
				if ((pstat->aggre_mthd == AGGRE_MTHD_MPDU) && is_MCS_rate(txcfg->tx_rate))
					txcfg->aggre_en = FG_AGGRE_MPDU;
			}

			return TRUE;
		}
#endif

#ifdef WIFI_WMM
		if (OPMODE & WIFI_AP_STATE) {
			if( !txcfg->pstat ) {
#ifdef MCAST2UI_REFINE
				pstat = get_stainfo(priv, &pskb->cb[10]);
#else
				pstat = get_stainfo(priv, pskb->data);
#endif
			} else
				pstat = txcfg->pstat;
#ifdef A4_STA
			if (pstat == NULL) {
				pstat = txcfg->pstat;
			}
#endif
		}
		else if (OPMODE & WIFI_STATION_STATE)
			pstat = get_stainfo(priv, BSSID);
		else if (OPMODE & WIFI_ADHOC_STATE)
			pstat = get_stainfo(priv, pskb->data);

		if ((pstat) && (QOS_ENABLE) && (pstat->QosEnabled) && (is_qos_data(txcfg->phdr))) {
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
        {
#ifdef MCAST2UI_REFINE
			da = &pskb->cb[10];
#else
			da = pskb->data;
#endif
        }

#ifdef A4_STA
		if(priv->pmib->miscEntry.a4_enable && get_tofr_ds(txcfg->phdr) == 3) {
			if(pstat)
				da = pstat->cmn_info.mac_addr;
			txcfg->hdr_len += WLAN_ADDR_LEN;
		}
#endif

		//check if da is associated, if not, just drop and return false
		if (!IS_MCAST(da)
#ifdef CLIENT_MODE
			|| (OPMODE & WIFI_STATION_STATE)
#endif
			)
		{
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

			if ((pstat == NULL) || (!(pstat->state & WIFI_ASOC_STATE)))
			{
				DEBUG_INFO("TX DROP: Non asoc tx request!\n");
				return FALSE;
			}

			protocol = ntohs(*((UINT16 *)((UINT8 *)pskb->data + ETH_ALEN*2)));

			/* fix frag eap packet cause wps fail with 8812au */
			if (protocol == 0x888e)
				txcfg->frg_num = 1;

			/*--decide privacy--*/
#ifdef CONFIG_RTL_WAPI_SUPPORT
			if (protocol == ETH_P_WAPI){
				txcfg->privacy = 0;
			}else
#endif
#if (BEAMFORMING_SUPPORT == 1)
			if(txcfg->ndpa){
				txcfg->privacy = 0;
			}else
#endif			
			if (protocol == 0x888e ){	/*EAP type packets*/


				/*handle EAP protocol =1 and EAP type=0 or =1 =>all EAP exchange packets for WPS
				   -  all EAP exchange packets for WPS doesn't need encrypt				   
				   -  for WPA/WPA2  4-ways handle shake 4-4 may need encrpty,TKIP type 2 ways need encrypt*/
				#ifdef WIFI_SIMPLE_CONFIG				   
				unsigned char   *EAP_VERSION=NULL;	
				unsigned char   *EAP_TYPE=NULL;		
				   
				EAP_VERSION = (  (UINT8 *)pskb->data + ETH_ALEN*2  + 2);
				EAP_TYPE = EAP_VERSION+1 ;
				
				if( *EAP_VERSION==1 && ( *EAP_TYPE==1 ||*EAP_TYPE==0)){
					txcfg->privacy = 0; /*all EAP exchange packets for WPS no need encrypt*/
				}else
				#endif
				{
				
					if( GET_UNICAST_ENCRYP_KEYLEN == 0){
						/*for AES  4-ways handle shake 4-1~4-4 no need encrypt*/
						txcfg->privacy = 0;
					}else{
					
						/*for TKIP  4-ways handle shake 4-4 ,2-1 , 2-2 need encrpty*/
						txcfg->privacy = get_privacy(priv, pstat, &txcfg->iv, &txcfg->icv, &txcfg->mic);
					}
				}
			}
			else{
				txcfg->privacy = get_privacy(priv, pstat, &txcfg->iv, &txcfg->icv, &txcfg->mic);
			}


			/*--decide privacy--*/

			if ((!txcfg->is_test) && (OPMODE & WIFI_AP_STATE) && !IS_MCAST(da) && (pskb->cb[_SKB_CB_FLAGS] & _SKB_CB_FLAGS_DHCP))
				is_dhcp++;

			if ((!txcfg->is_test) && ((protocol == 0x888E)
#ifdef CONFIG_RTL_WAPI_SUPPORT
				||(protocol == ETH_P_WAPI)
#endif
				|| is_dhcp)) {
				// use basic rate to send EAP packet for sure
				txcfg->tx_rate = find_rate(priv, NULL, 0, 1);
#ifndef TX_LOWESTRATE
				txcfg->lowest_tx_rate = txcfg->tx_rate;
#endif
				txcfg->fixed_rate = 1;
			} else {
				txcfg->tx_rate = get_tx_rate(priv, pstat);
#ifndef TX_LOWESTRATE
				txcfg->lowest_tx_rate = get_lowest_tx_rate(priv, pstat, txcfg->tx_rate);
#endif
				if (!is_auto_rate(priv, pstat) &&
					!(should_restrict_Nrate(priv, pstat) && is_fixedMCSTxRate(priv, pstat)))
                    {            
#ifdef RTK_AC_SUPPORT
					if(! is_fixedVHTTxRate(priv, pstat) || (pstat->vht_cap_len))
#endif
					{
						txcfg->fixed_rate = 1;
					}
				}
			}

			if (txcfg->aggre_en < FG_AGGRE_MSDU_FIRST) {
#ifdef TX_SKB_REFINE
				if (txcfg->is_test) {
					struct sk_buff *tmp_skb = (struct sk_buff *)txcfg->pframe;
					txcfg->q_num = tmp_skb->cb[_SKB_CB_QNUM];
				} else
#endif
				{ 
					priority = get_skb_priority(priv, (struct sk_buff *)txcfg->pframe, pstat, txcfg->is_11s);
					txcfg->q_num = pri_to_qnum(priv, priority);
				}
			}

#ifdef CONFIG_RTL_WAPI_SUPPORT
			if (protocol==ETH_P_WAPI)
			{
				txcfg->q_num = MGNT_QUEUE;
			}
#endif

			if (pskb->cb[_SKB_CB_FLAGS] & _SKB_CB_FLAGS_URGENT) {
				txcfg->q_num = MGNT_QUEUE;
			}

			if (txcfg->aggre_en == 0
#if 0//def SUPPORT_TX_MCAST2UNI
				&& (priv->pshare->rf_ft_var.mc2u_disable || (pskb->cb[2] != (char)0xff))
#endif
			   ) {
				if ((pstat->aggre_mthd & AGGRE_MTHD_MPDU) &&
					/*	is_MCS_rate(txcfg->tx_rate) &&*/ (protocol != 0x888E)
#ifdef CONFIG_RTL_WAPI_SUPPORT
					&& (protocol != ETH_P_WAPI)
#endif
					&& !is_dhcp)
					txcfg->aggre_en = FG_AGGRE_MPDU;
			}

#ifdef WMM_DSCP_C42
			if((IS_HAL_CHIP(priv)) && (txcfg->q_num==BE_QUEUE) && priv->pshare->iot_mode_VO_exist) {
				txcfg->q_num++;
			}
#endif
			if(
#if defined(RTK_AC_SUPPORT) || defined(CONFIG_WLAN_HAL_8192EE) //FOR_VHT5G_PF
				( txcfg->pstat &&
				  ((txcfg->pstat->aggre_mthd == AGGRE_MTHD_MPDU_AMSDU) || (txcfg->pstat->aggre_mthd == AGGRE_MTHD_MPDU))
				  && txcfg->aggre_en ) ||
#endif
				(txcfg->aggre_en >= FG_AGGRE_MPDU && txcfg->aggre_en <= FG_AGGRE_MPDU_BUFFER_LAST)
			)
			{
				//panic_printk("%s %d pstat->ADDBA_ready[priority]=%d, priority=%d\n",__func__,__LINE__,pstat->ADDBA_ready[priority],priority);
				if (!pstat->ADDBA_ready[priority]) {
					if ((pstat->ADDBA_req_num[priority] < 5) && !pstat->ADDBA_sent[priority]) {
						pstat->ADDBA_req_num[priority]++;
						SMP_UNLOCK_XMIT(flags);
						issue_ADDBAreq(priv, pstat, priority);
						SMP_LOCK_XMIT(flags);
						pstat->ADDBA_sent[priority]++;
					}
				}
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
#if defined(CONFIG_WLAN_HAL)
			if (IS_HAL_CHIP(priv)) {
				if (OPMODE & WIFI_AP_STATE) {
                    u1Byte highq_start_idx;
#if IS_RTL88XX_MAC_V4
                    if(_GET_HAL_DATA(priv)->MacVersion.is_MAC_v4)
                        highq_start_idx = HIGH_QUEUE_V2;
#endif
#if IS_RTL88XX_MAC_V1_V2_V3
                    if(_GET_HAL_DATA(priv)->MacVersion.is_MAC_v1_v2_v3)
                        highq_start_idx = HIGH_QUEUE;
#endif
#ifdef MBSSID
					if (IS_ROOT_INTERFACE(priv))
						txcfg->q_num = highq_start_idx;
					else if (priv->vap_init_seq == 1)
						txcfg->q_num = highq_start_idx+1;
					else if (priv->vap_init_seq == 2)
						txcfg->q_num = highq_start_idx+2;
					else if (priv->vap_init_seq == 3)
						txcfg->q_num = highq_start_idx+3;
					else if (priv->vap_init_seq == 4)
						txcfg->q_num = highq_start_idx+4;
					else if (priv->vap_init_seq == 5)
						txcfg->q_num = highq_start_idx+5;
					else if (priv->vap_init_seq == 6)
						txcfg->q_num = highq_start_idx+6;
					else if (priv->vap_init_seq == 7)
						txcfg->q_num = highq_start_idx+7;
#if IS_RTL8198F_SERIES || IS_RTL8814B_SERIES
            if ( IS_HARDWARE_TYPE_8198F(priv) || IS_HARDWARE_TYPE_8814B(priv)) {   
                
                	if (priv->vap_init_seq == 8)
						txcfg->q_num = highq_start_idx+8;
					else if (priv->vap_init_seq == 9)
						txcfg->q_num = highq_start_idx+9;
					else if (priv->vap_init_seq == 10)
						txcfg->q_num = highq_start_idx+10;
					else if (priv->vap_init_seq == 11)
						txcfg->q_num = highq_start_idx+11;
					else if (priv->vap_init_seq == 12)
						txcfg->q_num = highq_start_idx+12;
					else if (priv->vap_init_seq == 13)
						txcfg->q_num = highq_start_idx+13;
					else if (priv->vap_init_seq == 14)
						txcfg->q_num = highq_start_idx+14;
					else if (priv->vap_init_seq == 15)
						txcfg->q_num = highq_start_idx+15;                   
            }
#endif // IS_RTL8198F_SERIES || IS_RTL8814B_SERIES  
#else
					txcfg->q_num = highq_start_idx;
#endif
					SetMData(txcfg->phdr);
				} else {
					// to do: sta mode?
					txcfg->q_num = BE_QUEUE;
					pskb->cb[1] = 0;
				}
			} else if (CONFIG_WLAN_NOT_HAL_EXIST)
#endif
			{
#if !defined(TAROKO_0)
				//not HAL
				if ((OPMODE & WIFI_AP_STATE) && priv->pkt_in_dtimQ) {
					txcfg->q_num = MCAST_QNUM;
					SetMData(txcfg->phdr);
				}
				else {
					txcfg->q_num = BE_QUEUE;
					pskb->cb[1] = 0;
				}
#endif
			}

			if ((*da) == 0xff)	// broadcast
				txcfg->tx_rate = find_rate(priv, NULL, 0, 1);
			else {				// multicast
				if (priv->pmib->dot11StationConfigEntry.lowestMlcstRate)
					txcfg->tx_rate = get_rate_from_bit_value(priv->pmib->dot11StationConfigEntry.lowestMlcstRate);
				else
					txcfg->tx_rate = find_rate(priv, NULL, 1, 1);
			}

#ifndef TX_LOWESTRATE
			txcfg->lowest_tx_rate = txcfg->tx_rate;
#endif
			txcfg->fixed_rate = 1;
		}
	}

	if (!da)
	{
		// This is non data frame, no need to frag.
#ifdef CONFIG_RTK_MESH
		if(txcfg->is_11s)
			da = GetAddr1Ptr(txcfg->phdr);
		else
#endif
			da = get_da((unsigned char *) (txcfg->phdr));

		txcfg->frg_num = 1;

		if (IS_MCAST(da))
			txcfg->need_ack = 0;
		else
			txcfg->need_ack = 1;

		if (GetPrivacy(txcfg->phdr))
		{
#ifdef CONFIG_IEEE80211W 
			if(txcfg->isPMF) {
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

#ifdef CLIENT_MODE
	if (OPMODE & (WIFI_AP_STATE|WIFI_ADHOC_STATE) )
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
		else if(!(txcfg->phdr && (GetFrameType(txcfg->phdr) == WIFI_MGT_TYPE) && (GetFrameSubType((unsigned char *) (txcfg->phdr))>>4 == 5)))	//  exclude probe rsp
		{
			pstat = get_stainfo(priv, da);
			txcfg->pstat = pstat;
		}
	}
#ifdef CLIENT_MODE
	else if (OPMODE & WIFI_STATION_STATE)
	{
		if ((txcfg->fr_type == _SKB_FRAME_TYPE_) ||							// skb frame
				(!memcmp(GetAddr1Ptr(txcfg->phdr), BSSID, MACADDRLEN) && (GetFrameSubType(txcfg->phdr) != WIFI_PROBEREQ))) {		// mgt frame to AP
			pstat = get_stainfo(priv, BSSID);
			txcfg->pstat = pstat;
		}
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
	if (txcfg->privacy == _TKIP_PRIVACY_)
		txcfg->fr_len += 8;	// for Michael padding.

	txcfg->frag_thrshld -= (txcfg->mic + txcfg->iv + txcfg->icv + txcfg->hdr_len);

	if (txcfg->frg_num == 0)
	{
		if (txcfg->aggre_en > 0)
			txcfg->frg_num = 1;
		else {
			// how many mpdu we need...
			int llc;

			if ((ntohs(*((UINT16 *)((UINT8 *)pskb->data + ETH_ALEN*2))) + WLAN_ETHHDR_LEN) > WLAN_MAX_ETHFRM_LEN)
				llc = sizeof(struct llc_snap);
			else
				llc = 0;

			txcfg->frg_num = (txcfg->fr_len + llc) / txcfg->frag_thrshld;
			if (((txcfg->fr_len + llc) % txcfg->frag_thrshld) != 0)
				txcfg->frg_num += 1;
		}
	}

#ifdef TX_SCATTER
	if (pskb && pskb->list_num > 0 && txcfg->frg_num > 1) {
		struct sk_buff *newskb = copy_skb(pskb);
		if (newskb == NULL) {
			DEBUG_ERR("TX DROP: Can't copy the skb for list buffer in frag!\n");
			return FALSE;
		}
		dev_kfree_skb_any(pskb);
		txcfg->pframe = (void *)newskb;
	}
#endif

	return TRUE;
}
#endif // CONFIG_PCI_HCI





int __rtl8192cd_firetx(struct rtl8192cd_priv *priv, struct tx_insn* txcfg)
{
	int					*tx_head=NULL, *tx_tail=NULL, q_num;
	unsigned int		val32=0, is_dhcp=0;
	struct sk_buff		*pskb=NULL;
	struct llc_snap		*pllc_snap;
	struct wlan_ethhdr_t	*pethhdr=NULL;
#ifdef SUPPORT_TX_AMSDU
	struct wlan_ethhdr_t	*pmsdu_hdr;
	struct wlan_ethhdr_t  pethhdr_data;
#endif
	struct rtl8192cd_hw	*phw = GET_HW(priv);
#ifdef CONFIG_WLAN_HAL
#ifndef TRXBD_CACHABLE_REGION
//	PHCI_TX_DMA_MANAGER_88XX	ptx_dma;
#endif	
#endif

	unsigned long		x;
    unsigned char *da;
#if defined(CONFIG_RTL_WAPI_SUPPORT)
	struct wlan_ethhdr_t	ethhdr;
	pethhdr = &ethhdr;
#endif

#ifdef PCIE_POWER_SAVING_TEST // yllin
    if((priv->pwr_state >= L2) || (priv->pwr_state == L1)) { 
        return FAIL; 
    }
#endif

	int bUnAvail = FALSE;

#ifdef CONFIG_PCI_HCI
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
		/*---frag_thrshld setting---plus tune---0115*/
#ifdef	WDS
		if (txcfg->wdsIdx >= 0) {
			txcfg->frag_thrshld = 2346; // if wds, disable fragment
		} else
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
		txcfg->frg_num = 0;

#ifdef DFS
		if (!priv->pmib->dot11DFSEntry.disable_DFS &&
			GET_ROOT(priv)->pmib->dot11DFSEntry.disable_tx
			&& (priv->site_survey)
			&& (priv->site_survey->hidden_ap_found != HIDE_AP_FOUND_DO_ACTIVE_SSAN)) {
			priv->ext_stats.tx_drops++;
			DEBUG_ERR("TX DROP: DFS probation period\n");

			if (txcfg->fr_type == _SKB_FRAME_TYPE_) {
				rtl_kfree_skb(priv, (struct sk_buff *)txcfg->pframe, _SKB_TX_);
				release_wlanllchdr_to_poll(priv, txcfg->phdr);
			} else if (txcfg->fr_type == _PRE_ALLOCMEM_) {
				release_mgtbuf_to_poll(priv, txcfg->pframe);
				release_wlanhdr_to_poll(priv, txcfg->phdr);
			} else if (txcfg->fr_type == _PRE_ALLOCHDR_) {
				release_wlanhdr_to_poll(priv, txcfg->phdr);
			}
#if (BEAMFORMING_SUPPORT == 1)
			else if (txcfg->ndpa) {
				release_wlanllchdr_to_poll(priv, txcfg->phdr);
			}
#endif

			return SUCCESS;
		}
#endif

#ifdef AP_SWPS_OFFLOAD_VERI
        if(txcfg->test_pkt==1)
            txcfg->frg_num = 1;
        else
#endif
		if ((check_txdesc(priv, txcfg)) == FALSE) // this will only happen in errorous forwarding
		{
			priv->ext_stats.tx_drops++;
			if (txcfg->fr_type == _SKB_FRAME_TYPE_) {
				rtl_kfree_skb(priv, (struct sk_buff *)txcfg->pframe, _SKB_TX_);
				release_wlanllchdr_to_poll(priv, txcfg->phdr);
			}
			else if (txcfg->fr_type == _PRE_ALLOCMEM_) {
				release_mgtbuf_to_poll(priv, txcfg->pframe);
				release_wlanhdr_to_poll(priv, txcfg->phdr);
			} else if (txcfg->fr_type == _PRE_ALLOCHDR_) {
				release_wlanhdr_to_poll(priv, txcfg->phdr);
			}
			return SUCCESS;
		}
 
		if (txcfg->aggre_en > 0)
			txcfg->frag_thrshld = 2346;

		q_num = txcfg->q_num;
		
		if (txcfg->aggre_en < FG_AGGRE_MSDU_FIRST) {
			// now we are going to calculate how many hw desc we should have before tx...
			// wlan_hdr(including iv and llc) will occupy one desc, payload will occupy one, and
			// icv/mic will occupy the third desc if swcrypto is utilized.

			int num_swq_rsvd = 0;

#ifdef CONFIG_WLAN_HAL
			if (IS_HAL_CHIP(priv)) {
		        val32 = txcfg->frg_num;
			} else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif
			{//not HAL
				{
					tx_head = get_txhead_addr(phw, q_num);
					tx_tail = get_txtail_addr(phw, q_num);
				}
			

				if (txcfg->privacy)
				{
					if (
		#if defined(CONFIG_RTL_WAPI_SUPPORT)
							(_WAPI_SMS4_ != txcfg->privacy) &&
		#endif
#ifdef CONFIG_IEEE80211W
				UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE), txcfg->isPMF))
#else
				UseSwCrypto(priv, txcfg->pstat, (txcfg->pstat ? FALSE : TRUE)))
#endif	

						val32 = txcfg->frg_num * 3;  //extra one is for ICV padding.
					else
						val32 = txcfg->frg_num * 2;
				}
				else {
					val32 = txcfg->frg_num * 2;
				}

#ifdef TX_SCATTER
				if (txcfg->fr_type == _SKB_FRAME_TYPE_ &&
						((struct sk_buff *)txcfg->pframe)->key > 0 &&  
							((struct sk_buff *)txcfg->pframe)->list_num > 1)
					val32 += ((struct sk_buff *)txcfg->pframe)->list_num -1;
#endif
			}

#ifdef SW_TXQ_RSVD_DESC
			{
				int q_num_t = txcfg->q_num;
				if (txcfg->fr_type==_SKB_FRAME_TYPE_ && q_num_t>=BK_QUEUE && q_num_t<=VO_QUEUE)
				{
					struct sk_buff *skb_t = (struct sk_buff *)txcfg->pframe;
					struct stat_info *pstat_t = txcfg->pstat;
					if (skb_t && pstat_t)
					{
						int pri_t = get_skb_priority(priv, skb_t, pstat_t, txcfg->is_11s);
						if (!pstat_t->ADDBA_ready[pri_t])
						{
							int max_queue_cnt = CURRENT_NUM_TX_DESC >> 3;
							if (priv->pshare->swq_skb_queue_cnt[q_num_t - 1] >= max_queue_cnt)
								num_swq_rsvd = max_queue_cnt;
							else
								num_swq_rsvd = priv->pshare->swq_skb_queue_cnt[q_num_t - 1];

							num_swq_rsvd = num_swq_rsvd >> 1;
						}
					}
				}
			}
#endif

#ifdef RESERVE_TXDESC_FOR_EACH_IF
			if(RSVQ_ENABLE && IS_RSVQ(q_num)) {
				int num_consumed =
#ifdef CONFIG_WLAN_HAL
					IS_HAL_CHIP(priv) ? txcfg->frg_num :
#endif
					val32;
				bUnAvail = check_rsv_txdesc(priv, q_num, num_consumed);
			} else
#endif
			{
#ifdef CONFIG_WLAN_HAL
				if (IS_HAL_CHIP(priv)) {
					bUnAvail = (getAvailableTXBD(priv, q_num) < (val32 + 1 + num_swq_rsvd));
				} else
#endif
				{
					bUnAvail = (getAvailableTXBD(priv, q_num) < (val32 + 2 + num_swq_rsvd));
				}
			}

			if (bUnAvail) {
					if(IS_HAL_CHIP(priv)) {
						DEBUG_WARN("%s:%d: tx drop: %d hw Queue desc not available!\n", __FUNCTION__, __LINE__, q_num);
					} else
						DEBUG_ERR("%d hw Queue desc not available! head=%d, tail=%d request %d\n",q_num,*tx_head,*tx_tail,2);
					//SMP_UNLOCK_XMIT(x);
					return CONGESTED;
				}
			}

		// then we have to check if wlan-hdr is available for usage...
		// actually, the checking can be void

		/* ----------
					Actually, I believe the check below is redundant.
					Since at this moment, desc is available, hdr/icv/
					should be enough.
														--------------*/
		val32 = txcfg->frg_num;

		if (val32 >= priv->pshare->pwlan_hdr_poll->count)
		{
			DEBUG_ERR("%d hw Queue tx without enough wlan_hdr\n", q_num);
			return CONGESTED;
		}
	}
#endif // CONFIG_PCI_HCI

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
		switch( rtw_xmit_decision(priv, txcfg) ) {
		case XMIT_DECISION_ENQUEUE:
			return SUCCESS;
		case XMIT_DECISION_STOP:
			return CONGESTED;
		case XMIT_DECISION_CONTINUE:
			break;
		}
		
		if (update_txinsn_stage2(priv, txcfg) == FALSE) {
			return CONGESTED;
		}
		
		q_num = txcfg->q_num;
	}
#endif // CONFIG_USB_HCI || CONFIG_SDIO_HCI

	// then we have to check if wlan_snapllc_hdrQ is enough for use
	// for each msdu, we need wlan_snapllc_hdrQ for maximum

	if (txcfg->fr_type == _SKB_FRAME_TYPE_)
	{
		pskb = (struct sk_buff *)txcfg->pframe;

#ifdef CONFIG_PCI_HCI
		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
#ifdef TX_SKB_REFINE
			if (! txcfg->is_test)	// cliff test
#endif
			{
				if ((OPMODE & WIFI_AP_STATE) &&(pskb->cb[_SKB_CB_FLAGS] & _SKB_CB_FLAGS_DHCP))
					is_dhcp++;
			}
		}
#endif

#ifdef SUPPORT_TX_AMSDU
		// for AMSDU
		if (txcfg->aggre_en >= FG_AGGRE_MSDU_FIRST) {
			unsigned short usLen;
			int msdu_len;

			memcpy(&pethhdr_data, pskb->data, sizeof(struct wlan_ethhdr_t));
			pethhdr = &pethhdr_data;
			msdu_len = pskb->len - WLAN_ETHHDR_LEN;
			if ((ntohs(pethhdr->type) + WLAN_ETHHDR_LEN) > WLAN_MAX_ETHFRM_LEN) {
				if (skb_headroom(pskb) < sizeof(struct llc_snap)||skb_tailroom(pskb)<4) {
					struct sk_buff *skb2 = dev_alloc_skb(pskb->len+4);
#ifdef MCAST2UI_REFINE
					memcpy(skb2->cb, pskb->cb, sizeof(pskb->cb));
#endif
					if (skb2 == NULL) {
						printk("%s: %s, dev_alloc_skb() failed!\n", priv->dev->name, __FUNCTION__);
						rtl_kfree_skb(priv, pskb, _SKB_TX_);
						if(txcfg->aggre_en == FG_AGGRE_MSDU_FIRST)
							release_wlanllchdr_to_poll(priv, txcfg->phdr);
						return 0;
					}
					memcpy(skb_put(skb2, pskb->len), pskb->data, pskb->len);
					dev_kfree_skb_any(pskb);
					pskb = skb2;
					txcfg->pframe = (void *)pskb;
				}
				skb_push(pskb, sizeof(struct llc_snap));
			}
			pmsdu_hdr = (struct wlan_ethhdr_t *)pskb->data;

			memcpy(pmsdu_hdr, pethhdr, 12);
			if ((ntohs(pethhdr->type) + WLAN_ETHHDR_LEN) > WLAN_MAX_ETHFRM_LEN) {
				usLen = (unsigned short)(msdu_len + sizeof(struct llc_snap));
				pmsdu_hdr->type = htons(usLen);
				eth_2_llc(pethhdr, (struct llc_snap *)(((unsigned long)pmsdu_hdr)+sizeof(struct wlan_ethhdr_t)));
			}
			else {
				usLen = (unsigned short)msdu_len;
				pmsdu_hdr->type = htons(usLen);
			}

			if (txcfg->aggre_en == FG_AGGRE_MSDU_FIRST)
				eth2_2_wlanhdr(priv, pethhdr, txcfg);

			txcfg->llc = 0;
			pllc_snap = NULL;

			if (txcfg->aggre_en == FG_AGGRE_MSDU_FIRST || txcfg->aggre_en == FG_AGGRE_MSDU_MIDDLE) {
				if ((usLen+14) % 4)
					skb_put(pskb, 4-((usLen+14)%4));
			}
			txcfg->fr_len = pskb->len;
		}
		else 
#endif /* SUPPORT_TX_AMSDU */
		{	// not A-MSDU
#if defined(CONFIG_SDIO_HCI) && defined(TX_SCATTER)
			// in case 1st buffer len is 14, we get ether header pointer first and then ajust the skb
			pethhdr = (struct wlan_ethhdr_t *)(pskb->data);
			// now, we should adjust the skb ...
			skb_pull(pskb, WLAN_ETHHDR_LEN);
#else
			// now, we should adjust the skb ...
			skb_pull(pskb, WLAN_ETHHDR_LEN);
#ifdef CONFIG_RTK_MESH
            pethhdr = &priv->ethhdr;
			memcpy(pethhdr, pskb->data - sizeof(struct wlan_ethhdr_t), sizeof(struct wlan_ethhdr_t));
#else
			pethhdr = (struct wlan_ethhdr_t *)(pskb->data - sizeof(struct wlan_ethhdr_t));
#endif

#endif

#ifdef AP_SWPS_OFFLOAD_VERI
        if(txcfg->test_pkt==1)
            pllc_snap = NULL;
        else
#endif
        {
			pllc_snap = (struct llc_snap *)((UINT8 *)(txcfg->phdr) + txcfg->hdr_len + txcfg->iv);

			if ((ntohs(pethhdr->type) + WLAN_ETHHDR_LEN) > WLAN_MAX_ETHFRM_LEN) {
				eth_2_llc(pethhdr, pllc_snap);
				txcfg->llc = sizeof(struct llc_snap);
			}
			else
			{
				pllc_snap = NULL;
			}
        }
#ifdef AP_SWPS_OFFLOAD_VERI
        if(txcfg->test_pkt!=1)
#endif
        {
			eth2_2_wlanhdr(priv, pethhdr, txcfg);
        }
#ifdef CONFIG_PCI_HCI
			if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
#ifdef TX_SCATTER
				if (pskb->len == 0 && pskb->list_num > 1) {
					pskb->list_idx++;
					skb_assign_buf(pskb, pskb->list_buf[pskb->list_idx].buf, pskb->list_buf[pskb->list_idx].len);
					pskb->len = pskb->list_buf[pskb->list_idx].len;
				}
#endif
			}
#endif

#ifdef CONFIG_RTK_MESH
			if (txcfg->is_11s & 1 && GetFrameSubType(txcfg->phdr) == WIFI_11S_MESH)
			{
				const short meshhdrlen = (txcfg->mesh_header.mesh_flag & 0x01) ? 16 : 4;
				if (skb_cloned(pskb))
				{
					struct sk_buff	*newskb = skb_copy(pskb, GFP_ATOMIC);
					rtl_kfree_skb(priv, pskb, _SKB_TX_);
					if (newskb == NULL) {
						priv->ext_stats.tx_drops++;
						release_wlanllchdr_to_poll(priv, txcfg->phdr);
						DEBUG_ERR("TX DROP: Can't copy the skb!\n");
						return SUCCESS;
					}
					txcfg->pframe = pskb = newskb;
#ifdef ENABLE_RTL_SKB_STATS
					rtl_atomic_inc(&priv->rtl_tx_skb_cnt);
#endif
				}
				memcpy(skb_push(pskb, meshhdrlen), &(txcfg->mesh_header), meshhdrlen);
				txcfg->fr_len += meshhdrlen;
			}
#endif // CONFIG_RTK_MESH
		}

		// TODO: modify as when skb is not bigger enough, take ICV from local pool
#ifdef CONFIG_RTL_WAPI_SUPPORT
		if (txcfg->privacy == _WAPI_SMS4_)
		{
			if ((pskb->tail + SMS4_MIC_LEN) > pskb->end)
			{
				struct sk_buff *pnewskb;
				
				pnewskb = skb_copy_expand(pskb, skb_headroom(pskb), SMS4_MIC_LEN, GFP_ATOMIC);
				if (NULL == pnewskb) {
					priv->ext_stats.tx_drops++;
					DEBUG_ERR("TX DROP: an over size skb!\n");
					rtl_kfree_skb(priv, (struct sk_buff *)txcfg->pframe, _SKB_TX_);
					release_wlanllchdr_to_poll(priv, txcfg->phdr);
					return SUCCESS;
				}
				
				dev_kfree_skb_any(pskb);
				pskb = pnewskb;
				txcfg->pframe = pskb;
#ifndef CONFIG_RTK_MESH
				if (txcfg->aggre_en < FG_AGGRE_MSDU_FIRST)
					pethhdr = (struct wlan_ethhdr_t *)(pskb->data - sizeof(struct wlan_ethhdr_t));
#endif
			}
			memcpy(&ethhdr, pethhdr, sizeof(struct wlan_ethhdr_t));
#ifdef MCAST2UI_REFINE
                        memcpy(&ethhdr.daddr, &pskb->cb[10], 6);
#endif
			pethhdr = &ethhdr;
		} else
#endif

		if (txcfg->privacy == _TKIP_PRIVACY_)
		{
			if ((pskb->tail + 8) > pskb->end)
			{
#if defined(__ECOS)
				priv->ext_stats.tx_drops++;
				DEBUG_ERR("TX DROP: an over size skb!\n");
				rtl_kfree_skb(priv, (struct sk_buff *)txcfg->pframe, _SKB_TX_);
				release_wlanllchdr_to_poll(priv, txcfg->phdr);
				return SUCCESS;
				
#else // NOT_RTK_BSP
				struct sk_buff *pnewskb;
				
				pnewskb = skb_copy_expand(pskb, skb_headroom(pskb), 8, GFP_ATOMIC);
				if (NULL == pnewskb) {
					priv->ext_stats.tx_drops++;
					DEBUG_ERR("TX DROP: an over size skb!\n");
					rtl_kfree_skb(priv, (struct sk_buff *)txcfg->pframe, _SKB_TX_);
					release_wlanllchdr_to_poll(priv, txcfg->phdr);
					return SUCCESS;
				}
				
				dev_kfree_skb_any(pskb);
				pskb = pnewskb;
				txcfg->pframe = pskb;
#ifndef CONFIG_RTK_MESH
				if (txcfg->aggre_en < FG_AGGRE_MSDU_FIRST)
					pethhdr = (struct wlan_ethhdr_t *)(pskb->data - sizeof(struct wlan_ethhdr_t));
#endif
#endif
			}

            da = pethhdr->daddr;
#ifdef MCAST2UI_REFINE
            memcpy(pethhdr->daddr, &pskb->cb[10], 6);
#endif

#ifdef A4_STA
            if(txcfg->pstat && (txcfg->pstat->state & WIFI_A4_STA)) {
                da = GetAddr3Ptr(txcfg->phdr);
            }
#endif

#ifdef WIFI_WMM
			if ((tkip_mic_padding(priv, da, pethhdr->saddr, ((QOS_ENABLE) && (txcfg->pstat) && (txcfg->pstat->QosEnabled))?pskb->cb[1]:0, (UINT8 *)pllc_snap,
					pskb, txcfg)) == FALSE)
#else
			if ((tkip_mic_padding(priv, da, pethhdr->saddr, 0, (UINT8 *)pllc_snap,
					pskb, txcfg)) == FALSE)
#endif
			{
				priv->ext_stats.tx_drops++;
				DEBUG_ERR("TX DROP: Tkip mic padding fail!\n");
				rtl_kfree_skb(priv, pskb, _SKB_TX_);
				release_wlanllchdr_to_poll(priv, txcfg->phdr);
				return SUCCESS;
			}
			if ((txcfg->aggre_en < FG_AGGRE_MSDU_FIRST) || (txcfg->aggre_en == FG_AGGRE_MSDU_LAST))
				skb_put((struct sk_buff *)txcfg->pframe, 8);
		}
	}

	if (txcfg->privacy && txcfg->aggre_en <= FG_AGGRE_MSDU_FIRST)
		SetPrivacy(txcfg->phdr);

	// log tx statistics...
	tx_sum_up(priv, txcfg->pstat, txcfg);

	SNMP_MIB_INC(dot11TransmittedFragmentCount, 1);

	// for SW LED
	if (txcfg->aggre_en > FG_AGGRE_MSDU_FIRST || GetFrameType(txcfg->phdr) == WIFI_DATA_TYPE) {
		priv->pshare->LED_tx_cnt++;
	} else {
		if (priv->pshare->LED_cnt_mgn_pkt)
			priv->pshare->LED_tx_cnt++;
	}

#ifdef PCIE_POWER_SAVING
	PCIeWakeUp(priv, (POWER_DOWN_T0));
#endif

	SAVE_INT_AND_CLI(x);
	//SMP_LOCK_XMIT(x);
	
#ifdef SUPPORT_TX_AMSDU
	if (txcfg->aggre_en == FG_AGGRE_MSDU_MIDDLE || txcfg->aggre_en == FG_AGGRE_MSDU_LAST) {
#ifdef CONFIG_WLAN_HAL
        if (IS_HAL_CHIP(priv)) {
            rtl88XX_signin_txdesc_amsdu(priv, txcfg);
        } else 
#endif
        {
        	rtl8192cd_signin_txdesc_amsdu(priv, txcfg);
	    }
	} else
#endif
	{

#if defined(CONFIG_PCI_HCI)
        if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
#ifdef CONFIG_WLAN_HAL
			if (IS_HAL_CHIP(priv)) {
				// TODO: these if conditions are all the same as the following code, it should be optimzed.....
				if((!priv->pmib->dot11OperationEntry.disable_txsc) &&
					(txcfg->fr_type == _SKB_FRAME_TYPE_) &&
					(txcfg->pstat) &&
					(txcfg->frg_num == 1) &&
					((txcfg->privacy == 0)	
#ifdef CONFIG_IEEE80211W
						|| (!UseSwCrypto(priv, txcfg->pstat, FALSE, txcfg->isPMF))) &&
#else
						|| (!UseSwCrypto(priv, txcfg->pstat, FALSE))) &&
#endif
					(pethhdr->type != htons(0x888e)) &&
					!is_dhcp && 
					!GetMData(txcfg->phdr) &&
					#ifdef MCAST2UI_REFINE
	                pskb && !IS_MCAST(&pskb->cb[10]) &&
					#else
					!IS_MCAST((unsigned char *)pethhdr) &&
					#endif
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
					(txcfg->aggre_en <=FG_AGGRE_MSDU_FIRST)
#else
					(txcfg->aggre_en < FG_AGGRE_MSDU_FIRST)
#endif
				) {				
					rtl88XX_signin_txdesc(priv, txcfg, HW_TX_SC_BACKUP_HEADER);
				} else {
					rtl88XX_signin_txdesc(priv, txcfg, HW_TX_SC_NORMAL);
				}
				
			} else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif		
			{//not HAL
				rtl8192cd_signin_txdesc(priv, txcfg);
			}
		}
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
		if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
		#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
			rtl8192cd_signin_txdesc_usb(priv, txcfg, pethhdr);
		#else
			rtl8192cd_signin_txdesc(priv, txcfg, pethhdr);
		#endif
		}
#endif

	}

	//SMP_UNLOCK_XMIT(x);
	RESTORE_INT(x);

#ifdef CONFIG_PCI_HCI
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
#ifdef TX_SHORTCUT
		if ((!priv->pmib->dot11OperationEntry.disable_txsc) &&
			(txcfg->fr_type == _SKB_FRAME_TYPE_) &&
			(txcfg->pstat) &&
			(txcfg->frg_num == 1) &&
			((txcfg->privacy == 0)
#ifdef CONFIG_RTL_WAPI_SUPPORT
			|| (txcfg->privacy == _WAPI_SMS4_)
#endif
#ifdef CONFIG_IEEE80211W
			|| !UseSwCrypto(priv, txcfg->pstat, FALSE, txcfg->isPMF)) &&
#else
			|| !UseSwCrypto(priv, txcfg->pstat, FALSE)) &&
#endif
			/*(!IEEE8021X_FUN ||
				(IEEE8021X_FUN && (txcfg->pstat->ieee8021x_ctrlport == 1) &&
				(pethhdr->type != htons(0x888e)))) && */
			(pethhdr->type != htons(0x888e)) &&
			!is_dhcp && 
#ifdef CONFIG_RTL_WAPI_SUPPORT
			(pethhdr->type != htons(ETH_P_WAPI)) &&
#endif
			!GetMData(txcfg->phdr) &&
#ifdef A4_STA		
			((txcfg->pstat && txcfg->pstat->state & WIFI_A4_STA) 
	        #ifdef MCAST2UI_REFINE
	            ||(pskb && !IS_MCAST(&pskb->cb[10])))&& 
	        #else
				||!IS_MCAST((unsigned char *)pethhdr)) &&
	        #endif	
#else
#ifdef MCAST2UI_REFINE
	        (
#ifdef WDS
		(txcfg->pstat->state & WIFI_WDS)? (!IS_BCAST2((unsigned char *)pethhdr)) : 
#endif
			(pskb && !IS_MCAST(&pskb->cb[10])))&&                
#else
	        (
#ifdef WDS
	        (txcfg->pstat->state & WIFI_WDS)? (!IS_BCAST2((unsigned char *)pethhdr)) : 
#endif  
	        (!IS_MCAST((unsigned char *)pethhdr))) &&
#endif
#endif
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
			(txcfg->aggre_en <= FG_AGGRE_MSDU_FIRST)
#else
			(txcfg->aggre_en < FG_AGGRE_MSDU_FIRST)
#endif
			)
		{
			int i = get_tx_sc_index(priv, txcfg->pstat, (unsigned char *)pethhdr, txcfg->aggre_en);
			if (i >= 0) {		
				memcpy(&txcfg->pstat->tx_sc_ent[i].txcfg, txcfg, sizeof(struct tx_insn));
				memcpy((void *)&txcfg->pstat->tx_sc_ent[i].wlanhdr, txcfg->phdr, sizeof(struct wlanllc_hdr));
			}
		}
		else {
			if (txcfg->pstat && pethhdr 
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
				&& txcfg->aggre_en <= FG_AGGRE_MSDU_FIRST
#endif
				) {
				int i = get_tx_sc_index(priv, txcfg->pstat, (unsigned char *)pethhdr, txcfg->aggre_en);
				if (i >= 0) {
					txcfg->pstat->tx_sc_ent[i].txcfg.fr_len = 0;
#ifdef TX_SCATTER
					txcfg->pstat->tx_sc_ent[i].has_desc3 = 0;
#endif
				}
			}
		}
#endif
	}
#endif // CONFIG_PCI_HCI

	return SUCCESS;
}

#ifdef CONFIG_PCI_HCI
int rtl8192cd_firetx_pci(struct rtl8192cd_priv *priv, struct tx_insn* txcfg)
{
#if defined(RX_TASKLET) || defined(TAROKO_0)
	unsigned long x;
#ifdef SMP_SYNC
	int locked=0;	
#endif
	int ret;

	SAVE_INT_AND_CLI(x);
#ifdef SMP_SYNC				
	SMP_TRY_LOCK_XMIT(x,locked);
#endif

	ret = __rtl8192cd_firetx(priv, txcfg);

#ifdef SMP_SYNC				
	if(locked)
		SMP_UNLOCK_XMIT(x);
#endif
	RESTORE_INT(x);
	return ret;
#else
	return (__rtl8192cd_firetx(priv, txcfg));
#endif
}
#endif // CONFIG_PCI_HCI


#ifdef GBWC
/*static*/ int rtl8192cd_tx_gbwc(struct rtl8192cd_priv *priv, struct stat_info	*pstat, struct sk_buff *skb)
{
	if (((priv->pmib->gbwcEntry.GBWCMode == GBWC_MODE_LIMIT_MAC_INNER) && (pstat->GBWC_in_group)) ||
		((priv->pmib->gbwcEntry.GBWCMode == GBWC_MODE_LIMIT_MAC_OUTTER) && !(pstat->GBWC_in_group)) ||
		(priv->pmib->gbwcEntry.GBWCMode == GBWC_MODE_LIMIT_IF_TX) ||
		(priv->pmib->gbwcEntry.GBWCMode == GBWC_MODE_LIMIT_IF_TRX)) {
		if ((priv->GBWC_tx_count + skb->len) > ((priv->pmib->gbwcEntry.GBWCThrd_tx * 1024 / 8) / (1000/GBWC_TO_MILISECS))) {
			// over the bandwidth
			if (priv->GBWC_consuming_Q) {
				// in rtl8192cd_GBWC_timer context
				priv->ext_stats.tx_drops++;
				DEBUG_ERR("TX DROP: BWC bandwidth over!\n");
				rtl_kfree_skb(priv, skb, _SKB_TX_);
			}
			else {
				// normal Tx path
				int ret = enque(priv, 
#if defined(AP_SWPS_OFFLOAD)
                        NULL,
#endif				
				        &(priv->GBWC_tx_queue.head), &(priv->GBWC_tx_queue.tail),
						(unsigned long)(priv->GBWC_tx_queue.pSkb), NUM_TXPKT_QUEUE, (void *)skb);
				if (ret == FALSE) {
					priv->ext_stats.tx_drops++;
					DEBUG_ERR("TX DROP: BWC tx queue full!\n");
					rtl_kfree_skb(priv, skb, _SKB_TX_);
				}
			}
			goto stop_proc;
		}
		else {
			// not over the bandwidth
			if (CIRC_CNT(priv->GBWC_tx_queue.head, priv->GBWC_tx_queue.tail, NUM_TXPKT_QUEUE) &&
					!priv->GBWC_consuming_Q) {
				// there are already packets in queue, put in queue too for order
				int ret = enque(priv, 
#if defined(AP_SWPS_OFFLOAD)
                        NULL,
#endif				
				        &(priv->GBWC_tx_queue.head), &(priv->GBWC_tx_queue.tail),
						(unsigned long)(priv->GBWC_tx_queue.pSkb), NUM_TXPKT_QUEUE, (void *)skb);
				if (ret == FALSE) {
					priv->ext_stats.tx_drops++;
					DEBUG_ERR("TX DROP: BWC tx queue full!\n");
					rtl_kfree_skb(priv, skb, _SKB_TX_);
				}
				goto stop_proc;
			}
			else {
				// can transmit directly
				priv->GBWC_tx_count += skb->len;
			}
		}
	}

	/* Reply caller function : Continue process */
	return TX_PROCEDURE_CTRL_CONTINUE;

stop_proc:
	/* Reply caller function : STOP process */
	return TX_PROCEDURE_CTRL_STOP;
}
#endif


int dz_queue_pci(struct rtl8192cd_priv *priv, struct stat_info *pstat, struct sk_buff *pskb)
{
	unsigned int ret;
//TODO, power saving
#if defined(TAROKO_0)
#else
	if (pstat)
	{
		if(0 == pstat->expire_to)
			return FALSE;
#if defined(WIFI_WMM) && defined(WMM_APSD)
		if ((QOS_ENABLE) && (APSD_ENABLE) && (pstat->QosEnabled) && (pstat->apsd_bitmap & 0x0f)) {
			int pri = 0;

			pri = pskb->cb[_SKB_CB_PRIORITY];

			if (((pri == 7) || (pri == 6)) && (pstat->apsd_bitmap & 0x01)) {
				ret = enque(priv, 
#if defined(AP_SWPS_OFFLOAD)
                    pstat->VO_dz_queue,
#endif                    
                    &(pstat->VO_dz_queue->head), &(pstat->VO_dz_queue->tail),
					(unsigned long)(pstat->VO_dz_queue->pSkb), NUM_APSD_TXPKT_QUEUE, (void *)pskb);
				if (ret)
					DEBUG_INFO("enque VO pkt\n");
			}
			else if (((pri == 5) || (pri == 4)) && (pstat->apsd_bitmap & 0x02)) {
				ret = enque(priv, 
#if defined(AP_SWPS_OFFLOAD)
                    pstat->VI_dz_queue,
#endif                                        
                    &(pstat->VI_dz_queue->head), &(pstat->VI_dz_queue->tail),
					(unsigned long)(pstat->VI_dz_queue->pSkb), NUM_APSD_TXPKT_QUEUE, (void *)pskb);
				if (ret)
					DEBUG_INFO("enque VI pkt\n");
			}
			else if (((pri == 0) || (pri == 3)) && (pstat->apsd_bitmap & 0x08)) {
				ret = enque(priv, 
#if defined(AP_SWPS_OFFLOAD)
                    pstat->BE_dz_queue,
#endif                                        
                    &(pstat->BE_dz_queue->head), &(pstat->BE_dz_queue->tail),
					(unsigned long)(pstat->BE_dz_queue->pSkb), NUM_APSD_TXPKT_QUEUE, (void *)pskb);
				if (ret)
					DEBUG_INFO("enque BE pkt\n");
			}
			else if (pstat->apsd_bitmap & 0x04) {
				ret = enque(priv, 
#if defined(AP_SWPS_OFFLOAD)
                    pstat->BK_dz_queue,
#endif                                        
                    &(pstat->BK_dz_queue->head), &(pstat->BK_dz_queue->tail),
					(unsigned long)(pstat->BK_dz_queue->pSkb), NUM_APSD_TXPKT_QUEUE, (void *)pskb);
				if (ret)
					DEBUG_INFO("enque BK pkt\n");
			}
			else
				goto legacy_ps;

			if (!pstat->apsd_pkt_buffering)
				pstat->apsd_pkt_buffering = 1;

			if (ret == FALSE) {
				DEBUG_ERR("sleep Q full for priority = %d!\n", pri);
				return CONGESTED;
			}
			return TRUE;
		}
		else
legacy_ps:
#endif
		if (pstat->dz_queue.qlen<NUM_TXPKT_QUEUE){
			skb_queue_tail(&pstat->dz_queue, pskb);
#if defined(AP_SWPS_OFFLOAD)
            if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv)) {
                struct dz_seq_node* tmpnode;
                tmpnode = kmalloc(sizeof(struct dz_seq_node),GFP_ATOMIC);
                tmpnode->swps_seq = 0;
                list_add_tail(&tmpnode->node,&pstat->dz_list_head);
            }
#endif
            
			return TRUE;
		}
	}
	else {	// Multicast or Broadcast
#if defined(TAROKO_0)
		printk("ERROR MCAST PACKET\r\n");
#else
		ret = enque(priv, 
#if defined(AP_SWPS_OFFLOAD)
            NULL,
#endif            
            &(priv->dz_queue.head), &(priv->dz_queue.tail),
			(unsigned long)(priv->dz_queue.pSkb), NUM_TXPKT_QUEUE, (void *)pskb);
		if (ret == TRUE) {
		        if (!priv->pkt_in_dtimQ)
			        priv->pkt_in_dtimQ = 1;
		        return TRUE;
                }
        else
            priv->mcast_drop++;
#endif
	}
#endif
	return FALSE;
}


#if(CONFIG_WLAN_NOT_HAL_EXIST==1)
#if (!defined(__OSK__)) || (defined(__OSK__) && !defined(CONFIG_RTL6028))
__MIPS16
#endif
__IRAM_IN_865X
void rtl8192cd_signin_txdesc_shortcut(struct rtl8192cd_priv *priv, struct tx_insn *txcfg, int idx)
{
	struct tx_desc *phdesc, *pdesc, *pfrstdesc;
	struct tx_desc_info *pswdescinfo, *pdescinfo;
	struct rtl8192cd_hw	*phw;
	int *tx_head, q_num;
	struct stat_info *pstat;
	struct sk_buff *pskb;
	unsigned long pfrst_dma_desc;
	unsigned long *dma_txhead;
/*
#if defined(CONFIG_RTL_WAPI_SUPPORT)
	uint8				*wapiMic2;
	struct tx_desc		*pmicdesc;
	struct tx_desc_info	*pmicdescinfo;
#endif
*/
#ifdef TX_SCATTER
	int	go_desc3=0;
#endif
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
	if (GET_CHIP_VER(priv)== VERSION_8812E || GET_CHIP_VER(priv)== VERSION_8723B) {
		rtl8192cd_signin_txdesc_shortcut_8812(priv, txcfg, idx);
		return;
	}
#endif

	pstat = txcfg->pstat;
	pskb = (struct sk_buff *)txcfg->pframe;
	pfrst_dma_desc=0;

	phw	= GET_HW(priv);
	q_num = txcfg->q_num;

	dma_txhead	= get_txdma_addr(phw, q_num);
	tx_head		= get_txhead_addr(phw, q_num);
	phdesc		= get_txdesc(phw, q_num);
	pswdescinfo	= get_txdesc_info(priv,priv->pshare->pdesc_info, q_num);

	/*------------------------------------------------------------*/
	/*           fill descriptor of header + iv + llc             */
	/*------------------------------------------------------------*/
	pfrstdesc = pdesc = phdesc + *tx_head;
	pdescinfo = pswdescinfo + *tx_head;
   	
	desc_copy(pdesc, &pstat->tx_sc_ent[idx].hwdesc1);	
	assign_wlanseq(GET_HW(priv), txcfg->phdr, pstat, GET_MIB(priv)
#ifdef CONFIG_RTK_MESH	// For broadcast data frame via mesh (ex:ARP requst)
	, txcfg->is_11s
#endif
		);

			
	pdesc->Dword3 = 0;
	pdesc->Dword3 = set_desc((GetSequence(txcfg->phdr) & TX_SeqMask) << TX_SeqSHIFT);

#if defined(CONFIG_RTL_92D_SUPPORT) || defined(CONFIG_RTL_92C_SUPPORT)
	if (CHIP_VER_92X_SERIES(priv))
#endif
	if (priv->pmib->dot11RFEntry.txbf == 1) {
		pdesc->Dword2 &= set_desc(0x03ffffff); // clear related bits
		pdesc->Dword2 |= set_desc(1 << TX_TxAntCckSHIFT);	// Set Default CCK rate with 1T
		pdesc->Dword2 |= set_desc(1 << TX_TxAntlSHIFT); 	// Set Default Legacy rate with 1T
		pdesc->Dword2 |= set_desc(1 << TX_TxAntHtSHIFT);	// Set Default Ht rate

		if (is_MCS_rate(txcfg->tx_rate)) {
			if ((txcfg->tx_rate - HT_RATE_ID) <= 6){
					pdesc->Dword2 |= set_desc(3 << TX_TxAntHtSHIFT); // Set Ht rate < MCS6 with 2T
			}
		}
	}
	if(priv->pmib->dot11RFEntry.bcn2path){
		pdesc->Dword2 &= set_desc(0x03ffffff); // clear related bits
		pdesc->Dword2 |= set_desc(1 << TX_TxAntCckSHIFT);	// Set Default CCK rate with 1T
	}

//	if (txcfg->pstat)
//		pdesc->Dword1 |= set_desc(txcfg->pstat->cmn_info.aid & TX_MACIDMask);

	//set Break
	if((txcfg->q_num >=1 && txcfg->q_num <=4)){
		if((pstat != priv->pshare->CurPstat[txcfg->q_num-1])) {
#ifdef CONFIG_RTL_88E_SUPPORT
			if (GET_CHIP_VER(priv)==VERSION_8188E)
				pdesc->Dword2 |= set_desc(TXdesc_88E_BK);
			else
#endif
				pdesc->Dword1 |= set_desc(TX_BK);
			priv->pshare->CurPstat[txcfg->q_num-1] = pstat;
		} else
#ifdef CONFIG_RTL_88E_SUPPORT
		if (GET_CHIP_VER(priv)==VERSION_8188E) {
			pdesc->Dword2 &= set_desc(~TXdesc_88E_BK);
		} else
#endif
		{
			pdesc->Dword1 &= set_desc(~TX_BK); // clear it
		}
	} else {
#ifdef CONFIG_RTL_88E_SUPPORT
		if (GET_CHIP_VER(priv)==VERSION_8188E)
			pdesc->Dword2 |= set_desc(TXdesc_88E_BK);
		else
#endif
			pdesc->Dword1 |= set_desc(TX_BK);
	}


	if ((pstat->IOTPeer==HT_IOT_PEER_INTEL) && (pstat->retry_inc)) {
		if (is_MCS_rate(pstat->current_tx_rate) && !(pstat->leave)
			&& priv->pshare->intel_rty_lmt) {
			pdesc->Dword5 |= set_desc(TX_RtyLmtEn);
			
			pdesc->Dword5 &= set_desc(~(TX_DataRtyLmtMask << TX_DataRtyLmtSHIFT));
			pdesc->Dword5 |= set_desc((priv->pshare->intel_rty_lmt & TX_DataRtyLmtMask) << TX_DataRtyLmtSHIFT);
		} else {
			pdesc->Dword5 &= set_desc(~TX_RtyLmtEn);
			pdesc->Dword5 &= set_desc(~(TX_DataRtyLmtMask << TX_DataRtyLmtSHIFT));
		}
	}

#if defined(CONFIG_RTL_88E_SUPPORT) && defined(TXREPORT)
	if ((GET_CHIP_VER(priv)==VERSION_8188E) && (!txcfg->fixed_rate)) {
		if (pstat->ht_current_tx_info & TX_USE_SHORT_GI)
			pdesc->Dword5 |= set_desc(TX_SGI);
		else
			pdesc->Dword5 &= set_desc(~TX_SGI);
	}
#endif

	if (txcfg->one_txdesc) {
#ifdef TX_EARLY_MODE
		if (GET_TX_EARLY_MODE) {
			pdesc->Dword0 = set_desc(((get_desc(pdesc->Dword0) & 0xff00ffff) |(0x28 << TX_OffsetSHIFT)) |
									TX_LastSeg | 	(txcfg->hdr_len + txcfg->llc + txcfg->iv + txcfg->fr_len));
			pdesc->Dword1 = set_desc(get_desc(pdesc->Dword1) | (1 << TX_PktOffsetSHIFT) );
			pdesc->Dword7 = set_desc((get_desc(pdesc->Dword7) & 0xffff0000) |
						(txcfg->hdr_len + txcfg->llc + txcfg->iv + txcfg->fr_len+8));
#ifdef CONFIG_RTL_88E_SUPPORT
			if (GET_CHIP_VER(priv) == VERSION_8188E) {
				pdesc->Dword6 &= set_desc(~ (0xf << TX_MaxAggNumSHIFT));
				pdesc->Dword6 |= set_desc(0xf << TX_MaxAggNumSHIFT);
			}
#endif
			memset(txcfg->phdr-8, '\0', 8);			
			if (pstat->empkt_num > 0) 				
				insert_emcontent(priv, txcfg, txcfg->phdr-8);
			pdesc->Dword8 = set_desc(get_physical_addr(priv, txcfg->phdr-8,
				(get_desc(pdesc->Dword7)&TX_TxBufSizeMask), PCI_DMA_TODEVICE));
					
		}
		else
#endif
		{	
#if defined(TX_EARLY_MODE) && defined(CONFIG_RTL_88E_SUPPORT)
			if (GET_CHIP_VER(priv) == VERSION_8188E)
				pdesc->Dword6 &= set_desc(~ (0x0f << TX_MaxAggNumSHIFT));
#endif
			pdesc->Dword0 = set_desc((get_desc(pdesc->Dword0) & 0xffff0000) |
				TX_LastSeg | (txcfg->hdr_len + txcfg->llc + txcfg->iv + txcfg->fr_len));
			pdesc->Dword7 = set_desc((get_desc(pdesc->Dword7) & 0xffff0000) |
				(txcfg->hdr_len + txcfg->llc + txcfg->iv + txcfg->fr_len));
		}
	}

	if(priv->pshare->rf_ft_var.txforce != 0xff)	{
		pdesc->Dword5 &= set_desc(~(TX_DataRateMask << TX_DataRateSHIFT));
		pdesc->Dword4 |= set_desc(TX_UseRate);
		pdesc->Dword5 |= set_desc((priv->pshare->rf_ft_var.txforce & TX_DataRateMask) << TX_DataRateSHIFT);
	}

#ifdef TX_EARLY_MODE
	if (GET_TX_EARLY_MODE) 
		pdesc->Dword8 = set_desc(get_physical_addr(priv, txcfg->phdr-8,
			(get_desc(pdesc->Dword7)& TX_TxBufSizeMask), PCI_DMA_TODEVICE));
	else
#endif
	pdesc->Dword8 = set_desc(get_physical_addr(priv, txcfg->phdr,
		(get_desc(pdesc->Dword7)& TX_TxBufSizeMask), PCI_DMA_TODEVICE));

	descinfo_copy(pdescinfo, &pstat->tx_sc_ent[idx].swdesc1);
#ifndef TXDESC_INFO
	pdescinfo->paddr  = get_desc(pdesc->Dword8); // buffer addr
#endif	
	if (txcfg->one_txdesc) {
		pdescinfo->type = _SKB_FRAME_TYPE_;
		pdescinfo->pframe = pskb;
#if defined(ENABLE_RTL_SKB_STATS) || defined(RESERVE_TXDESC_FOR_EACH_IF)
		pdescinfo->priv = priv;
#endif
#if defined(WIFI_WMM) && defined(WMM_APSD)
#ifndef TXDESC_INFO
		pdescinfo->pstat = pstat;
#endif		
#endif
	}
	else {
		pdescinfo->pframe = txcfg->phdr;
#if defined(WIFI_WMM) && defined(WMM_APSD)
#if defined(ENABLE_RTL_SKB_STATS) || defined(RESERVE_TXDESC_FOR_EACH_IF)
		pdescinfo->priv = priv;
#endif
#ifndef TXDESC_INFO		
		pdescinfo->pstat = pstat;
#endif		
#endif
	}

#ifdef CLIENT_MODE
	if (OPMODE & WIFI_STATION_STATE) {
		if (GetFrameSubType(pdescinfo->pframe) == WIFI_PSPOLL)
			pdesc->Dword1 |= set_desc(TX_NAVUSEHDR);

		if (priv->ps_state)
			SetPwrMgt(pdescinfo->pframe);
		else
			ClearPwrMgt(pdescinfo->pframe);
	}
#endif

	pfrst_dma_desc = dma_txhead[*tx_head];
/*
#ifdef USE_RTL8186_SDK
	rtl_cache_sync_wback(priv, get_desc(pdesc->Dword8), (get_desc(pdesc->Dword7)&TX_TxBufferSizeMask), PCI_DMA_TODEVICE);
#endif
*/
#ifdef OSK_LOW_TX_DESC
	if (q_num!=BE_QUEUE && q_num!=HIGH_QUEUE)
		txdesc_rollover_nonbe(pdesc, (unsigned int *)tx_head);
	else
#endif
	txdesc_rollover(pdesc, (unsigned int *)tx_head);

	if (txcfg->one_txdesc)
		goto one_txdesc;

	/*------------------------------------------------------------*/
	/*              fill descriptor of frame body                 */
	/*------------------------------------------------------------*/
#ifdef TX_SCATTER
next_desc:
#endif	
	pdesc = phdesc + *tx_head;
	pdescinfo = pswdescinfo + *tx_head;
#ifdef TX_SCATTER
	if (go_desc3)
		desc_copy(pdesc, &pstat->tx_sc_ent[idx].hwdesc3);
	else
#endif
		desc_copy(pdesc, &pstat->tx_sc_ent[idx].hwdesc2);

#ifdef TX_SCATTER
	if (pskb->list_num > 1) {
		if (go_desc3)
			pdesc->Dword7 = set_desc((get_desc(pdesc->Dword7) & 0xffff0000) |
				pskb->list_buf[2].len);
		else
			pdesc->Dword7 = set_desc((get_desc(pdesc->Dword7) & 0xffff0000) |
				pskb->list_buf[1].len);
	}
#endif

	pdesc->Dword8 = set_desc(get_physical_addr(priv, pskb->data,
		(get_desc(pdesc->Dword7)&0x0fff), PCI_DMA_TODEVICE));

#ifdef TX_SCATTER
	if (go_desc3) {
		descinfo_copy(pdescinfo, &pstat->tx_sc_ent[idx].swdesc3);
		pdescinfo->type = _RESERVED_FRAME_TYPE_;
	} else
#endif
	{
		descinfo_copy(pdescinfo, &pstat->tx_sc_ent[idx].swdesc2);
	}
#ifndef TXDESC_INFO
	pdescinfo->paddr  = get_desc(pdesc->Dword8);
#endif	
	pdescinfo->pframe = pskb;
#if defined(ENABLE_RTL_SKB_STATS) || defined(RESERVE_TXDESC_FOR_EACH_IF)
	pdescinfo->priv = priv;
#endif
/*
#ifndef USE_RTL8186_SDK
	rtl_cache_sync_wback(priv, dma_txhead[*tx_head], sizeof(struct tx_desc), PCI_DMA_TODEVICE);
#else
	rtl_cache_sync_wback(priv, get_desc(pdesc->Dword8), (get_desc(pdesc->Dword7)&TX_TxBufferSizeMask), PCI_DMA_TODEVICE);
#endif
*/

#if defined(CONFIG_RTL_WAPI_SUPPORT)
	if (txcfg->privacy == _WAPI_SMS4_)
	{
		SecSWSMS4Encryption(priv, txcfg);
	}
#endif

#ifndef NOT_RTK_BSP
	if ((txcfg->privacy == _TKIP_PRIVACY_) &&
		(priv->pshare->have_hw_mic) &&
		!(priv->pmib->dot11StationConfigEntry.swTkipMic))
	{
		register unsigned long int l,r;
		unsigned char *mic;
		int delay = 18;

		while ((*(volatile unsigned int *)GDMAISR & GDMA_COMPIP) == 0) {
			delay_us(delay);
			delay = delay / 2;
		}

		l = *(volatile unsigned int *)GDMAICVL;
		r = *(volatile unsigned int *)GDMAICVR;

		mic = ((struct sk_buff *)txcfg->pframe)->data + txcfg->fr_len - 8;
		mic[0] = (unsigned char)(l & 0xff);
		mic[1] = (unsigned char)((l >> 8) & 0xff);
		mic[2] = (unsigned char)((l >> 16) & 0xff);
		mic[3] = (unsigned char)((l >> 24) & 0xff);
		mic[4] = (unsigned char)(r & 0xff);
		mic[5] = (unsigned char)((r >> 8) & 0xff);
		mic[6] = (unsigned char)((r >> 16) & 0xff);
		mic[7] = (unsigned char)((r >> 24) & 0xff);

#ifdef MICERR_TEST
		if (priv->micerr_flag) {
			mic[7] ^= mic[7];
			priv->micerr_flag = 0;
		}
#endif
	}
#endif // NOT_RTK_BSP

#ifndef USE_RTL8186_SDK
	rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(dma_txhead[*tx_head]-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(struct tx_desc), PCI_DMA_TODEVICE);
#endif
	rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(get_desc(pdesc->Dword8)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), (get_desc(pdesc->Dword7)&TX_TxBufSizeMask), PCI_DMA_TODEVICE);

#ifdef OSK_LOW_TX_DESC
	if (q_num!=BE_QUEUE && q_num!=HIGH_QUEUE)
		txdesc_rollover_nonbe(pdesc, (unsigned int *)tx_head);
	else
#endif
	txdesc_rollover(pdesc, (unsigned int *)tx_head);

#ifdef TX_SCATTER
	if (pstat->tx_sc_ent[idx].has_desc3 && go_desc3 == 0) {
		go_desc3 = 1;
		goto next_desc;
	}
#endif
one_txdesc:

	rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(get_desc(pfrstdesc->Dword8)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), (get_desc(pfrstdesc->Dword7)&TX_TxBufSizeMask), PCI_DMA_TODEVICE);

#ifdef SUPPORT_SNMP_MIB
	if (txcfg->rts_thrshld <= get_mpdu_len(txcfg, txcfg->fr_len))
		SNMP_MIB_INC(dot11RTSSuccessCount, 1);
#endif

	pfrstdesc->Dword0 |= set_desc(TX_OWN);

#ifndef USE_RTL8186_SDK
	rtl_cache_sync_wback(priv, (unsigned long)bus_to_virt(pfrst_dma_desc-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(struct tx_desc), PCI_DMA_TODEVICE);
#endif

#ifdef RESERVE_TXDESC_FOR_EACH_IF
	if (RSVQ_ENABLE && IS_RSVQ(q_num))
		priv->use_txdesc_cnt[RSVQ(q_num)] += (txcfg->one_txdesc)? 1 : 2;
#endif

	if (q_num == HIGH_QUEUE) {
		DEBUG_WARN("signin shortcut for DTIM pkt?\n");
		return;
	} else {
		tx_poll(priv, q_num);
	}

	return;
}
#else
#if !defined(__OSK__) && !defined(CONFIG_RTL6028)
__MIPS16
#endif
__IRAM_IN_865X
void rtl8192cd_signin_txdesc_shortcut(struct rtl8192cd_priv *priv, struct tx_insn *txcfg, int idx)
{
    if (net_ratelimit()) {
        printk("==> %s:%d, do nothing\n", __func__, __LINE__);
    }
	return;
}
#endif//CONFIG_WLAN_NOT_HAL_EXIST

#ifdef THERMAL_CONTROL
#ifdef RTK_STA_BWC
static int rtl8192cd_tx_sta_bwc(struct rtl8192cd_priv *priv, struct stat_info	*pstat, struct sk_buff *skb)
{
	int TP_thd = (((pstat->sta_bwcthrd_tx*1024)/8)/1000)*priv->pshare->rf_ft_var.sta_bwc_to;
//	if(priv->pshare->rf_ft_var.swq_dbg == 200)
//		TP_thd = (((pstat->sta_bwcthrd_tx*1024)/8)/1000)*priv->pshare->rf_ft_var.sta_bwc_to;

	//current bps > bwc bps thrd	
	if ((pstat->sta_bwctx_cnt+skb->len) > TP_thd) 
	{
		// over the bandwidth
		// drop packet
		priv->ext_stats.tx_drops++;
		pstat->sta_bwcdrop_cnt++;
		DEBUG_ERR("TX DROP: BWC bandwidth over!\n");
		rtl_kfree_skb(priv, skb, _SKB_TX_);
		goto stop_proc;
	}
	else 
	{
		// can transmit directly
		pstat->sta_bwctx_cnt += skb->len;
	}
	/* Reply caller function : Continue process */
	return TX_PROCEDURE_CTRL_CONTINUE;

stop_proc:
	/* Reply caller function : STOP process */
	return TX_PROCEDURE_CTRL_STOP;
}
#endif

#endif


#if (!defined(__OSK__)) || (defined(__OSK__) && !defined(CONFIG_RTL6028))
__MIPS16
#endif
__IRAM_IN_865X
int __rtl8192cd_start_xmit_out(struct sk_buff *skb, struct stat_info *pstat, struct tx_insn *ptxcfg)
{
    struct rtl8192cd_priv *priv;
    //struct stat_info        *pstat=NULL;
    struct net_device *dev = skb->dev;
    struct sk_buff *newskb = NULL;
    struct net_device *wdsDev = NULL;
#ifdef CONFIG_RTK_MESH
    int block_mesh = 0;
#endif
#if defined(CONFIG_RTL8672) || defined(TX_SHORTCUT)
    int k;
#endif
#ifdef TXSC_HDR
	int txsc_lv2 = 0;
#endif	
#if defined(TX_SHORTCUT)
	struct tx_sc_entry *ptxsc_entry = NULL;
	struct tx_insn		*txsc_txcfg;
#endif
    struct tx_insn tx_insn;
	int needTxBD=4;
    DECLARE_TXCFG(txcfg, tx_insn);

	priv = GET_DEV_PRIV(dev);

#ifdef TX_SKB_REFINE
	if (skb->cb[33] == 0x11)
	{
		txcfg->is_test = 1;
	}
#endif
	
#if defined(CONFIG_RTK_MESH)
	if(dev == priv->mesh_dev) {
		if(ptxcfg->is_11s == 1) {
			memcpy(txcfg, ptxcfg, sizeof(*ptxcfg));
            priv = txcfg->priv;
        }    
		else { /* if ptxcfg->is_11s is RELAY_11S */
            txcfg->is_11s = RELAY_11S;                            
            memcpy(txcfg->nhop_11s, pstat->cmn_info.mac_addr, MACADDRLEN); 
            memcpy(&txcfg->mesh_header, skb->data + WLAN_ETHHDR_LEN, sizeof(struct lls_mesh_header));       
            priv = ptxcfg->priv;
       }    
    }    
#endif

#if defined(CONFIG_TRIBAND_MESH)
if (GET_HCI_TYPE(priv) == RTL_HCI_USB && txcfg->is_11s) {
    txcfg->fr_type = _SKB_FRAME_TYPE_;
    txcfg->pframe = skb;
    txcfg->pstat = pstat;
    if (update_txinsn_stage1(priv, txcfg) == FALSE) {
        priv->ext_stats.tx_drops++;
        goto free_and_stop;
    }
}
#endif

#ifdef CONFIG_RTL8672
#ifdef SUPPORT_TX_MCAST2UNI
	if (skb->cb[16] == TX_NO_MUL2UNI)
		txcfg->isMC2UC = 1;
	else
		txcfg->isMC2UC = 0;
#endif
#endif

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

#ifdef SUPPORT_SNMP_MIB
	if (IS_MCAST(skb->data))
		SNMP_MIB_INC(dot11MulticastTransmittedFrameCount, 1);
#endif

#if 0//defined(AP_SWPS_OFFLOAD)
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv)) {
        if(pstat){
            pstat->SWPS_pkt_Qos = GetPktQoStype(priv,pstat,skb);
            if(pstat->SWPS_pkt_Qos == -1)
                printk("[%s][%d]Error, find no pkt Qos, =%d\n",__FUNCTION__,__LINE__, pstat->SWPS_pkt_Qos);
        }
    }
#endif


	if ((OPMODE & WIFI_AP_STATE)
#ifdef WDS
			&& (!wdsDev)
#endif
#ifdef CONFIG_RTK_MESH
			&& (dev != priv->mesh_dev)
#endif
#ifdef SUPPORT_TX_AMSDU_SHORTCUT //amsdu shortcut do not into dz_queue
			&& (!skb->cb[_SKB_CB_AMSDU_TXSC])
#endif

		) {
#ifdef MCAST2UI_REFINE
                if ((pstat && ((pstat->state & (WIFI_SLEEP_STATE | WIFI_ASOC_STATE)) ==
                                (WIFI_SLEEP_STATE | WIFI_ASOC_STATE))) ||
                        (((IS_MCAST(&skb->cb[10]) && (priv->sleep_list.next != &priv->sleep_list)) ||
                        ((priv->pshare->rf_ft_var.bcast_to_dzq) && ((unsigned char )(skb->cb[10]) == 0xff))) 
#if !defined(TAROKO_0)
                        &&(!priv->release_mcast)
#endif
                                ))
#else
		if ((pstat && ((pstat->state & (WIFI_SLEEP_STATE | WIFI_ASOC_STATE)) ==
				(WIFI_SLEEP_STATE | WIFI_ASOC_STATE))) ||
			(((IS_MCAST(skb->data) && (priv->sleep_list.next != &priv->sleep_list)) ||
			(priv->pshare->rf_ft_var.bcast_to_dzq && (*(skb->data) == 0xff))) 
#if !defined(TAROKO_0)
			&&(!priv->release_mcast)
#endif
			))
#endif
		{

			if (dz_queue(priv, pstat, skb) == TRUE)
			{
				DEBUG_INFO("queue up skb due to sleep mode\n");
				goto stop_proc;
			}
			else {
				if (pstat) {
					DEBUG_WARN("ucst sleep queue full!!\n");
				}
				else {
					DEBUG_WARN("mcst sleep queue full!!\n");
				}
				goto free_and_stop;
			}
		}
	}

#if defined(GBWC) || defined(SBWC)
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
#endif

#ifdef RTK_STA_BWC
	if (priv->pshare->rf_ft_var.sta_bwc_en && pstat && pstat->sta_bwcthrd_tx>0) {
		if (rtl8192cd_tx_sta_bwc(priv, pstat, skb) == TX_PROCEDURE_CTRL_STOP) {
			goto stop_proc;
		}
	}
#endif

	if (pstat) {
#ifdef CONFIG_RTK_MESH
		if (dev == priv->mesh_dev) {
			int priority;
			priority = get_skb_priority(priv, skb, pstat, txcfg->is_11s);
			skb->cb[_SKB_CB_QNUM] = pri_to_qnum(priv, priority);
		}
#endif
		txcfg->q_num = skb->cb[_SKB_CB_QNUM];
	}

#ifdef MCR_WIRELESS_EXTEND	
	if ((IS_UDP_PROTO(skb->data) && pstat) && (priv->pshare->rf_ft_var.dropUDP))
	{
		int pri = skb->cb[_SKB_CB_PRIORITY];
		int q_num = skb->cb[_SKB_CB_QNUM];

		if (GET_CHIP_VER(priv)==VERSION_8814A) {
				if (priv->pshare->phw->VO_pkt_count && (priv->pshare->phw->VI_pkt_count || priv->pshare->phw->BE_pkt_count || priv->pshare->phw->BK_pkt_count)) {
					if ((q_num == VI_QUEUE) && (priv->pshare->phw->VI_droppkt_count < (priv->pshare->phw->VO_droppkt_count<<1))) {
						priv->pshare->phw->VI_droppkt_count++;
						goto free_and_stop; 	
					}
					else if ((q_num == BK_QUEUE) && (priv->pshare->phw->BK_droppkt_count < (priv->pshare->phw->VO_droppkt_count<<2))) {
						priv->pshare->phw->BK_droppkt_count++;
						goto free_and_stop;		
					}
					else if ((q_num == BE_QUEUE) && (priv->pshare->phw->BE_droppkt_count < (priv->pshare->phw->VO_droppkt_count<<2))) {
						priv->pshare->phw->BE_droppkt_count++;
						goto free_and_stop;
					}
				} else if (priv->pshare->phw->VI_pkt_count && (priv->pshare->phw->BE_pkt_count || priv->pshare->phw->BK_pkt_count)) {
					if ((q_num == BK_QUEUE) && (priv->pshare->phw->BK_droppkt_count < (priv->pshare->phw->VI_droppkt_count<<2))) {
						priv->pshare->phw->BK_droppkt_count++;
						goto free_and_stop;		
					}
					else if ((q_num == BE_QUEUE) && (priv->pshare->phw->BE_droppkt_count < (priv->pshare->phw->VI_droppkt_count<<1))) {
						priv->pshare->phw->BE_droppkt_count++;
						goto free_and_stop;
					}
				}

		}else if (GET_CHIP_VER(priv)==VERSION_8192E){

				if (priv->pshare->phw->VO_pkt_count && (priv->pshare->phw->VI_pkt_count || priv->pshare->phw->BE_pkt_count || priv->pshare->phw->BK_pkt_count)) {
					if ((q_num == VI_QUEUE) && (priv->pshare->phw->VI_droppkt_count < (priv->pshare->phw->VO_droppkt_count*3/2))) {
						priv->pshare->phw->VI_droppkt_count++;
						goto free_and_stop; 	
					}
					else if ((q_num == BK_QUEUE) && (priv->pshare->phw->BK_droppkt_count < (priv->pshare->phw->VO_droppkt_count<<1))) {
						priv->pshare->phw->BK_droppkt_count++;
						goto free_and_stop;		
					}
					else if ((q_num == BE_QUEUE) && (priv->pshare->phw->BE_droppkt_count < (priv->pshare->phw->VO_droppkt_count<<1))) {
						priv->pshare->phw->BE_droppkt_count++;
						goto free_and_stop;
					}
				} else if (priv->pshare->phw->VI_pkt_count && (priv->pshare->phw->BE_pkt_count || priv->pshare->phw->BK_pkt_count)) {
					if ((q_num == BK_QUEUE) && (priv->pshare->phw->BK_droppkt_count < (priv->pshare->phw->VI_droppkt_count<<1))) {
						priv->pshare->phw->BK_droppkt_count++;
						goto free_and_stop;		
					}
					else if ((q_num == BE_QUEUE) && (priv->pshare->phw->BE_droppkt_count < (priv->pshare->phw->VI_droppkt_count<<1))) {
						priv->pshare->phw->BE_droppkt_count++;
						goto free_and_stop;
					}
				}
			


			}

	}
#endif


#ifdef SUPPORT_TX_AMSDU
#if defined(CONFIG_RTK_MESH) && !defined(MESH_AMSDU)
	if(dev == priv->mesh_dev){
		block_mesh = 1;
    }
#endif

#ifdef SUPPORT_TX_AMSDU_SHORTCUT
	if (skb->cb[_SKB_CB_AMSDU_TXSC]) {
		// from amsdu_xmit, don't go to amsdu_check again
		txcfg->q_num = skb->cb[_SKB_CB_AMSDU_TXSC] & 15;
		txcfg->aggre_en = (skb->cb[_SKB_CB_AMSDU_TXSC] >> 4) & 0xf;
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
		if (OFFLOAD_ENABLE(priv))
			txcfg->h2d_type = TXPKTINFO_TYPE_AMSDU;
#endif
	} else 
#endif    
	if (IS_SUPPORT_TX_AMSDU(priv) 
#ifdef SUPPORT_TX_SWQ_AMSDU
		&& 0
#endif
#if defined(CONFIG_RTK_MESH)
		&& !block_mesh
#endif
		&& pstat && (pstat->amsdu_level > 0) &&
		((pstat->aggre_mthd & AGGRE_MTHD_MSDU)
		
#ifdef SUPPORT_TX_AMSDU_SMALL_PKTS_ONLY
		|| (pstat->aggre_mthd & AGGRE_MTHD_MPDU && 
			priv->pmib->dot11nConfigEntry.dot11nAMSDUSmallPkts && 
			(skb->len <= priv->pmib->dot11nConfigEntry.dot11nAMSDUSmallPktLen || ((skb->cb[_SKB_CB_FLAGS] & _SKB_CB_FLAGS_TCPACK) && pstat->amsdu_tcpack_en)))
#endif
		)
#ifdef SUPPORT_TX_MCAST2UNI
#ifdef CONFIG_WLAN_HAL_8814BE
		&& ((GET_CHIP_VER(priv) == VERSION_8814B)?(!IS_MCAST(skb->data) && (skb->cb[16]!=TX_NO_MUL2UNI)):(priv->pshare->rf_ft_var.mc2u_disable || (skb->cb[2] != (char)0xff)))
#else
		&& (priv->pshare->rf_ft_var.mc2u_disable || (skb->cb[2] != (char)0xff))
#endif
#endif
		) {
		int ret = amsdu_check(priv, skb, pstat, txcfg);

		if (ret == RET_AGGRE_ENQUE)
			goto stop_proc;

		if (ret == RET_AGGRE_DESC_FULL)
			goto free_and_stop;
	}
#endif


#ifdef SW_TX_QUEUE
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
#ifdef CONFIG_RTK_MESH
	    if(dev != priv->mesh_dev) /*mesh do not support sw tx queue yet, skip this*/
#endif        
	    {

	    	if ((priv->assoc_num > 1) && pstat)
	        {
			if(priv->pshare->record_pstat != pstat)

			{
				priv->pshare->swq_txmac_chg++;

				priv->pshare->record_pstat = pstat;

	           	}
	            else
	            {
	                int q_num = txcfg->q_num;
	                if (priv->pshare->record_qnum != q_num)
	                {
	                	priv->pshare->swq_txmac_chg++;
	                    priv->pshare->record_qnum = q_num;
	                }
	            }
	        }

#ifdef SW_TX_QUEUE_SMALL_PACKET_CHECK
	        // add check for small udp packet(88B) test with veriwave tool
	        if ((priv->pshare->swq_en == 0) && (priv->assoc_num > 1) && (AMPDU_ENABLE))
	        {
	            int thd_value;
#ifdef SMP_SYNC
	            unsigned long flags = 0;
	            int locked = 0;
#endif

	            if ((priv->swq_boost_delay > 0) && (priv->swq_boost_delay < 10))
	                thd_value = priv->pshare->rf_ft_var.swq_en_highthd / 10;
	            else
	                thd_value = priv->pshare->rf_ft_var.swq_en_highthd;

	            if (priv->pshare->swq_txmac_chg >= thd_value);
	            {
	                //printk("%s %d : thd_value %d\n", __FUNCTION__, __LINE__, thd_value);

	                if (priv->pshare->txop_enlarge == 0)
	                    priv->pshare->txop_enlarge = 2;

	                priv->pshare->swq_en = 1;
	                priv->pshare->swqen_keeptime = priv->up_time;

	                struct stat_info	*pstat_swq;
	                struct list_head	*phead, *plist;
	                struct stat_info	*pstat_highest = NULL;

	                phead = &priv->asoc_list;
#ifdef SMP_SYNC
					if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE)
	                	SMP_TRY_LOCK_ASOC_LIST(flags, locked);
#endif
	                
	                plist = phead->next;
	                while (plist != phead)
	                {
	                    pstat_swq = list_entry(plist, struct stat_info, asoc_list);

	                    int i;
	                    for (i = BK_QUEUE; i <= VO_QUEUE; i++) 
	                    {				
	                        pstat_swq->swq.q_aggnum[i] = priv->pshare->rf_ft_var.swq_aggnum; // MAX_BACKOFF_CNT;
	                    }

	                    if (plist == plist->next)
	                        break;

	                    plist = plist->next;
	                }
#ifdef SMP_SYNC					
					if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE && locked)
		        		SMP_UNLOCK_ASOC_LIST(flags);
#endif                
	            }
	        }
#endif

	    }
	}
#endif //SW_TX_QUEUE

#ifdef DYN_AMPDU_NUM
	if (priv->pmib->miscEntry.dyn_ampdu)
		MOV_AVG(priv->tx_pktlen_avg, skb->len, 100, 1);
#endif

	if (pstat) {
#ifdef CONFIG_IEEE80211W
	    txcfg->use_sw_enc = UseSwCrypto(priv, pstat, FALSE, 0) ;
#else
	    txcfg->use_sw_enc =	UseSwCrypto(priv, pstat, FALSE);
#endif
	}

#ifdef CONFIG_RTL_OFFLOAD_DRIVER
	if (OFFLOAD_ENABLE(priv) && 
		rtl8192cd_xmit_offload(pstat, skb, txcfg)==SUCCESS)
		return 0;
#endif

#ifdef TX_SHORTCUT
	if (!priv->pmib->dot11OperationEntry.disable_txsc && pstat
			&& ((k = get_tx_sc_index(priv, pstat, skb->data, txcfg->aggre_en)) >= 0)
#ifdef CONFIG_WLAN_HAL_8814BE
			&& (GET_CHIP_VER(priv) != VERSION_8814B)
#endif
			)
	{
		ptxsc_entry = &pstat->tx_sc_ent[k];
		txsc_txcfg = &(ptxsc_entry->txcfg);
		if (
#if !defined(__ECOS)
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
			(skb->cb[_SKB_CB_PRIORITY] == ptxsc_entry->pktpri) &&
			(FRAGTHRSLD > 1500) 
			)
		{
			int	q_num;
			unsigned int uiCurr_tx_desc = CURRENT_NUM_TX_DESC;
#ifdef TX_SCATTER
			int reuse_txdesc = 0;
#endif
			int bUnAvail = FALSE;
			int num_swq_rsvd = 0;
			unsigned int q_len = 0;

#ifdef OSK_LOW_TX_DESC
			uiCurr_tx_desc = ((q_num==BE_QUEUE || q_num==HIGH_QUEUE)?BE_TXDESC:NONBE_TXDESC);
#endif

#ifdef CONFIG_WLAN_HAL
			if (IS_HAL_CHIP(priv)) {
#if IS_EXIST_RTL8814BE && defined(CONFIG_PE_ENABLE)
                if( IS_HARDWARE_TYPE_8814B(priv)){
				    q_num = txsc_txcfg->q_num; //need check if its ok //yllinPEOFLD
                }
				else
#endif
                {   
                    q_num = GET_HAL_INTERFACE(priv)->MappingTxQueueHandler(priv, txsc_txcfg->q_num);
                }
				q_len = 32;
                
			} else
#endif
			{
	 			q_num = txsc_txcfg->q_num;
	 			q_len = 10;
			}
			// check if we need active tx tasklet
            if (getAvailableTXBD(priv, q_num) < (uiCurr_tx_desc/2))    {
#if (NUM_TX_DESC > 64)
				if (getAvailableTXBD(priv, q_num) < q_len) {
					rtl8192cd_tx_queueDsr(priv, q_num);
				} else 
#endif // #if (NUM_TX_DESC > 64)
				{
					if (!priv->pshare->has_triggered_tx_tasklet) {
#ifdef __KERNEL__
						tasklet_schedule(&priv->pshare->tx_tasklet);
#endif
						priv->pshare->has_triggered_tx_tasklet = 1;
					}
				}
			}


			if(priv->pshare->rf_ft_var.lmtdesc && IS_UDP_PROTO(skb->data)) {
				if (GET_CHIP_VER(priv)==VERSION_8814A) {				
					if (priv->pshare->phw->VO_pkt_count >= 50) {
						if(q_num == VI_QUEUE || q_num == BE_QUEUE || q_num == BK_QUEUE)
							needTxBD += priv->pshare->rf_ft_var.lmt1;		//768		
						if(q_num == BE_QUEUE || q_num == BK_QUEUE)
							needTxBD += priv->pshare->rf_ft_var.lmt2;		//192
						if(q_num == BK_QUEUE)
							needTxBD += priv->pshare->rf_ft_var.lmt2;		// 192
					}
					else if (priv->pshare->phw->VI_pkt_count >= 50) {
						if(q_num == BE_QUEUE || q_num == BK_QUEUE)
							needTxBD += priv->pshare->rf_ft_var.lmt1;		
						if(q_num == BK_QUEUE)
							needTxBD += priv->pshare->rf_ft_var.lmt2;		
					} else if (priv->pshare->phw->BE_pkt_count >= 50) {
						if(q_num == BK_QUEUE)
							needTxBD += priv->pshare->rf_ft_var.lmt1;		
					}
				} else {
					if (priv->pshare->phw->VO_pkt_count >= 50) {
						if(q_num == VI_QUEUE || q_num == BE_QUEUE || q_num == BK_QUEUE)
							needTxBD += priv->pshare->rf_ft_var.lmt1;		//768
						if(q_num == BE_QUEUE || q_num == BK_QUEUE)
							needTxBD += priv->pshare->rf_ft_var.lmt2;		//192
#if 1							
						if(q_num == BK_QUEUE)
							needTxBD += priv->pshare->rf_ft_var.lmt2;		// 192
#endif							
					}
					else if (priv->pshare->phw->VI_pkt_count >= 50) {
						if(q_num == BE_QUEUE || q_num == BK_QUEUE)
							needTxBD += priv->pshare->rf_ft_var.lmt1;		
#if 1							
						if(q_num == BK_QUEUE)
							needTxBD += priv->pshare->rf_ft_var.lmt2;		
#endif							
					} else if (priv->pshare->phw->BE_pkt_count >= 50) {
						if(q_num == BK_QUEUE)
							needTxBD += priv->pshare->rf_ft_var.lmt3;
					}
				}
			}
				
			// Check if we need to reclaim TX-ring before processing TX
#if (NUM_TX_DESC <= 64)
#ifdef CONFIG_WLAN_HAL
			if (IS_HAL_CHIP(priv)) {
				if (getAvailableTXBD(priv, q_num) < (needTxBD+28))
					rtl8192cd_tx_queueDsr(priv, q_num);
			} else
#endif
			{
				if (getAvailableTXBD(priv, q_num) < 10)
					rtl8192cd_tx_queueDsr(priv, q_num);
			}
#endif

#ifdef SW_TXQ_RSVD_DESC
			{
				int pri_t = ptxsc_entry->pktpri;
				int q_num_t = ptxsc_entry->txcfg.q_num;
				if (q_num_t>=BK_QUEUE && q_num_t<=VO_QUEUE)
				{
					if (!pstat->ADDBA_ready[pri_t])
					{
						int max_queue_cnt = CURRENT_NUM_TX_DESC >> 3;
						if (priv->pshare->swq_skb_queue_cnt[q_num_t - 1] >= max_queue_cnt)
							num_swq_rsvd = max_queue_cnt;
						else
							num_swq_rsvd = priv->pshare->swq_skb_queue_cnt[q_num_t - 1];

						num_swq_rsvd = num_swq_rsvd >> 1;
					}
				}
			}
#endif

#ifdef RESERVE_TXDESC_FOR_EACH_IF
			if ( RSVQ_ENABLE && IS_RSVQ(q_num) ) {
				q_len = 
#ifdef CONFIG_WLAN_HAL
					IS_HAL_CHIP(priv) ? 1 :
#endif
					2;
				bUnAvail = check_rsv_txdesc(priv, q_num, q_len);
			} else
#endif
			{
				int num_rsvd = 
#ifdef TX_EARLY_MODE
					( GET_TX_EARLY_MODE && GET_EM_SWQ_ENABLE ) ? 14 :
#endif
					4;

#ifdef CONFIG_WLAN_HAL
				if (IS_HAL_CHIP(priv)) {
#ifdef TX_EARLY_MODE
					if (GET_TX_EARLY_MODE && GET_EM_SWQ_ENABLE) {
						q_len = num_rsvd + num_swq_rsvd;
					} else
#endif
					{
						q_len = needTxBD + num_swq_rsvd;
					}

					bUnAvail = (getAvailableTXBD(priv, q_num) < q_len);
				} else
#endif
				{
#ifdef TX_SCATTER
					q_len = 6;
					bUnAvail = 					
						((ptxsc_entry->has_desc3 && getAvailableTXBD(priv, q_num) < 6) ||
						(!ptxsc_entry->has_desc3 && getAvailableTXBD(priv, q_num) < 4));
#else
					q_len = num_rsvd + num_swq_rsvd;
					bUnAvail = (getAvailableTXBD(priv, q_num) < q_len);
#endif
				}
			}

			if (bUnAvail) {
				DEBUG_WARN("%s:%d: tx drop: hw Queue(%d) desc not available! request %d\n", __FUNCTION__, __LINE__, q_num, q_len);
					rtl8192cd_tx_xmitSkbFail(priv, skb, dev, wdsDev, txcfg);
				goto stop_proc;
			}


#if defined(MESH_TX_SHORTCUT)
			if(txcfg->is_11s) {
                if(!mesh_txsc_decision(txcfg, &ptxsc_entry->txcfg)) { /* compare addr 5 and 6 */
                    goto just_skip;
                }

                if(memcmp(skb->data, GetAddr3Ptr(&ptxsc_entry->wlanhdr), MACADDRLEN)) { /* compare addr 3 */
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

#ifdef SUPPORT_TX_AMSDU_SHORTCUT
			if((txcfg->aggre_en == FG_AGGRE_MSDU_MIDDLE)||(txcfg->aggre_en == FG_AGGRE_MSDU_LAST))
			{
				//it doesn't need all backup txcfg for middle and last of AMSDU, set which it needed only
				txcfg->privacy = txsc_txcfg->privacy;
			}
			else
#endif
			{	
#if !defined(TXSC_CFG)
				memcpy(txcfg, txsc_txcfg, sizeof(struct tx_insn));
#else
				txcfg = txsc_txcfg;
				txcfg->one_txdesc = 0;
#endif
			}
			

#ifdef CONFIG_RTL8672
#ifdef SUPPORT_TX_MCAST2UNI
			if (skb->cb[16] == TX_NO_MUL2UNI)
				txcfg->isMC2UC = 1;
			else
				txcfg->isMC2UC = 0;
#endif
#endif

#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
			if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(priv)) {
				txcfg->phdr   	= skb->data; 	// 802.3 hdr
				//txcfg->hdr_len	= WLAN_ETHHDR_LEN;
				txcfg->pframe 	= skb;  		// skb->data:  802.3 hdr + payload
				txcfg->fr_len 	= skb->len; 	// len(802.3 hdr + payload)
				// TODO: currently , we don't care encryption.... it's only support AES in HW
			    #ifdef SUPPORT_TX_HW_AMSDU
                    if (IS_SUPPORT_WLAN_HAL_HW_TX_AMSDU(priv)){//tingchu: need to append LLC if use HW-AMSDU
                        //struct llc_snap		*pllc_snap;
	                    struct wlan_ethhdr_t	*pethhdr=NULL;
                        struct wlan_ethhdr_t	*pmsdu_hdr;
                        struct wlan_ethhdr_t	pethhdr_data;
                        unsigned short usLen;
            			int msdu_len;
                        u2Byte i;
            			memcpy(&pethhdr_data, skb->data, sizeof(struct wlan_ethhdr_t));
            			pethhdr = &pethhdr_data;
            			msdu_len = skb->len - WLAN_ETHHDR_LEN;
            			if ((ntohs(pethhdr->type) + WLAN_ETHHDR_LEN) > WLAN_MAX_ETHFRM_LEN) {
            				if (skb_headroom(skb) < sizeof(struct llc_snap)) {
            					struct sk_buff *skb2 = dev_alloc_skb(skb->len);
            					if (skb2 == NULL) {
            						printk("%s: %s, dev_alloc_skb() failed!\n", priv->dev->name, __FUNCTION__);
            						rtl_kfree_skb(priv, skb, _SKB_TX_);                                    
            					}
            					memcpy(skb_put(skb2, skb->len), skb->data, skb->len);
            					dev_kfree_skb_any(skb);
            					skb = skb2;
            					txcfg->pframe = (void *)skb;
            				}
            				skb_push(skb, sizeof(struct llc_snap));
            			}
                        #if 1 //combine this into ntohs > WLAN_MAX_ETHFRM_LEN??
            			pmsdu_hdr = (struct wlan_ethhdr_t *)skb->data;
                        
            			memcpy(pmsdu_hdr, pethhdr, 12);
                        #endif
            			if ((ntohs(pethhdr->type) + WLAN_ETHHDR_LEN) > WLAN_MAX_ETHFRM_LEN) {
            				usLen = (unsigned short)(msdu_len + sizeof(struct llc_snap));
            				pmsdu_hdr->type = htons(usLen);
            				eth_2_llc(pethhdr, (struct llc_snap *)(((unsigned long)pmsdu_hdr)+sizeof(struct wlan_ethhdr_t)));
                            #if 0
                            printk("header after append LLC:\n");
                            for (i=0;i<40;i++){
                                if(i%16==0){printk("\n");}
                                printk("%x ",*(skb->data +i));
                                
                            }
                            #endif
            			}
            			else {// we need this??
            				usLen = (unsigned short)msdu_len;
            				pmsdu_hdr->type = htons(usLen);
            			}
                        txcfg->phdr   	= skb->data;
            			txcfg->fr_len = skb->len;
                        txcfg->pframe 	= skb;
                                            
                    } 
                
                #endif
			} else
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV

			{
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
				if ((txcfg->aggre_en == FG_AGGRE_MSDU_MIDDLE) || (txcfg->aggre_en == FG_AGGRE_MSDU_LAST)){
					txcfg->fr_type = _SKB_FRAME_TYPE_;
					txcfg->phdr = NULL;
				}
				else
#endif
				{
#ifndef TXSC_HDR
					txcfg->phdr = (UINT8 *)get_wlanllchdr_from_poll(priv);
					if (txcfg->phdr == NULL) {
						DEBUG_ERR("Can't alloc wlan header!\n");
						rtl8192cd_tx_xmitSkbFail(priv, skb, dev, wdsDev, txcfg);
						goto stop_proc;
					}
					memcpy(txcfg->phdr, (const void *)&ptxsc_entry->wlanhdr, sizeof(struct wlanllc_hdr));
#endif
				}
				txcfg->pframe = skb;
#ifdef WDS
				/*Correct the aggre_mthd*/
				if(pstat->state & WIFI_WDS) {
					if(txcfg->aggre_en != pstat->aggre_mthd) {
						/*Invalid the tx_sc entry*/
						ptxsc_entry->txcfg.fr_len=0;
						txcfg->aggre_en = pstat->aggre_mthd;
					}
				}	
#endif

#ifdef SUPPORT_TX_AMSDU_SHORTCUT
				if(txcfg->aggre_en >= FG_AGGRE_MSDU_FIRST){
					unsigned short usLen;
					unsigned short needed_tailroom = 4;	//4 //4 bytes  needed for padding
					if (txcfg->privacy == _TKIP_PRIVACY_)
						needed_tailroom += 8;	// 8 bytes needed for TKIP MIC
					if (skb_tailroom(skb)< needed_tailroom) {
						struct sk_buff *skb2 = dev_alloc_skb(skb->len + needed_tailroom);
						if (skb2 == NULL) {
							printk("%s: %s, dev_alloc_skb() failed!\n", priv->dev->name, __FUNCTION__);
							rtl_kfree_skb(priv, skb, _SKB_TX_);
							if(txcfg->aggre_en == FG_AGGRE_MSDU_FIRST)
								release_wlanllchdr_to_poll(priv, txcfg->phdr);
							return 0;
						}
#if defined(MCAST2UI_REFINE) || defined(TX_SKB_REFINE)
						memcpy(skb2->cb, skb->cb, sizeof(skb->cb));
#endif
						memcpy(skb_put(skb2, skb->len), skb->data, skb->len);
						dev_kfree_skb_any(skb);
						skb = skb2;
						txcfg->pframe = (void *)skb;
					}
					skb_push(skb, sizeof(struct llc_snap));
					memcpy(skb->data, &ptxsc_entry->amsdullcsnaphdr, sizeof(struct wlan_amsdullcsnaphdr_t));
					usLen = (unsigned short)(skb->len - WLAN_ETHHDR_LEN);
					((struct wlan_amsdullcsnaphdr_t *)skb->data)->amsdulen = htons(usLen);
					if (txcfg->aggre_en == FG_AGGRE_MSDU_FIRST || txcfg->aggre_en == FG_AGGRE_MSDU_MIDDLE) {
						if ((usLen+14) & 0x3)
							skb_put(skb, 4-((usLen+14) & 0x3));
					}
					txcfg->fr_len = skb->len;
				}else
#endif
				{

#ifdef TX_SCATTER
					if (skb->list_num > 0)
						txcfg->fr_len = skb->total_len - WLAN_ETHHDR_LEN;
					else
#endif
						txcfg->fr_len = skb->len - WLAN_ETHHDR_LEN;
					skb_pull(skb, WLAN_ETHHDR_LEN);
	            }
            }

#ifdef SUPPORT_TX_AMSDU_SHORTCUT
			if (txcfg->aggre_en == FG_AGGRE_MSDU_MIDDLE || txcfg->aggre_en == FG_AGGRE_MSDU_LAST) {
				if (txcfg->privacy == _TKIP_PRIVACY_) {
					if (rtl8192cd_tx_tkip(priv, skb, pstat, txcfg) == TX_PROCEDURE_CTRL_STOP) {
						goto stop_proc;
					}
				}
				// only payload modfication needed for AMSDU MIDDLE and LAST
				// log tx statistics...
				tx_sum_up(priv, pstat, txcfg);

				SNMP_MIB_INC(dot11TransmittedFragmentCount, 1);

				// for SW LED
				priv->pshare->LED_tx_cnt++;
#ifdef CONFIG_WLAN_HAL
    			if (IS_HAL_CHIP(priv)) {
					rtl88XX_signin_txdesc_amsdu(priv, txcfg);
    			} else 
#endif
				{
					rtl8192cd_signin_txdesc_amsdu(priv, txcfg);
				}
				goto stop_proc;
			} 
#endif

#ifdef TXSC_HDR
#ifdef TX_SCATTER
				if (skb->list_num == 0) {
					reuse_txdesc = 1;
				} else {
					if (ptxsc_entry->has_desc3) {
						if (skb->list_num == 3 && (skb->list_buf[0].len == WLAN_ETHHDR_LEN) &&
							(skb->list_buf[2].len  == (get_desc(ptxsc_entry->hwdesc3.Dword7)&TX_TxBufSizeMask))){
							reuse_txdesc = 1;
						}
					} else {
						if ((skb->list_num == 1 && skb->list_buf[0].len > WLAN_ETHHDR_LEN) ||
							(skb->list_num == 2 && skb->list_buf[0].len == WLAN_ETHHDR_LEN))
							reuse_txdesc = 1;
					}
					if (skb->len == 0 && skb->list_num > 1) {
						skb->list_idx++;
						skb_assign_buf(skb, skb->list_buf[skb->list_idx].buf, skb->list_buf[skb->list_idx].len);
						skb->len = skb->list_buf[skb->list_idx].len;
					}
				}
#endif
	        if(
	                   (
                        #ifdef CONFIG_WLAN_HAL
	                    IS_HAL_CHIP(priv) ? 
                        ((GET_HAL_INTERFACE(priv)->GetShortCutTxBuffSizeHandler(priv, ptxsc_entry->hal_hw_desc)) > 0)            
                        :
                        #endif // CONFIG_WLAN_HAL
	                        ((get_desc(ptxsc_entry->hwdesc1.Dword7)&TX_TxBufSizeMask) > 0) 
	                    )
	                && (
#ifdef CONFIG_RTL_WAPI_SUPPORT
	                    // Note: for sw wapi, txcfg->mic=16; for hw wapi, txcfg->mic=0.
	                     (txcfg->privacy==_WAPI_SMS4_) ? ((skb->len+txcfg->mic)==(get_desc(ptxsc_entry->hwdesc2.Dword7)&TX_TxBufSizeMask)) :
#endif
	                    (
#ifdef CONFIG_WLAN_HAL
	                    IS_HAL_CHIP(priv) ?
	                    (skb->len == (GET_HAL_INTERFACE(priv)->GetShortCutTxBuffSizeHandler(priv, ptxsc_entry->hal_hw_desc))) 
	                    :
#endif // CONFIG_WLAN_HAL
#ifdef TXSC_SKBLEN
	                        ((GET_CHIP_VER(priv)== VERSION_8812E) ? 1 : (skb->len == (get_desc(ptxsc_entry->hwdesc2.Dword7)&TX_TxBufSizeMask)))
#else
	                        (skb->len ==  (get_desc(ptxsc_entry->hwdesc2.Dword7)&TX_TxBufSizeMask))
#endif				
	                    )
	                ) &&
#ifdef TXSC_HDR
	                        ((GET_CHIP_VER(priv)== VERSION_8812E) ? 1 : (txcfg->tx_rate == ptxsc_entry->txcfg.tx_rate)) &&
#else
	                        (txcfg->tx_rate == ptxsc_entry->txcfg.tx_rate) &&
#endif				
	                    (pstat->protection == priv->pmib->dot11ErpInfo.protection) &&
	                    (pstat->ht_protection == priv->ht_protection)
#if defined(WIFI_WMM) && defined(WMM_APSD)
	                    && (!(
#ifdef CLIENT_MODE
	                    (OPMODE & WIFI_AP_STATE) &&
#endif
	                    (APSD_ENABLE) && (pstat->state & WIFI_SLEEP_STATE)))
#endif
#ifdef TX_SCATTER
	                    && reuse_txdesc
#endif
	                    )
						 txsc_lv2 =1;

#ifdef CONFIG_WLAN_HAL                
		        if (IS_HAL_CHIP(priv)) {
#ifdef MERGE_HEADER_PAYLOAD
	                    memcpy((skb->data - (txcfg->hdr_len + txcfg->llc + txcfg->iv)), txcfg->phdr, (txcfg->hdr_len + txcfg->llc + txcfg->iv));
	                    release_wlanllchdr_to_poll(priv, txcfg->phdr);
	                    txcfg->phdr = skb->data - (txcfg->hdr_len + txcfg->llc + txcfg->iv);
	                    txcfg->one_txdesc = 1;
#else

#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
	                    if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(priv)) {
	                        txcfg->phdr     = skb->data;    // 802.3 hdr
	                        //txcfg->hdr_len    = WLAN_ETHHDR_LEN;
	                        txcfg->pframe   = skb;          // skb->data:  802.3 hdr + payload
	                        txcfg->fr_len   = skb->len;     // len(802.3 hdr + payload)
	                    } else
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
	                    {
	                    	txcfg->phdr = (UINT8 *)get_wlanllchdr_from_poll(priv);
	                    	if (txcfg->phdr == NULL) {
	                    	    DEBUG_ERR("Can't alloc wlan header!\n");
	                    		rtl8192cd_tx_xmitSkbFail(priv, skb, dev, wdsDev, txcfg);
	                    	    goto stop_proc;
	                    	}
	                    	memcpy(txcfg->phdr, (const void *)&ptxsc_entry->wlanhdr, sizeof(struct wlanllc_hdr));

	                    }
#endif	                  
		            // for TXBD mechanism, sending a packet always need one txdesc. So, txcfg->one_txdesc is useless.
		        } else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif // CONFIG_WLAN_HAL
		        {//not HAL
		            if (txsc_lv2 && (skb_headroom(skb) >= (txcfg->hdr_len + txcfg->llc + txcfg->iv
#ifdef TX_EARLY_MODE
		                + (GET_TX_EARLY_MODE ? 8 : 0)
#endif
		                )) &&
		                (txcfg->privacy != _TKIP_PRIVACY_) &&
#if defined(CONFIG_RTL_WAPI_SUPPORT)
		                (txcfg->privacy != _WAPI_SMS4_) &&
#endif
		                ((((unsigned int)skb->data) % 2) == 0)
		                )
		            {
		                txcfg->phdr = skb->data - (txcfg->hdr_len + txcfg->llc + txcfg->iv);
		                memcpy(txcfg->phdr, (const void *)&ptxsc_entry->wlanhdr, (txcfg->hdr_len + txcfg->llc + txcfg->iv));
		                txcfg->one_txdesc = 1;
		            } else {
						txcfg->phdr = (UINT8 *)get_wlanllchdr_from_poll(priv);
						if (txcfg->phdr == NULL) {
						    DEBUG_ERR("Can't alloc wlan header!\n");
							rtl8192cd_tx_xmitSkbFail(priv, skb, dev, wdsDev, txcfg);
						    goto stop_proc;
						}
						memcpy(txcfg->phdr, (const void *)&ptxsc_entry->wlanhdr, sizeof(struct wlanllc_hdr));
		            }
		        }
#endif

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
				if ((txcfg->is_11s & 1) && (GetFrameSubType(txcfg->phdr) == WIFI_11S_MESH))
	                    if(!reuse_meshhdr(priv, txcfg)) {
						        goto stop_proc;
						}
#endif

				txcfg->tx_rate = get_tx_rate(priv, pstat);
#ifndef TX_LOWESTRATE
				txcfg->lowest_tx_rate = get_lowest_tx_rate(priv, pstat, txcfg->tx_rate);
#endif			
			// log tx statistics...
			tx_sum_up(priv, pstat, txcfg);

			SNMP_MIB_INC(dot11TransmittedFragmentCount, 1);
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
			if(txcfg->aggre_en >= FG_AGGRE_MSDU_FIRST)
				pstat->tx_sc_amsdu_pkts_lv1++;
			else
#endif
				pstat->tx_sc_pkts_lv1++;

#ifdef PCIE_POWER_SAVING
				PCIeWakeUp(priv, (POWER_DOWN_T0));
#endif

				// for SW LED
				priv->pshare->LED_tx_cnt++;

				if (
#if defined(RTK_AC_SUPPORT) || defined(CONFIG_WLAN_HAL_8192EE) //FOR_VHT5G_PF
				((pstat->aggre_mthd & AGGRE_MTHD_MPDU) && txcfg->aggre_en ) ||
#endif
				((txcfg->aggre_en >= FG_AGGRE_MPDU) && (txcfg->aggre_en <= FG_AGGRE_MPDU_BUFFER_LAST))
				) {
					if (!pstat->ADDBA_ready[(int)skb->cb[1]]) {
						if ((pstat->ADDBA_req_num[(int)skb->cb[1]] < 5) && !pstat->ADDBA_sent[(int)skb->cb[1]]) {
							pstat->ADDBA_req_num[(int)skb->cb[1]]++;
							//SMP_UNLOCK_XMIT(flags);
							issue_ADDBAreq(priv, pstat, (int)skb->cb[1]);
							//SMP_LOCK_XMIT(flags);
							pstat->ADDBA_sent[(int)skb->cb[1]]++;
						}
					}
				}
#ifndef TXSC_HDR 
#ifdef TX_SCATTER
				if (skb->list_num == 0) {
					reuse_txdesc = 1;
				} else {
					if (ptxsc_entry->has_desc3) {
						if (skb->list_num == 3 && (skb->list_buf[0].len == WLAN_ETHHDR_LEN) &&
							(skb->list_buf[2].len  == (get_desc(ptxsc_entry->hwdesc3.Dword7)&TX_TxBufSizeMask))){
							reuse_txdesc = 1;
						}
					} else {
						if ((skb->list_num == 1 && skb->list_buf[0].len > WLAN_ETHHDR_LEN) ||
							(skb->list_num == 2 && skb->list_buf[0].len == WLAN_ETHHDR_LEN))
							reuse_txdesc = 1;
					}
					if (skb->len == 0 && skb->list_num > 1) {
						skb->list_idx++;
						skb_assign_buf(skb, skb->list_buf[skb->list_idx].buf, skb->list_buf[skb->list_idx].len);
						skb->len = skb->list_buf[skb->list_idx].len;
					}
				}
#endif	//TX_SCATTER
#endif

#ifdef TXSC_HDR
			if (txsc_lv2)
#elif defined(TXSC_CACHE_FRLEN)
			if (
				(ptxsc_entry->frlen > 0)
                &&
               (
                    #ifdef CONFIG_RTL_WAPI_SUPPORT
                    // Note: for sw wapi, txcfg->mic=16; for hw wapi, txcfg->mic=0.
                    (txcfg->privacy==_WAPI_SMS4_) ? ((skb->len+txcfg->mic)==(get_desc(ptxsc_entry->hwdesc2.Dword7)&TX_TxBufSizeMask)) :
                    #endif   
    				(skb->len == ptxsc_entry->frlen)                
                ) &&
                #if 1
                ((GET_CHIP_VER(priv)== VERSION_8812E) ? 1 : (txcfg->tx_rate == ptxsc_entry->txcfg.tx_rate)) &&
                #else
				(txcfg->tx_rate == ptxsc_entry->txcfg.tx_rate) &&
                #endif	

				(pstat->protection == priv->pmib->dot11ErpInfo.protection) &&
				(pstat->ht_protection == priv->ht_protection)
                #if defined(WIFI_WMM) && defined(WMM_APSD)
				&& (!(
                #ifdef CLIENT_MODE
				(OPMODE & WIFI_AP_STATE) &&
                #endif
				(APSD_ENABLE) && (pstat->state & WIFI_SLEEP_STATE)))
                #endif
                #ifdef TX_SCATTER
				&& reuse_txdesc
                #endif
            )
#else
			// check if we could re-use tx descriptor
			if (
               (
                    #ifdef CONFIG_WLAN_HAL
                    IS_HAL_CHIP(priv) ? 
                    ((GET_HAL_INTERFACE(priv)->GetShortCutTxBuffSizeHandler(priv, ptxsc_entry->hal_hw_desc)) > 0)
                    :
                    #endif // CONFIG_WLAN_HAL
                    ((get_desc(ptxsc_entry->hwdesc1.Dword7)&TX_TxBufSizeMask) > 0) 
                ) &&
                (
                    #ifdef CONFIG_RTL_WAPI_SUPPORT
    				// Note: for sw wapi, txcfg->mic=16; for hw wapi, txcfg->mic=0.
    				 (txcfg->privacy==_WAPI_SMS4_) ? ((skb->len+txcfg->mic)==(get_desc(ptxsc_entry->hwdesc2.Dword7)&TX_TxBufSizeMask)) :
                    #endif
                    (
                        #ifdef CONFIG_WLAN_HAL
                        IS_HAL_CHIP(priv) ?
                        ( skb->len == (GET_HAL_INTERFACE(priv)->GetShortCutTxBuffSizeHandler(priv, ptxsc_entry->hal_hw_desc))) 
                        :
                        #endif // CONFIG_WLAN_HAL
                        #ifdef TXSC_SKBLEN
                        ((GET_CHIP_VER(priv)== VERSION_8812E) ? 1 : (skb->len == (get_desc(ptxsc_entry->hwdesc2.Dword7)&TX_TxBufSizeMask)))
                        #else
                        (skb->len == (get_desc(ptxsc_entry->hwdesc2.Dword7)&TX_TxBufSizeMask))
                        #endif
                    )
                ) &&
                #if 1
                ((GET_CHIP_VER(priv)== VERSION_8812E) ? 1 : (txcfg->tx_rate == ptxsc_entry->txcfg.tx_rate)) &&
                #else
                (txcfg->tx_rate == ptxsc_entry->txcfg.tx_rate) &&
                #endif	
#if 1 /*eric-txsc*/
				(txcfg->q_num == ptxsc_entry->txcfg.q_num)&&
#endif

				(pstat->protection == priv->pmib->dot11ErpInfo.protection) &&
				(pstat->ht_protection == priv->ht_protection)
                #if defined(WIFI_WMM) && defined(WMM_APSD)
				&& (!(
                #ifdef CLIENT_MODE
				(OPMODE & WIFI_AP_STATE) &&
                #endif
				(APSD_ENABLE) && (pstat->state & WIFI_SLEEP_STATE)))
                #endif
                #ifdef TX_SCATTER
				&& reuse_txdesc
                #endif
            )
#endif		
            {
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
		if(txcfg->aggre_en >= FG_AGGRE_MSDU_FIRST)
			pstat->tx_sc_amsdu_pkts_lv2++;
		else
#endif
                	pstat->tx_sc_pkts_lv2++;

#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
	                if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(priv)) {
	                    // Don't fill AES IV by driver in this case.....
	                } else
#endif
	                {
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
	                }
#ifndef TXSC_HDR
#ifdef CONFIG_WLAN_HAL                
					if (IS_HAL_CHIP(priv)) {
						// for TXBD mechanism, sending a packet always need one txdesc. So, txcfg->one_txdesc is useless.
#ifdef MERGE_HEADER_PAYLOAD
	                    memcpy((skb->data - (txcfg->hdr_len + txcfg->llc + txcfg->iv)), txcfg->phdr, (txcfg->hdr_len + txcfg->llc + txcfg->iv));
	                    release_wlanllchdr_to_poll(priv, txcfg->phdr);
	                    txcfg->phdr = skb->data - (txcfg->hdr_len + txcfg->llc + txcfg->iv);
	                    txcfg->one_txdesc = 1;
#endif
					} else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif // CONFIG_WLAN_HAL
					{//not HAL
						if ((skb_headroom(skb) >= (txcfg->hdr_len + txcfg->llc + txcfg->iv
#ifdef TX_EARLY_MODE
							+ (GET_TX_EARLY_MODE ? 8 : 0)
#endif
							)) &&
							!skb_cloned(skb) &&
							(txcfg->privacy != _TKIP_PRIVACY_) &&
#if defined(CONFIG_RTL_WAPI_SUPPORT)
							(txcfg->privacy != _WAPI_SMS4_) &&
#endif
							((((unsigned int)skb->data) % 2) == 0)
							)
						{
							memcpy((skb->data - (txcfg->hdr_len + txcfg->llc + txcfg->iv)), txcfg->phdr, (txcfg->hdr_len + txcfg->llc + txcfg->iv));
							release_wlanllchdr_to_poll(priv, txcfg->phdr);
							txcfg->phdr = skb->data - (txcfg->hdr_len + txcfg->llc + txcfg->iv);
							txcfg->one_txdesc = 1;
						}
					}

#endif
                    
#ifdef CONFIG_WLAN_HAL                        
				if(IS_HAL_CHIP(priv)) {
                    
					rtl88XX_signin_txdesc_shortcut(priv, txcfg, k);
                }
				else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif
					rtl8192cd_signin_txdesc_shortcut(priv, txcfg, k);


#if defined(SHORTCUT_STATISTIC) /*defined(__ECOS) && defined(_DEBUG_RTL8192CD_)*/
					priv->ext_stats.tx_cnt_sc2++;
#endif
					goto stop_proc;
				}

#ifdef CONFIG_RTK_MESH
	            if(txcfg->is_11s) {
	                memcpy(&priv->ethhdr, &(ptxsc_entry->ethhdr), sizeof(struct wlan_ethhdr_t));
	            }
#endif

#ifdef  CONFIG_WLAN_HAL
				if (IS_HAL_CHIP(priv)) {
					rtl88XX_signin_txdesc(priv, txcfg, HW_TX_SC_HEADER_CONV);
				} else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif
				{//not HAL
					rtl8192cd_signin_txdesc(priv, txcfg);
				}

#if defined(SHORTCUT_STATISTIC) /*defined(__ECOS) && defined(_DEBUG_RTL8192CD_)*/
				priv->ext_stats.tx_cnt_sc1++; 
#endif
				ptxsc_entry->txcfg.tx_rate = txcfg->tx_rate;
				goto stop_proc;
		}
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
	if (rtl8192cd_tx_slowPath(priv, skb, pstat, dev, wdsDev, txcfg) == TX_PROCEDURE_CTRL_STOP) {
		goto stop_proc;
	}

#ifdef __KERNEL__
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,13,0)
	dev->trans_start = jiffies;
#endif
#endif

	goto stop_proc;

free_and_stop:		/* Free current packet and stop TX process */

	rtl_kfree_skb(priv, skb, _SKB_TX_);

stop_proc:			/* Stop process and assume the TX-ed packet is already "processed" (freed or TXed) in previous code. */
	
	return 0;
}


#ifdef TCP_ACK_ACC
static struct tcp_session *get_tcp_session(struct stat_info *pstat, unsigned long saddr, unsigned long daddr,
	unsigned short src_port, unsigned short dst_port)
{
	unsigned short hash = 0, idx;
	struct list_head *phead, *plist;
	struct tcp_session *tcp_ses;

	hash = ((saddr & 0xffff0000) >> 16) ^ (saddr & 0xffff);
	hash ^= ((daddr & 0xffff0000) >> 16) ^ (daddr & 0xffff);
	hash ^= src_port ^ dst_port;
	idx = hash & (TCP_SESSION_MAX_ENTRY - 1);

	phead = &pstat->tcp_ses_active_list[idx];
	plist = phead->next;
	while (plist != phead) {
		tcp_ses = list_entry(plist, struct tcp_session, list);
		plist = plist->next;

		if (tcp_ses->src_ip == saddr && tcp_ses->dst_ip == daddr && 
			tcp_ses->src_port == src_port && tcp_ses->dst_port == dst_port) {
			return tcp_ses;
		}
	}
	return NULL;
}

static struct tcp_session *add_tcp_session(struct stat_info *pstat, unsigned long saddr, unsigned long daddr,
	unsigned short src_port, unsigned short dst_port)
{
	unsigned short hash = 0, idx;
	struct list_head *phead, *plist;
	struct tcp_session *tcp_ses = NULL;

	if (list_empty(&pstat->tcp_ses_list))
		return NULL;

	hash = ((saddr & 0xffff0000) >> 16) ^ (saddr & 0xffff);
	hash ^= ((daddr & 0xffff0000) >> 16) ^ (daddr & 0xffff);
	hash ^= src_port ^ dst_port;
	idx = hash & (TCP_SESSION_MAX_ENTRY - 1);

	phead = &pstat->tcp_ses_list;
	plist = phead->next;
	tcp_ses = list_entry(plist, struct tcp_session, list);
	list_del_init(plist);
	tcp_ses->src_ip = saddr;
	tcp_ses->dst_ip = daddr;
	tcp_ses->src_port = src_port;
	tcp_ses->dst_port = dst_port;
	tcp_ses->used = 1;

	list_add_tail(plist, &pstat->tcp_ses_active_list[idx]);

	return tcp_ses;
}

static void del_tcp_session(struct stat_info *pstat, struct tcp_session *tcp_ses)
{
	list_del_init(&tcp_ses->list);
	memset(tcp_ses, 0, sizeof(struct tcp_session));
	list_add_tail(&tcp_ses->list, &pstat->tcp_ses_list);
}

void check_tcp_ack_timeout(struct rtl8192cd_priv *priv)
{
	struct list_head *phead, *plist;
	struct stat_info *pstat;
	struct tcp_session *tcp_ses;
	struct sk_buff *pskb;
	unsigned int i;
	unsigned int timeout = priv->pmib->miscEntry.tcpack_to;
	unsigned long flags = 0, flags2= 0;

	SAVE_INT_AND_CLI(flags);
	SMP_LOCK_XMIT(flags);

	SMP_LOCK_ASOC_LIST(flags2);

	phead = &priv->asoc_list;
	plist = phead->next;
	while (plist != phead) {
		pstat = list_entry(plist, struct stat_info, asoc_list);
		plist = plist->next;

		for (i = 0; i < TCP_SESSION_MAX_ENTRY; i++) {
			tcp_ses = &pstat->tcp_ses[i];
			if (tcp_ses->used) {
				if (!priv->pmib->miscEntry.tcpack_acc) {
					del_tcp_session(pstat, tcp_ses);
				}
				else if (priv->pmib->miscEntry.tcpack_delay_start != 0 &&
							jiffies - tcp_ses->boot_time < RTL_MILISECONDS_TO_JIFFIES(priv->pmib->miscEntry.tcpack_delay_start)) {
					continue;
				}
				else if (jiffies - tcp_ses->timestamp > RTL_SECONDS_TO_JIFFIES(2)) {
					del_tcp_session(pstat, tcp_ses);
				}
				else if (jiffies - tcp_ses->timestamp > RTL_MILISECONDS_TO_JIFFIES(timeout)) {
					pskb = tcp_ses->buf_ack;
					if (pskb) {
						tcp_ses->buf_ack = NULL;
						tcp_ses->timestamp = jiffies;

						__rtl8192cd_start_xmit(pskb, priv->dev, TX_NO_TCPACK_ACC);
					}
				}
			}
		}
	}

	SMP_UNLOCK_ASOC_LIST(flags2);
	
	RESTORE_INT(flags);
	SMP_UNLOCK_XMIT(flags);

	if (IS_DRV_OPEN(priv))
		mod_timer(&priv->tcpack_timer, jiffies + RTL_MILISECONDS_TO_JIFFIES(1));
}

int tx_tcp_ack_accelerate(struct rtl8192cd_priv *priv, struct stat_info *pstat, struct sk_buff *skb)
{
	struct iphdr *iph;
	struct tcphdr *tcph;
	struct tcp_session *tcp_ses;
	int ret = 0;
	unsigned int timeout = priv->pmib->miscEntry.tcpack_to;
	unsigned long seq;
	unsigned int ack_th = (pstat->veriwave_5G_TCP_Rx_test ? 0 : 
		(pstat->tcpack_acc_en == 1 ? priv->pmib->miscEntry.tcpack_cnt_th1 : priv->pmib->miscEntry.tcpack_cnt_th2));
	
	do {
		if (*(unsigned short *)(skb->data + ETH_ALEN * 2) != __constant_htons(ETH_P_IP))
			break;

		iph = (struct iphdr *)(skb->data + ETH_HLEN);
		if (iph->protocol != 6) //IPPROTO_TCP
			break;

		tcph = (struct tcphdr *)((unsigned long)iph + (iph->ihl << 2));
		if (!tcph->ack || tcph->syn)
			break;
		if ((tcph->doff << 2) + (iph->ihl << 2) != ntohs(iph->tot_len))
			break;
				
		seq = ntohl(tcph->ack_seq);
		if (seq == 1)
			break;

		tcp_ses = get_tcp_session(pstat, iph->saddr, iph->daddr, tcph->source, tcph->dest);
		if (tcp_ses != NULL) {
			if (priv->pmib->miscEntry.tcpack_delay_start != 0 &&
				jiffies - tcp_ses->boot_time < RTL_MILISECONDS_TO_JIFFIES(priv->pmib->miscEntry.tcpack_delay_start)) {					
				tcp_ses->timestamp = jiffies;
				break;
			}
					
			if (seq < tcp_ses->ack_seq) {
				rtl_kfree_skb(priv, skb, _SKB_TX_);
				tcp_ses->hit++;
				tcp_ses->ack_cnt++;
				ret = -1;
				break;
			}

			if (tcph->urg) {
				tcp_ses->timestamp = jiffies;
				break;
			}

			tcp_ses->ack_seq = seq;
					
			if (tcp_ses->buf_ack)
				rtl_kfree_skb(priv, tcp_ses->buf_ack, _SKB_TX_);
					
			if (jiffies - tcp_ses->timestamp < RTL_MILISECONDS_TO_JIFFIES(timeout)) {
				tcp_ses->ack_cnt++;
				tcp_ses->hit++;
				if(ack_th && tcp_ses->ack_cnt >= ack_th) {
					tcp_ses->ack_cnt = 0;
					tcp_ses->buf_ack = NULL;
					tcp_ses->timestamp = jiffies;
				}
				else {
					tcp_ses->buf_ack = skb;
					ret = -1;
				}
			}
			else {
				tcp_ses->ack_cnt = 0;
				tcp_ses->buf_ack = NULL;
				tcp_ses->timestamp = jiffies;
			}
					
		}
		else {
			tcp_ses = add_tcp_session(pstat, iph->saddr, iph->daddr, tcph->source, tcph->dest);
			if (tcp_ses) {
				tcp_ses->buf_ack = NULL;
				tcp_ses->ack_seq = seq;
				tcp_ses->timestamp = jiffies;
				tcp_ses->boot_time = jiffies;
			}
		}
	} while (0);

	return ret;
}
#endif

__MIPS16 __IRAM_IN_865X
unsigned int pri_to_qnum(struct rtl8192cd_priv *priv, int priority)
{
#ifdef RTL_MANUAL_EDCA
#ifdef SSID_PRIORITY_SUPPORT
			if (priv->pmib->miscEntry.manual_priority && (OPMODE & WIFI_AP_STATE))
				priority = priv->pmib->miscEntry.manual_priority;
#endif
	if (priv->pmib->dot11QosEntry.ManualEDCA) {
		return priv->pmib->dot11QosEntry.TID_mapping[priority];
	}
	else {
		if ((priority == 0) || (priority == 3)) {
			if (!((OPMODE & WIFI_STATION_STATE) && GET_STA_AC_BE_PARA.ACM))
				return BE_QUEUE;
			else
				return BK_QUEUE;
		} else if ((priority == 7) || (priority == 6)) {
			if (!((OPMODE & WIFI_STATION_STATE) && GET_STA_AC_VO_PARA.ACM)) {
				return VO_QUEUE;
			} else {
				if (!GET_STA_AC_VI_PARA.ACM)
					return VI_QUEUE;
				else if (!GET_STA_AC_BE_PARA.ACM)
					return BE_QUEUE;
				else
					return BK_QUEUE;
			}
		} else if ((priority == 5) || (priority == 4)) {
			if (!((OPMODE & WIFI_STATION_STATE) && GET_STA_AC_VI_PARA.ACM)) {
				return VI_QUEUE;
			} else {
				if (!GET_STA_AC_BE_PARA.ACM)
					return BE_QUEUE;
				else
					return BK_QUEUE;
			}
		} else {
			return BK_QUEUE;
		}
	}
#else
#ifdef SSID_PRIORITY_SUPPORT
	if(priv->pmib->miscEntry.manual_priority)
		priority = priv->pmib->miscEntry.manual_priority;
#endif
	if ((priority == 0) || (priority == 3))
		return BE_QUEUE;
	else if ((priority == 7) || (priority == 6))
		return VO_QUEUE;
	else if ((priority == 5) || (priority == 4))
		return VI_QUEUE;
	else
		return BK_QUEUE;
#endif
}



#ifdef SUPPORT_TX_MCAST2UNI
#ifndef CONFIG_RTK_MESH
static 
#endif
int isICMPv6Mng(struct sk_buff *skb)
{
#if defined(__ECOS) || defined(TAROKO_0)
	if((skb->data[12] == 0x86) &&(skb->data[13] == 0xdd)&&
#else
	if((skb->protocol == __constant_htons(0x86dd)) &&
#endif
		((skb->data[20] == 0x3a) || (skb->data[54] == 0x3a)) //next header is icmpv6
		//&& skb->data[54] == 0x86 //RA
		/*128,129,133,.....255 SHOULD BE MANAGMENT PACKET
		 REF:http://en.wikipedia.org/wiki/ICMPv6 */
	)
	{
		return 1;		
	}
	else
		return 0;
}

int isMDNS(unsigned char *data)
{
	if((data[3] == 0x00) && (data[4] == 0x00) && (data[5] == 0xfb) &&
			(((data[12] == 0x08) && (data[13] == 0x00) // IPv4
			&& (data[23] == 0x11) // UDP
			//&& (data[30] == 0xe0) // 224.0.0.251
			&& (data[36] == 0x14) && (data[37] == 0xe9)) // port 5353
			||
			((data[12] == 0x86) && (data[13] == 0xdd) // IPv6
			&& (data[20] == 0x11) // next header is UDP
			&& (data[56] == 0x14) && (data[57] == 0xe9)) // port 5353
			)) {
		return 1;
	}
		
	return 0;
}

#ifdef CONFIG_PCI_HCI

static inline void check_tx_queue(struct rtl8192cd_priv *priv)
{
	if (getAvailableTXBD( priv, BE_QUEUE) < (CURRENT_NUM_TX_DESC/4)) {
		rtl8192cd_tx_queueDsr(priv, BE_QUEUE);
	}
		
	return;
}
#endif // CONFIG_PCI_HCI


#if defined(CONFIG_RTL865X_ETH_PRIV_SKB) || defined(CONFIG_RTL_ETH_PRIV_SKB)
	extern struct sk_buff *priv_skb_copy(struct sk_buff *skb);
	extern int eth_skb_free_num;
#endif
#ifdef CONFIG_RTL8196B_GW_8M
#define ETH_SKB_FREE_TH 50
#else
#define ETH_SKB_FREE_TH 100
#endif

int isSpecialFloodMac(struct rtl8192cd_priv *priv, struct sk_buff *skb)
{
	int i;
	if(priv->pshare->rf_ft_var.mc2u_flood_ctrl==0)
	{
		return 0;
	}
	
	for(i=0; i< priv->pshare->rf_ft_var.mc2u_flood_mac_num; i++)
	{
		if(memcmp(skb->data, priv->pshare->rf_ft_var.mc2u_flood_mac[i].macAddr,MACADDRLEN)==0)
		{
			return 1;
		}

	}
	return 0;
}
#if defined(USER_RESERVED_MAC_TO_DISABLE_M2U)
int isReservedMac(struct rtl8192cd_priv *priv, struct sk_buff *skb)
{
	int i;
	
	for(i=0; i< priv->pshare->rf_ft_var.mc2u_reserved_mac_num; i++)
	{
		if(priv->pshare->rf_ft_var.mc2u_reserved_mac[i].valid && 
			memcmp(skb->data, priv->pshare->rf_ft_var.mc2u_reserved_mac[i].macAddr,MACADDRLEN)==0)
		{
			return 1;
		}

	}
	return 0;
}
#endif

int mlcst2unicst(struct rtl8192cd_priv *priv, struct sk_buff *skb)
{
	struct stat_info *pstat, *sa_stat;
	struct list_head *phead, *plist;
	struct sk_buff *newskb;
	unsigned char origDest[6];
	int i= 0;
	int m2uCnt =0;
	int fwdCnt=0;
	struct stat_info *pstat_found = NULL;
#ifdef MCAST2UI_REFINE
	unsigned int privacy;
#endif

#ifdef CONFIG_MAXED_MULTICAST	
	int M2Uanyway=0;
#endif
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif

#ifdef HS2_SUPPORT
	// if AP 
	// 1.support HS2 and 
	// 2.dgaf disable=0 
	// it means to let icmpv6 mgmt multicast to clients
	if ((priv->pmib->hs2Entry.hs2_ielen != 0) && (priv->dgaf_disable == 0)) {
		if (isICMPv6Mng(skb) || IS_ICMPV6_PROTO(skb->data))
			return 0;
    }
#endif

#if defined(USER_RESERVED_MAC_TO_DISABLE_M2U)
	if(isReservedMac(priv,skb))
		return FALSE;
#endif
	
	memcpy(origDest, skb->data, 6);

    sa_stat = get_stainfo(priv, skb->data+MACADDRLEN);
#ifdef A4_STA
    if (priv->pmib->miscEntry.a4_enable) {
        if(sa_stat == NULL) {
            sa_stat = a4_sta_lookup(priv, skb->data+MACADDRLEN);
        }
    }
#endif  

	// all multicast managment packet try do m2u
	if( isSpecialFloodMac(priv,skb) /*|| IS_MDNSV4_MAC(skb->data)||IS_MDNSV6_MAC(skb->data)*/||IS_IGMP_PROTO(skb->data) || isICMPv6Mng(skb) || IS_ICMPV6_PROTO(skb->data)|| isMDNS(skb->data))
	{
		/*added by qinjunjie,do multicast to unicast conversion, and send to every associated station */
		phead = &priv->asoc_list;
		plist = phead;
		
		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE)
			SMP_LOCK_ASOC_LIST(flags);
		while ((plist = asoc_list_get_next(priv, plist)) != phead) {
			pstat = list_entry(plist, struct stat_info, asoc_list);

            #ifdef CONFIG_RTK_MESH
            if(isPossibleNeighbor(pstat)) {
                continue;
            }
            #endif

			/* avoid   come from STA1 and send back STA1 */ 
			if (sa_stat == pstat) {
				continue; 
			}
			
            if(pstat->leave || pstat->expire_to==0)
				continue;
			if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE)
				SMP_UNLOCK_ASOC_LIST(flags);
#ifdef MCAST2UI_REFINE
			privacy = get_sta_encrypt_algthm(priv, pstat);
			if ((privacy == _NO_PRIVACY_ || (privacy == _CCMP_PRIVACY_
#ifdef CONFIG_IEEE80211W
				&& !UseSwCrypto(priv, pstat, FALSE,0)))
#else
				&& !UseSwCrypto(priv, pstat, FALSE)))
#endif
				&& (newskb = skb_clone(skb, GFP_ATOMIC)))
			{
				memcpy(&newskb->cb[10], pstat->cmn_info.mac_addr, 6);
				newskb->cb[2] = (char)0xff;
				__rtl8192cd_start_xmit(newskb, priv->dev, TX_NO_MUL2UNI);
			}
			else
#endif
			{
				if((plist->next == phead) && !skb_cloned(skb)) {
					asoc_list_unref(priv, pstat);
#ifdef MCAST2UI_REFINE                                                  
					memcpy(&skb->cb[10], pstat->cmn_info.mac_addr, 6);
#else
					memcpy(skb->data, pstat->cmn_info.mac_addr, 6);
#endif
					skb->cb[2] = (char)0xff;			// not do aggregation
#ifdef ENABLE_RTL_SKB_STATS
					rtl_atomic_dec(&priv->rtl_tx_skb_cnt);
#endif
					__rtl8192cd_start_xmit(skb, priv->dev, TX_NO_MUL2UNI);
					return TRUE;
				}
				
#if defined(CONFIG_RTL865X_ETH_PRIV_SKB) || defined(CONFIG_RTL_ETH_PRIV_SKB)
				newskb = priv_skb_copy(skb);
#else
				newskb = skb_copy(skb, GFP_ATOMIC);
#endif
				if (newskb) {
#ifdef MCAST2UI_REFINE                                                  
					memcpy(&newskb->cb[10], pstat->cmn_info.mac_addr, 6);
#else
					memcpy(newskb->data, pstat->cmn_info.mac_addr, 6);
#endif
					newskb->cb[2] = (char)0xff;			// not do aggregation
					__rtl8192cd_start_xmit(newskb, priv->dev, TX_NO_MUL2UNI);
				}
				else {
					asoc_list_unref(priv, pstat);
					
					DEBUG_ERR("%s: muti2unit skb_copy() failed!\n", priv->dev->name);
					priv->stop_tx_mcast2uni = 2;
					priv->ext_stats.tx_drops++;
					DEBUG_ERR("TX DROP: %s: run out ether buffer!\n", __FUNCTION__);
					rtl_kfree_skb(priv, skb, _SKB_TX_);
					return TRUE;
				}

			}

			if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE)
				SMP_LOCK_ASOC_LIST(flags);
			if (plist == plist->next)
				break;
		}
		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE)
			SMP_UNLOCK_ASOC_LIST(flags);
		rtl_kfree_skb(priv, skb, _SKB_TX_);
		return TRUE;
	}

#ifdef TV_MODE
    if((priv->tv_mode_status & BIT0) == 0) { /*TV mode is disabled*/
#ifdef A4_STA
        if(priv->pmib->miscEntry.a4_enable == 0)
#endif
            return FALSE;
    }
#endif

//#ifdef VIDEO_STREAMING_REFINE
	// for video streaming refine 
	phead = &priv->asoc_list;
	plist = phead;

	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE)
		SMP_LOCK_ASOC_LIST(flags);
	while ((plist = asoc_list_get_next(priv, plist)) != phead) {
		pstat = list_entry(plist, struct stat_info, asoc_list);

        #ifdef CONFIG_RTK_MESH
        if(isPossibleNeighbor(pstat)) {
            continue;
        }
        #endif

		/* avoid   come from STA1 and send back STA1 */ 
		if (sa_stat == pstat){		
			continue; 
		}		
        if(pstat->leave || pstat->expire_to==0)
            continue; 
		for (i=0; i < pstat->ipmc_num; i++) {
			if (!memcmp(&pstat->ipmc[i].mcmac[0], origDest, 6)) {
				pstat_found = pstat;
				m2uCnt++;
				break;
			}
		}
	}
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE)
		SMP_UNLOCK_ASOC_LIST(flags);
    
#ifdef TV_MODE
    if(priv->tv_mode_status & BIT0)/*TV mode is enabled*/    
#endif
    {
        if (m2uCnt == 1 && !skb_cloned(skb)) {
#ifdef MCAST2UI_REFINE                                                  
            memcpy(&skb->cb[10], pstat_found->cmn_info.mac_addr, 6);
#else
            memcpy(skb->data, pstat_found->cmn_info.mac_addr, 6);
#endif
#ifdef ENABLE_RTL_SKB_STATS
            rtl_atomic_dec(&priv->rtl_tx_skb_cnt);
#endif
            __rtl8192cd_start_xmit(skb, priv->dev, TX_NO_MUL2UNI);		
            return TRUE;
        }
    }

	fwdCnt = m2uCnt;
	
	if(m2uCnt == 0){

#ifdef CONFIG_MAXED_MULTICAST	
		/*case: when STA <=3 do M2U anyway ; 
		if STA number > 3 by orig method(multicast);*/
		if(priv->assoc_num <=3){
			M2Uanyway=1;
			fwdCnt = priv->assoc_num;			
		}else
#endif
		if(!priv->pshare->rf_ft_var.mc2u_drop_unknown) 
		{
			/*case: if M2U can't success then 
			  forward by multicast(orig method),
			  defect: may affect system performance
		    	 advantage:better compatibility*/ 
			return FALSE;		
		}
		else
		{

		/*case: if M2U can't success then drop this packet ;
		    defect:maybe some management packet will lose
		    advantage:better performance*/ 
	   	    DEBUG_WARN("TX DROP: %s %d !\n", __FUNCTION__,__LINE__);
			priv->ext_stats.tx_drops++;
			rtl_kfree_skb(priv, skb, _SKB_TX_);
			return TRUE;
			
		}
	}
//#endif
	
	// Do multicast to unicast conversion
	phead = &priv->asoc_list;
	plist = phead;
	
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE)
		SMP_LOCK_ASOC_LIST(flags);
	while ((plist = asoc_list_get_next(priv, plist)) != phead) {
		pstat = list_entry(plist, struct stat_info, asoc_list);

        #ifdef CONFIG_RTK_MESH
        if(isPossibleNeighbor(pstat)) {
            continue;
        }
        #endif

		/* avoid   come from STA1 and send back STA1 */ 
		if (sa_stat == pstat) {
			continue;
		}		
        
        if(pstat->leave || pstat->expire_to==0)
            continue;

#ifdef TV_MODE
        if((priv->tv_mode_status & BIT0) == 0) { /*TV mode is disabled*/
#ifdef A4_STA
            if((pstat->state & WIFI_A4_STA) == 0)
#endif
                continue;
        }
#endif

		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE)
			SMP_UNLOCK_ASOC_LIST(flags);
		
#ifdef CONFIG_PCI_HCI
		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE)
		{
			int *tx_head, *tx_tail, q_num;
			struct rtl8192cd_hw	*phw = GET_HW(priv);
			q_num = BE_QUEUE;	// use BE queue to send multicast
#ifdef CONFIG_WLAN_HAL
	        //PHCI_TX_DMA_MANAGER_88XX    ptx_dma;
			if (IS_HAL_CHIP(priv)) {
				//ptx_dma = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PTxDMA88XX);
			} else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif
			{//not HAL
				tx_head = get_txhead_addr(phw, q_num);
				tx_tail = get_txtail_addr(phw, q_num);
			}
			
			if (priv->stop_tx_mcast2uni) {
				rtl8192cd_tx_queueDsr(priv, q_num);

				if (priv->stop_tx_mcast2uni  == 1) {
					if (getAvailableTXBD(priv, q_num) < (CURRENT_NUM_TX_DESC/4))
						priv->stop_tx_mcast2uni = 0;
				}

#if defined(CONFIG_RTL865X_ETH_PRIV_SKB) || defined(CONFIG_RTL_ETH_PRIV_SKB)
				else if ((priv->stop_tx_mcast2uni == 2) && (eth_skb_free_num > ETH_SKB_FREE_TH))
				{
					priv->stop_tx_mcast2uni = 0;
				}
#endif
				else {
					asoc_list_unref(priv, pstat);
					priv->ext_stats.tx_drops++;
					DEBUG_ERR("TX DROP: %s: run out ether buffer!\n", __FUNCTION__);
					rtl_kfree_skb(priv, skb, _SKB_TX_);
					return TRUE;
				}
			} else {
				if (getAvailableTXBD(priv, q_num) < 20) {					
					asoc_list_unref(priv, pstat);
#ifndef __ECOS
					priv->stop_tx_mcast2uni = 1;
#endif
					priv->ext_stats.tx_drops++;
					DEBUG_ERR("TX DROP: %s: txdesc full!\n", __FUNCTION__);
					rtl_kfree_skb(priv, skb, _SKB_TX_);
					return TRUE;
				}
			}
		}
#endif // CONFIG_PCI_HCI

#ifdef CONFIG_MAXED_MULTICAST

		if(M2Uanyway){
			if((fwdCnt== 1) && !skb_cloned(skb))
			{
				asoc_list_unref(priv, pstat);
#ifdef MCAST2UI_REFINE                                                  
				memcpy(&skb->cb[10], pstat->cmn_info.mac_addr, 6);
#else
				memcpy(skb->data, pstat->cmn_info.mac_addr, 6);
#endif
				skb->cb[2] = (char)0xff;			// not do aggregation
#ifdef ENABLE_RTL_SKB_STATS
				rtl_atomic_dec(&priv->rtl_tx_skb_cnt);
#endif
				__rtl8192cd_start_xmit(skb, priv->dev, TX_NO_MUL2UNI);
				return TRUE;
			}
			
#if defined(CONFIG_RTL865X_ETH_PRIV_SKB) || defined(CONFIG_RTL_ETH_PRIV_SKB)
			newskb = priv_skb_copy(skb);
#else
			newskb = skb_copy(skb, GFP_ATOMIC);
#endif
			if (newskb) {
#ifdef MCAST2UI_REFINE                                                  
				memcpy(&newskb->cb[10], pstat->cmn_info.mac_addr, 6);
#else
				memcpy(newskb->data, pstat->cmn_info.mac_addr, 6);
#endif
				newskb->cb[2] = (char)0xff;			// not do aggregation
				__rtl8192cd_start_xmit(newskb, priv->dev, TX_NO_MUL2UNI);
			}
			else {
				asoc_list_unref(priv, pstat);
				
				DEBUG_ERR("%s: muti2unit skb_copy() failed!\n", priv->dev->name);
				#ifdef __ECOS
				#else
				priv->stop_tx_mcast2uni = 2;
				#endif
				priv->ext_stats.tx_drops++;
				DEBUG_ERR("TX DROP: %s: run out ether buffer!\n", __FUNCTION__);
				rtl_kfree_skb(priv, skb, _SKB_TX_);
				return TRUE;
			}
			fwdCnt--;
			continue;
		}
#endif
		for (i=0; i < pstat->ipmc_num; i++) {
			if (!memcmp(&pstat->ipmc[i].mcmac[0], origDest, 6)) {


				if((fwdCnt== 1) && !skb_cloned(skb))
				{
					asoc_list_unref(priv, pstat);
#ifdef MCAST2UI_REFINE                                                  
					memcpy(&skb->cb[10], pstat->cmn_info.mac_addr, 6);
#else
					memcpy(skb->data, pstat->cmn_info.mac_addr, 6);
#endif
					skb->cb[2] = (char)0xff; 		// not do aggregation
#ifdef ENABLE_RTL_SKB_STATS
					rtl_atomic_dec(&priv->rtl_tx_skb_cnt);
#endif
					__rtl8192cd_start_xmit(skb, priv->dev, TX_NO_MUL2UNI);
					return TRUE;
				}
				else {
#ifdef MCAST2UI_REFINE
					privacy = get_sta_encrypt_algthm(priv, pstat);
					if ((privacy == _NO_PRIVACY_ || (privacy == _CCMP_PRIVACY_
#ifdef CONFIG_IEEE80211W
							&& !UseSwCrypto(priv, pstat, FALSE,0)))
#else
							&& !UseSwCrypto(priv, pstat, FALSE)))
#endif
							&& (newskb = skb_clone(skb, GFP_ATOMIC)))
					{
						memcpy(&newskb->cb[10], pstat->cmn_info.mac_addr, 6);
						newskb->cb[2] = (char)0xff;
						__rtl8192cd_start_xmit(newskb, priv->dev, TX_NO_MUL2UNI);
					}
					else
#endif
					{	
						#if defined(CONFIG_RTL865X_ETH_PRIV_SKB) || defined(CONFIG_RTL_ETH_PRIV_SKB)

						newskb = priv_skb_copy(skb);
						#else
						newskb = skb_copy(skb, GFP_ATOMIC);
						#endif
						if (newskb) {
#ifdef MCAST2UI_REFINE                                                  
							memcpy(&newskb->cb[10], pstat->cmn_info.mac_addr, 6);
#else
							memcpy(newskb->data, pstat->cmn_info.mac_addr, 6);
#endif
							newskb->cb[2] = (char)0xff;			// not do aggregation
							__rtl8192cd_start_xmit(newskb, priv->dev, TX_NO_MUL2UNI);
						}
						else {
							asoc_list_unref(priv, pstat);
							
							DEBUG_ERR("%s: muti2unit skb_copy() failed!\n", priv->dev->name);
							#ifdef __ECOS
							#else
							priv->stop_tx_mcast2uni = 2;
							#endif
							priv->ext_stats.tx_drops++;
							DEBUG_ERR("TX DROP: %s: run out ether buffer!\n", __FUNCTION__);
							rtl_kfree_skb(priv, skb, _SKB_TX_);
							return TRUE;
						}
					}
					fwdCnt--;
					break;
				}
			}
		}

		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE)
			SMP_LOCK_ASOC_LIST(flags);
		if (plist == plist->next)
			break;
	}
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE)
		SMP_UNLOCK_ASOC_LIST(flags);
    
#ifdef TV_MODE
    if((priv->tv_mode_status & BIT0) == 0 && fwdCnt) {
        /*TV mode is disabled and has joint legacy sta*/
        if(!skb_cloned(skb)) {   
            #ifdef ENABLE_RTL_SKB_STATS
            rtl_atomic_dec(&priv->rtl_tx_skb_cnt);
            #endif            
            __rtl8192cd_start_xmit(skb, priv->dev, TX_NO_MUL2UNI);
        } else 
        {
            #if defined(CONFIG_RTL865X_ETH_PRIV_SKB) || defined(CONFIG_RTL_ETH_PRIV_SKB)
            newskb = priv_skb_copy(skb);
            #else
            newskb = skb_copy(skb, GFP_ATOMIC);
            #endif
            if (newskb) {
                __rtl8192cd_start_xmit(newskb, priv->dev, TX_NO_MUL2UNI);
            }
            else {            
                asoc_list_unref(priv, pstat);                
                DEBUG_ERR("%s: muti2unit skb_copy() failed!\n", priv->dev->name);
                #ifdef __ECOS
                #else
                priv->stop_tx_mcast2uni = 2;
                #endif
                priv->ext_stats.tx_drops++;
                DEBUG_ERR("TX DROP: %s: run out ether buffer!\n", __FUNCTION__);
            }
            rtl_kfree_skb(priv, skb, _SKB_TX_);
        }
        return TRUE;
    }
#endif
	/*
	 *	Device interested in this MC IP cannot be found, drop packet.
	 */
	rtl_kfree_skb(priv, skb, _SKB_TX_);
	return TRUE;
}

#endif // TX_SUPPORT_MCAST2U




#if defined(SW_TX_QUEUE) || defined(RTK_ATM)
__inline__ static int rtl8192cd_swq_bdfull(struct rtl8192cd_priv *priv, unsigned int num,  unsigned char qnum)
{
    if (getAvailableTXBD(priv, qnum) < num)
        return 1;
    else
    	return 0;
}
#endif


#ifdef SW_TX_QUEUE

#define SWQ_DEQ_ALL			1
#define SWQ_DEQ_LIMIT			0

/* when using hw timer,  the parameter "add_timer_timeout" is TSF time
     when using sw timer, the parameter "add_timer_timeout" is jiffies time
*/
__inline__ static unsigned char rtl8192cd_swq_settimer(struct rtl8192cd_priv *priv, UINT32 timer_timeout)
{
    UINT32 set_timeout;    
    UINT32 current_time;
    unsigned char set_timer = 0;

    if(timer_timeout == 0) { // from deltimer
        if(priv->pshare->swq_current_timeout) {
            //priv->pshare->swq_current_timeout = 0;
            if(priv->pshare->swq_use_hw_timer) {  
				/*
                current_time = RTL_R32(TSFTR1);  
                if(current_time - priv->pshare->swq_last_tsf < SWQ_HWTIMER_DELAY) {
                    //delay_us(SWQ_HWTIMER_DELAY - (current_time - priv->pshare->swq_last_tsf));
                    return;
                }
#ifndef SMP_LOAD_BALANCE_SUPPORT
                RTL_W32(TC4_CTRL, 0);
#endif
                priv->pshare->swq_last_tsf = RTL_R32(TSFTR1);
                */
            }
            else {      
                if (timer_pending(&priv->pshare->swq_sw_timer))
                    del_timer_sync(&priv->pshare->swq_sw_timer);
            }            
        }
    }
    else {
        if(priv->pshare->swq_use_hw_timer) {  
#ifdef SMP_LOAD_BALANCE_SUPPORT
            priv->pshare->swq_current_timeout = timer_timeout;
#else

#ifdef CONFIG_WLAN_HAL
            if ( IS_HAL_CHIP(priv) ) {
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

            if(priv->pshare->swq_current_timeout) {
                if(timer_timeout + SWQ_HWTIMER_TOLERANCE - priv->pshare->swq_current_timeout > SWQ_HWTIMER_MAXIMUN) {                
                    set_timer = 1;
                }
            }
            else {
                set_timer = 1;
            }

            if(set_timer) {
                if(current_time - priv->pshare->swq_last_tsf < SWQ_HWTIMER_DELAY) {
                    return 1;
                }
                set_timeout = RTL_MICROSECONDS_TO_GTIMERCOUNTER(set_timeout);
                set_timeout = BIT26 | BIT24 | (set_timeout & 0x00FFFFFF);  //TC40INT_EN | TC4EN | TC4Data
                RTL_W32(TC4_CTRL, set_timeout);

#ifdef CONFIG_WLAN_HAL
                if ( IS_HAL_CHIP(priv) ) {
                	GET_HAL_INTERFACE(priv)->GetHwRegHandler(priv, HW_VAR_TSF_TIMER, (pu1Byte)&priv->pshare->swq_last_tsf);
                } else
#endif
                {
                	priv->pshare->swq_last_tsf = RTL_R32(TSFTR1);
                }
                priv->pshare->swq_current_timeout = (timer_timeout == 0?1:timer_timeout);            
            }
#endif
        }
        else {
            if(priv->pshare->swq_current_timeout) {                          
                if(timer_timeout < priv->pshare->swq_current_timeout) {                
                    set_timer = 1;
                }                
            }
            else {            
                set_timer = 1;
            }

            if(set_timer) {                                
                priv->pshare->swq_current_timeout = timer_timeout;            
                mod_timer(&priv->pshare->swq_sw_timer, (unsigned long)timer_timeout);            
            }            
        }
    }
    return 0;
}


/* when using hw timer,  the parameter "add_timer_timeout" is TSF time
     when using sw timer, the parameter "add_timer_timeout" is jiffies time
*/
__inline__ static int rtl8192cd_swq_addtimer(struct rtl8192cd_priv *priv, struct stat_info* pstat, unsigned char qnum, UINT32 add_timer_timeout)
{
    unsigned short timer_index = (priv->pshare->swq_timer_head + 1) % SWQ_TIMER_NUM; 
    if(priv->pshare->swq_timer_tail == timer_index) { /* queue is full */
        /* find an empty slot */
        for(timer_index = 0 ; timer_index < SWQ_TIMER_NUM; timer_index++) {
            if(timer_index == priv->pshare->swq_timer_tail) {
                continue;
            }
            if(priv->pshare->swq_timer[timer_index].pstat == NULL) {
                 break;
            }
        }

        if(timer_index == SWQ_TIMER_NUM) {
            printk("%s: %s, swq timer overflow!\n", priv->dev->name, __FUNCTION__);
            return -1;
        }
    }else
        priv->pshare->swq_timer_head = timer_index;                 

    priv->pshare->swq_timer[timer_index].priv = priv;
    priv->pshare->swq_timer[timer_index].pstat = pstat;
    priv->pshare->swq_timer[timer_index].qnum = qnum;
    priv->pshare->swq_timer[timer_index].timeout = add_timer_timeout;
    pstat->swq.swq_timer_id[qnum] = timer_index + 1;

    return 0;
}


void rtl8192cd_swq_deltimer(struct rtl8192cd_priv *priv, struct stat_info* pstat, unsigned char qnum)
{
    unsigned char set_timer = 0;
    unsigned short temp_tail;
    unsigned short timer_index = pstat->swq.swq_timer_id[qnum]-1;
    UINT32 set_timer_timeout = 0;
    priv->pshare->swq_timer[timer_index].pstat = NULL;
    pstat->swq.swq_timer_id[qnum] = 0; 

    /*dequeu dummy slot */    
     while(priv->pshare->swq_timer_head != priv->pshare->swq_timer_tail) /*check if queue is empty*/
    {
        temp_tail = (priv->pshare->swq_timer_tail + 1) % SWQ_TIMER_NUM;
        if(priv->pshare->swq_timer[temp_tail].pstat != NULL) {
            break;
        }            
        priv->pshare->swq_timer_tail = temp_tail;
    }
  
#ifndef SMP_LOAD_BALANCE_SUPPORT
    if (priv->pshare->swq_timer_head != priv->pshare->swq_timer_tail) // if queue is not empty
    {
        temp_tail = priv->pshare->swq_timer_tail;
        do {            
            temp_tail = (temp_tail + 1) % SWQ_TIMER_NUM;
            if(priv->pshare->swq_timer[temp_tail].pstat != NULL) {
                if(priv->pshare->swq_use_hw_timer) { 
                    if(priv->pshare->swq_timer[temp_tail].timeout - (priv->pshare->swq_current_timeout + SWQ_HWTIMER_MINIMUN) > SWQ_HWTIMER_MAXIMUN) {
                        set_timer = 0;
                        break;
                    }
                    else {
                        if(set_timer == 0) {
                            set_timer_timeout = priv->pshare->swq_timer[temp_tail].timeout;
                        }
                        else {
                            if(priv->pshare->swq_timer[temp_tail].timeout - set_timer_timeout > SWQ_HWTIMER_MAXIMUN) {
                                set_timer_timeout = priv->pshare->swq_timer[temp_tail].timeout;
                            }                     
                        }
                        set_timer = 1;
                    }
                }
                else {
                    if(priv->pshare->swq_timer[temp_tail].timeout <= priv->pshare->swq_current_timeout) {
                        set_timer = 0;
                        break;
                    }
                    else {
                        if(set_timer == 0) {
                            set_timer_timeout = priv->pshare->swq_timer[temp_tail].timeout;
                        }
                        else {
                            if(priv->pshare->swq_timer[temp_tail].timeout < set_timer_timeout) {
                                set_timer_timeout = priv->pshare->swq_timer[temp_tail].timeout;
                            }
                        }                        
                        set_timer = 1;
                    }
                }                
            }
        }while(temp_tail != priv->pshare->swq_timer_head);

        if(set_timer) {
            rtl8192cd_swq_settimer(priv, 0);
            rtl8192cd_swq_settimer(priv, set_timer_timeout);   /*need to re-setup timer*/              
        }
    }
    else {
        rtl8192cd_swq_settimer(priv, 0);   /*disable timer*/
    }
#endif
}


#ifdef SUPPORT_TX_SWQ_AMSDU
__inline__ static int rtl8192cd_swq_dequeue_amsdu(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char q_num, unsigned int dqnum, unsigned char releaseALL, int max_size)
{
	struct sk_buff *pskb, *next_skb;
	int count = 0, qlen;
	int avail_txbd_num;
	int i;
	unsigned short protocol;
	int is_first=1, total_len=0, total_num=0;
	struct tx_insn tx_insn;
	struct net_device *wdsDev=NULL;
	int standalone;
	DECLARE_TXCFG(txcfg, tx_insn);
	
	txcfg->pstat = pstat;
	txcfg->q_num = q_num;
#ifdef WDS
	wdsDev = NULL;
	if (pstat->state & WIFI_WDS) {
		wdsDev = getWdsDevByAddr(priv, pstat->cmn_info.mac_addr);
		txcfg->wdsIdx = getWdsIdxByDev(priv, wdsDev);
	}
#endif

	qlen = skb_queue_len(&pstat->swq.swq_queue[q_num]);
	
	if (releaseALL)
		dqnum = qlen;
	else if (qlen < dqnum)
		dqnum = qlen;

#ifdef SUPPORT_TX_SWQ_ATM
	if (priv->pshare->swq_atm_state && qlen > pstat->swq_atm_max_deqnum) {
		dqnum = pstat->swq_atm_max_deqnum;
	}
#endif
	
	avail_txbd_num = getAvailableTXBD(priv, q_num) - 4;
	standalone = -1;		// unknown
	
	while (count < dqnum)
	{
		if (!releaseALL && avail_txbd_num <= 0)
			break;

		pskb = skb_dequeue(&pstat->swq.swq_queue[q_num]);
		next_skb = skb_peek(&pstat->swq.swq_queue[q_num]);
		
		total_len += (pskb->len + sizeof(struct llc_snap) + 3);
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
		pskb->cb[_SKB_CB_AMSDU_TXSC] = q_num;
#endif

		priv->ext_stats.swq_xmit_out_pkt++;
		priv->ext_stats.swq_real_deque_pkt++;
		priv->pshare->swq_cnt--;
#ifdef SUPPORT_TX_SWQ_ATM
		pstat->swq_atm_cnt--;
#endif
		if (pskb->len < SWQ_AMSDU_SMLPKT_LEN)
			pstat->sml_big_pkt_diff--;
		else 
			pstat->sml_big_pkt_diff++;
#ifdef SW_TXQ_RSVD_DESC
		priv->pshare->swq_skb_queue_cnt[q_num - 1]--;
#endif

		if (IS_MCAST(pskb->data) || (!IS_MCAST(pskb->data) && (pskb->cb[16]==TX_NO_MUL2UNI))) {
			standalone = -1;
			txcfg->aggre_en = 0;
			avail_txbd_num--;
			goto xmit_out;
		}

		if (standalone == -1) {
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
			if (OFFLOAD_ENABLE(priv))
				standalone = 0;
			else
#endif
			{
				protocol = ntohs(*((UINT16 *)((UINT8 *)pskb->data + ETH_ALEN*2)));
				if (((protocol + WLAN_ETHHDR_LEN) > WLAN_MAX_ETHFRM_LEN) &&
					 (skb_headroom(pskb) < sizeof(struct llc_snap)))
					standalone = 1;
				else
					standalone = 0;
			}
		}

		if ((standalone == 1) || (is_first && !next_skb) || (avail_txbd_num <= 0)) {
			standalone = -1;
			//Normal packet without AMSDU
			txcfg->aggre_en = 0;
			avail_txbd_num--;
		} else {
			if (next_skb) {
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
				if (OFFLOAD_ENABLE(priv))
					standalone = 0;
				else
#endif
				{
					protocol = ntohs(*((UINT16 *)((UINT8 *)next_skb->data + ETH_ALEN*2)));
					if (((protocol + WLAN_ETHHDR_LEN) > WLAN_MAX_ETHFRM_LEN) &&
						 (skb_headroom(next_skb) < sizeof(struct llc_snap)))
						standalone = 1;
					else
						standalone = 0;
				}
			}
			if (!next_skb || (standalone == 1)
					|| (total_len + next_skb->len + sizeof(struct llc_snap) + 3 > max_size)
					|| (total_num+1 >= priv->pmib->dot11nConfigEntry.dot11nCurAMSDUSendNum)
					|| (count+1 >= dqnum)
				) {
				if (is_first) {
					//Normal packet without AMSDU
					txcfg->aggre_en = 0;
				} else {
#ifdef SUPPORT_TX_AMSDU_SHORTCUT				
					pskb->cb[_SKB_CB_AMSDU_TXSC] |= (FG_AGGRE_MSDU_LAST << 4);
#endif
					txcfg->aggre_en = FG_AGGRE_MSDU_LAST;
					total_len = 0;
					is_first = 1;
					total_num = 0;
				}
				avail_txbd_num--;
			} else {
				if (is_first) {
#ifdef SUPPORT_TX_AMSDU_SHORTCUT	
					pskb->cb[_SKB_CB_AMSDU_TXSC] |= (FG_AGGRE_MSDU_FIRST << 4);
#endif
					txcfg->aggre_en = FG_AGGRE_MSDU_FIRST;
					is_first = 0;
				} else {
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
					pskb->cb[_SKB_CB_AMSDU_TXSC] |= (FG_AGGRE_MSDU_MIDDLE << 4);
#endif
					txcfg->aggre_en = FG_AGGRE_MSDU_MIDDLE;
				}
				total_num++;
			}
		}

xmit_out:
		
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
		if(!priv->pmib->dot11OperationEntry.disable_amsdu_txsc)
		{
#ifdef CONFIG_RTK_MESH
			__rtl8192cd_start_xmit_out(pskb, pstat, txcfg);
#else
			__rtl8192cd_start_xmit_out(pskb, pstat, NULL);
#endif
		}
		else
#endif
		{
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
			pstat->tx_sc_amsdu_pkts_slow++;
#endif
			rtl8192cd_tx_slowPath(priv, pskb, pstat, priv->dev, wdsDev, txcfg);
		}

		count++;
	}

	if (pstat->swq.swq_queue_xmit_max[q_num] < count)
		pstat->swq.swq_queue_xmit_max[q_num] = count;

	if (skb_queue_len(&pstat->swq.swq_queue[q_num]) == 0) {
		if(priv->pshare->swq_use_hw_timer && pstat->swq.swq_empty[q_num] == 1)
			priv->pshare->swq_numActiveSTA--;
		pstat->swq.swq_empty[q_num] = 0;		
	}
		
	return count;
}
#endif // SUPPORT_TX_SWQ_AMSDU


int tcp_ack_merge(struct rtl8192cd_priv *priv, struct sk_buff_head *queue)
{
	struct sk_buff *phead, *plist;
	struct sk_buff *skb1, *skb2;
	int num = 0;
	
	phead = (struct sk_buff *)queue;
	plist = phead->prev;
	
	while (plist != phead) {
		skb1 = plist;
		plist = plist->prev;
		
		if (skb1->cb[_SKB_CB_FLAGS] & _SKB_CB_FLAGS_TCPACK) {
			while (plist != phead) {
				skb2 = plist;
				plist = plist->prev;
				/* Drop duplicate TCP Ack if it's at the same tcp connection */
				if ((skb2->cb[_SKB_CB_FLAGS] & _SKB_CB_FLAGS_TCPACK)
					&& !memcmp(skb1->data+26, skb2->data+26, 12)) {
					__skb_unlink(skb2, queue);
					rtl_kfree_skb(priv, skb2, _SKB_TX_);
					num++;
				}
			}
			plist = skb1->prev;
		}
	}

	return num;
}


/*return: 1: not finished,  0: the queue is empty now or txbd is not enough now*/
#if (!defined(__OSK__)) || (defined(__OSK__) && !defined(CONFIG_RTL6028))
__MIPS16
#endif
__IRAM_IN_865X
__inline__ static int rtl8192cd_swq_dequeue(struct rtl8192cd_priv *priv, struct stat_info *pstat,  unsigned char qnum, unsigned int dqnum, unsigned char releaseALL)
{
	struct sk_buff *tmpskb;
	int count, qlen;

	qlen = skb_queue_len(&pstat->swq.swq_queue[qnum]);

	if(pstat->swq.swq_queue_len_max[qnum] < qlen)
		pstat->swq.swq_queue_len_max[qnum] = qlen;

#ifdef SUPPORT_TX_SWQ_AMSDU
	if (priv->pshare->rf_ft_var.tcpack_agg) {
		if (pstat->rx_avarage > (8 << 17) && pstat->rx_avarage > (pstat->tx_avarage << 3)) {
			count = tcp_ack_merge(priv, &pstat->swq.swq_queue[qnum]);
			if (count) {
				priv->ext_stats.swq_xmit_out_pkt += count;
				priv->ext_stats.swq_real_deque_pkt += count;
				priv->pshare->swq_cnt -= count;
#ifdef SUPPORT_TX_SWQ_ATM
				pstat->swq_atm_cnt -= count;
#endif				
				pstat->sml_big_pkt_diff -= count;
#ifdef SW_TXQ_RSVD_DESC
				priv->pshare->swq_skb_queue_cnt[qnum - 1] -= count;
#endif
			}
		}
	}
	
	if (pstat->amsdu_level && !(pstat->state & WIFI_SLEEP_STATE)) {
		if (pstat->aggre_mthd & AGGRE_MTHD_MSDU)
			return rtl8192cd_swq_dequeue_amsdu(priv, pstat, qnum, dqnum, releaseALL, pstat->amsdu_level);
		else if (priv->pshare->rf_ft_var.amsdusmallpkts && pstat->sml_big_pkt_diff > 0)
			return rtl8192cd_swq_dequeue_amsdu(priv, pstat, qnum, dqnum, releaseALL, 1572);	// 512*3 + 36
	}
#endif

#ifdef SUPPORT_TX_SWQ_ATM
	if (priv->pshare->swq_atm_state && qlen > pstat->swq_atm_max_deqnum) {
		dqnum = pstat->swq_atm_max_deqnum;
	}
#endif

	count = 0;
	while (1)
	{
		if(!releaseALL && count >= dqnum)
			break;
	
		if(!releaseALL && rtl8192cd_swq_bdfull(priv, 5, qnum))
			break;

		tmpskb = skb_dequeue(&pstat->swq.swq_queue[qnum]);
		if (tmpskb == NULL)
			break;

#ifdef TX_EARLY_MODE
		if (GET_TX_EARLY_MODE && GET_EM_SWQ_ENABLE) 
			get_tx_early_info(priv, pstat, &pstat->swq.swq_queue[qnum]);
#endif

		priv->ext_stats.swq_xmit_out_pkt++;
		priv->ext_stats.swq_real_deque_pkt++;
		priv->pshare->swq_cnt--;
#ifdef SUPPORT_TX_SWQ_ATM
		pstat->swq_atm_cnt--;
#endif		
#ifdef SUPPORT_TX_SWQ_AMSDU
		if (tmpskb->len < SWQ_AMSDU_SMLPKT_LEN)
			pstat->sml_big_pkt_diff--;
		else
			pstat->sml_big_pkt_diff++;
#endif
#ifdef SW_TXQ_RSVD_DESC
		priv->pshare->swq_skb_queue_cnt[qnum - 1]--;
#endif

		__rtl8192cd_start_xmit_out(tmpskb, pstat, NULL);
		count++;

#if defined(WIFI_LOGO_11N_4_2_47) && !defined(SMP_LOAD_BALANCE_SUPPORT)
		if(priv->is_ssid_ps && list_empty(&pstat->sleep_list))
			break;
#endif
	}

	if(pstat->swq.swq_queue_xmit_max[qnum] < count)
		pstat->swq.swq_queue_xmit_max[qnum] = count;

	if(skb_queue_len(&pstat->swq.swq_queue[qnum]) == 0) {
		if(priv->pshare->swq_use_hw_timer && pstat->swq.swq_empty[qnum] == 1)
			priv->pshare->swq_numActiveSTA--;
		pstat->swq.swq_empty[qnum] = 0;
	}

	return count;
}

#define MBPS_TO_BIT(mbps) (mbps*125000)
#define KBPS_TO_BIT(kbps) (kbps*125)  
__inline__ static UINT32 get_swq_tri_time_using_sw_timer(struct stat_info *pstat)
{
    UINT32 tri_time;
    
    if (pstat->tx_avarage > MBPS_TO_BIT(15))         //15M~
        tri_time = 1;
    else if (pstat->tx_avarage > MBPS_TO_BIT(4))    //4M~15M
        tri_time = 30;
    else if (pstat->tx_avarage > MBPS_TO_BIT(2))    //2M~4M
        tri_time = 90;
    else if (pstat->tx_avarage > KBPS_TO_BIT(800))  //800K~2M
        tri_time = 120;
    else
        tri_time = 10;
        
    return tri_time;
}


#define GET_TRI_TIME_VALUE(tri_time,skb,priv,num_of_tri_time)              \
    do {                                                                     \
		if (IS_UDP_PROTO(skb->data))                                         \
			 tri_time =priv->pshare->rf_ft_var.udp_tri_time##num_of_tri_time; \
		else                                                                  \
			 tri_time =priv->pshare->rf_ft_var.tri_time##num_of_tri_time;     \
	} while (0)    

unsigned int get_swq_turbo_thd(struct rtl8192cd_priv *priv)
{
	int result;

	if (priv->pshare->rf_ft_var.manual_swqturbothd) {
		result = priv->pshare->rf_ft_var.manual_swqturbothd;
	} else if (is_veriwave_testing(priv)) {
		result = 4;
	} else {
		result = 8;
	}

	return result;
}

__inline__ static UINT32 get_swq_tri_time_using_hw_timer(struct rtl8192cd_priv *priv, struct sk_buff *skb, struct stat_info *pstat)
{
    /*please reference to JIRA:APDEVELOP-181*/
    
    UINT32 tri_time;

    if (priv->pshare->swq_numActiveSTA > get_swq_turbo_thd(priv) && (priv->up_time - priv->pshare->swq_turbo_time) < priv->pshare->rf_ft_var.swqmaxturbotime){
		tri_time = 20;
    }
	else if (priv->pshare->traffic_mode == TRAFFIC_MODE_RX){
		tri_time = priv->pshare->rf_ft_var.tri_time_rx;
	}	
	else if (pstat->tx_avarage > MBPS_TO_BIT(110)){      // 110M~
        GET_TRI_TIME_VALUE(tri_time,skb,priv,1);
    }
	else if (pstat->tx_avarage > MBPS_TO_BIT(15)){       //15M~110M
		GET_TRI_TIME_VALUE(tri_time,skb,priv,2);
	}
	else if (pstat->tx_avarage > MBPS_TO_BIT(5)){        //5M~15M
		GET_TRI_TIME_VALUE(tri_time,skb,priv,3);
	}
	else if (pstat->tx_avarage > KBPS_TO_BIT(2200)){      //2.2M~5M
		GET_TRI_TIME_VALUE(tri_time,skb,priv,4);
	}
    else if (pstat->tx_avarage > MBPS_TO_BIT(2)){        //2M~2.2M
		GET_TRI_TIME_VALUE(tri_time,skb,priv,5);
    }
	else if (pstat->tx_avarage > KBPS_TO_BIT(800)){      //800K~2M
		GET_TRI_TIME_VALUE(tri_time,skb,priv,6);
	}
	else if (pstat->tx_avarage > KBPS_TO_BIT(200)){      //200K~800K
		GET_TRI_TIME_VALUE(tri_time,skb,priv,7);
	}
	else{
		GET_TRI_TIME_VALUE(tri_time,skb,priv,8);
	}

    return tri_time;
}

__inline__ static UINT32 get_swq_tri_time(struct rtl8192cd_priv *priv, struct sk_buff *skb, struct stat_info *pstat, int q_num)
{
    UINT32 tri_time;
    
    if(priv->pshare->swq_use_hw_timer) {
#if defined(WIFI_LOGO_11N_4_2_47) && !defined(SMP_LOAD_BALANCE_SUPPORT)
        if(priv->is_ssid_ps && list_empty(&pstat->sleep_list)) {
            tri_time = 500;		
        } else
#endif
        if (skb_queue_len(&pstat->swq.swq_queue[q_num]) >= pstat->swq.q_aggnum[q_num]) {
            tri_time = 1;
        } else {
            tri_time = get_swq_tri_time_using_hw_timer(priv,skb,pstat);
            pstat->swq.swq_tri_time[q_num] = tri_time;
	}
           
    	if(pstat->swq.swq_prev_timeout[q_num] != tri_time) {
    		pstat->swq.swq_prev_timeout[q_num] = tri_time;
    		pstat->swq.swq_timeout_change[q_num] = 1;                                 
    	}  

        //get hw timer setting
#ifdef CONFIG_WLAN_HAL
	if ( IS_HAL_CHIP(priv) ) {
		UINT32 tsf_time;
		GET_HAL_INTERFACE(priv)->GetHwRegHandler(priv, HW_VAR_TSF_TIMER, (pu1Byte)&tsf_time);
		tri_time = tsf_time + tri_time*1000;
	} else
#endif
	{
        	tri_time = RTL_R32(TSFTR1) + tri_time*1000;
    	} 
    } 
    else {
        tri_time = get_swq_tri_time_using_sw_timer(pstat);
        pstat->swq.swq_tri_time[q_num] = tri_time;

        tri_time = jiffies + RTL_MILISECONDS_TO_JIFFIES(tri_time);
    }

    return tri_time;   
}

__inline__ static int rtl8192cd_swq_enqueue(struct rtl8192cd_priv *priv, struct sk_buff *skb, struct stat_info	*pstat)
{
#if (MU_BEAMFORMING_SUPPORT == 1)	
#define for_each_mu_partner(pstat, psta_mu) \
	for (pn = 0, psta_mu = pstat->muPartner[0]; pn < pstat->muPartner_num; psta_mu = pstat->muPartner[++pn])
#endif

	int q_len, pri, q_num;
	unsigned char need_deque = 0;
#if (MU_BEAMFORMING_SUPPORT == 1)	
	unsigned char need_deque_mu = 0;
	PRT_BEAMFORMING_INFO	pBeamInfo = &(priv->pshare->BeamformingInfo);
#endif
	unsigned char change = 0;
	UINT32 tri_time;
	int i, pn;

#if defined(CONFIG_RTL8672) || defined(NOT_RTK_BSP)
	unsigned int tp_avg_low = 200000;
	unsigned char assoc_num_low = 4;
#else
	unsigned int tp_avg_low = 0;
	unsigned char assoc_num_low = get_swq_turbo_thd(priv);
#endif

#if (MU_BEAMFORMING_SUPPORT == 1)
    /*
     * HH71VM: was `pstat->muPartner`, became `pstat->muPartner[0]`.
     * In this revision SDK the field of struct stat_info is an ARRAY
     * (`struct stat_info *muPartner[MAX_NUM_MU_PARTNER]`, 8192cd.h:3068); scalar
     * `muPartner` remained only with another structure, stat_info_pe (8192cd.h:2433).
     * Here the string was not updated to the array, so the initialization was assigning
     * `struct stat_info **` variable of type `struct stat_info *`.
     * That the form with [0] is correct can be seen from the vendor himself: macro for_each_mu_partner
     * 23 lines above starts with `psta_mu = pstat->muPartner[0]`, and the same is written in
     * 8192cd_sme.c:7613 and Beamforming.c:3548.
     * This is compiled because MU_BEAMFORMING_SUPPORT becomes 1 from the mere
     * presence of CONFIG_WLAN_HAL_8812FE (8192cd_cfg.h:1629), this cannot be disabled by the config.
     */
    struct stat_info *psta_mu = pstat ? pstat->muPartner[0] : NULL;
#endif

	pri = skb->cb[_SKB_CB_PRIORITY];
	q_num = skb->cb[_SKB_CB_QNUM];

#ifdef SUPPORT_TX_SWQ_ATM
	if (priv->pshare->swq_atm_state && pstat->swq_atm_cnt >= pstat->swq_atm_max_enqnum) {
		priv->ext_stats.swq_overflow_drop_pkt++;
		priv->ext_stats.tx_drops++;
		pstat->swq_atm_drops++;
		rtl_kfree_skb(priv, skb, _SKB_TX_);
		goto exit;	
	}
#endif

	if(priv->pshare->swq_use_hw_timer) {
#if defined(WIFI_LOGO_11N_4_2_47) && !defined(SMP_LOAD_BALANCE_SUPPORT)
		//For wifi logo 11n 4.2.47, slower tx before entering sleep mode
		if(priv->is_ssid_ps && list_empty(&pstat->sleep_list)) { 
			pstat->swq.swq_en[q_num] = 1; /*enable swq*/ 
			pstat->swq.swq_prev_timeout[q_num] = 1;
			pstat->swq.swq_timeout_change[q_num] = 0;
			pstat->swq.swq_keeptime[q_num] = 0;    		
		 } else	  
#endif	
#if (MU_BEAMFORMING_SUPPORT == 1)
		if(pstat->muPartner_num){
			if(pstat->swq.swq_en[q_num]) {
				unsigned char cond_flag = 0;
				for_each_mu_partner(pstat, psta_mu) {
					if (psta_mu->tx_avarage < (MBPS_TO_BIT(priv->pshare->rf_ft_var.mutp_th_lower))) {
						cond_flag = 1;
						break;
					}
				}
				if(pstat->tx_avarage < (MBPS_TO_BIT(priv->pshare->rf_ft_var.mutp_th_lower)) || cond_flag) 
				{
					pstat->swq.swq_en[q_num] = 0; /*disable swq*/
					if (pstat->swq.swq_timer_id[q_num])
						rtl8192cd_swq_deltimer(priv, pstat, q_num);
					for_each_mu_partner(pstat, psta_mu) {
						if (psta_mu->swq.swq_timer_id[q_num])
							rtl8192cd_swq_deltimer(priv, psta_mu, q_num);
					}
					if(pstat->swq.swq_empty[q_num]) {
						rtl8192cd_swq_dequeue(priv, pstat, q_num, 0, SWQ_DEQ_ALL);                          
					}
					for_each_mu_partner(pstat, psta_mu) {
						if(psta_mu->swq.swq_empty[q_num]) {
	            	    	rtl8192cd_swq_dequeue(priv, psta_mu, q_num, 0, SWQ_DEQ_ALL);                          
	            		}
					}
				}
			} else {
				unsigned char cond_flag = 1;
				for_each_mu_partner(pstat, psta_mu) {
					if ( !(psta_mu->tx_avarage > (MBPS_TO_BIT(priv->pshare->rf_ft_var.mutp_th_up))) ) {
						cond_flag = 0;
						break;
					}
				}
				if(pstat->tx_avarage > (MBPS_TO_BIT(priv->pshare->rf_ft_var.mutp_th_up)) && cond_flag) {
					pstat->swq.swq_en[q_num] = 1; /*enable swq*/ 
		        	pstat->swq.swq_prev_timeout[q_num] = 1;
		        	pstat->swq.swq_timeout_change[q_num] = 0;
		        	pstat->swq.swq_keeptime[q_num] = 0;   
					for_each_mu_partner(pstat, psta_mu) {
						psta_mu->swq.swq_en[q_num] = 1; /*enable swq*/ 
			        	psta_mu->swq.swq_prev_timeout[q_num] = 1;
			        	psta_mu->swq.swq_timeout_change[q_num] = 0;
			        	psta_mu->swq.swq_keeptime[q_num] = 0;
					}
				}
			}
		} else
#endif
#if defined(SUPPORT_TX_SWQ_AMSDU) || defined(SUPPORT_TX_SWQ_ATM)
		{
			if(!pstat->swq.swq_en[q_num]) {
				pstat->swq.swq_en[q_num] = 1; /*enable swq*/
				pstat->swq.swq_prev_timeout[q_num] = 1;
				pstat->swq.swq_timeout_change[q_num] = 0;
				pstat->swq.swq_keeptime[q_num] = 0;
			}
		}
#else
		{
			unsigned int assoc_num = priv->pshare->total_assoc_num;
		
			/* IOT for some STAs Rx test */
            /* Rx Check : Rx > 10Mb && Rx > 5 * Tx  */
			if ((MBPS_TO_BIT(10) < pstat->rx_avarage) && ((pstat->tx_avarage * 5) < pstat->rx_avarage))
		    {
    			//Rx mode, force data dequeue
    			if (pstat->swq.swq_en[q_num]) {

                   //force dequeue
                    need_deque = 1;
                }
    		}
			else if (priv->pshare->swq_en == 3) {	// for veriwave debug
				pstat->swq.swq_en[q_num] = 1;
				pstat->swq.swq_prev_timeout[q_num] = 1;
				pstat->swq.swq_timeout_change[q_num] = 0;
				pstat->swq.swq_keeptime[q_num] = 0;
			}
			/* (Assoc number > 4 and TP > 200KB) or (5.5Mb < TP < 155Mb) */
			else if((assoc_num > assoc_num_low && pstat->tx_avarage >= tp_avg_low) || (KBPS_TO_BIT(5500) < pstat->tx_avarage && pstat->tx_avarage < MBPS_TO_BIT(155))) {
				if(!pstat->swq.swq_en[q_num]) {
					pstat->swq.swq_en[q_num] = 1;
					pstat->swq.swq_prev_timeout[q_num] = 1;
					pstat->swq.swq_timeout_change[q_num] = 0;
					pstat->swq.swq_keeptime[q_num] = 0;
				}
			}
			else {
#if defined(CONFIG_RTL8672) || defined(NOT_RTK_BSP)
				if(pstat->swq.swq_en[q_num]) {
					if(!rtl8192cd_swq_bdfull(priv, CURRENT_NUM_TX_DESC/2, q_num)) {
						change = 1;
					}
				}
				else {
					if(rtl8192cd_swq_bdfull(priv, 32, q_num)) {
						change = 2;
					}
				}
				if(change) {
					if(pstat->swq.swq_en[q_num]) {
						if (pstat->swq.swq_timer_id[q_num])
							rtl8192cd_swq_deltimer(priv, pstat, q_num);
					
						skb_queue_tail(&pstat->swq.swq_queue[q_num], skb);
						priv->ext_stats.swq_real_enque_pkt++;
						priv->pshare->swq_cnt++;
#ifdef SUPPORT_TX_SWQ_ATM
						pstat->swq_atm_cnt++;
#endif						
#ifdef SUPPORT_TX_SWQ_AMSDU
						if (skb->len < SWQ_AMSDU_SMLPKT_LEN)
							pstat->sml_big_pkt_diff++;
						else
							pstat->sml_big_pkt_diff--;
#endif
#ifdef SW_TXQ_RSVD_DESC
						priv->pshare->swq_skb_queue_cnt[q_num - 1]++;
#endif
						if(pstat->swq.swq_empty[q_num] == 0) {
							pstat->swq.swq_empty[q_num] = 1;
							priv->pshare->swq_numActiveSTA++;
							if(priv->pshare->swq_numActiveSTA == (assoc_num_low + 1)) {
								turbo_swq_setting(priv);
								priv->pshare->swq_turbo_time = priv->up_time;
							}
						}

						if(!priv->pshare->rf_ft_var.swq_max_xmit)
							rtl8192cd_swq_dequeue(priv, pstat, q_num, priv->pshare->rf_ft_var.swq_max_xmit, SWQ_DEQ_LIMIT);
						else
							rtl8192cd_swq_dequeue(priv, pstat, q_num, skb_queue_len(&pstat->swq.swq_queue[q_num]), SWQ_DEQ_LIMIT);
					
						if(pstat->swq.swq_empty[q_num] == 0) {
							pstat->swq.swq_en[q_num] = 0;
						}
					}
					else {
						pstat->swq.swq_en[q_num] = 1;
						pstat->swq.swq_prev_timeout[q_num] = 1;
						pstat->swq.swq_timeout_change[q_num] = 0;
						pstat->swq.swq_keeptime[q_num] = 0;
					}
				}	
#else
				/* 5Mb > TP || 160Mb < TP */
				if(assoc_num <= assoc_num_low && (pstat->tx_avarage < MBPS_TO_BIT(5) || MBPS_TO_BIT(160) < pstat->tx_avarage)) {
					if(pstat->swq.swq_en[q_num]) {
						pstat->swq.swq_en[q_num] = 0;
						if (pstat->swq.swq_timer_id[q_num])
							rtl8192cd_swq_deltimer(priv, pstat, q_num);
						if(pstat->swq.swq_empty[q_num])
							rtl8192cd_swq_dequeue(priv, pstat, q_num, 0, SWQ_DEQ_ALL);
					}
				}
#endif	
			}
		}
#endif // SUPPORT_TX_SWQ_AMSDU
		if(pstat->swq.swq_en[q_num] == 0) {
			if(change == 0) {
				priv->ext_stats.swq_xmit_out_pkt++;
				__rtl8192cd_start_xmit_out(skb, pstat, NULL);
			}
            goto exit;
		}
	}
	
	pstat->swq.q_used[q_num] = 1;

	/* queue the packet */
	if(change != 1) {
		if (!pstat->ADDBA_ready[pri]
#ifdef SUPPORT_TX_SWQ_ATM
			&& !priv->pshare->swq_atm_state
#endif
		)
		{
			need_deque = 1;
		} else if(need_deque == 0) {
			q_len = skb_queue_len(&pstat->swq.swq_queue[q_num]);
			#ifdef TX_EARLY_MODE
			if (q_len + 1 >= (GET_EM_SWQ_ENABLE ? MAX_EM_QUE_NUM : pstat->swq.q_aggnum[q_num]))
			#else
			if (q_len + 1 >= pstat->swq.q_aggnum[q_num])
			#endif		
			{
				need_deque = 1;
			}
		}

		skb_queue_tail(&pstat->swq.swq_queue[q_num], skb);
		priv->ext_stats.swq_real_enque_pkt++;
		priv->pshare->swq_cnt++;
#ifdef SUPPORT_TX_SWQ_ATM
		pstat->swq_atm_cnt++;
#endif		
#ifdef SUPPORT_TX_SWQ_AMSDU
		if (skb->len < SWQ_AMSDU_SMLPKT_LEN)
			pstat->sml_big_pkt_diff++;
		else
			pstat->sml_big_pkt_diff--;
#endif
#ifdef SW_TXQ_RSVD_DESC
		priv->pshare->swq_skb_queue_cnt[q_num - 1]++;
#endif
		if(priv->pshare->swq_use_hw_timer && pstat->swq.swq_empty[q_num] == 0) {
			priv->pshare->swq_numActiveSTA++;
			if(priv->pshare->swq_numActiveSTA == (assoc_num_low + 1)) {
				turbo_swq_setting(priv);
				priv->pshare->swq_turbo_time = priv->up_time;
			}
		}
		pstat->swq.swq_empty[q_num] = 1;	
		
#if defined(WIFI_LOGO_11N_4_2_47) && !defined(SMP_LOAD_BALANCE_SUPPORT)
		if(priv->is_ssid_ps && list_empty(&pstat->sleep_list))
			need_deque = 0;
#endif

#if (MU_BEAMFORMING_SUPPORT == 1)	
	
		if (pBeamInfo->beamformee_mu_cnt >= 2 && pstat->muPartner_num) {
			unsigned int qlen;
			unsigned char need_fire = 1;		

			qlen = skb_queue_len(&pstat->swq.swq_queue[q_num]);
			
			if(qlen <= pstat->mu_deq_num){
				need_fire = 0;
			}	
			else {
				for_each_mu_partner(pstat, psta_mu) {
					if (psta_mu) {
						qlen = skb_queue_len(&psta_mu->swq.swq_queue[q_num]);
						if (qlen <= psta_mu->mu_deq_num) {
							need_fire = 0;
							break;
						}
					}
				}
			}

			if(need_fire){
				need_deque_mu = 1;
			}
		}

		if(need_deque_mu) {
#if defined( CONFIG_WLAN_HAL_8822BE ) || defined(CONFIG_WLAN_HAL_8812FE)
			struct stat_info *psta[2];
			unsigned int qlen, qlen2;
			if(pstat->muPartner[0]) {
				psta[0] = pstat;
				psta[1] = pstat->muPartner[0];	
			
				qlen = skb_queue_len(&psta[0]->swq.swq_queue[q_num]);
				qlen2 = skb_queue_len(&psta[1]->swq.swq_queue[q_num]);
		
				if (psta[0]->swq.swq_timer_id[q_num])
					rtl8192cd_swq_deltimer(priv, psta[0], q_num);
				if (psta[1]->swq.swq_timer_id[q_num])
					rtl8192cd_swq_deltimer(priv, psta[1], q_num);
				

				
				if (priv->pshare->rf_ft_var.dqnum){
					if(qlen >= psta[0]->mu_deq_num && qlen2 >= psta[1]->mu_deq_num){
						int count = psta[0]->mu_deq_num, count2 = psta[1]->mu_deq_num;
						int deq_num, sta_deq_num[2];

						priv->cnt_sta1_sta2++;

						if(count > 0 && count2 > 0) {
							if(count > count2)
								deq_num = (count / count2) + 1;
							else
								deq_num = (count2 / count) + 1;
						} else {
							panic_printk("mu_deq_num = 0\n");
						}

						while (1)
					    {
					    	if(rtl8192cd_swq_bdfull(priv, 5, q_num)) {
								break;
					    	}
							if(count > 0 && count2 > 0) {
								if(count == count2) {
									sta_deq_num[0] = 1;
									sta_deq_num[1] = 1;
									count--;
									count2--;
								} else if(count > count2 && count >= deq_num) {
									sta_deq_num[0] = deq_num;
									sta_deq_num[1] = 1;
									count -= deq_num;
									count2 -= 1;
								} else if(count2 > count && count2 >= deq_num) {
									sta_deq_num[0] = 1;
									sta_deq_num[1] = deq_num;
									count -= 1;
									count2 -= deq_num;
								} else {
									sta_deq_num[0] = count;
									sta_deq_num[1] = count2;
									count = 0;
									count2 = 0;
								}
								if(rtl8192cd_swq_bdfull(priv, sta_deq_num[0]+sta_deq_num[1], q_num)) {           
									break;
					    		}
								rtl8192cd_swq_dequeue(priv, psta[0], q_num, sta_deq_num[0], SWQ_DEQ_LIMIT);
								rtl8192cd_swq_dequeue(priv, psta[1], q_num, sta_deq_num[1], SWQ_DEQ_LIMIT);
									
							} else if(count > 0) {
								rtl8192cd_swq_dequeue(priv, psta[0], q_num, count, SWQ_DEQ_LIMIT);
								count = 0;
							} else if(count2 > 0) {
								rtl8192cd_swq_dequeue(priv, psta[1], q_num, count2, SWQ_DEQ_LIMIT);
								count2 = 0;
							} else if(count == 0 && count2 == 0) {
								qlen = skb_queue_len(&psta[0]->swq.swq_queue[q_num]);
								qlen2 = skb_queue_len(&psta[1]->swq.swq_queue[q_num]);
								if(qlen < psta[0]->mu_deq_num || qlen2 < psta[1]->mu_deq_num)
									break;
								else {
									count = psta[0]->mu_deq_num;
									count2 = psta[1]->mu_deq_num;
								}
							} else {
								panic_printk("%s (%d) Wrong case, count=%d, count2=%d\n", __FUNCTION__, __LINE__, count, count2);
								break;
							}
					    }					
					}
					else if(qlen >= (priv->pshare->rf_ft_var.dqnum)){					
						priv->cnt_sta1_only++;
						rtl8192cd_swq_dequeue(priv, psta[0], q_num, psta[0]->mu_deq_num, SWQ_DEQ_LIMIT);
					} else if(qlen2 >= (priv->pshare->rf_ft_var.dqnum)){
						priv->cnt_sta2_only++;
						rtl8192cd_swq_dequeue(priv, psta[1], q_num, psta[1]->mu_deq_num, SWQ_DEQ_LIMIT);
					}
				}
				else{
					if(psta[0]->swq.swq_empty[q_num]) {
						rtl8192cd_swq_dequeue(priv, psta[0], q_num, psta[0]->mu_deq_num, SWQ_DEQ_LIMIT);
					}
					if(psta[1]->swq.swq_empty[q_num]) {
						rtl8192cd_swq_dequeue(priv, psta[1], q_num, psta[1]->mu_deq_num, SWQ_DEQ_LIMIT);
					}

					priv->ext_stats.swq_xmit_out_pkt++;
				}
			}
#endif
#ifdef CONFIG_WLAN_HAL_8814BE
			struct stat_info *psta[1 + MAX_NUM_MU_PARTNER];
			unsigned int psta_num = 1;
			psta[0] = pstat;
			for_each_mu_partner(pstat, psta_mu) {
				psta[psta_num++] = psta_mu;
			}
			for (i = 0; i < psta_num; i++) {
				if (psta[i]->swq.swq_timer_id[q_num])
					rtl8192cd_swq_deltimer(priv, psta[i], q_num);
				if (psta[i]->swq.swq_empty[q_num])
					rtl8192cd_swq_dequeue(priv, psta[i], q_num, psta[i]->mu_deq_num, SWQ_DEQ_LIMIT);
			}
			priv->ext_stats.swq_xmit_out_pkt++;
#endif
		} else
#endif
	    if(need_deque) {
	        if (pstat->swq.swq_timer_id[q_num])
	            rtl8192cd_swq_deltimer(priv, pstat, q_num);
	        if (rtl8192cd_swq_bdfull(priv, (pstat->swq.q_aggnum[q_num] + 1) * 2, q_num))
	            rtl8192cd_tx_queueDsr(priv, q_num);
	        if (!rtl8192cd_swq_bdfull(priv, (pstat->swq.q_aggnum[q_num] + 1) * 2, q_num)) {
				if(!priv->pshare->rf_ft_var.swq_max_xmit)
					rtl8192cd_swq_dequeue(priv, pstat, q_num, skb_queue_len(&pstat->swq.swq_queue[q_num]), SWQ_DEQ_LIMIT);
				else	
					rtl8192cd_swq_dequeue(priv, pstat, q_num, priv->pshare->rf_ft_var.swq_max_xmit, SWQ_DEQ_LIMIT);
	        }
	    }
	}

#if (MU_BEAMFORMING_SUPPORT == 1)
	if(pstat->muPartner_num) {
		if(pstat->swq.swq_empty[q_num]) { /*queue not empty, add timer for it*/        
	        if (pstat->swq.swq_timer_id[q_num] == 0)
	        {
				if(priv->pshare->swq_use_hw_timer) {
					if(priv->pshare->rf_ft_var.qtime)
						tri_time = priv->pshare->rf_ft_var.qtime;
					if(pstat->swq.swq_prev_timeout[q_num] != tri_time) {
	                    pstat->swq.swq_prev_timeout[q_num] = tri_time;
	                    pstat->swq.swq_timeout_change[q_num] = 1;                                 
	                }  

#ifdef CONFIG_WLAN_HAL
					if ( IS_HAL_CHIP(priv) ) {
						UINT32 tsf_time;
						GET_HAL_INTERFACE(priv)->GetHwRegHandler(priv, HW_VAR_TSF_TIMER, (pu1Byte)&tsf_time);
						tri_time = tsf_time + tri_time*1000;
					} else
#endif
					{
	                			tri_time = RTL_R32(TSFTR1) + tri_time*1000;
					}
				} else {
					tri_time = jiffies + RTL_MILISECONDS_TO_JIFFIES(tri_time);
				}
				rtl8192cd_swq_addtimer(priv, pstat, q_num, tri_time);           
	            pstat->swq.swq_timer_status[q_num] = rtl8192cd_swq_settimer(priv, tri_time);   /*need to re-setup timer*/                        
	        } else if(pstat->swq.swq_timer_status[q_num]) { /*re-set the timer*/
            	pstat->swq.swq_timer_status[q_num] = rtl8192cd_swq_settimer(priv, priv->pshare->swq_timer[pstat->swq.swq_timer_id[q_num] - 1].timeout); 
        	}			
		}
		for_each_mu_partner(pstat, psta_mu) {
			if(psta_mu->swq.swq_empty[q_num]) { /*queue not empty, add timer for it*/        
		        if (psta_mu->swq.swq_timer_id[q_num] == 0)
		        {
					if(priv->pshare->swq_use_hw_timer) {
						if(priv->pshare->rf_ft_var.qtime)
							tri_time = priv->pshare->rf_ft_var.qtime;
						if(psta_mu->swq.swq_prev_timeout[q_num] != tri_time) {
		                    psta_mu->swq.swq_prev_timeout[q_num] = tri_time;
		                    psta_mu->swq.swq_timeout_change[q_num] = 1;                                 
		                }  

#ifdef CONFIG_WLAN_HAL
					if ( IS_HAL_CHIP(priv) ) {
						UINT32 tsf_time;
						GET_HAL_INTERFACE(priv)->GetHwRegHandler(priv, HW_VAR_TSF_TIMER, (pu1Byte)&tsf_time);
						tri_time = tsf_time + tri_time*1000;
					} else
#endif
					{
		                		tri_time = RTL_R32(TSFTR1) + tri_time*1000;
					}
				} else {
					tri_time = jiffies + RTL_MILISECONDS_TO_JIFFIES(tri_time);
				}
					rtl8192cd_swq_addtimer(priv, psta_mu, q_num, tri_time);           
		            psta_mu->swq.swq_timer_status[q_num] = rtl8192cd_swq_settimer(priv, tri_time);   /*need to re-setup timer*/                        
		        } else if(psta_mu->swq.swq_timer_status[q_num]) { /*re-set the timer*/
	            	psta_mu->swq.swq_timer_status[q_num] = rtl8192cd_swq_settimer(priv, priv->pshare->swq_timer[psta_mu->swq.swq_timer_id[q_num] - 1].timeout); 
	        	}
			}
		}
	} 
	else
#endif	
	{
		if(pstat->swq.swq_empty[q_num]) { /* queue is not empty, add timer for it */		
			if (pstat->swq.swq_timer_id[q_num] == 0)
			{   
                tri_time = get_swq_tri_time(priv,skb,pstat,q_num);
				if (tri_time == 0)
					tri_time = 1;

                rtl8192cd_swq_addtimer(priv, pstat, q_num, tri_time);           
				pstat->swq.swq_timer_status[q_num] = rtl8192cd_swq_settimer(priv, tri_time);   /*need to re-setup timer*/                        
			} else if(pstat->swq.swq_timer_status[q_num]) { /*re-set the timer*/
				pstat->swq.swq_timer_status[q_num] = rtl8192cd_swq_settimer(priv, priv->pshare->swq_timer[pstat->swq.swq_timer_id[q_num] - 1].timeout); 
			}
		}
	}
	
exit:  
    return 0;
}

void rtl8192cd_swq_timeout(unsigned long task_priv)
{
	struct rtl8192cd_priv       *priv = (struct rtl8192cd_priv *)task_priv;
	struct sw_tx_queue_timer    *swq_timer;
	int head;
	unsigned char need_dequeue;
	unsigned char set_timer = 0;
	
	UINT32 add_timer_timeout;
	UINT32 set_timer_timeout = 0;
	struct stat_info      *pstat;
	struct rtl8192cd_priv *priv_this = NULL;
	unsigned char qnum;	
	UINT32        current_time;
	int           bdfull;
	unsigned long x = 0;

#ifdef SMP_LOAD_BALANCE_SUPPORT
	priv->pshare->swq_timeout_seq++;
	rtl8192cd_xmit_dsr(task_priv);
	priv->pshare->swq_timeout_seq++;
#endif

	SMP_LOCK_XMIT(x);
	SAVE_INT_AND_CLI(x);
	
	priv->pshare->swq_current_timeout = 0;
	if(priv->pshare->swq_use_hw_timer)
	{
#ifdef CONFIG_WLAN_HAL
		if ( IS_HAL_CHIP(priv) ) {
			GET_HAL_INTERFACE(priv)->GetHwRegHandler(priv, HW_VAR_TSF_TIMER, (pu1Byte)&current_time);
		} else
#endif
		{
			current_time = RTL_R32(TSFTR1);
		}
	}
	else
		current_time = jiffies;

	head = priv->pshare->swq_timer_head;
	while (head != priv->pshare->swq_timer_tail) /* check if queue is empty */
	{
		priv->pshare->swq_timer_tail = (priv->pshare->swq_timer_tail + 1) % SWQ_TIMER_NUM;	
		swq_timer = &priv->pshare->swq_timer[priv->pshare->swq_timer_tail];
		if (swq_timer->pstat) {
			pstat = swq_timer->pstat;
			priv_this = swq_timer->priv;
			qnum = swq_timer->qnum;
			add_timer_timeout = swq_timer->timeout;

			swq_timer->pstat = NULL;
			pstat->swq.swq_timer_id[qnum] = 0;

			if (!(priv_this->drv_state & DRV_STATE_OPEN))
				continue;
		   
			need_dequeue = 0;
			if (skb_queue_len(&pstat->swq.swq_queue[qnum]) >= pstat->swq.q_aggnum[qnum]) {
				need_dequeue = 1;
			} else {
				if(priv->pshare->swq_use_hw_timer) {
					if(add_timer_timeout - (current_time + SWQ_HWTIMER_TOLERANCE) > SWQ_HWTIMER_MAXIMUN) {
						need_dequeue = 1;
					}
				}
				else {
					if(add_timer_timeout <= current_time) {
						need_dequeue = 1;
					}
				}
			}
			
			if(need_dequeue) {
#if (MU_BEAMFORMING_SUPPORT == 1)
				if(pstat->muPartner_num){
					bdfull = rtl8192cd_swq_bdfull(priv, 6, qnum);
				}else{
					if(pstat->swq.q_aggnum[qnum] ==  priv->pshare->rf_ft_var.qlmt)
						pstat->swq.q_aggnum[qnum] =  priv->pshare->rf_ft_var.swq_aggnum;	
					bdfull = rtl8192cd_swq_bdfull(priv, (pstat->swq.q_aggnum[qnum] * 2), qnum);
				}
#else
				bdfull = rtl8192cd_swq_bdfull(priv, (pstat->swq.q_aggnum[qnum] * 2), qnum);
#endif
				if(bdfull) {
					rtl8192cd_tx_queueDsr(priv, qnum);
					bdfull = rtl8192cd_swq_bdfull(priv, (pstat->swq.q_aggnum[qnum] * 2), qnum);
				}

				if(bdfull) {
					if(priv->pshare->swq_use_hw_timer) {
						add_timer_timeout = 1;   
						add_timer_timeout = add_timer_timeout * 1000; /* to us */
					}
					else {
						add_timer_timeout = 30;
						add_timer_timeout = RTL_MILISECONDS_TO_JIFFIES(add_timer_timeout);
					}
					add_timer_timeout += current_time;
					if (add_timer_timeout == 0)
						add_timer_timeout = 1;
				}
				else {
                    if(priv->pshare->swq_use_hw_timer) {
						priv->cnt_mu_swqtimeout++;
						rtl8192cd_swq_dequeue(priv_this, pstat, qnum, skb_queue_len(&pstat->swq.swq_queue[qnum]), SWQ_DEQ_LIMIT);
                        pstat->swq.q_TOCount[qnum]++;                        
                    }
                    else {
                    	pstat->swq.q_TOCount[qnum] += rtl8192cd_swq_dequeue(priv_this, pstat, qnum, skb_queue_len(&pstat->swq.swq_queue[qnum]), SWQ_DEQ_LIMIT);
                    }
					/* Becuase swq_dequeue costs time, renew current time for avoid add_timer_timeout overflow */
					if(priv->pshare->swq_use_hw_timer)
					{
#ifdef CONFIG_WLAN_HAL
						if ( IS_HAL_CHIP(priv) ) {
							GET_HAL_INTERFACE(priv)->GetHwRegHandler(priv, HW_VAR_TSF_TIMER, (pu1Byte)&current_time);
						} else
#endif
						{
							current_time = RTL_R32(TSFTR1);
						}
					}
					else
						current_time = jiffies;

					if (pstat->swq.swq_empty[qnum] == 0) {
						add_timer_timeout = 0;
					} else {
						if (skb_queue_len(&pstat->swq.swq_queue[qnum]) >= pstat->swq.q_aggnum[qnum]) {
							if(priv->pshare->swq_use_hw_timer) {
								add_timer_timeout = 1;   
								add_timer_timeout = add_timer_timeout * 1000; /* to us */
							}
							else {
								add_timer_timeout = 30;
								add_timer_timeout = RTL_MILISECONDS_TO_JIFFIES(add_timer_timeout);
							}
						} else {
							if(priv->pshare->swq_use_hw_timer) {
								add_timer_timeout = pstat->swq.swq_tri_time[qnum];
								add_timer_timeout = add_timer_timeout * 1000; /* to us */
							}
							else {
								add_timer_timeout = pstat->swq.swq_tri_time[qnum];
								add_timer_timeout = RTL_MILISECONDS_TO_JIFFIES(add_timer_timeout);
							}
						}
						add_timer_timeout += current_time;
						if (add_timer_timeout == 0)
							add_timer_timeout = 1;
					}
            	}
#ifndef SMP_LOAD_BALANCE_SUPPORT
                if(!bdfull) {
                    adjust_swq_setting(priv, pstat, qnum, CHECK_DEC_AGGN);
                }
#endif
            }

			if(add_timer_timeout) {
				rtl8192cd_swq_addtimer(priv_this, pstat, qnum, add_timer_timeout);
#ifndef SMP_LOAD_BALANCE_SUPPORT
				if(set_timer == 0) {
					set_timer_timeout = add_timer_timeout;
					set_timer = 1;
				}
				else {
					if(priv->pshare->swq_use_hw_timer) {
						if(add_timer_timeout - set_timer_timeout > SWQ_HWTIMER_MAXIMUN) {
							set_timer_timeout = add_timer_timeout;
						}
					}
					else {
						if(add_timer_timeout < set_timer_timeout) {
							set_timer_timeout = add_timer_timeout;
						}
					}
				}
#endif
			}
		}
	}

	if(set_timer) {
		rtl8192cd_swq_settimer(priv, set_timer_timeout);   /*need to re-setup timer*/
	}

	priv->pshare->has_triggered_sw_tx_Q_tasklet = 0;
#ifdef SMP_LOAD_BALANCE_SUPPORT
	RTL_W32(TC4_CTRL, BIT26 | BIT25 | BIT24 | (RTL_MICROSECONDS_TO_GTIMERCOUNTER(1000) & 0x00FFFFFF));
#endif

	RESTORE_INT(x);		
	SMP_UNLOCK_XMIT(x);
}
#endif //SW_TX_QUEUE


#ifdef RTK_ATM
__inline__ static int rtl8192cd_atm_swq_enqueue(struct rtl8192cd_priv *priv, struct sk_buff *skb, struct stat_info *pstat)
{
	int pri, q_num, q_len;
	//check qnum
	pri = skb->cb[_SKB_CB_PRIORITY];
	q_num = skb->cb[_SKB_CB_QNUM];

	//q_len >= atm_enqmax and burst_sent is over than burst_num, drop 1 packet
	{
		q_len = skb_queue_len(&pstat->atm_swq.swq_queue[q_num]);
		if(q_len >= priv->pshare->rf_ft_var.atm_enqmax)
		{
			pstat->atm_swq_full++;
			if(/*pstat->atm_burst_sent >= pstat->atm_burst_num &&*/
				(priv->pshare->atm_ttl_stanum>1 && !pstat->atm_is_maxsta) ||
				(pstat->atm_is_maxsta &&
					q_len>=priv->pshare->rf_ft_var.atm_enqmax+priv->pshare->rf_ft_var.atm_hista_enqbuf))
			{
		        struct sk_buff *tmpskb;
		    	tmpskb = skb_dequeue(&pstat->atm_swq.swq_queue[q_num]);
		        if (tmpskb) {
					priv->ext_stats.tx_drops++;
					pstat->atm_drop_cnt++;
					rtl_kfree_skb(priv, tmpskb, _SKB_TX_);
	        	}
			}
		}
	}

	//enqueue
	{
        skb_queue_tail(&pstat->atm_swq.swq_queue[q_num], skb);
		pstat->atm_swq.swq_empty[q_num] = 1;
    }

	//check refill
	//if sta already rearch my target burst, check other if burst out, too
	{
		if(pstat->atm_burst_sent >= pstat->atm_burst_num){
			pstat->atm_wait_cnt++;
			atm_check_refill(priv);
		}
	}

	//init timer
	{
		if(!priv->pshare->atm_timer_init){
			rtl8192cd_atm_swq_inittimer(priv);
			priv->pshare->atm_timer_init = 1;
			priv->pshare->atm_timeout_count = 0;
		}
	}

	return 0;
}
#endif

#if (!defined(__OSK__)) || (defined(__OSK__) && !defined(CONFIG_RTL6028))
__MIPS16
#endif
__IRAM_IN_865X
int __rtl8192cd_start_xmit_pci(struct sk_buff *skb, struct net_device *dev, int tx_flag)
{
	struct rtl8192cd_priv *priv;
	struct stat_info	*pstat=NULL;
	unsigned char		*da;
#if defined(__KERNEL__) || defined(__OSK__)
#if defined(HS2_SUPPORT) || !defined(CONFIG_PCI_HCI)
	struct sk_buff *newskb = NULL;
#endif
#endif
#ifdef TX_SCATTER
	struct sk_buff *newskb = NULL;
#endif
	struct net_device *wdsDev = NULL;
#if defined(SW_TX_QUEUE) || defined(RTK_ATM)
	int swq_out = 0;
#endif
#if defined(CONFIG_RTK_VLAN_SUPPORT)
	struct vlan_info *vlan=NULL;
#endif
	struct tx_insn tx_insn;
#ifdef CONFIG_RTL_EAP_RELAY
	int real_len;
#endif
#ifdef CONFIG_RTL_VLAN_8021Q
    uint16 vid;
    struct sk_buff *newskb;
#endif
#ifdef CONFIG_PUMA_VLAN_8021Q
    struct sk_buff *newskb;
#endif
    int temp;
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
    unsigned short protocol = ntohs(*((UINT16 *)((UINT8 *)skb->data + ETH_ALEN*2)));

	DECLARE_TXCFG(txcfg, tx_insn);

	priv = GET_DEV_PRIV(dev);

#ifdef ENABLE_RTL_SKB_STATS
	rtl_atomic_inc(&priv->rtl_tx_skb_cnt);
#endif

#ifdef TX_SKB_REFINE
    if (skb->cb[33] == 0x11) {
		skb->dev = dev;
		goto check_swq_enq;
    }
#endif
	
	if (skb->len < 15)
    {
        _DEBUG_ERR("TX DROP: SKB len small:%d\n", skb->len);
        goto free_and_stop;
    }

    #ifdef CONFIG_RTL_NETSNIPER_SUPPORT
    rtl_netsniper_check(skb, dev);
    #endif

#if defined(CONFIG_RTL_CUSTOM_PASSTHRU)
	if (dev==wlan_device[passThruWanIdx].priv->pWlanDev ||skb->dev==wlan_device[passThruWanIdx].priv->pWlanDev)
	{
#ifdef __ECOS
		if (SUCCESS==rtl_isWlanPassthruFrame(skb->data))
#else
		if (SUCCESS==rtl_isPassthruFrame(skb->data))
#endif
		{
#ifdef UNIVERSAL_REPEATER			
			if(passThruStatusWlan & 0x8) //WISP Mode Enable. default is vxd
			{
			    // TODO: need to add 8881A?, check it
         		   //#if defined(CONFIG_RTL_92D_SUPPORT) || defined(CONFIG_RTL_8812_SUPPORT)||defined (CONFIG_RTL_8881A)
			//dual band			
				#if (defined (CONFIG_USE_PCIE_SLOT_0)&&(defined (CONFIG_USE_PCIE_SLOT_1) || defined (CONFIG_RTL_8197F)))||( defined(CONFIG_RTL_8881A)&& !defined(CONFIG_RTL_8881A_SELECTIVE))|| defined(CONFIG_WLAN_HAL_8814AE)
				unsigned int wispWlanIndex=(passThruStatusWlan&WISP_WLAN_IDX_MASK)>>WISP_WLAN_IDX_RIGHT_SHIFT;
				  if ((wlan_device[wispWlanIndex].priv->drv_state & DRV_STATE_OPEN )&&
				  	 ((GET_MIB(GET_VXD_PRIV((wlan_device[wispWlanIndex].priv)))->dot11OperationEntry.opmode) & WIFI_STATION_STATE))
				  {
					#if 0//def NETDEV_NO_PRIV
				    	//dev=skb->dev=((struct rtl8192cd_priv *)(((struct rtl8192cd_priv *)netdev_priv(wlan_device[wispWlanIndex].priv->pWlanDev))->wlan_priv))->pvxd_priv->dev;
						dev=skb->dev=(((struct rtl8192cd_priv *)netdev_priv(wlan_device[wispWlanIndex].priv->dev))->wlan_priv)->pvxd_priv->dev;
					#else	
						dev=skb->dev=GET_VXD_PRIV(wlan_device[wispWlanIndex].priv)->dev;
					#endif
				  } else {
					goto free_and_stop;
				  }
				#else		
					#if 0//def NETDEV_NO_PRIV
					//dev=skb->dev=((struct rtl8192cd_priv *)(((struct rtl8192cd_priv *)netdev_priv(wlan_device[passThruWanIdx].priv->pWlanDev))->wlan_priv))->pvxd_priv->dev;
					dev=skb->dev=(((struct rtl8192cd_priv *)netdev_priv(wlan_device[wispWlanIndex].priv->dev))->wlan_priv)->pvxd_priv->dev;
					#else	
					dev=skb->dev= GET_VXD_PRIV(wlan_device[passThruWanIdx].priv)->dev;
					#endif
				#endif
			} else
#endif
			{
			    // TODO: need to add 8881A?, check it
				//#if defined(CONFIG_RTL_92D_SUPPORT) || defined(CONFIG_RTL_8812_SUPPORT)||defined (CONFIG_RTL_8881A)
				//dual band
				#if (defined (CONFIG_USE_PCIE_SLOT_0)&&(defined (CONFIG_USE_PCIE_SLOT_1) || defined (CONFIG_RTL_8197F)))||( defined(CONFIG_RTL_8881A)&& !defined(CONFIG_RTL_8881A_SELECTIVE))|| defined(CONFIG_WLAN_HAL_8814AE)
				unsigned int wispWlanIndex=(passThruStatusWlan&WISP_WLAN_IDX_MASK)>>WISP_WLAN_IDX_RIGHT_SHIFT;
				  if ((wlan_device[wispWlanIndex].priv->drv_state & DRV_STATE_OPEN )&&
				  	 (((GET_MIB(wlan_device[wispWlanIndex].priv))->dot11OperationEntry.opmode) & WIFI_STATION_STATE))
				  {
					#if 0//def NETDEV_NO_PRIV
				        dev=skb->dev=((struct rtl8192cd_priv *)netdev_priv(wlan_device[passThruWanIdx].priv->pWlanDev))->wlan_priv->dev;
					#else	
						dev=skb->dev=wlan_device[wispWlanIndex].priv->dev;
					#endif
				  } else {
					goto free_and_stop;
				  }
				#else		
					#ifdef NETDEV_NO_PRIV
					dev=skb->dev=((struct rtl8192cd_priv *)netdev_priv(wlan_device[passThruWanIdx].priv->pWlanDev))->wlan_priv->dev;				
					#else	
					dev=skb->dev=((struct rtl8192cd_priv *)(wlan_device[passThruWanIdx].priv->pWlanDev->priv))->dev;
					#endif
				#endif
			}

		SMP_UNLOCK_XMIT(flags);
			
		rtl8192cd_start_xmit(skb, dev);
		
		SMP_LOCK_XMIT(flags);
		return 0;	

		} else {
			goto free_and_stop;
		}
	}
#endif	/* defined(CONFIG_RTL_CUSTOM_PASSTHRU) */

#if 0/*def CONFIG_RTL_STP*/
	//port5: virtual device for wlan0 stp BPDU process
	if(memcmp((void *)(dev->name), "port5", 5)==0)
	{
		if ((skb->data[0]&0x01) && !memcmp(&(skb->data[0]), STPmac, 5) && !(skb->data[5] & 0xF0))
		{
			//To virtual device port5, tx bpdu.
		}
		else
		{
			//To virtual device port5, drop tx pkt becasue not bpdu!
			goto free_and_stop;
		}
	}
#endif

#ifdef CONFIG_RTL_EAP_RELAY
//mark_test , remove EAP padding by ethernet
	if (protocol == 0x888e) {
		real_len = ETH_ALEN*2+2+4+ntohs(*((UINT16 *)((UINT8 *)skb->data + ETH_ALEN*2+2+2)));
		skb_trim(skb,real_len);
		//printk("2 wlan tx EAP ,skb->len=%d,skb->data=%d \n",skb->len,skb->data_len);		
	}
#endif

//#ifdef SW_TX_QUEUE
        skb->dev = dev;
//#endif

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

#ifdef	IGMP_FILTER_CMO
	if (priv->pshare->rf_ft_var.igmp_deny)
	{
		if ((OPMODE & WIFI_AP_STATE) &&	IS_MCAST(skb->data))
		{
			if (IP_MCAST_MAC(skb->data)
				#ifdef	TX_SUPPORT_IPV6_MCAST2UNI
				|| ICMPV6_MCAST_MAC(skb->data)
				#endif
				)
			{
				if(!IS_IGMP_PROTO(skb->data)){
					priv->ext_stats.tx_drops++;										
					DEBUG_ERR("TX DROP: Multicast packet filtered\n");
					goto free_and_stop;
				}
			}
		}
	}
#endif

#ifdef HS2_SUPPORT
	if (priv->proxy_arp) {
		if (isDHCPpkt(skb)) {
			staip_snooping_bydhcp(skb, priv);
		}

		if (IS_MCAST(skb->data)) {
			//proxy arp	
			if (protocol == ETH_P_ARP) {
				if (proxy_arp_handle(priv, skb)) {
					//reply ok and return
			        	goto stop_proc;
				}
				//drop packets
		        goto free_and_stop;
			}
			//icmpv6 
			if (ICMPV6_MCAST_SOLI_MAC(skb->data)) {
			//if (ICMPV6_MCAST_MAC(skb->data))
				if (proxy_icmpv6_ndisc(priv,skb)) {
					goto stop_proc;
				}
				//drop packets
				goto free_and_stop;
			} 
			//drop unsolicited neighbor advertisement when proxy arp=1
			if (ICMPV6_MCAST_MAC(skb->data)) {
				if (check_nei_advt(priv,skb)) {
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
			/*closed by SD9 Kai, only multicast packets are permited*/
			/*which will casue dhcp broadcast packet dropped*/
			/*jim 20160415*/
		#if 0 //defined(__OSK__)
				// on osk platform, we transform dhcp broadcast packet into unicast packet for lease packet loss.
				|| IPV4_DHCP_BROADCAST_PACKET(skb->data)
        #endif
				#ifdef	TX_SUPPORT_IPV6_MCAST2UNI
				|| IPV6_MCAST_MAC(skb->data)
				#endif
				) && (tx_flag != TX_NO_MUL2UNI))
			{
#if defined(__OSK__) && defined(CONFIG_RTL8672)
				int i, num, idx;
				num = TOTAL_WLAN_PORT_NUM-1;
				idx = wlan_find_idx_by_linux_name(skb->dev->name);
				if(enable_IGMP_SNP){		
					for (i=0; i<=num; i++) {	
						if ( (skb->dev == wlanDev[i].dev_pointer) && isWLANInterfaceIdx(idx)) {
							if(check_wlan_mcast_tx(skb, idx)==1){
								//xprintfk("stop tx to wlan.[%s]:[%d].\n", __FUNCTION__,__LINE__);
								goto free_and_stop;
							}
						}
					}
				}
#ifdef INET6
				if (enable_MLD_SNP) {
					for (i=0; i<=num; i++) {	
						if ( (skb->dev == wlanDev[i].dev_pointer) && isWLANInterfaceIdx(idx)) {
							 if(check_wlan_ipv6_mcast_tx(skb, idx)==1) {
								goto free_and_stop;
							}  		
						}
					}
				}
#endif
#endif
				if (mlcst2unicst(priv, skb)){
					return 0;
				}
			}
		}

		if (tx_flag != TX_NO_MUL2UNI)
			skb->cb[2] = 0;	// allow aggregation
	}

	skb->cb[16] = tx_flag;
#endif

#if !defined(CONFIG_RTL_8676HWNAT) && defined(CONFIG_RTL8672) && !defined(CONFIG_RTL8686) && !defined(CONFIG_RTL8685) && !defined(CONFIG_RTL8685S) && !defined(CONFIG_RTL9607C)
#ifdef __OSK__
//IGMP snooping
	int i, num, idx;
	num = TOTAL_WLAN_PORT_NUM-1;
	idx = wlan_find_idx_by_linux_name(skb->dev->name);
	if(enable_IGMP_SNP){
		for (i=0; i<=num; i++) {	
			if ( (skb->dev == wlanDev[i].dev_pointer) && isWLANInterfaceIdx(idx)) {
				if(check_wlan_mcast_tx(skb, idx)==1) {
					goto free_and_stop;
				}  
				break;
			}
		}
	}
#ifdef INET6
	if (enable_MLD_SNP) {\
		for (i=0; i<=num; i++) {	
			if ( (skb->dev == wlanDev[i].dev_pointer) && isWLANInterfaceIdx(idx)) {
				if(check_wlan_ipv6_mcast_tx(skb, idx)==1) {
					goto free_and_stop;
				}  
				break;
			}
		}
	}
#endif
#else
	//IGMP snooping
	if (check_wlan_mcast_tx(skb)==1) {
		goto free_and_stop;
	}
#endif //__OSK__
#endif

#if defined(__OSK__) && defined(CONFIG_MULTICAST_PORTMAPPING)
	if( enable_port_mapping )
	{
		unsigned int wlanportmap=0;
		int ipv6flag=0;
		int multflag=0;
		if((skb->data[0]==0x01) && (skb->data[1]==0x0) && ((skb->data[2]&0xfe)==0x5e)){
			multflag=1;
		}
#ifdef INET6
		if((skb->data[0]==0x33) && (skb->data[1]==0x33)){
			multflag=1;
			ipv6flag=1;
		}
#endif
		if(multflag && (skb->wan_logport != 0)){
			//bypass non-multicast pkt
			int k, num, idx;
			num = TOTAL_WLAN_PORT_NUM-1;
			//idx = wlan_find_idx_by_linux_name(dev);
			idx = wlan_find_idx_by_linux_name((const char*)skb->dev->name);
			for (k=0; k<=num; k++) {
				if ( (dev == wlanDev[k].dev_pointer) && isWLANInterfaceIdx(idx)) {
					wlanportmap=wlan_multicast_portmapping(skb,ipv6flag);
					IGMPSNOOPDEBUG(("%s: line %d, wlanportmap=0x%x, k=%d, idx=%d\n",__FUNCTION__, __LINE__, wlanportmap, k, idx));
					if (wlanportmap<0)
					{		
					goto free_and_stop;
					} else {
						if((wlanportmap&(1<<idx))!=0)
						{
							break;
						} else {	
							goto free_and_stop;
						}
					}
				}
			}
	   	}		
	}
#endif

	if(priv->pshare->rf_ft_var.drop_multicast && IS_MCAST(skb->data) && ((unsigned char)skb->data[0] != 0xff))
		goto free_and_stop;	

#if defined(TAROKO_0) && defined(PE_RX_BCMC_PKT_RECV)
	//Copy Broadcast/Multicast frame to A17 through IPC
	if(IS_MCAST(skb->data))
	{
		//printk("BS %d,", __LINE__);
		//dev_kfree_skb_any(skb);
		taroko_t2h_ipc_pkt_send(skb, (unsigned int)skb->data, skb->len);
		goto stop_proc;
	}
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
	else
		ACTIVE_ID = 0;
#endif

#ifdef CONFIG_RTL867X_VLAN_MAPPING
	if (re_vlan_loaded()) {
		struct ethhdr *eth = (struct ethhdr *)skb->data;
		unsigned short vid = 0;

		if (eth->h_proto != ETH_P_PAE) {
			if (re_vlan_skb_xmit(skb, &vid)) {
				priv->ext_stats.tx_drops++;
				return 0;
			}
			if (vid && re_vlan_addtag(skb, vid)) {
				priv->ext_stats.tx_drops++;
				return 0;
			}
		}
	}
#endif

#ifdef CONFIG_RTK_VLAN_SUPPORT
		vlan = &priv->pmib->vlan;

	if (vlan->global_vlan && protocol != 0x888e) {
		int get_pri = 0;
#ifdef WIFI_WMM
		if (QOS_ENABLE) {
#ifdef CLIENT_MODE
			if ((OPMODE & (WIFI_STATION_STATE | WIFI_ASOC_STATE)) == (WIFI_STATION_STATE | WIFI_ASOC_STATE))
				pstat = get_stainfo(priv, BSSID);
			else
#endif
			{
#ifdef MCAST2UI_REFINE
                                if (pstat == NULL && !IS_MCAST(&skb->cb[10]))
                                        pstat = get_stainfo(priv, &skb->cb[10]);
#else
				if (pstat == NULL && !IS_MCAST(skb->data))
					pstat = get_stainfo(priv, skb->data);
#endif
			}

			if (pstat && pstat->QosEnabled)
				get_pri = 1;
		}
#endif

		if (!get_pri)
			skb->cb[0] = '\0';

		if (tx_vlan_process(dev, &priv->pmib->vlan, skb, get_pri)) {
			priv->ext_stats.tx_drops++;
			DEBUG_ERR("TX DROP: by vlan!\n");
			goto free_and_stop;
		}
	}
	else
		skb->cb[0] = '\0';
#endif

#if defined(CONFIG_RTL_VLAN_PASSTHROUGH_SUPPORT)
	if (protocol != ETH_P_8021Q) {	
		extern int rtl_vlan_passthrough_enable;
		if (rtl_vlan_passthrough_enable){
			if(FAILED == rtl_process_vlan_passthrough_tx(&skb))
				goto stop_proc;
		}
	}
#endif

#if defined(CONFIG_RTL_ISP_MULTI_WAN_SUPPORT)
	do {
		extern int wan_mapping_check(struct sk_buff * skb);
		if (FAILED == wan_mapping_check(skb)) {
			goto free_and_stop;
		}
	} while (0);
#endif

#if defined(CONFIG_RTL_819X_ECOS)&&defined(CONFIG_RTL_VLAN_SUPPORT)&&defined(CONFIG_RTL_819X_SWCORE)
	if (rtl_vlan_support_enable && protocol != 0x888e) {
		int get_pri = 0;
		if (QOS_ENABLE) {
#ifdef CLIENT_MODE
			if ((OPMODE & (WIFI_STATION_STATE | WIFI_ASOC_STATE)) == (WIFI_STATION_STATE | WIFI_ASOC_STATE))
				pstat = get_stainfo(priv, BSSID);
			else
#endif
			{
				if (pstat == NULL && !IS_MCAST(skb->data))
					pstat = get_stainfo(priv, skb->data);
			}

			if (pstat && pstat->QosEnabled)
				get_pri = 1;
		}

		if (!get_pri)
			skb->cb[0] = '\0';

		if (rtl_vlanEgressProcess(skb, priv->dev->name, get_pri) < 0) {
			priv->ext_stats.tx_drops++;
			DEBUG_ERR("TX DROP: by vlan!\n");
			goto free_and_stop;
		}
	}
	else
		skb->cb[0] = '\0';
#endif

#ifdef CONFIG_RTL_VLAN_8021Q
	//Ingress check for WAN->WLAN(Only NAT).
	/*
	#ifdef CONFIG_RTL_8021Q_VLAN_SUPPORT_SRC_TAG
	if (*(uint16*)skb->linux_vlan_src_tag == __constant_htons(ETH_P_8021Q) &&
		protocol == ETH_P_8021Q)
	{	
		uint16 tag_vid=0, src_vid=0;
		
		src_vid = ntohs(*(uint16*)(skb->linux_vlan_src_tag+2)) & 0x0fff;
		tag_vid = ntohs(*(uint16*)(skb->data+(ETH_ALEN<<1)+2)) & 0x0fff;

		if(src_vid != tag_vid)
		{	
			priv->ext_stats.tx_drops++;
			goto free_and_stop;
		}
	}
	#endif
	*/
	
	/*remove vlan tag if it is not need to tagged out*/
	if(linux_vlan_enable){		
		if(protocol == ETH_P_8021Q){
			vid = ntohs(*(uint16*)(skb->data+(ETH_ALEN<<1)+2)) & 0x0fff;
			
			#ifndef CONFIG_RTL_8021Q_VLAN_SUPPORT_SRC_TAG
			//In source-tag mode, wlan tx doesn't support tagged.
			if(!(vlan_ctl_p->group[vid].tagMemberMask&(1<<dev->vlan_member_map)))
			#endif
			{
				if (skb_cloned(skb)){
					newskb = skb_copy(skb, GFP_ATOMIC);
					if (newskb == NULL) {
 					 	goto free_and_stop;
					}
					dev_kfree_skb_any(skb);
					skb = newskb;
				}
				memmove(skb->data+VLAN_HLEN, skb->data, ETH_ALEN<<1);
				skb_pull(skb,VLAN_HLEN);
			}
		}
	}
#endif

#ifdef CONFIG_PUMA_VLAN_8021Q
	if (priv->pmib->vlan.vlan_enable) {
		if (protocol == ETH_P_8021Q) {
			if (skb_cloned(skb)){
				newskb = skb_copy(skb, GFP_ATOMIC);
				if (newskb == NULL) {
			 		priv->ext_stats.tx_drops++;
			 		DEBUG_ERR("TX DROP: Can't copy the skb!\n");
					goto free_and_stop;
				}
				dev_kfree_skb_any(skb);
				skb = newskb;
			}
			memmove(skb->data+VLAN_HLEN, skb->data, ETH_ALEN<<1);
			skb_pull(skb,VLAN_HLEN);
		}
	}
#endif

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

	if (priv->pmib->miscEntry.func_off || priv->pmib->miscEntry.raku_only) {
		goto free_and_stop;
	}

	// drop packets if link status is null
#ifdef WDS
	if (!wdsDev)
#endif
	{
		if (priv->assoc_num == 0) {
#ifdef RTK_BR_EXT
			if(!((OPMODE & WIFI_STATION_STATE) && priv->pmib->ethBrExtInfo.macclone_enable && !priv->macclone_completed))
#endif
			{
				priv->ext_stats.tx_drops_noasoc++;
				DEBUG_WARN("TX DROP: Non asoc tx request!\n");
				goto free_and_stop;
			}
		}
	}

#ifdef CLIENT_MODE
	// nat2.5 translation, mac clone, dhcp broadcast flag add.
	if (OPMODE & (WIFI_STATION_STATE|WIFI_ADHOC_STATE)) {

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
#if defined(__KERNEL__) || defined(__OSK__)
					GET_BR_PORT(priv->dev) &&
#endif
					memcmp(skb->data+MACADDRLEN, priv->br_mac, MACADDRLEN) &&
//					*((unsigned short *)(skb->data+MACADDRLEN*2)) != __constant_htons(ETH_P_8021Q) &&
					protocol == ETH_P_IP &&
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
#ifdef RTK_BR_EXT
				if(!(priv->pmib->ethBrExtInfo.macclone_enable && !priv->macclone_completed) &&
					((OPMODE & WIFI_STATION_STATE) && memcmp(skb->data+ETH_ALEN, GET_MY_HWADDR, ETH_ALEN)))	// ignore tx from root br to vap(client mode) if vap is not connected to remote ap successfully
#else
				if((OPMODE & WIFI_STATION_STATE) && memcmp(skb->data+ETH_ALEN, GET_MY_HWADDR, ETH_ALEN))	// ignore tx from root br to vap(client mode) if vap is not connected to remote ap successfully
#endif
				{				
					struct stat_info *pstat = get_stainfo(priv,BSSID);
					if (!((NULL != pstat) && (pstat->state & (WIFI_AUTH_SUCCESS | WIFI_ASOC_STATE))
						   && (((0 != pstat->dot11KeyMapping.dot11EncryptKey.dot11TTKeyLen) && (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm != _NO_PRIVACY_))
							   || (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _NO_PRIVACY_)))) {
						goto free_and_stop;
					}
				}
#ifdef CLIENT_MODE
				SMP_UNLOCK_XMIT(flags);
				if (rtl8192cd_tx_clientMode(priv, &skb) == TX_PROCEDURE_CTRL_STOP) {
					SMP_LOCK_XMIT(flags);
					goto stop_proc;
				}
				SMP_LOCK_XMIT(flags);
#endif				
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
				(*(unsigned int *)&(skb->cb[24]) != priv->pmib->miscEntry.groupID)		// check group ID
				#else
				(*(unsigned int *)&(skb->cb[12]) != priv->pmib->miscEntry.groupID)		// check group ID
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

check_swq_enq:

#ifdef WDS
	if (wdsDev)
	{
		if ((txcfg->wdsIdx >= 0))
			da = priv->pmib->dot11WdsInfo.entry[txcfg->wdsIdx].macAddr;
		else
		{
			DEBUG_ERR("wdsIdx < 0\n");
			priv->ext_stats.tx_drops++;
			goto free_and_stop;
		}
	}
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
        if (pstat == NULL && (OPMODE & WIFI_AP_STATE)) {
            if(priv->pmib->miscEntry.a4_enable == 2) {
            if(IS_MCAST(da)) {
                    /*duplicate the packet and use 4-address to send to every a4 client, 
                                      and send one 3-address packet  to other a3 clients*/
                if(a4_tx_mcast_to_unicast(priv, skb) == 1) {
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

#if defined(TV_MODE) && defined(SUPPORT_TX_MCAST2UNI) /*drop (joint multicast && (ipv4 || ipv6)) packet*/
        if(priv->pmib->miscEntry.forward_streaming == 0 && (priv->tv_mode_status & BIT0) == 0 && IS_MCAST(da)) {
            if(protocol == ETH_P_IP || protocol == ETH_P_IPV6) {
                if(tv_mode_igmp_group_check(priv, skb))
                    goto free_and_stop;
            }
        }
#endif
        }

	if (pstat) {
		int priority;
		priority = get_skb_priority(priv, skb, pstat, 0);
		skb->cb[_SKB_CB_QNUM] = pri_to_qnum(priv, priority);
	}

    #if defined(RTK_ATM) || defined(SW_TX_QUEUE)
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
    	if (pstat)
        {
            #ifdef RTK_ATM
            if(priv->pshare->rf_ft_var.atm_en)
                swq_out = priv->pshare->atm_swq_en | pstat->atm_swq.swq_empty[BK_QUEUE] | 
                                     pstat->atm_swq.swq_empty[BE_QUEUE] | 
                                     pstat->atm_swq.swq_empty[VO_QUEUE] | 
                                     pstat->atm_swq.swq_empty[VI_QUEUE];   
            else
            #endif
            {
                #ifdef SW_TX_QUEUE
                swq_out = priv->pshare->swq_en | pstat->swq.swq_empty[BK_QUEUE] | 
                                         pstat->swq.swq_empty[BE_QUEUE] | 
                                         pstat->swq.swq_empty[VO_QUEUE] | 
                                         pstat->swq.swq_empty[VI_QUEUE];
                #endif
            }

            if (skb->cb[_SKB_CB_FLAGS] & _SKB_CB_FLAGS_URGENT)
                swq_out = 0;
        }
	}
    #endif //defined(RTK_ATM) || defined(SW_TX_QUEUE)

#ifdef DETECT_STA_EXISTANCE
	if (pstat && pstat->leave) {
		priv->ext_stats.tx_drops++;
		DEBUG_WARN("TX DROP: sta may leave! %02x%02x%02x%02x%02x%02x\n", pstat->cmn_info.mac_addr[0],pstat->cmn_info.mac_addr[1],pstat->cmn_info.mac_addr[2],pstat->cmn_info.mac_addr[3],pstat->cmn_info.mac_addr[4],pstat->cmn_info.mac_addr[5]);
		goto free_and_stop;
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

#ifdef TX_SKB_REFINE
    if (skb->cb[33] != 0x11)
#endif
    {
		if(pstat && (skb->cb[_SKB_CB_FLAGS] & _SKB_CB_FLAGS_TCPACK) && tx_flag != TX_NO_TCPACK_ACC)
			pstat->tcpack_pkts++;

#ifdef TCP_ACK_ACC
		if (pstat && priv->pmib->miscEntry.tcpack_acc && (pstat->tcpack_acc_en || pstat->veriwave_5G_TCP_Rx_test) && tx_flag != TX_NO_TCPACK_ACC) {
			if (tx_tcp_ack_accelerate(priv, pstat, skb))
				goto stop_proc;
		}
#endif
	}
	
#if defined(SW_TX_QUEUE) || defined(RTK_ATM) 
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
		if (swq_out == 0
	        #ifdef GBWC
			|| priv->GBWC_consuming_Q
	        #endif
			#ifdef SBWC
			|| (pstat && pstat->SBWC_consuming_q)
			#endif
#ifdef SUPPORT_TX_AMSDU_SMALL_PKTS_ONLY
			|| (priv->pmib->dot11nConfigEntry.dot11nAMSDUSmallPkts &&
			   (skb->len <= priv->pmib->dot11nConfigEntry.dot11nAMSDUSmallPktLen || ((skb->cb[_SKB_CB_FLAGS] & _SKB_CB_FLAGS_TCPACK) && pstat->amsdu_tcpack_en)))
#endif	
			|| (pstat && (get_sta_encrypt_algthm(priv, pstat)) && !(pstat->dot11KeyMapping.keyInCam))
			) {//direct send and no use swq
	        goto xmit_out;
	    } 
	    #ifdef RTK_ATM
	    else if (priv->pshare->rf_ft_var.atm_en) {
	        if(priv->pshare->atm_swq_en)
	            return rtl8192cd_atm_swq_enqueue(priv, skb, pstat);
	        else {
	            for(temp = BK_QUEUE; temp <= VO_QUEUE; temp++)
	            {
	                if(pstat->atm_swq.swq_empty[temp])                                             
	                    atm_skb_dequeue(priv, pstat, 0xff, temp);//send out all packet in atm_swq
	            }
	            goto xmit_out;
	        }
	    } 
	    #endif
	    #ifdef SW_TX_QUEUE
		else if (priv->pshare->swq_en) {//swq_en=1
#ifndef SMP_LOAD_BALANCE_SUPPORT
			if (priv->pshare->swq_cnt > priv->pshare->rf_ft_var.swq_max_enqueue_len)
			{
				DEBUG_ERR("TX DROP: exceed the SW tx queue!\n");
				priv->ext_stats.tx_drops++;
				priv->ext_stats.swq_overflow_drop_pkt++;
				goto free_and_stop;
			} else
#endif
			{
				priv->ext_stats.swq_enque_pkt++;
				return rtl8192cd_swq_enqueue(priv, skb, pstat);
			}
		} 
		else {//swq_en=0 and swq still have packet queuing
			for(temp = BK_QUEUE; temp <= VO_QUEUE; temp++)
			{
				if(pstat->swq.swq_empty[temp]) {
					if (pstat->swq.swq_timer_id[temp])
						rtl8192cd_swq_deltimer(priv, pstat, temp);
					rtl8192cd_swq_dequeue(priv, pstat, temp, 0, SWQ_DEQ_ALL);
				}
			}
			goto xmit_out;
		}
		#endif
	}
xmit_out:
#endif //defined(SW_TX_QUEUE) || defined(RTK_ATM) 

    return __rtl8192cd_start_xmit_out(skb, pstat, NULL);

free_and_stop:          /* Free current packet and stop TX process */
	
	rtl_kfree_skb(priv, skb, _SKB_TX_);

stop_proc:                      /* Stop process and assume the TX-ed packet is already "processed" (freed or TXed) in previous code. */

    return 0;
}

#if defined(CONFIG_PUMA_UDMA_SUPPORT)

int rtl8192cd_udma_start_xmit(struct sk_buff *skb, struct net_device *dev){

	unsigned int i;
	struct sk_buff *skb_tmp;
	struct rtl8192cd_priv *priv = GET_DEV_PRIV(dev);
	struct rtl8192cd_priv *priv_tmp;

#if defined(CONFIG_PUMA_VLAN_8021Q) 
	
	if((skb!=NULL) && (*((UINT16*)(skb->data+(ETH_ALEN<<1))) == __constant_htons(ETH_P_8021Q))){


		UINT16 tag_vid = ntohs(*(UINT16*)(skb->data+(ETH_ALEN<<1)+2)) & 0x0fff;
		
		// xmit to root interface
		{
	
			if(IS_DRV_OPEN(priv) && priv->assoc_num && (0 == priv->pmib->miscEntry.func_off) && (1 == priv->pmib->vlan.vlan_enable) && (tag_vid == priv->pmib->vlan.vlan_id)){
	
				skb_tmp = skb_copy(skb, GFP_ATOMIC);
				
				if (skb_tmp == NULL) {
						priv->ext_stats.tx_drops++;
						DEBUG_ERR("TX DROP: Can't copy the skb!\n");
						goto stop_xmit;
				}
				
				rtl8192cd_start_xmit(skb_tmp,priv->dev);
			}

		}
		
#if defined(MBSSID)

		// xmit to vap interface
		{
			
			for(i=0;i<RTL8192CD_NUM_VWLAN;i++){
				
				priv_tmp = GET_VAP_PRIV(priv,i);
				
				if( IS_DRV_OPEN(priv_tmp) && priv_tmp->assoc_num && (0 == priv_tmp->pmib->miscEntry.func_off) && (1 == priv_tmp->pmib->vlan.vlan_enable) && (tag_vid == priv_tmp->pmib->vlan.vlan_id)){

					skb_tmp = skb_copy(skb, GFP_ATOMIC);
					
					if (skb_tmp == NULL) {
							priv->ext_stats.tx_drops++;
							DEBUG_ERR("TX DROP: Can't copy the skb!\n");
							goto stop_xmit;
					}
					
					rtl8192cd_start_xmit(skb_tmp,priv_tmp->dev);
				}
			}	
		}

#endif	// defined(MBSSID)

#ifdef UNIVERSAL_REPEATER
	
			// xmit to vxd interface
			{
						
				priv_tmp = GET_VXD_PRIV(priv);
				
				if(IS_DRV_OPEN(priv_tmp) && priv_tmp->assoc_num && (0 == priv_tmp->pmib->miscEntry.func_off) && (1 == priv_tmp->pmib->vlan.vlan_enable) && (tag_vid == priv_tmp->pmib->vlan.vlan_id)){
	
					skb_tmp = skb_copy(skb, GFP_ATOMIC);
					
					if (skb_tmp == NULL) {
							priv->ext_stats.tx_drops++;
							DEBUG_ERR("TX DROP: Can't copy the skb!\n");
							goto stop_xmit;
					}
					
					rtl8192cd_start_xmit(skb_tmp,priv_tmp->dev);
				}
	
			}
	
#endif
	
		}else
	
#endif // defined(CONFIG_PUMA_VLAN_8021Q) 
		{
#if 0 //for debug
			DEBUG_WARN("Receive a NON-VLAN PACKET\n");
			mem_dump("INFO",skb->data,skb->len);
#endif
		}

stop_xmit:

	dev_kfree_skb_any(skb);
	
	return 0;
	
}
#endif //defined(CONFIG_PUMA_UDMA_SUPPORT)


__IRAM_IN_865X
int rtl8192cd_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int ret;
	struct rtl8192cd_priv *priv = GET_DEV_PRIV(dev);
	unsigned short protocol;
#if (!defined(CONFIG_USB_HCI) && !defined(CONFIG_SDIO_HCI)) || defined(CONFIG_RTL_TRIBAND_SUPPORT)
	unsigned long x;
#endif
	int index;

#ifdef WLAN_VDEV_SUPPORT
	if(dev->name[0]=='v')
	{
		struct net_device *rdev;
		int i = 0;

		//update vdev stat
		priv->net_stats.tx_packets++;
		priv->net_stats.tx_bytes += skb->len;

		//printk("[%s:%d] skb->dev->name:%s, dev->name:%s, priv->dev->name:%s, priv->vdev->name:%s\n", __FUNCTION__, __LINE__, 
		//	skb->dev->name, dev->name, priv->dev->name, priv->vdev->name);
		dev = rdev = priv->vdev;
		priv = GET_DEV_PRIV(rdev);
		skb->dev = rdev;
	}
#endif

#ifdef CONFIG_SPECIAL_ENV_TEST
	if (skb->len == priv->pshare->rf_ft_var.spec_pktsz-4)
		priv->ext_stats.spec_start_xmit_pkt++;
#endif

	protocol = *((u16*)(skb->data+ETH_ALEN*2));

#ifdef __OSK__	
 	extern unsigned int AvailableMemSize;  // 20100818 defined in oskmem.c,  bad declaration, but still here for bad include on OSK.
	if(AvailableMemSize < (100*1024))
	{	// 20100818 this case should not happened, but we still add here to avoid some potential queuing more buffer in wifi driver
		dev_kfree_skb_any(skb);
		printk("TX: Memory is not enoughput\n");
		return 0; 
	}
#endif

#if defined(RTK_NL80211) && defined(MBSSID)
	if (protocol == __constant_htons(0x888e))
	{
		NDEBUG3("Tx EAP packets,len[%d],centCh[%d],Ch[%d] \n", 
             skb->len,priv->pshare->working_channel,priv->pshare->working_channel2);
        
		if(IS_ROOT_INTERFACE(priv) && priv->pmib->miscEntry.vap_enable)
		{	
			struct stat_info	*pstat=NULL;
			unsigned char		*da;
			unsigned char		*sa;
			
			da = skb->data;
			sa = skb->data+ETH_ALEN;

			//dump_mac2(da);
			//dump_mac2(sa);
			pstat = get_stainfo(priv, da);
			if(pstat == NULL)
			{
				int i;
				for (i=0; i<RTL8192CD_NUM_VWLAN; i++) {	
					if(IS_DRV_OPEN(priv->pvap_priv[i]))
					{
						pstat = get_stainfo(priv->pvap_priv[i], da);
						if(pstat)
						{
							priv = priv->pvap_priv[i];
							dev = priv->dev;
							memcpy(sa, dev->dev_addr, ETH_ALEN);
							printk("Use [%s] Tx EAP packets \n", dev->name);
							//dump_mac2(sa);
							break;
						}
					}
				}				
			}
		}
	}
#endif

#ifdef PCIE_POWER_SAVING_TEST //yllin
    if((priv->pwr_state >= L2) || (priv->pwr_state == L1)) {
        dev_kfree_skb_any(skb);
        return 0; 
    }
#endif

#ifdef CROSSBAND_REPEATER			
	if(IS_VXD_INTERFACE(priv) && IS_DRV_OPEN(priv))
	{
		unsigned char *da;
		da = skb->data;
		//Do not change path for broadcast/multicast otherwise storm occurs
		if(!(IS_MCAST(da) || IS_BCAST2(da))){
			//Check if crossband enabled && valid switch path available && this is not preferred interface
			if(GET_ROOT(priv)->crossband.crossband_status
				&& GET_ROOT(priv)->pmib->crossBand.crossband_pathReady
				&& !GET_ROOT(priv)->pmib->crossBand.crossband_prefer) 
			{			
					if(IS_DRV_OPEN(GET_ROOT(priv)->crossband.crossband_priv_sc))
					{					
						priv = GET_VXD_PRIV(GET_ROOT(priv)->crossband.crossband_priv_sc);
						dev = priv->dev; //switch network device for transmission
						priv->ext_stats.cb_pathswitch_pkts++;
					}
			}
		}
	}
#endif

	if (!(priv->drv_state & DRV_STATE_OPEN)){ 
		dev_kfree_skb_any(skb);
  		return 0; 
 	}

#ifdef CONFIG_POWER_SAVE
	if (rtw_ap_ps_xmit_monitor(priv)) {
		dev_kfree_skb_any(skb);	
		return 0;
	}
	// defer suspend procedure when EAP packet exchange
	// and avoid to do ToDrv_DisconnectSTA in suspend mode when EAP packet resend timeout due to short ps_timeout
	if ((protocol == __constant_htons(0x888e))
#ifdef CONFIG_RTL_WAPI_SUPPORT
		|| (protocol == __constant_htons(ETH_P_WAPI))
#endif
		)
		rtw_lock_suspend_timeout(priv, 2000);
#endif

#ifdef TX_SKB_REFINE
	if (skb->len > 38) {
		if (skb->data[23] == 0x11 &&
			skb->data[34] == 0x04 && skb->data[35] == 0x00 &&
			skb->data[36] == 0x04 && skb->data[37] == 0x00)
		{
			skb->cb[33] = 0x11;
		} else if (skb->data[23] == 0x11 &&
			skb->data[34] == 0xaf && skb->data[35] == 0xc8 &&
			skb->data[36] == 0xb3 && skb->data[37] == 0xb0)
		{
			skb->cb[33] = 0x11;
		}
	}
#endif

#ifdef CONFIG_SPECIAL_ENV_TEST
	if(priv->pshare->rf_ft_var.debug8822 == 0xff){
		if(!IS_MCAST(skb->data) && (IS_UDP_PROTO(skb->data) || IS_EXPERIMENT_PROTO(skb->data)) && memcmp(priv->wlan_addr,"\x0\x0\x0\x0\x0\x0",6))
			memcpy(skb->data, (const void *) priv->wlan_addr, WLAN_ADDR_LEN);
	}
#endif

#ifdef CONFIG_ARCH_LUNA_SLAVE
    /*
     * wps logo 4.1.12 fail, the STAUT(Intel) will not respond EAP Identity Req packet
     * wifi do wps on slave cpu, the wps daemon will send packet from master cpu to slave cpu
     * if packet length less than 64 byte, system will pad the packet length to 64 byte
     */
    
    if ((protocol != __constant_htons(ETH_P_PAE)) || (skb->len < 9))
        goto skip_check_eap;

    struct eapol_t *eapol = skb->data + sizeof(struct ethhdr);

    if ((eapol->protocol_version != 1) || 
        (eapol->packet_type != 0) ||        /* eap packet */
        (eapol->packet_body_length >= 42))
        goto skip_check_eap;

    if (skb->len > (sizeof(struct ethhdr) + sizeof(struct eapol_t) + eapol->packet_body_length))
        skb->len = sizeof(struct ethhdr) + sizeof(struct eapol_t) + eapol->packet_body_length;

skip_check_eap:
#endif

	if (IPV4_TCP_ACK_PACKET(skb->data))
		skb->cb[_SKB_CB_FLAGS] = _SKB_CB_FLAGS_TCPACK;
	else if ((protocol == __constant_htons(0x888e))
#ifdef CONFIG_RTL_WAPI_SUPPORT
			|| (protocol == __constant_htons(ETH_P_WAPI))
#endif
			)
		skb->cb[_SKB_CB_FLAGS] = _SKB_CB_FLAGS_URGENT;
	else if (isDHCPpkt(skb))
		skb->cb[_SKB_CB_FLAGS] = (_SKB_CB_FLAGS_URGENT | _SKB_CB_FLAGS_DHCP);
	else
		skb->cb[_SKB_CB_FLAGS] = 0;

#ifdef SMP_LOAD_BALANCE_SUPPORT
	if (skb->cb[_SKB_CB_FLAGS] & _SKB_CB_FLAGS_URGENT)
		goto direct_xmit;

#ifdef SW_TX_QUEUE
	long skb_qlen = rtl_atomic_read(&priv->pshare->skb_xmit_queue_len);
	if (priv->pshare->swq_cnt + skb_qlen >= priv->pshare->rf_ft_var.swq_max_enqueue_len) {
#ifdef _CORTINA_
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
		printk_ratelimited(KERN_WARNING "TX DROP: exceed skb Q!(seq %04x swq %3d,%d)\n",
			priv->pshare->swq_timeout_seq & 0xffff, priv->pshare->swq_cnt, skb_qlen);
#endif
#else /*_CORTINA_*/
		DEBUG_ERR("TX DROP: exceed skb Q!(seq %04x swq %3d,%d)\n",
			priv->pshare->swq_timeout_seq & 0xffff, priv->pshare->swq_cnt, skb_qlen);
#endif /*_CORTINA_*/
		priv->ext_stats.swq_overflow_drop_pkt++;
		priv->ext_stats.tx_drops++;
		dev_kfree_skb_any(skb);
		return 0; 
	}
#endif

#ifdef MCAST2UI_REFINE
	memcpy(&skb->cb[10], skb->data, 6);
#endif
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
	skb->cb[_SKB_CB_AMSDU_TXSC] = 0; /* clear amsdu short-cut indicate */
#endif
	skb->dev = dev;
#if (MAX_SKB_XMIT_QUEUE > 1)
	if ((OPMODE & WIFI_AP_STATE) && !IS_MCAST(skb->data))
		index = skb->data[5] % priv->pshare->skb_xmit_queue_num;
	else
#endif
		index = 0;
	skb_queue_tail(&priv->pshare->skb_xmit_queue[index], skb);
	rtl_atomic_inc(&priv->pshare->skb_xmit_queue_len);

	return 0;

direct_xmit:
#endif

#ifdef MCAST2UI_REFINE
	memcpy(&skb->cb[10], skb->data, 6);
#endif
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
	skb->cb[_SKB_CB_AMSDU_TXSC] = 0; /* clear amsdu short-cut indicate */
#endif

#ifdef CONFIG_DROP_TX_MCAST
	if (priv->pshare->rf_ft_var.drop_txmcast && IS_MCAST(skb->data) && !IS_BCAST2(skb->data)){
		dev_kfree_skb_any(skb);
		return 0;
	}
#endif /* CONFIG_DROP_TX_MCAST */

	SAVE_INT_AND_CLI(x);
	SMP_LOCK_XMIT(x);

	STARTROMEPERF(priv,0);
	
	ret = __rtl8192cd_start_xmit(skb, dev, TX_NORMAL);

	STOPROMEPERF(priv,1,0);
	RESTORE_INT(x);
	SMP_UNLOCK_XMIT(x);

	

	return ret;
}


