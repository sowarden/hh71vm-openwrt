/*
 *  a4 sta functions
 *
 *  $Id: 8192cd_a4_sta.c,v 1.1 2010/10/13 06:38:58 davidhsu Exp $
 *
 *  Copyright (c) 2010 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _8192CD_STA_CONTROL_C_

#ifdef __KERNEL__
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/timer.h>
#endif

#include "./8192cd_cfg.h"
#include "./8192cd.h"
#include "./8192cd_headers.h"
#include "./8192cd_debug.h"

#ifdef STA_CONTROL
int DEBUG_TIME_INETRVAL = 5;
//#define STA_CONTROL_DEBUGMSG
#ifdef STA_CONTROL_DEBUGMSG
unsigned char stactrl_debug_mac[6] = {0xff,0xff,0xff,0xff,0xff,0xff};


#define STACTRL_DEBUG(mac, fmt, args...)     \
	do { \
		if(memcmp(mac, stactrl_debug_mac, MACADDRLEN)==0) \			
			panic_printk("[%s, %d][%s] "fmt" from %02x:%02x:%02x:%02x:%02x:%02x\n",__func__,__LINE__, priv->dev->name, ## args, mac[0], mac[1],mac[2],mac[3],mac[4],mac[5]); \
	}while(0)

#define STACTRL_INIT_DEBUG(fmt, args...)  panic_printk("[%s, %d][%s] "fmt, __func__,__LINE__, priv->dev->name, ## args);
      
#else
#define STACTRL_DEBUG(mac, fmt, args...) {}
#define STACTRL_INIT_DEBUG
#endif

#ifdef STA_CONTROL_BAND_TRANSITION
// STA's current load (Tx + Rx) in Kbps
#define STA_CUR_LOAD(pstat) ((pstat->tx_avarage + pstat->rx_avarage) * 8 / 1024)
#define BAND_TRANS_BLOCK_TIME 20
static int stactrl_cal_basic_load_balance_score(struct rtl8192cd_priv *priv)
{
	int score = (priv->pshare->total_tp * 100 / priv_max_tp(priv) * priv->pmib->staControl.stactrl_ap_rate_weight)
				+ (priv->pshare->total_sta_num * priv->pmib->staControl.stactrl_sta_num_weight)
				+ (GET_ROOT(priv)->ext_stats.ch_utilization*100/255 * priv->pmib->staControl.stactrl_clm_weight);

	return score;
}
#endif

__inline__ static int stactrl_mac_hash(unsigned char *networkAddr, int hash_size)
{
    unsigned long x;

    x = networkAddr[0] ^ networkAddr[1] ^ networkAddr[2] ^ networkAddr[3] ^ networkAddr[4] ^ networkAddr[5];

    return x & (hash_size - 1);
}

__inline__ static void stactrl_mac_hash_link(struct stactrl_link_list *link, struct stactrl_link_list **head)
{
    link->next_hash = *head;

    if (link->next_hash != NULL)
        link->next_hash->pprev_hash = &link->next_hash;
    *head = link;
    link->pprev_hash = head;
}

__inline__ static void stactrl_mac_hash_unlink(struct stactrl_link_list *link)
{		
    *(link->pprev_hash) = link->next_hash;	
    if (link->next_hash != NULL)
        link->next_hash->pprev_hash = link->pprev_hash;
    link->next_hash = NULL;
    link->pprev_hash = NULL;		
}


#ifdef CONFIG_RTL_PROC_NEW
int stactrl_info_read(struct seq_file *s, void *data)
#else
int stactrl_info_read(char *buf, char **start, off_t offset, int length, int *eof, void *data)
#endif
{
    struct net_device *dev = PROC_GET_DEV();
    struct rtl8192cd_priv *priv = GET_DEV_PRIV(dev);
    struct stactrl_preferband_entry * prefer_ent;
    struct stactrl_block_entry * block_ent;


    int i;
    int pos = 0;
    PRINT_ONE(" -- sta control info -- ", "%s", 1);
    PRINT_ONE(priv->stactrl.stactrl_status, "    sta control status: %d", 1);
    PRINT_ONE(priv->stactrl.stactrl_prefer, "    is prefer band: %d", 1);
    PRINT_ONE(STA_CONTROL_ALGO, "    algorithm: %d", 1);

    if(priv->stactrl.stactrl_status) {
        if(priv->stactrl.stactrl_prefer)
        {
            PRINT_ONE(priv->stactrl.stactrl_priv_sc->dev->name, "    non-prefer band: %s", 1);        
            PRINT_ONE("    -- prefer band capable client list -- ", "%s", 1);
            for (i=0; i<MAX_STACTRL_PREFERBAND_NUM; i++)
            {
                prefer_ent = &(priv->stactrl.stactrl_preferband_ent[i]);
                if (prefer_ent->used)
                {
                    PRINT_ARRAY_ARG("      STA_MAC: ",    prefer_ent->mac, "%02x", MACADDRLEN);
                    PRINT_ONE(prefer_ent->rssi,  "        rssi: %d", 1);
                    PRINT_ONE(prefer_ent->aging,  "        aging: %d", 1);
#if STA_CONTROL_ALGO == STA_CONTROL_ALGO3
                    PRINT_ONE(prefer_ent->retry,	"      retry: %d", 1);
                    PRINT_ONE(prefer_ent->local,	"      local: %d", 1);
					if(prefer_ent->kickoff > 0)
                    	PRINT_ONE(prefer_ent->kickoff,	"      kickoff: %d", 1);
#else
					PRINT_ONE(prefer_ent->blockRetry,  "        blockRetry: %d", 1);
                    PRINT_ONE(prefer_ent->blockRetry_resetTimer,  "        blockRetry_resetTimer: %d", 1);
                    PRINT_ONE(prefer_ent->block_timer,  "        block_timer: %d", 1);
#endif
                }
            }
#if STA_CONTROL_ALGO == STA_CONTROL_ALGO3
			if(STACTRL_BLOCK_5G){
				/* prefer band block entry */			
				PRINT_ONE("    -- prefer band blocklist -- ", "%s", 1);
				for (i=0; i<MAX_STACTRL_BLOCK_NUM; i++)
	            {
	                block_ent = &(priv->stactrl.stactrl_block_ent[i]);
	                if (block_ent->used)
	                {
	                    PRINT_ARRAY_ARG("      Block STA_MAC: ",    block_ent->mac, "%02x", MACADDRLEN);
	                    PRINT_ONE(block_ent->timerX,  "        timerX: %d", 1);
	                    PRINT_ONE(block_ent->retryY,  "        retryY: %d", 1);
						PRINT_ONE(block_ent->aging,	  "        aging: %d", 1);
	                }				
	            }
			}
#endif
        }
        else
        {
            PRINT_ONE(priv->stactrl.stactrl_priv_sc->dev->name, "    prefer band: %s", 1);                
            PRINT_ONE("    -- non-prefer band blocklist -- ", "%s", 1);
            for (i=0; i<MAX_STACTRL_BLOCK_NUM; i++)
            {
                block_ent = &(priv->stactrl.stactrl_block_ent[i]);
                if (block_ent->used)
                {
                    PRINT_ARRAY_ARG("      STA_MAC: ",    block_ent->mac, "%02x", MACADDRLEN);
                    PRINT_ONE(block_ent->timerX,  "        timerX: %d", 1);
                    PRINT_ONE(block_ent->retryY,  "        retryY: %d", 1);
#if STA_CONTROL_ALGO == STA_CONTROL_ALGO3
					PRINT_ONE(block_ent->aging,		"      aging: %d", 1);
#endif
                }
            }
#if STA_CONTROL_ALGO == STA_CONTROL_ALGO3
			if(STACTRL_BLOCK_5G){
				PRINT_ONE("    -- non-prefer band prefer list -- ", "%s", 1);
				for (i=0; i<MAX_STACTRL_PREFERBAND_NUM; i++)
	            {
	                prefer_ent = &(priv->stactrl.stactrl_preferband_ent[i]);
	                if (prefer_ent->used)
	                {
	                    PRINT_ARRAY_ARG("      STA_MAC: ",    prefer_ent->mac, "%02x", MACADDRLEN);
	                    PRINT_ONE(prefer_ent->rssi,  "        rssi: %d", 1);
	                    PRINT_ONE(prefer_ent->aging,  "        aging: %d", 1);

	                    PRINT_ONE(prefer_ent->retry,	"      retry: %d", 1);
	                    PRINT_ONE(prefer_ent->local,	"      local: %d", 1);
						if(prefer_ent->kickoff > 0)
	                    	PRINT_ONE(prefer_ent->kickoff,	"      kickoff: %d", 1);
	                }
	            }
			}
#endif
        }
    }

#ifdef STA_CONTROL_DEBUGMSG
    PRINT_ONE("    -- debug mac -- ", "%s", 1);
    PRINT_ARRAY_ARG("      STA_MAC: ",    stactrl_debug_mac, "%02x", MACADDRLEN);
#endif
    return pos;
}

int stactrl_info_write(struct file *file, const char *buffer,
                  unsigned long count, void *data)
{
#ifdef STA_CONTROL_DEBUGMSG
    if (buffer)
    {
        get_array_val(stactrl_debug_mac, (char *)buffer, count);
    }
#endif
    return count;
}


void stactrl_init(struct rtl8192cd_priv *priv)
{
    struct rtl8192cd_priv *priv_sc;
    struct rtl8192cd_priv *prefer_priv;
    int i;
    
    STACTRL_INIT_DEBUG("groupID: %d\n",  priv->pmib->staControl.stactrl_groupID);

    if(priv->pmib->staControl.stactrl_enable > 1)
        priv->pmib->staControl.stactrl_enable = 1;
    if(priv->pmib->staControl.stactrl_prefer_band > 1)
        priv->pmib->staControl.stactrl_prefer_band = 1;
    
    priv->stactrl.stactrl_status = 0;
    priv->stactrl.stactrl_priv_sc = NULL;
	priv->stactrl.stactrl_block5G = 0;
	priv->stactrl.stactrl_enable_hiddenAP = 0;

    if(!(OPMODE & WIFI_AP_STATE) || priv->pmib->staControl.stactrl_enable == 0) {
        STACTRL_INIT_DEBUG("WARNING: not ap mode or not enabled\n");
        return;
    }

	if(priv->pmib->staControl.stactrl_functions & BIT(0)){
		STACTRL_INIT_DEBUG("Enable block_5G feature\n");
		priv->stactrl.stactrl_block5G = 1;
	}
	else
		STACTRL_INIT_DEBUG("Disable block_5G feature\n");

	if(priv->pmib->staControl.stactrl_functions & BIT(1)){
		STACTRL_INIT_DEBUG("Enable hiddenAP feature\n");
		priv->stactrl.stactrl_enable_hiddenAP = 1;
	}
	else
		STACTRL_INIT_DEBUG("Disable hiddenAP feature\n");
	
    /*search other band to find the priv with same groupID*/
    for(i = 0; i < RTL8192CD_NUM_VWLAN+1; i++) {
        if(i == 0) {
            priv_sc = GET_ROOT(priv)->stactrl.stactrl_rootpriv_sc;
        }
        #ifdef MBSSID
        else { /*search vap of other band*/
            priv_sc = GET_ROOT(priv)->stactrl.stactrl_rootpriv_sc->pvap_priv[i - 1];

        }
        #endif
        if((priv_sc!=NULL) && priv_sc->pmib->staControl.stactrl_groupID == priv->pmib->staControl.stactrl_groupID &&
             IS_DRV_OPEN(priv_sc) && 
             priv_sc->pmib->staControl.stactrl_enable == 1 &&
             (priv_sc->pmib->dot11OperationEntry.opmode & WIFI_AP_STATE) &&
             priv_sc->stactrl.stactrl_status == 0) 
         {        
             priv->stactrl.stactrl_priv_sc = priv_sc;
             break;
         }        
    }

    
    if(priv->stactrl.stactrl_priv_sc == NULL || priv_sc == NULL) { /*can not find other band with same groupID*/        
        STACTRL_INIT_DEBUG("WARNING: not find available priv\n");
        return;
    }
    
    STACTRL_INIT_DEBUG("find other band : %s\n", priv->stactrl.stactrl_priv_sc->dev->name);
    priv_sc->stactrl.stactrl_priv_sc = priv;
    

    /*find other band's priv with same groupID*/
    /*setting prefer band and non-prefer band*/
	priv->stactrl.stactrl_prefer = priv->pmib->staControl.stactrl_prefer_band;
    priv->stactrl.stactrl_priv_sc->stactrl.stactrl_prefer = priv->pmib->staControl.stactrl_prefer_band?0:1;


    /* memory allocated structure*/
    if(priv->stactrl.stactrl_prefer) {
        prefer_priv = priv;
    }
    else {
        prefer_priv = priv->stactrl.stactrl_priv_sc;
    }
    priv_sc = prefer_priv->stactrl.stactrl_priv_sc;


    STACTRL_INIT_DEBUG("prefer band : %s, non-prefer band: %s\n", prefer_priv->dev->name, priv_sc->dev->name);

	/*check SSID of different bands */	
	if(strcmp(prefer_priv->pmib->dot11StationConfigEntry.dot11DesiredSSID, priv_sc->pmib->dot11StationConfigEntry.dot11DesiredSSID)){
		STACTRL_INIT_DEBUG("WARNING: different SSID of preferband(%s) and non-preferband(%s)\n", 
					prefer_priv->pmib->dot11StationConfigEntry.dot11DesiredSSID, 
					priv_sc->pmib->dot11StationConfigEntry.dot11DesiredSSID);
		return;
	}

    prefer_priv->stactrl.stactrl_preferband_ent = (struct stactrl_preferband_entry *)
                kmalloc((sizeof(struct stactrl_preferband_entry) * MAX_STACTRL_PREFERBAND_NUM), GFP_ATOMIC);
    if (!prefer_priv->stactrl.stactrl_preferband_ent) {
        printk(KERN_ERR "Can't kmalloc for stactrl_preferband_entry (size %d)\n", sizeof(struct stactrl_preferband_entry) * MAX_STACTRL_PREFERBAND_NUM);
        goto err;
    }
    memset(prefer_priv->stactrl.stactrl_preferband_machash, 0, sizeof(prefer_priv->stactrl.stactrl_preferband_machash));
    memset(prefer_priv->stactrl.stactrl_preferband_ent, 0, sizeof(struct stactrl_preferband_entry) * MAX_STACTRL_PREFERBAND_NUM);     

#if STA_CONTROL_ALGO == STA_CONTROL_ALGO3
	/* malloc prefrband_ent for non-prefer band */
	if(STACTRL_BLOCK_5G){
		priv_sc->stactrl.stactrl_preferband_ent = (struct stactrl_preferband_entry *)
	                kmalloc((sizeof(struct stactrl_preferband_entry) * MAX_STACTRL_PREFERBAND_NUM), GFP_ATOMIC);
	    if (!priv_sc->stactrl.stactrl_preferband_ent) {
	        printk(KERN_ERR "Can't kmalloc for stactrl_preferband_entry (size %d)\n", sizeof(struct stactrl_preferband_entry) * MAX_STACTRL_PREFERBAND_NUM);
	        goto err;
	    }
	    memset(priv_sc->stactrl.stactrl_preferband_machash, 0, sizeof(priv_sc->stactrl.stactrl_preferband_machash));
	    memset(priv_sc->stactrl.stactrl_preferband_ent, 0, sizeof(struct stactrl_preferband_entry) * MAX_STACTRL_PREFERBAND_NUM);     	
	}
#endif
    priv_sc->stactrl.stactrl_block_ent = (struct stactrl_block_entry *)
                kmalloc((sizeof(struct stactrl_block_entry) * MAX_STACTRL_BLOCK_NUM), GFP_ATOMIC);
    if (!priv_sc->stactrl.stactrl_block_ent) {
        printk(KERN_ERR "Can't kmalloc for stactrl_preferband_entry (size %d)\n", sizeof(struct stactrl_block_entry) * MAX_STACTRL_BLOCK_NUM);
        goto err;
    }          
    memset(priv_sc->stactrl.stactrl_block_machash, 0, sizeof(priv_sc->stactrl.stactrl_block_machash));
    memset(priv_sc->stactrl.stactrl_block_ent, 0, sizeof(struct stactrl_block_entry) * MAX_STACTRL_BLOCK_NUM);

#if STA_CONTROL_ALGO == STA_CONTROL_ALGO3
	/* malloc stactrl_block_ent for prefer band */
	if(STACTRL_BLOCK_5G){
		prefer_priv->stactrl.stactrl_block_ent = (struct stactrl_block_entry *)
	                kmalloc((sizeof(struct stactrl_block_entry) * MAX_STACTRL_BLOCK_NUM), GFP_ATOMIC);
	    if (!prefer_priv->stactrl.stactrl_block_ent) {
	        printk(KERN_ERR "Can't kmalloc for stactrl_preferband_entry (size %d)\n", sizeof(struct stactrl_block_entry) * MAX_STACTRL_BLOCK_NUM);
	        goto err;
	    }          
	    memset(prefer_priv->stactrl.stactrl_block_machash, 0, sizeof(prefer_priv->stactrl.stactrl_block_machash));
	    memset(prefer_priv->stactrl.stactrl_block_ent, 0, sizeof(struct stactrl_block_entry) * MAX_STACTRL_BLOCK_NUM);	
	}
#endif
    
#if STA_CONTROL_ALGO == STA_CONTROL_ALGO2
    init_timer(&priv_sc->stactrl.stactrl_timer);
    priv_sc->stactrl.stactrl_timer.data = (unsigned long) priv_sc;
    priv_sc->stactrl.stactrl_timer.function = stactrl_non_prefer_expire;
    mod_timer(&priv_sc->stactrl.stactrl_timer, jiffies + RTL_MILISECONDS_TO_JIFFIES(STACTRL_NON_PREFER_TIMER));    
#endif

#ifdef SMP_SYNC
    spin_lock_init(&(prefer_priv->stactrl.stactrl_lock));
    spin_lock_init(&(priv_sc->stactrl.stactrl_lock));
#endif

    /*start up sta control*/
    prefer_priv->stactrl.stactrl_status = 1;
    priv_sc->stactrl.stactrl_status = 1;
	priv_sc->stactrl.stactrl_hiddenAP_aging = 0;
    return;


err:
    if(prefer_priv->stactrl.stactrl_preferband_ent)
        kfree(prefer_priv->stactrl.stactrl_preferband_ent);
    if(priv_sc->stactrl.stactrl_block_ent) 
        kfree(priv_sc->stactrl.stactrl_block_ent);
#if STA_CONTROL_ALGO == STA_CONTROL_ALGO3
	if(STACTRL_BLOCK_5G){
		if(priv_sc->stactrl.stactrl_preferband_ent)
	        kfree(priv_sc->stactrl.stactrl_preferband_ent);
	    if(prefer_priv->stactrl.stactrl_block_ent) 
	        kfree(prefer_priv->stactrl.stactrl_block_ent);
	}
#endif

    return;
}


