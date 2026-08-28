/*
 * Copyright c                Realtek Semiconductor Corporation, 2003
 * All rights reserved.
 *
 */
#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/netdevice.h>
#include <linux/spinlock.h>
#include <net/rtl/rtl_brsc.h>

#define RTW_MAX_BRSC_INFO (128) /*shoud be 2^n*/
#define RTW_MAX_BRSC_HASH (RTW_MAX_BRSC_INFO/2)
#define RTW_SAME_MAC(a,b) (0==memcmp((a),(b),ETH_ALEN))

struct rtw_brsc_dev_blacklist {
	unsigned char valid;
	char ifname[IFNAMSIZ];
};

struct rtw_brsc_entry {
	struct hlist_node hnode;
	unsigned char mac[ETH_ALEN];
	struct net_device* dev;
} ;

struct rtw_brsc_ctx {
	struct hlist_head  brsc_hash[RTW_MAX_BRSC_HASH];
	struct rtw_brsc_entry brsc_info[RTW_MAX_BRSC_INFO];
	unsigned char br0_mac[ETH_ALEN];
	unsigned short replace_idx; 
	unsigned char disable;
	struct rtw_brsc_dev_blacklist dev_blacklist[RTW_BRSC_MAX_BLACK_DEV_NUM];
	unsigned char blacklist_enable;
	spinlock_t brsc_lock;
} rtw_brsc;

void rtw_brsc_init(void)
{
	int i;
	memset(&rtw_brsc,0,sizeof(rtw_brsc));
	spin_lock_init(&rtw_brsc.brsc_lock);
	for(i=0;i<RTW_MAX_BRSC_HASH;i++)
		INIT_HLIST_HEAD(&rtw_brsc.brsc_hash[i]);
	return;
}

static unsigned int rtw_mac_hash(unsigned char *mac)
{
	unsigned int x;

	x = mac[0] ^ mac[1] ^ mac[2] ^ mac[3] ^
		mac[4] ^ mac[5];

	return x & (RTW_MAX_BRSC_HASH - 1);
}

struct rtw_brsc_entry *rtw_find_brsc_entry(unsigned char *mac)
{
	struct rtw_brsc_entry *entry=NULL;
	hlist_for_each_entry(entry,&rtw_brsc.brsc_hash[rtw_mac_hash(mac)], hnode)
	{
		if(RTW_SAME_MAC(mac,entry->mac))
			return entry;
	}
	return NULL;
}

void rtw_flush_brsc_info_by_dev(struct net_device *dev)
{
	struct rtw_brsc_entry *entry = NULL;
	int i;

	spin_lock_bh(&rtw_brsc.brsc_lock);
	for(i = 0; i < RTW_MAX_BRSC_INFO; i++)
	{
		entry = &rtw_brsc.brsc_info[i];
		if(entry->dev == dev)
		{
			entry->dev = NULL;
			memset(entry->mac, 0, ETH_ALEN);
			hlist_del_init(&entry->hnode);
		}
	}
	spin_unlock_bh(&rtw_brsc.brsc_lock);
	return;
}

void rtw_flush_brsc_info_by_mac(unsigned char *mac)
{
	struct rtw_brsc_entry *entry = NULL;

	spin_lock_bh(&rtw_brsc.brsc_lock);
	entry = rtw_find_brsc_entry(mac);
	if(entry)
	{
		entry->dev = NULL;
		memset(entry->mac, 0, ETH_ALEN);
		hlist_del_init(&entry->hnode);
	}
	spin_unlock_bh(&rtw_brsc.brsc_lock);
	return;
}

void rtw_flush_brsc_info(struct net_device *dev, unsigned char *mac)
{
	if(mac)
	{
		rtw_flush_brsc_info_by_mac(mac);
	}
	else if(dev)
	{
		rtw_flush_brsc_info_by_dev(dev);
	}
	return;
}

void rtw_update_brsc_info(unsigned char *mac, struct net_device *dev)
{
	/*update existed entry incase of sta roaming*/
	struct rtw_brsc_entry *entry;

	if(memcmp(rtw_brsc.br0_mac, mac, ETH_ALEN) == 0)
		return;
	spin_lock_bh(&rtw_brsc.brsc_lock);
	entry = rtw_find_brsc_entry(mac);
	if(NULL == entry) {
		entry = &rtw_brsc.brsc_info[rtw_brsc.replace_idx++];
		if(rtw_brsc.replace_idx >= RTW_MAX_BRSC_INFO)
			rtw_brsc.replace_idx = 0;
		memcpy(entry->mac,mac,ETH_ALEN);
		hlist_del_init(&entry->hnode);
		hlist_add_head(&entry->hnode,&rtw_brsc.brsc_hash[rtw_mac_hash(mac)]);
	}		
	entry->dev = dev;
	spin_unlock_bh(&rtw_brsc.brsc_lock);
	return;
}

struct net_device* rtw_get_brsc_dev(unsigned char *dmac)
{
	struct net_device *dev=NULL;
	struct rtw_brsc_entry *entry;

	spin_lock_bh(&rtw_brsc.brsc_lock);
	entry = rtw_find_brsc_entry(dmac);
	spin_unlock_bh(&rtw_brsc.brsc_lock);
	if(entry)
		dev = entry->dev;
	return dev;
}

