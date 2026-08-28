/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	Hal8812FEGen.c
	
Abstract:
	Defined RTL8812FE HAL Function
	    
Major Change History:
	When       Who               What
	---------- ---------------   -------------------------------
	2015-06-25 Eric             Create.	
--*/

#ifndef __ECOS
#include "HalPrecomp.h"
#else
#include "../../../HalPrecomp.h"

#include "../../../../phydm/phydm_precomp.h" //eric-8822

#endif

enum rt_status
InitPON8812FE(
    IN  HAL_PADAPTER Adapter,
    IN  u4Byte     	ClkSel        
)
{
    u32     bytetmp;
    u32     retry;
    u1Byte	u1btmp;
	
    RT_TRACE_F( COMP_INIT, DBG_LOUD, ("\n"));

    // TODO: Filen, first write IO will fail, don't know the root cause
    printk("0: Reg0x0: 0x%x, Reg0x4: 0x%x, Reg0x1C: 0x%x\n", HAL_RTL_R32(0x0), HAL_RTL_R32(0x4), HAL_RTL_R32(0x1C));
	HAL_RTL_W8(REG_RSV_CTRL, 0x00);
    printk("1: Reg0x0: 0x%x, Reg0x4: 0x%x, Reg0x1C: 0x%x\n", HAL_RTL_R32(0x0), HAL_RTL_R32(0x4), HAL_RTL_R32(0x1C));
	HAL_RTL_W8(REG_RSV_CTRL, 0x00);
    printk("2: Reg0x0: 0x%x, Reg0x4: 0x%x, Reg0x1C: 0x%x\n", HAL_RTL_R32(0x0), HAL_RTL_R32(0x4), HAL_RTL_R32(0x1C));

    // TODO: Filen, check 8812F setting
	if(ClkSel == XTAL_CLK_SEL_25M) {
	} else if (ClkSel == XTAL_CLK_SEL_40M){
	}	

	// YX sugguested 2014.06.03
	u1btmp = platform_efio_read_1byte(Adapter, 0x10C2);
	platform_efio_write_1byte(Adapter, 0x10C2, (u1btmp | BIT1));
	
	if (!HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK,
			PWR_INTF_PCI_MSK, rtl8812F_card_enable_flow))
    {
        RT_TRACE( COMP_INIT, DBG_SERIOUS, ("%s %d, HalPwrSeqCmdParsing init fail!!!\n", __FUNCTION__, __LINE__));
        return RT_STATUS_FAILURE;
    }

    printk("3: Reg0x0: 0x%x, Reg0x4: 0x%x, Reg0x1C: 0x%x\n", HAL_RTL_R32(0x0), HAL_RTL_R32(0x4), HAL_RTL_R32(0x1C));

#ifdef RTL_8812F_MP_TEMP

    HAL_RTL_W32(REG_BD_RWPTR_CLR,0xffffffff);

    HAL_RTL_W32(0x1000, HAL_RTL_R32(0x1000)|BIT16|BIT17);
    printk("%s(%d): 0x1000:0x%x \n", __FUNCTION__, __LINE__, HAL_RTL_R32(0x1000));
#endif

    return  RT_STATUS_SUCCESS;
}


enum rt_status
StopHW8812FE(
    IN  HAL_PADAPTER Adapter
)
{
    // TODO:

    return RT_STATUS_SUCCESS;
}


enum rt_status
ResetHWForSurprise8812FE(
    IN  HAL_PADAPTER Adapter
)
{
    // TODO: Filen, necessary to be added code here

    return RT_STATUS_SUCCESS;
}


enum rt_status	
hal_Associate_8812FE(
	 HAL_PADAPTER        Adapter,
    BOOLEAN             IsDefaultAdapter
)
{
    PHAL_INTERFACE              pHalFunc = GET_HAL_INTERFACE(Adapter);
    PHAL_DATA_TYPE              pHalData = _GET_HAL_DATA(Adapter);


    //
    //Initialization Related
    //
    pHalData->AccessSwapCtrl        = HAL_ACCESS_SWAP_MEM;
    pHalFunc->InitPONHandler        = InitPON88XX;
    pHalFunc->InitMACHandler        = InitMAC88XX;
    pHalFunc->InitFirmwareHandler   = InitMIPSFirmware88XX;
    pHalFunc->InitHCIDMAMemHandler  = InitHCIDMAMem88XX;
    pHalFunc->InitHCIDMARegHandler  = InitHCIDMAReg88XX;    
#if CFG_HAL_SUPPORT_MBSSID    
    pHalFunc->InitMBSSIDHandler     = InitMBSSID88XX;
	pHalFunc->InitMBIDCAMHandler	= InitMBIDCAM88XX;
#endif  //CFG_HAL_SUPPORT_MBSSID
    pHalFunc->InitVAPIMRHandler     = InitVAPIMR88XX;
    pHalFunc->InitLLT_TableHandler  = InitLLT_Table88XX_V1;
#if CFG_HAL_HW_FILL_MACID
    pHalFunc->InitMACIDSearchHandler    = InitMACIDSearch88XX;            
    pHalFunc->CheckHWMACIDResultHandler = CheckHWMACIDResult88XX;            
#endif //CFG_HAL_HW_FILL_MACID
#ifdef MULTI_MAC_CLONE
	pHalFunc->McloneSetMBSSIDHandler	= McloneSetMBSSID88XX;
	pHalFunc->McloneStopMBSSIDHandler	= McloneStopMBSSID88XX;
#endif //CFG_HAL_HW_FILL_MACID
    pHalFunc->SetMBIDCAMHandler     = SetMBIDCAM88XX;
    pHalFunc->InitVAPIMRHandler     = InitVAPIMR88XX;

    //
    //Stop Related
    //
#if CFG_HAL_SUPPORT_MBSSID        
    pHalFunc->StopMBSSIDHandler     = StopMBSSID88XX;
#endif  //CFG_HAL_SUPPORT_MBSSID
    pHalFunc->StopHWHandler         = StopHW88XX;
    pHalFunc->StopSWHandler         = StopSW88XX;
    pHalFunc->DisableVXDAPHandler   = DisableVXDAP88XX;
    pHalFunc->ResetHWForSurpriseHandler     = ResetHWForSurprise8812FE;

    //
    //ISR Related
    //
    pHalFunc->InitIMRHandler                    = InitIMR88XX;
    pHalFunc->EnableIMRHandler                  = EnableIMR88XX;
    pHalFunc->InterruptRecognizedHandler        = InterruptRecognized88XX;
    pHalFunc->GetInterruptHandler               = GetInterrupt88XX;
    pHalFunc->AddInterruptMaskHandler           = AddInterruptMask88XX;
    pHalFunc->RemoveInterruptMaskHandler        = RemoveInterruptMask88XX;
    pHalFunc->DisableRxRelatedInterruptHandler  = DisableRxRelatedInterrupt88XX;
    pHalFunc->EnableRxRelatedInterruptHandler   = EnableRxRelatedInterrupt88XX;

    //
    //Tx Related
    //
    pHalFunc->PrepareTXBDHandler            = PrepareTXBD88XX;    
    pHalFunc->FillTxHwCtrlHandler           = FillTxHwCtrl88XX;
    pHalFunc->SyncSWTXBDHostIdxToHWHandler  = SyncSWTXBDHostIdxToHW88XX;
    pHalFunc->TxPollingHandler              = TxPolling88XX;
    pHalFunc->SigninBeaconTXBDHandler       = SigninBeaconTXBD88XX;
    pHalFunc->SetBeaconDownloadHandler      = SetBeaconDownload88XX;
    pHalFunc->FillBeaconDescHandler         = FillBeaconDesc88XX_V1;
    pHalFunc->GetTxQueueHWIdxHandler        = GetTxQueueHWIdx88XX;
    pHalFunc->MappingTxQueueHandler         = MappingTxQueue88XX;
    pHalFunc->QueryTxConditionMatchHandler  = QueryTxConditionMatch88XX;
    pHalFunc->FillTxDescHandler             = FillTxDesc88XX_V1;
#if CFG_HAL_TX_SHORTCUT /*eric-8822*/
    pHalFunc->FillShortCutTxDescHandler     = FillShortCutTxDesc88XX_V1;    
//    pHalFunc->GetShortCutTxDescHandler      = GetShortCutTxDesc88XX;
//    pHalFunc->ReleaseShortCutTxDescHandler  = ReleaseShortCutTxDesc88XX;
    pHalFunc->GetShortCutTxBuffSizeHandler  = GetShortCutTxBuffSize88XX_V1;
    pHalFunc->SetShortCutTxBuffSizeHandler  = SetShortCutTxBuffSize88XX_V1;
    pHalFunc->CopyShortCutTxDescHandler     = CopyShortCutTxDesc88XX;
    pHalFunc->FillShortCutTxHwCtrlHandler   = FillShortCutTxHwCtrl88XX;    
#if CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC            
    pHalFunc->FillHwShortCutTxDescHandler   = FillHwShortCutTxDesc88XX_V1;    
#endif
#endif // CFG_HAL_TX_SHORTCUT
    pHalFunc->ReleaseOnePacketHandler       = ReleaseOnePacket88XX;                  

    //
    //Rx Related
    //
    pHalFunc->PrepareRXBDHandler            = PrepareRXBD88XX;
    pHalFunc->QueryRxDescHandler            = QueryRxDesc88XX_V1;
    pHalFunc->UpdateRXBDInfoHandler         = UpdateRXBDInfo88XX;
    pHalFunc->UpdateRXBDHWIdxHandler        = UpdateRXBDHWIdx88XX;
    pHalFunc->UpdateRXBDHostIdxHandler      = UpdateRXBDHostIdx88XX;    

    //
    // General operation
    //
    pHalFunc->GetChipIDMIMOHandler          =   GetChipIDMIMO88XX;
    pHalFunc->SetHwRegHandler               =   SetHwReg88XX;
    pHalFunc->GetHwRegHandler               =   GetHwReg88XX;
    pHalFunc->SetMACIDSleepHandler          =   SetMACIDSleep88XX;
	pHalFunc->CheckHangHandler              =   CheckHang88XX;
    pHalFunc->GetMACIDQueueInTXPKTBUFHandler=   GetMACIDQueueInTXPKTBUF88XX;

    //
    // Timer Related
    //
    pHalFunc->Timer1SecHandler              =   Timer1Sec88XX;


    //
    // Security Related     
    //
    pHalFunc->CAMReadMACConfigHandler       =   CAMReadMACConfig88XX;
    pHalFunc->CAMEmptyEntryHandler          =   CAMEmptyEntry88XX;
    pHalFunc->CAMFindUsableHandler          =   CAMFindUsable88XX;
    pHalFunc->CAMProgramEntryHandler        =   CAMProgramEntry88XX;


    //
    // PHY/RF Related
    //

	//eric-8822
    pHalFunc->PHYSetCCKTxPowerHandler       = PHYSetCCKTxPower88XX;
    pHalFunc->PHYSetOFDMTxPowerHandler      = PHYSetOFDMTxPower88XX;
    pHalFunc->PHYSwBWModeHandler            = SwBWMode88XX_AC;
    pHalFunc->PHYUpdateBBRFValHandler       = UpdateBBRFVal88XX_AC;
    // TODO: 8812F Power Tracking should be done
    pHalFunc->TXPowerTrackingHandler        = TXPowerTracking_ThermalMeter_Tmp8812F;
    pHalFunc->PHYSSetRFRegHandler           = PHY_SetRFReg_88XX_AC; //config_phydm_write_rf_reg_8812F; 
    pHalFunc->PHYQueryRFRegHandler          = PHY_QueryRFReg_8812F;  //config_phydm_read_rf_reg_8812F;
    pHalFunc->IsBBRegRangeHandler           = IsBBRegRange88XX_V1;
    pHalFunc->PHYSetSecCCATHbyRXANT         = PHY_Set_SecCCATH_by_RXANT_8812F;
    pHalFunc->PHYSpurCalibration            = phy_SpurCalibration_8812F;


    //
    // Firmware CMD IO related
    //
    pHalData->H2CBufPtr88XX     = 0;
    pHalData->bFWReady          = _FALSE;
    // TODO: code below should be sync with new 3081 FW
    pHalFunc->fill_h2c_cmd_handler          = FillH2CCmd88XX;
    pHalFunc->UpdateHalRAMaskHandler        = UpdateHalRAMask8814A;
    pHalFunc->UpdateHalMSRRPTHandler        = UpdateHalMSRRPT88XX;
    pHalFunc->SetAPOffloadHandler           = SetAPOffload88XX;;
#ifdef AP_PS_Offlaod
    pHalFunc->SetAPPSOffloadHandler         = HalGeneralDummy;
#endif
#ifdef HW_DETECT_PS_OFFLOAD
    pHalFunc->MACIDPauseHandler             = MacidPauseCtrl88XX;
#endif
#ifdef AP_SWPS_OFFLOAD
    pHalFunc->ClearDropIDHandler            = HalGeneralDummy;
    pHalFunc->UpdateMacidSWPSInfoHandler    = HalGeneralDummy;
#endif
    pHalFunc->SetRsvdPageHandler	        = SetRsvdPage88XX;
    pHalFunc->GetRsvdPageLocHandler	        = GetRsvdPageLoc88XX;
    pHalFunc->DownloadRsvdPageHandler	    = HalGeneralDummy;
    pHalFunc->C2HHandler                    = HalGeneralDummy;
    pHalFunc->C2HPacketHandler              = C2HPacket88XX;    
    pHalFunc->GetTxRPTHandler               = GetTxRPTBuf88XX;
    pHalFunc->SetTxRPTHandler               = SetTxRPTBuf88XX;    
#if CFG_HAL_ATM_REPORT        
    pHalFunc->SetATMReportCfgHandler        = HalGeneralDummy;
#endif //#if CFG_HAL_ATM_REPORT       
#if CFG_HAL_HW_FILL_MACID
    pHalFunc->SetCRC5ToRPTBufferHandler     = SetCRC5ToRPTBuffer88XX;        
#endif //#if CFG_HAL_HW_FILL_MACID

#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
    pHalFunc->ClearHWTXShortcutBufHandler   = ClearHWTXShortcutBufHandler88XX;     
#endif //CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
#if CFG_HAL_HW_SEQ
    pHalFunc->GetHwSequenceHandler          = GetHwSequenceHandler88XX;     
#endif //CFG_HAL_HW_SEQ
    pHalFunc->DumpRxBDescTestHandler        = DumpRxBDesc88XX;
    pHalFunc->DumpTxBDescTestHandler        = DumpTxBDesc88XX;
    return  RT_STATUS_SUCCESS;    
}