void stactrl_deinit(struct rtl8192cd_priv *priv)
{
    struct rtl8192cd_priv *priv_sc;
    struct rtl8192cd_priv *prefer_priv;

    STACTRL_INIT_DEBUG("\n");
    if(priv->stactrl.stactrl_status) {
        if(priv->stactrl.stactrl_prefer) {
            prefer_priv = priv;
        }
        else {
            prefer_priv = priv->stactrl.stactrl_priv_sc;
        }
        priv_sc = prefer_priv->stactrl.stactrl_priv_sc;

        STACTRL_INIT_DEBUG("prefer band : %s, non-prefer band: %s\n", prefer_priv->dev->name, priv_sc->dev->name);
        
        if(prefer_priv->stactrl.stactrl_preferband_ent)
            kfree(prefer_priv->stactrl.stactrl_preferband_ent);
        if(priv_sc->stactrl.stactrl_block_ent) 
            kfree(priv_sc->stactrl.stactrl_block_ent);  
		
#if STA_CONTROL_ALGO == STA_CONTROL_ALGO3
		if(STACTRL_BLOCK_5G){
			if(priv_sc->stactrl.stactrl_preferband_ent)
				kfree(priv_sc->stactrl.stactrl_preferband_ent);
			if(prefer_priv->stactrl.stactrl_block_ent) 
				kfree(prefer_priv->stactrl.stactrl_block_ent);
		}
#endif
        
        prefer_priv->stactrl.stactrl_status = 0;
        priv_sc->stactrl.stactrl_status = 0;
        
#if STA_CONTROL_ALGO == STA_CONTROL_ALGO2
        if (timer_pending(&priv_sc->stactrl.stactrl_timer))
        {
            del_timer(&priv_sc->stactrl.stactrl_timer);
        }
#endif
    }
}

static struct stactrl_block_entry *stactrl_block_lookup(struct rtl8192cd_priv *priv, unsigned char *mac)
{
    unsigned long offset;
    int hash;
    struct stactrl_link_list *link;
    struct stactrl_block_entry * ent;

    offset = (unsigned long)(&((struct stactrl_block_entry *)0)->link_list);
    hash = stactrl_mac_hash(mac, STACTRL_BLOCK_HASH_SIZE);
    link = priv->stactrl.stactrl_block_machash[hash];
    while (link != NULL)
    {
        ent = (struct stactrl_block_entry *)((unsigned long)link - offset);
        if (!memcmp(ent->mac, mac, MACADDRLEN))
        {
            return ent;
        }
        link = link->next_hash;
    }

    return NULL;

}

/*return value: NULL: error,  other:success*/
static struct stactrl_block_entry * stactrl_block_add(struct rtl8192cd_priv *priv, unsigned char *mac)
{
    struct stactrl_block_entry * ent = NULL;
    int i, hash;
    ASSERT(mac);
	
	//STACTRL_DEBUG(mac, "is added to [%s] blocklist",priv->dev->name);

    ent = stactrl_block_lookup(priv, mac); //Avoid exist mac to add again
 	if(ent != NULL)  		return ent;


    /* find an empty entry*/
    for (i=0; i<MAX_STACTRL_BLOCK_NUM; i++)
    {
        if (!priv->stactrl.stactrl_block_ent[i].used)
        {
            ent = &(priv->stactrl.stactrl_block_ent[i]);
            break;
        }
    }

    if(ent)
    {
        ent->used = 1;
        memcpy(ent->mac, mac, MACADDRLEN);
        ent->retryY = STACTRL_BLOCK_RETRY_Y;
        ent->timerX = STACTRL_BLOCK_EXPIRE_X; 
        ent->timerZ = 0;
#if STA_CONTROL_ALGO == STA_CONTROL_ALGO3
        ent->aging = 0;
#endif
        hash = stactrl_mac_hash(mac, STACTRL_BLOCK_HASH_SIZE);		
		//panic_printk("[%s %d][%s]Target mac not existed. Add. aging:%d\n",__FUNCTION__,__LINE__,priv->dev->name, ent->aging);
        stactrl_mac_hash_link(&(ent->link_list), &(priv->stactrl.stactrl_block_machash[hash]));
        return ent;
    }

    return NULL;
}

/*return value: 0:success, other: error*/
static int stactrl_block_delete(struct rtl8192cd_priv *priv, unsigned char *mac)
{
    unsigned long offset;
    int hash;
    struct stactrl_link_list *link;
    struct stactrl_block_entry * ent;

    
   
	/*panic_printk("[%s %d][%s]Target mac:%02x:%02x:%02x:%02x:%02x:%02x\n", 
		__FUNCTION__,__LINE__,priv->dev->name,
		mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);*/

    offset = (unsigned long)(&((struct stactrl_block_entry *)0)->link_list);
    hash = stactrl_mac_hash(mac, STACTRL_BLOCK_HASH_SIZE);
    link = priv->stactrl.stactrl_block_machash[hash];
    while (link != NULL)
    {
        ent = (struct stactrl_block_entry *)((unsigned long)link - offset);		
        if (!memcmp(ent->mac, mac, MACADDRLEN))
        {
            ent->used = 0;
            stactrl_mac_hash_unlink(link);
			/*panic_printk("[%s %d][%s]Delete existed mac:%02x:%02x:%02x:%02x:%02x:%02x\n", 
					__FUNCTION__,__LINE__,priv->dev->name,
					ent->mac[0],ent->mac[1],ent->mac[2],ent->mac[3],ent->mac[4],ent->mac[5]);*/
			
            STACTRL_DEBUG(ent->mac, "block delete");
            
            return 0;
        }
        link = link->next_hash;
    }
    //panic_printk("[%s %d][%s]Block list NULL.\n", __FUNCTION__,__LINE__,priv->dev->name);    

    
	return 1;

}

#ifdef RTK_MULTI_AP
#ifdef CONFIG_IEEE80211V 
void set_11v_target_band(struct rtl8192cd_priv *priv, unsigned char *macaddr)
{
	int i, empty_slot;
	unsigned int flags;
	struct target_transition_list list;

	memset(&list, 0, sizeof(struct target_transition_list));
	memcpy(list.addr, macaddr, MACADDRLEN);
	list.mode = 128; //set bit 7 (request mode mandate)
	list.disassoc_timer = 3000;
	memcpy(list.target_bssid, priv->stactrl.stactrl_priv_sc->pmib->dot11StationConfigEntry.dot11Bssid, MACADDRLEN);
	list.opclass = rm_get_op_class(priv, priv->stactrl.stactrl_priv_sc->pmib->dot11RFEntry.dot11channel); // opclass
	list.channel = priv->stactrl.stactrl_priv_sc->pmib->dot11RFEntry.dot11channel; //channel

	struct stat_info *pstat = get_stainfo(priv, list.addr);
	if (pstat == NULL) {
		panic_printk("pstat is null!");
		return;
	}

	pstat->bssTransTriggered = 0;

	SAVE_INT_AND_CLI(flags);

	SMP_LOCK_TRANSITION_LIST(flags);
	addEntryTransitionList(priv, &list);
	SMP_UNLOCK_TRANSITION_LIST(flags);

	RESTORE_INT(flags);

}

#endif
#endif

#if STA_CONTROL_ALGO == STA_CONTROL_ALGO3
//20170106
static unsigned char stactrl_sta_in_the_other_band(struct rtl8192cd_priv *priv, unsigned char *mac)
{
    struct rtl8192cd_priv *priv_tmp=NULL;
	struct stat_info *pstat=NULL;
	extern u32 if_priv_stactrl[NUM_WLAN_IFACE];
	int i;

	if(priv->pshare->wlandev_idx == 0)//wlan0, check wlan1
		priv_tmp = (struct rtl8192cd_priv *)if_priv_stactrl[1];
	else if(priv->pshare->wlandev_idx == 1)//wlan1, check wlan0
		priv_tmp = (struct rtl8192cd_priv *)if_priv_stactrl[0];

	if(priv_tmp==NULL){
		panic_printk("[STA_CONTROL] Error, priv_tmp should not be NULL !!\n");
		return 0;
	}

	for(i=0; i<NUM_STAT; i++)
	{
		if (priv_tmp->pshare->aidarray[i] && (priv_tmp->pshare->aidarray[i]->used == TRUE)) 
		{
			pstat = &(priv_tmp->pshare->aidarray[i]->station);
			if(pstat && !memcmp(pstat->cmn_info.mac_addr, mac, MACADDRLEN))
			{
				//20170321, return current rssi to associated AP
				return pstat->rssi;
			}
		}
	}
	
    return 0;
}

//20170106
static unsigned long stactrl_sta_in_the_prefer_band(struct rtl8192cd_priv *priv, unsigned char *mac)
{
	struct stat_info *pstat=NULL;
	int i;

	for(i=0; i<NUM_STAT; i++)
	{
		if (priv->pshare->aidarray[i] && (priv->pshare->aidarray[i]->used == TRUE)) 
		{
			pstat = &(priv->pshare->aidarray[i]->station);
			if(pstat && !memcmp(pstat->cmn_info.mac_addr, mac, MACADDRLEN))
			{
				return pstat->link_time;
			}
		}
	}
	
    return 0;
}
#endif

static struct stactrl_preferband_entry *stactrl_preferband_sta_lookup(struct rtl8192cd_priv *priv, unsigned char *mac)
{
    unsigned long offset;
    int hash;
    struct stactrl_link_list *link;
    struct stactrl_preferband_entry * ent;

    offset = (unsigned long)(&((struct stactrl_preferband_entry *)0)->link_list);
    hash = stactrl_mac_hash(mac, STACTRL_PREFERBAND_HASH_SIZE);
    link = priv->stactrl.stactrl_preferband_machash[hash];
    while (link != NULL)
    {
        ent = (struct stactrl_preferband_entry *)((unsigned long)link - offset);
        if (!memcmp(ent->mac, mac, MACADDRLEN))
        {
            return ent;
        }
        link = link->next_hash;
    }

    return NULL;

}

