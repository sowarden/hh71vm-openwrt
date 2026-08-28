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
#include "../../phydm_precomp.h"

#if (RTL8197F_SUPPORT == 1)

void set_iqk_matrix_8197f(
	void *dm_void,
	u8 OFDM_index,
	u8 rf_path,
	s32 iqk_result_x,
	s32 iqk_result_y)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	s32 ele_A = 0, ele_D, ele_C = 0, value32;

	ele_D = (ofdm_swing_table_new[OFDM_index] & 0xFFC00000) >> 22;

	/*new element A = element D x X*/
	if (iqk_result_x != 0) {
		if ((iqk_result_x & 0x00000200) != 0) /*consider minus*/
			iqk_result_x = iqk_result_x | 0xFFFFFC00;
		ele_A = ((iqk_result_x * ele_D) >> 8) & 0x000003FF;

		/*new element C = element D x Y*/
		if ((iqk_result_y & 0x00000200) != 0)
			iqk_result_y = iqk_result_y | 0xFFFFFC00;
		ele_C = ((iqk_result_y * ele_D) >> 8) & 0x000003FF;

		/*if (rf_path == RF_PATH_A)// Remove this to Fix path B PowerTracking */
		switch (rf_path) {
		case RF_PATH_A:
			/*write new elements A, C, D to regC80 and regC94, element B is always 0*/
			value32 = (ele_D << 22) | ((ele_C & 0x3F) << 16) | ele_A;
			odm_set_bb_reg(dm, REG_OFDM_0_XA_TX_IQ_IMBALANCE, MASKDWORD, value32);

			value32 = (ele_C & 0x000003C0) >> 6;
			odm_set_bb_reg(dm, REG_OFDM_0_XC_TX_AFE, MASKH4BITS, value32);

			value32 = ((iqk_result_x * ele_D) >> 7) & 0x01;
			odm_set_bb_reg(dm, REG_OFDM_0_ECCA_THRESHOLD, BIT(24), value32);
			break;
		case RF_PATH_B:
			/*write new elements A, C, D to regC88 and regC9C, element B is always 0*/
			value32 = (ele_D << 22) | ((ele_C & 0x3F) << 16) | ele_A;
			odm_set_bb_reg(dm, REG_OFDM_0_XB_TX_IQ_IMBALANCE, MASKDWORD, value32);

			value32 = (ele_C & 0x000003C0) >> 6;
			odm_set_bb_reg(dm, REG_OFDM_0_XD_TX_AFE, MASKH4BITS, value32);

			value32 = ((iqk_result_x * ele_D) >> 7) & 0x01;
			odm_set_bb_reg(dm, REG_OFDM_0_ECCA_THRESHOLD, BIT(28), value32);

			break;
		default:
			break;
		}
	} else {
		switch (rf_path) {
		case RF_PATH_A:
			odm_set_bb_reg(dm, REG_OFDM_0_XA_TX_IQ_IMBALANCE, MASKDWORD, ofdm_swing_table_new[OFDM_index]);
			odm_set_bb_reg(dm, REG_OFDM_0_XC_TX_AFE, MASKH4BITS, 0x00);
			odm_set_bb_reg(dm, REG_OFDM_0_ECCA_THRESHOLD, BIT(24), 0x00);
			break;

		case RF_PATH_B:
			odm_set_bb_reg(dm, REG_OFDM_0_XB_TX_IQ_IMBALANCE, MASKDWORD, ofdm_swing_table_new[OFDM_index]);
			odm_set_bb_reg(dm, REG_OFDM_0_XD_TX_AFE, MASKH4BITS, 0x00);
			odm_set_bb_reg(dm, REG_OFDM_0_ECCA_THRESHOLD, BIT(28), 0x00);
			break;

		default:
			break;
		}
	}

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
	       "TxPwrTracking path %c: X = 0x%x, Y = 0x%x ele_A = 0x%x ele_C = 0x%x ele_D = 0x%x 0xeb4 = 0x%x 0xebc = 0x%x\n",
	       (rf_path == RF_PATH_A ? 'A' : 'B'), (u32)iqk_result_x,
	       (u32)iqk_result_y, (u32)ele_A, (u32)ele_C, (u32)ele_D,
	       (u32)iqk_result_x, (u32)iqk_result_y);
}

