#ifndef _CORE_WIFI_H_
#define _CORE_WIFI_H_



#ifdef BIT
#undef BIT
#endif
#define BIT(x)	(1 << (x))


#define WLAN_ETHHDR_LEN		14
#define WLAN_ADDR_LEN		6
#define WLAN_MIN_ETHFRM_LEN	60
#define WLAN_MAX_ETHFRM_LEN	1514
#define WLAN_HDR_A3_LEN		24
#define WLAN_HDR_A3_QOS_LEN	26
#define WLAN_HDR_A4_QOS_LEN	32

#define _TO_DS_		BIT(8)
#define _FROM_DS_	BIT(9)
#define _MORE_FRAG_	BIT(10)
#define _RETRY_		BIT(11)
#define _PWRMGT_	BIT(12)
#define _MORE_DATA_	BIT(13)
#define _PRIVACY_	BIT(14)
#define _ORDER_	    BIT(15)

/*-----------------------------------------------------------------------------
			Below is for the security related definition
------------------------------------------------------------------------------*/
#define _RESERVED_FRAME_TYPE_	0
#define _SKB_FRAME_TYPE_	2

#define _PRE_ALLOCMEM_			1
#define _PRE_ALLOCHDR_			3
#define _PRE_ALLOCLLCHDR_		4
#define _PRE_ALLOCICVHDR_		5
#define _PRE_ALLOCMICHDR_		6

#define _CRCLNG_			4

/*----------------------------------------------------------------------------*/




/*-----------------------------------------------------------------------------
			Below is the bit definition for HT Capabilities element
------------------------------------------------------------------------------*/
#define _HTCAP_SHORTGI_20M_		BIT(5)
#define _HTCAP_SHORTGI_40M_		BIT(6)
#define _HTCAP_AMPDU_SPC_SHIFT_		2
#define _HTCAP_AMPDU_SPC_MASK_		0x1c

/*----------------------------------------------------------------------------*/




/*-----------------------------------------------------------------------------
				Below is the definition for 802.11i / 802.1x
------------------------------------------------------------------------------*/
#define _IEEE8021X_MGT_			1		// WPA
#define _IEEE8021X_PSK_			2		// WPA with pre-shared key

#define _NO_PRIVACY_			0
#define _WEP_40_PRIVACY_		1
#define _TKIP_PRIVACY_			2
#define _WRAP_PRIVACY_			3
#define _CCMP_PRIVACY_			4
#define _WEP_104_PRIVACY_		5
#define _WEP_WPA_MIXED_PRIVACY_ 	6	// WEP + WPA
#define _WAPI_SMS4_			7
/*----------------------------------------------------------------------------*/



/*-----------------------------------------------------------------------------
			Below is for HT related definition
------------------------------------------------------------------------------*/
#define _HT_MIMO_PS_STATIC_		BIT(0)
#define _HT_MIMO_PS_DYNAMIC_		BIT(1)
/*----------------------------------------------------------------------------*/





#define GetPwrMgt(pbuf)	(((*(unsigned short *)(pbuf)) & le16_to_cpu(_PWRMGT_)) != 0)
#define GetTupleCache(pbuf)	(cpu_to_le16(*(unsigned short *)((unsigned long)(pbuf) + 22)))

#define GetAddr1Ptr(pbuf)	((unsigned char *)((unsigned long)(pbuf) + 4))
#define GetAddr2Ptr(pbuf)	((unsigned char *)((unsigned long)(pbuf) + 10))

#define GetFrameType(pbuf)	(le16_to_cpu(*(unsigned short *)(pbuf)) & (BIT(3) | BIT(2)))
#define GetFrameSubType(pbuf)	(cpu_to_le16(*(unsigned short *)(pbuf)) & (BIT(7) | BIT(6) | BIT(5) | BIT(4) | BIT(3) | BIT(2)))
#define GetQosControl(pbuf) (unsigned char *)((unsigned long)(pbuf) + (((GetToDs(pbuf)<<1)|GetFrDs(pbuf))==3?30:24))

