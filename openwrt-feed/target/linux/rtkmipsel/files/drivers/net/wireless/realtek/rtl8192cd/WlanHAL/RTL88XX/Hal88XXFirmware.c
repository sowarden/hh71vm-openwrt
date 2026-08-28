/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	Hal88XXFirmware.c
	
Abstract:
	Defined RTL88XX Firmware Related Function
	    
Major Change History:
	When       Who               What
	---------- ---------------   -------------------------------
	2012-04-11 Filen            Create.	
--*/
#if !defined(__ECOS) && !defined(CPTCFG_CFG80211_MODULE)
#include "HalPrecomp.h"
#else
#include "../HalPrecomp.h"
#endif
#if defined(__ECOS) || defined(CPTCFG_CFG80211_MODULE)
#include "halmac_type.h"
#include "halmac_fw_info.h"
#endif

#if (IS_RTL8881A_SERIES || IS_RTL8192E_SERIES || IS_RTL8814A_SERIES || IS_RTL8197F_SERIES || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
// c2h callback function register here
struct cmdobj	HalC2Hcmds[] = 
{
    {0, NULL},                                                 //0x0 not register yet 
    {0, NULL},                                                 //0x1 not register yet 
    {0, NULL},                                                 //0x2 not register yet 
    {0, NULL},                                                 //0x3 not register yet 
#ifdef TXREPORT	
	GEN_FW_CMD_HANDLER(sizeof(APREQTXRPT), APReqTXRpt)	    //0x4 APReqTXRptHandler
#else
    {0, NULL}
#endif
};
#endif // #if (IS_RTL8881A_SERIES || IS_RTL8192E_SERIES || IS_RTL8814A_SERIES || IS_RTL8197F_SERIES || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)


#if IS_RTL88XX_FW_MIPS
//
//3 Download Firmware
//

u2Byte checksum(u1Byte *idx, u4Byte len) //Cnt - count of bytes
{

	u4Byte	i;
	u4Byte	val, val_32;
	u1Byte	val_8[4];
 	u1Byte	checksum[4]={0,0,0,0};
	u4Byte	checksum32 = 0;

	for(i = 0; i< len; i+=4, idx+=4)
	{        
			val_8[0] =	*idx;
			val_8[1] =	*(idx+1);
			val_8[2] =	*(idx+2);
			val_8[3] =	*(idx+3);

		checksum[0] ^= val_8[0];
		checksum[1] ^= val_8[1];
		checksum[0] ^= val_8[2];
		checksum[1] ^= val_8[3]; 

	}
	checksum32 = checksum[0] |(checksum[1]<<8);		

	printk("sofetware check sum = %x !!!!!!!!!!!!!!!!!!\n ",checksum32 );
	
	return checksum32;
}



VOID
ReadMIPSFwHdr88XX(
    IN  HAL_PADAPTER    Adapter
)
{
    PRTL88XX_MIPS_FW_HDR pfw_hdr;
    pfw_hdr = (PRTL88XX_MIPS_FW_HDR)_GET_HAL_DATA(Adapter)->PFWHeader;

	Adapter->pshare->fw_signature= le16_to_cpu(pfw_hdr->signature);
	Adapter->pshare->fw_version = le16_to_cpu(pfw_hdr->version);
	//pfw_hdr->SVN_index = le32_to_cpu(pfw_hdr->SVN_index); 
	//pfw_hdr->Year = le16_to_cpu(pfw_hdr->Year); 

	Adapter->pshare->fw_category	= pfw_hdr->category;
	Adapter->pshare->fw_function	= pfw_hdr->function;
	Adapter->pshare->fw_sub_version = pfw_hdr->Subversion;
	Adapter->pshare->fw_date_month	= pfw_hdr->Month;
	Adapter->pshare->fw_date_day	= pfw_hdr->Date;
	Adapter->pshare->fw_date_hour	= pfw_hdr->Hour;
	Adapter->pshare->fw_date_minute = pfw_hdr->Min;
	
    RT_TRACE_F(COMP_INIT, DBG_TRACE ,("FW version = %x \n", pfw_hdr->version));
    RT_TRACE_F(COMP_INIT, DBG_TRACE ,("FW release at %x/%x \n",     pfw_hdr->Month, pfw_hdr->Date));    
}

VOID
ConfigBeforeDLFW(
    IN  HAL_PADAPTER    Adapter,
    IN  pu4Byte         temBuf    
)
{
    u4Byte tmpVal;
    u4Byte reg_bcn_crtl,reg_fifopage_ctrl;
    u1Byte bit_en_bcn_function;
#if IS_RTL88XX_MAC_V2_V3
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3) {
        reg_bcn_crtl = REG_BCN_CTRL;
        bit_en_bcn_function = BIT_EN_BCN_FUNCTION;
        reg_fifopage_ctrl = REG_FIFOPAGE_CTRL_2;
    }
#endif    
#if IS_RTL88XX_MAC_V4
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4) {
        reg_bcn_crtl = REG_PORT_CTRL_CFG;
     	bit_en_bcn_function = BIT_EN_PORT_FUNCTION;
        reg_fifopage_ctrl = REG_BCN_CTRL_0;
    }
#endif
	//Enable SW TX beacon
	temBuf[0] =  HAL_RTL_R32(REG_CR);
    HAL_RTL_W32(REG_CR,temBuf[0] | BIT_ENSWBCN);

	//Disable BIT_EN_BCN_FUNCTION
	temBuf[1] =  HAL_RTL_R32(reg_bcn_crtl);	
    tmpVal = (temBuf[1] | BIT_DIS_TSF_UDT) & (~bit_en_bcn_function);
    HAL_RTL_W32(reg_bcn_crtl,tmpVal);    

    temBuf[2] =  HAL_RTL_R32(REG_FWHW_TXQ_CTRL);  
    HAL_RTL_W32(REG_FWHW_TXQ_CTRL,temBuf[2] & ~BIT22);

    // set beacon header to page 0
    temBuf[3] =  HAL_RTL_R32(reg_fifopage_ctrl);   

    tmpVal = temBuf[3] & 0xF000;
    HAL_RTL_W32(reg_fifopage_ctrl,tmpVal);     

    // Disable MCU Core (CPU RST)
    temBuf[4] =  (u2Byte)HAL_RTL_R16(REG_SYS_FUNC_EN);  
    tmpVal = temBuf[4] & (~BIT_FEN_CPUEN);
    //tmpVal = tmpVal & (~BIT_FEN_BBRSTB);
    //tmpVal = tmpVal & (~BIT_FEN_BB_GLB_RSTn);
    HAL_RTL_W16(REG_SYS_FUNC_EN,tmpVal);    

    SetHwReg88XX(Adapter, HW_VAR_ENABLE_BEACON_DMA, NULL);

#ifdef ENABLE_PCIE_MULTI_TAG
	// Do nothing
#else
	// disable multi-tag
	HAL_RTL_W8(REG_HCI_MIX_CFG,0);
#endif

	tmpVal = HAL_RTL_R32(REG_MCUFW_CTRL);
	tmpVal = (tmpVal & (~0x7ff));
	tmpVal = (tmpVal & ~(BIT_FW_DW_RDY|BIT_FW_INIT_RDY));
#if IS_RTL8197F_SERIES || IS_RTL8198F_SERIES|| IS_RTL8197G_SERIES
    if(IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter))
	tmpVal = (tmpVal | BIT_MCU_FWDL_EN);
#endif
#if IS_RTL8814A_SERIES || IS_RTL8814B_SERIES
    if(IS_HARDWARE_TYPE_8814A(Adapter) || IS_HARDWARE_TYPE_8814B(Adapter))
	tmpVal = (tmpVal | BIT_MCUFWDL_EN);
#endif

	tmpVal = tmpVal & 0xFFFFCFFF;    
    
    if ( IS_HARDWARE_TYPE_8197F(Adapter)) {
        // set 3081 CPU clock to 40M
        tmpVal = tmpVal | BIT_CPU_CLK_SEL(1);
		RT_TRACE_F(COMP_INIT, DBG_TRACE ,("set 3081 CPU clock to 40M ! \n"));
    }
    else if ( IS_HARDWARE_TYPE_8814A(Adapter)) {
        // set 3081 CPU clock to 100M
        tmpVal = tmpVal | BIT_CPU_CLK_SEL(2);
		RT_TRACE_F(COMP_INIT, DBG_TRACE ,("set 3081 CPU clock to 100M ! \n"));
    }
    else if ( IS_HARDWARE_TYPE_8198F(Adapter)) {
        // set 3081 CPU clock to 80M
        tmpVal = tmpVal | BIT_CPU_CLK_SEL(1);
		RT_TRACE_F(COMP_INIT, DBG_TRACE ,("set 3081 CPU clock to 100M ! \n"));
    }
    else {
        //tmp set to 2
        tmpVal = tmpVal | BIT_CPU_CLK_SEL(2);
		RT_TRACE_F(COMP_INIT, DBG_TRACE ,("set 3081 CPU clock to ???M ! \n"));
    }



	HAL_RTL_W32(REG_MCUFW_CTRL,tmpVal);
	
	
#if IS_RTL8197F_SERIES
    if ( IS_HARDWARE_TYPE_8197F(Adapter)) {
    //WLAN platform reset before download fw, or 97F will have platform err INT
        HAL_RTL_W32(REG_CPU_DMEM_CON, HAL_RTL_R32(REG_CPU_DMEM_CON) & ~BIT(16));
        HAL_RTL_W32(REG_CPU_DMEM_CON, HAL_RTL_R32(REG_CPU_DMEM_CON) | BIT(16));
    }
#endif

}

VOID
RestoreAfterDLFW(
    IN  HAL_PADAPTER    Adapter,
    IN  pu4Byte         temBuf    
)
{
    u4Byte tmpVal,count = 0;
    u4Byte reg_bcn_crtl,reg_fifopage_ctrl;
#if IS_RTL88XX_MAC_V2_V3
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3) {
        reg_bcn_crtl = REG_BCN_CTRL;
        reg_fifopage_ctrl = REG_FIFOPAGE_CTRL_2;
    }
#endif    
#if IS_RTL88XX_MAC_V4
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4) {
        reg_bcn_crtl = REG_PORT_CTRL_CFG;
        reg_fifopage_ctrl = REG_BCN_CTRL_0;
    }
#endif

#if IS_EXIST_EMBEDDED || IS_EXIST_PCI
	if (Adapter->hci_type == RTL_HCI_PCIE) {
    	SetBeaconDownload88XX(Adapter, HW_VAR_BEACON_DISABLE_DOWNLOAD);
	}
#endif
    // Restore beacon header to page 0
    HAL_RTL_W32(reg_fifopage_ctrl,temBuf[3]);    
    
	//Restore SW TX beacon
    HAL_RTL_W32(REG_CR,temBuf[0]);

	//Restore BIT_EN_BCN_FUNCTION
    HAL_RTL_W32(reg_bcn_crtl,temBuf[1]);    

#if !CFG_HAL_MAC_LOOPBACK
    HAL_RTL_W32(REG_FWHW_TXQ_CTRL,temBuf[2]);
#endif  //#if !CFG_HAL_MAC_LOOPBACK


    // Restore MCU Core (CPU RST)
    HAL_RTL_W16(REG_SYS_FUNC_EN,(u2Byte)temBuf[4]);  

}

enum rt_status 
InitDDMA88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte	source,
    IN  u4Byte	dest,
    IN  u4Byte 	length
)
{
	// TODO: Replace all register define & bit define
	
	u4Byte	ch0ctrl = (DDMA_CHKSUM_EN|DDMA_CH_OWN );
	u4Byte	cnt=50000;
	u1Byte	tmp;
	
	//check if ddma ch0 is idle
	while(HAL_RTL_R32(REG_DDMA_CH0CTRL)&DDMA_CH_OWN){
		delay_ms(10);
		cnt--;
		if(cnt==0){			
            printk("1 InitDDMA88XX polling fail \n");
			return RT_STATUS_FAILURE;
		}
	}
	
	
	ch0ctrl |= length & DDMA_LEN_MASK;
	
	//check if chksum continuous
    ch0ctrl |= DDMA_CH_CHKSUM_CNT;
		
	HAL_RTL_W32(REG_DDMA_CH0SA, source);
	HAL_RTL_W32(REG_DDMA_CH0DA, dest);
	HAL_RTL_W32(REG_DDMA_CH0CTRL, ch0ctrl);

//check if ddma ch0 is idle
        while(HAL_RTL_R32(REG_DDMA_CH0CTRL)&DDMA_CH_OWN){
                delay_ms(10);
                cnt--;
                if(cnt==0){                       
            printk("2 InitDDMA88XX polling fail \n");
                        return RT_STATUS_FAILURE;
                }
        }

	return RT_STATUS_SUCCESS;
}

enum rt_status 
DLImageCheckSum(
    IN  HAL_PADAPTER    Adapter
)
{
    u4Byte	cnt=5000;
	u1Byte	tmp;
	
	//check if ddma ch0 is idle
	while(HAL_RTL_R32(REG_DDMA_CH0CTRL)&DDMA_CH_OWN){
		delay_ms(1);
		cnt--;
		if(cnt==0){			
            printk("CNT error \n");
			return RT_STATUS_FAILURE;
		}
	}

    if(HAL_RTL_R32(REG_DDMA_CH0CTRL)&DDMA_CHKSUM_FAIL)
    {    
        printk("DDMA_CHKSUM_FAIL error = %x\n",HAL_RTL_R16(REG_DDMA_CHKSUM));        
        return RT_STATUS_FAILURE;
    }
    else
    {  
        return RT_STATUS_SUCCESS;
    }              
}

enum rt_status
CheckMIPSFWImageSize(
    IN  HAL_PADAPTER    Adapter
)
{
    PRTL88XX_MIPS_FW_HDR pfw_hdr;
    u4Byte              imemSZ;
    u2Byte              dmemSZ;
    u4Byte imageSZ;
    

    pfw_hdr = (PRTL88XX_MIPS_FW_HDR)_GET_HAL_DATA(Adapter)->PFWHeader;

    // ReaddDMEM, check length
    dmemSZ = HAL_EF2Byte(pfw_hdr->FW_CFG_SZ)+MIPS_FW_CHKSUM_DUMMY_SZ;

    // ReadIMEM, check length
    imemSZ = HAL_EF4Byte(pfw_hdr->IRAM_SZ)+MIPS_FW_CHKSUM_DUMMY_SZ;

    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_FWFILE_SIZE, (pu1Byte)&imageSZ);

    if(imageSZ != (sizeof(RTL88XX_MIPS_FW_HDR)+ dmemSZ + imemSZ))
    {
        RT_TRACE_F(COMP_INIT, DBG_WARNING, 
            ("FW size error ! Ima = %d Cal =%d \n",imageSZ,(sizeof(RTL88XX_MIPS_FW_HDR)+ imemSZ + dmemSZ)));
        return RT_STATUS_FAILURE;        
    }
    else
    {
        return RT_STATUS_SUCCESS;
    }
}

#if CFG_FW_VERIFICATION
enum rt_status
VerifyRsvdPage88XX
(
    IN	HAL_PADAPTER        Adapter,
    IN  pu1Byte             pbuf,
    IN  u4Byte              frlen
)
{
    u4Byte i=0;
    u4Byte TXDESCSize;

    #if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE )    
    if ( IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8814AE(Adapter) ) 
        HAL_RTL_W8(REG_PKT_BUFF_ACCESS_CTRL,0x69);
    #endif

    #if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM)
    if ( IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8814AE(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) ) {
        TXDESCSize = SIZE_TXDESC_88XX;
    }
	#endif  //#if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM)

	#if (IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE || IS_EXIST_RTL8197GEM)
    if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter))  {
        TXDESCSize = SIZE_TXDESC_88XX_V1;
    }
	#endif //IS_EXIST_RTL8822BE

    #if (IS_EXIST_RTL8814BE) 
        if ( IS_HARDWARE_TYPE_8814B(Adapter)) {
            TXDESCSize = SIZE_TXDESC_88XX_V2_BCN;
        }
    #endif //IS_EXIST_RTL8814BE

    // compare payload download to TXPKTBUF
    for(i=0;i<frlen;i++)
    {
	    HAL_RTL_W32(REG_PKTBUF_DBG_CTRL,0x780+((i+TXDESCSize)>>12));    		
        if(HAL_RTL_R8(0x8000+i+TXDESCSize) != *((pu1Byte)pbuf+i))
        {
            RT_TRACE_F(COMP_INIT, DBG_WARNING, 
            ("Verify DLtxbuf fail at %x A=%x B=%x \n",i,HAL_RTL_R8(0x8000+i+TXDESCSize),*((pu1Byte)pbuf+i)));

            return RT_STATUS_FAILURE;
        }        
    }

    return RT_STATUS_SUCCESS;
}
#endif //#if CFG_FW_VERIFICATION

enum rt_status
DLtoTXBUFandDDMA88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  pu1Byte         pbuf,    
    IN  u4Byte          len,        
    IN  u4Byte          offset,
    IN  u1Byte          target
)
{
    u4Byte  TXDESCSize;
    
        
#if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM)
        if ( IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8814AE(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter)) {
            TXDESCSize = SIZE_TXDESC_88XX;
        }
#endif  //#if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM )
    
#if (IS_EXIST_RTL8822BE || IX_EXIST_RTL8821CE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE|| IS_EXIST_RTL8197GEM)
        if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter))  {
            TXDESCSize = SIZE_TXDESC_88XX_V1;
        }
#endif //IS_EXIST_RTL8822BE

#if (IS_EXIST_RTL8814BE)
        if ( IS_HARDWARE_TYPE_8814B(Adapter) )  {
            TXDESCSize = SIZE_TXDESC_88XX_V2_BCN;
        }
#endif //IS_EXIST_RTL8814BE)

#if IS_EXIST_EMBEDDED || IS_EXIST_PCI
	if (Adapter->hci_type == RTL_HCI_PCIE) {
    	SigninBeaconTXBD88XX(Adapter,(pu4Byte)pbuf, len);    // fill TXDESC & TXBD
	}
#endif
    //RT_PRINT_DATA(COMP_INIT, DBG_LOUD, "DLtoTXBUFandDDMA88XX\n", pbuf, len);

#if IS_EXIST_EMBEDDED || IS_EXIST_PCI
	if (Adapter->hci_type == RTL_HCI_PCIE) {
	    if(_FALSE == DownloadRsvdPage88XX(Adapter,NULL,0,0)) {
	        RT_TRACE_F(COMP_INIT, DBG_WARNING,("Download to TXpktbuf fail ! \n"));
	        return RT_STATUS_FAILURE;
	    }
	}
#endif

#if CFG_FW_VERIFICATION
    if(RT_STATUS_SUCCESS != VerifyRsvdPage88XX(Adapter,pbuf, len)) {
          RT_TRACE_F(COMP_INIT, DBG_WARNING,("VerifyRsvdPage88XX fail ! \n"));        
          return RT_STATUS_FAILURE;
    }
#endif //#if CFG_FW_VERIFICATION    

    if(target == MIPS_DL_DMEM) // DMEM
    {
        if(RT_STATUS_SUCCESS != InitDDMA88XX(Adapter,OCPBASE_TXBUF+TXDESCSize,OCPBASE_DMEM+offset,len)) {
            RT_TRACE_F(COMP_INIT, DBG_WARNING,("DDMA to DMEM fail ! \n"));        
            return RT_STATUS_FAILURE;
        }      
    }else {
        if(RT_STATUS_SUCCESS != InitDDMA88XX(Adapter,OCPBASE_TXBUF+TXDESCSize,OCPBASE_IMEM+offset,len)) {
            RT_TRACE_F(COMP_INIT, DBG_WARNING,("DDMA to IMEM fail ! \n"));                    
            return RT_STATUS_FAILURE;
        }      
    }

    return RT_STATUS_SUCCESS;
}


enum rt_status
checkFWSizeAndDLFW88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  pu1Byte         pbuf,
    IN  u4Byte          len,    
    IN  u1Byte          target 
)
{
    u4Byte                  downloadcnt = 0;    
    u4Byte                  offset = 0;          

    while(1){
        if(len > MIPS_MAX_FWBLOCK_DL_SIZE){

            // the offset in DMEM
            offset = downloadcnt*MIPS_MAX_FWBLOCK_DL_SIZE;

            // download len = limitation length
            if(RT_STATUS_SUCCESS != DLtoTXBUFandDDMA88XX(Adapter,pbuf,MIPS_MAX_FWBLOCK_DL_SIZE,offset,target)) {
               return RT_STATUS_FAILURE;
            } 

            // pointer 
            //pbuf += MIPS_MAX_FWBLOCK_DL_SIZE/sizeof(u4Byte);
            pbuf += MIPS_MAX_FWBLOCK_DL_SIZE;
            len = len - MIPS_MAX_FWBLOCK_DL_SIZE;
            downloadcnt++;

            delay_ms(10);
        }
        else{
            offset = downloadcnt*MIPS_MAX_FWBLOCK_DL_SIZE;            

            if(RT_STATUS_SUCCESS != DLtoTXBUFandDDMA88XX(Adapter,pbuf,len,offset,target)) {
                return RT_STATUS_FAILURE;
            }   
            break;
        }
    }

    return RT_STATUS_SUCCESS;
}

