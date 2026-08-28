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

#ifndef __PHYRF_IQK_8197F_H__
#define __PHYRF_IQK_8197F_H__

/*--------------------------Define Parameters-------------------------------*/
#define MAC_REG_NUM_8197F 2
#define BB_REG_NUM_8197F 12
#define RF_REG_NUM_8197F 3

#define LOK_delay_8197F 100
#define IQK_delay_8197F 100
#define WBIQK_delay_8197F 100

/*---------------------------End Define Parameters-------------------------------*/

#if !(DM_ODM_SUPPORT_TYPE & ODM_AP)
void do_iqk_8197f(
	void *dm_void,
	u8 delta_thermal_index,
	u8 thermal_value,
	u8 threshold);
#else
void do_iqk_8197f(
	void *dm_void,
	u8 delta_thermal_index,
	u8 thermal_value,
	u8 threshold);
#endif

void phy_iq_calibrate_8197f(
	void *dm_void,
	boolean is_recovery);

void phy_lc_calibrate_8197f(
	void *dm_void);

#endif /*#ifndef __PHYRF_IQK_8197F_H__*/
