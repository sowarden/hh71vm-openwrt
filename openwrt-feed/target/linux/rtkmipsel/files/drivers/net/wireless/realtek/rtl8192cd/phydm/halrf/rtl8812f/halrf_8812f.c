/******************************************************************************
 *
 * Copyright(c) 2007 - 2017  Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/

#include "mp_precomp.h"
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
#if RT_PLATFORM == PLATFORM_MACOSX
#include "phydm_precomp.h"
#else
#include "../phydm_precomp.h"
#endif
#else
#include "../../phydm_precomp.h"
#endif

#if (RTL8812F_SUPPORT == 1)
void halrf_rf_lna_setting_8812f(struct dm_struct *dm_void,
				enum halrf_lna_set type)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 path = 0x0;

	for (path = 0x0; path < 2; path++)
		if (type == HALRF_LNA_DISABLE) {
			/*S0*/
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(19),
				       0x1);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x33,
				       RFREGOFFSETMASK, 0x00003);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x3e,
				       RFREGOFFSETMASK, 0x00064);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x3f,
				       RFREGOFFSETMASK, 0x0afce);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(19),
				       0x0);
		} else if (type == HALRF_LNA_ENABLE) {
			/*S0*/
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(19),
				       0x1);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x33,
				       RFREGOFFSETMASK, 0x00003);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x3e,
				       RFREGOFFSETMASK, 0x00064);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0x3f,
				       RFREGOFFSETMASK, 0x1afce);
			odm_set_rf_reg(dm, (enum rf_path)path, RF_0xef, BIT(19),
				       0x0);
		}
}

boolean get_mix_mode_tx_agc_bb_swing_offset_8812f(void *dm_void,
						  enum pwrtrack_method method,
						  u8 rf_path,
						  u8 tx_power_index_offset)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_rf_calibration_struct *cali_info = &dm->rf_calibrate_info;

	u8 bb_swing_upper_bound = cali_info->default_ofdm_index + 10;
	u8 bb_swing_lower_bound = 0;

	s8 tx_agc_index = 0;
	u8 tx_bb_swing_index = cali_info->default_ofdm_index;

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
	       "Path_%d absolute_ofdm_swing[%d]=%d tx_power_idx_offset=%d\n",
	       rf_path, rf_path, cali_info->absolute_ofdm_swing_idx[rf_path],
	       tx_power_index_offset);

	if (tx_power_index_offset > 0XF)
		tx_power_index_offset = 0XF;

	if (cali_info->absolute_ofdm_swing_idx[rf_path] >= 0 &&
	    cali_info->absolute_ofdm_swing_idx[rf_path] <=
		    tx_power_index_offset) {
		tx_agc_index = cali_info->absolute_ofdm_swing_idx[rf_path];
		tx_bb_swing_index = cali_info->default_ofdm_index;
	} else if (cali_info->absolute_ofdm_swing_idx[rf_path] >
		   tx_power_index_offset) {
		tx_agc_index = tx_power_index_offset;
		cali_info->remnant_ofdm_swing_idx[rf_path] =
			cali_info->absolute_ofdm_swing_idx[rf_path] -
			tx_power_index_offset;
		tx_bb_swing_index = cali_info->default_ofdm_index +
				    cali_info->remnant_ofdm_swing_idx[rf_path];

		if (tx_bb_swing_index > bb_swing_upper_bound)
			tx_bb_swing_index = bb_swing_upper_bound;
	} else {
		tx_agc_index = 0;

		if (cali_info->default_ofdm_index >
		    (cali_info->absolute_ofdm_swing_idx[rf_path] * (-1)))
			tx_bb_swing_index =
				cali_info->default_ofdm_index +
				cali_info->absolute_ofdm_swing_idx[rf_path];
		else
			tx_bb_swing_index = bb_swing_lower_bound;

		if (tx_bb_swing_index < bb_swing_lower_bound)
			tx_bb_swing_index = bb_swing_lower_bound;
	}

	cali_info->absolute_ofdm_swing_idx[rf_path] = tx_agc_index;
	cali_info->bb_swing_idx_ofdm[rf_path] = tx_bb_swing_index;

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
	       "absolute_ofdm[%d]=%d bb_swing_ofdm[%d]=%d tx_pwr_offset=%d\n",
	       rf_path, cali_info->absolute_ofdm_swing_idx[rf_path],
	       rf_path, cali_info->bb_swing_idx_ofdm[rf_path],
	       tx_power_index_offset);

	return true;
}

void odm_pwrtrack_method_set_pwr8812f(void *dm_void,
				      enum pwrtrack_method method,
				      u8 rf_path, u8 tx_pwr_idx_offset)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_rf_calibration_struct *cali_info = &dm->rf_calibrate_info;
	struct _hal_rf_ *rf = &dm->rf_table;
	struct _halrf_tssi_data *tssi = &rf->halrf_tssi_data;

	u32 tmp_reg1, tmp_reg2, tmp_reg3;
	u8 bb_swing_idx_ofdm = 0;
	u32 bitmask_6_0 = BIT(6) | BIT(5) | BIT(4) | BIT(3) |
			BIT(2) | BIT(1) | BIT(0);

	/*use for mp driver clean power tracking status*/
	if (method == CLEAN_MODE) { /*use for mp driver clean power tracking status*/
		RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "===> %s method=%d clear power tracking rf_path=%d\n",
		       __func__, method, rf_path);
		tssi->tssi_trk_txagc_offset[rf_path] = 0;

		switch (rf_path) {
		case RF_PATH_A:
			odm_set_bb_reg(dm, R_0x18a0, bitmask_6_0, (cali_info->absolute_ofdm_swing_idx[rf_path] & 0x7f));
			odm_set_rf_reg(dm, rf_path, RF_0x7f, 0x00002, 0x0);
			odm_set_rf_reg(dm, rf_path, RF_0x7f, 0x00100, 0x0);
			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
			       "Path-%d 0x%x=0x%x\n", rf_path, R_0x18a0,
			       odm_get_bb_reg(dm, R_0x18a0, bitmask_6_0));
			break;
		case RF_PATH_B:
			odm_set_bb_reg(dm, R_0x41a0, bitmask_6_0, (cali_info->absolute_ofdm_swing_idx[rf_path] & 0x7f));
			odm_set_rf_reg(dm, rf_path, RF_0x7f, 0x00002, 0x0);
			odm_set_rf_reg(dm, rf_path, RF_0x7f, 0x00100, 0x0);
			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
			       "Path-%d 0x%x=0x%x\n", rf_path, R_0x41a0,
			       odm_get_bb_reg(dm, R_0x41a0, bitmask_6_0));
			break;
		default:
			break;
		}
	} else if (method == BBSWING) {
#if 0
		if (rf_path == RF_PATH_A) {
			tmp_reg1 = R_0xc94;
			tmp_reg2 = REG_A_TX_SCALE_JAGUAR;
		} else if (rf_path == RF_PATH_B) {
			tmp_reg1 = R_0xe94;
			tmp_reg2 = REG_B_TX_SCALE_JAGUAR;
		} else {
			return;
		}

		odm_set_bb_reg(dm, tmp_reg1,
			       BIT(29) | BIT(28) | BIT(27) | BIT(26) | BIT(25),
			       cali_info->absolute_ofdm_swing_idx[rf_path]);
		odm_set_bb_reg(dm, tmp_reg2, 0xFFE00000,
			       tx_scaling_table_jaguar[bb_swing_idx_ofdm]);
#else
		///halrf_disable_tssi_8812f(dm);
#endif
	} else if (method == MIX_MODE) {
		RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "===> %s method=%d MIX_MODE power tracking rf_path=%d\n",
		       __func__, method, rf_path);

		switch (rf_path) {
		case RF_PATH_A:
			odm_set_bb_reg(dm, R_0x18a0, bitmask_6_0, (cali_info->absolute_ofdm_swing_idx[rf_path] & 0x7f));
			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
			       "Path-%d 0x%x=0x%x\n", rf_path, R_0x18a0,
			       odm_get_bb_reg(dm, R_0x18a0, bitmask_6_0));
			break;
		case RF_PATH_B:
			odm_set_bb_reg(dm, R_0x41a0, bitmask_6_0, (cali_info->absolute_ofdm_swing_idx[rf_path] & 0x7f));
			RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
			       "Path-%d 0x%x=0x%x\n", rf_path, R_0x41a0,
			       odm_get_bb_reg(dm, R_0x41a0, bitmask_6_0));
			break;
		default:
			break;
		}

	}
}