#if STA_CONTROL_ALGO == STA_CONTROL_ALGO3
void stactrl_preferband_sta_add(struct rtl8192cd_priv *priv, unsigned char *mac, unsigned char rssi, unsigned int is_local)
{
    struct stactrl_preferband_entry * ent;
    int i, hash;
    int temp_aging, temp_index;
    unsigned long flags = 0;
    ASSERT(mac);
#if 0
     panic_printk("add prefer %02x%02x%02x%02x%02x%02x %s,  %p\n",
  		mac[0], mac[1],mac[2],mac[3],mac[4],mac[5], priv->dev->name, __builtin_return_address(0));
   
	if(is_local){
		STACTRL_DEBUG(mac, "receive notify, rssi %d", rssi);}
	else{
    	STACTRL_DEBUG(mac, "receive probe, rssi %d", rssi);}
#endif 

    SAVE_INT_AND_CLI(flags);
    SMP_LOCK_STACONTROL_LIST(flags);
    ent = stactrl_preferband_sta_lookup(priv, mac);
    if(ent) /*find existed entry*/
    {
    	//STACTRL_DEBUG(mac, "update");
        if(rssi == 0)
            goto ret;

            ent->aging = 0;
			//reset retry counter when STA connects to the blocking band after it consumes retry counter to zero
			//or when STA connects to the non-blocking band instead of resetting here
			//ent->retry = STACTRL_BLOCK_RETRY_Y;
            ent->rssi = rssi;
            
			//201701015
			//update local
			if(ent->local==0 && is_local==1)
				ent->local = is_local;			
				
        goto ret;
    }


    /* find an empty entry*/
    ent = NULL;
    for (i=0; i<MAX_STACTRL_PREFERBAND_NUM; i++)
    {		
        if (!priv->stactrl.stactrl_preferband_ent[i].used)
        {
        	//STACTRL_DEBUG(mac, "stactrl_preferband_sta_add: add new\n");
            ent = &(priv->stactrl.stactrl_preferband_ent[i]);
            break;
        }
    }

    /* not found, find a entry with max aging*/
    if(ent == NULL)
    {
        temp_aging = 0;
        temp_index = 0;
        for (i=0; i<MAX_STACTRL_PREFERBAND_NUM; i++)
        {
            if(priv->stactrl.stactrl_preferband_ent[i].used && temp_aging < priv->stactrl.stactrl_preferband_ent[i].aging)
            {
            	if(!priv->stactrl.stactrl_preferband_ent[i].local){//20170105
					temp_aging = priv->stactrl.stactrl_preferband_ent[i].aging;
                	temp_index = i;
					}
				}

        }
        ent = &(priv->stactrl.stactrl_preferband_ent[temp_index]);

		//20170105
		//if all stactrl list is local, choose one local sta that have max aging
		if(temp_index==0 && temp_aging==0 && ent->local==1){
			for (i=0; i<MAX_STACTRL_PREFERBAND_NUM; i++)
	        {
	            if(priv->stactrl.stactrl_preferband_ent[i].used && temp_aging < priv->stactrl.stactrl_preferband_ent[i].aging)
	            {
					temp_aging = priv->stactrl.stactrl_preferband_ent[i].aging;
                	temp_index = i;						
				}

        }
        ent = &(priv->stactrl.stactrl_preferband_ent[temp_index]);
		}
		STACTRL_DEBUG(mac, "[%s]del old entry, add new", priv->dev->name);
		
		//panic_printk("[STA_CONTROL][%s] entry full, remove max aging mac:%02x:%02x:%02x:%02x:%02x:%02x, local=%d, temp_index:%d, temp_aging:%d\n",
		//	priv->dev->name,
		//	ent->mac[0],ent->mac[1],ent->mac[2],ent->mac[3],ent->mac[4],ent->mac[5], ent->local, temp_index, temp_aging);
		
        stactrl_mac_hash_unlink(&(ent->link_list));	
    }

	//STACTRL_DEBUG(mac, "[%s]add new reset\n", priv->dev->name);
    ent->used = 1;
    memcpy(ent->mac, mac, MACADDRLEN);
    ent->aging = 0;
	ent->retry = STACTRL_BLOCK_RETRY_Y;
    ent->rssi = rssi;
	ent->local = is_local;//20170105
	ent->kickoff = 0;//20170106		
    hash = stactrl_mac_hash(mac, STACTRL_PREFERBAND_HASH_SIZE);
	STACTRL_DEBUG(mac, "[%s]add new reset, hash:%d", priv->dev->name, hash);
    stactrl_mac_hash_link(&(ent->link_list), &(priv->stactrl.stactrl_preferband_machash[hash]));

ret:
    SMP_UNLOCK_STACONTROL_LIST(flags);
    RESTORE_INT(flags);
}

//20170105
void stactrl_preferband_sta_del(struct rtl8192cd_priv *priv, unsigned char *mac)
{
    struct stactrl_preferband_entry * ent;
    int i, hash;
    int temp_aging, temp_index;
    unsigned long flags = 0;
    ASSERT(mac);
#if 0
     panic_printk("add prefer %02x%02x%02x%02x%02x%02x %s,  %p\n", 
  mac[0], mac[1],mac[2],mac[3],mac[4],mac[5], priv->dev->name, __builtin_return_address(0));
#endif    

	SAVE_INT_AND_CLI(flags);
    SMP_LOCK_STACONTROL_LIST(flags);
	
    ent = stactrl_preferband_sta_lookup(priv, mac);
    if(ent != NULL) /*find existed entry*/
    {
    	ent->used = 0;
        stactrl_mac_hash_unlink(&(ent->link_list));
    }

    SMP_UNLOCK_STACONTROL_LIST(flags);
    RESTORE_INT(flags);
}

static struct stactrl_preferband_entry * stactrl_is_dualband(struct rtl8192cd_priv *priv, unsigned char *mac)
{
    struct stactrl_preferband_entry *prefer_ent, *ret=NULL;

    
	
    prefer_ent = stactrl_preferband_sta_lookup(priv, mac);	
    if(prefer_ent && priv->assoc_num < NUM_STAT)
    {
//20170321, return prefer_ent to check retry counter at non-prefer band    
        ret = prefer_ent;
    }

    return ret;
}

static unsigned char stactrl_is_kickoff(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
    struct stactrl_preferband_entry * prefer_ent;
    unsigned char ret = 0;
    unsigned long   flags=0;

    SAVE_INT_AND_CLI(flags);
    SMP_LOCK_STACONTROL_LIST(flags);

    prefer_ent = stactrl_preferband_sta_lookup(priv, pstat->cmn_info.mac_addr);
    if(prefer_ent && priv->assoc_num < NUM_STAT)
    {
        if(pstat->link_time > STACTRL_KICKOFF_TIME && (pstat->tx_avarage + pstat->rx_avarage) < STACTRL_KICKOFF_TP) {
            ret = 1;
        }
    }
    SMP_UNLOCK_STACONTROL_LIST(flags);
    RESTORE_INT(flags);
    return ret;
}


#else
void stactrl_preferband_sta_add(struct rtl8192cd_priv *priv, unsigned char *mac, unsigned char rssi)
{
    struct stactrl_preferband_entry * ent;
    int i, hash;
    int temp_aging, temp_index;
    unsigned long flags = 0;
    ASSERT(mac);

    STACTRL_DEBUG(mac, "[%s] receive probe", priv->dev->name);
    SAVE_INT_AND_CLI(flags);
    SMP_LOCK_STACONTROL_LIST(flags);
    ent = stactrl_preferband_sta_lookup(priv, mac);
    if(ent) /*find existed entry*/
    {
        if(rssi < STACTRL_PREFERBAND_RSSI - STACTRL_PREFERBAND_RSSI_TOLERANCE)
        {           
            stactrl_block_delete(priv->stactrl.stactrl_priv_sc, mac);
        }

        ent->aging = 0;
        ent->rssi = rssi;
            
        goto ret;
    }


    /* find an empty entry*/
    ent = NULL;
    for (i=0; i<MAX_STACTRL_PREFERBAND_NUM; i++)
    {
        if (!priv->stactrl.stactrl_preferband_ent[i].used)
        {
            ent = &(priv->stactrl.stactrl_preferband_ent[i]);
            break;
        }
    }

    /* not found, find a entry with max aging*/
    if(ent == NULL)
    {
        temp_aging = 0;
        temp_index = 0;
        for (i=0; i<MAX_STACTRL_PREFERBAND_NUM; i++)
        {
            if(priv->stactrl.stactrl_preferband_ent[i].used && temp_aging < priv->stactrl.stactrl_preferband_ent[i].aging)
            {
                temp_aging = priv->stactrl.stactrl_preferband_ent[i].aging;
                temp_index = i;
            }

        }
        ent = &(priv->stactrl.stactrl_preferband_ent[temp_index]);
        stactrl_mac_hash_unlink(&(ent->link_list));
    }

    ent->used = 1;
    memcpy(ent->mac, mac, MACADDRLEN);
    ent->aging = 0;
    ent->rssi = rssi;
	ent->blockRetry = STACTRL_BLOCK_RETRY_Y;
	ent->blockRetry_resetTimer = -1;
    hash = stactrl_mac_hash(mac, STACTRL_PREFERBAND_HASH_SIZE);
    stactrl_mac_hash_link(&(ent->link_list), &(priv->stactrl.stactrl_preferband_machash[hash]));

ret:
    SMP_UNLOCK_STACONTROL_LIST(flags);
    RESTORE_INT(flags);
}

static unsigned char stactrl_is_need(struct rtl8192cd_priv *priv, unsigned char *mac, unsigned char checkRSSI)
{
    struct stactrl_preferband_entry * prefer_ent;
    struct rtl8192cd_priv *priv_temp;
    unsigned int assoc_num = priv->pshare->total_assoc_num;
    int i;
    unsigned char ret = 0;
    unsigned long   flags=0;

    SAVE_INT_AND_CLI(flags);
    SMP_LOCK_STACONTROL_LIST(flags);

    prefer_ent = stactrl_preferband_sta_lookup(priv, mac);
    if(prefer_ent)
    {
        if (assoc_num < NUM_STAT)
        {
            if(checkRSSI) {
                if(prefer_ent->rssi > STACTRL_PREFERBAND_RSSI) 
                    ret = 1;
            }
            else   
                ret = 1;
        }
    }
    SMP_UNLOCK_STACONTROL_LIST(flags);
    RESTORE_INT(flags);
    return ret;
}
#endif

#if STA_CONTROL_ALGO == STA_CONTROL_ALGO3
//20170103
static int stactrl_check_if_kickoff(struct rtl8192cd_priv *priv,unsigned char* kickoff_mac)
{
	struct rtl8192cd_priv *priv_tmp=NULL;
	extern u32 if_priv_stactrl[NUM_WLAN_IFACE];
	int i;

	if(priv->pshare->wlandev_idx == 0)//wlan0, check wlan1
		priv_tmp = (struct rtl8192cd_priv *)if_priv_stactrl[1];
	else if(priv->pshare->wlandev_idx == 1)//wlan1, check wlan0
		priv_tmp = (struct rtl8192cd_priv *)if_priv_stactrl[0];

	if(priv_tmp==NULL){
		panic_printk("[STA_CONTROL] Error, priv_tmp should not be NULL !!\n");
		return 0;
	}

	for (i=0; i<MAX_STACTRL_PREFERBAND_NUM; i++)
	{
         if(priv_tmp->stactrl.stactrl_preferband_ent[i].used)
		 {
			if(!memcmp(priv_tmp->stactrl.stactrl_preferband_ent[i].mac, kickoff_mac, MACADDRLEN))
			{
				if(priv_tmp->stactrl.stactrl_preferband_ent[i].kickoff >= STACTRL_KICKOFF_TIMES)
				{
					//panic_printk("Hit %s mac: %02x:%02x:%02x:%02x:%02x:%02x, kickoff=%d\n",
					//	priv_tmp->dev->name,
					//	kickoff_mac[0],kickoff_mac[1],kickoff_mac[2],
					//	kickoff_mac[3],kickoff_mac[4],kickoff_mac[5],
					//	priv_tmp->stactrl.stactrl_preferband_ent[i].kickoff);
					return 1;
		 		}							
			}
		}
	}

	return 0;
}

//20170103
static void stactrl_record_kickoff(struct rtl8192cd_priv *priv, unsigned char* kickoff_mac)
{
	struct rtl8192cd_priv *priv_tmp=NULL;
	extern u32 if_priv_stactrl[NUM_WLAN_IFACE];
	int i;
	int no_kickoff_mac=1;

	if(priv->pshare->wlandev_idx == 0)//wlan0, check wlan1
		priv_tmp = (struct rtl8192cd_priv *)if_priv_stactrl[1];
	else if(priv->pshare->wlandev_idx == 1)//wlan1, check wlan0
		priv_tmp = (struct rtl8192cd_priv *)if_priv_stactrl[0];

	if(priv_tmp==NULL){
		panic_printk("[STA_CONTROL] Error, priv_tmp should not be NULL !!\n");
		return;
	}

	for (i=0; i<MAX_STACTRL_PREFERBAND_NUM; i++){
         if(priv_tmp->stactrl.stactrl_preferband_ent[i].used){
			if(!memcmp(priv_tmp->stactrl.stactrl_preferband_ent[i].mac, kickoff_mac, MACADDRLEN))
			{
				priv_tmp->stactrl.stactrl_preferband_ent[i].kickoff++;
				STACTRL_DEBUG(kickoff_mac, "[%s] kickoff=%d",
					priv_tmp->dev->name, priv_tmp->stactrl.stactrl_preferband_ent[i].kickoff);
				no_kickoff_mac = 0;
				break;
            }
		 }
	}

	if(no_kickoff_mac)
		STACTRL_DEBUG(kickoff_mac, "[%s]is not in prefer entry",priv->dev->name);

	return;
}
#endif

static void stactrl_preferband_expire(struct rtl8192cd_priv *priv)
{
    int i;
    unsigned long offset;
    struct stactrl_link_list *link, *temp_link;
    struct stactrl_preferband_entry * ent;
    unsigned long flags = 0;
    SAVE_INT_AND_CLI(flags);
    SMP_LOCK_STACONTROL_LIST(flags);

    offset = (unsigned long)(&((struct stactrl_preferband_entry *)0)->link_list);

    for (i=0; i<STACTRL_PREFERBAND_HASH_SIZE; i++)
    {
        link = priv->stactrl.stactrl_preferband_machash[i];
        while (link != NULL)
        {
            temp_link = link->next_hash;
            ent = (struct stactrl_preferband_entry *)((unsigned long)link - offset);
            if(ent->used)
            {
                ent->aging++;
#if STA_CONTROL_ALGO == STA_CONTROL_ALGO3
				//20170106
				//reset kickoff
				// 1. STA in preferband, reset kickoff after 60s
				// 2. STA not connet to AP, reset kickoff after 30s
				unsigned long preferband_linketime = 0;
				unsigned char is_in_nonpreferband = 0 ;
				preferband_linketime = stactrl_sta_in_the_prefer_band(priv, ent->mac);
				is_in_nonpreferband = stactrl_sta_in_the_other_band(priv, ent->mac);				
				// check if in non prefer band and calc time
				if(is_in_nonpreferband)
					ent->sta_not_in_nonpreferband = 0;
				else{
					if(ent->kickoff>0)
						ent->sta_not_in_nonpreferband++;//sec
				}				
				// STA connect to prefer band for STACTRL_KICKOFF_REMOVE_WHEN_LINK sec
				if(preferband_linketime>=STACTRL_KICKOFF_REMOVE_WHEN_LINK && 
					ent->kickoff>0)
				{
					panic_printk("[STA_CONTROL][%s] remove %02x:%02x:%02x:%02x:%02x:%02x kickoff times, beacuse STA connect over %ds\n",
						priv->dev->name,
						ent->mac[0],ent->mac[1],ent->mac[2],
						ent->mac[3],ent->mac[4],ent->mac[5],
						STACTRL_KICKOFF_REMOVE_WHEN_LINK);
					ent->kickoff = 0;
					ent->sta_not_in_nonpreferband = 0;
				}
				// STA is leaving AP for STACTRL_KICKOFF_REMOVE sec
				if(preferband_linketime==0 &&
					ent->sta_not_in_nonpreferband>=STACTRL_KICKOFF_REMOVE && 
					ent->kickoff>0)
				{
					panic_printk("[STA_CONTROL][%s] remove %02x:%02x:%02x:%02x:%02x:%02x kickoff times, beacuse STA leave AP over %ds\n",
						priv->dev->name,
						ent->mac[0],ent->mac[1],ent->mac[2],
						ent->mac[3],ent->mac[4],ent->mac[5],
						STACTRL_KICKOFF_REMOVE);
					ent->kickoff = 0;
					ent->sta_not_in_nonpreferband = 0;
				}
				//update this client is local client
				if(preferband_linketime>0)
					ent->local = 1;
#else
                if(ent->aging > STACTRL_PREFERBAND_EXPIRE)
                {
                    ent->used = 0;
                    stactrl_mac_hash_unlink(link);
                }
                if(ent->blockRetry_resetTimer > 0)
				{
                    ent->blockRetry_resetTimer--;
                }
                else if(ent->blockRetry_resetTimer == 0)
				{
                    ent->blockRetry = STACTRL_BLOCK_RETRY_Y;
                    ent->blockRetry_resetTimer = -1;
                }
				if(ent->block_timer)
					ent->block_timer--;
#endif
            }
            link = temp_link;
        }
    }
    SMP_UNLOCK_STACONTROL_LIST(flags);
    RESTORE_INT(flags);

}