#define GetPrivacy(pbuf)	(((*(unsigned short *)(pbuf)) & le16_to_cpu(_PRIVACY_)) != 0)
#define SetPrivacy(pbuf)	\
	do	{	\
		*(unsigned short *)(pbuf) |= cpu_to_le16(_PRIVACY_); \
	} while(0)

#define GetFragNum(pbuf)	(cpu_to_le16(*(unsigned short *)((unsigned long)(pbuf) + 22)) & 0x0f)
#define SetFragNum(pbuf, num) \
	do {    \
		*(unsigned short *)((unsigned long)(pbuf) + 22) = \
			((*(unsigned short *)((unsigned long)(pbuf) + 22)) & le16_to_cpu(~(0x000f))) | \
			cpu_to_le16(0x0f & (num));     \
	} while(0)


#define GetMFrag(pbuf)	(((*(unsigned short *)(pbuf)) & le16_to_cpu(_MORE_FRAG_)) != 0)
#define SetMFrag(pbuf)	\
	do	{	\
		*(unsigned short *)(pbuf) |= cpu_to_le16(_MORE_FRAG_); \
	} while(0)
#define ClearMFrag(pbuf)	\
	do	{	\
		*(unsigned short *)(pbuf) &= (~cpu_to_le16(_MORE_FRAG_)); \
	} while(0)


#define GetSequence(pbuf)	(cpu_to_le16(*(unsigned short *)((unsigned long)(pbuf) + 22)) >> 4)

#define GetRetry(pbuf)	(((*(unsigned short *)(pbuf)) & le16_to_cpu(_RETRY_)) != 0)

#define SetFrameType(pbuf,type)	\
	do { 	\
		*(unsigned short *)(pbuf) &= __constant_cpu_to_le16(~(BIT(3) | BIT(2))); \
		*(unsigned short *)(pbuf) |= __constant_cpu_to_le16(type); \
	} while(0)

#define SetFrameSubType(pbuf,type) \
	do {    \
		*(unsigned short *)(pbuf) &= cpu_to_le16(~(BIT(7) | BIT(6) | BIT(5) | BIT(4) | BIT(3) | BIT(2))); \
		*(unsigned short *)(pbuf) |= cpu_to_le16(type); \
	} while(0)

#define SetFrDs(pbuf)	\
		do	{	\
			*(unsigned short *)(pbuf) |= cpu_to_le16(_FROM_DS_); \
		} while(0)
	
#define GetFrDs(pbuf)	(((*(unsigned short *)(pbuf)) & le16_to_cpu(_FROM_DS_)) != 0)
	
#define ClearFrDs(pbuf)	\
		do	{	\
			*(unsigned short *)(pbuf) &= (~cpu_to_le16(_FROM_DS_)); \
		} while(0)

#define SetSeqNum(pbuf, num) \
			do {	\
				*(unsigned short *)((unsigned long)(pbuf) + 22) = \
					((*(unsigned short *)((unsigned long)(pbuf) + 22)) & le16_to_cpu((unsigned short)~0xfff0)) | \
					le16_to_cpu((unsigned short)(0xfff0 & (num << 4))); \
			} while(0)
#define SetMData(pbuf)	\
					do	{	\
						*(unsigned short *)(pbuf) |= cpu_to_le16(_MORE_DATA_); \
					} while(0)
				
#define GetMData(pbuf)	(((*(unsigned short *)(pbuf)) & le16_to_cpu(_MORE_DATA_)) != 0)
#define SetDuration(pbuf, dur) \
	do {    \
		*(unsigned short *)((unsigned long)(pbuf) + 2) |= cpu_to_le16(0xffff & (dur)); \
	} while(0)


/**
 *	@brief Frame type value
 *  See 802.11 Table.1 , type value define by bit2 bit3
 */
