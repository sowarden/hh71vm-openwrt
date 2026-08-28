#ifndef _8192CD_CORE_HW_H_
#define _8192CD_CORE_HW_H_

#ifdef __ECOS
#include <pkgconf/system.h>
#endif

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/skbuff.h>
#include <asm/io.h>
#endif

#ifdef __DRAYTEK_OS__
#include <draytek/skbuff.h>
#endif

#include "./8192c_reg.h"
#if defined(CONFIG_RTL_92D_SUPPORT) || defined(CONFIG_RTL_88E_SUPPORT)
#include "./8192d_reg.h"
#endif
#if defined(CONFIG_RTL_88E_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)	
#include "./8188e_reg.h"
#endif

#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL_8192EE) || defined(CONFIG_WLAN_HAL_8192FE) ||defined(CONFIG_RTL_8723B_SUPPORT)
#include "./8192e_reg.h"
#endif
#include "./8192cd_cfg.h"

#if defined(CONFIG_RTL_8723B_SUPPORT)
#include "./8723b_reg.h"
#endif

#if defined(CONFIG_WLAN_HAL_8822BE)
#include "./8822b_reg.h"
#endif

#if defined(CONFIG_WLAN_HAL_8821CE)
#include "./8821c_reg.h"
#endif

#if 1
#include "./8812_reg.h"
#endif

#if defined(CONFIG_WLAN_HAL)
#include "./WlanHAL/HalMac88XX/halmac_reg2.h"
#include "./WlanHAL/HalMac88XX/halmac_bit2.h"
#endif

#if !defined(__KERNEL__) && !defined(__ECOS)
#include "./sys-support.h"
#endif
#ifdef __ECOS
#include <cyg/io/eth/rltk/819x/wrapper/sys_support.h>
#include <cyg/io/eth/rltk/819x/wrapper/timer.h>
#endif

#include "./wifi.h"
#include "./8192cd_phyreg.h"

#include "cmn_info_file/rtw_sta_info.h"



#define HT_RATE_ID			0x80					//must >= 0x80
#define VHT_RATE_ID			0xA0

//
// For 8651C H/W MIC engine
//
#if !defined(_ASICREGS_H) || defined(__OSK__)
#define SYSTEM_BASE	(0xB8000000)
/* Generic DMA */
#define GDMA_BASE   (SYSTEM_BASE+0xA000)	/* 0xB800A000 */
#define GDMACNR		(GDMA_BASE+0x00)	/* Generic DMA Control Register */
#define GDMAIMR		(GDMA_BASE+0x04)	/* Generic DMA Interrupt Mask Register */
#define GDMAISR		(GDMA_BASE+0x08)	/* Generic DMA Interrupt Status Register */
#define GDMAICVL	(GDMA_BASE+0x0C)	/* Generic DMA Initial Checksum Value (Left Part) Register */
#define GDMAICVR	(GDMA_BASE+0x10)	/* Generic DMA Initial Checksum Value (Right Part) Register */
#define GDMASBP0	(GDMA_BASE+0x20)	/* Generic DMA Source Block Pointer 0 Register */
#define GDMASBL0	(GDMA_BASE+0x24)	/* Generic DMA Source Block Length 0 Register */
#define GDMASBP1	(GDMA_BASE+0x28)	/* Generic DMA Source Block Pointer 1 Register */
#define GDMASBL1	(GDMA_BASE+0x2C)	/* Generic DMA Source Block Length 1 Register */
#define GDMASBP2	(GDMA_BASE+0x30)	/* Generic DMA Source Block Pointer 2 Register */
#define GDMASBL2	(GDMA_BASE+0x34)	/* Generic DMA Source Block Length 2 Register */
#define GDMASBP3	(GDMA_BASE+0x38)	/* Generic DMA Source Block Pointer 3 Register */
#define GDMASBL3	(GDMA_BASE+0x3C)	/* Generic DMA Source Block Length 3 Register */
#define GDMASBP4	(GDMA_BASE+0x40)	/* Generic DMA Source Block Pointer 4 Register */
#define GDMASBL4	(GDMA_BASE+0x44)	/* Generic DMA Source Block Length 4 Register */
#define GDMASBP5	(GDMA_BASE+0x48)	/* Generic DMA Source Block Pointer 5 Register */
#define GDMASBL5	(GDMA_BASE+0x4C)	/* Generic DMA Source Block Length 5 Register */
#define GDMASBP6	(GDMA_BASE+0x50)	/* Generic DMA Source Block Pointer 6 Register */
#define GDMASBL6	(GDMA_BASE+0x54)	/* Generic DMA Source Block Length 6 Register */
#define GDMASBP7	(GDMA_BASE+0x58)	/* Generic DMA Source Block Pointer 7 Register */
#define GDMASBL7	(GDMA_BASE+0x5C)	/* Generic DMA Source Block Length 7 Register */
#define GDMADBP0	(GDMA_BASE+0x60)	/* Generic DMA Destination Block Pointer 0 Register */
#define GDMADBL0	(GDMA_BASE+0x64)	/* Generic DMA Destination Block Length 0 Register */
#define GDMADBP1	(GDMA_BASE+0x68)	/* Generic DMA Destination Block Pointer 1 Register */
#define GDMADBL1	(GDMA_BASE+0x6C)	/* Generic DMA Destination Block Length 1 Register */
#define GDMADBP2	(GDMA_BASE+0x70)	/* Generic DMA Destination Block Pointer 2 Register */
#define GDMADBL2	(GDMA_BASE+0x74)	/* Generic DMA Destination Block Length 2 Register */
#define GDMADBP3	(GDMA_BASE+0x78)	/* Generic DMA Destination Block Pointer 3 Register */
#define GDMADBL3	(GDMA_BASE+0x7C)	/* Generic DMA Destination Block Length 3 Register */
#define GDMADBP4	(GDMA_BASE+0x80)	/* Generic DMA Destination Block Pointer 4 Register */
#define GDMADBL4	(GDMA_BASE+0x84)	/* Generic DMA Destination Block Length 4 Register */
#define GDMADBP5	(GDMA_BASE+0x88)	/* Generic DMA Destination Block Pointer 5 Register */
#define GDMADBL5	(GDMA_BASE+0x8C)	/* Generic DMA Destination Block Length 5 Register */
#define GDMADBP6	(GDMA_BASE+0x90)	/* Generic DMA Destination Block Pointer 6 Register */
#define GDMADBL6	(GDMA_BASE+0x94)	/* Generic DMA Destination Block Length 6 Register */
#define GDMADBP7	(GDMA_BASE+0x98)	/* Generic DMA Destination Block Pointer 7 Register */
#define GDMADBL7	(GDMA_BASE+0x9C)	/* Generic DMA Destination Block Length 7 Register */