#if 0
void
set_cck_filter_coefficient_8197f(
	void		*dm_void,
	u8		cck_swing_index
)
{
	struct dm_struct	*dm = (struct dm_struct *)dm_void;
	struct rtl8192cd_priv *priv = dm->priv;

	unsigned char channel;

#ifdef MP_TEST
	if ((OPMODE & WIFI_MP_STATE) || *(dm->mp_mode))
		channel = priv->pshare->working_channel;
	else
#endif
		channel = (priv->pmib->dot11RFEntry.dot11channel);

	if (channel != 14) {
		odm_write_1byte(dm, 0xa22, cck_swing_table_ch1_ch13_new[cck_swing_index][0]);
		odm_write_1byte(dm, 0xa23, cck_swing_table_ch1_ch13_new[cck_swing_index][1]);
		odm_write_1byte(dm, 0xa24, cck_swing_table_ch1_ch13_new[cck_swing_index][2]);
		odm_write_1byte(dm, 0xa25, cck_swing_table_ch1_ch13_new[cck_swing_index][3]);
		odm_write_1byte(dm, 0xa26, cck_swing_table_ch1_ch13_new[cck_swing_index][4]);
		odm_write_1byte(dm, 0xa27, cck_swing_table_ch1_ch13_new[cck_swing_index][5]);
		odm_write_1byte(dm, 0xa28, cck_swing_table_ch1_ch13_new[cck_swing_index][6]);
		odm_write_1byte(dm, 0xa29, cck_swing_table_ch1_ch13_new[cck_swing_index][7]);
		RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
		       "CCK channel=%d 0xa20=0x%x 0xa24=0x%x 0xa28=0x%x\n",
		       channel, odm_get_bb_reg(dm, R_0xa20, MASKDWORD),
		       odm_get_bb_reg(dm, R_0xa24, MASKDWORD),
		       odm_get_bb_reg(dm, R_0xa28, MASKDWORD));
	} else {
		odm_write_1byte(dm, 0xa22, cck_swing_table_ch14_new[cck_swing_index][0]);
		odm_write_1byte(dm, 0xa23, cck_swing_table_ch14_new[cck_swing_index][1]);
		odm_write_1byte(dm, 0xa24, cck_swing_table_ch14_new[cck_swing_index][2]);
		odm_write_1byte(dm, 0xa25, cck_swing_table_ch14_new[cck_swing_index][3]);
		odm_write_1byte(dm, 0xa26, cck_swing_table_ch14_new[cck_swing_index][4]);
		odm_write_1byte(dm, 0xa27, cck_swing_table_ch14_new[cck_swing_index][5]);
		odm_write_1byte(dm, 0xa28, cck_swing_table_ch14_new[cck_swing_index][6]);
		odm_write_1byte(dm, 0xa29, cck_swing_table_ch14_new[cck_swing_index][7]);
		RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
		       "CCK channel=%d 0xa20=0x%x 0xa24=0x%x 0xa28=0x%x\n",
		       channel, odm_get_bb_reg(dm, R_0xa20, MASKDWORD),
		       odm_get_bb_reg(dm, R_0xa24, MASKDWORD),
		       odm_get_bb_reg(dm, R_0xa28, MASKDWORD));
	}
}
#endif

