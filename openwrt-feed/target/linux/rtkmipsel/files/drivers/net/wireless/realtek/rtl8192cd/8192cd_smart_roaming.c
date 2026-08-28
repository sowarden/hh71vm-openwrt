
/*
 *  Software Smart Roaming routines
 *
 *  $Id: rtl_smart_roaming.c, v 1.4.4.2 2016/04/30 05:27:28 Tracy Exp $
 *
 *  Copyright (c) 2009 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _8192CD_SMART_ROAMING_C_

#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/module.h>
#include <linux/timer.h>
#include "8192cd.h"
#include "8192cd_util.h"
#include "8192cd_headers.h"

#define NETLINK_RTK 31
#define MAX_PAYLOAD	2048


struct timer_list		*timer_wlan0;
struct timer_list		*timer_wlan1;

#if defined(MBSSID)
struct timer_list		*send_timer_wlan0_vap[RTL8192CD_NUM_VWLAN];
struct timer_list		*send_timer_wlan1_vap[RTL8192CD_NUM_VWLAN];
#endif

struct sock *rtl_smart_roaming_nl = NULL;

int wlan0_used,wlan1_used,wlan0_deleted,wlan1_deleted,msg_dropcounter;
int wlan0_vap_delete[RTL8192CD_NUM_VWLAN];
int wlan1_vap_delete[RTL8192CD_NUM_VWLAN];
int wlan0_vap_used[RTL8192CD_NUM_VWLAN];
int wlan1_vap_used[RTL8192CD_NUM_VWLAN];

static int pid = 0;

#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
struct timer_list		*timer_wlan2;
int wlan2_used,wlan2_deleted;
#endif
#ifdef STA_ROAMING_CHECK    //for andlink
int pid2 = 0;
#endif

static void sr_timer_rec_add(void) {
    if (wlan0_used) {
        if (!wlan0_deleted) {
            del_timer_sync(timer_wlan0);
            wlan0_deleted = 1;
        }
        if (wlan0_deleted) {
            add_timer(timer_wlan0);
            wlan0_deleted = 0;
        }
    }

    if (wlan1_used) {
        if (!wlan1_deleted) {
            del_timer_sync(timer_wlan1);
            wlan1_deleted = 1;
        }
        if (wlan1_deleted) {
            add_timer(timer_wlan1);
            wlan1_deleted = 0;
        }
    }

#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
    if (wlan2_used) {
        if (!wlan2_deleted) {
            del_timer_sync(timer_wlan2);
            wlan2_deleted = 1;
        }

        if (wlan2_deleted) {
          add_timer(timer_wlan2);
          wlan0_deleted = 0;
        }
    }
#endif
#ifdef MBSSID
	int j;
	for (j = 0; j < RTL8192CD_NUM_VWLAN; j++) {
		if (wlan0_vap_used[j]) {
			if (!wlan0_vap_delete[j]) {
				del_timer_sync(send_timer_wlan0_vap[j]);
				wlan0_vap_delete[j] = 1;
			}
			if (wlan0_vap_delete[j]) {
				add_timer(send_timer_wlan0_vap[j]);
				wlan0_vap_delete[j] = 0;
			}
		}

		if(wlan1_vap_used[j]) {
			if (!wlan1_vap_delete[j]) {
				del_timer_sync(send_timer_wlan1_vap[j]);
				wlan1_vap_delete[j] = 1;
			}
			if (wlan1_vap_delete[j]) {
				add_timer(send_timer_wlan1_vap[j]);
				wlan1_vap_delete[j] = 0;
			}
		}
	}
#endif
}

static void sr_timer_rec_del(void){
    if(wlan0_used && (wlan0_deleted == 0)){
        del_timer_sync(timer_wlan0);
        // wlan0_used = 0;
        wlan0_deleted = 1;
    }
    if(wlan1_used && (wlan1_deleted == 0)){ 
        del_timer_sync(timer_wlan1);
        // wlan1_used = 0;
        wlan1_deleted = 1;
    }
#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
    if(wlan2_used && (wlan2_deleted == 0)){
        del_timer_sync(timer_wlan2);
        // wlan2_used = 0;
        wlan2_deleted = 1;
    }
#endif
#ifdef MBSSID
	int j;
	for (j = 0; j < RTL8192CD_NUM_VWLAN; j++) {
		if(wlan0_vap_used[j] && (wlan0_vap_delete[j] == 0)){
			del_timer_sync(send_timer_wlan0_vap[j]);
			//wlan0_vap_used[j] = 0;
			wlan0_vap_delete[j] = 1;
		}
		if(wlan1_vap_used[j] && (wlan1_vap_delete[j] == 0)){
			del_timer_sync(send_timer_wlan1_vap[j]);
			//wlan1_vap_used[j] = 0;
			wlan1_vap_delete[j] = 1;
		}
	}
#endif
}

//init timer when wlan interface up
void timer_ready(struct rtl8192cd_priv *priv)
{
	if ((!strcmp(priv->dev->name, "wlan0")) && (wlan0_used != 1)){
		init_timer(&priv->send_timer_wlan0);
		priv->send_timer_wlan0.expires = jiffies+RTL_SECONDS_TO_JIFFIES(5);
		priv->send_timer_wlan0.data = (unsigned long) priv;
		priv->send_timer_wlan0.function = send_roam_info;
        priv->wlanid = 0;
		priv->vapid = 0;
        timer_wlan0 = &(priv->send_timer_wlan0);
		wlan0_used = 1;
        wlan0_deleted = 1;
		panic_printk("[%s] wlan0 timer started!\n", __FUNCTION__);
	}
	else if ((!strcmp(priv->dev->name, "wlan1")) && (wlan1_used != 1)){
		init_timer(&priv->send_timer_wlan1);
		priv->send_timer_wlan1.expires = jiffies+RTL_SECONDS_TO_JIFFIES(4);
		priv->send_timer_wlan1.data = (unsigned long) priv;
		priv->send_timer_wlan1.function = send_roam_info;
        priv->wlanid = 1;
		priv->vapid = 0;
        timer_wlan1 = &(priv->send_timer_wlan1);
		wlan1_used = 1;
        wlan1_deleted = 1;
		panic_printk("[%s] wlan1 timer started!\n", __FUNCTION__);
	}
#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
    else if ((!strcmp(priv->dev->name, "wlan2")) && (wlan2_used != 1)){
        init_timer(&priv->send_timer_wlan2);
        priv->send_timer_wlan2.expires = jiffies+RTL_SECONDS_TO_JIFFIES(4);
    #if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
        priv->send_timer_wlan2.data = (unsigned long) &priv->send_timer_wlan2_event;
        priv->send_timer_wlan2.function = timer_event_timer_fn;
        INIT_TIMER_EVENT_ENTRY(&priv->send_timer_wlan2_event,send_roam_info, (unsigned long)priv);
    #else
        priv->send_timer_wlan2.data = (unsigned long) priv;
        priv->send_timer_wlan2.function = send_roam_info;
    #endif
        priv->wlanid = 2;
        priv->vapid = 0;
        timer_wlan2 = &(priv->send_timer_wlan2);
        wlan2_used = 1;
        wlan2_deleted = 1; 
    }
#endif

	if(pid != 0){
        sr_timer_rec_add();
    }
#ifdef STA_ROAMING_CHECK
    if(pid2 != 0){
        sr_timer_rec_add();
    }
#endif
}

#ifdef MBSSID
void vap_timer_ready(struct rtl8192cd_priv *priv, int i) {
	priv = GET_ROOT(priv); /* now priv is root */
	if (!strcmp(priv->dev->name, "wlan0")) {
		if (netif_running(GET_VAP_PRIV(priv, i)->dev) && (wlan0_vap_used[i] != 1)) {
			init_timer(&priv->send_timer_wlan0_vap[i]);
			priv->send_timer_wlan0_vap[i].expires = jiffies+RTL_SECONDS_TO_JIFFIES(5);
			priv->send_timer_wlan0_vap[i].data = (unsigned long) (GET_VAP_PRIV(priv, i));
			priv->send_timer_wlan0_vap[i].function = send_roam_info;
			GET_VAP_PRIV(priv, i)->wlanid = 0;
			GET_VAP_PRIV(priv, i)->vapid = i+1;
			send_timer_wlan0_vap[i] = &priv->send_timer_wlan0_vap[i];
			wlan0_vap_used[i] = 1;
			wlan0_vap_delete[i] = 1;
			panic_printk("[%s] wlan0-vap%d timer started!\n", __FUNCTION__, i);
		}
	}
	else if (!strcmp(priv->dev->name, "wlan1")) {
		if (netif_running(GET_VAP_PRIV(priv, i)->dev) && (wlan1_vap_used[i] != 1)) {
			init_timer(&priv->send_timer_wlan1_vap[i]);
			priv->send_timer_wlan1_vap[i].expires = jiffies+RTL_SECONDS_TO_JIFFIES(4);
			priv->send_timer_wlan1_vap[i].data = (unsigned long) (GET_VAP_PRIV(priv, i));
			priv->send_timer_wlan1_vap[i].function = send_roam_info;
			GET_VAP_PRIV(priv, i)->wlanid = 1;
			GET_VAP_PRIV(priv, i)->vapid = i+1;
			send_timer_wlan1_vap[i] = &priv->send_timer_wlan1_vap[i];
			wlan1_vap_used[i] = 1;
			wlan1_vap_delete[i] = 1;
			panic_printk("[%s] wlan1-vap%d timer started!\n", __FUNCTION__, i);
		}
	}
}
#endif

