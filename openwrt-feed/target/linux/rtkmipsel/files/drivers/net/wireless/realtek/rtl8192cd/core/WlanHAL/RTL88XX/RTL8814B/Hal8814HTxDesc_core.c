/**
  *  @file Hal88XXHTxDesc.c  : Fill HTXBD/TXdESC/TxPktInfo  (Use 8814A to simulation)
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

#if !defined(__ECOS) && !defined(CPTCFG_CFG80211_MODULE)
#include "HalPrecomp.h"
#else
#include "../HalPrecomp.h"
#endif

#include "Hal8814HTxDesc_core.h"

#ifdef CONFIG_RTL_PROC_NEW
#define PROC_PRINT(fmt, arg...)	seq_printf(s, fmt, ## arg)
#else
#define PROC_PRINT	printk
#endif

#if (HAL_DEV_BUS_TYPE & (HAL_RT_EMBEDDED_INTERFACE | HAL_RT_PCI_INTERFACE))

/** @brief -- use the prepared HTXBD/HTXDesc/pktinfo to fill HTXBD/TXdESC/TxPktInfo.
  * @param param_out None
  * @param param_in  Adapter:       which Wi-Fi interface.
  *                  queueIndex:    if host need to parse tid, need this
  *                  pDescData:     prepare txdesc/txpktinfo
  * @return 1: OK, 0: Failed
  * 
 **/
__IRAM_IN_865X BOOLEAN FillH2D_TxHwCtrl88XX(
    IN      HAL_PADAPTER    Adapter,
    IN      u4Byte          queueIndex,
    IN      PVOID           pDescData
)
{
    PHCI_TX_DMA_MANAGER_88XX         ph2d_tx_dma = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);
    PTX_DESC_DATA_88XX       ph2d_desc_data = (PTX_DESC_DATA_88XX)(pDescData);
    PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q = &(ph2d_tx_dma->tx_queue[queueIndex]);
    PTX_DESC_88XX             ptx_desc = ((PTX_DESC_88XX)(cur_q->ptx_desc_head)) + cur_q->host_idx;

    //fillHTXDesc
#ifdef _BIG_ENDIAN_
{
    int i, *src, *dst;
    src = (int *)ph2d_desc_data->ph2d_txdma_info;
    dst = (int *)ptx_desc;
    for (i=0; i<4; i++) /* TxDMAInfo */
        *dst++ = cpu_to_le32(*src++);
    for (i=0; i<12; i++) /* TXIEs */
        *dst++ = *src++;
}
#else
    memcpy((u1Byte *)ptx_desc, (u1Byte *)ph2d_desc_data->ph2d_txdma_info, sizeof(H2D_TXDMA_INFO));
#endif
}

#endif //#if (HAL_DEV_BUS_TYPE & (HAL_RT_EMBEDDED_INTERFACE | HAL_RT_PCI_INTERFACE))

