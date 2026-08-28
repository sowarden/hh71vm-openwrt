#ifndef __HAL8812F_DEF_H__
#define __HAL8812F_DEF_H__

/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	Hal8812FDef.h
	
Abstract:
	Defined HAL 8812F data structure & Define
	    
Major Change History:
	When       Who               What
	---------- ---------------   -------------------------------
	2015/06/25 Eric            Create.	
--*/

// Test Chip
extern u1Byte *data_AGC_TAB_8812F_start,    *data_AGC_TAB_8812F_end;
extern u1Byte *data_MAC_REG_8812F_start,    *data_MAC_REG_8812F_end;
extern u1Byte *data_PHY_REG_8812F_start,    *data_PHY_REG_8812F_end;
//extern u1Byte *data_PHY_REG_1T_8812F_start, *data_PHY_REG_1T_8812F_end;
extern u1Byte *data_PHY_REG_MP_8812F_start, *data_PHY_REG_MP_8812F_end;
extern u1Byte *data_PHY_REG_PG_8812F_start, *data_PHY_REG_PG_8812F_end;
extern u1Byte *data_RadioA_8812F_start,     *data_RadioA_8812F_end;
extern u1Byte *data_RadioB_8812F_start,     *data_RadioB_8812F_end;


//High Power
#if CFG_HAL_HIGH_POWER_EXT_PA
extern u1Byte *data_AGC_TAB_8812F_hp_start,    *data_AGC_TAB_8812F_hp_end;
extern u1Byte *data_PHY_REG_8812F_hp_start,    *data_PHY_REG_8812F_hp_end;
extern u1Byte *data_RadioA_8812F_hp_start,     *data_RadioA_8812F_hp_end;
extern u1Byte *data_RadioB_8812F_hp_start,     *data_RadioB_8812F_hp_end;
#endif

// FW
extern u1Byte *data_rtl8812Ffw_start,         *data_rtl8812Ffw_end;
extern u1Byte *data_rtl8812FfwMP_start,       *data_rtl8812FfwMP_end;

// Power Tracking
extern u1Byte *data_TxPowerTrack_AP_8812F_start,    *data_TxPowerTrack_AP_8812F_end;


//3 MACDM
//default
extern u1Byte *data_MACDM_def_high_8812F_start, *data_MACDM_def_high_8812F_end;
extern u1Byte *data_MACDM_def_low_8812F_start, *data_MACDM_def_low_8812F_end;
extern u1Byte *data_MACDM_def_normal_8812F_start, *data_MACDM_def_normal_8812F_end;

//general
extern u1Byte *data_MACDM_gen_high_8812F_start, *data_MACDM_gen_high_8812F_end;
extern u1Byte *data_MACDM_gen_low_8812F_start, *data_MACDM_gen_low_8812F_end;
extern u1Byte *data_MACDM_gen_normal_8812F_start, *data_MACDM_gen_normal_8812F_end;

//txop
extern u1Byte *data_MACDM_txop_high_8812F_start, *data_MACDM_txop_high_8812F_end;
extern u1Byte *data_MACDM_txop_low_8812F_start, *data_MACDM_txop_low_8812F_end;
extern u1Byte *data_MACDM_txop_normal_8812F_start, *data_MACDM_txop_normal_8812F_end;

//criteria
extern u1Byte *data_MACDM_state_criteria_8812F_start, *data_MACDM_state_criteria_8812F_end;


// MP chip 
extern u1Byte *data_AGC_TAB_8812Fmp_start,    *data_AGC_TAB_8812Fmp_end;
extern u1Byte *data_MAC_REG_8812Fmp_start,    *data_MAC_REG_8812Fmp_end;
extern u1Byte *data_PHY_REG_8812Fmp_start,    *data_PHY_REG_8812Fmp_end;
//extern u1Byte *data_PHY_REG_1T_8812F_start, *data_PHY_REG_1T_8812F_end;
extern u1Byte *data_PHY_REG_MP_8812Fmp_start, *data_PHY_REG_MP_8812Fmp_end;
extern u1Byte *data_PHY_REG_PG_8812Fmp_start, *data_PHY_REG_PG_8812Fmp_end;
extern u1Byte *data_PHY_REG_PG_8812Fmp_Type0_start, *data_PHY_REG_PG_8812Fmp_Type0_end;
extern u1Byte *data_PHY_REG_PG_8812Fmp_Type2_start, *data_PHY_REG_PG_8812Fmp_Type2_end;
extern u1Byte *data_PHY_REG_PG_8812Fmp_Type8_start, *data_PHY_REG_PG_8812Fmp_Type8_end;
extern u1Byte *data_PHY_REG_PG_8812Fmp_Type9_start, *data_PHY_REG_PG_8812Fmp_Type9_end;
extern u1Byte *data_PHY_REG_PG_8812Fmp_Type10_start, *data_PHY_REG_PG_8812Fmp_Type10_end;
extern u1Byte *data_PHY_REG_PG_8812Fmp_Type13_start, *data_PHY_REG_PG_8812Fmp_Type13_end;
extern u1Byte *data_PHY_REG_PG_8812Fmp_Type14_start, *data_PHY_REG_PG_8812Fmp_Type14_end;
extern u1Byte *data_RadioA_8812Fmp_start,     *data_RadioA_8812Fmp_end;
extern u1Byte *data_RadioB_8812Fmp_start,     *data_RadioB_8812Fmp_end;

