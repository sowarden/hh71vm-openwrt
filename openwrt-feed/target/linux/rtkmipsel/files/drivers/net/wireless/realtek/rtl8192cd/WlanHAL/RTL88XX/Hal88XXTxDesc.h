#ifndef __HAL88XX_TXDESC_H__
#define __HAL88XX_TXDESC_H__

typedef struct _TX_DESC_88XX_
{
    volatile    u4Byte     Dword0;
    volatile    u4Byte     Dword1;
    volatile    u4Byte     Dword2;
    volatile    u4Byte     Dword3;
    volatile    u4Byte     Dword4;
    volatile    u4Byte     Dword5;
    volatile    u4Byte     Dword6;
    volatile    u4Byte     Dword7;
    volatile    u4Byte     Dword8;
    volatile    u4Byte     Dword9;
    volatile    u4Byte     Dword10;
    volatile    u4Byte     Dword11;        
    volatile    u4Byte     Dword12;   
    volatile    u4Byte     Dword13;  
    volatile    u4Byte     Dword14;
    volatile    u4Byte     Dword15;
} TX_DESC_88XX, *PTX_DESC_88XX;

#ifdef SUPPORT_TXDESC_IE
#define SIZE_TXDESC_IE  8
#define SIZE_TXDESC_BODY    16
#endif

#define SIZE_TXDESC_88XX    40
#define SIZE_TXDESC_88XX_V1    48
#define SIZE_TXDESC_88XX_V2    64 //IE_body+IE0~5=16+8*6
#define SIZE_TXDESC_88XX_V2_BCN 56 //IE_body+IE0~4=16+8*5

#define SIZE_TXDESC_88XX_MAX   64 

typedef enum _SET_TXBD_SOURCE_88XX_
{
    SET_TXBD_SOURCE_NORMAL,
    SET_TXBD_SOURCE_SHORTCUT,
    SET_TXBD_SOURCE_OFLD_SHORTCUT,
}SET_TXBD_SOURCE_88XX, *PSET_TXBD_SOURCE_88XX;

typedef enum _HCI_TX_DMA_QUEUE_88XX_
{
    //MGT
    HCI_TX_DMA_QUEUE_MGT = 0,

    //QoS
    HCI_TX_DMA_QUEUE_BK,
    HCI_TX_DMA_QUEUE_BE,
    HCI_TX_DMA_QUEUE_VI,
    HCI_TX_DMA_QUEUE_VO,

    //HI
    HCI_TX_DMA_QUEUE_HI0,
    HCI_TX_DMA_QUEUE_HI1,
    HCI_TX_DMA_QUEUE_HI2,
    HCI_TX_DMA_QUEUE_HI3,
    HCI_TX_DMA_QUEUE_HI4,
    HCI_TX_DMA_QUEUE_HI5,
    HCI_TX_DMA_QUEUE_HI6,
    HCI_TX_DMA_QUEUE_HI7,
    HCI_TX_DMA_QUEUE_CMD,            
    // Beacon
    HCI_TX_DMA_QUEUE_BCN,
    HCI_TX_DMA_QUEUE_MAX_NUM        //15
} HCI_TX_DMA_QUEUE_88XX, *PHCI_TX_DMA_QUEUE_88XX;