//delete timer when wlan interface down
void timer_del(struct rtl8192cd_priv *priv)
{
    if (!strcmp(priv->dev->name, "wlan0")) {
    	del_timer_sync(&priv->send_timer_wlan0);
    	wlan0_used = 0;
        wlan0_deleted = 1;
		panic_printk("[%s] %s timer deleted!\n", __FUNCTION__, priv->dev->name);
    }
	else if (!strcmp(priv->dev->name, "wlan1")) {
    	del_timer_sync(&priv->send_timer_wlan1);
        wlan1_used = 0;
        wlan1_deleted = 1;
		panic_printk("[%s] %s timer deleted!\n", __FUNCTION__, priv->dev->name);
    }
#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
    else if (!strcmp(priv->dev->name, "wlan2")) {
        del_timer_sync(&priv->send_timer_wlan2);
        wlan2_used = 0;
        wlan2_deleted = 1;
		panic_printk("[%s] %s timer deleted!\n", __FUNCTION__, priv->dev->name);
    }
#endif
#ifdef MBSSID
	else if (priv->dev->name[4] == '0' && IS_VAP_INTERFACE(priv)) {
		int i = priv->vap_id;
		del_timer_sync(&GET_ROOT(priv)->send_timer_wlan0_vap[i]);
		wlan0_vap_used[i] = 0;
		wlan0_vap_delete[i] = 1;
		panic_printk("[%s] %s timer deleted!\n", __FUNCTION__, priv->dev->name);
	}
	else if (priv->dev->name[4] == '1' && IS_VAP_INTERFACE(priv)) {
		int i = priv->vap_id;
		del_timer_sync(&GET_ROOT(priv)->send_timer_wlan1_vap[i]);
		wlan1_vap_used[i] = 0;
		wlan1_vap_delete[i] = 1;
		panic_printk("[%s] %s timer deleted!\n", __FUNCTION__, priv->dev->name);
	}
#endif
}

//netlink recv msg 
void rtl_netlink_rcv(struct sk_buff *skb) 
{
	struct nlmsghdr *nlh = NULL;
	unsigned char *message;
	
	if(skb == NULL) {				
		panic_printk(KERN_INFO "%s: skb is NULL\n", __FUNCTION__);		
		return ;	
	}

	nlh=(struct nlmsghdr*)skb->data;
	//printk(KERN_INFO "kernel received msg payload:%s\n",(unsigned char*)NLMSG_DATA(nlh));

	message = NLMSG_DATA(nlh);

	if (*message == 'S')
	{
		pid = nlh->nlmsg_pid; //pid of sending process
        sr_timer_rec_add();
    }

#ifdef STA_ROAMING_CHECK
    if (*message == 'A')
    {
        panic_printk("\nSR [%s][%d] received andlink pid %d!\n", __FUNCTION__, __LINE__, nlh->nlmsg_pid);
        pid2 = nlh->nlmsg_pid;
        sr_timer_rec_add();
    }
#endif
	
#ifdef STA_ROAMING_CHECK
    if (*message == 'F')
    {
        panic_printk("\nSR received delete from %d!\n", nlh->nlmsg_pid);
        if(nlh->nlmsg_pid == pid)
            pid = 0;
        else
            pid2 = 0;
        if(!pid && !pid2)
            sr_timer_rec_del();
    }
#else
    if (*message == 'F')
	{
	    panic_printk("\nSR received delete!\n");
		sr_timer_rec_del();
	}
#endif	
	
}

