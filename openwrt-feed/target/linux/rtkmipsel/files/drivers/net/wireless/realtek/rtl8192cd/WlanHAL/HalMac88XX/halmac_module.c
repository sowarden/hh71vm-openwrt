/******************************************************************************
 *
 * Copyright(c) 2016 - 2019 Realtek Corporation. All rights reserved.
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

#include "halmac_api.h"
#include "halmac_module.h"

struct halmac_api_arg api_arg_table[HALMAC_API_MAX] = {
#if (HALMAC_VERSION(1, 2, 0) <= HALMAC_CURRENT_VERSION)
		{HALMAC_API_INIT_OBJ, 3, {TLP_TYPE_PVOID, TLP_TYPE_POS_API_ENTRY, TLP_TYPE_PHALMAC_INTERFACE} },
		{HALMAC_API_DEINIT_OBJ, 0, {TLP_TYPE_NONE} },
		{HALMAC_API_MAC_POWER_SWITCH, 1, {TLP_TYPE_PHALMAC_MAC_POWER} },
		{HALMAC_API_DOWNLOAD_FIRMWARE, 1, {TLP_TYPE_PU8_BUFFER} },
		{HALMAC_API_CFG_MAC_ADDR, 2, {TLP_TYPE_PU8, TLP_TYPE_PHALMAC_WLAN_ADDR} },
		{HALMAC_API_CFG_BSSID, 2, {TLP_TYPE_PU8, TLP_TYPE_PHALMAC_WLAN_ADDR} },
		{HALMAC_API_CFG_MULTICAST_ADDR, 1, {TLP_TYPE_PHALMAC_WLAN_ADDR} },
		{HALMAC_API_PRE_INIT_SYSTEM_CFG, 0, {TLP_TYPE_NONE} },
		{HALMAC_API_INIT_SYSTEM_CFG, 0, {TLP_TYPE_NONE} },
		{HALMAC_API_INIT_TRX_CFG, 1, {TLP_TYPE_PHALMAC_TRX_MODE} },
		{HALMAC_API_CFG_RX_AGGREGATION, 1, {TLP_TYPE_PHALMAC_RXAGG_CFG} },
		{HALMAC_API_CFG_OPERATION_MODE, 1, {TLP_TYPE_PHALMAC_WIRELESS_MODE} },
		{HALMAC_API_CFG_BW, 1, {TLP_TYPE_PHALMAC_BW} },
		{HALMAC_API_INIT_MAC_CFG, 1, {TLP_TYPE_PHALMAC_TRX_MODE} },
		{HALMAC_API_INIT_SDIO_CFG, 0, {TLP_TYPE_NONE} },
		{HALMAC_API_INIT_USB_CFG, 0, {TLP_TYPE_NONE} },
		{HALMAC_API_INIT_PCIE_CFG, 0, {TLP_TYPE_NONE} },
		{HALMAC_API_DEINIT_SDIO_CFG, 0, {TLP_TYPE_NONE} },
		{HALMAC_API_DEINIT_USB_CFG, 0, {TLP_TYPE_NONE} },
		{HALMAC_API_DEINIT_PCIE_CFG, 0, {TLP_TYPE_NONE} },
		{HALMAC_API_GET_EFUSE_SIZE, 1, {TLP_TYPE_PU32} },
		{HALMAC_API_DUMP_EFUSE_MAP, 1, {TLP_TYPE_PHALMAC_EFUSE_READ_CFG} },
		{HALMAC_API_GET_LOGICAL_EFUSE_SIZE,	1, {TLP_TYPE_PU32} },
		{HALMAC_API_DUMP_LOGICAL_EFUSE_MAP,	1, {TLP_TYPE_PHALMAC_EFUSE_READ_CFG} },
		{HALMAC_API_WRITE_LOGICAL_EFUSE, 2, {TLP_TYPE_PU32, TLP_TYPE_PU8} },
		{HALMAC_API_READ_LOGICAL_EFUSE, 2, {TLP_TYPE_PU32, TLP_TYPE_PU8} },
		{HALMAC_API_GET_C2H_INFO, 1, {TLP_TYPE_PU8_BUFFER} },
		{HALMAC_API_H2C_LB, 0, {TLP_TYPE_NONE} },
		{HALMAC_API_CFG_PARAMETER, 2, {TLP_TYPE_PHALMAC_PHY_PARAMETER_INFO, TLP_TYPE_PU8} },
		{HALMAC_API_UPDATE_PACKET, 2, {TLP_TYPE_PHALMAC_PACKET_ID, TLP_TYPE_PU8_BUFFER} },
		{HALMAC_API_TX_ALLOWED_SDIO, 1, {TLP_TYPE_PU8_BUFFER} },
		{HALMAC_API_SET_BULKOUT_NUM, 1, {TLP_TYPE_PU8} },
		{HALMAC_API_GET_SDIO_TX_ADDR, 2, {TLP_TYPE_PU8_BUFFER, TLP_TYPE_PU32} },
		{HALMAC_API_GET_USB_BULKOUT_ID, 2, {TLP_TYPE_PU8_BUFFER, TLP_TYPE_PU8} },
		{HALMAC_API_TIMER_2S, 0, {TLP_TYPE_NONE} },
		{HALMAC_API_CFG_DRV_INFO, 1, {TLP_TYPE_PHALMAC_DRV_INFO} },
		{HALMAC_API_SEND_BT_COEX, 1, {TLP_TYPE_PU8_BUFFER, TLP_TYPE_PU8} },
		{HALMAC_API_VERIFY_PLATFORM_API, 0, {TLP_TYPE_NONE} },
		{HALMAC_API_CFG_TXBF, 3, {TLP_TYPE_PU8,	TLP_TYPE_PHALMAC_BW, TLP_TYPE_PU8_1} },
		{HALMAC_API_CFG_MUMIMO, 1, {TLP_TYPE_PHALMAC_CFG_MUMIMO_PARA} },
		{HALMAC_API_CFG_SOUNDING, 2, {TLP_TYPE_PHALMAC_SND_ROLE, TLP_TYPE_PHALMAC_DATA_RATE} },
		{HALMAC_API_DEL_SOUNDING, 1, {TLP_TYPE_PHALMAC_SND_ROLE} },
		{HALMAC_API_SU_BFER_ENTRY_INIT, 1, {TLP_TYPE_PHALMAC_SU_BFER_INIT_PARA} },
		{HALMAC_API_SU_BFEE_ENTRY_INIT, 2, {TLP_TYPE_PU8, TLP_TYPE_PU16} },
		{HALMAC_API_MU_BFER_ENTRY_INIT, 1, {TLP_TYPE_PHALMAC_MU_BFER_INIT_PARA} },
		{HALMAC_API_MU_BFEE_ENTRY_INIT, 1, {TLP_TYPE_PHALMAC_MU_BFEE_INIT_PARA} },
		{HALMAC_API_SU_BFER_ENTRY_DEL, 1, {TLP_TYPE_PU8} },
		{HALMAC_API_SU_BFEE_ENTRY_DEL, 1, {TLP_TYPE_PU8} },
		{HALMAC_API_MU_BFER_ENTRY_DEL, 0, {TLP_TYPE_NONE} },
		{HALMAC_API_MU_BFEE_ENTRY_DEL, 1, {TLP_TYPE_PU8} },
		{HALMAC_API_ADD_CH_INFO, 1, {TLP_TYPE_PHALMAC_CH_INFO} },
		{HALMAC_API_CTRL_CH_SWITCH, 1, {TLP_TYPE_PHALMAC_CH_SWITCH_OPTION} },
		{HALMAC_API_CLEAR_CH_INFO, 0, {TLP_TYPE_NONE} },
		{HALMAC_API_SEND_GENERAL_INFO, 1, {TLP_TYPE_PHALMAC_GENERAL_INFO} },
		{HALMAC_API_QUERY_STATE, 4, {TLP_TYPE_PHALMAC_FEATURE_ID, TLP_TYPE_PHALMAC_CMD_PROCESS_STATUS, TLP_TYPE_PU8_BUFFER, TLP_TYPE_PU32} },
		{HALMAC_API_CHECK_FW_STATUS, 1, {TLP_TYPE_PU8} },
		{HALMAC_API_DUMP_FW_DMEM, 2, {TLP_TYPE_PU8_BUFFER, TLP_TYPE_PU32} },
		{HALMAC_API_RESET_FEATURE, 1, {TLP_TYPE_PHALMAC_FEATURE_ID} },
		{HALMAC_API_START_IQK, 1, {TLP_TYPE_PHALMAC_IQK} },
		{HALMAC_API_CTRL_PWR_TRACKING, 1, {TLP_TYPE_PHALMAC_PWR_TRACKING} },
		{HALMAC_API_CFG_LA_MODE, 1, {TLP_TYPE_PHALMAC_LA_MODE} },
		{HALMAC_API_GET_HW_VALUE, 2, {TLP_TYPE_PHALMAC_HW_ID, TLP_TYPE_PVOID} },
		{HALMAC_API_SET_HW_VALUE, 2, {TLP_TYPE_PHALMAC_HW_ID, TLP_TYPE_PVOID} },
		{HALMAC_API_CFG_DRV_RSVD_PG_NUM, 1, {TLP_TYPE_PHALMAC_DRV_RSVD_PG_NUM} },
		{HALMAC_API_DUMP_EFUSE_MAP_BT, 2, {TLP_TYPE_PHALMAC_EFUSE_BANK, TLP_TYPE_PU8_BUFFER} },
		{HALMAC_API_WRITE_EFUSE_BT, 3, {TLP_TYPE_PU32, TLP_TYPE_PU8, TLP_TYPE_PHALMAC_EFUSE_BANK} },
		{HALMAC_API_PG_EFUSE_BY_MAP, 2, {TLP_TYPE_PHALMAC_PG_EFUSE_INFO, TLP_TYPE_PHALMAC_EFUSE_READ_CFG} },
		{HALMAC_API_CFG_CSI_RATE, 5, {TLP_TYPE_PU8, TLP_TYPE_PU8_1, TLP_TYPE_PU8_2, TLP_TYPE_PU8_BUFFER, TLP_TYPE_PU8_BUFFER_1} },
		{HALMAC_API_DL_DRV_RSVD_PG, 2, {TLP_TYPE_PU8, TLP_TYPE_PU8_BUFFER} },
		{HALMAC_API_REG_READ_8, 1, {TLP_TYPE_PU32} },
		{HALMAC_API_REG_WRITE_8, 2, {TLP_TYPE_PU32, TLP_TYPE_PU8} },
		{HALMAC_API_REG_READ_16, 1, {TLP_TYPE_PU32} },
		{HALMAC_API_REG_WRITE_16, 2, {TLP_TYPE_PU32, TLP_TYPE_PU16} },
		{HALMAC_API_REG_READ_32, 1, {TLP_TYPE_PU32} },
		{HALMAC_API_REG_WRITE_32, 2, {TLP_TYPE_PU32, TLP_TYPE_PU32_1} },
#endif
#if (HALMAC_VERSION(1, 3, 0) <= HALMAC_CURRENT_VERSION)
		{HALMAC_API_PHY_CFG, 1, {TLP_TYPE_PHALMAC_INTF_PHY_PLATFORM} },
		{HALMAC_API_FREE_DOWNLOAD_FIRMWARE, 2, {TLP_TYPE_PHALMAC_DLFW_MEM, TLP_TYPE_PU8_BUFFER} },
		{HALMAC_API_GET_FW_VERSION, 1, {TLP_TYPE_PHALMAC_FW_VERSION} },
		{HALMAC_API_DEBUG, 0, {TLP_TYPE_NONE} },
		{HALMAC_API_GET_EFUSE_AVAL_SIZE, 1, {TLP_TYPE_PU32} },
		{HALMAC_API_FILL_TXDESC_CHECKSUM, 1, {TLP_TYPE_PU8} },
		{HALMAC_API_GET_FIFO_SIZE, 1, {TLP_TYPE_PHALMAC_FIFO_SEL} },
		{HALMAC_API_DUMP_FIFO, 3, {TLP_TYPE_PHALMAC_FIFO_SEL, TLP_TYPE_PU32, TLP_TYPE_PU8_BUFFER} },
		{HALMAC_API_P2PPS, 1, {TLP_TYPE_PHALMAC_P2PPS} },
		{HALMAC_API_CFG_TX_AGG_ALIGN, 2, {TLP_TYPE_PU8, TLP_TYPE_PU16} },
		{HALMAC_API_CFG_MAX_DL_SIZE, 1, {TLP_TYPE_PU32} },
		{HALMAC_API_CFG_RX_FIFO_EXPANDING_MODE, 1, {TLP_TYPE_PHALMAC_RX_EXPAND_MODE} },
		{HALMAC_API_CHK_TXDESC, 1, {TLP_TYPE_PU8_BUFFER} },
		{HALMAC_API_PCIE_SWITCH, 1, {TLP_TYPE_PHALMAC_PCIE_CFG} },
		{HALMAC_API_SDIO_CMD53_4BYTE, 1, {TLP_TYPE_PHALMAC_SDIO_CMD53_4BYTE_MODE} },
		{HALMAC_API_INTF_INTEGRA_TUNING, 0, {TLP_TYPE_NONE} },
		{HALMAC_API_TXFIFO_IS_EMPTY, 1, {TLP_TYPE_PU32} },
		{HALMAC_API_SDIO_HW_INFO, 1, {TLP_TYPE_PHALMAC_SDIO_HW_INFO} },
		{HALMAC_API_DOWNLOAD_FLASH, 2, {TLP_TYPE_PU8_BUFFER, TLP_TYPE_PU32} },
		{HALMAC_API_READ_FLASH, 2, {TLP_TYPE_PU32, TLP_TYPE_PU8_BUFFER} },
		{HALMAC_API_ERASE_FLASH, 2, {TLP_TYPE_PU8, TLP_TYPE_PU32} },
		{HALMAC_API_CHECK_FLASH, 2, {TLP_TYPE_PU8_BUFFER, TLP_TYPE_PU32} },
		{HALMAC_API_READ_EFUSE_BT, 3, {TLP_TYPE_PU32, TLP_TYPE_PU8, TLP_TYPE_PHALMAC_EFUSE_BANK} },
		{HALMAC_API_CFG_EFUSE_AUTO_CHECK, 1, {TLP_TYPE_PU8} },
		{HALMAC_API_CFG_PINMUX_GET_FUNC, 2, {TLP_TYPE_PHALMAC_GPIO_FUNC, TLP_TYPE_PU8} },
		{HALMAC_API_CFG_PINMUX_SET_FUNC, 1, {TLP_TYPE_PHALMAC_GPIO_FUNC} },
		{HALMAC_API_CFG_PINMUX_FREE_FUNC, 1, {TLP_TYPE_PHALMAC_GPIO_FUNC} },
		{HALMAC_API_CFG_PINMUX_WL_LED_MODE, 1, {TLP_TYPE_PHALMAC_WLLED_MODE} },
		{HALMAC_API_CFG_PINMUX_WL_LED_SW_CTRL, 1, {TLP_TYPE_PU8} },
		{HALMAC_API_CFG_PINMUX_SDIO_INT_POLARITY, 1, {TLP_TYPE_PU8} },
		{HALMAC_API_CFG_PINMUX_GPIO_MODE, 2, {TLP_TYPE_PU8, TLP_TYPE_PU8_1} },
		{HALMAC_API_CFG_PINMUX_GPIO_OUTPUT, 1, {TLP_TYPE_PU8, TLP_TYPE_PU8_1} },
		{HALMAC_API_REG_READ_INDIRECT_32, 1, {TLP_TYPE_PU32} },
		{HALMAC_API_REG_SDIO_CMD53_READ_N, 2, {TLP_TYPE_PU32, TLP_TYPE_PU8_BUFFER} },
		{HALMAC_API_PINMUX_PIN_STATUS, 1, {TLP_TYPE_PU8, TLP_TYPE_PU8_1} },
#endif
#if (HALMAC_VERSION(1, 4, 0) <= HALMAC_CURRENT_VERSION)
		{HALMAC_API_REGISTER_API, 1, {TLP_TYPE_PHALMAC_API_REGISTRY} },
		{HALMAC_API_CFG_TRANS_ADDR, 2, {TLP_TYPE_PU8, TLP_TYPE_PHALMAC_WLAN_ADDR} },
		{HALMAC_API_CFG_NET_TYPE, 2, {TLP_TYPE_PU8, TLP_TYPE_PHALMAC_NETWORK_TYPE_SELECT} },
		{HALMAC_API_CFG_TSF_RESET, 1, {TLP_TYPE_PU8} },
		{HALMAC_API_CFG_BCN_SPACE, 1, {TLP_TYPE_PU8, TLP_TYPE_PU32} },
		{HALMAC_API_CFG_BCN_CTRL, 3, {TLP_TYPE_PU8, TLP_TYPE_PU8_1, TLP_TYPE_PHALMAC_BCN_CTRL} },
		{HALMAC_API_OFLD_FUNC_CFG, 1, {TLP_TYPE_PHALMAC_OFLD_FUNC_INFO} },
		{HALMAC_API_RX_CUT_AMSDU_CFG, 1, {TLP_TYPE_PHALMAC_CUT_AMSDU_CFG} },
		{HALMAC_API_MASK_LOGICAL_EFUSE, 1, {TLP_TYPE_PHALMAC_PG_EFUSE_INFO} },
		{HALMAC_API_FW_SNDING, 3, {TLP_TYPE_PSU_SNDING, TLP_TYPE_PMU_SNDING, TLP_TYPE_PU8_1} },
		{HALMAC_API_ENTER_CPU_SLEEP_MODE, 0, {TLP_TYPE_NONE} },
		{HALMAC_API_GET_CPU_MODE, 1, {TLP_TYPE_PWLCPU_MODE} },
		{HALMAC_API_DRV_FWCTRL, 2, {TLP_TYPE_PU8_BUFFER, TLP_TYPE_PU8} },
		{HALMAC_API_EN_REF_AUTOK, 1, {TLP_TYPE_PU8} },
#endif
#if (HALMAC_VERSION(1, 5, 0) <= HALMAC_CURRENT_VERSION)
		{HALMAC_API_RESET_WIFI_FW, 0, {TLP_TYPE_NONE} },
		{HALMAC_API_CFGSPC_SET_PCIE, 1, {TLP_TYPE_PCIE_CFGSPC_PARAM} },
		{HALMAC_API_GET_WATCHER, 2, {TLP_TYPE_PHALMAC_WATCHER_SEL, TLP_TYPE_PVOID} },
		{HALMAC_API_DUMP_LOGICAL_EFUSE_MASK, 1, {TLP_TYPE_PHALMAC_EFUSE_READ_CFG} },
#endif
#if (HALMAC_VERSION(1, 6, 0) <= HALMAC_CURRENT_VERSION)
		{HALMAC_API_READ_WIFI_PHY_EFUSE, 3, {TLP_TYPE_PU32, TLP_TYPE_PU32_1, TLP_TYPE_PU8_BUFFER} },
		{HALMAC_API_WRITE_WIFI_PHY_EFUSE, 2, {TLP_TYPE_PU32, TLP_TYPE_PU8} },
		{HALMAC_API_START_DPK, 0, {TLP_TYPE_NONE} },
#endif
		{HALMAC_API_MAX, 0, {TLP_TYPE_NONE} }
};

static void
halmac_obj_init(struct halmac_obj *halmac_obj, u8 init)
{
	halmac_obj->init = init;
}

static u8
is_halmac_initialized(struct halmac_obj *halmac_obj)
{
	return halmac_obj->init;
}

static u8
parse_halmac_api_arg_list(enum halmac_api_id api_id,
			  struct halmac_tlp_struct *parg,
			  struct halmac_tlp_struct *ptlp, u32 *arg_count)
{
	u32 i, j;
	u32 idx = 0;
	u32	match_count;

	for (i = 0; i < HALMAC_API_MAX; i++) {
		if (api_arg_table[i].api_id == api_id) {
			idx = i;
			break;
		}
	}

	if (*arg_count < api_arg_table[idx].arg_count)
		return 0;

	for (i = 0; i < api_arg_table[idx].arg_count; i++) {
		match_count = 0;
		for (j = 0; j < *arg_count; j++) {
			if (api_arg_table[idx].arg_type_seq[i] == ptlp[j].type) {
				if (match_count >= 1)
					return 0; /* argument types should be different to each other for the same API */

				parg[i].type = ptlp[j].type;
				parg[i].length = ptlp[j].length;
				parg[i].ptr = ptlp[j].ptr;
				match_count++;
			}
		}
	}

	for (i = 0; i < api_arg_table[idx].arg_count; i++) {
		if (parg[i].type != api_arg_table[idx].arg_type_seq[i])
			return 0;
	}

	return 1;
}