typedef enum _HCI_TX_DMA_QUEUE_88XX_V1_
{
    //MGT
    HCI_TX_DMA_QUEUE_MGT_V1 = 0,

    //QoS
    HCI_TX_DMA_QUEUE_BK_V1,
    HCI_TX_DMA_QUEUE_BE_V1,
    HCI_TX_DMA_QUEUE_VI_V1,
    HCI_TX_DMA_QUEUE_VO_V1,

    //HI
    HCI_TX_DMA_QUEUE_HI0_V1,
    HCI_TX_DMA_QUEUE_HI1_V1,
    HCI_TX_DMA_QUEUE_HI2_V1,
    HCI_TX_DMA_QUEUE_HI3_V1,
    HCI_TX_DMA_QUEUE_HI4_V1,
    HCI_TX_DMA_QUEUE_HI5_V1,
    HCI_TX_DMA_QUEUE_HI6_V1,
    HCI_TX_DMA_QUEUE_HI7_V1,
    HCI_TX_DMA_QUEUE_HI8,
    HCI_TX_DMA_QUEUE_HI9,
    HCI_TX_DMA_QUEUE_HI10,
    HCI_TX_DMA_QUEUE_HI11,
    HCI_TX_DMA_QUEUE_HI12,
    HCI_TX_DMA_QUEUE_HI13,
    HCI_TX_DMA_QUEUE_HI14,
    HCI_TX_DMA_QUEUE_HI15,
    HCI_TX_DMA_QUEUE_CMD_V1,            
    // Beacon
    HCI_TX_DMA_QUEUE_BCN_V1,
    HCI_TX_DMA_QUEUE_MAX_NUM_V1        //22
} HCI_TX_DMA_QUEUE_88XX_V1, *PHCI_TX_DMA_QUEUE_88XX_V1;

typedef enum _HCI_TX_DMA_QUEUE_88XX_V2_
{
    //MGT
    HCI_TX_DMA_QUEUE_MGT_V2 = 0, //CH14

    //DMA channel
    HCI_TX_DMA_QUEUE_BK_V2, //BK
    HCI_TX_DMA_QUEUE_BE_V2, //BE
    HCI_TX_DMA_QUEUE_VI_V2, //VI
    HCI_TX_DMA_QUEUE_VO_V2, //VO
    HCI_TX_DMA_QUEUE_ACH4,
    HCI_TX_DMA_QUEUE_ACH5,
    HCI_TX_DMA_QUEUE_ACH6,
    HCI_TX_DMA_QUEUE_ACH7,
    HCI_TX_DMA_QUEUE_ACH8,
    HCI_TX_DMA_QUEUE_ACH9,
    HCI_TX_DMA_QUEUE_ACH10,
    HCI_TX_DMA_QUEUE_ACH11,
    HCI_TX_DMA_QUEUE_ACH12, //specialQ0
    HCI_TX_DMA_QUEUE_ACH13, //specialQ1

    //HI
    HCI_TX_DMA_QUEUE_HI0_V2,
    HCI_TX_DMA_QUEUE_HI1_V2,
    HCI_TX_DMA_QUEUE_HI2_V2,
    HCI_TX_DMA_QUEUE_HI3_V2,
    HCI_TX_DMA_QUEUE_HI4_V2,
    HCI_TX_DMA_QUEUE_HI5_V2,
    HCI_TX_DMA_QUEUE_HI6_V2,
    HCI_TX_DMA_QUEUE_HI7_V2,
    HCI_TX_DMA_QUEUE_HI8_V1,
    HCI_TX_DMA_QUEUE_HI9_V1,
    HCI_TX_DMA_QUEUE_HI10_V1,
    HCI_TX_DMA_QUEUE_HI11_V1,
    HCI_TX_DMA_QUEUE_HI12_V1,
    HCI_TX_DMA_QUEUE_HI13_V1,
    HCI_TX_DMA_QUEUE_HI14_V1,
    HCI_TX_DMA_QUEUE_HI15_V1,
    HCI_TX_DMA_QUEUE_HI16_V1,
    HCI_TX_DMA_QUEUE_HI17_V1,
    HCI_TX_DMA_QUEUE_HI18_V1,
    HCI_TX_DMA_QUEUE_HI19_V1,
    HCI_TX_DMA_QUEUE_FWCMD,//new //CH16
    HCI_TX_DMA_QUEUE_CMD_V2,//H2C //CH20
    // Beacon
    HCI_TX_DMA_QUEUE_BCN_V2, //CH19
    HCI_TX_DMA_QUEUE_MAX_NUM_V2        //33
} HCI_TX_DMA_QUEUE_88XX_V2, *PHCI_TX_DMA_QUEUE_88XX_V2;
typedef enum _HCI_TX_DMA_QUEUE_88XX_PEOFLD_
{
    //MGT
    HCI_TX_DMA_QUEUE_MGT_PEOFLD = 0, //CH14

    //HI
    HCI_TX_DMA_QUEUE_HI0_PEOFLD,
    HCI_TX_DMA_QUEUE_HI1_PEOFLD,
    HCI_TX_DMA_QUEUE_HI2_PEOFLD,
    HCI_TX_DMA_QUEUE_HI3_PEOFLD,
    HCI_TX_DMA_QUEUE_HI4_PEOFLD,
    HCI_TX_DMA_QUEUE_HI5_PEOFLD,
    HCI_TX_DMA_QUEUE_HI6_PEOFLD,
    HCI_TX_DMA_QUEUE_HI7_PEOFLD,
    HCI_TX_DMA_QUEUE_HI8_PEOFLD,
    HCI_TX_DMA_QUEUE_HI9_PEOFLD,
    HCI_TX_DMA_QUEUE_HI10_PEOFLD,
    HCI_TX_DMA_QUEUE_HI11_PEOFLD,
    HCI_TX_DMA_QUEUE_HI12_PEOFLD,
    HCI_TX_DMA_QUEUE_HI13_PEOFLD,
    HCI_TX_DMA_QUEUE_HI14_PEOFLD,
    HCI_TX_DMA_QUEUE_HI15_PEOFLD,
    HCI_TX_DMA_QUEUE_FWCMD_PEOFLD,//new //CH16
    HCI_TX_DMA_QUEUE_CMD_PEOFLD,//H2C //CH20
    // Beacon
    HCI_TX_DMA_QUEUE_BCN_PEOFLD, //CH19
    HCI_TX_DMA_QUEUE_MAX_NUM_PEOFLD
} HCI_TX_DMA_QUEUE_88XX_PEOFLD, *PHCI_TX_DMA_QUEUE_88XX_PEOFLD;


