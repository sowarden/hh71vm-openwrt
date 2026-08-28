/******************************************************************************
 *
 * Copyright(c) 2018 - 2019 Realtek Corporation. All rights reserved.
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

#ifndef _HALMAC_8812F_CFG_H_
#define _HALMAC_8812F_CFG_H_

#include "../../halmac_hw_cfg.h"
#include "../halmac_88xx_cfg.h"

#if HALMAC_8812F_SUPPORT

#define TX_FIFO_SIZE_8812F	262144
#define RX_FIFO_SIZE_8812F	24576
#define TRX_SHARE_SIZE0_8812F	40960
#define TRX_SHARE_SIZE1_8812F	24576
#define TRX_SHARE_SIZE2_8812F	(TRX_SHARE_SIZE0_8812F + TRX_SHARE_SIZE1_8812F)

#define TX_FIFO_SIZE_LA_8812F	(TX_FIFO_SIZE_8812F >>  1)
#define TX_FIFO_SIZE_RX_EXPAND_1BLK_8812F	\
	(TX_FIFO_SIZE_8812F - TRX_SHARE_SIZE0_8812F)
#define RX_FIFO_SIZE_RX_EXPAND_1BLK_8812F	\
	(RX_FIFO_SIZE_8812F + TRX_SHARE_SIZE0_8812F)
#define TX_FIFO_SIZE_RX_EXPAND_2BLK_8812F	\
	(TX_FIFO_SIZE_8812F - TRX_SHARE_SIZE2_8812F)
#define RX_FIFO_SIZE_RX_EXPAND_2BLK_8812F	\
	(RX_FIFO_SIZE_8812F + TRX_SHARE_SIZE2_8812F)
#define TX_FIFO_SIZE_RX_EXPAND_3BLK_8812F	\
	(TX_FIFO_SIZE_8812F - TRX_SHARE_SIZE2_8812F - TRX_SHARE_SIZE0_8812F)
#define RX_FIFO_SIZE_RX_EXPAND_3BLK_8812F	\
	(RX_FIFO_SIZE_8812F + TRX_SHARE_SIZE2_8812F + TRX_SHARE_SIZE0_8812F)
#define TX_FIFO_SIZE_RX_EXPAND_4BLK_8812F	\
	(TX_FIFO_SIZE_8812F - (2 * TRX_SHARE_SIZE2_8812F))
#define RX_FIFO_SIZE_RX_EXPAND_4BLK_8812F	\
	(RX_FIFO_SIZE_8812F + (2 * TRX_SHARE_SIZE2_8812F))

#define EFUSE_SIZE_8812F	512
#define EEPROM_SIZE_8812F	768
#define BT_EFUSE_SIZE_8812F	0
#define PRTCT_EFUSE_SIZE_8812F	124

#define SEC_CAM_NUM_8812F	64

#define OQT_ENTRY_AC_8812F	32
#define OQT_ENTRY_NOAC_8812F	32
#define MACID_MAX_8812F		128

#define WLAN_FW_IRAM_MAX_SIZE_8812F	65536
#define WLAN_FW_DRAM_MAX_SIZE_8812F	65536
#define WLAN_FW_ERAM_MAX_SIZE_8812F	131072
#define WLAN_FW_MAX_SIZE_8812F		(WLAN_FW_IRAM_MAX_SIZE_8812F + \
	WLAN_FW_DRAM_MAX_SIZE_8812F + WLAN_FW_ERAM_MAX_SIZE_8812F)

#endif /* HALMAC_8812F_SUPPORT*/

#endif
