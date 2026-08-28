/**
  *  @file Hal88XXHRxDesc.c  : Fill HTXBD/TXdESC/TxPktInfo  (Use 8814A to simulation)
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

#include "Hal8814HRxDesc_core.h"
#include "Hal8814HTxDesc_core.h"

typedef void (*INIT_RXBUF_FUNC)(HAL_PADAPTER Adapter, PVOID pSkb, u2Byte rxbd_idx, pu4Byte pBufAddr, pu4Byte pBufLen);

#ifdef CONFIG_RTL_PROC_NEW
#define PROC_PRINT(fmt, arg...)	seq_printf(s, fmt, ## arg)
#else
#define PROC_PRINT	printk
#endif

static const struct pci_dma_register_t pci_rxdma_reg_map[] = {
	{HCI_RXDMA_CHAN, REG_P0RXQ_RXBD_DESA_L_8814B, REG_P0RXQ_RXBD_IDX_8814B, (REG_P0MGQ_RXQ_TXRXBD_NUM + 2)},
};

#define RXBD_RXTAG_POLLING_CNT  100
#define RXBD_RXTAG_MASK         0x1FFF

#if (HAL_DEV_BUS_TYPE & (HAL_RT_EMBEDDED_INTERFACE | HAL_RT_PCI_INTERFACE))
HAL_IMEM enum rt_status QueryD2H_RxDesc88XX (
    IN      HAL_PADAPTER    Adapter,
    IN      u4Byte          queueIndex,
    IN      pu1Byte         pBufAddr,
    OUT     PVOID           pRxDescStatus
)
{
    BOOLEAN                     bResult = SUCCESS;
    u4Byte                      PollingCnt = 0;
    PHCI_RX_DMA_MANAGER_88XX         pd2h_rx_dma;
    PHCI_RX_DMA_QUEUE_STRUCT_88XX           cur_q;
    PRX_DESC_STATUS_88XX        prx_desc_status;
    struct d2h_rxpkt_info       *prx_pktinfo;
    PRX_DESC_88XX               prx_desc;
    PRX_BUFFER_DESCRIPTOR       prxbd;
    u4Byte                      RXBDDword0;
    u4Byte                      pktinfo_offset;
    u4Byte                      desc_offset;

    pd2h_rx_dma = (PHCI_RX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PRxDMA88XX);
    cur_q = &(pd2h_rx_dma->rx_queue[queueIndex]);
    prxbd = &cur_q->pRXBD_head[cur_q->cur_host_idx];
    prx_desc_status = (PRX_DESC_STATUS_88XX)pRxDescStatus;

#ifdef CONFIG_PE_ENABLE	//TBD with YL 
    if(Adapter->pshare->rf_ft_var.manual_pe_enable){
        //do nothing
    }else
#endif        
    {

#ifdef CONFIG_NET_PCI
	unsigned long rxbd_dma_addr = cur_q->rxbd_dma_addr + sizeof(RX_BUFFER_DESCRIPTOR)*cur_q->cur_host_idx;
#endif
	
	do {
#ifdef CONFIG_NET_PCI
		if (HAL_IS_PCIBIOS_TYPE(Adapter)) {
			HAL_CACHE_SYNC_WBACK(Adapter, rxbd_dma_addr, sizeof(RX_BUFFER_DESCRIPTOR), HAL_PCI_DMA_TODEVICE);
		}
#endif
	
#if RXBD_READY_CHECK_METHOD
		//RXBDDword0 = nonCacheAddr->Dword0;
		RXBDDword0 = prxbd->Dword0;
	
		if ( cur_q->rxtag_seq_num !=  GET_DESC_FIELD(RXBDDword0, 0x1fff, 16)) {
			/*RT_TRACE(COMP_RECV, DBG_WARNING, ("Polling failed(cnt: %d), keep trying, DW0(0x%x), RXBDCheckRdySeqNum(0x%x) FS,LS(0x%x,0x%x)\n", 
											PollingCnt, 
											GET_DESC_FIELD(RXBDDword0, RXBD_DW0_TOTALRXPKTSIZE_MSK, RXBD_DW0_TOTALRXPKTSIZE_SH), 
											cur_q->rxtag_seq_num,
											GET_DESC_FIELD(RXBDDword0, RXBD_DW0_FS_MSK, RXBD_DW0_FS_SH),
											GET_DESC_FIELD(RXBDDword0, RXBD_DW0_LS_MSK, RXBD_DW0_LS_SH)
											));*/
		}
		else {
			break;
		}