#if STA_CONTROL_ALGO == STA_CONTROL_ALGO3

int sta_support_dual_band(struct rtl8192cd_priv *priv, struct stat_info *pstat){
    struct rtl8192cd_priv *other_band = priv->stactrl.stactrl_priv_sc;
    struct stactrl_preferband_entry * prefer_ent;
    prefer_ent = stactrl_preferband_sta_lookup(other_band, pstat->cmn_info.mac_addr);
    if(prefer_ent ==NULL){
        return 0;
    }
    return 1;
}

int check_sta_linktime_and_tp(struct rtl8192cd_priv *priv, struct stat_info *pstat) {
    struct rtl8192cd_priv *other_band = priv->stactrl.stactrl_priv_sc;
    if (other_band->assoc_num >= NUM_STAT) {
        return 0;
    }
    if (pstat->link_time <= STACTRL_KICKOFF_TIME ) {
        return 0;
    }
    if ((pstat->tx_avarage + pstat->rx_avarage) >= STACTRL_KICKOFF_TP) {
        return 0;
    }

    return 1;
}

int check_kickoff_times(struct stat_info *pstat, struct rtl8192cd_priv *priv){
    if(pstat->kickoff_time > STACTRL_KICKOFF_TIMES){
        return 0;
    }
    return 1;
}
/* timer: on prefer band */ 
static void stactrl_preferband_expire_3_1(struct rtl8192cd_priv *priv)
{	
    int i;
    unsigned long offset;
    struct stactrl_link_list *link, *temp_link;
    struct stactrl_preferband_entry * ent, *ent1;
    unsigned long flags = 0;
	struct list_head	*phead, *plist;
	struct stat_info	*pstat;
	unsigned char kickoff_prefer = 0;	
	struct stactrl_block_entry *block_ent;
	unsigned long preferband_linketime = 0;
	unsigned char is_in_nonpreferband = 0 ;
    unsigned char sta_mac[16]; 

    SAVE_INT_AND_CLI(flags);
    SMP_LOCK_STACONTROL_LIST(flags);

    offset = (unsigned long)(&((struct stactrl_preferband_entry *)0)->link_list);

    for (i=0; i<STACTRL_PREFERBAND_HASH_SIZE; i++)
    {
        link = priv->stactrl.stactrl_preferband_machash[i];
        while (link != NULL)
        {
            temp_link = link->next_hash;
            ent = (struct stactrl_preferband_entry *)((unsigned long)link - offset);
            if(ent->used)
            {
                ent->aging++;
				//20170106
				// reset kickoff, to make STA can be kickoff on non-prefer band
				// 1. STA in preferband, reset kickoff after 60s
				// 2. STA not connet to AP, reset kickoff after 30s	
				preferband_linketime = 0;
				is_in_nonpreferband = 0 ;
				preferband_linketime = stactrl_sta_in_the_prefer_band(priv, ent->mac);
				is_in_nonpreferband = stactrl_sta_in_the_other_band(priv, ent->mac);
				
				// check if in non prefer band and calc time 
				if(is_in_nonpreferband){
					ent->sta_not_in_nonpreferband = 0;
					if(ent->kickoff>0)
						ent->sta_in_non_preferband++;
				}
				else{
					if(ent->kickoff>0)
						ent->sta_not_in_nonpreferband++;//sec
				}

				if( priv->up_time % DEBUG_TIME_INETRVAL==0 && preferband_linketime != 0)
					STACTRL_DEBUG(ent->mac, "preferband_linketime:%d,not_in_nonpreferband:%d,in_nonpreferband:%d,kickoff:%d",							
							preferband_linketime,
							ent->sta_not_in_nonpreferband,
							is_in_nonpreferband,
							ent->kickoff);
				
				// STA connect to prefer band for STACTRL_KICKOFF_REMOVE_WHEN_LINK sec
				if(preferband_linketime>=STACTRL_KICKOFF_REMOVE_WHEN_LINK && ent->kickoff>0)
				{
					STACTRL_DEBUG(ent->mac, "[%s]remove kickoff [%d] times, beacuse STA connect [prefer band] over %ds",
						priv->dev->name, ent->kickoff, STACTRL_KICKOFF_REMOVE_WHEN_LINK);
					ent->kickoff = 0;
					ent->sta_not_in_nonpreferband = 0;
				}
				
				// STA is leaving AP for STACTRL_KICKOFF_REMOVE sec
				if(preferband_linketime==0 &&
					ent->sta_not_in_nonpreferband>=STACTRL_KICKOFF_REMOVE && ent->kickoff>0)
				{
					STACTRL_DEBUG(ent->mac, "[%s]remove kickoff [%d] times, beacuse STA leave AP over %ds",
						priv->dev->name, ent->kickoff, STACTRL_KICKOFF_REMOVE);
					ent->kickoff = 0;					
					ent->sta_not_in_nonpreferband = 0;
				}	

				// STA connect to prefer-band, and kickoff is more than STACTRL_KICKOFF_TIMES
				if(ent->kickoff >= STACTRL_KICKOFF_TIMES && ent->sta_in_non_preferband >=STACTRL_KICKOFF_REMOVE_WHEN_LINK){
					STACTRL_DEBUG(ent->mac, "[%s]remove kickoff [%d] times, beacuse STA connect [non-prefer] band over %ds",
						priv->dev->name, ent->kickoff, STACTRL_KICKOFF_REMOVE_WHEN_LINK);
					ent->kickoff= 0;	
					ent->sta_in_non_preferband = 0;
				}
				
				//update this client is local client
				if(preferband_linketime>0)
					ent->local = 1;	
            }
            link = temp_link;
        }
    }
    SMP_UNLOCK_STACONTROL_LIST(flags);
    RESTORE_INT(flags);	

#if 1	
	// if block time more than STACTRL_PREFERBAND_EXPIRE_Z, then un-block		
    SAVE_INT_AND_CLI(flags);
    SMP_LOCK_STACONTROL_LIST(flags);

	offset = (unsigned long)(&((struct stactrl_block_entry *)0)->link_list);
    for (i=0; i<STACTRL_BLOCK_HASH_SIZE; i++)
    {
        link = priv->stactrl.stactrl_block_machash[i];
        while (link != NULL)
        {
            temp_link = link->next_hash;
            block_ent = (struct stactrl_block_entry *)((unsigned long)link - offset);
            block_ent->aging++;
			
            if(block_ent->aging > STACTRL_BLOCK_EXPIRE_Z)
            {
                block_ent->used = 0;
				stactrl_record_kickoff(priv, block_ent->mac);				   
                stactrl_mac_hash_unlink(link);				
                STACTRL_DEBUG(ent->mac, "[%s]block expire, aging:%d", priv->dev->name, block_ent->aging);
            }
            link = temp_link;
        }
    }

    RESTORE_INT(flags);
    SMP_UNLOCK_STACONTROL_LIST(flags);	   
#endif

	/*kickoff prefer band station(eg: sta go away), to force it to go through sta control process*/		
	phead = &priv->asoc_list;
	plist = phead;

	/*no need to block STAs if the number of associated STAs of the other band reaches the threshold*/
	if((priv->stactrl.stactrl_priv_sc->pmib->dot11StationConfigEntry.supportedStaNum > 0)
		&& (priv->stactrl.stactrl_priv_sc->assoc_num >= priv->stactrl.stactrl_priv_sc->pmib->dot11StationConfigEntry.supportedStaNum)){
		 return 0;
	}
	
	while ((plist = asoc_list_get_next(priv, plist)) != phead)
	{		
		kickoff_prefer= 0;
		pstat = list_entry(plist, struct stat_info, asoc_list);	
		if( priv->up_time % 5==0 )
			STACTRL_DEBUG(pstat->cmn_info.mac_addr, "[%s]rssi:%d triger time: %d, kickoff: %d tp :%d", priv->dev->name, pstat->rssi, pstat->stactrl_trigger_time, kickoff_prefer, pstat->tx_avarage + pstat->rx_avarage);
/*			
#ifdef CONFIG_IEEE80211V
		if(pstat->bssTransSupport == TRUE && pstat->bssTransRejectionCount < 5){
			STACTRL_DEBUG(pstat->cmn_info.mac_addr, "sta support 11v, no band steering.");
			continue;
		}
#endif
*/
#ifdef CONFIG_RTK_MESH
		if(!isMeshPoint(pstat) && pstat->rssi <= STACTRL_PREFERBAND_RSSI) 
#else
		if(pstat->rssi <= STACTRL_PREFERBAND_RSSI) 
#endif  
		{
			if(pstat->stactrl_trigger_time >= STACTRL_TRIGGER_TIME) {
                STACTRL_DEBUG(pstat->cmn_info.mac_addr, "check steering conditions.");
                if(sta_support_dual_band(priv, pstat) != 1){
                    STACTRL_DEBUG(pstat->cmn_info.mac_addr, "sta doesn't support dual band, can't steering.");
                    continue;
                }
#ifdef CONFIG_IEEE80211V
				if(pstat->bssTransSupport == TRUE && pstat->bssTransPktSent < 10){
					STACTRL_DEBUG(pstat->cmn_info.mac_addr, "sta support 11v, no band steering.");
#ifdef RTK_MULTI_AP
					set_11v_target_band(priv, pstat->cmn_info.mac_addr);
#endif
					pstat->bssTransPktSent++;
					memcpy(priv->bssTransPara.bssid_mac, priv->stactrl.stactrl_priv_sc->pmib->dot11StationConfigEntry.dot11Bssid, MACADDRLEN);
					priv->bssTransPara.channel = priv->stactrl.stactrl_priv_sc->pmib->dot11RFEntry.dot11channel;
#ifndef RTK_MULTI_AP
					priv->bssTransPara.FromUser = TRUE;
#endif
					if(issue_BSS_Trans_Req(priv, pstat->cmn_info.mac_addr, NULL, NULL, 0) == 0)
					{
						pstat->bssTransExpiredTime = 0;
						pstat->bssTransTriggered = TRUE;
						priv->startCounting = TRUE;

						DOT11VDEBUG("Send BSS Trans Req to STA [SUCCESS]:[%02x][%02x][%02x][%02x][%02x][%02x] \n", 
								pstat->cmn_info.mac_addr[0],
								pstat->cmn_info.mac_addr[1],
								pstat->cmn_info.mac_addr[2],
								pstat->cmn_info.mac_addr[3],
								pstat->cmn_info.mac_addr[4],
								pstat->cmn_info.mac_addr[5]);
					}
					else
					{
						DOT11VDEBUG("BSS Trans Req for bandsteering failed! \n");
					}
#ifdef RTK_MULTI_AP
					deleteEntryTransitionList(priv, pstat->cmn_info.mac_addr);
#else
					priv->bssTransPara.FromUser = FALSE;
#endif			
					continue;
				}
#endif
                STACTRL_DEBUG(pstat->cmn_info.mac_addr, "not support 11v or 11v times exceeded.");
				SAVE_INT_AND_CLI(flags);
	    		SMP_LOCK_STACONTROL_LIST(flags);	
			    ent1 = stactrl_preferband_sta_lookup(priv, pstat->cmn_info.mac_addr);		    
			    SMP_UNLOCK_STACONTROL_LIST(flags);
			    RESTORE_INT(flags);

				if(ent1==NULL)
					continue;
				
                if(check_sta_linktime_and_tp(priv, pstat) !=1 ){
                    STACTRL_DEBUG(pstat->cmn_info.mac_addr, "station's linktime or tp can't meet the threshold.");
                    //stactrl_block_delete(priv, pstat->cmn_info.mac_addr);
                    continue;
				} 
				
                kickoff_prefer = 1;

				
	            //check if this client is kickoff more than threshold times
                if(check_kickoff_times(pstat, priv) !=1 ){
                    STACTRL_DEBUG(pstat->cmn_info.mac_addr, "station's kickoff times exceed threshold[%d]", STACTRL_KICKOFF_TIMES);
                    continue;
				}	
				
	            if(kickoff_prefer){
						stactrl_hidden_ap(priv);
						
						STACTRL_DEBUG(pstat->cmn_info.mac_addr, "Deauth it on prefer band"); 

						SMP_LOCK_STACONTROL_LIST(flags);
		                block_ent = stactrl_block_add(priv, pstat->cmn_info.mac_addr);
		                if(block_ent)   /*add success*/
		                {
		                    STACTRL_DEBUG(pstat->cmn_info.mac_addr, "[%s]block add success, aging:%d", priv->dev->name, block_ent->aging);
		                }
		                else
		                {
		                    STACTRL_DEBUG(pstat->cmn_info.mac_addr, "[%s]block add fail", priv->dev->name);
		                }
		                SMP_UNLOCK_STACONTROL_LIST(flags);

						/* if DUT not recv deauth from STA, then the sta_info is not clear,
						    DUT will always tx deauth */
		               	// issue_deauth(priv,	pstat->cmn_info.mac_addr, _RSON_DISAOC_STA_LEAVING_);  //marked by tesia
		               	//del_sta(priv, pstat->cmn_info.mac_addr);
						issue_deauth(priv,	pstat->cmn_info.mac_addr, _RSON_DISAOC_STA_LEAVING_);
						pstat->kickoff_time++;
						pstat->prepare_to_free = 1;
					}		          	
			} else {
				pstat->stactrl_trigger_time++;
			}
		} else {
			pstat->stactrl_trigger_time = 0;
		}
		if (plist == plist->next) {
			asoc_list_unref(priv, pstat);
			break;
		}
    }
}

