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
InitPON8812FS(
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
			PWR_INTF_SDIO_MSK, rtl8812F_card_enable_flow))
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
StopHW8812FS(
    IN  HAL_PADAPTER Adapter
)
{
    // TODO:

    return RT_STATUS_SUCCESS;
}


enum rt_status
ResetHWForSurprise8812FS(
    IN  HAL_PADAPTER Adapter
)
{
    // TODO: Filen, necessary to be added code here

    return RT_STATUS_SUCCESS;
}

VOID
InitIMR_8812FS(
    IN  HAL_PADAPTER    Adapter,
    IN  RT_OP_MODE      OPMode
)
{
	InitSdioInterrupt(Adapter);
}

VOID
EnableIMR_8812FS(
    IN  HAL_PADAPTER    Adapter
)
{
	EnableSdioInterrupt(Adapter);
}

enum rt_status
StopHW_8812FS(
    IN  HAL_PADAPTER Adapter
)
{   
	HAL_RTL_W8(REG_RSV_CTRL, HAL_RTL_R8(REG_RSV_CTRL)& ~BIT(1));//unlock reg0x00~0x03 for 8881a, 92e
	HAL_RTL_W32(REG_HIMR0, 0);
	HAL_RTL_W32(REG_HIMR1, 0);
	HAL_RTL_W32(REG_HSIMR, 0); 
		
#if IS_RTL88XX_MAC_V2    
	if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2) {
		HAL_RTL_W32(REG_HIMR2, 0);	  
		HAL_RTL_W32(REG_HIMR3, 0);
	}
#endif 
	   
#if CFG_HAL_TX_AMSDU
	if ( IS_SUPPORT_TX_AMSDU(Adapter) ) {
		HAL_RTL_W32(REG_FTIMR, 0);
	}
#endif

	//MCU reset
	HAL_RTL_W8(REG_RSV_CTRL+1, HAL_RTL_R8(REG_RSV_CTRL+1) & ~BIT0);
	HAL_RTL_W16(REG_SYS_FUNC_EN, HAL_RTL_R16(REG_SYS_FUNC_EN) & ~BIT10);

	// Run LPS WL RFOFF flow
	//if (_FALSE == HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, 
	//		PWR_INTF_SDIO_MSK, rtl8812F_enter_lps_flow)) {
	//	RT_TRACE(COMP_INIT, DBG_SERIOUS, ("rtl8192E_enter_lps_flow\n"));
	//}
	
	// Disable CMD53 R/W Operation
	GET_HAL_INTF_DATA(Adapter)->bMacPwrCtrlOn = FALSE;
	
	// Card disable power action flow
	//if (_FALSE == HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK,
	//		PWR_INTF_SDIO_MSK, rtl8812F_card_disable_flow)) {
	//	RT_TRACE(COMP_INIT, DBG_SERIOUS, ("rtl8192E_card_disable_flow\n"));
	//}
	if (HALMAC_RET_SUCCESS != GET_MACHALAPI_INTERFACE(Adapter)->halmac_mac_power_switch(Adapter->pHalmac_adapter, HALMAC_MAC_POWER_OFF)) {
		printk("power down sequence failed.\n");
	}

	// Reset IO Wraper
//	HAL_RTL_W8(REG_RSV_CTRL+1, HAL_RTL_R8(REG_RSV_CTRL+1) & ~BIT(3));
//	HAL_RTL_W8(REG_RSV_CTRL+1, HAL_RTL_R8(REG_RSV_CTRL+1) | BIT(3));

	// lock ISO/CLK/Power control register
//	HAL_RTL_W8(REG_RSV_CTRL, 0x0e);

	return RT_STATUS_SUCCESS;
}


#if CFG_HAL_SUPPORT_MBSSID
VOID
InitMBSSID_8812FS(
    IN  HAL_PADAPTER Adapter
)
{
	rtl8192cd_init_mbssid(Adapter);
}

