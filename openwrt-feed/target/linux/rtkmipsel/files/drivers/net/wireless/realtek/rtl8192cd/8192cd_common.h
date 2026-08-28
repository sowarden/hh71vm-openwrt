/*
 *  Header file defines some private structures and macro
 *
 *  Copyright (c) 2017 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef	_8192CD_COMMON_H_
#define _8192CD_COMMON_H_

/*=============================================================*/
/*-----------Driver and Users do not share the same structure ---------*/
/*=============================================================*/

#ifndef CONFIG_RTL_KERNEL_USER_SHARE_STRUCTURE
#define KERNEL_USER_NO_SHARE_STRUCTURE
#endif

#ifndef KERNEL_USER_NO_SHARE_STRUCTURE
//============================== Constant =============================
#define MESH_ID_LEN				32			//moved from ieee802_mib.h
#define	MAX_BSS_DESC			64			//new add, sync from users
#define MACADDRLEN				6
#define MAXRSNIELEN				128 		//moved from 8192cd_cfg.h
#define MAX_WSC_IE_LEN			(256+128) 	//moved from 8192cd_cfg.h
#define PROBEIELEN				260 		//moved from 8192cd_cfg.h
#define MAXDATALEN				1560		//moved from 8192cd_cfg.h

#define _WNM_BSS_TRANS_CANDIDATE_PREFRENCE_   	3	//moved from 8192cd_11v.h
#define MAX_AP_CHANNEL_NUM          8		//moved from 8192cd.h		
#define MAX_REQUEST_IE_LEN          16		//moved from 8192cd.h		
#define MAX_AP_CHANNEL_REPORT       4		//moved from 8192cd.h	
#define WLAN_SSID_MAXLEN			32		//copy from wifi.h
#define MAX_BEACON_SUBLEMENT_LEN    226		//copy from wifi.h

// moved from 8192cd_cfg.h
#if 1//defined( CONFIG_IEEE80211R) ||defined(CONFIG_IEEE80211V) 
#define MAX_R0KHID_LEN				48
#define FT_R0KH_R1KH_PULL_NONCE_LEN	16
#define FT_PMKID_LEN				16
#define FT_PMK_LEN					32
#define MAX_FTACTION_LEN			MAXDATALEN - 20
#define MAX_FTREASSOC_DEADLINE		65535
#define MAX_TRANS_LIST_NUM         	32
#endif

// moved from 8192cd_ioctl.c
#if 1//def CONFIG_IEEE80211R 
#define SIOCSIWRTLSETFTPID	0x8BE7
#define SIOCGIFTGETEVENT	0x8BE8
#define SIOCGIFTGETKEY		0x8BE9
#define SIOCSIFTSETKEY		0x8BEA
#define SIOCSIFTINFORM		0x8BEB
#define SIOCSIFTACTION		0x8BEC
#endif

/*moved from 8192cd_security.h*/
#define DOT11_AI_REQFI_CAPABILITIES      1
#define DOT11_AI_REQFI_LISTENINTERVAL    2
#define DOT11_AI_REQFI_CURRENTAPADDRESS  4

#define DOT11_AI_RESFI_CAPABILITIES      1
#define DOT11_AI_RESFI_STATUSCODE        2
#define DOT11_AI_RESFI_ASSOCIATIONID     4

/* moved from wapi_wai.h */
//Application to driver
#define	WAPI_IOCTL_TYPE_ACTIVEAUTH	0
#define	WAPI_IOCTL_TYPE_SETBK			1
#define	WAPI_IOCTL_TYPE_AUTHRSP		2

//Driver to application
#define	WAPI_IOCTL_TYPE_REQ_ACTIVE	3
#define	WAPI_IOCTL_TYPE_CA_AUTH		4

#ifndef IFNAMSIZ
#define IFNAMSIZ        16
#endif

/*moved from 8192cd_p2p.h*/
enum {
	P2P_S_IDLE = 			0,			/* between state and state */	

	P2P_S_LISTEN ,			/*1 listen state */
	
	P2P_S_SCAN ,			/*2 Scan state */
	
	P2P_S_SEARCH ,			/*3 Search state*/

	// 4~14 ; show status 4 in web page
	P2P_S_PROVI_TX_REQ ,	/*4 send provision req*/	
	P2P_S_PROVI_WAIT_RSP ,	/*5 wait provision rsp*/	
	P2P_S_PROVI_RX_RSP	,	/*6 rx provision rsp*/	

	P2P_S_PROVI_RX_REQ ,	/*7 received provision req*/	
	P2P_S_PROVI_TX_RSP ,	/*8 send provision rsp*/	

	
	P2P_S_NEGO_TX_REQ ,		/*9 send NEGO req*/	
	P2P_S_NEGO_WAIT_RSP ,	/*10 waiting for NEGO rsp*/		
	P2P_S_NEGO_TX_CONF ,	/*11 send NEGO confirm*/			


	P2P_S_NEGO_RX_REQ ,		/*12 rx NEGO req */	
	P2P_S_NEGO_TX_RSP ,		/*13 send NEGO rsp */	
	P2P_S_NEGO_WAIT_CONF ,	/*14 wait NEGO conf */	

	// 15~16 ; show status 5 in web page	
	P2P_S_CLIENT_CONNECTED_DHCPC ,		/*15 p2p client Rdy connected */		
	P2P_S_CLIENT_CONNECTED_DHCPC_done, 	/*16 p2p client Rdy connected */			

	// 17~18 ; show status 6 in web page	
	P2P_S_preGO2GO_DHCPD ,			/*17 GO not start dhcpd yet */		
	P2P_S_preGO2GO_DHCPD_done, 		/*18 GO rdy start dhcpd*/

	P2P_S_back2dev 		/*exceed 20 seconds p2p client can't connected*/
};