/* timer: on non-prefer band */ 
static void stactrl_non_prefer_expire(struct rtl8192cd_priv *priv)
{
	if(priv->stactrl.stactrl_hiddenAP_aging > 0){
		//printk("%s: hiddenAP_aging = %d\n", __FUNCTION__, priv->stactrl.stactrl_hiddenAP_aging);
		if(priv->stactrl.stactrl_hiddenAP_aging == 1){
			if(priv->pmib->dot11OperationEntry.hiddenAP){
				printk("%s: reset hiddenAP to recover ssid in Beacon\n", __FUNCTION__);
				priv->pmib->dot11OperationEntry.hiddenAP = 0;
			}
		}
		priv->stactrl.stactrl_hiddenAP_aging--;
	}

    int i;
    unsigned long offset;
    struct stactrl_link_list *link, *temp_link;
    struct stactrl_block_entry * ent;
    unsigned long flags = 0;
	struct stat_info	*pstat;
	struct list_head	*phead, *plist;
    unsigned char kickoff;

    offset = (unsigned long)(&((struct stactrl_block_entry *)0)->link_list);

    SAVE_INT_AND_CLI(flags);
    SMP_LOCK_STACONTROL_LIST(flags);

    for (i=0; i<STACTRL_BLOCK_HASH_SIZE; i++)
    {
        link = priv->stactrl.stactrl_block_machash[i];
        while (link != NULL)
        {
            temp_link = link->next_hash;
            ent = (struct stactrl_block_entry *)((unsigned long)link - offset);
            ent->aging++;
            if(ent->aging > STACTRL_BLOCK_EXPIRE)
			{
				ent->used = 0;
				//20170106
				stactrl_record_kickoff(priv, ent->mac);
				stactrl_mac_hash_unlink(link);
                STACTRL_DEBUG(ent->mac, "block expire");

            }

            link = temp_link;
        }
    }

    RESTORE_INT(flags);
    SMP_UNLOCK_STACONTROL_LIST(flags);


	/*kick out non-prefer band station, to force it to go through sta control process*/
	phead = &priv->asoc_list;
	plist = phead;
        
	while ((plist = asoc_list_get_next(priv, plist)) != phead)
	{
        kickoff= 0;
		pstat = list_entry(plist, struct stat_info, asoc_list);
#ifdef CONFIG_RTK_MESH
		if(!isMeshPoint(pstat) && pstat->rssi > STACTRL_NONPREFERBAND_RSSI) 
#else 
		if(pstat->rssi > STACTRL_NONPREFERBAND_RSSI) 
#endif 
		{
            if(pstat->stactrl_candidate == 1) {
                kickoff = 1;
			}

			if(kickoff == 0 && stactrl_is_kickoff(priv->stactrl.stactrl_priv_sc, pstat))					
				kickoff = 1;
            
            //20170103
			//check if this client is kickoff again
			if((stactrl_check_if_kickoff(priv, pstat->cmn_info.mac_addr) == 1) && (kickoff == 1)){
				kickoff = 0;
				//panic_printk("[STA_CONTROL] mac:%02x:%02x:%02x:%02x:%02x:%02x is kickoff %d times already, do not kickoff again !!\n",
						//pstat->cmn_info.mac_addr[0], pstat->cmn_info.mac_addr[1], pstat->cmn_info.mac_addr[2],
						//pstat->cmn_info.mac_addr[3], pstat->cmn_info.mac_addr[4], pstat->cmn_info.mac_addr[5],
						//STACTRL_KICKOFF_TIMES);
			}
            
			if(kickoff) 
			{ /*check if it is a dual-band client*/                        
		 		#ifdef RTK_SMART_ROAMING
				notify_hidden_ap(priv, STACTRL_HIDDEN_AP);
				#endif
				stactrl_hidden_ap(priv);
			
				STACTRL_DEBUG(pstat->cmn_info.mac_addr, "Deauth it on non-prefer band");
				SMP_LOCK_STACONTROL_LIST(flags);
				ent = stactrl_block_add(priv, pstat->cmn_info.mac_addr);
                if(ent)   /*add success*/
                {
                    STACTRL_DEBUG(pstat->cmn_info.mac_addr, "block add");
                }
                else
                {
                    STACTRL_DEBUG(pstat->cmn_info.mac_addr, "block add fail");
                }
                SMP_UNLOCK_STACONTROL_LIST(flags);

				//DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_SERIOUS, "issue_deauth - %02X:%02X:%02X:%02X:%02X:%02X, _RSON_DISAOC_STA_LEAVING_\n",
				//		pstat->hwaddr[0],pstat->hwaddr[1],pstat->hwaddr[2],pstat->hwaddr[3],pstat->hwaddr[4],pstat->hwaddr[5]);
                issue_deauth(priv,	pstat->cmn_info.mac_addr, _RSON_DISAOC_STA_LEAVING_);                               

				pstat->prepare_to_free = 1;

			}
		}

		if (plist == plist->next) {
			asoc_list_unref(priv, pstat);
			break;
		}
    }

}

static void stactrl_non_prefer_expire_3_1(struct rtl8192cd_priv *priv)
{
	if(priv->stactrl.stactrl_hiddenAP_aging > 0){
		//printk("%s: hiddenAP_aging = %d\n", __FUNCTION__, priv->stactrl.stactrl_hiddenAP_aging);
		if(priv->stactrl.stactrl_hiddenAP_aging == 1){
			if(priv->pmib->dot11OperationEntry.hiddenAP){
				printk("%s: reset hiddenAP to recover ssid in Beacon\n", __FUNCTION__);
				priv->pmib->dot11OperationEntry.hiddenAP = 0;
			}
		}
		priv->stactrl.stactrl_hiddenAP_aging--;
	}
	
    int i;
    unsigned long offset;
    struct stactrl_link_list *link, *temp_link;
    struct stactrl_block_entry * ent;
    unsigned long flags = 0;
	struct stat_info	*pstat;
	struct list_head	*phead, *plist;
    unsigned char kickoff;
	unsigned long nonpreferband_linketime = 0;
	unsigned char is_in_preferband = 0; 
	struct stactrl_preferband_entry *prefer_ent, *prefer_ent1;
	unsigned char sta_mac[16]; 

    offset = (unsigned long)(&((struct stactrl_preferband_entry *)0)->link_list);

    SAVE_INT_AND_CLI(flags);
    SMP_LOCK_STACONTROL_LIST(flags);

    for (i=0; i<STACTRL_PREFERBAND_HASH_SIZE; i++)
    {
        link = priv->stactrl.stactrl_preferband_machash[i];
        while (link != NULL)
        {
            temp_link = link->next_hash;
            prefer_ent = (struct stactrl_preferband_entry *)((unsigned long)link - offset);
            if(prefer_ent->used)
            {
                prefer_ent->aging++;				
				
				//reset kickoff
				// 1. STA in preferband, reset kickoff after 60s
				// 2. STA not connet to AP, reset kickoff after 30s
				nonpreferband_linketime = 0;
				is_in_preferband = 0;
				nonpreferband_linketime = stactrl_sta_in_the_prefer_band(priv, prefer_ent->mac);				
				is_in_preferband = stactrl_sta_in_the_other_band(priv, prefer_ent->mac);

				// check if in prefer band and calc time 
				if(is_in_preferband){
					prefer_ent->sta_not_in_preferband = 0;
					
					if(prefer_ent->kickoff>0)
						prefer_ent->sta_in_prefer_band++;
				}
				else
					if(prefer_ent->kickoff>0)
						prefer_ent->sta_not_in_preferband++;				

				if( priv->up_time % DEBUG_TIME_INETRVAL==0 && nonpreferband_linketime != 0 )
					STACTRL_DEBUG(prefer_ent->mac, "nonpreferband_linketime:%d,not_in_preferband:%d,in_prefer_band:%d,kickoff:%d",							
							nonpreferband_linketime,
							prefer_ent->sta_not_in_preferband,
							prefer_ent->sta_in_prefer_band,
							prefer_ent->kickoff);

				// STA connect to non-prefer band for STACTRL_KICKOFF_REMOVE_WHEN_LINK sec
				if(nonpreferband_linketime>=STACTRL_KICKOFF_REMOVE_WHEN_LINK && prefer_ent->kickoff>0)
				{									
					STACTRL_DEBUG(prefer_ent->mac, "[%s]remove kickoff [%d] times, beacuse STA connect [non-prefer band] over %ds",
							priv->dev->name,prefer_ent->kickoff,STACTRL_KICKOFF_REMOVE_WHEN_LINK);
					prefer_ent->kickoff= 0;
					prefer_ent->sta_not_in_preferband = 0; 
				}						

				// STA is leaving AP for STACTRL_KICKOFF_REMOVE sec
				if(nonpreferband_linketime==0 &&
					prefer_ent->sta_not_in_preferband>=STACTRL_KICKOFF_REMOVE && prefer_ent->kickoff>0)
				{
					STACTRL_DEBUG(prefer_ent->mac, "[%s]remove kickoff [%d] times, beacuse STA leave AP over %ds",
						priv->dev->name, prefer_ent->kickoff, STACTRL_KICKOFF_REMOVE);
					prefer_ent->kickoff= 0;					
					prefer_ent->sta_not_in_preferband = 0;
				}

				// STA connect to prefer-band, and kickoff is more than STACTRL_KICKOFF_TIMES
				if(prefer_ent->kickoff >= STACTRL_KICKOFF_TIMES && prefer_ent->sta_in_prefer_band >=STACTRL_KICKOFF_REMOVE_WHEN_LINK){
					STACTRL_DEBUG(prefer_ent->mac, "[%s]remove kickoff [%d] times, beacuse STA connect [prefer band] over %ds",
							priv->dev->name,prefer_ent->kickoff,STACTRL_KICKOFF_REMOVE_WHEN_LINK);
					prefer_ent->kickoff= 0;	
					prefer_ent->sta_in_prefer_band = 0;
				}

				//update this client is local client
				if(nonpreferband_linketime>0)
					prefer_ent->local = 1; 							
            }
            link = temp_link;
        }		
    }
    SMP_UNLOCK_STACONTROL_LIST(flags);
    RESTORE_INT(flags);		

	offset = (unsigned long)(&((struct stactrl_block_entry *)0)->link_list);

	SAVE_INT_AND_CLI(flags);
    SMP_LOCK_STACONTROL_LIST(flags);
    for (i=0; i<STACTRL_BLOCK_HASH_SIZE; i++)
    {
        link = priv->stactrl.stactrl_block_machash[i];
        while (link != NULL)
        {
            temp_link = link->next_hash;
            ent = (struct stactrl_block_entry *)((unsigned long)link - offset);
            ent->aging++;
            if(ent->aging > STACTRL_BLOCK_EXPIRE_Z)
            {
                ent->used = 0;
				//20170106
				stactrl_record_kickoff(priv, ent->mac);
                stactrl_mac_hash_unlink(link);
                STACTRL_DEBUG(ent->mac, "[%s]block expire", priv->dev->name);
            }

            link = temp_link;
        }
    }

    RESTORE_INT(flags);
    SMP_UNLOCK_STACONTROL_LIST(flags);	

	/*kickoff STA on non-prefer band. to force it to go through sta control process*/
	phead = &priv->asoc_list;
	plist = phead;

	/*no need to block STAs if the number of associated STAs of the other band reaches the threshold*/
	if((priv->stactrl.stactrl_priv_sc->pmib->dot11StationConfigEntry.supportedStaNum > 0)
		&& (priv->stactrl.stactrl_priv_sc->assoc_num >= priv->stactrl.stactrl_priv_sc->pmib->dot11StationConfigEntry.supportedStaNum)){
		 return 0;
	}

	while ((plist = asoc_list_get_next(priv, plist)) != phead)
	{
		kickoff= 0;
		pstat = list_entry(plist, struct stat_info, asoc_list);
		
		if(priv->up_time % 5==0) // just for debug
			STACTRL_DEBUG(pstat->cmn_info.mac_addr, "[%s] rssi:%d triger time:%d tp: %d", priv->dev->name, pstat->rssi, pstat->stactrl_trigger_time, pstat->tx_avarage + pstat->rx_avarage);
/*
#ifdef CONFIG_IEEE80211V
		if(pstat->bssTransSupport == TRUE && pstat->bssTransRejectionCount < 5){
			STACTRL_DEBUG(pstat->cmn_info.mac_addr, "sta support 11v, no band steering.");
			continue;
		}
#endif
*/
#ifdef CONFIG_RTK_MESH
		if(!isMeshPoint(pstat) && pstat->rssi > STACTRL_NONPREFERBAND_RSSI)
#else 
		if(pstat->rssi > STACTRL_NONPREFERBAND_RSSI) 
#endif 
		{
			if(pstat->stactrl_trigger_time >= STACTRL_TRIGGER_TIME) {
                STACTRL_DEBUG(pstat->cmn_info.mac_addr, "check steering conditions.");
                if(sta_support_dual_band(priv, pstat) != 1){
                    STACTRL_DEBUG(pstat->cmn_info.mac_addr, "sta doesn't support dual band, can't steering.");
                    continue;
                }
#ifdef CONFIG_IEEE80211V
				if(pstat->bssTransSupport == TRUE && pstat->bssTransPktSent < 10){
					STACTRL_DEBUG(pstat->cmn_info.mac_addr, "sta support 11v, no band steering.");
#ifdef RTK_MULTI_AP
					set_11v_target_band(priv, pstat->cmn_info.mac_addr);
#endif
					pstat->bssTransPktSent++;
					memcpy(priv->bssTransPara.bssid_mac, priv->stactrl.stactrl_priv_sc->pmib->dot11StationConfigEntry.dot11Bssid, MACADDRLEN);
					priv->bssTransPara.channel = priv->stactrl.stactrl_priv_sc->pmib->dot11RFEntry.dot11channel;
#ifndef RTK_MULTI_AP
					priv->bssTransPara.FromUser = TRUE;
#endif
					if(issue_BSS_Trans_Req(priv, pstat->cmn_info.mac_addr, NULL, NULL, 0) == SUCCESS)
					{
						pstat->bssTransExpiredTime = 0;
						pstat->bssTransTriggered = TRUE;
						priv->startCounting = TRUE;

								DOT11VDEBUG("Send BSS Trans Req to STA [SUCCESS]:[%02x][%02x][%02x][%02x][%02x][%02x] \n", 
										pstat->cmn_info.mac_addr[0],
										pstat->cmn_info.mac_addr[1],
										pstat->cmn_info.mac_addr[2],
										pstat->cmn_info.mac_addr[3],
										pstat->cmn_info.mac_addr[4],
										pstat->cmn_info.mac_addr[5]);
					}
					else
					{
						DOT11VDEBUG("BSS Trans Req for bandsteering failed! \n");
					}
#ifndef RTK_MULTI_AP
					priv->bssTransPara.FromUser = FALSE;
#else
					deleteEntryTransitionList(priv, pstat->cmn_info.mac_addr);
#endif
					continue;
				}
#endif
                STACTRL_DEBUG(pstat->cmn_info.mac_addr, "not support 11v or 11v times exceeded.");
				//not support 11v or 11v request too many 
				if(check_sta_linktime_and_tp(priv, pstat) !=1 ){
                    STACTRL_DEBUG(pstat->cmn_info.mac_addr, "station's linktime or tp can't meet the threshold.");
                    //stactrl_block_delete(priv, pstat->cmn_info.mac_addr);
                    continue;
                }

					kickoff = 1;
				
                
				//check if this client is kickoff more than threshold times
                if(check_kickoff_times(pstat, priv) !=1 ){
                    STACTRL_DEBUG(pstat->cmn_info.mac_addr, "station's kickoff times exceed threshold[%d]", STACTRL_KICKOFF_TIMES);
                    continue;
				}
				
				if(kickoff) 
				{ /*check if it is a dual-band client*/
							#ifdef RTK_SMART_ROAMING
							notify_hidden_ap(priv, STACTRL_HIDDEN_AP);
							#endif
							stactrl_hidden_ap(priv);
							STACTRL_DEBUG(pstat->cmn_info.mac_addr, "Deauth it on non-prefer band");
							SMP_LOCK_STACONTROL_LIST(flags);
							ent = stactrl_block_add(priv, pstat->cmn_info.mac_addr);
							if(ent)   /*add success*/
							{
								STACTRL_DEBUG(pstat->cmn_info.mac_addr, "block add success");
							}
							else
							{
								STACTRL_DEBUG(pstat->cmn_info.mac_addr, "block add fail");
							}
							SMP_UNLOCK_STACONTROL_LIST(flags);					

							//issue_deauth(priv,	pstat->cmn_info.mac_addr, _RSON_DISAOC_STA_LEAVING_); //marked by tesia 
							//del_sta(priv, pstat->cmn_info.mac_addr);
							issue_deauth(priv, pstat->cmn_info.mac_addr, _RSON_DISAOC_STA_LEAVING_);
                            pstat->kickoff_time++;
							pstat->prepare_to_free = 1;
				}
			} else {
				pstat->stactrl_trigger_time++;
			}
		} else {
			pstat->stactrl_trigger_time = 0;
		}

		if (plist == plist->next) {
			asoc_list_unref(priv, pstat);
			break;
		}
    }
}

