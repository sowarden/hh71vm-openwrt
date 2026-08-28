#ifndef __HALCFG_H__
#define __HALCFG_H__
/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	HalComDef.h
	
Abstract:
	Defined HAL Mapping Type
	    
Major Change History:
	When       Who               What
	---------- ---------------   -------------------------------
	2012-03-23 Filen            Create.	
--*/


//3  Driver provide some header files in order to eliminate warning message
#include "Wlan_TypeDef.h"
#include "Wlan_QoSType.h"
#include "8192cd_cfg.h"             /// ??????
#include "8192cd.h"
#include "8192cd_util.h"
#include "8192cd_headers.h"
#if defined(CONFIG_PE_ENABLE)
#include "pe_fw.h"
#include "pe_ipc.h"
#endif
#ifdef RTK_AC_SUPPORT
#include "8812_vht_gen.h"   // TODO: Filen, this name should be rename to 11AC related(independent with chip)
#endif
#ifdef CONFIG_DUAL_CPU_SLAVE
#include "dual_cpu.h"
#endif

extern void delay_us(unsigned int t);
extern void delay_ms(unsigned int t);
extern unsigned char RTL_R8_F(struct rtl8192cd_priv *priv, unsigned int reg);
extern unsigned short RTL_R16_F(struct rtl8192cd_priv *priv, unsigned int reg);
extern unsigned int RTL_R32_F(struct rtl8192cd_priv *priv, unsigned int reg);
extern void RTL_W8_F(struct rtl8192cd_priv *priv, unsigned int reg, unsigned char val8);
extern void RTL_W16_F(struct rtl8192cd_priv *priv, unsigned int reg, unsigned short val16);
extern void RTL_W32_F(struct rtl8192cd_priv *priv, unsigned int reg, unsigned int val32);


//
//  CONFIG_WLAN_HAL_8881A / CONFIG_WLAN_HAL_8192EE / ...
//  Only used to here
//
#ifdef CONFIG_WLAN_HAL_8881A
#define	CODEBASE1	RTL8881AEM
#else
#define	CODEBASE1	0
#endif

#ifdef CONFIG_WLAN_HAL_8192EE
#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
#if defined(CONFIG_RTL_92E_SUPPORT)
#define	CODEBASE2	RTL8192EU
#else
#define	CODEBASE2	RTL8192EE
#endif
#elif defined(CONFIG_PCI_HCI)
#define	CODEBASE2	RTL8192EE
#elif defined(CONFIG_USB_HCI)
#define	CODEBASE2	RTL8192EU
#elif defined(CONFIG_SDIO_HCI)
#define	CODEBASE2	RTL8192ES
#endif
#else // !CONFIG_WLAN_HAL_8192EE
#define	CODEBASE2	0
#endif

#ifdef CONFIG_WLAN_HAL_8814AE
#define	CODEBASE3	RTL8814AE
#else
#define	CODEBASE3	0
#endif

#ifdef CONFIG_WLAN_HAL_8197F
#define	CODEBASE4	RTL8197F
#else
#define	CODEBASE4	0
#endif

#ifdef CONFIG_WLAN_HAL_8822BE
#if defined(CONFIG_PCI_HCI)
#define	CODEBASE5	RTL8822BE
#elif defined(CONFIG_USB_HCI)
#define	CODEBASE5	RTL8822BU
#elif defined(CONFIG_SDIO_HCI)
#define	CODEBASE5	RTL8822BS
#endif
#else
#define	CODEBASE5	0
#endif

#ifdef CONFIG_WLAN_HAL_8821CE
#if defined(CONFIG_PCI_HCI)
#define	CODEBASE6	RTL8821CE
#elif defined(CONFIG_SDIO_HCI)
#define	CODEBASE6	RTL8821CS
#else
#define	CODEBASE6	0
#endif
#else
#define	CODEBASE6	0
#endif

#ifdef CONFIG_WLAN_HAL_8198F
#define	CODEBASE7	RTL8198F
#else
#define	CODEBASE7   0
#endif

#ifdef CONFIG_WLAN_HAL_8814BE
#define	CODEBASE8	RTL8814BE
#else
#define	CODEBASE8   0
#endif

#ifdef CONFIG_WLAN_HAL_8192FE
#define CODEBASE9	RTL8192FE
#else
#define CODEBASE9	0
#endif

#ifdef CONFIG_WLAN_HAL_8822CE
#if defined(CONFIG_PCI_HCI)
#define	CODEBASE10	RTL8822CE
#elif defined(CONFIG_USB_HCI)
#define	CODEBASE10	RTL8822CU
#elif defined(CONFIG_SDIO_HCI)
#define	CODEBASE10	RTL8822CS
#endif
#else
#define	CODEBASE10	0
#endif

#ifdef CONFIG_WLAN_HAL_8812FE
#if defined(CONFIG_PCI_HCI)
#define	CODEBASE11	RTL8812FE
#elif defined(CONFIG_USB_HCI)
#define	CODEBASE11	RTL8812FU
#elif defined(CONFIG_SDIO_HCI)
#define	CODEBASE11	RTL8812FS
#endif
#else
#define	CODEBASE11	0
#endif

#ifdef CONFIG_WLAN_HAL_8197G
#define	CODEBASE12	RTL8197G
#else
#define	CODEBASE12   0
#endif
//1 Configuration below are decided by Driver

//3 Setting Compile Option
#define HAL_CODE_BASE   (CODEBASE1|CODEBASE2|CODEBASE3|CODEBASE4|CODEBASE5|CODEBASE6|CODEBASE7|CODEBASE8|CODEBASE9|CODEBASE10|CODEBASE11|CODEBASE12)
#if !defined(HAL_CODE_BASE)
	#error "HAL_CODE_BASE is not yet defined!\n"
#endif

#if defined(CONFIG_PCI_HCI) && defined(CONFIG_USB_HCI) /* CONFIG_RTL_TRIBAND_SUPPORT */
#define HAL_DEV_BUS_TYPE	(HAL_RT_EMBEDDED_INTERFACE | HAL_RT_PCI_INTERFACE | HAL_RT_USB_INTERFACE)
#elif defined(CONFIG_PCI_HCI) /* !CONFIG_RTL_TRIBAND_SUPPORT */
#define HAL_DEV_BUS_TYPE	(HAL_RT_EMBEDDED_INTERFACE | HAL_RT_PCI_INTERFACE)
#elif defined(CONFIG_USB_HCI)
#define HAL_DEV_BUS_TYPE	HAL_RT_USB_INTERFACE
#elif defined(CONFIG_SDIO_HCI)
#define HAL_DEV_BUS_TYPE	HAL_RT_SDIO_INTERFACE
#endif

//2 HAL Compile flag: 
//Prefix: CFG_HAL_XXXXXXXXXXX
#define CFG_HAL_DBG                          0

#ifdef DISABLE_BB_RF
#define CFG_HAL_DISABLE_BB_RF                1
#else
#define CFG_HAL_DISABLE_BB_RF                0
#endif //DISABLE_BB_RF

#ifdef ENABLE_LA_MODE
#define CFG_HAL_MAC_ENABLE_LA_MODE           1
#else
#define CFG_HAL_MAC_ENABLE_LA_MODE           0
#endif

#ifdef DRVMAC_LB
#define CFG_HAL_MAC_LOOPBACK                 1
#else
#define CFG_HAL_MAC_LOOPBACK                 0
#endif

#ifdef MP_TEST
#define CFG_HAL_MP_TEST					1
#else
#define CFG_HAL_MP_TEST					0
#endif

#ifdef CLIENT_MODE
#define CFG_HAL_SUPPORT_CLIENT_MODE          1
#else
#define CFG_HAL_SUPPORT_CLIENT_MODE          0
#endif

#ifdef CONFIG_RTL_VAP_SUPPORT
#define CFG_HAL_SUPPORT_MBSSID               1
#else
#define CFG_HAL_SUPPORT_MBSSID               0
#endif

#ifdef UNIVERSAL_REPEATER
#define CFG_HAL_SUPPORT_UNIVERSAL_REPEATER   1
#else
#define CFG_HAL_SUPPORT_UNIVERSAL_REPEATER   0
#endif

#ifdef CHECK_SWAP
#define CFG_HAL_CHECK_SWAP                   1
#else
#define CFG_HAL_CHECK_SWAP                   0
#endif

#ifdef  WIFI_WMM
#define CFG_HAL_WIFI_WMM                     1
#else
#define CFG_HAL_WIFI_WMM                     0
#endif  //WIFI_WMM

#ifdef  RTL_MANUAL_EDCA
#define CFG_HAL_RTL_MANUAL_EDCA              1
#else
#define CFG_HAL_RTL_MANUAL_EDCA              0
#endif  //RTL_MANUAL_EDCA

#ifdef CONFIG_RTL_HW_WAPI_SUPPORT
#define CFG_HAL_RTL_HW_WAPI_SUPPORT          1
#else
#define CFG_HAL_RTL_HW_WAPI_SUPPORT          0
#endif  //CONFIG_RTL_HW_WAPI_SUPPORT

#ifdef TX_SHORTCUT
#define CFG_HAL_TX_SHORTCUT                  1
#else
#define CFG_HAL_TX_SHORTCUT                  0
#endif // TX_SHORTCUT

#ifdef WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC
#define CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC	 1
#else
#define CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC	 0
#endif // WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC

#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
#define CFG_HAL_HW_TX_SHORTCUT_HDR_CONV		 1
#define CFG_HAL_HW_TX_SHORTCUT_HDR_CONV_LLC  1
#else
#define CFG_HAL_HW_TX_SHORTCUT_HDR_CONV		 0
#define CFG_HAL_HW_TX_SHORTCUT_HDR_CONV_LLC  0
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV

#ifdef SUPPORT_TX_HW_AMSDU
#define CFG_HAL_HW_TX_AMSDU                  1
#else
#define CFG_HAL_HW_TX_AMSDU                  0
#endif


#ifdef WLAN_HAL_HW_AES_IV
#define CFG_HAL_HW_AES_IV               	 1
#else
#define CFG_HAL_HW_AES_IV                    0
#endif // WLAN_HAL_HW_AES_IV


#ifdef WLAN_HAL_HW_SEQ
#define CFG_HAL_HW_SEQ						 1
#else
#define CFG_HAL_HW_SEQ						 0
#endif

#define CFG_FW_VERIFICATION                  0

#ifdef RTK_AC_SUPPORT
#define CFG_HAL_RTK_AC_SUPPORT               1
#else
#define CFG_HAL_RTK_AC_SUPPORT               0
#endif // RTK_AC_SUPPORT

#define CFG_HAL_MEASURE_BEACON               0

#ifdef DELAY_REFILL_RX_BUF
#define CFG_HAL_DELAY_REFILL_RX_BUF          1
#else
#define CFG_HAL_DELAY_REFILL_RX_BUF          0
#endif // DELAY_REFILL_RX_BUF

#ifdef WLANHAL_MACDM
#define CFG_HAL_MACDM                        1
#else
#define CFG_HAL_MACDM                        0
#endif

#ifdef WLAN_HAL_TX_AMSDU
#define CFG_HAL_TX_AMSDU                     1
#else
#define CFG_HAL_TX_AMSDU                     0
#endif

#ifdef WLAN_SUPPORT_H2C_PACKET
#define CFG_HAL_WLAN_SUPPORT_H2C_PACKET      1
#else
#define CFG_HAL_WLAN_SUPPORT_H2C_PACKET      0
#endif

#ifdef WLAN_SUPPORT_FW_CMD
#define CFG_HAL_WLAN_SUPPORT_FW_CMD      1
#else
#define CFG_HAL_WLAN_SUPPORT_FW_CMD      0
#endif

//3 Configuration parameter Setting

// config Txpower Setting
#ifdef POWER_PERCENT_ADJUSTMENT
#define CFG_HAL_POWER_PERCENT_ADJUSTMENT    1
#else
#define CFG_HAL_POWER_PERCENT_ADJUSTMENT    0
#endif // POWER_PERCENT_ADJUSTMENT

#ifdef HIGH_POWER_EXT_PA
#define CFG_HAL_HIGH_POWER_EXT_PA    1
#else
#define CFG_HAL_HIGH_POWER_EXT_PA    0
#endif // HIGH_POWER_EXT_PA

#ifdef HIGH_POWER_EXT_LNA
#define CFG_HAL_HIGH_POWER_EXT_LNA    1
#else
#define CFG_HAL_HIGH_POWER_EXT_LNA    0
#endif // HIGH_POWER_EXT_LNA


#ifdef ADD_TX_POWER_BY_CMD
#define CFG_HAL_ADD_TX_POWER_BY_CMD    1
#else
#define CFG_HAL_ADD_TX_POWER_BY_CMD    0
#endif // ADD_TX_POWER_BY_CMD


#ifdef _TRACKING_TABLE_FILE
#define CFG_TRACKING_TABLE_FILE         1
#else
#define CFG_TRACKING_TABLE_FILE         0
#endif // _TRACKING_TABLE_FILE


#ifdef HW_DETEC_POWER_STATE
#define CFG_HAL_HW_DETEC_POWER_STATE    1
#else
#define CFG_HAL_HW_DETEC_POWER_STATE    0
#endif //#ifdef HW_DETEC_POWER_STATE
#ifdef HW_FILL_MACID
#define CFG_HAL_HW_FILL_MACID           1
#else
#define CFG_HAL_HW_FILL_MACID           0
#endif //#ifdef HW_FILL_MACID

#ifdef AP_SWPS_OFFLOAD
#define CFG_HAL_AP_SWPS_OFFLOAD         1
#else
#define CFG_HAL_AP_SWPS_OFFLOAD         0
#endif

#ifdef STA_AGING_FUNC_OFFLOAD
#define CFG_HAL_STA_AGING_FUNC_OFFLOAD         1
#else
#define CFG_HAL_STA_AGING_FUNC_OFFLOAD         0
#endif

#ifdef SUPPORT_RELEASE_ONE_PACKET
#define CFG_HAL_RELEASE_ONE_PACKET      1
#else
#define CFG_HAL_RELEASE_ONE_PACKET      0
#endif //#ifdef HW_FILL_MACID


#ifdef MULTICAST_BMC_ENHANCE
#define CFG_HAL_MULTICAST_BMC_ENHANCE   1
#else
#define CFG_HAL_MULTICAST_BMC_ENHANCE   0
#endif //MULTICAST_BMC_ENHANCE

#ifdef RTL_8814_RFE_TYPE0
#define CFG_8814_RFE_TYPE0   1
#else
#define CFG_8814_RFE_TYPE0   0
#endif //#ifdef RTL_8814_RFE_TYPE0

#ifdef RTL_8814_RFE_TYPE2
#define CFG_8814_RFE_TYPE2   1
#else
#define CFG_8814_RFE_TYPE2   0
#endif //#ifdef RTL_8814_RFE_TYPE2

#ifdef MULTI_MAC_CLONE
#define CFG_HAL_MULTI_MAC_CLONE         1
#else
#define CFG_HAL_MULTI_MAC_CLONE         0
#endif //MULTI_MAC_CLONE

#ifdef SUPPORT_EACH_VAP_INT
#define CFG_HAL_SUPPORT_EACH_VAP_INT    1
#else
#define CFG_HAL_SUPPORT_EACH_VAP_INT    0
#endif  //SUPPORT_EACH_VAP_INT

#ifdef SUPPORT_AXI_BUS_EXCEPTION
#define CFG_HAL_SUPPORT_AXI_BUS_EXCEPTION    1
#else
#define CFG_HAL_SUPPORT_AXI_BUS_EXCEPTION    0
#endif  //SUPPORT_AXI_BUS_EXCEPTION

#ifdef WLAN_HAL_ENHANCED_EDCA
#define CFG_HAL_SUPPORT_ENHANCED_EDCA    1
#else
#define CFG_HAL_SUPPORT_ENHANCED_EDCA    0
#endif  //WLAN_HAL_ENHANCED_EDCA

#ifdef SUPPORT_TXDESC_IE
#define CFG_HAL_SUPPORT_TXDESC_IE    1
#else
#define CFG_HAL_SUPPORT_TXDESC_IE    0
#endif