/*moved from ./core/core_security.h*/
/*   1. Don't use define flag
       2. any modify please also sync 
	-users/auth/include/1x_ioctl.h , 
	-users/wsc/src/wsc.h 
	-linux-DIR/driver/net/wireless/rtl8192cd/8192cd_security.h
	  (linux-DIR = linux-2.6.30 or linux-3.10)
*/
typedef enum{
    DOT11_EVENT_NO_EVENT = 1,
    DOT11_EVENT_REQUEST = 2,
    DOT11_EVENT_ASSOCIATION_IND = 3,
    DOT11_EVENT_ASSOCIATION_RSP = 4,
    DOT11_EVENT_AUTHENTICATION_IND = 5,
    DOT11_EVENT_REAUTHENTICATION_IND = 6,
    DOT11_EVENT_DEAUTHENTICATION_IND = 7,
    DOT11_EVENT_DISASSOCIATION_IND = 8,
    DOT11_EVENT_DISCONNECT_REQ = 9,
    DOT11_EVENT_SET_802DOT11 = 10,
    DOT11_EVENT_SET_KEY = 11,
    DOT11_EVENT_SET_PORT = 12,
    DOT11_EVENT_DELETE_KEY = 13,
    DOT11_EVENT_SET_RSNIE = 14,
    DOT11_EVENT_GKEY_TSC = 15,
    DOT11_EVENT_MIC_FAILURE = 16,
    DOT11_EVENT_ASSOCIATION_INFO = 17,
    DOT11_EVENT_INIT_QUEUE = 18,
    DOT11_EVENT_EAPOLSTART = 19,

	/*CONFIG_IEEE80211W*/
	DOT11_EVENT_SA_QUERY=20,
	DOT11_EVENT_SA_QUERY_RSP=21,

    DOT11_EVENT_ACC_SET_EXPIREDTIME = 31,
    DOT11_EVENT_ACC_QUERY_STATS = 32,
    DOT11_EVENT_ACC_QUERY_STATS_ALL = 33,
    DOT11_EVENT_REASSOCIATION_IND = 34,
    DOT11_EVENT_REASSOCIATION_RSP = 35,
    DOT11_EVENT_STA_QUERY_BSSID = 36,
    DOT11_EVENT_STA_QUERY_SSID = 37,
    DOT11_EVENT_EAP_PACKET = 41,

	/*RTL_WPA2_PREAUTH*/
    DOT11_EVENT_EAPOLSTART_PREAUTH = 45,
    DOT11_EVENT_EAP_PACKET_PREAUTH = 46,

    DOT11_EVENT_WPA2_MULTICAST_CIPHER = 47,
    DOT11_EVENT_WPA_MULTICAST_CIPHER = 48,

	/*AUTO_CONFIG*/
	DOT11_EVENT_AUTOCONF_ASSOCIATION_IND = 50,
	DOT11_EVENT_AUTOCONF_ASSOCIATION_CONFIRM = 51,
	DOT11_EVENT_AUTOCONF_PACKET = 52,
	DOT11_EVENT_AUTOCONF_LINK_IND = 53,

	/*WIFI_SIMPLE_CONFIG*/ 
	DOT11_EVENT_WSC_SET_IE = 55,
	DOT11_EVENT_WSC_PROBE_REQ_IND = 56,
	DOT11_EVENT_WSC_PIN_IND = 57,
	DOT11_EVENT_WSC_ASSOC_REQ_IE_IND = 58,

#ifdef INCLUDE_WPS 
	DOT11_EVENT_WSC_SET_MIB=42,
	DOT11_EVENT_WSC_GET_MIB=43,
	DOT11_EVENT_REQUEST_F_INCLUDE_WPS=44,		
	DOT11_EVENT_WSC_INIT_IND = 70,
	DOT11_EVENT_WSC_EXIT_IND = 71,
	DOT11_EVENT_WSC_TERM_IND = 72,
	DOT11_EVENT_WSC_GETCONF_IND = 73,
	DOT11_EVENT_WSC_PUTCONF_IND = 74,
	DOT11_EVENT_WSC_LEDCONTROL_IND = 75,
	DOT11_EVENT_WSC_SENDMSG_IND = 76,
	DOT11_EVENT_WSC_PUTCONF = 77,
	DOT11_EVENT_WSC_SOAP = 78,
	DOT11_EVENT_WSC_PIN = 79,
	DOT11_EVENT_WSC_PBC = 80,
	DOT11_EVENT_WSC_SYS = 81,
	DOT11_EVENT_WSC_PUTWLANREQUEST_IND = 82,
	DOT11_EVENT_WSC_PUTPKT = 83,
	DOT11_EVENT_WSC_GETDEVINFO = 84,
	DOT11_EVENT_WSC_M2M4M6M8 = 85,
	DOT11_EVENT_WSC_PUTWLANRESPONSE = 86,
	DOT11_EVENT_WSC_PUTMESSAGE = 87,
	DOT11_EVENT_WSC_PUTWLREQ_PROBEIND = 88,
	DOT11_EVENT_WSC_PUTWLREQ_STATUSIND = 89,
#endif				
	/*end of WIFI_SIMPLE_CONFIG*/ 

	/*CONFIG_RTK_MESH*/
	DOT11_EVENT_PATHSEL_GEN_RREQ = 59,
	DOT11_EVENT_PATHSEL_GEN_RERR = 60,
	DOT11_EVENT_PATHSEL_RECV_RREQ = 61,
	DOT11_EVENT_PATHSEL_RECV_RREP = 62,
	DOT11_EVENT_PATHSEL_RECV_RERR = 63,
	DOT11_EVENT_PATHSEL_RECV_PANN = 65,
	DOT11_EVENT_PATHSEL_RECV_RANN = 66,
	/*CONFIG_RTK_MESH*/

	/*CONFIG_RTL_WAPI_SUPPORT*/
	DOT11_EVENT_WAPI_INIT_QUEUE =67,
	DOT11_EVENT_WAPI_READ_QUEUE = 68,
	DOT11_EVENT_WAPI_WRITE_QUEUE  =69,
	/*CONFIG_RTL_WAPI_SUPPORT*/

#if 1//defined(CONFIG_RTL_COMAPI_CFGFILE) && defined(WIFI_SIMPLE_CONFIG) 
#ifdef INCLUDE_WPS 
	DOT11_EVENT_WSC_START_IND = 90,
	//EV_MODE, EV_STATUS, EV_MEHOD, EV_STEP, EV_OOB
	DOT11_EVENT_WSC_MODE_IND = 91,
	DOT11_EVENT_WSC_STATUS_IND = 92,
	DOT11_EVENT_WSC_METHOD_IND = 93,
	DOT11_EVENT_WSC_STEP_IND = 94,
	DOT11_EVENT_WSC_OOB_IND = 95,
#else
	DOT11_EVENT_WSC_START_IND = 70,
	//EV_MODE, EV_STATUS, EV_MEHOD, EV_STEP, EV_OOB
	DOT11_EVENT_WSC_MODE_IND = 71,
	DOT11_EVENT_WSC_STATUS_IND = 72,
	DOT11_EVENT_WSC_METHOD_IND = 73,
	DOT11_EVENT_WSC_STEP_IND = 74,
	DOT11_EVENT_WSC_OOB_IND = 75,
#endif
#endif

	DOT11_EVENT_WSC_SWITCH_MODE = 		100,
	DOT11_EVENT_WSC_STOP = 				101,				
	DOT11_EVENT_WSC_SWITCH_WLAN_MODE =	108,						
	DOT11_EVENT_WSC_SET_MY_PIN =	 	102,
	DOT11_EVENT_WSC_SPEC_SSID = 	103,
	DOT11_EVENT_WSC_SPEC_MAC_IND = 	104,
	DOT11_EVENT_WSC_CHANGE_MODE = 	105,	
	
	DOT11_EVENT_WSC_RM_PBC_STA=		106,
	DOT11_EVENT_WSC_CHANGE_MAC_IND = 107,		

	/*HS2_SUPPORT*/
	DOT11_EVENT_WNM_NOTIFY = 		109,
	DOT11_EVENT_GAS_INIT_REQ = 		110,
	DOT11_EVENT_GAS_COMEBACK_REQ = 	111,
	DOT11_EVENT_HS2_SET_IE = 		112,
	DOT11_EVENT_HS2_GAS_RSP = 		113,
	DOT11_EVENT_HS2_GET_TSF = 		114,
	DOT11_EVENT_HS2_TSM_REQ = 		115,
	DOT11_EVENT_HS2_GET_RSN = 		116,
	DOT11_EVENT_HS2_GET_MMPDULIMIT=117,
	DOT11_EVENT_WNM_DEAUTH_REQ = 	118,
	DOT11_EVENT_QOS_MAP_CONF = 		119,

	/*CONFIG_IEEE80211W*/ 
	DOT11_EVENT_SET_PMF = 			120,
	DOT11_EVENT_GET_IGTK_PN = 		121,
	DOT11_EVENT_INIT_PMF = 			122,	// HS2 R2 logo test

	/*RSSI_MONITOR_NCR*/
	DOT11_EVENT_RSSI_MONITOR_REPORT = 122,
	DOT11_EVENT_RSSI_MONITOR_SETTYPE = 123,	

	/*CONFIG_IEEE80211R*/
	DOT11_EVENT_FT_GET_EVENT		= 125,
	DOT11_EVENT_FT_IMD_ASSOC_IND	= 126,
	DOT11_EVENT_FT_GET_KEY			= 127,
	DOT11_EVENT_FT_SET_KEY			= 128,
	DOT11_EVENT_FT_PULL_KEY_IND		= 129,
	DOT11_EVENT_FT_ASSOC_IND		= 130,
	DOT11_EVENT_FT_KEY_EXPIRE_IND	= 131,
	DOT11_EVENT_FT_ACTION_IND		= 132,
	DOT11_EVENT_FT_QUERY_INFO		= 133,
	DOT11_EVENT_FT_SET_INFO			= 134,
	DOT11_EVENT_FT_AUTH_INSERT_R0	= 135,
	DOT11_EVENT_FT_AUTH_INSERT_R1	= 136,
	DOT11_EVENT_FT_TRIGGER_EVENT	= 137,
	
	/*USER_ADDIE*/
	DOT11_EVENT_USER_SETIE	= 138,
	
	/*SUPPORT_UCFGING_LED*/
	DOT11_EVENT_UCFGING_LED	= 139,
	
	/*INDICATE_LINK_CHANGE*/
	DOT11_EVENT_LINK_CHANGE_IND = 140,
	
	/*RTK_PEER AP and STA fast connection*/
	DOT11_EVENT_FC_DISASSOC_ALL_STA = 141,
	DOT11_EVENT_STA_ASSOCIATION_IND = 142,	
	
#if 1//def CONFIG_IEEE80211R_CLI 
	DOT11_EVENT_FT_REASSOC_RSP		= 148, 
#endif

#if 1//def ROAMING_SUPPORT 
	DOT11_EVENT_ROAMING_STA_RSSI_ALARM = 143,
	DOT11_EVENT_ROAMING_STA_INFO_REPORT = 144,
	DOT11_EVENT_ROAMING_BSS_TRANSMIT_RESP = 145,
#endif

#if 1//def BEACON_VS_IE 
	EVENT_PROBE_RX_VSIE = 146,
#endif

#if 1//defined(GENERAL_EVENT) 
	DOT11_EVENT_GENERAL_GET_EVENT = 147, 
#endif

	DOT11_EVENT_UNKNOWN = 149	
} DOT11_EVENT;

