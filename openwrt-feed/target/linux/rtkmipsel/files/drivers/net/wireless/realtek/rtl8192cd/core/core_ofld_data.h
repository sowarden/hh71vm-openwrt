/**
  *
  *  @file info.h 
  *  @brief Header of information for Data-CPU
  *         Interface, Station information
  *
  *  Copyright (c) 2015 Realtek Semiconductor Corp.
  *
  *  @author Peter Yu
  *  @date 2015/11/8
 **/

#ifndef _DATA_INFO_H
#define _DATA_INFO_H

#ifdef MBSSID
#define MULTI_BSSID_NUM     (RTL8192CD_NUM_VWLAN+1)
#else
#define MULTI_BSSID_NUM     (1)
#endif
#ifdef UNIVERSAL_REPEATER
#define CLIENT_BSSID_NUM	(1)
#else
#define CLIENT_BSSID_NUM	(0)
#endif
#define MULTI_PORT_NUM      (0)
#define CONFIG_MACID_NUM    (128)

#define _SIL_NULL_ID	0xff

/** @brief Interface information in Data-CPU
  *
  * ifID            : which interface
  *	vap_init_seq    : For AP only to record the vap_seq
  *	macAddr[6]      : MAC address of interface
  * band            : B/G/N/A/AC
  * operMode        : AP/STA/Ad-hoc
  * authType        : Open/Shared/Auto  
  * EncType         : WEP/WPA/WPA2/AES/TKIP/Mixed/802_1x  
  * fixedRate       : fixed rate setting
  * txforce         : force txrate
  * rtsThrshld      : Threshold of RTS
  * fragThrshold    : Threshold of fragment
  *	isProtection    : Use this for HWRTSEn¡BRTSEn¡BCTS2Self
  *	ctsToSelf       : Use this for HWRTSEn¡BRTSEn¡BCTS2Self
  * shortpreamble   : long/short preamble -- use for RTS Short
  *	aggrType        : Supported AGGR-method 
  *	currentBW       : Current Bandwidth, use for dataSC¡BRTSSC¡BdataBW
  *	longRetry       : Long-Retry limit
  *	shortRetry      : Short-Retry limit
  * longPreambleStaNum  : Number Long preamle station-- use for RTS Short
  *	tidMapping[8]   : which tid should be( or take this for every packet)
  *	txNoAck         : tx packect -- need ACK or not
  *	rsvd            : reserved	
 **/
typedef struct datacpu_info_if
{
	/* DWORD0/1 */
    UINT8 if_id;
    UINT8 vap_init_seq;
    UINT8 mac_addr[6];

	/* DWORD2 */
    UINT8 band;
    UINT8 oper_mode;
    UINT8 auth_type;
    UINT8 enc_type;
    
	/* DWORD3 */
    UINT16 rts_thrshld;
    UINT16 frag_thrshold;

	/* DWORD4 */
#ifdef _LITTLE_ENDIAN_
    UINT8 ht_protection :1;
    UINT8 erp_protection :1;
    UINT8 cts2self :1;
    UINT8 tx_no_ack :1;
    UINT8 shortpreamble :1;
    UINT8 current_bw :3;
    UINT8 long_retry;
    UINT8 short_retry;
    UINT8 longpreamble_sta_num;
#else /* _BIG_ENDIAN_ */
    UINT8 longpreamble_sta_num;
    UINT8 short_retry;
    UINT8 long_retry;
    UINT8 current_bw :3;
    UINT8 shortpreamble :1;
    UINT8 tx_no_ack :1;
    UINT8 cts2self :1;
    UINT8 erp_protection :1;
    UINT8 ht_protection :1;
#endif

	/* DWORD5 */
#ifdef _LITTLE_ENDIAN_
    UINT8 ampdu;
    UINT8 amsdu;
    UINT8 total_assoc_num;
    UINT8 cca_rts:1;
    UINT8 qos_enabled:1;
    UINT8 rsvd:6;
#else /* _BIG_ENDIAN_ */
    UINT8 rsvd:6;
    UINT8 qos_enabled:1;
    UINT8 cca_rts:1;
    UINT8 total_assoc_num;
    UINT8 amsdu;
    UINT8 ampdu;
#endif

	/* DWORD6 */
    UINT16 seq;
#ifdef CONFIG_DYNAMIC_SELF_MACID
    UINT16 self_macid;
#else
    UINT16 rsvd1;
#endif
}__WLAN_ATTRIB_PACK__   INFO_INTERFACE, *PINFO_INTERFACE;



