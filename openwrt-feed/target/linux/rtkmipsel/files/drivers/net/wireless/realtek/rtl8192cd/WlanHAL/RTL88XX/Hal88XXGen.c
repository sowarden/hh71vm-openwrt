/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	Hal88XXGen.c

Abstract:
	Defined RTL88XX HAL common Function

Major Change History:
	When       Who               What
	---------- ---------------   -------------------------------
	2012-03-23 Filen            Create.
--*/

#if !defined(__ECOS) && !defined(CPTCFG_CFG80211_MODULE)
#include "HalPrecomp.h"
#else
#include "../HalPrecomp.h"
#endif

#ifdef USE_DMA_ALLOCATE
#define os_dma_alloc(priv, size, dma)		dma_alloc_coherent(priv->pshare->cma_dev, size, dma, GFP_KERNEL)
#define os_dma_free(priv, size, cpu, dma)	dma_free_coherent(priv->pshare->cma_dev, size, cpu, dma)
#else
#define os_dma_alloc(priv, size, dma)		pci_alloc_consistent(priv->pshare->pdev, size, dma)
#define os_dma_free(priv, size, cpu, dma)	pci_free_consistent(priv->pshare->pdev, size, cpu, dma)
#endif

enum rf_type
GetChipIDMIMO88XX(
	IN  HAL_PADAPTER Adapter
)
{
	u4Byte      value32;

#if IS_RTL8814A_SERIES
	if (GET_CHIP_VER(Adapter) == VERSION_8814A) {
		return RF_3T3R;
		//return RF_4T4R;
	}
#endif

#if IS_RTL8198F_SERIES || IS_RTL8814B_SERIES
	if ((GET_CHIP_VER(Adapter) == VERSION_8198F) || (GET_CHIP_VER(Adapter) == VERSION_8814B)) {
		return RF_4T4R;
	}
#endif
	value32 = platform_efio_read_4byte(Adapter, REG_SYS_CFG1);

	if ( value32 & BIT27 ) {
		return RF_2T2R;
	} else {
		return RF_1T1R;
	}
}

VOID
CAMEmptyEntry88XX(
	IN  HAL_PADAPTER    Adapter,
	IN  u1Byte          index
)
{
	u4Byte  command = 0, content = 0;
	u4Byte  i;

	for (i = 0; i < HAL_CAM_CONTENT_COUNT; i++) {
		// polling bit, and No Write enable, and address
		command = HAL_CAM_CONTENT_COUNT * index + i;
		command = command | BIT_SECCAM_POLLING | BIT_SECCAM_WE;
		// write content 0 is equal to mark invalid
		HAL_RTL_W32(REG_CAMWRITE, content);
		HAL_RTL_W32(REG_CAMCMD, command);
	}
}

u4Byte
CAMFindUsable88XX(
	IN  HAL_PADAPTER    Adapter,
	IN  u4Byte          for_begin
)
{
	u4Byte command = 0, content = 0, round = 0;
	u4Byte index;

	for (index = for_begin; index < HAL_TOTAL_CAM_ENTRY(Adapter); index++) {
		// polling bit, and No Write enable, and address
		command = HAL_CAM_CONTENT_COUNT * index;
		HAL_RTL_W32(REG_CAMCMD, (BIT_SECCAM_POLLING | command));

		// Check polling bit is clear
		while (1) {
			command = HAL_RTL_R32(REG_CAMCMD);
			if (!(command & BIT_SECCAM_POLLING))
				break;

			if (++round > 10000) {
				panic_printk("%s[%d] while (1) goes too many\n", __FUNCTION__, __LINE__);
				break;
			}
		}
		content = HAL_RTL_R32(REG_CAMREAD);

		// check valid bit. if not valid,
		if ((content & BIT15) == 0) {
			return index;
		}
	}

	return HAL_TOTAL_CAM_ENTRY(Adapter);
}


VOID
CAMReadMACConfig88XX
(
	IN  HAL_PADAPTER    Adapter,
	IN  u1Byte          index,
	OUT pu1Byte         pMacad,
	OUT PCAM_ENTRY_CFG  pCfg
)
{
	u4Byte  command = 0, content = 0, round;
	u2Byte  TempConfig;

	// polling bit, and No Write enable, and address
	// cam address...
	// first 32-bit
	command = HAL_CAM_CONTENT_COUNT * index + 0;
	command = command | BIT_SECCAM_POLLING;
	HAL_RTL_W32(REG_CAMCMD, command);

	//Check polling bit is clear
	round = 0;
	while (1) {
#ifdef CONFIG_SDIO_HCI
		{
			s32 err;
			command = sdio_read32(Adapter, REG_CAMCMD, &err);
			if (-ENOMEDIUM == err)
				return;
		}
#else
		command = HAL_RTL_R32(REG_CAMCMD);
#endif
		if (!(command & BIT_SECCAM_POLLING))
			break;

		if (++round > 10000) {
			panic_printk("%s[%d] while (1) goes too many\n", __FUNCTION__, __LINE__);
			break;
		}
	}

	content = HAL_RTL_R32(REG_CAMREAD);

	//first 32-bit is MAC address and CFG field
	*(pMacad + 0) = (u1Byte)((content >> 16) & 0x000000FF);
	*(pMacad + 1) = (u1Byte)((content >> 24) & 0x000000FF);

	TempConfig      = (u2Byte)(content & 0x0000FFFF);
	pCfg->bValid    = (TempConfig & BIT15) ? _TRUE : _FALSE;
	pCfg->KeyID     = TempConfig & 0x3;
	pCfg->EncAlgo   = (TempConfig & 0x1c) >> 2;

	command = HAL_CAM_CONTENT_COUNT * index + 1;
	command = command | BIT_SECCAM_POLLING;
	HAL_RTL_W32(REG_CAMCMD, command);

	//Check polling bit is clear
	round = 0;
	while (1) {
#ifdef CONFIG_SDIO_HCI
		{
			s32 err;
			command = sdio_read32(Adapter, REG_CAMCMD, &err);
			if (-ENOMEDIUM == err)
				return;
		}
#else
		command = HAL_RTL_R32(REG_CAMCMD);
#endif
		if (command & BIT_SECCAM_POLLING)
			continue;
		else
			break;

		if (++round > 10000) {
			panic_printk("%s[%d] while (1) goes too many\n", __FUNCTION__, __LINE__);
			break;
		}
	}
	content = HAL_RTL_R32(REG_CAMREAD);

	*(pMacad + 5) = (u1Byte)((content >> 24) & 0x000000FF);
	*(pMacad + 4) = (u1Byte)((content >> 16) & 0x000000FF);
	*(pMacad + 3) = (u1Byte)((content >> 8) & 0x000000FF);
	*(pMacad + 2) = (u1Byte)((content) & 0x000000FF);
}


VOID
CAMProgramEntry88XX(
	IN	HAL_PADAPTER		Adapter,
	IN  u1Byte              index,
	IN  pu1Byte             macad,
	IN  pu1Byte             key128,
	IN  u2Byte              config
)
{
	u4Byte  target_command = 0, target_content = 0;
	s1Byte  entry_i = 0;

	for (entry_i = (HAL_CAM_CONTENT_USABLE_COUNT - 1); entry_i >= 0; entry_i--) {
		// polling bit, and write enable, and address
		target_command = entry_i + HAL_CAM_CONTENT_COUNT * index;
		target_command = target_command | BIT_SECCAM_POLLING | BIT_SECCAM_WE;
		if (entry_i == 0) {
			//first 32-bit is MAC address and CFG field
			target_content = (u4Byte)(*(macad + 0)) << 16
							 | (u4Byte)(*(macad + 1)) << 24
							 | (u4Byte)config;
			target_content = target_content | config;
		} else if (entry_i == 1) {
			//second 32-bit is MAC address
			target_content = (u4Byte)(*(macad + 5)) << 24
							 | (u4Byte)(*(macad + 4)) << 16
							 | (u4Byte)(*(macad + 3)) << 8
							 | (u4Byte)(*(macad + 2));
		} else {
			target_content = (u4Byte)(*(key128 + (entry_i * 4 - 8) + 3)) << 24
							 | (u4Byte)(*(key128 + (entry_i * 4 - 8) + 2)) << 16
							 | (u4Byte)(*(key128 + (entry_i * 4 - 8) + 1)) << 8
							 | (u4Byte)(*(key128 + (entry_i * 4 - 8) + 0));
		}

		HAL_RTL_W32(REG_CAMWRITE, target_content);
		HAL_RTL_W32(REG_CAMCMD, target_command);
	}

	target_content = HAL_RTL_R32(REG_CR);
	if ((target_content & BIT_MAC_SEC_EN) == 0) {
		HAL_RTL_W32(REG_CR, (target_content | BIT_MAC_SEC_EN));
	}
}

VOID
CAMProgramEntry88XX_V1(
	IN	HAL_PADAPTER		Adapter,
	IN  u1Byte              index,
	IN  pu1Byte             macad,
	IN  pu1Byte             key128,
	IN  u2Byte              config
)
{
	u4Byte  target_command = 0, target_content = 0;
	s1Byte  entry_i = 0;

	// In 8192F,8198F,8197G fill CAM entry must follow the sequence
	// Entry8 => Entry7 => Enrty6 => ... => Entry0 => Entry1
	// Entry1 must fill after Entry0
	
	for (entry_i = (HAL_CAM_CONTENT_USABLE_COUNT - 1); entry_i >= 0; entry_i--) {
		// polling bit, and write enable, and address
		target_command = entry_i + HAL_CAM_CONTENT_COUNT * index;
		target_command = target_command | BIT_SECCAM_POLLING | BIT_SECCAM_WE;
		if (entry_i == 0) {
			//first 32-bit is MAC address and CFG field
			target_content = (u4Byte)(*(macad + 0)) << 16
							 | (u4Byte)(*(macad + 1)) << 24
							 | (u4Byte)config;
			target_content = target_content | config;

			HAL_RTL_W32(REG_CAMWRITE, target_content);
			HAL_RTL_W32(REG_CAMCMD, target_command);

			// Fill security entry one
			target_command = 1 + HAL_CAM_CONTENT_COUNT * index;
			target_command = target_command | BIT_SECCAM_POLLING | BIT_SECCAM_WE;
			//second 32-bit is MAC address
			target_content = (u4Byte)(*(macad + 5)) << 24
							 | (u4Byte)(*(macad + 4)) << 16
							 | (u4Byte)(*(macad + 3)) << 8
							 | (u4Byte)(*(macad + 2));

			HAL_RTL_W32(REG_CAMWRITE, target_content);
			HAL_RTL_W32(REG_CAMCMD, target_command);
		} else if (entry_i == 1) {
			continue;
		} else {
			target_content = (u4Byte)(*(key128 + (entry_i * 4 - 8) + 3)) << 24
							 | (u4Byte)(*(key128 + (entry_i * 4 - 8) + 2)) << 16
							 | (u4Byte)(*(key128 + (entry_i * 4 - 8) + 1)) << 8
							 | (u4Byte)(*(key128 + (entry_i * 4 - 8) + 0));
			HAL_RTL_W32(REG_CAMWRITE, target_content);
			HAL_RTL_W32(REG_CAMCMD, target_command);
		}
	}

	target_content = HAL_RTL_R32(REG_CR);
	if ((target_content & BIT_MAC_SEC_EN) == 0) {
		HAL_RTL_W32(REG_CR, (target_content | BIT_MAC_SEC_EN));
	}
}


#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV && IS_RTL88XX_MAC_V4
u1Byte FindFreeSmhCam88XX(struct rtl8192cd_priv *priv,struct stat_info *pstat)
{

    u1Byte macid = REMAP_AID(pstat);
    u1Byte find_i;
    u1Byte used_bit;
        
    for(find_i = 0;find_i<64;find_i++){ //64 is hw entry num
        used_bit = priv->pshare->SmhInfo[find_i].used;
        if(used_bit & 0x1){
            //used;
        }else{
            priv->pshare->SmhInfo[find_i].used = 1;
            priv->pshare->SmhInfo[find_i].macid = macid;
            ClearHWTXShortcutBufHandler88XX(priv,find_i);
            return find_i;
        }
    }
    return SMHCAM_IS_FULL;
}
u1Byte GetSmhCam88XX(struct rtl8192cd_priv *priv,struct stat_info *pstat)
{

    u1Byte macid = REMAP_AID(pstat);
    u1Byte find_i;
    
    for(find_i = 0;find_i<64;find_i++){ //64 is hw entry num
        if(macid == priv->pshare->SmhInfo[find_i].macid)
            return find_i;
    }
    return SMHCAM_SEARCH_FAIL;
}

#endif

VOID
SetHwReg88XX(
	IN	HAL_PADAPTER		Adapter,
	IN	u1Byte				variable,
	IN	pu1Byte				val
)
{
#ifdef PCIE_POWER_SAVING_TEST //yllin
    if((Adapter->pwr_state == L2) || (Adapter->pwr_state == L1)) 
    {
        printk("Error, in L1/L2, dont access MAC reg = %x\n",variable);
            return;
    }
#endif


	switch (variable) {

	case HW_VAR_ETHER_ADDR: {
		u1Byte	idx = 0;
		//For Endian Free.
		for (idx = 0; idx < 6 ; idx++) {
			platform_efio_write_1byte(Adapter, (REG_MACID + idx), val[idx]);
		}

#if IS_EXIST_PCI || IS_EXIST_EMBEDDED || !defined(SDIO_2_PORT)
	#if defined(SDIO_2_PORT)
	#define IS_SDIO_2_PORT 1
	#else
	#define IS_SDIO_2_PORT 0
	#endif
		if (Adapter->hci_type == RTL_HCI_PCIE || !IS_SDIO_2_PORT) {
			// Win8: Let the device port use the locally-administered mac address -----------------------------------------
			for (idx = 0; idx < 6; idx++) {
				platform_efio_write_1byte(Adapter, (REG_MACID1 + idx), val[idx]);

				if (idx == 0) {
					platform_efio_write_1byte(Adapter, (REG_MACID1 + idx), val[idx] | BIT1);
				}
			}
			// ---------------------------------------------------------------------------------------------------
		}
#endif
	}
	break;

	case HW_VAR_MULTICAST_REG: {
		u1Byte	idx = 0;
		//For endian free.
		for (idx = 0; idx < 8 ; idx++) {
			platform_efio_write_1byte(Adapter, (REG_MAR + idx), val[idx]);
		}
	}
	break;

	case  HW_VAR_BSSID: {
		u1Byte  idx = 0;
		for (idx = 0 ; idx < 6; idx++) {
			platform_efio_write_1byte(Adapter, (REG_BSSID + idx), val[idx]);
		}
	}
	break;

	case HW_VAR_MEDIA_STATUS: {
		RT_OP_MODE	OpMode = *((RT_OP_MODE *)(val));
		u1Byte		btMsr = platform_efio_read_1byte(Adapter, REG_CR + 2);

		btMsr &= 0xfc;

		switch ( OpMode ) {
		case RT_OP_MODE_INFRASTRUCTURE:
			btMsr |= MSR_INFRA;
			break;

		case RT_OP_MODE_IBSS:
			btMsr |= MSR_ADHOC;
			break;

		case RT_OP_MODE_AP:
			btMsr |= MSR_AP;
			break;

		default:
			btMsr |= MSR_NOLINK;
			break;
		}

		platform_efio_write_1byte(Adapter, REG_CR + 2, btMsr);
	}
	break;

	case HW_VAR_MAC_LOOPBACK_ENABLE: {
		// accept all packets
		HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) | BIT_AAP);

		// enable MAC loopback
		//HAL_RTL_W32(REG_CR, HAL_RTL_R32(REG_CR) | (LBMODE_MAC_DLY & BIT_MASK_LBMODE) << BIT_SHIFT_LBMODE);
		HAL_RTL_W32(REG_CR, HAL_RTL_R32(REG_CR) | (LBMODE_MAC & BIT_MASK_LBMODE) << BIT_SHIFT_LBMODE);
	}
	break;

	case HW_VAR_MAC_CONFIG: {
		PMACCONFIG_PARA	pMacCfgPara = (MACCONFIG_PARA *)(val);
		u1Byte          tmpU1Byte;
        u4Byte          tmpRegName;
        u4Byte          bit_en_bcn_pkt_rel;
        #if IS_RTL88XX_MAC_V1_V2_V3
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3){
            bit_en_bcn_pkt_rel = BIT_EN_BCN_PKT_REL;
        }
        #endif
        #if IS_RTL88XX_MAC_V4
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
            bit_en_bcn_pkt_rel = BIT_EN_BCN_PKT_REL_P0;
        }
        #endif
		HAL_RTL_W8(REG_INIRTS_RATE_SEL, 0x8); // 24M

		// 2007/02/07 Mark by Emily becasue we have not verify whether this register works
		//For 92C,which reg?
		//	RTL_W8(BWOPMODE, BW_20M);	//	set if work at 20m

		// Ack timeout.
		if ((pMacCfgPara->AckTO > 0) && (pMacCfgPara->AckTO < 0xff)) {
			HAL_RTL_W8(REG_ACKTO, pMacCfgPara->AckTO);
		} else {
			HAL_RTL_W8(REG_ACKTO, 0x40);
		}

        /*
            * Disable TXOP CFE
            */
		HAL_RTL_W16(REG_RD_CTRL, HAL_RTL_R16(REG_RD_CTRL) | BIT10);

#if 0
		// clear for mbid beacon tx
		HAL_RTL_W8(MULTI_BCNQ_EN, 0);
		HAL_RTL_W8(MULTI_BCNQ_OFFSET, 0);
#else
		// TODO: Spec has changed, check design
#endif

		// add by Eric, set RateID table 10 for ARFR1 (1SS VHT)
		// RateID 9 is for ARFR0(2SS VHT)

#if IS_RTL88XX_MAC_V4
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
            //temp do nothing, ARFR table using method change, need confirm how to set this, yllin8814B
        }
#endif // IS_RTL88XX_MAC_V4
#if IS_RTL88XX_MAC_V1_V2_V3
#if (IS_RTL8814A_SERIES)
		if (IS_HARDWARE_TYPE_8814A(Adapter) ) {
			HAL_RTL_W32(REG_ARFR1_V1, 0x3FF010);
			HAL_RTL_W32(REG_ARFR1_V1 + 4, 0x40000000);

		} else
#endif	
		{
		HAL_RTL_W32(REG_ARFR1_V1, 0x00000015);
		HAL_RTL_W32(REG_ARFR1_V1 + 4, 0x003FF000);
		}
#endif //IS_RTL88XX_MAC_V1_V2_V3
		

		/*
		 *	RA try rate aggr limit
		 */
#if (IS_RTL8814A_SERIES)
		if ( !IS_HARDWARE_TYPE_8814A(Adapter) )
#endif		            
#if (IS_RTL8822B_SERIES)
		if ( !IS_HARDWARE_TYPE_8822B(Adapter) )
#endif		
		{
			HAL_RTL_W8(REG_RA_TRY_RATE_AGG_LMT, 2);
		}

		//3 MAC AMPDU Related	 MAC Beacon Related

        HAL_RTL_W32(REG_CCK_CHECK,  HAL_RTL_R32(REG_CCK_CHECK) & ~bit_en_bcn_pkt_rel);
        HAL_RTL_W32(REG_PKT_LIFETIME_CTRL,  HAL_RTL_R32(REG_PKT_LIFETIME_CTRL) & ~BIT(19));


		//   AMPDU MAX duration
		//  Note:
		//        the max packet length in Japan is necessary to be less than 4ms
		//        8812 unit: 8 us
		//        92E/8881A/8814A unit: 32 us
#if (IS_RTL88XX_MAC_V1)
		if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1 ) {
            HAL_RTL_W16(REG_PROT_MODE_CTRL + 2, 0x0909);
            //   AMPDU MAX duration
			HAL_RTL_W8(REG_AMPDU_MAX_TIME, 0x3F);
            
            if ((GET_MIB(Adapter))->dot11OperationEntry.opmode & WIFI_STATION_STATE)
                HAL_RTL_W8(REG_TBTT_PROHIBIT, 0x14);

            if ( RT_OP_MODE_AP == pMacCfgPara->OP_Mode ) {
                // Remove BIT_ADF because setting BIT_ADF will also accept BA and BAR
				// Then, set REG_RXFLTMAP1 and REG_RXFLTMAP to accept PS-Poll and all data frames, respectively.
				HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) & ~BIT_ADF);
				HAL_RTL_W16(REG_RXFLTMAP1, BIT_CTRLFLT10EN);
				HAL_RTL_W16(REG_RXFLTMAP, BIT_DATAFLT15EN | BIT_DATAFLT14EN | BIT_DATAFLT13EN | BIT_DATAFLT12EN |
							BIT_DATAFLT11EN | BIT_DATAFLT10EN | BIT_DATAFLT9EN | BIT_DATAFLT8EN | BIT_DATAFLT7EN |
							BIT_DATAFLT6EN | BIT_DATAFLT5EN | BIT_DATAFLT4EN | BIT_DATAFLT3EN | BIT_DATAFLT2EN |
							BIT_DATAFLT1EN | BIT_DATAFLT0EN);
            }
		}
#endif //(IS_RTL8192E_SERIES || IS_RTL8881A_SERIES)

#if (IS_RTL88XX_MAC_V2_V3_V4)
    if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3_v4) {
            // configure max tx agg num by parameter file
            if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v3 )
                HAL_RTL_W16(REG_PROT_MODE_CTRL + 2, 0x0909);
            //   AMPDU MAX duration
			HAL_RTL_W8(REG_AMPDU_MAX_TIME_V1, 0x70);
/*
            if ((GET_MIB(Adapter))->dot11OperationEntry.opmode & WIFI_STATION_STATE) {
#if (IS_RTL88XX_MAC_V2_V3)                
                if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3)
				    HAL_RTL_W32(REG_TBTT_PROHIBIT, 0x4004);
#endif
#if (IS_RTL88XX_MAC_V4) 
                if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)
                    HAL_RTL_W32(REG_TBTT_PROHIBIT_CFG, 0x4004);
#endif                
            }*/
            
            if ( RT_OP_MODE_AP == pMacCfgPara->OP_Mode ) {
                // TODO: currently, we accept all types of packet
				HAL_RTL_W16(REG_RXFLTMAP0, BIT_MGTFLT0EN | BIT_MGTFLT1EN | BIT_MGTFLT2EN | BIT_MGTFLT3EN | BIT_MGTFLT4EN | BIT_MGTFLT5EN |
							BIT_MGTFLT8EN | BIT_MGTFLT9EN | BIT_MGTFLT10EN | BIT_MGTFLT11EN | BIT_MGTFLT12EN | BIT_MGTFLT13EN);
                HAL_RTL_W16(REG_RXFLTMAP1, BIT_CTRLFLT10EN);
                #if (IS_RTL8192E_SERIES || IS_RTL8197F_SERIES || IS_RTL8198F_SERIES || IS_RTL8814A_SERIES || IS_RTL8881A_SERIES || IS_RTL8197G_SERIES)
                if(IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8814A(Adapter) || IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter))
                    tmpRegName = REG_RXFLTMAP;
                #endif
                #if (IS_RTL8814B_SERIES || IS_RTL8822B_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
                if(IS_HARDWARE_TYPE_8814B(Adapter) || IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter))
                    tmpRegName = REG_RXFLTMAP2;
                #endif
				HAL_RTL_W16(tmpRegName, BIT_DATAFLT0EN | BIT_DATAFLT1EN | BIT_DATAFLT2EN | BIT_DATAFLT3EN | BIT_DATAFLT4EN | BIT_DATAFLT5EN |
							BIT_DATAFLT6EN | BIT_DATAFLT7EN | BIT_DATAFLT8EN | BIT_DATAFLT9EN | BIT_DATAFLT10EN | BIT_DATAFLT11EN |
							BIT_DATAFLT12EN | BIT_DATAFLT13EN | BIT_DATAFLT14EN | BIT_DATAFLT15EN);
            }else{
                #if IS_RTL8814A_SERIES
                if (IS_HARDWARE_TYPE_8814AE(Adapter)) {
                    // TODO: currently, we accept all types of packet
                    HAL_RTL_W16(REG_RXFLTMAP0, BIT_MGTFLT0EN | BIT_MGTFLT1EN | BIT_MGTFLT2EN | BIT_MGTFLT3EN | BIT_MGTFLT4EN | BIT_MGTFLT5EN |
                                BIT_MGTFLT8EN | BIT_MGTFLT9EN | BIT_MGTFLT10EN | BIT_MGTFLT11EN | BIT_MGTFLT12EN | BIT_MGTFLT13EN);
                    HAL_RTL_W16(REG_RXFLTMAP, BIT_DATAFLT0EN | BIT_DATAFLT1EN | BIT_DATAFLT2EN | BIT_DATAFLT3EN | BIT_DATAFLT4EN | BIT_DATAFLT5EN |
                                BIT_DATAFLT6EN | BIT_DATAFLT7EN | BIT_DATAFLT8EN | BIT_DATAFLT9EN | BIT_DATAFLT10EN | BIT_DATAFLT11EN |
                                BIT_DATAFLT12EN | BIT_DATAFLT13EN | BIT_DATAFLT14EN | BIT_DATAFLT15EN);
                }
                #endif
            }
            
		}
#endif //#if IS_RTL88XX_MAC_V2_V3_V4

		// for Oppo IOT, set AGGR bk time to 26 us
		if(IS_HARDWARE_TYPE_8822B(Adapter)) {
			HAL_RTL_W16(REG_TX_PTCL_CTRL, HAL_RTL_R16(REG_TX_PTCL_CTRL) | BIT_SIFS_BK_EN);
			HAL_RTL_W8(REG_AGGR_BREAK_TIME, 0x1a);
		}

#if (IS_RTL88XX_MAC_V1_V2)
    if( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2 ) {
		HAL_RTL_W8(REG_DTIM_COUNTER_ROOT,   pMacCfgPara->dot11DTIMPeriod - 1);
    }
#endif //#if (IS_RTL88XX_MAC_V1_V2)

        
#if (IS_RTL88XX_MAC_V3)    
            if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v3 ) {
                // TODO : 8198F 16 MBSSID setting
                //setting root interface ATIM / DTIM / control frame counter
                HAL_RTL_W32(REG_MBID_BCNQ_EN, ((HAL_RTL_R32(REG_MBID_BCNQ_EN) & ~ BITS_ATIM_CFG_SEL) | BIT_ATIM_CFG_SEL(0)));
                HAL_RTL_W8(REG_ATIMWND_GROUP1, 0x3c);
                HAL_RTL_W8(REG_DTIM_COUNT_GROUP2, HAL_VAR_DTIM_PERIOD - 1);
                HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT0CTRL);
                HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(0));
            }
#endif 


#if IS_RTL88XX_MAC_V4
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)
        {
            if (pMacCfgPara->vap_enable)
                HAL_RTL_W32(REG_TBTT_PROHIBIT_CFG, 0x1df04);
            else
                HAL_RTL_W32(REG_TBTT_PROHIBIT_CFG, 0x40004);
            if ( (Adapter->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE)){
    	        HAL_RTL_W32(REG_TBTT_PROHIBIT_CFG, 0x104); // Station mode could reduce hold time to 1
            }
            if (pMacCfgPara->WdsPure)
                HAL_RTL_W32(REG_TBTT_PROHIBIT_CFG, 0x104);
            HAL_RTL_W8(REG_PORT_CTRL_SEL, HAL_RTL_R8(REG_PORT_CTRL_SEL) & ~BIT_MASK_PORT_CTRL_SEL);
            HAL_RTL_W8(REG_DRVERLYINT_CFG, BEACON_ERALY_INIT_TIME);        
            HAL_RTL_W32(REG_BCNDMATIM_CFG, 1);        
            HAL_RTL_W16(REG_ATIMWND_CFG,            0x3C);
#ifdef CFG_HAL_SUPPORT_MBSSID            
            if (pMacCfgPara->vap_enable && HAL_NUM_VWLAN == 1 &&
				(HAL_RTL_R16(REG_BCN_SPACE_CFG) < 30)) {
    			HAL_RTL_W8(REG_DRVERLYINT_CFG, 6);
    		}
#endif            
            HAL_RTL_W8(REG_PORT_CTRL_CFG, HAL_RTL_R8(REG_PORT_CTRL_CFG) | BIT_DIS_TSF_UDT);
            HAL_RTL_W8(REG_BCN_MAX_ERR_V1,         0xff);
            HAL_RTL_W16(REG_TSFTR_SYNC_OFFSET_CFG,0);
            HAL_RTL_W8(REG_PORT_CTRL_CFG, HAL_RTL_R8(REG_PORT_CTRL_CFG) | BIT_TSFTR_RST_V1);

            if ( RT_OP_MODE_INFRASTRUCTURE == pMacCfgPara->OP_Mode ) {
    			HAL_RTL_W16(REG_FWHW_TXQ_CTRL, HAL_RTL_R16(REG_FWHW_TXQ_CTRL) ^ (BIT_EN_BCNQ_DL0 | BIT_EN_BCNQ_DL1 | BIT_EN_BCNQ_DL2 | BIT_EN_BCNQ_DL3 | BIT_EN_BCNQ_DL4 | BIT_EN_BCNQ_DL5)); //ok?? yllin8814B
    		}

			HAL_RTL_W16(REG_CONTROL_FRAME_CNT_CTRL, BIT(8));
			HAL_RTL_W8(REG_CONTROL_FRAME_REPORT, BIT(0));
        }
#endif

#if IS_RTL88XX_MAC_V1_V2_V3
        if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3)
        {
			// for Oppo IOT, set AGGR bk time to 24 us
			if(IS_HARDWARE_TYPE_8822B(Adapter)) {
				HAL_RTL_W16(REG_TX_PTCL_CTRL, HAL_RTL_R16(REG_TX_PTCL_CTRL) | BIT_SIFS_BK_EN);
				HAL_RTL_W8(REG_AGGR_BREAK_TIME, 0x18);
			}
		
            if (pMacCfgPara->vap_enable)
                HAL_RTL_W32(REG_TBTT_PROHIBIT, 0x1df04);
            else
                HAL_RTL_W32(REG_TBTT_PROHIBIT, 0x40004);
			if ( (Adapter->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE)){
    	        HAL_RTL_W32(REG_TBTT_PROHIBIT, 0x104); // Station mode could reduce hold time to 1
            }
            if (pMacCfgPara->WdsPure)
        	    HAL_RTL_W32(REG_TBTT_PROHIBIT, 0x104);        
   		    HAL_RTL_W8(REG_DRVERLYINT,          BEACON_ERALY_INIT_TIME);
            HAL_RTL_W8(REG_BCNDMATIM,           1);
#ifdef CFG_HAL_SUPPORT_MBSSID               
            if (pMacCfgPara->vap_enable && HAL_NUM_VWLAN == 1 &&
    				(HAL_RTL_R16(REG_MBSSID_BCN_SPACE) < 30)) {
    			HAL_RTL_W8(REG_DRVERLYINT, 6);
    		}
#endif            
		    HAL_RTL_W8(REG_BCN_CTRL,            BIT_DIS_TSF_UDT);
            HAL_RTL_W8(REG_BCN_MAX_ERR,         0xff);
            HAL_RTL_W16(REG_TSFTR_SYN_OFFSET,   0);
            HAL_RTL_W8(REG_DUAL_TSF_RST,        3);

            if ( RT_OP_MODE_INFRASTRUCTURE == pMacCfgPara->OP_Mode ) {
    			HAL_RTL_W8(REG_FWHW_TXQ_CTRL + 2, HAL_RTL_R8(REG_FWHW_TXQ_CTRL + 2) ^ BIT6);
    		}
            
            
        }
#endif

#if IS_RTL88XX_MAC_V1_V2
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2)
            HAL_RTL_W16(REG_ATIMWND,            0x3C);
#endif

#if IS_RTL88XX_MAC_V4
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
            HAL_RTL_W8(REG_PORT_CTRL_SEL, HAL_RTL_R8(REG_PORT_CTRL_SEL) & ~BIT_MASK_PORT_CTRL_SEL);
            tmpRegName = REG_PORT_CTRL_CFG;
        }
#endif
#if IS_RTL88XX_MAC_V1_V2_V3
        if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3)
		    tmpRegName = REG_BCN_CTRL;
#endif	
        tmpU1Byte = HAL_RTL_R8(tmpRegName);

		if ( RT_OP_MODE_AP == pMacCfgPara->OP_Mode ) {

			//Beacon Error Interrupt happen when AP received other AP's Beacon
			if ((IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8192E(Adapter))
					&& IS_HAL_TEST_CHIP(Adapter)) {
				//Do Nothing , there is no BIT6 at that time
			} else {
				tmpU1Byte |= BIT_DIS_RX_BSSID_FIT;
			}

#if IS_RTL88XX_MAC_V2 && !IS_RTL8814A_SERIES
            if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2) {
			    tmpU1Byte |= BIT_EN_BCN_FUNCTION | BIT_P0_EN_TXBCN_RPT;
            }
#endif
#if IS_RTL88XX_MAC_V1
            if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1) {
			    tmpU1Byte |= BIT_EN_BCN_FUNCTION | BIT_EN_TXBCN_RPT;
            }
#endif
#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES )
            if (IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8881A(Adapter) ||IS_HARDWARE_TYPE_8814A(Adapter)) {
		    	tmpU1Byte |= BIT_EN_BCN_FUNCTION | BIT_EN_TXBCN_RPT;
            }
#endif  

#if (IS_RTL8197F_SERIES || IS_RTL8822B_SERIES || IS_RTL8198F_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES|| IS_RTL8197G_SERIES)
            if (IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8822B(Adapter)||IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter)) {
                tmpU1Byte |= BIT_EN_BCN_FUNCTION | BIT_P0_EN_TXBCN_RPT;
            }
#endif //#if IS_RTL8198F_SERIES

#if IS_RTL8814B_SERIES
            if (IS_HARDWARE_TYPE_8814B(Adapter)) {
                tmpU1Byte |= BIT_EN_PORT_FUNCTION | BIT_EN_TXBCN_RPT_V1;
            }
#endif
			HAL_RTL_W16(REG_BCNTCFG, 0x3213);//HAL_RTL_W16(REG_BCNTCFG, 0x000C);

#if (IS_RTL8822B_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
			if (IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter))
			{
				HAL_RTL_W32(0xc98, ((HAL_RTL_R32(0xc98)&0x80FFFFFF)| BIT(31) | (0x20<<24)));
			}
#endif

			HAL_RTL_W16(REG_BCNTCFG, 0x3213);//HAL_RTL_W16(REG_BCNTCFG, 0x000C);

#if (IS_RTL8822B_SERIES)
			if ( IS_HARDWARE_TYPE_8822B(Adapter) )
			{
				HAL_RTL_W32(0xc98, ((HAL_RTL_R32(0xc98)&0x80FFFFFF)| BIT(31) | (0x20<<24)) );
			}
#endif

			
			// Set TCR to enable mactx update DTIM count, group bit, and moreData bit
			 HAL_RTL_W8(REG_TCR, HAL_RTL_R8(REG_TCR) | BIT_WMAC_TCR_UPD_TIMIE | BIT_WMAC_TCR_UPD_HGQMD);

		} else {
			if ((IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8192E(Adapter))
					&& IS_HAL_TEST_CHIP(Adapter)) {
				//Do Nothing , there is no BIT6 at that time
			} else {
				tmpU1Byte &= ~BIT_DIS_RX_BSSID_FIT;
			}


#if IS_RTL88XX_MAC_V2 && !IS_RTL8814A_SERIES
            if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2) {
			    tmpU1Byte |= BIT_EN_BCN_FUNCTION | BIT_P0_EN_TXBCN_RPT;
            }
#endif
#if IS_RTL88XX_MAC_V1
            if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1) {
			    tmpU1Byte |= BIT_EN_BCN_FUNCTION | BIT_EN_TXBCN_RPT;
            }
#endif

#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES)
            if (IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8881A(Adapter) ||IS_HARDWARE_TYPE_8814A(Adapter)) {
		    	tmpU1Byte |= BIT_EN_BCN_FUNCTION | BIT_EN_TXBCN_RPT; 
            }
#endif     

#if (IS_RTL8197F_SERIES || IS_RTL8822B_SERIES || IS_RTL8198F_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES || IS_RTL8197G_SERIES)
            if (IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8822B(Adapter)||IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter)) {
                tmpU1Byte |= BIT_EN_BCN_FUNCTION | BIT_P0_EN_TXBCN_RPT;
            }
#endif //#if (IS_RTL8197F_SERIES || IS_RTL8822B_SERIES || IS_RTL8198F_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)

#if IS_RTL8814B_SERIES
            if (IS_HARDWARE_TYPE_8814B(Adapter)){
                tmpU1Byte |= BIT_EN_PORT_FUNCTION | BIT_EN_TXBCN_RPT_V1;
            }
#endif

			HAL_RTL_W16(REG_BCNTCFG, 0x0204);
		}

        HAL_RTL_W8(tmpRegName, tmpU1Byte);
#if IS_EXIST_SDIO
		if (Adapter->hci_type == RTL_HCI_SDIO) {
		//
		// Configure SDIO TxRx Control to enable Rx DMA timer masking.
		// 2010.02.24.
		//
		SdioLocalCmd52Write4Byte(Adapter, SDIO_REG_TX_CTRL, 0x00);

#ifdef CONFIG_SDIO_TX_INTERRUPT
		// Invalidate All TX Free Page Threshold
#if IS_EXIST_RTL8192ES
		HAL_RTL_W32(REG_92E_TQPNT1, 0xFFFFFFFF);
		HAL_RTL_W32(REG_92E_TQPNT2, 0xFFFFFFFF);
#endif
#if IS_EXIST_RTL8822BS
			HAL_RTL_W32(REG_22B_TQPNT1, 0xFFFFFFFF);
			HAL_RTL_W32(REG_22B_TQPNT2, 0xFFFFFFFF);
			HAL_RTL_W32(REG_22B_TQPNT3, 0xFFFFFFFF);
			HAL_RTL_W32(REG_22B_TQPNT4, 0xFFFFFFFF);
#endif
#if IS_EXIST_RTL8821CS
			HAL_RTL_W32(REG_21C_TQPNT1, 0xFFFFFFFF);
			HAL_RTL_W32(REG_21C_TQPNT2, 0xFFFFFFFF);
			HAL_RTL_W32(REG_21C_TQPNT3, 0xFFFFFFFF);
			HAL_RTL_W32(REG_21C_TQPNT4, 0xFFFFFFFF);
#endif
#endif
			//
			// Update current Tx FIFO page status.
			//
		sdio_query_txbuf_status(Adapter);
		sdio_query_txoqt_status(Adapter);
	
			// Enable MACTXEN/MACRXEN block
			HAL_RTL_W32(REG_CR, HAL_RTL_R32(REG_CR) | (BIT_MACTXEN | BIT_MACRXEN));
		}
#endif // IS_EXIST_SDIO

#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
if (Adapter->hci_type == RTL_HCI_USB) {
          #if defined(CONFIG_RTL_92E_SUPPORT) || defined(CONFIG_RTL_92F_SUPPORT)
		/* Enable MACTXEN/MACRXEN block */
		HAL_RTL_W32(REG_CR, HAL_RTL_R32(REG_CR) | (BIT_MACTXEN | BIT_MACRXEN));
          #else
            #error "triband undefined!!"
          #endif
}
#endif /* defined(CONFIG_RTL_TRIBAND_SUPPORT) */

	}
	break;

	case HW_VAR_EDCA: {
		PEDCA_PARA pEDCA = (PEDCA_PARA)(val);
		u1Byte      QueueIdx;
		u4Byte      ACPara;

		for ( QueueIdx = 0; QueueIdx < AC_PARAM_SIZE; QueueIdx++ ) {

			ACPara = ((pEDCA->Para[QueueIdx].TXOPlimit) << 16) \
					 | ((pEDCA->Para[QueueIdx].ECWmax) << 12) \
					 | ((pEDCA->Para[QueueIdx].ECWmin) << 8) \
					 | (pEDCA->sifs_time + (pEDCA->Para[QueueIdx].AIFSN) * pEDCA->slot_time);

			switch (QueueIdx) {
			case AC0_BE:
				HAL_RTL_W32(REG_EDCA_BE_PARAM, ACPara);
				break;

			case AC1_BK:
				HAL_RTL_W32(REG_EDCA_BK_PARAM, ACPara);
				break;

			case AC2_VI:
				HAL_RTL_W32(REG_EDCA_VI_PARAM, ACPara);
				break;

			case AC3_VO:
				HAL_RTL_W32(REG_EDCA_VO_PARAM, ACPara);
				break;
			}
		}

		HAL_RTL_W8(REG_ACMHWCTRL, 0x00);
	}
	break;

	case HW_VAR_CAM_RESET_ALL_ENTRY: {
		u1Byte  index;

		HAL_RTL_W32(REG_CAMCMD, BIT30);

		for (index = 0; index < HAL_TOTAL_CAM_ENTRY(Adapter); index++)
			CAMEmptyEntry88XX(Adapter, index);

		HAL_RTL_W32(REG_CR, HAL_RTL_R32(REG_CR) & (~BIT_MAC_SEC_EN));
	}
	break;

	case HW_VAR_SECURITY_CONFIG: {
		SECURITY_CONFIG_OPERATION SecCfg = *((PSECURITY_CONFIG_OPERATION)(val));
		u2Byte  SecCfgReg = 0;

		if (SecCfg & SCO_TXUSEDK) {
			SecCfgReg |= BIT_TXUHUSEDK;
		}

		if (SecCfg & SCO_RXUSEDK) {
			SecCfgReg |= BIT_RXUHUSEDK;
		}

		if (SecCfg & SCO_TXENC) {
			SecCfgReg |= BIT_TXENC;
		}

		if (SecCfg & SCO_RXDEC) {
			SecCfgReg |= BIT_RXDEC;
		}

		if (SecCfg & SCO_SKBYA2) {
			SecCfgReg |= BIT_SKBYA2;
		}

		if (SecCfg & SCO_NOSKMC) {
			SecCfgReg |= BIT_NOSKMC;
		}

		if (SecCfg & SCO_TXBCUSEDK) {
			SecCfgReg |= BIT_TXBCUSEDK;
		}

		if (SecCfg & SCO_RXBCUSEDK) {
			SecCfgReg |= BIT_RXBCUSEDK;
		}

		if (SecCfg & SCO_CHK_KEYID) {
			SecCfgReg |= BIT_CHK_KEYID;
		}

		HAL_RTL_W16(REG_SECCFG, SecCfgReg);
	}
	break;

	case HW_VAR_BEACON_INTERVAL: {
		u2Byte BcnInterval = *((pu2Byte)(val));
#if IS_RTL88XX_MAC_V4
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
            HAL_RTL_W8(REG_PORT_CTRL_SEL,HAL_RTL_R8(REG_PORT_CTRL_SEL) & ~BIT_MASK_PORT_CTRL_SEL);
            HAL_RTL_W16(REG_BCN_SPACE_CFG,BcnInterval);
        }
#endif
#if IS_RTL88XX_MAC_V1_V2_V3
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3)
		HAL_RTL_W16(REG_MBSSID_BCN_SPACE, BcnInterval);
#endif
	}
	break;

	case HW_VAR_ENABLE_BEACON_DMA: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			u2Byte stop_queue = HAL_RTL_R16(REG_LX_CTRL1);
			stop_queue &= ~BIT_STOP_BCNQ;
			HAL_RTL_W16(REG_LX_CTRL1, stop_queue);
		}
#endif //IS_RTL8881A_SERIES
#if (IS_RTL8192E_SERIES || IS_RTL8192F_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
		if ( IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8192F(Adapter) || IS_HARDWARE_TYPE_8814A(Adapter)|| IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)) {
			u2Byte stop_queue = HAL_RTL_R16(REG_PCIE_CTRL);
			stop_queue &= ~BIT_STOP_BCNQ;
			HAL_RTL_W16(REG_PCIE_CTRL, stop_queue);
		}
#endif //(IS_RTL8192E_SERIES || IS_RTL8192F_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
#if (IS_RTL8197F_SERIES || IS_RTL8198F_SERIES || IS_RTL8197G_SERIES)
        if (IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter)) {
            u2Byte stop_queue = HAL_RTL_R16(REG_HCI_CTRL);
            stop_queue &= ~BIT_STOP_BCNQ;
            HAL_RTL_W16(REG_HCI_CTRL, stop_queue);
        }
#endif //(IS_RTL8197F_SERIES || IS_RTL8198F_SERIES || IS_RTL8197G_SERIES)
#if IS_RTL8814B_SERIES
        if (IS_HARDWARE_TYPE_8814B(Adapter)){
            u4Byte stop_queue = HAL_RTL_R32(REG_ACH_CTRL);
            stop_queue &= ~BIT_STOP_P0BCNQ;
            HAL_RTL_W32(REG_ACH_CTRL, stop_queue);
        }
#endif

	}
	break;

	case HW_VAR_TXPAUSE: {
		u1Byte QueueIndexBIT = *((pu1Byte)(val));

		HAL_RTL_W8(REG_TXPAUSE, QueueIndexBIT);
	}
	break;


	case HW_VAR_HIQ_NO_LMT_EN: {
		u1Byte HiQNoLMTEn = *((pu1Byte)(val));
        u2Byte HiQNoLMTEn2 = *((pu2Byte)(val));
#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8197F_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
        if( IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8881A(Adapter) ||IS_HARDWARE_TYPE_8814A(Adapter) ||IS_HARDWARE_TYPE_8822B(Adapter)|| IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)) {
		HAL_RTL_W8(REG_HIQ_NO_LMT_EN, HiQNoLMTEn);
	}
#endif //(IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8197F_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
#if IS_RTL8198F_SERIES || IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter)) 
    		HAL_RTL_W16(REG_HIQ_NO_LMT_EN_V2, HiQNoLMTEn2);            
#endif //#if IS_RTL8198F_SERIES || IS_RTL8197G_SERIES
#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter)) 
            HAL_RTL_W16(REG_HIQ_NO_LMT_EN_V1, HiQNoLMTEn2);            
#endif //#if IS_RTL8814B_SERIES

	}
	break;
    case HW_VAR_MBSSID_EN:{
   		u1Byte MBssid = *((pu1Byte)(val));
        u2Byte MBssid2 = *((pu2Byte)(val));
#if IS_RTL88XX_MAC_V1_V2
	if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2)
    		HAL_RTL_W8(REG_MBSSID_CTRL, MBssid);
#endif // IS_RTL88XX_MAC_V1_V2

#if IS_RTL88XX_MAC_V3
	if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v3){
    		HAL_RTL_W16(REG_MBID_BCNQ_EN, MBssid2);            
    }
#endif // IS_RTL88XX_MAC_V3
#if IS_RTL88XX_MAC_V4
    if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
            HAL_RTL_W16(REG_MBSSID_CTRL_V1, MBssid2); 
    }
#endif //IS_RTL88XX_MAC_V4       

    }
	break;

	case HW_VAR_DRV_DBG: {
		u4Byte ErrorFlag = *((pu4Byte)(val));
		HAL_RTL_W32(REGDUMP_DRV_ERR0, ErrorFlag);
	}
	break;

	case HW_VAR_NUM_TXDMA_STATUS: {
		u4Byte RegTxDMA = *((pu4Byte)(val));
		HAL_RTL_W32(REG_TXDMA_STATUS, RegTxDMA);
	}
	break;

	case HW_VAR_NUM_RXDMA_STATUS: {
		u1Byte RegRxDMA = *((pu1Byte)(val));
		HAL_RTL_W8(REG_RXDMA_STATUS, RegRxDMA);
	}
	break;
	
	case HW_VAR_PS_TIMER:
    {
        u4Byte psTimer = *((pu4Byte)(val));
#if IS_RTL88XX_MAC_V1
		if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1) {        
            HAL_RTL_W32(REG_PS_TIMER, psTimer);
        }
#endif  //IS_RTL88XX_MAC_V1        
#if (IS_RTL88XX_MAC_V2_V3)
    if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3) {
            HAL_RTL_W32(REG_PS_TIMER0, psTimer);
        }
#endif  //IS_RTL88XX_MAC_V2_V3
#if (IS_RTL88XX_MAC_V4)
        if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4) {
            HAL_RTL_W32(REG_PS_TIMER_0, psTimer);
            HAL_RTL_W8(REG_PS_TIMER_01_CTRL, BIT_PS_TIMER_0_TSF_SEL(0) | BIT_PS_TIMER_0_EN);
        }
#endif        

    }
    break;

#if CFG_HAL_AP_SWPS_OFFLOAD
    case HW_VAR_SWPS_RPT_POLLING:
    {
        HAL_RTL_W8(REG_SWPS_CTRL, HAL_RTL_R8(REG_SWPS_CTRL) | BIT1);
    }
    break;
#endif

#if CFG_HAL_MULTICAST_BMC_ENHANCE
    case HW_VAR_BMC_RTS0_INVALID:
        {
            u1Byte valid = *((pu1Byte)(val));
            HAL_RTL_W8(REG_RTS_ADDR0, valid);
        }
        break;
        
        case HW_VAR_BMC_RTS0_ADDR:
        {
            HAL_RTL_W32(REG_TCR,HAL_RTL_R32(REG_TCR)|BIT31);
            s1Byte  idx = 0;
            //For Endian Free.
            for(idx=6; idx > 0 ; idx--)
            {
                HAL_RTL_W8((REG_RTS_ADDRESS_0+0x6-idx),val[6-idx]);
            }
        }
        break;        
        
        case HW_VAR_BMC_RTS1_INVALID:
        {
            u1Byte valid = *((pu1Byte)(val));
            HAL_RTL_W8(REG_RTS_ADDR1, valid);
        }
        break;
        
        case HW_VAR_BMC_RTS1_ADDR:
        {
            HAL_RTL_W32(REG_TCR,HAL_RTL_R32(REG_TCR)|BIT31);            
            s1Byte  idx = 0;
            //For Endian Free.
            for(idx=6; idx > 0 ; idx--)
            {
                HAL_RTL_W8((REG_RTS_ADDR1+0x6-idx),val[6-idx]);
            }
        }
        break;     
#endif // CFG_HAL_MULTICAST_BMC_ENHANCE
		case HW_VAR_REG_CCK_CHECK: 
		{
			u1Byte RegVal = *((pu1Byte)(val));
			HAL_RTL_W8(REG_CCK_CHECK, RegVal);
		}
		break;
#if (IS_RTL8192E_SERIES || IS_RTL8192F_SERIES || IS_RTL8881A_SERIES)
		case HW_VAR_HWSEQ_CTRL:
		{
			u1Byte RegVal = *((pu1Byte)(val));
			HAL_RTL_W8(REG_HWSEQ_CTRL, RegVal);
		}
		break;
#endif //(IS_RTL8192E_SERIES | IS_RTL8881A_SERIES)
		case HW_VAR_REG_CR:
		{
			u4Byte RegVal = *((pu4Byte)(val));
			HAL_RTL_W32(REG_CR, RegVal);
		}
		break;
        case HW_VAR_RXPKT_NUM:
        {
            u4Byte RegVal = *((pu4Byte)(val));
#if IS_RTL88XX_MAC_V4
        if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)
            HAL_RTL_W32(REG_RXDMA_CTRL,RegVal);
#endif //IS_RTL88XX_MAC_V4            
#if IS_RTL88XX_MAC_V1_V2_V3
        if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3)
            HAL_RTL_W32(REG_RXPKT_NUM, RegVal);
#endif
        }
        break;
#if CFG_HAL_SUPPORT_TXDESC_IE        
        case HW_VAR_TXDESC_TRANS_STATUS:
        {
            u1Byte RegVal = *((pu1Byte)(val));
			HAL_RTL_W8(REG_PKT_TRANS, RegVal);
        }
        break;
#endif        
	default:
		RT_TRACE_F(COMP_IO, DBG_WARNING, ("Command ID(%d) not Supported\n", variable));
		break;
	}
}


VOID
GetHwReg88XX(
	IN      HAL_PADAPTER          Adapter,
	IN      u1Byte                variable,
	OUT     pu1Byte               val
)
{

#if  defined(WLAN_REG_FW_RAM_REFINE) && defined(__ECOS)
	pu1Byte var_address = val;
	u4Byte	var_size;
#endif

#ifdef PCIE_POWER_SAVING_TEST //yllin
    if((Adapter->pwr_state == L2) || (Adapter->pwr_state == L1)) 
    {
        printk("Error, in L1/L2, dont access MAC reg = %x\n",variable);
            return;
    }
#endif

	switch (variable) {
	case HW_VAR_ETHER_ADDR: {
		*((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, REG_MACID);
		*((pu2Byte)(val + 4)) = platform_efio_read_2byte(Adapter, REG_MACID + 4);
	}
	break;

	case HW_VAR_BSSID: {
		*((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, REG_BSSID);
		*((pu2Byte)(val + 4)) = platform_efio_read_2byte(Adapter, (REG_BSSID + 4));
	}
	break;


	case HW_VAR_MAC_IO_ENABLE: {
		*((PBOOLEAN)val) = ((platform_efio_read_2byte(Adapter, REG_SYS_FUNC_EN) & (BIT_FEN_MREGEN | BIT_FEN_DCORE) ) == (BIT_FEN_MREGEN | BIT_FEN_DCORE));
	}
	break;

	
#if  defined(WLAN_REG_FW_RAM_REFINE) && defined(__ECOS)
		case HW_VAR_MACREGFILE_START: 
		case HW_VAR_PHYREGFILE_START:
		case HW_VAR_PHYREGFILE_HP_START:
		case HW_VAR_PHYREGFILE_EXTPA_START:
		case HW_VAR_PHYREGFILE_EXTLNA_START:
		case HW_VAR_PHYREGFILE_1T_START:
		case HW_VAR_PHYREGFILE_MP_START:
		case HW_VAR_PHYREGFILE_PG_START: 
		case HW_VAR_PHYREGFILE_PG_HP_START:
		case HW_VAR_POWERLIMITFILE_START:
		case HW_VAR_POWERLIMITFILE_TXBF_START:
		case HW_VAR_POWERLIMITFILE_HP_START:
		case HW_VAR_PHYREGFILE_AGC_START:
		case HW_VAR_PHYREGFILE_AGC_HP_START:
		case HW_VAR_PHYREGFILE_AGC_EXTPA_START:
		case HW_VAR_RFREGFILE_RADIO_A_START:
		case HW_VAR_RFREGFILE_RADIO_B_START:
		case HW_VAR_RFREGFILE_RADIO_A_HP_START:
		case HW_VAR_RFREGFILE_RADIO_A_EXTPA_START:
		case HW_VAR_RFREGFILE_RADIO_A_EXTLNA_START:
		case HW_VAR_RFREGFILE_RADIO_B_HP_START:
		case HW_VAR_RFREGFILE_RADIO_B_EXTPA_START:
		case HW_VAR_RFREGFILE_RADIO_B_EXTLNA_START:
		case HW_VAR_RFREGFILE_RADIO_C_START:
		case HW_VAR_RFREGFILE_RADIO_C_HP_START:
		case HW_VAR_RFREGFILE_RADIO_D_START:
		case HW_VAR_RFREGFILE_RADIO_D_HP_START:
		case HW_VAR_FWFILE_START:
		case HW_VAR_POWERTRACKINGFILE_START: 
		case HW_VAR_TXPKTFWFILE_START:
		{
			get_hw_reg_88xx(Adapter->dev->name, variable, (pu1Byte)&var_address);
			*((dma_addr_t*)(val)) = (dma_addr_t)var_address;
		}
		break;
			
		case HW_VAR_MACREGFILE_SIZE: 
		case HW_VAR_PHYREGFILE_SIZE: 
		case HW_VAR_PHYREGFILE_EXTPA_SIZE:
		case HW_VAR_PHYREGFILE_EXTLNA_SIZE:
		case HW_VAR_PHYREGFILE_1T_SIZE:
		case HW_VAR_PHYREGFILE_MP_SIZE:
		case HW_VAR_PHYREGFILE_PG_SIZE:
		case HW_VAR_PHYREGFILE_PG_HP_SIZE:
		case HW_VAR_POWERLIMITFILE_SIZE:
		case HW_VAR_POWERLIMITFILE_TXBF_SIZE:
		case HW_VAR_POWERLIMITFILE_HP_SIZE:
		case HW_VAR_PHYREGFILE_AGC_SIZE:
		case HW_VAR_PHYREGFILE_AGC_HP_SIZE:
		case HW_VAR_PHYREGFILE_AGC_EXTLNA_SIZE:
		case HW_VAR_RFREGFILE_RADIO_A_SIZE:
		case HW_VAR_RFREGFILE_RADIO_A_HP_SIZE: 
		case HW_VAR_RFREGFILE_RADIO_A_EXTPA_SIZE:
		case HW_VAR_RFREGFILE_RADIO_A_EXTLNA_SIZE:
		case HW_VAR_RFREGFILE_RADIO_B_SIZE:
		case HW_VAR_RFREGFILE_RADIO_B_HP_SIZE:
		case HW_VAR_RFREGFILE_RADIO_B_EXTPA_SIZE:
		case HW_VAR_RFREGFILE_RADIO_B_EXTLNA_SIZE:
		case HW_VAR_RFREGFILE_RADIO_C_SIZE:
		case HW_VAR_RFREGFILE_RADIO_C_HP_SIZE:
		case HW_VAR_RFREGFILE_RADIO_D_SIZE:
		case HW_VAR_RFREGFILE_RADIO_D_HP_SIZE:
		case HW_VAR_FWFILE_SIZE:
		case HW_VAR_POWERTRACKINGFILE_SIZE:
		case HW_VAR_TXPKTFWFILE_SIZE:
		{
			get_hw_reg_88xx(Adapter->dev->name, variable, (pu1Byte)&var_size);
			*((pu4Byte)(val)) = var_size;
		}
		break;
	
#else
	case HW_VAR_MACREGFILE_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				printk("[MAC_REG_8881A]\n");
				*((unsigned long*)(val)) = (unsigned long)data_MAC_REG_8881A_start;
			} else { // mp chip
				printk("[MAC_REG_8881Am]\n");
				*((unsigned long*)(val)) = (unsigned long)data_MAC_REG_8881Am_start;
			}
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				if (_GET_HAL_DATA(Adapter)->cutVersion	== ODM_CUT_B) {
					printk("[MAC_REG_8192Eb]\n");
					*((unsigned long*)(val)) = (unsigned long)data_MAC_REG_8192Eb_start;
				} else {
					printk("[MAC_REG_8192E]\n");
					*((unsigned long*)(val)) = (unsigned long)data_MAC_REG_8192E_start;
				}
			} else { // mp chip
				*((unsigned long*)(val)) = (unsigned long)data_MAC_REG_8192Emp_start;
				printk("[MAC_REG_8192Emp]\n");
			}
		}
#endif
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
                printk("[%s][MAC_REG_8814A]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_MAC_REG_8814A_start;
			} else { // mp chip
         	   printk("[%s][MAC_REG_8814Amp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_MAC_REG_8814Amp_start;
			}
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
               printk("[%s][MAC_REG_8197F]\n",__FUNCTION__);
               *((unsigned long*)(val)) = (unsigned long)data_MAC_REG_8197F_start;
        }
#endif  //IS_RTL8197F_SERIES

#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
               printk("[%s][MAC_REG_8198F]\n",__FUNCTION__);
               *((dma_addr_t*)(val)) = (dma_addr_t)data_MAC_REG_8198F_start;
        }
#endif  //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
            if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
                   printk("[%s][MAC_REG_8822Bmp]\n",__FUNCTION__);
                    //*((pu4Byte)(val)) = (u4Byte)data_MAC_REG_88;
            }
#endif  //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
            if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
                   printk("[%s][MAC_REG_8822Cmp]\n",__FUNCTION__);
                    //*((pu4Byte)(val)) = (u4Byte)data_MAC_REG_88;
            }
#endif  //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
            if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
                   printk("[%s][MAC_REG_8812Fmp]\n",__FUNCTION__);
                    //*((pu4Byte)(val)) = (u4Byte)data_MAC_REG_88;
            }
#endif  //IS_RTL8812F_SERIES

#if IS_RTL8814B_SERIES
            if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
                   printk("[%s][MAC_REG_8814B]\n",__FUNCTION__);//yllin8814B
                    *((unsigned long*)(val)) = (unsigned long)data_MAC_REG_8814B_start;
            }
#endif  //IS_RTL8814B_SERIES

#if IS_RTL8197G_SERIES
            if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
                   printk("[%s][MAC_REG_8197G]\n",__FUNCTION__);
                   *((unsigned long*)(val)) = (unsigned long)data_MAC_REG_8197G_start;
            }
#endif  //IS_RTL8197G_SERIES

	}
	break;

	case HW_VAR_MACREGFILE_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_MAC_REG_8881A_end - data_MAC_REG_8881A_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_MAC_REG_8881Am_end - data_MAC_REG_8881Am_start);
			}
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				if (_GET_HAL_DATA(Adapter)->cutVersion	== ODM_CUT_B)
					*((pu4Byte)(val)) = (u4Byte)(data_MAC_REG_8192Eb_end - data_MAC_REG_8192Eb_start);
				else
					*((pu4Byte)(val)) = (u4Byte)(data_MAC_REG_8192E_end - data_MAC_REG_8192E_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_MAC_REG_8192Emp_end - data_MAC_REG_8192Emp_start);
			}
		}
#endif
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_MAC_REG_8814A_end - data_MAC_REG_8814A_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_MAC_REG_8814Amp_end - data_MAC_REG_8814Amp_start);
				RT_TRACE(COMP_INIT, DBG_SERIOUS, ("%s(%d):Error, code should be added \n", __FUNCTION__, __LINE__));
			}
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MAC_REG_8197F_end- data_MAC_REG_8197F_start);
            //RT_TRACE(COMP_INIT, DBG_SERIOUS, ("%s(%d):Error, code should be added \n", __FUNCTION__, __LINE__));
        }
#endif  //IS_RTL8197F_SERIES

#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MAC_REG_8198F_end- data_MAC_REG_8198F_start);
            //RT_TRACE(COMP_INIT, DBG_SERIOUS, ("%s(%d):Error, code should be added \n", __FUNCTION__, __LINE__));
        }
#endif  //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
          //*((pu4Byte)(val)) = (u4Byte)(data_MAC_REG_8197Fm_end- data_MAC_REG_8197Fm_start);
          //      RT_TRACE(COMP_INIT, DBG_SERIOUS, ("%s(%d):Error, code should be added \n", __FUNCTION__, __LINE__));
        }
#endif  //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
          //*((pu4Byte)(val)) = (u4Byte)(data_MAC_REG_8197Fm_end- data_MAC_REG_8197Fm_start);
          //      RT_TRACE(COMP_INIT, DBG_SERIOUS, ("%s(%d):Error, code should be added \n", __FUNCTION__, __LINE__));
        }
#endif  //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
          //*((pu4Byte)(val)) = (u4Byte)(data_MAC_REG_8197Fm_end- data_MAC_REG_8197Fm_start);
          //      RT_TRACE(COMP_INIT, DBG_SERIOUS, ("%s(%d):Error, code should be added \n", __FUNCTION__, __LINE__));
        }
#endif  //IS_RTL8812F_SERIES

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) { //yllin8814B
            *((pu4Byte)(val)) = (u4Byte)(data_MAC_REG_8814B_end- data_MAC_REG_8814B_start);
            printk("[%s][MAC_REG_8814B size] %d\n", __func__, *((pu4Byte)(val)));
          //      RT_TRACE(COMP_INIT, DBG_SERIOUS, ("%s(%d):Error, code should be added \n", __FUNCTION__, __LINE__));
        }
#endif  //IS_RTL8814B_SERIES

#if IS_RTL8197G_SERIES
            if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
                *((pu4Byte)(val)) = (u4Byte)(data_MAC_REG_8197G_end- data_MAC_REG_8197G_start);
                //RT_TRACE(COMP_INIT, DBG_SERIOUS, ("%s(%d):Error, code should be added \n", __FUNCTION__, __LINE__));
            }
#endif  //IS_RTL8197G_SERIES


	}
	break;

	case HW_VAR_PHYREGFILE_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				printk("[PHY_REG_8881A]\n");
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8881A_start;
			} else { // mp chip
				printk("[PHY_REG_8881Am]\n");
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8881Am_start;				
			}
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				if (_GET_HAL_DATA(Adapter)->cutVersion	== ODM_CUT_B) {
					printk("[PHY_REG_8192Eb]\n");
					*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8192Eb_start;
				} else {
					printk("[PHY_REG_8192E]\n");
					*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8192E_start;
				}
			} else { // mp chip
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8192Emp_start;
				printk("[PHY_REG_8192Emp]\n");
			}

		}
#endif
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
                printk("[%s][PHY_REG_8814A]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8814A_start;
			} else { // mp chip
               	 printk("[%s][PHY_REG_8814Amp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8814Amp_start;
			}
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
                printk("[%s][PHY_REG_8197F]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8197F_start;
        }
#endif  //IS_RTL8197F_SERIES

#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
                printk("[%s][PHY_REG_8198F]\n",__FUNCTION__);
                *((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_8198F_start;
        }
#endif  //IS_RTL8198F_SERIES


#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            if (IS_HAL_TEST_CHIP(Adapter)) {
                printk("[%s][PHY_REG_8822B]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8822B_start;
			} else { // mp chip
               	 printk("[%s][PHY_REG_8822Bmp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8822Bmp_start;
			}
        }
#endif  //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            if (IS_HAL_TEST_CHIP(Adapter)) {
                printk("[%s][PHY_REG_8822C]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8822C_start;
			} else { // mp chip
               	 printk("[%s][PHY_REG_8822Cmp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8822Cmp_start;
			}
        }
#endif  //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            if (IS_HAL_TEST_CHIP(Adapter)) {
                printk("[%s][PHY_REG_8812F]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8812F_start;
			} else { // mp chip
               	 printk("[%s][PHY_REG_8812Fmp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8812Fmp_start;
			}
        }
#endif  //IS_RTL8812F_SERIES

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
             printk("[%s][PHY_REG_8814B]\n",__FUNCTION__);
            *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8814B_start; //yllin8814B, need phy reg file
        }
#endif  //IS_RTL8814B_SERIES

#if IS_RTL8197G_SERIES
            if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
                    printk("[%s][PHY_REG_8197G]\n",__FUNCTION__);
                    *((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_8197G_start;
            }
#endif  //IS_RTL8197G_SERIES

	}
	break;

	case HW_VAR_PHYREGFILE_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8881A_end - data_PHY_REG_8881A_start);
			} else { // mp chip
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8881Am_end - data_PHY_REG_8881Am_start);
			}
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				if (_GET_HAL_DATA(Adapter)->cutVersion	== ODM_CUT_B)
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8192Eb_end - data_PHY_REG_8192Eb_start);
				else
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8192E_end - data_PHY_REG_8192E_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8192Emp_end - data_PHY_REG_8192Emp_start);
			}
		}
#endif
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8814A_end - data_PHY_REG_8814A_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8814Amp_end - data_PHY_REG_8814Amp_start);
			}
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
			printk("[%s][PHY_REG_8197F] size\n",__FUNCTION__);
            *((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8197F_end - data_PHY_REG_8197F_start);
        }
#endif  //IS_RTL8197F_SERIES

#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
			printk("[%s][PHY_REG_8198F] size\n",__FUNCTION__);
            *((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8198F_end - data_PHY_REG_8198F_start);
        }

#endif  //IS_RTL8198F_SERIES


#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8822B_end - data_PHY_REG_8822B_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8822Bmp_end - data_PHY_REG_8822Bmp_start);
			}
        }
#endif  //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8822C_end - data_PHY_REG_8822C_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8822Cmp_end - data_PHY_REG_8822Cmp_start);
			}
        }
#endif  //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8812F_end - data_PHY_REG_8812F_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8812Fmp_end - data_PHY_REG_8812Fmp_start);
			}
        }
#endif  //IS_RTL8812F_SERIES

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8814B_end - data_PHY_REG_8814B_start);
            printk("[%s][PHY_REG_8814B size] %d\n", __func__, *((pu4Byte)(val)));
        }
#endif  //IS_RTL8814B_SERIES

#if IS_RTL8197G_SERIES
            if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
                printk("[%s][PHY_REG_8197G] size\n",__FUNCTION__);
                *((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8197G_end - data_PHY_REG_8197G_start);
            }
    
#endif  //IS_RTL8197G_SERIES
	}
	break;

#if (CFG_HAL_HIGH_POWER_EXT_PA & CFG_HAL_HIGH_POWER_EXT_LNA)
	case HW_VAR_PHYREGFILE_HP_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			//	*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8881A_hp_start;
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				printk("[PHY_REG_8192E_hp]\n");
#if (IS_EXIST_PCI || IS_EXIST_USB)
                if (Adapter->hci_type == RTL_HCI_PCIE || Adapter->hci_type == RTL_HCI_USB) {
					*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8192E_hp_start;
				}
#endif
			} else { // mp chip
#if (IS_EXIST_PCI || IS_EXIST_USB)
				if (Adapter->hci_type == RTL_HCI_PCIE || Adapter->hci_type == RTL_HCI_USB) {
					*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8192E_hp_start;
				}
#endif
				printk("[PHY_REG_8192E_hp]\n");
			}
		}
#endif
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				printk("[%s][PHY_REG_8814A_hp]\n",__FUNCTION__);
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8814A_hp_start;
			} else { // mp chip
				printk("[%s][PHY_REG_8814Amp_hp]\n",__FUNCTION__);
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8814Amp_hp_start;
			}
		}
#endif //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            printk("[%s][PHY_REG_8197F]\n",__FUNCTION__);
            *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8197F_start;
        }
#endif //IS_RTL8197F_SERIES

#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            printk("[%s][PHY_REG_8198F]\n",__FUNCTION__);
            *((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_8198F_start;
        }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				panic_printk("[%s][PHY_REG_8822B_hp]\n",__FUNCTION__);
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8822B_hp_start;
			} else { // mp chip
				panic_printk("[%s][PHY_REG_8822Bmp_hp]\n",__FUNCTION__);
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8822Bmp_hp_start;
			}
		}
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				panic_printk("[%s][PHY_REG_8822C_hp]\n",__FUNCTION__);
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8822C_hp_start;
			} else { // mp chip
				panic_printk("[%s][PHY_REG_8822Cmp_hp]\n",__FUNCTION__);
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8822Cmp_hp_start;
			}
		}
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				panic_printk("[%s][PHY_REG_8812F_hp]\n",__FUNCTION__);
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8812F_hp_start;
			} else { // mp chip
				panic_printk("[%s][PHY_REG_8812Fmp_hp]\n",__FUNCTION__);
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8812Fmp_hp_start;
			}
		}
#endif //IS_RTL8812F_SERIES

#if IS_RTL8197G_SERIES
            if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
                printk("[%s][PHY_REG_8197G]\n",__FUNCTION__);
                *((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_8197G_start;
            }
#endif //IS_RTL8197G_SERIES
	}
	break;

	case HW_VAR_PHYREGFILE_HP_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			//printk("Get 8881A PHY_REG_hp Len\n");
			//*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8881A_hp_end - data_PHY_REG_8881A_hp_start);
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
#if (IS_EXIST_PCI || IS_EXIST_USB)
                if (Adapter->hci_type == RTL_HCI_PCIE || Adapter->hci_type == RTL_HCI_USB) {
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8192E_hp_end - data_PHY_REG_8192E_hp_start);
				}
#endif
			} else { // mp chip
#if (IS_EXIST_PCI || IS_EXIST_USB)
				if (Adapter->hci_type == RTL_HCI_PCIE || Adapter->hci_type == RTL_HCI_USB) {
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8192E_hp_end - data_PHY_REG_8192E_hp_start);
				}
#endif
				//*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8192Emp_hp_end - data_PHY_REG_8192Emp_hp_start);
			}
		}
#endif
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8814A_hp_end - data_PHY_REG_8814A_hp_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8814Amp_hp_end - data_PHY_REG_8814Amp_hp_start);
			}

		}
#endif
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            printk("[%s][PHY_REG_8197F] size\n",__FUNCTION__);
            *((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8197F_end - data_PHY_REG_8197F_start);
        }
#endif //IS_RTL8197F_SERIES

#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            printk("[%s][PHY_REG_8198F] size\n",__FUNCTION__);
            *((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8198F_end - data_PHY_REG_8198F_start);
        }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8822B_hp_end - data_PHY_REG_8822B_hp_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8822Bmp_hp_end - data_PHY_REG_8822Bmp_hp_start);
			}

		}
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8822C_hp_end - data_PHY_REG_8822C_hp_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8822Cmp_hp_end - data_PHY_REG_8822Cmp_hp_start);
			}

		}
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8812F_hp_end - data_PHY_REG_8812F_hp_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8812Fmp_hp_end - data_PHY_REG_8812Fmp_hp_start);
			}

		}
#endif //IS_RTL8812F_SERIES

#if IS_RTL8197G_SERIES
            if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
                printk("[%s][PHY_REG_8197G] size\n",__FUNCTION__);
                *((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8197G_end - data_PHY_REG_8197G_start);
            }
#endif //IS_RTL8197G_SERIES
	}
	break;
#endif
#if CFG_HAL_HIGH_POWER_EXT_PA
	case HW_VAR_PHYREGFILE_EXTPA_START: {
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8192E_extpa_start;
			printk("[PHY_REG_8192E_extpa]\n");
		}
#endif
	}
	break;
	case HW_VAR_PHYREGFILE_EXTPA_SIZE: {
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8192E_extpa_end - data_PHY_REG_8192E_extpa_start);
		}
#endif
	}
	break;
#endif

#if CFG_HAL_HIGH_POWER_EXT_LNA
	case HW_VAR_PHYREGFILE_EXTLNA_START: {
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
#if (IS_EXIST_PCI || IS_EXIST_USB)
			if (Adapter->hci_type == RTL_HCI_PCIE || Adapter->hci_type == RTL_HCI_USB) {
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8192E_extlna_start;
			}
#endif
#if IS_EXIST_SDIO
			if (Adapter->hci_type == RTL_HCI_SDIO) {
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_8192ES_extlna_start;
			}
#endif
			printk("[PHY_REG_8192E_extlna]\n");
		}
#endif // IS_RTL8192E_SERIES
	}
	break;
	case HW_VAR_PHYREGFILE_EXTLNA_SIZE: {
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
#if (IS_EXIST_PCI || IS_EXIST_USB)
			if (Adapter->hci_type == RTL_HCI_PCIE || Adapter->hci_type == RTL_HCI_USB) {
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8192E_extlna_end - data_PHY_REG_8192E_extlna_start);
			}
#endif
#if IS_EXIST_SDIO
			if (Adapter->hci_type == RTL_HCI_SDIO) {
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_8192ES_extlna_end - data_PHY_REG_8192ES_extlna_start);
			}
#endif
		}
#endif // IS_RTL8192E_SERIES
	}
	break;
#endif // CFG_HAL_HIGH_POWER_EXT_LNA
	case HW_VAR_PHYREGFILE_1T_START:
		break;

	case HW_VAR_PHYREGFILE_1T_SIZE:
		break;

	case HW_VAR_PHYREGFILE_MP_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_MP_8881A_start;
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_MP_8192E_start;
			} else { // mp chip
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_MP_8192Emp_start;
			}
		}
#endif
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
            	printk("[%s][PHY_REG_MP_8814A]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_MP_8814A_start;
			} else { // mp chip
           		printk("[%s][PHY_REG_MP_8814Amp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_MP_8814Amp_start;                
			}
		}
#endif
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            printk("[%s][PHY_REG_MP_8197F]\n",__FUNCTION__);
            *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_MP_8197F_start;                
        }
#endif //IS_RTL8197F_SERIES

#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            printk("[%s][PHY_REG_MP_8198F]\n",__FUNCTION__);
            *((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_MP_8198F_start;                
        }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
            	printk("[%s][PHY_REG_MP_8822B]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_MP_8822B_start;
			} else { // mp chip
           		printk("[%s][PHY_REG_MP_8822Bmp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_MP_8822Bmp_start;                
			}
		}
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
            	printk("[%s][PHY_REG_MP_8822C]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_MP_8822C_start;
			} else { // mp chip
           		printk("[%s][PHY_REG_MP_8822Cmp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_MP_8822Cmp_start;                
			}
		}
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
            	printk("[%s][PHY_REG_MP_8812F]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_MP_8812F_start;
			} else { // mp chip
           		printk("[%s][PHY_REG_MP_8812Fmp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_MP_8812Fmp_start;                
			}
		}
#endif //IS_RTL8812F_SERIES

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            printk("[%s][PHY_REG_MP_8814Bmp]\n",__FUNCTION__);
            *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_MP_8814Bmp_start;    //need phy reg file yllin8814B            
        }
#endif //IS_RTL8814B_SERIES

#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            printk("[%s][PHY_REG_MP_8197G]\n",__FUNCTION__);
            *((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_MP_8197G_start;                
        }
#endif //IS_RTL8197G_SERIES

	}
	break;

	case HW_VAR_PHYREGFILE_MP_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_MP_8881A_end - data_PHY_REG_MP_8881A_start);
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_MP_8192E_end - data_PHY_REG_MP_8192E_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_MP_8192Emp_end - data_PHY_REG_MP_8192Emp_start);
			}
		}
#endif
#if IS_RTL8814A_SERIES
        if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
            if (IS_HAL_TEST_CHIP(Adapter)) {
                *((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_MP_8814A_end - data_PHY_REG_MP_8814A_start);
            } else { // mp chip
                *((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_MP_8814Amp_end - data_PHY_REG_MP_8814Amp_start);
            }
        }
#endif
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_MP_8197F_end - data_PHY_REG_MP_8197F_start);
        }
#endif //IS_RTL8197F_SERIES

#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_MP_8198F_end - data_PHY_REG_MP_8198F_start);
        }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            if (IS_HAL_TEST_CHIP(Adapter)) {
                *((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_MP_8822B_end - data_PHY_REG_MP_8822B_start);
            } else { // mp chip
                *((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_MP_8822Bmp_end - data_PHY_REG_MP_8822Bmp_start);
            }
        }
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            if (IS_HAL_TEST_CHIP(Adapter)) {
                *((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_MP_8822C_end - data_PHY_REG_MP_8822C_start);
            } else { // mp chip
                *((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_MP_8822Cmp_end - data_PHY_REG_MP_8822Cmp_start);
            }
        }
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            if (IS_HAL_TEST_CHIP(Adapter)) {
                *((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_MP_8812F_end - data_PHY_REG_MP_8812F_start);
            } else { // mp chip
                *((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_MP_8812Fmp_end - data_PHY_REG_MP_8812Fmp_start);
            }
        }
#endif //IS_RTL8812F_SERIES

#if IS_RTL8814B_SERIES
            if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
                *((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_MP_8814Bmp_end - data_PHY_REG_MP_8814Bmp_start); //need phy reg file yllin8814B
            }
#endif //IS_RTL8814B_SERIES

#if IS_RTL8197G_SERIES
            if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
                *((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_MP_8197G_end - data_PHY_REG_MP_8197G_start);
            }
#endif //IS_RTL8197G_SERIES

	}
	break;

	case HW_VAR_PHYREGFILE_PG_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				//printk("[PHY_REG_PG_8881A]\n");
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8881A_start;
			} else { // mp chip
				if (get_bonding_type_8881A() == BOND_8881AM) {
					if (HAL_VAR_INTERLPA_8881A) {
						printk("[PHY_REG_PG_8881AMP_intpa]\n");
						*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8881AMP_intpa_start;
					} else {
						printk("[PHY_REG_PG_8881AMP]\n");
						*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8881AMP_start;
					}
				} else if (get_bonding_type_8881A() == BOND_8881AB) {
					if (HAL_VAR_INTERLPA_8881A) {
						printk("[PHY_REG_PG_8881ABP_intpa]\n");
						*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8881ABP_intpa_start;
					} else {
						printk("[PHY_REG_PG_8881ABP]\n");
						*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8881ABP_start;
					}
				} else if (get_bonding_type_8881A() == BOND_8881AN) {
					if (HAL_VAR_INTERLPA_8881A) {
						printk("[PHY_REG_PG_8881AN]\n");
						*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8881AN_start;
					} else {
						printk("[PHY_REG_PG_8881AN_extpa]\n");
						*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8881AN_extpa_start;
					}
				} else {
#ifdef TXPWR_LMT_NEWFILE
					printk("[PHY_REG_PG_8881A_new]\n");
					*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8881A_new_start;
#else
					printk("[PHY_REG_PG_8881Am]\n");
					*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8881Am_start;
#endif
				}
			}
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
#ifdef TXPWR_LMT_92EE
			printk("[PHY_REG_PG_8192E_new]\n");
			*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8192E_new_start;
#else
			if (IS_HAL_TEST_CHIP(Adapter)) {
				printk("[PHY_REG_PG_8192E]\n");
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8192E_start;
			} else { // mp chip
				printk("[PHY_REG_PG_8192Emp]\n");
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8192Emp_start;
			}
#endif
		}
#endif
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
                panic_printk("[%s][PHY_REG_PG_8814A]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8814A_start;
			} else { // mp chip
#if CFG_HAL_HIGH_POWER_EXT_PA			
				if(HAL_VAR_use_ext_pa){
					if(HAL_RFE_TYPE == 2){
						panic_printk("[%s][PHY_REG_PG_8814Amp_Type2]\n",__FUNCTION__);
						*((unsigned long*)(val)) = (unsigned long)(data_PHY_REG_PG_8814Amp_Type2_start);
					}else if(HAL_RFE_TYPE == 3){
						panic_printk("[%s][PHY_REG_PG_8814Amp_Type3]\n",__FUNCTION__);
						*((unsigned long*)(val)) = (unsigned long)(data_PHY_REG_PG_8814Amp_Type3_start);
					}else if(HAL_RFE_TYPE == 4){
						panic_printk("[%s][PHY_REG_PG_8814Amp_Type4]\n",__FUNCTION__);
						*((unsigned long*)(val)) = (unsigned long)(data_PHY_REG_PG_8814Amp_Type4_start);
					}else if(HAL_RFE_TYPE == 5){
						panic_printk("[%s][PHY_REG_PG_8814Amp_Type5]\n",__FUNCTION__);
						*((unsigned long*)(val)) = (unsigned long)(data_PHY_REG_PG_8814Amp_Type5_start);
					}else if(HAL_RFE_TYPE == 8){
						panic_printk("[%s][PHY_REG_PG_8814Amp_Type8]\n",__FUNCTION__);
						*((unsigned long*)(val)) = (unsigned long)(data_PHY_REG_PG_8814Amp_Type8_start);
					}else if(HAL_RFE_TYPE == 10){
						panic_printk("[%s][PHY_REG_PG_8814Amp_Type10]\n",__FUNCTION__);
						*((unsigned long*)(val)) = (unsigned long)(data_PHY_REG_PG_8814Amp_Type10_start);
					}else if(HAL_RFE_TYPE == 11){
						panic_printk("[%s][PHY_REG_PG_8814Amp_Type11]\n",__FUNCTION__);
						*((unsigned long*)(val)) = (unsigned long)(data_PHY_REG_PG_8814Amp_Type11_start);
					}else {
						panic_printk("[%s][PHY_REG_PG_8814Amp]\n",__FUNCTION__);
						*((unsigned long*)(val)) = (unsigned long)(data_PHY_REG_PG_8814Amp_start);
					}
				}else
#endif				
				{
					if(HAL_RFE_TYPE == 0){
						panic_printk("[%s][PHY_REG_PG_8814Amp_Type0]\n",__FUNCTION__);
						*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8814Amp_Type0_start;
					}else if(HAL_RFE_TYPE == 7){
						panic_printk("[%s][PHY_REG_PG_8814Amp_Type7]\n",__FUNCTION__);
						*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8814Amp_Type7_start;
					}else if(HAL_RFE_TYPE == 9){
						panic_printk("[%s][PHY_REG_PG_8814Amp_Type9]\n",__FUNCTION__);
						*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8814Amp_Type9_start;
					}else{
						panic_printk("[%s][PHY_REG_PG_8814Amp]\n",__FUNCTION__);
						*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8814Amp_start;
					}
				}
			}
		}
#endif
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
#if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				if(HAL_RFE_TYPE == 1){
					panic_printk("[%s][PHY_REG_PG_8197Fmp_Type1]\n",__FUNCTION__);
					*((unsigned long*)(val)) = (unsigned long)(data_PHY_REG_PG_8197Fmp_Type1_start);
				}else if(HAL_RFE_TYPE == 3){
					panic_printk("[%s][PHY_REG_PG_8197Fmp_Type3]\n",__FUNCTION__);
					*((unsigned long*)(val)) = (unsigned long)(data_PHY_REG_PG_8197Fmp_Type3_start);					
				}else if(HAL_RFE_TYPE == 4){
					panic_printk("[%s][PHY_REG_PG_8197Fmp_Type4]\n",__FUNCTION__);
					*((unsigned long*)(val)) = (unsigned long)(data_PHY_REG_PG_8197Fmp_Type4_start);					
				}else if(HAL_RFE_TYPE == 5){
					panic_printk("[%s][PHY_REG_PG_8197Fmp_Type5]\n",__FUNCTION__);
					*((unsigned long*)(val)) = (unsigned long)(data_PHY_REG_PG_8197Fmp_Type5_start);					
				}else {
					printk("[%s][PHY_REG_PG_8197Fmp]\n",__FUNCTION__);
          			*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8197Fmp_start;
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
					panic_printk("[%s][PHY_REG_PG_8197Fmp_Type0]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8197Fmp_Type0_start;
				}else if(HAL_RFE_TYPE == 2){
					printk("[%s][PHY_REG_PG_8197Fmp_Type2]\n",__FUNCTION__);
          			*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8197Fmp_Type2_start;
				}else if(HAL_RFE_TYPE == 3){
					panic_printk("[%s][PHY_REG_PG_8197Fmp_Type3]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8197Fmp_Type3_start);					
				}else if(HAL_RFE_TYPE == 4){
					panic_printk("[%s][PHY_REG_PG_8197Fmp_Type4]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8197Fmp_Type4_start);					
				}else if(HAL_RFE_TYPE == 6){
					panic_printk("[%s][PHY_REG_PG_8197Fmp_Type6]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8197Fmp_Type6_start);							
				}else{
					printk("[%s][PHY_REG_PG_8197Fmp]\n",__FUNCTION__);
          			*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8197Fmp_start;
				}
			}
        }
#endif //IS_RTL8197F_SERIES

#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
#if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				if(HAL_RFE_TYPE == 1){
					printk("[%s][PHY_REG_PG_8198Fmp_Type1 start ] %x \n",__FUNCTION__,(dma_addr_t)data_PHY_REG_PG_8198Fmp_Type1_start);
 					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8198Fmp_Type1_start;
				}else if(HAL_RFE_TYPE == 3){
					printk("[%s][PHY_REG_PG_8198Fmp_Type3 start ] %x \n",__FUNCTION__,(dma_addr_t)data_PHY_REG_PG_8198Fmp_Type3_start);
 					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8198Fmp_Type3_start;
				}else {
					printk("[%s][PHY_REG_PG_8198Fmp start ] %x \n",__FUNCTION__,(dma_addr_t)data_PHY_REG_PG_8198Fmp_start);
 					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8198Fmp_start;
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
					printk("[%s][PHY_REG_PG_8198Fmp_Type0 start ] %x \n",__FUNCTION__,(dma_addr_t)data_PHY_REG_PG_8198Fmp_Type0_start);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8198Fmp_Type0_start;
				}else if(HAL_RFE_TYPE == 2){
					printk("[%s][PHY_REG_PG_8198Fmp_Type2 start ] %x \n",__FUNCTION__,(dma_addr_t)data_PHY_REG_PG_8198Fmp_Type2_start);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8198Fmp_Type2_start;
				}else {
					printk("[%s][PHY_REG_PG_8198Fmp start ] %x \n",__FUNCTION__,(dma_addr_t)data_PHY_REG_PG_8198Fmp_start);
 					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8198Fmp_start;
				}
			}
        }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
#if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				{
					printk("[%s][PHY_REG_PG_8197Gmp start ] %x \n",__FUNCTION__,(dma_addr_t)data_PHY_REG_PG_8197Gmp_start);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8197Gmp_start;
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
					printk("[%s][PHY_REG_PG_8197Gmp_Type0 start ] %x \n",__FUNCTION__,(dma_addr_t)data_PHY_REG_PG_8197Gmp_Type0_start);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8197Gmp_Type0_start;
				}else {
					printk("[%s][PHY_REG_PG_8197Gmp start ] %x \n",__FUNCTION__,(dma_addr_t)data_PHY_REG_PG_8197Gmp_start);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8197Gmp_start;
				}
			}
        }
#endif //IS_RTL8197G_SERIES

#if IS_RTL8822B_SERIES
	if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
		if (IS_HAL_TEST_CHIP(Adapter)) {
			printk("[%s][PHY_REG_PG_8822B]\n",__FUNCTION__);
			*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8822B_start;
		} else { // mp chip
#if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				if(HAL_RFE_TYPE == 1){
					panic_printk("[%s][PHY_REG_PG_8822Bmp_Type1]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8822Bmp_Type1_start);
				}else if(HAL_RFE_TYPE == 4){
					panic_printk("[%s][PHY_REG_PG_8822Bmp_Type4]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8822Bmp_Type4_start);
				}else if(HAL_RFE_TYPE == 6){
					panic_printk("[%s][PHY_REG_PG_8822Bmp_Type6]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8822Bmp_Type6_start);
				}else if(HAL_RFE_TYPE == 7){
					panic_printk("[%s][PHY_REG_PG_8822Bmp_Type7]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8822Bmp_Type7_start);
				}else if(HAL_RFE_TYPE == 11){
					panic_printk("[%s][PHY_REG_PG_8822Bmp_Type11]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8822Bmp_Type11_start);
				}else {
					panic_printk("[%s][PHY_REG_PG_8822Bmp]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8822Bmp_start);
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
					panic_printk("[%s][PHY_REG_PG_8822Bmp_Type0]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8822Bmp_Type0_start;
				}else if(HAL_RFE_TYPE == 8){
					panic_printk("[%s][PHY_REG_PG_8822Bmp_Type8]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8822Bmp_Type8_start;
				}else if(HAL_RFE_TYPE == 9){
					panic_printk("[%s][PHY_REG_PG_8822Bmp_Type9\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8822Bmp_Type9_start;
				}else if(HAL_RFE_TYPE == 10){
					panic_printk("[%s][PHY_REG_PG_8822Bmp_Type10\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8822Bmp_Type10_start;
				}else if(HAL_RFE_TYPE == 13){
					panic_printk("[%s][PHY_REG_PG_8822Bmp_Type13\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8822Bmp_Type13_start;
				}else if(HAL_RFE_TYPE == 14){
					panic_printk("[%s][PHY_REG_PG_8822Bmp_Type14\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8822Bmp_Type14_start;
				}else{
					panic_printk("[%s][PHY_REG_PG_8822Bmp]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8822Bmp_start;
				}
			}
		}
	}
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
	if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
		if (IS_HAL_TEST_CHIP(Adapter)) {
			printk("[%s][PHY_REG_PG_8822C]\n",__FUNCTION__);
			*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8822C_start;
		} else { // mp chip
#if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				if(HAL_RFE_TYPE == 1){
					panic_printk("[%s][PHY_REG_PG_8822Cmp_Type1]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8822Cmp_Type1_start);
				}else if(HAL_RFE_TYPE == 4){
					panic_printk("[%s][PHY_REG_PG_8822Cmp_Type4]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8822Cmp_Type4_start);
				}else if(HAL_RFE_TYPE == 6){
					panic_printk("[%s][PHY_REG_PG_8822Cmp_Type6]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8822Cmp_Type6_start);
				}else if(HAL_RFE_TYPE == 7){
					panic_printk("[%s][PHY_REG_PG_8822Cmp_Type7]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8822Cmp_Type7_start);
				}else if(HAL_RFE_TYPE == 11){
					panic_printk("[%s][PHY_REG_PG_8822Cmp_Type11]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8822Cmp_Type11_start);
				}else {
					panic_printk("[%s][PHY_REG_PG_8822Cmp]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8822Cmp_start);
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
					panic_printk("[%s][PHY_REG_PG_8822Cmp_Type0]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8822Cmp_Type0_start;
				}else if(HAL_RFE_TYPE == 8){
					panic_printk("[%s][PHY_REG_PG_8822Cmp_Type8]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8822Cmp_Type8_start;
				}else if(HAL_RFE_TYPE == 9){
					panic_printk("[%s][PHY_REG_PG_8822Cmp_Type9\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8822Cmp_Type9_start;
				}else if(HAL_RFE_TYPE == 10){
					panic_printk("[%s][PHY_REG_PG_8822Cmp_Type10\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8822Cmp_Type10_start;
				}else if(HAL_RFE_TYPE == 13){
					panic_printk("[%s][PHY_REG_PG_8822Cmp_Type13\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8822Cmp_Type13_start;
				}else if(HAL_RFE_TYPE == 14){
					panic_printk("[%s][PHY_REG_PG_8822Cmp_Type14\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8822Cmp_Type14_start;
				}else{
					panic_printk("[%s][PHY_REG_PG_8822Cmp]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8822Cmp_start;
				}
			}
		}
	}
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
	if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
		if (IS_HAL_TEST_CHIP(Adapter)) {
			printk("[%s][PHY_REG_PG_8812F]\n",__FUNCTION__);
			*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8812F_start;
		} else { // mp chip
#if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				if(HAL_RFE_TYPE == 1){
					panic_printk("[%s][PHY_REG_PG_8812Fmp_Type1]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8812Fmp_Type1_start);
				}else if(HAL_RFE_TYPE == 3){
					panic_printk("[%s][PHY_REG_PG_8812Fmp_Type3]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8812Fmp_Type3_start);
				}else if(HAL_RFE_TYPE == 4){
					panic_printk("[%s][PHY_REG_PG_8812Fmp_Type4]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8812Fmp_Type4_start);
				}else if(HAL_RFE_TYPE == 6){
					panic_printk("[%s][PHY_REG_PG_8812Fmp_Type6]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8812Fmp_Type6_start);
				}else if(HAL_RFE_TYPE == 7){
					panic_printk("[%s][PHY_REG_PG_8812Fmp_Type7]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8812Fmp_Type7_start);
				}else if(HAL_RFE_TYPE == 11){
					panic_printk("[%s][PHY_REG_PG_8812Fmp_Type11]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8812Fmp_Type11_start);
				}else {
					panic_printk("[%s][PHY_REG_PG_8812Fmp]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)(data_PHY_REG_PG_8812Fmp_start);
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
					panic_printk("[%s][PHY_REG_PG_8812Fmp_Type0]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8812Fmp_Type0_start;
				}else if(HAL_RFE_TYPE == 2){
					panic_printk("[%s][PHY_REG_PG_8812Fmp_Type2]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8812Fmp_Type2_start;
				}else if(HAL_RFE_TYPE == 8){
					panic_printk("[%s][PHY_REG_PG_8812Fmp_Type8]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8812Fmp_Type8_start;
				}else if(HAL_RFE_TYPE == 9){
					panic_printk("[%s][PHY_REG_PG_8812Fmp_Type9\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8812Fmp_Type9_start;
				}else if(HAL_RFE_TYPE == 10){
					panic_printk("[%s][PHY_REG_PG_8812Fmp_Type10\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8812Fmp_Type10_start;
				}else if(HAL_RFE_TYPE == 13){
					panic_printk("[%s][PHY_REG_PG_8812Fmp_Type13\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8812Fmp_Type13_start;
				}else if(HAL_RFE_TYPE == 14){
					panic_printk("[%s][PHY_REG_PG_8812Fmp_Type14\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8812Fmp_Type14_start;
				}else{
					panic_printk("[%s][PHY_REG_PG_8812Fmp]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8812Fmp_start;
				}
			}
		}
	}
#endif //IS_RTL8812F_SERIES

#if IS_RTL8821C_SERIES
	if ( IS_HARDWARE_TYPE_8821C(Adapter) ) {
		if (IS_HAL_TEST_CHIP(Adapter)) {
			printk("[%s][PHY_REG_PG_8821C]\n",__FUNCTION__);
			*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8821C_start;
		} else { // mp chip
			if(HAL_RFE_TYPE == 0){
				panic_printk("[%s][PHY_REG_PG_8821Cmp_Type0]\n",__FUNCTION__);
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8821Cmp_Type0_start;
			}else{
				panic_printk("[%s][PHY_REG_PG_8821Cmp]\n",__FUNCTION__);
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8821Cmp_start;
			}
		}
	}
#endif //IS_RTL8821C_SERIES

#if IS_RTL8814B_SERIES
			if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
#if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				if(HAL_RFE_TYPE == 1){
					printk("[%s][PHY_REG_PG_8814Bmp_Type1 start ] %x \n",__FUNCTION__,(unsigned long)data_PHY_REG_PG_8814Bmp_Type1_start);
 					*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8814Bmp_Type1_start;
				}else if(HAL_RFE_TYPE == 4){
					printk("[%s][PHY_REG_PG_8814Bmp_Type4 start ] %x \n",__FUNCTION__,(unsigned long)data_PHY_REG_PG_8814Bmp_Type4_start);
 					*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8814Bmp_Type4_start;
				}else if(HAL_RFE_TYPE == 5){
					printk("[%s][PHY_REG_PG_8814Bmp_Type5 start ] %x \n",__FUNCTION__,(unsigned long)data_PHY_REG_PG_8814Bmp_Type5_start);
 					*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8814Bmp_Type5_start;
				}else {
					printk("[%s][PHY_REG_PG_8814Bmp start ] %x \n",__FUNCTION__,(unsigned long)data_PHY_REG_PG_8814Bmp_start);
 					*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8814Bmp_start;
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
					//FIXME: PHY_REG_PG_8814Bmp_Type0?
					printk("[%s][PHY_REG_PG_8814Bmp_Type0 start ] %x \n",__FUNCTION__,(unsigned long)data_PHY_REG_PG_8814Bmp_Type0_start);
					*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8814Bmp_Type0_start;
				}else if(HAL_RFE_TYPE == 2){
					//FIXME: PHY_REG_PG_8814Bmp_Type0?
					printk("[%s][PHY_REG_PG_8814Bmp_Type2 start ] %x \n",__FUNCTION__,(unsigned long)data_PHY_REG_PG_8814Bmp_Type2_start);
					*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8814Bmp_Type2_start;
				}else if(HAL_RFE_TYPE == 7){
					printk("[%s][PHY_REG_PG_8814Bmp_Type7 start ] %x \n",__FUNCTION__,(unsigned long)data_PHY_REG_PG_8814Bmp_Type7_start);
					*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8814Bmp_Type7_start;
				}else if(HAL_RFE_TYPE == 8){
					printk("[%s][PHY_REG_PG_8814Bmp_Type8 start ] %x \n",__FUNCTION__,(unsigned long)data_PHY_REG_PG_8814Bmp_Type8_start);
					*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8814Bmp_Type8_start;
				}else {
					printk("[%s][PHY_REG_PG_8814Bmp start ] %x \n",__FUNCTION__,(unsigned long)data_PHY_REG_PG_8814Bmp_start);
 					*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8814Bmp_start;
				}
			}
            // TODO other PHY_REG
				
                ///printk("[%s][PHY_REG_PG_8814Bmp start ] %x \n",__FUNCTION__,(dma_addr_t)data_PHY_REG_PG_8814Bmp_start);
                ///*((dma_addr_t*)(val)) = (dma_addr_t)data_PHY_REG_PG_8814Bmp_start;  // need phy reg yllin8814B
                
                //3 need other phy reg for _EXT_PA
            }
#endif //IS_RTL8814B_SERIES

#if IS_RTL8192F_SERIES
#if (CONFIG_WLAN_CONF_TXT_EXIST==1)
	if ( IS_HARDWARE_TYPE_8192F(Adapter) ) {
		if (IS_HAL_TEST_CHIP(Adapter)) {
			printk("[PHY_REG_PG_8192F]\n");
			*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8192F_start;
		} else { // mp chip
			printk("[PHY_REG_PG_8192Fmp]\n");
			*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8192Fmp_start;
		}
	}
#endif
#endif
	}
	break;

	case HW_VAR_PHYREGFILE_PG_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8881A_end - data_PHY_REG_PG_8881A_start);
			} else { // mp chip
				if (get_bonding_type_8881A() == BOND_8881AM) {
					if (HAL_VAR_INTERLPA_8881A)
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8881AMP_intpa_end - data_PHY_REG_PG_8881AMP_intpa_start);
					else
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8881AMP_end - data_PHY_REG_PG_8881AMP_start);
				} else if (get_bonding_type_8881A() == BOND_8881AB) {
					if (HAL_VAR_INTERLPA_8881A)
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8881ABP_intpa_end - data_PHY_REG_PG_8881ABP_intpa_start);
					else
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8881ABP_end - data_PHY_REG_PG_8881ABP_start);
				} else if (get_bonding_type_8881A() == BOND_8881AN) {
					if (HAL_VAR_INTERLPA_8881A)
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8881AN_end - data_PHY_REG_PG_8881AN_start);
					else
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8881AN_extpa_end - data_PHY_REG_PG_8881AN_extpa_start);
				} else {
#ifdef TXPWR_LMT_NEWFILE
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8881A_new_end - data_PHY_REG_PG_8881A_new_start);
#else
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8881Am_end - data_PHY_REG_PG_8881Am_start);
#endif
				}
			}
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {

#ifdef TXPWR_LMT_92EE
			//printk("Get PHY_REG_PG_8192E_new size\n");
			*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8192E_new_end - data_PHY_REG_PG_8192E_new_start);
#else
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8192E_end - data_PHY_REG_PG_8192E_start);
			} else { // mp chip
				//printk("Get PHY_REG_PG_8192Emp size\n");
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8192Emp_end - data_PHY_REG_PG_8192Emp_start);
			}
#endif
		}
#endif
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				//printk("[%s]HW_VAR_PHYREGFILE_PG_SIZE\n",__FUNCTION__);
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814A_end - data_PHY_REG_PG_8814A_start);
			} else { // mp chip
#if CFG_HAL_HIGH_POWER_EXT_PA			
				if(HAL_VAR_use_ext_pa){
					if(HAL_RFE_TYPE == 2){
						printk("[%s][size PHY_REG_PG_8814Amp_Type2]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814Amp_Type2_end - data_PHY_REG_PG_8814Amp_Type2_start);
					}else if(HAL_RFE_TYPE == 3){
						printk("[%s][size PHY_REG_PG_8814Amp_Type3]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814Amp_Type3_end - data_PHY_REG_PG_8814Amp_Type3_start);
					}else if(HAL_RFE_TYPE == 4){
						printk("[%s][size PHY_REG_PG_8814Amp_Type4]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814Amp_Type4_end - data_PHY_REG_PG_8814Amp_Type4_start);
					}else if(HAL_RFE_TYPE == 5){
						printk("[%s][size PHY_REG_PG_8814Amp_Type5]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814Amp_Type5_end - data_PHY_REG_PG_8814Amp_Type5_start);
					}else if(HAL_RFE_TYPE == 8){
						printk("[%s][size PHY_REG_PG_8814Amp_Type8]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814Amp_Type8_end - data_PHY_REG_PG_8814Amp_Type8_start);
					}else if(HAL_RFE_TYPE == 10){
						printk("[%s][size PHY_REG_PG_8814Amp_Type10]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814Amp_Type10_end - data_PHY_REG_PG_8814Amp_Type10_start);
					}else if(HAL_RFE_TYPE == 11){
						printk("[%s][size PHY_REG_PG_8814Amp_Type11]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814Amp_Type11_end - data_PHY_REG_PG_8814Amp_Type11_start);
					}else {
						printk("[%s][size PHY_REG_PG_8814Amp]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814Amp_end - data_PHY_REG_PG_8814Amp_start);
					}
				}else
#endif				
				{
					if(HAL_RFE_TYPE == 0){
						printk("[%s][size PHY_REG_PG_8814Amp_Type0]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814Amp_Type0_end - data_PHY_REG_PG_8814Amp_Type0_start);
					}else if(HAL_RFE_TYPE == 7){
						printk("[%s][size PHY_REG_PG_8814Amp_Type7]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814Amp_Type7_end - data_PHY_REG_PG_8814Amp_Type7_start);
					}else if(HAL_RFE_TYPE == 9){
						printk("[%s][size PHY_REG_PG_8814Amp_Type9]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814Amp_Type9_end - data_PHY_REG_PG_8814Amp_Type9_start);
					}else{
						printk("[%s][size PHY_REG_PG_8814Amp]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814Amp_end - data_PHY_REG_PG_8814Amp_start);
					}
				}			
            }
		}
#endif
#if IS_RTL8197F_SERIES
		if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
#if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				if(HAL_RFE_TYPE == 1){
					panic_printk("[%s][PHY_REG_PG_8197Fmp_Type1] size\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8197Fmp_Type1_end - data_PHY_REG_PG_8197Fmp_Type1_start);
				}else if(HAL_RFE_TYPE == 3){
					panic_printk("[%s][PHY_REG_PG_8197Fmp_Type3] size\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8197Fmp_Type3_end - data_PHY_REG_PG_8197Fmp_Type3_start);
				}else if(HAL_RFE_TYPE == 4){
					panic_printk("[%s][PHY_REG_PG_8197Fmp_Type4] size\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8197Fmp_Type4_end - data_PHY_REG_PG_8197Fmp_Type4_start);
				}else if(HAL_RFE_TYPE == 5){
					panic_printk("[%s][PHY_REG_PG_8197Fmp_Type5] size\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8197Fmp_Type5_end - data_PHY_REG_PG_8197Fmp_Type5_start);
				}else {
					printk("[%s][PHY_REG_PG_8197Fmp] size\n",__FUNCTION__);
          			*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8197Fmp_end - data_PHY_REG_PG_8197Fmp_start);
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
					panic_printk("[%s][PHY_REG_PG_8197Fmp_Type0] size\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8197Fmp_Type0_end - data_PHY_REG_PG_8197Fmp_Type0_start);
				}else if(HAL_RFE_TYPE == 2){
					panic_printk("[%s][PHY_REG_PG_8197Fmp_Type2] size\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8197Fmp_Type2_end - data_PHY_REG_PG_8197Fmp_Type2_start);
				}else if(HAL_RFE_TYPE == 3){
					panic_printk("[%s][PHY_REG_PG_8197Fmp_Type3] size\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8197Fmp_Type3_end - data_PHY_REG_PG_8197Fmp_Type3_start);
				}else if(HAL_RFE_TYPE == 4){
					panic_printk("[%s][PHY_REG_PG_8197Fmp_Type4] size\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8197Fmp_Type4_end - data_PHY_REG_PG_8197Fmp_Type4_start);
				}else if(HAL_RFE_TYPE == 6){
					panic_printk("[%s][PHY_REG_PG_8197Fmp_Type6] size\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8197Fmp_Type6_end - data_PHY_REG_PG_8197Fmp_Type6_start);
				}else{
					printk("[%s][PHY_REG_PG_8197Fmp] size\n",__FUNCTION__);
          			*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8197Fmp_end - data_PHY_REG_PG_8197Fmp_start);
				}
			}			
		}
#endif //IS_RTL8197F_SERIES
#if IS_RTL8198F_SERIES
		if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
#if CFG_HAL_HIGH_POWER_EXT_PA			
				if(HAL_VAR_use_ext_pa){
					if(HAL_RFE_TYPE == 1){
						printk("[%s][PHY_REG_PG_8198Fmp_Type1] size = 0x%x *((pu4Byte)(val))=%x \n",__FUNCTION__,(u4Byte)(data_PHY_REG_PG_8198Fmp_Type1_end - data_PHY_REG_PG_8198Fmp_Type1_start),*((pu4Byte)(val)));
           				u4Byte size = (u4Byte)(data_PHY_REG_PG_8198Fmp_Type1_end - data_PHY_REG_PG_8198Fmp_Type1_start);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8198Fmp_Type1_end - data_PHY_REG_PG_8198Fmp_Type1_start);	
					}else if(HAL_RFE_TYPE == 3){
						printk("[%s][PHY_REG_PG_8198Fmp_Type3] size = 0x%x *((pu4Byte)(val))=%x \n",__FUNCTION__,(u4Byte)(data_PHY_REG_PG_8198Fmp_Type3_end - data_PHY_REG_PG_8198Fmp_Type3_start),*((pu4Byte)(val)));
           				u4Byte size = (u4Byte)(data_PHY_REG_PG_8198Fmp_Type3_end - data_PHY_REG_PG_8198Fmp_Type3_start);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8198Fmp_Type3_end - data_PHY_REG_PG_8198Fmp_Type3_start);	
					}else {
						printk("[%s][PHY_REG_PG_8198Fmp] size = 0x%x *((pu4Byte)(val))=%x \n",__FUNCTION__,(u4Byte)(data_PHY_REG_PG_8198Fmp_end - data_PHY_REG_PG_8198Fmp_start),*((pu4Byte)(val)));
           				u4Byte size = (u4Byte)(data_PHY_REG_PG_8198Fmp_end - data_PHY_REG_PG_8198Fmp_start);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8198Fmp_end - data_PHY_REG_PG_8198Fmp_start);
					}
				}else
#endif				
				{
					if(HAL_RFE_TYPE == 0){
						printk("[%s][PHY_REG_PG_8198Fmp_Type0] size = 0x%x *((pu4Byte)(val))=%x \n",__FUNCTION__,(u4Byte)(data_PHY_REG_PG_8198Fmp_Type0_end - data_PHY_REG_PG_8198Fmp_Type0_start),*((pu4Byte)(val)));
						u4Byte size = (u4Byte)(data_PHY_REG_PG_8198Fmp_Type0_end - data_PHY_REG_PG_8198Fmp_Type0_start);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8198Fmp_Type0_end - data_PHY_REG_PG_8198Fmp_Type0_start);
					}else if(HAL_RFE_TYPE == 2){
						printk("[%s][PHY_REG_PG_8198Fmp_Type2] size = 0x%x *((pu4Byte)(val))=%x \n",__FUNCTION__,(u4Byte)(data_PHY_REG_PG_8198Fmp_Type2_end - data_PHY_REG_PG_8198Fmp_Type2_start),*((pu4Byte)(val)));
						u4Byte size = (u4Byte)(data_PHY_REG_PG_8198Fmp_Type2_end - data_PHY_REG_PG_8198Fmp_Type2_start);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8198Fmp_Type2_end - data_PHY_REG_PG_8198Fmp_Type2_start);
					}else {
						printk("[%s][PHY_REG_PG_8198Fmp] size = 0x%x *((pu4Byte)(val))=%x \n",__FUNCTION__,(u4Byte)(data_PHY_REG_PG_8198Fmp_end - data_PHY_REG_PG_8198Fmp_start),*((pu4Byte)(val)));
           				u4Byte size = (u4Byte)(data_PHY_REG_PG_8198Fmp_end - data_PHY_REG_PG_8198Fmp_start);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8198Fmp_end - data_PHY_REG_PG_8198Fmp_start);		
					}
				}	
		}
#endif //IS_RTL8198F_SERIES

#if IS_RTL8197G_SERIES
		if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
#if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				{
					printk("[%s][PHY_REG_PG_8197Gmp] size = 0x%x *((pu4Byte)(val))=%x \n",__FUNCTION__,(u4Byte)(data_PHY_REG_PG_8197Gmp_end - data_PHY_REG_PG_8197Gmp_start),*((pu4Byte)(val)));
					u4Byte size = (u4Byte)(data_PHY_REG_PG_8197Gmp_end - data_PHY_REG_PG_8197Gmp_start);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8197Gmp_end - data_PHY_REG_PG_8197Gmp_start);
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
						printk("[%s][PHY_REG_PG_8197Gmp_Type0] size = 0x%x *((pu4Byte)(val))=%x \n",__FUNCTION__,(u4Byte)(data_PHY_REG_PG_8197Gmp_Type0_end - data_PHY_REG_PG_8197Gmp_Type0_start),*((pu4Byte)(val)));
						u4Byte size = (u4Byte)(data_PHY_REG_PG_8197Gmp_Type0_end - data_PHY_REG_PG_8197Gmp_Type0_start);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8197Gmp_Type0_end - data_PHY_REG_PG_8197Gmp_Type0_start);
				}else {
					printk("[%s][PHY_REG_PG_8197Gmp] size = 0x%x *((pu4Byte)(val))=%x \n",__FUNCTION__,(u4Byte)(data_PHY_REG_PG_8197Gmp_end - data_PHY_REG_PG_8197Gmp_start),*((pu4Byte)(val)));
       				u4Byte size = (u4Byte)(data_PHY_REG_PG_8197Gmp_end - data_PHY_REG_PG_8197Gmp_start);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8197Gmp_end - data_PHY_REG_PG_8197Gmp_start);		
				}
			}	
		}
#endif //IS_RTL8197G_SERIES

#if IS_RTL8822B_SERIES
		if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				//printk("[%s]HW_VAR_PHYREGFILE_PG_SIZE\n",__FUNCTION__);
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822B_end - data_PHY_REG_PG_8822B_start);
			} else { // mp chip
#if CFG_HAL_HIGH_POWER_EXT_PA			
				if(HAL_VAR_use_ext_pa){
					if(HAL_RFE_TYPE == 1){
						printk("[%s][size PHY_REG_PG_8822Bmp_Type1]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Bmp_Type1_end - data_PHY_REG_PG_8822Bmp_Type1_start);
					}else if(HAL_RFE_TYPE == 4){
						printk("[%s][size PHY_REG_PG_8822Bmp_Type4]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Bmp_Type4_end - data_PHY_REG_PG_8822Bmp_Type4_start);
					}else if(HAL_RFE_TYPE == 6){
						printk("[%s][size PHY_REG_PG_8822Bmp_Type6]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Bmp_Type6_end - data_PHY_REG_PG_8822Bmp_Type6_start);
					}else if(HAL_RFE_TYPE == 7){
						printk("[%s][size PHY_REG_PG_8822Bmp_Type7]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Bmp_Type7_end - data_PHY_REG_PG_8822Bmp_Type7_start);
					}else if(HAL_RFE_TYPE == 11){
						printk("[%s][size PHY_REG_PG_8822Bmp_Type11]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Bmp_Type11_end - data_PHY_REG_PG_8822Bmp_Type11_start);
					}else {
						printk("[%s][size PHY_REG_PG_8822Bmp]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Bmp_end - data_PHY_REG_PG_8822Bmp_start);
					}
				}else
#endif				
				{
					if(HAL_RFE_TYPE == 0){
						printk("[%s][size PHY_REG_PG_8822Bmp_Type0]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Bmp_Type0_end - data_PHY_REG_PG_8822Bmp_Type0_start);
					}else if(HAL_RFE_TYPE == 8){
						printk("[%s][size PHY_REG_PG_8822Bmp_Type8]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Bmp_Type8_end - data_PHY_REG_PG_8822Bmp_Type8_start);
					}else if(HAL_RFE_TYPE == 9){
						printk("[%s][size PHY_REG_PG_8822Bmp_Type9]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Bmp_Type9_end - data_PHY_REG_PG_8822Bmp_Type9_start);
					}else if(HAL_RFE_TYPE == 10){
						printk("[%s][size PHY_REG_PG_8822Bmp_Type10]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Bmp_Type10_end - data_PHY_REG_PG_8822Bmp_Type10_start);
					}else if(HAL_RFE_TYPE == 13){
						printk("[%s][size PHY_REG_PG_8822Bmp_Type13]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Bmp_Type13_end - data_PHY_REG_PG_8822Bmp_Type13_start);
					}else if(HAL_RFE_TYPE == 14){
						printk("[%s][size PHY_REG_PG_8822Bmp_Type14]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Bmp_Type13_end - data_PHY_REG_PG_8822Bmp_Type14_start);
					}else{
						printk("[%s][size PHY_REG_PG_8822Bmp]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Bmp_end - data_PHY_REG_PG_8822Bmp_start);
					}
				}

			}
		}
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
		if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				//printk("[%s]HW_VAR_PHYREGFILE_PG_SIZE\n",__FUNCTION__);
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822C_end - data_PHY_REG_PG_8822C_start);
			} else { // mp chip
#if CFG_HAL_HIGH_POWER_EXT_PA			
				if(HAL_VAR_use_ext_pa){
					if(HAL_RFE_TYPE == 1){
						printk("[%s][size PHY_REG_PG_8822Cmp_Type1]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Cmp_Type1_end - data_PHY_REG_PG_8822Cmp_Type1_start);
					}else if(HAL_RFE_TYPE == 4){
						printk("[%s][size PHY_REG_PG_8822Cmp_Type4]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Cmp_Type4_end - data_PHY_REG_PG_8822Cmp_Type4_start);
					}else if(HAL_RFE_TYPE == 6){
						printk("[%s][size PHY_REG_PG_8822Cmp_Type6]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Cmp_Type6_end - data_PHY_REG_PG_8822Cmp_Type6_start);
					}else if(HAL_RFE_TYPE == 7){
						printk("[%s][size PHY_REG_PG_8822Cmp_Type7]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Cmp_Type7_end - data_PHY_REG_PG_8822Cmp_Type7_start);
					}else if(HAL_RFE_TYPE == 11){
						printk("[%s][size PHY_REG_PG_8822Cmp_Type11]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Cmp_Type11_end - data_PHY_REG_PG_8822Cmp_Type11_start);
					}else {
						printk("[%s][size PHY_REG_PG_8822Cmp]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Cmp_end - data_PHY_REG_PG_8822Cmp_start);
					}
				}else
#endif				
				{
					if(HAL_RFE_TYPE == 0){
						printk("[%s][size PHY_REG_PG_8822Cmp_Type0]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Cmp_Type0_end - data_PHY_REG_PG_8822Cmp_Type0_start);
					}else if(HAL_RFE_TYPE == 8){
						printk("[%s][size PHY_REG_PG_8822Cmp_Type8]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Cmp_Type8_end - data_PHY_REG_PG_8822Cmp_Type8_start);
					}else if(HAL_RFE_TYPE == 9){
						printk("[%s][size PHY_REG_PG_8822Cmp_Type9]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Cmp_Type9_end - data_PHY_REG_PG_8822Cmp_Type9_start);
					}else if(HAL_RFE_TYPE == 10){
						printk("[%s][size PHY_REG_PG_8822Cmp_Type10]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Cmp_Type10_end - data_PHY_REG_PG_8822Cmp_Type10_start);
					}else if(HAL_RFE_TYPE == 13){
						printk("[%s][size PHY_REG_PG_8822Cmp_Type13]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Cmp_Type13_end - data_PHY_REG_PG_8822Cmp_Type13_start);
					}else if(HAL_RFE_TYPE == 14){
						printk("[%s][size PHY_REG_PG_8822Cmp_Type14]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Cmp_Type13_end - data_PHY_REG_PG_8822Cmp_Type14_start);
					}else{
						printk("[%s][size PHY_REG_PG_8822Cmp]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8822Cmp_end - data_PHY_REG_PG_8822Cmp_start);
					}
				}

			}
		}
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
		if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				//printk("[%s]HW_VAR_PHYREGFILE_PG_SIZE\n",__FUNCTION__);
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8812F_end - data_PHY_REG_PG_8812F_start);
			} else { // mp chip
#if CFG_HAL_HIGH_POWER_EXT_PA			
				if(HAL_VAR_use_ext_pa){
					if(HAL_RFE_TYPE == 1){
						printk("[%s][size PHY_REG_PG_8812Fmp_Type1]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8812Fmp_Type1_end - data_PHY_REG_PG_8812Fmp_Type1_start);
					}else if(HAL_RFE_TYPE == 3){
						printk("[%s][size PHY_REG_PG_8812Fmp_Type3]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8812Fmp_Type3_end - data_PHY_REG_PG_8812Fmp_Type3_start);
					}else if(HAL_RFE_TYPE == 4){
						printk("[%s][size PHY_REG_PG_8812Fmp_Type4]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8812Fmp_Type4_end - data_PHY_REG_PG_8812Fmp_Type4_start);
					}else if(HAL_RFE_TYPE == 6){
						printk("[%s][size PHY_REG_PG_8812Fmp_Type6]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8812Fmp_Type6_end - data_PHY_REG_PG_8812Fmp_Type6_start);
					}else if(HAL_RFE_TYPE == 7){
						printk("[%s][size PHY_REG_PG_8812Fmp_Type7]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8812Fmp_Type7_end - data_PHY_REG_PG_8812Fmp_Type7_start);
					}else if(HAL_RFE_TYPE == 11){
						printk("[%s][size PHY_REG_PG_8812Fmp_Type11]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8812Fmp_Type11_end - data_PHY_REG_PG_8812Fmp_Type11_start);
					}else {
						printk("[%s][size PHY_REG_PG_8812Fmp]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8812Fmp_end - data_PHY_REG_PG_8812Fmp_start);
					}
				}else
#endif				
				{
					if(HAL_RFE_TYPE == 0){
						printk("[%s][size PHY_REG_PG_8812Fmp_Type0]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8812Fmp_Type0_end - data_PHY_REG_PG_8812Fmp_Type0_start);
					}else if(HAL_RFE_TYPE == 2){
						printk("[%s][size PHY_REG_PG_8812Fmp_Type2]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8812Fmp_Type2_end - data_PHY_REG_PG_8812Fmp_Type2_start);
					}else if(HAL_RFE_TYPE == 8){
						printk("[%s][size PHY_REG_PG_8812Fmp_Type8]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8812Fmp_Type8_end - data_PHY_REG_PG_8812Fmp_Type8_start);
					}else if(HAL_RFE_TYPE == 9){
						printk("[%s][size PHY_REG_PG_8812Fmp_Type9]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8812Fmp_Type9_end - data_PHY_REG_PG_8812Fmp_Type9_start);
					}else if(HAL_RFE_TYPE == 10){
						printk("[%s][size PHY_REG_PG_8812Fmp_Type10]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8812Fmp_Type10_end - data_PHY_REG_PG_8812Fmp_Type10_start);
					}else if(HAL_RFE_TYPE == 13){
						printk("[%s][size PHY_REG_PG_8812Fmp_Type13]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8812Fmp_Type13_end - data_PHY_REG_PG_8812Fmp_Type13_start);
					}else if(HAL_RFE_TYPE == 14){
						printk("[%s][size PHY_REG_PG_8812Fmp_Type14]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8812Fmp_Type13_end - data_PHY_REG_PG_8812Fmp_Type14_start);
					}else{
						printk("[%s][size PHY_REG_PG_8812Fmp]\n",__FUNCTION__);
						*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8812Fmp_end - data_PHY_REG_PG_8812Fmp_start);
					}
				}

			}
		}
#endif //IS_RTL8812F_SERIES

#if IS_RTL8821C_SERIES
		if ( IS_HARDWARE_TYPE_8821C(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				//printk("[%s]HW_VAR_PHYREGFILE_PG_SIZE\n",__FUNCTION__);
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8821C_end - data_PHY_REG_PG_8821C_start);
			} else { // mp chip
				if(HAL_RFE_TYPE == 0){
					printk("[%s][size PHY_REG_PG_8821Cmp_Type0]\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8821Cmp_Type0_end - data_PHY_REG_PG_8821Cmp_Type0_start);
				}else{
					printk("[%s][size PHY_REG_PG_8821Cmp]\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8821Cmp_end - data_PHY_REG_PG_8821Cmp_start);
				}
			}
		}
#endif //IS_RTL8821C_SERIES

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) { //yllin8814B
            #if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				if(HAL_RFE_TYPE == 1){
					printk("[%s][PHY_REG_PG_8814Bmp_Type1] size = 0x%x *((pu4Byte)(val))=%x \n",__FUNCTION__,(u4Byte)(data_PHY_REG_PG_8814Bmp_Type1_end - data_PHY_REG_PG_8814Bmp_Type1_start),*((pu4Byte)(val)));
					u4Byte size = (u4Byte)(data_PHY_REG_PG_8814Bmp_Type1_end - data_PHY_REG_PG_8814Bmp_Type1_start);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814Bmp_Type1_end - data_PHY_REG_PG_8814Bmp_Type1_start);
				}else if(HAL_RFE_TYPE == 4){
					printk("[%s][PHY_REG_PG_8814Bmp_Type4] size = 0x%x *((pu4Byte)(val))=%x \n",__FUNCTION__,(u4Byte)(data_PHY_REG_PG_8814Bmp_Type4_end - data_PHY_REG_PG_8814Bmp_Type4_start),*((pu4Byte)(val)));
					u4Byte size = (u4Byte)(data_PHY_REG_PG_8814Bmp_Type4_end - data_PHY_REG_PG_8814Bmp_Type4_start);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814Bmp_Type4_end - data_PHY_REG_PG_8814Bmp_Type4_start);
				}else if(HAL_RFE_TYPE == 5){
					printk("[%s][PHY_REG_PG_8814Bmp_Type5] size = 0x%x *((pu4Byte)(val))=%x \n",__FUNCTION__,(u4Byte)(data_PHY_REG_PG_8814Bmp_Type5_end - data_PHY_REG_PG_8814Bmp_Type5_start),*((pu4Byte)(val)));
					u4Byte size = (u4Byte)(data_PHY_REG_PG_8814Bmp_Type5_end - data_PHY_REG_PG_8814Bmp_Type5_start);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814Bmp_Type5_end - data_PHY_REG_PG_8814Bmp_Type5_start);
				}else {
					printk("[%s][PHY_REG_PG_8814Bmp] size = 0x%x *((pu4Byte)(val))=%x \n",__FUNCTION__,(u4Byte)(data_PHY_REG_PG_8814Bmp_end - data_PHY_REG_PG_8814Bmp_start),*((pu4Byte)(val)));
					u4Byte size = (u4Byte)(data_PHY_REG_PG_8814Bmp_end - data_PHY_REG_PG_8814Bmp_start);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814Bmp_end - data_PHY_REG_PG_8814Bmp_start);
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
					printk("[%s][PHY_REG_PG_8814Bmp_Type0] size = 0x%x *((pu4Byte)(val))=%x \n",__FUNCTION__,(u4Byte)(data_PHY_REG_PG_8814Bmp_Type0_end - data_PHY_REG_PG_8814Bmp_Type0_start),*((pu4Byte)(val)));
					u4Byte size = (u4Byte)(data_PHY_REG_PG_8814Bmp_Type0_end - data_PHY_REG_PG_8814Bmp_Type0_start);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814Bmp_Type0_end - data_PHY_REG_PG_8814Bmp_Type0_start);
				}else if(HAL_RFE_TYPE == 2){
					printk("[%s][PHY_REG_PG_8814Bmp_Type2] size = 0x%x *((pu4Byte)(val))=%x \n",__FUNCTION__,(u4Byte)(data_PHY_REG_PG_8814Bmp_Type2_end - data_PHY_REG_PG_8814Bmp_Type2_start),*((pu4Byte)(val)));
					u4Byte size = (u4Byte)(data_PHY_REG_PG_8814Bmp_Type2_end - data_PHY_REG_PG_8814Bmp_Type2_start);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814Bmp_Type2_end - data_PHY_REG_PG_8814Bmp_Type2_start);
				}else if(HAL_RFE_TYPE == 7){
					printk("[%s][PHY_REG_PG_8814Bmp_Type7] size = 0x%x *((pu4Byte)(val))=%x \n",__FUNCTION__,(u4Byte)(data_PHY_REG_PG_8814Bmp_Type7_end - data_PHY_REG_PG_8814Bmp_Type7_start),*((pu4Byte)(val)));
					u4Byte size = (u4Byte)(data_PHY_REG_PG_8814Bmp_Type7_end - data_PHY_REG_PG_8814Bmp_Type7_start);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814Bmp_Type7_end - data_PHY_REG_PG_8814Bmp_Type7_start);
				}else if(HAL_RFE_TYPE == 8){
					printk("[%s][PHY_REG_PG_8814Bmp_Type8] size = 0x%x *((pu4Byte)(val))=%x \n",__FUNCTION__,(u4Byte)(data_PHY_REG_PG_8814Bmp_Type8_end - data_PHY_REG_PG_8814Bmp_Type8_start),*((pu4Byte)(val)));
					u4Byte size = (u4Byte)(data_PHY_REG_PG_8814Bmp_Type8_end - data_PHY_REG_PG_8814Bmp_Type8_start);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814Bmp_Type8_end - data_PHY_REG_PG_8814Bmp_Type8_start);
				}else {
					printk("[%s][PHY_REG_PG_8814Bmp] size = 0x%x *((pu4Byte)(val))=%x \n",__FUNCTION__,(u4Byte)(data_PHY_REG_PG_8814Bmp_end - data_PHY_REG_PG_8814Bmp_start),*((pu4Byte)(val)));
					u4Byte size = (u4Byte)(data_PHY_REG_PG_8814Bmp_end - data_PHY_REG_PG_8814Bmp_start);
					*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8814Bmp_end - data_PHY_REG_PG_8814Bmp_start);		
				}
			}	

			///printk("[%s][PHY_REG_PG_8814Bmp] size = 0x%x *((pu4Byte)(val))=%x \n",__FUNCTION__,(u4Byte)(data_PHY_REG_PG_8814Bmp_end - data_PHY_REG_PG_8814Bmp_start),*((pu4Byte)(val)));
            ///u4Byte size = (u4Byte)(data_PHY_REG_PG_8814Bmp_end - data_PHY_REG_PG_8814Bmp_start);
            //*((pu4Byte)(val)) = size; //(u4Byte)(data_PHY_REG_PG_8198Fmp_end - data_PHY_REG_PG_8198Fmp_start);
            //printk("%s %d val =%x %x %x !!!! \n",__func__,__LINE__,val,data_PHY_REG_PG_8198Fmp_end,data_PHY_REG_PG_8198Fmp_start);        
            //3 need other phy reg
        }
#endif //IS_RTL8814B_SERIES

#if IS_RTL8192F_SERIES
#if (CONFIG_WLAN_CONF_TXT_EXIST==1)
		if ( IS_HARDWARE_TYPE_8192F(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8192F_end - data_PHY_REG_PG_8192F_start);
			} else { // mp chip
				//printk("Get PHY_REG_PG_8192Fmp size\n");
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8192Fmp_end - data_PHY_REG_PG_8192Fmp_start);
			}
		}
#endif
#endif
	}
	break;
#ifdef PWR_BY_RATE_92E_HP
#if (CFG_HAL_HIGH_POWER_EXT_PA | CFG_HAL_HIGH_POWER_EXT_LNA)
	case HW_VAR_PHYREGFILE_PG_HP_START: {
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				printk("[PHY_REG_PG_8192E]\n");
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8192E_start;
			} else { // mp chip
				*((unsigned long*)(val)) = (unsigned long)data_PHY_REG_PG_8192Emp_hp_start;
				printk("[PHY_REG_PG_8192Emp_hp]\n");
			}
		}
#endif
	}
	break;

	case HW_VAR_PHYREGFILE_PG_HP_SIZE: {
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8192E_end - data_PHY_REG_PG_8192E_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_PHY_REG_PG_8192Emp_hp_end - data_PHY_REG_PG_8192Emp_hp_start);
			}
		}
#endif
	}
	break;
#endif //CFG_HAL_HIGH_POWER_EXT_PA
#endif // PWR_BY_RATE_92E_HP			

	case HW_VAR_POWERLIMITFILE_START: {
#ifdef TXPWR_LMT_8881A
#if IS_RTL8881A_SERIES
		if (IS_HARDWARE_TYPE_8881A(Adapter)) {
			if (get_bonding_type_8881A() == BOND_8881AM) {
				if (HAL_VAR_INTERLPA_8881A) {
					printk("[TXPWR_LMT_8881AMP_intpa]\n");
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8881AMP_intpa_start;
				} else {
					printk("[TXPWR_LMT_8881AMP]\n");
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8881AMP_start;
				}
			} else if (get_bonding_type_8881A() == BOND_8881AB) {
				if (HAL_VAR_INTERLPA_8881A) {
					printk("[TXPWR_LMT_8881ABP_intpa]\n");
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8881ABP_intpa_start;
				} else {
					printk("[TXPWR_LMT_8881ABP]\n");
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8881ABP_start;
				}
			} else if (get_bonding_type_8881A() == BOND_8881AN) {
				if (HAL_VAR_INTERLPA_8881A) {
					printk("[TXPWR_LMT_8881AN]\n");
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8881AN_start;
				} else {
					printk("[TXPWR_LMT_8881AN_extpa]\n");
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8881AN_extpa_start;
				}
			} else {
				printk("[TXPWR_LMT_8881A_new]\n");
				*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8881A_new_start;
			}
		}
#endif
#endif
#ifdef TXPWR_LMT_92EE
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			printk("[TXPWR_LMT_92EE_new]\n");
			*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_92EE_new_start;
		}
#endif
#endif
#ifdef TXPWR_LMT_8197F
#if IS_RTL8197F_SERIES
		if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
#ifdef BEAMFORMING_AUTO
			if(Adapter->pshare->rf_ft_var.txbf_pwrlmt == TXBF_TXPWRLMT_ENABLE) {
				panic_printk("[%s][TXPWR_LMT_8197Fmp_TXBF]\n",__FUNCTION__);
				*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8197Fmp_TXBF_start;
			}
			else
#endif			
#if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				if(HAL_RFE_TYPE == 1){
					panic_printk("[%s][TXPWR_LMT_8197Fmp_Type1]\n",__FUNCTION__);
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8197Fmp_Type1_start;
				}else if(HAL_RFE_TYPE == 3){
					panic_printk("[%s][TXPWR_LMT_8197Fmp_Type3]\n",__FUNCTION__);
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8197Fmp_Type3_start;
				}else if(HAL_RFE_TYPE == 4){
					panic_printk("[%s][TXPWR_LMT_8197Fmp_Type4]\n",__FUNCTION__);
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8197Fmp_Type4_start;
				}else if(HAL_RFE_TYPE == 5){
					panic_printk("[%s][TXPWR_LMT_8197Fmp_Type5]\n",__FUNCTION__);
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8197Fmp_Type5_start;
				}else {
					panic_printk("[TXPWR_LMT_8197Fmp]\n");
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8197Fmp_start;
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
					panic_printk("[TXPWR_LMT_8197Fmp_Type0]\n");
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8197Fmp_Type0_start;
				}else if(HAL_RFE_TYPE == 2){
					panic_printk("[TXPWR_LMT_8197Fmp_Type2]\n");
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8197Fmp_Type2_start;
				}else if(HAL_RFE_TYPE == 3){
					panic_printk("[%s][TXPWR_LMT_8197Fmp_Type3]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8197Fmp_Type3_start;
				}else if(HAL_RFE_TYPE == 4){
					panic_printk("[%s][TXPWR_LMT_8197Fmp_Type4]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8197Fmp_Type4_start;
				}else if(HAL_RFE_TYPE == 6){
					panic_printk("[%s][TXPWR_LMT_8197Fmp_Type6]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8197Fmp_Type6_start;
				}else{
					panic_printk("[TXPWR_LMT_8197Fmp]\n");
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8197Fmp_start;
				}
			}				
		}
#endif //#if IS_RTL8197F_SERIES
#endif //#ifdef TXPWR_LMT_8197F

#ifdef TXPWR_LMT_8198F
#if IS_RTL8198F_SERIES
    if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
#if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				if(HAL_RFE_TYPE == 1){
					panic_printk("[TXPWR_LMT_8198Fmp_Type1]\n");
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8198Fmp_Type1_start;
				}else if(HAL_RFE_TYPE == 3){
					panic_printk("[TXPWR_LMT_8198Fmp_Type3]\n");
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8198Fmp_Type3_start;
				}else{
				  	panic_printk("[TXPWR_LMT_8198Fmp]\n");
       			  	*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8198Fmp_start;
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
					panic_printk("[TXPWR_LMT_8198Fmp_Type0]\n");
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8198Fmp_Type0_start;
				}else if(HAL_RFE_TYPE == 2){
					panic_printk("[TXPWR_LMT_8198Fmp_Type2]\n");
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8198Fmp_Type2_start;
				}else {
					panic_printk("[TXPWR_LMT_8198Fmp]\n");
       				 *((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8198Fmp_start;
				}
			}				
    }
#endif //#if IS_RTL8198F_SERIES
#endif //#ifdef TXPWR_LMT_8198F

#ifdef TXPWR_LMT_8197G
#if IS_RTL8197G_SERIES
    if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
#if CFG_HAL_HIGH_POWER_EXT_PA			
		if(HAL_VAR_use_ext_pa){
			  panic_printk("[TXPWR_LMT_8197Gmp]\n");
			  *((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8197Gmp_start;
		}else
#endif				
		{
			if(HAL_RFE_TYPE == 0){
				panic_printk("[TXPWR_LMT_8197Gmp_Type0]\n");
				*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8197Gmp_Type0_start;
			}else {
				panic_printk("[TXPWR_LMT_8197Gmp]\n");
				 *((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8197Gmp_start;
			}
		}
    }
#endif //#if IS_RTL8197G_SERIES
#endif //#ifdef TXPWR_LMT_8197G

#ifdef TXPWR_LMT_8822B
#if IS_RTL8822B_SERIES
		if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
#ifdef BEAMFORMING_AUTO
			if(Adapter->pshare->rf_ft_var.txbf_pwrlmt == TXBF_TXPWRLMT_ENABLE) {
				panic_printk("[%s][TXPWR_LMT_8822Bmp_TXBF]\n",__FUNCTION__);
				*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Bmp_TXBF_start;
			}
			else
#endif					
#if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				if(HAL_RFE_TYPE == 1){
					panic_printk("[%s][TXPWR_LMT_8822Bmp_Type1]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Bmp_Type1_start;
				}else if(HAL_RFE_TYPE == 4){
					panic_printk("[%s][TXPWR_LMT_8822Bmp_Type4]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Bmp_Type4_start;
				}else if(HAL_RFE_TYPE == 6){
					panic_printk("[%s][TXPWR_LMT_8822Bmp_Type6]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Bmp_Type6_start;
				}else if(HAL_RFE_TYPE == 7){
					panic_printk("[%s][TXPWR_LMT_8822Bmp_Type7]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Bmp_Type7_start;
				}else if(HAL_RFE_TYPE == 11){
					panic_printk("[%s][TXPWR_LMT_8822Bmp_Type11]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Bmp_Type11_start;
				}else {
					panic_printk("[%s][TXPWR_LMT_8822Bmp]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Bmp_start;
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
					panic_printk("[%s][TXPWR_LMT_8822Bmp_Type0]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Bmp_Type0_start;
				}else if (HAL_RFE_TYPE == 8){
					panic_printk("[%s][TXPWR_LMT_8822Bmp_TYPE8]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Bmp_Type8_start;
				}else if (HAL_RFE_TYPE == 9){
					panic_printk("[%s][TXPWR_LMT_8822Bmp_TYPE9]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Bmp_Type9_start;
				}else if (HAL_RFE_TYPE == 10){
					panic_printk("[%s][TXPWR_LMT_8822Bmp_TYPE10]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Bmp_Type10_start;
				}else if (HAL_RFE_TYPE == 13){
					panic_printk("[%s][TXPWR_LMT_8822Bmp_TYPE13]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Bmp_Type13_start;
				}else if (HAL_RFE_TYPE == 14){
					panic_printk("[%s][TXPWR_LMT_8822Bmp_TYPE14]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Bmp_Type14_start;
				}else{
					panic_printk("[%s][TXPWR_LMT_8822Bmp]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Bmp_start;
				}
			}				
		}
#endif
#endif

#ifdef TXPWR_LMT_8822C
#if IS_RTL8822C_SERIES
		if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
#ifdef BEAMFORMING_AUTO
			if(Adapter->pshare->rf_ft_var.txbf_pwrlmt == TXBF_TXPWRLMT_ENABLE) {
				panic_printk("[%s][TXPWR_LMT_8822Cmp_TXBF]\n",__FUNCTION__);
				*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Cmp_TXBF_start;
			}
			else
#endif					
#if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				if(HAL_RFE_TYPE == 1){
					panic_printk("[%s][TXPWR_LMT_8822Cmp_Type1]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Cmp_Type1_start;
				}else if(HAL_RFE_TYPE == 4){
					panic_printk("[%s][TXPWR_LMT_8822Cmp_Type4]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Cmp_Type4_start;
				}else if(HAL_RFE_TYPE == 6){
					panic_printk("[%s][TXPWR_LMT_8822Cmp_Type6]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Cmp_Type6_start;
				}else if(HAL_RFE_TYPE == 7){
					panic_printk("[%s][TXPWR_LMT_8822Cmp_Type7]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Cmp_Type7_start;
				}else if(HAL_RFE_TYPE == 11){
					panic_printk("[%s][TXPWR_LMT_8822Cmp_Type11]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Cmp_Type11_start;
				}else {
					panic_printk("[%s][TXPWR_LMT_8822Cmp]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Cmp_start;
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
					panic_printk("[%s][TXPWR_LMT_8822Cmp_Type0]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Cmp_Type0_start;
				}else if (HAL_RFE_TYPE == 8){
					panic_printk("[%s][TXPWR_LMT_8822Cmp_TYPE8]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Cmp_Type8_start;
				}else if (HAL_RFE_TYPE == 9){
					panic_printk("[%s][TXPWR_LMT_8822Cmp_TYPE9]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Cmp_Type9_start;
				}else if (HAL_RFE_TYPE == 10){
					panic_printk("[%s][TXPWR_LMT_8822Cmp_TYPE10]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Cmp_Type10_start;
				}else if (HAL_RFE_TYPE == 13){
					panic_printk("[%s][TXPWR_LMT_8822Cmp_TYPE13]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Cmp_Type13_start;
				}else if (HAL_RFE_TYPE == 14){
					panic_printk("[%s][TXPWR_LMT_8822Cmp_TYPE14]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Cmp_Type14_start;
				}else{
					panic_printk("[%s][TXPWR_LMT_8822Cmp]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8822Cmp_start;
				}
			}				
		}
#endif
#endif

#ifdef TXPWR_LMT_8812F
#if IS_RTL8812F_SERIES
		if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
#ifdef BEAMFORMING_AUTO
			if(Adapter->pshare->rf_ft_var.txbf_pwrlmt == TXBF_TXPWRLMT_ENABLE) {
				panic_printk("[%s][TXPWR_LMT_8812Fmp_TXBF]\n",__FUNCTION__);
				*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8812Fmp_TXBF_start;
			}
			else
#endif					
#if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				if(HAL_RFE_TYPE == 1){
					panic_printk("[%s][TXPWR_LMT_8812Fmp_Type1]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8812Fmp_Type1_start;
				}else if(HAL_RFE_TYPE == 3){
					panic_printk("[%s][TXPWR_LMT_8812Fmp_Type3]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8812Fmp_Type3_start;
				}else if(HAL_RFE_TYPE == 4){
					panic_printk("[%s][TXPWR_LMT_8812Fmp_Type4]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8812Fmp_Type4_start;
				}else if(HAL_RFE_TYPE == 6){
					panic_printk("[%s][TXPWR_LMT_8812Fmp_Type6]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8812Fmp_Type6_start;
				}else if(HAL_RFE_TYPE == 7){
					panic_printk("[%s][TXPWR_LMT_8812Fmp_Type7]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8812Fmp_Type7_start;
				}else if(HAL_RFE_TYPE == 11){
					panic_printk("[%s][TXPWR_LMT_8812Fmp_Type11]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8812Fmp_Type11_start;
				}else {
					panic_printk("[%s][TXPWR_LMT_8812Fmp]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8812Fmp_start;
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
					panic_printk("[%s][TXPWR_LMT_8812Fmp_Type0]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8812Fmp_Type0_start;
				}else if(HAL_RFE_TYPE == 2){
					panic_printk("[%s][TXPWR_LMT_8812Fmp_Type2]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8812Fmp_Type2_start;
				}else if (HAL_RFE_TYPE == 8){
					panic_printk("[%s][TXPWR_LMT_8812Fmp_TYPE8]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8812Fmp_Type8_start;
				}else if (HAL_RFE_TYPE == 9){
					panic_printk("[%s][TXPWR_LMT_8812Fmp_TYPE9]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8812Fmp_Type9_start;
				}else if (HAL_RFE_TYPE == 10){
					panic_printk("[%s][TXPWR_LMT_8812Fmp_TYPE10]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8812Fmp_Type10_start;
				}else if (HAL_RFE_TYPE == 13){
					panic_printk("[%s][TXPWR_LMT_8812Fmp_TYPE13]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8812Fmp_Type13_start;
				}else if (HAL_RFE_TYPE == 14){
					panic_printk("[%s][TXPWR_LMT_8812Fmp_TYPE14]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8812Fmp_Type14_start;
				}else{
					panic_printk("[%s][TXPWR_LMT_8812Fmp]\n",__FUNCTION__);
					*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8812Fmp_start;
				}
			}				
		}
#endif
#endif

#ifdef TXPWR_LMT_8814B
#if IS_RTL8814B_SERIES
    if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
#if CFG_HAL_HIGH_POWER_EXT_PA			
		if(HAL_VAR_use_ext_pa){
			if(HAL_RFE_TYPE == 1) {
				panic_printk("[%s][TXPWR_LMT_8814Bmp_Type1]\n",__FUNCTION__);
				*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8814Bmp_Type1_start;
			}else if(HAL_RFE_TYPE == 4) {
				panic_printk("[%s][TXPWR_LMT_8814Bmp_Type4]\n",__FUNCTION__);
				*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8814Bmp_Type4_start;
			}else if(HAL_RFE_TYPE == 5) {
				panic_printk("[%s][TXPWR_LMT_8814Bmp_Type5]\n",__FUNCTION__);
				*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8814Bmp_Type5_start;
			}else {
				panic_printk("[%s][TXPWR_LMT_8814Bmp]\n",__FUNCTION__);
				*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8814Bmp_start;
			}
		}else
#endif				
		{
			if(HAL_RFE_TYPE == 2) {
				panic_printk("[%s][TXPWR_LMT_8814Bmp_Type2]\n",__FUNCTION__);
				*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8814Bmp_Type2_start;
			}else if(HAL_RFE_TYPE == 7) {
				panic_printk("[%s][TXPWR_LMT_8814Bmp_Type7]\n",__FUNCTION__);
				*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8814Bmp_Type7_start;
			}else if(HAL_RFE_TYPE == 8) {
				panic_printk("[%s][TXPWR_LMT_8814Bmp_Type8]\n",__FUNCTION__);
				*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8814Bmp_Type8_start;
			}else{
				panic_printk("[%s][TXPWR_LMT_8814Bmp]\n",__FUNCTION__);
				*((dma_addr_t*)(val)) = (dma_addr_t)data_TXPWR_LMT_8814Bmp_start;
			}
		}

	}
#endif //#if IS_RTL8814B_SERIES
#endif //#ifdef TXPWR_LMT_8814B

#ifdef TXPWR_LMT_8814A
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
#ifdef BEAMFORMING_AUTO
			if(Adapter->pshare->rf_ft_var.txbf_pwrlmt == TXBF_TXPWRLMT_ENABLE) {
				panic_printk("[%s][TXPWR_LMT_8814Amp_TXBF]\n",__FUNCTION__);
				*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8814Amp_TXBF_start;
			}
			else
#endif		
#if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				if(HAL_RFE_TYPE == 2){
					panic_printk("[%s][TXPWR_LMT_8814Amp_Type2]\n",__FUNCTION__);
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8814Amp_Type2_start;
				}else if(HAL_RFE_TYPE == 3){
					panic_printk("[%s][TXPWR_LMT_8814Amp_Type3]\n",__FUNCTION__);
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8814Amp_Type3_start;
				}else if(HAL_RFE_TYPE == 4){
					panic_printk("[%s][TXPWR_LMT_8814Amp_Type4]\n",__FUNCTION__);
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8814Amp_Type4_start;
				}else if(HAL_RFE_TYPE == 5){
					panic_printk("[%s][TXPWR_LMT_8814Amp_Type5]\n",__FUNCTION__);
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8814Amp_Type5_start;
				}else if(HAL_RFE_TYPE == 8){
					panic_printk("[%s][TXPWR_LMT_8814Amp_Type8]\n",__FUNCTION__);
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8814Amp_Type8_start;
				}else if(HAL_RFE_TYPE == 10){
					panic_printk("[%s][TXPWR_LMT_8814Amp_Type10]\n",__FUNCTION__);
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8814Amp_Type10_start;
				}else if(HAL_RFE_TYPE == 11){
					panic_printk("[%s][TXPWR_LMT_8814Amp_Type11]\n",__FUNCTION__);
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8814Amp_Type11_start;
				}else {
					panic_printk("[%s][TXPWR_LMT_8814Amp]\n",__FUNCTION__);
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8814Amp_start;
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
					panic_printk("[%s][TXPWR_LMT_8814Amp_Type0]\n",__FUNCTION__);
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8814Amp_Type0_start;
				}else if(HAL_RFE_TYPE == 7){
					panic_printk("[%s][TXPWR_LMT_8814Amp_Type7]\n",__FUNCTION__);
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8814Amp_Type7_start;
				}else if(HAL_RFE_TYPE == 9){
					panic_printk("[%s][TXPWR_LMT_8814Amp_Type9]\n",__FUNCTION__);
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8814Amp_Type9_start;
				}else{
					panic_printk("[%s][TXPWR_LMT_8814Amp]\n",__FUNCTION__);
					*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8814Amp_start;
				}
			}			
		}
#endif
#endif
	}
	break;

	case HW_VAR_POWERLIMITFILE_SIZE: {
#ifdef TXPWR_LMT_8881A
#if IS_RTL8881A_SERIES
		if (IS_HARDWARE_TYPE_8881A(Adapter)) {
			if (get_bonding_type_8881A() == BOND_8881AM) {
				if (HAL_VAR_INTERLPA_8881A)
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8881AMP_intpa_end - data_TXPWR_LMT_8881AMP_intpa_start);
				else
				*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8881AMP_end - data_TXPWR_LMT_8881AMP_start);
			} else if (get_bonding_type_8881A() == BOND_8881AB) {
				if (HAL_VAR_INTERLPA_8881A)
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8881ABP_intpa_end - data_TXPWR_LMT_8881ABP_intpa_start);
				else
				*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8881ABP_end - data_TXPWR_LMT_8881ABP_start);
			} else if (get_bonding_type_8881A() == BOND_8881AN) {
				if (HAL_VAR_INTERLPA_8881A)
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8881AN_end - data_TXPWR_LMT_8881AN_start);
				else
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8881AN_extpa_end - data_TXPWR_LMT_8881AN_extpa_start);
			} else {
				*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8881A_new_end - data_TXPWR_LMT_8881A_new_start);
			}
		}
#endif
#endif
#ifdef TXPWR_LMT_92EE
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_92EE_new_end - data_TXPWR_LMT_92EE_new_start);
		}
#endif
#endif
#ifdef TXPWR_LMT_8197F
#if IS_RTL8197F_SERIES
		if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
#ifdef BEAMFORMING_AUTO
			if(Adapter->pshare->rf_ft_var.txbf_pwrlmt == TXBF_TXPWRLMT_ENABLE) {
				printk("[%s][size TXPWR_LMT_8197Fmp_TXBF]\n",__FUNCTION__);
				*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8197Fmp_TXBF_end - data_TXPWR_LMT_8197Fmp_TXBF_start);
			}
			else
#endif			
#if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				if(HAL_RFE_TYPE == 1){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8197Fmp_Type1_end - data_TXPWR_LMT_8197Fmp_Type1_start);
				}else if(HAL_RFE_TYPE == 4){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8197Fmp_Type4_end - data_TXPWR_LMT_8197Fmp_Type4_start);
				}else if(HAL_RFE_TYPE == 5){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8197Fmp_Type5_end - data_TXPWR_LMT_8197Fmp_Type5_start);
				}else if(HAL_RFE_TYPE == 6){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8197Fmp_Type6_end - data_TXPWR_LMT_8197Fmp_Type6_start);
				}else {
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8197Fmp_end - data_TXPWR_LMT_8197Fmp_start);
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8197Fmp_Type0_end - data_TXPWR_LMT_8197Fmp_Type0_start);
				}else if(HAL_RFE_TYPE == 2){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8197Fmp_Type2_end - data_TXPWR_LMT_8197Fmp_Type2_start);
				}else if(HAL_RFE_TYPE == 3){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8197Fmp_Type3_end - data_TXPWR_LMT_8197Fmp_Type3_start);
				}else if(HAL_RFE_TYPE == 4){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8197Fmp_Type4_end - data_TXPWR_LMT_8197Fmp_Type4_start);
				}else{
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8197Fmp_end - data_TXPWR_LMT_8197Fmp_start);
				}
			}				
		}
#endif
#endif

#ifdef TXPWR_LMT_8198F
#if IS_RTL8198F_SERIES
    if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
#if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				if(HAL_RFE_TYPE == 1){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8198Fmp_Type1_end - data_TXPWR_LMT_8198Fmp_Type1_start);
				}else if(HAL_RFE_TYPE == 3){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8198Fmp_Type3_end - data_TXPWR_LMT_8198Fmp_Type3_start);
				}else {
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8198Fmp_end - data_TXPWR_LMT_8198Fmp_start);
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8198Fmp_Type0_end - data_TXPWR_LMT_8198Fmp_Type0_start);
				}else if(HAL_RFE_TYPE == 2){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8198Fmp_Type2_end - data_TXPWR_LMT_8198Fmp_Type2_start);
				}else {
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8198Fmp_end - data_TXPWR_LMT_8198Fmp_start);
				}
			}				
	}
#endif //#if IS_RTL8198F_SERIES
#endif //#ifdef TXPWR_LMT_8198F

#ifdef TXPWR_LMT_8197G
#if IS_RTL8197G_SERIES
    if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
#if CFG_HAL_HIGH_POWER_EXT_PA			
		if(HAL_VAR_use_ext_pa){
				*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8197Gmp_end - data_TXPWR_LMT_8197Gmp_start);
		}else
#endif				
		{
			if(HAL_RFE_TYPE == 0){
				*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8197Gmp_Type0_end - data_TXPWR_LMT_8197Gmp_Type0_start);
			}else {
				*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8197Gmp_end - data_TXPWR_LMT_8197Gmp_start);
			}
		}
    }
#endif //#if IS_RTL8197G_SERIES
#endif //#ifdef TXPWR_LMT_8197G

#ifdef TXPWR_LMT_8822B
#if IS_RTL8822B_SERIES
		if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
#ifdef BEAMFORMING_AUTO
			if(Adapter->pshare->rf_ft_var.txbf_pwrlmt == TXBF_TXPWRLMT_ENABLE) {
				printk("[%s][size TXPWR_LMT_8822Bmp_TXBF]\n",__FUNCTION__);
				*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Bmp_TXBF_end - data_TXPWR_LMT_8822Bmp_TXBF_start);
			}
			else
#endif					
#if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				if(HAL_RFE_TYPE == 1){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Bmp_Type1_end - data_TXPWR_LMT_8822Bmp_Type1_start);
				}else if(HAL_RFE_TYPE == 4){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Bmp_Type4_end - data_TXPWR_LMT_8822Bmp_Type4_start);
				}else if(HAL_RFE_TYPE == 6){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Bmp_Type6_end - data_TXPWR_LMT_8822Bmp_Type6_start);
				}else if(HAL_RFE_TYPE == 7){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Bmp_Type7_end - data_TXPWR_LMT_8822Bmp_Type7_start);
				}else if(HAL_RFE_TYPE == 11){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Bmp_Type11_end - data_TXPWR_LMT_8822Bmp_Type11_start);
				}else {
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Bmp_end - data_TXPWR_LMT_8822Bmp_start);
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Bmp_Type0_end - data_TXPWR_LMT_8822Bmp_Type0_start);
				}else if(HAL_RFE_TYPE == 8){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Bmp_Type8_end - data_TXPWR_LMT_8822Bmp_Type8_start);
				}else if(HAL_RFE_TYPE == 9){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Bmp_Type9_end - data_TXPWR_LMT_8822Bmp_Type9_start);
				}else if(HAL_RFE_TYPE == 10){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Bmp_Type10_end - data_TXPWR_LMT_8822Bmp_Type10_start);
				}else if(HAL_RFE_TYPE == 13){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Bmp_Type13_end - data_TXPWR_LMT_8822Bmp_Type13_start);
				}else if(HAL_RFE_TYPE == 14){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Bmp_Type14_end - data_TXPWR_LMT_8822Bmp_Type14_start);
				}else{
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Bmp_end - data_TXPWR_LMT_8822Bmp_start);
				}
			}				
		}
#endif
#endif

#ifdef TXPWR_LMT_8822C
#if IS_RTL8822C_SERIES
		if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
#ifdef BEAMFORMING_AUTO
			if(Adapter->pshare->rf_ft_var.txbf_pwrlmt == TXBF_TXPWRLMT_ENABLE) {
				printk("[%s][size TXPWR_LMT_8822Cmp_TXBF]\n",__FUNCTION__);
				*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Cmp_TXBF_end - data_TXPWR_LMT_8822Cmp_TXBF_start);
			}
			else
#endif					
#if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				if(HAL_RFE_TYPE == 1){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Cmp_Type1_end - data_TXPWR_LMT_8822Cmp_Type1_start);
				}else if(HAL_RFE_TYPE == 4){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Cmp_Type4_end - data_TXPWR_LMT_8822Cmp_Type4_start);
				}else if(HAL_RFE_TYPE == 6){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Cmp_Type6_end - data_TXPWR_LMT_8822Cmp_Type6_start);
				}else if(HAL_RFE_TYPE == 7){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Cmp_Type7_end - data_TXPWR_LMT_8822Cmp_Type7_start);
				}else if(HAL_RFE_TYPE == 11){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Cmp_Type11_end - data_TXPWR_LMT_8822Cmp_Type11_start);
				}else {
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Cmp_end - data_TXPWR_LMT_8822Cmp_start);
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Cmp_Type0_end - data_TXPWR_LMT_8822Cmp_Type0_start);
				}else if(HAL_RFE_TYPE == 8){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Cmp_Type8_end - data_TXPWR_LMT_8822Cmp_Type8_start);
				}else if(HAL_RFE_TYPE == 9){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Cmp_Type9_end - data_TXPWR_LMT_8822Cmp_Type9_start);
				}else if(HAL_RFE_TYPE == 10){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Cmp_Type10_end - data_TXPWR_LMT_8822Cmp_Type10_start);
				}else if(HAL_RFE_TYPE == 13){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Cmp_Type13_end - data_TXPWR_LMT_8822Cmp_Type13_start);
				}else if(HAL_RFE_TYPE == 14){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Cmp_Type14_end - data_TXPWR_LMT_8822Cmp_Type14_start);
				}else{
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8822Cmp_end - data_TXPWR_LMT_8822Cmp_start);
				}
			}				
		}
#endif
#endif

#ifdef TXPWR_LMT_8812F
#if IS_RTL8812F_SERIES
		if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
#ifdef BEAMFORMING_AUTO
			if(Adapter->pshare->rf_ft_var.txbf_pwrlmt == TXBF_TXPWRLMT_ENABLE) {
				printk("[%s][size TXPWR_LMT_8812Fmp_TXBF]\n",__FUNCTION__);
				*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8812Fmp_TXBF_end - data_TXPWR_LMT_8812Fmp_TXBF_start);
			}
			else
#endif					
#if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				if(HAL_RFE_TYPE == 1){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8812Fmp_Type1_end - data_TXPWR_LMT_8812Fmp_Type1_start);
				}else if(HAL_RFE_TYPE == 3){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8812Fmp_Type3_end - data_TXPWR_LMT_8812Fmp_Type3_start);
				}else if(HAL_RFE_TYPE == 4){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8812Fmp_Type4_end - data_TXPWR_LMT_8812Fmp_Type4_start);
				}else if(HAL_RFE_TYPE == 6){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8812Fmp_Type6_end - data_TXPWR_LMT_8812Fmp_Type6_start);
				}else if(HAL_RFE_TYPE == 7){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8812Fmp_Type7_end - data_TXPWR_LMT_8812Fmp_Type7_start);
				}else if(HAL_RFE_TYPE == 11){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8812Fmp_Type11_end - data_TXPWR_LMT_8812Fmp_Type11_start);
				}else {
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8812Fmp_end - data_TXPWR_LMT_8812Fmp_start);
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8812Fmp_Type0_end - data_TXPWR_LMT_8812Fmp_Type0_start);
				}else if(HAL_RFE_TYPE == 2){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8812Fmp_Type2_end - data_TXPWR_LMT_8812Fmp_Type2_start);
				}else if(HAL_RFE_TYPE == 8){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8812Fmp_Type8_end - data_TXPWR_LMT_8812Fmp_Type8_start);
				}else if(HAL_RFE_TYPE == 9){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8812Fmp_Type9_end - data_TXPWR_LMT_8812Fmp_Type9_start);
				}else if(HAL_RFE_TYPE == 10){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8812Fmp_Type10_end - data_TXPWR_LMT_8812Fmp_Type10_start);
				}else if(HAL_RFE_TYPE == 13){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8812Fmp_Type13_end - data_TXPWR_LMT_8812Fmp_Type13_start);
				}else if(HAL_RFE_TYPE == 14){
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8812Fmp_Type14_end - data_TXPWR_LMT_8812Fmp_Type14_start);
				}else{
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8812Fmp_end - data_TXPWR_LMT_8812Fmp_start);
				}
			}				
		}
#endif
#endif

#ifdef TXPWR_LMT_8814B
#if IS_RTL8814B_SERIES
    if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {       
#if CFG_HAL_HIGH_POWER_EXT_PA			
		if(HAL_VAR_use_ext_pa){
			if(HAL_RFE_TYPE == 1) {
				*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Bmp_Type1_end - data_TXPWR_LMT_8814Bmp_Type1_start);
			}else if(HAL_RFE_TYPE == 4) {
				*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Bmp_Type4_end - data_TXPWR_LMT_8814Bmp_Type4_start);
			}else if(HAL_RFE_TYPE == 5) {
				*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Bmp_Type5_end - data_TXPWR_LMT_8814Bmp_Type5_start);
			}else {
				*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Bmp_end - data_TXPWR_LMT_8814Bmp_start);
			}
		}else
#endif				
		{
			if(HAL_RFE_TYPE == 2) {
				*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Bmp_Type2_end - data_TXPWR_LMT_8814Bmp_Type2_start);
			}else if(HAL_RFE_TYPE == 7) {
				*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Bmp_Type7_end - data_TXPWR_LMT_8814Bmp_Type7_start);
			}else if(HAL_RFE_TYPE == 8) {
				*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Bmp_Type8_end - data_TXPWR_LMT_8814Bmp_Type8_start);
			}else{
				*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Bmp_end - data_TXPWR_LMT_8814Bmp_start);
			}
		}		
	}
#endif //#if IS_RTL8814B_SERIES
#endif //#ifdef TXPWR_LMT_8814B

#ifdef TXPWR_LMT_8814A
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
#ifdef BEAMFORMING_AUTO
			if(Adapter->pshare->rf_ft_var.txbf_pwrlmt == TXBF_TXPWRLMT_ENABLE) {
				printk("[%s][size TXPWR_LMT_8814Amp_TXBF]\n",__FUNCTION__);
				*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Amp_TXBF_end - data_TXPWR_LMT_8814Amp_TXBF_start);
			}
			else
#endif
#if CFG_HAL_HIGH_POWER_EXT_PA			
			if(HAL_VAR_use_ext_pa){
				if(HAL_RFE_TYPE == 2){
					printk("[%s][size TXPWR_LMT_8814Amp_Type2]\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Amp_Type2_end - data_TXPWR_LMT_8814Amp_Type2_start);
				}else if(HAL_RFE_TYPE == 3){
					printk("[%s][size TXPWR_LMT_8814Amp_Type3]\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Amp_Type3_end - data_TXPWR_LMT_8814Amp_Type3_start);
				}else if(HAL_RFE_TYPE == 4){
					printk("[%s][size TXPWR_LMT_8814Amp_Type4]\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Amp_Type4_end - data_TXPWR_LMT_8814Amp_Type4_start);
				}else if(HAL_RFE_TYPE == 5){
					printk("[%s][size TXPWR_LMT_8814Amp_Type5]\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Amp_Type5_end - data_TXPWR_LMT_8814Amp_Type5_start);
				}else if(HAL_RFE_TYPE == 8){
					printk("[%s][size TXPWR_LMT_8814Amp_Type8]\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Amp_Type8_end - data_TXPWR_LMT_8814Amp_Type8_start);
				}else if(HAL_RFE_TYPE == 10){
					printk("[%s][size TXPWR_LMT_8814Amp_Type10]\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Amp_Type10_end - data_TXPWR_LMT_8814Amp_Type10_start);
				}else if(HAL_RFE_TYPE == 11){
					printk("[%s][size TXPWR_LMT_8814Amp_Type11]\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Amp_Type11_end - data_TXPWR_LMT_8814Amp_Type11_start);
				}else {
					printk("[%s][size TXPWR_LMT_8814Amp]\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Amp_end - data_TXPWR_LMT_8814Amp_start);
				}
			}else
#endif				
			{
				if(HAL_RFE_TYPE == 0){
					printk("[%s][size TXPWR_LMT_8814Amp_Type0]\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Amp_Type0_end - data_TXPWR_LMT_8814Amp_Type0_start);
				}else if(HAL_RFE_TYPE == 7){
					printk("[%s][size TXPWR_LMT_8814Amp_Type7]\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Amp_Type7_end - data_TXPWR_LMT_8814Amp_Type7_start);
				}else if(HAL_RFE_TYPE == 9){
					printk("[%s][size TXPWR_LMT_8814Amp_Type9]\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Amp_Type9_end - data_TXPWR_LMT_8814Amp_Type9_start);
				}else{
					printk("[%s][size TXPWR_LMT_8814Amp]\n",__FUNCTION__);
					*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Amp_end - data_TXPWR_LMT_8814Amp_start);
				}
			}
		}
#endif
#endif
	}
	break;
#ifdef BEAMFORMING_AUTO	
	case HW_VAR_POWERLIMITFILE_TXBF_START: {
#ifdef TXPWR_LMT_8814A
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			panic_printk("[%s][TXPWR_LMT_8814Amp_TXBF]\n",__FUNCTION__);
			*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8814Amp_TXBF_start;
		}
#endif
#endif
#ifdef TXPWR_LMT_8197F
#if IS_RTL8197F_SERIES
		if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
			panic_printk("[%s][TXPWR_LMT_8197Fmp_TXBF]\n",__FUNCTION__);
			*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8197Fmp_TXBF_start;
		}
#endif
#endif
#ifdef TXPWR_LMT_8822B
#if IS_RTL8822B_SERIES
		if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
			panic_printk("[%s][TXPWR_LMT_8822Bmp_TXBF]\n",__FUNCTION__);
			*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8822Bmp_TXBF_start;
		}
#endif
#endif
#ifdef TXPWR_LMT_8822C
#if IS_RTL8822C_SERIES
			if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
				panic_printk("[%s][TXPWR_LMT_8822Cmp_TXBF]\n",__FUNCTION__);
				*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8822Cmp_TXBF_start;
			}
#endif
#endif
#ifdef TXPWR_LMT_8812F
#if IS_RTL8812F_SERIES
			if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
				panic_printk("[%s][TXPWR_LMT_8812Fmp_TXBF]\n",__FUNCTION__);
				*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_8812Fmp_TXBF_start;
			}
#endif
#endif
		
	}
	break;
	case HW_VAR_POWERLIMITFILE_TXBF_SIZE: {
#ifdef TXPWR_LMT_8814A
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			printk("[%s][size TXPWR_LMT_8814Amp_TXBF]\n",__FUNCTION__);
			*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_8814Amp_TXBF_end - data_TXPWR_LMT_8814Amp_TXBF_start);
		}
#endif
#endif		
	}
	break;
#endif	// BEAMFORMING_AUTO
#ifdef PWR_BY_RATE_92E_HP
#if CFG_HAL_HIGH_POWER_EXT_PA
	case HW_VAR_POWERLIMITFILE_HP_START: {
#ifdef TXPWR_LMT_92EE
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			printk("[TXPWR_LMT_92EE_hp]\n");
			*((unsigned long*)(val)) = (unsigned long)data_TXPWR_LMT_92EE_hp_start;
		}
#endif
#endif
	}
	break;

	case HW_VAR_POWERLIMITFILE_HP_SIZE: {
#ifdef TXPWR_LMT_92EE
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			printk("[TXPWR_LMT_92EE_hp]\n");
			*((pu4Byte)(val)) = (u4Byte)(data_TXPWR_LMT_92EE_hp_end - data_TXPWR_LMT_92EE_hp_start);
		}
#endif
#endif
	}
	break;
#endif
#endif //PWR_BY_RATE_92E_HP

	case HW_VAR_PHYREGFILE_AGC_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				printk("[AGC_TAB_8881A]\n");
				*((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8881A_start;
			} else { // mp chip
				if (get_bonding_type_8881A() == BOND_8881AB) {
					if (HAL_VAR_INTERLPA_8881A) {
						printk("[AGC_TAB_8881ABP_intpa]\n");
						*((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8881ABP_intpa_start;
					} else {
					printk("[AGC_TAB_8881ABP]\n");
					*((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8881ABP_start;
					}
				} else if (get_bonding_type_8881A() == BOND_8881AM) {
					if (HAL_VAR_INTERLPA_8881A) {
						printk("[AGC_TAB_8881AMP_intpa]\n");
						*((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8881AMP_intpa_start;
					} else {					
#ifdef CONFIG_8881A_2LAYER
						if (Adapter->pshare->rf_ft_var.use_8881a_2layer == 1){
							printk("[AGC_TAB_8881AMP_2layer]\n");
							*((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8881AMP_2layer_start;
						}
						else
#endif							
						{                    
							printk("[AGC_TAB_8881AMP]\n");
							*((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8881AMP_start;						
						}
					}
				} else if (get_bonding_type_8881A() == BOND_8881AN) {
					if (HAL_VAR_INTERLPA_8881A) {
						printk("[AGC_TAB_8881AN]\n");
						*((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8881AN_start;
					} else {
						printk("[AGC_TAB_8881AN_extpa]\n");
						*((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8881AN_extpa_start;
					}
				} else {
					printk("[AGC_TAB_8881Am]\n");
					*((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8881Am_start;
				}

			}
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
#if IS_EXIST_PCI
			if (Adapter->hci_type == RTL_HCI_PCIE) {
				if (IS_HAL_TEST_CHIP(Adapter)) {
					printk("[AGC_TAB_8192E]\n");
					*((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8192E_start;
				} else { // mp chip
					printk("[AGC_TAB_8192Emp]\n");
					*((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8192Emp_start;
				}
			}
#endif
#if IS_EXIST_SDIO
			if (Adapter->hci_type == RTL_HCI_SDIO) {
				printk("[AGC_TAB_8192ES]\n");
				*((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8192ES_start;
			}
#endif
#if IS_EXIST_USB /* CONFIG_RTL_TRIBAND_SUPPORT */
			if (Adapter->hci_type == RTL_HCI_USB) {
				printk("[AGC_TAB_8192EU]\n");
				*((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8192EU_start;
			}
#endif
		}
#endif // IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
            	printk("[%s][AGC_TAB_8814A]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8814A_start;
			} else { // mp chip
           		printk("[%s][AGC_TAB_8814Amp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8814Amp_start;                    
			}
		}
#endif

#if IS_RTL8197F_SERIES
    if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
        printk("[%s][AGC_TAB_8197F]\n",__FUNCTION__);
        *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8197F_start;                    
    }
#endif

#if IS_RTL8198F_SERIES
    if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
        printk("[%s][AGC_TAB_8198F]\n",__FUNCTION__);
        *((dma_addr_t*)(val)) = (dma_addr_t)data_AGC_TAB_8198F_start;                    
    }
#endif

#if IS_RTL8822B_SERIES
    if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
		if (IS_HAL_TEST_CHIP(Adapter)) {
        	printk("[%s][AGC_TAB_8822B]\n",__FUNCTION__);
            *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8822B_start;
		} else { // mp chip
       		printk("[%s][AGC_TAB_8822Bmp]\n",__FUNCTION__);
            *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8822Bmp_start;                    
		}
	}
#endif

#if IS_RTL8822C_SERIES
    if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
		if (IS_HAL_TEST_CHIP(Adapter)) {
        	printk("[%s][AGC_TAB_8822C]\n",__FUNCTION__);
            *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8822C_start;
		} else { // mp chip
       		printk("[%s][AGC_TAB_8822Cmp]\n",__FUNCTION__);
            *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8822Cmp_start;                    
		}
	}
#endif

#if IS_RTL8812F_SERIES
    if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
		if (IS_HAL_TEST_CHIP(Adapter)) {
        	printk("[%s][AGC_TAB_8812F]\n",__FUNCTION__);
            *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8812F_start;
		} else { // mp chip
       		printk("[%s][AGC_TAB_8812Fmp]\n",__FUNCTION__);
            *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8812Fmp_start;                    
		}
	}
#endif

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            printk("[%s][AGC_TAB_8814B]\n",__FUNCTION__);
            *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8814B_start;  //need agc table file, yllin8814B
        }
#endif

#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            printk("[%s][AGC_TAB_8197G]\n",__FUNCTION__);
            *((dma_addr_t*)(val)) = (dma_addr_t)data_AGC_TAB_8197G_start;                    
        }
#endif

	}
	break;

	case HW_VAR_PHYREGFILE_AGC_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8881A_end - data_AGC_TAB_8881A_start);
			} else { // mp chip
				//printk("Get MP Radio A len \n");
				if (get_bonding_type_8881A() == BOND_8881AB) {
					if (HAL_VAR_INTERLPA_8881A)
						*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8881ABP_intpa_end - data_AGC_TAB_8881ABP_intpa_start);
					else
					*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8881ABP_end - data_AGC_TAB_8881ABP_start);
				} else if (get_bonding_type_8881A() == BOND_8881AM) {
					if (HAL_VAR_INTERLPA_8881A)
						*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8881AMP_intpa_end - data_AGC_TAB_8881AMP_intpa_start);
					else {
#ifdef CONFIG_8881A_2LAYER						
						if (Adapter->pshare->rf_ft_var.use_8881a_2layer == 1)
							*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8881AMP_2layer_end - data_AGC_TAB_8881AMP_2layer_start);
						else 	
#endif
							*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8881AMP_end - data_AGC_TAB_8881AMP_start);												
					}
				} else if (get_bonding_type_8881A() == BOND_8881AN) {
					if (HAL_VAR_INTERLPA_8881A) {
						*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8881AN_end - data_AGC_TAB_8881AN_start);
					} else {
						*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8881AN_extpa_end - data_AGC_TAB_8881AN_extpa_start);
					}
				} else {
					*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8881Am_end - data_AGC_TAB_8881Am_start);
				}
			}
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
#if IS_EXIST_PCI
			if (Adapter->hci_type == RTL_HCI_PCIE) {
				if (IS_HAL_TEST_CHIP(Adapter)) {
					*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192E_end - data_AGC_TAB_8192E_start);
				} else { // mp chip
					*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192Emp_end - data_AGC_TAB_8192Emp_start);
				}
			}
#endif
#if IS_EXIST_SDIO
			if (Adapter->hci_type == RTL_HCI_SDIO) {
				*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192ES_end - data_AGC_TAB_8192ES_start);
			}
#endif
#if IS_EXIST_USB /* CONFIG_RTL_TRIBAND_SUPPORT */
			if (Adapter->hci_type == RTL_HCI_USB) {
				*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192EU_end - data_AGC_TAB_8192EU_start);
			}
#endif
		}
#endif // IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8814A_end - data_AGC_TAB_8814A_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8814Amp_end - data_AGC_TAB_8814Amp_start);
			}
		}
#endif
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
				printk("[%s][AGC_TAB_8197F] size\n",__FUNCTION__);
                *((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8197F_end - data_AGC_TAB_8197F_start);
        }
#endif

#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
				printk("[%s][AGC_TAB_8198F] size = *((pu4Byte)(val)) = %x \n",__FUNCTION__,*((pu4Byte)(val)));
                *((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8198F_end - data_AGC_TAB_8198F_start);
        }
#endif

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8822B_end - data_AGC_TAB_8822B_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8822Bmp_end - data_AGC_TAB_8822Bmp_start);
			}
		}
#endif

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8822C_end - data_AGC_TAB_8822C_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8822Cmp_end - data_AGC_TAB_8822Cmp_start);
			}
		}
#endif

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8812F_end - data_AGC_TAB_8812F_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8812Fmp_end - data_AGC_TAB_8812Fmp_start);
			}
		}
#endif

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8814B_end - data_AGC_TAB_8814B_start);
            printk("[%s][AGC_TAB_8814B size] %d\n", __func__, *((pu4Byte)(val)));
        }
#endif
#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
                printk("[%s][AGC_TAB_8197G] size = *((pu4Byte)(val)) = %x \n",__FUNCTION__,*((pu4Byte)(val)));
                *((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8197G_end - data_AGC_TAB_8197G_start);
        }
#endif

	}
	break;

#if (CFG_HAL_HIGH_POWER_EXT_PA & CFG_HAL_HIGH_POWER_EXT_LNA)
	case HW_VAR_PHYREGFILE_AGC_HP_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			//*((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8881A_hp_start;
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				printk("[AGC_TAB_8192E_hp]\n");
				*((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8192E_hp_start;
			} else { // mp chip
				if (Adapter->pshare->rf_ft_var.lna_type == LNA_TYPE_1) {
				    printk("[AGC_TAB_8192E_extlna_type1]\n");
	                *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8192E_extlna_type1_start;
				}else if (Adapter->pshare->rf_ft_var.lna_type == LNA_TYPE_2){
				    printk("[AGC_TAB_8192E_extlna_type2]\n");
	                *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8192E_extlna_type2_start;
				}else if (Adapter->pshare->rf_ft_var.lna_type == LNA_TYPE_3){
				    printk("[AGC_TAB_8192E_extlna_type3]\n");
	                *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8192E_extlna_type3_start;
				}else if (Adapter->pshare->rf_ft_var.lna_type == LNA_TYPE_DSLPON_SKY85201_11_2L){
				    printk("[AGC_TAB_8192E_extlna_for_dslpon_sky85201_11_2L]\n");
	                *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8192E_extlna_dslpon_sky85201_11_2L_start;
				}else if (Adapter->pshare->rf_ft_var.lna_type == LNA_TYPE_DSLPON_SKY85201_11_4L){
				    printk("[AGC_TAB_8192E_extlna_for_dslpon_sky85201_11_4L]\n");
	                *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8192E_extlna_dslpon_sky85201_11_4L_start;
				}else {
				    printk("[AGC_TAB_8192E_hp]\n");
	                *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8192E_hp_start;
				}
			}
		}
#endif
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {

                printk("[%s][AGC_TAB_8814A_hp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8814A_hp_start;
			} else { // mp chip
                printk("[%s][AGC_TAB_8814Amp_hp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8814Amp_hp_start;
			}
		}
#endif

#if IS_RTL8197F_SERIES
    if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
        printk("[%s][AGC_TAB_8197F]\n",__FUNCTION__);
       *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8197F_start; 
    }
#endif

#if IS_RTL8198F_SERIES
    if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
        printk("[%s][AGC_TAB_8198F]\n",__FUNCTION__);
       *((dma_addr_t*)(val)) = (dma_addr_t)data_AGC_TAB_8198F_start; 
    }
#endif

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {

                printk("[%s][AGC_TAB_8822B_hp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8822B_hp_start;
			} else { // mp chip
                printk("[%s][AGC_TAB_8822Bmp_hp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8822Bmp_hp_start;
			}
		}
#endif

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {

                printk("[%s][AGC_TAB_8822C_hp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8822C_hp_start;
			} else { // mp chip
                printk("[%s][AGC_TAB_8822Cmp_hp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8822Cmp_hp_start;
			}
		}
#endif

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {

                printk("[%s][AGC_TAB_8812F_hp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8812F_hp_start;
			} else { // mp chip
                printk("[%s][AGC_TAB_8812Fmp_hp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8812Fmp_hp_start;
			}
		}
#endif

#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            printk("[%s][AGC_TAB_8197G]\n",__FUNCTION__);
           *((dma_addr_t*)(val)) = (dma_addr_t)data_AGC_TAB_8197G_start; 
        }
#endif
	}
	break;

	case HW_VAR_PHYREGFILE_AGC_HP_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			//*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8881A_hp_end - data_AGC_TAB_8881A_hp_start);
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				//printk("Get 8192E AGC_TAB_hp len\n");
				*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192E_hp_end - data_AGC_TAB_8192E_hp_start);
			} else { // mp chip
			
                if (Adapter->pshare->rf_ft_var.lna_type == LNA_TYPE_1) {
	                *((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192E_extlna_type1_end - data_AGC_TAB_8192E_extlna_type1_start);
				}else if (Adapter->pshare->rf_ft_var.lna_type == LNA_TYPE_2){
	                *((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192E_extlna_type2_end - data_AGC_TAB_8192E_extlna_type2_start);
				}else if (Adapter->pshare->rf_ft_var.lna_type == LNA_TYPE_3){
	                *((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192E_extlna_type3_end - data_AGC_TAB_8192E_extlna_type3_start);
				}else if (Adapter->pshare->rf_ft_var.lna_type == LNA_TYPE_DSLPON_SKY85201_11_2L){
	                *((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192E_extlna_dslpon_sky85201_11_2L_end - data_AGC_TAB_8192E_extlna_dslpon_sky85201_11_2L_start);
				}else if (Adapter->pshare->rf_ft_var.lna_type == LNA_TYPE_DSLPON_SKY85201_11_4L){
	                *((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192E_extlna_dslpon_sky85201_11_4L_end - data_AGC_TAB_8192E_extlna_dslpon_sky85201_11_4L_start);
				}else{
                    *((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192E_hp_end - data_AGC_TAB_8192E_hp_start);
				}


				//printk("Get 8192E MP chip AGC_TAB_hp len\n");
				//*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192E_hp_end - data_AGC_TAB_8192E_hp_start);
				//*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192Emp_hp_end - data_AGC_TAB_8192Emp_hp_start);
			}
		}
#endif
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				//printk("Get 8814A AGC_TAB_hp len\n");
				*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8814A_hp_end - data_AGC_TAB_8814A_hp_start);
			} else { // mp chip
				//printk("Get 8814A MP chip AGC_TAB_hp len\n");
				*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8814Amp_hp_end - data_AGC_TAB_8814Amp_hp_start);
			}
		}
#endif
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            printk("[%s][AGC_TAB_8197F] size\n",__FUNCTION__);
            *((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8197F_end - data_AGC_TAB_8197F_start);
        }
#endif

#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
    		printk("[%s][AGC_TAB_8198F] size = *((pu4Byte)(val)) = %x \n",__FUNCTION__,*((pu4Byte)(val)));
           *((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8198F_end - data_AGC_TAB_8198F_start);
        }
#endif

#if IS_RTL8822B_SERIES
	if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
		if (IS_HAL_TEST_CHIP(Adapter)) {
			//printk("Get 8814A AGC_TAB_hp len\n");
			*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8822B_hp_end - data_AGC_TAB_8822B_hp_start);
		} else { // mp chip
			//printk("Get 8814A MP chip AGC_TAB_hp len\n");
			*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8822Bmp_hp_end - data_AGC_TAB_8822Bmp_hp_start);
		}
	}
#endif

#if IS_RTL8822C_SERIES
	if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
		if (IS_HAL_TEST_CHIP(Adapter)) {
			//printk("Get 8814A AGC_TAB_hp len\n");
			*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8822C_hp_end - data_AGC_TAB_8822C_hp_start);
		} else { // mp chip
			//printk("Get 8814A MP chip AGC_TAB_hp len\n");
			*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8822Cmp_hp_end - data_AGC_TAB_8822Cmp_hp_start);
		}
	}
#endif

#if IS_RTL8812F_SERIES
	if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
		if (IS_HAL_TEST_CHIP(Adapter)) {
			//printk("Get 8814A AGC_TAB_hp len\n");
			*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8812F_hp_end - data_AGC_TAB_8812F_hp_start);
		} else { // mp chip
			//printk("Get 8814A MP chip AGC_TAB_hp len\n");
			*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8812Fmp_hp_end - data_AGC_TAB_8812Fmp_hp_start);
		}
	}
#endif

#if IS_RTL8197G_SERIES
            if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
                printk("[%s][AGC_TAB_8197G] size = *((pu4Byte)(val)) = %x \n",__FUNCTION__,*((pu4Byte)(val)));
               *((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8197G_end - data_AGC_TAB_8197G_start);
            }
#endif
	}
	break;
#endif //CFG_HAL_HIGH_POWER_EXT_PA
#if CFG_HAL_HIGH_POWER_EXT_PA
	case HW_VAR_PHYREGFILE_AGC_EXTPA_START: {
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8192E_extpa_start;
			printk("[AGC_TAB_8192E_extpa]\n");
		}
#endif
	}
	break;
	case HW_VAR_PHYREGFILE_AGC_EXTPA_SIZE: {
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192E_extpa_end - data_AGC_TAB_8192E_extpa_start);
		}
#endif
	}
	break;
#endif

#if CFG_HAL_HIGH_POWER_EXT_LNA
	case HW_VAR_PHYREGFILE_AGC_EXTLNA_START: {
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			//printk("[AGC_TAB_8192E_extlna]\n");
#if (IS_EXIST_PCI || IS_EXIST_USB)
			if (Adapter->hci_type == RTL_HCI_PCIE || Adapter->hci_type == RTL_HCI_USB) {
				if (Adapter->pshare->rf_ft_var.lna_type == LNA_TYPE_1) {
				    printk("[AGC_TAB_8192E_extlna_type1]\n");
		            *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8192E_extlna_type1_start;
				}else if (Adapter->pshare->rf_ft_var.lna_type == LNA_TYPE_2){
				    printk("[AGC_TAB_8192E_extlna_type2]\n");
		            *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8192E_extlna_type2_start;
				}else if (Adapter->pshare->rf_ft_var.lna_type == LNA_TYPE_3){
				    printk("[AGC_TAB_8192E_extlna_type3]\n");
		            *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8192E_extlna_type3_start;
				}else if (Adapter->pshare->rf_ft_var.lna_type == LNA_TYPE_DSLPON_SKY85201_11_2L){
				    printk("[AGC_TAB_8192E_extlna_for_dslpon_sky85201_11_2L]\n");
		            *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8192E_extlna_dslpon_sky85201_11_2L_start;
				}else if (Adapter->pshare->rf_ft_var.lna_type == LNA_TYPE_DSLPON_SKY85201_11_4L){
				    printk("[AGC_TAB_8192E_extlna_for_dslpon_sky85201_11_4L]\n");
		            *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8192E_extlna_dslpon_sky85201_11_4L_start;
				}else {
				    printk("[AGC_TAB_8192E_extlna]\n");
		            *((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8192E_extlna_start;
				}
				//*((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8192E_lna_start;
			}
#endif
#if IS_EXIST_SDIO
			if (Adapter->hci_type == RTL_HCI_SDIO) {
				*((unsigned long*)(val)) = (unsigned long)data_AGC_TAB_8192ES_extlna_start;
			}
#endif
		}
#endif // IS_RTL8192E_SERIES
	}
	break;
	case HW_VAR_PHYREGFILE_AGC_EXTLNA_SIZE: {
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			//printk("Get 8192E MP chip AGC_TAB_hp len\n");
#if (IS_EXIST_PCI || IS_EXIST_USB)
			if (Adapter->hci_type == RTL_HCI_PCIE || Adapter->hci_type == RTL_HCI_USB) {
				if (Adapter->pshare->rf_ft_var.lna_type == LNA_TYPE_1){
		            *((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192E_extlna_type1_end - data_AGC_TAB_8192E_extlna_type1_start);
				}else if (Adapter->pshare->rf_ft_var.lna_type == LNA_TYPE_2){
		            *((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192E_extlna_type2_end - data_AGC_TAB_8192E_extlna_type2_start);
				}else if (Adapter->pshare->rf_ft_var.lna_type == LNA_TYPE_3){
		            *((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192E_extlna_type3_end - data_AGC_TAB_8192E_extlna_type3_start);
				}else if (Adapter->pshare->rf_ft_var.lna_type == LNA_TYPE_DSLPON_SKY85201_11_2L){
		            *((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192E_extlna_dslpon_sky85201_11_2L_end - data_AGC_TAB_8192E_extlna_dslpon_sky85201_11_2L_start);
				}else if (Adapter->pshare->rf_ft_var.lna_type == LNA_TYPE_DSLPON_SKY85201_11_4L){
		            *((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192E_extlna_dslpon_sky85201_11_4L_end - data_AGC_TAB_8192E_extlna_dslpon_sky85201_11_4L_start);
				}else {
		            *((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192E_extlna_end - data_AGC_TAB_8192E_extlna_start);
				}
				//*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192Emp_hp_end - data_AGC_TAB_8192Emp_hp_start);
			}
#endif
#if IS_EXIST_SDIO
			if (Adapter->hci_type == RTL_HCI_SDIO) {
				*((pu4Byte)(val)) = (u4Byte)(data_AGC_TAB_8192ES_extlna_end - data_AGC_TAB_8192ES_extlna_start);
			}
#endif
		}
#endif // IS_RTL8192E_SERIES
	}
	break;
#endif // CFG_HAL_HIGH_POWER_EXT_LNA

	case HW_VAR_RFREGFILE_RADIO_A_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				printk("[RadioA_8881A]\n");
				*((unsigned long*)(val)) = (unsigned long)data_RadioA_8881A_start;
			} else { // mp chip
				if (get_bonding_type_8881A() == BOND_8881AB) {
					if (HAL_VAR_INTERLPA_8881A) {
						printk("[RadioA_8881ABP_intpa]\n");
						*((unsigned long*)(val)) = (unsigned long)data_RadioA_8881ABP_intpa_start;
					} else {
					printk("[RadioA_8881ABP]\n");
					*((unsigned long*)(val)) = (unsigned long)data_RadioA_8881ABP_start;
					}
				} else if (get_bonding_type_8881A() == BOND_8881AM) {
					if (HAL_VAR_INTERLPA_8881A) {
						printk("[RadioA_8881AMP_intpa]\n");
						*((unsigned long*)(val)) = (unsigned long)data_RadioA_8881AMP_intpa_start;
					} else {
#ifdef CONFIG_8881A_2LAYER						
						if (Adapter->pshare->rf_ft_var.use_8881a_2layer == 1){
							printk("[RadioA_8881AMP_2layer]\n");
							*((unsigned long*)(val)) = (unsigned long)data_RadioA_8881AMP_2layer_start;
						}
						else 	
#endif
                        {
                            printk("[RadioA_8881AMP]\n");
                            *((unsigned long*)(val)) = (unsigned long)data_RadioA_8881AMP_start; 
                        }
					
					}
				} else if (get_bonding_type_8881A() == BOND_8881AN) {
					if (HAL_VAR_INTERLPA_8881A) {
						printk("[RadioA_8881AN]\n");
						*((unsigned long*)(val)) = (unsigned long)data_RadioA_8881AN_start;
					} else {
						printk("[RadioA_8881AN_extpa]\n");
						*((unsigned long*)(val)) = (unsigned long)data_RadioA_8881AN_extpa_start;
					}
				} else {
					printk("[RadioA_8881Am]\n");
					*((unsigned long*)(val)) = (unsigned long)data_RadioA_8881Am_start;
				}
			}
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
#if IS_EXIST_PCI
			if (Adapter->hci_type == RTL_HCI_PCIE) {
				if (IS_HAL_TEST_CHIP(Adapter)) {
					if (_GET_HAL_DATA(Adapter)->cutVersion	== ODM_CUT_B) {
						printk("[RadioA_8192Eb]\n");
						*((unsigned long*)(val)) = (unsigned long)data_RadioA_8192Eb_start;
					} else {
						printk("[RadioA_8192E]\n");
						*((unsigned long*)(val)) = (unsigned long)data_RadioA_8192E_start;
					}
				} else { // mp chip
					if (_GET_HAL_DATA(Adapter)->cutVersion	== ODM_CUT_A) {
						printk("[RadioA_8192EmpA]\n");
						*((unsigned long*)(val)) = (unsigned long)data_RadioA_8192EmpA_start;
					} else {
						printk("[RadioA_8192Emp]\n");
						*((unsigned long*)(val)) = (unsigned long)data_RadioA_8192Emp_start;
					}
				}
			}
#endif
#if IS_EXIST_SDIO
			if (Adapter->hci_type == RTL_HCI_SDIO) {
				printk("[RadioA_8192ES]\n");
				*((unsigned long*)(val)) = (unsigned long)data_RadioA_8192ES_start;
			}
#endif
#if IS_EXIST_USB//#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
			if (Adapter->hci_type == RTL_HCI_USB) {			
				printk("[RadioA_8192EU]\n");
				*((unsigned long*)(val)) = (unsigned long)data_RadioA_8192EU_start;
			}
#endif
		}
#endif // IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
            	printk("[%s][RadioA_8814A]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioA_8814A_start;                    
			} else { // mp chip
				//printk("select MP Radio A \n");
                printk("[%s][RadioA_8814Amp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioA_8814Amp_start;
			}
		}
		//*((pu4Byte)(val)) = (u4Byte)data_RadioA_8814Amp_start;
		RT_TRACE(COMP_INIT, DBG_SERIOUS, ("%s(%d):Error, code should be added \n", __FUNCTION__, __LINE__));
#endif //IS_RTL8814A_SERIES

#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            printk("[%s][RadioA_8197F]\n",__FUNCTION__);
            *((unsigned long*)(val)) = (unsigned long)data_RadioA_8197F_start;
        }
        //RT_TRACE(COMP_INIT, DBG_SERIOUS, ("%s(%d):Error, code should be added \n", __FUNCTION__, __LINE__));
#endif //IS_RTL8197F_SERIES

#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            printk("[%s][RadioA_8198F]\n",__FUNCTION__);
            *((dma_addr_t*)(val)) = (dma_addr_t)data_RadioA_8198F_start;
        }
        //RT_TRACE(COMP_INIT, DBG_SERIOUS, ("%s(%d):Error, code should be added \n", __FUNCTION__, __LINE__));
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
            	printk("[%s][RadioA_8822B]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioA_8822B_start;                    
			} else { // mp chip
				//printk("select MP Radio A \n");
                printk("[%s][RadioA_8822Bmp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioA_8822Bmp_start;
			}
		}
        RT_TRACE(COMP_INIT, DBG_SERIOUS, ("%s(%d):Error, code should be added \n", __FUNCTION__, __LINE__));
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
            	printk("[%s][RadioA_8822C]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioA_8822C_start;                    
			} else { // mp chip
				//printk("select MP Radio A \n");
                printk("[%s][RadioA_8822Cmp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioA_8822Cmp_start;
			}
		}
        RT_TRACE(COMP_INIT, DBG_SERIOUS, ("%s(%d):Error, code should be added \n", __FUNCTION__, __LINE__));
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
            	printk("[%s][RadioA_8812F]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioA_8812F_start;                    
			} else { // mp chip
				//printk("select MP Radio A \n");
                printk("[%s][RadioA_8812Fmp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioA_8812Fmp_start;
			}
		}
        RT_TRACE(COMP_INIT, DBG_SERIOUS, ("%s(%d):Error, code should be added \n", __FUNCTION__, __LINE__));
#endif //IS_RTL8812F_SERIES

#if IS_RTL8814B_SERIES
            if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
                printk("[%s][RadioA_8814B]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioA_8814B_start; //need radio A file, yllin8814B
            }
            //RT_TRACE(COMP_INIT, DBG_SERIOUS, ("%s(%d):Error, code should be added \n", __FUNCTION__, __LINE__));
#endif //IS_RTL8814B_SERIES

#if IS_RTL8197G_SERIES
            if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
                printk("[%s][RadioA_8197G]\n",__FUNCTION__);
                *((dma_addr_t*)(val)) = (dma_addr_t)data_RadioA_8197G_start;
            }
            //RT_TRACE(COMP_INIT, DBG_SERIOUS, ("%s(%d):Error, code should be added \n", __FUNCTION__, __LINE__));
#endif //IS_RTL8197G_SERIES
	}
	break;

	case HW_VAR_RFREGFILE_SYN_A_START: {
#if IS_RTL8814B_SERIES
		if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
			printk("[%s][RadioSyn0_8814B]\n",__FUNCTION__);
			*((unsigned long*)(val)) = (unsigned long)data_RadioSyn0_8814B_start; //need radio A file, yllin8814B
		}
		//RT_TRACE(COMP_INIT, DBG_SERIOUS, ("%s(%d):Error, code should be added \n", __FUNCTION__, __LINE__));
#endif //IS_RTL8814B_SERIES
		}
		break;

	case HW_VAR_RFREGFILE_RADIO_A_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8881A_end - data_RadioA_8881A_start);
			} else { // mp chip
				//printk("Get MP Radio A len \n");
				if (get_bonding_type_8881A() == BOND_8881AB) {
					if (HAL_VAR_INTERLPA_8881A)
						*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8881ABP_intpa_end - data_RadioA_8881ABP_intpa_start);
					else
					*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8881ABP_end - data_RadioA_8881ABP_start);
				} else if (get_bonding_type_8881A() == BOND_8881AM) {
					if (HAL_VAR_INTERLPA_8881A)
						*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8881AMP_intpa_end - data_RadioA_8881AMP_intpa_start);
					else
#ifdef CONFIG_8881A_2LAYER						
						if (Adapter->pshare->rf_ft_var.use_8881a_2layer == 1)
							*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8881AMP_2layer_end - data_RadioA_8881AMP_2layer_start);
						else 	
#endif
                            *((pu4Byte)(val)) = (u4Byte)(data_RadioA_8881AMP_end - data_RadioA_8881AMP_start);  			
					
				} else if (get_bonding_type_8881A() == BOND_8881AN) {
					if (HAL_VAR_INTERLPA_8881A) {
						*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8881AN_end - data_RadioA_8881AN_start);
					} else {
						*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8881AN_extpa_end - data_RadioA_8881AN_extpa_start);
					}
				} else {
					*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8881Am_end - data_RadioA_8881Am_start);
				}
			}
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
#if IS_EXIST_PCI
			if (Adapter->hci_type == RTL_HCI_PCIE) {
				if (IS_HAL_TEST_CHIP(Adapter)) {
					if (_GET_HAL_DATA(Adapter)->cutVersion	== ODM_CUT_B)
						*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8192Eb_end - data_RadioA_8192Eb_start);
					else
						*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8192E_end - data_RadioA_8192E_start);
				} else { // mp chip
					//printk("Get MP Radio A len \n");
					if (_GET_HAL_DATA(Adapter)->cutVersion	== ODM_CUT_A)
						*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8192EmpA_end - data_RadioA_8192EmpA_start);
					else
						*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8192Emp_end - data_RadioA_8192Emp_start);
				}
			}
#endif
#if IS_EXIST_SDIO
			if (Adapter->hci_type == RTL_HCI_SDIO) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8192ES_end - data_RadioA_8192ES_start);
			}
#endif
#if IS_EXIST_USB//#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
			if (Adapter->hci_type == RTL_HCI_USB) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8192EU_end - data_RadioA_8192EU_start);
			}
#endif
		}
#endif // IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8814A_end - data_RadioA_8814A_start);
			} else { // mp chip
				//printk("Get MP Radio A len \n");
				*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8814Amp_end - data_RadioA_8814Amp_start);
			}
		}
#endif //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            printk("[%s][RadioA_8197F size]\n",__FUNCTION__);
            *((pu4Byte)(val)) = (u4Byte)(data_RadioA_8197F_end - data_RadioA_8197F_start);
        }
#endif //IS_RTL8197F_SERIES

#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            printk("[%s][RadioA_8198F size]\n",__FUNCTION__);
            *((pu4Byte)(val)) = (u4Byte)(data_RadioA_8198F_end - data_RadioA_8198F_start);
        }
#endif //IS_RTL8197F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8822B_end - data_RadioA_8822B_start);
			} else { // mp chip
				//printk("Get MP Radio A len \n");
				*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8822Bmp_end - data_RadioA_8822Bmp_start);
			}
		}
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8822C_end - data_RadioA_8822C_start);
			} else { // mp chip
				//printk("Get MP Radio A len \n");
				*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8822Cmp_end - data_RadioA_8822Cmp_start);
			}
		}
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8812F_end - data_RadioA_8812F_start);
			} else { // mp chip
				//printk("Get MP Radio A len \n");
				*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8812Fmp_end - data_RadioA_8812Fmp_start);
			}
		}
#endif //IS_RTL8812F_SERIES

#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            printk("[%s][RadioA_8198F size]\n",__FUNCTION__);
            *((pu4Byte)(val)) = (u4Byte)(data_RadioA_8197G_end - data_RadioA_8197G_start);
        }
#endif //IS_RTL8197G_SERIES
#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_RadioA_8814B_end - data_RadioA_8814B_start);
            printk("[%s][RadioA_8814B size] %d\n", __func__, *((pu4Byte)(val)));
        }
#endif //IS_RTL8814B_SERIES

	}
	break;

	case HW_VAR_RFREGFILE_SYN_A_SIZE: {
#if IS_RTL8814B_SERIES
		if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_RadioSyn0_8814B_end - data_RadioSyn0_8814B_start);
			printk("[%s][RadioSyn0_8814B size] %d\n", __func__, *((pu4Byte)(val)));
		}
#endif //IS_RTL8814B_SERIES

	}
	break;

#if (CFG_HAL_HIGH_POWER_EXT_PA & CFG_HAL_HIGH_POWER_EXT_LNA)
	case HW_VAR_RFREGFILE_RADIO_A_HP_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			//*((unsigned long*)(val)) = (unsigned long)data_RadioA_8881A_hp_start;
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				printk("[RadioA_8192E_hp]\n");
#if (IS_EXIST_PCI || IS_EXIST_USB)
				if (Adapter->hci_type == RTL_HCI_PCIE || Adapter->hci_type == RTL_HCI_USB) {
					*((unsigned long*)(val)) = (unsigned long)data_RadioA_8192E_hp_start;
				}
#endif
			} else { // mp chip
				printk("[RadioA_8192E_hp]\n");
#if (IS_EXIST_PCI || IS_EXIST_USB)
				if (Adapter->hci_type == RTL_HCI_PCIE || Adapter->hci_type == RTL_HCI_USB) {
					*((unsigned long*)(val)) = (unsigned long)data_RadioA_8192E_hp_start;
				}
#endif
			}
		}
#endif
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				printk("[%s][RadioA_8814A_hp]\n",__FUNCTION__);
				*((unsigned long*)(val)) = (unsigned long)data_RadioA_8814A_hp_start;
			} else { // mp chip
				printk("[%s][RadioA_8814Amp_hp]\n",__FUNCTION__);
				*((unsigned long*)(val)) = (unsigned long)data_RadioA_8814Amp_hp_start;
			}
		}
#endif //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
    if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
        printk("[%s][RadioA_8197F]\n",__FUNCTION__);
            *((unsigned long*)(val)) = (unsigned long)data_RadioA_8197F_start;
    }
#endif //IS_RTL8197F_SERIES

#if IS_RTL8198F_SERIES
    if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
        printk("[%s][RadioA_8198F]\n",__FUNCTION__);
            *((dma_addr_t*)(val)) = (dma_addr_t)data_RadioA_8198F_start;
    }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
    if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
		if (IS_HAL_TEST_CHIP(Adapter)) {
			printk("[%s][RadioA_8822B_hp]\n",__FUNCTION__);
			*((unsigned long*)(val)) = (unsigned long)data_RadioA_8822B_hp_start;
		} else { // mp chip
			printk("[%s][RadioA_8822Bmp_hp]\n",__FUNCTION__);
			*((unsigned long*)(val)) = (unsigned long)data_RadioA_8822Bmp_hp_start;
		}
	}
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
    if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
		if (IS_HAL_TEST_CHIP(Adapter)) {
			printk("[%s][RadioA_8822C_hp]\n",__FUNCTION__);
			*((unsigned long*)(val)) = (unsigned long)data_RadioA_8822C_hp_start;
		} else { // mp chip
			printk("[%s][RadioA_8822Cmp_hp]\n",__FUNCTION__);
			*((unsigned long*)(val)) = (unsigned long)data_RadioA_8822Cmp_hp_start;
		}
	}
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
    if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
		if (IS_HAL_TEST_CHIP(Adapter)) {
			printk("[%s][RadioA_8812F_hp]\n",__FUNCTION__);
			*((unsigned long*)(val)) = (unsigned long)data_RadioA_8812F_hp_start;
		} else { // mp chip
			printk("[%s][RadioA_8812Fmp_hp]\n",__FUNCTION__);
			*((unsigned long*)(val)) = (unsigned long)data_RadioA_8812Fmp_hp_start;
		}
	}
#endif //IS_RTL8812F_SERIES

#if IS_RTL8197G_SERIES
    if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
        printk("[%s][RadioA_8197G]\n",__FUNCTION__);
            *((dma_addr_t*)(val)) = (dma_addr_t)data_RadioA_8197G_start;
    }
#endif //IS_RTL8197G_SERIES
	}
	break;

	case HW_VAR_RFREGFILE_RADIO_A_HP_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			//*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8881A_hp_end - data_RadioA_8881A_hp_start);
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				//printk("Get 8192E MP chip RadioA_hp len\n");
#if (IS_EXIST_PCI || IS_EXIST_USB)
				if (Adapter->hci_type == RTL_HCI_PCIE || Adapter->hci_type == RTL_HCI_USB) {
					*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8192E_hp_end - data_RadioA_8192E_hp_start);
				}
#endif
			} else { // mp chip
				//printk("Get 8192E MP chip RadioA_hp len\n");
#if (IS_EXIST_PCI || IS_EXIST_USB)
				if (Adapter->hci_type == RTL_HCI_PCIE || Adapter->hci_type == RTL_HCI_USB) {
					*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8192E_hp_end - data_RadioA_8192E_hp_start);
				}
#endif
				//*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8192Emp_hp_end - data_RadioA_8192Emp_hp_start);
			}
		}
#endif
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				//printk("Get 8814A chip RadioA_hp len\n");
				*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8814A_hp_end - data_RadioA_8814A_hp_start);
			} else { // mp chip
				//printk("Get 8814A MP chip RadioA_hp len\n");
				*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8814Amp_hp_end - data_RadioA_8814Amp_hp_start);
			}
		}
#endif //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            printk("[%s][RadioA_8197F size]\n",__FUNCTION__);
            *((pu4Byte)(val)) = (u4Byte)(data_RadioA_8197F_end - data_RadioA_8197F_start);
        }
#endif //IS_RTL8197F_SERIES
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            printk("[%s][RadioA_8198F size]\n",__FUNCTION__);
            *((pu4Byte)(val)) = (u4Byte)(data_RadioA_8198F_end - data_RadioA_8198F_start);
        }
#endif //IS_RTL8198F_SERIES
#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				//printk("Get 8814A chip RadioA_hp len\n");
				*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8822B_hp_end - data_RadioA_8822B_hp_start);
			} else { // mp chip
				//printk("Get 8814A MP chip RadioA_hp len\n");
				*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8822Bmp_hp_end - data_RadioA_8822Bmp_hp_start);
			}
		}
#endif //IS_RTL8822B_SERIES
#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				//printk("Get 8814A chip RadioA_hp len\n");
				*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8822C_hp_end - data_RadioA_8822C_hp_start);
			} else { // mp chip
				//printk("Get 8814A MP chip RadioA_hp len\n");
				*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8822Cmp_hp_end - data_RadioA_8822Cmp_hp_start);
			}
		}
#endif //IS_RTL8822C_SERIES
#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				//printk("Get 8814A chip RadioA_hp len\n");
				*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8812F_hp_end - data_RadioA_8812F_hp_start);
			} else { // mp chip
				//printk("Get 8814A MP chip RadioA_hp len\n");
				*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8812Fmp_hp_end - data_RadioA_8812Fmp_hp_start);
			}
		}
#endif //IS_RTL8812F_SERIES

#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            printk("[%s][RadioA_8197G size]\n",__FUNCTION__);
            *((pu4Byte)(val)) = (u4Byte)(data_RadioA_8197G_end - data_RadioA_8197G_start);
        }
#endif //IS_RTL8197G_SERIES
	}
	break;
#endif

#if CFG_HAL_HIGH_POWER_EXT_PA
	case HW_VAR_RFREGFILE_RADIO_A_EXTPA_START: {
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_RadioA_8192E_extpa_start;
			printk("[RadioA_8192E_extpa]\n");
		}
#endif
	}
	break;

	case HW_VAR_RFREGFILE_RADIO_A_EXTPA_SIZE: {
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8192E_extpa_end - data_RadioA_8192E_extpa_start);
		}
#endif
	}
	break;
#endif
#if CFG_HAL_HIGH_POWER_EXT_LNA
	case HW_VAR_RFREGFILE_RADIO_A_EXTLNA_START: {
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
#if (IS_EXIST_PCI || IS_EXIST_USB)
			if (Adapter->hci_type == RTL_HCI_PCIE || Adapter->hci_type == RTL_HCI_USB) {
				*((unsigned long*)(val)) = (unsigned long)data_RadioA_8192E_extlna_start;
			}
#endif
#if IS_EXIST_SDIO
			if (Adapter->hci_type == RTL_HCI_SDIO) {
				*((unsigned long*)(val)) = (unsigned long)data_RadioA_8192ES_extlna_start;
			}
#endif
			printk("[RadioA_8192E_extlna]\n");
		}
#endif // IS_RTL8192E_SERIES
	}
	break;

	case HW_VAR_RFREGFILE_RADIO_A_EXTLNA_SIZE: {
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
#if (IS_EXIST_PCI || IS_EXIST_USB)
			if (Adapter->hci_type == RTL_HCI_PCIE || Adapter->hci_type == RTL_HCI_USB) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8192E_extlna_end - data_RadioA_8192E_extlna_start);
			}
#endif
#if IS_EXIST_SDIO
			if (Adapter->hci_type == RTL_HCI_SDIO) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioA_8192ES_extlna_end - data_RadioA_8192ES_extlna_start);
			}
#endif
		}
#endif // IS_RTL8192E_SERIES
	}
	break;
#endif // CFG_HAL_HIGH_POWER_EXT_LNA
	case HW_VAR_RFREGFILE_RADIO_B_START: {
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
#if IS_EXIST_PCI
			if (Adapter->hci_type == RTL_HCI_PCIE) {
				if (IS_HAL_TEST_CHIP(Adapter)) {
					if (_GET_HAL_DATA(Adapter)->cutVersion == ODM_CUT_B) {
						printk("[RadioB_8192Eb]\n");
						*((unsigned long*)(val)) = (unsigned long)data_RadioB_8192Eb_start;
					} else {
						printk("[RadioB_8192E]\n");
						*((unsigned long*)(val)) = (unsigned long)data_RadioB_8192E_start;
					}
				} else { // mp chip
					if (_GET_HAL_DATA(Adapter)->cutVersion	== ODM_CUT_A) {
						printk("[RadioB_8192EmpA]\n");
						*((unsigned long*)(val)) = (unsigned long)data_RadioB_8192EmpA_start;
					} else {
						printk("[RadioB_8192Emp]\n");
						*((unsigned long*)(val)) = (unsigned long)data_RadioB_8192Emp_start;
					}
				}
			}
#endif
#if IS_EXIST_SDIO
			if (Adapter->hci_type == RTL_HCI_SDIO) {
				printk("[RadioB_8192ES]\n");
				*((unsigned long*)(val)) = (unsigned long)data_RadioB_8192ES_start;
			}
#endif
#if IS_EXIST_USB//#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
			if (Adapter->hci_type == RTL_HCI_USB) {			
				printk("[RadioB_8192EU]\n");
				*((unsigned long*)(val)) = (unsigned long)data_RadioB_8192EU_start;
			}
#endif
		}
#endif // IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
            	printk("[%s][RadioB_8814A]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioB_8814A_start;                    
			} else { // mp chip
                printk("[%s][RadioB_8814Amp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioB_8814Amp_start;
			}
		}
#endif //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            printk("[%s][RadioB_8197F]\n",__FUNCTION__);
            *((unsigned long*)(val)) = (unsigned long)data_RadioB_8197F_start;
        }
#endif //IS_RTL8197F_SERIES

#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            printk("[%s][RadioB_8198F]\n",__FUNCTION__);
            *((dma_addr_t*)(val)) = (dma_addr_t)data_RadioB_8198F_start;
        }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
            	printk("[%s][RadioB_8822B]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioB_8822B_start;                    
			} else { // mp chip
                printk("[%s][RadioB_8822Bmp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioB_8822Bmp_start;
			}
		}
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
            	printk("[%s][RadioB_8812F]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioB_8812F_start;                    
			} else { // mp chip
                printk("[%s][RadioB_8812Fmp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioB_8812Fmp_start;
			}
		}
#endif //IS_RTL8812F_SERIES

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            printk("[%s][RadioB_8814B]\n",__FUNCTION__);
            *((unsigned long*)(val)) = (unsigned long)data_RadioB_8814B_start; //need radioB file, yllin8814B
        }
#endif //IS_RTL8814B_SERIES
#if IS_RTL8197G_SERIES
            if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
                printk("[%s][RadioB_8197G]\n",__FUNCTION__);
                *((dma_addr_t*)(val)) = (dma_addr_t)data_RadioB_8197G_start;
            }
#endif //IS_RTL8197G_SERIES
	}
	break;

	case HW_VAR_RFREGFILE_SYN_B_START: {
#if IS_RTL8814B_SERIES
		if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
			printk("[%s][RadioSyn1_8814B]\n",__FUNCTION__);
			*((unsigned long*)(val)) = (unsigned long)data_RadioSyn1_8814B_start;
		}
#endif //IS_RTL8814B_SERIES
		}
		break;

	case HW_VAR_RFREGFILE_RADIO_B_SIZE: {
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
#if IS_EXIST_PCI
			if (Adapter->hci_type == RTL_HCI_PCIE) {
				if (IS_HAL_TEST_CHIP(Adapter)) {
					if (_GET_HAL_DATA(Adapter)->cutVersion == ODM_CUT_B)
						*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8192Eb_end - data_RadioB_8192Eb_start);
					else
						*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8192E_end - data_RadioB_8192E_start);
				} else { // mp chip
					//printk("Get MP Radio B len \n");
					if (_GET_HAL_DATA(Adapter)->cutVersion	== ODM_CUT_A)
						*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8192EmpA_end - data_RadioB_8192EmpA_start);
					else
						*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8192Emp_end - data_RadioB_8192Emp_start);
				}
			}
#endif
#if IS_EXIST_SDIO
			if (Adapter->hci_type == RTL_HCI_SDIO) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8192ES_end - data_RadioB_8192ES_start);
			}
#endif
#if IS_EXIST_USB//#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
			if (Adapter->hci_type == RTL_HCI_USB) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8192EU_end - data_RadioB_8192EU_start);
			}
#endif
		}
#endif // IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8814A_end - data_RadioB_8814A_start);
			} else { // mp chip
				//printk("Get MP Radio B len \n");
				*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8814Amp_end - data_RadioB_8814Amp_start);
			}
		}
#endif //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            printk("[%s][RadioB_8197F size]\n",__FUNCTION__);
            *((pu4Byte)(val)) = (u4Byte)(data_RadioB_8197F_end - data_RadioB_8197F_start);
        }
#endif //IS_RTL8197F_SERIES

#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            printk("[%s][RadioB_8198F size] *((pu4Byte)(val))=%x \n",__FUNCTION__,*((pu4Byte)(val)));
            *((pu4Byte)(val)) = (u4Byte)(data_RadioB_8198F_end - data_RadioB_8198F_start);
        }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8822B_end - data_RadioB_8822B_start);
			} else { // mp chip
				//printk("Get MP Radio B len \n");
				*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8822Bmp_end - data_RadioB_8822Bmp_start);
			}
		}
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8822C_end - data_RadioB_8822C_start);
			} else { // mp chip
				//printk("Get MP Radio B len \n");
				*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8822Cmp_end - data_RadioB_8822Cmp_start);
			}
		}
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8812F_end - data_RadioB_8812F_start);
			} else { // mp chip
				//printk("Get MP Radio B len \n");
				*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8812Fmp_end - data_RadioB_8812Fmp_start);
			}
		}
#endif //IS_RTL8812F_SERIES

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_RadioB_8814B_end - data_RadioB_8814B_start);
            printk("[%s][RadioB_8814B size] %d\n", __func__, *((pu4Byte)(val)));
        }
#endif //IS_RTL8814B_SERIES

#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            printk("[%s][RadioB_8197G size] *((pu4Byte)(val))=%x \n",__FUNCTION__,*((pu4Byte)(val)));
            *((pu4Byte)(val)) = (u4Byte)(data_RadioB_8197G_end - data_RadioB_8197G_start);
        }
#endif //IS_RTL8197G_SERIES
	}
	break;

	case HW_VAR_RFREGFILE_SYN_B_SIZE: {
#if IS_RTL8814B_SERIES
		if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_RadioSyn1_8814B_end - data_RadioSyn1_8814B_start);
			printk("[%s][RadioSyn1_8814B size] %d\n", __func__, *((pu4Byte)(val)));
		}
#endif //IS_RTL8814B_SERIES
	}
	break;

#if (CFG_HAL_HIGH_POWER_EXT_PA & CFG_HAL_HIGH_POWER_EXT_LNA)
	case HW_VAR_RFREGFILE_RADIO_B_HP_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			//*((unsigned long*)(val)) = (unsigned long)data_RadioA_8881B_hp_start;
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				printk("[RadioB_8192E_hp]\n");
#if (IS_EXIST_PCI || IS_EXIST_USB)
				if (Adapter->hci_type == RTL_HCI_PCIE || Adapter->hci_type == RTL_HCI_USB) {
					*((unsigned long*)(val)) = (unsigned long)data_RadioB_8192E_hp_start;
				}
#endif
			} else { // mp chip
				printk("[RadioB_8192E_hp]\n");
#if (IS_EXIST_PCI || IS_EXIST_USB)
				if (Adapter->hci_type == RTL_HCI_PCIE || Adapter->hci_type == RTL_HCI_USB) {
					*((unsigned long*)(val)) = (unsigned long)data_RadioB_8192E_hp_start;
				}
#endif
			}
		}
#endif
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
                printk("[%s][RadioB_8814A_hp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioB_8814A_hp_start;
			} else { // mp chip
                printk("[%s][RadioB_8814Amp_hp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioB_8814Amp_hp_start;
			}
		}
#endif //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            printk("[%s][RadioB_8197F]\n",__FUNCTION__);
            *((unsigned long*)(val)) = (unsigned long)data_RadioB_8197F_start;
        }
#endif //IS_RTL8197F_SERIES

#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            printk("[%s][RadioB_8198F]\n",__FUNCTION__);
            *((dma_addr_t*)(val)) = (dma_addr_t)data_RadioB_8198F_start;
        }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
                printk("[%s][RadioB_8822B_hp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioB_8822B_hp_start;
			} else { // mp chip
                printk("[%s][RadioB_8822Bmp_hp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioB_8822Bmp_hp_start;
			}
		}
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
                printk("[%s][RadioB_8822C_hp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioB_8822C_hp_start;
			} else { // mp chip
                printk("[%s][RadioB_8822Cmp_hp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioB_8822Cmp_hp_start;
			}
		}
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
                printk("[%s][RadioB_8812F_hp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioB_8812F_hp_start;
			} else { // mp chip
                printk("[%s][RadioB_8812Fmp_hp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioB_8812Fmp_hp_start;
			}
		}
#endif //IS_RTL8812F_SERIES

#if IS_RTL8197G_SERIES
            if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
                printk("[%s][RadioB_8197G]\n",__FUNCTION__);
                *((dma_addr_t*)(val)) = (dma_addr_t)data_RadioB_8197G_start;
            }
#endif //IS_RTL8197G_SERIES
	}
	break;

	case HW_VAR_RFREGFILE_RADIO_B_HP_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			//*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8881A_hp_end - data_RadioB_8881A_hp_start);
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				//printk("Get 8192E MP chip RadioB_hp len\n");
#if (IS_EXIST_PCI || IS_EXIST_USB)
				if (Adapter->hci_type == RTL_HCI_PCIE || Adapter->hci_type == RTL_HCI_USB) {
					*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8192E_hp_end - data_RadioB_8192E_hp_start);
				}
#endif
			} else { // mp chip
				//printk("Get 8192E MP chip RadioB_hp len\n");
#if (IS_EXIST_PCI || IS_EXIST_USB)
				if (Adapter->hci_type == RTL_HCI_PCIE || Adapter->hci_type == RTL_HCI_USB) {
					*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8192E_hp_end - data_RadioB_8192E_hp_start);
				}	
#endif
				//*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8192Emp_hp_end - data_RadioB_8192Emp_hp_start);
			}
		}
#endif
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8814A_hp_end - data_RadioB_8814A_hp_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8814Amp_hp_end - data_RadioB_8814Amp_hp_start);
			}
		}
#endif //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
    if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
       printk("[%s][RadioB_8197F size]\n",__FUNCTION__);
       *((pu4Byte)(val)) = (u4Byte)(data_RadioB_8197F_end - data_RadioB_8197F_start);
    }
#endif //IS_RTL8197F_SERIES

#if IS_RTL8198F_SERIES
    if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
       printk("[%s][RadioB_8198F size]\n",__FUNCTION__);
       *((pu4Byte)(val)) = (u4Byte)(data_RadioB_8198F_end - data_RadioB_8198F_start);
    }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8822B_hp_end - data_RadioB_8822B_hp_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8822Bmp_hp_end - data_RadioB_8822Bmp_hp_start);
			}
		}
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8822C_hp_end - data_RadioB_8822C_hp_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8822Cmp_hp_end - data_RadioB_8822Cmp_hp_start);
			}
		}
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8812F_hp_end - data_RadioB_8812F_hp_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8812Fmp_hp_end - data_RadioB_8812Fmp_hp_start);
			}
		}
#endif //IS_RTL8812F_SERIES

#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
           printk("[%s][RadioB_8197G size]\n",__FUNCTION__);
           *((pu4Byte)(val)) = (u4Byte)(data_RadioB_8197G_end - data_RadioB_8197G_start);
        }
#endif //IS_RTL8197G_SERIES
	}
	break;
#endif
#if CFG_HAL_HIGH_POWER_EXT_PA
	case HW_VAR_RFREGFILE_RADIO_B_EXTPA_START: {
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_RadioB_8192E_extpa_start;
			printk("[RadioB_8192E_extpa]\n");
		}
#endif
	}
	break;

	case HW_VAR_RFREGFILE_RADIO_B_EXTPA_SIZE: {
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8192E_extpa_end - data_RadioB_8192E_extpa_start);
		}
#endif
	}
	break;
#endif

#if CFG_HAL_HIGH_POWER_EXT_LNA
	case HW_VAR_RFREGFILE_RADIO_B_EXTLNA_START: {
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
#if (IS_EXIST_PCI || IS_EXIST_USB)
			if (Adapter->hci_type == RTL_HCI_PCIE || Adapter->hci_type == RTL_HCI_USB) {
				*((unsigned long*)(val)) = (unsigned long)data_RadioB_8192E_extlna_start;
			}
#endif
#if IS_EXIST_SDIO
			if (Adapter->hci_type == RTL_HCI_SDIO) {
				*((unsigned long*)(val)) = (unsigned long)data_RadioB_8192ES_extlna_start;
			}
#endif
			printk("[RadioB_8192E_extlna]\n");
		}
#endif // IS_RTL8192E_SERIES
	}
	break;

	case HW_VAR_RFREGFILE_RADIO_B_EXTLNA_SIZE: {
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
#if (IS_EXIST_PCI || IS_EXIST_USB)
			if (Adapter->hci_type == RTL_HCI_PCIE || Adapter->hci_type == RTL_HCI_USB) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8192E_extlna_end - data_RadioB_8192E_extlna_start);
				//*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8192Emp_hp_end - data_RadioB_8192Emp_hp_start);
			}
#endif
#if IS_EXIST_SDIO
			if (Adapter->hci_type == RTL_HCI_SDIO) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioB_8192ES_extlna_end - data_RadioB_8192ES_extlna_start);
			}
#endif
		}
#endif // IS_RTL8192E_SERIES
	}
	break;
#endif // CFG_HAL_HIGH_POWER_EXT_LNA
#if defined(CONFIG_WLAN_HAL_8814AE) || defined(CONFIG_WLAN_HAL_8814BE) || defined(CONFIG_WLAN_HAL_8198F)
	case HW_VAR_RFREGFILE_RADIO_C_START: {
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
            	printk("[%s][RadioC_8814A]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioC_8814A_start;
			} else { // mp chip
                printk("[%s][RadioC_8814Amp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioC_8814Amp_start;
			}
		}
#endif //IS_RTL8814A_SERIES
#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            printk("[%s][RadioC_8814B]\n",__FUNCTION__);
            *((unsigned long*)(val)) = (unsigned long)data_RadioC_8814B_start; //need radio C file, yllin8814B
        }
#endif //IS_RTL8814B_SERIES
#if IS_RTL8198F_SERIES
		if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
			printk("[%s][RadioC_8198F]\n",__FUNCTION__);
			*((dma_addr_t*)(val)) = (dma_addr_t)data_RadioC_8198F_start;
		}
#endif //IS_RTL8198F_SERIES


	}
	break;

#if CFG_HAL_HIGH_POWER_EXT_PA
	case HW_VAR_RFREGFILE_RADIO_C_HP_START: {
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
                printk("[%s][RadioC_8814A_hp]\n",__FUNCTION__);
                *((unsigned long*)(val)) = (unsigned long)data_RadioC_8814A_hp_start;
			} else { // mp chip
                printk("[%s][RadioC_8814Amp_hp]\n",__FUNCTION__); 
                *((unsigned long*)(val)) = (unsigned long)data_RadioC_8814Amp_hp_start;
			}
		}
#endif //IS_RTL8814A_SERIES

#if IS_RTL8198F_SERIES
		if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
			printk("[%s][RadioC_8198F]\n",__FUNCTION__);
			*((dma_addr_t*)(val)) = (dma_addr_t)data_RadioC_8198F_start;
		}
#endif //IS_RTL8198F_SERIES


	}
	break;
#endif

	case HW_VAR_RFREGFILE_RADIO_C_SIZE: {
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioC_8814A_end - data_RadioC_8814A_start);
			} else { // mp chip
				//printk("Get MP Radio C len \n");
				*((pu4Byte)(val)) = (u4Byte)(data_RadioC_8814Amp_end - data_RadioC_8814Amp_start);
			}
		}
#endif //IS_RTL8814A_SERIES
#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_RadioC_8814B_end - data_RadioC_8814B_start);
            printk("[%s][RadioC_8814B size] %d\n", __func__, *((pu4Byte)(val)));
        }
#endif //IS_RTL8814A_SERIES
#if IS_RTL8198F_SERIES
		if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
			//printk("Get MP Radio C len \n");
			*((pu4Byte)(val)) = (u4Byte)(data_RadioC_8198F_end - data_RadioC_8198F_start); 
		}
#endif //IS_RTL8198F_SERIES


	}
	break;

#if CFG_HAL_HIGH_POWER_EXT_PA
	case HW_VAR_RFREGFILE_RADIO_C_HP_SIZE: {
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioC_8814A_hp_end - data_RadioC_8814A_hp_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_RadioC_8814Amp_hp_end - data_RadioC_8814Amp_hp_start);
			}
		}
#endif //IS_RTL8814A_SERIES

#if IS_RTL8198F_SERIES
		if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_RadioC_8198F_end - data_RadioC_8198F_start);
		}
#endif //IS_RTL8198F_SERIES

	}
	break;
#endif

	case HW_VAR_RFREGFILE_RADIO_D_START: {
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
           		printk("[%s][RadioD_8814A]\n",__FUNCTION__); 
                *((unsigned long*)(val)) = (unsigned long)data_RadioD_8814A_start;
			} else { // mp chip
                printk("[%s][RadioD_8814Amp]\n",__FUNCTION__); 
                *((unsigned long*)(val)) = (unsigned long)data_RadioD_8814Amp_start;
			}
		}
#endif //IS_RTL8814A_SERIES
#if IS_RTL8814B_SERIES
            if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
                printk("[%s][RadioD_8814B]\n",__FUNCTION__); 
                *((unsigned long*)(val)) = (unsigned long)data_RadioD_8814B_start; //need radio D file, yllin8814B
            }
#endif //IS_RTL8814B_SERIES
#if IS_RTL8198F_SERIES
            if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
                printk("[%s][RadioD_8198F]\n",__FUNCTION__); 
                *((dma_addr_t*)(val)) = (dma_addr_t)data_RadioD_8198F_start; 
            }
#endif //IS_RTL8198F_SERIES


	}
	break;

#if CFG_HAL_HIGH_POWER_EXT_PA
	case HW_VAR_RFREGFILE_RADIO_D_HP_START: {
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
                printk("[%s][RadioD_8814A_hp]\n",__FUNCTION__);  
                *((unsigned long*)(val)) = (unsigned long)data_RadioD_8814A_hp_start;
			} else { // mp chip
                printk("[%s][RadioD_8814Amp_hp]\n",__FUNCTION__);     
                *((unsigned long*)(val)) = (unsigned long)data_RadioD_8814Amp_hp_start;
			}
		}
#endif //IS_RTL8814A_SERIES

#if IS_RTL8198F_SERIES
		if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
			printk("[%s][RadioD_8198F]\n",__FUNCTION__);	  
			*((dma_addr_t*)(val)) = (dma_addr_t)data_RadioD_8198F_start; 
		}
#endif //IS_RTL8198F_SERIES


	}
	break;
#endif

	case HW_VAR_RFREGFILE_RADIO_D_SIZE: {
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioD_8814A_end - data_RadioD_8814A_start);
			} else { // mp chip
				//printk("Get MP Radio D len \n");
				*((pu4Byte)(val)) = (u4Byte)(data_RadioD_8814Amp_end - data_RadioD_8814Amp_start);
			}
		}
#endif //IS_RTL8814A_SERIES
#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_RadioD_8814B_end - data_RadioD_8814B_start);  //need radio D file, yllin8814B
            printk("[%s][RadioD_8814B size] %d\n", __func__, *((pu4Byte)(val)));
        }
#endif //IS_RTL8814B_SERIES
#if IS_RTL8198F_SERIES
		if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
			//printk("Get MP Radio D len \n");
			*((pu4Byte)(val)) = (u4Byte)(data_RadioD_8198F_end - data_RadioD_8198F_start);	
		}
#endif //IS_RTL8198F_SERIES

	}
	break;

#if CFG_HAL_HIGH_POWER_EXT_PA
	case HW_VAR_RFREGFILE_RADIO_D_HP_SIZE: {
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_RadioD_8814A_hp_end - data_RadioD_8814A_hp_start);
			} else { // mp chip
				*((pu4Byte)(val)) = (u4Byte)(data_RadioD_8814Amp_hp_end - data_RadioD_8814Amp_hp_start);
			}
		}
#endif //IS_RTL8814A_SERIES

#if IS_RTL8198F_SERIES
		if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_RadioD_8198F_end - data_RadioD_8198F_start); 
		}
#endif //IS_RTL8198F_SERIES

	}
	break;
#endif
#endif
	case HW_VAR_FWFILE_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter)) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((unsigned long*)(val)) = (unsigned long)data_RTL8881FW_Test_T_start;
			} else {
				*((unsigned long*)(val)) = (unsigned long)data_RTL8881FW_A_CUT_T_start;
			}
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
#if IS_EXIST_PCI
			if (Adapter->hci_type == RTL_HCI_PCIE) {
				if (IS_HAL_TEST_CHIP(Adapter)) {
					*((unsigned long*)(val)) = (unsigned long)data_rtl8192Efw_start;
				} else { // mp chip
					*((unsigned long*)(val)) = (unsigned long)data_rtl8192EfwMP_start;
				}
			}
#endif
#if IS_EXIST_USB
			if (Adapter->hci_type == RTL_HCI_USB) {
				*((unsigned long*)(val)) = (unsigned long)data_rtl8192EfwMP_start;
			}
#endif
#if IS_EXIST_SDIO
			if (Adapter->hci_type == RTL_HCI_SDIO) {
				*((unsigned long*)(val)) = (unsigned long)data_rtl8192ESfwMP_start;
			}
#endif
		}
#endif
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter)) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
            	printk("[%s][rtl8814Afw]\n",__FUNCTION__); 
                *((unsigned long*)(val)) = (unsigned long)data_rtl8814Afw_start;               
			} else {
            	printk("[%s][rtl8814AfwMP]\n",__FUNCTION__); 
                *((unsigned long*)(val)) = (unsigned long)data_rtl8814AfwMP_start;
			}
		}
#endif //IS_RTL8814A_SERIES

#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            printk("[%s][rtl8197Ffw]\n",__FUNCTION__); 
            *((unsigned long*)(val)) = (unsigned long)data_rtl8197Ffw_start;   
        }
#endif //IS_RTL8197F_SERIES
    
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            printk("[%s][rtl8198Ffw]\n",__FUNCTION__); 
            *((dma_addr_t*)(val)) = (dma_addr_t)data_rtl8198Ffw_start;   
        }
#endif //IS_RTL8198F_SERIES
    
#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            *((unsigned long*)(val)) = (unsigned long)data_rtl8822Bfw_start;   
        }
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            *((unsigned long*)(val)) = (unsigned long)data_rtl8822Cfw_start;   
        }
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            *((unsigned long*)(val)) = (unsigned long)data_rtl8812Ffw_start;   
        }
#endif //IS_RTL8812F_SERIES

#if IS_RTL8821C_SERIES
        if ( IS_HARDWARE_TYPE_8821C(Adapter) ) {
            *((unsigned long*)(val)) = (unsigned long)data_rtl8821Cfw_start;   
        }
#endif //IS_RTL8821C_SERIES

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            printk("[%s][rtl8814BfwMP]\n",__FUNCTION__); 
            *((unsigned long*)(val)) = (unsigned long)data_rtl8814BfwMP_start;
        }
#endif //IS_RTL8814B_SERIES
#if IS_RTL8192F_SERIES
		if ( IS_HARDWARE_TYPE_8192F(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_rtl8192Ffw_start;   
		}
#endif //IS_RTL8192F_SERIES
#if IS_RTL8197G_SERIES
            if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
                printk("[%s][rtl8197Gfw]\n",__FUNCTION__); 
                *((dma_addr_t*)(val)) = (dma_addr_t)data_rtl8197Gfw_start;   
            }
#endif //IS_RTL8197G_SERIES


	}
	break;

	case HW_VAR_FWEXTFILE_START: {
#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            printk("[%s][rtl8814BfwExtMP]\n",__FUNCTION__); 
            *((unsigned long*)(val)) = (unsigned long)data_rtl8814BfwExtMP_start;
        }
#endif //IS_RTL8814B_SERIES
	}
	break;

	case HW_VAR_FWFILE_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter)) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_RTL8881FW_Test_T_end - data_RTL8881FW_Test_T_start);
			} else {
				*((pu4Byte)(val)) = (u4Byte)(data_RTL8881FW_A_CUT_T_end - data_RTL8881FW_A_CUT_T_start);
			}
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
#if IS_EXIST_PCI
			if (Adapter->hci_type == RTL_HCI_PCIE) {
				if (IS_HAL_TEST_CHIP(Adapter)) {
					*((pu4Byte)(val)) = (u4Byte)(data_rtl8192Efw_end - data_rtl8192Efw_start);
				} else { // mp chip
					*((pu4Byte)(val)) = (u4Byte)(data_rtl8192EfwMP_end - data_rtl8192EfwMP_start);
				}
			}
#endif
#if IS_EXIST_USB
			if (Adapter->hci_type == RTL_HCI_USB) {
				*((pu4Byte)(val)) = (u4Byte)(data_rtl8192EfwMP_end - data_rtl8192EfwMP_start);
			}
#endif
#if IS_EXIST_SDIO
			if (Adapter->hci_type == RTL_HCI_SDIO) {
				*((pu4Byte)(val)) = (u4Byte)(data_rtl8192ESfwMP_end - data_rtl8192ESfwMP_start);
			}
#endif
		}
#endif
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter)) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_rtl8814Afw_end - data_rtl8814Afw_start);
			} else {
				*((pu4Byte)(val)) = (u4Byte)(data_rtl8814AfwMP_end - data_rtl8814AfwMP_start);
			}
		}
#endif //IS_RTL8814A_SERIES

#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            printk("[%s][rtl8197Ffw size]\n",__FUNCTION__); 
            *((pu4Byte)(val)) = (u4Byte)(data_rtl8197Ffw_end - data_rtl8197Ffw_start);  
        }
#endif //IS_RTL8197F_SERIES
        
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            printk("[%s][rtl8198Ffw size]\n",__FUNCTION__); 
            *((pu4Byte)(val)) = (u4Byte)(data_rtl8198Ffw_end - data_rtl8198Ffw_start);  
        }
#endif //IS_RTL8198F_SERIES
        
#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_rtl8822Bfw_end - data_rtl8822Bfw_start);   
        }
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_rtl8822Cfw_end - data_rtl8822Cfw_start);   
        }
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_rtl8812Ffw_end - data_rtl8812Ffw_start);   
        }
#endif //IS_RTL8812F_SERIES

#if IS_RTL8821C_SERIES
        if ( IS_HARDWARE_TYPE_8821C(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_rtl8821Cfw_end - data_rtl8821Cfw_start);   
        }
#endif //IS_RTL8821C_SERIES
#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            printk("[%s][rtl8814BfwMP size]\n",__FUNCTION__); 
            *((pu4Byte)(val)) = (u4Byte)(data_rtl8814BfwMP_end - data_rtl8814BfwMP_start);
        }
#endif //IS_RTL8814B_SERIES
#if IS_RTL8192F_SERIES
		if ( IS_HARDWARE_TYPE_8192F(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_rtl8192Ffw_end - data_rtl8192Ffw_start);   
		}
#endif //IS_RTL8192F_SERIES

#if IS_RTL8197G_SERIES
    if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
        printk("[%s][rtl8197Gfw size]\n",__FUNCTION__); 
        *((pu4Byte)(val)) = (u4Byte)(data_rtl8197Gfw_end - data_rtl8197Gfw_start);  
    }
#endif //IS_RTL8197G_SERIES
	}
	break;

	case HW_VAR_FWEXTFILE_SIZE: {
#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            printk("[%s][rtl8814BfwExtMP size]\n",__FUNCTION__); 
            *((pu4Byte)(val)) = (u4Byte)(data_rtl8814BfwExtMP_end - data_rtl8814BfwExtMP_start);
        }
#endif //IS_RTL8814B_SERIES
	}
	break;

	case HW_VAR_POWERTRACKINGFILE_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			if (get_bonding_type_8881A() == BOND_8881AM) {
				if (HAL_VAR_INTERLPA_8881A) {
					printk("[TxPowerTrack_AP_8881AMP_intpa]\n");
					*((unsigned long*)(val)) = (unsigned long)data_TxPowerTrack_AP_8881AMP_intpa_start;
				} else {
					printk("[TxPowerTrack_AP_8881AMP]\n");
					*((unsigned long*)(val)) = (unsigned long)data_TxPowerTrack_AP_8881AMP_start;
				}
			} else if (get_bonding_type_8881A() == BOND_8881AB) {
				if (HAL_VAR_INTERLPA_8881A) {
					printk("[TxPowerTrack_AP_8881ABP_intpa]\n");
					*((unsigned long*)(val)) = (unsigned long)data_TxPowerTrack_AP_8881ABP_intpa_start;
				} else {
					printk("[TxPowerTrack_AP_8881ABP]\n");
					*((unsigned long*)(val)) = (unsigned long)data_TxPowerTrack_AP_8881ABP_start;
				}
			} else if (get_bonding_type_8881A() == BOND_8881AN) {
				if (HAL_VAR_INTERLPA_8881A) {
					printk("[TxPowerTrack_AP_8881AN]\n");
					*((unsigned long*)(val)) = (unsigned long)data_TxPowerTrack_AP_8881AN_start;
				} else {
					printk("[TxPowerTrack_AP_8881AN_extpa]\n");
					*((unsigned long*)(val)) = (unsigned long)data_TxPowerTrack_AP_8881AN_extpa_start;
				}
			} else {
				printk("[TxPowerTrack_AP_8881A]\n");
				*((unsigned long*)(val)) = (unsigned long)data_TxPowerTrack_AP_8881A_start;
			}	
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			printk("[TxPowerTrack_AP]\n");
			*((unsigned long*)(val)) = (unsigned long)data_TxPowerTrack_AP_start;
		}
#endif // #if IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
                if(IS_HAL_TEST_CHIP(Adapter)) {
                    *((unsigned long*)(val)) = (unsigned long)data_TxPowerTrack_AP_8814A_start;
                } else {
                    *((unsigned long*)(val)) = (unsigned long)data_TxPowerTrack_AP_8814Amp_start;
                }
		}
#endif // #if IS_RTL8814A_SERIES
#if IS_RTL8814B_SERIES
				if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
						if(IS_HAL_TEST_CHIP(Adapter)) {
							*((unsigned long*)(val)) = (unsigned long)data_TxPowerTrack_AP_8814B_start;
						} else {
							if(HAL_RFE_TYPE == 0)	
								*((unsigned long*)(val)) = (unsigned long)data_TxPowerTrack_AP_8814Bmp_start;
							else
								*((unsigned long*)(val)) = (unsigned long)data_TxPowerTrack_AP_8814Bmp_start;
						}
				}
#endif
#if IS_RTL8198F_SERIES
		if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
				if(IS_HAL_TEST_CHIP(Adapter)) {
					*((unsigned long*)(val)) = (unsigned long)data_TxPowerTrack_AP_8198F_start;
				} else {
					if(HAL_RFE_TYPE == 0)	
						*((unsigned long*)(val)) = (unsigned long)data_TxPowerTrack_AP_8198Fmp_start;
					else
						*((unsigned long*)(val)) = (unsigned long)data_TxPowerTrack_AP_8198Fmp_start;
				}
		}
#endif

	}
	break;

	case HW_VAR_POWERTRACKINGFILE_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			if (get_bonding_type_8881A() == BOND_8881AM) {
				if (HAL_VAR_INTERLPA_8881A)
					*((pu4Byte)(val)) = (u4Byte)(data_TxPowerTrack_AP_8881AMP_intpa_end - data_TxPowerTrack_AP_8881AMP_intpa_start);
				else
					*((pu4Byte)(val)) = (u4Byte)(data_TxPowerTrack_AP_8881AMP_end - data_TxPowerTrack_AP_8881AMP_start);
			} else if (get_bonding_type_8881A() == BOND_8881AB) {
				if (HAL_VAR_INTERLPA_8881A)
					*((pu4Byte)(val)) = (u4Byte)(data_TxPowerTrack_AP_8881ABP_intpa_end - data_TxPowerTrack_AP_8881ABP_intpa_start);
				else
					*((pu4Byte)(val)) = (u4Byte)(data_TxPowerTrack_AP_8881ABP_end - data_TxPowerTrack_AP_8881ABP_start);
			} else if (get_bonding_type_8881A() == BOND_8881AN) {
				if (HAL_VAR_INTERLPA_8881A)
					*((pu4Byte)(val)) = (u4Byte)(data_TxPowerTrack_AP_8881AN_end - data_TxPowerTrack_AP_8881AN_start);
				else
					*((pu4Byte)(val)) = (u4Byte)(data_TxPowerTrack_AP_8881AN_extpa_end - data_TxPowerTrack_AP_8881AN_extpa_start);
			} else
				*((pu4Byte)(val)) = (u4Byte)(data_TxPowerTrack_AP_8881A_end - data_TxPowerTrack_AP_8881A_start);
		}
#endif
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_TxPowerTrack_AP_end - data_TxPowerTrack_AP_start);
		}
#endif // #if IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
               if(IS_HAL_TEST_CHIP(Adapter)) {
                    *((pu4Byte)(val)) = (u4Byte)(data_TxPowerTrack_AP_8814A_end - data_TxPowerTrack_AP_8814A_start);
               } else {
                    *((pu4Byte)(val)) = (u4Byte)(data_TxPowerTrack_AP_8814Amp_end - data_TxPowerTrack_AP_8814Amp_start);
               }
		}
#endif //IS_RTL8814A_SERIES	
#if IS_RTL8814B_SERIES
				if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
					   if(IS_HAL_TEST_CHIP(Adapter)) {
							*((pu4Byte)(val)) = (u4Byte)(data_TxPowerTrack_AP_8814B_end - data_TxPowerTrack_AP_8814B_start);
					   } else {
							if(HAL_RFE_TYPE == 0)	
								*((pu4Byte)(val)) = (u4Byte)(data_TxPowerTrack_AP_8814Bmp_end - data_TxPowerTrack_AP_8814Bmp_start);
							else
								*((pu4Byte)(val)) = (u4Byte)(data_TxPowerTrack_AP_8814Bmp_end - data_TxPowerTrack_AP_8814Bmp_start);
					   }
				}
#endif
#if IS_RTL8198F_SERIES
		if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
			   if(IS_HAL_TEST_CHIP(Adapter)) {
					*((pu4Byte)(val)) = (u4Byte)(data_TxPowerTrack_AP_8198F_end - data_TxPowerTrack_AP_8198F_start);
			   } else {
					if(HAL_RFE_TYPE == 0)	
						*((pu4Byte)(val)) = (u4Byte)(data_TxPowerTrack_AP_8198Fmp_end - data_TxPowerTrack_AP_8198Fmp_start);
					else
						*((pu4Byte)(val)) = (u4Byte)(data_TxPowerTrack_AP_8198Fmp_end - data_TxPowerTrack_AP_8198Fmp_start);
				}
		}
#endif

	}
	break;


#if IS_RTL8881A_SERIES
	case HW_VAR_TXPKTFWFILE_START: {
		if ( IS_HARDWARE_TYPE_8881A(Adapter)) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((unsigned long*)(val)) = (unsigned long)data_RTL8881TXBUF_Test_T_start;
			} else {
				*((unsigned long*)(val)) = (unsigned long)data_RTL8881TXBUF_A_CUT_T_start;
			}
		}
	}
	break;

	case HW_VAR_TXPKTFWFILE_SIZE: {
		if ( IS_HARDWARE_TYPE_8881A(Adapter)) {
			if (IS_HAL_TEST_CHIP(Adapter)) {
				*((pu4Byte)(val)) = (u4Byte)(data_RTL8881TXBUF_Test_T_end - data_RTL8881TXBUF_Test_T_start);
			} else {
				*((pu4Byte)(val)) = (u4Byte)(data_RTL8881TXBUF_A_CUT_T_end - data_RTL8881TXBUF_A_CUT_T_start);
			}
		}
	}
	break;
#endif
#endif

	case HW_VAR_MEDIA_STATUS: {
		val[0] = platform_efio_read_1byte(Adapter, REG_CR + 2) & 0x3;

		switch ( val[0] ) {
		case MSR_INFRA:
			val[0] = RT_OP_MODE_INFRASTRUCTURE;
			break;

		case MSR_ADHOC:
			val[0] = RT_OP_MODE_IBSS;
			break;

		case MSR_AP:
			val[0] = RT_OP_MODE_AP;
			break;

		default:
			val[0] = RT_OP_MODE_NO_LINK;
			break;
		}
	}
	break;

	case HW_VAR_SECURITY_CONFIG: {
		u2Byte                      SecCfgReg;
		SECURITY_CONFIG_OPERATION   SecCfg = 0;

		SecCfgReg = HAL_RTL_R16(REG_SECCFG);

		if (SecCfgReg & BIT_TXUHUSEDK) {
			SecCfg |= SCO_TXUSEDK;
		}

		if (SecCfgReg & BIT_RXUHUSEDK) {
			SecCfg |= SCO_RXUSEDK;
		}

		if (SecCfgReg & BIT_TXENC) {
			SecCfg |= SCO_TXENC;
		}

		if (SecCfgReg & BIT_RXDEC) {
			SecCfg |= SCO_RXDEC;
		}

		if (SecCfgReg & BIT_SKBYA2) {
			SecCfg |= SCO_SKBYA2;
		}

		if (SecCfgReg & BIT_NOSKMC) {
			SecCfg |= SCO_NOSKMC;
		}

		if (SecCfgReg & BIT_TXBCUSEDK) {
			SecCfg |= SCO_TXBCUSEDK;
		}

		if (SecCfgReg & BIT_RXBCUSEDK) {
			SecCfg |= SCO_RXBCUSEDK;
		}

		if (SecCfgReg & BIT_CHK_KEYID) {
			SecCfg |= SCO_CHK_KEYID;
		}

		*((PSECURITY_CONFIG_OPERATION)(val)) = SecCfg;
	}
	break;

	case HW_VAR_BEACON_INTERVAL: {
#if IS_RTL88XX_MAC_V4
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)
            *((pu2Byte)(val)) = platform_efio_read_2byte(Adapter, REG_BCN_SPACE_CFG);
#endif
#if IS_RTL88XX_MAC_V1_V2_V3
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3)
		    *((pu2Byte)(val)) = platform_efio_read_2byte(Adapter, REG_MBSSID_BCN_SPACE);
#endif
	}
	break;

	case HW_VAR_TXPAUSE: {
		*((pu1Byte)(val)) = platform_efio_read_1byte(Adapter, REG_TXPAUSE);
	}
	break;

	case HW_VAR_HIQ_NO_LMT_EN: {
#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8197F_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
        if( IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8881A(Adapter) ||IS_HARDWARE_TYPE_8814A(Adapter) ||IS_HARDWARE_TYPE_8822B(Adapter)|| IS_HARDWARE_TYPE_8197F(Adapter) ||IS_HARDWARE_TYPE_8822C(Adapter) ||IS_HARDWARE_TYPE_8812F(Adapter)) {
    		*((pu1Byte)(val)) = platform_efio_read_1byte(Adapter, REG_HIQ_NO_LMT_EN);
        }
#endif //(IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8197F_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
#if IS_RTL8198F_SERIES || IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter) ) {
    		*((pu2Byte)(val)) = platform_efio_read_2byte(Adapter, REG_HIQ_NO_LMT_EN_V2);                        
        }
#endif //#if IS_RTL8198F_SERIES || IS_RTL8197G_SERIES
#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            *((pu2Byte)(val)) = platform_efio_read_2byte(Adapter, REG_HIQ_NO_LMT_EN_V1); // two bytes,   only port0                      
        }
#endif //#if IS_RTL8814B_SERIES

	}
	break;
    case HW_VAR_MBSSID_EN:{
#if IS_RTL88XX_MAC_V1_V2
	if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2)  {
    		*((pu1Byte)(val)) = platform_efio_read_1byte(Adapter, REG_MBSSID_CTRL);
        }
#endif //IS_RTL88XX_MAC_V1_V2
#if IS_RTL88XX_MAC_V3
        if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v3) {
    		*((pu2Byte)(val)) = platform_efio_read_2byte(Adapter, REG_MBID_BCNQ_EN);                        
        }
#endif //#if IS_RTL88XX_MAC_V3
#if IS_RTL88XX_MAC_V4
        if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4) {
            *((pu2Byte)(val)) = platform_efio_read_2byte(Adapter, REG_MBSSID_CTRL_V1); 
        }
#endif
    }	
	break;
	case HW_VAR_DRV_DBG: {
		*((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, REGDUMP_DRV_ERR0);
	}
	break;


	case HW_VAR_NUM_TXDMA_STATUS: {
		*((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, REG_TXDMA_STATUS);
	}
	break;

	case HW_VAR_NUM_RXDMA_STATUS: {
		*((pu1Byte)(val)) = platform_efio_read_1byte(Adapter, REG_RXDMA_STATUS);
	}
	break;

    case HW_VAR_TSF_TIMER: {
#if IS_RTL88XX_MAC_V4
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)
            *((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, REG_TSFTR_LOW); //if need high 32 bits, need to add another entry //yllin8814B
#endif
#if IS_RTL88XX_MAC_V1_V2_V3
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3)
            *((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, REG_TSFTR);
#endif
    }
    break;

    case HW_VAR_FREERUN_TIMER: {
#if IS_RTL88XX_MAC_V4
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)
            *((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, REG_FREERUN_CNT_LOW);
#endif
#if IS_RTL88XX_MAC_V1_V2_V3
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3) {
#if (IS_RTL8192E_SERIES || IS_RTL8192F_SERIES || IS_RTL8881A_SERIES || IS_RTL8822B_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
            *((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, 0x0568); //workaround the compile errors for 22b+97f platform
#else
            *((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, REG_FREERUN_CNT);
#endif
        }
#endif
    }
    break;

    case HW_VAR_PS_TIMER: {
#if IS_RTL88XX_MAC_V1
		if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1) {        
            *((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, REG_PS_TIMER);
        }
#endif  //IS_RTL88XX_MAC_V1        
#if (IS_RTL88XX_MAC_V2_V3)
    if( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3) {
            *((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, REG_PS_TIMER0);
        }
#endif  //(IS_RTL88XX_MAC_V2_V3)         
#if IS_RTL88XX_MAC_V4
        if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4) {    
            *((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, REG_PS_TIMER_0);
        }
#endif 
         
    }
    break;
	case HW_VAR_NUM_TOTAL_RF_PATH: {
#if (IS_RTL8881A_SERIES || IS_RTL8821C_SERIES)
		if ( IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)1;
		}
#endif
#if (IS_RTL8192E_SERIES || IS_RTL8192F_SERIES || IS_RTL8197F_SERIES || IS_RTL8822B_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES|| IS_RTL8197G_SERIES)
		if ( IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8192F(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8822B(Adapter)|| IS_HARDWARE_TYPE_8822C(Adapter)|| IS_HARDWARE_TYPE_8812F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter)) {
			*((pu4Byte)(val)) = (u4Byte)2;
		}
#endif
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)3;
		}
#endif
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)4;
    	}
#endif //#if IS_RTL8198F_SERIES
#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)4;
        }
#endif //#if IS_RTL8814B_SERIES

	}
	break;

#if CFG_HAL_MACDM
	case HW_VAR_MACDM_DEF_LOW_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_low_8881A_start;
		}
#endif //IS_RTL8881A_SERIES
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_low_8192E_start;
		}
#endif //IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
        	printk("[%s][MACDM_def_low_8814A]\n",__FUNCTION__); 
            *((unsigned long*)(val)) = (unsigned long)data_MACDM_def_low_8814A_start;
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            printk("[%s][MACDM_def_low_8197F]\n",__FUNCTION__); 
            *((unsigned long*)(val)) = (unsigned long)data_MACDM_def_low_8197F_start;   
        }
#endif //IS_RTL8197F_SERIES
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            printk("[%s][MACDM_def_low_8198F]\n",__FUNCTION__); 
            *((dma_addr_t*)(val)) = (dma_addr_t)data_MACDM_def_low_8198F_start;   
        }
#endif //IS_RTL8198F_SERIES        
#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            printk("[%s][MACDM_def_low_8822B]\n",__FUNCTION__); 
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_low_8822B_start;   
        }
#endif //IS_RTL8822B_SERIES
#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            printk("[%s][MACDM_def_low_8822C]\n",__FUNCTION__); 
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_low_8822C_start;   
        }
#endif //IS_RTL8822C_SERIES
#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            printk("[%s][MACDM_def_low_8812F]\n",__FUNCTION__); 
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_low_8812F_start;   
        }
#endif //IS_RTL8812F_SERIES
#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            printk("[%s][MACDM_def_low_8814B]\n",__FUNCTION__); 
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_low_8814B_start;    //yllin8814B
        }
#endif //IS_RTL8814B_SERIES
#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            printk("[%s][MACDM_def_low_8197G]\n",__FUNCTION__); 
            *((dma_addr_t*)(val)) = (dma_addr_t)data_MACDM_def_low_8197G_start;   
        }
#endif //IS_RTL8197G_SERIES 
	}
	break;

	case HW_VAR_MACDM_DEF_LOW_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_low_8881A_end - data_MACDM_def_low_8881A_start);
		}
#endif //IS_RTL8881A_SERIES
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_low_8192E_end - data_MACDM_def_low_8192E_start);
		}
#endif //IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_low_8814A_end - data_MACDM_def_low_8814A_start);
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
    if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
        *((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_low_8197F_end - data_MACDM_def_low_8197F_start);   
    }
#endif //IS_RTL8197F_SERIES
    
#if IS_RTL8198F_SERIES
    if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
        *((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_low_8198F_end - data_MACDM_def_low_8198F_start);   
    }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
    if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
        //*((pu4Byte)(val)) = (u4Byte)data_MACDM_def_low_8822B_start;   
    }
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
    if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
        //*((pu4Byte)(val)) = (u4Byte)data_MACDM_def_low_8822C_start;   
    }
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
    if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
        //*((pu4Byte)(val)) = (u4Byte)data_MACDM_def_low_8812F_start;   
    }
#endif //IS_RTL8812F_SERIES
#if IS_RTL8197G_SERIES
    if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
        *((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_low_8197G_end - data_MACDM_def_low_8197G_start);   
    }
#endif //IS_RTL8197G_SERIES
#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)data_MACDM_def_low_8814B_start;   //yllin8814B
        }
#endif //IS_RTL8814B_SERIES

	}
	break;


	case HW_VAR_MACDM_DEF_NORMAL_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_normal_8881A_start;
		}
#endif //IS_RTL8881A_SERIES
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_normal_8192E_start;
		}
#endif //IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
        	printk("[%s][MACDM_def_normal_8814A]\n",__FUNCTION__); 
            *((unsigned long*)(val)) = (unsigned long)data_MACDM_def_normal_8814A_start;
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            printk("[%s][MACDM_def_normal_8197F]\n",__FUNCTION__); 
            *((unsigned long*)(val)) = (unsigned long)data_MACDM_def_normal_8197F_start;   
        }
#endif //IS_RTL8197F_SERIES
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            printk("[%s][MACDM_def_normal_8198F]\n",__FUNCTION__); 
            *((dma_addr_t*)(val)) = (dma_addr_t)data_MACDM_def_normal_8198F_start;   
        }
#endif //IS_RTL8198F_SERIES        
#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            printk("[%s][MACDM_def_normal_8822B]\n",__FUNCTION__); 
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_normal_8822B_start;   
        }
#endif //IS_RTL8822B_SERIES
#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            printk("[%s][MACDM_def_normal_8822C]\n",__FUNCTION__); 
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_normal_8822C_start;   
        }
#endif //IS_RTL8822C_SERIES
#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            printk("[%s][MACDM_def_normal_8812F]\n",__FUNCTION__); 
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_normal_8812F_start;   
        }
#endif //IS_RTL8812F_SERIES
#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            printk("[%s][MACDM_def_normal_8814B]\n",__FUNCTION__); 
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_normal_8814B_start;   //yllin8814B
        }
#endif //IS_RTL8814B_SERIES
#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            printk("[%s][MACDM_def_normal_8197G]\n",__FUNCTION__); 
            *((dma_addr_t*)(val)) = (dma_addr_t)data_MACDM_def_normal_8197G_start;   
        }
#endif //IS_RTL8197G_SERIES   
	}
	break;

	case HW_VAR_MACDM_DEF_NORMAL_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_normal_8881A_end - data_MACDM_def_normal_8881A_start);
		}
#endif //IS_RTL8881A_SERIES
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_normal_8192E_end - data_MACDM_def_normal_8192E_start);
		}
#endif //IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_normal_8814A_end - data_MACDM_def_normal_8814A_start);
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_normal_8197F_end - data_MACDM_def_normal_8197F_start);   
        }
#endif //IS_RTL8197F_SERIES
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_normal_8198F_end - data_MACDM_def_normal_8198F_start);   
        }
#endif //IS_RTL8198F_SERIES        
#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_normal_8822B_end - data_MACDM_def_normal_8822B_start);   
        }
#endif //IS_RTL8822B_SERIES
#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_normal_8822C_end - data_MACDM_def_normal_8822C_start);   
        }
#endif //IS_RTL8822C_SERIES
#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_normal_8812F_end - data_MACDM_def_normal_8812F_start);   
        }
#endif //IS_RTL8812F_SERIES
#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_normal_8814B_end - data_MACDM_def_normal_8814B_start);    //yllin8814B
        }
#endif //IS_RTL8814B_SERIES
#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_normal_8197G_end - data_MACDM_def_normal_8197G_start);   
        }
#endif //IS_RTL8197G_SERIES

	}
	break;


	case HW_VAR_MACDM_DEF_HIGH_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_high_8881A_start;
		}
#endif //IS_RTL8881A_SERIES
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_high_8192E_start;
		}
#endif //IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
       		printk("[%s][MACDM_def_high_8814A]\n",__FUNCTION__); 
            *((unsigned long*)(val)) = (unsigned long)data_MACDM_def_high_8814A_start;
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            printk("[%s][MACDM_def_low_8197F]\n",__FUNCTION__); 
            *((unsigned long*)(val)) = (unsigned long)data_MACDM_def_high_8197F_start;   
        }
#endif //IS_RTL8197F_SERIES
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            printk("[%s][MACDM_def_low_8198F]\n",__FUNCTION__); 
            *((dma_addr_t*)(val)) = (dma_addr_t)data_MACDM_def_high_8198F_start;   
        }
#endif //IS_RTL8198F_SERIES        
#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            printk("[%s][MACDM_def_low_8822B]\n",__FUNCTION__); 
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_high_8822B_start;   
        }
#endif //IS_RTL8822B_SERIES
#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            printk("[%s][MACDM_def_low_8822C]\n",__FUNCTION__); 
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_high_8822C_start;   
        }
#endif //IS_RTL8822C_SERIES
#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            printk("[%s][MACDM_def_low_8812F]\n",__FUNCTION__); 
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_high_8812F_start;   
        }
#endif //IS_RTL8812F_SERIES
#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            printk("[%s][MACDM_def_low_8814B]\n",__FUNCTION__); 
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_high_8814B_start;   //yllin8814B
        }
#endif //IS_RTL8814B_SERIES
#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            printk("[%s][MACDM_def_low_8197G]\n",__FUNCTION__); 
            *((dma_addr_t*)(val)) = (dma_addr_t)data_MACDM_def_high_8197G_start;   
        }
#endif //IS_RTL8197G_SERIES 
	}
	break;

	case HW_VAR_MACDM_DEF_HIGH_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_high_8881A_end - data_MACDM_def_high_8881A_start);
		}
#endif //IS_RTL8881A_SERIES
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_high_8192E_end - data_MACDM_def_high_8192E_start);
		}
#endif //IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_high_8814A_end - data_MACDM_def_high_8814A_start);
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_high_8197F_end - data_MACDM_def_high_8197F_start);
        }
#endif //IS_RTL8197F_SERIES
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_high_8198F_end - data_MACDM_def_high_8198F_start);
        }
#endif //IS_RTL8198F_SERIES        
#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_high_8822B_end - data_MACDM_def_high_8822B_start);
        }
#endif //IS_RTL8822B_SERIES
#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_high_8822C_end - data_MACDM_def_high_8822C_start);
        }
#endif //IS_RTL8822C_SERIES
#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_high_8812F_end - data_MACDM_def_high_8812F_start);
        }
#endif //IS_RTL8812F_SERIES
#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_high_8814B_end - data_MACDM_def_high_8814B_start); //yllin8814B
        }
#endif //IS_RTL8814B_SERIES
#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_def_high_8197G_end - data_MACDM_def_high_8197G_start);
        }
#endif //IS_RTL8197G_SERIES   
	}
	break;


	case HW_VAR_MACDM_GEN_LOW_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_gen_low_8881A_start;
		}
#endif //IS_RTL8881A_SERIES
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_gen_low_8192E_start;
		}
#endif //IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
        	printk("[%s][MACDM_gen_low_8814A]\n",__FUNCTION__);
            *((unsigned long*)(val)) = (unsigned long)data_MACDM_gen_low_8814A_start;
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            printk("[%s][MACDM_def_low_8197F]\n",__FUNCTION__); 
            *((unsigned long*)(val)) = (unsigned long)data_MACDM_def_low_8197F_start;   
        }
#endif //IS_RTL8197F_SERIES
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            printk("[%s][MACDM_def_low_8198F]\n",__FUNCTION__); 
            *((dma_addr_t*)(val)) = (dma_addr_t)data_MACDM_def_low_8198F_start;   
        }
#endif //IS_RTL8198F_SERIES        
#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            printk("[%s][MACDM_def_low_8822B]\n",__FUNCTION__); 
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_low_8822B_start;   
        }
#endif //IS_RTL8822B_SERIES
#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            printk("[%s][MACDM_def_low_8822C]\n",__FUNCTION__); 
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_low_8822C_start;   
        }
#endif //IS_RTL8822C_SERIES
#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            printk("[%s][MACDM_def_low_8812F]\n",__FUNCTION__); 
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_low_8812F_start;   
        }
#endif //IS_RTL8812F_SERIES
#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            printk("[%s][MACDM_def_low_8814B]\n",__FUNCTION__); 
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_def_low_8814B_start;   //yllin8814B 
        }
#endif //IS_RTL8814B_SERIES
#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            printk("[%s][MACDM_def_low_8197G]\n",__FUNCTION__); 
            *((dma_addr_t*)(val)) = (dma_addr_t)data_MACDM_def_low_8197G_start;   
        }
#endif //IS_RTL8197G_SERIES  
	}
	break;

	case HW_VAR_MACDM_GEN_LOW_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_low_8881A_end - data_MACDM_gen_low_8881A_start);
		}
#endif //IS_RTL8881A_SERIES
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_low_8192E_end - data_MACDM_gen_low_8192E_start);
		}
#endif //IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_low_8814A_end - data_MACDM_gen_low_8814A_start);
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_low_8197F_end - data_MACDM_gen_low_8197F_start);
        }
#endif //IS_RTL8197F_SERIES
        
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_low_8198F_end - data_MACDM_gen_low_8198F_start);
        }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_low_8822B_end - data_MACDM_gen_low_8822B_start);   
        }
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_low_8822C_end - data_MACDM_gen_low_8822C_start);   
        }
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_low_8812F_end - data_MACDM_gen_low_8812F_start);   
        }
#endif //IS_RTL8812F_SERIES

#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_low_8197G_end - data_MACDM_gen_low_8197G_start);
        }
#endif //IS_RTL8197G_SERIES

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_low_8814B_end - data_MACDM_gen_low_8814B_start);   //yllin8814B
        }
#endif //IS_RTL8814B_SERIES

	}
	break;


	case HW_VAR_MACDM_GEN_NORMAL_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_gen_normal_8881A_start;
		}
#endif //IS_RTL8881A_SERIES
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_gen_normal_8192E_start;
		}
#endif //IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_gen_normal_8814A_start;
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            *((unsigned long*)(val)) = (unsigned long)data_MACDM_gen_normal_8197F_start;   
        }
#endif //IS_RTL8197F_SERIES
        
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            *((dma_addr_t*)(val)) = (dma_addr_t)data_MACDM_gen_normal_8198F_start;   
        }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_gen_normal_8822B_start;   
        }
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_gen_normal_8822C_start;   
        }
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_gen_normal_8812F_start;   
        }
#endif //IS_RTL8812F_SERIES

#if IS_RTL8814B_SERIES
            if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
                //*((unsigned long*)(val)) = (unsigned long)data_MACDM_gen_normal_8814B_start;   //yllin8814B
            }
#endif //IS_RTL8814B_SERIES

#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            *((dma_addr_t*)(val)) = (dma_addr_t)data_MACDM_gen_normal_8197G_start;   
        }
#endif //IS_RTL8197G_SERIES
	}
	break;

	case HW_VAR_MACDM_GEN_NORMAL_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_normal_8881A_end - data_MACDM_gen_normal_8881A_start);
		}
#endif //IS_RTL8881A_SERIES
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_normal_8192E_end - data_MACDM_gen_normal_8192E_start);
		}
#endif //IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_normal_8814A_end - data_MACDM_gen_normal_8814A_start);
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_normal_8197F_end - data_MACDM_gen_normal_8197F_end);   
        }
#endif //IS_RTL8197F_SERIES
        
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_normal_8198F_end - data_MACDM_gen_normal_8198F_end);   
        }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_normal_8822B_end - data_MACDM_gen_normal_8822B_end); 
        }
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_normal_8822C_end - data_MACDM_gen_normal_8822C_end); 
        }
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_normal_8812F_end - data_MACDM_gen_normal_8812F_end); 
        }
#endif //IS_RTL8812F_SERIES

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_normal_8814B_end - data_MACDM_gen_normal_8814B_end);  //yllin8814B
        }
#endif //IS_RTL8814B_SERIES

#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_normal_8197G_end - data_MACDM_gen_normal_8197G_end);   
        }
#endif //IS_RTL8197G_SERIES

	}
	break;


	case HW_VAR_MACDM_GEN_HIGH_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_gen_high_8881A_start;
		}
#endif //IS_RTL8881A_SERIES
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_gen_high_8192E_start;
		}
#endif //IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_gen_high_8814A_start;
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            *((unsigned long*)(val)) = (unsigned long)data_MACDM_gen_high_8197F_start;   
        }
#endif //IS_RTL8197F_SERIES
        
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            *((dma_addr_t*)(val)) = (dma_addr_t)data_MACDM_gen_high_8198F_start;   
        }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_gen_high_8822B_start;   
        }
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_gen_high_8822C_start;   
        }
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_gen_high_8812F_start;   
        }
#endif //IS_RTL8812F_SERIES

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_gen_high_8814B_start;   //yllin8814B
        }
#endif //IS_RTL8814B_SERIES

#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            *((dma_addr_t*)(val)) = (dma_addr_t)data_MACDM_gen_high_8197G_start;   
        }
#endif //IS_RTL8197G_SERIES

	}
	break;

	case HW_VAR_MACDM_GEN_HIGH_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_high_8881A_end - data_MACDM_gen_high_8881A_start);
		}
#endif //IS_RTL8881A_SERIES
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_high_8192E_end - data_MACDM_gen_high_8192E_start);
		}
#endif //IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_high_8814A_end - data_MACDM_gen_high_8814A_start);
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_high_8197F_end - data_MACDM_gen_high_8197F_start);  
        }
#endif //IS_RTL8197F_SERIES
        
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_high_8198F_end - data_MACDM_gen_high_8198F_start);  
        }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_high_8822B_end - data_MACDM_gen_high_8822B_start);   
        }
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_high_8822C_end - data_MACDM_gen_high_8822C_start);   
        }
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_high_8812F_end - data_MACDM_gen_high_8812F_start);   
        }
#endif //IS_RTL8812F_SERIES

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_high_8814B_end - data_MACDM_gen_high_8814B_start);   //yllin8814B
        }
#endif //IS_RTL8814B_SERIES

#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_gen_high_8197G_end - data_MACDM_gen_high_8197G_start);  
        }
#endif //IS_RTL8197G_SERIES

	}
	break;


	case HW_VAR_MACDM_TXOP_LOW_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_low_8881A_start;
		}
#endif //IS_RTL8881A_SERIES
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_low_8192E_start;
		}
#endif //IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_low_8814A_start;
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            *((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_low_8197F_start;   
        }
#endif //IS_RTL8197F_SERIES
        
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            *((dma_addr_t*)(val)) = (dma_addr_t)data_MACDM_txop_low_8198F_start;   
        }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_low_8822B_start;    
        }
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_low_8822C_start;    
        }
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_low_8812F_start;    
        }
#endif //IS_RTL8812F_SERIES

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_low_8814B_start;    //yllin8814B
        }
#endif //IS_RTL8814B_SERIES

#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            *((dma_addr_t*)(val)) = (dma_addr_t)data_MACDM_txop_low_8197G_start;   
        }
#endif //IS_RTL8197G_SERIES

	}
	break;

	case HW_VAR_MACDM_TXOP_LOW_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_low_8881A_end - data_MACDM_txop_low_8881A_start);
		}
#endif //IS_RTL8881A_SERIES
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_low_8192E_end - data_MACDM_txop_low_8192E_start);
		}
#endif //IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_low_8814A_end - data_MACDM_txop_low_8814A_start);
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_low_8197F_end - data_MACDM_txop_low_8197F_start);   
        }
#endif //IS_RTL8197F_SERIES
        
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_low_8198F_end - data_MACDM_txop_low_8198F_start);   
        }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
           // *((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_low_8822B_end - data_MACDM_txop_low_8822B_start);      
        }
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
           // *((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_low_8822C_end - data_MACDM_txop_low_8822C_start);      
        }
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
           // *((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_low_8812F_end - data_MACDM_txop_low_8812F_start);      
        }
#endif //IS_RTL8812F_SERIES

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
           // *((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_low_8814B_end - data_MACDM_txop_low_8814B_start);      //yllin8814B
        }
#endif //IS_RTL8814B_SERIES

#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_low_8197G_end - data_MACDM_txop_low_8197G_start);   
        }
#endif //IS_RTL8197G_SERIES

	}
	break;


	case HW_VAR_MACDM_TXOP_NORMAL_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_normal_8881A_start;
		}
#endif //IS_RTL8881A_SERIES
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_normal_8192E_start;
		}
#endif //IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_normal_8814A_start;
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            *((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_normal_8197F_start;   
        }
#endif //IS_RTL8197F_SERIES
        
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            *((dma_addr_t*)(val)) = (dma_addr_t)data_MACDM_txop_normal_8198F_start;   
        }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_normal_8822B_start;   
        }
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_normal_8822C_start;   
        }
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_normal_8812F_start;   
        }
#endif //IS_RTL8812F_SERIES

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_normal_8814B_start;   //yllin8814B
        }
#endif //IS_RTL8814B_SERIES

#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            *((dma_addr_t*)(val)) = (dma_addr_t)data_MACDM_txop_normal_8197G_start;   
        }
#endif //IS_RTL8197G_SERIES

	}
	break;

	case HW_VAR_MACDM_TXOP_NORMAL_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_normal_8881A_end - data_MACDM_txop_normal_8881A_start);
		}
#endif //IS_RTL8881A_SERIES
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_normal_8192E_end - data_MACDM_txop_normal_8192E_start);
		}
#endif //IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_normal_8814A_end - data_MACDM_txop_normal_8814A_start);
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_normal_8197F_end - data_MACDM_txop_normal_8197F_start); 
        }
#endif //IS_RTL8197F_SERIES
        
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_normal_8198F_end - data_MACDM_txop_normal_8198F_start); 
        }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
           // *((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_normal_8822B_end - data_MACDM_txop_normal_8822B_start);  
        }
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
           // *((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_normal_8822B_end - data_MACDM_txop_normal_8822C_start);  
        }
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
           // *((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_normal_8822B_end - data_MACDM_txop_normal_8812F_start);  
        }
#endif //IS_RTL8812F_SERIES

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
           // *((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_normal_8814B_end - data_MACDM_txop_normal_8814B_start);   //yllin8814B
        }
#endif //IS_RTL8814B_SERIES

#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_normal_8197G_end - data_MACDM_txop_normal_8197G_start); 
        }
#endif //IS_RTL8197G_SERIES

	}
	break;


	case HW_VAR_MACDM_TXOP_HIGH_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_high_8881A_start;
		}
#endif //IS_RTL8881A_SERIES
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_high_8192E_start;
		}
#endif //IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_high_8814A_start;
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            *((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_high_8197F_start;  
        }
#endif //IS_RTL8197F_SERIES
        
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            *((dma_addr_t*)(val)) = (dma_addr_t)data_MACDM_txop_high_8198F_start;  
        }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_high_8822B_start; 
        }
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_high_8822C_start; 
        }
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_high_8812F_start; 
        }
#endif //IS_RTL8812F_SERIES

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_txop_high_8814B_start;  //yllin8814B
        }
#endif //IS_RTL8814B_SERIES

#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            *((dma_addr_t*)(val)) = (dma_addr_t)data_MACDM_txop_high_8197G_start;  
        }
#endif //IS_RTL8197G_SERIES

	}
	break;

	case HW_VAR_MACDM_TXOP_HIGH_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_high_8881A_end - data_MACDM_txop_high_8881A_start);
		}
#endif //IS_RTL8881A_SERIES
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_high_8192E_end - data_MACDM_txop_high_8192E_start);
		}
#endif //IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_high_8814A_end - data_MACDM_txop_high_8814A_start);
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_high_8197F_end - data_MACDM_txop_high_8197F_start);  
        }
#endif //IS_RTL8197F_SERIES
        
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_high_8198F_end - data_MACDM_txop_high_8198F_start);  
        }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_high_8822B_end - data_MACDM_txop_high_8822B_start); 
        }
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_high_8822C_end - data_MACDM_txop_high_8822C_start); 
        }
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_high_8812F_end - data_MACDM_txop_high_8812F_start); 
        }
#endif //IS_RTL8812F_SERIES

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_high_8814B_end - data_MACDM_txop_high_8814B_start);  //yllin8814B
        }
#endif //IS_RTL8814B_SERIES

#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_txop_high_8197G_end - data_MACDM_txop_high_8197G_start);  
        }
#endif //IS_RTL8197G_SERIES

	}
	break;

	case HW_VAR_MACDM_CRITERIA_START: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_state_criteria_8881A_start;
		}
#endif //IS_RTL8881A_SERIES
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_state_criteria_8192E_start;
		}
#endif //IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			*((unsigned long*)(val)) = (unsigned long)data_MACDM_state_criteria_8814A_start;
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            *((unsigned long*)(val)) = (unsigned long)data_MACDM_state_criteria_8197F_start;   
        }
#endif //IS_RTL8197F_SERIES
        
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            *((dma_addr_t*)(val)) = (dma_addr_t)data_MACDM_state_criteria_8198F_start;   
        }
#endif //IS_RTL8198F_SERIES
        
#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_state_criteria_8822B_start;
        }
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_state_criteria_8822C_start;
        }
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_state_criteria_8812F_start;
        }
#endif //IS_RTL8812F_SERIES

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            //*((unsigned long*)(val)) = (unsigned long)data_MACDM_state_criteria_8814B_start; //yllin8814B
        }
#endif //IS_RTL8814B_SERIES

#if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            *((dma_addr_t*)(val)) = (dma_addr_t)data_MACDM_state_criteria_8197G_start;   
        }
#endif //IS_RTL8197G_SERIES

	}
	break;

	case HW_VAR_MACDM_CRITERIA_SIZE: {
#if IS_RTL8881A_SERIES
		if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_state_criteria_8881A_end - data_MACDM_state_criteria_8881A_start);
		}
#endif //IS_RTL8881A_SERIES
#if IS_RTL8192E_SERIES
		if ( IS_HARDWARE_TYPE_8192E(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_state_criteria_8192E_end - data_MACDM_state_criteria_8192E_start);
		}
#endif //IS_RTL8192E_SERIES
#if IS_RTL8814A_SERIES
		if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
			*((pu4Byte)(val)) = (u4Byte)(data_MACDM_state_criteria_8814A_end - data_MACDM_state_criteria_8814A_start);
		}
#endif  //IS_RTL8814A_SERIES
#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_state_criteria_8197F_end - data_MACDM_state_criteria_8197F_start);   
        }
#endif //IS_RTL8197F_SERIES
        
#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            *((pu4Byte)(val)) = (u4Byte)(data_MACDM_state_criteria_8198F_end - data_MACDM_state_criteria_8198F_start);   
        }
#endif //IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_state_criteria_8822B_end - data_MACDM_state_criteria_8822B_start);   
        }
#endif //IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_state_criteria_8822C_end - data_MACDM_state_criteria_8822C_start);   
        }
#endif //IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_state_criteria_8812F_end - data_MACDM_state_criteria_8812F_start);   
        }
#endif //IS_RTL8812F_SERIES

#if IS_RTL8814B_SERIES
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            //*((pu4Byte)(val)) = (u4Byte)(data_MACDM_state_criteria_8814B_end - data_MACDM_state_criteria_8814B_start);  //yllin8814B 
        }
#endif //IS_RTL8814B_SERIES

#if IS_RTL8197G_SERIES
            if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
                *((pu4Byte)(val)) = (u4Byte)(data_MACDM_state_criteria_8197G_end - data_MACDM_state_criteria_8197G_start);   
            }
#endif //IS_RTL8197G_SERIES

	}
	break;

#endif //#if CFG_HAL_MACDM

#if CFG_HAL_HW_DETEC_POWER_STATE
	case HW_VAR_HW_PS_STATE0: {
#if IS_EXIST_RTL8192FE
		if (IS_HARDWARE_TYPE_8192FE(Adapter)) {
            *((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, 0x730);
        }
		#endif
		if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3_v4) {
			*((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, REG_SPWR0);
		}
	}
	break;

	case HW_VAR_HW_PS_STATE1: {
#if IS_EXIST_RTL8192FE
		if (IS_HARDWARE_TYPE_8192FE(Adapter)) {
                *((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, 0x734);
        }
		#endif
		if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3_v4) {
			*((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, REG_SPWR1);
		}
	}
	break;

	case HW_VAR_HW_PS_STATE2: {
#if IS_EXIST_RTL8192FE
		if (IS_HARDWARE_TYPE_8192FE(Adapter)) {
                *((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, 0x738);
        }
		#endif
		if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3_v4) {
			*((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, REG_SPWR2);
		}
	}
	break;

	case HW_VAR_HW_PS_STATE3: {
#if IS_EXIST_RTL8192FE
		if (IS_HARDWARE_TYPE_8192FE(Adapter)) {
                *((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, 0x73C);
        }
		#endif
		if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3_v4) {
			*((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, REG_SPWR3);
		}
	}
	break;
#endif //#if CFG_HAL_HW_DETEC_POWER_STATE        
#if CFG_HAL_AP_PS_OFFLOAD
        case HW_VAR_MACID_SLEEP0: {
            if ( IS_HARDWARE_TYPE_8814A(Adapter) || IS_HARDWARE_TYPE_8192FE(Adapter)) {
                *((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, 0x4D4);
            }
        }
        break;
    
        case HW_VAR_MACID_SLEEP1: {
            if ( IS_HARDWARE_TYPE_8814A(Adapter) || IS_HARDWARE_TYPE_8192FE(Adapter)) {
                *((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, 0x488);
            }
        }
        break;
    
        case HW_VAR_MACID_SLEEP2: {
            if ( IS_HARDWARE_TYPE_8814A(Adapter) || IS_HARDWARE_TYPE_8192FE(Adapter)) {
                *((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, 0x4D0);
            }
        }
        break;
    
        case HW_VAR_MACID_SLEEP3: {
            if ( IS_HARDWARE_TYPE_8814A(Adapter) || IS_HARDWARE_TYPE_8192FE(Adapter)) {
                *((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, 0x484);
            }
        }
        break;
    
#endif
	case HW_VAR_REG_CCK_CHECK: {
		*((pu1Byte)(val)) = platform_efio_read_1byte(Adapter, REG_CCK_CHECK);
	}
	break;
#if (IS_RTL8192E_SERIES | IS_RTL8881A_SERIES)
	case HW_VAR_HWSEQ_CTRL:{
		*((pu1Byte)(val)) = platform_efio_read_1byte(Adapter, REG_HWSEQ_CTRL);
	}
	break;
#endif //#if (IS_RTL8192E_SERIES | IS_RTL8881A_SERIES)
	case HW_VAR_REG_CR:
	{
		*((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, REG_CR);
	}
	break;
    case HW_VAR_BCN_HEAD:
    {
#if IS_RTL88XX_MAC_V1
        if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1)
            *((pu2Byte)(val)) = (u2Byte) (platform_efio_read_1byte(Adapter, REG_DWBCN0_CTRL+1));      
#endif
#if(IS_RTL88XX_MAC_V2_V3)
        if( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3)
            *((pu2Byte)(val)) = (u2Byte) (platform_efio_read_2byte(Adapter, REG_FIFOPAGE_CTRL_2) & 0x0fff);
#endif
#if(IS_RTL88XX_MAC_V4)
        if( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)
            *((pu2Byte)(val)) = (u2Byte) (platform_efio_read_2byte(Adapter, REG_BCN_CTRL_0) & 0x0fff);
#endif
    }
    break;
    case HW_VAR_RXPKT_NUM:
    {
#if IS_RTL88XX_MAC_V4
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)
            *((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, REG_RXDMA_CTRL);
#endif            
#if IS_RTL88XX_MAC_V1_V2_V3
        if( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3)
            *((pu4Byte)(val)) = platform_efio_read_4byte(Adapter, REG_RXPKT_NUM);
#endif
    }
    break;
#if CFG_HAL_SUPPORT_TXDESC_IE            
    case HW_VAR_TXDESC_TRANS_STATUS:
    {
        *((pu1Byte)(val)) = platform_efio_read_1byte(Adapter, REG_PKT_TRANS);
    }
    break;
#endif   
	default:
		RT_TRACE_F(COMP_IO, DBG_WARNING, ("Command ID(%d) not Supported\n", variable));
		break;
	}
}

#if (IS_RTL8881A_SERIES || IS_RTL8192E_SERIES || IS_RTL8192F_SERIES)
enum rt_status
InitLLT_Table88XX(
	IN  HAL_PADAPTER    Adapter
)
{
	u4Byte i, count = 0;
	u4Byte txpktbufSz, bufBd;

	if (TXPKTBUF_LLT_PAGECNT >= TXPKTBUF_TOTAL_PAGECNT) {
		RT_TRACE(COMP_INIT, DBG_SERIOUS, ("LLT init fail, size not match, error !!! \n") );
		return RT_STATUS_FAILURE;
	}

	if (TX_PAGE_CNT_RSV + TXPKTBUF_LLT_PAGECNT > TXPKTBUF_TOTAL_PAGECNT) {
		RT_TRACE(COMP_INIT, DBG_SERIOUS, ("LLT init fail, size not match, error !!! \n") );
		return RT_STATUS_FAILURE;
	}

	if ((&Adapter->pshare->_dmODM)->la_mode)
	{
		txpktbufSz = 0x21;

		HAL_RTL_W8(REG_RQPN_NPQ, 0x0);
		HAL_RTL_W8(REG_RQPN_NPQ + 2, 0x0);
		HAL_RTL_W32(REG_RQPN_CTRL_HLPQ, BIT31 | (0x20 << 16));
	}
	else
	{
	txpktbufSz  = TXPKTBUF_LLT_PAGECNT;
	bufBd       = TXPKTBUF_TOTAL_PAGECNT - 1;

	// Set reserved page for each queue
	/* normal queue init MUST be previous of RQPN enable */
	HAL_RTL_W8(REG_RQPN_NPQ, TX_PAGE_CNT_NPQ);
	HAL_RTL_W8(REG_RQPN_NPQ + 2, TX_PAGE_CNT_EPQ);
	HAL_RTL_W32(REG_RQPN_CTRL_HLPQ, BIT31 | (TX_PAGE_CNT_PUBQ << 16) | (TX_PAGE_CNT_LPQ << 8) | (TX_PAGE_CNT_HPQ));
	}

	HAL_RTL_W8(REG_BCNQ_BDNY, txpktbufSz);
	HAL_RTL_W8(REG_MGQ_BDNY, txpktbufSz);
	HAL_RTL_W8(REG_DWBCN0_CTRL + 1, txpktbufSz);

#if CFG_HAL_DUAL_BCN_BUF
	HAL_RTL_W8(REG_BCNQ1_BDNY, txpktbufSz + SECOND_BCN_PAGE_OFFSET);

#if CFG_HAL_SUPPORT_MBSSID
	HAL_RTL_W8(REG_DWBCN1_CTRL + 1, txpktbufSz + SECOND_BCN_PAGE_OFFSET);
#else
	HAL_RTL_W8(REG_DWBCN1_CTRL + 1, txpktbufSz);
#endif // CFG_HAL_SUPPORT_MBSSID
#else
	HAL_RTL_W8(REG_BCNQ1_BDNY, txpktbufSz);
	HAL_RTL_W8(REG_DWBCN1_CTRL + 1, txpktbufSz);
#endif

    #if 0 /* TX page file already fixed in HW. No need to set register. Including old IC 8881A,8192E*/
	// Set Tx/Rx page size (Tx must be 256(92E/8881A)  128(8814A) Bytes, Rx can be 64,128,256,512,1024 bytes)
	HAL_RTL_W8(REG_PBP, ((PBP_PSTX_SIZE / PBP_UNIT)&BIT_MASK_PSTX) << BIT_SHIFT_PSTX |
			   ((PBP_PSRX_SIZE / PBP_UNIT)&BIT_MASK_PSRX) << BIT_SHIFT_PSRX);
    #endif

	HAL_RTL_W32(REG_AUTO_LLT, HAL_RTL_R32(REG_AUTO_LLT) | BIT16);

	count = 0;
	do {
		if ((HAL_RTL_R32(REG_AUTO_LLT) & BIT16) == 0 ) {
			// Success
			break;
		}

		if (++count >= LLT_TABLE_INIT_POLLING_CNT) {
			RT_TRACE(COMP_INIT, DBG_WARNING, ("Auto LLT Polling failed !!!\n") );
			return RT_STATUS_FAILURE;
		}
	} while (1);

	return RT_STATUS_SUCCESS;
}
#endif //(IS_RTL8881A_SERIES || IS_RTL8192E_SERIES)

#if IS_RTL88XX_MAC_V2_V3

//2 LLT_INIT
#define _LLT_NO_ACTIVE				0x0
#define _LLT_WRITE_ACCESS			0x1
#define _LLT_READ_ACCESS			0x2

#define _LLT_INIT_DATA(x)			((x) & 0xFF)
#define _LLT_INIT_ADDR(x)			(((x) & 0xFF) << 8)
#define _LLT_OP(x)				(((x) & 0x3) << 30)
#define _LLT_OP_VALUE(x)			(((x) >> 30) & 0x3)

#define _LLT_INIT_DATA_8814A(x)			(((x) & 0xFFF)<<16)
#define _LLT_INIT_ADDR_8814A(x)			((x) & 0xFFF)

#define REG_LLT_INIT_8814A				0x01E0
#define REG_LLT_ADDR_8814A				0x01E4

#define POLLING_LLT_THRESHOLD				20

static enum rt_status
_LLTWrite_8814AE(
	IN  HAL_PADAPTER    Adapter,
	IN	u4Byte		address,
	IN	u4Byte		data
)
{
	enum rt_status	status = RT_STATUS_SUCCESS;
	s4Byte 		count = 0;
	u4Byte 		value = _LLT_INIT_DATA_8814A(data) | _LLT_OP(_LLT_WRITE_ACCESS);
	u2Byte 		valueaddr = (u2Byte)_LLT_INIT_ADDR_8814A(address) ;

	HAL_RTL_W16(REG_LLT_ADDR_8814A, valueaddr);
	HAL_RTL_W32(REG_LLT_INIT_8814A, value);

	//polling
	do {

		value = HAL_RTL_R32(REG_LLT_INIT_8814A);
		if (_LLT_NO_ACTIVE == _LLT_OP_VALUE(value)) {
			break;
		}

		if (count > POLLING_LLT_THRESHOLD) {
			RT_TRACE(COMP_INIT, DBG_SERIOUS, ("Failed to polling write LLT done at address %d!\n", address));
			status = RT_STATUS_FAILURE;
			break;
		}
	} while (++count);

	return status;

}



enum rt_status
InitLLT_Table88XX_V1(
	IN  HAL_PADAPTER    Adapter
)
{
	u4Byte          i, count = 0;
	u1Byte	        tmp1byte = 0, testcnt = 0;
	u4Byte          txpktbuf_bndy;
	u4Byte          agg_num,value32;
	enum rt_status		status;

#if (IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
    if ( IS_HARDWARE_TYPE_8814A(Adapter) || IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter)|| IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter) ) {
    	if (TXPKTBUF_LLT_PAGECNT_V1 >= TXPKTBUF_TOTAL_PAGECNT_V1) {
    		RT_TRACE(COMP_INIT, DBG_SERIOUS, ("LLT init fail, error 0 !!!, (%d, %d) \n",
    										  TXPKTBUF_LLT_PAGECNT_V1,
    										  TXPKTBUF_TOTAL_PAGECNT_V1) );
    		return RT_STATUS_FAILURE;
    	}

    	if (TX_PAGE_CNT_RSV_V1 + TXPKTBUF_LLT_PAGECNT_V1 > TXPKTBUF_TOTAL_PAGECNT_V1) {
    		RT_TRACE(COMP_INIT, DBG_SERIOUS, ("LLT init fail, error 1 !!!, (%d, %d, %d) \n",
    										  TX_PAGE_CNT_RSV_V1,
    										  TXPKTBUF_LLT_PAGECNT_V1,
    										  TXPKTBUF_TOTAL_PAGECNT_V1) );
    		return RT_STATUS_FAILURE;
    	}

   
	txpktbuf_bndy  = TXPKTBUF_LLT_PAGECNT_V1;

	//------------Tx FIFO page number setting------------------
	// Queue page number setting
	HAL_RTL_W16(REG_FIFOPAGE_INFO_1, TX_PAGE_CNT_HPQ_V1);
	HAL_RTL_W16(REG_FIFOPAGE_INFO_2, TX_PAGE_CNT_LPQ_V1);
	HAL_RTL_W16(REG_FIFOPAGE_INFO_3, TX_PAGE_CNT_NPQ_V1);
	HAL_RTL_W16(REG_FIFOPAGE_INFO_4, TX_PAGE_CNT_EPQ_V1);
	HAL_RTL_W16(REG_FIFOPAGE_INFO_5, TX_PAGE_CNT_PUBQ_V1);
    } 
#endif //#if IS_RTL8814A_SERIES

#if IS_RTL8197F_SERIES
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            if (TXPKTBUF_LLT_PAGECNT >= TXPKTBUF_TOTAL_PAGECNT) {
                RT_TRACE(COMP_INIT, DBG_SERIOUS, ("LLT init fail, error 0 !!!, (%d, %d) \n",
                                                  TXPKTBUF_LLT_PAGECNT,
                                                  TXPKTBUF_TOTAL_PAGECNT) );
                return RT_STATUS_FAILURE;
            }
    
            if (TX_PAGE_CNT_RSV + TXPKTBUF_LLT_PAGECNT > TXPKTBUF_TOTAL_PAGECNT) {
                RT_TRACE(COMP_INIT, DBG_SERIOUS, ("LLT init fail, error 1 !!!, (%d, %d, %d) \n",
                                                  TX_PAGE_CNT_RSV,
                                                  TXPKTBUF_LLT_PAGECNT,
                                                  TXPKTBUF_TOTAL_PAGECNT) );
                return RT_STATUS_FAILURE;
            }
    
       
        txpktbuf_bndy  = TXPKTBUF_LLT_PAGECNT;
    
        //------------Tx FIFO page number setting------------------
        // Queue page number setting
        HAL_RTL_W16(REG_FIFOPAGE_INFO_1, TX_PAGE_CNT_HPQ);  // 0x0e
        HAL_RTL_W16(REG_FIFOPAGE_INFO_2, TX_PAGE_CNT_LPQ);  // 0x20;  VI+BK
        HAL_RTL_W16(REG_FIFOPAGE_INFO_3, TX_PAGE_CNT_NPQ);  // 0x29;  VO+BE
        HAL_RTL_W16(REG_FIFOPAGE_INFO_4, TX_PAGE_CNT_EPQ);  // 0x04
        HAL_RTL_W16(REG_FIFOPAGE_INFO_5, TX_PAGE_CNT_PUBQ); // 0x9a

        /*
                // temp for 97F TP tuning
                HAL_RTL_W16(REG_FIFOPAGE_INFO_1, 0x3);
                HAL_RTL_W16(REG_FIFOPAGE_INFO_2, 0x1);
                HAL_RTL_W16(REG_FIFOPAGE_INFO_3, 0x1);
                HAL_RTL_W16(REG_FIFOPAGE_INFO_4, 0x1);
                HAL_RTL_W16(REG_FIFOPAGE_INFO_5, 0xEF);        
            */
        
        } 
#endif //#if IS_RTL8197F_SERIES

#if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            if (TXPKTBUF_LLT_PAGECNT_V2 >= TXPKTBUF_TOTAL_PAGECNT_V2) {
                RT_TRACE(COMP_INIT, DBG_SERIOUS, ("LLT init fail, error 0 !!!, (%d, %d) \n",
                                                  TXPKTBUF_LLT_PAGECNT_V2,
                                                  TXPKTBUF_TOTAL_PAGECNT_V2) );
                return RT_STATUS_FAILURE;
            }
    
            if (TX_PAGE_CNT_RSV_V2 + TXPKTBUF_LLT_PAGECNT_V2 > TXPKTBUF_TOTAL_PAGECNT_V2) {
                RT_TRACE(COMP_INIT, DBG_SERIOUS, ("LLT init fail, error 1 !!!, (%d, %d, %d) \n",
                                                  TX_PAGE_CNT_RSV_V2,
                                                  TXPKTBUF_LLT_PAGECNT_V2,
                                                  TXPKTBUF_TOTAL_PAGECNT_V2) );
                return RT_STATUS_FAILURE;
            }
    
        txpktbuf_bndy  = TXPKTBUF_LLT_PAGECNT_V2;
        //------------Tx FIFO page number setting------------------
        // Queue page number setting
       
        HAL_RTL_W16(REG_FIFOPAGE_INFO_1, TX_PAGE_CNT_HPQ_V2);  // 
        HAL_RTL_W16(REG_FIFOPAGE_INFO_2, TX_PAGE_CNT_LPQ_V2);  // VI
        HAL_RTL_W16(REG_FIFOPAGE_INFO_3, TX_PAGE_CNT_NPQ_V2);  // VO
        HAL_RTL_W16(REG_FIFOPAGE_INFO_4, TX_PAGE_CNT_EPQ_V2);  // 
        HAL_RTL_W16(REG_FIFOPAGE_INFO_5, TX_PAGE_CNT_PUBQ_V2); // 
        HAL_RTL_W16(REG_FIFOPAGE_INFO_6, TX_PAGE_CNT_EX1Q_V2); // BE
        HAL_RTL_W16(REG_FIFOPAGE_INFO_7, TX_PAGE_CNT_EX2Q_V2); // BK

		agg_num = (TX_PAGE_CNT_PUBQ_V2+TX_PAGE_CNT_EX1Q_V2)/USED_PAGE_NUM_PER_MMPDU/2;
		value32 = HAL_RTL_R32(REG_PROT_MODE_CTRL) | (agg_num << 16) | (agg_num << 24);
		HAL_RTL_W32(REG_PROT_MODE_CTRL,value32);

		

        }
#endif //#if IS_RTL8198F_SERIES
#if IS_RTL8197G_SERIES
        if (IS_HARDWARE_TYPE_8197G(Adapter)) {
            if (TXPKTBUF_LLT_PAGECNT_V3 >= TXPKTBUF_TOTAL_PAGECNT_V3) {
                RT_TRACE(COMP_INIT, DBG_SERIOUS, ("LLT init fail, error 0 !!!, (%d, %d) \n",
                                                  TXPKTBUF_LLT_PAGECNT_V3,
                                                  TXPKTBUF_TOTAL_PAGECNT_V3) );
                return RT_STATUS_FAILURE;
            }
    
            if (TX_PAGE_CNT_RSV_V3 + TXPKTBUF_LLT_PAGECNT_V3 > TXPKTBUF_TOTAL_PAGECNT_V3) {
                RT_TRACE(COMP_INIT, DBG_SERIOUS, ("LLT init fail, error 1 !!!, (%d, %d, %d) \n",
                                                  TX_PAGE_CNT_RSV_V3,
                                                  TXPKTBUF_LLT_PAGECNT_V3,
                                                  TXPKTBUF_TOTAL_PAGECNT_V3) );
                return RT_STATUS_FAILURE;
            }
    
        txpktbuf_bndy  = TXPKTBUF_LLT_PAGECNT_V3;
        //------------Tx FIFO page number setting------------------
        // Queue page number setting
       
        HAL_RTL_W16(REG_FIFOPAGE_INFO_1, TX_PAGE_CNT_HPQ_V3);  // 0x20
        HAL_RTL_W16(REG_FIFOPAGE_INFO_2, TX_PAGE_CNT_LPQ_V3);  // 0x20;  VI
        HAL_RTL_W16(REG_FIFOPAGE_INFO_3, TX_PAGE_CNT_NPQ_V3);  // 0x20;  VO
        HAL_RTL_W16(REG_FIFOPAGE_INFO_4, TX_PAGE_CNT_EPQ_V3);  // 0x10
        HAL_RTL_W16(REG_FIFOPAGE_INFO_5, TX_PAGE_CNT_PUBQ_V3); 
        HAL_RTL_W16(REG_FIFOPAGE_INFO_6, TX_PAGE_CNT_EX1Q_V3); // 0x20   BE
        HAL_RTL_W16(REG_FIFOPAGE_INFO_7, TX_PAGE_CNT_EX2Q_V3); // 0x20   BK

        }
#endif //#if  IS_RTL8197G_SERIES


	//Page number enable
	//Write 1 to set RQPN bit[79:0] value to page numbers for initialization
	HAL_RTL_W8(REG_RQPN_CTRL_2 + 3, 0x80);

	//Set page boundary and header
	// For 8814A, set 0x422[4] = 1 before setting REG_BCNQ_BDNY_V1 (0x424), suggested by Austin Lo
	HAL_RTL_W32(REG_FWHW_TXQ_CTRL, HAL_RTL_R32(REG_FWHW_TXQ_CTRL) | BIT_EN_WR_FREE_TAIL);
	HAL_RTL_W16(REG_BCNQ_BDNY_V1, txpktbuf_bndy);
	// For 8814A, set 0x422[4] = 0 after setting REG_BCNQ_BDNY_V1 (0x424), suggested by Austin Lo
	HAL_RTL_W32(REG_FWHW_TXQ_CTRL, HAL_RTL_R32(REG_FWHW_TXQ_CTRL) & ~BIT_EN_WR_FREE_TAIL);

	HAL_RTL_W16(REG_BCNQ1_BDNY_V1, txpktbuf_bndy + SECOND_BCN_PAGE_OFFSET);
	HAL_RTL_W16(REG_MGQ_BDNY_V1, txpktbuf_bndy);

	//Set The head page of packet of Bcnq
	HAL_RTL_W16(REG_FIFOPAGE_CTRL_2, txpktbuf_bndy);
	//The head page of packet of Bcnq1
	HAL_RTL_W16(REG_FIFOPAGE_CTRL_2 + 2, txpktbuf_bndy + SECOND_BCN_PAGE_OFFSET);

	HAL_RTL_W16(REG_WMAC_LBK_BUF_HD_V1, txpktbuf_bndy);

#if 0 /* TX page file already fixed in HW. No need to set register. Including old IC 8814A,8197F*/
	// Set Tx/Rx page size (Tx must be 128(8814A) Bytes, Rx can be 64,128,256,512,1024 bytes)
	HAL_RTL_W8(REG_PBP, ((PBP_PSTX_SIZE_V1 / PBP_UNIT)&BIT_MASK_PSTX) << BIT_SHIFT_PSTX |
			   ((PBP_PSRX_SIZE_V1 / PBP_UNIT)&BIT_MASK_PSRX) << BIT_SHIFT_PSRX);
#endif // if 0

	// Step 3: Auto-init LLT table ( Set REG:0x208[BIT0] )
	//Write 1 to enable HW init LLT, driver need polling to 0 meaning init success

	tmp1byte = HAL_RTL_R8(REG_AUTO_LLT_V1);
	HAL_RTL_W8(REG_AUTO_LLT_V1, tmp1byte | BIT0);
	while (tmp1byte & BIT0) {
		tmp1byte = HAL_RTL_R8(REG_AUTO_LLT_V1);
		delay_us(10);
		testcnt++;
		if (testcnt > 10)
			break;
	}

	return RT_STATUS_SUCCESS;
}

#endif //#if IS_RTL88XX_MAC_V2_V3_V4

enum rt_status
InitPON88XX(
	IN  HAL_PADAPTER Adapter
)
{   
    u4Byte result = 0;
#if IS_SUPPORT_MACHAL_API
    if(IS_HARDWARE_MACHAL_SUPPORT(Adapter))
    {
        result = GET_MACHALAPI_INTERFACE(Adapter)->halmac_pre_init_system_cfg(Adapter->pHalmac_adapter);
        if ( HALMAC_RET_SUCCESS != result) {
    		panic_printk("Pre_init_system Failed, error = %x \n",result);
            return RT_STATUS_FAILURE;
    	} else {
    	    // continue
    	}

        result = GET_MACHALAPI_INTERFACE(Adapter)->halmac_mac_power_switch(Adapter->pHalmac_adapter,HALMAC_MAC_POWER_ON);

        if ( HALMAC_RET_SUCCESS != result) {
    		panic_printk("InitPON Failed, error = %x \n",result);
            return RT_STATUS_FAILURE;
    	} else {
       		panic_printk("InitPON OK!!!\n");
    	    return RT_STATUS_SUCCESS;
    	}  
    }
#endif //IS_SUPPORT_MACHAL_API    
}

#ifdef AP_SWPS_OFFLOAD_WITH_AMPDU_VERI

VOID
InitAPSWPS_AMPDU_Verification(
    IN  HAL_PADAPTER Adapter

)
{
    u2Byte SWPS_RPT_init_size;
    u4Byte init_i =0;
    
    u8 entry_size,bitmap_size;
    u8 one_reocrd_size;
    u8 mac_macid_num;
    u32 report_addr_offset;
#if IS_RTL8197G_SERIES
    if(IS_HARDWARE_TYPE_8197G(Adapter)){
        bitmap_size = SWPSRPT_BITMAP_SIZE_V1*2;
        entry_size = SWPSRPT_ENTRY_SIZE_V1;
        one_reocrd_size = SWPSRPT_ONE_RECORD_SIZE_V1;
        mac_macid_num = 64;
        report_addr_offset = 0x680;
    }
#endif

        SWPS_RPT_init_size = mac_macid_num*entry_size+bitmap_size+sizeof(RX_DESC_88XX); //16 is one report size,64 is MACID bitmap, 24 is RXDESC size
            
        HAL_RTL_W16(REG_PKTBUF_DBG_CTRL, report_addr_offset);
        
        for(init_i=0;init_i<(SWPS_RPT_init_size/4);init_i++)
            HAL_RTL_W32(0x8000+(init_i*4), 0x0);

        //set SWPS_RPT bit in RXDESC
        HAL_RTL_W8(0x8010, BIT(5));

        HAL_RTL_W8(REG_SWPS_CTRL+1,0x5 & BIT_MASK_SWPS_RPT_LENGTH); //tmp fix to 2 macid for test
       
        #if IS_RTL8197G_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
            HAL_RTL_W16(REG_SWPS_CTRL+2,0x100);
        #endif
        HAL_RTL_W32(REG_SWPS_TIME_TH,BIT_SWPS_PSTIME_TH(0x0) | BIT_SWPS_TIME_TH(0xc8)); //just random set for test, 0x0, 0xc8
            
        //enable SWPS
        HAL_RTL_W8(REG_SWPS_CTRL, HAL_RTL_R8(REG_SWPS_CTRL) | BIT0);
        


}

#endif
#ifdef AP_SWPS_OFFLOAD

VOID
UpdateAPSWPSReportSize88XX(
    IN  HAL_PADAPTER Adapter,
    IN  unsigned int max_macid
)
{
    u8 set = 0;

    if ((Adapter->SWPS_RPT_macid_num!=4) && (max_macid < 4)){
        Adapter->SWPS_RPT_macid_num = 4;
        set = 1;
    }
    else{
        if( (Adapter->SWPS_RPT_macid_num - max_macid) < 2){
            Adapter->SWPS_RPT_macid_num += 4; 
            set = 1;
        }
    }

    if(set==1)
        HAL_RTL_W8(REG_SWPS_CTRL+1,(Adapter->SWPS_RPT_macid_num+1) & BIT_MASK_SWPS_RPT_LENGTH);

}

VOID
UpdateAPSWPSReportTH88XX(
    IN  HAL_PADAPTER Adapter,
    IN  u2Byte       PKT_TH,
    IN  u2Byte       PSTIME_TH,
    IN  u2Byte       TIME_TH
)
{

    //set gen report threshold
    #if IS_RTL8198F_SERIES
    if(IS_HARDWARE_TYPE_8198F(Adapter))
        HAL_RTL_W16(REG_SWPS_PKT_TH,PKT_TH); //just random set for test, 0x100
    #endif
    #if IS_RTL8197G_SERIES
    if(IS_HARDWARE_TYPE_8198F(Adapter))
        HAL_RTL_W16(REG_SWPS_CTRL+2,PKT_TH);
    #endif
    HAL_RTL_W32(REG_SWPS_TIME_TH,BIT_SWPS_PSTIME_TH(PSTIME_TH) | BIT_SWPS_TIME_TH(TIME_TH)); //just random set for test, 0x0, 0xc8
}
VOID
InitAPSWPS88XX(
    IN  HAL_PADAPTER Adapter

)
{

        u2Byte SWPS_RPT_init_size;
        u4Byte init_i =0;

        u8 entry_size,bitmap_size;
        u8 one_reocrd_size;
        u8 mac_macid_num;
        u32 report_addr_offset;

#if IS_RTL8198F_SERIES || IS_RTL8814B_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8814B(Adapter)){
            bitmap_size = SWPSRPT_BITMAP_SIZE;
            entry_size = SWPSRPT_ENTRY_SIZE;
            one_reocrd_size = SWPSRPT_ONE_RECORD_SIZE;
            mac_macid_num = 128;
            report_addr_offset = 0x800;
        }
#endif
#if IS_RTL8197G_SERIES
        if(IS_HARDWARE_TYPE_8197G(Adapter)){
            bitmap_size = SWPSRPT_BITMAP_SIZE_V1*2;
            entry_size = SWPSRPT_ENTRY_SIZE_V1;
            one_reocrd_size = SWPSRPT_ONE_RECORD_SIZE_V1;
            mac_macid_num = 64;
            report_addr_offset = 0x680;
        }
#endif

        SWPS_RPT_init_size = mac_macid_num*entry_size+bitmap_size+sizeof(RX_DESC_88XX); //16 is one report size,64 is MACID bitmap, 24 is RXDESC size
            
        HAL_RTL_W16(REG_PKTBUF_DBG_CTRL, report_addr_offset);
        
        for(init_i=0;init_i<(SWPS_RPT_init_size/4);init_i++)
            HAL_RTL_W32(0x8000+(init_i*4), 0x0);

        //set SWPS_RPT bit in RXDESC
        HAL_RTL_W8(0x8010, BIT(5));

        UpdateAPSWPSReportSize88XX(Adapter,0);
       
        UpdateAPSWPSReportTH88XX(Adapter,0x100,0x0,0xc8); //init set report size to 5 macid, tmp
            
        //enable SWPS
        HAL_RTL_W8(REG_SWPS_CTRL, HAL_RTL_R8(REG_SWPS_CTRL) | BIT0);
        
        for(init_i=0;init_i<4;init_i++)
            Adapter->stop_recycle_cnt[init_i] = 0;
        
        Adapter->not_prepare_done = 0;
        Adapter->prepare_inWakeup = 0;
    

}

#endif

#if (IS_RTL88XX_MAC_V3)
// 98F/97G
enum rt_status
InitMAC88XX_V1(
	IN  HAL_PADAPTER Adapter
)
{
	u4Byte  errorFlag = 0;
	u4Byte  MaxAggreNum;
	enum rt_status   status;
	u1Byte result;
	pu1Byte pFWStart;
	u4Byte imageSZ;
	RT_OP_MODE      OP_Mode;
	MACCONFIG_PARA  MacCfgPara;
	u2Byte          beaconPeriod;    
	unsigned int opmode;
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)Adapter;

	RT_TRACE(COMP_INIT, DBG_LOUD, ("===>%s\n", __FUNCTION__));
#if 0
	if(IS_HARDWARE_MACHAL_SUPPORT(Adapter))
	{
		result = GET_MACHALAPI_INTERFACE(Adapter)->halmac_init_system_cfg(Adapter->pHalmac_adapter);
		if ( HALMAC_RET_SUCCESS != result) {
			panic_printk("InitMAC Page0 Failed ,error status = %x \n",result);
			return RT_STATUS_FAILURE;
		} else {
			printk("InitMAC Page0 \n");
		}   
    
		GetHwReg88XX(Adapter, HW_VAR_FWFILE_SIZE, (pu1Byte)&imageSZ);
		GetHwReg88XX(Adapter, HW_VAR_FWFILE_START, (pu1Byte)&pFWStart);
#ifdef SUPPORT_TXDESC_IE
		if(IS_SUPPORT_TXDESC_IE(Adapter)){ //use IE to download FW
			HAL_RTL_W32(REG_CR, HAL_RTL_R32(REG_CR) | BIT_SHCUT_EN);
			HAL_RTL_W32(REG_PKT_TRANS, (HAL_RTL_R32(REG_PKT_TRANS) & ~BIT_TXDESC_TRANS_EN) | BIT_PKT_TRANS_ERRINT_EN); 
		}
#endif
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
		if (IS_HARDWARE_TYPE_8814B(Adapter)) {
			//HAL_RTL_W8(0x2, (HAL_RTL_R8(0x2)|BIT(0)|BIT(1))); /* FIXME: 2MAC */
			/* output UART to GPIO7 */
			HAL_RTL_W32(0x40, (HAL_RTL_R32(0x40) & ~(BIT(20)|BIT(21))));
			HAL_RTL_W32(0x64, (HAL_RTL_R32(0x64) & ~(BIT(26))));
			result = FirmwareDownload8814B(Adapter);
			HAL_RTL_W16(REG_TCUNIT_BASE, 1); /* FIXME: should be set in FW ? */
			HAL_RTL_W32(REG_CPUMGQ_FREERUN_TIMER_CTRL,
					(HAL_RTL_R32(REG_CPUMGQ_FREERUN_TIMER_CTRL)|BIT_EN_FREECNT_V1));
		} else
#endif
		{
			result = GET_MACHALAPI_INTERFACE(Adapter)->halmac_download_firmware(Adapter->pHalmac_adapter,pFWStart,imageSZ);
		}
		if ( HALMAC_RET_SUCCESS != result) {
			panic_printk("Init Download FW Failed ,error status = %x \n",result);
		} else {
			_GET_HAL_DATA(Adapter)->bFWReady = _TRUE;
			printk("Init Download FW OK \n");

#ifdef AP_PS_Offlaod
			Adapter->APPS_phase = 0; //0:phase1, 1:phase2
			GET_HAL_INTERFACE(Adapter)->SetAPPSOffloadHandler(Adapter,1, Adapter->APPS_phase, 8, 1);
#endif
		}

#if IS_RTL8814B_SERIES /* FIXME: disabled for 8814b development */
	if (!IS_HARDWARE_TYPE_8814B(Adapter)) {
#endif
#if CFG_HAL_MAC_ENABLE_LA_MODE
		result = GET_MACHALAPI_INTERFACE(Adapter)->halmac_cfg_la_mode(Adapter->pHalmac_adapter,HALMAC_LA_MODE_PARTIAL);
#endif 
            
#if CFG_HAL_MAC_LOOPBACK
		result = GET_MACHALAPI_INTERFACE(Adapter)->halmac_init_mac_cfg(Adapter->pHalmac_adapter,HALMAC_TRX_MODE_LOOPBACK);
#else
		result = GET_MACHALAPI_INTERFACE(Adapter)->halmac_init_mac_cfg(Adapter->pHalmac_adapter,HALMAC_TRX_MODE_NORMAL);
#endif 
		if(HALMAC_RET_SUCCESS != result)
		{
			panic_printk("halmac_init_mac_cfg Failed,error status = %x \n",result);
			return RT_STATUS_FAILURE;
		} else {
			printk("halmac_init_mac_cfg OK\n");
		}   

		result = GET_MACHALAPI_INTERFACE(Adapter)->halmac_cfg_rx_aggregation(Adapter->pHalmac_adapter,HALMAC_RX_AGG_MODE_NONE);
		if(HALMAC_RET_SUCCESS != result) 
		{
			panic_printk("halmac_cfg_rx_aggregation Failed,error status = %x \n",result);
			return RT_STATUS_FAILURE;
		} else {
			printk("halmac_cfg_rx_aggregation OK\n");
		}   

		if (HALMAC_RET_SUCCESS != GET_MACHALAPI_INTERFACE(Adapter)->halmac_cfg_mac_addr(Adapter->pHalmac_adapter,0,(pu1Byte)Adapter->dev->dev_addr)||
				HALMAC_RET_SUCCESS != GET_MACHALAPI_INTERFACE(Adapter)->halmac_cfg_operation_mode(Adapter->pHalmac_adapter,HALMAC_WIRELESS_MODE_AC)) 
		{
			panic_printk("halmac_init_mac_cfg Failed\n");
			return RT_STATUS_FAILURE;            
		} else {
			printk("halmac_init_mac_cfg OK\n");
		}  
#if IS_RTL8814B_SERIES /* FIXME */
	}
#endif
	}
#endif //IS_SUPPORT_MACHAL_API    

	// Release MAC IO register reset
#if IS_EXIST_PCI || IS_EXIST_EMBEDDED
	if (Adapter->hci_type == RTL_HCI_PCIE) {
		HAL_RTL_W32(REG_CR, HAL_RTL_R32(REG_CR) | BIT_MACRXEN | BIT_MACTXEN | BIT_SCHEDULE_EN | BIT_PROTOCOL_EN
					| BIT_RXDMA_EN | BIT_TXDMA_EN | BIT_HCI_RXDMA_EN | BIT_HCI_TXDMA_EN);
	}
#endif
#if IS_EXIST_SDIO
	if (Adapter->hci_type == RTL_HCI_SDIO) {
		// Reset TX/RX DMA before LLT init to avoid TXDMA error "LLT_NULL_PG"
		HAL_RTL_W8(REG_CR, 0x00);
		// for SDIO - Set CR bit10 to enable 32k calibration. Suggested by SD1 Gimmy. Added by tynli. 2011.08.31.
		HAL_RTL_W16(REG_CR, HAL_RTL_R16(REG_CR) | BIT_HCI_TXDMA_EN | BIT_HCI_RXDMA_EN | BIT_TXDMA_EN
					| BIT_RXDMA_EN | BIT_PROTOCOL_EN | BIT_SCHEDULE_EN | BIT_32K_CAL_TMR_EN);

		// Enable CMD53 R/W Operation
		GET_HAL_INTF_DATA(Adapter)->bMacPwrCtrlOn = TRUE;
	}
#endif
#if IS_EXIST_USB /* CONFIG_RTL_TRIBAND_SUPPORT */
	if (Adapter->hci_type == RTL_HCI_USB) {
		// Reset TX/RX DMA before LLT init to avoid TXDMA error "LLT_NULL_PG"
		HAL_RTL_W8(REG_CR, 0x00);
		// for SDIO - Set CR bit10 to enable 32k calibration. Suggested by SD1 Gimmy. Added by tynli. 2011.08.31.
		HAL_RTL_W16(REG_CR, HAL_RTL_R16(REG_CR) | BIT_HCI_TXDMA_EN | BIT_HCI_RXDMA_EN | BIT_TXDMA_EN
					| BIT_RXDMA_EN | BIT_PROTOCOL_EN | BIT_SCHEDULE_EN | BIT_32K_CAL_TMR_EN);

		// Enable CMD53 R/W Operation
		GET_HAL_INTF_DATA(Adapter)->bMacPwrCtrlOn = TRUE;
	}
#endif

	// RX 6 packets or  (3*128=384us) timeout trigger RX interrupt
	//HAL_RTL_W32(REG_INT_MIG, 0x160000);

	// Clear interrupt
	HAL_RTL_W32(REG_HISR0, 0xFFFFFFFF);
	HAL_RTL_W32(REG_HISR1, 0xFFFFFFFF);

	opmode = Adapter->pmib->dot11OperationEntry.opmode;

	if (opmode & WIFI_AP_STATE) {
		printk("AP-mode enabled...\n");	
		//DEBUG_INFO("AP-mode enabled...\n");
#if defined(CONFIG_RTK_MESH)	//Mesh Mode but mesh not enable
		if (Adapter->pmib->dot11WdsInfo.wdsPure || Adapter->pmib->dot1180211sInfo.meshSilence )
#else
		if (Adapter->pmib->dot11WdsInfo.wdsPure)
#endif
		{
			printk("RT_OP_MODE_NO_LINK...\n");	
			OP_Mode = RT_OP_MODE_NO_LINK;
		} else {
			printk("RT_OP_MODE_AP...\n");	
			OP_Mode = RT_OP_MODE_AP;
		}
	} else if (opmode & WIFI_STATION_STATE) {
		printk("Station-mode enabled...\n");	
		//DEBUG_INFO("Station-mode enabled...\n");
		OP_Mode = RT_OP_MODE_INFRASTRUCTURE;
	} else if (opmode & WIFI_ADHOC_STATE) {
		printk("Adhoc-mode enabled...\n");	
		//DEBUG_INFO("Adhoc-mode enabled...\n");
		OP_Mode = RT_OP_MODE_IBSS;
	} else {
		printk("Operation mode error!\n");
		return 2;
	}
	GET_HAL_INTERFACE(Adapter)->SetHwRegHandler(Adapter, HW_VAR_MEDIA_STATUS, (pu1Byte)&OP_Mode);

	MacCfgPara.AckTO                = Adapter->pmib->miscEntry.ack_timeout;
	MacCfgPara.vap_enable           = GET_ROOT(Adapter)->pmib->miscEntry.vap_enable;
	MacCfgPara.OP_Mode              = OP_Mode;
	MacCfgPara.dot11DTIMPeriod      = Adapter->pmib->dot11StationConfigEntry.dot11DTIMPeriod;
	MacCfgPara.WdsPure      		= Adapter->pmib->dot11WdsInfo.wdsPure;
	beaconPeriod                    = Adapter->pmib->dot11StationConfigEntry.dot11BeaconPeriod;

	GET_HAL_INTERFACE(Adapter)->SetHwRegHandler(Adapter, HW_VAR_BEACON_INTERVAL, (pu1Byte)&beaconPeriod);
	GET_HAL_INTERFACE(Adapter)->SetHwRegHandler(Adapter, HW_VAR_MAC_CONFIG, (pu1Byte)&MacCfgPara); //8814Byllin
    
	// disable BT_enable
	HAL_RTL_W8(REG_GPIO_MUXCFG, 0);
    
#if IS_EXIST_RTL8198FEM
    if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
    		//Enable 98F pinmux setting due to RTL8814B -eFEM 
				if(Adapter->pmib->dot11RFEntry.rfe_type==1 || Adapter->pmib->dot11RFEntry.rfe_type==3){
					HAL_RTL_W32(REG_GPIO_MUXCFG, HAL_RTL_R32(REG_GPIO_MUXCFG)| BIT_PIN_USECASE_V1(7));   //0x40[24:27]=7	 	
					HAL_RTL_W32(REG_PAD_CTRL1, HAL_RTL_R32(REG_PAD_CTRL1)|BIT_PAPE_WLBT_SEL | BIT_LNAON_WLBT_SEL );  		//0x66[13]=1				
				}
        // TODO : check 8198F need Todo?
        // Disable BT coextense 
        HAL_RTL_W8(REG_DIS_TXREQ_CLR,0x80);
        HAL_RTL_W32(REG_SYS_CLK_CTRL,HAL_RTL_R32(REG_SYS_CLK_CTRL)|BIT14);        
        HAL_RTL_W32(REG_SYS_SDIO_CTRL,HAL_RTL_R32(0x70)|BIT_LTE_MUX_CTRL_PATH);

    	HAL_RTL_W32(0x7D8, 0x30810041);
    	HAL_RTL_W32(0x7D4, 0x50802080);
    	HAL_RTL_W32(0x1700, 0xC00F0038);
    }
#endif //#if IS_EXIST_RTL8198FEM

#if IS_EXIST_RTL8197GEM
    if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {

        // Disable BT coextense 
        HAL_RTL_W8(REG_DIS_TXREQ_CLR,0x80);
        HAL_RTL_W32(REG_SYS_CLK_CTRL,HAL_RTL_R32(REG_SYS_CLK_CTRL)|BIT14);        
        HAL_RTL_W32(REG_SYS_SDIO_CTRL,HAL_RTL_R32(0x70)|BIT_LTE_MUX_CTRL_PATH);

    	HAL_RTL_W32(0x7D8, 0x30810041);
    	HAL_RTL_W32(0x7D4, 0x50802080);
    	HAL_RTL_W32(0x1700, 0xC00F0038);
     

    }
#endif //#if IS_EXIST_RTL8197GEM


#if 0
    if (IS_HARDWARE_TYPE_8814B(Adapter)) { //yllin8814B
        //HAL_RTL_W32(REG_RXFF_BNDY,(HAL_RTL_R32(REG_RXFF_BNDY) & 0xFFFC0000) | BIT_RXFF0_BNDY_V2(MAC_RXFF_SIZE_V2)); //set in halmac
        HAL_RTL_W8(0x1538 , 0x78);
        HAL_RTL_W8(0x638 , 0x78);
        HAL_RTL_W8(0x639 , 0x19);
        HAL_RTL_W8(0x640 , 0x19);  
        HAL_RTL_W32(0x1700, 0xC00F0038);
    }
#endif // (IS_RTL8814B_SERIES )

	//Clear RegDumpErr
	GET_HAL_INTERFACE(Adapter)->SetHwRegHandler(Adapter, HW_VAR_DRV_DBG, (pu1Byte)&errorFlag);

	RT_TRACE(COMP_INIT, DBG_LOUD, ("%s===>\n", __FUNCTION__) );

	return RT_STATUS_SUCCESS;
}
#endif

#if (IS_RTL88XX_MAC_V1_V2_V4)
// 8881, 8192E, 8814A, 8197F, 8822B, 8814B used
enum rt_status
InitMAC88XX(
	IN  HAL_PADAPTER Adapter
)
{
	u4Byte  errorFlag = 0;
	u4Byte  MaxAggreNum;
	enum rt_status   status;
    u1Byte result;
    pu1Byte pFWStart;
    pu1Byte fwBufPtr;
    u4Byte imageSZ,i;
	HALMAC_RXAGG_CFG rx_cfg;
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)Adapter;
	RT_TRACE(COMP_INIT, DBG_LOUD, ("===>%s\n", __FUNCTION__));

    //MACFM_software_init(Adapter);

#if IS_SUPPORT_MACHAL_API
    if(IS_HARDWARE_MACHAL_SUPPORT(Adapter))
    {

#ifdef ENABLE_PCIE_MULTI_TAG
        // Do nothing
#else
        #if IS_RTL88XX_MAC_V4
        if(_GET_HAL_DATA(priv)->MacVersion.is_MAC_v4){
            HAL_RTL_W32(REG_HCI_MIX_CFG, HAL_RTL_R32(REG_HCI_MIX_CFG) & ~(BIT21 | BIT22) );
        }
        #endif
        #if IS_RTL88XX_MAC_V1_V2
        if(_GET_HAL_DATA(priv)->MacVersion.is_MAC_v1_v2){
        //Disable Multi-tag
        printk("REG_HCI_MIX_CFG = %x \n", HAL_RTL_R8(REG_HCI_MIX_CFG));
        HAL_RTL_W8(REG_HCI_MIX_CFG, HAL_RTL_R8(REG_HCI_MIX_CFG) & ~BIT6 );
		}
		#endif
#endif

#if IS_EXIST_SDIO
	if (Adapter->hci_type == RTL_HCI_SDIO) {
		// Reset TX/RX DMA before LLT init to avoid TXDMA error "LLT_NULL_PG"
		HAL_RTL_W8(REG_CR, 0x00);
		HAL_RTL_W32(0x210, 0x0);
	}
#endif
        result = GET_MACHALAPI_INTERFACE(Adapter)->halmac_init_system_cfg(Adapter->pHalmac_adapter);
        if ( HALMAC_RET_SUCCESS != result) {
			panic_printk("InitMAC Page0 Failed ,error status = %x \n",result);
            return RT_STATUS_FAILURE;
		} else {
			printk("InitMAC Page0 \n");
		}   

#ifdef SUPPORT_TXDESC_IE
		if(IS_SUPPORT_TXDESC_IE(Adapter)){ //use IE to download FW
			HAL_RTL_W32(REG_CR, HAL_RTL_R32(REG_CR) | BIT_SHCUT_EN);
			HAL_RTL_W32(REG_PKT_TRANS, (HAL_RTL_R32(REG_PKT_TRANS) & ~BIT_TXDESC_TRANS_EN) | BIT_PKT_TRANS_ERRINT_EN); 
		}
#endif

        if(priv->pshare->rf_ft_var.load_fw){
        GetHwReg88XX(Adapter, HW_VAR_FWFILE_SIZE, (pu1Byte)&imageSZ);
        GetHwReg88XX(Adapter, HW_VAR_FWFILE_START, (pu1Byte)&pFWStart);        
		SetHwReg88XX(Adapter, HW_VAR_ENABLE_BEACON_DMA, NULL);

		_GET_HAL_DATA(Adapter)->PFWHeader = (PVOID)pFWStart;		
		ReadMIPSFwHdr88XX(Adapter);

#if defined(CONFIG_PE_ENABLE)
		if(IS_HARDWARE_TYPE_8814BE(priv)){
			download_pe_fw();
			while(!check_pe_ready(priv))
			{
				mdelay(100);
			}

			//tx_init_cmd();
			tx_start_process_cmd();
		}
#endif
		
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
		if (IS_HARDWARE_TYPE_8814B(Adapter)) {
			//HAL_RTL_W8(0x2, (HAL_RTL_R8(0x2)|BIT(0)|BIT(1))); /* FIXME: 2MAC */
			/* output UART to GPIO7 */
			HAL_RTL_W32(0x40, (HAL_RTL_R32(0x40) & ~(BIT(20)|BIT(21))));
			HAL_RTL_W32(0x64, (HAL_RTL_R32(0x64) & ~(BIT(26))));
			HAL_RTL_W16(0x440, 0x150); /* FIXME: should be set in MAC_reg or HALMAC? */	
			result = FirmwareDownload8814B(Adapter);
			//HAL_RTL_W8(0x639, 0xff); /* FIXME: only for FPGA */
			HAL_RTL_W16(REG_TCUNIT_BASE, 1); /* FIXME: should be set in FW ? */
			HAL_RTL_W32(REG_CPUMGQ_FREERUN_TIMER_CTRL,
					(HAL_RTL_R32(REG_CPUMGQ_FREERUN_TIMER_CTRL)|BIT_EN_FREECNT_V1));
		} else
#endif
		{				
#if defined(CONFIG_RTL8192CD_MODULE) || defined(NOT_RTK_BSP)
        	    fwBufPtr = HALMalloc(Adapter,imageSZ );
		    if (NULL == fwBufPtr) {
		        RT_TRACE_F(COMP_INIT, DBG_WARNING, ("Allocate FW buffer error ! \n"));
		        return RT_STATUS_FAILURE;
		    }
		    HAL_memcpy(fwBufPtr, pFWStart, imageSZ);
                    result = GET_MACHALAPI_INTERFACE(Adapter)->halmac_download_firmware(Adapter->pHalmac_adapter,fwBufPtr,imageSZ);

	            HAL_free(fwBufPtr);
#else
        result = GET_MACHALAPI_INTERFACE(Adapter)->halmac_download_firmware(Adapter->pHalmac_adapter,pFWStart,imageSZ);
#endif 
		}

#if IS_EXIST_EMBEDDED || IS_EXIST_PCI
			SetBeaconDownload88XX(Adapter, HW_VAR_BEACON_DISABLE_DOWNLOAD);
			panic_printk(">>SetBeaconDownload88XX\n");
#endif

        if ( HALMAC_RET_SUCCESS != result) {
          panic_printk("Init Download FW Failed ,error status = %x \n",result);
        } else {
          _GET_HAL_DATA(Adapter)->bFWReady = _TRUE;
          _GET_HAL_DATA(Adapter)->H2CBufPtr88XX = 0;
          printk("Init Download FW OK \n");                          

#ifdef CONFIG_RTL_OFFLOAD_DRIVER
		if (IS_HARDWARE_TYPE_8814B(Adapter))
			; // No need to update fw_version
		else
#endif
		{
			HALMAC_FW_VERSION	fw_version;
			GET_MACHALAPI_INTERFACE(Adapter)->halmac_get_fw_version(Adapter->pHalmac_adapter, &fw_version);
			Adapter->pshare->fw_version = fw_version.version;
			Adapter->pshare->fw_sub_version = fw_version.sub_version;
		}

		#ifdef AP_PS_Offlaod
        Adapter->APPS_phase = 0; //0:phase1, 1:phase2
    GET_HAL_INTERFACE(Adapter)->SetAPPSOffloadHandler(Adapter,1, Adapter->APPS_phase, 8, 1);
        #endif
        }     
       	}

	#if 1//CFG_HAL_MAC_ENABLE_LA_MODE
	if ((&(Adapter->pshare->_dmODM))->la_mode)
		result = GET_MACHALAPI_INTERFACE(Adapter)->halmac_cfg_la_mode(Adapter->pHalmac_adapter,HALMAC_LA_MODE_PARTIAL);
	else
		result = GET_MACHALAPI_INTERFACE(Adapter)->halmac_cfg_la_mode(Adapter->pHalmac_adapter,HALMAC_LA_MODE_DISABLE);
	#endif 
        
    #if CFG_HAL_MAC_LOOPBACK
        result = GET_MACHALAPI_INTERFACE(Adapter)->halmac_init_mac_cfg(Adapter->pHalmac_adapter,HALMAC_TRX_MODE_LOOPBACK);
    #else
        result = GET_MACHALAPI_INTERFACE(Adapter)->halmac_init_mac_cfg(Adapter->pHalmac_adapter,HALMAC_TRX_MODE_NORMAL);
    #endif

        if(HALMAC_RET_SUCCESS != result)
        {
    		panic_printk("halmac_init_mac_cfg Failed,error status = %x \n",result);
            return RT_STATUS_FAILURE;
    	} else {
    		printk("halmac_init_mac_cfg OK\n");                                
    	}   

		rx_cfg.mode = HALMAC_RX_AGG_MODE_NONE;
		rx_cfg.threshold.drv_define = 0;
        result = GET_MACHALAPI_INTERFACE(Adapter)->halmac_cfg_rx_aggregation(Adapter->pHalmac_adapter, &rx_cfg);

        if(HALMAC_RET_SUCCESS != result) 
        {
            panic_printk("halmac_cfg_rx_aggregation Failed,error status = %x \n",result);
            return RT_STATUS_FAILURE;
        } else {
            printk("halmac_cfg_rx_aggregation OK\n");                                
        }   

        if (HALMAC_RET_SUCCESS != GET_MACHALAPI_INTERFACE(Adapter)->halmac_cfg_mac_addr(Adapter->pHalmac_adapter,0,(pu1Byte)Adapter->dev->dev_addr)||
            HALMAC_RET_SUCCESS != GET_MACHALAPI_INTERFACE(Adapter)->halmac_cfg_operation_mode(Adapter->pHalmac_adapter,HALMAC_WIRELESS_MODE_AC)) 
        {
            panic_printk("halmac_init_mac_cfg Failed\n");
            return RT_STATUS_FAILURE;            
        } else {
            printk("halmac_init_mac_cfg OK\n");                                
        }  


GET_MACHALAPI_INTERFACE(Adapter)->halmac_cfg_drv_info(Adapter->pHalmac_adapter,HALMAC_DRV_INFO_PHY_STATUS);

#if IS_EXIST_RTL8822BE || IS_EXIST_RTL8822BS || IS_EXIST_RTL8821CE || IS_EXIST_RTL8821CS || IS_EXIST_RTL8814BE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8822CS|| IS_EXIST_RTL8812FE || IS_EXIST_RTL8812FS /* 8822B temp Etic remove*/
        //HAL_RTL_W32(0x100, HAL_RTL_R32(0x100)|BIT24|BIT25|BIT27);
        ////reset H2C write addr 5. Write reg 0x254[2]=1'b1
        HAL_RTL_W32(0x254, 0xD); 
        // enable latch to H2C_Head address 6. Write reg 0x20D[7]=1'b1
        HAL_RTL_W8(0x20D, 0x80); 
        //HAL_RTL_W8(0x11c, 0x5eff); 
        
        HAL_RTL_W32(0x70, HAL_RTL_R32(0x70)|BIT26);
        HAL_RTL_W32(0x1080, HAL_RTL_R32(0x1080)|BIT16);
        // Disable BT coextense 
        HAL_RTL_W8(REG_DIS_TXREQ_CLR,0x80);

#if IS_RTL88XX_MAC_V1_V2
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2){
            // RX 6 packets or (3*128=384us) timeout trigger RX interrupt
#if IS_EXIST_RTL8812FE
			if (IS_HARDWARE_TYPE_8812F(Adapter))
				HAL_RTL_W32(REG_INT_MIG, 0x30001);
			else
#endif
            HAL_RTL_W32(REG_INT_MIG, 0x160000);
        }
#endif
#if IS_RTL88XX_MAC_V4
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
					if(priv->pshare->rf_ft_var.mp_specific)
						HAL_RTL_W32(REG_INT_MIG_V1, 0x0);
					else
						// RX 6 packets or (3*128=384us) timeout trigger RX interrupt
		        HAL_RTL_W32(REG_INT_MIG_V1, 0x160000);
        }
#endif

#if (CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC) 
	if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC(Adapter)) {
#if IS_RTL88XX_MAC_V2_V3
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3){
            // Enable HW TX shortcut
            HAL_RTL_W32(REG_CR, HAL_RTL_R32(REG_CR) | BIT_SHCUT_EN);
            HAL_RTL_W32(REG_SHCUT_SETTING, HAL_RTL_R32(REG_SHCUT_SETTING) & ~BIT8);
        }
#endif
    }
#endif        
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV_LLC
        if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(Adapter)) {
#if IS_RTL88XX_MAC_V2
            if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2){
                // Enable auto append LLC to header
                HAL_RTL_W32(REG_SHCUT_SETTING, HAL_RTL_R32(REG_SHCUT_SETTING) | (BIT9|BIT10|BIT11));
            }
#endif
#if IS_RTL88XX_MAC_V4
            if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
                // Enable auto append LLC to header
                HAL_RTL_W32(REG_PKT_TRANS, HAL_RTL_R32(REG_PKT_TRANS) | (BIT9|BIT10|BIT11));
                //enable Hdr conversion
                HAL_RTL_W16(REG_PKT_TRANS, HAL_RTL_R16(REG_PKT_TRANS) | BIT_MAC_HDR_CONVERT_EN);
            }
#endif
            // clear HW TX shortCut SRAM buffer
            for(i=0;i<64;i++)
            {
                ClearHWTXShortcutBufHandler88XX(Adapter,i);
            }
        }
#endif // CFG_HAL_HW_TX_SHORTCUT_HDR_CONV_LLC
#endif //CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
	
         
		// Disable BT
		HAL_RTL_W8(0x6a, HAL_RTL_R8(0x6a) & ~BIT2);
#endif 


		// Set retry limit
		if (HAL_VAR_RETRY_LIMIT_LONG_MIB)
			HAL_VAR_RETRY_LIMIT = HAL_VAR_RETRY_LIMIT_LONG_MIB & 0xff;
		else {
#ifdef CLIENT_MODE
			if (HAL_OPMODE & WIFI_STATION_STATE)
				HAL_VAR_RETRY_LIMIT = RETRY_LIMIT_LONG_CLIENT;
			else
#endif
			{
				if (HAL_VAR_PHYBANDSELECT == PHY_BAND_2G)
					HAL_VAR_RETRY_LIMIT = RETRY_LIMIT_LONG_AP_2G;
				else
					HAL_VAR_RETRY_LIMIT = RETRY_LIMIT_LONG_AP;
			}
		}
		
		if (HAL_VAR_RETRY_LIMIT_SHORT_MIB)
			HAL_VAR_RETRY_LIMIT |= ((HAL_VAR_RETRY_LIMIT_SHORT_MIB & 0xff) << 8);
		else {
#ifdef CLIENT_MODE
		if (HAL_OPMODE & WIFI_STATION_STATE)
			HAL_VAR_RETRY_LIMIT |= (RETRY_LIMIT_SHORT_CLIENT << 8);
		else
#endif
			{
				if (HAL_VAR_PHYBANDSELECT == PHY_BAND_2G)
					HAL_VAR_RETRY_LIMIT |= (RETRY_LIMIT_SHORT_AP_2G << 8);
				else
					HAL_VAR_RETRY_LIMIT |= (RETRY_LIMIT_SHORT_AP << 8);
			}
		}
		HAL_RTL_W16(REG_RETRY_LIMIT, HAL_VAR_RETRY_LIMIT);
#if IS_EXIST_SDIO
	if (Adapter->hci_type == RTL_HCI_SDIO) {
		GET_HAL_INTF_DATA(Adapter)->bMacPwrCtrlOn = TRUE;
	}
#endif
#if IS_EXIST_SDIO
		if (Adapter->hci_type == RTL_HCI_SDIO) {
            _initSdioAggregationSetting(Adapter);
		}
#endif
#if  (IS_RTL8814B_SERIES )
			if (IS_HARDWARE_TYPE_8814B(Adapter)) { //yllin8814B
				//HAL_RTL_W32(REG_RXFF_BNDY,(HAL_RTL_R32(REG_RXFF_BNDY) & 0xFFFC0000) | BIT_RXFF0_BNDY_V2(MAC_RXFF_SIZE_V2)); //set in halmac
				HAL_RTL_W8(0x1538 , 0x78);
				HAL_RTL_W8(0x638 , 0x78);
				HAL_RTL_W8(0x639 , 0x19);
				HAL_RTL_W8(0x640 , 0x19);  
				HAL_RTL_W32(0x1700, 0xC00F0038);
				
#ifdef CONFIG_8814_AP_MAC_VERI
				
					//test use, init txff to 0xffffffff for fist 0x400 bytes	
					int indx=0;
				
					HAL_RTL_W32(0x140, 0x780);
				
					for(indx=0;indx<100;indx++)
					{
						HAL_RTL_W32(0x8000+indx*4, 0xffffffff);
				
					}
				
					//test use, init txreport to 0xffffffff 
					HAL_RTL_W32(0x140, 0x662);
					for(indx=0;indx<1024;indx++)
					{
						HAL_RTL_W32(0x8000+indx*4, 0xffffffff);
					}
		
#endif
		
#if CFG_HAL_HW_FILL_MACID
				if( IS_SUPPORT_HW_FILL_MACID(Adapter))
				{
#if IS_RTL88XX_MAC_V2_V3
					// set CRC5 buffer addr
					HAL_RTL_W32(REG_SEARCH_MACID, HAL_RTL_R32(REG_SEARCH_MACID) | BIT_INFO_INDEX_OFFSET(0x300) ); // set CRC5 buffer addr
#endif //#if IS_RTL88XX_MAC_V2_V3
#if CFG_HAL_HW_DETEC_POWER_STATE
			
				if( IS_SUPPORT_HW_DETEC_POWER_STATE(Adapter))
				{
					HAL_RTL_W32(0x7D4, HAL_RTL_R32(0x7D4) | BIT15| BIT17); // set ps detect enable
					HAL_RTL_W32(REG_POWSEQ, 0x1); // set ps seq debug port
				}
#endif //#if CFG_HAL_HW_DETEC_POWER_STATE
				}
#endif 
#if defined(AP_SWPS_OFFLOAD)
				if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter))
					InitAPSWPS88XX(Adapter);
#endif
#if CFG_HAL_TX_AMSDU & IS_RTL88XX_MAC_V2
				if ( IS_SUPPORT_TX_AMSDU(Adapter) && _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2 ) {
					// Enable 0x0B0[31] timer interrupt
					//HAL_RTL_W8(REG_MISC_CTRL, HAL_RTL_R8(REG_MISC_CTRL)|BIT3);
			
					// Enable 0x0B0[30] timer interrupt, for port0.  0x5B1[5:3] = 3b'000
					//HAL_RTL_W8(REG_EARLY_128US, HAL_RTL_R8(REG_EARLY_128US) & 0xC7);
			
					// Enable 0x0B0[29] timer interrupt, for port0. 0x5B4[2:0] = 3b'000
					HAL_RTL_W8(REG_TIMER0_SRC_SEL, HAL_RTL_R8(REG_TIMER0_SRC_SEL) & 0xF8);
				}
#endif //#if CFG_HAL_TX_AMSDU
			
#if CFG_HAL_SUPPORT_ENHANCED_EDCA
				u32 edcaPara;
				if ( IS_SUPPORT_ENHANCED_EDCA(Adapter) ) {
#if IS_RTL88XX_MAC_V4
				if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
					edcaPara = (0x20 | BIT_RANDOM_VALUE_SHIFT(0x4) | BIT_ENABLE_NEW_EDCA);
					HAL_RTL_W32(REG_NEW_EDCA_CTRL_V1, edcaPara);  
				}
#endif
				}
#endif 
		
#ifdef DRVMAC_LB 
#if IS_RTL88XX_MAC_V4
				if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)		
					HAL_RTL_W32(REG_ACH_CTRL,HAL_RTL_R32(REG_ACH_CTRL) & ~BIT_STOP_P0BCNQ); //its set in init_beacon if not in loopback mode
#endif        
#endif
		
				RT_OP_MODE		OP_Mode;
				MACCONFIG_PARA	MacCfgPara;
				u2Byte			beaconPeriod;
				unsigned int opmode;
				
				opmode = Adapter->pmib->dot11OperationEntry.opmode;
		
				if (opmode & WIFI_AP_STATE) {
					DEBUG_INFO("AP-mode enabled...\n");
#if defined(CONFIG_RTK_MESH)	//Mesh Mode but mesh not enable
					if (priv->pmib->dot11WdsInfo.wdsPure || priv->pmib->dot1180211sInfo.meshSilence )
#else
					if (priv->pmib->dot11WdsInfo.wdsPure)
#endif
					{
						OP_Mode = RT_OP_MODE_NO_LINK;
					} else {
						OP_Mode = RT_OP_MODE_AP;
					}
				} else if (opmode & WIFI_STATION_STATE) {
					DEBUG_INFO("Station-mode enabled...\n");
					OP_Mode = RT_OP_MODE_INFRASTRUCTURE;
				} else if (opmode & WIFI_ADHOC_STATE) {
					DEBUG_INFO("Adhoc-mode enabled...\n");
					OP_Mode = RT_OP_MODE_IBSS;
				} else {
					printk("Operation mode error!\n");
		
					return 2;
				}
		
				GET_HAL_INTERFACE(priv)->SetHwRegHandler(priv, HW_VAR_MEDIA_STATUS, (pu1Byte)&OP_Mode);
		
				MacCfgPara.AckTO				= priv->pmib->miscEntry.ack_timeout;
				MacCfgPara.vap_enable			= GET_ROOT(priv)->pmib->miscEntry.vap_enable;
				MacCfgPara.OP_Mode				= OP_Mode;
				MacCfgPara.dot11DTIMPeriod		= priv->pmib->dot11StationConfigEntry.dot11DTIMPeriod;		
				MacCfgPara.WdsPure				= priv->pmib->dot11WdsInfo.wdsPure;
				beaconPeriod					= priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod;
		
				GET_HAL_INTERFACE(priv)->SetHwRegHandler(priv, HW_VAR_BEACON_INTERVAL, (pu1Byte)&beaconPeriod);
				GET_HAL_INTERFACE(priv)->SetHwRegHandler(priv, HW_VAR_MAC_CONFIG, (pu1Byte)&MacCfgPara);
		
		
				u8	MulticastAddr[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
				GET_HAL_INTERFACE(priv)->SetHwRegHandler(priv, HW_VAR_MULTICAST_REG, (pu1Byte)MulticastAddr);
		
				GET_HAL_INTERFACE(priv)->SetHwRegHandler(priv, HW_VAR_BSSID, (pu1Byte)priv->pmib->dot11OperationEntry.hwaddr);
		
				if (opmode & WIFI_AP_STATE) {
					GET_HAL_INTERFACE(priv)->InitIMRHandler(priv, RT_OP_MODE_AP);
				} else if (opmode & WIFI_STATION_STATE) {
					GET_HAL_INTERFACE(priv)->InitIMRHandler(priv, RT_OP_MODE_INFRASTRUCTURE);
				} else if (opmode & WIFI_ADHOC_STATE) {
					GET_HAL_INTERFACE(priv)->InitIMRHandler(priv, RT_OP_MODE_IBSS);
				}
		
#if IS_EXIST_PCI || IS_EXIST_EMBEDDED
				GET_HAL_INTERFACE(priv)->AddInterruptMaskHandler(priv, HAL_INT_TYPE_PSTIMEOUT2);
		
#if defined(SW_TX_QUEUE) || defined(RTK_ATM)
				GET_HAL_INTERFACE(priv)->AddInterruptMaskHandler(priv, HAL_INT_TYPE_GTIMER4);
#endif
#endif
				// TODO: Filen, no need to sync !?
				priv->pshare->InterruptMask = _GET_HAL_DATA(priv)->IntMask[0];
				priv->pshare->InterruptMaskExt = _GET_HAL_DATA(priv)->IntMask[1];

				u8 enable = 1;
				GET_MACHAL_API(Adapter)->halmac_set_hw_value(Adapter->pHalmac_adapter,HALMAC_HW_EN_BB_RF, &enable);
				}
#endif // (IS_RTL8814B_SERIES )

#if IS_RTL8812F_SERIES
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
#ifdef HIGH_POWER_EXT_PA                
            if(priv->pshare->rf_ft_var.use_ext_pa){        
                    result = GET_MACHAL_API(Adapter)->halmac_pinmux_set_func(Adapter->pHalmac_adapter,HALMAC_GPIO_FUNC_S0_PAPE);
                    result = GET_MACHAL_API(Adapter)->halmac_pinmux_set_func(Adapter->pHalmac_adapter,HALMAC_GPIO_FUNC_S0_TRSW);
                    result = GET_MACHAL_API(Adapter)->halmac_pinmux_set_func(Adapter->pHalmac_adapter,HALMAC_GPIO_FUNC_S0_TRSWB);
                    result = GET_MACHAL_API(Adapter)->halmac_pinmux_set_func(Adapter->pHalmac_adapter,HALMAC_GPIO_FUNC_S1_PAPE);
                    result = GET_MACHAL_API(Adapter)->halmac_pinmux_set_func(Adapter->pHalmac_adapter,HALMAC_GPIO_FUNC_S1_TRSW);
                    result = GET_MACHAL_API(Adapter)->halmac_pinmux_set_func(Adapter->pHalmac_adapter,HALMAC_GPIO_FUNC_S1_TRSWB);
            }
#endif              
        }
#endif 		

        return RT_STATUS_SUCCESS;
    }
#endif //IS_SUPPORT_MACHAL_API    

	//Clear RegDumpErr
	GET_HAL_INTERFACE(Adapter)->SetHwRegHandler(Adapter, HW_VAR_DRV_DBG, (pu1Byte)&errorFlag);

	// Release MAC IO register reset
#if IS_EXIST_PCI || IS_EXIST_EMBEDDED
	if (Adapter->hci_type == RTL_HCI_PCIE) {
		HAL_RTL_W32(REG_CR, HAL_RTL_R32(REG_CR) | BIT_MACRXEN | BIT_MACTXEN | BIT_SCHEDULE_EN | BIT_PROTOCOL_EN
					| BIT_RXDMA_EN | BIT_TXDMA_EN | BIT_HCI_RXDMA_EN | BIT_HCI_TXDMA_EN);
	}
#endif
#if IS_EXIST_SDIO
	if (Adapter->hci_type == RTL_HCI_SDIO) {
		// Reset TX/RX DMA before LLT init to avoid TXDMA error "LLT_NULL_PG"
		HAL_RTL_W8(REG_CR, 0x00);
		// for SDIO - Set CR bit10 to enable 32k calibration. Suggested by SD1 Gimmy. Added by tynli. 2011.08.31.
		HAL_RTL_W16(REG_CR, HAL_RTL_R16(REG_CR) | BIT_HCI_TXDMA_EN | BIT_HCI_RXDMA_EN | BIT_TXDMA_EN
					| BIT_RXDMA_EN | BIT_PROTOCOL_EN | BIT_SCHEDULE_EN | BIT_32K_CAL_TMR_EN);

		// Enable CMD53 R/W Operation
		GET_HAL_INTF_DATA(Adapter)->bMacPwrCtrlOn = TRUE;
	}
#endif
#if IS_EXIST_USB//#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
	if (Adapter->hci_type == RTL_HCI_USB) {	
		// Reset TX/RX DMA before LLT init to avoid TXDMA error "LLT_NULL_PG"
		HAL_RTL_W8(REG_CR, 0x00);
		// for SDIO - Set CR bit10 to enable 32k calibration. Suggested by SD1 Gimmy. Added by tynli. 2011.08.31.
		HAL_RTL_W16(REG_CR, HAL_RTL_R16(REG_CR) | BIT_HCI_TXDMA_EN | BIT_HCI_RXDMA_EN | BIT_TXDMA_EN
					| BIT_RXDMA_EN | BIT_PROTOCOL_EN | BIT_SCHEDULE_EN | BIT_32K_CAL_TMR_EN);

		// Enable CMD53 R/W Operation
		GET_HAL_INTF_DATA(Adapter)->bMacPwrCtrlOn = TRUE;
	}
#endif

#if IS_RTL88XX_MAC_V2
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2){
#if (CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC ) 
	if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC(Adapter)) {
		// Enable HW TX shortcut
		HAL_RTL_W32(REG_CR, HAL_RTL_R32(REG_CR) | BIT_SHCUT_EN);
        if (IS_HAL_TEST_CHIP(Adapter)==FALSE) {
            HAL_RTL_W32(REG_SHCUT_SETTING, HAL_RTL_R32(REG_SHCUT_SETTING) & ~BIT8);
        } else {
#if IS_EXIST_RTL8814AE || IS_EXIST_RTL8814AU
    		HAL_RTL_W32(REG_TXRPT_START_OFFSET, HAL_RTL_R32(REG_TXRPT_START_OFFSET) & ~BIT_SHCUT_BYPASS);
#endif
        }
	}
#endif        
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV_LLC
        if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(Adapter)) {
            // Enable auto append LLC to header
            HAL_RTL_W32(REG_SHCUT_SETTING, HAL_RTL_R32(REG_SHCUT_SETTING) | (BIT9|BIT10|BIT11));

            // clear HW TX shortCut SRAM buffer
            for(i=0;i<64;i++)
            {
                ClearHWTXShortcutBufHandler88XX(Adapter,i);
            }
        }
#endif // CFG_HAL_HW_TX_SHORTCUT_HDR_CONV_LLC
#endif //CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
    }
#endif // IS_RTL88XX_MAC_V2

	//System init
	status = GET_HAL_INTERFACE(Adapter)->InitLLT_TableHandler(Adapter);


	if ( RT_STATUS_SUCCESS != status ) {
		RT_TRACE(COMP_INIT, DBG_SERIOUS, ("LLT_table_init Failed \n") );
		return RT_STATUS_FAILURE;
	} else {
		RT_TRACE(COMP_INIT, DBG_TRACE, ("LLT_table_init OK \n") );
	}

#if CFG_HAL_MACDM
	//calculate aggregation parameter & TXOP
#if IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE
	if ( IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8192FE(Adapter)) {
		MaxAggreNum = CalMaxAggreNum(PBP_PSTX_SIZE,
									 TX_PAGE_CNT_MIN_DEDQ_PUBQ,
									 _FALSE,     // TODO: LOAD mib
									 SIZE_TXDESC_88XX
									);

		_GET_HAL_DATA(Adapter)->MACDM_MAX_AggreNum = MaxAggreNum;
	}
#endif  //#if IS_EXIST_RTL8192EE

#if IS_EXIST_RTL8881AEM
	if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
		MaxAggreNum = CalMaxAggreNum(PBP_PSTX_SIZE,
									 TX_PAGE_CNT_MIN_DEDQ_PUBQ,
									 _FALSE,     // TODO: LOAD mib
									 SIZE_TXDESC_88XX
									);

		_GET_HAL_DATA(Adapter)->MACDM_MAX_AggreNum = MaxAggreNum;
	}
#endif //#if IS_EXIST_RTL8881AEM

#if IS_EXIST_RTL8814AE
	if ( IS_HARDWARE_TYPE_8814AE(Adapter) ) {
		MaxAggreNum = CalMaxAggreNum(PBP_PSTX_SIZE_V1,
									 TX_PAGE_CNT_MIN_DEDQ_PUBQ_V1,
									 _FALSE,     // TODO: LOAD mib
									 SIZE_TXDESC_88XX
									);

		_GET_HAL_DATA(Adapter)->MACDM_MAX_AggreNum = MaxAggreNum;
	}
#endif  //#if IS_EXIST_RTL8814AE

#if IS_EXIST_RTL8197FEM
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            MaxAggreNum = CalMaxAggreNum(PBP_PSTX_SIZE,
                                         TX_PAGE_CNT_MIN_DEDQ_PUBQ_V1,
                                         _FALSE,     // TODO: LOAD mib
                                         SIZE_TXDESC_88XX
                                        );
    
            _GET_HAL_DATA(Adapter)->MACDM_MAX_AggreNum = MaxAggreNum;
        }
#endif  //#if IS_EXIST_RTL8197FEM

#if IS_EXIST_RTL8822BE || IS_EXIST_RTL8822BS
        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            MaxAggreNum = CalMaxAggreNum(PBP_PSTX_SIZE_V1,
                                         TX_PAGE_CNT_MIN_DEDQ_PUBQ_V1,
                                         _FALSE,     // TODO: LOAD mib
                                         SIZE_TXDESC_88XX_V1
                                        );
    
            _GET_HAL_DATA(Adapter)->MACDM_MAX_AggreNum = MaxAggreNum;
        }
#endif  //#if IS_EXIST_RTL8822BE

#if IS_EXIST_RTL8822CE || IS_EXIST_RTL8822CS
        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            MaxAggreNum = CalMaxAggreNum(PBP_PSTX_SIZE_V1,
                                         TX_PAGE_CNT_MIN_DEDQ_PUBQ_V1,
                                         _FALSE,     // TODO: LOAD mib
                                         SIZE_TXDESC_88XX_V1
                                        );
    
            _GET_HAL_DATA(Adapter)->MACDM_MAX_AggreNum = MaxAggreNum;
        }
#endif  //#if IS_EXIST_RTL8822CE

#if IS_EXIST_RTL8812FE || IS_EXIST_RTL8812FS
        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            MaxAggreNum = CalMaxAggreNum(PBP_PSTX_SIZE_V1,
                                         TX_PAGE_CNT_MIN_DEDQ_PUBQ_V1,
                                         _FALSE,     // TODO: LOAD mib
                                         SIZE_TXDESC_88XX_V1
                                        );
    
            _GET_HAL_DATA(Adapter)->MACDM_MAX_AggreNum = MaxAggreNum;
        }
#endif  //#if IS_EXIST_RTL8812FE


#if IS_EXIST_RTL8821CE || IS_EXIST_RTL8821CS
			if ( IS_HARDWARE_TYPE_8821C(Adapter) ) {
				MaxAggreNum = CalMaxAggreNum(PBP_PSTX_SIZE_V1,
											 TX_PAGE_CNT_MIN_DEDQ_PUBQ_V1,
											 _FALSE,	 // TODO: LOAD mib
											 SIZE_TXDESC_88XX
											);
		
				_GET_HAL_DATA(Adapter)->MACDM_MAX_AggreNum = MaxAggreNum;
			}
#endif  //#if IS_EXIST_RTL8821CE


	DecisionAggrePara(Adapter, MACDM_AGGRE_STATE_TXOP, MaxAggreNum);
#endif  //#if CFG_HAL_MACDM


	// Clear interrupt
	HAL_RTL_W32(REG_HISR0, 0xFFFFFFFF);
	HAL_RTL_W32(REG_HISR1, 0xFFFFFFFF);
            
#if IS_RTL8192F_SERIES
	if (IS_HARDWARE_TYPE_8192F(Adapter)) {
		// RX 6 packets or (2*128=256us) timeout trigger RX interrupt
		HAL_RTL_W32(REG_INT_MIG, 0x160000);
#ifdef ENABLE_PCIE_MULTI_TAG
		// Do nothing
#else
		HAL_RTL_W8(0x3fc, HAL_RTL_R8(0x3fc) & ~BIT6);
#endif
	}
#endif

#if IS_RTL8814A_SERIES 
	if (IS_HARDWARE_TYPE_8814AE(Adapter)) {
		HAL_RTL_W32(REG_HISR2, 0xFFFFFFFF);
		HAL_RTL_W32(REG_HISR3, 0xFFFFFFFF);

#if defined(NOT_RTK_BSP) || defined(CONFIG_RTL8672)
		// For 8814 rx throughput, avoid TCP ACK can't send caused by too many rx interrupt 
		HAL_RTL_W32(REG_INT_MIG, 0x30300000);
#endif
#ifdef ENABLE_PCIE_MULTI_TAG
		// Do nothing
#else
		//Disable Multi-tag
		printk("REG_HCI_MIX_CFG = %x \n", HAL_RTL_R8(REG_HCI_MIX_CFG));
		HAL_RTL_W8(REG_HCI_MIX_CFG, HAL_RTL_R8(REG_HCI_MIX_CFG) & ~BIT6);
#endif
		HAL_RTL_W16(REG_SYS_FUNC_EN, HAL_RTL_R16(REG_SYS_FUNC_EN) & ~(BIT0 | BIT1));
		RT_TRACE(COMP_INIT, DBG_TRACE, ("REG_SYS_FUNC_EN = %x \n", HAL_RTL_R16(REG_SYS_FUNC_EN)));

		// temp add by Eric for 8814 RF b path power on
		HAL_RTL_W8(0x20, 0x7);    // PathB RF Power On
		HAL_RTL_W8(0x21, 0x7);    // PathC RF Power On
		HAL_RTL_W8(0x76, 0x7);    // PathD RF Power On

		HAL_RTL_W8(REG_STATE_MON + 3, 0x9);

        // Disable pre tx cmd
        HAL_RTL_W8(REG_SW_AMPDU_BURST_MODE_CTRL, HAL_RTL_R8(REG_SW_AMPDU_BURST_MODE_CTRL) & ~BIT6);        
	}
#endif //#if IS_RTL8814A_SERIES

	// RXFF
#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8192F_SERIES)
	if (IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8192F(Adapter)) {
		HAL_RTL_W32(REG_TRXFF_BNDY, (HAL_RTL_R32(REG_TRXFF_BNDY) & 0x0000FFFF) | BIT_RXFF0_BNDY(MAC_RXFF_SIZE));
	}
#endif //(IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8197F_SERIES)

#if (IS_RTL8197F_SERIES)
    if (IS_HARDWARE_TYPE_8197F(Adapter)) {
        HAL_RTL_W32(REG_RXFF_BNDY,(HAL_RTL_R32(REG_RXFF_BNDY) & 0xFFFC0000) | BIT_RXFF0_BNDY_V2(MAC_RXFF_SIZE));
	}
#endif //(IS_RTL8197F_SERIES )

#if IS_RTL8814A_SERIES
	if (IS_HARDWARE_TYPE_8814A(Adapter)) {
        HAL_RTL_W32(REG_RXFF_BNDY,(HAL_RTL_R32(REG_RXFF_BNDY) & 0xFFFC0000) | BIT_RXFF0_BNDY_V2(MAC_RXFF_SIZE_V1));
	}
#endif // IS_RTL8814A_SERIES

	// TRX DMA Queue Mapping
#if IS_EXIST_PCI || IS_EXIST_EMBEDDED
	if (Adapter->hci_type == RTL_HCI_PCIE) {
#if IS_RTL8814A_SERIES
		if (IS_HARDWARE_TYPE_8814A(Adapter))
		{
			HAL_RTL_W32(REG_TXDMA_PQ_MAP, (TRX_DMA_QUEUE_MAP_PARA_14 | BIT_RXSHFT_EN | BIT_RXDMA_ARBBW_EN));
		}
#endif //IS_RTL8814A_SERIES

#if (IS_RTL8192E_SERIES)
		if (IS_HARDWARE_TYPE_8192E(Adapter))
		{
			HAL_RTL_W32(REG_TXDMA_PQ_MAP, (TRX_DMA_QUEUE_MAP_PARA | BIT_RXDMA_ARBBW_EN));
		}
#endif

#if (IS_RTL8192F_SERIES)
		if (IS_HARDWARE_TYPE_8192F(Adapter))
		{
			HAL_RTL_W32(REG_TXDMA_PQ_MAP, (TRX_DMA_QUEUE_MAP_PARA_V1 | BIT_RXDMA_ARBBW_EN));
		}
#endif

#if (IS_RTL8881A_SERIES || IS_RTL8822B_SERIES || IS_RTL8197F_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
    if( IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8822B(Adapter) ||IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)) {
			HAL_RTL_W32(REG_TXDMA_PQ_MAP, (TRX_DMA_QUEUE_MAP_PARA | BIT_RXSHFT_EN | BIT_RXDMA_ARBBW_EN));
		}
#endif //#if (IS_RTL8881A_SERIES || IS_RTL8822B_SERIES || IS_RTL8197F_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)	
	}
#endif
#if IS_EXIST_USB || IS_EXIST_SDIO
	if (Adapter->hci_type == RTL_HCI_USB || Adapter->hci_type == RTL_HCI_SDIO) {
		HAL_RTL_W32(REG_TXDMA_PQ_MAP, (HAL_RTL_R32(REG_TXDMA_PQ_MAP) & 0x7) | TRX_DMA_QUEUE_MAP_PARA);
	}
#endif //IS_EXIST_PCI || IS_EXIST_EMBEDDED

	// Set Network type: ap mode
	HAL_RTL_W32(REG_CR, HAL_RTL_R32(REG_CR) | ((MSR_AP & BIT_MASK_NETYPE0) << BIT_SHIFT_NETYPE0));

	// Set SLOT time
	HAL_RTL_W8(REG_SLOT, 0x09);

	// Set RCR register

#if IS_RTL88XX_MAC_V1
	if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1) {        
        HAL_RTL_W32(REG_RCR, BIT_APP_FCS | BIT_APP_MIC | BIT_APP_ICV | BIT_APP_PHYSTS | BIT_HTC_LOC_CTRL
			| BIT_AMF | BIT_ADF | BIT_ACRC32 | BIT_AB | BIT_AM | BIT_APM);
    }
#endif  //IS_RTL88XX_MAC_V1        

#if (IS_RTL88XX_MAC_V2)
    if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2) {        
        	HAL_RTL_W32(REG_RCR, BIT_APP_FCS | BIT_APP_MIC | BIT_APP_ICV | BIT_APP_PHYSTS | BIT_VHT_DACK | BIT_HTC_LOC_CTRL
			| BIT_AICV | BIT_ACRC32 | BIT_AB | BIT_AM | BIT_APM);
    }
#endif  //IS_RTL88XX_MAC_V2     

#if (IS_RTL8881A_SERIES || IS_RTL8192E_SERIES || IS_RTL8814A_SERIES || IS_RTL8192F_SERIES)
	if (IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8814A(Adapter) || IS_HARDWARE_TYPE_8192F(Adapter)) {
		// Set Driver info size
		HAL_RTL_W8(REG_RX_DRVINFO_SZ, BIT_DRVINFO_SZ(4));
	}
#endif     

#if (IS_RTL8197F_SERIES)
	if (IS_HARDWARE_TYPE_8197F(Adapter)) {
    	// Set Driver info size
    	HAL_RTL_W8(REG_RX_DRVINFO_SZ, (HAL_RTL_R8(REG_RX_DRVINFO_SZ) & 0xF0 ) | BIT_DRVINFO_SZ_V1(4));
        // Enable RX shift to make payload(IP header) is 4-byte alignment
        HAL_RTL_W8(REG_RX_DRVINFO_SZ, HAL_RTL_R8(REG_RX_DRVINFO_SZ) | BIT_APP_MH_SHIFT_VAL & ~BIT_WMAC_ENSHIFT);
	}
#endif  //(IS_RTL8197F_SERIES)

	// This part is not in WMAC InitMAC()
	// Set SEC register
	HAL_RTL_W16(REG_SECCFG, HAL_RTL_R16(REG_SECCFG) & ~(BIT_RXUHUSEDK | BIT_TXUHUSEDK));

#if (IS_RTL88XX_MAC_V1_V2)
    if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2) {        
		// Set TCR register
		HAL_RTL_W32(REG_TCR, HAL_RTL_R32(REG_TCR) | BIT_CFEND_FORMAT);
	}
#endif

	// Set TCR to avoid deadlock
	HAL_RTL_W32(REG_TCR, HAL_RTL_R32(REG_TCR) | BIT_WMAC_TCR_ERRSTEN_3 | BIT_WMAC_TCR_ERRSTEN_2 | BIT_WMAC_TCR_ERRSTEN_1 | BIT_WMAC_TCR_ERRSTEN_0);

	// Set TCR to enable mactx update DTIM count, group bit, and moreData bit
	HAL_RTL_W8(REG_TCR, HAL_RTL_R8(REG_TCR) | BIT_WMAC_TCR_UPD_TIMIE | BIT_WMAC_TCR_UPD_HGQMD);


	HAL_RTL_W16(REG_RRSR, 0xFFFF);
	HAL_RTL_W8(REG_RRSR + 2, 0xFF);

	// Set Spec SIFS (used in NAV)
	// Joseph test
	HAL_RTL_W16(REG_SPEC_SIFS, (0x10 & BIT_MASK_SPEC_SIFS_OFDM) << BIT_SHIFT_SPEC_SIFS_OFDM
				| (0x0A & BIT_MASK_SPEC_SIFS_CCK) << BIT_SHIFT_SPEC_SIFS_CCK);


	// Set SIFS for CTX
	// Joseph test
	HAL_RTL_W32(REG_SIFS, ((0x0A & BIT_MASK_SIFS_CCK_CTX) << BIT_SHIFT_SIFS_CCK_CTX)
				| ((0x10 & BIT_MASK_SIFS_OFDM_CTX) << BIT_SHIFT_SIFS_OFDM_CTX)
				// Set SIFS for TRX
				// Joseph test
				| ((0x0A & BIT_MASK_SIFS_CCK_TRX) << BIT_SHIFT_SIFS_CCK_TRX)
				| ((0x10 & BIT_MASK_SIFS_OFDM_TRX) << BIT_SHIFT_SIFS_OFDM_TRX));

	// EIFS
	HAL_RTL_W16(REG_EIFS, 0x0040);	// eifs = 40 us

	// Set retry limit
#if 0
	HAL_VAR_RETRY_LIMIT_SHORT           = RETRY_LIMIT_SHORT_AP;
	HAL_VAR_RETRY_LIMIT_LONG            = RETRY_LIMIT_LONG_AP;

#if CFG_HAL_SUPPORT_CLIENT_MODE
	HAL_VAR_RETRY_LIMIT_SHORT           = RETRY_LIMIT_SHORT_CLIENT;
	HAL_VAR_RETRY_LIMIT_LONG            = RETRY_LIMIT_LONG_CLIENT;
#endif

	HAL_RTL_W16(REG_RETRY_LIMIT, (HAL_VAR_RETRY_LIMIT_SHORT & BIT_MASK_SRL) << BIT_SHIFT_SRL |
				(HAL_VAR_RETRY_LIMIT_LONG & BIT_MASK_LRL) << BIT_SHIFT_LRL);
#else

	// Set retry limit
	if (HAL_VAR_RETRY_LIMIT_LONG_MIB)
		HAL_VAR_RETRY_LIMIT = HAL_VAR_RETRY_LIMIT_LONG_MIB & 0xff;
	else {
#ifdef CLIENT_MODE
		if (HAL_OPMODE & WIFI_STATION_STATE)
			HAL_VAR_RETRY_LIMIT = RETRY_LIMIT_LONG_CLIENT;
		else
#endif
		{
			if (HAL_VAR_PHYBANDSELECT == PHY_BAND_2G)
				HAL_VAR_RETRY_LIMIT = RETRY_LIMIT_LONG_AP;
			else
				HAL_VAR_RETRY_LIMIT = RETRY_LIMIT_LONG_AP;
		}
	}
	if (HAL_VAR_RETRY_LIMIT_SHORT_MIB)
		HAL_VAR_RETRY_LIMIT |= ((HAL_VAR_RETRY_LIMIT_SHORT_MIB & 0xff) << 8);
	else {
#ifdef CLIENT_MODE
		if (HAL_OPMODE & WIFI_STATION_STATE)
			HAL_VAR_RETRY_LIMIT |= (RETRY_LIMIT_SHORT_CLIENT << 8);
		else
#endif
		{
			if (HAL_VAR_PHYBANDSELECT == PHY_BAND_2G)
				HAL_VAR_RETRY_LIMIT |= (RETRY_LIMIT_SHORT_AP_2G << 8);
			else
				HAL_VAR_RETRY_LIMIT |= (RETRY_LIMIT_SHORT_AP << 8);
		}
	}
	HAL_RTL_W16(REG_RETRY_LIMIT, HAL_VAR_RETRY_LIMIT);

#endif

	// disable BT_enable
	HAL_RTL_W8(REG_GPIO_MUXCFG, 0);

	// if AMSDU MAC size exceed 8K, fill pkt limit to 11k
	if (HAL_VAR_AMSDURECVMAX > 2) {
		HAL_RTL_W8(REG_RX_PKT_LIMIT, BIT_RXPKTLMT(0x16));
	}

#if 1 /*  TODO: Filen, Because 8881A Pin Mux issue */
	if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
		HAL_RTL_W32(REG_LED_CFG, HAL_RTL_R32(REG_LED_CFG) & ~(BIT23 | BIT22));
	}
#endif

#if CFG_HAL_HW_FILL_MACID
	// set CRC5 buffer addr
	HAL_RTL_W32(REG_SEARCH_MACID, HAL_RTL_R32(REG_SEARCH_MACID) | BIT_INFO_INDEX_OFFSET(0x300) ); // set CRC5 buffer addr
#if CFG_HAL_HW_DETEC_POWER_STATE
#if (IS_RTL8192F_SERIES)
    if (IS_HARDWARE_TYPE_8192F(Adapter)) {
	    HAL_RTL_W32(REG_PWR_INT_CTRL, HAL_RTL_R32(REG_PWR_INT_CTRL) | BIT7 | BIT0); // set ps detect enable
	}
#else
	HAL_RTL_W32(0x7D4, HAL_RTL_R32(0x7D4) | BIT17); // set ps detect enable
	HAL_RTL_W32(0x1150, 0x1); // set ps seq debug port
#endif

#endif //#if CFG_HAL_HW_DETEC_POWER_STATE
#endif 

#if CFG_HAL_TX_AMSDU && IS_RTL88XX_MAC_V2
    if ( IS_SUPPORT_TX_AMSDU(Adapter) && _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2 ) {
        // Enable 0x0B0[31] timer interrupt
        //HAL_RTL_W8(REG_MISC_CTRL, HAL_RTL_R8(REG_MISC_CTRL)|BIT3);

        // Enable 0x0B0[30] timer interrupt, for port0.  0x5B1[5:3] = 3b'000
        //HAL_RTL_W8(REG_EARLY_128US, HAL_RTL_R8(REG_EARLY_128US) & 0xC7);

        // Enable 0x0B0[29] timer interrupt, for port0. 0x5B4[2:0] = 3b'000
#if (IS_RTL8192F_SERIES) || (IS_RTL8197F_SERIES) || (IS_RTL8198F_SERIES) || (IS_RTL8814A_SERIES) || (IS_RTL8821C_SERIES) || (IS_RTL8822B_SERIES) || (IS_RTL8822C_SERIES) || (IS_RTL8812F_SERIES)
		if (IS_HARDWARE_TYPE_8192F(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) || 
			IS_HARDWARE_TYPE_8814A(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8822B(Adapter) ||
			IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter))
		{
        	HAL_RTL_W8(REG_TIMER0_SRC_SEL, HAL_RTL_R8(REG_TIMER0_SRC_SEL) & 0xF8);
		}
#endif
    }
#endif //#if CFG_HAL_TX_AMSDU


	if (IS_C_CUT_8192E(Adapter)) {
		unsigned int dwMACTmp1;
		dwMACTmp1 = HAL_RTL_R32(0x24);
		dwMACTmp1 &= ~(BIT(20) | BIT(21));
		if (Adapter->pmib->dot11nConfigEntry.dot11nUse40M == CHANNEL_WIDTH_5) {
			dwMACTmp1 |= (BIT(21));
			HAL_RTL_W32(0x24, dwMACTmp1);
		} else if (Adapter->pmib->dot11nConfigEntry.dot11nUse40M == CHANNEL_WIDTH_10) {
			dwMACTmp1 |= (BIT(20));
			HAL_RTL_W32(0x24, dwMACTmp1);
		}
	}

#if IS_EXIST_SDIO
	if (Adapter->hci_type == RTL_HCI_SDIO) {
		_initSdioAggregationSetting(Adapter);
	}
#endif

#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
	if (Adapter->hci_type == RTL_HCI_USB) {
    #if defined(CONFIG_RTL_92E_SUPPORT) || defined(CONFIG_RTL_92F_SUPPORT)
		_InitQueuePriority(Adapter);
		_init_UsbBusSetting(Adapter);
		_init_UsbAggregationSetting(Adapter);
	#else
        #error "triband undefined!!"
	#endif
	}
#endif

#if	IS_EXIST_RTL8814AE
	//initialize sepcial queue number
	u1Byte mac_id = 0;
	u1Byte zero_value = 0;
	if ( IS_HARDWARE_TYPE_8814A(Adapter) ) {
		for(mac_id = 1; mac_id < MaxMacIDNum; mac_id++){
			GET_HAL_INTERFACE(Adapter)->SetTxRPTHandler(Adapter,mac_id,TXRPT_VAR_SPECIALQ_PKT_NUM1,&zero_value);
			GET_HAL_INTERFACE(Adapter)->SetTxRPTHandler(Adapter,mac_id,TXRPT_VAR_SPECIALQ_PKT_NUM2,&zero_value);
		}
	}
#endif

#if IS_EXIST_RTL8197FEM
    if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
        // Disable BT coextense 
        HAL_RTL_W8(REG_DIS_TXREQ_CLR,0x80);
        HAL_RTL_W32(REG_SYS_CLK_CTRL,HAL_RTL_R32(REG_SYS_CLK_CTRL)|BIT14);
    }
#endif //#if      IS_EXIST_RTL8197FEM
    
#ifdef WLAN_SUPPORT_H2C_PACKET
    if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
        //3 init h2c pkt reg
        u32 Value32;
        u8 Value8;
         //Set H2C cmd related register    
        Value32 = HAL_RTL_R32(REG_H2C_HEAD);    
        Value32 = (Value32 & 0xFFFC0000) | 0xFB00;    
        //Value32 = (Value32 & 0xFFFC0000) | 0x4000;    
        HAL_RTL_W32(REG_H2C_HEAD, Value32);    
        Value32 = HAL_RTL_R32(REG_H2C_READ_ADDR);    
        Value32 = (Value32 & 0xFFFC0000) | 0xFB00;    
        //Value32 = (Value32 & 0xFFFC0000) | 0x4000;    
        HAL_RTL_W32(REG_H2C_READ_ADDR, Value32);    
        Value32 = HAL_RTL_R32(REG_H2C_TAIL);    
        Value32 = (Value32 & 0xFFFC0000) | 0xFF00;    
        //Value32 = (Value32 & 0xFFFC0000) | 0x4380;    
        HAL_RTL_W32(REG_H2C_TAIL, Value32);    
//        Adapter->h2c_buf_len = 0xFF00 - 0xFB00;     
        //pveri_adapt->h2c_tx_fifo_size = 0x4380 - 0x4000;     
        Value8 = HAL_RTL_R8(REG_H2C_INFO);    
        Value8 = (Value8 & 0xFC) | 0x01;    
        HAL_RTL_W8(REG_H2C_INFO, Value8);        
//        Value32 = HAL_RTL_R32(REG_RXFF_BNDY);    
//        Value32 = (Value32 & 0xFFFF0000) | 0x3C00;    
//        HAL_RTL_W32(REG_RXFF_BNDY, Value32);        
        Value8 = HAL_RTL_R8(REG_H2C_INFO);    
        Value8 = (Value8 & 0xFB) | 0x04;    
        HAL_RTL_W8(REG_H2C_INFO, Value8);
        
    }
#endif
#if defined(AP_SWPS_OFFLOAD)
     InitAPSWPS88XX(Adapter);
#endif
    
	RT_TRACE(COMP_INIT, DBG_LOUD, ("%s===>\n", __FUNCTION__) );

	return RT_STATUS_SUCCESS;
}
#endif //#if (IS_RTL88XX_MAC_V1V2)

#if (IS_RTL88XX_MAC_V3)
enum rt_status
InitMACProtocol88XX(
	IN  HAL_PADAPTER Adapter
)
{

    
    return RT_STATUS_SUCCESS;
}

enum rt_status
InitMACScheduler88XX(
	IN  HAL_PADAPTER Adapter
)
{

	// Set SLOT time
	HAL_RTL_W8(REG_SLOT, 0x09);
    
	HAL_RTL_W8(REG_MBSSID_BCN_SPACE3+2, 0x64);
	
    return RT_STATUS_SUCCESS;
}

enum rt_status
InitMACWMAC88XX(
	IN  HAL_PADAPTER Adapter
)
{
	u8  MulticastAddr[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	// Set RCR register
   	//HAL_RTL_W32(REG_RCR, BIT_APP_FCS | BIT_APP_MIC | BIT_APP_ICV | BIT_APP_PHYSTS | BIT_VHT_DACK | BIT_HTC_LOC_CTRL
	//	| BIT_ACRC32 | BIT_AB | BIT_AM | BIT_APM | BIT_AAP);
	
 	    HAL_RTL_W32(REG_RCR, BIT_APP_FCS | BIT_APP_MIC | BIT_APP_ICV | BIT_APP_PHYSTS | BIT_VHT_DACK | BIT_AB | BIT_AM | BIT_APM);
    
	// This part is not in WMAC InitMAC()
	// Set SEC register
	HAL_RTL_W16(REG_SECCFG, HAL_RTL_R16(REG_SECCFG) & ~(BIT_RXUHUSEDK | BIT_TXUHUSEDK));
    
#if IS_RTL88XX_MAC_V3
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v3)    
	    // Set TCR register
	    HAL_RTL_W32(REG_TCR, HAL_RTL_R32(REG_TCR) | BIT_CFEND_FORMAT);  //14B didnt have this bit
#endif
	// Set TCR to avoid deadlock
	HAL_RTL_W32(REG_TCR, HAL_RTL_R32(REG_TCR) | BIT_WMAC_TCR_ERRSTEN_3 | BIT_WMAC_TCR_ERRSTEN_2 | BIT_WMAC_TCR_ERRSTEN_1 | BIT_WMAC_TCR_ERRSTEN_0);

	// Set TCR to enable mactx update DTIM count, group bit, and moreData bit
	HAL_RTL_W8(REG_TCR, HAL_RTL_R8(REG_TCR) | BIT_WMAC_TCR_UPD_TIMIE | BIT_WMAC_TCR_UPD_HGQMD);

    // get RF antenna
    GET_HAL_INTERFACE(Adapter)->GetHwRegHandler(Adapter, HW_VAR_NUM_TOTAL_RF_PATH, (pu1Byte)&Adapter->pshare->phw->NumTotalRFPath);

    // set Multicast addr
	GET_HAL_INTERFACE(Adapter)->SetHwRegHandler(Adapter, HW_VAR_MULTICAST_REG, (pu1Byte)MulticastAddr);
	GET_HAL_INTERFACE(Adapter)->SetHwRegHandler(Adapter, HW_VAR_BSSID, (pu1Byte)Adapter->pmib->dot11OperationEntry.hwaddr);    
    GET_HAL_INTERFACE(Adapter)->SetHwRegHandler(Adapter, HW_VAR_ETHER_ADDR, (pu1Byte)Adapter->dev->dev_addr);
    
    // security 
    GET_HAL_INTERFACE(Adapter)->SetHwRegHandler(Adapter, HW_VAR_CAM_RESET_ALL_ENTRY, NULL);
    
    return RT_STATUS_SUCCESS;
}

enum rt_status
InitMACSysyemCfg88XX(
	IN  HAL_PADAPTER Adapter
)
{
    unsigned int opmode;

    opmode = Adapter->pmib->dot11OperationEntry.opmode;
    
    if (opmode & WIFI_AP_STATE) {
		GET_HAL_INTERFACE(Adapter)->InitIMRHandler(Adapter, RT_OP_MODE_AP);
	} else if (opmode & WIFI_STATION_STATE) {
		GET_HAL_INTERFACE(Adapter)->InitIMRHandler(Adapter, RT_OP_MODE_INFRASTRUCTURE);
	} else if (opmode & WIFI_ADHOC_STATE) {
		GET_HAL_INTERFACE(Adapter)->InitIMRHandler(Adapter, RT_OP_MODE_IBSS);
	}

    #if defined(SW_TX_QUEUE) || defined(RTK_ATM)
    GET_HAL_INTERFACE(Adapter)->AddInterruptMaskHandler(Adapter, HAL_INT_TYPE_GTIMER4);
    #endif
	// TODO: Filen, no need to sync !?
	Adapter->pshare->InterruptMask = _GET_HAL_DATA(Adapter)->IntMask[0];
	Adapter->pshare->InterruptMask = _GET_HAL_DATA(Adapter)->IntMask[1];


    return RT_STATUS_SUCCESS;
}

enum rt_status
InitMACFunction88XX(
	IN  HAL_PADAPTER Adapter
)
{

	char shcut_pass;
#ifdef CONFIG_WLAN_HAL_8197G
	if (IS_HARDWARE_TYPE_8197G(Adapter))
		shcut_pass = BIT_SHCUT_BYPASS_2;
#endif	

#ifdef CONFIG_WLAN_HAL_8198F
	if (IS_HARDWARE_TYPE_8198F(Adapter))		
		shcut_pass = BIT_SHCUT_BYPASS;
#endif


	
#if (CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC)
	if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC(Adapter)) {
        #if IS_RTL88XX_MAC_V2_V3
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3) {
		    // Enable HW TX shortcut
            HAL_RTL_W32(REG_CR, HAL_RTL_R32(REG_CR) | BIT_SHCUT_EN);
            if (IS_HAL_TEST_CHIP(Adapter)==FALSE) {
                HAL_RTL_W32(REG_SHCUT_SETTING, HAL_RTL_R32(REG_SHCUT_SETTING) & ~BIT8);
            } else {
                HAL_RTL_W32(REG_TXRPT_START_OFFSET, HAL_RTL_R32(REG_TXRPT_START_OFFSET) & ~shcut_pass);
            }
        }
        #endif
    }
#endif //CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV_LLC
    if(IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(Adapter) && IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV_LLC(Adapter)){
        HAL_RTL_W32(REG_CR, HAL_RTL_R32(REG_CR) | BIT_SHCUT_EN);//Enable HW TX shortcut
    #if IS_RTL88XX_MAC_V4
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
        //enable Hdr conversion
        HAL_RTL_W16(REG_SHCUT_SETTING,HAL_RTL_R16(REG_SHCUT_SETTING)|BIT_MAC_HDR_CONVERT_EN);
    }
    #endif
        if (IS_HAL_TEST_CHIP(Adapter)==FALSE) {
            // Enable auto append LLC to header
            HAL_RTL_W32(REG_SHCUT_SETTING, HAL_RTL_R32(REG_SHCUT_SETTING) | (BIT10|BIT11));
        }
        
#if CFG_HAL_HW_TX_AMSDU
        if(IS_SUPPORT_WLAN_HAL_HW_TX_AMSDU(Adapter)){
            HAL_RTL_W8(0x2250, HAL_RTL_R8(0x2250) | BIT0);//enable HW-AMSDU
            HAL_RTL_W16(0x2254, 0xff);//AMSDU timeout
            HAL_RTL_W16(0x2256, 0xff);//packet enter timeout
            
        }
    }
#endif
#endif // CFG_HAL_HW_TX_SHORTCUT_HDR_CONV_LLC
#endif // CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
	

#if CFG_HAL_HW_FILL_MACID
    if( IS_SUPPORT_HW_FILL_MACID(Adapter))
    {
#if IS_RTL88XX_MAC_V2_V3
	    // set CRC5 buffer addr
    	HAL_RTL_W32(REG_SEARCH_MACID, HAL_RTL_R32(REG_SEARCH_MACID) | BIT_INFO_INDEX_OFFSET(0x300) ); // set CRC5 buffer addr
#endif //#if IS_RTL88XX_MAC_V2_V3
#if CFG_HAL_HW_DETEC_POWER_STATE

    if( IS_SUPPORT_HW_DETEC_POWER_STATE(Adapter))
    {
	    HAL_RTL_W32(0x7D4, HAL_RTL_R32(0x7D4) | BIT17); // set ps detect enable
    	HAL_RTL_W32(REG_POWSEQ, 0x1); // set ps seq debug port
    }
#endif //#if CFG_HAL_HW_DETEC_POWER_STATE
    }
#endif 

#ifdef WLAN_SUPPORT_H2C_PACKET
    if ( IS_SUPPORT_H2C_PACKET(Adapter) ) {
        //3 init h2c pkt reg
        u32 Value32;
        u8 Value8;
         //Set H2C cmd related register    
        Value32 = HAL_RTL_R32(REG_H2C_HEAD);    
        Value32 = (Value32 & 0xFFFC0000) | 0xFB00;    
        //Value32 = (Value32 & 0xFFFC0000) | 0x4000;    
        HAL_RTL_W32(REG_H2C_HEAD, Value32);    
        Value32 = HAL_RTL_R32(REG_H2C_READ_ADDR);    
        Value32 = (Value32 & 0xFFFC0000) | 0xFB00;    
        //Value32 = (Value32 & 0xFFFC0000) | 0x4000;    
        HAL_RTL_W32(REG_H2C_READ_ADDR, Value32);    
        Value32 = HAL_RTL_R32(REG_H2C_TAIL);    
        Value32 = (Value32 & 0xFFFC0000) | 0xFF00;    
        //Value32 = (Value32 & 0xFFFC0000) | 0x4380;    
        HAL_RTL_W32(REG_H2C_TAIL, Value32);    
//        Adapter->h2c_buf_len = 0xFF00 - 0xFB00;     
        //pveri_adapt->h2c_tx_fifo_size = 0x4380 - 0x4000;     
        Value8 = HAL_RTL_R8(REG_H2C_INFO);    
        Value8 = (Value8 & 0xFC) | 0x01;    
        HAL_RTL_W8(REG_H2C_INFO, Value8);        
//        Value32 = HAL_RTL_R32(REG_RXFF_BNDY);    
//        Value32 = (Value32 & 0xFFFF0000) | 0x3C00;    
//        HAL_RTL_W32(REG_RXFF_BNDY, Value32);        
        Value8 = HAL_RTL_R8(REG_H2C_INFO);    
        Value8 = (Value8 & 0xFB) | 0x04;    
        HAL_RTL_W8(REG_H2C_INFO, Value8);
        
    }
#endif
#if defined(AP_SWPS_OFFLOAD)
    InitAPSWPS88XX(Adapter);
#elif defined(AP_SWPS_OFFLOAD_WITH_AMPDU_VERI)
    InitAPSWPS_AMPDU_Verification(Adapter);    
#endif

#if 0//#ifdef STA_AGING_FUNC_OFFLOAD //already set in other cmd
        if(IS_SUPPORT_AGING_FUNC_OFFLOAD(Adapter))
            GET_HAL_INTERFACE(Adapter)->AgingFuncOfflaodHandler(Adapter,8,1);//yllinSWPS, need to update qnum
#endif

#if CFG_HAL_TX_AMSDU && IS_RTL88XX_MAC_V3
    if ( IS_SUPPORT_TX_AMSDU(Adapter) && _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v3 ) {
        // Enable 0x0B0[31] timer interrupt
        //HAL_RTL_W8(REG_MISC_CTRL, HAL_RTL_R8(REG_MISC_CTRL)|BIT3);

        // Enable 0x0B0[30] timer interrupt, for port0.  0x5B1[5:3] = 3b'000
        //HAL_RTL_W8(REG_EARLY_128US, HAL_RTL_R8(REG_EARLY_128US) & 0xC7);

        // Enable 0x0B0[29] timer interrupt, for port0. 0x5B4[2:0] = 3b'000
        HAL_RTL_W8(REG_TIMER0_SRC_SEL, HAL_RTL_R8(REG_TIMER0_SRC_SEL) & 0xF8);
    }
#endif //#if CFG_HAL_TX_AMSDU

#if CFG_HAL_SUPPORT_ENHANCED_EDCA
    u32 edcaPara;
    if ( IS_SUPPORT_ENHANCED_EDCA(Adapter) ) {
#if IS_RTL88XX_MAC_V3        
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v3){
        edcaPara = (0x20 | BIT_RANDOM_VALUE_SHIFT(0x4) | BIT_ENABLE_NEW_EDCA);
        HAL_RTL_W32(REG_NEW_EDCA_CTRL, edcaPara);    
    }
#endif        
#if IS_RTL88XX_MAC_V4
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
        edcaPara = (0x20 | BIT_RANDOM_VALUE_SHIFT(0x4) | BIT_ENABLE_NEW_EDCA);
        HAL_RTL_W32(REG_NEW_EDCA_CTRL_V1, edcaPara);  
    }
#endif
    }
#endif 

#ifdef DRVMAC_LB 
#if IS_RTL88XX_MAC_V4
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)        
            HAL_RTL_W32(REG_ACH_CTRL,HAL_RTL_R32(REG_ACH_CTRL) & ~BIT_STOP_P0BCNQ); //its set in init_beacon if not in loopback mode
#endif        
#endif

#ifdef CONFIG_8814_AP_MAC_VERI

    //test use, init txff to 0xffffffff for fist 0x400 bytes    
    int indx=0;

    HAL_RTL_W32(0x140, 0x780);

    for(indx=0;indx<100;indx++)
    {
        HAL_RTL_W32(0x8000+indx*4, 0xffffffff);

    }

    //test use, init txreport to 0xffffffff 
    HAL_RTL_W32(0x140, 0x662);
    for(indx=0;indx<1024;indx++)
    {
        HAL_RTL_W32(0x8000+indx*4, 0xffffffff);
    }
    //tingchu test, init shortcut buffer to 0x0
    HAL_RTL_W32(0x140,0x663);
    for(indx=0;indx<256;indx++)
    {
        HAL_RTL_W32(0x8c00+indx*4, 0x0);
    }
#endif

    return RT_STATUS_SUCCESS;
}

enum rt_status
InitMACTRX88XX(
	IN  HAL_PADAPTER Adapter
)
{
    enum rt_status   status;

    /*****************************
    //   TX init                //
    ****************************/
#if 0
    if(IS_HARDWARE_MACHAL_SUPPORT(Adapter)){
        //set driver info size
        GET_MACHALAPI_INTERFACE(Adapter)->halmac_cfg_drv_info(Adapter->pHalmac_adapter,HALMAC_DRV_INFO_PHY_STATUS);
        return RT_STATUS_SUCCESS;
    }
#endif

// TRX DMA Queue Mapping
#if IS_EXIST_PCI || IS_EXIST_EMBEDDED
	if (Adapter->hci_type == RTL_HCI_PCIE) {
#if (IS_RTL8198F_SERIES || IS_RTL8197G_SERIES)
	if (IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter))
		{
			HAL_RTL_W32(REG_TXDMA_PQ_MAP, (TRX_DMA_QUEUE_MAP_PARA_V1 | BIT_RXSHFT_EN | BIT_RXDMA_ARBBW_EN));
		}
#endif //IS_RTL8198F_SERIES || IS_RTL8197G_SERIES
	}
#endif	
#if IS_EXIST_USB || IS_EXIST_SDIO
	if (Adapter->hci_type == RTL_HCI_USB || Adapter->hci_type == RTL_HCI_SDIO) {
		HAL_RTL_W32(REG_TXDMA_PQ_MAP, (HAL_RTL_R32(REG_TXDMA_PQ_MAP) & 0x7) | TRX_DMA_QUEUE_MAP_PARA);
	}
#endif //IS_EXIST_PCI || IS_EXIST_EMBEDDED

    
    //System init
	status = GET_HAL_INTERFACE(Adapter)->InitLLT_TableHandler(Adapter);


	if ( RT_STATUS_SUCCESS != status ) {
		RT_TRACE(COMP_INIT, DBG_SERIOUS, ("LLT_table_init Failed \n") );
		return RT_STATUS_FAILURE;
	} else {
		RT_TRACE(COMP_INIT, DBG_TRACE, ("LLT_table_init OK \n") );
	}
    
// RXFF

#if (IS_RTL8198F_SERIES || IS_RTL8197G_SERIES)
    if (IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter)) {
        HAL_RTL_W32(REG_RXFF_BNDY,(HAL_RTL_R32(REG_RXFF_BNDY) & 0xFFFC0000) | BIT_RXFF0_BNDY_V2(MAC_RXFF_SIZE));
	}
#endif //(IS_RTL8198F_SERIES || IS_RTL8197G_SERIES)

	
#if (IS_RTL8198F_SERIES || IS_RTL8197G_SERIES)
	if (IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter)) {
		// Set Driver info size
		HAL_RTL_W8(REG_RX_DRVINFO_SZ, (HAL_RTL_R8(REG_RX_DRVINFO_SZ) & 0xF0 ) | BIT_DRVINFO_SZ_V1(4));
	}
#endif  //(IS_RTL8198F_SERIES || IS_RTL8197G_SERIES)

	// If Pre-TX enable, enable pre-txcmd delay count function
#if (IS_RTL8197G_SERIES)
		if (IS_HARDWARE_TYPE_8197G(Adapter)){
			if(HAL_RTL_R8(REG_SW_AMPDU_BURST_MODE_CTRL) & BIT_PRE_TX_CMD)
			{
				u4Byte pre_tx_valueTime;
				pre_tx_valueTime = BIT_SET_PRECNT_TH(HAL_RTL_R32(REG_VIDEO_ENHANCEMENT_FUN), 0x1E4)|BIT_EN_PRECNT;
				HAL_RTL_W32(REG_VIDEO_ENHANCEMENT_FUN,pre_tx_valueTime);  
			}
		}	
#endif

#if (IS_RTL8198F_SERIES)
		if (IS_HARDWARE_TYPE_8198F(Adapter)){
			if(HAL_RTL_R8(REG_SW_AMPDU_BURST_MODE_CTRL) & BIT_PRE_TX_CMD)
			{
				u4Byte pre_tx_valueTime;
				pre_tx_valueTime = BIT_SET_MAX_PRETX_AGGR_TIME(HAL_RTL_R32(REG_VIDEO_ENHANCEMENT_FUN), 0x1E4)|BIT_MAX_PRETX_AGGR_EN;
				HAL_RTL_W32(REG_VIDEO_ENHANCEMENT_FUN,pre_tx_valueTime);  
			}
		}
#endif
	

	// if AMSDU MAC size exceed 8K, fill pkt limit to 11k
	if (HAL_VAR_AMSDURECVMAX > 2) {
		HAL_RTL_W8(REG_RX_PKT_LIMIT, BIT_RXPKTLMT(0x16));
	}
      
    return RT_STATUS_SUCCESS;
}

#endif //#if (IS_RTL88XX_MAC_V3_V4)

VOID
InitIMR88XX(
	IN  HAL_PADAPTER    Adapter,
	IN  RT_OP_MODE      op_mode
)
{
	PHAL_DATA_TYPE              pHalData = _GET_HAL_DATA(Adapter);
    u4Byte  bit_hisr1_ind_int, bit_bedok, bit_axi_except_hint;
	RT_TRACE_F(COMP_INIT, DBG_LOUD, ("\n"));
#if IS_RTL88XX_MAC_V1_V2_V3
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3){
        bit_hisr1_ind_int = BIT_HISR1_IND_INT;
        bit_bedok =    BIT_BEDOK;        
    }
#endif    
#if IS_RTL88XX_MAC_V4
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
        bit_hisr1_ind_int = BIT_HISR1_IND;
        bit_bedok =    BIT_TXDMAOK_CHANNEL2_MSK;
    }
#endif    

#ifdef CONFIG_PE_ENABLE
    if(Adapter->pshare->rf_ft_var.manual_pe_enable){
        pHalData->IntMask[0]        = bit_hisr1_ind_int;
    }else
#endif
    {
	    pHalData->IntMask[0]        = BIT_RXOK | bit_hisr1_ind_int | BIT_RDU;
    }

#if (IS_RTL8814A_SERIES || IS_RTL8822B_SERIES)
		if ((GET_CHIP_VER(Adapter) == VERSION_8814A) || (GET_CHIP_VER(Adapter) == VERSION_8822B)) {
			pHalData->IntMask[0] |= BIT_HISR2_IND_INT | BIT_HISR3_IND_INT;
		}
#endif
#if (IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
		if((GET_CHIP_VER(Adapter) == VERSION_8822C) || (GET_CHIP_VER(Adapter) == VERSION_8812F)) {
			pHalData->IntMask[0] |= BIT_HISR2_IND | BIT_HISR3_IND;
		}
#endif
#if IS_RTL88XX_MAC_V3
    if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v3) {  
        pHalData->IntMask[0]        |= BIT_HISR2_IND_INT | BIT_HISR3_IND_INT | BIT_HISR4_IND_INT | BIT_HISR5_IND_INT;
    }
#endif
#if IS_RTL88XX_MAC_V4
    if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4) {  
        pHalData->IntMask[0]        |= BIT_HISR2_IND | BIT_HISR3_IND | BIT_HISR4_IND | BIT_HISR5_IND;
    }
#endif

#if CFG_HAL_MP_TEST
	if (HAL_VAR_MP_SPECIFIC)
		pHalData->IntMask[0]    |= bit_bedok;
#endif
#ifdef CONFIG_PE_ENABLE
    if(Adapter->pshare->rf_ft_var.manual_pe_enable){
        pHalData->IntMask[1]        = /*BIT_TXFOVW |*/ BIT_TXERR_INT;
    }else
#endif
    {
	    pHalData->IntMask[1]        = BIT_FOVW /*| BIT_TXFOVW*/ | BIT_RXERR_INT | BIT_TXERR_INT;
    } 
#ifdef TXFOVW_INT_EN
	// [CAUTION] excessive TXFOVW interrupt will downgrade throughput
	pHalData->IntMask[1] |= BIT_TXFOVW;
#endif
	
#if CFG_HAL_TX_AMSDU
	if ( IS_SUPPORT_TX_AMSDU(Adapter) ) {
#ifdef BIT_PSTIMER_0_MSK
		pHalData->IntMask[0]    |= BIT_PSTIMER_0_MSK;
#else
		pHalData->IntMask[0]    |= BIT_PSTIMEOUT;
#endif
	}
#endif //#if CFG_HAL_TX_AMSDU

	switch (op_mode) {
        case RT_OP_MODE_AP:
            if (IS_HAL_TEST_CHIP(Adapter)) {
                pHalData->IntMask[0]	 |= BIT_BCNDMAINT0 | BIT_TXBCN0OK | BIT_TXBCN0ERR;
            } else {
                pHalData->IntMask[0]     |= BIT_BCNDMAINT0 | BIT_TXBCN0OK | BIT_TXBCN0ERR;
            }
#ifdef TXREPORT
            pHalData->IntMask[0]     |= BIT_C2HCMD;
#endif
#ifdef SUPPORT_AP_OFFLOAD
            //yllin
            if(IS_SUPPORT_AP_OFFLOAD(Adapter))
                pHalData->IntMask[0]     |= BIT_CPWM2;
#endif
#if CFG_HAL_HW_DETEC_POWER_STATE
            if(IS_SUPPORT_HW_DETEC_POWER_STATE(Adapter)){
            #if IS_RTL88XX_MAC_V2_V3
                if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3)
                    pHalData->IntMask[0]     |= BIT_HISR3_IND_INT_MSK;
            #endif
            #if IS_RTL88XX_MAC_V4
                if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)
                    pHalData->IntMask[0]     |= BIT_HISR3_IND_MSK;
            #endif
            }
#endif

#if CFG_HAL_SUPPORT_EACH_VAP_INT
            if ( IS_SUPPORT_EACH_VAP_INT(Adapter) ) {
                
#if CFG_HAL_SUPPORT_MBSSID
                pHalData->IntMask[2] = BIT_TXBCN7OK | BIT_TXBCN6OK | BIT_TXBCN5OK | BIT_TXBCN4OK | BIT_TXBCN3OK | BIT_TXBCN2OK | BIT_TXBCN1OK | \
                                       BIT_TXBCN7ERR | BIT_TXBCN6ERR | BIT_TXBCN5ERR | BIT_TXBCN4ERR | BIT_TXBCN3ERR | BIT_TXBCN2ERR | BIT_TXBCN1ERR;

#if IS_RTL8198F_SERIES
            if (IS_HARDWARE_TYPE_8198F(Adapter))
            {
                                    pHalData->IntMask[4] |= BIT_TX_BCN_OK_INT8 | BIT_TX_BCN_OK_INT9 | BIT_TX_BCN_OK_INT10 | BIT_TX_BCN_OK_INT11 | BIT_TX_BCN_OK_INT12 | BIT_TX_BCN_OK_INT13 | BIT_TX_BCN_OK_INT14 | BIT_TX_BCN_OK_INT15 |  \
                                                        BIT_TX_BCN_ERR_INT8 | BIT_TX_BCN_ERR_INT9 | BIT_TX_BCN_ERR_INT10 | BIT_TX_BCN_ERR_INT11 | BIT_TX_BCN_ERR_INT12 | BIT_TX_BCN_ERR_INT13 | BIT_TX_BCN_ERR_INT14 | BIT_TX_BCN_ERR_INT15 | \
                                                        BIT_BCN_DMA_INT8| BIT_BCN_DMA_INT9| BIT_BCN_DMA_INT10 | BIT_BCN_DMA_INT11 | BIT_BCN_DMA_INT12 | BIT_BCN_DMA_INT13 | BIT_BCN_DMA_INT14 | BIT_BCN_DMA_INT15 ;
                    }
#endif //IS_RTL8198F_SERIES            

#if IS_RTL88XX_MAC_V4            
            if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)
            {                
                pHalData->IntMask[4] |= BIT_TXBCN8OK | BIT_TXBCN9OK | BIT_TXBCN10OK | BIT_TXBCN11OK | BIT_TXBCN12OK | BIT_TXBCN13OK | BIT_TXBCN14OK | BIT_TXBCN15OK |  \
                                        BIT_TXBCN8ERR | BIT_TXBCN9ERR | BIT_TXBCN10ERR | BIT_TXBCN11ERR | BIT_TXBCN12ERR | BIT_TXBCN13ERR | BIT_TXBCN14ERR | BIT_TXBCN15ERR ;                
            }
#endif //IS_RTL88XX_MAC_V4  

#endif //CFG_HAL_SUPPORT_MBSSID
            }
#endif // #if CFG_HAL_SUPPORT_EACH_VAP_INT
#if CFG_HAL_HW_DETEC_POWER_STATE
            if(IS_SUPPORT_HW_DETEC_POWER_STATE(Adapter)){
                pHalData->IntMask[3] |= BIT_PWR_INT_31TO0 | BIT_PWR_INT_63TO32 | BIT_PWR_INT_95TO64; 
                #if IS_RTL88XX_MAC_V4
                if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
                    pHalData->IntMask[3] |= BIT_PWR_INT_127TO96 | BIT_SEARCH_FAIL;
                }
                #endif
                #if IS_RTL88XX_MAC_V2_V3
                if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3)
                    pHalData->IntMask[3] |= BIT_PWR_INT_126TO96 | BIT_PWR_INT_127_V1;
                #endif
 #if IS_EXIST_RTL8192FE
			if (IS_HARDWARE_TYPE_8192FE(Adapter)) {
                pHalData->IntMask[1] |= BIT_PWR_INT_31TO0 | BIT_PWR_INT_63TO32 | BIT_PWR_INT_95TO64 | BIT_PWR_INT_126TO96 | BIT_PWR_INT_127; 
			}
			#endif
            }
#endif //CFG_HAL_HW_DETEC_POWER_STATE

#if CFG_HAL_SUPPORT_TXDESC_IE
            if(IS_SUPPORT_TXDESC_IE(Adapter)){
                pHalData->IntMask[3] |= BIT_PKT_TRANS_ERR_MASK;
            }
#endif

#if CFG_HAL_SUPPORT_AXI_BUS_EXCEPTION
            if(IS_SUPPORT_AXI_EXCEPTION(Adapter)) {
                pHalData->IntMask[8] |= BIT_AXI_EXCEPT_HINT;   
            }
#endif // CFG_HAL_SUPPORT_AXI_BUS_EXCEPTION

		break;

#if CFG_HAL_SUPPORT_CLIENT_MODE
	case RT_OP_MODE_INFRASTRUCTURE:
		break;

	case RT_OP_MODE_IBSS:
		pHalData->IntMask[0]     |= BIT_RXOK | bit_hisr1_ind_int;
		break;
#endif  //CFG_HAL_SUPPORT_CLIENT_MODE

	default:
		break;
	}
}


VOID
InitVAPIMR88XX(
	IN  HAL_PADAPTER    Adapter,
	IN  u4Byte          VapSeq
)
{
	PHAL_DATA_TYPE      pHalData = _GET_HAL_DATA(Adapter);

    if(VapSeq > 7){
#if IS_RTL88XX_MAC_V4
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
            
            pHalData->IntMask[5]    |=  BIT_BCNDMAINT8 << (VapSeq - 8);
            HAL_RTL_W32(REG_HIMR_5, pHalData->IntMask[5]);
            
        }
#endif
#if IS_RTL8198F_SERIES //can't use V3 because 97G doesn't have MBSSID8-15
        if(IS_HARDWARE_TYPE_8198F(Adapter))
        {
            pHalData->IntMask[4]    |=  BIT_BCN_DMA_INT8 << (VapSeq - 8);
            HAL_RTL_W32(REG_HIMR4, pHalData->IntMask[4]);
        }
#endif
    }
    else
    {
		pHalData->IntMask[1]    |=  BIT_BCNDMAINT1 << (VapSeq - 1);

		// TODO: Filen, we should add code for relative opeartion belw
#if 0
		pHalData->IntMask[1]    |=  BIT_BCNDERR1 << (VapSeq - 1);
#endif

		HAL_RTL_W32(REG_HIMR1, pHalData->IntMask[1]);
	}
}


#if (HAL_DEV_BUS_TYPE & (HAL_RT_EMBEDDED_INTERFACE | HAL_RT_PCI_INTERFACE))
enum rt_status
InitHCIDMAMem88XX(
	IN  HAL_PADAPTER Adapter
)
{
	PHCI_TX_DMA_MANAGER_88XX    ptx_dma = NULL;
	PHCI_RX_DMA_MANAGER_88XX    prx_dma = NULL;
#if CFG_HAL_TX_AMSDU
    PHCI_TX_AMSDU_DMA_MANAGER_88XX ptx_dma_amsdu = NULL;
#endif

#ifdef CONFIG_NET_PCI
	unsigned char *cpu_addr_desc = NULL;
	dma_addr_t     dma_handle_desc = 0;
	unsigned int   dma_len = 0;
#if CFG_HAL_TX_AMSDU
	dma_addr_t     dma_handle_amsdu = 0;
	unsigned char *cpu_addr_amsdu = NULL;
	int            dma_len_amsdu = 0;
#endif // CFG_HAL_TX_AMSDU

	if (HAL_IS_PCIBIOS_TYPE(Adapter)) {
#if defined(CONFIG_RTL_OFFLOAD_DRIVER) && defined(CONFIG_WLAN_HAL_8814BE)
		if (IS_HARDWARE_TYPE_8814B(Adapter))
			dma_len = H2D_DESC_8814B_DMA_SIZE_MAX + PAGE_SIZE /*alignment*/ + PAGE_SIZE /*FWDL*/;
		else
#endif
			dma_len = DESC_DMA_PAGE_SIZE_MAX_HAL_IF(Adapter);
			cpu_addr_desc = os_dma_alloc(Adapter, dma_len, &dma_handle_desc);
			_GET_HAL_DATA(Adapter)->alloc_dma_buf = (unsigned long)cpu_addr_desc;
			if (NULL == cpu_addr_desc) {
				RT_TRACE_F( COMP_INIT, DBG_SERIOUS, ("Allocate HAL Memory-TX Failed\n") );
				return RT_STATUS_FAILURE;
			}
			_GET_HAL_DATA(Adapter)->ring_dma_addr = dma_handle_desc;
			_GET_HAL_DATA(Adapter)->ring_buf_len = dma_len;
			_GET_HAL_DATA(Adapter)->ring_virt_addr = (unsigned long)cpu_addr_desc;
			printk("DESC: cpu_addr %p, dma_handle %p, size %d\n", cpu_addr_desc, (unsigned long)dma_handle_desc, dma_len);

#if defined(CONFIG_RTL_OFFLOAD_DRIVER) && defined(CONFIG_WLAN_HAL_8814BE)
		if (IS_HARDWARE_TYPE_8814B(Adapter)) {
			/* configure FW download buffer */
			_GET_HAL_DATA(Adapter)->h2d_fwdl_cpu_addr = ALIGN((unsigned long)(cpu_addr_desc + H2D_DESC_8814B_DMA_SIZE_MAX), PAGE_SIZE);
			_GET_HAL_DATA(Adapter)->h2d_fwdl_dma_handle = HAL_VIRT_TO_BUS(_GET_HAL_DATA(Adapter)->h2d_fwdl_cpu_addr);
			printk("OFLD: cpu_addr %p, dma_handle %p, size %d\n", 
				_GET_HAL_DATA(Adapter)->h2d_fwdl_cpu_addr, 
				_GET_HAL_DATA(Adapter)->h2d_fwdl_dma_handle, 
				dma_len);
		}
#endif
	}

#ifdef WLAN_HAL_TX_AMSDU
		// Avoid the combination of (64-bit dma_addr_t + big endian) to get invalid phw->ring_dma_addr
		dma_len_amsdu = DESC_DMA_PAGE_SIZE_HAL_FOR_AMSDU;
		cpu_addr_amsdu = os_dma_alloc(Adapter, dma_len_amsdu, &dma_handle_amsdu);
		_GET_HAL_DATA(Adapter)->desc_dma_buf_amsdu = (unsigned long)cpu_addr_amsdu;
		if (cpu_addr_amsdu == NULL) {
			_GET_HAL_DATA(Adapter)->desc_dma_buf_len = 0;
			_GET_HAL_DATA(Adapter)->desc_dma_buf_len_amsdu = 0;
			printk(KERN_ERR "can't allocate AMSDU descriptior page, abort!\n");
			return RT_STATUS_FAILURE;
		}
		_GET_HAL_DATA(Adapter)->desc_dma_buf_addr_amsdu[0] = dma_handle_amsdu;
		_GET_HAL_DATA(Adapter)->desc_dma_buf_len_amsdu = dma_len_amsdu;
		_GET_HAL_DATA(Adapter)->desc_dma_buf_virt_amsdu = (unsigned long)cpu_addr_amsdu;
		printk("AMSDU: cpu_addr %p, dma_handle %p, size %d\n", cpu_addr_amsdu, (unsigned long)dma_handle_amsdu, dma_len_amsdu);
#endif // WLAN_HAL_TX_AMSDU
#endif // CONFIG_NET_PCI

	{
		ptx_dma = (PHCI_TX_DMA_MANAGER_88XX)HALMalloc(Adapter, sizeof(HCI_TX_DMA_MANAGER_88XX));
		if (NULL == ptx_dma) {
			RT_TRACE_F( COMP_INIT, DBG_SERIOUS, ("Allocate HAL Memory-TX Failed\n") );
			return RT_STATUS_FAILURE;
		}
		prx_dma = (PHCI_RX_DMA_MANAGER_88XX)HALMalloc(Adapter, sizeof(HCI_RX_DMA_MANAGER_88XX));
		if (NULL == prx_dma) {
			HAL_free(ptx_dma);
			RT_TRACE_F( COMP_INIT, DBG_SERIOUS, ("Allocate HAL Memory-RX Failed\n") );
			return RT_STATUS_FAILURE;
		}
		platform_zero_memory(ptx_dma, sizeof(HCI_TX_DMA_MANAGER_88XX));
		platform_zero_memory(prx_dma, sizeof(HCI_RX_DMA_MANAGER_88XX));
		//Register to HAL_DATA
		_GET_HAL_DATA(Adapter)->PTxDMA88XX = ptx_dma;
		_GET_HAL_DATA(Adapter)->PRxDMA88XX = prx_dma;
	}

#if CFG_HAL_TX_AMSDU
    if ( IS_SUPPORT_TX_AMSDU(Adapter) ) {
        ptx_dma_amsdu = (PHCI_TX_AMSDU_DMA_MANAGER_88XX)HALMalloc(Adapter, sizeof(HCI_TX_AMSDU_DMA_MANAGER_88XX));
        if (NULL == ptx_dma_amsdu) {
            RT_TRACE_F( COMP_INIT, DBG_SERIOUS, ("Allocate HAL Memory-TX-AMSDU Failed\n") );
            return RT_STATUS_FAILURE;
        }
        _GET_HAL_DATA(Adapter)->PTxDMAAMSDU88XX = ptx_dma_amsdu;
    }
#endif

	return RT_STATUS_SUCCESS;
}

#if IS_RTL88XX_MAC_V1_V2_V3
enum rt_status
InitHCIDMAReg88XX(
	IN  HAL_PADAPTER Adapter
)
{
	u4Byte                      value32 = 0;
	u4Byte                      RXBDReg;

#if (IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8192FE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE)
	if ( IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8814AE(Adapter) ||  IS_HARDWARE_TYPE_8822BE(Adapter) ||  IS_HARDWARE_TYPE_8821CE(Adapter) || IS_HARDWARE_TYPE_8192FE(Adapter) || IS_HARDWARE_TYPE_8822CE(Adapter) || IS_HARDWARE_TYPE_8812FE(Adapter)) {
		value32 = HAL_RTL_R32(REG_PCIE_CTRL);
		//Clear Bit
		value32 = value32 & ~((BIT_MASK_PCIE_MAX_RXDMA << BIT_SHIFT_PCIE_MAX_RXDMA) | (BIT_MASK_PCIE_MAX_TXDMA << BIT_SHIFT_PCIE_MAX_TXDMA));

#if RXBD_READY_CHECK_METHOD
		// RXTAG, Do Nothing, HW default value
		value32 |= BIT15;
#else
		// TOTALRXPKTSIZE
		value32 &= ~BIT15;
#endif  //RXBD_READY_CHECK_METHOD

		//Set Bit
		value32 |= BIT_PCIE_MAX_RXDMA(PCIE_RXDMA_BURST_SIZE) | BIT_PCIE_MAX_TXDMA(PCIE_TXDMA_BURST_SIZE) | BIT_STOP_BCNQ;

		HAL_RTL_W32(REG_PCIE_CTRL, value32);

#ifdef ENABLE_PCIE_PREFETCH
		// Do nothing
		// Enable TX/RX DMA pre-fetch
#else
		// Disable TX/RX DMA pre-fetch
		HAL_RTL_W8(REG_PCIE_CTRL2, HAL_RTL_R8(REG_PCIE_CTRL2) | BIT_DIS_RXDMA_PRE | BIT_DIS_TXDMA_PRE);
#endif
	}
#endif  //(IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822BE)

#if (IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM || IS_EXIST_RTL8197GEM)
	if ( IS_HARDWARE_TYPE_8197F(Adapter)|| IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter)) {
		value32 = HAL_RTL_R32(REG_HCI_CTRL);
		//Clear Bit
		value32 = value32 & ~((BIT_MASK_HCI_MAX_RXDMA << BIT_SHIFT_HCI_MAX_RXDMA) | 
		(BIT_MASK_HCI_MAX_TXDMA << BIT_SHIFT_HCI_MAX_TXDMA));

#if RXBD_READY_CHECK_METHOD
		// RXTAG, Do Nothing, HW default value
		value32 |= BIT_RX_CLOSE_EN;
#else
		// TOTALRXPKTSIZE
		value32 &= ~BIT_RX_CLOSE_EN;
#endif  //RXBD_READY_CHECK_METHOD

		//Set Bit
		value32 |= BIT_HCI_MAX_RXDMA(PCIE_RXDMA_BURST_SIZE) | 
		BIT_HCI_MAX_TXDMA(PCIE_TXDMA_BURST_SIZE) | BIT_STOP_BCNQ;

		HAL_RTL_W32(REG_HCI_CTRL, value32);

#ifdef ENABLE_PCIE_PREFETCH
		// Do nothing
		// Enable TX/RX DMA pre-fetch
#else
		// Disable TX/RX DMA pre-fetch
		HAL_RTL_W8(REG_HCI_CTRL2, HAL_RTL_R8(REG_HCI_CTRL2) | BIT_DIS_RXDMA_PRE | BIT_DIS_TXDMA_PRE);
#endif
	}
#endif  //(IS_EXIST_RTL8197FEM)  || IS_EXIST_RTL8198FEM || IS_EXIST_RTL8197GEM)



#if IS_EXIST_RTL8881AEM
	if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
		value32 = HAL_RTL_R32(REG_LX_CTRL1);
		//Clear Bit
		value32 = value32 & ~((BIT_MASK_MAX_RXDMA << BIT_SHIFT_MAX_RXDMA) | (BIT_MASK_MAX_TXDMA << BIT_SHIFT_MAX_TXDMA));
#if RXBD_READY_CHECK_METHOD
		// RXTAG, Do Nothing, HW default value
		value32 |= BIT_RX_CLOSE_EN;
#else
		// TOTALRXPKTSIZE
		value32 &= ~BIT_RX_CLOSE_EN;
#endif  //RXBD_READY_CHECK_METHOD
		//Set Bit
		value32 |= BIT_MAX_RXDMA(LBUS_RXDMA_BURST_SIZE) | BIT_MAX_TXDMA(LBUS_TXDMA_BURST_SIZE) | BIT_STOP_BCNQ;

		HAL_RTL_W32(REG_LX_CTRL1, value32);
	}
#endif  //IS_EXIST_RTL8881AEM

	//3 ===Set TXBD Mode and Number===
	HAL_RTL_W16(REG_MGQ_TXBD_NUM, BIT_MGQ_DESC_MODE(TX_MGQ_TXBD_MODE_SEL) |
				BIT_MGQ_DESC_NUM(TX_MGQ_TXBD_NUM));
	HAL_RTL_W16(REG_VOQ_TXBD_NUM, BIT_VOQ_DESC_MODE(TX_VOQ_TXBD_MODE_SEL) |
				BIT_VOQ_DESC_NUM(TX_VOQ_TXBD_NUM));
	HAL_RTL_W16(REG_VIQ_TXBD_NUM, BIT_VIQ_DESC_MODE(TX_VIQ_TXBD_MODE_SEL) |
				BIT_VIQ_DESC_NUM(TX_VIQ_TXBD_NUM));
	HAL_RTL_W16(REG_BEQ_TXBD_NUM, BIT_BEQ_DESC_MODE(TX_BEQ_TXBD_MODE_SEL) |
				BIT_BEQ_DESC_NUM(TX_BEQ_TXBD_NUM));
	HAL_RTL_W16(REG_BKQ_TXBD_NUM, BIT_BKQ_DESC_MODE(TX_BKQ_TXBD_MODE_SEL) |
				BIT_BKQ_DESC_NUM(TX_BKQ_TXBD_NUM));

	HAL_RTL_W16(REG_HI0Q_TXBD_NUM, BIT_HI0Q_DESC_MODE(TX_HI0Q_TXBD_MODE_SEL) |
				BIT_HI0Q_DESC_NUM(TX_HI0Q_TXBD_NUM));
	HAL_RTL_W16(REG_HI1Q_TXBD_NUM, BIT_HI1Q_DESC_MODE(TX_HI1Q_TXBD_MODE_SEL) |
				BIT_HI1Q_DESC_NUM(TX_HI1Q_TXBD_NUM));
	HAL_RTL_W16(REG_HI2Q_TXBD_NUM, BIT_HI2Q_DESC_MODE(TX_HI2Q_TXBD_MODE_SEL) |
				BIT_HI2Q_DESC_NUM(TX_HI2Q_TXBD_NUM));
	HAL_RTL_W16(REG_HI3Q_TXBD_NUM, BIT_HI3Q_DESC_MODE(TX_HI3Q_TXBD_MODE_SEL) |
				BIT_HI3Q_DESC_NUM(TX_HI3Q_TXBD_NUM));
	HAL_RTL_W16(REG_HI4Q_TXBD_NUM, BIT_HI4Q_DESC_MODE(TX_HI4Q_TXBD_MODE_SEL) |
				BIT_HI4Q_DESC_NUM(TX_HI4Q_TXBD_NUM));
	HAL_RTL_W16(REG_HI5Q_TXBD_NUM, BIT_HI5Q_DESC_MODE(TX_HI5Q_TXBD_MODE_SEL) |
				BIT_HI5Q_DESC_NUM(TX_HI5Q_TXBD_NUM));
	HAL_RTL_W16(REG_HI6Q_TXBD_NUM, BIT_HI6Q_DESC_MODE(TX_HI6Q_TXBD_MODE_SEL) |
				BIT_HI6Q_DESC_NUM(TX_HI6Q_TXBD_NUM));
	HAL_RTL_W16(REG_HI7Q_TXBD_NUM, BIT_HI7Q_DESC_MODE(TX_HI7Q_TXBD_MODE_SEL) |
				BIT_HI7Q_DESC_NUM(TX_HI7Q_TXBD_NUM));
#if IS_EXIST_RTL8198FEM 
    if ( IS_HARDWARE_TYPE_8198F(Adapter)){
    HAL_RTL_W16(REG_HI8Q_TXBD_NUM, BIT_HI8Q_DESC_MODE(TX_HI8Q_TXBD_MODE_SEL) |
                BIT_HI8Q_DESC_NUM(TX_HI8Q_TXBD_NUM));
    HAL_RTL_W16(REG_HI9Q_TXBD_NUM, BIT_HI9Q_DESC_MODE(TX_HI9Q_TXBD_MODE_SEL) |
                BIT_HI9Q_DESC_NUM(TX_HI9Q_TXBD_NUM));
    HAL_RTL_W16(REG_HI10Q_TXBD_NUM, BIT_HI10Q_DESC_MODE(TX_HI10Q_TXBD_MODE_SEL) |
                BIT_HI10Q_DESC_NUM(TX_HI10Q_TXBD_NUM));
    HAL_RTL_W16(REG_HI11Q_TXBD_NUM, BIT_HI11Q_DESC_MODE(TX_HI11Q_TXBD_MODE_SEL) |
                BIT_HI11Q_DESC_NUM(TX_HI11Q_TXBD_NUM));
    HAL_RTL_W16(REG_HI12Q_TXBD_NUM, BIT_HI12Q_DESC_MODE(TX_HI12Q_TXBD_MODE_SEL) |
                BIT_HI12Q_DESC_NUM(TX_HI12Q_TXBD_NUM));
    HAL_RTL_W16(REG_HI13Q_TXBD_NUM, BIT_HI13Q_DESC_MODE(TX_HI13Q_TXBD_MODE_SEL) |
                BIT_HI13Q_DESC_NUM(TX_HI13Q_TXBD_NUM));
    HAL_RTL_W16(REG_HI14Q_TXBD_NUM, BIT_HI14Q_DESC_MODE(TX_HI14Q_TXBD_MODE_SEL) |
                BIT_HI14Q_DESC_NUM(TX_HI14Q_TXBD_NUM));
    HAL_RTL_W16(REG_HI15Q_TXBD_NUM, BIT_HI15Q_DESC_MODE(TX_HI15Q_TXBD_MODE_SEL) |
                BIT_HI15Q_DESC_NUM(TX_HI15Q_TXBD_NUM));
    }
#endif //#if IS_EXIST_RTL8198FEM

#if (IS_EXIST_RTL8822BE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE|| IS_EXIST_RTL8197GEM)
    if ( IS_HARDWARE_TYPE_8822BE(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter)|| IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8822CE(Adapter)|| IS_HARDWARE_TYPE_8812FE(Adapter)|| IS_HARDWARE_TYPE_8197G(Adapter)) {
    HAL_RTL_W16(REG_H2CQ_TXBD_NUM, BIT_H2CQ_DESC_MODE(TX_H2CQ_TXBD_MODE_SEL) |
				BIT_H2CQ_DESC_NUM(TX_CMDQ_TXBD_NUM));
    }
#endif //IS_EXIST_RTL8822BE

	//3 ===Set Beacon Mode: 2, 4, or 8 segment each descriptor===
	RXBDReg = HAL_RTL_R16(REG_RX_RXBD_NUM);
	RXBDReg = (RXBDReg & ~(BIT_MASK_BCNQ_DESC_MODE << BIT_SHIFT_BCNQ_DESC_MODE)) |
			  BIT_BCNQ_DESC_MODE(TX_BCNQ_TXBD_MODE_SEL);
	HAL_RTL_W16(REG_RX_RXBD_NUM, RXBDReg);

	//3 ===Set RXBD Number===
	RXBDReg = (RXBDReg & ~BIT_MASK_RXQ_DESC_NUM) | BIT_RXQ_DESC_NUM(RX_Q_RXBD_NUM_IF(Adapter));
	HAL_RTL_W16(REG_RX_RXBD_NUM, RXBDReg);

	//3 ===Set 32Bit / 64 Bit System===
	RXBDReg = (RXBDReg & ~BIT_SYS_32_64) | (TXBD_SEG_32_64_SEL << 15);
	HAL_RTL_W16(REG_RX_RXBD_NUM, RXBDReg);

#if IS_EXIST_RTL8881AEM
	if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
		value32 = HAL_RTL_R32(REG_LX_CTRL1);
		value32 = value32 & (~0x3fff);
		HAL_RTL_W32(REG_LX_CTRL1, value32);
	}
#endif

#if (IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8192FE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE)
	if ( IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8814AE(Adapter) || IS_HARDWARE_TYPE_8822BE(Adapter) || IS_HARDWARE_TYPE_8821CE(Adapter) || IS_HARDWARE_TYPE_8192FE(Adapter) || IS_HARDWARE_TYPE_8822CE(Adapter)|| IS_HARDWARE_TYPE_8812FE(Adapter)) {
		value32 = HAL_RTL_R32(REG_PCIE_CTRL);
		value32 = value32 & (~0x3fff);
		HAL_RTL_W32(REG_PCIE_CTRL, value32);
	}
#endif //#if (IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8822BE)


#if (IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM || IS_EXIST_RTL8197GEM)
    if (IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter)) {
        value32 = HAL_RTL_R32(REG_HCI_CTRL);
        value32 = value32 & (~0x3fff);
        HAL_RTL_W32(REG_HCI_CTRL, value32);
    }
#endif //#if (IS_EXIST_RTL8197FEM || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8197GEM)

	return RT_STATUS_SUCCESS;
}
#endif //#if IS_RTL88XX_MAC_V1_V2_V3

#if IS_RTL88XX_MAC_V4

enum rt_status
InitHCIDMAReg88XX_V1(
	IN  HAL_PADAPTER Adapter
)
{
#if defined(CONFIG_RTL_OFFLOAD_DRIVER)
	if (GET_CHIP_VER(Adapter) == VERSION_8814B) {
		
	u4Byte						value32;
	u4Byte						RXBDReg;


#if defined(CONFIG_PE_ENABLE)
#else
	// Set RXBD number
	RXBDReg = HAL_RTL_R32(REG_P0MGQ_RXQ_TXRXBD_NUM);
	RXBDReg = (RXBDReg & ~(BIT_MASK_P0RXQ_DESC_NUM << BIT_SHIFT_P0RXQ_DESC_NUM)) | BIT_P0RXQ_DESC_NUM(_GET_HAL_DATA(Adapter)->rxBDNum);
	RXBDReg = (RXBDReg & ~BIT_SYS_32_64_V1) | (TXBD_SEG_32_64_SEL << 31);
	HAL_RTL_W32(REG_P0MGQ_RXQ_TXRXBD_NUM, RXBDReg);
#endif

	/* NOTE: for 8814b, move TXBD reg. init to PrepareH2D_TXBD88XX */
	host_hw_ring_dma_init(Adapter);

    {
    	value32 = HAL_RTL_R32(REG_ACH_CTRL);
    	value32 = value32 & (~0xF15FFFD);
    	HAL_RTL_W32(REG_ACH_CTRL, value32);
    }

	value32 = HAL_RTL_R32(REG_HIQ_CTRL);
	value32 = value32 & (~0xFFFF);
	HAL_RTL_W32(REG_HIQ_CTRL, value32);

	return RT_STATUS_SUCCESS;
	}
#endif
    u4Byte                      value32 = 0;
	u4Byte                      RXBDReg;    
#if (IS_EXIST_RTL8814BE)
    u4Byte TXBD_NUM[HCI_TX_DMA_QUEUE_MAX_NUM_V2] =
    {
        TX_MGQ_TXBD_NUM_8814B,   TX_BKQ_TXBD_NUM_8814B,   TX_BEQ_TXBD_NUM_8814B,   TX_VIQ_TXBD_NUM_8814B, 
        TX_VOQ_TXBD_NUM_8814B,   TX_ACH4_TXBD_NUM_8814B,    TX_ACH5_TXBD_NUM_8814B, TX_ACH6_TXBD_NUM_8814B,
        TX_ACH7_TXBD_NUM_8814B,   TX_ACH8_TXBD_NUM_8814B,   TX_ACH9_TXBD_NUM_8814B,   TX_ACH10_TXBD_NUM_8814B,
        TX_ACH11_TXBD_NUM_8814B,  TX_ACH12_TXBD_NUM_8814B,  TX_ACH13_TXBD_NUM_8814B,  TX_HI0Q_TXBD_NUM_8814B,  
        TX_HI1Q_TXBD_NUM_8814B,  TX_HI2Q_TXBD_NUM_8814B,    TX_HI3Q_TXBD_NUM_8814B,   TX_HI4Q_TXBD_NUM_8814B,  
        TX_HI5Q_TXBD_NUM_8814B,  TX_HI6Q_TXBD_NUM_8814B,    TX_HI7Q_TXBD_NUM_8814B,  TX_HI8Q_TXBD_NUM_8814B,  
        TX_HI9Q_TXBD_NUM_8814B,  TX_HI10Q_TXBD_NUM_8814B, TX_HI11Q_TXBD_NUM_8814B, TX_HI12Q_TXBD_NUM_8814B, 
        TX_HI13Q_TXBD_NUM_8814B, TX_HI14Q_TXBD_NUM_8814B, TX_HI15Q_TXBD_NUM_8814B, TX_FWCMDQ_TXBD_NUM_8814B,
        TX_CMDQ_TXBD_NUM_8814B,  TX_BCNQ_TXBD_NUM_V1
    };
    u4Byte RXBD_NUM = RX_Q_RXBD_NUM_8814B;
#endif  //IS_EXIST_RTL8814BE 

        value32 = HAL_RTL_R32(REG_PCIE_CTRL);
		//Clear Bit
		value32 = value32 & ~((BIT_MASK_PCIE_MAX_RXDMA << BIT_SHIFT_PCIE_MAX_RXDMA) | 
		(BIT_MASK_PCIE_MAX_TXDMA << BIT_SHIFT_PCIE_MAX_TXDMA));

		//Set Bit
		value32 |= BIT_PCIE_MAX_RXDMA(PCIE_RXDMA_BURST_SIZE) | BIT_PCIE_MAX_TXDMA(PCIE_TXDMA_BURST_SIZE);

		HAL_RTL_W32(REG_PCIE_CTRL, value32);

        value32 = HAL_RTL_R32(REG_ACH_CTRL);
        
        value32 |= BIT_STOP_P0BCNQ;
        
#if RXBD_READY_CHECK_METHOD
        // RXTAG, Do Nothing, HW default value
        value32 |= BIT_RX_CLOSE_EN_V1;
#else
        // TOTALRXPKTSIZE
        value32 &= ~BIT_RX_CLOSE_EN_V1;
#endif  //RXBD_READY_CHECK_METHOD

        HAL_RTL_W32(REG_ACH_CTRL, value32);

#ifdef ENABLE_PCIE_PREFETCH
		// Do nothing
		// Enable TX/RX DMA pre-fetch
#else
		// Disable TX/RX DMA pre-fetch
		HAL_RTL_W32(REG_DBGSEL_PCIE_HRPWM1_HCPWM1_V1, HAL_RTL_R32(REG_DBGSEL_PCIE_HRPWM1_HCPWM1_V1) | BIT_DIS_RXDMA_PRE_V1 | BIT_DIS_TXDMA_PRE_V1);
#endif


        //3 ===Set TXBD Mode and Number===
        HAL_RTL_W16(REG_P0MGQ_RXQ_TXRXBD_NUM, BIT_P0MGQ_DESC_MODE(TX_MGQ_TXBD_MODE_SEL) |
                    BIT_P0MGQ_DESC_NUM(TXBD_NUM[0]));

        HAL_RTL_W32(REG_ACH0_ACH1_TXBD_NUM, BIT_ACH0_DESC_MODE(TX_VOQ_TXBD_MODE_SEL) |
                    BIT_ACH0_DESC_NUM(TXBD_NUM[4]) | BIT_ACH1_DESC_MODE_V1(TX_VIQ_TXBD_MODE_SEL) |
                    BIT_ACH1_DESC_NUM_V1(TXBD_NUM[3]));

        HAL_RTL_W32(REG_ACH2_ACH3_TXBD_NUM, BIT_ACH2_DESC_MODE(TX_BEQ_TXBD_MODE_SEL) | 
                    BIT_ACH2_DESC_NUM(TXBD_NUM[2]) | BIT_ACH3_DESC_MODE_V1(TX_BKQ_TXBD_MODE_SEL) |
                    BIT_ACH3_DESC_NUM_V1(TXBD_NUM[1]));
        //ch4~13
        HAL_RTL_W32(REG_ACH4_ACH5_TXBD_NUM, BIT_ACH4_DESC_MODE(TX_ACH4_TXBD_MODE_SEL) | 
                    BIT_ACH4_DESC_NUM(TXBD_NUM[5]) | BIT_ACH5_DESC_MODE(TX_ACH5_TXBD_MODE_SEL) |
                    BIT_ACH5_DESC_NUM(TXBD_NUM[6]));

        HAL_RTL_W32(REG_ACH6_ACH7_TXBD_NUM, BIT_ACH6_DESC_MODE(TX_ACH6_TXBD_MODE_SEL) | 
                    BIT_ACH6_DESC_NUM(TXBD_NUM[7]) | BIT_ACH7_DESC_MODE(TX_ACH7_TXBD_MODE_SEL) |
                    BIT_ACH7_DESC_NUM(TXBD_NUM[8]));
        
        HAL_RTL_W32(REG_ACH8_ACH9_TXBD_NUM, BIT_ACH8_DESC_MODE(TX_ACH8_TXBD_MODE_SEL) | 
                    BIT_ACH8_DESC_NUM(TXBD_NUM[9]) | BIT_ACH9_DESC_MODE(TX_ACH9_TXBD_MODE_SEL) |
                    BIT_ACH9_DESC_NUM(TXBD_NUM[10]));
        
        HAL_RTL_W32(REG_ACH10_ACH11_TXBD_NUM, BIT_ACH10_DESC_MODE(TX_ACH10_TXBD_MODE_SEL) | 
                    BIT_ACH10_DESC_NUM(TXBD_NUM[11]) | BIT_ACH11_DESC_MODE(TX_ACH11_TXBD_MODE_SEL) |
                    BIT_ACH11_DESC_NUM(TXBD_NUM[12]));
        
        HAL_RTL_W32(REG_ACH12_ACH13_TXBD_NUM, BIT_ACH12_DESC_MODE(TX_ACH12_TXBD_MODE_SEL) | 
                    BIT_ACH12_DESC_NUM(TXBD_NUM[13]) | BIT_ACH13_DESC_MODE(TX_ACH13_TXBD_MODE_SEL) |
                    BIT_ACH13_DESC_NUM(TXBD_NUM[14]));
        
     
        HAL_RTL_W32(REG_P0HI0Q_HI1Q_TXBD_NUM, BIT_P0HI0Q_DESC_MODE(TX_HI0Q_TXBD_MODE_SEL) |
                    BIT_P0HI0Q_DESC_NUM(TXBD_NUM[15]) | BIT_P0HI1Q_DESC_MODE(TX_HI1Q_TXBD_MODE_SEL) |
                    BIT_P0HI1Q_DESC_NUM(TXBD_NUM[16]));

        HAL_RTL_W32(REG_P0HI2Q_HI3Q_TXBD_NUM, BIT_P0HI2Q_DESC_MODE(TX_HI2Q_TXBD_MODE_SEL) |
                    BIT_P0HI2Q_DESC_NUM(TXBD_NUM[17]) | BIT_P0HI3Q_DESC_MODE(TX_HI3Q_TXBD_MODE_SEL) |
                    BIT_P0HI3Q_DESC_NUM(TXBD_NUM[18]));

        HAL_RTL_W32(REG_P0HI4Q_HI5Q_TXBD_NUM, BIT_P0HI4Q_DESC_MODE(TX_HI4Q_TXBD_MODE_SEL) |
                    BIT_P0HI4Q_DESC_NUM(TXBD_NUM[19]) | BIT_P0HI5Q_DESC_MODE(TX_HI5Q_TXBD_MODE_SEL) |
                    BIT_P0HI5Q_DESC_NUM(TXBD_NUM[20]));

        HAL_RTL_W32(REG_P0HI6Q_HI7Q_TXBD_NUM, BIT_P0HI6Q_DESC_MODE(TX_HI6Q_TXBD_MODE_SEL) |
                    BIT_P0HI6Q_DESC_NUM(TXBD_NUM[21]) | BIT_P0HI7Q_DESC_MODE(TX_HI7Q_TXBD_MODE_SEL) |
                    BIT_P0HI7Q_DESC_NUM(TXBD_NUM[22]));

        HAL_RTL_W32(REG_P0HI8Q_HI9Q_TXBD_NUM, BIT_P0HI8Q_DESC_MODE(TX_HI8Q_TXBD_MODE_SEL) |
                    BIT_P0HI8Q_DESC_NUM(TXBD_NUM[23]) | BIT_P0HI9Q_DESC_MODE(TX_HI9Q_TXBD_MODE_SEL) |
                    BIT_P0HI9Q_DESC_NUM(TXBD_NUM[24]));

        HAL_RTL_W32(REG_P0HI10Q_HI11Q_TXBD_NUM, BIT_P0HI10Q_DESC_MODE(TX_HI10Q_TXBD_MODE_SEL) |
                    BIT_P0HI10Q_DESC_NUM(TXBD_NUM[25]) | BIT_P0HI11Q_DESC_MODE(TX_HI11Q_TXBD_MODE_SEL) |
                    BIT_P0HI11Q_DESC_NUM(TXBD_NUM[26]));
        
        HAL_RTL_W32(REG_P0HI12Q_HI13Q_TXBD_NUM, BIT_P0HI12Q_DESC_MODE(TX_HI12Q_TXBD_MODE_SEL) |
                    BIT_P0HI12Q_DESC_NUM(TXBD_NUM[27]) | BIT_P0HI13Q_DESC_MODE(TX_HI13Q_TXBD_MODE_SEL) |
                    BIT_P0HI13Q_DESC_NUM(TXBD_NUM[28]));

        HAL_RTL_W32(REG_P0HI14Q_HI15Q_TXBD_NUM, BIT_P0HI14Q_DESC_MODE(TX_HI14Q_TXBD_MODE_SEL) |
                    BIT_P0HI14Q_DESC_NUM(TXBD_NUM[29]) | BIT_P0HI15Q_DESC_MODE(TX_HI15Q_TXBD_MODE_SEL) |
                    BIT_P0HI15Q_DESC_NUM(TXBD_NUM[30]));
#if 0 //port1-port4 use, no define now, yllin8814B       
        HAL_RTL_W32(REG_P0HI16Q_HI17Q_TXBD_NUM, BIT_P0HI16Q_DESC_MODE(TX_HI16Q_TXBD_MODE_SEL) |
                    BIT_P0HI16Q_DESC_NUM(TX_HI16Q_TXBD_NUM) | BIT_P0HI17Q_DESC_MODE(TX_HI17Q_TXBD_MODE_SEL) |
                    BIT_P0HI17Q_DESC_NUM(TX_HI17Q_TXBD_NUM));
        
        HAL_RTL_W32(REG_P0HI18Q_HI19Q_TXBD_NUM, BIT_P0HI18Q_DESC_MODE(TX_HI18Q_TXBD_MODE_SEL) |
                    BIT_P0HI18Q_DESC_NUM(TX_HI18Q_TXBD_NUM) | BIT_P0HI19Q_DESC_MODE(TX_HI19Q_TXBD_MODE_SEL) |
                    BIT_P0HI19Q_DESC_NUM(TX_HI19Q_TXBD_NUM));
        
#endif        
        HAL_RTL_W16(REG_H2CQ_TXBD_NUM, BIT_H2CQ_DESC_MODE(TX_H2CQ_TXBD_MODE_SEL) |
                        BIT_H2CQ_DESC_NUM(TXBD_NUM[32]));
#ifdef WLAN_SUPPORT_FW_CMD
        HAL_RTL_W16(REG_FWCMDQ_TXBD_NUM, BIT_FWCMDQ_DESC_MODE(TX_FWCMD_MODE_SEL) |
                        BIT_FWCMDQ_DESC_NUM(TXBD_NUM[31]));
#endif
        
        //3 ===Set Beacon Mode: 2, 4, or 8 segment each descriptor===
        RXBDReg = HAL_RTL_R32(REG_P0MGQ_RXQ_TXRXBD_NUM);
        RXBDReg = (RXBDReg & ~(BIT_MASK_P0BCNQ_DESC_MODE << BIT_SHIFT_P0BCNQ_DESC_MODE)) |
                  BIT_P0BCNQ_DESC_MODE(TX_BCNQ_TXBD_MODE_SEL);

        //3 ===Set RXBD Number===
        RXBDReg = (RXBDReg & ~(BIT_MASK_P0RXQ_DESC_NUM << BIT_SHIFT_P0RXQ_DESC_NUM)) | BIT_P0RXQ_DESC_NUM(RXBD_NUM);
        //3 ===Set 32Bit / 64 Bit System===
        RXBDReg = (RXBDReg & ~BIT_SYS_32_64_V1) | (TXBD_SEG_32_64_SEL << 31);

        HAL_RTL_W32(REG_P0MGQ_RXQ_TXRXBD_NUM, RXBDReg);

        //release stopDMA, only no setting for bcnq(include  multi-port), follow old flow //yllin8814B
        value32 = HAL_RTL_R32(REG_HIQ_CTRL);
        value32 = value32 & (~0xffff);
        HAL_RTL_W32(REG_HIQ_CTRL, value32);
        
        value32 = HAL_RTL_R32(REG_ACH_CTRL);
        value32 = value32 & (~0xF11FFFD);
        HAL_RTL_W32(REG_ACH_CTRL, value32);
      

    return RT_STATUS_SUCCESS;
}
#endif //#if IS_RTL88XX_MAC_V4

VOID
StopHCIDMASW88XX(
	IN  HAL_PADAPTER Adapter
)
{
#ifdef CONFIG_NET_PCI
	if (HAL_IS_PCIBIOS_TYPE(Adapter)) {
			if (_GET_HAL_DATA(Adapter)->alloc_dma_buf)
				os_dma_free(Adapter, _GET_HAL_DATA(Adapter)->ring_buf_len,
						(void *)_GET_HAL_DATA(Adapter)->alloc_dma_buf,
						(dma_addr_t)_GET_HAL_DATA(Adapter)->ring_dma_addr);

#ifdef WLAN_HAL_TX_AMSDU
		if (_GET_HAL_DATA(Adapter)->desc_dma_buf_amsdu)
			os_dma_free(Adapter, _GET_HAL_DATA(Adapter)->desc_dma_buf_len_amsdu,
					(void *)_GET_HAL_DATA(Adapter)->desc_dma_buf_amsdu,
					(dma_addr_t)_GET_HAL_DATA(Adapter)->desc_dma_buf_addr_amsdu[0]);
#endif
	}
#endif // CONFIG_NET_PCI

	//Free TRX DMA Manager Memory
	{
		if (_GET_HAL_DATA(Adapter)->PTxDMA88XX)
			HAL_free(_GET_HAL_DATA(Adapter)->PTxDMA88XX);

		if (_GET_HAL_DATA(Adapter)->PRxDMA88XX)
			HAL_free(_GET_HAL_DATA(Adapter)->PRxDMA88XX);
	}

#if CFG_HAL_TX_AMSDU
    if ( IS_SUPPORT_TX_AMSDU(Adapter) ) {
		if (_GET_HAL_DATA(Adapter)->PTxDMAAMSDU88XX)
        	HAL_free(_GET_HAL_DATA(Adapter)->PTxDMAAMSDU88XX);
	}
#endif
}


VOID
StopHCIDMAHW88XX(
	IN  HAL_PADAPTER Adapter
)
{
	u4Byte     value32 ;

	//TRX DMA Stop
#if IS_EXIST_RTL8881AEM
	if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
		value32 = HAL_RTL_R32(REG_LX_CTRL1);
		value32 = value32 | (0x7FFF);
		HAL_RTL_W32(REG_LX_CTRL1, value32);
	}
#endif

#if (IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE)
	if ( IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8192FE(Adapter) || IS_HARDWARE_TYPE_8814AE(Adapter) || IS_HARDWARE_TYPE_8822BE(Adapter) || IS_HARDWARE_TYPE_8821CE(Adapter) || IS_HARDWARE_TYPE_8822CE(Adapter)|| IS_HARDWARE_TYPE_8812FE(Adapter)) {
		value32 = HAL_RTL_R32(REG_PCIE_CTRL);
		value32 = value32 | (0x7FFF);
		HAL_RTL_W32(REG_PCIE_CTRL, value32);
	}
#endif

#if (IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM || IS_EXIST_RTL8197GEM)
    if (IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter)) {
        value32 = HAL_RTL_R32(REG_HCI_CTRL);
        value32 = value32 | (0x7FFF);
        HAL_RTL_W32(REG_HCI_CTRL, value32);
    }
#endif //(IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM || IS_EXIST_RTL8197GEM)

#if IS_EXIST_RTL8814BE
    if (IS_HARDWARE_TYPE_8814B(Adapter)) {
        value32 = HAL_RTL_R32(REG_PCIE_CTRL); //stop P0 multiple-port BCNQ DMA
        value32 = value32 | (0x55);
        HAL_RTL_W32(REG_PCIE_CTRL, value32);
        
        value32 = HAL_RTL_R32(REG_ACH_CTRL); //stop ac channel,P0 BCN DMA
        value32 = value32 | (0xF15FFFD);
        HAL_RTL_W32(REG_ACH_CTRL, value32);

        value32 = HAL_RTL_R32(REG_HIQ_CTRL); //stop HIQ DMA
        value32 = value32 | (0xFFFF);
        HAL_RTL_W32(REG_HIQ_CTRL, value32);
    }
#endif

	//Sugested by DD-TimChen
	// Reason: make sure thar TRX DMA operation is done (To avoid transaction error in LBUS)
	HAL_delay_ms(5);

}
#endif // (HAL_DEV_BUS_TYPE & (HAL_RT_EMBEDDED_INTERFACE | HAL_RT_PCI_INTERFACE))


#ifdef MULTI_MAC_CLONE
VOID
McloneSetMBSSID88XX(
	IN  HAL_PADAPTER Adapter,
	IN	pu1Byte 	 macAddr,
	IN	int          entIdx
)
{
	u4Byte      camData[2], camIdx;
    u4Byte      mask_MBIDCAM_addr;
    u4Byte      shift_MBIDCAM_addr;
    u4Byte      bit_MBIDCAM_valid;

	camIdx = MAC_CLONE_MBIDCAM_START + entIdx;// for multiple repeater, camIdx 0~3 are uesed for repeaters

	panic_printk("[%s:%d]entIdx = %d,camIdx = %d %02x:%02x:%02x:%02x:%02x:%02x\n", __func__, __LINE__,
				 entIdx,camIdx, macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);

#if IS_RTL8197G_SERIES
    if( IS_HARDWARE_TYPE_8197G(Adapter) ) {
		mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR_V1;
		shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR_V1;
		bit_MBIDCAM_valid = BIT_MBIDCAM_VALID_V1;	
    }
#endif  // #if IS_RTL8197G_SERIES    

#if IS_RTL8198F_SERIES
    if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
		mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR_V2;
		shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR_V2;
		bit_MBIDCAM_valid = BIT_MBIDCAM_VALID_V1;
    }
#endif  //#if IS_RTL8198F_SERIES
#if IS_RTL8197F_SERIES
    if( IS_HARDWARE_TYPE_8197F(Adapter) ) {
		mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR_V1;
		shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR_V1;
		bit_MBIDCAM_valid = BIT_MBIDCAM_VALID;	
    }
#endif  // #if IS_RTL8197F_SERIES    
#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
    if( IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8881A(Adapter) ||IS_HARDWARE_TYPE_8814A(Adapter) ||IS_HARDWARE_TYPE_8822B(Adapter) 
		|| IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)) {
		mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR;
		shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR;
		bit_MBIDCAM_valid = BIT_MBIDCAM_VALID;
    }
#endif //#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES)


	camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN | bit_MBIDCAM_valid |
				 (camIdx & mask_MBIDCAM_addr) << shift_MBIDCAM_addr |
				 (macAddr[5] << 8) | macAddr[4];  
	camData[1] = (macAddr[3] << 24) | (macAddr[2] << 16) | (macAddr[1] << 8) | macAddr[0];

	HAL_RTL_W32(REG_MBIDCAMCFG_1, camData[1]);
	HAL_RTL_W32(REG_MBIDCAMCFG_2, camData[0]);
	HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) | BIT_ENMBID);	// MBSSID enable
}

#if IS_RTL88XX_MAC_V4
VOID
McloneSetMBSSID88XX_V1(
	IN  HAL_PADAPTER Adapter,
	IN	pu1Byte 	 macAddr,
	IN	int          entIdx 
)
{
	u4Byte      camData[8], camIdx, write_cmd;


        //3 may need modify due to address cam usage scenario change
        camIdx = MAC_CLONE_MBIDCAM_START + entIdx*8;// for multiple repeater, camIdx 0~3 are uesed for repeaters
        camData[0] =  BIT0 | (0x3 << 16); //valid bit | netwoek type=AP, temp set to AP, need to check if only AP call this api
        camData[1] = (macAddr[3] << 24) | (macAddr[2] << 16) | (macAddr[1] << 8) | macAddr[0];
        camData[2] = (macAddr[5] << 8) | macAddr[4];
        //only set self address for now
        write_cmd = ((HAL_RTL_R32(REG_ADDRCAM_CFG) & BIT_MASK_ADDRCAM_RANGE) | BIT_ADDRCAM_POLL | BIT__ADDRCAM_WT_EN & ~BIT_MASK__ADDRCAM_ADDR) | BIT__ADDRCAM_ADDR(camIdx);
        HAL_RTL_W32(REG_ADDRCAM_WRITE_CONTENT, camData[0]);
        HAL_RTL_W32(REG_ADDRCAM_CFG, write_cmd);
        write_cmd = (write_cmd & ~BIT_MASK__ADDRCAM_ADDR) | BIT__ADDRCAM_ADDR(camIdx+1);
        HAL_RTL_W32(REG_ADDRCAM_WRITE_CONTENT, camData[1]);
        HAL_RTL_W32(REG_ADDRCAM_CFG, write_cmd);
        write_cmd = (write_cmd & ~BIT_MASK__ADDRCAM_ADDR) | BIT__ADDRCAM_ADDR(camIdx+2);
        HAL_RTL_W32(REG_ADDRCAM_WRITE_CONTENT, camData[2]);
        HAL_RTL_W32(REG_ADDRCAM_CFG, write_cmd);

        HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) | BIT_ENMBID);	// MBSSID enable


}
#endif //IS_RTL88XX_MAC_V4 

VOID
McloneStopMBSSID88XX(
	IN  HAL_PADAPTER Adapter,
	IN	int          entIdx
)
{
    s4Byte      i;
    u4Byte      camData[2], camIdx;
    u4Byte      mask_MBIDCAM_addr;
u4Byte      shift_MBIDCAM_addr;
    u4Byte      bit_MBIDCAM_valid;

	camData[1] = 0;

#if IS_RTL8197G_SERIES
        if( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR_V1;
            shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR_V1;
            bit_MBIDCAM_valid = BIT_MBIDCAM_VALID_V1;  
        }
#endif  // #if IS_RTL8197G_SERIES   

#if IS_RTL8198F_SERIES
    if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
		mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR_V2;
		shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR_V2;
		bit_MBIDCAM_valid = BIT_MBIDCAM_VALID_V1;
    }
#endif  //#if IS_RTL8198F_SERIES
#if IS_RTL8197F_SERIES
    if( IS_HARDWARE_TYPE_8197F(Adapter) ) {
		mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR_V1;
		shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR_V1;
		bit_MBIDCAM_valid = BIT_MBIDCAM_VALID;	
    }
#endif  // #if IS_RTL8197F_SERIES    
#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
    if( IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8881A(Adapter) ||IS_HARDWARE_TYPE_8814A(Adapter) ||IS_HARDWARE_TYPE_8822B(Adapter) 
		|| IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)) {
        mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR;
		shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR;
		bit_MBIDCAM_valid = BIT_MBIDCAM_VALID;
    }
#endif //#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES)  

    if (entIdx == -1) {
        // clear all the multi-STA CAM
        for (i = 0; i < Adapter->pshare->mclone_num_max; i++) {
            if (Adapter == Adapter->pshare->mclone_sta[i].priv) {
                camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN | ((MAC_CLONE_MBIDCAM_START + i)&mask_MBIDCAM_addr) << BIT_SHIFT_MBIDCAM_ADDR;
                HAL_RTL_W32(REG_MBIDCAMCFG_1, camData[1]);
                HAL_RTL_W32(REG_MBIDCAMCFG_2, camData[0]);
                panic_printk("[%s:%d] entIdx=%d\n", __func__, __LINE__, i);
            }
        }
    } else {
        panic_printk("[%s:%d] entIdx=%d\n", __func__, __LINE__, entIdx);
        camIdx = MAC_CLONE_MBIDCAM_START + entIdx;
        camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN | (camIdx & mask_MBIDCAM_addr) << BIT_SHIFT_MBIDCAM_ADDR;
        HAL_RTL_W32(REG_MBIDCAMCFG_1, camData[1]);
        HAL_RTL_W32(REG_MBIDCAMCFG_2, camData[0]);
    }
}
#if IS_RTL88XX_MAC_V4

VOID
McloneStopMBSSID88XX_V1(
	IN  HAL_PADAPTER Adapter,
	IN	int          entIdx
)
{
    u4Byte      camData[2], camIdx, write_cmd;
    s4Byte      i,j;

        camData[0] = 0;
        
        if (entIdx == -1) {
            // clear all the multi-STA CAM
            for (i = 0; i < MAX_MAC_CLONE_NUM; i++) {
                if (Adapter == Adapter->pshare->mclone_sta[i].priv) {           
                    for(j=0;j<8;j++){
                        camIdx = MAC_CLONE_MBIDCAM_START+i*8+j;
                        write_cmd = ((HAL_RTL_R32(REG_ADDRCAM_CFG) & BIT_MASK_ADDRCAM_RANGE) | BIT_ADDRCAM_POLL | BIT__ADDRCAM_WT_EN & ~BIT_MASK__ADDRCAM_ADDR) | BIT__ADDRCAM_ADDR(camIdx);
                        HAL_RTL_W32(REG_ADDRCAM_WRITE_CONTENT, camData[0]);
                        HAL_RTL_W32(REG_ADDRCAM_CFG, write_cmd);
                        
                    }
                    panic_printk("[%s:%d] entIdx=%d\n", __func__, __LINE__, i);
                }
            }
        } else {
            panic_printk("[%s:%d] entIdx=%d\n", __func__, __LINE__, entIdx);
            camIdx = MAC_CLONE_MBIDCAM_START + entIdx*8;
            write_cmd = ((HAL_RTL_R32(REG_ADDRCAM_CFG) & BIT_MASK_ADDRCAM_RANGE) | BIT_ADDRCAM_POLL | BIT__ADDRCAM_WT_EN & ~BIT_MASK__ADDRCAM_ADDR) | BIT__ADDRCAM_ADDR(camIdx);
            HAL_RTL_W32(REG_ADDRCAM_WRITE_CONTENT, camData[0]);
            HAL_RTL_W32(REG_ADDRCAM_CFG, write_cmd);
            
        }
  
}
#endif// IS_RTL88XX_MAC_V4   
#endif

#if CFG_HAL_SUPPORT_MBSSID

#if IS_RTL88XX_MAC_V1_V2
VOID SetDTIM(IN  HAL_PADAPTER Adapter)
{
		switch (HAL_VAR_VAP_INIT_SEQ) {
		case 1:
#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8197F_SERIES)
			if ( IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter)  ) {
				HAL_RTL_W16(REG_ATIMWND1, 0x3C);
			}
#endif
#if (IS_RTL8814A_SERIES)
			if ( IS_HARDWARE_TYPE_8814A(Adapter)) {
					HAL_RTL_W8(REG_ATIMWND1_V1, 0x3C);
			}
#endif
			HAL_RTL_W8(REG_DTIM_COUNTER_VAP1, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT1CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(1));
			break;
		case 2:
			HAL_RTL_W8(REG_ATIMWND2, 0x3C);
			HAL_RTL_W8(REG_DTIM_COUNTER_VAP2, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT2CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(2));
			break;
		case 3:
			HAL_RTL_W8(REG_ATIMWND3, 0x3C);
			HAL_RTL_W8(REG_DTIM_COUNTER_VAP3, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT3CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(3));
			break;
		case 4:
			HAL_RTL_W8(REG_ATIMWND4, 0x3C);
			HAL_RTL_W8(REG_DTIM_COUNTER_VAP4, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT4CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(4));
			break;
		case 5:
			HAL_RTL_W8(REG_ATIMWND5, 0x3C);
			HAL_RTL_W8(REG_DTIM_COUNTER_VAP5, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT5CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(5));
			break;
		case 6:
			HAL_RTL_W8(REG_ATIMWND6, 0x3C);
			HAL_RTL_W8(REG_DTIM_COUNTER_VAP6, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT6CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(6));
			break;
		case 7:
			HAL_RTL_W8(REG_ATIMWND7, 0x3C);
			HAL_RTL_W8(REG_DTIM_COUNTER_VAP7, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT7CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(7));
			break;
		default:
			RT_TRACE(COMP_INIT, DBG_WARNING, ("Invalid init swq=%d\n", HAL_VAR_VAP_INIT_SEQ));
			break;
		}
}
#endif //(IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES)

#if IS_RTL88XX_MAC_V3
VOID SetDTIM_V1(IN  HAL_PADAPTER Adapter)
{

    // TODO : MBSSID setting
    //Although 97G only has 8 MBSSID, REG config in the same way, so no need to refine this function, just don't config 8-15 MBSSID
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v3)
    {
        switch (HAL_VAR_VAP_INIT_SEQ) {
		case 1:
            HAL_RTL_W32(REG_MBID_BCNQ_EN, ((HAL_RTL_R32(REG_MBID_BCNQ_EN) & ~ BITS_ATIM_CFG_SEL) | BIT_ATIM_CFG_SEL(0)));
            HAL_RTL_W8(REG_ATIMWND_GROUP2, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNT_GROUP2, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT1CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(1));
			break;
		case 2:
            HAL_RTL_W32(REG_MBID_BCNQ_EN, ((HAL_RTL_R32(REG_MBID_BCNQ_EN) & ~ BITS_ATIM_CFG_SEL) | BIT_ATIM_CFG_SEL(0)));
            HAL_RTL_W8(REG_ATIMWND_GROUP3, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNT_GROUP3, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT2CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(2));
			break;
		case 3:
            HAL_RTL_W32(REG_MBID_BCNQ_EN, ((HAL_RTL_R32(REG_MBID_BCNQ_EN) & ~ BITS_ATIM_CFG_SEL) | BIT_ATIM_CFG_SEL(0)));
            HAL_RTL_W8(REG_ATIMWND_GROUP4, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNT_GROUP4, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT3CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(3));
			break;
		case 4:
            HAL_RTL_W32(REG_MBID_BCNQ_EN, ((HAL_RTL_R32(REG_MBID_BCNQ_EN) & ~ BITS_ATIM_CFG_SEL) | BIT_ATIM_CFG_SEL(1)));
            HAL_RTL_W8(REG_ATIMWND_GROUP1, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNT_GROUP1, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT4CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(4));
			break;
		case 5:
            HAL_RTL_W32(REG_MBID_BCNQ_EN, ((HAL_RTL_R32(REG_MBID_BCNQ_EN) & ~ BITS_ATIM_CFG_SEL) | BIT_ATIM_CFG_SEL(1)));
            HAL_RTL_W8(REG_ATIMWND_GROUP2, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNT_GROUP2, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT5CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(5));
			break;
		case 6:
            HAL_RTL_W32(REG_MBID_BCNQ_EN, ((HAL_RTL_R32(REG_MBID_BCNQ_EN) & ~ BITS_ATIM_CFG_SEL) | BIT_ATIM_CFG_SEL(1)));
            HAL_RTL_W8(REG_ATIMWND_GROUP3, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNT_GROUP3, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT6CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(6));
			break;
		case 7:
            HAL_RTL_W32(REG_MBID_BCNQ_EN, ((HAL_RTL_R32(REG_MBID_BCNQ_EN) & ~ BITS_ATIM_CFG_SEL) | BIT_ATIM_CFG_SEL(1)));
            HAL_RTL_W8(REG_ATIMWND_GROUP4, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNT_GROUP4, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT7CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(7));
			break;
        case 8:
            HAL_RTL_W32(REG_MBID_BCNQ_EN, ((HAL_RTL_R32(REG_MBID_BCNQ_EN) & ~ BITS_ATIM_CFG_SEL) | BIT_ATIM_CFG_SEL(2)));
            HAL_RTL_W8(REG_ATIMWND_GROUP1, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNT_GROUP1, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT8CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(8));
			break;
        case 9:
            HAL_RTL_W32(REG_MBID_BCNQ_EN, ((HAL_RTL_R32(REG_MBID_BCNQ_EN) & ~ BITS_ATIM_CFG_SEL) | BIT_ATIM_CFG_SEL(2)));
            HAL_RTL_W8(REG_ATIMWND_GROUP2, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNT_GROUP2, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT9CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(9));
			break;
        case 10:
            HAL_RTL_W32(REG_MBID_BCNQ_EN, ((HAL_RTL_R32(REG_MBID_BCNQ_EN) & ~ BITS_ATIM_CFG_SEL) | BIT_ATIM_CFG_SEL(2)));
            HAL_RTL_W8(REG_ATIMWND_GROUP3, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNT_GROUP3, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT10CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(10));
			break;
        case 11:
            HAL_RTL_W32(REG_MBID_BCNQ_EN, ((HAL_RTL_R32(REG_MBID_BCNQ_EN) & ~ BITS_ATIM_CFG_SEL) | BIT_ATIM_CFG_SEL(2)));
            HAL_RTL_W8(REG_ATIMWND_GROUP4, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNT_GROUP4, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT11CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(11));
			break;    
        case 12:
            HAL_RTL_W32(REG_MBID_BCNQ_EN, ((HAL_RTL_R32(REG_MBID_BCNQ_EN) & ~ BITS_ATIM_CFG_SEL) | BIT_ATIM_CFG_SEL(3)));
            HAL_RTL_W8(REG_ATIMWND_GROUP1, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNT_GROUP1, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT12CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(12));
			break;
        case 13:
            HAL_RTL_W32(REG_MBID_BCNQ_EN, ((HAL_RTL_R32(REG_MBID_BCNQ_EN) & ~ BITS_ATIM_CFG_SEL) | BIT_ATIM_CFG_SEL(3)));
            HAL_RTL_W8(REG_ATIMWND_GROUP2, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNT_GROUP2, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT13CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(13));
			break;
        case 14:
            HAL_RTL_W32(REG_MBID_BCNQ_EN, ((HAL_RTL_R32(REG_MBID_BCNQ_EN) & ~ BITS_ATIM_CFG_SEL) | BIT_ATIM_CFG_SEL(3)));
            HAL_RTL_W8(REG_ATIMWND_GROUP3, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNT_GROUP3, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT14CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(14));
			break;
        case 15:
            HAL_RTL_W32(REG_MBID_BCNQ_EN, ((HAL_RTL_R32(REG_MBID_BCNQ_EN) & ~ BITS_ATIM_CFG_SEL) | BIT_ATIM_CFG_SEL(3)));
            HAL_RTL_W8(REG_ATIMWND_GROUP4, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNT_GROUP4, HAL_VAR_DTIM_PERIOD - 1);
			HAL_RTL_W16(REG_WMAC_PKTCNT_CTRL, FUNCTRL_ADDR_CNT15CTRL);
			HAL_RTL_W8(REG_WMAC_PKTCNT_RWD, BIT_PKTCNT_CNTEN | BIT_PKTCNT_BSSIDMAP(15));
			break;     
		default:
			RT_TRACE(COMP_INIT, DBG_WARNING, ("Invalid init swq=%d\n", HAL_VAR_VAP_INIT_SEQ));
			break;
		}
    }
}
#endif //IS_RTL88XX_MAC_V3
    
#if IS_RTL88XX_MAC_V4
VOID SetDTIM_V2(IN  HAL_PADAPTER Adapter)
{
    // TODO : MBSSID setting
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)
    {
        switch (HAL_VAR_VAP_INIT_SEQ)
    {
		case 1:
            HAL_RTL_W8(REG_ATIM_DTIM_CTRL_SEL, (HAL_RTL_R8(REG_ATIM_DTIM_CTRL_SEL) &~BIT_MASK_ATIM_DTIM_SEL) | BIT_ATIM_DTIM_SEL(1));
            HAL_RTL_W8(REG_ATIMWND_CFG, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNTER_CFG, HAL_VAR_DTIM_PERIOD - 1);
			break;
		case 2:
            HAL_RTL_W8(REG_ATIM_DTIM_CTRL_SEL, (HAL_RTL_R8(REG_ATIM_DTIM_CTRL_SEL) &~BIT_MASK_ATIM_DTIM_SEL) | BIT_ATIM_DTIM_SEL(2));
            HAL_RTL_W8(REG_ATIMWND_CFG, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNTER_CFG, HAL_VAR_DTIM_PERIOD - 1);
			break;
		case 3:
            HAL_RTL_W8(REG_ATIM_DTIM_CTRL_SEL, (HAL_RTL_R8(REG_ATIM_DTIM_CTRL_SEL) &~BIT_MASK_ATIM_DTIM_SEL) | BIT_ATIM_DTIM_SEL(3));
            HAL_RTL_W8(REG_ATIMWND_CFG, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNTER_CFG, HAL_VAR_DTIM_PERIOD - 1);
			break;
		case 4:
            HAL_RTL_W8(REG_ATIM_DTIM_CTRL_SEL, (HAL_RTL_R8(REG_ATIM_DTIM_CTRL_SEL) &~BIT_MASK_ATIM_DTIM_SEL) | BIT_ATIM_DTIM_SEL(4));
            HAL_RTL_W8(REG_ATIMWND_CFG, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNTER_CFG, HAL_VAR_DTIM_PERIOD - 1);
			break;
		case 5:
            HAL_RTL_W8(REG_ATIM_DTIM_CTRL_SEL, (HAL_RTL_R8(REG_ATIM_DTIM_CTRL_SEL) &~BIT_MASK_ATIM_DTIM_SEL) | BIT_ATIM_DTIM_SEL(5));
            HAL_RTL_W8(REG_ATIMWND_CFG, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNTER_CFG, HAL_VAR_DTIM_PERIOD - 1);
			break;
		case 6:
            HAL_RTL_W8(REG_ATIM_DTIM_CTRL_SEL, (HAL_RTL_R8(REG_ATIM_DTIM_CTRL_SEL) &~BIT_MASK_ATIM_DTIM_SEL) | BIT_ATIM_DTIM_SEL(6));
            HAL_RTL_W8(REG_ATIMWND_CFG, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNTER_CFG, HAL_VAR_DTIM_PERIOD - 1);
			break;
		case 7:
            HAL_RTL_W8(REG_ATIM_DTIM_CTRL_SEL, (HAL_RTL_R8(REG_ATIM_DTIM_CTRL_SEL) &~BIT_MASK_ATIM_DTIM_SEL) | BIT_ATIM_DTIM_SEL(7));
            HAL_RTL_W8(REG_ATIMWND_CFG, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNTER_CFG, HAL_VAR_DTIM_PERIOD - 1);
			break;
        case 8:
            HAL_RTL_W8(REG_ATIM_DTIM_CTRL_SEL, (HAL_RTL_R8(REG_ATIM_DTIM_CTRL_SEL) &~BIT_MASK_ATIM_DTIM_SEL) | BIT_ATIM_DTIM_SEL(8));
            HAL_RTL_W8(REG_ATIMWND_CFG, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNTER_CFG, HAL_VAR_DTIM_PERIOD - 1);
			break;
        case 9:
            HAL_RTL_W8(REG_ATIM_DTIM_CTRL_SEL, (HAL_RTL_R8(REG_ATIM_DTIM_CTRL_SEL) &~BIT_MASK_ATIM_DTIM_SEL) | BIT_ATIM_DTIM_SEL(9));
            HAL_RTL_W8(REG_ATIMWND_CFG, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNTER_CFG, HAL_VAR_DTIM_PERIOD - 1);
			break;
        case 10:
            HAL_RTL_W8(REG_ATIM_DTIM_CTRL_SEL, (HAL_RTL_R8(REG_ATIM_DTIM_CTRL_SEL) &~BIT_MASK_ATIM_DTIM_SEL) | BIT_ATIM_DTIM_SEL(10));
            HAL_RTL_W8(REG_ATIMWND_CFG, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNTER_CFG, HAL_VAR_DTIM_PERIOD - 1);
			break;
        case 11:
            HAL_RTL_W8(REG_ATIM_DTIM_CTRL_SEL, (HAL_RTL_R8(REG_ATIM_DTIM_CTRL_SEL) &~BIT_MASK_ATIM_DTIM_SEL) | BIT_ATIM_DTIM_SEL(11));
            HAL_RTL_W8(REG_ATIMWND_CFG, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNTER_CFG, HAL_VAR_DTIM_PERIOD - 1);
			break;    
        case 12:
            HAL_RTL_W8(REG_ATIM_DTIM_CTRL_SEL, (HAL_RTL_R8(REG_ATIM_DTIM_CTRL_SEL) &~BIT_MASK_ATIM_DTIM_SEL) | BIT_ATIM_DTIM_SEL(12));
            HAL_RTL_W8(REG_ATIMWND_CFG, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNTER_CFG, HAL_VAR_DTIM_PERIOD - 1);
			break;
        case 13:
            HAL_RTL_W8(REG_ATIM_DTIM_CTRL_SEL, (HAL_RTL_R8(REG_ATIM_DTIM_CTRL_SEL) &~BIT_MASK_ATIM_DTIM_SEL) | BIT_ATIM_DTIM_SEL(13));
            HAL_RTL_W8(REG_ATIMWND_CFG, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNTER_CFG, HAL_VAR_DTIM_PERIOD - 1);
			break;
        case 14:
            HAL_RTL_W8(REG_ATIM_DTIM_CTRL_SEL, (HAL_RTL_R8(REG_ATIM_DTIM_CTRL_SEL) &~BIT_MASK_ATIM_DTIM_SEL) | BIT_ATIM_DTIM_SEL(14));
            HAL_RTL_W8(REG_ATIMWND_CFG, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNTER_CFG, HAL_VAR_DTIM_PERIOD - 1);
			break;
        case 15:
            HAL_RTL_W8(REG_ATIM_DTIM_CTRL_SEL, (HAL_RTL_R8(REG_ATIM_DTIM_CTRL_SEL) &~BIT_MASK_ATIM_DTIM_SEL) | BIT_ATIM_DTIM_SEL(15));
            HAL_RTL_W8(REG_ATIMWND_CFG, 0x3C);
            HAL_RTL_W8(REG_DTIM_COUNTER_CFG, HAL_VAR_DTIM_PERIOD - 1);
			break;     
		default:
			RT_TRACE(COMP_INIT, DBG_WARNING, ("Invalid init swq=%d\n", HAL_VAR_VAP_INIT_SEQ));
			break;
		}
    }
}
#endif //#if  IS_RTL88XX_MAC_V4

#if (HAL_DEV_BUS_TYPE & (HAL_RT_EMBEDDED_INTERFACE | HAL_RT_PCI_INTERFACE))

#if (IS_RTL88XX_MAC_V1_V2)
// InitMBSSID88XX for 8881A, 8192E, 8814A ,8197F ,8822B used 
VOID
InitMBSSID88XX(
	IN  HAL_PADAPTER Adapter
)
{
	s4Byte      i, j;
	u4Byte      camData[2], camIdx;
	pu1Byte     macAddr = HAL_VAR_MY_HWADDR;
	u4Byte      bcn_early_time ;
	u4Byte      tbtt_hold;
	u4Byte      vap_bcn_offset;
    u4Byte      mask_MBIDCAM_addr,shift_MBIDCAM_addr;
#if (IS_RTL8197F_SERIES || IS_RTL8822C_SERIES|| IS_RTL8812F_SERIES)
	if ( IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter))
	{
            mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR_V1;
            shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR_V1;
        }
#endif  // #if IS_RTL8197F_SERIES    
#if (IS_RTL8192E_SERIES || IS_RTL8192F_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES )
        if ( IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8192F(Adapter) || 
		IS_HARDWARE_TYPE_8881A(Adapter) ||IS_HARDWARE_TYPE_8814A(Adapter) ||IS_HARDWARE_TYPE_8822B(Adapter) )
        {
            mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR;
            shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR;
        }
#endif
        if (HAL_IS_ROOT_INTERFACE(Adapter))
        {
		camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN | BIT_MBIDCAM_VALID | (macAddr[5] << 8) | macAddr[4];
		camData[1] = (macAddr[3] << 24) | (macAddr[2] << 16) | (macAddr[1] << 8) | macAddr[0];
		HAL_RTL_W32(REG_MBIDCAMCFG_1, camData[1]);
		HAL_RTL_W32(REG_MBIDCAMCFG_2, camData[0]);

#if 0	//Move to init_vap_hw for root interface, since in MBSSID mode, root MBSSID will be init until first VAP is opened.
		// clear the rest area of CAM
		camData[1] = 0;
            for (i = 1; i < 8; i++)
            {
			camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN | (i & BIT_MASK_MBIDCAM_ADDR) << BIT_SHIFT_MBIDCAM_ADDR;
			HAL_RTL_W32(REG_MBIDCAMCFG_1, camData[1]);
			HAL_RTL_W32(REG_MBIDCAMCFG_2, camData[0]);
		}

#endif
		// set MBIDCTRL & MBID_BCN_SPACE by cmd
#if IS_RTL88XX_MAC_V1
            if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1)
            {
			HAL_RTL_W32(REG_MBSSID_BCN_SPACE,
						(HAL_VAR_BCN_INTERVAL & BIT_MASK_BCN_SPACE1) << BIT_SHIFT_BCN_SPACE1
						| (HAL_VAR_BCN_INTERVAL & BIT_MASK_BCN_SPACE0) << BIT_SHIFT_BCN_SPACE0);
		}
#endif //IS_RTL88XX_MAC_V1

#if IS_RTL88XX_MAC_V2
            if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2)
            {
			HAL_RTL_W16(REG_MBSSID_BCN_SPACE,
						(HAL_VAR_BCN_INTERVAL & BIT_MASK_BCN_SPACE0) << BIT_SHIFT_BCN_SPACE0);
			HAL_RTL_W16(REG_MBSSID_BCN_SPACE3 + 2, HAL_VAR_BCN_INTERVAL);
		}
#endif //(IS_RTL8814A_SERIES || IS_RTL8197F_SERIES)


		HAL_RTL_W8(REG_HIQ_NO_LMT_EN, 0xff);
		HAL_RTL_W8(REG_BCN_CTRL, 0);
		HAL_RTL_W8(REG_DUAL_TSF_RST, 1);

#if (IS_RTL8197F_SERIES || IS_RTL8822B_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
        if( IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter))
        HAL_RTL_W8(REG_BCN_CTRL, BIT_EN_BCN_FUNCTION | BIT_DIS_TSF_UDT | BIT_DIS_RX_BSSID_FIT);
#endif
#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES )
        if( IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8881A(Adapter) ||IS_HARDWARE_TYPE_8814A(Adapter) )
            HAL_RTL_W8(REG_BCN_CTRL, BIT_EN_BCN_FUNCTION | BIT_DIS_TSF_UDT | BIT_EN_TXBCN_RPT | BIT_DIS_RX_BSSID_FIT);
#endif

		HAL_RTL_W32(REG_CCK_CHECK,  HAL_RTL_R32(REG_CCK_CHECK) | BIT_EN_BCN_PKT_REL);
		HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) | BIT_ENMBID);	// MBSSID enable

		if ((HAL_RTL_R8(REG_MBID_NUM) & BIT_MASK_MBID_BCN_NUM)==0) { // fix set root ap mac address after vap intf up will clear vap count issue.
			HAL_VAR_VAP_COUNT = 0;
		}
	}
#ifdef MBSSID
        else if (HAL_IS_VAP_INTERFACE(Adapter))
        {
		HAL_VAR_VAP_COUNT++;
		HAL_VAR_VAP_INIT_SEQ = HAL_VAR_VAP_COUNT;

		RT_TRACE(COMP_INIT, DBG_LOUD, ("init swq=%d\n", HAL_VAR_VAP_INIT_SEQ));

		SetDTIM(Adapter);

		camIdx = Adapter->vap_id + 1;
		camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN | BIT_MBIDCAM_VALID |
                     (camIdx & mask_MBIDCAM_addr) << shift_MBIDCAM_addr |
					 (macAddr[5] << 8) | macAddr[4];
		camData[1] = (macAddr[3] << 24) | (macAddr[2] << 16) | (macAddr[1] << 8) | macAddr[0];
		HAL_RTL_W32(REG_MBIDCAMCFG_1, camData[1]);
		HAL_RTL_W32(REG_MBIDCAMCFG_2, camData[0]);

#if CFG_HAL_DUAL_BCN_BUF
		// if odd number of AP, open one more AP.
		if ((HAL_VAR_VAP_COUNT % 2) == 0) {
			vap_bcn_offset = HAL_VAR_BCN_INTERVAL/(HAL_VAR_VAP_COUNT+2);
		} else
#endif
		{
			vap_bcn_offset = HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 1);
		}
		if (vap_bcn_offset > 200)
			vap_bcn_offset = 200;

#if IS_RTL88XX_MAC_V1
		if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1) {  
			HAL_RTL_W32(REG_MBSSID_BCN_SPACE, (vap_bcn_offset & BIT_MASK_BCN_SPACE1) << BIT_SHIFT_BCN_SPACE1
						| (HAL_VAR_BCN_INTERVAL & BIT_MASK_BCN_SPACE0) << BIT_SHIFT_BCN_SPACE0);
			if (HAL_RTL_R16(REG_MBSSID_BCN_SPACE + 2) < 10)
				HAL_RTL_W8(REG_DRVERLYINT,  HAL_RTL_R16(REG_MBSSID_BCN_SPACE + 2) - 2);
		}
#endif //IS_RTL88XX_MAC_V1
#if IS_RTL88XX_MAC_V2
        if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2) { 
			HAL_RTL_W16(REG_MBSSID_BCN_SPACE,
						(HAL_VAR_BCN_INTERVAL & BIT_MASK_BCN_SPACE0) << BIT_SHIFT_BCN_SPACE0);
			HAL_RTL_W16(REG_MBSSID_BCN_SPACE3 + 2, vap_bcn_offset);
			if (HAL_RTL_R16(REG_MBSSID_BCN_SPACE3 + 2) < 10)
				HAL_RTL_W8(REG_DRVERLYINT,  HAL_RTL_R16(REG_MBSSID_BCN_SPACE3 + 2) - 2);
		}
#endif //IS_RTL88XX_MAC_V2

		HAL_RTL_W8(REG_BCN_CTRL, 0);
		HAL_RTL_W8(REG_DUAL_TSF_RST, 1);
#if IS_RTL88XX_MAC_V2 && !IS_RTL8814A_SERIES
        if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2)
            HAL_RTL_W8(REG_BCN_CTRL, BIT_EN_BCN_FUNCTION | BIT_DIS_TSF_UDT | BIT_P0_EN_TXBCN_RPT | BIT_DIS_RX_BSSID_FIT);
#endif
#if IS_RTL88XX_MAC_V1
        if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1)
		    HAL_RTL_W8(REG_BCN_CTRL, BIT_EN_BCN_FUNCTION | BIT_DIS_TSF_UDT | BIT_EN_TXBCN_RPT | BIT_DIS_RX_BSSID_FIT);
#endif
#if IS_RTL8814A_SERIES
        if (IS_HARDWARE_TYPE_8814A(Adapter))
            HAL_RTL_W8(REG_BCN_CTRL, BIT_EN_BCN_FUNCTION | BIT_DIS_TSF_UDT | BIT_EN_TXBCN_RPT | BIT_DIS_RX_BSSID_FIT);
#endif
		// if odd number of AP, open one more AP. add close this additional AP
#if CFG_HAL_DUAL_BCN_BUF
		if ((HAL_VAR_VAP_INIT_SEQ % 2) == 0) {
			HAL_RTL_W8(REG_MBID_NUM, (HAL_RTL_R8(REG_MBID_NUM) & ~BIT_MASK_MBID_BCN_NUM) | ((HAL_VAR_VAP_INIT_SEQ + 1) & BIT_MASK_MBID_BCN_NUM));
			HAL_RTL_W8(REG_MBSSID_CTRL, (HAL_RTL_R8(REG_MBSSID_CTRL) | (1 << (HAL_VAR_VAP_INIT_SEQ))));
			HAL_RTL_W8(REG_MBSSID_CTRL, (HAL_RTL_R8(REG_MBSSID_CTRL) & (~(1 << (HAL_VAR_VAP_INIT_SEQ + 1)))));
		} else
#endif
		{
			HAL_RTL_W8(REG_MBID_NUM, (HAL_RTL_R8(REG_MBID_NUM) & ~BIT_MASK_MBID_BCN_NUM) | (HAL_VAR_VAP_INIT_SEQ & BIT_MASK_MBID_BCN_NUM));
			HAL_RTL_W8(REG_MBSSID_CTRL, (HAL_RTL_R8(REG_MBSSID_CTRL) | (1 << (HAL_VAR_VAP_INIT_SEQ))));
		}

#if CFG_HAL_DUAL_BCN_BUF
		bcn_early_time = HAL_RTL_R8(REG_DRVERLYINT);
#else
		bcn_early_time = HAL_RTL_R8(REG_BCNDMATIM);
#endif
		if ((HAL_VAR_VAP_COUNT % 2) == 0) {
#if CFG_HAL_DUAL_BCN_BUF
			tbtt_hold = (HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 2)) * 2 - bcn_early_time - 2;
#else
			tbtt_hold = (HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 1))   - bcn_early_time - 2;
#endif
		} else {
			tbtt_hold = (HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 1)) * 2 - bcn_early_time - 2;
		}
		if (tbtt_hold > 16)
			tbtt_hold = 16;
		HAL_RTL_W32(REG_TBTT_PROHIBIT, HAL_RTL_R8(REG_TBTT_PROHIBIT) | (((tbtt_hold * 1024 / 32)& BIT_MASK_TBTT_HOLD_TIME_AP) << BIT_SHIFT_TBTT_HOLD_TIME_AP));

		RT_TRACE(COMP_INIT, DBG_LOUD, ("REG_MBID_NUM(0x%x),HAL_VAR_VAP_INIT_SEQ(0x%x)\n", HAL_RTL_R8(REG_MBID_NUM), HAL_VAR_VAP_INIT_SEQ));

		HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) & ~BIT_ENMBID);
		HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) | BIT_ENMBID);	// MBSSID enable
	}
#endif
}

VOID
StopMBSSID88XX(
	IN  HAL_PADAPTER Adapter
)
{
	s4Byte      i, j;
	u4Byte      camData[2], camIdx;
	u4Byte      tempVal;
	HAL_PADAPTER tmpAdapter;
	u4Byte      vap_bcn_offset;
	u4Byte      bcn_early_time, tbtt_hold;
	PTX_BUFFER_DESCRIPTOR       pTXBD;
	PTX_DESC_88XX               ptx_desc;
    u4Byte      mask_MBIDCAM_addr,shift_MBIDCAM_addr;
#if (IS_RTL8197F_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
	if( IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter))
	{
        mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR_V1;
        shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR_V1;
    }
#endif  // #if IS_RTL8197F_SERIES    
#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES )
    if( IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8881A(Adapter) ||IS_HARDWARE_TYPE_8814A(Adapter) ||IS_HARDWARE_TYPE_8822B(Adapter) )
    {
        mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR;
        shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR;
    }
#endif
	camData[1] = 0;
	if (HAL_IS_ROOT_INTERFACE(Adapter)) {
		// clear the rest area of CAM
            for (i = 0; i < 8; i++)
            {
            camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN | (i & mask_MBIDCAM_addr) << shift_MBIDCAM_addr;
			HAL_RTL_W32(REG_MBIDCAMCFG_1, camData[1]);
			HAL_RTL_W32(REG_MBIDCAMCFG_2, camData[0]);
		}

		HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) & ~BIT_ENMBID);  // MBSSID disable
		HAL_RTL_W16(REG_MBSSID_BCN_SPACE,
			(HAL_VAR_BCN_INTERVAL & BIT_MASK_BCN_SPACE0) << BIT_SHIFT_BCN_SPACE0);

		HAL_RTL_W8(REG_BCN_CTRL, 0);
		HAL_RTL_W8(0x553, 1);
        
#if (IS_RTL8197F_SERIES || IS_RTL8822B_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
        if( IS_HARDWARE_TYPE_8197F(Adapter) ||IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter) )
        HAL_RTL_W8(REG_BCN_CTRL, BIT3 | BIT_DIS_TSF_UDT );
#endif
#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES )
        if( IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8881A(Adapter) ||IS_HARDWARE_TYPE_8814A(Adapter) )
		HAL_RTL_W8(REG_BCN_CTRL, BIT3 | BIT_DIS_TSF_UDT | BIT_EN_TXBCN_RPT);
#endif
        }
        else if (HAL_IS_VAP_INTERFACE(Adapter) && (HAL_VAR_VAP_INIT_SEQ >= 0))
        {
		camIdx = Adapter->vap_id + 1;
		camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN |
                     (camIdx & mask_MBIDCAM_addr) << shift_MBIDCAM_addr;
		HAL_RTL_W32(REG_MBIDCAMCFG_1, camData[1]);
		HAL_RTL_W32(REG_MBIDCAMCFG_2, camData[0]);

        if (HAL_OPMODE & WIFI_AP_STATE) {
			HAL_VAR_VAP_COUNT--;
			for (i = 0; i < HAL_NUM_VWLAN; i++) {
				tmpAdapter = HAL_GET_ROOT(Adapter)->pvap_priv[i];
				if (tmpAdapter && (tmpAdapter->pmib->dot11OperationEntry.opmode & WIFI_AP_STATE) && 
					(tmpAdapter->vap_init_seq > HAL_VAR_VAP_INIT_SEQ)) {
					tmpAdapter->vap_init_seq -= 1;
#ifdef HW_ENC_FOR_GROUP_CIPHER
					ToDrv_SetGTK(tmpAdapter);
#endif					
					GetBeaconTXBDTXDESC88XX(tmpAdapter, &pTXBD, &ptx_desc);
                    GET_HAL_INTERFACE(Adapter)->FillBeaconDescHandler(tmpAdapter, ptx_desc, tmpAdapter->beaconbuf, tmpAdapter->tx_beacon_len, 1);
					//FillBeaconDesc88XX(tmpAdapter, ptx_desc, tmpAdapter->beaconbuf, tmpAdapter->tx_beacon_len, 1);
					SetDTIM(tmpAdapter);
				}
			}

			if (HAL_RTL_R8(REG_MBID_NUM) & BIT_MASK_MBID_BCN_NUM) {
#if CFG_HAL_DUAL_BCN_BUF
				// if odd number of AP, open one more AP.
				if ((HAL_VAR_VAP_COUNT != 0) && ((HAL_VAR_VAP_COUNT % 2) == 0)) {
					vap_bcn_offset = HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 2);
				} else
#endif
				{
					vap_bcn_offset = HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 1);
				}
				if (vap_bcn_offset > 200)
					vap_bcn_offset = 200;

#if IS_RTL88XX_MAC_V1
		if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1) { 
					HAL_RTL_W32(REG_MBSSID_BCN_SPACE, (vap_bcn_offset & BIT_MASK_BCN_SPACE1) << BIT_SHIFT_BCN_SPACE1
						| (HAL_VAR_BCN_INTERVAL & BIT_MASK_BCN_SPACE0) << BIT_SHIFT_BCN_SPACE0);
					if (HAL_RTL_R16(REG_MBSSID_BCN_SPACE + 2) < 10)
					HAL_RTL_W8(REG_DRVERLYINT,  HAL_RTL_R16(REG_MBSSID_BCN_SPACE + 2) - 2);
				}
#endif //IS_RTL88XX_MAC_V1
#if IS_RTL88XX_MAC_V2
        if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2) {  
					HAL_RTL_W16(REG_MBSSID_BCN_SPACE,
                        (HAL_VAR_BCN_INTERVAL & BIT_MASK_BCN_SPACE0) << BIT_SHIFT_BCN_SPACE0);
					
					// TODO: need to fixed the following code for MBSSID and check other REG_MBSSID_BCN_SPACE3
					tempVal = ((HAL_VAR_BCN_INTERVAL / ((HAL_RTL_R8(REG_MBID_NUM) & 0x07))));
					HAL_RTL_W32(REG_MBSSID_BCN_SPACE3, (HAL_RTL_R32(REG_MBSSID_BCN_SPACE3) & 0xFF00FFFF) | BIT_SUB_BCN_SPACE(tempVal));

					if (HAL_RTL_R16(REG_MBSSID_BCN_SPACE3 + 2) < 10)
						HAL_RTL_W8(REG_DRVERLYINT,  HAL_RTL_R16(REG_MBSSID_BCN_SPACE3 + 2) - 2);
				}
#endif //IS_RTL88XX_MAC_V2

				

				HAL_RTL_W8(REG_BCN_CTRL, 0);
				HAL_RTL_W8(REG_DUAL_TSF_RST, 1);
#if IS_RTL88XX_MAC_V2 && !IS_RTL8814A_SERIES
                if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2)
                    HAL_RTL_W8(REG_BCN_CTRL, BIT_EN_BCN_FUNCTION | BIT_DIS_TSF_UDT | BIT_P0_EN_TXBCN_RPT | BIT_DIS_RX_BSSID_FIT);
#endif
#if IS_RTL88XX_MAC_V1
                if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1)
				    HAL_RTL_W8(REG_BCN_CTRL, BIT_EN_BCN_FUNCTION | BIT_DIS_TSF_UDT | BIT_EN_TXBCN_RPT | BIT_DIS_RX_BSSID_FIT);
#endif
#if (IS_RTL8197F_SERIES || IS_RTL8822B_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
                if(IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter))
                    HAL_RTL_W8(REG_BCN_CTRL, BIT_EN_BCN_FUNCTION | BIT_DIS_TSF_UDT | BIT_DIS_RX_BSSID_FIT);
#endif
#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES)
                if(IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8881A(Adapter) ||IS_HARDWARE_TYPE_8814A(Adapter))
                    HAL_RTL_W8(REG_BCN_CTRL, BIT_EN_BCN_FUNCTION | BIT_DIS_TSF_UDT | BIT_EN_TXBCN_RPT | BIT_DIS_RX_BSSID_FIT);
#endif
			}

			HAL_RTL_W8(REG_MBSSID_CTRL, (HAL_RTL_R8(REG_MBSSID_CTRL) & (~(1 << (HAL_VAR_VAP_COUNT + 1)))));
			// if odd number of AP, open one more AP. add close this additional AP
#if CFG_HAL_DUAL_BCN_BUF
			if ((HAL_VAR_VAP_COUNT != 0) && (HAL_VAR_VAP_COUNT % 2) == 0) {
				HAL_RTL_W8(REG_MBID_NUM, (HAL_RTL_R8(REG_MBID_NUM) & ~BIT_MASK_MBID_BCN_NUM) | ((HAL_VAR_VAP_COUNT + 1) & BIT_MASK_MBID_BCN_NUM));
			} else
#endif
			{
				HAL_RTL_W8(REG_MBID_NUM, (HAL_RTL_R8(REG_MBID_NUM) & ~BIT_MASK_MBID_BCN_NUM) | (HAL_VAR_VAP_COUNT & BIT_MASK_MBID_BCN_NUM));
			}

#if CFG_HAL_DUAL_BCN_BUF
			bcn_early_time = HAL_RTL_R8(REG_DRVERLYINT);
#else
			bcn_early_time = HAL_RTL_R8(REG_BCNDMATIM);
#endif
                if ((HAL_VAR_VAP_COUNT % 2) == 0)
                {
#if CFG_HAL_DUAL_BCN_BUF
				tbtt_hold = (HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 2)) * 2 - bcn_early_time - 2;
#else
				tbtt_hold = (HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 1))   - bcn_early_time - 2;
#endif
			}
                else
    				{
    				tbtt_hold = (HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 1)) * 2 - bcn_early_time - 2;
    			}
    			if (tbtt_hold > 16)
    				tbtt_hold = 16;
                HAL_RTL_W32(REG_TBTT_PROHIBIT, HAL_RTL_R8(REG_TBTT_PROHIBIT) | (((tbtt_hold * 1024 / 32)& BIT_MASK_TBTT_HOLD_TIME_AP) << BIT_SHIFT_TBTT_HOLD_TIME_AP));
    		}

    		HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) & ~BIT_ENMBID);
    		HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) | BIT_ENMBID);
    		HAL_VAR_VAP_INIT_SEQ = -1;
    	}
    }

#endif //#if (IS_RTL88XX_MAC_V1_V2)


#if (IS_RTL88XX_MAC_V3)
// InitMBSSID88XX_V1 for 8198F used 
VOID
InitMBSSID88XX_V1(
	IN  HAL_PADAPTER Adapter
)
{
        s4Byte      i, j;
        u4Byte      camData[2], camIdx;
        pu1Byte     macAddr = HAL_VAR_MY_HWADDR;
        u4Byte      bcn_early_time ;
        u4Byte      tbtt_hold;
        u4Byte      vap_bcn_offset;
        u4Byte      mask_MBIDCAM_addr;
        u4Byte      shift_MBIDCAM_addr;
        u4Byte      bit_MBIDCAM_valid;
        u1Byte      bcn_ctrl;
        u4Byte      mask_MBID_BCN_NUM;
    
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter)){
        mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR_V2;
        shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR_V2;
        bit_MBIDCAM_valid = BIT_MBIDCAM_VALID_V1;
        bcn_ctrl = BIT_EN_BCN_FUNCTION | BIT_DIS_TSF_UDT | BIT_P0_EN_TXBCN_RPT | BIT_DIS_RX_BSSID_FIT;
        mask_MBID_BCN_NUM = BIT_MASK_MBID_BCN_NUM_V2;
        }
#endif

#if IS_RTL8197G_SERIES
        if(IS_HARDWARE_TYPE_8197G(Adapter)){
            mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR_V1;
            shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR_V1;
            bit_MBIDCAM_valid = BIT_MBIDCAM_VALID_V1;
            bcn_ctrl = BIT_EN_BCN_FUNCTION | BIT_DIS_TSF_UDT | BIT_P0_EN_TXBCN_RPT | BIT_DIS_RX_BSSID_FIT;
            mask_MBID_BCN_NUM = BIT_MASK_MBID_BCN_NUM_V2;
        }
#endif
    
        if (HAL_IS_ROOT_INTERFACE(Adapter))
        {
            camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN | bit_MBIDCAM_valid | (macAddr[5] << 8) | macAddr[4];
            camData[1] = (macAddr[3] << 24) | (macAddr[2] << 16) | (macAddr[1] << 8) | macAddr[0];
            HAL_RTL_W32(REG_MBIDCAMCFG_1, camData[1]);
            HAL_RTL_W32(REG_MBIDCAMCFG_2, camData[0]);
    
#if 0	//Move to init_vap_hw for root interface, since in MBSSID mode, root MBSSID will be init until first VAP is opened.
            // clear the rest area of CAM
            camData[1] = 0;
            for (i = 1; i < 8; i++)
            {
                camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN | (i & BIT_MASK_MBIDCAM_ADDR) << BIT_SHIFT_MBIDCAM_ADDR;
                HAL_RTL_W32(REG_MBIDCAMCFG_1, camData[1]);
                HAL_RTL_W32(REG_MBIDCAMCFG_2, camData[0]);
            }
    
#endif
            // set MBIDCTRL & MBID_BCN_SPACE by cmd
            HAL_RTL_W16(REG_MBSSID_BCN_SPACE,
                        (HAL_VAR_BCN_INTERVAL & BIT_MASK_BCN_SPACE0) << BIT_SHIFT_BCN_SPACE0);
            HAL_RTL_W16(REG_MBSSID_BCN_SPACE3 + 2, HAL_VAR_BCN_INTERVAL);
            HAL_RTL_W16(REG_HIQ_NO_LMT_EN_V2, 0xffff);
    
            HAL_RTL_W8(REG_BCN_CTRL, 0);
            HAL_RTL_W8(REG_DUAL_TSF_RST, BIT_TSFTR_RST);
            HAL_RTL_W8(REG_BCN_CTRL, bcn_ctrl);    
            HAL_RTL_W32(REG_CCK_CHECK,  HAL_RTL_R32(REG_CCK_CHECK) | BIT_EN_BCN_PKT_REL);
            HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) | BIT_ENMBID);    // MBSSID enable
    
            HAL_VAR_VAP_COUNT = 0;
        }
#ifdef MBSSID
        else if (HAL_IS_VAP_INTERFACE(Adapter))
        {
            HAL_VAR_VAP_COUNT++;
            HAL_VAR_VAP_INIT_SEQ = HAL_VAR_VAP_COUNT;
    
            RT_TRACE(COMP_INIT, DBG_LOUD, ("init swq=%d\n", HAL_VAR_VAP_INIT_SEQ));
            SetDTIM_V1(Adapter);
    
            camIdx = Adapter->vap_id + 1;
            camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN | bit_MBIDCAM_valid |
                         (camIdx & mask_MBIDCAM_addr) << shift_MBIDCAM_addr |
                         (macAddr[5] << 8) | macAddr[4];
            camData[1] = (macAddr[3] << 24) | (macAddr[2] << 16) | (macAddr[1] << 8) | macAddr[0];
            HAL_RTL_W32(REG_MBIDCAMCFG_1, camData[1]);
            HAL_RTL_W32(REG_MBIDCAMCFG_2, camData[0]);
    
#if CFG_HAL_DUAL_BCN_BUF
            // if odd number of AP, open one more AP.
            if ((HAL_VAR_VAP_COUNT % 2) == 0)
            {
                vap_bcn_offset = HAL_VAR_BCN_INTERVAL/(HAL_VAR_VAP_COUNT+2);
            }
            else
#endif
            {
                vap_bcn_offset = HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 1);
            }
            if (vap_bcn_offset > 200)
                vap_bcn_offset = 200;
    
            HAL_RTL_W16(REG_MBSSID_BCN_SPACE,
                        (HAL_VAR_BCN_INTERVAL & BIT_MASK_BCN_SPACE0) << BIT_SHIFT_BCN_SPACE0);
            HAL_RTL_W16(REG_MBSSID_BCN_SPACE3 + 2, vap_bcn_offset);
            if (HAL_RTL_R16(REG_MBSSID_BCN_SPACE3 + 2) < 10)
                HAL_RTL_W8(REG_DRVERLYINT,  HAL_RTL_R16(REG_MBSSID_BCN_SPACE3 + 2) - 2);
    
            HAL_RTL_W8(REG_BCN_CTRL, 0);
            HAL_RTL_W8(REG_DUAL_TSF_RST, BIT_TSFTR_RST);
            HAL_RTL_W8(REG_BCN_CTRL, bcn_ctrl);
    
            // if odd number of AP, open one more AP. add close this additional AP
#if CFG_HAL_DUAL_BCN_BUF
            if ((HAL_VAR_VAP_INIT_SEQ % 2) == 0)
            {
                HAL_RTL_W8(REG_MBID_NUM, (HAL_RTL_R8(REG_MBID_NUM) & ~mask_MBID_BCN_NUM) | ((HAL_VAR_VAP_INIT_SEQ + 1) & mask_MBID_BCN_NUM));
                HAL_RTL_W8(REG_MBSSID_CTRL, (HAL_RTL_R8(REG_MBSSID_CTRL) | (1 << (HAL_VAR_VAP_INIT_SEQ))));
                HAL_RTL_W8(REG_MBSSID_CTRL, (HAL_RTL_R8(REG_MBSSID_CTRL) & (~(1 << (HAL_VAR_VAP_INIT_SEQ + 1)))));
            }
            else
#endif
    
                HAL_RTL_W8(REG_MBID_NUM, ((HAL_RTL_R8(REG_MBID_NUM)& ~BITS_MBID_BCN_NUM_V2 ) | ((HAL_VAR_VAP_INIT_SEQ & BIT_MASK_MBID_BCN_NUM_V2) << BIT_SHIFT_MBID_BCN_NUM_V2) ));
            HAL_RTL_W16(REG_MBID_BCNQ_EN, (HAL_RTL_R16(REG_MBID_BCNQ_EN) | (1 << (HAL_VAR_VAP_INIT_SEQ))) );
    
#if CFG_HAL_DUAL_BCN_BUF
            bcn_early_time = HAL_RTL_R8(REG_DRVERLYINT);
#else
            bcn_early_time = HAL_RTL_R8(REG_BCNDMATIM);
#endif
            if ((HAL_VAR_VAP_COUNT % 2) == 0)
            {
#if CFG_HAL_DUAL_BCN_BUF
                tbtt_hold = (HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 2)) * 2 - bcn_early_time - 2;
#else
                tbtt_hold = (HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 1))   - bcn_early_time - 2;
#endif
            }
            else
            {
                tbtt_hold = (HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 1)) * 2 - bcn_early_time - 2;
            }
            if (tbtt_hold > 16)
                tbtt_hold = 16;
            HAL_RTL_W32(REG_TBTT_PROHIBIT, HAL_RTL_R8(REG_TBTT_PROHIBIT) | (((tbtt_hold * 1024 / 32)& BIT_MASK_TBTT_HOLD_TIME_AP) << BIT_SHIFT_TBTT_HOLD_TIME_AP));
    
            RT_TRACE(COMP_INIT, DBG_LOUD, ("REG_MBID_NUM(0x%x),HAL_VAR_VAP_INIT_SEQ(0x%x)\n", HAL_RTL_R8(REG_MBID_NUM), HAL_VAR_VAP_INIT_SEQ));
    
            HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) & ~BIT_ENMBID);
            HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) | BIT_ENMBID);    // MBSSID enable
        }
#endif
}

VOID
StopMBSSID88XX_V1(
	IN  HAL_PADAPTER Adapter
)
{
        s4Byte      i, j;
        u4Byte      camData[2], camIdx;
        u4Byte      tempVal;
        HAL_PADAPTER tmpAdapter;
        u4Byte      vap_bcn_offset;
        u4Byte      bcn_early_time, tbtt_hold;
        PTX_BUFFER_DESCRIPTOR       pTXBD;
        PTX_DESC_88XX               ptx_desc;
        u4Byte      mask_MBIDCAM_addr;
        u4Byte      shift_MBIDCAM_addr;
        u4Byte      bit_MBIDCAM_valid;
        u1Byte      bcn_ctrl;
        u4Byte      mask_MBID_BCN_NUM;
    
#if IS_RTL8198F_SERIES
    if(IS_HARDWARE_TYPE_8198F(Adapter)){
        mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR_V2;
        shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR_V2;
    }
#endif
#if IS_RTL8197G_SERIES
    if(IS_HARDWARE_TYPE_8197G(Adapter)){
        mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR_V1;
        shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR_V1;
    }
#endif
        
        
        bit_MBIDCAM_valid = BIT_MBIDCAM_VALID_V1;
        bcn_ctrl = BIT_EN_BCN_FUNCTION | BIT_DIS_TSF_UDT | BIT_P0_EN_TXBCN_RPT;
        mask_MBID_BCN_NUM = BIT_MASK_MBID_BCN_NUM_V2;
        camData[1] = 0;
        if (HAL_IS_ROOT_INTERFACE(Adapter))
        {
            // clear the rest area of CAM
            for (i = 0; i < 8; i++)
            {
                camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN | (i & mask_MBIDCAM_addr) << shift_MBIDCAM_addr;
                HAL_RTL_W32(REG_MBIDCAMCFG_1, camData[1]);
                HAL_RTL_W32(REG_MBIDCAMCFG_2, camData[0]);
            }
    
            HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) & ~BIT_ENMBID);  // MBSSID disable
            HAL_RTL_W16(REG_MBSSID_BCN_SPACE,
                        (HAL_VAR_BCN_INTERVAL & BIT_MASK_BCN_SPACE0) << BIT_SHIFT_BCN_SPACE0);
    
            HAL_RTL_W8(REG_BCN_CTRL, 0);
            HAL_RTL_W8(REG_DUAL_TSF_RST, BIT_TSFTR_RST);
            HAL_RTL_W8(REG_BCN_CTRL, bcn_ctrl);
    
        }
        else if (HAL_IS_VAP_INTERFACE(Adapter) && (HAL_VAR_VAP_INIT_SEQ >= 0))
        {
            camIdx = Adapter->vap_id + 1;
            camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN |
                         (camIdx & mask_MBIDCAM_addr) << shift_MBIDCAM_addr;
            HAL_RTL_W32(REG_MBIDCAMCFG_1, camData[1]);
            HAL_RTL_W32(REG_MBIDCAMCFG_2, camData[0]);
    
            if (HAL_OPMODE & WIFI_AP_STATE)
            {
                HAL_VAR_VAP_COUNT--;
                for (i = 0; i < HAL_NUM_VWLAN; i++)
                {
                    tmpAdapter = HAL_GET_ROOT(Adapter)->pvap_priv[i];
                    if (tmpAdapter && (tmpAdapter->pmib->dot11OperationEntry.opmode & WIFI_AP_STATE) &&
                            (tmpAdapter->vap_init_seq > HAL_VAR_VAP_INIT_SEQ))
                    {
                        tmpAdapter->vap_init_seq -= 1;
#ifdef HW_ENC_FOR_GROUP_CIPHER
						ToDrv_SetGTK(tmpAdapter);
#endif						
                        GetBeaconTXBDTXDESC88XX(tmpAdapter, &pTXBD, &ptx_desc);
                        GET_HAL_INTERFACE(Adapter)->FillBeaconDescHandler(tmpAdapter, ptx_desc, tmpAdapter->beaconbuf, tmpAdapter->tx_beacon_len, 1);
                        //FillBeaconDesc88XX(tmpAdapter, ptx_desc, tmpAdapter->beaconbuf, tmpAdapter->tx_beacon_len, 1);
                        //#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8197F_SERIES)
                        SetDTIM_V1(tmpAdapter);
                    }
                }
    
                if (HAL_RTL_R8(REG_MBID_NUM)& BITS_MBID_BCN_NUM_V2 & ~mask_MBID_BCN_NUM)
                {
#if CFG_HAL_DUAL_BCN_BUF
                    // if odd number of AP, open one more AP.
                    if ((HAL_VAR_VAP_COUNT != 0) && ((HAL_VAR_VAP_COUNT % 2) == 0))
                    {
                        vap_bcn_offset = HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 2);
                    }
                    else
#endif
                    {
                        vap_bcn_offset = HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 1);
                    }
                    if (vap_bcn_offset > 200)
                        vap_bcn_offset = 200;
    
                    HAL_RTL_W16(REG_MBSSID_BCN_SPACE,
                                ((HAL_VAR_BCN_INTERVAL / ((HAL_RTL_R8(REG_MBID_NUM)& BITS_MBID_BCN_NUM_V2 & ~mask_MBID_BCN_NUM) + 1)) & BIT_MASK_BCN_SPACE0) << BIT_SHIFT_BCN_SPACE0);
                    // TODO: need to fixed the following code for MBSSID and check other REG_MBSSID_BCN_SPACE3
                    tempVal = HAL_VAR_BCN_INTERVAL - ((HAL_VAR_BCN_INTERVAL / ((HAL_RTL_R8(REG_MBID_NUM)& BITS_MBID_BCN_NUM_V2 & ~mask_MBID_BCN_NUM) + 1)) * (HAL_RTL_R8(REG_MBID_NUM)& BITS_MBID_BCN_NUM_V2 & ~mask_MBID_BCN_NUM));
                    HAL_RTL_W32(REG_MBSSID_BCN_SPACE3, (HAL_RTL_R32(REG_MBSSID_BCN_SPACE3) & 0xFF00FFFF) | BIT_SUB_BCN_SPACE(tempVal));
                    if (HAL_RTL_R16(REG_MBSSID_BCN_SPACE3 + 2) < 10)
                        HAL_RTL_W8(REG_DRVERLYINT,  HAL_RTL_R16(REG_MBSSID_BCN_SPACE3 + 2) - 2);
                    HAL_RTL_W8(REG_BCN_CTRL, 0);
                    HAL_RTL_W8(REG_DUAL_TSF_RST, BIT_TSFTR_RST);
                    HAL_RTL_W8(REG_BCN_CTRL, bcn_ctrl);
                }
                HAL_RTL_W16(REG_MBID_BCNQ_EN, (HAL_RTL_R16(REG_MBID_BCNQ_EN) & (~(1 << (HAL_VAR_VAP_COUNT + 1)))));
    
                // if odd number of AP, open one more AP. add close this additional AP
#if CFG_HAL_DUAL_BCN_BUF
                if ((HAL_VAR_VAP_COUNT != 0) && (HAL_VAR_VAP_COUNT % 2) == 0)
                {
                    HAL_RTL_W8(REG_MBID_NUM, (HAL_RTL_R8(REG_MBID_NUM)& ~BITS_MBID_BCN_NUM_V2 ) | ((HAL_VAR_VAP_COUNT + 1) & BITS_MBID_BCN_NUM_V2));
                }
                else
#endif
                {
                    HAL_RTL_W8(REG_MBID_NUM, (HAL_RTL_R8(REG_MBID_NUM) & ~BITS_MBID_BCN_NUM_V2) | ((HAL_VAR_VAP_COUNT << BIT_SHIFT_MBID_BCN_NUM_V2) & BITS_MBID_BCN_NUM_V2));                   
                }
    
#if CFG_HAL_DUAL_BCN_BUF
                bcn_early_time = HAL_RTL_R8(REG_DRVERLYINT);
#else
                bcn_early_time = HAL_RTL_R8(REG_BCNDMATIM);
#endif
                if ((HAL_VAR_VAP_COUNT % 2) == 0)
                {
#if CFG_HAL_DUAL_BCN_BUF
				tbtt_hold = (HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 2)) * 2 - bcn_early_time - 2;
#else
				tbtt_hold = (HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 1))   - bcn_early_time - 2;
#endif
			} else {
				tbtt_hold = (HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 1)) * 2 - bcn_early_time - 2;
			}
			if (tbtt_hold > 16)
				tbtt_hold = 16;
			HAL_RTL_W32(REG_TBTT_PROHIBIT, HAL_RTL_R8(REG_TBTT_PROHIBIT) | (((tbtt_hold * 1024 / 32)& BIT_MASK_TBTT_HOLD_TIME_AP) << BIT_SHIFT_TBTT_HOLD_TIME_AP));
		}

		HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) & ~BIT_ENMBID);
		HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) | BIT_ENMBID);
		HAL_VAR_VAP_INIT_SEQ = -1;
	}
}

#endif //IS_RTL88XX_MAC_V3

#if (IS_RTL88XX_MAC_V4)

VOID
InitMBSSID88XX_V2(
	IN  HAL_PADAPTER Adapter
)
{

    u4Byte      camData[8], camIdx, write_cmd;
	pu1Byte     macAddr = HAL_VAR_MY_HWADDR;
    u1Byte      bcn_ctrl;
  	u4Byte      vap_bcn_offset;
  	u4Byte      bcn_early_time ;
	u4Byte      tbtt_hold;
    u1Byte idx=0;

    if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4 )
    {
        bcn_ctrl = BIT_EN_PORT_FUNCTION | BIT_DIS_TSF_UDT | BIT_EN_TXBCN_RPT_V1 | BIT_DIS_RX_BSSID_FIT;

        if (HAL_IS_ROOT_INTERFACE(Adapter))
        {
			HAL_RTL_W16(REG_BCN_SPACE_CFG,
						(HAL_VAR_BCN_INTERVAL & BIT_MASK_BCN_SPACE) << BIT_SHIFT_BCN_SPACE);
			HAL_RTL_W8(REG_SUB_BCN_SPACE, HAL_VAR_BCN_INTERVAL);
  
            HAL_RTL_W16(REG_HIQ_NO_LMT_EN_V1, 0xffff);        


            //select port0
            HAL_RTL_W8(REG_PORT_CTRL_SEL,HAL_RTL_R8(REG_PORT_CTRL_SEL) & ~BIT_MASK_PORT_CTRL_SEL);

    		HAL_RTL_W8(REG_PORT_CTRL_CFG, 0);
    		HAL_RTL_W8(REG_PORT_CTRL_CFG, HAL_RTL_R8(REG_PORT_CTRL_CFG) | BIT_TSFTR_RST_V1);
    		HAL_RTL_W8(REG_PORT_CTRL_CFG, bcn_ctrl);

    		HAL_RTL_W32(REG_CCK_CHECK,  HAL_RTL_R32(REG_CCK_CHECK) |  BIT_EN_BCN_PKT_REL_P0);
    		HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) | BIT_ENADDRCAM);	// MBSSID enable = BIT_ENADDRCAM

    		HAL_VAR_VAP_COUNT = 0;
    	}
#ifdef MBSSID
        else if (HAL_IS_VAP_INTERFACE(Adapter)) {

            HAL_VAR_VAP_COUNT++;
            HAL_VAR_VAP_INIT_SEQ = HAL_VAR_VAP_COUNT;
    
            RT_TRACE(COMP_INIT, DBG_LOUD, ("init swq=%d\n", HAL_VAR_VAP_INIT_SEQ));
            SetDTIM_V2(Adapter);
            
#if CFG_HAL_DUAL_BCN_BUF
            // if odd number of AP, open one more AP.
            if ((HAL_VAR_VAP_COUNT % 2) == 0) {
                vap_bcn_offset = HAL_VAR_BCN_INTERVAL/(HAL_VAR_VAP_COUNT+2);
            } else
#endif
            {
                vap_bcn_offset = HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 1);
            }
            if (vap_bcn_offset > 200)
                vap_bcn_offset = 200;
    
            HAL_RTL_W16(REG_BCN_SPACE_CFG,
                        (HAL_VAR_BCN_INTERVAL & BIT_MASK_BCN_SPACE) << BIT_SHIFT_BCN_SPACE);
            HAL_RTL_W8(REG_SUB_BCN_SPACE, vap_bcn_offset);
            if (HAL_RTL_R8(REG_SUB_BCN_SPACE) < 10)
                HAL_RTL_W8(REG_DRVERLYINT_CFG,  HAL_RTL_R8(REG_SUB_BCN_SPACE) - 2);

    
            HAL_RTL_W8(REG_PORT_CTRL_CFG, 0);
            HAL_RTL_W8(REG_PORT_CTRL_CFG,BIT_TSFTR_RST_V1);
            HAL_RTL_W8(REG_PORT_CTRL_CFG, bcn_ctrl);
    
            // if odd number of AP, open one more AP. and close this additional AP
#if CFG_HAL_DUAL_BCN_BUF
            if ((HAL_VAR_VAP_INIT_SEQ % 2) == 0) {
                HAL_RTL_W8(REG_MBID_NUM_V1, (HAL_RTL_R8(REG_MBID_NUM_V1) & ~BIT_MASK_MBID_BCN_NUM_V1) | BIT_MBID_BCN_NUM_V1(HAL_VAR_VAP_INIT_SEQ + 1));
                HAL_RTL_W8(REG_MBSSID_CTRL_V1, (HAL_RTL_R8(REG_MBSSID_CTRL_V1) | (1 << (HAL_VAR_VAP_INIT_SEQ))));
                HAL_RTL_W8(REG_MBSSID_CTRL_V1, (HAL_RTL_R8(REG_MBSSID_CTRL_V1) & (~(1 << (HAL_VAR_VAP_INIT_SEQ + 1)))));
            } else
#endif
      
            HAL_RTL_W8(REG_MBID_NUM_V1, ((HAL_RTL_R8(REG_MBID_NUM_V1)& ~BIT_MASK_MBID_BCN_NUM_V1 ) | BIT_MBID_BCN_NUM_V1(HAL_VAR_VAP_INIT_SEQ)));
            HAL_RTL_W16(REG_MBSSID_CTRL_V1, (HAL_RTL_R16(REG_MBSSID_CTRL_V1) | (1 << (HAL_VAR_VAP_INIT_SEQ))) );

#if 0    
#if CFG_HAL_DUAL_BCN_BUF
            bcn_early_time = HAL_RTL_R8(REG_DRVERLYINT_CFG);
#else
            bcn_early_time = HAL_RTL_R8(REG_BCNDMATIM_CFG);
#endif
            if ((HAL_VAR_VAP_COUNT % 2) == 0) {
#if CFG_HAL_DUAL_BCN_BUF
                tbtt_hold = (HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 2)) * 2 - bcn_early_time - 2;
#else
                tbtt_hold = (HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 1))   - bcn_early_time - 2;
#endif
            } else {
                tbtt_hold = (HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 1)) * 2 - bcn_early_time - 2;
            }
            if (tbtt_hold > 16)
                tbtt_hold = 16;
            HAL_RTL_W32(REG_TBTT_PROHIBIT_CFG, (HAL_RTL_R32(REG_TBTT_PROHIBIT_CFG) & ~BITS_TBTT_HOLD_TIME) | BIT_TBTT_HOLD_TIME(tbtt_hold * 1024 / 32));
#endif    
            RT_TRACE(COMP_INIT, DBG_LOUD, ("REG_MBID_NUM(0x%x),HAL_VAR_VAP_INIT_SEQ(0x%x)\n", HAL_RTL_R8(REG_MBID_NUM_V1), HAL_VAR_VAP_INIT_SEQ));
    
            HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) & ~BIT_ENADDRCAM);
            HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) | BIT_ENADDRCAM);    // MBSSID enable
          
			HAL_RTL_W16(REG_CONTROL_FRAME_CNT_CTRL, BIT(8) | ((Adapter->vap_id + 1) << 4));
			HAL_RTL_W8(REG_CONTROL_FRAME_REPORT, BIT(0));
        }
#endif //#ifdef MBSSID
    }
}

VOID
StopMBSSID88XX_V2(
    IN  HAL_PADAPTER Adapter
)
{
    s4Byte      i, j;
    u4Byte      write_cmd, camIdx;
    HAL_PADAPTER tmpAdapter;
    u4Byte      vap_bcn_offset;
    u4Byte      bcn_early_time, tbtt_hold;
    PTX_BUFFER_DESCRIPTOR       pTXBD;
    PTX_DESC_88XX               ptx_desc;
    u4Byte      mask_MBIDCAM_addr;
    u4Byte      shift_MBIDCAM_addr;
    u4Byte      bit_MBIDCAM_valid;
    u1Byte      bcn_ctrl;
    u4Byte      mask_MBID_BCN_NUM;

    bcn_ctrl = BIT_EN_PORT_FUNCTION | BIT_DIS_TSF_UDT | BIT_EN_TXBCN_RPT_V1;
    if (HAL_IS_ROOT_INTERFACE(Adapter))
    {
#ifdef ADDRESS_CAM
        // clear the rest area of CAM
        for(camIdx = 0; camIdx < ADDR_CAM_ENTRY_NUM; camIdx++)
        {
            for (i = 0; i < 8; i++)
            {
                write_cmd =  BIT_MASK_ADDRCAM_RANGE | BIT_ADDRCAM_POLL | BIT__ADDRCAM_WT_EN | BIT__ADDRCAM_ADDR((camIdx*8)+j);
                HAL_RTL_W32(REG_ADDRCAM_WRITE_CONTENT, 0);
                HAL_RTL_W32(REG_ADDRCAM_CFG, write_cmd);
            }
        }
		memset(Adapter->pshare->addr_cam_used, 0, sizeof(Adapter->pshare->addr_cam_used));

        HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) & ~BIT_ENADDRCAM);  // MBSSID disable
#endif

        HAL_RTL_W16(REG_BCN_SPACE_CFG, (HAL_VAR_BCN_INTERVAL & BIT_MASK_BCN_SPACE) << BIT_SHIFT_BCN_SPACE);
        
        HAL_RTL_W8(REG_PORT_CTRL_CFG, 0);
        HAL_RTL_W8(REG_PORT_CTRL_CFG, HAL_RTL_R8(REG_PORT_CTRL_CFG) | BIT_TSFTR_RST_V1);
        HAL_RTL_W8(REG_PORT_CTRL_CFG, bcn_ctrl);

    }
    else if (HAL_IS_VAP_INTERFACE(Adapter) && (HAL_VAR_VAP_INIT_SEQ >= 0))
    {
#ifdef ADDRESS_CAM
        camIdx = Adapter->interface_addrcam_entry;
        for (i = 0; i < 8; i++)
        {
            write_cmd = BIT_MASK_ADDRCAM_RANGE | BIT_ADDRCAM_POLL | BIT__ADDRCAM_WT_EN | BIT__ADDRCAM_ADDR((camIdx*8)+j);
            HAL_RTL_W32(REG_ADDRCAM_WRITE_CONTENT, 0);
            HAL_RTL_W32(REG_ADDRCAM_CFG, write_cmd);
        }
        CLEAR_ADDR_CAM_USED(Adapter, camIdx);
#endif
        
        if (HAL_OPMODE & WIFI_AP_STATE)
        {
            HAL_VAR_VAP_COUNT--;
            for (i = 0; i < HAL_NUM_VWLAN; i++)
            {
                tmpAdapter = HAL_GET_ROOT(Adapter)->pvap_priv[i];
                if (tmpAdapter && (tmpAdapter->pmib->dot11OperationEntry.opmode & WIFI_AP_STATE) &&
                        (tmpAdapter->vap_init_seq > HAL_VAR_VAP_INIT_SEQ))
                {
                    tmpAdapter->vap_init_seq -= 1;
#ifdef HW_ENC_FOR_GROUP_CIPHER
					ToDrv_SetGTK(tmpAdapter);
#endif					
#if 0
                    GetBeaconTXBDTXDESC88XX(tmpAdapter, &pTXBD, &ptx_desc);
                    GET_HAL_INTERFACE(Adapter)->FillBeaconDescHandler(tmpAdapter, ptx_desc, tmpAdapter->beaconbuf, tmpAdapter->tx_beacon_len, 1);
#endif
                    SetDTIM_V2(tmpAdapter);
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
					tmpAdapter->beacon_updated = 0;
#endif
                }
            }

            if (HAL_RTL_R8(REG_MBID_NUM_V1)& BITS_MBID_BCN_NUM_V1)
            {
#if CFG_HAL_DUAL_BCN_BUF
                // if odd number of AP, open one more AP.
                if ((HAL_VAR_VAP_COUNT != 0) && ((HAL_VAR_VAP_COUNT % 2) == 0))
                {
                    vap_bcn_offset = HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 2);
                }
                else
#endif
                {
                    vap_bcn_offset = HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 1);
                }
                if (vap_bcn_offset > 200)
                    vap_bcn_offset = 200;

                HAL_RTL_W16(REG_BCN_SPACE_CFG,((HAL_VAR_BCN_INTERVAL / ((HAL_RTL_R8(REG_MBID_NUM_V1)& BITS_MBID_BCN_NUM_V1 & ~BIT_MASK_MBID_BCN_NUM_V1) + 1)) & BIT_MASK_BCN_SPACE) << BIT_SHIFT_BCN_SPACE);
                HAL_RTL_W8(REG_SUB_BCN_SPACE,  BIT_SUB_BCN_SPACE_V2(vap_bcn_offset));
                if (HAL_RTL_R16(REG_SUB_BCN_SPACE + 2) < 10)
                    HAL_RTL_W8(REG_DRVERLYINT_CFG,  HAL_RTL_R16(REG_SUB_BCN_SPACE + 2) - 2);
                HAL_RTL_W8(REG_PORT_CTRL_CFG, 0);
                HAL_RTL_W8(REG_PORT_CTRL_CFG, HAL_RTL_R8(REG_PORT_CTRL_CFG) | BIT_TSFTR_RST_V1);
                HAL_RTL_W8(REG_PORT_CTRL_CFG, bcn_ctrl);
            }
            HAL_RTL_W16(REG_MBSSID_CTRL_V1, (HAL_RTL_R16(REG_MBSSID_CTRL_V1) & (~(1 << (HAL_VAR_VAP_COUNT + 1)))));

            // if odd number of AP, open one more AP. add close this additional AP
#if CFG_HAL_DUAL_BCN_BUF
            if ((HAL_VAR_VAP_COUNT != 0) && (HAL_VAR_VAP_COUNT % 2) == 0)
            {
                HAL_RTL_W8(REG_MBID_NUM_V1, (HAL_RTL_R8(REG_MBID_NUM_V1)& ~BITS_MBID_BCN_NUM_V1 ) | ((HAL_VAR_VAP_COUNT + 1) & BITS_MBID_BCN_NUM_V1));
            }
            else
#endif
            {
                HAL_RTL_W8(REG_MBID_NUM_V1, (HAL_RTL_R8(REG_MBID_NUM_V1) & ~BITS_MBID_BCN_NUM_V1) | (HAL_VAR_VAP_COUNT & BITS_MBID_BCN_NUM_V1));
            }
#if 0
#if CFG_HAL_DUAL_BCN_BUF
            bcn_early_time = HAL_RTL_R8(REG_DRVERLYINT_CFG);
#else
            bcn_early_time = HAL_RTL_R8(REG_BCNDMATIM_CFG);
#endif
            if ((HAL_VAR_VAP_COUNT % 2) == 0)
            {
#if CFG_HAL_DUAL_BCN_BUF
                tbtt_hold = (HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 2)) * 2 - bcn_early_time - 2;
#else
                tbtt_hold = (HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 1))   - bcn_early_time - 2;
#endif
            }
            else
            {
                tbtt_hold = (HAL_VAR_BCN_INTERVAL / (HAL_VAR_VAP_COUNT + 1)) * 2 - bcn_early_time - 2;
            }
            if (tbtt_hold > 16)
                tbtt_hold = 16;
            HAL_RTL_W32(REG_TBTT_PROHIBIT_CFG, HAL_RTL_R8(REG_TBTT_PROHIBIT_CFG) | (((tbtt_hold * 1024 / 32)& BIT_MASK_TBTT_HOLD_TIME) << BIT_SHIFT_TBTT_HOLD_TIME));
#endif
        }

        HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) & ~BIT_ENADDRCAM);
        HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) | BIT_ENADDRCAM);

		HAL_RTL_W16(REG_CONTROL_FRAME_CNT_CTRL, BIT(8) | ((Adapter->vap_id + 1) << 4));
		HAL_RTL_W8(REG_CONTROL_FRAME_REPORT, BIT(1));

        HAL_VAR_VAP_INIT_SEQ = -1;
    }
}
#endif //IS_RTL88XX_MAC_V4

#endif // (HAL_DEV_BUS_TYPE & (HAL_RT_EMBEDDED_INTERFACE | HAL_RT_PCI_INTERFACE))
#endif  //CFG_HAL_SUPPORT_MBSSID

VOID
ClearHWTXShortcutBufHandler88XX(
    IN  HAL_PADAPTER        Adapter,
    IN  u4Byte              macID
)
{
   // clear HW TX shortCut SRAM buffer
    int lenTXSC;
    int baseReg, start_addr, shift, index, newidx, bndy0, bndy1, addr0;
    
    if(macID > 63)
    {
        printk("Error, HW TX shorcut only support 64 MACID \n");
    }

#if IS_RTL8197G_SERIES //yllin8197G need check
    if(IS_HARDWARE_TYPE_8197G(Adapter)){
        lenTXSC = 0x80;
        baseReg = 0x8400;
        bndy0 = 23;
        bndy1 = 55;
        addr0 = 0x662;
    }
#endif

#if IS_RTL8197F_SERIES
    if(IS_HARDWARE_TYPE_8197F(Adapter)){
        lenTXSC = 0x80;
        baseReg = 0x8000;
        bndy0 = 31;
        bndy1 = 64;
        addr0 = 0x662;
    }
#endif
#if IS_RTL8198F_SERIES
    if(IS_HARDWARE_TYPE_8198F(Adapter)){
        lenTXSC = 0x80;
        baseReg = 0x8400;
        bndy0 = 23;
        bndy1 = 55;
        addr0 = 0x662;
    }
#endif
#if IS_RTL8814B_SERIES
    if(IS_HARDWARE_TYPE_8814B(Adapter)){
        lenTXSC = 0x58;
        baseReg = 0x8C00;
        bndy0 = 10;
        bndy1 = 56;
        addr0 = 0x663;
        newidx = 0;
    }
#endif

    start_addr = ((baseReg+ (macID * lenTXSC))&0x8FFF);
    
    HAL_RTL_W8(0x106, 0x7F);

    if(macID > bndy1) {
        HAL_RTL_W32(REG_PKTBUF_DBG_CTRL, addr0+2);
    }
    else if(macID > bndy0){
        HAL_RTL_W32(REG_PKTBUF_DBG_CTRL, addr0+1);
    }
    else {
        HAL_RTL_W32(REG_PKTBUF_DBG_CTRL, addr0);
    }
    
    
    
    for(index = 0; index < lenTXSC; index++) {
        if((start_addr+index) & 0x9000){
            HAL_RTL_W32(REG_PKTBUF_DBG_CTRL, HAL_RTL_R32(REG_PKTBUF_DBG_CTRL)+1);
            start_addr = 0x8000;
            newidx = index;
        }
        HAL_RTL_W8(start_addr+ index - newidx,0x0);            
    }       
}

VOID
GetHwSequenceHandler88XX(
    IN	HAL_PADAPTER        Adapter,
    IN	u4Byte              macID,
    IN  u1Byte              tid, 
    OUT pu4Byte             val  
)
{
    int lenTXSC;
    int baseReg, start_addr, shift, index, bndy0, bndy1, addr0, read_addr;

    
    if(macID > 63)
    {
        printk("Error, HW TX shorcut only support 64 MACID \n");
    }
    
#if IS_RTL8197G_SERIES //yllin8197G need check
        if(IS_HARDWARE_TYPE_8197G(Adapter)){
            lenTXSC = 0x80;
            baseReg = 0x8400;
            bndy0 = 23;
            bndy1 = 55;
            addr0 = 0x662;
        }
#endif    
#if IS_RTL8197F_SERIES
        if(IS_HARDWARE_TYPE_8197F(Adapter)){
            lenTXSC = 0x80;
            baseReg = 0x8000;
            bndy0 = 31;
            bndy1 = 64;
            addr0 = 0x662;
        }
#endif
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter)){
            lenTXSC = 0x80;
            baseReg = 0x8400;
            bndy0 = 23;
            bndy1 = 55;
            addr0 = 0x662;
        }
#endif
#if IS_RTL8814B_SERIES
        if(IS_HARDWARE_TYPE_8814B(Adapter)){
            lenTXSC = 0x58;
            baseReg = 0x8C00;
            bndy0 = 10;
            bndy1 = 56;
            addr0 = 0x663;
        }
#endif
    
    start_addr = ((baseReg+ (macID * lenTXSC))&0x8FFF);
    
    HAL_RTL_W8(0x106, 0x7F);

    if(macID > bndy1) {
        HAL_RTL_W32(0x140, addr0+2);
    }
    else if(macID > bndy0){
        HAL_RTL_W32(0x140, addr0+1);
    }
    else {
        HAL_RTL_W32(0x140, addr0);
    }


	if ((tid == 0) || (tid == 3)) {
	    shift = 16; //BE
	}
	else if ((tid == 7) || (tid == 6)) {
	    shift = 0; //VO
	}
	else if ((tid == 5) || (tid == 4)) {
	    shift = 8; //VO
	}
	else{
	    shift = 24; //BK
	}

#if IS_RTL8197F_SERIES || IS_RTL8198F_SERIES || IS_RTL8197G_SERIES
    if(IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter)){
        read_addr = start_addr+0x28+shift;
    }
#endif

#if IS_RTL8814B_SERIES
    if(IS_HARDWARE_TYPE_8814B(Adapter)){
        read_addr = start_addr+shift;
    }
#endif

    if(read_addr & 0x9000) {
        HAL_RTL_W32(0x140, HAL_RTL_R32(0x140)+1);
        read_addr = read_addr & 0x8FFF;
    }
    *(val) = ((HAL_RTL_R32(read_addr)>>8)&0xFFF);


    //printk("start_addr =0x%x shift addr = 0x%x \n",start_addr, (start_addr+0x28+shift));
    //printk("R32 =0x%x shift = 0x%x after & = 0x%x \n",HAL_RTL_R32(start_addr+0x28+shift), (HAL_RTL_R32(start_addr+0x28+shift)>>8),((HAL_RTL_R32(start_addr+0x28+shift)>>8)&0xFFF));    
    //printk("macID =%d TID = %d HW seq = %d \n",macID, tid,*(val));
}

enum rt_status
PktBufAccessCtrl(
    IN	HAL_PADAPTER        Adapter,
    IN u1Byte mode,
    IN u1Byte rw,
    IN u2Byte offset, //Addr >> 3
    IN u1Byte wbit
)
{
    u4Byte bit_dbgbuf,rw_sts,value;
    u1Byte dbgbuf_mode;
	u4Byte WaitWriteLimmit=0;
          
    offset &= 0x3FFF;

#if ( HALMAC_8192F_SUPPORT ) //fix compile errors of 22b+97f
if (IS_HARDWARE_TYPE_8192FE(Adapter)) {
    switch(mode)
    {
        case DBGBUF_TXPKTBUF:
            rw_sts = (rw << BIT_SHIFT_TXPKTBUF) & BIT_TXPKTBUF_DBG ;
			dbgbuf_mode = DBGBUF_TXPKTBUF;
            bit_dbgbuf = BIT_TXPKTBUF_DBG;
            break;

        case DBGBUF_RXPKTBUF:
            rw_sts = (rw << BIT_SHIFT_RXPKTBUF) & BIT_RXPKTBUF_DBG ;
            dbgbuf_mode = DBGBUF_RXPKTBUF;
            bit_dbgbuf = BIT_RXPKTBUF_DBG;
            break;

        case DBGBUF_TXRPTBUF:
            rw_sts = (rw << BIT_SHIFT_TXRPTBUF) & BIT_TXRPTBUF_DBG_V2 ;
            dbgbuf_mode = DBGBUF_TXRPTBUF;
            bit_dbgbuf = BIT_TXRPTBUF_DBG_V2;
            break;

        default:
            return RT_STATUS_FAILURE;
    }
} else
#endif
#if (HALMAC_8192E_SUPPORT || HALMAC_8881A_SUPPORT) //fix compile errors of 22b+97f
{
    switch(mode)
    {
        case DBGBUF_TXPKTBUF:
            rw_sts = (rw << BIT_SHIFT_TXPKTBUF) & BIT_TXPKT_BUF_READ_EN ;
			dbgbuf_mode = DBGBUF_TXPKTBUF;
            bit_dbgbuf = BIT_TXPKT_BUF_READ_EN; 
            break;

        case DBGBUF_RXPKTBUF:
            rw_sts = (rw << BIT_SHIFT_RXPKTBUF) & BIT_RXPKT_BUF_READ_EN ;
            dbgbuf_mode = DBGBUF_RXPKTBUF;
            bit_dbgbuf = BIT_RXPKT_BUF_READ_EN; 
            break;

        case DBGBUF_TXRPTBUF:
            rw_sts = (rw << BIT_SHIFT_TXRPTBUF) & BIT_TXRPT_BUF_READ_EN ;
            dbgbuf_mode = DBGBUF_TXRPTBUF;
            bit_dbgbuf = BIT_TXRPT_BUF_READ_EN; 
            break;

        default:
            return RT_STATUS_FAILURE;
    }
}
#endif
    //HAL_RTL_W8(REG_PKT_BUFF_ACCESS_CTRL, dbgbuf_mode);
    HAL_RTL_W8(0x0106, dbgbuf_mode);//workaround the compile errors for 22b+97f platform

    if(rw_sts){
        // write, rw_sts =1
        HAL_RTL_W32(REG_PKTBUF_DBG_CTRL, wbit<< 24 | rw_sts | offset);
    }
    else{
		// read, rw_sts =0
        HAL_RTL_W32(REG_PKTBUF_DBG_CTRL, rw_sts | offset);
    }

	 do{
		 value = HAL_RTL_R32(REG_PKTBUF_DBG_CTRL)&bit_dbgbuf;   // rdy, write is 0, read is 1.
	 
		 if (WaitWriteLimmit++ >= 100) {
			 GDEBUG("Polling Write TXRPT fail \n");
			 return RT_STATUS_FAILURE;
		 } else if (value == rw_sts) { // when state not change, delay
			 HAL_delay_us(10); //us
		 }
		 
	  }while(value == rw_sts); 

    //HAL_RTL_W8(REG_PKTBUF_DBG_CTRL+3, 0x00);
    //HAL_RTL_W8(REG_PKT_BUFF_ACCESS_CTRL, 0);
    return RT_STATUS_SUCCESS;
}
#if CFG_HAL_HW_FILL_MACID

enum rt_status
CheckHWMACIDResult88XX(
	IN	HAL_PADAPTER        Adapter,
	IN  u4Byte              MacID,
	OUT pu1Byte             result
)
{
    if( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
        if (MacID > HW_MACID_SEARCH_SUPPORT_NUM_V1)
            *(result) = HAL_HWMACID_RESULT_FAIL;
        else
            *(result) = HAL_HWMACID_RESULT_SUCCESS;
    }
    else if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3){
	if (MacID == HW_MACID_SEARCH_FAIL)
		*(result) = HAL_HWMACID_RESULT_FAIL;
	else if (MacID == HW_MACID_SEARCH_NOT_READY)
		*(result) = HAL_HWMACID_RESULT_NOT_READY;
	else
		*(result) = HAL_HWMACID_RESULT_SUCCESS;
    }

	return RT_STATUS_SUCCESS;
}
#if IS_RTL88XX_MAC_V2_V3
VOID
InitMACIDSearch88XX(
	IN	HAL_PADAPTER        Adapter
)
{
	u1Byte i;
	for (i = 0; i < MaxMacIDNum; i++) {
		_GET_HAL_DATA(Adapter)->crc5Valid[i] = false;


		// set random value to CRC buffer
		SetCRC5ToRPTBuffer88XX(Adapter, (i % 32), i, false);
	}

	for (i = 0; i < MaxMacIDGroupNum; i++) {
		// set valid bit 0
		_GET_HAL_DATA(Adapter)->crc5groupValid[i] = false;

		SetCRC5ValidBit88XX(Adapter, i, 0);
		// set end bit 1
		SetCRC5EndBit88XX(Adapter, i, 0);
	}
}

VOID
SetCRC5ToRPTBuffer88XX(
	IN	HAL_PADAPTER        Adapter,
	IN	u1Byte              val,
	IN	u4Byte              macID,
	IN  u1Byte              bValid

)
{
	u4Byte CRC5GroupAddr;
	u4Byte CRC5Val;
	u1Byte shift, i, Valid, bEnd;
	u1Byte group;
	u1Byte Mask = 0x1f;
	u1Byte AcrossH_MSK = 0x3;
	u1Byte AcrossL_MSK = 0x7;
u4Byte CRC5Val_low,CRC5Val_hi;

#if IS_EXIST_RTL8192FE
if (IS_HARDWARE_TYPE_8192FE(Adapter)) {

	shift = (macID % macIDNumperGroup) * CRC5BitNum;
	group = macID / macIDNumperGroup;
	CRC5GroupAddr = ((CRC5_START_ADDR + group * CRC5GroupByteNum) >> 3 ) & 0xffff;

	if (CRCAcrossSHT == shift) {
		// read 8 byte based on CRC5GroupAddr.
		
		if (RT_STATUS_FAILURE ==PktBufAccessCtrl(Adapter,DBGBUF_TXRPTBUF,0,CRC5GroupAddr, 0xff))
			return RT_STATUS_FAILURE;

		CRC5Val_low = HAL_RTL_R32(REG_PKTBUF_DBG_DATA_L);
		CRC5Val_hi  = HAL_RTL_R32(REG_PKTBUF_DBG_DATA_H);

	    // modify crc5 value based on val.
		CRC5Val_low =  ((CRC5Val_low & (~(AcrossH_MSK << CRCAcrossSHT))) | ((val & AcrossH_MSK) << CRCAcrossSHT));
		CRC5Val_hi  =  ((CRC5Val_hi & (~(AcrossL_MSK << 0))) | (((val >> SecondBitSHT)&AcrossL_MSK) << 0));
				
		// write back modified crc5 value.
		HAL_RTL_W32(REG_PKTBUF_DBG_DATA_L,CRC5Val_low);
		HAL_RTL_W32(REG_PKTBUF_DBG_DATA_H,CRC5Val_hi);

		if (RT_STATUS_FAILURE ==PktBufAccessCtrl(Adapter,DBGBUF_TXRPTBUF,0x1,CRC5GroupAddr, 0xff))
			return RT_STATUS_FAILURE;

		
	} else {
		if (shift < 32) {
			// read 8 byte based on CRC5GroupAddr.
			
			if (RT_STATUS_FAILURE ==PktBufAccessCtrl(Adapter,DBGBUF_TXRPTBUF,0,CRC5GroupAddr, 0xff))
				return RT_STATUS_FAILURE;
			
			CRC5Val_low = HAL_RTL_R32(REG_PKTBUF_DBG_DATA_L);		
	
			// modify crc5 low value based on val.
			CRC5Val_low =  ((CRC5Val_low & (~(Mask << shift))) | ((val & Mask) << shift));

			// write back modified crc5 value.
			HAL_RTL_W32(REG_PKTBUF_DBG_DATA_L,CRC5Val_low);
			
			if (RT_STATUS_FAILURE ==PktBufAccessCtrl(Adapter,DBGBUF_TXRPTBUF,0x1,CRC5GroupAddr, 0x0f)) // only write low 4 bytes
				return RT_STATUS_FAILURE;
			
		} else {
			// read 8 byte based on CRC5GroupAddr.
			shift -= 32;
			if (RT_STATUS_FAILURE ==PktBufAccessCtrl(Adapter,DBGBUF_TXRPTBUF,0,CRC5GroupAddr, 0xff))
				return RT_STATUS_FAILURE;
			
			CRC5Val_hi	= HAL_RTL_R32(REG_PKTBUF_DBG_DATA_H);		

			// modify crc5 high value based on val.
		    CRC5Val_hi =  ((CRC5Val_hi & (~(Mask << shift))) | ((val & Mask) << shift));

			// write back modified crc5 value.
			HAL_RTL_W32(REG_PKTBUF_DBG_DATA_H,CRC5Val_hi);
			
			if (RT_STATUS_FAILURE ==PktBufAccessCtrl(Adapter,DBGBUF_TXRPTBUF,0x1,CRC5GroupAddr, 0xf0)) // only write high 4 bytes
				return RT_STATUS_FAILURE;
			
		}
	}

	if (bValid) {
		_GET_HAL_DATA(Adapter)->crc5Valid[macID] = true;
		_GET_HAL_DATA(Adapter)->crc5groupValid[group] = true;

		//set this group valid
		SetCRC5ValidBit88XX(Adapter, group, true);

		//set all group end bit = 0 before this MACID group
		// EX macid 12 = group1, set group0 end bit = 0
		for (i = 0; i < group; i++) {
			//GDEBUG("start SetCRC5EndBit88XX, group =%d\n",group);
			SetCRC5EndBit88XX(Adapter, i, false);
			
		}
	} else {
		_GET_HAL_DATA(Adapter)->crc5Valid[macID] = false;

		// check this group in-valid
		for (i = 0; i < macIDNumperGroup; i++) {
			if (_GET_HAL_DATA(Adapter)->crc5Valid[(group * macIDNumperGroup) + i] == true) {
				Valid = true;
				break;
			}
			Valid = false;
		}

		// check all macid are in-valid
		if (Valid == false) {
			//set this group in-valid
			_GET_HAL_DATA(Adapter)->crc5groupValid[group] = false;
			SetCRC5ValidBit88XX(Adapter, group, false);
		}

		for (i = (MaxMacIDGroupNum - 1); i >= 0; i--) {
			if (_GET_HAL_DATA(Adapter)->crc5groupValid[i] == true ) {
				SetCRC5EndBit88XX(Adapter, i, true);
				break;
			}

			if (i == 0)
				break;
		}
	}
}
else 
#endif
{

	// First set CRC buffer addr
	HAL_RTL_W32(REG_PKTBUF_DBG_CTRL, RPTBufferStartHiAddr + 1);

	shift = (macID % macIDNumperGroup) * CRC5BitNum;
	group = macID / macIDNumperGroup;
    if(IS_HARDWARE_TYPE_8198F(Adapter))
        CRC5GroupAddr = CRC5StartAddr_V1 + group * CRC5GroupByteNum;
    else
	CRC5GroupAddr = CRC5StartAddr + group * CRC5GroupByteNum;

	if (CRCAcrossSHT == shift) {
		// Write group First 32bit [31:30]
		CRC5Val = HAL_RTL_R32(CRC5GroupAddr | RWCtrlBit);
		CRC5Val =  ((CRC5Val & (~(AcrossH_MSK << CRCAcrossSHT))) | ((val & AcrossH_MSK) << CRCAcrossSHT));
		HAL_RTL_W32(CRC5GroupAddr | RWCtrlBit, CRC5Val);

		// Write group Second 32bit [2:0]
		CRC5Val = HAL_RTL_R32((CRC5GroupAddr + 4) | RWCtrlBit);
		CRC5Val =  ((CRC5Val & (~(AcrossL_MSK << 0))) | (((val >> SecondBitSHT)&AcrossL_MSK) << 0));
		HAL_RTL_W32((CRC5GroupAddr + 4) | RWCtrlBit, CRC5Val);
	} else {
		if (shift < 32) {
			// In group low 32bits
			CRC5Val = HAL_RTL_R32(CRC5GroupAddr | RWCtrlBit);
			CRC5Val =  ((CRC5Val & (~(Mask << shift))) | ((val & Mask) << shift));
			HAL_RTL_W32(CRC5GroupAddr | RWCtrlBit, CRC5Val);
		} else {
			// In group high 32bits
			shift -= 32;
			CRC5Val = HAL_RTL_R32((CRC5GroupAddr + 4) | RWCtrlBit);
			CRC5Val =  ((CRC5Val & (~(Mask << shift))) | ((val & Mask) << shift));
			HAL_RTL_W32((CRC5GroupAddr + 4) | RWCtrlBit, CRC5Val);
		}
	}

	if (bValid) {
		_GET_HAL_DATA(Adapter)->crc5Valid[macID] = true;
		_GET_HAL_DATA(Adapter)->crc5groupValid[group] = true;

		//set this group valid
		SetCRC5ValidBit88XX(Adapter, group, true);

		//set all group end bit = 0 before this MACID group
		// EX macid 12 = group1, set group0 end bit = 0
		for (i = 0; i < group; i++) {
			SetCRC5EndBit88XX(Adapter, i, false);
		}
	} else {
		_GET_HAL_DATA(Adapter)->crc5Valid[macID] = false;

		// check this group in-valid
		for (i = 0; i < macIDNumperGroup; i++) {
			if (_GET_HAL_DATA(Adapter)->crc5Valid[(group * macIDNumperGroup) + i] == true) {
				Valid = true;
				break;
			}
			Valid = false;
		}

		// check all macid are in-valid
		if (Valid == false) {
			//set this group in-valid
			_GET_HAL_DATA(Adapter)->crc5groupValid[group] = false;
			SetCRC5ValidBit88XX(Adapter, group, false);
		}

		for (i = (MaxMacIDGroupNum - 1); i >= 0; i--) {
			if (_GET_HAL_DATA(Adapter)->crc5groupValid[i] == true ) {
				SetCRC5EndBit88XX(Adapter, i, true);
				break;
			}

			if (i == 0)
				break;
		}
	}


	// set back RPT buffer start address
	HAL_RTL_W32(REG_PKTBUF_DBG_CTRL, RPTBufferStartHiAddr);
}


	
}



VOID
SetCRC5ValidBit88XX(
	IN	HAL_PADAPTER        Adapter,
	IN	u1Byte              group,
	IN  u1Byte              bValid

)
{
	u4Byte CRC5bValidAddr;
	u4Byte CRC5Val;

#if IS_EXIST_RTL8192FE
   if (IS_HARDWARE_TYPE_8192FE(Adapter)) {

   CRC5bValidAddr = ((CRC5_START_ADDR + group * CRC5GroupByteNum) >> 3 ) & 0xffff;

   PktBufAccessCtrl(Adapter,DBGBUF_TXRPTBUF,0,CRC5bValidAddr, 0xff);     

   // write back modified crc5 valid bit.
   if (bValid) {
       HAL_RTL_W32(REG_PKTBUF_DBG_DATA_H, HAL_RTL_R32(REG_PKTBUF_DBG_DATA_H) | CRC5ValidBit);

   	} else 
   	{
       HAL_RTL_W32(REG_PKTBUF_DBG_DATA_H, HAL_RTL_R32(REG_PKTBUF_DBG_DATA_H) & ~CRC5ValidBit);  	

	}
   PktBufAccessCtrl(Adapter,DBGBUF_TXRPTBUF,0x1,CRC5bValidAddr, 0xf0);  // only write high 4 bytes
   }
   else
#endif
    if(IS_HARDWARE_TYPE_8198F(Adapter))
        CRC5bValidAddr = (CRC5StartAddr_V1 + group * CRC5GroupByteNum);
    else
	CRC5bValidAddr = (CRC5StartAddr + group * CRC5GroupByteNum);
	// First set CRC buffer addr
	HAL_RTL_W32(REG_PKTBUF_DBG_CTRL, RPTBufferStartHiAddr + 1);

	if (bValid) {
		HAL_RTL_W32((RWCtrlBit | (CRC5bValidAddr + 4)), HAL_RTL_R32(RWCtrlBit | (CRC5bValidAddr + 4)) | CRC5ValidBit);
	} else
		HAL_RTL_W32((RWCtrlBit | (CRC5bValidAddr + 4)), HAL_RTL_R32(RWCtrlBit | (CRC5bValidAddr + 4)) & (~CRC5ValidBit));

	// set back RPT buffer start address
	HAL_RTL_W32(REG_PKTBUF_DBG_CTRL, RPTBufferStartHiAddr);

}

VOID
SetCRC5EndBit88XX(
	IN	HAL_PADAPTER        Adapter,
	IN	u1Byte              group,
	IN  u1Byte              bEnd
)
{
	u4Byte CRC5bEndAddr;
	u4Byte CRC5Val;
#if IS_EXIST_RTL8192FE
	if (IS_HARDWARE_TYPE_8192FE(Adapter)) {

	CRC5bEndAddr = ((CRC5_START_ADDR + group * CRC5GroupByteNum) >> 3 ) & 0xffff;

	PktBufAccessCtrl(Adapter,DBGBUF_TXRPTBUF,0,CRC5bEndAddr, 0xff);	  

// write back modified crc5 valid bit.
	  if (bEnd){
	     HAL_RTL_W32(REG_PKTBUF_DBG_DATA_H, HAL_RTL_R32(REG_PKTBUF_DBG_DATA_H) | CRC5EndBit);
	  }else {
	     HAL_RTL_W32(REG_PKTBUF_DBG_DATA_H, HAL_RTL_R32(REG_PKTBUF_DBG_DATA_H) & (~CRC5EndBit));
	  }
	  PktBufAccessCtrl(Adapter,DBGBUF_TXRPTBUF,0x1,CRC5bEndAddr, 0xf0);  // only write high 4 bytes
	}
	else
#endif
    if(IS_HARDWARE_TYPE_8198F(Adapter))
        CRC5bEndAddr = (CRC5StartAddr_V1 + group * CRC5GroupByteNum);
    else
	CRC5bEndAddr = (CRC5StartAddr + group * CRC5GroupByteNum);
	// First set CRC buffer addr
	HAL_RTL_W32(REG_PKTBUF_DBG_CTRL, RPTBufferStartHiAddr + 1);

	if (bEnd)
		HAL_RTL_W32((RWCtrlBit | (CRC5bEndAddr + 4)), HAL_RTL_R32(RWCtrlBit | (CRC5bEndAddr + 4)) | CRC5EndBit);
	else
		HAL_RTL_W32((RWCtrlBit | (CRC5bEndAddr + 4)), HAL_RTL_R32(RWCtrlBit | (CRC5bEndAddr + 4)) & (~CRC5EndBit));

	// set back RPT buffer start address
	HAL_RTL_W32(REG_PKTBUF_DBG_CTRL, RPTBufferStartHiAddr);

}
#endif //IS_RTL88XX_MAC_V2_V3
#endif //#if CFG_HAL_HW_FILL_MACID

#if CFG_HAL_RELEASE_ONE_PACKET
enum rt_status
ReleaseOnePacket88XX(
	IN  HAL_PADAPTER        Adapter,
	IN  u1Byte              macID
)
{
	unsigned char releaseBit;
	unsigned int reg_addr;
	u4Byte  value;
	u4Byte  data[2];
	u1Byte	WaitReadLimmit = 0;
    u1Byte MACID_release_sel ;

	releaseBit = macID % 32;
    MACID_release_sel = macID >> 5;

#if IS_RTL88XX_MAC_V4
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4){
        HAL_RTL_W8(REG_MACID_RELEASE_CTRL+3, (HAL_RTL_R8(REG_MACID_RELEASE_CTRL+3) & ~BIT_MASK_MACID_RELEASE_SEL) | MACID_release_sel);
        HAL_RTL_W32(REG_MACID_RELEASE_INFO, HAL_RTL_R32(REG_MACID_RELEASE_INFO) | BIT(releaseBit));
    }
#endif

#if IS_RTL88XX_MAC_V2_V3
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3){
	reg_addr = REG_MACID_RELEASE0 + (macID >> 5) * 4;

	HAL_RTL_W32(reg_addr, HAL_RTL_R32(reg_addr) | BIT(releaseBit));
    }
#endif

	do {
		value = HAL_RTL_R32(reg_addr) & BIT(releaseBit);

		if (WaitReadLimmit++ >= 100) {
			//printk("Release one packet fail \n");
			RT_TRACE_F(COMP_DBG, DBG_WARNING, ("Release one packet fail \n"));
			return RT_STATUS_FAILURE;
		} else if (value == BIT(releaseBit)) {
			HAL_delay_us(10); //us
		}
	} while (value == BIT(releaseBit));

	return RT_STATUS_SUCCESS;
}
#endif //#if CFG_HAL_RELEASE_ONE_PACKET

enum rt_status
SetTxRPTBuf88XX(
	IN	HAL_PADAPTER        Adapter,
	IN	u4Byte              macID,
	IN  u1Byte              variable,
	IN  pu1Byte             val
)
{

#if IS_RTL8814A_SERIES
	// 8814 indirect access, fill 0x140 with high addr
	//
	Ctrl_Info_8814 ctrlInfo;
	unsigned char readBuffer[16];
	u4Byte  byteOffset;
	u4Byte  Mask;
	u4Byte  Shift;
	u1Byte  ReadVal;
	u1Byte  WriteVal;
	u1Byte  i, j, page;
	u4Byte  cnt;
	u1Byte  byteLen = 0;

    #if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE )    
    if ( IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8814AE(Adapter) ) 
        HAL_RTL_W8(REG_PKT_BUFF_ACCESS_CTRL,0x7F);
    #endif

	HAL_RTL_W32(REG_PKTBUF_DBG_CTRL, RPTBufferStartHiAddr);

	switch (variable) {
	case TXRPT_VAR_DATA_RTY_LOW_RATE: {
		byteOffset = 3;
		Mask = 0x7f;
		Shift = 0x0;
	}
	break;

	case TXRPT_VAR_RTY_LOW_RATE_EN: {
		byteOffset = 3;
		Mask = 0x1;
		Shift = 0x7;
	}
	break;

	case TXRPT_VAR_RTS_RTY_LOW_RATE: {
		byteOffset = 4;
		Mask = 0x7f;
		Shift = 0x0;
	}
	break;

	case TXRPT_VAR_RTY_LMT_EN: {
		byteOffset = 4;
		Mask = 0x1;
		Shift = 0x7;
	}
	break;

	case TXRPT_VAR_DATA_RT_LMT: {
		byteOffset = 5;
		Mask = 0x3f;
		Shift = 0x0;
	}
	break;

	case TXRPT_VAR_PKT_TX_ONE_SEL: {
		byteOffset = 5;
		Mask = 0x1;
		Shift = 0x6;
	}
	break;

#if IS_RTL8814A_SERIES
	if ( IS_HARDWARE_TYPE_8814AE(Adapter)) {

		case TXRPT_VAR_MAC_ADDRESS: {
			byteOffset = 16;
			byteLen = 6;
		}
		break;
		case TXRPT_VAR_SPECIALQ_PKT_NUM1: {
			byteOffset = 14;
			Mask = 0xFF;
			Shift = 0x0;
		}
		break;
		case TXRPT_VAR_SPECIALQ_PKT_NUM2: {
			byteOffset = 15;
			Mask = 0x0F;
			Shift = 0x0;
		}
		break;
	}
#endif //#if IS_RTL8814A_SERIES

	case TXRPT_VAR_ALL: {
		// clear MACID
		for (page = 0; page < 2; page++) {
			HAL_RTL_W32(REG_PKTBUF_DBG_CTRL, RPTBufferStartHiAddr + page);
			for (cnt = 0; cnt < 0xfff; cnt++) {
				HAL_RTL_W8((cnt | RWCtrlBit), 0);
			}
		}
		return RT_STATUS_SUCCESS;
	}
	break;

	default:
		RT_TRACE_F(COMP_DBG, DBG_WARNING, ("Command ID(%d) not Supported\n", variable));
		return RT_STATUS_FAILURE;
		break;
	}

	if (byteLen) {
		for (i = 0; i < byteLen; i++) {
			WriteVal = *(val + i);
			HAL_RTL_W8((macID * ctrlInfoSZ_8814 + i + byteOffset) | RWCtrlBit, WriteVal);

			//printk("address = %x content = %x \n",(macID*ctrlInfoSZ_8814 + i + byteOffset)|RWCtrlBit,WriteVal);
		}
	} else    {
		ReadVal =  HAL_RTL_R8((macID * ctrlInfoSZ_8814 + byteOffset) | RWCtrlBit);
		WriteVal =  ((ReadVal & (~(Mask << Shift))) | ((*(val)&Mask) << Shift));

		// printk("((*(val)&Mask)<<Shift)=%x \n",((*(val)&Mask)<<Shift));
		//  printk("ReadVal =%x WriteVal = %x \n",ReadVal,WriteVal);
		HAL_RTL_W8((macID * ctrlInfoSZ_8814 + byteOffset) | RWCtrlBit, WriteVal);
	}

	GetTxRPTBuf88XX(Adapter, macID, TXRPT_VAR_ALL,0, (pu1Byte)&ctrlInfo);

#if 0
	printk("sizeof(ctrlInfo) =%x \n", sizeof(ctrlInfo));
	printk("Data_Rate =%x \n", ctrlInfo.Data_Rate);
	printk("SGI =%x \n", ctrlInfo.SGI);
	printk("PWRSTS =%x \n", ctrlInfo.PWRSTS);
	printk("DATA_BW =%x \n", ctrlInfo.DATA_BW);
	printk("Rsvd =%x \n", ctrlInfo.Rsvd);
	printk("TRY_RATE =%x \n", ctrlInfo.TRY_RATE);
	printk("PKT_DROP =%x \n", ctrlInfo.PKT_DROP);
	printk("GROUP_TABLE_ID =%x \n", ctrlInfo.GROUP_TABLE_ID);
	printk("Rsvd2 =%x \n", ctrlInfo.Rsvd2);
	printk("RTY_PKT_1SS_ANT_INFO_EN =%x \n", ctrlInfo.RTY_PKT_1SS_ANT_INFO_EN);
	printk("DATA_RTY_LOW_RATE =%x \n", ctrlInfo.DATA_RTY_LOW_RATE);
	printk("RTY_LOW_RATE_EN =%x \n", ctrlInfo.RTY_LOW_RATE_EN);
	printk("RTS_RT_LOW_RATE =%x \n", ctrlInfo.RTS_RT_LOW_RATE);
	printk("RTY_LMT_EN =%x \n", ctrlInfo.RTY_LMT_EN);
	printk("DATA_RT_LMT =%x \n", ctrlInfo.DATA_RT_LMT);
	printk("PKT_TX_ONE_SEL =%x \n", ctrlInfo.PKT_TX_ONE_SEL);
#endif
#endif // #if IS_EXIST_RTL8814AE

#if IS_RTL8198F_SERIES || IS_RTL8197G_SERIES //yllin8197G need check
    u4Byte  byteOffset;
    u1Byte  byteLen = 0;
    u4Byte  Mask;
    u4Byte  Shift;
    u1Byte  ReadVal;
    u1Byte  WriteVal;
    u1Byte  i;
    u1Byte ctrlInfoSZ_chip;

    HAL_RTL_W32(REG_PKTBUF_DBG_CTRL, RPTBufferStartHiAddr);

    switch (variable) {
        case TXRPT_VAR_MAC_ADDRESS: {
    #if IS_RTL8198F_SERIES
        if ( IS_HARDWARE_TYPE_8198F(Adapter)) {
                byteOffset = 16;
                byteLen = 6;
                ctrlInfoSZ_chip = ctrlInfoSZ_8198;
            }
        #endif
        
        #if IS_RTL8197G_SERIES
        if ( IS_HARDWARE_TYPE_8197G(Adapter)) {
                byteOffset = 16;
                byteLen = 6;
                ctrlInfoSZ_chip = ctrlInfoSZ_8197G;
        }
    #endif
        }
        break;
        
        default:
        RT_TRACE_F(COMP_DBG, DBG_WARNING, ("Command ID(%d) not Supported\n", variable));
        return RT_STATUS_FAILURE;
        break;
    }
    if(byteLen) {
        for (i = 0; i < byteLen; i++) {
            WriteVal = *(val + i);
            HAL_RTL_W8((macID * ctrlInfoSZ_chip + i + byteOffset) | RWCtrlBit, WriteVal);

            //printk("address = %x content = %x \n",(macID*ctrlInfoSZ_8814 + i + byteOffset)|RWCtrlBit,WriteVal);
        }
    } else    {
        ReadVal =  HAL_RTL_R8((macID * ctrlInfoSZ_chip + byteOffset) | RWCtrlBit);
        WriteVal =  ((ReadVal & (~(Mask << Shift))) | ((*(val)&Mask) << Shift));

        // printk("((*(val)&Mask)<<Shift)=%x \n",((*(val)&Mask)<<Shift));
        //  printk("ReadVal =%x WriteVal = %x \n",ReadVal,WriteVal);
        HAL_RTL_W8((macID * ctrlInfoSZ_chip + byteOffset) | RWCtrlBit, WriteVal);
    }

    //GetTxRPTBuf88XX(Adapter, macID, TXRPT_VAR_ALL,0, (pu1Byte)&ctrlInfo);
    
#endif
#if IS_EXIST_RTL8192FE
	// 92F indirect access(8051), fill 0x140 with high addr
if (IS_HARDWARE_TYPE_8192FE(Adapter)) {

    u2Byte offset;
	u4Byte mac_addr_low=0;
    u4Byte mac_adr_hi=0;
	u1Byte  i;
	u1Byte  byteLen = 0;

	switch (variable) {
		case TXRPT_VAR_MAC_ADDRESS: {
			byteLen = 6;
			offset = ((CTRLINFO_START_ADDR + macID * CTRLINFO_SIZE) >> 3) & 0xffff;

		if (RT_STATUS_FAILURE ==PktBufAccessCtrl(Adapter,DBGBUF_TXRPTBUF,0,offset, 0xff))
			return RT_STATUS_FAILURE;
			
            for (i = 0; i < byteLen; i++) {
				if (i<4) {
			       mac_addr_low |= (*(val + i)) << i*8 ;
				} else
				   mac_adr_hi   |= (*(val + i)) << (i-4)*8;
            }			

			mac_adr_hi = (HAL_RTL_R32(REG_PKTBUF_DBG_DATA_H) & 0xffff0000) | ((mac_adr_hi) & 0xffff);

			// set write register.
			HAL_RTL_W32(REG_PKTBUF_DBG_DATA_L,mac_addr_low);
			HAL_RTL_W32(REG_PKTBUF_DBG_DATA_H,mac_adr_hi);
			// write 8 byte 	
			if (RT_STATUS_FAILURE ==PktBufAccessCtrl(Adapter,DBGBUF_TXRPTBUF,0x1,offset, 0xff))
				return RT_STATUS_FAILURE;
		}
        break;

		case TXRPT_VAR_SWPS_RPT: {
			offset = ((SWPSRPT_START_ADDR + 16) >> 3) & 0xffff;

		if (RT_STATUS_FAILURE==PktBufAccessCtrl(Adapter,DBGBUF_TXRPTBUF,0,offset, 0xff))
			return RT_STATUS_FAILURE;
		
        if (val)
        	{
		          HAL_RTL_W32(REG_PKTBUF_DBG_DATA_L, HAL_RTL_R32(REG_PKTBUF_DBG_DATA_L) | SWPSRPTBit); 	
        	}
		else
		  HAL_RTL_W32(REG_PKTBUF_DBG_DATA_L, HAL_RTL_R32(REG_PKTBUF_DBG_DATA_L) & (~SWPSRPTBit)); 			
		
		if (RT_STATUS_FAILURE==PktBufAccessCtrl(Adapter,DBGBUF_TXRPTBUF,0x1,offset, 0x0f))  // only write low 4 bytes
			return RT_STATUS_FAILURE;
		}
        break;		

	default:
		RT_TRACE_F(COMP_DBG, DBG_WARNING, ("Command ID(%d) not Supported\n", variable));
		return RT_STATUS_FAILURE;
		break;
	}

	}

#endif
	return RT_STATUS_SUCCESS;
}

enum rt_status
GetTxRPTBuf88XX(
	IN	HAL_PADAPTER        Adapter,
	IN	u4Byte              macID,
	IN  u1Byte              variable,
	IN	u1Byte				offset,
	OUT pu1Byte             val
)
{
#if HALMAC_8192F_SUPPORT || HALMAC_8192E_SUPPORT	//fix compile error of 22b+97f
	u4Byte  addr = TXRPT_START_ADDR + macID * TXRPT_SIZE; // txrpt buffer start at 0x8100, sizeof(TXRPT) per MACID = 16bytes
#endif
	u4Byte  cnt = 0;
	u4Byte  i = 0;
	u4Byte  value;
	u4Byte  data[2];
	u1Byte	WaitReadLimmit = 0;
	u1Byte	txrpt[16];

#if IS_RTL88XX_MAC_V2
	// 8814 indirect access, fill 0x140 with high addr
	//
	Ctrl_Info ctrlInfo;
	//u1Byte Ctrl_Info_8814[32];     // txrpt 32bytes
    if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2) {

        #if IS_EXIST_RTL8814AE
        if (IS_HARDWARE_TYPE_8814AE(Adapter)) 
            HAL_RTL_W8(REG_PKT_BUFF_ACCESS_CTRL,0x7F);
        #endif

		HAL_RTL_W32(REG_PKTBUF_DBG_CTRL, RPTBufferStartHiAddr);

		data[0] = GET_DESC(HAL_RTL_R32((macID * ctrlInfoSZ + offset*8) | RWCtrlBit));
		data[1] = GET_DESC(HAL_RTL_R32(((macID * ctrlInfoSZ) + offset*8 + 4) | RWCtrlBit));

		if(offset == 0)
		{
			memcpy(&ctrlInfo, &(data[0]), 4);
			memcpy((pu1Byte)&ctrlInfo + 4, &(data[1]), 4);
		}
	}
#endif // #if IS_RTL88XX_MAC_V2

#if IS_EXIST_RTL8192EE
	if ( IS_HARDWARE_TYPE_8192EE(Adapter)) {

		addr = addr / 8;
		HAL_RTL_W8(REG_PKT_BUFF_ACCESS_CTRL, 0x7F);

		for (cnt = 0; cnt < 2; cnt++) {
			HAL_RTL_W32(REG_PKTBUF_DBG_CTRL, addr + cnt);
			value = 0;
			do {
				value = HAL_RTL_R32(REG_PKTBUF_DBG_CTRL) & BIT20;

				if (WaitReadLimmit++ >= 100) {
					printk("Polling Get TXRPT fail \n");
					RT_TRACE_F(COMP_DBG, DBG_WARNING, ("Polling TXRPT fail \n"));
					return RT_STATUS_FAILURE;
				} else if (value != BIT20) {
					HAL_delay_us(10); //us
				}
			} while (value != BIT20);

			data[0] = GET_DESC(HAL_RTL_R32(REG_PKTBUF_DBG_DATA_L));
			data[1] = GET_DESC(HAL_RTL_R32(REG_PKTBUF_DBG_DATA_H));



			for (i = 0; i < 2; i++) {
				memcpy(txrpt + (i * 4) + (cnt * 8), &(data[i]), 4);
			}
		}
	}
#endif // #if IS_EXIST_RTL8192EE

#if (IS_EXIST_RTL8192FE)
	if (IS_HARDWARE_TYPE_8192FE(Adapter)) {
		u4Byte access_addr;
			access_addr = ((addr) >> 3) & 0xffff;
	   for (cnt = 0; cnt < 2; cnt++) {
			if (RT_STATUS_FAILURE==PktBufAccessCtrl(Adapter,DBGBUF_TXRPTBUF,0,access_addr+cnt, 0xff))
				return RT_STATUS_FAILURE;			

			data[0] = HAL_RTL_R32(REG_PKTBUF_DBG_DATA_L);
			data[1] = HAL_RTL_R32(REG_PKTBUF_DBG_DATA_H);
			for (i = 0; i < 2; i++) {
				memcpy(txrpt + (i * 4) + (cnt * 8), &(data[i]), 4);
			}	
	   	}
	}
#endif // #if IS_EXIST_RTL8192FE

#if IS_EXIST_RTL8881AEM
	if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {

		for (i = 0; i < 4; i++) {
			data[0] = GET_DESC(HAL_RTL_R32(addr + i * 4));
			memcpy(txrpt + (i * 4), &(data[0]), 4);
		}
	}
#endif //#if IS_EXIST_RTL8881AEM

	switch (variable) {
	case TXRPT_VAR_PKT_DROP: {
#if (IS_EXIST_RTL8192EE || IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192FE)
		if ( IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8192FE(Adapter)) {
			*((pu1Byte)val) = (txrpt[5] & BIT2 ? 1 : 0);
		}
#endif
	}
	break;

	case TXRPT_VAR_DATA_RATE: {
#if (IS_EXIST_RTL8192FE)
		if (IS_HARDWARE_TYPE_8192FE(Adapter)) {
			*((pu1Byte)val) = (txrpt[0] & 0x7f);
		}
#endif
	}
	break;
	

	case TXRPT_VAR_ALL: {
#if IS_RTL88XX_MAC_V2
		if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2) {
			if(offset == 0){
				memcpy((pu1Byte)val, (pu1Byte)&ctrlInfo, sizeof(ctrlInfo));
			}
			else{
				memcpy(val, &(data[0]),4);
				memcpy(val+4, &(data[1]),4);
			}
		} else
#endif
		{
			memcpy(val, txrpt, 16);
		}
	}
	break;

	default:
		printk("Command ID(%d) not Supported\n", variable);
		RT_TRACE_F(COMP_DBG, DBG_WARNING, ("Command ID(%d) not Supported\n", variable));
		return RT_STATUS_FAILURE;
		break;
	}
	return RT_STATUS_SUCCESS;
}
#if IS_RTL88XX_MAC_V1_V2_V3
#if CFG_HAL_SUPPORT_MBSSID 
VOID
InitMBIDCAM88XX(
	IN  HAL_PADAPTER Adapter
)
{
	u4Byte      camData[2], camIdx;
	pu1Byte     macAddr = HAL_VAR_MY_HWADDR;
    u4Byte      mask_MBIDCAM_addr;
    u4Byte      shift_MBIDCAM_addr;
    u4Byte      bit_MBIDCAM_valid;
    
#ifdef VXD_DIFF_MAC
	if (IS_VXD_INTERFACE(Adapter)) {
		camIdx = HAL_NUM_VWLAN + 1;
		panic_printk("InitMBIDCAM88XX:%d", camIdx);
	} else
#endif
	{
	HAL_VAR_VAP_INIT_SEQ = HAL_NUM_VWLAN;
	camIdx = Adapter->vap_id + 1;
	}
#if IS_RTL8197G_SERIES
    if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
		mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR_V1;
		shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR_V1;
		bit_MBIDCAM_valid = BIT_MBIDCAM_VALID_V1;
    }
#endif  //#if IS_RTL8197G_SERIES

#if IS_RTL8198F_SERIES
    if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
		mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR_V2;
		shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR_V2;
		bit_MBIDCAM_valid = BIT_MBIDCAM_VALID_V1;
    }
#endif  //#if IS_RTL8198F_SERIES
#if (IS_RTL8197F_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
    if( IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)) {
		mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR_V1;
		shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR_V1;
		bit_MBIDCAM_valid = BIT_MBIDCAM_VALID;	
    }
#endif  // #if IS_RTL8197F_SERIES    
#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES )
    if( IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8881A(Adapter) ||IS_HARDWARE_TYPE_8814A(Adapter) ||IS_HARDWARE_TYPE_8822B(Adapter) ) {
		mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR;
		shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR;
		bit_MBIDCAM_valid = BIT_MBIDCAM_VALID;
    }
#endif //#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES)

	camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN | bit_MBIDCAM_valid |
				 (camIdx & mask_MBIDCAM_addr) << shift_MBIDCAM_addr |
				 (macAddr[5] << 8) | macAddr[4];
	camData[1] = (macAddr[3] << 24) | (macAddr[2] << 16) | (macAddr[1] << 8) | macAddr[0];
	HAL_RTL_W32(REG_MBIDCAMCFG_1, camData[1]);
	HAL_RTL_W32(REG_MBIDCAMCFG_2, camData[0]);
	
        HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) & ~BIT_ENMBID);
	HAL_RTL_W32(REG_RCR, HAL_RTL_R32(REG_RCR) | BIT_ENMBID);	// MBSSID enable
}
#endif //#if CFG_HAL_SUPPORT_MBSSID 
enum rt_status
SetMBIDCAM88XX(
	IN  HAL_PADAPTER Adapter,
	IN  u1Byte       MBID_Addr,
	IN  u1Byte       IsRoot
)
{
	s4Byte      i, j;
	u4Byte      camData[2];
	pu1Byte     macAddr = HAL_VAR_MY_HWADDR;
    u4Byte      mask_MBIDCAM_addr;
    u4Byte      shift_MBIDCAM_addr;
    u4Byte      bit_MBIDCAM_valid;

#if IS_RTL8197G_SERIES
    if( IS_HARDWARE_TYPE_8197G(Adapter) ) {
		mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR_V1;
		shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR_V1;
		bit_MBIDCAM_valid = BIT_MBIDCAM_VALID_V1;	
    }
#endif  // #if IS_RTL8197G_SERIES   

#if IS_RTL8198F_SERIES
    if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
		mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR_V2;
		shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR_V2;
		bit_MBIDCAM_valid = BIT_MBIDCAM_VALID_V1;
    }
#endif  //#if IS_RTL8198F_SERIES
#if (IS_RTL8197F_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
    if( IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)) {
		mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR_V1;
		shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR_V1;
		bit_MBIDCAM_valid = BIT_MBIDCAM_VALID;	
    }
#endif  // #if IS_RTL8197F_SERIES    
#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES )
    if( IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8881A(Adapter) ||IS_HARDWARE_TYPE_8814A(Adapter) ||IS_HARDWARE_TYPE_8822B(Adapter)) {
		mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR;
		shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR;
		bit_MBIDCAM_valid = BIT_MBIDCAM_VALID;
    }
#endif //#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES)

    camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN | bit_MBIDCAM_valid |
			 (MBID_Addr & mask_MBIDCAM_addr) << shift_MBIDCAM_addr |
				 (macAddr[5] << 8) | macAddr[4];
	camData[1] = (macAddr[3] << 24) | (macAddr[2] << 16) | (macAddr[1] << 8) | macAddr[0];

	HAL_RTL_W32(REG_MBIDCAMCFG_1, camData[1]);
	HAL_RTL_W32(REG_MBIDCAMCFG_2, camData[0]);

	if (IsRoot) {
		// clear the rest area of CAM
		camData[1] = 0;
		for (i = 1; i < 8; i++) {
            camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN | (i & mask_MBIDCAM_addr) << shift_MBIDCAM_addr;
			HAL_RTL_W32(REG_MBIDCAMCFG_1, camData[1]);
			HAL_RTL_W32(REG_MBIDCAMCFG_2, camData[0]);
		}
	}
	return RT_STATUS_SUCCESS;
}


enum rt_status
StopMBIDCAM88XX(
	IN  HAL_PADAPTER Adapter,
	IN  u1Byte       MBID_Addr
)
{
	u4Byte      camData[2];
    u4Byte      mask_MBIDCAM_addr;
    u4Byte      shift_MBIDCAM_addr;
    u4Byte      bit_MBIDCAM_valid;
	// clear the rest area of CAM

#if IS_RTL8197G_SERIES
    if( IS_HARDWARE_TYPE_8197G(Adapter) ) {
        mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR_V1;
        shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR_V1;
        bit_MBIDCAM_valid = BIT_MBIDCAM_VALID_V1;  
    }
#endif  // #if IS_RTL8197G_SERIES    

#if IS_RTL8198F_SERIES
    if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
		mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR_V2;
		shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR_V2;
		bit_MBIDCAM_valid = BIT_MBIDCAM_VALID_V1;
    }
#endif  //#if IS_RTL8198F_SERIES
#if (IS_RTL8197F_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
    if( IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)) {
		mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR_V1;
		shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR_V1;
		bit_MBIDCAM_valid = BIT_MBIDCAM_VALID;	
    }
#endif  // #if IS_RTL8197F_SERIES    
#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES)
    if( IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8881A(Adapter) ||IS_HARDWARE_TYPE_8814A(Adapter) ||IS_HARDWARE_TYPE_8822B(Adapter)) {
		mask_MBIDCAM_addr = BIT_MASK_MBIDCAM_ADDR;
		shift_MBIDCAM_addr = BIT_SHIFT_MBIDCAM_ADDR;
		bit_MBIDCAM_valid = BIT_MBIDCAM_VALID;
    }
#endif //#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES)

	camData[1] = 0;
    camData[0] = BIT_MBIDCAM_POLL | BIT_MBIDCAM_WT_EN | (MBID_Addr & mask_MBIDCAM_addr) << shift_MBIDCAM_addr;
	HAL_RTL_W32(REG_MBIDCAMCFG_1, camData[1]);
	HAL_RTL_W32(REG_MBIDCAMCFG_2, camData[0]);
	return RT_STATUS_SUCCESS;
}

#endif // #if IS_RTL88XX_MAC_V1_V2_V3

#if IS_RTL88XX_MAC_V4 && defined(ADDRESS_CAM)

VOID
InitADDRCAM88XX(
	IN  HAL_PADAPTER Adapter
)
{
    u4Byte      camData[8], write_cmd, read_cmd, i;
    u2Byte      camIdx = 0;
	pu1Byte     macAddr = HAL_VAR_MY_HWADDR;
#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
	u1Byte		vap_num = Adapter->vap_id + 1;
#else
	u1Byte		vap_num = 0;
#endif

    /* find available cam address to store */
    while(camIdx < ADDR_CAM_ENTRY_NUM)
    {
    	if (!ADDR_CAM_USED(Adapter, camIdx))
			break;
        camIdx++;
    }
	if (camIdx == ADDR_CAM_ENTRY_NUM)
		return;
    
    camData[0] =  ADDR_CAM_Valid | ADDR_CAM_Network_type_No_Link | ADDR_CAM_SET_CtrlCnt(vap_num) | ADDR_CAM_CtrlCnt_Valid;
    camData[1] = (macAddr[3] << 24) | (macAddr[2] << 16) | (macAddr[1] << 8) | macAddr[0];
    camData[2] = (macAddr[5] << 8) | macAddr[4];
    camData[3] = camData[4] = camData[5] =
    camData[6] = camData[7] = 0;

    for(i = 0; i < 8; i++)
    {
        write_cmd = BIT_MASK_ADDRCAM_RANGE | BIT_ADDRCAM_POLL | BIT__ADDRCAM_WT_EN | BIT__ADDRCAM_ADDR((camIdx*8) + i);
        read_cmd = BIT_MASK_ADDRCAM_RANGE | BIT_ADDRCAM_POLL | BIT__ADDRCAM_ADDR((camIdx*8) + i);
        HAL_RTL_W32(REG_ADDRCAM_WRITE_CONTENT, camData[i]); 
        HAL_RTL_W32(REG_ADDRCAM_CFG, write_cmd);
        HAL_RTL_W32(REG_ADDRCAM_CFG, read_cmd);
    }

	SET_ADDR_CAM_USED(Adapter, camIdx);
	Adapter->interface_addrcam_entry = camIdx;
}

enum rt_status
SetADDRCAMEntry88XX(
	IN  HAL_PADAPTER Adapter,
    IN  pu1Byte      Client_Addr
)
{
	struct rtl8192cd_priv *priv = Adapter;
    struct stat_info    *pstat;
    pu1Byte     macAddr = HAL_VAR_MY_HWADDR;
	u2Byte      camIdx = 0;
    u4Byte      camData[8], write_cmd, i;    
#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
	u1Byte		vap_num = Adapter->vap_id + 1;
#else
	u1Byte		vap_num = 0;
#endif

	if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4 == FALSE)
		return RT_STATUS_FAILURE;

	pstat = get_stainfo(priv, Client_Addr);
	if (!pstat)
		return RT_STATUS_FAILURE;
	
	/* find available ADDRCAM entry to insert */
	if (priv->interface_addrcam_entry >= 0) {
		camIdx = priv->interface_addrcam_entry;
		priv->interface_addrcam_entry = -1;
	}
	else {
		while (camIdx < ADDR_CAM_ENTRY_NUM) {
			if (!ADDR_CAM_USED(Adapter, camIdx))
				break;
			camIdx++;
		}
		if (camIdx == ADDR_CAM_ENTRY_NUM)
			return RT_STATUS_FAILURE;
	}

	/* update assoc sta ADDRCAM related info */
	pstat->addr_cam_entry = camIdx;
	SET_ADDR_CAM_USED(priv, camIdx);
        

	if (OPMODE & WIFI_STATION_STATE) {
		camData[0] =  ADDR_CAM_Valid | ADDR_CAM_Network_type_Infrastructure | ADDR_CAM_SET_CtrlCnt(vap_num) | ADDR_CAM_CtrlCnt_Valid | SET_ADDR_CAM_MACID(pstat->cmn_info.aid);
		camData[1] = (macAddr[3] << 24) | (macAddr[2] << 16) | (macAddr[1] << 8) | macAddr[0];
		camData[2] = (Client_Addr[1] << 24) | (Client_Addr[0] << 16) | (macAddr[5] << 8) | macAddr[4];
		camData[3] = (Client_Addr[5] << 24) | (Client_Addr[4] << 16) | (Client_Addr[3] << 8) | Client_Addr[2];
		camData[4] = (Client_Addr[3] << 24) | (Client_Addr[2] << 16) | (Client_Addr[1] << 8) | Client_Addr[0];
		camData[5] = (Client_Addr[5] << 8) | Client_Addr[4];
		camData[6] = camData[7] = 0;
	}
	else {
		camData[0] =  ADDR_CAM_Valid | ADDR_CAM_Network_type_AP | ADDR_CAM_SET_CtrlCnt(vap_num) | ADDR_CAM_CtrlCnt_Valid | SET_ADDR_CAM_MACID(pstat->cmn_info.aid);
		camData[1] = (macAddr[3] << 24) | (macAddr[2] << 16) | (macAddr[1] << 8) | macAddr[0];
		camData[2] = (Client_Addr[1] << 24) | (Client_Addr[0] << 16) | (macAddr[5] << 8) | macAddr[4];
		camData[3] = (Client_Addr[5] << 24) | (Client_Addr[4] << 16) | (Client_Addr[3] << 8) | Client_Addr[2];
		camData[4] = (macAddr[3] << 24) | (macAddr[2] << 16) | (macAddr[1] << 8) | macAddr[0];
		camData[5] = (macAddr[5] << 8) | macAddr[4];
		camData[6] = camData[7] = 0;
	}

	for(i = 0; i < 8; i++)
	{
		write_cmd = (BIT_MASK_ADDRCAM_RANGE | BIT_ADDRCAM_POLL | BIT__ADDRCAM_WT_EN ) | BIT__ADDRCAM_ADDR(camIdx*8 + i);
		HAL_RTL_W32(REG_ADDRCAM_WRITE_CONTENT, camData[i]);
		HAL_RTL_W32(REG_ADDRCAM_CFG, write_cmd);
	}

	return RT_STATUS_SUCCESS;
}
enum rt_status
DelADDRCAMEntry88XX(
    IN  HAL_PADAPTER Adapter,
    IN  struct stat_info *pstat
)
{
	struct rtl8192cd_priv *priv = Adapter;
    pu1Byte     macAddr = HAL_VAR_MY_HWADDR;
    u4Byte      camData[8], write_cmd, i;
	u2Byte		camIdx = 0;
    u1Byte      SSID_assoc_number = 0;              // this SSID assoc sta number
    u1Byte      ClearEn = 0;
#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
	u1Byte		vap_num = Adapter->vap_id + 1;
#else
	u1Byte		vap_num = 0;
#endif

	if (IS_HARDWARE_TYPE_8814B(Adapter) == FALSE)
		return RT_STATUS_FAILURE;

	if (!pstat)
		return RT_STATUS_FAILURE;
	
	if (priv->assoc_num == 0)
		return RT_STATUS_FAILURE;

	camIdx = pstat->addr_cam_entry;
	camData[0] = camData[1] = camData[2] = camData[3] =0;
	camData[4] = camData[5] = camData[6] = camData[7] =0;

	for(i = 0; i < 8; i++) {
		write_cmd = (BIT_MASK_ADDRCAM_RANGE | BIT_ADDRCAM_POLL | BIT__ADDRCAM_WT_EN ) | BIT__ADDRCAM_ADDR(camIdx*8 + i);
		HAL_RTL_W32(REG_ADDRCAM_WRITE_CONTENT, camData[i]); 
		HAL_RTL_W32(REG_ADDRCAM_CFG, write_cmd);
	}

	CLEAR_ADDR_CAM_USED(priv, camIdx);
	pstat->addr_cam_entry = -1;

	/* the last STA condition */
	if (priv->assoc_num == 1) {
		camIdx = 0;
		while (camIdx < ADDR_CAM_ENTRY_NUM) {
			if (!ADDR_CAM_USED(Adapter, camIdx))
				break;
			camIdx++;
		}
		if (camIdx == ADDR_CAM_ENTRY_NUM)
			return RT_STATUS_FAILURE;

		camData[0] =  ADDR_CAM_Valid | ADDR_CAM_Network_type_No_Link | ADDR_CAM_SET_CtrlCnt(vap_num) | ADDR_CAM_CtrlCnt_Valid;
		camData[1] = (macAddr[3] << 24) | (macAddr[2] << 16) | (macAddr[1] << 8) | macAddr[0];
		camData[2] = (macAddr[5] << 8) | macAddr[4];
		camData[3] = camData[4] = camData[5] =
		camData[6] = camData[7] = 0;

		for(i = 0; i < 8; i++) {
			write_cmd = (BIT_MASK_ADDRCAM_RANGE | BIT_ADDRCAM_POLL | BIT__ADDRCAM_WT_EN ) | BIT__ADDRCAM_ADDR(camIdx*8 + i);
			HAL_RTL_W32(REG_ADDRCAM_WRITE_CONTENT, camData[i]); 
			HAL_RTL_W32(REG_ADDRCAM_CFG, write_cmd);
		}

		priv->interface_addrcam_entry = camIdx;
		SET_ADDR_CAM_USED(priv, camIdx);
	}
}

#endif  //#if IS_RTL88XX_MAC_V4


enum rt_status
ResetHWForSurprise88XX(
    IN  HAL_PADAPTER Adapter
)
{
	HAL_RTL_W8(0x1c, HAL_RTL_R8(0x1c)& ~BIT(1));
	HAL_RTL_W8(0xcc, HAL_RTL_R8(0xcc) | 0x04);
	HAL_RTL_W8(REG_CR,0);
	HAL_RTL_W8(REG_SYS_FUNC_EN+1, HAL_RTL_R8(REG_SYS_FUNC_EN+1) & 0xfe);
	HAL_RTL_W8(REG_SYS_FUNC_EN+1, HAL_RTL_R8(REG_SYS_FUNC_EN+1) |1);
	HAL_RTL_W8(0xcc, HAL_RTL_R8(0xcc) & 0xfb);

    return RT_STATUS_SUCCESS;
}

#if (IS_RTL88XX_MAC_V1_V2) || IS_RTL8197G_SERIES
enum rt_status
SetMACIDSleep88XX(
	IN  HAL_PADAPTER Adapter,
	IN  BOOLEAN      bSleep,
	IN  u4Byte       aid
)
{

	if (HAL_VAR_ENABLE_MACID_SLEEP) {
		if (bSleep) {
			if (aid > MACID_REGION3_LIMIT)
				HAL_RTL_W32(REG_MACID_SLEEP3, HAL_RTL_R32(REG_MACID_SLEEP3) | BIT(aid - MACID_REGION3_LIMIT-1));
			else if (aid > MACID_REGION2_LIMIT)
				HAL_RTL_W32(REG_MACID_SLEEP2, HAL_RTL_R32(REG_MACID_SLEEP2) | BIT(aid - MACID_REGION2_LIMIT-1));
			else if (aid > MACID_REGION1_LIMIT)
				HAL_RTL_W32(REG_MACID_SLEEP1, HAL_RTL_R32(REG_MACID_SLEEP1) | BIT(aid - MACID_REGION1_LIMIT-1));
			else
				HAL_RTL_W32(REG_MACID_SLEEP, HAL_RTL_R32(REG_MACID_SLEEP) | BIT(aid));

			RT_TRACE(COMP_MLME, DBG_LOUD, ("%s %d Sleep AID = 0x%x Reg[4D4] = 0x%x \n",  __FUNCTION__,
										   __LINE__,
										   (unsigned int)aid,
										   HAL_RTL_R32(REG_MACID_SLEEP)));
		} else {
			if (aid > MACID_REGION3_LIMIT)
				HAL_RTL_W32(REG_MACID_SLEEP3, HAL_RTL_R32(REG_MACID_SLEEP3) & ~BIT(aid - MACID_REGION3_LIMIT-1));
			else if (aid > MACID_REGION2_LIMIT)
				HAL_RTL_W32(REG_MACID_SLEEP2, HAL_RTL_R32(REG_MACID_SLEEP2) & ~BIT(aid - MACID_REGION2_LIMIT-1));
			else if (aid > MACID_REGION1_LIMIT)
				HAL_RTL_W32(REG_MACID_SLEEP1, HAL_RTL_R32(REG_MACID_SLEEP1) & ~BIT(aid - MACID_REGION1_LIMIT-1));
			else
				HAL_RTL_W32(REG_MACID_SLEEP, HAL_RTL_R32(REG_MACID_SLEEP) & ~BIT(aid));
			RT_TRACE(COMP_MLME, DBG_LOUD, ("%s %d WakeUP AID = 0x%x Reg[4D4] = 0x%x \n", __FUNCTION__,
										   __LINE__,
										   (unsigned int)aid,
										   HAL_RTL_R32(REG_MACID_SLEEP)));
		}
	}

	return RT_STATUS_SUCCESS;
}
#endif
#if IS_RTL8814B_SERIES
#if 0 //macid sleep implemented by h2c to FW, and FW will handle, this is for driver/FW sync
enum rt_status
SetMACIDSleep88XX_V1(
	IN  HAL_PADAPTER Adapter,
	IN  BOOLEAN      bSleep,
	IN  u4Byte       aid

)
{
    u1Byte macid_region;
    u4Byte write_value;
    if (HAL_VAR_ENABLE_MACID_SLEEP) {
        macid_region = bSleep / 32;

        HAL_RTL_W8(REG_MACID_SLEEP_CTRL, BIT_SET_MACID_SLEEP_SEL(HAL_RTL_R8(REG_MACID_SLEEP_CTRL),macid_region));

        if (aid > MACID_REGION3_LIMIT)
            write_value = BIT(aid - MACID_REGION3_LIMIT-1);
        else if (aid > MACID_REGION2_LIMIT)
            write_value = BIT(aid - MACID_REGION2_LIMIT-1);
        else if (aid > MACID_REGION1_LIMIT)
            write_value = BIT(aid - MACID_REGION1_LIMIT-1);
        else
            write_value =  BIT(aid);
            
        if (bSleep) {
            
            HAL_RTL_W32(REG_MACID_SLEEP_INFO, HAL_RTL_R32(REG_MACID_SLEEP_INFO) | write_value);

            RT_TRACE(COMP_MLME, DBG_LOUD, ("%s %d Sleep AID = 0x%x Reg[4D4] = 0x%x \n",  __FUNCTION__,
                                           __LINE__,
                                           (unsigned int)aid,
                                           HAL_RTL_R32(REG_MACID_SLEEP_INFO)));
        } else {
            
            HAL_RTL_W32(REG_MACID_SLEEP_INFO, HAL_RTL_R32(REG_MACID_SLEEP_INFO) & ~write_value);
            
            RT_TRACE(COMP_MLME, DBG_LOUD, ("%s %d WakeUP AID = 0x%x Reg[4D4] = 0x%x \n", __FUNCTION__,
                                           __LINE__,
                                           (unsigned int)aid,
                                           HAL_RTL_R32(REG_MACID_SLEEP_INFO)));
        }
    }

    return RT_STATUS_SUCCESS;

}
#endif
#endif
#if (IS_RTL8192E_SERIES || IS_RTL8192F_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8197F_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
enum rt_status
GetMACIDQueueInTXPKTBUF88XX(
	IN      HAL_PADAPTER          Adapter,
	OUT     pu1Byte               MACIDList
)
{
	u1Byte AC_Stop;
	u1Byte result = RT_STATUS_FAILURE;
	u1Byte MacID = 0;
	u1Byte totoalPkt = 0;
	u4Byte AC_NUM = 0;
	u4Byte q_info = 0;
	u4Byte q_list = 0;


	AC_Stop = HAL_RTL_R8(REG_ACQ_STOP);
	if (AC_Stop) {
		for (q_list = 0; q_list < NUM_AC_QUEUE; q_list++) {
			if (AC_Stop & BIT(q_list)) {
				if (q_list < 4)
					q_info = HAL_RTL_R32(REG_Q0_INFO + q_list * 4);
				else
					q_info = HAL_RTL_R32(REG_Q4_INFO + (q_list % 4) * 4);

				MacID = (u1Byte)((q_info >> 25) & 0xff);

				#if (RTL8814A_SUPPORT) || (IS_RTL8822B_SERIES) || (IS_RTL8197F_SERIES) || (IS_RTL8822C_SERIES) || (IS_RTL8812F_SERIES)
				if ( IS_HARDWARE_TYPE_8814A(Adapter) || IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)) {
					if (q_list < 4)
						totoalPkt = (HAL_RTL_R16(REG_Q0_Q1_INFO + q_list*2) & 0xFFF);
					else
						totoalPkt = (HAL_RTL_R16(REG_Q4_Q5_INFO + (q_list % 4)*2) & 0xFFF);
				} else 
				#endif
				{
					totoalPkt = (u1Byte)((q_info >> 8) & 0x7f);
				}

				if (totoalPkt > 0) {
					MACIDList[q_list] = MacID;
					result = RT_STATUS_SUCCESS;
				} else {
					MACIDList[q_list] = 0;
				}
			}
		}
	}

	return result;
}
#endif //#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8197F_SERIES)

#if IS_RTL88XX_MAC_V4
enum rt_status
GetMACIDQueueInTXPKTBUF88XX_V2(
	IN      HAL_PADAPTER          Adapter,
	OUT     pu1Byte               MACIDList
)
{

    u4Byte AC_Stop;
    u4Byte AC_isEmpty;    
	u1Byte result = RT_STATUS_FAILURE;
	u1Byte MacID = 0;
	u4Byte q_info = 0;
	u4Byte q_list = 0;
    u1Byte q_info_sel = 0;
    u4Byte q_info_reg = 0;

	AC_Stop = HAL_RTL_R32(REG_ACQ_STOP_V2);
    AC_isEmpty  = HAL_RTL_R32(REG_QUEUELIST_INFO_EMPTY);

    HAL_RTL_W32(REG_QUEUELIST_ACQ_EN, (HAL_RTL_R32(REG_QUEUELIST_ACQ_EN) & ~(BIT_MASK_QINFO_MODE_BAND << BIT_SHIFT_QINFO_MODE_BAND)));

	if (AC_Stop) {
		for (q_list = 0; q_list < NUM_AC_QUEUE_V1; q_list++) {
            q_info_sel = q_list/4;
            switch(q_list % 4)
            {
            case 0:
                q_info_reg = REG_QUEUELIST_INFO0;
                break;
            case 1:
                q_info_reg = REG_QUEUELIST_INFO1;
                break;
            case 2:
                q_info_reg = REG_QUEUELIST_INFO2;
                break;
            case 3:
                q_info_reg = REG_QUEUELIST_INFO3;
                break;                
            }
            HAL_RTL_W8(REG_QUEUELIST_ACQ_EN+3, (HAL_RTL_R8(REG_QUEUELIST_ACQ_EN+3) & ~BIT_MASK_QINFO_CTRL) | q_info_sel);
			if (AC_Stop & BIT(q_list) & (~AC_isEmpty)) {
                q_info = HAL_RTL_R32(q_info_reg);

                MacID = q_info & 0xFF;

    			MACIDList[q_list] = MacID;
				result = RT_STATUS_SUCCESS;
			} else {
				MACIDList[q_list] = 0;
			}
		}
    }


	return result;
    //return RT_STATUS_SUCCESS;
}
#endif //IS_RTL88XX_MAC_V4

#if IS_RTL8198F_SERIES || IS_RTL8197G_SERIES
enum rt_status
GetMACIDQueueInTXPKTBUF88XX_V1(
	IN      HAL_PADAPTER          Adapter,
	OUT     pu1Byte               MACIDList
)
{

    u2Byte AC_Stop;
    u2Byte AC_isEmpty;    
	u1Byte result = RT_STATUS_FAILURE;
	u1Byte MacID = 0;
	u4Byte q_info = 0;
	u4Byte q_list = 0;


	AC_Stop = HAL_RTL_R16(REG_ACQ_STOP_V1);
    AC_isEmpty  = HAL_RTL_R16(REG_QUEUE_EMPTY);
    
	if (AC_Stop) {
		for (q_list = 0; q_list < NUM_AC_QUEUE_V2; q_list++) {
            HAL_RTL_W8(REG_QINFO_INDEX,q_list);
			if (AC_Stop & BIT(q_list) & (~AC_isEmpty)) {
                q_info = HAL_RTL_R32(REG_QUEUE_INFO1);

                MacID = BIT_QUEUEMACID(q_info);

    			MACIDList[q_list] = MacID;
				result = RT_STATUS_SUCCESS;
			} else {
				MACIDList[q_list] = 0;
			}
		}
    }


	return result;
    //return RT_STATUS_SUCCESS;
}
#endif //IS_RTL8198F_SERIES || IS_RTL8197G_SERIES

#if (HAL_DEV_BUS_TYPE & (HAL_RT_EMBEDDED_INTERFACE | HAL_RT_PCI_INTERFACE))
enum rt_status
StopHW88XX(
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
#if IS_EXIST_RTL8822BE || IS_EXIST_RTL8814BE
	//8822B skip this to avoid PA & LNA activate together
	//8814B skip this to avoid conflict with DataCPU
	if (!IS_HARDWARE_TYPE_8822B(Adapter) && !IS_HARDWARE_TYPE_8814B(Adapter))
#endif
	{
		//MCU reset
		HAL_RTL_W8(REG_RSV_CTRL+1, HAL_RTL_R8(REG_RSV_CTRL+1) & ~BIT0);
		HAL_RTL_W16(REG_SYS_FUNC_EN, HAL_RTL_R16(REG_SYS_FUNC_EN) & ~BIT10);
	}
	//Stop HCI DMA
	StopHCIDMAHW88XX(Adapter);

#if IS_EXIST_RTL8192EE
	if ( IS_HARDWARE_TYPE_8192EE(Adapter) ) {
		HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK,
								PWR_INTF_PCI_MSK, rtl8192E_enter_lps_flow);
	}
#endif  //IS_EXIST_RTL8192EE

#if IS_EXIST_RTL8192FE
	if ( IS_HARDWARE_TYPE_8192FE(Adapter) ) {
		HAL_RTL_W8(REG_RF_CTRL, 0x0);
		HAL_RTL_W8(REG_8051FW_CTRL, 0x0);
		HAL_RTL_W32(0x39c, 0x3fffffff);
		
		HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK,
								PWR_INTF_PCI_MSK, rtl8192F_enter_lps_flow);
	}
#endif //IS_EXIST_RTL8192FE

#if IS_EXIST_RTL8881AEM
	if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
		HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK,
								PWR_INTF_PCI_MSK, rtl8881A_enter_lps_flow);
	}
#endif  //IS_EXIST_RTL8881AEM

#if IS_EXIST_RTL8814AE
	if ( IS_HARDWARE_TYPE_8814AE(Adapter) ) {
		HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK,
								PWR_INTF_PCI_MSK, rtl8814A_enter_lps_flow);
	}
#endif  //IS_EXIST_RTL8814AE

#if IS_EXIST_RTL8192EE
	if ( IS_HARDWARE_TYPE_8192EE(Adapter) ) {
		RT_TRACE(COMP_INIT, DBG_LOUD, ("rtl8192E_card_disable_flow\n"));
		HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8192E_card_disable_flow);
	}
#endif  //IS_EXIST_RTL8192EE

#if IS_EXIST_RTL8192FE
	if ( IS_HARDWARE_TYPE_8192FE(Adapter) ) {
		RT_TRACE(COMP_INIT, DBG_LOUD, ("rtl8192F_card_disable_flow\n"));
		HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8192F_card_disable_flow);
	}
#endif  //IS_EXIST_RTL8192FE

#if IS_EXIST_RTL8881AEM
	if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
		RT_TRACE(COMP_INIT, DBG_LOUD, ("rtl8881A_card_disable_flow\n"));
		HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8881A_card_disable_flow);
	}
#endif  //IS_EXIST_RTL8881AEM

#if IS_EXIST_RTL8814AE
	if ( IS_HARDWARE_TYPE_8814AE(Adapter) ) {
		RT_TRACE(COMP_INIT, DBG_LOUD, ("RTL8814AE_card_disable_flow\n"));
		HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8814A_card_disable_flow);
	}
#endif  //IS_EXIST_RTL8814AE

#if IS_EXIST_RTL8197FEM
        if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
            RT_TRACE(COMP_INIT, DBG_LOUD, ("RTL8197F_card_disable_flow\n"));
            HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8197F_card_disable_flow);
        }
#endif  //IS_EXIST_RTL8197FEM

#if IS_EXIST_RTL8198FEM
        if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
            RT_TRACE(COMP_INIT, DBG_LOUD, ("RTL8198F_card_disable_flow\n"));
            HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8198F_card_disable_flow);
        }
#endif  //IS_EXIST_RTL8198FEM

#if IS_EXIST_RTL8197GEM
        if ( IS_HARDWARE_TYPE_8197G(Adapter) ) {
            RT_TRACE(COMP_INIT, DBG_LOUD, ("RTL8197G_card_disable_flow\n"));
            HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8197G_card_disable_flow);
        }
#endif  //IS_EXIST_RTL8197GEM

#if IS_EXIST_RTL8814BE

        #if 0
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
            //HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8822B_enter_lps_flow);
            HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8814B_card_disable_flow);
        }        
        #else //use api to test first
        if ( IS_HARDWARE_TYPE_8814B(Adapter) ){
            if ( HALMAC_RET_SUCCESS != GET_MACHAL_API(Adapter)->halmac_mac_power_switch(Adapter->pHalmac_adapter,HALMAC_MAC_POWER_OFF)) {
    			panic_printk("InitPOff Failed\n");
              	return RT_STATUS_FAILURE;
    		} else {
    			printk("InitPOFF OK\n");                
    		}     
        }
        #endif
        
		return RT_STATUS_SUCCESS;
#endif  //IS_EXIST_RTL8814BE


#if IS_EXIST_RTL8822BE

        if ( IS_HARDWARE_TYPE_8822B(Adapter) ) {
            //HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8822B_enter_lps_flow);
            HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8822B_card_disable_flow);
        }        
        #if 0            
            if ( HALMAC_RET_SUCCESS != GET_MACHAL_API(Adapter)->halmac_mac_power_switch(Adapter->pHalmac_adapter,HALMAC_MAC_POWER_OFF)) {
    			panic_printk("InitPOff Failed\n");
              	return RT_STATUS_FAILURE;
    		} else {
    			printk("InitPOFF OK\n");                
    		}     
        }
        #endif
		return RT_STATUS_SUCCESS;
#endif  //IS_EXIST_RTL8822BE

#if IS_EXIST_RTL8822CE

        if ( IS_HARDWARE_TYPE_8822C(Adapter) ) {
            //HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8822C_enter_lps_flow);
            HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8822C_card_disable_flow);
        }        
        #if 0            
            if ( HALMAC_RET_SUCCESS != GET_MACHAL_API(Adapter)->halmac_mac_power_switch(Adapter->pHalmac_adapter,HALMAC_MAC_POWER_OFF)) {
    			panic_printk("InitPOff Failed\n");
              	return RT_STATUS_FAILURE;
    		} else {
    			printk("InitPOFF OK\n");                
    		}     
        }
        #endif
		return RT_STATUS_SUCCESS;
#endif  //IS_EXIST_RTL8822CE

#if IS_EXIST_RTL8812FE

        if ( IS_HARDWARE_TYPE_8812F(Adapter) ) {
            //HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8812F_enter_lps_flow);
            HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8812F_card_disable_flow);
        }        
        #if 0            
            if ( HALMAC_RET_SUCCESS != GET_MACHAL_API(Adapter)->halmac_mac_power_switch(Adapter->pHalmac_adapter,HALMAC_MAC_POWER_OFF)) {
    			panic_printk("InitPOff Failed\n");
              	return RT_STATUS_FAILURE;
    		} else {
    			printk("InitPOFF OK\n");                
    		}     
        }
        #endif
		return RT_STATUS_SUCCESS;
#endif  //IS_EXIST_RTL8812FE

#if IS_EXIST_RTL8821CE

        if ( IS_HARDWARE_TYPE_8821C(Adapter) ) {
            //HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8822B_enter_lps_flow);
            HalPwrSeqCmdParsing88XX(Adapter, PWR_CUT_ALL_MSK, PWR_FAB_ALL_MSK, PWR_INTF_PCI_MSK, rtl8821C_card_disable_flow);
        }        
        #if 0            
            if ( HALMAC_RET_SUCCESS != GET_MACHAL_API(Adapter)->halmac_mac_power_switch(Adapter->pHalmac_adapter,HALMAC_MAC_POWER_OFF)) {
    			panic_printk("InitPOff Failed\n");
              	return RT_STATUS_FAILURE;
    		} else {
    			printk("InitPOFF OK\n");                
    		}     
        }
        #endif
		return RT_STATUS_SUCCESS;
#endif  //IS_EXIST_RTL8821CE


	// Reset IO Wraper
	HAL_RTL_W8(REG_RSV_CTRL + 1, HAL_RTL_R8(REG_RSV_CTRL + 1) & ~BIT(3));
	HAL_RTL_W8(REG_RSV_CTRL + 1, HAL_RTL_R8(REG_RSV_CTRL + 1) | BIT(3));

	HAL_RTL_W8(REG_RSV_CTRL, 0x0e);                // lock ISO/CLK/Power control register

	return RT_STATUS_SUCCESS;
}
#endif // (HAL_DEV_BUS_TYPE & (HAL_RT_EMBEDDED_INTERFACE | HAL_RT_PCI_INTERFACE))


enum rt_status
StopSW88XX(
	IN  HAL_PADAPTER Adapter
)
{
	HAL_DATA_TYPE	*pHalData   = _GET_HAL_DATA(Adapter);

	//Firmware
	pHalData->H2CBufPtr88XX     = 0;
	pHalData->bFWReady           = _FALSE;

	//HCI
#if IS_EXIST_PCI || IS_EXIST_EMBEDDED
	if (Adapter->hci_type == RTL_HCI_PCIE) {
		StopHCIDMASW88XX(Adapter);
	}
#endif

	return RT_STATUS_SUCCESS;
}


VOID
DisableVXDAP88XX(
	IN  HAL_PADAPTER Adapter
)
{
	HAL_DATA_TYPE	*pHalData   = _GET_HAL_DATA(Adapter);
	RT_OP_MODE      OP_Mode     = RT_OP_MODE_NO_LINK;

#if IS_EXIST_PCI || IS_EXIST_EMBEDDED
	if (Adapter->hci_type == RTL_HCI_PCIE) {
		pHalData->IntMask[0] &= ~(BIT_BCNDMAINT0 | BIT_TXBCN0OK | BIT_TXBCN0ERR);
		HAL_RTL_W32(REG_HIMR0, pHalData->IntMask[0]);
	}
#endif

	GET_HAL_INTERFACE(Adapter)->SetHwRegHandler(Adapter, HW_VAR_MEDIA_STATUS, (pu1Byte)&OP_Mode);
}


VOID
Timer1Sec88XX(
	IN  HAL_PADAPTER Adapter
)
{
#if CFG_HAL_MACDM
	GET_HAL_INTERFACE(Adapter)->Timer1SecDMHandler(Adapter);
#endif //CFG_HAL_MACDM
}

u4Byte
CheckHang88XX(
	IN  HAL_PADAPTER        Adapter
)
{
	u4Byte hang_state = HANG_VAR_NORMAL;
	u4Byte value32;
	u1Byte stateVal;

#if IS_RTL8881A_SERIES
	if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {

		value32 = HAL_RTL_R32(REG_LX_DMA_DBG);
		if ((value32 & BIT_HD_SIZE_ERR) == BIT_HD_SIZE_ERR) { // Tx Desc Len < 40 byte
			//panic_printk("%s(%d): Tx desc len < 40 byte: 0x3f0:0x%x !!! \n", __FUNCTION__, __LINE__, value32);
			// write BIT_HD_SIZE_ERR to resolve tx hang
			HAL_RTL_W32(REG_LX_DMA_DBG, value32 | BIT_HD_SIZE_ERR);
			hang_state = HANG_VAR_TX_DESC_LEN_ERROR;
			return hang_state;
		}

		// write 0x3F0[0] = 1 to enable debug message
		HAL_RTL_W8(REG_LX_DMA_DBG, HAL_RTL_R8(REG_LX_DMA_DBG) | BIT0);
		value32 = HAL_RTL_R32(REG_LX_DMA_DBG);

		if ((value32 & BIT_TXDMA_STUCK) == BIT_TXDMA_STUCK) { // Tx hang
			//panic_printk("%s(%d): Tx hang: 0x3f0:0x%x !!! \n", __FUNCTION__, __LINE__, value32);
			hang_state = HANG_VAR_TX_STUCK;
		} else if ((value32 & BIT_RXDMA_STUCK) == BIT_RXDMA_STUCK) { // Rx hang
			//panic_printk("%s(%d): Rx hang: 0x3f0:0x%x !!! \n", __FUNCTION__, __LINE__, value32);
			hang_state = HANG_VAR_RX_STUCK;
		}
	}
#endif  //IS_RTL8881A_SERIES

#if IS_RTL8814A_SERIES
	if ( IS_HARDWARE_TYPE_8814AE(Adapter) ) {
#if 0		
        // Case 1: HT hang
		stateVal = HAL_RTL_R8(REG_STATE_MON);
		if (stateVal == 0x3)
			stateCNT++;
		else
			stateCNT = 0;

		if (stateCNT == 0x3) {
            // TODO: mark the reset mechanism for testing 8814A MP ok or not ...
            //HAL_RTL_W8(REG_STATE_MON,0x80);
            stateCNT = 0;
            printk("HT hang reset state machine MP\n");
		}
#endif
        // Case 2: PCIe TX hang
        HAL_RTL_W8(0x3f3, 0x4);

		if (HAL_RTL_R8(0x3f3) == 0x5) {
			hang_state = HANG_VAR_TX_STUCK; 
			printk("PCIE TX hang ! 0x3f3 =%x \n", HAL_RTL_R8(0x3f3));
		}

        // Case 3: MAC protocol TX hang
        stateVal = HAL_RTL_R8(0x4e2);
		if (stateVal != 0x0) {
			if(stateVal & BIT5)
				Adapter->check_cnt_2s_notx++;
			//printk("MAC protocol TX hang ! 0x4e2=%x \n", stateVal);

			//HAL_RTL_W8(0x4e2, 0x20); // Write one clean. 0x4e2=0x20:  Tx hang
		}
	}
#endif //IS_RTL8814A_SERIES

#if (IS_RTL8192E_SERIES || IS_RTL8822B_SERIES || IS_RTL8192F_SERIES || IS_RTL8812F_SERIES)
	if (IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8822BE(Adapter) || 
		IS_HARDWARE_TYPE_8192FE(Adapter) || IS_HARDWARE_TYPE_8812FE(Adapter)) {
		// write 0x3f0[26]=1 to enable PCIe stuck debug
		HAL_RTL_W8(0x3f3, 0x4);

		stateVal = HAL_RTL_R8(0x3f3);
		if (stateVal & BIT(0)) {
			hang_state = HANG_VAR_TX_STUCK; 			
			printk("TX PCIe stuck! 0x3f3=%x\n", stateVal);
		} else if (stateVal & BIT(1)) {
			hang_state = HANG_VAR_RX_STUCK;
			printk("RX PCIe stuck! 0x3f3=%x\n", stateVal);
		}			
	}
#endif // (IS_RTL8192E_SERIES || IS_RTL8822B_SERIES)

	return hang_state;
}



