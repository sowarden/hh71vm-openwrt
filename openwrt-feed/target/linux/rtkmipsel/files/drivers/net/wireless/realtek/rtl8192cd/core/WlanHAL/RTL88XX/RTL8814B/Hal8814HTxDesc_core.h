/**
  *  @file Hal88XXHTxDesc.h  : Data structure of HTXBD/HTxDMAInfo/TxPktInfo  (Use 8814A to simulation)
  *  @brief Handle HTXBD/TXdESC/TxPktInfo of Tx-processes in HostCPU
  *
  *  Packet Offload Engine will help RTK WiFi Chip to decrease host platform CPU utilization.
  *  This functon will handle Tx-processes in Host-CPU
  *
  *  Copyright (c) 2015 Realtek Semiconductor Corp.
  *
  *  @author Peter Yu
  *  @date 2015/11/4
 **/

#ifndef _HAL88XXHTXDESC_CORE_H
#define _HAL88XXHTXDESC_CORE_H
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
typedef enum _HCI_TX_DMA_QUEUE_OFLD_
{
    HCI_TXDMA_CHAN_00 = 0,
    HCI_TXDMA_CHAN_01,
    HCI_TXDMA_CHAN_02,
    HCI_TXDMA_CHAN_03,
    HCI_TXDMA_CHAN_04,
    HCI_TXDMA_CHAN_05,
    HCI_TXDMA_CHAN_06,
    HCI_TXDMA_CHAN_07,
    HCI_TXDMA_CHAN_08,
    HCI_TXDMA_CHAN_09,
    HCI_TXDMA_CHAN_10,
    HCI_TXDMA_CHAN_11,
    HCI_TXDMA_CHAN_S0,		/* 12 */
    HCI_TXDMA_CHAN_S1,
    HCI_TXDMA_CHAN_MGT,		/* 14 */
    HCI_TXDMA_CHAN_BCN,		/* 15 */
    HCI_TXDMA_CHAN_CMD,		/* 16 */
    HCI_TXDMA_CHAN_HI0,  	/* 17 */
    HCI_TXDMA_CHAN_HI1,
    HCI_TXDMA_CHAN_HI2,
    HCI_TXDMA_CHAN_HI3,
    HCI_TXDMA_CHAN_HI4,
    HCI_TXDMA_CHAN_HI5,
    HCI_TXDMA_CHAN_HI6,
    HCI_TXDMA_CHAN_HI7,
    HCI_TXDMA_CHAN_HI8,
    HCI_TXDMA_CHAN_HI9,
    HCI_TXDMA_CHAN_HI10,
    HCI_TXDMA_CHAN_HI11,
    HCI_TXDMA_CHAN_HI12,
    HCI_TXDMA_CHAN_HI13,
    HCI_TXDMA_CHAN_HI14,
    HCI_TXDMA_CHAN_HI15,
    HCI_TXDMA_CHAN_HI16,
    HCI_TXDMA_CHAN_HI17,
    HCI_TXDMA_CHAN_HI18,
    HCI_TXDMA_CHAN_HI19,

    /* keep last */
    HCI_TXDMA_CHAN_MAX_NUM	/* 37 */
} HCI_TX_DMA_QUEUE_OFLD, *PHCI_TX_DMA_QUEUE_OFLD;
#if 0 //redefine will effect other chip
#define HCI_TX_DMA_QUEUE_BE		HCI_TXDMA_CHAN_00
#define HCI_TX_DMA_QUEUE_BK		HCI_TXDMA_CHAN_01
#define HCI_TX_DMA_QUEUE_VO		HCI_TXDMA_CHAN_02
#define HCI_TX_DMA_QUEUE_VI		HCI_TXDMA_CHAN_03
#define HCI_TX_DMA_QUEUE_MGT	HCI_TXDMA_CHAN_MGT
#define HCI_TX_DMA_QUEUE_CMD	HCI_TXDMA_CHAN_CMD
#define HCI_TX_DMA_QUEUE_BCN	HCI_TXDMA_CHAN_MGT
#endif
#define H2D_TX_DMA_QUEUE_MAX_NUM    HCI_TXDMA_CHAN_MAX_NUM
#define HTXBD_ELE_NUM               4
#endif
/** @brief TxBD Element used for Host-CPU fill to Data-CPU
  *  Dword0: OWN(BIT31)    RSVD(BIT24~30)    PSB(BIT16~23)     TXBUFFSIZE(BIT0~15)
  *  Dword1: Physical address Low
  *  Dword2: Physical address high
  *  Dword3: RSVD
 **/
