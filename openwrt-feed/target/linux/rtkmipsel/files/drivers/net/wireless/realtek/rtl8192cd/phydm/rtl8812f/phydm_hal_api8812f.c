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
#include "../phydm_precomp.h"

#if (RTL8812F_SUPPORT)
#if (PHYDM_FW_API_ENABLE_8812F)
/* ======================================================================== */
/* These following functions can be used for PHY DM only*/

enum channel_width bw_8812f;
static u8 central_ch_8812f;
static u32 bp_0x9b0_8812f;
static u8 TXAGC_IDX_8812F[2][ODM_RATEVHTSS2MCS9];

__odm_func__
void phydm_bb_reset_8812f(struct dm_struct *dm)
{
	if (*dm->mp_mode)
		return;

	odm_set_mac_reg(dm, R_0x0, BIT(16), 1);
	odm_set_mac_reg(dm, R_0x0, BIT(16), 0);
	odm_set_mac_reg(dm, R_0x0, BIT(16), 1);
}

__odm_func__
boolean phydm_chk_pkg_set_valid_8812f(struct dm_struct *dm,
				      u8 ver_bb, u8 ver_rf)
{
	boolean valid = true;

#if 0
	if (ver_bb >= ver_1 && ver_rf < ver_2)
		valid = false;

	if (!valid) {
		odm_set_bb_reg(dm, R_0x1c3c, (BIT(0) | BIT(1)), 0x0);
		pr_debug("[Warning][%s] Pkg_ver{bb, rf}={%d, %d} disable all BB block\n",
			 __func__, ver_bb, ver_rf);
	}
#endif

	return valid;
}

__odm_func__
void phydm_igi_toggle_8812f(struct dm_struct *dm)
{
/*
 * @Toggle IGI to force BB HW send 3-wire-cmd and will let RF HW enter RX mode.
 * @Because BB HW does not send 3-wire command automacically when BB setting
 * @is changed including the configuration of path/channel/BW
 */
	u32 igi = 0x20;

	/* @Do not use PHYDM API to read/write because FW can not access */
	igi = odm_get_bb_reg(dm, R_0x1d70, 0x7f);
	odm_set_bb_reg(dm, R_0x1d70, 0x7f, igi - 2);
	odm_set_bb_reg(dm, R_0x1d70, 0x7f00, igi - 2);
	odm_set_bb_reg(dm, R_0x1d70, 0x7f, igi);
	odm_set_bb_reg(dm, R_0x1d70, 0x7f00, igi);
}

__odm_func__
u32 phydm_check_bit_mask_8812f(u32 bit_mask, u32 data_original, u32 data)
{
	u8 bit_shift = 0;

	if (bit_mask != 0xfffff) {
		for (bit_shift = 0; bit_shift <= 19; bit_shift++) {
			if ((bit_mask >> bit_shift) & 0x1)
				break;
		}
		return (data_original & (~bit_mask)) | (data << bit_shift);
	}

	return data;
}

__odm_func__
u32 config_phydm_read_rf_reg_8812f(struct dm_struct *dm, enum rf_path path,
				   u32 reg_addr, u32 bit_mask)
{
	u32 readback_value = 0, direct_addr = 0;
	u32 offset_read_rf[2] = {R_0x3c00, R_0x4c00};

	PHYDM_DBG(dm, ODM_PHY_CONFIG, "%s ======>\n", __func__);

	/* @Error handling.*/
	if (path > RF_PATH_B) {
		PHYDM_DBG(dm, ODM_PHY_CONFIG, "Unsupported path (%d)\n", path);
		return INVALID_RF_DATA;
	}

	/* @Calculate offset */
	reg_addr &= 0xff;
	direct_addr = offset_read_rf[path] + (reg_addr << 2);

	/* @RF register only has 20bits */
	bit_mask &= RFREG_MASK;

	/* @Read RF register directly */
	readback_value = odm_get_bb_reg(dm, direct_addr, bit_mask);
	PHYDM_DBG(dm, ODM_PHY_CONFIG,
		  "RF-%d 0x%x = 0x%x, bit mask = 0x%x\n", path, reg_addr,
		  readback_value, bit_mask);
	return readback_value;
}

__odm_func__
boolean
config_phydm_direct_write_rf_reg_8812f(struct dm_struct *dm, enum rf_path path,
				       u32 reg_addr, u32 bit_mask, u32 data)
{
	u32 direct_addr = 0;
	u32 offset_write_rf[2] = {R_0x3c00, R_0x4c00};

	PHYDM_DBG(dm, ODM_PHY_CONFIG, "%s ======>\n", __func__);

	/* @Calculate offset */
	reg_addr &= 0xff;
	direct_addr = offset_write_rf[path] + (reg_addr << 2);

	/* @RF register only has 20bits */
	bit_mask &= RFREG_MASK;

	/* direct write only*/
	if (reg_addr == RF_0x18) {
		odm_set_mac_reg(dm, R_0x1c, BIT(31) | BIT(30), 0x3);
		odm_set_mac_reg(dm, R_0xec, BIT(31) | BIT(30), 0x3);
	}

	/* @write RF register directly*/
	odm_set_bb_reg(dm, direct_addr, bit_mask, data);

	ODM_delay_us(1);

	/* default setting: RF-0x0 is PI, others are direct*/
	if (reg_addr == RF_0x18) {
		odm_set_mac_reg(dm, R_0x1c, BIT(31) | BIT(30), 0x2);
		odm_set_mac_reg(dm, R_0xec, BIT(31) | BIT(30), 0x2);
	}

	PHYDM_DBG(dm, ODM_PHY_CONFIG, "RF-%d 0x%x = 0x%x , bit mask = 0x%x\n",
		  path, reg_addr, data, bit_mask);

	return true;
}

__odm_func__
boolean
config_phydm_write_rf_reg_8812f(struct dm_struct *dm, enum rf_path path,
				u32 reg_addr, u32 bit_mask, u32 data)
{
	u32 data_and_addr = 0, data_original = 0;
	u32 offset_write_rf[2] = {R_0x1808, R_0x4108};
	boolean result = false;

	PHYDM_DBG(dm, ODM_PHY_CONFIG, "%s ======>\n", __func__);

	/* @Error handling.*/
	if (path > RF_PATH_B) {
		PHYDM_DBG(dm, ODM_PHY_CONFIG, "Invalid path=%d\n", path);
		return false;
	}

	if (!(reg_addr == RF_0x0)) {
		result = config_phydm_direct_write_rf_reg_8812f(dm, path,
								reg_addr,
								bit_mask, data);
		return result;
	}

	/* @Read RF register content first */
	reg_addr &= 0xff;
	bit_mask &= RFREG_MASK;

	if (bit_mask != RFREG_MASK) {
		data_original = config_phydm_read_rf_reg_8812f(dm, path,
							       reg_addr,
							       RFREG_MASK);

		/* @Error handling. RF is disabled */
		if (!(data_original != INVALID_RF_DATA)) {
			PHYDM_DBG(dm, ODM_PHY_CONFIG,
				  "Write fail, RF is disable\n");
			return false;
		}

		/* @check bit mask */
		data = phydm_check_bit_mask_8812f(bit_mask, data_original,
						  data);
	}

	/* @Put write addr in [27:20] and write data in [19:00] */
	data_and_addr = ((reg_addr << 20) | (data & 0x000fffff)) & 0x0fffffff;

	/* @Write operation */
	odm_set_bb_reg(dm, offset_write_rf[path], MASKDWORD, data_and_addr);

	PHYDM_DBG(dm, ODM_PHY_CONFIG,
		  "RF-%d 0x%x = 0x%x (original: 0x%x), bit mask = 0x%x\n",
		  path, reg_addr, data, data_original, bit_mask);
#if (defined(CONFIG_RUN_IN_DRV))
	if (dm->support_interface == ODM_ITRF_PCIE)
		ODM_delay_us(13);
#elif (defined(CONFIG_RUN_IN_FW))
	ODM_delay_us(13);
#endif

	return true;
}

__odm_func__
boolean
phydm_write_txagc_1byte_8812f(struct dm_struct *dm, u32 pw_idx, u8 hw_rate)
{
#if (PHYDM_FW_API_FUNC_ENABLE_8812F)

	u32 offset_txagc = R_0x3a00;
	u8 rate_idx = (hw_rate & 0xfc), i = 0;
	u8 rate_offset = (hw_rate & 0x3);
	u8 ret = 0;
	u32 txagc_idx = 0x0;

	PHYDM_DBG(dm, ODM_PHY_CONFIG, "%s ======>\n", __func__);
	/* @For debug command only!!!! */

	/* @bbrstb TX AGC report - default disable */
	/* @Enable for writing the TX AGC table when bb_reset=0 */
	odm_set_bb_reg(dm, R_0x1c90, BIT(15), 0x0);

	/* @Error handling */
	if (hw_rate > 0x53) {
		PHYDM_DBG(dm, ODM_PHY_CONFIG, "Unsupported rate\n");
		return false;
	}

	/* @For HW limitation, We can't write TXAGC once a byte. */
	for (i = 0; i < 4; i++) {
		if (i != rate_offset) {
			ret = config_phydm_read_txagc_diff_8812f(dm,
								 rate_idx + i);
			txagc_idx |= ret << (i << 3);
		} else {
			txagc_idx |= (pw_idx & 0x7f) << (i << 3);
		}
	}
	odm_set_bb_reg(dm, (offset_txagc + rate_idx), MASKDWORD, txagc_idx);

	PHYDM_DBG(dm, ODM_PHY_CONFIG, "rate_idx 0x%x (0x%x) = 0x%x\n",
		  hw_rate, (offset_txagc + hw_rate), txagc_idx);
	return true;
#else
	return false;
#endif
}

__odm_func__
boolean
config_phydm_write_txagc_ref_8812f(struct dm_struct *dm, u8 power_index,
				   enum rf_path path,
				   enum PDM_RATE_TYPE rate_type)
{
	/* @2-path power reference */
	u32 txagc_ofdm_ref[2] = {R_0x18e8, R_0x41e8};
	u32 txagc_cck_ref[2] = {R_0x18a0, R_0x41a0};