/* Generic DMA Control Register */
#define GDMA_ENABLE			(1<<31)		/* Enable GDMA */
#define GDMA_POLL			(1<<30)		/* Kick off GDMA */
#define GDMA_FUNCMASK		(0xf<<24)	/* GDMA Function Mask */
#define GDMA_MEMCPY			(0x0<<24)	/* Memory Copy */
#define GDMA_CHKOFF			(0x1<<24)	/* Checksum Offload */
#define GDMA_STCAM			(0x2<<24)	/* Sequential T-CAM */
#define GDMA_MEMSET			(0x3<<24)	/* Memory Set */
#define GDMA_B64ENC			(0x4<<24)	/* Base 64 Encode */
#define GDMA_B64DEC			(0x5<<24)	/* Base 64 Decode */
#define GDMA_QPENC			(0x6<<24)	/* Quoted Printable Encode */
#define GDMA_QPDEC			(0x7<<24)	/* Quoted Printable Decode */
#define GDMA_MIC			(0x8<<24)	/* Wireless MIC */
#define GDMA_MEMXOR			(0x9<<24)	/* Memory XOR */
#define GDMA_MEMCMP			(0xa<<24)	/* Memory Compare */
#define GDMA_BYTESWAP		(0xb<<24)	/* Byte Swap */
#define GDMA_PATTERN		(0xc<<24)	/* Pattern Match */
#define GDMA_SWAPTYPE0		(0<<22)		/* Original:{0,1,2,3} => {1,0,3,2} */
#define GDMA_SWAPTYPE1		(1<<22)		/* Original:{0,1,2,3} => {3,2,1,0} */
#define GDMA_ENTSIZMASK		(3<<20)		/* T-CAM Entry Size Mask */
#define GDMA_ENTSIZ32		(0<<20)		/* T-CAM Entry Size 32 bits */
#define GDMA_ENTSIZ64		(1<<20)		/* T-CAM Entry Size 64 bits */
#define GDMA_ENTSIZ128		(2<<20)		/* T-CAM Entry Size 128 bits */
#define GDMA_ENTSIZ256		(3<<20)		/* T-CAM Entry Size 256 bits */

/* Generic DMA Interrupt Mask Register */
#define GDMA_COMPIE			(1<<31)		/* Completed Interrupt Enable */
#define GDMA_NEEDCPUIE		(1<<28)		/* Need-CPU Interrupt Enable */

/* Generic DMA Interrupt Status Register */
#define GDMA_COMPIP			(1<<31)		/* Completed Interrupt Status (write 1 to clear) */
#define GDMA_NEEDCPUIP		(1<<28)		/* Need-CPU Interrupt Status (write 1 to clear) */

/* Generic DMA Source Block Length n. Register */
#define GDMA_LDB			(1<<31)		/* Last Data Block */
#define GDMA_BLKLENMASK		(0x1fff)	/* Block Length (valid value: from 1 to 8K-1 bytes) */

/*
 *	Some bits in GDMACNR are only for internal used.
 *	However, driver needs to configure them.
 *
 *	burstSize[7:6] -- 00:4W, 01:8W, 10:16W, 11:32W.
 *	enough[5:4]    -- 00:>16, 01:>10, 10:>4, 00:>0
 *	dlow[3:2]      -- 00:>24, 01:>20. 10:>16, 11:>8
 *	slow[1:0]      -- 00:>24, 01:>20. 10:>16, 11:>8
 */
#define internalUsedGDMACNR (0x000000C0)
#endif // _ASICREGS_H



struct tx_desc_info {
    // for cur_txbd->TXBD_ELE[1].Dword1 : packet header
    // for WlanHAL, paddr does not use currently.
	void				*pframe;
	unsigned char		type;
#if defined(CONFIG_PCI_HCI) || defined(CONFIG_RTL_TRIBAND_SUPPORT)
#ifdef CONFIG_WLAN_HAL
    // buf_pframe[0]: cur_txbd->TXBD_ELE[2].Dword1 : packet payload
    // buf_pframe[1]: cur_txbd->TXBD_ELE[3].Dword1 : icv/mic
    // Currently, buf_paddr and buf_len do not use...
    unsigned char        buf_type[2];
#if defined(CONFIG_NET_PCI) || defined(CONFIG_PCI_HCI)
    unsigned int        buf_paddr[2];
#endif
	unsigned int        buf_len[2];
    void                *buf_pframe[2];
#ifdef WLAN_HAL_TX_AMSDU

    unsigned char        buf_type_amsdu[WLAN_HAL_TX_AMSDU_MAX_NUM];
    void                *buf_pframe_amsdu[WLAN_HAL_TX_AMSDU_MAX_NUM];
#if defined(CONFIG_NET_PCI) && !defined(USE_RTL8186_SDK)	
	unsigned int        buf_paddr_amsdu[WLAN_HAL_TX_AMSDU_MAX_NUM];
    unsigned int        buf_len_amsdu[WLAN_HAL_TX_AMSDU_MAX_NUM];
#endif

	unsigned int amsdubuf_len[WLAN_HAL_TX_AMSDU_MAX_NUM];
	unsigned char amsdu_num;
#endif

#endif // CONFIG_WLAN_HAL

#if defined(AP_SWPS_OFFLOAD)
    unsigned char   test_pkt;
    unsigned short  SWPS_sequence;
    unsigned char   Drop_ID;
    //unsigned char   SWPS_pktQ;
    int    SWPS_pkt_Qos;
    struct stat_info *swps_pstat;
    //to record txbd needed info
    unsigned int hdr_len;
    unsigned int skbbuf_len;
#ifdef SUPPORT_TX_AMSDU    
    //unsigned int amsdubuf_len[WLAN_HAL_TX_AMSDU_MAX_NUM];
    //unsigned char amsdu_num;
#endif    
    unsigned int mic_icv_len;
    unsigned int PSB_len;
    unsigned char isWiFiHdr;
    struct TX_DESC_88XX *ptxdesc; //is the final filled ok txdesc
#if defined(SUPPORT_TXDESC_IE)
    unsigned char IE_bitmap;
#endif
#endif

#if defined(CONFIG_NET_PCI) || defined(CONFIG_PCI_HCI)
	unsigned long		paddr;
	unsigned int		len;
#endif	
#ifndef TXDESC_INFO
	struct stat_info *pstat;
	unsigned int	rate;
#endif
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	u8		*buf_ptr;
  #if !defined(CONFIG_RTL_TRIBAND_SUPPORT)
	u32		buf_len;
  #endif

	struct stat_info *pstat;
	unsigned int	rate;
#endif
	struct rtl8192cd_priv	*priv;
};



struct rtl8192cd_tx_desc_info {
    /* refer to _TX_QUEUE_ to map tx_info to tx queue */
	struct tx_desc_info	tx_info0[NONBE_TXDESC];     /* mgt queue */
	struct tx_desc_info	tx_info1[NONBE_TXDESC];     /* bk queue */
	struct tx_desc_info	tx_info2[BE_TXDESC];        /* be queue */
	struct tx_desc_info	tx_info3[NONBE_TXDESC];     /* vi queue */
	struct tx_desc_info	tx_info4[NONBE_TXDESC];     /* vo queue */
#if defined(TAROKO_0)
#else
#if defined(CONFIG_WLAN_HAL_8814BE) || defined(CONFIG_WLAN_HAL_8198F) //98F tmp to avoid sw error, 16 MBSSID need more tx_info
	struct tx_desc_info	tx_info5[NUM_TX_DESC_HQ*20]; /* for max. 8 interfaces, HIQ0 ~ HIQ19 */
#else
	struct tx_desc_info	tx_info5[NUM_TX_DESC_HQ*8]; /* for max. 8 interfaces, HIQ0 ~ HIQ7 */
#endif
    struct tx_desc_info tx_info_cmd[NONBE_TXDESC];  /* cmd queue */
#endif
};