//netlink send msg 
void rtl_netlink_sendmsg(int pid, struct sock *nl_sk,char *data, int data_len)
{
	struct nlmsghdr *nlh;
	struct sk_buff *skb;
	unsigned int skblen;
	unsigned char *datab;
	const char *fn;
	int err;
	
	if(pid == 0 
#ifdef STA_ROAMING_CHECK
		&& pid2 == 0
#endif
		)
		return;
		
	if(data_len > MAX_PAYLOAD)
	{
		err = -ENOBUFS;		
		fn = "data_len";		
		goto msg_fail;
	}
	
	skblen = NLMSG_SPACE(data_len + 4); //+len
	skb = alloc_skb(skblen, GFP_ATOMIC);
	
	if(!skb)
	{
		err = -ENOBUFS;		
		fn = "alloc_skb";		
		goto msg_fail;
	}
	
	nlh = nlmsg_put(skb,0,0,0,skblen-sizeof(*nlh),0);
	
	if(!nlh)
	{
		err = -ENOBUFS;		
		fn = "nlmsg_put";		
		goto msg_fail_skb;
	}
	
#if defined(__LINUX_3_10__)
	NETLINK_CB(skb).portid = 0; //from kernel 
#else
	NETLINK_CB(skb).pid = 0; //from kernel 
#endif
	NETLINK_CB(skb).dst_group = 0; //unicast

	datab = NLMSG_DATA(nlh);
	memset(datab, 0, data_len+4); 
	memcpy(datab,&data_len,4); //+ total len
	memcpy(datab+4,data, data_len);

	nlh->nlmsg_len = NLMSG_HDRLEN + data_len + 4;

	//printk("check data_len before send=%d\n",data_len);
	err= netlink_unicast(nl_sk, skb, pid, MSG_DONTWAIT);

	if (err < 0)
	{
		fn = "nlmsg_unicast";				
		goto msg_fail;	 //nlmsg_unicast already kfree_skb 
	}
	
	return;
	
msg_fail_skb:	
	kfree_skb(skb);
	
msg_fail:
	if(msg_dropcounter < 3){
		msg_dropcounter++;
		panic_printk("[%s] drop msg: pid=%d msglen=%d %s: err=%d\n",__FUNCTION__, pid, data_len, fn, err);
	}
	return;
}

#ifdef STA_CONTROL
void notify_hidden_ap(struct rtl8192cd_priv *priv, int type)
{
	if(!priv->stactrl.stactrl_enable_hiddenAP || priv->stactrl.stactrl_prefer)
		return;
	
	int offset = 0;
	unsigned char send_buf[10] = {0};
	
	if (!strcmp(priv->dev->name, "wlan0"))
		priv->wlanid = 0;
	else if (!strcmp(priv->dev->name, "wlan1"))
		priv->wlanid = 1;

	send_buf[offset] = type;
	offset+=sizeof(unsigned char);

	memcpy(send_buf+offset,&(priv->wlanid),sizeof(unsigned char));
	offset+=sizeof(unsigned char);

    memcpy(send_buf+offset,&(priv->vap_id),sizeof(unsigned char));
    offset+=sizeof(unsigned char);

	rtl_netlink_sendmsg(pid,rtl_smart_roaming_nl,send_buf,offset);
}
#endif

void send_roam_info(unsigned long task_priv)
{
	unsigned long flags;
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;

	if(!SMART_ROAMING_ENABLE 
#ifdef STA_ROAMING_CHECK
			&& !RTL_LINK_ROAMING_ENABLE
#endif
		)
		return;

#ifdef CONFIG_IEEE80211V
	struct list_head *phead, *plist;
	struct stat_info *pstat;
	
	phead = &priv->asoc_list;
	if (!list_empty(phead))
	{
		plist = phead->next;
	
		while (plist != phead)
		{
			pstat = list_entry(plist, struct stat_info, asoc_list);
			if(isDualBandClient(priv->bss11v_priv_sc, pstat->cmn_info.mac_addr))
				pstat->dual_band_capable = 1;
			plist = plist->next;
		}
	}
#endif

	int vapsr_enable = 0;
#ifdef MBSSID
	int i;
	if (GET_ROOT(priv)->pmib->miscEntry.vap_enable != 0) {
		if (IS_VAP_INTERFACE(priv)) {
			vapsr_enable = 1;
			i = priv->vap_id;
			priv->wlanid = priv->dev->name[4] - '0';
			priv->vapid = (unsigned char)(i+1);
		}
	}
#endif

	SMP_LOCK(flags);
	construct_netlink_send(priv);
	construct_netlink_send_extend(priv);

	struct rtl8192cd_priv *root_priv = GET_ROOT(priv);
	if(strcmp(priv->dev->name, "wlan0") == 0 || strcmp(priv->dev->name, "wlan1") == 0 
#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
		|| strcmp(priv->dev->name, "wlan2") == 0
#endif
	) {
#ifdef MBSSID
		root_priv->neighbor_clear_flag = root_priv->vap_count;
#else
		root_priv->neighbor_clear_flag = 0;
#endif
	}
	else {
		root_priv->neighbor_clear_flag--;
	}
	if(root_priv->neighbor_clear_flag == 0) {
		clear_send_info(root_priv);	//clear after send
	}
	SMP_UNLOCK(flags);

	// modify timer for sending periodically
	if(vapsr_enable != 1) {
		if ((!strcmp(priv->dev->name, "wlan0")) && (!timer_pending(&priv->send_timer_wlan0))) {
			mod_timer(&priv->send_timer_wlan0, jiffies+RTL_SECONDS_TO_JIFFIES(5));
			return;
		}
		if ((!strcmp(priv->dev->name, "wlan1")) && (!timer_pending(&priv->send_timer_wlan1))) {
			mod_timer(&priv->send_timer_wlan1, jiffies+RTL_SECONDS_TO_JIFFIES(4));
			return;
		}
#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
	    if ((!strcmp(priv->dev->name, "wlan2")) && (!timer_pending(&priv->send_timer_wlan2))) {
	        mod_timer(&priv->send_timer_wlan2, jiffies+RTL_SECONDS_TO_JIFFIES(4));
			return;
		}
#endif
	}
#ifdef MBSSID
	if(vapsr_enable == 1) {
		if ((priv->dev->name[4] == '0') && (!timer_pending(&priv->send_timer_wlan0_vap[i]))) {
			mod_timer(&root_priv->send_timer_wlan0_vap[i], jiffies+RTL_SECONDS_TO_JIFFIES(5));
		}
		if ((priv->dev->name[4] == '1') && (!timer_pending(&priv->send_timer_wlan1_vap[i]))) {
			mod_timer(&root_priv->send_timer_wlan1_vap[i], jiffies+RTL_SECONDS_TO_JIFFIES(4));
		}
#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
	    if ((priv->dev->name[4] == '2') && (!timer_pending(&priv->send_timer_wlan2_vap[i]))) {
			mod_timer(&root_priv->send_timer_wlan2_vap[i], jiffies+RTL_SECONDS_TO_JIFFIES(4));
		}
#endif
	}
#endif
}