#if 0
// TODO: endian....
#ifdef _BIG_ENDIAN_
typedef struct _TXBD_ELEMENT_DW0_
{
    u4Byte         Rsvd_31         :1;
    u4Byte         PsbLen          :15;
    u4Byte         Rsvd_14_15      :2;
    u4Byte         Len             :14;
} TXBD_ELEMENT_DW0,*PTXBD_ELEMENT_DW0;

typedef struct _TXBD_ELEMENT_DW_
{
    u4Byte         AmpduEn         :1;
    u4Byte         Rsvd16To30      :15;
    u4Byte         Len             :16;
} TXBD_ELEMENT_DW,*PTXBD_ELEMENT_DW;
#else // _LITTLE_ENDIAN_
typedef struct _TXBD_ELEMENT_DW0_
{
    u4Byte         Len             :14;
    u4Byte         Rsvd_14_15      :2;
    u4Byte         PsbLen          :15;
    u4Byte         Rsvd_31         :1;
} TXBD_ELEMENT_DW0,*PTXBD_ELEMENT_DW0;

typedef struct _TXBD_ELEMENT_DW_
{
    u4Byte         Len             :16;
    u4Byte         Rsvd16To30      :15;
    u4Byte         AmpduEn         :1;
} TXBD_ELEMENT_DW,*PTXBD_ELEMENT_DW;
#endif
#endif

typedef struct _TXBD_ELEMENT_
{
    u4Byte              Dword0;
    u4Byte              Dword1;
#if TXBD_SEG_32_64_SEL
    u4Byte              Dword2;
    u4Byte              Dword3;
#endif  //TXBD_SEG_32_64_SEL
} TXBD_ELEMENT,*PTXBD_ELEMENT;

