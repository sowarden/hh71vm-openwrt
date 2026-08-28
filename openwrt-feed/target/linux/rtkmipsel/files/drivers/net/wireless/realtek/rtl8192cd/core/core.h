#ifndef	_CORE_H_
#define _CORE_H_

#ifdef __KERNEL__
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <../net/bridge/br_private.h>
#endif

#ifdef HS2_SUPPORT
#include <linux/in6.h>
#endif

#ifdef __DRAYTEK_OS__
#include <draytek/softimer.h>
#include <draytek/skbuff.h>
#include <draytek/wl_dev.h>
#endif

#include "./8192cd_cfg.h"

#if !defined(__KERNEL__) && !defined(__ECOS)
#include "./sys-support.h"
#endif

#ifdef __ECOS
#include <cyg/io/eth/rltk/819x/wrapper/sys_support.h>
#include <cyg/io/eth/rltk/819x/wrapper/skbuff.h>
#include <cyg/io/eth/rltk/819x/wrapper/timer.h>
#ifdef CONFIG_RTL_ALP
#include <cyg/io/eth/rltk/819x/wlan/hw_mib.h>
#endif
#include <netinet/in.h>
#endif

#ifdef SUPPORT_SNMP_MIB
#include "./8192cd_mib.h"
#endif

#ifdef P2P_SUPPORT
#include "./8192cd_p2p.h"
#endif

#ifdef     CONFIG_WLAN_HAL
//#include "WlanHAL/Output/HalLib.h"

//#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
enum _HW_TX_SHORTCUT_ {
	// for header conversion ( 802.3 -> 802.11 )
	HW_TX_SC_NORMAL 		= 0,
	HW_TX_SC_BACKUP_HEADER 	= 1,
	HW_TX_SC_HEADER_CONV 	= 2,
};
//#endif
#endif


#ifdef SUCCESS
#undef SUCCESS
#endif
#define SUCCESS		1
#define FAIL		0

#define CONGESTED	2
#define MANAGEMENT_AID	0
#define DEFAULT_RTS_THRESHOLD  2347

#if defined(WIFI_QOS_ENHANCE)
#define QOS_ENHANCE_ENABLE(priv) (priv->pshare->rf_ft_var.qos_enhance_enable)
#define QOS_ENHANCE_RSSI_LOW_THRESHOLD(priv) (priv->pshare->rf_ft_var.qos_enhance_rssi_low_thd)
#define QOS_ENHANCE_RSSI_HIGH_THRESHOLD(priv) (priv->pshare->rf_ft_var.qos_enhance_rssi_high_thd)
#define QOS_ENHANCE_TP_LOW_THRESHOLD(priv) (priv->pshare->rf_ft_var.qos_enhance_tp_low_thd)
#define QOS_ENHANCE_TP_MIDDLE_THRESHOLD(priv) (priv->pshare->rf_ft_var.qos_enhance_tp_middle_thd)
#define QOS_ENHANCE_TP_HIGH_THRESHOLD(priv) (priv->pshare->rf_ft_var.qos_enhance_tp_high_thd)
#define QOS_ENHANCE_RSSI_EDGE_THRESHOLD(priv) (priv->pshare->rf_ft_var.qos_enhance_rssi_edge_thd)
#endif


// for packet aggregation
#define FG_AGGRE_MPDU			1
#define FG_AGGRE_MPDU_BUFFER_FIRST	2
#define FG_AGGRE_MPDU_BUFFER_MID	3
#define FG_AGGRE_MPDU_BUFFER_LAST	4
#define FG_AGGRE_MSDU_FIRST		5
#define FG_AGGRE_MSDU_MIDDLE		6
#define FG_AGGRE_MSDU_LAST		7

#ifdef NETDEV_NO_PRIV
	#define GET_DEV_PRIV(dev) ((struct rtl8192cd_priv *)netdev_priv(dev))->wlan_priv
#else
	#define GET_DEV_PRIV(dev) (struct rtl8192cd_priv *)dev->priv
#endif

#if !defined(__KERNEL__) 
#define GET_BR_PORT(netdev)	(netdev)->br_port
#else
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,35)
#define GET_BR_PORT(netdev)	(netdev)->br_port
#else
#define GET_BR_PORT(netdev)	br_port_get_rcu(netdev)
#endif
#endif