typedef struct _BB_REGISTER_DEFINITION {
	unsigned int rfintfs; 			// set software control:
									//		0x870~0x877[8 bytes]
	unsigned int rfintfi; 			// readback data:
									//		0x8e0~0x8e7[8 bytes]
	unsigned int rfintfo; 			// output data:
									//		0x860~0x86f [16 bytes]
	unsigned int rfintfe; 			// output enable:
									//		0x860~0x86f [16 bytes]
	unsigned int rf3wireOffset; 	// LSSI data:
									//		0x840~0x84f [16 bytes]
	unsigned int rfLSSI_Select; 	// BB Band Select:
									//		0x878~0x87f [8 bytes]
	unsigned int rfTxGainStage;		// Tx gain stage:
									//		0x80c~0x80f [4 bytes]
	unsigned int rfHSSIPara1;		// wire parameter control1 :
									//		0x820~0x823,0x828~0x82b, 0x830~0x833, 0x838~0x83b [16 bytes]
	unsigned int rfHSSIPara2;		// wire parameter control2 :
									//		0x824~0x827,0x82c~0x82f, 0x834~0x837, 0x83c~0x83f [16 bytes]
	unsigned int rfSwitchControl; 	//Tx Rx antenna control :
									//		0x858~0x85f [16 bytes]
	unsigned int rfAGCControl1; 	//AGC parameter control1 :
									//		0xc50~0xc53,0xc58~0xc5b, 0xc60~0xc63, 0xc68~0xc6b [16 bytes]
	unsigned int rfAGCControl2; 	//AGC parameter control2 :
									//		0xc54~0xc57,0xc5c~0xc5f, 0xc64~0xc67, 0xc6c~0xc6f [16 bytes]
	unsigned int rfRxIQImbalance; 	//OFDM Rx IQ imbalance matrix :
									//		0xc14~0xc17,0xc1c~0xc1f, 0xc24~0xc27, 0xc2c~0xc2f [16 bytes]
	unsigned int rfRxAFE;  			//Rx IQ DC ofset and Rx digital filter, Rx DC notch filter :
									//		0xc10~0xc13,0xc18~0xc1b, 0xc20~0xc23, 0xc28~0xc2b [16 bytes]
	unsigned int rfTxIQImbalance; 	//OFDM Tx IQ imbalance matrix
									//		0xc80~0xc83,0xc88~0xc8b, 0xc90~0xc93, 0xc98~0xc9b [16 bytes]
	unsigned int rfTxAFE; 			//Tx IQ DC Offset and Tx DFIR type
									//		0xc84~0xc87,0xc8c~0xc8f, 0xc94~0xc97, 0xc9c~0xc9f [16 bytes]
	unsigned int rfLSSIReadBack; 	//LSSI RF readback data
									//		0x8a0~0x8af [16 bytes]
	unsigned int rfLSSIReadBackPi;	//LSSI RF readback data
									//		0x8b8~0x8bc [8 bytes]
}BB_REGISTER_DEFINITION_T, *PBB_REGISTER_DEFINITION_T;

struct rx_desc_info {
	void*			pbuf;
	unsigned long		paddr;
};

struct rtl8192cd_hw {
#if defined(CONFIG_PCI_HCI) || defined(CONFIG_RTL_TRIBAND_SUPPORT)
	unsigned short	seq;	// sw seq
#elif defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	atomic_t	seq;
#endif
#if (BEAMFORMING_SUPPORT == 1)
	unsigned short	sounding_seq;
#endif

#ifdef WIFI_WMM
//	unsigned short	AC_seq[8];

// switch BE to VI
	unsigned int	VO_pkt_count;
	unsigned int	VI_pkt_count;
	unsigned int	VI_rx_pkt_count;
	unsigned int	BE_pkt_count;
	unsigned int	BK_pkt_count;

	unsigned int	VI_droppkt_count;
	unsigned int	VO_droppkt_count;	
	unsigned int	BE_droppkt_count;	
	unsigned int	BK_droppkt_count;		
#endif

#if defined(RTLWIFINIC_GPIO_CONTROL)
	unsigned char	GPIO_dir[16];		// bit[0-1] 0x01: input, 0x10: output
#ifdef PCIE_POWER_SAVING
	unsigned int	GPIO_cache[2];				
#endif	
#endif

#ifndef PRIV_STA_BUF
	unsigned long	alloc_dma_buf;
#endif

#ifdef CONFIG_PCI_HCI
	unsigned long	ring_dma_addr;
	unsigned long	ring_virt_addr;
	unsigned int	ring_buf_len;
	unsigned int	cur_rx;
#ifdef DELAY_REFILL_RX_BUF
	unsigned int	cur_rx_refill;
#endif
	struct	rx_desc			*rx_descL;
	unsigned long			rx_ring_addr;
	struct	rx_desc_info	rx_infoL[NUM_RX_DESC];

	/* For TX DMA Synchronization */
	unsigned long	rx_descL_dma_addr[NUM_RX_DESC];

	unsigned int	txhead0;
	unsigned int	txhead1;
	unsigned int	txhead2;
	unsigned int	txhead3;
	unsigned int	txhead4;
	unsigned int	txhead5;

	unsigned int	txtail0;
	unsigned int	txtail1;
	unsigned int	txtail2;
	unsigned int	txtail3;
	unsigned int	txtail4;
	unsigned int	txtail5;

	struct tx_desc	*tx_desc0;
	struct tx_desc	*tx_desc1;
	struct tx_desc	*tx_desc2;
	struct tx_desc	*tx_desc3;
	struct tx_desc	*tx_desc4;
	struct tx_desc	*tx_desc5;
	struct tx_desc	*tx_descB;

#if CONFIG_WLAN_NOT_HAL_EXIST
	/* For TX DMA Synchronization */
#ifdef OSK_LOW_TX_DESC
	unsigned long	tx_desc0_dma_addr[NONBE_TXDESC];
	unsigned long	tx_desc1_dma_addr[NONBE_TXDESC];
	unsigned long	tx_desc2_dma_addr[BE_TXDESC];
	unsigned long	tx_desc3_dma_addr[NONBE_TXDESC];
	unsigned long	tx_desc4_dma_addr[NONBE_TXDESC];
	unsigned long	tx_desc5_dma_addr[BE_TXDESC];//for every interface use, NUM_TX_DESC_HQ = (BE_TXDESC>>3)
#else
	unsigned long	tx_desc0_dma_addr[NUM_TX_DESC];
	unsigned long	tx_desc1_dma_addr[NUM_TX_DESC];
	unsigned long	tx_desc2_dma_addr[NUM_TX_DESC];
	unsigned long	tx_desc3_dma_addr[NUM_TX_DESC];
	unsigned long	tx_desc4_dma_addr[NUM_TX_DESC];
	unsigned long	tx_desc5_dma_addr[NUM_TX_DESC];
#endif
	unsigned long	tx_descB_dma_addr[NUM_TX_DESC];
#endif

	unsigned long	tx_ring0_addr;
	unsigned long	tx_ring1_addr;
	unsigned long	tx_ring2_addr;
	unsigned long	tx_ring3_addr;
	unsigned long	tx_ring4_addr;
	unsigned long	tx_ring5_addr;
	unsigned long	tx_ringB_addr;
#if 0/*no use now*/
	unsigned int		cur_rxcmd;
	struct rx_desc		*rxcmd_desc;
	unsigned long		rxcmd_ring_addr;
	struct rx_desc_info	rxcmd_info[NUM_CMD_DESC];
	unsigned long		rxcmd_desc_dma_addr[NUM_CMD_DESC];