//4 AP Service Support
#if CFG_HAL_SUPPORT_MBSSID
#define HAL_NUM_VWLAN   RTL8192CD_NUM_VWLAN
#else
#define HAL_NUM_VWLAN   0
#endif

#ifdef ATM_REPORT
#define CFG_HAL_ATM_REPORT 1
#else
#define CFG_HAL_ATM_REPORT 0
#endif

#ifdef PWR_CONSUMPTION_REDUCE
#define CFG_HAL_PWR_REDUCE 1
#else
#define CFG_HAL_PWR_REDUCE 0
#endif

#if IS_RTL88XX_GENERATION

//3 Method Selection for RXTAG or TOTALRXPKTSIZE of RXBD
// 1: RXTAG
// 0: TOTALRXPKTSIZE
#define RXBD_READY_CHECK_METHOD     1

//4 HW Offset between TXBD Beacon
// Eric change, Beacon offset different in 8881 & other chips
#if IS_EXIST_RTL8192EE || IS_EXIST_RTL8192EU || IS_EXIST_RTL8814AE ||IS_EXIST_RTL8197FEM ||IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8198FEM || IS_EXIST_RTL8814BE|| IS_EXIST_RTL8192FE || IS_EXIST_RTL8822CE  || IS_EXIST_RTL8812FE|| IS_EXIST_RTL8197GEM
#define TXBD_BEACON_OFFSET_V1       128 
#endif  //#if IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE ||IS_EXIST_RTL8197FEM ||IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM|| IS_EXIST_RTL8192FE || IS_EXIST_RTL8814BE || IS_EXIST_RTL8197GEM

#if IS_EXIST_RTL8881AEM
#define TXBD_BEACON_OFFSET_V2       64  //Filen: Cant's be modified (HW Fixed value)       
#endif  //IS_RTL8881A_SERIES

#if 0
#if IS_EXIST_RTL8192EE
#define TXBD_BEACON_OFFSET_8192E    128 //Filen: Cant's be modified (HW Fixed value)       
#endif  //IS_RTL8192E_SERIES

#if IS_EXIST_RTL8881AEM
#define TXBD_BEACON_OFFSET_8881A    64  //Filen: Cant's be modified (HW Fixed value)       
#endif  //IS_RTL8881A_SERIES

#if IS_EXIST_RTL8814AE
#define TXBD_BEACON_OFFSET_8814AE   128 //Filen: Cant's be modified (HW Fixed value)       
#endif  //IS_EXIST_RTL8814AE

#if IS_EXIST_RTL8197FEM
#define TXBD_BEACON_OFFSET_8197F    128 //Filen: Cant's be modified (HW Fixed value)       
#endif  //IS_EXIST_RTL8814AE
#endif 

#define TXBD_BEACON_OFFSET_MAX      128 //Compare all Chip Beacon Offset

//4 TDECTRL
#define SECOND_BCN_PAGE_OFFSET          5   //including Beacon Page + Probe Response Page + .....

//4 RQPN
#if CFG_HAL_MAC_LOOPBACK
#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8197F_SERIES || IS_RTL8192F_SERIES)
#define TXPKTBUF_TOTAL_PAGECNT          256   // Filen: can't be modified in 92E/8881A
#define TX_PAGE_CNT_HPQ                 0x04                 
#define TX_PAGE_CNT_NPQ                 0x29
#define TX_PAGE_CNT_LPQ                 0x04
#define TX_PAGE_CNT_EPQ                 0x04
#define TX_PAGE_CNT_RSV                 (TXPKTBUF_TOTAL_PAGECNT/2)
#define TX_PAGE_CNT_PUBQ                ((TXPKTBUF_TOTAL_PAGECNT/2) - TX_PAGE_CNT_HPQ - TX_PAGE_CNT_NPQ - TX_PAGE_CNT_LPQ - TX_PAGE_CNT_EPQ - 1)

//value below should depend on each queue setting
#define TX_PAGE_CNT_MIN_AC_DEDICATEDQ   TX_PAGE_CNT_HPQ //EXTRA QUEUE is excluding
#define TX_PAGE_CNT_MIN_DEDQ_PUBQ       (TX_PAGE_CNT_PUBQ+TX_PAGE_CNT_MIN_AC_DEDICATEDQ)
#endif //(IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8197F_SERIES || IS_RTL8192F_SERIES)

#if (IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
#define TXPKTBUF_TOTAL_PAGECNT_V1       2048  // Filen: can't be modified in 8814A
#ifdef WMM_DSCP_C42
#define TX_PAGE_CNT_HPQ_V1              80 
#define TX_PAGE_CNT_NPQ_V1              80
#define TX_PAGE_CNT_LPQ_V1              80
#define TX_PAGE_CNT_EPQ_V1              20
#else
#define TX_PAGE_CNT_HPQ_V1              20 
#define TX_PAGE_CNT_NPQ_V1              20
#define TX_PAGE_CNT_LPQ_V1              20
#define TX_PAGE_CNT_EPQ_V1              20
#endif
#if 0/*IS_RTL8197F_SERIES*/
#define TXPKTBUF_TOTAL_PAGECNT_V1       256  // Filen: can't be modified in 8197F
#define TX_PAGE_CNT_HPQ_V1              0x4 
#define TX_PAGE_CNT_NPQ_V1              0x29
#define TX_PAGE_CNT_LPQ_V1              0x4
#define TX_PAGE_CNT_EPQ_V1              0x4
#endif
#define TX_PAGE_CNT_RSV_V1              (TXPKTBUF_TOTAL_PAGECNT_V1/2)
#define TX_PAGE_CNT_PUBQ_V1             ((TXPKTBUF_TOTAL_PAGECNT_V1/2) - TX_PAGE_CNT_HPQ_V1 - TX_PAGE_CNT_NPQ_V1 - TX_PAGE_CNT_LPQ_V1 - TX_PAGE_CNT_EPQ_V1 - 1)

//value below should depend on each queue setting
#define TX_PAGE_CNT_MIN_AC_DEDICATEDQ_V1   TX_PAGE_CNT_HPQ_V1 //EXTRA QUEUE is excluding
#define TX_PAGE_CNT_MIN_DEDQ_PUBQ_V1       (TX_PAGE_CNT_PUBQ_V1+TX_PAGE_CNT_MIN_AC_DEDICATEDQ_V1)

#endif //(IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)

#if (IS_RTL8198F_SERIES)
#define TXPKTBUF_TOTAL_PAGECNT_V2       (Adapter->pmib->miscEntry.normal_LA_on ? 512: 1024)  // kkbomb for LA mode adjust
//#define TXPKTBUF_TOTAL_PAGECNT_V2       1024  // Cant't modify 
#define TX_PAGE_CNT_HPQ_V2              4 
#define TX_PAGE_CNT_NPQ_V2              20
#define TX_PAGE_CNT_LPQ_V2              20
#define TX_PAGE_CNT_EPQ_V2              10
#define TX_PAGE_CNT_EX1Q_V2             20
#define TX_PAGE_CNT_EX2Q_V2             20
#define TX_PAGE_CNT_RSV_V2              (TXPKTBUF_TOTAL_PAGECNT_V2/2)
#define TX_PAGE_CNT_PUBQ_V2             (TXPKTBUF_TOTAL_PAGECNT_V2/2 - TX_PAGE_CNT_HPQ_V2 - TX_PAGE_CNT_NPQ_V2 - TX_PAGE_CNT_LPQ_V2 - TX_PAGE_CNT_EPQ_V2 - TX_PAGE_CNT_EX1Q_V2 - TX_PAGE_CNT_EX2Q_V2 - 1)

//value below should depend on each queue setting
#define TX_PAGE_CNT_MIN_AC_DEDICATEDQ_V2   TX_PAGE_CNT_HPQ_V2 //EXTRA QUEUE is excluding
#define TX_PAGE_CNT_MIN_DEDQ_PUBQ_V2       (TX_PAGE_CNT_PUBQ_V2+TX_PAGE_CNT_MIN_AC_DEDICATEDQ_V2)
#endif //(IS_RTL8198F_SERIES)

#if (IS_RTL8814B_SERIES)
// RQPN is set in halmac , these regs are no use//yllin8814B 
#endif //(IS_RTL8814B_SERIES)

#if (IS_RTL8197G_SERIES)
#define TXPKTBUF_TOTAL_PAGECNT_V3       512  // Cant't modify 
#define TX_PAGE_CNT_HPQ_V3              20 
#define TX_PAGE_CNT_NPQ_V3              20
#define TX_PAGE_CNT_LPQ_V3              20
#define TX_PAGE_CNT_EPQ_V3              10
#define TX_PAGE_CNT_EX1Q_V3             20
#define TX_PAGE_CNT_EX2Q_V3             20
#define TX_PAGE_CNT_RSV_V3              (TXPKTBUF_TOTAL_PAGECNT_V3/2)
#define TX_PAGE_CNT_PUBQ_V3             (TXPKTBUF_TOTAL_PAGECNT_V3/2 - TX_PAGE_CNT_HPQ_V3 - TX_PAGE_CNT_NPQ_V3 - TX_PAGE_CNT_LPQ_V3 - TX_PAGE_CNT_EPQ_V3 - TX_PAGE_CNT_EX1Q_V3 - TX_PAGE_CNT_EX2Q_V3 - 1)

//value below should depend on each queue setting
#define TX_PAGE_CNT_MIN_AC_DEDICATEDQ_V3   TX_PAGE_CNT_HPQ_V3 //EXTRA QUEUE is excluding
#define TX_PAGE_CNT_MIN_DEDQ_PUBQ_V3       (TX_PAGE_CNT_PUBQ_V3+TX_PAGE_CNT_MIN_AC_DEDICATEDQ_V3)
#endif //(IS_RTL8197G_SERIES)

#else // #if CFG_HAL_MAC_LOOPBACK

#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8197F_SERIES|| IS_RTL8192F_SERIES)
#define TXPKTBUF_TOTAL_PAGECNT          256   // Filen: can't be modified in 92E/8881A
#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
#define TX_PAGE_CNT_HPQ                 0x0e
#define TX_PAGE_CNT_NPQ                 (Adapter->hci_type == RTL_HCI_PCIE && Adapter->pshare->rf_ft_var.txbuf_merge ? 0x49: 0x29) 
#define TX_PAGE_CNT_LPQ                 (Adapter->hci_type == RTL_HCI_PCIE && Adapter->pshare->rf_ft_var.txbuf_merge ? 0x00: 0x20) 
#define TX_PAGE_CNT_EPQ                 0x04
#define TX_PAGE_CNT_PUBQ                0x9a

#else /* !defined(CONFIG_RTL_TRIBAND_SUPPORT) */

#if IS_EXIST_PCI || IS_EXIST_EMBEDDED
#ifdef WMM_DSCP_C42
#define TX_PAGE_CNT_HPQ                 0x20
#define TX_PAGE_CNT_NPQ                 0x20
#define TX_PAGE_CNT_LPQ                	0x20
#define TX_PAGE_CNT_EPQ                	0x17
#define TX_PAGE_CNT_PUBQ                0x7E
#elif IS_RTL8197F_SERIES
#define TX_PAGE_CNT_HPQ                 0x20
#define TX_PAGE_CNT_NPQ                 0x20
#define TX_PAGE_CNT_LPQ                	0x20
#define TX_PAGE_CNT_EPQ                	0x17
#define TX_PAGE_CNT_PUBQ                0x7E
#else
#if 1
#define TX_PAGE_CNT_HPQ                 0x0e
#define TX_PAGE_CNT_NPQ                 (Adapter->pshare->rf_ft_var.txbuf_merge ? 0x49: 0x29) 
#define TX_PAGE_CNT_LPQ                 (Adapter->pshare->rf_ft_var.txbuf_merge ? 0x00: 0x20) 
#define TX_PAGE_CNT_EPQ                 0x04
#define TX_PAGE_CNT_PUBQ                0x9a
#else
#define TX_PAGE_CNT_HPQ                 0x0e
#define TX_PAGE_CNT_NPQ                 0x29
#define TX_PAGE_CNT_LPQ                 0x20
#define TX_PAGE_CNT_EPQ                 0x04
#define TX_PAGE_CNT_PUBQ                0x9a
#endif
#endif
#endif // IS_EXIST_PCI || IS_EXIST_EMBEDDED

#if IS_EXIST_SDIO
#ifdef CONFIG_SDIO_TX_INTERRUPT
#define TX_PAGE_CNT_HPQ                 0x12
#define TX_PAGE_CNT_NPQ                 0x29
#define TX_PAGE_CNT_LPQ                 0x20
#define TX_PAGE_CNT_EPQ                 0x00
#define TX_PAGE_CNT_PUBQ                0x9a
#else
#define TX_PAGE_CNT_HPQ                 0x0e
#define TX_PAGE_CNT_NPQ                 0x29
#define TX_PAGE_CNT_LPQ                 0x20
#define TX_PAGE_CNT_EPQ                 0x04
#define TX_PAGE_CNT_PUBQ                0x9a
#endif // CONFIG_SDIO_TX_INTERRUPT
#endif // IS_EXIST_SDIO
#endif /* defined(CONFIG_RTL_TRIBAND_SUPPORT) */


#if CFG_HAL_SUPPORT_MBSSID
#define TX_PAGE_CNT_RSV                 (SECOND_BCN_PAGE_OFFSET << 1)
#else
#define TX_PAGE_CNT_RSV                 SECOND_BCN_PAGE_OFFSET
#endif // CFG_HAL_SUPPORT_MBSSID

//value below should depend on each queue setting
#define TX_PAGE_CNT_MIN_AC_DEDICATEDQ   TX_PAGE_CNT_HPQ //EXTRA QUEUE is excluding
#define TX_PAGE_CNT_MIN_DEDQ_PUBQ       (TX_PAGE_CNT_PUBQ+TX_PAGE_CNT_MIN_AC_DEDICATEDQ)
#endif //(IS_RTL8192E_SERIES || IS_RTL8881A_SERIES)

#if (IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
#define TXPKTBUF_TOTAL_PAGECNT_V1       2048  // Filen: can't be modified in 8814A
#define TX_PAGE_CNT_HPQ_V1              80 
#define TX_PAGE_CNT_NPQ_V1              80
#define TX_PAGE_CNT_LPQ_V1              80
#define TX_PAGE_CNT_EPQ_V1              40
#if CFG_HAL_SUPPORT_MBSSID
#define TX_PAGE_CNT_RSV_V1              (SECOND_BCN_PAGE_OFFSET << 1)
#else
#define TX_PAGE_CNT_RSV_V1              SECOND_BCN_PAGE_OFFSET
#endif // CFG_HAL_SUPPORT_MBSSID
#define TX_PAGE_CNT_PUBQ_V1             (TXPKTBUF_TOTAL_PAGECNT_V1 - TX_PAGE_CNT_HPQ_V1 - TX_PAGE_CNT_NPQ_V1 - TX_PAGE_CNT_LPQ_V1 - TX_PAGE_CNT_EPQ_V1 - TX_PAGE_CNT_RSV_V1 - 1)

//value below should depend on each queue setting
#define TX_PAGE_CNT_MIN_AC_DEDICATEDQ_V1   TX_PAGE_CNT_HPQ_V1 //EXTRA QUEUE is excluding
#define TX_PAGE_CNT_MIN_DEDQ_PUBQ_V1       (TX_PAGE_CNT_PUBQ_V1+TX_PAGE_CNT_MIN_AC_DEDICATEDQ_V1)
#endif //(IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)

#if (IS_RTL8198F_SERIES)
#define TXPKTBUF_TOTAL_PAGECNT_V2       (Adapter->pmib->miscEntry.normal_LA_on ? 512: 1024)  // kkbomb for LA mode adjust
//#define TXPKTBUF_TOTAL_PAGECNT_V2       1024  // Cant't modify 
#define TX_PAGE_CNT_HPQ_V2              4 
#define TX_PAGE_CNT_NPQ_V2              20
#define TX_PAGE_CNT_LPQ_V2              20
#define TX_PAGE_CNT_EPQ_V2              10
#define TX_PAGE_CNT_EX1Q_V2             20
#define TX_PAGE_CNT_EX2Q_V2             20
#if CFG_HAL_SUPPORT_MBSSID
#define TX_PAGE_CNT_RSV_V2              (SECOND_BCN_PAGE_OFFSET << 1)
#else
#define TX_PAGE_CNT_RSV_V2              SECOND_BCN_PAGE_OFFSET
#endif // CFG_HAL_SUPPORT_MBSSID
#define TX_PAGE_CNT_PUBQ_V2             (TXPKTBUF_TOTAL_PAGECNT_V2 - TX_PAGE_CNT_HPQ_V2 - TX_PAGE_CNT_NPQ_V2 - TX_PAGE_CNT_LPQ_V2 - TX_PAGE_CNT_EPQ_V2 - TX_PAGE_CNT_EX1Q_V2 -TX_PAGE_CNT_EX2Q_V2 - TX_PAGE_CNT_RSV_V2 - 1)