void odm_tx_pwr_track_set_pwr8197f(
	void *dm_void,
	enum pwrtrack_method method,
	u8 rf_path,
	u8 channel_mapped_index)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_rf_calibration_struct *cali_info = &(dm->rf_calibrate_info);
	struct rtl8192cd_priv *priv = dm->priv;
	s8 final_ofdm_swing_index = 0;
	s8 final_cck_swing_index = 0;
	u32 bit_mask_10_0 = (BIT(10) | BIT(9) | BIT(8) | BIT(7) | BIT(6) | BIT(5) | BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0));
	u32 bit_mask_21_11 = (BIT(21) | BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(14) | BIT(13) | BIT(12) | BIT(11));

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
	       "pRF->default_ofdm_index=%d   pRF->default_cck_index=%d\n",
	       cali_info->default_ofdm_index, cali_info->default_cck_index);

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
	       "pRF->absolute_ofdm_swing_idx=%d   pRF->remnant_ofdm_swing_idx=%d   pRF->absolute_cck_swing_idx=%d   pRF->remnant_cck_swing_idx=%d   rf_path=%d\n",
	       cali_info->absolute_ofdm_swing_idx[rf_path],
	       cali_info->remnant_ofdm_swing_idx[rf_path],
	       cali_info->absolute_cck_swing_idx[rf_path],
	       cali_info->remnant_cck_swing_idx, rf_path);

	final_ofdm_swing_index = cali_info->default_ofdm_index + cali_info->absolute_ofdm_swing_idx[rf_path];
	final_cck_swing_index = cali_info->default_cck_index + cali_info->absolute_cck_swing_idx[rf_path];

	/*OFDM BB-Swing index Limit*/
	if (final_ofdm_swing_index < 0)
		final_ofdm_swing_index = 0;
	else if (final_ofdm_swing_index > OFDM_TABLE_SIZE_92D - 1)
		final_ofdm_swing_index = OFDM_TABLE_SIZE_92D - 1;

	/*CCK BB-Swing index Limit*/
	if (final_cck_swing_index < 0)
		final_cck_swing_index = 0;
	else if (final_cck_swing_index > CCK_TABLE_SIZE_8723D - 1)
		final_cck_swing_index = CCK_TABLE_SIZE_8723D - 1;

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
	       "final_ofdm_swing_index=%d	 final_cck_swing_index=%d rf_path=%d\n",
	       final_ofdm_swing_index, final_cck_swing_index, rf_path);

	if (method == BBSWING) {
		switch (rf_path) {
		case RF_PATH_A:
#ifdef CONFIG_RF_DPK_SETTING_SUPPORT
			odm_set_bb_reg(dm, R_0xc80, MASKDWORD, ofdm_swing_table_new[final_ofdm_swing_index]);
#else
			set_iqk_matrix_8197f(dm_void, final_ofdm_swing_index, rf_path, priv->pshare->rege94, priv->pshare->rege9c);
#endif
			/*			set_cck_filter_coefficient_8197f(dm_void, final_cck_swing_index);*/
			odm_set_bb_reg(dm, R_0xab4, bit_mask_10_0, cck_swing_table_ch1_ch14_8723d[final_cck_swing_index]);

			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
			       "BBSwing=0x%x CCKBBSwing=0x%x rf_path=%d\n",
			       odm_get_bb_reg(dm, REG_OFDM_0_XA_TX_IQ_IMBALANCE,
			       MASKDWORD),
			       odm_get_bb_reg(dm, R_0xab4, bit_mask_10_0),
			       rf_path);
			break;

		case RF_PATH_B:
#ifdef CONFIG_RF_DPK_SETTING_SUPPORT
			odm_set_bb_reg(dm, R_0xc88, MASKDWORD, ofdm_swing_table_new[final_ofdm_swing_index]);
#else
			set_iqk_matrix_8197f(dm_void, final_ofdm_swing_index, rf_path, priv->pshare->regeb4, priv->pshare->regebc);
#endif
			/*			set_cck_filter_coefficient_8197f(dm_void, final_cck_swing_index);*/
			odm_set_bb_reg(dm, R_0xab4, bit_mask_21_11, cck_swing_table_ch1_ch14_8723d[final_cck_swing_index]);

			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
			       "BBSwing=0x%x CCKBBSwing=0x%x rf_path=%d\n",
			       odm_get_bb_reg(dm, REG_OFDM_0_XB_TX_IQ_IMBALANCE,
			       MASKDWORD),
			       odm_get_bb_reg(dm, R_0xab4, bit_mask_21_11),
			       rf_path);
			break;

		default:
			break;
		}
	}
}