	PHYDM_DBG(dm, ODM_PHY_CONFIG, "%s ======>\n", __func__);

	/* @Input need to be HW rate index, not driver rate index!!!! */
	if (dm->is_disable_phy_api) {
		PHYDM_DBG(dm, ODM_PHY_CONFIG, "Disable PHY API for debug\n");
		return true;
	}

	/* @Error handling */
	if (path > RF_PATH_B) {
		PHYDM_DBG(dm, ODM_PHY_CONFIG, "Unsupported path (%d)\n",
			  path);
		return false;
	}

	/* @bbrstb TX AGC report - default disable */
	/* @Enable for writing the TX AGC table when bb_reset=0 */
	odm_set_bb_reg(dm, R_0x1c90, BIT(15), 0x0);

	/* @According the rate to write in the ofdm or the cck */
	/* @CCK reference setting */
	if (rate_type == PDM_CCK) {
		odm_set_bb_reg(dm, txagc_cck_ref[path], 0x007f0000,
			       power_index);
		PHYDM_DBG(dm, ODM_PHY_CONFIG,
			  "path-%d rate type %d (0x%x) = 0x%x\n",
			  path, rate_type, txagc_cck_ref[path], power_index);

	/* @OFDM reference setting */
	} else {
		odm_set_bb_reg(dm, txagc_ofdm_ref[path], 0x0001fc00,
			       power_index);
		PHYDM_DBG(dm, ODM_PHY_CONFIG,
			  "path-%d rate type %d (0x%x) = 0x%x\n",
			  path, rate_type, txagc_ofdm_ref[path], power_index);
	}

	return true;
}

__odm_func__
boolean
config_phydm_write_txagc_diff_8812f(struct dm_struct *dm, s8 power_index1,
				    s8 power_index2, s8 power_index3,
				    s8 power_index4, u8 hw_rate)
{
	u32 offset_txagc = R_0x3a00;
	u8 rate_idx = hw_rate & 0xfc; /* @Extract the 0xfc */
	u8 power_idx1 = 0;
	u8 power_idx2 = 0;
	u8 power_idx3 = 0;
	u8 power_idx4 = 0;
	u32 pw_all = 0;

	power_idx1 = power_index1 & 0x7f;
	power_idx2 = power_index2 & 0x7f;
	power_idx3 = power_index3 & 0x7f;
	power_idx4 = power_index4 & 0x7f;
	pw_all = (power_idx4 << 24) | (power_idx3 << 16) | (power_idx2 << 8) |
		 power_idx1;

	PHYDM_DBG(dm, ODM_PHY_CONFIG, "%s ======>\n", __func__);

	/* @Input need to be HW rate index, not driver rate index!!!! */
	if (dm->is_disable_phy_api) {
		PHYDM_DBG(dm, ODM_PHY_CONFIG, "Disable PHY API for debug\n");
		return true;
	}

	/* @Error handling */
	if (hw_rate > ODM_RATEVHTSS2MCS9) {
		PHYDM_DBG(dm, ODM_PHY_CONFIG, "Unsupported rate\n");
		return false;
	}

	/* @bbrstb TX AGC report - default disable */
	/* @Enable for writing the TX AGC table when bb_reset=0 */
	odm_set_bb_reg(dm, R_0x1c90, BIT(15), 0x0);

	/* @According the rate to write in the ofdm or the cck */
	/* @driver need to construct a 4-byte power index */
	odm_set_bb_reg(dm, (offset_txagc + rate_idx), MASKDWORD, pw_all);

	PHYDM_DBG(dm, ODM_PHY_CONFIG, "rate index 0x%x (0x%x) = 0x%x\n",
		  hw_rate, (offset_txagc + hw_rate), pw_all);
	return true;
}


__odm_func__
boolean config_phydm_write_txagc_8812f(struct dm_struct *dm, u32 pw_idx,
				       enum rf_path path, u8 hw_rate)
{
	u8 ref_rate = ODM_RATEMCS15;
	u8 fill_valid_cnt = 0;
	u8 i = 0;

	if (dm->is_disable_phy_api) {
		PHYDM_DBG(dm, ODM_PHY_CONFIG, "Disable PHY API for debug\n");
		return true;
	}

	if (path > RF_PATH_B) {
		return false;
	}

	if ((hw_rate > ODM_RATEMCS15 && hw_rate <= ODM_RATEMCS31) ||
	    hw_rate > ODM_RATEVHTSS2MCS9) {
		return false;
	}

	if (hw_rate <= ODM_RATEMCS15)
		ref_rate = ODM_RATEMCS15;
	else
		ref_rate = ODM_RATEVHTSS2MCS9;

	fill_valid_cnt = ref_rate - hw_rate + 1;
	if (fill_valid_cnt > 4)
		fill_valid_cnt = 4;

	for (i = 0; i < fill_valid_cnt; i++)
		TXAGC_IDX_8812F[path][hw_rate + i] = (pw_idx >> (8 * i)) & 0xff;

	return true;
}

__odm_func__
s8 config_phydm_read_txagc_diff_8812f(struct dm_struct *dm, u8 hw_rate)
{
#if (PHYDM_FW_API_FUNC_ENABLE_8812F)
	s8 read_back_data = 0;

	PHYDM_DBG(dm, ODM_PHY_CONFIG, "%s ======>\n", __func__);

	/* @Input need to be HW rate index, not driver rate index!!!! */

	/* @Error handling */
	if (hw_rate > 0x53) {
		PHYDM_DBG(dm, ODM_PHY_CONFIG, "Unsupported rate\n");
		return INVALID_TXAGC_DATA;
	}

	/* @Disable TX AGC report */
	odm_set_bb_reg(dm, R_0x1c7c, BIT(23), 0x0); /* need to check */

	/* @Set data rate index (bit30~24) */
	odm_set_bb_reg(dm, R_0x1c7c, 0x7F000000, hw_rate);

	/* @Enable TXAGC report */
	odm_set_bb_reg(dm, R_0x1c7c, BIT(23), 0x1);

	/* @Read TX AGC report */
	read_back_data = (s8)odm_get_bb_reg(dm, R_0x2de8, 0xff);
	if (read_back_data & BIT(6))
		read_back_data |= BIT(7);

	/* @Driver have to disable TXAGC report after reading TXAGC */
	odm_set_bb_reg(dm, R_0x1c7c, BIT(23), 0x0);

	PHYDM_DBG(dm, ODM_PHY_CONFIG, "rate index 0x%x = 0x%x\n", hw_rate,
		  read_back_data);
	return read_back_data;
#else
	return 0;
#endif
}

__odm_func__
u8 config_phydm_read_txagc_8812f(struct dm_struct *dm, enum rf_path path,
				 u8 hw_rate, enum PDM_RATE_TYPE rate_type)
{
	s8 read_back_data = 0;
	u8 ref_data = 0;
	u8 result_data = 0;
	/* @2-path power reference */
	u32 r_txagc_ofdm[2] = {R_0x18e8, R_0x41e8};
	u32 r_txagc_cck[2] = {R_0x18a0, R_0x41a0};

	PHYDM_DBG(dm, ODM_PHY_CONFIG, "%s ======>\n", __func__);

	/* @Input need to be HW rate index, not driver rate index!!!! */

	/* @Error handling */
	if (path > RF_PATH_B || hw_rate > 0x53) {
		PHYDM_DBG(dm, ODM_PHY_CONFIG, "Unsupported path (%d)\n", path);
		return INVALID_TXAGC_DATA;
	}

	/* @Disable TX AGC report */
	odm_set_bb_reg(dm, R_0x1c7c, BIT(23), 0x0); /* need to check */

	/* @Set data rate index (bit30~24) */
	odm_set_bb_reg(dm, R_0x1c7c, 0x7F000000, hw_rate);

	/* @Enable TXAGC report */
	odm_set_bb_reg(dm, R_0x1c7c, BIT(23), 0x1);

	/* @Read power difference report */
	read_back_data = (s8)odm_get_bb_reg(dm, R_0x2de8, 0xff);
	if (read_back_data & BIT(6))
		read_back_data |= BIT(7);

	/* @Read power reference value report */
	if (rate_type == PDM_CCK) /* @Bit=22:16 */
		ref_data = (u8)odm_get_bb_reg(dm, r_txagc_cck[path], 0x7F0000);
	else if (rate_type == PDM_OFDM) /* @Bit=16:10 */
		ref_data = (u8)odm_get_bb_reg(dm, r_txagc_ofdm[path], 0x1FC00);

	PHYDM_DBG(dm, ODM_PHY_CONFIG, "diff=%d ref=%d\n", read_back_data,
		  ref_data);

	if (read_back_data + ref_data < 0)
		result_data = 0;
	else
		result_data = read_back_data + ref_data;

	/* @Driver have to disable TXAGC report after reading TXAGC */
	odm_set_bb_reg(dm, R_0x1c7c, BIT(23), 0x0);

	PHYDM_DBG(dm, ODM_PHY_CONFIG, "path-%d rate index 0x%x = 0x%x\n",
		  path, hw_rate, result_data);
	return result_data;
}

__odm_func__
void

phydm_config_ofdm_tx_path_8812f(struct dm_struct *dm, enum bb_path tx_path_2ss,
				enum bb_path tx_path_sel_1ss)
{
	u8 tx_path_2ss_en = false;

	if (tx_path_2ss == BB_PATH_AB)
		tx_path_2ss_en = true;

