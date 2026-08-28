

#ifdef __KERNEL__
#include <linux/module.h>
#include <asm/byteorder.h>
#elif defined(__ECOS)
#include <cyg/io/eth/rltk/819x/wrapper/sys_support.h>
#include <cyg/io/eth/rltk/819x/wrapper/skbuff.h>
#include <cyg/io/eth/rltk/819x/wrapper/timer.h>
#include <cyg/io/eth/rltk/819x/wrapper/wrapper.h>
#endif

#include "./8192cd_cfg.h"
#include "./8192cd.h"
#include "./8192cd_util.h"
#include "./8192cd_headers.h"
#include "./8192cd_debug.h"
#include "./8192cd_11v.h"

#if !defined(__KERNEL__) && !defined(__ECOS)
#include "../sys-support.h"
#endif

#ifdef __KERNEL__
#include "./ieee802_mib.h"
#elif defined(__ECOS)
#include <cyg/io/eth/rltk/819x/wlan/ieee802_mib.h>
#endif

#define _8192CD_11V_C_

extern void qsort (void  *base, int nel, int width, int (*comp)(const void *, const void *));
extern unsigned int issue_assocreq(struct rtl8192cd_priv *priv);

//++++++++ customized functions 
typedef enum _PREFERENCE_ALGO_ {
	ALGORITHM_0	= 0,
	ALGORITHM_1	= 1
} PREFERENCE_ALGO;

inline unsigned char calculation_method0(bool excluded, unsigned char channel_utilization)
{
	if(excluded)
		return 0;
	return (MAX_PREFERRED_VAL - channel_utilization);
}

unsigned char getPreferredVal(struct rtl8192cd_priv *priv, unsigned char channel_utilization, unsigned char rcpi, bool excluded)
{
	unsigned char retval = 0;
	switch(priv->pmib->wnmEntry.algoType)
	{	
		case ALGORITHM_0:
			retval = calculation_method0(0, channel_utilization);
			break;
		default:
			panic_printk("Undefined Algorithm Type! \n");
			break;
	}
}

void addEntryWaitingList(struct rtl8192cd_priv *priv, const struct response_waiting_list *list)
{
	int i, empty_slot;
	struct stat_info *pstat = get_stainfo(priv, list->addr);

	if(pstat == NULL)
		return;

	for(i = 0, empty_slot = -1; i < MAX_TRANS_LIST_NUM; i++)
	{
		if((priv->waiting_list_bitmask[i>>3] & (1<<(i&7))) == 0) {
			if(empty_slot == -1)
				empty_slot = i;
		}else if(0 == memcmp(list->addr, priv->waiting_list[i].addr, MACADDRLEN)) {
			if(memcmp(list->target_bssid, priv->waiting_list[i].target_bssid, MACADDRLEN) != 0){
				break;
			}
			else{
				return;
			}
		}
	}

	if(i == MAX_TRANS_LIST_NUM && empty_slot != -1) {/*not found, and has empty slot*/
		i = empty_slot;
	}
	memcpy(priv->waiting_list[i].addr, list->addr, MACADDRLEN);
	memcpy(priv->waiting_list[i].bssid, list->bssid, MACADDRLEN);
	memcpy(priv->waiting_list[i].target_bssid, list->target_bssid, MACADDRLEN);
	priv->waiting_list[i].retry_counter = list->retry_counter;
	priv->waiting_list[i].status = list->status;
	priv->waiting_list[i].response_timer = list->response_timer;

	priv->waiting_list_bitmask[i>>3] |= (1<<(i&7));

	DOT11VDEBUG("ADDED WAITING LIST STA (%d) :[%02x][%02x][%02x][%02x][%02x][%02x] \n",
					i, priv->waiting_list[i].addr[0], priv->waiting_list[i].addr[1], priv->waiting_list[i].addr[2],
					priv->waiting_list[i].addr[3], priv->waiting_list[i].addr[4], priv->waiting_list[i].addr[5]);
}

void deleteEntryWaitingList(struct rtl8192cd_priv *priv, const unsigned char* macAddress)
{
	int i;
	for (i = 0 ; i < MAX_TRANS_LIST_NUM; i++) {
		if((priv->waiting_list_bitmask[i>>3] & (1<<(i&7))) == 0)
			continue;

		if(0 == memcmp(macAddress, priv->waiting_list[i].addr, MACADDRLEN)) {

			DOT11VDEBUG("REMOVE WAITING LIST STA (%d) :[%02x][%02x][%02x][%02x][%02x][%02x] \n",
								i, priv->waiting_list[i].addr[0], priv->waiting_list[i].addr[1], priv->waiting_list[i].addr[2],
								priv->waiting_list[i].addr[3], priv->waiting_list[i].addr[4], priv->waiting_list[i].addr[5]);
			priv->waiting_list_bitmask[i>>3] &= ~(1<<(i&7));
			memset(&priv->waiting_list[i], 0, sizeof(struct response_waiting_list));
			break;
		}
	}
}

struct response_waiting_list *getEntryWaitingList(struct rtl8192cd_priv *priv, const unsigned char* macAddress){
	int i;
	for (i = 0 ; i < MAX_TRANS_LIST_NUM; i++) {
		if((priv->waiting_list_bitmask[i>>3] & (1<<(i&7))) == 0)
			continue;

		if(0 == memcmp(macAddress, priv->waiting_list[i].addr, MACADDRLEN)) {
			return &priv->waiting_list[i];
		}
	}
	return NULL;
}

int getRetryWaitingList(struct rtl8192cd_priv *priv, const unsigned char* macAddress){
	int i;
	for (i = 0 ; i < MAX_TRANS_LIST_NUM; i++) {
		if((priv->waiting_list_bitmask[i>>3] & (1<<(i&7))) == 0)
			continue;

		if(0 == memcmp(macAddress, priv->waiting_list[i].addr, MACADDRLEN)) {
			return priv->waiting_list[i].retry_counter;
		}
	}
	return -1;
}

void addEntryBlockList(struct rtl8192cd_priv *priv, const struct assoc_control_block_list *list)
{
	int i, empty_slot;

	for(i = 0, empty_slot = -1; i < MAX_TRANS_LIST_NUM; i++)
	{
		if((priv->block_list_bitmask[i>>3] & (1<<(i&7))) == 0) {
			if(empty_slot == -1)
				empty_slot = i;
		}else if(0 == memcmp(list->addr, priv->block_list[i].addr, MACADDRLEN)) {
			break;
		}
	}

	if(i == MAX_TRANS_LIST_NUM && empty_slot != -1) {/*not found, and has empty slot*/
		i = empty_slot;
	}
	memcpy(priv->block_list[i].addr, list->addr, MACADDRLEN);
	priv->block_list[i].timer = list->timer;

	priv->block_list_bitmask[i>>3] |= (1<<(i&7));

	DOT11VDEBUG("ADDED BLOCK LIST STA (%d) :[%02x][%02x][%02x][%02x][%02x][%02x] \n",
					i, priv->block_list[i].addr[0], priv->block_list[i].addr[1], priv->block_list[i].addr[2],
					priv->block_list[i].addr[3], priv->block_list[i].addr[4], priv->block_list[i].addr[5]);
}

void deleteEntryBlockList(struct rtl8192cd_priv *priv, const unsigned char* macAddress)
{
	int i;
	for (i = 0 ; i < MAX_TRANS_LIST_NUM; i++) {
		if((priv->block_list_bitmask[i>>3] & (1<<(i&7))) == 0)
			continue;

		if(0 == memcmp(macAddress, priv->block_list[i].addr, MACADDRLEN)) {

			DOT11VDEBUG("REMOVE BLOCK LIST STA (%d) :[%02x][%02x][%02x][%02x][%02x][%02x] \n",
								i, priv->block_list[i].addr[0], priv->block_list[i].addr[1], priv->block_list[i].addr[2],
								priv->block_list[i].addr[3], priv->block_list[i].addr[4], priv->block_list[i].addr[5]);
			priv->block_list_bitmask[i>>3] &= ~(1<<(i&7));
			memset(&priv->block_list[i], 0, sizeof(struct assoc_control_block_list));
			break;
		}
	}
}

void expireEntryBlockList(struct rtl8192cd_priv *priv)
{
	int i;
	for (i = 0 ; i < MAX_TRANS_LIST_NUM; i++) {
		if ((priv->block_list_bitmask[i>>3] & (1<<(i&7))) != 0) {
			if (priv->block_list[i].timer == 0) {
				priv->block_list_bitmask[i>>3] &= ~(1<<(i&7));
				DOT11VDEBUG("BLOCK LIST STA EXPIRES (%d) :[%02x][%02x][%02x][%02x][%02x][%02x] \n",
								i, priv->block_list[i].addr[0], priv->block_list[i].addr[1], priv->block_list[i].addr[2],
								priv->block_list[i].addr[3], priv->block_list[i].addr[4], priv->block_list[i].addr[5]);
				memset(&priv->block_list[i], 0, sizeof(struct assoc_control_block_list));
			} else {
				priv->block_list[i].timer--;
			}
		}
	}
}

struct assoc_control_block_list *getEntryBlockList(struct rtl8192cd_priv *priv, const unsigned char* macAddress){
	int i;
	for (i = 0 ; i < MAX_TRANS_LIST_NUM; i++) {
		if((priv->block_list_bitmask[i>>3] & (1<<(i&7))) == 0)
			continue;

		if(0 == memcmp(macAddress, priv->block_list[i].addr, MACADDRLEN)) {
			return &priv->block_list[i];
		}
	}
	return NULL;
}

void addEntryTransitionList(struct rtl8192cd_priv *priv, const struct target_transition_list *list)
{
	int i, empty_slot;
	struct stat_info *pstat = get_stainfo(priv, list->addr);

	if(pstat == NULL)
		return;

	for(i = 0, empty_slot = -1; i < MAX_TRANS_LIST_NUM; i++)
	{
		if((priv->transition_list_bitmask[i>>3] & (1<<(i&7))) == 0) {
			if(empty_slot == -1)
				empty_slot = i;
		}else if(0 == memcmp(list->addr, priv->transition_list[i].addr, MACADDRLEN)) {
			if(memcmp(list->target_bssid, priv->transition_list[i].target_bssid, MACADDRLEN) != 0){
				if(list->mode & BIT(6))
					pstat->bssTransDisassocCountdown = list->disassoc_timer / 100;
				else
					pstat->bssTransDisassocCountdown = 0;
				DOT11VDEBUG("UPDATED TRANSITION LIST STA (%d) :[%02x][%02x][%02x][%02x][%02x][%02x] \n",
					i, priv->transition_list[i].addr[0], priv->transition_list[i].addr[1], priv->transition_list[i].addr[2],
					priv->transition_list[i].addr[3], priv->transition_list[i].addr[4], priv->transition_list[i].addr[5]);
				break;
			}
			else{
				DOT11VDEBUG("EXIST TRANSITION LIST STA (%d) :[%02x][%02x][%02x][%02x][%02x][%02x] \n",
					i, priv->transition_list[i].addr[0], priv->transition_list[i].addr[1], priv->transition_list[i].addr[2],
					priv->transition_list[i].addr[3], priv->transition_list[i].addr[4], priv->transition_list[i].addr[5]);
				return;
			}
		}
	}

	if(i == MAX_TRANS_LIST_NUM && empty_slot != -1) {/*not found, and has empty slot*/
		i = empty_slot;
		if(list->mode & BIT(6))
			pstat->bssTransDisassocCountdown = list->disassoc_timer / 100;
	}
	memcpy(priv->transition_list[i].addr, list->addr, MACADDRLEN);
	memcpy(priv->transition_list[i].target_bssid, list->target_bssid, MACADDRLEN);
	priv->transition_list[i].mode = list->mode;
	priv->transition_list[i].opclass = list->opclass;
	priv->transition_list[i].channel = list->channel;
	priv->transition_list[i].disassoc_timer = list->disassoc_timer;
	//priv->transition_list[i].disassoc = list->disassoc;

	priv->transition_list_bitmask[i>>3] |= (1<<(i&7));

	DOT11VDEBUG("ADDED TRANSITION LIST STA (%d) :[%02x][%02x][%02x][%02x][%02x][%02x] \n",
					i, priv->transition_list[i].addr[0], priv->transition_list[i].addr[1], priv->transition_list[i].addr[2],
					priv->transition_list[i].addr[3], priv->transition_list[i].addr[4], priv->transition_list[i].addr[5]);

}

