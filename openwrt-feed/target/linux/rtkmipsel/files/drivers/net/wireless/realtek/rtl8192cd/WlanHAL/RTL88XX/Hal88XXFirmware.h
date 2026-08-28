#ifndef __HAL88XX_FIRMWARE_H__
#define __HAL88XX_FIRMWARE_H__

/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	Hal88XXFirmware.h
	
Abstract:
	Defined HAL 88XX Firmware data structure & Define
	    
Major Change History:
	When       Who               What
	---------- ---------------   -------------------------------
	2012-04-11 Filen            Create.	
--*/

#if defined(__ECOS)
#include "HalTxDutySetting.h"
#endif
#define H2C_PACKET_PAYLOAD_MAX_SIZE 32
#define H2C_ENTERY_MAX_NUM          32

#if CFG_HAL_WLAN_SUPPORT_FW_CMD
#define FWCMD_PACKET_PAYLOAD_MAX_SIZE 32
#define FWCMD_ENTERY_MAX_NUM          32
#endif

#ifndef REG_8051FW_CTRL
#define REG_8051FW_CTRL REG_MCUFW_CTRL
#endif
#ifndef BIT_FWDL_CHK_RPT
#define BIT_FWDL_CHK_RPT BIT_IMEM_BOOT_LOAD_CHKSUM_OK
#endif
#ifndef BIT_MCUFWDL_RDY
#define BIT_MCUFWDL_RDY BIT_IMEM_BOOT_LOAD_DW_OK
#endif
#ifndef BIT_WINTINI_RDY
#define BIT_WINTINI_RDY BIT_DMEM_CHKSUM_OK
#endif
/**********************/
// Mapping C2H callback function

#define ctrlInfoSZ  32

typedef struct _Ctrl_Info_ {
	u1Byte	SGI: 1;
	u1Byte Data_Rate: 7;
	u1Byte	PKT_DROP: 1;    
	u1Byte	TRY_RATE: 1;    
	u1Byte	Rsvd: 1;    
	u1Byte	DATA_BW: 2;    
	u1Byte	PWRSTS: 3;
	u1Byte	RTY_PKT_1SS_ANT_INFO_EN: 4;    
	u1Byte	Rsvd2: 1;    
	u1Byte	GROUP_TABLE_ID: 3;    
	u1Byte	RTY_LOW_RATE_EN: 1;    
	u1Byte	DATA_RTY_LOW_RATE: 7;    
	u1Byte	RTY_LMT_EN: 1;  
	u1Byte	RTS_RT_LOW_RATE: 7;    
	u1Byte	Rsvd3: 1;      
	u1Byte	PKT_TX_ONE_SEL: 1;  
	u1Byte	DATA_RT_LMT: 6;    
	u1Byte	Rsvd4: 8;          
	u1Byte	Rsvd5: 8;          
} Ctrl_Info , *PCtrl_Info;


typedef enum _RTL88XX_C2H_EVT
{
	C2H_88XX_DBG = 0,
	C2H_88XX_LB = 1,
	C2H_88XX_TXBF = 2,
	C2H_88XX_TX_REPORT = 3,
	C2H_88XX_TX_RATE =4,
	C2H_88XX_BT_INFO = 9,
	C2H_88XX_BT_MP = 11,
#ifdef TXRETRY_CNT
	C2H_88XX_TX_RETRY = 13, //0x0D
#endif	
	C2H_88XX_RA_PARA_RPT=14,
	C2H_88XX_RA_DYNAMIC_TX_PATH_RPT = 15,
        C2H_88XX_MACID_PAUSE_RPT = 0x21,
	C2H_88XX_APPS_INFO_RPT = 0x26,
	C2H_88XX_MU_SCORE_RPT = 0x31,
	C2H_88XX_EXTEND_IND = 0xFF,
	MAX_88XX_C2HEVENT
}RTL88XX_C2H_EVT;

typedef enum _RTL88XX_EXTEND_C2H_EVT
{
	EXTEND_C2H_88XX_DBG_PRINT = 0,
    EXTEND_C2H_88XX_H2C_ACK   = 0x01,
	EXTEND_C2H_88XX_CCX_RPT   = 0x0f,	
    EXTEND_C2H_88XX_ATM_RPT   = 0x11,
    EXTEND_C2H_88XX_DETECT_THERMAL	= 0x1d
}RTL88XX_EXTEND_C2H_EVT;

