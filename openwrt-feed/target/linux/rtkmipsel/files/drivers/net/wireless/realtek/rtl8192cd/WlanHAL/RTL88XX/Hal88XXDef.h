#ifndef __HAL88XX_DEF_H__
#define __HAL88XX_DEF_H__

/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	Hal88XXDef.h
	
Abstract:
	Defined HAL 88XX common data structure & Define
	    
Major Change History:
	When       Who               What
	---------- ---------------   -------------------------------
	2012-03-23 Filen            Create.	
--*/

#ifdef  WLAN_HAL_INTERNAL_USED

enum rf_type
GetChipIDMIMO88XX(
    IN  HAL_PADAPTER        Adapter
);


VOID
CAMEmptyEntry88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  u1Byte          index
);


u4Byte
CAMFindUsable88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          for_begin
);


VOID
CAMReadMACConfig88XX
(
    IN  HAL_PADAPTER    Adapter,
    IN  u1Byte          index, 
    OUT pu1Byte         pMacad,
    OUT PCAM_ENTRY_CFG  pCfg
);


VOID
CAMProgramEntry88XX(
    IN	HAL_PADAPTER		Adapter,
    IN  u1Byte              index,
    IN  pu1Byte             macad,
    IN  pu1Byte             key128,
    IN  u2Byte              config
);

VOID
CAMProgramEntry88XX_V1(
    IN	HAL_PADAPTER		Adapter,
    IN  u1Byte              index,
    IN  pu1Byte             macad,
    IN  pu1Byte             key128,
    IN  u2Byte              config
);


u1Byte 
FindFreeSmhCam88XX(
    IN struct rtl8192cd_priv *priv,
    IN struct stat_info *pstat
);

u1Byte 
GetSmhCam88XX(
    IN struct rtl8192cd_priv *priv,
    IN struct stat_info *pstat
);

VOID
SetHwReg88XX(
    IN	HAL_PADAPTER		Adapter,
    IN	u1Byte				variable,
    IN	pu1Byte				val
);


VOID
GetHwReg88XX(
    IN      HAL_PADAPTER    Adapter,
    IN      u1Byte          variable,
    OUT     pu1Byte         val
);

enum rt_status
GetMACIDQueueInTXPKTBUF88XX(
    IN      HAL_PADAPTER          Adapter,
    OUT     pu1Byte               MACIDList
);

enum rt_status
GetMACIDQueueInTXPKTBUF88XX_V1(
    IN      HAL_PADAPTER          Adapter,
    OUT     pu1Byte               MACIDList
);
#if IS_RTL88XX_MAC_V4
enum rt_status
GetMACIDQueueInTXPKTBUF88XX_V2(
    IN      HAL_PADAPTER          Adapter,
    OUT     pu1Byte               MACIDList
);
#endif //#if IS_RTL88XX_MAC_V4

enum rt_status
SetMACIDSleep88XX(
    IN  HAL_PADAPTER Adapter,
    IN  BOOLEAN      bSleep,   
    IN  u4Byte       aid
);

unsigned char
ReadMACIDSleep88XX(
	IN  HAL_PADAPTER Adapter,
	IN  u4Byte       aid
);

#if (IS_RTL8881A_SERIES || IS_RTL8192E_SERIES || IS_RTL8192F_SERIES)
enum rt_status
InitLLT_Table88XX(
    IN  HAL_PADAPTER    Adapter
);
#endif //#if (IS_RTL8881A_SERIES || IS_RTL8192E_SERIES)

#if (IS_RTL8814A_SERIES || IS_RTL8197F_SERIES || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES|| IS_RTL8198F_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES || IS_RTL8197G_SERIES)
enum rt_status
InitLLT_Table88XX_V1(
    IN  HAL_PADAPTER    Adapter
);
#endif //#if (IS_RTL8814A_SERIES || IS_RTL8197F_SERIES || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES || IS_RTL8198F_SERIES || IS_RTL8197G_SERIES)

enum rt_status
InitPON88XX(
    IN  HAL_PADAPTER Adapter
);

enum rt_status
InitMAC88XX(
    IN  HAL_PADAPTER Adapter
);

enum rt_status
InitMAC88XX_V1(
	IN  HAL_PADAPTER Adapter
);

enum rt_status
InitMACProtocol88XX(
	IN  HAL_PADAPTER Adapter
);
enum rt_status
InitMACScheduler88XX(
	IN  HAL_PADAPTER Adapter
);
enum rt_status
InitMACWMAC88XX(
	IN  HAL_PADAPTER Adapter
);