void deleteEntryTransitionList(struct rtl8192cd_priv *priv, const unsigned char* macAddress)
{
	int i;
	for (i = 0 ; i < MAX_TRANS_LIST_NUM; i++) {
		if((priv->transition_list_bitmask[i>>3] & (1<<(i&7))) == 0)
			continue;
		printk("[%s:%d]\n", __FUNCTION__,__LINE__);
		if(0 == memcmp(macAddress, priv->transition_list[i].addr, MACADDRLEN)) {

			printk("REMOVE TRANSITION LIST STA (%d) :[%02x][%02x][%02x][%02x][%02x][%02x] \n",
								i, priv->transition_list[i].addr[0], priv->transition_list[i].addr[1], priv->transition_list[i].addr[2],
								priv->transition_list[i].addr[3], priv->transition_list[i].addr[4], priv->transition_list[i].addr[5]);
			priv->transition_list_bitmask[i>>3] &= ~(1<<(i&7));
			memset(&priv->transition_list[i], 0, sizeof(struct target_transition_list));
			break;
		}
		printk("[%s:%d]\n", __FUNCTION__,__LINE__);
	}
}

struct target_transition_list *getEntryTransitionList(struct rtl8192cd_priv *priv, const unsigned char* macAddress){
	int i;
	for (i = 0 ; i < MAX_TRANS_LIST_NUM; i++) {
		if((priv->transition_list_bitmask[i>>3] & (1<<(i&7))) == 0)
			continue;

		if(0 == memcmp(macAddress, priv->transition_list[i].addr, MACADDRLEN)) {
			return &priv->transition_list[i];
		}
	}
	return NULL;
}

#ifdef CONFIG_IEEE80211V_CLI

static int validate_target_bssid(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	int i = 0;

	for(i=priv->join_index+1; i<priv->site_survey->count_target; i++) {
		if(!memcmp(pstat->wnm.target_bssid, priv->site_survey->bss_target[i].bssid, MACADDRLEN)) {
			return 0;
		}
	}
	return -1;
}

#endif

//--------- customized functions 

#ifdef CONFIG_RTL_PROC_NEW
int rtl8192cd_proc_transition_list_read(struct seq_file *s, void *data)
#else
int rtl8192cd_proc_transition_list_read(char *buf, char **start, off_t offset,
		int length, int *eof, void *data)
#endif
{
	struct net_device *dev = PROC_GET_DEV();
	struct rtl8192cd_priv *priv = GET_DEV_PRIV(dev);
	int pos = 0;
	int i,j;
	struct stat_info *pstat;

	if((OPMODE & WIFI_AP_STATE) == 0) {
		panic_printk("\nwarning: invalid command!\n");
		return pos;
	}

	PRINT_ONE("-- Target Transition List --", "%s", 1);
	j = 1;
	for (i = 0 ; i < MAX_TRANS_LIST_NUM; i++) 
	{
		if((priv->transition_list_bitmask[i>>3] & (1<<(i&7))) == 0)
			continue;

		pstat = get_stainfo(priv, priv->transition_list[i].addr);
		if(pstat) {
			PRINT_ONE(j, "  [%d]", 0);
			PRINT_ARRAY_ARG("STA:", priv->transition_list[i].addr, "%02x", MACADDRLEN);
			PRINT_ONE("    BSS Trans Rejection Count:", "%s", 0);
			PRINT_ONE(pstat->bssTransRejectionCount, "%d", 1);
			PRINT_ONE("    BSS Trans Trans Expired Time:", "%s", 0);
			PRINT_ONE(pstat->bssTransExpiredTime, "%d", 1);
		}
		j++;
	}
	return pos;
}

#ifdef __ECOS
int rtl8192cd_proc_transition_list_write(char *tmp, void *data)
#else
int rtl8192cd_proc_transition_list_write(struct file *file, const char *buffer,
		unsigned long count, void *data)
#endif
{
#ifdef __ECOS
	return 0;
#else
	struct net_device *dev = (struct net_device *)data;
	struct rtl8192cd_priv *priv = GET_DEV_PRIV(dev);
	unsigned char error_code = 0;
	char * tokptr;
	int command = 0;
	int empty_slot;
	int i;
	char tmp[TRANS_LIST_PROC_LEN];
	char *tmpptr;
	struct target_transition_list list;
	unsigned int flags;

	if((OPMODE & WIFI_AP_STATE) == 0) {
		error_code = 1;
		goto end;
	}
	if (count < 2 || count >= TRANS_LIST_PROC_LEN) {
		return -EFAULT;
	}

	if (buffer == NULL || copy_from_user(tmp, buffer, count))
		return -EFAULT;

	tmp[count] = 0;
	tmpptr = tmp;
	tmpptr = strsep((char **)&tmpptr, "\n");
	tokptr = strsep((char **)&tmpptr, " ");
	if(!memcmp(tokptr, "add", 3))
		command = 1;
	else if (!memcmp(tokptr, "delall", 6)) 
		command = 3;
	else if(!memcmp(tokptr, "del", 3))
		command = 2;

	if(command) 
	{        
		if(command == 1 || command == 2) {
			tokptr = strsep((char **)&tmpptr," ");
			if(tokptr)
				get_array_val(list.addr, tokptr, 12);
			else {
				error_code = 1;
				goto end;
			}
		}
	    SAVE_INT_AND_CLI(flags);
		SMP_LOCK_TRANSITION_LIST(flags);
		if(command == 1)   /*add*/
		{
#ifdef RTK_MULTI_AP
	    	addEntryTransitionList(priv, &list);
#endif
#if 0
			for(i = 0, empty_slot = -1; i < MAX_TRANS_LIST_NUM; i++)
			{
				if((priv->transition_list_bitmask[i>>3] & (1<<(i&7))) == 0) {
					if(empty_slot == -1)
						empty_slot = i;
				}else if(0 == memcmp(list.addr, priv->transition_list[i].addr, MACADDRLEN)) {
					break;
				}
			}
				
			if(i == MAX_TRANS_LIST_NUM && empty_slot != -1) {/*not found, and has empty slot*/
				i = empty_slot;
			}
			memcpy(&priv->transition_list[i], &list, sizeof(struct target_transition_list));
		     	priv->transition_list_bitmask[i>>3] |= (1<<(i&7));
#endif
	    }
	    else if(command == 3)   /*delete all*/
	    {
	        memset(priv->transition_list_bitmask, 0x00, sizeof(priv->transition_list_bitmask));
	    }
	   	else if(command == 2)  /*delete*/
	   	{
#ifdef RTK_MULTI_AP
	   		deleteEntryTransitionList(priv, list.addr);
#endif
#if 0
			for (i = 0 ; i < MAX_TRANS_LIST_NUM; i++) {
				if((priv->transition_list_bitmask[i>>3] & (1<<(i&7))) == 0)
					continue;
			    
				if(0 == memcmp(list.addr, priv->transition_list[i].addr, MACADDRLEN)) {
					priv->transition_list_bitmask[i>>3] &= ~(1<<(i&7));
					break;
				}
			}
#endif
	    }
		SMP_UNLOCK_TRANSITION_LIST(flags);
		RESTORE_INT(flags);
	}
	else {
		error_code = 1;
		goto end;
	}

end:
	if(error_code == 1)
		panic_printk("\nwarning: invalid command!\n");
	else if(error_code == 2)
		panic_printk("\nwarning: neighbor report table full!\n");
	return count;
#endif
}

unsigned char * construct_target_neighbor_report_ie(struct rtl8192cd_priv *priv, unsigned char *pbuf, unsigned int *frlen, struct target_transition_list *entry)
{
	int i;
	unsigned int len = 16;
	*pbuf = _NEIGHBOR_REPORT_IE_;							//Information element ID
	*(pbuf + 1) = len;
	memset((pbuf + 2), 0,len);								//Length
#ifdef RTK_MULTI_AP
    memcpy((pbuf + 2), entry->target_bssid, MACADDRLEN);	//target BSSID
#else
	memcpy((pbuf + 2), &(priv->bssTransPara.bssid_mac), MACADDRLEN);	//target BSSID
#endif

/*	
	printk("Target BSSID Mac: %02x%02x%02x%02x%02x%02x\n",
	priv->bssTransPara.bssid_mac[0],priv->bssTransPara.bssid_mac[1],priv->bssTransPara.bssid_mac[2],
	priv->bssTransPara.bssid_mac[3],priv->bssTransPara.bssid_mac[4],priv->bssTransPara.bssid_mac[5]);
*/	
	//pbuf + 8 - BSSID Information
#ifdef RTK_MULTI_AP
	*(pbuf + 12) = entry->opclass;
	*(pbuf + 13) = entry->channel; //target channel number
#else
    //pbuf + 12 - Regulatory class
	*(pbuf + 13) = priv->bssTransPara.channel; //target channel number
#endif
	*(pbuf + 14) = 0; //phy type
	*(pbuf + 15) = _WNM_BSS_TRANS_CANDIDATE_PREFRENCE_; //subelement id
	*(pbuf + 16) = 1;				//length
	*(pbuf + 17) = 255;				//target preference value: default 1		
	*frlen = *frlen + (len + 2);
	pbuf = pbuf + len + 2;
	
	return pbuf;
}

unsigned char * construct_self_neighbor_report_ie(struct rtl8192cd_priv *priv, unsigned char *pbuf, unsigned int *frlen)
{
	struct dot11k_neighbor_report neighbor_report;
	unsigned int len = sizeof(struct dot11k_neighbor_report);

	*pbuf = _NEIGHBOR_REPORT_IE_;							//Information element ID
	*(pbuf + 1) = len;
	memcpy(neighbor_report.bssid, GET_MY_HWADDR, MACADDRLEN);
	neighbor_report.bssinfo.value = 0;
	neighbor_report.channel = priv->pmib->dot11RFEntry.dot11channel;
	neighbor_report.op_class = rm_get_op_class(priv, neighbor_report.channel);
	neighbor_report.phytype = 0;
	neighbor_report.subelemnt.subelement_id = _WNM_BSS_TRANS_CANDIDATE_PREFRENCE_;
	neighbor_report.subelemnt.len = 1;
	neighbor_report.subelemnt.preference = 0;

	memcpy(pbuf + 2, &neighbor_report, len);
	*frlen = *frlen + (len + 2);
	pbuf = pbuf + len + 2;

	return pbuf;

}

