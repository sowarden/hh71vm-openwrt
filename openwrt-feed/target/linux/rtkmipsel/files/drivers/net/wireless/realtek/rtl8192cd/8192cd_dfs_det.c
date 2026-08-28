/*
 *  Handling routines for DFS DET functions
 *
 *  Copyright (c) 2017 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _8192CD_DFS_DET_C_

#include "./8192cd_cfg.h"
#include "./8192cd.h"
#include "./8192cd_headers.h"
#include "./8192cd_debug.h"


#if defined(DFS) && !defined(CONFIG_RTL_92D_SUPPORT)
extern u2Byte db_invert_table[8][12];

static u4Byte 
ConvertTo_dB(
	u4Byte 	Value)
{
	u1Byte i;
	u1Byte j;
	u4Byte dB;

	Value = Value & 0xFFFF;
	
	for (i=0;i<8;i++)
	{
		if (Value <= db_invert_table[i][11])
		{
			break;
		}
	}

	if (i >= 8)
	{
		return (96);	// maximum 96 dB
	}

	for (j=0;j<12;j++)
	{
		if (Value <= db_invert_table[i][j])
		{
			break;
		}
	}

	dB = i*12 + j + 1;

	return (dB);
}


static int
GetPSDData_8812(
	IN struct dm_struct*	p_dm_odm,
	unsigned int 	point,
	u1Byte initial_gain_psd)
{
	int	psd_report;
	struct rtl8192cd_priv *priv=p_dm_odm->priv;
	
	//Set DCO frequency index, offset=(40MHz/SamplePts)*point
	odm_set_bb_reg(p_dm_odm, 0x910, 0x3FF, point);

	//Start PSD calculation, Reg808[22]=0->1
	odm_set_bb_reg(p_dm_odm, 0x910, BIT22, 1);

	//Need to wait for HW PSD report
	delay_us(priv->pshare->rf_ft_var.dfs_psd_delay);

	odm_set_bb_reg(p_dm_odm, 0x910, BIT22, 0);

	//Read PSD report, Reg8B4[15:0]
	psd_report = (int)odm_get_bb_reg(p_dm_odm,0xf44, bMaskDWord) & 0x0000FFFF;
	if(priv->pshare->rf_ft_var.psd_skip_lookup_table){
		if(psd_report >=14)
			psd_report = 23;
		else
			psd_report = 8;
	}
	else{
		psd_report = (int)(ConvertTo_dB((u4Byte)psd_report));		
	}

	return psd_report;
	
}


void Scan_BB_PSD(
	IN struct dm_struct*	p_dm_odm,
	int *PSD_report_right,
	int *PSD_report_left,
	int len,
	int initial_gain)
{
	struct rtl8192cd_priv *priv=p_dm_odm->priv;
	struct phydm_dig_struct*  p_dm_dig_table = &p_dm_odm->dm_dig_table;
	u1Byte  ST_TH_origin;
	u1Byte	idx[20]={//96,99,102,106,109,112,115,118,122,125,
	                     224,227,230,234,237,240,243,246,250,253,
					0,3,6,10,13,16,19,22,26,29};
	int tone_idx, channel_org, channel, i, BW_org;

	// set DFS ST_TH to max value
	ST_TH_origin = RTL_R8(0x91c);
	RTL_W8(0x91c, 0x4e);
	
	// Turn off CCK
	odm_set_bb_reg(p_dm_odm, 0x808, BIT28, 0);   //808[28]

	// Turn off TX
	// Pause TX Queue
	if (!priv->pmib->dot11DFSEntry.disable_tx)
		odm_write_1byte(p_dm_odm, 0x522, 0xFF); //REG_TXPAUSE 改為0x522

	// Turn off CCA
	if((GET_CHIP_VER(priv) == VERSION_8814A) || (GET_CHIP_VER(priv) == VERSION_8822B)){
		odm_set_bb_reg(p_dm_odm, 0x838, BIT1, 0x1); //838[1] 設為1
	}
	else{
		odm_set_bb_reg(p_dm_odm, 0x838, BIT3, 0x1); //838[3] 設為1
	}
	
	// PHYTXON while loop
	phy_set_bb_reg(priv, 0x8fc, 0xfff, 0);
	i = 0;
	while (odm_get_bb_reg(p_dm_odm, 0xfa0, BIT18)) {
		i++;
		if (i > 1000000) {
			panic_printk("Wait in %s() more than %d times!\n", __FUNCTION__, i);
			break;
		}
	}

	// backup IGI_origin , set IGI = 0x3e;
	//p_dm_dig_table->is_psd_in_progress = TRUE;
	odm_pause_dig(p_dm_odm, PHYDM_PAUSE, PHYDM_PAUSE_LEVEL_2, initial_gain);

	// Turn off 3-wire
	odm_set_bb_reg(p_dm_odm, 0xC00, BIT1|BIT0, 0x0); //c00[1:0] 寫0

	// pts value = 128, 256, 512, 1024
	odm_set_bb_reg(p_dm_odm, 0x910, BIT14|BIT15, 0x1); //910[15:14]設為1, 用256點
	odm_set_bb_reg(p_dm_odm, 0x910, BIT12|BIT13, 0x1); //910[13:12]設為1, avg 8 次

	// scan in-band PSD
	channel_org = odm_get_rf_reg(p_dm_odm, RF_PATH_A, RF_CHNLBW, 0x3FF);
	BW_org = priv->pshare->CurrentChannelBW;
	if(priv->pshare->CurrentChannelBW != CHANNEL_WIDTH_20){
		priv->pshare->No_RF_Write = 0;
		SwBWMode(priv, CHANNEL_WIDTH_20, 0);
		priv->pshare->No_RF_Write = 1;
	}
	
	if (priv->pshare->rf_ft_var.dfs_scan_inband) {
		int PSD_report_inband[20];
		for (tone_idx=0;tone_idx<len;tone_idx++)
			PSD_report_inband[tone_idx] = GetPSDData_8812(p_dm_odm, idx[tone_idx], initial_gain);
		panic_printk("PSD inband: ");
		for (i=0; i<len; i++)
			panic_printk("%d ", PSD_report_inband[i]);
		panic_printk("\n");
	}

	// scan right(higher) neighbor channel
	if (priv->pshare->CurrentChannelBW == CHANNEL_WIDTH_20)
		channel = channel_org + 4;
	else if (priv->pshare->CurrentChannelBW == CHANNEL_WIDTH_40)
		channel = channel_org + 6;
	else
		channel = channel_org + 10;
	delay_us(300);  // for idle 20M, it will  emit signal in right 20M channel
	priv->pshare->No_RF_Write = 0;
	odm_set_rf_reg(p_dm_odm, RF_PATH_A, RF_CHNLBW, 0x3FF, channel);
	priv->pshare->No_RF_Write = 1;

	for (tone_idx=0;tone_idx<len;tone_idx++)
		PSD_report_right[tone_idx] = GetPSDData_8812(p_dm_odm, idx[tone_idx], initial_gain);
	
	// scan left(lower) neighbor channel
	if (priv->pshare->CurrentChannelBW == CHANNEL_WIDTH_20)
		channel = channel_org - 4;
	else if (priv->pshare->CurrentChannelBW == CHANNEL_WIDTH_40)
		channel = channel_org - 6;
	else
		channel = channel_org - 10;
	priv->pshare->No_RF_Write = 0;
	odm_set_rf_reg(p_dm_odm, RF_PATH_A, RF_CHNLBW, 0x3FF, channel);
	priv->pshare->No_RF_Write = 1;

	for (tone_idx=0;tone_idx<len;tone_idx++)
		PSD_report_left[tone_idx] = GetPSDData_8812(p_dm_odm, idx[tone_idx], initial_gain);
	

	// restore originl center frequency
	if(BW_org != priv->pshare->CurrentChannelBW){
		priv->pshare->No_RF_Write = 0;
		SwBWMode(priv, BW_org, priv->pshare->offset_2nd_chan);
		priv->pshare->No_RF_Write = 1;
	}
	priv->pshare->No_RF_Write = 0;
	odm_set_rf_reg(p_dm_odm, RF_PATH_A, RF_CHNLBW, 0x3FF, channel_org);
	priv->pshare->No_RF_Write = 1;

	// Turn on 3-wire
	odm_set_bb_reg(p_dm_odm, 0xc00, BIT1|BIT0, 0x3); //c00[1:0] 寫3

	// Restore Current Settings
	// Resume DIG
	//p_dm_dig_table->is_psd_in_progress = FALSE;
	odm_pause_dig(p_dm_odm, PHYDM_RESUME, PHYDM_PAUSE_LEVEL_2, NONE);

	//Turn on CCA
	if((GET_CHIP_VER(priv) == VERSION_8814A) || (GET_CHIP_VER(priv) == VERSION_8822B)){
		odm_set_bb_reg(p_dm_odm, 0x838, BIT1, 0); //838[1] 設為0
	}
	else{
		odm_set_bb_reg(p_dm_odm, 0x838, BIT3, 0); //838[3] 設為0
	}
	

	// Turn on TX
	// Resume TX Queue
	if (!priv->pmib->dot11DFSEntry.disable_tx)
		odm_write_1byte(p_dm_odm, 0x522, 0x00); //REG_TXPAUSE 改為0x522

	// CCK on
	if (priv->pmib->dot11RFEntry.phyBandSelect == PHY_BAND_2G)
	odm_set_bb_reg(p_dm_odm, 0x808, BIT28, 1); //808[28]	
	
	// Resume DFS ST_TH
	RTL_W8(0x91c, ST_TH_origin);
}

#if defined(CONFIG_WLAN_HAL_8814BE)
void dfs_radar_distinguish_for_8814b(struct rtl8192cd_priv *priv, void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct ccx_info *ccx = &dm->dm_ccx_info;
	
	//syn0
	unsigned int reg_2e08, reg_2e0c, reg_2e10, reg_2e18, reg_2e00;
	//syn1
	unsigned int reg_2e28, reg_2e2c, reg_2e30, reg_2e20;
	unsigned int pw_rpt_14b_set[4]={0}, pri_rpt_14b;
	signed char loct_rpt_14b_set[8]={0};
	unsigned char i, j, dc_cnt = 0, short_cnt_th = 0, long_cnt_th = 0, loct_loop_cnt = 0, pw_loop_cnt = 0, radar_class_tmp = 0;
	unsigned int loct_diff_14b = 0, radar_class_num = 0, radar_class_cnt[10] = {0}, pw_diff_14b = 0;

	// for NHMxDFS //
	priv->nhm_dfs_cnt = 0;
	
	/*for MIB*/
	unsigned int pw_lowth_14b[10], pw_uppth_14b[10], pri_rpt_lowth_14b[10], pri_rpt_uppth_14b[10];
	unsigned int loct_diff_th_14b = 0, pw_diff_th_14b = 0, pw_cnt_th_14b = 0 ,cnt_th_14b = 0, pri_cnt_th_14b = 0, pri_max = 0 ,pri_min = 32767;

	//pw_cnt_th_14b = priv->pshare->rf_ft_var.pw_cnt_th;
	loct_diff_th_14b = priv->pshare->rf_ft_var.loct_diff_th;
	pw_diff_th_14b = priv->pshare->rf_ft_var.pw_diff_th;		
		
	/*read pw/pri/lctn report*/
	if (priv->seg1_flag == 1) {
		//panic_printk("\n SYN 1 \n");
		/*Wordlength : U(8,0) , unit: 400ns*/
		reg_2e28 = phy_query_bb_reg(priv, 0x2e28, 0xffffffff);
		pw_rpt_14b_set[0]=(unsigned int)(reg_2e28 & 0x000000ff);
		pw_rpt_14b_set[1]=(unsigned int)((reg_2e28 & 0x0000ff00)>>8);
		pw_rpt_14b_set[2]=(unsigned int)((reg_2e28 & 0x00ff0000)>>16);
		pw_rpt_14b_set[3]=(unsigned int)((reg_2e28 & 0xff000000)>>24);

		/* Wordlength : U(12,0) , unit: us */
		reg_2e18 = phy_query_bb_reg(priv, 0x2e18, 0xffffffff);
		pri_rpt_14b = (unsigned int)(reg_2e18 & 0x00000fff);
		
		/* Wordlength : S(8,0) , unit: MHz */
		reg_2e2c = phy_query_bb_reg(priv, 0x2e2c, 0xffffffff);
		loct_rpt_14b_set[0]=(signed char)(reg_2e2c & 0x000000ff);
		loct_rpt_14b_set[1]=(signed char)((reg_2e2c & 0x0000ff00)>>8);
		loct_rpt_14b_set[2]=(signed char)((reg_2e2c & 0x00ff0000)>>16);
		loct_rpt_14b_set[3]=(signed char)((reg_2e2c & 0xff000000)>>24);
		reg_2e30 = phy_query_bb_reg(priv, 0x2e30, 0xffffffff);
		loct_rpt_14b_set[4]=(signed char)(reg_2e30 & 0x000000ff);
		loct_rpt_14b_set[5]=(signed char)((reg_2e30 & 0x0000ff00)>>8);
		loct_rpt_14b_set[6]=(signed char)((reg_2e30 & 0x00ff0000)>>16);
		loct_rpt_14b_set[7]=(signed char)((reg_2e30 & 0xff000000)>>24);
	
		
	}
	else {
		//panic_printk("\n SYN 0 \n");
		/*Wordlength : U(8,0) , unit: 400ns*/
		reg_2e08 = phy_query_bb_reg(priv, 0x2e08, 0xffffffff);
		pw_rpt_14b_set[0]=(unsigned int)(reg_2e08 & 0x000000ff);
		pw_rpt_14b_set[1]=(unsigned int)((reg_2e08 & 0x0000ff00)>>8);
		pw_rpt_14b_set[2]=(unsigned int)((reg_2e08 & 0x00ff0000)>>16);
		pw_rpt_14b_set[3]=(unsigned int)((reg_2e08 & 0xff000000)>>24);

		/* Wordlength : U(12,0) , unit: us */
		reg_2e18 = phy_query_bb_reg(priv, 0x2e18, 0xffffffff);
		pri_rpt_14b = (unsigned int)((reg_2e18 & 0x0fff0000)>>16);
		
		/* Wordlength : S(8,0) , unit: MHz */
		reg_2e0c = phy_query_bb_reg(priv, 0x2e0c, 0xffffffff);
		loct_rpt_14b_set[0]=(signed char)(reg_2e0c & 0x000000ff);
		loct_rpt_14b_set[1]=(signed char)((reg_2e0c & 0x0000ff00)>>8);
		loct_rpt_14b_set[2]=(signed char)((reg_2e0c & 0x00ff0000)>>16);
		loct_rpt_14b_set[3]=(signed char)((reg_2e0c & 0xff000000)>>24);
		reg_2e10 = phy_query_bb_reg(priv, 0x2e10, 0xffffffff);
		loct_rpt_14b_set[4]=(signed char)(reg_2e10 & 0x000000ff);
		loct_rpt_14b_set[5]=(signed char)((reg_2e10 & 0x0000ff00)>>8);
		loct_rpt_14b_set[6]=(signed char)((reg_2e10 & 0x00ff0000)>>16);
		loct_rpt_14b_set[7]=(signed char)((reg_2e10 & 0xff000000)>>24);

		/* long_pulse_flag */
		reg_2e00 = phy_query_bb_reg(priv, 0x2e00, 0xffffffff);
		
	}

	priv->pulsewidth_flag = 0;
	priv->loct_flag = 0;
	priv->pulseinterval_flag = 0;
	// for NHMxDFS //
	priv->nhm_dfs_flag = 0;
	
	
	for (i = 0; i < 4; i++) {
		priv->pw_14b_rslt[i] = 0;
	}
	for (i = 0; i < 8; i++) {
		priv->loct_14b_rslt[i] = 0;
	}
	priv->pri_14b_rslt = 0;

	short_cnt_th = phy_query_bb_reg(priv, 0xa50, 0x00f00000);
	long_cnt_th = phy_query_bb_reg(priv, 0xa4c, 0xf0000000);

	if (priv->pshare->rf_ft_var.manual_dfs_regdomain == 3) {
		loct_loop_cnt = short_cnt_th + 2;
		pw_loop_cnt = short_cnt_th + 2;
	}
	else{
		if (long_cnt_th > short_cnt_th)
		{
			loct_loop_cnt = short_cnt_th  + 2;
			pw_loop_cnt = short_cnt_th + 2;
		}	
		else
		{
			loct_loop_cnt = long_cnt_th  + 2;
			pw_loop_cnt = long_cnt_th  + 2;
		}
	}

	if (loct_loop_cnt > 8)
		loct_loop_cnt = 8;
	if (pw_loop_cnt > 4)
		pw_loop_cnt = 4;

	if (priv->pshare->rf_ft_var.pw_pri_cnt_debug == 1)
		cnt_th_14b = priv->pshare->rf_ft_var.loop_cnt_th;
	else
		cnt_th_14b = pw_loop_cnt/2;

	/*Calculate the difference between each element (1st Not calculated)*/
	for(i = 0; i < loct_loop_cnt; i++)
		priv->loct_14b_rslt[i] = loct_rpt_14b_set[i];
	for(i = 1; i < loct_loop_cnt; i++)
		loct_diff_14b = loct_diff_14b + abs(priv->loct_14b_rslt[i] - priv->loct_14b_rslt[i-1]);
	/*The value may less than the average deviation, since the variable type is int (not float)*/

	for(i = 0; i < loct_loop_cnt; i++) {
		if (-2 < loct_rpt_14b_set[i] && loct_rpt_14b_set[i] < 2)
			dc_cnt++;
	}

	if (dc_cnt >= 3) 
		loct_diff_th_14b = 8;
	
	if (loct_diff_14b < loct_diff_th_14b)
		priv->loct_flag = 1;
	
	// Location Judgement (Bypass for long pulse flag)
	if (priv->radar_type == 1)
		priv->loct_flag = 1;	

	/*radar type classification*/
	priv->pri_14b_rslt = pri_rpt_14b;

	pw_lowth_14b[0] = priv->pshare->rf_ft_var.pw_lth1;
	pw_uppth_14b[0] = priv->pshare->rf_ft_var.pw_uth1;
	pw_lowth_14b[1] = priv->pshare->rf_ft_var.pw_lth2;
	pw_uppth_14b[1] = priv->pshare->rf_ft_var.pw_uth2;
	pw_lowth_14b[2] = priv->pshare->rf_ft_var.pw_lth3;
	pw_uppth_14b[2] = priv->pshare->rf_ft_var.pw_uth3;
	pw_lowth_14b[3] = priv->pshare->rf_ft_var.pw_lth4;
	pw_uppth_14b[3] = priv->pshare->rf_ft_var.pw_uth4;
	pw_lowth_14b[4] = priv->pshare->rf_ft_var.pw_lth5;
	pw_uppth_14b[4] = priv->pshare->rf_ft_var.pw_uth5;
	pw_lowth_14b[5] = priv->pshare->rf_ft_var.pw_lth6;
	pw_uppth_14b[5] = priv->pshare->rf_ft_var.pw_uth6;
	
		
	pri_rpt_lowth_14b[0] = priv->pshare->rf_ft_var.pri_lth1;
	pri_rpt_uppth_14b[0] = priv->pshare->rf_ft_var.pri_uth1;
	pri_rpt_lowth_14b[1] = priv->pshare->rf_ft_var.pri_lth2;
	pri_rpt_uppth_14b[1] = priv->pshare->rf_ft_var.pri_uth2;
	pri_rpt_lowth_14b[2] = priv->pshare->rf_ft_var.pri_lth3;
	pri_rpt_uppth_14b[2] = priv->pshare->rf_ft_var.pri_uth3;
	pri_rpt_lowth_14b[3] = priv->pshare->rf_ft_var.pri_lth4;
	pri_rpt_uppth_14b[3] = priv->pshare->rf_ft_var.pri_uth4;
	pri_rpt_lowth_14b[4] = priv->pshare->rf_ft_var.pri_lth5;
	pri_rpt_uppth_14b[4] = priv->pshare->rf_ft_var.pri_uth5;
	pri_rpt_lowth_14b[5] = priv->pshare->rf_ft_var.pri_lth6;
	pri_rpt_uppth_14b[5] = priv->pshare->rf_ft_var.pri_uth6;
	
	switch (priv->pshare->rf_ft_var.manual_dfs_regdomain) {
	case 1 :
		pw_lowth_14b[6] = priv->pshare->rf_ft_var.pw_lth7;
		pw_uppth_14b[6] = priv->pshare->rf_ft_var.pw_uth7;
		pri_rpt_lowth_14b[6] = priv->pshare->rf_ft_var.pri_lth7;
		pri_rpt_uppth_14b[6] = priv->pshare->rf_ft_var.pri_uth7;
		radar_class_num = 7;
		if (dc_cnt >= 3) {
			pw_lowth_14b[0] = 0;
			pw_lowth_14b[1] = 0;
			pw_lowth_14b[2] = 0;
			pw_lowth_14b[3] = 0;
			pw_lowth_14b[4] = 0;
			pw_lowth_14b[5] = 25;
			pw_lowth_14b[6] = 0;
		}
		break;
	case 2 :
		pw_lowth_14b[6] = priv->pshare->rf_ft_var.pw_lth7;
		pw_uppth_14b[6] = priv->pshare->rf_ft_var.pw_uth7;
		pw_lowth_14b[7] = priv->pshare->rf_ft_var.pw_lth8;
		pw_uppth_14b[7] = priv->pshare->rf_ft_var.pw_uth8;
		pri_rpt_lowth_14b[6] = priv->pshare->rf_ft_var.pri_lth7;
		pri_rpt_uppth_14b[6] = priv->pshare->rf_ft_var.pri_uth7;
		pri_rpt_lowth_14b[7] = priv->pshare->rf_ft_var.pri_lth8;
		pri_rpt_uppth_14b[7] = priv->pshare->rf_ft_var.pri_uth8;
		radar_class_num = 8;
		if (dc_cnt >= 3) {
			if ((priv->pmib->dot11RFEntry.dot11channel >= 52) && (priv->pmib->dot11RFEntry.dot11channel <= 64)) {
				pw_lowth_14b[0] = 0;
				pw_lowth_14b[1] = 0;
				pw_lowth_14b[2] = 0;
				pw_lowth_14b[3] = 0;
				pw_lowth_14b[4] = 0;
				pw_lowth_14b[5] = 0;
				pw_lowth_14b[6] = 0;
				pw_lowth_14b[7] = 0;
			}
			else{
				pw_lowth_14b[0] = 0;
				pw_lowth_14b[1] = 0;
				pw_lowth_14b[2] = 0;
				pw_lowth_14b[3] = 0;
				pw_lowth_14b[4] = 0;
				pw_lowth_14b[5] = 0;
				pw_lowth_14b[6] = 80;
				pw_lowth_14b[7] = 0;
			}
		}
		break;
	case 3 :
		radar_class_num = 6;
		if (dc_cnt >= 3) {
			pw_lowth_14b[0] = 0;
			pw_lowth_14b[1] = 0;
			pw_lowth_14b[2] = 0;
			pw_lowth_14b[3] = 0;
			pw_lowth_14b[4] = 0;
			pw_lowth_14b[5] = 0;
			
		}
		break;
	}

	if (priv->radar_type == 0) {
		for (i = 0; i < pw_loop_cnt; i++) {
			priv->pw_14b_rslt[i] = pw_rpt_14b_set[i];			
			//Idealistically, #PW = #PRI + 1
			for ( j = 0; j < radar_class_num ; j++) {
				if ( pw_lowth_14b[j] <= priv->pw_14b_rslt[i] && priv->pw_14b_rslt[i] <= pw_uppth_14b[j] 
					&& pri_rpt_lowth_14b[j] <= priv->pri_14b_rslt && priv->pri_14b_rslt <= pri_rpt_uppth_14b[j])
					radar_class_cnt[j]++;					
			}		
		}
		for(j = 1; j < pw_loop_cnt; j++)
			pw_diff_14b = pw_diff_14b + abs(priv->pw_14b_rslt[j] - priv->pw_14b_rslt[j-1]);
			/*The value may less than the average deviation, since the variable type is int (not float)*/
	}

	// PRI of long pulse is not accurate enough
	else {
		for (i = 0; i < pw_loop_cnt; i++) {
			priv->pw_14b_rslt[i] = pw_rpt_14b_set[i];
			//Idealistically, #PW = #PRI + 1
			for ( j = 0; j < radar_class_num ; j++) {
				if ( pw_lowth_14b[j] <= priv->pw_14b_rslt[i] && priv->pw_14b_rslt[i] <= pw_uppth_14b[j] )
					radar_class_cnt[j]++;					
			}		
		}
	}
	

	
	
	for (i = 0; i < radar_class_num ; i++) {
		if ( radar_class_cnt[i] > cnt_th_14b ) {
			if (!(priv->radar_type)) {
				if (pw_diff_14b < pw_diff_th_14b)
					priv->pulsewidth_flag = 1;
				if (priv->pri_14b_rslt != 0)
					priv->pulseinterval_flag = 1;
			}
			else {
				// for long pulse, we relax the threshold
				priv->pulsewidth_flag = 1;
				priv->pulseinterval_flag = 1;  
			}
				
			radar_class_tmp = i ;
		}
	}

	/* add NHM to decide the validity of the three flags */
	/* use for chaotic open space */
	if (priv->pshare->rf_ft_var.nhm_dfs_en) {
		//panic_printk("DFS NHM threshold start,and nhm_dfs_cnt = %d!\n",nhm_dfs_cnt);

		//we think that the group of smallest power is background noise => skip it 
		for (i = 0; i<= NHM_RPT_NUM - 1; i++) {
			if (ccx->nhm_result[i] > 0) {
				break;
			}
		}

		//only count those groups which power is larger than background noise
		for (j = (i + 1); j<= NHM_RPT_NUM - 1; j++) {
			if (ccx->nhm_result[j] > 0) {
				//panic_printk("nhm_result[%d] = %d!\n",i,ccx->nhm_result[i]);
				priv->nhm_dfs_cnt++;
			}
		}
		if (priv->nhm_dfs_cnt > priv->pshare->rf_ft_var.nhm_dfs_cnt_th) {
			priv->nhm_dfs_flag = 1;
		}
	}
	
			
	if (priv->pshare->rf_ft_var.dfs_print_hist_report) {
		if (priv->nhm_dfs_flag) {
			panic_printk("Enable NHMxDFS\n");
			panic_printk("NHMxDFS cnt = %d!\n", priv->nhm_dfs_cnt);
		} else {
			panic_printk("\n");
			if (priv->pulsewidth_flag && priv->pulseinterval_flag && priv->loct_flag ) 
				panic_printk("Radar type %d has detected!!\n", (radar_class_tmp + 1));
			panic_printk("\n");
			panic_printk("SYN%d\n",priv->seg1_flag);
			if (priv->radar_type == 1)
				panic_printk("long_pulse_flag raise up!\n");
			panic_printk("The current mib information of 8814b: ([Lower bound / Upper bound])\n");
			panic_printk("pw_th1 : [%d, %d], pw_th2 : [%d, %d], pw_th3 : [%d, %d]\n",
				pw_lowth_14b[0], pw_uppth_14b[0], pw_lowth_14b[1], pw_uppth_14b[1], pw_lowth_14b[2], pw_uppth_14b[2]);
			panic_printk("pw_th4 : [%d, %d], pw_th5 : [%d, %d], pw_th6 : [%d, %d] \n",
				pw_lowth_14b[3], pw_uppth_14b[3], pw_lowth_14b[4], pw_uppth_14b[4], pw_lowth_14b[5], pw_uppth_14b[5]); 
			if (priv->pshare->rf_ft_var.manual_dfs_regdomain == 1)
				panic_printk("pw_th7 : [%d, %d]\n",pw_lowth_14b[6], pw_uppth_14b[6]);
			if (priv->pshare->rf_ft_var.manual_dfs_regdomain == 2)
				panic_printk("pw_th7 : [%d, %d], pw_th8 : [%d, %d]\n",pw_lowth_14b[6], pw_uppth_14b[6],pw_lowth_14b[7], pw_uppth_14b[7]);
			panic_printk("pri_th1 : [%d, %d], pri_th2 : [%d, %d], pri_th3 : [%d, %d]\n",
				pri_rpt_lowth_14b[0], pri_rpt_uppth_14b[0], pri_rpt_lowth_14b[1], pri_rpt_uppth_14b[1], pri_rpt_lowth_14b[2], pri_rpt_uppth_14b[2]);
			panic_printk("pri_th4 : [%d, %d], pri_th5 : [%d, %d], pri_th6 : [%d, %d]\n",
				pri_rpt_lowth_14b[3], pri_rpt_uppth_14b[3],pri_rpt_lowth_14b[4], pri_rpt_uppth_14b[4], pri_rpt_lowth_14b[5], pri_rpt_uppth_14b[5]);
			if (priv->pshare->rf_ft_var.manual_dfs_regdomain == 1)
				panic_printk("pri_th7 : [%d, %d]\n",pri_rpt_lowth_14b[6], pri_rpt_uppth_14b[6]);
			if (priv->pshare->rf_ft_var.manual_dfs_regdomain == 2)
				panic_printk("pri_th7 : [%d, %d], pri_th8 : [%d, %d]\n",pri_rpt_lowth_14b[6], pri_rpt_uppth_14b[6],pri_rpt_lowth_14b[7], pri_rpt_uppth_14b[7]);
			panic_printk("loct_diff_th_14b = %d\n", loct_diff_th_14b);
			panic_printk("pw_diff_th_14b = %d\n", pw_diff_th_14b);
			panic_printk("cnt_th_14b (half of loop cnt) = %d\n", cnt_th_14b);
			
			panic_printk("\n"); 
			panic_printk("The current register of 8814b:\n");
			if (priv->seg1_flag == 0)
				panic_printk("PW : [0x2e08] = [%08x]\n", reg_2e08);
			else
				panic_printk("PW : [0x2e28] = [%08x]\n", reg_2e28);
			if (priv->seg1_flag == 0)
				panic_printk("Loct : [0x2e0c, 0x2e10] = [%08x, %08x]\n", reg_2e0c, reg_2e10);
			else
				panic_printk("Loct : [0x2e2c, 0x2e30] = [%08x, %08x]\n", reg_2e2c, reg_2e30);
			panic_printk("PRI : [0x2e18] = [%08x]\n", reg_2e18);
			
			panic_printk("\n");
			panic_printk("The current report of 8814b:\n");
			panic_printk("pw_rpt(1-4) = [%d, %d, %d, %d]\n",
				     pw_rpt_14b_set[0], pw_rpt_14b_set[1], pw_rpt_14b_set[2], pw_rpt_14b_set[3]);
			panic_printk("loct_rpt(1-8) = [%d, %d, %d, %d, %d, %d, %d, %d]\n",
				     loct_rpt_14b_set[0], loct_rpt_14b_set[1], loct_rpt_14b_set[2], loct_rpt_14b_set[3], loct_rpt_14b_set[4], loct_rpt_14b_set[5], loct_rpt_14b_set[6], loct_rpt_14b_set[7]);
			panic_printk("pri_rpt = [%d]\n",pri_rpt_14b);

			panic_printk("\n");
			panic_printk("loct_loop_cnt = %d\n",loct_loop_cnt);
			panic_printk("loct_diff = %d\n", loct_diff_14b);
			panic_printk("pw_diff = %d\n", pw_diff_14b);
			if (priv->pshare->rf_ft_var.manual_dfs_regdomain == 3)
			panic_printk("radar_class_cnt[1-6] = [%d, %d, %d, %d, %d, %d]\n",
				     radar_class_cnt[0], radar_class_cnt[1], radar_class_cnt[2], radar_class_cnt[3], radar_class_cnt[4], radar_class_cnt[5]);
			else if (priv->pshare->rf_ft_var.manual_dfs_regdomain == 1)
			panic_printk("radar_class_cnt[1-7] = [%d, %d, %d, %d, %d, %d, %d]\n",
				     radar_class_cnt[0], radar_class_cnt[1], radar_class_cnt[2], radar_class_cnt[3], radar_class_cnt[4], radar_class_cnt[5], radar_class_cnt[6]);
			
			else
			panic_printk("radar_class_cnt[1-8] = [%d, %d, %d, %d, %d, %d, %d, %d]\n",
				     radar_class_cnt[0], radar_class_cnt[1], radar_class_cnt[2], radar_class_cnt[3], radar_class_cnt[4], radar_class_cnt[5], radar_class_cnt[6], radar_class_cnt[7]);
			}
	}	
}
#endif