VOID
StopMBSSID_8812FS(
    IN  HAL_PADAPTER Adapter
)
{
	rtl8192cd_stop_mbssid(Adapter);
}
#endif

enum rt_status
SetMACIDSleep_8812FS(
    IN  HAL_PADAPTER Adapter,
    IN  BOOLEAN      bSleep,   
    IN  u4Byte       aid
)
{
	notify_macid_pause_change(Adapter, aid, bSleep);
	
        return RT_STATUS_SUCCESS;
}

void
UpdateHalMSRRPT_8812FS(
	IN HAL_PADAPTER     Adapter,
	HAL_PSTAINFO        pEntry,
	u1Byte              opmode
	)
{
	notify_update_sta_msr(Adapter, pEntry, opmode);
}


enum rt_status
hal_Associate_8812FS(
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
    pHalFunc->InitPONHandler        = InitPON88XX; //InitPON8812FS;
    pHalFunc->InitMACHandler        = InitMAC88XX;
    pHalFunc->InitFirmwareHandler   = InitMIPSFirmware88XX;  
#if CFG_HAL_SUPPORT_MBSSID    
    pHalFunc->InitMBSSIDHandler     = InitMBSSID_8812FS;
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
    pHalFunc->StopMBSSIDHandler     = StopMBSSID_8812FS;
#endif  //CFG_HAL_SUPPORT_MBSSID
    pHalFunc->StopHWHandler         = StopHW_8812FS;
    pHalFunc->StopSWHandler         = StopSW88XX;
    pHalFunc->DisableVXDAPHandler   = DisableVXDAP88XX;
    pHalFunc->ResetHWForSurpriseHandler     = ResetHWForSurprise8812FS;

    //
    //ISR Related
    //
    pHalFunc->InitIMRHandler                    = InitIMR_8812FS;
    pHalFunc->EnableIMRHandler                  = EnableIMR_8812FS;

    //
    //Tx Related
    //              

    //
    //Rx Related
    // 

    //
    // General operation
    //
    pHalFunc->GetChipIDMIMOHandler          =   GetChipIDMIMO88XX;
    pHalFunc->SetHwRegHandler               =   SetHwReg88XX;
    pHalFunc->GetHwRegHandler               =   GetHwReg88XX;
    pHalFunc->SetMACIDSleepHandler          =   SetMACIDSleep_8812FS;
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
    pHalFunc->PHYQueryRFRegHandler          = PHY_QueryRFReg_8822; //config_phydm_read_rf_reg_8812F;
    pHalFunc->IsBBRegRangeHandler           = IsBBRegRange88XX_V1;
    pHalFunc->PHYSetSecCCATHbyRXANT         = PHY_Set_SecCCATH_by_RXANT_8812F;
    pHalFunc->PHYSpurCalibration            = phy_SpurCalibration_8812F;


    //
    // Firmware CMD IO related
    //
    pHalData->H2CBufPtr88XX     = 0;
    pHalData->bFWReady          = _FALSE;
    // TODO: code below should be sync with new 3081 FW
    pHalFunc->fill_h2c_cmd_handler             = FillH2CCmd88XX;
    pHalFunc->UpdateHalRAMaskHandler        = UpdateHalRAMask8814A;
    pHalFunc->UpdateHalMSRRPTHandler        = UpdateHalMSRRPT_8812FS;
#ifdef SDIO_AP_OFFLOAD
#if defined(SOFTAP_PS_DURATION) || defined(CONFIG_POWER_SAVE)
	pHalFunc->SetSAPPsHandler				= SetSAPPS88XX;
#endif
    pHalFunc->SetAPOffloadHandler           = SetAPOffload88XX;
#endif
#ifdef AP_PS_Offlaod
    pHalFunc->SetAPPSOffloadHandler         = SetAPPSOffload88XX;
    pHalFunc->APPSOffloadMACIDPauseHandler  = APPSOffloadMacidPauseCtrl88XX;
#endif    
    pHalFunc->SetRsvdPageHandler	        = SetRsvdPage88XX;
    pHalFunc->GetRsvdPageLocHandler	        = GetRsvdPageLoc88XX;
    pHalFunc->DownloadRsvdPageHandler	    = HalGeneralDummy;
    pHalFunc->C2HHandler                    = HalGeneralDummy;
    pHalFunc->C2HPacketHandler              = C2HPacket88XX;    
  
#if CFG_HAL_HW_FILL_MACID
    pHalFunc->SetCRC5ToRPTBufferHandler     = SetCRC5ToRPTBuffer88XX;        
#endif //#if CFG_HAL_HW_FILL_MACID

    return  RT_STATUS_SUCCESS;    
}