void notify_new_sta(struct rtl8192cd_priv *priv, unsigned char *mac, int type, unsigned char rssi)
{
	int offset =0;
	
	unsigned char send_buf[16]={0};
	unsigned char channel_util;
	
	if (!strcmp(priv->dev->name, "wlan0"))
		priv->wlanid = 0;
	else if (!strcmp(priv->dev->name, "wlan1"))
		priv->wlanid = 1;
#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
	else if (!strcmp(priv->dev->name, "wlan2"))
		priv->wlanid = 2;
#endif

	send_buf[offset] = type;
	offset+=sizeof(unsigned char);

	memcpy(send_buf+offset,&(priv->wlanid),sizeof(unsigned char));
	offset+=sizeof(unsigned char);

#ifndef STA_ROAMING_CHECK
	memcpy(send_buf+offset,&(priv->vapid),sizeof(unsigned char));
	offset+=sizeof(unsigned char);
#endif

    memcpy(send_buf+offset,&(priv->pmib->dot11RFEntry.dot11channel),sizeof(unsigned char));
    offset+=sizeof(unsigned char);

	channel_util = 255 - priv->ext_stats.ch_utilization;
	memcpy(send_buf+offset,&(channel_util),sizeof(channel_util));
	offset+=sizeof(channel_util);	

	memcpy(send_buf+offset, mac, MACADDRLEN);
	offset += MACADDRLEN;

	memcpy(send_buf+offset,&(rssi),sizeof(rssi));
	offset+=sizeof(rssi);

	if(pid)
		rtl_netlink_sendmsg(pid,rtl_smart_roaming_nl,send_buf,offset);
#ifdef STA_ROAMING_CHECK
	if(priv->pmib->miscEntry.telco_selected == TELCO_CMCC){
		if(pid2 && priv->pmib->rlr_profile.roaming_switch && priv->pmib->rlr_profile.roaming_qos)
	    	rtl_netlink_sendmsg(pid2,rtl_smart_roaming_nl,send_buf,offset);
	}else if(priv->pmib->miscEntry.telco_selected == TELCO_CT){
		extern void rtk_smart_roaming_netlink_send_multicast(int pid, struct sock *nl_sk, char *data, int data_len);
		if(priv->pmib->rlr_profile.roaming_switch && priv->pmib->rlr_profile.roaming_qos)
			rtk_smart_roaming_netlink_send_multicast(0,rtl_smart_roaming_nl,send_buf,offset);
	}

#endif
}

#ifdef STA_ROAMING_CHECK
void notify_connect_fail(struct rtl8192cd_priv *priv, unsigned char *mac, int type)
{
    int offset =0;
	
	unsigned char send_buf[10]={0};
	unsigned char channel_util;
	
	if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G)
		priv->wlanid = 0;
	else if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_2G)
		priv->wlanid = 1;

	send_buf[offset] = type;
	offset+=sizeof(unsigned char);

	memcpy(send_buf+offset,&(priv->wlanid),sizeof(unsigned char));
	offset+=sizeof(unsigned char);	

	memcpy(send_buf+offset, mac, MACADDRLEN);
	offset += MACADDRLEN;

	if(pid2 && priv->pmib->rlr_profile.roaming_switch && priv->pmib->rlr_profile.roaming_qos)
	    rtl_netlink_sendmsg(pid2,rtl_smart_roaming_nl,send_buf,offset);
}
#endif

//Get neighbor channel unicast Null data rssi 
void add_neighbor_unicast_sta(struct rtl8192cd_priv *priv,unsigned char* addr, unsigned char rssi)
{
	int i, idx=-1, idx2 =0;
	unsigned char rssi_input;	
	for (i = 0; i < MAX_NEIGHBOR_STA; i++) {
		if (priv->neigbor_sta[i].used == 0) {
			if (idx < 0)
				idx = i; //search for empty entry
			continue;
		}
		if (!memcmp(priv->neigbor_sta[i].addr, addr, MACADDRLEN)) {
			idx2 = i;
			break;      // check if it is already in the list
		}
	}
	if (idx >= 0){
		rssi_input = rssi;
		memcpy(priv->neigbor_sta[idx].addr, addr, MACADDRLEN);	
		priv->neigbor_sta[idx].used = 1;		
		priv->neigbor_sta[idx].Entry = idx;   //check which entry is the neighbor sta recorded
		priv->neigbor_sta[idx].rssi = rssi_input;
		priv->NeighborStaEntryOccupied++;
		
	}
	else if (idx2){
		rssi_input = ((priv->neigbor_sta[idx2].rssi * 7)+(rssi * 3)) / 10;			
		priv->neigbor_sta[idx2].rssi = rssi_input;
		
		return;
	}
	else if (priv->NeighborStaEntryOccupied == MAX_NEIGHBOR_STA) {// sta list full, need to replace sta
			idx = priv->NeighborStaEntryNum;	
			for (i = 0; i < MAX_NEIGHBOR_STA; i++) {
				if (!memcmp(priv->neigbor_sta[i].addr, addr, MACADDRLEN))					
					return;		// check if it is already in the list			
			}
			memcpy(priv->neigbor_sta[idx].addr, addr, MACADDRLEN);		
			priv->neigbor_sta[idx].used = 1;
			priv->neigbor_sta[idx].Entry = idx;		
			priv->neigbor_sta[idx].rssi = rssi;
			priv->NeighborStaEntryNum++;			
			if( priv->NeighborStaEntryNum == MAX_NEIGHBOR_STA)	
				priv->NeighborStaEntryNum = 0; // Reset entry counter;
			return;
	}
}

//construct associated sta info 
unsigned char construct_assoc_sta(struct rtl8192cd_priv *priv,unsigned char *send_buf, int * offset)
{
	struct stat_info	*pstat;
	struct list_head	*phead, *plist;
	unsigned char	data_rate;
	unsigned char	assoc_num = 0;
	unsigned int	link_time = 0;
	unsigned int	tx_throughput = 0;
	unsigned int	rx_throughput = 0;
	unsigned char	dot11kv_flag = 0;
	unsigned char   dual_band = 0;
	unsigned char	btm_fail_count = 0;
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	
	phead = &priv->asoc_list;
	
	if (list_empty(phead)) {
		return 0;
	}
	
	SMP_LOCK_ASOC_LIST(flags);
	plist = phead->next;

	//construct associated sta info
	while (plist != phead) {
		
		pstat = list_entry(plist, struct stat_info, asoc_list);
		plist = plist->next;		

#ifdef CONFIG_RTK_MESH
		if(isMeshPoint(pstat))
			continue;
#endif
#ifdef WDS
		if(pstat->state & WIFI_WDS)
			continue;
#endif
		//printk("pstat->IOTPeer=%d\n",pstat->IOTPeer);
		if (pstat && pstat->is_realtek_sta) //pstat->IOTPeer == HT_IOT_PEER_RTK_APCLIENT)
		    continue;

		if(pstat->expire_to==0) // exclude expired STA
			continue;	

		memcpy(send_buf+*offset,&(pstat->cmn_info.mac_addr), MACADDRLEN);
		*offset += MACADDRLEN;
		
		memcpy(send_buf+*offset,&(pstat->rssi),sizeof(unsigned char));
		*offset+=sizeof(unsigned char);

		//data rate only when tx pkts increase within 5s
		if (pstat->tx_pkts > pstat->tx_pkts_pre)
			data_rate = (pstat->current_tx_rate >= VHT_RATE_ID) ? ((pstat->current_tx_rate - VHT_RATE_ID)%10):((pstat->current_tx_rate >= HT_RATE_ID)? (pstat->current_tx_rate - HT_RATE_ID) : pstat->current_tx_rate/2);
		else
			data_rate = 255; //will not trigger data_rate threshold

		memcpy(send_buf+*offset,&data_rate,sizeof(unsigned char));
		*offset+=sizeof(unsigned char);
		
		//int link_time: host byte order convert to network byte order
		link_time=htonl(pstat->link_time);			 	
		memcpy(send_buf+*offset,&(link_time),sizeof(link_time));
		*offset+=sizeof(link_time);

		//int tx_tp: host byte order convert to network byte order
		tx_throughput = htonl(pstat->tx_avarage);
		memcpy(send_buf+*offset, &(tx_throughput), sizeof(tx_throughput));
		*offset += sizeof(tx_throughput);

		//int rx_tp: host byte order convert to network byte order
		rx_throughput = htonl(pstat->rx_avarage);
		memcpy(send_buf+*offset, &(rx_throughput),sizeof(rx_throughput));
		*offset += sizeof(rx_throughput);

		//unsigned char dualBand_flag: indicate client support for dual band
#ifdef CONFIG_IEEE80211V
		dual_band = pstat->dual_band_capable;
#endif
		memcpy(send_buf+*offset, &dual_band, sizeof(unsigned char));
		*offset += sizeof(unsigned char);

		//unsigned char dotkv_flag: indicate client support for 11k/11v
#ifdef DOT11K
		if(pstat->rm.rm_cap[0] & 0x10)
			dot11kv_flag |= 1; //support 11k
#endif
#ifdef CONFIG_IEEE80211V
		if(pstat->bssTransSupport){
			dot11kv_flag |= 2; //support 11v
			//btm_fail_count = pstat->bssTransRejectionCount;
			btm_fail_count = pstat->bssTransPktSent;
		}
#endif
		memcpy(send_buf+*offset, &dot11kv_flag, sizeof(unsigned char));
		*offset += sizeof(unsigned char);

		memcpy(send_buf+*offset, &btm_fail_count, sizeof(unsigned char));
		*offset += sizeof(unsigned char);
		
        assoc_num++;
	}
	SMP_UNLOCK_ASOC_LIST(flags);	
	return assoc_num;
}