void odm_tx_pwr_track_set_pwr8812f(void *dm_void, enum pwrtrack_method method,
				   u8 rf_path, u8 channel_mapped_index)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 tx_pwr_idx_offset = 0;
#if 0
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
	struct _ADAPTER *adapter = dm->adapter;
#endif
	struct dm_rf_calibration_struct *cali_info = &dm->rf_calibrate_info;
	struct _hal_rf_ *rf = &dm->rf_table;
	u8 tx_pwr_idx_offset = 0;
	u8 tx_pwr_idx = 0;
	u8 mpt_rate_index = 0;
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
	u8 channel = *dm->channel;
	u8 band_width = *dm->band_width;
	u8 tx_rate = 0xFF;

	if (*dm->mp_mode == 1) {
#if (DM_ODM_SUPPORT_TYPE & (ODM_WIN | ODM_CE))
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
#if (MP_DRIVER == 1)
		PMPT_CONTEXT p_mpt_ctx = &adapter->MptCtx;

		tx_rate = MptToMgntRate(p_mpt_ctx->MptRateIndex);
#endif
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
#ifdef CONFIG_MP_INCLUDED
		if (rf->mp_rate_index)
			mpt_rate_index = *rf->mp_rate_index;

		tx_rate = mpt_to_mgnt_rate(mpt_rate_index);
#endif
#endif
#endif
	} else {
		u16 rate = *dm->forced_data_rate;

		if (!rate) { /*auto rate*/
#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
			struct _ADAPTER *adapter = dm->adapter;

			tx_rate = ((PADAPTER)adapter)->HalFunc.GetHwRateFromMRateHandler(dm->tx_rate);
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE) && defined(DM_ODM_CE_MAC80211)
			tx_rate = dm->tx_rate;
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
			if (dm->number_linked_client != 0)
				tx_rate = hw_rate_to_m_rate(dm->tx_rate);
			else
				tx_rate = rf->p_rate_index;
#endif
		} else { /*force rate*/
			tx_rate = (u8)rate;
		}
	}

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK, "Call:%s tx_rate=0x%X\n", __func__,
	       tx_rate);
#endif

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
	       "pRF->default_ofdm_index=%d   pRF->default_cck_index=%d\n",
	       cali_info->default_ofdm_index, cali_info->default_cck_index);

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
	       "absolute_ofdm_swing_idx=%d remnant_ofdm_swing_idx=%d path=%d\n",
	       cali_info->absolute_ofdm_swing_idx[rf_path],
	       cali_info->remnant_ofdm_swing_idx[rf_path], rf_path);

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
	       "absolute_cck_swing_idx=%d remnant_cck_swing_idx=%d path=%d\n",
	       cali_info->absolute_cck_swing_idx[rf_path],
	       cali_info->remnant_cck_swing_idx, rf_path);

#if (DM_ODM_SUPPORT_TYPE & ODM_WIN)
	tx_pwr_idx = odm_get_tx_power_index(dm, (enum rf_path)rf_path, tx_rate, (enum channel_width)band_width, channel);
#elif (DM_ODM_SUPPORT_TYPE & ODM_CE)
	tx_pwr_idx = odm_get_tx_power_index(dm, (enum rf_path)rf_path,
					    tx_rate, band_width, channel);
#else
	/*0x04(TX_AGC_OFDM_6M)*/
	tx_pwr_idx = config_phydm_read_txagc_8812f(dm, rf_path, 0x04, PDM_OFDM);
#endif

	if (tx_pwr_idx >= 63)
		tx_pwr_idx = 63;

	tx_pwr_idx_offset = 63 - tx_pwr_idx;

	RF_DBG(dm, DBG_RF_TX_PWR_TRACK,
	       "tx_power_index=%d tx_power_index_offset=%d rf_path=%d\n",
	       tx_pwr_idx, tx_pwr_idx_offset, rf_path);
#endif
	odm_pwrtrack_method_set_pwr8812f(dm, method, rf_path,
					 tx_pwr_idx_offset);
}

void get_delta_swing_table_8812f(void *dm_void,
				 u8 **temperature_up_a,
				 u8 **temperature_down_a,
				 u8 **temperature_up_b,
				 u8 **temperature_down_b)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct dm_rf_calibration_struct *cali_info = &dm->rf_calibrate_info;
	u8 channel = *dm->channel;

	*temperature_up_a = cali_info->delta_swing_table_idx_2ga_p;
	*temperature_down_a = cali_info->delta_swing_table_idx_2ga_n;
	*temperature_up_b = cali_info->delta_swing_table_idx_2gb_p;
	*temperature_down_b = cali_info->delta_swing_table_idx_2gb_n;

	if (channel >= 36 && channel <= 64) {
		*temperature_up_a = cali_info->delta_swing_table_idx_5ga_p[0];
		*temperature_down_a = cali_info->delta_swing_table_idx_5ga_n[0];
		*temperature_up_b = cali_info->delta_swing_table_idx_5gb_p[0];
		*temperature_down_b = cali_info->delta_swing_table_idx_5gb_n[0];
	} else if (channel >= 100 && channel <= 144) {
		*temperature_up_a = cali_info->delta_swing_table_idx_5ga_p[1];
		*temperature_down_a = cali_info->delta_swing_table_idx_5ga_n[1];
		*temperature_up_b = cali_info->delta_swing_table_idx_5gb_p[1];
		*temperature_down_b = cali_info->delta_swing_table_idx_5gb_n[1];
	} else if (channel >= 149 && channel <= 177) {
		*temperature_up_a = cali_info->delta_swing_table_idx_5ga_p[2];
		*temperature_down_a = cali_info->delta_swing_table_idx_5ga_n[2];
		*temperature_up_b = cali_info->delta_swing_table_idx_5gb_p[2];
		*temperature_down_b = cali_info->delta_swing_table_idx_5gb_n[2];
	}
}

