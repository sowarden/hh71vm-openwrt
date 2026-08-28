/******************************************************************************
 *
 * Copyright(c) 2007 - 2017 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/

#include "mp_precomp.h"

#if RT_PLATFORM == PLATFORM_MACOSX
#include "phydm_precomp.h"
#else
#include "../../phydm_precomp.h"
#endif

/*---------------------------Define Local Constant---------------------------*/

/*---------------------------Define Local Constant---------------------------*/

#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
void do_iqk_8197f(
	void *dm_void,
	u8 delta_thermal_index,
	u8 thermal_value,
	u8 threshold)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	odm_reset_iqk_result(dm);

	dm->rf_calibrate_info.thermal_value_iqk = thermal_value;

	halrf_iqk_trigger(dm, false);
}
#else
/*Originally config->do_iqk is hooked phy_iq_calibrate_8197f, but do_iqk_8197f and phy_iq_calibrate_8197f have different arguments*/
void do_iqk_8197f(
	void *dm_void,
	u8 delta_thermal_index,
	u8 thermal_value,
	u8 threshold)
{
#if 1
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	boolean is_recovery = (boolean)delta_thermal_index;

	halrf_iqk_trigger(dm, is_recovery);
#endif
}
#endif

#define MAX_TOLERANCE 5
#define IQK_DELAY_TIME 1 /*ms*/

u8 /*bit0 = 1 => Tx OK, bit1 = 1 => Rx OK*/
	phy_path_a_iqk_8197f(
		void *dm_void,
		boolean config_path_b)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	u32 reg_eac, reg_e94, reg_e9c, tmp;
	u8 result = 0x00;

	RF_DBG(dm, DBG_RF_IQK,
	       "[IQK] ====================path A TXIQK start!====================\n");

	/*=============================TXIQK setting=============================*/
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x000000);

#if 0	/*move to radio_a @v68*/
	/*modify TXIQK mode table*/
	odm_set_rf_reg(dm, RF_PATH_A, RF_WE_LUT, 0x80000, 0x1);
	odm_set_rf_reg(dm, RF_PATH_A, RF_RCK_OS, RFREGOFFSETMASK, 0x20000);
	odm_set_rf_reg(dm, RF_PATH_A, RF_TXPA_G1, RFREGOFFSETMASK, 0x0005f);
	odm_set_rf_reg(dm, RF_PATH_A, RF_TXPA_G2, RFREGOFFSETMASK, 0x01ff7); /*PA off, default: 0x1fff*/
	odm_set_rf_reg(dm, RF_PATH_A, RF_WE_LUT, 0x80000, 0x0);
#endif
	if (dm->ext_pa) {
		odm_set_rf_reg(dm, RF_PATH_A, RF_0xdf, 0x00800, 0x1);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x56, 0x003ff, 0x71);
	} else if (dm->ext_pa == 0 && dm->cut_version != ODM_CUT_A) {
		odm_set_rf_reg(dm, RF_PATH_A, RF_0xdf, 0x00800, 0x1);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x56, 0x003ff, 0xe8);
	}

	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0x56 at path A TXIQK = 0x%x\n",
	       odm_get_rf_reg(dm, RF_PATH_A, RF_0x56, RFREGOFFSETMASK));

	/*enter IQK mode*/
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x808000);

	/*RF_DBG(dm,DBG_RF_IQK, "[IQK] path-A IQK setting!\n");*/
	odm_set_bb_reg(dm, REG_TX_IQK_TONE_A, MASKDWORD, 0x18008c0c);
	odm_set_bb_reg(dm, REG_RX_IQK_TONE_A, MASKDWORD, 0x38008c0c);
	odm_set_bb_reg(dm, REG_TX_IQK_TONE_B, MASKDWORD, 0x38008c0c);
	odm_set_bb_reg(dm, REG_RX_IQK_TONE_B, MASKDWORD, 0x38008c0c);

	if (dm->cut_version == ODM_CUT_A) {
		if (dm->ext_pa)
			odm_set_bb_reg(dm, REG_TX_IQK_PI_A, MASKDWORD, 0x8214400f);
		else
			odm_set_bb_reg(dm, REG_TX_IQK_PI_A, MASKDWORD, 0x82140002);
	} else
		odm_set_bb_reg(dm, REG_TX_IQK_PI_A, MASKDWORD, 0x8214000f);

	odm_set_bb_reg(dm, REG_RX_IQK_PI_A, MASKDWORD, 0x28140000);

	/*RF_DBG(dm,DBG_RF_IQK, "[IQK] LO calibration setting!\n");*/
	odm_set_bb_reg(dm, REG_IQK_AGC_RSP, MASKDWORD, 0x00e62911);

	/*RF_DBG(dm,DBG_RF_IQK, "[IQK] One shot, path A LOK & IQK!\n");*/
	odm_set_bb_reg(dm, REG_IQK_AGC_PTS, MASKDWORD, 0xf9005800);
	odm_set_bb_reg(dm, REG_IQK_AGC_PTS, MASKDWORD, 0xf8005800);

	/*RF_DBG(dm,DBG_RF_IQK, "[IQK] delay %d ms for One shot, path A LOK & IQK.\n", IQK_DELAY_TIME_97F);*/
	ODM_delay_ms(IQK_DELAY_TIME_97F);

	/*Check failed*/
	reg_eac = odm_get_bb_reg(dm, REG_RX_POWER_AFTER_IQK_A_2, MASKDWORD);
	reg_e94 = odm_get_bb_reg(dm, REG_TX_POWER_BEFORE_IQK_A, MASKDWORD);
	reg_e9c = odm_get_bb_reg(dm, REG_TX_POWER_AFTER_IQK_A, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0xeac = 0x%x\n", reg_eac);
	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0xe94 = 0x%x, 0xe9c = 0x%x\n", reg_e94,
	       reg_e9c);
	/*monitor image power before & after IQK*/
	RF_DBG(dm, DBG_RF_IQK,
	       "[IQK] 0xe90(before IQK)= 0x%x, 0xe98(afer IQK) = 0x%x\n",
	       odm_get_bb_reg(dm, R_0xe90, MASKDWORD),
	       odm_get_bb_reg(dm, R_0xe98, MASKDWORD));

	/*reload 0xdf and CCK_IND off */
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x000000);
	odm_set_rf_reg(dm, RF_PATH_A, RF_0x0, BIT(14), 0x0);
	odm_set_rf_reg(dm, RF_PATH_A, RF_0xdf, 0x00800, 0x0);

	if (!(reg_eac & BIT(28)) &&
	    (((reg_e94 & 0x03FF0000) >> 16) != 0x142) &&
	    (((reg_e9c & 0x03FF0000) >> 16) != 0x42))
		result |= 0x01;
	else
		RF_DBG(dm, DBG_RF_IQK, "[IQK] path A TXIQK is not success\n");
	return result;
}

u8 /*bit0 = 1 => Tx OK, bit1 = 1 => Rx OK*/
	phy_path_a_rx_iqk_97f(
		void *dm_void,
		boolean config_path_b)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	u32 reg_eac, reg_e94, reg_e9c, reg_ea4, u4tmp, RXPGA;
	u8 result = 0x00;

	RF_DBG(dm, DBG_RF_IQK, "[IQK] path A RxIQK start!\n");

	/* =============================Get TXIMR setting=============================*/
	RF_DBG(dm, DBG_RF_IQK,
	       "[IQK] ====================path A RXIQK step1!====================\n");

	/*modify RXIQK mode table*/
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x000000);

#if 0	/*move to radio_a @v68*/
	odm_set_rf_reg(dm, RF_PATH_A, RF_WE_LUT, 0x80000, 0x1);
	odm_set_rf_reg(dm, RF_PATH_A, RF_RCK_OS, RFREGOFFSETMASK, 0x30000);
	odm_set_rf_reg(dm, RF_PATH_A, RF_TXPA_G1, RFREGOFFSETMASK, 0x0005f);
	odm_set_rf_reg(dm, RF_PATH_A, RF_TXPA_G2, RFREGOFFSETMASK, 0xf1df3); /*PA off, deafault:0xf1dfb*/
	odm_set_rf_reg(dm, RF_PATH_A, RF_WE_LUT, 0x80000, 0x0);
