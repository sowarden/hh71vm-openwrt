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

#if 0
	/* ============================================================
	*  include files
	* ============================================================ */
#endif

#include "mp_precomp.h"
#include "../phydm_precomp.h"

#if (RTL8197F_SUPPORT == 1)

s8 odm_cckrssi_8197f(
	struct dm_struct *dm,
	u8 lna_idx,
	u8 vga_idx)
{
	s8 rx_pwr_all;
	s8 diff_para;

	if ((dm->board_type == (ODM_BOARD_EXT_TRSW | ODM_BOARD_EXT_LNA | ODM_BOARD_EXT_PA)) && dm->package_type == 1)
		diff_para = -7;
	else
		diff_para = 7;

	switch (lna_idx) {
	case 7:
		rx_pwr_all = -52 + diff_para - 2 * (vga_idx);
		break;

	case 6:
		rx_pwr_all = -42 + diff_para - 2 * (vga_idx);
		break;

	case 5:
		rx_pwr_all = -32 + diff_para - 2 * (vga_idx);
		break;

	case 4:
		rx_pwr_all = -26 + diff_para - 2 * (vga_idx);
		break;

	case 3:
		rx_pwr_all = -18 + diff_para - 2 * (vga_idx);
		break;

	case 2:
		rx_pwr_all = -12 + diff_para - 2 * (vga_idx);
		break;

	case 1:
		rx_pwr_all = -2 + diff_para - 2 * (vga_idx);
		break;

	case 0:
		rx_pwr_all = 2 + diff_para - 2 * (vga_idx);
		break;

	default:
		break;
	}

	return rx_pwr_all;
}

void phydm_phypara_a_cut(
	struct dm_struct *dm)
{
	odm_set_bb_reg(dm, R_0x97c, 0xff000000, 0x20); /*97f A-cut workaround*/

	odm_set_bb_reg(dm, R_0xa9c, BIT(17), 0); /*97f A-cut workaround*/
	odm_set_bb_reg(dm, R_0xa0c, MASKBYTE2, 0x7e); /*97f A-cut workaround*/
	odm_set_bb_reg(dm, R_0xa0c, MASKBYTE1, 0x0); /*97f A-cut workaround*/
	odm_set_bb_reg(dm, R_0xa84, MASKBYTE2, 0x1f); /*97f A-cut workaround*/

	odm_set_bb_reg(dm, R_0x824, BIT(14), 0x1); /*pathA r_rxhp_tx*/
	odm_set_bb_reg(dm, R_0x824, BIT(17), 0x1); /*pathA r_rxhp_t2r*/
	odm_set_bb_reg(dm, R_0x82c, BIT(14), 0x1); /*pathB r_rxhp_tx*/
	odm_set_bb_reg(dm, R_0x82c, BIT(17), 0x1); /*pathB r_rxhp_t2r*/
	odm_set_bb_reg(dm, R_0xc5c, BIT(2), 0x1); /*97f A-cut workaround*/
	odm_set_bb_reg(dm, R_0xc5c, BIT(5), 0x1); /*97f A-cut workaround*/
	odm_set_bb_reg(dm, R_0xc5c, BIT(8), 0x1); /*97f A-cut workaround*/
	odm_set_bb_reg(dm, R_0xc5c, BIT(11), 0x1); /*97f A-cut workaround*/
	odm_set_bb_reg(dm, R_0xc5c, BIT(14), 0x1); /*97f A-cut workaround*/
	odm_set_bb_reg(dm, R_0xce0, BIT(2), 0x1); /*97f A-cut workaround*/
	odm_set_bb_reg(dm, R_0xc7c, BIT(24), 0x1); /*97f A-cut workaround*/
	odm_set_bb_reg(dm, R_0xc7c, BIT(27), 0x1); /*97f A-cut workaround*/
}

void phydm_dynamic_disable_ecs(
	struct dm_struct *dm)
{
	struct phydm_fa_struct *false_alm_cnt = (struct phydm_fa_struct *)phydm_get_structure(dm, PHYDM_FALSEALMCNT);

	if (dm->is_disable_dym_ecs == true || (*dm->mp_mode == true)) /*use mib to disable this dym function*/
		return;

	if (dm->rssi_min < 30 || (false_alm_cnt->cnt_all * 4 >= false_alm_cnt->cnt_cca_all))
		odm_set_bb_reg(dm, R_0x9ac, BIT(17), 0);
	else if ((dm->rssi_min >= 34) && (false_alm_cnt->cnt_all * 5 <= false_alm_cnt->cnt_cca_all))
		odm_set_bb_reg(dm, R_0x9ac, BIT(17), 1);
}

#ifdef DYN_ANT_WEIGHTING_SUPPORT
void phydm_dynamic_ant_weighting_8197f(
	void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	u8 rssi_l2h = 43, rssi_h2l = 37;

	if (dm->is_disable_dym_ant_weighting)
		return;

	/* force AGC weighting */
	odm_set_bb_reg(dm, R_0xc54, BIT(0), 1);
	/* MRC by AGC table */
	odm_set_bb_reg(dm, R_0xce8, BIT(30), 1);
	/* Enable antenna_weighting_shift mechanism */
	odm_set_bb_reg(dm, R_0xd5c, BIT(29), 1);

	if (dm->rssi_min_by_path != 0xFF) {
		if (dm->rssi_min_by_path >= rssi_l2h) {
			odm_set_bb_reg(dm, R_0xd5c, (BIT(31) | BIT(30)), 0); /*equal weighting*/
		} else if (dm->rssi_min_by_path <= rssi_h2l) {
			odm_set_bb_reg(dm, R_0xd5c, (BIT(31) | BIT(30)), 1); /*fix sec_min_wgt = 1/2*/
		}
	} else {
		odm_set_bb_reg(dm, R_0xd5c, (BIT(31) | BIT(30)), 1); /*fix sec_min_wgt = 1/2*/
	}
}
#endif

phydm_hwsetting_8197f(
	struct dm_struct *dm)
{
	phydm_dynamic_disable_ecs(dm);
	phydm_dynamic_ant_weighting(dm);
}

#endif /* RTL8197F_SUPPORT == 1 */
