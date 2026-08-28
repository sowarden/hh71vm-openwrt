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

#ifndef __HALRF_8197F_H__
#define __HALRF_8197F_H__

/*--------------------------Define Parameters-------------------------------*/
#if (DM_ODM_SUPPORT_TYPE & ODM_CE)
#define IQK_DELAY_TIME_97F 15 /*ms*/
#else
#define IQK_DELAY_TIME_97F 10
#endif

#define AVG_THERMAL_NUM_8197F 4
#define RF_T_METER_8197F 0x42

void configure_txpower_track_8197f(
	struct txpwrtrack_cfg *config);

void odm_tx_pwr_track_set_pwr8197f(
	void *dm_void,
	enum pwrtrack_method method,
	u8 rf_path,
	u8 channel_mapped_index);

void get_delta_swing_table_8197f(
	void *dm_void,
	u8 **temperature_up_a,
	u8 **temperature_down_a,
	u8 **temperature_up_b,
	u8 **temperature_down_b,
	u8 **temperature_up_cck_a,
	u8 **temperature_down_cck_a,
	u8 **temperature_up_cck_b,
	u8 **temperature_down_cck_b);

void phy_set_rf_path_switch_8197f(
#if (DM_ODM_SUPPORT_TYPE & ODM_AP)
	struct dm_struct *dm,
#else
	void *adapter,
#endif
	boolean is_main);

#endif /*#ifndef __HALRF_8197F_H__*/
