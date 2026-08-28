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

/*Image2HeaderVersion: R3 1.5.3*/
#if (RTL8197F_SUPPORT == 1)
#ifndef __INC_MP_RF_HW_IMG_8197F_H
#define __INC_MP_RF_HW_IMG_8197F_H

/* Please add following compiler flags definition (#define CONFIG_XXX_DRV_DIS)
 * into driver source code to reduce code size if necessary.
 * #define CONFIG_8197F_DRV_DIS
 * #define CONFIG_8197F_TYPE0_DRV_DIS
 * #define CONFIG_8197F_TYPE1_DRV_DIS
 * #define CONFIG_8197F_TYPE2_DRV_DIS
 * #define CONFIG_8197F_TYPE3_DRV_DIS
 * #define CONFIG_8197F_TYPE4_DRV_DIS
 * #define CONFIG_8197F_TYPE5_DRV_DIS
 * #define CONFIG_8197F_TYPE6_DRV_DIS
 * #define CONFIG_8197F_TYPE7_DRV_DIS
 */

#define CONFIG_8197F
#ifdef CONFIG_8197F_DRV_DIS
    #undef CONFIG_8197F
#endif

#define CONFIG_8197F_TYPE0
#ifdef CONFIG_8197F_TYPE0_DRV_DIS
    #undef CONFIG_8197F_TYPE0
#endif

#define CONFIG_8197F_TYPE1
#ifdef CONFIG_8197F_TYPE1_DRV_DIS
    #undef CONFIG_8197F_TYPE1
#endif

#define CONFIG_8197F_TYPE2
#ifdef CONFIG_8197F_TYPE2_DRV_DIS
    #undef CONFIG_8197F_TYPE2
#endif

#define CONFIG_8197F_TYPE3
#ifdef CONFIG_8197F_TYPE3_DRV_DIS
    #undef CONFIG_8197F_TYPE3
#endif

#define CONFIG_8197F_TYPE4
#ifdef CONFIG_8197F_TYPE4_DRV_DIS
    #undef CONFIG_8197F_TYPE4
#endif

#define CONFIG_8197F_TYPE5
#ifdef CONFIG_8197F_TYPE5_DRV_DIS
    #undef CONFIG_8197F_TYPE5
#endif

#define CONFIG_8197F_TYPE6
#ifdef CONFIG_8197F_TYPE6_DRV_DIS
    #undef CONFIG_8197F_TYPE6
#endif

#define CONFIG_8197F_TYPE7
#ifdef CONFIG_8197F_TYPE7_DRV_DIS
    #undef CONFIG_8197F_TYPE7
#endif

/******************************************************************************
 *                           radioa.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8197f_radioa(struct dm_struct *dm);
u32 odm_get_version_mp_8197f_radioa(void);

/******************************************************************************
 *                           radiob.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8197f_radiob(struct dm_struct *dm);
u32 odm_get_version_mp_8197f_radiob(void);

/******************************************************************************
 *                           txpowertrack.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8197f_txpowertrack(struct dm_struct *dm);
u32 odm_get_version_mp_8197f_txpowertrack(void);

/******************************************************************************
 *                           txpowertrack_type0.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8197f_txpowertrack_type0(struct dm_struct *dm);
u32 odm_get_version_mp_8197f_txpowertrack_type0(void);

/******************************************************************************
 *                           txpowertrack_type1.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8197f_txpowertrack_type1(struct dm_struct *dm);
u32 odm_get_version_mp_8197f_txpowertrack_type1(void);

/******************************************************************************
 *                           txpowertrack_type2.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8197f_txpowertrack_type2(struct dm_struct *dm);
u32 odm_get_version_mp_8197f_txpowertrack_type2(void);

/******************************************************************************
 *                           txpowertrack_type3.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8197f_txpowertrack_type3(struct dm_struct *dm);
u32 odm_get_version_mp_8197f_txpowertrack_type3(void);

/******************************************************************************
 *                           txpowertrack_type4.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8197f_txpowertrack_type4(struct dm_struct *dm);
u32 odm_get_version_mp_8197f_txpowertrack_type4(void);

/******************************************************************************
 *                           txpowertrack_type5.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8197f_txpowertrack_type5(struct dm_struct *dm);
u32 odm_get_version_mp_8197f_txpowertrack_type5(void);

/******************************************************************************
 *                           txpowertrack_type6.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8197f_txpowertrack_type6(struct dm_struct *dm);
u32 odm_get_version_mp_8197f_txpowertrack_type6(void);

/******************************************************************************
 *                           txpowertrack_type7.TXT
 ******************************************************************************/

/* tc: Test Chip, mp: mp Chip*/
void
odm_read_and_config_mp_8197f_txpowertrack_type7(struct dm_struct *dm);
u32 odm_get_version_mp_8197f_txpowertrack_type7(void);

#endif
#endif /* end of HWIMG_SUPPORT*/