void 
InitMAC8812FE(
    IN  HAL_PADAPTER Adapter
)
{


    
}

#if (BEAMFORMING_SUPPORT == 1)
#define		bMaskDWord					0xffffffff

u1Byte
halTxbf8812F_GetNtx(
	IN PVOID			p_dm_void
	)
{
	struct dm_struct*	p_dm_odm = (struct dm_struct*)p_dm_void;
	u1Byte			Ntx = 0;

#if DEV_BUS_TYPE == RT_USB_INTERFACE
	if (p_dm_odm->support_interface == ODM_ITRF_USB) {
		if (*p_dm_odm->hub_usb_mode == 2) {/*USB3.0*/
			if (p_dm_odm->rf_type == RF_4T4R)
				Ntx = 3;
			else if (p_dm_odm->rf_type == RF_3T3R)
				Ntx = 2;
			else
				Ntx = 1;
		} else if (*p_dm_odm->hub_usb_mode == 1)	/*USB 2.0 always 2Tx*/
			Ntx = 1;
		else
			Ntx = 1;
	} else
#endif
	{
		if (p_dm_odm->rf_type == RF_4T4R)
			Ntx = 3;
		else if (p_dm_odm->rf_type == RF_3T3R)
			Ntx = 2;
		else
			Ntx = 1;
	}

	return Ntx;

}

u1Byte
halTxbf8812F_GetNrx(
	IN PVOID			p_dm_void
	)
{
	struct dm_struct*	p_dm_odm = (struct dm_struct*)p_dm_void;
	u1Byte			Nrx = 0;

	if (p_dm_odm->rf_type == RF_4T4R)
		Nrx = 3;
	else if (p_dm_odm->rf_type == RF_3T3R)
		Nrx = 2;
	else if (p_dm_odm->rf_type == RF_2T2R)
		Nrx = 1;
	else if (p_dm_odm->rf_type == RF_2T3R)
		Nrx = 2;
	else if (p_dm_odm->rf_type == RF_2T4R)
		Nrx = 3;
	else if (p_dm_odm->rf_type == RF_1T1R)
		Nrx = 0;
	else if (p_dm_odm->rf_type == RF_1T2R)
		Nrx = 1;
	else
		Nrx = 0;

	return Nrx;
	
}

/***************SU & MU BFee Entry********************/
VOID
halTxbf8812F_RfMode(
	IN PVOID			p_dm_void,
	IN	struct _RT_BEAMFORMING_INFO*	pBeamformingInfo,
	IN	u1Byte					idx
	)
{
	struct dm_struct*	p_dm_odm = (struct dm_struct*)p_dm_void;
	u1Byte				i;
	PRT_BEAMFORMING_ENTRY	BeamformeeEntry;

	if (idx < BEAMFORMEE_ENTRY_NUM)
		BeamformeeEntry = &pBeamformingInfo->BeamformeeEntry[idx];
	else
		return;

	if (p_dm_odm->rf_type == RF_1T1R)
		return;

#if (MU_BEAMFORMING_SUPPORT == 1)		
	phydm_txbf_rfmode(p_dm_odm, pBeamformingInfo->beamformee_su_cnt, pBeamformingInfo->beamformee_mu_cnt);
#else
	phydm_txbf_rfmode(p_dm_odm, pBeamformingInfo->beamformee_su_cnt, 0);
#endif		
}
#if 0
VOID
halTxbf8812F_DownloadNDPA(
	IN	PADAPTER			Adapter,
	IN	u1Byte				Idx
	)
{
	u1Byte			u1bTmp = 0, tmpReg422 = 0;
	u1Byte			BcnValidReg = 0, count = 0, DLBcnCount = 0;
	u2Byte			Head_Page = 0x7FE;
	BOOLEAN			bSendBeacon = FALSE;
	HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(Adapter);
	u2Byte			TxPageBndy = LAST_ENTRY_OF_TX_PKT_BUFFER_8814A; /*default reseved 1 page for the IC type which is undefined.*/
	struct _RT_BEAMFORMING_INFO*	pBeamInfo = GET_BEAMFORM_INFO(Adapter);
	struct _RT_BEAMFORMEE_ENTRY*	pBeamEntry = pBeamInfo->BeamformeeEntry+Idx;

	pHalData->bFwDwRsvdPageInProgress = TRUE;
	Adapter->HalFunc.GetHalDefVarHandler(Adapter, HAL_DEF_TX_PAGE_BOUNDARY, (pu2Byte)&TxPageBndy);
	
	/*Set REG_CR bit 8. DMA beacon by SW.*/
	u1bTmp = platform_efio_read_1byte(Adapter, REG_CR_8814A+1);
	platform_efio_write_1byte(Adapter,  REG_CR_8814A+1, (u1bTmp|BIT0));


	/*Set FWHW_TXQ_CTRL 0x422[6]=0 to tell Hw the packet is not a real beacon frame.*/
	tmpReg422 = platform_efio_read_1byte(Adapter, REG_FWHW_TXQ_CTRL_8814A+2);
	platform_efio_write_1byte(Adapter, REG_FWHW_TXQ_CTRL_8814A+2,  tmpReg422&(~BIT6));

	if (tmpReg422 & BIT6) {
		RT_TRACE(COMP_INIT, DBG_LOUD, ("SetBeamformDownloadNDPA_8814A(): There is an Adapter is sending beacon.\n"));
		bSendBeacon = TRUE;
	}

	/*0x204[11:0]	Beacon Head for TXDMA*/
	platform_efio_write_2byte(Adapter, REG_FIFOPAGE_CTRL_2_8814A, Head_Page);
	
	do {		
		/*Clear beacon valid check bit.*/
		BcnValidReg = platform_efio_read_1byte(Adapter, REG_FIFOPAGE_CTRL_2_8814A+1);
		platform_efio_write_1byte(Adapter, REG_FIFOPAGE_CTRL_2_8814A+1, (BcnValidReg|BIT7));
		
		/*download NDPA rsvd page.*/
		if (pBeamEntry->BeamformEntryCap & BEAMFORMER_CAP_VHT_SU)
			Beamforming_SendVHTNDPAPacket(p_dm_odm, pBeamEntry->MacAddr, pBeamEntry->AID, pBeamEntry->SoundBW, BEACON_QUEUE);
		else 
			Beamforming_SendHTNDPAPacket(p_dm_odm, pBeamEntry->MacAddr, pBeamEntry->SoundBW, BEACON_QUEUE);
	
		/*check rsvd page download OK.*/
		BcnValidReg = platform_efio_read_1byte(Adapter, REG_FIFOPAGE_CTRL_2_8814A + 1);
		count = 0;
		while (!(BcnValidReg & BIT7) && count < 20) {
			count++;
			delay_us(10);
			BcnValidReg = platform_efio_read_1byte(Adapter, REG_FIFOPAGE_CTRL_2_8814A+2);
		}
		DLBcnCount++;
	} while (!(BcnValidReg & BIT7) && DLBcnCount < 5);
	
	if (!(BcnValidReg & BIT0))
		RT_DISP(FBEAM, FBEAM_ERROR, ("%s Download RSVD page failed!\n", __func__));

	/*0x204[11:0]	Beacon Head for TXDMA*/
	platform_efio_write_2byte(Adapter, REG_FIFOPAGE_CTRL_2_8814A, TxPageBndy);

	/*To make sure that if there exists an adapter which would like to send beacon.*/
	/*If exists, the origianl value of 0x422[6] will be 1, we should check this to*/
	/*prevent from setting 0x422[6] to 0 after download reserved page, or it will cause */
	/*the beacon cannot be sent by HW.*/
	/*2010.06.23. Added by tynli.*/
	if (bSendBeacon)
		platform_efio_write_1byte(Adapter, REG_FWHW_TXQ_CTRL_8814A+2, tmpReg422);

	/*Do not enable HW DMA BCN or it will cause Pcie interface hang by timing issue. 2011.11.24. by tynli.*/
	/*Clear CR[8] or beacon packet will not be send to TxBuf anymore.*/
	u1bTmp = platform_efio_read_1byte(Adapter, REG_CR_8814A+1);
	platform_efio_write_1byte(Adapter, REG_CR_8814A+1, (u1bTmp&(~BIT0)));

	pBeamEntry->BeamformEntryState = BEAMFORMING_ENTRY_STATE_PROGRESSED;

	pHalData->bFwDwRsvdPageInProgress = FALSE;
}