enum rt_status
InitMACSysyemCfg88XX(
	IN  HAL_PADAPTER Adapter
);

enum rt_status
InitMACTRX88XX(
	IN  HAL_PADAPTER Adapter
);

enum rt_status
InitMACFunction88XX(
	IN  HAL_PADAPTER Adapter
);

VOID
InitIMR88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  RT_OP_MODE      op_mode
);

VOID
InitVAPIMR88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          VapSeq
);


enum rt_status      
InitHCIDMAMem88XX(
    IN      HAL_PADAPTER    Adapter
);  

enum rt_status
InitHCIDMAReg88XX(
    IN      HAL_PADAPTER    Adapter
);  
#if IS_EXIST_RTL8814BE
enum rt_status
InitHCIDMAReg88XX_V1(
    IN      HAL_PADAPTER    Adapter
);  
#endif
VOID
StopHCIDMASW88XX(
    IN  HAL_PADAPTER Adapter
);

VOID
StopHCIDMAHW88XX(
    IN  HAL_PADAPTER Adapter
);

#if CFG_HAL_SUPPORT_MBSSID
VOID
InitMBSSID88XX(
    IN  HAL_PADAPTER Adapter
);

VOID
InitMBSSID88XX_V1(
    IN  HAL_PADAPTER Adapter
);
VOID
InitMBSSID88XX_V2(
    IN  HAL_PADAPTER Adapter
);

VOID
InitMBIDCAM88XX(
    IN  HAL_PADAPTER Adapter
);

VOID
StopMBSSID88XX(
    IN  HAL_PADAPTER Adapter
);

VOID
StopMBSSID88XX_V1(
    IN  HAL_PADAPTER Adapter
);
VOID
StopMBSSID88XX_V2(
    IN  HAL_PADAPTER Adapter
);

VOID
SetDTIM(
    IN  HAL_PADAPTER Adapter
);
VOID
SetDTIM_V1(
    IN  HAL_PADAPTER Adapter
);
VOID
SetDTIM_V2(
    IN  HAL_PADAPTER Adapter
);


#endif  //CFG_HAL_SUPPORT_MBSSID

#ifdef AP_SWPS_OFFLOAD
VOID
UpdateAPSWPSReportSize88XX(
    IN  HAL_PADAPTER Adapter,
    IN  unsigned int max_macid
);

VOID
UpdateAPSWPSReportTH88XX(
    IN  HAL_PADAPTER Adapter,
    IN  u2Byte       PKT_TH,
    IN  u2Byte       PSTIME_TH,
    IN  u2Byte       TIME_TH
);

VOID
InitAPSWPS88XX(
    IN  HAL_PADAPTER Adapter
);
#endif
#ifdef ADDRESS_CAM
VOID
InitADDRCAM88XX(
    IN  HAL_PADAPTER Adapter
);
#endif

enum rt_status
SetMBIDCAM88XX(
    IN  HAL_PADAPTER Adapter,
    IN  u1Byte       MBID_Addr,    
    IN  u1Byte       IsRoot
);

enum rt_status
SetADDRCAMEntry88XX(
    IN  HAL_PADAPTER Adapter,
    IN  pu1Byte       Client_Addr
);

enum rt_status
StopMBIDCAM88XX(
    IN  HAL_PADAPTER Adapter,
    IN  u1Byte       MBID_Addr
);

enum rt_status
DelADDRCAMEntry88XX(
    IN  HAL_PADAPTER Adapter,
    IN  struct stat_info *pstat
);

enum rt_status
ResetHWForSurprise88XX(
    IN  HAL_PADAPTER Adapter
);

#if CFG_HAL_MULTI_MAC_CLONE
VOID
McloneSetMBSSID88XX(
    IN  HAL_PADAPTER Adapter,
    IN	pu1Byte 	 macAddr,
    IN	int          entIdx
);
#if IS_RTL88XX_MAC_V4
VOID
McloneSetMBSSID88XX_V1(
    IN  HAL_PADAPTER Adapter,
    IN	pu1Byte 	 macAddr,
    IN	int          entIdx
);
#endif
VOID
McloneStopMBSSID88XX(
    IN  HAL_PADAPTER Adapter,
    IN	int          entIdx
);
VOID
McloneStopMBSSID88XX_V1(
    IN  HAL_PADAPTER Adapter,
    IN	int          entIdx
);

#endif // #if CFG_HAL_MULTI_MAC_CLONE

enum rt_status
StopHW88XX(
    IN  HAL_PADAPTER Adapter
);

enum rt_status
StopSW88XX(
    IN  HAL_PADAPTER Adapter
);