typedef struct _H2D_TXBD_ELEMENT_
{
    u4Byte              Dword0;
    u4Byte              Dword1;
#if TXBD_SEG_32_64_SEL
    u4Byte              Dword2;
    u4Byte              Dword3;
#endif  //TXBD_SEG_32_64_SEL
} H2D_TXBD_ELEMENT,*PH2D_TXBD_ELEMENT;


/* TX Buffer Descriptor*/
//Dword 0 MSK
#define H2D_TXBD_DW0_TXBUFSIZE_MSK      0xFFFF

//---Normal Packet
#define H2D_TXBD_DW0_PSLEN_MSK          0xFFFF
//---Beacon Packet
#define H2D_TXBD_DW0_BCN_PSLEN_MSK      0x7FFF
#define H2D_TXBD_DW0_BCN_OWN_MSK        0x1

#define H2D_TXBD_DW0_EXTENDTXBUF_MSK    0x1

//Dword 0 SHIFT
#define H2D_TXBD_DW0_TXBUFSIZE_SH       0

//---Normal Packet
#define H2D_TXBD_DW0_PSLEN_SH           16

//---Beacon Packet
#define H2D_TXBD_DW0_BCN_PSLEN_SH       16
#define H2D_TXBD_DW0_BCN_OWN_SH         31

#define	H2D_TXBD_DW0_EXTENDTXBUF_SH     31


//Dword 1 MSK
#define H2D_TXBD_DW1_PHYADDR_LOW_MSK    0xFFFFFFFF
//Dword 1 SHIFT
#define H2D_TXBD_DW1_PHYADDR_LOW_SH     0


//Dword 2 MSK
#define H2D_TXBD_DW2_PHYADDR_HIGH_MSK   0xFFFFFFFF
//Dword 2 SHIFT
#define H2D_TXBD_DW2_PHYADDR_HIGH_SH    0


//Dword 3 MSK
#define H2D_TXBD_DW3_PHYADDR_RSVD_MSK   0xFFFFFFFF
//Dword 3 SHIFT
#define H2D_TXBD_DW3_PHYADDR_RSVD_SH    0
/**/


#define MAX_TXIE_NUMBER     (6)
struct h2d_txie_t
{
	u4Byte		dw0;
	u4Byte		dw1;
};

/** @brief _TXDMA_INFO_(H2D_Txdesc) Element used for Host-CPU fill to Data-CPU
  *  Dword0: IE_END(BIT31)          AGG_EN(BIT30)       BK(BIT29)           PKT_OFFSET(BIT24~28)
  *          OFFSET(BIT16~23)       TXPKTSIZE(BIT0~15)
  *  Dword1: AMSDU(BIT31)           PHY_SEL(BIT30)      MORE_IEBODY(BIT29)  CHANNEL_DMA(BIT24~28)
  *          SMH_EN(BIT23)	        MHR_CP(BIT22)       HW_AES_IV(BIT21)    WHEADER_LEN(BIT16~20)
  *          RSVD(BIT13~15)         QSEL(BIT8~12)       MACID(BIT0~7)
  *  Dword2: CHK_EN(BIT31)	        DMA_PRI(BIT29)      RSVD(BIT24~29)      DMA_TXAGG_NUM/FINAL_DATA_RATE(BIT16~23)
  *          TXDESC_CHECKSUM(PCIe/USB/SDIO)/TIMESTAMP(MAC) : BIT0~16
  *  Dword3: RSVD(BIT31)              OFFSE_SIZE(BIT16~30)      
  *          SHM_CAM(BIT8~15)         RSVD(BIT0~7)
 **/
typedef struct _H2D_TXDMA_INFO_
{
    u4Byte              Dword0;
    u4Byte              Dword1;
    u4Byte              Dword2;
    u4Byte              Dword3;
	struct h2d_txie_t   txie[MAX_TXIE_NUMBER];
} H2D_TXDMA_INFO,*PH2D_TXDMA_INFO;

//TXDMA-Info Dword 0 MSK
#define H2D_TXDMAINFO_DW0_TXPKTSIZE_MSK         0xFFFF
#define H2D_TXDMAINFO_DW0_OFFSET_MSK            0xFF
#define H2D_TXDMAINFO_DW0_PKT_OFFSET_MSK        0x1F
#define H2D_TXDMAINFO_DW0_BK_MSK                0x1
#define H2D_TXDMAINFO_DW0_AGG_EN_MSK            0x1
#define H2D_TXDMAINFO_DW0_IE_END_MSK            0x1