//value below should depend on each queue setting
#define TX_PAGE_CNT_MIN_AC_DEDICATEDQ_V2   TX_PAGE_CNT_HPQ_V2 //EXTRA QUEUE is excluding
#define TX_PAGE_CNT_MIN_DEDQ_PUBQ_V2       (TX_PAGE_CNT_PUBQ_V2+TX_PAGE_CNT_MIN_AC_DEDICATEDQ_V2)
#endif //(IS_RTL8198F_SERIES)

#if (IS_RTL8814B_SERIES)
// RQPN is set in halmac , these regs are no use//yllin8814B 
#endif //(IS_RTL8814B_SERIES)

#if (IS_RTL8197G_SERIES)
#define TXPKTBUF_TOTAL_PAGECNT_V3       512  // Cant't modify 
#define TX_PAGE_CNT_HPQ_V3              20 
#define TX_PAGE_CNT_NPQ_V3              20
#define TX_PAGE_CNT_LPQ_V3              20
#define TX_PAGE_CNT_EPQ_V3              10
#define TX_PAGE_CNT_EX1Q_V3             20
#define TX_PAGE_CNT_EX2Q_V3             20
#if CFG_HAL_SUPPORT_MBSSID
#define TX_PAGE_CNT_RSV_V3              (SECOND_BCN_PAGE_OFFSET << 1)
#else
#define TX_PAGE_CNT_RSV_V3              SECOND_BCN_PAGE_OFFSET
#endif // CFG_HAL_SUPPORT_MBSSID

#define TX_PAGE_CNT_PUBQ_V3             (TXPKTBUF_TOTAL_PAGECNT_V3 - TX_PAGE_CNT_HPQ_V3 - TX_PAGE_CNT_NPQ_V3 - TX_PAGE_CNT_LPQ_V3 - TX_PAGE_CNT_EPQ_V3 - TX_PAGE_CNT_EX1Q_V3 -TX_PAGE_CNT_EX2Q_V3 - TX_PAGE_CNT_RSV_V3 - 1)


//value below should depend on each queue setting
#define TX_PAGE_CNT_MIN_AC_DEDICATEDQ_V3   TX_PAGE_CNT_HPQ_V3 //EXTRA QUEUE is excluding
#define TX_PAGE_CNT_MIN_DEDQ_PUBQ_V3       (TX_PAGE_CNT_PUBQ_V3+TX_PAGE_CNT_MIN_AC_DEDICATEDQ_V3)
#endif //(IS_RTL8197G_SERIES)

#endif // CFG_HAL_MAC_LOOPBACK

//4 Page Size
//
// we set RPQN pagecnt is less than LLT pagecnt,
// because we avoid some critical condition
//
#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8197F_SERIES || IS_RTL8192F_SERIES)
#define TXPKTBUF_RQPN_PAGECNT           (TX_PAGE_CNT_HPQ + TX_PAGE_CNT_NPQ + TX_PAGE_CNT_LPQ + TX_PAGE_CNT_EPQ + TX_PAGE_CNT_PUBQ)
#define TXPKTBUF_LLT_PAGECNT            (TXPKTBUF_RQPN_PAGECNT+1)   
#endif //#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8197F_SERIES)

#if (IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
#define TXPKTBUF_RQPN_PAGECNT_V1           (TX_PAGE_CNT_HPQ_V1 + TX_PAGE_CNT_NPQ_V1 + TX_PAGE_CNT_LPQ_V1 + TX_PAGE_CNT_EPQ_V1 + TX_PAGE_CNT_PUBQ_V1)
#define TXPKTBUF_LLT_PAGECNT_V1            (TXPKTBUF_RQPN_PAGECNT_V1 + 1)
#endif //#if (IS_RTL8814A_SERIES || IS_RTL8822B_SERIES)

#if (IS_RTL8198F_SERIES)
#define TXPKTBUF_RQPN_PAGECNT_V2           (TX_PAGE_CNT_HPQ_V2 + TX_PAGE_CNT_NPQ_V2 + TX_PAGE_CNT_LPQ_V2 + TX_PAGE_CNT_EPQ_V2 + TX_PAGE_CNT_PUBQ_V2 + TX_PAGE_CNT_EX1Q_V2 + TX_PAGE_CNT_EX2Q_V2 )
#define TXPKTBUF_LLT_PAGECNT_V2            (TXPKTBUF_RQPN_PAGECNT_V2 + 1)
#endif //#if (IS_RTL8198F_SERIES)

#if (IS_RTL8197G_SERIES)
#define TXPKTBUF_RQPN_PAGECNT_V3           (TX_PAGE_CNT_HPQ_V3 + TX_PAGE_CNT_NPQ_V3 + TX_PAGE_CNT_LPQ_V3 + TX_PAGE_CNT_EPQ_V3 + TX_PAGE_CNT_PUBQ_V3 + TX_PAGE_CNT_EX1Q_V3 + TX_PAGE_CNT_EX2Q_V3 )
#define TXPKTBUF_LLT_PAGECNT_V3            (TXPKTBUF_RQPN_PAGECNT_V3 + 1)
#endif //#if (IS_RTL8198F_SERIES)

#if (IS_RTL8814B_SERIES)
// RQPN is set in halmac , these regs are no use//yllin8814B 
#endif //#if (IS_RTL8814B_SERIES)


//4 PBP: Packet Buffer Page
//Size Selection:
//  64 / 128 / 256 / 512 / 1024
#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8197F_SERIES || IS_RTL8192F_SERIES)
#define PBP_PSTX_SIZE                   256     // Filen: can't be modified in 92E/8881A
#define PBP_PSRX_SIZE                   128
#endif

#if (IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES || IS_RTL8198F_SERIES || IS_RTL8814B_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES|| IS_RTL8197G_SERIES)
#define PBP_PSTX_SIZE_V1                128     // Filen: can't be modified in 8814A
#define PBP_PSRX_SIZE_V1                64
#endif //#if (IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES || IS_RTL8198F_SERIES) 

//4 LLT Table
#define LLT_TABLE_INIT_POLLING_CNT      100

//4 TRX Pktbuf
#if (IS_RTL8192E_SERIES || IS_RTL8881A_SERIES || IS_RTL8197F_SERIES || IS_RTL8198F_SERIES || IS_RTL8192F_SERIES|| IS_RTL8197G_SERIES)
#define MAC_RXFF_SIZE               0x3E7F  // 16*1024 = 16384, 384 for C2H Pkt, 16000-1=15999=0x3E7F, because real location is 0~16383
#endif
#if (IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES)
#define MAC_RXFF_SIZE_V1            0x5EFF  // 24*1024 = 24576, 256 for C2H Pkt, 24576-256-1=24319=0x5EFF
#endif //#if (IS_RTL8814A_SERIES || IS_RTL8197F_SERIES)
#if IS_RTL8814B_SERIES
#define MAC_RXFF_SIZE_V2             0x7EF3//32*1024 = 32768, 268 for C2H pkt, take 32500-1=0x7EF3
#endif

#define NUM_AC_QUEUE                8

#if IS_RTL8814B_SERIES
#define NUM_AC_QUEUE_V1                20
#endif

#if (IS_RTL8198F_SERIES || IS_RTL8197G_SERIES)
#define NUM_AC_QUEUE_V2                16
#endif
//4 TRX DMA Queue
//00: Extra priority queue
//01: Low priority queue
//10: Normal priority queue
//11: High priority queue
//100: extra_1 queue
//101: extra_2 queue
#define TXDMA_MAP_EXTRA     0x0
#define TXDMA_MAP_LOW       0x1
#define TXDMA_MAP_NORMAL    0x2
#define TXDMA_MAP_HIGH      0x3
#define TXDMA_MAP_EXTRA1    0x4
#define TXDMA_MAP_EXTRA2    0x5

//Setting
#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
#define TXDMA_VOQ_MAP_SEL   TXDMA_MAP_NORMAL
#define TXDMA_VIQ_MAP_SEL   (( Adapter->hci_type == RTL_HCI_PCIE && Adapter->pshare->rf_ft_var.txbuf_merge) ? TXDMA_MAP_NORMAL : TXDMA_MAP_LOW)
#define TXDMA_BEQ_MAP_SEL  	TXDMA_MAP_NORMAL
#define TXDMA_BKQ_MAP_SEL   (( Adapter->hci_type == RTL_HCI_PCIE && Adapter->pshare->rf_ft_var.txbuf_merge) ? TXDMA_MAP_NORMAL : TXDMA_MAP_LOW)
#define TXDMA_MGQ_MAP_SEL   TXDMA_MAP_EXTRA
#define TXDMA_HIQ_MAP_SEL   TXDMA_MAP_HIGH
#define TXDMA_CMQ_MAP_SEL   TXDMA_MAP_EXTRA

#if defined(CONFIG_RTL_92F_SUPPORT)
#define TXDMA_VOQ_MAP_SEL_V1   TXDMA_MAP_NORMAL
#define TXDMA_VIQ_MAP_SEL_V1   (( Adapter->pshare->rf_ft_var.txbuf_merge) ? TXDMA_MAP_NORMAL : TXDMA_MAP_LOW)
#define TXDMA_BEQ_MAP_SEL_V1  	TXDMA_MAP_NORMAL
#define TXDMA_BKQ_MAP_SEL_V1   (( Adapter->pshare->rf_ft_var.txbuf_merge) ? TXDMA_MAP_NORMAL : TXDMA_MAP_LOW)
#define TXDMA_MGQ_MAP_SEL_V1   TXDMA_MAP_EXTRA
#endif /* CONFIG_RTL_92F_SUPPORT */

#else /* !defined(CONFIG_RTL_TRIBAND_SUPPORT) */

#if IS_EXIST_PCI || IS_EXIST_EMBEDDED

#ifdef WMM_DSCP_C42
#define TXDMA_VOQ_MAP_SEL   TXDMA_MAP_NORMAL
#define TXDMA_VIQ_MAP_SEL   TXDMA_MAP_LOW
#define TXDMA_BEQ_MAP_SEL   TXDMA_MAP_NORMAL
#define TXDMA_BKQ_MAP_SEL   TXDMA_MAP_EXTRA
#define TXDMA_MGQ_MAP_SEL   TXDMA_MAP_EXTRA
#define TXDMA_VOQ_MAP_SEL_14   TXDMA_MAP_HIGH
#define TXDMA_VIQ_MAP_SEL_14   TXDMA_MAP_LOW
#define TXDMA_BEQ_MAP_SEL_14   TXDMA_MAP_NORMAL
#define TXDMA_BKQ_MAP_SEL_14   TXDMA_MAP_EXTRA
#elif IS_RTL8197F_SERIES
#define TXDMA_VOQ_MAP_SEL   TXDMA_MAP_NORMAL
#define TXDMA_VIQ_MAP_SEL   TXDMA_MAP_EXTRA
#define TXDMA_BEQ_MAP_SEL   TXDMA_MAP_LOW
#define TXDMA_BKQ_MAP_SEL   TXDMA_MAP_LOW
#define TXDMA_MGQ_MAP_SEL   TXDMA_MAP_EXTRA

#if (IS_RTL8198F_SERIES || IS_RTL8197G_SERIES)
#define TXDMA_VOQ_MAP_SEL_V1   TXDMA_MAP_NORMAL
#define TXDMA_VIQ_MAP_SEL_V1   TXDMA_MAP_LOW
#define TXDMA_BEQ_MAP_SEL_V1   TXDMA_MAP_EXTRA1
#define TXDMA_BKQ_MAP_SEL_V1   TXDMA_MAP_EXTRA2
#endif //IS_RTL8198F_SERIES || IS_RTL8197G_SERIES

#else // WMM_DSCP_C42
#define TXDMA_VOQ_MAP_SEL   TXDMA_MAP_NORMAL
#define TXDMA_VIQ_MAP_SEL   (( Adapter->pshare->rf_ft_var.txbuf_merge) ? TXDMA_MAP_NORMAL : TXDMA_MAP_LOW)
#define TXDMA_BEQ_MAP_SEL  	TXDMA_MAP_NORMAL
#define TXDMA_BKQ_MAP_SEL   (( Adapter->pshare->rf_ft_var.txbuf_merge) ? TXDMA_MAP_NORMAL : TXDMA_MAP_LOW)
#define TXDMA_MGQ_MAP_SEL   TXDMA_MAP_EXTRA

#if IS_RTL8192F_SERIES
#define TXDMA_VOQ_MAP_SEL_V1   TXDMA_MAP_NORMAL
#define TXDMA_VIQ_MAP_SEL_V1   (( Adapter->pshare->rf_ft_var.txbuf_merge) ? TXDMA_MAP_NORMAL : TXDMA_MAP_LOW)
#define TXDMA_BEQ_MAP_SEL_V1  	TXDMA_MAP_NORMAL
#define TXDMA_BKQ_MAP_SEL_V1   (( Adapter->pshare->rf_ft_var.txbuf_merge) ? TXDMA_MAP_NORMAL : TXDMA_MAP_LOW)
#define TXDMA_MGQ_MAP_SEL_V1   TXDMA_MAP_EXTRA
#endif

#if (IS_RTL8198F_SERIES || IS_RTL8197G_SERIES)
#define TXDMA_VOQ_MAP_SEL_V1   TXDMA_MAP_NORMAL
#define TXDMA_VIQ_MAP_SEL_V1   TXDMA_MAP_LOW
#define TXDMA_BEQ_MAP_SEL_V1   TXDMA_MAP_EXTRA1
#define TXDMA_BKQ_MAP_SEL_V1   TXDMA_MAP_EXTRA2
#endif //IS_RTL8198F_SERIES || IS_RTL8197G_SERIES

#endif //WMM_DSCP_C42
#define TXDMA_HIQ_MAP_SEL   TXDMA_MAP_HIGH
#define TXDMA_CMQ_MAP_SEL   TXDMA_MAP_EXTRA
#endif // IS_EXIST_PCI || IS_EXIST_EMBEDDED

#if IS_EXIST_SDIO
#define TXDMA_VOQ_MAP_SEL   TXDMA_MAP_NORMAL
#define TXDMA_VIQ_MAP_SEL   TXDMA_MAP_LOW
#define TXDMA_BEQ_MAP_SEL   TXDMA_MAP_NORMAL
#define TXDMA_BKQ_MAP_SEL   TXDMA_MAP_LOW
#ifdef CONFIG_SDIO_TX_INTERRUPT
#define TXDMA_MGQ_MAP_SEL   TXDMA_MAP_HIGH
#else
#define TXDMA_MGQ_MAP_SEL   TXDMA_MAP_EXTRA
#endif
#define TXDMA_HIQ_MAP_SEL   TXDMA_MAP_HIGH
#define TXDMA_CMQ_MAP_SEL   TXDMA_MAP_EXTRA
#endif // IS_EXIST_SDIO
#endif /* defined(CONFIG_RTL_TRIBAND_SUPPORT) */

#if 0
//#define TRX_DMA_QUEUE_MAP_PARA          0xC660
#define TRX_DMA_QUEUE_MAP_PARA          (BIT_TXDMA_VOQ_MAP(TXDMA_VOQ_MAP_SEL)|BIT_TXDMA_VIQ_MAP(TXDMA_VIQ_MAP_SEL)|\
                                        BIT_TXDMA_BEQ_MAP(TXDMA_BEQ_MAP_SEL)|BIT_TXDMA_BKQ_MAP(TXDMA_BKQ_MAP_SEL)|\
                                        BIT_TXDMA_MGQ_MAP(TXDMA_MGQ_MAP_SEL)|BIT_TXDMA_HIQ_MAP(TXDMA_HIQ_MAP_SEL)|\
                                        BIT_TXDMA_CMQ_MAP(TXDMA_CMQ_MAP_SEL))
