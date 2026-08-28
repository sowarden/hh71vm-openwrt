/******************************************************************************
 *
 * Copyright(c) 2016 - 2017 Realtek Corporation. All rights reserved.
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
 ******************************************************************************/

#ifndef __HALMAC__HW_CFG_H__
#define __HALMAC__HW_CFG_H__


#if 1
#ifndef BIT
    #define BIT(x)		(1 << (x))
#endif
#endif


/* Interface define*/

#ifdef CONFIG_WLAN_HAL_8881A
#define HALMAC_8881A_SUPPORT    1
#else
#define HALMAC_8881A_SUPPORT    0
#endif 

#ifdef CONFIG_WLAN_HAL_8192EE
#define HALMAC_8192E_SUPPORT    1
#else
#define HALMAC_8192E_SUPPORT    0
#endif 

#ifdef CONFIG_WLAN_HAL_8814AE
#define HALMAC_8814AMP_SUPPORT  1
#define HALMAC_8814A_SUPPORT    1
#else
#define HALMAC_8814AMP_SUPPORT  0
#define HALMAC_8814A_SUPPORT    0
#endif 

#ifdef CONFIG_WLAN_HAL_8197F
#define HALMAC_8197F_SUPPORT    1
#else
#define HALMAC_8197F_SUPPORT    0
#endif 

#ifdef CONFIG_WLAN_HAL_8198F
#define HALMAC_8198F_SUPPORT    1
#else
#define HALMAC_8198F_SUPPORT    0
#endif 

#ifdef CONFIG_WLAN_HAL_8822BE
#define HALMAC_8822B_SUPPORT    1
#else
#define HALMAC_8822B_SUPPORT    0
#endif 

#ifdef CONFIG_WLAN_HAL_8814BE
#define HALMAC_8814B_SUPPORT    1
#else
#define HALMAC_8814B_SUPPORT    0
#endif 

#ifdef CONFIG_WLAN_HAL_8822CE
#define HALMAC_8822C_SUPPORT    1
#else
#define HALMAC_8822C_SUPPORT    0
#endif

#ifdef CONFIG_WLAN_HAL_8812FE
#define HALMAC_8812F_SUPPORT    1
#else
#define HALMAC_8812F_SUPPORT    0
#endif

#ifdef CONFIG_WLAN_HAL_8192FE
#define HALMAC_8192F_SUPPORT    1
#else
#define HALMAC_8192F_SUPPORT    0
#endif 


#ifdef CONFIG_WLAN_HAL_8197G
#define HALMAC_8197G_SUPPORT    1
#else
#define HALMAC_8197G_SUPPORT    0
#endif 

#define HALMAC_8723A_SUPPORT    0
#define HALMAC_8188E_SUPPORT    0
#define HALMAC_8821A_SUPPORT    0
#define HALMAC_8723B_SUPPORT    0
#define HALMAC_8812A_SUPPORT    0
#define HALMAC_8821B_SUPPORT    0
#define HALMAC_8703B_SUPPORT    0
#define HALMAC_8723D_SUPPORT    0
#define HALMAC_8188F_SUPPORT    0
#define HALMAC_8821BMP_SUPPORT  0
#define HALMAC_8195A_SUPPORT    0
#define HALMAC_8821B_SUPPORT	0
#define HALMAC_8196F_SUPPORT    0
#define HALMAC_8821C_SUPPORT    0



#define HALMAC_8195B_SUPPORT    0
#define HALMAC_8710B_SUPPORT    0

#endif