//TXDMA-Info Dword 0 SHIFT
#define H2D_TXDMAINFO_DW0_TXPKTSIZE_SH          0
#define H2D_TXDMAINFO_DW0_OFFSET_SH             16
#define H2D_TXDMAINFO_DW0_PKT_OFFSET_SH         24
#define H2D_TXDMAINFO_DW0_BK_SH                 29
#define H2D_TXDMAINFO_DW0_AGG_EN_SH             30
#define H2D_TXDMAINFO_DW0_IE_END_SH             31

//TXDMA-Info Dword 1 MSK
#define H2D_TXDMAINFO_DW1_MACID_MSK             0xFF
#define H2D_TXDMAINFO_DW1_QSEL_MSK              0x1F
#define H2D_TXDMAINFO_DW1_RSVD0_MSK             0x3
#define H2D_TXDMAINFO_DW1_WHEADER_LEN_MSK       0x1F
#define H2D_TXDMAINFO_DW1_HW_AES_IV_MSK         0x1
#define H2D_TXDMAINFO_DW1_HW_MHR_CP_MSK         0x1
#define H2D_TXDMAINFO_DW1_HW_SMH_EN_MSK         0x1
#define H2D_TXDMAINFO_DW1_CHANNEL_DMA_MSK       0x1F
#define H2D_TXDMAINFO_DW1_MORE_IEBODY_MSK       0x1
#define H2D_TXDMAINFO_DW1_HW_PHY_SEl_MSK        0x1
#define H2D_TXDMAINFO_DW1_HW_AMSDU_MSK          0x1

//TXDMA-Info Dword 1 SHIFT
#define H2D_TXDMAINFO_DW1_MACID_SH              0
#define H2D_TXDMAINFO_DW1_QSEL_SH               8
#define H2D_TXDMAINFO_DW1_RSVD0_SH              13
#define H2D_TXDMAINFO_DW1_WHEADER_LEN_SH        16
#define H2D_TXDMAINFO_DW1_HW_AES_IV_SH          21
#define H2D_TXDMAINFO_DW1_HW_MHR_CP_SH          22
#define H2D_TXDMAINFO_DW1_HW_SMH_EN_SH          23
#define H2D_TXDMAINFO_DW1_CHANNEL_DMA_SH        24
#define H2D_TXDMAINFO_DW1_MORE_IEBODY_SH        29
#define H2D_TXDMAINFO_DW1_HW_PHY_SEl_SH         30
#define H2D_TXDMAINFO_DW1_HW_AMSDU_SH           31

//TXDMA-Info Dword 2 MSK
#define H2D_TXDMAINFO_DW2_TXDESC_CKSUM_MSK      0xFFFF  //TXDESC_CHECKSUM (PCIe/USB/SDIO)/TIMESTAMP (MAC)
#define H2D_TXDMAINFO_DW2_DMA_TXAGGNUM_MSK      0xFF    //DMA_TXAGG_NUM/FINAL_DATA_RATE
#define H2D_TXDMAINFO_DW2_RSVD0_MSK             0x3F    //RESERVED
#define H2D_TXDMAINFO_DW2_DMA_PRI_MSK           0x1     //DMA_PRI
#define H2D_TXDMAINFO_DW2_CHK_EN_MSK            0x1     //CHECK ENABLE

//TXDMA-Info Dword 2 SHIFT
#define H2D_TXDMAINFO_DW2_TXDESC_CKSUM_SH       0
#define H2D_TXDMAINFO_DW2_DMA_TXAGGNUM_SH       16
#define H2D_TXDMAINFO_DW2_RSVD0_SH              24
#define H2D_TXDMAINFO_DW2_DMA_PRI_SH            30
#define H2D_TXDMAINFO_DW2_CHK_EN_SH             31

/** @brief _TXDMA_INFO_(H2D_Txdesc) Element used for Host-CPU fill to Data-CPU
  *  Dword3: RSVD(BIT31)              OFFSE_SIZE(BIT16~30)      
  *          SHM_CAM(BIT8~15)         RSVD(BIT0~7)
 **/

//TXDMA-Info Dword 3 MSK
#define H2D_TXDMAINFO_DW3_RSVD0_MSK             0xFF    //RSVD
#define H2D_TXDMAINFO_DW3_SHM_CAM_MSK           0xFF
#define H2D_TXDMAINFO_DW3_OFFLOAD_SIZE_MSK      0x7FFF
#define H2D_TXDMAINFO_DW3_RSVD1_MSK             0x1