#else
// 8814A no support cmd queue mapping
#if  (IS_RTL8192F_SERIES || IS_RTL8198F_SERIES || IS_RTL8197G_SERIES)
#define TRX_DMA_QUEUE_MAP_PARA_V1      (BIT_TXDMA_VOQ_MAP_V1(TXDMA_VOQ_MAP_SEL_V1)|BIT_TXDMA_VIQ_MAP_V1(TXDMA_VIQ_MAP_SEL_V1)|\
                                        BIT_TXDMA_BEQ_MAP_V1(TXDMA_BEQ_MAP_SEL_V1)|BIT_TXDMA_BKQ_MAP_V1(TXDMA_BKQ_MAP_SEL_V1)|\
                                        BIT_TXDMA_MGQ_MAP_V1(TXDMA_MGQ_MAP_SEL)|BIT_TXDMA_HIQ_MAP_V1(TXDMA_HIQ_MAP_SEL))
#endif 

#define TRX_DMA_QUEUE_MAP_PARA          (BIT_TXDMA_VOQ_MAP(TXDMA_VOQ_MAP_SEL)|BIT_TXDMA_VIQ_MAP(TXDMA_VIQ_MAP_SEL)|\
                                        BIT_TXDMA_BEQ_MAP(TXDMA_BEQ_MAP_SEL)|BIT_TXDMA_BKQ_MAP(TXDMA_BKQ_MAP_SEL)|\
                                        BIT_TXDMA_MGQ_MAP(TXDMA_MGQ_MAP_SEL)|BIT_TXDMA_HIQ_MAP(TXDMA_HIQ_MAP_SEL))
#ifdef WMM_DSCP_C42                                        
#define TRX_DMA_QUEUE_MAP_PARA_14      	(BIT_TXDMA_VOQ_MAP(TXDMA_VOQ_MAP_SEL_14)|BIT_TXDMA_VIQ_MAP(TXDMA_VIQ_MAP_SEL_14)|\
                                        BIT_TXDMA_BEQ_MAP(TXDMA_BEQ_MAP_SEL_14)|BIT_TXDMA_BKQ_MAP(TXDMA_BKQ_MAP_SEL_14)|\
                                        BIT_TXDMA_MGQ_MAP(TXDMA_MGQ_MAP_SEL)|BIT_TXDMA_HIQ_MAP(TXDMA_HIQ_MAP_SEL))
#else
#define TRX_DMA_QUEUE_MAP_PARA_14		TRX_DMA_QUEUE_MAP_PARA
#endif
#endif
#define CHECK_DOWNLOAD_RSVD_PAGE_READY_TIMES        10

#define USED_PAGE_NUM_PER_MMPDU		13  // (1500B Payload +26B Header +40B TXDESC /128 page size)

//4 Retry Limit
#if defined(CONFIG_RTL8672) || defined(NOT_RTK_BSP)
#define RETRY_LIMIT_SHORT_AP        0x30
#define RETRY_LIMIT_SHORT_AP_2G       0x30
#define RETRY_LIMIT_LONG_AP         0x30
#define RETRY_LIMIT_LONG_AP_2G         0x30
#else
#define RETRY_LIMIT_SHORT_AP        0x10
#define RETRY_LIMIT_SHORT_AP_2G        0x10
#define RETRY_LIMIT_LONG_AP         0x10
#define RETRY_LIMIT_LONG_AP_2G         0x10
#endif
#define RETRY_LIMIT_SHORT_CLIENT    0x30
#define RETRY_LIMIT_LONG_CLIENT     0x30

//4 CAM

#define HAL_TOTAL_CAM_ENTRY(Adapter)         (Adapter->pshare->total_cam_entry)

#define HAL_CAM_CONTENT_COUNT               8
#define HAL_CAM_CONTENT_USABLE_COUNT        6

//4 Packet count report
#define     FUNCTRL_ADDR_CNT0CTRL	0x100
#define     FUNCTRL_ADDR_CNT0TRX	0x01
#define     FUNCTRL_ADDR_CNT1CTRL   0x110
#define     FUNCTRL_ADDR_CNT1TRX    0x11
#define     FUNCTRL_ADDR_CNT2CTRL   0x120
#define     FUNCTRL_ADDR_CNT2TRX    0x21
#define     FUNCTRL_ADDR_CNT3CTRL   0x130
#define     FUNCTRL_ADDR_CNT3TRX    0x31
#define     FUNCTRL_ADDR_CNT4CTRL   0x140
#define     FUNCTRL_ADDR_CNT4TRX    0x41
#define     FUNCTRL_ADDR_CNT5CTRL   0x150
#define     FUNCTRL_ADDR_CNT5TRX    0x51
#define     FUNCTRL_ADDR_CNT6CTRL   0x160
#define     FUNCTRL_ADDR_CNT6TRX    0x61
#define     FUNCTRL_ADDR_CNT7CTRL   0x170
#define     FUNCTRL_ADDR_CNT7TRX    0x71
#if (IS_RTL8198F_SERIES || IS_RTL8814B_SERIES || IS_RTL8197G_SERIES)
#define     FUNCTRL_ADDR_CNT8CTRL	0x180
#define     FUNCTRL_ADDR_CNT8TRX	0x81
#define     FUNCTRL_ADDR_CNT9CTRL   0x190
#define     FUNCTRL_ADDR_CNT9TRX    0x91
#define     FUNCTRL_ADDR_CNT10CTRL   0x1A0
#define     FUNCTRL_ADDR_CNT10TRX    0xA1
#define     FUNCTRL_ADDR_CNT11CTRL   0x1B0
#define     FUNCTRL_ADDR_CNT11TRX    0xB1
#define     FUNCTRL_ADDR_CNT12CTRL   0x1C0
#define     FUNCTRL_ADDR_CNT12TRX    0xC1
#define     FUNCTRL_ADDR_CNT13CTRL   0x1D0
#define     FUNCTRL_ADDR_CNT13TRX    0xD1
#define     FUNCTRL_ADDR_CNT14CTRL   0x1E0
#define     FUNCTRL_ADDR_CNT14TRX    0xE1
#define     FUNCTRL_ADDR_CNT15CTRL   0x1F0
#define     FUNCTRL_ADDR_CNT15TRX    0xF1
#endif //#if (IS_RTL8198F_SERIES || IS_RTL8814B_SERIES || IS_RTL8197G_SERIES)

//4 Firmware
#define	RT_FIRMWARE_HDR_SIZE	            32
#define DOWNLOAD_FIRMWARE_RETRY_TIMES       5
#define FW_DOWNLOAD_START_ADDRESS           0x1000
#define CHECK_FW_RAMCODE_READY_TIMES        10
#define CHECK_FW_RAMCODE_READY_DELAY_MS     20
#define H2CBUF_OCCUPY_DELAY_CNT             30
#define H2CBUF_OCCUPY_DELAY_US              10    
#define C2H_CONTENT_LEN                     12
#define LENGTH_C2HEXT_CONTENT				228 //256-24-4
// For 3081 FW 
#if IS_RTL88XX_FW_MIPS
#define MIPS_DL_IMEM                        0
#define MIPS_DL_DMEM                        1
#define MIPS_FW_HEADER_SIZE                 64
#define MIPS_FW_CHKSUM_DUMMY_SZ             8
#define MIPS_MAX_FWBLOCK_DL_SIZE            0x8000  // 32K
#define OCPBASE_IMEM                        0x00000000
#define OCPBASE_DMEM                        0x00200000

#define OCPBASE_RPTBUF                      0x18660000
#define OCPBASE_RXBUF2                      0x18680000
#define OCPBASE_RXBUF                       0x18700000
#define OCPBASE_TXBUF                       0x18780000

// 8051FWDL
#define FWDL_EN                 BIT0
#define IMEM_BOOT_DL_RDY        BIT1
#define IMEM_BOOT_CHKSUM_FAIL   BIT2
#define IMEM_DL_RDY             BIT3
#define IMEM_CHKSUM_FAIL        BIT4
#define DMEM_DL_RDY             BIT5
#define DMEM_CHKSUM_FAIL        BIT6
#define EMEM_DL_RDY             BIT7
#define EMEM_CHKSUM_FAIL        BIT8
#define EMEM_TXBUF_DL_RDY       BIT9
#define EMEM_TXBUF_CHKSUM_FAIL  BIT10
#define CPU_CLK_SWITCH_BUSY     BIT11
#define CPU_CLK_SEL             (BIT12|BIT13)
#define FWDL_OK                 BIT14
#define FW_INIT_RDY             BIT15
#define R_EN_BOOT_FLASH         BIT20

#endif //if IS_RTL88XX_FW_MIPS

//4 MAC Sleep 
#define MACID_REGION1_LIMIT                 31
#define MACID_REGION2_LIMIT                 63
#define MACID_REGION3_LIMIT                 95

//4 HW_FILL_MACID
#define HW_MACID_SEARCH_NOT_READY           0x7E
#define HW_MACID_SEARCH_FAIL                0x7F
#define HW_MACID_SEARCH_FAIL_V1             0xFF
#define HW_MACID_SEARCH_SUPPORT_NUM         (HW_MACID_SEARCH_NOT_READY-1)
#define HW_MACID_SEARCH_SUPPORT_NUM_V1      0x7F
#define HAL_HWMACID_RESULT_SUCCESS          0                
#define HAL_HWMACID_RESULT_NOT_READY        1
#define HAL_HWMACID_RESULT_FAIL             2

//4 Beacon Related
#define BEACON_ERALY_INIT_TIME              10

//4 BB RF 
#define CHANNEL_MAX_NUMBER_2G	            14	


//4 TXRPT buffer related 
#define MaxMacIDNum                         128   
#define MaxMacIDGroupNum                    11
#define RPTBufferStartHiAddr                0x660    // ReportBuffer base address: 0x18660000
#define CRC5StartAddr_V1                    0xC00
#define CRC5StartAddr                       0x800    // CRC5 start address in RPTbuffer: 0x18661800   
#define CRC5BitNum                          5       
#define CRC5GroupBitNum                     64
#define CRC5GroupByteNum                    8
#define macIDNumperGroup                    12       // 64/5=12
#define RWCtrlBit                           BIT15
#define CRCAcrossSHT                        30       // The six number of CRC val will across the 32bit, start at bit30
#define SecondBitSHT                        2       
#define CRC5EndBit                          BIT28
#define CRC5ValidBit                        BIT29      

//4 SMH CAM
#define SMHCAM_IS_FULL                         0xFF
#define SMHCAM_SEARCH_FAIL                     0xFF

#if (IS_RTL8814B_SERIES)
//Address CAM related
#define ADDR_CAM_Valid                      BIT0
//Interrupt for PORT 0~ 4
#define ADDR_CAM_Port_INT_Mask              (BIT1|BIT2|BIT3)
#define ADDR_CAM_Port_INT_0                 
#define ADDR_CAM_Port_INT_1                 BIT1
#define ADDR_CAM_Port_INT_2                 BIT2
#define ADDR_CAM_Port_INT_3                 (BIT1|BIT2)
#define ADDR_CAM_Port_INT_4                 BIT3
#define ADDR_CAM_Port_INT_none              (BIT1|BIT2|BIT3)
//TSF_sync  for timer 0~4
#define ADDR_CAM_TSF_sync_0                 
#define ADDR_CAM_TSF_sync_1                 BIT4
#define ADDR_CAM_TSF_sync_2                 BIT5
#define ADDR_CAM_TSF_sync_3                 BIT4|BIT5
#define ADDR_CAM_TSF_sync_4                 BIT6
#define ADDR_CAM_TSF_sync_none              BIT4|BIT5|BIT6
#define ADDR_CAM_MACID_mask                 0x7F
#define ADDR_CAM_MACID_shift                8
#define SET_ADDR_CAM_MACID(x)               ((x) & ADDR_CAM_MACID_mask) << ADDR_CAM_MACID_shift
#define ADDR_CAM_Network_type_No_Link          0
#define ADDR_CAM_Network_type_AdHoc             BIT16
#define ADDR_CAM_Network_type_Infrastructure    BIT17
#define ADDR_CAM_Network_type_AP                (BIT16|BIT17)
// Rx control frame packet counts for A1 match 
// PKTCNT_RWD 0x7b8  PKTCNT_CTRL 0x7bc
#define ADDR_CAM_CtrlCnt_MASK              0xf
#define ADDR_CAM_CtrlCnt_shift              18
#define ADDR_CAM_SET_CtrlCnt(x)             ((x) & ADDR_CAM_CtrlCnt_MASK) << ADDR_CAM_CtrlCnt_shift

#define ADDR_CAM_CtrlCnt_Valid              BIT22

#define ADDR_CAM_LSIG_TXOP                  BIT24
#define ADDR_CAM_BA_Search_A2               ~BIT25  //default
#define ADDR_CAM_BA_Search_A1               BIT25

#define ADDR_CAM_BCN_Hit_Condition_A3       ~(BIT26|BIT27)  //default
#define ADDR_CAM_BCN_Hit_Condition_A2       (BIT26&(~BIT27))
#define ADDR_CAM_BCN_Hit_Condition_A2_AND_3 ((~BIT26)|BIT27)
#define ADDR_CAM_BCN_Hit_Condition_A2_OR_3  (BIT26|BIT27)

#define ADDR_CAM_WOL_Pattern                BIT28
#define ADDR_CAM_WOL_Unicast                BIT29     
#define ADDR_CAM_WOL_MAGIC                  BIT30
#endif //(IS_RTL8814B_SERIES)

#if HALMAC_8192E_SUPPORT
#define TXRPT_START_ADDR                    0x8100
#define TXRPT_SIZE          16

#endif

#if HALMAC_8192F_SUPPORT
#define CTRLINFO_START_ADDR    0x8000
#define CTRLINFO_SIZE         16

#define SPERPT_START_ADDR   0x8800
#define SPERPT_SIZE         8
#define SPERPT_NUM          16

#define BCNRPT_START_ADDR   0x8880
#define BCNRPT_SIZE         16
#define BCNRPT_NUM          8

#define TXRPT_START_ADDR    0x8900
#define TXRPT_SIZE          24
#define PAUSE_TXRPT_TIME    (12)

#define NANBCNRPT_START_ADDR    0x9500
#define NANBCNRPT_SIZE              128

#define SWPSRPT_START_ADDR    0x9580
#define SWPSRPT_SIZE              2136
#define SWPSRPTBit            BIT16    

#define CRC5_START_ADDR     0x9DD8
#define CRC5_SIZE                   88

#define RAMask_START_ADDR   0x9E30
#define RAMASK_SIZE         8
#endif
#if IS_EXIST_PCI
//4 PCIE Configuration
//TXDMA Burst Size selection default 7.
//0:16; 1:32; 2:64; 3:128; 4:256; 5:512; 6:1024; 7:2048 bytes.
#ifdef CONFIG_RTL_8198C
#define PCIE_TXDMA_BURST_SIZE               0x4 //0x7
#else
#define PCIE_TXDMA_BURST_SIZE               0x7
#endif

//RXDMA Burst Size selection default 7.
//0:16bytes; 1:32bytes; 2:64bytes; 3~7: 128bytes
#ifdef CONFIG_RTL_8198C
#define PCIE_RXDMA_BURST_SIZE               0x3 //0x7
#else
#define PCIE_RXDMA_BURST_SIZE               0x7
#endif
#endif  //IS_EXIST_PCI

#if IS_EXIST_EMBEDDED
//4 LBUS Configuration
//TXDMA Burst Size selection default 1.
//0:16; 1:32; 2:64;
#define LBUS_TXDMA_BURST_SIZE               0x1

//RXDMA Burst Size selection default 1.
//0:16; 1:32; 2:64;
#define LBUS_RXDMA_BURST_SIZE               0x1
#endif  //IS_EXIST_EMBEDDED

//4  TX/RX BD

//Avoid IO/DMA Racing Method
//  0: check totalpktlen
//  1: check tag
#define RXBD_AVOID_RACING_METHOD    0