void _phy_aac_calibrate_8812f(struct dm_struct *dm)
{
#if 1
	u32 cnt = 0;

	RF_DBG(dm, DBG_RF_LCK, "[AACK]AACK start!!!!!!!\n");
	odm_set_rf_reg(dm, RF_PATH_A, 0xbb, RFREGOFFSETMASK, 0x80010);
	odm_set_rf_reg(dm, RF_PATH_A, 0xb0, RFREGOFFSETMASK, 0x1F0FA);
	ODM_delay_ms(1);
	odm_set_rf_reg(dm, RF_PATH_A, RF_0xca, RFREGOFFSETMASK, 0x80000);
	odm_set_rf_reg(dm, RF_PATH_A, RF_0xc9, RFREGOFFSETMASK, 0x80001);
	for (cnt = 0; cnt < 100; cnt++) {
		ODM_delay_ms(1);
		if (odm_get_rf_reg(dm, RF_PATH_A, RF_0xca, 0x1000) != 0x1)
			break;
	}

	odm_set_rf_reg(dm, RF_PATH_A, RF_0xb0, RFREGOFFSETMASK, 0x1F0F8);
	odm_set_rf_reg(dm, RF_PATH_B, 0xbb, RFREGOFFSETMASK, 0x80010);

	RF_DBG(dm, DBG_RF_IQK, "[AACK]AACK end!!!!!!!\n");
#endif
}

void _phy_rt_calibrate_8812f(struct dm_struct *dm)
{
	RF_DBG(dm, DBG_RF_IQK, "[RTK]RTK start!!!!!!!\n");
	odm_set_rf_reg(dm, RF_PATH_A, 0xcc, RFREGOFFSETMASK, 0x0f000);
	odm_set_rf_reg(dm, RF_PATH_A, 0xcc, RFREGOFFSETMASK, 0x4f000);
	ODM_delay_ms(1);
	odm_set_rf_reg(dm, RF_PATH_A, 0xcc, RFREGOFFSETMASK, 0x0f000);
	RF_DBG(dm, DBG_RF_IQK, "[RTK]RTK end!!!!!!!\n");
}

void halrf_reload_bp_8812f(struct dm_struct *dm, u32 *bp_reg, u32 *bp)
{
	u32 i;

	for (i = 0; i < DACK_REG_8812F; i++)
		odm_write_4byte(dm, bp_reg[i], bp[i]);
}

void halrf_reload_bprf_8812f(struct dm_struct *dm, u32 *bp_reg, u32 bp[][2])
{
	u32 i;

	for (i = 0; i < DACK_RF_8812F; i++) {
		odm_set_rf_reg(dm, RF_PATH_A, bp_reg[i], MASK20BITS,
			       bp[i][RF_PATH_A]);
		odm_set_rf_reg(dm, RF_PATH_B, bp_reg[i], MASK20BITS,
			       bp[i][RF_PATH_B]);
	}
}

void halrf_bp_8812f(struct dm_struct *dm, u32 *bp_reg, u32 *bp)
{
	u32 i;

	for (i = 0; i < DACK_REG_8812F; i++)
		bp[i] = odm_read_4byte(dm, bp_reg[i]);
}

void halrf_bprf_8812f(struct dm_struct *dm, u32 *bp_reg, u32 bp[][2])
{
	u32 i;

	for (i = 0; i < DACK_RF_8812F; i++) {
		bp[i][RF_PATH_A] =
			odm_get_rf_reg(dm, RF_PATH_A, bp_reg[i], MASK20BITS);
		bp[i][RF_PATH_B] =
			odm_get_rf_reg(dm, RF_PATH_B, bp_reg[i], MASK20BITS);
	}
}

void halrf_swap_8812f(struct dm_struct *dm, u32 *v1, u32 *v2)
{
	u32 temp;

	temp = *v1;
	*v1 = *v2;
	*v2 = temp;
}

void halrf_bubble_8812f(struct dm_struct *dm, u32 *v1, u32 *v2)
{
	u32 temp;

	if (*v1 >= 0x200 && *v2 >= 0x200) {
		if (*v1 > *v2)
			halrf_swap_8812f(dm, v1, v2);
	} else if (*v1 < 0x200 && *v2 < 0x200) {
		if (*v1 > *v2)
			halrf_swap_8812f(dm, v1, v2);
	} else if (*v1 < 0x200 && *v2 >= 0x200) {
		halrf_swap_8812f(dm, v1, v2);
	}
}

void halrf_b_sort_8812f(struct dm_struct *dm, u32 *iv, u32 *qv)
{
	u32 temp;
	u32 i, j;

	RF_DBG(dm, DBG_RF_DACK, "[DACK]bubble!!!!!!!!!!!!");
	for (i = 0; i < SN - 1; i++) {
		for (j = 0; j < (SN - 1 - i) ; j++) {
			halrf_bubble_8812f(dm, &iv[j], &iv[j + 1]);
			halrf_bubble_8812f(dm, &qv[j], &qv[j + 1]);
		}
	}
}

void halrf_minmax_compare_8812f(struct dm_struct *dm, u32 value, u32 *min,
				u32 *max)
{
	if (value >= 0x200) {
		if (*min >= 0x200) {
			if (*min > value)
				*min = value;
		} else {
			*min = value;
		}
		if (*max >= 0x200) {
			if (*max < value)
				*max = value;
		}
	} else {
		if (*min < 0x200) {
			if (*min > value)
				*min = value;
		}

		if (*max  >= 0x200) {
			*max = value;
		} else {
			if (*max < value)
				*max = value;
		}
	}
}

boolean halrf_compare_8812f(struct dm_struct *dm, u32 value)
{
	boolean fail = false;

	if (value >= 0x200 && (0x400 - value) > 0x64)
		fail = true;
	else if (value < 0x200 && value > 0x64)
		fail = true;

	if (fail)
		RF_DBG(dm, DBG_RF_DACK, "[DACK]overflow!!!!!!!!!!!!!!!");
	return fail;
}