VOID
halTxbf8812F_FwTxBFCmd(
	IN	PADAPTER	Adapter
	)
{
	u1Byte	Idx, period = 0;
	u1Byte	PageNum0 = 0xFF, PageNum1 = 0xFF;
	u1Byte	u1TxBFParm[3] = {0};

	PMGNT_INFO				pMgntInfo = &(Adapter->MgntInfo);
	struct _RT_BEAMFORMING_INFO*	pBeamInfo = GET_BEAMFORM_INFO(Adapter);

	for (Idx = 0; Idx < BEAMFORMEE_ENTRY_NUM; Idx++) {
		if (pBeamInfo->BeamformeeEntry[Idx].bUsed && pBeamInfo->BeamformeeEntry[Idx].BeamformEntryState == BEAMFORMING_ENTRY_STATE_PROGRESSED) {
			if (pBeamInfo->BeamformeeEntry[Idx].bSound) {
				PageNum0 = 0xFE;
				PageNum1 = 0x07;
				period = (u1Byte)(pBeamInfo->BeamformeeEntry[Idx].SoundPeriod);
			} else if (PageNum0 == 0xFF) {
				PageNum0 = 0xFF; /*stop sounding*/
				PageNum1 = 0x0F;
			}
		}
	}

	u1TxBFParm[0] = PageNum0;
	u1TxBFParm[1] = PageNum1;
	u1TxBFParm[2] = period;
	FillH2CCmd(Adapter, PHYDM_H2C_TXBF, 3, u1TxBFParm);
	
	RT_DISP(FBEAM, FBEAM_FUN, ("@%s End, PageNum0 = 0x%x, PageNum1 = 0x%x period = %d", __func__, PageNum0, PageNum1, period));
}
#endif

VOID
SetBeamformInit8812F(
	struct rtl8192cd_priv *priv
	)
{
	struct dm_struct*	p_dm_odm = ODMPTR;
	u1Byte		u1bTmp;
	struct _RT_BEAMFORMING_INFO* pBeamformingInfo = &(priv->pshare->BeamformingInfo);
	int i;

	odm_set_bb_reg(p_dm_odm, 0x14c0 , BIT16, 1); /*Enable P1 aggr new packet according to P0 transfer time*/

	/*MU Retry Limit*/
	if(priv->pshare->rf_ft_var.mu_retry > 15)
		priv->pshare->rf_ft_var.mu_retry = 15;

	if(priv->pshare->rf_ft_var.mu_retry == 0)
		priv->pshare->rf_ft_var.mu_retry = 10;

	for(i = 0; i < BEAMFORMEE_ENTRY_NUM; i++) {			
#if (MU_BEAMFORMING_SUPPORT == 1)		
		pBeamformingInfo->BeamformeeEntry[i].mu_reg_index = 0xFF;
#endif
		pBeamformingInfo->BeamformeeEntry[i].su_reg_index = 0xFF;
	}

	for(i = 0; i < BEAMFORMER_ENTRY_NUM; i++) {				
#if (MU_BEAMFORMING_SUPPORT == 1)		
		pBeamformingInfo->BeamformerEntry[i].is_mu_ap = FALSE;
#endif
		pBeamformingInfo->BeamformerEntry[i].su_reg_index = 0xFF;
	}
		
	odm_set_bb_reg(p_dm_odm, 0x14c0 , BIT15|BIT14|BIT13|BIT12, priv->pshare->rf_ft_var.mu_retry); 

	odm_set_bb_reg(p_dm_odm, 0x14c0 , BIT7, 0); /*Disable Tx MU-MIMO until sounding done*/	
	odm_set_bb_reg(p_dm_odm, 0x14c0 , 0x3F, 0); /* Clear validity of MU STAs */
	odm_write_1byte(p_dm_odm, 0x167c , 0x70); /*MU-MIMO Option as default value*/
	odm_write_2byte(p_dm_odm, 0x1680 , 0); /*MU-MIMO Control as default value*/

	/* Set MU NDPA rate & BW source */
	/* 0x42C[30] = 1 (0: from Tx desc, 1: from 0x45F) */
	u1bTmp = odm_read_1byte(p_dm_odm, 0x42C);
	odm_write_1byte(p_dm_odm, REG_TXBF_CTRL_8812F, (u1bTmp|BIT6)); //eric-mu
	/* 0x45F[7:0] = 0x10 (Rate=OFDM_6M, BW20) */
	odm_write_1byte(p_dm_odm, REG_NDPA_OPT_CTRL_8812F, 0x10);

	/*Temp Settings*/
	odm_set_bb_reg(p_dm_odm, 0x6DC , 0x7F000000, 0x44); /*STA2's CSI rate is fixed at 6M*/

#if 1
	/*Grouping bitmap parameters*/
	odm_set_bb_reg(p_dm_odm, 0xb34 , bMaskDWord, 0x314c314c); 
	odm_set_bb_reg(p_dm_odm, 0xb48 , bMaskDWord, 0x30cc30cc); 
	odm_set_bb_reg(p_dm_odm, 0xb28 , bMaskDWord, 0x350c350c); 
	odm_set_bb_reg(p_dm_odm, 0xb38 , bMaskDWord, 0x328c328c); 
	
	odm_set_bb_reg(p_dm_odm, 0xb4c , bMaskDWord, 0x314c314c); 
	
	if(AC_SIGMA_MODE == AC_SIGMA_APUT) 
	odm_set_bb_reg(p_dm_odm, 0xb2c , bMaskDWord, 0xafffafff); //Force MU Tx for 11ac logo r2
	else
	odm_set_bb_reg(p_dm_odm, 0xb2c , bMaskDWord, 0x3a0c3a0c);
	
	odm_set_bb_reg(p_dm_odm, 0xb3c , bMaskDWord, 0x350c350c); 
	odm_set_bb_reg(p_dm_odm, 0xb50 , bMaskDWord, 0x328c328c); 

	odm_set_bb_reg(p_dm_odm, 0xb24 , bMaskDWord, 0x328c328c); 
#endif

#if (MU_BEAMFORMING_SUPPORT == 1)
	/* Init HW variable */
	pBeamformingInfo->RegMUTxCtrl = odm_read_4byte(p_dm_odm, 0x14c0);
#endif

#if 0
	odm_set_bb_reg(p_dm_odm, 0x19E0 , BIT5|BIT4, 3); /*8812F grouping method*/
#endif

	if (p_dm_odm->rf_type == RF_2T2R) { /*2T2R*/
		PHYDM_DBG(p_dm_odm, DBG_TXBF, "%s: rf_type is 2T2R\n", __func__);
		config_phydm_trx_mode_8812f(p_dm_odm, 3, 3, 1);/*Tx2path*/
	}
}