VOID
DisableVXDAP88XX(
    IN  HAL_PADAPTER Adapter
);

VOID
Timer1Sec88XX(
    IN  HAL_PADAPTER Adapter
);


enum rt_status
PktBufAccessCtrl(
    IN	HAL_PADAPTER        Adapter,
    IN u1Byte mode,
    IN u1Byte rw,
    IN u2Byte offset, //Addr >> 3
    IN u1Byte wbit
);


enum rt_status 
GetTxRPTBuf88XX(
    IN	HAL_PADAPTER        Adapter,
    IN	u4Byte              macID,
    IN  u1Byte              variable,   
    IN 	u1Byte				byteoffset,
    OUT pu1Byte             val
);

enum rt_status 
SetTxRPTBuf88XX(
    IN	HAL_PADAPTER        Adapter,
    IN	u4Byte              macID,
    IN  u1Byte              variable,
    IN  pu1Byte             val    
);

u4Byte
CheckHang88XX(
    IN	HAL_PADAPTER        Adapter
);

VOID
SetCRC5ToRPTBuffer88XX(
    IN	HAL_PADAPTER        Adapter,
    IN	u1Byte              val,
    IN	u4Byte              macID,
    IN  u1Byte              bValid
);

VOID
ClearHWTXShortcutBufHandler88XX(
    IN  HAL_PADAPTER        Adapter,
    IN  u4Byte              macID
);

VOID
GetHwSequenceHandler88XX(
    IN	HAL_PADAPTER        Adapter,
    IN	u4Byte              macID,
    IN  u1Byte              tid, 
    OUT pu4Byte             val  
);
         
VOID
SetCRC5ValidBit88XX(
    IN	HAL_PADAPTER        Adapter,
    IN	u1Byte              group,
    IN  u1Byte              bValid
    
);

VOID
SetCRC5EndBit88XX(
    IN	HAL_PADAPTER        Adapter,
    IN	u1Byte              group,
    IN  u1Byte              bEnd    
);

VOID
InitMACIDSearch88XX(
    IN	HAL_PADAPTER        Adapter    
);


enum rt_status
CheckHWMACIDResult88XX(
    IN	HAL_PADAPTER        Adapter,    
    IN  u4Byte              MacID,
    OUT pu1Byte             result
);


enum rt_status 
InitDDMA88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte	source,
    IN  u4Byte	dest,
    IN  u4Byte 	length
);

#endif  //WLAN_HAL_INTERNAL_USED

#ifndef REG_ARFR1_V1
#define REG_ARFR1_V1 REG_ARFR1
#endif
#ifndef REG_RXFLTMAP1
#define REG_RXFLTMAP1 REG_RXFLTER1
#endif
#ifndef REG_RXFLTMAP
#define REG_RXFLTMAP REG_RXFLTER2
#endif
#ifndef REG_PS_TIMER
#define REG_PS_TIMER REG_PS_TIMER0
#endif
#ifndef REG_TSFTR
#define REG_TSFTR REG_TSFTR0_L
#endif
#ifndef REG_MGQ_BDNY
#define REG_MGQ_BDNY 0x0425
#endif
#ifndef REG_TRXFF_BNDY
#define REG_TRXFF_BNDY REG_RXFF_BNDY_V1
#endif
#ifndef REG_MBIDCAMCFG_2
#define REG_MBIDCAMCFG_2 REG_MBIDCAM_CFG
#endif
#ifndef REG_RX_PKT_LIMIT
#define REG_RX_PKT_LIMIT REG_RXPKT_LIMIT
#endif
#ifndef BIT_DRVINFO_SZ
#define BIT_DRVINFO_SZ BIT_DRVINFO_SZ_V1
#endif
#ifndef REG_ASSOCIATED_BFMER1_INFO
#define REG_ASSOCIATED_BFMER1_INFO REG_SOUNDING_CFG2
#endif
#ifndef REG_TX_CSI_RPT_PARAM_BW20
#define REG_TX_CSI_RPT_PARAM_BW20 REG_SOUNDING_CFG0
#endif
#ifndef REG_TX_CSI_RPT_PARAM_BW40
#define REG_TX_CSI_RPT_PARAM_BW40 REG_ANTCD_INFO
#endif
#ifndef REG_ACQ_STOP
#define REG_ACQ_STOP 0x045C
#endif
#ifndef REG_Q0_INFO
#define REG_Q0_INFO REG_QUEUELIST_INFO0
#endif
#ifndef REG_Q4_INFO
#define REG_Q4_INFO 0x0468
#endif

#endif  //__HAL88XX_DEF_H__
