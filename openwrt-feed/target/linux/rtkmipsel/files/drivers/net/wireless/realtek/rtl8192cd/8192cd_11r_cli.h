
#ifndef	_8192CD_11R_H_
#define _8192CD_11R_H_


#ifdef CONFIG_IEEE80211R_CLI_DEBUG
#define FT_CLI_DEBUG(fmt, args...) panic_printk("[%s %d]"fmt,__FUNCTION__,__LINE__,## args)
#else
#define FT_CLI_DEBUG(fmt, args...) {}
#endif

#define MDID_IE_LEN 		5	
#define FT_IE_LEN			119
#define FT_PMK_ID_LEN		18

#define FT_AUTH_REQ_TIME	60  // can be modified

enum _FT_OP_MODE_ {
	FT_DISABLE			= -1,
	FT_MENUAL_MODE 	= 1,
	FT_AUTO_MODE 		= 2,
};

#ifdef CONFIG_IEEE80211R_CLI
extern void setClientFTCapability(struct rtl8192cd_priv *priv, struct bss_desc *bss_target);
extern void parse_FTIE(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *p, int len);
extern void setFTMDIEData(OCTET_STRING EapolKeyMsgSend, struct rtl8192cd_priv *priv);
extern void setFTIEData(OCTET_STRING EapolKeyMsgSend, struct rtl8192cd_priv *priv, struct stat_info *pstat);
extern void setFTPMKIDData(OCTET_STRING EapolKeyMsgSend, struct rtl8192cd_priv *priv, struct stat_info *pstat);
extern void FT_EventTimer(struct rtl8192cd_priv *priv);
extern void update_ft_bss(struct rtl8192cd_priv *priv, struct stat_info *pstat);
extern void start_fast_bss_transition(struct rtl8192cd_priv *priv);
#endif 

#endif