typedef struct _TX_BUFFER_DESCRIPTOR_
{
    TXBD_ELEMENT	TXBD_ELE[TXBD_ELE_NUM];
} TX_BUFFER_DESCRIPTOR, *PTX_BUFFER_DESCRIPTOR;


typedef struct _HCI_TX_DMA_QUEUE_STRUCT_88XX_
{
    //TXBD       
    PTX_BUFFER_DESCRIPTOR   pTXBD_head;

    //TXBD Queue management    
    u2Byte                  hw_idx;
    u2Byte	                host_idx;

    //Two Method:
    // 1.) TXDESC Only
    // 2.) TXDESC + Payload
    PVOID                   ptx_desc_head;
    u2Byte                  total_txbd_num;
    u2Byte                  avail_txbd_num;

    // RWPtr IDX Reg
    u4Byte                  reg_rwptr_idx;
} HCI_TX_DMA_QUEUE_STRUCT_88XX, *PHCI_TX_DMA_QUEUE_STRUCT_88XX;

typedef struct _HCI_TX_DMA_MANAGER_88XX_
{
    HCI_TX_DMA_QUEUE_STRUCT_88XX  tx_queue[HCI_TX_DMA_QUEUE_MAX_NUM_V2];
} HCI_TX_DMA_MANAGER_88XX, *PHCI_TX_DMA_MANAGER_88XX;

#if CFG_HAL_TX_AMSDU
typedef struct _TX_BUFFER_DESCRIPTOR_AMSDU_
{
    TXBD_ELEMENT	TXBD_ELE[MAX_NUM_OF_MSDU_IN_AMSDU];
} TX_BUFFER_DESCRIPTOR_AMSDU, *PTX_BUFFER_DESCRIPTOR_AMSDU;

typedef enum _HCI_TX_AMSDU_DMA_QUEUE_88XX_
{
    //QoS
    HCI_TX_AMSDU_DMA_QUEUE_BK,
    HCI_TX_AMSDU_DMA_QUEUE_BE,
    HCI_TX_AMSDU_DMA_QUEUE_VI,
    HCI_TX_AMSDU_DMA_QUEUE_VO,
    HCI_TX_AMSDU_DMA_QUEUE_MAX_NUM
} HCI_TX_AMSDU_DMA_QUEUE_88XX, *PHCI_TX_AMSDU_DMA_QUEUE_88XX;

typedef struct _HCI_TX_AMSDU_DMA_QUEUE_STRUCT_88XX_
{
    //AMSDU TXBD
    PTX_BUFFER_DESCRIPTOR_AMSDU     pTXBD_head_amsdu;

    //Current AMSDU TXBD element
    u1Byte                          cur_txbd_element;
} HCI_TX_AMSDU_DMA_QUEUE_STRUCT_88XX, *PHCI_TX_AMSDU_DMA_QUEUE_STRUCT_88XX;

typedef struct _HCI_TX_AMSDU_DMA_MANAGER_88XX_
{
    HCI_TX_AMSDU_DMA_QUEUE_STRUCT_88XX  tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_MAX_NUM];
} HCI_TX_AMSDU_DMA_MANAGER_88XX, *PHCI_TX_AMSDU_DMA_MANAGER_88XX;

#endif // CFG_HAL_TX_AMSDU


//typedef struct _TXBD_INFO_
//{
//    u4Byte      Length;
//    u4Byte      AddrLow;
//} TXBD_INFO_88XX, *PTXBD_INFO_88XX;