#else
		if (0 == GET_DESC_FIELD(cur_q->pRXBD_head[cur_q->cur_host_idx].Dword0, 0x1fff, 16)) {
			RT_TRACE(COMP_RECV, DBG_WARNING, ("Polling failed(cnt: %d), keep trying, DW0(0x%x)\n", PollingCnt, GET_DESC_FIELD(cur_q->pRXBD_head[cur_q->cur_host_idx].Dword0, RXBD_DW0_TOTALRXPKTSIZE_MSK, RXBD_DW0_TOTALRXPKTSIZE_SH)));
		}
		else {
			break;
		}
#endif   //RXBD_READY_CHECK_METHOD     
	
		PollingCnt++;
		HAL_delay_us(10);
	} while(PollingCnt < RXBD_RXTAG_POLLING_CNT);
	
	
	if ( PollingCnt >= RXBD_RXTAG_POLLING_CNT ) {
	
		RT_TRACE(COMP_RECV, DBG_SERIOUS, ("Polling failed(0x%x)\n", Adapter->pshare->RxTagPollingCount));
	
		Adapter->pshare->RxTagPollingCount++;
		Adapter->pshare->RxTagMismatchCount++;
	
#if CFG_HAL_DBG        
		//code below in order to dump packet
		bResult = FAIL;
		prx_desc_status->FS = 1;
		prx_desc_status->LS = 1;
		goto _RXPKT_DUMP;
#else
	//		  return RT_STATUS_FAILURE;
	
#if RXBD_READY_CHECK_METHOD
		cur_q->rxtag_seq_num = GET_DESC_FIELD(RXBDDword0, RXBD_DW0_TOTALRXPKTSIZE_MSK, RXBD_DW0_TOTALRXPKTSIZE_SH);
#endif
#endif  //#if  CFG_HAL_DBG        
	
	}
	else {
		Adapter->pshare->RxTagPollingCount=0;
	}
#if RXBD_READY_CHECK_METHOD
	cur_q->rxtag_seq_num++;
	cur_q->rxtag_seq_num &= RXBD_RXTAG_MASK;
#endif  //#if RXBD_READY_CHECK_METHOD

}

    // get RXBD
	#ifdef CONFIG_PE_ENABLE	//TBD with YL 
	if(Adapter->pshare->rf_ft_var.manual_pe_enable){
		prx_desc_status->FS = 1;
		prx_desc_status->LS = 1;
		prx_desc_status->RXBuffSize = 0; // ignore
	}else
    #endif
    {
		//RXBDDword0 = prxbd->Dword0;
		prx_desc_status->FS = GET_DESC_FIELD(RXBDDword0, RXBD_DW0_FS_MSK, RXBD_DW0_FS_SH);
		prx_desc_status->LS = GET_DESC_FIELD(RXBDDword0, RXBD_DW0_LS_MSK, RXBD_DW0_LS_SH);
		prx_desc_status->RXBuffSize = GET_DESC_FIELD(RXBDDword0, RXBD_DW0_RXBUFSIZE_MSK, RXBD_DW0_RXBUFSIZE_SH);
		pktinfo_offset = sizeof(struct d2h_rxdma_info);
		desc_offset = sizeof(struct d2h_rxdma_info) + sizeof(struct d2h_rxpkt_info);
    }
#if defined(CONFIG_PE_ENABLE)
#else
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
	if (OFFLOAD_ENABLE(Adapter))
    	SET_DESC_FIELD_CLR(prxbd->Dword0, Adapter->pshare->rx_buf_len, RXBD_DW0_RXBUFSIZE_MSK, RXBD_DW0_RXBUFSIZE_SH);