#ifdef CONFIG_RTW_BRSC
int rtw_brsc_go(struct sk_buff *skb, struct net_device *src_dev)
{
	struct net_device *dev;
	unsigned char *da = skb->data;
	dev = rtw_get_brsc_dev(da);
	if(dev == src_dev)
	{
		printk("Error: pkt should not go back\n");
		dev_kfree_skb_any(skb);
		return RTW_BRSC_DROP;
	}
	if(dev) {
		if(rtw_brsc_is_black_dev(dev->name))
			return RTW_BRSC_CONTINUE;
		if(!netif_running(dev))
			return RTW_BRSC_CONTINUE;
		if(rtw_brsc_process_vlan(dev, &skb))
			return RTW_BRSC_CONTINUE;
		skb->dev = dev;
#if defined(CONFIG_COMPAT_NET_DEV_OPS)
		dev->hard_start_xmit(skb, dev);
#else
		dev->netdev_ops->ndo_start_xmit(skb,dev);
#endif
		return RTW_BRSC_SUCCESS;
	}
	return RTW_BRSC_CONTINUE;
}
EXPORT_SYMBOL(rtw_brsc_go);
#endif

int rtw_brsc_update_br0_mac(unsigned char *mac)
{
	if(!mac)
		return -1;

	memcpy(rtw_brsc.br0_mac, mac, ETH_ALEN);
	rtw_flush_brsc_info_by_mac(mac);
	return 0;
}

int rtw_brsc_set_blacklist_enable(int enable)
{
	rtw_brsc.blacklist_enable = enable;

	return 0;
}

int rtw_brsc_is_black_dev(char *name)
{
	int i;

	if(!rtw_brsc.blacklist_enable)
		return 0;

	for(i = 0; i < RTW_BRSC_MAX_BLACK_DEV_NUM; i++)
	{
		if(rtw_brsc.dev_blacklist[i].valid && !strcmp(rtw_brsc.dev_blacklist[i].ifname, name))
			return 1;
	}
	return 0;
}

int rtw_brsc_add_black_dev(char *name)
{
	int i;
	int valid_index = -1;

	if(!name)
		return -1;

	for(i = 0; i < RTW_BRSC_MAX_BLACK_DEV_NUM; i++)
	{
		if(rtw_brsc.dev_blacklist[i].valid)
		{
			if(!strcmp(rtw_brsc.dev_blacklist[i].ifname, name))
				return 0;
		}
		else
		{
			if(valid_index < 0)
			{
				valid_index = i;
			}
		}
	}
	if(valid_index >= 0)
	{
		memcpy(rtw_brsc.dev_blacklist[valid_index].ifname, name, strlen(name));
		rtw_brsc.dev_blacklist[valid_index].valid = 1;
	}
	else
	{
		return -1;
	}

	return 0;
}

int rtw_brsc_del_black_dev(char *name)
{
	int i;

	if(!name)
		return -1;

	for(i = 0; i < RTW_BRSC_MAX_BLACK_DEV_NUM; i++)
	{
		if(rtw_brsc.dev_blacklist[i].valid && !strcmp(rtw_brsc.dev_blacklist[i].ifname, name))
		{
			memset(rtw_brsc.dev_blacklist[i].ifname, 0, IFNAMSIZ);
			rtw_brsc.dev_blacklist[i].valid = 0;
			break;
		}
	}

	return 0;
}

int rtw_brsc_flush_black_dev(void)
{
	memset(&rtw_brsc.dev_blacklist[0], 0, sizeof(struct rtw_brsc_dev_blacklist) * RTW_BRSC_MAX_BLACK_DEV_NUM);

	return 0;
}

void rtw_brsc_disable(void)
{
	rtw_brsc.disable = 1;
}

void rtw_brsc_enable(void)
{
	rtw_brsc.disable = 0;
}

unsigned char rtw_brsc_get_disable(void)
{
	return rtw_brsc.disable;
}

void rtw_brsc_dump(struct seq_file *s, void *v)
{
	int i;
	struct rtw_brsc_entry *entry;

	seq_printf(s,  "brsc disable: %d\n", rtw_brsc.disable);
	seq_printf(s,  "br0 mac address: %pM\n", rtw_brsc.br0_mac);
	for(i=0;i<RTW_MAX_BRSC_INFO;i++)
	{
		if(rtw_brsc.brsc_info[i].dev)
			seq_printf(s,  "entry[%d]: mac %pM, dev_name %s\n", i,rtw_brsc.brsc_info[i].mac, rtw_brsc.brsc_info[i].dev->name);
	}

	for(i=0;i<RTW_MAX_BRSC_HASH;i++){
		hlist_for_each_entry(entry,&rtw_brsc.brsc_hash[i], hnode)
		{
			if(entry->dev)
				seq_printf(s,  "hast list %d: mac->%pM, dev_name %s\n", i, entry->mac, entry->dev->name);
		}
	}
	seq_printf(s,  "brsc blacklist_enable: %d\n", rtw_brsc.blacklist_enable);
	for(i=0;i<RTW_BRSC_MAX_BLACK_DEV_NUM;i++)
	{
		seq_printf(s,  "dev_blocklist[%d]: valid: %d, ifname: %s\n", i, rtw_brsc.dev_blacklist[i].valid, rtw_brsc.dev_blacklist[i].ifname);
	}
	return;
}

EXPORT_SYMBOL(rtw_flush_brsc_info);
EXPORT_SYMBOL(rtw_get_brsc_dev);
EXPORT_SYMBOL(rtw_update_brsc_info);