#endif
	if (dm->cut_version == ODM_CUT_A) {
		RXPGA = odm_get_rf_reg(dm, RF_PATH_A, RF_0x8f, RFREGOFFSETMASK);

		if (dm->ext_pa)
			odm_set_rf_reg(dm, RF_PATH_A, RF_0x8f, RFREGOFFSETMASK, 0xa8000);
		else
			odm_set_rf_reg(dm, RF_PATH_A, RF_0x8f, RFREGOFFSETMASK, 0xae000);
	}
	/*enter IQK mode*/
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x808000);

	/*path-A IQK setting*/
	odm_set_bb_reg(dm, REG_TX_IQK_TONE_A, MASKDWORD, 0x18008c0c);
	odm_set_bb_reg(dm, REG_RX_IQK_TONE_A, MASKDWORD, 0x38008c0c);
	odm_set_bb_reg(dm, REG_TX_IQK_TONE_B, MASKDWORD, 0x38008c0c);
	odm_set_bb_reg(dm, REG_RX_IQK_TONE_B, MASKDWORD, 0x38008c0c);
	odm_set_bb_reg(dm, REG_TX_IQK_PI_A, MASKDWORD, 0x82160000);
	odm_set_bb_reg(dm, REG_RX_IQK_PI_A, MASKDWORD, 0x28160000);
	odm_set_bb_reg(dm, REG_TX_IQK, MASKDWORD, 0x01007c00);
	odm_set_bb_reg(dm, REG_RX_IQK, MASKDWORD, 0x01004800);

	/*RF_DBG(dm,DBG_RF_IQK, "[IQK] LO calibration setting!\n");*/
	odm_set_bb_reg(dm, REG_IQK_AGC_RSP, MASKDWORD, 0x0046a911);

	/*RF_DBG(dm,DBG_RF_IQK, "[IQK] One shot, path A LOK & IQK!\n");*/
	odm_set_bb_reg(dm, REG_IQK_AGC_PTS, MASKDWORD, 0xf9005800);
	odm_set_bb_reg(dm, REG_IQK_AGC_PTS, MASKDWORD, 0xf8005800);

	/*RF_DBG(dm,DBG_RF_IQK, "[IQK] delay %d ms for One shot, path A LOK & IQK.\n", IQK_DELAY_TIME_92E);*/
	ODM_delay_ms(IQK_DELAY_TIME_97F);

	/*Check failed*/
	reg_eac = odm_get_bb_reg(dm, REG_RX_POWER_AFTER_IQK_A_2, MASKDWORD);
	reg_e94 = odm_get_bb_reg(dm, REG_TX_POWER_BEFORE_IQK_A, MASKDWORD);
	reg_e9c = odm_get_bb_reg(dm, REG_TX_POWER_AFTER_IQK_A, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0xeac = 0x%x\n", reg_eac);
	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0xe94 = 0x%x, 0xe9c = 0x%x\n", reg_e94,
	       reg_e9c);
	/*monitor image power before & after IQK*/
	RF_DBG(dm, DBG_RF_IQK,
	       "[IQK] 0xe90(before IQK)= 0x%x, 0xe98(afer IQK) = 0x%x\n",
	       odm_get_bb_reg(dm, R_0xe90, MASKDWORD),
	       odm_get_bb_reg(dm, R_0xe98, MASKDWORD));

	if (dm->cut_version == ODM_CUT_A) {
		/*Restore RXPGA*/
		odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x000000);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x8f, RFREGOFFSETMASK, RXPGA);
	}

	if (!(reg_eac & BIT(28)) &&
	    (((reg_e94 & 0x03FF0000) >> 16) != 0x142) &&
	    (((reg_e9c & 0x03FF0000) >> 16) != 0x42))
		result |= 0x01;
	else { /*if Tx not OK, ignore Rx*/
		RF_DBG(dm, DBG_RF_IQK,
		       "[IQK] path A RXIQK step1 is not success\n");

		return result;
	}

	u4tmp = 0x80007C00 | (reg_e94 & 0x3FF0000) | ((reg_e9c & 0x3FF0000) >> 16);
	odm_set_bb_reg(dm, REG_TX_IQK, MASKDWORD, u4tmp);
	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0xe40 = 0x%x u4tmp = 0x%x\n",
	       odm_get_bb_reg(dm, REG_TX_IQK, MASKDWORD), u4tmp);

	/* =============================RXIQK setting=============================*/
	/*modify RXIQK mode table*/
	RF_DBG(dm, DBG_RF_IQK,
	       "[IQK]====================path A RXIQK step2!====================\n");
	/*RF_DBG(dm,DBG_RF_IQK, "[IQK] path A RXIQK modify RXIQK mode table 2!\n");*/
	/*leave IQK mode*/
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x000000);

#if 0	/*move to radio_a @v68*/
	odm_set_rf_reg(dm, RF_PATH_A, RF_WE_LUT, 0x80000, 0x1);
	odm_set_rf_reg(dm, RF_PATH_A, RF_RCK_OS, RFREGOFFSETMASK, 0x38000);
	odm_set_rf_reg(dm, RF_PATH_A, RF_TXPA_G1, RFREGOFFSETMASK, 0x0005f);
	odm_set_rf_reg(dm, RF_PATH_A, RF_TXPA_G2, RFREGOFFSETMASK, 0xf1ff2); /*PA off : default:0xf1ffa*/
	odm_set_rf_reg(dm, RF_PATH_A, RF_WE_LUT, 0x80000, 0x0);
#endif
	/*PA/PAD control by 0x56, and set = 0x0*/
	if (dm->cut_version == ODM_CUT_A) {
		RXPGA = odm_get_rf_reg(dm, RF_PATH_A, RF_0x8f, RFREGOFFSETMASK);

		if (dm->ext_pa) {
			odm_set_rf_reg(dm, RF_PATH_A, RF_0xdf, 0x00800, 0x1);
			odm_set_rf_reg(dm, RF_PATH_A, RF_0x56, 0x003e0, 0x3);
			odm_set_rf_reg(dm, RF_PATH_A, RF_0x8f, RFREGOFFSETMASK, 0x28000);
		} else {
			odm_set_rf_reg(dm, RF_PATH_A, RF_0xdf, 0x00800, 0x1);
			odm_set_rf_reg(dm, RF_PATH_A, RF_0x56, 0x003e0, 0x0);
			odm_set_rf_reg(dm, RF_PATH_A, RF_0x8f, RFREGOFFSETMASK, 0xae000);
		}
	} else {
		odm_set_rf_reg(dm, RF_PATH_A, RF_0xdf, 0x00800, 0x1);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x56, 0x003e0, 0x3);
	}
	/*enter IQK mode*/
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x808000);

	/*IQK setting*/
	odm_set_bb_reg(dm, REG_RX_IQK, MASKDWORD, 0x01004800);

	/*path-A IQK setting*/
	odm_set_bb_reg(dm, REG_TX_IQK_TONE_A, MASKDWORD, 0x38008c0c);
	odm_set_bb_reg(dm, REG_RX_IQK_TONE_A, MASKDWORD, 0x18008c0c);
	odm_set_bb_reg(dm, REG_TX_IQK_TONE_B, MASKDWORD, 0x38008c0c);
	odm_set_bb_reg(dm, REG_RX_IQK_TONE_B, MASKDWORD, 0x38008c0c);
	odm_set_bb_reg(dm, REG_TX_IQK_PI_A, MASKDWORD, 0x82170000);

	if (dm->ext_pa == 1 && dm->cut_version == ODM_CUT_A)
		odm_set_bb_reg(dm, REG_RX_IQK_PI_A, MASKDWORD, 0x28170c00);
	else
		odm_set_bb_reg(dm, REG_RX_IQK_PI_A, MASKDWORD, 0x28170000);

	/*RF_DBG(dm, DBG_RF_IQK, "[IQK] 0xe3c(RX_PI_Data)= 0x%x\n",
				 odm_get_bb_reg(dm, R_0xe3c, MASKDWORD));*/

	/*RF_DBG(dm,DBG_RF_IQK, "[IQK] LO calibration setting!\n");*/
	odm_set_bb_reg(dm, REG_IQK_AGC_RSP, MASKDWORD, 0x0046a8d1);

	/*RF_DBG(dm,DBG_RF_IQK, "[IQK] One shot, path A LOK & IQK!\n");*/
	odm_set_bb_reg(dm, REG_IQK_AGC_PTS, MASKDWORD, 0xf9005800);
	odm_set_bb_reg(dm, REG_IQK_AGC_PTS, MASKDWORD, 0xf8005800);

	/*RF_DBG(dm,DBG_RF_IQK, "delay %d ms for One shot, path A LOK & IQK.\n", IQK_DELAY_TIME_92E);*/
	ODM_delay_ms(IQK_DELAY_TIME_97F);

	/*Check failed*/
	reg_eac = odm_get_bb_reg(dm, REG_RX_POWER_AFTER_IQK_A_2, MASKDWORD);
	reg_ea4 = odm_get_bb_reg(dm, REG_RX_POWER_BEFORE_IQK_A_2, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0xeac = 0x%x\n", reg_eac);
	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0xea4 = 0x%x, 0xeac = 0x%x\n", reg_ea4,
	       reg_eac);
	/*monitor image power before & after IQK*/
	RF_DBG(dm, DBG_RF_IQK,
	       "[IQK] 0xea0(before IQK)= 0x%x, 0xea8(afer IQK) = 0x%x\n",
	       odm_get_bb_reg(dm, R_0xea0, MASKDWORD),
	       odm_get_bb_reg(dm, R_0xea8, MASKDWORD));

	/*PA/PAD controlled by 0x0 & Restore RXPGA*/
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x000000);
	odm_set_rf_reg(dm, RF_PATH_A, RF_0xdf, 0x00800, 0x0);
	if (dm->cut_version == ODM_CUT_A)
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x8f, RFREGOFFSETMASK, RXPGA);

	if (!(reg_eac & BIT(27)) && /*if Tx is OK, check whether Rx is OK*/
	    (((reg_ea4 & 0x03FF0000) >> 16) != 0x132) &&
	    (((reg_eac & 0x03FF0000) >> 16) != 0x36))
		result |= 0x02;
	else
		RF_DBG(dm, DBG_RF_IQK,
		       "[IQK] path A RxIQK step2 is not success!!\n");
	return result;
}

u8 /*bit0 = 1 => Tx OK, bit1 = 1 => Rx OK*/
	phy_path_b_iqk_8197f(
		void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	u32 reg_eac, reg_eb4, reg_ebc;
	u8 result = 0x00;

	RF_DBG(dm, DBG_RF_IQK,
	       "[IQK] ====================path B TXIQK start!====================\n");

	/* =============================TXIQK setting=============================*/
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x000000);

#if 0	/*move to radio_a @v68*/
	/*modify TXIQK mode table*/
	odm_set_rf_reg(dm, RF_PATH_B, RF_WE_LUT, 0x80000, 0x1);
	odm_set_rf_reg(dm, RF_PATH_B, RF_RCK_OS, RFREGOFFSETMASK, 0x20000);
	odm_set_rf_reg(dm, RF_PATH_B, RF_TXPA_G1, RFREGOFFSETMASK, 0x0005f);
	odm_set_rf_reg(dm, RF_PATH_B, RF_TXPA_G2, RFREGOFFSETMASK, 0x01ff7); /*PA off, deafault:0xf1dfb*/
	odm_set_rf_reg(dm, RF_PATH_B, RF_WE_LUT, 0x80000, 0x0);