//High Power
#if CFG_HAL_HIGH_POWER_EXT_PA
extern u1Byte *data_AGC_TAB_8812Fmp_hp_start,    *data_AGC_TAB_8812Fmp_hp_end;
extern u1Byte *data_PHY_REG_8812Fmp_hp_start,    *data_PHY_REG_8812Fmp_hp_end;
//extern u1Byte *data_PHY_REG_PG_8812Fmp_hp_start, *data_PHY_REG_PG_8812Fmp_hp_end;
extern u1Byte *data_PHY_REG_PG_8812Fmp_Type1_start, *data_PHY_REG_PG_8812Fmp_Type1_end;
extern u1Byte *data_PHY_REG_PG_8812Fmp_Type3_start, *data_PHY_REG_PG_8812Fmp_Type3_end;
extern u1Byte *data_PHY_REG_PG_8812Fmp_Type4_start, *data_PHY_REG_PG_8812Fmp_Type4_end;
extern u1Byte *data_PHY_REG_PG_8812Fmp_Type6_start, *data_PHY_REG_PG_8812Fmp_Type6_end;
extern u1Byte *data_PHY_REG_PG_8812Fmp_Type7_start, *data_PHY_REG_PG_8812Fmp_Type7_end;
extern u1Byte *data_PHY_REG_PG_8812Fmp_Type11_start, *data_PHY_REG_PG_8812Fmp_Type11_end;
extern u1Byte *data_RadioA_8812Fmp_hp_start,     *data_RadioA_8812Fmp_hp_end;
extern u1Byte *data_RadioB_8812Fmp_hp_start,     *data_RadioB_8812Fmp_hp_end;
#endif

// FW
extern u1Byte *data_rtl8812Ffw_start,         *data_rtl8812Ffw_end;
extern u1Byte *data_rtl8812FfwMP_start,       *data_rtl8812FfwMP_end;

// Power Tracking
extern u1Byte *data_TxPowerTrack_AP_8812Fmp_start,    *data_TxPowerTrack_AP_8812Fmp_end;

#ifdef TXPWR_LMT_8812F
extern unsigned char *data_TXPWR_LMT_8812Fmp_start, *data_TXPWR_LMT_8812Fmp_end;
extern unsigned char *data_TXPWR_LMT_8812Fmp_Type0_start, *data_TXPWR_LMT_8812Fmp_Type0_end;
extern unsigned char *data_TXPWR_LMT_8812Fmp_Type2_start, *data_TXPWR_LMT_8812Fmp_Type2_end;
extern unsigned char *data_TXPWR_LMT_8812Fmp_Type3_start, *data_TXPWR_LMT_8812Fmp_Type3_end;
extern unsigned char *data_TXPWR_LMT_8812Fmp_Type8_start, *data_TXPWR_LMT_8812Fmp_Type8_end;
extern unsigned char *data_TXPWR_LMT_8812Fmp_Type9_start, *data_TXPWR_LMT_8812Fmp_Type9_end;
extern unsigned char *data_TXPWR_LMT_8812Fmp_Type10_start, *data_TXPWR_LMT_8812Fmp_Type10_end;
extern unsigned char *data_TXPWR_LMT_8812Fmp_Type13_start, *data_TXPWR_LMT_8812Fmp_Type13_end;
extern unsigned char *data_TXPWR_LMT_8812Fmp_Type14_start, *data_TXPWR_LMT_8812Fmp_Type14_end;
#if CFG_HAL_HIGH_POWER_EXT_PA
extern unsigned char *data_TXPWR_LMT_8812Fmp_Type1_start, *data_TXPWR_LMT_8812Fmp_Type1_end;
extern unsigned char *data_TXPWR_LMT_8812Fmp_Type4_start, *data_TXPWR_LMT_8812Fmp_Type4_end;
extern unsigned char *data_TXPWR_LMT_8812Fmp_Type6_start, *data_TXPWR_LMT_8812Fmp_Type6_end;
extern unsigned char *data_TXPWR_LMT_8812Fmp_Type7_start, *data_TXPWR_LMT_8812Fmp_Type7_end;
extern unsigned char *data_TXPWR_LMT_8812Fmp_Type11_start, *data_TXPWR_LMT_8812Fmp_Type11_end;
#endif
#ifdef BEAMFORMING_AUTO
extern unsigned char *data_TXPWR_LMT_8812Fmp_TXBF_start, *data_TXPWR_LMT_8812Fmp_TXBF_end;
#endif
#endif



//3 MACDM
//default
extern u1Byte *data_MACDM_def_high_8812Fmp_start, *data_MACDM_def_high_8812Fmp_end;
extern u1Byte *data_MACDM_def_low_8812Fmp_start, *data_MACDM_def_low_8812Fmp_end;
extern u1Byte *data_MACDM_def_normal_8812Fmp_start, *data_MACDM_def_normal_8812Fmp_end;

//general
extern u1Byte *data_MACDM_gen_high_8812Fmp_start, *data_MACDM_gen_high_8812Fmp_end;
extern u1Byte *data_MACDM_gen_low_8812Fmp_start, *data_MACDM_gen_low_8812Fmp_end;
extern u1Byte *data_MACDM_gen_normal_8812Fmp_start, *data_MACDM_gen_normal_8812Fmp_end;

//txop
extern u1Byte *data_MACDM_txop_high_8812Fmp_start, *data_MACDM_txop_high_8812Fmp_end;
extern u1Byte *data_MACDM_txop_low_8812Fmp_start, *data_MACDM_txop_low_8812Fmp_end;
extern u1Byte *data_MACDM_txop_normal_8812Fmp_start, *data_MACDM_txop_normal_8812Fmp_end;

//criteria
extern u1Byte *data_MACDM_state_criteria_8812Fmp_start, *data_MACDM_state_criteria_8812Fmp_end;

#endif  //__HAL8812F_DEF_H__