typedef struct _TXRPT_
{
    u1Byte RPT_MACID;
    u2Byte RPT_TXOK;    
    u2Byte RPT_TXFAIL;        
    u1Byte RPT_InitialRate;  
}__attribute__ ((packed)) TXRPT,*PTXRPT ;


typedef struct _APREQTXRPT_
{
    TXRPT txrpt[2];
}__attribute__ ((packed)) APREQTXRPT,*PAPREQTXRPT ;


#define GEN_FW_CMD_HANDLER(size, cmd)	{size, cmd##Handler},


//void	h2csetdsr(void);

struct cmdobj {
	u4Byte	        parmsize;
	VOID            (*c2hfuns)(IN HAL_PADAPTER Adapter,u1Byte *pbuf);	
};



//-----------------------------------------------------
//
//	0x1200h ~ 0x12FFh	DDMA CTRL
//
//-----------------------------------------------------
#define DDMA_LEN_MASK               0x0001FFFF
#define DDMA_CH_CHKSUM_CNT          BIT(24)
#define DDMA_RST_CHKSUM_STS         BIT(25)
#define DDMA_MODE_BLOCK_CPU         BIT(26)
#define DDMA_CHKSUM_FAIL            BIT(27)
#define DDMA_DA_W_DISABLE           BIT(28)
#define DDMA_CHKSUM_EN              BIT(29)
#define DDMA_CH_OWN                 BIT(31)




typedef struct _RTL88XX_FW_HDR_ 
{
    u2Byte      signature;
    u1Byte       category;
    u1Byte       function;

    u2Byte      version;
    u1Byte       subversion;
    u1Byte       rsvd1;

    u1Byte       month;      //human easy reading format
    u1Byte       day;        //human easy reading format
    u1Byte       hour;       //human easy reading format
    u1Byte       minute;     //human easy reading format

    u2Byte      ram_code_size;
    u1Byte       Foundry;  //0: TSMC,  1:UMC, 2:SMIC
    u1Byte       rsvd3;
    u4Byte        svnidx;
    u4Byte        rsvd5;
    u4Byte        rsvd6;
    u4Byte        rsvd7;
    
}RTL88XX_FW_HDR, *PRTL88XX_FW_HDR;

typedef struct _RTL88XX_MIPS_FW_HDR_ 
{
    //offset0
    u2Byte  signature;
    u1Byte  category;
    u1Byte  function;
    u2Byte  version;
    u1Byte  Subversion;
    u1Byte  sub_index;
    //offset8
    u4Byte  SVN_index;
    u4Byte  rsvd1;
    //offset16
    u1Byte  Month;
    u1Byte  Date;
    u1Byte  Hour;
    u1Byte  Min;
    u2Byte  Year;
    u1Byte  Foundry;
    u1Byte  rsvd2;
    //offset24
    u1Byte  MEM_USAGE__DL_from:1;
    u1Byte  MEM_USAGE__BOOT_from:1;
    u1Byte  MEM_USAGE__BOOT_LOADER:1;
    u1Byte  MEM_USAGE__IRAM:1;
    u1Byte  MEM_USAGE__ERAM:1;
    u1Byte  MEM_USAGE__rsvd4:3;
    u1Byte  rsvd3;
    u2Byte  BOOT_LOADER_SZ;
    u4Byte  rsvd5;
    //offset32
    u4Byte  TOTAL_DMEM_SZ;
    u2Byte  FW_CFG_SZ;
    u2Byte  FW_ATTR_SZ;
    //offset40
    u4Byte  IROM_SZ;
    u4Byte  EROM_SZ;           
    //offset 48
    u4Byte  IRAM_SZ;
    u4Byte  ERAM_SZ;
    //offset 56
    u4Byte  rsvd6;
    u4Byte  rsvd7;
}RTL88XX_MIPS_FW_HDR, *PRTL88XX_MIPS_FW_HDR;