enum _FT_ACTION_CODE{
	ACTION_CODE_REQUEST		= 0,
	ACTION_CODE_RESPONSE	= 1
};

enum _FTKEY_TYPE{
	FTKEY_TYPE_PUSH		= 1,
	FTKEY_TYPE_PULL		= 2,
};

/*moved from 8192cd_psk.h*/
/*
 * Reason code for Disconnect
 */
typedef enum _ReasonCode{
	unspec_reason					= 0x01,
	auth_not_valid  				= 0x02,
	deauth_lv_ss    				= 0x03,
	inactivity						= 0x04,
	ap_overload						= 0x05,
	class2_err						= 0x06,
	class3_err						= 0x07,
	disas_lv_ss						= 0x08,
	asoc_not_auth					= 0x09,
	RSN_invalid_info_element		= 13,
	RSN_MIC_failure					= 14,
	RSN_4_way_handshake_timeout	= 15,
	RSN_diff_info_element			= 17,
	RSN_multicast_cipher_not_valid	= 18,
	RSN_unicast_cipher_not_valid	= 19,
	RSN_AKMP_not_valid				= 20,
	RSN_unsupported_RSNE_version	= 21,
	RSN_invalid_RSNE_capabilities	= 22,
	RSN_ieee_802dot1x_failed		= 23,
	//belowing are Realtek definition
	RSN_PMK_not_avaliable			= 24,
	expire							= 30,
	session_timeout					= 31,
	acct_idle_timeout				= 32,
	acct_user_request				= 33
}ReasonCode;

/*moved from 8192cd_security.h*/
typedef enum{
    DOT11_KeyType_Group = 0,
    DOT11_KeyType_Pairwise = 1,
    DOT11_KeyType_IGTK = 2
} DOT11_KEY_TYPE;

/*moved from 8192cd_security.h*/
typedef enum{
    DOT11_KeyUsage_ENC,
    DOT11_KeyUsage_MIC
} DOT11_KEY_USAGE;

/*moved from 8192cd_security.h*/
typedef enum{
    DOT11_Role_Auth,
    DOT11_Role_Supp
} DOT11_ROLE;

/*moved from 8192cd_security.h*/
typedef enum{
    DOT11_VARIABLE_MACEnable,
    DOT11_VARIABLE_SystemAuthControl,
    DOT11_VARIABLE_AuthControlledPortStatus,
    DOT11_VARIABLE_AuthControlledPortControl,
    DOT11_VARIABLE_AuthenticationType,
    DOT11_VARIABLE_KeyManagement,
    DOT11_VARIABLE_MulticastCipher,
    DOT11_VARIABLE_UnicastCipher
} DOT11_VARIABLE_TYPE;

/*moved from 8192cd_security.h*/
typedef enum{
    DOT11_SysAuthControl_Disabled,
    DOT11_SysAuthControl_Enabled
} DOT11_SYSTEM_AUTHENTICATION_CONTROL;

/*moved from 8192cd_security.h*/
typedef enum{
    DOT11_PortControl_ForceUnauthorized,
    DOT11_PortControl_ForceAuthorized,
    DOT11_PortControl_Auto
} DOT11_PORT_CONTROL;

/*moved from 8192cd_security.h*/
typedef enum{
    DOT11_Association_Fail,
    DOT11_Association_Success
}DOT11_ASSOCIATION_RESULT;

/*moved from core_security.h*/
typedef enum{
    DOT11_PortStatus_Unauthorized,
    DOT11_PortStatus_Authorized,
    DOT11_PortStatus_Guest
}DOT11_PORT_STATUS;

/*moved from 8192cd_security.h*/
typedef enum{
	DOT11_AuthKeyType_RSNReserved = 0,			//copy from auth daemon
	DOT11_AuthKeyType_RSN = 1,
	DOT11_AuthKeyType_PSK = 2,
	DOT11_AuthKeyType_RSNPSK = 2,				//copy from auth daemon
	DOT11_AuthKeyType_FT8021x = 3,
	DOT11_AuthKeyType_NonRSN802dot1x = 3,		//copy from auth daemon
	DOT11_AuthKeyType_FTPSK = 4,
	DOT11_AuthKeyType_FT = 4,					//copy from auth daemon(value in auth is 3, but driver value is 4)
	DOT11_AuthKeyType_802_1X_SHA256 = 5,		//CONFIG_IEEE80211W_CLI
	DOT11_AuthKeyType_PSK_SHA256 = 6,
	/* rtk_sae */
	DOT11_AuthKeyType_SAE = 8,
	DOT11_AuthKeyType_FT_SAE = 9,
	DOT11_AuthKeyType_PRERSN = 255,				//copy from auth daemon
} DOT11_AUTHKEY_TYPE;

/*moved from 8192cd_security.h*/
typedef enum{
	DOT11_Ioctl_Query = 0,
	DOT11_Ioctl_Set = 1
} DOT11_Ioctl_Flag;