#endif
	/*path A to SI mode to avoid RF go to shot-down mode*/
	odm_set_bb_reg(dm, R_0x820, BIT(8), 0x0);

	if (dm->ext_pa) {
		odm_set_rf_reg(dm, RF_PATH_B, RF_0xdf, 0x00800, 0x1);
		odm_set_rf_reg(dm, RF_PATH_B, RF_0x56, 0x003ff, 0x71);
	} else if (dm->ext_pa == 0 && dm->cut_version != ODM_CUT_A) {
		odm_set_rf_reg(dm, RF_PATH_B, RF_0xdf, 0x00800, 0x1);
		odm_set_rf_reg(dm, RF_PATH_B, RF_0x56, 0x003ff, 0xe8);
	}

	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0x56 at path B TXIQK = 0x%x\n",
	       odm_get_rf_reg(dm, RF_PATH_B, RF_0x56, RFREGOFFSETMASK));

	/*enter IQK mode*/
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x808000);
	odm_set_bb_reg(dm, REG_TX_IQK_TONE_A, MASKDWORD, 0x38008c0c);
	odm_set_bb_reg(dm, REG_RX_IQK_TONE_A, MASKDWORD, 0x38008c0c);
	odm_set_bb_reg(dm, REG_TX_IQK_TONE_B, MASKDWORD, 0x18008c0c);
	odm_set_bb_reg(dm, REG_RX_IQK_TONE_B, MASKDWORD, 0x38008c0c);

	if (dm->cut_version == ODM_CUT_A) {
		if (dm->ext_pa)
			odm_set_bb_reg(dm, REG_TX_IQK_PI_B, MASKDWORD, 0x8214400f);
		else
			odm_set_bb_reg(dm, REG_TX_IQK_PI_B, MASKDWORD, 0x82140002);
	} else
		odm_set_bb_reg(dm, REG_TX_IQK_PI_B, MASKDWORD, 0x8214000f);

	odm_set_bb_reg(dm, REG_RX_IQK_PI_B, MASKDWORD, 0x28140000);

	/*RF_DBG(dm,DBG_RF_IQK, "[IQK] LO calibration setting!\n");*/
	odm_set_bb_reg(dm, REG_IQK_AGC_RSP, MASKDWORD, 0x00e62911);

	/*RF_DBG(dm,DBG_RF_IQK, "[IQK] One shot, path B LOK & IQK!\n");*/
	odm_set_bb_reg(dm, REG_IQK_AGC_PTS, MASKDWORD, 0xfa005800);
	odm_set_bb_reg(dm, REG_IQK_AGC_PTS, MASKDWORD, 0xf8005800);

	/*RF_DBG(dm,DBG_RF_IQK, "[IQK] delay %d ms for One shot, path B LOK & IQK.\n", IQK_DELAY_TIME_97F);*/
	ODM_delay_ms(IQK_DELAY_TIME_97F);

	/*Check failed*/
	reg_eac = odm_get_bb_reg(dm, REG_RX_POWER_AFTER_IQK_A_2, MASKDWORD);
	reg_eb4 = odm_get_bb_reg(dm, REG_TX_POWER_BEFORE_IQK_B, MASKDWORD);
	reg_ebc = odm_get_bb_reg(dm, REG_TX_POWER_AFTER_IQK_B, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0xeac = 0x%x\n", reg_eac);
	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0xeb4 = 0x%x, 0xebc = 0x%x\n", reg_eb4,
	       reg_ebc);
	/*monitor image power before & after IQK*/
	RF_DBG(dm, DBG_RF_IQK,
	       "[IQK] 0xeb0(before IQK)= 0x%x, 0xeb8(afer IQK) = 0x%x\n",
	       odm_get_bb_reg(dm, R_0xeb0, MASKDWORD),
	       odm_get_bb_reg(dm, R_0xeb8, MASKDWORD));

	/*path A back to PI mode*/
	odm_set_bb_reg(dm, R_0x820, BIT(8), 0x1);

	/*reload 0xdf and CCK_IND off */
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x000000);
	odm_set_rf_reg(dm, RF_PATH_B, RF_0x0, BIT(14), 0x0);
	odm_set_rf_reg(dm, RF_PATH_B, RF_0xdf, 0x00800, 0x0);

	if (!(reg_eac & BIT(31)) &&
	    (((reg_eb4 & 0x03FF0000) >> 16) != 0x142) &&
	    (((reg_ebc & 0x03FF0000) >> 16) != 0x42))
		result |= 0x01;
	else
		RF_DBG(dm, DBG_RF_IQK, "[IQK] path B TXIQK is not success\n");
	return result;
}

u8 /*bit0 = 1 => Tx OK, bit1 = 1 => Rx OK*/
	phy_path_b_rx_iqk_97f(
		void *dm_void,
		boolean config_path_b)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	u32 reg_eac, reg_eb4, reg_ebc, reg_ecc, reg_ec4, u4tmp, RXPGA;
	u8 result = 0x00;

	RF_DBG(dm, DBG_RF_IQK, "[IQK] path B RxIQK start!\n");

	/* =============================Get TXIMR setting=============================*/
	RF_DBG(dm, DBG_RF_IQK,
	       "[IQK] ====================path B RXIQK step1!====================\n");
	/*modify RXIQK mode table*/
	/*RF_DBG(dm,DBG_RF_IQK, "[IQK] path B RXIQK modify RXIQK mode table!\n");*/
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x000000);

#if 0	/*move to radio_a @v68*/
	odm_set_rf_reg(dm, RF_PATH_B, RF_WE_LUT, 0x80000, 0x1);
	odm_set_rf_reg(dm, RF_PATH_B, RF_RCK_OS, RFREGOFFSETMASK, 0x30000);
	odm_set_rf_reg(dm, RF_PATH_B, RF_TXPA_G1, RFREGOFFSETMASK, 0x0005f);
	odm_set_rf_reg(dm, RF_PATH_B, RF_TXPA_G2, RFREGOFFSETMASK, 0xf1df3); /*PA off, deafault:0xf1dfb*/
	odm_set_rf_reg(dm, RF_PATH_B, RF_WE_LUT, 0x80000, 0x0);
#endif
	if (dm->cut_version == ODM_CUT_A) {
		RXPGA = odm_get_rf_reg(dm, RF_PATH_B, RF_0x8f, RFREGOFFSETMASK);

		if (dm->ext_pa)
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x8f, RFREGOFFSETMASK, 0xa8000);
		else
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x8f, RFREGOFFSETMASK, 0xae000);
	}

	/*path A to SI mode to avoid RF go to shot-down mode*/
	odm_set_bb_reg(dm, R_0x820, BIT(8), 0x0);

	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x808000);

	/*path-B IQK setting*/
	odm_set_bb_reg(dm, REG_TX_IQK_TONE_A, MASKDWORD, 0x38008c0c);
	odm_set_bb_reg(dm, REG_RX_IQK_TONE_A, MASKDWORD, 0x38008c0c);
	odm_set_bb_reg(dm, REG_TX_IQK_TONE_B, MASKDWORD, 0x18008c0c);
	odm_set_bb_reg(dm, REG_RX_IQK_TONE_B, MASKDWORD, 0x38008c0c);
	odm_set_bb_reg(dm, REG_TX_IQK_PI_B, MASKDWORD, 0x82160000);
	odm_set_bb_reg(dm, REG_RX_IQK_PI_B, MASKDWORD, 0x28160000);
	odm_set_bb_reg(dm, REG_TX_IQK, MASKDWORD, 0x01007c00);
	odm_set_bb_reg(dm, REG_RX_IQK, MASKDWORD, 0x01004800);

	/*RF_DBG(dm,DBG_RF_IQK, "[IQK] LO calibration setting!\n");*/
	odm_set_bb_reg(dm, REG_IQK_AGC_RSP, MASKDWORD, 0x0046a911);

	/*RF_DBG(dm,DBG_RF_IQK, "[IQK] One shot, path B LOK & IQK!\n");*/
	odm_set_bb_reg(dm, REG_IQK_AGC_PTS, MASKDWORD, 0xfa005800);
	odm_set_bb_reg(dm, REG_IQK_AGC_PTS, MASKDWORD, 0xf8005800);

	/*RF_DBG(dm,DBG_RF_IQK, "[IQK]delay %d ms for One shot, path B LOK & IQK.\n", IQK_DELAY_TIME_97F);*/
	ODM_delay_ms(IQK_DELAY_TIME_97F);

	/*Check failed*/
	reg_eac = odm_get_bb_reg(dm, REG_RX_POWER_AFTER_IQK_A_2, MASKDWORD);
	reg_eb4 = odm_get_bb_reg(dm, REG_TX_POWER_BEFORE_IQK_B, MASKDWORD);
	reg_ebc = odm_get_bb_reg(dm, REG_TX_POWER_AFTER_IQK_B, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0xeac = 0x%x\n", reg_eac);
	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0xeb4 = 0x%x, 0xebc = 0x%x\n", reg_eb4,
	       reg_ebc);
	/*monitor image power before & after IQK*/
	RF_DBG(dm, DBG_RF_IQK,
	       "[IQK] 0xeb0(before IQK)= 0x%x, 0xeb8(afer IQK) = 0x%x\n",
	       odm_get_bb_reg(dm, R_0xeb0, MASKDWORD),
	       odm_get_bb_reg(dm, R_0xeb8, MASKDWORD));

	if (dm->cut_version == ODM_CUT_A) {
		/*Restore RXPGA*/
		odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x000000);
		odm_set_rf_reg(dm, RF_PATH_B, RF_0x8f, RFREGOFFSETMASK, RXPGA);
	}

	/*path A back to PI mode*/
	odm_set_bb_reg(dm, R_0x820, BIT(8), 0x1);

	if (!(reg_eac & BIT(31)) &&
	    (((reg_eb4 & 0x03FF0000) >> 16) != 0x142) &&
	    (((reg_ebc & 0x03FF0000) >> 16) != 0x42))
		result |= 0x01;
	else { /*if Tx not OK, ignore Rx*/
		RF_DBG(dm, DBG_RF_IQK,
		       "[IQK] path B RXIQK step1 is not success\n");
		return result;
	}

	u4tmp = 0x80007C00 | (reg_eb4 & 0x3FF0000) | ((reg_ebc & 0x3FF0000) >> 16);
	odm_set_bb_reg(dm, REG_TX_IQK, MASKDWORD, u4tmp);
	RF_DBG(dm, DBG_RF_IQK, "0xe40 = 0x%x u4tmp = 0x%x\n",
	       odm_get_bb_reg(dm, REG_TX_IQK, MASKDWORD), u4tmp);

	/* =============================RXIQK setting=============================*/
	RF_DBG(dm, DBG_RF_IQK,
	       "[IQK] ====================path B RXIQK step2!====================\n");

	/*modify RXIQK mode table*/
	/*RF_DBG(dm,DBG_RF_IQK, "path-B RXIQK modify RXIQK mode table 2!\n");*/
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x000000);

