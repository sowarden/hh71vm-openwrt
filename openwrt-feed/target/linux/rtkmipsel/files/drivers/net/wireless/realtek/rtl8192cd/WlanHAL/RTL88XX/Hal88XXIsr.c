/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	Hal88XXIsr.c

Abstract:
	Defined RTL88XX HAL common Function

Major Change History:
	When       Who               What
	---------- ---------------   -------------------------------
	2012-04-30 Filen            Create.
--*/

#if !defined(__ECOS) && !defined(CPTCFG_CFG80211_MODULE)
#include "HalPrecomp.h"
#else
#include "../HalPrecomp.h"
#endif

VOID
EnableIMR88XX(
    IN  HAL_PADAPTER    Adapter
)
{
    PHAL_DATA_TYPE              pHalData = _GET_HAL_DATA(Adapter);
    
    RT_TRACE_F(COMP_INIT, DBG_LOUD, ("\n"));

    HAL_RTL_W32(REG_HIMR0, pHalData->IntMask[0]);
    HAL_RTL_W32(REG_HIMR1, pHalData->IntMask[1]);

#if (IS_RTL88XX_MAC_V2_V3_V4)
    if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3_v4) {
        HAL_RTL_W32(REG_HIMR2, pHalData->IntMask[2]);    
        HAL_RTL_W32(REG_HIMR3, pHalData->IntMask[3]);
    }
#endif //IS_RTL88XX_MAC_V2_V3_V4

#if IS_RTL8198F_SERIES
    if (IS_HARDWARE_TYPE_8198F(Adapter)) {
        HAL_RTL_W32(REG_HIMR4, pHalData->IntMask[4]);    
        HAL_RTL_W32(REG_HIMR5, pHalData->IntMask[5]);
    }
#endif //IS_RTL88XX_MAC_V3
#if IS_RTL88XX_MAC_V4
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
        HAL_RTL_W32(REG_HIMR_4, pHalData->IntMask[4]);    
        HAL_RTL_W32(REG_HIMR_5, pHalData->IntMask[5]);
    }
#endif //IS_RTL88XX_MAC_V4

#if CFG_HAL_SUPPORT_AXI_BUS_EXCEPTION
    if(IS_SUPPORT_AXI_EXCEPTION(Adapter)) {
        // write AXI exception interrupt IMR
        HAL_RTL_W32(REG_HSIMR, pHalData->IntMask[8]);  
    }
#endif // CFG_HAL_SUPPORT_AXI_BUS_EXCEPTION
}

//
// Description:
//	Recognize the interrupt content by reading the interrupt register or content and masking interrupt mask (IMR)
//	if it is our NIC's interrupt. After recognizing, we may clear the all interrupts (ISR).
// Arguments:
//	[in] Adapter -
//		The adapter context.
//	[in] pContent -
//		Under PCI interface, this field is ignord.
//		Under USB interface, the content is the interrupt content pointer.
//		Under SDIO interface, this is the interrupt type which is Local interrupt or system interrupt.
//	[in] ContentLen -
//		The length in byte of pContent.
// Return:
//	If any interrupt matches the mask (IMR), return TRUE, and return FALSE otherwise.
//

HAL_IMEM
BOOLEAN
InterruptRecognized88XX(
    IN  HAL_PADAPTER        Adapter,
	IN	PVOID				pContent,
	IN	u4Byte				ContentLen
)
{
    PHAL_DATA_TYPE              pHalData = _GET_HAL_DATA(Adapter);
    u1Byte                      result;
    u2Byte                      tmp_regname;
    u4Byte bit_hisr2_ind_mask,bit_hisr3_ind_mask;

        //Because 22C has differnt HISR2,3 INT IND bits
        
#if (IS_RTL8814A_SERIES || IS_RTL8197F_SERIES || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES || IS_RTL8198F_SERIES )//IS_RTL88XX_MAC_V2_V3
        if ((GET_CHIP_VER(Adapter) == VERSION_8814A) || (GET_CHIP_VER(Adapter) == VERSION_8197F) || (GET_CHIP_VER(Adapter) == VERSION_8822B) || (GET_CHIP_VER(Adapter) == VERSION_8821C) || (GET_CHIP_VER(Adapter) == VERSION_8198F)) {
        bit_hisr2_ind_mask   = BIT_HISR2_IND_INT_MSK;
        bit_hisr3_ind_mask   = BIT_HISR3_IND_INT_MSK;                       
    }
#endif

#if (IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
        if((GET_CHIP_VER(Adapter) == VERSION_8822C) || (GET_CHIP_VER(Adapter) == VERSION_8812F)){
                bit_hisr2_ind_mask       = BIT_HISR2_IND;
                bit_hisr3_ind_mask       = BIT_HISR3_IND;                                            
        }
#endif

#if IS_RTL88XX_MAC_V4
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
        bit_hisr2_ind_mask   = BIT_HISR2_IND_MSK;
        bit_hisr3_ind_mask   = BIT_HISR3_IND_MSK;                         
    }
#endif

	pHalData->IntArray_bak[0] = pHalData->IntArray[0];
	pHalData->IntArray_bak[1] = pHalData->IntArray[1];

    pHalData->IntArray[0] = HAL_RTL_R32(REG_HISR0);
    HAL_RTL_W32(REG_HISR0, pHalData->IntArray[0]);
    pHalData->IntArray[0] &= pHalData->IntMask[0];

    pHalData->IntArray[1] = HAL_RTL_R32(REG_HISR1);
    HAL_RTL_W32(REG_HISR1, pHalData->IntArray[1]);
	pHalData->IntArray[1] &= pHalData->IntMask[1];

#if (IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
        if((GET_CHIP_VER(Adapter) == VERSION_8822C) || (GET_CHIP_VER(Adapter) == VERSION_8812F)){
                pHalData->IntArray_PCIE[0] = HAL_RTL_R32(REG_PCIE_HISR0_V1);          
        }
#endif

    result = (pHalData->IntArray[0]!=0 || pHalData->IntArray[1]!=0);

/******************************************************
0xB0 BIT15 indicate 0x5C REG_HSISR      => all
0xB0 BIT11 indicate 0xBC REG_HISR1      => all

0xB0 BIT13 indicate 0x10b4  REG_HISR2   => 8814A,8197F,8822B,8198F,8814B
0xB0 BIT14 indicate 0x10bc  REG_HISR3   => 8814A,8197F,8822B,8198F,8814B

0x13B4 BIT13 indicate 0x10b4      REG_HISR2      => 8822C
0x13B4 BIT14 indicate 0x10bc       REG_HISR3      => 8822C

0xB0 BIT17 indicate 0x1094 REG_HISR4    => 8198F add
0xB0 BIT18 indicate 0x109C REG_HISR5    => 8198F add
                              0x1064 REG_HISR_4   => 8814B
                              0x1068 REG_HISR_5   => 8814B
                              
0xB0 BIT19 indicate 0x107C REG_HISR_6    => 8814B add
0xB0 BIT21 indicate 0x10CC REG_HISR_7    => 8814B add

                              
*******************************************************/


//#if (IS_RTL88XX_MAC_V2_V3_V4) Because 22C has different difinition
//       if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3_v4) {
#if (IS_RTL8814A_SERIES || IS_RTL8197F_SERIES || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES || IS_RTL8198F_SERIES || IS_RTL8814B_SERIES || IS_RTL8197G_SERIES)//IS_RTL88XX_MAC_V2_V3
        if ((GET_CHIP_VER(Adapter) == VERSION_8814A) || (GET_CHIP_VER(Adapter) == VERSION_8197F) || (GET_CHIP_VER(Adapter) == VERSION_8822B) || (GET_CHIP_VER(Adapter) == VERSION_8821C) || (GET_CHIP_VER(Adapter) == VERSION_8198F) || (GET_CHIP_VER(Adapter) == VERSION_8814B)|| (GET_CHIP_VER(Adapter) == VERSION_8197G)) {
        if(pHalData->IntArray[0] & bit_hisr2_ind_mask) {
            pHalData->IntArray[2] = HAL_RTL_R32(REG_HISR2);
            HAL_RTL_W32(REG_HISR2, pHalData->IntArray[2]);
            pHalData->IntArray[2] &= pHalData->IntMask[2];
            // No need calculate result,
            // because if this interrupt in tihs group raised. REG_HISR0 already raised.
            //result = (result || (pHalData->IntArray[2]!=0));
        }else
            pHalData->IntArray[2] = 0;

        if(pHalData->IntArray[0] & bit_hisr3_ind_mask) {    
            pHalData->IntArray[3] = HAL_RTL_R32(REG_HISR3);
            HAL_RTL_W32(REG_HISR3, pHalData->IntArray[3]);
            pHalData->IntArray[3] &= pHalData->IntMask[3];
            //result = (result || (pHalData->IntArray[3]!=0));        
        }else
            pHalData->IntArray[3] = 0;      
    }