enum halmac_ret_status
halmac_initialize_obj(struct halmac_obj *halmac_obj,
		      struct halmac_tlp_struct *ptlp, u32 tlp_count)
{
	u32 arg_count = tlp_count;
	enum halmac_ret_status result = HALMAC_RET_SUCCESS;
	struct halmac_tlp_struct arg_list[MAX_ARG_NUM] = {0};
	struct halmac_ver chip_version;
	struct halmac_adapter *adapter;

	if (is_halmac_initialized(halmac_obj))
		return HALMAC_RET_SUCCESS;

	if (!parse_halmac_api_arg_list(HALMAC_API_INIT_OBJ, &arg_list[0], ptlp, &arg_count))
		return HALMAC_RET_WRONG_ARGUMENT;

	halmac_obj->halmac_api_entry = (struct halmac_api *)0;
	halmac_obj->halmac_adapter = (struct halmac_adapter *)0;

	result = halmac_init_adapter(arg_list[0].ptr, (struct halmac_platform_api *)arg_list[1].ptr,
				     *((enum halmac_interface *)arg_list[2].ptr), &halmac_obj->halmac_adapter, &halmac_obj->halmac_api_entry);

	if (result != HALMAC_RET_SUCCESS)
		return result;

	halmac_obj_init(halmac_obj, 1);