	unsigned int		txcmdhead;
	unsigned int		txcmdtail;
	struct tx_desc		*txcmd_desc;
	unsigned long		txcmd_desc_dma_addr[NUM_CMD_DESC];
	unsigned long		txcmd_ring_addr;
#endif
#endif // CONFIG_PCI_HCI
#if defined(CONFIG_WLAN_HAL_8814AE) || defined(CONFIG_WLAN_HAL_8822BE) || defined(CONFIG_WLAN_HAL_8821CE) || defined(CONFIG_WLAN_HAL_8814BE)	/* change to ANT_3T3R or ANT_4T4R */
	char				MCSTxAgcOffset_A[32];
	char				MCSTxAgcOffset_B[32];
	char				MCSTxAgcOffset_C[32];
	char				MCSTxAgcOffset_D[32];
	char				VHTTxAgcOffset_A[40];
	char				VHTTxAgcOffset_B[40];
	char				VHTTxAgcOffset_C[40];
	char				VHTTxAgcOffset_D[40];
#else
	char				MCSTxAgcOffset_A[16];
	char				MCSTxAgcOffset_B[16];
	char				VHTTxAgcOffset_A[20];
	char				VHTTxAgcOffset_B[20];
#endif	
	//_TXPWR_REDEFINE ?? int or char ??
#if 0
	int					CCKTxAgc_A[4];
	int					CCKTxAgc_B[4];
#endif

#if defined(CONFIG_WLAN_HAL_8198F) || defined(CONFIG_WLAN_HAL_8814BE) || defined(CONFIG_WLAN_HAL_8822CE) || defined(CONFIG_WLAN_HAL_8812FE) || defined(CONFIG_WLAN_HAL_8197G)
	char				CCKTxAgc[4];
	char				OFDMTxAgcOffset[8];
	char				MCSTxAgcOffset[32];
	char				VHTTxAgcOffset[40];
#endif


	char				CCKTxAgc_A[4];
	char				CCKTxAgc_B[4];
	char				OFDMTxAgcOffset_A[8];
	char				OFDMTxAgcOffset_B[8];
#if defined(CONFIG_WLAN_HAL_8814AE) || defined(CONFIG_WLAN_HAL_8822BE) || defined(CONFIG_WLAN_HAL_8821CE) || defined(CONFIG_WLAN_HAL_8814BE)
    char                CCKTxAgc_C[4];
    char                CCKTxAgc_D[4];
	char				OFDMTxAgcOffset_C[8];
	char				OFDMTxAgcOffset_D[8];	
#endif
#if defined(CONFIG_WLAN_HAL_8814AE) || defined(CONFIG_WLAN_HAL_8822BE) || defined(CONFIG_WLAN_HAL_8822CE) || defined(CONFIG_WLAN_HAL_8821CE) || defined(CONFIG_WLAN_HAL_8814BE) || defined(CONFIG_WLAN_HAL_8198F) || defined(CONFIG_WLAN_HAL_8812FE) || defined(CONFIG_WLAN_HAL_8197G)
	unsigned char		CurrentTxAgcCCK[4][4];
	unsigned char		CurrentTxAgcOFDM[4][8];
	unsigned char		CurrentTxAgcMCS[4][32];	
	unsigned char		CurrentTxAgcVHT[4][40];
	unsigned char		minTxAgcVal;	
#endif

	unsigned char		TXPowerOffset;

	unsigned int				NumTotalRFPath;
	unsigned char				tx_path_start;
	unsigned char				tx_path_end;
	unsigned char				rx_path_start;
	unsigned char				rx_path_end;
	/*PHY related*/
	BB_REGISTER_DEFINITION_T	PHYRegDef[4];	//Radio A/B/C/D

	// Joseph test for shorten RF config.
	unsigned int				RfReg0Value[4];

	// for DIG checking
	unsigned char				signal_strength;	// 1=low and dig off, 2=normal and dig on, 3=high power and dig on

//#ifdef INTERFERENCE_CONTROL
	// for NBI filter checking
	unsigned char				nbi_filter_on;
//#endif

	// dynamic CCK Tx power by rssi
	unsigned char				CCKTxAgc_enhanced;

	// dynamic CCK CCA enhance by rssi
	unsigned char				CCK_CCA_enhanced;

	unsigned char				EDCCA_on;

	// for Multicast Rx dynamic mechanism
	unsigned char				rxmlcst_rssi;
	unsigned char				initial_gain;

	// MIMO TR hw support checking
	unsigned char				MIMO_TR_hw_support;

	// AC Tx checking
	unsigned char				is_AC_TX_support;

	// dynamic Rx path selection by signal strength
	unsigned char				ant_off_num;
	unsigned char				ant_off_bitmap;
	unsigned char				ant_on_criteria[4];
	unsigned char				ant_cck_sel;

	// Tx power control
	unsigned char				lower_tx_power;
	unsigned int				power_backup[15];
/*
	// Tx power tracking
#if defined(RTL8190) || defined(RTL8192E)
	unsigned char				tpt_inited;
	unsigned char				tpt_ofdm_swing_idx;
	unsigned char				tpt_cck_swing_idx;
	unsigned char				tpt_tssi_total;
	unsigned char				tpt_tssi_num;
	unsigned char				tpt_tssi_waiting;
	unsigned char				tpt_tracking_num;
#endif
*/
//	struct timer_list			tpt_timer;
#ifdef CONFIG_PCI_HCI
	struct rtl8192cd_tx_desc_info	tx_info;
#endif

	unsigned char				check_reg824;
	unsigned int				reg824_bit9;

	unsigned char				InternalPA5G[2];
	unsigned char				bNewTxGainTable;

	unsigned char				RTSInitRate;
	unsigned char				RTSInitRate_Candidate;
	unsigned char				LowestInitRate;
//	unsigned char				bErpProtection;
	
#if defined(CONFIG_USB_HCI) && defined(CONFIG_RTL_92C_SUPPORT)
	unsigned char				HIQ_nolimit_en;
#endif
};

#ifdef RX_BUFFER_GATHER
enum {
	GATHER_FIRST = 1,
	GATHER_MIDDLE = 2,
	GATHER_LAST = 4,
};
#endif

typedef enum _PHY_BAND_SELECT_ {
	PHY_BAND_2G = BIT(0),
	PHY_BAND_5G = BIT(1)
} PHY_BAND_SELECT;


#define VERSION_MASK  0xF00F
#define CHIPVER_MASK  0x00F0
enum _CHIP_VERSION_ {
	VERSION_8188C = 0x1000,
	VERSION_8192C = 0x1001,
	VERSION_8192D = 0x1002,
	VERSION_8188E = 0x1003,
	VERSION_8812E = 0x1004, //CONFIG_RTL_8812_SUPPORT
	VERSION_8192E = 0x1005,
	VERSION_8881A = 0x1006,
	VERSION_8814A = 0x1007, //CONFIG_WLAN_HAL_8814AE
	VERSION_8723B = 0x1008, //CONFIG_RTL_8723B_SUPPORT
	VERSION_8822B = 0x1009, //CONFIG_WLAN_HAL_8822B
	VERSION_8197F = 0x100A, //CONFIG_WLAN_HAL_8197F
	VERSION_8821C = 0x100B, //CONFIG_WLAN_HAL_8821C
	VERSION_8192F = 0x100C, //CONFIG_WLAN_HAL_8192F
	VERSION_8198F = 0x100D, //CONFIG_WLAN_HAL_8198F
	VERSION_8814B = 0x100E, //CONFIG_WLAN_HAL_8814BE
	VERSION_8197G = 0x100F, //CONFIG_WLAN_HAL_8197G
	VERSION_8822C = 0x2000, //CONFIG_WLAN_HAL_8822C
	VERSION_8812F = 0x2001, //CONFIG_WLAN_HAL_8812F
};

enum _ARFR_TABLE_SET_
{
	ARFR_2T_40M = 0,
	ARFR_2T_Band_A_40M = 1,
	ARFR_2T_Band_A_20M = 1,
	ARFR_2T_20M = 2,
	ARFR_1T_40M = 12,
	ARFR_1T_20M = 13,
	ARFR_BG_MIX = 4,
	ARFR_G_ONLY = 5,
	ARFR_Band_A_BMC = 5,
	ARFR_B_ONLY = 6,
	ARFR_BMC = 6,
};