#if defined(CONFIG_WLAN_HAL_8812FE)
void dfs_radar_distinguish_for_8812f(struct rtl8192cd_priv *priv, void *dm_void)
{
	struct dm_struct *dm = (struct dm_struct *)dm_void;
	struct ccx_info *ccx = &dm->dm_ccx_info;
	
	/* Added by Big Bill */
	unsigned int reg_2e08, reg_2e24, reg_2e28, reg_2e0c, reg_2e10, reg_2e20, reg_2e2c, reg_2e30, reg_2e34;
	unsigned int pw_rpt_12f_set[12]={0}, pri_rpt_12f_set[12]={0};
	signed char loct_rpt_12f_set[12]={0};
	unsigned char i, j, dc_cnt = 0, short_cnt_th = 0, long_cnt_th = 0, loop_cnt = 0;
	unsigned char radar_class_tmp, pri_cnt_th, pri_var_offset = 0, pri_vaild_max = 0, pri_vaild_min = 32767, pri_var_up=0, pri_var_low=0;
	unsigned int loct_diff = 0, radar_class_cnt[10] = {0}, radar_class_num = 0;
	
	/*for mib*/
	unsigned int pw_lowth_12f[10], pw_uppth_12f[10], pri_rpt_lowth_12f[10], pri_rpt_uppth_12f[10];
	unsigned int loct_diff_th_12f = 0, cnt_th_12f = 0, pri_cnt_th_12f = 0, pri_cnt_final = 0, pri_cnt_temp = 0;
	
	loct_diff_th_12f = priv->pshare->rf_ft_var.loct_diff_th;
	pri_cnt_th = priv->pshare->rf_ft_var.pri_cnt_th;
	pri_var_offset = priv->pshare->rf_ft_var.pri_var_offset;

	if (priv->pshare->rf_ft_var.manual_dfs_regdomain == 2){
		if ((priv->pmib->dot11RFEntry.dot11channel >= 52) && (priv->pmib->dot11RFEntry.dot11channel <= 64)) 
			loct_diff_th_12f = loct_diff_th_12f  + 15;
	}
	
	/* Read pw/pri/lctn report */
	/* PW : Wordlength : U(8,0) , unit: 400ns*/
	reg_2e08 = phy_query_bb_reg(priv, 0x2e08, 0xffffffff);
	pw_rpt_12f_set[0]=(unsigned int)(reg_2e08 & 0x000000ff);
	pw_rpt_12f_set[1]=(unsigned int)((reg_2e08 & 0x0000ff00)>>8);
	pw_rpt_12f_set[2]=(unsigned int)((reg_2e08 & 0x00ff0000)>>16);
	pw_rpt_12f_set[3]=(unsigned int)((reg_2e08 & 0xff000000)>>24);
	reg_2e24 = phy_query_bb_reg(priv, 0x2e24, 0xffffffff);
	pw_rpt_12f_set[4]=(unsigned int)(reg_2e24 & 0x000000ff);
	pw_rpt_12f_set[5]=(unsigned int)((reg_2e24 & 0x0000ff00)>>8);
	pw_rpt_12f_set[6]=(unsigned int)((reg_2e24 & 0x00ff0000)>>16);
	pw_rpt_12f_set[7]=(unsigned int)((reg_2e24 & 0xff000000)>>24);
	reg_2e28 = phy_query_bb_reg(priv, 0x2e28, 0xffffffff);
	pw_rpt_12f_set[8]=(unsigned int)(reg_2e28 & 0x000000ff);
	pw_rpt_12f_set[9]=(unsigned int)((reg_2e28 & 0x0000ff00)>>8);
	pw_rpt_12f_set[10]=(unsigned int)((reg_2e28 & 0x00ff0000)>>16);
	pw_rpt_12f_set[11]=(unsigned int)((reg_2e28 & 0xff000000)>>24);

	/* PRI : Wordlength : U(8,0) , unit: 25.6us */
	reg_2e2c = phy_query_bb_reg(priv, 0x2e2c, 0xffffffff);
	pri_rpt_12f_set[0]=(unsigned int)(reg_2e2c & 0x000000ff);
	pri_rpt_12f_set[1]=(unsigned int)((reg_2e2c & 0x0000ff00)>>8);
	pri_rpt_12f_set[2]=(unsigned int)((reg_2e2c & 0x00ff0000)>>16);
	pri_rpt_12f_set[3]=(unsigned int)((reg_2e2c & 0xff000000)>>24);
	reg_2e30 = phy_query_bb_reg(priv, 0x2e30, 0xffffffff);
	pri_rpt_12f_set[4]=(unsigned int)(reg_2e30 & 0x000000ff);
	pri_rpt_12f_set[5]=(unsigned int)((reg_2e30 & 0x0000ff00)>>8);
	pri_rpt_12f_set[6]=(unsigned int)((reg_2e30 & 0x00ff0000)>>16);
	pri_rpt_12f_set[7]=(unsigned int)((reg_2e30 & 0xff000000)>>24);
	reg_2e34 = phy_query_bb_reg(priv, 0x2e34, 0xffffffff);
	pri_rpt_12f_set[8]=(unsigned int)(reg_2e34 & 0x000000ff);
	pri_rpt_12f_set[9]=(unsigned int)((reg_2e34 & 0x0000ff00)>>8);
	pri_rpt_12f_set[10]=(unsigned int)((reg_2e34 & 0x00ff0000)>>16);
	pri_rpt_12f_set[11]=(unsigned int)((reg_2e34 & 0xff000000)>>24);
	
	/* Location : Wordlength : S(8,0) , unit: MHz */
	reg_2e0c = phy_query_bb_reg(priv, 0x2e0c, 0xffffffff);
	loct_rpt_12f_set[0]=(signed char)(reg_2e0c & 0x000000ff);
	loct_rpt_12f_set[1]=(signed char)((reg_2e0c & 0x0000ff00)>>8);
	loct_rpt_12f_set[2]=(signed char)((reg_2e0c & 0x00ff0000)>>16);
	loct_rpt_12f_set[3]=(signed char)((reg_2e0c & 0xff000000)>>24);
	reg_2e10 = phy_query_bb_reg(priv, 0x2e10, 0xffffffff);
	loct_rpt_12f_set[4]=(signed char)(reg_2e10 & 0x000000ff);
	loct_rpt_12f_set[5]=(signed char)((reg_2e10 & 0x0000ff00)>>8);
	loct_rpt_12f_set[6]=(signed char)((reg_2e10 & 0x00ff0000)>>16);
	loct_rpt_12f_set[7]=(signed char)((reg_2e10 & 0xff000000)>>24);
	reg_2e20 = phy_query_bb_reg(priv, 0x2e20, 0xffffffff);
	loct_rpt_12f_set[8]=(signed char)(reg_2e20 & 0x000000ff);
	loct_rpt_12f_set[9]=(signed char)((reg_2e20 & 0x0000ff00)>>8);
	loct_rpt_12f_set[10]=(signed char)((reg_2e20 & 0x00ff0000)>>16);
	loct_rpt_12f_set[11]=(signed char)((reg_2e20 & 0xff000000)>>24);

	priv->pulsewidth_flag = 0;
	priv->loct_flag = 0;
	priv->pulseinterval_flag = 0;
	for (i = 0; i < 12; i++) {
		priv->pw_12f_rslt[i] = 0;
		priv->loct_12f_rslt[i] = 0;
		priv->pri_12f_rslt[i] = 0;
	}
	// for NHMxDFS //
	priv->nhm_dfs_flag = 0;
	priv->nhm_dfs_cnt = 0;

	short_cnt_th = phy_query_bb_reg(priv, 0xa50, 0x00f00000);
	long_cnt_th = phy_query_bb_reg(priv, 0xa4c, 0xf0000000);

	// In 8812F, a50[23:20] = N => The short_pulse_cnt_th is N+2
	if (priv->radar_type) 
		loop_cnt = long_cnt_th + 2;
	else
		loop_cnt = short_cnt_th  + 2;
	
	if (priv->pshare->rf_ft_var.manual_dfs_regdomain == 3)
		loop_cnt = short_cnt_th  + 2;
	
	if (loop_cnt > 12)
		loop_cnt = 12;

	if (!(priv->pshare->rf_ft_var.pw_pri_cnt_debug)) 
		cnt_th_12f = loop_cnt / 2;
	else 
		cnt_th_12f = priv->pshare->rf_ft_var.loop_cnt_th;	
	

	/*Calculate the difference between each element (1st Not calculated)*/
	for(i = 0; i < loop_cnt; i++)
		priv->loct_12f_rslt[i] = loct_rpt_12f_set[i];
	for(i = 1; i < loop_cnt; i++)
		loct_diff = loct_diff + abs(priv->loct_12f_rslt[i] - priv->loct_12f_rslt[i-1]);
	
	for (i = 0; i < loop_cnt; i++) {
		if (-priv->pshare->rf_ft_var.dc_judge_left < loct_rpt_12f_set[i] && loct_rpt_12f_set[i] < priv->pshare->rf_ft_var.dc_judge_right)
			dc_cnt++;
	}

	/* Added by Big Bill */
	/* Radar type classification */
	pw_lowth_12f[0] = priv->pshare->rf_ft_var.pw_lth1;
	pw_uppth_12f[0] = priv->pshare->rf_ft_var.pw_uth1;
	pw_lowth_12f[1] = priv->pshare->rf_ft_var.pw_lth2;
	pw_uppth_12f[1] = priv->pshare->rf_ft_var.pw_uth2;
	pw_lowth_12f[2] = priv->pshare->rf_ft_var.pw_lth3;
	pw_uppth_12f[2] = priv->pshare->rf_ft_var.pw_uth3;
	pw_lowth_12f[3] = priv->pshare->rf_ft_var.pw_lth4;
	pw_uppth_12f[3] = priv->pshare->rf_ft_var.pw_uth4;
	pw_lowth_12f[4] = priv->pshare->rf_ft_var.pw_lth5;
	pw_uppth_12f[4] = priv->pshare->rf_ft_var.pw_uth5;
	pw_lowth_12f[5] = priv->pshare->rf_ft_var.pw_lth6;
	pw_uppth_12f[5] = priv->pshare->rf_ft_var.pw_uth6;

	pri_rpt_lowth_12f[0] = priv->pshare->rf_ft_var.pri_lth1;
	pri_rpt_uppth_12f[0] = priv->pshare->rf_ft_var.pri_uth1;
	pri_rpt_lowth_12f[1] = priv->pshare->rf_ft_var.pri_lth2;
	pri_rpt_uppth_12f[1] = priv->pshare->rf_ft_var.pri_uth2;
	pri_rpt_lowth_12f[2] = priv->pshare->rf_ft_var.pri_lth3;
	pri_rpt_uppth_12f[2] = priv->pshare->rf_ft_var.pri_uth3;
	pri_rpt_lowth_12f[3] = priv->pshare->rf_ft_var.pri_lth4;
	pri_rpt_uppth_12f[3] = priv->pshare->rf_ft_var.pri_uth4;
	pri_rpt_lowth_12f[4] = priv->pshare->rf_ft_var.pri_lth5;
	pri_rpt_uppth_12f[4] = priv->pshare->rf_ft_var.pri_uth5;
	pri_rpt_lowth_12f[5] = priv->pshare->rf_ft_var.pri_lth6;
	pri_rpt_uppth_12f[5] = priv->pshare->rf_ft_var.pri_uth6;

	if (priv->pshare->rf_ft_var.manual_dfs_regdomain == 1) {
		pw_lowth_12f[6] = priv->pshare->rf_ft_var.pw_lth7;
		pw_uppth_12f[6] = priv->pshare->rf_ft_var.pw_uth7;
		pri_rpt_lowth_12f[6] = priv->pshare->rf_ft_var.pri_lth7;
		pri_rpt_uppth_12f[6] = priv->pshare->rf_ft_var.pri_uth7;
		radar_class_num = 7;
		if (dc_cnt >= priv->pshare->rf_ft_var.dc_cnt_th) {
			pw_lowth_12f[0] = 0;
			pw_lowth_12f[1] = 0;
			pw_lowth_12f[2] = 0;
			pw_lowth_12f[3] = 0;
			pw_lowth_12f[4] = 0;
			pw_lowth_12f[5] = 80;
			pw_lowth_12f[6] = 0;
		}
	} else if (priv->pshare->rf_ft_var.manual_dfs_regdomain == 2) {
		pw_lowth_12f[6] = priv->pshare->rf_ft_var.pw_lth7;
		pw_uppth_12f[6] = priv->pshare->rf_ft_var.pw_uth7;
		pw_lowth_12f[7] = priv->pshare->rf_ft_var.pw_lth8;
		pw_uppth_12f[7] = priv->pshare->rf_ft_var.pw_uth8;
		pri_rpt_lowth_12f[6] = priv->pshare->rf_ft_var.pri_lth7;
		pri_rpt_uppth_12f[6] = priv->pshare->rf_ft_var.pri_uth7;
		pri_rpt_lowth_12f[7] = priv->pshare->rf_ft_var.pri_lth8;
		pri_rpt_uppth_12f[7] = priv->pshare->rf_ft_var.pri_uth8;
		radar_class_num = 8;
		if (dc_cnt >= priv->pshare->rf_ft_var.dc_cnt_th) {
			if ((priv->pmib->dot11RFEntry.dot11channel >= 52) && (priv->pmib->dot11RFEntry.dot11channel <= 64)) {
				if(priv->pshare->rf_ft_var.jp_new_w53 == 1){
				pw_lowth_12f[0] = 0;
				pw_lowth_12f[1] = 0;
				pw_lowth_12f[2] = 0;
				pw_lowth_12f[3] = 0;
				pw_lowth_12f[4] = 0;
				pw_lowth_12f[5] = 0;
				pw_lowth_12f[6] = 0;
				pw_lowth_12f[7] = 0;
				cnt_th_12f--;
			}
			else{
				radar_class_num = 2;
					pw_lowth_12f[0] = 0;
					pw_lowth_12f[1] = 0;
					if(!(priv->idle_flag)) {
						pri_rpt_uppth_12f[1] = 255;
					}
				}
			}else {
				pw_lowth_12f[0] = 0;
				pw_lowth_12f[1] = 0;
				pw_lowth_12f[2] = 0;
				pw_lowth_12f[3] = 0;
				pw_lowth_12f[4] = 0;
				pw_lowth_12f[5] = 0;
				pw_lowth_12f[6] = 80;
				pw_lowth_12f[7] = 0;
			}
		}
	} else if (priv->pshare->rf_ft_var.manual_dfs_regdomain == 3) {
		radar_class_num = 6;
		if (dc_cnt >= priv->pshare->rf_ft_var.dc_cnt_th) {
			pw_lowth_12f[0] = 0;
			pw_lowth_12f[1] = 0;
			pw_lowth_12f[2] = 0;
			pw_lowth_12f[3] = 0;
			pw_lowth_12f[4] = 0;
			pw_lowth_12f[5] = 0;
		}
	}

	for (i = 0; i < loop_cnt; i++) {
		priv->pw_12f_rslt[i] = pw_rpt_12f_set[i];
		priv->pri_12f_rslt[i] = pri_rpt_12f_set[i];
		// Idealistically, #PW = #PRI + 1
		for (j = 0; j < radar_class_num; j++) {
			if (priv->radar_type) {
				if ((pw_lowth_12f[j] <= priv->pw_12f_rslt[i]) && (priv->pw_12f_rslt[i] <= pw_uppth_12f[j]))
					radar_class_cnt[j]++;
			} else {
				if ((pw_lowth_12f[j] <= priv->pw_12f_rslt[i]) && (priv->pw_12f_rslt[i] <= pw_uppth_12f[j]) &&
					(pri_rpt_lowth_12f[j] <= priv->pri_12f_rslt[i]) && (priv->pri_12f_rslt[i] <= pri_rpt_uppth_12f[j])) {
					radar_class_cnt[j]++;
					if (priv->pri_12f_rslt[i] > pri_vaild_max)
						pri_vaild_max = priv->pri_12f_rslt[i];
					if (priv->pri_12f_rslt[i] < pri_vaild_min)
						pri_vaild_min = priv->pri_12f_rslt[i];
				}
			}
		}
	}
	
	// Pattern Judgement
	for (i = 0; i < radar_class_num; i++) {
		if (radar_class_cnt[i] >= cnt_th_12f) {
			priv->pulsewidth_flag = 1;
			radar_class_tmp = i ;
		}
	}

	// Relax approximation
	if(!(priv->idle_flag)) {
		pri_cnt_th--;
		if (!(priv->pshare->rf_ft_var.manual_dfs_regdomain == 3)){
			pri_var_offset = 15;
			if (priv->pshare->rf_ft_var.manual_dfs_regdomain == 2)
				pri_var_offset = 5 + 10*pri_vaild_min/20;
		}
		else
			pri_var_offset = 9;
	}

	// Mode approximation for PRI
	for (i = 0; i < (loop_cnt - 1); i++) {
		pri_cnt_temp = 0;
		for (j = 0; j < (loop_cnt - 1); j++) {
			if ((i == j) || (priv->pri_12f_rslt[i] <= 4))
				continue;
			if (priv->pri_12f_rslt[j] > (255 - pri_var_offset))
				pri_var_up = 255;
			else
				pri_var_up = priv->pri_12f_rslt[j] + pri_var_offset;
			if (pri_var_offset >= priv->pri_12f_rslt[j])
				pri_var_low = 0;
			else
				pri_var_low = (priv->pri_12f_rslt[j] - pri_var_offset);
			if ((priv->pri_12f_rslt[i] <= pri_var_up) && (priv->pri_12f_rslt[i] >= pri_var_low)
				&& (priv->pri_12f_rslt[i] <= pri_vaild_max) && (priv->pri_12f_rslt[i] >= pri_vaild_min))
				pri_cnt_temp++;
		}
		if (pri_cnt_temp > pri_cnt_final)
			pri_cnt_final = pri_cnt_temp;

	}

	// Advanced PRI Judgement (Bypass for long pulse flag)
	if (pri_cnt_final != 0)
		pri_cnt_final++;

	if (priv->radar_type)
		priv->pulseinterval_flag = 1;
	else if (pri_cnt_final >= pri_cnt_th)
		priv->pulseinterval_flag = 1;
#ifdef CONFIG_DLINK_DFS_AZROAD_CE_TEST
	else if ((priv->pri_12f_rslt[0] == 0) && (priv->pri_12f_rslt[1] > 0) && (priv->pri_12f_rslt[2] == 0) &&
		(priv->pri_12f_rslt[3] > 0))
		priv->pulseinterval_flag = 1;
	else if ((priv->pri_12f_rslt[0] > 0) && (priv->pri_12f_rslt[1] == 0) && (priv->pri_12f_rslt[2] > 0) &&
		(priv->pri_12f_rslt[3] == 0))
		priv->pulseinterval_flag = 1;
#endif /* CONFIG_DLINK_DFS_AZROAD_CE_TEST */

	if (priv->pshare->rf_ft_var.manual_dfs_regdomain == 2){
		if ((priv->pmib->dot11RFEntry.dot11channel >= 52) && (priv->pmib->dot11RFEntry.dot11channel <= 64)){
				if(!(priv->idle_flag))
					priv->pulseinterval_flag = 1;
		}
	}

	// Location Judgement (Bypass for long pulse flag)
	if (priv->radar_type)
		priv->loct_flag = 1;
	else if (loct_diff <= loct_diff_th_12f)
		priv->loct_flag = 1;

	/* Add NHM to decide the validity of the three flags */
	/* Use for chaotic open space */
	if (priv->pshare->rf_ft_var.nhm_dfs_en) {
		for (i = 0; i <= (NHM_RPT_NUM - 1); i++) {
			if (ccx->nhm_result[i] > 0)
				break;
		}
		for (j = (i+1); j <= (NHM_RPT_NUM - 1); j++) {
			if (ccx->nhm_result[j] > 0)
				priv->nhm_dfs_cnt++;
		}
		if (priv->nhm_dfs_cnt > priv->pshare->rf_ft_var.nhm_dfs_cnt_th)
			priv->nhm_dfs_flag = 1;
	}
	
	if (priv->pshare->rf_ft_var.dfs_print_hist_report) {
		if (priv->nhm_dfs_flag) {
			panic_printk("Enable NHMxDFS\n");
			panic_printk("NHMxDFS cnt = %d!\n", priv->nhm_dfs_cnt);
		} else {
			panic_printk("\n");
			if (priv->pulsewidth_flag && priv->pulseinterval_flag && priv->loct_flag)
				panic_printk("Radar type %d is detected!!\n", (radar_class_tmp + 1));
			else
				panic_printk("Suspected radar result is dropped by SW mechanism\n");

			if (!(priv->pulsewidth_flag))
				panic_printk("- PW/PRI-J Failure\n");
			if (!(priv->pulseinterval_flag))
				panic_printk("- Adv-PRI-J Failure\n");
			if (!(priv->loct_flag))
				panic_printk("- Loct-J Failure\n");
			
			panic_printk("\n");
			panic_printk("The current mib information of 8812F: ([Lower bound / Upper bound])\n");
			panic_printk("pw_th1 : [%d, %d], pw_th2 : [%d, %d], pw_th3 : [%d, %d]\n",
				pw_lowth_12f[0], pw_uppth_12f[0], pw_lowth_12f[1], pw_uppth_12f[1], pw_lowth_12f[2], pw_uppth_12f[2]);
			panic_printk("pw_th4 : [%d, %d], pw_th5 : [%d, %d], pw_th6 : [%d, %d] \n",
				pw_lowth_12f[3], pw_uppth_12f[3], pw_lowth_12f[4], pw_uppth_12f[4], pw_lowth_12f[5], pw_uppth_12f[5]); 
			if (!(priv->pshare->rf_ft_var.manual_dfs_regdomain == 3)) {
				panic_printk("pw_th7 : [%d, %d]\n", pw_lowth_12f[6], pw_uppth_12f[6]);
				if (priv->pshare->rf_ft_var.manual_dfs_regdomain == 2)
					panic_printk("pw_th8 : [%d, %d]\n", pw_lowth_12f[7], pw_uppth_12f[7]);
			}
			panic_printk("pri_th1 : [%d, %d], pri_th2 : [%d, %d], pri_th3 : [%d, %d]\n",
				pri_rpt_lowth_12f[0], pri_rpt_uppth_12f[0], pri_rpt_lowth_12f[1], pri_rpt_uppth_12f[1], pri_rpt_lowth_12f[2], pri_rpt_uppth_12f[2]);
			panic_printk("pri_th4 : [%d, %d], pri_th5 : [%d, %d], pri_th6 : [%d, %d]\n",
				pri_rpt_lowth_12f[3], pri_rpt_uppth_12f[3],pri_rpt_lowth_12f[4], pri_rpt_uppth_12f[4], pri_rpt_lowth_12f[5], pri_rpt_uppth_12f[5]);
			if (!(priv->pshare->rf_ft_var.manual_dfs_regdomain == 3)) {
				panic_printk("pri_th7 : [%d, %d]\n", pri_rpt_lowth_12f[6], pri_rpt_uppth_12f[6]);
				if (priv->pshare->rf_ft_var.manual_dfs_regdomain == 2)
					panic_printk("pri_th8 : [%d, %d]\n", pri_rpt_lowth_12f[7], pri_rpt_uppth_12f[7]);
			}
			panic_printk("loct_diff_th_12f = [%d]\n", loct_diff_th_12f);
			panic_printk("dc_cnt = [%d]\n", dc_cnt);
			panic_printk("loop_cnt = [%d]\n", loop_cnt);
			if (!(priv->idle_flag)) {
				panic_printk("pri_var_offset = [%d] (Relax for TP mode)\n", pri_var_offset);
				panic_printk("pri_cnt_th = [%d] (Relax for TP mode)\n", pri_cnt_th);
			} else {
				panic_printk("pri_var_offset = [%d]\n", pri_var_offset);
				panic_printk("pri_cnt_th = [%d]\n",pri_cnt_th);
			}
			panic_printk("cnt_th_12f = [%d] (Default : Half of loop cnt)\n", cnt_th_12f);

			panic_printk("\n");
			panic_printk("The current report of 8812F:\n");
			panic_printk("pw_rpt(1-12) = [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d]\n",
			     	pw_rpt_12f_set[0], pw_rpt_12f_set[1], pw_rpt_12f_set[2], pw_rpt_12f_set[3], pw_rpt_12f_set[4], pw_rpt_12f_set[5], pw_rpt_12f_set[6], pw_rpt_12f_set[7],pw_rpt_12f_set[8], pw_rpt_12f_set[9], pw_rpt_12f_set[10], pw_rpt_12f_set[11]);
			panic_printk("loct_rpt(1-12) = [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d]\n",
			     	loct_rpt_12f_set[0], loct_rpt_12f_set[1], loct_rpt_12f_set[2], loct_rpt_12f_set[3], loct_rpt_12f_set[4], loct_rpt_12f_set[5], loct_rpt_12f_set[6], loct_rpt_12f_set[7], loct_rpt_12f_set[8], loct_rpt_12f_set[9], loct_rpt_12f_set[10], loct_rpt_12f_set[11]);
			panic_printk("pri_rpt(1-12) = [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d]\n",
			     	pri_rpt_12f_set[0], pri_rpt_12f_set[1], pri_rpt_12f_set[2], pri_rpt_12f_set[3], pri_rpt_12f_set[4], pri_rpt_12f_set[5], pri_rpt_12f_set[6], pri_rpt_12f_set[7], pri_rpt_12f_set[8], pri_rpt_12f_set[9], pri_rpt_12f_set[10], pri_rpt_12f_set[11]);
			panic_printk("\n");
			panic_printk("loct_diff = %d\n", loct_diff);

			if (pri_vaild_max == 0 && pri_vaild_min == 32767)
				panic_printk("No vaild pri_max and pri_min");
			else
				panic_printk("pri_vaild_max = %d, pri_vaild_min = %d\n", pri_vaild_max, pri_vaild_min);
			panic_printk("advanced_pri_cnt = %d\n", pri_cnt_final);
			
			if (priv->pshare->rf_ft_var.manual_dfs_regdomain == 1)
				panic_printk("radar_class_cnt[1-7] = [%d, %d, %d, %d, %d, %d, %d]\n", radar_class_cnt[0], radar_class_cnt[1], radar_class_cnt[2], radar_class_cnt[3], radar_class_cnt[4], radar_class_cnt[5], radar_class_cnt[6]);
			else if (priv->pshare->rf_ft_var.manual_dfs_regdomain == 2)
				panic_printk("radar_class_cnt[1-8] = [%d, %d, %d, %d, %d, %d, %d, %d]\n", radar_class_cnt[0], radar_class_cnt[1], radar_class_cnt[2], radar_class_cnt[3], radar_class_cnt[4], radar_class_cnt[5], radar_class_cnt[6], radar_class_cnt[7]);
			else if (priv->pshare->rf_ft_var.manual_dfs_regdomain == 3)
				panic_printk("radar_class_cnt[1-6] = [%d, %d, %d, %d, %d, %d]\n", radar_class_cnt[0], radar_class_cnt[1], radar_class_cnt[2], radar_class_cnt[3], radar_class_cnt[4], radar_class_cnt[5]);
		}
	}
}
#endif