void 
InitMAC8812FE(
    IN  HAL_PADAPTER Adapter
)
{


    
}

#if (BEAMFORMING_SUPPORT == 1)
#if 0
VOID
halTxbf8812F_GetBeamformCap(
	struct rtl8192cd_priv *priv
	)
{
	PRT_BEAMFORMING_INFO 	pBeamformingInfo = &(priv->pshare->BeamformingInfo);
	BEAMFORMING_CAP		BeamformCap = BEAMFORMING_CAP_NONE;

	BeamformCap = Beamforming_GetBeamCap(pBeamformingInfo);

	pBeamformingInfo->BeamformCap = BeamformCap;
	
}

u1Byte
halTxbf8812F_GetNtx(
	struct rtl8192cd_priv *priv
	)
{
	u1Byte			Ntx = 0;

	if(get_rf_mimo_mode(priv) == RF_4T4R)
		Ntx = 3;
	else if(get_rf_mimo_mode(priv) == RF_3T3R)
		Ntx = 2;
	else if(get_rf_mimo_mode(priv) == RF_2T4R)
		Ntx = 1;
	else if(get_rf_mimo_mode(priv) == RF_2T2R)
		Ntx = 1;
	else
		Ntx = 1;

	return Ntx;

}

u1Byte
halTxbf8812F_GetNrx(
	struct rtl8192cd_priv *priv
	)
{
	u1Byte			Nrx = 0;

	if(get_rf_mimo_mode(priv) == RF_4T4R)
		Nrx = 3;
	else if(get_rf_mimo_mode(priv) == RF_3T3R)
		Nrx = 2;
	else if(get_rf_mimo_mode(priv) == RF_2T4R)
		Nrx = 3;
	else if(get_rf_mimo_mode(priv) == RF_2T2R)
		Nrx = 1;
	else if(get_rf_mimo_mode(priv) == RF_1T2R)
		Nrx = 1;
	else
		Nrx = 0;

	return Nrx;
	
}