#if 0	/*move to radio_a @v68*/
	odm_set_rf_reg(dm, RF_PATH_B, RF_WE_LUT, 0x80000, 0x1);
	odm_set_rf_reg(dm, RF_PATH_B, RF_RCK_OS, RFREGOFFSETMASK, 0x38000);
	odm_set_rf_reg(dm, RF_PATH_B, RF_TXPA_G1, RFREGOFFSETMASK, 0x0005f);
	odm_set_rf_reg(dm, RF_PATH_B, RF_TXPA_G2, RFREGOFFSETMASK, 0xf1ff2); /*PA off : default:0xf1ffa*/
	odm_set_rf_reg(dm, RF_PATH_B, RF_WE_LUT, 0x80000, 0x0);
#endif
	/*PA/PAD control by 0x56, and set = 0x0*/
	if (dm->cut_version == ODM_CUT_A) {
		RXPGA = odm_get_rf_reg(dm, RF_PATH_B, RF_0x8f, RFREGOFFSETMASK);

		if (dm->ext_pa) {
			odm_set_rf_reg(dm, RF_PATH_B, RF_0xdf, 0x00800, 0x1);
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x56, 0x003e0, 0x3);
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x8f, RFREGOFFSETMASK, 0x28000);
		} else {
			odm_set_rf_reg(dm, RF_PATH_B, RF_0xdf, 0x00800, 0x1);
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x56, 0x003e0, 0x0);
			odm_set_rf_reg(dm, RF_PATH_B, RF_0x8f, RFREGOFFSETMASK, 0xae000);
		}
	} else {
		odm_set_rf_reg(dm, RF_PATH_B, RF_0xdf, 0x00800, 0x1);
		odm_set_rf_reg(dm, RF_PATH_B, RF_0x56, 0x003e0, 0x3);
	}

	/*path A to SI mode to avoid RF go to shot-down mode*/
	odm_set_bb_reg(dm, R_0x820, BIT(8), 0x0);

	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x808000);

	/*IQK setting*/
	odm_set_bb_reg(dm, REG_RX_IQK, MASKDWORD, 0x01004800);

	/*path-B IQK setting*/
	odm_set_bb_reg(dm, REG_TX_IQK_TONE_A, MASKDWORD, 0x38008c0c);
	odm_set_bb_reg(dm, REG_RX_IQK_TONE_A, MASKDWORD, 0x38008c0c);
	odm_set_bb_reg(dm, REG_TX_IQK_TONE_B, MASKDWORD, 0x38008c0c);
	odm_set_bb_reg(dm, REG_RX_IQK_TONE_B, MASKDWORD, 0x18008c0c);
	odm_set_bb_reg(dm, REG_TX_IQK_PI_B, MASKDWORD, 0x82170000);

	if (dm->ext_pa == 1 && dm->cut_version == ODM_CUT_A)
		odm_set_bb_reg(dm, REG_RX_IQK_PI_B, MASKDWORD, 0x28170c00);
	else
		odm_set_bb_reg(dm, REG_RX_IQK_PI_B, MASKDWORD, 0x28170000);

	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0xe5c(RX_PI_Data)= 0x%x\n",
	       odm_get_bb_reg(dm, R_0xe5c, MASKDWORD));

	/*RF_DBG(dm,DBG_RF_IQK, "LO calibration setting!\n");*/
	odm_set_bb_reg(dm, REG_IQK_AGC_RSP, MASKDWORD, 0x0046a8d1);

	/*RF_DBG(dm,DBG_RF_IQK, "One shot, path B LOK & IQK!\n");*/
	odm_set_bb_reg(dm, REG_IQK_AGC_PTS, MASKDWORD, 0xfa005800);
	odm_set_bb_reg(dm, REG_IQK_AGC_PTS, MASKDWORD, 0xf8005800);

	/*RF_DBG(dm,DBG_RF_IQK, "delay %d ms for One shot, path B LOK & IQK.\n", IQK_DELAY_TIME_97F);*/
	ODM_delay_ms(IQK_DELAY_TIME_97F);

	/*Check failed*/
	reg_eac = odm_get_bb_reg(dm, REG_RX_POWER_AFTER_IQK_A_2, MASKDWORD);
	reg_ec4 = odm_get_bb_reg(dm, REG_RX_POWER_BEFORE_IQK_B_2, MASKDWORD);
	reg_ecc = odm_get_bb_reg(dm, REG_RX_POWER_AFTER_IQK_B_2, MASKDWORD);
	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0xeac = 0x%x\n", reg_eac);
	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0xec4 = 0x%x, 0xecc = 0x%x\n", reg_ec4,
	       reg_ecc);
	/*monitor image power before & after IQK*/
	RF_DBG(dm, DBG_RF_IQK,
	       "[IQK] 0xec0(before IQK)= 0x%x, 0xec8(afer IQK) = 0x%x\n",
	       odm_get_bb_reg(dm, R_0xec0, MASKDWORD),
	       odm_get_bb_reg(dm, R_0xec8, MASKDWORD));

	/*PA/PAD controlled by 0x0 & Restore RXPGA*/
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x000000);
	odm_set_rf_reg(dm, RF_PATH_B, RF_0xdf, 0x00800, 0x0);
	if (dm->cut_version == ODM_CUT_A)
		odm_set_rf_reg(dm, RF_PATH_B, RF_0x8f, RFREGOFFSETMASK, RXPGA);

	/*path A back to PI mode*/
	odm_set_bb_reg(dm, R_0x820, BIT(8), 0x1);

	if (!(reg_eac & BIT(30)) && /*if Tx is OK, check whether Rx is OK*/
	    (((reg_ec4 & 0x03FF0000) >> 16) != 0x132) &&
	    (((reg_ecc & 0x03FF0000) >> 16) != 0x36))
		result |= 0x02;
	else
		RF_DBG(dm, DBG_RF_IQK,
		       "[IQK] path B RXIQK step2 is not success!!\n");
	return result;
}

void _phy_path_a_fill_iqk_matrix_97f(
	void *dm_void,
	boolean is_iqk_ok,
	s32 result[][8],
	u8 final_candidate,
	boolean is_tx_only)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	u32 oldval_0, X, TX0_A, reg;
	s32 Y, TX0_C;

	RF_DBG(dm, DBG_RF_IQK, "[IQK] path A IQ Calibration %s !\n",
	       (is_iqk_ok) ? "Success" : "Failed");

	if (final_candidate == 0xFF)
		return;

	else if (is_iqk_ok) {
#ifdef CONFIG_RF_DPK_SETTING_SUPPORT

		oldval_0 = (odm_get_bb_reg(dm, REG_OFDM_0_XA_TX_IQ_IMBALANCE, MASKDWORD) >> 22) & 0x3FF;

		odm_set_bb_reg(dm, REG_OFDM_0_XA_TX_IQ_IMBALANCE, 0x3FF, oldval_0);

		X = result[final_candidate][0];
		if ((X & 0x00000200) != 0)
			X = X | 0xFFFFFC00;

		TX0_A = (X * 0x100) >> 8;
		RF_DBG(dm, DBG_RF_IQK, "[IQK] X = 0x%x, TX0_A = 0x%x\n", X,
		       TX0_A);

		odm_set_bb_reg(dm, R_0xe30, 0x3FF00000, TX0_A);

		Y = result[final_candidate][1];
		if ((Y & 0x00000200) != 0)
			Y = Y | 0xFFFFFC00;

		TX0_C = (Y * 0x100) >> 8;
		RF_DBG(dm, DBG_RF_IQK, "[IQK] Y = 0x%x, TX0_C = 0x%x\n", Y,
		       TX0_C);
		odm_set_bb_reg(dm, R_0xe20, 0x000003C0, ((TX0_C & 0x3C0) >> 6));
		odm_set_bb_reg(dm, R_0xe20, 0x0000003F, (TX0_C & 0x3F));
#else
		oldval_0 = (odm_get_bb_reg(dm, REG_OFDM_0_XA_TX_IQ_IMBALANCE, MASKDWORD) >> 22) & 0x3FF;

		X = result[final_candidate][0];
		if ((X & 0x00000200) != 0)
			X = X | 0xFFFFFC00;
		TX0_A = (X * oldval_0) >> 8;
		RF_DBG(dm, DBG_RF_IQK,
		       "[IQK] X = 0x%x, TX0_A = 0x%x, oldval_0 0x%x\n", X,
		       TX0_A, oldval_0);
		odm_set_bb_reg(dm, REG_OFDM_0_XA_TX_IQ_IMBALANCE, 0x3FF, TX0_A);

		odm_set_bb_reg(dm, REG_OFDM_0_ECCA_THRESHOLD, BIT(31), ((X * oldval_0 >> 7) & 0x1));

		Y = result[final_candidate][1];
		if ((Y & 0x00000200) != 0)
			Y = Y | 0xFFFFFC00;

		TX0_C = (Y * oldval_0) >> 8;
		RF_DBG(dm, DBG_RF_IQK, "[IQK] Y = 0x%x, TX0_C = 0x%x\n", Y,
		       TX0_C);
		odm_set_bb_reg(dm, REG_OFDM_0_XC_TX_AFE, 0xF0000000, ((TX0_C & 0x3C0) >> 6));
		odm_set_bb_reg(dm, REG_OFDM_0_XA_TX_IQ_IMBALANCE, 0x003F0000, (TX0_C & 0x3F));

		odm_set_bb_reg(dm, REG_OFDM_0_ECCA_THRESHOLD, BIT(29), ((Y * oldval_0 >> 7) & 0x1));
#endif
		if (is_tx_only) {
			RF_DBG(dm, DBG_RF_IQK, "[IQK] %s only Tx OK\n",
			       __func__);
			return;
		}

		reg = result[final_candidate][2];
#if (DM_ODM_SUPPORT_TYPE == ODM_AP)
		if (RTL_ABS(reg, 0x100) >= 16)
			reg = 0x100;
#endif
		odm_set_bb_reg(dm, REG_OFDM_0_XA_RX_IQ_IMBALANCE, 0x3FF, reg);

		reg = result[final_candidate][3] & 0x3F;
		odm_set_bb_reg(dm, REG_OFDM_0_XA_RX_IQ_IMBALANCE, 0xFC00, reg);

		reg = (result[final_candidate][3] >> 6) & 0xF;
		odm_set_bb_reg(dm, REG_OFDM_0_RX_IQ_EXT_ANTA, 0xF0000000, reg);
	}
}

