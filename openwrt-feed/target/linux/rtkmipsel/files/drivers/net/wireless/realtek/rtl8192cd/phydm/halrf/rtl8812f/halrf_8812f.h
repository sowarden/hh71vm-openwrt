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

#ifndef __HALRF_8812F_H__
#define __HALRF_8812F_H__

#define AVG_THERMAL_NUM_8812F 4
#define RF_T_METER_8812F 0x42
#define DACK_REG_8812F 14
#define DACK_RF_8812F 1
void halrf_rf_lna_setting_8812f(
	struct dm_struct *p_dm_void,
	enum halrf_lna_set type);

void configure_txpower_track_8812f(
	struct txpwrtrack_cfg *config);

void odm_tx_pwr_track_set_pwr8812f(
	void *dm_void,
	enum pwrtrack_method method,
	u8 rf_path,
	u8 channel_mapped_index);

void get_delta_swing_table_8812f(
	void *dm_void,
	u8 **temperature_up_a,
	u8 **temperature_down_a,
	u8 **temperature_up_b,
	u8 **temperature_down_b
	);

void get_delta_swing_table_8812f_ex(
	void *p_dm_void,
	u8 **temperature_up_c,
	u8 **temperature_down_c,
	u8 **temperature_up_d,
	u8 **temperature_down_d,
	u8 **temperature_up_cck_c,
	u8 **temperature_down_cck_c,
	u8 **temperature_up_cck_d,
	u8 **temperature_down_cck_d
	);

void halrf_dac_cal_all_8812f(void *dm_void);

void halrf_dac_cal_8812f(void *dm_void);

void phy_lc_calibrate_8812f(
	void *dm_void);

void phy_x2_check_8812f(void *dm_void);


void phy_set_rf_path_switch_8812f(
#if ((DM_ODM_SUPPORT_TYPE & ODM_AP) || (DM_ODM_SUPPORT_TYPE == ODM_CE))
	struct dm_struct *dm,
#else
	void *adapter,
#endif
	boolean is_main);

void halrf_rxbb_dc_cal_8812f(void *dm_void);

#endif /*__HALRF_8812F_H__*/