VOID
SetBeamformEnter8812F(
	struct rtl8192cd_priv *priv,
	IN u1Byte				BFerBFeeIdx
	)
{
	struct dm_struct*	p_dm_odm = ODMPTR;
	u1Byte					i = 0;
	u1Byte					BFerIdx = (BFerBFeeIdx & 0xF0)>>4;
	u1Byte					BFeeIdx = (BFerBFeeIdx & 0xF);
	u2Byte					CSI_Param = 0;
	struct _RT_BEAMFORMING_INFO* pBeamformingInfo = &(priv->pshare->BeamformingInfo);
	PRT_BEAMFORMING_ENTRY	pBeamformeeEntry;
	struct _RT_BEAMFORMER_ENTRY*	pBeamformerEntry;
	u2Byte					value16, STAid = 0;
	u1Byte					Nc_index = 0, Nr_index = 0, grouping = 0, codebookinfo = 0, coefficientsize = 0;
#if (MU_BEAMFORMING_SUPPORT == 1)	
	u4Byte					gid_valid, user_position_l, user_position_h;
	u4Byte					mu_reg[6] = {0x1684, 0x1686, 0x1688, 0x168a, 0x168c, 0x168e};
#endif	
	u1Byte					u1bTmp;
	u4Byte					u4bTmp;
	u1Byte					h2c_content[6] = {0};
	u4Byte					flags;
	
	SAVE_INT_AND_CLI(flags);
	//RT_DISP(FBEAM, FBEAM_FUN, ("%s: BFerBFeeIdx=%d, BFerIdx=%d, BFeeIdx=%d\n", __func__, BFerBFeeIdx, BFerIdx, BFeeIdx));
#if (MU_BEAMFORMING_SUPPORT == 1)	
	PHYDM_DBG(p_dm_odm, DBG_TXBF, "[%s] BFerBFeeIdx=%d, BFerIdx=%d, BFeeIdx=%d, beamformee_su_cnt=%d, beamformee_mu_cnt=%d, beamformee_su_reg_maping=%d, beamformee_mu_reg_maping=%d\n", __func__, BFerBFeeIdx, BFerIdx, BFeeIdx, pBeamformingInfo->beamformee_su_cnt, pBeamformingInfo->beamformee_mu_cnt,pBeamformingInfo->beamformee_su_reg_maping, pBeamformingInfo->beamformee_mu_reg_maping);
#endif
	/*************SU BFer Entry Init*************/
	if ((pBeamformingInfo->beamformer_su_cnt > 0) && (BFerIdx < BEAMFORMER_ENTRY_NUM)) {
		PHYDM_DBG(p_dm_odm, DBG_TXBF, "[%s]SU BFer Entry Init\n", __func__);
		pBeamformerEntry = &pBeamformingInfo->BeamformerEntry[BFerIdx];
		pBeamformerEntry->HwState = BEAMFORM_ENTRY_HW_STATE_ADDING;
#if (MU_BEAMFORMING_SUPPORT == 1)		
		pBeamformerEntry->is_mu_ap = FALSE;
#endif
		/*Sounding protocol control*/
		odm_write_1byte(p_dm_odm, REG_SND_PTCL_CTRL_8812F, 0xDB);	
				
		/*MAC address/Partial AID of Beamformer*/
		if (pBeamformerEntry->su_reg_index == 0) {
			for (i = 0; i < 6 ; i++)
				odm_write_1byte(p_dm_odm, (REG_ASSOCIATED_BFMER0_INFO_8812F+i), pBeamformerEntry->MacAddr[i]);
		} else {
			for (i = 0; i < 6 ; i++)
				odm_write_1byte(p_dm_odm, (REG_ASSOCIATED_BFMER1_INFO_8812F+i), pBeamformerEntry->MacAddr[i]);
		}

		/*CSI report parameters of Beamformer*/
		Nc_index = halTxbf8812F_GetNrx(p_dm_odm);	/*for 8814A Nrx = 3(4 Ant), min=0(1 Ant)*/
		Nr_index = pBeamformerEntry->NumofSoundingDim;	/*0x718[7] = 1 use Nsts, 0x718[7] = 0 use reg setting. as Bfee, we use Nsts, so Nr_index don't care*/
		
		grouping = 0;

		/*for ac = 1, for n = 3*/
		if (pBeamformerEntry->BeamformEntryCap & BEAMFORMEE_CAP_VHT_SU)
			codebookinfo = 1;	
		else if (pBeamformerEntry->BeamformEntryCap & BEAMFORMEE_CAP_HT_EXPLICIT)
			codebookinfo = 3;	

		coefficientsize = 3;

		CSI_Param = (u2Byte)((coefficientsize<<10)|(codebookinfo<<8)|(grouping<<6)|(Nr_index<<3)|(Nc_index));

		if (BFerIdx == 0)
			odm_write_2byte(p_dm_odm, 0x6F4, CSI_Param);
		else
			odm_write_2byte(p_dm_odm, 0x6F4+2, CSI_Param);
		/*ndp_rx_standby_timer, 8814 need > 0x56, suggest from Dvaid*/
		odm_write_1byte(p_dm_odm, REG_SND_PTCL_CTRL_8812F+3, 0x70);
		pBeamformerEntry->HwState = BEAMFORM_ENTRY_HW_STATE_ADDED;
	}

	/*************SU BFee Entry Init*************/
	if ((pBeamformingInfo->beamformee_su_cnt > 0) && (BFeeIdx < BEAMFORMEE_ENTRY_NUM)) {
		PHYDM_DBG(p_dm_odm, DBG_TXBF, "[%s]SU BFee Entry Init\n", __func__);
		pBeamformeeEntry = &pBeamformingInfo->BeamformeeEntry[BFeeIdx];
		if((pBeamformeeEntry->BeamformEntryCap & BEAMFORMER_CAP_VHT_SU) && 
			!(pBeamformeeEntry->BeamformEntryCap & BEAMFORM_CAP_VHT_MU_BFER) &&
			pBeamformeeEntry->su_reg_index == 0xFF)
		{
			pBeamformeeEntry->HwState = BEAMFORM_ENTRY_HW_STATE_ADDING;
	#if (MU_BEAMFORMING_SUPPORT == 1)		
			pBeamformeeEntry->is_mu_sta = FALSE;
	#endif
			halTxbf8812F_RfMode(p_dm_odm, pBeamformingInfo, BFeeIdx);
		
			//if (phydm_acting_determine(p_dm_odm, phydm_acting_as_ibss))
				//STAid = pBeamformeeEntry->MacId;
			//else 
				STAid = pBeamformeeEntry->P_AID;

			for (i = 0; i < MAX_NUM_BEAMFORMEE_SU; i++) {
				if ((pBeamformingInfo->beamformee_su_reg_maping & BIT(i)) == 0) {
					pBeamformingInfo->beamformee_su_reg_maping |= BIT(i);
					pBeamformeeEntry->su_reg_index = i;
					break;
				}
			}
		
			/*P_AID of Beamformee & enable NDPA transmission & enable NDPA interrupt*/
			if (pBeamformeeEntry->su_reg_index == 0) {	
				odm_write_2byte(p_dm_odm, REG_TXBF_CTRL_8812F, STAid);	
				odm_write_1byte(p_dm_odm, REG_TXBF_CTRL_8812F+3, odm_read_1byte(p_dm_odm, REG_TXBF_CTRL_8812F+3)|BIT4|BIT6|BIT7);
			} else {
				odm_write_2byte(p_dm_odm, REG_TXBF_CTRL_8812F+2, STAid | BIT14 | BIT15 | BIT12);
			}	

			/*CSI report parameters of Beamformee*/
			if (pBeamformeeEntry->su_reg_index == 0) {
				/*Get BIT24 & BIT25*/
				u1Byte	tmp = odm_read_1byte(p_dm_odm, REG_ASSOCIATED_BFMEE_SEL_8812F+3) & 0x3;
			
				odm_write_2byte(p_dm_odm, REG_ASSOCIATED_BFMEE_SEL_8812F, STAid | BIT9);
			} else		
				odm_write_2byte(p_dm_odm, REG_ASSOCIATED_BFMEE_SEL_8812F+2, STAid | 0x0200);	/*Set BIT25*/

			pBeamformeeEntry->HwState = BEAMFORM_ENTRY_HW_STATE_ADDED;
			Beamforming_Notify(p_dm_odm->priv);
		}
	}

#if (MU_BEAMFORMING_SUPPORT == 1)
	/*************MU BFer Entry Init*************/
	if ((pBeamformingInfo->beamformer_mu_cnt > 0) && (BFerIdx < BEAMFORMER_ENTRY_NUM)) {
		PHYDM_DBG(p_dm_odm, DBG_TXBF, "[%s]MU BFer Entry Init\n", __func__);
		pBeamformerEntry = &pBeamformingInfo->BeamformerEntry[BFerIdx];
		if(pBeamformerEntry->BeamformEntryCap & BEAMFORM_CAP_VHT_MU_BFEE)
		{
			pBeamformerEntry->HwState = BEAMFORM_ENTRY_HW_STATE_ADDING;
			pBeamformingInfo->mu_ap_index = BFerIdx;
			pBeamformerEntry->is_mu_ap = TRUE;
			for (i = 0; i < 8; i++)
				pBeamformerEntry->gid_valid[i] = 0;
			for (i = 0; i < 16; i++)
				pBeamformerEntry->user_position[i] = 0;
		
			/*Sounding protocol control*/
			odm_write_1byte(p_dm_odm, REG_SND_PTCL_CTRL_8812F, 0xDB);	

			/* MAC address */
			for (i = 0; i < 6 ; i++)
				odm_write_1byte(p_dm_odm, (REG_ASSOCIATED_BFMER0_INFO_8812F+i), pBeamformerEntry->MacAddr[i]);

			/* Set partial AID */
			odm_write_2byte(p_dm_odm, (REG_ASSOCIATED_BFMER0_INFO_8812F+6), pBeamformerEntry->P_AID);

			/* Fill our AID to 0x1680[11:0] and [13:12] = 2b'00, BF report segement select to 3895 bytes*/
			value16 = odm_read_2byte(p_dm_odm, 0x1680)&0xC000;
			value16 |= (pBeamformerEntry->AID)&0xFFF;
			odm_write_2byte(p_dm_odm, 0x1680, value16);

			/* Set 80us for leaving ndp_rx_standby_state */
			odm_write_1byte(p_dm_odm, 0x71B, 0x50);
		
			/* Set 0x6A0[14] = 1 to accept action_no_ack */
			u1bTmp = odm_read_1byte(p_dm_odm, REG_RXFLTMAP0_8812F+1);
			u1bTmp |= 0x40;
			odm_write_1byte(p_dm_odm, REG_RXFLTMAP0_8812F+1, u1bTmp);
			/* Set 0x6A2[5:4] = 1 to NDPA and BF report poll */
			u1bTmp = odm_read_1byte(p_dm_odm, REG_RXFLTMAP1_8812F);
			u1bTmp |= 0x30;
			odm_write_1byte(p_dm_odm, REG_RXFLTMAP1_8812F, u1bTmp);
		
			/*CSI report parameters of Beamformer*/
			Nc_index = halTxbf8812F_GetNrx(p_dm_odm);	/* Depend on RF type */
			Nr_index = 1;	/*0x718[7] = 1 use Nsts, 0x718[7] = 0 use reg setting. as Bfee, we use Nsts, so Nr_index don't care*/
			grouping = 0; /*no grouping*/
			codebookinfo = 1; /*7 bit for psi, 9 bit for phi*/
			coefficientsize = 0; /*This is nothing really matter*/ 
			CSI_Param = (u2Byte)((coefficientsize<<10)|(codebookinfo<<8)|(grouping<<6)|(Nr_index<<3)|(Nc_index));
			odm_write_2byte(p_dm_odm, 0x6F4, CSI_Param);
			pBeamformerEntry->HwState = BEAMFORM_ENTRY_HW_STATE_ADDED;
		}
	}
	

	/*************MU BFee Entry Init*************/
	if ((pBeamformingInfo->beamformee_mu_cnt > 0) && (BFeeIdx < BEAMFORMEE_ENTRY_NUM)) {
		PHYDM_DBG(p_dm_odm, DBG_TXBF, "[%s]MU BFee Entry Init, RegMUTxCtrl=0x%x\n", __func__, pBeamformingInfo->RegMUTxCtrl);
		pBeamformeeEntry = &pBeamformingInfo->BeamformeeEntry[BFeeIdx];
		if((pBeamformeeEntry->BeamformEntryCap & BEAMFORM_CAP_VHT_MU_BFER) && pBeamformeeEntry->mu_reg_index == 0xFF)
		{
			pBeamformeeEntry->HwState = BEAMFORM_ENTRY_HW_STATE_ADDING;
			pBeamformeeEntry->is_mu_sta = TRUE;

			pBeamformeeEntry->mu_reg_index = 0xFF;
			
			for (i = 0; i < MAX_NUM_BEAMFORMEE_MU; i++) {
				if ((pBeamformingInfo->beamformee_mu_reg_maping & BIT(i)) == 0) {
					pBeamformingInfo->beamformee_mu_reg_maping |= BIT(i);
					pBeamformeeEntry->mu_reg_index = i;
					break;
				}
			}

			if (pBeamformeeEntry->mu_reg_index == 0xFF) {
				/* There is no valid bit in beamformee_mu_reg_maping */
				panic_printk("%s: ERROR! There is no valid bit in beamformee_mu_reg_maping!\n", __func__);
				//RT_DISP(FBEAM, FBEAM_FUN, ("%s: ERROR! There is no valid bit in beamformee_mu_reg_maping!\n", __func__));
				RESTORE_INT(flags);
				return;
			}
			
			/*User position table*/
			switch (pBeamformeeEntry->mu_reg_index) {
			case 0:
				gid_valid = 0x7fe;
				user_position_l = 0x111110;
				user_position_h = 0x0;
				break;
			case 1:
				gid_valid = 0x7f806;
				user_position_l = 0x11000004;
				user_position_h = 0x11;
				break;
			case 2:
				gid_valid = 0x1f81818;
				user_position_l = 0x400040;
				user_position_h = 0x11100;
				break;
			case 3:
				gid_valid = 0x1e186060;
				user_position_l = 0x4000400;
				user_position_h = 0x1100040;
				break;
			case 4:
				gid_valid = 0x66618180;
				user_position_l = 0x40004000;
				user_position_h = 0x10040400;
				break;
			case 5:
				gid_valid = 0x79860600;
				user_position_l = 0x40000;
				user_position_h = 0x4404004;
				break;
			}

			for (i = 0; i < 8; i++) {
				if (i < 4) {
					pBeamformeeEntry->gid_valid[i] = (u1Byte)(gid_valid & 0xFF);
					gid_valid = (gid_valid >> 8);
				} else
					pBeamformeeEntry->gid_valid[i] = 0;
			}
			for (i = 0; i < 16; i++) {
				if (i < 4)
					pBeamformeeEntry->user_position[i] = (u1Byte)((user_position_l >>(i*8)) & 0xFF);
				else if (i < 8)
					pBeamformeeEntry->user_position[i] = (u1Byte)((user_position_h >>((i-4)*8)) & 0xFF);
				else
					pBeamformeeEntry->user_position[i] = 0;
			}

			/*Sounding protocol control*/
			odm_write_1byte(p_dm_odm, REG_SND_PTCL_CTRL_8812F, 0xDB);	

			/*select MU STA table*/
			pBeamformingInfo->RegMUTxCtrl &= ~(BIT8|BIT9|BIT10);
			pBeamformingInfo->RegMUTxCtrl |= (pBeamformeeEntry->mu_reg_index << 8)&(BIT8|BIT9|BIT10);
			odm_write_4byte(p_dm_odm, 0x14c0, pBeamformingInfo->RegMUTxCtrl);	
		
			odm_set_bb_reg(p_dm_odm, 0x14c4 , bMaskDWord, 0); /*Reset gid_valid table*/
			odm_set_bb_reg(p_dm_odm, 0x14c8 , bMaskDWord, user_position_l);
			odm_set_bb_reg(p_dm_odm, 0x14cc , bMaskDWord, user_position_h);

			/*set validity of MU STAs*/		
			pBeamformingInfo->RegMUTxCtrl &= 0xFFFFFFC0;
			pBeamformingInfo->RegMUTxCtrl |= pBeamformingInfo->beamformee_mu_reg_maping&0x3F;
			odm_write_4byte(p_dm_odm, 0x14c0, pBeamformingInfo->RegMUTxCtrl);

			PHYDM_DBG(p_dm_odm, DBG_TXBF, "@%s, RegMUTxCtrl = 0x%x, user_position_l = 0x%x, user_position_h = 0x%x\n", 
				__func__, pBeamformingInfo->RegMUTxCtrl, user_position_l, user_position_h);

			value16 = odm_read_2byte(p_dm_odm, mu_reg[pBeamformeeEntry->mu_reg_index]);
			value16 &= 0xFE00; /*Clear PAID*/
			value16 |= BIT9; /*Enable MU BFee*/
			value16 |= pBeamformeeEntry->P_AID;
			odm_write_2byte(p_dm_odm, mu_reg[pBeamformeeEntry->mu_reg_index] , value16);
			PHYDM_DBG(p_dm_odm, DBG_TXBF, "@%s|%d, reg:0x%x value:0x%x\n", __func__, __LINE__, mu_reg[pBeamformeeEntry->mu_reg_index], value16);
			/* 0x42C[30] = 1 (0: from Tx desc, 1: from 0x45F) */
			u1bTmp = odm_read_1byte(p_dm_odm, REG_TXBF_CTRL_8812F+3);
			u1bTmp |= 0xD0; // Set bit 28, 30, 31 to 3b'111

			odm_write_1byte(p_dm_odm, REG_TXBF_CTRL_8812F+3, u1bTmp);

				if(AC_SIGMA_MODE == AC_SIGMA_APUT) {
					/* Set NDPA to VHT-1SS0 BW = 80M for 11ac logo r2*/
					odm_write_1byte(p_dm_odm, REG_NDPA_RATE_8812F, 0x2c); // VHT 1SS-0
					odm_write_1byte(p_dm_odm, REG_NDPA_OPT_CTRL_8812F, 0x12); //BW=80M
				}
				else
				{
					/* Set NDPA to 6M*/
					odm_write_1byte(p_dm_odm, REG_NDPA_RATE_8812F, 0x4); // 6M

					u1bTmp = odm_read_1byte(p_dm_odm, REG_NDPA_OPT_CTRL_8812F);
					u1bTmp &= 0xFC; // Clear bit 0, 1
					odm_write_1byte(p_dm_odm, REG_NDPA_OPT_CTRL_8812F, u1bTmp);
				}

			u4bTmp = odm_read_4byte(p_dm_odm, REG_SND_PTCL_CTRL_8812F);
			u4bTmp = ((u4bTmp & 0xFF0000FF) | 0x020200); // Set [23:8] to 0x0202
			odm_write_4byte(p_dm_odm, REG_SND_PTCL_CTRL_8812F, u4bTmp);	

			/* Set 0x6A0[14] = 1 to accept action_no_ack */
			u1bTmp = odm_read_1byte(p_dm_odm, REG_RXFLTMAP0_8812F+1);
			u1bTmp |= 0x40;
			odm_write_1byte(p_dm_odm, REG_RXFLTMAP0_8812F+1, u1bTmp);
			/* End of MAC registers setting */
			
			halTxbf8812F_RfMode(p_dm_odm, pBeamformingInfo, BFeeIdx);
			pBeamformeeEntry->HwState = BEAMFORM_ENTRY_HW_STATE_ADDED;

			/*Special for plugfest*/
			delay_ms(50); /* wait for 4-way handshake ending*/
	
			issue_action_GROUP_ID(p_dm_odm->priv, BFeeIdx);

			Beamforming_Notify(priv);

			h2c_content[0] = 1;
			h2c_content[1] = pBeamformeeEntry->mu_reg_index;
			h2c_content[2] = pBeamformeeEntry->MacId;
			h2c_content[3] = 0; // 0: support 256QAM, 1: not support 256QAM
			odm_fill_h2c_cmd(p_dm_odm, PHYDM_H2C_MU, 4, h2c_content);
			
			{
				u4Byte ctrl_info_offset, index;
				/*Set Ctrl Info*/
				odm_write_2byte(p_dm_odm, 0x140, 0x660);
				ctrl_info_offset = 0x8000 + 32 * pBeamformeeEntry->MacId;
				/*Reset Ctrl Info*/
				for (index = 0; index < 8; index++)
					odm_write_4byte(p_dm_odm, ctrl_info_offset + index*4, 0);
			
				odm_write_4byte(p_dm_odm, ctrl_info_offset, (pBeamformeeEntry->mu_reg_index + 1) << 16);
				odm_write_1byte(p_dm_odm, 0x81, 0x80); /*RPTBUF ready*/

				PHYDM_DBG(p_dm_odm, DBG_TXBF, "@%s, MacId = %d, ctrl_info_offset = 0x%x, mu_reg_index = %x\n", 
				__func__, pBeamformeeEntry->MacId, ctrl_info_offset, pBeamformeeEntry->mu_reg_index);
			}

		}
		PHYDM_DBG(p_dm_odm, DBG_TXBF, "[%s]MU BFee Entry End\n", __func__);
	}
#endif // #if (MU_BEAMFORMING_SUPPORT == 1)
	RESTORE_INT(flags);
}