	result = halmac_obj->halmac_api_entry->halmac_get_chip_version(halmac_obj->halmac_adapter, &chip_version);

	halmac_obj->version = HALMAC_VERSION(chip_version.major_ver, chip_version.prototype_ver, chip_version.minor_ver);

	adapter = halmac_obj->halmac_adapter;

	PLTFM_MSG_TRACE("[TRACE]halmac_init, current ver = 0x%x!!\n", HALMAC_CURRENT_VERSION);
	PLTFM_MSG_TRACE("[TRACE]halmac_init, 88xx ver = 0x%x!!\n", halmac_obj->version);

	return result;
}

enum halmac_ret_status
halmac_deinitialize_obj(struct halmac_obj *halmac_obj,
			struct halmac_tlp_struct *ptlp, u32 tlp_count)
{
	u32 arg_count = tlp_count;
	enum halmac_ret_status result = HALMAC_RET_SUCCESS;
	struct halmac_tlp_struct arg_list[MAX_ARG_NUM] = {0};

	if (!is_halmac_initialized(halmac_obj))
		return HALMAC_RET_NOT_SUPPORT;

	if (!parse_halmac_api_arg_list(HALMAC_API_DEINIT_OBJ, &arg_list[0], ptlp, &arg_count))
		return HALMAC_RET_WRONG_ARGUMENT;