/*moved from 8192cd_security.h*/
typedef enum{
	DOT11_ENC_NONE  = 0,
	DOT11_ENC_WEP40 = 1,
	DOT11_ENC_TKIP  = 2,
	DOT11_ENC_WRAP  = 3,
	DOT11_ENC_CCMP  = 4,
	DOT11_ENC_WEP104= 5,
	DOT11_ENC_BIP	= 6,
	DOT11_ENC_WAPI	= 6,
	DOT11_ENC_NOGA  = 7, //Group addressed traffic not allowed (copy from 1x_ioctl.h)
	/*WPA3_ENT*/
	DOT11_ENC_GCMP	= 8,
	DOT11_ENC_GCMP256	= 9,
	DOT11_ENC_BIP_GMAC256	= 12,
} DOT11_ENC_ALGO;

/*moved from 8192cd.h*/
enum MEASUREMENT_RESULT{
    MEASUREMENT_UNKNOWN = 0,
    MEASUREMENT_PROCESSING = 1,
    MEASUREMENT_SUCCEED = 2,
    MEASUREMENT_INCAPABLE = 3,
    MEASUREMENT_REFUSED = 4,    
    MEASUREMENT_RECEIVED = 5,    
};

/*moved from 8192cd.h*/
enum BEACON_MODE{
    BEACON_MODE_PASSIVE = 0,
    BEACON_MODE_ACTIVE = 1,
    BEACON_MODE_TABLE = 2,
};

//============================== Structure =============================
/*moved from 8192cd_cfg.h*/
#ifndef __ECOS
#ifdef __GNUC__
#define __WLAN_ATTRIB_PACK__		__attribute__ ((packed))
#define __PACK
#endif
#endif

#ifdef __arm
#define __WLAN_ATTRIB_PACK__
#define __PACK	__packed
#endif

#if defined(GREEN_HILL) || defined(__ECOS)
#define __WLAN_ATTRIB_PACK__
#define __PACK
#endif

/*moved from 8192cd.h*/
/* BSS info, reported to web server */
typedef struct _bss_info_2_web {
    unsigned char state;
    unsigned char channel;
    unsigned char txRate;
    unsigned char bssid[6];
    unsigned char rssi, sq;
    unsigned char ssid[33];
} bss_info_2_web;

//for user application use
typedef struct _bss_info_2_web bss_info;	

/*moved from 8192cd.h*/
typedef struct _wds_info {
	unsigned char	state;
	unsigned char	addr[6];
	unsigned long	tx_packets;
	unsigned long	rx_packets;
	unsigned long	tx_errors;
	unsigned char	TxOperaRate;
} web_wds_info;

//for user application use
typedef struct _wds_info WDS_INFO_T, *WDS_INFO_Tp;

/*moved from 8192cd_ioctl.h*/
struct _misc_data_ {
	unsigned char	mimo_tr_hw_support;
	unsigned char	mimo_tr_used;
	unsigned char	resv[30];
};

/*moved from ieee802_mib.h*/
struct ibss_priv {
	unsigned short	atim_win;
};

/*moved from ieee802_mib.h*/
struct bss_desc {
	unsigned char	bssid[MACADDRLEN];
	char			ssid[32];
	unsigned char	*ssidptr;			// unused, for backward compatible
	unsigned short	ssidlen;
	unsigned char	meshid[MESH_ID_LEN];
	unsigned char	*meshidptr;			// unused, for backward compatible
	unsigned short	meshidlen;
	unsigned int	bsstype;
	unsigned short	beacon_prd;
	unsigned char	dtim_prd;
	unsigned int	t_stamp[2];
	struct ibss_priv	ibss_par;
	unsigned short	capability;
	unsigned char	channel;
	unsigned char	channel2;
	unsigned int	basicrate;
	unsigned int	supportrate;
	unsigned char	bdsa[MACADDRLEN];
	unsigned char	rssi;
	unsigned char	sq;
	unsigned char	network;
	/*add for P2P_SUPPORT ; for sync; it exist no matter p2p enabled or not*/
	unsigned char	p2pdevname[33];
	unsigned char	p2prole;
	unsigned short	p2pwscconfig;
	unsigned char	p2paddress[MACADDRLEN];
	/* multi-stage 2.3.0 */
	unsigned char	stage;	
    /*cfg p2p cfg p2p*/	
    #if	defined(WIFI_WPAS) || defined(RTK_NL80211)
	#if 0
    unsigned char	    p2p_ie_len;
    unsigned char	    p2p_ie[256];    
	unsigned char	    wscie_len;
	unsigned char	    wscie[256];
	#endif
	struct ht_info_elmt ht_info;
	struct ht_cap_elmt ht_cap;
	#endif
	#if	defined(WIFI_WPAS) || defined(RTK_NL80211) || defined(CONFIG_RTL_COMAPI_WLTOOLS)
    unsigned char	    wpa_ie_len;
    unsigned char	    wpa_ie[256];
    unsigned char	    rsn_ie_len;    
    unsigned char	    rsn_ie[256];
	unsigned long long	timestamp;
    #endif    
	//CONFIG_IEEE80211R_CLI
    unsigned char mdid[2];
    unsigned char ft_cap; 
    /*cfg p2p cfg p2p*/
};

//for user application use
typedef struct bss_desc BssDscr, *pBssDscr;	

typedef struct _sitesurvey_status {
    unsigned char number;
    unsigned char pad[3];
    struct bss_desc bssdb[MAX_BSS_DESC];
} SS_STATUS_T, *SS_STATUS_Tp;

#if 1//def CONFIG_RTL_P2P_SUPPORT 
/*moved from 8192cd_p2p.h*/
struct p2p_state_event{
	unsigned char  p2p_status;
	unsigned char  p2p_event;	
	unsigned short p2p_wsc_method;		
	unsigned char  p2p_role;	
};
//for user application use
typedef struct p2p_state_event P2P_SS_STATUS_T, *P2P_SS_STATUS_Tp;

/*moved from 8192cd_p2p.h*/
struct __p2p_wsc_confirm
{
	unsigned char dev_address[MACADDRLEN];	
	unsigned short wsc_config_method;	
	unsigned char pincode[9];
};
//for user application use
typedef struct __p2p_wsc_confirm P2P_WSC_CONFIRM_T, *P2P_WSC_CONFIRM_Tp;

/*moved from 8192cd_p2p.h*/
typedef struct provision_comm
{
	unsigned char dev_address[MACADDRLEN];
	unsigned short wsc_config_method;	
	unsigned char channel;  
	
}P2P_PROVISION_COMM_T, *P2P_PROVISION_COMM_Tp;
#endif

/*moved from 8192cd.h*/
typedef struct _sta_info_2_web {
	unsigned short	aid;
	unsigned char	addr[6];
	unsigned long	tx_packets;
	unsigned long	rx_packets;
	unsigned long	expired_time;	// 10 msec unit
	unsigned short	flags;
	unsigned char	TxOperaRate;
	unsigned char	rssi;
	unsigned long	link_time;		// 1 sec unit
	//jim 20160617 must keep same with the one defined in 1x_common.h
#if defined(__OSK__)
	unsigned long idle_time;
#endif
	unsigned long	tx_fail;
	unsigned long	tx_bytes;
	unsigned long	rx_bytes;
	unsigned char	network;
	unsigned char	ht_info;		// bit0: 0=20M mode, 1=40M mode; bit1: 0=longGI, 1=shortGI
	unsigned char	RxOperaRate;
#ifdef TLN_STATS 
	unsigned char	auth_type;
	unsigned char	enc_type;
	unsigned char 	resv[1];
#else
	unsigned char 	resv[3];
#endif
	unsigned short	acTxOperaRate;
#if 1 //def RTK_MULTI_AP
	unsigned char   multiap_profile;
#endif
#if 1 //def CONFIG_DLINK_STA_RATE
	unsigned short  CurrentRxRate;
#endif
} sta_info_2_web; 