VOID
SetBeamformLeave8812F(
	struct rtl8192cd_priv *priv,
	IN u1Byte				Idx
	)
{
	
	struct dm_struct*	p_dm_odm = ODMPTR;
	struct _RT_BEAMFORMING_INFO* pBeamformingInfo = &(priv->pshare->BeamformingInfo);
	struct _RT_BEAMFORMER_ENTRY*	pBeamformerEntry = NULL; 
	PRT_BEAMFORMING_ENTRY	pBeamformeeEntry = NULL;
	u4Byte					mu_reg[6] = {0x1684, 0x1686, 0x1688, 0x168a, 0x168c, 0x168e};
	u1Byte					h2c_content[6] = {0};

	if (Idx < BEAMFORMER_ENTRY_NUM)
		pBeamformerEntry = &pBeamformingInfo->BeamformerEntry[Idx];
	
	if (Idx < BEAMFORMEE_ENTRY_NUM)
		pBeamformeeEntry = &pBeamformingInfo->BeamformeeEntry[Idx];
	
	/*Clear P_AID of Beamformee*/
	/*Clear MAC address of Beamformer*/
	/*Clear Associated Bfmee Sel*/

	if(pBeamformerEntry && pBeamformerEntry->BeamformEntryCap == BEAMFORMING_CAP_NONE) { //eric-txbf
		pBeamformerEntry = &pBeamformingInfo->BeamformerEntry[Idx];
		odm_write_1byte(p_dm_odm, REG_SND_PTCL_CTRL_8812F, 0xD8);	
#if (MU_BEAMFORMING_SUPPORT == 1)		
		if (pBeamformerEntry->is_mu_ap) { /*MU BFer */
			/*set validity of MU STA0 and MU STA1*/
			if(pBeamformingInfo->beamformer_mu_cnt > 0)
				pBeamformingInfo->beamformer_mu_cnt -= 1;
			else
				panic_printk("%s(%d) Invalid beamformer_mu_cnt=%d\n", __FUNCTION__, __LINE__, pBeamformingInfo->beamformer_mu_cnt);
			
			pBeamformingInfo->RegMUTxCtrl &= 0xFFFFFFC0;
			odm_write_4byte(p_dm_odm, 0x14c0, pBeamformingInfo->RegMUTxCtrl);

			odm_memory_set(p_dm_odm, pBeamformerEntry->gid_valid, 0, 8);
			odm_memory_set(p_dm_odm, pBeamformerEntry->user_position, 0, 16);
			pBeamformerEntry->is_mu_ap = FALSE;
		} else 
#endif		
		{ /*SU BFer */
			if(pBeamformerEntry->su_reg_index != 0xFF) {
				if(pBeamformingInfo->beamformer_su_cnt > 0)
					pBeamformingInfo->beamformer_su_cnt -= 1;
				else
					panic_printk("%s(%d) Invalid beamformer_su_cnt=%d\n", __FUNCTION__, __LINE__, pBeamformingInfo->beamformer_su_cnt);
				
				if (pBeamformerEntry->su_reg_index == 0) {	
					odm_write_4byte(p_dm_odm, REG_ASSOCIATED_BFMER0_INFO_8812F, 0);
					odm_write_2byte(p_dm_odm, REG_ASSOCIATED_BFMER0_INFO_8812F+4, 0);
					odm_write_2byte(p_dm_odm, 0x6F4, 0);
				} else {
					odm_write_4byte(p_dm_odm, REG_ASSOCIATED_BFMER1_INFO_8812F, 0);
					odm_write_2byte(p_dm_odm, REG_ASSOCIATED_BFMER1_INFO_8812F+4, 0);
					odm_write_2byte(p_dm_odm, 0x6F4+2, 0);
				}
				pBeamformingInfo->beamformer_su_reg_maping &= ~(BIT(pBeamformerEntry->su_reg_index));
				pBeamformerEntry->su_reg_index = 0xFF;
			}
		} 
	}
	if(pBeamformeeEntry && pBeamformeeEntry->BeamformEntryCap == BEAMFORMING_CAP_NONE) { //eric-txbf
		halTxbf8812F_RfMode(p_dm_odm, pBeamformingInfo, Idx);
#if (MU_BEAMFORMING_SUPPORT == 1)		
		if (pBeamformeeEntry->is_mu_sta == 0) 
#endif			
		{ /*SU BFee*/
			if(pBeamformeeEntry->su_reg_index != 0xFF) { 
				if(pBeamformingInfo->beamformee_su_cnt > 0)
					pBeamformingInfo->beamformee_su_cnt -= 1;
				else
					panic_printk("%s(%d) Invalid beamformee_su_cnt=%d\n", __FUNCTION__, __LINE__, pBeamformingInfo->beamformee_su_cnt);

				if (pBeamformeeEntry->su_reg_index == 0) {	
					odm_write_2byte(p_dm_odm, REG_TXBF_CTRL_8812F, 0x0);	
					odm_write_1byte(p_dm_odm, REG_TXBF_CTRL_8812F+3, odm_read_1byte(p_dm_odm, REG_TXBF_CTRL_8812F+3)|BIT4|BIT6|BIT7);
					odm_write_2byte(p_dm_odm, REG_ASSOCIATED_BFMEE_SEL_8812F, 0);
				} else {
					odm_write_2byte(p_dm_odm, REG_TXBF_CTRL_8812F+2, 0x0 | BIT14 | BIT15 | BIT12);

					odm_write_2byte(p_dm_odm, REG_ASSOCIATED_BFMEE_SEL_8812F+2, 
					odm_read_2byte(p_dm_odm, REG_ASSOCIATED_BFMEE_SEL_8812F+2) & 0x60);
				}
				pBeamformingInfo->beamformee_su_reg_maping &= ~(BIT(pBeamformeeEntry->su_reg_index));
				pBeamformeeEntry->su_reg_index = 0xFF;
			}
		} 
#if (MU_BEAMFORMING_SUPPORT == 1)		
		else { /*MU BFee */
			if(pBeamformeeEntry->mu_reg_index < MAX_NUM_BEAMFORMEE_MU ) {
				/*Disable sending NDPA & BF-rpt-poll to this BFee*/
				if(pBeamformingInfo->beamformee_mu_cnt > 0)
					pBeamformingInfo->beamformee_mu_cnt -= 1;
				else
					panic_printk("%s(%d) Invalid beamformee_mu_cnt=%d\n", __FUNCTION__, __LINE__, pBeamformingInfo->beamformee_mu_cnt);
				
				odm_write_2byte(p_dm_odm, mu_reg[pBeamformeeEntry->mu_reg_index] , 0);
				/*set validity of MU STA*/
				pBeamformingInfo->RegMUTxCtrl &= ~(BIT(pBeamformeeEntry->mu_reg_index));
				odm_write_4byte(p_dm_odm, 0x14c0, pBeamformingInfo->RegMUTxCtrl);

				h2c_content[0] = 2;
				h2c_content[1] = pBeamformeeEntry->mu_reg_index;
				odm_fill_h2c_cmd(p_dm_odm, PHYDM_H2C_MU, 2, h2c_content);

				pBeamformeeEntry->is_mu_sta = FALSE;				
				pBeamformingInfo->beamformee_mu_reg_maping &= ~(BIT(pBeamformeeEntry->mu_reg_index));
				pBeamformeeEntry->mu_reg_index = 0xFF;
			} 
		}
#endif		
	}
}