//construct 11k neighbor report info
int construct_neighbor_report(struct rtl8192cd_priv *priv,unsigned char *send_buf,int * offset)
{	
	struct stat_info	*pstat;
	struct list_head	*phead, *plist;
	int i;
	
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	
	phead = &priv->asoc_list;
	
	if (list_empty(phead)) {
		return 0;
	}
	
	SMP_LOCK_ASOC_LIST(flags);
	plist = phead->next;
	
	while (plist != phead) {
			
		pstat = list_entry(plist, struct stat_info, asoc_list);
		plist = plist->next;	
		
#ifdef CONFIG_RTK_MESH
		if(isMeshPoint(pstat))
			continue;
#endif
#ifdef WDS
		if(pstat->state & WIFI_WDS)
			continue;
#endif
		if (pstat && pstat->is_realtek_sta) //pstat->IOTPeer == HT_IOT_PEER_RTK_APCLIENT)
		    continue;

		if(pstat->expire_to==0) // exclude expired STA
			continue;		
#if defined(DOT11K) && defined(CONFIG_IEEE80211V)
		if (pstat->rcvNeighborReport && pstat->rm.neighbor_ap_num)
		{
			//check assoc sta support 11k or 11v
			priv->sta_flag = 0;
			if (pstat->rm.rm_cap[0] & 0x10){
#if defined(CONFIG_IEEE80211V)
				if(pstat->bssTransSupport)
					priv->sta_flag |= 3; //set bit 1 and bit 0
				else
#endif
					priv->sta_flag |= 1; //set bit 0
			}
							
			memcpy(send_buf+*offset,&(priv->sta_flag),sizeof(unsigned char));
			*offset+=sizeof(unsigned char);					
			
			memcpy(send_buf+*offset,&(pstat->cmn_info.mac_addr),MACADDRLEN);
			*offset+=MACADDRLEN;		

			memcpy(send_buf+*offset,&(pstat->rm.neighbor_ap_num),sizeof(unsigned char));
			*offset+=sizeof(unsigned char);
			
			for (i = 0 ; i < pstat->rm.neighbor_ap_num; i++)
			{		
				memcpy(send_buf+*offset,&(pstat->rm.beacon_report[i].info.bssid),6);
				*offset+=MACADDRLEN;				
				
				memcpy(send_buf+*offset,&(priv->rm_neighbor_report[i].subelemnt.preference),1);
				*offset+=sizeof(unsigned char);	
				
				memcpy(send_buf+*offset,&(pstat->rm.beacon_report[i].info.RCPI),1);
				*offset+=sizeof(unsigned char);	
			}
			pstat->rm.neighbor_ap_num = 0; //reset to prevent duplicate sending
		}
#endif
	}
	SMP_UNLOCK_ASOC_LIST(flags);
	
	return 1;
}

//clear all info after send 
void clear_send_info(struct rtl8192cd_priv *priv)
{
	priv->NeighborStaEntryOccupied = 0;	
	memset(&priv->neigbor_sta, 0, sizeof(priv->neigbor_sta));
}

