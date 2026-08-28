/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	Hal8812FGen.c
	
Abstract:
	Defined RTL8812F HAL Function
	    
Major Change History:
	When       Who               What
	---------- ---------------   -------------------------------
	2015-06-25 Eric             Create.	
--*/

#ifdef __ECOS
#include "../../HalPrecomp.h"
#else
#include "HalPrecomp.h"
#endif

// TestChip
#include "../../../data_AGC_TAB_8812F.c"
#include "../../../data_MAC_REG_8812F.c"
#include "../../../data_PHY_REG_8812F.c"
#include "../../../data_PHY_REG_MP_8812F.c"
#include "../../../data_PHY_REG_PG_8812F.c"
#include "../../../data_RadioA_8812F.c"
#include "../../../data_RadioB_8812F.c"
#include "../../../data_rtl8812Ffw.c"

// High Power
#if CFG_HAL_HIGH_POWER_EXT_PA
#include "../../../data_AGC_TAB_8812F_hp.c"
#include "../../../data_PHY_REG_8812F_hp.c"
#include "../../../data_RadioA_8812F_hp.c"
#include "../../../data_RadioB_8812F_hp.c"
#endif

// Power Tracking
#include "../../../data_TxPowerTrack_AP_8812F.c"


//3 MACDM
//default
#include "../../../data_MACDM_def_high_8812F.c"
#include "../../../data_MACDM_def_low_8812F.c"
#include "../../../data_MACDM_def_normal_8812F.c"
//general
#include "../../../data_MACDM_gen_high_8812F.c"
#include "../../../data_MACDM_gen_low_8812F.c"
#include "../../../data_MACDM_gen_normal_8812F.c"
//txop
#include "../../../data_MACDM_txop_high_8812F.c"
#include "../../../data_MACDM_txop_low_8812F.c"
#include "../../../data_MACDM_txop_normal_8812F.c"
//criteria
#include "../../../data_MACDM_state_criteria_8812F.c"


#define VAR_MAPPING(dst,src) \
	u1Byte *data_##dst##_start = &data_##src[0]; \
	u1Byte *data_##dst##_end   = &data_##src[sizeof(data_##src)];

VAR_MAPPING(AGC_TAB_8812F, AGC_TAB_8812F);
VAR_MAPPING(MAC_REG_8812F, MAC_REG_8812F);
VAR_MAPPING(PHY_REG_8812F, PHY_REG_8812F);
//VAR_MAPPING(PHY_REG_1T_8812F, PHY_REG_1T_8812F);
VAR_MAPPING(PHY_REG_PG_8812F, PHY_REG_PG_8812F);
VAR_MAPPING(PHY_REG_MP_8812F, PHY_REG_MP_8812F);
VAR_MAPPING(RadioA_8812F, RadioA_8812F);
VAR_MAPPING(RadioB_8812F, RadioB_8812F);
VAR_MAPPING(rtl8812Ffw, rtl8812Ffw);

// High Power
#if CFG_HAL_HIGH_POWER_EXT_PA
VAR_MAPPING(AGC_TAB_8812F_hp, AGC_TAB_8812F_hp);
VAR_MAPPING(PHY_REG_8812F_hp, PHY_REG_8812F_hp);
VAR_MAPPING(RadioA_8812F_hp, RadioA_8812F_hp);
VAR_MAPPING(RadioB_8812F_hp, RadioB_8812F_hp);
#endif

// Power Tracking
VAR_MAPPING(TxPowerTrack_AP_8812F, TxPowerTrack_AP_8812F);