unsigned char * construct_bss_termination_ie(struct rtl8192cd_priv *priv, unsigned char *pbuf, unsigned int *frlen){

	unsigned int len = 10;
	UINT64 tsf;
	struct dot11v_bss_termination_subelement se;

	tsf = RTL_R32(TSFTR);
	tsf = tsf + (5 * 1000000); //5 seconds in microseconds

	se.subelement_id = _WNM_BSS_TERMINATION_DURATION_;
	se.len = len;
	se.bss_termination_tsf = cpu_to_le64(tsf);
	se.duration = cpu_to_le16(2);

	memcpy(pbuf, &se, 12);

	*frlen = *frlen + (len + 2);
	pbuf = pbuf + len + 2;

	return pbuf;
}

void send_bss_trans_event(struct rtl8192cd_priv *priv, struct stat_info *pstat, int i)
{
	int ret = 0;
#ifndef RTK_MULTI_AP
	if (priv->pmib->wnmEntry.dot11vDiassocDeadline)
		pstat->expire_to = priv->pmib->wnmEntry.dot11vDiassocDeadline;
	else
		pstat->expire_to = MAX_FTREASSOC_DEADLINE;
#endif
	pstat->bssTransPktSent++;
#ifdef RTK_MULTI_AP
    if(issue_BSS_Trans_Req(priv, pstat->cmn_info.mac_addr, NULL, NULL, 0) == SUCCESS) //SUCCESS
	{
		pstat->bssTransExpiredTime = 0;
		pstat->bssTransTriggered = TRUE;
		//priv->startCounting = TRUE;

		DOT11VDEBUG("Send BSS Trans Req to STA [SUCCESS]:[%02x][%02x][%02x][%02x][%02x][%02x] \n",
				pstat->cmn_info.mac_addr[0], pstat->cmn_info.mac_addr[1], pstat->cmn_info.mac_addr[2],
				pstat->cmn_info.mac_addr[3], pstat->cmn_info.mac_addr[4], pstat->cmn_info.mac_addr[5]);
	} else {
		DOT11VDEBUG("Send BSS Trans Req to STA [FAIL]:[%02x][%02x][%02x][%02x][%02x][%02x] \n",
				pstat->cmn_info.mac_addr[0], pstat->cmn_info.mac_addr[1], pstat->cmn_info.mac_addr[2],
				pstat->cmn_info.mac_addr[3], pstat->cmn_info.mac_addr[4], pstat->cmn_info.mac_addr[5]);
	}
#else
	/* From priv parameter */
	if(i == -1) {
		if(issue_BSS_Trans_Req(priv, priv->bssTransPara.addr, NULL, NULL, 0) == SUCCESS) {
			pstat->bssTransExpiredTime = 0;
			pstat->bssTransTriggered = TRUE;
			priv->startCounting = TRUE;
			priv->dot11vDiassocDeadline = priv->pmib->wnmEntry.dot11vDiassocDeadline;
			DOT11VDEBUG("Send BSS Trans Req to STA [SUCCESS]:[%02x][%02x][%02x][%02x][%02x][%02x] \n",
				priv->bssTransPara.addr[0], priv->bssTransPara.addr[1], priv->bssTransPara.addr[2],
				priv->bssTransPara.addr[3], priv->bssTransPara.addr[4], priv->bssTransPara.addr[5]);
		} else {
			DOT11VDEBUG("Send BSS Trans Req to STA [FAIL]:[%02x][%02x][%02x][%02x][%02x][%02x] \n",
					priv->bssTransPara.addr[0], priv->bssTransPara.addr[1], priv->bssTransPara.addr[2],
					priv->bssTransPara.addr[3], priv->bssTransPara.addr[4], priv->bssTransPara.addr[5]);
		}
	}
	/* From transition list */
	else {
		if(issue_BSS_Trans_Req(priv, priv->transition_list[i].addr, NULL, NULL, 0) == SUCCESS) {
			pstat->bssTransExpiredTime = 0;
			pstat->bssTransTriggered = TRUE;
			priv->startCounting = TRUE;
			priv->dot11vDiassocDeadline = priv->pmib->wnmEntry.dot11vDiassocDeadline;
			DOT11VDEBUG("Send BSS Trans Req to STA [SUCCESS]:[%02x][%02x][%02x][%02x][%02x][%02x] \n",
				priv->transition_list[i].addr[0], priv->transition_list[i].addr[1], priv->transition_list[i].addr[2], 
				priv->transition_list[i].addr[3], priv->transition_list[i].addr[4], priv->transition_list[i].addr[5]);
		} else {
			DOT11VDEBUG("Send BSS Trans Req to STA [FAIL]:[%02x][%02x][%02x][%02x][%02x][%02x] \n",
					priv->transition_list[i].addr[0], priv->transition_list[i].addr[1], priv->transition_list[i].addr[2],
					priv->transition_list[i].addr[3], priv->transition_list[i].addr[4], priv->transition_list[i].addr[5]);
		}
	}
#endif
}

void process_BssTransReq(struct rtl8192cd_priv *priv)
{
	struct stat_info *pstat;

	int i, j = 0;
	unsigned long flags;


	if(!IS_DRV_OPEN(priv))
		return;
	SAVE_INT_AND_CLI(flags);
	SMP_LOCK_TRANSITION_LIST(flags);
#ifdef RTK_MULTI_AP
	struct response_waiting_list *entry;

	for (i = 0; i < MAX_TRANS_LIST_NUM; i++) {
		if((priv->transition_list_bitmask[i>>3] & (1<<(i&7))) == 0) {
			j++;
			continue;
		}
		pstat = get_stainfo(priv, priv->transition_list[i].addr);

		if(pstat) {

			entry = getEntryWaitingList(priv, pstat->cmn_info.mac_addr);
			/* If AP has already sent BTM request but the STA didn't accept BTM request */
			if(entry && entry->status == BTM_FAILED){
				if((priv->transition_list[i].mode & BIT(6)) == 0) { /* disassociation imminent is not set, use BTM with disassociation imminent */
					priv->transition_list[i].mode |= BIT(6);
					pstat->bssTransDisassocCountdown = priv->transition_list[i].disassoc_timer / 100;
					send_bss_trans_event(priv, pstat, i);
					entry->retry_counter++;
					entry->status = BTM_SEND;
				}
			} else {
				if (pstat->bssTransTriggered != TRUE) {
					if(priv->transition_list[i].mode & BIT(6)) {
						pstat->bssTransDisassocCountdown = priv->transition_list[i].disassoc_timer / 100;
					}
					send_bss_trans_event(priv, pstat, i);
					if(entry) {
						entry->retry_counter++;
						entry->status = BTM_SEND;
					}
				}
			}

			/* Disassociation imminent */
			if(priv->transition_list[i].mode & BIT(6)) {
				if(pstat->bssTransDisassocCountdown == 0) {
					panic_printk("Disassoc imminent time out: issue diassoc to trigger bss transition!!\n");
					issue_disassoc(priv, pstat->cmn_info.mac_addr, _RSON_DISASSOC_DUE_BSS_TRANSITION);
					deleteEntryTransitionList(priv, pstat->cmn_info.mac_addr);
					deleteEntryWaitingList(priv, pstat->cmn_info.mac_addr);
					del_station(priv, pstat, 0);
				} else {
					pstat->bssTransDisassocCountdown--;
				}
			}
		}
	}
#else
	if(!priv->bssTransPara.FromUser) {  /* From issue_actionFrame() */
		/* From transition list */
		for (i = 0; i < MAX_TRANS_LIST_NUM; i++) {
			if((priv->transition_list_bitmask[i>>3] & (1<<(i&7))) == 0) {
				j++;
				continue;
			}
			pstat = get_stainfo(priv, priv->transition_list[i].addr);
			if(pstat) {
				if(!priv->pmib->wnmEntry.Is11kDaemonOn || pstat->rcvNeighborReport)  //collect neighbor report by dot11k daemon
					send_bss_trans_event(priv, pstat, i);
				else
					panic_printk("Target clients may not Ready yet!!\n");
			} else {
				panic_printk("No such station(%d):[%02x][%02x][%02x][%02x][%02x][%02x] \n", i,
								priv->transition_list[i].addr[0], priv->transition_list[i].addr[1], priv->transition_list[i].addr[2],
								priv->transition_list[i].addr[3], priv->transition_list[i].addr[4], priv->transition_list[i].addr[5]);
			}
		}
		if(j == MAX_TRANS_LIST_NUM)
			DOT11VTRACE("Transition List is empty !!\n");
	} else {  /* From set_BssTransPara() */
		/* From priv parameter */
		pstat = get_stainfo(priv, priv->bssTransPara.addr);
		if(pstat) {
#if CONFIG_DLINK_WIFI_11V_ADDITIONS
			send_bss_trans_event(priv, pstat, -1);
#else /* CONFIG_DLINK_WIFI_11V_ADDITIONS */
			if(!priv->pmib->wnmEntry.Is11kDaemonOn || pstat->rcvNeighborReport)  //collect neighbor report by dot11k daemon
				send_bss_trans_event(priv, pstat, -1);
			else
				panic_printk("Target clients may not Ready yet!!\n");
#endif /* CONFIG_DLINK_WIFI_11V_ADDITIONS */
		} else {
			panic_printk("No such station(%d):[%02x][%02x][%02x][%02x][%02x][%02x] \n", i,
							priv->bssTransPara.addr[0], priv->bssTransPara.addr[1], priv->bssTransPara.addr[2],
							priv->bssTransPara.addr[3], priv->bssTransPara.addr[4], priv->bssTransPara.addr[5]);
		}
	}
#endif
	SMP_UNLOCK_TRANSITION_LIST(flags);
	RESTORE_INT(flags);
}

void BssTrans_ExpiredTimer(struct rtl8192cd_priv *priv)
{
	int i;
	unsigned long flags;
	struct stat_info *pstat;
	struct response_waiting_list *entry;

	SAVE_INT_AND_CLI(flags);
	/* From transition list */
	for (i = 0; i < MAX_TRANS_LIST_NUM; i++) {
		if((priv->transition_list_bitmask[i>>3] & (1<<(i&7))) == 0) 
			continue;

#ifndef RTK_MULTI_AP
		pstat = get_stainfo(priv, priv->bssTransPara.addr);
#else	
		pstat = get_stainfo(priv, priv->transition_list[i].addr);
#endif
		if(pstat) {
			if(pstat->bssTransTriggered) 	// client does not reply bss trans request
				pstat->bssTransExpiredTime++;

			if(pstat->bssTransExpiredTime == EVENT_TIMEOUT){
				pstat->bssTransStatusCode = _TIMEOUT_STATUS_CODE_;
#ifdef RTK_MULTI_AP
				DOT11VDEBUG("TIMEOUT EXPIRE STA (%d) :[%02x][%02x][%02x][%02x][%02x][%02x] \n",
									i, priv->transition_list[i].addr[0], priv->transition_list[i].addr[1], priv->transition_list[i].addr[2],
									priv->transition_list[i].addr[3], priv->transition_list[i].addr[4], priv->transition_list[i].addr[5]);
				deleteEntryTransitionList(priv, priv->transition_list[i].addr);
#endif
			}
		}
	}
#ifdef RTK_MULTI_AP
	for (i = 0; i < MAX_TRANS_LIST_NUM; i++) {
		if((priv->waiting_list_bitmask[i>>3] & (1<<(i&7))) == 0)
			continue;

		pstat = get_stainfo(priv, priv->waiting_list[i].addr);

		if(pstat) {
			entry = getEntryWaitingList(priv, priv->waiting_list[i].addr);
			if(entry && entry->status == BTM_ACCEPT) {
				/* send deauth */
				issue_disassoc(priv, pstat->cmn_info.mac_addr, _RSON_DISASSOC_DUE_BSS_TRANSITION);
				deleteEntryWaitingList(priv, pstat->cmn_info.mac_addr);
				del_station(priv, pstat, 0);
			} else if(entry && entry->status == BTM_SEND) {
				if(entry->response_timer >= 2) { // 2 seconds wait for BTM response, otherwise BTM is indicated as "failed"
					entry->status = BTM_FAILED;
				} else {
					entry->response_timer++;
				}
			}
		} else {
			/* STA is no longer connected with AP */
			deleteEntryWaitingList(priv, priv->waiting_list[i].addr);
		}
	}
#else
	/* From priv parameter */
	pstat = get_stainfo(priv, priv->bssTransPara.addr);
	if(pstat) {
		if(pstat->bssTransTriggered) 	// client does not reply bss trans request
			pstat->bssTransExpiredTime++;

		if(pstat->bssTransExpiredTime == EVENT_TIMEOUT)
			pstat->bssTransStatusCode = _TIMEOUT_STATUS_CODE_;
	}
#endif
	RESTORE_INT(flags);
}