enum rt_status
InitMIPSFirmware88XX(
    IN  HAL_PADAPTER    Adapter
)
{
    PRTL88XX_MIPS_FW_HDR    pfw_hdr;
    pu1Byte                 pFWStart;
    pu1Byte                 imemPtr;    
    pu1Byte                 dmemPtr;        
    pu1Byte                 fwBufPtr;
    u4Byte                  temBuf[8];
    u4Byte                  imemSZ;
    u2Byte                  dmemSZ;
    u4Byte                  offset = 0;
    u4Byte                  fwdlRdybit;
    enum rt_status status;
    PHAL_DATA_TYPE          pHalData    = _GET_HAL_DATA(Adapter);    

    pHalData->bFWReady = _FALSE;

    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_FWFILE_START, (pu1Byte)&pFWStart);
    //Register to HAL_DATA
    _GET_HAL_DATA(Adapter)->PFWHeader = (PVOID)pFWStart;

	ReadMIPSFwHdr88XX(Adapter);
	
    pfw_hdr = (PRTL88XX_MIPS_FW_HDR)pFWStart;
 

    if(RT_STATUS_SUCCESS != CheckMIPSFWImageSize(Adapter)) {
        RT_TRACE_F(COMP_INIT, DBG_WARNING, ("FW size error ! \n"));
        return RT_STATUS_FAILURE;
    }

    // ReaddDMEM, check length
    dmemSZ = HAL_EF2Byte(pfw_hdr->FW_CFG_SZ)+MIPS_FW_CHKSUM_DUMMY_SZ;
   
    // ReadIMEM, check length
    imemSZ = HAL_EF4Byte(pfw_hdr->IRAM_SZ)+MIPS_FW_CHKSUM_DUMMY_SZ;
   
    dmemPtr = pFWStart + MIPS_FW_HEADER_SIZE;
    imemPtr = pFWStart + MIPS_FW_HEADER_SIZE + dmemSZ;

#if (defined(CONFIG_NET_PCI) && defined(NOT_RTK_BSP)) || defined(CONFIG_RTL8192CD_MODULE)
    fwBufPtr = HALMalloc(Adapter, GET_MAX(dmemSZ, imemSZ));
    if (NULL == fwBufPtr) {
        RT_TRACE_F(COMP_INIT, DBG_WARNING, ("Allocate FW buffer error ! \n"));
        return RT_STATUS_FAILURE;
    }
#endif
    status = RT_STATUS_FAILURE;

    ConfigBeforeDLFW(Adapter,(pu4Byte)&temBuf);

    // Download size limitation
    // IMEM: 128K, DMEM: 128K
    // download image -------> txpktbuf --------> IMEM/DMEM
    //                 txDMA               DDMA
    //          tx descriptor:64K          128K
    // Download limitation = 64K - sizeof(TX_DESC) 
    
#if (defined(CONFIG_NET_PCI) && defined(NOT_RTK_BSP)) || defined(CONFIG_RTL8192CD_MODULE)
    HAL_memcpy(fwBufPtr, dmemPtr, dmemSZ);
#else
    fwBufPtr = dmemPtr;
#endif
    if(RT_STATUS_SUCCESS != checkFWSizeAndDLFW88XX(Adapter,fwBufPtr,dmemSZ,MIPS_DL_DMEM))
    {
        RestoreAfterDLFW(Adapter,(pu4Byte)&temBuf);    
        RT_TRACE_F(COMP_INIT, DBG_WARNING, ("%s %d DL DMEM fail ! \n",__FUNCTION__,__LINE__));
        goto out;
    }

    // verify checkSum
    if(RT_STATUS_SUCCESS != DLImageCheckSum(Adapter)) {
        HAL_RTL_W8(REG_MCUFW_CTRL, HAL_RTL_R8(REG_MCUFW_CTRL)|DMEM_CHKSUM_FAIL);
        RestoreAfterDLFW(Adapter,(pu4Byte)&temBuf);        
        RT_TRACE_F(COMP_INIT, DBG_WARNING, ("%s %d DL DMEM checkSum fail ! \n",__FUNCTION__,__LINE__));        
        goto out;
    }else {
        RT_TRACE_F(COMP_INIT, DBG_WARNING, ("%s %d DL DMEM checkSum ok ! \n",__FUNCTION__,__LINE__));     
        HAL_RTL_W8(REG_MCUFW_CTRL, ((HAL_RTL_R8(REG_MCUFW_CTRL)&(~DMEM_CHKSUM_FAIL))|DMEM_DL_RDY));
    }

    // Copy IMEM
#if (defined(CONFIG_NET_PCI) && defined(NOT_RTK_BSP)) || defined(CONFIG_RTL8192CD_MODULE)
    HAL_memcpy(fwBufPtr, imemPtr, imemSZ);
#else
    fwBufPtr = imemPtr;
#endif
    if(RT_STATUS_SUCCESS != checkFWSizeAndDLFW88XX(Adapter,fwBufPtr,imemSZ,MIPS_DL_IMEM))	
    {
        RestoreAfterDLFW(Adapter,(pu4Byte)&temBuf);        
        RT_TRACE_F(COMP_INIT, DBG_WARNING, ("%s %d DL IMEM fail ! \n",__FUNCTION__,__LINE__));  
        goto out;
    }    

    // verify checkSum
    if(RT_STATUS_SUCCESS != DLImageCheckSum(Adapter))    {
        HAL_RTL_W8(REG_MCUFW_CTRL, HAL_RTL_R8(REG_MCUFW_CTRL)|IMEM_CHKSUM_FAIL);
        RestoreAfterDLFW(Adapter,(pu4Byte)&temBuf);   
        RT_TRACE_F(COMP_INIT, DBG_WARNING, ("%s %d DL IMEM checkSum fail ! \n",__FUNCTION__,__LINE__));          
        goto out;
    }else {
        RT_TRACE_F(COMP_INIT, DBG_WARNING, ("%s %d DL IMEM checkSum ok ! \n",__FUNCTION__,__LINE__));     		
        HAL_RTL_W8(REG_MCUFW_CTRL, ((HAL_RTL_R8(REG_MCUFW_CTRL)&(~IMEM_CHKSUM_FAIL))|IMEM_DL_RDY));
    }
    
    RestoreAfterDLFW(Adapter,(pu4Byte)&temBuf);
#if IS_RTL8814A_SERIES || IS_RTL8814B_SERIES
		if(IS_HARDWARE_TYPE_8814A(Adapter)	|| IS_HARDWARE_TYPE_8814B(Adapter))
    HAL_RTL_W8(REG_MCUFW_CTRL,HAL_RTL_R8(REG_MCUFW_CTRL)&(~BIT_MCUFWDL_EN));
#endif
#if IS_RTL8197F_SERIES || IS_RTL8198F_SERIES || IS_RTL8197G_SERIES
    if(IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter))
    HAL_RTL_W8(REG_MCUFW_CTRL,HAL_RTL_R8(REG_MCUFW_CTRL)&(~BIT_MCU_FWDL_EN));
#endif
    HAL_RTL_W16(REG_SYS_FUNC_EN,HAL_RTL_R16(REG_SYS_FUNC_EN)|BIT_FEN_CPUEN);
    // wait CPU set ready flag 
    delay_ms(100);    

#if IS_RTL8197F_SERIES || IS_RTL8198F_SERIES
    if(IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter))
        fwdlRdybit = BIT10;
    else
        fwdlRdybit =BIT_FW_INIT_RDY;
#else

    fwdlRdybit =BIT_FW_INIT_RDY;
#endif


    if(HAL_RTL_R32(REG_MCUFW_CTRL)&fwdlRdybit)
    {
        RT_TRACE_F(COMP_INIT, DBG_WARNING, ("MIPS CPU Running \n"));        
        pHalData->bFWReady = _TRUE;
		_GET_HAL_DATA(Adapter)->H2CBufPtr88XX = 0;
    }else
    {
        RT_TRACE_F(COMP_INIT, DBG_WARNING, ("MIPS CPU Not Running \n"));          
        goto out;
    }

    status = RT_STATUS_SUCCESS;

out:

#if (defined(CONFIG_NET_PCI) && defined(NOT_RTK_BSP)) || defined(CONFIG_RTL8192CD_MODULE)
    HAL_free(fwBufPtr);
#endif

    return status;
}




#endif //IS_RTL88XX_FW_MIPS

#if IS_RTL88XX_FW_8051
//
//3 Download Firmware
//

static VOID
ReadFwHdr88XX(
    IN  HAL_PADAPTER    Adapter
)
{
#if 0
    PRTL88XX_FW_HDR pfw_hdr;
    pu1Byte     pFWStart;
    
    pfw_hdr = (PRTL88XX_FW_HDR)HALMalloc(Adapter, sizeof(RTL88XX_FW_HDR));
    if (NULL == pfw_hdr) {
        RT_TRACE(COMP_INIT, DBG_WARNING, ("ReadFwHdr88XX\n"));
        return;
    }

    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_FWFILE_START, (pu1Byte)&pFWStart);
    HAL_memcpy(pfw_hdr, pFWStart, RT_FIRMWARE_HDR_SIZE);
 
    //Register to HAL_DATA
    _GET_HAL_DATA(Adapter)->PFWHeader = pfw_hdr;
    RT_TRACE_F(COMP_INIT, DBG_TRACE ,("FW version = %x \n",pfw_hdr->version));
    RT_TRACE_F(COMP_INIT, DBG_TRACE ,("FW release at %x/%x \n",pfw_hdr->month,pfw_hdr->day));
    HAL_free(pfw_hdr);
#else
    PRTL88XX_FW_HDR pfw_hdr;
    pu1Byte     pFWStart;

    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_FWFILE_START, (pu1Byte)&pFWStart);
    //Register to HAL_DATA
    _GET_HAL_DATA(Adapter)->PFWHeader = (PVOID)pFWStart;
    pfw_hdr = (PRTL88XX_FW_HDR)pFWStart;
    
    RT_TRACE_F(COMP_INIT, DBG_TRACE ,("FW version = %x \n", pfw_hdr->version));
    RT_TRACE_F(COMP_INIT, DBG_TRACE ,("FW release at %x/%x \n", pfw_hdr->month, pfw_hdr->day));    
#endif
}

static VOID
WriteToFWSRAM88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  pu1Byte         pFWRealStart,
    IN  u4Byte          FWRealLen
)
{
    u4Byte      WriteAddr   = FW_DOWNLOAD_START_ADDRESS;
    u4Byte      CurPtr      = 0;
    u4Byte      Temp;
    u4Byte		StartAddr	= 0x1000;

#ifdef CONFIG_WLAN_HAL_8192FE
	if (GET_CHIP_VER(Adapter) == VERSION_8192F)
		WriteAddr = StartAddr = 0x4000;
#endif
    
	while (CurPtr < FWRealLen) {
		if ((CurPtr+4) > FWRealLen) {
			// Reach the end of file.
			while (CurPtr < FWRealLen) {
				Temp = *(pFWRealStart + CurPtr);
				HAL_RTL_W8(WriteAddr, (u1Byte)Temp);
				WriteAddr++;
				CurPtr++;
			}
		} else {
			// Write FW content to memory.
			Temp = *((pu4Byte)(pFWRealStart + CurPtr));
			Temp = HAL_cpu_to_le32(Temp);
			HAL_RTL_W32(WriteAddr, Temp);
			WriteAddr += 4;

			if(WriteAddr == StartAddr + 0x1000) {
				u1Byte  tmp = HAL_RTL_R8(REG_8051FW_CTRL+2);
                
                //Switch to next page
				tmp += 1;
                //Reset Address
				WriteAddr = StartAddr;
                
				HAL_RTL_W8(REG_8051FW_CTRL+2, tmp);
			}
			CurPtr += 4;
		}
	}
}

static VOID
DownloadFWInit88XX(
    IN  HAL_PADAPTER    Adapter
)
{
   //Clear 0x80,0x81,0x82[0],0x82[1],0x82[2],0x82[3]
   HAL_RTL_W8(REG_8051FW_CTRL,0x0);
#ifdef CONFIG_WLAN_HAL_8192FE
   if (GET_CHIP_VER(Adapter) == VERSION_8192F)
   	    HAL_RTL_W8(REG_8051FW_CTRL+1, 0x1);
   else
#endif
   HAL_RTL_W8(REG_8051FW_CTRL+1,0x0);

   // Enable MCU
   HAL_RTL_W8(REG_SYS_FUNC_EN+1, HAL_RTL_R8(REG_SYS_FUNC_EN+1) | BIT2);
#if defined(CONFIG_USB_HCI) && defined(CONFIG_RTL_TRIBAND_SUPPORT)
   if (Adapter->hci_type == RTL_HCI_USB) {
		HAL_RTL_W8(REG_SYS_FUNC_EN+1, HAL_RTL_R8(REG_SYS_FUNC_EN+1) | BIT3);
   }
#endif
   HAL_delay_ms(1);

   // Load SRAM
   HAL_RTL_W8(REG_8051FW_CTRL, HAL_RTL_R8(REG_8051FW_CTRL) | BIT_MCUFWDL_EN);
   HAL_delay_ms(1);

   //Clear ROM FPGA Related Parameter
   HAL_RTL_W32(REG_8051FW_CTRL, HAL_RTL_R32(REG_8051FW_CTRL) & 0xfff0ffff);
   delay_ms(1);
}

#if CFG_FW_VERIFICATION
static BOOLEAN
VerifyDownloadStatus88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  pu1Byte         pFWRealStart,
    IN  u4Byte          FWRealLen
)
{
    u4Byte      WriteAddr   = FW_DOWNLOAD_START_ADDRESS;
    u4Byte      CurPtr      = 0;
    u4Byte      binTemp;
    u4Byte      ROMTemp;    
    u1Byte      u1ByteTmp;


    // first clear page number 

    u1ByteTmp =  HAL_RTL_R8(REG_8051FW_CTRL+2);
    u1ByteTmp &= ~BIT0;
    u1ByteTmp &= ~BIT1;
    u1ByteTmp &= ~BIT2;    
    HAL_RTL_W8(REG_8051FW_CTRL+2, u1ByteTmp);
	delay_ms(1);

    // then compare FW image and download content
    CurPtr = 0;
         
	while (CurPtr < FWRealLen) {
		if ((CurPtr+4) > FWRealLen) {
			// Reach the end of file.
			while (CurPtr < FWRealLen) {
                if(HAL_RTL_R8(WriteAddr)!=((u1Byte)*(pFWRealStart + CurPtr)))
    			{
        		    RT_TRACE_F(COMP_INIT, DBG_LOUD,("Verify download fail at [%x] \n",WriteAddr));
                    return _FALSE;
    			}
				WriteAddr++;
				CurPtr++;
			}
		} else {
			// Comapre Download code with original binary

            binTemp = *((pu4Byte)(pFWRealStart + CurPtr));

			binTemp = HAL_cpu_to_le32(binTemp);
			ROMTemp = HAL_RTL_R32(WriteAddr);

			if(binTemp != ROMTemp)
			{
    		   RT_TRACE_F(COMP_INIT, DBG_LOUD,("Verify download fail at [0x%x] binTemp=%x,ROMTemp=%x \n",
                    WriteAddr,binTemp,ROMTemp));
                return _FALSE;
			}
			WriteAddr += 4;

			if(WriteAddr == 0x2000) {
				u1Byte  tmp = HAL_RTL_R8(REG_8051FW_CTRL+2);
                
                //Switch to next page
				tmp += 1;
                //Reset Address
				WriteAddr = 0x1000;
                
				HAL_RTL_W8(REG_8051FW_CTRL+2, tmp);
			}
			CurPtr += 4;
		}
	}
    return _TRUE;
}
#endif  //CFG_FW_VERIFICATION

static BOOLEAN
LoadFirmware88XX(
    IN  HAL_PADAPTER    Adapter
)
{
    pu1Byte     pFWRealStart;
    u4Byte      FWRealLen;
    u1Byte      u1ByteTmp;
    u1Byte      wait_cnt = 0;

    
    RT_TRACE_F(COMP_INIT, DBG_LOUD, ("\n"));

    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_FWFILE_START, (pu1Byte)&pFWRealStart);
	// get firmware info
#ifdef _BIG_ENDIAN_
    Adapter->pshare->fw_signature	= le16_to_cpu(*(unsigned short *)pFWRealStart);
	Adapter->pshare->fw_version		= le16_to_cpu(*(unsigned short *)(pFWRealStart+4));
#else
    Adapter->pshare->fw_signature	= *(unsigned short *)pFWRealStart;
	Adapter->pshare->fw_version		= *(unsigned short *)(pFWRealStart+4);
#endif
	Adapter->pshare->fw_category	= *(pFWRealStart+2);
	Adapter->pshare->fw_function	= *(pFWRealStart+3);
	Adapter->pshare->fw_sub_version	= *(pFWRealStart+6);
	Adapter->pshare->fw_date_month	= *(pFWRealStart+8);
	Adapter->pshare->fw_date_day	= *(pFWRealStart+9);
	Adapter->pshare->fw_date_hour	= *(pFWRealStart+10);
	Adapter->pshare->fw_date_minute	= *(pFWRealStart+11);
    pFWRealStart += RT_FIRMWARE_HDR_SIZE;

    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_FWFILE_SIZE, (pu1Byte)&FWRealLen);
    FWRealLen -= RT_FIRMWARE_HDR_SIZE;

    DownloadFWInit88XX(Adapter);

    WriteToFWSRAM88XX(Adapter, pFWRealStart, FWRealLen);

    u1ByteTmp = HAL_RTL_R8(REG_8051FW_CTRL);

    if ( u1ByteTmp & BIT_FWDL_CHK_RPT ) {
        RT_TRACE_F(COMP_INIT, DBG_TRACE , ("CheckSum Pass\n"));
    }
    else {
        RT_TRACE_F(COMP_INIT, DBG_WARNING, ("CheckSum Failed\n"));
        return _FALSE;        
    }

#if CFG_FW_VERIFICATION    
    if(VerifyDownloadStatus88XX(Adapter,pFWRealStart,FWRealLen)==_TRUE)
    {
        RT_TRACE_F(COMP_INIT, DBG_TRACE,("download verify ok!\n"));
    }
    else
    {
        RT_TRACE_F(COMP_INIT, DBG_WARNING,("download verify fail!\n"));
        return _FALSE;
    }
#endif //#if CFG_FW_VERIFICATION

    // download and verify ok, clear download enable bit, and set MCU DL ready
    u1ByteTmp &= ~BIT_MCUFWDL_EN;
    u1ByteTmp |= BIT_MCUFWDL_RDY;
	HAL_RTL_W8(REG_8051FW_CTRL, u1ByteTmp);
	HAL_delay_ms(1);
    
    // reset MCU, 8051 will jump to RAM code    
#ifdef CONFIG_WLAN_HAL_8192FE
	if (GET_CHIP_VER(Adapter) == VERSION_8192F)
		HAL_RTL_W8(REG_8051FW_CTRL+1, 0x01);
	else
#endif
	HAL_RTL_W8(REG_8051FW_CTRL+1, 0x00);

    // Disable WLMCU IO interface control
    HAL_RTL_W8(REG_RSV_CTRL+1, HAL_RTL_R8(REG_RSV_CTRL+1)&(~BIT0));
    HAL_RTL_W8(REG_SYS_FUNC_EN+1, HAL_RTL_R8(REG_SYS_FUNC_EN+1)&(~BIT2));
    HAL_delay_ms(1);
    HAL_RTL_W8(REG_RSV_CTRL+1, HAL_RTL_R8(REG_RSV_CTRL+1)| BIT0);  
    HAL_RTL_W8(REG_SYS_FUNC_EN+1, HAL_RTL_R8(REG_SYS_FUNC_EN+1) | BIT2);   


    RT_TRACE_F(COMP_INIT, DBG_WARNING , ("After download RAM reset MCU\n"));

	// Check if firmware RAM Code is ready
    while (!(HAL_RTL_R8(REG_8051FW_CTRL) & BIT_WINTINI_RDY)) {
        if (++wait_cnt > CHECK_FW_RAMCODE_READY_TIMES) {        
            RT_TRACE_F(COMP_INIT, DBG_WARNING, ("RAMCode Failed\n"));
            return _FALSE;
		}
        
        RT_TRACE_F(COMP_INIT, DBG_WARNING, ("Firmware is not ready, wait\n"));
        HAL_delay_ms(CHECK_FW_RAMCODE_READY_DELAY_MS);
    }

	_GET_HAL_DATA(Adapter)->H2CBufPtr88XX = 0;

    return _TRUE;
}


enum rt_status
InitFirmware88XX(
    IN  HAL_PADAPTER    Adapter
)
{
    PHAL_DATA_TYPE      pHalData    = _GET_HAL_DATA(Adapter);    
    u4Byte              dwnRetry    = DOWNLOAD_FIRMWARE_RETRY_TIMES;
    BOOLEAN             bfwStatus   = _FALSE;

    ReadFwHdr88XX(Adapter);

    while(dwnRetry-- && !bfwStatus) {
        bfwStatus = LoadFirmware88XX(Adapter);
    }

    if ( _TRUE == bfwStatus ) {
        RT_TRACE_F(COMP_INIT, DBG_WARNING, ("LoadFirmware88XX is Successful\n"));
        pHalData->bFWReady = _TRUE;
    }
    else {
        RT_TRACE_F(COMP_INIT, DBG_WARNING, ("LoadFirmware88XX failed\n"));
        pHalData->bFWReady = _FALSE;
        return RT_STATUS_FAILURE;
    }
    
    return RT_STATUS_SUCCESS;
}

#endif //IS_RTL88XX_FW_8051

#if IS_RTL88XX_GENERATION


static VOID
SetBcnCtrlReg88XX(
	IN HAL_PADAPTER     Adapter,	
	IN	u1Byte		    SetBits,
	IN	u1Byte		    ClearBits
	)
{
    u4Byte reg_bcn_crtl;
#if IS_RTL88XX_MAC_V1_V2_V3
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3) 
        reg_bcn_crtl = REG_BCN_CTRL;
#endif    
#if IS_RTL88XX_MAC_V4
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4) 
        reg_bcn_crtl = REG_PORT_CTRL_CFG;