enum _RF_TX_RATE_ {
	_1M_RATE_	= 2,
	_2M_RATE_	= 4,
	_5M_RATE_	= 11,
	_6M_RATE_	= 12,
	_9M_RATE_	= 18,
	_11M_RATE_	= 22,
	_12M_RATE_	= 24,
	_18M_RATE_	= 36,
	_22M_RATE_	= 44,
	_24M_RATE_	= 48,
	_33M_RATE_	= 66,
	_36M_RATE_	= 72,
	_48M_RATE_	= 96,
	_54M_RATE_	= 108,
	
	_MCS0_RATE_	= (HT_RATE_ID +  0),
	_MCS1_RATE_	= (HT_RATE_ID +  1),
	_MCS2_RATE_	= (HT_RATE_ID +  2),
	_MCS3_RATE_	= (HT_RATE_ID +  3),
	_MCS4_RATE_	= (HT_RATE_ID +  4),
	_MCS5_RATE_	= (HT_RATE_ID +  5),
	_MCS6_RATE_	= (HT_RATE_ID +  6),
	_MCS7_RATE_	= (HT_RATE_ID +  7),
	_MCS8_RATE_	= (HT_RATE_ID +  8),
	_MCS9_RATE_	= (HT_RATE_ID +  9),
	_MCS10_RATE_= (HT_RATE_ID + 10),
	_MCS11_RATE_= (HT_RATE_ID + 11),
	_MCS12_RATE_= (HT_RATE_ID + 12),
	_MCS13_RATE_= (HT_RATE_ID + 13),
	_MCS14_RATE_= (HT_RATE_ID + 14),
	_MCS15_RATE_= (HT_RATE_ID + 15),
	_MCS16_RATE_= (HT_RATE_ID + 16),
	_MCS17_RATE_= (HT_RATE_ID + 17),
	_MCS18_RATE_= (HT_RATE_ID + 18),
	_MCS19_RATE_= (HT_RATE_ID + 19),
	_MCS20_RATE_= (HT_RATE_ID + 20),
	_MCS21_RATE_= (HT_RATE_ID + 21),
	_MCS22_RATE_= (HT_RATE_ID + 22),
	_MCS23_RATE_= (HT_RATE_ID + 23),
	_MCS24_RATE_= (HT_RATE_ID + 24),
	_MCS25_RATE_= (HT_RATE_ID + 25),
	_MCS26_RATE_= (HT_RATE_ID + 26),
	_MCS27_RATE_= (HT_RATE_ID + 27),
	_MCS28_RATE_= (HT_RATE_ID + 28),
	_MCS29_RATE_= (HT_RATE_ID + 29),
	_MCS30_RATE_= (HT_RATE_ID + 30),
	_MCS31_RATE_= (HT_RATE_ID + 31),

	_NSS1_MCS0_RATE_ = (VHT_RATE_ID +	0),
	_NSS1_MCS1_RATE_ = (VHT_RATE_ID +	1),
	_NSS1_MCS2_RATE_ = (VHT_RATE_ID +	2),
	_NSS1_MCS3_RATE_ = (VHT_RATE_ID +	3),
	_NSS1_MCS4_RATE_ = (VHT_RATE_ID +	4),
	_NSS1_MCS5_RATE_ = (VHT_RATE_ID +	5),
	_NSS1_MCS6_RATE_ = (VHT_RATE_ID +	6),
	_NSS1_MCS7_RATE_ = (VHT_RATE_ID +	7),
	_NSS1_MCS8_RATE_ = (VHT_RATE_ID +	8),
	_NSS1_MCS9_RATE_ = (VHT_RATE_ID +	9),
	_NSS2_MCS0_RATE_ = (VHT_RATE_ID +  10),
	_NSS2_MCS1_RATE_ = (VHT_RATE_ID +  11),
	_NSS2_MCS2_RATE_ = (VHT_RATE_ID +  12),
	_NSS2_MCS3_RATE_ = (VHT_RATE_ID +  13),
	_NSS2_MCS4_RATE_ = (VHT_RATE_ID +  14),
	_NSS2_MCS5_RATE_ = (VHT_RATE_ID +  15),
	_NSS2_MCS6_RATE_ = (VHT_RATE_ID +  16),
	_NSS2_MCS7_RATE_ = (VHT_RATE_ID +  17),
	_NSS2_MCS8_RATE_ = (VHT_RATE_ID +  18),
	_NSS2_MCS9_RATE_ = (VHT_RATE_ID +  19),
	_NSS3_MCS0_RATE_ = (VHT_RATE_ID +  20),
	_NSS3_MCS1_RATE_ = (VHT_RATE_ID +  21),
	_NSS3_MCS2_RATE_ = (VHT_RATE_ID +  22),
	_NSS3_MCS3_RATE_ = (VHT_RATE_ID +  23),
	_NSS3_MCS4_RATE_ = (VHT_RATE_ID +  24),
	_NSS3_MCS5_RATE_ = (VHT_RATE_ID +  25),
	_NSS3_MCS6_RATE_ = (VHT_RATE_ID +  26),
	_NSS3_MCS7_RATE_ = (VHT_RATE_ID +  27),
	_NSS3_MCS8_RATE_ = (VHT_RATE_ID +  28),
	_NSS3_MCS9_RATE_ = (VHT_RATE_ID +  29),
	_NSS4_MCS0_RATE_ = (VHT_RATE_ID +  30),
	_NSS4_MCS1_RATE_ = (VHT_RATE_ID +  31),
	_NSS4_MCS2_RATE_ = (VHT_RATE_ID +  32),
	_NSS4_MCS3_RATE_ = (VHT_RATE_ID +  33),
	_NSS4_MCS4_RATE_ = (VHT_RATE_ID +  34),
	_NSS4_MCS5_RATE_ = (VHT_RATE_ID +  35),
	_NSS4_MCS6_RATE_ = (VHT_RATE_ID +  36),
	_NSS4_MCS7_RATE_ = (VHT_RATE_ID +  37),
	_NSS4_MCS8_RATE_ = (VHT_RATE_ID +  38),
	_NSS4_MCS9_RATE_ = (VHT_RATE_ID +  39),
};


enum _RTL8192CD_TX_DESC_ {
	// TX cmd desc
	// Dword 0
	TX_OWN				= BIT(31),
	TX_GF				= BIT(30),
	TX_NoACM			= BIT(29),
	TX_LINIP			= BIT(28),
	TX_FirstSeg			= BIT(27),
	TX_LastSeg			= BIT(26),
	TX_HTC				= BIT(25),
	TX_BMC				= BIT(24),
	TX_OffsetSHIFT		= 16,
	TX_OffsetMask		= 0x0FF,
	TX_PktSizeSHIFT		= 0,
	TX_PktSizeMask 		= 0x0FFFF,

//	TX_AMSDU			= BIT(29),
//	TX_TYPESHIFT		= 24,
//	TX_TYPEMask			= 0x3,


	// Dword 1
#if  defined(CONFIG_RTL_8812_SUPPORT)
	TXdesc_92E_MOREDATA	= BIT(29),
#endif		
//	TX_PaddingSHIFT	= 24,
//	TX_PaddingMask		= 0x0FF,
	TX_PktOffsetSHIFT	= 26,
	TX_PktOffsetMask	= 0x01F,
	TX_SecTypeSHIFT	= 22,
	TX_SecTypeMask	= 0x03,
	TX_EnDescID		= BIT(21),
	TX_NAVUSEHDR		= BIT(20),
	TX_RateIDSHIFT		= 16 ,
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
	TXdesc_92E_RateIDMask		= 0x1F,
#endif	
	TX_RateIDMask		= 0x0F,	
	TX_PIFS				= BIT(15),
	TX_LSigTxopEn		= BIT(14),
	TX_RdNavExt		= BIT(13),
	TX_QSelSHIFT		= 8,
	TX_QSelMask		= 0x01F,
	TX_RdEn			= BIT(7),
	TX_BK				= BIT(6),
	TX_AggEn			= BIT(5),
	TX_MacIdSHIFT		= 0,
	TX_MacIdMask		= 0x01F,
#ifdef CONFIG_RTL_88E_SUPPORT
	TXdesc_88E_MacIdSHIFT       = 0,
	TXdesc_88E_MacIdMask        = 0x03F,
#endif
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
	TXdesc_92E_MacIdMask        = 0x07F,
#endif	
//	TX_MoreData			= BIT(5),
//	TX_MoreFrag			= BIT(6),
//	TX_AckPolicySHIFT	= 13,
//	TX_AckPolicyMask	= 0x3, // 2 bits
//	TX_NoACM			= BIT(15),
//	TX_NonQos			= BIT(16),
//	TX_KeyIDSHIFT		= 17 ,
//	TX_OUI 				= BIT(19),
//	TX_PktType			= BIT(20),
//	TX_HTC				= BIT(24),	//padding0
//	TX_WDS				= BIT(25),	//padding1
//	TX_HWPC				= BIT(31),