void rtl8192cd_dfs_det_chk(struct rtl8192cd_priv *priv)
{
	struct dm_struct *dm = &(priv->pshare->_dmODM);
	struct ccx_info *ccx = &dm->dm_ccx_info;
	unsigned int regf98_value;
	unsigned int reg2e00_value;
#if defined(CONFIG_WLAN_HAL_8814BE)
	unsigned int reg2e20_value;
#endif
	unsigned int reg918_value;
	unsigned int reg91c_value;
	unsigned int reg920_value;
	unsigned int reg924_value;
	unsigned int reg19e4_value;

	unsigned int rega40_value;
	unsigned int rega44_value;
	unsigned int rega48_value;
	unsigned int rega4c_value;
	unsigned int rega50_value;
	unsigned int rega54_value;
	u32 reg_dfsrpt_value = 0;
	
	unsigned int FA_count_cur=0, FA_count_inc=0;
	unsigned int CCA_count_cur=0, CCA_count_inc=0;
	unsigned int VHT_CRC_ok_cnt_cur=0, VHT_CRC_ok_cnt_inc=0;
	unsigned int HT_CRC_ok_cnt_cur=0, HT_CRC_ok_cnt_inc=0;
	unsigned int LEG_CRC_ok_cnt_cur=0, LEG_CRC_ok_cnt_inc=0;
	unsigned int Total_CRC_OK_cnt_inc=0, FA_CRCOK_ratio=0;
	unsigned char DFS_tri_short_pulse=0, DFS_tri_long_pulse=0, fa_mask_mid_th=0, fa_mask_lower_th=0;
	unsigned int short_pulse_cnt_cur=0, short_pulse_cnt_inc=0;
	unsigned int long_pulse_cnt_cur=0, long_pulse_cnt_inc=0;
	unsigned int total_pulse_count_inc=0, max_sht_pusle_cnt_th=0;
	unsigned int sum, k, fa_flag=0;
	unsigned int st_L2H_new=0, st_L2H_tmp, index=0, fault_flag_det, fault_flag_psd;
	int j, flags=0;
	unsigned long throughput = 0;

	int i, PSD_report_right[20], PSD_report_left[20];
	int max_right, max_left;
	int max_fa_in_hist=0, total_fa_in_hist=0, pre_post_now_acc_fa_in_hist=0;

	if (priv->det_asoc_clear > 0) {
		priv->det_asoc_clear--;
		priv->pmib->dot11DFSEntry.DFS_detected = 0;
		priv->FA_count_pre = 0;
		priv->CCA_count_pre = 0;
		priv->VHT_CRC_ok_cnt_pre = 0;
		priv->HT_CRC_ok_cnt_pre = 0;
		priv->LEG_CRC_ok_cnt_pre = 0;
		priv->mask_idx = 0;
		priv->hist_idx = 0;
		priv->mask_hist_checked = 0;
		memset(priv->radar_det_mask_hist, 0, sizeof(priv->radar_det_mask_hist));
		memset(priv->pulse_flag_hist, 0, sizeof(priv->pulse_flag_hist));
		memset(priv->pulse_type_hist, 0, sizeof(priv->pulse_type_hist));
		mod_timer(&priv->dfs_det_chk_timer, jiffies + RTL_MILISECONDS_TO_JIFFIES(priv->pshare->rf_ft_var.dfs_det_period*10));
		return;
	}
	
	throughput = priv->ext_stats.tx_avarage+priv->ext_stats.rx_avarage;
#ifdef UNIVERSAL_REPEATER
	if (GET_VXD_PRIV(priv) && (GET_VXD_PRIV(priv)->drv_state & DRV_STATE_OPEN))
		throughput += (GET_VXD_PRIV(priv)->ext_stats.tx_avarage + GET_VXD_PRIV(priv)->ext_stats.rx_avarage);
#endif

#ifdef MBSSID
	if (priv->pmib->miscEntry.vap_enable) {
		for (j=0; j<RTL8192CD_NUM_VWLAN; j++) {
			if (IS_DRV_OPEN(priv->pvap_priv[j])) {
				throughput += priv->pvap_priv[j]->ext_stats.tx_avarage+priv->pvap_priv[j]->ext_stats.rx_avarage;
			}
		}
	}
#endif

	// Get FA count during past 100ms
	if ((GET_CHIP_VER(priv) == VERSION_8814B) || (GET_CHIP_VER(priv) == VERSION_8812F)){
		FA_count_cur = phy_query_bb_reg(priv, 0x2d00, 0x0000ffff);
		CCA_count_cur = phy_query_bb_reg(priv, 0x2c08, 0xffff0000);
	}
	else {
		FA_count_cur = phy_query_bb_reg(priv, 0xf48, 0x0000ffff);
		CCA_count_cur = phy_query_bb_reg(priv, 0xf08, 0xffff0000);
	}
	
	if (priv->FA_count_pre == 0)
		FA_count_inc = 0;
	else if (FA_count_cur >= priv->FA_count_pre)
		FA_count_inc = FA_count_cur - priv->FA_count_pre;
	else
		FA_count_inc = FA_count_cur;
	priv->FA_count_pre = FA_count_cur;

	if (priv->CCA_count_pre == 0)
		CCA_count_inc = 0;
	else if (CCA_count_cur >= priv->CCA_count_pre)
		CCA_count_inc = CCA_count_cur - priv->CCA_count_pre;
	else
		CCA_count_inc = CCA_count_cur;	
	priv->CCA_count_pre = CCA_count_cur;

	priv->fa_inc_hist[priv->mask_idx] = FA_count_inc;	
	for (i=0; i<5; i++) {		
		total_fa_in_hist = total_fa_in_hist + priv->fa_inc_hist[i]; 	
		if (priv->fa_inc_hist[i] > max_fa_in_hist)			
			max_fa_in_hist = priv->fa_inc_hist[i];	
	}	
	/*the index is the two times before*/
	if (priv->mask_idx >= priv->pshare->rf_ft_var.dfs_det_flag_offset)		
		index = priv->mask_idx - priv->pshare->rf_ft_var.dfs_det_flag_offset;
	else		
		index = priv->pshare->rf_ft_var.dfs_det_hist_len + priv->mask_idx - priv->pshare->rf_ft_var.dfs_det_flag_offset;	
	if (index == 0) 	
		pre_post_now_acc_fa_in_hist = priv->fa_inc_hist[index] + priv->fa_inc_hist[index+1] + priv->fa_inc_hist[4]; 
	else if (index == 4)		
		pre_post_now_acc_fa_in_hist = priv->fa_inc_hist[index] + priv->fa_inc_hist[0] + priv->fa_inc_hist[index-1]; 
	else		
		pre_post_now_acc_fa_in_hist = priv->fa_inc_hist[index] + priv->fa_inc_hist[index+1] + priv->fa_inc_hist[index-1];

	// Get VHT CRC32 ok count during past 100ms
	if ((GET_CHIP_VER(priv) == VERSION_8814B) || (GET_CHIP_VER(priv) == VERSION_8812F)) {
		VHT_CRC_ok_cnt_cur = phy_query_bb_reg(priv, 0x2c0c, 0x0000ffff);
		if (VHT_CRC_ok_cnt_cur >= priv->VHT_CRC_ok_cnt_pre)
			VHT_CRC_ok_cnt_inc = VHT_CRC_ok_cnt_cur - priv->VHT_CRC_ok_cnt_pre;
		else
			VHT_CRC_ok_cnt_inc = VHT_CRC_ok_cnt_cur;
		priv->VHT_CRC_ok_cnt_pre = VHT_CRC_ok_cnt_cur;
	} else {
		VHT_CRC_ok_cnt_cur = phy_query_bb_reg(priv, 0xf0c, 0x00003fff);
		if (VHT_CRC_ok_cnt_cur >= priv->VHT_CRC_ok_cnt_pre)
			VHT_CRC_ok_cnt_inc = VHT_CRC_ok_cnt_cur - priv->VHT_CRC_ok_cnt_pre;
		else
			VHT_CRC_ok_cnt_inc = VHT_CRC_ok_cnt_cur;
		priv->VHT_CRC_ok_cnt_pre = VHT_CRC_ok_cnt_cur;
	}
	

	// Get HT CRC32 ok count during past 100ms
	if ((GET_CHIP_VER(priv) == VERSION_8814B) || (GET_CHIP_VER(priv) == VERSION_8812F)) {
		HT_CRC_ok_cnt_cur = phy_query_bb_reg(priv, 0x2c10, 0x0000ffff);
		if (HT_CRC_ok_cnt_cur >= priv->HT_CRC_ok_cnt_pre)
			HT_CRC_ok_cnt_inc = HT_CRC_ok_cnt_cur - priv->HT_CRC_ok_cnt_pre;
		else
			HT_CRC_ok_cnt_inc = HT_CRC_ok_cnt_cur;
		priv->HT_CRC_ok_cnt_pre = HT_CRC_ok_cnt_cur;
	} else {
		HT_CRC_ok_cnt_cur = phy_query_bb_reg(priv, 0xf10, 0x00003fff);
		if (HT_CRC_ok_cnt_cur >= priv->HT_CRC_ok_cnt_pre)
			HT_CRC_ok_cnt_inc = HT_CRC_ok_cnt_cur - priv->HT_CRC_ok_cnt_pre;
		else
			HT_CRC_ok_cnt_inc = HT_CRC_ok_cnt_cur;
		priv->HT_CRC_ok_cnt_pre = HT_CRC_ok_cnt_cur;
	}

	// Get Legacy CRC32 ok count during past 100ms
	if ((GET_CHIP_VER(priv) == VERSION_8814B) || (GET_CHIP_VER(priv) == VERSION_8812F)) {
		LEG_CRC_ok_cnt_cur = phy_query_bb_reg(priv, 0x2c14, 0x0000ffff);
			if (LEG_CRC_ok_cnt_cur >= priv->LEG_CRC_ok_cnt_pre)
				LEG_CRC_ok_cnt_inc = LEG_CRC_ok_cnt_cur - priv->LEG_CRC_ok_cnt_pre;
			else
				LEG_CRC_ok_cnt_inc = LEG_CRC_ok_cnt_cur;
		priv->LEG_CRC_ok_cnt_pre = LEG_CRC_ok_cnt_cur;
	} else {
		LEG_CRC_ok_cnt_cur = phy_query_bb_reg(priv, 0xf14, 0x00003fff);
			if (LEG_CRC_ok_cnt_cur >= priv->LEG_CRC_ok_cnt_pre)
				LEG_CRC_ok_cnt_inc = LEG_CRC_ok_cnt_cur - priv->LEG_CRC_ok_cnt_pre;
			else
				LEG_CRC_ok_cnt_inc = LEG_CRC_ok_cnt_cur;
		priv->LEG_CRC_ok_cnt_pre = LEG_CRC_ok_cnt_cur;
	} 

	if ((GET_CHIP_VER(priv) == VERSION_8814B) || (GET_CHIP_VER(priv) == VERSION_8812F)) {
		if (VHT_CRC_ok_cnt_cur == 0xffff ||
			HT_CRC_ok_cnt_cur == 0xffff ||
			LEG_CRC_ok_cnt_cur == 0xffff) {
		phy_set_bb_reg(priv, 0x1eb4, BIT(25), 1);
		phy_set_bb_reg(priv, 0x1eb4, BIT(25), 0);
		}
	} else {
		if ((VHT_CRC_ok_cnt_cur == 0x3fff) || (HT_CRC_ok_cnt_cur == 0x3fff) || (LEG_CRC_ok_cnt_cur == 0x3fff)) {
		phy_set_bb_reg(priv, 0xb58, BIT(0), 1);
		phy_set_bb_reg(priv, 0xb58, BIT(0), 0);
		}
	}

	Total_CRC_OK_cnt_inc = VHT_CRC_ok_cnt_inc + HT_CRC_ok_cnt_inc + LEG_CRC_ok_cnt_inc;

	// check if the FA occrus frequencly during 100ms
	// FA_count_inc is divided by Total_CRC_OK_cnt_inc, which helps to distinguish normal trasmission from interference
	if (Total_CRC_OK_cnt_inc > 0)
		FA_CRCOK_ratio	= FA_count_inc / Total_CRC_OK_cnt_inc;

	//=====dynamic power threshold (DPT) ========
	// Get short pulse count, need carefully handle the counter overflow
#if defined(CONFIG_WLAN_HAL_8814BE)
	if ((GET_CHIP_VER(priv) == VERSION_8814B)) {
		if (priv->seg1_flag == 1) {
			reg2e20_value = phy_query_bb_reg(priv, 0x2e20, 0xffffffff);
			short_pulse_cnt_cur = (u16)((reg2e20_value & 0x000ff800) >> 11);
			long_pulse_cnt_cur = (u16)((reg2e20_value & 0x0fc00000) >> 22);
		} else {
			reg2e00_value =  phy_query_bb_reg(priv, 0x2e00, 0xffffffff);
			short_pulse_cnt_cur = (u16)((reg2e00_value & 0x000ff800) >> 11);
			long_pulse_cnt_cur = (u16)((reg2e00_value & 0x0fc00000) >> 22);
		}
	} else 
#endif
	if ((GET_CHIP_VER(priv) == VERSION_8812F)){
		reg2e00_value =  phy_query_bb_reg(priv, 0x2e00, 0xffffffff);
		short_pulse_cnt_cur = (u16)((reg2e00_value & 0x000ff800) >> 11);
		long_pulse_cnt_cur = (u16)((reg2e00_value & 0x0fc00000) >> 22);
	} else 

	{
		regf98_value = phy_query_bb_reg(priv, 0xf98, 0xffffffff);
		short_pulse_cnt_cur = regf98_value & 0x000000ff;
		long_pulse_cnt_cur = (regf98_value & 0x0000ff00) >> 8;
	}
	
	if (short_pulse_cnt_cur >= priv->short_pulse_cnt_pre)
		short_pulse_cnt_inc = short_pulse_cnt_cur - priv->short_pulse_cnt_pre;
	else
		short_pulse_cnt_inc = short_pulse_cnt_cur;
	priv->short_pulse_cnt_pre = short_pulse_cnt_cur;

	// Get long pulse count, need carefully handle the counter overflow

	if (long_pulse_cnt_cur >= priv->long_pulse_cnt_pre)
		long_pulse_cnt_inc = long_pulse_cnt_cur - priv->long_pulse_cnt_pre;
	else
		long_pulse_cnt_inc = long_pulse_cnt_cur;
	priv->long_pulse_cnt_pre = long_pulse_cnt_cur;

	total_pulse_count_inc = short_pulse_cnt_inc + long_pulse_cnt_inc;

	if (priv->pshare->rf_ft_var.dfs_det_print) {
		panic_printk("=====================================================================\n");
		panic_printk("Total_CRC_OK_cnt_inc[%d] VHT_CRC_ok_cnt_inc[%d] HT_CRC_ok_cnt_inc[%d] LEG_CRC_ok_cnt_inc[%d] FA_count_inc[%d] CCA_count_inc[%d] FA_CRCOK_ratio[%d]\n",
			Total_CRC_OK_cnt_inc, VHT_CRC_ok_cnt_inc, HT_CRC_ok_cnt_inc, LEG_CRC_ok_cnt_inc, FA_count_inc, CCA_count_inc, FA_CRCOK_ratio);
		panic_printk("Init_Gain[%x] st_L2H_cur[%x] 0xf98[%08x] short_pulse_cnt_inc[%d] long_pulse_cnt_inc[%d]\n",
			priv->ini_gain_cur, priv->st_L2H_cur, regf98_value, short_pulse_cnt_inc, long_pulse_cnt_inc);
		panic_printk("Throughput: %luMbps\n", (throughput>>17));
		panic_printk("nhm_duty_cycle = %d\n", (100-((ccx->nhm_rpt_sum * 100) >> 8)));
		if ((GET_CHIP_VER(priv) == VERSION_8814B) || (GET_CHIP_VER(priv) == VERSION_8812F)){
			rega40_value = phy_query_bb_reg(priv, 0xa40, 0xffffffff);
			rega44_value = phy_query_bb_reg(priv, 0xa44, 0xffffffff);
			rega48_value = phy_query_bb_reg(priv, 0xa48, 0xffffffff);
			rega4c_value = phy_query_bb_reg(priv, 0xa4c, 0xffffffff);
			rega50_value = phy_query_bb_reg(priv, 0xa50, 0xffffffff);
			rega54_value = phy_query_bb_reg(priv, 0xa54, 0xffffffff);
			reg2e00_value =  phy_query_bb_reg(priv, 0x2e00, 0xffffffff);
			panic_printk("0xa40[%08x] 0xa44[%08x] 0xa48[%08x] 0xa4c[%08x] 0xa50[%08x] 0xa54[%08x] 0x2e00[%08x] \n", rega40_value, rega44_value, rega48_value, rega4c_value, rega50_value, rega54_value, reg2e00_value);
#if defined(CONFIG_WLAN_HAL_8814BE)
			reg2e20_value = phy_query_bb_reg(priv, 0x2e20, 0xffffffff);
			panic_printk("0x2e20[%08x] \n", reg2e20_value);
#endif
		} else {
			reg918_value = phy_query_bb_reg(priv, 0x918, 0xffffffff);
			reg91c_value = phy_query_bb_reg(priv, 0x91c, 0xffffffff);
			reg920_value = phy_query_bb_reg(priv, 0x920, 0xffffffff);
			reg924_value = phy_query_bb_reg(priv, 0x924, 0xffffffff);
			reg19e4_value = phy_query_bb_reg(priv, 0x19e4, 0xffffffff);
			panic_printk("0x918[%08x] 0x91c[%08x] 0x920[%08x] 0x924[%08x] 0x19e4[%08x] \n", reg918_value, reg91c_value, reg920_value, reg924_value, reg19e4_value);
		}
		
	}

	// read hw dfs detection flag
#if defined(CONFIG_WLAN_HAL_8814BE)
	if ((GET_CHIP_VER(priv) == VERSION_8814B)) {
		if (priv->seg1_flag == 1) {
			DFS_tri_short_pulse = (reg2e20_value & BIT(20)) ? 1 : 0;
			DFS_tri_long_pulse = (reg2e20_value & BIT(28)) ? 1 : 0;
		} else {
			DFS_tri_short_pulse = (reg2e00_value & BIT(20)) ? 1 : 0;
			DFS_tri_long_pulse = (reg2e00_value & BIT(28)) ? 1 : 0;
		}
	} else
#endif
	if ((GET_CHIP_VER(priv) == VERSION_8812F)) {
		DFS_tri_short_pulse = (reg2e00_value & BIT(20)) ? 1 : 0;
		DFS_tri_long_pulse = (reg2e00_value & BIT(28)) ? 1 : 0;
	} else {
		DFS_tri_short_pulse = (regf98_value & BIT(17))? 1 : 0;
		DFS_tri_long_pulse = (regf98_value & BIT(19))? 1 : 0;
	}

	if (DFS_tri_short_pulse) {
		//RTL_W32(0x920, RTL_R32(0x920) | BIT(24));
		//RTL_W32(0x920, RTL_R32(0x920) & ~BIT(24));
		//RTL_W32(0x924, RTL_R32(0x924) & ~BIT(15));
		//RTL_W32(0x924, RTL_R32(0x924) | BIT(15));
	}
	if (DFS_tri_long_pulse) {
		//RTL_W32(0x920, RTL_R32(0x920) | BIT(25));
		//RTL_W32(0x920, RTL_R32(0x920) & ~BIT(25));
		//RTL_W32(0x924, RTL_R32(0x924) & ~BIT(15));
		//RTL_W32(0x924, RTL_R32(0x924) | BIT(15));
		// MKK
		if (priv->pshare->rf_ft_var.manual_dfs_regdomain == 2) {	
			if ((priv->pmib->dot11RFEntry.dot11channel >= 52) &&
				(priv->pmib->dot11RFEntry.dot11channel <= 64)) {
				DFS_tri_long_pulse = 0;
			}
		}
		// ETSI
		if (priv->pshare->rf_ft_var.manual_dfs_regdomain == 3) {
			DFS_tri_long_pulse = 0;
		}
	}

	st_L2H_new = priv->st_L2H_cur;
	priv->pulse_flag_hist[priv->mask_idx] = DFS_tri_short_pulse | DFS_tri_long_pulse;
	priv->pulse_type_hist[priv->mask_idx] = (DFS_tri_long_pulse)? 1 : 0;
	
#if defined(CONFIG_WLAN_HAL_8814BE)
	if((GET_CHIP_VER(priv) == VERSION_8814B)) {
		if (priv->pulse_type_hist[priv->mask_idx])
			priv->radar_type = 1;
		else
			priv->radar_type = 0;
		if(priv->pshare->rf_ft_var.dfs_radar_diff_on && priv->pshare->rf_ft_var.dfs_det_off == 0) {
			if(priv->pulse_flag_hist[priv->mask_idx])
				dfs_radar_distinguish_for_8814b(priv,dm);
		}
	}
#endif

#if defined(CONFIG_WLAN_HAL_8812FE)
	if((GET_CHIP_VER(priv) == VERSION_8812F)) {
		if (priv->pulse_type_hist[priv->mask_idx])
			priv->radar_type = 1;
		else
			priv->radar_type = 0;
		if(priv->pshare->rf_ft_var.dfs_radar_diff_on && priv->pshare->rf_ft_var.dfs_det_off == 0) {
			if(priv->pulse_flag_hist[priv->mask_idx])
				dfs_radar_distinguish_for_8812f(priv,dm);
		}
	}
#endif

	//max_sht_pusle_cnt_th =  phy_query_bb_reg(priv, 0x920, 0x000f0000)-1; //read 920[19:16]
	max_sht_pusle_cnt_th = priv->pshare->rf_ft_var.dfs_max_sht_pusle_cnt_th;
	if (priv->pshare->rf_ft_var.dfs_det_print3)
		panic_printk("max_sht_pusle_cnt_th = %d\n", max_sht_pusle_cnt_th);

	if(priv->idle_flag == 1){		
		if(priv->pshare->rf_ft_var.dfs_psd_idle_on == 1){
			SAVE_INT_AND_CLI(flags);
			SMP_LOCK(flags);
			Scan_BB_PSD(ODMPTR, PSD_report_right, PSD_report_left, 20, 0x3e);	
			SMP_UNLOCK(flags);
			RESTORE_INT(flags);
		}
		else{
			/*for (i=0; i<20; i++) {
				PSD_report_right[i] = 0;
				PSD_report_left[i] = 0;
			}*/
		}		
	}
	else{
		if(priv->pshare->rf_ft_var.dfs_psd_TP_on == 1){
			SAVE_INT_AND_CLI(flags);
			SMP_LOCK(flags);
			Scan_BB_PSD(ODMPTR, PSD_report_right, PSD_report_left, 20, 0x3e);	
			SMP_UNLOCK(flags);
			RESTORE_INT(flags);
		}
		else{
			for (i=0; i<20; i++) {
				PSD_report_right[i] = 0;
				PSD_report_left[i] = 0;
			}
		}
	}
	
	for (i=0; i<20; i++) {
		PSD_report_right[i] = (-110 + 0x3e) - 39 + PSD_report_right[i];
		PSD_report_left[i] = (-110 + 0x3e) - 39 + PSD_report_left[i];
	}
	if (priv->pshare->rf_ft_var.dfs_det_print_psd) {
		panic_printk("PSD right: ");
		for (i=0; i<20; i++)
			panic_printk("%d ", PSD_report_right[i]);
		//panic_printk("\n");
		panic_printk("	 PSD left: ");
		for (i=0; i<20; i++)
			panic_printk("%d ", PSD_report_left[i]);
		panic_printk("\n");
	}

	fault_flag_det = 0;
	fault_flag_psd = 0;
	fa_flag = 0;
	// ETSI
	if(priv->pshare->rf_ft_var.manual_dfs_regdomain == 3){
		fa_mask_mid_th = priv->pshare->rf_ft_var.dfs_fa_cnt_mid + 20;
		fa_mask_lower_th = priv->pshare->rf_ft_var.dfs_fa_cnt_lower + 40;
	}
	else{
		fa_mask_mid_th = priv->pshare->rf_ft_var.dfs_fa_cnt_mid;
		fa_mask_lower_th = priv->pshare->rf_ft_var.dfs_fa_cnt_lower;
	}
	//if (max_fa_in_hist >= fa_mask_mid_th || total_fa_in_hist >= priv->pshare->rf_ft_var.dfs_fa_hist || pre_post_now_acc_fa_in_hist >= fa_mask_mid_th){		
		/*if (priv->mask_idx >= priv->pshare->rf_ft_var.dfs_det_flag_offset)			
			index = priv->mask_idx - priv->pshare->rf_ft_var.dfs_det_flag_offset;		
		else			
			index = priv->pshare->rf_ft_var.dfs_det_hist_len + priv->mask_idx - priv->pshare->rf_ft_var.dfs_det_flag_offset;	*/	
	/*	priv->radar_det_mask_hist[index] = 1;		
		if (priv->pulse_flag_hist[index] == 1){ 		
			priv->pulse_flag_hist[index] = 0;			
			if (priv->pshare->rf_ft_var.dfs_det_print4){				
				panic_printk("Radar is masked : FA mask case3\n");						
			}		
		}				
		fa_flag = 1;	
	}
	if (((FA_count_inc >= priv->pshare->rf_ft_var.dfs_dpt_fa_th_upper) && (short_pulse_cnt_inc > max_sht_pusle_cnt_th)) ||
		(priv->ini_gain_cur >= priv->pshare->rf_ft_var.dpt_ini_gain_th)) {
		st_L2H_new = priv->pshare->rf_ft_var.dfs_dpt_st_l2h_max;
		if (priv->pshare->rf_ft_var.dfs_det_print3)
			panic_printk("[1] st_L2H_new %x\n", st_L2H_new);
		priv->radar_det_mask_hist[priv->mask_idx] = 1;
		if (priv->pulse_flag_hist[priv->mask_idx] == 1){
			priv->pulse_flag_hist[priv->mask_idx] = 0;
			if (priv->pshare->rf_ft_var.dfs_det_print4){
				panic_printk("Radar is masked : FA mask case1\n");			
			}
		}
		fa_flag = 1;
	}
	else if (((FA_count_inc >= fa_mask_mid_th) ||
				((FA_count_inc >= fa_mask_lower_th) &&
					(FA_count_inc >= priv->pshare->rf_ft_var.dfs_fa_cnt_inc_ratio * priv->FA_count_inc_pre))) 
		&& (short_pulse_cnt_inc > max_sht_pusle_cnt_th)) {
		if (priv->pshare->rf_ft_var.dfs_dpt_st_l2h_add)
			st_L2H_new += 2;
		if (priv->pshare->rf_ft_var.dfs_det_print3)
			panic_printk("[2] st_L2H_new %x\n", st_L2H_new);
		priv->radar_det_mask_hist[priv->mask_idx] = 1;
		if (priv->pulse_flag_hist[priv->mask_idx] == 1){
			priv->pulse_flag_hist[priv->mask_idx] = 0;
			if (priv->pshare->rf_ft_var.dfs_det_print4){
				panic_printk("Radar is masked : FA mask case2\n");			
			}
		}
		fa_flag = 1;
	}
	else 
	{
		if (((FA_CRCOK_ratio > priv->pshare->rf_ft_var.dfs_fa_ratio_th) &&
				(FA_count_inc >= priv->pshare->rf_ft_var.dfs_fa_cnt_lower) &&
				(Total_CRC_OK_cnt_inc > priv->pshare->rf_ft_var.dfs_crc32_cnt_lower)) ||
			((short_pulse_cnt_inc <= max_sht_pusle_cnt_th) && (short_pulse_cnt_inc != 0) &&
				(priv->idle_flag == 1) && (priv->pshare->rf_ft_var.manual_dfs_regdomain == 2)) ||
			(FA_count_inc >= priv->pshare->rf_ft_var.dfs_fa_cnt_upper))
			priv->radar_det_mask_hist[priv->mask_idx] = 1;
		else
			priv->radar_det_mask_hist[priv->mask_idx] = 0;
	}
*/
	priv->radar_det_mask_hist[priv->mask_idx] = 0;

		if ((dm->support_ability & ODM_BB_ENV_MONITOR)){	
		if ((100-((ccx->nhm_rpt_sum * 100) >> 8)) >= priv->pshare->rf_ft_var.dfs_nhm_duty_cycle_th) {
		if (priv->pulse_flag_hist[index] == 1) {
			priv->pulse_flag_hist[index] = 0;
			if (priv->pshare->rf_ft_var.dfs_det_print4) {
				panic_printk("Radar is masked : NHM duty cycle over TH\n");
			}
		}
	}
		}
	if (priv->pshare->rf_ft_var.dfs_det_print2) {
		panic_printk("mask_idx: %d\n", priv->mask_idx);
		panic_printk("radar_det_mask_hist: ");
		for (i=0; i<priv->pshare->rf_ft_var.dfs_det_hist_len; i++)
			panic_printk("%d ", priv->radar_det_mask_hist[i]);
		panic_printk("pulse_flag_hist: ");
		for (i=0; i<priv->pshare->rf_ft_var.dfs_det_hist_len; i++)
			panic_printk("%d ", priv->pulse_flag_hist[i]);
		panic_printk("fa_inc_hist: ");		
		for (i=0; i<5; i++) 		
			panic_printk("%d ", priv->fa_inc_hist[i]);		
		panic_printk("\n"); 	
		panic_printk("max_fa_in_hist: %d pre_post_now_acc_fa_in_hist: %d ",max_fa_in_hist,pre_post_now_acc_fa_in_hist);
	}

	/*memcpy(&priv->PSD_report_right[priv->mask_idx][0], PSD_report_right, 20*sizeof(int));
	memcpy(&priv->PSD_report_left[priv->mask_idx][0], PSD_report_left, 20*sizeof(int));

	for (i=0; i<20; i++) {
		priv->max_hold_right[i] = -1000;
		priv->max_hold_left[i] = -1000;
	}

	for (k=0; k<5; k++) {
		for (i=0; i<20; i++) {
			if (priv->PSD_report_right[k][i] > priv->max_hold_right[i])
				priv->max_hold_right[i] = priv->PSD_report_right[k][i];
			if (priv->PSD_report_left[k][i] > priv->max_hold_left[i])
				priv->max_hold_left[i] = priv->PSD_report_left[k][i];
		}
	}

	if ((priv->pshare->rf_ft_var.dfs_psd_op == 1) && (priv->idle_flag == 0))
	{
		int right_index_start, left_index_end;
		int avg_1 = (priv->max_hold_right[0]+priv->max_hold_right[1]+priv->max_hold_right[2])/3;
		int avg_2 = (priv->max_hold_right[17]+priv->max_hold_right[18]+priv->max_hold_right[19])/3;
		max_right = -1000;
		if(RTL_ABS(avg_1, avg_2) <= 3)
			right_index_start=2;
		else
			right_index_start=11;								
		for (i=right_index_start; i<20; i++) {
				if ((i != 10) && (max_right < priv->max_hold_right[i]))
					max_right = priv->max_hold_right[i];
		}

		avg_1 = (priv->max_hold_left[0]+priv->max_hold_left[1]+priv->max_hold_left[2])/3;
		avg_2 = (priv->max_hold_left[17]+priv->max_hold_left[18]+priv->max_hold_left[19])/3;
		max_left = -1000;
		if (RTL_ABS(avg_1, avg_2) <= 3)
			left_index_end=20;								
		else
			left_index_end=8;		
		for (i=0; i<left_index_end; i++) {
			if ((i != 10) && (max_left < priv->max_hold_left[i]))
				max_left = priv->max_hold_left[i];
		}
		if(max_right == 0){
			max_right = -99;
		}
		if(max_left == 0){
			max_left = -99;
		}
	}
	else
	{
		max_right = -1000;
		for (i=2; i<20; i++) {
			if ((i != 10) && (max_right < priv->max_hold_right[i]))
					max_right = priv->max_hold_right[i];
		}
		max_left = -1000;
		for (i=0; i<20; i++) {
			if ((i != 10) && (max_left < priv->max_hold_left[i]))
				max_left = priv->max_hold_left[i];
		}
		if(max_right == 0){
			max_right = -99;
		}
		if(max_left == 0){
			max_left = -99;
		}
	}
*/

	max_right = -99;
	max_left = -99;
	if (priv->pshare->rf_ft_var.dfs_det_print)
		panic_printk("max_right %d, max_left %d\n", max_right, max_left);

	/*if (priv->mask_idx >= priv->pshare->rf_ft_var.dfs_det_flag_offset)
		index = priv->mask_idx - priv->pshare->rf_ft_var.dfs_det_flag_offset;
	else
		index = priv->pshare->rf_ft_var.dfs_det_hist_len + priv->mask_idx - priv->pshare->rf_ft_var.dfs_det_flag_offset;*/
		
	// use PSD detection result
	if ((max_right > (0-(int)priv->pshare->rf_ft_var.dfs_psd_pw_th)) || (max_left > (0-(int)priv->pshare->rf_ft_var.dfs_psd_pw_th))) {
		if (priv->pshare->rf_ft_var.dfs_dpt_st_l2h_add) {
			st_L2H_tmp = 110 + ((max_right > max_left)? max_right:max_left) - priv->pshare->rf_ft_var.dfs_psd_fir_decay;
			if (st_L2H_tmp > priv->st_L2H_cur)
				st_L2H_new = st_L2H_tmp;
		}
		if (priv->pshare->rf_ft_var.dfs_det_print3)
			panic_printk("[3] st_L2H_new %x\n", st_L2H_new);

		if (priv->pshare->rf_ft_var.dfs_det_print)
			panic_printk("st_L2H_cur %x pwdb_th %x\n", st_L2H_new, priv->pwdb_th);

		fault_flag_psd = 1;
		if (priv->pulse_flag_hist[index] == 1){
			priv->pulse_flag_hist[index] = 0;
			if (priv->pshare->rf_ft_var.dfs_det_print4){
				panic_printk("Radar is masked : PSD mask\n");			
			}
		}
		priv->radar_det_mask_hist[index] = 1;
	}

	sum = 0;
	for (k=0; k<priv->pshare->rf_ft_var.dfs_det_hist_len; k++) {
		if (priv->radar_det_mask_hist[k] == 1)
			sum++;
	}

	if (priv->mask_hist_checked <= priv->pshare->rf_ft_var.dfs_det_hist_len)
		priv->mask_hist_checked++;

	//only the decision result of short pulse needs to refer the FA decision results
	if ((priv->mask_hist_checked >= priv->pshare->rf_ft_var.dfs_det_hist_len) &&
		//(DFS_tri_long_pulse || DFS_tri_short_pulse) &&
		priv->pulse_flag_hist[index])
	{		
		//if ((sum <= priv->pshare->rf_ft_var.dfs_det_sum_th) &&
		//	(fa_flag == 0))
		if (sum <= priv->pshare->rf_ft_var.dfs_det_sum_th) 
		{
//#if defined(CONFIG_WLAN_HAL_8814AE) || defined(CONFIG_WLAN_HAL_8822BE)
			if(((GET_CHIP_VER(priv) == VERSION_8814A) || (GET_CHIP_VER(priv) == VERSION_8822B)) &&
				priv->pshare->rf_ft_var.dfs_radar_diff_on){
				if (priv->pulse_type_hist[index] == 0){
					priv->radar_type = 0;
					if (priv->pulsewidth_flag && priv->pulseinterval_flag && priv->pri_type3_4_flag){
						priv->pmib->dot11DFSEntry.DFS_detected = 1 ; // DFS detect
						if (priv->pshare->rf_ft_var.dfs_det_print4){
							panic_printk("hist_idx= %d\n", (priv->hist_idx+3)%4);
							for (j=0;j<4;j++){
								for (i=0;i<6;i++){
									panic_printk("pluseinterval_hold = %d ", priv->pluseinterval_hold[j][i]);
								}
								panic_printk("\n");
							}			
							panic_printk("\n");
							for (j=0;j<4;j++){
								for (i=0;i<6;i++){
									panic_printk("plusewidth_hold = %d ", priv->plusewidth_hold[j][i]);
								}
								panic_printk("\n");
							}			
							panic_printk("\n");
							panic_printk("idle_flag = %d\n",priv->idle_flag);
							panic_printk("plusewidth_hold_sum = %d %d %d %d %d %d\n",
					priv->plusewidth_hold_sum[0], priv->plusewidth_hold_sum[1],priv->plusewidth_hold_sum[2],priv->plusewidth_hold_sum[3],priv->plusewidth_hold_sum[4],priv->plusewidth_hold_sum[5]);
							panic_printk("pluseinterval_hold_sum = %d %d %d %d %d %d\n",
					priv->pluseinterval_hold_sum[0], priv->pluseinterval_hold_sum[1],priv->pluseinterval_hold_sum[2],priv->pluseinterval_hold_sum[3],priv->pluseinterval_hold_sum[4],priv->pluseinterval_hold_sum[5]);
						}
					}
					else{
						if(priv->pshare->rf_ft_var.dfs_det_print4){
							if (priv->pulse_flag_hist[index] && (priv->pulseinterval_flag==0)){
								panic_printk("PRI criterion is not satisfied!\n");
								panic_printk("pulseinterval_variation = %d\n",priv->pri_std);
								if(priv->pri_cond1 == 0)
									panic_printk("pri_cond1 is not satisfied!\n");
								if(priv->pri_cond2 == 0)
									panic_printk("pri_cond2 is not satisfied!\n");
								if(priv->pri_cond3 == 0){
									panic_printk("pri_cond3 is not satisfied!\n");
								}
								if(priv->pri_cond4 == 0)
									panic_printk("pri_cond4 is not satisfied!\n");
								if(priv->pri_cond5 == 0)
									panic_printk("pri_cond5 is not satisfied!\n");
							}
							if (priv->pulse_flag_hist[index] && (priv->pulsewidth_flag==0)){
								panic_printk("PW criterion is not satisfied!\n");
								panic_printk("pulsewidth_variation = %d\n",priv->pw_std);
								if(priv->pw_cond1 == 0)
									panic_printk("pw_cond1 is not satisfied!\n");
								if(priv->pw_cond2 == 0)
									panic_printk("pw_cond2 is not satisfied!\n");
								if(priv->pw_cond3 == 0){
									panic_printk("pw_cond3 is not satisfied!\n");
								}
							}
							if (priv->pulse_flag_hist[index] && (priv->pri_type3_4_flag==0)){
								panic_printk("pri_type3_4 criterion is not satisfied!\n");
								if(priv->pri_type3_4_cond1 == 0)
									panic_printk("pri_type3_4_cond1 is not satisfied!\n");
								if(priv->pri_type3_4_cond2 == 0)
									panic_printk("pri_type3_4_cond2 is not satisfied!\n");
							}
							panic_printk("hist_idx= %d\n", (priv->hist_idx+3)%4);
							for (j=0;j<4;j++){
								for (i=0;i<6;i++){
									panic_printk("pluseinterval_hold = %d ", priv->pluseinterval_hold[j][i]);
								}
								panic_printk("\n");
							}			
							panic_printk("\n");
							for (j=0;j<4;j++){
								for (i=0;i<6;i++){
									panic_printk("plusewidth_hold = %d ", priv->plusewidth_hold[j][i]);
								}
								panic_printk("\n");
							}			
							panic_printk("\n");
							panic_printk("idle_flag = %d\n",priv->idle_flag);
							panic_printk("plusewidth_hold_sum = %d %d %d %d %d %d\n",
					priv->plusewidth_hold_sum[0], priv->plusewidth_hold_sum[1],priv->plusewidth_hold_sum[2],priv->plusewidth_hold_sum[3],priv->plusewidth_hold_sum[4],priv->plusewidth_hold_sum[5]);
							panic_printk("pluseinterval_hold_sum = %d %d %d %d %d %d\n",
					priv->pluseinterval_hold_sum[0], priv->pluseinterval_hold_sum[1],priv->pluseinterval_hold_sum[2],priv->pluseinterval_hold_sum[3],priv->pluseinterval_hold_sum[4],priv->pluseinterval_hold_sum[5]);
						}				
					}			
				}
				else{
					priv->radar_type = 1;
					/* Long radar should satisfy three conditions */
					if(priv->long_radar_flag == 1) {						
						priv->pmib->dot11DFSEntry.DFS_detected = 1 ; // DFS detect
						if (priv->pshare->rf_ft_var.dfs_det_print4) {
							panic_printk("\n");
							panic_printk("idle_flag = %d\n",priv->idle_flag);
							panic_printk("long_radar_plusewidth_hold_sum = %d %d %d %d %d %d\n",
							priv->plusewidth_long_hold_sum[0], priv->plusewidth_long_hold_sum[1],priv->plusewidth_long_hold_sum[2],priv->plusewidth_long_hold_sum[3],priv->plusewidth_long_hold_sum[4],priv->plusewidth_long_hold_sum[5]);
							panic_printk("long_radar_pluseinterval_hold_sum = %d %d %d %d %d %d\n",
							priv->pluseinterval_long_hold_sum[0], priv->pluseinterval_long_hold_sum[1],priv->pluseinterval_long_hold_sum[2],priv->pluseinterval_long_hold_sum[3],priv->pluseinterval_long_hold_sum[4],priv->pluseinterval_long_hold_sum[5]);
							panic_printk("\n");
						}
					} else {
						if (priv->pshare->rf_ft_var.dfs_det_print4){
							if(priv->pw_long_cond1 == 0)
								panic_printk("--pw_long_cond1 is not satisfied!--\n");
							if(priv->pw_long_cond2 == 0)
								panic_printk("--pw_long_cond2 is not satisfied!--\n");
							if(priv->pri_long_cond1 == 0)
								panic_printk("--pri_long_cond1 is not satisfied!--\n");
						}
					}
				}			
				
			}
#if defined(CONFIG_WLAN_HAL_8814BE)			
			else if((GET_CHIP_VER(priv) == VERSION_8814B) && priv->pshare->rf_ft_var.dfs_radar_diff_on) {
				if (priv->pulse_type_hist[priv->mask_idx] == 0) {
					priv->radar_type = 0;
					if (priv->pulsewidth_flag && priv->pulseinterval_flag && priv->loct_flag && !(priv->nhm_dfs_flag)) {
						priv->pmib->dot11DFSEntry.DFS_detected = 1 ; // DFS detect
						if (priv->pshare->rf_ft_var.dfs_det_print4) {
							panic_printk("Short radar pulse is detected\n");
							panic_printk("pw_rpt(1-8) = [%d, %d, %d, %d]\n",
										priv->pw_14b_rslt[0], priv->pw_14b_rslt[1], priv->pw_14b_rslt[2], priv->pw_14b_rslt[3]);
							panic_printk("loct_rpt(1-8) = [%d, %d, %d, %d, %d, %d, %d, %d]\n",
										priv->loct_14b_rslt[0], priv->loct_14b_rslt[1], priv->loct_14b_rslt[2], priv->loct_14b_rslt[3],
										priv->loct_14b_rslt[4], priv->loct_14b_rslt[5], priv->loct_14b_rslt[6], priv->loct_14b_rslt[7]);
							panic_printk("pri information = %d\n", priv->pri_14b_rslt);
							panic_printk("\n");
						}
					} else {
						if(priv->pshare->rf_ft_var.dfs_det_print4) {
							panic_printk("Suspected short radar result is dropped by SW mechanism\n");
							if (priv->pulse_flag_hist[index] && (priv->pulsewidth_flag==0)) {
								panic_printk("pw_14b_rslt = [%d, %d, %d, %d]\n",
										 priv->pw_14b_rslt[0],priv->pw_14b_rslt[1],priv->pw_14b_rslt[2],priv->pw_14b_rslt[3]);
								panic_printk("legal pw result is not enough\n");
								panic_printk("\n");
							}
							if (priv->pulse_flag_hist[index] && (priv->loct_flag==0)) {
								panic_printk("loct_rpt(1-8) = [%d, %d, %d, %d, %d, %d, %d, %d]\n",
										priv->loct_14b_rslt[0], priv->loct_14b_rslt[1], priv->loct_14b_rslt[2], priv->loct_14b_rslt[3],
										priv->loct_14b_rslt[4], priv->loct_14b_rslt[5], priv->loct_14b_rslt[6], priv->loct_14b_rslt[7]);
								panic_printk("The gap of location result is too much\n");
								panic_printk("\n");
							}
							if (priv->pulse_flag_hist[index] && (priv->pulseinterval_flag==0)) {
								panic_printk("PRI=%d isn't satisfied in the interval!\n",priv->pri_14b_rslt);
								panic_printk("\n");
							}
						}
					}
				}
				else {
					priv->radar_type = 1;
					if (priv->pulsewidth_flag && priv->pulseinterval_flag && priv->loct_flag && !(priv->nhm_dfs_flag))  {						
						priv->pmib->dot11DFSEntry.DFS_detected = 1 ; // DFS detect
						if (priv->pshare->rf_ft_var.dfs_det_print4) {
							panic_printk("Long radar pulse\n");
							panic_printk("pw_rpt(1-4) = [%d, %d, %d, %d]\n",
										priv->pw_14b_rslt[0], priv->pw_14b_rslt[1], priv->pw_14b_rslt[2], priv->pw_14b_rslt[3]);
							panic_printk("loct_rpt(1-8) = [%d, %d, %d, %d, %d, %d, %d, %d]\n",
										priv->loct_14b_rslt[0], priv->loct_14b_rslt[1], priv->loct_14b_rslt[2], priv->loct_14b_rslt[3],
										priv->loct_14b_rslt[4], priv->loct_14b_rslt[5], priv->loct_14b_rslt[6], priv->loct_14b_rslt[7]);
							panic_printk("pri information = %d\n", priv->pri_14b_rslt);
							panic_printk("\n");
						}
					} else {
						if (priv->pshare->rf_ft_var.dfs_det_print4) {
							panic_printk("Suspected long radar result is dropped by SW mechanism\n");
							if (priv->pulse_flag_hist[index] && (priv->pulsewidth_flag==0)) {
								panic_printk("pw_rpt(1-4) = [%d, %d, %d, %d]\n",
										priv->pw_14b_rslt[0], priv->pw_14b_rslt[1], priv->pw_14b_rslt[2], priv->pw_14b_rslt[3]);
								panic_printk("None of PWs are in the interval\n");
								panic_printk("\n");
							}
							if (priv->pulse_flag_hist[index] && (priv->loct_flag==0)) {
								panic_printk("loct_rpt(1-8) = [%d, %d, %d, %d, %d, %d, %d, %d]\n",
										priv->loct_14b_rslt[0], priv->loct_14b_rslt[1], priv->loct_14b_rslt[2], priv->loct_14b_rslt[3],
										priv->loct_14b_rslt[4], priv->loct_14b_rslt[5], priv->loct_14b_rslt[6], priv->loct_14b_rslt[7]);
								panic_printk("The gap of location result is too much\n");
								panic_printk("\n");
							}
							if (priv->pulse_flag_hist[index] && (priv->pulseinterval_flag==0)) {
								panic_printk("PRI = %d, which is not zero!\n",priv->pri_14b_rslt);
								panic_printk("\n");
							}
						}
					}
				}
			}
#endif
#if defined(CONFIG_WLAN_HAL_8812FE)
			else if((GET_CHIP_VER(priv) == VERSION_8812F) && priv->pshare->rf_ft_var.dfs_radar_diff_on) {
				if (priv->pulse_type_hist[priv->mask_idx] == 0) {
					priv->radar_type = 0;
					if (priv->pulsewidth_flag && priv->pulseinterval_flag && priv->loct_flag && !(priv->nhm_dfs_flag)) {
						priv->pmib->dot11DFSEntry.DFS_detected = 1 ; // DFS detect
						if (priv->pshare->rf_ft_var.dfs_det_print4) {
							panic_printk("Short radar pulse is detected\n");
							panic_printk("\n");
						}
					} else {
						if(priv->pshare->rf_ft_var.dfs_det_print4) {
							panic_printk("Suspected short radar result is dropped by SW mechanism\n");
							panic_printk("\n");
						}
					}
				}
				else {
					priv->radar_type = 1;
					if(priv->pulsewidth_flag && priv->pulseinterval_flag && priv->loct_flag && !(priv->nhm_dfs_flag)) {						
						priv->pmib->dot11DFSEntry.DFS_detected = 1 ; // DFS detect
						if (priv->pshare->rf_ft_var.dfs_det_print4) {
							panic_printk("Long radar pulse is detected\n");
							panic_printk("\n");
						}
					} else {
						if (priv->pshare->rf_ft_var.dfs_det_print4) {
							panic_printk("Suspected long radar result is dropped by SW mechanism\n");
							panic_printk("\n");
						}
					}
					}
				}
			
#endif			
			else
			{
				if (priv->pulse_type_hist[index] == 0)
					priv->radar_type = 0;
				else if (priv->pulse_type_hist[index] == 1)
					priv->radar_type = 1;				
				priv->pmib->dot11DFSEntry.DFS_detected = 1 ; // DFS detect
			}
		}
		else {
			fault_flag_det = 1;
			if (priv->pshare->rf_ft_var.dfs_det_print4){
				panic_printk("Radar is masked : mask_hist large than thd\n");			
			}
		}
	}

	priv->mask_idx++;
	if (priv->mask_idx == priv->pshare->rf_ft_var.dfs_det_hist_len)
		priv->mask_idx = 0;

	if ((fault_flag_det == 0) && (fault_flag_psd == 0) && (fa_flag ==0)) {
		//if (total_pulse_count_inc < priv->pshare->rf_ft_var.dfs_dpt_pulse_th_lower) {
		//	// limit the ST value to absoulte lower bound 0x1c
		//	st_L2H_new -= 4;
		//	if (priv->pshare->rf_ft_var.dfs_det_print3)
		//		panic_printk("[4] st_L2H_new %x\n", st_L2H_new);
		//}
		///*else if (total_pulse_count_inc < priv->pshare->rf_ft_var.dfs_dpt_pulse_th_mid) {
		//	// limit the ST value to absoulte lower bound 0x1c
		//	st_L2H_new -= 2;
		//	if (priv->pshare->rf_ft_var.dfs_det_print3)
		//		panic_printk("[5] st_L2H_new %x\n", st_L2H_new);
		//}*/
		//else if (ini_gain_inc < 0){
		//	st_L2H_new += ini_gain_inc;
		//	if (priv->pshare->rf_ft_var.dfs_det_print3)
		//		panic_printk("[6] st_L2H_new %x\n", st_L2H_new);
		//}
		///*else if ((total_pulse_count_inc < max_sht_pusle_cnt_th) &&
		//		 (((priv->ext_stats.tx_avarage+priv->ext_stats.rx_avarage)>>17) <= priv->pshare->rf_ft_var.dfs_psd_tp_th)) {
		//	st_L2H_new += priv->pshare->rf_ft_var.dfs_dpt_st_l2h_idle_th;
		//}*/
		if (priv->ini_gain_cur < priv->pshare->rf_ft_var.dpt_ini_gain_th) {
			if ((priv->idle_flag == 1) && (priv->pshare->rf_ft_var.manual_dfs_regdomain == 2))
				st_L2H_new = priv->pshare->rf_ft_var.dfs_dpt_st_l2h_min + priv->pshare->rf_ft_var.dfs_dpt_st_l2h_idle_offset;
			else
				st_L2H_new = priv->pshare->rf_ft_var.dfs_dpt_st_l2h_min;
		}
	 }	
	else{
		
		
		if (priv->pshare->rf_ft_var.dfs_dpt_st_l2h_add)
			st_L2H_new += 2;
		if (priv->pshare->rf_ft_var.dfs_det_print3)
			panic_printk("[7] st_L2H_new %x\n", st_L2H_new);
		
		
		if (DFS_tri_short_pulse) {
			//RTL_W32(0x920, RTL_R32(0x920) | (BIT(24) | BIT(28)));
			//RTL_W32(0x920, RTL_R32(0x920) & ~(BIT(24) | BIT(28)));
			//RTL_W32(0x924, RTL_R32(0x924) & ~BIT(15));
			//RTL_W32(0x924, RTL_R32(0x924) | BIT(15));
		}
		if (DFS_tri_long_pulse) {
			//RTL_W32(0x920, RTL_R32(0x920) | (BIT(25) | BIT(28)));
			//RTL_W32(0x920, RTL_R32(0x920) & ~(BIT(25) | BIT(28)));
			//RTL_W32(0x924, RTL_R32(0x924) & ~BIT(15));
			//RTL_W32(0x924, RTL_R32(0x924) | BIT(15));
		}
	}

	if(priv->pmib->dot11nConfigEntry.dot11nUse40M == CHANNEL_WIDTH_20){
		if(priv->pshare->highTP_found_pstat != NULL){
			if(priv->pshare->highTP_found_pstat->rf_info.mimorssi[0] >= 20 && priv->pshare->highTP_found_pstat->rf_info.mimorssi[0] <= 30){
				if (priv->pshare->rf_ft_var.dfs_dpt_st_l2h_add)
				st_L2H_new = priv->pshare->rf_ft_var.dfs_dpt_st_l2h_min - 19 + priv->pshare->highTP_found_pstat->rf_info.mimorssi[0];
		}
	}
	}
	if ((st_L2H_new != priv->st_L2H_cur)) {
		// limit the ST value to absoulte lower bound 0x22
		// limit the ST value to absoulte upper bound 0x4e
		if (st_L2H_new < priv->pshare->rf_ft_var.dfs_dpt_st_l2h_min) {
			if ((priv->idle_flag == 1) && (priv->pshare->rf_ft_var.manual_dfs_regdomain == 2))
				priv->st_L2H_cur = priv->pshare->rf_ft_var.dfs_dpt_st_l2h_min + priv->pshare->rf_ft_var.dfs_dpt_st_l2h_idle_offset;
			else
				priv->st_L2H_cur = priv->pshare->rf_ft_var.dfs_dpt_st_l2h_min;
			//priv->st_L2H_cur = priv->pshare->rf_ft_var.dfs_dpt_st_l2h_min;
		}
		else if (st_L2H_new > priv->pshare->rf_ft_var.dfs_dpt_st_l2h_max)
			priv->st_L2H_cur = priv->pshare->rf_ft_var.dfs_dpt_st_l2h_max;
		else
			priv->st_L2H_cur = st_L2H_new;
		if (((GET_CHIP_VER(priv) == VERSION_8814B) || (GET_CHIP_VER(priv) == VERSION_8812F)))
			phy_set_bb_reg(priv, 0xa40, 0x00007f00, ((priv->st_L2H_cur) << 1));
		else 
			RTL_W8(0x91c, priv->st_L2H_cur);
		// Since The ST_L2H difference between Jgar3 series and others is 6 (6/2=3) 
		if ((GET_CHIP_VER(priv) == VERSION_8814B) || (GET_CHIP_VER(priv) == VERSION_8812F))
			priv->pwdb_th = ((int)priv->st_L2H_cur - (int)priv->ini_gain_cur)/2 + priv->pshare->rf_ft_var.dfs_pwdb_scalar_factor + 3;
		else
			priv->pwdb_th = ((int)priv->st_L2H_cur - (int)priv->ini_gain_cur)/2 + priv->pshare->rf_ft_var.dfs_pwdb_scalar_factor;
		priv->pwdb_th = MAX_NUM(priv->pwdb_th, (int)priv->pshare->rf_ft_var.dfs_pwdb_th); // limit the pwdb value to absoulte lower bound 0x8
		priv->pwdb_th = MIN_NUM(priv->pwdb_th, 0x1f);	 // limit the pwdb value to absoulte upper bound 0x1f
		if ((GET_CHIP_VER(priv) == VERSION_8814B) || (GET_CHIP_VER(priv) == VERSION_8812F)) {
			phy_set_bb_reg(priv, 0xa50, 0x000000f0, priv->pwdb_th);
		} else {
			phy_set_bb_reg(priv, 0x918, 0x00001f00, priv->pwdb_th);
		}
	}

	if (priv->pshare->rf_ft_var.dfs_det_print2) {
		panic_printk("fault_flag_det[%d], fault_flag_psd[%d], DFS_detected [%d]\n",fault_flag_det, fault_flag_psd, priv->pmib->dot11DFSEntry.DFS_detected );
	}
	
	priv->FA_count_inc_pre = FA_count_inc;
#if defined(CONFIG_WLAN_HAL_8814BE)
	if ((GET_CHIP_VER(priv) == VERSION_8814B)) {
		if (priv->seg1_flag == 1) {
			reg_dfsrpt_value = phy_query_bb_reg(priv, 0x2e20, 0xffffffff);
			if ((reg_dfsrpt_value & BIT(20)) || (reg_dfsrpt_value & BIT(28))) {
				odm_set_bb_reg(dm, 0xa6c, BIT(0), 0);
				//panic_printk("\n0xA6C=0\n" );
				odm_set_bb_reg(dm, 0xa6c, BIT(0), 1);
				//panic_printk("\n0xA6C=1\n" );
			}
		} else {
			reg_dfsrpt_value = phy_query_bb_reg(priv, 0x2e00, 0xffffffff);
			if ((reg_dfsrpt_value & BIT(20)) || (reg_dfsrpt_value & BIT(28))) {
				odm_set_bb_reg(dm, 0xa40, BIT(15), 0);
				odm_set_bb_reg(dm, 0xa40, BIT(15), 1);
			}
		}
	} else
#endif
	if ((GET_CHIP_VER(priv) == VERSION_8812F)) {
		reg_dfsrpt_value = phy_query_bb_reg(priv, 0x2e00, 0xffffffff);
		if ((reg_dfsrpt_value & BIT(20)) || (reg_dfsrpt_value & BIT(28))) {
			odm_set_bb_reg(dm, 0xa40, BIT(15), 0);
			odm_set_bb_reg(dm, 0xa40, BIT(15), 1);
		}
	} else {
		if (phy_query_bb_reg(priv, 0xf98, BIT(17)) || phy_query_bb_reg(priv, 0xf98, BIT(19))) {
			RTL_W32(0x924, RTL_R32(0x924) & ~BIT(15));
			RTL_W32(0x924, RTL_R32(0x924) | BIT(15));
		}
	}
	
}