#ifdef STA_ROAMING_CHECK
static int construct_roaming_sta(struct rtl8192cd_priv *priv,unsigned char *send_buf, int * offset)
{
    struct stat_info    *pstat;
    struct list_head    *phead, *plist;
    unsigned int    retry_ratio;
    unsigned int    fail_ratio;
    unsigned char   roaming_num = 0;
#ifdef SMP_SYNC
    unsigned long flags = 0;
#endif
	int tx_pkts;

    
    phead = &priv->asoc_list;
    
    if (list_empty(phead)) {
        return 0;
    }
    
    SMP_LOCK_ASOC_LIST(flags);
    plist = phead->next;

    //construct associated sta info
    while (plist != phead) {
        
        pstat = list_entry(plist, struct stat_info, asoc_list);
        plist = plist->next;        

#ifdef CONFIG_RTK_MESH
        if( isMeshPoint(pstat))
            continue;
#endif
#ifdef WDS
        if(pstat->state & WIFI_WDS)
            continue;
#endif
//printk("pstat->IOTPeer=%d\n",pstat->IOTPeer);
        if (pstat && pstat->is_realtek_sta) //pstat->IOTPeer == HT_IOT_PEER_RTK_APCLIENT)
            continue;

        if(pstat->expire_to==0) // exclude expired STA
            continue;

		if(priv->pmib->miscEntry.telco_selected == TELCO_CMCC){
			tx_pkts = pstat->tx_pkts-pstat->prev_tx_pkts2;
			int retry_pkts = pstat->total_tx_retry_cnt-pstat->prev_tx_retry_pkts2;
			int fail_pkts = pstat->tx_fail-pstat->prev_tx_fail_pkts2;
			if(retry_pkts>0)
				retry_ratio = (retry_pkts*100)/(tx_pkts+retry_pkts);
			else
				retry_ratio = 0;
			if(fail_pkts>0)
				fail_ratio = fail_pkts*100/tx_pkts;
			else
				fail_ratio = 0;
			pstat->prev_tx_pkts2 = pstat->tx_pkts;
			pstat->prev_tx_retry_pkts2 = pstat->total_tx_retry_cnt;
			pstat->prev_tx_fail_pkts2 = pstat->tx_fail;
			if(pstat->rssi >= priv->pmib->rlr_profile.RSSIThreshold &&
					retry_ratio <= priv->pmib->rlr_profile.retry_ratio &&
					fail_ratio <= priv->pmib->rlr_profile.fail_ratio)
			   continue; 
		}
		else if(priv->pmib->miscEntry.telco_selected == TELCO_CT){
			if(pstat->rssi > priv->pmib->rlr_profile.RSSIThreshold)
				continue;
		}

        memcpy(send_buf+*offset,&(pstat->cmn_info.mac_addr), MACADDRLEN);
        *offset += MACADDRLEN;
        
        memcpy(send_buf+*offset,&(pstat->rssi),sizeof(unsigned char));
        *offset+=sizeof(unsigned char);
		
		if(priv->pmib->miscEntry.telco_selected == TELCO_CMCC){
			memcpy(send_buf+*offset, &(priv->pmib->dot11RFEntry.dot11channel), sizeof(unsigned char));
			*offset+=sizeof(unsigned char);
			
			memcpy(send_buf+*offset, &(priv->pmib->dot11nConfigEntry.dot11nUse40M), sizeof(unsigned int));
			*offset+=sizeof(unsigned int);
			
			memcpy(send_buf+*offset,&(retry_ratio),sizeof(unsigned int));
			*offset+=sizeof(unsigned int);
			
			memcpy(send_buf+*offset,&(fail_ratio),sizeof(unsigned int));
			*offset+=sizeof(unsigned int);
			
			memcpy(send_buf+*offset,&(tx_pkts),sizeof(unsigned int));
			*offset+=sizeof(unsigned int);
		}
		else if(priv->pmib->miscEntry.telco_selected == TELCO_CT){
			memcpy(send_buf+*offset,&(pstat->link_time),sizeof(unsigned long));
			*offset+=sizeof(unsigned long);
		}
      
        roaming_num++;
    }
    SMP_UNLOCK_ASOC_LIST(flags);    
    return roaming_num;
}

void construct_netlink_send_rtl_link_neighbor_info(struct rtl8192cd_priv *priv)
{
    int offset=0, i;
	unsigned char send_buf[2048]={0};
	unsigned int neighbor_sta_num, sta_num=0;

    send_buf[offset] = NEIGHBOR_INFO;
	offset+=sizeof(unsigned char);
	if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G)
		priv->wlanid = 0;
	else if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_2G)
		priv->wlanid = 1;
    memcpy(send_buf+offset,&(priv->wlanid),sizeof(unsigned char));
	offset+=sizeof(unsigned char);

	neighbor_sta_num = priv->NeighborStaEntryOccupied;
    for (i = 0; i < neighbor_sta_num; i++)
	{
        if(priv->neigbor_sta[i].rssi >= priv->pmib->rlr_profile.RSSIThreshold){
    		sta_num++;
        }
	}
	memcpy(send_buf+offset,&sta_num,sizeof(sta_num));
	offset+=sizeof(sta_num);

	for (i = 0; i < neighbor_sta_num; i++)
	{
        if(priv->neigbor_sta[i].rssi >= priv->pmib->rlr_profile.RSSIThreshold){
    		memcpy(send_buf+offset,&(priv->neigbor_sta[i].addr) ,MACADDRLEN);
    		offset += MACADDRLEN;	

            send_buf[offset] = priv->neigbor_sta[i].rssi;
            offset++;

            memcpy(send_buf+offset, &(priv->pmib->dot11RFEntry.dot11channel), sizeof(unsigned char));
            offset+=sizeof(unsigned char);

            memcpy(send_buf+offset, &(priv->pmib->dot11nConfigEntry.dot11nUse40M), sizeof(unsigned int));
            offset+=sizeof(unsigned int);
        }
	}

	rtl_netlink_sendmsg(pid2, rtl_smart_roaming_nl, send_buf, offset);
}

void construct_netlink_send_rtl_link_roaming_info(struct rtl8192cd_priv *priv)
{
    int offset=0, roaming_offset, i;
	unsigned char send_buf[2048]={0};
	unsigned int roaming_sta_num, sta_num=0;

    send_buf[offset] = ROAMING_INFO;
	offset+=sizeof(unsigned char);
	if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G)
		priv->wlanid = 0;
	else if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_2G)
		priv->wlanid = 1;
    memcpy(send_buf+offset,&(priv->wlanid),sizeof(unsigned char));
	offset+=sizeof(unsigned char);

	roaming_offset = offset;
	offset += sizeof(roaming_sta_num);
	roaming_sta_num = construct_roaming_sta(priv,send_buf,&offset);

	memcpy(send_buf+roaming_offset,&roaming_sta_num,sizeof(unsigned int));

	if(priv->pmib->miscEntry.telco_selected == TELCO_CMCC){
		rtl_netlink_sendmsg(pid2, rtl_smart_roaming_nl, send_buf, offset);
	}else if(priv->pmib->miscEntry.telco_selected == TELCO_CT){
		extern void rtk_smart_roaming_netlink_send_multicast(int pid, struct sock *nl_sk, char *data, int data_len);
		rtk_smart_roaming_netlink_send_multicast(0,rtl_smart_roaming_nl,send_buf,offset);
	}

	
}
#endif
//construct all wlan info and send 
void construct_netlink_send(struct rtl8192cd_priv *priv)
{
	int offset=0, assoc_offset, i, ap_stanum;
	unsigned int neighbor_sta_num, assoc_sta_num, sta_num;
	unsigned char send_buf[2048]={0};
	unsigned char channel_util;
	unsigned char ap_rate;
		
	send_buf[offset] = WLAN_STA_INFO;
	offset+=sizeof(unsigned char);
	if (!strcmp(priv->dev->name, "wlan0"))
		priv->wlanid = 0;
	else if (!strcmp(priv->dev->name, "wlan1"))
		priv->wlanid = 1;
#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
	else if (!strcmp(priv->dev->name, "wlan2"))
		priv->wlanid = 2;
#endif
	memcpy(send_buf+offset,&(priv->wlanid),sizeof(unsigned char));
	offset+=sizeof(unsigned char);

	memcpy(send_buf+offset,&(priv->vapid),sizeof(unsigned char));
	offset+=sizeof(unsigned char);

	memcpy(send_buf+offset,&(priv->auto_channel_backup),sizeof(unsigned char));
    offset+=sizeof(unsigned char);

	memcpy(send_buf+offset,&(priv->pmib->dot11RFEntry.dot11channel),sizeof(unsigned char));
	offset+=sizeof(unsigned char);

	channel_util = 255 - priv->ext_stats.ch_utilization;
	memcpy(send_buf+offset,&(channel_util),sizeof(channel_util));
	offset+=sizeof(channel_util);

	ap_rate = priv->pshare->total_tp * 100 / priv_max_tp(priv);
	memcpy(send_buf+offset,&(ap_rate),sizeof(ap_rate));
	offset+=sizeof(ap_rate);

	ap_stanum = htonl(priv->pshare->total_sta_num);
	memcpy(send_buf+offset,&(ap_stanum),sizeof(ap_stanum));
	offset+=sizeof(ap_stanum);

	neighbor_sta_num = GET_ROOT(priv)->NeighborStaEntryOccupied;
	sta_num = htonl(neighbor_sta_num);
	memcpy(send_buf+offset,&sta_num,sizeof(neighbor_sta_num));
	offset+=sizeof(sta_num);

	//construct neighbor unicast table info
	for (i = 0; i < neighbor_sta_num; i++)
	{
		memcpy(send_buf+offset,&(GET_ROOT(priv)->neigbor_sta[i].addr) ,MACADDRLEN);
		offset += MACADDRLEN;

        send_buf[offset] = GET_ROOT(priv)->neigbor_sta[i].rssi;
        offset++;
	}
	
	assoc_offset = offset;
	offset += sizeof(assoc_sta_num);
	assoc_sta_num = construct_assoc_sta(priv,send_buf,&offset);
	assoc_sta_num = htonl(assoc_sta_num);
	memcpy(send_buf+assoc_offset,&assoc_sta_num,sizeof(unsigned int));

	construct_neighbor_report(priv,send_buf,&offset);

	//send all wlan info via netlink
	if(pid)
		rtl_netlink_sendmsg(pid,rtl_smart_roaming_nl,send_buf,offset);
#ifdef STA_ROAMING_CHECK
	if(priv->pmib->miscEntry.telco_selected == TELCO_CMCC){
		if(pid2 && priv->pmib->rlr_profile.roaming_switch && priv->pmib->rlr_profile.roaming_qos){
			construct_netlink_send_rtl_link_neighbor_info(priv);
			construct_netlink_send_rtl_link_roaming_info(priv);
		}
	}else if(priv->pmib->miscEntry.telco_selected == TELCO_CT){
		 if(priv->pmib->rlr_profile.roaming_switch && priv->pmib->rlr_profile.roaming_qos)
			construct_netlink_send_rtl_link_roaming_info(priv);
	}
#endif
}