// TODO: Filen, check below
typedef enum _RTL88XX_H2C_CMD 
{
//	H2C_88XX_RSVDPAGE               = 0,
	H2C_88XX_MSRRPT             	= 0x1,	
//	H2C_88XX_KEEP_ALIVE_CTRL    	= 0x3,
//	H2C_88XX_WO_WLAN            	= 0x5,	// Wake on Wlan.
//	H2C_88XX_REMOTE_WAKEUP      	= 0x7, 
	H2C_88XX_AP_OFFLOAD         	= 0x8,
	H2C_88XX_BCN_RSVDPAGE       	= 0x9,
	H2C_88XX_PROBE_RSVDPAGE     	= 0xa,
	H2C_88XX_APPS_OFFLOAD_SETTING   = 0xc,
    H2C_88XX_APPS_OFFLOAD_CTRL      = 0xd,	
	H2C_88XX_WAKEUP_PIN         	= 0x13,	
	H2C_88XX_MACID_PAUSE_DRV_INFO   = 0x1F,
//	H2C_88XX_SETPWRMODE         	= 0x20,		
//	H2C_88XX_P2P_PS_MODE        	= 0x24,
	H2C_88XX_SAP_PS             	= 0x26,
	H2C_88XX_MU_SCORE_RPT			= 0x32,
	H2C_88XX_RA_MASK            	= 0x40,
	H2C_88XX_RSSI_REPORT        	= 0x42,
	H2C_88XX_AP_REQ_TXREP			= 0x43,
	H2C_88XX_RA_MASK_3SS			= 0x46,
	H2C_88XX_RA_PARA_ADJUST 		= 0x47,
	H2C_88XX_DYNAMIC_TX_PATH		= 0x48,
	H2C_88XX_FW_TRACE_EN			= 0x49,
#ifdef BT_COEXIST	
	H2C_88XX_BT_TDMA				= 0x60, /* BT TDMA */
	H2C_88XX_BT_INFO				= 0x61, /* BT info */
	H2C_88XX_BT_63					= 0x63,
#endif	
	H2C_88XX_PWR_REDUCE				= 0xA2,  // for   8814B Power Consumption Reduce Plan
    H2C_88XX_ATM_REPORT				= 0xB3,
	H2C_88XX_NHM					= 0xC1,
	H2C_88XX_BCN_IGNORE_EDCCA		= 0xC2,
	H2C_88XX_REPEAT_WAKE_PULSE      = 0xC4,
	MAX_88XX_H2CCMD
}RTL88XX_H2C_CMD;


typedef enum _RTL88XX_C2H_CMD 
{
//	C2H_88XX_DBG                = 0,
//	C2H_88XX_C2H_LB             = 0x1,	
//	C2H_88XX_SND_TXBF           = 0x2,
//	C2H_88XX_CCXRPT             = 0x3,
	C2H_88XX_APREQTXRPT         = 0x4,
//	C2H_88XX_INITIALRATE        = 0x5,
//	C2H_88XX_PSD_RPT            = 0x6,
//	C2H_88XX_SCAN_COMPLETE      = 0x7, 
//	C2H_88XX_PSD_CONTROL        = 0x8,
//	C2H_88XX_BT_INFO            = 0x9,
//	C2H_88XX_BT_LOOPBACK        = 0xa,

	MAX_88XX_C2HCMD
}RTL88XX_C2H_CMD;


VOID
ReadMIPSFwHdr88XX(
    IN  HAL_PADAPTER    Adapter
);

enum rt_status
InitMIPSFirmware88XX(
    IN  HAL_PADAPTER    Adapter
);


enum rt_status
InitFirmware88XX(
    IN  HAL_PADAPTER    Adapter
);

u8Byte
  RateToBitmap_VHT88XX(
	pu1Byte			pVHTRate,
	u1Byte 		rf_mimo_mode
	
);



#if 0
typedef struct _H2C_CONTENT_
{
    u4Byte  content;
    u2Byte  ext_content;
}H2C_CONTENT, *PH2C_CONTENT;



BOOLEAN
IsH2CBufOccupy88XX(
    IN  HAL_PADAPTER    Adapter
);


BOOLEAN
SigninH2C88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  PH2C_CONTENT    pH2CContent
);
#else
BOOLEAN
CheckFwReadLastH2C88XX(
	IN  HAL_PADAPTER    Adapter,
	IN  u1Byte          BoxNum
);