#ifdef TXPWR_LMT_8812F
#include "../../../data_TXPWR_LMT_8812Fmp.c"
VAR_MAPPING(TXPWR_LMT_8812Fmp,TXPWR_LMT_8812Fmp);
#include "../../../data_TXPWR_LMT_8812Fmp_Type0.c"
VAR_MAPPING(TXPWR_LMT_8812Fmp_Type0,TXPWR_LMT_8812Fmp_Type0);
#include "../../../data_TXPWR_LMT_8812Fmp_Type2.c"
VAR_MAPPING(TXPWR_LMT_8812Fmp_Type2,TXPWR_LMT_8812Fmp_Type2);
#include "../../../data_TXPWR_LMT_8812Fmp_Type8.c"
VAR_MAPPING(TXPWR_LMT_8812Fmp_Type8,TXPWR_LMT_8812Fmp_Type8);
#include "../../../data_TXPWR_LMT_8812Fmp_Type9.c"
VAR_MAPPING(TXPWR_LMT_8812Fmp_Type9,TXPWR_LMT_8812Fmp_Type9);
#include "../../../data_TXPWR_LMT_8812Fmp_Type10.c"
VAR_MAPPING(TXPWR_LMT_8812Fmp_Type10,TXPWR_LMT_8812Fmp_Type10);
#include "../../../data_TXPWR_LMT_8812Fmp_Type13.c"
VAR_MAPPING(TXPWR_LMT_8812Fmp_Type13,TXPWR_LMT_8812Fmp_Type13);
#include "../../../data_TXPWR_LMT_8812Fmp_Type14.c"
VAR_MAPPING(TXPWR_LMT_8812Fmp_Type14,TXPWR_LMT_8812Fmp_Type14);


#if CFG_HAL_HIGH_POWER_EXT_PA
#include "../../../data_TXPWR_LMT_8812Fmp_Type1.c"
VAR_MAPPING(TXPWR_LMT_8812Fmp_Type1,TXPWR_LMT_8812Fmp_Type1);
#include "../../../data_TXPWR_LMT_8812Fmp_Type3.c"
VAR_MAPPING(TXPWR_LMT_8812Fmp_Type3,TXPWR_LMT_8812Fmp_Type3);
#include "../../../data_TXPWR_LMT_8812Fmp_Type4.c"
VAR_MAPPING(TXPWR_LMT_8812Fmp_Type4,TXPWR_LMT_8812Fmp_Type4);
#include "../../../data_TXPWR_LMT_8812Fmp_Type6.c"
VAR_MAPPING(TXPWR_LMT_8812Fmp_Type6,TXPWR_LMT_8812Fmp_Type6);
#include "../../../data_TXPWR_LMT_8812Fmp_Type7.c"
VAR_MAPPING(TXPWR_LMT_8812Fmp_Type7,TXPWR_LMT_8812Fmp_Type7);
#include "../../../data_TXPWR_LMT_8812Fmp_Type11.c"
VAR_MAPPING(TXPWR_LMT_8812Fmp_Type11,TXPWR_LMT_8812Fmp_Type11);
#endif
#ifdef BEAMFORMING_AUTO
#include "../../../data_TXPWR_LMT_8812Fmp_TXBF.c"
VAR_MAPPING(TXPWR_LMT_8812Fmp_TXBF,TXPWR_LMT_8812Fmp_TXBF);
#endif
#endif


//3 MACDM
VAR_MAPPING(MACDM_def_high_8812F, MACDM_def_high_8812F);
VAR_MAPPING(MACDM_def_low_8812F, MACDM_def_low_8812F);
VAR_MAPPING(MACDM_def_normal_8812F, MACDM_def_normal_8812F);

VAR_MAPPING(MACDM_gen_high_8812F, MACDM_gen_high_8812F);
VAR_MAPPING(MACDM_gen_low_8812F, MACDM_gen_low_8812F);
VAR_MAPPING(MACDM_gen_normal_8812F, MACDM_gen_normal_8812F);

VAR_MAPPING(MACDM_txop_high_8812F, MACDM_txop_high_8812F);
VAR_MAPPING(MACDM_txop_low_8812F, MACDM_txop_low_8812F);
VAR_MAPPING(MACDM_txop_normal_8812F, MACDM_txop_normal_8812F);

VAR_MAPPING(MACDM_state_criteria_8812F, MACDM_state_criteria_8812F);


//MP Chip
#include "../../../data_AGC_TAB_8812Fmp.c"
#include "../../../data_MAC_REG_8812Fmp.c"
#include "../../../data_PHY_REG_8812Fmp.c"
#include "../../../data_PHY_REG_MP_8812Fmp.c"
#include "../../../data_PHY_REG_PG_8812Fmp.c"
#include "../../../data_PHY_REG_PG_8812Fmp_Type0.c"
#include "../../../data_PHY_REG_PG_8812Fmp_Type2.c"
#include "../../../data_PHY_REG_PG_8812Fmp_Type8.c"
#include "../../../data_PHY_REG_PG_8812Fmp_Type9.c"
#include "../../../data_PHY_REG_PG_8812Fmp_Type10.c"
#include "../../../data_PHY_REG_PG_8812Fmp_Type13.c"
#include "../../../data_PHY_REG_PG_8812Fmp_Type14.c"
#include "../../../data_RadioA_8812Fmp.c"
#include "../../../data_RadioB_8812Fmp.c"
#include "../../../data_rtl8812FfwMP.c"