#endif
#endif
    if ( prx_desc_status->FS==0x01 ) {
#ifdef CONFIG_NET_PCI
        if (HAL_IS_PCIBIOS_TYPE(Adapter))
            HAL_CACHE_SYNC_WBACK(Adapter, GET_HW(Adapter)->rx_infoL[cur_q->cur_host_idx].paddr,
                            desc_offset + sizeof(RX_DESC_88XX), HAL_PCI_DMA_FROMDEVICE);
#else

#ifdef TRXBD_CACHABLE_REGION
#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
        // With CCI400 ,Do nothing for cache coherent code
#else
        rtl_cache_sync_wback(Adapter,(unsigned long)bus_to_virt((PVOID)GET_HW(Adapter)->rx_infoL[cur_q->cur_host_idx].paddr), 
        desc_offset + sizeof(RX_DESC_88XX),HAL_PCI_DMA_FROMDEVICE);

#endif //CONFIG_ENABLE_CCI400
#endif //#ifdef CONFIG_RTL_8198F            
#endif //#ifdef TRXBD_CACHABLE_REGION

#endif

        // get RxPktInfo
        prx_pktinfo = (struct d2h_rxpkt_info *)&pBufAddr[pktinfo_offset];
#ifdef _BIG_ENDIAN_
        {
            int i, *pswap = (int *)prx_pktinfo;
            for (i=0; i<(sizeof(*prx_pktinfo)-4); i+=4, pswap++)
                *pswap = cpu_to_le32(*pswap);
        }
#endif
        if (prx_pktinfo->pkt_type == RXPKTINFO_TYPE_D2H) {
            prx_desc_status->PKT_LEN = prx_pktinfo->rx_raw_len;
            return RT_STATUS_SUCCESS;
        } else if (prx_pktinfo->pkt_type == RXPKTINFO_TYPE_RAW) {
           desc_offset += sizeof(RX_DESC_88XX);
        }

        // get RxPktDESC
        prx_desc = (PRX_DESC_88XX)&pBufAddr[desc_offset];
        prx_desc_status->PKT_LEN        = GET_RX_DESC_PKT_LEN(prx_desc);
        prx_desc_status->CRC32          = GET_RX_DESC_CRC32(prx_desc);
        prx_desc_status->ICVERR         = GET_RX_DESC_ICV_ERR(prx_desc);
        prx_desc_status->DRV_INFO_SIZE  = (GET_RX_DESC_DRV_INFO_SIZE(prx_desc)) << 3;
        prx_desc_status->SHIFT          = GET_RX_DESC_SHIFT(prx_desc);
        prx_desc_status->PHYST          = GET_RX_DESC_PHYST(prx_desc);
        prx_desc_status->SWDEC          = GET_RX_DESC_SWDEC(prx_desc);   
        prx_desc_status->TID            = GET_RX_DESC_TID(prx_desc);
        prx_desc_status->PAGGR          = GET_RX_DESC_AMPDU(prx_desc);
        prx_desc_status->AMSDU          = GET_RX_DESC_AMSDU(prx_desc);
        prx_desc_status->C2HPkt         = GET_RX_DESC_C2H(prx_desc);
        prx_desc_status->SEQ            = GET_RX_DESC_SEQ(prx_desc);
        prx_desc_status->FRAG           = GET_RX_DESC_FRAG(prx_desc);
        prx_desc_status->RX_RATE        = GET_RX_DESC_RX_RATE(prx_desc);
        prx_desc_status->A1_MATCH       = GET_RX_DESC_A1_MATCH(prx_desc);
        prx_desc_status->AMSDU_CUT      = GET_RX_DESC_AMSDU_CUT(prx_desc);
        prx_desc_status->LAST_MSDU      = GET_RX_DESC_LAST_MSDU(prx_desc);
    }

_RXPKT_DUMP:

    if ( SUCCESS == bResult )
        return RT_STATUS_SUCCESS;
    else
        return RT_STATUS_FAILURE;
}


#endif //#if (HAL_DEV_BUS_TYPE & (HAL_RT_EMBEDDED_INTERFACE | HAL_RT_PCI_INTERFACE))