	halmac_obj_init(halmac_obj, 0);
	result = halmac_deinit_adapter(halmac_obj->halmac_adapter);

	halmac_obj->halmac_api_entry = (struct halmac_api *)0;
	halmac_obj->halmac_adapter = (struct halmac_adapter *)0;

	return result;
}

enum halmac_ret_status
halmac_set_information(struct halmac_obj *halmac_obj, enum halmac_api_id api_id,
		       struct halmac_tlp_struct *ptlp, u32 tlp_count)
{
	u32 arg_count = tlp_count;
	enum halmac_ret_status state = HALMAC_RET_SUCCESS;
	struct halmac_tlp_struct arg_list[MAX_ARG_NUM] = {0};
	struct halmac_adapter *adapter = halmac_obj->halmac_adapter;

	if (!is_halmac_initialized(halmac_obj))
		return HALMAC_RET_NOT_SUPPORT;

	if (!parse_halmac_api_arg_list(api_id, &arg_list[0], ptlp, &arg_count))
		return HALMAC_RET_WRONG_ARGUMENT;

	state = HALMAC_RET_NOT_SUPPORT;

	switch (api_id) {
#if (HALMAC_VERSION(1, 2, 0) <= HALMAC_CURRENT_VERSION)
	case HALMAC_API_MAC_POWER_SWITCH:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_mac_power_switch(halmac_obj->halmac_adapter, *((enum halmac_mac_power *)arg_list[0].ptr));
		break;
	case HALMAC_API_DOWNLOAD_FIRMWARE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_download_firmware(halmac_obj->halmac_adapter, (u8 *)arg_list[0].ptr, arg_list[0].length);
		break;
	case HALMAC_API_CFG_MAC_ADDR:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_cfg_mac_addr(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr), (union halmac_wlan_addr *)arg_list[1].ptr);
		break;
	case HALMAC_API_CFG_BSSID:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_cfg_bssid(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr), (union halmac_wlan_addr *)arg_list[1].ptr);
		break;
	case HALMAC_API_CFG_MULTICAST_ADDR:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_cfg_multicast_addr(halmac_obj->halmac_adapter, (union halmac_wlan_addr *)arg_list[0].ptr);
		break;
	case HALMAC_API_PRE_INIT_SYSTEM_CFG:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_pre_init_system_cfg(halmac_obj->halmac_adapter);
		break;
	case HALMAC_API_INIT_SYSTEM_CFG:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_init_system_cfg(halmac_obj->halmac_adapter);
		break;
	case HALMAC_API_INIT_TRX_CFG:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_init_trx_cfg(halmac_obj->halmac_adapter, *((enum halmac_trx_mode *)arg_list[0].ptr));
		break;
	case HALMAC_API_CFG_RX_AGGREGATION:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_cfg_rx_aggregation(halmac_obj->halmac_adapter, (struct halmac_rxagg_cfg *)arg_list[0].ptr);
		break;
	case HALMAC_API_CFG_OPERATION_MODE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_cfg_operation_mode(halmac_obj->halmac_adapter, *((enum halmac_wireless_mode *)arg_list[0].ptr));
		break;
	case HALMAC_API_CFG_BW:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_cfg_bw(halmac_obj->halmac_adapter, *((enum halmac_bw *)arg_list[0].ptr));
		break;
	case HALMAC_API_INIT_MAC_CFG:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_init_mac_cfg(halmac_obj->halmac_adapter, *((enum halmac_trx_mode *)arg_list[0].ptr));
		break;
	case HALMAC_API_INIT_SDIO_CFG:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0))
			state = halmac_obj->halmac_api_entry->halmac_init_interface_cfg(halmac_obj->halmac_adapter);
		else if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_init_sdio_cfg(halmac_obj->halmac_adapter);
		break;
	case HALMAC_API_INIT_USB_CFG:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0))
			state = halmac_obj->halmac_api_entry->halmac_init_interface_cfg(halmac_obj->halmac_adapter);
		else if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_init_usb_cfg(halmac_obj->halmac_adapter);
		break;
	case HALMAC_API_INIT_PCIE_CFG:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0))
			state = halmac_obj->halmac_api_entry->halmac_init_interface_cfg(halmac_obj->halmac_adapter);
		else if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_init_pcie_cfg(halmac_obj->halmac_adapter);
		break;
	case HALMAC_API_DEINIT_SDIO_CFG:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0))
			state = halmac_obj->halmac_api_entry->halmac_deinit_interface_cfg(halmac_obj->halmac_adapter);
		else if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_deinit_sdio_cfg(halmac_obj->halmac_adapter);
		break;
	case HALMAC_API_DEINIT_USB_CFG:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0))
			state = halmac_obj->halmac_api_entry->halmac_deinit_interface_cfg(halmac_obj->halmac_adapter);
		else if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_deinit_usb_cfg(halmac_obj->halmac_adapter);
		break;
	case HALMAC_API_DEINIT_PCIE_CFG:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0))
			state = halmac_obj->halmac_api_entry->halmac_deinit_interface_cfg(halmac_obj->halmac_adapter);
		else if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_deinit_pcie_cfg(halmac_obj->halmac_adapter);
		break;
	case HALMAC_API_PG_EFUSE_BY_MAP:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_pg_efuse_by_map(halmac_obj->halmac_adapter, (struct halmac_pg_efuse_info *)arg_list[0].ptr, *((enum halmac_efuse_read_cfg *)arg_list[1].ptr));
		break;
	case HALMAC_API_GET_C2H_INFO:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_get_c2h_info(halmac_obj->halmac_adapter, (u8 *)arg_list[0].ptr, arg_list[0].length);
		break;
	case HALMAC_API_H2C_LB:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_h2c_lb(halmac_obj->halmac_adapter);
		break;
	case HALMAC_API_CFG_PARAMETER:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_cfg_parameter(halmac_obj->halmac_adapter, (struct halmac_phy_parameter_info *)(arg_list[0].ptr), *((u8 *)arg_list[1].ptr));
		break;
	case HALMAC_API_UPDATE_PACKET:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_update_packet(halmac_obj->halmac_adapter, *((enum halmac_packet_id *)arg_list[0].ptr), (u8 *)arg_list[1].ptr, arg_list[1].length);
		break;
	case HALMAC_API_SET_BULKOUT_NUM:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_set_bulkout_num(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr));
		break;
	case HALMAC_API_CFG_DRV_INFO:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_cfg_drv_info(halmac_obj->halmac_adapter, *((enum halmac_drv_info *)arg_list[0].ptr));
		break;
	case HALMAC_API_SEND_BT_COEX:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_send_bt_coex(halmac_obj->halmac_adapter, (u8 *)arg_list[0].ptr, arg_list[0].length, *((u8 *)arg_list[1].ptr));
		break;
	case HALMAC_API_VERIFY_PLATFORM_API:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_verify_platform_api(halmac_obj->halmac_adapter);
		break;
	case HALMAC_API_CFG_TXBF:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_cfg_txbf(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr), *((enum halmac_bw *)arg_list[1].ptr), *((u8 *)arg_list[2].ptr));
		break;
	case HALMAC_API_CFG_MUMIMO:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_cfg_mumimo(halmac_obj->halmac_adapter, (struct halmac_cfg_mumimo_para *)arg_list[0].ptr);
		break;
	case HALMAC_API_CFG_SOUNDING:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_cfg_sounding(halmac_obj->halmac_adapter, *((enum halmac_snd_role *)arg_list[0].ptr), *((enum halmac_data_rate *)arg_list[1].ptr));
		break;
	case HALMAC_API_DEL_SOUNDING:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_del_sounding(halmac_obj->halmac_adapter, *((enum halmac_snd_role *)arg_list[0].ptr));
		break;
	case HALMAC_API_SU_BFER_ENTRY_INIT:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_su_bfer_entry_init(halmac_obj->halmac_adapter, (struct halmac_su_bfer_init_para *)arg_list[0].ptr);
		break;
	case HALMAC_API_SU_BFEE_ENTRY_INIT:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_su_bfee_entry_init(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr), *((u16 *)arg_list[1].ptr));
		break;
	case HALMAC_API_MU_BFER_ENTRY_INIT:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_mu_bfer_entry_init(halmac_obj->halmac_adapter, (struct halmac_mu_bfer_init_para *)arg_list[0].ptr);
		break;
	case HALMAC_API_MU_BFEE_ENTRY_INIT:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_mu_bfee_entry_init(halmac_obj->halmac_adapter, (struct halmac_mu_bfee_init_para *)arg_list[0].ptr);
		break;
	case HALMAC_API_SU_BFER_ENTRY_DEL:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_su_bfer_entry_del(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr));
		break;
	case HALMAC_API_SU_BFEE_ENTRY_DEL:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_su_bfee_entry_del(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr));
		break;
	case HALMAC_API_MU_BFER_ENTRY_DEL:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_mu_bfer_entry_del(halmac_obj->halmac_adapter);
		break;
	case HALMAC_API_MU_BFEE_ENTRY_DEL:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_mu_bfee_entry_del(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr));
		break;
	case HALMAC_API_ADD_CH_INFO:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_add_ch_info(halmac_obj->halmac_adapter, (struct halmac_ch_info *)arg_list[0].ptr);
		break;
	case HALMAC_API_CTRL_CH_SWITCH:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_ctrl_ch_switch(halmac_obj->halmac_adapter, (struct halmac_ch_switch_option *)arg_list[0].ptr);
		break;
	case HALMAC_API_CLEAR_CH_INFO:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_clear_ch_info(halmac_obj->halmac_adapter);
		break;
	case HALMAC_API_SEND_GENERAL_INFO:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_send_general_info(halmac_obj->halmac_adapter, (struct halmac_general_info *)arg_list[0].ptr);
		break;
	case HALMAC_API_RESET_FEATURE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_reset_feature(halmac_obj->halmac_adapter, *((enum halmac_feature_id *)arg_list[0].ptr));
		break;
	case HALMAC_API_WRITE_LOGICAL_EFUSE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_write_logical_efuse(halmac_obj->halmac_adapter, *((u32 *)arg_list[0].ptr), *((u8 *)arg_list[1].ptr));
		break;
	case HALMAC_API_CTRL_PWR_TRACKING:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_ctrl_pwr_tracking(halmac_obj->halmac_adapter, (struct halmac_pwr_tracking_option *)arg_list[0].ptr);
		break;
	case HALMAC_API_START_IQK:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_start_iqk(halmac_obj->halmac_adapter, (struct halmac_iqk_para *)arg_list[0].ptr);
		break;
	case HALMAC_API_CFG_LA_MODE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_cfg_la_mode(halmac_obj->halmac_adapter, *((enum halmac_la_mode *)arg_list[0].ptr));
		break;
	case HALMAC_API_SET_HW_VALUE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_set_hw_value(halmac_obj->halmac_adapter, *((enum halmac_hw_id *)arg_list[0].ptr), (void *)arg_list[1].ptr);
		break;
	case HALMAC_API_CFG_DRV_RSVD_PG_NUM:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_cfg_drv_rsvd_pg_num(halmac_obj->halmac_adapter, *((enum halmac_drv_rsvd_pg_num *)arg_list[0].ptr));
		break;
	case HALMAC_API_WRITE_EFUSE_BT:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_write_efuse_bt(halmac_obj->halmac_adapter, *((u32 *)arg_list[0].ptr), *((u8 *)arg_list[1].ptr), *((enum halmac_efuse_bank *)arg_list[2].ptr));
		break;
	case HALMAC_API_CFG_CSI_RATE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_cfg_csi_rate(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr), *((u8 *)arg_list[1].ptr), *((u8 *)arg_list[2].ptr), (u8 *)arg_list[3].ptr, (u8 *)arg_list[4].ptr);
		break;
	case HALMAC_API_DL_DRV_RSVD_PG:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_dl_drv_rsvd_page(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr), (u8 *)arg_list[1].ptr, arg_list[1].length);
		break;
	case HALMAC_API_REG_WRITE_8:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_reg_write_8(halmac_obj->halmac_adapter, *((u32 *)arg_list[0].ptr), *((u8 *)arg_list[1].ptr));
		break;
	case HALMAC_API_REG_WRITE_16:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_reg_write_16(halmac_obj->halmac_adapter, *((u32 *)arg_list[0].ptr), *((u16 *)arg_list[1].ptr));
		break;
	case HALMAC_API_REG_WRITE_32:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_reg_write_32(halmac_obj->halmac_adapter, *((u32 *)arg_list[0].ptr), *((u32 *)arg_list[1].ptr));
		break;