#endif
	u1Byte tmp = HAL_RTL_R8(reg_bcn_crtl);

	tmp |=  SetBits;
	tmp &= ~ClearBits;

	HAL_RTL_W8(reg_bcn_crtl, tmp);
        
}

static u1Byte
MRateIdxToARFRId88XX(
	IN HAL_PADAPTER     Adapter,
	u1Byte  			RateIdx,    //RATR_TABLE_MODE
	u1Byte	    		RfType
)
{
	u1Byte Ret = 0;
	
	switch(RateIdx){

	case RATR_INX_WIRELESS_NGB:
		if(RfType == RF_1T1R)
			Ret = 1;
		else if(RfType == RF_2T2R)
			Ret = 0;
		else if(RfType == RF_3T3R)// 3T
			Ret = 14;
		break;

	case RATR_INX_WIRELESS_N:
	case RATR_INX_WIRELESS_NG:
		if(RfType == RF_1T1R)
			Ret = 5;
		else if(RfType == RF_2T2R)
			Ret = 4;
		else if(RfType == RF_3T3R)// 3T // becarefull, share with 3T BGN mode, we need to change register setting
			Ret = 14;
		break;

	case RATR_INX_WIRELESS_NB:
		if(RfType == RF_1T1R)
			Ret = 3;
		else if(RfType == RF_2T2R)
			Ret = 2;
		else if(RfType == RF_3T3R)// 3T // becarefull, share with 3T BGN mode, we need to change register setting
			Ret = 14;
		break;

	case RATR_INX_WIRELESS_GB:
		Ret = 6;
		break;

	case RATR_INX_WIRELESS_G:
		Ret = 7;
		break;	

	case RATR_INX_WIRELESS_B:
		Ret = 8;
		break;

	case RATR_INX_WIRELESS_MC:
		if (!(HAL_VAR_NETWORK_TYPE & WIRELESS_11A))
			Ret = 6;
		else
			Ret = 7;
		break;
#if CFG_HAL_RTK_AC_SUPPORT
	case RATR_INX_WIRELESS_AC_N:
		if(RfType == RF_1T1R)
			Ret = 10;
		else if(RfType == RF_2T2R)
			Ret = 9;
		else
			Ret = 13;
		break;
#endif

	default:
		Ret = 0;
		break;
	}	

	return Ret;
}

static u1Byte
Get_RA_BW88XX(
	BOOLEAN 	bCurTxBW80MHz, 
	BOOLEAN		bCurTxBW40MHz
)
{
	u1Byte	BW = CHANNEL_WIDTH_20;
    
	if(bCurTxBW80MHz)
		BW = CHANNEL_WIDTH_80;
	else if(bCurTxBW40MHz)
		BW = CHANNEL_WIDTH_40;
	else
		BW = CHANNEL_WIDTH_20;

	return BW;
}

static u1Byte
Get_VHT_ENI88XX(
	u4Byte			IOTAction,
	u1Byte			WirelessMode,
	u4Byte			ratr_bitmap 
	)
{
	u1Byte Ret = 0;

	if(WirelessMode & WIRELESS_MODE_AC_5G)
		Ret = 1;

	return (Ret << 4);
}

static BOOLEAN 
Get_RA_ShortGI88XX(	
	IN HAL_PADAPTER         Adapter,
	struct stat_info *		pEntry,
	IN	WIRELESS_MODE		WirelessMode,
	IN	u1Byte				ChnlBW
)
{	
	BOOLEAN	bShortGI;

	BOOLEAN	bShortGI20MHz = FALSE,bShortGI40MHz = FALSE, bShortGI80MHz = FALSE;
	
	if(	(WirelessMode & WIRELESS_MODE_N_24G) || 
		(WirelessMode & WIRELESS_MODE_N_5G) || 
		(WirelessMode & WIRELESS_MODE_AC_5G) )
	{
		if (pEntry->ht_cap_buf.ht_cap_info & HAL_cpu_to_le16(_HTCAP_SHORTGI_40M_)
			&& Adapter->pmib->dot11nConfigEntry.dot11nShortGIfor40M) {
			bShortGI40MHz = TRUE;
		} 
		if ((pEntry->ht_cap_buf.ht_cap_info & HAL_cpu_to_le16(_HTCAP_SHORTGI_20M_)
				|| (Adapter->pshare->rf_ft_var.sgi20Mforce && pEntry->ht_cap_len)) //for tp-link 822 v2
				&& Adapter->pmib->dot11nConfigEntry.dot11nShortGIfor20M) {
			bShortGI20MHz = TRUE;
		}
	}
#if CFG_HAL_RTK_AC_SUPPORT
	if(WirelessMode & WIRELESS_MODE_AC_5G)
	{
		{
			if(( HAL_cpu_to_le32(pEntry->vht_cap_buf.vht_cap_info) & BIT(SHORT_GI80M_E))
				&& Adapter->pmib->dot11nConfigEntry.dot11nShortGIfor80M)
				bShortGI80MHz = TRUE;
		}
	}
#endif

	switch(ChnlBW){
		case CHANNEL_WIDTH_40:
			bShortGI = bShortGI40MHz;
			break;
#if CFG_HAL_RTK_AC_SUPPORT            
		case CHANNEL_WIDTH_80:
			bShortGI = bShortGI80MHz;
			break;
#endif
		default:case CHANNEL_WIDTH_20:
			bShortGI = bShortGI20MHz;
			break;
	}		
	return bShortGI;
}

u8Byte
  RateToBitmap_VHT88XX(
	pu1Byte			pVHTRate,
	u1Byte 		rf_mimo_mode
	
)
{

	u1Byte	i,j , tmpRate,rateMask;
	u8Byte	RateBitmap = 0;

    if(rf_mimo_mode == RF_1T1R)
		rateMask = 2;
	else if(rf_mimo_mode == RF_2T2R)
        rateMask = 4;
	else if(rf_mimo_mode == RF_4T4R)
        rateMask = 8;
	else
    	rateMask = 6;

	for(i = j= 0; i < rateMask; i+=2, j+=10)
	{
        // now support for 1ss ~ 4ss
		tmpRate = (pVHTRate[0] >> i) & 3;

		switch(tmpRate){
		case 2:
			RateBitmap = RateBitmap | (0x03ff << j);
			break;
		case 1:
			RateBitmap = RateBitmap | (0x01ff << j);
		break;

		case 0:
			RateBitmap = RateBitmap | (0x00ff << j);
		break;

		default:
			break;
		}
	}

	return RateBitmap;
}

#if 0
u4Byte
Get_VHT_HT_Mix_Ratrbitmap(
	u4Byte					IOTAction,
	WIRELESS_MODE			WirelessMode,
	u4Byte					HT_ratr_bitmap,
	u4Byte					VHT_ratr_bitmap
	)
{
	u4Byte	ratr_bitmap = 0;
	if(WirelessMode == WIRELESS_MODE_N_24G || WirelessMode == WIRELESS_MODE_N_5G)
	{
/*	
		if(IOTAction == HT_IOT_VHT_HT_MIX_MODE)
			ratr_bitmap = HT_ratr_bitmap | BIT28 | BIT29;
		else
			ratr_bitmap =  HT_ratr_bitmap;
*/		
	}
	else
		ratr_bitmap =  VHT_ratr_bitmap;

	return ratr_bitmap;
}
#endif

VOID
UpdateHalRAMask88XX(
	IN HAL_PADAPTER         Adapter,
	HAL_PSTAINFO            pEntry,
	u1Byte                  rssi_level
	)
{
	PRTL8192CD_PRIV		priv = Adapter;
	u1Byte		        WirelessMode    = WIRELESS_MODE_A;
	u1Byte		        BW              = CHANNEL_WIDTH_20;
	u1Byte			MimoPs = MIMO_PS_NOLIMIT, MimoPs_enable = FALSE, ratr_index = 8, H2CCommand[7] ={ 0};
	u1Byte			disable_cck_rate = FALSE;
	u4Byte			ratr_bitmap = 0, ratr_bitmap_msb = 0;
	u4Byte			IOTAction = 0;
	BOOLEAN		        bShortGI        = FALSE, bCurTxBW80MHz=FALSE, bCurTxBW40MHz=FALSE;
	struct stat_info    *pstat          = pEntry;
	u1Byte              rf_mimo_mode    = get_rf_mimo_mode(Adapter);
	#if CFG_HAL_RTK_AC_SUPPORT
	u4Byte 				VHT_TxMap = Adapter->pmib->dot11acConfigEntry.dot11VHT_TxMap;
	#endif
	
	if((pEntry == NULL) || (pEntry->cmn_info.ra_info.disable_ra))	
	{
		return;
	}
	
	
	if(pEntry->MIMO_ps & _HT_MIMO_PS_STATIC_)
		MimoPs = MIMO_PS_STATIC;
	else if(pEntry->MIMO_ps & _HT_MIMO_PS_DYNAMIC_)
		MimoPs = MIMO_PS_DYNAMIC;

	#if	1
	BW = pEntry->tx_bw;

	if( BW > (Adapter->pshare->CurrentChannelBW))
		BW = Adapter->pshare->CurrentChannelBW;
	#endif
	add_RATid(Adapter, pEntry);		
	rssi_level = pstat->cmn_info.ra_info.rssi_level;
	//rssi_level = 3;
	ratr_bitmap =  0xfffffff;

	#if CFG_HAL_RTK_AC_SUPPORT
	if(pstat->vht_cap_len && (HAL_VAR_NETWORK_TYPE & WIRELESS_11AC) && (!should_restrict_Nrate(Adapter, pstat))) {
		WirelessMode = WIRELESS_MODE_AC_5G;
		if(((HAL_le32_to_cpu(pstat->vht_cap_buf.vht_support_mcs[0])>>2)&3)==3)
			rf_mimo_mode = RF_1T1R;
	} 
    else 
	#endif
    if ((HAL_VAR_NETWORK_TYPE & WIRELESS_11N) && pstat->ht_cap_len && (!should_restrict_Nrate(Adapter, pstat))) {
		if(HAL_VAR_NETWORK_TYPE & WIRELESS_11A)
			WirelessMode = WIRELESS_MODE_N_5G;
		else
			WirelessMode = WIRELESS_MODE_N_24G;
        
		if((pstat->tx_ra_bitmap & 0xff00000) == 0)
			rf_mimo_mode = RF_1T1R;
	}
	else if (((HAL_VAR_NETWORK_TYPE & WIRELESS_11G) && isErpSta(pstat))){
			WirelessMode = WIRELESS_MODE_G;		
	}
        else if ((HAL_VAR_NETWORK_TYPE & WIRELESS_11A) &&
		((HAL_OPMODE & WIFI_AP_STATE) || (Adapter->pmib->dot11RFEntry.phyBandSelect & PHY_BAND_5G))) {
		WirelessMode = WIRELESS_MODE_A;		
	}
	else if(HAL_VAR_NETWORK_TYPE & WIRELESS_11B){
		WirelessMode = WIRELESS_MODE_B;		
	}
	#if (defined(AC2G_256QAM) || defined(WLAN_HAL_8814AE))
	else if (is_ac2g(Adapter) && pstat->vht_cap_len) {
		WirelessMode = WIRELESS_MODE_AC_24G;
	}
	#endif

	pstat->wireless_mode = WirelessMode;

	switch (WirelessMode) {
	case WIRELESS_MODE_A:
		pstat->cmn_info.support_wireless_set = WIRELESS_OFDM;
		break;
	case WIRELESS_MODE_B:
		pstat->cmn_info.support_wireless_set = WIRELESS_CCK;
		break;
	case WIRELESS_MODE_G:
		pstat->cmn_info.support_wireless_set = (WIRELESS_CCK | WIRELESS_OFDM);
		break;
	case WIRELESS_MODE_N_24G:
		pstat->cmn_info.support_wireless_set = (WIRELESS_CCK | WIRELESS_OFDM | WIRELESS_HT);
		break;
	case WIRELESS_MODE_N_5G:
		pstat->cmn_info.support_wireless_set = (WIRELESS_OFDM | WIRELESS_HT);
		break;
	case WIRELESS_MODE_AC_24G:
		pstat->cmn_info.support_wireless_set = (WIRELESS_CCK | WIRELESS_OFDM | WIRELESS_VHT);
		break;
	case WIRELESS_MODE_AC_5G:
		pstat->cmn_info.support_wireless_set = (WIRELESS_OFDM | WIRELESS_VHT);
		break;	
	}

	#if CFG_HAL_RTK_AC_SUPPORT
	if ((WirelessMode & WIRELESS_MODE_AC_5G) || (WirelessMode & WIRELESS_MODE_AC_24G)) {
		ratr_bitmap &= 0xfff;
		ratr_bitmap |= RateToBitmap_VHT88XX((pu1Byte)&(pstat->vht_cap_buf.vht_support_mcs[0]), rf_mimo_mode) << 12;
		//ratr_bitmap &= 0x3FCFFFFF;
		if(rf_mimo_mode == RF_1T1R)
			ratr_bitmap &= 0x003fffff;
		else if(rf_mimo_mode == RF_2T2R)
			ratr_bitmap &= 0x3FCFFFFF;			// Test Chip...	2SS MCS7
		else
			ratr_bitmap &= 0x3FF3FCFFFFFULL;		// Test Chip...	3SS MCS7

		if(BW==CHANNEL_WIDTH_80)
			bCurTxBW80MHz = TRUE;
	}
	#endif

	if (Adapter->pshare->is_40m_bw && (BW == CHANNEL_WIDTH_40)
	#ifdef WIFI_11N_2040_COEXIST
		&& !(((((GET_MIB(Adapter))->dot11OperationEntry.opmode) & WIFI_AP_STATE)) 
		&& COEXIST_ENABLE
		&& (Adapter->bg_ap_timeout || orForce20_Switch20Map(Adapter)
		))
	#endif
	){
		bCurTxBW40MHz = TRUE;
	}
	
	if(((GET_MIB(Adapter))->dot11OperationEntry.opmode) & WIFI_STATION_STATE) {
		if(((GET_MIB(Adapter))->dot11Bss.t_stamp[1] & 0x6) == 0) {
			bCurTxBW40MHz = bCurTxBW80MHz = FALSE;
		}
	}
	BW = Get_RA_BW88XX(bCurTxBW80MHz, bCurTxBW40MHz);
	
	#if CFG_HAL_RTK_AC_SUPPORT	
	if(BW == 0)
	{
		//remove MCS9 for BW=20m
		if (rf_mimo_mode == RF_1T1R)
			VHT_TxMap &= ~(BIT(9));
		else if (rf_mimo_mode == RF_2T2R)
			VHT_TxMap &= ~(BIT(9)|BIT(19));
	}
	#endif

#if 1
	bShortGI = Get_RA_ShortGI88XX(Adapter, pEntry, WirelessMode, BW);

	if (is_veriwave_testing(Adapter)) {
		bShortGI = TRUE;
	}

	pstat->cmn_info.ra_info.is_support_sgi = bShortGI;
	pstat->cmn_info.bw_mode = BW;
	pstat->cmn_info.ra_info.ra_bw_mode = BW;
	pstat->cmn_info.mimo_type = rf_mimo_mode;
	pstat->cmn_info.sm_ps = MimoPs;

	if (MimoPs < MIMO_PS_DYNAMIC)
		MimoPs_enable = TRUE;
	 else if((MimoPs == MIMO_PS_DYNAMIC) && (pEntry->tx_avarage  < (3<<17))) {
			MimoPs_enable = TRUE;
	}

	pstat->cmn_info.ra_info.ramask = ratr_bitmap | ((u8Byte)ratr_bitmap_msb << 32);

	pstat->MimoPs_fw = MimoPs_enable;
	phydm_update_hal_ra_mask(ODMPTR, WirelessMode, rf_mimo_mode, BW, MimoPs_enable, disable_cck_rate, &ratr_bitmap_msb, &ratr_bitmap, rssi_level);
	pstat->cmn_info.ra_info.rate_id = phydm_rate_id_mapping(ODMPTR, WirelessMode, rf_mimo_mode, BW);

#else
	// assign band mask and rate bitmap
	switch (WirelessMode)
	{
		case WIRELESS_MODE_B:
		{
			ratr_index = RATR_INX_WIRELESS_B;
			if(ratr_bitmap & 0x0000000c)		//11M or 5.5M enable				
				ratr_bitmap &= 0x0000000d;
			else
				ratr_bitmap &= 0x0000000f;
		}
		break;

		case WIRELESS_MODE_G:
		{
#ifdef MCR_WIRELESS_EXTEND			
			if (pstat->IOTPeer == HT_IOT_PEER_CMW)
			{
				ratr_index = RATR_INX_WIRELESS_G;
				ratr_bitmap &= 0x00000ff0;
			}
			else
#endif				
			{
			ratr_index = RATR_INX_WIRELESS_GB;
			
			if(rssi_level == 1)
				ratr_bitmap &= 0x00000f00;
			else if(rssi_level == 2)
				ratr_bitmap &= 0x00000ff0;
			else
				ratr_bitmap &= 0x00000ff5;
			}
		}
		break;
			
		case WIRELESS_MODE_A:
		{
			ratr_index = RATR_INX_WIRELESS_G;
			ratr_bitmap &= 0x00000ff0;
		}
		break;
			
		case WIRELESS_MODE_N_24G:
		case WIRELESS_MODE_N_5G:
		{
			if(WirelessMode == WIRELESS_MODE_N_24G)
			{
#ifdef MCR_WIRELESS_EXTEND			
				if (pstat->IOTPeer == HT_IOT_PEER_CMW) 					
					ratr_index = RATR_INX_WIRELESS_N;
				else
#endif					
				ratr_index = RATR_INX_WIRELESS_NGB;
			}
			else
				ratr_index = RATR_INX_WIRELESS_NG;

//			if(MimoPs <= MIMO_PS_DYNAMIC)
#if 0
			if(MimoPs < MIMO_PS_DYNAMIC)
			{
				if(rssi_level == 1)
					ratr_bitmap &= 0x00070000;
				else if(rssi_level == 2)
					ratr_bitmap &= 0x0007f000;
				else
					ratr_bitmap &= 0x0007f005;
			}
			else
#endif			
#ifdef MCR_WIRELESS_EXTEND
			if (pstat->IOTPeer == HT_IOT_PEER_CMW)
			{
				if (rf_mimo_mode == RF_1T1R)
					ratr_bitmap &= 0x000ff000;
				else
					ratr_bitmap &= 0x0ffff000;
			}
			else
#endif
			{
				if (rf_mimo_mode == RF_1T1R)
				{					
					if (bCurTxBW40MHz)
					{
						if(rssi_level == 1)
							ratr_bitmap &= 0x000f0000;
						else if(rssi_level == 2)
							ratr_bitmap &= 0x000ff000;
						else
							ratr_bitmap &= 0x000ff015;
					}
					else
					{
						if(rssi_level == 1)
							ratr_bitmap &= 0x000f0000;
						else if(rssi_level == 2)
							ratr_bitmap &= 0x000ff000;
						else
							ratr_bitmap &= 0x000ff005;
					}	
				}
				else
				{
					if (bCurTxBW40MHz)
					{
						if(rssi_level == 1)
							ratr_bitmap &= 0x0f8f0000;
						else if(rssi_level == 2)
							ratr_bitmap &= 0x0ffff000;
						else
							ratr_bitmap &= 0x0ffff015;
					}
					else
					{
						if(rssi_level == 1)
							ratr_bitmap &= 0x0f8f0000;
						else if(rssi_level == 2)
							ratr_bitmap &= 0x0ffff000;
						else
							ratr_bitmap &= 0x0ffff005;
					}
				}
			}
		}
		break;

#if CFG_HAL_RTK_AC_SUPPORT

		case WIRELESS_MODE_AC_24G:
		{
			// need to check !!!
			ratr_index = RATR_INX_WIRELESS_AC_24N;
			if(rssi_level == 1)
				ratr_bitmap &= 0xfc3f0000;
			else if(rssi_level == 2)
				ratr_bitmap &= 0xfffff000;
			else
				ratr_bitmap &= 0xffffffff;
		}
		break;

		case WIRELESS_MODE_AC_5G:
		{
			ratr_index = RATR_INX_WIRELESS_AC_N;

			if (rf_mimo_mode == RF_1T1R) {
				ratr_bitmap &= 0x003ff010;
				if(rssi_level == 3)
					ratr_bitmap &= 0x000ff010;
			}
			else if (rf_mimo_mode == RF_2T2R) { 
				if (rssi_level==1)
					ratr_bitmap &= 0xfe3f8010;
				else if (rssi_level==2)
					ratr_bitmap &= 0xfffff010;
				else
					ratr_bitmap &= 0xfffffff0;	
			} 
		}
		break;
#endif

		default:
			ratr_index = RATR_INX_WIRELESS_NGB;
			
			if(rf_mimo_mode == RF_1T1R)
				ratr_bitmap &= 0x000ff0ff;
			else
				ratr_bitmap &= 0x0f8ff0ff;
			break;
	}
	BW = Get_RA_BW88XX(bCurTxBW80MHz, bCurTxBW40MHz);
	bShortGI = Get_RA_ShortGI88XX(Adapter, pEntry, WirelessMode, BW);
	pstat->cmn_info.ra_info.is_support_sgi = bShortGI;

	pstat->cmn_info.ra_info.rate_id = MRateIdxToARFRId88XX(Adapter, ratr_index, rf_mimo_mode);
	pstat->cmn_info.bw_mode = BW;
	pstat->cmn_info.ra_info.ra_bw_mode = BW;

#endif

	#if 0//defined(AC2G_256QAM) || defined(WLAN_HAL_8814AE)
	if(is_ac2g(Adapter) && pstat->vht_cap_len ) {
		 printk("AC2G STA Associated !!\n");
		 if (rf_mimo_mode == RF_1T1R)
		 {
			 //bShortGI = 1;
			 ratr_bitmap = 0x003ff015;

			 if(BW == 2)
				pstat->cmn_info.ra_info.rate_id = 10;
			 else
				pstat->cmn_info.ra_info.rate_id = 11;
 
			 if(BW == 0)
				VHT_TxMap = 0x1ff;
			 else
				VHT_TxMap = 0x3ff;
 
		 }
		 else if (rf_mimo_mode == RF_2T2R)
		 {
			// bShortGI = 0;
			 ratr_bitmap = 0xffcff015;

			 if(BW == 2)
				pstat->cmn_info.ra_info.rate_id = 9;
			 else
				pstat->cmn_info.ra_info.rate_id = 12;

			 if(BW == 0)
				VHT_TxMap = 0x7fdff;
			 else
				VHT_TxMap = 0xfffff;
		 }

		 ratr_bitmap &= ((VHT_TxMap << 12)|0xfff);
		 
		 pstat->wireless_mode = WIRELESS_MODE_AC_24G;
		 pstat->cmn_info.support_wireless_set = WIRELESS_CCK | WIRELESS_OFDM | WIRELESS_HT | WIRELESS_VHT;
	}
	#endif

	// for debug, set vht_txmap for RAMask
	#if CFG_HAL_RTK_AC_SUPPORT
	if (WirelessMode & WIRELESS_MODE_AC_5G)
		ratr_bitmap &= (VHT_TxMap << 12)|0xff0;
	else if(WirelessMode & WIRELESS_MODE_AC_24G)
		ratr_bitmap &= (VHT_TxMap << 12)|0xfff;
	#endif	

#ifdef CONFIG_VERIWAVE_CHECK
		if(GET_CHIP_VER(priv) == VERSION_8192E) {
			if(BW == CHANNEL_WIDTH_40 && WirelessMode == WIRELESS_MODE_N_24G && pstat->veriwave_2G_RvR_test) {
				if(pstat->current_tx_pkt_size == VERIWAVE_PKT_SIZE_SML) 
					ratr_bitmap = (ratr_bitmap & 0xf87c3fff);
				else if(pstat->current_tx_pkt_size == VERIWAVE_PKT_SIZE_BIG)
					ratr_bitmap = (ratr_bitmap & 0xffffffff);
			}
		}
#endif
	
	H2CCommand[0] = REMAP_AID(pstat); //macid
	H2CCommand[1] = (pstat->cmn_info.ra_info.rate_id)| (bShortGI?0x80:0x00) ; //rate_id
	H2CCommand[2] = BW |Get_VHT_ENI88XX(IOTAction, WirelessMode, ratr_bitmap);

	H2CCommand[2] |= BIT6;			// DisableTXPowerTraining

	H2CCommand[3] = (u1Byte)(ratr_bitmap & 0x000000ff);
	H2CCommand[4] = (u1Byte)((ratr_bitmap & 0x0000ff00) >>8);
	H2CCommand[5] = (u1Byte)((ratr_bitmap & 0x00ff0000) >> 16);
	H2CCommand[6] = (u1Byte)((ratr_bitmap & 0xff000000) >> 24);
	pstat->tx_ra_bitmap = ratr_bitmap;
	
	FillH2CCmd88XX(Adapter, H2C_88XX_RA_MASK, 7, H2CCommand);

	SetBcnCtrlReg88XX(Adapter, BIT3, 0);
#if 0	
	panic_printk("UpdateHalRAMask88XX(): bitmap = %x ratr_index = %1x, MacID:%x, ShortGI:%x, MimoPs=%d\n", 
		ratr_bitmap, pstat->cmn_info.ra_info.rate_id,  REMAP_AID(pstat), bShortGI, MimoPs);
	
	panic_printk("Cmd: %02x, %02x, %02x, %02x, %02x, %02x, %02x  \n",
		H2CCommand[0] ,H2CCommand[1], H2CCommand[2],
		H2CCommand[3] ,H2CCommand[4], H2CCommand[5], H2CCommand[6]		);
#endif	
}