// High Power
#if CFG_HAL_HIGH_POWER_EXT_PA
#include "../../../data_AGC_TAB_8812Fmp_hp.c"
#include "../../../data_PHY_REG_8812Fmp_hp.c"
#include "../../../data_PHY_REG_PG_8812Fmp_Type1.c"
#include "../../../data_PHY_REG_PG_8812Fmp_Type3.c"
#include "../../../data_PHY_REG_PG_8812Fmp_Type4.c"
#include "../../../data_PHY_REG_PG_8812Fmp_Type6.c"
#include "../../../data_PHY_REG_PG_8812Fmp_Type7.c"
#include "../../../data_PHY_REG_PG_8812Fmp_Type11.c"

#include "../../../data_RadioA_8812Fmp_hp.c"
#include "../../../data_RadioB_8812Fmp_hp.c"
#endif

// Power Tracking
#include "../../../data_TxPowerTrack_AP_8812Fmp.c"


VAR_MAPPING(AGC_TAB_8812Fmp, AGC_TAB_8812Fmp);
VAR_MAPPING(MAC_REG_8812Fmp, MAC_REG_8812Fmp);
VAR_MAPPING(PHY_REG_8812Fmp, PHY_REG_8812Fmp);
VAR_MAPPING(PHY_REG_PG_8812Fmp, PHY_REG_PG_8812Fmp);
VAR_MAPPING(PHY_REG_PG_8812Fmp_Type0, PHY_REG_PG_8812Fmp_Type0);
VAR_MAPPING(PHY_REG_PG_8812Fmp_Type2, PHY_REG_PG_8812Fmp_Type2);
VAR_MAPPING(PHY_REG_PG_8812Fmp_Type8, PHY_REG_PG_8812Fmp_Type8);
VAR_MAPPING(PHY_REG_PG_8812Fmp_Type9, PHY_REG_PG_8812Fmp_Type9);
VAR_MAPPING(PHY_REG_PG_8812Fmp_Type10, PHY_REG_PG_8812Fmp_Type10);
VAR_MAPPING(PHY_REG_PG_8812Fmp_Type13, PHY_REG_PG_8812Fmp_Type13);
VAR_MAPPING(PHY_REG_PG_8812Fmp_Type14, PHY_REG_PG_8812Fmp_Type14);

VAR_MAPPING(PHY_REG_MP_8812Fmp, PHY_REG_MP_8812Fmp);
VAR_MAPPING(RadioA_8812Fmp, RadioA_8812Fmp);
VAR_MAPPING(RadioB_8812Fmp, RadioB_8812Fmp);
VAR_MAPPING(rtl8812FfwMP, rtl8812FfwMP);

// High Power
#if CFG_HAL_HIGH_POWER_EXT_PA
VAR_MAPPING(AGC_TAB_8812Fmp_hp, AGC_TAB_8812Fmp_hp);
VAR_MAPPING(PHY_REG_8812Fmp_hp, PHY_REG_8812Fmp_hp);
VAR_MAPPING(PHY_REG_PG_8812Fmp_Type1, PHY_REG_PG_8812Fmp_Type1);
VAR_MAPPING(PHY_REG_PG_8812Fmp_Type3, PHY_REG_PG_8812Fmp_Type3);
VAR_MAPPING(PHY_REG_PG_8812Fmp_Type4, PHY_REG_PG_8812Fmp_Type4);
VAR_MAPPING(PHY_REG_PG_8812Fmp_Type6, PHY_REG_PG_8812Fmp_Type6);
VAR_MAPPING(PHY_REG_PG_8812Fmp_Type7, PHY_REG_PG_8812Fmp_Type7);
VAR_MAPPING(PHY_REG_PG_8812Fmp_Type11, PHY_REG_PG_8812Fmp_Type11);
VAR_MAPPING(RadioA_8812Fmp_hp, RadioA_8812Fmp_hp);
VAR_MAPPING(RadioB_8812Fmp_hp, RadioB_8812Fmp_hp);
#endif

// Power Tracking
VAR_MAPPING(TxPowerTrack_AP_8812Fmp, TxPowerTrack_AP_8812Fmp);