VOID
SetBeamformRfMode8812F(
	struct rtl8192cd_priv *priv,
	PRT_BEAMFORMING_INFO 	pBeamformingInfo,
	u1Byte					idx
	)
{
	u1Byte					i, Nr_index = 0;
	BOOLEAN					bSelfBeamformer = FALSE;
	RT_BEAMFORMING_ENTRY	BeamformeeEntry;
	BEAMFORMING_CAP		BeamformCap = BEAMFORMING_CAP_NONE;

	if (idx < BEAMFORMEE_ENTRY_NUM)
		BeamformeeEntry = pBeamformingInfo->BeamformeeEntry[idx];
	else
		return;
	
	BeamformCap = Beamforming_GetBeamCap(pBeamformingInfo);

	Nr_index = TxBF_Nr(halTxbf8812F_GetNtx(priv), BeamformeeEntry.CompSteeringNumofBFer);
	
	pBeamformingInfo->BeamformCap = BeamformCap;

	if (get_rf_mimo_mode(priv) == RF_1T1R)
		return;

	bSelfBeamformer = BeamformeeEntry.BeamformEntryCap & (BEAMFORMEE_CAP_HT_EXPLICIT|BEAMFORMEE_CAP_VHT_SU);

	for (i= RF_PATH_A; i < MAX_RF_PATH; i++) {
		PHY_SetRFReg(priv, i, RF_WE_LUT, 0x80000,0x1); // RF Mode table write enable
	}
	
	if (bSelfBeamformer) {	
		for (i= RF_PATH_A; i < MAX_RF_PATH; i++) {
			PHY_SetRFReg(priv, i, RF_RCK_OS, 0xfffff,0x18000); // Select Rx mode
			PHY_SetRFReg(priv, i, RF_TXPA_G1, 0xfffff,0xBE77F); // Set Table data
			PHY_SetRFReg(priv, i, RF_TXPA_G2, 0xfffff,0x226BF); // Enable TXIQGEN in Rx mode
		}
		PHY_SetRFReg(priv, RF_PATH_A, RF_TXPA_G2, 0xfffff,0xE26BF); // Enable TXIQGEN in Rx mode

		for (i= RF_PATH_A; i < MAX_RF_PATH; i++) {
			PHY_SetRFReg(priv, i, RF_RCK_OS, 0xfffff,0x8000);
			/*Select Standby mode*/
			PHY_SetRFReg(priv, i, RF_TXPA_G1, 0xfffff,0xFE441);
			/*Set Table data*/
			PHY_SetRFReg(priv, i, RF_TXPA_G2, 0xfffff,0xE20BF);
			/*Enable TXIQGEN in Standby mode*/
		}
	}

	for (i= RF_PATH_A; i < MAX_RF_PATH; i++) {
		PHY_SetRFReg(priv, i, RF_WE_LUT, 0x80000,0x0); // RF Mode table write disable
	}
	
	
	if(bSelfBeamformer) {			// for 8814 19ac(idx 1), 19b4(idx 0), different Tx ant setting
		PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF1, BIT28|BIT29, 0x2);// enable BB TxBF ant mapping register		
		if(idx == 0) {
			switch(Nr_index) {			
				case 0:	
				break;

				case 1:		// Nsts = 2	BC
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF0, BIT3|BIT2|BIT1|BIT0, 0x6);	//1ss
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF0, BIT7|BIT6|BIT5|BIT4, 0x6);	//2ss
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF0, 0x0000ff00, 0x10);			//BC
				PHY_SetBBReg(priv, REG_BB_TX_PATH_SEL_1, BIT23|BIT22|BIT21|BIT20, 0x6);	/*set TxPath selection for 8812F BFer bug refine*/
				PHY_SetBBReg(priv, REG_BB_TX_PATH_SEL_1, 0xff000000, 0x10);				/*if Bfer enable, always use 3Tx for all Spatial stream*/
				PHY_SetBBReg(priv, REG_BB_TX_PATH_SEL_2, 0x0000ffff, 0x1060);
				break;

				case 2:		//Nsts = 3	BCD
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF0, BIT3|BIT2|BIT1|BIT0, 0xe);	//1ss
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF0, BIT7|BIT6|BIT5|BIT4, 0xe);	//2ss
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF0, 0x0000ff00, 0x90);			//BCD
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF0, BIT19|BIT18|BIT17|BIT16, 0xe);	//3ss
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF0, 0xff00000, 0x90);			//bcd
				PHY_SetBBReg(priv, REG_BB_TX_PATH_SEL_1, BIT23|BIT22|BIT21|BIT20, 0xe);	/*set TxPath selection for 8812F BFer bug refine*/
				PHY_SetBBReg(priv, REG_BB_TX_PATH_SEL_1, 0xff000000, 0x90);				/*if Bfer enable, always use 3Tx for all Spatial stream*/
				PHY_SetBBReg(priv, REG_BB_TX_PATH_SEL_2, 0xffffffff, 0x90e90e0);
				break;
			
				default:		//Nr>3, same as Case 3
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF0, BIT3|BIT2|BIT1|BIT0, 0xf);	//1ss
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF0, BIT7|BIT6|BIT5|BIT4, 0xf);	//2ss
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF0, 0x0000ff00, 0x93);			//aBCd
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF0, BIT19|BIT18|BIT17|BIT16, 0xf);	//4ss
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF0, 0xff00000, 0x93);			//abcd
				PHY_SetBBReg(priv, REG_BB_TX_PATH_SEL_1, BIT23|BIT22|BIT21|BIT20, 0xf);	/*set TxPath selection for 8812F BFer bug refine*/
				PHY_SetBBReg(priv, REG_BB_TX_PATH_SEL_1, 0xff000000, 0x93);				/*if Bfer enable, always use 3Tx for all Spatial stream*/
				PHY_SetBBReg(priv, REG_BB_TX_PATH_SEL_2, 0xffffffff, 0x93f93f0);
				break;
			}
		} else {		// IDX =1
			switch(Nr_index) {
				case 0:	
				break;

				case 1:		// Nsts = 2	BC
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF1, BIT3|BIT2|BIT1|BIT0, 0x6);	//1ss
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF1, BIT7|BIT6|BIT5|BIT4, 0x6);	//2ss
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF1, 0x0000ff00, 0x10);			//BC
				PHY_SetBBReg(priv, REG_BB_TX_PATH_SEL_1, BIT23|BIT22|BIT21|BIT20, 0x6);	/*set TxPath selection for 8812F BFer bug refine*/
				PHY_SetBBReg(priv, REG_BB_TX_PATH_SEL_1, 0xff000000, 0x10);				/*if Bfer enable, always use 3Tx for all Spatial stream*/
				PHY_SetBBReg(priv, REG_BB_TX_PATH_SEL_2, 0x0000ffff, 0x1060);
				break;

				case 2:		//Nsts = 3	BCD
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF1, BIT3|BIT2|BIT1|BIT0, 0xe);	//1ss
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF1, BIT7|BIT6|BIT5|BIT4, 0xe);	//2ss
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF1, 0x0000ff00, 0x90);			//BC
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF1, BIT19|BIT18|BIT17|BIT16, 0xe);	//3ss
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF1, 0xff00000, 0x90);			//bcd
				PHY_SetBBReg(priv, REG_BB_TX_PATH_SEL_1, BIT23|BIT22|BIT21|BIT20, 0xe);	/*set TxPath selection for 8812F BFer bug refine*/
				PHY_SetBBReg(priv, REG_BB_TX_PATH_SEL_1, 0xff000000, 0x90);				/*if Bfer enable, always use 3Tx for all Spatial stream*/
				PHY_SetBBReg(priv, REG_BB_TX_PATH_SEL_2, 0xffffffff, 0x90e90e0);
				break;
			
				default:		//Nr>3, same as Case 3
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF1, BIT3|BIT2|BIT1|BIT0, 0xf);	//1ss
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF1, BIT7|BIT6|BIT5|BIT4, 0xf);	//2ss
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF1, 0x0000ff00, 0x93);			//BC
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF1, BIT19|BIT18|BIT17|BIT16, 0xf);	//3ss
				PHY_SetBBReg(priv, REG_BB_TXBF_ANT_SET_BF1, 0xff00000, 0x93);			//bcd
				PHY_SetBBReg(priv, REG_BB_TX_PATH_SEL_1, BIT23|BIT22|BIT21|BIT20, 0xf);	/*set TxPath selection for 8812F BFer bug refine*/
				PHY_SetBBReg(priv, REG_BB_TX_PATH_SEL_1, 0xff000000, 0x93);				/*if Bfer enable, always use 3Tx for all Spatial stream*/
				PHY_SetBBReg(priv, REG_BB_TX_PATH_SEL_2, 0xffffffff, 0x93f93f0);
				break;
	
			}
		}
		
	}


}
VOID
SetBeamformEnter8812F(
	struct rtl8192cd_priv *priv,
	u1Byte				BFerBFeeIdx
	)
{
	u1Byte					i = 0;
	u1Byte					BFerIdx = (BFerBFeeIdx & 0xF0)>>4;
	u1Byte					BFeeIdx = (BFerBFeeIdx & 0xF);
	u2Byte					CSI_Param = 0;
	PRT_BEAMFORMING_INFO 	pBeamformingInfo = &(priv->pshare->BeamformingInfo);
	RT_BEAMFORMING_ENTRY	BeamformeeEntry;
	RT_BEAMFORMER_ENTRY	BeamformerEntry;
	u2Byte					STAid = 0;
	u1Byte					Nc_index = 0,Nr_index = 0, grouping = 0, codebookinfo = 0, coefficientsize = 0;

	halTxbf8812F_GetBeamformCap(priv);

	//I am Bfee
	if ((pBeamformingInfo->BeamformCap & BEAMFORMEE_CAP) && (BFerIdx < BEAMFORMER_ENTRY_NUM)) {
		BeamformerEntry = pBeamformingInfo->BeamformerEntry[BFerIdx];
	
		RTL_W8(REG_SND_PTCL_CTRL, 0xDB);	

		// MAC addresss/Partial AID of Beamformer
		if (BFerIdx == 0) {
			for (i = 0; i < 6 ; i++)
				RTL_W8( (REG_ASSOCIATED_BFMER0_INFO+i), BeamformerEntry.MacAddr[i]);
		} else {
			for (i = 0; i < 6 ; i++)
				RTL_W8( (REG_ASSOCIATED_BFMER1_INFO+i), BeamformerEntry.MacAddr[i]);
		}

		// CSI report parameters of Beamformer

		Nc_index = halTxbf8812F_GetNrx(priv);	// for 8812F Nrx = 3(4 Ant), min=0(1 Ant)
		Nr_index = BeamformerEntry.NumofSoundingDim;	//0x718[7] = 1 use Nsts, 0x718[7] = 0 use reg setting. as Bfee, we use Nsts, so Nr_index don't care
		grouping = 0;

		//// for ac = 1, for n = 3
		if(BeamformerEntry.BeamformEntryCap & BEAMFORMEE_CAP_VHT_SU)
			codebookinfo = 1;	
		else if(BeamformerEntry.BeamformEntryCap & BEAMFORMEE_CAP_HT_EXPLICIT)
			codebookinfo = 3;	

		coefficientsize = 3;

		CSI_Param =(u2Byte)((coefficientsize<<10)|(codebookinfo<<8)|(grouping<<6)|(Nr_index<<3)|(Nc_index));

		if (BFerIdx == 0) {
			RTL_W16(REG_TX_CSI_RPT_PARAM_BW20, CSI_Param);
		} else {
			RTL_W16(REG_TX_CSI_RPT_PARAM_BW20+2, CSI_Param);
		}

		// Timeout value for MAC to leave NDP_RX_standby_state 60 us
		RTL_W8( REG_SND_PTCL_CTRL+3, 0x70);				//ndp_rx_standby_timer, 8814 need > 0x56, suggest from Dvaid
	}

	//I am Bfer
	if((pBeamformingInfo->BeamformCap & BEAMFORMER_CAP) && (BFeeIdx < BEAMFORMEE_ENTRY_NUM)) {	

		BeamformeeEntry = pBeamformingInfo->BeamformeeEntry[BFeeIdx];
		SetBeamformRfMode8812F(priv, pBeamformingInfo, BFeeIdx);	

		if(OPMODE & WIFI_ADHOC_STATE)
			STAid = BeamformeeEntry.AID;
		else 
			STAid = BeamformeeEntry.P_AID;

		// P_AID of Beamformee & enable NDPA transmission
		if(BFeeIdx == 0) {	
			RTL_W16( REG_TXBF_CTRL, STAid);	
			RTL_W8( REG_TXBF_CTRL+3, RTL_R8( REG_TXBF_CTRL+3)|BIT6|BIT7|BIT4);
		} else {
			RTL_W16( REG_TXBF_CTRL+2, STAid |BIT14| BIT15|BIT12);
		}	

		// CSI report parameters of Beamformee
		if(BFeeIdx == 0) {
			// Get BIT24 & BIT25
			u1Byte	tmp = RTL_R8( REG_ASSOCIATED_BFMEE_SEL+3) & 0x3;	
			RTL_W8( REG_ASSOCIATED_BFMEE_SEL+3, tmp | 0x60);
			RTL_W16( REG_ASSOCIATED_BFMEE_SEL, STAid | BIT9);
		} else {
			// Set BIT25
			RTL_W16( REG_ASSOCIATED_BFMEE_SEL+2, STAid | 0xE200);
		}

		RTL_W16( REG_TXBF_CTRL, RTL_R16(REG_TXBF_CTRL)|BIT15);	// disable NDP/NDPA packet use beamforming 

		Beamforming_Notify(priv);
	}
	
}