VOID
UpdateHalRAMask8814A(
	IN HAL_PADAPTER         Adapter,
	HAL_PSTAINFO            pEntry,
	u1Byte                  rssi_level
	)
{

	u1Byte		        WirelessMode    = WIRELESS_MODE_A;
	u1Byte		        BW              = CHANNEL_WIDTH_20;
	u1Byte			MimoPs          = MIMO_PS_NOLIMIT, MimoPs_enable = FALSE, ratr_index = 8, H2CCommand[7] ={ 0};
	u8Byte			ratr_bitmap = 0, IOTAction = 0;
	u1Byte			disable_cck_rate = FALSE;
	u4Byte			ratr_bitmap_lsb = 0, ratr_bitmap_msb = 0;
	BOOLEAN		        bShortGI        = FALSE, bCurTxBW80MHz=FALSE, bCurTxBW40MHz=FALSE;
	struct stat_info    *pstat          = pEntry;
	u1Byte              rf_mimo_mode    = get_rf_mimo_mode(Adapter);
	u1Byte		sta_mimo_mode;
	u1Byte		sta_mimo_mode_nss = 0;
	#if CFG_HAL_RTK_AC_SUPPORT
	u8Byte 				VHT_TxMap = Adapter->pmib->dot11acConfigEntry.dot11VHT_TxMap;
	#endif

	//printk("%s %d ratr_bitmap = %llx \n",__func__,__LINE__,ratr_bitmap);            
	//printk("pstat=%x pstat->cmn_info.aid= %x \n",pstat,pstat->cmn_info.aid);

	if((pEntry == NULL) || (pEntry->cmn_info.ra_info.disable_ra))	
	{
		return;
	}
	{
		if(pEntry->MIMO_ps & _HT_MIMO_PS_STATIC_)
            MimoPs = MIMO_PS_STATIC;
		else if(pEntry->MIMO_ps & _HT_MIMO_PS_DYNAMIC_)
			MimoPs = MIMO_PS_DYNAMIC;

#if	1
		BW = pEntry->tx_bw;
		if( BW > Adapter->pshare->CurrentChannelBW)
			BW = Adapter->pshare->CurrentChannelBW;
#endif	
		add_RATid(Adapter, pEntry);		
		rssi_level = pstat->cmn_info.ra_info.rssi_level;
//		rssi_level = 3;
		ratr_bitmap =  0xffffffffffffffffULL;

#if CFG_HAL_RTK_AC_SUPPORT
		if(pstat->vht_cap_len && (HAL_VAR_NETWORK_TYPE & WIRELESS_11AC)) {
			WirelessMode = WIRELESS_MODE_AC_5G;
			if(((HAL_le32_to_cpu(pstat->vht_cap_buf.vht_support_mcs[0])>>8)&3)==3) // no support RX 5ss
				sta_mimo_mode = RF_4T4R;
			if(((HAL_le32_to_cpu(pstat->vht_cap_buf.vht_support_mcs[0])>>6)&3)==3) // no support RX 4ss
				sta_mimo_mode = RF_3T3R;
			if(((HAL_le32_to_cpu(pstat->vht_cap_buf.vht_support_mcs[0])>>4)&3)==3) // no support RX 3ss
				sta_mimo_mode = RF_2T2R;
			if(((HAL_le32_to_cpu(pstat->vht_cap_buf.vht_support_mcs[0])>>2)&3)==3) // no support RX 2ss
				sta_mimo_mode = RF_1T1R;

			pstat->sta_mimo_mode = sta_mimo_mode;
			
			if (get_rf_NTx(sta_mimo_mode) < get_rf_NTx(rf_mimo_mode))
				rf_mimo_mode = sta_mimo_mode;

			if (pstat->nss == 1)
				sta_mimo_mode_nss = RF_1T1R;
			else if (pstat->nss == 2)
				sta_mimo_mode_nss = RF_2T2R;

			if ((sta_mimo_mode_nss != 0) && (get_rf_NTx(sta_mimo_mode_nss) < get_rf_NTx(rf_mimo_mode)))
				rf_mimo_mode = sta_mimo_mode_nss;
		
		} 
        else 
#endif
        if ((HAL_VAR_NETWORK_TYPE & WIRELESS_11N) && pstat->ht_cap_len && (!should_restrict_Nrate(Adapter, pstat))) {
			if(HAL_VAR_NETWORK_TYPE & WIRELESS_11A)
				WirelessMode = WIRELESS_MODE_N_5G;
			else
				WirelessMode = WIRELESS_MODE_N_24G;
			
			if (pstat->ht_cap_buf.support_mcs[4] == 0) // no support RX 5ss
				sta_mimo_mode = RF_4T4R;
			if (pstat->ht_cap_buf.support_mcs[3] == 0) // no support RX 4ss
				sta_mimo_mode = RF_3T3R;
			if (pstat->ht_cap_buf.support_mcs[2] == 0) // no support RX 3ss
				sta_mimo_mode = RF_2T2R;
			if (pstat->ht_cap_buf.support_mcs[1] == 0) // no support RX 2ss
				sta_mimo_mode = RF_1T1R;

			if (get_rf_NTx(sta_mimo_mode) < get_rf_NTx(rf_mimo_mode))
				rf_mimo_mode = sta_mimo_mode;

#ifdef RTK_AC_SUPPORT
			if (pstat->nss == 1)
				sta_mimo_mode_nss = RF_1T1R;
			else if (pstat->nss == 2)
				sta_mimo_mode_nss = RF_2T2R;

			if ((sta_mimo_mode_nss != 0) && (get_rf_NTx(sta_mimo_mode_nss) < get_rf_NTx(rf_mimo_mode)))
				rf_mimo_mode = sta_mimo_mode_nss;
#endif

			//if((pstat->tx_ra_bitmap & 0xff00000) == 0)
			//	rf_mimo_mode = RF_1T1R;
		}
		else if (((HAL_VAR_NETWORK_TYPE & WIRELESS_11G) && isErpSta(pstat))){
				WirelessMode = WIRELESS_MODE_G;		
		}
        else if ((HAL_VAR_NETWORK_TYPE & WIRELESS_11A) &&
				((HAL_OPMODE & WIFI_AP_STATE) || (Adapter->pmib->dot11RFEntry.phyBandSelect & PHY_BAND_5G))) {
				WirelessMode = WIRELESS_MODE_A;		
		}
		else if(HAL_VAR_NETWORK_TYPE & WIRELESS_11B){
			WirelessMode = WIRELESS_MODE_B;		
		}

		pstat->wireless_mode = WirelessMode;
		switch (WirelessMode) {
		case WIRELESS_MODE_A:
			pstat->cmn_info.support_wireless_set = WIRELESS_OFDM;
			break;
		case WIRELESS_MODE_B:
			pstat->cmn_info.support_wireless_set = WIRELESS_CCK;
			break;
		case WIRELESS_MODE_G:
			pstat->cmn_info.support_wireless_set = (WIRELESS_CCK | WIRELESS_OFDM);
			break;
		case WIRELESS_MODE_N_24G:
			pstat->cmn_info.support_wireless_set = (WIRELESS_CCK | WIRELESS_OFDM | WIRELESS_HT);
			break;
		case WIRELESS_MODE_N_5G:
			pstat->cmn_info.support_wireless_set = (WIRELESS_OFDM | WIRELESS_HT);
			break;
		case WIRELESS_MODE_AC_24G:
			pstat->cmn_info.support_wireless_set = (WIRELESS_CCK | WIRELESS_OFDM | WIRELESS_VHT);
			break;
		case WIRELESS_MODE_AC_5G:
			pstat->cmn_info.support_wireless_set = (WIRELESS_OFDM | WIRELESS_VHT);
			break;	
		}

#if CFG_HAL_RTK_AC_SUPPORT
		if(WirelessMode & WIRELESS_MODE_AC_5G) {
			ratr_bitmap &= 0xfff;
			ratr_bitmap |= RateToBitmap_VHT88XX((pu1Byte)&(pstat->vht_cap_buf.vht_support_mcs[0]), rf_mimo_mode) << 12;

			if(rf_mimo_mode == RF_1T1R)
				ratr_bitmap &= 0x003fffff;
            else if(rf_mimo_mode == RF_3T3R)
                ratr_bitmap &= 0xFFF3FCFFFFFULL;			// 3SS MCS9
			else
				ratr_bitmap &= 0xFFCFFFFF;			// 2SS MCS9
				
			if(BW==CHANNEL_WIDTH_80)
				bCurTxBW80MHz = TRUE;

		}
#endif

		if (Adapter->pshare->is_40m_bw && (BW == CHANNEL_WIDTH_40)
#ifdef WIFI_11N_2040_COEXIST
				&& !(((((GET_MIB(Adapter))->dot11OperationEntry.opmode) & WIFI_AP_STATE)||
				(((GET_MIB(Adapter))->dot11OperationEntry.opmode) & WIFI_STATION_STATE)) 
				&& Adapter->pmib->dot11nConfigEntry.dot11nCoexist 
				&& (Adapter->bg_ap_timeout || orForce20_Switch20Map(Adapter))
			)
#endif
		)
			bCurTxBW40MHz = TRUE;
	}


#if 1

	BW = Get_RA_BW88XX(bCurTxBW80MHz, bCurTxBW40MHz);

	#if CFG_HAL_RTK_AC_SUPPORT	
	if(BW == CHANNEL_WIDTH_20)
	{
		//remove MCS9 for BW=20m
		if (rf_mimo_mode == RF_1T1R)
			VHT_TxMap &= ~(BIT(9));
		else if (rf_mimo_mode == RF_2T2R)
			VHT_TxMap &= ~(BIT(9)|BIT(19));
	}
	#endif

	bShortGI = Get_RA_ShortGI88XX(Adapter, pEntry, WirelessMode, BW);

	if (is_veriwave_testing(Adapter)) {
		bShortGI = TRUE;
	}
	
	pstat->cmn_info.ra_info.is_support_sgi = bShortGI;
	pstat->cmn_info.mimo_type = rf_mimo_mode;
	pstat->cmn_info.sm_ps = MimoPs;

	if (MimoPs < MIMO_PS_DYNAMIC)
		MimoPs_enable = TRUE;
	 else if((MimoPs == MIMO_PS_DYNAMIC) && (pEntry->tx_avarage  < (3<<17))) {
			MimoPs_enable = TRUE;
	}
	pEntry->MimoPs_fw = MimoPs_enable;
	ratr_bitmap_msb = (u4Byte)(ratr_bitmap>>32);
	ratr_bitmap_lsb = (u4Byte)(ratr_bitmap);

	pstat->cmn_info.ra_info.ramask = ratr_bitmap | ((u8Byte)ratr_bitmap_msb << 32);

	//dbg_print("11111 ratr_bitmap_msb = 0x%x, ratr_bitmap_lsb = 0x%x\n", ratr_bitmap_msb, ratr_bitmap_lsb);	
	phydm_update_hal_ra_mask(&(Adapter->pshare->_dmODM), WirelessMode, rf_mimo_mode, BW, MimoPs_enable, disable_cck_rate, &ratr_bitmap_msb, &ratr_bitmap_lsb, rssi_level);	
	ratr_bitmap = (((u8Byte)ratr_bitmap_msb)<<32) | ((u8Byte)ratr_bitmap_lsb);
	//dbg_print("22222 ratr_bitmap = 0x%llx\n", ratr_bitmap);
	pstat->cmn_info.ra_info.rate_id = phydm_rate_id_mapping(&(Adapter->pshare->_dmODM), WirelessMode, rf_mimo_mode, BW);
	pstat->cmn_info.bw_mode = BW;
	pstat->cmn_info.ra_info.ra_bw_mode = BW;

#else
	// assign band mask and rate bitmap
	switch (WirelessMode)
	{
		case WIRELESS_MODE_B:
		{
			ratr_index = RATR_INX_WIRELESS_B;
			if(ratr_bitmap & 0x000000000000000c)		//11M or 5.5M enable				
				ratr_bitmap &= 0x000000000000000d;
			else
				ratr_bitmap &= 0x000000000000000f;
		}
		break;

		case WIRELESS_MODE_G:
		{
			ratr_index = RATR_INX_WIRELESS_GB;
			
			if(rssi_level == 1)
				ratr_bitmap &= 0x0000000000000f00;
			else if(rssi_level == 2)
				ratr_bitmap &= 0x0000000000000ff0;
			else
				ratr_bitmap &= 0x0000000000000ff5;
		}
		break;
			
		case WIRELESS_MODE_A:
		{
			ratr_index = RATR_INX_WIRELESS_G;
			ratr_bitmap &= 0x00000ff0;
		}
		break;
			
		case WIRELESS_MODE_N_24G:
		case WIRELESS_MODE_N_5G:
		{
			if(WirelessMode == WIRELESS_MODE_N_24G)
				ratr_index = RATR_INX_WIRELESS_NGB;
			else
				ratr_index = RATR_INX_WIRELESS_NG;

//			if(MimoPs <= MIMO_PS_DYNAMIC)
#if 0
			if(MimoPs < MIMO_PS_DYNAMIC)
			{
				if(rssi_level == 1)
					ratr_bitmap &= 0x00070000;
				else if(rssi_level == 2)
					ratr_bitmap &= 0x0007f000;
				else
					ratr_bitmap &= 0x0007f005;
			}
			else
#endif			
			{
				if (rf_mimo_mode == RF_1T1R)
				{					
					if (bCurTxBW40MHz)
					{
						if(rssi_level == 1)
							ratr_bitmap &= 0x00000000000f0000;
						else if(rssi_level == 2)
							ratr_bitmap &= 0x00000000000ff000;
						else
							ratr_bitmap &= 0x00000000000ff015;
					}
					else
					{
						if(rssi_level == 1)
							ratr_bitmap &= 0x00000000000f0000;
						else if(rssi_level == 2)
							ratr_bitmap &= 0x00000000000ff000;
						else
							ratr_bitmap &= 0x00000000000ff005;
					}	
				}
				else if (rf_mimo_mode == RF_2T2R)
				{
					if (bCurTxBW40MHz)
					{
						if(rssi_level == 1)
							ratr_bitmap &= 0x000000000f8f0000;
						else if(rssi_level == 2) {
							//ratr_bitmap &= 0x0f8ff000;
							ratr_bitmap &= 0x000000000ffff000;
						}
						else {
							//ratr_bitmap &= 0x0f8ff015;
							ratr_bitmap &= 0x000000000ffff015;
						}
					}
					else
					{
						if(rssi_level == 1)
							ratr_bitmap &= 0x000000000f8f0000;
						else if(rssi_level == 2) {
							//ratr_bitmap &= 0x0f8ff000;
							ratr_bitmap &= 0x000000000ffff000;
						}
						else {
							//ratr_bitmap &= 0x0f8ff005;
							ratr_bitmap &= 0x000000000ffff005;
						}
					}
				}
				else
				{
					if(rssi_level == 1){
						ratr_bitmap &= 0x0000000f8f8f0000ULL;
					}
					else if(rssi_level == 2){
						ratr_bitmap &= 0x0000000ffffff000ULL;
					}
					else{
						ratr_bitmap &= 0x0000000ffffff015ULL;
					}
				}
			}
		}
		break;

#if CFG_HAL_RTK_AC_SUPPORT
		case WIRELESS_MODE_AC_24G:
		{
			// need to check !!!
			ratr_index = RATR_INX_WIRELESS_AC_24N;
			if(rssi_level == 1)
				ratr_bitmap &= 0x00000000fc3f0000ULL;
			else if(rssi_level == 2)
				ratr_bitmap &= 0x00000000fffff000ULL;
			else
				ratr_bitmap &= 0x00000000ffffffffULL;
		}
		break;
		
		case WIRELESS_MODE_AC_5G:
		{
			ratr_index = RATR_INX_WIRELESS_AC_N;

			if (rf_mimo_mode == RF_1T1R) {
				ratr_bitmap &= 0x00000000003ff010ULL;
				if(rssi_level == 3)
					ratr_bitmap &= 0x00000000000ff010ULL;
			}
			else if (rf_mimo_mode == RF_2T2R) { 
				if (rssi_level==1)
					ratr_bitmap &= 0x00000000fe3f8010ULL;
				else if (rssi_level==2)
					ratr_bitmap &= 0x00000000fffff010ULL;
				else
					ratr_bitmap &= 0x00000000fffffff0ULL;	
			} 
			else {
				if (rssi_level==1)
					ratr_bitmap &= 0x000003f8fe3f8010ULL;
				else if (rssi_level==2)
					ratr_bitmap &= 0x000003fffffff010ULL;
				else					
					ratr_bitmap &= 0x000003fffffff010ULL; // VHT 3SS MCS0~9
			}

			//marked to prevent resize ratr_bitmap from 8-byte to 4-byte
			//ratr_bitmap &= (Adapter->pmib->dot11acConfigEntry.dot11VHT_TxMap << 12)|0xff0;
		}
		break;
#endif

		default:
			ratr_index = RATR_INX_WIRELESS_NGB;
			
			if(rf_mimo_mode == RF_1T1R)
				ratr_bitmap &= 0x00000000000ff0ffULL;
			else
				ratr_bitmap &= 0x000000000f8ff0ffULL;
			break;
	}

	BW = Get_RA_BW88XX(bCurTxBW80MHz, bCurTxBW40MHz);
	bShortGI = Get_RA_ShortGI88XX(Adapter, pEntry, WirelessMode, BW);
	pstat->cmn_info.ra_info.is_support_sgi = bShortGI;

	pstat->cmn_info.ra_info.rate_id = MRateIdxToARFRId88XX(Adapter, ratr_index, rf_mimo_mode);
	pstat->cmn_info.bw_mode = BW;
	pstat->cmn_info.ra_info.ra_bw_mode = BW;

#endif

	// for debug, set vht_txmap for RAMask
	#if CFG_HAL_RTK_AC_SUPPORT
	if (WirelessMode & WIRELESS_MODE_AC_5G)
		ratr_bitmap &= (VHT_TxMap << 12)|0xff0;
	else if(WirelessMode & WIRELESS_MODE_AC_24G)
		ratr_bitmap &= (VHT_TxMap << 12)|0xfff;
	#endif	
	
	H2CCommand[0] = REMAP_AID(pstat);
	H2CCommand[1] = (pstat->cmn_info.ra_info.rate_id) | (bShortGI?0x80:0x00);	
	H2CCommand[2] = BW |Get_VHT_ENI88XX(IOTAction, WirelessMode, ratr_bitmap);
	

	H2CCommand[2] |= BIT6;			// DisableTXPowerTraining

	H2CCommand[3] = (u1Byte)(ratr_bitmap & 0x000000ff);
	H2CCommand[4] = (u1Byte)((ratr_bitmap & 0x0000ff00) >>8);
	H2CCommand[5] = (u1Byte)((ratr_bitmap & 0x00ff0000) >> 16);
	H2CCommand[6] = (u1Byte)((ratr_bitmap & 0xff000000) >> 24);
	pstat->tx_ra_bitmap = ratr_bitmap;
	
	FillH2CCmd88XX(Adapter, H2C_88XX_RA_MASK, 7, H2CCommand);
#if 0
    printk("H2C H2C_88XX_RA_MASK 0x40\n");
    printk("%s %d ratr_bitmap = %llx \n",__func__,__LINE__,ratr_bitmap);
    printk("pstat->cmn_info.ra_info.rate_id = %x \n",pstat->cmn_info.ra_info.rate_id);
    printk("pstat->cmn_info.aid = %x \n",pstat->cmn_info.aid);
    printk("bShortGI = %x \n",bShortGI);
    printk("MimoPs = %x \n",MimoPs);    
	printk("UpdateHalRAMask88XX(): bitmap = %x ratr_index = %x, MACID = %x, ShortGI:%x, MimoPs=%d\n", 
			ratr_bitmap, pstat->cmn_info.ra_info.rate_id,  REMAP_AID(pstat), bShortGI, MimoPs);

	panic_printk("Cmd: %02x, %02x, %02x, %02x, %02x, %02x, %02x  \n",
					H2CCommand[0] ,H2CCommand[1], H2CCommand[2],
					H2CCommand[3] ,H2CCommand[4], H2CCommand[5], H2CCommand[6]      );
#endif
	H2CCommand[0] = REMAP_AID(pstat);
	H2CCommand[3] = (u1Byte)((ratr_bitmap>>32) & 0x000000ff);
	H2CCommand[4] = (u1Byte)(((ratr_bitmap>>32) & 0x0000ff00) >>8);
    H2CCommand[5] = H2CCommand[6] = 0;
	#if (IS_RTL8814A_SERIES)
	FillH2CCmd88XX(Adapter, H2C_88XX_RA_MASK_3SS, 7, H2CCommand);
	#endif

	//SetBcnCtrlReg88XX(Adapter, BIT3, 0);
#if 0
	printk("H2C H2C_88XX_RA_MASK_3SS 0x46\n");
	printk("%s %d ratr_bitmap = %llx \n",__func__,__LINE__,ratr_bitmap);
	printk("pstat->cmn_info.ra_info.rate_id = %x \n",pstat->cmn_info.ra_info.rate_id);
	printk("pstat->cmn_info.aid = %x \n",pstat->cmn_info.aid);
	printk("bShortGI = %x \n",bShortGI);
	printk("MimoPs = %x \n",MimoPs);

	//printk("UpdateHalRAMask88XX(): bitmap = %x ratr_index = %x, MACID:%x, ShortGI:%x, MimoPs=%d\n", 
	//	ratr_bitmap, pstat->cmn_info.ra_info.rate_id,  REMAP_AID(pstat), bShortGI, MimoPs);
	
	panic_printk("Cmd: %02x, %02x, %02x, %02x, %02x, %02x, %02x  \n",
		H2CCommand[0] ,H2CCommand[1], H2CCommand[2],
		H2CCommand[3] ,H2CCommand[4], H2CCommand[5], H2CCommand[6]		);
#endif	
}