	if (!tx_path_2ss_en) {/* 1ss1T, do not config this with STBC*/
		if (tx_path_sel_1ss == BB_PATH_A) {
			odm_set_bb_reg(dm, R_0x820, 0xff, 0x1);
			odm_set_bb_reg(dm, R_0x1e2c, 0xffff, 0x0);
		} else { /*if (tx_path_sel_1ss == BB_PATH_B)*/
			odm_set_bb_reg(dm, R_0x820, 0xff, 0x2);
			odm_set_bb_reg(dm, R_0x1e2c, 0xffff, 0x0);
		}
	} else {
		if (tx_path_sel_1ss == BB_PATH_A) {
			odm_set_bb_reg(dm, R_0x820, 0xff, 0x31);
			odm_set_bb_reg(dm, R_0x1e2c, 0xffff, 0x0400);
		} else if (tx_path_sel_1ss == BB_PATH_B) {
			odm_set_bb_reg(dm, R_0x820, 0xff, 0x32);
			odm_set_bb_reg(dm, R_0x1e2c, 0xffff, 0x0400);
		} else { /*BB_PATH_AB*/
			odm_set_bb_reg(dm, R_0x820, 0xff, 0x33);
			odm_set_bb_reg(dm, R_0x1e2c, 0xffff, 0x0404);
		}
	}

#ifdef CONFIG_PATH_DIVERSITY
	if (!dm->dm_path_div.path_div_in_progress)
		phydm_bb_reset_8812f(dm);
#else
	phydm_bb_reset_8812f(dm);
#endif
}

__odm_func__
void
phydm_config_ofdm_rx_path_8812f(struct dm_struct *dm, enum bb_path rx_path)
{
	u32 ofdm_rx = 0x0;

	ofdm_rx = (u32)rx_path;
	if (!(*dm->mp_mode)) {
		if (ofdm_rx == BB_PATH_B) {
			ofdm_rx = BB_PATH_AB;
			odm_set_bb_reg(dm, R_0xcc0, 0x7ff, 0x0);
			odm_set_bb_reg(dm, R_0xcc0, BIT(22), 0x1);
			odm_set_bb_reg(dm, R_0xcc8, 0x7ff, 0x0);
			odm_set_bb_reg(dm, R_0xcc8, BIT(22), 0x1);
		} else { /* ofdm_rx == BB_PATH_A || ofdm_rx == BB_PATH_AB*/
			odm_set_bb_reg(dm, R_0xcc0, 0x7ff, 0x400);
			odm_set_bb_reg(dm, R_0xcc0, BIT(22), 0x0);
			odm_set_bb_reg(dm, R_0xcc8, 0x7ff, 0x400);
			odm_set_bb_reg(dm, R_0xcc8, BIT(22), 0x0);
		}
	}

	if (ofdm_rx == BB_PATH_A || ofdm_rx == BB_PATH_B) {
		/*@ ht_mcs_limit*/
		odm_set_bb_reg(dm, R_0x1d30, 0x300, 0x0);
		/*@ vht_nss_limit*/
		odm_set_bb_reg(dm, R_0x1d30, 0x600000, 0x0);
		/* @Disable Antenna weighting */
		odm_set_bb_reg(dm, R_0xc44, BIT(17), 0x0);
		/* @htstf ant-wgt enable = 0*/
		odm_set_bb_reg(dm, R_0xc54, BIT(20), 0x0);
		/* @MRC_mode = 'original ZF eqz'*/
		odm_set_bb_reg(dm, R_0xc38, BIT(24), 0x0);
		/* @Rx_ant */
		odm_set_bb_reg(dm, R_0x824, 0x000f0000, rx_path);
		/* @Rx_CCA*/
		odm_set_bb_reg(dm, R_0x824, 0x0f000000, rx_path);
	} else if (ofdm_rx == BB_PATH_AB) {
		/*@ ht_mcs_limit*/
		odm_set_bb_reg(dm, R_0x1d30, 0x300, 0x1);
		/*@ vht_nss_limit*/
		odm_set_bb_reg(dm, R_0x1d30, 0x600000, 0x1);
		/* @Enable Antenna weighting */
		odm_set_bb_reg(dm, R_0xc44, BIT(17), 0x1);
		/* @htstf ant-wgt enable = 1*/
		odm_set_bb_reg(dm, R_0xc54, BIT(20), 0x1);
		/* @MRC_mode = 'modified ZF eqz'*/
		odm_set_bb_reg(dm, R_0xc38, BIT(24), 0x1);
		/* @Rx_ant */
		odm_set_bb_reg(dm, R_0x824, 0x000f0000, BB_PATH_AB);
		/* @Rx_CCA*/
		odm_set_bb_reg(dm, R_0x824, 0x0f000000, BB_PATH_AB);
	}

#ifdef CONFIG_PATH_DIVERSITY
	if (!dm->dm_path_div.path_div_in_progress)
		phydm_bb_reset_8812f(dm);
#else
	phydm_bb_reset_8812f(dm);
#endif
}

__odm_func__
void phydm_config_tx_path_8812f(struct dm_struct *dm, enum bb_path tx_path_2ss,
				enum bb_path tx_path_sel_1ss,
				enum bb_path tx_path_sel_cck)
{
	dm->tx_2ss_status = tx_path_2ss;
	dm->tx_1ss_status = tx_path_sel_1ss;

	dm->tx_ant_status = dm->tx_2ss_status | dm->tx_1ss_status;

	/* @OFDM TX antenna mapping*/
	phydm_config_ofdm_tx_path_8812f(dm, tx_path_2ss, tx_path_sel_1ss);

	PHYDM_DBG(dm, ODM_PHY_CONFIG, "path_sel_2ss/1ss/cck={%d, %d, %d}\n",
		  tx_path_2ss, tx_path_sel_1ss, tx_path_sel_cck);

#ifdef CONFIG_PATH_DIVERSITY
	if (!dm->dm_path_div.path_div_in_progress)
		phydm_bb_reset_8812f(dm);
#else
	phydm_bb_reset_8812f(dm);
#endif
}

__odm_func__
void phydm_config_rx_path_8812f(struct dm_struct *dm, enum bb_path rx_path)
{

	/* @OFDM RX antenna mapping*/
	phydm_config_ofdm_rx_path_8812f(dm, rx_path);

	dm->rx_ant_status = rx_path;

#ifdef CONFIG_PATH_DIVERSITY
	if (!dm->dm_path_div.path_div_in_progress)
		phydm_bb_reset_8812f(dm);
#else
	phydm_bb_reset_8812f(dm);
#endif
}

__odm_func__
void
phydm_set_rf_mode_table_8812f(struct dm_struct *dm,
			      enum bb_path tx_path_mode_table,
			      enum bb_path rx_path,
			      enum bb_path tx_path_sel_1ss)
{
	 /* @Cannot shut down path-A, beacause synthesizer will shut down
	  * @when path-A is in shut down mode
	  */

	/* @[3-wire setting]  0: shutdown, 1: standby, 2: TX, 3: RX*/
	if (tx_path_mode_table == BB_PATH_A && rx_path == BB_PATH_A) {
		odm_set_bb_reg(dm, R_0x1800, MASK20BITS, 0x33312);
		odm_set_bb_reg(dm, R_0x4100, MASK20BITS, 0x11111);
	} else if (tx_path_mode_table == BB_PATH_A && rx_path == BB_PATH_B) {
		odm_set_bb_reg(dm, R_0x1800, MASK20BITS, 0x11112);
		odm_set_bb_reg(dm, R_0x4100, MASK20BITS, 0x33311);
	} else if (tx_path_mode_table == BB_PATH_A && rx_path == BB_PATH_AB) {
		odm_set_bb_reg(dm, R_0x1800, MASK20BITS, 0x33312);
		odm_set_bb_reg(dm, R_0x4100, MASK20BITS, 0x33311);
	} else if (tx_path_mode_table == BB_PATH_B && rx_path == BB_PATH_A) {
		odm_set_bb_reg(dm, R_0x1800, MASK20BITS, 0x33311);
		odm_set_bb_reg(dm, R_0x4100, MASK20BITS, 0x11112);
	} else if (tx_path_mode_table == BB_PATH_B && rx_path == BB_PATH_B) {
		odm_set_bb_reg(dm, R_0x1800, MASK20BITS, 0x11111);
		odm_set_bb_reg(dm, R_0x4100, MASK20BITS, 0x33312);
	} else if (tx_path_mode_table == BB_PATH_B && rx_path == BB_PATH_AB) {
		odm_set_bb_reg(dm, R_0x1800, MASK20BITS, 0x33311);
		odm_set_bb_reg(dm, R_0x4100, MASK20BITS, 0x33312);
	} else if (tx_path_mode_table == BB_PATH_AB && rx_path == BB_PATH_A) {
		odm_set_bb_reg(dm, R_0x1800, MASK20BITS, 0x33312);
		odm_set_bb_reg(dm, R_0x4100, MASK20BITS, 0x11112);
	} else if (tx_path_mode_table == BB_PATH_AB && rx_path == BB_PATH_B) {
		odm_set_bb_reg(dm, R_0x1800, MASK20BITS, 0x11112);
		odm_set_bb_reg(dm, R_0x4100, MASK20BITS, 0x33312);
	} else if (tx_path_mode_table == BB_PATH_AB && rx_path == BB_PATH_AB) {
		odm_set_bb_reg(dm, R_0x1800, MASK20BITS, 0x33312);
		odm_set_bb_reg(dm, R_0x4100, MASK20BITS, 0x33312);
	}
}

__odm_func__
boolean
config_phydm_trx_mode_8812f(struct dm_struct *dm, enum bb_path tx_path_en,
			    enum bb_path rx_path, enum bb_path tx_path_sel_1ss)
{
#ifdef CONFIG_PATH_DIVERSITY
	struct _ODM_PATH_DIVERSITY_ *p_div = &dm->dm_path_div;
#endif
	boolean disable_2sts_div_mode = false;
	enum bb_path tx_path_mode_table = tx_path_en;
	enum bb_path tx_path_2ss = BB_PATH_AB;

	PHYDM_DBG(dm, ODM_PHY_CONFIG, "%s ======>\n", __func__);

	if (dm->is_disable_phy_api) {
		pr_debug("[%s] Disable PHY API\n", __func__);
		return true;
	}