//for user application use
typedef struct _sta_info_2_web WLAN_STA_INFO_T, *WLAN_STA_INFO_Tp; 

/*moved from 8192cd.h*/
#if 1//defined(CONFIG_RTL_CROSSBAND_REPEATER_SUPPORT)
struct crossband_metric{
	unsigned int rssi_metric;
	unsigned int cu_metric;
	unsigned int noise_metric;
};
#endif
 
#if 1//def WIFI_SIMPLE_CONFIG
/*moved from 8192cd_security.h*/
typedef struct _DOT11_PROBE_REQUEST_IND{
	unsigned char   EventId;
	unsigned char   IsMoreEvent;
	unsigned char   MACAddr[MACADDRLEN];
	unsigned short  ProbeIELen;
	unsigned char   ProbeIE[PROBEIELEN];
}DOT11_PROBE_REQUEST_IND;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_WSC_ASSOC_IND{
	unsigned char   EventId;
	unsigned char   IsMoreEvent;
	unsigned char   MACAddr[MACADDRLEN];
	unsigned short  AssocIELen;
	unsigned char   AssocIE[PROBEIELEN];
	unsigned char	wscIE_included;
}DOT11_WSC_ASSOC_IND;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_GETSET_MIB {
	unsigned char EventId;
	unsigned char IsMoreEvent;
	unsigned char cmd[50];
}DOT11_GETSET_MIB;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_WSC_PIN_IND{
	unsigned char EventId;
	unsigned char IsMoreEvent;
	char code[256];
} DOT11_WSC_PIN_IND;

/*moved from 8192cd_security.h*/
#if 1//def CONFIG_RTL_COMAPI_CFGFILE 
typedef struct _DOT11_WSC_IND{
	unsigned char EventId;
	unsigned char IsMoreEvent;
	unsigned int value;
} DOT11_WSC_IND;
#endif  //CONFIG_RTL_COMAPI_CFGFILE

/*moved from core_security.h*/
typedef struct _DOT11_EAP_PACKET{
	unsigned char	EventId;
	unsigned char	IsMoreEvent;
	unsigned short  packet_len;
	unsigned char	packet[1550];
}DOT11_EAP_PACKET;

/*moved from 8192cd_p2p.h*/
/*	need sync with wsc.h 	*/
typedef struct _DOT11_P2P_INDICATE_WSC{
    unsigned char   EventId;
    unsigned char   IsMoreEvent;

	unsigned char 	modeSwitch ;
	char 	network_key[65] ;	
	char 	gossid[33] ;		
	unsigned char 	trigger_method ;		
	
	unsigned char 	whosPINuse ;		
	char 	PINCode[9] ;		
	unsigned char 	requestor;
	char 	interfacename[16];		                
}DOT11_P2P_INDICATE_WSC;

/*moved from 8192cd.h*/
#if 1//ndef INCLUDE_WPS 
/* this struct no sync with wsc daemon ; 
   add ssid for sorting by ssid intent ; wps_ie_info2 is sync with wsc daemon;take care*/
struct wps_ie_info {
	unsigned char rssi;
	unsigned int wps_ie_len;
	unsigned char data[MAX_WSC_IE_LEN];
	char ssid[33];
	unsigned char chan;
#if 1//def CONFIG_RTK_WPS_MESH 
	unsigned char mesh_IR;
#endif
};
#endif

/*moved from wsc.h*/
typedef struct _sitesurvey_ie {
	unsigned char number;
	unsigned char pad[3];
	struct wps_ie_info ie[MAX_BSS_DESC];
} SS_IE_T, *SS_IE_Tp;
#endif

/*moved from 8192cd_security.h*/
typedef struct _DOT11_SET_WSCIE {
	unsigned char EventId;
	unsigned char IsMoreEvent;
	unsigned short Flag;
	unsigned short RSNIELen;
	unsigned char  RSNIE[MAX_WSC_IE_LEN];
	unsigned char  MACAddr[MACADDRLEN];
} DOT11_SET_WSCIE;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_SET_PORT{
    unsigned char EventId;
    unsigned char PortStatus;
    unsigned char PortType;
    unsigned char MACAddr[MACADDRLEN];
}DOT11_SET_PORT;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_DISCONNECT_REQ{
    unsigned char   EventId;
    unsigned char   IsMoreEvent;
    unsigned short  Reason;
    char            MACAddr[MACADDRLEN];
}DOT11_DISCONNECT_REQ;

/*moved from Mesh_route.h*/
// _DOT11s_XXX serires struct size need less than MAXDATALEN(1560)	, which defined in iwreq.c
typedef __PACK struct _DOT11s_GEN_RREQ_PACKET{
    unsigned char	EventId;
    unsigned char   IsMoreEvent;
    unsigned char	MyMACAddr[MACADDRLEN];
    unsigned char	destMACAddr[MACADDRLEN];
    unsigned char   TTL;
    unsigned short  Seq_num;
}__WLAN_ATTRIB_PACK__ DOT11s_GEN_RREQ_PACKET;

/*moved from Mesh_route.h*/
typedef __PACK struct _DOT11s_GEN_RERR_PACKET{
    unsigned char	EventId;
    unsigned char   IsMoreEvent;
    unsigned char	MyMACAddr[MACADDRLEN];
    unsigned char	SorNMACAddr[MACADDRLEN];	//this address is the neighbor that hello message finds it is unreachabel.
    											// if flag == 1, this field means disconnected neighbor MAC addr
    											// if flag == 2, this field means source MAC addr of the data frame
    unsigned char	DataDestMAC[MACADDRLEN];	// this field is valid if and only if flag == 2
    unsigned char	PrehopMAC[MACADDRLEN];		// this field is valid if and only if flag == 2
    unsigned char   TTL;
    unsigned short  Seq_num;
    unsigned char   Flag;						// flag = 1 means the link to neighbor has broken
												// flag = 2 means this MP doesn't have the nexthop information for the destination in pathseleciton table
}__WLAN_ATTRIB_PACK__ DOT11s_GEN_RERR_PACKET;

/*moved from Mesh_route.h*/
typedef __PACK struct _DOT11s_RECV_PACKET{
    unsigned char	EventId;
    unsigned char   IsMoreEvent;
    unsigned char	MyMACAddr[MACADDRLEN];
    unsigned char	PreHopMACAddr[MACADDRLEN];
    unsigned char	DesMACAddr[MACADDRLEN];
    unsigned int  	Pre_Hop_Metric;
    unsigned char 	ReceiveData[128];
    unsigned char   TTL;
    unsigned short  Seq_num;
    unsigned char 	Is6AddrFormat;
    unsigned char	MACAddr5[MACADDRLEN];
    unsigned char	MACAddr6[MACADDRLEN];
#if 1//def RTK_MESH_METRIC_REFINE 
    unsigned char	InterfaceMACAddr[MACADDRLEN];
    unsigned int	RSSI_Metric;
    unsigned int	Cu_Metric;
    unsigned int	Noise_Metric;
#endif
}__WLAN_ATTRIB_PACK__ DOT11s_RECV_PACKET;

/*moved from Mesh_route.h*/
struct mesh_mibValues{
	unsigned char rssiThreshold;
	unsigned char cuThreshold;
	unsigned char noiseThreshold;
	unsigned char rssiWeight;
	unsigned char cuWeight;
	unsigned char noiseWeight;
};

/*moved from 8192cd.h*/
#if 1//def CONFIG_IEEE80211V
struct dot11v_bss_candidate_subelement {
    unsigned char subelement_id;
    unsigned char len;    
    unsigned char preference;
};