	// DWORD 2
	TX_TxAntHtSHIFT	= 30,
	TX_TxAntHtMask		= 0x03,
	TX_TxAntlSHIFT		= 28,
	TX_TxAntlMask		= 0x03,
	TX_TxAntCckSHIFT	= 26,
	TX_TxAntCckMask	= 0x03,
	TX_AntSelB			= BIT(25),
	TX_AntSelA			= BIT(24),
#ifdef CONFIG_RTL_88E_SUPPORT
	TXdesc_88E_BtInt    = BIT(23),
#endif
	TX_AmpduDstySHIFT	= 20,
	TX_AmpduDstyMask	= 0x07,
	TX_CCX				= BIT(19),
	TX_RAW				= BIT(18),
	TX_MoreFrag		= BIT(17),
#ifdef CONFIG_RTL_88E_SUPPORT
	TXdesc_88E_BK       = BIT(16),
#endif
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
	TXdesc_92E_BK       = BIT(16),
#endif
	TX_BarRtyThSHIFT	= 14,
	TX_BarRtyThMask	= 0x03,
#ifdef CONFIG_RTL_88E_SUPPORT
	TXdesc_88E_RdEn		= BIT(13),
	TXdesc_88E_AggEn    = BIT(12),
#endif
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
	TXdesc_92E_RdEn		= BIT(13),
	TXdesc_92E_AggEn	= BIT(12),
#endif	
	TX_DataRcSHIFT		= 6,
	TX_DataRcMask		= 0x03F,
	TX_RtsRcSHIFT		= 0,
	TX_RtsRcMask		= 0x03F,


	TX_8812_CcaRtsSHIFT	= 10,
	TX_8812_CcaRtsMask	= 0x03,

#if (BEAMFORMING_SUPPORT == 1)
	TX_8812_PAIDSHIFT = 0,
	TX_8812_PAIDMask	= 0x1ff,
	TX_8812_GIDSHIFT = 24,
	TX_8812_GIDMask	= 0x3f,	
#endif

//	TX_DataRetryLmtSHIFT	= 0,
//	TX_DataRetryLmtMask	= 0x3F,	// 6 bits
//	TX_RetryLmtEn			= BIT(6),
//	TX_TSFLSHIFT			= 7,
//	TX_TSFLMask 			= 0x1f,
//	TX_RTSRCSHIFT			= 12,
//	TX_RTSRCMask			= 0x3F,	// Reserved for HW RTS Retry Count.
//	TX_DATARCSHIFT			= 18,
//	TX_DATARCMask			 = 0x3F ,	// Reserved for HW DATA Retry Count.
	//TX_Rsvd1:5;
//	TX_AggEn				=BIT(29),
//	TX_BK					= BIT(30),	//Aggregation break.
//	TX_OwnMAC				= BIT(31),


	//DWORD3
#ifdef CONFIG_RTL_88E_SUPPORT
	TXdesc_88E_EnHwSeq  = BIT(31),
	TXdesc_88E_TriggerInt   = BIT(30),
	TXdesc_88E_Tagi     = BIT(29),
	TXdesc_88E_CpuHandle    = BIT(28),
#endif
	TX_PktIdSHIFT			= 28,
	TX_PktIdMask			= 0x0F,
	TX_SeqSHIFT			= 16,
	TX_SeqMask				= 0x0FFF,
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
	TXdesc_92E_NDPASHIFT	= 22,
	TXdesc_92E_NDPAMASK		= 0x03,	
	TXdesc_92E_MAX_AGG_NUMSHIFT = 17,
	TXdesc_92E_MAX_AGG_NUMMask	= 0x1F,
	TXdesc_92E_NAVUSEHDR	= BIT(15),
	TXdesc_92E_PortId		= BIT(14), 
	TXdesc_92E_HwRtsEn		= BIT(13),
	TXdesc_92E_RtsEn		= BIT(12),
	TXdesc_92E_CTS2Self		= BIT(11),
	TXdesc_92E_DisDataFB	= BIT(10),
	TXdesc_92E_DisRtsFB 	= BIT(9),
	TXdesc_92E_UseRate		= BIT(8),
#endif	
	TX_TailPageSHIFT		= 8,
	TX_TailPageMask		= 0x0FF,
	TX_NextHeadPageSHIFT	= 0,
	TX_NextHeadPageMask	= 0x0FF,

//	TX_FragSHIFT			= 28,
//	TX_FragMask				= 0xF,


	// DWORD 4
	TX_CtrlStbcSHIFT	= 30,
	TX_CtrlStbcMask		= 0x03,
	TX_RtsScSHIFT		= 28,
	TX_RtsScMask		= 0x3,
	TX_RtsBw			= BIT(27),
	TX_RtsShort			= BIT(26),
	TX_DataBw			= BIT(25),
	TX_DataShort		= BIT(24),
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
	TXdesc_92E_RtsRateSHIFT		= 24,
	TXdesc_92E_RtsRateMask		= 0x01F,
#endif		
	TX_DataStbcSHIFT	= 22,
	TX_DataStbcMask	= 0x03,
	TX_DataScSHIFT		= 20,
	TX_DataScMask		= 0x03,
	TX_CTS2AP			= BIT(19),
	TX_WaitDCTS		= BIT(18),
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
	TXdesc_92E_DataRtyLmtSHIFT	= 18,
	TXdesc_92E_DataRtyLmtMask	= 0x03F,
#endif		
#ifdef CONFIG_RTL_88E_SUPPORT
	TXdesc_88E_PwrStatusSHIFT       = 15,
	TXdesc_88E_PwrStatusMask        = 0x07,
	TXdesc_88E_PortId	= BIT(14), 
#endif
	TX_HwRtsEn			= BIT(13),
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
	TXdesc_92E_RtyLmtEn			= BIT(17),

	TXdesc_92E_RtsRateFBLmtSHIFT	= 13,	
	TXdesc_92E_RtsRateFBLmtMask		= 0x0f,	

#endif		
	TX_RtsEn			= BIT(12),
	TX_CTS2Self			= BIT(11),
	TX_DisDataFB		= BIT(10),
	TX_DisRtsFB 		= BIT(9),
	TX_UseRate			= BIT(8),
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
	TXdesc_92E_DataRateFBLmtSHIFT	= 8,
	TXdesc_92E_DataRateFBLmtMask	= 0x01F,
#endif		
	TX_HwSeqEn			= BIT(7),
	TX_QOS				= BIT(6),
#ifdef CONFIG_RTL_88E_SUPPORT
	TXdesc_88E_HwSeqSelSHIFT       = 6,
	TXdesc_88E_HwSeqSelMask        = 0x03,
#endif
	TX_ApDCFE			= BIT(5),
	TX_RtsRateSHIFT	= 0,
	TX_RtsRateMask		= 0x01F,
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
	TX_RtsRateSHIFT_8812	= 24,
	TX_RtsRateMask_8812		= 0x01F,
	TXdesc_92E_DataRateSHIFT		= 0,
	TXdesc_92E_DataRateMask		= 0x7F,
#endif	
//	TX_RTSRateFBLmtSHIFT	= 7,
//	TX_RTSRateFBLmtMask	= 0xF,
//	TX_RaBRSRIDSHIFT		= 13,	//Rate adaptive BRSR ID.
//	TX_RaBRSRIDMask		= 0x7, // 3bits
//	TX_TXHT					= BIT(16),
//	TX_RD					= BIT(23),
//	TX_RTSHT				= BIT(24),
//	TX_RTSSTBCSHIFT		= 29,
//	TX_RTSSTBCMask		= 0x3,