#endif
#if (HALMAC_VERSION(1, 3, 0) <= HALMAC_CURRENT_VERSION)
	case HALMAC_API_PHY_CFG:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0))
			state = halmac_obj->halmac_api_entry->halmac_phy_cfg(halmac_obj->halmac_adapter, *((enum halmac_intf_phy_platform *)arg_list[0].ptr));
		break;
	case HALMAC_API_DEBUG:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0))
			state = halmac_obj->halmac_api_entry->halmac_debug(halmac_obj->halmac_adapter);
		break;
	case HALMAC_API_CFG_TX_AGG_ALIGN:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0))
			state = halmac_obj->halmac_api_entry->halmac_cfg_tx_agg_align(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr), *((u16 *)arg_list[1].ptr));
		break;
	case HALMAC_API_CFG_MAX_DL_SIZE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0))
			state = halmac_obj->halmac_api_entry->halmac_cfg_max_dl_size(halmac_obj->halmac_adapter, *((u32 *)arg_list[0].ptr));
		break;
	case HALMAC_API_CFG_RX_FIFO_EXPANDING_MODE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0))
			state = halmac_obj->halmac_api_entry->halmac_cfg_rxff_expand_mode(halmac_obj->halmac_adapter, *((enum halmac_rx_fifo_expanding_mode *)arg_list[0].ptr));
		break;
	case HALMAC_API_CHK_TXDESC:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0))
			state = halmac_obj->halmac_api_entry->halmac_chk_txdesc(halmac_obj->halmac_adapter, (u8 *)arg_list[0].ptr, arg_list[0].length);
		break;
	case HALMAC_API_PCIE_SWITCH:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0))
			state = halmac_obj->halmac_api_entry->halmac_pcie_switch(halmac_obj->halmac_adapter, *((enum halmac_pcie_cfg *)arg_list[0].ptr));
		break;