	/*RX Check*/
	if (rx_path & ~BB_PATH_AB) {
		pr_debug("[Warning][%s] RX:0x%x\n", __func__, rx_path);
		return false;
	}

	/*TX Check*/
	if (tx_path_en == BB_PATH_AUTO && tx_path_sel_1ss == BB_PATH_AUTO) {
		/*@ Shutting down 2sts rate, but 1sts PathDiv is enabled*/
		disable_2sts_div_mode = true;
		tx_path_mode_table = BB_PATH_AB;
	} else if (tx_path_en & ~BB_PATH_AB) {
		pr_debug("[Warning][%s] TX:0x%x\n", __func__, tx_path_en);
		return false;
	}

	/* @==== [RF Mode Table] ========================================*/
	phydm_set_rf_mode_table_8812f(dm, tx_path_mode_table,
				      rx_path, tx_path_sel_1ss);

	/* RFE Type 1 & 3 & 4Control Pin Configuration */
	if (dm->rfe_type == 1 || dm->rfe_type == 3 || dm->rfe_type == 4) {
		if (tx_path_en == BB_PATH_A && rx_path == BB_PATH_A)
			odm_set_bb_reg(dm, R_0x4144, MASKBYTE1, 0x77);
		else
			odm_set_bb_reg(dm, R_0x4144, MASKBYTE1, 0x43);
		
		if (tx_path_en == BB_PATH_B && rx_path == BB_PATH_B)
			odm_set_bb_reg(dm, R_0x1840, MASKBYTE1, 0x77);
		else
			odm_set_bb_reg(dm, R_0x1840, MASKBYTE1, 0x43);
	}
	/* @==== [RX Path] ==============================================*/
	phydm_config_rx_path_8812f(dm, rx_path);

	/* @==== [TX Path] ==============================================*/
#ifdef CONFIG_PATH_DIVERSITY
	/*@ [PHYDM-312]*/
	if (p_div->default_tx_path != BB_PATH_A &&
	    p_div->default_tx_path != BB_PATH_B)
		p_div->default_tx_path = BB_PATH_A;

	if (tx_path_en == BB_PATH_A || tx_path_en == BB_PATH_B) {
		p_div->stop_path_div = true;
		tx_path_sel_1ss = tx_path_en;
		tx_path_2ss = BB_PATH_NON;
	} else if (tx_path_en == BB_PATH_AB) {
		if (tx_path_sel_1ss == BB_PATH_AUTO) {
			p_div->stop_path_div = false;
			tx_path_sel_1ss = p_div->default_tx_path;
		} else { /* @BB_PATH_AB, BB_PATH_A, BB_PATH_B*/
			p_div->stop_path_div = true;
		}
		tx_path_2ss = BB_PATH_AB;
	} else if (disable_2sts_div_mode) {
		p_div->stop_path_div = false;
		tx_path_sel_1ss = p_div->default_tx_path;
		tx_path_2ss = BB_PATH_NON;
	}
#else
	tx_path_sel_1ss = tx_path_en;
#endif
	phydm_config_tx_path_8812f(dm, tx_path_2ss, tx_path_sel_1ss,
				   tx_path_sel_1ss);

	phydm_igi_toggle_8812f(dm);

	PHYDM_DBG(dm, ODM_PHY_CONFIG, "RX_en=%x, tx_en/2ss/1ss={%x,%x,%x}\n",
		  rx_path, tx_path_en, tx_path_2ss, tx_path_sel_1ss);

#ifdef CONFIG_PATH_DIVERSITY
	if (!p_div->path_div_in_progress)
		phydm_bb_reset_8812f(dm);
#else
	phydm_bb_reset_8812f(dm);
#endif
	return true;
}

__odm_func__
boolean
config_phydm_switch_band_8812f(struct dm_struct *dm, u8 central_ch)
{
	return true;
}

__odm_func__
void
phydm_agc_tab_sel_8812f(struct dm_struct *dm, u8 central_ch)
{
	struct phydm_dig_struct *dig_tab = &dm->dm_dig_table;
	//Check AGC Table
	if (central_ch >= 36 && central_ch <= 64) {
		odm_set_bb_reg(dm, R_0x18ac, 0x1f0, 0x1);
		odm_set_bb_reg(dm, R_0x41ac, 0x1f0, 0x1);
		dig_tab->agc_table_idx = 0x1;
	} else if ((central_ch >= 100) && (central_ch <= 144)) {
		odm_set_bb_reg(dm, R_0x18ac, 0x1f0, 0x2);
		odm_set_bb_reg(dm, R_0x41ac, 0x1f0, 0x2);
		dig_tab->agc_table_idx = 0x2;
	} else { /*if (central_ch >= 149)*/
		odm_set_bb_reg(dm, R_0x18ac, 0x1f0, 0x3);
		odm_set_bb_reg(dm, R_0x41ac, 0x1f0, 0x3);
		dig_tab->agc_table_idx = 0x3;
	}
}

__odm_func__
void
phydm_sco_trk_fc_setting_8812f(struct dm_struct *dm, u8 central_ch)
{
	if (central_ch == 13 || central_ch == 14) {
		/* @n:41, s:37 */
		odm_set_bb_reg(dm, R_0xc30, 0xfff, 0x969);
	} else if (central_ch == 11 || central_ch == 12) {
		/* @n:42, s:37 */
		odm_set_bb_reg(dm, R_0xc30, 0xfff, 0x96a);
	} else if (central_ch >= 1 && central_ch <= 10) {
		/* @n:42, s:38 */
		odm_set_bb_reg(dm, R_0xc30, 0xfff, 0x9aa);
	} else if (central_ch >= 36 && central_ch <= 51) {
		/* @n:20, s:18 */
		odm_set_bb_reg(dm, R_0xc30, 0xfff, 0x494);
	} else if (central_ch >= 52 && central_ch <= 55) {
		/* @n:19, s:18 */
		odm_set_bb_reg(dm, R_0xc30, 0xfff, 0x493);
	} else if ((central_ch >= 56) && (central_ch <= 111)) {
		/* @n:19, s:17 */
		odm_set_bb_reg(dm, R_0xc30, 0xfff, 0x453);
	} else if ((central_ch >= 112) && (central_ch <= 119)) {
		/* @n:18, s:17 */
		odm_set_bb_reg(dm, R_0xc30, 0xfff, 0x452);
	} else if ((central_ch >= 120) && (central_ch <= 172)) {
		/* @n:18, s:16 */
		odm_set_bb_reg(dm, R_0xc30, 0xfff, 0x412);
	} else { /* if ((central_ch >= 173) && (central_ch <= 177)) */
		/* n:17, s:16 */
		odm_set_bb_reg(dm, R_0xc30, 0xfff, 0x411);
	}
}

boolean phydm_tone_intf_bw_mapping_8812f(struct dm_struct *dm, u8 ch, u8 *bw,
					 u32 *tone_idx, u32 *intf,
					 u8 *efem_only)
{
	/*spur on DC needs to move central freq, then do psd*/
	/*20M flow needs to refine coz spur on DC tone*/
	boolean mapping_result = true;

	if (ch >= 36) {
		if (ch == 153 && *bw == CHANNEL_WIDTH_20) {
			*tone_idx = 0xfec; /* idx = -20 */
			*intf = 5760;
			*bw = 20;
			dm->csi_wgt = 4;
		} else if (ch == 151 && *bw == CHANNEL_WIDTH_40) {
			*tone_idx = 0x14; /* idx = 20 */
			*intf = 5760;
			*bw = 40;
			dm->csi_wgt = 4;
		} else if (ch == 155 && *bw == CHANNEL_WIDTH_80) {
			*tone_idx = 0xfc4;/* idx = -60 */
			*intf = 5760;
			*bw = 80;
			dm->csi_wgt = 4;
		} else {
			mapping_result = false;
		}
	} else {
		mapping_result = false;
	}

	return mapping_result;
}

