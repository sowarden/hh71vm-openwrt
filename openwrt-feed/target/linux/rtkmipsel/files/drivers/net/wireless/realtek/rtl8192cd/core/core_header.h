#ifndef _CORE_HEADERS_H_
#define _CORE_HEADERS_H_

#ifdef __KERNEL__
#include <linux/wireless.h>
#endif

#ifdef __ECOS
#include <pkgconf/system.h>
#include <pkgconf/devs_eth_rltk_819x_wrapper.h>
#include <sys/socket.h>
#include <cyg/io/eth/rltk/819x/wrapper/wireless.h>
#endif

#include "./8192cd_tx.h"

#if defined(CONFIG_RTL_88E_SUPPORT) || defined(CONFIG_RTL_8812_SUPPORT ) || defined(CONFIG_RTL_8723B_SUPPORT)
#include "HalPwrSeqCmd.h"
#endif

#if defined(CONFIG_RTL_TRIBAND_SUPPORT) && defined(CONFIG_USB_HCI)
#if defined(CONFIG_RTL_92E_SUPPORT)
	#include "./WlanHAL/RTL88XX/RTL8192E/RTL8192EU/Hal8192EUDef.h"
#elif defined(CONFIG_RTL_92F_SUPPORT)
	#include "./WlanHAL/RTL88XX/RTL8192F/RTL8192FU/Hal8192FUDef.h"
#else
    #error "triband undefined!!"
#endif
#endif /* defined(CONFIG_RTL_TRIBAND_SUPPORT) && defined(CONFIG_USB_HCI) */

#ifdef CONFIG_WLAN_HAL_8192EE
#if defined(CONFIG_PCI_HCI)
#include "./WlanHAL/RTL88XX/RTL8192E/RTL8192EE/Hal8192EEDef.h"
#endif
#if defined(CONFIG_SDIO_HCI)
#include "./WlanHAL/RTL88XX/RTL8192E/RTL8192ES/Hal8192ESDef.h"
#endif
#endif

#ifdef CONFIG_WLAN_HAL_8814AE
#include "./WlanHAL/RTL88XX/RTL8814A/RTL8814AE/Hal8814AEDef.h"
#endif

#ifdef TAROKO_0
//no Hal8814BEDef.h
#else
#ifdef CONFIG_WLAN_HAL_8814BE
#include "./WlanHAL/RTL88XX/RTL8814B/RTL8814BE/Hal8814BEDef.h"
#endif
#endif

#ifdef CONFIG_WLAN_HAL_8881A
#include "./WlanHAL/RTL88XX/RTL8881A/RTL8881AE/Hal8881AEDef.h"
#endif

#ifdef CONFIG_WLAN_HAL_8197F
#include "./WlanHAL/RTL88XX/RTL8197F/RTL8197FE/Hal8197FEDef.h"
#endif

#ifdef TAROKO_0
//no Hal8198FEDef.h
#else
#ifdef CONFIG_WLAN_HAL_8198F
#include "./WlanHAL/RTL88XX/RTL8198F/RTL8198FE/Hal8198FEDef.h"
#endif
#endif

#ifdef CONFIG_WLAN_HAL_8822BE
#if defined(CONFIG_PCI_HCI)
#include "./WlanHAL/RTL88XX/RTL8822B/RTL8822BE/Hal8822BEDef.h"
#elif defined(CONFIG_SDIO_HCI)
#include "./WlanHAL/RTL88XX/RTL8822B/RTL8822BS/Hal8822BSDef.h"
#endif
#endif

#ifdef CONFIG_WLAN_HAL_8822CE
#if defined(CONFIG_PCI_HCI)
#include "./WlanHAL/RTL88XX/RTL8822C/RTL8822CE/Hal8822CEDef.h"
#elif defined(CONFIG_SDIO_HCI)
#include "./WlanHAL/RTL88XX/RTL8822C/RTL8822CS/Hal8822CSDef.h"
#endif
#endif

#ifdef CONFIG_WLAN_HAL_8812FE
#if defined(CONFIG_PCI_HCI)
#include "./WlanHAL/RTL88XX/RTL8812F/RTL8812FE/Hal8812FEDef.h"
#elif defined(CONFIG_SDIO_HCI)
#include "./WlanHAL/RTL88XX/RTL8812F/RTL8812FS/Hal8812FSDef.h"
#endif
#endif

#ifdef CONFIG_WLAN_HAL_8821CE
#if defined(CONFIG_PCI_HCI)
#include "./WlanHAL/RTL88XX/RTL8821C/RTL8821CE/Hal8821CEDef.h"
#elif defined(CONFIG_SDIO_HCI)
#include "./WlanHAL/RTL88XX/RTL8821C/RTL8821CS/Hal8821CSDef.h"
#endif
#endif

#ifdef TAROKO_0
//no Hal8814BEDef.h
#else
#ifdef CONFIG_WLAN_HAL_8814BE
#include "./WlanHAL/RTL88XX/RTL8814B/RTL8814BE/Hal8814BEDef.h"
#endif
#endif

#if (BEAMFORMING_SUPPORT == 1)
#include "Beamforming.h"
#endif