//TXDMA-Info Dword 3 SHIFT
#define H2D_TXDMAINFO_DW3_RSVD0_SH              0
#define H2D_TXDMAINFO_DW3_SHM_CAM_SH            8
#define H2D_TXDMAINFO_DW3_OFFLOAD_SIZE_SH       16
#define H2D_TXDMAINFO_DW3_RSVD1_SH              31
  
typedef struct _H2D_TXDMA_INFO_DATA_
{
#ifdef _LITTLE_ENDIAN_
    u4Byte      dw0_txpktsize:16;
    u4Byte      dw0_offset:8;
    u4Byte      dw0_pkt_offset:5;
    u4Byte      dw0_bk:1;
    u4Byte      dw0_agg_en:1;
    u4Byte      dw0_ie_end:1;

    u4Byte      dw1_macid:8;
    u4Byte      dw1_qsel:5;
    u4Byte      dw1_ext_edca:1;
    u4Byte      dw1_rsvd0:2;
    u4Byte      dw1_shm_cam:8;
    u4Byte      dw1_shm_en:1;
    u4Byte      dw1_mhr_cp:1;
    u4Byte      dw1_rsvd1:3;
    u4Byte      dw1_hw_aes_iv:1;
    u4Byte      dw1_amsdu:1;
    u4Byte      dw1_rsvd2:1;

    u4Byte      dw2_txdesc_checksum:16;
    u4Byte      dw2_dma_txagg_num:8;
    u4Byte      dw2_max_amsdu_mode:3;
    u4Byte      dw2_dma_pri:1;
    u4Byte      dw2_rsvd0:3;
    u4Byte      dw2_chk_en:1;

    u4Byte      dw3_wheader_len:5;
    u4Byte      dw3_ie_cnt_en:1;
    u4Byte      dw3_ie_cnt:3;
    u4Byte      dw3_rsvd0:2;
    u4Byte      dw3_dma_channel:5;
    u4Byte      dw3_offload_size:15;
    u4Byte      dw3_rsvd1:1;
#else
    u4Byte      dw0_ie_end:1;
    u4Byte      dw0_agg_en:1;
    u4Byte      dw0_bk:1;
    u4Byte      dw0_pkt_offset:5;
    u4Byte      dw0_offset:8;
    u4Byte      dw0_txpktsize:16;

    u4Byte      dw1_rsvd2:1;
    u4Byte      dw1_amsdu:1;
    u4Byte      dw1_hw_aes_iv:1;
    u4Byte      dw1_rsvd1:3;
    u4Byte      dw1_mhr_cp:1;
    u4Byte      dw1_shm_en:1;
    u4Byte      dw1_shm_cam:8;
    u4Byte      dw1_rsvd0:2;
    u4Byte      dw1_ext_edca:1;
    u4Byte      dw1_qsel:5;
    u4Byte      dw1_macid:8;

    u4Byte      dw2_chk_en:1;
    u4Byte      dw2_rsvd0:3;
    u4Byte      dw2_dma_pri:1;
    u4Byte      dw2_max_amsdu_mode:3;
    u4Byte      dw2_dma_txagg_num:8;
    u4Byte      dw2_txdesc_checksum:16;

    u4Byte      dw3_rsvd1:1;
    u4Byte      dw3_offload_size:15;
    u4Byte      dw3_dma_channel:5;
    u4Byte      dw3_rsvd0:2;
    u4Byte      dw3_ie_cnt:3;
    u4Byte      dw3_ie_cnt_en:1;
    u4Byte      dw3_wheader_len:5;
#endif
	struct h2d_txie_t txie[MAX_TXIE_NUMBER];
} H2D_TXDMA_INFO_DATA, *PH2D_TXDMA_INFO_DATA;

/* End Data-CPU TX-DMA-Info structure */



__IRAM_IN_865X BOOLEAN FillH2D_TxHwCtrl88XX(
    IN      HAL_PADAPTER    Adapter,
    IN      u4Byte          queueIndex,
    IN      PVOID           pDescData
);

struct pci_dma_register_t {
	u32		chan_id;
	u32		desc_reg_addr;
	u32		idx_reg_addr;
	u32		num_reg_addr;
};

#endif //#ifndef _HAL88XXHTXDESC_H