#endif // #if (IS_RTL88XX_MAC_V2_V3_V4)

#if (IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
        if((GET_CHIP_VER(Adapter) == VERSION_8822C) || (GET_CHIP_VER(Adapter) == VERSION_8812F)){
                if(pHalData->IntArray_PCIE[0] & bit_hisr2_ind_mask) {
                        pHalData->IntArray[2] = HAL_RTL_R32(REG_HISR2);
                        HAL_RTL_W32(REG_HISR2, pHalData->IntArray[2]);
                        pHalData->IntArray[2] &= pHalData->IntMask[2];
                        // No need calculate result,
                        // because if this interrupt in tihs group raised. REG_HISR0 already raised.
                        //result = (result || (pHalData->IntArray[2]!=0));
                }else
                        pHalData->IntArray[2] = 0;

                if(pHalData->IntArray_PCIE[0] & bit_hisr3_ind_mask) {      
                        pHalData->IntArray[3] = HAL_RTL_R32(REG_HISR3);
                        HAL_RTL_W32(REG_HISR3, pHalData->IntArray[3]);
                        pHalData->IntArray[3] &= pHalData->IntMask[3];
                        //result = (result || (pHalData->IntArray[3]!=0));                  
                }else
                        pHalData->IntArray[3] = 0;            
        }
#endif // #if (IS_RTL8822C_SERIES || IS_RTL8812F_SERIES

#if IS_RTL8198F_SERIES //can't use V3 because 97G has no ISR4, ISR5
    if ( IS_HARDWARE_TYPE_8198F(Adapter)) {
       if(pHalData->IntArray[0] & BIT_HISR4_MSK) {
        
            pHalData->IntArray[4] = HAL_RTL_R32(REG_HISR4);
            HAL_RTL_W32(REG_HISR4, pHalData->IntArray[4]);
            pHalData->IntArray[4] &= pHalData->IntMask[4];            
            //result = (result || (pHalData->IntArray[4]!=0));        
        }else
            pHalData->IntArray[4] = 0;

        if(pHalData->IntArray[0] & BIT_HISR5_MSK) { 

            pHalData->IntArray[5] = HAL_RTL_R32(REG_HISR5);
            HAL_RTL_W32(REG_HISR5, pHalData->IntArray[5]);
            pHalData->IntArray[5] &= pHalData->IntMask[5];            
            //result = (result || (pHalData->IntArray[5]!=0));                    
        }else
            pHalData->IntArray[5] = 0;
    }
#endif  //IS_RTL88XX_MAC_V3


#if IS_RTL88XX_MAC_V4
    if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4) {
        if(pHalData->IntArray[0] & BIT_HISR4_IND_MSK) {
            
            pHalData->IntArray[4] = HAL_RTL_R32(REG_HISR_4);
            HAL_RTL_W32(REG_HISR_4, pHalData->IntArray[4]);
            pHalData->IntArray[4] &= pHalData->IntMask[4];
            //result = (result || (pHalData->IntArray[4]!=0));        
        }else
            pHalData->IntArray[4] = 0;

        if(pHalData->IntArray[0] & BIT_HISR5_IND_MSK) { 
        
            pHalData->IntArray[5] = HAL_RTL_R32(REG_HISR_5);
            HAL_RTL_W32(REG_HISR_5, pHalData->IntArray[5]);
            pHalData->IntArray[5] &= pHalData->IntMask[5];
            //result = (result || (pHalData->IntArray[5]!=0));                    
        }else
            pHalData->IntArray[5] = 0;

        if(pHalData->IntArray[0] & BIT_HISR6_IND_MSK) {
            pHalData->IntArray[6] = HAL_RTL_R32(REG_HISR_6);
            HAL_RTL_W32(REG_HISR_6, pHalData->IntArray[6]);
            pHalData->IntArray[6] &= pHalData->IntMask[6];
        }else
            pHalData->IntArray[6]=0;

        if(pHalData->IntArray[0] & BIT_HISR7_IND_MSK) {
            pHalData->IntArray[7] = HAL_RTL_R32(REG_HISR_7);
            HAL_RTL_W32(REG_HISR_7, pHalData->IntArray[7]);
            pHalData->IntArray[7] &= pHalData->IntMask[7];
        }else
            pHalData->IntArray[7] = 0;
    }
#endif //IS_RTL88XX_MAC_V4

#if CFG_HAL_SUPPORT_AXI_BUS_EXCEPTION
    if(IS_SUPPORT_AXI_EXCEPTION(Adapter)) {

        if(pHalData->IntArray[0] & BIT_HSISR_IND_ON_INT_MSK) {
            pHalData->IntArray[8] = HAL_RTL_R32(REG_HSISR);
            HAL_RTL_W32(REG_HSISR, pHalData->IntArray[8]);
            pHalData->IntArray[8] &= pHalData->IntMask[8];
        }
    }
    
    //result = (result || (pHalData->IntArray[6]!=0));    
    
#endif // CFG_HAL_SUPPORT_AXI_BUS_EXCEPTION


    return result;
}

//
// Description:
//	Check the interrupt content (read from previous process) in HAL.
// Arguments:
//	[in] pAdapter -
//		The adapter context pointer.
//	[in] intType -
//		The HAL interrupt type for querying.
// Return:
//	If the corresponding interrupt content (bit) is toggled, return TRUE.
//	If the input interrupt type isn't recognized or this corresponding
//	hal interupt isn't toggled, return FALSE.
// Note:
//	We don't perform I/O here to read interrupt such as ISR here, so the
//	interrupt content shall be read before this handler.
//
HAL_IMEM
BOOLEAN
GetInterrupt88XX(
	IN	HAL_PADAPTER	Adapter,
	IN	HAL_INT_TYPE	intType
	)
{
	HAL_DATA_TYPE	*pHalData   = _GET_HAL_DATA(Adapter);
	BOOLEAN			bResult     = FALSE;
    u4Byte bit_pstimeout,bit_pstimeout1,bit_pstimeout2;
    u4Byte bit_VIdok,bit_VOdok,bit_BEdok,bit_BKdok,bit_MGTdok,bit_HIGHdok;
#if IS_RTL88XX_MAC_V1_V2_V3
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3){
        bit_pstimeout   = BIT_PSTIMEOUT;
        bit_pstimeout1  = BIT_PSTIMEOUT1;
        bit_pstimeout2  = BIT_PSTIMEOUT2;
        bit_HIGHdok     = BIT_HIGHDOK;   
        bit_MGTdok      = BIT_MGTDOK;
        bit_BKdok       = BIT_BKDOK;
        bit_BEdok       = BIT_BEDOK;        
        bit_VIdok       = BIT_VIDOK;
        bit_VOdok       = BIT_VODOK;                            
    }
#endif
#if IS_RTL88XX_MAC_V4
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
        bit_pstimeout   = BIT_PSTIMER_0;
        bit_pstimeout1  = BIT_PSTIMER_1;
        bit_pstimeout2  = BIT_PSTIMER_2;
        bit_HIGHdok     = BIT_TXDMAOK_CHANNEL15; 
        bit_MGTdok      = BIT_TXDMAOK_CHANNEL14;
        bit_BKdok       = BIT_TXDMAOK_CHANNEL3;
        bit_BEdok       = BIT_TXDMAOK_CHANNEL2;
        bit_VIdok       = BIT_TXDMAOK_CHANNEL1;
        bit_VOdok       = BIT_TXDMAOK_CHANNEL0;                                 
    }