void
Beamforming_dym_disable_bf_coeff_8812F(
	struct rtl8192cd_priv *priv,
	u1Byte				   enable
)
{
	if(enable) {
		RTL_W16(0x1910, 0x200);
		RTL_W16(0x9e8, RTL_R16(0x9e8) | BIT12);
	} else {
		RTL_W16(0x1910, 0);
		RTL_W16(0x9e8, RTL_R16(0x9e8) & ~BIT12);
	}
}

/***********SU & MU BFee Entry Only when souding done****************/
VOID
SetBeamformStatus8812F(
	struct rtl8192cd_priv *priv,
	IN u1Byte				Idx
	)
{
	struct dm_struct*	p_dm_odm = ODMPTR;
	u2Byte					BeamCtrlVal, tmpVal;
	u4Byte					BeamCtrlReg;
	struct _RT_BEAMFORMING_INFO* pBeamformingInfo = &(priv->pshare->BeamformingInfo);
	PRT_SOUNDING_INFOV2 pSoundingInfo = &(pBeamformingInfo->SoundingInfoV2);
	PRT_BEAMFORMING_ENTRY	pBeamformEntry;
	u16 bitmap;
	u8 idx, gid, i;
	u8 id1, id0;
#if (MU_BEAMFORMING_SUPPORT == 1)	
	u32 gid_valid[6] = {0};
	u32 user_position_lsb[6] = {0};
	u32 user_position_msb[6] = {0};
	u8  bSnding;
#endif	
	u32 value32;
	BOOLEAN is_sounding_success[6] = {FALSE};
	u8		snd_result = 0, isApplyMU = 0;

	if (Idx < BEAMFORMEE_ENTRY_NUM)
		pBeamformEntry = &pBeamformingInfo->BeamformeeEntry[Idx];
	else
		return;

	/*SU sounding done */
	if(pSoundingInfo->State == SOUNDING_STATE_SU_SOUNDDOWN) {
		
		BeamCtrlVal = pBeamformEntry->P_AID;

		PHYDM_DBG(p_dm_odm, DBG_TXBF, "@%s, BeamformEntry.BeamformEntryState = %d", __func__, pBeamformEntry->BeamformEntryState);

		if (pBeamformEntry->su_reg_index == 0) {
			BeamCtrlReg = REG_TXBF_CTRL_8812F;
		} else {
			BeamCtrlReg = REG_TXBF_CTRL_8812F+2;
			BeamCtrlVal |= BIT12|BIT14|BIT15;
		}
			
		if(pBeamformEntry->pSTA && pBeamformEntry->pSTA->error_csi == 1 && pBeamformEntry->pSTA->current_tx_rate >= _NSS2_MCS0_RATE_ && (priv->pshare->rf_ft_var.applyVmatrix)) {
			PHYDM_DBG(p_dm_odm, DBG_TXBF, "2SS Don't Apply Vmatrix");
			BeamCtrlVal &= ~(BIT9|BIT10|BIT11);
		}
#ifdef CONFIG_VERIWAVE_MU_CHECK		
		else if(pBeamformEntry->pSTA && pBeamformEntry->pSTA->isVeriwaveSTA && !pBeamformEntry->is_mu_sta) {
			BeamCtrlVal &= ~(BIT9|BIT10|BIT11); 
		}	
#endif
		else if ((pBeamformEntry->BeamformEntryState == BEAMFORMING_ENTRY_STATE_PROGRESSED) && (priv->pshare->rf_ft_var.applyVmatrix)) {
			if (pBeamformEntry->BW == CHANNEL_WIDTH_20)
				BeamCtrlVal |= BIT9;
			else if (pBeamformEntry->BW == CHANNEL_WIDTH_40)
				BeamCtrlVal |= (BIT9|BIT10);
			else if (pBeamformEntry->BW == CHANNEL_WIDTH_80)
				BeamCtrlVal |= (BIT9|BIT10|BIT11);		
		} else {
			PHYDM_DBG(p_dm_odm, DBG_TXBF, "@%s, Don't apply Vmatrix", __func__);
			BeamCtrlVal &= ~(BIT9|BIT10|BIT11);
		}

		odm_write_2byte(p_dm_odm, BeamCtrlReg, BeamCtrlVal);
		/*disable NDP packet use beamforming */
		tmpVal = odm_read_2byte(p_dm_odm, REG_TXBF_CTRL_8812F);
		odm_write_2byte(p_dm_odm, REG_TXBF_CTRL_8812F, tmpVal|BIT15);
	} 
#if (MU_BEAMFORMING_SUPPORT == 1)	
	else {
		//PHYDM_DBG(p_dm_odm, DBG_TXBF, "@%s, MU Sounding Done\n", __func__);
		/*MU sounding done */
		if (1){//(pBeamformEntry->BeamformEntryState == BEAMFORMING_ENTRY_STATE_PROGRESSED) {
			//PHYDM_DBG(p_dm_odm, DBG_TXBF, "@%s, BEAMFORMING_ENTRY_STATE_PROGRESSED\n", __func__);

			value32 = odm_get_bb_reg(p_dm_odm, 0x1684, bMaskDWord);
			is_sounding_success[0] = (value32 & BIT10)?1:0;
			is_sounding_success[1] = (value32 & BIT26)?1:0;
			value32 = odm_get_bb_reg(p_dm_odm, 0x1688, bMaskDWord);
			is_sounding_success[2] = (value32 & BIT10)?1:0;
			is_sounding_success[3] = (value32 & BIT26)?1:0;
			value32 = odm_get_bb_reg(p_dm_odm, 0x168C, bMaskDWord);
			is_sounding_success[4] = (value32 & BIT10)?1:0;
			is_sounding_success[5] = (value32 & BIT26)?1:0;

#ifdef CONFIG_VERIWAVE_MU_CHECK		
			for(Idx = 0; Idx < BEAMFORMEE_ENTRY_NUM; Idx++)
			{		
				pBeamformEntry = &(pBeamformingInfo->BeamformeeEntry[Idx]);
				if(pBeamformEntry->pSTA && pBeamformEntry->pSTA->isVeriwaveInValidSTA) {					
					Beamforming_set_csi_data(priv, pBeamformEntry->pSTA->isVeriwaveInValidSTA - 1, pBeamformEntry->mu_reg_index);						
					is_sounding_success[pBeamformEntry->mu_reg_index] = 1;
				}
			}
#endif
			snd_result = is_sounding_success[0];
			for(i=1;i<6;i++) {
				snd_result |= (is_sounding_success[i] << i);
			}

			isApplyMU = 0;
			for(i = 0; i < BEAMFORMEE_ENTRY_NUM; i++) {				
				pBeamformEntry = &(pBeamformingInfo->BeamformeeEntry[i]);
				if( pBeamformEntry->bUsed && pBeamformEntry->is_mu_sta && pBeamformEntry->pSTA && pBeamformEntry->mu_reg_index < MAX_NUM_BEAMFORMEE_MU) {
						isApplyMU |= (pBeamformEntry->pSTA->isRssiApplyMU << pBeamformEntry->mu_reg_index);
				}
			}
			pBeamformingInfo->RegMUTxCtrl &= 0xFFFFFFC0;
			pBeamformingInfo->RegMUTxCtrl |= (0x3F & snd_result & isApplyMU);
				
			PHYDM_DBG(p_dm_odm, DBG_TXBF, "@%s, is_sounding_success STA1:%d,  STA2:%d, STA3:%d, STA4:%d, STA5:%d, STA6:%d\n", 
				__func__, is_sounding_success[0], is_sounding_success[1] , is_sounding_success[2] , is_sounding_success[3] , is_sounding_success[4] , is_sounding_success[5]);

			bSnding = 0;
			for(i = 0; i < BEAMFORMEE_ENTRY_NUM; i++) {				
				pBeamformEntry = &(pBeamformingInfo->BeamformeeEntry[i]);
				if( pBeamformEntry->bUsed && pBeamformEntry->is_mu_sta && pBeamformEntry->pSTA && pBeamformEntry->mu_reg_index < MAX_NUM_BEAMFORMEE_MU) {
					if(pBeamformEntry->pSTA->isSendNDPA) {
						bSnding = 1;
						if(is_sounding_success[pBeamformEntry->mu_reg_index])
							priv->pshare->rf_ft_var.mu_ok[pBeamformEntry->mu_reg_index] ++;
						else {							
							priv->pshare->rf_ft_var.mu_fail[pBeamformEntry->mu_reg_index] ++;
						}
					}
				}
			}
			
			{
				u2Byte ptcl_gen_mu_cnt, wmac_tx_mu_cnt, wmac_rx_mu_ba_ok_cnt, wmac_tx_bar_cnt, wmac_rx_sta2_ba_ok_cnt, mu2su_tx_cnt; 

				odm_set_bb_reg(p_dm_odm, 0x14DC, 0xF0000, 0);
				ptcl_gen_mu_cnt = odm_get_bb_reg(p_dm_odm, 0x14DC, 0xFFFF);
				odm_set_bb_reg(p_dm_odm, 0x14DC, 0xF0000, 1);
				wmac_tx_mu_cnt = odm_get_bb_reg(p_dm_odm, 0x14DC, 0xFFFF);
				odm_set_bb_reg(p_dm_odm, 0x14DC, 0xF0000, 2);
				wmac_rx_mu_ba_ok_cnt = odm_get_bb_reg(p_dm_odm, 0x14DC, 0xFFFF);
				odm_set_bb_reg(p_dm_odm, 0x14DC, 0xF0000, 3);
				wmac_tx_bar_cnt = odm_get_bb_reg(p_dm_odm, 0x14DC, 0xFFFF);
				odm_set_bb_reg(p_dm_odm, 0x14DC, 0xF0000, 4);
				wmac_rx_sta2_ba_ok_cnt = odm_get_bb_reg(p_dm_odm, 0x14DC, 0xFFFF);
				odm_set_bb_reg(p_dm_odm, 0x14DC, 0xF0000, 5);
				mu2su_tx_cnt = odm_get_bb_reg(p_dm_odm, 0x14DC, 0xFFFF);
				
				PHYDM_DBG(p_dm_odm, DBG_TXBF, "%s, ptcl_gen_mu_cnt = %d\n",  __func__, ptcl_gen_mu_cnt);
				PHYDM_DBG(p_dm_odm, DBG_TXBF, "%s, wmac_tx_mu_cnt = %d\n",  __func__, wmac_tx_mu_cnt);
				PHYDM_DBG(p_dm_odm, DBG_TXBF, "%s, wmac_rx_mu_ba_ok_cnt = %d\n",  __func__, wmac_rx_mu_ba_ok_cnt);
				PHYDM_DBG(p_dm_odm, DBG_TXBF, "%s, wmac_tx_bar_cnt = %d\n",  __func__, wmac_tx_bar_cnt);
				PHYDM_DBG(p_dm_odm, DBG_TXBF, "%s, wmac_rx_sta2_ba_ok_cnt = %d\n",  __func__, wmac_rx_sta2_ba_ok_cnt);
				PHYDM_DBG(p_dm_odm, DBG_TXBF, "%s, mu2su_tx_cnt = %d\n\n\n",  __func__, mu2su_tx_cnt);

				odm_set_bb_reg(p_dm_odm, 0x14DC, BIT20, 1); //reset MU counter
			}	
			
			value32 = odm_get_bb_reg(p_dm_odm, 0x2F10, 0xFFFF);
			//odm_set_bb_reg(p_dm_odm, 0x19E0, bMaskHWord, 0xFFFF);/*Let MAC ignore bitmap*/
			
			bitmap = (u16)(value32 & 0x7FFF);

			if(bSnding) {
				if(bitmap)
					priv->pshare->rf_ft_var.mu_BB_ok++;
				else
					priv->pshare->rf_ft_var.mu_BB_fail++;
			}

			beamform_MUGrouping(priv, bitmap, priv->pshare->rf_ft_var.muPairResult);

		
			for (idx = 0; idx < 15; idx++) {
				if (idx < 5) {/*bit0~4*/
					id0 = 0;
					id1 = (u8)(idx + 1);
				} else if (idx < 9) { /*bit5~8*/
					id0 = 1;
					id1 = (u8)(idx - 3);
				} else if (idx < 12) { /*bit9~11*/
					id0 = 2;
					id1 = (u8)(idx - 6);
				} else if (idx < 14) { /*bit12~13*/	
					id0 = 3;
					id1 = (u8)(idx - 8);
				} else { /*bit14*/
					id0 = 4;
					id1 = (u8)(idx - 9);
				}
				
				if(priv->pshare->rf_ft_var.muPairResult[id0] != id1) // unmark nonpair bitmap
					continue;
				
				if (bitmap & BIT(idx)) {
					/*Pair 1*/
					gid = (idx << 1) + 1;
					gid_valid[id0] |= (BIT(gid));
					gid_valid[id1] |= (BIT(gid));
					/*Pair 2*/
					gid += 1;
					gid_valid[id0] |= (BIT(gid));
					gid_valid[id1] |= (BIT(gid));
				} else {
					/*Pair 1*/
					gid = (idx << 1) + 1;
					gid_valid[id0] &= ~(BIT(gid));
					gid_valid[id1] &= ~(BIT(gid));
					/*Pair 2*/
					gid += 1;
					gid_valid[id0] &= ~(BIT(gid));
					gid_valid[id1] &= ~(BIT(gid));
				}
			}

			for (i = 0; i < BEAMFORMEE_ENTRY_NUM; i++) {
				pBeamformEntry = &pBeamformingInfo->BeamformeeEntry[i];
				if (pBeamformEntry->bUsed && pBeamformEntry->is_mu_sta && (pBeamformEntry->mu_reg_index < MAX_NUM_BEAMFORMEE_MU)) {
					value32 = gid_valid[pBeamformEntry->mu_reg_index];
					for (idx = 0; idx < 4; idx++) {
						pBeamformEntry->gid_valid[idx] = (u8)(value32 & 0xFF);
						value32 = (value32 >> 8);
					}
				}
			}

			for (idx = 0; idx < 6; idx++) {
				pBeamformingInfo->RegMUTxCtrl &= ~(BIT8|BIT9|BIT10);
				pBeamformingInfo->RegMUTxCtrl |= ((idx<<8)&(BIT8|BIT9|BIT10));
				odm_write_4byte(p_dm_odm, 0x14c0, pBeamformingInfo->RegMUTxCtrl);
				odm_set_mac_reg(p_dm_odm, 0x14C4, bMaskDWord, gid_valid[idx]); /*set MU STA gid valid table*/
			}

			/*Enable TxMU PPDU*/
#if 1 //eric-mu enable mu tx
			if (priv->pshare->rf_ft_var.applyVmatrix)
				pBeamformingInfo->RegMUTxCtrl |= BIT7;
			else
				pBeamformingInfo->RegMUTxCtrl &= ~ (BIT7 | 0x3F);
#else
			pBeamformingInfo->RegMUTxCtrl |= BIT7;
#endif

			odm_write_4byte(p_dm_odm, 0x14c0, pBeamformingInfo->RegMUTxCtrl);
		}
	}
#endif	
}