void _phy_path_b_fill_iqk_matrix_97f(
	void *dm_void,
	boolean is_iqk_ok,
	s32 result[][8],
	u8 final_candidate,
	boolean is_tx_only /*do Tx only*/
	)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	u32 oldval_1, X, TX1_A, reg;
	s32 Y, TX1_C;

	RF_DBG(dm, DBG_RF_IQK, "[IQK] path B IQ Calibration %s !\n",
	       (is_iqk_ok) ? "Success" : "Failed");

	if (final_candidate == 0xFF)
		return;

	else if (is_iqk_ok) {
#ifdef CONFIG_RF_DPK_SETTING_SUPPORT

		oldval_1 = (odm_get_bb_reg(dm, REG_OFDM_0_XB_TX_IQ_IMBALANCE, MASKDWORD) >> 22) & 0x3FF;

		odm_set_bb_reg(dm, REG_OFDM_0_XB_TX_IQ_IMBALANCE, 0x3FF, oldval_1);

		X = result[final_candidate][4];
		if ((X & 0x00000200) != 0)
			X = X | 0xFFFFFC00;

		TX1_A = (X * 0x100) >> 8;
		RF_DBG(dm, DBG_RF_IQK, "[IQK] X = 0x%x, TX1_A = 0x%x\n", X,
		       TX1_A);

		odm_set_bb_reg(dm, R_0xe50, 0x3FF00000, TX1_A);

		Y = result[final_candidate][5];
		if ((Y & 0x00000200) != 0)
			Y = Y | 0xFFFFFC00;

		TX1_C = (Y * 0x100) >> 8;
		RF_DBG(dm, DBG_RF_IQK, "[IQK] Y = 0x%x, TX1_C = 0x%x\n", Y,
		       TX1_C);
		odm_set_bb_reg(dm, R_0xe24, 0x000003C0, ((TX1_C & 0x3C0) >> 6));
		odm_set_bb_reg(dm, R_0xe24, 0x0000003F, (TX1_C & 0x3F));
#else
		oldval_1 = (odm_get_bb_reg(dm, REG_OFDM_0_XB_TX_IQ_IMBALANCE, MASKDWORD) >> 22) & 0x3FF;

		X = result[final_candidate][4];
		if ((X & 0x00000200) != 0)
			X = X | 0xFFFFFC00;
		TX1_A = (X * oldval_1) >> 8;
		RF_DBG(dm, DBG_RF_IQK, "[IQK] X = 0x%x, TX1_A = 0x%x\n", X,
		       TX1_A);
		odm_set_bb_reg(dm, REG_OFDM_0_XB_TX_IQ_IMBALANCE, 0x3FF, TX1_A);

		odm_set_bb_reg(dm, REG_OFDM_0_ECCA_THRESHOLD, BIT(27), ((X * oldval_1 >> 7) & 0x1));

		Y = result[final_candidate][5];
		if ((Y & 0x00000200) != 0)
			Y = Y | 0xFFFFFC00;

		TX1_C = (Y * oldval_1) >> 8;
		RF_DBG(dm, DBG_RF_IQK, "[IQK] Y = 0x%x, TX1_C = 0x%x\n", Y,
		       TX1_C);
		odm_set_bb_reg(dm, REG_OFDM_0_XD_TX_AFE, 0xF0000000, ((TX1_C & 0x3C0) >> 6));
		odm_set_bb_reg(dm, REG_OFDM_0_XB_TX_IQ_IMBALANCE, 0x003F0000, (TX1_C & 0x3F));

		odm_set_bb_reg(dm, REG_OFDM_0_ECCA_THRESHOLD, BIT(25), ((Y * oldval_1 >> 7) & 0x1));
#endif

		if (is_tx_only) {
			RF_DBG(dm, DBG_RF_IQK, "[IQK] %s only Tx OK\n",
			       __func__);
			return;
		}
		reg = result[final_candidate][6];
		odm_set_bb_reg(dm, REG_OFDM_0_XB_RX_IQ_IMBALANCE, 0x3FF, reg);

		reg = result[final_candidate][7] & 0x3F;
		odm_set_bb_reg(dm, REG_OFDM_0_XB_RX_IQ_IMBALANCE, 0xFC00, reg);

		reg = (result[final_candidate][7] >> 6) & 0xF;
		odm_set_bb_reg(dm, R_0xca8, 0x000000F0, reg);
	}
}

void _phy_save_adda_registers_97f(
	void *dm_void,
	u32 *adda_reg,
	u32 *adda_backup,
	u32 register_num)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	u32 i;

	/*RF_DBG(dm,DBG_RF_IQK, "[IQK] Save ADDA parameters.\n");*/
	for (i = 0; i < register_num; i++)
		adda_backup[i] = odm_get_bb_reg(dm, adda_reg[i], MASKDWORD);
}

void _phy_save_mac_registers_97f(
	void *dm_void,
	u32 *mac_reg,
	u32 *mac_backup)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	u32 i;

	/*RF_DBG(dm,DBG_RF_IQK, "[IQK] Save MAC parameters.\n");*/
	for (i = 0; i < (IQK_MAC_REG_NUM - 1); i++)
		mac_backup[i] = odm_read_1byte(dm, mac_reg[i]);
	mac_backup[i] = odm_read_4byte(dm, mac_reg[i]);
}

void _phy_reload_adda_registers_97f(
	void *dm_void,
	u32 *adda_reg,
	u32 *adda_backup,
	u32 regiester_num)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	u32 i;

	RF_DBG(dm, DBG_RF_IQK, "[IQK] Reload ADDA power saving parameters !\n");
	for (i = 0; i < regiester_num; i++)
		odm_set_bb_reg(dm, adda_reg[i], MASKDWORD, adda_backup[i]);
}

void _phy_reload_mac_registers_97f(
	void *dm_void,
	u32 *mac_reg,
	u32 *mac_backup)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 i;

	RF_DBG(dm, DBG_RF_IQK, "[IQK] Reload MAC parameters !\n");
#if 0
	odm_set_bb_reg(dm, R_0x520, MASKBYTE2, 0x0);
#else
	for (i = 0; i < (IQK_MAC_REG_NUM - 1); i++)
		odm_write_1byte(dm, mac_reg[i], (u8)mac_backup[i]);
	odm_write_4byte(dm, mac_reg[i], mac_backup[i]);
#endif
}

void _phy_path_adda_on_97f(
	void *dm_void,
	u32 *adda_reg,
	boolean is_path_a_on,
	boolean is2T)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 path_on;
	u32 i;
	/*RF_DBG(dm,DBG_RF_IQK, "[IQK] ADDA ON.\n");*/

	odm_set_bb_reg(dm, R_0xd94, 0x00ff0000, 0xff);
	odm_set_bb_reg(dm, R_0xe70, MASKDWORD, 0x00400040);

#if 0
	path_on = is_path_a_on ? 0x0fc01616 : 0x0fc01616;
	if (false == is2T) {
		path_on = 0x0fc01616;
		odm_set_bb_reg(dm, adda_reg[0], MASKDWORD, 0x0fc01616);
	} else
		odm_set_bb_reg(dm, adda_reg[0], MASKDWORD, path_on);

	for (i = 1 ; i < IQK_ADDA_REG_NUM ; i++)
		odm_set_bb_reg(dm, adda_reg[i], MASKDWORD, path_on);
#endif
}

void _phy_mac_setting_calibration_97f(
	void *dm_void,
	u32 *mac_reg,
	u32 *mac_backup)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

/*u32	i = 0;*/
/*	RF_DBG(dm,DBG_RF_IQK, "MAC settings for Calibration.\n");*/

#if 0
	odm_write_1byte(dm, mac_reg[i], 0x3F);

	for (i = 1 ; i < (IQK_MAC_REG_NUM - 1); i++)
		odm_write_1byte(dm, mac_reg[i], (u8)(mac_backup[i] & (~BIT(3))));

	odm_write_1byte(dm, mac_reg[i], (u8)(mac_backup[i] & (~BIT(5))));
#endif

#if 1
	odm_set_bb_reg(dm, R_0x520, MASKBYTE2, 0xff);
#else
	odm_set_bb_reg(dm, R_0x522, MASKBYTE0, 0x7f);
	odm_set_bb_reg(dm, R_0x550, MASKBYTE0, 0x15);
	odm_set_bb_reg(dm, R_0x551, MASKBYTE0, 0x00);
#endif
}

void _phy_path_a_stand_by_97f(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	RF_DBG(dm, DBG_RF_IQK, "[IQK] path-A standby mode!\n");

	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x0);
	/*odm_set_bb_reg(dm, R_0x840, MASKDWORD, 0x00010000);*/
	odm_set_rf_reg(dm, 0x0, RF_0x0, RFREGOFFSETMASK, 0x10000);
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x808000);
}

void _phy_path_b_stand_by_97f(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	RF_DBG(dm, DBG_RF_IQK, "[IQK] path-B standby mode!\n");

	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x000000);
	odm_set_rf_reg(dm, 0x1, RF_0x0, RFREGOFFSETMASK, 0x10000);

	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x808000);
}

void _phy_pi_mode_switch_97f(
	void *dm_void,
	boolean pi_mode)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	u32 mode;
	/*RF_DBG(dm,DBG_RF_IQK, "[IQK] BB Switch to %s mode!\n", (pi_mode ? "PI" : "SI"));*/

	mode = pi_mode ? 0x01000100 : 0x01000000;
	odm_set_bb_reg(dm, REG_FPGA0_XA_HSSI_PARAMETER1, MASKDWORD, mode);
	odm_set_bb_reg(dm, REG_FPGA0_XB_HSSI_PARAMETER1, MASKDWORD, mode);
}