#endif
	switch(intType)
	{
	default:
		// Unknown interrupt type, no need to alarm because this IC may not
		// support this interrupt.
		RT_TRACE_F(COMP_SYSTEM, DBG_WARNING, ("Unkown intType: %d!\n", intType));
		break;

	case HAL_INT_TYPE_ANY:
		bResult = (pHalData->IntArray[0] || pHalData->IntArray[1]) ? TRUE : FALSE;
		break;

	//4 // ========== DWORD 0 ==========
	case HAL_INT_TYPE_BCNDERR0:
		bResult = (pHalData->IntArray[0] & BIT_BCNDERR0) ? TRUE : FALSE;
		break;

	case HAL_INT_TYPE_TBDOK:
		bResult = (pHalData->IntArray[0] & BIT_TXBCN0OK) ? TRUE : FALSE;
		break;
		
	case HAL_INT_TYPE_TBDER:
		bResult = (pHalData->IntArray[0] & BIT_TXBCN0ERR) ? TRUE : FALSE;
		break;
		
	case HAL_INT_TYPE_BcnInt:
		bResult = (pHalData->IntArray[0] & BIT_BCNDMAINT0) ? TRUE : FALSE;
		break;
    case HAL_INT_TYPE_GTIMER4:
        bResult = (pHalData->IntArray[0] & BIT_GTINT4) ? TRUE : FALSE;
		break;
		
	case HAL_INT_TYPE_PSTIMEOUT:
		bResult = (pHalData->IntArray[0] & bit_pstimeout) ? TRUE : FALSE;
		break;

	case HAL_INT_TYPE_PSTIMEOUT1:
		bResult = (pHalData->IntArray[0] & bit_pstimeout1) ? TRUE : FALSE;
		break;

	case HAL_INT_TYPE_PSTIMEOUT2:
		bResult = (pHalData->IntArray[0] & bit_pstimeout2) ? TRUE : FALSE;
		break; 
		
	case HAL_INT_TYPE_C2HCMD:
		bResult = (pHalData->IntArray[0] & BIT_C2HCMD) ? TRUE : FALSE;
		break;	
		
	case HAL_INT_TYPE_VIDOK:
		bResult = (pHalData->IntArray[0] & bit_VIdok) ? TRUE : FALSE;
		break;
		
	case HAL_INT_TYPE_VODOK:
		bResult = (pHalData->IntArray[0] & bit_VOdok) ? TRUE : FALSE;
		break;
		
	case HAL_INT_TYPE_BEDOK:
		bResult = (pHalData->IntArray[0] & bit_BEdok) ? TRUE : FALSE;
		break;
		
	case HAL_INT_TYPE_BKDOK:
		bResult = (pHalData->IntArray[0] & bit_BKdok) ? TRUE : FALSE;
		break;

	case HAL_INT_TYPE_MGNTDOK:
		bResult = (pHalData->IntArray[0] & bit_MGTdok) ? TRUE : FALSE;
		break;

	case HAL_INT_TYPE_HIGHDOK:
		bResult = (pHalData->IntArray[0] & bit_HIGHdok) ? TRUE : FALSE;
		break;

    #if 0   //Filen: removed
	case HAL_INT_TYPE_BDOK:
		bResult = (pHalData->IntArray[0] & IMR_BCNDOK0_88E) ? TRUE : FALSE;
		break;
    #endif
		
	case HAL_INT_TYPE_CPWM:
		bResult = (pHalData->IntArray[0] & BIT_CPWM) ? TRUE : FALSE;
		break;

	case HAL_INT_TYPE_TSF_BIT32_TOGGLE:
		bResult = (pHalData->IntArray[0] & BIT_TSF_BIT32_TOGGLE) ? TRUE : FALSE;
		break;

    case HAL_INT_TYPE_RX_OK:
		bResult = (pHalData->IntArray[0] & BIT_RXOK) ? TRUE : FALSE;
        break;
        
    case HAL_INT_TYPE_RDU:
		bResult = (pHalData->IntArray[0] & BIT_RDU) ? TRUE : FALSE;
        break;        
    case HAL_INT_TYPE_CPWM2:
            bResult = (pHalData->IntArray[0] & BIT_CPWM2) ? TRUE : FALSE;
            break;

    case HAL_INT_TYPE_CTWEND:
#if IS_RTL88XX_MAC_V1
        if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1) { 
    		bResult = (pHalData->IntArray[0] & BIT_CTWEND) ? TRUE : FALSE;
        }
#endif //(IS_RTL8192E_SERIES || IS_RTL8881A_SERIES)
        
#if IS_RTL88XX_MAC_V2
        if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2)  
        {
            //this interrupt is removed at 8814A
            bResult = FALSE;
        }
#endif // IS_RTL8192E_SERIES
        break;

#if IS_RTL88XX_MAC_V4
	case HAL_INT_TXDMA_START:
		bResult = (pHalData->IntArray[0] & BIT_TXDMA_START_INT_MSK) ? TRUE : FALSE;
		break;

	case HAL_INT_TXDMA_STOP:
		bResult = (pHalData->IntArray[0] & BIT_TXDMA_STOP_INT_MSK) ? TRUE : FALSE;
		break;
#endif

	//4 // ========== DWORD 1 ==========
	case HAL_INT_TYPE_RXFOVW:
		bResult = (pHalData->IntArray[1] & BIT_FOVW) ? TRUE : FALSE;
		break;

    case HAL_INT_TYPE_TXFOVW:
        bResult = (pHalData->IntArray[1] & BIT_TXFOVW) ? TRUE : FALSE;
		break;

    case HAL_INT_TYPE_RXERR:
        bResult = (pHalData->IntArray[1] & BIT_RXERR_INT) ? TRUE : FALSE;
		break;        
        
    case HAL_INT_TYPE_TXERR:
        bResult = (pHalData->IntArray[1] & BIT_TXERR_INT) ? TRUE : FALSE;
		break;   
    case HAL_INT_TYPE_BcnInt_MBSSID:    
#if IS_RTL88XX_MAC_V1_V2 || IS_RTL8197G_SERIES
        if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2 || IS_HARDWARE_TYPE_8197G(Adapter)) {
            bResult = ((pHalData->IntArray[1] & (BIT_BCNDMAINT1|BIT_BCNDMAINT2|BIT_BCNDMAINT3|BIT_BCNDMAINT4|
                                        BIT_BCNDMAINT5|BIT_BCNDMAINT6|BIT_BCNDMAINT7)) ||
                        (pHalData->IntArray[0] & BIT_BCNDMAINT0)
                        ) ? TRUE : FALSE;
        }
#endif //IS_RTL88XX_MAC_V1_V2       
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter)) {
            bResult = ((pHalData->IntArray[1] & (BIT_BCNDMAINT1|BIT_BCNDMAINT2|BIT_BCNDMAINT3|BIT_BCNDMAINT4|BIT_BCNDMAINT5|
                    BIT_BCNDMAINT6|BIT_BCNDMAINT7) )||
                    (pHalData->IntArray[4] & (BIT_BCN_DMA_INT8|BIT_BCN_DMA_INT9|BIT_BCN_DMA_INT10|BIT_BCN_DMA_INT11|
                    BIT_BCN_DMA_INT12|BIT_BCN_DMA_INT13|BIT_BCN_DMA_INT14|BIT_BCN_DMA_INT15)) ||
                    (pHalData->IntArray[0] & BIT_BCNDMAINT0)
                    ) ? TRUE : FALSE;
            
        }
#endif //IS_RTL8198F_SERIES
#if IS_RTL88XX_MAC_V4
        if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4) {
            bResult = ((pHalData->IntArray[1] & (BIT_BCNDMAINT1|BIT_BCNDMAINT2|BIT_BCNDMAINT3|BIT_BCNDMAINT4|BIT_BCNDMAINT5|
                        BIT_BCNDMAINT6|BIT_BCNDMAINT7)) || 
                        (pHalData->IntArray[5] & (BIT_BCNDMAINT8|BIT_BCNDMAINT9|BIT_BCNDMAINT10|BIT_BCNDMAINT11|
                        BIT_BCNDMAINT12|BIT_BCNDMAINT13|BIT_BCNDMAINT14|BIT_BCNDMAINT15)) ||
                        (pHalData->IntArray[0] & BIT_BCNDMAINT0)) ? TRUE : FALSE;
        }
#endif //#if IS_RTL88XX_MAC_V4
        break;
    case HAL_INT_TYPE_BcnInt1:
		bResult = (pHalData->IntArray[1] & BIT_BCNDMAINT1) ? TRUE : FALSE;
        break;

    case HAL_INT_TYPE_BcnInt2:
		bResult = (pHalData->IntArray[1] & BIT_BCNDMAINT2) ? TRUE : FALSE;
        break;

    case HAL_INT_TYPE_BcnInt3:
		bResult = (pHalData->IntArray[1] & BIT_BCNDMAINT3) ? TRUE : FALSE;
        break;

    case HAL_INT_TYPE_BcnInt4:
		bResult = (pHalData->IntArray[1] & BIT_BCNDMAINT4) ? TRUE : FALSE;
        break;

    case HAL_INT_TYPE_BcnInt5:
		bResult = (pHalData->IntArray[1] & BIT_BCNDMAINT5) ? TRUE : FALSE;
        break;

    case HAL_INT_TYPE_BcnInt6:
		bResult = (pHalData->IntArray[1] & BIT_BCNDMAINT6) ? TRUE : FALSE;
        break;

    case HAL_INT_TYPE_BcnInt7:
		bResult = (pHalData->IntArray[1] & BIT_BCNDMAINT7) ? TRUE : FALSE;
        break;
    //4 // ========== DWORD 2 ==========
#if CFG_HAL_SUPPORT_EACH_VAP_INT
    case HAL_INT_TYPE_TXBCNOK_MBSSID:
#if IS_RTL88XX_MAC_V1_V2 || IS_RTL8197G_SERIES
        if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2 || IS_HARDWARE_TYPE_8197G(Adapter)) {
            bResult = ((pHalData->IntArray[2] & (BIT_TXBCN1OK|BIT_TXBCN2OK|BIT_TXBCN3OK|BIT_TXBCN4OK|
                                        BIT_TXBCN5OK|BIT_TXBCN6OK|BIT_TXBCN7OK)) ||
                        (pHalData->IntArray[0] & BIT_TXBCN0OK)
                        ) ? TRUE : FALSE;
        }
#endif //IS_RTL88XX_MAC_V1_V2    
#if IS_RTL8198F_SERIES
        if (IS_HARDWARE_TYPE_8198F(Adapter)) {
            bResult = ((pHalData->IntArray[2] & (BIT_TXBCN1OK|BIT_TXBCN2OK|BIT_TXBCN3OK|BIT_TXBCN4OK|BIT_TXBCN5OK|
                        BIT_TXBCN6OK|BIT_TXBCN7OK)) ||
                        (pHalData->IntArray[4] & (BIT_TX_BCN_OK_INT8|BIT_TX_BCN_OK_INT9|BIT_TX_BCN_OK_INT10|BIT_TX_BCN_OK_INT11|
                        BIT_TX_BCN_OK_INT12|BIT_TX_BCN_OK_INT13|BIT_TX_BCN_OK_INT14|BIT_TX_BCN_OK_INT15)) || 
                        (pHalData->IntArray[0] & BIT_TXBCN0OK)
                        ) ? TRUE : FALSE;
        }
#endif //IS_RTL8198F_SERIES
#if IS_RTL88XX_MAC_V4
        if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4) {
            bResult = ((pHalData->IntArray[2] & (BIT_TXBCN1OK|BIT_TXBCN2OK|BIT_TXBCN3OK|BIT_TXBCN4OK|BIT_TXBCN5OK|
                        BIT_TXBCN6OK|BIT_TXBCN7OK)) || 
                        (pHalData->IntArray[4] & (BIT_TXBCN8OK|BIT_TXBCN9OK|BIT_TXBCN10OK|BIT_TXBCN11OK|
                        BIT_TXBCN12OK|BIT_TXBCN13OK|BIT_TXBCN14OK|BIT_TXBCN15OK)) || 
                        (pHalData->IntArray[0] & BIT_TXBCN0OK)
                        ) ? TRUE : FALSE;
        }
#endif //#if IS_RTL88XX_MAC_V4
        break;
    case HAL_INT_TYPE_TXBCN1OK:
        bResult = (pHalData->IntArray[2] & BIT_TXBCN1OK) ? TRUE : FALSE;
        break;
    case HAL_INT_TYPE_TXBCN2OK:
        bResult = (pHalData->IntArray[2] & BIT_TXBCN2OK) ? TRUE : FALSE;
        break;
    case HAL_INT_TYPE_TXBCN3OK:
        bResult = (pHalData->IntArray[2] & BIT_TXBCN3OK) ? TRUE : FALSE;
        break;
    case HAL_INT_TYPE_TXBCN4OK:
        bResult = (pHalData->IntArray[2] & BIT_TXBCN4OK) ? TRUE : FALSE;
        break;
    case HAL_INT_TYPE_TXBCN5OK:
        bResult = (pHalData->IntArray[2] & BIT_TXBCN5OK) ? TRUE : FALSE;
        break;    
    case HAL_INT_TYPE_TXBCN6OK:
        bResult = (pHalData->IntArray[2] & BIT_TXBCN6OK) ? TRUE : FALSE;
        break;    
    case HAL_INT_TYPE_TXBCN7OK:
        bResult = (pHalData->IntArray[2] & BIT_TXBCN7OK) ? TRUE : FALSE;
        break;    
    case HAL_INT_TYPE_TXBCNERR_MBSSID:    
#if IS_RTL88XX_MAC_V1_V2 || IS_RTL8197G_SERIES
        if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2 || IS_HARDWARE_TYPE_8197G(Adapter)) {
            bResult = ((pHalData->IntArray[2] & (BIT_TXBCN1ERR|BIT_TXBCN2ERR|BIT_TXBCN3ERR|BIT_TXBCN4ERR|
                                       BIT_TXBCN5ERR|BIT_TXBCN6ERR|BIT_TXBCN7ERR)) ||
                       (pHalData->IntArray[0] & BIT_TXBCN0ERR)
                       ) ? TRUE : FALSE;
        }
#endif// IS_RTL88XX_MAC_V1_V2 
#if IS_RTL8198F_SERIES
        if (IS_HARDWARE_TYPE_8198F(Adapter)) {
            bResult = ((pHalData->IntArray[2] & (BIT_TXBCN1ERR|BIT_TXBCN2ERR|BIT_TXBCN3ERR|BIT_TXBCN4ERR|
                                       BIT_TXBCN5ERR|BIT_TXBCN6ERR|BIT_TXBCN7ERR)) ||
                       (pHalData->IntArray[4] & (BIT_TX_BCN_ERR_INT8|BIT_TX_BCN_ERR_INT9|BIT_TX_BCN_ERR_INT10|BIT_TX_BCN_ERR_INT11|
                        BIT_TX_BCN_ERR_INT12|BIT_TX_BCN_ERR_INT13|BIT_TX_BCN_ERR_INT14|BIT_TX_BCN_ERR_INT15)) || 
                       (pHalData->IntArray[0] & BIT_TXBCN0ERR)
                       ) ? TRUE : FALSE;
        }
#endif //IS_RTL8198F_SERIES
#if IS_RTL88XX_MAC_V4
        if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4) {
            bResult = ((pHalData->IntArray[2] & (BIT_TXBCN1ERR|BIT_TXBCN2ERR|BIT_TXBCN3ERR|BIT_TXBCN4ERR|
                        BIT_TXBCN5ERR|BIT_TXBCN6ERR|BIT_TXBCN7ERR)) || 
                        (pHalData->IntArray[4] & (BIT_TXBCN8ERR|BIT_TXBCN9ERR|BIT_TXBCN10ERR|BIT_TXBCN11ERR|
                        BIT_TXBCN12ERR|BIT_TXBCN13ERR|BIT_TXBCN14ERR|BIT_TXBCN15ERR)) ||
                       (pHalData->IntArray[0] & BIT_TXBCN0ERR)) ? TRUE : FALSE;
        }
#endif//#if IS_RTL88XX_MAC_V4
        break;
    case HAL_INT_TYPE_TXBCN1ERR:
        bResult = (pHalData->IntArray[2] & BIT_TXBCN1ERR) ? TRUE : FALSE;
        break;
    case HAL_INT_TYPE_TXBCN2ERR:
        bResult = (pHalData->IntArray[2] & BIT_TXBCN2ERR) ? TRUE : FALSE;
        break;
    case HAL_INT_TYPE_TXBCN3ERR:
        bResult = (pHalData->IntArray[2] & BIT_TXBCN3ERR) ? TRUE : FALSE;
        break;
    case HAL_INT_TYPE_TXBCN4ERR:
        bResult = (pHalData->IntArray[2] & BIT_TXBCN4ERR) ? TRUE : FALSE;
        break;
    case HAL_INT_TYPE_TXBCN5ERR:
        bResult = (pHalData->IntArray[2] & BIT_TXBCN5ERR) ? TRUE : FALSE;
        break;    
    case HAL_INT_TYPE_TXBCN6ERR: 
        bResult = (pHalData->IntArray[2] & BIT_TXBCN6ERR) ? TRUE : FALSE;
        break;    
    case HAL_INT_TYPE_TXBCN7ERR:
        bResult = (pHalData->IntArray[2] & BIT_TXBCN7ERR) ? TRUE : FALSE;
        break;            
#endif // #if CFG_HAL_SUPPORT_EACH_VAP_INT
    //4 // ========== DWORD 3 ==========
#if CFG_HAL_HW_DETEC_POWER_STATE
	case HAL_INT_TYPE_PwrInt0:
#if IS_EXIST_RTL8192FE
		if ( IS_HARDWARE_TYPE_8192FE(Adapter) )
			bResult = (pHalData->IntArray[1] & BIT_PWR_INT_31TO0) ? TRUE : FALSE;
		else
#endif
		bResult = (pHalData->IntArray[3] & BIT_PWR_INT_31TO0) ? TRUE : FALSE;
		break;
	case HAL_INT_TYPE_PwrInt1:
#if IS_EXIST_RTL8192FE
		if ( IS_HARDWARE_TYPE_8192FE(Adapter) )
			bResult = (pHalData->IntArray[1] & BIT_PWR_INT_63TO32) ? TRUE : FALSE;
		else
#endif
		bResult = (pHalData->IntArray[3] & BIT_PWR_INT_63TO32) ? TRUE : FALSE;
		break;
	case HAL_INT_TYPE_PwrInt2:
#if IS_EXIST_RTL8192FE
		if ( IS_HARDWARE_TYPE_8192FE(Adapter) )
			bResult = (pHalData->IntArray[1] & BIT_PWR_INT_95TO64) ? TRUE : FALSE;
		else
#endif
		bResult = (pHalData->IntArray[3] & BIT_PWR_INT_95TO64) ? TRUE : FALSE;
		break;
	case HAL_INT_TYPE_PwrInt3:
#if IS_EXIST_RTL8192FE
		if ( IS_HARDWARE_TYPE_8192FE(Adapter) )
			bResult = (pHalData->IntArray[1] & BIT_PWR_INT_126TO96) ? TRUE : FALSE;
		else
#endif
		{
#if IS_RTL88XX_MAC_V4		 
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
            bResult = (pHalData->IntArray[3] & BIT_PWR_INT_127TO96) ? TRUE : FALSE;
        }
#endif
#if IS_RTL88XX_MAC_V2_V3
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3){
            bResult = (pHalData->IntArray[3] & BIT_PWR_INT_126TO96) ? TRUE : FALSE;
        }
#endif
		}
		break;
	case HAL_INT_TYPE_PwrInt4:
#if IS_EXIST_RTL8192FE
		if ( IS_HARDWARE_TYPE_8192FE(Adapter) )
			bResult = (pHalData->IntArray[1] & BIT_PWR_INT_127) ? TRUE : FALSE;
		else
#endif
		{
#if IS_RTL88XX_MAC_V4		 
		if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
			bResult = (pHalData->IntArray[3] & BIT_SEARCH_FAIL) ? TRUE : FALSE;
		}
#endif
#if IS_RTL88XX_MAC_V2_V3
		if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3){
			bResult = (pHalData->IntArray[3] & BIT_PWR_INT_127_V1) ? TRUE : FALSE;
		}
#endif
		}
		break;
#endif //#if CFG_HAL_HW_DETEC_POWER_STATE

#if CFG_HAL_SUPPORT_TXDESC_IE
    case HAL_INT_TYPE_TXDESC_TRANS_ERR:
        bResult = (pHalData->IntArray[3] & BIT_PKT_TRANS_ERR) ? TRUE : FALSE;
    break;
#endif

    //4 // ========== DWORD 8 ==========
#if CFG_HAL_SUPPORT_AXI_BUS_EXCEPTION
    case HAL_INT_TYPE_AXI_EXCEPTION:
    	bResult = (pHalData->IntArray[8] & BIT_AXI_EXCEPT_HINT) ? TRUE : FALSE;
		break;
#endif // CFG_HAL_SUPPORT_AXI_BUS_EXCEPTION

#if IS_RTL8198F_SERIES || IS_RTL8814B_SERIES

    case HAL_INT_TYPE_BcnInt8:
#if IS_RTL8814B_SERIES
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)
        bResult = (pHalData->IntArray[5] & BIT_BCNDMAINT8) ? TRUE : FALSE;
#endif      
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
		bResult = (pHalData->IntArray[4] & BIT_BCN_DMA_INT8) ? TRUE : FALSE;
#endif        
        break;
    case HAL_INT_TYPE_BcnInt9:
#if IS_RTL8814B_SERIES        
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)
        bResult = (pHalData->IntArray[5] & BIT_BCNDMAINT9) ? TRUE : FALSE;
#endif            
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
		bResult = (pHalData->IntArray[4] & BIT_BCN_DMA_INT9) ? TRUE : FALSE;
#endif        
        break;
    case HAL_INT_TYPE_BcnInt10:
#if IS_RTL8814B_SERIES        
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)
        bResult = (pHalData->IntArray[5] & BIT_BCNDMAINT10) ? TRUE : FALSE;    
#endif            
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
		bResult = (pHalData->IntArray[4] & BIT_BCN_DMA_INT10) ? TRUE : FALSE;
#endif
        break;
    case HAL_INT_TYPE_BcnInt11:
#if IS_RTL8814B_SERIES        
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)
        bResult = (pHalData->IntArray[5] & BIT_BCNDMAINT11) ? TRUE : FALSE;
#endif           
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
		bResult = (pHalData->IntArray[4] & BIT_BCN_DMA_INT11) ? TRUE : FALSE;
#endif
        break;
    case HAL_INT_TYPE_BcnInt12:
#if IS_RTL8814B_SERIES        
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)
        bResult = (pHalData->IntArray[5] & BIT_BCNDMAINT12) ? TRUE : FALSE;
#endif     
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
		bResult = (pHalData->IntArray[4] & BIT_BCN_DMA_INT12) ? TRUE : FALSE;
#endif
        break;
    case HAL_INT_TYPE_BcnInt13:
#if IS_RTL8814B_SERIES        
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)
        bResult = (pHalData->IntArray[5] & BIT_BCNDMAINT13) ? TRUE : FALSE;
#endif           
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
		bResult = (pHalData->IntArray[4] & BIT_BCN_DMA_INT13) ? TRUE : FALSE;
#endif
        break;
    case HAL_INT_TYPE_BcnInt14:
#if IS_RTL8814B_SERIES        
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)
        bResult = (pHalData->IntArray[5] & BIT_BCNDMAINT14) ? TRUE : FALSE;
#endif          
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
		bResult = (pHalData->IntArray[4] & BIT_BCN_DMA_INT14) ? TRUE : FALSE;
#endif
        break;
    case HAL_INT_TYPE_BcnInt15:
#if IS_RTL8814B_SERIES        
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)
        bResult = (pHalData->IntArray[5] & BIT_BCNDMAINT15) ? TRUE : FALSE;
#endif            
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
		bResult = (pHalData->IntArray[4] & BIT_BCN_DMA_INT15) ? TRUE : FALSE;
#endif
        break;    

    case HAL_INT_TYPE_TXBCN8OK:
#if IS_RTL8814B_SERIES        
        if(IS_HARDWARE_TYPE_8814B(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TXBCN8OK) ? TRUE : FALSE;
#endif            
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TX_BCN_OK_INT8) ? TRUE : FALSE;
#endif
        break;
    case HAL_INT_TYPE_TXBCN9OK:
#if IS_RTL8814B_SERIES        
        if(IS_HARDWARE_TYPE_8814B(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TXBCN9OK) ? TRUE : FALSE;
#endif            
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TX_BCN_OK_INT9) ? TRUE : FALSE;
#endif
        break;
    case HAL_INT_TYPE_TXBCN10OK:
#if IS_RTL8814B_SERIES        
        if(IS_HARDWARE_TYPE_8814B(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TXBCN10OK) ? TRUE : FALSE;
#endif            
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TX_BCN_OK_INT10) ? TRUE : FALSE;
#endif
        break;
    case HAL_INT_TYPE_TXBCN11OK:
#if IS_RTL8814B_SERIES        
        if(IS_HARDWARE_TYPE_8814B(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TXBCN11OK) ? TRUE : FALSE;
#endif            
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TX_BCN_OK_INT11) ? TRUE : FALSE;
#endif
        break;
    case HAL_INT_TYPE_TXBCN12OK:
#if IS_RTL8814B_SERIES        
        if(IS_HARDWARE_TYPE_8814B(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TXBCN12OK) ? TRUE : FALSE;
#endif    
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TX_BCN_OK_INT12) ? TRUE : FALSE;
#endif
        break;    
    case HAL_INT_TYPE_TXBCN13OK:
#if IS_RTL8814B_SERIES        
        if(IS_HARDWARE_TYPE_8814B(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TXBCN13OK) ? TRUE : FALSE;
#endif            
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TX_BCN_OK_INT13) ? TRUE : FALSE;
#endif
        break;    
    case HAL_INT_TYPE_TXBCN14OK:
#if IS_RTL8814B_SERIES        
        if(IS_HARDWARE_TYPE_8814B(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TXBCN14OK) ? TRUE : FALSE;
#endif            
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TX_BCN_OK_INT14) ? TRUE : FALSE;
#endif
        break;
    case HAL_INT_TYPE_TXBCN15OK:
#if IS_RTL8814B_SERIES        
        if(IS_HARDWARE_TYPE_8814B(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TXBCN15OK) ? TRUE : FALSE;
#endif            
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TX_BCN_OK_INT15) ? TRUE : FALSE;
#endif
        break;    
        
    case HAL_INT_TYPE_TXBCN8ERR:
#if IS_RTL8814B_SERIES        
        if(IS_HARDWARE_TYPE_8814B(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TXBCN8ERR) ? TRUE : FALSE;
#endif            
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TX_BCN_ERR_INT8) ? TRUE : FALSE;
#endif
        break;
    case HAL_INT_TYPE_TXBCN9ERR:
#if IS_RTL8814B_SERIES        
        if(IS_HARDWARE_TYPE_8814B(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TXBCN9ERR) ? TRUE : FALSE;
#endif            
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TX_BCN_ERR_INT9) ? TRUE : FALSE;
#endif
        break;
    case HAL_INT_TYPE_TXBCN10ERR:
#if IS_RTL8814B_SERIES        
        if(IS_HARDWARE_TYPE_8814B(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TXBCN10ERR) ? TRUE : FALSE;
#endif            
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TX_BCN_ERR_INT10) ? TRUE : FALSE;
#endif
        break;
    case HAL_INT_TYPE_TXBCN11ERR:
#if IS_RTL8814B_SERIES        
        if(IS_HARDWARE_TYPE_8814B(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TXBCN11ERR) ? TRUE : FALSE;
#endif            
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TX_BCN_ERR_INT11) ? TRUE : FALSE;
#endif
        break;
    case HAL_INT_TYPE_TXBCN12ERR:
#if IS_RTL8814B_SERIES        
        if(IS_HARDWARE_TYPE_8814B(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TXBCN12ERR) ? TRUE : FALSE;

#endif            
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TX_BCN_ERR_INT12) ? TRUE : FALSE;
#endif
        break;    
    case HAL_INT_TYPE_TXBCN13ERR:
#if IS_RTL8814B_SERIES        
        if(IS_HARDWARE_TYPE_8814B(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TXBCN13ERR) ? TRUE : FALSE;
#endif            
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TX_BCN_ERR_INT13) ? TRUE : FALSE;
#endif
        break;    
    case HAL_INT_TYPE_TXBCN14ERR:
#if IS_RTL8814B_SERIES        
        if(IS_HARDWARE_TYPE_8814B(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TXBCN14ERR) ? TRUE : FALSE;
#endif            
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TX_BCN_ERR_INT14) ? TRUE : FALSE;
#endif
        break;
    case HAL_INT_TYPE_TXBCN15ERR:
#if IS_RTL8814B_SERIES        
        if(IS_HARDWARE_TYPE_8814B(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TXBCN15ERR) ? TRUE : FALSE;
#endif            
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
        bResult = (pHalData->IntArray[4] & BIT_TX_BCN_ERR_INT15) ? TRUE : FALSE;
#endif
        break;
    
#endif //#if IS_RTL8198F_SERIES || IS_RTL8814B_SERIES

#if CFG_HAL_TX_AMSDU
    

    case HAL_INT_TYPE_FS_TIMEOUT0:
#if (IS_RTL8197F_SERIES) || (IS_RTL8198F_SERIES) || (IS_RTL8192F_SERIES) || (IS_RTL8814A_SERIES) || (IS_RTL8814B_SERIES) || (IS_RTL8821C_SERIES) || (IS_RTL8822B_SERIES) || (IS_RTL8822C_SERIES) || (IS_RTL8812F_SERIES)
		if (IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8192F(Adapter) || IS_HARDWARE_TYPE_8814A(Adapter) ||
			IS_HARDWARE_TYPE_8814B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter))
        	bResult = (pHalData->FtIntArray & BIT_FS_PS_TIMEOUT0_EN) ? TRUE : FALSE;
#endif
        break;
#endif

	}

	return bResult;
}


// TODO: Pedro, we can set several IMR combination for different scenario. Ex: 1) AP, 2) Client, 3) ....
// TODO: this can avoid to check non-necessary interrupt in __wlan_interrupt(..)...
VOID
AddInterruptMask88XX(
	IN	HAL_PADAPTER	Adapter,
	IN	HAL_INT_TYPE	intType
	)
{
	HAL_DATA_TYPE	*pHalData   = _GET_HAL_DATA(Adapter);
    u4Byte bit_pstimeout,bit_pstimeout1,bit_pstimeout2;
    u4Byte bit_VIdok,bit_VOdok,bit_BEdok,bit_BKdok,bit_MGTdok,bit_HIGHdok;
#if IS_RTL88XX_MAC_V1_V2_V3
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3){
        bit_pstimeout   = BIT_PSTIMEOUT_MSK;
        bit_pstimeout1  = BIT_TIMEOUT_INTERRUTP1_MASK;        
        bit_pstimeout2  = BIT_TIMEOUT_INTERRUPT2_MASK;
        bit_HIGHdok     = BIT_HIGHDOK_MSK;   
        bit_MGTdok      = BIT_MGTDOK_MSK;
        bit_BKdok       = BIT_BKDOK_MSK;
        bit_BEdok       = BIT_BEDOK_MSK;        
        bit_VIdok       = BIT_VIDOK_MSK;
        bit_VOdok       = BIT_VODOK_MSK;                            
    }
#endif
#if IS_RTL88XX_MAC_V4
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
        bit_pstimeout   = BIT_PSTIMER_0_MSK;
        bit_pstimeout1  = BIT_PSTIMER_1_MSK;
        bit_pstimeout2  = BIT_PSTIMER_2_MSK;
        bit_HIGHdok     = BIT_TXDMAOK_CHANNEL15_MSK; 
        bit_MGTdok      = BIT_TXDMAOK_CHANNEL14_MSK;
        bit_BKdok       = BIT_TXDMAOK_CHANNEL3_MSK;
        bit_BEdok       = BIT_TXDMAOK_CHANNEL2_MSK;
        bit_VIdok       = BIT_TXDMAOK_CHANNEL1_MSK;
        bit_VOdok       = BIT_TXDMAOK_CHANNEL0_MSK;                                 
    }
#endif
	switch(intType)
	{
    	default:
    		// Unknown interrupt type, no need to alarm because this IC may not
    		// support this interrupt.
    		RT_TRACE_F(COMP_SYSTEM, DBG_WARNING, ("Unkown intType: %d!\n", intType));
    		break;

    	case HAL_INT_TYPE_ANY:
            pHalData->IntMask[0] = 0xFFFFFFFF;
            pHalData->IntMask[1] = 0xFFFFFFFF;
    		break;

    	//4 // ========== DWORD 0 ==========
    	case HAL_INT_TYPE_BCNDERR0:
            pHalData->IntMask[0] |= BIT_BCNDERR0;
    		break;
            
    	case HAL_INT_TYPE_TBDOK:
    		pHalData->IntMask[0] |= BIT_TXBCN0OK;
    		break;
    		
    	case HAL_INT_TYPE_TBDER:
            pHalData->IntMask[0] |= BIT_TXBCN0ERR;
    		break;
    		
        case HAL_INT_TYPE_GTIMER4:
            pHalData->IntMask[0] |= BIT_GTINT4;
            break;
    	case HAL_INT_TYPE_BcnInt:
    		pHalData->IntMask[0] |= BIT_BCNDMAINT0;
    		break;
    		
    	case HAL_INT_TYPE_PSTIMEOUT:
            pHalData->IntMask[0] |= bit_pstimeout;
    		break;

    	case HAL_INT_TYPE_PSTIMEOUT1:
    		pHalData->IntMask[0] |= bit_pstimeout1;
    		break;

    	case HAL_INT_TYPE_PSTIMEOUT2:
    		pHalData->IntMask[0] |= bit_pstimeout2;
    		break; 
    		
    	case HAL_INT_TYPE_C2HCMD:
    		pHalData->IntMask[0] |= BIT_C2HCMD;
    		break;	
    		
    	case HAL_INT_TYPE_VIDOK:
    		pHalData->IntMask[0] |= bit_VIdok;
    		break;
    		
    	case HAL_INT_TYPE_VODOK:
    		pHalData->IntMask[0] |= bit_VOdok;
    		break;
    		
    	case HAL_INT_TYPE_BEDOK:
    		pHalData->IntMask[0] |= bit_BEdok;
    		break;
    		
    	case HAL_INT_TYPE_BKDOK:
    		pHalData->IntMask[0] |= bit_BKdok;
    		break;

    	case HAL_INT_TYPE_MGNTDOK:
    		pHalData->IntMask[0] |= bit_MGTdok;
    		break;

    	case HAL_INT_TYPE_HIGHDOK:
    		pHalData->IntMask[0] |= bit_HIGHdok;
    		break;

        #if 0   //Filen: removed
    	case HAL_INT_TYPE_BDOK:
            pHalData->IntMask[0] |= IMR_BCNDOK0_88E;
    		break;
        #endif
    		
    	case HAL_INT_TYPE_CPWM:
    		pHalData->IntMask[0] |= BIT_CPWM;
    		break;

    	case HAL_INT_TYPE_TSF_BIT32_TOGGLE:
    		pHalData->IntMask[0] |= BIT_TSF_BIT32_TOGGLE;
    		break;

        case HAL_INT_TYPE_RX_OK:
    		pHalData->IntMask[0] |= BIT_RXOK;
            break;

        case HAL_INT_TYPE_RDU:
    		pHalData->IntMask[0] |= BIT_RDU;
            break;
        case HAL_INT_TYPE_CPWM2:
            pHalData->IntMask[0] |= BIT_CPWM2;
            break;

#if IS_RTL88XX_MAC_V4
		case HAL_INT_TXDMA_START:
			pHalData->IntMask[0] |= BIT_TXDMA_START_INT_MSK;
			break;

		case HAL_INT_TXDMA_STOP:
			pHalData->IntMask[0] |= BIT_TXDMA_STOP_INT_MSK;
			break;
#endif

    	//4 // ========== DWORD 1 ==========
    	case HAL_INT_TYPE_RXFOVW:
    		pHalData->IntMask[1] |= BIT_FOVW;
    		break;

        case HAL_INT_TYPE_TXFOVW:
            pHalData->IntMask[1] |= BIT_TXFOVW;
            break;

        case HAL_INT_TYPE_RXERR:
            pHalData->IntMask[1] |= BIT_RXERR_INT;
            break;
            
        case HAL_INT_TYPE_TXERR:
            pHalData->IntMask[1] |= BIT_TXERR_INT;
            break;
        //4 // ========== DWORD 2 ==========

        //4 // ========== DWORD 3 ==========
#if CFG_HAL_HW_DETEC_POWER_STATE
		case HAL_INT_TYPE_PwrInt0:
#ifdef IS_EXIST_RTL8192FE
			if ( IS_HARDWARE_TYPE_8192FE(Adapter) )
				pHalData->IntMask[1] |= BIT_PWR_INT_31TO0;
			else
#endif
			pHalData->IntMask[3] |= BIT_PWR_INT_31TO0;
			break;
		case HAL_INT_TYPE_PwrInt1:
#ifdef IS_EXIST_RTL8192FE
			if ( IS_HARDWARE_TYPE_8192FE(Adapter) )
				pHalData->IntMask[1] |= BIT_PWR_INT_63TO32;
			else
#endif
			pHalData->IntMask[3] |= BIT_PWR_INT_63TO32;
			break;
		case HAL_INT_TYPE_PwrInt2:
#ifdef IS_EXIST_RTL8192FE
			if ( IS_HARDWARE_TYPE_8192FE(Adapter) )
				pHalData->IntMask[1] |= BIT_PWR_INT_95TO64;
			else
#endif
			pHalData->IntMask[3] |= BIT_PWR_INT_95TO64;
			break;
		case HAL_INT_TYPE_PwrInt3:
#ifdef IS_EXIST_RTL8192FE
			if ( IS_HARDWARE_TYPE_8192FE(Adapter) )
				pHalData->IntMask[1] |= BIT_PWR_INT_126TO96;			
			else
#endif
			{
#if IS_RTL88XX_MAC_V2_V3
				if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3) 		   
					pHalData->IntMask[3] |= BIT_PWR_INT_126TO96;			
#endif
#if IS_RTL88XX_MAC_V4
				if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)
					pHalData->IntMask[3] |= BIT_PWR_INT_127TO96; 