void stactrl_expire(struct rtl8192cd_priv *priv)
{
    if(priv->stactrl.stactrl_prefer){
		if(!STACTRL_BLOCK_5G)
        	stactrl_preferband_expire(priv);
		else
			stactrl_preferband_expire_3_1(priv);
    }
    else{
		if(!STACTRL_BLOCK_5G)
			stactrl_non_prefer_expire(priv);
		else
			stactrl_non_prefer_expire_3_1(priv);
    }
}

#else
void stactrl_non_prefer_expire(unsigned long task_priv)
{
    struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
    int i;
    unsigned long offset;
    struct stactrl_link_list *link, *temp_link;
    struct stactrl_block_entry * ent;
    unsigned long flags = 0;

    offset = (unsigned long)(&((struct stactrl_block_entry *)0)->link_list);

    SAVE_INT_AND_CLI(flags);
    SMP_LOCK_STACONTROL_LIST(flags);

    for (i=0; i<STACTRL_BLOCK_HASH_SIZE; i++)
    {
        link = priv->stactrl.stactrl_block_machash[i];
        while (link != NULL)
        {
            temp_link = link->next_hash;
            ent = (struct stactrl_block_entry *)((unsigned long)link - offset);
            if(ent->timerZ)
            {
                ent->timerZ--;
                if(ent->timerZ == 0)
                {
                    ent->used = 0;
                    stactrl_mac_hash_unlink(link);
                    STACTRL_DEBUG(ent->mac, "block del");
                }
            }
            else if(ent->timerX)
            {
                ent->timerX--;

                if(ent->timerX == 0)
                {
                    ent->timerZ = STACTRL_BLOCK_EXPIRE_Z;
                    STACTRL_DEBUG(ent->mac, "block Z stage");
                }
            }

            link = temp_link;
        }
    }

    RESTORE_INT(flags);
    SMP_UNLOCK_STACONTROL_LIST(flags);

#if STA_CONTROL_ALGO == STA_CONTROL_ALGO2
    mod_timer(&priv->stactrl.stactrl_timer, jiffies + RTL_MILISECONDS_TO_JIFFIES(STACTRL_NON_PREFER_TIMER));
#endif
}


void stactrl_expire(struct rtl8192cd_priv *priv)
{
	struct stat_info	*pstat;
	struct list_head	*phead, *plist;

    if(priv->stactrl.stactrl_prefer)
    {
        stactrl_preferband_expire(priv);
    }

    if(priv->stactrl.stactrl_prefer == 0)
    {
#if STA_CONTROL_ALGO == STA_CONTROL_ALGO1
        stactrl_non_prefer_expire((unsigned long)priv);
#endif
        /*kick out non-prefer band station, to force it to go through sta control process*/
        phead = &priv->asoc_list;
        plist = phead;
        
        while ((plist = asoc_list_get_next(priv, plist)) != phead)
        {
            pstat = list_entry(plist, struct stat_info, asoc_list);
            if(pstat->stactrl_candidate) {
                if(pstat->rssi > STACTRL_NONPREFERBAND_RSSI) {
                    if(stactrl_is_need(priv->stactrl.stactrl_priv_sc, pstat->cmn_info.mac_addr, 0)) { /*check if it is a dual-band client*/                        
                        STACTRL_DEBUG(pstat->cmn_info.mac_addr, "Dissasociate it on non-prefer band");
                        unsigned char sta_mac[16];      
                        sprintf(sta_mac,"%02X%02X%02X%02X%02X%02X",
							pstat->cmn_info.mac_addr[0],pstat->cmn_info.mac_addr[1],
							pstat->cmn_info.mac_addr[2],pstat->cmn_info.mac_addr[3],
							pstat->cmn_info.mac_addr[4],pstat->cmn_info.mac_addr[5]);                             

						DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_SERIOUS, "del_sta - %02X:%02X:%02X:%02X:%02X:%02X\n",
							pstat->cmn_info.mac_addr[0],pstat->cmn_info.mac_addr[1],
							pstat->cmn_info.mac_addr[2],pstat->cmn_info.mac_addr[3],
							pstat->cmn_info.mac_addr[4],pstat->cmn_info.mac_addr[5]);
						
                        del_sta(priv, sta_mac);
                    }
                }
            }

    		if (plist == plist->next) {
				asoc_list_unref(priv, pstat);
    			break;
    		}

        }
    }
}
#endif

#if STA_CONTROL_ALGO == STA_CONTROL_ALGO1 || STA_CONTROL_ALGO == STA_CONTROL_ALGO2
#if STA_CONTROL_ALGO == STA_CONTROL_ALGO1
/*return: 0: continu process(no stactrl), 1: ignore, 2: error code, 3: continue process(have done stactrl process)*/
unsigned char stactrl_check_request(struct rtl8192cd_priv *priv, unsigned char *mac, int frame_type, unsigned char rssi)
{
    struct stactrl_block_entry * block_ent;
    unsigned char ret = 0;
    unsigned long flags = 0;
    SAVE_INT_AND_CLI(flags);
    SMP_LOCK_STACONTROL_LIST(flags);
    block_ent =  stactrl_block_lookup(priv, mac);
    if(block_ent)
    {
        ret = 3;
        if(block_ent->timerX && block_ent->retryY)
        {
            if(frame_type == WIFI_PROBEREQ)
            {
                ret = 1;
            }
            else if(frame_type == WIFI_ASSOCREQ)
            {
                ret = 2;
                block_ent->retryY--;

                if(block_ent->retryY == 0)
                {
                    block_ent->timerZ = STACTRL_BLOCK_EXPIRE_Z;
                    STACTRL_DEBUG(block_ent->mac, "block Z stage");
                }
            }
        }
        SMP_UNLOCK_STACONTROL_LIST(flags);
    }
    else
    {
        SMP_UNLOCK_STACONTROL_LIST(flags);
        if(stactrl_is_need(priv->stactrl.stactrl_priv_sc, mac, 1))
        {
            ret = 1;
            SMP_LOCK_STACONTROL_LIST(flags);
            block_ent = stactrl_block_add(priv, mac);
            if(block_ent)   /*add success*/
            {
                STACTRL_DEBUG(mac, "block add");
            }
            else
            {
                STACTRL_DEBUG(mac, "block add fail");
            }
            SMP_UNLOCK_STACONTROL_LIST(flags);
        }
    }
    RESTORE_INT(flags);

	STACTRL_DEBUG(mac, "[%s] receive %s drop: %d rssi %d", priv->dev->name,
			(frame_type == WIFI_PROBEREQ ? "probe" : (frame_type == WIFI_AUTH ? "auth" : "assoc")), ret, rssi);

    return ret;
}
#elif STA_CONTROL_ALGO == STA_CONTROL_ALGO2
/*return: 0: continu process(no stactrl), 1: ignore, 2: error code, 3: continue process(have done stactrl process)*/
unsigned char stactrl_check_request(struct rtl8192cd_priv *priv, unsigned char *mac, int frame_type, unsigned char rssi)
{
    struct stactrl_block_entry * block_ent;
    unsigned char ret = 0;

    unsigned long flags = 0;
    SAVE_INT_AND_CLI(flags);
    SMP_LOCK_STACONTROL_LIST(flags);

    block_ent =  stactrl_block_lookup(priv, mac);
    if(block_ent)
    {
        ret = 3;
        if(block_ent->timerZ)
        {
            if(frame_type == WIFI_ASSOCREQ)
            {
                if(block_ent->retryY)
                {
                    block_ent->retryY--;
                    ret = 2;
                }
            }
        }
        else
        {
            ret = 1;
        }
    }
    else
    {
        if(stactrl_is_need(priv->stactrl.stactrl_priv_sc, mac, 1))
        {
            ret = 1;
            if(frame_type == WIFI_AUTH || frame_type == WIFI_ASSOCREQ)
            {
                block_ent = stactrl_block_add(priv, mac);
                if(block_ent)   /*add success*/
                {
                    STACTRL_DEBUG(mac, "block add");
                }
                else
                {
                    STACTRL_DEBUG(mac, "block add fail");
                }
            }
        }
    }
    SMP_UNLOCK_STACONTROL_LIST(flags);
    RESTORE_INT(flags);

    STACTRL_DEBUG(mac, "receive %s drop: %d rssi: %d", (frame_type == WIFI_PROBEREQ? "probe": (frame_type == WIFI_AUTH? "auth": "assoc")),ret, rssi);

    return ret;
}
#endif
unsigned char stactrl_pass_request(struct rtl8192cd_priv *priv, unsigned char *mac, int frame_type)
{
	struct stactrl_preferband_entry *prefer_ent;
	int ret = 0;
	unsigned long flags = 0;
	SAVE_INT_AND_CLI(flags);
    SMP_LOCK_STACONTROL_LIST(flags);
	prefer_ent = stactrl_preferband_sta_lookup(priv, mac);
	if(prefer_ent && prefer_ent->block_timer) ret = 1;
	SMP_UNLOCK_STACONTROL_LIST(flags);
    RESTORE_INT(flags);
	/*
	if(ret && (frame_type == WIFI_ASSOCREQ || frame_type == WIFI_AUTH))
		printk("Pass [%02x%02x%02x%02x%02x%02x] frame_type = %d\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], frame_type);
	*/
	return ret;
}

unsigned char stactrl_check_request_on_preferband(struct rtl8192cd_priv *priv, unsigned char *mac, int frame_type, unsigned char rssi)
{
    struct stactrl_block_entry *block_ent;
	struct stactrl_preferband_entry *prefer_ent;
    int ret = 0;
	int achieve = 0;
    unsigned long flags = 0;
	int blockRetry_resetTimer = 30;

    SAVE_INT_AND_CLI(flags);
    SMP_LOCK_STACONTROL_LIST(flags);

    block_ent = stactrl_block_lookup(priv->stactrl.stactrl_priv_sc, mac);
    prefer_ent = stactrl_preferband_sta_lookup(priv, mac);

    if(block_ent == NULL && prefer_ent && rssi < STACTRL_PREFERBAND_BLOCK_RSSI) {
        achieve = 1;
    }
#ifdef STA_CONTROL_BAND_TRANSITION
    else if(priv->pmib->staControl.stactrl_load_balance && prefer_ent && prefer_ent->block_timer) {
        blockRetry_resetTimer = priv->pmib->staControl.stactrl_load_balance;
        achieve = 2;
    }
    else if(priv->pmib->staControl.stactrl_load_balance && prefer_ent) {
        struct rtl8192cd_priv *priv_sc = priv->stactrl.stactrl_priv_sc;
        int priv_score = priv->load_balance_score;
        int priv_sc_score = priv_sc->load_balance_score;

        if(priv_score > priv_sc_score + priv->pmib->staControl.stactrl_sta_num_weight &&
            priv_score + priv->pmib->staControl.stactrl_sta_num_weight > priv->pmib->staControl.stactrl_score_upper) {
            blockRetry_resetTimer = priv->pmib->staControl.stactrl_load_balance;
            prefer_ent->block_timer = priv->pmib->staControl.stactrl_load_balance;
            achieve = 3;
        }
    }
	/*
    if(achieve > 1) {
        printk("Check [%02x%02x%02x%02x%02x%02x] achieve = %d\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], achieve);
        printk("prefer_ent->block_timer = %d, frame_type = %d\n", prefer_ent->block_timer, frame_type);
    }
	*/
#endif

    if(prefer_ent->blockRetry && achieve) {
        if(frame_type == WIFI_PROBEREQ) {
            ret = 1;
        }
        else if(frame_type == WIFI_ASSOCREQ) {
            if(prefer_ent->blockRetry_resetTimer == -1) {
                prefer_ent->blockRetry_resetTimer = blockRetry_resetTimer;
            }
            ret = 2;
            prefer_ent->blockRetry--;
        }
    }

    SMP_UNLOCK_STACONTROL_LIST(flags);
    RESTORE_INT(flags);

    STACTRL_DEBUG(mac, "[%s] receives %s (rssi %d, ret %d)", priv->dev->name,
                (frame_type == WIFI_PROBEREQ ? "probe" : (frame_type == WIFI_AUTH ? "auth" : "assoc")), rssi, ret);

    return ret;
}
#elif STA_CONTROL_ALGO == STA_CONTROL_ALGO3
unsigned char stactrl_check_request(struct rtl8192cd_priv *priv, unsigned char *mac, int frame_type, unsigned char rssi)
{
	struct stactrl_block_entry * block_ent;
	struct stactrl_preferband_entry *prefer_ent;
	unsigned char ret = 0;

	unsigned char delete_blockent = 0;

	if(priv->stactrl.stactrl_prefer){
		prefer_ent = stactrl_is_dualband(priv,mac);
		if(prefer_ent){
			prefer_ent->retry = STACTRL_BLOCK_RETRY_Y;
		}
		goto ret;
	}
	
	prefer_ent = stactrl_is_dualband(priv->stactrl.stactrl_priv_sc, mac);
	if(prefer_ent){
				
//20170321, associated at prefer band with a prefer band rssi
		if(stactrl_sta_in_the_other_band(priv, mac) > STACTRL_PREFERBAND_RSSI){
			STACTRL_DEBUG(mac, "sta in the preferband with good rssi");
			ret = 1;
		}
//20170321, initial connect with non-prefer band rssi
		else if (rssi > STACTRL_NONPREFERBAND_RSSI){
			if(frame_type == WIFI_PROBEREQ)
			{
				ret = 1;
			}
			else if(frame_type == WIFI_ASSOCREQ)
			{
				STACTRL_DEBUG(mac, "retry = %d", prefer_ent->retry);
				if(prefer_ent->retry > 0){
					ret = 1;
					prefer_ent->retry--;
				}
				else{
					//connect to non-preferband and reset the retry value
					STACTRL_DEBUG(mac, "retry counter = 0, stop blocking.");
					prefer_ent->retry = STACTRL_BLOCK_RETRY_Y;
				}
			}
		}
		else{
			block_ent =  stactrl_block_lookup(priv, mac);
			if(block_ent)
			{
				if(frame_type == WIFI_PROBEREQ)
				{
					ret = 1;
				}
				else if(frame_type == WIFI_ASSOCREQ)
				{
					if(block_ent->retryY) { 			   
						ret = 1;
						block_ent->retryY--;
						if(block_ent->retryY == 0)
						{
								delete_blockent = 1;
						}
					}
				}
			}
		}
	

	
		if(delete_blockent){
			panic_printk("%s: call stactrl_block_delete\n",__FUNCTION__);
			if(stactrl_block_delete(priv, mac) == 0){
				//20170103 
				//record STA is kickoff and on the other band's prefer table
				stactrl_record_kickoff(priv->stactrl.stactrl_priv_sc, mac);
			}
		}
	}
ret:
	STACTRL_DEBUG(mac, "receive %s drop: %d rssi %d", (frame_type == WIFI_PROBEREQ? "probe": (frame_type == WIFI_AUTH? "auth": "assoc")),ret, rssi);
	
	return ret;
}