VOID
SetBeamformLeave8812F(
	struct rtl8192cd_priv *priv,
	u1Byte				Idx
	)
{
	PRT_BEAMFORMING_INFO 	pBeamformingInfo = &(priv->pshare->BeamformingInfo);
	RT_BEAMFORMING_ENTRY	BeamformeeEntry;
	RT_BEAMFORMER_ENTRY	BeamformerEntry;

	if (Idx < BEAMFORMER_ENTRY_NUM) {
		BeamformeeEntry = pBeamformingInfo->BeamformeeEntry[Idx];
		BeamformerEntry = pBeamformingInfo->BeamformerEntry[Idx];
	} else
		return;
	
	/*	Clear P_AID of Beamformee
	* 	Clear MAC addresss of Beamformer
	*	Clear Associated Bfmee Sel
	*/
	if (BeamformeeEntry.BeamformEntryCap == BEAMFORMING_CAP_NONE) {
		if(Idx == 0) {
			RTL_W16( REG_TXBF_CTRL, 0|BIT15);
			RTL_W16( REG_ASSOCIATED_BFMEE_SEL, 0);
		} else {
			RTL_W16(REG_TXBF_CTRL+2, RTL_R16( REG_TXBF_CTRL+2) & 0xF000);
			RTL_W16(REG_ASSOCIATED_BFMEE_SEL+2,	RTL_R16( REG_ASSOCIATED_BFMEE_SEL+2) & 0x60);
		}	
	}
	
	if (BeamformerEntry.BeamformEntryCap == BEAMFORMING_CAP_NONE) {
		if(Idx == 0) {
			RTL_W32( REG_ASSOCIATED_BFMER0_INFO, 0);
			RTL_W16( REG_ASSOCIATED_BFMER0_INFO+4, 0);
			RTL_W16( REG_TX_CSI_RPT_PARAM_BW20, 0);
		} else {
			RTL_W32( REG_ASSOCIATED_BFMER1_INFO, 0);
			RTL_W16( REG_ASSOCIATED_BFMER1_INFO+4, 0);
			RTL_W16( REG_TX_CSI_RPT_PARAM_BW20+2, 0);
		}	
	}

	if (((pBeamformingInfo->BeamformerEntry[0]).BeamformEntryCap == BEAMFORMING_CAP_NONE)
		&& ((pBeamformingInfo->BeamformerEntry[1]).BeamformEntryCap == BEAMFORMING_CAP_NONE))
			RTL_W8( REG_SND_PTCL_CTRL, 0xD8);

}
VOID
SetBeamformStatus8812F(
	struct rtl8192cd_priv *priv,
	 u1Byte				Idx
	)
{
	u2Byte					BeamCtrlVal;
	u4Byte					BeamCtrlReg;
	PRT_BEAMFORMING_INFO pBeamformingInfo = &(priv->pshare->BeamformingInfo);
	RT_BEAMFORMING_ENTRY	BeamformEntry;

	if (Idx < BEAMFORMEE_ENTRY_NUM)
		BeamformEntry = pBeamformingInfo->BeamformeeEntry[Idx];
	
	if (OPMODE & WIFI_ADHOC_STATE)
		BeamCtrlVal = BeamformEntry.MacId;
	else 
		BeamCtrlVal = BeamformEntry.P_AID;
	if (Idx == 0)
		BeamCtrlReg = REG_TXBF_CTRL;
	else {
		BeamCtrlReg = REG_TXBF_CTRL+2;
		BeamCtrlVal |= BIT12|BIT14|BIT15;
	}
	if((BeamformEntry.BeamformEntryState == BEAMFORMING_ENTRY_STATE_PROGRESSED) && (priv->pshare->rf_ft_var.applyVmatrix)) {
		if (BeamformEntry.BW == CHANNEL_WIDTH_20)
			BeamCtrlVal |= BIT9;
		else if (BeamformEntry.BW == CHANNEL_WIDTH_40)
			BeamCtrlVal |= (BIT9 | BIT10);
		else if (BeamformEntry.BW == CHANNEL_WIDTH_80)
			BeamCtrlVal |= (BIT9 | BIT10 | BIT11);
	} else {
		BeamCtrlVal &= ~(BIT9 | BIT10 | BIT11);
	}
	RTL_W16(BeamCtrlReg, BeamCtrlVal);
	RTL_W16( REG_TXBF_CTRL, RTL_R16(REG_TXBF_CTRL)|BIT15);	// disable NDP/NDPA packet use beamforming 

}
//2REG_C2HEVT_CLEAR
#define		C2H_EVT_HOST_CLOSE			0x00	// Set by driver and notify FW that the driver has read the C2H command message
#define		C2H_EVT_FW_CLOSE			0xFF	// Set by FW indicating that FW had set the C2H command message and it's not yet read by driver.