#define BSSID		((GET_MIB(priv))->dot11StationConfigEntry.dot11Bssid)
#define IEEE8021X_FUN	((GET_MIB(priv))->dot118021xAuthEntry.dot118021xAlgrthm)
#define FRAGTHRSLD		((GET_MIB(priv))->dot11OperationEntry.dot11FragmentationThreshold)
#define RTSTHRSLD		((GET_MIB(priv))->dot11OperationEntry.dot11RTSThreshold)
#define IS_UDP_PROTO(mac)	((mac[12]==0x08) && (mac[13]==0x00) && (mac[23]==0x11))

#define GET_GROUP_ENCRYP_KEYLEN			((GET_MIB(priv))->dot11GroupKeysTable.dot11EncryptKey.dot11TTKeyLen)
#define GET_GROUP_MIC_KEYLEN	((GET_MIB(priv))->dot11GroupKeysTable.dot11EncryptKey.dot11TMicKeyLen)
#define GET_GROUP_IDX2_MIC_KEYLEN	((GET_MIB(priv))->dot11GroupKeysTable.dot11EncryptKey2.dot11TMicKeyLen)
#define GET_GROUP_TKIP_MIC1_KEY	((GET_MIB(priv))->dot11GroupKeysTable.dot11EncryptKey.dot11TMicKey1.skey)
#define GET_GROUP_TKIP_MIC2_KEY	((GET_MIB(priv))->dot11GroupKeysTable.dot11EncryptKey.dot11TMicKey2.skey)
#define GET_GROUP_TKIP_IDX2_MIC1_KEY	((GET_MIB(priv))->dot11GroupKeysTable.dot11EncryptKey2.dot11TMicKey1.skey)
#define GET_GROUP_TKIP_IDX2_MIC2_KEY	((GET_MIB(priv))->dot11GroupKeysTable.dot11EncryptKey2.dot11TMicKey2.skey)

#define GET_UNICAST_MIC_KEYLEN		(pstat->dot11KeyMapping.dot11EncryptKey.dot11TMicKeyLen)
#define GET_UNICAST_ENCRYP_KEYLEN		(pstat->dot11KeyMapping.dot11EncryptKey.dot11TTKeyLen)
#define GET_UNICAST_TKIP_MIC1_KEY	(pstat->dot11KeyMapping.dot11EncryptKey.dot11TMicKey1.skey)
#define GET_UNICAST_TKIP_MIC2_KEY	(pstat->dot11KeyMapping.dot11EncryptKey.dot11TMicKey2.skey)


#define GET_MIB(priv)		(priv->pmib)
#define GET_HW(priv)		(priv->pshare->phw)

#define GET_GROUP_ENCRYP_PN	(&((GET_MIB(priv))->dot11GroupKeysTable.dot11EncryptKey.dot11TXPN48))
#define GET_UNICAST_ENCRYP_PN	(&(pstat->dot11KeyMapping.dot11EncryptKey.dot11TXPN48))

#define IS_DRV_OPEN(priv) ((priv==NULL) ? 0 : ((priv->drv_state & DRV_STATE_OPEN) ? 1 : 0))
/*match is  (1)ipv4 && (2)(IGMP control/management packet) */
#define IS_IGMP_PROTO(mac)	((mac[12]==0x08) && (mac[13]==0x00) && (mac[23]==0x02))
#define IP_MCAST_MAC(mac)		((mac[0]==0x01)&&(mac[1]==0x00)&&(mac[2]==0x5e))
#define IS_1905_MCAST(mac)  ((mac[0] == 0x01)&&(mac[1] == 0x80)&&(mac[2] == 0xc2)&&(mac[3] == 0x00)&&(mac[4] == 0x00)&&(mac[5] == 0x13))

#define MAX_DEAUTH_STA_NUM  4
#define MANAGE_QUE_NUM		MGNT_QUEUE
#define RX_BUF_LEN		((GET_MIB(priv))->dot11nConfigEntry.dot11nAMSDURecvMax?((priv->pshare->wlandev_idx == (0^WLANIDX))?(MAX_RX_BUF_LEN):(MAX_RX_BUF_LEN_1)):((priv->pshare->wlandev_idx == (0^WLANIDX))?(MIN_RX_BUF_LEN):(MIN_RX_BUF_LEN_1)))

#define TSF_LESS(a, b)	(((a - b) & 0x80000000) != 0)
#define TSF_DIFF(a, b)	((a >= b)? (a - b):(0xffffffff - b + a + 1))