void
UpdateHalMSRRPT88XX(
	IN HAL_PADAPTER     Adapter,
	HAL_PSTAINFO        pEntry,
	u1Byte              opmode
	)
{
	u1Byte		H2CCommand[7] ={0};
    update_remapAid(Adapter,pEntry);
	H2CCommand[0] = opmode & 0x01;
	H2CCommand[1] = REMAP_AID(pEntry) & 0xff;
	H2CCommand[2] = H2CCommand[3] = H2CCommand[4] = H2CCommand[5] = H2CCommand[6] = 0;
	FillH2CCmd88XX(Adapter, H2C_88XX_MSRRPT, 7, H2CCommand);
	
//	panic_printk("UpdateHalMSRRPT88XX Cmd: %02x, %02x, %02x  \n",
//		H2CCommand[0] ,H2CCommand[1], H2CCommand[2]);
}


static VOID 
SetBCNRsvdPage88XX
(    
	IN HAL_PADAPTER     Adapter,
    u1Byte              numOfAP,
	pu1Byte             loc_bcn
)
{
    u1Byte      H2CCommand[7] ={0};
    u1Byte      i;

    for(i=0;i<numOfAP;i++)
    {
        H2CCommand[i] = *(loc_bcn+i);
    }   

   	FillH2CCmd88XX(Adapter, H2C_88XX_BCN_RSVDPAGE, 7, H2CCommand);
}

static VOID 
SetProbeRsvdPage88XX
(    
	IN HAL_PADAPTER     Adapter,
    u1Byte              numOfAP,
	pu1Byte             loc_probe
)
{
    u1Byte      H2CCommand[7] ={0};
    u1Byte      i;

    for(i=0;i<numOfAP;i++)
    {
        H2CCommand[i] = *(loc_probe+i);
    }   

   	FillH2CCmd88XX(Adapter, H2C_88XX_PROBE_RSVDPAGE, 7, H2CCommand);
}

static VOID SetGpioWakePin
(
        IN HAL_PADAPTER  Adapter,
        u1Byte duration,
        u1Byte en,
	u1Byte pull_high,
	u1Byte pulse,
	u1Byte pin
)
{
   u1Byte  H2CCommand[7] ={0};

   H2CCommand[0] =   pin;
   H2CCommand[0] |= (pulse << 5);
   H2CCommand[0] |= (pull_high << 6);
   H2CCommand[0] |= (en << 7);

   H2CCommand[1] = duration;
   
   H2CCommand[2] = 0xff;

   H2CCommand[3] = H2CCommand[4] = H2CCommand[5] = H2CCommand[6] = 0;

   printk("gpio wakeup=%x\n", H2CCommand[0]); 

   FillH2CCmd88XX(Adapter, H2C_88XX_WAKEUP_PIN, 7, H2CCommand);
}

#ifdef SDIO_AP_OFFLOAD
static VOID SetRepeatWakePulse
(
	IN HAL_PADAPTER  Adapter,
	u1Byte en,
	u1Byte triggerTimeUnit,
	u1Byte duration
)
{
	u1Byte  H2CCommand[8] ={0};

	if (en)
	{
		H2CCommand[0] = BIT(0);

		switch(triggerTimeUnit) {
			case TRIGGER_TIME_8MILISEC:
				H2CCommand[1] = duration;
				break;
			case TRIGGER_TIME_2SEC:
				H2CCommand[2] = duration;
				break;
			default:case TRIGGER_TIME_NONE:
				break;
		}
	}

	//printk("SetRepeatWakePulse88XX H2CCommand[1]=%d H2CCommand[2]=%d\n", H2CCommand[1], H2CCommand[2]); 
	FillH2CCmd88XX(Adapter, H2C_88XX_REPEAT_WAKE_PULSE, 3, H2CCommand);
}
#endif


VOID SetMACIDPauseDriverInfo88XX
(
    IN HAL_PADAPTER  Adapter,
    IN  BOOLEAN      bSleep,
	IN  u4Byte       aid
)
{
    u1Byte H2CCommand[7] ={0};
    u1Byte macid_shift_flag;
    u1Byte macid_pause_1;
    u1Byte macid_pause_2;
    u1Byte macid_en_1;
    u1Byte macid_en_2;
    u1Byte macid_en;


    macid_shift_flag = aid/16;
    macid_en = aid % 16;
    if(macid_en < 8)
        macid_en_1 = BIT(macid_en);
    else
        macid_en_2 = BIT(macid_en-8);

    if(bSleep){
        if(macid_en < 8)
            macid_pause_1 = BIT(macid_en);
        else
            macid_pause_2 = BIT(macid_en-8);
    }    
    else{
        macid_pause_1 = 0;
        macid_pause_2 = 0;
    }
    
   H2CCommand[0] = macid_shift_flag & 0x1F;

   H2CCommand[1] = macid_pause_1;
   H2CCommand[2] = macid_pause_2;
   H2CCommand[3] = macid_en_1;
   H2CCommand[4] = macid_en_2;
   H2CCommand[5] = H2CCommand[6] = 0;

   FillH2CCmd88XX(Adapter, H2C_88XX_MACID_PAUSE_DRV_INFO, 7, H2CCommand);
}
static VOID 
SetAPOffloadEnable88XX
(    
	IN HAL_PADAPTER     Adapter,
    u1Byte              bEn, 
#ifdef CONFIG_POWER_SAVE
	u1Byte              bOn,
#endif
    u1Byte              numOfAP,
	u1Byte              bHidden,	
	u1Byte              bDenyAny
#ifdef CONFIG_POWER_SAVE
	,u1Byte             wakeDelay
	,u1Byte             wakeTimeout
#endif
)
{

    
    u1Byte      H2CCommand[7] ={0};
    u1Byte      i;

	if(bEn)
	{
		H2CCommand[0] |= BIT(0);
    H2CCommand[1] = bHidden;
    H2CCommand[2] = bDenyAny;
#ifdef CONFIG_POWER_SAVE
		if(bOn)
		{
			H2CCommand[0] |= BIT(2);//linked 1,idle 0
			H2CCommand[0] |= BIT(3);//EnAutoWake
		}
#endif
	}
    H2CCommand[0] = bEn;

    H2CCommand[1] = bHidden;
    H2CCommand[2] = bDenyAny;
    H2CCommand[3] = H2CCommand[4] = H2CCommand[5] = H2CCommand[6] = 0;

#if 0
    if(bHidden)
    {
        for(i=0;i<numOfAP;i++)
        {
            H2CCommand[1] |= BIT(i);           
        }   
    }

    if(bDenyAny)
    {
        for(i=0;i<numOfAP;i++)
        {
            H2CCommand[2] |= BIT(i);            
        }   
    }
#endif    
#ifdef CONFIG_POWER_SAVE
   H2CCommand[3]= wakeDelay;
   H2CCommand[4]= wakeTimeout;
   //printk("%s SetAPOffloadEnable88XX  H2CCommand[3]=%d\n", __FUNCTION__,wakeDelay);
   FillH2CCmd88XX(Adapter, H2C_88XX_AP_OFFLOAD, 5, H2CCommand);
#else
	FillH2CCmd88XX(Adapter, H2C_88XX_AP_OFFLOAD, 7, H2CCommand);
#endif
}


void
SetAPOffload88XX(
	IN HAL_PADAPTER     Adapter,
	u1Byte              bEn,
#ifdef CONFIG_POWER_SAVE
	u1Byte              bOn,
#endif
	u1Byte              numOfAP,
	u1Byte              bHidden,	
	u1Byte              bDenyAny,
	pu1Byte             loc_bcn,
	pu1Byte             loc_probe
	)
{
    if(bEn)
    {
#ifdef SDIO_AP_OFFLOAD
#if (IS_RTL8192E_SERIES)
	     SetGpioWakePin(Adapter, PULSE_DURATION, 1, 1, 1, 14);
#endif // IS_RTL8192E_SERIES
#if (IS_RTL8822B_SERIES || IS_RTL8821C_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
	     SetGpioWakePin(Adapter, PULSE_DURATION, 1, 1, 1, 10);
#endif // (IS_RTL8822B_SERIES || IS_RTL8821C_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
	     //SetRepeatWakePulse(Adapter, 1, REPEAT_TRIGGER_UNIT, REPEAT_TRIGGER_DURATION);
#else
	     SetGpioWakePin(Adapter, 2, 1, 1, 1, 14);
#endif

        SetBCNRsvdPage88XX(Adapter,numOfAP,loc_bcn);
        SetProbeRsvdPage88XX(Adapter,numOfAP,loc_probe);      
#ifdef CONFIG_POWER_SAVE
	     SetAPOffloadEnable88XX(Adapter,bEn,bOn,numOfAP,bHidden,bDenyAny,8,254);//zyj test
#else
        SetAPOffloadEnable88XX(Adapter,bEn,numOfAP,bHidden,bDenyAny);
#endif
    }
    else
    {
#ifdef CONFIG_POWER_SAVE
	     SetAPOffloadEnable88XX(Adapter,bEn,bOn,numOfAP,bHidden,bDenyAny,0,0); //zyj test
#else
        SetAPOffloadEnable88XX(Adapter,bEn,numOfAP,bHidden,bDenyAny);
#endif
    }
}
#if CFG_HAL_ATM_REPORT    
void
SetATMReportCfg88XX(
    IN HAL_PADAPTER     Adapter,
    u1Byte              bEn,
    u1Byte              EnRanges, //BIT0 range1, BIT1 range2, BIT3 range3
    u1Byte              ATM_period,
    pu1Byte             STAMACIDs, //[0] MACID1, [1] MACID2, [2] MACID3
	pu1Byte             STALengths, //[0] length1, [1] length2, [2] length3
	u1Byte              numOfSTA
)
{
    u1Byte      H2CCommand[7] ={0};
    u1Byte      range;
    u1Byte      period;
    u1Byte      i,j;
    
    range = (EnRanges & 0x7) << 1;
    period = (ATM_period & 0xF) << 4;
        
    H2CCommand[0] = bEn | range | period;

    j=0;
    for(i=0;i<(numOfSTA*2);i=i+2)
    {
        H2CCommand[1+i] = *(STALengths+j);
        H2CCommand[2+i] = *(STAMACIDs+j);
        j++;
    }  
 
    FillH2CCmd88XX(Adapter, H2C_88XX_ATM_REPORT, 7, H2CCommand);
}
#endif

#if CFG_HAL_PWR_REDUCE   
void
SetPWRReduce88XX(
    IN HAL_PADAPTER     Adapter,
    u1Byte              bEn,
    u1Byte              bForce_duty_cycle, //Only used by MP driver, force all rate to "Duty_cycle_by_forced"
    u1Byte              Duty_cycle_by_forced, //when "Force_duty_cycle" = 1, set this value to 5~100ms, the value will be mod 5 by FW to ensure the unit to be 5ms.
    u1Byte              NumOfRateSet, //The number of the RateIdx set in "Rate2DutyCycleMap".
	u1Byte              Loc_Rate2DutyCycleMap //The offset of "Rate2DutyCycleMap" in rsvd page.
)
{
    u1Byte      H2CCommand[4] ={0};
        
    H2CCommand[0] = bEn | bForce_duty_cycle << 1;
    H2CCommand[1] = Duty_cycle_by_forced;
    H2CCommand[2] = NumOfRateSet;
	H2CCommand[3] = Loc_Rate2DutyCycleMap;
 
 
    FillH2CCmd88XX(Adapter, H2C_88XX_PWR_REDUCE, 4, H2CCommand);
}

void HalTxDutyParsing88XX
(
    IN      HAL_PADAPTER        Adapter,
    IN      WLAN_TXDUTY_CFG     TxDutyArray[]

)
{
#if 0
	WLAN_TXDUTY_CFG 	   TxDutyCfgCmd = {0};
	u32 				   AryIdx=0;
	u32					   DlRsvdAddr,page,addr;
	DlRsvdAddr=HAL_RTL_R16(REG_BCN_CTRL_0)+4;
	
	do {
		TxDutyCfgCmd=TxDutyArray[AryIdx];
		//printk("test = %x\n",TxDutyCfgCmd.valid);
		addr = 0x780000 + DlRsvdAddr*128 + 2*AryIdx;
		page = addr >> 12;
		//printk("DlRsvdAddr =%x addr = %x page = %x\n",DlRsvdAddr,addr,page);
		if(TxDutyCfgCmd.valid==0xFF)
			break;
		else{
			//printk("valid = %d rate = %d duty = %d\n",TxDutyCfgCmd.valid,TxDutyCfgCmd.rate,TxDutyCfgCmd.duty);
			HAL_RTL_W32(REG_PKTBUF_DBG_CTRL,page);
			HAL_RTL_W16(0x8000+(addr&0xfff), TxDutyCfgCmd.rate <<8 | TxDutyCfgCmd.duty);
			//printk("%s page = %x rate&duty = %x \n",Adapter->dev->name,page,TxDutyCfgCmd.rate <<8 | TxDutyCfgCmd.duty);
		 }		
		AryIdx++;
	}while(1);

#else
	PWLAN_TXDUTY_CFG	TxDutyCfgCmd;
	u32					AryIdx=0;
	u32					DlRsvdAddr,page,addr;
	u32					first_page;
	int					loop;

	DlRsvdAddr = (HAL_RTL_R16(REG_BCN_CTRL_0) & 0xfff) + 4;
	addr = 0x780000 + DlRsvdAddr * 128;
	first_page = page = addr >> 12;
	HAL_RTL_W32(REG_PKTBUF_DBG_CTRL, page | BIT20);
	for (loop=0; loop<1000; loop++) {
		if (HAL_RTL_R32(REG_PKTBUF_DBG_CTRL) & BIT20)
			mdelay(1);
		else
			break;
	}

	for (AryIdx=0; AryIdx<=HALMAC_VHT_NSS4_MCS9; AryIdx++) {
		//printk("%s DlRsvdAddr %x addr %x page %x\n", Adapter->dev->name, DlRsvdAddr, addr, page);
		TxDutyCfgCmd = TxDutyArray + AryIdx;
		//printk("valid %d rate %d duty %d\n", TxDutyCfgCmd->valid, TxDutyCfgCmd->rate, TxDutyCfgCmd->duty);
		if (TxDutyCfgCmd->valid == 1)
			HAL_RTL_W16(0x8000+(addr&0xfff), (TxDutyCfgCmd->rate<<8) | TxDutyCfgCmd->duty);
		else if (TxDutyCfgCmd->valid == 0xFF)
			break;

		addr += 2;
		page = addr >> 12;
		if (page != first_page) {
			HAL_RTL_W32(REG_PKTBUF_DBG_CTRL, page);
			first_page = page;
		}
	}



#endif
}

void ClearHalTxDutyParsing88XX
(
    IN      HAL_PADAPTER        Adapter
)
{
	u32 				   AryIdx=0;
	u32					   DlRsvdAddr,page,addr;
	DlRsvdAddr=HAL_RTL_R16(REG_BCN_CTRL_0)+5;	
	DlRsvdAddr = DlRsvdAddr &0xfff;
	do {
		//printk("test = %x\n",TxDutyCfgCmd.valid);
		addr = 0x780000 + DlRsvdAddr*128 + 2*AryIdx;
		page = addr >> 12;
		//printk("%s DlRsvdAddr =%x addr = %x page = %x\n",Adapter->dev->name,DlRsvdAddr,addr,page);
		if(AryIdx==85)
			break;
		else{
			//printk("valid = %d rate = %d duty = %d\n",TxDutyCfgCmd.valid,TxDutyCfgCmd.rate,TxDutyCfgCmd.duty);
			HAL_RTL_W32(REG_PKTBUF_DBG_CTRL,page);
			HAL_RTL_W16(0x8000+(addr&0xfff), 0);
			//printk("%s page = %x rate&duty = %x \n",Adapter->dev->name,page,TxDutyCfgCmd.rate <<8 | TxDutyCfgCmd.duty);
		 }		
		AryIdx++;
	}while(1);

}

void TxDutyCycleCtrl88XX(
    IN HAL_PADAPTER     Adapter
)
{
	WLAN_TXDUTY_CFG Rate2TxDutyTable[100]=
    {
        data_8814B_TxDuty
    };

    HalTxDutyParsing88XX(Adapter,Rate2TxDutyTable);
}

#endif

#ifdef HW_DETECT_PS_OFFLOAD
void 
MacidPauseCtrl88XX(
    IN HAL_PADAPTER     Adapter,
    u1Byte macid,
    u1Byte setPause
)
{
    unsigned char H2CCommand[7] = {0};

    H2CCommand[0] = BIT(2);
    if(setPause)
        H2CCommand[0] = H2CCommand[0] | BIT(3);
    H2CCommand[1] = macid;
    H2CCommand[2] = H2CCommand[3] = H2CCommand[4] = H2CCommand[5] = H2CCommand[6] = 0;
    FillH2CCmd88XX(Adapter, H2C_88XX_APPS_OFFLOAD_CTRL, 7, H2CCommand);
}
#endif