enum WIFI_FRAME_TYPE {
	WIFI_MGT_TYPE  =	(0),
	WIFI_CTRL_TYPE =	(BIT(2)),
	WIFI_DATA_TYPE =	(BIT(3)),

#ifdef CONFIG_RTK_MESH
	// Hardware of 8186 doesn't support it. Confirm by David, 2007/1/5
	WIFI_EXT_TYPE  =	(BIT(2) | BIT(3))	///< 11 is 802.11S Extended Type
#endif
};


/**
 *	@brief WIFI_FRAME_SUBTYPE
 *  See 802.11 Table.1 valid type and subtype combinations
 *
 */
enum WIFI_FRAME_SUBTYPE {

	// below is for mgt frame
	WIFI_ASSOCREQ       = (0 | WIFI_MGT_TYPE),
    WIFI_ASSOCRSP       = (BIT(4) | WIFI_MGT_TYPE),
    WIFI_REASSOCREQ     = (BIT(5) | WIFI_MGT_TYPE),
    WIFI_REASSOCRSP     = (BIT(5) | BIT(4) | WIFI_MGT_TYPE),
    WIFI_PROBEREQ       = (BIT(6) | WIFI_MGT_TYPE),
    WIFI_PROBERSP       = (BIT(6) | BIT(4) | WIFI_MGT_TYPE),
    WIFI_BEACON         = (BIT(7) | WIFI_MGT_TYPE),
    WIFI_ATIM           = (BIT(7) | BIT(4) | WIFI_MGT_TYPE),
    WIFI_DISASSOC       = (BIT(7) | BIT(5) | WIFI_MGT_TYPE),
    WIFI_AUTH           = (BIT(7) | BIT(5) | BIT(4) | WIFI_MGT_TYPE),
    WIFI_DEAUTH         = (BIT(7) | BIT(6) | WIFI_MGT_TYPE),
    WIFI_WMM_ACTION		= (BIT(7) | BIT(6) | BIT(4) | WIFI_MGT_TYPE),
#ifdef CONFIG_RTK_MESH
	WIFI_MULTIHOP_ACTION 	= (BIT(7) | BIT(6) | BIT(5) |BIT(4) | WIFI_MGT_TYPE),	// (Refer: Draft 1.06, Page 8, 7.1.3.1.2, Table 7-1, 2007/08/13 by popen)
#endif

    // below is for control frame
    WIFI_BLOCKACK_REQ	= (BIT(7) | WIFI_CTRL_TYPE),
    WIFI_BLOCKACK		= (BIT(7) | BIT(4) | WIFI_CTRL_TYPE),
    WIFI_PSPOLL         = (BIT(7) | BIT(5) | WIFI_CTRL_TYPE),
    WIFI_RTS            = (BIT(7) | BIT(5) | BIT(4) | WIFI_CTRL_TYPE),
    WIFI_CTS            = (BIT(7) | BIT(6) | WIFI_CTRL_TYPE),
    WIFI_ACK            = (BIT(7) | BIT(6) | BIT(4) | WIFI_CTRL_TYPE),
    WIFI_CFEND          = (BIT(7) | BIT(6) | BIT(5) | WIFI_CTRL_TYPE),
    WIFI_CFEND_CFACK    = (BIT(7) | BIT(6) | BIT(5) | BIT(4) | WIFI_CTRL_TYPE),

    // below is for data frame
    WIFI_DATA           = (0 | WIFI_DATA_TYPE),
    WIFI_QOS_DATA       = (BIT(7) | WIFI_DATA_TYPE),
    WIFI_DATA_CFACK     = (BIT(4) | WIFI_DATA_TYPE),
    WIFI_DATA_CFPOLL    = (BIT(5) | WIFI_DATA_TYPE),
    WIFI_DATA_CFACKPOLL = (BIT(5) | BIT(4) | WIFI_DATA_TYPE),
    WIFI_DATA_NULL      = (BIT(6) | WIFI_DATA_TYPE),
    WIFI_CF_ACK         = (BIT(6) | BIT(4) | WIFI_DATA_TYPE),
    WIFI_CF_POLL        = (BIT(6) | BIT(5) | WIFI_DATA_TYPE),
    WIFI_CF_ACKPOLL     = (BIT(6) | BIT(5) | BIT(4) | WIFI_DATA_TYPE),

#ifdef CONFIG_RTK_MESH    // (CAUTION!! Below not exist in D1.06!!)
	// Because hardware of RTL8186 doen's support TYPE=11, we use BIT(7) | WIFI_DATA_TYPE to
	// simulate TYPE=11, 2007/1/8
    WIFI_11S_MESH	    = (BIT(7) | WIFI_DATA_TYPE),	// CAUTION!! Below not exist in D1.06!!
    WIFI_11S_MESH_ACTION = (BIT(5) | WIFI_11S_MESH),	///< Mesh Action
#endif

};