// TXBD 32BIT/64BIT System Selection
// 0: 32BIT(8 Bytes each segment)
// 1: 64BIT(16 Bytes each segment)
#define TXBD_SEG_32_64_SEL      0

// TXBD Segment Selection
// 0: 2 Segment 
// 1: 4 Segment
// 2: 8 Segment
#define TX_VOQ_TXBD_MODE_SEL    1
#define TX_VIQ_TXBD_MODE_SEL    1    
#define TX_BEQ_TXBD_MODE_SEL    1        
#define TX_BKQ_TXBD_MODE_SEL    1
#define TX_MGQ_TXBD_MODE_SEL    1        
#define TX_BCNQ_TXBD_MODE_SEL   1
#define TX_HI0Q_TXBD_MODE_SEL   1        
#define TX_HI1Q_TXBD_MODE_SEL   1
#define TX_HI2Q_TXBD_MODE_SEL   1        
#define TX_HI3Q_TXBD_MODE_SEL   1
#define TX_HI4Q_TXBD_MODE_SEL   1        
#define TX_HI5Q_TXBD_MODE_SEL   1
#define TX_HI6Q_TXBD_MODE_SEL   1        
#define TX_HI7Q_TXBD_MODE_SEL   1
#define TX_HI8Q_TXBD_MODE_SEL   1
#define TX_HI9Q_TXBD_MODE_SEL   1
#define TX_HI10Q_TXBD_MODE_SEL   1
#define TX_HI11Q_TXBD_MODE_SEL   1
#define TX_HI12Q_TXBD_MODE_SEL   1
#define TX_HI13Q_TXBD_MODE_SEL   1
#define TX_HI14Q_TXBD_MODE_SEL   1
#define TX_HI15Q_TXBD_MODE_SEL   1


#ifdef IS_EXIST_RTL8814BE
#define TX_HI16Q_TXBD_MODE_SEL   1
#define TX_HI17Q_TXBD_MODE_SEL   1
#define TX_HI18Q_TXBD_MODE_SEL   1
#define TX_HI19Q_TXBD_MODE_SEL   1
#endif //#ifdef IS_EXIST_RTL8814BE
#define TX_H2CQ_TXBD_MODE_SEL   1

#if IS_EXIST_RTL8814BE
#define TX_ACH4_TXBD_MODE_SEL    1
#define TX_ACH5_TXBD_MODE_SEL    1
#define TX_ACH6_TXBD_MODE_SEL    1
#define TX_ACH7_TXBD_MODE_SEL    1
#define TX_ACH8_TXBD_MODE_SEL    1
#define TX_ACH9_TXBD_MODE_SEL    1
#define TX_ACH10_TXBD_MODE_SEL    1
#define TX_ACH11_TXBD_MODE_SEL    1
#define TX_ACH12_TXBD_MODE_SEL    1
#define TX_ACH13_TXBD_MODE_SEL    1
#ifdef WLAN_SUPPORT_FW_CMD
#define TX_FWCMD_MODE_SEL    1
#endif
#endif

// TX_XXQ_TXBD_MODE_SEL: 0 ==> 2
// TX_XXQ_TXBD_MODE_SEL: 1 ==> 4
// TX_XXQ_TXBD_MODE_SEL: 2 ==> 8
#define TXBD_ELE_NUM            4


// Tx
#if defined(__OSK__) && defined(OSK_LOW_TX_DESC)
#define TX_VOQ_TXBD_NUM		    NONBE_TXDESC
#define TX_VIQ_TXBD_NUM		    NONBE_TXDESC
#define TX_BEQ_TXBD_NUM		    BE_TXDESC
#define TX_BKQ_TXBD_NUM		    NONBE_TXDESC
#define TX_MGQ_TXBD_NUM         NONBE_TXDESC
#else
#define TX_VOQ_TXBD_NUM		    NUM_TX_DESC
#define TX_VIQ_TXBD_NUM		    NUM_TX_DESC
#define TX_BEQ_TXBD_NUM		    NUM_TX_DESC
#define TX_BKQ_TXBD_NUM		    NUM_TX_DESC
#define TX_MGQ_TXBD_NUM         NUM_TX_DESC
#endif

#if (IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822BE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8198FEM || IS_EXIST_RTL8814BE || IS_RTL8192F_SERIES || IS_EXIST_RTL8812FE || IS_EXIST_RTL8197GEM )
#define TX_BCNQ_TXBD_NUM_V1 	(1+HAL_NUM_VWLAN)*(TXBD_BEACON_OFFSET_V1/sizeof(TX_BUFFER_DESCRIPTOR))  // Root + VAP Num
#endif //(IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM ||IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8198FEM || IS_RTL8192F_SERIES|| IS_EXIST_RTL8814BE || IS_EXIST_RTL8197GEM)

#if IS_EXIST_RTL8881AEM
#define TX_BCNQ_TXBD_NUM_V2	(1+HAL_NUM_VWLAN)*(TXBD_BEACON_OFFSET_V2/sizeof(TX_BUFFER_DESCRIPTOR))  // Root + VAP Num
#endif //IS_EXIST_RTL8881AEM

#if 0
#if IS_EXIST_RTL8192EE
#define TX_BCNQ_TXBD_NUM_8192E	(1+HAL_NUM_VWLAN)*(TXBD_BEACON_OFFSET_V1/sizeof(TX_BUFFER_DESCRIPTOR))  // Root + VAP Num
#endif //IS_EXIST_RTL8192EE

#if IS_EXIST_RTL8881AEM
#define TX_BCNQ_TXBD_NUM_8881A	(1+HAL_NUM_VWLAN)*(TXBD_BEACON_OFFSET_V2/sizeof(TX_BUFFER_DESCRIPTOR))  // Root + VAP Num
#endif //IS_EXIST_RTL8881AEM

#if IS_EXIST_RTL8814AE
#define TX_BCNQ_TXBD_NUM_8814AE	(1+HAL_NUM_VWLAN)*(TXBD_BEACON_OFFSET_V1/sizeof(TX_BUFFER_DESCRIPTOR))  // Root + VAP Num
#endif //IS_EXIST_RTL8814AE
#endif 

#define NUM_TX_DESC_CMDQ        32

#define TX_HI0Q_TXBD_NUM	    NUM_TX_DESC_HQ
#define TX_HI1Q_TXBD_NUM	    NUM_TX_DESC_HQ
#define TX_HI2Q_TXBD_NUM	    NUM_TX_DESC_HQ
#define TX_HI3Q_TXBD_NUM	    NUM_TX_DESC_HQ
#define TX_HI4Q_TXBD_NUM	    NUM_TX_DESC_HQ
#define TX_HI5Q_TXBD_NUM	    NUM_TX_DESC_HQ
#define TX_HI6Q_TXBD_NUM	    NUM_TX_DESC_HQ
#define TX_HI7Q_TXBD_NUM	    NUM_TX_DESC_HQ
#if (IS_EXIST_RTL8198FEM || IS_EXIST_RTL8814BE)
#define TX_HI8Q_TXBD_NUM	    NUM_TX_DESC_HQ
#define TX_HI9Q_TXBD_NUM	    NUM_TX_DESC_HQ
#define TX_HI10Q_TXBD_NUM	    NUM_TX_DESC_HQ
#define TX_HI11Q_TXBD_NUM	    NUM_TX_DESC_HQ
#define TX_HI12Q_TXBD_NUM	    NUM_TX_DESC_HQ
#define TX_HI13Q_TXBD_NUM	    NUM_TX_DESC_HQ
#define TX_HI14Q_TXBD_NUM	    NUM_TX_DESC_HQ
#define TX_HI15Q_TXBD_NUM	    NUM_TX_DESC_HQ
#endif
#if IS_EXIST_RTL8814BE
#define TX_HI16Q_TXBD_NUM	    NUM_TX_DESC_HQ
#define TX_HI17Q_TXBD_NUM	    NUM_TX_DESC_HQ
#define TX_HI18Q_TXBD_NUM	    NUM_TX_DESC_HQ
#define TX_HI19Q_TXBD_NUM	    NUM_TX_DESC_HQ
#endif

#if IS_EXIST_RTL8814BE
#define TX_FWCMDQ_TXBD_NUM	    NUM_TX_DESC_CMDQ
#endif

#define TX_CMDQ_TXBD_NUM	    NUM_TX_DESC_CMDQ //H2CQ


//Get the max value in all queue
#define GET_MAX(a,b)    ((a)>(b)?(a):(b))
#define TX_Q_MAX_TXBD_NUM       GET_MAX(NUM_TX_DESC, NUM_TX_DESC_HQ)

//Rx
#define RX_Q_RXBD_NUM		    NUM_RX_DESC
#define RX_Q_RXBD_NUM_2G	    NUM_RX_DESC_2G

#define RX_Q_RXBD_NUM_IF(Adapter)	(Adapter->pshare->wlandev_idx == (0^WLANIDX)?RX_Q_RXBD_NUM:RX_Q_RXBD_NUM_2G)


//Total
#define TOTAL_NUM_TXBD_NO_BCN   (TX_MGQ_TXBD_NUM + TX_VOQ_TXBD_NUM + TX_VIQ_TXBD_NUM + TX_BEQ_TXBD_NUM + TX_BKQ_TXBD_NUM + \
                                    TX_HI0Q_TXBD_NUM + TX_HI1Q_TXBD_NUM + TX_HI2Q_TXBD_NUM + TX_HI3Q_TXBD_NUM + \
                                    TX_HI4Q_TXBD_NUM + TX_HI5Q_TXBD_NUM + TX_HI6Q_TXBD_NUM + TX_HI7Q_TXBD_NUM + TX_CMDQ_TXBD_NUM)

#define TOTAL_NUM_TXBD_NO_BCN_V1   (TX_MGQ_TXBD_NUM + TX_VOQ_TXBD_NUM + TX_VIQ_TXBD_NUM + TX_BEQ_TXBD_NUM + TX_BKQ_TXBD_NUM + \
                                    TX_HI0Q_TXBD_NUM + TX_HI1Q_TXBD_NUM + TX_HI2Q_TXBD_NUM + TX_HI3Q_TXBD_NUM + \
                                    TX_HI4Q_TXBD_NUM + TX_HI5Q_TXBD_NUM + TX_HI6Q_TXBD_NUM + TX_HI7Q_TXBD_NUM + \
                                    TX_HI8Q_TXBD_NUM + TX_HI9Q_TXBD_NUM + TX_HI10Q_TXBD_NUM + TX_HI11Q_TXBD_NUM + \
                                    TX_HI12Q_TXBD_NUM + TX_HI13Q_TXBD_NUM + TX_HI14Q_TXBD_NUM + TX_HI15Q_TXBD_NUM + \
                                    TX_CMDQ_TXBD_NUM)

#define TOTAL_NUM_RXBD          (RX_Q_RXBD_NUM)
#define TOTAL_NUM_RXBD_2G          (RX_Q_RXBD_NUM_2G)

#define TOTAL_NUM_RXBD_IF(Adapter)	(Adapter->pshare->wlandev_idx == (0^WLANIDX)?TOTAL_NUM_RXBD:TOTAL_NUM_RXBD_2G)


// Note: no RX_DESC here.....
#if 0
#define DESC_DMA_SIZE_NO_BCNQ \
    (TOTAL_NUM_RXBD * sizeof(RX_BUFFER_DESCRIPTOR) + \
     TOTAL_NUM_TXBD_NO_BCN * (sizeof(TX_BUFFER_DESCRIPTOR) + SIZE_TXDESC_88XX))
#else
#define DESC_DMA_SIZE_NO_BCNQ \
    (TOTAL_NUM_RXBD * sizeof(RX_BUFFER_DESCRIPTOR) + \
     TOTAL_NUM_TXBD_NO_BCN * (sizeof(TX_BUFFER_DESCRIPTOR) + SIZE_TXDESC_88XX_MAX))
     
#define DESC_DMA_SIZE_NO_BCNQ_IF(Adapter) \
		(TOTAL_NUM_RXBD_IF(Adapter) * sizeof(RX_BUFFER_DESCRIPTOR) + \
		 TOTAL_NUM_TXBD_NO_BCN * (sizeof(TX_BUFFER_DESCRIPTOR) + SIZE_TXDESC_88XX_MAX))
     
#endif 

#define HAL_PAGE_SIZE   PAGE_SIZE

#if IS_RTL8192E_SERIES || IS_RTL8192F_SERIES
#define DESC_DMA_SIZE_8192E (DESC_DMA_SIZE_NO_BCNQ + \
                             (1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1 + \
                             (1+HAL_NUM_VWLAN) * SIZE_TXDESC_88XX)

#define DESC_DMA_PAGE_SIZE_8192E    (DESC_DMA_SIZE_8192E + HAL_PAGE_SIZE)
#endif  // IS_RTL8192E_SERIES

#if IS_RTL8881A_SERIES
#define DESC_DMA_SIZE_8881A (DESC_DMA_SIZE_NO_BCNQ + \
                             (1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V2 + \
                             (1+HAL_NUM_VWLAN) * SIZE_TXDESC_88XX)
    
#define DESC_DMA_PAGE_SIZE_8881A    (DESC_DMA_SIZE_8881A + HAL_PAGE_SIZE)
#endif  // IS_RTL8881A_SERIES

#if 0
#define DESC_DMA_SIZE_MAX (DESC_DMA_SIZE_NO_BCNQ + \
                             (1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_MAX + \
                             (1+HAL_NUM_VWLAN) * SIZE_TXDESC_88XX)
#else
// After 8822B, TXDESC size increase to 48 bytes
#define DESC_DMA_SIZE_MAX (DESC_DMA_SIZE_NO_BCNQ + \
                             (1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_MAX + \
                             (1+HAL_NUM_VWLAN) * SIZE_TXDESC_88XX_MAX)
                             
#define DESC_DMA_SIZE_MAX_IF(Adapter) (DESC_DMA_SIZE_NO_BCNQ_IF(Adapter) + \
                             (1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_MAX + \
                             (1+HAL_NUM_VWLAN) * SIZE_TXDESC_88XX_MAX)                             
#endif


#define DESC_DMA_PAGE_SIZE_MAX_HAL    (DESC_DMA_SIZE_MAX + HAL_PAGE_SIZE)
#define DESC_DMA_PAGE_SIZE_MAX_HAL_IF(Adapter)    (DESC_DMA_SIZE_MAX_IF(Adapter) + HAL_PAGE_SIZE)

#if CFG_HAL_TX_AMSDU
#define TOTAL_NUM_TXBD_FOR_AMSDU    (TX_VOQ_TXBD_NUM + TX_VIQ_TXBD_NUM + TX_BEQ_TXBD_NUM + TX_BKQ_TXBD_NUM)
#define DESC_DMA_SIZE_FOR_AMSDU     (TOTAL_NUM_TXBD_FOR_AMSDU * sizeof(TX_BUFFER_DESCRIPTOR_AMSDU))

// Note: MAX_NUM_OF_MSDU_IN_AMSDU must be the same as WLAN_HAL_TX_AMSDU_MAX_NUM
#define MAX_NUM_OF_MSDU_IN_AMSDU    WLAN_HAL_TX_AMSDU_MAX_NUM //TXBD_ELE_NUM
#endif

//Tx Condition Match
#define TX_CONDITION_MATCH_TXBD_CNT   10


//4 TXDESC
//#define HAL_TXDESC_OFFSET_SIZE      48 //temp modify to 48

//4 RXDESC

#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
// FramCtrl, DurID, A1, A2, A3, SeqNum, A4, Qos, HTCtrl, IV, EIV,
//       2   +    2   + 6 + 6 + 6 +    2    + 6 + 2  +   4    + 4 + 4  = 44 bytes 
// TODO: in test chip, we should add 8 byte (llc) for this max value, 
// TODO: i.e., HAL_WIFI_HEADER_LEN_MAX is (44+8) for 8814AE test chip
#define HAL_WIFI_HEADER_LEN_MAX		44
#define HAL_ETH_HEADER_LEN_MAX		WLAN_ETHHDR_LEN // 14 bytes
//#define HAL_HW_TXSC_HDR_CONV_OFFSET	(HAL_TXDESC_OFFSET_SIZE + HAL_WIFI_HEADER_LEN_MAX - HAL_ETH_HEADER_LEN_MAX + 2) // +2 for  four bytes alignment
#define HAL_HW_TXSC_HDR_CONV_ADD_OFFSET	(HAL_WIFI_HEADER_LEN_MAX - HAL_ETH_HEADER_LEN_MAX + 2) // +2 for  four bytes alignment
#define HAL_HW_TXSC_WHEADER_LEN		(HAL_ETH_HEADER_LEN_MAX >> 1)
#endif // CFG_HAL_HW_TX_SHORTCUT_HDR_CONV