/*return: 0: continu process(no stactrl), 1: block*/
/* in processing OnProbeReq or (onAuth ?) or OnAssocReq */
unsigned char stactrl_check_request_on_nonpreferband(struct rtl8192cd_priv *priv, unsigned char *mac, int frame_type, unsigned char rssi)
{
    struct stactrl_block_entry *block_prefer_ent, *block_nonprefer;
	struct stactrl_preferband_entry *prefer_ent, *own_ent;
    unsigned char ret = 0;
    
    unsigned char delete_blockent = 0;	
	unsigned char block_prefer = 0;

	prefer_ent = stactrl_is_dualband(priv->stactrl.stactrl_priv_sc, mac);
	own_ent = stactrl_is_dualband(priv,mac);
	if(prefer_ent){
		/*no need to block STAs if the number of associated STAs of the other band reaches the threshold*/
		if((priv->stactrl.stactrl_priv_sc->pmib->dot11StationConfigEntry.supportedStaNum > 0)
			&& (priv->stactrl.stactrl_priv_sc->assoc_num >= priv->stactrl.stactrl_priv_sc->pmib->dot11StationConfigEntry.supportedStaNum)){
		 	return 0;
		}
	    //SAVE_INT_AND_CLI(flags);
	    //SMP_LOCK_STACONTROL_LIST(flags);		

// if block prefer-band
		block_prefer_ent =  stactrl_block_lookup(priv->stactrl.stactrl_priv_sc, mac);
		if(block_prefer_ent){
			ret = 0;
			STACTRL_DEBUG(mac, "[%s]Sta in prefer-band block entry, Block prefer band !!!",(frame_type == WIFI_PROBEREQ? "probe": (frame_type == WIFI_AUTH? "auth": "assoc")));
			block_prefer= 1;
			if(frame_type == WIFI_AUTH || frame_type == WIFI_ASSOCREQ){
				if(own_ent){
					
					own_ent->retry = STACTRL_BLOCK_RETRY_Y;
					
				}
			}
		}else
			block_prefer = 0;

		//SMP_UNLOCK_STACONTROL_LIST(flags);
	    //RESTORE_INT(flags);
		
//20170321, associated at prefer band with a prefer band rssi	
		if(!block_prefer){
			if((stactrl_sta_in_the_other_band(priv, mac) > STACTRL_PREFERBAND_RSSI) && (frame_type != WIFI_AUTH)){
				ret = 1;
				//panic_printk("[%s %d]associated at prefer band.\n", __FUNCTION__,__LINE__);
			}
//20170321, initial connect with non-prefer band rssi
			else if (rssi > STACTRL_NONPREFERBAND_RSSI){ // 35
				//SAVE_INT_AND_CLI(flags);
		    	//SMP_LOCK_STACONTROL_LIST(flags);
				if(frame_type == WIFI_PROBEREQ)
				{
					ret = 1;					
				}
				else if(frame_type == WIFI_ASSOCREQ)
				{
					STACTRL_DEBUG(mac,"retry = %d", prefer_ent->retry);
					if(prefer_ent->retry > 0){
						ret = 1;
						prefer_ent->retry--;		
					}
					else{
						//connect to non-preferband and reset the retry value
						STACTRL_DEBUG(mac, "retry counter = 0, stop blocking.");
						prefer_ent->retry = STACTRL_BLOCK_RETRY_Y;
					}
				}
				//SMP_UNLOCK_STACONTROL_LIST(flags);
			    //RESTORE_INT(flags);
			}
// initial connect with small rssi
			else{
				//SAVE_INT_AND_CLI(flags);
		    	//SMP_LOCK_STACONTROL_LIST(flags);
				
				block_nonprefer =  stactrl_block_lookup(priv, mac);
				if(block_nonprefer)
				{
		            if(frame_type == WIFI_PROBEREQ)
		            {
		                ret = 1;					
		            }
		            else if(frame_type == WIFI_ASSOCREQ)
		            {
			            if(block_nonprefer->retryY) {                
			                ret = 1;						
			                block_nonprefer->retryY--;
			                if(block_nonprefer->retryY == 0)
			                {
			                        delete_blockent = 1;
			                }
						}
					}
				}else{
					// reset the retry counter of 5G when 2G is better choice
					if(frame_type == WIFI_AUTH || frame_type == WIFI_ASSOCREQ){
						if(own_ent){
							
							own_ent->retry = STACTRL_BLOCK_RETRY_Y;
							
						}
					}
					if(frame_type == WIFI_ASSOCREQ)
						STACTRL_DEBUG(mac, "RSSI is low and connect to 2G.");
				}
				//SMP_UNLOCK_STACONTROL_LIST(flags);
			    //RESTORE_INT(flags);
			}
		}			
		
		if(delete_blockent){
			STACTRL_DEBUG(mac, "[%s]Block del bcz STA try connect more than [%d] times. No block the STA again!",
				priv->dev->name);			
			if(stactrl_block_delete(priv, mac) == 0){
				//20170103 
		    	//record STA is kickoff and on the other band's prefer table
		    	stactrl_record_kickoff(priv, mac);
	        }
	    }
	}
	/*else{		
		STACTRL_DEBUG(mac, "single band.");
	}*/

    //STACTRL_DEBUG(mac, "receive %s drop: %d rssi %d", (frame_type == WIFI_PROBEREQ? "probe": (frame_type == WIFI_AUTH? "auth": "assoc")),ret, rssi);
    return ret;
}

unsigned char stactrl_check_request_on_preferband(struct rtl8192cd_priv *priv, unsigned char *mac, int frame_type, unsigned char rssi)
{ 	
	struct stactrl_block_entry * block_ent;
    unsigned char ret = 0;
    unsigned char delete_blockent = 0;	
	struct stactrl_preferband_entry *prefer_ent;

	prefer_ent = stactrl_is_dualband(priv->stactrl.stactrl_priv_sc, mac);
	if(prefer_ent){
		/*no need to block STAs if the number of associated STAs of the other band reaches the threshold*/
		if((priv->stactrl.stactrl_priv_sc->pmib->dot11StationConfigEntry.supportedStaNum > 0)
			&& (priv->stactrl.stactrl_priv_sc->assoc_num >= priv->stactrl.stactrl_priv_sc->pmib->dot11StationConfigEntry.supportedStaNum)){
		 	return 0;
		}
	    

// if block prefer-band
		block_ent =  stactrl_block_lookup(priv, mac);
		if(block_ent){
			//STACTRL_DEBUG(mac, "in block list");
			/* if sta in prefer band block list, we should not always block prefer band */
			if(frame_type == WIFI_PROBEREQ)
            {
                ret = 1;					
            }
		    else if(frame_type == WIFI_ASSOCREQ)
            {
	            if(block_ent->retryY) {                
	                ret = 1;						
	                block_ent->retryY--;
	                if(block_ent->retryY == 0)
	                {	   
	                	STACTRL_DEBUG(mac, "Block del bcz STA try connect more than [%d] times. No block the STA again!", STACTRL_BLOCK_RETRY_Y);
						panic_printk("[%s %d]call stactrl_block_delete\n", __FUNCTION__,__LINE__);
						if(stactrl_block_delete(priv, mac) == 0)				
		    				stactrl_record_kickoff(priv, mac);	        			
	                }
				}
			}					
			//panic_printk("[%s %d]block prefer band\n", __FUNCTION__,__LINE__);
		}
		else{
			//STACTRL_DEBUG(mac, "not in block list");
			if((stactrl_sta_in_the_other_band(priv, mac) < STACTRL_NONPREFERBAND_RSSI) &&
				(stactrl_sta_in_the_other_band(priv, mac) > 0) &&
				(frame_type != WIFI_AUTH)){
				STACTRL_DEBUG(mac, "sta associates in 2G with low RSSI.");
				ret = 1;
			}
			else if (rssi < STACTRL_PREFERBAND_RSSI - STACTRL_PREFERBAND_RSSI_TOLERANCE){
				if(frame_type == WIFI_PROBEREQ)
				{
					ret = 1;
				}
				else if(frame_type == WIFI_ASSOCREQ)
				{
					STACTRL_DEBUG(mac, "retry = %d", prefer_ent->retry);
					if(prefer_ent->retry > 0){
						ret = 1;
						prefer_ent->retry--;
					}
					else{
						//connect to preferband and reset the retry value
						STACTRL_DEBUG(mac, "retry counter = 0, stop blocking.");
						prefer_ent->retry = STACTRL_BLOCK_RETRY_Y;
					}
				}
			}
			else{
				// reset the retry counter of 1G when 5G is better choice
				if(frame_type == WIFI_AUTH || frame_type == WIFI_ASSOCREQ){
					prefer_ent = stactrl_is_dualband(priv,mac);
					if(prefer_ent){
						prefer_ent->retry = STACTRL_BLOCK_RETRY_Y;
					}
				}
				if(frame_type == WIFI_ASSOCREQ)
					STACTRL_DEBUG(mac, "RSSI is high and connect to 5G.");
			}
		}
			
	}
	/*else{
		STACTRL_DEBUG(mac, "single band.");
	}*/

    //STACTRL_DEBUG(mac, "receive %s drop: %d rssi %d", (frame_type == WIFI_PROBEREQ? "probe": (frame_type == WIFI_AUTH? "auth": "assoc")),ret, rssi);
    return ret;
}

/* return 0: success, 1: reject*/
unsigned char stactrl_OnAssocReq(struct rtl8192cd_priv *priv, unsigned char *mac, int frame_type, unsigned char rssi, unsigned char * candidate) {
    unsigned short val16 = 0;
	unsigned long flags = 0;
	unsigned char ret = 0;
	struct stactrl_block_entry *block_ent_prefer = NULL;
	SAVE_INT_AND_CLI(flags);
	SMP_LOCK_STACONTROL_LIST(flags);
	if(priv->stactrl.stactrl_block5G){
		if(priv->stactrl.stactrl_prefer){
		/* check if block prefer band*/
		
		
		block_ent_prefer = stactrl_block_lookup(priv, mac);
		if(block_ent_prefer){
			val16 = stactrl_check_request_on_preferband(priv, mac, WIFI_ASSOCREQ, rssi);
			if(val16){	
				STACTRL_DEBUG(mac, "recv assreq, block prefer");	
				ret = 1;
				goto ret;
			}else{
				STACTRL_DEBUG(mac, "recv assreq, direct connect");
			}
		}else{
			//STACTRL_DEBUG(mac, "prefer-band recv assreq, and not block prefer band. Connect...");
			//STACTRL_DEBUG(mac, "block del");
			//record STA is kickoff and on the other band's prefer table
			if(stactrl_block_delete(priv->stactrl.stactrl_priv_sc, mac)==0){				
				stactrl_record_kickoff(priv->stactrl.stactrl_priv_sc, mac);	
				STACTRL_DEBUG(mac, "2G->5G");
			}
			//STA not in either block list of 5G or block list of 2G
			else{
				val16 = stactrl_check_request_on_preferband(priv, mac, WIFI_ASSOCREQ, rssi);
				if(val16){	
					STACTRL_DEBUG(mac, "recv assreq, block prefer");				
					ret = 1;
	    			goto ret;
				}else{
					STACTRL_DEBUG(mac, "recv assreq, direct connect");
				}
			}
		}

				
		}else {
			/* check if block prefer band */
			panic_printk("[%s %d]call stactrl_block_delete\n", __FUNCTION__,__LINE__);
			if(stactrl_block_delete(priv->stactrl.stactrl_priv_sc, mac)==0){
				STACTRL_DEBUG(mac, "5G->2G, recv assreq, block prefer, connect non-prefer");					
				stactrl_record_kickoff(priv->stactrl.stactrl_priv_sc, mac);
			}
			/* block non-prefer band or non block 2G/5G, prefer band first  */
			else{
				STACTRL_DEBUG(mac, "check request when Asso()");
				SMP_UNLOCK_STACONTROL_LIST(flags);
	    		RESTORE_INT(flags);	
				val16 = stactrl_check_request_on_nonpreferband(priv, mac, WIFI_ASSOCREQ, rssi);
				SAVE_INT_AND_CLI(flags);
				SMP_LOCK_STACONTROL_LIST(flags);
		         if(val16 == 1){ /*reject*/
					STACTRL_DEBUG(mac, "recv assreq, block 2G");
		            ret = 1;
	    			goto ret;
		         }
		         if(rssi < STACTRL_NONPREFERBAND_RSSI - STACTRL_PREFERBAND_RSSI_TOLERANCE){
				 	*candidate = 1;
					STACTRL_DEBUG(mac, "recv assreq, candidate");
				 }
			}
		}
		ret = 0;
		goto ret;
	}
	else{
	    if(priv->stactrl.stactrl_prefer) {
			//panic_printk("%s: call stactrl_block_delete\n",__FUNCTION__);
	        if(stactrl_block_delete(priv->stactrl.stactrl_priv_sc, mac) == 0){
				//20170103 
				//record STA is kickoff and on the other band's prefer table
				stactrl_record_kickoff(priv->stactrl.stactrl_priv_sc, mac);
			}
			val16 = stactrl_check_request(priv, mac, WIFI_ASSOCREQ, rssi);
	        if(val16 == 1){ /*reject*/
	        	ret = 1;
	    		goto ret;
	        	}
	    }else {
	         val16 = stactrl_check_request(priv, mac, WIFI_ASSOCREQ, rssi);
	         if(val16 == 1){ /*reject*/
	            ret = 1;
	    		goto ret; 
	            }
	         if(rssi < STACTRL_NONPREFERBAND_RSSI - STACTRL_PREFERBAND_RSSI_TOLERANCE)
	             *candidate = 1;
	    }
	    ret = 0;
	    goto ret;
	}
ret:
	SMP_UNLOCK_STACONTROL_LIST(flags);
	RESTORE_INT(flags);		

	return ret;

}

unsigned char stactrl_hidden_ap(struct rtl8192cd_priv *priv)
{
	if(!priv->stactrl.stactrl_enable_hiddenAP || priv->stactrl.stactrl_prefer)
		return 1;
	printk("[%s]\n", __FUNCTION__);
	
	if(!priv->pmib->dot11OperationEntry.hiddenAP)
		priv->pmib->dot11OperationEntry.hiddenAP = 1;
	
	if(priv->stactrl.stactrl_hiddenAP_aging != STACTRL_HIDDENAP_EXPIRE)
		priv->stactrl.stactrl_hiddenAP_aging = STACTRL_HIDDENAP_EXPIRE;
	return 1;
}
#endif

#ifdef STA_CONTROL_BAND_TRANSITION
// Lookup STA in bandTransList
static struct BandTransNode *band_trans_lookup(struct rtl8192cd_priv *priv, unsigned char *mac, unsigned char del_flag)
{
	struct BandTransNode *entry;
	struct list_head *phead, *plist;
	bool result = false;
	unsigned long flags;

	spin_lock_irqsave(&priv->BandTransLock, flags);

	phead = &priv->BandTransList;
	if (!list_empty(phead)) {
		plist = phead->next;
		while (plist != phead) {
			entry = list_entry(plist, struct BandTransNode, list);
			plist = plist->next;
			if (0 == memcmp(entry->mac, mac, MACADDRLEN)) {
				result = true;
				if (del_flag)
					list_del(&entry->list);
				break;
			}
		}
	}

	spin_unlock_irqrestore(&priv->BandTransLock, flags);

	if (result)
		return entry;
	else
		return NULL;
}

// Get STA from bandTransList (the entry is removed from the list)
struct BandTransNode *band_trans_get(struct rtl8192cd_priv *priv, unsigned char *mac)
{
	return band_trans_lookup(priv, mac, 1);
}

