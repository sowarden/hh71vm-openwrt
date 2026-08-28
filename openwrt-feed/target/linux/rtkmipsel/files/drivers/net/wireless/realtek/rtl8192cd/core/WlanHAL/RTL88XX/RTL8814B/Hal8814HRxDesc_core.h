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

#ifndef _HAL88XXHRXDESC_CORE_H
#define _HAL88XXHRXDESC_CORE_H

/** @brief d2h_rxdma_info
  *
 **/
struct d2h_rxdma_info
{
	u32 word0;
	u32 word1;
};

HAL_IMEM enum rt_status QueryD2H_RxDesc88XX (
    IN      HAL_PADAPTER    Adapter,
    IN      u4Byte          queueIndex,  //HCI_RX_DMA_QUEUE_88XX
    IN      pu1Byte         pBufAddr,
    OUT     PVOID           pRxDescStatus
);

typedef enum _HCI_RX_DMA_QUEUE_OFLD_ {
    HCI_RXDMA_CHAN,

    /* keep last */
    HCI_RXDMA_CHAN_MAX_NUM
} HCI_RX_DMA_QUEUE_OFLD, *PHCI_RX_DMA_QUEUE_OFLD;

#endif //#ifndef _HAL88XXHTXDESC_H