#if CFG_HAL_AP_SWPS_OFFLOAD
#define SWPSRPT_BITMAP_SIZE 64
#define SWPSRPT_BITMAP_SIZE_V1 32
#define SWPSRPT_ENTRY_SIZE 16
#define SWPSRPT_ENTRY_SIZE_V1 64
#define SWPSRPT_ONE_RECORD_SIZE 4
#define SWPSRPT_ONE_RECORD_SIZE_V1 16
#define SWPS_MAX_MACID_NUM 128
#define SWPS_MAX_MACID_NUM_V1 64
#elif defined(AP_SWPS_OFFLOAD_WITH_AMPDU_VERI)
#define SWPSRPT_BITMAP_SIZE_V1 32
#define SWPSRPT_ENTRY_SIZE_V1 64
#define SWPSRPT_ONE_RECORD_SIZE_V1 16
#define SWPS_MAX_MACID_NUM_V1 64

#endif


//4 IMEM
#define HAL_IMEM    __IRAM_IN_865X

//4 MIPS16
#define HAL_MIPS16  __MIPS16


#endif  //IS_RTL88XX_GENERATION


//3 Mapping Basic Type 
#define VOID void
#define PVOID void *

#if 0
//typedef unsigned char   		BOOLEAN,*PBOOLEAN;
typedef unsigned char               *PBOOLEAN;
//typedef unsigned char			UCHAR, *PUCHAR;
//typedef unsigned short			USHORT, *PUSHORT;
//typedef short					SHORT;
//typedef unsigned int			ULONG, *PULONG;
//typedef long					LONG;

//typedef long long				LONGLONG;
//typedef unsigned int			UINT;
//typedef unsigned long long		ULONGLONG;
//typedef unsigned long long  	LARGE_INTEGER;

typedef unsigned char			u1Byte,*pu1Byte;
typedef unsigned short			u2Byte,*pu2Byte;
typedef unsigned int			u4Byte,*pu4Byte;
typedef unsigned long long		u8Byte,*pu8Byte;

typedef signed char				s1Byte,*ps1Byte;
typedef signed short			s2Byte,*ps2Byte;
typedef signed int				s4Byte,*ps4Byte;
typedef signed long long		s8Byte,*ps8Byte;
typedef unsigned long long		ULONG64,*PULONG64;

//typedef unsigned char			UINT8;
//typedef unsigned short			UINT16;
//typedef unsigned int			UINT32;
//typedef signed int				INT32;
//typedef signed char				INT8;
//typedef signed int				INT;

typedef const unsigned char	cu8;

typedef __signed char s8;
//typedef unsigned char u8;

typedef __signed short s16;
//typedef unsigned short u16;

typedef __signed int s32;
//typedef unsigned int u32;

typedef __signed__ long s64;
//typedef unsigned long u64;
#endif

//3  Mapping IO
#define HAL_PADAPTER    PRTL8192CD_PRIV
typedef struct stat_info        _HAL_STA_INFO,*P_HAL_STA_INFO;

#define HAL_PSTAINFO    P_HAL_STA_INFO

#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
#define HAL_RTL_R8(reg)		\
    (__HAL_RTL_R8(Adapter, reg))

#define HAL_RTL_R16(reg)	\
    (__HAL_RTL_R16(Adapter, reg))

#define HAL_RTL_R32(reg)	\
    (__HAL_RTL_R32(Adapter, reg))

#define HAL_RTL_W8(reg, val8)	\
    do { \
        __HAL_RTL_W8(Adapter, reg, val8); \
    } while (0)

#define HAL_RTL_W16(reg, val16)	\
    do { \
        __HAL_RTL_W16(Adapter, reg, val16); \
    } while (0)

#define HAL_RTL_W32(reg, val32)	\
    do { \
        __HAL_RTL_W32(Adapter, reg, val32) ; \
    } while (0)


#define platform_efio_read_1byte(Adapter,reg)        \
            (__HAL_RTL_R8(Adapter, reg))
#define platform_efio_read_2byte(Adapter,reg)        \
            (__HAL_RTL_R16(Adapter, reg))
#define platform_efio_read_4byte(Adapter,reg)        \
            (__HAL_RTL_R32(Adapter, reg))   


#define platform_efio_write_1byte(Adapter,reg,val8)        \
    do { \
        __HAL_RTL_W8(Adapter, reg, val8); \
    } while (0)            
#define platform_efio_write_2byte(Adapter,reg,val16)        \
    do { \
        __HAL_RTL_W16(Adapter, reg, val16); \
    } while (0)
#define platform_efio_write_4byte(Adapter,reg,val32)        \
    do { \
        __HAL_RTL_W32(Adapter, reg, val32) ; \
    } while (0)


#else /* !defined(CONFIG_RTL_TRIBAND_SUPPORT) */

#if defined(CONFIG_PCI_HCI)
#ifdef PCIE_POWER_SAVING_TEST //yllin
#define HAL_RTL_R8(reg)		\
	(( Adapter->pwr_state==L2  || Adapter->pwr_state==L1) ? 0 :(RTL_R8_F(Adapter, reg)) )

#define HAL_RTL_R16(reg)	\
	(( Adapter->pwr_state==L2  || Adapter->pwr_state==L1) ? 0 : (RTL_R16_F(Adapter, reg)))

#define HAL_RTL_R32(reg)	\
	(( Adapter->pwr_state==L2  || Adapter->pwr_state==L1) ? 0 : (RTL_R32_F(Adapter, reg)))

#define HAL_RTL_W8(reg, val8)	\
	do { \
	if( Adapter->pwr_state==L2  || Adapter->pwr_state==L1) \
		{  	printk("Error!!! w8:%x,%x in L%d\n", reg, val8, Adapter->pwr_state);} \
	else \
		RTL_W8_F(Adapter, reg, val8); \
	} while (0)

#define HAL_RTL_W16(reg, val16)	\
	do { \
	if( Adapter->pwr_state==L2  || Adapter->pwr_state==L1) \
		printk("Err!!! w16:%x,%x in L%d\n", reg, val16, Adapter->pwr_state); \
	else \
		RTL_W16_F(Adapter, reg, val16); \
	} while (0)

#define HAL_RTL_W32(reg, val32)	\
	do { \
	if( Adapter->pwr_state==L2  || Adapter->pwr_state==L1) \
		printk("Err!!! w32:%x,%x in L%d\n", reg, (unsigned int)val32, Adapter->pwr_state); \
	else \
		RTL_W32_F(Adapter, reg, val32) ; \
	} while (0)

#define PlatformEFIORead1Byte(Adapter,reg)        \
        (( Adapter->pwr_state==L2  || Adapter->pwr_state==L1) ? 0 :(RTL_R8_F(Adapter, reg)) )
#define PlatformEFIORead2Byte(Adapter,reg)        \
        (( Adapter->pwr_state==L2  || Adapter->pwr_state==L1) ? 0 : (RTL_R16_F(Adapter, reg)))
#define PlatformEFIORead4Byte(Adapter,reg)        \
        (( Adapter->pwr_state==L2  || Adapter->pwr_state==L1) ? 0 : (RTL_R32_F(Adapter, reg)))

#define platform_efio_read_1byte(Adapter,reg)		\
        (( Adapter->pwr_state==L2  || Adapter->pwr_state==L1) ? 0 :(RTL_R8_F(Adapter, reg)) )
    
#define platform_efio_read_2byte(Adapter,reg)	\
        (( Adapter->pwr_state==L2  || Adapter->pwr_state==L1) ? 0 : (RTL_R16_F(Adapter, reg)))
    
#define platform_efio_read_4byte(Adapter,reg)	\
        (( Adapter->pwr_state==L2  || Adapter->pwr_state==L1) ? 0 : (RTL_R32_F(Adapter, reg)))
    

#define PlatformEFIOWrite1Byte(Adapter,reg,val8)        \
        do { \
        if( Adapter->pwr_state==L2  || Adapter->pwr_state==L1) \
            {   printk("Error!!! w8:%x,%x in L%d\n", reg, val8, Adapter->pwr_state);} \
        else \
            RTL_W8_F(Adapter, reg, val8); \
        } while (0)
    
#define PlatformEFIOWrite2Byte(Adapter,reg,val16)        \
        do { \
        if( Adapter->pwr_state==L2  || Adapter->pwr_state==L1) \
            printk("Err!!! w16:%x,%x in L%d\n", reg, val16, Adapter->pwr_state); \
        else \
            RTL_W16_F(Adapter, reg, val16); \
        } while (0)
    
#define PlatformEFIOWrite4Byte(Adapter,reg,val32)        \
        do { \
        if( Adapter->pwr_state==L2  || Adapter->pwr_state==L1) \
            printk("Err!!! w32:%x,%x in L%d\n", reg, (unsigned int)val32, Adapter->pwr_state); \
        else \
            RTL_W32_F(Adapter, reg, val32) ; \
        } while (0)

#define platform_efio_write_1byte(Adapter,reg,val8)	\
        do { \
        if( Adapter->pwr_state==L2  || Adapter->pwr_state==L1) \
            {   printk("Error!!! w8:%x,%x in L%d\n", reg, val8, Adapter->pwr_state);} \
        else \
            RTL_W8_F(Adapter, reg, val8); \
        } while (0)
    
#define platform_efio_write_2byte(Adapter,reg,val16)	\
        do { \
        if( Adapter->pwr_state==L2  || Adapter->pwr_state==L1) \
            printk("Err!!! w16:%x,%x in L%d\n", reg, val16, Adapter->pwr_state); \
        else \
            RTL_W16_F(Adapter, reg, val16); \
        } while (0)
    
#define platform_efio_write_4byte(Adapter,reg,val32)	\
        do { \
        if( Adapter->pwr_state==L2  || Adapter->pwr_state==L1) \
            printk("Err!!! w32:%x,%x in L%d\n", reg, (unsigned int)val32, Adapter->pwr_state); \
        else \
            RTL_W32_F(Adapter, reg, val32) ; \
        } while (0)


#else
#define HAL_RTL_R8(reg)		\
    (RTL_R8_F(Adapter, reg))

#define HAL_RTL_R16(reg)	\
    (RTL_R16_F(Adapter, reg))

#define HAL_RTL_R32(reg)	\
    (RTL_R32_F(Adapter, reg))

#define HAL_RTL_W8(reg, val8)	\
    do { \
        RTL_W8_F(Adapter, reg, val8); \
    } while (0)

#define HAL_RTL_W16(reg, val16)	\
    do { \
        RTL_W16_F(Adapter, reg, val16); \
    } while (0)

#define HAL_RTL_W32(reg, val32)	\
    do { \
        RTL_W32_F(Adapter, reg, val32) ; \
    } while (0)


#define platform_efio_read_1byte(Adapter,reg)        \
            (RTL_R8_F(Adapter, reg))
#define platform_efio_read_2byte(Adapter,reg)        \
            (RTL_R16_F(Adapter, reg))
#define platform_efio_read_4byte(Adapter,reg)        \
            (RTL_R32_F(Adapter, reg))   


#define platform_efio_write_1byte(Adapter,reg,val8)        \
    do { \
        RTL_W8_F(Adapter, reg, val8); \
    } while (0)            
#define platform_efio_write_2byte(Adapter,reg,val16)        \
    do { \
        RTL_W16_F(Adapter, reg, val16); \
    } while (0)
#define platform_efio_write_4byte(Adapter,reg,val32)        \
    do { \
        RTL_W32_F(Adapter, reg, val32) ; \
    } while (0)
#endif    

#endif

#ifdef CONFIG_USB_HCI
#define HAL_RTL_R8(reg)			usb_read8(Adapter, reg)
#define HAL_RTL_R16(reg)			usb_read16(Adapter, reg)
#define HAL_RTL_R32(reg)			usb_read32(Adapter, reg)

#define HAL_RTL_W8(reg, val8)		usb_write8(Adapter, reg, val8)
#define HAL_RTL_W16(reg, val16)	usb_write16(Adapter, reg, val16)
#define HAL_RTL_W32(reg, val32)	usb_write32(Adapter, reg, val32)
#define HAL_RTL_Wn(reg, len, val)	usb_writeN(Adapter, reg, len, val)

#define platform_efio_read_1byte(Adapter,reg)		usb_read8(Adapter, reg)
#define platform_efio_read_2byte(Adapter,reg)		usb_read16(Adapter, reg)
#define platform_efio_read_4byte(Adapter,reg)		usb_read32(Adapter, reg)

#define platform_efio_write_1byte(Adapter,reg,val8)	usb_write8(Adapter, reg, val8)
#define platform_efio_write_2byte(Adapter,reg,val16)	usb_write16(Adapter, reg, val16)
#define platform_efio_write_4byte(Adapter,reg,val32)	usb_write32(Adapter, reg, val32)
#endif // CONFIG_USB_HCI

#ifdef CONFIG_SDIO_HCI
#define HAL_RTL_R8(reg)			sdio_read8(Adapter, reg, NULL)
#define HAL_RTL_R16(reg)			sdio_read16(Adapter, reg, NULL)
#define HAL_RTL_R32(reg)			sdio_read32(Adapter, reg, NULL)

#define HAL_RTL_W8(reg, val8)		sdio_write8(Adapter, reg, val8)
#define HAL_RTL_W16(reg, val16)	sdio_write16(Adapter, reg, val16)
#define HAL_RTL_W32(reg, val32)	sdio_write32(Adapter, reg, val32)
#define HAL_RTL_Wn(reg, len, val)	sdio_writeN(Adapter, reg, len, val)

#define platform_efio_read_1byte(Adapter,reg)		sdio_read8(Adapter, reg, NULL)
#define platform_efio_read_2byte(Adapter,reg)		sdio_read16(Adapter, reg, NULL)
#define platform_efio_read_4byte(Adapter,reg)		sdio_read32(Adapter, reg, NULL)

#define platform_efio_write_1byte(Adapter,reg,val8)	sdio_write8(Adapter, reg, val8)
#define platform_efio_write_2byte(Adapter,reg,val16)	sdio_write16(Adapter, reg, val16)
#define platform_efio_write_4byte(Adapter,reg,val32)	sdio_write32(Adapter, reg, val32)
#endif // CONFIG_SDIO_HCI

#endif /* defined(CONFIG_RTL_TRIBAND_SUPPORT) */

//3 Mapping General Function
#define HALMalloc(Adapter, Size)        kmalloc(Size, GFP_ATOMIC)
#define HAL_free(x)                     kfree(x)
#define platform_zero_memory(Ptr, Size)   memset(Ptr, 0, Size)
#define HAL_delay_ms(t)                 delay_ms(t)
#define HAL_delay_us(t)                 delay_us(t)
#define HAL_memcpy(dst, src, cnt)       memcpy(dst, src, cnt)
#define HAL_memcmp(src1, src2, size)    memcmp(src1, src2, size)
#define HAL_memset(Ptr, Content, Size)  memset(Ptr, Content, Size)

//This is for LUNA SDK - Apollo to config 92er in slave CPU and shift mem 33M
#if defined(CONFIG_ARCH_LUNA_SLAVE) && defined(CONFIG_WLAN_HAL)
#define CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL CONFIG_RTL8686_DSP_MEM_BASE
#else
#define CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL 0x0
#endif

//LX MIPS MMU
//#define HAL_TO_NONCACHE_ADDR(addr)    (addr|0x20000000)	// Justin: This works only for MIPS CPU
#ifdef __MIPSEB__
#define HAL_TO_NONCACHE_ADDR(addr)      (KSEG1ADDR(addr))
#elif defined(NOT_RTK_BSP)
#define HAL_TO_NONCACHE_ADDR(addr)      (addr)
#else
#ifdef __MIPSEL__
#define HAL_TO_NONCACHE_ADDR(addr)      (addr|0x20000000)
#else // 8198F ARM
#define HAL_TO_NONCACHE_ADDR(addr)      (addr) 
#endif 
#endif