/** @brief Station information in Data-CPU
  *
  * aid         : Aid of station
  * MacId       : MACID of station   //remappedAid of station
  *	ifID        : What interfaceID is the station belong
  * band        : support b/g/n/a/AC
  *	aggre_type  : Supported AGGR-method  
  * noRts       : use for HWRTSEn¡BRTSEn¡BCTS2Self
  * currentBW   : Current BW of station
  * leave       : Station is leave or not (but Data-CPU is not drop packet, so this maybe not needed)
  * rsvd        : reserved
  *	macAddress  : MAC address of station	 
  *	state       : Current link state (but Data-CPU is not drop packet, so this maybe not needed)
  * authType    : Open/Shared/Auto
  *	EncType     : WEP/WPA/WPA2/AES/TKIP/Mixed/802_1x
  * gAid        : BEAMFORMING_SUPPORT
  * pAid        : BEAMFORMING_SUPPORT
  * maxAggNum   : Max number of AGGR-Number	
  * expireTime  : expire tim of Station (but Data-CPU is not drop packet, so this maybe not needed)
 **/   
typedef struct datacpu_info_sta
{
	/* DWORD0 */
    UINT16  aid;
    UINT16  macid;        //remappedAid;

	/* DWORD1 */
    UINT8   if_id;
    UINT8   auth_type;
    UINT16  max_agg_num;

	/* DWORD2 */
    UINT32  state;

	/* DWORD3 */
    UINT32  p_aid;

	/* DWORD4 */
#ifdef _LITTLE_ENDIAN_
    UINT8   g_id;
    //----------------------
    UINT8   aggre_type:2;
    UINT8   no_rts:1;
    UINT8   cts2self:1;
    UINT8   tx_bw:3;
    UINT8   forced_rts:1;
    //----------------------
    UINT8   shortpreamble:1;
    UINT8   txsc_20:3;
    UINT8   txsc_40:3;
    UINT8   leave:1;
    //----------------------
    UINT8   mimo_ps:1;
    UINT8   qos_enabled:1;
    UINT8   keyid:2;
    UINT8   rsvd0:4;
#else /* _BIG_ENDIAN_ */
    UINT8   g_id;
    //----------------------
    UINT8   forced_rts:1;
    UINT8   tx_bw:3;
    UINT8   cts2self:1;
    UINT8   no_rts:1;
    UINT8   aggre_type:2;
    //----------------------
    UINT8   leave:1;
    UINT8   txsc_40:3;
    UINT8   txsc_20:3;
    UINT8   shortpreamble:1;
    //----------------------
    UINT8   rsvd0:4;
    UINT8   keyid:2;
    UINT8   qos_enabled:1;
    UINT8   mimo_ps:1;
#endif

	/* DWORD5/6 */
    UINT8   mac_addr[6];
    UINT8   vht_cap_len;
    UINT8   ampdu_density;

	/* DWORD7 */
    UINT8   enc_type;
    UINT8   rsvd1[3];
}__WLAN_ATTRIB_PACK__   INFO_STATION, *PINFO_STATION;

enum h2d_cmd_type {
	h2d_intf_add = 0,
	h2d_intf_update,
	h2d_intf_remove,
	h2d_intf_remove_all,
	h2d_sta_add,
	h2d_sta_update,
	h2d_sta_remove,
	h2d_sta_remove_all,
	h2d_pe_debug,
	h2d_sys_config,
	h2d_sys_start_fw,
	h2d_sys_stop_fw,
};