#ifdef AP_PS_Offlaod
void
SetAPPSOffload88XX(
    IN HAL_PADAPTER     Adapter,
    u1Byte _PS_offload_En, 
    u1Byte _phase, 
    u1Byte _pause_Qnum_limit,
    u1Byte _timeout_time
    )
{
		u1Byte H2CCommand[7] = {0};
		u1Byte PS_offload_En, phase, pause_Qnum_limit, timeout_time, pwrIntEn,agingEn;
		PS_offload_En = _PS_offload_En & BIT(0);
		phase = (_phase & BIT(0)) << 1;
#ifdef STA_AGING_FUNC_OFFLOAD
		if( IS_SUPPORT_AGING_FUNC_OFFLOAD(Adapter) && PS_offload_En){
			if((_pause_Qnum_limit!=0) || (_timeout_time!=0)){
				agingEn = 1 << 3;
				pause_Qnum_limit = (_pause_Qnum_limit & 0x1f);
				timeout_time = (_timeout_time & 0x3) << 5;
			}
		}else
			agingEn = 0;
#endif  
#ifdef HW_DETECT_PS_OFFLOAD
		if(PS_offload_En)
			pwrIntEn = 1 << 2;
		else
			pwrIntEn = 0;
#endif
		H2CCommand[0] = PS_offload_En | phase | pwrIntEn | agingEn;
		H2CCommand[1] = pause_Qnum_limit | timeout_time;
		H2CCommand[2] = H2CCommand[3] = H2CCommand[4] = H2CCommand[5] = H2CCommand[6] = 0;
		FillH2CCmd88XX(Adapter, H2C_88XX_APPS_OFFLOAD_SETTING, 7, H2CCommand);

}
#endif


#ifdef AP_SWPS_OFFLOAD
void 
ClearDropID88XX(
    IN HAL_PADAPTER     Adapter,
    u1Byte macid,
    u1Byte DropID
)
{
    unsigned char H2CCommand[7] = {0};

    H2CCommand[0] = BIT(4);
    H2CCommand[1] = macid;
    H2CCommand[2] = BIT(DropID);
    H2CCommand[3] = H2CCommand[4] = H2CCommand[5] = H2CCommand[6] = 0;
    FillH2CCmd88XX(Adapter, H2C_88XX_APPS_OFFLOAD_CTRL, 7, H2CCommand);
}
void
MACIDSWPSCtrl88XX(
    //IN HAL_PADAPTER     Adapter,
    struct rtl8192cd_priv *priv,
    struct stat_info *pstat,
    u1Byte enable
)
{
    unsigned char H2CCommand[7] = {0};
    u8 macid = REMAP_AID(pstat);

    if(enable)
        H2CCommand[0] = BIT0 | BIT1;
    else
        H2CCommand[0] = BIT0;
    
    H2CCommand[1] = macid;
    H2CCommand[2] = 0;
    H2CCommand[3] = 0;
    H2CCommand[4] = H2CCommand[5] = H2CCommand[6] = 0;  
        
    FillH2CCmd88XX(priv, H2C_88XX_APPS_OFFLOAD_CTRL, 7, H2CCommand);
    
}
void
AgingFuncOfflaod88XX(
    IN HAL_PADAPTER     Adapter,
    u1Byte queuenum,
    u1Byte expiretime
)
{
    unsigned char H2CCommand[7] = {0};
    
    H2CCommand[0] = (queuenum << 2) | BIT0; //fix to phase 0
    H2CCommand[1] = expiretime;
    H2CCommand[2] = 0;
    H2CCommand[3] = 0;
    H2CCommand[4] = H2CCommand[5] = H2CCommand[6] = 0; 
        
    FillH2CCmd88XX(Adapter, H2C_88XX_APPS_OFFLOAD_SETTING, 7, H2CCommand);

}

void
UpdateMacidSWPSInfo88XX(
    IN HAL_PADAPTER     Adapter,
    u1Byte macid,
    u1Byte IsInSWPS

)
{
    unsigned char H2CCommand[7] = {0};
    
    H2CCommand[0] = BIT(0);
    if(IsInSWPS)
        H2CCommand[0] = H2CCommand[0] | BIT(1);
    H2CCommand[1] = macid;
    H2CCommand[2] = H2CCommand[3] = H2CCommand[4] = H2CCommand[5] = H2CCommand[6] = 0;
        
    FillH2CCmd88XX(Adapter, H2C_88XX_APPS_OFFLOAD_CTRL, 7, H2CCommand);

}
#endif

#if defined(SOFTAP_PS_DURATION) || defined(CONFIG_POWER_SAVE) || defined(CONFIG_PCIE_POWER_SAVING)
VOID 
SetSAPPS88XX
(    
	IN HAL_PADAPTER     Adapter,
	u1Byte en,
#if defined(CONFIG_POWER_SAVE) || defined(CONFIG_PCIE_POWER_SAVING)
	u1Byte en_32K,
	u1Byte lps,
#endif
	u1Byte duration
)
{
	 u1Byte      H2CCommand[7] ={0};
	 if(en)
	{
#ifdef SOFTAP_PS_DURATION
	 	  H2CCommand[0] = BIT(0)|BIT(1)|BIT(2);  //BIT(1) enter 32K  BIT(0) enter PS BIT 2 low power rx
#elif defined(CONFIG_POWER_SAVE) || defined(CONFIG_PCIE_POWER_SAVING)
		H2CCommand[0] = BIT(0);
#endif
	}
	 else
		H2CCommand[0] = 0;//~(BIT(0)|BIT(1)|BIT(2));

#if defined(CONFIG_POWER_SAVE) || defined(CONFIG_PCIE_POWER_SAVING)
	if(en_32K)
		H2CCommand[0]|=BIT(1);
	if(lps)
		H2CCommand[0]|=BIT(2);
#endif

	 H2CCommand[1] = duration;
H2CCommand[2] = H2CCommand[3] = H2CCommand[4] = H2CCommand[5] = H2CCommand[6] = 0;
//	printk("SetSAPPS88XX H2CCommand[0]=%x H2CCommand[1]=%d\n",H2CCommand[0],H2CCommand[1]);
	 FillH2CCmd88XX(Adapter, H2C_88XX_SAP_PS, 7, H2CCommand);

}
#endif // defined(SOFTAP_PS_DURATION) || defined(CONFIG_POWER_SAVE)



// This function is call before download Rsvd page
// Function input 1. current len
// return 1. the len after add dmmuy byte, 2. the page location

u4Byte
GetRsvdPageLoc88XX
( 
	IN  IN HAL_PADAPTER     Adapter,
    IN  u4Byte              frlen,
    OUT pu1Byte             loc_page
)
{
#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8197F_SERIES)
	if ( IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter)) {
    if(frlen%PBP_PSTX_SIZE)
	{
		frlen = (frlen+PBP_PSTX_SIZE-(frlen%PBP_PSTX_SIZE)) ;
	}

    *(loc_page) = (u1Byte)(frlen /PBP_PSTX_SIZE);
	}
#endif // (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES)

#if (IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES  || IS_RTL8198F_SERIES || IS_RTL8814B_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES|| IS_RTL8197G_SERIES)
	if ( IS_HARDWARE_TYPE_8814A(Adapter) || IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8814B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter)) {
	    if(frlen%PBP_PSTX_SIZE_V1)
	    {
	        frlen = (frlen+PBP_PSTX_SIZE_V1-(frlen%PBP_PSTX_SIZE_V1)) ;
	    } 

	    *(loc_page) = (u1Byte)(frlen /PBP_PSTX_SIZE_V1);
        
	}
#endif //  (IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES  || IS_RTL8198F_SERIES || IS_RTL8814B_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
    return frlen;
}

VOID
SetRsvdPage88XX
( 
	IN  IN HAL_PADAPTER     Adapter,
    IN  pu1Byte             prsp,
    IN  pu4Byte             beaconbuf,    
    IN  u4Byte              pktLen,  
    IN  u4Byte              bigPktLen,
    IN  u4Byte              bcnLen
)
{
   // TX_DESC_88XX tx_desc; 
   u1Byte txdesc_size = SIZE_TXDESC_88XX;

#if IS_EXIST_PCI || IS_EXIST_EMBEDDED
	if (Adapter->hci_type == RTL_HCI_PCIE) {
#if (IS_RTL8821C_SERIES)
	    if(IS_HARDWARE_TYPE_8821C(Adapter))
	        txdesc_size = SIZE_TXDESC_88XX_V1;
	    else
#endif
#if IS_RTL8814B_SERIES
	    if ( IS_HARDWARE_TYPE_8814B(Adapter))
	        txdesc_size = SIZE_TXDESC_88XX_V2_BCN;
	    else
#endif
#if IS_RTL8822B_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES || IS_RTL8197G_SERIES
    if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter))
	        txdesc_size = SIZE_TXDESC_88XX_V1;
	    else
#endif 
#if IS_RTL8814A_SERIES || IS_RTL8881A_SERIES || IS_RTL8192E_SERIES || IS_RTL8197F_SERIES || IS_RTL8198F_SERIES
if(IS_HARDWARE_TYPE_8814A(Adapter) || IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter))	       
 txdesc_size = SIZE_TXDESC_88XX;
#endif
	       
	        txdesc_size = SIZE_TXDESC_88XX;

	    GET_HAL_INTERFACE(Adapter)->FillBeaconDescHandler(Adapter, prsp-txdesc_size, (void*)prsp, pktLen, 1);
	    //FillBeaconDesc88XX(Adapter, prsp-SIZE_TXDESC_88XX, (void*)prsp, pktLen, 1);

	    SigninBeaconTXBD88XX(Adapter, beaconbuf, bigPktLen);
	    DownloadRsvdPage88XX(Adapter,beaconbuf,bcnLen,1);
	}
#endif	
}




//
//3 H2C Command
//
#if 0
BOOLEAN
IsH2CBufOccupy88XX(
    IN  HAL_PADAPTER    Adapter
)
{
    PHAL_DATA_TYPE              pHalData = _GET_HAL_DATA(Adapter);
   
    if ( HAL_RTL_R8(REG_HMETFR) & BIT(pHalData->H2CBufPtr88XX) ) {
        return _TRUE;
    }
    else {
        RT_TRACE(COMP_DBG, DBG_WARNING, ("H2CBufOccupy(%d) !!\n", 
                                            pHalData->H2CBufPtr88XX) );
        return _FALSE;
    }
}


BOOLEAN
SigninH2C88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  PH2C_CONTENT    pH2CContent
)
{
    PHAL_DATA_TYPE      pHalData = _GET_HAL_DATA(Adapter);
    u4Byte              DelayCnt = H2CBUF_OCCUPY_DELAY_CNT;
    
    //Check if h2c cmd signin buffer is occupied
    while( _TRUE == IsH2CBufOccupy88XX(Adapter) ) {
       HAL_delay_us(H2CBUF_OCCUPY_DELAY_US);
       DelayCnt--;

       if ( 0 == DelayCnt ) {
            RT_TRACE(COMP_DBG, DBG_WARNING, ("H2CBufOccupy retry timeout\n") );
            return _FALSE;
       }
       else {
            //Continue to check H2C Buf
       }
       
    }

    //signin reg in order to fit hw requirement
    if ( pH2CContent->content & BIT7 ) {
        HAL_RTL_W16(REG_HMEBOX_E0_E1 + (pHalData->H2CBufPtr88XX*2), pH2CContent->ext_content);
    }

    HAL_RTL_W16(REG_HMEBOX0 + (pHalData->H2CBufPtr88XX*4), pH2CContent->content);

	//printk("(smcc) sign in h2c %x\n", HMEBOX_0+(priv->pshare->fw_q_fifo_count*4));
    RT_TRACE(COMP_DBG, DBG_LOUD, ("sign in h2c(%d) 0x%x\n", 
                                        pHalData->H2CBufPtr88XX, 
                                        REG_HMEBOX0 + (pHalData->H2CBufPtr88XX*4)) );

    //rollover ring buffer count
    if (++pHalData->H2CBufPtr88XX > 3) {
        pHalData->H2CBufPtr88XX = 0;
    }

    return _TRUE;
}

#else
BOOLEAN
CheckFwReadLastH2C88XX(
	IN  HAL_PADAPTER    Adapter,
	IN  u1Byte          BoxNum
)
{
	u1Byte      valHMETFR;
	BOOLEAN     Result = FALSE;
	
	valHMETFR = HAL_RTL_R8(REG_HMETFR);

	if(((valHMETFR>>BoxNum)&BIT0) == 0)
		Result = TRUE;
		
	return Result;
}


enum rt_status
FillH2CCmd88XX(
	IN  HAL_PADAPTER    Adapter,
	IN	u1Byte 		    ElementID,
	IN	u4Byte 		    CmdLen,
	IN	pu1Byte		    pCmdBuffer
)
{
    PHAL_DATA_TYPE      pHalData = _GET_HAL_DATA(Adapter);
	u1Byte	            BoxNum;
	u2Byte	            BOXReg=0, BOXExtReg=0;
	u2Byte	            BOXRegLast=0, BOXExtRegLast=0;
	BOOLEAN             bFwReadClear=FALSE;
	u1Byte	            BufIndex=0;
	u2Byte	            WaitH2cLimmit=0;
	u1Byte	            BoxContent[4], BoxExtContent[4];
#if IS_EXIST_USB || IS_EXIST_SDIO
	u4Byte value = 0;
#endif
#if IS_EXIST_PCI || IS_EXIST_EMBEDDED
	u1Byte	            idx=0;
#endif
#ifdef BT_COEXIST	
	int 				i=0;
#endif
	unsigned long flags;

#ifdef BT_COEXIST	
	if(HAL_BT_DUMP){
		if(ElementID >= 0x60 && ElementID <= 0x66){
			panic_printk("[%s] ID=%x, data:",__FUNCTION__,ElementID);
			for(i=0;i<CmdLen;i++)
				panic_printk("%x ", *(pCmdBuffer+i)); 	
			panic_printk("\n");
		}
	}		
#endif	
    if(!pHalData->bFWReady) {
        //RT_TRACE(COMP_DBG, DBG_WARNING, ("H2C bFWReady=False !!\n") );
        return RT_STATUS_FAILURE;
    }

	// 1. Find the last BOX number which has been writen.
	spin_lock_irqsave(&Adapter->h2c_lock, flags);	
	BoxNum = pHalData->H2CBufPtr88XX;	//pHalData->LastHMEBoxNum;
	switch(BoxNum)
	{
		case 0:
			BOXReg = REG_HMEBOX0;
			BOXExtReg = REG_HMEBOX_E0;
			BOXRegLast = REG_HMEBOX3;
			BOXExtRegLast = REG_HMEBOX_E3;
			break;
		case 1:
			BOXReg = REG_HMEBOX1;
			BOXExtReg = REG_HMEBOX_E1;
			BOXRegLast = REG_HMEBOX0;
			BOXExtRegLast = REG_HMEBOX_E0;
			break;
		case 2:
			BOXReg = REG_HMEBOX2;
			BOXExtReg = REG_HMEBOX_E2;
			BOXRegLast = REG_HMEBOX1;
			BOXExtRegLast = REG_HMEBOX_E1;
			break;
		case 3:
			BOXReg = REG_HMEBOX3;
			BOXExtReg = REG_HMEBOX_E3;
			BOXRegLast = REG_HMEBOX2;
			BOXExtRegLast = REG_HMEBOX_E2;
			break;
		default:
			break;
	}

	if (++pHalData->H2CBufPtr88XX > 3)
		pHalData->H2CBufPtr88XX = 0;	
	spin_unlock_irqrestore(&Adapter->h2c_lock, flags);

	// 2. Check if the box content is empty.
	while(!bFwReadClear)
	{
		bFwReadClear = CheckFwReadLastH2C88XX(Adapter, BoxNum);
		if(WaitH2cLimmit == 10000) { //do the first stage, clear last cmd
			//printk("H2C cmd-TO, first stage!! BOXReg:0x%x, BoxNum:%d\n", HAL_RTL_R8(BOXReg), BoxNum);
			HAL_RTL_W32(BOXRegLast, 0x00020101);
			//printk("H2C cmd-TO, first stage!! REG_HMETFR:0x%x, BoxNum:%d\n", HAL_RTL_R8(REG_HMETFR), BoxNum);
            Adapter->pshare->h2c_box_to_first++;
		} else if(WaitH2cLimmit == 20000) { //do the second stage, clear all cmd
			//printk("H2C cmd-TO, second stage!! REG_HMEBOX0:0x%x, REG_HMEBOX1:0x%x, REG_HMEBOX2:0x%x, REG_HMEBOX3:0x%x, BoxNum:%d\n", HAL_RTL_R8(REG_HMEBOX0), HAL_RTL_R8(REG_HMEBOX1), HAL_RTL_R8(REG_HMEBOX2), HAL_RTL_R8(REG_HMEBOX3), BoxNum);		
			HAL_RTL_W32(REG_HMEBOX0, 0x00020101);
			HAL_RTL_W32(REG_HMEBOX1, 0x00020101);
			HAL_RTL_W32(REG_HMEBOX2, 0x00020101);
			HAL_RTL_W32(REG_HMEBOX3, 0x00020101);
			//printk("H2C cmd-TO, second stage!! REG_HMETFR:0x%x, BoxNum:%d\n", HAL_RTL_R8(REG_HMETFR), BoxNum);
            Adapter->pshare->h2c_box_to_second++;
		} else if(WaitH2cLimmit >= 30000) {
			//printk("H2C cmd-TO, final stage!! REG_HMETFR:0x%x, BoxNum:%d\n", HAL_RTL_R8(REG_HMETFR), BoxNum);
			Adapter->pshare->h2c_box_full++;
			return RT_STATUS_FAILURE;
		}
		else if(!bFwReadClear)
		{	
			HAL_delay_us(10); //us
		}
		WaitH2cLimmit++;
	}

	// 4. Fill the H2C cmd into box 	
	HAL_memset(BoxContent, 0, sizeof(BoxContent));
	HAL_memset(BoxExtContent, 0, sizeof(BoxExtContent));
	
	BoxContent[0] = ElementID; // Fill element ID
//	RTPRINT(FFW, FW_MSG_H2C_CONTENT, ("[FW], Write ElementID BOXReg(%4x) = %2x \n", BOXReg, ElementID));

	switch(CmdLen)
	{
	case 1:
	case 2:
	case 3:
	{
		//BoxContent[0] &= ~(BIT7);
		HAL_memcpy((pu1Byte)(BoxContent)+1, pCmdBuffer+BufIndex, CmdLen);
#if IS_EXIST_USB || IS_EXIST_SDIO
		if (Adapter->hci_type == RTL_HCI_USB || Adapter->hci_type == RTL_HCI_SDIO) {
			HAL_memcpy(&value, BoxContent, 4);
			HAL_RTL_W32(BOXReg, le32_to_cpu(value));
		}
#endif
#if IS_EXIST_PCI || IS_EXIST_EMBEDDED
		if (Adapter->hci_type == RTL_HCI_PCIE) {		
			//For Endian Free.
			for(idx= 0; idx < 4; idx++)
			{
				HAL_RTL_W8(BOXReg+idx, BoxContent[idx]);
			}
		}
#endif
		break;
	}
	case 4: 
	case 5:
	case 6:
	case 7:
	{
		//BoxContent[0] |= (BIT7);
		HAL_memcpy((pu1Byte)(BoxExtContent), pCmdBuffer+BufIndex+3, (CmdLen-3));
		HAL_memcpy((pu1Byte)(BoxContent)+1, pCmdBuffer+BufIndex, 3);
#if IS_EXIST_USB || IS_EXIST_SDIO
		if (Adapter->hci_type == RTL_HCI_USB || Adapter->hci_type == RTL_HCI_SDIO) {		
			HAL_memcpy(&value, BoxExtContent, 4);
			HAL_RTL_W32(BOXExtReg, le32_to_cpu(value));
			HAL_memcpy(&value, BoxContent, 4);
			HAL_RTL_W32(BOXReg, le32_to_cpu(value));
		}
#endif
#if IS_EXIST_PCI || IS_EXIST_EMBEDDED
		if (Adapter->hci_type == RTL_HCI_PCIE) {
			//For Endian Free.
			for(idx = 0 ; idx < 4 ; idx ++)
			{
				HAL_RTL_W8(BOXExtReg+idx, BoxExtContent[idx]);
			}		
			for(idx = 0 ; idx < 4 ; idx ++)
			{
				HAL_RTL_W8(BOXReg+idx, BoxContent[idx]);
			}
		}
#endif
		break;
	}
	
	default:
//RTPRINT(FFW, FW_MSG_H2C_STATE, ("[FW], Invalid command len=%d!!!\n", CmdLen));
		return RT_STATUS_FAILURE;
	}

//RTPRINT(FFW, FW_MSG_H2C_CONTENT, ("[FW], pHalData->LastHMEBoxNum = %d\n", pHalData->LastHMEBoxNum));
	return RT_STATUS_SUCCESS;
}
#endif


#ifdef WLAN_SUPPORT_FW_CMD //tingchu

enum rt_status
FillFWCmd88XX(
	IN  HAL_PADAPTER    Adapter,
	IN	u1Byte 		    ElementID,
	IN  u2Byte          SubCmdID,
	IN	u4Byte 		    CmdLen,
	IN  u1Byte          needACK,
	IN	pu1Byte		    pCmdBuffer
)
{

    u1Byte              FWCmdContent[24];
    u4Byte              FWCmd_index;
    u1Byte              FWCmd_hdr[8];
    u1Byte              FWCmd_buf_hdr_cont[32];
        
    if(IS_SUPPORT_FW_CMD(Adapter)) 
    {
        HAL_memset(FWCmdContent, 0, sizeof(FWCmdContent));  
        #if 0
        if(ElementID != 0xFF){
            H2CContent[0] = ElementID; // Fill element ID
            HAL_memcpy((pu1Byte)(H2CContent)+1, pCmdBuffer, CmdLen);    
#if IS_SUPPORT_MACHAL_API
            if(IS_HARDWARE_MACHAL_SUPPORT(Adapter))
            {
                printk("no support halmac_send_original_h2c!!!\n");
                //GET_MACHALAPI_INTERFACE(Adapter)->halmac_send_original_h2c(Adapter->pHalmac_adapter,H2CContent);  
                return RT_STATUS_SUCCESS;
            }
#endif // IS_SUPPORT_MACHAL_API    
        
            SetH2CPacketHeader88XX(Adapter,h2c_hdr,needACK,0x20,0x0,ElementID,0x0);//32 bytes
        
        }
        else
        #endif
        SetFWCmdHeader88XX(Adapter,FWCmd_hdr,needACK,0x20,0x1,ElementID,SubCmdID);
            
        HAL_memcpy(FWCmd_buf_hdr_cont,FWCmd_hdr,8);
        if(ElementID!=0xFF)
            HAL_memcpy(FWCmd_buf_hdr_cont+8,FWCmdContent,CmdLen+1);
        else
            HAL_memcpy(FWCmd_buf_hdr_cont+8,pCmdBuffer,CmdLen);

        SendFWCmd88XX(Adapter,FWCmd_buf_hdr_cont,sizeof(FWCmd_buf_hdr_cont));

        return RT_STATUS_SUCCESS;
    }
    else        
        printk("this chip dose not sopport FW Cmd!\n");


}

