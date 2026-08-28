#ifndef _RTW_CROSSBAND_REPEATER_H_
#define _RTW_CROSSBAND_REPEATER_H_

#define CROSSBAND_2G 0
#define CROSSBAND_5G 1

struct crossband_devinfo {
	struct net_device 	*dev_2G;
	unsigned char 		dev_mac_2G[ETH_ALEN];
	struct net_device 	*dev_5G;
	unsigned char 		dev_mac_5G[ETH_ALEN];
};

struct crossband_dev_status {
	unsigned char		crossband_pathReady;
	unsigned char		crossband_assoc;
	unsigned char		crossband_prefer;
};

typedef int (*CROSSBAND_CALLBACK)(struct sk_buff *skb, struct net_device *dev);

int crossband_dev_register(struct net_device *dev, unsigned char *mac, int band, CROSSBAND_CALLBACK func);
int crossband_dev_unregister(struct net_device *dev, int band);
int crossband_check_assoc(int band);
int crossband_check_prefer(int band);
int crossband_dev_xmit(struct sk_buff *skb, int band);
int crossband_check_loopback(unsigned char *src_mac, int band);

#endif