/*
 * HEADER:
 *  ctrl0@BIT00 - BIT02 : Type (8)
 *        BIT03 - BIT03 : SyncRequest (1)
 *        BIT04 - BIT07 : Category (16)
 *        BIT08 - BIT15 : ID (256)
 *        BIT16 - BIT26 : Length (2048)
 *        BIT27 - BIT29 : Packet ID (8)
 *        BIT30 - BIT31 : Version (4)
 *  ctrl1@BIT00 - BIT31 : RESERVED
 */

#define CIL_VERSION           (0x2)

#define CONFIG_CIL_PKT_LEN              (1500)
#define CONFIG_CIL_PKT_SYNC_INFO_LEN    (16)

#define CIL_HDR_LEN           (sizeof(u32) * 2)
#define CIL_PKT_SYNC_INFO_LEN (CONFIG_CIL_PKT_SYNC_INFO_LEN)
#define CIL_MAX_PKT_LEN       (CONFIG_CIL_PKT_LEN - CIL_HDR_LEN - CONFIG_CIL_PKT_SYNC_INFO_LEN)

#define CIL_MAX_CAT           (16)
#define CIL_MAX_ID            (256)

#define CIL_MAX_WAITING_PKT     CIL_MAX_PKT_ID
#define CIL_INVALID_PKT_ID      (CIL_MAX_WAITING_PKT + 1)
#define CIL_FLAGS_WAITING_SYNC      BIT(0)

/* ctrl0 field */
#define CIL_CTRL_TYPE_MSK       (0x7)
#define CIL_CTRL_SYNC_BIT       BIT(3)
#define CIL_CTRL_CAT_MSK        (0xf)
#define CIL_CTRL_ID_MSK         (0xff)
#define CIL_CTRL_LEN_MSK        (0x7ff)
#define CIL_CTRL_PKT_ID_MSK     (0x7)
#define CIL_CTRL_SYNC_DONE_BIT   BIT(31)

#define CIL_CTRL_TYPE_SH        (0)
#define CIL_CTRL_CAT_SH         (4)
#define CIL_CTRL_ID_SH          (8)
#define CIL_CTRL_LEN_SH         (16)
#define CIL_CTRL_PKT_ID_SH      (27)

/* ctrl1 field */
#define CIL_CTRL_RESV_MSK       (0xffffffff)

#define CIL_CTRL_RESV_SH        (0)

#define CIL_MAX_PKT_ID      (CIL_CTRL_PKT_ID_MSK)

enum cil_pkt_type_e {
    /* TX CMD */
    CIL_PKT_TYPE_H2D_CMD = 0,
    CIL_PKT_TYPE_D2C_CMD,

    /* RX EVT */
    CIL_PKT_TYPE_C2D_EVT,
    CIL_PKT_TYPE_D2H_EVT,

    /* keep last */
    CIL_PKT_TYPE_LAST = 4,
};

/* H2C CMD */
#define CIL_H2D_C_SYS                               (0)
#define CIL_H2D_I_SYS_CONFIG                            (0x0)
#define CIL_H2D_I_SYS_FWDL                              (0x1)
#define CIL_H2D_I_SYS_PROFILING                         (0xfd)
#define CIL_H2D_I_SYS_RAW_H2C                           (0xfe)
#define CIL_H2D_I_SYS_ECHO                              (0xff)

#define CIL_H2D_C_PE                                (1)
#define CIL_H2D_I_PE_UPDATE_DEVICE                      (0x0)
#define CIL_H2D_I_PE_UPDATE_SIL                         (0x1)
#define CIL_H2D_I_PE_UPDATE_MACID                       (0x2)
#define CIL_H2D_I_PE_REMOVE_MACID                       (0x3)
#define CIL_H2D_I_PE_REMOVE_ALL_MACID                   (0x4)
#define CIL_H2D_I_PE_DEBUG                              (0x80)