VOID 
SetFWCmdHeader88XX(
    IN VOID *pDriver_adapter, 
	OUT u8 *pHal_FWCmd_hdr, 
	IN u8 ack,
	IN u16 totallength,
	IN u8 category,
	IN u8 CMDID,
	IN u16 sub_CMDID
)
{
    HAL_PADAPTER    Adapter =      (HAL_PADAPTER)pDriver_adapter;

    HAL_memset(pHal_FWCmd_hdr, 0, 8);	

	if(category==0)
        pHal_FWCmd_hdr[0] = 0x00;
    else
        pHal_FWCmd_hdr[0] = 0x01;

    if(category==1)
        pHal_FWCmd_hdr[1] = CMDID; //0xFF
    
    if(category==1){
        pHal_FWCmd_hdr[2] = sub_CMDID & 0xFF;
        pHal_FWCmd_hdr[3] = (sub_CMDID & 0xFF00) >> 8;
    }
        
    pHal_FWCmd_hdr[4] = totallength & 0xFF;
    pHal_FWCmd_hdr[5] = (totallength & 0xFF00) >> 8;
    
	//H2C_CMD_HEADER_SET_SEQ_NUM(pHal_h2c_hdr, Adapter->h2c_packet_seq);
	pHal_FWCmd_hdr[6] = Adapter->FW_Cmd_seq & 0x00FF; // also need FWCmd seq??
    pHal_FWCmd_hdr[7] = Adapter->FW_Cmd_seq & 0xFF00;
    
	if(Adapter->FW_Cmd_seq==0xFFFF)
        Adapter->FW_Cmd_seq = 0;
    else        
	    Adapter->FW_Cmd_seq++;

	if(_TRUE == ack){
		//H2C_CMD_HEADER_SET_ACK1(pHal_h2c_hdr, _TRUE);
		 pHal_FWCmd_hdr[0]=  pHal_FWCmd_hdr[0] | BIT(7);
	}

}

u8
SendFWCmd88XX(
    IN VOID *pDriver_adapter, 
    IN u8 *pBuf, 
    IN u32 size

)
{
    u4Byte queueIndex = HCI_TX_DMA_QUEUE_FWCMD;
    u1Byte DMAChannelNum = TXDESC_DMA_CHANNEL16;
    pu1Byte cur_fwcmd_buffer;
    HAL_PADAPTER    Adapter =      (HAL_PADAPTER)pDriver_adapter;
    PHCI_TX_DMA_MANAGER_88XX ptx_dma = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);
    PHCI_TX_DMA_QUEUE_STRUCT_88XX cur_q       = &(ptx_dma->tx_queue[queueIndex]);
    PTX_DESC_88XX ptx_desc    = (((PTX_DESC_88XX)(cur_q->ptx_desc_head)) + cur_q->host_idx);
    PTX_BUFFER_DESCRIPTOR cur_txbd;
    PTX_DESC_88XX pfwcmd_tx_desc, ptest;
    u1Byte i;    
    PFWCMD_PAYLOAD_88XX   fwcmd_buf_start = (PFWCMD_PAYLOAD_88XX)_GET_HAL_DATA(Adapter)->fwcmd_buf;
    cur_fwcmd_buffer = &(fwcmd_buf_start[cur_q->host_idx]);
    
    unsigned long dma_addr;
    cur_txbd =  cur_q->pTXBD_head + cur_q->host_idx;

    //pfwcmd_tx_desc = ptx_desc;
  
    memcpy(cur_fwcmd_buffer,pBuf,size);
    if (size) {
    #ifdef CFG_HAL_DBG
        for(i=0;i<size;i++){
            if(i%16==0){printk("\n");}
            printk("%x ",*(cur_fwcmd_buffer+i));
        }
        printk("\n");
    #endif
        _dma_cache_wback(( ((unsigned long)cur_fwcmd_buffer)- CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
            (u4Byte)size);
    
    
    }
    //last step, update TXBD Write index
    UpdateSWTXBDHostIdx88XX(Adapter, cur_q);
    SyncSWTXBDHostIdxToHW88XX(Adapter, queueIndex);   
    
}

#endif


#ifdef WLAN_SUPPORT_H2C_PACKET
enum rt_status
FillH2CPacket88XX(
	IN  HAL_PADAPTER    Adapter,
	IN	u1Byte 		    ElementID,
	IN  u2Byte          SubCmdID,
	IN	u4Byte 		    CmdLen,
	IN  u1Byte          needACK,
	IN	pu1Byte		    pCmdBuffer
)
{

    u1Byte              H2CContent[24];
    u4Byte              h2c_index;
    u1Byte              h2c_hdr[8];
    u1Byte              h2c_buf_hdr_cont[16];
        
    if(IS_SUPPORT_H2C_PACKET(Adapter)) 
    {
        //printk("FillH2CCmd88XX to  halmac_send_original_h2c !!!!!!!!!!!!!!!!!!!!!!!!!!!!! \n");
        HAL_memset(H2CContent, 0, sizeof(H2CContent));  
        if(ElementID != 0xFF){
            H2CContent[0] = ElementID; // Fill element ID
            HAL_memcpy((pu1Byte)(H2CContent)+1, pCmdBuffer, CmdLen);    
#if IS_SUPPORT_MACHAL_API
            if(IS_HARDWARE_MACHAL_SUPPORT(Adapter))
            {
                printk("no support halmac_send_original_h2c!!!\n");
                //GET_MACHALAPI_INTERFACE(Adapter)->halmac_send_original_h2c(Adapter->pHalmac_adapter,H2CContent);  
                return RT_STATUS_SUCCESS;
            }
#endif // IS_SUPPORT_MACHAL_API    
        
            SetH2CPacketHeader88XX(Adapter,h2c_hdr,needACK,0x20,0x0,ElementID,0x0);//32 bytes
        
        }
        else
            SetH2CPacketHeader88XX(Adapter,h2c_hdr,needACK,0x20,0x1,ElementID,SubCmdID);
            
        HAL_memcpy(h2c_buf_hdr_cont,h2c_hdr,8);
        if(ElementID!=0xFF)
            HAL_memcpy(h2c_buf_hdr_cont+8,H2CContent,CmdLen+1);
        else
            HAL_memcpy(h2c_buf_hdr_cont+8,pCmdBuffer,CmdLen);

        SendH2CPacket88XX(Adapter,h2c_buf_hdr_cont,sizeof(h2c_buf_hdr_cont));

        return RT_STATUS_SUCCESS;
    }
    else        
        printk("this chip dose not sopport H2C packet, please use FillH2CCmd88XX!\n");


}

VOID 
SetH2CPacketHeader88XX(
    IN VOID *pDriver_adapter, 
	OUT u8 *pHal_h2c_hdr, 
	IN u8 ack,
	IN u16 totallength,
	IN u8 category,
	IN u8 CMDID,
	IN u16 sub_CMDID
)
{
    HAL_PADAPTER    Adapter =      (HAL_PADAPTER)pDriver_adapter;

    HAL_memset(pHal_h2c_hdr, 0, 8);	

	if(category==0)
        pHal_h2c_hdr[0] = 0x00;
    else
        pHal_h2c_hdr[0] = 0x01;

    if(category==1)
        pHal_h2c_hdr[1] = CMDID; //0xFF
    
    if(category==1){
        pHal_h2c_hdr[2] = sub_CMDID & 0xFF;
        pHal_h2c_hdr[3] = (sub_CMDID & 0xFF00) >> 8;
    }
        
    pHal_h2c_hdr[4] = totallength & 0xFF;
    pHal_h2c_hdr[5] = (totallength & 0xFF00) >> 8;
    
	//H2C_CMD_HEADER_SET_SEQ_NUM(pHal_h2c_hdr, Adapter->h2c_packet_seq);
	pHal_h2c_hdr[6] = Adapter->h2c_packet_seq & 0x00FF;
    pHal_h2c_hdr[7] = Adapter->h2c_packet_seq & 0xFF00;
    
	if(Adapter->h2c_packet_seq==0xFFFF)
        Adapter->h2c_packet_seq = 0;
    else        
	    Adapter->h2c_packet_seq++;

	if(_TRUE == ack){
		//H2C_CMD_HEADER_SET_ACK1(pHal_h2c_hdr, _TRUE);
		 pHal_h2c_hdr[0]=  pHal_h2c_hdr[0] | BIT(7);
	}

}
u8 SendH2CPacket88XX(
    IN VOID *pDriver_adapter, 
    IN u8 *pBuf, 
    IN u32 size
)
{

    //TX_DESC_88XX               desc_data;
    PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q;
	PHCI_TX_DMA_MANAGER_88XX        ptx_dma;
    PTX_BUFFER_DESCRIPTOR           cur_txbd;    
    HAL_PADAPTER    Adapter =      (HAL_PADAPTER)pDriver_adapter;
    pu1Byte                         cur_h2c_buffer;
    u2Byte                          host_wrtie,hw_read;
    u4Byte                          DMA_QUEUE_CMD;	
    u32 counter = 100;
    PH2C_PAYLOAD_88XX   h2c_buf_start = (PH2C_PAYLOAD_88XX)_GET_HAL_DATA(Adapter)->h2c_buf;
    ptx_dma     = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);
   
#if (IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822BE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE)
    if(IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter)|| IS_HARDWARE_TYPE_8812FE(Adapter)) {    
        DMA_QUEUE_CMD = HCI_TX_DMA_QUEUE_CMD;  
    }
#endif  //(IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822BE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE)

#if IS_EXIST_RTL8198FEM || IS_EXIST_RTL8197GEM
    if ( IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter)) {
        DMA_QUEUE_CMD = HCI_TX_DMA_QUEUE_CMD_V1; 
    }
#endif  //IS_EXIST_RTL8198FEM

    cur_q       = &(ptx_dma->tx_queue[DMA_QUEUE_CMD]);
    cur_txbd    = cur_q->pTXBD_head + cur_q->host_idx;
    
    cur_h2c_buffer = &(h2c_buf_start[cur_q->host_idx]);

    //memset(&desc_data, 0, sizeof(TX_DESC_88XX));
    hw_read = GetTxQueueHWIdx88XX(pDriver_adapter,DMA_QUEUE_CMD);
    host_wrtie = cur_q->host_idx;

    //printk("h2c send packet \n");
    while(CIRC_SPACE_RTK(host_wrtie,hw_read,H2C_ENTERY_MAX_NUM) < 1){  //can use this way only when txbd num= 32
		counter--;
		if(0 == counter){
             printk("h2c page full \n");
			return FALSE;
		}
        hw_read = GetTxQueueHWIdx88XX(pDriver_adapter,DMA_QUEUE_CMD);
        host_wrtie = cur_q->host_idx;
	}
    

    memcpy(cur_h2c_buffer,pBuf,size);
    // 1. Fill desc_data
    //desc_data.hdrLen = size;
    //desc_data.pHdr = pBuf;
    // 2. FillTXDESC
    //FillH2CTxDesc88XX(pDriver_adapter, HCI_TX_DMA_QUEUE_CMD,&desc_data);

    // 3. SetTXBufferDesc
    //SetTxBufferDesc88XX(pDriver_adapter, HCI_TX_DMA_QUEUE_CMD, (PTX_DESC_DATA_88XX)&desc_data);
    
    if (size) {
        _dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
            (u4Byte)size);
    }


    //printk("cur_h2c_buffer adr = %x \n",cur_h2c_buffer);
    //RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "H2C Payload:\n", cur_h2c_buffer, 32);    

    
    UpdateSWTXBDHostIdx88XX(pDriver_adapter, cur_q);
    SyncSWTXBDHostIdxToHW88XX(pDriver_adapter, DMA_QUEUE_CMD);
    return TRUE;

}
#endif
u8 
ParseC2HPacket88XX(
    IN VOID *pDriver_adapter, 
	IN u8 *pbuf, 
	IN u32 c2hsize
)
{
	
	u8 c2h_cmd, c2h_sub_cmd_id, resource_index, h2c_cmd_id, h2c_sub_cmd_id;
	u16 h2c_seq;
	u8  segment_id=0, segment_size=0;
	//u8* pC2h_buf = halmac_buf+HALMAC_RX_DESC_SIZE_88XX;
	u8* pC2h_buf = pbuf;
	//u32 c2h_size = halmac_size -HALMAC_TX_DESC_SIZE_88XX;
	u32 c2h_size = c2hsize;
	u8 return_status = HALMAC_H2C_RETURN_SUCCESS;

    //printk("parse c2h pkt!!\n");

	/* Get C2H CMD ID */

	c2h_cmd = pC2h_buf[0];

	return_status =  (u8)pC2h_buf[4];

	if(0xFF!=c2h_cmd){
		
		//printk("C2H_PKT not for FwOffloadC2HFormat!!\n");
		return HALMAC_RET_SUCCESS;
	}
	if(return_status!=HALMAC_H2C_RETURN_SUCCESS){
		
		printk("C2H_PKT Status Error!! Status = %d\n",return_status);
	}	

	c2h_sub_cmd_id = pC2h_buf[2];
	if(0x1 ==c2h_sub_cmd_id	){ //this is ack
		
		printk("Ack for C2H!!\n");

        h2c_cmd_id = pC2h_buf[5];

		if(0xFF==h2c_cmd_id){
            printk("h2c cmd id = 0xff!!, need add handle for it.\n");

		}
        else{
            printk("the h2c cmd id = 0x%x\n",h2c_cmd_id);
        }
            
	}
	else if(0x00==c2h_sub_cmd_id){
			
			printk("c2h debug!!\n");
			return HALMAC_RET_SUCCESS;
	}

	return 0;
}


void C2HHandler88XX(
    IN HAL_PADAPTER     Adapter
)
{

#if 0 // 8881A & 8192E receive C2H by rx packets
	u4Byte      C2H_ID;
    u1Byte      C2HCONTENT[C2H_CONTENT_LEN];
	u4Byte      idx;
    VOID        (*c2hcallback)(IN HAL_PADAPTER Adapter,u1Byte *pbuf);

#ifdef CONFIG_WLAN_HAL_8192EE
	if (GET_CHIP_VER(Adapter) == VERSION_8192E) 
		return;
#endif
	C2H_ID = HAL_RTL_R8(REG_C2HEVT);

	HAL_memset(C2HCONTENT, 0, sizeof(C2HCONTENT));

    //For Endian Free.
    for(idx= 0; idx < C2H_CONTENT_LEN; idx++)
    {
        // content start at Reg[0x1a2]
        C2HCONTENT[idx] = HAL_RTL_R8(idx+(REG_C2HEVT+2));
    }


	if(C2H_ID >=  sizeof(HalC2Hcmds)/sizeof(struct cmdobj)) {
		panic_printk("Get Error C2H ID = %x \n",C2H_ID);
	} else {
	    c2hcallback = HalC2Hcmds[C2H_ID].c2hfuns;
	    if(c2hcallback)	{
	        c2hcallback(Adapter,C2HCONTENT);       
	    } else {
	        RT_TRACE_F(COMP_IO, DBG_WARNING ,("Get Error C2H ID = %x \n",C2H_ID)); 
	    }
	}
	HAL_RTL_W8(REG_C2HEVT + 0xf, 0);
#ifdef TXREPORT	
	if( C2H_ID == 4)
		requestTxReport88XX(Adapter);
#endif	

#endif
}

VOID
C2HPacket88XX
(
	IN  HAL_PADAPTER    Adapter,
	IN  pu1Byte			pBuf,
	IN	u2Byte			length
)
{
    u2Byte  cmd_len = 0;
    u1Byte	cmd_id = 0, cmd_seq = 0;
    u1Byte	*tmp_buf = NULL;
	
    cmd_id = *(pBuf);
    cmd_seq = *(pBuf + 1);

    //RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "C2H packet :\n", pBuf, length); 
#if 0 //remove by eric, for avoiding system hang
#if IS_SUPPORT_MACHAL_API
    if(IS_HARDWARE_MACHAL_SUPPORT(Adapter))
    {
        GET_MACHALAPI_INTERFACE(Adapter)->halmac_get_c2h_info(Adapter->pHalmac_adapter,pBuf,(u4Byte)length);
    }
#endif // IS_SUPPORT_MACHAL_API
#if CFG_HAL_WLAN_SUPPORT_H2C_PACKET
#if IS_SUPPORT_MACHAL_API
        else
#endif        
            ParseC2HPacket88XX(Adapter, pBuf, length);   //for new format c2h, including ack...etc
#endif    
#endif

	if(cmd_id == C2H_88XX_EXTEND_IND)
	{
		cmd_len = length - 2;
		tmp_buf = pBuf + 2;
		C2HExtEventHandler88XX(Adapter, cmd_id, cmd_len, tmp_buf);
	}
	else
	{
		cmd_len = length - 2;
		tmp_buf = pBuf + 2;
		C2HEventHandler88XX(Adapter, cmd_id, cmd_len, tmp_buf);
	}
}


VOID
C2HEventHandler88XX
(
	IN HAL_PADAPTER     Adapter,
	IN u1Byte			c2hCmdId, 
	IN u1Byte			c2hCmdLen,
	IN pu1Byte 			tmpBuf
)
{
	static u1Byte CmdBuffer = 0x1;
	u1Byte i;
	u1Byte	Extend_c2hSubID = 0;
		
	switch(c2hCmdId)
	{
	
	case C2H_88XX_LB:
		// get c2h loopback
		printk("c2h content c2hCmdId = 0x%x c2hCmdLen= 0x%x \n", c2hCmdId, c2hCmdLen);

		for(i=0;i<c2hCmdLen;i++)
		{
			printk("%x ",*(tmpBuf+i));    
		}

		printk("\n");

		CmdBuffer++;

		delay_ms(100);
		// issue h2c 0xE0 back
		printk("GEN h2c cmd CmdBuffer = %x \n",CmdBuffer);        
		FillH2CCmd88XX(Adapter,0xE0,1,&CmdBuffer);        
		//  FillH2CCmd88XX(Adapter,0xE0,1,&CmdBuffer);                
		break;

	case C2H_88XX_TX_RATE:
		#ifdef TXREPORT	
		APReqTXRptHandler(Adapter,tmpBuf);
#ifdef CONFIG_SDIO_HCI
		notify_request_tx_report(Adapter);
#else
#if (MU_BEAMFORMING_SUPPORT == 1)
		if(Adapter->pmib->dot11RFEntry.txbf_mu)
			requestTxReport88XX_MU(Adapter);
		else	
#endif
#ifdef TXRETRY_CNT
		requestTxRetry88XX(Adapter);
#else
		requestTxReport88XX(Adapter);
#endif			
		#endif			
		#endif			
		break;

#ifdef TXRETRY_CNT
	case C2H_88XX_TX_RETRY:
		C2HTxTxRetryHandler(Adapter, tmpBuf);
		requestTxReport88XX(Adapter);	
		break;
#endif

	case C2H_88XX_TXBF:
		#if (BEAMFORMING_SUPPORT == 1)
		C2HTxBeamformingHandler88XX(Adapter, tmpBuf, c2hCmdLen);
		#endif
		break;

	case C2H_88XX_MU_SCORE_RPT:
		#if (MU_BEAMFORMING_SUPPORT == 1)
		C2HTxBeamformingGroupDoneHandler88XX(Adapter, tmpBuf, c2hCmdLen);
		#endif
		break;
		
	case C2H_88XX_MACID_PAUSE_RPT:
        break;
		
    case C2H_88XX_APPS_INFO_RPT:
        #ifdef AP_SWPS_OFFLOAD
        C2HAPPSInfoHandler88XX(Adapter, tmpBuf, c2hCmdLen);
        #endif // #ifdef AP_SWPS_OFFLOAD
        break;
		
	default:
		if(!(phydm_c2H_content_parsing(&(Adapter->pshare->_dmODM), c2hCmdId, c2hCmdLen, tmpBuf))) {
			pr_debug("[C2H], Unkown packet!! CmdId(%#X)!\n", c2hCmdId);
		}
		break;
	}
}

VOID
C2HExtEventHandler88XX
(
    IN HAL_PADAPTER     Adapter,
    IN u1Byte			c2hCmdId, 
    IN u2Byte			c2hCmdLen,
    IN pu1Byte 			tmpBuf    			
)
{
    u1Byte sub_id = 0;

    sub_id = *(tmpBuf);
        
    switch(sub_id)
    {
        case EXTEND_C2H_88XX_DBG_PRINT:
			phydm_fw_trace_handler_8051(&(Adapter->pshare->_dmODM), tmpBuf, c2hCmdLen);      
        break;
		
        case EXTEND_C2H_88XX_ATM_RPT:
#if CFG_HAL_ATM_REPORT
            C2HATMReportHandler88XX(Adapter, tmpBuf, c2hCmdLen);
#endif              
        break;

		case EXTEND_C2H_88XX_CCX_RPT:
#if (BEAMFORMING_SUPPORT == 1)
			C2HCCXRptHandler88XX(Adapter, tmpBuf, c2hCmdLen);
#endif
		break;

		case EXTEND_C2H_88XX_DETECT_THERMAL:
			// Do nothing to avoid warning message for 8812F recommended by kkbomb
		break;
        default:
            printk("[C2H EXT][%s], Unkown packet!! CmdId(%#X), sub_id(%#X)!\n", Adapter->dev->name, c2hCmdId, sub_id);
        break;

    }
}