#ifdef CONFIG_VERIWAVE_MU_CHECK	
void Beamforming_set_csi_data(struct rtl8192cd_priv *priv, int idx, int reg_idx)
{
	struct dm_struct*	p_dm_odm = ODMPTR;
	unsigned char csidata[2][3] = {{0xb4, 0x45, 0x5b},
										{0x12, 0x28, 0x81}};
	unsigned long	flags;

	unsigned int i;
	unsigned int memaddr;
    unsigned int dw0,dw1; 
	memaddr=0;	

	odm_set_bb_reg(p_dm_odm, 0x19b8, 0x40,1);
	odm_set_bb_reg(p_dm_odm, 0x19e0, 0x3FC0,0xFF);

	odm_set_bb_reg(p_dm_odm, 0x9e8, 0x2000000, 1); //abnormal txbf
	odm_set_bb_reg(p_dm_odm, 0x9e8, 0x1000000, 0); //read phi psi
	odm_set_bb_reg(p_dm_odm, 0x9e8, 0x70000000, reg_idx+2); //user 0	                            
	odm_set_bb_reg(p_dm_odm, 0x1910, 0x8000, 0); //BFer

	memaddr = 4;
	dw0 = 0;	
	dw1 = 0;
	for(i=32;i<32+3*128;i++) {
		if(i%8 == 0) {
			dw0 = 0;
			dw1 = 0;
		}
		if(((i)%8) < 4)
			dw0 = (dw0 >> 8) + (csidata[idx][(i-32)%3]<<24);
		else
			dw1 = (dw1 >> 8) + (csidata[idx][(i-32)%3]<<24);

		if((i%8) == 7) {			
			odm_set_bb_reg(p_dm_odm, 0x9e8, 0x1000000, 0); //read phi psi
			odm_set_bb_reg(p_dm_odm, 0x1910, 0x3FF0000, memaddr);
			odm_set_bb_reg(p_dm_odm, 0x1900, 0xFFFFFFFF, dw1);
			odm_set_bb_reg(p_dm_odm, 0x09ec, 0xFFFFFFFF, dw0);
			odm_set_bb_reg(p_dm_odm, 0x9e8, 0x1000000, 1); //write phi psi
			

				
			memaddr ++;
			if (memaddr>=80) {
				memaddr =0;
			}
		
		}
		
	}
	odm_set_bb_reg(p_dm_odm, 0x9e8, 0x2000000, 0); //normal txbf

}
#endif