typedef struct _TX_DESC_DATA_88XX_
{
    // header
    pu1Byte         pHdr;
    u4Byte          hdrLen;    
    u4Byte          llcLen;

    // frame
    pu1Byte         pBuf;    
    u4Byte          frLen;    

    // encryption
    pu1Byte         pMic;
    pu1Byte         pIcv;

    // TXDESC Dword 1
    u4Byte          rateId;
    u1Byte          macId;
    u1Byte          tid;
    BOOLEAN         moreData;    
	u1Byte          enDescId;
    
    // TXDESC Dword 2
    BOOLEAN         aggEn;
    u1Byte          ampduDensity;
    BOOLEAN         frag;
    BOOLEAN         bk;
    u4Byte          p_aid;	
    BOOLEAN         g_id;
#if CFG_HAL_HW_AES_IV
    BOOLEAN         hwAESIv;
#endif
    u1Byte			cca_rts;


    // TXDESC Dword 3
    BOOLEAN         RTSEn;
    BOOLEAN         HWRTSEn;
    BOOLEAN         CTS2Self;
    BOOLEAN         useRate;
    BOOLEAN         disRTSFB;
    BOOLEAN         disDataFB;
    u1Byte          maxAggNum;
    BOOLEAN         navUseHdr;
    BOOLEAN         ndpa;

    // TXDESC Dword 4
    u1Byte          RTSRate;
    u1Byte          RTSRateFBLmt;
    u1Byte          dataRate;
    BOOLEAN         rtyLmtEn;
    u1Byte          dataRtyLmt;
    u1Byte          dataRateFBLmt;    
    u1Byte          BMCRtyLmt;

    // TXDESC Dword 5
    u1Byte          dataBW;
    u1Byte          dataSC;
    u1Byte          RTSSC;
    u1Byte          dataStbc;
    u1Byte          dataLdpc;
    u1Byte          dataShort;
    u1Byte          RTSShort;
    u1Byte          TXPowerOffset;
    u1Byte          TXAnt;
	
    // TXDESC Dword 6
#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))
    u1Byte          ant_sel;
    u1Byte          ant_sel_a;
    u1Byte          ant_sel_b;
    u1Byte          ant_sel_c;
#endif	//#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))
	
	
#if (CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC || CFG_HAL_HW_TX_SHORTCUT_HDR_CONV)
	// TXDESC Dword 8
	BOOLEAN		 	smhEn;
	BOOLEAN			stwEn;
	BOOLEAN			stwAntDis;
	BOOLEAN			stwRateDis;
	BOOLEAN			stwRbDis;
	BOOLEAN			stwPktReDis;
	BOOLEAN			macCp;
	BOOLEAN			txwifiCp;
    u1Byte          shcut_cam;
#endif
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV && IS_RTL88XX_MAC_V4
    u1Byte          smhCamIdx;
#endif

#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV && CFG_HAL_HW_TX_AMSDU
    BOOLEAN         HW_AMSDU;
    u1Byte          HW_AMSDU_size;
#endif

#if 1 /*eric-8822*/
	u1Byte			SND_target;
	u1Byte			SND_pkt_sel;
	u1Byte			is_GID;
#endif

    // encrypt
    u4Byte          iv;
    u4Byte          icv;
    u4Byte          mic;
    u4Byte          secType;
    BOOLEAN         swCrypt;

#if CFG_HAL_TX_AMSDU
    // AMSDU
    u1Byte          aggreEn;
    u4Byte          amsduLen;
#endif

#if defined(AP_SWPS_OFFLOAD)
    u2Byte          SWPS_sequence;
    u1Byte          DropID;
#endif

#if CFG_HAL_SUPPORT_TXDESC_IE
    u1Byte          dma_channel;
#endif

#ifdef CONFIG_RTL_OFFLOAD_DRIVER
	PVOID	ph2d_txdma_info;
#endif
} TX_DESC_DATA_88XX, *PTX_DESC_DATA_88XX;

typedef struct _H2C_PAYLOAD_88XX_
{
    u4Byte          offset0;
    u4Byte          offset4;
    u4Byte          offset8;
    u4Byte          offset12;
    u4Byte          offset16;
    u4Byte          offset20;
    u4Byte          offset24;
    u4Byte          offset28;    
} H2C_PAYLOAD_88XX, *PH2C_PAYLOAD_88XX;