boolean
phy_simularity_compare_8197f(
	void *dm_void,
	s32 result[][8],
	u8 c1,
	u8 c2)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	u32 i, j, diff, simularity_bit_map, bound = 0;
	u8 final_candidate[2] = {0xFF, 0xFF}; /*for path A and path B*/
	boolean is_result = true;
	/*#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)*/
	/*	bool		is2T = IS_92C_SERIAL( hal_data->version_id);*/
	/*#else*/
	boolean is2T = true;
	/*#endif*/

	s32 tmp1 = 0, tmp2 = 0;

	if (is2T)
		bound = 8;
	else
		bound = 4;

	RF_DBG(dm, DBG_RF_IQK, "[IQK] ===> IQK:%s c1 %d c2 %d!!!\n", __func__,
	       c1, c2);

	simularity_bit_map = 0;

	for (i = 0; i < bound; i++) {
		if (i == 1 || i == 3 || i == 5 || i == 7) {
			if ((result[c1][i] & 0x00000200) != 0)
				tmp1 = result[c1][i] | 0xFFFFFC00;
			else
				tmp1 = result[c1][i];

			if ((result[c2][i] & 0x00000200) != 0)
				tmp2 = result[c2][i] | 0xFFFFFC00;
			else
				tmp2 = result[c2][i];
		} else {
			tmp1 = result[c1][i];
			tmp2 = result[c2][i];
		}

		diff = (tmp1 > tmp2) ? (tmp1 - tmp2) : (tmp2 - tmp1);

		if (diff > MAX_TOLERANCE) {
			RF_DBG(dm, DBG_RF_IQK,
			       "[IQK] differnece overflow %d index %d compare1 0x%x compare2 0x%x!!!\n",
			       diff, i, result[c1][i], result[c2][i]);

			if ((i == 2 || i == 6) && !simularity_bit_map) {
				if (result[c1][i] + result[c1][i + 1] == 0)
					final_candidate[(i / 4)] = c2;
				else if (result[c2][i] + result[c2][i + 1] == 0)
					final_candidate[(i / 4)] = c1;
				else
					simularity_bit_map = simularity_bit_map | (1 << i);
			} else
				simularity_bit_map = simularity_bit_map | (1 << i);
		}
	}

	RF_DBG(dm, DBG_RF_IQK, "[IQK] %s simularity_bit_map   %x !!!\n",
	       __func__, simularity_bit_map);

	if (simularity_bit_map == 0) {
		for (i = 0; i < (bound / 4); i++) {
			if (final_candidate[i] != 0xFF) {
				for (j = i * 4; j < (i + 1) * 4 - 2; j++)
					result[3][j] = result[final_candidate[i]][j];
				is_result = false;
			}
		}
		return is_result;

	} else {
		if (!(simularity_bit_map & 0x03)) { /*path A TX OK*/
			for (i = 0; i < 2; i++)
				result[3][i] = result[c1][i];
		}

		if (!(simularity_bit_map & 0x0c)) { /*path A RX OK*/
			for (i = 2; i < 4; i++)
				result[3][i] = result[c1][i];
		}

		if (!(simularity_bit_map & 0x30)) { /*path B TX OK*/
			for (i = 4; i < 6; i++)
				result[3][i] = result[c1][i];
		}

		if (!(simularity_bit_map & 0xc0)) { /*path B RX OK*/
			for (i = 6; i < 8; i++)
				result[3][i] = result[c1][i];
		}

		return false;
	}
}

void _phy_iqk_check_97f(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 txa_fail, rxa_fail, txb_fail, rxb_fail;

	txa_fail = odm_get_bb_reg(dm, R_0xeac, BIT(28));
	rxa_fail = odm_get_bb_reg(dm, R_0xeac, BIT(27));
	txb_fail = odm_get_bb_reg(dm, R_0xeac, BIT(31));
	rxb_fail = odm_get_bb_reg(dm, R_0xeac, BIT(30));

#if RT_PLATFORM != PLATFORM_MACOSX
	if (txa_fail == 1)
		panic_printk("[IQK] path A TXIQK load default value!!!\n");
	if (rxa_fail == 1)
		panic_printk("[IQK] path A RXIQK load default value!!!\n");
	if (txb_fail == 1)
		panic_printk("[IQK] path B TXIQK load default value!!!\n");
	if (rxb_fail == 1)
		panic_printk("[IQK] path B RXIQK load default value!!!\n");
#endif
}

void _phy_iq_calibrate_8197f(
	void *dm_void,
	s32 result[][8],
	u8 t,
	boolean is2T)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u32 i, DAC_gain_A, DAC_gain_B;
	u8 path_aok = 0, path_bok = 0;
	u8 tmp0xc50 = (u8)odm_get_bb_reg(dm, R_0xc50, MASKBYTE0);
	u8 tmp0xc58 = (u8)odm_get_bb_reg(dm, R_0xc58, MASKBYTE0);
	u32 ADDA_REG[IQK_ADDA_REG_NUM] = {
		0xd94, REG_RX_WAIT_CCA};
	u32 IQK_MAC_REG[IQK_MAC_REG_NUM] = {
		REG_TXPAUSE, REG_BCN_CTRL,
		REG_BCN_CTRL_1, REG_GPIO_MUXCFG};

	/*since 92C & 92D have the different define in IQK_BB_REG*/
	u32 IQK_BB_REG_92C[IQK_BB_REG_NUM] = {
		REG_OFDM_0_TRX_PATH_ENABLE, REG_OFDM_0_TR_MUX_PAR,
		REG_FPGA0_XCD_RF_INTERFACE_SW, REG_CONFIG_ANT_A, REG_CONFIG_ANT_B,
		0x930, 0x934,
		0x93c, REG_CCK_0_AFE_SETTING};
	u32 retry_count = 2;

	/*Note: IQ calibration must be performed after loading*/
	/*PHY_REG.txt,and radio_a,radio_b.txt*/

	/*u32 bbvalue;*/

	if (*dm->mp_mode)
		retry_count = 2;

#if 0
			/*RF setting :RXBB leave power saving*/
			if (dm->cut_version != ODM_CUT_A) {
				odm_set_rf_reg(dm, RF_PATH_A, RF_0xdf, 0x00002, 0x1);
				odm_set_rf_reg(dm, RF_PATH_B, RF_0xdf, 0x00002, 0x1);
			}
#endif
	/*Save and set DAC gain for IQK*/
	DAC_gain_A = odm_get_rf_reg(dm, RF_PATH_A, RF_0x55, RFREGOFFSETMASK);
	DAC_gain_B = odm_get_rf_reg(dm, RF_PATH_B, RF_0x55, RFREGOFFSETMASK);
	RF_DBG(dm, DBG_RF_IQK, "[IQK] Ori_0x55 at path A = 0x%x\n", DAC_gain_A);
	RF_DBG(dm, DBG_RF_IQK, "[IQK] Ori_0x55 at path B = 0x%x\n", DAC_gain_B);

	odm_set_rf_reg(dm, RF_PATH_A, RF_0x55, 0x000e0, 0x0);
	odm_set_rf_reg(dm, RF_PATH_B, RF_0x55, 0x000e0, 0x0);
	RF_DBG(dm, DBG_RF_IQK, "[IQK] Set_0x55 at path A = 0x%x\n",
	       odm_get_rf_reg(dm, RF_PATH_A, RF_0x55, RFREGOFFSETMASK));
	RF_DBG(dm, DBG_RF_IQK, "[IQK] Set_0x55 at path B = 0x%x\n",
	       odm_get_rf_reg(dm, RF_PATH_B, RF_0x55, RFREGOFFSETMASK));

	if (t == 0) {
		/*bbvalue = odm_get_bb_reg(dm, REG_FPGA0_RFMOD, MASKDWORD);*/
		/*RT_DISP(FINIT, INIT_IQK, ("_phy_iq_calibrate_8188e()==>0x%08x\n",bbvalue));*/
		/*RF_DBG(dm,DBG_RF_IQK, "IQ Calibration for %s for %d times\n", (is2T ? "2T2R" : "1T1R"), t);*/

		/*Save ADDA parameters, turn path A ADDA on*/
		_phy_save_adda_registers_97f(dm, ADDA_REG, dm->rf_calibrate_info.ADDA_backup, IQK_ADDA_REG_NUM);
		_phy_save_mac_registers_97f(dm, IQK_MAC_REG, dm->rf_calibrate_info.IQK_MAC_backup);
		_phy_save_adda_registers_97f(dm, IQK_BB_REG_92C, dm->rf_calibrate_info.IQK_BB_backup, IQK_BB_REG_NUM);
	}
	RF_DBG(dm, DBG_RF_IQK, "[IQK] IQ Calibration for %s for %d times\n",
	       (is2T ? "2T2R" : "1T1R"), t);

	_phy_path_adda_on_97f(dm, ADDA_REG, true, is2T);

	/*BB setting*/
	/*odm_set_bb_reg(dm, REG_FPGA0_RFMOD, BIT24, 0x00);*/
	odm_set_bb_reg(dm, REG_CCK_0_AFE_SETTING, 0x0f000000, 0xf);
	odm_set_bb_reg(dm, REG_OFDM_0_TRX_PATH_ENABLE, MASKDWORD, 0x6f005403);
	odm_set_bb_reg(dm, REG_OFDM_0_TR_MUX_PAR, MASKDWORD, 0x000804e4);
	odm_set_bb_reg(dm, REG_FPGA0_XCD_RF_INTERFACE_SW, MASKDWORD, 0x04203400);

#if 1
	/*FEM off when ext_pa or ext_lna= 1*/
	if (dm->ext_pa || dm->ext_lna) {
		odm_set_bb_reg(dm, R_0x930, MASKDWORD, 0xFFFF77FF);
		odm_set_bb_reg(dm, R_0x934, MASKDWORD, 0xFFFFFFF7);
		odm_set_bb_reg(dm, R_0x93c, MASKDWORD, 0xFFFF777F);
	}
#endif

	/*	if(is2T) {*/
	/*		odm_set_bb_reg(dm, REG_FPGA0_XA_LSSI_PARAMETER, MASKDWORD, 0x00010000);*/
	/*		odm_set_bb_reg(dm, REG_FPGA0_XB_LSSI_PARAMETER, MASKDWORD, 0x00010000);*/
	/*	}*/

	/*MAC settings*/
	_phy_mac_setting_calibration_97f(dm, IQK_MAC_REG, dm->rf_calibrate_info.IQK_MAC_backup);

	/* IQ calibration setting*/
	/*RF_DBG(dm,DBG_RF_IQK, "IQK setting!\n");*/
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x808000);
	odm_set_bb_reg(dm, REG_TX_IQK, MASKDWORD, 0x01007c00);
	odm_set_bb_reg(dm, REG_RX_IQK, MASKDWORD, 0x01004800);