void BssTrans_DiassocTimer(struct rtl8192cd_priv *priv)
{
	int i;
	struct stat_info *pstat;
	unsigned long flags;
	
#ifndef RTK_MULTI_AP
	if(!priv->dot11vDiassocDeadline)
		priv->startCounting = FALSE;
	
	if(priv->startCounting == TRUE && priv->dot11vDiassocDeadline) {
		priv->dot11vDiassocDeadline--;
		DOT11VDEBUG("Counting down = %d\n", priv->dot11vDiassocDeadline);
		if(!priv->dot11vDiassocDeadline) {
			SAVE_INT_AND_CLI(flags);
			/* From transition list */
			for (i = 0; i < MAX_TRANS_LIST_NUM; i++) {
				if((priv->transition_list_bitmask[i>>3] & (1<<(i&7))) == 0) 
					continue;
				pstat = get_stainfo(priv, priv->transition_list[i].addr);
				if(!pstat) {
					panic_printk("Cant find associated STA (%02x%02x%02x%02x%02x%02x)\n",
						priv->transition_list[i].addr[0], priv->transition_list[i].addr[1], priv->transition_list[i].addr[2],
						priv->transition_list[i].addr[3], priv->transition_list[i].addr[4], priv->transition_list[i].addr[5]);
				} else {
					panic_printk("issue diassoc to trigger bss transition!!\n");
					issue_disassoc(priv, priv->transition_list[i].addr, _RSON_DISASSOC_DUE_BSS_TRANSITION);
					del_station(priv, pstat, 0);		
				}
				priv->startCounting  = FALSE;
			}   
			/* From priv parameter */
			pstat = get_stainfo(priv, priv->bssTransPara.addr);
			if(!pstat) {
				panic_printk("Cant find associated STA (%02x%02x%02x%02x%02x%02x)\n",
					priv->bssTransPara.addr[0], priv->bssTransPara.addr[1], priv->bssTransPara.addr[2],
					priv->bssTransPara.addr[3], priv->bssTransPara.addr[4], priv->bssTransPara.addr[5]);
			} else {
				panic_printk("issue diassoc to trigger bss transition!!\n");
				issue_disassoc(priv, priv->bssTransPara.addr, _RSON_DISASSOC_DUE_BSS_TRANSITION);
				del_station(priv, pstat, 0);
			}
			RESTORE_INT(flags);
		}
	}
#else
	SAVE_INT_AND_CLI(flags);
	for (i = 0; i < MAX_TRANS_LIST_NUM; i++) {
		if((priv->transition_list_bitmask[i>>3] & (1<<(i&7))) == 0)
			continue;
		pstat = get_stainfo(priv, priv->transition_list[i].addr);
		if(!pstat) {
			panic_printk("Cant find associated STA (%02x%02x%02x%02x%02x%02x)\n",
				pstat->cmn_info.mac_addr[0], pstat->cmn_info.mac_addr[1], pstat->cmn_info.mac_addr[2],
				pstat->cmn_info.mac_addr[3], pstat->cmn_info.mac_addr[4], pstat->cmn_info.mac_addr[5]);
			deleteEntryTransitionList(priv, pstat->cmn_info.mac_addr);
		} else {
			send_bss_trans_event(priv, pstat, -1);
			if(pstat->bssTransDisassocCountdown)
				pstat->bssTransDisassocCountdown--;

			DOT11VDEBUG("COUNTDOWN STA (%d) :[%02x][%02x][%02x][%02x][%02x][%02x] - %d\n",
								i, pstat->cmn_info.mac_addr[0], pstat->cmn_info.mac_addr[1], pstat->cmn_info.mac_addr[2],
								pstat->cmn_info.mac_addr[3], pstat->cmn_info.mac_addr[4], pstat->cmn_info.mac_addr[5], pstat->bssTransDisassocCountdown);

			if(pstat->bssTransDisassocCountdown == 0){
				if(priv->transition_list[i].mode & BIT(6)){
				panic_printk("issue diassoc to trigger bss transition!!\n");
				issue_disassoc(priv, pstat->cmn_info.mac_addr, _RSON_DISASSOC_DUE_BSS_TRANSITION);
				deleteEntryTransitionList(priv, pstat->cmn_info.mac_addr);
				del_station(priv, pstat, 0);
				}else{
					deleteEntryTransitionList(priv, pstat->cmn_info.mac_addr);
				}
			}
		}
	}
	RESTORE_INT(flags);
#endif
}

void reset_staBssTransStatus(struct stat_info *pstat)
{
	pstat->bssTransRejectionCount = 0;
	pstat->bssTransPktSent = 0;
	pstat->bssTransExpiredTime = 0;
	pstat->rcvNeighborReport = 0;
	pstat->bssTransTriggered = 0;
	pstat->bssTransDisassocCountdown = 0;
	pstat->bssTransStatusCode = _DEFAULT_STATUS_CODE_;
}

void set_staBssTransCap(struct stat_info *pstat, unsigned char *pframe, int frameLen, unsigned short ie_offset)
{
	int ie_len = 0;
	unsigned char *ext_cap = NULL;
	unsigned char *p = get_ie(pframe + WLAN_HDR_A3_LEN + ie_offset, _EXTENDED_CAP_IE_, &ie_len,  frameLen);
	
	if(p != NULL) {
		ext_cap = (unsigned char *)kmalloc(ie_len, GFP_ATOMIC);
		memcpy(ext_cap, p+2, ie_len);
		if(ext_cap[2] & _WNM_BSS_TRANS_SUPPORT_) {
			pstat->bssTransSupport = TRUE;

			panic_printk("(%s)line=%d, set normal extended capabilities IE \n", __FUNCTION__, __LINE__);
		}else {	// when there are 2 extended Capabilities IE (IOT issue with Fujitsu)
			panic_printk("(%s)line=%d, Set 2 extended capabilities IE! \n", __FUNCTION__, __LINE__);
			p = p + ie_len + 2;
			if(*p == _EXTENDED_CAP_IE_){
				ie_len = *(p+1);
				kfree(ext_cap);
				ext_cap = (unsigned char *)kmalloc(ie_len, GFP_ATOMIC);
				memcpy(ext_cap, p+2, ie_len);
				if(ext_cap[2] & _WNM_BSS_TRANS_SUPPORT_)
					pstat->bssTransSupport = TRUE;
			}
		}
		kfree(ext_cap);
	}
	pstat->bssTransStatusCode = _DEFAULT_STATUS_CODE_;
	DOT11VDEBUG("pstat->dot11v_support  %d\n", pstat->bssTransSupport);
}

#ifdef DOT11K
void set_BssTransPara(struct rtl8192cd_priv *priv, unsigned char *tmpbuf)
{
	int i, empty_slot;
#ifdef RTK_MULTI_AP
	unsigned int flags;
	struct target_transition_list list;
	struct response_waiting_list waiting_list;
	//priv->bssTransPara.FomUser = TRUE;
	//unsigned char empty_bssid[6] = {0x00,0x00,0x00,0x00,0x00,0x00};

	memset(&list, 0, sizeof(struct target_transition_list));
	memcpy(list.addr, tmpbuf, MACADDRLEN);
	memcpy(&list.mode, tmpbuf+6, 1);
	memcpy(&list.disassoc_timer, tmpbuf+7, 2);
	memcpy(list.target_bssid, tmpbuf+9, MACADDRLEN);
	memcpy(&list.opclass, tmpbuf+15, 1);
	memcpy(&list.channel, tmpbuf+16, 1);

	memset(&waiting_list, 0, sizeof(struct response_waiting_list));
	memcpy(waiting_list.addr, tmpbuf, MACADDRLEN);
	memcpy(waiting_list.bssid, BSSID, MACADDRLEN);
	memcpy(waiting_list.target_bssid, tmpbuf+9, MACADDRLEN);
	waiting_list.retry_counter = 0;
	waiting_list.status = BTM_IDLE;
	waiting_list.response_timer = 0;
#else

	memcpy(priv->bssTransPara.addr, tmpbuf, MACADDRLEN);
	memcpy(priv->bssTransPara.bssid_mac, tmpbuf+8, MACADDRLEN);
	memcpy(&priv->bssTransPara.channel, tmpbuf+14, 1);
	//priv->bssTransPara.chan_until = tmpbuf[15];
	priv->bssTransPara.chan_until = 0;

#if 0  /* mechanism: find a specific neighbor report and caluclate preference value by priv->bssTransPara.chan_until into */
	for(i = 0, empty_slot = -1; i < MAX_NEIGHBOR_REPORT; i++) {
		if((priv->rm_neighbor_bitmask[i>>3] & (1<<(i&7))) == 0) {
			if(empty_slot == -1)
				empty_slot = i;
		} else if(0 == memcmp(priv->bssTransPara.addr, priv->rm_neighbor_report[i].bssid, MACADDRLEN)) {
			 break;
		}
	}
	if(i == MAX_NEIGHBOR_REPORT && empty_slot != -1)   /*not found, and has empty slot*/
	{
		i = empty_slot;
	}

	panic_printk("(%s)line=%d, i = %d, Channel_Untilization = %d\n", __FUNCTION__, __LINE__, i, priv->bssTransPara.chan_until);
	priv->rm_neighbor_report[i].subelemnt.subelement_id = _WNM_BSS_TRANS_CANDIDATE_PREFRENCE_;
	priv->rm_neighbor_report[i].subelemnt.len = 1;
	priv->rm_neighbor_report[i].subelemnt.preference = MAX_PREFERRED_VAL - priv->bssTransPara.chan_until;
#else
	if(!priv->pmib->wnmEntry.Is11kDaemonOn && !(priv->bssTransPara.channel > 0)){
		/*Cannot construct neighbor report if no 11k report to reference and channel number is 0*/
		DOT11VDEBUG("Channel number is 0 and 11k daemon not running!\n");
		return;
	}
#endif
#endif

#ifdef RTK_MULTI_AP 
	struct stat_info *pstat = get_stainfo(priv, list.addr);
	if (pstat == NULL) {
		panic_printk("pstat is null!");
		return;
	}
	if (pstat->bssTransSupport == false) {
		panic_printk("issue disassoc to STA because the client doesn't support 11v!!\n");
		issue_disassoc(priv, pstat->cmn_info.mac_addr, _RSON_DISASSOC_DUE_BSS_TRANSITION);
		del_station(priv, pstat, 0);
		return;
	}

	pstat->bssTransTriggered = 0;

	SAVE_INT_AND_CLI(flags);

	SMP_LOCK_TRANSITION_LIST(flags);
	addEntryTransitionList(priv, &list);
	SMP_UNLOCK_TRANSITION_LIST(flags);

	SMP_LOCK_WAITING_LIST(flags);
	addEntryWaitingList(priv, &waiting_list);
	SMP_UNLOCK_WAITING_LIST(flags);

	RESTORE_INT(flags);

#else

	DOT11VTRACE("(%s) 11V BSS Trans Req (chan_until=%u)...\n", __FUNCTION__, priv->bssTransPara.chan_until);
#if 1
	priv->bssTransPara.FromUser = TRUE;
	process_BssTransReq(priv);
	priv->bssTransPara.FromUser = FALSE;
#endif
#endif
}
#ifdef RTK_MULTI_AP
int operate_BTM_disallowed_sta(struct rtl8192cd_priv *priv, unsigned char *tmpbuf)
{
	struct stat_info *pstat;
	unsigned char sta_mac[MACADDRLEN] = {0};
	memcpy(sta_mac, tmpbuf, MACADDRLEN);

	pstat = get_stainfo(priv, sta_mac);
	if (pstat) {
		panic_printk("%s - issue disassoc to STA because the client is BTM disallowed!!\n", priv->dev->name);
		issue_disassoc(priv, pstat->cmn_info.mac_addr, _RSON_DISASSOC_DUE_BSS_TRANSITION);
		del_station(priv, pstat, 0);
	} else {
		panic_printk("%s - pstat is NULL!!\n", priv->dev->name);
		return -1;
	}
	return 0;
}