// TODO: Filen, replace HAL_VIRT_TO_BUS
#define HAL_VIRT_TO_BUS(ptr)                                    virt_to_phys(ptr)
#define HAL_VIRT_TO_BUS1(Adapter, ptr, size, direction)         get_physical_addr(Adapter, ptr, size, direction)
//direction
#define HAL_PCI_DMA_TODEVICE    PCI_DMA_TODEVICE
#define HAL_PCI_DMA_FROMDEVICE    PCI_DMA_FROMDEVICE

#ifdef CONFIG_NET_PCI
#define HAL_CACHE_SYNC_WBACK(Adapter, start, size, direction)   rtl_cache_sync_wback(Adapter, bus_to_virt(start), size, direction)
#else
#define HAL_CACHE_SYNC_WBACK(Adapter, start, size, direction)   rtl_cache_sync_wback(Adapter, start, size, direction)
#endif

#ifdef CONFIG_NET_PCI
#define HAL_IS_PCIBIOS_TYPE(Adapter)	(((Adapter->pshare->type >> TYPE_SHIFT) & TYPE_MASK) == TYPE_PCI_BIOS)
#endif

#define HAL_CIRC_CNT_RTK(head,tail,size)	((head>=tail)?(head-tail):(size-tail+head))
#define HAL_CIRC_SPACE_RTK(head,tail,size)  HAL_CIRC_CNT_RTK((tail),((head)+1),(size))


//3 Mapping AP Function
#if (CFG_HAL_SUPPORT_UNIVERSAL_REPEATER) || (CFG_HAL_SUPPORT_MBSSID)
//#define HAL_GET_ROOT_PRIV(Adapter)          GET_ROOT_PRIV(Adapter)
#define HAL_IS_ROOT_INTERFACE(Adapter)      IS_ROOT_INTERFACE(Adapter)
#define HAL_GET_ROOT(Adapter)               GET_ROOT(Adapter)								
#else
#define HAL_GET_ROOT(Adapter)               GET_ROOT(Adapter)
#endif

#if CFG_HAL_SUPPORT_UNIVERSAL_REPEATER
#define HAL_GET_VXD_PRIV(Adapter)           GET_VXD_PRIV(Adapter)
#if CFG_HAL_SUPPORT_MBSSID
#define HAL_IS_VXD_INTERFACE(Adapter)       IS_VXD_INTERFACE(Adapter)
#else
#define HAL_IS_VXD_INTERFACE(Adapter)       IS_VXD_INTERFACE(Adapter)
#endif
#endif // CFG_HAL_SUPPORT_UNIVERSAL_REPEATER

#if CFG_HAL_SUPPORT_MBSSID
#define HAL_IS_VAP_INTERFACE(Adapter)       IS_VAP_INTERFACE(Adapter)
#endif

#ifdef CFG_HAL_POWER_PERCENT_ADJUSTMENT
#define  HAL_PwrPercent2PwrLevel(percentage)                PwrPercent2PwrLevel(percentage)
#endif //CFG_HAL_POWER_PERCENT_ADJUSTMENT

#define HAL_ASSIGN_TX_POWER_OFFSET(offset, setting)         ASSIGN_TX_POWER_OFFSET(offset, setting)
#define HAL_POWER_RANGE_CHECK(val)                          POWER_RANGE_CHECK(val)
#define HAL_POWER_RANGE_CHECK_NEW(val)                      POWER_RANGE_CHECK_NEW(val)
#define HAL_EFUSE_POWER_RANGE_CHECK(val)                    (((val) > 0x3f)? 0x0 : ((val < 0) ? 0x0 : val))
#define HAL_COUNT_SIGN_OFFSET(base, offset)                 COUNT_SIGN_OFFSET(base, offset)
#define HAL_RTL_ABS(a,b)                                    RTL_ABS(a,b)    

//3 Mapping Protocol Function
#define HAL_IS_MCAST(da)                    IS_MCAST(da)
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
#define GetEthDAPtr(pbuf)					((unsigned char *)((unsigned int)(pbuf)))
#endif


//3  Mapping Linker Section
#define __HAL_MIPS16__
#define __HAL_FAST__
#define __HAL_MIDIUM__
#define __HAL_LOW__


//3 Mapping Critical Section Protection Method
#define HAL_SAVE_INT_AND_CLI(x)     SAVE_INT_AND_CLI(x)
#define HAL_RESTORE_INT(x)          RESTORE_INT(x)


//3 Mapping Endian Transformer

/*
 *	Call endian free function when
 *		1. Read/write packet content.
 *		2. Before write integer to IO.
 *		3. After read integer from IO.
*/


//#define HAL_cpu_to_le64
//#define HAL_le64_to_cpu
#define HAL_cpu_to_le32     cpu_to_le32
#define HAL_le32_to_cpu     le32_to_cpu
#define HAL_cpu_to_le16     cpu_to_le16
#define HAL_le16_to_cpu     le16_to_cpu
//#define HAL_cpu_to_be64
//#define HAL_be64_to_cpu
#define HAL_cpu_to_be32     cpu_to_be32
#define HAL_be32_to_cpu     be32_to_cpu
#define HAL_cpu_to_be16     cpu_to_be16
#define HAL_be16_to_cpu     be16_to_cpu

//
// Byte Swapping routine.
//
#define HAL_EF1Byte	
#define HAL_EF2Byte 	le16_to_cpu
#define HAL_EF4Byte     le32_to_cpu

//
// Read LE format data from memory
//
#define HAL_ReadEF1Byte(_ptr)		HAL_EF1Byte(*((u1Byte *)(_ptr)))
#define HAL_ReadEF2Byte(_ptr)		HAL_EF2Byte(*((u2Byte *)(_ptr)))
#define HAL_ReadEF4Byte(_ptr)		HAL_EF4Byte(*((u4Byte *)(_ptr)))

//
// Write LE data to memory
//
#define HAL_WriteEF1Byte(_ptr, _val)	(*((u1Byte *)(_ptr)))=HAL_EF1Byte(_val)
#define HAL_WriteEF2Byte(_ptr, _val)	(*((u2Byte *)(_ptr)))=HAL_EF2Byte(_val)
#define HAL_WriteEF4Byte(_ptr, _val)	(*((u4Byte *)(_ptr)))=HAL_EF4Byte(_val)

//3 Mapping OS API
#define HAL_OS_malloc(Adapter, size, flag, could_alloc_from_kerenl) rtl_dev_alloc_skb(Adapter, size, flag, could_alloc_from_kerenl);
#define PHAL_BUF                                                    struct sk_buff *
#define GET_BUF_DATA_PTR(ptr)                                       (ptr->data)


//3 Mapping Debug 
#if     CFG_HAL_DBG
#define HalDbgPrint    printk
#else
#define HalDbgPrint
#endif


//3 Mapping Variable
//each adapter
#define HAL_GET_MIB(Adapter)        (Adapter->pmib)
#define HAL_VAR_MANUAL_EDCA         (Adapter->pmib->dot11QosEntry.ManualEDCA)
#define HAL_VAR_MY_HWADDR           ((HAL_GET_MIB(Adapter))->dot11OperationEntry.hwaddr)
#define HAL_OPMODE                  ((HAL_GET_MIB(Adapter))->dot11OperationEntry.opmode)
#define HAL_P2PMODE                  ((HAL_GET_MIB(Adapter))->p2p_mib.p2p_type)

#define HAL_VAR_BCN_INTERVAL        (Adapter->pmib->dot11StationConfigEntry.dot11BeaconPeriod)
#define HAL_VAR_DTIM_PERIOD         (Adapter->pmib->dot11StationConfigEntry.dot11DTIMPeriod)
#define HAL_VAR_NETWORK_TYPE        (Adapter->pmib->dot11BssType.net_work_type)
#define HAL_VAR_AMSDURECVMAX        (Adapter->pmib->dot11nConfigEntry.dot11nAMSDURecvMax)

#define HAL_VAR_VAP_INIT_SEQ        (Adapter->vap_init_seq)
#define HAL_VAR_VAP_COUNT           (GET_ROOT(Adapter)->vap_count)
#define HAL_VAR_VAP_ID              (Adapter->vap_id)
#define HAL_VAR_TX_BEACON_LEN       (Adapter->tx_beacon_len)
#define HAL_VAR_TIM_OFFSET          (Adapter->timoffset)

// for data rate
#define HAL_VAR_TX_FORCE_RATE       (Adapter->pshare->rf_ft_var.txforce)

//Shared, only one
//#define HAL_VAR_RETRY_LIMIT_SHORT       (Adapter->pshare->RLShort)
//#define HAL_VAR_RETRY_LIMIT_LONG        (Adapter->pshare->RLLong)
#define HAL_VAR_RETRY_LIMIT        		(Adapter->pshare->RL_setting)
#define HAL_VAR_RETRY_LIMIT_SHORT_MIB	(Adapter->pmib->dot11OperationEntry.dot11ShortRetryLimit)
#define HAL_VAR_RETRY_LIMIT_LONG_MIB	(Adapter->pmib->dot11OperationEntry.dot11LongRetryLimit)


#define HAL_VAR_IS_40M_BW               (Adapter->pshare->is_40m_bw)
#define HAL_VAR_IS_40M_BW_BAK           (Adapter->pshare->is_40m_bw_bak)
#define HAL_VAR_OFFSET_2ND_CHANNEL      (Adapter->pshare->offset_2nd_chan)
#define HAL_VAR_MP_SPECIFIC             (Adapter->pshare->rf_ft_var.mp_specific)
#define HAL_VAR_pre_channel             (Adapter->pshare->pre_channel)
#define HAL_VAR_ENABLE_MACID_SLEEP      (Adapter->pshare->rf_ft_var.enable_macid_sleep)
#define HAL_VAR_TXSC_20                 (Adapter->pshare->txsc_20)
#define HAL_VAR_TXSC_40                 (Adapter->pshare->txsc_40)
#define HAL_VAR_TXSC_80                 (Adapter->pshare->txsc_80)
#define HAL_VAR_REG_RRSR_2			    (Adapter->pshare->Reg_RRSR_2)
#define HAL_VAR_REG_81B			        (Adapter->pshare->Reg_81b)
#define HAL_VAR_CURR_BAND               (Adapter->pshare->curr_band)
#define HAL_VAR_CURRENTCHANNELBW        (Adapter->pshare->CurrentChannelBW )
#define HAL_VAR_USE_FRQ_2_3G            (Adapter->pshare->rf_ft_var.use_frq_2_3G)

// for CFG_HAL_CONCURRENT_MODE
#define HAL_VAR_WLANDEV_IDX            	(Adapter->pshare->wlandev_idx)
#ifdef CONFIG_WLAN_HAL_8881A
#define HAL_VAR_INTERLPA_8881A         	(Adapter->pshare->rf_ft_var.use_intpa8881A)
#ifdef CONFIG_8881A_HP
#define HAL_VAR_HP_8881A         	    (Adapter->pshare->rf_ft_var.hp_8881a)
#endif
#endif
#define HAL_VAR_PA_TYPE                 (Adapter->pmib->dot11RFEntry.pa_type)
#define HAL_TX2PATH                     (Adapter->pmib->dot11RFEntry.tx2path)
#ifdef BT_COEXIST
#define HAL_BT_DUMP                     (Adapter->pshare->rf_ft_var.bt_dump)
#endif
#define HAL_RFE_TYPE                    (Adapter->pmib->dot11RFEntry.rfe_type)