// set neighbor report parameter
struct bss_transition_para
{
    unsigned char addr[MACADDRLEN];
    unsigned char chan_until;
    unsigned char FromUser;  /* Transition from ioctl */
	unsigned char bssid_mac[MACADDRLEN];
	unsigned char channel;
};
#endif

#if 1// defined(CONFIG_IEEE80211V) || defined(DOT11K) 
#pragma pack(1)

union dot11k_neighbor_report_bssinfo {
    unsigned int value;
    struct {        
#ifdef _BIG_ENDIAN_
        unsigned int reserved:20;    
        unsigned int high_tp:1;        
        unsigned int mde:1;
        unsigned int cap_im_ba:1;  
        unsigned int cap_delay_ba:1;
        unsigned int cap_rm:1;
        unsigned int cap_apsd:1;
        unsigned int cap_qos:1;
        unsigned int cap_spectrum:1;
        unsigned int key_scope:1;
        unsigned int security:1;        
        unsigned int ap_reachability:2;
#else
        unsigned int ap_reachability:2;
        unsigned int security:1;
        unsigned int key_scope:1;
        unsigned int cap_spectrum:1;
        unsigned int cap_qos:1;
        unsigned int cap_apsd:1;
        unsigned int cap_rm:1;
        unsigned int cap_delay_ba:1;
        unsigned int cap_im_ba:1;    
        unsigned int mde:1;
        unsigned int high_tp:1;
        unsigned int reserved:20;
#endif    
    } field;

};

/*this is 802.11k neighbor report structure, please do not modify*/
struct dot11k_neighbor_report
{
    unsigned char bssid[MACADDRLEN];					//6
    union dot11k_neighbor_report_bssinfo bssinfo;		//4 //4
    unsigned char op_class;								// 1
    unsigned char channel;								// 1
    unsigned char phytype;								// 1
#if 1//def CONFIG_IEEE80211V	
    struct dot11v_bss_candidate_subelement subelemnt;	// 3
#endif
};

#pragma pack()
#endif

/*moved from 8192cd.h*/
#if 1//def DOT11K 
#pragma pack(1)
struct dot11k_link_measurement {
    unsigned char tpc_tx_power;
    unsigned char tpc_link_margin;    
    unsigned char recv_antenna_id;
    unsigned char xmit_antenna_id;    
    unsigned char RCPI;
    unsigned char RSNI;    
};

struct dot11k_ap_channel_report
{
    unsigned char len;
    unsigned char op_class;
    unsigned char channel[MAX_AP_CHANNEL_NUM];
};

struct dot11k_beacon_measurement_req
{
    unsigned char op_class;
    unsigned char channel;
    unsigned short random_interval;    
    unsigned short measure_duration;    
    unsigned char mode;     
    unsigned char bssid[MACADDRLEN];
    char ssid[WLAN_SSID_MAXLEN + 1];
    unsigned char report_detail; /* 0: no-fixed len field and element, 
					                1: all fixed len field and elements in Request ie,
					                2: all fixed len field and elements (default)*/
    unsigned char request_ie_len;
    unsigned char request_ie[MAX_REQUEST_IE_LEN];    
    struct dot11k_ap_channel_report ap_channel_report[MAX_AP_CHANNEL_REPORT];    
};

struct dot11k_beacon_measurement_report_info
{
    unsigned char op_class;
    unsigned char channel;
    unsigned int  measure_time_hi;
    unsigned int  measure_time_lo;    
    unsigned short measure_duration;
    unsigned char frame_info;
    unsigned char RCPI;
    unsigned char RSNI;
    unsigned char bssid[MACADDRLEN];    
    unsigned char antenna_id;
    unsigned int  parent_tsf;     
};

struct dot11k_beacon_measurement_report
{
    struct dot11k_beacon_measurement_report_info info;
    unsigned char subelements_len;    
    unsigned char subelements[MAX_BEACON_SUBLEMENT_LEN];
};

struct dot11k_neighbor_bss_load
{
    unsigned short sta_count; /* from BSS Load ie*/
    unsigned char channel_utilization; /* from BSS Load ie*/    
};

#pragma pack(0)
#endif

#if 1//def CONFIG_IEEE80211R 
typedef struct _DOT11_FT_IMD_ASSOC_IND{
	unsigned char EventId;
	unsigned char IsMoreEvent;
	unsigned char MACAddr[MACADDRLEN];
} __WLAN_ATTRIB_PACK__ DOT11_FT_IMD_ASSOC_IND;

typedef struct _DOT11_FT_PULL_KEY_IND{
	unsigned char EventId;
	unsigned char IsMoreEvent;
	unsigned char Type;
	unsigned char r0kh_id[MAX_R0KHID_LEN];
	unsigned int Length;
	unsigned char nonce[FT_R0KH_R1KH_PULL_NONCE_LEN];
	unsigned char pmk_r0_name[FT_PMKID_LEN];
	unsigned char r1kh_id[MACADDRLEN];
	unsigned char s1kh_id[MACADDRLEN];
} __WLAN_ATTRIB_PACK__ DOT11_FT_PULL_KEY_IND;

typedef struct _DOT11_FT_GET_KEY_PROTO{
	unsigned char EventId;
	unsigned char IsMoreEvent;
	unsigned char Type;
} __WLAN_ATTRIB_PACK__ DOT11_FT_GET_KEY_PROTO;

typedef struct _DOT11_FT_GET_KEY{
	unsigned char EventId;
	unsigned char IsMoreEvent;
	unsigned char Type;
	unsigned int Length;
	unsigned char r1kh_id[MACADDRLEN];
	unsigned char s1kh_id[MACADDRLEN];
} __WLAN_ATTRIB_PACK__ DOT11_FT_GET_KEY;

typedef struct _DOT11_FT_GET_KEY_PUSH{
	unsigned char EventId;
	unsigned char IsMoreEvent;
	unsigned char Type;
	unsigned int Length;
	unsigned int timestamp;
	unsigned char r1kh_id[MACADDRLEN];
	unsigned char s1kh_id[MACADDRLEN];
	unsigned char pmk_r0_name[FT_PMKID_LEN];
	unsigned char pmk_r1[FT_PMK_LEN];
	unsigned char pmk_r1_name[FT_PMKID_LEN];
	unsigned short pairwise;
} __WLAN_ATTRIB_PACK__ DOT11_FT_GET_KEY_PUSH;

typedef struct _DOT11_FT_GET_KEY_PULL{
	unsigned char EventId;
	unsigned char IsMoreEvent;
	unsigned char Type;
	unsigned int Length;
	unsigned char nonce[FT_R0KH_R1KH_PULL_NONCE_LEN];
	unsigned char r1kh_id[MACADDRLEN];
	unsigned char s1kh_id[MACADDRLEN];
	unsigned char pmk_r1[FT_PMK_LEN];
	unsigned char pmk_r1_name[FT_PMKID_LEN];
	unsigned short pairwise;
} __WLAN_ATTRIB_PACK__ DOT11_FT_GET_KEY_PULL;

typedef struct _DOT11_FT_SET_KEY_PROTO{
	unsigned char EventId;
	unsigned char IsMoreEvent;
	unsigned char Type;
} __WLAN_ATTRIB_PACK__ DOT11_FT_SET_KEY_PROTO;