#ifdef __KERNEL__
#ifdef SMP_SYNC
#include <linux/interrupt.h>
#endif
#endif

#ifdef CONFIG_IEEE80211V
#include "./8192cd_11v.h"
#endif

extern UINT8 dot11_rate_table[];

#define rtl8192cd_firetx(PRIV, TXCFG) \
	priv->hci_ops->firetx(PRIV, TXCFG)
	
#define process_dzqueue(PRIV) \
	priv->hci_ops->process_dzqueue(PRIV)
	
#define __rtl8192cd_start_xmit(SKB, DEV, TX_FLAG) \
	priv->hci_ops->start_xmit(SKB, DEV, TX_FLAG)


#ifdef ROMEPERF_SUPPORT
#include "../../rtl819x/m24kctrl.h"
#define STARTROMEPERF(priv, index) if(priv->pshare->rf_ft_var.perf_index==index) startCP3Ctrl(0)
#define STOPROMEPERF(priv, record, index) if(priv->pshare->rf_ft_var.perf_index==index) stopCP3Ctrl(record, index)
#else
#define STARTROMEPERF(priv, index)
#define STOPROMEPERF(priv, record, index)
#endif

#ifdef CONFIG_RTL_OFFLOAD_DRIVER
extern __IRAM_IN_865X int rtl88XX_signin_offload_htxdesc(struct rtl8192cd_priv *priv, struct tx_insn *txcfg);
#ifdef TX_SHORTCUT
extern __IRAM_IN_865X int rtl88XX_signin_offload_htxdesc_shortcut(struct rtl8192cd_priv *priv, struct tx_insn *txcfg, struct tx_sc_entry *sc);
#endif
#endif

extern bool is_veriwave_testing (struct rtl8192cd_priv *priv);

#ifdef WIFI_WMM
extern void issue_ADDBAreq(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char TID);
#endif
/*8192cd_util.c*/
extern int skb_p80211_to_ether(struct net_device *dev, int wep_mode, struct rx_frinfo *pfrinfo);
/*============*/

/*8192cd_sme.c*/
extern void pwr_state(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo);
/*============*/

extern void setSTABitMap(STA_BITMAP *map, int bitIdx);

extern int free_stainfo(struct rtl8192cd_priv *priv, struct	stat_info *pstat);

extern void issue_deauth(struct rtl8192cd_priv *priv,	unsigned char *da, int reason);


extern int DOT11_Indicate_MIC_Failure(struct net_device *dev, struct stat_info *pstat);


extern unsigned int get_mcast_encrypt_algthm(struct rtl8192cd_priv *priv);


extern int tkip_rx_mic(struct rtl8192cd_priv *priv, unsigned char *pframe, unsigned char *da,
				unsigned char *sa, unsigned char priority, unsigned char *pbuf, unsigned int len,
				unsigned char *tkipmic, int no_wait);

extern void tkip_icv(unsigned char *picv, unsigned char *frag1, unsigned int frag1_len,
				unsigned char *frag2,unsigned int frag2_len);
extern void tkip_encrypt(struct rtl8192cd_priv *priv, unsigned char *pwlhdr,
				unsigned int hdrlen, unsigned char *frag1, unsigned int frag1_len,
				unsigned char *frag2, unsigned int frag2_len, unsigned char *frag3,
				unsigned int frag3_len);
extern unsigned int tkip_decrypt(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo,unsigned int fr_len);

extern unsigned int wep_decrypt(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo,
				unsigned int fr_len, int type, int keymap);
extern void wep_encrypt(struct rtl8192cd_priv *priv, unsigned char *pwlhdr, unsigned int hdrlen,
				unsigned char *frag1, unsigned int frag1_len,
				unsigned char *frag2, unsigned int frag2_len,
				unsigned char *frag3, unsigned int frag3_len,
				int type);

extern void aesccmp_encrypt(struct rtl8192cd_priv *priv, unsigned char *pwlhdr,
				unsigned char *frag1,unsigned char *frag2, unsigned int frag2_len,
				unsigned char *frag3
#ifdef CONFIG_IEEE80211W				
				, unsigned char isMgmt
#endif
			);
extern unsigned int aesccmp_decrypt(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo
#ifdef CONFIG_IEEE80211W				
				, unsigned char isMgmt
#endif	
			);




#if defined(AP_SWPS_OFFLOAD)
extern int enque(struct rtl8192cd_priv *priv,  struct apsd_pkt_queue* AC_dz_queue, int *head, int *tail, unsigned long ffptr, int ffsize, void *elm);
#else
extern int enque(struct rtl8192cd_priv *priv, int *head, int *tail, unsigned long ffptr, int ffsize, void *elm);
#endif

#if defined(AP_SWPS_OFFLOAD)
extern void* deque(struct rtl8192cd_priv *priv, struct apsd_pkt_queue* AC_dz_queue, struct reprepare_info* pkt_info, int *head, int *tail, unsigned long ffptr, int ffsize);
#else
extern void* deque(struct rtl8192cd_priv *priv, int *head, int *tail, unsigned long ffptr, int ffsize);
#endif




#endif