VOID Beamforming_CSIRate_8812F(
	struct rtl8192cd_priv *priv
)
{
	struct dm_struct*	p_dm_odm = ODMPTR;
	u1Byte Rate;

	if(priv->pshare->rssi_min > 30) // link RSSI > 30%
		Rate = 8;
	else
		Rate = 4;

	odm_set_bb_reg(p_dm_odm, 0x6dc, 0x3F000000, Rate);
}

VOID Beamforming_NDPARate_8812F(
	struct rtl8192cd_priv *priv,
	BOOLEAN		Mode,
	u1Byte		BW,
	u1Byte		Rate
	)
{
	u2Byte	NDPARate = Rate;

	if (NDPARate == 0) {
		if(priv->pshare->rssi_min > 30) // link RSSI > 30%
			NDPARate = 0x8;				//MGN_24M
		else
			NDPARate = 0x4;				//MGN_6M
	}

	RTL_W8(REG_NDPA_RATE, NDPARate);

}


#if (MU_BEAMFORMING_SUPPORT == 1)

/*Only used for MU BFer Entry when get GID management frame (self is as MU STA)*/
VOID
HalTxbf8812F_ConfigGtab(
	struct rtl8192cd_priv *priv
	)
{
	struct dm_struct*	p_dm_odm = ODMPTR;
	struct _RT_BEAMFORMING_INFO* pBeamformingInfo = &(priv->pshare->BeamformingInfo);
	struct _RT_BEAMFORMER_ENTRY*	pBeamformerEntry = NULL;
	u4Byte		gid_valid = 0, user_position_l = 0, user_position_h = 0, i;

	if (pBeamformingInfo->mu_ap_index < BEAMFORMER_ENTRY_NUM)
		pBeamformerEntry = &pBeamformingInfo->BeamformerEntry[pBeamformingInfo->mu_ap_index];
	else
		return;

	PHYDM_DBG(p_dm_odm, DBG_TXBF, "%s==>\n", __func__);

	/*For GID 0~31*/
	for (i = 0; i < 4; i++)
		gid_valid |= (pBeamformerEntry->gid_valid[i] << (i<<3));
	for (i = 0; i < 8; i++) {
		if (i < 4)
			user_position_l |= (pBeamformerEntry->user_position[i] << (i << 3));
		else
			user_position_h |= (pBeamformerEntry->user_position[i] << ((i - 4)<<3));
	}
	/*select MU STA0 table*/
	pBeamformingInfo->RegMUTxCtrl &= ~(BIT8|BIT9|BIT10);
	odm_write_4byte(p_dm_odm, 0x14c0, pBeamformingInfo->RegMUTxCtrl);
	odm_set_bb_reg(p_dm_odm, 0x14c4, bMaskDWord, gid_valid); 
	odm_set_bb_reg(p_dm_odm, 0x14c8, bMaskDWord, user_position_l);
	odm_set_bb_reg(p_dm_odm, 0x14cc, bMaskDWord, user_position_h);

	PHYDM_DBG(p_dm_odm, DBG_TXBF, "%s: STA0: gid_valid = 0x%x, user_position_l = 0x%x, user_position_h = 0x%x\n",
		__func__, gid_valid, user_position_l, user_position_h);

	gid_valid = 0;
	user_position_l = 0;
	user_position_h = 0;

	/*For GID 32~64*/
	for (i = 4; i < 8; i++)
		gid_valid |= (pBeamformerEntry->gid_valid[i] << ((i - 4)<<3));
	for (i = 8; i < 16; i++) {
		if (i < 12)
			user_position_l |= (pBeamformerEntry->user_position[i] << ((i - 8) << 3));
		else
			user_position_h |= (pBeamformerEntry->user_position[i] << ((i - 12) << 3));
	}
	/*select MU STA1 table*/
	pBeamformingInfo->RegMUTxCtrl &= ~(BIT8|BIT9|BIT10);
	pBeamformingInfo->RegMUTxCtrl |= BIT8;
	odm_write_4byte(p_dm_odm, 0x14c0, pBeamformingInfo->RegMUTxCtrl);
	odm_set_bb_reg(p_dm_odm, 0x14c4, bMaskDWord, gid_valid); 
	odm_set_bb_reg(p_dm_odm, 0x14c8, bMaskDWord, user_position_l);
	odm_set_bb_reg(p_dm_odm, 0x14cc, bMaskDWord, user_position_h);

	PHYDM_DBG(p_dm_odm, DBG_TXBF, "%s: STA1: gid_valid = 0x%x, user_position_l = 0x%x, user_position_h = 0x%x\n",
		__func__, gid_valid, user_position_l, user_position_h);

	/* Set validity of MU STA0 and MU STA1*/
	pBeamformingInfo->RegMUTxCtrl &= 0xFFFFFFC0;
	pBeamformingInfo->RegMUTxCtrl |= 0x3; /* STA0, STA1*/
	odm_write_4byte(p_dm_odm, 0x14c0, pBeamformingInfo->RegMUTxCtrl);
	
}
#endif

VOID
HalTxbf8812F_FwTxBF(
	IN PVOID			p_dm_void,
	IN	u1Byte				Idx
	)
{
#if 0
	struct _RT_BEAMFORMING_INFO* 	pBeamInfo = GET_BEAMFORM_INFO(Adapter);
	struct _RT_BEAMFORMEE_ENTRY*	pBeamEntry = pBeamInfo->BeamformeeEntry+Idx;

	if (pBeamEntry->BeamformEntryState == BEAMFORMING_ENTRY_STATE_PROGRESSING)
		halTxbf8812F_DownloadNDPA(Adapter, Idx);

	halTxbf8812F_FwTxBFCmd(Adapter);
#endif
}

VOID
C2HTxBeamformingHandler_8812F(
	struct rtl8192cd_priv *priv,
		pu1Byte			CmdBuf,
		u1Byte			CmdLen
)
{
	u1Byte 	status = CmdBuf[0] & BIT0;

	//PHYDM_DBG(ODMPTR, DBG_TXBF, "%s =>\n", __FUNCTION__);
	Beamform_SoundingDown(priv, status);
}

#endif 