void halrf_mode_8812f(struct dm_struct *dm, u32 *i_value, u32 *q_value)
{
	u32 iv[SN], qv[SN], im[SN], qm[SN], temp, temp1, temp2;
	u32 p, m, t;
	u32 i_max = 0, q_max = 0, i_min = 0x0, q_min = 0x0, c = 0x0;
	u32 i_delta, q_delta;
	u8 i, j, ii = 0, qi = 0;
	boolean fail = false;

	ODM_delay_ms(10);
	RF_DBG(dm, DBG_RF_DACK, "[DACK]pathA RF0x0 = 0x%x",
	       odm_get_rf_reg(dm, 0x0, 0x0, 0xfffff));
	RF_DBG(dm, DBG_RF_DACK, "[DACK]pathB RF0x0 = 0x%x",
	       odm_get_rf_reg(dm, 0x1, 0x0, 0xfffff));

	for (i = 0; i < SN; i++) {
		im[i] = 0;
		qm[i] = 0;
	}

	i = 0;
	c = 0;
	while (i < SN && c < 10000) {
		c++;
		temp = odm_get_bb_reg(dm, 0x2dbc, 0x3fffff);
		iv[i] = (temp & 0x3ff000) >> 12;
		qv[i] = temp & 0x3ff;

		fail = false;
		if (halrf_compare_8812f(dm, iv[i]))
			fail = true;
		if (halrf_compare_8812f(dm, qv[i]))
			fail = true;
		if (!fail)
			i++;
	}

	c = 0;
	do {
		i_min = iv[0];
		i_max = iv[0];
		q_min = qv[0];
		q_max = qv[0];
		for (i = 0; i < SN; i++) {
			halrf_minmax_compare_8812f(dm, iv[i], &i_min, &i_max);
			halrf_minmax_compare_8812f(dm, qv[i], &q_min, &q_max);
		}

		c++;
		RF_DBG(dm, DBG_RF_DACK, "[DACK]i_min=0x%x, i_max=0x%x",
		       i_min, i_max);
		RF_DBG(dm, DBG_RF_DACK, "[DACK]q_min=0x%x, q_max=0x%x",
		       q_min, q_max);

		if (i_max < 0x200 && i_min < 0x200)
			i_delta = i_max - i_min;
		else if (i_max >= 0x200 && i_min >= 0x200)
			i_delta = i_max - i_min;
		else
			i_delta = i_max + (0x400 - i_min);

		if (q_max < 0x200 && q_min < 0x200)
			q_delta = q_max - q_min;
		else if (q_max >= 0x200 && q_min >= 0x200)
			q_delta = q_max - q_min;
		else
			q_delta = q_max + (0x400 - q_min);
		RF_DBG(dm, DBG_RF_DACK, "[DACK]i_delta=0x%x, q_delta=0x%x",
		       i_delta, q_delta);
		halrf_b_sort_8812f(dm, iv, qv);
		if (i_delta > 5 || q_delta > 5) {
//			halrf_b_sort_8812f(dm, iv, qv);
			temp = odm_get_bb_reg(dm, 0x2dbc, 0x3fffff);
			iv[0] = (temp & 0x3ff000) >> 12;
			qv[0] = temp & 0x3ff;
			temp = odm_get_bb_reg(dm, 0x2dbc, 0x3fffff);
			iv[SN - 1] = (temp & 0x3ff000) >> 12;
			qv[SN - 1] = temp & 0x3ff;
		} else {
			break;
		}
	} while (c < 100);
#if 0
	for (i = 0; i < SN; i++) {
		for (j = 0; j < SN; j++) {
			if (i != j) {
				if (iv[i] == iv[j])
					im[i]++;
				if (qv[i] == qv[j])
					qm[i]++;
			}
		}
	}

	for (i = 0; i < SN; i++)
		RF_DBG(dm, DBG_RF_DACK, "[DACK]iv[%d] = 0x%x\n", i, iv[i]);

	for (i = 0; i < SN; i++)
		RF_DBG(dm, DBG_RF_DACK, "[DACK]qv[%d] = 0x%x\n", i, qv[i]);

	for (i = 1; i < SN; i++) {
		if (im[ii] < im[i])
			ii = i;
		if (qm[qi] < qm[i])
			qi = i;
	}

	*i_value = iv[ii];
	*q_value = qv[qi];
#endif
#if 1
#if 0
	for (i = 0; i < SN; i++)
		RF_DBG(dm, DBG_RF_DACK, "[DACK]iv[%d] = 0x%x\n", i, iv[i]);

	for (i = 0; i < SN; i++)
		RF_DBG(dm, DBG_RF_DACK, "[DACK]qv[%d] = 0x%x\n", i, qv[i]);
#endif
	/*i*/
	m = 0;
	p = 0;
	for (i = 10; i < SN - 10; i++) {
		if (iv[i] > 0x200)
			m = (0x400 - iv[i]) + m;
		else
			p = iv[i] + p;
	}

	if (p > m) {
		t = p - m;
		t = t / (SN - 20);
	} else {
		t = m - p;
		t = t / (SN - 20);
		if (t != 0x0)
			t = 0x400 - t;
	}
	*i_value = t;
	/*q*/
	m = 0;
	p = 0;
	for (i = 10; i < SN - 10; i++) {
		if (qv[i] > 0x200)
			m = (0x400 - qv[i]) + m;
		else
			p = qv[i] + p;
	}
	if (p > m) {
		t = p - m;
		t = t / (SN - 20);
	} else {
		t = m - p;
		t = t / (SN - 20);
		if (t != 0x0)
			t = 0x400 - t;
	}
	*q_value = t;
#endif
}