#if HALMAC_SDIO_SUPPORT
	case HALMAC_API_SDIO_CMD53_4BYTE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0))
			state = halmac_obj->halmac_api_entry->halmac_sdio_cmd53_4byte(halmac_obj->halmac_adapter, *((enum halmac_sdio_cmd53_4byte_mode *)arg_list[0].ptr));
		break;
#endif
	case HALMAC_API_INTF_INTEGRA_TUNING:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0))
			state = halmac_obj->halmac_api_entry->halmac_interface_integration_tuning(halmac_obj->halmac_adapter);
		break;
	case HALMAC_API_TXFIFO_IS_EMPTY:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 3))
			state = halmac_obj->halmac_api_entry->halmac_txfifo_is_empty(halmac_obj->halmac_adapter, *((u32 *)arg_list[0].ptr));
		break;
	case HALMAC_API_P2PPS:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 3))
			state = halmac_obj->halmac_api_entry->halmac_p2pps(halmac_obj->halmac_adapter, (struct halmac_p2pps *)arg_list[0].ptr);
		break;
	case HALMAC_API_FREE_DOWNLOAD_FIRMWARE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 5))
			state = halmac_obj->halmac_api_entry->halmac_free_download_firmware(halmac_obj->halmac_adapter, *((enum halmac_dlfw_mem *)arg_list[0].ptr), (u8 *)arg_list[1].ptr, arg_list[1].length);
		break;
#if HALMAC_SDIO_SUPPORT
	case HALMAC_API_SDIO_HW_INFO:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 6))
			state = halmac_obj->halmac_api_entry->halmac_sdio_hw_info(halmac_obj->halmac_adapter, (struct halmac_sdio_hw_info *)arg_list[0].ptr);
		break;
#endif
	case HALMAC_API_DOWNLOAD_FLASH:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 7))
			state = halmac_obj->halmac_api_entry->halmac_download_flash(halmac_obj->halmac_adapter, (u8 *)arg_list[0].ptr, arg_list[0].length, *((u32 *)arg_list[1].ptr));
		break;
	case HALMAC_API_READ_FLASH:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 7))
			state = halmac_obj->halmac_api_entry->halmac_read_flash(halmac_obj->halmac_adapter, *((u32 *)arg_list[0].ptr), arg_list[1].length, (u8 *)arg_list[1].ptr);
		break;
	case HALMAC_API_ERASE_FLASH:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 7))
			state = halmac_obj->halmac_api_entry->halmac_erase_flash(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr), *((u32 *)arg_list[1].ptr));
		break;
	case HALMAC_API_CHECK_FLASH:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 7))
			state = halmac_obj->halmac_api_entry->halmac_check_flash(halmac_obj->halmac_adapter, (u8 *)arg_list[0].ptr, arg_list[0].length, *((u32 *)arg_list[1].ptr));
		break;
	case HALMAC_API_CFG_EFUSE_AUTO_CHECK:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 7))
			state = halmac_obj->halmac_api_entry->halmac_cfg_efuse_auto_check(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr));
		break;
	case HALMAC_API_CFG_PINMUX_GET_FUNC:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 8))
			state = halmac_obj->halmac_api_entry->halmac_pinmux_get_func(halmac_obj->halmac_adapter, *((enum halmac_gpio_func *)arg_list[0].ptr), (u8 *)arg_list[1].ptr);
		break;
	case HALMAC_API_CFG_PINMUX_SET_FUNC:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 8))
			state = halmac_obj->halmac_api_entry->halmac_pinmux_set_func(halmac_obj->halmac_adapter, *((enum halmac_gpio_func *)arg_list[0].ptr));
		break;
	case HALMAC_API_CFG_PINMUX_FREE_FUNC:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 8))
			state = halmac_obj->halmac_api_entry->halmac_pinmux_free_func(halmac_obj->halmac_adapter, *((enum halmac_gpio_func *)arg_list[0].ptr));
		break;
	case HALMAC_API_CFG_PINMUX_WL_LED_MODE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 8))
			state = halmac_obj->halmac_api_entry->halmac_pinmux_wl_led_mode(halmac_obj->halmac_adapter, *((enum halmac_wlled_mode *)arg_list[0].ptr));
		break;
	case HALMAC_API_CFG_PINMUX_WL_LED_SW_CTRL:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 8)) {
			halmac_obj->halmac_api_entry->halmac_pinmux_wl_led_sw_ctrl(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr));
			state = HALMAC_RET_SUCCESS;
		}
		break;
	case HALMAC_API_CFG_PINMUX_SDIO_INT_POLARITY:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 8)) {
			halmac_obj->halmac_api_entry->halmac_pinmux_sdio_int_polarity(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr));
			state = HALMAC_RET_SUCCESS;
		}
		break;
	case HALMAC_API_CFG_PINMUX_GPIO_MODE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 8))
			state = halmac_obj->halmac_api_entry->halmac_pinmux_gpio_mode(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr), *((u8 *)arg_list[1].ptr));
		break;
	case HALMAC_API_CFG_PINMUX_GPIO_OUTPUT:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 8))
			state = halmac_obj->halmac_api_entry->halmac_pinmux_gpio_output(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr), *((u8 *)arg_list[1].ptr));
		break;