// phy config
#define HAL_VAR_dot11nUse40M                (Adapter->pmib->dot11nConfigEntry.dot11nUse40M)
#define HAL_VAR_pwrlevelCCK_A(chIdx)        (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrlevelCCK_A)+chIdx))
#define HAL_VAR_pwrlevelCCK_B(chIdx)        (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrlevelCCK_B)+chIdx))
#define HAL_VAR_pwrlevelCCK_C(chIdx)        (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrlevelCCK_C)+chIdx))
#define HAL_VAR_pwrlevelCCK_D(chIdx)        (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrlevelCCK_D)+chIdx))
#define HAL_VAR_pwrlevelHT40_1S_A(chIdx)    (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrlevelHT40_1S_A)+chIdx))
#define HAL_VAR_pwrlevelHT40_1S_B(chIdx)    (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrlevelHT40_1S_B)+chIdx))
#define HAL_VAR_pwrlevelHT40_1S_C(chIdx)    (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrlevelHT40_1S_C)+chIdx))
#define HAL_VAR_pwrlevelHT40_1S_D(chIdx)    (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrlevelHT40_1S_D)+chIdx))
#define HAL_VAR_pwrdiffHT40_2S(chIdx)       (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiffHT40_2S)+chIdx))
#define HAL_VAR_pwrdiffHT20(chIdx)          (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiffHT20)+chIdx))
#define HAL_VAR_pwrdiffOFDM(chIdx)          (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiffOFDM)+chIdx))
#define HAL_VAR_pwrlevel5GHT40_1S_A(chIdx)  (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrlevel5GHT40_1S_A)+chIdx))
#define HAL_VAR_pwrlevel5GHT40_1S_B(chIdx)  (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrlevel5GHT40_1S_B)+chIdx))
#define HAL_VAR_pwrlevel5GHT40_1S_C(chIdx)  (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrlevel5GHT40_1S_C)+chIdx))
#define HAL_VAR_pwrlevel5GHT40_1S_D(chIdx)  (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrlevel5GHT40_1S_D)+chIdx))
#define HAL_VAR_pwrdiff5GHT40_2S(chIdx)     (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff5GHT40_2S)+chIdx))
#define HAL_VAR_pwrdiff5GHT20(chIdx)        (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff5GHT20)+chIdx))
#define HAL_VAR_pwrdiff5GOFDM(chIdx)        (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff5GOFDM)+chIdx))
#define HAL_VAR_pwrdiff_20BW1S_OFDM1T_A(chIdx)  	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_20BW1S_OFDM1T_A)+chIdx))	
#define HAL_VAR_pwrdiff_40BW2S_20BW2S_A(chIdx)  	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_40BW2S_20BW2S_A)+chIdx))	
#define HAL_VAR_pwrdiff_OFDM2T_CCK2T_A(chIdx)       (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_OFDM2T_CCK2T_A)+chIdx))
#define HAL_VAR_pwrdiff_40BW3S_20BW3S_A(chIdx)  	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_40BW3S_20BW3S_A)+chIdx))	
#define HAL_VAR_pwrdiff_4OFDM3T_CCK3T_A(chIdx)  	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_OFDM3T_CCK3T_A)+chIdx))
#define HAL_VAR_pwrdiff_40BW4S_20BW4S_A(chIdx)  	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_40BW4S_20BW4S_A)+chIdx))	
#define HAL_VAR_pwrdiff_OFDM4T_CCK4T_A(chIdx)       (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_OFDM4T_CCK4T_A)+chIdx))
#define HAL_VAR_pwrdiff_5G_20BW1S_OFDM1T_A(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_20BW1S_OFDM1T_A)+chIdx))
#define HAL_VAR_pwrdiff_5G_40BW2S_20BW2S_A(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_40BW2S_20BW2S_A)+chIdx))
#define HAL_VAR_pwrdiff_5G_40BW3S_20BW3S_A(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_40BW3S_20BW3S_A)+chIdx))
#define HAL_VAR_pwrdiff_5G_40BW4S_20BW4S_A(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_40BW4S_20BW4S_A)+chIdx))
#define HAL_VAR_pwrdiff_5G_RSVD_OFDM4T_A(chIdx)	    (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_RSVD_OFDM4T_A)+chIdx))	
#define HAL_VAR_pwrdiff_5G_80BW1S_160BW1S_A(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_80BW1S_160BW1S_A)+chIdx))	
#define HAL_VAR_pwrdiff_5G_80BW2S_160BW2S_A(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_80BW2S_160BW2S_A)+chIdx))	
#define HAL_VAR_pwrdiff_5G_80BW3S_160BW3S_A(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_80BW3S_160BW3S_A)+chIdx))	
#define HAL_VAR_pwrdiff_5G_80BW4S_160BW4S_A(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_80BW4S_160BW4S_A)+chIdx))	
#define HAL_VAR_pwrdiff_20BW1S_OFDM1T_B(chIdx)  	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_20BW1S_OFDM1T_B)+chIdx))	
#define HAL_VAR_pwrdiff_40BW2S_20BW2S_B(chIdx)  	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_40BW2S_20BW2S_B)+chIdx))	
#define HAL_VAR_pwrdiff_OFDM2T_CCK2T_B(chIdx)       (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_OFDM2T_CCK2T_B)+chIdx))
#define HAL_VAR_pwrdiff_40BW3S_20BW3S_B(chIdx)  	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_40BW3S_20BW3S_B)+chIdx))	
#define HAL_VAR_pwrdiff_4OFDM3T_CCK3T_B(chIdx)  	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_OFDM3T_CCK3T_B)+chIdx))
#define HAL_VAR_pwrdiff_40BW4S_20BW4S_B(chIdx)  	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_40BW4S_20BW4S_B)+chIdx))	
#define HAL_VAR_pwrdiff_OFDM4T_CCK4T_B(chIdx)       (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_OFDM4T_CCK4T_B)+chIdx))
#define HAL_VAR_pwrdiff_5G_20BW1S_OFDM1T_B(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_20BW1S_OFDM1T_B)+chIdx))
#define HAL_VAR_pwrdiff_5G_40BW2S_20BW2S_B(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_40BW2S_20BW2S_B)+chIdx))
#define HAL_VAR_pwrdiff_5G_40BW3S_20BW3S_B(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_40BW3S_20BW3S_B)+chIdx))
#define HAL_VAR_pwrdiff_5G_40BW4S_20BW4S_B(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_40BW4S_20BW4S_B)+chIdx))
#define HAL_VAR_pwrdiff_5G_RSVD_OFDM4T_B(chIdx)	    (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_RSVD_OFDM4T_B)+chIdx))	
#define HAL_VAR_pwrdiff_5G_80BW1S_160BW1S_B(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_80BW1S_160BW1S_B)+chIdx))	
#define HAL_VAR_pwrdiff_5G_80BW2S_160BW2S_B(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_80BW2S_160BW2S_B)+chIdx))	
#define HAL_VAR_pwrdiff_5G_80BW3S_160BW3S_B(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_80BW3S_160BW3S_B)+chIdx))	
#define HAL_VAR_pwrdiff_5G_80BW4S_160BW4S_B(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_80BW4S_160BW4S_B)+chIdx))	
#define HAL_VAR_pwrdiff_20BW1S_OFDM1T_C(chIdx)  	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_20BW1S_OFDM1T_C)+chIdx))	
#define HAL_VAR_pwrdiff_40BW2S_20BW2S_C(chIdx)  	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_40BW2S_20BW2S_C)+chIdx))	
#define HAL_VAR_pwrdiff_OFDM2T_CCK2T_C(chIdx)       (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_OFDM2T_CCK2T_C)+chIdx))
#define HAL_VAR_pwrdiff_40BW3S_20BW3S_C(chIdx)  	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_40BW3S_20BW3S_C)+chIdx))	
#define HAL_VAR_pwrdiff_4OFDM3T_CCK3T_C(chIdx)  	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_OFDM3T_CCK3T_C)+chIdx))
#define HAL_VAR_pwrdiff_40BW4S_20BW4S_C(chIdx)  	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_40BW4S_20BW4S_C)+chIdx))	
#define HAL_VAR_pwrdiff_OFDM4T_CCK4T_C(chIdx)       (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_OFDM4T_CCK4T_C)+chIdx))
#define HAL_VAR_pwrdiff_5G_20BW1S_OFDM1T_C(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_20BW1S_OFDM1T_C)+chIdx))
#define HAL_VAR_pwrdiff_5G_40BW2S_20BW2S_C(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_40BW2S_20BW2S_C)+chIdx))
#define HAL_VAR_pwrdiff_5G_40BW3S_20BW3S_C(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_40BW3S_20BW3S_C)+chIdx))
#define HAL_VAR_pwrdiff_5G_40BW4S_20BW4S_C(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_40BW4S_20BW4S_C)+chIdx))
#define HAL_VAR_pwrdiff_5G_RSVD_OFDM4T_C(chIdx)	    (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_RSVD_OFDM4T_C)+chIdx))	
#define HAL_VAR_pwrdiff_5G_80BW1S_160BW1S_C(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_80BW1S_160BW1S_C)+chIdx))	
#define HAL_VAR_pwrdiff_5G_80BW2S_160BW2S_C(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_80BW2S_160BW2S_C)+chIdx))	
#define HAL_VAR_pwrdiff_5G_80BW3S_160BW3S_C(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_80BW3S_160BW3S_C)+chIdx))	
#define HAL_VAR_pwrdiff_5G_80BW4S_160BW4S_C(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_80BW4S_160BW4S_C)+chIdx))	
#define HAL_VAR_pwrdiff_20BW1S_OFDM1T_D(chIdx)  	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_20BW1S_OFDM1T_D)+chIdx))	
#define HAL_VAR_pwrdiff_40BW2S_20BW2S_D(chIdx)  	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_40BW2S_20BW2S_D)+chIdx))	
#define HAL_VAR_pwrdiff_OFDM2T_CCK2T_D(chIdx)       (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_OFDM2T_CCK2T_D)+chIdx))
#define HAL_VAR_pwrdiff_40BW3S_20BW3S_D(chIdx)  	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_40BW3S_20BW3S_D)+chIdx))	
#define HAL_VAR_pwrdiff_4OFDM3T_CCK3T_D(chIdx)  	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_OFDM3T_CCK3T_D)+chIdx))
#define HAL_VAR_pwrdiff_40BW4S_20BW4S_D(chIdx)  	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_40BW4S_20BW4S_D)+chIdx))	
#define HAL_VAR_pwrdiff_OFDM4T_CCK4T_D(chIdx)       (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_OFDM4T_CCK4T_D)+chIdx))
#define HAL_VAR_pwrdiff_5G_20BW1S_OFDM1T_D(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_20BW1S_OFDM1T_D)+chIdx))
#define HAL_VAR_pwrdiff_5G_40BW2S_20BW2S_D(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_40BW2S_20BW2S_D)+chIdx))
#define HAL_VAR_pwrdiff_5G_40BW3S_20BW3S_D(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_40BW3S_20BW3S_D)+chIdx))
#define HAL_VAR_pwrdiff_5G_40BW4S_20BW4S_D(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_40BW4S_20BW4S_D)+chIdx))
#define HAL_VAR_pwrdiff_5G_RSVD_OFDM4T_D(chIdx)	    (*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_RSVD_OFDM4T_D)+chIdx))	
#define HAL_VAR_pwrdiff_5G_80BW1S_160BW1S_D(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_80BW1S_160BW1S_D)+chIdx))	
#define HAL_VAR_pwrdiff_5G_80BW2S_160BW2S_D(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_80BW2S_160BW2S_D)+chIdx))	
#define HAL_VAR_pwrdiff_5G_80BW3S_160BW3S_D(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_80BW3S_160BW3S_D)+chIdx))	
#define HAL_VAR_pwrdiff_5G_80BW4S_160BW4S_D(chIdx)	(*(((u1Byte *)Adapter->pmib->dot11RFEntry.pwrdiff_5G_80BW4S_160BW4S_D)+chIdx))	
#if (IS_RTL8814A_SERIES || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES || IS_RTL8198F_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES|| IS_RTL8197G_SERIES || IS_RTL8814B_SERIES)
#define HAL_VAR_CurrentTxAgcCCK(path,chIdx)			(*(((u1Byte *)Adapter->pshare->phw->CurrentTxAgcCCK)+path*4+chIdx))	
#define HAL_VAR_CurrentTxAgcOFDM(path,rate)			(*(((u1Byte *)Adapter->pshare->phw->CurrentTxAgcOFDM)+path*8+rate))	
#define HAL_VAR_CurrentTxAgcMCS(path,rate)			(*(((u1Byte *)Adapter->pshare->phw->CurrentTxAgcMCS)+path*32+rate))	
#define HAL_VAR_CurrentTxAgcVHT(path,rate)			(*(((u1Byte *)Adapter->pshare->phw->CurrentTxAgcVHT)+path*40+rate))	
#endif

// 11n OFDM setting
#define HAL_VAR_power_percent               (Adapter->pmib->dot11RFEntry.power_percent)
#define HAL_VAR_use_ext_pa                  (Adapter->pshare->rf_ft_var.use_ext_pa)
#define HAL_VAR_CurrentChannelBW            (Adapter->pshare->CurrentChannelBW)
#define HAL_VAR_txPowerPlus_ofdm_6          (Adapter->pshare->rf_ft_var.txPowerPlus_ofdm_6)
#define HAL_VAR_txPowerPlus_ofdm_9          (Adapter->pshare->rf_ft_var.txPowerPlus_ofdm_9)
#define HAL_VAR_txPowerPlus_ofdm_12         (Adapter->pshare->rf_ft_var.txPowerPlus_ofdm_12)
#define HAL_VAR_txPowerPlus_ofdm_18         (Adapter->pshare->rf_ft_var.txPowerPlus_ofdm_18)
#define HAL_VAR_txPowerPlus_ofdm_24         (Adapter->pshare->rf_ft_var.txPowerPlus_ofdm_24)
#define HAL_VAR_txPowerPlus_ofdm_36         (Adapter->pshare->rf_ft_var.txPowerPlus_ofdm_36)
#define HAL_VAR_txPowerPlus_ofdm_48         (Adapter->pshare->rf_ft_var.txPowerPlus_ofdm_48)
#define HAL_VAR_txPowerPlus_ofdm_54         (Adapter->pshare->rf_ft_var.txPowerPlus_ofdm_54)
#define HAL_VAR_txPowerPlus_mcs_0           (Adapter->pshare->rf_ft_var.txPowerPlus_mcs_0)
#define HAL_VAR_txPowerPlus_mcs_1           (Adapter->pshare->rf_ft_var.txPowerPlus_mcs_1)
#define HAL_VAR_txPowerPlus_mcs_2           (Adapter->pshare->rf_ft_var.txPowerPlus_mcs_2)
#define HAL_VAR_txPowerPlus_mcs_3           (Adapter->pshare->rf_ft_var.txPowerPlus_mcs_3)
#define HAL_VAR_txPowerPlus_mcs_4           (Adapter->pshare->rf_ft_var.txPowerPlus_mcs_4)
#define HAL_VAR_txPowerPlus_mcs_5           (Adapter->pshare->rf_ft_var.txPowerPlus_mcs_5)
#define HAL_VAR_txPowerPlus_mcs_6           (Adapter->pshare->rf_ft_var.txPowerPlus_mcs_6)
#define HAL_VAR_txPowerPlus_mcs_7           (Adapter->pshare->rf_ft_var.txPowerPlus_mcs_7)
#define HAL_VAR_OFDMTxAgcOffset_A(idx)      (*(((u1Byte *)Adapter->pshare->phw->OFDMTxAgcOffset_A)+idx))
#define HAL_VAR_OFDMTxAgcOffset_B(idx)      (*(((u1Byte *)Adapter->pshare->phw->OFDMTxAgcOffset_B)+idx))
#define HAL_VAR_MCSTxAgcOffset_A(idx)       (*(((u1Byte *)Adapter->pshare->phw->MCSTxAgcOffset_A)+idx))
#define HAL_VAR_MCSTxAgcOffset_B(idx)       (*(((u1Byte *)Adapter->pshare->phw->MCSTxAgcOffset_B)+idx))

// 11n CCK setting 
#define HAL_cck_pwr_max                     (Adapter->pshare->rf_ft_var.cck_pwr_max)
#define HAL_txPowerPlus_cck_1               (Adapter->pshare->rf_ft_var.txPowerPlus_cck_1)
#define HAL_txPowerPlus_cck_2               (Adapter->pshare->rf_ft_var.txPowerPlus_cck_2)
#define HAL_txPowerPlus_cck_5               (Adapter->pshare->rf_ft_var.txPowerPlus_cck_5)
#define HAL_txPowerPlus_cck_11              (Adapter->pshare->rf_ft_var.txPowerPlus_cck_11)
#define HAL_CCKTxAgc_A(idx)                 (*(((u1Byte *)Adapter->pshare->phw->CCKTxAgc_A)+idx))
#define HAL_CCKTxAgc_B(idx)                 (*(((u1Byte *)Adapter->pshare->phw->CCKTxAgc_B)+idx))

// For Power Tracking setting
#define HAL_VAR_DOT11CHANNEL                (Adapter->pmib->dot11RFEntry.dot11channel)
#define HAL_VAR_POWER_TRACKING_ON_88XX      (Adapter->pshare->Power_tracking_on_88XX)
#define HAL_VAR_THERMALVALUE                (Adapter->pshare->thermal_value)
#define HAL_VAR_THER                        (Adapter->pmib->dot11RFEntry.ther)
#define HAL_VAR_THERMALVALUE_AVG_88XX(idx)  (*(((u1Byte *)Adapter->pshare->thermal_value_avg_88xx)+idx))
#define HAL_VAR_THERMALVALUE_AVG_INDEX_88XX (Adapter->pshare->thermal_value_avg_index_88xx)
#define HAL_VAR_PHYBANDSELECT               (Adapter->pmib->dot11RFEntry.phyBandSelect)
#define HAL_VAR_OFDM_BASE_INDEX(idx)        (*(((u1Byte *)Adapter->pshare->OFDM_index0)+idx))
#define HAL_VAR_OFDM_INDEX(idx)             (*(((u1Byte *)Adapter->pshare->OFDM_index)+idx))
#define HAL_VAR_CCK_BASE_INDEX              (Adapter->pshare->CCK_index0)
#define HAL_VAR_CCK_INDEX                   (Adapter->pshare->CCK_index)
#define HAL_VAR_TXPWR_TRACKING_5GL(x,y)     (*(*((Adapter->pshare->txpwr_tracking_5GL)+(x))+(y)))
#define HAL_VAR_TXPWR_TRACKING_5GM(x,y)     (*(*((Adapter->pshare->txpwr_tracking_5GM)+(x))+(y)))
#define HAL_VAR_TXPWR_TRACKING_5GH(x,y)     (*(*((Adapter->pshare->txpwr_tracking_5GH)+(x))+(y)))
#define HAL_VAR_TXPWR_TRACKING_2G_CCK(x,y)  (*(*((Adapter->pshare->txpwr_tracking_2G_CCK)+(x))+(y)))
#define HAL_VAR_TXPWR_TRACKING_2G_OFDM(x,y) (*(*((Adapter->pshare->txpwr_tracking_2G_OFDM)+(x))+(y)))
#define HAL_VAR_PWR_TRACK_FILE              (Adapter->pshare->rf_ft_var.pwr_track_file)
#define HAL_VAR_MP_SPECIFIC                 (Adapter->pshare->rf_ft_var.mp_specific)
#define HAL_VAR_WORKING_CHANNEL             (Adapter->pshare->working_channel)
#define HAL_VAR_MP_TXPWR_TRACKING           (Adapter->pshare->mp_txpwr_tracking)


#if     IS_RTL88XX_GENERATION
HAL_IMEM
u4Byte
MappingTxQueue88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          TxQNum      //enum _TX_QUEUE_
);

HAL_IMEM
u4Byte  
MappingTxQueue88XX_V1(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          TxQNum      //enum _TX_QUEUE_
);

HAL_IMEM
u4Byte  
MappingTxQueue88XX_V2(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          TxQNum      //enum _TX_QUEUE_
);


void
MappingVariable88XX(
    IN  HAL_PADAPTER    Adapter
);

#endif  //IS_RTL88XX_GENERATION


#if (HALMAC_8192E_SUPPORT || HALMAC_8192F_SUPPORT || HALMAC_8881A_SUPPORT)
#define BIT_SHIFT_TXPKTBUF                  23
#define DBGBUF_TXPKTBUF                     0x69

#define BIT_SHIFT_TXRPTBUF                  20
#define DBGBUF_TXRPTBUF                     0x7F

#define BIT_SHIFT_RXPKTBUF                  16
#define DBGBUF_RXPKTBUF                     0xa5
#endif


#endif  //#ifndef __HALCFG_H__