#ifdef GBWC
#define GBWC_MODE_DISABLE			0
#define GBWC_MODE_LIMIT_MAC_INNER	1 // limit bw by mac address
#define GBWC_MODE_LIMIT_MAC_OUTTER	2 // limit bw by excluding the mac
#define GBWC_MODE_LIMIT_IF_TX		3 // limit bw by interface tx
#define GBWC_MODE_LIMIT_IF_RX		4 // limit bw by interface rx
#define GBWC_MODE_LIMIT_IF_TRX		5 // limit bw by interface tx/rx
#endif

#ifdef SUPPORT_SNMP_MIB
#define SNMP_MIB(f)					(priv->snmp_mib.f)
#define SNMP_MIB_ASSIGN(f,v)		(SNMP_MIB(f)=v)
#define SNMP_MIB_COPY(f,v,len)		(memcpy(&SNMP_MIB(f), v, len))
#define SNMP_MIB_INC(f,v)			(SNMP_MIB(f)+=v)
#define SNMP_MIB_DEC(f,v)			(SNMP_MIB(f)-=v)
#else
#define SNMP_MIB(f)
#define SNMP_MIB_ASSIGN(f,v)
#define SNMP_MIB_COPY(f,v,len)
#define SNMP_MIB_INC(f,v)
#define SNMP_MIB_DEC(f,v)
#endif //SUPPORT_SNMP_MIB

#ifdef USE_OUT_SRC
#define	ODMPTR					(&(priv->pshare->_dmODM))
#endif

#ifdef MULTI_MAC_CLONE
#if defined(SMP_SYNC)
#define ACTIVE_ID 		(priv->pshare->mclone_active_id[smp_processor_id()])
#else
#define ACTIVE_ID 		(priv->pshare->mclone_active_id)
#endif
#define MCLONE_NUM		(priv->pshare->mclone_num)
#define OPMODE_VXD			(((ACTIVE_ID > 0) && ((GET_MIB((GET_VXD_PRIV(priv))))->dot11OperationEntry.opmode & WIFI_STATION_STATE)) ? priv->pshare->mclone_sta[ACTIVE_ID-1].opmode : (GET_MIB(priv))->dot11OperationEntry.opmode)
#define OPMODE			(((ACTIVE_ID > 0) && ((GET_MIB(priv))->dot11OperationEntry.opmode & WIFI_STATION_STATE)) ? priv->pshare->mclone_sta[ACTIVE_ID-1].opmode : (GET_MIB(priv))->dot11OperationEntry.opmode)

#define OPMODE_VAL(mod) do {\
		if ( (ACTIVE_ID > 0) && ((GET_MIB(priv))->dot11OperationEntry.opmode & WIFI_STATION_STATE)) \
			priv->pshare->mclone_sta[ACTIVE_ID-1].opmode = mod;\
		else\
			(GET_MIB(priv))->dot11OperationEntry.opmode	= mod;\
		} while (0)
#else
#define ACTIVE_ID 		(0)
#define OPMODE_VXD		((GET_MIB((GET_VXD_PRIV(priv))))->dot11OperationEntry.opmode)
#define OPMODE			((GET_MIB(priv))->dot11OperationEntry.opmode)
#define OPMODE_VAL(mod) do {\
		(GET_MIB(priv))->dot11OperationEntry.opmode = mod;\
		} while (0)
#endif


#ifdef WIFI_WMM
#define QOS_ENABLE 		((GET_MIB(priv))->dot11QosEntry.dot11QosEnable)
#define APSD_ENABLE 		((GET_MIB(priv))->dot11QosEntry.dot11QosAPSD)
#define GET_WMM_IE		((GET_MIB(priv))->dot11QosEntry.WMM_IE)
#define GET_WMM_PARA_IE		((GET_MIB(priv))->dot11QosEntry.WMM_PARA_IE)
#define GET_EDCA_PARA_UPDATE 	((GET_MIB(priv))->dot11QosEntry.EDCAparaUpdateCount)
#define GET_STA_AC_BE_PARA	((GET_MIB(priv))->dot11QosEntry.STA_AC_BE_paraRecord)
#define GET_STA_AC_BK_PARA	((GET_MIB(priv))->dot11QosEntry.STA_AC_BK_paraRecord)
#define GET_STA_AC_VI_PARA	((GET_MIB(priv))->dot11QosEntry.STA_AC_VI_paraRecord)
#define GET_STA_AC_VO_PARA	((GET_MIB(priv))->dot11QosEntry.STA_AC_VO_paraRecord)
#if defined(UNIVERSAL_REPEATER) && defined(WIFI_11N_2040_PERMIT_LOGIC)
#define COEXIST_ENABLE		(((IS_DRV_OPEN(GET_VXD_PRIV(GET_ROOT(priv)))) && ((GET_VXD_PRIV(GET_ROOT(priv))->pmib->dot11OperationEntry.opmode & (WIFI_STATION_STATE|WIFI_ASOC_STATE|WIFI_AUTH_SUCCESS)) == (WIFI_STATION_STATE|WIFI_ASOC_STATE|WIFI_AUTH_SUCCESS)))? 0 :(GET_MIB(priv)->dot11nConfigEntry.dot11nCoexist))
#else
#define COEXIST_ENABLE		((GET_MIB(priv))->dot11nConfigEntry.dot11nCoexist)
#endif
#endif