#endif
#if (HALMAC_VERSION(1, 4, 0) <= HALMAC_CURRENT_VERSION)
	case HALMAC_API_REGISTER_API:
		if (halmac_obj->version >= HALMAC_VERSION(1, 4, 0))
			state = halmac_obj->halmac_api_entry->halmac_register_api(halmac_obj->halmac_adapter, (struct halmac_api_registry *)arg_list[0].ptr);
		break;
	case HALMAC_API_CFG_TRANS_ADDR:
		if (halmac_obj->version >= HALMAC_VERSION(1, 4, 0))
			state = halmac_obj->halmac_api_entry->halmac_cfg_transmitter_addr(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr), (union halmac_wlan_addr *)arg_list[1].ptr);
		break;
	case HALMAC_API_CFG_NET_TYPE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 4, 0))
			state = halmac_obj->halmac_api_entry->halmac_cfg_net_type(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr), *((enum halmac_network_type_select *)arg_list[1].ptr));
		break;
	case HALMAC_API_CFG_TSF_RESET:
		if (halmac_obj->version >= HALMAC_VERSION(1, 4, 0))
			state = halmac_obj->halmac_api_entry->halmac_cfg_tsf_rst(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr));
		break;
	case HALMAC_API_CFG_BCN_SPACE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 4, 0))
			state = halmac_obj->halmac_api_entry->halmac_cfg_bcn_space(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr), *((u32 *)arg_list[1].ptr));
		break;
	case HALMAC_API_CFG_BCN_CTRL:
		if (halmac_obj->version >= HALMAC_VERSION(1, 4, 0))
			state = halmac_obj->halmac_api_entry->halmac_rw_bcn_ctrl(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr), *((u8 *)arg_list[1].ptr), (struct halmac_bcn_ctrl *)arg_list[2].ptr);
		break;
	case HALMAC_API_OFLD_FUNC_CFG:
		if (halmac_obj->version >= HALMAC_VERSION(1, 4, 1))
			state = halmac_obj->halmac_api_entry->halmac_ofld_func_cfg(halmac_obj->halmac_adapter, ((struct halmac_ofld_func_info *)arg_list[0].ptr));
		break;
	case HALMAC_API_MASK_LOGICAL_EFUSE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 4, 2))
			state = halmac_obj->halmac_api_entry->halmac_mask_logical_efuse(halmac_obj->halmac_adapter, ((struct halmac_pg_efuse_info *)arg_list[0].ptr));
		break;
	case HALMAC_API_RX_CUT_AMSDU_CFG:
		if (halmac_obj->version >= HALMAC_VERSION(1, 4, 2))
			state = halmac_obj->halmac_api_entry->halmac_rx_cut_amsdu_cfg(halmac_obj->halmac_adapter, ((struct halmac_cut_amsdu_cfg *)arg_list[0].ptr));
		break;
	case HALMAC_API_FW_SNDING:
		if (halmac_obj->version >= HALMAC_VERSION(1, 4, 3))
			state = halmac_obj->halmac_api_entry->halmac_fw_snding(halmac_obj->halmac_adapter, ((struct halmac_su_snding_info *)arg_list[0].ptr),
												((struct halmac_mu_snding_info *)arg_list[1].ptr), *(u8 *)arg_list[2].ptr);
		break;
	case HALMAC_API_ENTER_CPU_SLEEP_MODE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 4, 6))
			state = halmac_obj->halmac_api_entry->halmac_enter_cpu_sleep_mode(halmac_obj->halmac_adapter);
		break;
	case HALMAC_API_DRV_FWCTRL:
		if (halmac_obj->version >= HALMAC_VERSION(1, 4, 7))
			state = halmac_obj->halmac_api_entry->halmac_drv_fwctrl(halmac_obj->halmac_adapter, (u8 *)arg_list[0].ptr,
										arg_list[0].length, *((u8 *)arg_list[1].ptr));
		break;
	case HALMAC_API_EN_REF_AUTOK:
		if (halmac_obj->version >= HALMAC_VERSION(1, 4, 8))
			state = halmac_obj->halmac_api_entry->halmac_en_ref_autok_pcie(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr));
		break;
#endif
#if (HALMAC_VERSION(1, 5, 0) <= HALMAC_CURRENT_VERSION)
	case HALMAC_API_RESET_WIFI_FW:
		if (halmac_obj->version >= HALMAC_VERSION(1, 5, 13))
			state = halmac_obj->halmac_api_entry->halmac_reset_wifi_fw(halmac_obj->halmac_adapter);
		break;
#if HALMAC_PCIE_SUPPORT
	case HALMAC_API_CFGSPC_SET_PCIE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 5, 13))
			state = halmac_obj->halmac_api_entry->halmac_cfgspc_set_pcie(halmac_obj->halmac_adapter, ((struct halmac_pcie_cfgspc_param *)arg_list[0].ptr));
		break;
#endif
#endif
#if (HALMAC_VERSION(1, 6, 0) <= HALMAC_CURRENT_VERSION)
	case HALMAC_API_WRITE_WIFI_PHY_EFUSE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 6, 5))
			state = halmac_obj->halmac_api_entry->halmac_write_wifi_phy_efuse(halmac_obj->halmac_adapter, *((u32 *)arg_list[0].ptr), *((u8 *)arg_list[1].ptr));
		break;
	case HALMAC_API_START_DPK:
		if (halmac_obj->version >= HALMAC_VERSION(1, 6, 6))
			state = halmac_obj->halmac_api_entry->halmac_start_dpk(halmac_obj->halmac_adapter);
		break;
#endif
	default:
		state = HALMAC_RET_NOT_SUPPORT;
		PLTFM_MSG_WARN("[WARN]HALMAC ver = %x, api_id = %x, HALMAC_RET_NOT_SUPPORT !!\n", halmac_obj->version, api_id);
		break;
	}

	if (state != HALMAC_RET_SUCCESS)
		PLTFM_MSG_WARN("[WARN]HALMAC ver = %x, api_id = %x, state = %x!!\n", halmac_obj->version, api_id, state);
	else
		PLTFM_MSG_TRACE("[TRACE]api_id = %x, state = %x!!\n", api_id, state);

	return state;
}

enum halmac_ret_status
halmac_get_information(struct halmac_obj *halmac_obj, enum halmac_api_id api_id,
		       struct halmac_tlp_struct *ptlp, u32 tlp_count)
{
	u32 arg_count = tlp_count;
	enum halmac_ret_status state = HALMAC_RET_SUCCESS;
	struct halmac_tlp_struct arg_list[MAX_ARG_NUM] = {0};
	struct halmac_adapter *adapter = halmac_obj->halmac_adapter;

	if (!is_halmac_initialized(halmac_obj))
		return HALMAC_RET_NOT_SUPPORT;

	if (!parse_halmac_api_arg_list(api_id, &arg_list[0], ptlp, &arg_count))
		return HALMAC_RET_WRONG_ARGUMENT;

	state = HALMAC_RET_NOT_SUPPORT;

	switch (api_id) {
#if (HALMAC_VERSION(1, 2, 0) <= HALMAC_CURRENT_VERSION)
	case HALMAC_API_GET_EFUSE_SIZE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_get_efuse_size(halmac_obj->halmac_adapter, (u32 *)arg_list[0].ptr);
		break;
	case HALMAC_API_DUMP_EFUSE_MAP:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_dump_efuse_map(halmac_obj->halmac_adapter, *((enum halmac_efuse_read_cfg *)arg_list[0].ptr));
		break;
	case HALMAC_API_GET_LOGICAL_EFUSE_SIZE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_get_logical_efuse_size(halmac_obj->halmac_adapter, (u32 *)arg_list[0].ptr);
		break;
	case HALMAC_API_DUMP_LOGICAL_EFUSE_MAP:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_dump_logical_efuse_map(halmac_obj->halmac_adapter, *((enum halmac_efuse_read_cfg *)arg_list[0].ptr));
		break;
	case HALMAC_API_READ_LOGICAL_EFUSE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_read_logical_efuse(halmac_obj->halmac_adapter, *((u32 *)arg_list[0].ptr), (u8 *)arg_list[1].ptr);
		break;
	case HALMAC_API_TX_ALLOWED_SDIO:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_tx_allowed_sdio(halmac_obj->halmac_adapter, (u8 *)arg_list[0].ptr, arg_list[0].length);
		break;
	case HALMAC_API_GET_SDIO_TX_ADDR:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_get_sdio_tx_addr(halmac_obj->halmac_adapter, (u8 *)arg_list[0].ptr, arg_list[0].length, (u32 *)arg_list[1].ptr);
		break;
	case HALMAC_API_GET_USB_BULKOUT_ID:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_get_usb_bulkout_id(halmac_obj->halmac_adapter, (u8 *)arg_list[0].ptr, arg_list[0].length, (u8 *)arg_list[1].ptr);
		break;

	case HALMAC_API_QUERY_STATE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_query_status(halmac_obj->halmac_adapter, *((enum halmac_feature_id *)arg_list[0].ptr), (enum halmac_cmd_process_status *)arg_list[1].ptr, (u8 *)arg_list[2].ptr, (u32 *)arg_list[3].ptr);
		break;
	case HALMAC_API_CHECK_FW_STATUS:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_check_fw_status(halmac_obj->halmac_adapter, (u8 *)arg_list[0].ptr);
		break;
	case HALMAC_API_DUMP_FW_DMEM:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_dump_fw_dmem(halmac_obj->halmac_adapter, (u8 *)arg_list[0].ptr, (u32 *)arg_list[1].ptr);
		break;
	case HALMAC_API_GET_HW_VALUE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_get_hw_value(halmac_obj->halmac_adapter, *((enum halmac_hw_id *)arg_list[0].ptr), (void *)arg_list[1].ptr);
		break;
	case HALMAC_API_DUMP_EFUSE_MAP_BT:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0))
			state = halmac_obj->halmac_api_entry->halmac_dump_efuse_map_bt(halmac_obj->halmac_adapter, *((enum halmac_efuse_bank *)arg_list[0].ptr), arg_list[1].length, (u8 *)arg_list[1].ptr);
		break;
	case HALMAC_API_REG_READ_8:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0)) {
			*((u32 *)arg_list[0].ptr) = (u32)halmac_obj->halmac_api_entry->halmac_reg_read_8(halmac_obj->halmac_adapter, *((u32 *)arg_list[0].ptr));
			state = HALMAC_RET_SUCCESS;
		}
		break;
	case HALMAC_API_REG_READ_16:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0)) {
			*((u32 *)arg_list[0].ptr) = (u32)halmac_obj->halmac_api_entry->halmac_reg_read_16(halmac_obj->halmac_adapter, *((u32 *)arg_list[0].ptr));
			state = HALMAC_RET_SUCCESS;
		}
		break;
	case HALMAC_API_REG_READ_32:
		if (halmac_obj->version >= HALMAC_VERSION(1, 2, 0)) {
			*((u32 *)arg_list[0].ptr) = halmac_obj->halmac_api_entry->halmac_reg_read_32(halmac_obj->halmac_adapter, *((u32 *)arg_list[0].ptr));
			state = HALMAC_RET_SUCCESS;
		}
		break;