	// DWORD 5
	TX_UsbAggNumSHIFT	= 24,
	TX_UsbAggNumMask		= 0x0FF,
	TX_DataRtyLmtSHIFT		= 18,
	TX_DataRtyLmtMask		= 0x03F,
	TX_RtyLmtEn			= BIT(17),
	TX_RtsRateFBLmtSHIFT	= 13,
	TX_RtsRateFBLmtMask	= 0x0F,
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
	TXdesc_92E_RtsScSHIFT		= 13,
	TXdesc_92E_RtsScMask		= 0xf,
	TXdesc_92E_RtsShort			= BIT(12),
	TXdesc_92E_DataLDPCSHIFT	= 7,	
	TXdesc_92E_DataStbcSHIFT	= 8,
	TXdesc_92E_DataStbcMask		= 0x03,
#endif		
	TX_DataRateFBLmtSHIFT	= 8,
	TX_DataRateFBLmtMask	= 0x01F,
	TX_CcxTag				= BIT(7),
#ifdef CONFIG_RTL_88E_SUPPORT
	TXdesc_88E_TryRate		= BIT(7), 
#endif
	TX_SGI					= BIT(6),
	TX_DataRateSHIFT		= 0,
	TX_DataRateMask		= 0x03F,

#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
	TXdesc_92E_DataBwSHIFT		= 5,
	TXdesc_92E_DataBwMask		= 0x03,
	TXdesc_92E_DataShort		= BIT(4),
	TXdesc_92E_DataScSHIFT		= 0,
	TXdesc_92E_DataScMask		= 0x0f,
	TXdesc_92E_DataLDPC			= BIT(7),
	DATA_RATE_FB_LIMIT = 8,
	RTS_RATE_FB_LIMIT = 13,

	TXdesc_8812_TxPwrOffetSHIFT	= 28,
	TXdesc_8812_TxPwrOffetMask 	= 0x7,
#endif

//	TX_PktIDSHIFT		= 0,
//	TX_PktIDMask		= 0x1FF,
//	TX_DISFB			= BIT(15),
//	TX_TxAGCSHIFT			= 21,
//	TX_TxAGCMask			= 0x7FF,


	// DWORD 6
	TX_MCS7SgiMaxSHIFT	= 28,
	TX_MCS7SgiMaxMask		= 0x0F,
	TX_MCS3gMaxSHIFT		= 24,
	TX_MCS3gMaxMask		= 0x0F,
	TX_MCS2gMaxSHIFT		= 20,
	TX_MCS2gMaxMask		= 0x0F,
	TX_MCS1gMaxSHIFT		= 16,
	TX_MCS1gMaxMask		= 0x0F,
	TX_MaxAggNumSHIFT	= 11,
	TX_MaxAggNumMask		= 0x01F,
	TX_UseMaxLen			= BIT(10),
	TX_TxAgcBSHIFT			= 5,
	TX_TxAgcBMask			= 0x01F,
	TX_TxAgcASHIFT		= 0,
	TX_TxAgcAMask			= 0x01F,

//	TX_IPChkSumSHIFT		=  0,
//	TX_IPChkSumMask		=  0xFFFF,
//	TX_TCPChkSumSHIFT		=	16,
//	TX_TCPChkSumMask		=	0xFFFF,


	// DWORD 7
#ifdef CONFIG_RTL_88E_SUPPORT
	TXdesc_88E_Null1		= BIT(31), 
	TXdesc_88E_Null0		= BIT(30), 
	TXdesc_88E_AntSelC		= BIT(29), 
	TXdesc_88E_SwOffset31SHIFT	= 27,
	TXdesc_88E_SwOffset31Mask		= 0x0F,
	TXdesc_88E_SwOffset30SHIFT	= 23,
	TXdesc_88E_SwOffset30Mask		= 0x0F,
	TXdesc_88E_SwOffset29SHIFT	= 19,
	TXdesc_88E_SwOffset29Mask		= 0x0F,
#endif
	TX_MCS15SgiMaxSHIFT	= 28,
	TX_MCS15SgiMaxMask	= 0x0F,
	TX_MCS6gMaxSHIFT		= 24,
	TX_MCS6gMaxMask		= 0x0F,
	TX_MCS5gMaxSHIFT		= 20,
	TX_MCS5gMaxMask		= 0x0F,
	TX_MCS4gMaxSHIFT		= 16,
	TX_MCS4gMaxMask		= 0x0F,
	TX_TxBufSizeSHIFT		= 0,
	TX_TxBufSizeMask		= 0x0FFFF,

//	TX_IPHdrOffsetSHIFT	= 16,
//	TX_IPHdrOffsetMask		= 0xFF,
//	unsigned int		Rsvd2:7;
//	TX_TCPEn				= BIT(31),
	
	// DWORD 9
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
	TXdesc_92E_TX_SeqSHIFT       = 12,
	TXdesc_92E_TX_SeqMask        = 0x0FFF,
	TXdesc_92E_TX_GroupIEEnable  = BIT(7),
	TXdesc_92E_TX_GroupIESHIFT   = 0,
	TXdesc_92E_TX_GroupIEMask    = 0xFF,
#endif
};


enum _RTL8192CD_RX_DESC_ {
	// RX cmd desc
	// Dword 0
	RX_OWN					= BIT(31),
	RX_EOR					= BIT(30),
	RX_FirstSeg				= BIT(29),
	RX_LastSeg				= BIT(28),
	RX_SwDec				= BIT(27),
	RX_PHYST				= BIT(26),
	RX_ShiftSHIFT			= 24,
	RX_ShiftMask			= 0x03,
	RX_QOS					= BIT(23),
	RX_SecuritySHIFT		= 20,
	RX_SecurityMask			= 0x07,
	RX_DrvInfoSizeSHIFT	= 16,
	RX_DrvInfoSizeMask		= 0x0F,
	RX_ICVERR				= BIT(15),
	RX_CRC32				= BIT(14),
#if defined(CONFIG_RTL_8812_SUPPORT)
	RXdesc_92E_FirstSeg		= BIT(15),
	RXdesc_92E_LastSeg		= BIT(14),
#endif	
	RX_PktLenSHIFT			= 0,
	RX_PktLenMask 			= 0x03FFF,