#endif
			}
			break;
		case HAL_INT_TYPE_PwrInt4:
#ifdef IS_EXIST_RTL8192FE
			if ( IS_HARDWARE_TYPE_8192FE(Adapter) )
				pHalData->IntMask[1] |= BIT_PWR_INT_127;			
			else
#endif
			{
#if IS_RTL88XX_MAC_V4
				if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
					pHalData->IntMask[3] |= BIT_SEARCH_FAIL;
				}
#endif
#if IS_RTL88XX_MAC_V2_V3
				if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3){
					pHalData->IntMask[3] |= BIT_PWR_INT_127_V1; 	 
				}
#endif
			}
			break;
#endif // #if CFG_HAL_HW_DETEC_POWER_STATE
        //4 // ========== DWORD 4 ==========
#if CFG_HAL_SUPPORT_AXI_BUS_EXCEPTION
        case HAL_INT_TYPE_AXI_EXCEPTION:
            pHalData->IntMask[8] |= BIT_AXI_EXCEPT_HINT;
        break;
#endif // CFG_HAL_SUPPORT_AXI_BUS_EXCEPTION

#if CFG_HAL_TX_AMSDU
        case HAL_INT_TYPE_FS_TIMEOUT0:
#if (IS_RTL8197F_SERIES) || (IS_RTL8198F_SERIES) || (IS_RTL8192F_SERIES) || (IS_RTL8814A_SERIES) || (IS_RTL8814B_SERIES) || (IS_RTL8821C_SERIES) || (IS_RTL8822B_SERIES) || (IS_RTL8822C_SERIES) || (IS_RTL8812F_SERIES)
			if (IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8192F(Adapter) || IS_HARDWARE_TYPE_8814A(Adapter) ||
				IS_HARDWARE_TYPE_8814B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter))
            pHalData->FtIntArray |= BIT_FS_PS_TIMEOUT0_EN;
#endif
            break;
#endif
	}
}


VOID
RemoveInterruptMask88XX(
	IN	HAL_PADAPTER	Adapter,
	IN	HAL_INT_TYPE	intType
	)
{
	HAL_DATA_TYPE	*pHalData   = _GET_HAL_DATA(Adapter);
    u4Byte bit_pstimeout,bit_pstimeout1,bit_pstimeout2;
    u4Byte bit_VIdok,bit_VOdok,bit_BEdok,bit_BKdok,bit_MGTdok,bit_HIGHdok;
#if IS_RTL88XX_MAC_V1_V2_V3
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3){
        bit_pstimeout   = BIT_PSTIMEOUT_MSK;
        bit_pstimeout1  = BIT_TIMEOUT_INTERRUTP1_MASK;
        bit_pstimeout2  = BIT_TIMEOUT_INTERRUPT2_MASK;
        bit_HIGHdok     = BIT_HIGHDOK_MSK;   
        bit_MGTdok      = BIT_MGTDOK_MSK;
        bit_BKdok       = BIT_BKDOK_MSK;
        bit_BEdok       = BIT_BEDOK_MSK;        
        bit_VIdok       = BIT_VIDOK_MSK;
        bit_VOdok       = BIT_VODOK_MSK;                            
    }
#endif
#if IS_RTL88XX_MAC_V4
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
        bit_pstimeout   = BIT_PSTIMER_0_MSK;
        bit_pstimeout1  = BIT_PSTIMER_1_MSK;
        bit_pstimeout2  = BIT_PSTIMER_2_MSK;
        bit_HIGHdok     = BIT_TXDMAOK_CHANNEL15_MSK; 
        bit_MGTdok      = BIT_TXDMAOK_CHANNEL14_MSK;
        bit_BKdok       = BIT_TXDMAOK_CHANNEL3_MSK;
        bit_BEdok       = BIT_TXDMAOK_CHANNEL2_MSK;
        bit_VIdok       = BIT_TXDMAOK_CHANNEL1_MSK;
        bit_VOdok       = BIT_TXDMAOK_CHANNEL0_MSK;                                 
    }