#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
//#define GET_ROOT_PRIV(priv)			(priv->proot_priv)
#define IS_ROOT_INTERFACE(priv)	 	((priv->proot_priv) ? 0 : 1)
#define GET_ROOT(priv)				((priv->proot_priv) ? priv->proot_priv : priv)
#else
#define IS_ROOT_INTERFACE(priv)	 	(1)
#define GET_ROOT(priv)		(priv)
#endif

#define IPV4_TCP_ACK_PACKET(eth_packet)  (eth_packet[23] ==0x06 && eth_packet[12] == 0x08 && eth_packet[13] ==0x00 \
													&&  eth_packet[47] == 0x10 \
													&&  (htons(*(unsigned short *)(eth_packet+16)) == (((eth_packet[14] & 0xF)<<2)+(eth_packet[46]>>4<<2))))

// andrew, define a compatible data macro
#if defined(__ECOS) || defined(TAROKO_0)
// This marco is OK in RX flow, but TX flow need to confirm
#define SKB_MAC_HEADER(s) (s)->data
#elif defined(LINUX_2_6_22_)
#define SKB_MAC_HEADER(s) skb_mac_header(s)
#else // older 2.6 header
#define SKB_MAC_HEADER(s) (s)->mac.raw
#endif


enum _SKB_CB {
	// TX path used
	_SKB_CB_PRIORITY = 1,
	_SKB_CB_FLAGS = 5,
	_SKB_CB_QNUM = 33,
	_SKB_CB_AMSDU_TXSC = 47,
};
#define _SKB_CB_FLAGS_DHCP		BIT0
#define _SKB_CB_FLAGS_URGENT	BIT1
#define _SKB_CB_FLAGS_TCPACK	BIT2

enum SECONDARY_CHANNEL_OFFSET {
	HT_2NDCH_OFFSET_DONTCARE = 0,
	HT_2NDCH_OFFSET_BELOW    = 1,	// secondary channel is below primary channel, ex. primary:5 2nd:1
	HT_2NDCH_OFFSET_ABOVE    = 2	// secondary channel is above primary channel, ex. primary:5 2nd:9
};

enum AGGREGATION_METHOD {
	AGGRE_MTHD_NONE = 0,
	AGGRE_MTHD_MPDU = 1,
	AGGRE_MTHD_MSDU = 2,
	AGGRE_MTHD_MPDU_AMSDU = 3
};

enum wifi_tofr_ds {
	WIFI_FRMCTL_MGNT	= 0,
	WIFI_FRMCTL_FROM_DS	= 1,
	WIFI_FRMCTL_TO_DS	= 2,
	WIFI_FRMCTL_MESH	= 3,
};


enum {
	DRV_STATE_INIT	 = 1,	/* driver has been init */
	DRV_STATE_OPEN	= 2,	/* driver is opened */
#if defined( UNIVERSAL_REPEATER) || defined(MBSSID)
	DRV_STATE_VXD_INIT = 4,	/* vxd driver has been opened */
	DRV_STATE_VXD_AP_STARTED	= 8, /* vxd ap has been started */
#endif
};