#define CIL_H2D_C_WMAC                              (2)
#define CIL_H2D_I_WMAC_TX_MGT                           (0x0)
#define CIL_H2D_I_WMAC_SCAN                             (0x1)
#define CIL_H2D_I_WMAC_RAW_H2C                          (0xff)

#define CIL_H2D_C_WMAC_AP                           (3)
#define CIL_H2D_I_WMAC_AP_BCN                           (0x0)

/* D2C CMD */
#define CIL_D2C_C_MCU                               (0)
#define CIL_D2C_I_MCU_RAW_H2C                           (0x0)

/* D2H EVT */
#define CIL_D2H_C_SYS                               (0)
#define CIL_D2H_I_SYS_ACK                               (0x0)
#define CIL_D2H_I_SYS_READY                             (0x1)
#define CIL_D2H_I_SYS_FWD_PRINT                         (0x2)
#define CIL_D2H_I_SYS_FWLOG                             (0xfe)
#define CIL_D2H_I_SYS_ECHO                              (0xff)

#define CIL_D2H_C_WMAC                              (1)
#define CIL_D2H_I_WMAC_RX_MGT                           (0x0)
#define CIL_D2H_I_WMAC_RAW_C2H                          (0xff)

/* C2D EVT */
#define CIL_C2D_C_MCU                               (0)
#define CIL_C2D_I_MCU_RAW_C2H                           (0x0)

struct cil_pkt_t {
    u32 ctrl0;
    u32 ctrl1;

    u8 buf[CIL_MAX_PKT_LEN];
    u8 sync_info[CIL_PKT_SYNC_INFO_LEN];
} __WLAN_ATTRIB_PACK__;

#define CIL_H2D_HDR_LEN       (sizeof(u32) * 2)

struct cil_d2h_sys_ack_t {
    u32 ack_pkt_id;
    u32 rsvd;
} __WLAN_ATTRIB_PACK__;

/* @@@CIL_D2H_I_SYS_READY */
#define CIL_D2H_SYS_READY_FLAGS_DCNT                BIT(0)
#define CIL_D2H_SYS_READY_FLAGS_WDT                 BIT(1)
#define CIL_D2H_SYS_READY_FLAGS_PHYDM               BIT(2)
#define CIL_D2H_SYS_READY_FLAGS_MCU_FW              BIT(3)
#define CIL_D2H_SYS_READY_FLAGS_HALMAC              BIT(4)
#define CIL_D2H_SYS_READY_FLAGS_FWLOG               BIT(5)
#define CIL_D2H_SYS_READY_FLAGS_HV                  BIT(31)

struct cil_d2h_sys_ready_t {
    /* KEEP FIRST: used to negotiate CIL version before starting service */
    u8 cil_ofldVer;
    u8 nego_code;
    u8 common_rsvd[2];
    /* ADD NEW FIELD AFTER THIS */

    u32 sys_feature0;        /* SYSTEM: CIL_D2H_SYS_READY_FLAGS_xxx */
    u32 sys_feature1;        /* SYSTEM */
    u32 sys_feature2;        /* SYSTEM */
    u32 sys_feature3;        /* SYSTEM */
    u32 fw_version;          /* VERSION */
    u8 fwlog_version;        /* VERSION */
    u8 rsvd_[3];
    u32 wmac_version;        /* VERSION */
    u32 rsvd;
    u32 dcnt_magicAddr;      /* DCOUNTER */
    u32 pe_portNum;          /* PE */
    u32 pe_mbssidNum;        /* PE */
    u32 pe_macIdNum;         /* PE */
    u32 pe_ofldFunc;         /* PE */
    u8 phydm_regd[2];        /* WMAC-BB */
    u16 phydm_ofldFunc;      /* WMAC-BB */
    u8 wdt_mode;             /* WDT */
    u8 rsvd1[3];
    u8 mcu_fwInfo[32];       /* WMAC-MCU */
};