VOID Beamforming_NDPARate_8812F(
	struct rtl8192cd_priv *priv,
	BOOLEAN		Mode,
	u1Byte		BW,
	u1Byte		Rate)
{

	u2Byte	NDPARate = Rate;

	if(NDPARate == 0) {
		if(priv->pshare->rssi_min > 30) // link RSSI > 30%
			NDPARate = 0x8;				//MGN_24M, TxDesc = 0x08
		else
			NDPARate = 0x4;				//MGN_6M, TxDesc = 0x4
	}

	if(NDPARate < ODM_MGN_MCS0)
		BW = CHANNEL_WIDTH_20;	

	RTL_W8(REG_NDPA_OPT_CTRL, BW & 0x3);
	RTL_W8(REG_NDPA_RATE, NDPARate);

}

VOID
C2HTxBeamformingHandler_8812F(
	struct rtl8192cd_priv *priv,
		pu1Byte			CmdBuf,
		u1Byte			CmdLen
)
{
	u1Byte 	status = CmdBuf[0] & BIT0;
	Beamforming_End(priv, status);
}

VOID HW_VAR_HW_REG_TIMER_START_8812F(struct rtl8192cd_priv *priv)
{
	RTL_W8(0x15F, 0x0);
	RTL_W8(0x15F, 0x5);

}

VOID HW_VAR_HW_REG_TIMER_INIT_8812F(struct rtl8192cd_priv *priv, int t)
{
	RTL_W8(0x164, 1);
	RTL_W16(0x15C, t);

}

VOID HW_VAR_HW_REG_TIMER_STOP_8812F(struct rtl8192cd_priv *priv)
{
	RTL_W8(0x15F, 0);
}
#endif
#endif 


