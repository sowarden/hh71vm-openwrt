

#ifdef __KERNEL__
#include <linux/module.h>
#include <asm/byteorder.h>
#elif defined(__ECOS)
#include <cyg/io/eth/rltk/819x/wrapper/sys_support.h>
#include <cyg/io/eth/rltk/819x/wrapper/skbuff.h>
#include <cyg/io/eth/rltk/819x/wrapper/timer.h>
#include <cyg/io/eth/rltk/819x/wrapper/wrapper.h>
#endif

#if !defined(__KERNEL__) && !defined(__ECOS)
#include "../sys-support.h"
#endif

#ifdef __KERNEL__
#include "../ieee802_mib.h"
#elif defined(__ECOS)
#include <cyg/io/eth/rltk/819x/wlan/ieee802_mib.h>
#endif

#include "./8192cd_headers.h"
#include "./8192cd_debug.h"
#include "./8192cd.h"
#include "./8192cd_cfg.h"
#include "./8192cd_11r_cli.h"


#ifdef CONFIG_IEEE80211R_CLI
extern void qsort (void  *base, int nel, int width, int (*comp)(const void *, const void *));
extern unsigned char *set_fixed_ie(unsigned char *pbuf, unsigned int len, unsigned char *source, unsigned int *frlen);
extern void regenerate_nonce(struct rtl8192cd_priv *priv);
extern unsigned char *getFTIESubElmt(unsigned char *ftie, unsigned int ftie_len, unsigned char elmt_id, unsigned int *outlen);
extern void clnt_switch_chan_to_bss(struct rtl8192cd_priv *priv);