enum rt_status
FillH2CCmd88XX(
	IN  HAL_PADAPTER    Adapter,
	IN	u1Byte 		    ElementID,
	IN	u4Byte 		    CmdLen,
	IN	pu1Byte		    pCmdBuffer
);
#endif
#ifdef CFG_HAL_WLAN_SUPPORT_H2C_PACKET
VOID
SetH2CPacketHeader88XX(
    IN VOID *pDriver_adapter, 
	OUT u8 *pHal_h2c_hdr, 
	IN u8 ack,
	IN u16 totallength,
	IN u8 category,
	IN u8 CMDID,
	IN u16 sub_CMDID
);
u8 
SendH2CPacket88XX(
    IN VOID *pDriver_adapter, 
    IN u8 *pBuf, 
    IN u32 size
);
#endif

#ifdef CFG_HAL_WLAN_SUPPORT_FW_CMD
enum rt_status 
FillFWCmd88XX(
	IN  HAL_PADAPTER    Adapter,
	IN	u1Byte 		    ElementID,
	IN  u2Byte          SubCmdID,
	IN	u4Byte 		    CmdLen,
	IN  u1Byte          needACK,
	IN	pu1Byte		    pCmdBuffer
);

VOID
SetFWCmdHeader88XX(
    IN VOID *pDriver_adapter, 
	OUT u8 *pHal_FWCmd_hdr, 
	IN u8 ack,
	IN u16 totallength,
	IN u8 category,
	IN u8 CMDID,
	IN u16 sub_CMDID
);
u8 
SendFWCmd88XX(
    IN VOID *pDriver_adapter, 
    IN u8 *pBuf, 
    IN u32 size
);
#endif
u8 
ParseC2HPacket88XX(
    IN VOID *pDriver_adapter, 
	IN u8 *pbuf, 
	IN u32 c2hsize
);


VOID
UpdateHalRAMask88XX(
	IN HAL_PADAPTER         Adapter,	
	HAL_PSTAINFO            pEntry,
	u1Byte				    rssi_level
);

VOID
UpdateHalRAMask8814A(
	IN HAL_PADAPTER         Adapter,
	HAL_PSTAINFO            pEntry,
	u1Byte                  rssi_level
);

void
UpdateHalMSRRPT88XX(
	IN HAL_PADAPTER     Adapter,
	HAL_PSTAINFO        pEntry,
	u1Byte              opmode
);

VOID SetMACIDPauseDriverInfo88XX
(
    IN HAL_PADAPTER  Adapter,
    IN  BOOLEAN      bSleep,
	IN  u4Byte       aid
);

void
SetAPOffload88XX(
	IN HAL_PADAPTER     Adapter,
	u1Byte              bEn,
#ifdef CONFIG_POWER_SAVE
	u1Byte              bOn,
#endif
	u1Byte              numOfAP,
	u1Byte              bHidden,	
	u1Byte              bDenyAny,
	pu1Byte             loc_bcn,
	pu1Byte             loc_probe
);
#if CFG_HAL_ATM_REPORT    
void
SetATMReportCfg88XX(
    IN HAL_PADAPTER     Adapter,
    u1Byte              bEn,
    u1Byte              EnRanges, //BIT0 range1, BIT1 range2, BIT3 range3
    u1Byte              ATM_period,
    pu1Byte             STAMACIDs, //[0] MACID1, [1] MACID2, [2] MACID3
	pu1Byte             STALengths, //[0] length1, [1] length2, [2] length3
	u1Byte              numOfSTA
);
#endif

#if CFG_HAL_PWR_REDUCE   
void
SetPWRReduce88XX(
    IN HAL_PADAPTER     Adapter,
    u1Byte              bEn,
    u1Byte              bForce_duty_cycle, //Only used by MP driver, force all rate to "Duty_cycle_by_forced"
    u1Byte              Duty_cycle_by_forced, //when "Force_duty_cycle" = 1, set this value to 5~100ms, the value will be mod 5 by FW to ensure the unit to be 5ms.
    u1Byte              NumOfRateSet, //The number of the RateIdx set in "Rate2DutyCycleMap".
	u1Byte              Loc_Rate2DutyCycleMap //The offset of "Rate2DutyCycleMap" in rsvd page.
);

void HalTxDutyParsing88XX
(
    IN      HAL_PADAPTER        Adapter,
    IN      WLAN_TXDUTY_CFG     TxDutyArray[]

);

void ClearHalTxDutyParsing88XX
(
    IN      HAL_PADAPTER        Adapter
);

void TxDutyCycleCtrl88XX(
    IN HAL_PADAPTER     Adapter
);

#endif


#ifdef HW_DETECT_PS_OFFLOAD
void 
MacidPauseCtrl88XX(
    IN HAL_PADAPTER     Adapter,  
    u1Byte macid,
    u1Byte setPause
);
#endif