u32 phydm_rf_psd_8812f(struct dm_struct *dm, u8 path, u32 tone_idx)
{
	u32 reg_1b04 = 0, reg_1b08 = 0, reg_1b0c_11_10 = 0;
	u32 reg_1b14 = 0, reg_1b18 = 0, reg_1b1c = 0;
	u32 reg_1b28 = 0;
	u32 reg_1bcc_5_0 = 0;
	u32 reg_1b2c_27_16 = 0, reg_1b34 = 0, reg_1bd4 = 0;
	u32 reg_180c = 0, reg_410c = 0, reg_520c = 0, reg_530c = 0;
	u32 igi = 0;
	u32 i = 0;
	u32 psd_val = 0, psd_val_42_32 = 0, psd_val_31_0 = 0, psd_max = 0;

	/*read and record the ori. value*/
	reg_1b04 = odm_get_bb_reg(dm, R_0x1b04, MASKDWORD);
	reg_1b08 = odm_get_bb_reg(dm, R_0x1b08, MASKDWORD);
	reg_1b0c_11_10 = odm_get_bb_reg(dm, R_0x1b0c, 0xc00);
	reg_1b14 = odm_get_bb_reg(dm, R_0x1b14, MASKDWORD);
	reg_1b18 = odm_get_bb_reg(dm, R_0x1b18, MASKDWORD);
	reg_1b1c = odm_get_bb_reg(dm, R_0x1b1c, MASKDWORD);
	reg_1b28 = odm_get_bb_reg(dm, R_0x1b28, MASKDWORD);
	reg_1bcc_5_0 = odm_get_bb_reg(dm, R_0x1bcc, 0x3f);
	reg_1b2c_27_16 = odm_get_bb_reg(dm, R_0x1b2c, 0xfff0000);
	reg_1b34 = odm_get_bb_reg(dm, R_0x1b34, MASKDWORD);
	reg_1bd4 = odm_get_bb_reg(dm, R_0x1bd4, MASKDWORD);
	igi = odm_get_bb_reg(dm, R_0x1d70, MASKDWORD);
	reg_180c = odm_get_bb_reg(dm, R_0x180c, 0x3);
	reg_410c = odm_get_bb_reg(dm, R_0x410c, 0x3);
	//reg_520c = odm_get_bb_reg(dm, R_0x520c, 0x3);
	//reg_530c = odm_get_bb_reg(dm, R_0x530c, 0x3);

	/*rf psd reg setting*/
	odm_set_bb_reg(dm, R_0x1b00, 0x6, path); /*path is RF_path*/
	odm_set_bb_reg(dm, R_0x1b04, MASKDWORD, 0x0);
	odm_set_bb_reg(dm, R_0x1b08, MASKDWORD, 0x80);
	odm_set_bb_reg(dm, R_0x1b0c, 0xc00, 0x3);
	odm_set_bb_reg(dm, R_0x1b14, MASKDWORD, 0x0);
	odm_set_bb_reg(dm, R_0x1b18, MASKDWORD, 0x1);
/*#if (DM_ODM_SUPPORT_TYPE == ODM_AP)*/
	odm_set_bb_reg(dm, R_0x1b1c, MASKDWORD, 0x82103D21);
/*#else*/
	/*odm_set_bb_reg(dm, R_0x1b1c, MASKDWORD, 0x821A3D21);*/
/*#endif*/
	odm_set_bb_reg(dm, R_0x1b28, MASKDWORD, 0x0);
	odm_set_bb_reg(dm, R_0x1bcc, 0x3f, 0x3f);
	odm_set_bb_reg(dm, R_0x8a0, 0xf, 0x0); /* AGC off */
	odm_set_bb_reg(dm, R_0x1d70, MASKDWORD, 0x20202020);

	for (i = tone_idx; i <= tone_idx + 2; i++) {
		/*set psd tone_idx for detection*/
		odm_set_bb_reg(dm, R_0x1b2c, 0xfff0000, i-1);
		/*one shot for RXIQK psd*/
		odm_set_bb_reg(dm, R_0x1b34, MASKDWORD, 0x1);
		odm_set_bb_reg(dm, R_0x1b34, MASKDWORD, 0x0);

		ODM_delay_us(250);

		/*read RxIQK power*/
		odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x00250001);
		psd_val_42_32 = odm_get_bb_reg(dm, R_0x1bfc, 0x7ff0000);
		odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, 0x002e0001);
		psd_val_31_0 = odm_get_bb_reg(dm, R_0x1bfc, MASKDWORD);
		psd_val = (psd_val_42_32 << 21) + (psd_val_31_0 >> 11);
		if (psd_val > psd_max)
			psd_max = psd_val;
	}

	/*refill the ori. value*/
	odm_set_bb_reg(dm, R_0x1b00, 0x6, path);
	odm_set_bb_reg(dm, R_0x1b04, MASKDWORD, reg_1b04);
	odm_set_bb_reg(dm, R_0x1b08, MASKDWORD, reg_1b08);
	odm_set_bb_reg(dm, R_0x1b0c, 0xc00, reg_1b0c_11_10);
	odm_set_bb_reg(dm, R_0x1b14, MASKDWORD, reg_1b14);
	odm_set_bb_reg(dm, R_0x1b18, MASKDWORD, reg_1b18);
	odm_set_bb_reg(dm, R_0x1b1c, MASKDWORD, reg_1b1c);
	odm_set_bb_reg(dm, R_0x1b28, MASKDWORD, reg_1b28);
	odm_set_bb_reg(dm, R_0x1bcc, 0x3f, reg_1bcc_5_0);
	odm_set_bb_reg(dm, R_0x1b2c, 0xfff0000, reg_1b2c_27_16);
	odm_set_bb_reg(dm, R_0x1b34, MASKDWORD, reg_1b34);
	odm_set_bb_reg(dm, R_0x1bd4, MASKDWORD, reg_1bd4);
	odm_set_bb_reg(dm, R_0x8a0, 0xf, 0xf); /* AGC on */
	odm_set_bb_reg(dm, R_0x1d70, MASKDWORD, igi);
	PHYDM_DBG(dm, ODM_COMP_API, "psd_max %d\n", psd_max);

	return psd_max;
}

__odm_func__
void phydm_dynamic_spur_det_eliminate_8812f(struct dm_struct *dm)
{
#if (PHYDM_FW_API_FUNC_ENABLE_8812F == 1)
	u32 threshold_nbi_db = 38, threshold_csi_db = 38;
	u32 tone_idx = 0, f_intf = 0;
	u32 psd_db[NUM_PATH] = {0};
	u32 psd_tmp = 0, tmp_val = 0;
	u8 path[NUM_PATH] = {RF_PATH_A, RF_PATH_B};
	u8 path_en[NUM_PATH] = {0};
	boolean nbi_enable[NUM_PATH] = {false, false};
	boolean csi_enable = false, mapping_result = false;
	boolean nbi_2g_dis = false;
	u8 set_nbi[NUM_PATH] = {PHYDM_SET_NO_NEED, PHYDM_SET_NO_NEED};
	u8 set_csi = PHYDM_SET_NO_NEED;
	u8 init_nbi, init_csi;
	u8 channel = *dm->channel, bw = *dm->band_width;
	u8 dsde_sel = dm->dsde_sel, nbi_path = dm->nbi_path_sel;
	u8 csi_wgt = dm->csi_wgt;
	u8 i = 0, j = 0, efem_only = false;

	if (channel < 15) {
		threshold_nbi_db = 34;
		threshold_csi_db = 10;
		nbi_2g_dis = true;
	}

	/*Init NBI/CSI*/
	init_csi = phydm_csi_mask_setting_jgr3(dm, FUNC_DISABLE, 0, 0, 0, 0, 0);
	init_nbi = phydm_nbi_setting_jgr3(dm, FUNC_DISABLE, 0, 0, 0, 0, 0);
	PHYDM_DBG(dm, ODM_COMP_API, "init_nbi %d, init_csi %d, ext_pa %d\n",
		  init_csi, init_nbi, dm->ext_pa);

	if (dsde_sel == DET_DISABLE) {
		PHYDM_DBG(dm, ODM_COMP_API, "Spur Detect Disable\n");
		return;
	} else if (dsde_sel == DET_CSI) {
		set_csi = phydm_csi_mask_setting_jgr3(dm, FUNC_ENABLE,
						      channel, bw,
						      f_intf * 1000, 0,
						      csi_wgt);
		PHYDM_DBG(dm, ODM_COMP_API, "CSI DET only, set %d", set_csi);
		return;
	} else if (dsde_sel == DET_NBI) {
		set_nbi[nbi_path] = phydm_nbi_setting_jgr3(dm, FUNC_ENABLE,
							   channel, bw,
							   f_intf * 1000,
							   0, path[nbi_path]);
		PHYDM_DBG(dm, ODM_COMP_API, "NBI DET only, set %d",
			  set_nbi[nbi_path]);
		return;
	} else {
		PHYDM_DBG(dm, ODM_COMP_API, "Spur Detect auto\n");
	}

	/*check rx path on*/
	path_en[0] = (dm->rx_ant_status & BB_PATH_A) ? true : false; /*Path A*/
	path_en[1] = (dm->rx_ant_status & BB_PATH_B) ? true : false; /*Path B*/

	PHYDM_DBG(dm, ODM_COMP_API, "path en (%d,%d)\n",
		  path_en[0], path_en[1]);

	/*tone idx & interference mapping*/
	mapping_result = phydm_tone_intf_bw_mapping_8812f(dm, channel, &bw,
							  &tone_idx, &f_intf,
							  &efem_only);

	if (mapping_result) {
		PHYDM_DBG(dm, ODM_COMP_API,
			  "nbi_TH (%d)dB, csi_TH (%d)dB, map_r %d\n",
			  threshold_nbi_db, threshold_csi_db, mapping_result);
		PHYDM_DBG(dm, ODM_COMP_API,
			  "Tone Idx %x, interf (%d)M, ch %d, bw %d, efem %d\n",
			  tone_idx, f_intf, channel, bw, efem_only);
		odm_set_bb_reg(dm, R_0x82c, 0xf, 0xb);
	} else {
		PHYDM_DBG(dm, ODM_COMP_API,
			  "Idx Not Found, Not Support Dyn_Elmntr\n");
		odm_set_bb_reg(dm, R_0x82c, 0xf, 0xd);
		return;
	}

	/*RFIQK psd detection, average 32 times psd*/
	for (i = 0; i < NUM_PATH; i++) {
		if (path_en[i] == 0)
			continue;
		for (j = 0; j < NUM_TRY; j++) {
			tmp_val = phydm_rf_psd_8812f(dm, path[i], tone_idx);
			psd_tmp += (tmp_val >> 5);
			tmp_val = 0;
		}
		psd_db[i] = odm_convert_to_db(psd_tmp);
		psd_tmp = 0;
	}
	PHYDM_DBG(dm, ODM_COMP_API, "psd in dB (%d,%d)\n",
		  psd_db[0], psd_db[1]);

	/*NBI/CSI psd TH check*/
	for (i = 0; i < NUM_PATH; i++) {
		PHYDM_DBG(dm, ODM_COMP_API,
			  "psd_db = %d, threshold_nbi_db = %d\n",
			  psd_db[i], threshold_nbi_db);
		/*AP eFEM NBI always on*/
		if (psd_db[i] >= threshold_nbi_db)
			nbi_enable[i] = true;
		/*AP eFEM CSI always off*/
		if (psd_db[i] >= threshold_csi_db)
			csi_enable = true;
		if (nbi_2g_dis)
			nbi_enable[i] = false;
	}
	PHYDM_DBG(dm, ODM_COMP_API, "nbi_en (%d,%d), csi_en %d\n",
		  nbi_enable[0], nbi_enable[1], csi_enable);

	for (i = 0; i < NUM_PATH; i++) {
		if (nbi_enable[i] && path_en[i]) {
			set_nbi[i] = phydm_nbi_setting_jgr3(dm, FUNC_ENABLE,
							    channel, bw,
							    f_intf * 1000, 0,
							    path[i]);
			PHYDM_DBG(dm, ODM_COMP_API,
				  "rfe_type = %d, bw = %d, channel = %d\n",
				  dm->rfe_type, bw, channel);
			odm_set_bb_reg(dm, R_0x818, 0xff, 0xf9);
			odm_set_bb_reg(dm, R_0x1944, 0x300, 0x1);
			odm_set_bb_reg(dm, R_0x4044, 0x300, 0x0);
			odm_set_bb_reg(dm, R_0x810, 0xf, 0x7);
			odm_set_bb_reg(dm, R_0x810, 0xf0000, 0x7);
		}
	}

	if (csi_enable)
		set_csi = phydm_csi_mask_setting_jgr3(dm, FUNC_ENABLE, channel,
						      bw, f_intf * 1000, 0,
						      dm->csi_wgt);

	PHYDM_DBG(dm, ODM_COMP_API, "set_nbi (%d,%d), set_csi %d\n",
		  set_nbi[0], set_nbi[1], set_csi);
#endif
}