unsigned char null_mac[MACADDRLEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


void setClientFTCapability(struct rtl8192cd_priv *priv, struct bss_desc *bss_target)
{
	if(bss_target->t_stamp[1] & FT_SUPPORT) {
		priv->pmib->dot11FTEntry.dot11FTOverDSEnabled = bss_target->ft_cap;
		memcpy(MDID, bss_target->mdid, MDID_LEN);
	}else
		FT_CLI_DEBUG(" AP doesn't have FT capability !!\n");

	//FT_CLI_DEBUG("Connecting to FT AP, midi(%02x, %02x), over_ds = %d\n", bss_target->mdid[0], bss_target->mdid[1], bss_target->ft_cap);
}

void parse_FTIE(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *p, int len)
{		
	WPA_STA_INFO	*pStaInfo = pstat->wpa_sta_info;
	unsigned char *pos = p;
	unsigned char pmk_r1_len = 0;
	unsigned char *r0kh_id, *r1kh_id;
	unsigned int  r0kh_id_len, r1kh_id_len;
	
	//Tag + len + MIC control + MIC
	pos += (1+1+2+16);
	
	// ANonce, SNonce
	if (pstat->ft_state == state_imd_assoc || pstat->ft_state == state_imd_4way) {
		//ANonce & SNonce are 0
		pos += (2 * KEY_NONCE_LEN); 
	} else {
		memcpy(pStaInfo->ANonce.Octet, pos, KEY_NONCE_LEN);
		pos += KEY_NONCE_LEN;
		memcpy(pStaInfo->SNonce.Octet, pos, KEY_NONCE_LEN);
		pos += KEY_NONCE_LEN;
	}

	r0kh_id = getFTIESubElmt(p, len + 2, _FT_R0KH_ID_SUB_IE_, &r0kh_id_len);
	if(r0kh_id) {
		memset(pStaInfo->cache_r0kh_id, 0, MAX_R0KHID_LEN);
		memcpy(pStaInfo->cache_r0kh_id, r0kh_id, r0kh_id_len);
		pStaInfo->cache_r0kh_id_len = r0kh_id_len;

		// sync to FT mib
		R0KH_ID_LEN = r0kh_id_len;
		memcpy(R0KH_ID, r0kh_id,R0KH_ID_LEN);		
	}
	
}

void setFTMDIEData(OCTET_STRING EapolKeyMsgSend, struct rtl8192cd_priv *priv)
{
	OCTET_STRING ft_md;
	unsigned char temp[MDID_IE_LEN];

	memset(temp, 0, MDID_IE_LEN);
	
	temp[0] = _MOBILITY_DOMAIN_IE_;
	temp[1] = 3;
	
	memcpy(&temp[2], MDID, 2);	
	if (priv->pmib->dot11FTEntry.dot11FTOverDSEnabled)
		temp[4] |= BIT(0);
	if (priv->pmib->dot11FTEntry.dot11FTResourceRequestSupported)
		temp[4] |= BIT(1);

	ft_md.Octet = temp;
	ft_md.Length = MDID_IE_LEN;
	Message_setKeyMDIEData(EapolKeyMsgSend, ft_md);	
}


void setFTIEData(OCTET_STRING EapolKeyMsgSend, struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	OCTET_STRING ft;
	unsigned char gkout[128], *pos;
	unsigned short gkout_len;
	unsigned char gkey_len;
	unsigned char temp[512];
	WPA_GLOBAL_INFO *pGblInfo = priv->wpa_global_info;
	WPA_STA_INFO	*pStaInfo = pstat->wpa_sta_info;

	memset(temp, 0, sizeof(temp));

	pos = temp;	
	*pos++ = _FAST_BSS_TRANSITION_IE_;
	*pos++ = FT_IE_LEN - 2;

	if (pstat->ft_state == state_ft_assoc) {
		SetFTMICCtrl(pos, 3);	
	}
	pos += 18;

	// ANonce, SNonce
	if (pstat->ft_state == state_imd_assoc || pstat->ft_state == state_imd_4way) {
		pos += (2 * KEY_NONCE_LEN);
	}
	else {
		memcpy(pos, pStaInfo->ANonce.Octet, KEY_NONCE_LEN);
		pos += KEY_NONCE_LEN;
		memcpy(pos, pStaInfo->SNonce.Octet, KEY_NONCE_LEN);
		pos += KEY_NONCE_LEN;
	}

	// R1KH-ID
	*pos++ = _FT_R1KH_ID_SUB_IE_;
	*pos++ = MACADDRLEN;
	memcpy(pos, BSSID, MACADDRLEN);
	pos += MACADDRLEN;

	// R0KH-ID
	if (pstat->ft_state >= state_ft_auth) {
		if (pStaInfo->r1kh) {
			*pos++ = _FT_R0KH_ID_SUB_IE_;
			*pos++ = pStaInfo->r1kh->r0kh_id_len;
			memcpy(pos, pStaInfo->r1kh->r0kh_id, pStaInfo->r1kh->r0kh_id_len);
			pos += pStaInfo->r1kh->r0kh_id_len;
		}
	}
	else {
		*pos++ = _FT_R0KH_ID_SUB_IE_;
		*pos++ = R0KH_ID_LEN;
		memcpy(pos, R0KH_ID, R0KH_ID_LEN);
		pos += R0KH_ID_LEN;
	}

	ft.Octet = temp;
	ft.Length = FT_IE_LEN;
	Message_setKeyFTIEData(EapolKeyMsgSend, ft);
}


void setFTPMKIDData(OCTET_STRING EapolKeyMsgSend, struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	OCTET_STRING ft_pmkid;
	WPA_STA_INFO *pStaInfo = pstat->wpa_sta_info;
	unsigned char temp[FT_PMK_ID_LEN];

	unsigned char *pos = temp;
	memset(temp, 0, sizeof(temp));

	*((unsigned short *)pos) = cpu_to_le16(1);
	pos += 2;

	memcpy(pos, pStaInfo->r1kh->pmk_r1_name, PMKID_LEN);
	pos += PMKID_LEN;

	ft_pmkid.Octet = temp;
	ft_pmkid.Length = FT_PMK_ID_LEN;
	Message_setKeyPMKIDData(EapolKeyMsgSend, ft_pmkid);
}

void issue_ft_auth(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned short status)
{
	unsigned char	*pbuf;
	unsigned short  val = 0;
	DECLARE_TXINSN(txinsn);
	
	txinsn.q_num = MANAGE_QUE_NUM;
    	txinsn.tx_rate = find_rate(priv, NULL, 0, 1);
#ifndef TX_LOWESTRATE
	txinsn.lowest_tx_rate = txinsn.tx_rate;
#endif	
	txinsn.fixed_rate = 1;
	txinsn.fr_type = _PRE_ALLOCMEM_;

	pbuf = txinsn.pframe = get_mgtbuf_from_poll(priv);
	if (pbuf == NULL)
		goto issue_ft_auth_fail;

	txinsn.phdr = get_wlanhdr_from_poll(priv);
	if (txinsn.phdr == NULL)
		goto issue_ft_auth_fail;

	memset((void *)(txinsn.phdr), 0, sizeof (struct	wlan_hdr));
	
	val = cpu_to_le16(_AUTH_ALGM_FT_); 
	pbuf = set_fixed_ie(pbuf, _AUTH_ALGM_NUM_, (unsigned char *)&val, &txinsn.fr_len);

	// setting transaction sequence number...
	if (!pstat->auth_seq)
		pstat->auth_seq++;

	val = cpu_to_le16(pstat->auth_seq);	
	pbuf = set_fixed_ie(pbuf, _AUTH_SEQ_NUM_, (unsigned char *)&val, &txinsn.fr_len);

	//setting status code...
	val = cpu_to_le16(status);
	pbuf = set_fixed_ie(pbuf, _STATUS_CODE_, (unsigned char *)&val, &txinsn.fr_len);

	pbuf = set_ft_rsnie_with_pmkid(priv, pbuf, &txinsn.fr_len, pstat, 0);		//PMKR0_name
	pbuf = construct_mobility_domain_ie(priv, pbuf, &txinsn.fr_len);
	pbuf = construct_fast_bss_transition_ie(priv, pbuf, &txinsn.fr_len, pstat);

	SetFrameSubType((txinsn.phdr), WIFI_AUTH);
	
	panic_printk("FT target Addr: (%s)line=%d, [%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n", __FUNCTION__, __LINE__, 
		priv->ft_target_bssid[0], priv->ft_target_bssid[1], priv->ft_target_bssid[2], priv->ft_target_bssid[3], priv->ft_target_bssid[4], priv->ft_target_bssid[5]);

	memcpy((void *)GetAddr1Ptr((txinsn.phdr)), priv->ft_target_bssid, MACADDRLEN);
	memcpy((void *)GetAddr2Ptr((txinsn.phdr)), GET_MY_HWADDR, MACADDRLEN);
	memcpy((void *)GetAddr3Ptr((txinsn.phdr)), priv->ft_target_bssid, MACADDRLEN);
	
	if((rtl8192cd_firetx(priv, &txinsn)) == SUCCESS) {
		panic_printk("Release Prvious AP's sta_info\n");
		release_stainfo(priv, pstat);
		priv->FT_CheckRoamTime = 0;
		return;

	}

issue_ft_auth_fail:

	if(txinsn.phdr)

		release_wlanhdr_to_poll(priv, txinsn.phdr);

	if (txinsn.pframe)

		release_mgtbuf_to_poll(priv, txinsn.pframe);

}

void start_fast_bss_transition(struct rtl8192cd_priv *priv)
{
	struct r1_key_holder *r1kh = NULL;
	struct stat_info *pstat = get_stainfo(priv, BSSID);
	WPA_STA_INFO	*pStaInfo = pstat->wpa_sta_info;
	r1kh = search_r1kh(priv, priv->init_ap, priv->init_ap);	

	panic_printk("start_fast_bss_transition (Over the air) ft_state = %d, CliFTAuthState =%d\n", pstat->ft_state, priv->CliFTAuthState);

	if(!memcmp(priv->ft_target_bssid, pstat->cmn_info.mac_addr, MACADDRLEN)) {
		panic_printk("Target addr is connecting! \n");
		return;
	}
	
	if(memcmp(priv->ft_target_bssid, null_mac, MACADDRLEN)) {
		if(pstat->ft.bss_num > 1) {
			if(pstat->ft_state == state_imd_4way 
			|| priv->CliFTAuthState == state_ft_auth_done) {
				pstat->ft_state = state_ft_auth_req;
				priv->CliFTAuthState = state_ft_auth_req;
				priv->init_r0kh_id_len = r1kh->r0kh_id_len;
				memcpy(priv->init_r0kh_id, r1kh->r0kh_id, r1kh->r0kh_id_len);
				regenerate_nonce(priv);
				issue_ft_auth(priv, pstat, 0);
			}
		}else
			panic_printk("(%s)line=%d, The bss num is less than 2 !! \n", __FUNCTION__, __LINE__);
	}else
		panic_printk("(%s)line=%d, No issue (Null mac) !! \n", __FUNCTION__, __LINE__);

}

void sync_to_target_bssInfo(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	unsigned char manual_idx = priv->pmib->dot11StationConfigEntry.ft_manual_idx;
	
	if(priv->pmib->dot11StationConfigEntry.dot11FTTestMode == FT_MENUAL_MODE) {	
		priv->ft_target_idx = pstat->ft.report[manual_idx].ss_index;
		priv->ft_target_channel = pstat->ft.report[manual_idx].channel;
		memcpy(priv->ft_target_t_stamp, pstat->ft.report[manual_idx].t_stamp, sizeof(int)*2);
	}else if (priv->pmib->dot11StationConfigEntry.dot11FTTestMode == FT_AUTO_MODE){
		priv->ft_target_idx = pstat->ft.report[0].ss_index;
		priv->ft_target_channel = pstat->ft.report[0].channel;
		memcpy(priv->ft_target_t_stamp, pstat->ft.report[0].t_stamp, sizeof(int)*2);
	}

	FT_CLI_DEBUG("targer BssInfo: idx(%d), chn = %d \n", priv->ft_target_idx, priv->ft_target_channel);
}

static void set_ft_target_bssid(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	panic_printk("dot11FTTestMode = %d \n", priv->pmib->dot11StationConfigEntry.dot11FTTestMode);

	if(priv->pmib->dot11StationConfigEntry.dot11FTTestMode == FT_MENUAL_MODE) {
		// set the roaming target without considering any condition
		memcpy(priv->ft_target_bssid, priv->pmib->dot11StationConfigEntry.ft_auth_mac, MACADDRLEN);
	}else if(priv->pmib->dot11StationConfigEntry.dot11FTTestMode == FT_AUTO_MODE) {
		if(memcmp(pstat->cmn_info.mac_addr, pstat->ft.report[0].bssid, MACADDRLEN)) {
		// set the roaming target in auto mode
			memcpy(priv->ft_target_bssid, pstat->ft.report[0].bssid, MACADDRLEN);	
		}else
			panic_printk("No need to roaming! Connecting AP has the best RSSI !\n");
	}

	sync_to_target_bssInfo(priv, pstat);
}

static int bss_pref_compar(const void *a, const void *b)
{
	const struct dot11r_ft_bss_report *aa = a;
	const struct dot11r_ft_bss_report *bb = b;

	if (!aa->rssi && !bb->rssi) 
		return 0;

	if (!aa->rssi) 
		return 1;

	if (!bb->rssi) 
		return -1;

	if (bb->rssi > aa->rssi)
		return 1;

	if (bb->rssi < aa->rssi)
		return -1;

	return 0;
}

static void update_ft_bss_report(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	int i = 0, num_empty_report = 0;

	if(pstat->ft.bss_num == 0) {
		panic_printk("Table is empty!! No need to update!!\n"); 
		return;
	}
	
	for(i = 0; i < pstat->ft.bss_num; i++) {
		if(!memcmp(pstat->ft.report[i].bssid, null_mac, MACADDRLEN)) {
			num_empty_report++;		
		}
	}

	pstat->ft.bss_num -= num_empty_report;
	//FT_CLI_DEBUG("wnm.num_neighbor_report = %d,   num_empty_report = %d\n", pstat->ft.bss_num, num_empty_report);
}

static void dump_ft_bss_list(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	unsigned char i;
	struct dot11r_ft_bss_report *report;

	for(i = 0; i < pstat->ft.bss_num; i++) {
		report = &pstat->ft.report[i];

		panic_printk("(%d)=>ss_index(%d), [%02x][%02x][%02x][%02x][%02x][%02x], rssi= %u, ch = %u \n", 
			i, report->ss_index, report->bssid[0], report->bssid[1], report->bssid[2], report->bssid[3], report->bssid[4], report->bssid[5], report->rssi, report->channel);
	}
}

static void sort_ft_bss_list(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	FT_CLI_DEBUG("FT BSS List num = %d \n",pstat->ft.bss_num);	

	if(pstat->ft.bss_num  == 0)
		return;

	qsort(pstat->ft.report, pstat->ft.report, sizeof(struct dot11r_ft_bss_report), bss_pref_compar);
	update_ft_bss_report(priv, pstat);
}

static void construct_ft_bss_info(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	int i = 0;
	int bss_info_size = sizeof(struct dot11r_ft_bss_report);
	memset(pstat->ft.report, 0, bss_info_size * BSS_NUM);
	
	for(i = 0; i < priv->site_survey->count_target; i++) {
		if(!memcmp(priv->pmib->dot11Bss.ssid, priv->site_survey->bss_target[i].ssid, priv->pmib->dot11Bss.ssidlen)) {
			memcpy(pstat->ft.report[i].bssid, priv->site_survey->bss_target[i].bssid, MACADDRLEN);
			FT_CLI_DEBUG("i = %d, ssid = %s \n", i, priv->site_survey->bss_target[i].ssid);
			pstat->ft.report[i].ss_index = i;
			pstat->ft.report[i].rssi = priv->site_survey->bss_target[i].rssi;
			pstat->ft.report[i].channel = priv->site_survey->bss_target[i].channel;
			memcpy(pstat->ft.report[i].t_stamp, priv->site_survey->bss_target[i].t_stamp, sizeof(int)*2);
			pstat->ft.bss_num++;
		}
	}
}

//This is used to verify the FT ovet the air flow  	
void FT_manualMode_process(struct rtl8192cd_priv *priv)
{
	struct stat_info *pstat = get_stainfo(priv, BSSID);

	if(pstat != NULL) {
		FT_CLI_DEBUG("pstat->ft_state = %d , CliFTAuthState = %d\n", pstat->ft_state, priv->CliFTAuthState);
		if(pstat->state & (WIFI_AUTH_SUCCESS | WIFI_ASOC_STATE)) {
			construct_ft_bss_info(priv, pstat);
			sort_ft_bss_list(priv, pstat);
#ifdef CONFIG_IEEE80211R_CLI_DEBUG
			dump_ft_bss_list(priv, pstat);
#endif
			set_ft_target_bssid(priv, pstat);
			clnt_switch_chan_to_bss(priv); 
			start_fast_bss_transition(priv);
		}else
			panic_printk(" Not associated! \n");
	}else
		panic_printk("(%s)line=%d, Can't find such station!\n", __FUNCTION__, __LINE__);

	priv->pmib->dot11StationConfigEntry.dot11FTTestMode = FT_DISABLE;
}


void FT_autoMode_process(struct rtl8192cd_priv *priv)
{
	struct stat_info *pstat = NULL;

	if(priv->pmib->dot11StationConfigEntry.dot11FTAuthReqTime) {
		priv->pmib->dot11StationConfigEntry.dot11FTAuthReqTime--;
		FT_CLI_DEBUG("FTAuthReqTime Counting down= %d\n", priv->pmib->dot11StationConfigEntry.dot11FTAuthReqTime);
		if(priv->pmib->dot11StationConfigEntry.dot11FTAuthReqTime == 0) {	
			FT_CLI_DEBUG("FT Check Time Up, Construct BSS List ");
			pstat = get_stainfo(priv, BSSID);
			if(pstat != NULL) {
				if(pstat->state & (WIFI_AUTH_SUCCESS | WIFI_ASOC_STATE)) {
					FT_CLI_DEBUG("pstat->ft_state = %d \n", pstat->ft_state);
					construct_ft_bss_info(priv, pstat);
					sort_ft_bss_list(priv, pstat);
			#ifdef CONFIG_IEEE80211R_CLI_DEBUG
					dump_ft_bss_list(priv, pstat);
			#endif
					set_ft_target_bssid(priv, pstat);
					priv->FT_CheckRoamTime = TRUE;
				}else
					panic_printk(" Not associated! \n");
			}else
				panic_printk("(%s)line=%d, Can't find such station!\n", __FUNCTION__, __LINE__);

			// Reset FT Auth Req Time
			priv->pmib->dot11StationConfigEntry.dot11FTAuthReqTime = FT_AUTH_REQ_TIME;
		}
	}
}

void FT_EventTimer(struct rtl8192cd_priv *priv)
{
	unsigned long flags;

	SAVE_INT_AND_CLI(flags);
	
	if(priv->pmib->dot11StationConfigEntry.dot11FTTestMode == FT_MENUAL_MODE) 	
		FT_manualMode_process(priv);
	else if(priv->pmib->dot11StationConfigEntry.dot11FTTestMode == FT_AUTO_MODE) {
		FT_autoMode_process(priv);
	}

	RESTORE_INT(flags);
}

void update_ft_bss(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	
	memcpy((void *)priv->pmib->dot11StationConfigEntry.dot11Bssid, (void *)pstat->cmn_info.mac_addr, MACADDRLEN);
	priv->pmib->dot11Bss.channel = priv->ft_target_channel;

	FT_CLI_DEBUG("dot11Bss.channel = %d, ft_target_channe = %d\n", priv->pmib->dot11Bss.channel, priv->ft_target_channel);
}



#endif