/*path A TXIQK*/
#if 1
	for (i = 0; i < retry_count; i++) {
		path_aok = phy_path_a_iqk_8197f(dm, is2T);
		if (path_aok == 0x01) {
			RF_DBG(dm, DBG_RF_IQK,
			       "[IQK] path A TXIQK Success!!\n");
			result[t][0] = (odm_get_bb_reg(dm, REG_TX_POWER_BEFORE_IQK_A, MASKDWORD) & 0x3FF0000) >> 16;
			result[t][1] = (odm_get_bb_reg(dm, REG_TX_POWER_AFTER_IQK_A, MASKDWORD) & 0x3FF0000) >> 16;
			break;
		}

		RF_DBG(dm, DBG_RF_IQK, "[IQK] path A TXIQK Fail!!\n");

		result[t][0] = 0x100;
		result[t][1] = 0x0;
#if 0
		else if (i == (retry_count - 1) && path_aok == 0x01) {	/*Tx IQK OK*/
			RT_DISP(FINIT, INIT_IQK, ("path A IQK Only  Tx Success!!\n"));

			result[t][0] = (odm_get_bb_reg(dm, REG_TX_POWER_BEFORE_IQK_A, MASKDWORD) & 0x3FF0000) >> 16;
			result[t][1] = (odm_get_bb_reg(dm, REG_TX_POWER_AFTER_IQK_A, MASKDWORD) & 0x3FF0000) >> 16;
		}
#endif
	}
#endif

/*path A RXIQK*/
#if 1
	for (i = 0; i < retry_count; i++) {
		path_aok = phy_path_a_rx_iqk_97f(dm, is2T);
		if (path_aok == 0x03) {
			RF_DBG(dm, DBG_RF_IQK,
			       "[IQK] path A RXIQK Success!!\n");
			result[t][2] = (odm_get_bb_reg(dm, REG_RX_POWER_BEFORE_IQK_A_2, MASKDWORD) & 0x3FF0000) >> 16;
			result[t][3] = (odm_get_bb_reg(dm, REG_RX_POWER_AFTER_IQK_A_2, MASKDWORD) & 0x3FF0000) >> 16;
			break;
		}

		RF_DBG(dm, DBG_RF_IQK, "[IQK] path A RXIQK Fail!!\n");

		result[t][2] = 0x100;
		result[t][3] = 0x0;
	}

	if (0x00 == path_aok)
		RF_DBG(dm, DBG_RF_IQK, "[IQK] path A IQK failed!!\n");

#endif

	if (is2T) {
		_phy_path_a_stand_by_97f(dm);
		/*Turn ADDA on*/
		_phy_path_adda_on_97f(dm, ADDA_REG, false, is2T);
		/*IQ calibration setting*/
		/*RF_DBG(dm,DBG_RF_IQK, "IQK setting!\n");*/
		odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x808000);
		odm_set_bb_reg(dm, REG_TX_IQK, MASKDWORD, 0x01007c00);
		odm_set_bb_reg(dm, REG_RX_IQK, MASKDWORD, 0x01004800);

/*path B Tx IQK*/
#if 1
		for (i = 0; i < retry_count; i++) {
			path_bok = phy_path_b_iqk_8197f(dm);
			if (path_bok == 0x01) {
				RF_DBG(dm, DBG_RF_IQK,
				       "[IQK] path B TXIQK Success!!\n");
				result[t][4] = (odm_get_bb_reg(dm, REG_TX_POWER_BEFORE_IQK_B, MASKDWORD) & 0x3FF0000) >> 16;
				result[t][5] = (odm_get_bb_reg(dm, REG_TX_POWER_AFTER_IQK_B, MASKDWORD) & 0x3FF0000) >> 16;
				break;
			}

			RF_DBG(dm, DBG_RF_IQK, "path B TXIQK Fail!!\n");

			result[t][4] = 0x100;
			result[t][5] = 0x0;
#if 0
			else if (i == (retry_count - 1) && path_aok == 0x01) {	/*Tx IQK OK*/
				RT_DISP(FINIT, INIT_IQK, ("path B IQK Only  Tx Success!!\n"));

				result[t][0] = (odm_get_bb_reg(dm, REG_TX_POWER_BEFORE_IQK_B, MASKDWORD) & 0x3FF0000) >> 16;
				result[t][1] = (odm_get_bb_reg(dm, REG_TX_POWER_AFTER_IQK_B, MASKDWORD) & 0x3FF0000) >> 16;
			}
#endif
		}
#endif

/*path B RX IQK*/
#if 1

		for (i = 0; i < retry_count; i++) {
			path_bok = phy_path_b_rx_iqk_97f(dm, is2T);
			if (path_bok == 0x03) {
				RF_DBG(dm, DBG_RF_IQK,
				       "[IQK] path B RXIQK Success!!\n");
				result[t][6] = (odm_get_bb_reg(dm, REG_RX_POWER_BEFORE_IQK_B_2, MASKDWORD) & 0x3FF0000) >> 16;
				result[t][7] = (odm_get_bb_reg(dm, REG_RX_POWER_AFTER_IQK_B_2, MASKDWORD) & 0x3FF0000) >> 16;
				break;

			} else {
				RF_DBG(dm, DBG_RF_IQK,
				       "path B Rx IQK Fail!!\n");

				result[t][6] = 0x100;
				result[t][7] = 0x0;
			}
		}

		if (0x00 == path_bok)
			RF_DBG(dm, DBG_RF_IQK, "path B IQK failed!!\n");

#endif
	}

	/*Back to BB mode, load original value*/
	RF_DBG(dm, DBG_RF_IQK, "[IQK] Back to BB mode, load original value!\n");
	odm_set_bb_reg(dm, REG_FPGA0_IQK, 0xffffff00, 0x000000);

	if (t != 0) {
		/* Reload ADDA power saving parameters*/
		_phy_reload_adda_registers_97f(dm, ADDA_REG, dm->rf_calibrate_info.ADDA_backup, IQK_ADDA_REG_NUM);
		/* Reload MAC parameters*/
		_phy_reload_mac_registers_97f(dm, IQK_MAC_REG, dm->rf_calibrate_info.IQK_MAC_backup);
		_phy_reload_adda_registers_97f(dm, IQK_BB_REG_92C, dm->rf_calibrate_info.IQK_BB_backup, IQK_BB_REG_NUM);
		/*Allen initial gain 0xc50*/
		/* Restore RX initial gain*/
		odm_set_bb_reg(dm, R_0xc50, MASKBYTE0, 0x50);
		odm_set_bb_reg(dm, R_0xc50, MASKBYTE0, tmp0xc50);
		if (is2T) {
			odm_set_bb_reg(dm, R_0xc58, MASKBYTE0, 0x50);
			odm_set_bb_reg(dm, R_0xc58, MASKBYTE0, tmp0xc58);
		}
#if 0
		/*RF setting :RXBB enter power saving*/
		if (dm->cut_version != ODM_CUT_A) {
			odm_set_rf_reg(dm, RF_PATH_A, RF_0xdf, 0x00002, 0x0);
			odm_set_rf_reg(dm, RF_PATH_B, RF_0xdf, 0x00002, 0x0);
		}
#endif
		/*reload DAC gain for K-free*/
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x55, RFREGOFFSETMASK, DAC_gain_A);
		odm_set_rf_reg(dm, RF_PATH_B, RF_0x55, RFREGOFFSETMASK, DAC_gain_B);
		RF_DBG(dm, DBG_RF_IQK, "[IQK] Restore_0x55 at path A = 0x%x\n",
		       odm_get_rf_reg(dm, RF_PATH_A, RF_0x55, RFREGOFFSETMASK));
		RF_DBG(dm, DBG_RF_IQK, "[IQK] Restore_0x55 at path B = 0x%x\n",
		       odm_get_rf_reg(dm, RF_PATH_B, RF_0x55, RFREGOFFSETMASK));

#if 0
		/*load 0xe30 IQC default value*/
		odm_set_bb_reg(dm, REG_TX_IQK_TONE_A, MASKDWORD, 0x01008c00);
		odm_set_bb_reg(dm, REG_RX_IQK_TONE_A, MASKDWORD, 0x01008c00);
#endif
	}
	RF_DBG(dm, DBG_RF_IQK, "[IQK] %s <==\n", __func__);
}

void _phy_lc_calibrate_8197f(
	void *dm_void,
	boolean is2T)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	u8 tmp_reg;
	u32 rf_amode = 0, rf_bmode = 0, lc_cal, cnt;

	/*Check continuous TX and Packet TX*/
	tmp_reg = odm_read_1byte(dm, 0xd03);

	if ((tmp_reg & 0x70) != 0) /*Deal with contisuous TX case*/
		odm_write_1byte(dm, 0xd03, tmp_reg & 0x8F); /*disable all continuous TX*/
	else /* Deal with Packet TX case*/
		odm_write_1byte(dm, REG_TXPAUSE, 0xFF); /* block all queues*/

	/*backup RF0x18*/
	lc_cal = odm_get_rf_reg(dm, RF_PATH_A, RF_CHNLBW, RFREGOFFSETMASK);

	/*Start LCK*/
	odm_set_rf_reg(dm, RF_PATH_A, RF_CHNLBW, RFREGOFFSETMASK, lc_cal | 0x08000);

	for (cnt = 0; cnt < 100; cnt++) {
		if (odm_get_rf_reg(dm, RF_PATH_A, RF_CHNLBW, 0x8000) != 0x1)
			break;

		ODM_delay_ms(10);
	}

	/*Recover channel number*/
	odm_set_rf_reg(dm, RF_PATH_A, RF_CHNLBW, RFREGOFFSETMASK, lc_cal);

	/*Restore original situation*/
	if ((tmp_reg & 0x70) != 0) {
		/*Deal with contisuous TX case*/
		odm_write_1byte(dm, 0xd03, tmp_reg);
	} else {
		/* Deal with Packet TX case*/
		odm_write_1byte(dm, REG_TXPAUSE, 0x00);
	}
}