void halrf_dac_cal_8812f(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	static u32 count = 1;
#if 1
	u32 ic = 0, qc = 0, temp = 0, i;
	u32 bp[DACK_REG_8812F];
	u32 bp_reg[DACK_REG_8812F] = {0x180c, 0x410c, 0x1c3c, 0x1c24, 0x1d70,
				      0x9b4, 0x1a00, 0x1a14, 0x1d58, 0x1c38,
				      0x1e24, 0x1e28, 0x1860, 0x4160};
	u32 bp_rf[DACK_RF_8812F][2];
	u32 bp_rfreg[DACK_RF_8812F] = {0x8f};
	u32 i_a = 0x0, q_a = 0x0, i_b = 0x0, q_b = 0x0;
	u32 ic_a = 0x0, qc_a = 0x0, ic_b = 0x0, qc_b = 0x0;
	u32 adc_ic_a = 0x0, adc_qc_a = 0x0, adc_ic_b = 0x0, adc_qc_b = 0x0;

	count++;
	RF_DBG(dm, DBG_RF_DACK, "[DACK]count = %d", count);
	RF_DBG(dm, DBG_RF_DACK, "[DACK]DACK start!!!!!!!");
	halrf_bp_8812f(dm, bp_reg, bp);
	halrf_bprf_8812f(dm, bp_rfreg, bp_rf);
	/*BB setting*/
	odm_set_bb_reg(dm, 0x1d58, 0xff8, 0x1ff);
	odm_set_bb_reg(dm, 0x1a00, 0x3, 0x2);
	odm_set_bb_reg(dm, 0x1a14, 0x300, 0x3);
	odm_write_4byte(dm, 0x1d70, 0x7e7e7e7e);
	odm_set_bb_reg(dm, 0x180c, 0x3, 0x0);
	odm_set_bb_reg(dm, 0x410c, 0x3, 0x0);
	odm_write_4byte(dm, 0x1b00, 0x00000008);
	odm_write_4byte(dm, 0x1bcc, 0x0000003f);
	odm_write_4byte(dm, 0x1b00, 0x0000000a);
	odm_write_4byte(dm, 0x1bcc, 0x0000003f);
	odm_set_bb_reg(dm, 0x1e24, BIT(31), 0x0);
	odm_set_bb_reg(dm, 0x1e28, 0xf, 0x3);
/*path-A*/
	RF_DBG(dm, DBG_RF_DACK, "[DACK]pathA DACK!!!!!!!!!!!!!!!!!!!!!!!!!!!");
/*1.ADCK step1*/
	RF_DBG(dm, DBG_RF_DACK, "[DACK]step1 ADCK!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	odm_set_bb_reg(dm, 0x1830, BIT(30), 0x0);
	odm_write_4byte(dm, 0x1860, 0xf0040ff0);
	odm_write_4byte(dm, 0x180c, 0xdff00220);
	odm_write_4byte(dm, 0x1810, 0x02dd08c4);
	odm_write_4byte(dm, 0x180c, 0x10000260);
	odm_set_rf_reg(dm, RF_PATH_A, 0x0, RFREGOFFSETMASK, 0x10000);
	odm_set_rf_reg(dm, RF_PATH_B, 0x0, RFREGOFFSETMASK, 0x10000);

	i = 0;
	while (i < 10) {
		i++;
		RF_DBG(dm, DBG_RF_DACK, "[DACK]ADCK count=%d", i);
		odm_write_4byte(dm, 0x1c3c, 0x00088003);
		odm_write_4byte(dm, 0x1c24, 0x00010002);
		halrf_mode_8812f(dm, &ic, &qc);
		RF_DBG(dm, DBG_RF_DACK, "[DACK]before ADCK i=0x%x, q=0x%x",
		       ic, qc);
		/*compensation value*/
		if (ic != 0x0) {
			ic = 0x400 - ic;
			adc_ic_a = ic;
		}
		if (qc != 0x0) {
			qc = 0x400 - qc;
			adc_qc_a = qc;
		}
		temp = (ic & 0x3ff) | ((qc & 0x3ff) << 10);
		odm_write_4byte(dm, 0x1868, temp);
		RF_DBG(dm, DBG_RF_DACK, "[DACK]ADCK 0x1868 =0x%x\n", temp);
#if 1
		/*check ADC DC offset*/
		odm_write_4byte(dm, 0x1c3c, 0x00088103);
		halrf_mode_8812f(dm, &ic, &qc);
		RF_DBG(dm, DBG_RF_DACK, "[DACK]after ADCK i=0x%x, q=0x%x",
		       ic, qc);
#endif
		if (ic >= 0x200)
			ic = 0x400 - ic;
		if (qc >= 0x200)
			qc = 0x400 - qc;
		if (ic < 5 && qc < 5)
			break;
	}
	/*2.ADCK step2*/
	odm_write_4byte(dm, 0x1c3c, 0x00000003);
	odm_write_4byte(dm, 0x180c, 0x10000260);
	odm_write_4byte(dm, 0x1810, 0x02d508c4);
	/*3.release pull low switch on IQ path*/
	odm_set_rf_reg(dm, RF_PATH_A, 0x8f, BIT(13), 0x1);

	RF_DBG(dm, DBG_RF_DACK, "[DACK]step2 DACK!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	i = 0;
	while (i < 10) {
		/*DACK step1*/
		i++;
		RF_DBG(dm, DBG_RF_DACK, "[DACK]DACK count=%d\n", i);
		odm_write_4byte(dm, 0x180c, 0xdff00220);
		odm_write_4byte(dm, 0x1860, 0xf0040ff0);
		odm_write_4byte(dm, 0x1c38, 0xffffffff);
		odm_write_4byte(dm, 0x1810, 0x02d508c5);
		odm_write_4byte(dm, 0x9b4, 0xdb66db00);
		odm_write_4byte(dm, 0x18b0, 0x0a11fb88);
		odm_write_4byte(dm, 0x18bc, 0x0008ff81);
		odm_write_4byte(dm, 0x18c0, 0x0003d208);
		odm_write_4byte(dm, 0x18cc, 0x0a11fb88);
		odm_write_4byte(dm, 0x18d8, 0x0008ff81);
		odm_write_4byte(dm, 0x18dc, 0x0003d208);

		odm_write_4byte(dm, 0x18b8, 0x60000000);
		ODM_delay_us(2);
		odm_write_4byte(dm, 0x18bc, 0x000aff8d);
		ODM_delay_us(2);
		odm_write_4byte(dm, 0x18b0, 0x0a11fb89);
		odm_write_4byte(dm, 0x18cc, 0x0a11fb89);
		ODM_delay_us(1);
		odm_write_4byte(dm, 0x18b8, 0x62000000);
		ODM_delay_us(20);
		odm_write_4byte(dm, 0x18d4, 0x62000000);
		ODM_delay_us(20);
		odm_write_4byte(dm, 0x18b8, 0x02000000);
		ODM_delay_us(20);
		odm_write_4byte(dm, 0x18bc, 0x0008ff87);
		odm_write_4byte(dm, 0x9b4, 0xdb6db600);

		odm_write_4byte(dm, 0x1810, 0x02d508c5);
		odm_write_4byte(dm, 0x18bc, 0x0008ff87);
		odm_write_4byte(dm, 0x1860, 0xf0000000);
		/*4.DACK step2*/
		odm_set_bb_reg(dm, 0x18bc, 0xf0000000, 0x0);
		odm_set_bb_reg(dm, 0x18c0, 0xf, 0x8);
		odm_set_bb_reg(dm, 0x18d8, 0xf0000000, 0x0);
		odm_set_bb_reg(dm, 0x18dc, 0xf, 0x8);

		odm_write_4byte(dm, 0x1b00, 0x00000008);
		odm_write_4byte(dm, 0x1bcc, 0x0000003f);
		odm_write_4byte(dm, 0x180c, 0xdff00220);
		odm_write_4byte(dm, 0x1810, 0x02d508c5);
		odm_write_4byte(dm, 0x1c3c, 0x00088103);
		halrf_mode_8812f(dm, &ic, &qc);
		RF_DBG(dm, DBG_RF_DACK, "[DACK]before DACK i =0x%x, q = 0x%x",
		       ic, qc);
		/*compensation value*/
		if (ic != 0x0)
			ic = 0x400 - ic;
		if (qc != 0x0)
			qc = 0x400 - qc;
		if (ic < 0x300) {
			ic = ic * 2 * 6 / 5;
			ic = ic + 0x80;
		} else {
			ic = (0x400 - ic) * 2 * 6 / 5;
			ic = 0x7f - ic;
		}
		if (qc < 0x300) {
			qc = qc * 2 * 6 / 5;
			qc = qc + 0x80;
		} else {
			qc = (0x400 - qc) * 2 * 6 / 5;
			qc = 0x7f - qc;
		}
		RF_DBG(dm, DBG_RF_DACK, "[DACK]DACK ic =0x%x, qc = 0x%x\n",
		       ic, qc);
		ic_a = ic;
		qc_a = qc;
	/*5.DACK step3*/
		odm_write_4byte(dm, 0x180c, 0xdff00220);
		odm_write_4byte(dm, 0x1810, 0x02d508c5);
		odm_write_4byte(dm, 0x9b4, 0xdb66db00);
		odm_write_4byte(dm, 0x18b0, 0x0a11fb88);
		odm_write_4byte(dm, 0x18bc, 0xc008ff81);
		odm_write_4byte(dm, 0x18c0, 0x0003d208);
		odm_set_bb_reg(dm, 0x18bc, 0xf0000000, ic & 0xf);
		odm_set_bb_reg(dm, 0x18c0, 0xf, (ic & 0xf0) >> 4);
		odm_write_4byte(dm, 0x18cc, 0x0a11fb88);
		odm_write_4byte(dm, 0x18d8, 0xe008ff81);
		odm_write_4byte(dm, 0x18dc, 0x0003d208);
		odm_set_bb_reg(dm, 0x18d8, 0xf0000000, qc & 0xf);
		odm_set_bb_reg(dm, 0x18dc, 0xf, (qc & 0xf0) >> 4);
		odm_write_4byte(dm, 0x18b8, 0x60000000);
		ODM_delay_us(2);
		odm_set_bb_reg(dm, 0x18bc, 0xe, 0x6);
		ODM_delay_us(2);
		odm_write_4byte(dm, 0x18b0, 0x0a11fb89);
		odm_write_4byte(dm, 0x18cc, 0x0a11fb89);
		ODM_delay_us(1);
		odm_write_4byte(dm, 0x18b8, 0x62000000);
		ODM_delay_us(20);
		odm_write_4byte(dm, 0x18d4, 0x62000000);
		ODM_delay_us(20);
		odm_write_4byte(dm, 0x18b8, 0x02000000);
		ODM_delay_us(20);
		odm_set_bb_reg(dm, 0x18bc, 0xe, 0x3);
		odm_write_4byte(dm, 0x9b4, 0xdb6db600);
		RF_DBG(dm, DBG_RF_DACK, "[DACK]0x18bc =0x%x",
		       odm_read_4byte(dm, 0x18bc));
		RF_DBG(dm, DBG_RF_DACK, "[DACK]0x18c0 =0x%x",
		       odm_read_4byte(dm, 0x18c0));
		RF_DBG(dm, DBG_RF_DACK, "[DACK]0x18d8 =0x%x",
		       odm_read_4byte(dm, 0x18d8));
		RF_DBG(dm, DBG_RF_DACK, "[DACK]0x18dc =0x%x",
		       odm_read_4byte(dm, 0x18dc));
#if 1
		/*check DAC DC offset*/
		temp = ((adc_ic_a + 0x10) & 0x3ff) |
		       (((adc_qc_a + 0x10) & 0x3ff) << 10);
		odm_write_4byte(dm, 0x1868, temp);
		RF_DBG(dm, DBG_RF_DACK, "[DACK]shift 0x1868 =0x%x",
		       odm_read_4byte(dm, 0x1868));
		odm_write_4byte(dm, 0x1810, 0x02d508c5);
		odm_write_4byte(dm, 0x1860, 0xf0000000);
		halrf_mode_8812f(dm, &ic, &qc);
		if (ic >= 0x10)
			ic = ic - 0x10;
		else
			ic = 0x400 - (0x10 - ic);

		if (qc >= 0x10)
			qc = qc - 0x10;
		else
			qc = 0x400 - (0x10 - qc);
		RF_DBG(dm, DBG_RF_DACK, "[DACK]after DACK i=0x%x, q=0x%x",
		       ic, qc);
		i_a = ic;
		q_a = qc;
		if (ic >= 0x200)
			ic = 0x400 - ic;
		if (qc >= 0x200)
			qc = 0x400 - qc;
		if (ic < 5 && qc < 5)
			break;
#endif
	}
	odm_write_4byte(dm, 0x1810, 0x02d508c4);
	odm_set_bb_reg(dm, 0x18bc, 0x1, 0x0);
	odm_set_bb_reg(dm, 0x1830, BIT(30), 0x1);
#if 1
/*path-B*/
	RF_DBG(dm, DBG_RF_DACK, "[DACK]pathB DACK!!!!!!!!!!!!!!!!!!!!!!!!!!!");
/*1.ADCK step1*/
	RF_DBG(dm, DBG_RF_DACK, "[DACK]step1 ADCK!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	odm_set_bb_reg(dm, 0x4130, BIT(30), 0x0);
	odm_write_4byte(dm, 0x4130, 0x30db8041);
	odm_write_4byte(dm, 0x4160, 0xf0040ff0);
	odm_write_4byte(dm, 0x410c, 0xdff00220);
	odm_write_4byte(dm, 0x4110, 0x02dd08c4);
	odm_write_4byte(dm, 0x410c, 0x10000260);
	odm_set_rf_reg(dm, RF_PATH_A, 0x0, RFREGOFFSETMASK, 0x10000);
	odm_set_rf_reg(dm, RF_PATH_B, 0x0, RFREGOFFSETMASK, 0x10000);
	i = 0;
	while (i < 10) {
		i++;
		RF_DBG(dm, DBG_RF_DACK, "[DACK]ADCK count=%d\n", i);
		odm_write_4byte(dm, 0x1c3c, 0x000a8003);
		odm_write_4byte(dm, 0x1c24, 0x00010002);
		halrf_mode_8812f(dm, &ic, &qc);
		RF_DBG(dm, DBG_RF_DACK, "[DACK]before ADCK i=0x%x, q=0x%x",
		       ic, qc);
		/*compensation value*/
		if (ic != 0x0) {
			ic = 0x400 - ic;
			adc_ic_b = ic;
		}
		if (qc != 0x0) {
			qc = 0x400 - qc;
			adc_qc_b = qc;
		}
		temp = (ic & 0x3ff) | ((qc & 0x3ff) << 10);
		odm_write_4byte(dm, 0x4168, temp);
		RF_DBG(dm, DBG_RF_DACK, "[DACK]ADCK 0x1868 =0x%x\n", temp);
#if 1
		/*check ADC DC offset*/
		odm_write_4byte(dm, 0x1c3c, 0x000a8103);
		halrf_mode_8812f(dm, &ic, &qc);
		RF_DBG(dm, DBG_RF_DACK, "[DACK]after ADCK i=0x%x, q=0x%x",
		       ic, qc);
#endif
		if (ic >= 0x200)
			ic = 0x400 - ic;
		if (qc >= 0x200)
			qc = 0x400 - qc;
		if (ic < 5 && qc < 5)
			break;
	}
/*2.ADCK step2*/
	odm_write_4byte(dm, 0x1c3c, 0x00000003);
	odm_write_4byte(dm, 0x410c, 0x10000260);
	odm_write_4byte(dm, 0x4110, 0x02d508c4);

	/*3.release pull low switch on IQ path*/
	odm_set_rf_reg(dm, RF_PATH_B, 0x8f, BIT(13), 0x1);
	RF_DBG(dm, DBG_RF_DACK, "[DACK]step2 DACK!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	i = 0;
	while (i < 10) {
/*DACK step1*/
		i++;
		RF_DBG(dm, DBG_RF_DACK, "[DACK]DACK count=%d\n", i);
	odm_write_4byte(dm, 0x410c, 0xdff00220);
	odm_write_4byte(dm, 0x4110, 0x02d508c5);
		odm_write_4byte(dm, 0x9b4, 0xdb66db00);
	odm_write_4byte(dm, 0x41b0, 0x0a11fb88);
	odm_write_4byte(dm, 0x41bc, 0x0008ff81);
	odm_write_4byte(dm, 0x41c0, 0x0003d208);
	odm_write_4byte(dm, 0x41cc, 0x0a11fb88);
	odm_write_4byte(dm, 0x41d8, 0x0008ff81);
	odm_write_4byte(dm, 0x41dc, 0x0003d208);

	odm_write_4byte(dm, 0x41b8, 0x60000000);
		ODM_delay_ms(2);
	odm_write_4byte(dm, 0x41bc, 0x000aff8d);
		ODM_delay_ms(2);
	odm_write_4byte(dm, 0x41b0, 0x0a11fb89);
	odm_write_4byte(dm, 0x41cc, 0x0a11fb89);
		ODM_delay_ms(1);
	odm_write_4byte(dm, 0x41b8, 0x62000000);
		ODM_delay_ms(20);
	odm_write_4byte(dm, 0x41d4, 0x62000000);
		ODM_delay_ms(20);
	odm_write_4byte(dm, 0x41b8, 0x02000000);
		ODM_delay_ms(20);
	odm_write_4byte(dm, 0x41bc, 0x0008ff87);
	odm_write_4byte(dm, 0x9b4, 0xdb6db600);

		odm_write_4byte(dm, 0x4110, 0x02d508c5);
		odm_write_4byte(dm, 0x41bc, 0x0008ff87);
		odm_write_4byte(dm, 0x4160, 0xf0000000);
	/*4.DACK step2*/
		odm_set_bb_reg(dm, 0x41bc, 0xf0000000, 0x0);
		odm_set_bb_reg(dm, 0x41c0, 0xf, 0x8);
		odm_set_bb_reg(dm, 0x41d8, 0xf0000000, 0x0);
		odm_set_bb_reg(dm, 0x41dc, 0xf, 0x8);
		odm_write_4byte(dm, 0x1b00, 0x0000000a);
		odm_write_4byte(dm, 0x1bcc, 0x0000003f);
		odm_write_4byte(dm, 0x410c, 0xdff00220);
		odm_write_4byte(dm, 0x4110, 0x02d508c5);
		odm_write_4byte(dm, 0x1c3c, 0x000a8103);
		halrf_mode_8812f(dm, &ic, &qc);
		RF_DBG(dm, DBG_RF_DACK, "[DACK]before DACK i=0x%x, q=0x%x",
		       ic, qc);
		/*compensation value*/
		if (ic != 0x0)
			ic = 0x400 - ic;
		if (qc != 0x0)
			qc = 0x400 - qc;
		if (ic < 0x300) {
			ic = ic  * 2 * 6 / 5;
			ic = ic + 0x80;
		} else {
			ic = (0x400 - ic) * 2 * 6 / 5;
			ic = 0x7f - ic;
		}
		if (qc < 0x300) {
			qc = qc * 2 * 6 / 5;
			qc = qc + 0x80;
		} else {
			qc = (0x400 - qc) * 2 * 6 / 5;
			qc = 0x7f - qc;
		}
		RF_DBG(dm, DBG_RF_DACK, "[DACK]DACK ic=0x%x, qc=0x%x",
		       ic, qc);
		ic_b = ic;
		qc_b = qc;
	/*5.DACK step3*/
		odm_write_4byte(dm, 0x410c, 0xdff00220);
		odm_write_4byte(dm, 0x4110, 0x02d508c5);
		odm_write_4byte(dm, 0x9b4, 0xdb66db00);
		odm_write_4byte(dm, 0x41b0, 0x0a11fb88);
		odm_write_4byte(dm, 0x41bc, 0xc008ff81);
		odm_write_4byte(dm, 0x41c0, 0x0003d208);
		odm_set_bb_reg(dm, 0x41bc, 0xf0000000, ic & 0xf);
		odm_set_bb_reg(dm, 0x41c0, 0xf, (ic & 0xf0) >> 4);
		odm_write_4byte(dm, 0x41cc, 0x0a11fb88);
		odm_write_4byte(dm, 0x41d8, 0xe008ff81);
		odm_write_4byte(dm, 0x41dc, 0x0003d208);
		odm_set_bb_reg(dm, 0x41d8, 0xf0000000, qc & 0xf);
		odm_set_bb_reg(dm, 0x41dc, 0xf, (qc & 0xf0) >> 4);
		odm_write_4byte(dm, 0x41b8, 0x60000000);
		ODM_delay_ms(2);
		odm_set_bb_reg(dm, 0x41bc, 0xe, 0x6);
		ODM_delay_ms(2);
		odm_write_4byte(dm, 0x41b0, 0x0a11fb89);
		odm_write_4byte(dm, 0x41cc, 0x0a11fb89);
		ODM_delay_ms(1);
		odm_write_4byte(dm, 0x41b8, 0x62000000);
		ODM_delay_ms(20);
		odm_write_4byte(dm, 0x41d4, 0x62000000);
		ODM_delay_ms(20);
		odm_write_4byte(dm, 0x41b8, 0x02000000);
		ODM_delay_ms(20);
		odm_set_bb_reg(dm, 0x41bc, 0xe, 0x3);
		odm_write_4byte(dm, 0x9b4, 0xdb6db600);
		RF_DBG(dm, DBG_RF_DACK, "[DACK]0x41bc =0x%x",
		       odm_read_4byte(dm, 0x41bc));
		RF_DBG(dm, DBG_RF_DACK, "[DACK]0x41c0 =0x%x",
		       odm_read_4byte(dm, 0x41c0));
		RF_DBG(dm, DBG_RF_DACK, "[DACK]0x41d8 =0x%x",
		       odm_read_4byte(dm, 0x41d8));
		RF_DBG(dm, DBG_RF_DACK, "[DACK]0x41dc =0x%x",
		       odm_read_4byte(dm, 0x41dc));
#if 1
		/*check DAC DC offset*/
		temp = ((adc_ic_b + 0x10) & 0x3ff) |
		       (((adc_qc_b + 0x10) & 0x3ff) << 10);
		odm_write_4byte(dm, 0x4168, temp);
		RF_DBG(dm, DBG_RF_DACK, "[DACK]shift 0x1868 =0x%x\n",
		       odm_read_4byte(dm, 0x4168));
		odm_write_4byte(dm, 0x4110, 0x02d508c5);
		odm_write_4byte(dm, 0x4160, 0xf0000000);
		halrf_mode_8812f(dm, &ic, &qc);
		if (ic >= 0x10)
			ic = ic - 0x10;
		else
			ic = 0x400 - (0x10 - ic);

		if (qc >= 0x10)
			qc = qc - 0x10;
		else
			qc = 0x400 - (0x10 - qc);
		RF_DBG(dm, DBG_RF_DACK, "[DACK]after DACK i=0x%x, q=0x%x",
		       ic, qc);
		i_b = ic;
		q_b = qc;
#endif
		if (ic >= 0x200)
			ic = 0x400 - ic;
		if (qc >= 0x200)
			qc = 0x400 - qc;
		if (ic < 5 && qc < 5)
			break;
	}
#endif
	odm_write_4byte(dm, 0x4110, 0x02d508c4);
	odm_set_bb_reg(dm, 0x41bc, 0x1, 0x0);
	odm_set_bb_reg(dm, 0x4130, BIT(30), 0x1);
	odm_write_4byte(dm, 0x1b00, 0x00000008);
	odm_write_4byte(dm, 0x1bcc, 0x00000000);
	odm_write_4byte(dm, 0x1b00, 0x0000000a);
	odm_write_4byte(dm, 0x1bcc, 0x00000000);
	i_b = ic;
	q_b = qc;
	RF_DBG(dm, DBG_RF_DACK, "[DACK]PATH A:ic=0x%x, qc=0x%x", ic_a, qc_a);
	RF_DBG(dm, DBG_RF_DACK, "[DACK]PATH B:ic=0x%x, qc=0x%x", ic_b, qc_b);
	RF_DBG(dm, DBG_RF_DACK, "[DACK]PATH A:i=0x%x, q=0x%x", i_a, q_a);
	RF_DBG(dm, DBG_RF_DACK, "[DACK]PATH B:i=0x%x, q=0x%x", i_b, q_b);
	halrf_reload_bp_8812f(dm, bp_reg, bp);
	halrf_reload_bprf_8812f(dm, bp_rfreg, bp_rf);
	RF_DBG(dm, DBG_RF_DACK, "[DACK]DACK end!!!!!!!\n");
#endif
}

void phy_x2_check_8812f(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 X2K_BUSY;

	RF_DBG(dm, DBG_RF_IQK, "[X2K]X2K check start!!!!!!!\n");
	/*X2K*/
	//Path A
	ODM_delay_ms(1);
	X2K_BUSY = (u8) odm_get_rf_reg(dm, RF_PATH_A, 0xb8, BIT(15));
	if (X2K_BUSY == 1) {
		odm_set_rf_reg(dm, RF_PATH_A, 0xb8, RFREGOFFSETMASK, 0xC4440);	
		odm_set_rf_reg(dm, RF_PATH_A, 0xba, RFREGOFFSETMASK, 0x6840D);
		odm_set_rf_reg(dm, RF_PATH_A, 0xb8, RFREGOFFSETMASK, 0x80440);		
		ODM_delay_ms(1);
	}
	//Path B
	// SYN is in the path A
	RF_DBG(dm, DBG_RF_IQK, "[X2K]X2K check end!!!!!!!\n");
}

void _phy_x2_calibrate_8812f(struct dm_struct *dm)
{
	RF_DBG(dm, DBG_RF_IQK, "[X2K]X2K start!!!!!!!\n");
	/*X2K*/
	//Path A
	odm_set_rf_reg(dm, RF_PATH_A, 0x18, RFREGOFFSETMASK, 0x13108);
	ODM_delay_ms(1);
	odm_set_rf_reg(dm, RF_PATH_A, 0xb8, RFREGOFFSETMASK, 0xC0440);	
	odm_set_rf_reg(dm, RF_PATH_A, 0xba, RFREGOFFSETMASK, 0xE840D);
	ODM_delay_ms(1);
	odm_set_rf_reg(dm, RF_PATH_A, 0x18, RFREGOFFSETMASK, 0x13124);
	//Path B
	// SYN is in the path A
	RF_DBG(dm, DBG_RF_IQK, "[X2K]X2K end!!!!!!!\n");
}

/*LCK VERSION:0x1*/
void phy_lc_calibrate_8812f(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
#if 1
	_phy_aac_calibrate_8812f(dm);
	_phy_rt_calibrate_8812f(dm);
#endif
}

void configure_txpower_track_8812f(struct txpwrtrack_cfg *config)
{
	config->swing_table_size_cck = TXSCALE_TABLE_SIZE;
	config->swing_table_size_ofdm = TXSCALE_TABLE_SIZE;
	config->threshold_iqk = IQK_THRESHOLD;
	config->threshold_dpk = DPK_THRESHOLD;
	config->average_thermal_num = AVG_THERMAL_NUM_8812F;
	config->rf_path_count = MAX_PATH_NUM_8812F;
	config->thermal_reg_addr = RF_T_METER_8812F;

	config->odm_tx_pwr_track_set_pwr = odm_tx_pwr_track_set_pwr8812f;
	config->do_iqk = do_iqk_8812f;
	config->phy_lc_calibrate = halrf_lck_trigger;
	config->do_tssi_dck = halrf_tssi_dck;
	config->get_delta_swing_table = get_delta_swing_table_8812f;
}

#if ((DM_ODM_SUPPORT_TYPE & ODM_AP) || (DM_ODM_SUPPORT_TYPE == ODM_CE))
void phy_set_rf_path_switch_8812f(struct dm_struct *dm, boolean is_main)
#else
void phy_set_rf_path_switch_8812f(void *adapter, boolean is_main)
#endif
{
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(((PADAPTER)adapter));
	struct dm_struct *dm = &hal_data->DM_OutSrc;
#endif
#endif
	/*BY SY Request */
	odm_set_bb_reg(dm, R_0x4c, (BIT(24) | BIT(23)), 0x2);

	/* odm_set_bb_reg(dm, R_0x974, 0xff, 0xff); */

#if 0
	/*odm_set_bb_reg(dm, R_0x1991, 0x3, 0x0);*/
#endif
	/* odm_set_bb_reg(dm, R_0x1990, (BIT(9) | BIT(8)), 0x0); */

#if 0
	/*odm_set_bb_reg(dm, R_0xcbe, 0x8, 0x0);*/
#endif
	/* odm_set_bb_reg(dm, R_0xcbc, BIT(19), 0x0); */

	/* odm_set_bb_reg(dm, R_0xcb4, 0xff, 0x77); */

	odm_set_bb_reg(dm, R_0x70, MASKBYTE3, 0x0e);
	odm_set_bb_reg(dm, R_0x1704, MASKDWORD, 0x0000ff00);
	odm_set_bb_reg(dm, R_0x1700, MASKDWORD, 0xc00f0038);

	if (is_main) {
#if 0
		/*odm_set_bb_reg(dm, R_0xcbd, 0x3, 0x2); WiFi*/
#endif
		/*WiFi*/
		/* odm_set_bb_reg(dm, R_0xcbc, (BIT(9) | BIT(8)), 0x2); */
	} else {
#if 0
		/*odm_set_bb_reg(dm, R_0xcbd, 0x3, 0x1); BT*/
#endif
		/*BT*/
		/* odm_set_bb_reg(dm, R_0xcbc, (BIT(9) | BIT(8)), 0x1); */
	}
}

#if ((DM_ODM_SUPPORT_TYPE & ODM_AP) || (DM_ODM_SUPPORT_TYPE == ODM_CE))
boolean _phy_query_rf_path_switch_8812f(struct dm_struct *dm)
#else
boolean _phy_query_rf_path_switch_8812f(void *adapter)
#endif
{
#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
#if (DM_ODM_SUPPORT_TYPE == ODM_WIN)
	HAL_DATA_TYPE *hal_data = GET_HAL_DATA(((PADAPTER)adapter));
	struct dm_struct *dm = &hal_data->DM_OutSrc;
#endif
#endif
	/* if (odm_get_bb_reg(dm, R_0xcbc, (BIT(9) | BIT(8))) == 0x2) */
		/*WiFi*/
		return true;
	/* else */
		/* return false; */
}

#if ((DM_ODM_SUPPORT_TYPE & ODM_AP) || (DM_ODM_SUPPORT_TYPE == ODM_CE))
boolean phy_query_rf_path_switch_8812f(struct dm_struct *dm)
#else
boolean phy_query_rf_path_switch_8812f(void *adapter)
#endif
{
#if DISABLE_BB_RF
	return true;
#endif
#if ((DM_ODM_SUPPORT_TYPE & ODM_AP) || (DM_ODM_SUPPORT_TYPE == ODM_CE))
	return _phy_query_rf_path_switch_8812f(dm);
#else
	return _phy_query_rf_path_switch_8812f(adapter);
#endif
}

void halrf_rxbb_dc_cal_8812f(void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;

	u8 path;

	for (path = 0; path < 2; path++) {
		odm_set_rf_reg(dm, (enum rf_path)path, 0x92, RFREG_MASK, 0x84800);
		odm_set_rf_reg(dm, (enum rf_path)path, 0x92, RFREG_MASK, 0x84801);
		ODM_delay_us(400);
		odm_set_rf_reg(dm, (enum rf_path)path, 0x92, RFREG_MASK, 0x84800);
	}
}

#endif /*(RTL8812F_SUPPORT == 0)*/