enum wifi_state {
	WIFI_NULL_STATE		=	0x00000000,
	WIFI_ASOC_STATE		=	0x00000001,
	WIFI_REASOC_STATE	=	0x00000002,
	WIFI_SLEEP_STATE	=	0x00000004,
	WIFI_STATION_STATE	=	0x00000008,
	WIFI_AP_STATE		=	0x00000010,
	WIFI_ADHOC_STATE	=	0x00000020,
	WIFI_AUTH_NULL		=	0x00000100,
	WIFI_AUTH_STATE1	= 	0x00000200,
	WIFI_AUTH_SUCCESS	=	0x00000400,
	WIFI_SITE_MONITOR	=	0x00000800,		//to indicate the station is under site surveying
#ifdef WDS
	WIFI_WDS			=	0x00001000,
	WIFI_WDS_RX_BEACON	=	0x00002000,		// already rx WDS AP beacon
#ifdef LAZY_WDS
	WIFI_WDS_LAZY			=	0x00004000,
#endif
#endif

#ifdef MP_TEST
	WIFI_MP_STATE					= 0x00010000,
	WIFI_MP_CTX_BACKGROUND			= 0x00020000,	// in continuous tx background
	WIFI_MP_CTX_BACKGROUND_PENDING	= 0x00040000,	// pending in continuous tx background due to out of skb
	WIFI_MP_CTX_PACKET				= 0x00080000,	// in packet mode
	WIFI_MP_CTX_ST					= 0x00100000,	// in continuous tx with single-tone
	WIFI_MP_CTX_SCR					= 0x00200000,	// in continuous tx with single-carrier
	WIFI_MP_CTX_CCK_CS				= 0x00400000,	// in cck continuous tx with carrier suppression
	WIFI_MP_CTX_OFDM_HW				= 0x00800000,	// in ofdm continuous tx
	WIFI_MP_RX							= 0x01000000,
	WIFI_MP_ARX_FILTER				= 0x02000000,
#if 1/*def CONFIG_RTL8672*/
	WIFI_MP_CTX_BACKGROUND_STOPPING	= 0x04000000,	// stopping ctx
#endif
#endif

#ifdef WIFI_SIMPLE_CONFIG
#ifdef INCLUDE_WPS
	WIFI_WPS			=	0x02000000,
	WIFI_WPS_JOIN		=	0x04000000,
#else
	WIFI_WPS			= 0x01000000,
	WIFI_WPS_JOIN		= 0x02000000,
#endif
#endif

#ifdef A4_STA
	WIFI_A4_STA		=	0x04000000,
#endif


#if defined(CONFIG_RTL8672) || defined(__OSK__)
	WIFI_WAIT_FOR_CHANNEL_SELECT    = 0x08000000,
#endif

};


enum {
    RTL_HCI_PCIE,
    RTL_HCI_USB,
    RTL_HCI_SDIO,
};

enum NETWORK_TYPE {
	WIRELESS_11B = 1,
	WIRELESS_11G = 2,
	WIRELESS_11A = 4,
	WIRELESS_11N = 8,
	WIRELESS_11AC = 64
};


//
// Indicate different AP vendor for IOT issue.
//
typedef enum _HT_IOT_PEER {
	HT_IOT_PEER_UNKNOWN 			= 0,
	HT_IOT_PEER_REALTEK 			= 1,
	HT_IOT_PEER_REALTEK_92SE 		= 2,
	HT_IOT_PEER_BROADCOM 			= 3,
	HT_IOT_PEER_RALINK 				= 4,
	HT_IOT_PEER_ATHEROS 			= 5,
	HT_IOT_PEER_CISCO 				= 6,
	HT_IOT_PEER_MERU 				= 7,
	HT_IOT_PEER_MARVELL 			= 8,
	HT_IOT_PEER_REALTEK_SOFTAP 		= 9,  // peer is RealTek SOFT_AP, by Bohn, 2009.12.17
	HT_IOT_PEER_SELF_SOFTAP 		= 10, // Self is SoftAP
	HT_IOT_PEER_AIRGO 				= 11,
	HT_IOT_PEER_INTEL 				= 12,
	HT_IOT_PEER_RTK_APCLIENT 		= 13,
	HT_IOT_PEER_REALTEK_81XX 		= 14,
	HT_IOT_PEER_REALTEK_WOW 		= 15,
	HT_IOT_PEER_HTC 				= 16,
	HT_IOT_PEER_REALTEK_8812		= 17,	
	HT_IOT_PEER_CMW 				= 18,
	HT_IOT_PEER_SPIRENT				= 19,
	HT_IOT_PEER_VERIWAVE			= 20,
	HT_IOT_PEER_APPLE               = 21,
	HT_IOT_PEER_REALTEK_8814        = 22,
	HT_IOT_PEER_NETGEAR				= 23,
	HT_IOT_PEER_MAX
} HT_IOT_PEER_E, *PHTIOT_PEER_E;

#ifdef TCP_ACK_ACC
#define TCP_SESSION_MAX_ENTRY		32
struct tcp_session {
	struct list_head	list;
	unsigned long		src_ip;
	unsigned long		dst_ip;
	unsigned short		src_port;
	unsigned short		dst_port;
	unsigned short		ack_seq;
	unsigned short		used;
	struct sk_buff		*buf_ack;
	unsigned long		timestamp;
	unsigned long		hit;
	unsigned long		boot_time;
	unsigned int		ack_cnt;
};
#endif

