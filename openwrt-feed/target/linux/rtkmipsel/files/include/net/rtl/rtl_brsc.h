#ifndef _RTL_BRSC_H
#define _RTL_BRSC_H
#define RTW_BRSC_DROP 		NET_RX_DROP
#define RTW_BRSC_SUCCESS 	NET_RX_SUCCESS
#define RTW_BRSC_CONTINUE 	(3)
#define RTW_BRSC_MAX_BLACK_DEV_NUM 20

int rtw_brsc_process_vlan(struct net_device *dev, struct sk_buff **pskb);
void rtw_brsc_init(void);
void rtw_flush_brsc_info(struct net_device *dev, unsigned char *mac);
void rtw_update_brsc_info(unsigned char *mac, struct net_device *dev);
#ifdef CONFIG_RTW_BRSC
int rtw_brsc_go(struct sk_buff *skb, struct net_device *src_dev);
#endif
struct net_device* rtw_get_brsc_dev(unsigned char *dmac);
unsigned char rtw_brsc_get_disable(void);
int rtw_brsc_is_black_dev(char *name);

#endif	/* _RTL_BRSC_H */