//construct root extended info and send
void construct_netlink_send_extend(struct rtl8192cd_priv *priv)
{
	int offset = 0;
	
	unsigned char send_buf[10]={0};

	if(!IS_ROOT_INTERFACE(priv))
		return;
	
	if (!strcmp(priv->dev->name, "wlan0"))
		priv->wlanid = 0;
	else if (!strcmp(priv->dev->name, "wlan1"))
		priv->wlanid = 1;
#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
	else if (!strcmp(priv->dev->name, "wlan2"))
		priv->wlanid = 2;
#endif

	send_buf[offset] = WLAN_EXTEND_INFO;
	offset+=sizeof(unsigned char);

	send_buf[offset] = SR_WLANID;
	offset+=sizeof(unsigned char);
	memcpy(send_buf+offset,&(priv->wlanid),sizeof(unsigned char));
	offset+=sizeof(unsigned char);

	send_buf[offset] = SR_VAPID;
	offset+=sizeof(unsigned char);
	memcpy(send_buf+offset,&(priv->vapid),sizeof(unsigned char));
	offset+=sizeof(unsigned char);

	send_buf[offset] = SR_CSA_REQUEST;
	offset+=sizeof(unsigned char);
	memcpy(send_buf+offset,&(priv->csa_request),sizeof(unsigned char));
	offset+=sizeof(unsigned char);

	if(pid)
		rtl_netlink_sendmsg(pid,rtl_smart_roaming_nl,send_buf,offset);
}

int rtl_netlink_init(void) 
{

#if defined(__LINUX_3_10__)
	struct netlink_kernel_cfg cfg = {
		.input = rtl_netlink_rcv,
	};

	rtl_smart_roaming_nl = netlink_kernel_create(&init_net, NETLINK_RTK, &cfg);
#else	
	rtl_smart_roaming_nl = netlink_kernel_create(&init_net, NETLINK_RTK, 0, rtl_netlink_rcv, NULL, THIS_MODULE);
#endif
		
	if(!rtl_smart_roaming_nl)
	{
		panic_printk(KERN_ERR "rtl_smart_roaming_nl: Cannot create netlink socket");
		return -ENOMEM;
	}
	
	return 0;
}

void rtl_netlink_exit(void) 
{
	netlink_kernel_release(rtl_smart_roaming_nl);
    pid = 0;	
}

__inline__ static int smart_roaming_block_mac_hash(unsigned char *networkAddr, int hash_size)
{
    unsigned long x;

    x = networkAddr[0] ^ networkAddr[1] ^ networkAddr[2] ^ networkAddr[3] ^ networkAddr[4] ^ networkAddr[5];

    return x & (hash_size - 1);
}
__inline__ static void smart_roaming_block_mac_hash_link(struct smart_roaming_block_link_list *link, struct smart_roaming_block_link_list **head)
{
    link->next_hash = *head;

    if (link->next_hash != NULL)
        link->next_hash->pprev_hash = &link->next_hash;
    *head = link;
    link->pprev_hash = head;
}
__inline__ static void smart_roaming_block_mac_hash_unlink(struct smart_roaming_block_link_list *link)
{
    *(link->pprev_hash) = link->next_hash;
    if (link->next_hash != NULL)
        link->next_hash->pprev_hash = link->pprev_hash;
    link->next_hash = NULL;
    link->pprev_hash = NULL;
}


void smart_roaming_block_init(struct rtl8192cd_priv *priv)
{
	priv->sr_block.sr_block_status = 1;

    /* memory allocated structure*/
    priv->sr_block.sr_block_ent = (struct smart_roaming_block_entry *)
                kmalloc((sizeof(struct smart_roaming_block_entry) * SMART_ROAMING_BLOCK_MAX_NUM), GFP_ATOMIC);
    if (!priv->sr_block.sr_block_ent) {
        panic_printk(KERN_ERR "Can't kmalloc for smart_roaming_block_entry (size %d)\n", sizeof(struct smart_roaming_block_entry) * SMART_ROAMING_BLOCK_MAX_NUM);
        goto err;
    }          
    memset(priv->sr_block.sr_block_machash, 0, sizeof(priv->sr_block.sr_block_machash));
    memset(priv->sr_block.sr_block_ent, 0, sizeof(struct smart_roaming_block_entry) * SMART_ROAMING_BLOCK_MAX_NUM);

#ifdef SMP_SYNC
    spin_lock_init(&(priv->sr_block.sr_block_lock));
#endif

    return;

err:
    if(priv->sr_block.sr_block_ent) 
        kfree(priv->sr_block.sr_block_ent);

    return;
}

void smart_roaming_block_deinit(struct rtl8192cd_priv *priv)
{
	priv->sr_block.sr_block_status = 0;

	if(priv->sr_block.sr_block_ent)
		kfree(priv->sr_block.sr_block_ent);              
}