#define _H2D_SYS_CONF_INIT            BIT(0)
#define _H2D_SYS_CONF_DEINIT          BIT(1)
#define _H2D_SYS_CONF_FWLOG           BIT(30)
#define _H2D_SYS_CONF_DBG_PRINT       BIT(31)
struct cil_h2d_sys_config_t {
    /* KEEP FIRST: used to negotiate CIL version before starting service */
    u8 cil_hostVer;
    u8 common_rsvd[3];
    u32 conf_mask;                   /* _H2D_SYS_CONF_xxx */
    /* ADD NEW FIELD AFTER THIS */

    union {
        struct {
            u32 sys_feature;
            u32 rsvd[4];
            u32 pe_ofldFunc;
            u8 phydm_regd[2];
            u16 phydm_ofldFunc;
        } init;

        struct {
            u32 dummy;
        } deinit;

        struct {
            u32 level;
        } dbg_print;

        struct {
            u32 en_cat_mask;
            u32 en_ts;
			u32 en_pm;
        } fwlog;
    };
} __WLAN_ATTRIB_PACK__;

struct cil_h2d_pe_dbgcmd_t {
#define CIL_H2D_PE_DBGCMD_SID_MSK        (0x1f)
#define CIL_H2D_PE_DBGCMD_TYPE_MSK       (0xff)
#define CIL_H2D_PE_DBGCMD_SID_SH         (0)
#define CIL_H2D_PE_DBGCMD_TYPE_SH        (5)
/* dump basic information */
#define CIL_H2D_PE_DBGCMD_DUMP_IF        (0x0)
#define CIL_H2D_PE_DBGCMD_DUMP_STA       (0x1)
#define CIL_H2D_PE_DBGCMD_DUMP_MACID     (0x2)
#define CIL_H2D_PE_DBGCMD_DUMP_SMACID    (0x3)
/* dump Tx/Rx information */
#define CIL_H2D_PE_DBGCMD_DUMP_RING      (0x10)
/* debug option */
#define CIL_H2D_PE_DBGCMD_GDBGLVL        (0x20)
/* temp debug option */
#define CIL_H2D_PE_DBGCMD_RESET_IDLE     (0x80)
#define CIL_H2D_PE_DBGCMD_GDCNTADDR      (0x81)
#define CIL_H2D_PE_DBGCMD_DUMP_TXIE      (0x82)
#define CIL_H2D_PE_DBGCMD_TXRATE         (0x83)
#define CIL_H2D_PE_DBGCMD_DATABW         (0x84)
#define CIL_H2D_PE_DBGCMD_AMSDU_CNT      (0x85)
#define CIL_H2D_PE_DBGCMD_BCN_MODE       (0x86)
    u32 word0;
#define CIL_H2D_PE_DBGCMD_PARAM_MSK      (0xff)
#define CIL_H2D_PE_DBGCMD_PARAM_SH       (0)
    u32 word1;
} __WLAN_ATTRIB_PACK__;

struct cil_h2d_wmac_ap_bcn_t {
#define CIL_H2D_WMAC_AP_BCN_SID_MSK         (0x1f)
#define CIL_H2D_WMAC_AP_BCN_SID_SH          (0)
    u32 word0;
    u32 word1;
} __WLAN_ATTRIB_PACK__;

struct cil_h2d_pe_update_sil_t {
#define CIL_H2D_PE_UPDATE_SIL_SID_MSK       (0x1f)
#define CIL_H2D_PE_UPDATE_SIL_SID_SH        (0)
#define CIL_H2D_PE_UPDATE_SIL_OPCODE_MSK    (0xf)
#define CIL_H2D_PE_UPDATE_SIL_OPCODE_SH     (5)
    u32 word0;
    u32 word1;
} __WLAN_ATTRIB_PACK__;

struct cil_h2d_pe_update_macid_t {
#define CIL_H2D_PE_UPDATE_MACID_SID_MSK     (0x1f)
#define CIL_H2D_PE_UPDATE_MACID_SID_SH      (0)
#define CIL_H2D_PE_UPDATE_MACID_OPCODE_MSK  (0xf)
#define CIL_H2D_PE_UPDATE_MACID_OPCODE_SH   (5)
    u32 word0;
    u32 word1;
} __WLAN_ATTRIB_PACK__;

