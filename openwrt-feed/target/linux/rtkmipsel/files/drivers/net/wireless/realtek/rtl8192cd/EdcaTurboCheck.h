

#ifndef	__EDCATURBOCHECK_H__
#define __EDCATURBOCHECK_H__

void EdcaParaInit(
	struct rtl8192cd_priv *priv
	);

#ifdef WIFI_WMM
VOID
IotEdcaSwitch(
	struct rtl8192cd_priv *priv,
	unsigned char		enable
	);
#endif
#if 0
BOOLEAN
ChooseIotMainSTA(
	struct rtl8192cd_priv *priv,
	struct sta_info*		pstat
	);
#endif
void
Choose_IOT_MainSTA(
	struct rtl8192cd_priv *priv
	);

VOID 
IotEngine(
	struct rtl8192cd_priv *priv
	);

#endif