#endif
	switch(intType)
	{
    	default:
    		// Unknown interrupt type, no need to alarm because this IC may not
    		// support this interrupt.
    		RT_TRACE_F(COMP_SYSTEM, DBG_WARNING, ("Unkown intType: %d!\n", intType));
    		break;

    	case HAL_INT_TYPE_ANY:
            pHalData->IntMask[0] &= ~0xFFFFFFFF;
            pHalData->IntMask[1] &= ~0xFFFFFFFF;
    		break;

    	//4 // ========== DWORD 0 ==========
    	case HAL_INT_TYPE_BCNDERR0:
            pHalData->IntMask[0] &= ~BIT_BCNDERR0;
    		break;
            
    	case HAL_INT_TYPE_TBDOK:
    		pHalData->IntMask[0] &= ~BIT_TXBCN0OK;
    		break;
    		
    	case HAL_INT_TYPE_TBDER:
            pHalData->IntMask[0] &= ~BIT_TXBCN0ERR;
    		break;
    		
        case HAL_INT_TYPE_GTIMER4:
            pHalData->IntMask[0] &= ~BIT_GTINT4;
            break;
    	case HAL_INT_TYPE_BcnInt:
    		pHalData->IntMask[0] &= ~BIT_BCNDMAINT0;
    		break;
    		
    	case HAL_INT_TYPE_PSTIMEOUT:
            pHalData->IntMask[0] &= ~bit_pstimeout;
    		break;

    	case HAL_INT_TYPE_PSTIMEOUT1:
    		pHalData->IntMask[0] &= ~bit_pstimeout1;
    		break;

    	case HAL_INT_TYPE_PSTIMEOUT2:
    		pHalData->IntMask[0] &= ~bit_pstimeout2;
    		break; 
    		
    	case HAL_INT_TYPE_C2HCMD:
    		pHalData->IntMask[0] &= ~BIT_C2HCMD;
    		break;	
    		
    	case HAL_INT_TYPE_VIDOK:
    		pHalData->IntMask[0] &= ~bit_VIdok;
    		break;
    		
    	case HAL_INT_TYPE_VODOK:
    		pHalData->IntMask[0] &= ~bit_VOdok;
    		break;
    		
    	case HAL_INT_TYPE_BEDOK:
    		pHalData->IntMask[0] &= ~bit_BEdok;
    		break;
    		
    	case HAL_INT_TYPE_BKDOK:
    		pHalData->IntMask[0] &= ~bit_BKdok;
    		break;

    	case HAL_INT_TYPE_MGNTDOK:
    		pHalData->IntMask[0] &= ~bit_MGTdok;
    		break;

    	case HAL_INT_TYPE_HIGHDOK:
    		pHalData->IntMask[0] &= ~bit_HIGHdok;
    		break;

        #if 0   //Filen: removed
    	case HAL_INT_TYPE_BDOK:
            pHalData->IntMask[0] &= ~IMR_BCNDOK0_88E;
    		break;
        #endif
    		
    	case HAL_INT_TYPE_CPWM:
    		pHalData->IntMask[0] &= ~BIT_CPWM;
    		break;

    	case HAL_INT_TYPE_TSF_BIT32_TOGGLE:
    		pHalData->IntMask[0] &= ~BIT_TSF_BIT32_TOGGLE;
    		break;

        case HAL_INT_TYPE_RX_OK:
    		pHalData->IntMask[0] &= ~BIT_RXOK;
            break;

        case HAL_INT_TYPE_RDU:
    		pHalData->IntMask[0] &= ~BIT_RDU;
            break;            
        case HAL_INT_TYPE_CPWM2:
            pHalData->IntMask[0] &= ~BIT_CPWM2;
            break;

#if IS_RTL88XX_MAC_V4
		case HAL_INT_TXDMA_START:
			pHalData->IntMask[0] &= ~BIT_TXDMA_START_INT_MSK;
			break;

		case HAL_INT_TXDMA_STOP:
			pHalData->IntMask[0] &= ~BIT_TXDMA_STOP_INT_MSK;
			break;
#endif

    	//4 // ========== DWORD 1 ==========
    	case HAL_INT_TYPE_RXFOVW:
    		pHalData->IntMask[1] &= ~BIT_FOVW;
    		break;

        case HAL_INT_TYPE_TXFOVW:
            pHalData->IntMask[1] &= ~BIT_TXFOVW;
            break;

        case HAL_INT_TYPE_RXERR:
            pHalData->IntMask[1] &= ~BIT_RXERR_INT;
            break;
            
        case HAL_INT_TYPE_TXERR:
            pHalData->IntMask[1] &= ~BIT_TXERR_INT;
            break;
        //4 // ========== DWORD 2 ==========
        
        //4 // ========== DWORD 3 ==========
#if CFG_HAL_HW_DETEC_POWER_STATE
		case HAL_INT_TYPE_PwrInt0:
#ifdef IS_EXIST_RTL8192FE
			if ( IS_HARDWARE_TYPE_8192FE(Adapter) )
				pHalData->IntMask[1] &= ~BIT_PWR_INT_31TO0;
			else
#endif
			pHalData->IntMask[3] &= ~BIT_PWR_INT_31TO0;
			break;
		case HAL_INT_TYPE_PwrInt1:
#ifdef IS_EXIST_RTL8192FE
			if ( IS_HARDWARE_TYPE_8192FE(Adapter) )
				pHalData->IntMask[1] &= ~BIT_PWR_INT_63TO32;
			else
#endif
			pHalData->IntMask[3] &= ~BIT_PWR_INT_63TO32;
			break;
		case HAL_INT_TYPE_PwrInt2:
#ifdef IS_EXIST_RTL8192FE
			if ( IS_HARDWARE_TYPE_8192FE(Adapter) )
				pHalData->IntMask[1] &= ~BIT_PWR_INT_95TO64;
			else
#endif
			pHalData->IntMask[3] &= ~BIT_PWR_INT_95TO64;
			break;	
		case HAL_INT_TYPE_PwrInt3:
#ifdef IS_EXIST_RTL8192FE
			if ( IS_HARDWARE_TYPE_8192FE(Adapter) )
				pHalData->IntMask[1] &= ~BIT_PWR_INT_126TO96;			 
			else
#endif
			{
#if IS_RTL88XX_MAC_V4
				if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
					pHalData->IntMask[3] &= ~BIT_PWR_INT_127TO96;
				}
#endif
#if IS_RTL88XX_MAC_V2_V3
				if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3){
					pHalData->IntMask[3] &= ~BIT_PWR_INT_126TO96; 
				}
#endif
			}
			break;
		case HAL_INT_TYPE_PwrInt4:
#ifdef IS_EXIST_RTL8192FE
			if ( IS_HARDWARE_TYPE_8192FE(Adapter) )
				pHalData->IntMask[1] &= ~BIT_PWR_INT_127;			 
			else
#endif
			{
#if IS_RTL88XX_MAC_V4
				if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
					pHalData->IntMask[3] &= ~BIT_SEARCH_FAIL;
				}
#endif
#if IS_RTL88XX_MAC_V2_V3
				if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3){
					pHalData->IntMask[3] &= ~BIT_PWR_INT_127_V1;	 
				}
#endif
			}
			break;
#endif // #if CFG_HAL_HW_DETEC_POWER_STATE
        //4 // ========== DWORD 4 ==========
#if CFG_HAL_SUPPORT_AXI_BUS_EXCEPTION
        case HAL_INT_TYPE_AXI_EXCEPTION:
            pHalData->IntMask[8] &= ~BIT_AXI_EXCEPT_HINT;
        break;
#endif // CFG_HAL_SUPPORT_AXI_BUS_EXCEPTION

#if CFG_HAL_TX_AMSDU
        case HAL_INT_TYPE_FS_TIMEOUT0:
#if (IS_RTL8197F_SERIES) || (IS_RTL8198F_SERIES) || (IS_RTL8192F_SERIES) || (IS_RTL8814A_SERIES) || (IS_RTL8814B_SERIES) || (IS_RTL8821C_SERIES) || (IS_RTL8822B_SERIES) || (IS_RTL8822C_SERIES) || (IS_RTL8812F_SERIES)
			if (IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8192F(Adapter) || IS_HARDWARE_TYPE_8814A(Adapter) ||
				IS_HARDWARE_TYPE_8814B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter))
            pHalData->FtIntArray &= ~BIT_FS_PS_TIMEOUT0_EN;
#endif
            break;
#endif

	}
}


HAL_IMEM
VOID
DisableRxRelatedInterrupt88XX(
	IN	HAL_PADAPTER	Adapter
    )
{
    PHAL_DATA_TYPE      pHalData = _GET_HAL_DATA(Adapter);
    HAL_PADAPTER        priv     = Adapter;
    ULONG               flags;

#if 0
    HAL_SAVE_INT_AND_CLI(flags);

    pHalData->IntMask_RxINTBackup[0] = pHalData->IntMask[0];
    pHalData->IntMask_RxINTBackup[1] = pHalData->IntMask[1];

    pHalData->IntMask[0] &= ~BIT_RXOK;
    pHalData->IntMask[1] &= ~BIT_FOVW;

    HAL_RESTORE_INT(flags);

    HAL_RTL_W32(REG_HIMR0, pHalData->IntMask[0]);
    HAL_RTL_W32(REG_HIMR1, pHalData->IntMask[1]);
#else
    HAL_RTL_W8(REG_HIMR0, (pHalData->IntMask[0] & ~ (BIT_RXOK | BIT_RDU)) && 0xf);
    HAL_RTL_W32(REG_HIMR1, pHalData->IntMask[1] & ~BIT_FOVW);
#endif

}

HAL_IMEM
VOID
EnableRxRelatedInterrupt88XX(
	IN	HAL_PADAPTER	Adapter
    )
{
    PHAL_DATA_TYPE      pHalData    = _GET_HAL_DATA(Adapter);
    HAL_PADAPTER        priv        = Adapter;
    ULONG               flags;

#if 0
    HAL_SAVE_INT_AND_CLI(flags);

    pHalData->IntMask[0] = pHalData->IntMask_RxINTBackup[0];
    pHalData->IntMask[1] = pHalData->IntMask_RxINTBackup[1];

    HAL_RESTORE_INT(flags);
#endif
    HAL_RTL_W8(REG_HIMR0, pHalData->IntMask[0] && 0xf);
    HAL_RTL_W32(REG_HIMR1, pHalData->IntMask[1]);

}