int update_assoc_control_block_list(struct rtl8192cd_priv *priv, unsigned char *tmpbuf)
{
	unsigned int flags;
	struct assoc_control_block_list block_entry;
	unsigned char control;

	memset(&block_entry, 0, sizeof(struct assoc_control_block_list));
	memcpy(&block_entry.addr, tmpbuf, MACADDRLEN);
	memcpy(&block_entry.timer, tmpbuf+6, 2);
	memcpy(&control, tmpbuf+8, 1);
	SAVE_INT_AND_CLI(flags);
	SMP_LOCK_BLOCK_LIST(flags);

	if (control == 0x00) { // block STA
		addEntryBlockList(priv, &block_entry);
	}
	else if (control == 0x01) { // unblock STA
		deleteEntryBlockList(priv, block_entry.addr);
	} else {
		DOT11VTRACE("invalid control code: %02x!\n", control);
	}

	SMP_UNLOCK_BLOCK_LIST(flags);
	RESTORE_INT(flags);

	return 0;
}

int update_agent_steering_policy(struct rtl8192cd_priv *priv, unsigned char *tmpbuf)
{
	unsigned char steering_policy, chUtil_threshold, rcpi_threshold;

	steering_policy = tmpbuf[0];

	chUtil_threshold = tmpbuf[1];

	rcpi_threshold = tmpbuf[2];

	priv->pmib->multi_ap.multiap_steering_policy = steering_policy;

	priv->pmib->multi_ap.multiap_cu_threshold = chUtil_threshold;

	priv->pmib->multi_ap.multiap_steering_rcpi_threshold = rcpi_threshold;

	return 0;
}
#if 0
void rtk_multi_ap_agent_steering_trigger(struct rtl8192cd_priv *priv)
{
	struct stat_info *pstat;
	struct list_head *phead, *plist;
	phead = &priv->asoc_list;
	plist = phead;
	while ((plist = asoc_list_get_next(priv, plist)) != phead) {
		pstat = list_entry(plist, struct stat_info, asoc_list);
		if (pstat->bssTransTriggered != TRUE && (pstat->rssi << 1) < priv->pmib->multi_ap.multiap_steering_rcpi_threshold) {
			send_bss_trans_event(priv, pstat);
		}
		if (plist == plist->next) {
			asoc_list_unref(priv, pstat);
			break;
		}
	}
}
#endif
#endif
#endif // DOT11K