boolean
config_phydm_switch_channel_8812f(struct dm_struct *dm, u8 central_ch)
{
	u32 rf_reg18 = 0;
	boolean is_2g_ch = true;

	PHYDM_DBG(dm, ODM_PHY_CONFIG, "%s ======>\n", __func__);

	if (dm->is_disable_phy_api) {
		PHYDM_DBG(dm, ODM_PHY_CONFIG, "Disable PHY API\n");
		return true;
	}

	if ((central_ch > 14 && central_ch < 36) ||
	    (central_ch > 64 && central_ch < 100) ||
	    (central_ch > 144 && central_ch < 149) ||
	    central_ch > 177) {
		PHYDM_DBG(dm, ODM_PHY_CONFIG, "Error CH:%d\n", central_ch);
		return false;
	}

	rf_reg18 = config_phydm_read_rf_reg_8812f(dm, RF_PATH_A, RF_0x18,
						  RFREG_MASK);
	if (rf_reg18 == INVALID_RF_DATA) {
		PHYDM_DBG(dm, ODM_PHY_CONFIG, "Invalid RF_0x18\n");
		return false;
	}

	is_2g_ch = (central_ch <= 14) ? true : false;

/* ==== [Set BB Reg] =================================================*/

	/* @1. AGC table selection */
	phydm_agc_tab_sel_8812f(dm, central_ch);
	/* @2. Set fc for clock offset tracking */
	phydm_sco_trk_fc_setting_8812f(dm, central_ch);
	/* @Switch band and channel */
	if (central_ch > 35) {
		/* @5G */
		/* @Enable CCK check */
		odm_set_mac_reg(dm, R_0x454, BIT(7), 0x1);
		/* @CCA Mask */
		odm_set_bb_reg(dm, R_0x1c80, 0x3F000000, 0x22);

	/* ==== [Set RF Reg 0x18] ===========================================*/
	rf_reg18 &= ~0x703ff; /*[18:17],[16],[9:8],[7:0]*/
	rf_reg18 |= central_ch; /* @Channel*/
	rf_reg18 |= (BIT(16) | BIT(8));

	/* @5G Sub-Band, 01: 5400<f<=5720, 10: f>5720*/
		if (central_ch > 144)
			rf_reg18 |= BIT(18);
		else if (central_ch >= 80)
			rf_reg18 |= BIT(17);
		/* @TX DFIR*/
		odm_set_bb_reg(dm, R_0x808, 0x70, 0x3);
	}
	odm_set_rf_reg(dm, RF_PATH_A, RF_0x18, RFREG_MASK, rf_reg18);
	odm_set_rf_reg(dm, RF_PATH_B, RF_0x18, RFREG_MASK, rf_reg18);
	/*====================================================================*/
	//NBI Auto detect for spur channel
#if 0
	if (central_ch == 153 || central_ch == 155 || central_ch == 106) {
		if (dm->rfe_type != 1 || dm->rfe_type != 3) {
			odm_set_bb_reg(dm, R_0x818, 0x8, 0x1);
			odm_set_bb_reg(dm, R_0x82c, 0xf, 0xb);
		}
	} else {
		// Turn NBI auto detect off
		odm_set_bb_reg(dm, R_0x818, 0x8, 0x0);
		odm_set_bb_reg(dm, R_0x1d3c, BIT(27) | BIT(28), 0x0);
		odm_set_bb_reg(dm, R_0x1d3c, BIT(27) | BIT(28), 0x3);
		odm_set_bb_reg(dm, R_0x82c, 0xf, 0xd);
	}
#endif
	if (*dm->mp_mode) {
		dm->is_psd_in_process = 1;
		phydm_dynamic_spur_det_eliminate_8812f(dm);
		dm->is_psd_in_process = 0;
	}
	phydm_igi_toggle_8812f(dm);
	phydm_bb_reset_8812f(dm);

	PHYDM_DBG(dm, ODM_PHY_CONFIG, "Switch CH:%d success\n", central_ch);
	return true;
}

__odm_func__
boolean
config_phydm_switch_bandwidth_8812f(struct dm_struct *dm, u8 pri_ch,
				    enum channel_width bw)
{
	struct phydm_dig_struct *dig_tab = &dm->dm_dig_table;
	u32 rf_reg18 = 0;
	boolean rf_reg_status = true;

	PHYDM_DBG(dm, ODM_PHY_CONFIG, "%s ======>\n", __func__);

	if (dm->is_disable_phy_api) {
		PHYDM_DBG(dm, ODM_PHY_CONFIG, "Disable PHY API for debug!!\n");
		return true;
	}

	/* @Error handling */
	if (bw >= CHANNEL_WIDTH_MAX || (bw == CHANNEL_WIDTH_40 && pri_ch > 2) ||
	    (bw == CHANNEL_WIDTH_80 && pri_ch > 4)) {
		PHYDM_DBG(dm, ODM_PHY_CONFIG,
			  "Fail to switch bw(bw:%d, pri ch:%d)\n", bw, pri_ch);
		return false;
	}

	bw_8812f = bw;
	rf_reg18 = config_phydm_read_rf_reg_8812f(dm, RF_PATH_A, RF_0x18,
						  RFREG_MASK);
	if (rf_reg18 != INVALID_RF_DATA)
		rf_reg_status = true;
	else
		rf_reg_status = false;

	rf_reg18 &= ~(BIT(13) | BIT(12));

