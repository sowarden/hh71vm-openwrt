/*
 *  Utility routines
 *
 *  Copyright (c) 2017 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _8192CD_UTILS_C_

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

#include "./8192cd_cfg.h"
#include "./8192cd.h"
#include "./8192cd_util.h"
#include "./8192cd_headers.h"
#include "./8192cd_debug.h"
#if defined(RTK_NL80211)
#include "8192cd_cfg80211.h"
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
#if defined(USE_PID_NOTIFY) && defined(LINUX_2_6_27_)
struct pid *_wlanapp_pid;
struct pid *_wlanwapi_pid;
#ifdef CONFIG_IEEE80211R
struct pid *_wlanft_pid;
#endif
#if defined(GENERAL_EVENT)
struct pid *_wlangeneral_pid;
#endif
#endif

#if defined(CONFIG_RTL_OFFLOAD_DRIVER) && defined(CONFIG_PE_ENABLE)
#include "pe/pe_ipc.h"
#endif



unsigned char Realtek_OUI[] = {0x00, 0xe0, 0x4c};
//unsigned char dot11_rate_table[] = {2,4,11,22,12,18,24,36,48,72,96,108,0}; // last element must be zero!!
unsigned char oui_rfc1042[] = {0x00, 0x00, 0x00};
unsigned char oui_8021h[] = {0x00, 0x00, 0xf8};
unsigned char oui_cisco[] = {0x00, 0x00, 0x0c};
unsigned char SNAP_ETH_TYPE_IPX[2] = {0x81, 0x37};
unsigned char SNAP_ETH_TYPE_APPLETALK_AARP[2] = {0x80, 0xf3};
unsigned char SNAP_ETH_TYPE_APPLETALK_DDP[2] = {0x80, 0x9B};
unsigned char SNAP_HDR_APPLETALK_DDP[3] = {0x08, 0x00, 0x07}; // Datagram Delivery Protocol
unsigned char NULL_MAC[MACADDRLEN]={0,0,0,0,0,0};
unsigned char BROADCAST_MAC[MACADDRLEN]={0xff,0xff,0xff,0xff,0xff,0xff};
unsigned char RSN_OUI[] = {0x00, 0x0F, 0xAC};


#if defined(RTK_WLAN_EVENT_INDICATE)
extern struct sock *get_nl_eventd_sk(void);
extern int get_nl_eventd_pid(void);
extern void rtk_eventd_netlink_send(int pid, struct sock *nl_sk, int eventID, char *ifname, char *data, int data_len);
extern void rtk_eventd_netlink_send_multicast(int pid, struct sock *nl_sk, int eventID, char *ifname, char *data, int data_len);
#endif

#ifdef __ECOS
#ifndef VLAN_HLEN
#define VLAN_HLEN 4
#endif
#endif

#if defined(CONFIG_BRIDGE) && defined(CONFIG_RTL_819X_SWCORE)
int rtl_wifi_mac_create_hooks(const unsigned char *addr);
int rtl_wifi_mac_del_hooks(const unsigned char *addr);
extern INT32 rtl_add_ext_fdb_entry(const unsigned char *addr);	
extern INT32 rtl_del_ext_fdb_entry(const unsigned char *addr);
#endif

void mem_dump(unsigned char *ptitle, unsigned char *pbuf, int len)
{
	char tmpbuf[100];
	int i, n = 0;
	
	if (ptitle)
		snprintf(tmpbuf, sizeof(tmpbuf), "%s", ptitle);
	else
		tmpbuf[0] = '\0';
	
	for (i = 0; i < len; ++i ) {
		if (!(i & 0x0f)) {
			printk("%s\n", tmpbuf);
			n = sprintf(tmpbuf, "%03X:\t", i);
		}
		n += snprintf((tmpbuf+n), sizeof(tmpbuf)-n, " %02X", pbuf[i]);
	}
	printk("%s\n", tmpbuf);
}

struct rtl_arphdr
{
	//for corss platform
    __be16          ar_hrd;         /* format of hardware address   */
    __be16          ar_pro;         /* format of protocol address   */
    unsigned char   ar_hln;         /* length of hardware address   */
    unsigned char   ar_pln;         /* length of protocol address   */
    __be16          ar_op;          /* ARP opcode (command)         */
};

#ifdef DBG_MEMORY_LEAK
#include <asm/atomic.h>
atomic_t _malloc_cnt = ATOMIC_INIT(0);
atomic_t _malloc_size = ATOMIC_INIT(0);
#endif /* DBG_MEMORY_LEAK */

#ifdef __KERNEL__
inline u8* _rtw_vmalloc(u32 sz)
{
	u8 *pbuf;

	pbuf = vmalloc(sz);

#ifdef DBG_MEMORY_LEAK
	if (pbuf != NULL) {
		atomic_inc(&_malloc_cnt);
		atomic_add(sz, &_malloc_size);
	}
#endif /* DBG_MEMORY_LEAK */

	return pbuf;
}

inline u8* _rtw_zvmalloc(u32 sz)
{
	u8 	*pbuf;

	pbuf = _rtw_vmalloc(sz);
	if (pbuf != NULL) {
		memset(pbuf, 0, sz);
	}

	return pbuf;
}

inline void _rtw_vmfree(const void *pbuf, u32 sz)
{
	if (pbuf)
	{
		vfree(pbuf);

#ifdef DBG_MEMORY_LEAK
		atomic_dec(&_malloc_cnt);
		atomic_sub(sz, &_malloc_size);
#endif /* DBG_MEMORY_LEAK */
	}
}
#endif // __KERNEL__

#if defined(__ECOS)
u8* _rtw_malloc_flag(u32 sz, int gfp)
#else
u8* _rtw_malloc_flag(u32 sz, gfp_t gfp)
#endif
{
	u8 *pbuf = NULL;

	pbuf = kmalloc(sz, gfp);

#ifdef DBG_MEMORY_LEAK
	if (pbuf != NULL) {
		atomic_inc(&_malloc_cnt);
		atomic_add(sz, &_malloc_size);
	}
#endif /* DBG_MEMORY_LEAK */

	return pbuf;
}


u8* _rtw_malloc(u32 sz)
{
	u8 *pbuf = NULL;

	pbuf = kmalloc(sz, /*GFP_KERNEL*/GFP_ATOMIC);

#ifdef DBG_MEMORY_LEAK
	if (pbuf != NULL) {
		atomic_inc(&_malloc_cnt);
		atomic_add(sz, &_malloc_size);
	}
#endif /* DBG_MEMORY_LEAK */

	return pbuf;
}

u8* _rtw_zmalloc(u32 sz)
{
	u8 *pbuf = _rtw_malloc(sz);

	if (pbuf != NULL) {
		memset(pbuf, 0, sz);
	}

	return pbuf;
}

void* rtw_malloc2d(int h, int w, int size)
{
	int j;

	void **a = (void **) rtw_zmalloc( h*sizeof(void *) + h*w*size );
	if(a == NULL)
	{
		return NULL;
	}

	for( j=0; j<h; j++ )
		a[j] = ((char *)(a+h)) + j*w*size;

	return a;
}

void rtw_mfree2d(void *pbuf, int h, int w, int size)
{
	rtw_mfree((u8 *)pbuf, h*sizeof(void*) + w*h*size);
}

void _rtw_mfree(const void *pbuf, u32 sz)
{
	if (pbuf)
	{
		kfree((void*)pbuf);
		
#ifdef DBG_MEMORY_LEAK
		atomic_dec(&_malloc_cnt);
		atomic_sub(sz, &_malloc_size);
#endif /* DBG_MEMORY_LEAK */
	}
}





__inline__ unsigned int orSTABitMap(STA_BITMAP *map)
{
    return (
        map->_staMap_
#if (NUM_STAT >32)
        || map->_staMap_ext_1
#if (NUM_STAT >64)		
        || map->_staMap_ext_2 || map->_staMap_ext_3
#endif
#endif
    );	
}



/* return 1 or 0*/
unsigned char getSTABitMap(STA_BITMAP *map, int bitIdx)
{
    unsigned int ret = 0;
    bitIdx--;

    if (bitIdx < 32)
        ret = map->_staMap_ & BIT(bitIdx);
#if (NUM_STAT >32)		
    else if (bitIdx <= 64)
        ret = map->_staMap_ext_1 & BIT(bitIdx - 32);
#if (NUM_STAT >64)		
    else if (bitIdx <= 96)
        ret  = map->_staMap_ext_2 & BIT(bitIdx - 64);
    else if (bitIdx <= 128)
        ret = map->_staMap_ext_3 & BIT(bitIdx -96);
#endif	
#endif	

    return (ret?1:0);
}


void setSTABitMap(STA_BITMAP *map, int bitIdx)
{
    bitIdx--;

    if (bitIdx < 32)
        map->_staMap_ |= BIT(bitIdx);
#if (NUM_STAT >32)		
    else if (bitIdx <= 64)
        map->_staMap_ext_1 |= BIT(bitIdx - 32);
#if (NUM_STAT >64)		
    else if (bitIdx <= 96)
        map->_staMap_ext_2 |= BIT(bitIdx - 64);
    else if (bitIdx <= 128)
        map->_staMap_ext_3 |= BIT(bitIdx -96);
#endif	
#endif	
}



void clearSTABitMap(STA_BITMAP* map, int bitIdx)
{
    bitIdx--;

    if (bitIdx < 32)
        map->_staMap_ &= ~ BIT(bitIdx);
#if (NUM_STAT >32)		
    else if (bitIdx < 64)
        map->_staMap_ext_1 &= ~ BIT(bitIdx - 32);
#if (NUM_STAT >64)		
    else if (bitIdx < 96)
        map->_staMap_ext_2 &= ~ BIT(bitIdx - 64);
    else if (bitIdx < 128)
        map->_staMap_ext_3 &= ~ BIT(bitIdx - 96);
#endif	
#endif	
}

#ifdef PRIV_STA_BUF
struct priv_obj_buf {
	unsigned char magic[8];
	struct list_head	list;
	struct aid_obj obj;
};

#if defined(WIFI_WMM) && defined(WMM_APSD)
struct priv_apsd_que {
	unsigned char magic[8];
	struct list_head	list;
	struct apsd_pkt_queue que;
};
#endif

#if defined(WIFI_WMM)
struct priv_dz_mgt_que {
	unsigned char magic[8];
	struct list_head	list;
	struct dz_mgmt_queue que;
};
#endif

#if defined(INCLUDE_WPA_PSK) || defined(WIFI_HAPD) || defined(RTK_NL80211)
struct priv_wpa_buf {
	unsigned char magic[8];
	struct list_head	list;
	WPA_STA_INFO wpa;
};
#endif

#define MAGIC_CODE_BUF			"8192"

#define MAX_PRIV_OBJ_NUM		NUM_STAT

#ifdef CONCURRENT_MODE
static struct priv_obj_buf obj_buf[NUM_WLAN_IFACE][MAX_PRIV_OBJ_NUM];
static struct list_head objbuf_list[NUM_WLAN_IFACE];
static int free_obj_buf_num[NUM_WLAN_IFACE];


#if defined(WIFI_WMM) && defined(WMM_APSD)
	#define MAX_PRIV_QUE_NUM	(MAX_PRIV_OBJ_NUM*4)
	static struct priv_apsd_que que_buf[NUM_WLAN_IFACE][MAX_PRIV_QUE_NUM];
	static struct list_head quebuf_list[NUM_WLAN_IFACE];
	static int free_que_buf_num[NUM_WLAN_IFACE];
#endif

#if defined(WIFI_WMM)
	#define MAX_MGT_QUE_NUM	(MAX_PRIV_OBJ_NUM)
	static struct priv_dz_mgt_que mgt_que_buf[NUM_WLAN_IFACE][MAX_MGT_QUE_NUM];
	static struct list_head mgt_quebuf_list[NUM_WLAN_IFACE];
	static int free_mgt_que_buf_num[NUM_WLAN_IFACE];
#endif

#if defined(INCLUDE_WPA_PSK) || defined(WIFI_HAPD) || defined(RTK_NL80211)
	#define MAX_PRIV_WPA_NUM	MAX_PRIV_OBJ_NUM
	static struct priv_wpa_buf wpa_buf[NUM_WLAN_IFACE][MAX_PRIV_WPA_NUM];
	static struct list_head wpabuf_list[NUM_WLAN_IFACE];
	static int free_wpa_buf_num[NUM_WLAN_IFACE];
#endif

#else
static struct priv_obj_buf obj_buf[MAX_PRIV_OBJ_NUM];
static struct list_head objbuf_list;
static int free_obj_buf_num;


#if defined(WIFI_WMM) && defined(WMM_APSD)
	#define MAX_PRIV_QUE_NUM	(MAX_PRIV_OBJ_NUM*4)
	static struct priv_apsd_que que_buf[MAX_PRIV_QUE_NUM];
	static struct list_head quebuf_list;
	static int free_que_buf_num;
#endif

#if defined(WIFI_WMM)
	#define MAX_MGT_QUE_NUM	(MAX_PRIV_OBJ_NUM)
	static struct priv_dz_mgt_que mgt_que_buf[MAX_MGT_QUE_NUM];
	static struct list_head mgt_quebuf_list;
	static int free_mgt_que_buf_num;
#endif

#if defined(INCLUDE_WPA_PSK) || defined(WIFI_HAPD) || defined(RTK_NL80211)
	#define MAX_PRIV_WPA_NUM	MAX_PRIV_OBJ_NUM
	static struct priv_wpa_buf wpa_buf[MAX_PRIV_WPA_NUM];
	static struct list_head wpabuf_list;
	static int free_wpa_buf_num;
#endif
#endif

void init_priv_sta_buf(struct rtl8192cd_priv *priv)
{
	int i;
#ifdef CONCURRENT_MODE
	int idx = priv->pshare->wlandev_idx;
	memset(&obj_buf[idx], '\0', sizeof(struct priv_obj_buf)*MAX_PRIV_OBJ_NUM);
	INIT_LIST_HEAD(&objbuf_list[idx]);
	for (i=0; i<MAX_PRIV_OBJ_NUM; i++)  {
		memcpy(obj_buf[idx][i].magic, MAGIC_CODE_BUF, 4);
		INIT_LIST_HEAD(&obj_buf[idx][i].list);
		list_add_tail(&obj_buf[idx][i].list, &objbuf_list[idx]);
	}
	free_obj_buf_num[idx] = i;

#if defined(WIFI_WMM) && defined(WMM_APSD)
	memset(&que_buf[idx], '\0', sizeof(struct priv_apsd_que)*MAX_PRIV_QUE_NUM);
	INIT_LIST_HEAD(&quebuf_list[idx]);
	for (i=0; i<MAX_PRIV_QUE_NUM; i++)  {
		memcpy(que_buf[idx][i].magic, MAGIC_CODE_BUF, 4);
		INIT_LIST_HEAD(&que_buf[idx][i].list);
		list_add_tail(&que_buf[idx][i].list, &quebuf_list[idx]);
	}
	free_que_buf_num[idx] = i;
#endif

#if defined(WIFI_WMM)
	memset(&mgt_que_buf[idx], '\0', sizeof(struct priv_dz_mgt_que)*MAX_MGT_QUE_NUM);
	INIT_LIST_HEAD(&mgt_quebuf_list[idx]);
	for (i=0; i<MAX_MGT_QUE_NUM; i++)  {
		memcpy(mgt_que_buf[idx][i].magic, MAGIC_CODE_BUF, 4);
		INIT_LIST_HEAD(&mgt_que_buf[idx][i].list);
		list_add_tail(&mgt_que_buf[idx][i].list, &mgt_quebuf_list[idx]);
	}
	free_mgt_que_buf_num[idx] = i;
#endif

#if defined(INCLUDE_WPA_PSK) || defined(WIFI_HAPD) || defined(RTK_NL80211)
	memset(&wpa_buf[idx], '\0', sizeof(struct priv_wpa_buf)*MAX_PRIV_WPA_NUM);
	INIT_LIST_HEAD(&wpabuf_list[idx]);
	for (i=0; i<MAX_PRIV_WPA_NUM; i++)  {
		memcpy(wpa_buf[idx][i].magic, MAGIC_CODE_BUF, 4);
		INIT_LIST_HEAD(&wpa_buf[idx][i].list);
		list_add_tail(&wpa_buf[idx][i].list, &wpabuf_list[idx]);
	}
	free_wpa_buf_num[idx] = i;
#endif
#else	
	memset(obj_buf, '\0', sizeof(struct priv_obj_buf)*MAX_PRIV_OBJ_NUM);
	INIT_LIST_HEAD(&objbuf_list);
	for (i=0; i<MAX_PRIV_OBJ_NUM; i++)  {
		memcpy(obj_buf[i].magic, MAGIC_CODE_BUF, 4);
		INIT_LIST_HEAD(&obj_buf[i].list);
		list_add_tail(&obj_buf[i].list, &objbuf_list);
	}
	free_obj_buf_num = i;

#if defined(WIFI_WMM) && defined(WMM_APSD)
	memset(que_buf, '\0', sizeof(struct priv_apsd_que)*MAX_PRIV_QUE_NUM);
	INIT_LIST_HEAD(&quebuf_list);
	for (i=0; i<MAX_PRIV_QUE_NUM; i++)  {
		memcpy(que_buf[i].magic, MAGIC_CODE_BUF, 4);
		INIT_LIST_HEAD(&que_buf[i].list);
		list_add_tail(&que_buf[i].list, &quebuf_list);
	}
	free_que_buf_num = i;
#endif

#if defined(WIFI_WMM) 
	memset(&mgt_que_buf, '\0', sizeof(struct priv_dz_mgt_que)*MAX_MGT_QUE_NUM);
	INIT_LIST_HEAD(&mgt_quebuf_list);
	for (i=0; i<MAX_MGT_QUE_NUM; i++)  {
		memcpy(mgt_que_buf[i].magic, MAGIC_CODE_BUF, 4);
		INIT_LIST_HEAD(&mgt_que_buf[i].list);
		list_add_tail(&mgt_que_buf[i].list, &mgt_quebuf_list);
	}
	free_mgt_que_buf_num = i;
#endif

#if defined(INCLUDE_WPA_PSK) || defined(WIFI_HAPD) || defined(RTK_NL80211)
	memset(wpa_buf, '\0', sizeof(struct priv_wpa_buf)*MAX_PRIV_WPA_NUM);
	INIT_LIST_HEAD(&wpabuf_list);
	for (i=0; i<MAX_PRIV_WPA_NUM; i++)  {
		memcpy(wpa_buf[i].magic, MAGIC_CODE_BUF, 4);
		INIT_LIST_HEAD(&wpa_buf[i].list);
		list_add_tail(&wpa_buf[i].list, &wpabuf_list);
	}
	free_wpa_buf_num = i;
#endif
#endif
}

struct aid_obj *alloc_sta_obj(struct rtl8192cd_priv *priv)
{
#ifndef SMP_SYNC
	unsigned long flags=0;
#endif
	struct aid_obj *priv_obj;

    if(priv)
	    SAVE_INT_AND_CLI(flags);

#if defined(__ECOS) && defined(CONFIG_SDIO_HCI)
#ifdef CONCURRENT_MODE
	priv_obj = (struct aid_obj  *)get_buf_from_poll(priv, &objbuf_list[priv->pshare->wlandev_idx], (unsigned int *)&free_obj_buf_num[priv->pshare->wlandev_idx]);
#else	
	priv_obj = (struct aid_obj  *)get_buf_from_poll(priv, &objbuf_list, (unsigned int *)&free_obj_buf_num);
#endif
#else
#ifdef CONCURRENT_MODE
	priv_obj = (struct aid_obj  *)get_buf_from_poll(NULL, &objbuf_list[priv->pshare->wlandev_idx], (unsigned int *)&free_obj_buf_num[priv->pshare->wlandev_idx]);
#else	
	priv_obj = (struct aid_obj  *)get_buf_from_poll(NULL, &objbuf_list, (unsigned int *)&free_obj_buf_num);
#endif
#endif

   if(priv)
        RESTORE_INT(flags);

#if defined(__KERNEL__) || defined(__OSK__)
	if (priv_obj == NULL)
		return ((struct aid_obj *)kmalloc(sizeof(struct aid_obj), GFP_ATOMIC));
	else
#endif		
		return priv_obj;
}

void free_sta_obj(struct rtl8192cd_priv *priv, struct aid_obj *obj)
{
	unsigned long offset = (unsigned long)(&((struct priv_obj_buf *)0)->obj);
	struct priv_obj_buf *priv_obj = (struct priv_obj_buf *)(((unsigned long)obj) - offset);
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	if (!memcmp(priv_obj->magic, MAGIC_CODE_BUF, 4) &&
			((unsigned long)&priv_obj->obj) ==  ((unsigned long)obj)) {
		SAVE_INT_AND_CLI(flags);
#ifdef CONCURRENT_MODE
		release_buf_to_poll(priv, (unsigned char *)obj, &objbuf_list[priv->pshare->wlandev_idx], (unsigned int *)&free_obj_buf_num[priv->pshare->wlandev_idx]);
#else
		release_buf_to_poll(priv, (unsigned char *)obj, &objbuf_list, (unsigned int *)&free_obj_buf_num);
#endif
		RESTORE_INT(flags);
	}
	else
#ifdef __ECOS
		ASSERT(0);
#else
		kfree(obj);
#endif
}

#if defined(WIFI_WMM) && defined(WMM_APSD)
static struct apsd_pkt_queue *alloc_sta_que(struct rtl8192cd_priv *priv)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif
	struct apsd_pkt_queue *priv_que;
	SAVE_INT_AND_CLI(flags);
#ifdef CONCURRENT_MODE
	priv_que = (struct apsd_pkt_queue*)get_buf_from_poll(priv, &quebuf_list[priv->pshare->wlandev_idx], (unsigned int *)&free_que_buf_num[priv->pshare->wlandev_idx]);
#else	
	priv_que = (struct apsd_pkt_queue*)get_buf_from_poll(priv, &quebuf_list, (unsigned int *)&free_que_buf_num);
#endif
	RESTORE_INT(flags);

#if defined(__KERNEL__) || defined(__OSK__)
	if (priv_que == NULL)
		return ((struct apsd_pkt_queue *)kmalloc(sizeof(struct apsd_pkt_queue), GFP_ATOMIC));
	else
#endif		
		return priv_que;
}

void free_sta_que(struct rtl8192cd_priv *priv, struct apsd_pkt_queue *que)
{
	unsigned long offset = (unsigned long)(&((struct priv_apsd_que *)0)->que);
	struct priv_apsd_que *priv_que = (struct priv_apsd_que *)(((unsigned long)que) - offset);
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	if (!memcmp(priv_que->magic, MAGIC_CODE_BUF, 4) &&
			((unsigned long)&priv_que->que) ==  ((unsigned long)que)) {
		SAVE_INT_AND_CLI(flags);
#ifdef CONCURRENT_MODE
		release_buf_to_poll(priv, (unsigned char *)que, &quebuf_list[priv->pshare->wlandev_idx], (unsigned int *)&free_que_buf_num[priv->pshare->wlandev_idx]);
#else			
		release_buf_to_poll(priv, (unsigned char *)que, &quebuf_list, (unsigned int *)&free_que_buf_num);
#endif
		RESTORE_INT(flags);		
	}
	else
#ifdef __ECOS
		ASSERT(0);
#else
		kfree(que);
#endif
}
#endif // defined(WIFI_WMM) && defined(WMM_APSD)

#if defined(WIFI_WMM)
static struct dz_mgmt_queue *alloc_sta_mgt_que(struct rtl8192cd_priv *priv)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif
	struct dz_mgmt_queue *priv_que;

	SAVE_INT_AND_CLI(flags);
#ifdef CONCURRENT_MODE
	priv_que = (struct dz_mgmt_queue*)get_buf_from_poll(priv, &mgt_quebuf_list[priv->pshare->wlandev_idx], (unsigned int *)&free_mgt_que_buf_num[priv->pshare->wlandev_idx]);
#else
	priv_que = (struct dz_mgmt_queue*)get_buf_from_poll(priv, &mgt_quebuf_list, (unsigned int *)&free_mgt_que_buf_num);
#endif
	RESTORE_INT(flags);

#if defined(__KERNEL__) || defined(__OSK__)
	if (priv_que == NULL)
		return ((struct dz_mgmt_queue *)kmalloc(sizeof(struct dz_mgmt_queue), GFP_ATOMIC));
	else
#endif		
		return priv_que;
}

void free_sta_mgt_que(struct rtl8192cd_priv *priv, struct dz_mgmt_queue *que)
{
	unsigned long offset = (unsigned long)(&((struct priv_dz_mgt_que *)0)->que);
	struct priv_dz_mgt_que *priv_que = (struct priv_dz_mgt_que *)(((unsigned long)que) - offset);
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	if (!memcmp(priv_que->magic, MAGIC_CODE_BUF, 4) &&
			((unsigned long)&priv_que->que) ==  ((unsigned long)que)) {
		SAVE_INT_AND_CLI(flags);
#ifdef CONCURRENT_MODE
		release_buf_to_poll(priv, (unsigned char *)que, &mgt_quebuf_list[priv->pshare->wlandev_idx], (unsigned int *)&free_mgt_que_buf_num[priv->pshare->wlandev_idx]);
#else
		release_buf_to_poll(priv, (unsigned char *)que, &mgt_quebuf_list, (unsigned int *)&free_mgt_que_buf_num);
#endif
		RESTORE_INT(flags);		
	}
	else
		kfree(que);
}
#endif // defined(WIFI_WMM) 


#if defined(INCLUDE_WPA_PSK) || defined(WIFI_HAPD) || defined(RTK_NL80211)
static WPA_STA_INFO *alloc_wpa_buf(struct rtl8192cd_priv *priv)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif
	WPA_STA_INFO *priv_buf;
	SAVE_INT_AND_CLI(flags);
#ifdef CONCURRENT_MODE
	priv_buf = (WPA_STA_INFO *)get_buf_from_poll(priv, &wpabuf_list[priv->pshare->wlandev_idx], (unsigned int *)&free_wpa_buf_num[priv->pshare->wlandev_idx]);
#else
	priv_buf = (WPA_STA_INFO *)get_buf_from_poll(priv, &wpabuf_list, (unsigned int *)&free_wpa_buf_num);
#endif
	RESTORE_INT(flags);

#ifdef __ECOS
	ASSERT(priv_buf != NULL);
	return priv_buf;

#else	
	if (priv_buf == NULL)
		return ((WPA_STA_INFO *)kmalloc(sizeof(WPA_STA_INFO), GFP_ATOMIC));
	else
		return priv_buf;
#endif		
}

void free_wpa_buf(struct rtl8192cd_priv *priv, WPA_STA_INFO *buf)
{
	unsigned long offset = (unsigned long)(&((struct priv_wpa_buf *)0)->wpa);
	struct priv_wpa_buf *priv_buf = (struct priv_wpa_buf *)(((unsigned long)buf) - offset);
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	if (!memcmp(priv_buf->magic, MAGIC_CODE_BUF, 4) &&
			((unsigned long)&priv_buf->wpa) ==  ((unsigned long)buf)) {
		SAVE_INT_AND_CLI(flags);
#ifdef CONCURRENT_MODE
		release_buf_to_poll(priv, (unsigned char *)buf, &wpabuf_list[priv->pshare->wlandev_idx], (unsigned int *)&free_wpa_buf_num[priv->pshare->wlandev_idx]);
#else
		release_buf_to_poll(priv, (unsigned char *)buf, &wpabuf_list, (unsigned int *)&free_wpa_buf_num);
#endif
		RESTORE_INT(flags);
	}
	else
#ifdef __ECOS
		ASSERT(0);
#else	
		kfree(buf);
#endif
}
#endif // INCLUDE_WPA_PSK
#endif // PRIV_STA_BUF




#if defined(AP_SWPS_OFFLOAD)
int enque_before_Queuehead(struct rtl8192cd_priv *priv,struct tx_desc_info *pdescinfo, struct apsd_pkt_queue* AC_dz_queue, unsigned short last_seq, int ffsize, void *elm, unsigned char* prepare_done, int* rinsert_position)
{
	// critical section!
#ifndef SMP_SYNC
	unsigned long flags;
#endif
    int* head = &AC_dz_queue->head;
    int* tail = &AC_dz_queue->tail;
    int reserve_num;
    int new_tail;
    int insert_position=0;
        
	//SAVE_INT_AND_CLI(flags); //already disable interrupt in rtl88XX_tx_dsr
    if(AC_dz_queue->swps_seq[*tail]==0){//not insert yet
        //find the head position to insert skb
        //printk("last_seq=%x, pdescinfo->SWPS_sequence=%x\n",last_seq,pdescinfo->SWPS_sequence);
        if(last_seq > pdescinfo->SWPS_sequence)
            reserve_num = last_seq - pdescinfo->SWPS_sequence+1;
        else
            reserve_num = MAX_SWPS_SEQ - pdescinfo->SWPS_sequence + last_seq +1;
        
        if (CIRC_SPACE(*head, *tail, ffsize) < reserve_num) {
            RESTORE_INT(flags);
            *rinsert_position = -1;
            printk("Q is full, *head=%x, *tail=%x, reserve_num=%x\n",*head,*tail,reserve_num);
            return FALSE;
        }
        if(*tail-reserve_num < 0)
            new_tail = ffsize-reserve_num + *tail;
        else
            new_tail = *tail-reserve_num;
        //printk("new_tail=%x,old_tail=%x\n",new_tail,*tail);

        *tail = new_tail;
        insert_position = *tail;
  
    }else{ //insert at position, assume we have inserted the first pkt, and its seq is correct , so we dont need to check if queue is full, we just insert the pkt
        //insert_position = count from tail
        if(pdescinfo->SWPS_sequence < AC_dz_queue->swps_seq[*tail])
            insert_position = MAX_SWPS_SEQ - AC_dz_queue->swps_seq[*tail] + pdescinfo->SWPS_sequence;
        else
            insert_position = pdescinfo->SWPS_sequence - AC_dz_queue->swps_seq[*tail];
        //insert_position = real position
        insert_position = (*tail + insert_position) & (ffsize - 1);
    }

    *(unsigned long *)((unsigned long)AC_dz_queue->pSkb + (insert_position)*(sizeof(void *))) = (unsigned long)elm;
    AC_dz_queue->swps_seq[insert_position] = pdescinfo->SWPS_sequence;

    if(pdescinfo->SWPS_sequence == last_seq)
        *prepare_done = 1; //will be cleared when wakeup and find it is 1

    *rinsert_position = insert_position;

    //RESTORE_INT(flags);
    
	return TRUE;
}

int find_dzposition(struct list_head* dzhead, struct tx_desc_info *pdescinfo)
{
    struct list_head *iterator;
    int node_seq;
    int position_count = 0;
	int seqzero_count =0;
    
    if(list_empty(dzhead))
        return 0; //insert to first position

    list_for_each(iterator, dzhead){ //find position
        node_seq = list_entry(iterator, struct dz_seq_node, node)->swps_seq;
        position_count++;
		GDEBUG("finding node_seq=%x, swps_seq=%x, pos_cnt =%x\n",node_seq,pdescinfo->SWPS_sequence,position_count);
        if(((node_seq+1)==(MAX_SWPS_SEQ+1)) && (pdescinfo->SWPS_sequence==1))//4095 to 1
            return position_count;
        else if((node_seq != 0) && ((node_seq+1) == pdescinfo->SWPS_sequence))//normal case
            return position_count;
		else if(node_seq==0)
			seqzero_count++;
    }
    if(seqzero_count == position_count)
        return 0; //insert to first position
	
    printk("[%s]find_dzposition FAILED\n",__FUNCTION__);
    return -1; //FAILED, if FAILED is defined, return FAILED
}
int insert_dzposition(struct tx_desc_info* pdescinfo,struct stat_info * pstat,int position_count,unsigned short last_seq,unsigned char * prepare_done,struct sk_buff *pskb)
{
    struct list_head *iterator;
    int search_count = 0;
    struct dz_seq_node* insert_node= NULL;
    struct dz_seq_node* tmp;
    struct sk_buff* buf_to_insert_before; 
    struct list_head* dzhead = &pstat->dz_list_head;
    struct sk_buff_head* skb_head = &pstat->dz_queue;
	


    buf_to_insert_before = skb_head->next; //point to first skb
    if(position_count==0){
        goto add_first;
    }
    
    list_for_each(iterator, dzhead){
        search_count++;
        buf_to_insert_before = buf_to_insert_before->next;
        if(search_count == position_count){
            insert_node = list_entry(iterator, struct dz_seq_node, node);
            break;
        }
    }

add_first:
    if((insert_node != NULL) || (position_count==0)){
        tmp = kmalloc(sizeof(struct dz_seq_node),GFP_ATOMIC);
        tmp->swps_seq = pdescinfo->SWPS_sequence;
        if(position_count!=0)
            list_add(&tmp->node,&insert_node->node);
        else
            list_add(&tmp->node,dzhead);

        skb_insert(buf_to_insert_before,pskb,skb_head);

        if((pdescinfo->SWPS_sequence == last_seq) || (last_seq==0 && pstat->PS_get_SWPSRPT==1))
            *prepare_done = 1; //will be cleared when wakeup and find it is 1

		GDEBUG("insert_dzpos ok, sw_seq=%x,last_seq=%x,done= %x\n ",pdescinfo->SWPS_sequence,last_seq,*prepare_done);
        return _TRUE;
    }
    else{
        printk("insert_dzposition failed to get insert position!\n"); //debug
        return _FALSE;
    }
        
}
#endif
#if defined(AP_SWPS_OFFLOAD)
int enque(struct rtl8192cd_priv *priv, struct apsd_pkt_queue* AC_dz_queue, int *head, int *tail, unsigned long ffptr, int ffsize, void *elm)
#else
int enque(struct rtl8192cd_priv *priv, int *head, int *tail, unsigned long ffptr, int ffsize, void *elm)
#endif
{
	// critical section!
#ifndef SMP_SYNC
	unsigned long flags;
#endif
    unsigned int  amsdu_i;

	SAVE_INT_AND_CLI(flags);
	if (CIRC_SPACE(*head, *tail, ffsize) == 0) {
		RESTORE_INT(flags);
		return FALSE;
	}

	*(unsigned long *)(ffptr + (*head)*(sizeof(void *))) = (unsigned long)elm;
#if defined(AP_SWPS_OFFLOAD)
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv)) {
        if(AC_dz_queue){
            AC_dz_queue->swps_seq[*head] = 0;
            AC_dz_queue->amsdu_flag[*head] = 0;
            AC_dz_queue->hdr[*head] = NULL;
            AC_dz_queue->llchdr[*head] = NULL;
            AC_dz_queue->icv[*head] = NULL;
            AC_dz_queue->mic[*head] = NULL;
            AC_dz_queue->hdr_len[*head] = 0;
            AC_dz_queue->mic_icv_len[*head] = 0;
            AC_dz_queue->PSB_len[*head] = 0;
            AC_dz_queue->ptxdesc[*head] = NULL;
            AC_dz_queue->skbbuf_len[*head] = 0;
            AC_dz_queue->isWiFiHdr[*head] = 0;
#if CFG_HAL_SUPPORT_TXDESC_IE
            AC_dz_queue->IE_bitmap[*head] = 0;
#endif            
#ifdef WLAN_HAL_TX_AMSDU                 
            AC_dz_queue->amsdu_num[*head] = 0;
            for(amsdu_i=0;amsdu_i < WLAN_HAL_TX_AMSDU_MAX_NUM;amsdu_i++)
                AC_dz_queue->amsdubuf_len[*head][amsdu_i] = 0;
#endif            
                
        }
    }
#endif    
	*head = (*head + 1) & (ffsize - 1);
	RESTORE_INT(flags);
	return TRUE;
}

#if defined(AP_SWPS_OFFLOAD)
void* deque(struct rtl8192cd_priv *priv, struct apsd_pkt_queue* AC_dz_queue, struct reprepare_info* pkt_info, int *head, int *tail, unsigned long ffptr, int ffsize)
#else
void* deque(struct rtl8192cd_priv *priv, int *head, int *tail, unsigned long ffptr, int ffsize)
#endif
{
	// critical section!
	unsigned int  i;
	unsigned int  amsdu_i; 
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	void *elm;
	
	SAVE_INT_AND_CLI(flags);
	if (CIRC_CNT(*head, *tail, ffsize) == 0) {
		RESTORE_INT(flags);
		return NULL;
	}

	i = *tail;
	*tail = (*tail + 1) & (ffsize - 1);
	elm = (void*)(*(unsigned long *)(ffptr + i*(sizeof(void *))));
#if defined(AP_SWPS_OFFLOAD)
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv)) {
        if(AC_dz_queue && pkt_info){
            pkt_info->amsdu_flag = AC_dz_queue->amsdu_flag[i];
            pkt_info->hdr = AC_dz_queue->hdr[i];
            pkt_info->llchdr = AC_dz_queue->llchdr[i];
            pkt_info->hdr_len = AC_dz_queue->hdr_len[i];
            pkt_info->icv = AC_dz_queue->icv[i];
            pkt_info->mic = AC_dz_queue->mic[i];
            pkt_info->mic_icv_len = AC_dz_queue->mic_icv_len[i];
            pkt_info->skbbuf_len = AC_dz_queue->skbbuf_len[i];
            pkt_info->ptxdesc = AC_dz_queue->ptxdesc[i];
            pkt_info->swps_seq = AC_dz_queue->swps_seq[i];
            pkt_info->pSkb = (struct sk_buff *)elm;
            pkt_info->PSB_len = AC_dz_queue->PSB_len[i];
            pkt_info->isWiFiHdr = AC_dz_queue->isWiFiHdr[i];
#if CFG_HAL_SUPPORT_TXDESC_IE
            pkt_info->IE_bitmap= AC_dz_queue->IE_bitmap[i];
#endif
#ifdef WLAN_HAL_TX_AMSDU            
            pkt_info->amsdu_num = AC_dz_queue->amsdu_num[i];
            for(amsdu_i=0;amsdu_i < pkt_info->amsdu_num;amsdu_i++)
                pkt_info->amsdubuf_len[amsdu_i]= AC_dz_queue->amsdubuf_len[i][amsdu_i];
#endif                
            AC_dz_queue->swps_seq[i] = 0;
            AC_dz_queue->amsdu_flag[i] = 0;
            AC_dz_queue->hdr[i] = NULL;
            AC_dz_queue->llchdr[i] = NULL;
            AC_dz_queue->icv[i] = NULL;
            AC_dz_queue->mic[i] = NULL;
            AC_dz_queue->hdr_len[i] = 0;
            AC_dz_queue->skbbuf_len[i] = 0;
            AC_dz_queue->mic_icv_len[i] = 0;
            AC_dz_queue->ptxdesc[i] = NULL;
            AC_dz_queue->PSB_len[i] = 0;
            AC_dz_queue->isWiFiHdr[i] = 0;
#if CFG_HAL_SUPPORT_TXDESC_IE            
            AC_dz_queue->IE_bitmap[i] = 0;
#endif
#ifdef WLAN_HAL_TX_AMSDU            
            AC_dz_queue->amsdu_num[i] = 0;
            for(amsdu_i=0;amsdu_i < pkt_info->amsdu_num;amsdu_i++)
                AC_dz_queue->amsdubuf_len[i][amsdu_i]=0;
#endif            
        }
    }
#endif    
    
	RESTORE_INT(flags);
	
	return elm;
}

#ifdef CONFIG_DLINK_SIOCGIWRATE_FIX
/* from cfg80211 util.c (kernel 4.12) without 60G */
enum rate_info_flags {
	RATE_INFO_FLAGS_MCS			= BIT(0),
	RATE_INFO_FLAGS_VHT_MCS			= BIT(1),
	RATE_INFO_FLAGS_SHORT_GI		= BIT(2),
};

enum rate_info_bw {
	RATE_INFO_BW_20 = 0,
	RATE_INFO_BW_5,
	RATE_INFO_BW_10,
	RATE_INFO_BW_40,
	RATE_INFO_BW_80,
	RATE_INFO_BW_160,
};

struct rate_info {
	u8 flags;
	u8 mcs;
	u16 legacy;
	u8 nss;
	u8 bw;
};

static u32 cfg80211_calculate_bitrate_ht(struct rate_info *rate)
{
    int modulation, streams, bitrate;

    /* the formula below does only work for MCS values smaller than 32 */
    if (WARN_ON_ONCE(rate->mcs >= 32)) 
        return 0;

    modulation = rate->mcs & 7; 
    streams = (rate->mcs >> 3) + 1; 

    bitrate = (rate->bw == RATE_INFO_BW_40) ? 13500000 : 6500000;

    if (modulation < 4) 
        bitrate *= (modulation + 1);
    else if (modulation == 4)
        bitrate *= (modulation + 2);
    else 
        bitrate *= (modulation + 3);

    bitrate *= streams;

    if (rate->flags & RATE_INFO_FLAGS_SHORT_GI)
        bitrate = (bitrate / 9) * 10;

    /* do NOT round down here */
    return (bitrate + 50000) / 100000;
}

static u32 cfg80211_calculate_bitrate_vht(struct rate_info *rate)
{
	static const u32 base[4][10] = {
		{   6500000,
		   13000000,
		   19500000,
		   26000000,
		   39000000,
		   52000000,
		   58500000,
		   65000000,
		   78000000,
		   /* not in the spec, but some devices use this: */
		   86500000,
		},
		{  13500000,
		   27000000,
		   40500000,
		   54000000,
		   81000000,
		  108000000,
		  121500000,
		  135000000,
		  162000000,
		  180000000,
		},
		{  29300000,
		   58500000,
		   87800000,
		  117000000,
		  175500000,
		  234000000,
		  263300000,
		  292500000,
		  351000000,
		  390000000,
		},
		{  58500000,
		  117000000,
		  175500000,
		  234000000,
		  351000000,
		  468000000,
		  526500000,
		  585000000,
		  702000000,
		  780000000,
		},
	};
	u32 bitrate;
	int idx;

	if (WARN_ON_ONCE(rate->mcs > 9))
		return 0;

	switch (rate->bw) {
	case RATE_INFO_BW_160:
		idx = 3;
		break;
	case RATE_INFO_BW_80:
		idx = 2;
		break;
	case RATE_INFO_BW_40:
		idx = 1;
		break;
	case RATE_INFO_BW_5:
	case RATE_INFO_BW_10:
	default:
		WARN_ON(1);
		/* fall through */
	case RATE_INFO_BW_20:
		idx = 0;
	}

	bitrate = base[idx][rate->mcs];
	bitrate *= rate->nss;

	if (rate->flags & RATE_INFO_FLAGS_SHORT_GI)
		bitrate = (bitrate / 9) * 10;

	/* do NOT round down here */
	return (bitrate + 50000) / 100000;
}

static u32 cfg80211_calculate_bitrate(struct rate_info *rate)
{
	if (rate->flags & RATE_INFO_FLAGS_MCS)
		return cfg80211_calculate_bitrate_ht(rate);
	if (rate->flags & RATE_INFO_FLAGS_VHT_MCS)
		return cfg80211_calculate_bitrate_vht(rate);

	return rate->legacy;
}

u32 find_rate_dlink(struct rtl8192cd_priv *priv)
{
	int i;
	struct rate_info info = {0};
	unsigned bw, mode;

	/* set nss */
	mode = get_rf_mimo_mode(priv);
	if (mode == MIMO_2T2R)
		info.nss = 2;
	else if (mode == MIMO_3T3R)
		info.nss = 3;
	else if (mode == MIMO_4T4R)
		info.nss = 4;
	else
		info.nss = 1;

	/* set bw and SGI */
	bw = priv->pmib->dot11nConfigEntry.dot11nUse40M;

	switch (bw) {
#ifdef RTK_AC_SUPPORT
	case CHANNEL_WIDTH_80:
	case CHANNEL_WIDTH_80_80:
		info.bw = RATE_INFO_BW_80;
		if (cpu_to_le32(priv->vht_cap_buf.vht_cap_info) & BIT(SHORT_GI80M_S))
			info.flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;
#endif
	case CHANNEL_WIDTH_40:
		info.bw = RATE_INFO_BW_40;
		if (priv->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_40M_))
			info.flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;
	case CHANNEL_WIDTH_20:
		info.bw = RATE_INFO_BW_20;
		if (priv->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_20M_))
			info.flags |= RATE_INFO_FLAGS_SHORT_GI;
		break;
	default:
		/* unsupported value */
		return 0; 
	}

	/* set mcs or legacy speed */
#ifdef RTK_AC_SUPPORT
	if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11AC) {
		u8 offset = 2 * (info.nss - 1);
		u8 supported_mcs =
			(cpu_to_le32(priv->vht_cap_buf.vht_support_mcs[0]) >> offset) & 0x3;

		if (supported_mcs == 0x0)
			info.mcs = 7;
		else if (supported_mcs == 0x1)
			info.mcs = 8;
		else if (supported_mcs == 0x2)
			info.mcs = 9;
		else	/* invalid value */
			return 0;

		info.flags |= RATE_INFO_FLAGS_VHT_MCS;
	} else
#endif
	if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) {
		info.mcs = info.nss * 8 - 1;
		info.flags |= RATE_INFO_FLAGS_MCS;
	} else if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11G)
		info.legacy = 540;
	else if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11A)
		info.legacy = 540;
	else if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11B)
		info.legacy = 110;
	else	/* unknown type */
		return 0;

	//printk("nss:%d bw:%d flags:%x mcs:%d leg: %d\n",
	//		info.nss, info.bw, info.flags, info.mcs, info.legacy);
	return cfg80211_calculate_bitrate(&info);

}
#endif /* CONFIG_DLINK_SIOCGIWRATE_FIX */

void initque(struct rtl8192cd_priv *priv, int *head, int *tail)
{
	// critical section!
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);
	*head = *tail = 0;
	RESTORE_INT(flags);
}


int	isFFempty(int head, int tail)
{
	return (head == tail);
}

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
void _init_txservq(struct tx_servq *ptxservq, int q_num)
{
	_rtw_init_listhead(&ptxservq->tx_pending);
	_rtw_init_queue(&ptxservq->xframe_queue);
	ptxservq->q_num = q_num;
#ifdef CONFIG_SDIO_HCI
	if (GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
		ptxservq->ts_used = 0;
	}
#endif
}

#ifdef CONFIG_TCP_ACK_TXAGG
void _init_tcpack_servq(struct tcpack_servq *tcpackq, int q_num)
{
	_rtw_init_listhead(&tcpackq->tx_pending);
	_rtw_init_queue(&tcpackq->xframe_queue);
	tcpackq->q_num = q_num;
}
#endif
#endif // CONFIG_USB_HCI || CONFIG_SDIO_HCI

#ifdef CONFIG_RTK_MESH
unsigned int find_rate_MP(struct rtl8192cd_priv *priv, struct stat_info *pstat, struct ht_cap_elmt * peer_ht_cap,  int peer_ht_cap_len, char *peer_rate,int peer_rate_len,int mode, int isBasicRate)
{
	unsigned int len, i, hirate, lowrate, rate_limit, OFDM_only=0;
	unsigned char *rateset, *p;

	if ((get_rf_mimo_mode(priv)== RF_1T2R) || (get_rf_mimo_mode(priv)== RF_1T1R)) //eric-8814 ?? 3t3r ??
		rate_limit = 8;
	else if (get_rf_mimo_mode(priv)== RF_2T2R)
		rate_limit = 16;
	else if (get_rf_mimo_mode(priv)== RF_3T3R)
		rate_limit = 24; 
	else
		rate_limit = 16;

	if (pstat) {
		rateset = pstat->bssrateset;
		len = pstat->bssratelen;
	}
	else {
		rateset = peer_rate;
		len = peer_rate_len;
	}

	hirate = _1M_RATE_;
	lowrate = _54M_RATE_;
	if (priv->pshare->curr_band == BAND_5G)
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
		if ((mode == 1) && (isBasicRate == 0) && pstat->ht_cap_len) {
			for (i=0; i<rate_limit; i++)
			{
				if (pstat->ht_cap_buf.support_mcs[i/8] & BIT(i%8)) {
					hirate = i;
					hirate += HT_RATE_ID;
				}
			}
		}
	}
	else {
		if ((mode == 1) && (isBasicRate == 0) && priv->ht_cap_len && peer_ht_cap_len) {
			for (i=0; i<rate_limit; i++)
			{
				if (peer_ht_cap->support_mcs[i/8] & BIT(i%8)) {
					hirate = i;
					hirate += HT_RATE_ID;
				}
			}
		}
	}

	if (mode == 0)
		return lowrate;
	else
		return hirate;
}

#endif











int get_bit_value_from_ieee_value(UINT8 val)
{
	int i=0;
	while(dot11_rate_table[i] != 0) {
		if (dot11_rate_table[i] == val)
			return BIT(i);
		i++;
	}
	return 0;
}





#if 1 //register driver info for RA
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
extern BOOLEAN Get_RA_ShortGI(struct rtl8192cd_priv *priv, struct stat_info *pEntry, IN WIRELESS_MODE WirelessMode, IN u1Byte ChnlBW);
#else
BOOLEAN
Get_RA_ShortGI(
	struct rtl8192cd_priv *priv,
	struct stat_info *		pEntry,
	IN	WIRELESS_MODE		WirelessMode,
	IN	u1Byte				ChnlBW
)
{
	BOOLEAN						bShortGI;

	BOOLEAN	bShortGI20MHz = FALSE, bShortGI40MHz = FALSE, bShortGI80MHz = FALSE;

	if (	WirelessMode & WIRELESS_MODE_N_24G ||
			WirelessMode & WIRELESS_MODE_N_5G ||
			WirelessMode & WIRELESS_MODE_AC_24G ||
			WirelessMode & WIRELESS_MODE_AC_5G  ) {
		if ((pEntry->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_20M_) ||
				(priv->pshare->rf_ft_var.sgi_vw_force && pEntry->ht_cap_len)) &&
				priv->pmib->dot11nConfigEntry.dot11nShortGIfor20M) {
			bShortGI20MHz = TRUE;
		}
		if ((pEntry->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_SHORTGI_40M_) ||
				(priv->pshare->rf_ft_var.sgi_vw_force && pEntry->ht_cap_len)) &&
				priv->pmib->dot11nConfigEntry.dot11nShortGIfor40M) {
			bShortGI40MHz = TRUE;
		}
     #ifdef RTK_AC_SUPPORT
		if ((WirelessMode & WIRELESS_MODE_AC_5G) && (GET_CHIP_VER(priv) != VERSION_8723B)) {
			if (((cpu_to_le32(pEntry->vht_cap_buf.vht_cap_info) & BIT(SHORT_GI80M_E)) ||
					(priv->pshare->rf_ft_var.sgi_vw_force && pEntry->vht_cap_len)) &&
					priv->pmib->dot11nConfigEntry.dot11nShortGIfor80M)
			bShortGI80MHz = TRUE;
#if 0
// disable SGI when LDPC is enabled in AC mode
			if((pEntry->is_realtek_sta) && (priv->pmib->dot11nConfigEntry.dot11nLDPC == 1) && 
				((pEntry->ht_cap_len && cpu_to_le16(pEntry->ht_cap_buf.ht_cap_info) & _HTCAP_SUPPORT_RX_LDPC_) ||
					(pEntry->vht_cap_len && (cpu_to_le32(pEntry->vht_cap_buf.vht_cap_info) & BIT(RX_LDPC_E))))	) {
					bShortGI80MHz = bShortGI40MHz = bShortGI20MHz = 0;
				}
#endif			
		}
	#endif
	}

	switch (ChnlBW) {
		case CHANNEL_WIDTH_40:
			bShortGI = bShortGI40MHz;
			break;
		case CHANNEL_WIDTH_80:
			bShortGI = bShortGI80MHz;
			break;
		default:
		case CHANNEL_WIDTH_20:
			bShortGI = bShortGI20MHz;
			break;
	}

	if (is_veriwave_testing(priv))
		bShortGI = TRUE;

	return bShortGI;
}
#endif
#if 0
u1Byte get_wirelessmode(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{

	u1Byte		        WirelessMode    = WIRELESS_MODE_A;

#ifdef RTK_AC_SUPPORT
	if(pstat->vht_cap_len && (priv->pmib->dot11BssType.net_work_type & WIRELESS_11AC) && (!should_restrict_Nrate(priv, pstat))) {
		WirelessMode = WIRELESS_MODE_AC_5G;
	} 
        else 
#endif
    if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) && pstat->ht_cap_len && (!should_restrict_Nrate(priv, pstat))) {
		if(priv->pmib->dot11BssType.net_work_type & WIRELESS_11A)
			WirelessMode = WIRELESS_MODE_N_5G;
		else
			WirelessMode = WIRELESS_MODE_N_24G;

		if (pstat->ht_cap_buf.support_mcs[4] == 0) // no support RX 5ss
			sta_mimo_mode = RF_4T4R;
		if (pstat->ht_cap_buf.support_mcs[3] == 0) // no support RX 4ss
			sta_mimo_mode = RF_3T3R;
		if (pstat->ht_cap_buf.support_mcs[2] == 0) // no support RX 3ss
			sta_mimo_mode = RF_2T2R;
		if (pstat->ht_cap_buf.support_mcs[1] == 0) // no support RX 2ss
			sta_mimo_mode = RF_1T1R;

		if (get_rf_NTx(sta_mimo_mode) < get_rf_NTx(rf_mimo_mode))
			rf_mimo_mode = sta_mimo_mode;

#ifdef RTK_AC_SUPPORT
		if (pstat->nss == 1)
		sta_mimo_mode_nss = RF_1T1R;
		else if (pstat->nss == 2)
		sta_mimo_mode_nss = RF_2T2R;

		if ((sta_mimo_mode_nss != 0) && (get_rf_NTx(sta_mimo_mode_nss) < get_rf_NTx(rf_mimo_mode)))
			rf_mimo_mode = sta_mimo_mode_nss;
#endif
        
	}
	else if (((priv->pmib->dot11BssType.net_work_type & WIRELESS_11G) && isErpSta(pstat))){
			WirelessMode = WIRELESS_MODE_G;		
	}
    else if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11A) &&
	((priv->pmib->dot11OperationEntry.opmode & WIFI_AP_STATE) || (priv->pmib->dot11RFEntry.phyBandSelect & PHY_BAND_5G))) {
			WirelessMode = WIRELESS_MODE_A;		
	}
	else if(priv->pmib->dot11BssType.net_work_type & WIRELESS_11B){
		WirelessMode = WIRELESS_MODE_B;		
	}
#if defined(AC2G_256QAM) || defined(CONFIG_WLAN_HAL_8814AE)
	else if (is_ac2g(priv) && pstat->vht_cap_len) {
		WirelessMode = WIRELESS_MODE_AC_24G;
	}
#endif

	pstat->cmn_info.mimo_type = rf_mimo_mode;

	return WirelessMode;
	
}
#endif
void init_rainfo(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	//struct wifi_mib *pmib = priv->pmib;
	unsigned long	offset;
	u1Byte		        WirelessMode    = 0;
	u1Byte		        BW              = CHANNEL_WIDTH_20;
	u1Byte			MimoPs = MIMO_PS_NOLIMIT;
	unsigned short	bk_aid;
	unsigned char		bk_hwaddr[MACADDRLEN];
	BOOLEAN 			bShortGI		= FALSE;
	u1Byte				rf_mimo_mode	= get_rf_mimo_mode(priv);
	u1Byte		sta_mimo_mode;
	u1Byte		sta_mimo_mode_nss = 0;
	int i;
	u8Byte ratr_bitmap = 0;
	BOOLEAN		bCurTxBW80MHz=FALSE, bCurTxBW40MHz=FALSE;

	pstat->cmn_info.dm_ctrl |= STA_DM_CTRL_ACTIVE;
	phydm_cmn_sta_info_hook(ODMPTR, pstat->cmn_info.mac_id, &pstat->cmn_info);
	phydm_sta_rssi_init(ODMPTR, pstat->cmn_info.mac_id, pstat->rssi);
	
	if(pstat->MIMO_ps & _HT_MIMO_PS_STATIC_)
		MimoPs = MIMO_PS_STATIC;
	else if(pstat->MIMO_ps & _HT_MIMO_PS_DYNAMIC_)
		MimoPs = MIMO_PS_DYNAMIC;
	
	pstat->cmn_info.sm_ps = MimoPs;
#if 1
	BW = pstat->tx_bw;

	if( BW > (priv->pshare->CurrentChannelBW))
		BW = priv->pshare->CurrentChannelBW;
#endif


#if defined(RTK_AC_SUPPORT) || defined(RTK_AC_TX_SUPPORT)
		if(pstat->vht_cap_len && ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11AC) || priv->pshare->phw->is_AC_TX_support) && (!should_restrict_Nrate(priv, pstat))) {

			if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_2G)
				WirelessMode = WIRELESS_MODE_AC_24G;
			else
			WirelessMode = WIRELESS_MODE_AC_5G;
			if(((le32_to_cpu(pstat->vht_cap_buf.vht_support_mcs[0])>>8)&3)==3) // no support RX 5ss
				sta_mimo_mode = RF_4T4R;
			if(((le32_to_cpu(pstat->vht_cap_buf.vht_support_mcs[0])>>6)&3)==3) // no support RX 4ss
				sta_mimo_mode = RF_3T3R;
			if(((le32_to_cpu(pstat->vht_cap_buf.vht_support_mcs[0])>>4)&3)==3) // no support RX 3ss
				sta_mimo_mode = RF_2T2R;
			if(((le32_to_cpu(pstat->vht_cap_buf.vht_support_mcs[0])>>2)&3)==3) // no support RX 2ss
				sta_mimo_mode = RF_1T1R;

			pstat->sta_mimo_mode = sta_mimo_mode;
			pstat->cmn_info.ra_info.is_vht_enable = 1;
			if (get_rf_NTx(sta_mimo_mode) < get_rf_NTx(rf_mimo_mode))
				rf_mimo_mode = sta_mimo_mode;

			if (pstat->nss == 1)
				sta_mimo_mode_nss = RF_1T1R;
			else if (pstat->nss == 2)
				sta_mimo_mode_nss = RF_2T2R;
			else if (pstat->nss == 3)
				sta_mimo_mode_nss = RF_3T3R;
			else if (pstat->nss == 4)
				sta_mimo_mode_nss = RF_4T4R;

			if ((sta_mimo_mode_nss != 0) && (get_rf_NTx(sta_mimo_mode_nss) < get_rf_NTx(rf_mimo_mode)))
				rf_mimo_mode = sta_mimo_mode_nss;

            if(priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_2G){
				ratr_bitmap |= 0xfff;
            }else{	
			ratr_bitmap |= 0xff0;
            }
			ratr_bitmap |= RateToBitmap_VHT88XX((pu1Byte)&(pstat->vht_cap_buf.vht_support_mcs[0]), rf_mimo_mode) << 12;

			if(rf_mimo_mode == RF_1T1R)
				ratr_bitmap &= 0x003fffff;
			else if(rf_mimo_mode == RF_3T3R)
				ratr_bitmap &= 0x3FFFFFFFFFFULL;			// 3SS MCS9
			else if(rf_mimo_mode == RF_4T4R)
				ratr_bitmap &= 0xFFFFFFFFFFFFFULL;			// 4SS MCS9	
			else
				ratr_bitmap &= 0xFFFFFFFF;			// 2SS MCS9
				
			if(BW==CHANNEL_WIDTH_80)
				bCurTxBW80MHz = TRUE;
}
			else 
#endif
		if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) && pstat->ht_cap_len && (!should_restrict_Nrate(priv, pstat))) {
			if(priv->pmib->dot11BssType.net_work_type & WIRELESS_11A)
				WirelessMode = WIRELESS_MODE_N_5G;
			else
				WirelessMode = WIRELESS_MODE_N_24G;
	
			if (pstat->ht_cap_buf.support_mcs[4] == 0) // no support RX 5ss
				sta_mimo_mode = RF_4T4R;
			if (pstat->ht_cap_buf.support_mcs[3] == 0) // no support RX 4ss
				sta_mimo_mode = RF_3T3R;
			if (pstat->ht_cap_buf.support_mcs[2] == 0) // no support RX 3ss
				sta_mimo_mode = RF_2T2R;
			if (pstat->ht_cap_buf.support_mcs[1] == 0) // no support RX 2ss
				sta_mimo_mode = RF_1T1R;

			
			if (get_rf_NTx(sta_mimo_mode) < get_rf_NTx(rf_mimo_mode))
				rf_mimo_mode = sta_mimo_mode;
			

			for (i=0; i<32; i++) {
				if (pstat->bssrateset[i])
					ratr_bitmap |= get_bit_value_from_ieee_value(pstat->bssrateset[i]&0x7f);
			}
			
			ratr_bitmap |= ((((u8Byte)pstat->ht_cap_buf.support_mcs[3]) << 36) | (((u8Byte)pstat->ht_cap_buf.support_mcs[2]) << 28) | (((u8Byte)pstat->ht_cap_buf.support_mcs[1]) << 20) | (((u8Byte)pstat->ht_cap_buf.support_mcs[0]) << 12));
	
			

			
#ifdef RTK_AC_SUPPORT
			if (pstat->nss == 1)
			sta_mimo_mode_nss = RF_1T1R;
			else if (pstat->nss == 2)
			sta_mimo_mode_nss = RF_2T2R;
	
			if ((sta_mimo_mode_nss != 0) && (get_rf_NTx(sta_mimo_mode_nss) < get_rf_NTx(rf_mimo_mode)))
				rf_mimo_mode = sta_mimo_mode_nss;
#endif
			
		} else {
			ratr_bitmap = 0xfff;
		}

		if (((priv->pmib->dot11BssType.net_work_type & WIRELESS_11G) && isErpSta(pstat))){
				WirelessMode |= WIRELESS_MODE_G;
				for (i=0; i<32; i++) {
					if (pstat->bssrateset[i])
						ratr_bitmap |= get_bit_value_from_ieee_value(pstat->bssrateset[i]&0x7f);
				}
		}
		if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11A) &&
		((priv->pmib->dot11OperationEntry.opmode & WIFI_AP_STATE) || (priv->pmib->dot11RFEntry.phyBandSelect & PHY_BAND_5G))) {
				WirelessMode |= WIRELESS_MODE_A;
				for (i=0; i<32; i++) {
					if (pstat->bssrateset[i])
						ratr_bitmap |= get_bit_value_from_ieee_value(pstat->bssrateset[i]&0x7f);
				}
		}
		if(priv->pmib->dot11BssType.net_work_type & WIRELESS_11B){
			WirelessMode |= WIRELESS_MODE_B;
			for (i=0; i<32; i++) {
				if (pstat->bssrateset[i])
					ratr_bitmap |= get_bit_value_from_ieee_value(pstat->bssrateset[i]&0x7f);
			}
		}
#if defined(AC2G_256QAM) || defined(CONFIG_WLAN_HAL_8814AE)
		if (is_ac2g(priv) && pstat->vht_cap_len) {
			WirelessMode |= WIRELESS_MODE_AC_24G;
		}
#endif
	
		pstat->cmn_info.mimo_type = rf_mimo_mode;

	//WirelessMode = get_wirelessmode(priv,pstat);
/********************/
	pstat->wireless_mode = WirelessMode;

	
	if ((WirelessMode & WIRELESS_MODE_AC_5G) || (WirelessMode & WIRELESS_MODE_AC_24G))
		pstat->cmn_info.support_wireless_set |= WIRELESS_VHT;
	if ((WirelessMode & WIRELESS_MODE_N_5G) || (WirelessMode & WIRELESS_MODE_N_24G))
		pstat->cmn_info.support_wireless_set |= WIRELESS_HT;
	if ((WirelessMode & WIRELESS_MODE_A) || (WirelessMode & WIRELESS_MODE_G))
		pstat->cmn_info.support_wireless_set |= WIRELESS_OFDM;
	if (WirelessMode & WIRELESS_MODE_B)
		pstat->cmn_info.support_wireless_set |= WIRELESS_CCK;

	if(
#if defined(RTK_AC_SUPPORT) || defined(RTK_AC_TX_SUPPORT)
		!pstat->vht_cap_len && 
#endif
		!pstat->ht_cap_len
	)
	{
			if(pstat->cmn_info.support_wireless_set&WIRELESS_CCK)
					ratr_bitmap |= 0xf;
			if(pstat->cmn_info.support_wireless_set & WIRELESS_OFDM)
					ratr_bitmap |= 0xff0;
	}
	
	pstat->cmn_info.ra_info.ramask = ratr_bitmap;

	if (priv->pshare->is_40m_bw && (BW == CHANNEL_WIDTH_40)
	#ifdef WIFI_11N_2040_COEXIST
		&& !(((((GET_MIB(priv))->dot11OperationEntry.opmode) & WIFI_AP_STATE)) 
		&& COEXIST_ENABLE
		&& (priv->bg_ap_timeout || orForce20_Switch20Map(priv)
		))
	#endif
	){
		bCurTxBW40MHz = TRUE;
	}
	
	if(((GET_MIB(priv))->dot11OperationEntry.opmode) & WIFI_STATION_STATE) {
		if(((GET_MIB(priv))->dot11Bss.t_stamp[1] & 0x6) == 0) {
			bCurTxBW40MHz = bCurTxBW80MHz = FALSE;
		}
	}
	
	if(bCurTxBW80MHz)
		BW = CHANNEL_WIDTH_80;
	else if(bCurTxBW40MHz)
		BW = CHANNEL_WIDTH_40;
	else
		BW = CHANNEL_WIDTH_20;

	bShortGI = Get_RA_ShortGI(priv, pstat, WirelessMode, BW);
	pstat->cmn_info.ra_info.is_support_sgi = bShortGI;
	pstat->cmn_info.bw_mode = BW;
	if (BW == CHANNEL_WIDTH_80_80)
		pstat->cmn_info.ra_info.ra_bw_mode = CHANNEL_WIDTH_160;
	else
		pstat->cmn_info.ra_info.ra_bw_mode = BW;


	phydm_ra_registed(ODMPTR, pstat->cmn_info.mac_id, pstat->rssi);
	
}
#endif


void init_stainfo(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	struct wifi_mib	*pmib = priv->pmib;
	unsigned long	offset;
	int i, j;
#if !defined(SMP_SYNC) && defined(CONFIG_RTL_WAPI_SUPPORT)
	unsigned long		flags;
#endif

#ifdef WDS
	static unsigned char bssrateset[32];
	unsigned int	bssratelen=0;
	unsigned int	current_tx_rate=0;
#endif
	unsigned short	bk_aid;
	unsigned char		bk_hwaddr[MACADDRLEN];

#ifdef CONFIG_FON
	int idx=wlan_find_idx_by_linux_name(priv->dev->name);
	Fonclient *pFonClient;
	if(is_FON_SSID(idx) && !FON_getClient_byMac(pstat->cmn_info.mac_addr, &pFonClient))
	{
		pFonClient =  (Fonclient *)malloc(sizeof(Fonclient));
		memset(pFonClient, 0, sizeof(Fonclient));
		memcpy(pFonClient->mac, pstat->cmn_info.mac_addr, MACADDRLEN);
		pFonClient->ssid_idx = idx;
		REDIR_get_challange(pFonClient->challange);
		INIT_LIST_HEAD(&(pFonClient->list));
		list_add_tail((struct list_head*)&pFonClient->list,&fonClientList);
	}
#endif

	// init linked list header
	// BUT do NOT init hash_list
	INIT_LIST_HEAD(&pstat->asoc_list);
	INIT_LIST_HEAD(&pstat->auth_list);
	INIT_LIST_HEAD(&pstat->sleep_list);
	INIT_LIST_HEAD(&pstat->defrag_list);
	INIT_LIST_HEAD(&pstat->wakeup_list);
	INIT_LIST_HEAD(&pstat->frag_list);

#ifdef CONFIG_PCI_HCI
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
		// to avoid add RAtid fail
		INIT_LIST_HEAD(&pstat->addRAtid_list);
		INIT_LIST_HEAD(&pstat->addrssi_list);
	}
#endif

#ifdef CONFIG_RTK_MESH
	INIT_LIST_HEAD(&pstat->mesh_mp_ptr);
#endif	// CONFIG_RTK_MESH

#ifdef A4_STA
	INIT_LIST_HEAD(&pstat->a4_sta_list);
#endif
#if defined(CONFIG_PCI_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
		skb_queue_head_init(&pstat->dz_queue);
#if defined(AP_SWPS_OFFLOAD)
	    if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv))
	        INIT_LIST_HEAD(&pstat->dz_list_head);
#endif
	}
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
		INIT_LIST_HEAD(&pstat->pspoll_list);

		for (i = 0; i < MAX_STA_TX_SERV_QUEUE; ++i) {
			_init_txservq(&pstat->tx_queue[i], i);
#ifdef CONFIG_TCP_ACK_TXAGG
			_init_tcpack_servq(&pstat->tcpack_queue[i], i);
#endif
		}
		
		pstat->pending_cmd = 0;
		pstat->asoc_list_refcnt = 0;
#ifdef __ECOS
		cyg_flag_init(&pstat->asoc_unref_done);
#else
		init_completion(&pstat->asoc_unref_done);
#endif
	}
#endif

#ifdef STA_CONTROL
	pstat->stactrl_trigger_time = 0;
	pstat->kickoff_time = 0;
#endif
	pstat->cnt_sleep = 0;

#ifdef DOT11K
    pstat->rm_timer.data = (unsigned long) pstat;        
    pstat->rm_timer.function = rm_beacon_expire;          
    init_timer(&pstat->rm_timer);
#endif

#ifdef ROAMING_SUPPORT
	pstat->roaming_indication = 0;
	pstat->roaming_waitcount = 0;
#endif


	// we do NOT reset MAC here

#if defined(WIFI_WMM)
#ifdef DZ_ADDBA_RSP
	pstat->dz_addba.used = 0;
#endif
#endif

#ifdef WDS
	if (pstat->state & WIFI_WDS) {
		bssratelen = pstat->bssratelen;
		memcpy(bssrateset, pstat->bssrateset, bssratelen);
		current_tx_rate = pstat->current_tx_rate;
	}
#endif

    // TODO HAL by Eric, clear HWTX shorcut buffer
#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
    if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(priv)) {
        #if IS_RTL88XX_MAC_V2_V3
            if(_GET_HAL_DATA(priv)->MacVersion.is_MAC_v2_v3)
            GET_HAL_INTERFACE(priv)->ClearHWTXShortcutBufHandler(priv,REMAP_AID(pstat)); 
#endif
#if IS_RTL88XX_MAC_V4
            if(_GET_HAL_DATA(priv)->MacVersion.is_MAC_v4){
                u1Byte smhcam_idx;
                smhcam_idx = GET_HAL_INTERFACE(priv)->GetSmhCamHandler(priv,pstat);  
                if(smhcam_idx != SMHCAM_SEARCH_FAIL)
                    priv->pshare->SmhInfo[smhcam_idx].used = 0;
            }

#endif               
    }
#endif //WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
    
	// zero out all the rest
	bk_aid = pstat->cmn_info.aid;
	memcpy(bk_hwaddr, pstat->cmn_info.mac_addr, MACADDRLEN);

	offset = (unsigned long)(&((struct stat_info *)0)->auth_seq);
	memset((void *)((unsigned long)pstat + offset), 0, sizeof(struct stat_info)-offset);
	pstat->cmn_info.ra_info.disable_ra = 1;
	pstat->cmn_info.aid = bk_aid;
	memcpy(pstat->cmn_info.mac_addr, bk_hwaddr, MACADDRLEN);

#ifdef WDS
	if (bssratelen) {
		pstat->bssratelen = bssratelen;
		memcpy(pstat->bssrateset, bssrateset, bssratelen);
		pstat->current_tx_rate = current_tx_rate;
		pstat->state |= WIFI_WDS;
	}
#endif

	// some variables need initial value
	pstat->ieee8021x_ctrlport = pmib->dot118021xAuthEntry.dot118021xDefaultPort;
	pstat->expire_to = priv->expire_to;	
	for (i=0; i<8; i++)
		for (j=0; j<TUPLE_WINDOW; j++)
			pstat->tpcache[i][j] = 0xffff;
			// Stanldy mesh: pstat->tpcache[i][j] = j+1 is best solution, because its a hash table, fill slot[i] with i+1 can prevent collision,fix the packet loss of first unicast
	pstat->tpcache_mgt = 0xffff;
#ifdef CLIENT_MODE
	pstat->tpcache_mcast = 0xffff;
#endif
#ifdef GBWC
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
		for (i=0; i<priv->pmib->gbwcEntry.GBWCNum; i++) {
			if (!memcmp(pstat->cmn_info.mac_addr, priv->pmib->gbwcEntry.GBWCAddr[i], MACADDRLEN)) {
				pstat->GBWC_in_group = TRUE;
				break;
			}
		}
	}
#endif

// button 2009.05.21
#ifdef INCLUDE_WPA_PSK
	pstat->wpa_sta_info->clientHndshkProcessing = pstat->wpa_sta_info->clientHndshkDone = FALSE;
	pstat->wpa_sta_info->clientHndshkRekey = FALSE;
#endif

#ifdef CONFIG_RTK_MESH
	pstat->mesh_neighbor_TBL.BSexpire_LLSAperiod = jiffies + MESH_EXPIRE_TO;
#endif	//CONFIG_RTK_MESH

	memset(pstat->rc_entry, 0, sizeof(pstat->rc_entry));

	SMP_LOCK_XMIT(flags);
#ifdef RTK_ATM
	if(priv->pshare->rf_ft_var.atm_en){
		for(i=BK_QUEUE;i<HIGH_QUEUE;i++) {
			pstat->atm_swq.q_aggnum[i] = 2;
			skb_queue_head_init(&pstat->atm_swq.swq_queue[i]);
		}
	}
#endif

#ifdef SW_TX_QUEUE
	for(i=BK_QUEUE;i<HIGH_QUEUE;i++) {
		pstat->swq.q_aggnum[i] = priv->pshare->rf_ft_var.swq_aggnum;
		skb_queue_head_init(&pstat->swq.swq_queue[i]);
	}
#endif

#ifdef SUPPORT_TX_AMSDU
	for (i=0; i<8; i++)
		skb_queue_head_init(&pstat->amsdu_tx_que[i]);
#endif

#ifdef TCP_ACK_ACC
	INIT_LIST_HEAD(&pstat->tcp_ses_list);
	memset(pstat->tcp_ses, 0, sizeof(pstat->tcp_ses));
	for (i = 0; i < TCP_SESSION_MAX_ENTRY ; i++) {
		INIT_LIST_HEAD(&(pstat->tcp_ses_active_list[i]));
		list_add_tail(&pstat->tcp_ses[i].list, &pstat->tcp_ses_list);
	}
#endif
#ifdef SUPPORT_TX_AMSDU_SHORTCUT
	pstat->tx_sc_amsdu_replace_idx = TX_SC_ENTRY_NUM;
	pstat->tx_sc_amsdu_idx_cache = TX_SC_ENTRY_NUM;
#endif
	SMP_UNLOCK_XMIT(flags);

#if defined (HW_ANT_SWITCH) && (defined(CONFIG_RTL_92C_SUPPORT) || defined(CONFIG_RTL_92D_SUPPORT))
	pstat->CurAntenna = priv->pshare->rf_ft_var.CurAntenna;
#endif

#ifdef CONFIG_IEEE80211W
	init_timer(&pstat->SA_timer);
	pstat->SA_timer.data = (unsigned long) pstat;
	pstat->SA_timer.function = rtl8192cd_sa_query_timer;
#endif

#ifdef CONFIG_RTL_WAPI_SUPPORT
//	if (priv->pmib->wapiInfo.wapiType!=wapiDisable)
	{
		SAVE_INT_AND_CLI(flags);
//		wapiAssert(pstat->wapiInfo!=NULL);
		if (pstat->wapiInfo==NULL)
		{
			pstat->wapiInfo = (wapiStaInfo*)kmalloc(sizeof(wapiStaInfo), GFP_ATOMIC);
			if (pstat->wapiInfo==NULL)
			{
				printk("Err: kmalloc wapiStaInfo fail!\n");
			}
		}
		if (pstat->wapiInfo!=NULL)
		{
			pstat->wapiInfo->priv = priv;
			wapiStationInit(pstat);
			pstat->wapiInfo->wapiType = priv->pmib->wapiInfo.wapiType;
		}
		RESTORE_INT(flags);
	}
#endif

#ifdef MULTI_MAC_CLONE
	if ((OPMODE & WIFI_STATION_STATE) && priv->pmib->ethBrExtInfo.macclone_enable) 
	{
		pstat->mclone_id = ACTIVE_ID;
		memcpy(pstat->sa_addr, GET_MY_HWADDR, MACADDRLEN);		
	}
#endif

#ifdef RTK_ATM	
	pstat->atm_sta_time = 0;
	pstat->atm_match_sta_time = 0;
#endif

#ifdef SBWC
	pstat->SBWC_mode = SBWC_MODE_DISABLE;
	pstat->SBWC_tx_limit = 0;	
	pstat->SBWC_rx_limit = 0;
	pstat->SBWC_tx_limit_byte = 0;	
	pstat->SBWC_rx_limit_byte = 0;
	pstat->SBWC_tx_count = 0;
	pstat->SBWC_rx_count = 0;
	pstat->SBWC_txq.head = 0;
	pstat->SBWC_txq.tail = 0;
	pstat->SBWC_rxq.head = 0;
	pstat->SBWC_rxq.tail = 0;
	pstat->SBWC_consuming_q = FALSE;

	for (i = 0 ; i != priv->pmib->sbwcEntry.count ; ++i)
	{
		if (!memcmp(priv->pmib->sbwcEntry.entry[i].mac, pstat->cmn_info.mac_addr, MACADDRLEN))
		{
			pstat->SBWC_tx_limit = priv->pmib->sbwcEntry.entry[i].tx_lmt;
			pstat->SBWC_rx_limit = priv->pmib->sbwcEntry.entry[i].rx_lmt;
			pstat->SBWC_tx_limit_byte = ((pstat->SBWC_tx_limit * 1024 / 8) / (HZ / SBWC_TO));
			pstat->SBWC_rx_limit_byte = ((pstat->SBWC_rx_limit * 1024 / 8) / (HZ / SBWC_TO));
			break;
		}
	}
#endif

#ifdef AP_SWPS_OFFLOAD
    //init value = 0
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv)){
        free_swps_info(priv,pstat);
        pstat->EnSWPS = 1; //yllinSWPS, temp default set enable

        if(pstat->EnSWPS){
            u8 macid = pstat->cmn_info.aid;
            u8 macid_grp = macid/32;
            u8 macid_bit = macid%32;

            RTL_W8(REG_SWPS_CTRL, BIT_SET_MACID_SWPS_EN_SEL(RTL_R8(REG_SWPS_CTRL),macid_grp));
            RTL_W8(REG_MACID_SWPS_EN, RTL_R8(REG_MACID_SWPS_EN) | BIT(macid_bit));

            GET_HAL_INTERFACE(priv)->MACIDSWPSCtrlHandler(priv,pstat,pstat->EnSWPS);
        }
    }
#endif

#ifdef TX2_BK_QUEUE
	pstat->tx2bk_on = 0;
#endif

#ifdef CONFIG_SPECIAL_ENV_TEST
	pstat->sn_avg_gap = 0;
#endif

#ifdef ADDRESS_CAM
    if(IS_SUPPORT_ADDR_CAM(priv))
        pstat->addr_cam_entry = -1;
#endif

	pstat->cmn_info.ra_info.disable_pt = 1;
	pstat->cmn_info.ra_info.disable_ra = 1;

#if defined(CONFIG_WLAN_HAL_8814BE) && defined(CONFIG_RTL_OFFLOAD_DRIVER)
	IE0_SET_TX_DESC_IE_UP(pstat->txie[0], 1);
	IE0_SET_TX_DESC_IE_NUM(pstat->txie[0], 0);

	IE1_SET_TX_DESC_IE_UP(pstat->txie[1], 1);
	IE1_SET_TX_DESC_IE_NUM(pstat->txie[1], 1);

	IE2_SET_TX_DESC_IE_UP(pstat->txie[2], 1);
	IE2_SET_TX_DESC_IE_NUM(pstat->txie[2], 2);

	IE3_SET_TX_DESC_IE_UP(pstat->txie[3], 1);
	IE3_SET_TX_DESC_IE_NUM(pstat->txie[3], 3);

	IE4_SET_TX_DESC_IE_UP(pstat->txie[4], 1);
	IE4_SET_TX_DESC_IE_NUM(pstat->txie[4], 4);

    /* Skip IE5 at this moment */
	IE4_SET_TX_DESC_IE_END(pstat->txie[4], 1);
#endif

	// PN initialized to 1 when temporal key is initialized or refreshed
	pstat->dot11KeyMapping.dot11EncryptKey.dot11TXPN48.val48 = 1;
}

#if defined(CONFIG_RTL_ETH_PRIV_SKB_DEBUG)
void dump_sta_dz_queue_num(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#if defined(WIFI_WMM) && defined(WMM_APSD)
	int				hd, tl;
#endif

	// free all skb in dz_queue
	
	printk("---------------------------------------\n");
#if defined(CONFIG_PCI_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
		printk("pstat->dz_queue:%d\n",skb_queue_len(&pstat->dz_queue));
	}
#endif

#ifdef SW_TX_QUEUE
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
		printk("swq.be_queue:%d\n",skb_queue_len(&pstat->swq.swq_queue[BE_QUEUE]));
		printk("swq.bk_queue:%d\n",skb_queue_len(&pstat->swq.swq_queue[BK_QUEUE]));
		printk("swq.vi_queue:%d\n",skb_queue_len(&pstat->swq.swq_queue[VI_QUEUE]));
		printk("swq.vo_queue:%d\n",skb_queue_len(&pstat->swq.swq_queue[VO_QUEUE]));
	}
#endif

#if defined(WIFI_WMM) && defined(WMM_APSD)
	hd = pstat->VO_dz_queue->head;
	tl = pstat->VO_dz_queue->tail;
	printk("VO_dz_queue:%d\n",CIRC_CNT(hd, tl, NUM_APSD_TXPKT_QUEUE));
	hd = pstat->VI_dz_queue->head;
	tl = pstat->VI_dz_queue->tail;
	printk("VI_dz_queue:%d\n",CIRC_CNT(hd, tl, NUM_APSD_TXPKT_QUEUE));
	hd = pstat->BE_dz_queue->head;
	tl = pstat->BE_dz_queue->tail;
	printk("BE_dz_queue:%d\n",CIRC_CNT(hd, tl, NUM_APSD_TXPKT_QUEUE));
	hd = pstat->BK_dz_queue->head;
	tl = pstat->BK_dz_queue->tail;
	printk("BK_dz_queue:%d\n",CIRC_CNT(hd, tl, NUM_APSD_TXPKT_QUEUE));
#endif

#if defined(WIFI_WMM)
	hd = pstat->MGT_dz_queue->head;
	tl = pstat->MGT_dz_queue->tail;
	printk("BK_dz_queue:%d\n",CIRC_CNT(hd, tl, NUM_DZ_MGT_QUEUE));
#endif
	
	return;

}
#endif

#ifdef CONFIG_PCI_HCI
#if defined(AP_SWPS_OFFLOAD)
void free_dzqueue_seq(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
    struct list_head* dznode;
    struct dz_seq_node* node_tofree= NULL;
    while(pstat->dz_list_head.next != &pstat->dz_list_head){ //dz_queue
        dznode = pstat->dz_list_head.next;
        list_del_init(dznode);
        node_tofree = list_entry(dznode,struct dz_seq_node,node);
        kfree(node_tofree);
        node_tofree = NULL;
    }

    
}
void free_swps_info(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
    unsigned char qos_i = 0;
    int macid;

    //some need to clear, some need not, because new STA use the same stat will use the seq+1 instead of seq=1
    pstat->EnSWPS = 0;
    //pstat->SWPS_pkt_Qos = 0;
    pstat->check_drop_done = 0; 
    pstat->process_dz_ok = 0; //not sure if its ok
    pstat->next_prepare_ok = 1; //not sure if its ok
    pstat->PS_get_SWPSRPT = 0;
    
    for(qos_i=0;qos_i<4;qos_i++){
        pstat->SWPS_last_seq[qos_i] = 0;
        pstat->RPT_reprepare_SEQ[qos_i] = 0;
        pstat->prepare_done[qos_i] = 0;
        pstat->reprepare_num[qos_i] = 0;
    }
    
}

#endif

void free_sta_tx_skb_pci(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef WIFI_WMM
    int				hd, tl;
#endif
    struct sk_buff	*pskb;
#if defined(SW_TX_QUEUE) || defined(RTK_ATM)
    int i;
#endif
#ifdef SMP_SYNC
	unsigned long flags=0;
#endif

    SMP_LOCK_XMIT(x);    

    // free all skb in dz_queue
    while (skb_queue_len(&pstat->dz_queue)) {
        pskb = (struct sk_buff	*)skb_dequeue(&pstat->dz_queue);
        rtl_kfree_skb(priv, pskb, _SKB_TX_);
    }
#if defined(AP_SWPS_OFFLOAD)
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv))
        free_dzqueue_seq(priv, pstat);
#endif

#ifdef RTK_ATM
    if(priv->pshare->rf_ft_var.atm_en) {
        for(i = BK_QUEUE; i < HIGH_QUEUE; i++) {
            while (skb_queue_len(&pstat->atm_swq.swq_queue[i])) {
                pskb = skb_dequeue(&pstat->atm_swq.swq_queue[i]);
                rtl_kfree_skb(priv, pskb, _SKB_TX_);
            }
        }
    }
#endif

#ifdef SW_TX_QUEUE
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
	    for(i = BK_QUEUE; i < HIGH_QUEUE; i++) {
	        while (skb_queue_len(&pstat->swq.swq_queue[i])) {
	            pskb = skb_dequeue(&pstat->swq.swq_queue[i]);
	            priv->ext_stats.swq_residual_drop_pkt++;
	            priv->pshare->swq_cnt--;
#ifdef SUPPORT_TX_SWQ_ATM
				pstat->swq_atm_cnt--;
#endif						
#ifdef SUPPORT_TX_SWQ_AMSDU
				if (pskb->len < SWQ_AMSDU_SMLPKT_LEN)
					pstat->sml_big_pkt_diff--;
				else
					pstat->sml_big_pkt_diff++;
#endif
#ifdef SW_TXQ_RSVD_DESC
	            priv->pshare->swq_skb_queue_cnt[i - 1]--;
#endif
	            rtl_kfree_skb(priv, pskb, _SKB_TX_);
	        }
	    }
	}
#endif

#if defined(WIFI_WMM) && defined(WMM_APSD)
    hd = pstat->VO_dz_queue->head;
    tl = pstat->VO_dz_queue->tail;
#if defined(AP_SWPS_OFFLOAD)
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv)) 
        pstat->SWPS_seq_tail[3]=pstat->SWPS_seq_head[3]; // update to the same newest seq, if the next STA use the same stat, use the next seq directly
#endif    
    while (CIRC_CNT(hd, tl, NUM_APSD_TXPKT_QUEUE)) {
        pskb = pstat->VO_dz_queue->pSkb[tl];
        rtl_kfree_skb(priv, pskb, _SKB_TX_);
#if defined(AP_SWPS_OFFLOAD)
        if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv))
            pstat->VO_dz_queue->swps_seq[tl] = 0;
#endif        
        tl++;
        tl = tl & (NUM_APSD_TXPKT_QUEUE - 1);
    }
    pstat->VO_dz_queue->head = 0;
    pstat->VO_dz_queue->tail = 0;

    hd = pstat->VI_dz_queue->head;
    tl = pstat->VI_dz_queue->tail;
#if defined(AP_SWPS_OFFLOAD)
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv))
        pstat->SWPS_seq_tail[2]=pstat->SWPS_seq_head[2]; // update to the same newest seq, if the next STA use the same stat, use the next seq directly
#endif        
    while (CIRC_CNT(hd, tl, NUM_APSD_TXPKT_QUEUE)) {
        pskb = pstat->VI_dz_queue->pSkb[tl];
        rtl_kfree_skb(priv, pskb, _SKB_TX_);
#if defined(AP_SWPS_OFFLOAD)
        if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv)) 
            pstat->VI_dz_queue->swps_seq[tl] = 0;
#endif        
        tl++;
        tl = tl & (NUM_APSD_TXPKT_QUEUE - 1);
    }
    pstat->VI_dz_queue->head = 0;
    pstat->VI_dz_queue->tail = 0;

    hd = pstat->BE_dz_queue->head;
    tl = pstat->BE_dz_queue->tail;
#if defined(AP_SWPS_OFFLOAD)
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv))
        pstat->SWPS_seq_tail[1]=pstat->SWPS_seq_head[1]; // update to the same newest seq, if the next STA use the same stat, use the next seq directly
#endif        
    while (CIRC_CNT(hd, tl, NUM_APSD_TXPKT_QUEUE)) {
        pskb = pstat->BE_dz_queue->pSkb[tl];
        rtl_kfree_skb(priv, pskb, _SKB_TX_);
#if defined(AP_SWPS_OFFLOAD)
        if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv))
            pstat->BE_dz_queue->swps_seq[tl] = 0;
#endif        
        tl++;
        tl = tl & (NUM_APSD_TXPKT_QUEUE - 1);
    }
    pstat->BE_dz_queue->head = 0;
    pstat->BE_dz_queue->tail = 0;

    hd = pstat->BK_dz_queue->head;
    tl = pstat->BK_dz_queue->tail;
#if defined(AP_SWPS_OFFLOAD)
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv)) 
        pstat->SWPS_seq_tail[0]=pstat->SWPS_seq_head[0]; // update to the same newest seq, if the next STA use the same stat, use the next seq directly
#endif        
    while (CIRC_CNT(hd, tl, NUM_APSD_TXPKT_QUEUE)) {
        pskb = pstat->BK_dz_queue->pSkb[tl];
        rtl_kfree_skb(priv, pskb, _SKB_TX_);
#if defined(AP_SWPS_OFFLOAD)
        if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv))
            pstat->BK_dz_queue->swps_seq[tl] = 0;
#endif 
        tl++;           
        tl = tl & (NUM_APSD_TXPKT_QUEUE - 1);
    }
    pstat->BK_dz_queue->head = 0;
    pstat->BK_dz_queue->tail = 0;
#endif
#if defined(WIFI_WMM)
    hd = pstat->MGT_dz_queue->head;
    tl = pstat->MGT_dz_queue->tail;
    while (CIRC_CNT(hd, tl, NUM_DZ_MGT_QUEUE)) {
        struct tx_insn *ptx_insn = pstat->MGT_dz_queue->ptx_insn[tl];
        release_mgtbuf_to_poll(priv, ptx_insn->pframe);
        release_wlanhdr_to_poll(priv, ptx_insn->phdr);
        kfree(ptx_insn);
        tl++;
        tl = tl & (NUM_DZ_MGT_QUEUE - 1);
    }
    pstat->MGT_dz_queue->head = 0;
    pstat->MGT_dz_queue->tail = 0;


#ifdef DZ_ADDBA_RSP
    pstat->dz_addba.used = 0;
#endif
#endif

#ifdef SBWC
	SMP_LOCK_SBWC(flags);
	while (CIRC_CNT(pstat->SBWC_txq.head, pstat->SBWC_txq.tail, NUM_TXPKT_QUEUE))
	{
		struct sk_buff *pskb = pstat->SBWC_txq.pSkb[pstat->SBWC_txq.tail];
		rtl_kfree_skb(priv, pskb, _SKB_TX_);
		pstat->SBWC_txq.tail++;
		pstat->SBWC_txq.tail = pstat->SBWC_txq.tail & (NUM_TXPKT_QUEUE - 1);
	}
	while (CIRC_CNT(pstat->SBWC_rxq.head, pstat->SBWC_rxq.tail, NUM_TXPKT_QUEUE))
	{
		struct sk_buff *pskb = pstat->SBWC_rxq.pSkb[pstat->SBWC_rxq.tail];
		rtl_kfree_skb(priv, pskb, _SKB_RX_);
		pstat->SBWC_rxq.tail++;
		pstat->SBWC_rxq.tail = pstat->SBWC_rxq.tail & (NUM_TXPKT_QUEUE - 1);
	}
	SMP_UNLOCK_SBWC(flags);
#endif

    SMP_UNLOCK_XMIT(x);

}
#endif // CONFIG_PCI_HCI

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
void free_sta_tx_skb_usb(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	int i;
	for (i = 0; i < MAX_STA_TX_SERV_QUEUE; ++i) {
#ifdef CONFIG_TCP_ACK_TXAGG
		rtw_tcpack_servq_flush(priv, &pstat->tcpack_queue[i]);
#endif
		rtw_txservq_flush(priv, &pstat->tx_queue[i]);
	}
}
#endif

#ifdef RX_BUFFER_GATHER_REORDER
extern void flush_rx_list_inq(struct rtl8192cd_priv *priv,struct rx_frinfo *pfrinfo);
#endif

void free_sta_skb(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	int	 i, j;
	struct list_head frag_list;
	struct sk_buff	*pskb;
	unsigned long flags;
	
#if defined(SUPPORT_RX_AMSDU_AMPDU) && defined(RX_BUFFER_GATHER_REORDER)
	struct rx_frinfo *pfrinfo;
#endif	

	free_sta_tx_skb(priv,pstat);
	
	// free all skb in frag_list
	INIT_LIST_HEAD(&frag_list);

	DEFRAG_LOCK(flags);
	list_splice_init(&pstat->frag_list, &frag_list);
	DEFRAG_UNLOCK(flags);
	
	unchainned_all_frag(priv, &frag_list);

	// free all skb in rc queue
	SMP_LOCK_REORDER_CTRL(flags);
	for (i=0; i<8; i++) {
		pstat->rc_entry[i].start_rcv = FALSE;
		for (j=0; j<RC_ENTRY_NUM; j++) {
			if (pstat->rc_entry[i].packet_q[j]) {
				pskb = pstat->rc_entry[i].packet_q[j];
				#if defined(SUPPORT_RX_AMSDU_AMPDU) && defined(RX_BUFFER_GATHER_REORDER)
				pfrinfo=pstat->rc_entry[i].frinfo_q[j];
				if(pfrinfo && pfrinfo->gather_flag == GATHER_FIRST) {
					flush_rx_list_inq(priv,pfrinfo);
					pstat->rc_entry[i].frinfo_q[j]=NULL;
				} 
				#endif
				rtl_kfree_skb(priv, pskb, _SKB_RX_);
				pstat->rc_entry[i].packet_q[j] = NULL;
			}
		}
	}
	SMP_UNLOCK_REORDER_CTRL(flags);
}

void free_sta_frag_list(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	struct list_head frag_list;
	unsigned long flags;

	// free all skb in frag_list
	INIT_LIST_HEAD(&frag_list);

	DEFRAG_LOCK(flags);
	list_del_init(&pstat->defrag_list);
	list_splice_init(&pstat->frag_list, &frag_list);
	pstat->frag_count = 0;
	DEFRAG_UNLOCK(flags);

	unchainned_all_frag(priv, &frag_list);
}

#ifdef THERMAL_CONTROL
#ifdef TX_SHORTCUT
void clear_short_cut_cache(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#if defined(CONFIG_PCI_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
	memset(pstat->tx_sc_ent, 0, sizeof(pstat->tx_sc_ent));
	}
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
		memset(pstat->TX_SC_ENT, 0, sizeof(pstat->TX_SC_ENT));
	}
#endif
}
#endif
#endif

void release_stainfo(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	int				i, idx;
	unsigned long		flags;
	struct stat_info *tmp_pstat=NULL;
    struct list_head *phead, *plist; 
    struct reorder_ctrl_timer *prc_timer;
	struct reorder_ctrl_entry *rc_entry=NULL;

    idx = pstat->cmn_info.mac_addr[5];
	tmp_pstat = priv->sta_ary[idx].pstat;
	if(tmp_pstat && isEqualMACAddr(tmp_pstat->cmn_info.mac_addr, pstat->cmn_info.mac_addr))
		priv->sta_ary[idx].pstat = NULL;

	if (priv->pshare->is_40m_bw && (pstat->IOTPeer == HT_IOT_PEER_MARVELL))

	{
	    clearSTABitMap(&priv->pshare->marvellMapBit, pstat->cmn_info.aid);

#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL_8881A) || defined(CONFIG_RTL_8723B_SUPPORT)
		if((GET_CHIP_VER(priv)==VERSION_8812E)||(GET_CHIP_VER(priv)==VERSION_8881A) || (GET_CHIP_VER(priv)==VERSION_8723B)){
		}
        else
#endif
		if ((orSTABitMap(&priv->pshare->marvellMapBit) == 0) &&
			 (priv->pshare->Reg_RRSR_2 != 0) && (priv->pshare->Reg_81b != 0))
		{
#if defined(CONFIG_PCI_HCI)
			if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
				RTL_W8(RRSR+2, priv->pshare->Reg_RRSR_2);
				RTL_W8(0x81b, priv->pshare->Reg_81b);
				priv->pshare->Reg_RRSR_2 = 0;
				priv->pshare->Reg_81b = 0;
			}
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
			if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
				notify_40M_RRSR_SC_change(priv);
			}
#endif
		}
	}

	update_intel_sta_bitmap(priv, pstat, 1);
#if defined(WIFI_11N_2040_COEXIST_EXT)
	update_40m_staMap(priv, pstat, 1);
#endif

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
		// avoid xmit_tasklet deadlock when receiving auth req under heavy TX traffic
#ifdef WDS
		pstat->state &= WIFI_WDS;
#else
		pstat->state = 0;
#endif
	}
#endif

	// flush the stainfo cache
	//if (!memcmp(pstat->cmn_info.mac_addr, priv->stainfo_cache.hwaddr, MACADDRLEN))
	//	memset(&(priv->stainfo_cache), 0, sizeof(priv->stainfo_cache));
	if (pstat == priv->pstat_cache)
		priv->pstat_cache = NULL;

	// delete all list
	// BUT do NOT delete hash list
	asoc_list_del(priv, pstat);
	auth_list_del(priv, pstat);
	sleep_list_del(priv, pstat);
	wakeup_list_del(priv, pstat);

	DEFRAG_LOCK(flags);
	if (!list_empty(&(pstat->defrag_list)))
		list_del_init(&(pstat->defrag_list));
	DEFRAG_UNLOCK(flags);

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
		rtw_pspoll_sta_delete(priv, pstat);
	}
#endif

#ifdef CONFIG_PCI_HCI
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
		// to avoid add RAtid fail
		if (!list_empty(&(pstat->addRAtid_list)))
			list_del_init(&(pstat->addRAtid_list));

		if (!list_empty(&(pstat->addrssi_list)))
			list_del_init(&(pstat->addrssi_list));
	}
#endif

#if defined(CONFIG_RTL_92D_SUPPORT) || defined(CONFIG_RTL_92C_SUPPORT)
	#if defined(CONFIG_USB_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_USB) {
		rtw_flush_h2c_cmd_queue(priv, pstat);
	}
	#endif
#elif defined(CONFIG_RTL_88E_SUPPORT)
	#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
		rtw_flush_cmd_queue(priv, pstat);
	}
	#endif
#endif


#ifdef CONFIG_IEEE80211W
	if (timer_pending(&pstat->SA_timer))
	    del_timer(&pstat->SA_timer);
#endif

	// free all queued skb
	free_sta_skb(priv, pstat);

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
		for (i = 0; i < MAX_STA_TX_SERV_QUEUE; ++i) {
			_rtw_spinlock_free(&(pstat->tx_queue[i].xframe_queue.lock));
#ifdef CONFIG_TCP_ACK_TXAGG
			_rtw_spinlock_free(&(pstat->tcpack_queue[i].xframe_queue.lock));
#endif
		}
	}
#endif

#ifdef CONFIG_RTK_MESH
	SMP_LOCK_MESH_MP_HDR(flags);
	if (!list_empty(&(pstat->mesh_mp_ptr)))
		list_del_init(&(pstat->mesh_mp_ptr));
	SMP_UNLOCK_MESH_MP_HDR(flags);

    if(netif_running(GET_ROOT(priv)->mesh_dev)) {
        if(isMeshPoint(pstat)) {
            clear_route_info(GET_ROOT(priv), pstat->cmn_info.mac_addr);
        }
        else {
            remove_proxy_entry(GET_ROOT(priv), NULL, pstat->cmn_info.mac_addr);        
        }
    }
#endif

#ifdef A4_STA
    if(pstat->state & WIFI_A4_STA) {
        a4_sta_cleanup(priv, pstat);
    }

    if (!list_empty(&pstat->a4_sta_list))
        list_del_init(&pstat->a4_sta_list);	
#endif

#ifdef TV_MODE
    if(priv->tv_mode_status & BIT1) { /*TV mode is auto, check if there is STA that support tv auto*/
        tv_mode_auto_support_check(priv);
    }
#endif

	// remove key in CAM
	if (pstat->dot11KeyMapping.keyInCam == TRUE) {
		if (GET_ROOT(priv)->drv_state & DRV_STATE_OPEN) {
#ifdef USE_WEP_4_KEYS
			if((priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _WEP_40_PRIVACY_) ||
		 		(priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _WEP_104_PRIVACY_)) {
			int keyid=0;
			for(;keyid<4; keyid++) {
				if (CamDeleteOneEntry(priv, pstat->cmn_info.mac_addr, keyid, 0)) 
					priv->pshare->CamEntryOccupied--;				
			}
			pstat->dot11KeyMapping.keyInCam = FALSE;
			} else
#endif
			{
			if (CamDeleteOneEntry(priv, pstat->cmn_info.mac_addr, 0, 0)) {
				pstat->dot11KeyMapping.keyInCam = FALSE;
				priv->pshare->CamEntryOccupied--;
			}
#if defined(CONFIG_RTL_HW_WAPI_SUPPORT)
			/*	for wapi, one state take two cam entry	*/
			if (CamDeleteOneEntry(priv, pstat->cmn_info.mac_addr, 0, 0)) {
				pstat->dot11KeyMapping.keyInCam = FALSE;
				priv->pshare->CamEntryOccupied--;
			}
#endif
			}
		}
	}
    
	SMP_LOCK_REORDER_CTRL(flags);
    phead = &priv->pshare->rc_timer_list;
    plist = phead->next;
    while (phead && plist && (plist != phead)) {
        prc_timer = list_entry(plist, struct reorder_ctrl_timer, list);
        plist = plist->next;
        
        if (prc_timer && prc_timer->pstat == pstat) {
            rc_entry  = &prc_timer->pstat->rc_entry[prc_timer->tid];
            
			if (rc_entry->rc_timer != NULL) {
                reorder_ctrl_timer_del(rc_entry->rc_timer);
                rc_entry->rc_timer = NULL;
			}
        }
	}
	SMP_UNLOCK_REORDER_CTRL(flags);

    SMP_LOCK_XMIT(flags);
#ifdef SW_TX_QUEUE
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
	    for (i = BK_QUEUE; i <= VO_QUEUE; i++) {
	        if(pstat->swq.swq_timer_id[i]) {
	            rtl8192cd_swq_deltimer(priv, pstat, i);
	        }
	    }
	}
#endif
#ifdef SUPPORT_TX_AMSDU
	for (i=0; i<8; i++) {
		free_skb_queue(priv, &pstat->amsdu_tx_que[i]);
		if (pstat->amsdu_timer_id[i] > 0) {
			priv->pshare->amsdu_timer[pstat->amsdu_timer_id[i]-1].pstat = NULL;
			pstat->amsdu_timer_id[i] = 0;
		}
	}
#endif
#ifdef TCP_ACK_ACC
	if (!list_empty(&pstat->tcp_ses_list))
		list_del_init(&pstat->tcp_ses_list);
	for (i = 0; i < TCP_SESSION_MAX_ENTRY ; i++) {
		INIT_LIST_HEAD(&pstat->tcp_ses_active_list[i]);
	}
	memset(pstat->tcp_ses, 0, sizeof(pstat->tcp_ses));
#endif
    SMP_UNLOCK_XMIT(flags);

#ifdef DOT11K
    /*terminate the measurement request to other STA*/
    if (timer_pending(&pstat->rm_timer))
        del_timer_sync(&pstat->rm_timer);
    if(priv->pmib->dot11StationConfigEntry.dot11RadioMeasurementActivated) {  
        /*terminate the measurement process requested by other STA*/
        if(pstat == priv->rm.req_pstat) {
            rm_terminate(priv);            
            priv->rm.req_pstat = NULL;
        }
    }
#endif

#ifdef WDS
	pstat->state &= WIFI_WDS;
#else
	pstat->state = 0;
#endif

#ifdef INDICATE_LINK_CHANGE
	indicate_sta_link_change(priv, pstat, DECREASE, __FUNCTION__);
#endif

#ifdef TX_SHORTCUT
	#if defined(CONFIG_PCI_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
		memset(pstat->tx_sc_ent, 0, sizeof(pstat->tx_sc_ent));
	}
	#endif
	#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
		memset(pstat->TX_SC_ENT, 0, sizeof(pstat->TX_SC_ENT));
	}
	#endif
#endif

#ifdef RX_SHORTCUT
	for (i=0; i<RX_SC_ENTRY_NUM; i++)
		pstat->rx_sc_ent[i].rx_payload_offset = 0;
#endif

#ifdef INCLUDE_WPA_PSK
	if (timer_pending(&pstat->wpa_sta_info->resendTimer)) {
		del_timer(&pstat->wpa_sta_info->resendTimer);
	}
#endif

	release_remapAid(priv, pstat);

#ifdef USE_OUT_SRC
#ifdef _OUTSRC_COEXIST
	if(IS_OUTSRC_CHIP(priv))
#endif
	{
		phydm_cmn_sta_info_hook(ODMPTR, pstat->cmn_info.aid, NULL);
	}
#endif

#if (BEAMFORMING_SUPPORT == 1)
	if (priv->pmib->dot11RFEntry.txbf == 1 && (priv->pshare->WlanSupportAbility & WLAN_BEAMFORMING_SUPPORT) )
	{
        struct _RT_BEAMFORMING_INFO* pBeamformingInfo = &(priv->pshare->BeamformingInfo);

		PHYDM_DBG(ODMPTR, DBG_TXBF, "%s,\n", __FUNCTION__);

        pBeamformingInfo->CurDelBFerBFeeEntrySel = BFerBFeeEntry;

		if(Beamforming_DeInitEntry(priv, pstat->cmn_info.mac_addr))
			Beamforming_Notify(priv);

		// clean the del entry list of exist.
		if (isEqualMACAddr(pstat->cmn_info.mac_addr, pBeamformingInfo->DelEntryListByMACAddr.BFeeEntry_Idx0)) {
			memset(pBeamformingInfo->DelEntryListByMACAddr.BFeeEntry_Idx0, 0, 6);	
		}
		else if (isEqualMACAddr(pstat->cmn_info.mac_addr, pBeamformingInfo->DelEntryListByMACAddr.BFeeEntry_Idx1)) {
			memset(pBeamformingInfo->DelEntryListByMACAddr.BFeeEntry_Idx1, 0, 6);	
		}
		if (isEqualMACAddr(pstat->cmn_info.mac_addr, pBeamformingInfo->DelEntryListByMACAddr.BFerEntry_Idx0)) {
			memset(pBeamformingInfo->DelEntryListByMACAddr.BFerEntry_Idx0, 0, 6);
		}
		else if (isEqualMACAddr(pstat->cmn_info.mac_addr, pBeamformingInfo->DelEntryListByMACAddr.BFerEntry_Idx1)) {
			memset(pBeamformingInfo->DelEntryListByMACAddr.BFerEntry_Idx1, 0, 6);
		}
	}
#endif
#if 1
#if defined(BR_SHORTCUT) && defined(RTL_CACHED_BR_STA)
	release_brsc_cache(pstat->cmn_info.mac_addr);
#endif
#if defined(CONFIG_RTL_FASTBRIDGE)
		rtl_fb_del_entry(pstat->cmn_info.mac_addr);
#endif
#else
#ifdef BR_SHORTCUT
	clear_shortcut_cache();
#endif
#endif
#if defined(CONFIG_RTL_88E_SUPPORT) || defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL)
	if(pstat->txpause_flag && priv->pshare->paused_sta_num)
		priv->pshare->paused_sta_num--;
#endif		
#ifdef CONFIG_RTL_WAPI_SUPPORT
	free_sta_wapiInfo(priv, pstat);
#endif

#if defined(AP_SWPS_OFFLOAD)
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv)) {
        //polling a SWPS RPT
        //to avoid dropID in ctrl info be not cleared.  if timeout when STA is in sleep, no chance to clear dropID in ctrl info, so here polling SWPS report and update
        GET_HAL_INTERFACE(priv)->SetHwRegHandler(priv,HW_VAR_SWPS_RPT_POLLING,NULL);
        HAL_delay_ms(5); //not sure if its enough, in SWPS_RPT_Handler print a lot info
        if(SWPS_check_drop_done(pstat)){
            //send H2C to clear drop id in ctrl info        
            GET_HAL_INTERFACE(priv)->ClearDropIDHandler(priv, pstat->cmn_info.aid, pstat->Drop_ID-1);
        }else
            printk("ERROR, dropID in CTRL info is 0x%x, and is not cleared when clear STA info\n",pstat->RPT_DropBit);//debug

        free_swps_info(priv, pstat);
        
        if(pstat->EnSWPS){
            u8 macid = pstat->cmn_info.aid;
            u8 macid_grp = macid/32;
            u8 macid_bit = macid%32;
            pstat->EnSWPS = 0;
            
            RTL_W8(REG_SWPS_CTRL, BIT_SET_MACID_SWPS_EN_SEL(RTL_R8(REG_SWPS_CTRL),macid_grp));
            RTL_W8(REG_MACID_SWPS_EN, RTL_R8(REG_MACID_SWPS_EN) & ~macid_bit);
            GET_HAL_INTERFACE(priv)->MACIDSWPSCtrlHandler(priv,pstat,pstat->EnSWPS);
            
        }
    }
#endif
}


struct	stat_info *alloc_stainfo(struct rtl8192cd_priv *priv, unsigned char *hwaddr, int id)
{
#if 1/*!defined(SMP_SYNC) || (defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI))*/
	unsigned long	flags;
#endif
    unsigned int	i, idx;
    unsigned char   init_swps_seq_i;

	struct list_head	*phead, *plist;
	struct stat_info	*pstat;

#ifdef CONFIG_FON
	int idx=wlan_find_idx_by_linux_name(priv->dev->name);
	Fonclient *pFonClient;
	if(is_FON_SSID(idx) && !FON_getClient_byMac(hwaddr, &pFonClient))
	{
		pFonClient =  (Fonclient *)malloc(sizeof(Fonclient));
		memset(pFonClient, 0, sizeof(Fonclient));
		memcpy(pFonClient->mac, hwaddr, MACADDRLEN);
		pFonClient->ssid_idx = idx;
		REDIR_get_challange(pFonClient->challange);
		INIT_LIST_HEAD(&(pFonClient->list));
		list_add_tail((struct list_head*)&pFonClient->list,&fonClientList);
	}
#endif

	SAVE_INT_AND_CLI(flags);

	if (id < 0) { // not from FAST_RECOVERY
	// any free sta info?
		for(i=0; i<NUM_STAT; i++) {
			if (priv->pshare->aidarray[i] && (priv->pshare->aidarray[i]->used == FALSE))
				goto init;
		}

		// allocate new sta info
		for(i=0; i<NUM_STAT; i++) {
			if (priv->pshare->aidarray[i] == NULL)
				break;
		}
	} else {
		i = id;
		goto init;
	}

	if (i < NUM_STAT) {
#ifdef RTL8192CD_VARIABLE_USED_DMEM
			priv->pshare->aidarray[i] = (struct aid_obj *)rtl8192cd_dmem_alloc(AID_OBJ, &i);
#else
#ifdef PRIV_STA_BUF
			priv->pshare->aidarray[i] = alloc_sta_obj(priv);
#else
			priv->pshare->aidarray[i] = (struct aid_obj *)kmalloc(sizeof(struct aid_obj), GFP_ATOMIC);
#endif
#endif
			if (priv->pshare->aidarray[i] == NULL)
				goto no_free_memory;
			memset(priv->pshare->aidarray[i], 0, sizeof(struct aid_obj));

#ifdef CONFIG_PCI_HCI
			if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
#if defined(WIFI_WMM) && defined(WMM_APSD)
#ifdef PRIV_STA_BUF
				priv->pshare->aidarray[i]->station.VO_dz_queue = alloc_sta_que(priv);
#else
				priv->pshare->aidarray[i]->station.VO_dz_queue = (struct apsd_pkt_queue *)kmalloc(sizeof(struct apsd_pkt_queue), GFP_ATOMIC);
#endif
				if (priv->pshare->aidarray[i]->station.VO_dz_queue == NULL)
					goto no_free_memory;
				memset(priv->pshare->aidarray[i]->station.VO_dz_queue, 0, sizeof(struct apsd_pkt_queue));

#ifdef PRIV_STA_BUF
				priv->pshare->aidarray[i]->station.VI_dz_queue = alloc_sta_que(priv);
#else
				priv->pshare->aidarray[i]->station.VI_dz_queue = (struct apsd_pkt_queue *)kmalloc(sizeof(struct apsd_pkt_queue), GFP_ATOMIC);
#endif
				if (priv->pshare->aidarray[i]->station.VI_dz_queue == NULL)
					goto no_free_memory;
				memset(priv->pshare->aidarray[i]->station.VI_dz_queue, 0, sizeof(struct apsd_pkt_queue));

#ifdef PRIV_STA_BUF
				priv->pshare->aidarray[i]->station.BE_dz_queue = alloc_sta_que(priv);
#else
				priv->pshare->aidarray[i]->station.BE_dz_queue = (struct apsd_pkt_queue *)kmalloc(sizeof(struct apsd_pkt_queue), GFP_ATOMIC);
#endif
				if (priv->pshare->aidarray[i]->station.BE_dz_queue == NULL)
					goto no_free_memory;
				memset(priv->pshare->aidarray[i]->station.BE_dz_queue, 0, sizeof(struct apsd_pkt_queue));

#ifdef PRIV_STA_BUF
				priv->pshare->aidarray[i]->station.BK_dz_queue = alloc_sta_que(priv);
#else
				priv->pshare->aidarray[i]->station.BK_dz_queue = (struct apsd_pkt_queue *)kmalloc(sizeof(struct apsd_pkt_queue), GFP_ATOMIC);
#endif
				if (priv->pshare->aidarray[i]->station.BK_dz_queue == NULL)
					goto no_free_memory;
				memset(priv->pshare->aidarray[i]->station.BK_dz_queue, 0, sizeof(struct apsd_pkt_queue));
#endif

#if defined(WIFI_WMM)
#ifdef PRIV_STA_BUF
				priv->pshare->aidarray[i]->station.MGT_dz_queue = alloc_sta_mgt_que(priv);
#else
				priv->pshare->aidarray[i]->station.MGT_dz_queue = (struct dz_mgmt_queue *)kmalloc(sizeof(struct dz_mgmt_queue), GFP_ATOMIC);
#endif
				if (priv->pshare->aidarray[i]->station.MGT_dz_queue == NULL)
					goto no_free_memory;
				memset(priv->pshare->aidarray[i]->station.MGT_dz_queue, 0, sizeof(struct dz_mgmt_queue));
#endif
			}
#endif // CONFIG_PCI_HCI

#if defined(INCLUDE_WPA_PSK) || defined(WIFI_HAPD) || defined(RTK_NL80211)
#ifdef PRIV_STA_BUF
			priv->pshare->aidarray[i]->station.wpa_sta_info = alloc_wpa_buf(priv);
#else
			priv->pshare->aidarray[i]->station.wpa_sta_info = (WPA_STA_INFO *)kmalloc(sizeof(WPA_STA_INFO), GFP_ATOMIC);
#endif
			if (priv->pshare->aidarray[i]->station.wpa_sta_info == NULL)
				goto no_free_memory;
			memset(priv->pshare->aidarray[i]->station.wpa_sta_info, 0, sizeof(WPA_STA_INFO));
#endif

#if defined(WIFI_HAPD) || defined(RTK_NL80211)
			memset(priv->pshare->aidarray[i]->station.wpa_ie, 0, 256);
#ifndef HAPD_DRV_PSK_WPS
			memset(priv->pshare->aidarray[i]->station.wps_ie, 0, 256);
#endif
#endif

			pstat = &(priv->pshare->aidarray[i]->station);
			INIT_LIST_HEAD(&(pstat->hash_list));
			pstat->cmn_info.aid = i + 1; //aid 0 is reserved for AP
#ifdef AP_SWPS_OFFLOAD
            if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv)) {
                //swps init when allocate sta for the first time
                for(init_swps_seq_i=0;init_swps_seq_i<4;init_swps_seq_i++)
                    pstat->SWPS_seq[init_swps_seq_i]=1;
                //memset(pstat->SWPS_seq,0,sizeof(unsigned short)*4);
                memset(pstat->SWPS_seq_head,0,sizeof(unsigned short)*4);
                memset(pstat->SWPS_seq_tail,0,sizeof(unsigned short)*4);
                pstat->Drop_ID = 0;    
            }
#endif
			goto init;
	}

	// no more free sta info, check idle sta
	for(i=0; i<NUM_STAT; i++) {
		pstat = &(priv->pshare->aidarray[i]->station);
		if ((pstat->expire_to == 0)
#ifdef WDS
#ifdef LAZY_WDS
			&& ((pstat->state & WIFI_WDS_LAZY) ||
				(!(pstat->state & WIFI_WDS_LAZY) && !(pstat->state & WIFI_WDS)))
#else
			&& !(pstat->state & WIFI_WDS)
#endif
#endif
		)
		{
			release_stainfo(priv->pshare->aidarray[i]->priv, pstat);
			hash_list_del(priv->pshare->aidarray[i]->priv, pstat);
			goto init;
		}
	}

	RESTORE_INT(flags);
	DEBUG_ERR("AID buf is not enough\n");
	return	(struct stat_info *)NULL;

init:
	priv->pshare->aidarray[i]->priv = priv;
	priv->pshare->aidarray[i]->used = TRUE;
	pstat = &(priv->pshare->aidarray[i]->station);
	memcpy(pstat->cmn_info.mac_addr, hwaddr, MACADDRLEN);
	init_stainfo(priv, pstat);
#if defined(CONFIG_RTL_88E_SUPPORT) && defined(TXREPORT)
	if (GET_CHIP_VER(priv)==VERSION_8188E) {
#ifdef RATEADAPTIVE_BY_ODM
		odm_ra_info_init(ODMPTR, pstat->cmn_info.aid);
#else
		priv->pshare->RaInfo[pstat->cmn_info.aid].pstat = pstat;
		RateAdaptiveInfoInit(&priv->pshare->RaInfo[pstat->cmn_info.aid]);
#endif			
	}
#endif
	// insert to hash list
	hash_list_add(priv, pstat);

	RESTORE_INT(flags);

	idx = hwaddr[5];
	priv->sta_ary[idx].pstat = pstat;
	return pstat;

no_free_memory:

	if (priv->pshare->aidarray[i]) {
#if defined(INCLUDE_WPA_PSK) || defined(WIFI_HAPD) || defined(RTK_NL80211)
		if (priv->pshare->aidarray[i]->station.wpa_sta_info)
#ifdef PRIV_STA_BUF
			free_wpa_buf(priv, priv->pshare->aidarray[i]->station.wpa_sta_info);
#else
			kfree(priv->pshare->aidarray[i]->station.wpa_sta_info);
#endif
#endif

#ifdef CONFIG_PCI_HCI
		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
#if defined(WIFI_WMM) && defined(WMM_APSD)
#ifdef PRIV_STA_BUF
			if (priv->pshare->aidarray[i]->station.VO_dz_queue)
				free_sta_que(priv, priv->pshare->aidarray[i]->station.VO_dz_queue);
			if (priv->pshare->aidarray[i]->station.VI_dz_queue)
				free_sta_que(priv, priv->pshare->aidarray[i]->station.VI_dz_queue);
			if (priv->pshare->aidarray[i]->station.BE_dz_queue)
				free_sta_que(priv, priv->pshare->aidarray[i]->station.BE_dz_queue);
			if (priv->pshare->aidarray[i]->station.BK_dz_queue)
				free_sta_que(priv, priv->pshare->aidarray[i]->station.BK_dz_queue);
#else
			if (priv->pshare->aidarray[i]->station.VO_dz_queue)
				kfree(priv->pshare->aidarray[i]->station.VO_dz_queue);
			if (priv->pshare->aidarray[i]->station.VI_dz_queue)
				kfree(priv->pshare->aidarray[i]->station.VI_dz_queue);
			if (priv->pshare->aidarray[i]->station.BE_dz_queue)
				kfree(priv->pshare->aidarray[i]->station.BE_dz_queue);
			if (priv->pshare->aidarray[i]->station.BK_dz_queue)
				kfree(priv->pshare->aidarray[i]->station.BK_dz_queue);
#endif
#endif

#if defined(WIFI_WMM)
#ifdef PRIV_STA_BUF
			if (priv->pshare->aidarray[i]->station.MGT_dz_queue)
				free_sta_mgt_que(priv, priv->pshare->aidarray[i]->station.MGT_dz_queue);
#else
			if (priv->pshare->aidarray[i]->station.MGT_dz_queue)
				kfree(priv->pshare->aidarray[i]->station.MGT_dz_queue);

#endif
#endif
		}
#endif // CONFIG_PCI_HCI

#ifdef RTL8192CD_VARIABLE_USED_DMEM
		rtl8192cd_dmem_free(AID_OBJ, &i);
#else
#ifdef PRIV_STA_BUF
		free_sta_obj(priv, priv->pshare->aidarray[i]);
#else
		kfree(priv->pshare->aidarray[i]);
#endif
#endif
		priv->pshare->aidarray[i] = NULL;
	}

	RESTORE_INT(flags);
	DEBUG_ERR("No free memory to allocate station info\n");
	return NULL;
}


int del_station(struct rtl8192cd_priv *priv, struct stat_info *pstat, int send_disasoc)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif
	DOT11_DISASSOCIATION_IND Disassociation_Ind;

	if (!netif_running(priv->dev))
		return 0;

	if (pstat == NULL)
		return 0;

	if (asoc_list_del(priv, pstat))
	{
		if (IEEE8021X_FUN)
		{
#ifndef WITHOUT_ENQUEUE
			memcpy((void *)Disassociation_Ind.MACAddr, (void *)pstat->cmn_info.mac_addr, MACADDRLEN);
			Disassociation_Ind.EventId = DOT11_EVENT_DISASSOCIATION_IND;
			Disassociation_Ind.IsMoreEvent = 0;
			Disassociation_Ind.Reason = _STATS_OTHER_;
			Disassociation_Ind.tx_packets = pstat->tx_pkts;
			Disassociation_Ind.rx_packets = pstat->rx_pkts;
			Disassociation_Ind.tx_bytes   = pstat->tx_bytes;
			Disassociation_Ind.rx_bytes   = pstat->rx_bytes;
			DOT11_EnQueue((unsigned long)priv, priv->pevent_queue, (UINT8 *)&Disassociation_Ind,
						sizeof(DOT11_DISASSOCIATION_IND));
#endif
#if defined(INCLUDE_WPA_PSK) || defined(WIFI_HAPD) || defined(RTK_NL80211)
			psk_indicate_evt(priv, DOT11_EVENT_DISASSOCIATION_IND, pstat->cmn_info.mac_addr, NULL, 0);
#endif

#ifdef RTK_NL80211
			event_indicate_cfg80211(priv, pstat->cmn_info.mac_addr, CFG80211_DEL_STA, NULL);
#endif
#ifdef WIFI_HAPD
			event_indicate_hapd(priv, pstat->cmn_info.mac_addr, HAPD_EXIRED, NULL);
#ifdef HAPD_DRV_PSK_WPS
			event_indicate(priv, pstat->cmn_info.mac_addr, 2);
#endif
#else
			event_indicate(priv, pstat->cmn_info.mac_addr, 2);
#endif
		}

		if (send_disasoc)
			issue_disassoc(priv, pstat->cmn_info.mac_addr, _RSON_UNSPECIFIED_);

		if (pstat->expire_to > 0)
		{
			SAVE_INT_AND_CLI(flags);
			cnt_assoc_num(priv, pstat, DECREASE, (char *)__FUNCTION__);
			check_sta_characteristic(priv, pstat, DECREASE);
			RESTORE_INT(flags);

			LOG_MSG("A STA is deleted by application program - %02X:%02X:%02X:%02X:%02X:%02X\n",
				pstat->cmn_info.mac_addr[0], pstat->cmn_info.mac_addr[1], pstat->cmn_info.mac_addr[2], pstat->cmn_info.mac_addr[3], pstat->cmn_info.mac_addr[4], pstat->cmn_info.mac_addr[5]);
		}
#ifdef STA_ASSOC_STATISTIC	
		add_sta_assoc_status(priv, pstat->cmn_info.mac_addr, pstat->rssi, _RSON_UNSPECIFIED_ | 0x300);
#endif		
	}

	free_stainfo(priv, pstat);

#ifdef CLIENT_MODE
	if (OPMODE & WIFI_STATION_STATE) {
		OPMODE_VAL(OPMODE & ~(WIFI_AUTH_SUCCESS | WIFI_ASOC_STATE));
    	JOIN_RES_VAL(STATE_Sta_No_Bss);
		start_clnt_lookup(priv, DONTRESCAN);
	}
#endif

	return 1;
}


int	free_stainfo(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifndef SMP_SYNC
	unsigned long	flags;
#endif
	unsigned int	i;

	if (pstat == (struct stat_info *)NULL)
	{
		DEBUG_ERR("illegal free an NULL stat obj\n");
		return FAIL;
	}
#ifdef CONFIG_FON
	int idx=wlan_find_idx_by_linux_name(priv->dev->name);
	Fonclient *pFonClient;
	if(is_FON_SSID(idx) && FON_getClient_byMac(pstat->cmn_info.mac_addr, &pFonClient))
	{
		if(pFonClient->accStarted)
			pFonClient->accStopCause = 6;
		else
			pFonClient->accStopCause=17;
	}
#endif

	for(i=0; i<NUM_STAT; i++)
	{
		if (priv->pshare->aidarray[i] &&
#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
			(priv->pshare->aidarray[i]->priv == priv) &&
#endif
			(priv->pshare->aidarray[i]->used == TRUE) &&
			(&(priv->pshare->aidarray[i]->station) == pstat))
		{
			DEBUG_INFO("free station info of %02X%02X%02X%02X%02X%02X\n",
				pstat->cmn_info.mac_addr[0], pstat->cmn_info.mac_addr[1], pstat->cmn_info.mac_addr[2],
				pstat->cmn_info.mac_addr[3], pstat->cmn_info.mac_addr[4], pstat->cmn_info.mac_addr[5]);

			SAVE_INT_AND_CLI(flags);
#ifdef WDS
#ifdef LAZY_WDS
			if (!(pstat->state & WIFI_WDS) || (pstat->state & WIFI_WDS_LAZY))
#else
			if (!(pstat->state & WIFI_WDS))
#endif
#endif
			{
				priv->pshare->aidarray[i]->used = FALSE;
				// remove from hash_list
				hash_list_del(priv, pstat);
#if defined(CONFIG_RTL_88E_SUPPORT) && defined(TXREPORT) && !defined(RATEADAPTIVE_BY_ODM)
				if (GET_CHIP_VER(priv)==VERSION_8188E)
					priv->pshare->RaInfo[pstat->cmn_info.aid].pstat = NULL;
#endif
			}

			release_stainfo(priv, pstat);
			RESTORE_INT(flags);
			return SUCCESS;
		}
	}

#if defined(CONFIG_RTL_WAPI_SUPPORT)
	wapiAssert(pstat->wapiInfo==NULL);
#endif
	DEBUG_ERR("pstat can not be freed \n");
	return	FAIL;
}





#ifdef __OSK__
__IRAM_WIFI_PRI1
#else
__MIPS16
__IRAM_IN_865X
#endif
struct stat_info *get_stainfo_fast(struct rtl8192cd_priv *priv, unsigned char *hwaddr, unsigned char macID)
{
    //struct aid_obj  *obj;
    unsigned char search_fail_id;

#ifdef CONFIG_8814_AP_MAC_VERI
#ifndef HW_FILL_MACID
    return NULL;
#endif
#endif //#ifdef CONFIG_8814_AP_MAC_VERI

#ifdef HW_FILL_MACID
    if (IS_SUPPORT_HW_FILL_MACID(priv)) {    
#if RTL88XX_MAC_V4
    if(_GET_HAL_DATA(priv)->MacVersion.is_MAC_v4)
        search_fail_id = HW_MACID_SEARCH_FAIL_V1;
#endif
#if RTL88XX_MAC_V2_V3
    if(_GET_HAL_DATA(priv)->MacVersion.is_MAC_v2_v3)
        search_fail_id = HW_MACID_SEARCH_NOT_READY; 
#endif
        if(macID > 0)
        {
            if((macID >= search_fail_id))
            {
                printk("[%s]Serious issue, HW detect macid fail = 0x%x \n",__FUNCTION__,search_fail_id);
                return get_stainfo(priv, hwaddr);
            }
            else
            {
                //if(get_stainfo(priv, hwaddr)!= &(priv->pshare->aidarray[macID-1]->station))
                //{
                //    printk("SW HW getSTA error SW = %x HW = %x \n",get_stainfo(priv, hwaddr),&(priv->pshare->aidarray[macID-1]->station));
                //}
                if((macID-1) < NUM_STAT)
                return &(priv->pshare->aidarray[macID-1]->station);
                else if(hwaddr != NULL)
                    return get_stainfo(priv, hwaddr);
			   else
			   	return NULL;
            }
            /*
            obj = priv->pshare->aidarray[macID-1];

            if(obj->priv == priv)
                return &(priv->pshare->aidarray[macID-1]->station);
            else
            {
                printk("obj error at macID %x \n",macID);
                return (struct stat_info *)NULL;
            }*/
            
        }else {
            return get_stainfo(priv, hwaddr);
        }        
    }else        
#endif
    {
        return get_stainfo(priv, hwaddr);
    }   
}


#ifdef HW_FILL_MACID
__MIPS16
__IRAM_IN_865X
struct stat_info *get_HW_mapping_sta(struct rtl8192cd_priv *priv, unsigned char macID)
{
    struct aid_obj  *obj;
    unsigned char search_fail_id;
    
#if RTL88XX_MAC_V4
        if(_GET_HAL_DATA(priv)->MacVersion.is_MAC_v4)
            search_fail_id = HW_MACID_SEARCH_FAIL_V1;
#endif
#if RTL88XX_MAC_V2_V3
        if(_GET_HAL_DATA(priv)->MacVersion.is_MAC_v2_v3)
            search_fail_id = HW_MACID_SEARCH_NOT_READY;
#endif
    
    if(macID > 0)
    {
        if((macID >= search_fail_id))
            return (struct stat_info *)NULL;
        else
            return &(priv->pshare->aidarray[macID-1]->station);

        /*
        obj = priv->pshare->aidarray[macID-1];

        if(obj->priv == priv)
            return &(priv->pshare->aidarray[macID-1]->station);
        else
        {
            printk("obj error at macID %x \n",macID);
            return (struct stat_info *)NULL;
        }*/
        
    }
}
#endif // #ifdef HW_FILL_MACID

/* aid is only meaningful for assocated stations... */
struct stat_info *get_aidinfo(struct rtl8192cd_priv *priv, unsigned int aid)
{
	struct list_head	*plist, *phead;
	struct stat_info	*pstat = NULL;
#if defined(SMP_SYNC) /*&& (defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI))*/
	unsigned long flags = 0;
#endif

	if (aid == 0)
		return (struct stat_info *)NULL;
	
	SMP_LOCK_ASOC_LIST(flags);

	phead = &priv->asoc_list;
	plist = phead->next;

	while (plist != phead)
	{
		pstat = list_entry(plist, struct stat_info, asoc_list);
		plist = plist->next;
		if (pstat->cmn_info.aid == aid)
			goto exit;
	}
	pstat = (struct stat_info *)NULL;
exit:
	SMP_UNLOCK_ASOC_LIST(flags);
	
	return pstat;
}

#if 1
struct stat_info *get_macidinfo(struct rtl8192cd_priv *priv, unsigned int aid)
{
	struct list_head	*plist, *phead;
	struct stat_info	*pstat = NULL;
#if defined(SMP_SYNC)/* && (defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI))*/
	unsigned long flags = 0;
#endif

	if (aid == 0)
		return (struct stat_info *)NULL;
	
	SMP_LOCK_ASOC_LIST(flags);

	phead = &priv->asoc_list;
	plist = phead->next;

	while (plist != phead)
	{
		pstat = list_entry(plist, struct stat_info, asoc_list);
		plist = plist->next;
		if (REMAP_AID(pstat) == aid)
			goto exit;
	}
	pstat = (struct stat_info *)NULL;
exit:
	SMP_UNLOCK_ASOC_LIST(flags);
	
	return pstat;
}


struct stat_info *get_macIDinfo(struct rtl8192cd_priv *rpriv, unsigned int macid)
{
	struct rtl8192cd_priv *priv = GET_ROOT(rpriv);
	struct stat_info	*pstat = NULL;
#ifdef MBSSID
	unsigned int i;
#endif

	pstat = get_macidinfo(priv, macid);
	if(pstat)
		return pstat;
	
#ifdef UNIVERSAL_REPEATER
	if (IS_DRV_OPEN(GET_VXD_PRIV(priv)))	{
		pstat = get_macidinfo(GET_VXD_PRIV(priv), macid);
		if(pstat)
			return pstat;
	}
#endif

#ifdef MBSSID
	if (GET_ROOT(priv)->pmib->miscEntry.vap_enable) 		{
		for (i=0; i<RTL8192CD_NUM_VWLAN; i++) {
			if (IS_DRV_OPEN(priv->pvap_priv[i])) {
				pstat = get_macidinfo(priv->pvap_priv[i], macid);
				if(pstat)
					return pstat;	
			}
		}
	}
#endif
	return pstat;
}

#endif


int p80211_stt_findproto(UINT16 proto)
{
	/* Always return found for now.	This is the behavior used by the */
	/*  Zoom Win95 driver when 802.1h mode is selected */
	/* TODO: If necessary, add an actual search we'll probably
		 need this to match the CMAC's way of doing things.
		 Need to do some testing to confirm.
	*/

	if (proto == 0x80f3 ||   /* APPLETALK */
		proto == 0x8137 ) /* DIX II IPX */
		return 1;

	return 0;
}





int skb_p80211_to_ether(struct net_device *dev, int wep_mode, struct rx_frinfo *pfrinfo)
{
    UINT	to_fr_ds;
    INT		payload_length;
    INT		payload_offset, trim_pad;
    UINT8	daddr[WLAN_ETHADDR_LEN];
    UINT8	saddr[WLAN_ETHADDR_LEN];
    UINT8	*pframe;
#ifdef CONFIG_RTK_MESH
    INT 	mesh_header_len = 0;
#endif
    struct wlan_hdr *w_hdr;
    struct wlan_ethhdr_t   *e_hdr;
    struct wlan_llc_t      *e_llc;
    struct wlan_snap_t     *e_snap;
    struct rtl8192cd_priv *priv = GET_DEV_PRIV(dev);

    int wlan_pkt_format;
    struct sk_buff *skb = get_pskb(pfrinfo);

#ifdef RX_SHORTCUT
    extern int get_rx_sc_free_entry(struct stat_info *pstat, unsigned char *pframe);
    int privacy, idx=0;
    struct rx_sc_entry *prxsc_entry = NULL;
    struct wlan_hdr wlanhdr;
    struct stat_info 	*pstat;
#endif // RX_SHORTCUT

    pframe = get_pframe(pfrinfo);
    to_fr_ds = get_tofr_ds(pframe);
    payload_offset = get_hdrlen(priv, pframe);
    trim_pad = 0; // _CRCLNG_ has beed subtracted in isr
    w_hdr = (struct wlan_hdr *)pframe;

#ifdef CONFIG_RTK_MESH
    if(pfrinfo->is_11s & 8)
	{
        struct wlan_ethhdr_t eth;
        struct  MESH_HDR *mhdr = (struct MESH_HDR *) (pframe + sizeof(struct wlan_ethhdr_t));
        const short mlen = (mhdr->mesh_flag & 1) ? 16 : 4;
        memcpy(&eth, pframe, sizeof(struct wlan_ethhdr_t));
        if(mlen & 16)
            memcpy(&eth, mhdr->DestMACAddr, WLAN_ETHADDR_LEN << 1);
        memcpy(skb_pull(skb, mlen), &eth, sizeof(struct wlan_ethhdr_t));
        return SUCCESS;
    }
#endif

#ifdef RX_SHORTCUT
    pstat = get_stainfo(priv, GetAddr2Ptr(skb->data));
#endif // RX_SHORTCUT


	if ( to_fr_ds == 0x00) {
		memcpy(daddr, (const void *)w_hdr->addr1, WLAN_ETHADDR_LEN);
		memcpy(saddr, (const void *)w_hdr->addr2, WLAN_ETHADDR_LEN);
	}
	else if( to_fr_ds == 0x01) {
		memcpy(daddr, (const void *)w_hdr->addr1, WLAN_ETHADDR_LEN);
		memcpy(saddr, (const void *)w_hdr->addr3, WLAN_ETHADDR_LEN);
	}
	else if( to_fr_ds == 0x02) {
		memcpy(daddr, (const void *)w_hdr->addr3, WLAN_ETHADDR_LEN);
		memcpy(saddr, (const void *)w_hdr->addr2, WLAN_ETHADDR_LEN);
	}
	else {
#ifdef CONFIG_RTK_MESH
		// WIFI_11S_MESH = WIFI_QOS_DATA
		if(pfrinfo->is_11s & 1) {
			memcpy(daddr, (const void *)pfrinfo->mesh_header.DestMACAddr, WLAN_ETHADDR_LEN);
			memcpy(saddr, (const void *)pfrinfo->mesh_header.SrcMACAddr, WLAN_ETHADDR_LEN);
			mesh_header_len = 16;				
		}
		else
#endif // CONFIG_RTK_MESH
		{
			memcpy(daddr, (const void *)w_hdr->addr3, WLAN_ETHADDR_LEN);
			memcpy(saddr, (const void *)w_hdr->addr4, WLAN_ETHADDR_LEN);
		}
	}

#if defined(CONFIG_RTL_MESH_SINGLE_IFACE)
	unsigned char dumy_mac[]={0xff,0xff,0xff,0xff,0xff,0xff};
	if (IS_VXD_INTERFACE(priv) && !memcmp(dumy_mac, daddr, 6) &&
		!memcmp(GET_MIB(GET_ROOT(priv)->mesh_priv_sc)->dot11OperationEntry.hwaddr, saddr, 6)) {
		return FAIL; //fiberhome bug, cause looping with vxd, sent from mexh_xmit, multicast
	}
#endif	//CONFIG_RTL_MESH_SINGLE_IFACE   

	if (GetPrivacy(pframe)) {
#ifdef CONFIG_RTL_WAPI_SUPPORT
		if ((wep_mode == _WAPI_SMS4_)) {
			payload_offset += (WAPI_EXT_LEN-WAPI_ALIGNMENT_OFFSET);
			trim_pad += (SMS4_MIC_LEN);
		} else
#endif
		if (((wep_mode == _WEP_40_PRIVACY_) || (wep_mode == _WEP_104_PRIVACY_))) {
			payload_offset += 4;
			trim_pad += 4;
		}
		else if ((wep_mode == _TKIP_PRIVACY_)) {
			payload_offset += 8;
			trim_pad += (8 + 4);
		}
		else if ((wep_mode == _CCMP_PRIVACY_)) {
			payload_offset += 8;
			trim_pad += 8;
		}
		else {
			DEBUG_ERR("drop pkt due to unallowed wep_mode privacy=%d\n", wep_mode);
			return FAIL;
		}
	}

#if defined(CONFIG_RTL_HW_WAPI_SUPPORT)
	skb->len -= WAPI_ALIGNMENT_OFFSET;
#endif

	payload_length = skb->len - payload_offset - trim_pad;

#ifdef CONFIG_RTK_MESH
    payload_length -= mesh_header_len;
#endif

	if (payload_length <= 0) {
		DEBUG_ERR("drop pkt due to payload_length<=0\n");
		return FAIL;
	}

	e_hdr = (struct wlan_ethhdr_t *) (pframe + payload_offset);
	e_llc = (struct wlan_llc_t *) (pframe + payload_offset);
	e_snap = (struct wlan_snap_t *) (pframe + payload_offset + sizeof(struct wlan_llc_t));

	if ((e_llc->dsap==0xaa) && (e_llc->ssap==0xaa) && (e_llc->ctl==0x03))
	{
		if (!memcmp(e_snap->oui, oui_rfc1042, WLAN_IEEE_OUI_LEN)) {
			wlan_pkt_format = WLAN_PKT_FORMAT_SNAP_RFC1042;
			if(!memcmp(&e_snap->type, SNAP_ETH_TYPE_IPX, 2))
				wlan_pkt_format = WLAN_PKT_FORMAT_IPX_TYPE4;
			else if(!memcmp(&e_snap->type, SNAP_ETH_TYPE_APPLETALK_AARP, 2))
				wlan_pkt_format = WLAN_PKT_FORMAT_APPLETALK;
		}
		else if (!memcmp(e_snap->oui, SNAP_HDR_APPLETALK_DDP, WLAN_IEEE_OUI_LEN) &&
				 !memcmp(&e_snap->type, SNAP_ETH_TYPE_APPLETALK_DDP, 2))
			wlan_pkt_format = WLAN_PKT_FORMAT_APPLETALK;
		else if (!memcmp(e_snap->oui, oui_8021h, WLAN_IEEE_OUI_LEN))
			wlan_pkt_format = WLAN_PKT_FORMAT_SNAP_TUNNEL;
		else if (!memcmp(e_snap->oui, oui_cisco, WLAN_IEEE_OUI_LEN))
			wlan_pkt_format = WLAN_PKT_FORMAT_CDP;
		else {
#ifdef CONFIG_WLAN_STATS_EXTENTION
			priv->ext_stats.unknown_pro_pkts_cnt++;
#endif
			DEBUG_ERR("drop pkt due to invalid frame format!\n");
			return FAIL;
		}
	}
	else if ((memcmp(daddr, e_hdr->daddr, WLAN_ETHADDR_LEN) == 0) &&
			 (memcmp(saddr, e_hdr->saddr, WLAN_ETHADDR_LEN) == 0))
		wlan_pkt_format = WLAN_PKT_FORMAT_ENCAPSULATED;
	else
		wlan_pkt_format = WLAN_PKT_FORMAT_OTHERS;

	DEBUG_INFO("Convert 802.11 to 802.3 in format %d\n", wlan_pkt_format);

	if ((wlan_pkt_format == WLAN_PKT_FORMAT_SNAP_RFC1042) ||
		(wlan_pkt_format == WLAN_PKT_FORMAT_SNAP_TUNNEL) ||
		(wlan_pkt_format == WLAN_PKT_FORMAT_CDP)) {
		/* Test for an overlength frame */
		payload_length = payload_length - sizeof(struct wlan_llc_t) - sizeof(struct wlan_snap_t);

		if ((payload_length+WLAN_ETHHDR_LEN) > WLAN_MAX_ETHFRM_LEN) {
			/* A bogus length ethfrm has been sent. */
			/* Is someone trying an oflow attack? */
			DEBUG_WARN("SNAP frame too large (%d>%d)\n",
				(payload_length+WLAN_ETHHDR_LEN), WLAN_MAX_ETHFRM_LEN);
		}

#ifdef RX_SHORTCUT
		if (!priv->pmib->dot11OperationEntry.disable_rxsc && pstat) {
#ifdef CONFIG_RTK_MESH
			if (pfrinfo->is_11s)
				privacy = get_sta_encrypt_algthm(priv, pstat);
			else
#endif // CONFIG_RTK_MESH
#ifdef WDS
			if (pfrinfo->to_fr_ds == 3 && priv->pmib->dot11WdsInfo.wdsEnabled)
				privacy = priv->pmib->dot11WdsInfo.wdsPrivacy;
			else
#endif
				privacy = get_sta_encrypt_algthm(priv, pstat);
			if ((GetFragNum(pframe)==0) &&
#if defined(CONFIG_RTK_MESH) && defined(RX_RL_SHORTCUT)
                (!pfrinfo->is_11s || !IS_MCAST(pfrinfo->da)) &&
#endif
				((privacy == 0) ||
#ifdef CONFIG_RTL_WAPI_SUPPORT
				(privacy==_WAPI_SMS4_) ||
#endif
#ifdef CONFIG_IEEE80211W
				(!UseSwCrypto(priv, pstat, IS_MCAST(GetAddr1Ptr(pframe)), 0))))	
#else
				(!UseSwCrypto(priv, pstat, IS_MCAST(GetAddr1Ptr(pframe))))))
#endif
			{
				idx = get_rx_sc_free_entry(pstat, pframe);
				prxsc_entry = &pstat->rx_sc_ent[idx];
				memcpy((void *)&wlanhdr, pframe, pfrinfo->hdr_len);
			}
		}
#endif // RX_SHORTCUT


		/* chop 802.11 header from skb. */
		skb_pull(skb, payload_offset);

		if ((wlan_pkt_format == WLAN_PKT_FORMAT_SNAP_RFC1042) ||
			(wlan_pkt_format == WLAN_PKT_FORMAT_SNAP_TUNNEL))
		{
			/* chop llc header from skb. */
			skb_pull(skb, sizeof(struct wlan_llc_t));

			/* chop snap header from skb. */
			skb_pull(skb, sizeof(struct wlan_snap_t));
		}

#ifdef CONFIG_RTK_MESH
		/* chop mesh header from skb. */
		skb_pull(skb, mesh_header_len);
#endif

		/* create 802.3 header at beginning of skb. */
		e_hdr = (struct wlan_ethhdr_t *)skb_push(skb, WLAN_ETHHDR_LEN);
		if (wlan_pkt_format == WLAN_PKT_FORMAT_CDP)
			e_hdr->type = payload_length;
		else
			e_hdr->type = e_snap->type;
		memcpy((void *)e_hdr->daddr, daddr, WLAN_ETHADDR_LEN);
		memcpy((void *)e_hdr->saddr, saddr, WLAN_ETHADDR_LEN);

		/* chop off the 802.11 CRC */
		skb_trim(skb, payload_length + WLAN_ETHHDR_LEN);

#ifdef RX_SHORTCUT
		if (prxsc_entry) {
			if ((e_hdr->type != htons(0x888e)) && // for WIFI_SIMPLE_CONFIG
#ifdef CONFIG_RTL_WAPI_SUPPORT
				(e_hdr->type != htons(ETH_P_WAPI)) &&
#endif
				(e_hdr->type != htons(ETH_P_ARP)) &&
				(wlan_pkt_format != WLAN_PKT_FORMAT_CDP)) {
				memcpy((void *)&prxsc_entry->rx_wlanhdr, &wlanhdr, pfrinfo->hdr_len);
				memcpy((void *)&prxsc_entry->rx_ethhdr, (const void *)e_hdr, sizeof(struct wlan_ethhdr_t));
				prxsc_entry->rx_payload_offset = payload_offset;
				prxsc_entry->rx_trim_pad = trim_pad;
				pstat->rx_privacy = GetPrivacy(pframe);

#if defined(CONFIG_RTK_MESH) && defined(RX_RL_SHORTCUT)
				if ((pfrinfo->is_11s & 3) && (pfrinfo->mesh_header.mesh_flag & 1))
				{
					memcpy((void *)&prxsc_entry->rx_wlanhdr.meshhdr.DestMACAddr, (const void *)pfrinfo->mesh_header.DestMACAddr, WLAN_ETHADDR_LEN);
					memcpy((void *)&prxsc_entry->rx_wlanhdr.meshhdr.SrcMACAddr, (const void *)pfrinfo->mesh_header.SrcMACAddr, WLAN_ETHADDR_LEN);
				}
#endif
			}
		}
#endif
	}
	else if ((wlan_pkt_format == WLAN_PKT_FORMAT_OTHERS) ||
			 (wlan_pkt_format == WLAN_PKT_FORMAT_APPLETALK) ||
			 (wlan_pkt_format == WLAN_PKT_FORMAT_IPX_TYPE4)) {

		/* Test for an overlength frame */
		if ( (payload_length + WLAN_ETHHDR_LEN) > WLAN_MAX_ETHFRM_LEN ) {
			/* A bogus length ethfrm has been sent. */
			/* Is someone trying an oflow attack? */
			DEBUG_WARN("IPX/AppleTalk frame too large (%d>%d)\n",
				(payload_length + WLAN_ETHHDR_LEN), WLAN_MAX_ETHFRM_LEN);
		}

		/* chop 802.11 header from skb. */
		skb_pull(skb, payload_offset);

#ifdef CONFIG_RTK_MESH
		/* chop mesh header from skb. */
		skb_pull(skb, mesh_header_len);
#endif

		/* create 802.3 header at beginning of skb. */
		e_hdr = (struct wlan_ethhdr_t *)skb_push(skb, WLAN_ETHHDR_LEN);
		memcpy((void *)e_hdr->daddr, daddr, WLAN_ETHADDR_LEN);
		memcpy((void *)e_hdr->saddr, saddr, WLAN_ETHADDR_LEN);
		e_hdr->type = htons(payload_length);

		/* chop off the 802.11 CRC */
		skb_trim(skb, payload_length+WLAN_ETHHDR_LEN);
	}
	else if (wlan_pkt_format == WLAN_PKT_FORMAT_ENCAPSULATED) {

		if ( payload_length > WLAN_MAX_ETHFRM_LEN ) {
			/* A bogus length ethfrm has been sent. */
			/* Is someone trying an oflow attack? */
			DEBUG_WARN("Encapsulated frame too large (%d>%d)\n",
				payload_length, WLAN_MAX_ETHFRM_LEN);
		}

		/* Chop off the 802.11 header. */
		skb_pull(skb, payload_offset);

#ifdef CONFIG_RTK_MESH
		/* chop mesh header from skb. */
		skb_pull(skb, mesh_header_len);
#endif

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








unsigned int get_mcast_encrypt_algthm(struct rtl8192cd_priv *priv)
{
	unsigned int privacy;

#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
	if (!IS_ROOT_INTERFACE(priv) && !IEEE8021X_FUN &&
		((priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _WEP_40_PRIVACY_) ||
		 (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _WEP_104_PRIVACY_))){
		 privacy = priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm;
	}else
#endif
#ifdef CONFIG_RTL_WAPI_SUPPORT
	if (priv->pmib->wapiInfo.wapiType!=wapiDisable)
	{
		return _WAPI_SMS4_;
	} else
#endif
	{
	if (priv->pmib->dot118021xAuthEntry.dot118021xAlgrthm) {
		// check station info
		privacy = priv->pmib->dot11GroupKeysTable.dot11Privacy;
	}
	else {	// legacy system
		privacy = priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm;//must be wep40 or wep104
	}
	}
	return privacy;
}




















unsigned int get_curMaxRFPath(struct rtl8192cd_priv *priv) {
#if defined(CONFIG_WLAN_HAL_8192EE) || defined(CONFIG_WLAN_HAL_8822BE) || defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL_8192FE)
	if(GET_CHIP_VER(priv) == VERSION_8192E || GET_CHIP_VER(priv) == VERSION_8822B || GET_CHIP_VER(priv) == VERSION_8812E || GET_CHIP_VER(priv) == VERSION_8192F)
		return 2;
	else
#endif
#if defined(CONFIG_WLAN_HAL_8814AE) || defined(CONFIG_WLAN_HAL_8814BE)
	if(GET_CHIP_VER(priv) == VERSION_8814A || GET_CHIP_VER(priv) == VERSION_8814B)
		return 4;
	else
#endif
	return 0;
}




unsigned short get_pnl(union PN48 *ptsc)
{
	return (((ptsc->_byte_.TSC1) << 8) | (ptsc->_byte_.TSC0));
}


unsigned int get_pnh(union PN48 *ptsc)
{
	return 	(((ptsc->_byte_.TSC5) << 24) |
			 ((ptsc->_byte_.TSC4) << 16) |
			 ((ptsc->_byte_.TSC3) << 8) |
			  (ptsc->_byte_.TSC2));
}




void check_protection_shortslot(struct rtl8192cd_priv *priv)
{
#ifdef RTK_5G_SUPPORT /*eric-8822*/
	if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11A)
		return;
#endif
	if (priv->pmib->dot11ErpInfo.nonErpStaNum == 0 &&
			priv->pmib->dot11ErpInfo.olbcDetected == 0)
	{
		if (priv->pmib->dot11ErpInfo.protection) {
			priv->pmib->dot11ErpInfo.protection = 0;
			//priv->pshare->phw->RTSInitRate_Candidate = 0x8;	// 24Mbps
		}
	}
	else
	{
		if (!priv->pmib->dot11StationConfigEntry.protectionDisabled &&
					priv->pmib->dot11ErpInfo.protection == 0) {
			priv->pmib->dot11ErpInfo.protection = 1;
			//priv->pshare->phw->RTSInitRate_Candidate = 0x3; // 11Mbps
		}
	}

	if (priv->pmib->dot11ErpInfo.nonErpStaNum == 0)
	{
		if (priv->pmib->dot11ErpInfo.shortSlot == 0)
		{
			priv->pmib->dot11ErpInfo.shortSlot = 1;
#ifdef MBSSID
			if ((IS_ROOT_INTERFACE(priv))
#ifdef UNIVERSAL_REPEATER
				|| (IS_VXD_INTERFACE(priv))
#endif
				)
#endif
			set_slot_time(priv, priv->pmib->dot11ErpInfo.shortSlot);
			SET_SHORTSLOT_IN_BEACON_CAP;
			DEBUG_INFO("set short slot time\n");
		}
	}
	else
	{
		if (priv->pmib->dot11ErpInfo.shortSlot)
		{
			priv->pmib->dot11ErpInfo.shortSlot = 0;
#ifdef MBSSID
			if ((IS_ROOT_INTERFACE(priv))
#ifdef UNIVERSAL_REPEATER
				|| (IS_VXD_INTERFACE(priv))
#endif
				)
#endif
			set_slot_time(priv, priv->pmib->dot11ErpInfo.shortSlot);
			RESET_SHORTSLOT_IN_BEACON_CAP;
			DEBUG_INFO("reset short slot time\n");
		}
	}
}


void check_sta_characteristic(struct rtl8192cd_priv *priv, struct stat_info *pstat, int act)
{
	if (act == INCREASE) {
		if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11G) && !isErpSta(pstat)) {
			priv->pmib->dot11ErpInfo.nonErpStaNum++;
			check_protection_shortslot(priv);

			if (!pstat->useShortPreamble)
				priv->pmib->dot11ErpInfo.longPreambleStaNum++;
		}

		if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) && (pstat->ht_cap_len == 0))
			priv->ht_legacy_sta_num++;
	}
	else {
		if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11G) && !isErpSta(pstat)) {
			priv->pmib->dot11ErpInfo.nonErpStaNum--;
			check_protection_shortslot(priv);

			if (!pstat->useShortPreamble && priv->pmib->dot11ErpInfo.longPreambleStaNum > 0)
				priv->pmib->dot11ErpInfo.longPreambleStaNum--;
		}

		if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) && (pstat->ht_cap_len == 0))
			priv->ht_legacy_sta_num--;
	}
}

int should_forbid_Nmode(struct rtl8192cd_priv *priv)
{
	if (!(priv->pmib->dot11BssType.net_work_type & WIRELESS_11N))
		return 0;

	if (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _NO_PRIVACY_)
		return 0;

	if (!(priv->pmib->dot11nConfigEntry.dot11nLgyEncRstrct & BIT(3)))
		return 0;

	// if pure TKIP, change N mode to G mode
	if (priv->pmib->dot11nConfigEntry.dot11nLgyEncRstrct & BIT(1)) {
		if (priv->pmib->dot1180211AuthEntry.dot11EnablePSK ||
			priv->pmib->dot118021xAuthEntry.dot118021xAlgrthm) {

			if ((priv->pmib->dot1180211AuthEntry.dot11WPACipher == 2) &&
				(priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher == 0))
				return 1;
			else if ((priv->pmib->dot1180211AuthEntry.dot11WPACipher == 0) &&
				(priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher == 2))
				return 1;
			else if ((priv->pmib->dot1180211AuthEntry.dot11WPACipher == 2) &&
				(priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher == 2))
				return 1;
		}
	}

	// if WEP, forbid  N mode
	if ((priv->pmib->dot11nConfigEntry.dot11nLgyEncRstrct & BIT(0)) &&
		((priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _WEP_40_PRIVACY_) ||
		 (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _WEP_104_PRIVACY_)))
		return 1;

	return 0;
}





#ifdef WDS
int getWdsIdxByDev(struct rtl8192cd_priv *priv, struct net_device *dev)
{
	int i;

#ifdef LAZY_WDS
	int max_num;
	if (priv->pmib->dot11WdsInfo.wdsEnabled == WDS_LAZY_ENABLE) 
		max_num = NUM_WDS;
	else
		max_num = priv->pmib->dot11WdsInfo.wdsNum;

	for (i=0; i<max_num; i++) {	
#else

	for (i=0; i<priv->pmib->dot11WdsInfo.wdsNum; i++) {
#endif		

		if (dev == priv->wds_dev[i])
			return i;
	}
	return -1;
}


struct net_device *getWdsDevByAddr(struct rtl8192cd_priv *priv, unsigned char *addr)
{
	int i;

#ifdef LAZY_WDS
	int max_num;
	if (priv->pmib->dot11WdsInfo.wdsEnabled == WDS_LAZY_ENABLE) 
		max_num = NUM_WDS;
	else
		max_num = priv->pmib->dot11WdsInfo.wdsNum;

	for (i=0; i<max_num; i++) {	
#else

	for (i=0; i<priv->pmib->dot11WdsInfo.wdsNum; i++) {
#endif		
			if (!memcmp(priv->pmib->dot11WdsInfo.entry[i].macAddr, addr, 6))
				return priv->wds_dev[i];
	}
	return NULL;
}
#endif // WDS


void validate_oper_rate(struct rtl8192cd_priv *priv)
{
	unsigned int supportedRates;
	unsigned int basicRates;

	if (OPMODE & WIFI_AP_STATE)
	{
		supportedRates = priv->pmib->dot11StationConfigEntry.dot11SupportedRates;
		basicRates = priv->pmib->dot11StationConfigEntry.dot11BasicRates;

		if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11B) {
			if (!(priv->pmib->dot11BssType.net_work_type & WIRELESS_11G)) {
				// if use B only, mask G high rate
				supportedRates &= 0xf;
				basicRates &= 0xf;
			}
		}
		else {
			// if use A or G mode, mask B low rate
			supportedRates &= 0xff0;
			basicRates &= 0xff0;
		}

		if (supportedRates == 0) {
			if (priv->pmib->dot11BssType.net_work_type & (WIRELESS_11G | WIRELESS_11A))
				supportedRates = 0xff0;
			if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11B)
				supportedRates |= 0xf;

			PRINT_INFO("invalid supproted rate, use default value [%x]!\n", supportedRates);
		}

		if (basicRates == 0) {
			if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11A)
				//basicRates = 0x1f0;
				//11a basic rate is 6/12/24M 
				basicRates = 0x150;
			if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11B)
				basicRates = 0xf;
			if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11G) {
				if (!(priv->pmib->dot11BssType.net_work_type & WIRELESS_11B))
					basicRates = 0x1f0;
			}

			PRINT_INFO("invalid basic rate, use default value [%x]!\n", basicRates);
		}

		if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11G) {
			if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11B) {
				if ((basicRates & 0xf) == 0)		// if no CCK rates. jimmylin 2004/12/02
					basicRates |= 0xf;
				if ((supportedRates & 0xf) == 0)	// if no CCK rates. jimmylin 2004/12/02
					supportedRates |= 0xf;
			}
			if ((supportedRates & 0xff0) == 0) {	// no ERP rate existed
				supportedRates |= 0xff0;

				PRINT_INFO("invalid supported rate for 11G, use default value [%x]!\n",
																	supportedRates);
			}
		}

		priv->supported_rates = supportedRates;
		priv->basic_rates = basicRates;

		if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) {
			if (priv->pmib->dot11nConfigEntry.dot11nSupportedMCS == 0)
				priv->pmib->dot11nConfigEntry.dot11nSupportedMCS = 0xffff;
		}
	}
#ifdef CLIENT_MODE
	else
	{
		if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11A) {
			if (priv->pmib->dot11BssType.net_work_type & (WIRELESS_11B | WIRELESS_11G))
				priv->dual_band = 1;
			else
				priv->dual_band = 0;
		}
		else
			priv->dual_band = 0;

		if (priv->dual_band) {
			// for 2.4G band
			supportedRates = priv->pmib->dot11StationConfigEntry.dot11SupportedRates;
			basicRates = priv->pmib->dot11StationConfigEntry.dot11BasicRates;

			if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11B) {
				if (!(priv->pmib->dot11BssType.net_work_type & WIRELESS_11G)) {
					supportedRates &= 0xf;
					basicRates &= 0xf;
				}
				if ((supportedRates & 0xf) == 0)
					supportedRates |= 0xf;
				if ((basicRates & 0xf) == 0)
					basicRates |= 0xf;
			}
			if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11G) {
				if (!(priv->pmib->dot11BssType.net_work_type & WIRELESS_11B)) {
					supportedRates &= 0xff0;
					basicRates &= 0xff0;
				}
				if ((supportedRates & 0xff0) == 0)
					supportedRates |= 0xff0;
				if ((basicRates & 0xff0) == 0)
					basicRates |= 0x1f0;
			}

			priv->supported_rates = supportedRates;
			priv->basic_rates = basicRates;

			// for 5G band
			supportedRates = priv->pmib->dot11StationConfigEntry.dot11SupportedRates;
			basicRates = priv->pmib->dot11StationConfigEntry.dot11BasicRates;

			supportedRates &= 0xff0;
			basicRates &= 0xff0;
			if (supportedRates == 0)
				supportedRates |= 0xff0;
			if (basicRates == 0)
				basicRates |= 0x1f0;

			priv->supported_rates_alt = supportedRates;
			priv->basic_rates_alt = basicRates;
		}
		else {
			supportedRates = priv->pmib->dot11StationConfigEntry.dot11SupportedRates;
			basicRates = priv->pmib->dot11StationConfigEntry.dot11BasicRates;

			if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11B) {
				if (!(priv->pmib->dot11BssType.net_work_type & WIRELESS_11G)) {
					supportedRates &= 0xf;
					basicRates &= 0xf;
				}
				if ((supportedRates & 0xf) == 0)
					supportedRates |= 0xf;
				if ((basicRates & 0xf) == 0)
					basicRates |= 0xf;
			}
			if (priv->pmib->dot11BssType.net_work_type & (WIRELESS_11G | WIRELESS_11A)) {
				if (!(priv->pmib->dot11BssType.net_work_type & WIRELESS_11B)) {
					supportedRates &= 0xff0;
					basicRates &= 0xff0;
				}
				if ((supportedRates & 0xff0) == 0)
					supportedRates |= 0xff0;
				if ((basicRates & 0xff0) == 0)
					basicRates |= 0x1f0;
			}

			priv->supported_rates = supportedRates;
			priv->basic_rates = basicRates;
		}

		if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) {
			if (priv->pmib->dot11nConfigEntry.dot11nSupportedMCS == 0)
				priv->pmib->dot11nConfigEntry.dot11nSupportedMCS = 0xffff;
		}
	}
#endif
#if defined(RTK_AC_SUPPORT)
	if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11AC) {
		
		if (IS_TEST_CHIP(priv))	{
			if(get_rf_mimo_mode(priv) == RF_1T1R) {
				priv->pmib->dot11acConfigEntry.dot11VHT_TxMap &= 0x0ff;
				priv->pmib->dot11acConfigEntry.dot11SupportedVHT = 0xfffc;
			} else if ((get_rf_mimo_mode(priv) == RF_3T3R)||(get_rf_mimo_mode(priv) == RF_4T4R)) {  //eric_8814
				priv->pmib->dot11acConfigEntry.dot11VHT_TxMap &= 0x3fffffff;
				priv->pmib->dot11acConfigEntry.dot11SupportedVHT = 0xffea;
			} else {
				priv->pmib->dot11acConfigEntry.dot11VHT_TxMap &= 0xfffff;
				priv->pmib->dot11acConfigEntry.dot11SupportedVHT = 0xfffa;
			} 
		} else {
			if(get_rf_mimo_mode(priv) == RF_1T1R) {
				priv->pmib->dot11acConfigEntry.dot11VHT_TxMap &= 0x3ff;
				priv->pmib->dot11acConfigEntry.dot11SupportedVHT |= 0xfffc;

				if(!priv->pmib->dot11acConfigEntry.dot11VHT_TxMap)
					priv->pmib->dot11acConfigEntry.dot11VHT_TxMap = 0x3ff;
				if(priv->pmib->dot11acConfigEntry.dot11SupportedVHT == 0xffff)
					priv->pmib->dot11acConfigEntry.dot11SupportedVHT = 0xfffe;
			} else if (get_rf_mimo_mode(priv) == RF_2T2R) { //eric_8814
				priv->pmib->dot11acConfigEntry.dot11VHT_TxMap &= 0xfffff;
				priv->pmib->dot11acConfigEntry.dot11SupportedVHT |= 0xfff0;

				if(!priv->pmib->dot11acConfigEntry.dot11VHT_TxMap)
					priv->pmib->dot11acConfigEntry.dot11VHT_TxMap = 0xfffff;
				if(priv->pmib->dot11acConfigEntry.dot11SupportedVHT == 0xffff)
					priv->pmib->dot11acConfigEntry.dot11SupportedVHT = 0xfffa;
			} else if (get_rf_mimo_mode(priv) == RF_3T3R) {  //eric_8814
				priv->pmib->dot11acConfigEntry.dot11VHT_TxMap &= 0x3fffffff;
				priv->pmib->dot11acConfigEntry.dot11SupportedVHT |= 0xffc0;
				
				if(!priv->pmib->dot11acConfigEntry.dot11VHT_TxMap)
					priv->pmib->dot11acConfigEntry.dot11VHT_TxMap = 0x3fffffff;
				if(priv->pmib->dot11acConfigEntry.dot11SupportedVHT == 0xffff)
					priv->pmib->dot11acConfigEntry.dot11SupportedVHT = 0xffda;
			} else if (get_rf_mimo_mode(priv) == RF_4T4R) {
				priv->pmib->dot11acConfigEntry.dot11VHT_TxMap &= 0x3fffffff; /* FIXME */
				priv->pmib->dot11acConfigEntry.dot11SupportedVHT |= 0xff00;
				
				if(!priv->pmib->dot11acConfigEntry.dot11VHT_TxMap)
					priv->pmib->dot11acConfigEntry.dot11VHT_TxMap = 0x3fffffff;
				if(priv->pmib->dot11acConfigEntry.dot11SupportedVHT == 0xffff)
					priv->pmib->dot11acConfigEntry.dot11SupportedVHT = 0xffaa;
			}
		}
	} 

#endif	
}


void get_oper_rate(struct rtl8192cd_priv *priv)
{
	unsigned int supportedRates=0;
	unsigned int basicRates=0;
	unsigned char val;
	int i, idx=0;

	memset(AP_BSSRATE, 0, sizeof(AP_BSSRATE));
	AP_BSSRATE_LEN = 0;

	if (OPMODE & WIFI_AP_STATE) {
		supportedRates = priv->supported_rates;
		basicRates = priv->basic_rates;
	}
#ifdef CLIENT_MODE
	else {
		if (priv->dual_band && (priv->pshare->curr_band == BAND_5G)) {
			supportedRates = priv->supported_rates_alt;
			basicRates = priv->basic_rates_alt;
		}
		else {
			supportedRates = priv->supported_rates;
			basicRates = priv->basic_rates;
		}
	}
#endif

	for (i=0; dot11_rate_table[i]; i++) {
		int bit_mask = 1 << i;
		if (supportedRates & bit_mask) {
			val = dot11_rate_table[i];

#ifdef SUPPORT_SNMP_MIB
			SNMP_MIB_ASSIGN(dot11SupportedDataRatesSet[i], ((unsigned int)val));
			SNMP_MIB_ASSIGN(dot11OperationalRateSet[i], ((unsigned char)val));
#endif

			if (basicRates & bit_mask)
				val |= 0x80;

			AP_BSSRATE[idx] = val;
			AP_BSSRATE_LEN++;
			idx++;
		}
	}

#ifdef SUPPORT_SNMP_MIB
	SNMP_MIB_ASSIGN(dot11SupportedDataRatesNum, ((unsigned char)AP_BSSRATE_LEN));
#endif

}


// bssrate_ie: _SUPPORTEDRATES_IE_ get supported rate set
// bssrate_ie: _EXT_SUPPORTEDRATES_IE_ get extended supported rate set
int get_bssrate_set(struct rtl8192cd_priv *priv, int bssrate_ie, unsigned char **pbssrate, unsigned int *bssrate_len)
{
	int i;

	if ((priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G)||(priv->pmib->dot11nConfigEntry.dot11nUse40M == CHANNEL_WIDTH_80)) //AC2G_256QAM
	{
#ifdef P2P_SUPPORT			
		if (bssrate_ie == _SUPPORTED_RATES_NO_CCK_) {
				*pbssrate = &dot11_rate_table[4];
				*bssrate_len = 8;
				return TRUE;
		}
#endif			
		if (bssrate_ie == _SUPPORTEDRATES_IE_)
		{

			for(i=0; i<AP_BSSRATE_LEN; i++)									
				if (!is_CCK_rate(AP_BSSRATE[i] & 0x7f))
					break;

			if (i == AP_BSSRATE_LEN)
				return FALSE;
			else {
				*pbssrate = &AP_BSSRATE[i];
				*bssrate_len = AP_BSSRATE_LEN - i;
				return TRUE;
			}
		}
		else
			return FALSE;
	}
	else
	{
		if (bssrate_ie == _SUPPORTEDRATES_IE_)
		{
			*pbssrate = AP_BSSRATE;
			if (AP_BSSRATE_LEN > 8)
				*bssrate_len = 8;
			else
				*bssrate_len = AP_BSSRATE_LEN;
			return TRUE;
		}
#ifdef P2P_SUPPORT
		else if( bssrate_ie == _SUPPORTED_RATES_NO_CCK_){
				*pbssrate = &dot11_rate_table[4];
				*bssrate_len = 8;
				return TRUE;
		}
#endif		
		else
		{
			if (AP_BSSRATE_LEN > 8) {
				*pbssrate = &AP_BSSRATE[8];
				*bssrate_len = AP_BSSRATE_LEN - 8;
				return TRUE;
			}
			else
				return FALSE;
		}
	}
}


struct channel_list{
	unsigned char	channel[31];
	unsigned char	len;
};
#ifdef CONFIG_DLINK_WIFI_CHANNEL_SCHEME
static struct channel_list dlink_reg_channel_2_4g = {{1,2,3,4,5,6,7,8,9,10,11},11};
static struct channel_list dlink_reg_channel_5g = {{36,40,44,48},4};
static struct channel_list dlink_reg_channel_5g_not_dfs = {{36,40,44,48},4};

/* 0 on success, -1 otherwise */
int dlink_fill_chanlist(int is_5g, const char *chanlist)
{
	int i;
	int idx = 0;
	int len;
	int chan_num;
	char *strptr = chanlist;
	struct channel_list reg_channel = {0};

	if (!chanlist)
		return -1;

	len = strlen(chanlist);
	if (len < 1)
		return -1;

	while (idx < sizeof(reg_channel.channel)) {
		char *value = strsep(&strptr, ":");
		if (!value)
			break;

		chan_num = _atoi(value, 10);
		if (!chan_num)
			return -1;

		reg_channel.channel[idx++] = chan_num;
	}

	if (idx == 0)
		return -1;

	reg_channel.len = idx;

	if (is_5g)
		memcpy(&dlink_reg_channel_5g, &reg_channel, sizeof(struct channel_list));
	else
		memcpy(&dlink_reg_channel_2_4g, &reg_channel, sizeof(struct channel_list));

	return 0;
}
#elif defined(CONFIG_DLINK_WIFI_CHANNEL_SCHEME_OLD)
static struct channel_list reg_channel_2_4g[] = {
	/* FCC */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, 11},
	/* IC */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, 11},
	/* ETSI */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 13},
	/* SPAIN */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 13},
	/* FRANCE */		{{10, 11, 12, 13}, 4},
	/* MKK */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 14},
	/* ISRAEL */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 13},
	/* MKK1 */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14}, 14},
	/* MKK2 */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14}, 14},
	/* MKK3 */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14}, 14},
	/* NCC (Taiwan) */	{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, 11},
	/* RUSSIAN */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 13},
	/* CN */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 13},
	/* Global */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14}, 14},
	/* World_wide */	{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 13},
	/* Test */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14}, 14},
	/* 5M10M */		{{}, 0},
	/* SG */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 13},
	/* KR */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 13},
	/* UAE */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 13},
        /* IN */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 13},
        /* EUROPE */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 13},
        /* AU */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 13},
        /* MY */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 13},
        /* IN_ACT */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 13},
        /* BR */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 13},
        /* LA */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 13},
        /* PE */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 13},
        /* TH */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, 11},
        /* ID */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 13},
        /* BD */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 13},
        /* IR */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 13},
        /* EG_TN */		{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}, 13},

};

#ifdef DFS
static struct channel_list reg_channel_5g_full_band[] = {
	/* FCC */		{{36,40,44,48,52,56,60,64,100,104,108,112,116,132,136,140,149,153,157,161,165},21},
	/* IC */		{{36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 132, 136, 140, 149, 153, 157, 161, 165}, 21},
	/* ETSI */		{{36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140}, 19},
	/* SPAIN */		{{36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140}, 19},
	/* FRANCE */		{{36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140}, 19},
	/* MKK */		{{36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144}, 20},
	/* ISRAEL */		{{36, 40, 44, 48, 52, 56, 60, 64}, 8},	
	/* MKK1 */		{{34, 38, 42, 46}, 4},
	/* MKK2 */		{{36, 40, 44, 48}, 4},
	/* MKK3 */		{{36, 40, 44, 48, 52, 56, 60, 64}, 8},
	/* NCC (Taiwan) */	{{36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 132, 136, 140, 149, 153, 157, 161, 165}, 21},
#ifdef CHANNEL_EXTENSION_5GHZ
        // by decision of SCRF (#14269)
        /* RUSSIAN */		{{36, 40, 44, 48, 52, 56, 60, 64, 136, 140, 144, 149, 153, 157, 161, 165}, 16},
#else
        /* RUSSIAN */		{{36, 40, 44, 48, 52, 56, 60, 64}, 8},
#endif
	/* CN */		{{36, 40, 44, 48, 52, 56, 60, 64, 149, 153, 157, 161, 165}, 13},
	/* Global */		{{36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 136, 140, 149, 153, 157, 161, 165}, 20},
	/* World_wide */	{{36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 136, 140, 149, 153, 157, 161, 165}, 20},
	/* Test */		{{36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 149, 153, 157, 161, 165, 169, 173, 177}, 28},
	/* 5M10M */		{{146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170}, 25},
	/* SG */		{{36, 40, 44, 48, 52, 56, 60, 64, 149, 153, 157, 161, 165}, 13},
	/* KR */		{{36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 149, 153, 157, 161}, 19},
	/* UAE */		{{36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 149, 153, 157, 161, 165}, 25},
 	/* IN */		{{36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 149, 153, 157, 161, 165}, 25},
        /* EUROPE */		{{36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140}, 19},
        /* AU */		{{36,40,44,48,52,56,60,64,100,104,108,112,116,132,136,140,149,153,157,161,165}, 21},
        /* MY */		{{36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 149, 153, 157, 161, 165}, 21},
        /* IN_ACT */		{{36, 40, 44, 48, 52, 56, 60, 64, 144, 149, 153, 157, 161, 165}, 14},
        /* BR */		{{36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140,149,153,157,161,165}, 24},
        /* LA */		{{36,40,44,48,52,56,60,64,100,104,108,112,116,132,136,140,149,153,157,161,165}, 21},
        /* PE */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
        /* TH */		{{36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161, 165},24},
        /* ID */		{{149, 153, 157, 161},4},
		/* BD */		{{149, 153, 157, 161, 165},5},
		/* IR */		{{36, 40, 44, 48, 52, 56, 60, 64}, 8},
		/* EG_TN */		{{36, 40, 44, 48, 52, 56, 60, 64}, 8},
};

struct channel_list reg_channel_5g_not_dfs_band[] = {
	/* FCC */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* IC */		{{36,40,44,48,149,153,157,161,165},9},
	/* ETSI */		{{36, 40, 44, 48}, 4},
	/* SPAIN */		{{36, 40, 44, 48}, 4},
	/* FRANCE */		{{36, 40, 44, 48}, 4},
	/* MKK */		{{36, 40, 44, 48}, 4},
	/* ISRAEL */		{{36, 40, 44, 48}, 4},
	/* MKK1 */		{{34, 38, 42, 46}, 4},
	/* MKK2 */		{{36, 40, 44, 48}, 4},
	/* MKK3 */		{{36, 40, 44, 48}, 4},
	/* NCC (Taiwan) */	{{56, 60, 64, 149, 153, 157, 161, 165}, 8},
	/* RUSSIAN */		{{36, 40, 44, 48, 52, 56, 60, 64}, 8},
	/* CN */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* Global */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* World_wide */	{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* Test */		{{36, 40, 44, 48, 149, 153, 157, 161, 165, 169, 173, 177}, 12},
	/* 5M10M */		{{146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170}, 25},
	/* SG */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* KR */		{{36, 40, 44, 48, 149, 153, 157, 161}, 8},
	/* UAE */		{{36, 40, 44, 48, 149, 153, 157, 161}, 8},
	/* IN */		{{36, 40, 44, 48, 52, 56, 60, 64, 149, 153, 157, 161}, 12},
	/* EUROPE */		{{36, 40, 44, 48, 136, 140, 149, 153, 157, 161, 165}, 24},
	/* AU */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* MY */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* IN_ACT */		{{36, 40, 44, 48, 144, 149, 153, 157, 161, 165}, 10},
	/* BR */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* LA */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* PE */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* TH */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* ID */		{{149, 153, 157, 161},4},
	/* BD */		{{149, 153, 157, 161, 165},5},
	/* IR */		{{36, 40, 44, 48}, 4},
	/* EG_TN */		{{36, 40, 44, 48}, 4},
};
#else

// Exclude DFS channels
static struct channel_list reg_channel_5g_full_band[] = {
	/* FCC */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* IC */		{{36,40,44,48,149,153,157,161,165},9},
	/* ETSI */		{{36, 40, 44, 48}, 4},
	/* SPAIN */		{{36, 40, 44, 48}, 4},
	/* FRANCE */		{{36, 40, 44, 48}, 4},
	/* MKK */		{{36, 40, 44, 48}, 4},
	/* ISRAEL */		{{36, 40, 44, 48}, 4},
	/* MKK1 */		{{34, 38, 42, 46}, 4},
	/* MKK2 */		{{36, 40, 44, 48}, 4},
	/* MKK3 */		{{36, 40, 44, 48}, 4},
	/* NCC (Taiwan) */	{{56, 60, 64, 149, 153, 157, 161, 165}, 8},
	/* RUSSIAN */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* CN */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* Global */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* World_wide */	{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* Test */		{{36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 149, 153, 157, 161, 165, 169, 173, 177}, 28},
	/* 5M10M */		{{146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170}, 25},
	/* SG */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* KR */		{{36, 40, 44, 48, 149, 153, 157, 161}, 8},
	/* UAE */		{{36, 40, 44, 48, 149, 153, 157, 161}, 8},
	/* IN */		{{36, 40, 44, 48, 52, 56, 60, 64, 149, 153, 157, 161, 165}, 13},
	/* EUROPE */		{{36, 40, 44, 48, 136, 140, 149, 153, 157, 161, 165}, 24},
	/* AU */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* MY */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* IN_ACT */		{{36, 40, 44, 48, 144, 149, 153, 157, 161, 165}, 10},
	/* BR */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* LA */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* PE */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* TH */		{{36, 40, 44, 48, 149, 153, 157, 161, 165}, 9},
	/* ID */		{{149, 153, 157, 161},4},
	/* BD */		{{149, 153, 157, 161, 165},5},
	/* IR */		{{36, 40, 44, 48}, 4},
	/* EG_TN */		{{36, 40, 44, 48}, 4},
};
#endif

#else /* CONFIG_DLINK_WIFI_CHANNEL_SCHEME_OLD */
static struct channel_list reg_channel_2_4g[] = {
	/* FCC */		{{1,2,3,4,5,6,7,8,9,10,11},11},
	/* IC */		{{1,2,3,4,5,6,7,8,9,10,11},11},
	/* ETSI */		{{1,2,3,4,5,6,7,8,9,10,11,12,13},13},
	/* SPAIN */		{{1,2,3,4,5,6,7,8,9,10,11,12,13},13},
	/* FRANCE */	{{10,11,12,13},4},
	/* MKK */		{{1,2,3,4,5,6,7,8,9,10,11,12,13,14},14},
	/* ISRAEL */	{{3,4,5,6,7,8,9,10,11,12,13},11},
	/* MKK1 */		{{1,2,3,4,5,6,7,8,9,10,11,12,13,14},14},
	/* MKK2 */		{{1,2,3,4,5,6,7,8,9,10,11,12,13,14},14},
	/* MKK3 */		{{1,2,3,4,5,6,7,8,9,10,11,12,13,14},14},
	/* NCC (Taiwan) */	{{1,2,3,4,5,6,7,8,9,10,11},11},
	/* RUSSIAN */	{{1,2,3,4,5,6,7,8,9,10,11,12,13},13},
	/* CN */		{{1,2,3,4,5,6,7,8,9,10,11,12,13},13},
	/* Global */		{{1,2,3,4,5,6,7,8,9,10,11,12,13,14},14},
	/* World_wide */	{{1,2,3,4,5,6,7,8,9,10,11,12,13},13},
	/* Test */		{{1,2,3,4,5,6,7,8,9,10,11,12,13,14},14},
	/* 5M10M */		{{},0},
	/* SG */		{{1,2,3,4,5,6,7,8,9,10,11},11},
	/* KR */		{{1,2,3,4,5,6,7,8,9,10,11,12,13},13},
};

#ifdef DFS
static struct channel_list reg_channel_5g_full_band[] = {
	/* FCC */		{{36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 149, 153, 157, 161, 165}, 25},
	/* IC */		{{36,40,44,48,52,56,60,64,100,104,108,112,116,132,136,140,144,149,153,157,161,165},22},
	/* ETSI */		{{36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140},19},
	/* SPAIN */		{{36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140},19},
	/* FRANCE */	{{36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140},19},
	/* MKK */		{{36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140,144},20},
	/* ISRAEL */	{{36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140},19},
	/* MKK1 */		{{34,38,42,46},4},
	/* MKK2 */		{{36,40,44,48},4},
	/* MKK3 */		{{36,40,44,48,52,56,60,64},8},
	/* NCC (Taiwan) */	{{56,60,64,100,104,108,112,116,136,140,149,153,157,161,165},15},
	/* RUSSIAN */	{{36,40,44,48,52,56,60,64,132,136,140,149,153,157,161,165},16},
	/* CN */		{{36,40,44,48,52,56,60,64,149,153,157,161,165},13},
	/* Global */		{{36,40,44,48,52,56,60,64,100,104,108,112,116,136,140,149,153,157,161,165},20},
	/* World_wide */	{{36,40,44,48,52,56,60,64,100,104,108,112,116,136,140,149,153,157,161,165},20},
	/* Test */		{{36,40,44,48, 52,56,60,64, 100,104,108,112, 116,120,124,128, 132,136,140,144, 149,153,157,161, 165,169,173,177}, 28},
	/* 5M10M */		{{146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170}, 25},
	/* SG */		{{36,40,44,48,149,153,157,161,165},9},
	/* KR */		{{36,40,44,48,52,56,60,64,100,104,108,112,116,136,140,149,153,157,161,165},20},
};

struct channel_list reg_channel_5g_not_dfs_band[] = {
	/* FCC */		{{36,40,44,48,149,153,157,161,165},9},
	/* IC */		{{36,40,44,48,149,153,157,161,165},9},
	/* ETSI */		{{36,40,44,48},4},
	/* SPAIN */		{{36,40,44,48},4},
	/* FRANCE */	{{36,40,44,48},4},
	/* MKK */		{{36,40,44,48},4},
	/* ISRAEL */	{{36,40,44,48},4},
	/* MKK1 */		{{34,38,42,46},4},
	/* MKK2 */		{{36,40,44,48},4},
	/* MKK3 */		{{36,40,44,48},4},
	/* NCC (Taiwan) */	{{56,60,64,149,153,157,161,165},8},
	/* RUSSIAN */	{{36,40,44,48,149,153,157,161,165},9},
	/* CN */		{{36,40,44,48,149,153,157,161,165},9},
	/* Global */		{{36,40,44,48,149,153,157,161,165},9},
	/* World_wide */	{{36,40,44,48,149,153,157,161,165},9},
	/* Test */		{{36,40,44,48, 149,153,157,161, 165,169,173,177}, 12},
	/* 5M10M */		{{146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170}, 25},
	/* SG */		{{36,40,44,48,149,153,157,161,165},9},
	/* KR */		{{36,40,44,48,149,153,157,161,165},9},
};
#else

// Exclude DFS channels
static struct channel_list reg_channel_5g_full_band[] = {
	/* FCC */		{{36,40,44,48,149,153,157,161,165},9},
	/* IC */		{{36,40,44,48,149,153,157,161,165},9},
	/* ETSI */		{{36,40,44,48},4},
	/* SPAIN */		{{36,40,44,48},4},
	/* FRANCE */	{{36,40,44,48},4},
	/* MKK */		{{36,40,44,48},4},
	/* ISRAEL */	{{36,40,44,48},4},
	/* MKK1 */		{{34,38,42,46},4},
	/* MKK2 */		{{36,40,44,48},4},
	/* MKK3 */		{{36,40,44,48},4},
	/* NCC (Taiwan) */	{{56,60,64,149,153,157,161,165},8},
	/* RUSSIAN */	{{36,40,44,48,149,153,157,161,165},9},
	/* CN */		{{36,40,44,48,149,153,157,161,165},9},
	/* Global */		{{36,40,44,48,149,153,157,161,165},9},
	/* World_wide */	{{36,40,44,48,149,153,157,161,165},9},
	/* Test */		{{36,40,44,48, 52,56,60,64, 100,104,108,112, 116,120,124,128, 132,136,140,144, 149,153,157,161, 165,169,173,177}, 28},
	/* 5M10M */		{{146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170}, 25},
	/* SG */		{{36,40,44,48,149,153,157,161,165},9},
	/* KR */		{{36,40,44,48,149,153,157,161,165},9},	
};
#endif
#endif /* CONFIG_DLINK_WIFI_CHANNEL_SCHEME_OLD */


int is_available_channel(struct rtl8192cd_priv *priv, unsigned char channel) {
#ifdef DFS	
	if(priv->pmib->dot11DFSEntry.disable_DFS == 1) {
		if((channel >= 52 && channel <= 144))
			return 0;
	}
#endif	
	if((priv->pmib->dot11RFEntry.band5GSelected & PHY_BAND_5G_1) &&
				(channel >= 36 && channel <= 48))
		return 1;
	else if((priv->pmib->dot11RFEntry.band5GSelected & PHY_BAND_5G_2) &&
				(channel >= 52 && channel <= 64))
		return 1;
	else if((priv->pmib->dot11RFEntry.band5GSelected & PHY_BAND_5G_3) &&
				(channel >= 100 && channel <= 144))
		return 1;
	else if((priv->pmib->dot11RFEntry.band5GSelected & PHY_BAND_5G_4) &&
				(channel >= 149 && channel <= 177))
		return 1;
	else if((priv->pmib->dot11RFEntry.band5GSelected & PHY_BAND_5G_4) &&
				(channel >= 146 && channel <= 177) && 
				(priv->pmib->dot11nConfigEntry.dot11nUse40M == 4 ||priv->pmib->dot11nConfigEntry.dot11nUse40M == 5))
		return 1;
	else
		return 0;
}

int is_which_band_channel(unsigned int channel, unsigned char band){
	int result = 0;
	if(band == PHY_BAND_5G_1 && (channel >= 36 && channel <= 48))
		result  = 1;
	else if (band == PHY_BAND_5G_2 && (channel >= 52 && channel <= 64))
		result = 1;
	else if (band == PHY_BAND_5G_3 && (channel >= 100 && channel <= 144))
		result = 1;
	else if (band == PHY_BAND_5G_4 && (channel >= 149 && channel <= 177))
		result = 1;
	
	return result;
}

int is_available_NonDFS_channel(struct rtl8192cd_priv *priv, unsigned char channel)
{
	if((priv->pmib->dot11RFEntry.band5GSelected == PHY_BAND_5G_3))
		return 0;
	else
		return 1;
}


#define MAX_NUM_80M_CH 7
unsigned int CH_80m[MAX_NUM_80M_CH]={36,52,100,116,132,149,165};

int is80MChannel(unsigned int chnl_list[], unsigned int chnl_num,unsigned int channel)
{
	int idx;
	int chNO;
	int baseCH=0;
	idx = -1;

	if( (CH_80m[MAX_NUM_80M_CH-1] <= channel) && ((CH_80m[MAX_NUM_80M_CH-1]+8) >= channel))
		baseCH = CH_80m[MAX_NUM_80M_CH-1];
	else
	{
		for(chNO=0;chNO<MAX_NUM_80M_CH-1;chNO++) {
			if(CH_80m[chNO] <= channel && CH_80m[chNO+1] > channel) {
				baseCH = CH_80m[chNO];			
				break;
			}
		}
	}

	if(baseCH == 0)
		_DEBUG_ERR("Channel is out of scope\n");
	
	for(idx=0;idx<chnl_num;idx++) {
		// available_chnl is sorted.
		if(chnl_list[idx] == baseCH)
			break;
	}

	if(idx == chnl_num || idx + 3 >= chnl_num)
		return 0;

	if(chnl_list[idx+1] == baseCH + 4 &&
	   chnl_list[idx+2] == baseCH + 8 &&
	   chnl_list[idx+3] == baseCH + 12)
	   return 1;
	else
	   return 0;
}

int is80MSubChannel(unsigned int channel, unsigned int channel2)
{
	int base_ch = 0, base_ch2 = 0;
	int idx;

	for (idx = 0; idx < MAX_NUM_80M_CH - 1; idx++) {
		if (CH_80m[idx] <= channel && CH_80m[idx + 1] > channel)
			base_ch = CH_80m[idx];

		if (CH_80m[idx] <= channel2 && CH_80m[idx + 1] > channel2)
			base_ch2 = CH_80m[idx];

		if (base_ch && base_ch2)
			break;
	}

	if ((base_ch == 0) || (base_ch2 == 0) || (base_ch == base_ch2))
		return 0;

	return 1;
}

int get_80p80_sub_channel(unsigned int chnl_list[], unsigned int chnl_num,unsigned int channel)
{
	int idx, base_ch = 0, diff_ch = 0;
	int search_idx = 0;

	for (idx = 0; idx < MAX_NUM_80M_CH - 1; idx++) {
		if (CH_80m[idx] <= channel && CH_80m[idx + 1] > channel) {
			base_ch = CH_80m[idx];
			search_idx = idx;
			break;
		}
	}

	if (base_ch == 0) {
		_DEBUG_ERR("Channel is out of scope\n");
		return 0;
	}
	
	diff_ch = channel - base_ch;
	
	for (idx = search_idx + 1; idx < MAX_NUM_80M_CH - 1; idx++) {
		if (is80MChannel(chnl_list, chnl_num, CH_80m[idx] + diff_ch))
			return (CH_80m[idx] + diff_ch);
	}

	for (idx = search_idx - 1; idx >= 0; idx--) {
		if (is80MChannel(chnl_list, chnl_num, CH_80m[idx] + diff_ch))
			return (CH_80m[idx] + diff_ch);
	}

	return 0;
}

#define MAX_NUM_40M_CH 14
unsigned int CH_40m[MAX_NUM_40M_CH]={36,44,52,60,100,108,116,124,132,140,149,157,165,173};

int is40MChannel(unsigned int chnl_list[], unsigned int chnl_num,unsigned int channel)
{
	int idx;
	int chNO;
	int baseCH=0;
	idx = -1;

	if( (CH_40m[MAX_NUM_40M_CH-1] <= channel) && ((CH_40m[MAX_NUM_40M_CH-1]+4) >= channel))
		baseCH = CH_40m[MAX_NUM_40M_CH-1];
	else
	{
		for(chNO=0;chNO<MAX_NUM_40M_CH-1;chNO++) {
			if(CH_40m[chNO] <= channel && CH_40m[chNO+1] > channel) {
				baseCH = CH_40m[chNO];			
				break;
			}
		}
	}

	if(baseCH == 0)
		_DEBUG_ERR("Channel is out of scope\n");
	
	for(idx=0;idx<chnl_num;idx++) {
		// available_chnl is sorted.
		if(chnl_list[idx] == baseCH)
			break;
	}

	if(idx == chnl_num || idx + 1 >= chnl_num)
		return 0;

	if(chnl_list[idx+1] == baseCH + 4)
	   return 1;
	else
	   return 0;
}


int find80MChannel(unsigned int chnl_list[], unsigned int chnl_num) {
	int i,j;	
	unsigned int random;
	unsigned int num;
#ifdef __ECOS
	{
		unsigned char random_buf[4];
		get_random_bytes(random_buf, 4);
		random = random_buf[3];
	}
#else
		get_random_bytes(&random, 4);
#endif
		num = random % chnl_num;
	for(i=num;i<chnl_num+num;i++) {
		j=i;
		if (j>=chnl_num)
			j=j-chnl_num;
		if(is80MChannel(chnl_list,chnl_num,chnl_list[j]))
			return chnl_list[j];
	}
	return -1;
}


int find40MChannel(unsigned int chnl_list[], unsigned int chnl_num)
{
	int i,j;	
	unsigned int random;
	unsigned int num;
#ifdef __ECOS
	{
		unsigned char random_buf[4];
		get_random_bytes(random_buf, 4);
		random = random_buf[3];
	}
#else
		get_random_bytes(&random, 4);
#endif
		num = random % chnl_num;
	
	for(i=num;i<chnl_num+num;i++) {
		j=i;
		if (j>=chnl_num)
			j=j-chnl_num;
		if(is40MChannel(chnl_list,chnl_num,chnl_list[j]))
			return chnl_list[j];
	}
	return -1;
}


int get_available_channel(struct rtl8192cd_priv *priv)
{
	int i, reg;
	struct channel_list *ch_5g_lst=NULL;

	priv->available_chnl_num = 0;
	reg = priv->pmib->dot11StationConfigEntry.dot11RegDomain;

	if ((reg < DOMAIN_FCC) || (reg >= DOMAIN_MAX))
		return FAIL;

	if (priv->pmib->dot11BssType.net_work_type & (WIRELESS_11B | WIRELESS_11G) || 
		((priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) &&
			!(priv->pmib->dot11BssType.net_work_type & WIRELESS_11A))) {
#if 0 /*def AC2G_256QAM*/
		if(is_ac2g(priv)) //if 2.4G + 11ac mode, force available channels = 1, 5, 9, 13
		{
				priv->available_chnl[0]=1;
				priv->available_chnl[1]=5;
				priv->available_chnl[2]=9;
				priv->available_chnl[3]=13;
				priv->available_chnl_num=4;
		}
		else
#endif
#ifdef CONFIG_DLINK_WIFI_CHANNEL_SCHEME
        {
            for (i=0; i<dlink_reg_channel_2_4g.len; i++)
                priv->available_chnl[i] = dlink_reg_channel_2_4g.channel[i];
            priv->available_chnl_num += dlink_reg_channel_2_4g.len;
        }

#else
		{
			for (i=0; i<reg_channel_2_4g[reg-1].len; i++)
				priv->available_chnl[i] = reg_channel_2_4g[reg-1].channel[i];
			priv->available_chnl_num += reg_channel_2_4g[reg-1].len;
		}
#endif
	}

	if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11A) {
#ifdef CONFIG_DLINK_WIFI_CHANNEL_SCHEME
        for (i=0; i<dlink_reg_channel_5g.len; i++) {
                if(is_available_channel(priv,dlink_reg_channel_5g.channel[i]))
                    priv->available_chnl[priv->available_chnl_num++] = dlink_reg_channel_5g.channel[i];
        }
#else
		ch_5g_lst = reg_channel_5g_full_band;

		for (i=0; i<ch_5g_lst[reg-1].len; i++) {
				if(is_available_channel(priv,ch_5g_lst[reg-1].channel[i]))
#ifdef CONFIG_DLINK_WIFI_TEST_DOMAIN_CHANNELS
				{
					int forbidden = 0;

					if (reg == DOMAIN_TEST) {
						if (priv->pmib->dot11RFEntry.chanlist && *priv->pmib->dot11RFEntry.chanlist) {

							int chan;
							char *p = priv->pmib->dot11RFEntry.chanlist;

							for (p = priv->pmib->dot11RFEntry.chanlist; p && strstr(p, ";"); p = strstr(p, ";") + 1) {
								if (sscanf(p, "%i", &chan) == 1 && chan == ch_5g_lst[reg - 1].channel[i])
									forbidden = 1;
							}
						}
					}

					if (!forbidden)
						priv->available_chnl[priv->available_chnl_num++] = ch_5g_lst[reg-1].channel[i];
				}
#else
					priv->available_chnl[priv->available_chnl_num++] = ch_5g_lst[reg-1].channel[i];
#endif /* CONFIG_DLINK_WIFI_TEST_DOMAIN_CHANNELS */
		}
#endif /* CONFIG_DLINK_WIFI_CHANNEL_SCHEME */
		
		//for (i=0; i<ch_5g_lst[reg-1].len; i++)
		//	priv->available_chnl[priv->available_chnl_num+i] = ch_5g_lst[reg-1].channel[i];
		//priv->available_chnl_num += ch_5g_lst[reg-1].len;

#ifdef DFS
		/* remove the blocked channels from available_chnl[32] */
		if (priv->NOP_chnl_num)
			for (i=0; i<priv->NOP_chnl_num; i++)
				RemoveChannel(priv, priv->available_chnl, &priv->available_chnl_num, priv->NOP_chnl[i]);

		priv->Not_DFS_chnl_num = 0;
#ifdef CONFIG_DLINK_WIFI_CHANNEL_SCHEME
        // only for init in rtl8192cd_init_sw, after that driver uses only priv->available_chnl from dlink_reg_channel_5g
        for (i=0; i<dlink_reg_channel_5g_not_dfs.len; i++) {
            if(is_available_NonDFS_channel(priv,dlink_reg_channel_5g_not_dfs.channel[i]))
                priv->Not_DFS_chnl[priv->Not_DFS_chnl_num++] = dlink_reg_channel_5g_not_dfs.channel[i];
        }
#else
		for (i=0; i<reg_channel_5g_not_dfs_band[reg-1].len; i++) {
			if(is_available_NonDFS_channel(priv,reg_channel_5g_not_dfs_band[reg-1].channel[i]))
				priv->Not_DFS_chnl[priv->Not_DFS_chnl_num++] = reg_channel_5g_not_dfs_band[reg-1].channel[i];
			
		}
#endif
		
		
#endif
	}

// add by david ---------------------------------------------------
	if (priv->pmib->dot11RFEntry.dot11ch_low ||  priv->pmib->dot11RFEntry.dot11ch_hi) {
		unsigned int tmpbuf[MAX_CHANNEL_NUM];
		int num=0;
		for (i=0; i<priv->available_chnl_num; i++) {
			if ( (priv->pmib->dot11RFEntry.dot11ch_low &&
					priv->available_chnl[i] < priv->pmib->dot11RFEntry.dot11ch_low) ||
				(priv->pmib->dot11RFEntry.dot11ch_hi &&
					priv->available_chnl[i] > priv->pmib->dot11RFEntry.dot11ch_hi))
				continue;
			else
				tmpbuf[num++] = priv->available_chnl[i];
		}
		if (num) {
			memcpy(priv->available_chnl, tmpbuf, num*4);
			priv->available_chnl_num = num;
		}
	}
//------------------------------------------------------ 2007-04-14

	return SUCCESS;
}


#define GET_STAT_INFO_PE_SYNC_SIZE(pstat_pe)  ((unsigned int)((unsigned long)(&pstat_pe.end_of_para_sync)-(unsigned long)(&pstat_pe)))
#define SYNC_DATA(dest,src,member) memcpy(&dest->member,&src->member,sizeof(src->member))
#define SYNC_ARRAY(dest,src,member) memcpy(dest->member,src->member,sizeof(src->member))
#define SYNC_STRCUT(dest,src,member) memcpy(&dest->member,&src->member,sizeof(src->member))  

void get_stat_info_pe(struct stat_info *pstat,struct stat_info_pe *pstat_pe)
{
	//need to be enhance
	SYNC_DATA(pstat_pe,pstat,tx_bw_bak);
	SYNC_DATA(pstat_pe,pstat,auth_seq);
	SYNC_STRCUT(pstat_pe,pstat,cmn_info);
#ifdef WIFI_WMM	
	SYNC_DATA(pstat_pe,pstat,QosEnabled);
#endif
	SYNC_DATA(pstat_pe,pstat,state);
	SYNC_DATA(pstat_pe,pstat,cnt_sleep);
	SYNC_DATA(pstat_pe,pstat,AuthAlgrthm);
	SYNC_DATA(pstat_pe,pstat,ieee8021x_ctrlport);
	SYNC_ARRAY(pstat_pe,pstat,bssrateset);
	SYNC_DATA(pstat_pe,pstat,useShortPreamble);
	SYNC_DATA(pstat_pe,pstat,expire_to);
	SYNC_DATA(pstat_pe,pstat,rssi);
	SYNC_DATA(pstat_pe,pstat,is_realtek_sta);
	SYNC_DATA(pstat_pe,pstat,IOTPeer);
	SYNC_DATA(pstat_pe,pstat,no_rts);
	SYNC_DATA(pstat_pe,pstat,check_init_tx_rate);
	SYNC_DATA(pstat_pe,pstat,is_forced_ampdu);
	SYNC_DATA(pstat_pe,pstat,tx_bw);
	SYNC_DATA(pstat_pe,pstat,ht_current_tx_info);
	SYNC_DATA(pstat_pe,pstat,current_tx_rate);
#if defined(RTK_AC_SUPPORT) || defined(RTK_AC_TX_SUPPORT)
	SYNC_STRCUT(pstat_pe,pstat,vht_cap_buf);
	SYNC_DATA(pstat_pe,pstat,vht_cap_len);
	SYNC_STRCUT(pstat_pe,pstat,vht_oper_buf);
	SYNC_DATA(pstat_pe,pstat,vht_oper_len);
	SYNC_DATA(pstat_pe,pstat,nss);
#endif
	SYNC_STRCUT(pstat_pe,pstat,ht_cap_buf);
	SYNC_DATA(pstat_pe,pstat,ht_cap_len);
	SYNC_DATA(pstat_pe,pstat,MIMO_ps);
	SYNC_DATA(pstat_pe,pstat,is_8k_amsdu);
	SYNC_DATA(pstat_pe,pstat,amsdu_level);
	SYNC_DATA(pstat_pe,pstat,is_legacy_encrpt);
#if defined(CONFIG_RTL_88E_SUPPORT) || defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL) || defined(CONFIG_RTL_8723B_SUPPORT)
	SYNC_DATA(pstat_pe,pstat,txpause_flag);
#endif
	SYNC_DATA(pstat_pe,pstat,txpause_flag);
	SYNC_DATA(pstat_pe,pstat,txpause_flag);

#ifdef CONFIG_RTL_OFFLOAD_DRIVER
	SYNC_ARRAY(pstat_pe,pstat,txie);
#endif

}

void set_rts_protect_all(struct rtl8192cd_priv *priv, unsigned char enable)
{
	if(enable)
		RTL_W8(0x593,RTL_R8(0x593)|BIT1); // RTS Protect ALL			
	else
		RTL_W8(0x593,RTL_R8(0x593)& ~BIT1); // RTS Protect ALL			
}

void cnt_assoc_num(struct rtl8192cd_priv *priv, struct stat_info *pstat, int act, char *func)
{
#if defined(CONFIG_RTL_OFFLOAD_DRIVER) && defined(CONFIG_PE_ENABLE)
	struct stat_info_pe stat_pe;
#endif
#ifdef CONFIG_RTL_92D_SUPPORT
	int i;
#endif

#ifdef INDICATE_LINK_CHANGE
	if (!IEEE8021X_FUN && ((_NO_PRIVACY_ == priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm) ||
			(_WEP_40_PRIVACY_ == priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm) ||
			(_WEP_104_PRIVACY_ == priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm))) {
		indicate_sta_link_change(priv, pstat, act, func);
	}
#endif
	
	if (act == INCREASE) {
		if (priv->assoc_num <= NUM_STAT) {
			priv->assoc_num++;
#ifdef TLN_STATS
			if (priv->assoc_num > priv->wifi_stats.max_sta) {
				priv->wifi_stats.max_sta = priv->assoc_num;
				priv->wifi_stats.max_sta_timestamp = priv->up_time;
			}
#endif
			priv->pshare->total_assoc_num++;
#ifdef TC_CONCURRENT_FT
			if (pstat->ht_cap_len) {
				if(pstat->ht_cap_buf.support_mcs[1])
					priv->assoc_num_2x2++;
				else
					priv->assoc_num_1x1++;
			}
#endif

#ifdef CONFIG_SPECIAL_ENV_TEST
			if (pstat->IOTPeer == HT_IOT_PEER_SPIRENT)
			{
				priv->pshare->spirent_sta_num++;  
			}
			
			if (pstat->IOTPeer == HT_IOT_PEER_VERIWAVE)
			{
				if (priv->pshare->veriwave_sta_num == 0 || 
					(priv->pshare->rf_ft_var.veriwave_test && priv->assoc_num == 1)) {
#ifdef CONFIG_ADAPTIVE_SOML					
					odm_cmn_info_update(ODMPTR, ODM_CMNINFO_ABILITY, ODMPTR->support_ability & ~ODM_BB_ADAPTIVE_SOML);
					phydm_soml_on_off(ODMPTR, SOML_OFF);					
#endif
					ODMPTR->original_dig_restore = 1;
			
					// reject Destination Address packets
					if (RTL_R8(0x608) & RCR_AAP)
						RTL_W8(0x608, RTL_R8(0x608) & ~RCR_AAP);
			
				    if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G)
						RTL_W16(REG_RETRY_LIMIT, 0x2020);
					else
						RTL_W16(REG_RETRY_LIMIT, 0x3030);
				}

				priv->pshare->veriwave_sta_num++;  
			}
#endif
			
#if defined(CONFIG_RTL_88E_SUPPORT) && defined(TXREPORT)
			if (GET_CHIP_VER(priv) == VERSION_8188E) {
				update_remapAid(priv, pstat);
	#if defined(CONFIG_PCI_HCI)
				if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
					RTL8188E_AssignTxReportMacId(priv);
					if (priv->pshare->total_assoc_num == 1)
						RTL8188E_ResumeTxReport(priv);
				}
	#endif
	#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
				if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
					notify_tx_report_change(priv);
				}
	#endif
			}
#endif
#if 0
#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
			if (IS_ROOT_INTERFACE(priv))
#endif
			{
				if (priv->assoc_num > 1)
					check_DIG_by_rssi(priv, 0);	// force DIG temporary off for association after the fist one
			}
#endif
			if (pstat->ht_cap_len) {
				priv->pshare->ht_sta_num++;
				if (priv->pshare->iot_mode_enable && (priv->pshare->ht_sta_num == 1)
#ifdef RTL_MANUAL_EDCA
						&& (priv->pmib->dot11QosEntry.ManualEDCA == 0)
#endif
						) {
#ifdef USE_OUT_SRC
#ifdef _OUTSRC_COEXIST
						if(IS_OUTSRC_CHIP(priv))
#endif
						IotEdcaSwitch(GET_ROOT(priv), priv->pshare->iot_mode_enable);
#endif
#if !defined(USE_OUT_SRC) || defined(_OUTSRC_COEXIST)
#ifdef _OUTSRC_COEXIST
						if(!IS_OUTSRC_CHIP(priv))
#endif
						IOT_EDCA_switch(priv, priv->pmib->dot11BssType.net_work_type, priv->pshare->iot_mode_enable);
#endif
				}

#ifdef WIFI_11N_2040_COEXIST
				if (priv->pmib->dot11nConfigEntry.dot11nCoexist && priv->pshare->is_40m_bw &&
					(priv->pmib->dot11BssType.net_work_type & (WIRELESS_11N|WIRELESS_11G))) {
					if (pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_40M_INTOLERANT_)) {
						if (OPMODE & WIFI_AP_STATE) {
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL) || defined(CONFIG_RTL_8723B_SUPPORT)	
							unsigned int force_20_stamap;
							force_20_stamap = orSTABitMap(&priv->force_20_sta);
							setSTABitMap(&priv->force_20_sta, pstat->cmn_info.aid);
							// force all STA switch TXBW to 20M
							if (0 == force_20_stamap) {
								update_RAMask_to_FW(priv, 0);
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
								dc_if_update(priv);
#endif
							}
#else
							setSTABitMap(&priv->force_20_sta, pstat->cmn_info.aid);
#endif
#if defined(WIFI_11N_2040_COEXIST_EXT)
							clearSTABitMap(&priv->pshare->_40m_staMap, pstat->cmn_info.aid);

#endif
						} 
					}
				}
#endif

				check_NAV_prot_len(priv, pstat, 0);
			}
#ifdef MULTI_STA_REFINE
			if (priv->pshare->total_assoc_num > 10 && !is_veriwave_testing(priv)) {
				priv->ht_protection = 1;
				priv->ht_legacy_sta_num = 100;
			}
#endif	
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)					
			if (GET_CHIP_VER(priv) == VERSION_8812E || GET_CHIP_VER(priv) == VERSION_8723B) {
				UpdateHalMSRRPT8812(priv, pstat, INCREASE);		
				pstat->txpdrop_flag =0;	
				RTL8812_MACID_PAUSE(priv, 0, REMAP_AID(pstat));
				if(priv->pshare->paused_sta_num && pstat->txpause_flag) {
					priv->pshare->paused_sta_num--;	
					pstat->txpause_flag =0;
	        	}   				
			}
#endif
#ifdef CONFIG_WLAN_HAL
            if (IS_HAL_CHIP(priv)) {
               if(pstat->cmn_info.aid <128)
               {
                    GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, pstat, INCREASE);				
					pstat->txpdrop_flag =0;
                    GET_HAL_INTERFACE(priv)->SetMACIDSleepHandler(priv, 0, REMAP_AID(pstat));
					if(priv->pshare->paused_sta_num && pstat->txpause_flag) {
						priv->pshare->paused_sta_num--;	
						pstat->txpause_flag =0;
		        	}            
                    pstat->bDrop = 0; 
                }
            }
#endif

#ifdef HW_FILL_MACID
        if (IS_SUPPORT_HW_FILL_MACID(priv) && (!IS_SUPPORT_ADDR_CAM(priv)))  {
        //Init fill mac address,Init fill CRC5
        GET_HAL_INTERFACE(priv)->SetTxRPTHandler(priv, REMAP_AID(pstat), TXRPT_VAR_MAC_ADDRESS, &pstat->cmn_info.mac_addr);  
		GDEBUG("HW_FILL_MACID, remap_mac_id=%x,addr=%02X%02X%02X%02X%02X%02X \n",
			REMAP_AID(pstat),
		    pstat->cmn_info.mac_addr[0],
			pstat->cmn_info.mac_addr[1],
			pstat->cmn_info.mac_addr[2],
			pstat->cmn_info.mac_addr[3],
			pstat->cmn_info.mac_addr[4],
			pstat->cmn_info.mac_addr[5]);
			
        GET_HAL_INTERFACE(priv)->SetCRC5ToRPTBufferHandler(priv,CRC5(&pstat->cmn_info.mac_addr,6), REMAP_AID(pstat),1); 
		GDEBUG("HW_FILL_MACID, remap_mac_id=%x ,priv0_aid=%d,priv0_mac_id=%d,crc5=%x,  pstat=%x\n",
			REMAP_AID(pstat),
			priv->pshare->aidarray[0]->station.cmn_info.aid, 
			priv->pshare->aidarray[0]->station.cmn_info.mac_id,
			CRC5(&pstat->cmn_info.mac_addr,6),pstat);
		
        }       
#endif //#ifdef HW_FILL_MACID            
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
		dc_sta_add(priv, pstat);
#ifdef CONFIG_PE_ENABLE
	if(priv->pshare->rf_ft_var.manual_pe_enable){
		get_stat_info_pe(pstat,&stat_pe);
		rtl_ipc_tx_handler(CMD_ADD_STA,&stat_pe,GET_STAT_INFO_PE_SYNC_SIZE(stat_pe));
	
	}
#endif
#endif
#if defined(CONFIG_BRIDGE) && defined(CONFIG_RTL_819X_SWCORE)
			if (OPMODE & WIFI_AP_STATE) {
				if(pstat) {
					/* HH71VM: was `&pstat->cmn_info.mac_addr`. mac_addr is an array of u8[6], so `&` gives
 * a pointer to an array (u8 (*)[6]), and the function expects `const unsigned char *`. Array name and so on
 * decomposes into a pointer to the first element - the extra `&` is removed. The address value is the same
 * only the type changes, so the behavior is not affected. */
rtl_wifi_mac_create_hooks(pstat->cmn_info.mac_addr);
				}
			}
#endif

		} else {
			DEBUG_ERR("Association Number Error (%d)!\n", NUM_STAT);
		}
	} else {
		if (priv->assoc_num > 0) {
			priv->assoc_num--;
			priv->pshare->total_assoc_num--;
#ifdef TC_CONCURRENT_FT
			if (pstat->ht_cap_len) {
				if(pstat->ht_cap_buf.support_mcs[1])
					priv->assoc_num_2x2--;
				else
					priv->assoc_num_1x1--;	
			}
#endif

#ifdef CONFIG_SPECIAL_ENV_TEST
			if (pstat->IOTPeer == HT_IOT_PEER_SPIRENT)
			{
				priv->pshare->spirent_sta_num--;
			}
			
			if (pstat->IOTPeer == HT_IOT_PEER_VERIWAVE)
			{
				priv->pshare->veriwave_sta_num--;  
			}
#endif

#if defined(CONFIG_RTL_88E_SUPPORT) && defined(TXREPORT)
			if (GET_CHIP_VER(priv) == VERSION_8188E) {
	#if defined(CONFIG_PCI_HCI)
				if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
					if (!priv->pshare->total_assoc_num)
						RTL8188E_SuspendTxReport(priv);
					else
						RTL8188E_AssignTxReportMacId(priv);
				}
	#endif
	#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
				if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
					notify_tx_report_change(priv);
				}
	#endif
			}
#endif
#if defined(CONFIG_POWER_SAVE)
			if (priv->pshare->total_assoc_num == 0 && GET_ROOT(priv)->pmib->dot11OperationEntry.ps_level > 0)
			{
				rtw_lock_suspend_timeout(priv, 2000);
			}
#endif

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
			if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
				if (0 == priv->assoc_num) {
					rtw_txservq_flush(priv, &priv->tx_mc_queue);
				}
			}
#endif

#if 0
#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
			if (IS_ROOT_INTERFACE(priv))
#endif
				if (!priv->assoc_num) {
#ifdef INTERFERENCE_CONTROL
					if (priv->pshare->rf_ft_var.nbi_filter_enable)
						check_NBI_by_rssi(priv, 0xFF);	// force NBI on while no station associated
#else
	#if defined(CONFIG_PCI_HCI)
					if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
						check_DIG_by_rssi(priv, 0);	// force DIG off while no station associated
					}
	#endif
	#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
					if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
						notify_check_DIG_by_rssi(priv, 0);
					}
	#endif
#endif
				}
#endif
			if (pstat->ht_cap_len) {
				if (priv->pshare->ht_sta_num == 0) {
					printk("ht_sta_num error\n");  // this should not happen
				} else {
					priv->pshare->ht_sta_num--;
					if (priv->pshare->iot_mode_enable && !priv->pshare->ht_sta_num
#ifdef RTL_MANUAL_EDCA
							&& (priv->pmib->dot11QosEntry.ManualEDCA == 0)
#endif
							) {
#ifdef USE_OUT_SRC
#ifdef _OUTSRC_COEXIST
							if(IS_OUTSRC_CHIP(priv))
#endif
							IotEdcaSwitch(priv, priv->pshare->iot_mode_enable);
#endif

#if !defined(USE_OUT_SRC) || defined(_OUTSRC_COEXIST)
#ifdef _OUTSRC_COEXIST
							if(!IS_OUTSRC_CHIP(priv))
#endif
							IOT_EDCA_switch(priv, priv->pmib->dot11BssType.net_work_type, priv->pshare->iot_mode_enable);
#endif
					}
#ifdef WIFI_11N_2040_COEXIST
					if (priv->pmib->dot11nConfigEntry.dot11nCoexist && priv->pshare->is_40m_bw &&
						(priv->pmib->dot11BssType.net_work_type & (WIRELESS_11N|WIRELESS_11G))) {
						if (pstat->ht_cap_buf.ht_cap_info & cpu_to_le16(_HTCAP_40M_INTOLERANT_)) {
							if (OPMODE & WIFI_AP_STATE) {
                                clearSTABitMap(&priv->force_20_sta, pstat->cmn_info.aid);
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL) || defined(CONFIG_RTL_8723B_SUPPORT)
                                update_RAMask_to_FW(priv, 0);
#endif
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
								dc_if_update(priv);
#endif
								SetTxPowerLevel(priv, priv->pmib->dot11RFEntry.dot11channel);
							}
						}
					}
#endif

					check_NAV_prot_len(priv, pstat, 1);
				}
			}
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT) 				
			if (GET_CHIP_VER(priv) == VERSION_8812E || GET_CHIP_VER(priv) == VERSION_8723B) {
				UpdateHalMSRRPT8812(priv, pstat, DECREASE);
				pstat->txpdrop_flag =1;					
			}
#endif
#ifdef CONFIG_WLAN_HAL
            if (IS_HAL_CHIP(priv)) {
                if(pstat->cmn_info.aid <128)
                {
                    GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, pstat, DECREASE);
                    pstat->bDrop = 1;                      
                }
            }
#endif
#ifdef HW_FILL_MACID
        if (IS_SUPPORT_HW_FILL_MACID(priv) && (!IS_SUPPORT_ADDR_CAM(priv)))  {
            // Set hwaddr zero
            u1Byte hwaddr[] = {0x00,0x00,0x00,0x00,0x00,0x00};
            GET_HAL_INTERFACE(priv)->SetTxRPTHandler(priv, REMAP_AID(pstat), TXRPT_VAR_MAC_ADDRESS, &hwaddr);                         
            GET_HAL_INTERFACE(priv)->SetCRC5ToRPTBufferHandler(priv,CRC5(&hwaddr,6), REMAP_AID(pstat),0); 
			GDEBUG("HW_FILL_MACID, aid=%x ,crc5=%x, addr=%02X%02X%02X%02X%02X%02X\n",
				REMAP_AID(pstat),CRC5(&hwaddr,6),
				hwaddr[0],
				hwaddr[1],
				hwaddr[2],
				hwaddr[3],
				hwaddr[4],
				hwaddr[5]);	
			
        }
#endif //#ifdef HW_FILL_MACID                                                  
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
	dc_sta_remove(priv, pstat);

#ifdef CONFIG_PE_ENABLE
	if(priv->pshare->rf_ft_var.manual_pe_enable){
		get_stat_info_pe(pstat,&stat_pe);
		rtl_ipc_tx_handler(CMD_DEL_STA,&stat_pe,GET_STAT_INFO_PE_SYNC_SIZE(stat_pe));
	}
#endif
#endif

#if defined(CONFIG_BRIDGE) && defined(CONFIG_RTL_819X_SWCORE)
			if (OPMODE & WIFI_AP_STATE) {
				if(pstat) {
					/* HH71VM: extra `&` has been removed, see explanation in rtl_wifi_mac_create_hooks above. */
rtl_wifi_mac_del_hooks(pstat->cmn_info.mac_addr);
				}
			}
#endif
		} else {
			DEBUG_ERR("Association Number Error (0)!\n");
		}
	}
#if defined(MULTI_STA_REFINE) && defined(TX_SHORTCUT)
	if(((act == INCREASE) && ((priv->pshare->total_assoc_num==11) || (priv->pshare->total_assoc_num==31)))
	|| ((act == DECREASE) && ((priv->pshare->total_assoc_num==9)  || (priv->pshare->total_assoc_num==29)))){
		clearAllTxShortCut(priv);
	}
#endif

#ifdef CONFIG_RTL_92D_SUPPORT
	if (GET_CHIP_VER(priv) == VERSION_8192D){
		for (i=NUM_STAT-1; i>=0 ; i--){
			if (priv->pshare->aidarray[i]!=NULL){
				if (priv->pshare->aidarray[i]->used)
						break;
			}
		}
		priv->pshare->max_fw_macid = priv->pshare->aidarray[i]->station.cmn_info.aid+1; // fw check macid num from 1~32, so we add 1 to index.
		if (priv->pshare->max_fw_macid > (NUM_STAT+1))
			priv->pshare->max_fw_macid = (NUM_STAT+1);
	}
#endif

#ifdef TC_CONCURRENT_FT
	if ((GET_ROOT(priv)->assoc_num_2x2 == GET_ROOT(priv)->assoc_num_1x1) && (GET_ROOT(priv)->assoc_num_2x2 > 3))
		priv->pshare->tc_msta_mode = 1;
	else
		priv->pshare->tc_msta_mode = 0;
#if (defined(CONFIG_WLAN_HAL_8197F)||defined(CONFIG_WLAN_HAL_8197G)) && defined(CONFIG_WLAN_HAL_8812F)
	extern u32 if_priv[NUM_WLAN_IFACE];
	if (((struct rtl8192cd_priv *)if_priv[0])->pshare->tc_msta_mode
		&& ((struct rtl8192cd_priv *)if_priv[1])->pshare->tc_msta_mode) {
		((struct rtl8192cd_priv *)if_priv[0])->pshare->tc_msta_dual = 1;
		((struct rtl8192cd_priv *)if_priv[1])->pshare->tc_msta_dual = 1;
	} else {
		((struct rtl8192cd_priv *)if_priv[0])->pshare->tc_msta_dual = 0;
		((struct rtl8192cd_priv *)if_priv[1])->pshare->tc_msta_dual = 0;
	}
#endif
#endif

	DEBUG_INFO("assoc_num%s(%d) in %s %02X%02X%02X%02X%02X%02X\n",
		act?"++":"--",
		priv->assoc_num,
		func,
		pstat->cmn_info.mac_addr[0],
		pstat->cmn_info.mac_addr[1],
		pstat->cmn_info.mac_addr[2],
		pstat->cmn_info.mac_addr[3],
		pstat->cmn_info.mac_addr[4],
		pstat->cmn_info.mac_addr[5]);
}

#ifdef INDICATE_LINK_CHANGE
void indicate_sta_link_change(struct rtl8192cd_priv *priv, struct stat_info *pstat, int act, char *func)
{
	DOT11_LINK_CHANGE_IND LinkChange_Ind;
	
	if ((INCREASE == act) && !pstat->link_up) {
		pstat->link_up = 1;
		// do something about STA addition
	} else if ((DECREASE == act) && pstat->link_up) {
		pstat->link_up = 0;
		// do something about STA removal
	} else {
		return;
	}
	
	memcpy(LinkChange_Ind.MACAddr, pstat->cmn_info.mac_addr, MACADDRLEN);
	LinkChange_Ind.EventId = DOT11_EVENT_LINK_CHANGE_IND;
	LinkChange_Ind.IsMoreEvent = 0;
	LinkChange_Ind.LinkStatus = pstat->link_up;
	DOT11_EnQueue((unsigned long)priv, priv->pevent_queue, (UINT8 *)&LinkChange_Ind,
				sizeof(DOT11_LINK_CHANGE_IND));

	DEBUG_INFO("%s%s in %s %02X%02X%02X%02X%02X%02X\n",
		__func__,
		act ? "++" : "--",
		func,
		pstat->cmn_info.mac_addr[0], pstat->cmn_info.mac_addr[1], pstat->cmn_info.mac_addr[2],
		pstat->cmn_info.mac_addr[3], pstat->cmn_info.mac_addr[4], pstat->cmn_info.mac_addr[5]);
}
#endif // INDICATE_LINK_CHANGE




#ifdef CONFIG_RTL8672
#define TX_PACKETS_COUNTER  (priv->net_stats.tx_packets + priv->ext_stats.tx_mgnt_pkts);
#define RX_PACKETS_COUNTER  (priv->net_stats.rx_packets + priv->ext_stats.rx_mgnt_pkts);
#else
#define TX_PACKETS_COUNTER  (priv->net_stats.tx_packets);
#define RX_PACKETS_COUNTER  (priv->net_stats.rx_packets);
#endif

void update_all_wifi_txrx_pkts(struct rtl8192cd_priv *rootPriv)
{
	struct rtl8192cd_priv *priv = rootPriv;
	unsigned long rx_pkt=0, tx_pkt=0;
#if defined(MBSSID) || defined(UNIVERSAL_REPEATER)
	unsigned char i=1;
#endif  

	rx_pkt = RX_PACKETS_COUNTER;
	tx_pkt = TX_PACKETS_COUNTER;

#ifdef MBSSID
	for (i=0; i<RTL8192CD_NUM_VWLAN; i++){
		priv = rootPriv->pvap_priv[i];
		if (IS_DRV_OPEN(priv)) {
			rx_pkt += RX_PACKETS_COUNTER;
			tx_pkt += TX_PACKETS_COUNTER;
		}
	}
#endif

#ifdef UNIVERSAL_REPEATER
	priv = GET_VXD_PRIV(rootPriv);
	if (IS_DRV_OPEN(priv)){
		rx_pkt += RX_PACKETS_COUNTER;
		tx_pkt += TX_PACKETS_COUNTER;
	}
#endif

	rootPriv->pshare->tx_packets_pre2 = rootPriv->pshare->tx_packets_pre;
	rootPriv->pshare->tx_packets_pre = rootPriv->pshare->tx_packets_total;
	rootPriv->pshare->tx_packets_total = tx_pkt;
	
	rootPriv->pshare->rx_packets_pre2 = rootPriv->pshare->rx_packets_pre;
	rootPriv->pshare->rx_packets_pre = rootPriv->pshare->rx_packets_total;
	rootPriv->pshare->rx_packets_total = rx_pkt;
}


#if defined(CONFIG_VERIWAVE_CHECK) && (defined(CONFIG_VERIWAVE_MACBBTX) || defined(CONFIG_VERIWAVE_DIG_OPMODE))
//rootPriv should be root and not NULL
void update_wifi_allitf_txrx_stats(struct rtl8192cd_priv *rootPriv)
{
	struct rtl8192cd_priv *priv = rootPriv;
	unsigned int rx_pkt=0, tx_pkt=0;
	unsigned int rx_bytes=0, tx_bytes=0;
#if defined(MBSSID) || defined(UNIVERSAL_REPEATER)
	unsigned char i=1;
#endif

	if (priv == NULL)
		return;

#ifdef INTERFERENCE_CONTROL
	//priv->pshare->fa_all_counter = priv->pshare->ofdm_FA_total_cnt,
#else
	//priv->pshare->fa_all_counter = priv->pshare->ofdm_FA_cnt1+priv->pshare->ofdm_FA_cnt2+priv->pshare->ofdm_FA_cnt3+priv->pshare->ofdm_FA_cnt4,
#endif
	//priv->pshare->fa_all_counter += priv->pshare->cck_FA_cnt;

	rx_pkt = RX_PACKETS_COUNTER;
	tx_pkt = TX_PACKETS_COUNTER;
	rx_bytes = priv->net_stats.rx_bytes;
	tx_bytes = priv->net_stats.tx_bytes;  

#ifdef MBSSID
	for (i=0; i<RTL8192CD_NUM_VWLAN; i++){
		priv = rootPriv->pvap_priv[i];
		if (IS_DRV_OPEN(priv)) {
			rx_pkt += RX_PACKETS_COUNTER;
			tx_pkt += TX_PACKETS_COUNTER;
			rx_bytes += priv->net_stats.rx_bytes;
			tx_bytes += priv->net_stats.tx_bytes;
		}
	}
#endif

#ifdef UNIVERSAL_REPEATER
	priv = GET_VXD_PRIV(rootPriv);
	if (IS_DRV_OPEN(priv)){
		rx_pkt += RX_PACKETS_COUNTER;
		tx_pkt += TX_PACKETS_COUNTER;
		rx_bytes += priv->net_stats.rx_bytes;
		tx_bytes += priv->net_stats.tx_bytes;
	}
#endif

	rootPriv->pshare->wifi_rx_pkts[0] = rootPriv->pshare->wifi_rx_pkts[1];
	rootPriv->pshare->wifi_rx_pkts[1] = rootPriv->pshare->wifi_rx_pkts[2];
	rootPriv->pshare->wifi_tx_pkts[0] = rootPriv->pshare->wifi_tx_pkts[1];
	rootPriv->pshare->wifi_tx_pkts[1] = rootPriv->pshare->wifi_tx_pkts[2];

	rootPriv->pshare->wifi_rx_pkts[2] = rx_pkt;
	rootPriv->pshare->wifi_tx_pkts[2] = tx_pkt;

	rootPriv->pshare->wifi_rx_bytes[0] = rootPriv->pshare->wifi_rx_bytes[1];
	rootPriv->pshare->wifi_rx_bytes[1] = rootPriv->pshare->wifi_rx_bytes[2];
	rootPriv->pshare->wifi_tx_bytes[0] = rootPriv->pshare->wifi_tx_bytes[1];
	rootPriv->pshare->wifi_tx_bytes[1] = rootPriv->pshare->wifi_tx_bytes[2];

	rootPriv->pshare->wifi_rx_bytes[2] = rx_bytes;
	rootPriv->pshare->wifi_tx_bytes[2] = tx_bytes;
}
#endif



#ifdef WIFI_HAPD
int event_indicate_hapd(struct rtl8192cd_priv *priv, unsigned char *mac, int event, unsigned char *extra)
{
	struct net_device	*dev = (struct net_device *)priv->dev;
	union iwreq_data wreq;

	if(OPMODE & WIFI_AP_STATE)
	{

		printk("event_indicate_hapd +++, event =0x%x\n", event);

	memset(&wreq, 0, sizeof(wreq));

	if(event == HAPD_EXIRED)
		{
			memcpy(wreq.addr.sa_data, mac, 6);
			wireless_send_event(dev, IWEVEXPIRED, &wreq, NULL);
			return 0;
		}
	else if(event == HAPD_REGISTERED)
		{
			memcpy(wreq.addr.sa_data, mac, 6);
			wireless_send_event(dev, IWEVREGISTERED, &wreq, NULL);
			return 0;
		}
	else if(event == HAPD_MIC_FAILURE)
		{
			char buf[6];
			memcpy(buf, mac, 6);
			wreq.data.flags = event;
			wreq.data.length = 6;
			wireless_send_event(dev, IWEVCUSTOM, &wreq, buf);
			return 0;
		}
	else if(event == HAPD_WPS_PROBEREQ)
		{
			wreq.data.flags = event;
			wreq.data.length = sizeof(struct _DOT11_PROBE_REQUEST_IND);
			wireless_send_event(dev, IWEVGENIE, &wreq, extra); //IW_CUSTOM_MAX is 256, can NOT afford  _DOT11_PROBE_REQUEST_IND
					return 0;
		}		
	else{
			//Not used yet
			wreq.data.flags = event;
			wireless_send_event(dev, IWEVCUSTOM, &wreq, extra);
			return 0;
		}
	}

	return -1;
}
#endif

#ifdef WIFI_WPAS
int event_indicate_wpas(struct rtl8192cd_priv *priv, unsigned char *mac, int event, unsigned char *extra)
{
	struct net_device	*dev = (struct net_device *)priv->dev;
	union iwreq_data wreq;

	if(OPMODE & WIFI_STATION_STATE)
	{
		printk("event_indicate_wpas +++ event = 0x%x\n", event);
		
		memset(&wreq, 0, sizeof(wreq));

		if(event == WPAS_EXIRED)
			{
				memcpy(wreq.addr.sa_data, mac, 6);
				wireless_send_event(dev, IWEVEXPIRED, &wreq, NULL);
				return 0;
			}
		else if(event == WPAS_REGISTERED)
			{
				memcpy(wreq.addr.sa_data, mac, 6);
				wireless_send_event(dev, SIOCGIWAP, &wreq, NULL);
				return 0;
			}
		else if(event == WPAS_MIC_FAILURE)
			{
#ifdef WIFI_WPAS_CLI
				struct iw_michaelmicfailure ev;
				memset(&ev, 0, sizeof(ev));
//				if (hdr->addr1[0] & 0x01)
//					ev.flags |= IW_MICFAILURE_GROUP;
//				else
//					ev.flags |= IW_MICFAILURE_PAIRWISE;
				ev.src_addr.sa_family = ARPHRD_ETHER;
				memcpy(ev.src_addr.sa_data, mac, ETH_ALEN);
				wreq.data.length = sizeof(ev);
				wireless_send_event(dev, IWEVMICHAELMICFAILURE, &wreq, (char *)&ev);
#else
				char buf[6];
				memcpy(buf, mac, 6);
				wreq.data.flags = event;
				wreq.data.length = 6;
				wireless_send_event(dev, IWEVCUSTOM, &wreq, buf);
				return 0;
#endif
			}
		else if(event == WPAS_ASSOC_INFO)
			{
				wreq.data.flags = event;
				wreq.data.length = sizeof(struct _WPAS_ASSOCIATION_INFO);
				wireless_send_event(dev, IWEVCUSTOM, &wreq, extra); //IW_CUSTOM_MAX is 256, can NOT afford  _DOT11_PROBE_REQUEST_IND
				return 0;
			}	
		else if(event == WPAS_SCAN_DONE)
			{
				wireless_send_event(dev, SIOCGIWSCAN, &wreq, NULL);
				return 0;
			}
#ifdef WIFI_WPAS_CLI
		else if (event == WPAS_DISCON) 
			{
				wreq.ap_addr.sa_family = ARPHRD_ETHER;
				memset(wreq.ap_addr.sa_data, 0, ETH_ALEN);
				wireless_send_event(dev, SIOCGIWAP, &wreq, NULL);
				return 0;
			}
#endif		
		else
			{
				//Not used yet
				wreq.data.flags = event;
				wireless_send_event(dev, IWEVCUSTOM, &wreq, extra);
				return 0;
			}
	}

	return -1;
}
#endif


#ifdef CONFIG_RTL_WAPI_SUPPORT
void wapi_event_indicate(struct rtl8192cd_priv *priv)
{
	#ifdef LINUX_2_6_27_
	struct pid *pid;
	#endif

	if (priv->pshare->wlanwapi_pid > 0)
	{
#ifdef LINUX_2_6_27_
		kill_pid(_wlanwapi_pid, SIGIO, 1);
#else
		kill_proc(priv->pshare->wlanwapi_pid, SIGIO, 1);
#endif
	}
}
#endif

#ifdef RSSI_MONITOR_NCR
void rssi_event_indicate(struct rtl8192cd_priv *priv)
{
	if (priv->pshare->wlanrssim_pid > 0)
	{
#ifdef LINUX_2_6_27_
		kill_pid(priv->pshare->_wlanrssim_pid, SIGIO, 1);
#else
		kill_proc(priv->pshare->wlanrssim_pid, SIGIO, 1);
#endif
	}
}

void rssi_monitor(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char rssi)
{
	rssim_msg message;
	message.event = 0;

	if(rssi > priv->pshare->rf_ft_var.rssi_monitor_thd1 && pstat->rssi_report==0) {
		message.event = 1;
		pstat->rssi_report= rssi;
	} else if(rssi < priv->pshare->rf_ft_var.rssi_monitor_thd1-5 && pstat->rssi_report!=0) {
//		event = 2;			// not report leave event
		pstat->rssi_report= 0;
	} else if(pstat->rssi_report){
		if(RTL_ABS(rssi, pstat->rssi_report) > priv->pshare->rf_ft_var.rssi_monitor_thd2) {
			if(pstat->rssim_type !=1) {	// type 1: not report variation
				message.event = 3;
				pstat->rssi_report= rssi;
			}
		}
	}

	if(message.event) {		
		memcpy(message.hwaddr, pstat->cmn_info.mac_addr, MACADDRLEN);
		message.rssi = rssi;
		message.timestamp = jiffies;
		DOT11_EnQueue((unsigned long)priv, priv->rssimEvent_queue, &message, sizeof(message));
		rssi_event_indicate(priv);
	}
}
#endif

#ifdef USE_WEP_DEFAULT_KEY
void init_DefaultKey_Enc(struct rtl8192cd_priv *priv, unsigned char *key, int algorithm)
{
	unsigned char defaultmac[4][6];
	int i;

	memset(defaultmac, 0, sizeof(defaultmac));
	for(i=0; i<4; i++)
		defaultmac[i][5] = i;

	for(i=0; i<4; i++)
	{
		CamDeleteOneEntry(priv, defaultmac[i], i, 1);
		if (key == NULL)
			CamAddOneEntry(priv, defaultmac[i], i,
					(priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm)<<2,
					1, priv->pmib->dot11DefaultKeysTable.keytype[i].skey);
		else
			CamAddOneEntry(priv, defaultmac[i], i,
					algorithm<<2,
					1, key);
	}
	priv->pshare->CamEntryOccupied += 4;
}
#endif


#ifdef UNIVERSAL_REPEATER
//
// Disable AP function in virtual interface
//
void disable_vxd_ap(struct rtl8192cd_priv *priv)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	if ((priv==NULL) || !(priv->pmib->dot11OperationEntry.opmode & WIFI_AP_STATE))
		return;

	if (!(priv->drv_state & DRV_STATE_VXD_AP_STARTED))
		return;
	else
		priv->drv_state &= ~DRV_STATE_VXD_AP_STARTED;

	DEBUG_INFO("Disable vxd AP\n");

	if (IS_DRV_OPEN(priv))
		rtl8192cd_close(priv->dev);

	SAVE_INT_AND_CLI(flags);

#ifdef CONFIG_WLAN_HAL
	if (IS_HAL_CHIP(priv)) {
    	GET_HAL_INTERFACE(priv)->DisableVXDAPHandler(priv);
	} else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif
	{//not HAL
#if defined(CONFIG_PCI_HCI) || (defined(CONFIG_USB_HCI) && defined(CONFIG_SUPPORT_USB_INT) && defined(CONFIG_INTERRUPT_BASED_TXBCN))
		int run = 0;
		#if defined(CONFIG_PCI_HCI)
		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
			run = 1;
		}		
		#endif
		#if (defined(CONFIG_USB_HCI) && defined(CONFIG_SUPPORT_USB_INT) && defined(CONFIG_INTERRUPT_BASED_TXBCN))
		if (GET_HCI_TYPE(priv) == RTL_HCI_USB) {
			run = 1;
		}
		#endif

		if (run) {
#ifdef CONFIG_RTL_88E_SUPPORT
			if (GET_CHIP_VER(priv)==VERSION_8188E) {
				priv->pshare->InterruptMask &= ~(HIMR_88E_BcnInt | HIMR_88E_TBDOK | HIMR_88E_TBDER);
				RTL_W32(REG_88E_HIMR, priv->pshare->InterruptMask);
			} else
#endif
			{
				RTL_W32(HIMR, RTL_R32(HIMR) & ~(HIMR_BCNDOK0));
			}
		}
#endif

	//RTL_W16(ATIMWND, 2);
	RTL_W32(CR, (RTL_R32(CR) & ~(NETYPE_Mask << NETYPE_SHIFT)) | ((NETYPE_NOLINK & NETYPE_Mask) << NETYPE_SHIFT));
	}

	RESTORE_INT(flags);
}


//
// Enable AP function in virtual interface
//
void enable_vxd_ap(struct rtl8192cd_priv *priv)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	if ((priv==NULL) || !(priv->pmib->dot11OperationEntry.opmode & WIFI_AP_STATE) ||
		!(priv->drv_state & DRV_STATE_VXD_INIT))
		return;

	if (priv->drv_state & DRV_STATE_VXD_AP_STARTED)
		return;
	else
		priv->drv_state |= DRV_STATE_VXD_AP_STARTED;

	DEBUG_INFO("Enable vxd AP\n");

	priv->pmib->dot11RFEntry.dot11channel = GET_ROOT(priv)->pmib->dot11Bss.channel;
	//priv->pmib->dot11BssType.net_work_type = GET_ROOT_PRIV(priv)->oper_band;
	priv->pmib->dot11BssType.net_work_type = GET_ROOT(priv)->pmib->dot11BssType.net_work_type &
		GET_ROOT(priv)->pmib->dot11Bss.network;

	if (!IS_DRV_OPEN(priv))
		rtl8192cd_open(priv->dev);
	else {
		//priv->oper_band = priv->pmib->dot11BssType.net_work_type;
		validate_oper_rate(priv);
		get_oper_rate(priv);
	}

	memcpy(priv->pmib->dot11StationConfigEntry.dot11Bssid, GET_ROOT(priv)->pmib->dot11OperationEntry.hwaddr, MACADDRLEN);
	memcpy(GET_MY_HWADDR, priv->pmib->dot11StationConfigEntry.dot11Bssid, MACADDRLEN);
	memcpy(priv->pmib->dot11Bss.bssid, priv->pmib->dot11StationConfigEntry.dot11Bssid, MACADDRLEN);

	SAVE_INT_AND_CLI(flags);
	priv->ht_cap_len = 0;
	init_beacon(priv);

#if defined(CONFIG_PCI_HCI) || (defined(CONFIG_USB_HCI) && defined(CONFIG_SUPPORT_USB_INT) && defined(CONFIG_INTERRUPT_BASED_TXBCN))
{
	int run = 0;

#if defined(CONFIG_PCI_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
		run = 1;
	}
#endif
#if (defined(CONFIG_USB_HCI) && defined(CONFIG_SUPPORT_USB_INT) && defined(CONFIG_INTERRUPT_BASED_TXBCN))
	if (GET_HCI_TYPE(priv) == RTL_HCI_USB) {
		run =1;
	}
#endif
	
	if (run) {
#ifdef CONFIG_RTL_88E_SUPPORT
		if (GET_CHIP_VER(priv)==VERSION_8188E) {
			priv->pshare->InterruptMask |= HIMR_88E_BcnInt | HIMR_88E_TBDOK | HIMR_88E_TBDER;
			RTL_W32(REG_88E_HIMR, priv->pshare->InterruptMask);
		} else
#endif
		{
			RTL_W32(HIMR, RTL_R32(HIMR) | HIMR_BCNDOK0);
		}
	}
}
#endif

	//RTL_W16(ATIMWND, 0x0030);
	RTL_W32(CR, (RTL_R32(CR) & ~(NETYPE_Mask << NETYPE_SHIFT)) | ((NETYPE_AP & NETYPE_Mask) << NETYPE_SHIFT));
#ifdef CONFIG_RTL_92C_SUPPORT
	if (!IS_TEST_CHIP(priv))
#endif
	{
		RTL_W8(0x422, RTL_R8(0x422) | BIT(6));
		RTL_W8(BCN_CTRL, 0); 
		RTL_W8(0x553, 1); 
		RTL_W8(BCN_CTRL, DIS_TSF_UPDATE_N| EN_BCN_FUNCTION | DIS_SUB_STATE_N | EN_TXBCN_RPT);
	}

	RESTORE_INT(flags);
}
#endif // UNIVERSAL_REPEATER

#ifdef RTK_STA_BWC
void rtl8192cd_sta_bwc_timer(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	
	struct stat_info *pstat;
	int i;	

	// clear bandwidth control counter for every client	
	for(i=0; i<NUM_STAT; i++) {
		if (priv->pshare->aidarray[i] && (priv->pshare->aidarray[i]->used == TRUE)){
			pstat = &(priv->pshare->aidarray[i]->station);
			pstat->sta_bwctx_cnt = 0;
		}
	}

	mod_timer(&priv->sta_bwc_timer, jiffies + RTL_MILISECONDS_TO_JIFFIES(priv->pshare->rf_ft_var.sta_bwc_to));
}
#endif

#ifdef GBWC
void rtl8192cd_GBWC_timer(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	struct sk_buff *pskb;

	if (!(priv->drv_state & DRV_STATE_OPEN))
		return;

	if (priv->pmib->gbwcEntry.GBWCMode == GBWC_MODE_DISABLE)
		return;

	priv->GBWC_consuming_Q = 1;

	// clear bandwidth control counter
	priv->GBWC_tx_count = 0;
	priv->GBWC_rx_count = 0;

	// consume Tx queue
	while(1)
	{
		pskb = (struct sk_buff *)deque(priv, 
#if defined(AP_SWPS_OFFLOAD)
            NULL,
            NULL,
#endif                        
            &(priv->GBWC_tx_queue.head), &(priv->GBWC_tx_queue.tail),
			(unsigned long)(priv->GBWC_tx_queue.pSkb), NUM_TXPKT_QUEUE);

		if (pskb == NULL)
			break;

#ifdef ENABLE_RTL_SKB_STATS
		rtl_atomic_dec(&priv->rtl_tx_skb_cnt);
#endif

		if (rtl8192cd_start_xmit_noM2U(pskb, pskb->dev))
			rtl_kfree_skb(priv, pskb, _SKB_TX_);
	}

	// consume Rx queue
	while(1)
	{
		pskb = (struct sk_buff *)deque(priv, 
#if defined(AP_SWPS_OFFLOAD)
            NULL,
            NULL,
#endif            
            &(priv->GBWC_rx_queue.head), &(priv->GBWC_rx_queue.tail),
			(unsigned long)(priv->GBWC_rx_queue.pSkb), NUM_TXPKT_QUEUE);

		if (pskb == NULL)
			break;

		rtl_netif_rx(priv, pskb, (struct stat_info *)*(unsigned int *)&(pskb->cb[4]));
	}

	priv->GBWC_consuming_Q = 0;

	mod_timer(&priv->GBWC_timer, jiffies + GBWC_TO);
}
#endif

#ifdef SBWC
void rtl8192cd_SBWC_timer(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	struct sk_buff *pskb;
	SBWC_MODE mode;
	struct stat_info	*pstat;
	struct list_head	*phead, *plist;
	unsigned long	flags=0;
#ifdef SMP_SYNC
	unsigned long	flags2=0;
#endif

	if (!IS_DRV_OPEN(priv)) {
		return;
	}
	
	SAVE_INT_AND_CLI(flags);
	SMP_LOCK_ASOC_LIST(flags);

	phead = &priv->asoc_list;
	plist = phead;

	while ((plist = asoc_list_get_next(priv, plist)) != phead)
	{
		pstat = list_entry(plist, struct stat_info, asoc_list);

		mode = SBWC_MODE_DISABLE;

		if (pstat->SBWC_tx_limit)
			mode |= SBWC_MODE_LIMIT_STA_TX;

		if (pstat->SBWC_rx_limit)
			mode |= SBWC_MODE_LIMIT_STA_RX;
		
		if (mode == SBWC_MODE_DISABLE)
			continue;

		SMP_LOCK_SBWC(flags2);
		pstat->SBWC_consuming_q = 1;

		// clear bandwidth control counter
		pstat->SBWC_tx_count = 0;
		pstat->SBWC_rx_count = 0;

		// consume Tx queue
		while(1)
		{
			pskb = (struct sk_buff *)deque(priv, &(pstat->SBWC_txq.head), &(pstat->SBWC_txq.tail),
				(unsigned long)(pstat->SBWC_txq.pSkb), NUM_TXPKT_QUEUE);

			if (!pskb)
				break;

			#ifdef ENABLE_RTL_SKB_STATS
			rtl_atomic_dec(&priv->rtl_tx_skb_cnt);
			#endif

			if (rtl8192cd_start_xmit_noM2U(pskb, pskb->dev))
				rtl_kfree_skb(priv, pskb, _SKB_TX_);
		}

		// consume Rx queue
		while(1)
		{
			pskb = (struct sk_buff *)deque(priv, &(pstat->SBWC_rxq.head), &(pstat->SBWC_rxq.tail),
				(unsigned long)(pstat->SBWC_rxq.pSkb), NUM_TXPKT_QUEUE);

			if (!pskb)
				break;

			rtl_netif_rx(priv, pskb, (struct stat_info *)*(unsigned int *)&(pskb->cb[4]));
		}

		pstat->SBWC_consuming_q = 0;
		pstat->SBWC_mode = mode;
		SMP_UNLOCK_SBWC(flags2);
	}

	SMP_UNLOCK_ASOC_LIST(flags);
	RESTORE_INT(flags);

	if (IS_DRV_OPEN(priv))
	mod_timer(&priv->SBWC_timer, jiffies + SBWC_TO);
}
#endif

unsigned char fw_was_full(struct rtl8192cd_priv *priv)
{
	struct list_head *phead;
	struct list_head *plist;
	struct stat_info *pstat;
#if defined(SMP_SYNC) /*&& (defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI))*/
	unsigned long flags = 0;
#endif
	unsigned char is_full;

	phead = &priv->asoc_list;
	if(list_empty(phead))
		return 0;

	is_full = 0;

	SMP_LOCK_ASOC_LIST(flags);

	plist = phead->next;
	while (plist != phead) {
		pstat = list_entry(plist, struct stat_info, asoc_list);
		plist = plist->next;
        if (pstat->cmn_info.ra_info.disable_ra) 
		{
			is_full = 1;
			goto exit;
		}
	}

exit:
	SMP_UNLOCK_ASOC_LIST(flags);
	return is_full;
}


int realloc_RATid(struct rtl8192cd_priv *priv)
{
	struct list_head *phead;
	struct list_head *plist;
	struct stat_info *pstat =NULL, *pstat_chosen = NULL;
	unsigned int max_through_put = 0;
	unsigned int have_chosen = 0;
#if defined(SMP_SYNC)/* && (defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI))*/
	unsigned long flags = 0;
#endif

	phead = &priv->asoc_list;
	if(list_empty(phead))
		return 0;

	SMP_LOCK_ASOC_LIST(flags);
	
	plist = phead->next;
	while (plist != phead) {
		int temp_through_put ;
		pstat = list_entry(plist, struct stat_info, asoc_list);
		plist = plist->next;

		if (!pstat->cmn_info.ra_info.disable_ra)// STA has rate adaptive
			continue;

		if (pstat->expire_to == 0) // exclude expired STA
			continue;

		temp_through_put =  pstat->tx_avarage + pstat->rx_avarage;

		if (temp_through_put >= max_through_put){
			pstat_chosen = pstat;
			max_through_put = temp_through_put;
			have_chosen = 1;
		}
	}

	SMP_UNLOCK_ASOC_LIST(flags);
	
	if (have_chosen == 0)
		return 0;


#ifdef  CONFIG_WLAN_HAL
	if (IS_HAL_CHIP(priv)){
		init_rainfo(priv, pstat);
	} else
#endif
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
	if(GET_CHIP_VER(priv)== VERSION_8812E || GET_CHIP_VER(priv)== VERSION_8723B){
		UpdateHalRAMask8812(priv, pstat_chosen, 3);
	} else
#endif
#ifdef CONFIG_RTL_88E_SUPPORT
	if (GET_CHIP_VER(priv)==VERSION_8188E) {
#ifdef TXREPORT
		add_RATid(priv, pstat);
#endif
	} else
#endif
	{
#if defined(CONFIG_RTL_92D_SUPPORT) || defined(CONFIG_RTL_92C_SUPPORT)
		if(CHIP_VER_92X_SERIES(priv))
			add_update_RATid(priv, pstat_chosen);
#endif
	}

	return 1;
}


void update_remapAid(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	int array_idx;

    if (!pstat->cmn_info.ra_info.disable_ra)
        return; /*already exists, just return*/

    if (priv->pshare->fw_free_space) {
        /*find an empty slot*/
    	for (array_idx = 0; array_idx < NUM_STAT; array_idx++) {
    		if (priv->pshare->remapped_aidarray[array_idx] == 0)
    			break;
    	}

    	if (array_idx == NUM_STAT) {
            /*WARNING:  THIS SHOULD NOT HAPPEN*/
            printk("add AID fail!!\n");
            BUG();
    		return;
        }
        
    
        pstat->cmn_info.mac_id = array_idx + 1;
        priv->pshare->remapped_aidarray[array_idx] = pstat->cmn_info.aid; 
        pstat->cmn_info.ra_info.disable_ra = 0; // this value will updated in expire_timer
        priv->pshare->fw_free_space --;
    }
    else { /* free space run out, share the same aid*/
        pstat->cmn_info.mac_id = priv->pshare->fw_support_sta_num;
        pstat->cmn_info.ra_info.disable_ra = 1;
    }
}


void release_remapAid(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
    int i;
    if (!pstat->cmn_info.ra_info.disable_ra)
    {
        for(i = 0; i < NUM_STAT; i++) {
            if(priv->pshare->remapped_aidarray[i] == pstat->cmn_info.aid){
                priv->pshare->remapped_aidarray[i] = 0;                
                break;
            }
        }        
        priv->pshare->fw_free_space ++;
        pstat->cmn_info.mac_id = 0;
        pstat->cmn_info.ra_info.disable_ra = 1;
        DEBUG_INFO("Remove id %d from ratr\n", pstat->cmn_info.aid);        
    }
}


unsigned int is_h2c_buf_occupy(struct rtl8192cd_priv *priv)
{
	 unsigned int occupied = 0;

	if (
#ifdef CONFIG_RTL_92C_SUPPORT
		(IS_TEST_CHIP(priv) && RTL_R8(0x1c0+priv->pshare->fw_q_fifo_count)) ||
#endif
		(RTL_R8(0x1cc) & BIT(priv->pshare->fw_q_fifo_count)))
		occupied++;

	return occupied;
}

int under_apmode_repeater(struct rtl8192cd_priv *priv)
{
	int ret = 0;

#if defined(UNIVERSAL_REPEATER)
	if(IS_ROOT_INTERFACE(priv))
	{
		if(IS_DRV_OPEN(GET_VXD_PRIV(priv))) 
		{
			if((priv->pmib->dot11OperationEntry.opmode & WIFI_AP_STATE) &&
				(GET_VXD_PRIV(priv)->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE))
					ret = 1;
		}
	}
	else if(IS_VXD_INTERFACE(priv))
	{
		if(IS_DRV_OPEN(priv)) 
		{
			if((GET_ROOT(priv)->pmib->dot11OperationEntry.opmode & WIFI_AP_STATE) &&
				(priv->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE))
					ret = 1;
		}
	}
#endif

	return ret;
}

short signin_h2c_cmd(struct rtl8192cd_priv *priv, unsigned int content, unsigned short ext_content)
{
	int c=0;

#ifdef MP_TEST
	if (priv->pshare->rf_ft_var.mp_specific)
		goto SigninFAIL;
#endif

	/*
	 *	Check if h2c cmd signin buffer is occupied, 
	 *	for Power Saving related functions only
	 */
	//if ((content & 0x7f) < H2C_CMD_RSSI) {
		while (is_h2c_buf_occupy(priv)) {
		delay_us(10);
		if(++c ==30)
		{
			printk("signin_h2c_cmd() failed! [content = %x , ext_content = %x]\n", content, ext_content);
			goto SigninFAIL;
	}
	}
	//}

	/*
		 * signin reg in order to fit hw requirement
		 */
		if(content & BIT(7))
			RTL_W16(0x88+(priv->pshare->fw_q_fifo_count*2), ext_content);

	RTL_W32(HMEBOX_0+(priv->pshare->fw_q_fifo_count*4), content);

	//printk("(smcc) sign in h2c %x\n", HMEBOX_0+(priv->pshare->fw_q_fifo_count*4));

#if defined(TESTCHIP_SUPPORT) && defined(CONFIG_RTL_92C_SUPPORT)
	/*
	 * set own bit
	 */
	if(IS_TEST_CHIP(priv))
	RTL_W8(0x1c0+priv->pshare->fw_q_fifo_count, 1);
#endif

	/*
	 * rollover ring buffer count
	 */
	if (++priv->pshare->fw_q_fifo_count > 3)
		priv->pshare->fw_q_fifo_count = 0;

	return 0;
	
SigninFAIL:
	return 1;
}


void set_ps_cmd(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif
	unsigned int content = 0;

	if(! CHIP_VER_92X_SERIES(priv))
		return;

	SAVE_INT_AND_CLI(flags);

	/*
	 * set ps state
	 */
	 if (pstat->state & WIFI_SLEEP_STATE)
	 	content |= BIT(24);

	/*
	 * set macid
	 */
	content |= REMAP_AID(pstat) << 8;

	/*
	 * set cmd id
	 */
	 content |= H2C_CMD_PS;

	signin_h2c_cmd(priv, content, 0);

	RESTORE_INT(flags);
}

#ifdef TX_SHORTCUT
__inline__ void clearTxShortCutBufSize(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#if defined(CONFIG_PCI_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
		int i;
	    for (i=0; i<MAX_TXSC_ENTRY; i++) {
#ifdef CONFIG_WLAN_HAL
	        if (IS_HAL_CHIP(priv)) {
	            GET_HAL_INTERFACE(priv)->SetShortCutTxBuffSizeHandler(priv, pstat->tx_sc_ent[i].hal_hw_desc, 0);
	        } else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif // CONFIG_WLAN_HAL
	        {//not HAL
		        pstat->tx_sc_ent[i].hwdesc1.Dword7 &= set_desc(~TX_TxBufSizeMask);
	        }
	    }
	}
#endif //CONFIG_PCI_HCI

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
		int i, j;
		for (i = 0; i < NR_NET80211_UP; ++i) {
			for (j = 0; j < TX_SC_ENTRY_NUM; ++j)
				pstat->TX_SC_ENT[i][j].hwdesc1.Dword7 &= set_desc(~TX_TxBufSizeMask);
		}
	}
#endif //CONFIG_USB_HCI || CONFIG_SDIO_HCI
}

void clearAllTxShortCut(struct rtl8192cd_priv *priv)
{
	int i;
	for(i= 0; i<NUM_STAT; i++) {
		if (priv->pshare->aidarray[i] && priv->pshare->aidarray[i]->used == TRUE) {
			clearTxShortCutBufSize(priv, &(priv->pshare->aidarray[i]->station));
		}
	}
}
#endif

#if defined(MULTI_STA_REFINE)


void TxPktBuf_AgingTimer(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	if (!(priv->drv_state & DRV_STATE_OPEN))
		return;
#ifdef CONFIG_WLAN_HAL_8192FE
	if (GET_CHIP_VER(priv) == VERSION_8192F)
		return;
#endif
	if( priv->pshare->paused_sta_num >8) {
	{
		unsigned int Q_aid[8], Q_pkt[8], minIdx=0;
		 int i, pq=0; 
		 struct stat_info *pstat2;
		 
#if defined (CONFIG_WLAN_HAL_8814AE) || defined (CONFIG_WLAN_HAL_8822BE) || defined (CONFIG_WLAN_HAL_8197F)
			if (GET_CHIP_VER(priv) == VERSION_8814A || GET_CHIP_VER(priv) == VERSION_8822B || GET_CHIP_VER(priv) == VERSION_8197F) {
				for(i=0; i<4; i++) {
                    Q_pkt[i] = (RTL_R16(0x1400+i*2) & 0xFFF);     
                    Q_aid[i] = (RTL_R8(0x400+i*4+3)>>1) & 0x7f;       //31:25     7b
                    Q_pkt[i+4] = (RTL_R16(0x1408+i*2) & 0xFFF);
                    Q_aid[i+4] = (RTL_R8(0x468+i*4+3)>>1) & 0x7f; //31:25     7b	
                }		
			}
			else
#endif	

		{
		  for(i=0; i<4; i++) {
			unsigned int tmp1 = RTL_R32(0x400+i*4);
			unsigned int tmp2 = RTL_R32(0x468+i*4); 	
			Q_pkt[i] = ((tmp1>>8) & 0x7f); // 14:8	   7b
			Q_aid[i] = ((tmp1>>25) & 0x7f); //31:25 	7b
			Q_pkt[i+4] = ((tmp2>>8) & 0x7f); // 14:8	   7b
			Q_aid[i+4] = ((tmp2>>25) & 0x7f); //31:25	   7b							
			}	
		}
		 for(i=0; i<8; i++) {
			pstat2 = get_macIDinfo(priv, Q_aid[i]);
			if( pstat2 && pstat2->txpause_flag && Q_pkt[i]) {
				pq++;
				if( Q_pkt[i] <	Q_pkt[minIdx])
					minIdx = i;
			} 
		 }
		if( pq > MAXPAUSEDQUEUE) {
			pstat2 = get_macIDinfo(priv, Q_aid[minIdx]);
#if 0
			if(pq ==8) {
				struct stat_info *pstat3;
				unsigned minIdx2= (minIdx+1)&7;
				priv->pshare->lock_counter++;
//					panic_printk("%s, PAUSE All Queue !!! \n", __FUNCTION__ );
				for(i=0; i<8; i++) {
				   if( Q_pkt[i] <  Q_pkt[minIdx2] && (i!=minIdx))
					   minIdx2 = i; 
				}
				pstat3 = get_macIDinfo(priv, Q_aid[minIdx2]);
				if(pstat3) {					
#ifdef CONFIG_WLAN_HAL	
					if (IS_HAL_CHIP(priv)) {
						GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, pstat3, DECREASE);
					} else 
#endif
					{
#ifdef CONFIG_RTL_8812_SUPPORT						
						UpdateHalMSRRPT8812(priv, pstat3, DECREASE);			
#endif
					}
					pstat3->txpdrop_flag = 1;
#ifdef CONFIG_WLAN_HAL						
					if (IS_HAL_CHIP(priv)) {
						GET_HAL_INTERFACE(priv)->SetMACIDSleepHandler(priv, 0, REMAP_AID(pstat3));	
					} else
#endif
					{
#ifdef CONFIG_RTL_8812_SUPPORT						
						RTL8812_MACID_PAUSE(priv, 0, REMAP_AID(pstat3));
#endif
					}						
					pstat3->txpause_flag = 0;
//						panic_printk("%s, Free %d pkts of MAC ID:%d\n", __FUNCTION__, Q_pkt[minIdx2], Q_aid[minIdx2]);	
					if(priv->pshare->paused_sta_num)
						priv->pshare->paused_sta_num--;
					priv->pshare->unlock_counter2++;
					pstat3->dropPktCurr += Q_pkt[minIdx2];		
				}
			}
#endif	
			if(pstat2) {
#ifdef CONFIG_WLAN_HAL					
				if (IS_HAL_CHIP(priv)) {
					GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, pstat2, DECREASE);
				} else
#endif
				{
#ifdef CONFIG_RTL_8812_SUPPORT				
					UpdateHalMSRRPT8812(priv, pstat2, DECREASE);	
#endif
				}
				pstat2->txpdrop_flag = 1;
//					panic_printk("%s, Free %d pkts of MAC ID:%d\n", __FUNCTION__, Q_pkt[minIdx], Q_aid[minIdx]);
#ifdef CONFIG_WLAN_HAL	
				if (IS_HAL_CHIP(priv)) {
					GET_HAL_INTERFACE(priv)->SetMACIDSleepHandler(priv, 0, REMAP_AID(pstat2));	
				} else 
#endif				
				{
#ifdef CONFIG_RTL_8812_SUPPORT				
					RTL8812_MACID_PAUSE(priv, 0, REMAP_AID(pstat2));
#endif
				}
				pstat2->txpause_flag = 0;
				if(priv->pshare->paused_sta_num)
					priv->pshare->paused_sta_num--;
				priv->pshare->unlock_counter2++;
				pstat2->dropPktCurr += Q_pkt[minIdx];				
			}						
		}
	}

	// process aging
	{
		int idx = 0;
		struct stat_info* pstatd = findNextSTA(priv, &idx);
		while(pstatd) {
			if(pstatd && pstatd->txpause_flag && 
			   (TSF_DIFF(jiffies, pstatd->txpause_time) > RTL_MILISECONDS_TO_JIFFIES(PKTAGINGTIME))) 
			{
#ifdef CONFIG_WLAN_HAL			
				if (IS_HAL_CHIP(priv)) {
					GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, pstatd, DECREASE);
				} else
#endif					
				{
#ifdef CONFIG_RTL_8812_SUPPORT				
					UpdateHalMSRRPT8812(priv, pstatd, DECREASE);	
#endif
				}

				pstatd->txpdrop_flag = 1;							
#ifdef CONFIG_WLAN_HAL			
				if (IS_HAL_CHIP(priv)) {				
					GET_HAL_INTERFACE(priv)->SetMACIDSleepHandler(priv, 0, REMAP_AID(pstatd));	
				} else 
#endif
				{
#ifdef CONFIG_RTL_8812_SUPPORT				
					RTL8812_MACID_PAUSE(priv, 0, REMAP_AID(pstatd));
#endif				
				}
				pstatd->txpause_flag = 0;
				if(priv->pshare->paused_sta_num)
					priv->pshare->paused_sta_num--;
				pstatd->drop_expire ++; 
			}
			pstatd = findNextSTA(priv, &idx);
		};
	}
	}
	if(priv->pshare->rf_ft_var.msta_refine&1 ) 
		mod_timer(&priv->pshare->PktAging_timer, jiffies + RTL_MILISECONDS_TO_JIFFIES(10));	
}
#endif

#ifdef CONFIG_PCI_HCI
void add_ps_timer(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	struct stat_info *pstat = NULL;
	unsigned int set_timer = 0;

#ifndef SMP_SYNC
    unsigned long flags = 0;
#endif
	if (!(priv->drv_state & DRV_STATE_OPEN))
		return;

	if (timer_pending(&priv->add_ps_timer))
		del_timer_sync(&priv->add_ps_timer);

#ifdef PCIE_POWER_SAVING
	if ((priv->pwr_state == L2) || (priv->pwr_state == L1))
			return;
#endif
#ifdef PCIE_POWER_SAVING_TEST //yllin
    if((priv->pwr_state >= L2) || (priv->pwr_state == L1)) {
            return;
    }
#endif

	if (!list_empty(&priv->addps_list)) {
		pstat = list_entry(priv->addps_list.next, struct stat_info, addps_list);
		if (!pstat)
	return ;

		if (!is_h2c_buf_occupy(priv)) {
			set_ps_cmd(priv, pstat);
			if (!list_empty(&pstat->addps_list)) {
				SAVE_INT_AND_CLI(flags);
				SMP_LOCK(flags);
				list_del_init(&pstat->addps_list);
				RESTORE_INT(flags);
				SMP_UNLOCK(flags);
			}

			if (!list_empty(&priv->addps_list))
				set_timer++;
		} else {
			set_timer++;
		}
	}

	if (set_timer)
		mod_timer(&priv->add_ps_timer, jiffies + RTL_MILISECONDS_TO_JIFFIES(50));	// 50 ms
}


#if defined(CONFIG_RTL_92D_SUPPORT) || defined(CONFIG_RTL_92C_SUPPORT)
void add_update_ps(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif
//#ifdef CONFIG_RTL_8812_SUPPORT
	if(! CHIP_VER_92X_SERIES(priv))
		return;
//#endif		
	if (is_h2c_buf_occupy(priv)) {
		if (list_empty(&pstat->addps_list)) {
			SAVE_INT_AND_CLI(flags);
			list_add_tail(&(pstat->addps_list), &(priv->addps_list));
			RESTORE_INT(flags);

			if (!timer_pending(&priv->add_ps_timer))
				mod_timer(&priv->add_ps_timer, jiffies + RTL_MILISECONDS_TO_JIFFIES(50));	// 50 ms
		}
	} else {
		set_ps_cmd(priv, pstat);
	}
}
#endif
#endif // CONFIG_PCI_HCI


void update_intel_sta_bitmap(struct rtl8192cd_priv *priv, struct stat_info *pstat, int release)
{
	if (pstat->IOTPeer != HT_IOT_PEER_INTEL)
		return;
	
	if (release) {
		clearSTABitMap(&priv->pshare->intel_sta_bitmap, pstat->cmn_info.aid);
	} else {	// join
		setSTABitMap(&priv->pshare->intel_sta_bitmap, pstat->cmn_info.aid);
	}
}

#if defined(WIFI_11N_2040_COEXIST_EXT)

void update_40m_staMap(struct rtl8192cd_priv *priv, struct stat_info *pstat, int release)
{
	if(pstat) {
		if(release) {
			clearSTABitMap(&priv->pshare->_40m_staMap, pstat->cmn_info.aid);
			clearSTABitMap(&priv->pshare->_80m_staMap, pstat->cmn_info.aid);			
		}
#if defined(RTK_AC_SUPPORT)
		else if (pstat->vht_cap_len) {
			setSTABitMap(&priv->pshare->_80m_staMap, pstat->cmn_info.aid);
			setSTABitMap(&priv->pshare->_40m_staMap, pstat->cmn_info.aid);
		}
#endif
		else if((pstat->tx_bw == CHANNEL_WIDTH_20)) {
			clearSTABitMap(&priv->pshare->_40m_staMap, pstat->cmn_info.aid);
			clearSTABitMap(&priv->pshare->_80m_staMap, pstat->cmn_info.aid);	
		} else if(pstat->tx_bw == CHANNEL_WIDTH_40){
			setSTABitMap(&priv->pshare->_40m_staMap, pstat->cmn_info.aid);
			clearSTABitMap(&priv->pshare->_80m_staMap, pstat->cmn_info.aid);	
		} 

	}
}

void checkBandwidth(struct rtl8192cd_priv *priv)
{
    unsigned int _40m_stamap = orSTABitMap(&priv->pshare->_40m_staMap);
    unsigned int _80m_stamap = orSTABitMap(&priv->pshare->_80m_staMap);
		
    int FA_counter = priv->pshare->FA_total_cnt;

    if(!priv->pshare->rf_ft_var.bws_enable)
    return;

#ifdef DFS
    if(is_DFS_channel(priv, priv->pmib->dot11RFEntry.dot11channel)
        #ifdef MCR_WIRELESS_EXTEND
        && !priv->pshare->cmw_link
        #endif
    )
    return;
#endif		

#ifdef MP_TEST
    if ( (OPMODE & WIFI_MP_STATE)|| priv->pshare->rf_ft_var.mp_specific)
        return ;
#endif

#ifdef WDS
    if (priv->pmib->dot11WdsInfo.wdsEnabled)
        return;
#endif

#ifdef CONFIG_RTK_MESH
    if(priv->pmib->dot1180211sInfo.mesh_enable)
        return;
#endif

    if (!(OPMODE & WIFI_AP_STATE))
        return;

#ifdef UNIVERSAL_REPEATER
    if (IS_DRV_OPEN(GET_VXD_PRIV(priv)))
        return;
#endif		

    if (timer_pending(&priv->ss_timer) || priv->ss_req_ongoing)
        return;

#if	defined(USE_OUT_SRC)
    if(IS_OUTSRC_CHIP(priv))
        FA_counter = ODMPTR->false_alm_cnt.cnt_all;
#endif

    if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G)
    {	    
        if (priv->pmib->dot11nConfigEntry.dot11nUse40M == CHANNEL_WIDTH_80) {            
            unsigned char do_switch = 0;        
            switch (priv->pshare->CurrentChannelBW) {
                case CHANNEL_WIDTH_80:
                    if (_80m_stamap == 0 && _40m_stamap == 0) {
                        priv->pshare->CurrentChannelBW = CHANNEL_WIDTH_20;
                        #ifdef MCR_WIRELESS_EXTEND
                        if (priv->pshare->cmw_link)
                            RTL_W8(0x82d, 0x91);
                        #endif
                        do_switch = 1;
                    }
                    else if (_80m_stamap == 0 && _40m_stamap != 0) {
                        priv->pshare->CurrentChannelBW = CHANNEL_WIDTH_40;
                        do_switch = 1;
                    }
                    break;

                case CHANNEL_WIDTH_40:
                    if (_80m_stamap == 0 && _40m_stamap == 0) {
                        priv->pshare->CurrentChannelBW = CHANNEL_WIDTH_20;
                        do_switch = 1;
                    }
                    else if (_80m_stamap != 0) {
                        priv->pshare->CurrentChannelBW = CHANNEL_WIDTH_80;
                        do_switch = 1;
                    }
                    break;

                case CHANNEL_WIDTH_20:
                    if (_80m_stamap == 0 && _40m_stamap != 0) {
                        priv->pshare->CurrentChannelBW = CHANNEL_WIDTH_40;
                        do_switch = 1;
                    }
                    else if (_80m_stamap != 0) {
                        priv->pshare->CurrentChannelBW = CHANNEL_WIDTH_80;
                        do_switch = 1;
                    }
                    break;

                default:
                    panic_printk("No such bandwidth mode (%d)\n", priv->pshare->CurrentChannelBW);
                    break;
            }

            if (do_switch)
            {
                SwBWMode(priv, priv->pshare->CurrentChannelBW, priv->pshare->offset_2nd_chan);
                SwChnl(priv, priv->pmib->dot11RFEntry.dot11channel, priv->pshare->offset_2nd_chan);
#ifdef dybw_rx
				if(priv->pshare->rf_ft_var.bws_enable&0x2 && priv->assoc_num==1) {
					priv->ht_cap_len = 0;
					construct_ht_ie(priv, priv->pshare->CurrentChannelBW, priv->pshare->offset_2nd_chan);
				}
#endif
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL) || defined(CONFIG_RTL_8723B_SUPPORT)
				update_RAMask_to_FW(priv,1);
#endif
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
				dc_if_update(priv);
#endif
            }
            return;  
        }        
    }

	//check assoc num
	unsigned int assoc_num = priv->pshare->total_assoc_num;

    if ((priv->pshare->CurrentChannelBW == CHANNEL_WIDTH_40)  && (!_40m_stamap) ) {
        priv->pshare->CurrentChannelBW = CHANNEL_WIDTH_20;		
        SwBWMode(priv, priv->pshare->CurrentChannelBW, priv->pshare->offset_2nd_chan);
        SwChnl(priv, priv->pmib->dot11RFEntry.dot11channel, priv->pshare->offset_2nd_chan);
        SetTxPowerLevel(priv, priv->pmib->dot11RFEntry.dot11channel);
    } 
    if( priv->pmib->dot11nConfigEntry.dot11nCoexist) {
		int run = 0;
#if defined(CONFIG_PCI_HCI)
		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
        	if((FA_counter> priv->pshare->rf_ft_var.bws_Thd)&&((RTL_R8(0xc50) & 0x7f) >= 0x32)) {
				run = 1;
			}
		}
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
		if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
        u1Byte IGI = RTL_R8(0xc50) & 0x7f;
        	if (((FA_counter> priv->pshare->rf_ft_var.bws_Thd) && (IGI >= 0x32)) || (IGI >= 0x42)) {
				run = 1;
			}
		}
#endif
		if (run)
		{
            if(priv->pshare->is_40m_bw != CHANNEL_WIDTH_20) {
                priv->pshare->is_40m_bw = CHANNEL_WIDTH_20;
                priv->ht_cap_len = 0;	// reconstruct ie
                if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11N)
                    construct_ht_ie(priv, priv->pshare->is_40m_bw, priv->pshare->offset_2nd_chan);				
            }
        } 
#if 1//BW40 back when no sta		
        else if(assoc_num==0 && (FA_counter < priv->pshare->rf_ft_var.bws_CleanThd)) {
            if(priv->pshare->is_40m_bw != priv->pmib->dot11nConfigEntry.dot11nUse40M) {
                priv->pshare->is_40m_bw = priv->pmib->dot11nConfigEntry.dot11nUse40M;
                priv->ht_cap_len = 0;				// reconstruct ie
                if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11N)
                    construct_ht_ie(priv, priv->pshare->is_40m_bw, priv->pshare->offset_2nd_chan);
            }
        }		
#endif		
    }		
    
    if( _40m_stamap && (priv->pshare->CurrentChannelBW == CHANNEL_WIDTH_20)  &&
        ((priv->pmib->dot11nConfigEntry.dot11nUse40M != CHANNEL_WIDTH_20))) {
        if(priv->pmib->dot11nConfigEntry.dot11nCoexist) {
            priv->pshare->is_40m_bw = priv->pmib->dot11nConfigEntry.dot11nUse40M;
            priv->ht_cap_len = 0;				// reconstruct ie
            if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11N)
                construct_ht_ie(priv, priv->pshare->is_40m_bw, priv->pshare->offset_2nd_chan);				
        }
        priv->pshare->CurrentChannelBW = priv->pmib->dot11nConfigEntry.dot11nUse40M;
        SwBWMode(priv, priv->pshare->CurrentChannelBW, priv->pshare->offset_2nd_chan);
        SwChnl(priv, priv->pmib->dot11RFEntry.dot11channel, priv->pshare->offset_2nd_chan);
	}

    if((priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) && (priv->pshare->rf_ft_var.bws_enable&0x2)) {
		if(priv->pshare->ctrl40m) {
			priv->ht_cap_len = 0;	// reconstruct ie
			construct_ht_ie(priv, priv->pshare->CurrentChannelBW, priv->pshare->offset_2nd_chan);
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL) || defined(CONFIG_RTL_8723B_SUPPORT)			
			update_RAMask_to_FW(priv,1);
#endif
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
			dc_if_update(priv);
#endif
			priv->pshare->ctrl40m=0;
		}
	}
}
#endif


#if defined(CONFIG_RTL_92D_SUPPORT) || defined(CONFIG_RTL_92C_SUPPORT)
#ifdef CONFIG_PCI_HCI
void add_RATid_timer(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	struct stat_info *pstat = NULL;
	unsigned int set_timer = 0;
	unsigned long flags = 0;

	if (!(priv->drv_state & DRV_STATE_OPEN))
		return;

	if (timer_pending(&priv->add_RATid_timer))
		del_timer_sync(&priv->add_RATid_timer);

#ifdef PCIE_POWER_SAVING
	if ((priv->pwr_state == L2) || (priv->pwr_state == L1))
			return;
#endif

	if (!list_empty(&priv->addRAtid_list)) {
		pstat = list_entry(priv->addRAtid_list.next, struct stat_info, addRAtid_list);
		if (!pstat)
			return;

		if (!is_h2c_buf_occupy(priv)) {
			add_RATid(priv, pstat);
			if (!list_empty(&pstat->addRAtid_list)) {
				SAVE_INT_AND_CLI(flags);
				SMP_LOCK(flags);
				list_del_init(&pstat->addRAtid_list);
				RESTORE_INT(flags);
				SMP_UNLOCK(flags);
			}

			if (!list_empty(&priv->addRAtid_list))
				set_timer++;
		} else {
			set_timer++;
		}
	}

	if (set_timer)
		mod_timer(&priv->add_RATid_timer, jiffies + RTL_MILISECONDS_TO_JIFFIES(50));	// 50 ms
}


void add_update_RATid(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	if (is_h2c_buf_occupy(priv)) {
		if (list_empty(&pstat->addRAtid_list)) {
			SAVE_INT_AND_CLI(flags);
			list_add_tail(&(pstat->addRAtid_list), &(priv->addRAtid_list));
			RESTORE_INT(flags);

			if (!timer_pending(&priv->add_RATid_timer))
				mod_timer(&priv->add_RATid_timer, jiffies + RTL_MILISECONDS_TO_JIFFIES(50));	// 50 ms
		}
	} else {
			add_RATid(priv, pstat);
	}
}
#endif // CONFIG_PCI_HCI
#endif

#ifdef SDIO_AP_OFFLOAD
void set_ap_ps_mode(struct rtl8192cd_priv *priv, unsigned char *data, unsigned int data_len)
{
	int mode;

	if (strlen(data) == 0)
		return;

	mode = _atoi(data, 16);

	if (mode == 0x00) {
		ap_offload_activate(priv, OFFLOAD_PROHIBIT_USER);
	} else {
		ap_offload_deactivate(priv, OFFLOAD_PROHIBIT_USER);
	}
}

void ap_offload_deactivate(struct rtl8192cd_priv *priv, int reason)
{
	_irqL irqL;
	
	_enter_critical(&priv->pshare->offload_lock, &irqL);

#ifdef CONFIG_POWER_SAVE
	if (!priv->pshare->offload_prohibited) {
		if (IS_DRV_OPEN(GET_ROOT(priv))) {
		del_timer(&priv->pshare->ps_timer);
		priv->pshare->ps_timer_expires = 0;
		rtw_lock_suspend(priv);
	}
	}
#endif
	priv->pshare->offload_prohibited |= reason;
	
	_exit_critical(&priv->pshare->offload_lock, &irqL);
	
	if (IS_DRV_OPEN(GET_ROOT(priv))) {
		if ((RTW_STS_SUSPEND == priv->pshare->pwr_state)
#ifdef CONFIG_POWER_SAVE
				|| (priv->pshare->offload_function_ctrl > RTW_PM_PREPROCESS)
#endif
				)
			ap_offload_exit(GET_ROOT(priv));
	}
}

void ap_offload_activate(struct rtl8192cd_priv *priv, int reason)
{
	_irqL irqL;
	
	_enter_critical(&priv->pshare->offload_lock, &irqL);
	
	priv->pshare->offload_prohibited &= ~reason;
	
	_exit_critical(&priv->pshare->offload_lock, &irqL);
	
	if (IS_DRV_OPEN(GET_ROOT(priv))) {
#ifdef CONFIG_POWER_SAVE
		rtw_lock_suspend_timeout(priv, 2000);
#else
		mod_timer(&priv->pshare->ps_timer, jiffies + POWER_DOWN_T0);
#endif
	}
}
#endif // SDIO_AP_OFFLOAD

void send_h2c_cmd_detect_wps_gpio(struct rtl8192cd_priv *priv, unsigned int gpio_num, unsigned int enable, unsigned int high_active)
{
#ifndef SMP_SYNC
	unsigned long flags;
#endif
	unsigned int content = 0;

	SAVE_INT_AND_CLI(flags);
	
	content = gpio_num << 16;
	
	/*
	 * enable firmware to detect wps gpio
	 */
	if (enable)
		content |= BIT(8);
	
	/*
	 * rising edge trigger
	 */
	if (high_active)
		content |= BIT(9);

	/*
	 * set cmd id
	 */
	content |= H2C_CMD_AP_WPS_CTRL;

	signin_h2c_cmd(priv, content, 0);
	printk("signin ap_wps_ctrl h2c: 0x%08X\n", content);
	
	RESTORE_INT(flags);
}


#ifdef DELAY_REFILL_RX_BUF
#ifdef CONFIG_WLAN_HAL
    extern int refill_rx_ring_88XX(struct rtl8192cd_priv * priv, struct sk_buff * skb, unsigned char * data, unsigned int q_num, PHCI_RX_DMA_QUEUE_STRUCT_88XX cur_q);
#endif
extern int refill_rx_ring(struct rtl8192cd_priv *priv, struct sk_buff *skb, unsigned char *data);
#endif

#ifdef __ECOS
#ifdef DELAY_REFILL_RX_BUF
int __MIPS16 rtk_wifi_delay_refill(struct sk_buff  *pskb)
{
	struct rtl8192cd_priv *priv;	
	struct rtl8192cd_hw *phw;
	struct sk_buff *skb=(struct sk_buff *)pskb;
	int ret=0;
	if(!(pskb && pskb->priv))
		return ret;
	
	priv=(struct rtl8192cd_priv *)pskb->priv;
 	phw=GET_HW(priv);

#ifdef CONFIG_WLAN_HAL
	unsigned int					q_num;
	PHCI_RX_DMA_MANAGER_88XX		prx_dma;
	PHCI_RX_DMA_QUEUE_STRUCT_88XX	cur_q;
#endif // CONFIG_WLAN_HAL

#ifdef CONFIG_WLAN_HAL
	if (IS_HAL_CHIP(priv)) {
        if (!(priv->drv_state & DRV_STATE_OPEN)){
            /* return 0 means can't refill (because interface be closed or not opened yet) to rx ring but relesae to skb_poll*/            
            ret=0;
        }else{        
    	    q_num   = 0;
    	    prx_dma = (PHCI_RX_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PRxDMA88XX);
    	    cur_q   = &(prx_dma->rx_queue[q_num]);
    	    ret = refill_rx_ring_88XX(priv, NULL, NULL, q_num, cur_q);
    	    GET_HAL_INTERFACE(priv)->UpdateRXBDHostIdxHandler(priv, q_num, cur_q->rxbd_ok_cnt);
    	    cur_q->rxbd_ok_cnt = 0;
        }
	} else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif
	{
		if (phw->cur_rx_refill != phw->cur_rx) {
		   	ret=refill_rx_ring(priv, NULL, NULL); 
		}
	}
	return ret;
}
#endif
#endif


#ifdef __OSK__
// moved from 8192cd_util.h jim 20100611
__IRAM_WIFI_PRI1
struct list_head *dequeue_frame(struct rtl8192cd_priv *priv, struct list_head *head)
{
#ifndef SMP_SYNC
	unsigned long flags=0;
#endif
	struct list_head *pnext;

	SAVE_INT_AND_CLI(flags);
	if (list_empty(head)) {
		RESTORE_INT(flags);
		return (void *)NULL;
	}

	pnext = head->next;
	list_del_init(pnext);

	RESTORE_INT(flags);

	return pnext;
}

__IRAM_WIFI_PRI2 struct sk_buff *
rtl_dev_alloc_skb(struct rtl8192cd_priv *priv,
				unsigned int length, int flag, int could_alloc_from_kerenl)
{
	struct sk_buff *skb = NULL;
#ifdef CONFIG_POOL_DEBUG
	RECORDRA;
#endif

#if defined(__ECOS) && defined(CONFIG_SDIO_HCI)
	skb = dev_alloc_skb(length);
#else
	skb = alloc_skb_from_queue(priv);

	if (skb == NULL && could_alloc_from_kerenl)
		skb = dev_alloc_skb(length);
#endif

#ifdef ENABLE_RTL_SKB_STATS
	if (NULL != skb) {
		if (flag & (_SKB_TX_ | _SKB_TX_IRQ_))
			rtl_atomic_inc(&priv->rtl_tx_skb_cnt);
		else
			rtl_atomic_inc(&priv->rtl_rx_skb_cnt);
	}
#endif

	return skb;
}

__IRAM_WIFI_PRI2 void rtl_kfree_skb(struct rtl8192cd_priv *priv, struct sk_buff *skb, int flag)
{
#ifdef CONFIG_POOL_DEBUG
	RECORDRA;
	SETCCBDEBUGINFO(skb2ccb(skb),0,0,__FUNCTION__,__LINE__);
#endif

#ifdef ENABLE_RTL_SKB_STATS
	if (flag & (_SKB_TX_ | _SKB_TX_IRQ_))
		rtl_atomic_dec(&priv->rtl_tx_skb_cnt);
	else
		rtl_atomic_dec(&priv->rtl_rx_skb_cnt);
#endif

	dev_kfree_skb_any(skb);
}

#ifdef CONFIG_ENABLE_NCBUFF
__IRAM_WIFI_PRI2 void rtl_kfree_skb_spec(struct rtl8192cd_priv *priv, struct sk_buff *skb, int flag)
{
#ifdef CONFIG_POOL_DEBUG
	RECORDRA;
	SETCCBDEBUGINFO(skb2ccb(skb),0,0,__FUNCTION__,__LINE__);
#endif

#ifdef ENABLE_RTL_SKB_STATS
	if (flag & (_SKB_TX_ | _SKB_TX_IRQ_))
		rtl_atomic_dec(&priv->rtl_tx_skb_cnt);
	else
		rtl_atomic_dec(&priv->rtl_rx_skb_cnt);
#endif

	//dev_kfree_skb_any(skb);
	dev_kfree_skb_to_ncbuff(skb);
}
#endif

__IRAM_WIFI_PRI3 void rtl_cache_sync_wback(struct rtl8192cd_priv *priv, unsigned int start,
				unsigned int size, int direction)
{
	if (0 == size)
		return;	// if the size of cache sync is equal to zero, don't do sync action

	dma_cache_wback_inv((unsigned long)bus_to_virt(start-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), size);
}
#endif//#ifdef __OSK_

#ifdef FAST_RECOVERY
struct backup_info {
	struct aid_obj *sta[NUM_STAT];
	struct Dot11KeyMappingsEntry gkey;
#ifdef WDS
	struct wds_info	wds;
#endif
};

void *backup_sta(struct rtl8192cd_priv *priv)
{
	int i;
	struct backup_info *pBackup;

	pBackup = (struct backup_info *)kmalloc((sizeof(struct backup_info)), GFP_ATOMIC);
	if (pBackup == NULL) {
		printk("%s: kmalloc() failed!\n", __FUNCTION__);
		return NULL;
	}
	memset(pBackup, '\0', sizeof(struct backup_info));
	for (i=0; i<NUM_STAT; i++) {
		if (priv->pshare->aidarray[i] && priv->pshare->aidarray[i]->used) {
#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
			if (priv !=  priv->pshare->aidarray[i]->priv)
				continue;
#endif
			pBackup->sta[i] = (struct aid_obj *)kmalloc((sizeof(struct aid_obj)), GFP_ATOMIC);
			if (pBackup->sta[i] == NULL) {
				printk("%s: kmalloc(sta) failed!\n", __FUNCTION__);
				for (--i; i>=0; --i) {
					if (pBackup->sta[i]) {
#ifdef CONFIG_RTL_WAPI_SUPPORT
						free_sta_wapiInfo(priv, &pBackup->sta[i]->station);
#endif
						kfree(pBackup->sta[i]);
					}
				}
				kfree(pBackup);
				return NULL;
			}
			memcpy(pBackup->sta[i], priv->pshare->aidarray[i], sizeof(struct aid_obj));
#ifdef CONFIG_RTL_WAPI_SUPPORT
			// prevent backup station.wapiInfo from being freed during recovery preiod
			priv->pshare->aidarray[i]->station.wapiInfo = NULL;
#endif
		}
	}

#ifdef WDS
	memcpy(&pBackup->wds, &priv->pmib->dot11WdsInfo, sizeof(struct wds_info));
#endif
	memcpy(&pBackup->gkey, &priv->pmib->dot11GroupKeysTable, sizeof(struct Dot11KeyMappingsEntry));

	return (void *)pBackup;
}


void restore_backup_sta(struct rtl8192cd_priv *priv, void *pInfo)
{
	unsigned int i, offset;
	struct stat_info *pstat;
	unsigned char	key_combo[32];
	struct backup_info *pBackup = (struct backup_info *)pInfo;
#ifdef CONFIG_RTK_MESH
	unsigned char is_11s_MP = FALSE;
	unsigned long flags;
#endif
	int retVal;

	for (i=0; i<NUM_STAT; i++) {
		if (pBackup->sta[i]) {

#ifdef CONFIG_RTK_MESH	/* Restore Establish MP ONLY */
			if ((1 == GET_MIB(priv)->dot1180211sInfo.mesh_enable) && !isSTA2(pBackup->sta[i]->station)) {
				UINT8 State = pBackup->sta[i]->station.mesh_neighbor_TBL.State;

				if ((State == MP_SUPERORDINATE_LINK_UP) || (State == MP_SUBORDINATE_LINK_UP) ||
					(State == MP_SUPERORDINATE_LINK_DOWN) || (State == MP_SUBORDINATE_LINK_DOWN_E))
					is_11s_MP = TRUE;
				else { /* is MP, but not establish, Give up */
					kfree(pBackup->sta[i]);
					continue;
				}
			}
#endif
			pstat = alloc_stainfo(priv, pBackup->sta[i]->station.cmn_info.mac_addr, i);
			if (!pstat) {
				printk("%s: alloc_stainfo() failed!\n", __FUNCTION__);
				for (; i<NUM_STAT; i++) {
					if (pBackup->sta[i]) {
#ifdef CONFIG_RTL_WAPI_SUPPORT
						free_sta_wapiInfo(priv, &pBackup->sta[i]->station);
#endif
						kfree(pBackup->sta[i]);
					}
				}
				kfree(pBackup);
				return;
			}

#ifdef CONFIG_RTL_WAPI_SUPPORT
			// free new allocated wapiInfo before restore backup wapiInfo
			if (pstat->wapiInfo) free_sta_wapiInfo(priv, pstat);
#endif
			offset = (unsigned long)(&((struct stat_info *)0)->backup_start);
			memcpy(((unsigned char *)pstat)+offset,
				((unsigned char *)&pBackup->sta[i]->station)+offset, sizeof(struct stat_info)-offset);
			asoc_list_add(priv, pstat);

			if (!pstat->cmn_info.ra_info.disable_ra) {
				priv->pshare->remapped_aidarray[pstat->cmn_info.mac_id-1] = pstat->cmn_info.aid;
				priv->pshare->fw_free_space--;
			}

#ifdef CONFIG_RTK_MESH
			if (TRUE == is_11s_MP) {
				is_11s_MP = FALSE;
				SMP_LOCK_MESH_MP_HDR(flags);
				list_add_tail(&pstat->mesh_mp_ptr, &priv->mesh_mp_hdr);
				SMP_UNLOCK_MESH_MP_HDR(flags);
				mesh_cnt_ASSOC_PeerLink_CAP(priv, pstat, INCREASE);
			}
#endif

#ifdef WDS
			if (!(pstat->state & WIFI_WDS))
#endif
			if (pstat->expire_to > 0) 
				cnt_assoc_num(priv, pstat, INCREASE, (char *)__FUNCTION__);

			if ((pstat->expire_to > 0) 
#ifdef WDS
				|| (pstat->state & WIFI_WDS)
#endif
			) {
			check_sta_characteristic(priv, pstat, INCREASE);
			if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11N)
				construct_ht_ie(priv, priv->pshare->is_40m_bw, priv->pshare->offset_2nd_chan);

#ifndef USE_WEP_DEFAULT_KEY
			set_keymapping_wep(priv, pstat);
#endif
			if (!SWCRYPTO && pstat->dot11KeyMapping.keyInCam == TRUE) {
#ifdef CONFIG_RTL_HW_WAPI_SUPPORT
				if (pstat->wapiInfo && (pstat->wapiInfo->wapiType != wapiDisable)) {
					wapiStaInfo *wapiInfo = pstat->wapiInfo;
					
					retVal = CamAddOneEntry(priv, 
							pstat->cmn_info.mac_addr, 
							wapiInfo->wapiUCastKeyId,	/* keyid */ 
							DOT11_ENC_WAPI<<2,	/* type */
							0,	/* use default key */
							wapiInfo->wapiUCastKey[wapiInfo->wapiUCastKeyId].dataKey);
					if (retVal) {
						priv->pshare->CamEntryOccupied++;
						
						retVal = CamAddOneEntry(priv, 
								pstat->cmn_info.mac_addr, 
								wapiInfo->wapiUCastKeyId,	/* keyid */
								DOT11_ENC_WAPI<<2,	/* type */
								1,	/* use default key */
								wapiInfo->wapiUCastKey[wapiInfo->wapiUCastKeyId].micKey);
						if (retVal) {
							//pstat->dot11KeyMapping.keyInCam = TRUE;
							priv->pshare->CamEntryOccupied++;
						} else {
							retVal = CamDeleteOneEntry(priv, pstat->cmn_info.mac_addr, wapiInfo->wapiUCastKeyId, 0);
							if (retVal) {
								priv->pshare->CamEntryOccupied--;
								pstat->dot11KeyMapping.keyInCam = FALSE;
							}
						}
					} else {
						pstat->dot11KeyMapping.keyInCam = FALSE;
					}
				} else
#endif // CONFIG_RTL_HW_WAPI_SUPPORT
				if (pstat->dot11KeyMapping.dot11Privacy) {
					unsigned int n = sizeof(key_combo);
					if (n >= pstat->dot11KeyMapping.dot11EncryptKey.dot11TTKeyLen)
					{
							memcpy(key_combo, 
								pstat->dot11KeyMapping.dot11EncryptKey.dot11TTKey.skey, 
								pstat->dot11KeyMapping.dot11EncryptKey.dot11TTKeyLen);
					}
					n -= pstat->dot11KeyMapping.dot11EncryptKey.dot11TTKeyLen;
					if (n >= pstat->dot11KeyMapping.dot11EncryptKey.dot11TMicKeyLen)
					{
						memcpy(&key_combo[pstat->dot11KeyMapping.dot11EncryptKey.dot11TTKeyLen], 
							pstat->dot11KeyMapping.dot11EncryptKey.dot11TMicKey1.skey, 
							pstat->dot11KeyMapping.dot11EncryptKey.dot11TMicKeyLen);
					}
#ifdef USE_WEP_4_KEYS
#ifdef MULTI_MAC_CLONE
				if ((OPMODE & WIFI_STATION_STATE) && priv->pmib->ethBrExtInfo.macclone_enable) 
					retVal = CamAddOneEntry(priv, pstat->sa_addr, pstat->dot11KeyMapping.keyid,
						pstat->dot11KeyMapping.dot11Privacy<<2, 0, key_combo);
				else
#endif
				if((priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _WEP_40_PRIVACY_) ||
					(priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _WEP_104_PRIVACY_)) 
				{
					if(priv->pshare->total_cam_entry - priv->pshare->CamEntryOccupied >=4) {
						int keyid=0;
						for(;keyid<4; keyid++) {
							if (CamDeleteOneEntry(priv, pstat->cmn_info.mac_addr, keyid, 0)) {
								priv->pshare->CamEntryOccupied--;
							}					
							if (CamAddOneEntry(priv, pstat->cmn_info.mac_addr, keyid,
								pstat->dot11KeyMapping.dot11Privacy<<2, 0, 
								priv->pmib->dot11DefaultKeysTable.keytype[keyid].skey)) { 					
								priv->pshare->CamEntryOccupied++;
								retVal ++;
							}						
						}				
					} 
					if( retVal ==4) {
						pstat->dot11KeyMapping.keyInCam = TRUE;
					} else {
						int keyid=0;
						for(;keyid<4; keyid++) {
							if (CamDeleteOneEntry(priv, pstat->cmn_info.mac_addr, keyid, 0)) 
								priv->pshare->CamEntryOccupied--;					
						}
						pstat->dot11KeyMapping.keyInCam = FALSE;
					}		
				}else {
					retVal = CamAddOneEntry(priv, pstat->cmn_info.mac_addr, pstat->dot11KeyMapping.keyid,
							pstat->dot11KeyMapping.dot11Privacy<<2, 0, key_combo);										
					if (retVal)
						priv->pshare->CamEntryOccupied++;
					else
						pstat->dot11KeyMapping.keyInCam = FALSE;
				}

#else
#ifdef MULTI_MAC_CLONE
				if ((OPMODE & WIFI_STATION_STATE) && priv->pmib->ethBrExtInfo.macclone_enable) 
					retVal = CamAddOneEntry(priv, pstat->sa_addr, pstat->dot11KeyMapping.keyid,
						pstat->dot11KeyMapping.dot11Privacy<<2, 0, key_combo);
				else
#endif
					retVal = CamAddOneEntry(priv, pstat->cmn_info.mac_addr, pstat->dot11KeyMapping.keyid,
						pstat->dot11KeyMapping.dot11Privacy<<2, 0, key_combo);
					
					if (retVal)
						priv->pshare->CamEntryOccupied++;
					else
						pstat->dot11KeyMapping.keyInCam = FALSE;
#endif					
				}
			}
			}
			// to avoid add RAtid fail
#ifdef CONFIG_WLAN_HAL
			if (IS_HAL_CHIP(priv)) {
				init_rainfo(priv, pstat);
			} else
#endif
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
			if(GET_CHIP_VER(priv)== VERSION_8812E || GET_CHIP_VER(priv)== VERSION_8723B) {
				UpdateHalRAMask8812(priv, pstat, 3);
			} else
#endif
#ifdef CONFIG_RTL_88E_SUPPORT
			if (GET_CHIP_VER(priv)==VERSION_8188E) {
#ifdef TXREPORT
				add_RATid(priv, pstat);
#endif
			} else
#endif
			{
#if defined(CONFIG_RTL_92D_SUPPORT) || defined(CONFIG_RTL_92C_SUPPORT)
				add_update_RATid(priv, pstat);
#endif
			}
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
            dc_sta_add(priv, pstat);
#endif
			kfree(pBackup->sta[i]);

			if (priv->pshare->is_40m_bw && (pstat->IOTPeer == HT_IOT_PEER_MARVELL))

			{
			    setSTABitMap(&priv->pshare->marvellMapBit, pstat->cmn_info.aid);

#if defined(CONFIG_RTL_8812_SUPPORT)||defined(CONFIG_WLAN_HAL) || defined(CONFIG_RTL_8723B_SUPPORT)
				if((GET_CHIP_VER(priv)== VERSION_8812E)||(IS_HAL_CHIP(priv)) || (GET_CHIP_VER(priv)== VERSION_8723B)){
				}
                else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif
				{//not HAL
				if (priv->pshare->Reg_RRSR_2 == 0 && priv->pshare->Reg_81b == 0){
					priv->pshare->Reg_RRSR_2 = RTL_R8(RRSR+2);
					priv->pshare->Reg_81b = RTL_R8(0x81b);
					RTL_W8(RRSR+2, priv->pshare->Reg_RRSR_2 | 0x60);
					RTL_W8(0x81b, priv->pshare->Reg_81b | 0x0E); 
				} 
			}
			}
			update_intel_sta_bitmap(priv, pstat, 0);
#if defined(WIFI_11N_2040_COEXIST_EXT)
			update_40m_staMap(priv, pstat, 0);
#endif
		}
	}

#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL) || defined(CONFIG_RTL_8723B_SUPPORT)
	update_RAMask_to_FW(priv, 1);
#endif
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
	dc_if_update(priv);
#endif
	SetTxPowerLevel(priv, priv->pmib->dot11RFEntry.dot11channel);
	memcpy(&priv->pmib->dot11GroupKeysTable, &pBackup->gkey, sizeof(struct Dot11KeyMappingsEntry));
		if (!SWCRYPTO && priv->pmib->dot11GroupKeysTable.keyInCam) {
#ifdef CONFIG_RTL_HW_WAPI_SUPPORT
		if (priv->pmib->wapiInfo.wapiType != wapiDisable)
		{
			const uint8	CAM_CONST_BCAST[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
			retVal = CamAddOneEntry(priv, 
								CAM_CONST_BCAST, 
								priv->wapiMCastKeyId<<1,		/* keyid */ 
								DOT11_ENC_WAPI<<2, 	/* type */
								0,						/* use default key */
								priv->wapiMCastKey[priv->wapiMCastKeyId].dataKey);
			if (retVal) {
				retVal = CamAddOneEntry(priv, 
								CAM_CONST_BCAST, 
								(priv->wapiMCastKeyId<<1)+1,		/* keyid */ 
								DOT11_ENC_WAPI<<2, 	/* type */
								1,						/* use default key */
								priv->wapiMCastKey[priv->wapiMCastKeyId].micKey);
				if (retVal) {
					priv->pshare->CamEntryOccupied++;
//					priv->pmib->dot11GroupKeysTable.keyInCam = TRUE;
				} else {
					retVal = CamDeleteOneEntry(priv, CAM_CONST_BCAST, 1, 0);
					if (retVal)
						priv->pmib->dot11GroupKeysTable.keyInCam = FALSE;
			}
			} else {
				priv->pmib->dot11GroupKeysTable.keyInCam = FALSE;
		}
		} else
#endif // CONFIG_RTL_HW_WAPI_SUPPORT
		{
			unsigned int n = sizeof(key_combo);
			if (n >= priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TTKeyLen)
			{
				memcpy(key_combo,
					priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TTKey.skey,
					priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TTKeyLen);
			}
			n -= priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TTKeyLen;
			if (n >= priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TMicKeyLen)
			{
				memcpy(&key_combo[priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TTKeyLen],
					priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TMicKey1.skey,
					priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TMicKeyLen);
			}
			retVal = CamAddOneEntry(priv, (unsigned char *)"\xff\xff\xff\xff\xff\xff", priv->pmib->dot11GroupKeysTable.keyid,
				priv->pmib->dot11GroupKeysTable.dot11Privacy<<2, 0, key_combo);
			
			if (retVal)
				priv->pshare->CamEntryOccupied++;
			else
				priv->pmib->dot11GroupKeysTable.keyInCam = FALSE;
		}
	}

#ifdef WDS
	memcpy(&priv->pmib->dot11WdsInfo, &pBackup->wds, sizeof(struct wds_info));
#endif
	kfree(pInfo);
}
#endif // FAST_RECOVERY

#ifdef CONFIG_RTL8190_PRIV_SKB
	#if defined(CONFIG_RTL8196B_GW_8M) || defined(CONFIG_RTL8196C_AP_ROOT) || defined(CONFIG_RTL8196C_CLIENT_ONLY) || defined(CONFIG_RTL_8198_AP_ROOT) || defined(__ECOS)
#ifdef __LINUX_2_6__
		#define SKB_BUF_SIZE	(MIN_RX_BUF_LEN+sizeof(struct skb_shared_info)+128+128)
		#define SKB_BUF_SIZE_1	(MIN_RX_BUF_LEN_1+sizeof(struct skb_shared_info)+128+128)
#else
		#define SKB_BUF_SIZE	(MIN_RX_BUF_LEN+sizeof(struct skb_shared_info)+128)
		#define SKB_BUF_SIZE_1	(MIN_RX_BUF_LEN_1+sizeof(struct skb_shared_info)+128)
#endif
	#else
#ifdef __LINUX_2_6__
		#define SKB_BUF_SIZE	(MIN_RX_BUF_LEN+sizeof(struct skb_shared_info)+128+128)
		#define SKB_BUF_SIZE_1	(MIN_RX_BUF_LEN_1+sizeof(struct skb_shared_info)+128+128)
#else
		#define SKB_BUF_SIZE	(MIN_RX_BUF_LEN+sizeof(struct skb_shared_info)+128)
		#define SKB_BUF_SIZE_1	(MIN_RX_BUF_LEN_1+sizeof(struct skb_shared_info)+128)
#endif
	#endif

#define MAGIC_CODE		"8190"

struct priv_skb_buf {
	unsigned char magic[4];
	unsigned int buf_pointer;
#ifdef CONCURRENT_MODE
	struct rtl8192cd_priv *root_priv;
#endif
	struct list_head	list;
	unsigned char buf[SKB_BUF_SIZE];
};

#ifdef CONCURRENT_MODE
struct priv_skb_buf_1 {
	unsigned char magic[4];
	unsigned int buf_pointer;
	struct rtl8192cd_priv *root_priv;
	struct list_head	list;
	unsigned char buf[SKB_BUF_SIZE_1];
};
#endif


#ifdef DUALBAND_ONLY
	#define REAL_MAX_SKB	(MAX_SKB_NUM/2)
	#define REAL_MAX_SKB_1	(MAX_SKB_NUM_1/2)
#else
	#define REAL_MAX_SKB	(MAX_SKB_NUM)
	#define REAL_MAX_SKB_1	(MAX_SKB_NUM_1)
#endif

#ifdef CONCURRENT_MODE
static struct priv_skb_buf skb_buf[REAL_MAX_SKB];
static struct priv_skb_buf_1 skb_buf_1[REAL_MAX_SKB_1];
static struct list_head skbbuf_list[NUM_WLAN_IFACE];
#ifdef CONFIG_WIRELESS_LAN_MODULE
#ifdef CONFIG_BAND_2G_ON_WLAN0
//static int skb_free_num[NUM_WLAN_IFACE] = {REAL_MAX_SKB, REAL_MAX_SKB};
int skb_free_num[NUM_WLAN_IFACE] = {REAL_MAX_SKB, REAL_MAX_SKB};
#else
//static int skb_free_num[NUM_WLAN_IFACE] = {REAL_MAX_SKB, REAL_MAX_SKB};
int skb_free_num[NUM_WLAN_IFACE] = {REAL_MAX_SKB, REAL_MAX_SKB};
#endif
#else
#ifdef CONFIG_BAND_2G_ON_WLAN0
int skb_free_num[NUM_WLAN_IFACE] = {REAL_MAX_SKB_1, REAL_MAX_SKB};
#else
int skb_free_num[NUM_WLAN_IFACE] = {REAL_MAX_SKB, REAL_MAX_SKB_1};
#endif
#endif

#else
static struct priv_skb_buf skb_buf[REAL_MAX_SKB];
static struct list_head skbbuf_list;
static struct rtl8192cd_priv *root_priv;
#ifdef CONFIG_WIRELESS_LAN_MODULE
static int skb_free_num = REAL_MAX_SKB;
#else
int skb_free_num = REAL_MAX_SKB;
#endif
#endif


extern struct sk_buff *dev_alloc_8190_skb(unsigned char *data, int size);


void init_priv_skb_buf(struct rtl8192cd_priv *priv)
{
    int i, skb_num, skb_size;
#ifdef CONCURRENT_MODE
    int idx;
	struct priv_skb_buf *pskb_buf;
	struct priv_skb_buf_1 *pskb_buf_1;
#endif

#ifdef CONCURRENT_MODE
    idx = priv->pshare->wlandev_idx;

	if(idx == (0^WLANIDX)){ 
		skb_num = REAL_MAX_SKB;	/* 5G */
		pskb_buf = skb_buf;
		skb_size = SKB_BUF_SIZE;
		/* panic_printk("wlan0 skb_num=%d, skb_size=%d, \n",skb_num, skb_size); */
		memset(pskb_buf, '\0', sizeof(struct priv_skb_buf)*skb_num);

	    INIT_LIST_HEAD(&skbbuf_list[idx]);

	    for (i=0; i < skb_num ; i++)  {
	        memcpy(pskb_buf[i].magic, MAGIC_CODE, 4);
	        pskb_buf[i].root_priv = priv;
	        pskb_buf[i].buf_pointer = (unsigned int)&pskb_buf[i];
	        INIT_LIST_HEAD(&pskb_buf[i].list);
	        list_add_tail(&pskb_buf[i].list, &skbbuf_list[idx]);
	    }
	}else{
		skb_num = REAL_MAX_SKB_1;  /* 2G */
		pskb_buf_1 = skb_buf_1;
		skb_size = SKB_BUF_SIZE_1;
		/* panic_printk("wlan1 skb_num=%d, skb_size=%d, \n",skb_num, skb_size); */
	 	memset(pskb_buf_1, '\0', sizeof(struct priv_skb_buf_1)*skb_num);

	    INIT_LIST_HEAD(&skbbuf_list[idx]);

	    for (i=0; i < skb_num ; i++)  {
	        memcpy(pskb_buf_1[i].magic, MAGIC_CODE, 4);
	        pskb_buf_1[i].root_priv = priv;
	        pskb_buf_1[i].buf_pointer = (unsigned int)&pskb_buf_1[i];
	        INIT_LIST_HEAD(&pskb_buf_1[i].list);
	        list_add_tail(&pskb_buf_1[i].list, &skbbuf_list[idx]);
	    }
	}

   
#else

	skb_num = REAL_MAX_SKB;
	skb_size = SKB_BUF_SIZE;

    memset(skb_buf, '\0', sizeof(struct priv_skb_buf)*skb_num);

    INIT_LIST_HEAD(&skbbuf_list);

    for (i=0; i<skb_num; i++)  {
        memcpy(skb_buf[i].magic, MAGIC_CODE, 4);
        skb_buf[i].buf_pointer = (unsigned int)&skb_buf[i];				
        INIT_LIST_HEAD(&skb_buf[i].list);
        list_add_tail(&skb_buf[i].list, &skbbuf_list);
    }
    root_priv = priv;
#endif
	panic_printk("\n\n#######################################################\n");
    panic_printk("SKB_BUF_SIZE=%d MAX_SKB_NUM=%d\n",skb_size, skb_num);
    panic_printk("#######################################################\n\n");

}


#ifdef CONCURRENT_MODE
static __inline__ unsigned char *get_priv_skb_buf(struct rtl8192cd_priv *priv)
{
	int i;
#ifndef SMP_SYNC
	unsigned long flags;
#endif
	unsigned char *data;

	SAVE_INT_AND_CLI(flags);

	i = priv->pshare->wlandev_idx;
	data = get_buf_from_poll(priv, &skbbuf_list[i], (unsigned int *)&skb_free_num[i]);

	RESTORE_INT(flags);		
	return data;
}

#else

static __inline__ unsigned char *get_priv_skb_buf(struct rtl8192cd_priv *priv)
{
	unsigned char *ret;
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	SAVE_INT_AND_CLI(flags);

	ret = get_buf_from_poll(root_priv, &skbbuf_list, (unsigned int *)&skb_free_num);

	RESTORE_INT(flags);	
	return ret;
}

#endif

#if defined(DUALBAND_ONLY) && defined(CONFIG_RTL8190_PRIV_SKB)
extern u32 if_priv[];
void merge_pool(struct rtl8192cd_priv *priv)
{
	unsigned char *buf;
	unsigned long offset = (unsigned long)(&((struct priv_skb_buf *)0)->buf);
	struct priv_skb_buf *priv_buf;	
	int next_idx;
	int idx = priv->pshare->wlandev_idx;

	if (idx == 0)
		next_idx = 1;
	else
		next_idx = 0;

	while (1) {
		if (skb_free_num[idx] >= REAL_MAX_SKB*2)
			break;

		buf = get_priv_skb_buf((struct rtl8192cd_priv *)if_priv[next_idx]);
		if (buf == NULL)
			break;

		priv_buf = (struct priv_skb_buf *)(((unsigned long)buf) - offset);
		priv_buf->root_priv = priv;
		release_buf_to_poll(priv, buf, &skbbuf_list[idx], (unsigned int *)&skb_free_num[idx]);
	}
}

void split_pool(struct rtl8192cd_priv *priv)
{
	unsigned char *buf;	
	unsigned long offset = (unsigned long)(&((struct priv_skb_buf *)0)->buf);
	struct priv_skb_buf *priv_buf;	
	
	int next_idx;
	int idx = priv->pshare->wlandev_idx;

	if (idx == 0)
		next_idx = 1;
	else
		next_idx = 0;

	while (1) {		
		if (skb_free_num[idx] <= REAL_MAX_SKB)
			break;
		
		buf = get_priv_skb_buf(priv);
		if (buf == NULL)
			break;

		priv_buf = (struct priv_skb_buf *)(((unsigned long)buf) - offset);
		priv_buf->root_priv = (struct rtl8192cd_priv *)if_priv[next_idx];
		release_buf_to_poll((struct rtl8192cd_priv *)if_priv[next_idx], 
			buf, 	&skbbuf_list[next_idx], (unsigned int *)&skb_free_num[next_idx]);
	}
}
#endif

__IRAM_IN_865X
#ifdef CONCURRENT_MODE
struct sk_buff *dev_alloc_skb_priv(struct rtl8192cd_priv *priv, unsigned int size)
{
	struct sk_buff *skb;

	unsigned char *data = get_priv_skb_buf(priv);
	if (data == NULL) {
//		_DEBUG_ERR("wlan: priv skb buffer empty!\n");
		return NULL;
	}

	skb = dev_alloc_8190_skb(data, size);
	if (skb == NULL) {
		free_rtl8190_priv_buf(data);
		return NULL;
	}
	return skb;
}
#else
struct sk_buff *dev_alloc_skb_priv(struct rtl8192cd_priv *priv, unsigned int size)
{
	struct sk_buff *skb;

	unsigned char *data = get_priv_skb_buf(priv);
	if (data == NULL) {
//		_DEBUG_ERR("wlan: priv skb buffer empty!\n");
		return NULL;
	}

	skb = dev_alloc_8190_skb(data, size);
	if (skb == NULL) {
		free_rtl8190_priv_buf(data);
		return NULL;
	}
	return skb;
}
#endif

int is_rtl8190_priv_buf(unsigned char *head)
{
	unsigned long offset = (unsigned long)(&((struct priv_skb_buf *)0)->buf);
	struct priv_skb_buf *priv_buf = (struct priv_skb_buf *)(((unsigned long)head) - offset);
#if 0
	if (!memcmp(priv_buf->magic, MAGIC_CODE, 4) &&
#else
	const unsigned int *magic_code = (unsigned int*) MAGIC_CODE;
	if ( (*(unsigned int*)(priv_buf->magic) == (*magic_code)) &&
#endif
			(priv_buf->buf_pointer == (unsigned int)priv_buf))
		return 1;
	else
		return 0;
}

__IRAM_IN_865X
void free_rtl8190_priv_buf(unsigned char *head)
{

#ifdef CONCURRENT_MODE
	unsigned long offset = (unsigned long)(&((struct priv_skb_buf *)0)->buf);
	struct priv_skb_buf *priv_buf = (struct priv_skb_buf *)(((unsigned long)head) - offset);
	struct rtl8192cd_priv *priv = priv_buf->root_priv;
	int i = priv->pshare->wlandev_idx;
#ifndef SMP_SYNC
	unsigned long x;
#endif	
#ifdef DELAY_REFILL_RX_BUF
	int ret;
#ifdef SMP_SYNC		
	unsigned long flags;
	int locked;
#endif
#ifdef CONFIG_WLAN_HAL
    unsigned int                    q_num;
    PHCI_RX_DMA_MANAGER_88XX        prx_dma;
    PHCI_RX_DMA_QUEUE_STRUCT_88XX   cur_q;
#endif // CONFIG_WLAN_HAL
#ifdef SMP_SYNC
	locked = 0;		
	SMP_TRY_LOCK_RECV(flags,locked);
#endif	
	SAVE_INT_AND_CLI(x);
#ifdef CONFIG_WLAN_HAL
	if (IS_HAL_CHIP(priv)) {
        if (!(priv->drv_state & DRV_STATE_OPEN)){
            /* return 0 means can't refill (because interface be closed or not opened yet) to rx ring but relesae to skb_poll*/            
            ret=0;
        }else{
            q_num   = 0;
            prx_dma = (PHCI_RX_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PRxDMA88XX);
            cur_q   = &(prx_dma->rx_queue[q_num]);
                        
            ret = refill_rx_ring_88XX(priv, NULL, head, q_num, cur_q);
            GET_HAL_INTERFACE(priv)->UpdateRXBDHostIdxHandler(priv, q_num, cur_q->rxbd_ok_cnt);
            cur_q->rxbd_ok_cnt = 0;
        }
	} else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif
	{//not HAL
		ret = refill_rx_ring(priv, NULL, head);
	}
	
	if (ret) {
#ifdef SMP_SYNC		
		if(locked)
			SMP_UNLOCK_RECV(flags);
#endif			
		RESTORE_INT(x);
		return;
	}
	else {
    	release_buf_to_poll(priv, head, &skbbuf_list[i], (unsigned int *)&skb_free_num[i]);
    }
#ifdef SMP_SYNC
    if(locked)
			SMP_UNLOCK_RECV(flags);
#endif			
	RESTORE_INT(x);
#else // ! DELAY_REFILL_RX_BUF
	SAVE_INT_AND_CLI(x);
	release_buf_to_poll(priv, head, &skbbuf_list[i], (unsigned int *)&skb_free_num[i]);
	RESTORE_INT(x);
#endif

#else // ! CONCURRENT_MODE

	unsigned long x;
	struct rtl8192cd_priv *priv = root_priv;
#if 0
	if (!is_rtl8190_priv_buf(head)) {
		printk("wlan: free invalid priv skb buf!\n");
		return;
	}
#endif

#ifdef DELAY_REFILL_RX_BUF
#ifdef CONFIG_WLAN_HAL
    unsigned int                    q_num;
    PHCI_RX_DMA_MANAGER_88XX        prx_dma;
    PHCI_RX_DMA_QUEUE_STRUCT_88XX   cur_q;
#endif
	int ret;

#ifdef SMP_SYNC
	unsigned long flags;
	int locked, cpuid;
	locked = 0;		
	SMP_TRY_LOCK_RECV(flags,locked);
#endif
	SAVE_INT_AND_CLI(x);
#ifdef CONFIG_WLAN_HAL
	if (IS_HAL_CHIP(priv)) {
	    // Currently, only one queue for rx...    
        if (!(priv->drv_state & DRV_STATE_OPEN)){
            /* return 0 means can't refill (because interface be closed or not opened yet) to rx ring but relesae to skb_poll*/            
            ret=0;
        }else{        
    	    q_num   = 0;
    	    prx_dma = (PHCI_RX_DMA_MANAGER_88XX)(_GET_HAL_DATA(priv)->PRxDMA88XX);
    	    cur_q   = &(prx_dma->rx_queue[q_num]); 	
    	    ret = refill_rx_ring_88XX(priv, NULL, head, q_num, cur_q);
    	    GET_HAL_INTERFACE(priv)->UpdateRXBDHostIdxHandler(priv, q_num, cur_q->rxbd_ok_cnt);
    	    cur_q->rxbd_ok_cnt = 0;
        }
	} else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif
	{//not HAL
		ret = refill_rx_ring(priv, NULL, head);
	}
	
	if (ret) {
#ifdef SMP_SYNC
		if(locked)
			SMP_UNLOCK_RECV(flags);
#endif	
		RESTORE_INT(x);
		return;
	} else {
		release_buf_to_poll(root_priv, head, &skbbuf_list, (unsigned int *)&skb_free_num);
	}
#ifdef SMP_SYNC
		if(locked)
			SMP_UNLOCK_RECV(flags);
#endif	
	RESTORE_INT(x);
#else // ! DELAY_REFILL_RX_BUF

	SAVE_INT_AND_CLI(x);
	release_buf_to_poll(root_priv, head, &skbbuf_list, (unsigned int *)&skb_free_num);
	RESTORE_INT(x);	

#endif

#endif
}
#endif //CONFIG_RTL8190_PRIV_SKB



/*
unsigned int set_fw_reg(struct rtl8192cd_priv *priv, unsigned int cmd, unsigned int val, unsigned int with_val)
{
	static unsigned int delay_count;

	delay_count = 10;

	do {
		if (!RTL_R32(0x2c0))
			break;
		delay_us(5);
		delay_count--;
	} while (delay_count);
	delay_count = 10;

	if (with_val == 1)
		RTL_W32(0x2c4, val);

	RTL_W32(0x2c0, cmd);

	do {
		if (!RTL_R32(0x2c0))
			break;
		delay_us(5);
		delay_count--;
	} while (delay_count);

	return 0;
}


void set_fw_A2_entry(struct rtl8192cd_priv *priv, unsigned int cmd, unsigned char *addr)
{
	unsigned int delay_count = 10;

	do{
		if (!RTL_R32(0x2c0))
			break;
		delay_us(5);
		delay_count--;
	} while (delay_count);
	delay_count = 10;

	RTL_W32(0x2c4, addr[3]<<24 | addr[2]<<16 | addr[1]<<8 | addr[0]);
	RTL_W32(0x2c8, addr[5]<<8 | addr[4]);
	RTL_W32(0x2c0, cmd);

	do{
		if (!RTL_R32(0x2c0))
			break;
		delay_us(5);
		delay_count--;
	} while (delay_count);
}
*/

//#if defined(TXREPORT) || defined(SW_ANT_SWITCH) || defined(USE_OUT_SRC)
#if 1
struct stat_info* findNextSTA(struct rtl8192cd_priv *priv, int *idx)
{
	int i;
	for(i= *idx; i<NUM_STAT; i++) {
		if (priv->pshare->aidarray[i] && priv->pshare->aidarray[i]->used == TRUE) {
			*idx = (i+1);
            if (priv->pshare->aidarray[i]->station.cmn_info.ra_info.disable_ra)
				continue;

			return &(priv->pshare->aidarray[i]->station);
		}
	}
	return NULL;
}
#endif

int is_DFS_channel(struct rtl8192cd_priv *priv , int channelVal)
{
	int dfs_channel;

	if(priv->pmib->dot11StationConfigEntry.dot11RegDomain==DOMAIN_MKK)
		dfs_channel = 144;
	else
		dfs_channel = 140;

    if (channelVal >= 52 && channelVal <= dfs_channel) {
        return 1;
    } else {
        return 0;
    }
}

int is_passive_channel(struct rtl8192cd_priv *priv , int domain, int chan)
{
	int dfs_channel;
#ifdef DFS
	if(priv->pmib->dot11StationConfigEntry.dot11RegDomain==DOMAIN_MKK)
		dfs_channel = 144;
	else
		dfs_channel = 140;
	/*during DFS channel , do passive scan*/
    if( (chan >= 52  && chan <= dfs_channel) && !priv->pmib->dot11DFSEntry.disable_DFS){  
        return 1;
    }
#endif    	
    #if 0/*when the mib "w52_passive_scan" enabled , do passive scan in ch W52(ch 36 40 44 48)*/
	else if(((chan >= 36) && (chan <= 48)) 
        && priv->pmib->dot11StationConfigEntry.w52_passive_scan ){
		return 1;
    }
    #endif
    #if 0
	else if ((chan >= 12 && chan <= 14) && (domain == DOMAIN_GLOBAL || domain == DOMAIN_WORLD_WIDE)){
		return 1;
    }
    #endif
	return 0;
}


#if defined(TXREPORT)
void requestTxReport(struct rtl8192cd_priv *priv)
{
	int h2ccmd, counter=20;
	struct stat_info *sta;

	if( priv->pshare->sta_query_idx == -1)
		return;
	
	while(is_h2c_buf_occupy(priv)) {
		delay_ms(2);
		if(--counter==0)
			break;
	}
	if(!counter) 
		return;

	h2ccmd= AP_REQ_RPT;
	
	sta = findNextSTA(priv, &priv->pshare->sta_query_idx);
	if(sta)
		h2ccmd |= (REMAP_AID(sta)<<24);
	else {
		priv->pshare->sta_query_idx = -1;
		return;		
	}
	sta = findNextSTA(priv, &priv->pshare->sta_query_idx);
	if(sta)	{	
		h2ccmd |= (REMAP_AID(sta)<<16);
	} else {
		priv->pshare->sta_query_idx = -1;	
	}

	signin_h2c_cmd(priv, h2ccmd , 0);
	DEBUG_INFO("signin h2c:%x\n", h2ccmd);

}

/*
inital tx rate report from fw
---------------------------------------------------------
0 -> cck 1		  12 -> MCS0	44 -> 1NSS-MCS0 
1 -> cck 2        13 -> MCS1    45 -> 1NSS-MCS1 
2 -> cck 5.5      14 -> MCS2    46 -> 1NSS-MCS2 
3 -> cck 11       15 -> MCS3    47 -> 1NSS-MCS3 
------------      16 -> MCS4    48 -> 1NSS-MCS4 
4 ->  ofdm 6      17 -> MCS5    49 -> 1NSS-MCS5 
5 ->  ofdm 9      18 -> MCS6    50 -> 1NSS-MCS6 
6 ->  ofdm 12     19 -> MCS7    51 -> 1NSS-MCS7 
7 ->  ofdm 18     20 -> MCS8    52 -> 1NSS-MCS8 
8 ->  ofdm 24     21 -> MCS9    53 -> 1NSS-MCS9 
9 ->  ofdm 36     22 -> MCS10   54 -> 2NSS-MCS0 
10 -> ofdm 48     23 -> MCS11   55 -> 2NSS-MCS1 
11 -> ofdm 54     24 -> MCS12   56 -> 2NSS-MCS2 
                  25 -> MCS13   57 -> 2NSS-MCS3 
                  26 -> MCS14   58 -> 2NSS-MCS4 
                  27 -> MCS15   59 -> 2NSS-MCS5 
                                60 -> 2NSS-MCS6 
                                61 -> 2NSS-MCS7 
                                62 -> 2NSS-MCS8 
                                63 -> 2NSS-MCS9 
---------------------------------------------------------                                
*/
#ifdef FOR_DISPLAY_RATE
void get_inital_tx_rate2string(unsigned char txrate ){
	static unsigned char rateStr[16];
    if(txrate>=44 && txrate<=53){
		printk("VHT 1SS-MCS%d\n",txrate-44);
	}
	else if(txrate>=54 && txrate<=63){
		printk("VHT 2SS-MCS%d\n",txrate-54);
	}
	else if(txrate>=12 && txrate<=27){
		printk("MCS%d\n",txrate-12);
	}
	else if(txrate>=0 && txrate<=3){
		if(txrate==0)
			printk("CCK-1\n");
		else if(txrate==1)
			printk("CCK-2\n");		
		else if(txrate==2)
			printk("CCK-5.5\n");
		else if(txrate==3)
			printk("CCK-11\n");
	}
	else if(txrate>=4 && txrate<=11){
		if(txrate==4)
			printk("OFDM-6\n");
		else if(txrate==5)
			printk("OFDM-9\n");
		else if(txrate==6)
			printk("OFDM-12\n");
		else if(txrate==7)
			printk("OFDM-18\n");
		else if(txrate==8)
			printk("OFDM-24\n");
		else if(txrate==9)
			printk("OFDM-36\n");
		else if(txrate==10)
			printk("OFDM-48\n");
		else if(txrate==11)
			printk("OFDM-54\n");		

	}

}

//#define FDEBUG(fmt, args...) panic_printk("[%s %d]"fmt,__FUNCTION__,__LINE__,## args)
#endif

#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL) || defined(CONFIG_RTL_8723B_SUPPORT)

void update_RAMask_to_FW(struct rtl8192cd_priv *priv, int forceUpdate)
{
	int idx = 0;
	struct stat_info *pstat = NULL;

	if( !IS_HAL_CHIP(priv) && (GET_CHIP_VER(priv)!= VERSION_8812E)
#ifdef CONFIG_RTL_8723B_SUPPORT
	&& (GET_CHIP_VER(priv)!= VERSION_8723B)
#endif
	) 
		return;


	if (!forceUpdate && !( priv->pshare->is_40m_bw 
#ifdef WIFI_11N_2040_COEXIST
			&& !((((GET_MIB(priv))->dot11OperationEntry.opmode) & WIFI_AP_STATE) 
			&& priv->pmib->dot11nConfigEntry.dot11nCoexist 
			&& (priv->bg_ap_timeout || orForce20_Switch20Map(priv)
			))
#endif
	))
		return;

	pstat = findNextSTA(priv, &idx);

	while(pstat) {
		if(forceUpdate) {
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
			if (GET_CHIP_VER(priv) == VERSION_8812E || GET_CHIP_VER(priv) == VERSION_8723B) {
				UpdateHalRAMask8812(priv, pstat, 3);
			}
			 else
#endif				
			{
#ifdef CONFIG_WLAN_HAL
				init_rainfo(priv, pstat);
#endif
			}
		} else {
				if(!pstat->cmn_info.bw_mode && pstat->tx_bw) {
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
					if (GET_CHIP_VER(priv) == VERSION_8812E || GET_CHIP_VER(priv) == VERSION_8723B){
						UpdateHalRAMask8812(priv, pstat, 3);
					}
					else
#endif
					{
#ifdef CONFIG_WLAN_HAL                                       
						init_rainfo(priv, pstat);
#endif
					}
				}
		}
		pstat = findNextSTA(priv, &idx);

	}		

}

// TODO: Filen, check 8192E code below
void txrpt_handler_8812(struct rtl8192cd_priv *priv, struct tx_rpt *report, struct stat_info	*pstat)
{
	static unsigned char initial_rate = 0x7f;
	static unsigned char legacyRA =0 ;
	static unsigned int autoRate1=0;
#if defined(MULTI_STA_REFINE) && defined(TX_SHORTCUT)
	unsigned char ratechg=0;
#endif
	/*under auto rate case , pstat->current_tx_rate just for display but it'll be changed, 
	  so, take care! if under fixed rate case don't enter below block*/ 		

	if(!pstat)
		return;
    if(!pstat->cmn_info.ra_info.disable_ra)
	{
		if( should_restrict_Nrate(priv, pstat) && is_fixedMCSTxRate(priv, pstat)){
			legacyRA = 1;
		}

		autoRate1= is_auto_rate(priv, pstat);

		if(	!(legacyRA || autoRate1) )
			return;
		
		//FDEBUG("STA[%02x%02x%02x:%02x%02x%02x]auto rate ,txfail=%d , txok=%d , rate=",
		//	pstat->cmn_info.mac_addr[0],pstat->cmn_info.mac_addr[1],pstat->cmn_info.mac_addr[2],pstat->cmn_info.mac_addr[3],pstat->cmn_info.mac_addr[4],pstat->cmn_info.mac_addr[5],
		//	report->txfail, report->txok );
		//get_inital_tx_rate2string(report->initil_tx_rate&0x3f);
		
		
		initial_rate = report->initil_tx_rate ; 			
		if ((initial_rate & 0x7f) == 0x7f)
			return;
#if defined(MULTI_STA_REFINE) && defined(TX_SHORTCUT)
		if(	((pstat->current_tx_rate >= _MCS0_RATE_) && (pstat->current_tx_rate <= _MCS2_RATE_)) 
			||((pstat->current_tx_rate >= _MCS8_RATE_) && (pstat->current_tx_rate <= _MCS10_RATE_)) 
#ifdef RTK_AC_SUPPORT			
			||((pstat->current_tx_rate >= _NSS1_MCS0_RATE_) && (pstat->current_tx_rate <= _NSS1_MCS2_RATE_)) 
			||((pstat->current_tx_rate >= _NSS2_MCS0_RATE_) && (pstat->current_tx_rate <= _NSS2_MCS2_RATE_))
#endif			
		)
			ratechg=1;
#endif
        if ((initial_rate&0x7f) < 12) {
            pstat->current_tx_rate = dot11_rate_table[initial_rate&0x7f];

			pstat->ht_current_tx_info &= ~TX_USE_SHORT_GI;				
		} else {
            if((initial_rate&0x7f) >= 44){
                pstat->current_tx_rate = VHT_RATE_ID+((initial_rate&0x7f) -44);
			}else{
                pstat->current_tx_rate = HT_RATE_ID+((initial_rate&0x7f) -12);
			}
								
			if (initial_rate & BIT(7))
				pstat->ht_current_tx_info |= TX_USE_SHORT_GI;
			else
				pstat->ht_current_tx_info &= ~TX_USE_SHORT_GI;
		}
	
		if (priv->pshare->is_40m_bw && (pstat->tx_bw == CHANNEL_WIDTH_40))
			pstat->ht_current_tx_info |= TX_USE_40M_MODE;
		else
			pstat->ht_current_tx_info &= ~TX_USE_40M_MODE;
			
		priv->pshare->current_tx_rate    = pstat->current_tx_rate;
		priv->pshare->ht_current_tx_info = pstat->ht_current_tx_info;
#if defined(MULTI_STA_REFINE) && defined(TX_SHORTCUT) && defined(SW_TX_QUEUE)
		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
		if(	((pstat->current_tx_rate >= _MCS0_RATE_) && (pstat->current_tx_rate <= _MCS2_RATE_)) 
			||((pstat->current_tx_rate >= _MCS8_RATE_) && (pstat->current_tx_rate <= _MCS10_RATE_)) 
#ifdef RTK_AC_SUPPORT			
			||((pstat->current_tx_rate >= _NSS1_MCS0_RATE_) && (pstat->current_tx_rate <= _NSS1_MCS2_RATE_)) 
			||((pstat->current_tx_rate >= _NSS2_MCS0_RATE_) && (pstat->current_tx_rate <= _NSS2_MCS2_RATE_))
#endif			
		)
			ratechg^=1;

		if(ratechg && (priv->pshare->swq_numActiveSTA > LOWAGGRESTA))
			clearTxShortCutBufSize(priv, pstat);
	    }
#endif
	}
}
#endif
#ifdef CONFIG_WLAN_HAL
void APReqTXRptHandler(
    struct rtl8192cd_priv   *priv,
    pu1Byte                  pbuf
)
{
    PAPREQTXRPT pparm = (PAPREQTXRPT)pbuf;
  	struct tx_rpt rpt1;
	unsigned char MacID = 0xff;        
    unsigned char idx = 0;
    int j;
    {
        for (j = 0; j < 2; j++) {

            MacID = pparm->txrpt[j].RPT_MACID;
            if (MacID == 0xff)
                continue;

            rpt1.macid =  MacID;

            if (rpt1.macid) { 
                rpt1.txok = le16_to_cpu(pparm->txrpt[j].RPT_TXOK);
                rpt1.txfail = le16_to_cpu(pparm->txrpt[j].RPT_TXFAIL);                  
                rpt1.initil_tx_rate = pparm->txrpt[j].RPT_InitialRate;
              
                txrpt_handler(priv, &rpt1); // add inital tx rate handle for 8812E
            }
            idx += 6;
        }
    }
}
#endif

#if (MU_BEAMFORMING_SUPPORT == 1)
void _txrpt_handler_MU(struct rtl8192cd_priv *priv, struct tx_rpt *report) 
{
	int i;
	PRT_BEAMFORMING_INFO		pBeamInfo = &(priv->pshare->BeamformingInfo);
	PRT_BEAMFORMING_ENTRY		pEntry = NULL;
	unsigned char gid_to_pos1_index[30] = {1,0,2,0,3,0,4,0,5,0,
											2,1,3,1,4,1,5,1,3,2,
											4,2,5,2,4,3,5,3,5,4};

	if(report->macid < 0x80 || report->macid > 0x80+29) {
		panic_printk("Wrong macid=%d\n", report->macid);
		return;
	}
	
	for(i=0; i<BEAMFORMEE_ENTRY_NUM; i++)
	{		
		pEntry = &(pBeamInfo->BeamformeeEntry[i]); 
		if(pEntry->mu_reg_index == gid_to_pos1_index[report->macid - 0x80] && pEntry->pSTA) {
			priv->net_stats.tx_errors += report->txfail;
			pEntry->pSTA->tx_fail += report->txfail;
			pEntry->pSTA->tx_pkts += report->txok+report->txfail;
			return;
		}
	}	

}

#endif

void _txrpt_handler(struct rtl8192cd_priv *priv, struct stat_info *pstat, struct tx_rpt *report)
{
#ifdef CONFIG_VERIWAVE_MU_CHECK
	if(pstat->isVeriwaveInValidSTA && report->txok > 200) {
		pstat->isVeriwaveInValidSTA = 0;		
	}
#endif

#if defined(TXRETRY_CNT)
	pstat->cur_tx_ok += report->txok;
	pstat->cur_tx_fail += report->txfail;
#endif

	if ((GET_CHIP_VER(priv) != VERSION_8822B) && (GET_CHIP_VER(priv) != VERSION_8821C)) /*eric-8822 ??*/
	if ((0 == report->txok) && (0 == report->txfail))
		return;

	priv->net_stats.tx_errors += report->txfail;
	pstat->tx_fail += report->txfail;
	pstat->tx_pkts += report->txfail;

	if (pstat->txrpt_tx_bytes_pre == pstat->tx_bytes) {
		if (report->txok) {
			pstat->txrpt_tx_ok_chk_cnt++;
		}
	} else {
		pstat->txrpt_tx_bytes_pre = pstat->tx_bytes;
		pstat->txrpt_tx_ok_chk_cnt = 0;
	}
	if (pstat->txrpt_tx_ok_chk_cnt <= 10)
		pstat->tx_pkts += report->txok;

	DEBUG_INFO("debug[%02X%02X%02X%02X%02X%02X]:id=%d,ok=%d,fail=%d\n", 
		pstat->cmn_info.mac_addr[0],pstat->cmn_info.mac_addr[1],pstat->cmn_info.mac_addr[2],pstat->cmn_info.mac_addr[3],pstat->cmn_info.mac_addr[4],pstat->cmn_info.mac_addr[5],
		report->macid, report->txok, report->txfail);
	
#ifdef DETECT_STA_EXISTANCE
#ifdef CONFIG_WLAN_HAL
	if(IS_HAL_CHIP(priv))
	{
		DetectSTAExistance88XX(priv, report, pstat);
	} else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif
	{//not HAL
		// Check for STA existance; added by Annie, 2010-08-10.Not support now
#if (defined(CONFIG_RTL_92C_SUPPORT) || defined(CONFIG_RTL_92D_SUPPORT)|| defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT))
		if (CHIP_VER_92X_SERIES(priv) || (GET_CHIP_VER(priv)== VERSION_8812E) || (GET_CHIP_VER(priv)== VERSION_8723B))
			DetectSTAExistance(priv, report, pstat);
#endif
#ifdef CONFIG_RTL_88E_SUPPORT
		if (GET_CHIP_VER(priv) == VERSION_8188E)
			RTL8188E_DetectSTAExistance(priv, report, pstat);
#endif
	}
#endif // DETECT_STA_EXISTANCE

#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL) || defined(CONFIG_RTL_8723B_SUPPORT)
	if( GET_CHIP_VER(priv)== VERSION_8812E || IS_HAL_CHIP(priv) || (GET_CHIP_VER(priv)== VERSION_8723B))
		txrpt_handler_8812(priv, report, pstat);
#endif
}

void txrpt_handler(struct rtl8192cd_priv *priv, struct tx_rpt *report)
{
	struct stat_info	*pstat;	
#ifdef MBSSID
	int i;
#endif

#if(MU_BEAMFORMING_SUPPORT == 1)
	if(report->macid >= 0x80) {
		_txrpt_handler_MU(priv, report);
		return;
	}
	else
#endif
	{
		pstat = get_macidinfo(priv, report->macid);
		if(pstat) {
			_txrpt_handler(priv, pstat, report);
			return;
		}
	}

#ifdef UNIVERSAL_REPEATER
	if (IS_DRV_OPEN(GET_VXD_PRIV(priv)))	{
		pstat = get_macidinfo(GET_VXD_PRIV(priv), report->macid);
		if(pstat) {
			_txrpt_handler(GET_VXD_PRIV(priv), pstat, report);
			return;
		}
	}
#endif
#ifdef MBSSID
	if (GET_ROOT(priv)->pmib->miscEntry.vap_enable) 		{
		for (i=0; i<RTL8192CD_NUM_VWLAN; i++) {
			if (IS_DRV_OPEN(priv->pvap_priv[i])) {
				pstat = get_macidinfo(priv->pvap_priv[i], report->macid);
				if(pstat) {
					_txrpt_handler(priv->pvap_priv[i], pstat, report);
					return;
				}
			}
		}
	}
#endif
}

#ifdef TXRETRY_CNT

int is_support_TxRetryCnt(struct rtl8192cd_priv *priv)
{
	if ( GET_CHIP_VER(priv) == VERSION_8188E
		 || GET_CHIP_VER(priv) == VERSION_8812E
		 || GET_CHIP_VER(priv) == VERSION_8192E 
		 || GET_CHIP_VER(priv) == VERSION_8192F 
		 || GET_CHIP_VER(priv) == VERSION_8881A 
		 || GET_CHIP_VER(priv) == VERSION_8814A
		 || GET_CHIP_VER(priv) == VERSION_8197G
		 || GET_CHIP_VER(priv) == VERSION_8812F)
	return 1;
		
	return 0;

}

void txretry_handler(struct rtl8192cd_priv *priv, struct tx_retry *retry)
{
	int i;
	struct stat_info	*pstat;	
	unsigned char macid_idx = 0;
	struct rtl8192cd_priv *priv_final = NULL;

	for(macid_idx=0; macid_idx<2; macid_idx++)
	{
		priv_final = NULL;
		pstat = get_macidinfo(priv, priv->pshare->sta_query_retry_macid[macid_idx]);

		if(pstat)
			priv_final = priv;
		
#ifdef MBSSID
		if(!pstat && GET_ROOT(priv)->pmib->miscEntry.vap_enable) {
			for (i=0; i<RTL8192CD_NUM_VWLAN; i++) {
				if (IS_DRV_OPEN(priv->pvap_priv[i])) {
					pstat = get_macidinfo(priv->pvap_priv[i], priv->pshare->sta_query_retry_macid[macid_idx]);
					if(pstat) {
						priv_final = priv->pvap_priv[i];
						break;
					}
				}

			}
		}
#endif

#ifdef UNIVERSAL_REPEATER
		if (!pstat && IS_DRV_OPEN(GET_VXD_PRIV(priv)))	{
			pstat = get_macidinfo(GET_VXD_PRIV(priv), priv->pshare->sta_query_retry_macid[macid_idx]);
			if(pstat) {
				priv_final = GET_VXD_PRIV(priv);
			}
		}
#endif

		if(!pstat)
			break;

		pstat->cur_tx_retry_pkts = retry->retry_pkt_macid[macid_idx];
		pstat->cur_tx_retry_cnt = retry->retry_cnt_macid[macid_idx];

		pstat->total_tx_retry_pkts += pstat->cur_tx_retry_pkts;
		pstat->total_tx_retry_cnt += pstat->cur_tx_retry_cnt;
		priv_final->ext_stats.tx_retrys += pstat->cur_tx_retry_cnt;

	}

}


void C2HTxTxRetryHandler(struct rtl8192cd_priv *priv, unsigned char *CmdBuf)
{
	struct tx_retry retry;
	
	retry.stat_idx= CmdBuf[0];
	retry.retry_pkt_macid[0] = CmdBuf[1] | ((short)CmdBuf[2]<<8);
	retry.retry_cnt_macid[0] = CmdBuf[3] | ((short)CmdBuf[4]<<8);
	retry.retry_pkt_macid[1] = CmdBuf[5] | ((short)CmdBuf[6]<<8);
	retry.retry_cnt_macid[1] = CmdBuf[7] | ((short)CmdBuf[8]<<8);

#if 0
	panic_printk("%s [%d] %d %d : %d %d\n", __FUNCTION__, retry.stat_idx, 
		retry.retry_pkt_macid[0], retry.retry_cnt_macid[0], 
		retry.retry_pkt_macid[1], retry.retry_cnt_macid[1] );
#endif

	txretry_handler(priv, &retry);
}

#endif

#if defined(CONFIG_RTL_92C_SUPPORT) || defined(CONFIG_RTL_92D_SUPPORT)
void C2H_isr(struct rtl8192cd_priv *priv)
{
	struct tx_rpt rpt1;
	int j, tmp32, idx=0x1a2;
#ifndef SMP_SYNC
	unsigned long flags;
#endif
	SAVE_INT_AND_CLI(flags);
	tmp32 = RTL_R16(0x1a0);
	if( (tmp32&0xff)==0xc2 ) {
		for(j=0; j<2; j++) {
			rpt1.macid= (0x1f) & RTL_R8(idx+4);
			if(rpt1.macid) {
#ifdef _BIG_ENDIAN_
				rpt1.txok = le16_to_cpu(RTL_R16(idx+2));
				rpt1.txfail = le16_to_cpu(RTL_R16(idx));
#else
				rpt1.txok = be16_to_cpu(RTL_R16(idx+2));
				rpt1.txfail = be16_to_cpu(RTL_R16(idx));
#endif
				txrpt_handler(priv, &rpt1);
			}			
			idx+=6;		
		}
	}
	RTL_W8( 0x1af, 0);
	requestTxReport(priv);
	RESTORE_INT(flags);
}
#endif

#endif
/*cfg p2p*/
#if !defined(__ECOS) && !defined(__OSK__)
void convert_bin_to_str(unsigned char *bin, int len, char *out)
{
        int i;
        char tmpbuf[10];

        out[0] = '\0';

        for (i=0; i<len; i++) {
                sprintf(tmpbuf, "%02x", bin[i]);
                strcat(out, tmpbuf);
        }
}
#endif
/*cfg p2p*/
static int _is_hex(char c)
{
    return (((c >= '0') && (c <= '9')) ||
            ((c >= 'A') && (c <= 'F')) ||
            ((c >= 'a') && (c <= 'f')));
}


int rtl_string_to_hex(char *string, unsigned char *key, int len)
{
	char tmpBuf[4];
	int idx, ii=0;
	for (idx=0; idx<len; idx+=2) {
		tmpBuf[0] = string[idx];
		tmpBuf[1] = string[idx+1];
		tmpBuf[2] = 0;
		if (!_is_hex(tmpBuf[0]) || !_is_hex(tmpBuf[1]))
			return 0;

		key[ii++] = (unsigned char) _atoi(tmpBuf,16);
	}
	return 1;
}


#if defined(CONFIG_RTL_CUSTOM_PASSTHRU)
#ifdef __ECOS
INT32 rtl_isWlanPassthruFrame(UINT8 *data)
#else
INT32 rtl_isPassthruFrame(UINT8 *data)
#endif
{
	int	ret;

#ifdef __ECOS
	ret = FAILED;
#else
	ret = FAIL;
#endif
	if (passThruStatusWlan)
	{
		if (passThruStatusWlan&IP6_PASSTHRU_MASK)
		{
			if ((*((UINT16*)(data+(ETH_ALEN<<1)))==__constant_htons(ETH_P_IPV6)) ||
				((*((UINT16*)(data+(ETH_ALEN<<1)))==__constant_htons(ETH_P_8021Q))&&(*((UINT16*)(data+(ETH_ALEN<<1)+VLAN_HLEN))==__constant_htons(ETH_P_IPV6))))
			{
				ret = SUCCESS;
			}
		}
		#if defined(CONFIG_RTL_CUSTOM_PASSTHRU_PPPOE)
		if (passThruStatusWlan&PPPOE_PASSTHRU_MASK)
		{
			if (((*((UINT16*)(data+(ETH_ALEN<<1)))==__constant_htons(ETH_P_PPP_SES))||(*((UINT16*)(data+(ETH_ALEN<<1)))==__constant_htons(ETH_P_PPP_DISC))) ||
				((*((UINT16*)(data+(ETH_ALEN<<1)))==__constant_htons(ETH_P_8021Q))&&((*((UINT16*)(data+(ETH_ALEN<<1)+VLAN_HLEN))==__constant_htons(ETH_P_PPP_SES))||(*((UINT16*)(data+(ETH_ALEN<<1)+VLAN_HLEN))==__constant_htons(ETH_P_PPP_DISC)))))
			{
				ret = SUCCESS;
			}
		}
		#endif
	}

	return ret;
}
#endif

#if 0	//defined(HS2_SUPPORT) || defined(DOT11K) || defined(CH_LOAD_CAL)
void rtl8192cd_cu_stop(struct rtl8192cd_priv *priv) {
    unsigned char stop_cu = 1;
    int i;
    struct rtl8192cd_priv * root_priv = GET_ROOT(priv);
    priv->cu_enable = 0;


    /*check if other interface need channel utilization calcultation*/
    if (IS_DRV_OPEN(root_priv) && root_priv->cu_enable) {
        stop_cu = 0;
    }
#ifdef MBSSID
    if (stop_cu == 1 && root_priv->pmib->miscEntry.vap_enable) {
        for (i=0; i<RTL8192CD_NUM_VWLAN; i++) {
            if (IS_DRV_OPEN(root_priv->pvap_priv[i]) && root_priv->pvap_priv[i]->cu_enable) {
                stop_cu = 0;
                break;
            }
        }
    }
#endif

#ifdef UNIVERSAL_REPEATER    
    if (stop_cu == 1 && IS_DRV_OPEN(GET_VXD_PRIV(root_priv)) &&
        GET_VXD_PRIV(root_priv)->cu_enable) {
        stop_cu = 0;
    }
#endif     
    
    if(stop_cu) {
        if (timer_pending(&priv->pshare->cu_info.cu_cntdwn_timer)) {
            del_timer_sync(&priv->pshare->cu_info.cu_cntdwn_timer);
        }
        priv->pshare->cu_info.cu_enable = 0;
    }
}


void rtl8192cd_cu_start(struct rtl8192cd_priv *priv) {
    priv->cu_enable = 1;    
    if(priv->pshare->cu_info.cu_enable == 0) {
        priv->pshare->cu_info.channel_utilization = 0;
        init_timer(&priv->pshare->cu_info.cu_cntdwn_timer);
#if defined(CONFIG_PCI_HCI)
		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
	        priv->pshare->cu_info.cu_cntdwn_timer.data = (unsigned long) priv;
	        priv->pshare->cu_info.cu_cntdwn_timer.function = rtl8192cd_cu_cntdwn_timer;
		}
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
		if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
        	priv->pshare->cu_info.cu_cntdwn_timer.data = (unsigned long) &priv->pshare->cu_info.cu_cntdwn_timer_event;
        	priv->pshare->cu_info.cu_cntdwn_timer.function = timer_event_timer_fn;
        	INIT_TIMER_EVENT_ENTRY(&priv->pshare->cu_info.cu_cntdwn_timer_event, rtl8192cd_cu_cntdwn_timer, (unsigned long)priv);
		}
#endif
        priv->pshare->cu_info.cu_cntdwn = priv->pshare->cu_info.cu_initialcnt = (priv->pmib->dot11StationConfigEntry.channel_utili_beaconIntval * priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod)/CU_Intval;
        start_bbp_ch_load(priv, 50000);
#ifdef __KERNEL__
        priv->pshare->cu_info.cu_cntdwn_timer.expires = jiffies + CU_TO;
#endif
        mod_timer(&priv->pshare->cu_info.cu_cntdwn_timer, jiffies + CU_TO);
        priv->pshare->cu_info.cu_enable = 1;
    }
}

void rtl8192cd_cu_cntdwn_timer(unsigned long task_priv)
{
    struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
    int val;

    if ((val = read_bbp_ch_load(priv)) == -1)
    {
        mod_timer(&priv->pshare->cu_info.cu_cntdwn_timer, jiffies + RTL_MILISECONDS_TO_JIFFIES(10));	
    }
    else
    {
        priv->pshare->cu_info.chbusytime += val;
        priv->pshare->cu_info.cu_cntdwn--;
        if (priv->pshare->cu_info.cu_cntdwn == 0)
        {
            priv->pshare->cu_info.channel_utilization = (priv->pshare->cu_info.chbusytime*255)/(priv->pmib->dot11StationConfigEntry.channel_utili_beaconIntval * priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod * 1024);
            priv->pshare->cu_info.chbusytime = 0;
            priv->pshare->cu_info.cu_cntdwn = priv->pshare->cu_info.cu_initialcnt;
        }
        start_bbp_ch_load(priv, 50000);
        mod_timer(&priv->pshare->cu_info.cu_cntdwn_timer, jiffies + CU_TO);
    }
}

#endif

#if defined(RTK_ATM) && !defined(HS2_SUPPORT)
void staip_snooping_byarp(struct sk_buff *pskb, struct stat_info *pstat)
{
	struct arphdr *arp = (struct arphdr *)(pskb->data + ETH_HLEN);
	unsigned char *arp_ptr = (unsigned char *)(arp + 1);
	if((arp->ar_pro == __constant_htons(ETH_P_IP)) && (arp->ar_op == htons(ARPOP_REQUEST))) {
		//find sender ip
		arp_ptr += arp->ar_hln;
		//backup sender ip
		if ((*arp_ptr == 0) && (*(arp_ptr+1) == 0) && (*(arp_ptr+2) == 0) && (*(arp_ptr+3) == 0))
			return;
		else {
			memcpy(pstat->sta_ip, arp_ptr, 4);
			//panic_printk("ARP cache ip=%d.%d.%d.%d\n", pstat->sta_ip[0],pstat->sta_ip[1],pstat->sta_ip[2],pstat->sta_ip[3]);
		}
	}
}
#endif

#ifdef HS2_SUPPORT
#ifdef __ECOS
#define ipv6_addr_copy(a1,a2) memcpy(a1, a2, sizeof(struct in6_addr))
static int ipv6_addr_equal(const struct in6_addr *a1,
				  const struct in6_addr *a2)
{
	return (((a1->s6_addr32[0] ^ a2->s6_addr32[0]) |
		 (a1->s6_addr32[1] ^ a2->s6_addr32[1]) |
		 (a1->s6_addr32[2] ^ a2->s6_addr32[2]) |
		 (a1->s6_addr32[3] ^ a2->s6_addr32[3])) == 0);
}
static int
inet_cksum(u_short *addr, int len)
{
    register int nleft = len;
    register u_short *w = addr;
    register u_short answer;
    register u_int sum = 0;
    u_short odd_byte = 0;
    while( nleft > 1 )  {
        sum += *w++;
        nleft -= 2;
    }
    if( nleft == 1 ) {
        *(u_char *)(&odd_byte) = *(u_char *)w;
        sum += odd_byte;
    }
    sum = (sum >> 16) + (sum & 0x0000ffff); /* add hi 16 to low 16 */
    sum += (sum >> 16);                     /* add carry */
    answer = ~sum;                          /* truncate to 16 bits */
    return (answer);
}
#endif

unsigned int getDSCP2UP(struct rtl8192cd_priv *priv, unsigned char DSCP)
{
	int i;

	for(i=0;i<MAX_DSCP_EXCEPT;i++) {
		if(DSCP == priv->pmib->hs2Entry.QoSMAP_except[i][0]) // EXCEPTION DSCP VALUE
			return priv->pmib->hs2Entry.QoSMAP_except[i][1]; // PRIORITY
	}

	for(i=0;i<MAX_QOS_PRIORITY;i++) {
		if(DSCP >= priv->pmib->hs2Entry.QoSMAP_range[i][0] &&  // DSCP LOWER BOUND
			DSCP <= priv->pmib->hs2Entry.QoSMAP_range[i][1])   // DSCP UPPER BOUND
			return i; // PRIORITY
	}
	return 0;
}

void setQoSMapConf(struct rtl8192cd_priv *priv)
{
	int i;
	int curIndex;
	int curQoSMap = priv->pmib->hs2Entry.curQoSMap;	
	
	for(i=0;i<MAX_DSCP_EXCEPT;i++)
	{			
		priv->pmib->hs2Entry.QoSMAP_except[i][0] = -1;
		priv->pmib->hs2Entry.QoSMAP_except[i][1] = -1;
		curIndex++;
	}

	if(priv->pmib->hs2Entry.QoSMap_ielen[curQoSMap] < 16) 
	{	
		HS2_DEBUG_ERR("QoSMAPIE length is too small\n");
		return;
	} else if(priv->pmib->hs2Entry.QoSMap_ielen[curQoSMap] > 16) 
	{	
		if(priv->pmib->hs2Entry.QoSMap_ielen[curQoSMap]-16 > MAX_DSCP_EXCEPT * 2) {
			HS2_DEBUG_ERR("QoSMAP_except matrix supports %d except only, need enlarge QoSMAP_except matrix\n", MAX_DSCP_EXCEPT);
			return;
		}
		curIndex = 0;	
		// include DSCP exception
		for(i=0;i<priv->pmib->hs2Entry.QoSMap_ielen[curQoSMap]-MAX_QOS_PRIORITY*2;i+=2)
		{			
			priv->pmib->hs2Entry.QoSMAP_except[curIndex][0] = priv->pmib->hs2Entry.QoSMap_ie[curQoSMap][i]; // DSCP
			priv->pmib->hs2Entry.QoSMAP_except[curIndex][1] = priv->pmib->hs2Entry.QoSMap_ie[curQoSMap][i+1]; // UP
			curIndex++;
		}
	}
	
	curIndex = 0;
	for(i=priv->pmib->hs2Entry.QoSMap_ielen[curQoSMap]-MAX_QOS_PRIORITY*2;i<priv->pmib->hs2Entry.QoSMap_ielen[curQoSMap];i+=2)
	{			
		priv->pmib->hs2Entry.QoSMAP_range[curIndex][0] = priv->pmib->hs2Entry.QoSMap_ie[curQoSMap][i]; // DSCP
		priv->pmib->hs2Entry.QoSMAP_range[curIndex][1] = priv->pmib->hs2Entry.QoSMap_ie[curQoSMap][i+1]; // UP
		curIndex++;
	}

	for(i=0;i<MAX_DSCP_EXCEPT;i++)
		HS2_DEBUG_INFO("DSCP Except[%d]=%x %x\n",i,priv->pmib->hs2Entry.QoSMAP_except[i][0],priv->pmib->hs2Entry.QoSMAP_except[i][1]);
	for(i=0;i<MAX_QOS_PRIORITY;i++)
		HS2_DEBUG_INFO("DSCP range[%d]=%x %x\n",i,priv->pmib->hs2Entry.QoSMAP_range[i][0],priv->pmib->hs2Entry.QoSMAP_range[i][1]);
	
		
}
void calcu_sta_v6ip(struct stat_info *pstat)
{
    int idx=0;

	struct in6_addr addrp;
	addrp.s6_addr[0] = 0xfe;
	addrp.s6_addr[1] = 0x80;
	addrp.s6_addr[2] = 0x00;
	addrp.s6_addr[3] = 0x00;
	addrp.s6_addr[4] = 0x00;
	addrp.s6_addr[5] = 0x00;
	addrp.s6_addr[6] = 0x00;
	addrp.s6_addr[7] = 0x00;
	addrp.s6_addr[8] = pstat->cmn_info.mac_addr[0] | 0x02;
	addrp.s6_addr[9] = pstat->cmn_info.mac_addr[1];
	addrp.s6_addr[10] = pstat->cmn_info.mac_addr[2];
	addrp.s6_addr[11] = 0xff;
	addrp.s6_addr[12] = 0xfe;
	addrp.s6_addr[13] = pstat->cmn_info.mac_addr[3];
    addrp.s6_addr[14] = pstat->cmn_info.mac_addr[4];
    addrp.s6_addr[15] = pstat->cmn_info.mac_addr[5];

    if(pstat->v6ipCount==0){
    	ipv6_addr_copy(&pstat->sta_v6ip[pstat->v6ipCount], &addrp);

        pstat->v6ipCount=1;

        HS2DEBUG("v6ipCount[%d],sta[%02X%02X%02X:%02X%02X%02X]\n",pstat->v6ipCount
             ,pstat->cmn_info.mac_addr[0],pstat->cmn_info.mac_addr[1],pstat->cmn_info.mac_addr[2]
             ,pstat->cmn_info.mac_addr[3],pstat->cmn_info.mac_addr[4],pstat->cmn_info.mac_addr[5]);
        HS2DEBUG("learn1 V6 IP[%02X%02X%02X%02X][%02X%02X%02X%02X][%02X%02X%02X%02X][%02X%02X%02X%02X]\n"
            ,addrp.s6_addr[0],addrp.s6_addr[1],addrp.s6_addr[2],addrp.s6_addr[3]
            ,addrp.s6_addr[4],addrp.s6_addr[5],addrp.s6_addr[6],addrp.s6_addr[7]
            ,addrp.s6_addr[8],addrp.s6_addr[9],addrp.s6_addr[10],addrp.s6_addr[11]
            ,addrp.s6_addr[12],addrp.s6_addr[13],addrp.s6_addr[14],addrp.s6_addr[15]);        
    }else{
        for(idx=0;idx<pstat->v6ipCount;idx++) {
            if (ipv6_addr_equal(&pstat->sta_v6ip[idx], &addrp)){
                return; /*has existed no need add*/ 
            }
        }

    	ipv6_addr_copy(&pstat->sta_v6ip[pstat->v6ipCount], &addrp);

        pstat->v6ipCount ++;
        
        /*didn't exist , add*/ 
        HS2DEBUG("v6ipCount[%d],sta[%02X%02X%02X:%02X%02X%02X]\n",pstat->v6ipCount
             ,pstat->cmn_info.mac_addr[0],pstat->cmn_info.mac_addr[1],pstat->cmn_info.mac_addr[2]
             ,pstat->cmn_info.mac_addr[3],pstat->cmn_info.mac_addr[4],pstat->cmn_info.mac_addr[5]);
        HS2DEBUG("learn2 V6 IP[%02X%02X%02X%02X][%02X%02X%02X%02X][%02X%02X%02X%02X][%02X%02X%02X%02X]\n"
            ,addrp.s6_addr[0],addrp.s6_addr[1],addrp.s6_addr[2],addrp.s6_addr[3]
            ,addrp.s6_addr[4],addrp.s6_addr[5],addrp.s6_addr[6],addrp.s6_addr[7]
            ,addrp.s6_addr[8],addrp.s6_addr[9],addrp.s6_addr[10],addrp.s6_addr[11]
            ,addrp.s6_addr[12],addrp.s6_addr[13],addrp.s6_addr[14],addrp.s6_addr[15]);

        
    }



                        

}

void staip_snooping_byarp(struct sk_buff *pskb, struct stat_info *pstat)
{
	struct arphdr *arp = (struct arphdr *)(pskb->data + ETH_HLEN);
	unsigned char *arp_ptr = (unsigned char *)(arp + 1);
	if((arp->ar_pro == __constant_htons(ETH_P_IP)) && (arp->ar_op == htons(ARPOP_REQUEST))) {
		//find sender ip
		arp_ptr += arp->ar_hln;
		//backup sender ip
		if ((*arp_ptr == 0) && (*(arp_ptr+1) == 0) && (*(arp_ptr+2) == 0) && (*(arp_ptr+3) == 0))
			return;
		else {
			memcpy(pstat->sta_ip, arp_ptr, 4);
			//HS2DEBUG("ARP cache ip=%d.%d.%d.%d\n", pstat->sta_ip[0],pstat->sta_ip[1],pstat->sta_ip[2],pstat->sta_ip[3]);
		}
	}
}
#ifdef __ECOS
void stav6ip_snooping_bynsolic(struct sk_buff *pskb, struct stat_info *pstat)
{
	struct ip6_hdr *hdr = (struct ip6_hdr *)(pskb->data+ETH_HLEN);
    struct icmp6hdr *icmphdr;
	int pkt_len, type;
	if (hdr->ip6_vfc != 6)
        return;

	if (hdr->ip6_hlim != 255)
		return;
	pkt_len = ntohs(hdr->ip6_plen);
	if (pkt_len>0)
    {
        icmphdr = (struct icmp6hdr *)(pskb->data+ETH_HLEN+sizeof(*hdr));
        type = icmphdr->icmp6_type;
		if (type == ND_NEIGHBOR_SOLICIT)
		{
			if ((hdr->ip6_src.s6_addr32[0] == 0) && (hdr->ip6_src.s6_addr32[1] == 0) && (hdr->ip6_src.s6_addr32[2] == 0) && (hdr->ip6_src.s6_addr32[3] == 0)) {
				struct in6_addr *target = (struct in6_addr *) (icmphdr + 1);
				if ((target->s6_addr32[0] == 0) && (target->s6_addr32[1] == 0) && (target->s6_addr32[2] == 0) && (target->s6_addr32[3] == 0)) {
					return;
				}
				else {
					printk("rcv multicast ns duplicate addr\n");
					ipv6_addr_copy(&pstat->sta_v6ip, target);
				}
			}
			else {
				printk("rcv multicast ns\n");
				ipv6_addr_copy(&pstat->sta_v6ip, &hdr->ip6_src);
			}
		}
	}
}
#else
void stav6ip_snooping_bynsolic(struct rtl8192cd_priv *priv,struct sk_buff *pskb, struct stat_info *pstat)
{
	struct ipv6hdr *hdr = (struct ipv6hdr *)(pskb->data+ETH_HLEN);
    struct icmp6hdr *icmphdr;
	int pkt_len, type;
	int i;
	struct stat_info *pstat1;
	struct list_head *phead, *plist;
	if (hdr->version != 6)
        return;

	if (hdr->hop_limit != 255)
		return;

	pkt_len = ntohs(hdr->payload_len);
	if (pkt_len>0)
    {
        icmphdr = (struct icmp6hdr *)(pskb->data+ETH_HLEN+sizeof(*hdr));
        type = icmphdr->icmp6_type;
		if (type == NDISC_NEIGHBOUR_SOLICITATION)
		{
    		HS2DEBUG("rx neighbour solicitation from[%02X%02X%02X:%02X%02X%02X]\n",
                pstat->cmn_info.mac_addr[0],pstat->cmn_info.mac_addr[1],pstat->cmn_info.mac_addr[2],pstat->cmn_info.mac_addr[3],pstat->cmn_info.mac_addr[4],pstat->cmn_info.mac_addr[5]);      
		    /*20141209 , p Note ,when src ip=NULL && target IP!=NULL*/
			if ((hdr->saddr.s6_addr32[0] == 0) && (hdr->saddr.s6_addr32[1] == 0) 
                 && (hdr->saddr.s6_addr32[2] == 0) && (hdr->saddr.s6_addr32[3] == 0)) 
            {
                HS2DEBUG("src ip(v6)=NULL\n");
				struct in6_addr *target = (struct in6_addr *) (icmphdr + 1);
                
				if ((target->s6_addr32[0] == 0) && (target->s6_addr32[1] == 0) 
                    && (target->s6_addr32[2] == 0) && (target->s6_addr32[3] == 0)) 
                {
                    HS2DEBUG("tar ip(v6)=NULL\n");                
					return;
				}
				else {

                    HS2DEBUG("tar ip(v6) [%08X %08X %08X %08X]\n",target->s6_addr32[0],target->s6_addr32[1],
                        target->s6_addr32[2],target->s6_addr32[3]);

					phead = &priv->asoc_list;
				    plist = phead->next;
					
				    while (phead && (plist != phead))
				    {
				    	pstat1 = list_entry(plist, struct stat_info, asoc_list);
						plist = plist->next;
						for(i=0;i<pstat1->v6ipCount;i++) {
					    	if (ipv6_addr_equal(&pstat1->sta_v6ip[i], target)){
								return;
                            }
						}
				    }
					if (pstat->v6ipCount<10){

						ipv6_addr_copy(&pstat->sta_v6ip[pstat->v6ipCount++], target);

						HS2DEBUG("learn tar ip(v6)[%08X %08X %08X %08X],v6ipCount[%d]\n",target->s6_addr32[0],target->s6_addr32[1],
                            target->s6_addr32[2],target->s6_addr32[3],pstat->v6ipCount);
                        
					}else{
						HS2DEBUG("the number of v6ipaddress > 10\n");
					}
				}
			}
			else {

                HS2DEBUG("src ip(v6)[%08X %08X %08X %08X]\n",
                    hdr->saddr.s6_addr32[0],hdr->saddr.s6_addr32[1],hdr->saddr.s6_addr32[2],hdr->saddr.s6_addr32[3]);
                

				phead = &priv->asoc_list;
			    plist = phead->next;
				
			    while (phead && (plist != phead))
			    {
			    	pstat1 = list_entry(plist, struct stat_info, asoc_list);
					plist = plist->next;
					for(i=0;i<pstat1->v6ipCount;i++) {
				    	if (ipv6_addr_equal(&pstat1->sta_v6ip[i], &hdr->saddr))
							return;
					}
			    }
				if(pstat->v6ipCount < 10){


					ipv6_addr_copy(&pstat->sta_v6ip[pstat->v6ipCount++], &hdr->saddr);

					HS2DEBUG("learn (src) v6 ip [%08X %08X %08X %08X],v6ipCount[%d]\n",
                        hdr->saddr.s6_addr32[0],hdr->saddr.s6_addr32[1],hdr->saddr.s6_addr32[2],hdr->saddr.s6_addr32[3],pstat->v6ipCount);
                    
				}else{
					HS2DEBUG("the number of v6ipaddress > 10\n");
				}
			}
		}
	}
}
#endif
#if 0
void stav6ip_snooping_bynadvert(struct sk_buff *pskb, struct stat_info *pstat)
{
    struct ipv6hdr *hdr = (struct ipv6hdr *)(pskb->data+ETH_HLEN);
    struct icmp6hdr *icmphdr;
    int pkt_len, type;
    if (hdr->version != 6)
        return;

	if (hdr->hop_limit != 255)
		return;

    pkt_len = ntohs(hdr->payload_len);
    if (pkt_len>0)
    {
        icmphdr = (struct icmp6hdr *)(pskb->data+ETH_HLEN+sizeof(*hdr));
        type = icmphdr->icmp6_type;
        if (type == NDISC_NEIGHBOUR_ADVERTISEMENT)
        {
            if ((hdr->saddr.s6_addr32[0] == 0) && (hdr->saddr.s6_addr32[1] == 0) && (hdr->saddr.s6_addr32[2] == 0) && (hdr->saddr.s6_addr32[3] == 0)) {
				return;
			}
			else {
				printk("rcv unsolicited neighbor advert multicast\n");
                ipv6_addr_copy(&pstat->sta_v6ip, &hdr->saddr);
            }
        }
    }
}
#endif

void staip_snooping_bydhcp(struct sk_buff *pskb, struct rtl8192cd_priv *priv) //struct stat_info *pstat)
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

	struct stat_info *pstat;
	struct iphdr* iph;
	struct udphdr *udph;
	struct dhcpMessage *dhcph;
	struct list_head *phead, *plist;
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif

	iph = (struct iphdr *)(pskb->data + ETH_HLEN);
	udph = (struct udphdr *)((unsigned long)iph + (iph->ihl << 2));
	dhcph = (struct dhcpMessage *)((unsigned long)udph + sizeof(struct udphdr));

    phead = &priv->asoc_list;
    SMP_LOCK_ASOC_LIST(flags);
    
    plist = phead->next;
    while (phead && (plist != phead))
    {
        pstat = list_entry(plist, struct stat_info, asoc_list);
        plist = plist->next;

        if (!memcmp(pstat->cmn_info.mac_addr, &dhcph->chaddr[0], 6)) {
			if (dhcph->op == 2) { //dhcp reply
				if (dhcph->yiaddr) {
					memcpy(pstat->sta_ip, &dhcph->yiaddr, 4);
					printk("dhcp give yip=%d.%d.%d.%d\n", pstat->sta_ip[0],pstat->sta_ip[1],pstat->sta_ip[2],pstat->sta_ip[3]);
				}
			}
			break;
		}
	}

    SMP_UNLOCK_ASOC_LIST(flags);
}

int check_nei_advt(struct rtl8192cd_priv *priv, struct sk_buff *skb)
{
#ifdef __ECOS
	struct ip6_hdr *hdr = (struct ip6_hdr *)(skb->data+ETH_HLEN);
#else
	struct ipv6hdr *hdr = (struct ipv6hdr *)(skb->data+ETH_HLEN);
#endif
	struct icmp6hdr *icmpv6;
	unsigned int pkt_len;
    int type;
#ifdef __ECOS
	pkt_len = ntohs(hdr->ip6_plen);
#else
	pkt_len = ntohs(hdr->payload_len);
#endif
	if (pkt_len>0)
    {
        icmpv6 = (struct icmp6hdr *)(skb->data+ETH_HLEN+sizeof(*hdr));
        type = icmpv6->icmp6_type;
        //printk("pkt len=%d,type=%d\n", pkt_len, type);
#ifdef __ECOS
        if (type == ND_NEIGHBOR_ADVERT)
#else
        if (type == NDISC_NEIGHBOUR_ADVERTISEMENT)
#endif
		{
			printk("drop nei advr\n");
			return 1;	
		}
		else
			return 0;
	}
	return 0;
}

int proxy_icmpv6_ndisc(struct rtl8192cd_priv *priv, struct sk_buff *skb)
{
	struct sk_buff *newskb = NULL;
	struct in6_addr *addrp;
#ifdef __ECOS
	struct ip6_hdr *hdr = (struct ip6_hdr *)(skb->data+ETH_HLEN);
	struct ip6_hdr *replyhdr;
#else
	struct ipv6hdr *hdr = (struct ipv6hdr *)(skb->data+ETH_HLEN); // ipv6hdr len = 40 bytes
	struct ipv6hdr *replyhdr;
#endif
	struct icmp6hdr *icmpv6_nsolic;
	struct icmp6hdr *icmpv6_nadvt;	
	struct stat_info *pstat;
    struct list_head *phead, *plist;
	unsigned int pkt_len;
	int type;
	int idx=0;
	int foundTarget=0;
	//HS2DEBUG("proxy_icmpv6_ndisc\n");
#ifdef __ECOS
	if (hdr->ip6_vfc != 6)
		return 0;
	pkt_len = ntohs(hdr->ip6_plen);
#else
	if (hdr->version != 6)
		return 0;
	
	pkt_len = ntohs(hdr->payload_len);
#endif
	if (pkt_len>0)
	{
		icmpv6_nsolic = (struct icmp6hdr *)(skb->data+ETH_HLEN+sizeof(*hdr));
		type = icmpv6_nsolic->icmp6_type;
		//printk("pkt len=%d,type=%d\n", pkt_len, type);
#ifdef __ECOS
		if (type == ND_NEIGHBOR_SOLICIT)
#else
		if (type == NDISC_NEIGHBOUR_SOLICITATION)
#endif
        {

			addrp = (struct in6_addr *)(icmpv6_nsolic+1);

  		    /*20141209 , p Note ,when src ip=NULL , need not reply*/
			if ((hdr->saddr.s6_addr32[0] == 0) && (hdr->saddr.s6_addr32[1] == 0) 
                 && (hdr->saddr.s6_addr32[2] == 0) && (hdr->saddr.s6_addr32[3] == 0)) 
            {
                HS2DEBUG("(src v6 ip is NULL)\n");
                return 0;
            }

        	HS2DEBUG(" Neighbour Solicitation form [%02X%02X%02X:%02X%02X%02X]\n",
                skb->data[6],skb->data[7],skb->data[8],skb->data[9],skb->data[10],skb->data[11]);
            
        	HS2DEBUG("search (tar v6 ip)[%08X %08X %08X %08X]\n",
                addrp->s6_addr32[0],addrp->s6_addr32[1],addrp->s6_addr32[2],addrp->s6_addr32[3]);
            
			if (!memcmp(skb->data+ETH_ALEN, priv->pmib->dot11StationConfigEntry.dot11Bssid, ETH_ALEN))
	        {
		        HS2DEBUG("!!!  v6:arp req src mac=BSSID\n");
			    return 0;
	        }
			#ifdef __ECOS
			if((hdr->ip6_dst.s6_addr32[0] | hdr->ip6_dst.s6_addr32[1] |
		 	hdr->ip6_dst.s6_addr32[2] | (hdr->ip6_dst.s6_addr32[3] ^ htonl(1))) == 0)
			#else
			if (ipv6_addr_loopback(&hdr->daddr))
			#endif
			{
				HS2DEBUG("v6:loopback\n");
		        return 0;
			}

			//search target ip mapping pstat mac
			phead = &priv->asoc_list;
	        plist = phead->next;
		    while (phead && (plist != phead))
			{
	            pstat = list_entry(plist, struct stat_info, asoc_list);
		        plist = plist->next;



				foundTarget=0;
				for(idx=0;idx<pstat->v6ipCount;idx++){
					if (ipv6_addr_equal(&pstat->sta_v6ip[idx], addrp)){
						foundTarget=1;
						break;
					}
				}
		        if (foundTarget)
			    {
					#if defined(CONFIG_RTL865X_ETH_PRIV_SKB) || defined(CONFIG_RTL_ETH_PRIV_SKB)
				    extern struct sk_buff *priv_skb_copy(struct sk_buff *skb);
	                newskb = priv_skb_copy(skb);
					#else
		            newskb = skb_copy(skb, GFP_ATOMIC);
					#endif
					//HS2DEBUG("compare ok!!\n");
					if (newskb == NULL)
	                {
		                priv->ext_stats.tx_drops++;
			            HS2DEBUG("alloc icmpv6 neighbor advertisement skb null!!\n");
				        rtl_kfree_skb(priv, skb, _SKB_TX_);
					    return 1;
					}
					else
					{
						unsigned char *opt;
						int len;
						//da
						memcpy(newskb->data, newskb->data+ETH_ALEN, 6);
						//sa
						memcpy(newskb->data+ETH_ALEN, pstat->cmn_info.mac_addr, 6);
#ifdef __ECOS
						replyhdr = (struct ip6_hdr *)(newskb->data+ETH_HLEN);
						ipv6_addr_copy(&replyhdr->ip6_src, &pstat->sta_v6ip);
						ipv6_addr_copy(&replyhdr->ip6_dst, &hdr->ip6_src);
						icmpv6_nadvt = (struct icmp6hdr *)(newskb->data+ETH_HLEN+sizeof(*hdr));
				        icmpv6_nadvt->icmp6_type = ND_NEIGHBOR_ADVERT;
						icmpv6_nadvt->icmp6_dataun.u_nd_advt.solicited = 1;
						icmpv6_nadvt->icmp6_dataun.u_nd_advt.override = 0;
						#else				
						replyhdr = (struct ipv6hdr *)(newskb->data+ETH_HLEN);
						ipv6_addr_copy(&replyhdr->saddr, &pstat->sta_v6ip[idx]);
						ipv6_addr_copy(&replyhdr->daddr, &hdr->saddr);

						icmpv6_nadvt = (struct icmp6hdr *)(newskb->data+ETH_HLEN+sizeof(*hdr));
				        icmpv6_nadvt->icmp6_type = NDISC_NEIGHBOUR_ADVERTISEMENT;
						icmpv6_nadvt->icmp6_solicited = 1;
						icmpv6_nadvt->icmp6_override = 0;
						#endif						
						//target ip
						opt = (unsigned char *)(newskb->data+ETH_HLEN+sizeof(*replyhdr)+sizeof(struct icmp6hdr));
						ipv6_addr_copy((struct in6_addr *)opt, &pstat->sta_v6ip[idx]);
						opt += sizeof(struct in6_addr);
						//option
						opt[0] = 2; // Type: Target link-layer addr
						opt[1] = 1; // Length: 1 (8 bytes)
						memcpy(opt+2, pstat->cmn_info.mac_addr, ETH_ALEN);
						icmpv6_nadvt->icmp6_cksum = 0;
						len = sizeof(struct icmp6hdr)+sizeof(struct in6_addr)+8;
						#ifdef __ECOS
							icmpv6_nadvt->icmp6_cksum = inet_cksum( (unsigned short *)&replyhdr, len);
						#else
						icmpv6_nadvt->icmp6_cksum = csum_ipv6_magic(&replyhdr->saddr, &replyhdr->daddr, len, 
							IPPROTO_ICMPV6, csum_partial(icmpv6_nadvt, len, 0));
						#endif
						
						dev_kfree_skb_any(skb);
						#ifdef __ECOS
						if (ipv6_addr_equal(&replyhdr->ip6_src, &replyhdr->ip6_dst))
						#else
						if (ipv6_addr_equal(&replyhdr->saddr, &replyhdr->daddr))
						#endif
                        {
                            HS2DEBUG("\n\n!!! v6:tip=sip!!\n\n\n");
                            dev_kfree_skb_any(newskb);
                            return 1;
                        }

						if ((pstat = get_stainfo(priv, newskb->data)) != NULL)
	                    {
	                        int i;
							HS2DEBUG("v6:da in wlan\n\n");
							newskb->cb[2] = (char)0xff;         // not do aggregation
							memcpy(newskb->cb+10,newskb->data,6);
							#if 1
							HS2DEBUG("data=");
							for(i=0;i<6;i++) {
								panic_printk("%02x ",newskb->data[i]);
							}
							panic_printk("\n");
							for(i=0;i<6;i++) {
								panic_printk("%02x ",newskb->data[i+6]);
							}
							panic_printk("\n");
							panic_printk("%02x %02x\n",newskb->data[12],newskb->data[13]); // type                            
							for(i=0;i< 8 ;i++) {
								panic_printk("%02x ",newskb->data[i+14]);
							}
							panic_printk("\n");                            
							for(i=0;i< 16 ;i++) {
								panic_printk("%02x ",newskb->data[i+14+8]);
							}
							panic_printk("\n");                            
							for(i=0;i< 16 ;i++) {
								panic_printk("%02x ",newskb->data[i+14+8+16]);
							}   
							panic_printk("\n");                            
							for(i=0;i< 6 ;i++) {
								panic_printk("%02x ",newskb->data[i+14+8+16+16]);
							}                                                        
							panic_printk("\n");                            
							#endif

							HS2DEBUG("rx Nei Solici relay Nei Adv\n\n");
							//dev_kfree_skb_any(newskb);
	                        __rtl8192cd_start_xmit(newskb, priv->dev, 1);
						}
						else
						{				
							HS2DEBUG("v6:da in lan\n");
							#if defined(__KERNEL__) || defined(__OSK__)
	                        if (newskb->dev)
							#ifdef __LINUX_2_6__
		                        newskb->protocol = eth_type_trans(newskb, newskb->dev);
			                else
							#endif
				                newskb->protocol = eth_type_trans(newskb, priv->dev);
							#endif

							#if defined(__LINUX_2_6__) && defined(RX_TASKLET) && !defined(CONFIG_RTL8672) && !(defined(__LINUX_3_10__) || defined(__LINUX_3_2__))
					        netif_receive_skb(newskb);
							#else
						    netif_rx(newskb);
							#endif		
						}
						return 1;
					}
				}
                
			}

            HS2DEBUG("no found!\n");
            
		}
	}
	return 0;
}

int proxy_arp_handle(struct rtl8192cd_priv *priv, struct sk_buff *skb)
{
	struct sk_buff *newskb = NULL;
	struct arphdr *arp = (struct arphdr *)(skb->data + ETH_HLEN);
	unsigned char *arp_ptr = (unsigned char *)(arp + 1), *psender, *ptarget, *psender_bak;
	struct stat_info *pstat;
	struct list_head *phead, *plist;
	int k;
	unsigned char *tmp = (unsigned char *)(skb->data);
	
	/*if((arp->ar_pro == __constant_htons(ETH_P_IP)) && (arp->ar_op == htons(ARPOP_REQUEST)||arp->ar_op == htons(ARPOP_REPLY)))
	{
		arp_ptr += arp->ar_hln;
		psender_bak = arp_ptr;
		//target ip
		arp_ptr += (arp->ar_hln + arp->ar_pln);
		ptarget = arp_ptr;
		if (!memcmp(psender_bak,ptarget,4))
		{
			printk("gratuitous ARP Request or Reply\n");
			return 0;
		}
	}*/
	HS2_DEBUG_TRACE(2, "Proxy ARP handle\n");
	if((arp->ar_pro == __constant_htons(ETH_P_IP)) && (arp->ar_op == htons(ARPOP_REQUEST)))
	{
		//{
		//	int j;
		//	printk("orin==>");
		//	for(j=0;j<skb->len;j++)
		//		printk("0x%02x:",*(skb->data+j));
		//	printk("\n");
		//}
		//sender ip
		arp_ptr += arp->ar_hln;
		psender_bak = arp_ptr;
		//target ip
		arp_ptr += (arp->ar_hln + arp->ar_pln);
		ptarget = arp_ptr;
	
		if (!memcmp(skb->data+ETH_ALEN, priv->pmib->dot11StationConfigEntry.dot11Bssid, ETH_ALEN))
		{
			HS2_DEBUG_TRACE(1, "arp req src mac=BSSID\n");
			return 0;
		}
#ifdef __ECOS
		if( (((unsigned int )ptarget & htonl(0xff000000)) == htonl(0x7f000000)) ||
			(((unsigned int )ptarget & htonl(0xf0000000)) == htonl(0xe0000000)))

#else
		if (ipv4_is_loopback(ptarget) || ipv4_is_multicast(ptarget))
#endif
		{
			HS2_DEBUG_TRACE(1, "loopback or muticast!!\n");
	        return 0;
		}

		//search target ip mapping pstat mac
		phead = &priv->asoc_list;
		plist = phead->next;
		while (phead && (plist != phead)) 
		{
			pstat = list_entry(plist, struct stat_info, asoc_list);
			plist = plist->next;
			HS2_DEBUG_INFO("Proxy ARP: Find Destination in Assocation List, sta_ip=%d.%d.%d.%d\n",pstat->sta_ip[0],pstat->sta_ip[1],pstat->sta_ip[2],pstat->sta_ip[3]);
			if (!memcmp(pstat->sta_ip, ptarget, 4))
			{
				panic_printk("Proxy ARP: Find Destination in Assocation List\n");
#if defined(CONFIG_RTL865X_ETH_PRIV_SKB) || defined(CONFIG_RTL_ETH_PRIV_SKB)
				extern struct sk_buff *priv_skb_copy(struct sk_buff *skb);
				newskb = priv_skb_copy(skb);
#else
				newskb = skb_copy(skb, GFP_ATOMIC);
#endif
				if (newskb == NULL) 
				{
					priv->ext_stats.tx_drops++;
					HS2_DEBUG_ERR("alloc arp rsp skb null!!\n");
					rtl_kfree_skb(priv, skb, _SKB_TX_);
					return 1;
				}	
				else
				{
					// ======================
					// build new arp response
					// ======================
					//da
					memcpy(newskb->data, newskb->data+ETH_ALEN, 6);
					//memcpy(newskb->data, priv->pmib->dot11StationConfigEntry.dot11Bssid, 6);
					//sa
					memcpy(newskb->data+ETH_ALEN, pstat->cmn_info.mac_addr, 6);
					//memcpy(newskb->data+ETH_ALEN, priv->pmib->dot11StationConfigEntry.dot11Bssid, 6);
					//arp response			
					arp = (struct arphdr *)(newskb->data + ETH_HLEN);
					arp_ptr = (unsigned char *)(arp + 1);
					arp->ar_op = htons(ARPOP_REPLY);
					//sender mac and ip
					memcpy(arp_ptr, pstat->cmn_info.mac_addr, 6);
					arp_ptr += arp->ar_hln;
					psender = (unsigned char *)arp_ptr;
					memcpy(psender, ptarget, 4);
					//printk("sender mac and ip:%02x:%02x:%02x:%02x:%02x:%02x,%d.%d.%d.%d\n",pstat->cmn_info.mac_addr[0],pstat->cmn_info.mac_addr[1],pstat->cmn_info.mac_addr[2],pstat->cmn_info.mac_addr[3],pstat->cmn_info.mac_addr[4],pstat->cmn_info.mac_addr[5],ptarget[0],ptarget[1],ptarget[2],ptarget[3]);
					//target mac and ip
					arp_ptr += arp->ar_pln;
					memcpy(arp_ptr, newskb->data, 6);
					//memcpy(newskb->data, priv->pmib->dot11StationConfigEntry.dot11Bssid, 6);

					arp_ptr += arp->ar_hln;
					ptarget = arp_ptr;
					memcpy((unsigned char *)ptarget, (unsigned char *)psender_bak, 4);
					//printk("target mac and ip:%02x:%02x:%02x:%02x:%02x:%02x,%d.%d.%d.%d\n",newskb->data[0],newskb->data[1],newskb->data[2],newskb->data[3],newskb->data[4],newskb->data[5],ptarget[0],ptarget[1],ptarget[2],ptarget[3]);
							
					rtl_kfree_skb(priv, skb, _SKB_TX_);
					
					if (!memcmp(ptarget, psender, 4))
					{
						HS2_DEBUG_TRACE(2, "target ip = sender ip!!\n");
						dev_kfree_skb_any(newskb);
						return 1;
					}

					if ((pstat = get_stainfo(priv, newskb->data)) != NULL)
					{
						//struct sk_buff_head *pqueue;
					    //struct timer_list *ptimer;
					    //void (*timer_hook)(unsigned long task_priv);

						//if (newskb->dev)
//#ifdef __LINUX_2_6__
//							newskb->protocol = eth_type_trans(newskb, newskb->dev);
//						else
//#endif
//							newskb->protocol = eth_type_trans(newskb, priv->dev);

//						printk("enq\n");
//						pqueue = &pstat->swq.be_queue;
//			            ptimer = &pstat->swq.beq_timer;
//						timer_hook = rtl8192cd_beq_timer;

//					    skb_queue_tail(pqueue, newskb);
//					    ptimer->data = (unsigned long)pstat;
//						ptimer->function = timer_hook; //rtl8190_tmp_timer;
//			            mod_timer(ptimer, jiffies + 1);

//						 SAVE_INT_AND_CLI(x);
						//pstat = get_stainfo(priv, newskb->data);
						HS2_DEBUG_TRACE(1, "da in wlan\n");
				        //__rtl8192cd_start_xmit_out(newskb, pstat, NULL);
						newskb->cb[2] = (char)0xff;         // not do aggregation
						memcpy(newskb->cb+10,newskb->data,6);
                        __rtl8192cd_start_xmit(newskb, priv->dev, 1);
//				        RESTORE_INT(x);
	
					}
					else
					{		
						HS2_DEBUG_TRACE(1, "da in lan\n");
#if defined(__KERNEL__) || defined(__OSK__)
						if (newskb->dev)
#ifdef __LINUX_2_6__
							newskb->protocol = eth_type_trans(newskb, newskb->dev);
	                    else
#endif
		                    newskb->protocol = eth_type_trans(newskb, priv->dev);
#endif
			
#if defined(__LINUX_2_6__) && defined(RX_TASKLET) && !defined(CONFIG_RTL8672)
						netif_receive_skb(newskb);
#else
						netif_rx(newskb);
#endif
					}
					return 1;
				}			
			}
		}
	}
	
	//drop packet
	return 0;		
}


#endif



#ifdef TLN_STATS
void stats_conn_rson_counts(struct rtl8192cd_priv *priv, unsigned int reason)
{
	switch (reason) {
	case _RSON_UNSPECIFIED_:
		priv->ext_wifi_stats.rson_UNSPECIFIED_1++;
		break;
	case _RSON_AUTH_NO_LONGER_VALID_:
		priv->ext_wifi_stats.rson_AUTH_INVALID_2++;
		break;
	case _RSON_DEAUTH_STA_LEAVING_:
		priv->ext_wifi_stats.rson_DEAUTH_STA_LEAVING_3++;
		break;
	case _RSON_INACTIVITY_:
		priv->ext_wifi_stats.rson_INACTIVITY_4++;
		break;
	case _RSON_UNABLE_HANDLE_:
		priv->ext_wifi_stats.rson_RESOURCE_INSUFFICIENT_5++;
		break;
	case _RSON_CLS2_:
		priv->ext_wifi_stats.rson_UNAUTH_CLS2FRAME_6++;
		break;
	case _RSON_CLS3_:
		priv->ext_wifi_stats.rson_UNAUTH_CLS3FRAME_7++;
		break;
	case _RSON_DISAOC_STA_LEAVING_:
		priv->ext_wifi_stats.rson_DISASSOC_STA_LEAVING_8++;
		break;
	case _RSON_ASOC_NOT_AUTH_:
		priv->ext_wifi_stats.rson_ASSOC_BEFORE_AUTH_9++;
		break;
	case _RSON_INVALID_IE_:
		priv->ext_wifi_stats.rson_INVALID_IE_13++;
		break;
	case _RSON_MIC_FAILURE_:
		priv->ext_wifi_stats.rson_MIC_FAILURE_14++;
		break;
	case _RSON_4WAY_HNDSHK_TIMEOUT_:
		priv->ext_wifi_stats.rson_4WAY_TIMEOUT_15++;
		break;
	case _RSON_GROUP_KEY_UPDATE_TIMEOUT_:
		priv->ext_wifi_stats.rson_GROUP_KEY_TIMEOUT_16++;
		break;
	case _RSON_DIFF_IE_:
		priv->ext_wifi_stats.rson_DIFF_IE_17++;
		break;
	case _RSON_MLTCST_CIPHER_NOT_VALID_:
		priv->ext_wifi_stats.rson_MCAST_CIPHER_INVALID_18++;
		break;
	case _RSON_UNICST_CIPHER_NOT_VALID_:
		priv->ext_wifi_stats.rson_UCAST_CIPHER_INVALID_19++;
		break;
	case _RSON_AKMP_NOT_VALID_:
		priv->ext_wifi_stats.rson_AKMP_INVALID_20++;
		break;
	case _RSON_UNSUPPORT_RSNE_VER_:
		priv->ext_wifi_stats.rson_UNSUPPORT_RSNIE_VER_21++;
		break;
	case _RSON_INVALID_RSNE_CAP_:
		priv->ext_wifi_stats.rson_RSNIE_CAP_INVALID_22++;
		break;
	case _RSON_IEEE_802DOT1X_AUTH_FAIL_:
		priv->ext_wifi_stats.rson_802_1X_AUTH_FAIL_23++;
		break;
	default:
		priv->ext_wifi_stats.rson_OUT_OF_SCOPE++;
		/*panic_printk("incorrect reason(%d) for statistics\n", reason);*/
		break;
	}

	priv->wifi_stats.rejected_sta++;
}


void stats_conn_status_counts(struct rtl8192cd_priv *priv, unsigned int status)
{
	switch (status) {
	case _STATS_SUCCESSFUL_:
		priv->wifi_stats.connected_sta++;
		break;
	case _STATS_FAILURE_:
		priv->ext_wifi_stats.status_FAILURE_1++;
		break;
	case _STATS_CAP_FAIL_:
		priv->ext_wifi_stats.status_CAP_FAIL_10++;
		break;
	case _STATS_NO_ASOC_:
		priv->ext_wifi_stats.status_NO_ASSOC_11++;
		break;
	case _STATS_OTHER_:
		priv->ext_wifi_stats.status_OTHER_12++;
		break;
	case _STATS_NO_SUPP_ALG_:
		priv->ext_wifi_stats.status_NOT_SUPPORT_ALG_13++;
		break;
	case _STATS_OUT_OF_AUTH_SEQ_:
		priv->ext_wifi_stats.status_OUT_OF_AUTH_SEQ_14++;
		break;
	case _STATS_CHALLENGE_FAIL_:
		priv->ext_wifi_stats.status_CHALLENGE_FAIL_15++;
		break;
	case _STATS_AUTH_TIMEOUT_:
		priv->ext_wifi_stats.status_AUTH_TIMEOUT_16++;
		break;
	case _STATS_UNABLE_HANDLE_STA_:
		priv->ext_wifi_stats.status_RESOURCE_INSUFFICIENT_17++;
		break;
	case _STATS_RATE_FAIL_:
		priv->ext_wifi_stats.status_RATE_FAIL_18++;
		break;
	default:
		priv->ext_wifi_stats.status_OUT_OF_SCOPE++;
		/*panic_printk("incorrect status(%d) for statistics\n", status);*/
		break;
	}

	if (status != _STATS_SUCCESSFUL_)
		priv->wifi_stats.rejected_sta++;
}
#endif

unsigned long
get_total_tx_average(
		struct rtl8192cd_priv *priv
		)
{
		unsigned char i;
		unsigned long totaltp=0;
		
		totaltp = priv->ext_stats.tx_avarage;
#ifdef UNIVERSAL_REPEATER  
		totaltp += GET_VXD_PRIV(priv)->ext_stats.tx_avarage;
#endif
#ifdef MBSSID
		for(i=0; i<RTL8192CD_NUM_VWLAN; i++)
			totaltp += priv->pvap_priv[i]->ext_stats.tx_avarage;
#endif
		
		return totaltp>>17;

}


#if defined(CONFIG_RTL_ULINKER)
int get_wlan_opmode(struct net_device *dev)
{
	int opmode = -1;
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)dev->priv;

	if (netif_running(dev)) {
		if ((priv->pmib->dot11OperationEntry.opmode) & WIFI_AP_STATE)
			opmode = 0;
		else
			opmode = 1;
	}

	return opmode;
}
#endif



#ifdef RF_MIMO_SWITCH

void Do_BB_Reset(struct rtl8192cd_priv *priv)
{
	unsigned char tmp_reg2 = 0;
	tmp_reg2 = RTL_R8(0x2);
			
	tmp_reg2 &= (~BIT(0));
	RTL_W8(0x2, tmp_reg2);
	tmp_reg2 |= BIT(0);
	RTL_W8(0x2, tmp_reg2);

}

void Assert_BB_Reset(struct rtl8192cd_priv *priv)
{
	unsigned char tmp_reg2 = 0;
	tmp_reg2 = RTL_R8(0x2);

	tmp_reg2 &= (~BIT(0));
	RTL_W8(0x2, tmp_reg2);
}

void Release_BB_Reset(struct rtl8192cd_priv *priv)
{
	unsigned char tmp_reg2 = 0;
	tmp_reg2 = RTL_R8(0x2);

	tmp_reg2 |= BIT(0);
	RTL_W8(0x2, tmp_reg2);
}

void RF_MIMO_check_timer(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	int i=0, assoc_num = priv->assoc_num;
	
	if (!(priv->drv_state & DRV_STATE_OPEN))
		return;

	if(get_rf_mimo_mode(priv) != RF_2T2R && get_rf_mimo_mode(priv) != RF_3T3R)
		return;

    if(priv->auto_channel || timer_pending(&priv->ss_timer)) {
        goto end;
    }
#ifdef MCR_WIRELESS_EXTEND
	return;
#endif 
#ifdef MP_TEST
	if (((OPMODE & WIFI_MP_STATE) || priv->pshare->rf_ft_var.mp_specific))
		return;
#endif	
	if(0 
#ifdef WDS
	|| (priv->pmib->dot11WdsInfo.wdsEnabled)
#endif
#ifdef DFS
	|| is_DFS_channel(priv, priv->pmib->dot11RFEntry.dot11channel)
#endif
#ifdef UNIVERSAL_REPEATER
	|| (IS_DRV_OPEN(GET_VXD_PRIV(priv)))
#endif
	) {
        if(get_rf_mimo_mode(priv) == RF_3T3R && (priv->pshare->rf_status == RF_2T2R || priv->pshare->rf_status == RF_1T1R))
            set_MIMO_Mode(priv, RF_3T3R);
        else if(get_rf_mimo_mode(priv) == RF_2T2R && priv->pshare->rf_status == RF_1T1R)
            set_MIMO_Mode(priv, RF_2T2R);
		return;	
	}

	if (priv->pshare->rf_ft_var.rf_mode == 0) {
#ifdef MBSSID
		if (priv->pmib->miscEntry.vap_enable) {
			for (i=0; i<RTL8192CD_NUM_VWLAN; ++i)
				assoc_num += priv->pvap_priv[i]-> assoc_num;
		}
#endif
		if (assoc_num) {
			if (get_rf_mimo_mode(priv) == RF_3T3R && (priv->pshare->rf_status == RF_2T2R || priv->pshare->rf_status == RF_1T1R))
				set_MIMO_Mode(priv, RF_3T3R);
			else if (get_rf_mimo_mode(priv) == RF_2T2R && priv->pshare->rf_status == RF_1T1R)
				set_MIMO_Mode(priv, RF_2T2R);
		}
		else {
#if defined(WIFI_11N_2040_COEXIST_EXT)
			if((priv->pshare->CurrentChannelBW == CHANNEL_WIDTH_40) /*|| (priv->pshare->CurrentChannelBW == CHANNEL_WIDTH_80)*/) {
				priv->pshare->CurrentChannelBW = CHANNEL_WIDTH_20;
				SwBWMode(priv, priv->pshare->CurrentChannelBW, priv->pshare->offset_2nd_chan);
				SwChnl(priv, priv->pmib->dot11RFEntry.dot11channel, priv->pshare->offset_2nd_chan);
			}
#endif	
			if (get_rf_mimo_mode(priv) == RF_3T3R && priv->pshare->rf_status != RF_2T2R && priv->pshare->rf_status != RF_1T1R) {
				if (priv->pshare->rf_ft_var.idle_ps_ext)
					set_MIMO_Mode(priv, RF_1T1R);
				else
					set_MIMO_Mode(priv, RF_2T2R);
			}
			else if (get_rf_mimo_mode(priv) == RF_2T2R && priv->pshare->rf_status != RF_1T1R) {
				 set_MIMO_Mode(priv, RF_1T1R);
			}
		}
	} else if (priv->pshare->rf_ft_var.rf_mode == 1) {
		set_MIMO_Mode(priv, RF_1T1R);
	} else if (priv->pshare->rf_ft_var.rf_mode == 2) {
		set_MIMO_Mode(priv, RF_2T2R);
	}else if (priv->pshare->rf_ft_var.rf_mode == 3) {
		set_MIMO_Mode(priv, RF_3T3R);
	} else if (priv->pshare->rf_ft_var.rf_mode == 4) {
		set_MIMO_Mode(priv, RF_4T4R);
	}

 end:
	mod_timer(&priv->ps_timer, jiffies + IDLE_T0);
}

int assign_MIMO_TR_Mode(struct rtl8192cd_priv *priv, unsigned char *data)
{
#define dprintf printk
	int mode = _atoi(data, 16);
	if (strlen(data) == 0) {
		printk("tr mode.\n");
		printk("mimo 1: switch to 1T1R\n");
		printk("mimo 2: switch to 2T2R\n");
		printk("mimo 3: switch to 3T3R\n");
		printk("mimo 4: switch to 4T4R\n");
		printk("mimo 0: auto\n");
		return 0;
	}
	if (mode == 0x01)	{
		MIMO_Mode_Switch(priv, RF_1T1R);
		priv->pshare->rf_ft_var.rf_mode = 1;
	} else if (mode == 0x02)	 {		
		MIMO_Mode_Switch(priv, RF_2T2R);
		priv->pshare->rf_ft_var.rf_mode = 2;
	} else if (mode == 0x03)	 {		
		MIMO_Mode_Switch(priv, RF_3T3R);
		priv->pshare->rf_ft_var.rf_mode = 3;
	} else if (mode == 0x04)	 {		
		MIMO_Mode_Switch(priv, RF_4T4R);
		priv->pshare->rf_ft_var.rf_mode = 4;
	} else {
		priv->pshare->rf_ft_var.rf_mode = 0;
	}
	return 0;
}
#endif

#if !defined(CONFIG_RTL_TRIBAND_SUPPORT)
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
struct list_head* asoc_list_get_next(struct rtl8192cd_priv *priv, struct list_head *plist)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	struct list_head *phead, *plist_next;
	struct stat_info *pstat;
	
	phead = &priv->asoc_list;
	
	SMP_LOCK_ASOC_LIST(flags);
	
	plist_next = plist->next;
	if (plist_next != phead) {
		pstat = list_entry(plist_next, struct stat_info, asoc_list);
		pstat->asoc_list_refcnt++;
#ifdef __ECOS
		cyg_flag_maskbits(&pstat->asoc_unref_done, ~0x1);
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0))
		reinit_completion(&pstat->asoc_unref_done);
#else
		INIT_COMPLETION(pstat->asoc_unref_done);
#endif
#endif
	}
	
	SMP_UNLOCK_ASOC_LIST(flags);
	
	if (plist != phead) {
		pstat =  list_entry(plist, struct stat_info, asoc_list);
		asoc_list_unref(priv, pstat);
	}
	
	return plist_next;
}

void asoc_list_unref(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	
	SMP_LOCK_ASOC_LIST(flags);
	
	BUG_ON(0 == pstat->asoc_list_refcnt);
	
	pstat->asoc_list_refcnt--;
	if (0 == pstat->asoc_list_refcnt) {
		list_del_init(&pstat->asoc_list);
#ifdef __ECOS
		cyg_flag_setbits(&pstat->asoc_unref_done, 0x1);
#else
		complete(&pstat->asoc_unref_done);
#endif
	}
	
	SMP_UNLOCK_ASOC_LIST(flags);
}

// The function returns whether it had linked to asoc_list before removing.
int asoc_list_del(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	int ret = 0, wait = 0;
	
#ifdef __KERNEL__
	might_sleep();
#endif

	SMP_LOCK_ASOC_LIST(flags);
	
	if (0 != pstat->asoc_list_refcnt) {
		pstat->asoc_list_refcnt--;
		if (0 ==pstat->asoc_list_refcnt) {
//			if (!list_empty(&pstat->asoc_list)) {
				list_del_init(&pstat->asoc_list);
				ret = 1;
//			}
		} else {
			wait = 1;
		}
	}
	
	SMP_UNLOCK_ASOC_LIST(flags);
	
	if (wait) {
#ifdef __ECOS
		cyg_flag_wait(&pstat->asoc_unref_done, 0x01, CYG_FLAG_WAITMODE_OR | CYG_FLAG_WAITMODE_CLR);
#else
		wait_for_completion(&pstat->asoc_unref_done);
#endif
		ret = 1;
	}
	
	return ret;
}

int asoc_list_add(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	int ret = 0;
	
	SMP_LOCK_ASOC_LIST(flags);
	
	if (list_empty(&pstat->asoc_list)) {
		list_add_tail(&pstat->asoc_list, &priv->asoc_list);
		pstat->asoc_list_refcnt = 1;
		ret = 1;
	}
	
	SMP_UNLOCK_ASOC_LIST(flags);
	
	return ret;
}
#endif // CONFIG_USB_HCI || CONFIG_SDIO_HCI
#endif /* !defined(CONFIG_RTL_TRIBAND_SUPPORT) */

#ifdef WIFI_SIMPLE_CONFIG
#define IGNORE_DISCON_TIMEOUT		3
void wsc_disconn_list_add(struct rtl8192cd_priv *priv, unsigned char *hwaddr)
{
	struct wsc_disconn_entry *entry;
	unsigned long flags;
	
	entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
	if (NULL == entry)
		return;

	INIT_LIST_HEAD(&entry->list);
	memcpy(entry->addr, hwaddr, MACADDRLEN);
	entry->state = WSC_DISCON_STATE_RECV_EAP_FAIL;
	entry->expire_to = IGNORE_DISCON_TIMEOUT;
	
	spin_lock_irqsave(&priv->wsc_disconn_list_lock, flags);

	list_add_tail(&entry->list, &priv->wsc_disconn_list);

	spin_unlock_irqrestore(&priv->wsc_disconn_list_lock, flags);
}

void wsc_disconn_list_expire(struct rtl8192cd_priv *priv)
{
	struct list_head *phead, *plist;
	struct wsc_disconn_entry *entry;
	unsigned long flags;

	phead = &priv->wsc_disconn_list;
	
	if (list_empty(phead))
		return;

	spin_lock_irqsave(&priv->wsc_disconn_list_lock, flags);

	plist = phead->next;
	
	while (plist != phead) {
		entry = list_entry(plist, struct wsc_disconn_entry, list);
		plist = plist->next;
		
		entry->expire_to--;

		if(priv->pshare->rf_ft_var.fix_expire_to_zero == 1) {
			entry->expire_to = 0;
			priv->pshare->rf_ft_var.fix_expire_to_zero = 0;
		}		

		if (0 == entry->expire_to) {
			list_del(&entry->list);
			kfree(entry);
		}
	}

	spin_unlock_irqrestore(&priv->wsc_disconn_list_lock, flags);
}

void wsc_disconn_list_update(struct rtl8192cd_priv *priv, unsigned char *hwaddr)
{
	struct list_head *phead, *plist;
	struct wsc_disconn_entry *entry;
	unsigned long flags;

	phead = &priv->wsc_disconn_list;
	
	if (list_empty(phead))
		return;

	spin_lock_irqsave(&priv->wsc_disconn_list_lock, flags);

	plist = phead->next;
	
	while (plist != phead) {
		entry = list_entry(plist, struct wsc_disconn_entry, list);
		plist = plist->next;
		
		if (!memcmp(entry->addr, hwaddr, MACADDRLEN)) {
			if (WSC_DISCON_STATE_RECV_EAP_FAIL == entry->state)
				entry->state = WSC_DISCON_STATE_IGNORE;
			break;
		}
	}

	spin_unlock_irqrestore(&priv->wsc_disconn_list_lock, flags);
}

int wsc_disconn_list_check(struct rtl8192cd_priv *priv, unsigned char *hwaddr)
{
	struct list_head *phead, *plist;
	struct wsc_disconn_entry *entry;
	unsigned long flags;
	int status;

	phead = &priv->wsc_disconn_list;

	if (list_empty(phead))
		return FALSE;
	
	spin_lock_irqsave(&priv->wsc_disconn_list_lock, flags);

	plist = phead->next;
	
	while (plist != phead) {
		entry = list_entry(plist, struct wsc_disconn_entry, list);
		plist = plist->next;
		
		if (!memcmp(entry->addr, hwaddr, MACADDRLEN)) {
			list_del(&entry->list);
			spin_unlock_irqrestore(&priv->wsc_disconn_list_lock, flags);
			
			status = (WSC_DISCON_STATE_IGNORE == entry->state) ? TRUE : FALSE;
			kfree(entry);
			return status;
		}
	}

	spin_unlock_irqrestore(&priv->wsc_disconn_list_lock, flags);

	return FALSE;
}

void wsc_disconn_list_flush(struct rtl8192cd_priv *priv)
{
	struct list_head *phead, *plist;
	struct wsc_disconn_entry *entry;
	unsigned long flags;

	phead = &priv->wsc_disconn_list;

	if (list_empty(phead))
		return;

	spin_lock_irqsave(&priv->wsc_disconn_list_lock, flags);

	plist = phead->next;
	
	while (plist != phead) {
		entry = list_entry(plist, struct wsc_disconn_entry, list);
		plist = plist->next;
		kfree(entry);
	}

	spin_unlock_irqrestore(&priv->wsc_disconn_list_lock, flags);
}
#endif // WIFI_SIMPLE_CONFIG

//This function check all AP interfaces' status
// func_map: bit0~bit15 : AP interface active or not, bit 16~bit31:active connection or not
int checkAPfunc(struct rtl8192cd_priv *priv, unsigned int *func_map)
{
	struct rtl8192cd_priv *root_priv=GET_ROOT(priv);
	unsigned int idx=0;

	if(root_priv->pmib->miscEntry.func_off==0) {
		*func_map |= BIT0;

		if(root_priv->assoc_num)
			*func_map |= BIT16;
	}
#if defined(MBSSID)
	if(root_priv->pmib->miscEntry.vap_enable) {
		for (idx=0; idx<RTL8192CD_NUM_VWLAN; idx++) {
			if (IS_DRV_OPEN(root_priv->pvap_priv[idx])&& 
				(root_priv->pvap_priv[idx]->pmib->dot11OperationEntry.opmode & WIFI_AP_STATE)) {
				if(root_priv->pvap_priv[idx]->pmib->miscEntry.func_off == 0) {
					*func_map |= (BIT0<<(idx+1));

					if(root_priv->pvap_priv[idx]->assoc_num)
						*func_map |= (BIT16<<(idx+1));
				}
			}
		}
	}
#endif

	return 0;
}

//return value: Auto site-survey level
//0(SS_LV_WSTA), STA connect to root AP/VAP
//1(SS_LV_WOSTA), No STA connected to root AP/VAP
//2(SS_LV_ROOTFUNCOFF), root AP only and func_off=1
int get_ss_level(struct rtl8192cd_priv *priv)
{
	int idx=0, ss_level=SS_LV_WSTA;
	unsigned int func_map=0;
	checkAPfunc(priv, &func_map);

	if(func_map & 0xffff0000)
		ss_level=SS_LV_WSTA;
	else if(func_map & 0xffff)
		ss_level=SS_LV_WOSTA;
	else
		ss_level=SS_LV_ROOTFUNCOFF;

	STADEBUG("[%s]checkAPfunc:%04x, ss_level:%d\n",priv->dev->name,func_map,ss_level);

	return ss_level;
}

#ifdef CONFIG_IEEE80211W_CLI 
void sync_11w_capability(struct rtl8192cd_priv *priv, struct bss_desc *bss_target)
{
	if((bss_target->t_stamp[1] & 0x600) == PMF_REQ) {
		log("peer's pmf[Required] AKMP=0x%x", priv->bss_support_akmp);
		priv->bss_support_pmf = REQ_PMF;
	}
	else if((bss_target->t_stamp[1] & 0x600) == PMF_CAP) {
		log("peer's pmf[cap], AKMP=0x%x", priv->bss_support_akmp);
		priv->bss_support_pmf = OPT_PMF;
	}
	else if((bss_target->t_stamp[1] & 0x600) == PMF_NONE) {
		log("peer's pmf[NONE], AKMP=0x%x", priv->bss_support_akmp);
		priv->bss_support_pmf = NO_PMF;
	}
}
#endif

/*
	set Multicast Cipher as same as AP's
*/ 
void syncMulticastCipher(struct rtl8192cd_priv *priv, struct bss_desc *bss_target)
{
	int mcipher = 1;

	/*WPA*/
	if (priv->pmib->dot11RsnIE.rsnie[0] == _RSN_IE_1_) {

		if(bss_target->t_stamp[0] & BIT(4))
			mcipher = 4;
		else if(bss_target->t_stamp[0] & BIT(2))
			mcipher = 2;
		if(priv->pmib->dot11RsnIE.rsnie[11]!=mcipher){
			log("WPA M cipher[%d] -> [%d]",priv->pmib->dot11RsnIE.rsnie[11],mcipher);
			priv->pmib->dot11RsnIE.rsnie[11] = mcipher;
		}

#if defined(INCLUDE_WPA_PSK) || defined(WIFI_HAPD) || defined(RTK_NL80211)
		priv->wpa_global_info->AuthInfoBuf[11] = mcipher;
#endif
	} 
	else if(priv->pmib->dot11RsnIE.rsnie[0] == _RSN_IE_2_) {

	/*WPA2*/
#if	0	//def CONFIG_IEEE80211W_CLI 	20190417 mark this section it rdy done at sync_11w_capability()
		if((bss_target->t_stamp[1] & 0x600) == PMF_REQ) {
			PMFDEBUG("AP PMF capability = Required, AKMP=0x%x \n", priv->bss_support_akmp);
			priv->bss_support_pmf = MGMT_FRAME_PROTECTION_REQUIRED;
		}
		else if((bss_target->t_stamp[1] & 0x600) == PMF_CAP) {
			PMFDEBUG("AP PMF capability = MFPC, AKMP=0x%x \n", priv->bss_support_akmp);
			priv->bss_support_pmf = MGMT_FRAME_PROTECTION_OPTIONAL;
		}
		else if((bss_target->t_stamp[1] & 0x600) == PMF_NONE) {
			PMFDEBUG("AP PMF capability = NONE, AKMP=0x%x\n", priv->bss_support_akmp);
			priv->bss_support_pmf = NO_MGMT_FRAME_PROTECTION;
		}		
		WPA_GLOBAL_INFO *pGblInfo=priv->wpa_global_info;
		log("=>ConstructIE");
		ConstructIE(priv, pGblInfo->AuthInfoElement.Octet,
					 &pGblInfo->AuthInfoElement.Length);
		ToDrv_SetIE(priv);
				
#endif

		if(bss_target->t_stamp[0] & BIT(20))
			mcipher = 4;
		else if(bss_target->t_stamp[0] & BIT(18))
			mcipher = 2;
		
		if(priv->pmib->dot11RsnIE.rsnie[7]!=mcipher){
			log("WPA2, Mcipher,[%d] -> [%d]",priv->pmib->dot11RsnIE.rsnie[7],mcipher);
			priv->pmib->dot11RsnIE.rsnie[7] = mcipher;
		}
		
#if defined(INCLUDE_WPA_PSK) || defined(WIFI_HAPD) || defined(RTK_NL80211)
		priv->wpa_global_info->AuthInfoBuf[7] = mcipher;		
#endif	

	}
}



#ifdef UNIVERSAL_REPEATER
#if defined(__OSK__) || defined (__ECOS) || defined(RTK_NL80211) || (defined(CONFIG_OPENWRT_SDK) && defined(__LINUX_3_10__))/*wrt-dhcp  //TBD //eric-sync ?? __LINUX_3_10__*/
int send_arp_response(struct rtl8192cd_priv *priv, unsigned int *dip, unsigned int *sip, unsigned char *dmac, unsigned char *smac)
{
	return -1;
}
#else
int send_arp_response(struct rtl8192cd_priv *priv, unsigned int *dip, unsigned int *sip, unsigned char *dmac, unsigned char *smac)
{
	unsigned int ret = -1;
	struct sk_buff *arp_skb = NULL;
	struct rtl_arphdr *arp=NULL;
	unsigned char *ptr;
	int hlen, tlen;

	hlen = LL_RESERVED_SPACE(priv->dev);
	tlen = priv->dev->needed_tailroom;
	arp_skb = __alloc_skb(arp_hdr_len(priv->dev) + hlen + tlen, GFP_ATOMIC, 0, -1);

	if (arp_skb == NULL)
		goto err_out;

	skb_reserve(arp_skb, LL_RESERVED_SPACE(priv->dev));
	skb_reset_network_header(arp_skb);
	arp = (struct rtl_arphdr *) skb_put(arp_skb, arp_hdr_len(priv->dev));
	arp_skb->dev = priv->dev;
	arp_skb->protocol = htons(ETH_P_ARP);
#if 0
	//without consideration of VLAN
//#ifdef CONFIG_RTK_VLAN_SUPPORT
	arp_skb->tag = arp_tag;
#if defined(CONFIG_RTK_BRIDGE_VLAN_SUPPORT)
	arp_skb->src_info = arp_info;
#endif
#endif
	if (dev_hard_header(arp_skb, priv->dev, ETH_P_ARP, dmac, smac, arp_skb->len) < 0) {
		kfree_skb(arp_skb);
		goto err_out;
	}
	// ======================
	// build new arp response
	// ======================
	//arp response
	ptr = (unsigned char *)(arp + 1);
	arp->ar_op = htons(ARPOP_REPLY);
	arp->ar_hrd = htons(priv->dev->type);
	arp->ar_pro = htons(ETH_P_IP);
	arp->ar_hln = priv->dev->addr_len;;
	arp->ar_pln = 4;	//for ipv4
	//sender mac and ip
	memcpy(ptr, smac, 6);
	ptr += arp->ar_hln;
	memcpy(ptr, sip, 4);
	//printk("sender mac and ip:%02x:%02x:%02x:%02x:%02x:%02x,%d.%d.%d.%d\n",pstat->cmn_info.mac_addr[0],pstat->cmn_info.mac_addr[1],pstat->cmn_info.mac_addr[2],pstat->cmn_info.mac_addr[3],pstat->cmn_info.mac_addr[4],pstat->cmn_info.mac_addr[5],pstat->sta_ip[0],pstat->sta_ip[1],pstat->sta_ip[2],pstat->sta_ip[3]);
	//target mac and ip
	ptr += arp->ar_pln;
	memcpy(ptr, dmac, MACADDRLEN);
	ptr += arp->ar_hln;
	memcpy(ptr, dip, 4);
	//printk("target mac and ip:%02x:%02x:%02x:%02x:%02x:%02x,%d.%d.%d.%d\n",newskb->data[0],newskb->data[1],newskb->data[2],newskb->data[3],newskb->data[4],newskb->data[5],ptarget[0],ptarget[1],ptarget[2],ptarget[3]);

#ifdef MULTI_MAC_CLONE
	*(unsigned int *)&(arp_skb->cb[40]) = 0x86518192;
#endif

	arp_skb->cb[2] = (char)0xff;         // not do aggregation
	memcpy(arp_skb->cb+10,arp_skb->data,6);
	SMP_LOCK_XMIT(x);
	if(__rtl8192cd_start_xmit(arp_skb, priv->dev, 1)) {
		DEBUG_ERR("%s %d ARP response sent failed\n",__func__,__LINE__);
	} else {
		int i=0;
		ret = 0;
		DEBUG_INFO("%s %d ARP response for %02x:%02x:%02x:%02x:%02x:%02x to %s sent\n",__func__,__LINE__,
			smac[0],smac[1],smac[2],smac[3],smac[4],smac[5],priv->dev->name);
		for(i=0;i<arp_skb->len;i++){
			DEBUG_INFO("%02x",arp_skb->data[i]);
			if(i/16 && ((i%16) == 0))
				DEBUG_INFO("\n");
		}
		DEBUG_INFO("\n");
	}
	SMP_UNLOCK_XMIT(x);

	return ret;

err_out:
	return ret;
}
#endif

void snoop_STA_IP(struct sk_buff *pskb, struct rtl8192cd_priv *priv)
{
	#define DHCP_MAGIC 0x63825363
#ifdef MBSSID
	int i;
#endif
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
	struct rtl8192cd_priv *ap_priv;
	struct iphdr* iph;
	struct udphdr *udph;
	struct dhcpMessage *dhcph;
	
	iph = (struct iphdr *)(pskb->data + ETH_HLEN);
	udph = (struct udphdr *)((unsigned long)iph + (iph->ihl << 2));
	dhcph = (struct dhcpMessage *)((unsigned long)udph + sizeof(struct udphdr));

	if(IS_VXD_INTERFACE(priv)) {
        ap_priv = GET_ROOT(priv);
    } else {
		DEBUG_INFO("Receive DHCP response but interface is not VXD\n");
        return ;
	}

	//dhcp reply only
	if(ap_priv && IS_DRV_OPEN(ap_priv) && (dhcph->op == 2)) {
        unsigned char sta_ip[4];

		memcpy(sta_ip,&dhcph->yiaddr,4);
		DEBUG_INFO("[%s]External DHCP Server give IP[%d.%d.%d.%d]\n",priv->dev->name,sta_ip[0],sta_ip[1],sta_ip[2],sta_ip[3]);
#ifdef MBSSID
		unsigned char vap_asso_sta = 0;
		if (GET_ROOT(priv)->pmib->miscEntry.vap_enable) {
			for (i=0; i<RTL8192CD_NUM_VWLAN; i++) {
				if (IS_DRV_OPEN(ap_priv->pvap_priv[i]) && get_stainfo(ap_priv->pvap_priv[i],&dhcph->chaddr[0]))
					vap_asso_sta ++;
			}
		}
#endif
		if((get_stainfo(ap_priv, &dhcph->chaddr[0]) 
#ifdef MBSSID
			|| vap_asso_sta > 0
#endif
			) && send_arp_response(priv,&dhcph->siaddr,&dhcph->yiaddr,pskb->data+MACADDRLEN,&dhcph->chaddr[0]) )
			DEBUG_ERR("Send ARP failed\n");

		return;
	}
}

int changePreamble(struct rtl8192cd_priv *priv, int preamble)
{
	unsigned char *p = (unsigned char *)priv->beaconbuf;
	unsigned short *bcn_cap = (unsigned short *)(p+BEACON_MACHDR_LEN+_TIMESTAMP_+_BEACON_ITERVAL_);

	if(preamble)
		*bcn_cap |= cpu_to_le16(BIT(5));
	else
		*bcn_cap &= ~(cpu_to_le16(BIT(5)));

	return 0;
}

//priv must be root interface's priv
int takeOverHidden(unsigned char active, struct rtl8192cd_priv *priv)
{
	if((OPMODE & WIFI_AP_STATE) && IS_DRV_OPEN(priv)) {
		if(priv->take_over_hidden != active) {
			if(active) {
				HideAP(priv);
				STADEBUG("Hidden take over!!\n");
			} else {
				DehideAP(priv);
				STADEBUG("Hidden take over end!!\n");
			}
		} else {
			//STADEBUG("Keep take_over_hidden:%d!!\n",priv->take_over_hidden);
		}

	    priv->take_over_hidden = active;
	} else {
		//do nothing exclude AP mode
	}

    return 0;
}

int zeroByteLength(unsigned char *p, int len)
{
	int ret=0;

	for(ret=0;ret<len;ret++) {
		if(p[ret]!=0)
            break;
	}

	return ret;
}

int isHiddenAP(unsigned char *pframe, struct rx_frinfo *pfrinfo, struct stat_info *pstat, struct rtl8192cd_priv *priv)
{
	unsigned int len=0, ret=0;
	unsigned char *p = get_ie(pframe + WLAN_HDR_A3_LEN + _BEACON_IE_OFFSET_, _SSID_IE_, &len, pfrinfo->pktlen - WLAN_HDR_A3_LEN - _BEACON_IE_OFFSET_);

	if(p) {
		if((len && (len==zeroByteLength(p+2,len))) || (len==0)) {
#ifdef MBSSID
			takeOverHidden(1,GET_VAP_PRIV(priv,CASE6_VAP_INDEX));
#endif
			ret = 1;
		} else {
#ifdef MBSSID
			takeOverHidden(0,GET_VAP_PRIV(priv,CASE6_VAP_INDEX));
#endif
		}
	} else {
		ret = 1;
		STADEBUG("SSID IE is not included, treat as hidden!!\n");
#ifdef MBSSID
		takeOverHidden(1,GET_VAP_PRIV(priv,CASE6_VAP_INDEX));
#endif
	}

    return ret;
}
#endif	//UNIVERSAL_REPEATER

#if defined(UNIVERSAL_REPEATER) || defined(STA_CONTROL)
int HideAP(struct rtl8192cd_priv *priv)
{
	unsigned char *p = (unsigned char *)priv->beaconbuf;

	memset(p + BEACON_MACHDR_LEN + _TIMESTAMP_ + _BEACON_ITERVAL_ + _CAPABILITY_ + 2, 0, SSID_LEN);

	return 0;
}

int DehideAP(struct rtl8192cd_priv *priv)
{
	unsigned char *p = (unsigned char *)priv->beaconbuf;

	memcpy(p + BEACON_MACHDR_LEN + _TIMESTAMP_ + _BEACON_ITERVAL_ + _CAPABILITY_ + 2, SSID, SSID_LEN);
	*(p + BEACON_MACHDR_LEN + _TIMESTAMP_ + _BEACON_ITERVAL_ + _CAPABILITY_ + 1) = SSID_LEN;

	return 0;
}
#endif

unsigned int priv_max_tp(struct rtl8192cd_priv *priv) {
	unsigned int max_tp;
	int          rf_mimo_mode = get_rf_mimo_mode(priv);

	if(priv->pmib->dot11BssType.net_work_type & WIRELESS_11AC) {
		if(priv->pshare->CurrentChannelBW == CHANNEL_WIDTH_80)
			max_tp = 433;	//80M NSS1 MCS9 
		else if (priv->pshare->CurrentChannelBW == CHANNEL_WIDTH_40)
			max_tp = 200;	//40M NSS1 MCS9
		else
			max_tp = 87;	//20M NSS1 MCS8
	}
	else {
		if(priv->pshare->CurrentChannelBW == CHANNEL_WIDTH_40)
			max_tp = 150;	//40M MCS7
		else 
			max_tp = 72;	//20M MCS7
	}
	
	if(rf_mimo_mode == RF_4T4R)
		max_tp = 4 * max_tp;
	else if(rf_mimo_mode == RF_3T3R)
		max_tp = 3 * max_tp;
	else if(rf_mimo_mode == RF_2T2R)	
		max_tp = 2 * max_tp;
	else
		max_tp = max_tp;

	return max_tp;
}

/* Do defered channel scan when,
   1.There is not any station connected to active AP interafce(Root AP/VAP)
   2.Scan is not requested by wscd
   3.Scan is not requested by Station mode without any VAP
*/
int should_defer_ss(struct rtl8192cd_priv *priv)
{
	int ret=0;

	if(GET_ROOT(priv)->pmib->dot11OperationEntry.opmode & (WIFI_STATION_STATE|WIFI_ADHOC_STATE)) {
		ret = 0;
	} else {
		if(get_ss_level(priv) < SS_LV_ROOTFUNCOFF) {
			if((priv->site_survey->ss_channel == 100) && (priv->pmib->miscEntry.ss_delay)) {
				ret=1;
				DEBUG_INFO("%s Sitesurvey defered\n",priv->dev->name);
			}
		}
	}

	if(priv->ss_req_ongoing == SSFROM_WSC)
		ret = 0;

	return ret;
}


#ifdef USE_OUT_SRC
unsigned char *Get_Adaptivity_Version(void)
{
	return ADAPTIVITY_VERSION;
}
#endif

/*cfg p2p*/
#ifdef P2P_SUPPORT
void copy_p2p_ie(struct rtl8192cd_priv *priv, unsigned char *p2p_ie_input, unsigned char mgmt_type)
{
    /*T(1) L(1) V , wps_ie[1]==L => LEN = value of (L) + sizeof(T)+sizeof(L), cy wang note*/ 
	int p2p_ie_len = (p2p_ie_input[1] + 2);

	if(p2p_ie_len > 256)
	{
		NDEBUG("P2P_IE length > 256 !! Can NOT copy !!\n");
		return;
	}

	if (mgmt_type == MGMT_BEACON) { /* Beacon */
		NDEBUG2("P2P:Set Beacon IE[%d]\n",p2p_ie_len);
		priv->p2pPtr->p2p_beacon_ie_len = p2p_ie_len;        
		memcpy((void *)priv->p2pPtr->p2p_beacon_ie, p2p_ie_input, p2p_ie_len);
	}
	else if (mgmt_type == MGMT_PROBERSP) {  /* Probe Rsp */
		NDEBUG2("P2P:Set Probe Rsp IE[%d]\n",p2p_ie_len);
		priv->p2pPtr->p2p_probe_rsp_ie_len= p2p_ie_len;
		memcpy((void *)priv->p2pPtr->p2p_probe_rsp_ie, p2p_ie_input, p2p_ie_len);        
	}
	else if (mgmt_type == MGMT_PROBEREQ) {  /* Probe Req */
		NDEBUG2("P2P:Set Probe Req IE[%d]\n",p2p_ie_len);
		priv->p2pPtr->p2p_probe_req_ie_len = p2p_ie_len;
		memcpy((void *)priv->p2pPtr->p2p_probe_req_ie, p2p_ie_input, p2p_ie_len);        
	}    
	else if (mgmt_type == MGMT_ASSOCRSP ) { /*Assoc Rsp*/
		NDEBUG2("P2P:Set Assoc Rsp IE[%d]\n",p2p_ie_len);
        priv->p2pPtr->p2p_assoc_RspIe_len = p2p_ie_len;        
		memcpy((void *)priv->p2pPtr->p2p_assoc_RspIe, p2p_ie_input, p2p_ie_len);
	}
	else if (mgmt_type == MGMT_ASSOCREQ) { /*Assoc Req*/	
		NDEBUG2("P2P:Set Assoc Req IE[%d]\n",p2p_ie_len);        
		priv->p2pPtr->p2p_assocReq_ie_len = p2p_ie_len;                
        memcpy((void *)priv->p2pPtr->p2p_assocReq_ie, p2p_ie_input, p2p_ie_len);        
	}else{
		NDEBUG2("P2P: rtk set IE unknown type!!\n");
    }
    
}
#endif

/*cfg p2p*/
void copy_wps_ie(struct rtl8192cd_priv *priv, unsigned char *wps_ie, unsigned char mgmt_type)
{

    /*T(1) L(1) V , wps_ie[1]==L => LEN = value of (L) + sizeof(T)+sizeof(L), cy wang note*/ 
	unsigned int wps_ie_len = (wps_ie[1] + 2);  

	if(wps_ie_len > 256){
		NDEBUG2("WPS_IE length > 256 !! Can NOT copy !!\n");
		return;
	}

	if(OPMODE & WIFI_AP_STATE)
		priv->pmib->wscEntry.wsc_enable = 2; //Enable WPS for AP mode
	else if(OPMODE & WIFI_STATION_STATE)
		priv->pmib->wscEntry.wsc_enable = 1;

	if (mgmt_type == MGMT_BEACON) {
		NDEBUG2("WSC:Set Beacon IE[%d]\n",wps_ie_len);
		priv->pmib->wscEntry.beacon_ielen = wps_ie_len;
		memcpy((void *)priv->pmib->wscEntry.beacon_ie, wps_ie, wps_ie_len);
	}
	else if (mgmt_type == MGMT_PROBERSP) {
		NDEBUG2("WSC:Set Probe Rsp IE[%d]\n",wps_ie_len);
		priv->pmib->wscEntry.probe_rsp_ielen = wps_ie_len;
		memcpy((void *)priv->pmib->wscEntry.probe_rsp_ie, wps_ie, wps_ie_len);
	}
	else if (mgmt_type == MGMT_PROBEREQ) {
		NDEBUG2("WSC:Set Probe Req IE[%d]\n",wps_ie_len);
		priv->pmib->wscEntry.probe_req_ielen= wps_ie_len;
		memcpy((void *)priv->pmib->wscEntry.probe_req_ie, wps_ie, wps_ie_len);
	}    
	else if ((mgmt_type == MGMT_ASSOCRSP) || (mgmt_type == MGMT_ASSOCREQ)) { //wrt-wps-clnt
		NDEBUG2("WSC:Set Assoc IE[%d]\n",wps_ie_len);
		priv->pmib->wscEntry.assoc_ielen = wps_ie_len;
		memcpy((void *)priv->pmib->wscEntry.assoc_ie, wps_ie, wps_ie_len);
	}
}

/**
 * rtk_get_wps_ie - Search WPS IE from a series of IEs
 * @in_ie: Address of IEs to search
 * @in_len: Length limit from in_ie
 * @wps_ie: If not NULL and WPS IE is found, WPS IE will be copied to the buf starting from wps_ie
 * @wps_ielen: If not NULL and WPS IE is found, will set to the length of the entire WPS IE
 *
 * Returns: The address of the WPS IE found, or NULL
 */

u8* rtk_get_wps_ie(u8 *in_ie, int in_len, u8 *wps_ie, int *wps_ielen)
{
	int cnt=0;
	u8 eid=0;
	u8 *wpsie_ptr=NULL;    

    u8 WSC_OUI[4] = {0x00, 0x50, 0xf2, 0x04};


	if(wps_ielen){
		*wps_ielen = 0;
    }

	if(!in_ie || in_len<=0)
		return wpsie_ptr;

	while(cnt<in_len)
	{
		eid = in_ie[cnt];
		if((eid==_VENDOR_SPECIFIC_IE_)&&( !memcmp(&in_ie[cnt+2], WSC_OUI, 4)))
		{
			wpsie_ptr = &in_ie[cnt];
			if(wps_ie){
				memcpy(wps_ie, &in_ie[cnt], in_ie[cnt+1]+2);
            }
			
			if(wps_ielen){
				*wps_ielen = in_ie[cnt+1]+2;
             }
			
			cnt+=in_ie[cnt+1]+2;

			break;
		}
		else
		{
			cnt+=in_ie[cnt+1]+2; //goto next	
		}		

	}	

	return wpsie_ptr;
}

/**
 * rtk_get_p2p_ie - Search P2P IE from a series of IEs
 * @in_ie: Address of IEs to search
 * @in_len: Length limit from in_ie
 * @p2p_ie: If not NULL and P2P IE is found, P2P IE will be copied to the buf starting from p2p_ie
 * @p2p_ielen: If not NULL and P2P IE is found, will set to the length of the entire P2P IE
 *
 * Returns: The address of the P2P IE found, or NULL
 */
/*cfg p2p*/
unsigned char*  rtk_get_p2p_ie(unsigned char*  in_ie, int in_len, unsigned char*  p2p_ie, int* p2p_ielen)
{
	int cnt=0;
	unsigned char* p2p_ptr=NULL;
	unsigned char eid=0;
    unsigned char P2P_OUI[4]={0x50,0x6F,0x9A,0x09};

	if(p2p_ielen){
		*p2p_ielen = 0;
    }

	if(!in_ie || in_len<=0)
		return p2p_ptr;

	while(cnt<in_len)
	{
		eid = in_ie[cnt];
		if((eid==_VENDOR_SPECIFIC_IE_)&&( !memcmp(&in_ie[cnt+2], P2P_OUI, 4)))
		{
			p2p_ptr = &in_ie[cnt];
			if(p2p_ie){
				memcpy(p2p_ie, &in_ie[cnt], in_ie[cnt+1]+2);
            }
			
			if(p2p_ielen){
				*p2p_ielen = in_ie[cnt+1]+2;
             }
			
			cnt+=in_ie[cnt+1]+2;
			break;
		}
        else
		{
			cnt+=in_ie[cnt+1]+2; //search next	
		}		

	}	

	return p2p_ptr;
}


/*cfg p2p*/
int rtk_get_wfd_ie(u8 *in_ie, int in_len, u8 *wfd_ie, int *wfd_ielen)
{
	int match;
	int cnt = 0;	
	u8 eid;
    unsigned char WFD_OUI[4]={0x50,0x6F,0x9A,0x0A};


	match=0;

	if ( in_len < 0 )
	{
		return match;
	}

	while(cnt<in_len)
	{
		eid = in_ie[cnt];
		
		if( ( eid == _VENDOR_SPECIFIC_IE_ ) && ( !memcmp( &in_ie[cnt+2], WFD_OUI, 4)) )
		{
			if ( wfd_ie != NULL ){
				memcpy( wfd_ie, &in_ie[ cnt ], in_ie[ cnt + 1 ] + 2 );			
			}
			else
			{
				if ( wfd_ielen != NULL ){
					*wfd_ielen = 0;
				}
			}
			
			if ( wfd_ielen != NULL ){
				*wfd_ielen = in_ie[ cnt + 1 ] + 2;
			}
			
			cnt += in_ie[ cnt + 1 ] + 2;

			match = 1;
			break;
		}
		else
		{
			cnt += in_ie[ cnt + 1 ] +2; //search next	
		}		
		
	}	

	if ( match == 1 )
	{
		match = cnt;
	}
	
	return match;

}
/*cfg p2p*/
/**
 * rtk_get_p2p_ie - Search P2P IE from a series of IEs
 * @in_ie: Address of IEs to search
 * @in_len: Length limit from in_ie
 * @p2p_ie: If not NULL and P2P IE is found, P2P IE will be copied to the buf starting from p2p_ie
 * @p2p_ielen: If not NULL and P2P IE is found, will set to the length of the entire P2P IE
 *
 * Returns: The address of the P2P IE found, or NULL
 */

u8* rtk_get_ie_with_oui(u8 *in_ie, int in_len, u8 ie_to_search, u8* oui_input ,int oui_len, int *report_ielen)
{
	int cnt=0;
	u8 *tar_ie_ptr=NULL;
	u8 eid=0;

	if(report_ielen){
		*report_ielen = 0;
    }

	if(!in_ie || in_len<=0)
		return tar_ie_ptr;

	while(cnt<in_len)
	{
		eid = in_ie[cnt];
		if((eid==ie_to_search)&&( !memcmp(&in_ie[cnt+2], oui_input,oui_len)))
		{
			tar_ie_ptr = (u8 *)&in_ie[cnt];

			
			if(report_ielen){
				*report_ielen = in_ie[cnt+1]+2;
             }
			
			cnt+=in_ie[cnt+1]+2;
			break;
		}
        else
		{
			cnt+=in_ie[cnt+1]+2; //search next	
		}		

	}	

	return tar_ie_ptr;
}
/*cfg p2p*/


#ifdef STA_ASSOC_STATISTIC
struct tm *get_current_timeInfo ( struct tm * const	tmp, int tz)
{
	extern void time_to_tm(time_t totalsecs, int offset, struct tm *result);
	struct timeval	 time_stamp;
	const time_t * const	timep = &time_stamp;
	static int padding_digit=0;
	static struct timeval time_stamp_prev;	
	do_gettimeofday(&time_stamp);
	if(time_stamp.tv_sec == time_stamp_prev.tv_sec && time_stamp.tv_usec == time_stamp_prev.tv_usec) {
		if(padding_digit<9)
			padding_digit++;
	} else {
		padding_digit=0;
		memcpy(&time_stamp_prev, &time_stamp, sizeof(struct timeval));
	}	
	memset(tmp, 0, sizeof(struct tm));
	time_to_tm(*timep, SECSPERHOUR *tz, tmp);
	tmp->tm_yday = time_stamp.tv_usec/1000 + padding_digit;
	return tmp;
}
#endif

#if defined(CONFIG_RTL_SIMPLE_CONFIG)
int rtk_sc_start_simple_config(struct rtl8192cd_priv *priv)
{
	int i=0;
	unsigned char null_mac[]={0,0,0,0,0,0};
	
#if defined(__ECOS)	
#ifdef UNIVERSAL_REPEATER	
	if(IS_VXD_INTERFACE(priv))
		sprintf(g_sc_ifname, "%s0", priv->dev->name);
	else
#endif
#endif
	strcpy(g_sc_ifname, priv->dev->name);

	rtk_sc_set_para(priv);
	if(priv->pmib->dot11StationConfigEntry.sc_debug)
		panic_printk("Start simple config now!\n");
	rtk_sc_clean_profile_value();
	
#if 0 //eric-sc
{
	if(priv->pmib->dot11StationConfigEntry.sc_debug)
	        panic_printk("\nFORCE ENABLE LDPC FOR SIMPLE CONFIG!!\n\n");
	RTL_W8(0x913, RTL_R8(0x913)|0x02);
}
#endif
	priv->simple_config_could_fix = 0;

	rtk_sc_set_value(SC_FIX_CHANNEL, 0);
	
#ifdef UNIVERSAL_REPEATER	
	if(IS_VXD_INTERFACE(priv))
	{
		if(priv->simple_config_status == 0)
			memcpy(priv->pmib->dot11Bss.bssid, null_mac, MACADDRLEN);
	}
#endif

	if(priv->simple_config_status == 0)
		priv->simple_config_status = 1;

	priv->pmib->dot11StationConfigEntry.sc_status = 1;
	priv->pmib->dot11StationConfigEntry.sc_vxd_rescan_time = CHECK_VXD_SC_TIMEOUT;
	priv->pmib->dot11StationConfigEntry.sc_fix_encrypt = 0;
	
	RTL_W32(RCR, RTL_R32(RCR) | RCR_AAP);	//Accept Destination Address packets.
	return 0;
}

int rtk_sc_start_connect_target(struct rtl8192cd_priv *priv)
{
	int i=0;
	//if(priv->pmib->dot11StationConfigEntry.sc_fix_channel >= 0)
	//	priv->pmib->dot11StationConfigEntry.sc_fix_channel = 0;

	if(priv->pmib->dot11StationConfigEntry.sc_reset_beacon_psk == 1)
	{
#if defined(INCLUDE_WPA_PSK) || defined(WIFI_HAPD)
		if (priv->pmib->dot1180211AuthEntry.dot11EnablePSK) {
#if defined(CONFIG_RTL_8197F) || defined(CONFIG_RTL_8197G) /*eric-dog */
			watchdog_kick(); 
#else
			watchdog_stop(priv);
#endif
			psk_init(priv);
			syncMulticastCipher(priv,&priv->pmib->dot11Bss);	
#if defined(CONFIG_RTL_8197F) || defined(CONFIG_RTL_8197G) /*eric-dog */
			watchdog_kick(); 
#else
			watchdog_resume(priv);
#endif

		} 
#endif
		if( (priv->pmib->dot11StationConfigEntry.sc_sync_vxd_to_root == 1) && IS_VXD_INTERFACE(priv))
		{
#if defined(CONFIG_RTL_8197F) || defined(CONFIG_RTL_8197G) /*eric-dog */
			watchdog_kick(); 
#else
			watchdog_stop(GET_ROOT(priv));
#endif
			if (GET_ROOT(priv)->pmib->dot1180211AuthEntry.dot11EnablePSK) {
				psk_init(GET_ROOT(priv));
				syncMulticastCipher(priv,&priv->pmib->dot11Bss);
				
			} 
			init_beacon(GET_ROOT(priv));
#if defined(CONFIG_RTL_8197F) || defined(CONFIG_RTL_8197G)  /*eric-dog */
			watchdog_kick(); 
#else
			watchdog_resume(GET_ROOT(priv));
#endif
		}
	}
	else
	{
#ifdef UNIVERSAL_REPEATER		
	if(IS_VXD_INTERFACE(priv))
	{
#ifdef MBSSID
		if (GET_ROOT(priv)->pmib->miscEntry.vap_enable) {
			for (i=0; i<RTL8192CD_NUM_VWLAN; i++) {
				if (IS_DRV_OPEN(priv->pvap_priv[i]))
					priv->pvap_priv[i]->pmib->dot11OperationEntry.keep_rsnie = 1;
			}
		}
#endif
	}
#endif
	RTL_W32(RCR, RTL_R32(RCR) & ~RCR_AAP);	// Disable Accept Destination Address packets.
#ifdef CONFIG_RTL_8198C
	/* rtl8192cd_ss_timer-->start_clnt_lookup-->rtk_sc_check_security-->rtk_sc_start_connect_target
	 * ss_timer call SMP_LOCK, so we should call unlock before close interface	
	 */
	unsigned long flags;
	SMP_UNLOCK(flags);
#endif
		if(priv->pmib->dot11StationConfigEntry.sc_fix_channel > 0)
			priv->pmib->dot11StationConfigEntry.sc_fix_channel = 0;
	RTL_W8(TXPAUSE, 0xff);
	rtl8192cd_close(priv->dev);
#ifdef UNIVERSAL_REPEATER	
	if( (priv->pmib->dot11StationConfigEntry.sc_sync_vxd_to_root == 1) && IS_VXD_INTERFACE(priv))
	{
		GET_ROOT(priv)->pmib->dot11StationConfigEntry.sc_enabled = 0;
		rtl8192cd_close(GET_ROOT(priv)->dev);
		rtl8192cd_open(GET_ROOT(priv)->dev);
		while(!netif_running(GET_ROOT(priv)->dev))
			delay_ms(10);
	}
#endif
	rtl8192cd_open(priv->dev);
	RTL_W8(TXPAUSE, 0x00);
#ifdef CONFIG_RTL_8198C
	SMP_LOCK(flags);
#endif
	}
	
	rtk_sc_set_value(SC_PRIV_STATUS, 5);
	rtk_sc_set_value(SC_STATUS, 5);

	if(priv->pmib->dot11StationConfigEntry.sc_reset_beacon_psk == 1)
	{
	
		extern void start_clnt_auth(struct rtl8192cd_priv *priv);
	
		//if(IS_VXD_INTERFACE(priv))
			start_clnt_auth(priv);
		//else
		//	start_clnt_join(priv);
	}

	return 0;
}
#ifdef UNIVERSAL_REPEATER	
int rtk_sc_sync_vxd_to_root(struct rtl8192cd_priv * priv)
{
	if(IS_VXD_INTERFACE(priv))
	{
		if(priv->pmib->dot11StationConfigEntry.sc_sync_vxd_to_root == 1)
		{
			GET_ROOT(priv)->pmib->dot1180211AuthEntry.dot11AuthAlgrthm = priv->pmib->dot1180211AuthEntry.dot11AuthAlgrthm;
			GET_ROOT(priv)->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm = priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm;
			GET_ROOT(priv)->pmib->dot1180211AuthEntry.dot11EnablePSK = priv->pmib->dot1180211AuthEntry.dot11EnablePSK;
			GET_ROOT(priv)->pmib->dot1180211AuthEntry.dot11PrivacyKeyIndex = priv->pmib->dot1180211AuthEntry.dot11PrivacyKeyIndex;
			GET_ROOT(priv)->pmib->dot118021xAuthEntry.dot118021xAlgrthm = priv->pmib->dot118021xAuthEntry.dot118021xAlgrthm;
			GET_ROOT(priv)->pmib->dot1180211AuthEntry.dot11WPA2Cipher = priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher;
			GET_ROOT(priv)->pmib->dot1180211AuthEntry.dot11WPACipher = priv->pmib->dot1180211AuthEntry.dot11WPACipher;
			strcpy(GET_ROOT(priv)->pmib->dot1180211AuthEntry.dot11PassPhrase, priv->pmib->dot1180211AuthEntry.dot11PassPhrase);
			strcpy(&(GET_ROOT(priv)->pmib->dot11DefaultKeysTable.keytype[0]), &(priv->pmib->dot11DefaultKeysTable.keytype[0]));
			memcpy(GET_ROOT(priv)->pmib->dot11StationConfigEntry.dot11DesiredSSID, priv->pmib->dot11StationConfigEntry.dot11DesiredSSID, priv->pmib->dot11StationConfigEntry.dot11DesiredSSIDLen);
			memcpy(GET_ROOT(priv)->pmib->dot11StationConfigEntry.dot11SSIDtoScan, priv->pmib->dot11StationConfigEntry.dot11SSIDtoScan, priv->pmib->dot11StationConfigEntry.dot11SSIDtoScanLen);
			GET_ROOT(priv)->pmib->dot11StationConfigEntry.dot11DesiredSSIDLen = priv->pmib->dot11StationConfigEntry.dot11DesiredSSIDLen;
			GET_ROOT(priv)->pmib->dot11StationConfigEntry.dot11SSIDtoScanLen = priv->pmib->dot11StationConfigEntry.dot11SSIDtoScanLen;
		}
	}

	return 0;
}
#else
int rtk_sc_sync_vxd_to_root(struct rtl8192cd_priv * priv)
{
	return 0;
}

#endif

int rtk_sc_stop_simple_config(struct rtl8192cd_priv *priv)
{
	int i=0, ack_num=0;
	int sc_security_type = priv->pmib->dot11StationConfigEntry.sc_security_type;

	if (OPMODE & WIFI_ASOC_STATE)
	{
		rtk_sc_set_value(SC_ENABLED, 0);
		rtk_sc_set_value(SC_PRIV_STATUS, 0);
		rtk_sc_set_value(SC_STATUS, 10+sc_security_type);
	}
	else
		rtk_sc_set_value(SC_STATUS, -1);

	rtk_sc_set_value(SC_FIX_CHANNEL, 0);
	return 0;
}

struct net_device * rtl_get_dev_by_wlan_name(char *name)
{
#if defined(__ECOS)
	return rtl_getDevByName(name);
#else
	return __dev_get_by_name(&init_net, name);
#endif
}

int rtk_sc_set_value(unsigned int id, unsigned int value)
{
	struct net_device *dev;
	dev=rtl_get_dev_by_wlan_name(g_sc_ifname);
	struct rtl8192cd_priv *priv = GET_DEV_PRIV(dev);


	switch(id)
	{
		case SC_ENABLED:
			priv->pmib->dot11StationConfigEntry.sc_enabled = value;
			break;
		case SC_DURATION_TIME:
			priv->pmib->dot11StationConfigEntry.sc_duration_time = value;
			break;
		case SC_GET_SYNC_TIME:
			priv->pmib->dot11StationConfigEntry.sc_get_sync_time = value;
			break;
		case SC_GET_PROFILE_TIME:
			priv->pmib->dot11StationConfigEntry.sc_get_profile_time = value;
			break;
		case SC_VXD_RESCAN_TIME:
			priv->pmib->dot11StationConfigEntry.sc_vxd_rescan_time = value;
			break;
		case SC_PIN_ENABLED:
			priv->pmib->dot11StationConfigEntry.sc_pin_enabled = value;
			break;
		case SC_STATUS:
			priv->pmib->dot11StationConfigEntry.sc_status = value;
			break;
		case SC_DEBUG:
			priv->pmib->dot11StationConfigEntry.sc_debug = value;
			break;	
		case SC_CHECK_LINK_TIME:
			priv->pmib->dot11StationConfigEntry.sc_check_link_time = value;
			break;			
		case SC_SYNC_VXD_TO_ROOT:
			priv->pmib->dot11StationConfigEntry.sc_sync_vxd_to_root = value;
			break;			
		case SC_ACK_ROUND:
			priv->pmib->dot11StationConfigEntry.sc_ack_round = value;
			break;			
		case SC_CONTROL_IP:
			priv->pmib->dot11StationConfigEntry.sc_control_ip = value;
			break;	
		case SC_PRIV_STATUS:
			priv->simple_config_status = value;
			break;
		case SC_CONFIG_TIME:
			priv->simple_config_time = value;
			break;
		case SC_FIX_CHANNEL:
			if(priv->pmib->dot11StationConfigEntry.sc_fix_channel >= 0){
				if ( (value > 0) && priv->simple_config_could_fix)
					priv->pmib->dot11StationConfigEntry.sc_fix_channel = value;
				else 
			priv->pmib->dot11StationConfigEntry.sc_fix_channel = value;
			}
			break;
		case SC_FROM_TO_DS:
			if(value == 2)
			{
				//if(priv->pmib->dot11StationConfigEntry.sc_debug)
				//	panic_printk("receive fromDS is 0, toDS is 1 packet\n");
				//priv->pmib->dot11StationConfigEntry.sc_get_sync_time = 120;
			}
			else if(value == 1)
			{
				//if(priv->pmib->dot11StationConfigEntry.sc_debug)
				//	panic_printk("receive fromDS is 1, toDS is 0 packet\n");
			}
#if 0
			if(priv->site_survey->ss_channel > 14 && priv->pmib->dot11StationConfigEntry.sc_fix_channel>=0)
			{
				priv->pmib->dot11StationConfigEntry.sc_fix_channel = phy_query_rf_reg(priv,RF_PATH_A,0x18,0xff,1);
				if(priv->pmib->dot11StationConfigEntry.sc_debug)
					panic_printk("Simple Config 5G switch to remote AP channel(center) %d\n", priv->pmib->dot11StationConfigEntry.sc_fix_channel );
			}
#endif
			break;
		case SC_FIX_ENCRYPT:
			priv->pmib->dot11StationConfigEntry.sc_fix_encrypt = value;
			break;
		default:
			break;
	}
	return 0;
}

int rtk_sc_get_value(unsigned int id)
{
	struct net_device *dev;
	int value=0;
	dev=rtl_get_dev_by_wlan_name(g_sc_ifname);
	struct rtl8192cd_priv *priv = GET_DEV_PRIV(dev);


	switch(id)
	{
		case SC_ENABLED:
			value = priv->pmib->dot11StationConfigEntry.sc_enabled;
			break;
		case SC_DURATION_TIME:
			value = priv->pmib->dot11StationConfigEntry.sc_duration_time;
			break;
		case SC_GET_SYNC_TIME:
			value = priv->pmib->dot11StationConfigEntry.sc_get_sync_time;
			break;
		case SC_GET_PROFILE_TIME:
			value = priv->pmib->dot11StationConfigEntry.sc_get_profile_time;
			break;
		case SC_VXD_RESCAN_TIME:
			value = priv->pmib->dot11StationConfigEntry.sc_vxd_rescan_time;
			break;
		case SC_PIN_ENABLED:
			value = priv->pmib->dot11StationConfigEntry.sc_pin_enabled;
			break;
		case SC_STATUS:
			value = priv->pmib->dot11StationConfigEntry.sc_status;
			break;
		case SC_DEBUG:
			value = priv->pmib->dot11StationConfigEntry.sc_debug;
			break;	
		case SC_CHECK_LINK_TIME:
			value = priv->pmib->dot11StationConfigEntry.sc_check_link_time;
			break;			
		case SC_SYNC_VXD_TO_ROOT:
			value = priv->pmib->dot11StationConfigEntry.sc_sync_vxd_to_root;
			break;			
		case SC_ACK_ROUND:
			value = priv->pmib->dot11StationConfigEntry.sc_ack_round;
			break;			
		case SC_CONTROL_IP:
			value = priv->pmib->dot11StationConfigEntry.sc_control_ip;
			break;	
		case SC_PRIV_STATUS:
			value = priv->simple_config_status;
			break;
		case SC_CONFIG_TIME:
			value = priv->simple_config_time;
			break;
		case SC_CHECK_LEVEL:
			value = priv->pmib->dot11StationConfigEntry.sc_check_level;
			break;	
		case SC_IGNORE_OVERLAP:
			value = priv->pmib->dot11StationConfigEntry.sc_ignore_overlap;
			break;	
		case SC_FIX_CHANNEL:
			value = priv->pmib->dot11StationConfigEntry.sc_fix_channel;
			break;
		case SC_FIX_ENCRYPT:
			value = priv->pmib->dot11StationConfigEntry.sc_fix_encrypt;
			break;
		case SC_CONFIG_TYPE:
			value = priv->pmib->dot11StationConfigEntry.sc_config_type;
			break;
		default:
			break;
	}
	return value;
}

int rtk_sc_set_string_value(unsigned int id, unsigned char *value)
{
	struct net_device *dev;
	int i=0;
	dev=rtl_get_dev_by_wlan_name(g_sc_ifname);
	struct rtl8192cd_priv *priv = GET_DEV_PRIV(dev);

	switch(id)
	{
		case SC_PIN:
			strcpy(priv->pmib->dot11StationConfigEntry.sc_pin, value);
			break;
		case SC_DEFAULT_PIN:		
			strcpy(priv->pmib->dot11StationConfigEntry.sc_default_pin, value);
			//DEBUG_PRINT("default pin %s\n",value);
			break;
		case SC_PASSWORD:
			strcpy(priv->pmib->dot11StationConfigEntry.sc_passwd, value);
			break;
		case SC_DEVICE_NAME:
			strcpy(priv->pmib->dot11StationConfigEntry.sc_device_name, value);
			break;
		case SC_DEVICE_TYPE:
			strcpy(priv->pmib->dot11StationConfigEntry.sc_device_type, value);
			break;
		case SC_SSID:
			strcpy(priv->pmib->dot11StationConfigEntry.dot11DesiredSSID, value);
			strcpy(priv->pmib->dot11StationConfigEntry.dot11SSIDtoScan, value);
			priv->pmib->dot11StationConfigEntry.dot11DesiredSSIDLen = strlen(value);
			priv->pmib->dot11StationConfigEntry.dot11SSIDtoScanLen = strlen(value);
			break;
		case SC_BSSID:
			for(i=0; i<6; i++)
			{
				priv->pmib->dot11StationConfigEntry.dot11DesiredBssid[i] = value[i];
				priv->pmib->dot11StationConfigEntry.dot11Bssid[i] = value[i];
			}
			break;
		default:
			panic_printk("RTL Simple Config don't support this mib setting now!\n");
	}
	return 0;
}

int rtk_sc_get_string_value(unsigned int id, unsigned char *value)
{
	struct net_device *dev;
	dev=rtl_get_dev_by_wlan_name(g_sc_ifname);
	struct rtl8192cd_priv *priv = GET_DEV_PRIV(dev);

	switch(id)
	{
		case SC_PIN:
			strcpy(value, priv->pmib->dot11StationConfigEntry.sc_pin);
			break;
		case SC_DEFAULT_PIN:
			strcpy(value, priv->pmib->dot11StationConfigEntry.sc_default_pin);
			break;
		case SC_PASSWORD:
			strcpy(value, priv->pmib->dot11StationConfigEntry.sc_passwd);
			break;
		case SC_DEVICE_NAME:
			strcpy(value, priv->pmib->dot11StationConfigEntry.sc_device_name);
			break;
		case SC_DEVICE_TYPE:
			strcpy(value, priv->pmib->dot11StationConfigEntry.sc_device_type);
			break;
		default:
			panic_printk("RTL Simple Config don't support this mib setting now!\n");
			break;
	}
	return 0;
}

int set_wep_key(struct rtl8192cd_priv *priv, int type)
{
	int i, length;
	unsigned char value;
	unsigned char wep_key[26]={0};
	if(type == 0)
		length=10;
	else if(type == 1)
		length=26;
	for(i=0; i<length; i++)
	{
		value = priv->pmib->dot11StationConfigEntry.sc_passwd[i];
		if((value >=0x30) && (value <=0x39))
			value -= 0x30;
		else if((value >= 'a')&&(value <= 'z'))
			value -= 0x57;
		else if((value >= 'A')&&(value <= 'Z'))
			value -= 0x37;
		value &= 0xf;
		if(i%2 == 0)
			wep_key[i/2] = value<<4;
		else
			wep_key[i/2] += value;
	}
	memcpy(&(GET_MIB(priv)->dot11DefaultKeysTable.keytype[0]), wep_key, length/2);
}

int rtk_sc_check_security(struct rtl8192cd_priv * priv,struct bss_desc * bss)
{
	int length=0, i=0;
	int status = rtk_sc_get_value(SC_STATUS);
	length = strlen(priv->pmib->dot11StationConfigEntry.sc_passwd);
	if ((bss->capability&BIT(4)) == 0)
	{
		GET_MIB(priv)->dot1180211AuthEntry.dot11AuthAlgrthm = 0;
		GET_MIB(priv)->dot1180211AuthEntry.dot11PrivacyAlgrthm=_NO_PRIVACY_;
		GET_MIB(priv)->dot1180211AuthEntry.dot11EnablePSK = 0;
		rtk_sc_set_value(SC_CHECK_LINK_TIME, 2);
		GET_MIB(priv)->dot11StationConfigEntry.sc_security_type= 0;
		GET_MIB(priv)->dot11RsnIE.rsnielen = 0;
		GET_MIB(priv)->dot118021xAuthEntry.dot118021xAlgrthm= 0;
	}
	else if((bss->capability&BIT(4)))
	{
		if(bss->t_stamp[0] == 0)
		{
			GET_MIB(priv)->dot1180211AuthEntry.dot11EnablePSK = 0;
			if((length == 5) || (length == 10))
			{
				GET_MIB(priv)->dot1180211AuthEntry.dot11PrivacyAlgrthm = _WEP_40_PRIVACY_;
				if(length == 5)
				{
					memcpy(&(GET_MIB(priv)->dot11DefaultKeysTable.keytype[0]), priv->pmib->dot11StationConfigEntry.sc_passwd, 5);
					priv->pmib->dot11StationConfigEntry.sc_security_type= 1;
				}
				else if(length == 10)
				{
					set_wep_key(priv, 0);
					priv->pmib->dot11StationConfigEntry.sc_security_type= 2;
				}
				GET_MIB(priv)->dot1180211AuthEntry.dot11PrivacyKeyIndex = 0;
			}
			else if((length == 13) || (length == 26))
			{
				GET_MIB(priv)->dot1180211AuthEntry.dot11PrivacyAlgrthm = _WEP_104_PRIVACY_;
				if(length == 13)
				{
					memcpy(&(GET_MIB(priv)->dot11DefaultKeysTable.keytype[0]), priv->pmib->dot11StationConfigEntry.sc_passwd, 13);
					priv->pmib->dot11StationConfigEntry.sc_security_type= 3;
				}
				else
				{
					set_wep_key(priv, 1);
					priv->pmib->dot11StationConfigEntry.sc_security_type= 4;
				}
				GET_MIB(priv)->dot1180211AuthEntry.dot11PrivacyKeyIndex = 0;
			}
			GET_MIB(priv)->dot11RsnIE.rsnielen = 0;
			GET_MIB(priv)->dot118021xAuthEntry.dot118021xAlgrthm= 0;
			rtk_sc_set_value(SC_CHECK_LINK_TIME, 2);
		}
		else
		{
			GET_MIB(priv)->dot1180211AuthEntry.dot11AuthAlgrthm = 2;
			GET_MIB(priv)->dot1180211AuthEntry.dot11PrivacyAlgrthm = 2;
			strcpy((char *)GET_MIB(priv)->dot1180211AuthEntry.dot11PassPhrase, priv->pmib->dot11StationConfigEntry.sc_passwd);
			if(bss->t_stamp[0] & 0xffff0000)
			{
				GET_MIB(priv)->dot1180211AuthEntry.dot11EnablePSK = 2;
				if(((bss->t_stamp[0] & 0xf0000000) >> 28) == 0x4)
				{
					GET_MIB(priv)->dot118021xAuthEntry.dot118021xAlgrthm = 1;
				}
				if(((bss->t_stamp[0] & 0x0f000000) >> 24) == 0x5)
				{
					GET_MIB(priv)->dot1180211AuthEntry.dot11WPA2Cipher = 8;
					priv->pmib->dot11StationConfigEntry.sc_security_type= 5;
				}
				else if(((bss->t_stamp[0] & 0x0f000000) >> 24) == 0x4)
				{
					GET_MIB(priv)->dot1180211AuthEntry.dot11WPA2Cipher = 8;
					priv->pmib->dot11StationConfigEntry.sc_security_type= 5;
				}
				else if(((bss->t_stamp[0] & 0x0f000000) >> 24) == 0x1)
				{
					GET_MIB(priv)->dot1180211AuthEntry.dot11WPA2Cipher = 2;
					priv->pmib->dot11StationConfigEntry.sc_security_type= 6;
				}
			}
			else if(bss->t_stamp[0] & 0x0000ffff)
			{
				GET_MIB(priv)->dot1180211AuthEntry.dot11EnablePSK = 1;
				if(((bss->t_stamp[0] & 0x0000f000) >> 12) == 0x4)
				{
					GET_MIB(priv)->dot118021xAuthEntry.dot118021xAlgrthm = 0;
				}
				if(((bss->t_stamp[0] & 0x00000f00) >> 8) == 0x5)
				{
					GET_MIB(priv)->dot1180211AuthEntry.dot11WPACipher = 8;	
					priv->pmib->dot11StationConfigEntry.sc_security_type= 7;
				}
				else if(((bss->t_stamp[0] & 0x00000f00) >> 8) == 0x4)
				{
					GET_MIB(priv)->dot1180211AuthEntry.dot11WPACipher = 8;	
					priv->pmib->dot11StationConfigEntry.sc_security_type= 7;
				}
				else if(((bss->t_stamp[0] & 0x00000f00) >> 8) == 0x1)
				{
					GET_MIB(priv)->dot1180211AuthEntry.dot11WPACipher = 2;	
					priv->pmib->dot11StationConfigEntry.sc_security_type= 8;
				}
			}
		}
	}
	if(status == 3)
	{
		//if sc_security_type>0, status 3 jump to status 4, don't need to get security again.
		if(bss->ssidlen != 0)
		{
			rtk_sc_set_string_value(SC_SSID,bss->ssid);
		}
		rtk_sc_set_value(SC_FIX_ENCRYPT, priv->pmib->dot11StationConfigEntry.sc_security_type+10);
		
		rtk_sc_set_value(SC_PRIV_STATUS, 4);
		rtk_sc_set_value(SC_STATUS, 4);
		
		rtk_sc_sync_vxd_to_root(priv);
		rtk_sc_set_value(SC_CONFIG_TIME, 0);
		rtk_sc_start_connect_target(priv);
	}
	else if(status == 2)
	{
		if(bss->ssidlen != 0)
		{
			rtk_sc_set_string_value(SC_SSID,bss->ssid);
		}
		rtk_sc_set_value(SC_FIX_ENCRYPT, priv->pmib->dot11StationConfigEntry.sc_security_type+10);
		if(0)//(priv->pmib->dot11StationConfigEntry.sc_security_type == 0)
		{
			rtk_sc_set_value(SC_PRIV_STATUS, 4);
			rtk_sc_set_value(SC_STATUS, 4);
			
			rtk_sc_sync_vxd_to_root(priv);
			rtk_sc_set_value(SC_CONFIG_TIME, 0);
			rtk_sc_start_connect_target(priv);
		}
		//panic_printk("get the encrypt in status 2 and it is %d\n", priv->pmib->dot11StationConfigEntry.sc_security_type);
		
	}
	return 0;
}

int rtk_sc_set_passwd(struct rtl8192cd_priv * priv)
{
	int sc_security_type;
	sc_security_type = priv->pmib->dot11StationConfigEntry.sc_security_type;

	switch(sc_security_type)
	{
		case 0:
			break;
		case 1:
			memcpy(&(GET_MIB(priv)->dot11DefaultKeysTable.keytype[0]), priv->pmib->dot11StationConfigEntry.sc_passwd, 5);
			break;
		case 2:
			set_wep_key(priv, 0);
			break;
		case 3:
			memcpy(&(GET_MIB(priv)->dot11DefaultKeysTable.keytype[0]), priv->pmib->dot11StationConfigEntry.sc_passwd, 13);
			break;
		case 4:
			set_wep_key(priv, 0);
			break;
		case 5:
		case 6:
		case 7:
		case 8:	
			strcpy((char *)GET_MIB(priv)->dot1180211AuthEntry.dot11PassPhrase, priv->pmib->dot11StationConfigEntry.sc_passwd);
			break;
		default:
			break;
	}
	return 0;
}
int rtk_sc_get_magic(unsigned int *magic, unsigned int len)
{
	int i=0;
	unsigned int magic_num[32] = {60, 62, 68, 70, 76, 78, 80, 82};
	for(i=0; i<len; i++)
		magic[i] = magic_num[i];
	return 0;
}

int rtk_sc_is_channel_fixed(struct rtl8192cd_priv * priv)
{
	struct rtl8192cd_priv * priv_root = GET_ROOT(priv);

	if(priv->pmib->dot11StationConfigEntry.sc_enabled == 1 && priv->simple_config_could_fix == 1) {
		
		if(priv->pmib->dot11StationConfigEntry.sc_fix_channel > 0)
			return 1;
		else if((priv->pmib->dot11StationConfigEntry.sc_fix_channel < 0) 
				&& (priv->pmib->dot11StationConfigEntry.sc_status == 2 || priv->pmib->dot11StationConfigEntry.sc_status == 3)
				)
			return 1; 
	}

#ifdef UNIVERSAL_REPEATER
	if(IS_DRV_OPEN(GET_VXD_PRIV(priv_root))&& 
		GET_VXD_PRIV(priv_root)->pmib->dot11StationConfigEntry.sc_enabled && priv->simple_config_could_fix == 1) {
			
		if(GET_VXD_PRIV(priv_root)->pmib->dot11StationConfigEntry.sc_fix_channel > 0)
			return 1;
		else if((GET_VXD_PRIV(priv_root)->pmib->dot11StationConfigEntry.sc_fix_channel < 0) &&
			(GET_VXD_PRIV(priv_root)->pmib->dot11StationConfigEntry.sc_status == 2 
			|| GET_VXD_PRIV(priv_root)->pmib->dot11StationConfigEntry.sc_status == 3)
			)
			return 1;
	}
#endif

	return 0;
}


int rtk_sc_get_scan_offset(struct rtl8192cd_priv * priv, int channel, int bw)
{
	int scan_offset = HT_2NDCH_OFFSET_DONTCARE;


	
	if(bw) {

		if(channel >= 36) {
		
			if((channel>144) ? ((channel-1)%8) : (channel%8))
				scan_offset = HT_2NDCH_OFFSET_ABOVE;
			else
				scan_offset = HT_2NDCH_OFFSET_BELOW;
		} else {

			if(channel < 5)
				scan_offset = HT_2NDCH_OFFSET_ABOVE;
			else if(channel > 9)
				scan_offset = HT_2NDCH_OFFSET_BELOW;
			else {

				if(GET_ROOT(priv)->pshare->offset_2nd_chan == HT_2NDCH_OFFSET_ABOVE)
					scan_offset = HT_2NDCH_OFFSET_BELOW;
				else 
					scan_offset = HT_2NDCH_OFFSET_ABOVE;
			}
		}
	}

	return scan_offset;
}


int rtk_sc_during_simple_config_scan(struct rtl8192cd_priv * priv)
{
	struct rtl8192cd_priv * priv_root = GET_ROOT(priv);

	if(priv->pmib->dot11StationConfigEntry.sc_enabled == 1) {
		
		if(priv->simple_config_status >= 1 && priv->simple_config_status <4)
			return 1;
	}

#ifdef UNIVERSAL_REPEATER
	if(IS_DRV_OPEN(GET_VXD_PRIV(priv_root))&& 
		GET_VXD_PRIV(priv_root)->pmib->dot11StationConfigEntry.sc_enabled) {
			
		if(GET_VXD_PRIV(priv_root)->simple_config_status >= 1 && GET_VXD_PRIV(priv_root)->simple_config_status <4)
			return 1;
	}
#endif

	return 0;
}


#endif

#ifdef SUPPORT_MONITOR
void start_monitor_mode(struct rtl8192cd_priv * priv)
{
	priv->is_monitor_mode = TRUE;	
	RTL_W32(RCR, RCR_APP_FCS | RCR_APP_MIC | RCR_APP_ICV | RCR_APP_PHYSTS | RCR_HTC_LOC_CTRL
				| RCR_AMF | RCR_ADF | RCR_AICV | RCR_ACRC32 | RCR_CBSSID_ADHOC | RCR_AB | RCR_AM | RCR_APM | RCR_AAP);

	if(!priv->pmib->miscEntry.chan_switch_disable) {
		init_timer(&priv->chan_switch_timer);
		priv->chan_switch_timer.data = (unsigned long) priv;
		priv->chan_switch_timer.function = rtl8192cd_chan_switch_timer;
		mod_timer(&priv->chan_switch_timer, jiffies + RTL_MILISECONDS_TO_JIFFIES(priv->pmib->miscEntry.chan_switch_time));
	}
	
	panic_printk("priv->is_monitor_mode = %d \n", priv->is_monitor_mode); 
}
void stop_monitor_mode(struct rtl8192cd_priv * priv)
{
	priv->is_monitor_mode = FALSE;
	
	OPMODE_VAL(OPMODE & (~WIFI_SITE_MONITOR));
	if (timer_pending(&priv->chan_switch_timer))
		del_timer_sync(&priv->chan_switch_timer);

	panic_printk("priv->is_monitor_mode = %d,\n", priv->is_monitor_mode); 
}
#endif

#ifdef RTK_WLAN_EVENT_INDICATE
//
// B0~B5     B6        B7~
// Sta Addr  Reason  Interface Name
//
int rtk_wlan_event_indicate(char* ifname, int event, unsigned char* addr, char reason)
{
	char data[MACADDRLEN+1+IFNAMSIZ+2] = {0};
	int data_len = 0;
	int rtk_eventd_pid = get_nl_eventd_pid();
	struct sock* nl_eventd_sk = get_nl_eventd_sk();
    struct net_device	*net_dev;
	struct rtl8192cd_priv	*priv;

	if((rtk_eventd_pid==0)||(nl_eventd_sk==NULL))
	{
		panic_printk("%s:%d, report wifi link status failed, pid=%d,sk=%p\n",__FUNCTION__,__LINE__, rtk_eventd_pid, nl_eventd_sk);
		return -1;
	}

	memcpy(data, addr, MACADDRLEN);
	data[MACADDRLEN] = reason;
	memcpy(data+MACADDRLEN+1, ifname, IFNAMSIZ);
	data[MACADDRLEN+1+IFNAMSIZ] = '\0';
	data_len = MACADDRLEN+1+IFNAMSIZ+2;
	 
    net_dev = dev_get_by_name(&init_net, ifname);
    if (net_dev == NULL)
    {
        printk("rtk_wlan_event_indicate(): can not get dev %s\n", ifname);
        return -1;
    }
    priv = GET_DEV_PRIV(net_dev);
	
	if(priv->pmib->miscEntry.telco_selected == TELCO_CMCC){
		rtk_eventd_netlink_send(rtk_eventd_pid, nl_eventd_sk, event, NULL, data, data_len);
	}else if(priv->pmib->miscEntry.telco_selected == TELCO_CT){
		rtk_eventd_netlink_send_multicast(rtk_eventd_pid, nl_eventd_sk, event, NULL, data, data_len);
	}

	return 0;
}
#endif


#ifdef CONFIG_IEEE80211R
void set_r0key_expire_time(struct rtl8192cd_priv *priv, unsigned char *sta_addr, unsigned long timeout)
{
	struct r0_key_holder *r0kh;
	unsigned long flags = 0;

	if (timeout == 0)
		return;
	
	SMP_LOCK_FT_R0KH(flags);
	r0kh = search_r0kh(priv, sta_addr);
	if (r0kh) {
		r0kh->key_expire_to = timeout;
	}
	SMP_UNLOCK_FT_R0KH(flags);
}

void check_r0key_expire(struct rtl8192cd_priv *priv)
{
	struct r0_key_holder *r0kh;
	struct list_head    *phead, *plist;
	unsigned long flags = 0;
	struct stat_info *pstat;	
	unsigned char tmpBuf[20];
	unsigned char sta_addr[6];

	SAVE_INT_AND_CLI(flags);
	SMP_LOCK_FT_R0KH(flags);

	phead = &priv->r0kh;
	plist = phead->next;
	while (plist != phead) {
		r0kh = list_entry(plist, struct r0_key_holder, list);
		plist = plist->next;

		if (r0kh->key_expire_to > 0 && --r0kh->key_expire_to == 0) {
			memcpy(sta_addr, r0kh->sta_addr, MACADDRLEN);
			
			memset(tmpBuf, 0, sizeof(tmpBuf));
			pstat = get_stainfo(priv, sta_addr);
			if (pstat) {
				sprintf(tmpBuf, "%02x%02x%02x%02x%02x%02xyes", 
						sta_addr[0], sta_addr[1], sta_addr[2], sta_addr[3], sta_addr[4], sta_addr[5]);
				DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_SERIOUS, "del_sta - %02X:%02X:%02X:%02X:%02X:%02X\n",
						sta_addr[0], sta_addr[1], sta_addr[2], sta_addr[3], sta_addr[4], sta_addr[5]);
				del_sta(priv, tmpBuf);		
			}

			remove_r1kh(priv, sta_addr, BSSID);
            
            list_del_init(&(r0kh->list));
            kfree(r0kh);
				
			FT_IndicateEvent(priv, NULL, DOT11_EVENT_FT_KEY_EXPIRE_IND, sta_addr);
		}
	}

	SMP_UNLOCK_FT_R0KH(flags);
	RESTORE_INT(flags);
}

int store_r0kh(struct rtl8192cd_priv *priv, unsigned char *sta_addr, unsigned char *pmk_r0, unsigned char *pmk_r0_name)
{
	struct r0_key_holder *r0kh;
	unsigned long flags = 0;

#ifdef CONFIG_IEEE80211R_CLI_DEBUG
	DEBUG_INFO("store_r0kh(): \n");
#endif

	remove_r0kh(priv, sta_addr);
	
	r0kh = (struct r0_key_holder *)kmalloc(sizeof(struct r0_key_holder), GFP_ATOMIC);
	if (!r0kh) {
		DEBUG_ERR("FT: unable to allocate r0kh store\n");
		return -1;
	}

	memset(r0kh, 0, sizeof(struct r0_key_holder));
	memcpy(r0kh->sta_addr, sta_addr, MACADDRLEN);
	memcpy(r0kh->pmk_r0, pmk_r0, PMK_LEN);
	memcpy(r0kh->pmk_r0_name, pmk_r0_name, PMKID_LEN);
	INIT_LIST_HEAD(&(r0kh->list));

	SAVE_INT_AND_CLI(flags);
	SMP_LOCK_FT_R0KH(flags);
	list_add_tail(&(r0kh->list), &(priv->r0kh));
	SMP_UNLOCK_FT_R0KH(flags);
	RESTORE_INT(flags);

	return 0;
}

struct r0_key_holder *search_r0kh(struct rtl8192cd_priv *priv, unsigned char *sta_addr)
{
	struct r0_key_holder *r0kh = NULL, *ret_r0kh = NULL;
	struct list_head *phead, *plist;
	unsigned long flags = 0;
#ifdef SMP_SYNC
    int locked = 0;
#endif

	if (!netif_running(priv->dev)){
		return NULL;		
	}	

	SAVE_INT_AND_CLI(flags);
    SMP_TRY_LOCK_FT_R0KH(flags, locked);

	phead = &priv->r0kh;
	plist = phead->next;
	while (plist != phead) {
		r0kh = list_entry(plist, struct r0_key_holder, list);
		plist = plist->next;
		
		if (!memcmp(r0kh->sta_addr, sta_addr, MACADDRLEN)) {
            ret_r0kh = r0kh;
			break;
		}
	}

#ifdef SMP_SYNC
    if (locked)
#endif
        SMP_UNLOCK_FT_R0KH(flags);
	RESTORE_INT(flags);

	return ret_r0kh;
}

void remove_r0kh(struct rtl8192cd_priv *priv, unsigned char *sta_addr)
{
	struct r0_key_holder *r0kh;
	unsigned long flags = 0;

    SMP_LOCK_FT_R0KH(flags);

	r0kh = search_r0kh(priv, sta_addr);
	if (r0kh) {
		SAVE_INT_AND_CLI(flags);
		list_del_init(&(r0kh->list));
		kfree(r0kh);
        RESTORE_INT(flags);    
	}

    SMP_UNLOCK_FT_R0KH(flags);
}

void free_r0kh(struct rtl8192cd_priv *priv)
{
	struct r0_key_holder *r0kh;
	struct list_head    *phead, *plist;
	unsigned long flags = 0;

	SAVE_INT_AND_CLI(flags);
	SMP_LOCK_FT_R0KH(flags);

	phead = &priv->r0kh;
	plist = phead->next;
	while (plist != NULL && plist != phead) {
		r0kh = list_entry(plist, struct r0_key_holder, list);
		plist = plist->next;

		list_del_init(&(r0kh->list));
		kfree(r0kh);
	}

	SMP_UNLOCK_FT_R0KH(flags);
	RESTORE_INT(flags);
}

int store_r1kh(struct rtl8192cd_priv *priv, unsigned char *sta_addr, unsigned char *r1kh_id,
	unsigned char *r0kh_id, unsigned int r0kh_id_len, unsigned char *pmk_r1, unsigned char *pmk_r1_name,
	unsigned char *pmk_r0_name, int pairwise)
{
	struct r1_key_holder *r1kh;
	unsigned long flags = 0;

	if (r0kh_id_len > MAX_R0KHID_LEN) {
		DEBUG_ERR("FT: R0KH-ID length is greater than MAX_ROKHID_LEN\n");
		return -1;
	}
	
	remove_r1kh(priv, sta_addr, r1kh_id);
	
	r1kh = (struct r1_key_holder *)kmalloc(sizeof(struct r1_key_holder), GFP_ATOMIC);
	if (!r1kh) {
		DEBUG_ERR("FT: unable to allocate r1kh store\n");
		return -1;
	}

	memset(r1kh, 0, sizeof(struct r1_key_holder));
	memcpy(r1kh->sta_addr, sta_addr, MACADDRLEN);
	memcpy(r1kh->r1kh_id, r1kh_id, MACADDRLEN);
	if (r0kh_id == NULL || r0kh_id_len == 0) {
		r1kh->r0kh_id_len = 0;
	} else {
		memcpy(r1kh->r0kh_id, r0kh_id, r0kh_id_len);
		r1kh->r0kh_id_len = r0kh_id_len;
	}
	memcpy(r1kh->pmk_r1, pmk_r1, PMK_LEN);
	memcpy(r1kh->pmk_r1_name, pmk_r1_name, PMKID_LEN);
	if (pmk_r0_name != NULL)
		memcpy(r1kh->pmk_r0_name, pmk_r0_name, PMKID_LEN);
	r1kh->pairwise = pairwise;
	INIT_LIST_HEAD(&(r1kh->list));

	SAVE_INT_AND_CLI(flags);
	SMP_LOCK_FT_R1KH(flags);
	list_add_tail(&(r1kh->list), &(priv->r1kh));
	SMP_UNLOCK_FT_R1KH(flags);
	RESTORE_INT(flags);
	
	return 0;
}

struct r1_key_holder *search_r1kh(struct rtl8192cd_priv *priv, unsigned char *sta_addr, unsigned char *r1kh_id)
{
	struct r1_key_holder *r1kh = NULL, *ret_r1kh = NULL;
	struct list_head *phead, *plist;
	unsigned long flags = 0;
#ifdef SMP_SYNC
     int locked = 0;
#endif

	SAVE_INT_AND_CLI(flags);
    SMP_TRY_LOCK_FT_R1KH(flags, locked);

	phead = &priv->r1kh;
	plist = phead->next;
	while (plist != phead) {
		r1kh = list_entry(plist, struct r1_key_holder, list);
		plist = plist->next;
		
		if (!memcmp(r1kh->sta_addr, sta_addr, MACADDRLEN) &&
			!memcmp(r1kh->r1kh_id, r1kh_id, MACADDRLEN)) {
			ret_r1kh = r1kh;
			break;
		}
	}

#ifdef SMP_SYNC
    if (locked)
#endif
        SMP_UNLOCK_FT_R1KH(flags);
	RESTORE_INT(flags);

	return ret_r1kh;
}

struct r1_key_holder *search_r1kh_by_pmkid(struct rtl8192cd_priv *priv, unsigned char *pmkid, unsigned int id)
{
	struct r1_key_holder *r1kh = NULL, *ret_r1kh = NULL;
	struct list_head    *phead, *plist;
	unsigned long flags = 0;
#ifdef SMP_SYNC
    int locked = 0;
#endif

	if (!netif_running(priv->dev)){
		return NULL;		
	}	
	
	SAVE_INT_AND_CLI(flags);
    SMP_TRY_LOCK_FT_R1KH(flags, locked);

	phead = &priv->r1kh;
	plist = phead->next;
	while (plist != phead) {
		r1kh = list_entry(plist, struct r1_key_holder, list);
		plist = plist->next;

		if ((id == 0 && !memcmp(r1kh->pmk_r0_name, pmkid, PMKID_LEN)) ||
            (id == 1 && !memcmp(r1kh->pmk_r1_name, pmkid, PMKID_LEN))) {
            ret_r1kh = r1kh;
            break;
		}
	}

#ifdef SMP_SYNC
    if (locked)
#endif
        SMP_UNLOCK_FT_R1KH(flags);
	RESTORE_INT(flags);
    
	return ret_r1kh;
}

int derive_r1_key(struct rtl8192cd_priv *priv, unsigned char *sta_addr, unsigned char *r1kh_id, struct r1_key_holder *out)
{
	struct r0_key_holder *r0kh;
	unsigned char r1_key[PMK_LEN], r1_key_id[PMKID_LEN];
	struct stat_info	*pstat;
	unsigned long flags = 0;

	pstat = get_stainfo(priv, sta_addr);
	if (!pstat) {
		DEBUG_ERR("get_stainfo failed\n");
		return -1;
	}
	
	SMP_LOCK_FT_R0KH(flags);	
	r0kh = search_r0kh(priv, sta_addr);
	if (!r0kh) {
		SMP_UNLOCK_FT_R0KH(flags);
		DEBUG_ERR("Can't ger R0KH\n");
		return -1;
	}

	derive_ft_pmk_r1(r0kh, sta_addr, r1kh_id, r1_key, r1_key_id);
	
	memset(out, 0, sizeof(struct r1_key_holder));
	memcpy(out->sta_addr, sta_addr, MACADDRLEN);
	memcpy(out->r1kh_id, r1kh_id, MACADDRLEN);
	memcpy(out->r0kh_id, R0KH_ID, R0KH_ID_LEN);
	out->r0kh_id_len = R0KH_ID_LEN;
	memcpy(out->pmk_r1, r1_key, PMK_LEN);
	memcpy(out->pmk_r1_name, r1_key_id, PMKID_LEN);
	memcpy(out->pmk_r0_name, r0kh->pmk_r0_name, PMKID_LEN);
	SMP_UNLOCK_FT_R0KH(flags);
    
	if (priv->pmib->dot1180211AuthEntry.dot11EnablePSK)
		out->pairwise = mapPairwise(pstat->wpa_sta_info->UnicastCipher);
	else
		out->pairwise = mapPairwise(pstat->wpa_sta_info->UnicastCipher_1x);
    
	return 0;
}

void remove_r1kh(struct rtl8192cd_priv *priv, unsigned char *sta_addr, unsigned char *r1kh_id)
{
	struct r1_key_holder *r1kh;
	unsigned long flags = 0;

    SMP_LOCK_FT_R1KH(flags);

	r1kh = search_r1kh(priv, sta_addr, r1kh_id);
	if (r1kh) {
		SAVE_INT_AND_CLI(flags);
		list_del_init(&(r1kh->list));
		kfree(r1kh);
		RESTORE_INT(flags);
	}
		
    SMP_UNLOCK_FT_R1KH(flags);
}

void free_r1kh(struct rtl8192cd_priv *priv)
{
	struct r1_key_holder *r1kh;
	struct list_head    *phead, *plist;
	unsigned long flags = 0;

	SAVE_INT_AND_CLI(flags);
	SMP_LOCK_FT_R1KH(flags);

	phead = &priv->r1kh;
	plist = phead->next;
	while (plist != NULL && plist != phead) {
		r1kh = list_entry(plist, struct r1_key_holder, list);
		plist = plist->next;

		list_del_init(&(r1kh->list));
		kfree(r1kh);
	}

	SMP_UNLOCK_FT_R1KH(flags);
	RESTORE_INT(flags);
}

void ft_event_indicate(struct rtl8192cd_priv *priv)
{
	if (priv->pshare->wlanft_pid > 0)
	{
#ifdef LINUX_2_6_27_
		kill_pid(_wlanft_pid, SIGUSR1, 1);
#else
		kill_proc(priv->pshare->wlanft_pid, SIGUSR1, 1);
#endif
	}
}

unsigned int mapPairwise(unsigned char enc)
{
	if (enc == DOT11_ENC_TKIP)
		return BIT(3);
	else if (enc == DOT11_ENC_CCMP)
		return BIT(4);
	else
		return BIT(0);
}
#endif

#if defined(GENERAL_EVENT)
void general_event_indicate(struct rtl8192cd_priv *priv)
{
#ifdef __KERNEL__
	#ifdef LINUX_2_6_27_
	struct pid *pid;
	#endif

	if (priv->pshare->wlangeneral_pid > 0)
	{
#ifdef LINUX_2_6_27_
		kill_pid(_wlangeneral_pid, SIGUSR1, 1);
#else
		kill_proc(priv->pshare->wlangeneral_pid, SIGUSR1, 1);
#endif
	}

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
}
#endif

#ifdef dybw_tx
void dynamic_AC_bandwidth(struct rtl8192cd_priv *priv, struct stat_info *pstat) {
	//dynamically switch ac mode bandwidth
	if(priv->pshare->rf_ft_var.shrink_ac_bw) {
		/* for testing */
		if((priv->pshare->rf_ft_var.shrink_ac_bw>>2) <= priv->pshare->is_40m_bw)
			pstat->shrink_ac_bw = priv->pshare->rf_ft_var.shrink_ac_bw;
	} else {
		switch(pstat->tx_bw) {
			case CHANNEL_WIDTH_80:
				if( pstat->rssi <= priv->pshare->rf_ft_var.ac_del80m ) {
					pstat->bw_cnt[CHANNEL_WIDTH_40]++;	//prepare switching to 40M
					pstat->bw_cnt[CHANNEL_WIDTH_80] = pstat->bw_cnt[CHANNEL_WIDTH_20]=0;

					if(pstat->bw_cnt[CHANNEL_WIDTH_40] == dybw_thrd) {
						pstat->shrink_ac_bw=4;
						*(unsigned int *)pstat->bw_cnt=0;
					}
				} else
					*(unsigned int *)pstat->bw_cnt=0;
                break;
			case CHANNEL_WIDTH_40:
				if( pstat->rssi <= priv->pshare->rf_ft_var.ac_del40m ) {
					pstat->bw_cnt[CHANNEL_WIDTH_20]++;	//prepare switching to 20M
					pstat->bw_cnt[CHANNEL_WIDTH_80] = pstat->bw_cnt[CHANNEL_WIDTH_40]=0;

					if(pstat->bw_cnt[CHANNEL_WIDTH_20] == dybw_thrd) {
						pstat->shrink_ac_bw=2;
						*(unsigned int *)pstat->bw_cnt=0;
					}
				} else if( pstat->rssi > priv->pshare->rf_ft_var.ac_add80m ) {
					if(pstat->tx_bw_bak > CHANNEL_WIDTH_40) {
						pstat->bw_cnt[CHANNEL_WIDTH_80]++;	//prepare switching to 80M
						pstat->bw_cnt[CHANNEL_WIDTH_40]=pstat->bw_cnt[CHANNEL_WIDTH_20]=0;

						if(pstat->bw_cnt[CHANNEL_WIDTH_80] == dybw_thrd) {
							pstat->shrink_ac_bw=8;
							*(unsigned int *)pstat->bw_cnt=0;
						}
					} else {
						//station is not support 80MHz bandwidth, keep 40MHz 
						pstat->shrink_ac_bw=4;
						*(unsigned int *)pstat->bw_cnt=0;
					}
				} else
					*(unsigned int *)pstat->bw_cnt=0;
				break;
			case CHANNEL_WIDTH_20:
				if( pstat->rssi > priv->pshare->rf_ft_var.ac_add80m ) {
					if(pstat->tx_bw_bak > CHANNEL_WIDTH_40) {
						pstat->bw_cnt[CHANNEL_WIDTH_80]++;	//prepare switching to 80M
						pstat->bw_cnt[CHANNEL_WIDTH_40]=pstat->bw_cnt[CHANNEL_WIDTH_20]=0;

						if(pstat->bw_cnt[CHANNEL_WIDTH_80] == dybw_thrd) {
							pstat->shrink_ac_bw=8;
							*(unsigned int *)pstat->bw_cnt=0;
						}
					} else {
						//station is not support 80MHz bandwidth, keep 20MHz 
						pstat->shrink_ac_bw=2;
						*(unsigned int *)pstat->bw_cnt=0;
					}
				} else if( pstat->rssi > priv->pshare->rf_ft_var.ac_add40m ) {
					if(pstat->tx_bw_bak > CHANNEL_WIDTH_20) {
						pstat->bw_cnt[CHANNEL_WIDTH_40]++;	//prepare switching to 40M
						pstat->bw_cnt[CHANNEL_WIDTH_80]=pstat->bw_cnt[CHANNEL_WIDTH_20]=0;

						if(pstat->bw_cnt[CHANNEL_WIDTH_40] == dybw_thrd) {
							pstat->shrink_ac_bw=4;
							*(unsigned int *)pstat->bw_cnt=0;
						}
					} else {
						//station is not support 40MHz bandwidth, keep 20MHz 
						pstat->shrink_ac_bw=2;
						*(unsigned int *)pstat->bw_cnt=0;
					}
				} else
					*(unsigned int *)pstat->bw_cnt=0;
				break;
			default:
				DEBUG_ERR("%02x:%02x:%02x:%02x:%02x:%02x is under unknown bandwidth:%d\n",
					pstat->cmn_info.mac_addr[0],pstat->cmn_info.mac_addr[1],pstat->cmn_info.mac_addr[2],
					pstat->cmn_info.mac_addr[3],pstat->cmn_info.mac_addr[4],pstat->cmn_info.mac_addr[5],pstat->tx_bw);
		}
	}

	if((pstat->shrink_ac_bw>>2) <= priv->pshare->is_40m_bw) {	//prevent target bandwidth not support
		if((pstat->shrink_ac_bw != pstat->shrink_ac_bw_bak)) {	//bandwidth changed
			DEBUG_INFO("AC bandwidth condition changed! from %d to %d\n",
											pstat->shrink_ac_bw_bak,pstat->shrink_ac_bw);

            if(pstat->shrink_ac_bw>>2 <= pstat->tx_bw_bak) {
				pstat->shrink_ac_bw_bak = pstat->shrink_ac_bw;
				switch(pstat->shrink_ac_bw) {
					case 2:
						pstat->tx_bw = CHANNEL_WIDTH_20;
						clearSTABitMap(&priv->pshare->_40m_staMap, REMAP_AID(pstat));
						clearSTABitMap(&priv->pshare->_80m_staMap, REMAP_AID(pstat));
						break;
					case 4:
						pstat->tx_bw = CHANNEL_WIDTH_40;
						setSTABitMap(&priv->pshare->_40m_staMap, REMAP_AID(pstat));
						clearSTABitMap(&priv->pshare->_80m_staMap, REMAP_AID(pstat));
						break;
					case 8:
						pstat->tx_bw = CHANNEL_WIDTH_80;
						clearSTABitMap(&priv->pshare->_40m_staMap, REMAP_AID(pstat));
						setSTABitMap(&priv->pshare->_80m_staMap, REMAP_AID(pstat));
						break;
					default:
						pstat->tx_bw = pstat->tx_bw_bak;
						break;
				}

#ifdef dybw_rx
				/* done by checkBandwidth
				if(priv->assoc_num == 1) {
					//for only one station connected, RX bandwidth shrinked as well
					switch(pstat->shrink_ac_bw) {
						case 2:
							priv->pshare->CurrentChannelBW = CHANNEL_WIDTH_20;
							break;
						case 4:
							priv->pshare->CurrentChannelBW = CHANNEL_WIDTH_40;
							break;
						case 8:
							priv->pshare->CurrentChannelBW = CHANNEL_WIDTH_80;
							break;
						default:
							priv->pshare->CurrentChannelBW = priv->pshare->is_40m_bw;
							break;
					}

					SwBWMode(priv, priv->pshare->CurrentChannelBW, priv->pshare->offset_2nd_chan);
	                SwChnl(priv, priv->pmib->dot11RFEntry.dot11channel, priv->pshare->offset_2nd_chan);
				} else {
					DEBUG_INFO("AC bandwidth rollback to initial value\n");
					priv->pshare->CurrentChannelBW = priv->pshare->is_40m_bw;
					SwBWMode(priv, priv->pshare->CurrentChannelBW, priv->pshare->offset_2nd_chan);
					SwChnl(priv, priv->pmib->dot11RFEntry.dot11channel, priv->pshare->offset_2nd_chan);
				}

				priv->ht_cap_len = 0;
				construct_ht_ie(priv, priv->pshare->CurrentChannelBW, priv->pshare->offset_2nd_chan);
				*/
#endif
				/* done by checkBandwidth
				update_RAMask_to_FW(priv,1);
				*/
			}
		}
	}
}
#endif

#if defined(WIFI_11N_2040_COEXIST_EXT)
void dynamic_N_bandwidth(struct rtl8192cd_priv *priv, struct stat_info *pstat) {
	//dynamically switch N mode bandwidth
	if(pstat->ht_cap_len && (priv->pmib->dot11nConfigEntry.dot11nUse40M == CHANNEL_WIDTH_40)) {
		switch(pstat->tx_bw) {
			case CHANNEL_WIDTH_40:
				if(pstat->rssi <= priv->pshare->rf_ft_var.n_del40m) {
					pstat->bw_cnt[CHANNEL_WIDTH_20]++;
					pstat->bw_cnt[CHANNEL_WIDTH_40]=0;
				} else
					*(unsigned int *)pstat->bw_cnt=0;

				if(pstat->bw_cnt[CHANNEL_WIDTH_20] == dybw_thrd) {
					*(unsigned int *)pstat->bw_cnt=0;
					pstat->tx_bw = CHANNEL_WIDTH_20;
					clearSTABitMap(&priv->pshare->_40m_staMap, REMAP_AID(pstat));
					setSTABitMap(&priv->switch_20_sta, REMAP_AID(pstat));

					if(!orSTABitMap(&priv->pshare->_40m_staMap))
						priv->pshare->ctrl40m=1;
				}
				break;
			case CHANNEL_WIDTH_20:
				if (pstat->rssi > priv->pshare->rf_ft_var.n_add40m) {
					if(pstat->tx_bw_bak > CHANNEL_WIDTH_20) {
						pstat->bw_cnt[CHANNEL_WIDTH_20]=0;
						pstat->bw_cnt[CHANNEL_WIDTH_40]++;

	                	if (pstat->bw_cnt[CHANNEL_WIDTH_40] == dybw_thrd) {
							*(unsigned int *)pstat->bw_cnt=0;

							if(priv->bg_ap_timeout == 0) {
								pstat->tx_bw = CHANNEL_WIDTH_40;

								if(!priv->pshare->CurrentChannelBW && priv->pmib->dot11nConfigEntry.dot11nUse40M==1) {
									priv->pshare->ctrl40m=2;

									pstat->tx_bw = CHANNEL_WIDTH_40;
									setSTABitMap(&priv->pshare->_40m_staMap, REMAP_AID(pstat));
									clearSTABitMap(&priv->switch_20_sta, REMAP_AID(pstat));
								}
							}
						}
					}
				} else
					*(unsigned int *)pstat->bw_cnt=0;

				break;
			default:
				DEBUG_ERR("%02x:%02x:%02x:%02x:%02x:%02x is under unknown bandwidth:%d\n",
					pstat->cmn_info.mac_addr[0],pstat->cmn_info.mac_addr[1],pstat->cmn_info.mac_addr[2],
					pstat->cmn_info.mac_addr[3],pstat->cmn_info.mac_addr[4],pstat->cmn_info.mac_addr[5],pstat->tx_bw);
		}
	}
}
#endif


#if defined(__ECOS) && defined(WLAN_REG_FW_RAM_REFINE)
unsigned char RTL_R8_F(struct rtl8192cd_priv *priv, unsigned int reg)
{
	unsigned long ioaddr = priv->pshare->ioaddr;
	unsigned char val8 = 0;

#if defined(CONFIG_WLAN_HAL_8197F) || defined(CONFIG_WLAN_HAL_8197G)
	if ((GET_CHIP_VER(priv) == VERSION_8197F) || (GET_CHIP_VER(priv) == VERSION_8197G)){
		if(!(REG32(0xB8000064)&BIT0)){
			panic_printk("Should not access WiFi register since 0xB8000064[0]=0,reg=0x%x\n",reg);
			return;
	  } 			   
	}
#endif

#ifdef IO_MAPPING
	unsigned char page = ((unsigned char)(reg >> 8)) & PAGE_NUM;
	if (priv->pshare->io_mapping && page)
	{
#ifndef SMP_SYNC
		unsigned long x;
#endif
		SAVE_INT_AND_CLI(x);

		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) | page, IO_TYPE_CAST(ioaddr + _PSR_));
		val8 = readb(IO_TYPE_CAST(ioaddr + (reg & 0x000000ff)));
		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) & (~PAGE_NUM), IO_TYPE_CAST(ioaddr + _PSR_));

		RESTORE_INT(x);
	}
	else
#endif
	{
#ifdef CONFIG_RTL_8198
		unsigned int data=0;
		int swap[4]={0,8,16,24};
		int diff = reg&0x3;
		data=REG32((ioaddr + (reg&(0xFFFFFFFC)) ) );
		val8=(unsigned char)(( data>>swap[diff])&0xff);
#elif defined(RTK_129X_PLATFORM)
		val8 = pci_io_read_129x(priv, reg, 1);
#else
		val8 = readb(IO_TYPE_CAST(ioaddr + reg));
#endif
	}

	return val8;
}

unsigned short RTL_R16_F(struct rtl8192cd_priv *priv, unsigned int reg)
{
	unsigned long ioaddr = priv->pshare->ioaddr;
	unsigned short val16 = 0;

#if defined(CONFIG_WLAN_HAL_8197F) || defined(CONFIG_WLAN_HAL_8197G)
	if ((GET_CHIP_VER(priv) == VERSION_8197F) || (GET_CHIP_VER(priv) == VERSION_8197G)){
		if(!(REG32(0xB8000064)&BIT0)){
			panic_printk("Should not access WiFi register since 0xB8000064[0]=0,reg=0x%x\n",reg);
			return 0;
	  } 			   
	}
#endif


	if (reg & 0x00000001) {
		panic_printk("Unaligned read to reg 0x%08x!\n", reg);
        return 0;
	}

#ifdef IO_MAPPING
	unsigned char page = ((unsigned char)(reg >> 8)) & PAGE_NUM;
	if (priv->pshare->io_mapping && page)
	{
#ifndef SMP_SYNC
		unsigned long x;
#endif
		SAVE_INT_AND_CLI(x);

		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) | page, IO_TYPE_CAST(ioaddr + _PSR_));
		val16 = readw(IO_TYPE_CAST(ioaddr + (reg & 0x000000ff)));
		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) & (~PAGE_NUM), IO_TYPE_CAST(ioaddr + _PSR_));

		RESTORE_INT(x);
	}
	else
#endif
	{
#ifdef CONFIG_RTL_8198
		unsigned int data=0;
		int swap[4]={0,8,16,24};
		int diff = reg&0x3;
		data=REG32((ioaddr + (reg&(0xFFFFFFFC)) ) );
		val16=(unsigned short)(( data>>swap[diff])&0xffff);
#elif defined(RTK_129X_PLATFORM)
		val16 = pci_io_read_129x(priv, reg, 2);
#else
		val16 = readw(IO_TYPE_CAST(ioaddr + reg));
#endif
	}

#ifdef CHECK_SWAP
	if (priv->pshare->type & ACCESS_SWAP_IO)
		val16 = le16_to_cpu(val16);
#endif

	return val16;
}


unsigned int RTL_R32_F(struct rtl8192cd_priv *priv, unsigned int reg)
{
	unsigned long ioaddr = priv->pshare->ioaddr;
	unsigned int val32 = 0;

#if defined(CONFIG_WLAN_HAL_8197F) || defined(CONFIG_WLAN_HAL_8197G)
	if ((GET_CHIP_VER(priv) == VERSION_8197F) || (GET_CHIP_VER(priv) == VERSION_8197G)){
		if(!(REG32(0xB8000064)&BIT0)){
			panic_printk("Should not access WiFi register since 0xB8000064[0]=0,reg=0x%x\n",reg);
			return 0;
	  } 			   
	}
#endif

	if (reg & 0x00000003) {
		panic_printk("Unaligned read to reg 0x%08x!\n", reg);
        return 0;
	}

#ifdef IO_MAPPING
	unsigned char page = ((unsigned char)(reg >> 8)) & PAGE_NUM;
	if (priv->pshare->io_mapping && page)
	{
#ifndef SMP_SYNC
		unsigned long x;
#endif
		SAVE_INT_AND_CLI(x);

		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) | page, IO_TYPE_CAST(ioaddr + _PSR_));
		val32 = readl(IO_TYPE_CAST(ioaddr + (reg & 0x000000ff)));
		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) & (~PAGE_NUM), IO_TYPE_CAST(ioaddr + _PSR_));

		RESTORE_INT(x);
	}
	else
#endif
	{
#if defined(RTK_129X_PLATFORM)
		val32 = pci_io_read_129x(priv, reg, 4);
#else
		val32 = readl(IO_TYPE_CAST(ioaddr + reg));
#endif
	}

#ifdef CHECK_SWAP
	if (priv->pshare->type & ACCESS_SWAP_IO)
		val32 = le32_to_cpu(val32);
#endif

	return val32;
}

void RTL_W8_F(struct rtl8192cd_priv *priv, unsigned int reg, unsigned char val8)
{
	unsigned long ioaddr = priv->pshare->ioaddr;
	
#if defined(CONFIG_WLAN_HAL_8197F) || defined(CONFIG_WLAN_HAL_8197G)
        if ((GET_CHIP_VER(priv) == VERSION_8197F) || (GET_CHIP_VER(priv) == VERSION_8197G)){
			if(!(REG32(0xB8000064)&BIT0)){
		panic_printk("Should not access WiFi register since 0xB8000064[0]=0,reg=0x%x\n",reg);
		return;
	  } 			   
	}
#endif

#ifdef IO_MAPPING
	unsigned char page = ((unsigned char)(reg >> 8)) & PAGE_NUM;
	if (priv->pshare->io_mapping && page)
	{
#ifndef SMP_SYNC
		unsigned long x;
#endif
		SAVE_INT_AND_CLI(x);

		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) | page, IO_TYPE_CAST(ioaddr + _PSR_));
		writeb(val8, IO_TYPE_CAST(ioaddr + (reg & 0x000000ff)));
		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) & (~PAGE_NUM), IO_TYPE_CAST(ioaddr + _PSR_));

		RESTORE_INT(x);
	}
	else
#endif
	{
#ifdef RTK_129X_PLATFORM
		pci_io_write_129x(priv, reg, 1, val8);
#else
		writeb(val8, IO_TYPE_CAST(ioaddr + reg));
#endif
	}
}

void RTL_W16_F(struct rtl8192cd_priv *priv, unsigned int reg, unsigned short val16)
{
	unsigned long ioaddr = priv->pshare->ioaddr;
	unsigned short val16_n = val16;
#ifdef IO_MAPPING
	unsigned char page;
#endif

#if defined(CONFIG_WLAN_HAL_8197F) || defined(CONFIG_WLAN_HAL_8197G)
	if ((GET_CHIP_VER(priv) == VERSION_8197F) || (GET_CHIP_VER(priv) == VERSION_8197G)){
		if(!(REG32(0xB8000064)&BIT0)){
		panic_printk("Should not access WiFi register since 0xB8000064[0]=0,reg=0x%x\n",reg);
		return;
	  } 			   
	}
#endif

	if (reg & 0x00000001) {
		panic_printk("Unaligned write to reg 0x%08x!, val16=0x%08x!\n", reg, val16);
        return;
	}

#ifdef CHECK_SWAP
	if (priv->pshare->type & ACCESS_SWAP_IO)
		val16_n = cpu_to_le16(val16);
#endif

#ifdef IO_MAPPING
	page = ((unsigned char)(reg >> 8)) & PAGE_NUM;
	if (priv->pshare->io_mapping && page)
	{
#ifndef SMP_SYNC
		unsigned long x;
#endif
		SAVE_INT_AND_CLI(x);

		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) | page, IO_TYPE_CAST(ioaddr + _PSR_));
		writew(val16_n, IO_TYPE_CAST(ioaddr + (reg & 0x000000ff)));
		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) & (~PAGE_NUM), IO_TYPE_CAST(ioaddr + _PSR_));

		RESTORE_INT(x);
	}
	else
#endif
	{
#ifdef RTK_129X_PLATFORM
		pci_io_write_129x(priv, reg, 2, val16_n);
#else
		writew(val16_n, IO_TYPE_CAST(ioaddr + reg));
#endif
	}
}

void RTL_W32_F(struct rtl8192cd_priv *priv, unsigned int reg, unsigned int val32)
{
	unsigned long ioaddr = priv->pshare->ioaddr;
	unsigned int val32_n = val32;
#ifdef IO_MAPPING
	unsigned char page;
#endif

#if defined(CONFIG_WLAN_HAL_8197F) || defined(CONFIG_WLAN_HAL_8197G)
	if ((GET_CHIP_VER(priv) == VERSION_8197F) || (GET_CHIP_VER(priv) == VERSION_8197G)){
		if(!(REG32(0xB8000064)&BIT0)){
		panic_printk("Should not access WiFi register since 0xB8000064[0]=0,reg=0x%x\n",reg);
		return;
	  } 			   
	}
#endif

	if (reg & 0x00000003) {
		panic_printk("Unaligned write to reg 0x%08x!, val32=0x%08x!\n", reg, val32);
        return;
	}

#ifdef CHECK_SWAP
	if (priv->pshare->type & ACCESS_SWAP_IO)
		val32_n = cpu_to_le32(val32);
#endif

#ifdef IO_MAPPING
	page = ((unsigned char)(reg >> 8)) & PAGE_NUM;
	if (priv->pshare->io_mapping && page)
	{
#ifndef SMP_SYNC
		unsigned long x;
#endif
		SAVE_INT_AND_CLI(x);

		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) | page, IO_TYPE_CAST(ioaddr + _PSR_));
		writel(val32_n, IO_TYPE_CAST(ioaddr + (reg & 0x000000ff)));
		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) & (~PAGE_NUM), IO_TYPE_CAST(ioaddr + _PSR_));

		RESTORE_INT(x);
	}
	else
#endif
	{
#ifdef RTK_129X_PLATFORM
		pci_io_write_129x(priv, reg, 4, val32_n);
#else
		writel(val32_n, IO_TYPE_CAST(ioaddr + reg));
#endif
	}
}

int get_rf_mimo_mode(struct rtl8192cd_priv *priv)
{
#if defined(CONFIG_WLAN_HAL_8822BE)
	//if(GET_CHIP_VER(priv) == VERSION_8822B)
	//	return RF_2T2R;
#endif

	if ((priv->pshare->phw->MIMO_TR_hw_support == RF_1T1R) ||
		(priv->pmib->dot11RFEntry.MIMO_TR_mode == RF_1T1R))
		return RF_1T1R;
#ifdef CONFIG_RTL_92D_SUPPORT
	else if ((priv->pshare->phw->MIMO_TR_hw_support == RF_1T2R) ||
		(priv->pmib->dot11RFEntry.MIMO_TR_mode == RF_1T2R)) 
		return RF_1T2R;
#endif
	else if (priv->pshare->phw->MIMO_TR_hw_support == RF_2T2R || 
		priv->pmib->dot11RFEntry.MIMO_TR_mode == RF_2T2R ||
		priv->pmib->dot11RFEntry.MIMO_TR_mode == RF_2T4R)
		return RF_2T2R;
	else if (priv->pshare->phw->MIMO_TR_hw_support == RF_3T3R ||
		priv->pmib->dot11RFEntry.MIMO_TR_mode == RF_3T3R)
		return RF_3T3R;
	else if (priv->pshare->phw->MIMO_TR_hw_support == RF_4T4R)
		return RF_4T4R;

	return RF_2T2R; //2ss as default
}

#endif


#if defined(CONFIG_RTL8672) && defined(CONFIG_WIFI_LED_USE_SOC_GPIO)
unsigned int get_wireless_LED_rx_cnt(struct net_device *dev)
{
	struct rtl8192cd_priv *priv = NULL;
	unsigned int cnt=0;

	if (dev != NULL) {
		priv = GET_DEV_PRIV(dev);
		if (priv)
			cnt = priv->pshare->LED_rx_cnt;
	}

	return cnt;
}

unsigned int get_wireless_LED_tx_cnt(struct net_device *dev)
{
	struct rtl8192cd_priv *priv = NULL;
	unsigned int cnt=0;

	if (dev != NULL) {
		priv = GET_DEV_PRIV(dev);
		if (priv)
			cnt = priv->pshare->LED_tx_cnt;
	}

	return cnt;
}

unsigned int get_wireless_LED_interval(struct net_device *dev)
{
	struct rtl8192cd_priv *priv = NULL;
	unsigned int cnt=0;

	if (dev != NULL) {
		priv = GET_DEV_PRIV(dev);
		if (priv)
			cnt = priv->pshare->LED_Interval;
	}

	return cnt;
}

unsigned int get_wireless_LED_func_off(struct net_device *dev)
{
	struct rtl8192cd_priv *priv = NULL;
	unsigned int func_off=0;

	if (dev != NULL) {
		priv = GET_DEV_PRIV(dev);
		if (priv)
			func_off = priv->pmib->miscEntry.func_off;
	}

	return func_off;
}
#endif


int reg_full_5g_channels[] = {36,40,44,48, 52,56,60,64, 100,104,108,112, 116,120,124,128, 132,136,140,144, 149,153,157,161, 165,169,173,177};

int get_ch_bitmap_idx(struct rtl8192cd_priv *priv , int channel) {
	
	int idx=0;
	int count = sizeof(reg_full_5g_channels) / sizeof(int) ;

	for( ; idx<count ; idx++){		
		if(reg_full_5g_channels[idx] == channel)
			return idx;

		//panic_printk("sizeof regxxx array[%d]\n",sizeof(reg_full_5g_channels));		
	}
	return idx;
}

#if defined(CONFIG_RTL_MONITOR_STA_INFO)
static int search_sta_info(struct rtl8192cd_priv *priv,unsigned char* macaddr)
{
	int i;
	int INVALID = -2, NOT_FOUND = -1;
	
	if(macaddr == NULL)
		return INVALID;
	
	for(i=0; i<NUM_MONITOR; i++)
	{
		if(priv->pshare->monitor_sta_info.monitor_sta_ent[i].valid == 1)		
		{
			if(isEqualMACAddr(priv->pshare->monitor_sta_info.monitor_sta_ent[i].mac,macaddr))
				return i;//found, return index 
			else
				continue;
		}
		else
		{
			break;
		}
	}
	
	return NOT_FOUND;//not found
}

static int is_from_ap(struct rx_frinfo *pfrinfo)
{
	unsigned char *pframe=NULL;
	unsigned int frtype=0;
	unsigned int sframe_type=0;
	int ret=-1;
	pframe = get_pframe(pfrinfo);
	frtype = GetFrameType(pframe);
	switch(frtype)
	{
		case WIFI_DATA_TYPE:
			switch(pfrinfo->to_fr_ds)
			{
				case 0x01:
				case 0x11:
					ret = 1;
					break;
				default:
					ret = 0;
					break;
			}
			break;
		case WIFI_CTRL_TYPE:
		case WIFI_MGT_TYPE:
			sframe_type = GetFrameSubType(pframe);
			switch(sframe_type)
			{
				case WIFI_ASSOCRSP:
				case WIFI_REASSOCRSP:
				case WIFI_PROBERSP:
				case WIFI_BEACON:
					ret = 1;
					break;
				default:
					ret = 0;
					break;
			}
			break;
		default:
			ret = 0;
			break;	
	}
	return ret;
}

static __inline__ unsigned char rtk_monitor_cal_rssi_avg(unsigned int agv, unsigned int pkt_rssi)
{
	unsigned int rssi;

	if(agv == 0)
	    return (unsigned char)pkt_rssi;

	rssi = ((agv * 9) + pkt_rssi) / 10;
	if (pkt_rssi > agv)
		rssi++;

	return (unsigned char)rssi;
}

int rtk_monitor_sta_info(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
	int search_result = 0, index = 0, i, tmp_index = 0;
	unsigned long max=0, sec=0;	
	
	if(pfrinfo == NULL)
	{
		return -1;
	}

	if(is_from_ap(pfrinfo)==1)
		return -1;	
		
	search_result = search_sta_info(priv,pfrinfo->sa);
	if( search_result>=0 ) // found, update	
	{
		index = search_result;		
		
	}else if( search_result==-1 ) //not in the list, insert
	{		
		if( NUM_MONITOR==priv->pshare->monitor_sta_info.sta_entry_num )
		{			
			for(i=0; i<NUM_MONITOR; ++i)
			{
				if(jiffies/HZ >= priv->pshare->monitor_sta_info.monitor_sta_ent[i].sec)
					sec = jiffies/HZ - priv->pshare->monitor_sta_info.monitor_sta_ent[i].sec;
				else
					sec = jiffies/HZ + ~(unsigned long)0/HZ - priv->pshare->monitor_sta_info.monitor_sta_ent[i].sec;	
				
				if( sec>=max ){
					max = sec;
					tmp_index = i;
				}
			}
			index = tmp_index;		
		}else{
			index = priv->pshare->monitor_sta_info.sta_entry_num;
			priv->pshare->monitor_sta_info.sta_entry_num++;			
		}

		memcpy(priv->pshare->monitor_sta_info.monitor_sta_ent[index].mac,pfrinfo->sa,ETH_ALEN);
		priv->pshare->monitor_sta_info.monitor_sta_ent[index].valid = 1;	
		priv->pshare->monitor_sta_info.monitor_sta_ent[index].isAP = is_from_ap(pfrinfo);			
	}	

    if(pfrinfo->rf_info.rssi)
	    priv->pshare->monitor_sta_info.monitor_sta_ent[index].rssi = rtk_monitor_cal_rssi_avg(priv->pshare->monitor_sta_info.monitor_sta_ent[index].rssi, pfrinfo->rf_info.rssi);
	priv->pshare->monitor_sta_info.monitor_sta_ent[index].sec = jiffies/HZ;	
	
	return 0;
}
#endif


/*retunr current jiffies*/
unsigned long rtw_get_current_time(void){

	return jiffies;
	
}

/*retunr value is ms , (current_time-start_time) as ms*/
unsigned long rtw_get_passing_time_ms(unsigned long start_time){

	return RTL_JIFFIES_TO_MILISECONDS(jiffies-start_time);
	
}

#ifdef AP_NEIGHBOR_INFO
/* clear info */
void ap_neighbor_deinit(struct rtl8192cd_priv *priv)
{		
    if(IS_ROOT_INTERFACE(priv))
	{      
		memset(&priv->pshare->ap_neighbor, 0x0, sizeof(struct ap_neighbor_info));
		priv->pshare->ap_neighbor.sta_entry_num = 0;
    }
}

static int ap_neighbor_lookup(struct rtl8192cd_priv *priv, unsigned char *macaddr)
{
	int i;
	int INVALID = -2, NOT_FOUND = -1;
	
	if(macaddr == NULL)
		return INVALID;
	
	for(i=0; i<MAX_AP_NEIGHBOR_INFO_NUM; i++)
	{
		if(priv->pshare->ap_neighbor.ap_neighbor_info_ent[i].valid == 1)		
		{
			if(isEqualMACAddr(priv->pshare->ap_neighbor.ap_neighbor_info_ent[i].mac, macaddr))
				return i;	//found, return index 
			else
				continue;
		}
		else
		{
			break;
		}
	}
	
	return NOT_FOUND;		//not found
}

int ap_neighbor_parse_pkt(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo, struct bss_desc *pBss)
{	
	unsigned char *addr, *p, *pframe, *sa, channel=0;
	int len=0, ret = 0, i;
	struct wifi_mib *pmib;	
	UINT16	val16;
	UINT32	basicrate=0, supportrate=0;
	
	pframe = get_pframe(pfrinfo);
	sa = GetAddr2Ptr(pframe);
	pmib = GET_MIB(priv);
	
    // check ssid 
    p = get_ie(pframe + WLAN_HDR_A3_LEN + _BEACON_IE_OFFSET_, _SSID_IE_, &len,
			pfrinfo->pktlen - WLAN_HDR_A3_LEN - _BEACON_IE_OFFSET_);

	if ((p == NULL) || (len == 0) || (*(p+2) == '\0'))	
	{
		//panic_printk("drop beacon due to null ssid\n");
		ret = -1;
		goto ERR_EXIT;
	}
	pBss->ssidlen = len;
	memcpy((void *)(pBss->ssid), (void *)(p+2), len);	
    
    // check bssid
#ifdef CONFIG_RTK_MESH
	if(pfrinfo->is_11s)
		addr = GetAddr2Ptr(pframe);
	else
#endif
		addr = GetAddr3Ptr(pframe);	
    memcpy(pBss->bssid, addr, MACADDRLEN);
	
    // check networkType(AP or ADHOC)
	memcpy(&val16, (pframe + WLAN_HDR_A3_LEN + 8 + 2), 2);
	pBss->capability = le16_to_cpu(val16);
	if ((pBss->capability & BIT(0)) && !(pBss->capability & BIT(1)))
		pBss->bsstype = WIFI_AP_STATE;
	else if (!(pBss->capability & BIT(0)) && (pBss->capability & BIT(1)))
		pBss->bsstype = WIFI_ADHOC_STATE;
	else
		pBss->bsstype = 0;
	
    // check channel    
	p = get_ie(pframe + WLAN_HDR_A3_LEN + _BEACON_IE_OFFSET_, _DSSET_IE_, &len,
			pfrinfo->pktlen - WLAN_HDR_A3_LEN - _BEACON_IE_OFFSET_);
	if (p != NULL)
		channel = *(p+2);
	else {
		p = get_ie(pframe + WLAN_HDR_A3_LEN + _BEACON_IE_OFFSET_, _HT_IE_, &len,
			pfrinfo->pktlen - WLAN_HDR_A3_LEN - _BEACON_IE_OFFSET_);
		if (p !=  NULL) 
			channel = *(p+2);
		else {
			//panic_printk("drop beacon due to no channel\n");
			ret = -1;
			goto ERR_EXIT;
		}
	}
	pBss->channel = channel;	
    
    // check standard
	p = get_ie(pframe + WLAN_HDR_A3_LEN + _BEACON_IE_OFFSET_, _SUPPORTEDRATES_IE_, &len,
			pfrinfo->pktlen - WLAN_HDR_A3_LEN - _BEACON_IE_OFFSET_);
	if (p != NULL) {
		for(i=0; i<len; i++) {
			if (p[2+i] & 0x80)
				basicrate |= get_bit_value_from_ieee_value(p[2+i] & 0x7f);
			supportrate |= get_bit_value_from_ieee_value(p[2+i] & 0x7f);
		}
	}

	p = get_ie(pframe + WLAN_HDR_A3_LEN + _BEACON_IE_OFFSET_, _EXT_SUPPORTEDRATES_IE_, &len,
			pfrinfo->pktlen - WLAN_HDR_A3_LEN - _BEACON_IE_OFFSET_);
	if (p != NULL) {
		for(i=0; i<len; i++) {
			if (p[2+i] & 0x80)
				basicrate |= get_bit_value_from_ieee_value(p[2+i] & 0x7f);
			supportrate |= get_bit_value_from_ieee_value(p[2+i] & 0x7f);
		}
	}
	
	if (channel <= 14)
	{
#ifdef P2P_SUPPORT			
		if(rtk_p2p_is_enabled(priv)){
			/*under P2P mode allow no support B rate*/ 
		}
		else
#endif
		if (!(pmib->dot11BssType.net_work_type & WIRELESS_11B)){
			if (((basicrate & 0xff0) == 0) && ((supportrate & 0xff0) == 0)){
				//panic_printk("drop beacon due to no b and g rate\n");
				ret = -1;
				goto ERR_EXIT;
			}
		}
		if (!(pmib->dot11BssType.net_work_type & WIRELESS_11G)){
			if (((basicrate & 0xf) == 0) && ((supportrate & 0xf) == 0)){
				//panic_printk("drop beacon due to no b and g rate\n");
				ret = -1;
				goto ERR_EXIT;
			}
		}
	}
	
    if (channel >= 36)
		pBss->network |= WIRELESS_11A;
	else {
		if ((basicrate & 0xff0) || (supportrate & 0xff0))
			pBss->network |= WIRELESS_11G;
		if ((basicrate & 0xf) || (supportrate & 0xf))
			pBss->network |= WIRELESS_11B;
	}

	p = get_ie(pframe + WLAN_HDR_A3_LEN + _BEACON_IE_OFFSET_, _HT_CAP_, &len,
				pfrinfo->pktlen - WLAN_HDR_A3_LEN - _BEACON_IE_OFFSET_);
	if (p !=  NULL) {		
		pBss->network |= WIRELESS_11N;
	} 
	
#ifdef RTK_AC_SUPPORT
	p = get_ie(pframe + WLAN_HDR_A3_LEN + _BEACON_IE_OFFSET_, EID_VHTCapability, &len,
			pfrinfo->pktlen - WLAN_HDR_A3_LEN - _BEACON_IE_OFFSET_);
	if ((p !=  NULL) && (len <= sizeof(struct vht_cap_elmt))) {
		pBss->network |= WIRELESS_11AC;
	}	
#endif
    
    // check rssi
    pBss->rssi = (unsigned char)pfrinfo->rf_info.rssi;

	return ret;

ERR_EXIT:
		return ret;
}

int ap_neighbor_add(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{     
	int search_result = 0, index = 0, i, tmp_index = 0;
	unsigned long max=0, sec=0;	
	unsigned char mac[MACADDRLEN] = {0};
	struct bss_desc bss;

	if(pfrinfo == NULL)	
		return -1;		

	memset(&bss, 0, sizeof(struct bss_desc));
	ap_neighbor_parse_pkt(priv, pfrinfo, &bss);
	memcpy(mac, bss.bssid, MACADDRLEN);	

	search_result = ap_neighbor_lookup(priv, mac);
	if( search_result>=0 ) // found, update	entry
	{
		index = search_result;		
		
	}else if( search_result==-1 ) //not in the list, insert
	{		
		if( MAX_AP_NEIGHBOR_INFO_NUM==priv->pshare->ap_neighbor.sta_entry_num )
		{			
			for(i=0; i<MAX_AP_NEIGHBOR_INFO_NUM; ++i)
			{
				if(jiffies/HZ >= priv->pshare->ap_neighbor.ap_neighbor_info_ent[i].sec)
					sec = jiffies/HZ - priv->pshare->ap_neighbor.ap_neighbor_info_ent[i].sec;
				else
					sec = jiffies/HZ + ~(unsigned long)0/HZ - priv->pshare->ap_neighbor.ap_neighbor_info_ent[i].sec;
				
				if( sec>=max ){
					max = sec;
					tmp_index = i;
				}
			}
			index = tmp_index;				
		}else {
			index = priv->pshare->ap_neighbor.sta_entry_num;
			priv->pshare->ap_neighbor.sta_entry_num++;			
		}	
		
		priv->pshare->ap_neighbor.ap_neighbor_info_ent[index].valid = 1;
		memcpy(priv->pshare->ap_neighbor.ap_neighbor_info_ent[index].mac, bss.bssid, MACADDRLEN);
		if(priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_5G)
			priv->pshare->ap_neighbor.ap_neighbor_info_ent[index].band = 2;
		else
			priv->pshare->ap_neighbor.ap_neighbor_info_ent[index].band = 1;	
	}	
	
	memcpy(priv->pshare->ap_neighbor.ap_neighbor_info_ent[index].ssid, bss.ssid, WLAN_SSID_MAXLEN+1);	
	priv->pshare->ap_neighbor.ap_neighbor_info_ent[index].channel = bss.channel;
	priv->pshare->ap_neighbor.ap_neighbor_info_ent[index].rssi = bss.rssi;
	priv->pshare->ap_neighbor.ap_neighbor_info_ent[index].standard = bss.network; 	
	priv->pshare->ap_neighbor.ap_neighbor_info_ent[index].sec = jiffies/HZ;	
	if(bss.bsstype == WIFI_AP_STATE)
		priv->pshare->ap_neighbor.ap_neighbor_info_ent[index].networktype = 0;
	else if(bss.bsstype == WIFI_ADHOC_STATE)
		priv->pshare->ap_neighbor.ap_neighbor_info_ent[index].networktype = 1;

	return 0;
}
#endif

#if defined(CONFIG_WLAN_HAL_8197F) && defined(__OSK__)

#define SYSTEM_REG_BASE 	0xB8000000
#define BSP_BOND_OPTION		(SYSTEM_REG_BASE + 0xC)

/*
 * Bonding Option
 */
#define BSP_BOND_97FB	1
#define BSP_BOND_97FN	2
#define BSP_BOND_97FS	3

#ifndef __raw_readl
#define __raw_readl __raw_readl
static inline u32 __raw_readl(const volatile void __iomem *addr)
{
		return *(const volatile u32 /*__force*/ *)addr;
}
#endif

unsigned int rtl819x_bond_option(void)
{
	unsigned int type = 0, ret = 0;
	
	type = __raw_readl((void __iomem*)BSP_BOND_OPTION) & 0xf;
	
	switch(type) {
	case 0x0:	/* 97FB */
		ret = BSP_BOND_97FB;
		break;
	case 0x4:	/* 97FN */
	case 0x5:
	case 0x6:
		 ret = BSP_BOND_97FN;
		break;
	case 0xa:	/* 97FS */
	case 0xb:
	case 0xc:
		ret = BSP_BOND_97FS;
	}
	
	// pr_debug("[%s][%d]: 97F type %d\n", __FUNCTION__, __LINE__, ret);
	return ret;
}
#endif

#ifdef ADAPTIVITY_STATS_DEBUG
void adaptivity_stats_check(struct rtl8192cd_priv *priv)
{
	unsigned char ada_stats = priv->pshare->rf_ft_var.ada_stats;
	unsigned int ada_stats_to = priv->pshare->rf_ft_var.ada_stats_to;
	unsigned int raise = priv->pshare->ada_stats_raise;
	unsigned int total = priv->pshare->ada_stats_total;
	unsigned int i, cnt;
#ifdef SMP_SYNC
	unsigned long flags;
#endif

	if (priv->pshare->ada_stats_prev != ada_stats) {
		priv->pshare->ada_stats_prev = ada_stats;
		if (ada_stats) {
			priv->pshare->ada_stats_total = 0;
			priv->pshare->ada_stats_raise = 0;
			mod_timer(&priv->pshare->ada_stats_timer, 
				jiffies + RTL_MILISECONDS_TO_JIFFIES(ada_stats_to));
		}
		else {
			if (timer_pending(&priv->pshare->ada_stats_timer))
				del_timer(&priv->pshare->ada_stats_timer);
		}
		return;
	}
	
	if (ada_stats && total) {
#ifdef SMP_SYNC
		spin_lock_irqsave(&priv->pshare->ada_stats_lock, flags);
#endif
		priv->pshare->ada_stats_ratio = (raise * 100) / total;
		priv->pshare->ada_stats_total = 0;
		priv->pshare->ada_stats_raise = 0;
#ifdef SMP_SYNC
		spin_unlock_irqrestore(&priv->pshare->ada_stats_lock, flags);
#endif
		cnt = priv->pshare->ada_stats_ratio / 10;
		
		panic_printk("[");
		for (i=0; i<10; i++) {
			if (i < cnt)
				panic_printk("#");
			else
				panic_printk(" ");
		}
		panic_printk("] %3d %%\n", priv->pshare->ada_stats_ratio);
	}
	else {
		priv->pshare->ada_stats_ratio = -1;
	}
}

void adaptivity_stats_timer(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	unsigned int ada_stats_to = priv->pshare->rf_ft_var.ada_stats_to;
#ifdef SMP_SYNC
	unsigned long flags;
#endif

#ifdef SMP_SYNC
	spin_lock_irqsave(&priv->pshare->ada_stats_lock, flags);
#endif
	priv->pshare->ada_stats_total++;
	if (check_adaptivity_test(priv))
		priv->pshare->ada_stats_raise++;
#ifdef SMP_SYNC
	spin_unlock_irqrestore(&priv->pshare->ada_stats_lock, flags);
#endif
	
	if (IS_DRV_OPEN(priv)) {
		mod_timer(&priv->pshare->ada_stats_timer, 
			jiffies + RTL_MILISECONDS_TO_JIFFIES(ada_stats_to));
	}
}
#endif

#if !defined(__ECOS)
#if defined(_CORTINA_) && LINUX_VERSION_CODE <= KERNEL_VERSION(4,5,7)
// refer to linux-4.6.x/mm/page_alloc.c
// refer to linux-4.6.x/mm/mmzone.c
struct pglist_data *first_online_pgdat(void)
{
	return NODE_DATA(first_online_node);
}

struct pglist_data *next_online_pgdat(struct pglist_data *pgdat)
{
	int nid = next_online_node(pgdat->node_id);

	if (nid == MAX_NUMNODES)
		return NULL;
	return NODE_DATA(nid);
}

/*
 * next_zone - helper magic for for_each_zone()
 */
struct zone *next_zone(struct zone *zone)
{
	pg_data_t *pgdat = zone->zone_pgdat;

	if (zone < pgdat->node_zones + MAX_NR_ZONES - 1)
		zone++;
	else {
		pgdat = next_online_pgdat(pgdat);
		if (pgdat)
			zone = pgdat->node_zones;
		else
			zone = NULL;
	}
	return zone;
}

long si_mem_available(void)
{
	long available;
	unsigned long pagecache;
	unsigned long wmark_low = 0;
	unsigned long pages[NR_LRU_LISTS];
	struct zone *zone;
	int lru;

	for (lru = LRU_BASE; lru < NR_LRU_LISTS; lru++)
		pages[lru] = global_page_state(NR_LRU_BASE + lru);

	for_each_zone(zone)
		wmark_low += zone->watermark[WMARK_LOW];

	/*
	* Estimate the amount of memory available for userspace allocations,
	* without causing swapping.
	*
	* Free memory cannot be taken below the low watermark, before the
	* system starts swapping.
	*/
	available = global_page_state(NR_FREE_PAGES) - wmark_low;

	/*
	* Not all the page cache can be freed, otherwise the system will
	* start swapping. Assume at least half of the page cache, or the
	* low watermark worth of cache, needs to stay.
	*/
	pagecache = pages[LRU_ACTIVE_FILE] + pages[LRU_INACTIVE_FILE];
	pagecache -= min(pagecache / 2, wmark_low);
	available += pagecache;

	/*
	* Part of the reclaimable slab consists of items that are in use,
	* and cannot be freed. Cap this estimate at the low watermark.
	*/
	available += global_page_state(NR_SLAB_RECLAIMABLE) -
			min(global_page_state(NR_SLAB_RECLAIMABLE) / 2, wmark_low);

	if (available < 0)
	   available = 0;

	return available;
}
#endif
#endif
void dump_hex(unsigned char *name, unsigned char *key, unsigned char len)
{
#if(DUMP_HEX==1) 
	int tmp = 0;
	if(key && (len > 0)) {
		panic_printk("[%s],len[%d] 0x[%x]\n", name, len , len);
		for(tmp=0; tmp<len; tmp++) {
			panic_printk("%02x ", (unsigned char)key[tmp]);
			if(tmp%16 == 15)
				panic_printk("\n");
		}
		panic_printk("\n");
	} else{
		panic_printk("!! error key");
		panic_printk("name[%s],len[%d] 0x[%x]\n", name, len , len);
	}
#else
#endif
}

void dump_security_mib(struct rtl8192cd_priv *priv)
{
	log("psk[%d] auth[%d] encr[%d] 11w[%d] sha256[%d]",
		priv->pmib->dot1180211AuthEntry.dot11EnablePSK,
		priv->pmib->dot1180211AuthEntry.dot11AuthAlgrthm,
		priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm,
		priv->pmib->dot1180211AuthEntry.dot11IEEE80211W,
		priv->pmib->dot1180211AuthEntry.dot11EnableSHA256);

	log("wpa[%d] wpa2[%d] wpa3[%d]",
		priv->pmib->dot1180211AuthEntry.dot11WPACipher,
		priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher,
		priv->pmib->dot1180211AuthEntry.dot11WPA3Cipher);
}

#if defined(CONFIG_RTL_EXT_PORT_SUPPORT)
extern int rtl_check_ext_port_napt_entry(uint32 sip, uint16 sport, uint32 dip, uint16 dport, uint8 protocol);
extern unsigned int _br0_ip;
extern unsigned int _br0_mask;
extern int gHwNatEnabled;
extern int extPortEnabled;
extern unsigned int statistic_wlan_xmit_to_eth;
extern unsigned int statistic_wlan_out_napt_exist;
extern unsigned int statistic_wlan_rx_unicast_pkt;
extern struct iphdr * rtl_get_ipv4_header(uint8 *macFrame);
extern int rtl_wlan_frame_use_hw_nat(uint8 *data);
#endif

#if defined(CONFIG_BRIDGE) && defined(CONFIG_RTL_819X_SWCORE)
int rtl_wifi_mac_create_hooks(const unsigned char *addr)
{
	int ret=0;
#if  defined(CONFIG_RTL_ISP_MULTI_WAN_SUPPORT)
	ret=rtl_add_ext_fdb_entry(addr);
#elif defined(CONFIG_RTL_EXT_PORT_SUPPORT)
	if(extPortEnabled)
		ret=rtl_add_ext_fdb_entry(addr);
#endif
	return ret;
}
int rtl_wifi_mac_del_hooks(const unsigned char *addr)
{
	int ret=0;
#if  defined(CONFIG_RTL_ISP_MULTI_WAN_SUPPORT)
	ret=rtl_del_ext_fdb_entry(addr);
#elif defined(CONFIG_RTL_EXT_PORT_SUPPORT)
	if(extPortEnabled)
		ret=rtl_del_ext_fdb_entry(addr);
#endif
	return ret;
}
#endif