typedef struct _DOT11_FT_SET_KEY_PUSH{
	unsigned char EventId;
	unsigned char IsMoreEvent;
	unsigned char Type;
	unsigned int Length;
	unsigned int timestamp;
	unsigned char r1kh_id[MACADDRLEN];
	unsigned char s1kh_id[MACADDRLEN];
	unsigned char pmk_r0_name[FT_PMKID_LEN];
	unsigned char pmk_r1[FT_PMK_LEN];
	unsigned char pmk_r1_name[FT_PMKID_LEN];
	unsigned short pairwise;
} __WLAN_ATTRIB_PACK__ DOT11_FT_SET_KEY_PUSH;

typedef struct _DOT11_FT_SET_KEY_PULL{
	unsigned char EventId;
	unsigned char IsMoreEvent;
	unsigned char Type;
	unsigned int Length;
	unsigned char nonce[FT_R0KH_R1KH_PULL_NONCE_LEN];
	unsigned char r1kh_id[MACADDRLEN];
	unsigned char s1kh_id[MACADDRLEN];
	unsigned char pmk_r1[FT_PMK_LEN];
	unsigned char pmk_r1_name[FT_PMKID_LEN];
	unsigned short pairwise;
} __WLAN_ATTRIB_PACK__ DOT11_FT_SET_KEY_PULL;

typedef struct _DOT11_FT_ASSOC_IND{
	unsigned char EventId;
	unsigned char IsMoreEvent;
	unsigned char MACAddr[MACADDRLEN];
} __WLAN_ATTRIB_PACK__ DOT11_FT_ASSOC_IND;

typedef struct _DOT11_FT_KEY_EXPIRE_IND{
	unsigned char EventId;
	unsigned char IsMoreEvent;
	unsigned char MACAddr[MACADDRLEN];
} __WLAN_ATTRIB_PACK__ DOT11_FT_KEY_EXPIRE_IND;

typedef struct _DOT11_FT_ACTION{
	unsigned char EventId;
	unsigned char IsMoreEvent;
	unsigned char MACAddr[MACADDRLEN];
	unsigned char ActionCode;
	unsigned int packet_len;
	unsigned char packet[MAX_FTACTION_LEN];
} __WLAN_ATTRIB_PACK__ DOT11_FT_ACTION;

// Following are for Auth daemon
typedef struct _DOT11_QUERY_FT_INFORMATION
{
    unsigned char EventId;
    unsigned char IsMoreEvent;
	unsigned char sta_addr[MACADDRLEN];
	unsigned char ssid[32];
	unsigned int ssid_len;
	unsigned char mdid[2];
	unsigned char r0kh_id[MAX_R0KHID_LEN];
	unsigned int r0kh_id_len;
	unsigned char bssid[MACADDRLEN];
	unsigned char over_ds;
	unsigned char res_request;
} DOT11_QUERY_FT_INFORMATION, *PDOT11_QUERY_FT_INFORMATION;

typedef struct _DOT11_SET_FT_INFORMATION
{
    unsigned char EventId;
    unsigned char IsMoreEvent;
	unsigned char sta_addr[MACADDRLEN];
	unsigned char UnicastCipher;
	unsigned char MulticastCipher;
	unsigned char bInstallKey;
} DOT11_SET_FT_INFORMATION, *PDOT11_SET_FT_INFORMATION;

typedef struct _DOT11_AUTH_FT_INSERT_R0_KEY
{
    unsigned char EventId;
    unsigned char IsMoreEvent;
	unsigned char sta_addr[MACADDRLEN];
	unsigned char pmk_r0[FT_PMK_LEN];
	unsigned char pmk_r0_name[FT_PMKID_LEN];
} DOT11_AUTH_FT_INSERT_R0_KEY, *PDOT11_AUTH_FT_INSERT_R0_KEY;

typedef struct _DOT11_AUTH_FT_INSERT_R1_KEY
{
    unsigned char EventId;
    unsigned char IsMoreEvent;
	unsigned char sta_addr[MACADDRLEN];
	unsigned char bssid[MACADDRLEN];
	unsigned char r0kh_id[MAX_R0KHID_LEN];
	unsigned int r0kh_id_len;
	unsigned char pmk_r1[FT_PMK_LEN];
	unsigned char pmk_r1_name[FT_PMKID_LEN];
	unsigned char pmk_r0_name[FT_PMKID_LEN];
	unsigned int pairwise;
} DOT11_AUTH_FT_INSERT_R1_KEY, *PDOT11_AUTH_FT_INSERT_R1_KEY;

typedef struct _DOT11_AUTH_FT_TRIGGER_EVENT
{
    unsigned char EventId;
    unsigned char IsMoreEvent;
	unsigned char trigger_eventid;
	unsigned char sta_addr[MACADDRLEN];
} DOT11_AUTH_FT_TRIGGER_EVENT, *PDOT11_AUTH_FT_TRIGGER_EVENT;
#endif

/* moved from wapi_wai.h */
typedef struct __wapiCAAppPara {
	unsigned char	eventID;
	unsigned char	moreData;
	unsigned short	type;
	void			*ptr;
	char			name[IFNAMSIZ];
	unsigned char	data[0];
} __WLAN_ATTRIB_PACK__ wapiCAAppPara, COMM_PARAMS, *COMM_PARAMSp;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_GENERAL{
    unsigned char   EventId;
    unsigned char   IsMoreEvent;
    unsigned char   *Data;
}DOT11_GENERAL;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_NOEVENT{
    unsigned char   EventId;
    unsigned char   IsMoreEvent;
}DOT11_NO_EVENT;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_REQUEST{
	unsigned char   EventId;
}DOT11_REQUEST;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_WPA2_MULTICAST_CIPHER{
    unsigned char   EventId;
    unsigned char   IsMoreEvent;
    unsigned char	MulticastCipher;
}DOT11_WPA2_MULTICAST_CIPHER;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_WPA_MULTICAST_CIPHER{
    unsigned char   EventId;
    unsigned char   IsMoreEvent;
    unsigned char	MulticastCipher;
}DOT11_WPA_MULTICAST_CIPHER;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_ASSOCIATION_RSP{
    unsigned char   EventId;
    unsigned char   IsMoreEvent;
    char            MACAddr[MACADDRLEN];
    unsigned char   Status;
}DOT11_ASSOCIATIIN_RSP;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_ASSOCIATION_IND{
	unsigned char   EventId;
	unsigned char   IsMoreEvent;
	unsigned char   MACAddr[MACADDRLEN];
	unsigned short  RSNIELen;
	unsigned char   RSNIE[MAXRSNIELEN]; // include ID and Length by kenny
}DOT11_ASSOCIATION_IND;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_REASSOCIATION_IND{
	unsigned char   EventId;
	unsigned char   IsMoreEvent;
	unsigned char   MACAddr[MACADDRLEN];
	unsigned short  RSNIELen;
	unsigned char   RSNIE[MAXRSNIELEN];
	unsigned char   OldAPaddr[MACADDRLEN];
}DOT11_REASSOCIATION_IND;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_REASSOCIATION_RSP{
    unsigned char   EventId;
    unsigned char   IsMoreEvent;
    char            MACAddr[MACADDRLEN];
    unsigned char   Status;
    char            CurrAPaddr[MACADDRLEN];
}DOT11_REASSOCIATIIN_RSP;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_AUTHENTICATION_IND{
	unsigned char   EventId;
    unsigned char   IsMoreEvent;
    char            MACAddr[MACADDRLEN];
}DOT11_AUTHENTICATION_IND;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_REAUTHENTICATION_IND{
    unsigned char   EventId;
    unsigned char   IsMoreEvent;
    char            MACAddr[MACADDRLEN];
}DOT11_REAUTHENTICATION_IND;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_DEAUTHENTICATION_IND{
	unsigned char   EventId;
	unsigned char   IsMoreEvent;
	char            MACAddr[MACADDRLEN];
	unsigned long	tx_packets;       // == transmited packets
	unsigned long	rx_packets;       // == received packets
	unsigned long	tx_bytes;         // == transmited bytes
	unsigned long	rx_bytes;         // == received bytes
	unsigned long  	Reason;
}DOT11_DEAUTHENTICATION_IND;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_DISASSOCIATION_IND{
    unsigned char   EventId;
    unsigned char   IsMoreEvent;
    char            MACAddr[MACADDRLEN];
	unsigned long	tx_packets;       // == transmited packets
	unsigned long	rx_packets;       // == received packets
	unsigned long	tx_bytes;         // == transmited bytes
	unsigned long	rx_bytes;         // == received bytes
	unsigned long  	Reason;
}DOT11_DISASSOCIATION_IND;