void get_delta_swing_table_8197f(
	void *dm_void,
	u8 **temperature_up_a,
	u8 **temperature_down_a,
	u8 **temperature_up_b,
	u8 **temperature_down_b,
	u8 **temperature_up_cck_a,
	u8 **temperature_down_cck_a,
	u8 **temperature_up_cck_b,
	u8 **temperature_down_cck_b)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_rf_calibration_struct *cali_info = &(dm->rf_calibrate_info);
	u8 channel = *(dm->channel);

	if (channel >= 1 && channel <= 14) {
		*temperature_up_cck_a = cali_info->delta_swing_table_idx_2g_cck_a_p;
		*temperature_down_cck_a = cali_info->delta_swing_table_idx_2g_cck_a_n;
		*temperature_up_cck_b = cali_info->delta_swing_table_idx_2g_cck_b_p;
		*temperature_down_cck_b = cali_info->delta_swing_table_idx_2g_cck_b_n;

		*temperature_up_a = cali_info->delta_swing_table_idx_2ga_p;
		*temperature_down_a = cali_info->delta_swing_table_idx_2ga_n;
		*temperature_up_b = cali_info->delta_swing_table_idx_2gb_p;
		*temperature_down_b = cali_info->delta_swing_table_idx_2gb_n;
	} else {
		*temperature_up_cck_a = (u8 *)delta_swing_table_idx_2ga_p_default;
		*temperature_down_cck_a = (u8 *)delta_swing_table_idx_2ga_n_default;
		*temperature_up_cck_b = (u8 *)delta_swing_table_idx_2ga_p_default;
		*temperature_down_cck_b = (u8 *)delta_swing_table_idx_2ga_n_default;
		*temperature_up_a = (u8 *)delta_swing_table_idx_2ga_p_default;
		*temperature_down_a = (u8 *)delta_swing_table_idx_2ga_n_default;
		*temperature_up_b = (u8 *)delta_swing_table_idx_2ga_p_default;
		*temperature_down_b = (u8 *)delta_swing_table_idx_2ga_n_default;
	}

	return;
}

void configure_txpower_track_8197f(
	struct txpwrtrack_cfg *config)
{
	config->swing_table_size_cck = TXSCALE_TABLE_SIZE;
	config->swing_table_size_ofdm = TXSCALE_TABLE_SIZE;
	config->threshold_iqk = IQK_THRESHOLD;
	config->threshold_dpk = DPK_THRESHOLD;
	config->average_thermal_num = AVG_THERMAL_NUM_8197F;
	config->rf_path_count = MAX_PATH_NUM_8197F;
	config->thermal_reg_addr = RF_T_METER_8197F;

	config->odm_tx_pwr_track_set_pwr = odm_tx_pwr_track_set_pwr8197f;
	config->do_iqk = do_iqk_8197f;
	config->phy_lc_calibrate = halrf_lck_trigger;
	config->get_delta_all_swing_table = get_delta_swing_table_8197f;
}

#if 0

void phy_set_rf_path_switch_8197f(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	struct dm_struct		*dm,
#else
	void	*adapter,
#endif
	boolean		is_main
)
{
}

boolean
_phy_query_rf_path_switch_8197f(
	void	*adapter
)
{
	return true;
}


boolean phy_query_rf_path_switch_8197f(
	void	*adapter
)
{
#if DISABLE_BB_RF
	return true;
#endif

	return _phy_query_rf_path_switch_8197f(adapter);
}
#endif

#endif /* (RTL8197F_SUPPORT == 0)*/