static struct smart_roaming_block_entry *smart_roaming_block_lookup(struct rtl8192cd_priv *priv, unsigned char *mac)
{
    unsigned long offset;
    int hash;
    struct smart_roaming_block_link_list	*link;
    struct smart_roaming_block_entry		*ent;

    offset = (unsigned long)(&((struct smart_roaming_block_entry *)0)->link_list);
    hash = smart_roaming_block_mac_hash(mac, SMART_ROAMING_BLOCK_HASH_SIZE);
    link = priv->sr_block.sr_block_machash[hash];
    while (link != NULL)
    {
        ent = (struct smart_roaming_block_entry *)((unsigned long)link - offset);
        if (ent->used && !memcmp(ent->mac, mac, MACADDRLEN))
        {
            return ent;
        }
        link = link->next_hash;
    }

    return NULL;
}

void smart_roaming_block_add(struct rtl8192cd_priv *priv, unsigned char *mac)
{
    struct smart_roaming_block_entry * ent = NULL;
    int i, hash;

    unsigned long flags;    
    SAVE_INT_AND_CLI(flags);
#ifdef SMP_SYNC  
	SMP_LOCK_SR_BLOCK_LIST(flags);
#endif
    for (i=0; i<SMART_ROAMING_BLOCK_MAX_NUM; i++)
    {
        if (!priv->sr_block.sr_block_ent[i].used)
        {
            ent = &(priv->sr_block.sr_block_ent[i]);
            break;
        }
    }

    if(ent)
    {
		ent->used = 1;
        memcpy(ent->mac, mac, MACADDRLEN);
        ent->aging = priv->pmib->sr_profile.block_aging;
        hash = smart_roaming_block_mac_hash(mac, SMART_ROAMING_BLOCK_HASH_SIZE);
        smart_roaming_block_mac_hash_link(&(ent->link_list), &(priv->sr_block.sr_block_machash[hash]));
#ifdef SMP_SYNC 
		SMP_UNLOCK_SR_BLOCK_LIST(flags);
#endif
		RESTORE_INT(flags); 		   
        return;
    }
#ifdef SMP_SYNC
	SMP_UNLOCK_SR_BLOCK_LIST(flags);
#endif
	RESTORE_INT(flags); 		   
    return;
}

void smart_roaming_block_expire(struct rtl8192cd_priv *priv, unsigned char *mac)
{
	int i;
	unsigned long offset;
	struct smart_roaming_block_link_list *link, *temp_link;
	struct smart_roaming_block_entry * ent;

	unsigned long flags = 0;	
	SAVE_INT_AND_CLI(flags);
#ifdef SMP_SYNC
	SMP_LOCK_SR_BLOCK_LIST(flags);
#endif	
	offset = (unsigned long)(&((struct smart_roaming_block_entry *)0)->link_list);

	for (i=0; i<SMART_ROAMING_BLOCK_HASH_SIZE; i++)
	{
		link = priv->sr_block.sr_block_machash[i];
		while (link != NULL)
		{
			temp_link = link->next_hash;
			ent = (struct smart_roaming_block_entry *)((unsigned long)link - offset);
			if(ent->used && !mac && ent->aging)
			{
				ent->aging -= 1;
				if(ent->aging == 0)
				{
					ent->used = 0;
					smart_roaming_block_mac_hash_unlink(link);
				}
			}
			else if(ent->used && mac && !memcmp(ent->mac, mac, MACADDRLEN)){
				ent->used = 0;
				smart_roaming_block_mac_hash_unlink(link);
			}			
			link = temp_link;
		}
	}
	
	RESTORE_INT(flags);
#ifdef SMP_SYNC
	SMP_UNLOCK_SR_BLOCK_LIST(flags);	
#endif
}

unsigned char smart_roaming_block_check_request(struct rtl8192cd_priv *priv, unsigned char *mac)
{
	struct smart_roaming_block_entry	*block_ent;
	unsigned char ret = 0;
	unsigned long flags = 0;

	SAVE_INT_AND_CLI(flags);
#ifdef SMP_SYNC
	SMP_LOCK_SR_BLOCK_LIST(flags);
#endif	
	block_ent =  smart_roaming_block_lookup(priv, mac);
	if(block_ent && block_ent->used && block_ent->aging)
		ret = 1;
	
	RESTORE_INT(flags);
#ifdef SMP_SYNC
	SMP_UNLOCK_SR_BLOCK_LIST(flags);
#endif
	return ret;
}

void smart_roaming_csa_expire_check(struct rtl8192cd_priv *priv)
{
	unsigned int  throughput = (priv->ext_stats.tx_avarage+priv->ext_stats.rx_avarage) >> 17;
	int           csa_ready_timeout = GET_MIB(priv)->sr_profile.csa_ready_timeout;
	unsigned char csa_metric_thres = GET_MIB(priv)->sr_profile.csa_metric_thres;
	unsigned char csa_state = 0;
	int i = 0;

	if(priv->assoc_num) csa_state |= BIT0;
	if(throughput < 1) csa_state |= BIT1;
	priv->csa_state = csa_state;

	if(IS_ROOT_INTERFACE(priv)) {
		csa_state |= priv->csa_state;
#ifdef MBSSID
		if (GET_ROOT(priv)->pmib->miscEntry.vap_enable) {
			for (i = 0; i < RTL8192CD_NUM_VWLAN; i++) {
				if(IS_DRV_OPEN(priv->pvap_priv[i])) {
					csa_state |= (priv->pvap_priv[i]->csa_state & BIT0); /* at least one STA over all interfaces */
					csa_state &= ((priv->pvap_priv[i]->csa_state & BIT1) | BIT0); /* no traffic over all interfaces */
				}
			}
		}
#endif
		if(priv->ext_stats.ch_utilization*100/255 > csa_metric_thres) csa_state |= BIT2;
		/* if there is at least one STA over all interfaces, no traffic over all interfaces, and heavy clm */
		if(csa_state == (BIT2|BIT1|BIT0))
			priv->csa_ready_time++;
		else if(priv->csa_ready_time - 2 >= 0)
			priv->csa_ready_time -= 2;
		else
			priv->csa_ready_time = 0;

		//printk("priv->csa_ready_time=%d", priv->csa_ready_time);

#ifdef RTK_MESH_SIMPLE_CSA
		if(priv->mesh_CSA_blocking) {
			priv->csa_request = 0; /* blocking, everybody can't do csa */
			return;
		}
#endif
		if(csa_ready_timeout && priv->csa_ready_time >= csa_ready_timeout) /* I can and I want to do csa */
			priv->csa_request = 3;
		else if(csa_state == (BIT2|BIT1|BIT0)) /* csa_ready_time ongoining, other nodes can do csa */
			priv->csa_request = 2;
		else if(csa_state & (BIT1))/* I don't have traffic, other nodes can do csa */
			priv->csa_request = 1;
		else /* everybody can't do csa */
			priv->csa_request = 0; 
	}
	return;
}