void phy_iq_calibrate_8197f(
	void *dm_void,
	boolean is_recovery)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct _hal_rf_ *rf = &(dm->rf_table);
	struct dm_rf_calibration_struct *cali_info = &(dm->rf_calibrate_info);
	struct dm_iqk_info *iqk_info = &dm->IQK_info;

	s32 result[4][8]; /*last is final result*/
	u8 i, final_candidate, indexforchannel;
	u8 channel_to_iqk = 7;
	boolean is_patha_ok, is_pathb_ok;
	s32 rege94, rege9c, regea4, regeac, regeb4, regebc, regec4, regecc;
	boolean is12simular, is13simular, is23simular;
	boolean is_start_cont_tx = false, is_single_tone = false, is_carrier_suppression = false;
	u32 IQK_BB_REG_92C[IQK_BB_REG_NUM] = {
		REG_OFDM_0_XA_RX_IQ_IMBALANCE, REG_OFDM_0_XB_RX_IQ_IMBALANCE,
		REG_OFDM_0_ECCA_THRESHOLD, 0xca8,
		REG_OFDM_0_XA_TX_IQ_IMBALANCE, REG_OFDM_0_XB_TX_IQ_IMBALANCE,
		REG_OFDM_0_XC_TX_AFE, REG_OFDM_0_XD_TX_AFE,
		REG_OFDM_0_RX_IQ_EXT_ANTA};

	if (is_recovery) {
		RF_DBG(dm, DBG_RF_INIT,
		       "[IQK] PHY_IQCalibrate_97F: Return due to is_recovery!\n");
#if 0
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
		_phy_reload_adda_registers_97f(adapter, IQK_BB_REG_92C, dm->rf_calibrate_info.IQK_BB_backup_recover, 9);
#else
		_phy_reload_adda_registers_97f(dm, IQK_BB_REG_92C, dm->rf_calibrate_info.IQK_BB_backup_recover, 9);
#endif
#endif
		return;
	}
#ifdef CONFIG_RF_DPK_SETTING_SUPPORT
	/*turn off DPK*/
	phy_dpkoff_8197f(dm);
#endif

	/*check IC cut and IQK version*/
	if (dm->cut_version == ODM_CUT_A)
		RF_DBG(dm, DBG_RF_IQK, "[IQK] Cv is A\n");
	else if (dm->cut_version == ODM_CUT_B)
		RF_DBG(dm, DBG_RF_IQK, "[IQK] Cv is B\n");
	else if (dm->cut_version == ODM_CUT_C)
		RF_DBG(dm, DBG_RF_IQK, "[IQK] Cv is C\n");
	else if (dm->cut_version == ODM_CUT_D)
		RF_DBG(dm, DBG_RF_IQK, "[IQK] Cv is D\n");

	RF_DBG(dm, DBG_RF_IQK, "[IQK] IQK version is v2.8 (20180423)\n");
	RF_DBG(dm, DBG_RF_IQK, "[IQK] PHY version is v%d\n",
	       odm_get_hw_img_version(dm));
	RF_DBG(dm, DBG_RF_IQK, "[IQK] ext_pa = %d, ext_lna = %d\n", dm->ext_pa,
	       dm->ext_lna);
	RF_DBG(dm, DBG_RF_IQK, "[IQK] IQK Start!!!\n");
	iqk_info->iqk_times++;

#if 0
	odm_acquire_spin_lock(dm, RT_IQK_SPINLOCK);
	cali_info->is_iqk_in_progress = true;
	odm_release_spin_lock(dm, RT_IQK_SPINLOCK);
#endif

	for (i = 0; i < 8; i++) {
		result[0][i] = 0;
		result[1][i] = 0;
		result[2][i] = 0;

		if (i == 0 || i == 2 || i == 4 || i == 6)
			result[3][i] = 0x100;
		else
			result[3][i] = 0;
	}

	final_candidate = 0xff;
	is_patha_ok = false;
	is_pathb_ok = false;
	is12simular = false;
	is23simular = false;
	is13simular = false;

	for (i = 0; i < 3; i++) {
		_phy_iq_calibrate_8197f(dm, result, i, true);
		if (i == 1) {
			is12simular = phy_simularity_compare_8197f(dm, result, 0, 1);
			if (is12simular) {
				final_candidate = 0;
				RF_DBG(dm, DBG_RF_IQK,
				       "[IQK] is12simular final_candidate is %x\n",
				       final_candidate);
				break;
			}
		}

		if (i == 2) {
			is13simular = phy_simularity_compare_8197f(dm, result, 0, 2);
			if (is13simular) {
				final_candidate = 0;
				RF_DBG(dm, DBG_RF_IQK,
				       "[IQK] is13simular final_candidate is %x\n",
				       final_candidate);
				break;
			}
			is23simular = phy_simularity_compare_8197f(dm, result, 1, 2);
			if (is23simular) {
				final_candidate = 1;
				RF_DBG(dm, DBG_RF_IQK,
				       "[IQK] is23simular final_candidate is %x\n",
				       final_candidate);
			} else {
#if 0
				for (i = 0; i < 4; i++)
					reg_tmp &= result[3][i * 2];

				if (reg_tmp != 0)
					final_candidate = 3;
				else
					final_candidate = 0xFF;
#endif
				final_candidate = 3;
			}
		}
	}
#if 0
	if ((result[final_candidate][0] | result[final_candidate][2] | result[final_candidate][4] | result[final_candidate][6]) == 0) {
		for (i = 0; i < 8; i++) {
			if (i == 0 || i == 2 || i == 4  || i == 6)
				result[final_candidate][i] = 0x100;
			else
				result[final_candidate][i] = 0;
		}
	}
#endif

	for (i = 0; i < 4; i++) {
		rege94 = result[i][0];
		rege9c = result[i][1];
		regea4 = result[i][2];
		regeac = result[i][3];
		regeb4 = result[i][4];
		regebc = result[i][5];
		regec4 = result[i][6];
		regecc = result[i][7];
		RF_DBG(dm, DBG_RF_IQK,
		       "[IQK] rege94=%x rege9c=%x regea4=%x regeac=%x regeb4=%x regebc=%x regec4=%x regecc=%x\n ",
		       rege94, rege9c, regea4, regeac, regeb4, regebc, regec4,
		       regecc);
	}

	if (final_candidate != 0xff) {
		dm->rf_calibrate_info.rege94 = rege94 = result[final_candidate][0];
		dm->rf_calibrate_info.rege9c = rege9c = result[final_candidate][1];
		regea4 = result[final_candidate][2];
		regeac = result[final_candidate][3];
		dm->rf_calibrate_info.regeb4 = regeb4 = result[final_candidate][4];
		dm->rf_calibrate_info.regebc = regebc = result[final_candidate][5];
		regec4 = result[final_candidate][6];
		regecc = result[final_candidate][7];
		RF_DBG(dm, DBG_RF_IQK, "[IQK] final_candidate is %x\n",
		       final_candidate);
		RF_DBG(dm, DBG_RF_IQK,
		       "[IQK] TX0_X=%x TX0_Y=%x RX0_X=%x RX0_Y=%x TX1_X=%x TX1_Y=%x RX1_X=%x RX1_Y=%x\n ",
		       rege94, rege9c, regea4, regeac, regeb4, regebc, regec4,
		       regecc);
		is_patha_ok = is_pathb_ok = true;
	} else {
		/*RF_DBG(dm, DBG_RF_IQK, "[IQK] FAIL use default value\n");*/
		_phy_iqk_check_97f(dm);
		dm->rf_calibrate_info.rege94 = dm->rf_calibrate_info.regeb4 = 0x100; /*X default value*/
		dm->rf_calibrate_info.rege9c = dm->rf_calibrate_info.regebc = 0x0; /*Y default value*/
		/*priv->pshare->IQK_fail_cnt++;*/
	}

	if ((rege94 != 0) /*&&(regea4 != 0)*/)
		_phy_path_a_fill_iqk_matrix_97f(dm, is_patha_ok, result, final_candidate, (regea4 == 0));

	if (regeb4 != 0 /*&&(regec4 != 0)*/)
		_phy_path_b_fill_iqk_matrix_97f(dm, is_pathb_ok, result, final_candidate, (regec4 == 0));

	indexforchannel = 0;

	/*To Fix BSOD when final_candidate is 0xff*/
	/*by sherry 20120321*/
	if (final_candidate < 4) {
		for (i = 0; i < iqk_matrix_reg_num; i++)
			dm->rf_calibrate_info.iqk_matrix_reg_setting[indexforchannel].value[0][i] = result[final_candidate][i];
		dm->rf_calibrate_info.iqk_matrix_reg_setting[indexforchannel].is_iqk_done = true;
	}
	/*RT_DISP(FINIT, INIT_IQK, ("\nIQK OK indexforchannel %d.\n", indexforchannel));*/
	RF_DBG(dm, DBG_RF_IQK, "[IQK] IQK OK indexforchannel %d.\n",
	       indexforchannel);

	_phy_save_adda_registers_97f(dm, IQK_BB_REG_92C, dm->rf_calibrate_info.IQK_BB_backup_recover, IQK_BB_REG_NUM);

	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0xc80 = 0x%x, 0xc94 = 0x%x\n",
	       odm_get_bb_reg(dm, R_0xc80, MASKDWORD),
	       odm_get_bb_reg(dm, R_0xc94, MASKDWORD));
	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0xc14 = 0x%x, 0xca0 = 0x%x\n",
	       odm_get_bb_reg(dm, R_0xc14, MASKDWORD),
	       odm_get_bb_reg(dm, R_0xca0, MASKDWORD));
	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0xc88 = 0x%x, 0xc9c = 0x%x\n",
	       odm_get_bb_reg(dm, R_0xc88, MASKDWORD),
	       odm_get_bb_reg(dm, R_0xc9c, MASKDWORD));
	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0xc1c = 0x%x, 0xca8 = 0x%x\n",
	       odm_get_bb_reg(dm, R_0xc1c, MASKDWORD),
	       odm_get_bb_reg(dm, R_0xca8, MASKDWORD));
	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0x58 at path A = 0x%x\n",
	       odm_get_rf_reg(dm, RF_PATH_A, RF_0x58, RFREGOFFSETMASK));
	RF_DBG(dm, DBG_RF_IQK, "[IQK] 0x58 at path B = 0x%x\n",
	       odm_get_rf_reg(dm, RF_PATH_B, RF_0x58, RFREGOFFSETMASK));
	RF_DBG(dm, DBG_RF_IQK, "[IQK] IQK finished\n");

#ifdef CONFIG_RF_DPK_SETTING_SUPPORT
	/*trun on DPK*/
	phy_dpkon_8197f(dm);
#endif
}

void phy_lc_calibrate_8197f(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	_phy_lc_calibrate_8197f(dm, false);
}