#if CFG_HAL_WLAN_SUPPORT_FW_CMD
//temp same as H2C
typedef struct _FWCMD_PAYLOAD_88XX_
{
    u4Byte          offset0;
    u4Byte          offset4;
    u4Byte          offset8;
    u4Byte          offset12;
    u4Byte          offset16;
    u4Byte          offset20;
    u4Byte          offset24;
    u4Byte          offset28;    
} FWCMD_PAYLOAD_88XX, *PFWCMD_PAYLOAD_88XX;

#endif
VOID
TxPolling88XX(
    IN	HAL_PADAPTER        Adapter,
    IN	u1Byte              QueueIndex
);

VOID
SigninBeaconTXBD88XX
(
    IN	HAL_PADAPTER        Adapter,
    IN  pu4Byte             beaconbuf,
    IN  u2Byte              frlen
);

VOID
SetBeaconDownload88XX (
    IN	HAL_PADAPTER        Adapter,
    IN  u4Byte              Value
);

u2Byte
GetTxQueueHWIdx88XX
(
    IN	HAL_PADAPTER        Adapter,
    IN  u4Byte              q_num       //enum _TX_QUEUE_
);

BOOLEAN
FillTxHwCtrl88XX(
    IN      HAL_PADAPTER    Adapter,
    IN      u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN      PVOID           pDescData
);

static enum rt_status
TxPktFinalIO88XX(
    IN      HAL_PADAPTER                    Adapter,
    IN      PTX_BUFFER_DESCRIPTOR           cur_txbd,
    IN      u4Byte                          CtrlFlag,   //enum _TxPktFinalIO88XX_FLAG_{
    IN      u4Byte                          DwordSettingValue
);

HAL_IMEM
enum rt_status
SyncSWTXBDHostIdxToHW88XX (
    IN      HAL_PADAPTER    Adapter,
    IN      u4Byte          queueIndex  //HCI_TX_DMA_QUEUE_88XX
);

#ifdef AP_SWPS_OFFLOAD

#if IS_RTL8192F_SERIES
VOID
ReprepareFillTxDescSetTxBD_92f(
    IN      HAL_PADAPTER    Adapter,
    IN      struct stat_info *pstat,
    IN      u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN      struct reprepare_info* pkt_info
);
#endif

#if IS_RTL88XX_MAC_V3
VOID
ReprepareFillTxDescSetTxBD(
    IN      HAL_PADAPTER    Adapter,
    IN      struct stat_info *pstat,
    IN      u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN      struct reprepare_info* pkt_info
);
#endif
#if IS_RTL88XX_MAC_V4
VOID
ReprepareFillTxDescSetTxBD_V1(
    IN      HAL_PADAPTER    Adapter,
    IN      struct stat_info *pstat,
    IN      u4Byte          queueIndex,  
    IN      struct reprepare_info* pkt_info
);
#endif
#endif

HAL_IMEM
BOOLEAN
QueryTxConditionMatch88XX(
    IN	    HAL_PADAPTER            Adapter
);

enum rt_status
PrepareTXBD88XX(
    IN      HAL_PADAPTER    Adapter
);

enum rt_status
PrepareTXBD88XX_V1(
    IN      HAL_PADAPTER    Adapter
);

VOID
FillBeaconDesc88XX
(
    IN	HAL_PADAPTER        Adapter,
    IN  PVOID               _pdesc,
    IN  PVOID               data_content,
    IN  u2Byte              txLength,
    IN  BOOLEAN             bForceUpdate
);

VOID
GetBeaconTXBDTXDESC88XX(
    IN	HAL_PADAPTER                Adapter,
    OUT PTX_BUFFER_DESCRIPTOR       *pTXBD,
    OUT PTX_DESC_88XX               *ptx_desc
);

VOID
FillTxDesc88XX (
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN  PVOID           pDescData
);

HAL_IMEM						
VOID
FillShortCutTxDesc88XX(
    IN      HAL_PADAPTER    Adapter,
    IN      u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN      PVOID           pDescData,
    IN      PVOID           pTxDesc
);