/*moved from 8192cd_security.h*/
#if 1//def HS2_SUPPORT 
typedef struct _DOT11_WNM_NOTIFY{
    unsigned char   EventId;
    unsigned char   IsMoreEvent;
	unsigned char   macAddr[6];
    unsigned char   remedSvrURL[2048];
	unsigned char   serverMethod;
}DOT11_WNM_NOTIFY;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_WNM_DEAUTH_REQ{
    unsigned char   EventId;
    unsigned char   IsMoreEvent;
	unsigned char   macAddr[6];
	unsigned char   reason;
	unsigned short  reAuthDelay;
    unsigned char   URL[2048];
}DOT11_WNM_DEAUTH_REQ;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_BSS_SessInfo_URL{
    unsigned char   EventId;
    unsigned char   IsMoreEvent;
	unsigned char   macAddr[6];
	unsigned char   SWT;
    unsigned char   URL[2048];
}DOT11_BSS_SessInfo_URL;
#endif

/*moved from 8192cd_security.h*/
#if 1//def CONFIG_IEEE80211W 
/*HS2 R2 logo test*/
typedef struct _DOT11_INIT_11W_Flags {
	unsigned char	EventId;
	unsigned char	IsMoreEvent;
	unsigned char   dot11IEEE80211W;
    unsigned char   dot11EnableSHA256;
}DOT11_INIT_11W_Flags;

typedef struct _DOT11_SET_11W_Flags {
	unsigned char	EventId;
	unsigned char	IsMoreEvent;
	unsigned char	macAddr[MACADDRLEN];
	unsigned char   isPMF;
}DOT11_SET_11W_Flags;
#endif

/*moved from 8192cd_security.h*/
typedef struct _DOT11_SET_802DOT11{
	unsigned char   EventId;
	unsigned char   IsMoreEvent;
	unsigned char   VariableType;
	unsigned char   VariableValue;
	char            MACAddr[MACADDRLEN];
}DOT11_SET_802DOT11;

/*moved from 8192cd_security.h*/
typedef unsigned char DOT11_KEY_RSC[8];

typedef struct _DOT11_SET_KEY{
	unsigned char   EventId;
	unsigned char   IsMoreEvent;
	unsigned long   KeyIndex;
	unsigned long   KeyLen;
	unsigned char   KeyType;
	unsigned char	EncType;
	unsigned char   MACAddr[MACADDRLEN];
	DOT11_KEY_RSC   KeyRSC;
	unsigned char   KeyMaterial[64];
}DOT11_SET_KEY;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_DELETE_KEY{
	unsigned char   EventId;
	unsigned char   IsMoreEvent;
	char            MACAddr[MACADDRLEN];
	unsigned char   KeyType;
}DOT11_DELETE_KEY;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_SET_RSNIE{
	unsigned char   EventId;
	unsigned char   IsMoreEvent;
	unsigned short	Flag;
	unsigned short  RSNIELen;
	char            RSNIE[MAXRSNIELEN];
	char            MACAddr[MACADDRLEN];
}DOT11_SET_RSNIE;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_GKEY_TSC{
	unsigned char   EventId;
	unsigned char   IsMoreEvent;
	unsigned char	KeyTSC[8];
}DOT11_GKEY_TSC;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_MIC_FAILURE{
	unsigned char   EventId;
	unsigned char   IsMoreEvent;
	char            MACAddr[MACADDRLEN];
}DOT11_MIC_FAILURE;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_STA_QUERY_BSSID{
	unsigned char   EventId;
	unsigned char   IsMoreEvent;
	unsigned long   IsValid;
	char            Bssid[MACADDRLEN];
}DOT11_STA_QUERY_BSSID;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_STA_QUERY_SSID{
	unsigned char   EventId;
	unsigned char   IsMoreEvent;
	unsigned long   IsValid;
	char            ssid[32];
	int             ssid_len;
}DOT11_STA_QUERY_SSID;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_EAPOL_START{
	unsigned char   EventId;
	unsigned char   IsMoreEvent;
	char            MACAddr[MACADDRLEN];
}DOT11_EAPOL_START;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_SET_EXPIREDTIME{
    unsigned char EventId;
    unsigned char IsMoreEvent;
    unsigned char MACAddr[MACADDRLEN];
	unsigned long ExpireTime;
}DOT11_SET_EXPIREDTIME;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_QUERY_STATS{
	unsigned char   EventId;
	unsigned char   IsMoreEvent;
	unsigned char	MACAddr[MACADDRLEN];
	unsigned long   IsSuccess;
	unsigned long	tx_packets;       // == transmited packets
	unsigned long	rx_packets;       // == received packets
	unsigned long	tx_bytes;         // == transmited bytes
	unsigned long	rx_bytes;         // == received bytes
}DOT11_QUERY_STATS;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_ASSOCIATION_INFO
{
    unsigned char   EventId;
    unsigned char   IsMoreEvent;
    unsigned char   SupplicantAddress[MACADDRLEN];
    unsigned int	Length;
    unsigned short  AvailableRequestFixedIEs;
    struct _DOT11_AI_REQFI {
		unsigned short Capabilities;
		unsigned short ListenInterval;
		char    	   CurrentAPAddress[MACADDRLEN];
    } RequestFixedIEs;
    unsigned int 	RequestIELength;
    unsigned int 	OffsetRequestIEs;
    unsigned short	AvailableResponseFixedIEs;
    struct _DOT11_AI_RESFI {
        unsigned short Capabilities;
        unsigned short StatusCode;
        unsigned short AssociationId;
    } ResponseFixedIEs;
    unsigned int ResponseIELength;
    unsigned int OffsetResponseIEs;
} DOT11_ASSOCIATION_INFO, *PDOT11_ASSOCIATION_INFO;

//for user application use
typedef struct _DOT11_ASSOCIATION_INFO DOT11_ASSOCIATION_INFORMATION, *PDOT11_ASSOCIATION_INFORMATION;

/*moved from 8192cd_security.h*/
typedef struct _DOT11_INIT_QUEUE
{
    unsigned char EventId;
    unsigned char IsMoreEvent;
} DOT11_INIT_QUEUE, *PDOT11_INIT_QUEUE;

/* moved from 8192cd.h */
#pragma pack(1)
struct target_transition_list
{
    unsigned char 	addr[MACADDRLEN];
	unsigned char	mode; // bit 7 -- request mode; bit 6 -- disassoc imminent bit; bit 5 -- abridged bit
    unsigned short	disassoc_timer;
    unsigned char	target_bssid[MACADDRLEN];
	unsigned char	opclass;
    unsigned char	channel;
    //unsigned char	disassoc;
};
#pragma pack(0)


#endif //#ifndef KERNEL_USER_NO_SHARE_STRUCTURE

#endif // _8192CD_COMMON_H_