#define CIL_H2D_PE_OPCODE_ADD               (0)
#define CIL_H2D_PE_OPCODE_UPDATE            (1)
#define CIL_H2D_PE_OPCODE_DEL               (2)
#define CIL_H2D_PE_OPCODE_CLEAN             (3)

#define CIL_PKT_SYNC_INFO_SET(_sync_info, _cb, _h)  do {                       \
    *((u32 *)(_sync_info) + 0) = ((u32)(_cb));                           \
    *((u32 *)(_sync_info) + 1) = ((u32)(_h));                            \
    } while (0);
#define CIL_PKT_SYNC_INFO_GET(_sync_info, _cb, _h)  do {                       \
    (_cb) = *((u32 *)(_sync_info) + 0);                                     \
    (_h) = *((u32 *)(_sync_info) + 1);                                      \
    } while (0);

#define OFFSET_VALUE(_v, _f) (((_v) & _f##_MSK) << _f##_SH)

#define CIL_PKT_GET_BUF(_skbuf)            \
                        (((struct cil_pkt_t *)_skbuf)
#define CIL_PKT_SET_CTRL(_skbuf, _t, _c, _i, _l, _s, _d, _cb, _h) do {         \
    struct cil_pkt_t *__cil_pkt = (struct cil_pkt_t *)(_skbuf);                \
    (__cil_pkt)->ctrl0 = (OFFSET_VALUE((_t), CIL_CTRL_TYPE) |                  \
                          OFFSET_VALUE((_c), CIL_CTRL_CAT) |                   \
                          OFFSET_VALUE((_i), CIL_CTRL_ID) |                    \
                          OFFSET_VALUE((_l), CIL_CTRL_LEN));                   \
    (__cil_pkt)->ctrl1 = CIL_CTRL_RESV_MSK;                                    \
    } while(0);
#define CIL_PKT_STAMP(_skbuf, _id)                                    do {     \
    struct cil_pkt_t *__cil_pkt = (struct cil_pkt_t *)(_skbuf);                \
    (__cil_pkt)->ctrl0 |= (OFFSET_VALUE((_id), CIL_CTRL_PKT_ID));              \
    } while(0);    

#define CIL_PKT_GET_SYNC_INFO(_skbuf)                                          \
    ((struct cil_pkt_t *)_skbuf)->sync_info
#define CIL_PKT_GET_SYNC(_skbuf)                                               \
    (((struct cil_pkt_t *)_skbuf)->ctrl0 & CIL_CTRL_SYNC_BIT ? 1 : 0)
#define CIL_PKT_GET_SYNC_DONE(_skbuf)                                               \
    (((struct cil_pkt_t *)_skbuf)->ctrl0 & CIL_CTRL_SYNC_DONE_BIT ? 1 : 0)
#define CIL_PKT_GET_TYPE(_skbuf)                                               \
    GET_FIELD((((struct cil_pkt_t *)_skbuf)->ctrl0), CIL_CTRL_TYPE)
#define CIL_PKT_GET_CAT(_skbuf)                                                \
    GET_FIELD((((struct cil_pkt_t *)_skbuf)->ctrl0), CIL_CTRL_CAT)
#define CIL_PKT_GET_ID(_skbuf)                                                 \
    GET_FIELD((((struct cil_pkt_t *)_skbuf)->ctrl0), CIL_CTRL_ID)
#define CIL_PKT_GET_LEN(_skbuf)                                                \
    GET_FIELD((((struct cil_pkt_t *)_skbuf)->ctrl0), CIL_CTRL_LEN)
#define CIL_PKT_GET_PKT_ID(_skbuf)                                             \
    GET_FIELD((((struct cil_pkt_t *)_skbuf)->ctrl0), CIL_CTRL_PKT_ID)

#endif /* #ifndef _DATA_TYPE_H */
