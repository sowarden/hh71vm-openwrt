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

#ifndef _HALMAC_API_8812F_PCIE_H_
#define _HALMAC_API_8812F_PCIE_H_

#include "../../halmac_api.h"

#if (HALMAC_8812F_SUPPORT && HALMAC_PCIE_SUPPORT)

extern struct halmac_intf_phy_para pcie_gen1_phy_param_8812f[];
extern struct halmac_intf_phy_para pcie_gen2_phy_param_8812f[];

enum halmac_ret_status
mac_pwr_switch_pcie_8812f(struct halmac_adapter *adapter,
			  enum halmac_mac_power pwr);

enum halmac_ret_status
pcie_switch_8812f(struct halmac_adapter *adapter, enum halmac_pcie_cfg cfg);

enum halmac_ret_status
phy_cfg_pcie_8812f(struct halmac_adapter *adapter,
		   enum halmac_intf_phy_platform pltfm);

enum halmac_ret_status
intf_tun_pcie_8812f(struct halmac_adapter *adapter);

#endif /* HALMAC_8812F_SUPPORT*/

#endif/* _HALMAC_API_8812F_PCIE_H_ */