#ifdef AP_PS_Offlaod
void
SetAPPSOffload88XX(
	IN HAL_PADAPTER     Adapter, 
    u1Byte _PS_offload_En, 
    u1Byte _phase, 
    u1Byte _pause_Qnum_limit,
    u1Byte _timeout_time
);
#endif


#ifdef AP_SWPS_OFFLOAD
void 
ClearDropID88XX(
    IN HAL_PADAPTER     Adapter,
    u1Byte macid,
    u1Byte DropID
);
void
MACIDSWPSCtrl88XX(
    IN HAL_PADAPTER     Adapter,
    struct stat_info *pstat,
    u1Byte enable
);
void
AgingFuncOfflaod88XX(
    IN HAL_PADAPTER     Adapter,
    u1Byte queuenum,
    u1Byte expiretime
);

void
UpdateMacidSWPSInfo88XX(
    IN HAL_PADAPTER     Adapter,
    u1Byte macid,
    u1Byte IsInSWPS

);
#endif

#if defined(SOFTAP_PS_DURATION) || defined(CONFIG_POWER_SAVE) || defined(CONFIG_PCIE_POWER_SAVING)
VOID SetSAPPS88XX
(    
	IN HAL_PADAPTER     Adapter,
        u1Byte en,
#if defined(CONFIG_POWER_SAVE) || defined(CONFIG_PCIE_POWER_SAVING)
	u1Byte              en_32K,
	u1Byte              lps,
#endif
        u1Byte duration
);
#endif // SOFTAP_PS_DURATION || CONFIG_POWER_SAVE

VOID
SetRsvdPage88XX
( 
	IN  IN HAL_PADAPTER     Adapter,
    IN  pu1Byte             prsp,
    IN  pu4Byte             beaconbuf,    
    IN  u4Byte              pktLen,  
    IN  u4Byte              bigPktLen,
    IN  u4Byte              bcnLen
);

u4Byte
GetRsvdPageLoc88XX
( 
	IN  IN HAL_PADAPTER     Adapter,
    IN  u4Byte              frlen,
    OUT pu1Byte             loc_page
);

BOOLEAN
DownloadRsvdPage88XX
( 
	IN HAL_PADAPTER     Adapter,
    IN  pu4Byte         beaconbuf,    
    IN  u4Byte          beaconPktLen,
    IN  u1Byte          bReDownload    
);

void C2HHandler88XX(
    IN HAL_PADAPTER     Adapter
);

void C2HPacket88XX(
	IN  HAL_PADAPTER    Adapter,
	IN  pu1Byte			pBuf,
	IN	u2Byte			length
);

VOID
C2HEventHandler88XX
(
    IN HAL_PADAPTER     Adapter,
    IN u1Byte			c2hCmdId, 
    IN u1Byte			c2hCmdLen,
    IN pu1Byte 			tmpBuf    			
);

VOID
C2HExtEventHandler88XX
(
    IN HAL_PADAPTER     Adapter,
    IN u1Byte			c2hCmdId, 
    IN u2Byte			c2hCmdLen,
    IN pu1Byte 			tmpBuf    			
);

#if (BEAMFORMING_SUPPORT == 1)
VOID
C2HTxBeamformingHandler88XX(
	struct rtl8192cd_priv *priv,
	pu1Byte			CmdBuf,
	u1Byte			CmdLen
);

VOID
C2HCCXRptHandler88XX(
	struct rtl8192cd_priv *priv,
	pu1Byte			CmdBuf,
	u1Byte			CmdLen
);

#if (MU_BEAMFORMING_SUPPORT == 1)
VOID
C2HTxBeamformingGroupDoneHandler88XX(
	struct rtl8192cd_priv *priv,
	pu1Byte			CmdBuf,
	u1Byte			CmdLen
);
#endif
#endif

#if CFG_HAL_ATM_REPORT    
VOID
C2HATMReportHandler88XX(
    struct rtl8192cd_priv *priv,
	pu1Byte			CmdBuf,
	u2Byte			CmdLen
);
#endif

VOID
C2HAPPSInfoHandler88XX(
	struct rtl8192cd_priv *priv,
	pu1Byte			CmdBuf,
	u1Byte			CmdLen
);

#endif  //__HAL88XX_FIRMWARE_H__