HAL_IMEM
VOID
FillHwShortCutTxDesc88XX (
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN  PVOID           pDescData
);

VOID
FillTxDesc88XX_V1 (
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN  PVOID           pDescData
);

VOID
FillTxDesc88XX_V2 (
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN  PVOID           pDescData
);

HAL_IMEM						
VOID
FillShortCutTxDesc88XX_V1(
    IN      HAL_PADAPTER    Adapter,
    IN      u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN      PVOID           pDescData,
    IN      PVOID           pTxDesc
);

VOID
FillShortCutTxDesc88XX_V2(
    IN      HAL_PADAPTER    Adapter,
    IN      u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN      PVOID           pDescData,
    IN      PVOID           pTxDesc
);
HAL_IMEM
VOID
FillHwShortCutTxDesc88XX_V1 (
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN  PVOID           pDescData
);

#if CFG_HAL_SUPPORT_TXDESC_IE
VOID
FillHwShortCutIE88XX (
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          queueIndex,  
    IN  PVOID           pDescData,
    IN  PVOID           pTxDesc
);
#endif

VOID
FillBeaconDesc88XX_V1
(
    IN	HAL_PADAPTER        Adapter,
    IN  PVOID               _pdesc,
    IN  PVOID               data_content,
    IN  u2Byte              txLength,
    IN  BOOLEAN             bForceUpdate
);

VOID
FillBeaconDesc88XX_V2
(
    IN	HAL_PADAPTER        Adapter,
    IN  PVOID               _pdesc,
    IN  PVOID               data_content,
    IN  u2Byte              txLength,
    IN  BOOLEAN             bForceUpdate
);
VOID
SetTxDescQSel88XX_V2
(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN  PTX_DESC_88XX   ptx_desc,
    IN  u1Byte          drvTID
);

VOID
SetTxDescQSel88XX_V3
(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN  PTX_DESC_88XX   ptx_desc,
    IN  u1Byte          drvTID
);
#if CFG_HAL_TX_SHORTCUT
#if 0
PVOID
GetShortCutTxDesc88XX(
    IN      HAL_PADAPTER    Adapter
);

VOID
ReleaseShortCutTxDesc88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  PVOID           pTxDesc
);
#endif

VOID
UpdateSWTXBDHostIdx88XX (
    IN  HAL_PADAPTER                    Adapter,
    IN  PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q
);
HAL_IMEM
VOID
SetShortCutTxBuffSize88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  PVOID           pTxDesc,
    IN  u2Byte          txPktSize
);

HAL_IMEM
u2Byte
GetShortCutTxBuffSize88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  PVOID           pTxDesc
);

HAL_IMEM
PVOID
CopyShortCutTxDesc88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX    
    IN  PVOID           pTxDesc,
    IN  u4Byte          direction    
);

HAL_IMEM
BOOLEAN
FillShortCutTxHwCtrl88XX(
    IN      HAL_PADAPTER    Adapter,
    IN      u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN      PVOID           pDescData,
    IN      PVOID           pTxDesc,
    IN      u4Byte          direction,
    IN      BOOLEAN         useHW  
);

HAL_IMEM
u2Byte
GetShortCutTxBuffSize88XX_V1(
    IN  HAL_PADAPTER    Adapter,
    IN  PVOID           pTxDesc
);

HAL_IMEM
VOID
SetShortCutTxBuffSize88XX_V1(
    IN  HAL_PADAPTER    Adapter,
    IN  PVOID           pTxDesc,
    IN  u2Byte          txBuffSize
);



#endif // CFG_HAL_TX_SHORTCUT

void DumpTxBDesc88XX(
    IN      HAL_PADAPTER    Adapter,
#ifdef CONFIG_RTL_PROC_NEW
    IN      struct seq_file *s,
#endif
    IN      u4Byte          q_num 
);

#endif  //#ifndef __HAL88XX_TXDESC_H__