	// Dword 1
	RX_BC				= BIT(31),
	RX_MC				= BIT(30),
	RX_TypeSHIFT		= 28,
	RX_TypeMask		= 0x03,
	RX_MoreFrag		= BIT(27),
	RX_MoreData		= BIT(26),
	RX_PWRMGT			= BIT(25),
	RX_PAM				= BIT(24),
	RX_A2FitSHIFT		= 20,
	RX_A2FitSHIFMask	= 0x0F,
	RX_A1FitSHIFT		= 16,
	RX_A1FitSHIFMask	= 0x0F,
	RX_FAGGR			= BIT(15),
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
	RXdesc_92E_PAGGR	= BIT(15),
#endif	
	RX_PAGGR			= BIT(14),
	RX_AMSDU			= BIT(13),
	RX_HwRsvdSHIFT	= 9,
	RX_HwRsvdMask		= 0x0F,
#ifdef CONFIG_RTL_88E_SUPPORT
	RXdesc_88E_TidSHIFT	= 8,
#endif
	RX_TidSHIFT			= 5,
	RX_TidMask			= 0x0F,
	RX_MacIdSHIFT		= 0,
	RX_MacIdMask		= 0x01F,
#ifdef CONFIG_RTL_88E_SUPPORT
	RXdesc_88E_MacIdMask	= 0x03F,
#endif


	// DWORD 2
	RX_NextInd				= BIT(30),
	RX_NextPktLenSHIFT		= 16,
#ifdef CONFIG_RTL_88E_SUPPORT
	RXdesc_88E_NextPktLenMask       = 0x0FF,
#endif
	RX_NextPktLenMask		= 0x03FFF,
	RX_FragSHIFT			= 12,
	RX_FragMask			= 0x0F,
	RX_SeqSHIFT			= 0,
	RX_SeqMask				= 0x0FFF,


	//DWORD3
#ifdef CONFIG_RTL_88E_SUPPORT
	RXdesc_88E_MagicWake	= BIT(31),
	RXdesc_88E_UCwake		= BIT(30),
	RXdesc_88E_PtnMatch	= BIT(29),
#endif
	RX_IV0SHIFT		= 16,
	RX_IV0Mask			= 0x0FFFF,
#ifdef CONFIG_RTL_88E_SUPPORT
	RXdesc_88E_RptSelSHIFT		= 14,
	RXdesc_88E_RptSelMask		= 0x03,
#endif
	RX_HwPCInd			= BIT(15),
	RX_HwPCErr			= BIT(14),
	RX_TCPChkValid		= BIT(13),
	RX_IPChkRpt			= BIT(12),
	RX_TCPChkRpt		= BIT(11),
	RX_HTC				= BIT(10),
	RX_BW				= BIT(9),
	RX_SPLCP			= BIT(8),
	RX_GF				= BIT(7),
	RX_RxHT			= BIT(6),
	RX_RxMcsSHIFT		= 0,
	RX_RxMcsMask		= 0x03F,


	//DWORD8
	RX_BaVld			= BIT(11),
	RX_BaSsnSHIFT		= 0,
	RX_BaSsnMask		= 0x07FF,
};



#ifdef CONFIG_PCI_HCI
static __inline__ int get_txhead(struct rtl8192cd_hw *phw, int q_num)
{
	return *(int *)((unsigned long)&(phw->txhead0) + sizeof(int)*q_num);
}

static __inline__ int get_txtail(struct rtl8192cd_hw *phw, int q_num)
{
	return *(int *)((unsigned long)&(phw->txtail0) + sizeof(int)*q_num);
}

static __inline__ int *get_txhead_addr(struct rtl8192cd_hw *phw, int q_num)
{
	return (int *)((unsigned long)&(phw->txhead0) + sizeof(int)*q_num);
}

static __inline__ int *get_txtail_addr(struct rtl8192cd_hw *phw, int q_num)
{
	return (int *)((unsigned long)&(phw->txtail0) + sizeof(int)*q_num);
}

static __inline__ struct tx_desc *get_txdesc(struct rtl8192cd_hw *phw, int q_num)
{
#if defined(__ECOS)
	return (struct tx_desc *)(*(unsigned int *)((unsigned long)&(phw->tx_desc0) + sizeof(struct tx_desc *)*q_num));
#else
	return (struct tx_desc *)(*(dma_addr_t*)((unsigned long)&(phw->tx_desc0) + sizeof(struct tx_desc *)*q_num));
#endif
}


#define TXDESC_HIGH_QUEUE_NUM   5
#define TXDESC_HIGH_QUEUE_NUM_V1   15
#define TXDESC_HIGH_QUEUE_NUM_PEOFLD 1

static __inline__ struct tx_desc_info *get_txdesc_info(struct rtl8192cd_priv* Adapter, struct rtl8192cd_tx_desc_info*pdesc, int q_num)
{
    // code below that depend on structure of rtl8192cd_tx_desc_info
    unsigned int NumOfOffsetBytes;
    unsigned int high_queue_num;
#if 0 //tmp,compile issue, yllinPEOFLD   
#if defined(CONFIG_WLAN_HAL_8814BE) && !defined(CONFIG_PE_ENABLE) 
    if(IS_HARDWARE_TYPE_8814BE(Adapter))
        high_queue_num = TXDESC_HIGH_QUEUE_NUM_V1;
    else
#endif
#if defined(CONFIG_WLAN_HAL_8814BE) && defined(CONFIG_PE_ENABLE) 
    if(IS_HARDWARE_TYPE_8814BE(Adapter))
        high_queue_num = TXDESC_HIGH_QUEUE_NUM_PEOFLD;
    else
#endif
#endif
        high_queue_num = TXDESC_HIGH_QUEUE_NUM;

    if (q_num <= 2) {
        NumOfOffsetBytes = sizeof(struct tx_desc_info)*q_num*NONBE_TXDESC;
    } else if (q_num <= 4) {
        NumOfOffsetBytes = sizeof(struct tx_desc_info)*(BE_TXDESC + (q_num-1)*NONBE_TXDESC);
    } else {
		if (q_num >= 15)
			high_queue_num = TXDESC_HIGH_QUEUE_NUM_V1;
        NumOfOffsetBytes = sizeof(struct tx_desc_info)*(BE_TXDESC + (4*NONBE_TXDESC) + (q_num-high_queue_num)*NUM_TX_DESC_HQ);
    }

    return (struct tx_desc_info *)((unsigned long)(pdesc->tx_info0) + NumOfOffsetBytes);
}


#if CONFIG_WLAN_NOT_HAL_EXIST
static __inline__ unsigned long *get_txdma_addr(struct rtl8192cd_hw *phw, int q_num)
{
#ifdef OSK_LOW_TX_DESC
	if (q_num<=2) {
		return (unsigned long *)((unsigned long)&(phw->tx_desc0_dma_addr) + (sizeof(unsigned long)*q_num*NONBE_TXDESC));
	} else if (q_num < TXDESC_HIGH_QUEUE_NUM) {
		return (unsigned long *)((unsigned long)&(phw->tx_desc0_dma_addr) + (sizeof(unsigned long)*(q_num-1)*NONBE_TXDESC)
				+(sizeof(unsigned long)*BE_TXDESC));
    } else {
		return (unsigned long *)((unsigned long)&(phw->tx_desc0_dma_addr) + (sizeof(unsigned long)*(q_num-1)*NONBE_TXDESC)
				+ (sizeof(unsigned long)*BE_TXDESC) + (sizeof(unsigned long)*(q_num-TXDESC_HIGH_QUEUE_NUM) * NUM_TX_DESC_HQ));
	}
#else
	if (q_num < TXDESC_HIGH_QUEUE_NUM) {
	return (unsigned long *)((unsigned long)&(phw->tx_desc0_dma_addr) + (sizeof(unsigned long)*q_num*NUM_TX_DESC));
	} else {
		return (unsigned long *)((unsigned long)&(phw->tx_desc0_dma_addr) + (sizeof(unsigned long)*q_num*NUM_TX_DESC)
		    + (sizeof(unsigned long)*(q_num-TXDESC_HIGH_QUEUE_NUM) * NUM_TX_DESC_HQ));
	}
#endif  //OSK_LOW_TX_DESC
}
#endif

#endif // CONFIG_PCI_HCI



#endif
