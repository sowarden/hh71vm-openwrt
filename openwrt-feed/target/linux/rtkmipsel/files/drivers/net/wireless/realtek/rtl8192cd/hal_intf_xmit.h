/*
 *  Header files defines some SDIO inline routines
 *
 *  Copyright (c) 2017 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef _HAL_INTF_XMIT_H_
#define _HAL_INTF_XMIT_H_

struct tx_insn;
#ifdef TX_SHORTCUT
struct tx_sc_entry;
#endif
//#include "./8192cd.h"

enum {
	XMIT_DECISION_CONTINUE = 0,
	XMIT_DECISION_ENQUEUE,
	XMIT_DECISION_STOP
};

#ifdef WMM_APSD
extern const unsigned char wmm_apsd_bitmask[5];
#endif

#ifdef TX_SHORTCUT
#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
int get_tx_sc_index_usb(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *hdr, unsigned char pktpri);
int get_tx_sc_free_entry_usb(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *hdr, unsigned char pktpri);
#else
int get_tx_sc_index(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *hdr, unsigned char pktpri);
int get_tx_sc_free_entry(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *hdr, unsigned char pktpri);
#endif /* defined(CONFIG_RTL_TRIBAND_SUPPORT) */
#endif //TX_SHORTCUT

u32 rtw_is_tx_queue_empty(struct rtl8192cd_priv *priv, struct tx_insn *txcfg);
int rtw_xmit_enqueue(struct rtl8192cd_priv *priv, struct tx_insn *txcfg);
void rtw_handle_xmit_fail(struct rtl8192cd_priv *priv, struct tx_insn *txcfg);
int rtw_xmit_decision(struct rtl8192cd_priv *priv, struct tx_insn *txcfg);

int dz_queue_usb(struct rtl8192cd_priv *priv, struct stat_info *pstat, struct tx_insn *txcfg);
int update_txinsn_stage1(struct rtl8192cd_priv *priv, struct tx_insn* txcfg);
int update_txinsn_stage2(struct rtl8192cd_priv *priv, struct tx_insn* txcfg);
#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
int rtl8192cd_signin_txdesc_usb(struct rtl8192cd_priv *priv, struct tx_insn* txcfg, struct wlan_ethhdr_t *pethhdr);
#ifdef TX_SHORTCUT
int rtl8192cd_signin_txdesc_shortcut_usb(struct rtl8192cd_priv *priv, struct tx_insn *txcfg, struct tx_sc_entry *ptxsc_entry);
#endif
#else /* !defined(CONFIG_RTL_TRIBAND_SUPPORT) */
int rtl8192cd_signin_txdesc(struct rtl8192cd_priv *priv, struct tx_insn* txcfg, struct wlan_ethhdr_t *pethhdr);
#ifdef TX_SHORTCUT
int rtl8192cd_signin_txdesc_shortcut(struct rtl8192cd_priv *priv, struct tx_insn *txcfg, struct tx_sc_entry *ptxsc_entry);
#endif
#endif /* defined(CONFIG_RTL_TRIBAND_SUPPORT) */

#ifdef CONFIG_NETDEV_MULTI_TX_QUEUE
#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
void rtl8192cd_tx_restartQueue_usb(struct rtl8192cd_priv *priv, unsigned int index);
#else
void rtl8192cd_tx_restartQueue(struct rtl8192cd_priv *priv, unsigned int index);
#endif /* defined(CONFIG_RTL_TRIBAND_SUPPORT) */T
#else
void rtl8192cd_tx_restartQueue(struct rtl8192cd_priv *priv);
#endif //CONFIG_NETDEV_MULTI_TX_QUEUE
void rtl8192cd_tx_stopQueue(struct rtl8192cd_priv *priv);

void stop_sta_xmit(struct rtl8192cd_priv *priv, struct stat_info *pstat);
void wakeup_sta_xmit(struct rtl8192cd_priv *priv, struct stat_info *pstat);
void process_APSD_dz_queue_usb(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned short tid);

#ifdef SDIO_AP_OFFLOAD
void ap_offload_process(struct rtl8192cd_priv *priv, unsigned int frlen);
#endif

u8* download_rsvd_page(struct rtl8192cd_priv *priv, u8* pbuf, u32 len);

#endif // _HAL_INTF_XMIT_H_