void rtl8192cd_dfs_dynamic_setting(struct rtl8192cd_priv *priv)
{
	unsigned char peak_th_cur=0, short_pulse_cnt_th_cur=0, long_pulse_cnt_th_cur=0, three_peak_opt_cur=0, three_peak_th2_cur=0;
	unsigned char peak_window_cur=0, nb2wb_th_cur=0;
	
	if ((priv->idle_flag == 1)) { // idle (no traffic)
		peak_th_cur = 3;
		short_pulse_cnt_th_cur = 6;
		long_pulse_cnt_th_cur = 9;
		peak_window_cur = 2;
		three_peak_opt_cur = 0;
		three_peak_th2_cur = 2;
		// MKK
		if(priv->pshare->rf_ft_var.manual_dfs_regdomain == 2){
			if ((priv->pmib->dot11RFEntry.dot11channel >= 52) &&
			(priv->pmib->dot11RFEntry.dot11channel <= 64)) {
				short_pulse_cnt_th_cur = priv->pshare->rf_ft_var.dfs_pc0_th_idle_w53;
				long_pulse_cnt_th_cur = 15;
				three_peak_th2_cur = 0;                
			}
			else {
				short_pulse_cnt_th_cur = priv->pshare->rf_ft_var.dfs_pc0_th_idle_w56;
				three_peak_th2_cur = 0;
				long_pulse_cnt_th_cur = 10;
			}
		}
		// FCC
		else if(priv->pshare->rf_ft_var.manual_dfs_regdomain == 1){
			three_peak_th2_cur = 0;
		}
		// ETSI
		else if(priv->pshare->rf_ft_var.manual_dfs_regdomain == 3){
			long_pulse_cnt_th_cur = 15;
			if((priv->ch_120_132_CAC_end == 0) && IS_METEOROLOGY_CHANNEL(priv->pmib->dot11RFEntry.dot11channel)){
				peak_th_cur = 2;
				three_peak_th2_cur = 0;	
				short_pulse_cnt_th_cur = 7;
			}
			else{
				three_peak_th2_cur = 0;	
				short_pulse_cnt_th_cur = 7;
			}
		}
		else{  // default: FCC
		}
	}
	else{ // in service (with TP)
		peak_th_cur = 2;
		short_pulse_cnt_th_cur = 6;
		long_pulse_cnt_th_cur = 7;
		peak_window_cur = 2;
		three_peak_opt_cur = 0;
		three_peak_th2_cur = 2;
		// MKK
		if(priv->pshare->rf_ft_var.manual_dfs_regdomain == 2){
			if ((priv->pmib->dot11RFEntry.dot11channel >= 52) &&
			(priv->pmib->dot11RFEntry.dot11channel <= 64)) {
				long_pulse_cnt_th_cur = 15;
				short_pulse_cnt_th_cur = 5; // for high duty cycle
				three_peak_th2_cur = 0;			
			}
			else {
				three_peak_opt_cur = 0;
				three_peak_th2_cur = 0;
				long_pulse_cnt_th_cur = 8;
			}
		}
		// FCC
		else if(priv->pshare->rf_ft_var.manual_dfs_regdomain == 1){
		}
		// ETSI
		else if(priv->pshare->rf_ft_var.manual_dfs_regdomain == 3){
			long_pulse_cnt_th_cur = 15;
			short_pulse_cnt_th_cur = 5;
				three_peak_opt_cur = 0;
		}
		else{
		}
	}

	if ((GET_CHIP_VER(priv) == VERSION_8814B) || (GET_CHIP_VER(priv) == VERSION_8812F)) {
		if ((priv->idle_flag == 1)) {
			short_pulse_cnt_th_cur = 8;	// Real cnt in 8812F
			long_pulse_cnt_th_cur = 6;	// Real cnt in 8812F
			// [8812F] a50[23:20] = N => The short_pulse_cnt_th is N+2
			short_pulse_cnt_th_cur = short_pulse_cnt_th_cur - 2;
			long_pulse_cnt_th_cur = long_pulse_cnt_th_cur - 2;
		} else {
			if (GET_CHIP_VER(priv) == VERSION_8812F) {
				short_pulse_cnt_th_cur = 6;	// Real cnt in 8812f
				long_pulse_cnt_th_cur = 4;	// Real cnt in 8812f
			}
			else {
				short_pulse_cnt_th_cur = 8;	// Real cnt in 8814b
				long_pulse_cnt_th_cur = 6;	// Real cnt in 8814b
			}
			// In 8812F, a50[23:20] = N => The short_pulse_cnt_th is N+2
			short_pulse_cnt_th_cur = short_pulse_cnt_th_cur - 2;
			long_pulse_cnt_th_cur = long_pulse_cnt_th_cur - 2;
		}	
	}

	
	if ((GET_CHIP_VER(priv) == VERSION_8814B) || (GET_CHIP_VER(priv) == VERSION_8812F)) {
		if((priv->peak_th != peak_th_cur)){		
			phy_set_bb_reg(priv, 0xa48, 0x00c00000, peak_th_cur);
		}
		if((priv->short_pulse_cnt_th != short_pulse_cnt_th_cur)){		
			phy_set_bb_reg(priv, 0xa50, 0x00f00000, short_pulse_cnt_th_cur);
		}
		if((priv->long_pulse_cnt_th != long_pulse_cnt_th_cur)){		
			phy_set_bb_reg(priv, 0xa4c, 0xf0000000, long_pulse_cnt_th_cur);
		}
		if((priv->peak_window != peak_window_cur)){		
			phy_set_bb_reg(priv, 0xa40, 0x00030000, peak_window_cur);
		}
		/*if((priv->three_peak_opt != three_peak_opt_cur)){		
			phy_set_bb_reg(priv, 0xa40, 0x30000000, three_peak_opt_cur);
		}*/
		if((priv->three_peak_th2 != three_peak_th2_cur)){		
			phy_set_bb_reg(priv, 0xa44, 0x00000007, three_peak_th2_cur);
		}
	} else {
		if((priv->peak_th != peak_th_cur)){		
			phy_set_bb_reg(priv, 0x918, 0x00030000, peak_th_cur);
		}
		if((priv->short_pulse_cnt_th != short_pulse_cnt_th_cur)){		
			phy_set_bb_reg(priv, 0x920, 0x000f0000, short_pulse_cnt_th_cur);
		}
		if((priv->long_pulse_cnt_th != long_pulse_cnt_th_cur)){		
			phy_set_bb_reg(priv, 0x920, 0x00f00000, long_pulse_cnt_th_cur);
		}
		if((priv->peak_window != peak_window_cur)){		
			phy_set_bb_reg(priv, 0x920, 0x00000300, peak_window_cur);
		}
		if((priv->nb2wb_th != nb2wb_th_cur)){		
			phy_set_bb_reg(priv, 0x920, 0x0000e000, nb2wb_th_cur);
		}
		if((priv->three_peak_opt != three_peak_opt_cur)){		
			phy_set_bb_reg(priv, 0x924, 0x00000180, three_peak_opt_cur);
		}
		if((priv->three_peak_th2 != three_peak_th2_cur)){		
			phy_set_bb_reg(priv, 0x924, 0x00007000, three_peak_th2_cur);
		}
	}
	
    	priv->peak_th = peak_th_cur;
	priv->short_pulse_cnt_th = short_pulse_cnt_th_cur;
	priv->long_pulse_cnt_th = long_pulse_cnt_th_cur;
	priv->peak_window = peak_window_cur;
	priv->nb2wb_th = nb2wb_th_cur;
	priv->three_peak_opt = three_peak_opt_cur;
	priv->three_peak_th2 = three_peak_th2_cur;
}

#endif