// Add STAs to bandTransList
static void band_trans_add(struct rtl8192cd_priv *priv, struct BandTransNode *entry)
{
	unsigned long flags;

	spin_lock_irqsave(&priv->BandTransLock, flags);

	list_add_tail(&entry->list, &priv->BandTransList);

	spin_unlock_irqrestore(&priv->BandTransLock, flags);
}

static int stactrl_band_transition_condition_check(struct rtl8192cd_priv *priv, struct stat_info *pstat, int *candidate_num)
{
	int ret = 0;
	struct rtl8192cd_priv *priv_sc = priv->stactrl.stactrl_priv_sc;
	unsigned int  sta_rate = ((pstat->tx_avarage+pstat->rx_avarage) >> 17) * 100 / priv_max_tp(priv);
    int           score = 0;
	int           score_sc = 0;

	if(priv->stactrl.stactrl_prefer)
	{
		if(pstat->rssi < priv->pmib->staControl.stactrl_rssi_th_low)
		{
			score += priv->pmib->staControl.stactrl_score_upper + 10;
		}
		if(priv->pmib->staControl.stactrl_load_balance)
		{
			score += priv->load_balance_score
					- ((*candidate_num) * priv->pmib->staControl.stactrl_sta_num_weight)
					- ((sta_rate * priv->pmib->staControl.stactrl_sta_rate_weight));

			score_sc += priv_sc->load_balance_score
					+ ((*candidate_num) * priv_sc->pmib->staControl.stactrl_sta_num_weight);
		}
		if(score > priv->pmib->staControl.stactrl_score_upper &&
		   score - priv->pmib->staControl.stactrl_sta_num_weight > score_sc + priv->pmib->staControl.stactrl_sta_num_weight)
		{
			ret = 1;
		}
	}
	else
	{
		if((pstat->rssi > priv->pmib->staControl.stactrl_rssi_th_high)
		&& (0 == priv->pmib->staControl.stactrl_ch_util_th ||
				 priv_sc->ext_stats.ch_utilization < priv->pmib->staControl.stactrl_ch_util_th)
		&& (0 == priv->pmib->staControl.stactrl_sta_load_th_2g ||
				 STA_CUR_LOAD(pstat) < priv->pmib->staControl.stactrl_sta_load_th_2g))
		{
			score = priv->pmib->staControl.stactrl_score_upper;
		}
		if(priv->pmib->staControl.stactrl_load_balance)
		{
			score_sc = priv_sc->load_balance_score;
		}
		if(score > priv->pmib->staControl.stactrl_score_lower
			&& score_sc < priv->pmib->staControl.stactrl_score_lower)
		{
			ret = 1;
		}
	}
	pstat->stactrl_load_balance_score = score;
	pstat->stactrl_load_balance_score_sc = score_sc;
	if(ret) (*candidate_num)++;

	return ret;
}

static void stactrl_band_transition_detection_non_prefer_band(struct rtl8192cd_priv *priv)
{
	struct list_head *phead, *plist;
	struct stat_info *pstat;
	struct BandTransNode *entry;
	int num = priv->assoc_num;
	int candidate_num = 0;

	// DONOT call spin lock, because this function is called by
	// rtl8192cd_expire_timer(), which has called spin lock

	priv->load_balance_score = stactrl_cal_basic_load_balance_score(priv);

	phead = &priv->asoc_list;
	if (list_empty(phead))
		return;

	plist = phead;
	while (((plist = asoc_list_get_next(priv, plist)) != phead) && (num > 0))
	{
		pstat = list_entry(plist, struct stat_info, asoc_list);

#ifdef CONFIG_RTK_MESH
		if ((GET_MIB(priv)->dot1180211sInfo.mesh_enable) && !isSTA(pstat))
			continue;
#endif

#ifdef WDS
		if (pstat->state & WIFI_WDS)
			continue;
#endif

		// Check whether STA staisfies the rssi threshold requirement of steering to prefer band, and
		// the prefer band has available capacity
		if ((pstat->state & WIFI_ASOC_STATE) && (pstat->expire_to > 0))
		{
			int ret = stactrl_band_transition_condition_check(priv, pstat, &candidate_num);
			pstat->stactrl_steer_detect_counter = ret ? pstat->stactrl_steer_detect_counter + 1 : 0;

			num--;
			/*
			printk("%s [%02x%02x%02x%02x%02x%02x] score = %d, score_sc = %d, counter = %d, ret = %d\n",
				priv->dev->name, pstat->cmn_info.mac_addr[0], pstat->cmn_info.mac_addr[1],
				pstat->cmn_info.mac_addr[2], pstat->cmn_info.mac_addr[3], pstat->cmn_info.mac_addr[4],
				pstat->cmn_info.mac_addr[5], pstat->stactrl_load_balance_score,
				pstat->stactrl_load_balance_score_sc, pstat->stactrl_steer_detect_counter, ret);
			*/
		}

		// Check whether STA staisfies the band transition requirement over a certain period of time
		if (pstat->stactrl_steer_detect_counter >= priv->pmib->staControl.stactrl_steer_detect)
		{
			// Half the required detection period to prevent overflow
			pstat->stactrl_steer_detect_counter >>= 1;

			// Check whether STAs is already in bandTransList
			entry = band_trans_lookup(priv, pstat->cmn_info.mac_addr, 0);
			if (entry)
			{
				entry->counter = BAND_TRANS_BLOCK_TIME;
				STACTRL_DEBUG(entry->mac, "in bandTransList already");
				continue;
			}

			entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
			if (NULL != entry)
			{
				INIT_LIST_HEAD(&entry->list);
				memcpy(entry->mac, pstat->cmn_info.mac_addr, MACADDRLEN);
				entry->counter = BAND_TRANS_BLOCK_TIME;
				band_trans_add(priv, entry);
				STACTRL_DEBUG(entry->mac, "bandTransList add");
			}
			else
			{
				STACTRL_DEBUG(entry->mac, "bandTransList add failed");
			}
		}
	}
}

static void stactrl_band_transition_detection_prefer_band(struct rtl8192cd_priv *priv)
{
	struct list_head *phead, *plist;
	struct stat_info *pstat;
	struct BandTransNode *entry;
	struct stactrl_preferband_entry *prefer_ent;
	unsigned long flags;
	int num = priv->assoc_num;
	int candidate_num = 0;

	priv->load_balance_score = stactrl_cal_basic_load_balance_score(priv);

	// DONOT call spin lock, because this function is called by
	// rtl8192cd_expire_timer(), which has called spin lock

	phead = &priv->asoc_list;
	if (list_empty(phead))
		return;

	plist = phead;
	while (((plist = asoc_list_get_next(priv, plist)) != phead) && (num > 0))
	{
		pstat = list_entry(plist, struct stat_info, asoc_list);

#ifdef CONFIG_RTK_MESH
		if ((GET_MIB(priv)->dot1180211sInfo.mesh_enable) && !isSTA(pstat))
			continue;
#endif

#ifdef WDS
		if (pstat->state & WIFI_WDS)
			continue;
#endif

		// Check whether STA staisfies the rssi threshold requirement of steering to non-prefer band
		if ((pstat->state & WIFI_ASOC_STATE) && (pstat->expire_to > 0))
		{
			int ret = stactrl_band_transition_condition_check(priv, pstat, &candidate_num);
			pstat->stactrl_steer_detect_counter = ret ? pstat->stactrl_steer_detect_counter + 1 : 0;

			num--;
			/*
			printk("%s [%02x%02x%02x%02x%02x%02x] score = %d, score_sc = %d, counter = %d, ret = %d\n",
				priv->dev->name, pstat->cmn_info.mac_addr[0], pstat->cmn_info.mac_addr[1],
				pstat->cmn_info.mac_addr[2], pstat->cmn_info.mac_addr[3], pstat->cmn_info.mac_addr[4],
				pstat->cmn_info.mac_addr[5], pstat->stactrl_load_balance_score,
				pstat->stactrl_load_balance_score_sc, pstat->stactrl_steer_detect_counter, ret);
			*/
		}

		// Check whether STA staisfies the band transition requirement over a certain period of time
		if (pstat->stactrl_steer_detect_counter >= priv->pmib->staControl.stactrl_steer_detect)
		{
			// Half the required detection period to prevent overflow
			pstat->stactrl_steer_detect_counter >>= 1;

			// Check whether STAs is already in bandTransList
			entry = band_trans_lookup(priv, pstat->cmn_info.mac_addr, 0);
			if (entry)
			{
				entry->counter = BAND_TRANS_BLOCK_TIME;
				STACTRL_DEBUG(entry->mac, "in bandTransList already");
				continue;
			}

			entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
			if (NULL != entry)
			{
				INIT_LIST_HEAD(&entry->list);
				memcpy(entry->mac, pstat->cmn_info.mac_addr, MACADDRLEN);
				entry->counter = BAND_TRANS_BLOCK_TIME;
				band_trans_add(priv, entry);
				STACTRL_DEBUG(entry->mac, "bandTransList add");
			}
			else
			{
				STACTRL_DEBUG(entry->mac, "bandTransList add failed");
			}

			if(priv->pmib->staControl.stactrl_load_balance)
			{
				SAVE_INT_AND_CLI(flags);
    			SMP_LOCK_STACONTROL_LIST(flags);

				prefer_ent = stactrl_preferband_sta_lookup(priv, pstat->cmn_info.mac_addr);
				if(prefer_ent)
					prefer_ent->block_timer = priv->pmib->staControl.stactrl_load_balance;

				SMP_UNLOCK_STACONTROL_LIST(flags);
    			RESTORE_INT(flags);
			}
		}
	}
}

// Send disassociation and add STAs to block list
// This function must be called inside priv->BandTransLock locked
void TriggerBandTrans(struct rtl8192cd_priv *priv, struct BandTransNode *blockNode)
{
	struct stat_info *pstat;
	struct BandTransNode *entry;
	struct list_head *phead, *plist;
	bool result = true;
	unsigned long flags;

	if (NULL == blockNode)
		return;

	// Check whether blockNode is already in the block list
	phead = &priv->BandTransBlockList;
	if (!list_empty(phead)) {
		plist = phead->next;
		while (plist != phead) {
			entry = list_entry(plist, struct BandTransNode, list);
			plist = plist->next;
			if(0 == memcmp(entry->mac, blockNode->mac, MACADDRLEN)) {
				result = false;
				break;
			}
		}
	}

	// Add blockNode to block list
	if (result) {
		entry = blockNode;
		entry->counter = BAND_TRANS_BLOCK_TIME;
		list_add_tail(&entry->list, &priv->BandTransBlockList);
		STACTRL_DEBUG(entry->mac, "add STA into list_head");
	}
	else {
		// blockNode is already in the block list, free memory
		kfree(blockNode);
	}

	// send disassociation to STA
	pstat = get_stainfo(priv, entry->mac);

	if(pstat){
		STACTRL_DEBUG(entry->mac, "disasasociate STA");

		//issue_disassoc(priv, entry->mac, _RSON_DISASSOC_DUE_BSS_TRANSITION);
		del_station(priv, pstat, 1);
	}
}

// Decrease counter and remove node from BandTransBlockList after expiration
static void band_trans_expire(struct rtl8192cd_priv *priv)
{
	struct list_head *phead, *plist;
	struct BandTransNode *entry;
	unsigned long flags;

	phead = &priv->BandTransBlockList;

	if (list_empty(phead))
		return;

	spin_lock_irqsave(&priv->BandTransLock, flags);

	plist = phead->next;

	while (plist != phead) {
		entry = list_entry(plist, struct BandTransNode, list);
		plist = plist->next;

		entry->counter--;
		if (0 == entry->counter) {
			STACTRL_DEBUG(entry->mac, "remove STA from list_head");
			list_del(&entry->list);
			kfree(entry);
		}
	}

	spin_unlock_irqrestore(&priv->BandTransLock, flags);
}

// Transit STAs to another band
//	non-11v-STAs: disassociation immediately
//	11v-STAs: send 11v BTM request frame
static void start_band_transition(struct rtl8192cd_priv *priv)
{
	struct list_head *phead, *plist;
	struct BandTransNode *entry;
	unsigned long flags;
	struct stat_info *pstat;
	struct rtl8192cd_priv *priv_sc = priv->stactrl.stactrl_priv_sc;

	spin_lock_irqsave(&priv->BandTransLock, flags);

	phead = &priv->BandTransList;

	if (list_empty(phead)) {
		goto end_band_transition;
	}

	plist = phead->next;

	// Extract STAs from BandTransList 1-by-1, then transit them to another band
	while (plist != phead) {
		entry = list_entry(plist, struct BandTransNode, list);
		plist = plist->next;

		entry->counter--;

		// if counter reach zero, remove the entry and stop transiting band for this STA
		if (0 == entry->counter) {
			STACTRL_DEBUG(entry->mac, "remove STA from list_head");
			list_del(&entry->list);
			kfree(entry);
			continue;
		}

		// if STA is gone, remove the entry
		pstat = get_stainfo(priv, entry->mac);
		if (NULL == pstat) {
			list_del(&entry->list);
			kfree(entry);
			continue;
		}

#if defined(DOT11K) && defined(CONFIG_IEEE80211V)
		// Check whether AP enables 11v support and STA supports 11v BTM
		if(WNM_ENABLE && pstat->bssTransSupport) {
			// Entry will be deleted from bandTransList after BTM response is received
			issue_BSS_Trans_Req(priv, entry->mac, NULL,
					(GET_MIB(priv_sc))->dot11StationConfigEntry.dot11Bssid,
					(GET_MIB(priv_sc))->dot11RFEntry.dot11channel);
		} else
#endif // defined(DOT11K) && defined(CONFIG_IEEE80211V)
		{  // non-11v STAs
			list_del(&entry->list);
			TriggerBandTrans(priv, entry);
		}
	}

end_band_transition:
	spin_unlock_irqrestore(&priv->BandTransLock, flags);
}

// Check whether STAs meet the RSSI threshold requirement for band transition
void stactrl_band_transition(struct rtl8192cd_priv *priv)
{
	if (NULL == priv->stactrl.stactrl_priv_sc)
		return;

	// Remove expired node from BandTransBlockList
	band_trans_expire(priv);

	// Find STAs which statisy the requirement of band transition
	if (priv->stactrl.stactrl_prefer)
		stactrl_band_transition_detection_prefer_band(priv);
	else
		stactrl_band_transition_detection_non_prefer_band(priv);

	// Start band transition
	start_band_transition(priv);
}

// Check whether the specified mac addr is listed in BandTransList
bool checkBandTransBlockList(struct rtl8192cd_priv *priv, unsigned char *mac, int frame_type)
{
	bool result = false;
	struct list_head *phead, *plist;
	struct BandTransNode *entry;
	unsigned long flags;

	phead = &priv->BandTransBlockList;

	if (list_empty(phead))
		return false;

	spin_lock_irqsave(&priv->BandTransLock, flags);

	plist=phead->next;
	while(plist!=phead){
		entry = list_entry(plist, struct BandTransNode, list);
		plist = plist->next;
		if(0 == memcmp(entry->mac, mac, MACADDRLEN)){
			result = true;
			break;
		}
	}

	if(result){
		STACTRL_DEBUG(entry->mac, "reject this packet frame_type=%d", frame_type);
	}

	spin_unlock_irqrestore(&priv->BandTransLock, flags);

	return result;
}
#endif  // STA_CONTROL_BAND_TRANSITION
#endif /* STA_CONTROL */
