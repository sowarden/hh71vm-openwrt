/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	HalCfg.c
	
Abstract:
	Defined Mapping Function for each Driver 
	    
Major Change History:
	When       Who               What
	---------- ---------------   -------------------------------
	2012-04-11 Filen            Create.	
--*/

#include "HalPrecomp.h"

#if     IS_RTL88XX_GENERATION
void
MappingVariable88XX(
    IN  HAL_PADAPTER    Adapter
)
{
    //PHAL_DATA_TYPE      pHalData = _GET_HAL_DATA(Adapter);

    //pHalData->
    

}

HAL_IMEM
u4Byte  //HCI_TX_DMA_QUEUE_88XX
MappingTxQueue88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          TxQNum      //enum _TX_QUEUE_
)
{
    int mapping[] = {
        HCI_TX_DMA_QUEUE_MGT,
        HCI_TX_DMA_QUEUE_BK,
        HCI_TX_DMA_QUEUE_BE,
        HCI_TX_DMA_QUEUE_VI,
        HCI_TX_DMA_QUEUE_VO,       
        HCI_TX_DMA_QUEUE_HI0,
        HCI_TX_DMA_QUEUE_HI1,
        HCI_TX_DMA_QUEUE_HI2,
        HCI_TX_DMA_QUEUE_HI3,
        HCI_TX_DMA_QUEUE_HI4,
        HCI_TX_DMA_QUEUE_HI5,
        HCI_TX_DMA_QUEUE_HI6,
        HCI_TX_DMA_QUEUE_HI7,
        HCI_TX_DMA_QUEUE_CMD,        
        HCI_TX_DMA_QUEUE_BCN};
      

    if ( HCI_TX_DMA_QUEUE_MAX_NUM <= TxQNum ) {
        RT_TRACE_F(COMP_SEND, DBG_WARNING, ("Unknown Queue Mapping\n"));        
        return HCI_TX_DMA_QUEUE_BE;
    }
    else {
        return mapping[TxQNum];
    }
}

HAL_IMEM
u4Byte  //HCI_TX_DMA_QUEUE_88XX
MappingTxQueue88XX_V1(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          TxQNum      //enum _TX_QUEUE_
)
{
    int mapping[] = {
        HCI_TX_DMA_QUEUE_MGT,
        HCI_TX_DMA_QUEUE_BK,
        HCI_TX_DMA_QUEUE_BE,
        HCI_TX_DMA_QUEUE_VI,
        HCI_TX_DMA_QUEUE_VO,       
        HCI_TX_DMA_QUEUE_HI0,
        HCI_TX_DMA_QUEUE_HI1,
        HCI_TX_DMA_QUEUE_HI2,
        HCI_TX_DMA_QUEUE_HI3,
        HCI_TX_DMA_QUEUE_HI4,
        HCI_TX_DMA_QUEUE_HI5,
        HCI_TX_DMA_QUEUE_HI6,
        HCI_TX_DMA_QUEUE_HI7,
        HCI_TX_DMA_QUEUE_HI8,
        HCI_TX_DMA_QUEUE_HI9,        
        HCI_TX_DMA_QUEUE_HI10,
        HCI_TX_DMA_QUEUE_HI11,
        HCI_TX_DMA_QUEUE_HI12,
        HCI_TX_DMA_QUEUE_HI13,
        HCI_TX_DMA_QUEUE_HI14,
        HCI_TX_DMA_QUEUE_HI15,        
        HCI_TX_DMA_QUEUE_CMD,        
        HCI_TX_DMA_QUEUE_BCN};
    if ( HCI_TX_DMA_QUEUE_MAX_NUM_V1 <= TxQNum ) {
        RT_TRACE_F(COMP_SEND, DBG_WARNING, ("Unknown Queue Mapping\n"));        
        return HCI_TX_DMA_QUEUE_BE;
    }
    else {
        return mapping[TxQNum];
    }
}

HAL_IMEM
u4Byte  //HCI_TX_DMA_QUEUE_88XX
MappingTxQueue88XX_V2(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          TxQNum      //enum _TX_QUEUE_
)
{
    int mapping[] = {
        HCI_TX_DMA_QUEUE_MGT_V2,
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
        HCI_TX_DMA_QUEUE_FWCMD,
        HCI_TX_DMA_QUEUE_CMD_V2,//H2C //CH20
        HCI_TX_DMA_QUEUE_BCN_V2};
#if defined(CONFIG_PE_ENABLE)
    if(Adapter->pshare->rf_ft_var.manual_pe_enable){
        if((TxQNum >= DATAQ_IDX_DCOFLD_MIN) && (TxQNum <= DATAQ_IDX_DCOFLD_MAX)){
            printk("[%s] Wrong Queue index!!! Data Queue index is not allowed in PE ENABLED mode, input TxQNum=%d\n",__FUNCTION__,TxQNum);
            return HCI_TX_DMA_QUEUE_MGT_V2;
        }
        else
            return mapping[TxQNum];
    }
    else 
#endif        
        if ( HCI_TX_DMA_QUEUE_MAX_NUM_V2 <= TxQNum ) {
        RT_TRACE_F(COMP_SEND, DBG_WARNING, ("Unknown Queue Mapping\n"));        
        return HCI_TX_DMA_QUEUE_BE_V2;
    }
    else {
        return mapping[TxQNum];
    }
}


#endif  //IS_RTL88XX_GENERATION

