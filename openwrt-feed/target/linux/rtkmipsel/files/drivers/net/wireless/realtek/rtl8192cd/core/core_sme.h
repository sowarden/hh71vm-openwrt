#ifndef _CORE_SME_H_
#define _CORE_SME_H_

bool qos_enhance_query_sta(struct rtl8192cd_priv *priv, unsigned char *macaddr);
void qos_enhance_add_sta(struct rtl8192cd_priv *priv, struct stat_info *pstat);
void qos_enhance_del_sta(struct rtl8192cd_priv *priv, struct stat_info *pstat);
void qos_enhance_proc(struct rtl8192cd_priv *priv);




#endif
