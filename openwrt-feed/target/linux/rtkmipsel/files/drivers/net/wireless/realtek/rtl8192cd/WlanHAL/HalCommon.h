#ifndef __HALCOMMON_H__
#define __HALCOMMON_H__

/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	HalCommon.h
	
Abstract:
	Defined HAL Common
	    
Major Change History:
	When       Who               What
	---------- ---------------   -------------------------------
	2012-05-18  Lun-Wu            Create.	
--*/

// Total 32bytes, we need control in 8bytes

// TODO: temp setting, we need to move to matching file
// Some setting can be replaced after normal MAC reg.h are released

// Loopback mode.
#define		LBMODE_NORMAL			0x00
#define		LBMODE_MAC				0x0B
#define		LBMODE_MAC_DLY			0x03

/*
Network Type
00: No link
01: Link in ad hoc network
10: Link in infrastructure network
11: AP mode
Default: 00b.
*/
#define	MSR_NOLINK				0x00
#define	MSR_ADHOC				0x01
#define	MSR_INFRA				0x02
#define	MSR_AP					0x03

//----------------------------------------------------------------------------
//      (PBP) Packet Buffer Page Register	(Offset 0x104[7:4], 4 bits)  
//----------------------------------------------------------------------------
#define PBP_UNIT                128

//TXBD_IDX Common
#define BIT_SHIFT_QUEUE_HOST_IDX    0
#define BIT_SHIFT_QUEUE_HW_IDX      16
#define BIT_MASK_QUEUE_IDX          0x0FFF


VOID
HalGeneralDummy(
	IN	HAL_PADAPTER    Adapter
);


enum rt_status 
HAL_ReadTypeID(
	INPUT	HAL_PADAPTER	Adapter
);

VOID
ResetHALIndex(
    VOID
);

VOID
DecreaseHALIndex(
    VOID
);

enum rt_status
HalAssociateNic(
    HAL_PADAPTER        Adapter,
    BOOLEAN			IsDefaultAdapter    
);

enum rt_status
HalDisAssociateNic(
    HAL_PADAPTER        Adapter,
    BOOLEAN			    IsDefaultAdapter    
);

VOID 
SoftwareCRC32 (
    IN  pu1Byte     pBuf,
    IN  u2Byte      byteNum,
    OUT pu4Byte     pCRC32
);


u1Byte 
GetXorResultWithCRC (
    IN  u1Byte      a,
    IN  u1Byte      b
);

u1Byte
CRC5 (
    IN pu1Byte      dwInput,
    IN u1Byte       len
);

VOID 
SoftwareCRC32_RXBuffGather (
    IN  pu1Byte     pPktBufAddr,
    IN  pu2Byte     pPktBufLen,  
    IN  u2Byte      pktNum,
    OUT pu4Byte     pCRC32
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

VOID
SetCRC5ToRPTBuffer88XX(
    IN	HAL_PADAPTER        Adapter,
    IN	u1Byte              val,
    IN	u4Byte              macID,
    IN  u1Byte              bValid
    
);

enum rt_status
ReleaseOnePacket88XX(
    IN  HAL_PADAPTER        Adapter,
    IN  u1Byte              macID
);

BOOLEAN
LoadFileToIORegTable(
    IN  pu1Byte     pRegFileStart,
    IN  u4Byte      RegFileLen,
    OUT pu1Byte     pTableStart,
    IN  u4Byte      TableEleNum
);

BOOLEAN
LoadFileToOneParaTable(
    IN  pu1Byte     pFileStart,
    IN  u4Byte      FileLen,
    OUT pu1Byte     pTableStart,
    IN  u4Byte      TableEleNum
);

#endif // __HALCOMMON_H__