/**
 *	@brief	REASON CODE
 *	16 bit field, See textbook Table.4-5.
 */
enum WIFI_REASON_CODE	{
	_RSON_RESERVED_					= 0,	// Reserved.
	_RSON_UNSPECIFIED_				= 1,	// Unspecified.
	_RSON_AUTH_NO_LONGER_VALID_		= 2,	// Auth invalid.
	_RSON_DEAUTH_STA_LEAVING_		= 3,	// STA leave BSS or ESS, DeAuth.
	_RSON_INACTIVITY_				= 4,	// Exceed idle timer, Disconnect.
	_RSON_UNABLE_HANDLE_			= 5,	// BS resoruce insufficient.
	_RSON_CLS2_						= 6,	// UnAuth STA frame or SubType incorrect.
	_RSON_CLS3_						= 7, 	// UnAuth STA frame or SubType incorrect.
	_RSON_DISAOC_STA_LEAVING_		= 8,	// STA leave BSS or ESS, DeAssoc.
	_RSON_ASOC_NOT_AUTH_			= 9,	// Request assiciate or reassociate, before authenticate
	// 10,11,12 for 802.11h
	// WPA reason
#ifdef CONFIG_IEEE80211V
	_RSON_DISASSOC_DUE_BSS_TRANSITION = 12,	//Disassociated due to BSS Transition Management	
#endif
	_RSON_INVALID_IE_				= 13,
	_RSON_MIC_FAILURE_				= 14,
	_RSON_4WAY_HNDSHK_TIMEOUT_		= 15,
	_RSON_GROUP_KEY_UPDATE_TIMEOUT_	= 16,
	_RSON_DIFF_IE_					= 17,
	_RSON_MLTCST_CIPHER_NOT_VALID_	= 18,
	_RSON_UNICST_CIPHER_NOT_VALID_	= 19,
	_RSON_AKMP_NOT_VALID_			= 20,
	_RSON_UNSUPPORT_RSNE_VER_		= 21,
	_RSON_INVALID_RSNE_CAP_			= 22,
	_RSON_IEEE_802DOT1X_AUTH_FAIL_	= 23,

	//belowing are Realtek definition
	_RSON_PMK_NOT_AVAILABLE_		= 24,
/*#if defined(CONFIG_RTL_WAPI_SUPPORT)*/
	_RSON_USK_HANDSHAKE_TIMEOUT_	= 25,	/* handshake timeout for unicast session key*/
	_RSON_MSK_HANDSHAKE_TIMEOUT_	= 26,	/* handshake timeout for multicast session key*/
	_RSON_IE_NOT_CONSISTENT_			= 27,	/* IE was different between USK handshake & assocReq/probeReq/Beacon */
	_RSON_INVALID_USK_				= 28,	/* Invalid unicast key set */
	_RSON_INVALID_MSK_				= 29,	/* Invalid multicast key set */
	_RSON_INVALID_WAPI_VERSION_		= 30,	/* Invalid wapi version */
	_RSON_INVALID_WAPI_CAPABILITY_	= 31,	/* Wapi capability not support */
/*#endif*/

    //below are mesh definition
    _RSON_MESH_CHANNEL_SWITCH_REGULATORY_REQUIREMENTS_ = 65,
    _RSON_MESH_CHANNEL_SWITCH_UNSPECIFIED_ = 66,
};






#endif