#if (BEAMFORMING_SUPPORT == 1)
VOID
C2HTxBeamformingHandler88XX(
	struct rtl8192cd_priv *priv,
	pu1Byte			CmdBuf,
	u1Byte			CmdLen
)
{
	IN  HAL_PADAPTER    Adapter = priv;

#ifdef CONFIG_WLAN_HAL_8814AE
	if ( IS_HARDWARE_TYPE_8814AE(Adapter))  {
		C2HTxBeamformingHandler_8814A(priv, CmdBuf, CmdLen);
	}
#endif
#ifdef CONFIG_WLAN_HAL_8822BE
	if ( IS_HARDWARE_TYPE_8822B(Adapter))  {
		u1Byte 	status = CmdBuf[0] & BIT0;
		C2HTxBeamformingHandler_8822B(priv, CmdBuf, CmdLen);
	}
#endif
#ifdef CONFIG_WLAN_HAL_8822CE
	if ( IS_HARDWARE_TYPE_8822C(Adapter))  {
		u1Byte 	status = CmdBuf[0] & BIT0;
		C2HTxBeamformingHandler_8822C(priv, CmdBuf, CmdLen);
	}
#endif
#ifdef CONFIG_WLAN_HAL_8812FE
	if ( IS_HARDWARE_TYPE_8812F(Adapter))  {
		u1Byte 	status = CmdBuf[0] & BIT0;
		C2HTxBeamformingHandler_8812F(priv, CmdBuf, CmdLen);
	}
#endif

#ifdef CONFIG_WLAN_HAL_8821CE
	if ( IS_HARDWARE_TYPE_8821C(Adapter))  {
		u1Byte 	status = CmdBuf[0] & BIT0;
		C2HTxBeamformingHandler_8821C(priv, CmdBuf, CmdLen);
	}
#endif
#ifdef CONFIG_WLAN_HAL_8197F
	if (IS_HARDWARE_TYPE_8197F(Adapter))  {
		C2HTxBeamformingHandler_8197F(priv, CmdBuf, CmdLen);
	}
#endif

#ifdef CONFIG_WLAN_HAL_8198F
	if (IS_HARDWARE_TYPE_8198F(Adapter))  {
		C2HTxBeamformingHandler_8198F(priv, CmdBuf, CmdLen);
	}
#endif

#ifdef CONFIG_WLAN_HAL_8814BE
	if ( IS_HARDWARE_TYPE_8814B(Adapter))  {
		C2HTxBeamformingHandler_8814B(priv, CmdBuf, CmdLen);
	}
#endif

}

#if 0//def CONFIG_WLAN_HAL_8197G
	if (IS_HARDWARE_TYPE_8197G(Adapter))  {
		//C2HTxBeamformingHandler_8197F(priv, CmdBuf, CmdLen);
	}
#endif

//eric-gid C2HCCXRptHandler88XX
unsigned int output_value_32(unsigned int value, unsigned char start, unsigned char end)
{
	unsigned int bit_mask = 0, output = 0;

	if(start == end) //1-bit value
	{
		if(value & BIT(start))
			output = 1;
		else
			output = 0;
	}
	else
	{
		unsigned char x = 0;
				
		for(x = 0; x<=(end-start); x ++)
			bit_mask |= BIT(x);

		output = value & (bit_mask << start);
		output = output >> start;
	}

	return output;
}


VOID
C2HCCXRptHandler88XX(
	struct rtl8192cd_priv *priv,
	pu1Byte			CmdBuf,
	u1Byte			CmdLen
)
{
	unsigned int tmp = 0;
	unsigned int *ptmp;
	unsigned int mac_id, sw_define, tx_state = 0xff;
	unsigned char Idx;
	
	if((CmdLen-2) != 0x10)
		panic_printk("C2H CCX RPT LEN Invalid\n");
	
	ptmp = CmdBuf + 2;
	tmp = *ptmp;
	tmp = le32_to_cpu(tmp);

	mac_id = output_value_32(tmp, 16, 22);


	ptmp += 1;
	tmp = *ptmp;
	tmp = le32_to_cpu(tmp);

	sw_define = output_value_32(tmp, 16, 23);
	tx_state = output_value_32(tmp, 30, 31);

	//panic_printk("mac_id=%d sw_define=%d tx_state=%d \n", mac_id, sw_define, tx_state);

	if(sw_define == 2) { // IS_GID_MGNT
		if(tx_state != 0) { // drop
			PRT_BEAMFORMING_ENTRY pEntry;						
			pEntry = Beamforming_GetEntryByMacId(priv, mac_id, &Idx);
			if(pEntry) {

				#if (MU_BEAMFORMING_SUPPORT == 1)	// fix compile error of 8814A+8194A	
				//panic_printk("[CCX] GID mgnt fail, gid_mgnt_retryCnt=%d\n", pEntry->gid_mgnt_retryCnt);
				#if (MU_BEAMFORMING_SUPPORT == 1)	
				pEntry->gid_mgnt_retryCnt++;
				if(pEntry->gid_mgnt_retryCnt < 5)
					issue_action_GROUP_ID(priv, Idx);			
				else
				#endif
				{
					panic_printk("GroupID mgnt retry > 5\n");
					Beamforming_Leave(priv, pEntry->MacAddr);
				}
				#endif
			}
				
		} else {			
			RT_TRACE(COMP_DBG, DBG_LOUD, ("[CCX] GID mgnt received\n"));
		}
	}

}

#if (MU_BEAMFORMING_SUPPORT == 1)
struct mu_group_st {
	u1Byte		user_num;
	u1Byte		user[4];
};

VOID
request_mu_group(
	struct rtl8192cd_priv *priv,
	u4Byte bitmap
)
{
	u1Byte H2CCommand[4] = {0};
	u1Byte raised;
	u4Byte i;

	for (i = 0; i < 28; i++) {
		raised = ((bitmap & BIT(i)) >> i) & 0x1;
		if (i < 7)
			H2CCommand[0] |= (raised << i);
		else if (i < 14)
			H2CCommand[1] |= (raised << (i - 7));
		else if (i < 21)
			H2CCommand[2] |= (raised << (i - 14));
		else
			H2CCommand[3] |= (raised << (i - 21));
	}
	
	GET_HAL_INTERFACE(priv)->fill_h2c_cmd_handler(priv, H2C_88XX_MU_SCORE_RPT, 4, H2CCommand);
}

VOID
beamform_mu_match_8814b(
	struct rtl8192cd_priv *priv,
	u1Byte group_index,
	struct mu_group_st *group
)
{
	PRT_BEAMFORMING_ENTRY	pEntry = NULL, pEntry2 = NULL;
	PRT_BEAMFORMING_INFO	pBeamInfo = &(priv->pshare->BeamformingInfo);
	struct stat_info	*pstat, *pstat2;
	PRT_BEAMFORMING_ENTRY	CandidateEntry[MAX_NUM_BEAMFORMEE_MU] = {NULL};
	u4Byte i, j;
	unsigned int dqnum = priv->pshare->rf_ft_var.dqnum;
	u1Byte group_ra_map[28][4] = {
		{0, 1}, {2, 3}, {4, 5}, {6, 7, 8}, {9, 10, 11}, {12, 13, 14}, {15, 16, 17, 18}, 
		{19, 20}, {21, 22}, {23, 24}, {25, 26, 27}, {28, 29, 30}, {31, 32, 33}, {34, 35, 36, 37},
		{38, 39}, {40, 41}, {42, 43}, {44, 45, 46}, {47, 48, 49}, {50, 51, 52}, {53, 54, 55, 56},
		{57, 58}, {59, 60}, {61, 62}, {63, 64, 65}, {66, 67, 68}, {69, 70, 71}, {72, 73, 74, 75}
	};
	
	for(i = 0; i < BEAMFORMEE_ENTRY_NUM; i++) {
		pEntry = &(pBeamInfo->BeamformeeEntry[i]);
		if( pEntry->bUsed && (pEntry->is_mu_sta == TXBF_TYPE_MU) && pEntry->mu_reg_index < MAX_NUM_BEAMFORMEE_MU) {
			pstat = pEntry->pSTA;
			if(pstat) {
				if(pstat->isSendNDPA && pstat->isRssiApplyMU)
					CandidateEntry[pEntry->mu_reg_index] = pEntry;
			}
		}
	}

	/* build partner relation */
	for (i = 0; i < group->user_num; i++) {
		pEntry = CandidateEntry[group->user[i]];
		pstat = pEntry->pSTA;

		for (j = 0; j < group->user_num; j++) {
			if (i != j) {
				pEntry2 = CandidateEntry[group->user[j]];
				pstat2 = pEntry2->pSTA;

				pstat->muPartner[pstat->muPartner_num++] = pstat2;
				pstat2->muPartner[pstat2->muPartner_num++] = pstat;
			}
		}
	}

	/* other porperty setting */
	for (i = 0; i < group->user_num; i++) {
		pEntry = CandidateEntry[group->user[i]];
		pstat = pEntry->pSTA;

		pstat->muFlagForAMSDU = 1;
		pstat->mu_deq_num = dqnum;
		pstat->mu_rate = ODMPTR->dm_ra_table.mu1_rate[group_ra_map[group_index][i]];
		pstat->mu_tx_rate = query_mu_vht_rate(pstat);
	}
	
}

VOID
beamform_mu_group_8814b(
	struct rtl8192cd_priv *priv, 
	u4Byte *bitmap,
	u1Byte *user_en
)
{
#define ARR_LEN(arr) (sizeof(arr)/sizeof(arr[0]))

	PRT_BEAMFORMING_ENTRY	pEntry = NULL;
	PRT_BEAMFORMING_INFO	pBeamInfo = &(priv->pshare->BeamformingInfo);
	struct stat_info	*pstat;
	PRT_BEAMFORMING_ENTRY	CandidateEntry[MAX_NUM_BEAMFORMEE_MU] = {NULL};
	u4Byte new_bitmap = 0;
	u4Byte i, j;
	u1Byte match_4sta = 0, match_3sta = 0, match_2sta = 0;
	struct mu_group_st *group;
	struct mu_group_st	mu_group[28] = {
		{2,{0,1}}, {2,{0,2}}, {2,{0,3}}, {3,{0,1,2}}, {3,{0,1,3}}, {3,{0,2,3}}, {4,{0,1,2,3}},
		{2,{1,0}}, {2,{1,2}}, {2,{1,3}}, {3,{1,0,2}}, {3,{1,0,3}}, {3,{1,2,3}}, {4,{1,0,2,3}},
		{2,{2,0}}, {2,{2,1}}, {2,{2,3}}, {3,{2,0,1}}, {3,{2,0,3}}, {3,{2,1,3}}, {4,{2,0,1,3}},
		{2,{3,0}}, {2,{3,1}}, {2,{3,2}}, {3,{3,0,1}}, {3,{3,0,2}}, {3,{3,1,2}}, {4,{3,0,1,2}}
	};
	u1Byte mu_group_sta_map[28] = {
		0x03, 0x05, 0x09, 0x07, 0x0b, 0x0d, 0x0f,
		0x03, 0x06, 0x0a, 0x07, 0x0b, 0x0e, 0x0f,
		0x05, 0x06, 0x0c, 0x07, 0x0d, 0x0e, 0x0f,
		0x09, 0x0a, 0x0c, 0x0b, 0x0d, 0x0e, 0x0f
	};
	u1Byte group_nsta_sta_map[11] = {
		0x0f,
		0x07, 0x0b, 0x0e, 0x0d,
		0x03, 0x05, 0x09, 0x06, 0x0a, 0x0c
	};
	u4Byte group_nsta_en = 0;
	u1Byte selected_sta_map = 0;
	u1Byte selected_group = 0;
	u1Byte skip;

	for(i = 0; i < BEAMFORMEE_ENTRY_NUM; i++) {
		pEntry = &(pBeamInfo->BeamformeeEntry[i]);
		if( pEntry->bUsed && (pEntry->is_mu_sta == TXBF_TYPE_MU) && pEntry->mu_reg_index < MAX_NUM_BEAMFORMEE_MU) {
			pstat = pEntry->pSTA;
			if(pstat) {
				pstat->muPartner_num = 0;
				for (j = 0; j < MAX_NUM_MU_PARTNER; j++)
					pstat->muPartner[j] = NULL;
				if(pstat->isSendNDPA && pstat->isRssiApplyMU)
					CandidateEntry[pEntry->mu_reg_index] = pEntry;
			}
		}
	}

	for (i = 0; i < ARR_LEN(mu_group); i++) {
		if (!(*bitmap & BIT(i)))
			continue;

		group = &mu_group[i];

		skip = 0;
		for (j = 0; j < group->user_num; j++) {
			if (CandidateEntry[group->user[j]] == NULL) {
				skip = 1;
				break;
			}
		}
		if (skip)
			continue;

		for (j = 0; j < ARR_LEN(group_nsta_sta_map); j++) {
			if (mu_group_sta_map[i] == group_nsta_sta_map[j]) {
				group_nsta_en |= BIT(j);
				break;
			}
		}

		if (group->user_num == 4) {
			if (!match_4sta) {
				match_4sta = i;
			}
		}
		else if (group->user_num == 3) {
			if (!match_3sta) {
				match_3sta = i;
			}
		}
		else {
			if (!match_2sta) {
				match_2sta = i;
			}
		}

	}

	if (match_4sta) {
		selected_sta_map = mu_group_sta_map[match_4sta];
		selected_group = match_4sta;
	}
	else if (match_3sta) {
		selected_sta_map = mu_group_sta_map[match_3sta];
		selected_group = match_3sta;
	}
	else if (match_2sta) {
		selected_sta_map = mu_group_sta_map[match_2sta];
		selected_group = match_2sta;
	}
	else {
		*bitmap = 0;
		*user_en = 0;
		return;
	}

	beamform_mu_match_8814b(priv, selected_group, &mu_group[selected_group]);

	for (i = 0; i < ARR_LEN(mu_group_sta_map); i++) {
		if (selected_sta_map == mu_group_sta_map[i])
			new_bitmap |= BIT(i);
	}

	*bitmap = new_bitmap;
	*user_en = selected_sta_map;
}

VOID
C2HTxBeamformingGroupDoneHandler88XX(
	struct rtl8192cd_priv *priv,
	pu1Byte			CmdBuf,
	u1Byte			CmdLen
)
{
	u4Byte		bitmap = 0;
	u4Byte		i, j;
	u1Byte		score, user_en = 0;
	struct halmac_cfg_mumimo_para mu_para, *para = &mu_para;
	struct mu_group_st	mu_group[28] = {
		{2,{0,1}}, {2,{0,2}}, {2,{0,3}}, {3,{0,1,2}}, {3,{0,1,3}}, {3,{0,2,3}}, {4,{0,1,2,3}},
		{2,{1,0}}, {2,{1,2}}, {2,{1,3}}, {3,{1,0,2}}, {3,{1,0,3}}, {3,{1,2,3}}, {4,{1,0,2,3}},
		{2,{2,0}}, {2,{2,1}}, {2,{2,3}}, {3,{2,0,1}}, {3,{2,0,3}}, {3,{2,1,3}}, {4,{2,0,1,3}},
		{2,{3,0}}, {2,{3,1}}, {2,{3,2}}, {3,{3,0,1}}, {3,{3,0,2}}, {3,{3,1,2}}, {4,{3,0,1,2}}
	};
	
	PHYDM_DBG(ODMPTR, DBG_TXBF, "%s =>\n", __FUNCTION__);

	if (CmdLen < 8)
		return;

	/*filter BB score < 2*/
	for (i = 0; i < 28; i++) {
		if (i < 7)
			score = ((le16_to_cpu(*(u2Byte *)CmdBuf)) >> (2 * i)) & 0x3;
		else if (i < 14)
			score = ((le16_to_cpu(*(u2Byte *)(CmdBuf + 2))) >> (2 * (i - 7))) & 0x3;
		else if (i < 21)
			score = ((le16_to_cpu(*(u2Byte *)(CmdBuf + 4))) >> (2 * (i - 14))) & 0x3;
		else
			score = ((le16_to_cpu(*(u2Byte *)(CmdBuf + 6))) >> (2 * (i - 21))) & 0x3;
		
		if (score >= 2)
			bitmap |= BIT(i);
	}

#if 1
	for (i = 0; i < 28; i++) {
		if (bitmap & BIT(i)) {
			printk("+MU Users: ");
			for (j = 0; j < mu_group[i].user_num; j++)
				printk("%d, ", mu_group[i].user[j]);
			printk("\n");
		}
	}
#endif

	beamform_mu_group_8814b(priv, &bitmap, &user_en);

#if 1
	for (i = 0; i < 28; i++) {
		if (bitmap & BIT(i)) {
			printk("-MU Users: ");
			for (j = 0; j < mu_group[i].user_num; j++)
				printk("%d, ", mu_group[i].user[j]);
			printk("\n");
		}
	}
#endif
	
	/* send h2c to notify wifi fw */
	request_mu_group(priv, bitmap);

	/* update mac registers */
	para->role = HAL_BFER;
	para->mu_tx_en = 1;

	for (i = 0; i < MAX_NUM_BEAMFORMEE_MU; i++) {
		if (user_en & BIT(i))
			para->sounding_sts[i] = 1;
	}
	
	GET_MACHALAPI_INTERFACE(priv)->halmac_cfg_mumimo(priv->pHalmac_adapter, para);

}
#endif
#endif

#if CFG_HAL_ATM_REPORT    
VOID
C2HATMReportHandler88XX(
    struct rtl8192cd_priv *priv,
	pu1Byte			CmdBuf,
	u2Byte			CmdLen
)
{
    u1Byte  i;
    u1Byte  macid;
    u4Byte  acctime;
    for(i=0;i<CmdLen;i=i+4){
        macid = CmdBuf[i];
        acctime = CmdBuf[i+1] | (CmdBuf[i+2] << 8) | (CmdBuf[i+3] << 16);
        //RT_TRACE_F(COMP_DBG,DBG_TRACE,("Get ATM Report macid=0x%x, acctime=0x%x\n",macid,acctime));
        //printk("Get ATM Report macid=0x%x, acctime=0x%x \n",macid,acctime);
    }
        
}
#endif

#ifdef AP_SWPS_OFFLOAD
VOID
C2HAPPSInfoHandler88XX(
	struct rtl8192cd_priv *priv,
	pu1Byte			CmdBuf,
	u1Byte			CmdLen
)
{
    
    struct stat_info * pstat ;
        
#ifdef HW_FILL_MACID    
    pstat = get_stainfo_fast(priv,NULL,CmdBuf[1]);
#else
    pstat = NULL;
#endif        
    if(!pstat)        
        return;
    
    if(CmdBuf[0] & BIT0){
        if(pstat)
            pstat->txpdrop_flag = 1;
        
    }
    if(CmdBuf[0] & BIT1){
        if(pstat){
            pstat->txpause_flag = 0;
            if(priv->pshare->paused_sta_num)
				priv->pshare->paused_sta_num--;		
        }
    }
    GDEBUG("TEST: get C2H APPS Info Report, MACID %x, dropflag=%x, pauseflag=%x\n",CmdBuf[1],pstat->txpdrop_flag ,pstat->txpause_flag);
}
#endif //#ifdef AP_SWPS_OFFLOAD

#if IS_EXIST_EMBEDDED || IS_EXIST_PCI
BOOLEAN
DownloadRsvdPage88XX
( 
	IN  HAL_PADAPTER    Adapter,
    IN  pu4Byte         beaconbuf,    
    IN  u4Byte          beaconPktLen,
    IN  u1Byte          bReDownload
)
{
    u1Byte      wait_cnt = 0;
    u4Byte      polling_reg;

    SetBeaconDownload88XX(Adapter, HW_VAR_BEACON_ENABLE_DOWNLOAD);
  

#if IS_RTL88XX_MAC_V4
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
        polling_reg = REG_P0MGQ_RXQ_TXRXBD_NUM+3;
    }
#endif
#if IS_RTL88XX_MAC_V1_V2_V3
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3){
        polling_reg = REG_RX_RXBD_NUM+1;
    }
#endif 

    HAL_RTL_W8(polling_reg, HAL_RTL_R8(polling_reg) | BIT(4));

    while((HAL_RTL_R8(polling_reg) & BIT(4))== 0x10)
    {
        // Note : 8198F FW enable CCI400, dma IMEM spend 1090us
        if (++wait_cnt > 200) { 
            RT_TRACE_F(COMP_INIT, DBG_SERIOUS, ("Download Img fail\n"));
            return _FALSE;
        }
       delay_us(10);
    }

    if(bReDownload) {
        // download small beacon
        SigninBeaconTXBD88XX(Adapter, beaconbuf, beaconPktLen);
        
        HAL_RTL_W8(polling_reg, HAL_RTL_R8(polling_reg) | BIT(4));
        while((HAL_RTL_R8(polling_reg) & BIT(4))== 0x10)
        {
            if (++wait_cnt > 100) { 
                RT_TRACE_F(COMP_INIT, DBG_SERIOUS, ("Download small beacon fail\n"));
                return _FALSE;
            }
           delay_us(10);
        }
    }

    return _TRUE;
}
#endif // IS_EXIST_EMBEDDED || IS_EXIST_PCI
#endif //if IS_RTL88XX_GENERATION

