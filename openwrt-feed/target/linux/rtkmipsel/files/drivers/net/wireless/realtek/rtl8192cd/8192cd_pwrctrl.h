
#if defined(CONFIG_POWER_SAVE) || defined(CONFIG_PCIE_POWER_SAVING)
#include "8192cd.h"
#define EXTERN  extern
#endif

#ifdef CONFIG_POWER_SAVE
#define REG_VALUE_32K	0xEAEAEAEA

EXTERN int rtw_sdio_prepare(struct device *dev);
EXTERN void rtw_sdio_complete(struct device *dev);
EXTERN int rtw_sdio_suspend(struct device *dev);
EXTERN int __rtw_sdio_resume(struct rtl8192cd_priv *priv);
EXTERN int rtw_sdio_resume(struct device *dev);

EXTERN int  init_wifi_wakeup_gpio(struct net_device *dev, struct rtl8192cd_priv *priv);
EXTERN void  free_wifi_wakeup_gpio(struct net_device *dev, struct rtl8192cd_priv *priv);

EXTERN void rtw_ap_stop_fw_ps(struct rtl8192cd_priv *priv);
EXTERN void rtw_ap_start_fw_ps(struct rtl8192cd_priv *priv, u4Byte en_32k, u4Byte reason);
#ifdef CONFIG_RTL_88E_SUPPORT
EXTERN void set_ap_offload(struct rtl8192cd_priv *priv, unsigned int deny_ap, unsigned int hid_ap, int enable, unsigned int linked);
EXTERN void set_repeat_wake_pulse(struct rtl8192cd_priv *priv, unsigned char en, unsigned char triggerTime, unsigned char duration);
EXTERN void set_bcn_resv_page(struct rtl8192cd_priv *priv, unsigned int rootap, unsigned int vap1, unsigned int vap2);
EXTERN void set_probe_res_resv_page(struct rtl8192cd_priv *priv, unsigned int rootap, unsigned int vap1, unsigned int vap2);
EXTERN void set_wakeup_pin(struct rtl8192cd_priv *priv, unsigned char duration, unsigned char en, unsigned char pull_high, unsigned char pulse, unsigned char pin);
EXTERN void set_softap_ps(struct rtl8192cd_priv *priv, u8 enable, u8 en_32K, u8 lps, u8 duration);
#endif
EXTERN void rtw_suspend_lock_init(void);
EXTERN void rtw_suspend_lock_deinit(void);
EXTERN void rtw_lock_suspend(struct rtl8192cd_priv *priv);
EXTERN void rtw_unlock_suspend(struct rtl8192cd_priv *priv);
EXTERN void rtw_lock_suspend_timeout(struct rtl8192cd_priv *priv, unsigned int timeout);
//EXTERN void rtw_ap_set_wake_lock(struct rtl8192cd_priv *priv, u1Byte level, u4Byte time_ms);
EXTERN int rtw_ap_ps_xmit_monitor(struct rtl8192cd_priv *priv);
EXTERN void rtw_ap_ps_recv_monitor(struct rtl8192cd_priv *priv);
EXTERN void rtw_ap_ps_init(struct rtl8192cd_priv *priv);
EXTERN void rtw_ap_ps_deinit(struct rtl8192cd_priv *priv);

EXTERN void ClearSdioInterrupt(struct rtl8192cd_priv *priv);
EXTERN void set_ap_32k(struct rtl8192cd_priv *priv, BOOLEAN bRpwmCfg);

EXTERN void rtw_flush_xmit_pending_queue(struct rtl8192cd_priv *priv);
EXTERN void rtw_flush_all_tx_mgt_queue(struct rtl8192cd_priv *priv);

EXTERN void sdio_power_save_timer(unsigned long task_priv);

#endif // CONFIG_POWER_SAVE

#if defined(CONFIG_PCIE_POWER_SAVING)
EXTERN void rtw_ap_start_fw_ps(struct rtl8192cd_priv *priv, u4Byte en_32k, u4Byte reason);
EXTERN void rtw_ap_stop_fw_ps(struct rtl8192cd_priv *priv);
EXTERN void set_ap_32k(struct rtl8192cd_priv *priv, BOOLEAN bRpwmCfg);
#endif

#if defined(CONFIG_POWER_SAVE) || defined(CONFIG_PCIE_POWER_SAVING)
#undef EXTERN
#endif