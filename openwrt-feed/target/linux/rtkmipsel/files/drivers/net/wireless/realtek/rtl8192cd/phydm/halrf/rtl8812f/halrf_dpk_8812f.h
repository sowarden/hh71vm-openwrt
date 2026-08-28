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

#ifndef __HALRF_DPK_8812F_H__
#define __HALRF_DPK_8812F_H__

#if (RTL8812F_SUPPORT == 1)
/*--------------------------Define Parameters-------------------------------*/
#define DPK_RF_PATH_NUM_8812F 2
#define DPK_GROUP_NUM_8812F 1
#define DPK_MAC_REG_NUM_8812F 2
#define DPK_BB_REG_NUM_8812F 16
#define DPK_RF_REG_NUM_8812F 7
#define DPK_PAS_CHK_DBG_8812F 0
#define DPK_PAS_DBG_8812F 0
#define DPK_COEF_DBG_8812F 0
#define DPK_SRAM_IQ_DBG_8812F 0
#define DPK_SRAM_read_DBG_8812F 0
#define DPK_SRAM_write_DBG_8812F 0
#define DPK_PATH_A_8812F 1
#define DPK_PATH_B_8812F 1
#define DPK_THRESHOLD_8812F 6
/*---------------------------End Define Parameters----------------------------*/

u8 dpk_channel_transfer_8812f(
	void *dm_void);

void dpk_coef_read_8812f(
	void *dm_void);

void dpk_enable_disable_8812f(
	void *dm_void);

void dpk_reload_8812f(
	void *dm_void);

void do_dpk_8812f(
	void *dm_void);

void dpk_track_8812f(
	void *dm_void);

void dpk_info_by_8812f(
	void *dm_void,
	u32 *_used,
	char *output,
	u32 *_out_len);

#endif /* RTL8812F_SUPPORT */

#endif /*#ifndef __HALRF_DPK_8812F_H__*/