	/* @Switch bandwidth */
	switch (bw) {
	case CHANNEL_WIDTH_5:
	case CHANNEL_WIDTH_10:
	case CHANNEL_WIDTH_20:
		if (bw == CHANNEL_WIDTH_5) {
			/* @RX DFIR*/
			odm_set_bb_reg(dm, R_0x810, 0x3ff0, 0x2ab);

			/* @small BW:[7:6]=0x1 */
			/* @TX pri ch:[11:8]=0x0, RX pri ch:[15:12]=0x0 */
			odm_set_bb_reg(dm, R_0x9b0, 0xffc0, 0x1);

			/* @DAC clock = 120M clock for BW5 */
			odm_set_bb_reg(dm, R_0x9b4, 0x00000700, 0x2);

			/* @ADC clock = 40M clock for BW5 */
			odm_set_bb_reg(dm, R_0x9b4, 0x00700000, 0x4);
		} else if (bw == CHANNEL_WIDTH_10) {
			/* @RX DFIR*/
			odm_set_bb_reg(dm, R_0x810, 0x3ff0, 0x2ab);

			/* @small BW:[7:6]=0x2 */
			/* @TX pri ch:[11:8]=0x0, RX pri ch:[15:12]=0x0 */
			odm_set_bb_reg(dm, R_0x9b0, 0xffc0, 0x2);

			/* @DAC clock = 240M clock for BW10 */
			odm_set_bb_reg(dm, R_0x9b4, 0x00000700, 0x4);

			/* @ADC clock = 80M clock for BW10 */
			odm_set_bb_reg(dm, R_0x9b4, 0x00700000, 0x5);
		} else if (bw == CHANNEL_WIDTH_20) {
			/* @RX DFIR*/
			odm_set_bb_reg(dm, R_0x810, 0x3ff0, 0x19b);

			/* @small BW:[7:6]=0x0 */
			/* @TX pri ch:[11:8]=0x0, RX pri ch:[15:12]=0x0 */
			odm_set_bb_reg(dm, R_0x9b0, 0xffc0, 0x0);

			/* @DAC clock = 480M clock for BW20 */
			odm_set_bb_reg(dm, R_0x9b4, 0x00000700, 0x6);

			/* @ADC clock = 160M clock for BW20 */
			odm_set_bb_reg(dm, R_0x9b4, 0x00700000, 0x6);
		}

		/* @TX_RF_BW:[1:0]=0x0, RX_RF_BW:[3:2]=0x0 */
		odm_set_bb_reg(dm, R_0x9b0, 0xf, 0x0);

		/* @RF bandwidth */
		rf_reg18 |= (BIT(13) | BIT(12));

		/* @RF RXBB setting, modify 0x3f for WLANBB-1081*/
		odm_set_rf_reg(dm, RF_PATH_A, RF_0xee, 0x4, 0x1);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x33, 0x1F, 0x12);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x3f, RFREG_MASK, 0x18);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0xee, 0x4, 0x0);
		odm_set_rf_reg(dm, RF_PATH_B, RF_0xee, 0x4, 0x1);
		odm_set_rf_reg(dm, RF_PATH_B, RF_0x33, 0x1F, 0x12);
		odm_set_rf_reg(dm, RF_PATH_B, RF_0x3f, RFREG_MASK, 0x18);
		odm_set_rf_reg(dm, RF_PATH_B, RF_0xee, 0x4, 0x0);

		/* @pilot smoothing on */
		odm_set_bb_reg(dm, R_0xcbc, BIT(21), 0x0);
		/* subtune*/
		odm_set_bb_reg(dm, R_0x88c, 0xf000, 0x1);
		break;

	case CHANNEL_WIDTH_40:
		/* @CCK primary channel */
		if (pri_ch == 1)
			odm_set_bb_reg(dm, R_0x1a00, BIT(4), pri_ch);
		else
			odm_set_bb_reg(dm, R_0x1a00, BIT(4), 0);

		/* @TX_RF_BW:[1:0]=0x1, RX_RF_BW:[3:2]=0x1 */
		odm_set_bb_reg(dm, R_0x9b0, 0xf, 0x5);

		/* @small BW */
		odm_set_bb_reg(dm, R_0x9b0, 0xc0, 0x0);

		/* @TX pri ch:[11:8], RX pri ch:[15:12] */
		odm_set_bb_reg(dm, R_0x9b0, 0xff00, (pri_ch | (pri_ch << 4)));

		/* @RF bandwidth */
		rf_reg18 |= BIT(13);

		/* @RF RXBB setting, modify 0x3f for WLANBB-1081 */
		odm_set_rf_reg(dm, RF_PATH_A, RF_0xee, 0x4, 0x1);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x33, 0x1F, 0x12);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x3f, RFREG_MASK, 0x10);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0xee, 0x4, 0x0);

		odm_set_rf_reg(dm, RF_PATH_B, RF_0xee, 0x4, 0x1);
		odm_set_rf_reg(dm, RF_PATH_B, RF_0x33, 0x1F, 0x12);
		odm_set_rf_reg(dm, RF_PATH_B, RF_0x3f, RFREG_MASK, 0x10);
		odm_set_rf_reg(dm, RF_PATH_B, RF_0xee, 0x4, 0x0);

		/* @pilot smoothing off */
		odm_set_bb_reg(dm, R_0xcbc, BIT(21), 0x1);

		/* subtune*/
		odm_set_bb_reg(dm, R_0x88c, 0xf000, 0x1);
		break;

	case CHANNEL_WIDTH_80:
		/* @TX_RF_BW:[1:0]=0x2, RX_RF_BW:[3:2]=0x2 */
		odm_set_bb_reg(dm, R_0x9b0, 0xf, 0xa);
		odm_set_bb_reg(dm, R_0x9b0, 0xc0, 0x0); /* small BW */
		/* @TX pri ch:[11:8], RX pri ch:[15:12] */
		odm_set_bb_reg(dm, R_0x9b0, 0xff00, (pri_ch | (pri_ch << 4)));

		/* @RF bandwidth */
		rf_reg18 |= BIT(12);

		/* @RF RXBB setting, modify 0x3f for WLANBB-1081 */
		odm_set_rf_reg(dm, RF_PATH_A, RF_0xee, 0x4, 0x1);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x33, 0x1F, 0x12);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0x3f, RFREG_MASK, 0x8);
		odm_set_rf_reg(dm, RF_PATH_A, RF_0xee, 0x4, 0x0);

		odm_set_rf_reg(dm, RF_PATH_B, RF_0xee, 0x4, 0x1);
		odm_set_rf_reg(dm, RF_PATH_B, RF_0x33, 0x1F, 0x12);
		odm_set_rf_reg(dm, RF_PATH_B, RF_0x3f, RFREG_MASK, 0x8);
		odm_set_rf_reg(dm, RF_PATH_B, RF_0xee, 0x4, 0x0);

		/* @pilot smoothing off */
		odm_set_bb_reg(dm, R_0xcbc, BIT(21), 0x1);
		/* subtune*/
		odm_set_bb_reg(dm, R_0x88c, 0xf000, 0x6);

		break;

	default:
		PHYDM_DBG(dm, ODM_PHY_CONFIG,
			  "Fail to switch bw (bw:%d, pri ch:%d)\n", bw, pri_ch);
	}

	/* @Write RF register */
	odm_set_rf_reg(dm, RF_PATH_A, RF_0x18, RFREG_MASK, rf_reg18);
	odm_set_rf_reg(dm, RF_PATH_B, RF_0x18, RFREG_MASK, rf_reg18);

	if (!rf_reg_status) {
		PHYDM_DBG(dm, ODM_PHY_CONFIG,
			  "Fail to switch bw (bw:%d, primary ch:%d), because writing RF register is fail\n",
			  bw, pri_ch);
		return false;
	}

	/* @Toggle IGI to let RF enter RX mode */
	phydm_igi_toggle_8812f(dm);

	PHYDM_DBG(dm, ODM_PHY_CONFIG,
		  "Success to switch bw (bw:%d, pri ch:%d)\n", bw, pri_ch);

	phydm_bb_reset_8812f(dm);
	return true;
}

__odm_func__
boolean
config_phydm_switch_channel_bw_8812f(struct dm_struct *dm, u8 central_ch,
				     u8 primary_ch_idx,
				     enum channel_width bandwidth)
{
	/* @Switch channel */
	if (!config_phydm_switch_channel_8812f(dm, central_ch))
		return false;

	/* @Switch bandwidth */
	if (!config_phydm_switch_bandwidth_8812f(dm, primary_ch_idx, bandwidth))
		return false;

	return true;
}

__odm_func__
void phydm_i_only_setting_8812f(struct dm_struct *dm, boolean en_i_only,
				boolean en_before_cca)
{
	if (en_i_only) { /*@ Set path-a*/
		if (en_before_cca) {
			odm_set_bb_reg(dm, R_0x1800, 0xfff00, 0x833);
			odm_set_bb_reg(dm, R_0x1c68, 0xc000, 0x2);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70038001);
		} else {
			if (!(*dm->band_width == CHANNEL_WIDTH_40))
				return;

			bp_0x9b0_8812f = odm_get_bb_reg(dm, R_0x9b0, MASKDWORD);
			odm_set_bb_reg(dm, R_0x1800, 0xfff00, 0x888);
			odm_set_bb_reg(dm, R_0x898, BIT(30), 0x1);
			odm_set_bb_reg(dm, R_0x1c68, 0xc000, 0x1);
			odm_set_bb_reg(dm, R_0x9b0, MASKDWORD, 0x2200);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70038001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70038001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70538001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70738001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70838001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70938001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70a38001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70b38001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70c38001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70d38001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70e38001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70f38001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70f38001);
		}
	} else {
		if (en_before_cca) {
			odm_set_bb_reg(dm, R_0x1800, 0xfff00, 0x333);
			odm_set_bb_reg(dm, R_0x1c68, 0xc000, 0x0);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x700b8001);
		} else {
			if (!(*dm->band_width == CHANNEL_WIDTH_40))
				return;

			odm_set_bb_reg(dm, R_0x1800, 0xfff00, 0x333);
			odm_set_bb_reg(dm, R_0x898, BIT(30), 0x0);
			odm_set_bb_reg(dm, R_0x1c68, 0xc000, 0x0);
			odm_set_bb_reg(dm, R_0x9b0, MASKDWORD, bp_0x9b0_8812f);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x700b8001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x700b8001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x705b8001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x707b8001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x708b8001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x709b8001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70ab8001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70bb8001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70cb8001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70db8001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70eb8001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70fb8001);
			odm_set_bb_reg(dm, R_0x1830, MASKDWORD, 0x70fb8001);
		}
	}
}

__odm_func__
void phydm_1rcca_setting_8812f(struct dm_struct *dm, boolean en_1rcca)
{
	if (en_1rcca) { /*@ Set path-a*/
		odm_set_bb_reg(dm, R_0x83c, 0x4, 0x1);
		odm_set_bb_reg(dm, R_0x824, 0x0f000000, 0x1);
		odm_set_bb_reg(dm, R_0x4100, 0xf0000, 0x1);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x70008001);

	} else {
		odm_set_bb_reg(dm, R_0x83c, 0x4, 0x0);
		odm_set_bb_reg(dm, R_0x824, 0x0f000000, 0x3);
		odm_set_bb_reg(dm, R_0x4100, 0xf0000, 0x3);
		odm_set_bb_reg(dm, R_0x4130, MASKDWORD, 0x700b8001);

	}
	phydm_bb_reset_8812f(dm);
}

__odm_func__
void phydm_ch_smooth_setting_8812f(struct dm_struct *dm, boolean en_ch_smooth)
{
	if (en_ch_smooth)
		/* @enable force channel smoothing*/
		odm_set_bb_reg(dm, R_0xc54, BIT(7), 0x1);
	else
		odm_set_bb_reg(dm, R_0xc54, BIT(7), 0x0);
}

__odm_func__
boolean
config_phydm_parameter_init_8812f(struct dm_struct *dm,
				  enum odm_parameter_init type)
{
	PHYDM_DBG(dm, ODM_PHY_CONFIG, "%s ======>\n", __func__);

	if (*dm->mp_mode)
		phydm_ch_smooth_setting_8812f(dm, true);

	/* @Do not use PHYDM API to read/write because FW can not access */
	/* @Turn on 3-wire*/
	odm_set_bb_reg(dm, R_0x180c, 0x3, 0x3);
	odm_set_bb_reg(dm, R_0x180c, BIT(28), 0x1);
	odm_set_bb_reg(dm, R_0x410c, 0x3, 0x3);
	odm_set_bb_reg(dm, R_0x410c, BIT(28), 0x1);

	if (type == ODM_PRE_SETTING) {
		odm_set_bb_reg(dm, R_0x1c3c, (BIT(0) | BIT(1)), 0x0);
		PHYDM_DBG(dm, ODM_PHY_CONFIG,
			  "Pre setting: disable OFDM and CCK block\n");
	} else if (type == ODM_POST_SETTING) {
		odm_set_bb_reg(dm, R_0x1c3c, (BIT(0) | BIT(1)), 0x3);
		PHYDM_DBG(dm, ODM_PHY_CONFIG,
			  "Post setting: enable OFDM and CCK block\n");
#if (PHYDM_FW_API_FUNC_ENABLE_8812F)
	} else if (type == ODM_INIT_FW_SETTING) {
		u8 h2c_content[4] = {0};

		h2c_content[0] = dm->rfe_type;
		h2c_content[1] = dm->rf_type;
		h2c_content[2] = dm->cut_version;
		h2c_content[3] = (dm->tx_ant_status << 4) | dm->rx_ant_status;

		odm_fill_h2c_cmd(dm, PHYDM_H2C_FW_GENERAL_INIT, 4, h2c_content);
#endif
	} else {
		PHYDM_DBG(dm, ODM_PHY_CONFIG, "Wrong type!!\n");
		return false;
	}