#endif
#if (HALMAC_VERSION(1, 3, 0) <= HALMAC_CURRENT_VERSION)
	case HALMAC_API_GET_FW_VERSION:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0))
			state = halmac_obj->halmac_api_entry->halmac_get_fw_version(halmac_obj->halmac_adapter, (struct halmac_fw_version *)arg_list[0].ptr);
		break;
	case HALMAC_API_GET_EFUSE_AVAL_SIZE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0))
			state = halmac_obj->halmac_api_entry->halmac_get_efuse_available_size(halmac_obj->halmac_adapter, (u32 *)arg_list[0].ptr);
		break;
	case HALMAC_API_FILL_TXDESC_CHECKSUM:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0))
			state = halmac_obj->halmac_api_entry->halmac_fill_txdesc_checksum(halmac_obj->halmac_adapter, (u8 *)arg_list[0].ptr);
		break;
	case HALMAC_API_GET_FIFO_SIZE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0)) {
			*((u32 *)arg_list[0].ptr) = halmac_obj->halmac_api_entry->halmac_get_fifo_size(halmac_obj->halmac_adapter, *((enum hal_fifo_sel *)arg_list[0].ptr));
			state = HALMAC_RET_SUCCESS;
		}
		break;
	case HALMAC_API_DUMP_FIFO:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0))
			state = halmac_obj->halmac_api_entry->halmac_dump_fifo(halmac_obj->halmac_adapter, *((enum hal_fifo_sel *)arg_list[0].ptr), *((u32 *)arg_list[1].ptr), arg_list[2].length, (u8 *)arg_list[2].ptr);
		break;
	case HALMAC_API_READ_EFUSE_BT:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 7))
			state = halmac_obj->halmac_api_entry->halmac_read_efuse_bt(halmac_obj->halmac_adapter, *((u32 *)arg_list[0].ptr), (u8 *)arg_list[1].ptr, *((enum halmac_efuse_bank *)arg_list[2].ptr));
		break;
	case HALMAC_API_REG_READ_INDIRECT_32:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0)) {
			*((u32 *)arg_list[0].ptr) = halmac_obj->halmac_api_entry->halmac_reg_read_indirect_32(halmac_obj->halmac_adapter, *((u32 *)arg_list[0].ptr));
			state = HALMAC_RET_SUCCESS;
		}
		break;
	case HALMAC_API_REG_SDIO_CMD53_READ_N:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 0))
			state = halmac_obj->halmac_api_entry->halmac_reg_sdio_cmd53_read_n(halmac_obj->halmac_adapter, *((u32 *)arg_list[0].ptr), arg_list[1].length, (u8 *)arg_list[1].ptr);
		break;
	case HALMAC_API_PINMUX_PIN_STATUS:
		if (halmac_obj->version >= HALMAC_VERSION(1, 3, 9))
			state = halmac_obj->halmac_api_entry->halmac_pinmux_pin_status(halmac_obj->halmac_adapter, *((u8 *)arg_list[0].ptr), (u8 *)arg_list[1].ptr);
		break;
#endif
#if (HALMAC_VERSION(1, 4, 0) <= HALMAC_CURRENT_VERSION)
	case HALMAC_API_GET_CPU_MODE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 4, 6))
			state = halmac_obj->halmac_api_entry->halmac_get_cpu_mode(halmac_obj->halmac_adapter, (enum halmac_wlcpu_mode *)arg_list[0].ptr);
		break;
#endif
#if (HALMAC_VERSION(1, 5, 0) <= HALMAC_CURRENT_VERSION)
	case HALMAC_API_GET_WATCHER:
		if (halmac_obj->version >= HALMAC_VERSION(1, 5, 15))
			state = halmac_obj->halmac_api_entry->halmac_get_watcher(halmac_obj->halmac_adapter, *((enum halmac_watcher_sel *)arg_list[0].ptr), (void *)arg_list[1].ptr);
		break;
	case HALMAC_API_DUMP_LOGICAL_EFUSE_MASK:
		if (halmac_obj->version >= HALMAC_VERSION(1, 5, 15))
			state = halmac_obj->halmac_api_entry->halmac_dump_logical_efuse_mask(halmac_obj->halmac_adapter, *((enum halmac_efuse_read_cfg *)arg_list[0].ptr));
		break;
#endif
#if (HALMAC_VERSION(1, 6, 0) <= HALMAC_CURRENT_VERSION)
	case HALMAC_API_READ_WIFI_PHY_EFUSE:
		if (halmac_obj->version >= HALMAC_VERSION(1, 6, 5))
			state = halmac_obj->halmac_api_entry->halmac_read_wifi_phy_efuse(halmac_obj->halmac_adapter, *((u32 *)arg_list[0].ptr), *((u32 *)arg_list[1].ptr), (u8 *)arg_list[2].ptr);
		break;
#endif
	default:
		PLTFM_MSG_WARN("[WARN]HALMAC ver = %x, api_id = %x, HALMAC_RET_NOT_SUPPORT !!\n", halmac_obj->version, api_id);
		state = HALMAC_RET_NOT_SUPPORT;
		break;
	}

	if (state != HALMAC_RET_SUCCESS)
		PLTFM_MSG_WARN("[WARN]HALMAC ver = %x, api_id = %x, state = %x!!\n", halmac_obj->version, api_id, state);
	else
		PLTFM_MSG_TRACE("[TRACE]api_id = %x, state = %x!!\n", api_id, state);

	return state;
}