#if defined(SW_TX_QUEUE) || defined(RTK_ATM)
#define CHECK_DEC_AGGN		0
#define CHECK_INC_AGGN		1
#define MAX_BACKOFF_CNT		8

struct sw_tx_q {
    int						q_aggnum[8];
    int						q_TOCount[8];
    unsigned char 			q_used[8];
    struct sk_buff_head     swq_queue[8];
    int                     swq_queue_len_max[8];
    int                     swq_queue_xmit_max[8];
    unsigned short          swq_timer_id[8];    
    unsigned char           swq_timer_status[8]; /* 0: success, 1: need to re-set*/
    unsigned char           swq_empty[8];      //0:empty; 1:not empty    
    unsigned int            swq_tri_time[8];    

    /* below is for old swq*/
    unsigned char			q_aggnumIncSlow[8];

    /* below is for new swq*/
    unsigned char           swq_en[8];
    unsigned long           swq_keeptime[8];
    unsigned char           swq_timeout_change[8];        
    unsigned char           swq_prev_timeout[8];     
};
#endif

#if defined(WIFI_WMM) && defined(WMM_APSD)
struct apsd_pkt_queue {
	struct sk_buff	*pSkb[NUM_APSD_TXPKT_QUEUE];
#if defined(AP_SWPS_OFFLOAD)
    unsigned short  swps_seq[NUM_APSD_TXPKT_QUEUE];
    char    amsdu_flag[NUM_APSD_TXPKT_QUEUE]; //if 1, this pskb point to buf_pframe_amsdu, not a single skb
    char    *hdr[NUM_APSD_TXPKT_QUEUE];
    char    *llchdr[NUM_APSD_TXPKT_QUEUE];
    char    *icv[NUM_APSD_TXPKT_QUEUE];
    char    *mic[NUM_APSD_TXPKT_QUEUE];
    //unsigned char       pkt_txbd_qnum;
    //unsigned int        pkt_txbd_idx;
    //unsigned int        pkt_txdesc_addr;
    
    //to record txbd needed info
    unsigned int hdr_len[NUM_APSD_TXPKT_QUEUE];
    unsigned int skbbuf_len[NUM_APSD_TXPKT_QUEUE];
    unsigned int mic_icv_len[NUM_APSD_TXPKT_QUEUE];
    unsigned int PSB_len[NUM_APSD_TXPKT_QUEUE];
    struct TX_DESC_88XX *ptxdesc[NUM_APSD_TXPKT_QUEUE]; //is the final filled ok txdesc
#ifdef SUPPORT_TX_AMSDU    
    unsigned int amsdubuf_len[NUM_APSD_TXPKT_QUEUE][WLAN_HAL_TX_AMSDU_MAX_NUM];
    unsigned int amsdu_num[NUM_APSD_TXPKT_QUEUE];
#endif    
    unsigned char isWiFiHdr[NUM_APSD_TXPKT_QUEUE];
#ifdef SUPPORT_TXDESC_IE
    unsigned char IE_bitmap[NUM_APSD_TXPKT_QUEUE];
#endif
#endif
	int				head;
	int				tail;
};
#endif



#if defined(__KERNEL__)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
/* Porting from linux kernel, for compatible with old kernel. */
static inline unsigned char *skb_tail_pointer(const struct sk_buff *skb)
{
	return skb->tail;
}

static inline void skb_reset_tail_pointer(struct sk_buff *skb)
{
	skb->tail = skb->data;
}

static inline void skb_set_tail_pointer(struct sk_buff *skb, const int offset)
{
	skb->tail = skb->data + offset;
}

static inline unsigned char *skb_end_pointer(const struct sk_buff *skb)
{
	return skb->end;
}
#endif
#elif defined(TAROKO_0)
#elif !defined(__OSK__)
static inline unsigned char *skb_tail_pointer(const struct sk_buff *skb)
{
	return skb->tail;
}

static inline void skb_reset_tail_pointer(struct sk_buff *skb)
{
	skb->tail = skb->data;
}

static inline void skb_set_tail_pointer(struct sk_buff *skb, const int offset)
{
	skb->tail = skb->data + offset;
}

static inline unsigned char *skb_end_pointer(const struct sk_buff *skb)
{
	return skb->end;
}
#endif


#endif 