	phydm_bb_reset_8812f(dm);

	return true;
}

#if CONFIG_POWERSAVING
__odm_func_aon__
boolean
phydm_rfe_8812f_lps(struct dm_struct *dm, boolean enable_sw_rfe)
{
	//u8 rfe_type = dm->rfe_type;
	u32 rf_reg18_ch = 0;

	rf_reg18_ch = config_phydm_read_rf_reg_8812f(dm, RF_PATH_A, RF_0x18,
						     0xff);

	/* HW Setting for each RFE type */
	#if 0
	if (rfe_type == 4) {
		if (rf_reg18_ch <= 14) {
			/* signal source */
			if (!enable_sw_rfe) {
				odm_set_bb_reg(dm, R_0x1840, 0xffffff,
					       0x745774);
				odm_set_bb_reg(dm, R_0x4140, 0xffffff,
					       0x745774);
				odm_set_bb_reg(dm, R_0x1844, MASKBYTE1, 0x57);
				odm_set_bb_reg(dm, R_0x4144, MASKBYTE1, 0x57);
			} else {
				odm_set_bb_reg(dm, R_0x1840, 0xffffff,
					       0x777777);
				odm_set_bb_reg(dm, R_0x4140, 0xffffff,
					       0x777777);
				odm_set_bb_reg(dm, R_0x1844, MASKBYTE1, 0x77);
				odm_set_bb_reg(dm, R_0x4144, MASKBYTE1, 0x77);
			}
		} else if (rf_reg18_ch > 35) {
			/* signal source */
			if (!enable_sw_rfe) {
				odm_set_bb_reg(dm, R_0x1840, 0xffffff,
					       0x477547);
				odm_set_bb_reg(dm, R_0x4140, 0xffffff,
					       0x477547);
				odm_set_bb_reg(dm, R_0x1844, MASKBYTE1, 0x75);
				odm_set_bb_reg(dm, R_0x4144, MASKBYTE1, 0x75);
			} else {
				odm_set_bb_reg(dm, R_0x1840, 0xffffff,
					       0x777777);
				odm_set_bb_reg(dm, R_0x4140, 0xffffff,
					       0x777777);
				odm_set_bb_reg(dm, R_0x1844, MASKBYTE1, 0x77);
				odm_set_bb_reg(dm, R_0x4144, MASKBYTE1, 0x77);
			}
		} else {
			return false;
		}
	} else if ((rfe_type == 1) || (rfe_type == 2) || (rfe_type == 6) ||
		   (rfe_type == 7) || (rfe_type == 9) || (rfe_type == 11)) {
		/* eFem */
		if (rf_reg18_ch <= 14) {
			/* signal source */
			if (!enable_sw_rfe) {
				odm_set_bb_reg(dm, R_0x1840, 0xffffff,
					       0x705770);
				odm_set_bb_reg(dm, R_0x4140, 0xffffff,
					       0x705770);
				odm_set_bb_reg(dm, R_0x1844, MASKBYTE1, 0x57);
				odm_set_bb_reg(dm, R_0x4144, MASKBYTE1, 0x57);
			} else {
				odm_set_bb_reg(dm, R_0x1840, 0xffffff,
					       0x777777);
				odm_set_bb_reg(dm, R_0x4140, 0xffffff,
					       0x777777);
				odm_set_bb_reg(dm, R_0x1844, MASKBYTE1, 0x77);
				odm_set_bb_reg(dm, R_0x4144, MASKBYTE1, 0x77);
			}
		} else if (rf_reg18_ch > 35) {
			/* signal source */
			if (!enable_sw_rfe) {
				odm_set_bb_reg(dm, R_0x1840, 0xffffff,
					       0x177517);
				odm_set_bb_reg(dm, R_0x4140, 0xffffff,
					       0x177517);
				odm_set_bb_reg(dm, R_0x1844, MASKBYTE1, 0x75);
				odm_set_bb_reg(dm, R_0x4144, MASKBYTE1, 0x75);
			} else {
				odm_set_bb_reg(dm, R_0x1840, 0xffffff,
					       0x777777);
				odm_set_bb_reg(dm, R_0x4140, 0xffffff,
					       0x777777);
				odm_set_bb_reg(dm, R_0x1844, MASKBYTE1, 0x77);
				odm_set_bb_reg(dm, R_0x4144, MASKBYTE1, 0x77);
			}
		} else {
			return false;
		}
	} else {
		return true;
	}
	#endif
	return true;
}

__odm_func_aon__
boolean
phydm_8812f_lps(struct dm_struct *dm, boolean enable_lps)
{
	u16 poll_cnt = 0;
	u32 bbtemp = 0;

	if (enable_lps == _TRUE) {
		/* backup RF reg0x0 */
		SysMib.Wlan.PS.PSParm.RxGainPathA = (u16)(config_phydm_read_rf_reg_8812f(dm, RF_PATH_A, RF_0x00, RFREG_MASK));
		SysMib.Wlan.PS.PSParm.RxGainPathB = (u16)(config_phydm_read_rf_reg_8812f(dm, RF_PATH_B, RF_0x00, RFREG_MASK));

		/* turn off TRx HSSI: 0x180c[1:0]=2'b00, 0x410c[1:0]=2'b00 */
		bbtemp = odm_get_bb_reg(dm, R_0x180c, MASKDWORD) & 0xfffffffc;
		odm_set_bb_reg(dm, R_0x180c, MASKDWORD, bbtemp);
		bbtemp = odm_get_bb_reg(dm, R_0x410c, MASKDWORD) & 0xfffffffc;
		odm_set_bb_reg(dm, R_0x410c, MASKDWORD, bbtemp);

		/* Set RF enter shutdown mode */
		config_phydm_write_rf_reg_8812f(dm, RF_PATH_A, RF_0x00,
						RFREG_MASK, 0);
		config_phydm_write_rf_reg_8812f(dm, RF_PATH_B, RF_0x00,
						RFREG_MASK, 0);

		/* if eFEM, RFE control for signal source = 0 */
		phydm_rfe_8812f_lps(dm, _TRUE);

		/* Check BB state is idle, do not check GNT_WL only for LPS */
		while (1) {
			odm_set_bb_reg(dm, R_0x1c3c, 0x00f00000, 0x0);
			bbtemp = odm_get_bb_reg(dm, R_0x2db4, MASKDWORD);
			if ((bbtemp & 0x1FFEFF3F) == 0 &&
			    (bbtemp & 0xC0000000) == 0xC0000000)
				break;

			if (poll_cnt > WAIT_TXSM_STABLE_CNT) {
				WriteMACRegDWord(REG_DBG_DW_FW_ERR, ReadMACRegDWord(REG_DBG_DW_FW_ERR) | FES_BBSTATE_IDLE);
			/* SysMib.Wlan.DbgPort.DbgInfoParm.u4ErrFlag[0] |= FES_BBSTATE_IDLE; */
				return _FALSE;
			}

			DelayUS(WAIT_TXSM_STABLE_ONCE_TIME);
			poll_cnt++;
		}

		/* disable CCK and OFDM module */
		WriteMACRegByte(REG_SYS_FUNC_EN, ReadMACRegByte(REG_SYS_FUNC_EN)
				& ~BIT_FEN_BBRSTB);

		if (poll_cnt < WAIT_TXSM_STABLE_CNT) {
			/* Gated BBclk 0x1c24[0] = 1 */
			bbtemp = odm_get_bb_reg(dm, R_0x1c24, MASKDWORD) |
				 0x00000001;
			odm_set_bb_reg(dm, R_0x1c24, MASKDWORD, bbtemp);
		}

		return _TRUE;
	} else {
		/* release BB clk 0x1c24[0] = 0 */
		bbtemp = odm_get_bb_reg(dm, R_0x1c24, MASKDWORD) &
			 (~0x00000001);
		odm_set_bb_reg(dm, R_0x1c24, MASKDWORD, bbtemp);

		PwrGatedRestoreBB();

		/* Enable CCK and OFDM module, */
		/* should be a delay large than 200ns before RF access */
		WriteMACRegByte(REG_SYS_FUNC_EN, ReadMACRegByte(REG_SYS_FUNC_EN)
				| BIT_FEN_BBRSTB);
		DelayUS(1);

		/* if eFEM, restore RFE control signal */
		phydm_rfe_8812f_lps(dm, _FALSE);

		/* Set RF enter active mode */
		config_phydm_write_rf_reg_8812f(dm, RF_PATH_A, R_0x00, RFREG_MASK, (0x30000 | SysMib.Wlan.PS.PSParm.RxGainPathA));
		config_phydm_write_rf_reg_8812f(dm, RF_PATH_B, R_0x00, RFREG_MASK, (0x30000 | SysMib.Wlan.PS.PSParm.RxGainPathB));

		/* turn on TRx HSSI: 0x180c[1:0]=2'b11, 0x410c[1:0]=2'b11 */
		bbtemp = odm_get_bb_reg(dm, R_0x180c, MASKDWORD) | 0x00000003;
		odm_set_bb_reg(dm, R_0x180c, MASKDWORD, bbtemp);
		bbtemp = odm_get_bb_reg(dm, R_0x410c, MASKDWORD) | 0x00000003;
		odm_set_bb_reg(dm, R_0x410c, MASKDWORD, bbtemp);

		return _TRUE;
	}
}
#endif /* #if CONFIG_POWERSAVING */

/* ======================================================================== */
#endif /* PHYDM_FW_API_ENABLE_8812F */
#endif /* RTL8812F_SUPPORT */