int issue_BSS_Trans_Req(struct rtl8192cd_priv *priv, unsigned char *da, unsigned char dialog_token,
		unsigned char *preferred_BSSID, unsigned char preferred_channel)
{	
	int ret;
	unsigned char *pbuf;
	unsigned int frlen = 0;
	int neighbor_size = 0, k =0;
	int report_found = 0;
#ifdef ROAMING_SUPPORT
	int target_bassid_found = 0;
#endif
    unsigned char req_mode, allReports, self=0;
	unsigned char emptyBssid[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	unsigned short diassoc_time;
	struct target_transition_list *entry = getEntryTransitionList(priv, da);
	struct stat_info *pstat = get_stainfo(priv, da);
#if defined(DOT11K) && defined(STA_CONTROL_BAND_TRANSITION)
	struct rtl8192cd_priv *priv_sc = NULL;
#endif

#ifndef RTK_MULTI_AP
    req_mode = priv->pmib->wnmEntry.dot11vReqMode;
	diassoc_time = priv->pmib->wnmEntry.dot11vDiassocDeadline;
#else
    req_mode=3;
	diassoc_time = 0;
#endif

	if(!pstat)
		return FAIL;

	DECLARE_TXINSN(txinsn);
#ifdef RTK_MULTI_AP
	if (entry != NULL && entry->mode & BIT(7)) { // steering mandate
		if (entry->mode & BIT(6)) {
			req_mode |= _WNM_DIASSOC_IMMINENT_;
		}
		if (entry->mode & BIT(5)) {
			req_mode |= _WNM_ABRIDGED_;
		}
	}

	diassoc_time = pstat->bssTransDisassocCountdown;
	if (entry) {
		diassoc_time = entry->disassoc_timer / 100; // TUs transfer to TBTTs
	}

	if(entry != NULL && memcmp(emptyBssid, entry->target_bssid, MACADDRLEN))
		allReports = 0;
	else
		allReports = 1;
	DOT11VTRACE("Req mode=%x, diassoc_time = %d, %x, all reports: %d\n", req_mode, diassoc_time, cpu_to_le16(diassoc_time), allReports);
#else
	DOT11VTRACE("Req mode=%x, diassoc_time = %d, %x\n", req_mode, diassoc_time, cpu_to_le16(diassoc_time));
	if(priv->bssTransPara.FromUser) {
		DOT11VTRACE("For station %02x%02x%02x%02x%02x%02x to target %02x%02x%02x%02x%02x%02x\n",
			da[0], da[1], da[2], da[3],da[4], da[5],
			priv->bssTransPara.bssid_mac[0],priv->bssTransPara.bssid_mac[1],priv->bssTransPara.bssid_mac[2],
			priv->bssTransPara.bssid_mac[3],priv->bssTransPara.bssid_mac[4],priv->bssTransPara.bssid_mac[5]);
	}
#endif
		
	txinsn.q_num = MANAGE_QUE_NUM;
	txinsn.fr_type = _PRE_ALLOCMEM_;
	txinsn.tx_rate = find_rate(priv, NULL, 0, 1);
#ifndef TX_LOWESTRATE	
	txinsn.lowest_tx_rate = txinsn.tx_rate;
#endif
	txinsn.fixed_rate = 1;

#ifdef CONFIG_IEEE80211W	
	 if(pstat)
		 txinsn.isPMF = pstat->isPMF;
	 else
		 txinsn.isPMF = 0;
#endif
	pbuf = txinsn.pframe = get_mgtbuf_from_poll(priv);
	if (pbuf == NULL)
		goto issue_wnm_bss_trans_fail;

	txinsn.phdr = get_wlanhdr_from_poll(priv);
	if (txinsn.phdr == NULL)
		goto issue_wnm_bss_trans_fail;

	memset((void *)(txinsn.phdr), 0, sizeof(struct wlan_hdr));

	pbuf[frlen++]= _WNM_CATEGORY_ID_; 
	pbuf[frlen++] = _BSS_TSMREQ_ACTION_ID_;

	if (!(++pstat->dialog_token))	// dialog token set to a non-zero value
		pstat->dialog_token++;
	
	//dialog_token(1): require mode field(1): dissoc timer(2): validity interval(1)
	if(req_mode & (_WNM_PREFERRED_CANDIDATE_LIST_|_WNM_ABRIDGED_)) {
		pbuf[frlen++] = pstat->dialog_token;
		pbuf[frlen++] = req_mode;
		 *(unsigned short *)(pbuf + frlen) = cpu_to_le16(diassoc_time);
		frlen += 2;
		pbuf[frlen++] = 50;	//validity interval
		pbuf += frlen;
#ifdef RTK_MULTI_AP
		if(req_mode & 8){
			pbuf = construct_bss_termination_ie(priv, pbuf, &frlen);
		}
		if(!allReports){
			pbuf = construct_target_neighbor_report_ie(priv, pbuf, &frlen, entry);
		} else
#endif
#ifdef DOT11K
#ifdef STA_CONTROL_BAND_TRANSITION
		if (priv->stactrl.stactrl_status && priv->pmib->staControl.stactrl_band_transit) {
			priv_sc = priv->stactrl.stactrl_priv_sc;
			// Tansit band if prefered channel & BSSID are equivlant to those of another band
			if ((preferred_channel == (GET_MIB(priv_sc))->dot11RFEntry.dot11channel) &&
					(0 == memcmp(preferred_BSSID, (GET_MIB(priv_sc))->dot11StationConfigEntry.dot11Bssid, MACADDRLEN))) {
				pbuf = construct_neighbor_report_ie(pbuf, &frlen, &priv_sc->band_trans_neighbor_report);
				goto neighbor_report_ie_constructed;
			}
		}
#endif // STA_CONTROL_BAND_TRANSITIO
		neighbor_size = sizeof(struct dot11k_neighbor_report);
		for(k = 0; k < MAX_NEIGHBOR_REPORT; k++) {
#ifdef ROAMING_SUPPORT
			if(preferred_BSSID && preferred_channel)
			{
				if(memcmp(priv->rm_neighbor_report[k].bssid, preferred_BSSID, MACADDRLEN) || (priv->rm_neighbor_report[k].op_class && (rm_get_op_class(priv,preferred_channel) != priv->rm_neighbor_report[k].op_class)))
					continue;
				else {
					target_bassid_found = 1;
					DEBUG_INFO("[ROAMING] BSSID found in neighbor report list\n");
				}
			}
#endif
			if((priv->rm_neighbor_bitmask[k>>3] & (1<<(k&7))) == 0)
				continue;
			if(frlen + neighbor_size > MAX_REPORT_FRAME_SIZE)
				break;
			if(priv->bssTransPara.FromUser) {  /* from set_BssTransPara() */
				if(!memcmp(priv->rm_neighbor_report[k].bssid ,priv->bssTransPara.bssid_mac, MACADDRLEN)) {	
					unsigned char tmp_preference = priv->rm_neighbor_report[k].subelemnt.preference;
					priv->rm_neighbor_report[k].subelemnt.preference = MAX_PREFERRED_VAL - priv->bssTransPara.chan_until;
					pbuf = construct_neighbor_report_ie(pbuf, &frlen, &priv->rm_neighbor_report[k]);
					priv->rm_neighbor_report[k].subelemnt.preference = tmp_preference;
					report_found = 1;
					DOT11VTRACE("Use existing 11k report\n");
					break;
				}
			}
			else {
				pbuf = construct_neighbor_report_ie(pbuf, &frlen, &priv->rm_neighbor_report[k]);
				report_found = 1;
			}
		}
#ifdef ROAMING_SUPPORT
		if((preferred_BSSID && preferred_channel) && target_bassid_found == 0)
			goto issue_wnm_bss_trans_fail;
#endif // DOT11K
#endif
#ifdef RTK_MULTI_AP
#ifdef CONFIG_MBO
		pbuf = construct_btm_req_mbo_ie(priv, pbuf, &frlen, entry);
#endif
#else

		if(!report_found && priv->bssTransPara.FromUser)  /* from set_BssTransPara() */
			pbuf = construct_target_neighbor_report_ie(priv, pbuf, &frlen, NULL);
#endif
	}
	else {
		panic_printk("Type2 : Bss Trans Req with no neighbor report \n"); 	
		pbuf[frlen++] = pstat->dialog_token;			
		pbuf[frlen++] = 0;
		 *(unsigned short *)(pbuf + frlen) = cpu_to_le16(0);	
		frlen += 2;
		pbuf[frlen++] = 1;
		pbuf += frlen;
	}

neighbor_report_ie_constructed:
	txinsn.fr_len = frlen;
	SetFrameSubType((txinsn.phdr), WIFI_WMM_ACTION);
#ifdef CONFIG_IEEE80211W	
	if (txinsn.isPMF)
		*(unsigned char*)(txinsn.phdr+1) |= BIT(6); // enable privacy 
#endif
	memcpy((void *)GetAddr1Ptr((txinsn.phdr)), da, MACADDRLEN);
	memcpy((void *)GetAddr2Ptr((txinsn.phdr)), GET_MY_HWADDR, MACADDRLEN);
	memcpy((void *)GetAddr3Ptr((txinsn.phdr)), BSSID, MACADDRLEN);
	
	if (pstat == NULL)
		goto issue_wnm_bss_trans_fail;
		
	txinsn.pstat = pstat;
#if defined(WIFI_WMM)
	ret = check_dz_mgmt(priv, pstat, &txinsn);
	if (ret < 0)
		goto issue_wnm_bss_trans_fail;
	else if (ret==1)
		return SUCCESS;
	else
#endif
	if ((rtl8192cd_firetx(priv, &txinsn)) == SUCCESS) {
		return SUCCESS;
	}
	
issue_wnm_bss_trans_fail:

	if (txinsn.phdr)
		release_wlanhdr_to_poll(priv, txinsn.phdr);
	if (txinsn.pframe)
		release_mgtbuf_to_poll(priv, txinsn.pframe);

	return FAIL;
}

#ifdef CONFIG_IEEE80211V_CLI
static void reset_nieghbor_list_pref_val(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	int i = 0;
	struct dot11k_neighbor_report *report;

	if(pstat->wnm.num_neighbor_report == 0) {
		panic_printk("(%s)line=%d, Table is empty!! No need to update!! \n", __FUNCTION__, __LINE__); 
		return;
	}
	
	DOT11VTRACE(" Candidate List valid timeout !!\n");
	for (i = 0 ; i < pstat->wnm.num_neighbor_report; i++) {
		report = &pstat->wnm.neighbor_report[i];
		report->subelemnt.preference = priv->pmib->wnmEntry.defaultPrefVal;
	}
}

void BssTrans_ValidatePrefListTimer(struct rtl8192cd_priv *priv)
{
	struct stat_info *pstat;
	pstat = get_stainfo(priv, priv->pmib->dot11StationConfigEntry.dot11Bssid);
	if(pstat) {
		if(pstat->wnm.candidate_valid_time > 0) {
			DOT11VTRACE("candidate_valid_time = %d\n", pstat->wnm.candidate_valid_time);
			pstat->wnm.candidate_valid_time--;
			
			if(pstat->wnm.candidate_valid_time == 0) 
				reset_nieghbor_list_pref_val(priv, pstat);
		}	
	}
}

void BssTrans_TerminationTimer(struct rtl8192cd_priv *priv)
{
	struct stat_info *pstat;
	pstat = get_stainfo(priv, priv->pmib->dot11StationConfigEntry.dot11Bssid);
	if(pstat) {
		if(pstat->wnm.dissoc_timer > 0) {
			DOT11VTRACE("bss_termination_duration = %d\n", pstat->wnm.dissoc_timer);
			pstat->wnm.dissoc_timer--;
			
			if(pstat->wnm.candidate_valid_time == 0) {
				DOT11VDEBUG("bss_termination_duration is time up, diassoc to current AP \n");
				//Assume the termination from the AP is collided, so client send diassoc to connecting AP
				issue_disassoc(priv, pstat->cmn_info.mac_addr, _RSON_DISASSOC_DUE_BSS_TRANSITION);
				del_station(priv, pstat, 0);
			}	
		}	
	}
}

// optional: can use function call this API. At present, it's useless
int issue_BSS_Trans_Query(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char reason)
{
	int ret;
	unsigned char *pbuf;
	unsigned int frlen = 0;

	DECLARE_TXINSN(txinsn);
	txinsn.q_num = MANAGE_QUE_NUM;
	txinsn.fr_type = _PRE_ALLOCMEM_;
	txinsn.tx_rate = find_rate(priv, NULL, 0, 1);
#ifndef TX_LOWESTRATE	
	txinsn.lowest_tx_rate = txinsn.tx_rate;
#endif
	txinsn.fixed_rate = 1;

#ifdef CONFIG_IEEE80211W	
	 if(pstat)
		txinsn.isPMF = pstat->isPMF;
	 else
		txinsn.isPMF = 0;
#endif
	pbuf = txinsn.pframe = get_mgtbuf_from_poll(priv);
	if (pbuf == NULL)
		goto issue_bss_trans_query_fail;

	txinsn.phdr = get_wlanhdr_from_poll(priv);
	if (txinsn.phdr == NULL)
		goto issue_bss_trans_query_fail;

	memset((void *)(txinsn.phdr), 0, sizeof(struct wlan_hdr));

	if (!(++pstat->dialog_token))	// dialog token set to a non-zero value
		pstat->dialog_token++;
	
	pbuf[frlen++] = _WNM_CATEGORY_ID_;
	pbuf[frlen++] = _WNM_TSMQUERY_ACTION_ID_;
	pbuf[frlen++] = pstat->dialog_token;
	pbuf[frlen++] = reason;
		
	txinsn.fr_len += frlen;
	SetFrameSubType((txinsn.phdr), WIFI_WMM_ACTION);
#ifdef CONFIG_IEEE80211W
	if (txinsn.isPMF)
	*(unsigned char*)(txinsn.phdr+1) |= BIT(6); // enable privacy
#endif
	memcpy((void *)GetAddr1Ptr((txinsn.phdr)), pstat->cmn_info.mac_addr, MACADDRLEN);
	memcpy((void *)GetAddr2Ptr((txinsn.phdr)), GET_MY_HWADDR, MACADDRLEN);
	memcpy((void *)GetAddr3Ptr((txinsn.phdr)), BSSID, MACADDRLEN);

#if defined(WIFI_WMM)
	ret = check_dz_mgmt(priv, pstat, &txinsn);
	if (ret < 0)
		goto issue_bss_trans_query_fail;
	else if (ret==1)
		return 0;
	else
#endif
	if ((rtl8192cd_firetx(priv, &txinsn)) == SUCCESS)
		return 0;
		
issue_bss_trans_query_fail:
	if (txinsn.phdr)
		release_wlanhdr_to_poll(priv, txinsn.phdr);
	if (txinsn.pframe)
		release_mgtbuf_to_poll(priv, txinsn.pframe);

	return -1;
}

int issue_BSS_Trans_Rsp(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char dialog_token, enum bss_trans_mgmt_status_code status)
{
	int ret;
	unsigned char *pbuf;
	unsigned int frlen = 0;
    	
	DECLARE_TXINSN(txinsn);
	txinsn.q_num = MANAGE_QUE_NUM;
	txinsn.fr_type = _PRE_ALLOCMEM_;
	txinsn.tx_rate = find_rate(priv, NULL, 0, 1);
#ifndef TX_LOWESTRATE	
	txinsn.lowest_tx_rate = txinsn.tx_rate;
#endif
	txinsn.fixed_rate = 1;

#ifdef CONFIG_IEEE80211W	
	if(pstat)
		txinsn.isPMF = pstat->isPMF;
	else
		txinsn.isPMF = 0;
#endif
	pbuf = txinsn.pframe = get_mgtbuf_from_poll(priv);
	if (pbuf == NULL)
		goto issue_bss_trans_rsp_fail;

	txinsn.phdr = get_wlanhdr_from_poll(priv);
	if (txinsn.phdr == NULL)
		goto issue_bss_trans_rsp_fail;

	memset((void *)(txinsn.phdr), 0, sizeof(struct wlan_hdr));

	pbuf[frlen++] = _WNM_CATEGORY_ID_;
	pbuf[frlen++] = _BSS_TSMRSP_ACTION_ID_;
	pbuf[frlen++] = dialog_token;
	pbuf[frlen++] = status;
	pbuf[frlen++] = 0;	// BSS Termination Delay

	if(pstat->wnm.target_bssid) {
		memcpy(pbuf+frlen, pstat->wnm.target_bssid, MACADDRLEN);
	}else if(status == WNM_BSS_TM_ACCEPT) {
		/*
		 * P802.11-REVmc clarifies that the Target BSSID field is always
		 * present when status code is zero, so use a fake value here if
		 * no BSSID is yet known.
		 */
		memset(pbuf+frlen, 0, MACADDRLEN);
	}
	
	DOT11VDEBUG("Target bssid:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x] \n",  
				pstat->wnm.target_bssid[0], pstat->wnm.target_bssid[1], pstat->wnm.target_bssid[2], 
				pstat->wnm.target_bssid[3], pstat->wnm.target_bssid[4], pstat->wnm.target_bssid[5]);

	frlen += MACADDRLEN;
	txinsn.fr_len += frlen;
	SetFrameSubType((txinsn.phdr), WIFI_WMM_ACTION);
#ifdef CONFIG_IEEE80211W
	if (txinsn.isPMF)
	*(unsigned char*)(txinsn.phdr+1) |= BIT(6); // enable privacy
#endif
	memcpy((void *)GetAddr1Ptr((txinsn.phdr)), pstat->cmn_info.mac_addr, MACADDRLEN);
	memcpy((void *)GetAddr2Ptr((txinsn.phdr)), GET_MY_HWADDR, MACADDRLEN);
	memcpy((void *)GetAddr3Ptr((txinsn.phdr)), BSSID, MACADDRLEN);

#if defined(WIFI_WMM)
	ret = check_dz_mgmt(priv, pstat, &txinsn);
	if (ret < 0)
		goto issue_bss_trans_rsp_fail;
	else if (ret==1)
		return 0;
	else
#endif
	if ((rtl8192cd_firetx(priv, &txinsn)) == SUCCESS)
		return 0;
		
issue_bss_trans_rsp_fail:
	if (txinsn.phdr)
		release_wlanhdr_to_poll(priv, txinsn.phdr);
	if (txinsn.pframe)
		release_mgtbuf_to_poll(priv, txinsn.pframe);

	return -1;
}

static int cand_pref_compar(const void *a, const void *b)
{
	const struct dot11k_neighbor_report *aa = a;
	const struct dot11k_neighbor_report *bb = b;

	if (!aa->subelemnt.preference && !bb->subelemnt.preference) {
		return 0;
	}
	if (!aa->subelemnt.preference) {
		return 1;
	}
	if (!bb->subelemnt.preference) {
		return -1;
	}
	
	if (bb->subelemnt.preference > aa->subelemnt.preference)
		return 1;
	if (bb->subelemnt.preference < aa->subelemnt.preference)
		return -1;
	
	return 0;
}

static void update_neighbor_report(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	int i = 0, num_empty_report = 0;
	unsigned char null_mac[] = {0,0,0,0,0,0};
	
	if(pstat->wnm.num_neighbor_report == 0) {
		panic_printk("(%s)line=%d, Table is empty!! No need to update!! \n", __FUNCTION__, __LINE__); 
		return;
	}

	for(i = 0; i < pstat->wnm.num_neighbor_report; i++) {
		if(!memcmp(pstat->wnm.neighbor_report[i].bssid, null_mac, MACADDRLEN)) {
			num_empty_report++;		
		}
	}
	
	pstat->wnm.num_neighbor_report -= num_empty_report;
	DOT11VTRACE("wnm.num_neighbor_report = %d,   num_empty_report = %d\n", pstat->wnm.num_neighbor_report, num_empty_report);
}

static void start_bss_transition(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	if(!memcmp(pstat->wnm.target_bssid, pstat->cmn_info.mac_addr, MACADDRLEN)) {
		DOT11VDEBUG("No need to roam!(The Same Bssid) \n"); 
	} else {
		if(validate_target_bssid(priv, pstat) == 0) {
			DOT11VDEBUG("start bss transition!\n"); 

			// diassoc the present connecting AP
			issue_disassoc(priv, pstat->cmn_info.mac_addr, _RSON_DISASSOC_DUE_BSS_TRANSITION);
			del_station(priv, pstat, 0);

			// set the roaming target
			memcpy(priv->pmib->dot11StationConfigEntry.dot11DesiredBssid ,pstat->wnm.target_bssid, MACADDRLEN);
		}else
			DOT11VDEBUG("Validate NG: Client couldn't find target !\n");
	}
}

static void set_target_bssid(struct stat_info *pstat)
{
	if (pstat->wnm.num_neighbor_report == 0)
		return;
	
	memcpy(pstat->wnm.target_bssid, pstat->wnm.neighbor_report[0].bssid, MACADDRLEN);	// preference highest one
}

// for debug usage
static void dump_cand_list(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	unsigned char i;
	struct dot11k_neighbor_report *report;
	
	for(i = 0; i < pstat->wnm.num_neighbor_report; i++) {
		report = &pstat->wnm.neighbor_report[i];
		
		panic_printk("(%d)=>[%02x][%02x][%02x][%02x][%02x][%02x], bssinfo=%u, op_class=%u chan=%u phy=%u, prf = %d \n", 
		i, report->bssid[0], report->bssid[1], report->bssid[2], report->bssid[3], report->bssid[4], report->bssid[5],
		report->bssinfo.value, report->op_class, report->channel, report->phytype, report->subelemnt.preference);
	}
}

static void sort_candidate_list(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	DOT11VDEBUG("Candidate List num = %d\n",pstat->wnm.num_neighbor_report);
	if (pstat->wnm.num_neighbor_report == 0)
		return;

	qsort(pstat->wnm.neighbor_report, pstat->wnm.num_neighbor_report, sizeof(struct dot11k_neighbor_report), cand_pref_compar);
	update_neighbor_report(priv, pstat);
}

static void parse_subelement(struct rtl8192cd_priv *priv, unsigned char id
							,unsigned char *pos, unsigned char elen, struct dot11k_neighbor_report *report)
{
	report->subelemnt.subelement_id = id;
	
	switch(id) {
		case _WNM_BSS_TRANS_CANDIDATE_PREFRENCE_:
			if(elen < 1) {
				panic_printk("Too short BSS transition candidate \n");
				break;
			}
			report->subelemnt.len = elen;
			report->subelemnt.preference = pos[0];
			break;
		default:			
			panic_printk("Not implemented subelement id! \n");
			break;
	}
}

static void parse_neighbor_report(struct rtl8192cd_priv *priv, unsigned char *pos, unsigned char len, 
									struct dot11k_neighbor_report *report)
{
	unsigned char left = len;
	if(left < NEIGHBOR_REPORT_SIZE) {
		panic_printk("Too short neighbor report \n");
		return;
	}

	memcpy(report->bssid, pos, MACADDRLEN);
	pos += MACADDRLEN;	
	report->bssinfo.value = le32_to_cpu(*(unsigned int *)&pos[0]);
	pos += sizeof(int);		
	report->op_class = *pos;
	pos++;				
	report->channel = *pos;
	pos++;				
	report->phytype = *pos;
	pos++;			

	left -= 13; 
	
	DOT11VTRACE("[%02x][%02x][%02x][%02x][%02x][%02x], bssinfo=%u, op_class=%u chan=%u phy=%u \n",
		report->bssid[0], report->bssid[1], report->bssid[2], report->bssid[3], report->bssid[4], report->bssid[5],
		report->bssinfo.value, report->op_class, report->channel, report->phytype);
	while(left >= 2) {
		unsigned char id, elen;
		id = *pos++;
		elen = *pos++;
		
		left -= 2;	
		if(elen > left) {
			DOT11VDEBUG("Truncated neighbor report subelement \n");
			break;
		}
		parse_subelement(priv, id, pos, elen, report);
		left -= elen;
		pos += elen;
	}
	
}

void OnBSSTransReq(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *pframe, int frame_len)
{
	int frlen = 0;
	enum bss_trans_mgmt_status_code status;
	unsigned char valid_int;
	unsigned int beacon_int = 100;	
	unsigned char *pos = (pframe+2);

	if(frame_len <5) {
		panic_printk(" Ignore too short BSS Transition Management Request!\n"); 
		return;
	}

	pstat->wnm.dialog_token = pos[0];
	pstat->wnm.req_mode = pos[1];
	pstat->wnm.dissoc_timer = le16_to_cpu(*(unsigned short *) &pos[2]); 
	pstat->wnm.reply = TRUE;
	valid_int = pos[4];
	
	DOT11VDEBUG("BSS Trans Req: dialog_token=%u, req_mode=0x%x, dissoc_timer=%u, valid_int=%u, frame_len = %d \n",
		   			pstat->wnm.dialog_token, pstat->wnm.req_mode, pstat->wnm.dissoc_timer, valid_int, frame_len);

	pos += 5;
	frlen += 7;
	if(pstat->wnm.req_mode & _WNM_BSS_TERMINATION_INCLUDED_) {
		DOT11VTRACE("BSS termination included \n");
		if(frlen + 12 > frame_len) {	//Bss Trans duration 0 or 12
			panic_printk("Too short BSS TM Request \n");
			return;
		}
		memcpy(pstat->wnm.bss_termination_duration, pos, BSS_TERMINATION_DURATION_LEN);
		pos  += BSS_TERMINATION_DURATION_LEN;
		frlen += BSS_TERMINATION_DURATION_LEN;
	}

	if(pstat->wnm.req_mode & _WNM_ESS_DIASSOC_IMMINENT_) {
		unsigned char url[256];
		if (frlen + 1 > frame_len || frlen + 1 + pos[1] > frame_len) {
			panic_printk("Invalid BSS Transition Management Request (URL)");
			return;
		}
		memcpy(url, pos+1, pos[0]); 
		url[pos[0]] ='\0';
		pos += 1 + pos[0];
		frlen += 1 + pos[0];
		DOT11VTRACE("ESS_DISASSOC_IMMINENT(disassoc_timer: %u, url: %s)", pstat->wnm.dissoc_timer * beacon_int * 128 / 125, url);
	}

	if(pstat->wnm.req_mode & _WNM_DIASSOC_IMMINENT_) {
		DOT11VTRACE("Disassociation Imminent: dissoc_timer:%u\n", pstat->wnm.dissoc_timer);
		if(pstat->wnm.dissoc_timer) {
			start_clnt_lookup(priv, 1); //start rescan(needed to check)
		}
	}

	if(pstat->wnm.req_mode & _WNM_PREFERRED_CANDIDATE_LIST_) {
		DOT11VTRACE("PREFERRED_CANDIDATE_LIST: \n");
		unsigned int valid_ms;
		
		int neighbor_size = sizeof(struct dot11k_neighbor_report);
		memset(pstat->wnm.neighbor_report, 0, neighbor_size * MAX_NEIGHBOR_REPORT);	

		while((frlen + 2 <= frame_len) &&
		   	pstat->wnm.num_neighbor_report < MAX_NEIGHBOR_REPORT)
		 {	
	   		unsigned char tag = *pos++;
			unsigned char len = *pos++;
			frlen += 2;

			if(frlen + len > frame_len) {
				panic_printk("Truncated request size");
				return;		
			}

			if(tag == _NEIGHBOR_REPORT_IE_) {
				struct dot11k_neighbor_report *report;
				report = &pstat->wnm.neighbor_report[pstat->wnm.num_neighbor_report];
				parse_neighbor_report(priv, pos, len, report);
			}
				
			pos += len;
			frlen += len;
			pstat->wnm.num_neighbor_report++;
		 }
		
		 sort_candidate_list(priv, pstat);
#ifdef DOT11V_DEBUG
		 dump_cand_list(priv, pstat);
#endif
		 set_target_bssid(pstat);		 
		 valid_ms = valid_int * beacon_int * 128 / 125;
		 pstat->wnm.candidate_valid_time = (valid_ms)/1000; 
		 DOT11VTRACE("Candidate list valid for (%d) ms/(%d) sec\n", valid_ms, pstat->wnm.candidate_valid_time);				
	}
	
	if(pstat->wnm.reply) {		
		if(pstat->wnm.req_mode & _WNM_PREFERRED_CANDIDATE_LIST_)
			status = WNM_BSS_TM_ACCEPT;
		else {
			DOT11VDEBUG("BSS Transition Request did not include candidates \n");
			status = WNM_BSS_TM_REJECT_UNSPECIFIED;	
		}

		if(issue_BSS_Trans_Rsp(priv, pstat, pstat->wnm.dialog_token, status) == 0)
			start_bss_transition(priv, pstat);
	}
	return;
}

#endif

static void process_status_code( struct stat_info *pstat, unsigned char status_code)
{
	pstat->bssTransStatusCode = status_code;
	
	if(status_code)		
		pstat->bssTransRejectionCount++;

	switch(status_code) {
		case WNM_BSS_TM_ACCEPT:
			panic_printk("Accept: WNM_BSS_TM_ACCEPT![%02x][%02x][%02x][%02x][%02x][%02x] \n", 
						pstat->cmn_info.mac_addr[0], pstat->cmn_info.mac_addr[1], pstat->cmn_info.mac_addr[2], pstat->cmn_info.mac_addr[3], pstat->cmn_info.mac_addr[4], pstat->cmn_info.mac_addr[5]); 
			
			pstat->bssTransRejectionCount = 0;
			break;
		case WNM_BSS_TM_REJECT_UNSPECIFIED:
			DOT11VDEBUG("Reject: WNM_BSS_TM_REJECT_UNSPECIFIED!\n"); 
			break;
		case WNM_BSS_TM_REJECT_INSUFFICIENT_BEACON: 
			DOT11VDEBUG("Reject: WNM_BSS_TM_REJECT_INSUFFICIENT_BEACON!\n"); 
			break;
		case WNM_BSS_TM_REJECT_INSUFFICIENT_CAPABITY:
			DOT11VDEBUG("Reject: WNM_BSS_TM_REJECT_INSUFFICIENT_CAPABITY!\n"); 
			break;
		case WNM_BSS_TM_REJECT_UNDESIRED :
			DOT11VDEBUG("Reject: WNM_BSS_TM_REJECT_UNDESIRED !\n"); 
			break;
		case WNM_BSS_TM_REJECT_DELAY_REQUEST : 
			DOT11VDEBUG("Reject: WNM_BSS_TM_REJECT_DELAY_REQUEST !\n"); 
			break;
		case WNM_BSS_TM_REJECT_STA_CANDIDATE_LIST_PROVIDED :
			panic_printk("Reject: WNM_BSS_TM_REJECT_STA_CANDIDATE_LIST_PROVIDED !\n"); 
			break;
		case WNM_BSS_TM_REJECT_NO_SUITABLE_CANDIDATES: 
			DOT11VDEBUG("Reject: WNM_BSS_TM_REJECT_NO_SUITABLE_CANDIDATES  !\n"); 
			break;
		case WNM_BSS_TM_REJECT_LEAVING_ESS:
			DOT11VDEBUG("Reject: WNM_BSS_TM_REJECT_LEAVING_ESS  !\n"); 
			break;
		default:
			DOT11VDEBUG("unknown type !\n"); 
			break;
	}
}

void OnBSSTransRsp(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *pframe, int frame_len)
{
	if(frame_len < 3) {
		panic_printk("Ignore too short BSS Trans Management RSP!\n");
		return;
	}
	int frlen = 0;
	unsigned char dialog_token = pframe[2];
	unsigned char status_code = pframe[3];
	unsigned char bss_termination_delay = pframe[4];	//mins
	unsigned long flags;
#ifdef ROAMING_SUPPORT
	unsigned char tmpbuf[MACADDRLEN+1];
#endif
#if defined(DOT11K) && defined(STA_CONTROL_BAND_TRANSITION)
	struct BandTransNode *entry;
#endif
#ifdef RTK_MULTI_AP
    struct response_waiting_list *resp_entry = getEntryWaitingList(priv, pstat->cmn_info.mac_addr);
#endif
	frlen = 5;

	if((frame_len - frlen) > MAX_LIST_LEN)	
		return;
	
	DOT11VTRACE("dialog_token = %d, bss_termination_delay = %d\n", dialog_token, bss_termination_delay);
	process_status_code(pstat, status_code);
	pstat->bssTransExpiredTime = 0;	
#ifndef RTK_MULTI_AP
	pstat->bssTransTriggered = 0;
#endif
#if defined(DOT11K) && defined(STA_CONTROL_BAND_TRANSITION)
	if (priv->stactrl.stactrl_status && priv->pmib->staControl.stactrl_band_transit) {
		// Search a match in bandTransList
		entry = band_trans_get(priv, pstat->cmn_info.mac_addr);
		if (entry) {
			if (status_code != WNM_BSS_TM_ACCEPT) { // force disassociation
				spin_lock_irqsave(&priv->BandTransLock, flags);
				TriggerBandTrans(priv, entry);
				spin_unlock_irqrestore(&priv->BandTransLock, flags);
			} else {
				kfree(entry);
			}
			goto process_bss_trans_rsp_code;
		}
	}
#endif // defined(DOT11K) && defined(STA_CONTROL_BAND_TRANSITION)
#ifdef ROAMING_SUPPORT
	if(priv->pmib->roamingEntry.roaming_enable)
	{
		memcpy(tmpbuf, pframe+3, 1);
		memcpy(tmpbuf+1, pstat->cmn_info.mac_addr, MACADDRLEN);
		general_IndicateEvent(priv, DOT11_EVENT_ROAMING_BSS_TRANSMIT_RESP, tmpbuf);
	}
#endif

process_bss_trans_rsp_code:
	if((frame_len - frlen) > 0) {	
		if(status_code == WNM_BSS_TM_ACCEPT) {
			debug_out("Target BSSID: ", &pframe[frlen], MACADDRLEN);
			frlen += MACADDRLEN;
			debug_out("Bss Trans Candidate List: ", &pframe[frlen], frame_len - frlen);
#ifdef RTK_MULTI_AP
			SAVE_INT_AND_CLI(flags);
			SMP_LOCK_TRANSITION_LIST(flags);
			deleteEntryTransitionList(priv, pstat->cmn_info.mac_addr);
			SMP_UNLOCK_TRANSITION_LIST(flags);
			RESTORE_INT(flags);
#endif

		}else if(status_code == WNM_BSS_TM_REJECT_DELAY_REQUEST){
#ifdef RTK_MULTI_AP
			pstat->bssTransDisassocCountdown = bss_termination_delay*60;
#else
			priv->dot11vDiassocDeadline = bss_termination_delay * 60;
#endif
		}else
			debug_out("Bss Trans Candidate List: ", &pframe[frlen], frame_len - frlen);
	}else {
#ifdef RTK_MULTI_AP
		//DEBUG_ERR("WNM: no info in bss trans response!\n");
		DOT11VTRACE("Not include Target BSSID and BTM Candidate List Entries info\n");
		if(status_code == WNM_BSS_TM_ACCEPT) {
			SAVE_INT_AND_CLI(flags);
			SMP_LOCK_TRANSITION_LIST(flags);
			deleteEntryTransitionList(priv, pstat->cmn_info.mac_addr);
			SMP_UNLOCK_TRANSITION_LIST(flags);
			RESTORE_INT(flags);
		}
#endif
	}
#ifdef RTK_MULTI_AP
	if (resp_entry) {
		if (status_code == WNM_BSS_TM_ACCEPT) {
			bss_transition_response_notify(resp_entry->bssid, resp_entry->addr, resp_entry->target_bssid, status_code);
			resp_entry->status = BTM_ACCEPT;
		} else {
			resp_entry->status = BTM_FAILED;
		}
	}
#endif
	return;
}

void OnBSSTransQuery(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char*pframe, int frame_len)
{
	int list_len;
#ifdef RTK_MULTI_AP
	
	unsigned int flags;
	struct target_transition_list entry;
#endif
	if(frame_len < 2) {
		DEBUG_ERR("Ignore too short BSS Transition Management Query!\n"); 		
		return;
	}

	unsigned char dialog_token = pframe[2];
	unsigned char reason  = pframe[3]; 

	DOT11VTRACE("dialog_token = %d, reason = %d\n", dialog_token, reason); 
	
 	if(reason == _WNM_PREFERED_BSS_TRANS_LIST_INCLUDED_) {
		list_len = frame_len - 4;
		
		if(list_len > MAX_LIST_LEN)
			return;
		debug_out("Bss List Len: ", &pframe[list_len], frame_len - list_len);
	} else
		panic_printk("WNM_PREFERED BSS TRANS LIST NOT INCLUDED!\n"); 

#ifdef RTK_MULTI_AP
    if(priv->pmib->wnmEntry.Is11kDaemonOn){
		//keith: add into transition list
		pstat->dialog_token = dialog_token;
		//issue_BSS_Trans_Req(priv, pstat->hwaddr, dialog_token);
		memset(&entry, 0, sizeof(struct target_transition_list));
		memcpy(&entry.addr, pstat->cmn_info.mac_addr, MACADDRLEN);
		SAVE_INT_AND_CLI(flags);
		SMP_LOCK_TRANSITION_LIST(flags);

		addEntryTransitionList(priv, &entry);

		SMP_UNLOCK_TRANSITION_LIST(flags);
		RESTORE_INT(flags);
	}
	else
		panic_printk("CANNOT REPLY BSS TRANS QUERY - 11K DAEMON NOT ENABLED!\n");
#else
		issue_BSS_Trans_Req(priv, pstat->cmn_info.mac_addr, dialog_token, NULL, 0);
#endif
}

void WNM_ActionHandler(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *pframe, int frame_len)
{
	unsigned char action_field = pframe[1];
	
	switch (action_field) {
		case _WNM_TSMQUERY_ACTION_ID_: 
			OnBSSTransQuery(priv, pstat, pframe, frame_len);
			break;
		case _BSS_TSMRSP_ACTION_ID_:
			OnBSSTransRsp(priv, pstat, pframe, frame_len);
			break;
	#ifdef CONFIG_IEEE80211V_CLI
		case _BSS_TSMREQ_ACTION_ID_:
			OnBSSTransReq(priv, pstat, pframe, frame_len);
			break;
	#endif
		default:
			DEBUG_INFO("Other WNM action: %d:\n", action_field);
			break;
	}
}

unsigned char isDualBandClient(struct rtl8192cd_priv *priv, unsigned char* addr)
{
	int i;
	unsigned char *hwaddr = addr;
	struct sta_mac_rssi *EntryDB;
	if(!priv)
		return 0;
	EntryDB = priv->probe_sta;
	for(i=0; i<MAX_PROBE_REQ_STA; i++){
		if(EntryDB[i].used){
			if(!memcmp(EntryDB[i].addr, addr, MACADDRLEN)){
				//printk("Found probe request in other interface\n");
				return 1;
			}
		}
	}
	return 0;
}

#ifdef ROAMING_SUPPORT
unsigned char issue_ROAMING_BSS_Trans_Req(struct rtl8192cd_priv * priv, unsigned char * data)
{
	int ret = FAIL;
	if(priv->pmib->roamingEntry.roaming_enable)
	{
		unsigned char   MAC_STA[MACADDRLEN];
		unsigned char   Bssid[MACADDRLEN];
		unsigned char	channel;

		DEBUG_INFO("Roaming BSS transition request:\n");
		DEBUG_INFO("    STA MACaddr: %02X%02X%02X%02X%02X%02X\n", data[0],data[1],
						data[2],data[3],data[4],data[5]);
		DEBUG_INFO("    BSS MACaddr: %02X%02X%02X%02X%02X%02X\n", data[7],data[8],
						data[9],data[10],data[11],data[12]);
		memcpy(MAC_STA, data, MACADDRLEN);
		memcpy(&channel, data+6, 1);
		memcpy(Bssid, data+7, MACADDRLEN);
		struct stat_info *pstat = get_stainfo(priv, MAC_STA);
		if(pstat)
		{
			if((pstat->state & WIFI_ASOC_STATE) && (pstat->expire_to > 0) && (pstat->bssTransSupport)) //station supports 11v bss transition
			{
				ret = issue_BSS_Trans_Req(priv, MAC_STA, NULL, Bssid, channel);//1:success, 0:fail
			}
		}
			
	}
	if(ret == SUCCESS)
		return 0;
	else
		return 1;
}

#endif

