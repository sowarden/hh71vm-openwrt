/*
 *  RX handle routines
 *
 *  Copyright (c) 2017 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _8192CD_RX_C_

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <net/ip.h>
#include <linux/ipv6.h>
#include <linux/icmpv6.h>


#elif defined(__ECOS)
#include <cyg/io/eth/rltk/819x/wrapper/sys_support.h>
#include <cyg/io/eth/rltk/819x/wrapper/skbuff.h>
#include <cyg/io/eth/rltk/819x/wrapper/timer.h>
#include <cyg/io/eth/rltk/819x/wrapper/wrapper.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#endif

#include "./8192cd_cfg.h"
#include "./8192cd.h"
#include "./8192cd_hw.h"
#include "./8192cd_headers.h"
#include "./8192cd_rx.h"
#include "./8192cd_debug.h"

#ifdef __LINUX_2_6__
#ifdef CONFIG_RTL8672
#if !defined(CONFIG_OPENWRT_SDK)
#include "./romeperf.h"
#else
#include "./rtl_types.h"
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
#undef __MIPS16
#define __MIPS16
#endif
#else
#if !defined(NOT_RTK_BSP)
#ifdef CONFIG_OPENWRT_SDK
#include "./rtl_types.h" //mark_wrt
#else
#if defined(CONFIG_RTL_8198F) || defined(CONFIG_RTL_8197G)
#include "./rtl_types.h"  //Charlie
#else
#include <net/rtl/rtl_types.h>
#endif
#undef __MIPS16
#if CONFIG_RTL_DISABLE_WLAN_MIPS16
#define __MIPS16			
#else
#ifdef __ECOS
#if defined(RTLPKG_DEVS_ETH_RLTK_819X_USE_MIPS16) || (!defined(CONFIG_RTL_8198C) && !defined(CONFIG_RTL_8197F) & !defined(CONFIG_RTL_8197G) )
#define __MIPS16			__attribute__ ((mips16))
#else
#define __MIPS16
#endif
#else
#if defined(CONFIG_WIRELESS_LAN_MODULE) || defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F) || defined(CONFIG_RTL_8197G) || (defined(CONFIG_RTL8672) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0)))
#define __MIPS16
#else
#define __MIPS16			__attribute__ ((mips16))
#endif
#endif
#endif
#endif
#endif
#endif
#endif

#if !defined(__ECOS) && !defined(CPTCFG_CFG80211_MODULE)
#include "HalPrecomp.h"
#else
#include "../HalPrecomp.h"
#endif


#ifdef _BROADLIGHT_FASTPATH_
extern int (*send_packet_to_upper_layer)(struct sk_buff *skb);
#endif

#ifdef PREVENT_ARP_SPOOFING
#include <linux/inetdevice.h>
#include <linux/if_arp.h>
static int check_arp_spoofing(struct rtl8192cd_priv *priv, struct sk_buff *pskb);
#endif


#if defined(CONFIG_RTL_WAPI_SUPPORT)
#include "wapiCrypto.h"
#endif
#if defined(CONFIG_RTL_FASTBRIDGE)
#include <net/rtl/features/fast_bridge.h>
#endif

#ifdef CONFIG_RTL867X_VLAN_MAPPING
#include "../../re_vlan.h"
#endif
#if defined (CONFIG_RTL_VLAN_8021Q) || defined (CONFIG_PUMA_VLAN_8021Q)
#include <linux/if_vlan.h>
#endif

#ifdef PERF_DUMP
#include "romeperf.h"
#endif

#ifdef PERF_DUMP_1074K
#include "m1074kctrl.c"
#endif

#ifdef __OSK__
#include "ifport.h"
#endif

#include "./core/8192cd_core_rx.h"

#ifdef __ECOS
#define uint32 unsigned int
#define uint16 unsigned short
#define int16 short
#endif

#ifdef BR_SHORTCUT
#ifdef WDS
__DRAM_IN_865X unsigned char cached_wds_mac[MACADDRLEN];
__DRAM_IN_865X struct net_device *cached_wds_dev = NULL;
#endif
#ifdef CONFIG_RTK_MESH
__DRAM_IN_865X unsigned char cached_mesh_mac[2][MACADDRLEN];
__DRAM_IN_865X struct net_device *cached_mesh_dev[2] = {NULL,NULL};
#endif
#ifdef CLIENT_MODE
__DRAM_IN_865X unsigned char cached_sta_mac[MAX_REPEATER_SC_NUM][MACADDRLEN];
__DRAM_IN_865X struct net_device *cached_sta_dev[MAX_REPEATER_SC_NUM] = {NULL,NULL};
#endif

#ifdef RTL_CACHED_BR_STA
//__DRAM_IN_865X unsigned char cached_br_sta_mac[MACADDRLEN];
//__DRAM_IN_865X struct net_device *cached_br_sta_dev = NULL;
__DRAM_IN_865X struct brsc_cache_t brsc_cache_arr[MAX_BRSC_NUM];
#endif

/* for 8198F:
	wlan driver is static link, NE is a module,
	So define the following variable in wlan driver,
	NE will refer to them.
	(NE will also refer to get_shortcut_dev())
	CONFIG_RTL_819X_SWCORE is not defined in 8198F.
 */
#ifdef CONFIG_ARCH_RTL8198F
	#if defined(CONFIG_RTL8198F_WLAN_MODULE)
	extern unsigned char cached_eth_addr[MACADDRLEN];
	extern unsigned char cached_eth_addr2[MACADDRLEN];
	extern unsigned char cached_eth_addr3[MACADDRLEN];
	extern unsigned char cached_eth_addr4[MACADDRLEN];
	extern struct net_device *cached_dev;
	extern struct net_device *cached_dev2;
	extern struct net_device *cached_dev3;
	extern struct net_device *cached_dev4;	
	#else
	unsigned char cached_eth_addr[MACADDRLEN];
	unsigned char cached_eth_addr2[MACADDRLEN];
	unsigned char cached_eth_addr3[MACADDRLEN];
	unsigned char cached_eth_addr4[MACADDRLEN];

	struct net_device *cached_dev = NULL;
	struct net_device *cached_dev2 = NULL;
	struct net_device *cached_dev3 = NULL;
	struct net_device *cached_dev4 = NULL;

	EXPORT_SYMBOL(cached_eth_addr);
	EXPORT_SYMBOL(cached_eth_addr2);
	EXPORT_SYMBOL(cached_eth_addr3);
	EXPORT_SYMBOL(cached_eth_addr4);

	EXPORT_SYMBOL(cached_dev);
	EXPORT_SYMBOL(cached_dev2);
	EXPORT_SYMBOL(cached_dev3);
	EXPORT_SYMBOL(cached_dev4);
	EXPORT_SYMBOL(get_shortcut_dev);
#endif
#endif

#endif // BR_SHORTCUT

//for 8671 IGMP snooping
#if defined(__OSK__) && defined(CONFIG_RTL8672)
extern int wlan_igmp_tag;
extern int enable_IGMP_SNP;
extern void check_IGMP_snoop_rx(CCB *ccb, int tag);
#ifdef CONFIG_RTL_92D_DMDP
extern struct port_map wlanDev[(RTL8192CD_NUM_VWLAN+2)*2];
#else
extern struct port_map wlanDev[RTL8192CD_NUM_VWLAN+2];		// Root(1)+vxd(1)+VAPs(4)
#endif

extern int enable_port_mapping;
#ifdef INET6
extern int enable_MLD_SNP;
extern void check_mld_snoop_rx(CCB * ccb, int tag);
#if defined(CONFIG_RTL8672)
extern unsigned int AvailableMemSize;  // 20100818 defined in oskmem.c,  bad declaration, but still here for bad include on OSK.
#endif
#else
#if defined(CONFIG_RTL8672)
extern unsigned int AvailableMemSize;  // 20100818 defined in oskmem.c,  bad declaration, but still here for bad include on OSK.
#endif
#endif

#elif defined(CONFIG_RTL8672)
#define wlan_igmp_tag 0x1f
extern int enable_IGMP_SNP;
#ifdef CONFIG_EXT_SWITCH
extern void check_IGMP_snoop_rx(struct sk_buff *skb, int tag);
#endif

#endif //CONFIG_RTL8672

#if 0/*def CONFIG_RTL_STP*/
unsigned char STPmac[6] = { 1, 0x80, 0xc2, 0,0,0};
static struct net_device* wlan_pseudo_dev;
#define WLAN_INTERFACE_NAME			"wlan0"
#endif

#if defined(__LINUX_2_6__) && defined(CONFIG_RTK_VLAN_SUPPORT)
extern int rtk_vlan_support_enable;
#endif
#ifdef CONFIG_RTL_VLAN_8021Q
extern int linux_vlan_enable;
extern linux_vlan_ctl_t *vlan_ctl_p;
#endif

#if defined(CONFIG_RTL_EAP_RELAY) || defined(CONFIG_RTK_INBAND_HOST_HACK)
extern unsigned char inband_Hostmac[]; //it's from br.c
#endif

#if defined(CONFIG_RTL_819X_ECOS)
extern unsigned char freebsd_Hostmac[];
#endif

/* ======================== RX procedure declarations ======================== */
static void rtl8192cd_rx_ctrlframe(struct rtl8192cd_priv *priv,
				struct list_head *list, struct rx_frinfo *inputPfrinfo);
static int auth_filter(struct rtl8192cd_priv *priv, struct stat_info *pstat,
				struct rx_frinfo *pfrinfo);
static void ctrl_handler(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo);



#ifdef __OSK__
extern unsigned int toswcore_cnt;
extern unsigned int to_cnt;
#endif

#if !defined(USE_OUT_SRC) || defined(_OUTSRC_COEXIST)
static unsigned char QueryRxPwrPercentage(signed char AntPower)
{
	if ((AntPower <= -100) || (AntPower >= 20))
		return	0;
	else if (AntPower >= 0)
		return	100;
	else
		return	(100+AntPower);
}
#endif

int SignalScaleMapping(int CurrSig)
{
	int RetSig;

	// Step 1. Scale mapping.
	if(CurrSig >= 61 && CurrSig <= 100)
	{
		RetSig = 90 + ((CurrSig - 60) / 4);
	}
	else if(CurrSig >= 41 && CurrSig <= 60)
	{
		RetSig = 78 + ((CurrSig - 40) / 2);
	}
	else if(CurrSig >= 31 && CurrSig <= 40)
	{
		RetSig = 66 + (CurrSig - 30);
	}
	else if(CurrSig >= 21 && CurrSig <= 30)
	{
		RetSig = 54 + (CurrSig - 20);
	}
	else if(CurrSig >= 5 && CurrSig <= 20)
	{
		RetSig = 42 + (((CurrSig - 5) * 2) / 3);
	}
	else if(CurrSig == 4)
	{
		RetSig = 36;
	}
	else if(CurrSig == 3)
	{
		RetSig = 27;
	}
	else if(CurrSig == 2)
	{
		RetSig = 18;
	}
	else if(CurrSig == 1)
	{
		RetSig = 9;
	}
	else
	{
		RetSig = CurrSig;
	}

	return RetSig;
}

#if !defined(USE_OUT_SRC) || defined(_OUTSRC_COEXIST)
static unsigned char EVMdbToPercentage(signed char Value)
{
	signed char ret_val;

	ret_val = Value;

	if (ret_val >= 0)
		ret_val = 0;
	if (ret_val <= -33)
		ret_val = -33;
	ret_val = 0 - ret_val;
	ret_val*=3;
	if (ret_val == 99)
		ret_val = 100;
	return(ret_val);
}
#endif


#ifdef MP_SWITCH_LNA

#define ss_threshold_H 0x28
#define ss_threshold_L 0x17

static __inline__ void dynamic_switch_lna(struct rtl8192cd_priv *priv)
{

	unsigned int tmp_b30 = phy_query_bb_reg(priv, 0xb30, bMaskDWord);


	unsigned int tmp_dd0 = phy_query_bb_reg(priv, 0xdd0, bMaskDWord);
	unsigned int tmp_dd0_a = (tmp_dd0 & 0x3f);
	unsigned int tmp_dd0_b = ((tmp_dd0 & 0x3f00) >> 8);

	//======= PATH  A ============

	if((tmp_dd0_a >= ss_threshold_H) && (!(tmp_b30 & BIT(21))))
	{
		if(priv->pshare->rx_packet_ss_a >= 10)
			priv->pshare->rx_packet_ss_a = 0;
		
		priv->pshare->rx_packet_ss_a = (priv->pshare->rx_packet_ss_a+1); 
		
		if(priv->pshare->rx_packet_ss_a > 3)
			priv->pshare->rx_packet_ss_a = 3; 

		if( priv->pshare->rx_packet_ss_a == 3)
		{
			tmp_b30 = (tmp_b30 | BIT(21)) ; 
			phy_set_bb_reg(priv, 0xb30, bMaskDWord, tmp_b30 );
			printk("!!!! UP 3 PACKETS !!!! PATH A dd0[0x%x] > 0x%x, Change b30 = 0x%x!!!!\n\n", 
					tmp_dd0_a , ss_threshold_H, tmp_b30 );
		}
			
	}
	else if((tmp_dd0_a <= ss_threshold_L) && (tmp_b30 & BIT(21)))
	{
		if(priv->pshare->rx_packet_ss_a < 10)
			priv->pshare->rx_packet_ss_a = 10;
		
		priv->pshare->rx_packet_ss_a = (priv->pshare->rx_packet_ss_a+1) ;
		
		if(priv->pshare->rx_packet_ss_a > 13)
			priv->pshare->rx_packet_ss_a = 13; 
		
		if(priv->pshare->rx_packet_ss_a == 13)
		{
			tmp_b30 = (tmp_b30 & ~(BIT(21))) ; 
			phy_set_bb_reg(priv, 0xb30, bMaskDWord, tmp_b30 );
			printk("!!!! UP 3 PACKETS !!!! PATH A dd0[0x%x] < 0x%x, Change b30 = 0x%x!!!!\n\n", 
					tmp_dd0_a , ss_threshold_L, tmp_b30 );

		}
	}

	//======= PATH  B ============

	if((tmp_dd0_b >= ss_threshold_H) && (!(tmp_b30 & BIT(23))))
	{
		if(priv->pshare->rx_packet_ss_b >= 10)
			priv->pshare->rx_packet_ss_b = 0;
		
		priv->pshare->rx_packet_ss_b = (priv->pshare->rx_packet_ss_b+1); 
		
		if(priv->pshare->rx_packet_ss_b > 3)
			priv->pshare->rx_packet_ss_b = 3; 

		if( priv->pshare->rx_packet_ss_b == 3)
		{
			tmp_b30 = (tmp_b30 | BIT(23)) ; 
			phy_set_bb_reg(priv, 0xb30, bMaskDWord, tmp_b30 );
			printk("!!!! UP 3 PACKETS !!!! PATH B dd0[0x%x] > 0x%x, Change b30 = 0x%x!!!!\n\n", 
					tmp_dd0_b , ss_threshold_H, tmp_b30 );
		}
			
	}
	else if((tmp_dd0_b <= ss_threshold_L) && (tmp_b30 & BIT(23)))
	{
		if(priv->pshare->rx_packet_ss_b < 10)
			priv->pshare->rx_packet_ss_b = 10;
		
		priv->pshare->rx_packet_ss_b = (priv->pshare->rx_packet_ss_b+1) ;
		
		if(priv->pshare->rx_packet_ss_b > 13)
			priv->pshare->rx_packet_ss_b = 13; 
		
		if(priv->pshare->rx_packet_ss_b == 13)
		{
			tmp_b30 = (tmp_b30 & ~(BIT(23))) ; 
			phy_set_bb_reg(priv, 0xb30, bMaskDWord, tmp_b30 );
			printk("!!!! UP 3 PACKETS !!!! PATH B dd0[0x%x] < 0x%x, Change b30 = 0x%x!!!!\n\n", 
					tmp_dd0_b , ss_threshold_L, tmp_b30 );
		}
	}

}
#endif


#ifdef USE_OUT_SRC

#ifdef CONFIG_WLAN_HAL_8192EE

#if defined(CONFIG_PCI_HCI) && !defined(CONFIG_RTL_TRIBAND_SUPPORT)
/*static*/ 
#endif
void translate_CRC32_outsrc(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo, BOOLEAN CRC32, u2Byte PKT_LEN)
{
	//unsigned char 		*frame = get_pframe(pfrinfo) + (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size);
	//struct stat_info 	*pstat = get_stainfo(priv, GetAddr2Ptr(frame));
	//u1Byte		StationID;
	struct dm_struct				*p_dm_odm =  &(priv->pshare->_dmODM);
	struct phydm_phyinfo_struct *pPhyInfo = &pfrinfo->phy_info;
	//StationID = (pstat ? pstat->cmn_info.aid : 0);	//MAC ID

	//if(StationID!=0)
#ifdef CONFIG_PHYDM_ANTENNA_DIVERSITY
	u4Byte	weighting;
	u1Byte				isCCKrate=0;
	isCCKrate = is_CCK_rate(pfrinfo->rx_rate);
	if (PKT_LEN < 30)
	{
		return;
	}
	else
	{
		if(CRC32==CRC32_FAIL)
		{
			weighting=0;
		}
		else if(CRC32==CRC32_OK)
		{
			if(PKT_LEN>1000)
				weighting=10;
			else if(PKT_LEN>500)
				weighting=5;
			else if(PKT_LEN>100)
				weighting=3;
			else
				weighting=1;
		}
	}

	#ifdef CONFIG_PHYDM_ANTENNA_DIVERSITY
	odm_antsel_statistics(ODMPTR,pPhyInfo, 1, 1, weighting, CRC32_METHOD, isCCKrate);
	#endif
	
#endif
	
}

#endif
#endif

#if !defined(USE_OUT_SRC) || defined(_OUTSRC_COEXIST)
/*static*/ void translate_rssi_sq(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
	typedef signed char		s1Byte;
	typedef unsigned char	u1Byte;
	typedef int				s4Byte;
	typedef unsigned int	u4Byte;

	PHY_STS_OFDM_8192CD_T	*pOfdm_buf;
	PHY_STS_CCK_8192CD_T	*pCck_buf;
	u1Byte				*prxpkt;
	u1Byte				i, Max_spatial_stream, tmp_rxsnr, tmp_rxevm; //, tmp_rxrssi;
	s1Byte				rx_pwr[4], rx_pwr_all=0;
	s1Byte				rx_snrX, rx_evmX; //, rx_rssiX;
	u1Byte				EVM, PWDB_ALL;
	u4Byte				RSSI;
	u1Byte				isCCKrate=0;
#if defined(CONFIG_RTL_92C_SUPPORT) || defined(CONFIG_RTL_92D_SUPPORT)
	u1Byte				report;
#endif
#if defined(CONFIG_RTL_92C_SUPPORT) || defined(CONFIG_RTL_88E_SUPPORT)
	unsigned int		ofdm_max_rssi=0, ofdm_min_rssi=0xff;
#endif

	/* 2007/07/04 MH For OFDM RSSI. For high power or not. */
	//static u1Byte		check_reg824 = 0;
	//static u4Byte		reg824_bit9 = 0;

	isCCKrate = is_CCK_rate(pfrinfo->rx_rate);

#ifdef CONFIG_PCI_HCI
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
		/*2007.08.30 requested by SD3 Jerry */
		if (priv->pshare->phw->check_reg824 == 0) {
			priv->pshare->phw->reg824_bit9 = phy_query_bb_reg(priv, rFPGA0_XA_HSSIParameter2, 0x200);
			priv->pshare->phw->check_reg824 = 1;
		}
	}
#endif

	prxpkt = (u1Byte *)pfrinfo->driver_info;

	/* Initial the cck and ofdm buffer pointer */
	pCck_buf = (PHY_STS_CCK_8192CD_T *)prxpkt;
	pOfdm_buf = (PHY_STS_OFDM_8192CD_T *)prxpkt;

	memset(&pfrinfo->rf_info, 0, sizeof(struct rf_misc_info));
	pfrinfo->rf_info.mimosq[0] = -1;
	pfrinfo->rf_info.mimosq[1] = -1;

	if (isCCKrate) {
/*
		//
		// (1)Hardware does not provide RSSI for CCK
		//
		if ((get_rf_mimo_mode(priv) == RF_2T4R) && (priv->pshare->rf_ft_var.cck_sel_ver == 2)) {
			for (i=RF_PATH_A; i<RF_PATH_MAX; i++) {
				tmp_rxrssi = pCck_buf->adc_pwdb_X[i];
				rx_rssiX = (s1Byte)(tmp_rxrssi);
				rx_rssiX /= 2;
				pfrinfo->cck_mimorssi[i] = rx_rssiX;
			}
		}
*/
		//
		// (2)PWDB, Average PWDB cacluated by hardware (for rate adaptive)
		//
#if defined(CONFIG_RTL_92C_SUPPORT) || defined(CONFIG_RTL_92D_SUPPORT)
		if (
#ifdef CONFIG_RTL_92C_SUPPORT
			(GET_CHIP_VER(priv) == VERSION_8192C)||(GET_CHIP_VER(priv) == VERSION_8188C) 
#endif
#ifdef CONFIG_RTL_92D_SUPPORT
#ifdef CONFIG_RTL_92C_SUPPORT
			|| 
#endif
			(GET_CHIP_VER(priv) == VERSION_8192D)
#endif
			) {
			if (!priv->pshare->phw->reg824_bit9) {
				report = pCck_buf->cck_agc_rpt & 0xc0;
				report = report>>6;

#ifdef CONFIG_RTL_92C_SUPPORT
				if ((GET_CHIP_VER(priv) == VERSION_8192C)||(GET_CHIP_VER(priv) == VERSION_8188C)) {
					switch (report) {
					case 0x3:
						rx_pwr_all = -46 - (pCck_buf->cck_agc_rpt & 0x3e);
						break;
					case 0x2:
						rx_pwr_all = -26 - (pCck_buf->cck_agc_rpt & 0x3e);
						break;
					case 0x1:
						rx_pwr_all = -12 - (pCck_buf->cck_agc_rpt & 0x3e);
						break;
					case 0x0:
						rx_pwr_all = 16 - (pCck_buf->cck_agc_rpt & 0x3e);
						break;
					}
				} else 
#endif
				{
					switch (report) {
					//Fixed by Wish and BB Cherry 2013.12.04
					case 0x3:
						rx_pwr_all = -46 - (pCck_buf->cck_agc_rpt & 0x3e);
						break;
					case 0x2:
						rx_pwr_all = -26 - (pCck_buf->cck_agc_rpt & 0x3e);
						break;
					case 0x1:
						rx_pwr_all = -12 - (pCck_buf->cck_agc_rpt & 0x3e);
						break;
					case 0x0:
						rx_pwr_all = 16 - (pCck_buf->cck_agc_rpt & 0x3e);
						break;
					}
				}
			} else {
				report = pCck_buf->cck_agc_rpt & 0x60;
				report = report>>5;

#ifdef CONFIG_RTL_92C_SUPPORT
				if ((GET_CHIP_VER(priv) == VERSION_8192C)||(GET_CHIP_VER(priv) == VERSION_8188C)) {
					switch (report) {
					case 0x3:
						rx_pwr_all = -46 - ((pCck_buf->cck_agc_rpt & 0x1f)<<1) ;
						break;
					case 0x2:
						rx_pwr_all = -26 - ((pCck_buf->cck_agc_rpt & 0x1f)<<1);
						break;
					case 0x1:
						rx_pwr_all = -12 - ((pCck_buf->cck_agc_rpt & 0x1f)<<1) ;
						break;
					case 0x0:
						rx_pwr_all = 16 - ((pCck_buf->cck_agc_rpt & 0x1f)<<1) ;
						break;
					}
				} else 
#endif
				{
					switch (report) {
					//Fixed by Wish and BB Cherry 2013.12.04
					case 0x3:
						rx_pwr_all = -46 - ((pCck_buf->cck_agc_rpt & 0x1f)<<1);
						break;
					case 0x2:
						rx_pwr_all = -26 - ((pCck_buf->cck_agc_rpt & 0x1f)<<1);
						break;
					case 0x1:
						rx_pwr_all = -12 - ((pCck_buf->cck_agc_rpt & 0x1f)<<1);
						break;
					case 0x0:
						rx_pwr_all = 16 - ((pCck_buf->cck_agc_rpt & 0x1f)<<1);
						break;
					}
				}
			}
			PWDB_ALL = QueryRxPwrPercentage(rx_pwr_all);

#ifdef CONFIG_RTL_92C_SUPPORT
			if ((GET_CHIP_VER(priv) == VERSION_8192C)||(GET_CHIP_VER(priv) == VERSION_8188C)) {
#ifdef HIGH_POWER_EXT_LNA
				if (priv->pshare->rf_ft_var.use_ext_lna) {
					if (!(pCck_buf->cck_agc_rpt>>7))
						PWDB_ALL = (PWDB_ALL>94)?100:(PWDB_ALL + 6);
					else
						PWDB_ALL = (PWDB_ALL<16)?0:(PWDB_ALL -16);

					/* CCK Modification */
					if (PWDB_ALL > 25 && PWDB_ALL <= 60)
						PWDB_ALL += 6;
	/*
					else if (PWDB_ALL <= 25)
						PWDB_ALL += 8;
	*/
				} else 
#endif
				{
					if (PWDB_ALL > 99)
						PWDB_ALL -= 8;
					else if (PWDB_ALL > 50 && PWDB_ALL <= 68)
						PWDB_ALL += 4;
				}

				pfrinfo->rf_info.rssi = PWDB_ALL;
#ifdef HIGH_POWER_EXT_LNA
				if (priv->pshare->rf_ft_var.use_ext_lna)
					pfrinfo->rf_info.rssi+=10;
#endif				
			} else 
#endif
			{
				pfrinfo->rf_info.rssi = PWDB_ALL;
				pfrinfo->rf_info.rssi+=3;
			}

			if (pfrinfo->rf_info.rssi > 100)
				pfrinfo->rf_info.rssi = 100;
		}
#endif

#ifdef CONFIG_RTL_88E_SUPPORT
		if (GET_CHIP_VER(priv)==VERSION_8188E) {
			unsigned int LNA_idx = ((pCck_buf->cck_agc_rpt & 0xE0) >>5);
			unsigned int VGA_idx = (pCck_buf->cck_agc_rpt & 0x1F); 
			switch(LNA_idx) {
			case 7:
				if(VGA_idx <= 27)
					rx_pwr_all = -100 + 2*(27-VGA_idx); //VGA_idx = 27~2
				else
					rx_pwr_all = -100;
				break;
			case 6:
				rx_pwr_all = -48 + 2*(2-VGA_idx); //VGA_idx = 2~0
				break;
			case 5:
				rx_pwr_all = -42 + 2*(7-VGA_idx); //VGA_idx = 7~5
				break;
			case 4:
				rx_pwr_all = -36 + 2*(7-VGA_idx); //VGA_idx = 7~4
				break;
			case 3:
				//rx_pwr_all = -28 + 2*(7-VGA_idx); //VGA_idx = 7~0
				rx_pwr_all = -24 + 2*(7-VGA_idx); //VGA_idx = 7~0
				break;
			case 2:
				if(priv->pshare->phw->reg824_bit9)
					rx_pwr_all = -12 + 2*(5-VGA_idx); //VGA_idx = 5~0
				else
					rx_pwr_all = -6+ 2*(5-VGA_idx);
				break;
			case 1:
				rx_pwr_all = 8-2*VGA_idx;
				break;
			case 0:
				rx_pwr_all = 14-2*VGA_idx;
				break;
			default:
				printk("%s %d, CCK Exception default\n", __FUNCTION__, __LINE__);
				break;
			}
			rx_pwr_all += 6;
			PWDB_ALL = QueryRxPwrPercentage(rx_pwr_all);

			if(!priv->pshare->phw->reg824_bit9) {
				if(PWDB_ALL >= 80)
					PWDB_ALL = ((PWDB_ALL-80)<<1)+((PWDB_ALL-80)>>1)+80;
				else if((PWDB_ALL <= 78) && (PWDB_ALL >= 20))
					PWDB_ALL += 3;
				if(PWDB_ALL>100)
					PWDB_ALL = 100;
			}

			pfrinfo->rf_info.rssi = PWDB_ALL;
		}
#endif

		//
		// (3) Get Signal Quality (EVM)
		//
		// if(bPacketMatchBSSID)
		{
			u1Byte SQ;

			if (pfrinfo->rf_info.rssi > 40) {
				SQ = 100;
			} else {
				SQ = pCck_buf->SQ_rpt;

				if (pCck_buf->SQ_rpt > 64)
					SQ = 0;
				else if (pCck_buf->SQ_rpt < 20)
					SQ = 100;
				else
					SQ = ((64-SQ) * 100) / 44;
			}
			pfrinfo->rf_info.sq = SQ;
			pfrinfo->rf_info.mimosq[0] = SQ;
		}
	} else {
		//
		// (1)Get RSSI for HT rate
		//
		for (i=RF_PATH_A; i<RF_PATH_MAX; i++) {
#if defined(CONFIG_RTL_92C_SUPPORT) || defined(CONFIG_RTL_88E_SUPPORT)
			if (
#ifdef CONFIG_RTL_92C_SUPPORT
				(GET_CHIP_VER(priv) == VERSION_8192C)||(GET_CHIP_VER(priv) == VERSION_8188C)
#endif
#ifdef CONFIG_RTL_88E_SUPPORT
#ifdef CONFIG_RTL_92C_SUPPORT
				||
#endif
				(GET_CHIP_VER(priv) == VERSION_8188E)
#endif
				)
				rx_pwr[i] = ((pOfdm_buf->trsw_gain_X[i]&0x3F)*2) - 110;
			else
#endif
				rx_pwr[i] = ((pOfdm_buf->trsw_gain_X[i]&0x3F)*2) - 106;

			//Get Rx snr value in DB
			if (priv->pshare->rf_ft_var.rssi_dump) {
				tmp_rxsnr =	pOfdm_buf->rxsnr_X[i];
				rx_snrX = (s1Byte)(tmp_rxsnr);
				rx_snrX >>= 1;
				pfrinfo->rf_info.RxSNRdB[i] = (s4Byte)rx_snrX;
			}

			/* Translate DBM to percentage. */
			RSSI = QueryRxPwrPercentage(rx_pwr[i]);
			//total_rssi += RSSI;

#if defined(CONFIG_RTL_92C_SUPPORT) || defined(CONFIG_RTL_88E_SUPPORT)
			if ((
#ifdef CONFIG_RTL_92C_SUPPORT
				(GET_CHIP_VER(priv) == VERSION_8192C)||(GET_CHIP_VER(priv) == VERSION_8188C)
#endif
#ifdef CONFIG_RTL_88E_SUPPORT
#ifdef CONFIG_RTL_92C_SUPPORT
				||
#endif
				(GET_CHIP_VER(priv) == VERSION_8188E)
#endif
				) 
#ifdef HIGH_POWER_EXT_LNA
				&& (priv->pshare->rf_ft_var.use_ext_lna)
#endif
				) {
				if ((pOfdm_buf->trsw_gain_X[i]>>7) == 1)
					RSSI = (RSSI>94)?100:(RSSI + 6);
				else
					RSSI = (RSSI<16)?0:(RSSI -16);

				if (RSSI <= 34 && RSSI >= 4)
					RSSI -= 4;
			}
#endif

			/* Record Signal Strength for next packet */
			//if(bPacketMatchBSSID)
			{
				pfrinfo->rf_info.mimorssi[i] = (u1Byte)RSSI;				
			}

#if defined(CONFIG_RTL_92C_SUPPORT) || defined(CONFIG_RTL_88E_SUPPORT)
			if (
#ifdef CONFIG_RTL_92C_SUPPORT
				(GET_CHIP_VER(priv) == VERSION_8192C)||(GET_CHIP_VER(priv) == VERSION_8188C)
#endif
#ifdef CONFIG_RTL_88E_SUPPORT
#ifdef CONFIG_RTL_92C_SUPPORT
				||
#endif
				(GET_CHIP_VER(priv) == VERSION_8188E)
#endif
				) {
				if (RSSI > ofdm_max_rssi)
					ofdm_max_rssi = RSSI;
				if (RSSI < ofdm_min_rssi)
					ofdm_min_rssi = RSSI;
			}
#endif
		}

		//
		// (2)PWDB, Average PWDB cacluated by hardware (for rate adaptive)
		//
#if defined(CONFIG_RTL_92C_SUPPORT) || defined(CONFIG_RTL_88E_SUPPORT)
		if (
#ifdef CONFIG_RTL_92C_SUPPORT
			(GET_CHIP_VER(priv) == VERSION_8192C)||(GET_CHIP_VER(priv) == VERSION_8188C)
#endif
#ifdef CONFIG_RTL_88E_SUPPORT
#ifdef CONFIG_RTL_92C_SUPPORT
			||
#endif
			(GET_CHIP_VER(priv) == VERSION_8188E)
#endif
			) {
			if ((ofdm_max_rssi - ofdm_min_rssi) < 3)
				PWDB_ALL = ofdm_max_rssi;
			else if ((ofdm_max_rssi - ofdm_min_rssi) < 6)
				PWDB_ALL = ofdm_max_rssi - 1;
			else if ((ofdm_max_rssi - ofdm_min_rssi) < 10)
				PWDB_ALL = ofdm_max_rssi - 2;
			else
				PWDB_ALL = ofdm_max_rssi - 3;
		} else 
#endif
		{
			rx_pwr_all = (((pOfdm_buf->pwdb_all ) >> 1 )& 0x7f) -106;
			PWDB_ALL = QueryRxPwrPercentage(rx_pwr_all);
		}

		pfrinfo->rf_info.rssi = PWDB_ALL;

		//
		// (3)EVM of HT rate
		//
		//eric_8814 ?? 3 spatial stream ??
		if ((pfrinfo->rx_rate >= _MCS8_RATE_) && (pfrinfo->rx_rate <= _MCS15_RATE_))
			Max_spatial_stream = 2; //both spatial stream make sense
		else
			Max_spatial_stream = 1; //only spatial stream 1 makes sense

		for (i=0; i<Max_spatial_stream; i++) {
			tmp_rxevm =	pOfdm_buf->rxevm_X[i];
			rx_evmX = (s1Byte)(tmp_rxevm);

			// Do not use shift operation like "rx_evmX >>= 1" because the compilor of free build environment
			// fill most significant bit to "zero" when doing shifting operation which may change a negative
			// value to positive one, then the dbm value (which is supposed to be negative)  is not correct anymore.
			rx_evmX /= 2;	//dbm

			EVM = EVMdbToPercentage(rx_evmX);

			//if(bPacketMatchBSSID)
			{
				if (i==0) // Fill value in RFD, Get the first spatial stream only
				{
					pfrinfo->rf_info.sq = (u1Byte)(EVM & 0xff);
				}
				pfrinfo->rf_info.mimosq[i] = (u1Byte)(EVM & 0xff);
			}
		}
	}
}
#endif


#if 0/*def CONFIG_RTL_STP*/
int rtl865x_wlanIF_Init(struct net_device *dev)
{
	if (dev == NULL)
		return FALSE;
	else
	{
		wlan_pseudo_dev = dev;
		printk("init wlan pseudo dev =====> %s\n", wlan_pseudo_dev->name);
	}
	return TRUE;
}
#endif


#ifdef SUPPORT_RX_UNI2MCAST
static unsigned int check_mcastL2L3Diff(struct sk_buff *skb)
{
	unsigned int DaIpAddr;
	#ifdef __ECOS
	struct iphdr* iph = (struct iphdr *)(skb->data + ETH_HLEN);
	#else
	struct iphdr* iph = SKB_IP_HEADER(skb);
	#endif

#ifdef _LITTLE_ENDIAN_
	DaIpAddr = ntohl(iph->daddr);
#else
	DaIpAddr = (unsigned int)iph->daddr;
#endif
	//printk("ip:%d, %d ,%d ,%d\n",(DaIpAddr>>24) ,(DaIpAddr<<8)>>24,(DaIpAddr<<16)>>24,(DaIpAddr<<24)>>24);

	if (((DaIpAddr & 0xFF000000) >= 0xE0000000) && ((DaIpAddr & 0xFF000000) <= 0xEF000000)) {
		#ifdef __ECOS
		if (!IP_MCAST_MAC(skb->data))
		#else
		if (!IP_MCAST_MAC(SKB_MAC_HEADER(skb)))
		#endif
			return DaIpAddr;
	}
	return 0;
}


static void ConvertMCastIPtoMMac(unsigned int group, unsigned char *gmac)
{
	unsigned int u32tmp, tmp;
	static int i;

	u32tmp = group & 0x007FFFFF;
	gmac[0] = 0x01;
	gmac[1] = 0x00;
	gmac[2] = 0x5e;

	for (i=5; i>=3; i--) {
		tmp = u32tmp & 0xFF;
		gmac[i] = tmp;
		u32tmp >>= 8;
	}
}


void CheckUDPandU2M(struct sk_buff *pskb)
{
	int MultiIP;

	MultiIP = check_mcastL2L3Diff(pskb);
	if (MultiIP) {
		unsigned char mactmp[6];
		ConvertMCastIPtoMMac(MultiIP, mactmp);
		//printk("%02x%02x%02x:%02x%02x%02x\n", mactmp[0],mactmp[1],mactmp[2],
		//      mactmp[3],mactmp[4],mactmp[5]);
	#ifdef __ECOS
		memcpy(pskb->data, mactmp, 6);
	#else
		memcpy(SKB_MAC_HEADER(pskb), mactmp, 6);
	#endif
#if defined(__LINUX_2_6__)
		/*added by qinjunjie,warning:should not remove next line*/
		pskb->pkt_type = PACKET_MULTICAST;
#endif
	}
}

void CheckV6UDPandU2M(struct sk_buff *pskb)
{
#ifdef __ECOS
	struct ip6_hdr *iph = (struct ip6_hdr *)(pskb->data + ETH_HLEN);
	unsigned char *DDA=pskb->data;	
#else
	struct ipv6hdr *iph;
	unsigned char *DDA;

	iph = (struct ipv6hdr *)SKB_IP_HEADER(pskb);
	DDA = (unsigned char *)SKB_MAC_HEADER(pskb);
#endif

	/*ip(v6) format is  multicast ip*/
#ifdef __ECOS
	if (iph->ip6_dst.s6_addr[0] == 0xff){
#else
	if (iph->daddr.s6_addr[0] == 0xff){
#endif

		/*mac is not ipv6 multicase mac*/
		if(!ICMPV6_MCAST_MAC(DDA) ){
			/*change mac (DA) to (ipv6 multicase mac) format by (ipv6 multicast ip)*/
			DDA[0] = 0x33;
			DDA[1] = 0x33;
		#ifdef __ECOS
			memcpy(DDA+2, &iph->ip6_dst.s6_addr[12], 4);
		#else
			memcpy(DDA+2, &iph->daddr.s6_addr[12], 4);
		#endif
		}
	}
}
#endif


#if defined(CONFIG_RTL_92C_SUPPORT) || defined(CONFIG_RTL_92D_SUPPORT)
/* 88C, 92C, and 92D need to check privacy algorithm and accept icv error packet when using CCMP,
                      because hw may report wrong icv status when using CCMP privacy*/  
static __inline__ unsigned int check_icverr_drop(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo) {/*return 0: accept packet, 1: drop packet*/
    unsigned int privacy;
    struct stat_info *pstat;
    if((GET_CHIP_VER(priv)== VERSION_8192C) || (GET_CHIP_VER(priv)== VERSION_8192D) || (GET_CHIP_VER(priv)== VERSION_8188C)){
        privacy = 0;
        pstat = NULL;
        #if defined(WDS) || defined(CONFIG_RTK_MESH) || defined(A4_STA)
        if (get_tofr_ds((unsigned char *)get_pframe(pfrinfo)) == 3) {            
            pstat = get_stainfo(priv, (unsigned char *)GetAddr2Ptr((unsigned char *)get_pframe(pfrinfo)));
        } else
        #endif
        {
            pstat = get_stainfo(priv, (unsigned char *)get_sa((unsigned char *)get_pframe(pfrinfo)));
        }

        if (pstat) {
            if (OPMODE & WIFI_AP_STATE) {
                #if defined(WDS) || defined(CONFIG_RTK_MESH) || defined(A4_STA)
                if (get_tofr_ds((unsigned char *)get_pframe(pfrinfo)) == 3) {
                    #if defined(CONFIG_RTK_MESH)
                    if(priv->pmib->dot1180211sInfo.mesh_enable) {
                        privacy = (IS_MCAST(GetAddr1Ptr((unsigned char *)get_pframe(pfrinfo)))) ? _NO_PRIVACY_ : priv->pmib->dot11sKeysTable.dot11Privacy;
                    } else
                    #endif
                    #if defined(WDS)
                    if(priv->pmib->dot11WdsInfo.wdsEnabled) {
                        privacy = priv->pmib->dot11WdsInfo.wdsPrivacy;
                    } else
                    #endif
                    #if defined(A4_STA)
                    if (priv->pmib->miscEntry.a4_enable) {
                        privacy = get_sta_encrypt_algthm(priv, pstat);
                    } else
                    #endif
                    {;}
                }
                else
                #endif	/* defined(WDS) || defined(CONFIG_RTK_MESH) */
                    {privacy = get_sta_encrypt_algthm(priv, pstat);}
            }
            #if defined(CLIENT_MODE)
            else {
                privacy = get_sta_encrypt_algthm(priv, pstat);
            }
            #endif

            if (privacy == _CCMP_PRIVACY_)
                return 0; /* do not drop this packet*/                                
        }
    }
    return 1;/* drop this packet*/
}
#endif

#ifdef BR_SHORTCUT
#ifdef CONFIG_RTL8672
extern struct net_device *get_eth_cached_dev(unsigned char *da);
#else
#ifdef CONFIG_RTL_819X
__inline__ struct net_device *get_eth_cached_dev(unsigned char *da)
{
#if defined(CONFIG_RTL_819X_SWCORE)	
    extern unsigned char cached_eth_addr[MACADDRLEN];
    extern struct net_device *cached_dev;

    #if !defined(NOT_RTK_BSP) && !defined(__ECOS)
    extern unsigned char cached_eth_addr2[MACADDRLEN];
    extern struct net_device *cached_dev2;

    extern unsigned char cached_eth_addr3[MACADDRLEN];
    extern struct net_device *cached_dev3;

    extern unsigned char cached_eth_addr4[MACADDRLEN];
    extern struct net_device *cached_dev4;
    #endif // !NOT_RTK_BSP
#endif

    struct net_device * dev = NULL;
    #if defined(__KERNEL__) || defined(__OSK__)
    struct net_bridge_port *br_port;
    #endif

#if defined(CONFIG_RTL_819X_SWCORE) || defined(CONFIG_ARCH_RTL8198F)
    if (cached_dev && isEqualMACAddr(da, cached_eth_addr))
        dev = cached_dev;
    #if !defined(NOT_RTK_BSP) && !defined(__ECOS)
    else if (cached_dev2 && isEqualMACAddr(da, cached_eth_addr2))
        dev = cached_dev2;
    else if (cached_dev3 && isEqualMACAddr(da, cached_eth_addr3))
        dev = cached_dev3;
    else if (cached_dev4 && isEqualMACAddr(da, cached_eth_addr4))	
        dev = cached_dev4;
    #endif // !NOT_RTK_BSP
#endif


    #if defined(__KERNEL__) || defined(__OSK__)
    if(dev) {
        br_port = GET_BR_PORT(dev);
        if(br_port) {
            if(br_port->br->stp_enabled && br_port->state != BR_STATE_FORWARDING) {               
                dev = NULL;
            }
        }  
    }
    #endif            

    return dev;      
}
#endif
#endif
#endif

#if ((defined(CONFIG_RTK_VLAN_SUPPORT) && defined(CONFIG_RTK_VLAN_FOR_CABLE_MODEM)) || defined(MCAST2UI_REFINE))
extern struct net_device* re865x_get_netdev_by_name(const char* name);
#endif

#ifdef _SINUX_
extern int g_sc_enable_brsc;
#endif

#ifdef _FULLY_WIFI_IGMP_SNOOPING_SUPPORT_
#include <linux/igmp.h>
static void ___ConvertMulticatIPtoMacAddr(__u32 group, unsigned char *gmac)
{
	__u32 u32tmp, tmp;
	int i;

	u32tmp = group & 0x007FFFFF;
	gmac[0]=0x01; gmac[1]=0x00; gmac[2]=0x5e;
	for (i=5; i>=3; i--) {
		tmp=u32tmp&0xFF;
		gmac[i]=tmp;
		u32tmp >>= 8;
	}
}

static char __igmp3_report_simple_check(struct sk_buff *skb, unsigned char *gmac, struct igmphdr *igmph)
{
	struct igmpv3_report *igmpv3;
	struct igmpv3_grec *igmpv3grec;
	unsigned int IGMP_Group;// add  for fit igmp v3
	__u16 rec_id =0;
	int srcnum=0;
	int op = 0;

	igmpv3 = (struct igmpv3_report *)igmph;
	igmpv3grec =  &igmpv3->grec[0];
		
	while( rec_id < ntohs(igmpv3->ngrec) )
	{
		IGMP_Group = be32_to_cpu(igmpv3grec->grec_mca);
		srcnum = ntohs(igmpv3grec->grec_nsrcs);

		/*check if it's protocol reserved group */
		if(IN_MULTICAST(IGMP_Group))
		{			
			___ConvertMulticatIPtoMacAddr(IGMP_Group, gmac);
			switch( igmpv3grec->grec_type )
			{
				case IGMPV3_MODE_IS_INCLUDE:
				case IGMPV3_CHANGE_TO_INCLUDE: 
					if (srcnum == 0){
						op = 0x8B81;	// SIOCGIMCAST_DEL;
					} else {
						op =0x8B80;	// SIOCGIMCAST_ADD;
					}
					break;
				case IGMPV3_MODE_IS_EXCLUDE:
				case IGMPV3_CHANGE_TO_EXCLUDE: 
				case IGMPV3_ALLOW_NEW_SOURCES:
					op =0x8B80;	// SIOCGIMCAST_ADD;
					break;
				case IGMPV3_BLOCK_OLD_SOURCES:
					op = 0x8B81;	// SIOCGIMCAST_DEL;
					break;
				default:
					//printk("%s> Not support Group Record Types [%x]\n", __FUNCTION__, igmpv3grec->grec_type );
					break;
			}
		} /*else {
		    printk("%s> Mcast err addr, group:%s, rec_id:%d, srcnum:%d\n", __FUNCTION__, inet_ntoa(IGMP_Group), rec_id, srcnum);
		}*/
			
		if (op != 0) {
			memcpy(gmac+6, SKB_MAC_HEADER(skb)+6, 6);
			ioctl_AddDelMCASTGroup2STA(skb->dev, (struct ifreq*)gmac, op);
			op =0;
		}

		rec_id++;
		igmpv3grec = (struct igmpv3_grec *)( (char*)igmpv3grec + sizeof( struct igmpv3_grec ) + (igmpv3grec->grec_auxwords+srcnum)*sizeof( __u32 ) );
	}

	return 3;
}

static char ___igmp_type_check(struct sk_buff *skb, unsigned char *gmac)
{
	struct iphdr *iph;
	__u8 hdrlen;
	struct igmphdr *igmph;
	unsigned int IGMP_Group;// add  for fit igmp v3

	/* check IP header information */
	iph = SKB_IP_HEADER(skb);
	hdrlen = iph->ihl << 2;
	if ((iph->version != 4) &&  (hdrlen < 20))
		return -1;

	if (ip_fast_csum((u8 *)iph, iph->ihl) != 0)
		return -2;

	{ /* check the length */
	__u32 len = ntohs(iph->tot_len);
	if (skb->len < len || len < hdrlen)
		return -3;
	}

	/* parsing the igmp packet */
	igmph = (struct igmphdr *)((u8*)iph+hdrlen);

	/*IGMP-V3 type Report*/
	if(igmph->type == IGMPV3_HOST_MEMBERSHIP_REPORT)
	{
		return __igmp3_report_simple_check(skb, gmac, igmph);
		
	}else{	//4 V2 or V1
		IGMP_Group = be32_to_cpu(igmph->group);
	}

	/*check if it's protocol reserved group */
	if(!IN_MULTICAST(IGMP_Group))
	{			
        if (igmph->type == IGMP_HOST_MEMBERSHIP_QUERY)
            return 4;
        else			
		  return -4;
	}

	___ConvertMulticatIPtoMacAddr(IGMP_Group, gmac);
	
	if ((igmph->type==IGMP_HOST_MEMBERSHIP_REPORT) ||
	    (igmph->type==IGMPV2_HOST_MEMBERSHIP_REPORT)) 
	{
		return 1; /* report and add it */
	}
	else if (igmph->type==IGMP_HOST_LEAVE_MESSAGE)
	{
		return 2; /* leave and delete it */
	}	
	
	return -5;
}

static void rtl_igmp_notify(struct sk_buff *skb, void *igmp_header)
{
	int op;
	char type;
	//struct igmphdr *ih = (struct igmphdr *)igmp_header;
	unsigned char StaMacAndGroup[20];
	
	type = ___igmp_type_check(skb,StaMacAndGroup);
	
	switch (type) {
	case 1:
		op = 0x8B80;	// SIOCGIMCAST_ADD;
		break;
	case 2:
		op = 0x8B81;	// SIOCGIMCAST_DEL;
		break;
	case 3: /* igmpv3 report */
    case 4: /* igmpv2 query */
		return;
	default:
		//DEBUG_ERR("IGMP Error type=%d\n",type);
		return;
		
	}
	memcpy(StaMacAndGroup+6, SKB_MAC_HEADER(skb)+6, 6);
	ioctl_AddDelMCASTGroup2STA(skb->dev, (struct ifreq*)StaMacAndGroup, op);
}

void check_igmp_snooping_pkt( struct sk_buff *pskb )
{
	unsigned char *dest = eth_hdr(pskb)->h_dest;
	struct iphdr *iph;
	int vlanTag = 0;
	
	if (IS_MCAST(dest)) 
	{
		if (unlikely((*(unsigned short *)(SKB_MAC_HEADER(pskb) + ETH_ALEN * 2) == __constant_htons(ETH_P_8021Q))))
			vlanTag = 4;
		if (unlikely((*(unsigned short *)(SKB_MAC_HEADER(pskb) + ETH_ALEN * 2 + vlanTag) == __constant_htons(ETH_P_IP)))) 
		{
			iph = SKB_IP_HEADER(pskb);
			if (vlanTag)
				iph = (struct iphdr *)((unsigned char*)iph + vlanTag);
#if defined(LINUX_2_6_22_)
			pskb->network_header = pskb->mac_header+ETH_HLEN+vlanTag;
#else
			pskb->nh.raw = (unsigned char *)iph;
#endif
			if (unlikely(iph->protocol == IPPROTO_IGMP)) 
			{
				//printk("dest=%02x-%02x-%02x-%02x-%02x-%02x\n",dest[0],dest[1],dest[2],dest[3],dest[4],dest[5]);
#if defined(LINUX_2_6_22_)
				pskb->transport_header = pskb->network_header + (iph->ihl * 4);
				rtl_igmp_notify(pskb, skb_transport_header(pskb));
#else
				pskb->h.raw = pskb->nh.raw + (iph->ihl * 4);
				rtl_igmp_notify(pskb, pskb->h.raw);
#endif
			}
		}
	}
}


#endif //_FULLY_WIFI_IGMP_SNOOPING_SUPPORT_

#ifdef _FULLY_WIFI_MLD_SNOOPING_SUPPORT_
#define	IPV6_ROUTER_ALTER_OPTION 0x05020000
#define	HOP_BY_HOP_OPTIONS_HEADER 0
#define	ROUTING_HEADER	43
#define	FRAGMENT_HEADER	44
#define	DESTINATION_OPTION_HEADER 60

#define	ICMP_PROTOCOL	58

#define	MLD_QUERY	130
#define	MLDV1_REPORT	131
#define	MLDV1_DONE	132
#define	MLDV2_REPORT	143

#define MLD2_CHANGE_TO_INCLUDE  3
#define MLD2_CHANGE_TO_EXCLUDE  4

/*Convert  MultiCatst IPV6_Addr to MAC_Addr*/
static void ___ConvertMulticatIPv6toMacAddr(unsigned char* icmpv6_McastAddr, unsigned char *gmac)
{
	/*ICMPv6 valid addr 2^32 -1*/
	gmac[0] = 0x33;
	gmac[1] = 0x33;
	gmac[2] = icmpv6_McastAddr[12];
	gmac[3] = icmpv6_McastAddr[13];
	gmac[4] = icmpv6_McastAddr[14];
	gmac[5] = icmpv6_McastAddr[15];			
}

static char ___mld_type_check(struct sk_buff *skb, unsigned char *gmac)
{
	unsigned char *ptr;
#ifdef __ECOS
	struct ip6_hdr *ipv6h;
#else
	struct ipv6hdr *ipv6h;
#endif

	unsigned char	*startPtr = NULL;
	unsigned char	*lastPtr = NULL;
	unsigned char	nextHeader = 0;
	unsigned short	extensionHdrLen = 0;

#if defined(LINUX_2_6_22_)
	ptr = (unsigned char *)skb_network_header(skb);
#else
	ptr = (unsigned char *)skb->nh.raw;
#endif

#ifdef __ECOS
	ipv6h = (struct ip6_hdr *)ptr;
	if (ipv6h->ip6_vfc != IPV6_VERSION)
#else
	ipv6h = (struct ipv6hdr *)ptr;
	if (ipv6h->version != 6)
#endif
	{
		return -1;
	}

	startPtr = (unsigned char *)ptr;
#ifdef __ECOS
	lastPtr = startPtr + sizeof(struct ip6_hdr) + ntohs(ipv6h->ip6_plen);
	nextHeader = ipv6h->ip6_nxt;
	ptr = startPtr + sizeof(struct ip6_hdr);
#else
	lastPtr = startPtr + sizeof(struct ipv6hdr) + ntohs(ipv6h->payload_len);
	nextHeader = ipv6h->nexthdr;
	ptr = startPtr + sizeof(struct ipv6hdr);
#endif

	while (ptr < lastPtr)
	{
		switch (nextHeader)
		{
			//printk("nextHeader : %d\n", nextHeader);

			case HOP_BY_HOP_OPTIONS_HEADER:
				/*parse hop-by-hop option*/
				nextHeader = ptr[0];
				extensionHdrLen = ((u16)(ptr[1]) + 1) * 8;
				ptr = ptr + extensionHdrLen;
				break;

			case ROUTING_HEADER:
				nextHeader = ptr[0];
				extensionHdrLen = ((u16)(ptr[1]) + 1) * 8;
				ptr = ptr + extensionHdrLen;
				break;

			case FRAGMENT_HEADER:
				nextHeader = ptr[0];
				ptr = ptr + 8;
				break;

			case DESTINATION_OPTION_HEADER:
				nextHeader = ptr[0];
				extensionHdrLen = ((u16)(ptr[1]) + 1) * 8;
				ptr = ptr + extensionHdrLen;
				break;

			case ICMP_PROTOCOL:
				if (ptr[0] == MLDV2_REPORT) {
				    unsigned short srcnum = ( (ptr[6]<<8) + ptr[7]);
				    int i=0, op=0, action=0;
					//printk("MLDV2_REPORT\n");
					for (i=0; i<srcnum; i++ ) {
						___ConvertMulticatIPv6toMacAddr(ptr + 12 + i*20, gmac);
						if (ptr[8 + i*20] == MLD2_CHANGE_TO_EXCLUDE) {
						//printk("MLD2_CHANGE_TO_EXCLUDE\n");
							op = 0x8B80;	/* SIOCGIMCAST_ADD;  report and add it */
						} else if (ptr[8 + i*20] == MLD2_CHANGE_TO_INCLUDE) {
							//printk("MLD2_CHANGE_TO_INCLUDE\n");
							op = 0x8B81;	/* SIOCGIMCAST_DEL;  leave and delete it */
						}

						if (op != 0) {
							memcpy(gmac + 6, SKB_MAC_HEADER(skb) + 6, 6);
							ioctl_AddDelMCASTGroup2STA(skb->dev, (struct ifreq*)gmac, op);
							op = 0;
							action++;
						}
					}
					if (action)
						return 3;
				} else if (ptr[0] == MLDV1_REPORT) {
					//printk("MLDV1_REPORT\n");
					___ConvertMulticatIPv6toMacAddr(ptr + 8, gmac);
					return 1; /* report and add it */
				} else if (ptr[0] == MLDV1_DONE) {
					//printk("MLDV1_DONE\n");
					___ConvertMulticatIPv6toMacAddr(ptr + 8, gmac);
					return 2; /* leave and delete it */
				}
	
				return -2;
				break;

			default:
				return -3;
		}
	}

	return -4;
}

static void rtl_mld_notify(struct sk_buff *skb)
{
	int op;
	char type;
	unsigned char StaMacAndGroup[20];
	
	type = ___mld_type_check(skb, StaMacAndGroup);
	//printk("MLD type = %d\n", type);
	
	switch (type) {
		case 1:
			op = 0x8B80;	// SIOCGIMCAST_ADD;
			break;
		case 2:
			op = 0x8B81;	// SIOCGIMCAST_DEL;
			break;
		case 3:
		    return;
		default:
			//printk("MLD Error type = %d\n", type);
			return;		
	}

	memcpy(StaMacAndGroup + 6, SKB_MAC_HEADER(skb) + 6, 6);
	ioctl_AddDelMCASTGroup2STA(skb->dev, (struct ifreq*)StaMacAndGroup, op);
	//printk("StaMacAndGroup[0] = %02x-%02x-%02x-%02x-%02x-%02x\n", StaMacAndGroup[0], StaMacAndGroup[1], StaMacAndGroup[2], StaMacAndGroup[3], StaMacAndGroup[4], StaMacAndGroup[5]);
	//printk("StaMacAndGroup[6] = %02x-%02x-%02x-%02x-%02x-%02x\n", StaMacAndGroup[0+6], StaMacAndGroup[1+6], StaMacAndGroup[2+6], StaMacAndGroup[3+6], StaMacAndGroup[4+6], StaMacAndGroup[5+6]);
}

void check_mld_snooping_pkt(struct sk_buff *pskb)
{
	unsigned char *dest = eth_hdr(pskb)->h_dest;
	struct iphdr *iph;
	int vlanTag = 0;
	
	if (IPV6_MCAST_MAC(dest)) 
	{
		if (unlikely((*(unsigned short *)(SKB_MAC_HEADER(pskb) + ETH_ALEN * 2) == __constant_htons(ETH_P_8021Q))))
			vlanTag = 4;

		if (unlikely((*(unsigned short *)(SKB_MAC_HEADER(pskb) + ETH_ALEN * 2 + vlanTag) == __constant_htons(ETH_P_IPV6)))) 
		{
			iph = SKB_IP_HEADER(pskb);
			if (vlanTag)
				iph = (struct iphdr *)((unsigned char*)iph + vlanTag);

#if defined(LINUX_2_6_22_)
			pskb->network_header = pskb->mac_header+ETH_HLEN+vlanTag;
#else
			pskb->nh.raw = (unsigned char *)iph;
#endif
			//printk("dest = %02x-%02x-%02x-%02x-%02x-%02x\n", dest[0], dest[1], dest[2], dest[3], dest[4], dest[5]);
			rtl_mld_notify(pskb);			
		}
	}
}
#endif //_FULLY_WIFI_MLD_SNOOPING_SUPPORT_





#if defined(BR_SHORTCUT)
#if defined(CONFIG_RTL_HTTP_REDIRECT)
extern int is_skb_http_packet(struct sk_buff* skb);
#endif
#if defined(CONFIG_RTL_DNS_TRAP)
extern int is_skb_dns_packet(struct sk_buff* skb);
#endif

#if defined(CONFIG_ARCH_RTL8198F) && defined(CONFIG_HWNAT_FLEETCONNTRACK)
extern int ca_ni_start_xmit_native_from_brsc(struct sk_buff *skb, struct net_device *dev);
#endif

#if defined(CONFIG_RTL_BRSHORTCUT_LINUX_VLAN_CTL)
extern unsigned int statistic_brsc_wlan_xmit_to_eth;
#endif

int gothrough_brsrc(struct rtl8192cd_priv *priv, struct sk_buff *pskb, struct stat_info *pstat)
{
    struct net_device *cached_dev = NULL;
    unsigned char* cached_sta_macPtr = NULL;
    struct net_device *cached_sta_devPtr = NULL;
    struct net_bridge_port *br_port = NULL;
#ifdef CONFIG_RTL_DNS_TRAP
    int dns_packet = 0;
#endif
#ifdef CONFIG_RTL_HTTP_REDIRECT
    int http_packet = 0;
#endif

#if !defined(CONFIG_RTL_BRSHORTCUT_LINUX_VLAN_CTL)
    #ifdef CONFIG_RTL_VLAN_8021Q
    if (linux_vlan_enable)
        return 0;
    #endif
#endif

#if defined(__KERNEL__) || defined(__OSK__)
    br_port = GET_BR_PORT(pskb->dev);
    if(br_port) {
        if(br_port->br->stp_enabled && br_port->state != BR_STATE_FORWARDING) {
            return 0;
        }
    }
#endif

	/*if lltd, don't go shortcut*/
	if (*(unsigned short *)(pskb->data+ETH_ALEN*2) == htons(0x88d9))
		return 0;

#ifdef CONFIG_RTL_DNS_TRAP
	dns_packet = is_skb_dns_packet(pskb);
#endif
#if defined(CONFIG_RTL_HTTP_REDIRECT)
	http_packet = is_skb_http_packet(pskb);
#endif

#ifdef CONFIG_RTK_MESH
	if (pskb->dev == priv->mesh_dev 
#if defined(CONFIG_RTL_MESH_SINGLE_IFACE)
            && priv->mesh_priv_first->pmib->dot1180211sInfo.mesh_portal_enable
#endif
        ) {
		int index = 0;
#ifdef CONFIG_RTL_MESH_CROSSBAND
		index = priv->dev->name[4] - '0';
#endif

		remove_proxy_entry(priv, NULL, pskb->data+MACADDRLEN);
		memcpy(cached_mesh_mac[index], &pskb->data[6], 6);
		cached_mesh_dev[index] = pskb->dev;
	}
#endif

#ifdef WDS
	if (pskb->dev->base_addr == 0) {
		if (priv->pmib->dot11WdsInfo.wdsNum > 1)
			cached_wds_dev = NULL;
		else
		{
			memcpy(cached_wds_mac, &pskb->data[6], 6);
			cached_wds_dev = pskb->dev;
		}
	}
#endif

#ifdef CLIENT_MODE
	if (OPMODE & WIFI_STATION_STATE) {
#ifdef CONFIG_RTL_MULTI_REPEATER_MODE_SUPPORT
		if (priv->reperater_idx == 1) {
			memcpy(cached_sta_mac[0], &pskb->data[6], 6);
			cached_sta_dev[0] = pskb->dev;
		} else if ((priv->reperater_idx == 2)) {
			memcpy(cached_sta_mac[1], &pskb->data[6], 6);
			cached_sta_dev[1] = pskb->dev;
		}
#else
		memcpy(cached_sta_mac[0], &pskb->data[6], 6);
		cached_sta_dev[0] = pskb->dev;
#endif

		if (((cached_dev = get_shortcut_dev(pskb->data)) != NULL)
			&& netif_running(cached_dev)
#if defined(CONFIG_RTL_DNS_TRAP)
			&& !(dns_packet)
#endif
#if defined(CONFIG_RTL_HTTP_REDIRECT)
			&& !(http_packet)
#endif
		) {
			pskb->dev = cached_dev;
#if !defined(__LINUX_2_6__) || defined(CONFIG_COMPAT_NET_DEV_OPS)
			cached_dev->hard_start_xmit(pskb, cached_dev);
#else
#if defined(CONFIG_ARCH_RTL8198F) && defined(CONFIG_HWNAT_FLEETCONNTRACK)
			ca_ni_start_xmit_native_from_brsc(pskb, cached_dev);
#else
			cached_dev->netdev_ops->ndo_start_xmit(pskb, cached_dev);
#endif
#endif
			return 1;
		}
	}
	/*AP mode side -> Client mode side*/
	else if (OPMODE & WIFI_AP_STATE) {
		if (TRUE
#ifdef CONFIG_RTL_MULTI_REPEATER_MODE_SUPPORT
			&& priv->reperater_idx
#endif
#if defined(CONFIG_RTL_DNS_TRAP)
			&& !(dns_packet)
#endif
#if defined(CONFIG_RTL_HTTP_REDIRECT)
			&& !(http_packet)
#endif
			) {
#ifdef CONFIG_RTL_MULTI_REPEATER_MODE_SUPPORT
			if (priv->reperater_idx == 1) {
				cached_sta_macPtr = cached_sta_mac[0];
				cached_sta_devPtr = cached_sta_dev[0];
			} else if (priv->reperater_idx == 2) {
				cached_sta_macPtr = cached_sta_mac[1];
				cached_sta_devPtr = cached_sta_dev[1];
			}
#else
			cached_sta_macPtr = cached_sta_mac[0];
			cached_sta_devPtr = cached_sta_dev[0];
#endif
			if ((isEqualMACAddr(cached_sta_macPtr, pskb->data)) &&
				(cached_sta_devPtr != NULL) &&
				netif_running(cached_sta_devPtr)) {
				pskb->dev = cached_sta_devPtr;
#if !defined(__LINUX_2_6__) || defined(CONFIG_COMPAT_NET_DEV_OPS)
				cached_sta_devPtr->hard_start_xmit(pskb, cached_sta_devPtr);
#else
				cached_sta_devPtr->netdev_ops->ndo_start_xmit(pskb,cached_sta_devPtr);
#endif
				return 1;
			}
		}
	}
#endif

#ifdef WDS
	if (pskb->dev->base_addr || (priv->pmib->dot11WdsInfo.wdsNum < 2))
#endif
	{
		if (TRUE
#if defined(CONFIG_DOMAIN_NAME_QUERY_SUPPORT) || defined(CONFIG_RTL_ULINKER)
			&& (pskb->data[37] != 68) /*port 68 is dhcp dest port. In order to hack dns ip, so dhcp packa can't enter bridge short cut.*/
#endif
#if defined(CONFIG_RTL_DNS_TRAP)
			&& !(dns_packet)
#endif
#if defined(CONFIG_RTL_HTTP_REDIRECT)
			&& !(http_packet)
#endif
#if defined(__KERNEL__) || defined(__OSK__)
#ifdef _SINUX_ 
			&& (g_sc_enable_brsc)
#endif
#endif
#ifdef CONFIG_RTL_819X
#if defined(CONFIG_RTL_ULINKER_BRSC)
			&& (((cached_dev = brsc_get_cached_dev(0, pskb->data)) != NULL) || ((cached_dev = get_eth_cached_dev(pskb->data)) != NULL))
#else
			&& (((cached_dev = get_eth_cached_dev(pskb->data)) != NULL) || ((priv->pmib->dot11OperationEntry.block_wlan_relay==0)&&((cached_dev = get_shortcut_dev(pskb->data)) != NULL))) /* AP_2_AP_BRSC */
#endif				
#else
			&& cached_dev
#endif
#ifdef CONFIG_RTK_VLAN_WAN_TAG
			&& rtl865x_same_root(pskb->dev, cached_dev)
#endif
			&& netif_running(cached_dev)
#if defined(CONFIG_RTK_GUEST_ZONE) && defined(__KERNEL__)
			&& (GET_BR_PORT(cached_dev) && GET_BR_PORT(cached_dev)->zone_type == br_port->zone_type)
#endif
			) 
		{
#if defined(CONFIG_RTL_ULINKER_BRSC)
			if (cached_usb.dev && cached_dev == cached_usb.dev) {
				BRSC_COUNTER_UPDATE(tx_wlan_sc);
				BDBG_BRSC("BRSC: get shortcut dev[%s]\n", cached_usb.dev->name);

				if (pskb->dev)
					brsc_cache_dev(1, pskb->dev, pskb->data+ETH_ALEN);
			}
#endif
					
#if defined(SHORTCUT_STATISTIC) /*defined(__ECOS) && defined(_DEBUG_RTL8192CD_)*/
			priv->ext_stats.br_cnt_sc++;
#endif
			pskb->dev = cached_dev;
#ifdef TX_SCATTER
			pskb->list_num = 0;
#endif
#if !defined(__LINUX_2_6__) || defined(CONFIG_COMPAT_NET_DEV_OPS)
			cached_dev->hard_start_xmit(pskb, cached_dev);
#else
#if defined(CONFIG_ARCH_RTL8198F) && defined(CONFIG_HWNAT_FLEETCONNTRACK)
			ca_ni_start_xmit_native_from_brsc(pskb, cached_dev);
#else
			cached_dev->netdev_ops->ndo_start_xmit(pskb,cached_dev);
#endif
#endif
#if defined(CONFIG_RTL_BRSHORTCUT_LINUX_VLAN_CTL)
			statistic_brsc_wlan_xmit_to_eth++;
#endif
			return 1;
		}
	}

	return 0;
}
#endif


#if defined(CONFIG_RTL_EXT_PORT_SUPPORT)
extern int rtl_check_ext_port_napt_entry(uint32 sip, uint16 sport, uint32 dip, uint16 dport, uint8 protocol);
extern unsigned int _br0_ip;
extern unsigned int _br0_mask;
extern int gHwNatEnabled;
extern int extPortEnabled;
extern unsigned int statistic_wlan_xmit_to_eth;
extern unsigned int statistic_wlan_out_napt_exist;
extern unsigned int statistic_wlan_rx_unicast_pkt;
extern int wan_dev_mtu;
struct iphdr * rtl_get_ipv4_header(uint8 *macFrame)
{
	uint8 *ptr;
	struct iphdr *iph=NULL;

	ptr = macFrame + 12;
	if(*(int16 *)(ptr) == (int16)htons(ETH_P_8021Q))
	{
		ptr=ptr+4;
	}

	/*it's not ipv4 packet*/
	if(*(int16 *)(ptr) != (int16)htons(ETH_P_IP))
	{
		return NULL;
	}

	iph=(struct iphdr *)(ptr+2);

	return iph;
}


int rtl_wlan_frame_use_hw_nat(uint8 *data)
{
	struct iphdr *iph=NULL;
	struct tcphdr *tcphupuh;  

	if (data == NULL)
		return FAILED;

	if (gHwNatEnabled&&(0 == (data[0]&0x01))  ) {
		statistic_wlan_rx_unicast_pkt++;
		iph = rtl_get_ipv4_header(data);
		if ((iph != NULL) && ((iph->daddr&_br0_mask)!=(_br0_ip&_br0_mask))) {
			
			//printk("iph->tot_len = %d, wan_dev_mtu = %d\n",htons(iph->tot_len),wan_dev_mtu);
			if(htons(iph->tot_len) > wan_dev_mtu)	
				return FAILED;
			
			if(((ntohs(iph->frag_off))&0x3fff) != 0x0000 )
				return FAILED;
			
			if ((iph->protocol==IPPROTO_TCP) || (iph->protocol==IPPROTO_UDP)) {
				tcphupuh = (struct tcphdr*)((__u32 *)iph + iph->ihl);
				//if ((tcphupuh->fin || tcphupuh->rst || tcphupuh->syn) || ((iph->tot_len-(tcphupuh->doff<<2))==20))
				if ((iph->protocol==IPPROTO_TCP) && (tcphupuh->fin || tcphupuh->rst || tcphupuh->syn))
					return FAILED;
				else if (rtl_check_ext_port_napt_entry(iph->saddr, tcphupuh->source, iph->daddr, tcphupuh->dest, iph->protocol) == 1){
					statistic_wlan_out_napt_exist++;
					return SUCCESS;
				}
			} 
		}
	}

	return FAILED;
}
#endif

#if defined(CLIENT_MODE) && defined(CONFIG_RTL_CUSTOM_PASSTHRU)
#if defined(__ECOS) || defined(RTK_129X_PLATFORM) || defined(__OSK__)
unsigned compare_ether_addr(const u8 *addr1, const u8 *addr2)
{
	const u16 *a = (const u16 *) addr1;
	const u16 *b = (const u16 *) addr2;

	//BUILD_BUG_ON(ETH_ALEN != 6);
	return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2])) != 0;
}
#else 
#if (LINUX_VERSION_CODE >= (KERNEL_VERSION(3,14,0))) && (LINUX_VERSION_CODE < (KERNEL_VERSION(4,4,0)))
unsigned compare_ether_addr(const u8 *addr1, const u8 *addr2)
{
	const u16 *a = (const u16 *) addr1;
	const u16 *b = (const u16 *) addr2;

	//BUILD_BUG_ON(ETH_ALEN != 6);
	return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2])) != 0;
}
#endif
#endif
#endif



#ifdef GBWC
int GBWC_forward_check(struct rtl8192cd_priv *priv, struct sk_buff *pskb, struct stat_info *pstat)
{
	if (priv->pmib->gbwcEntry.GBWCMode && pstat) {
		if (((priv->pmib->gbwcEntry.GBWCMode == GBWC_MODE_LIMIT_MAC_INNER) && (pstat->GBWC_in_group)) ||
			((priv->pmib->gbwcEntry.GBWCMode == GBWC_MODE_LIMIT_MAC_OUTTER) && !(pstat->GBWC_in_group)) ||
			(priv->pmib->gbwcEntry.GBWCMode == GBWC_MODE_LIMIT_IF_RX) ||
			(priv->pmib->gbwcEntry.GBWCMode == GBWC_MODE_LIMIT_IF_TRX)) {
			if ((priv->GBWC_rx_count + pskb->len) > ((priv->pmib->gbwcEntry.GBWCThrd_rx * 1024 / 8) / (1000/GBWC_TO_MILISECS))) {
				// over the bandwidth
				int ret = enque(priv, 
#if defined(AP_SWPS_OFFLOAD)
                        NULL,
#endif				
				        &(priv->GBWC_tx_queue.head), &(priv->GBWC_tx_queue.tail),
						(unsigned long)(priv->GBWC_tx_queue.pSkb), NUM_TXPKT_QUEUE, (void *)pskb);
				if (ret == FALSE) {
					priv->ext_stats.rx_data_drops++;
					DEBUG_ERR("RX DROP: BWC tx queue full!\n");
					dev_kfree_skb_any(pskb);
				}
				else {
#ifdef ENABLE_RTL_SKB_STATS
					rtl_atomic_inc(&priv->rtl_tx_skb_cnt);
#endif
				}
				return 1;
			}
			else {
				// not over the bandwidth
				if (CIRC_CNT(priv->GBWC_tx_queue.head, priv->GBWC_tx_queue.tail, NUM_TXPKT_QUEUE)) {
					// there are already packets in queue, put in queue too for order
					int ret = enque(priv, 
#if defined(AP_SWPS_OFFLOAD)
                            NULL,
#endif
					        &(priv->GBWC_tx_queue.head), &(priv->GBWC_tx_queue.tail),
							(unsigned long)(priv->GBWC_tx_queue.pSkb), NUM_TXPKT_QUEUE, (void *)pskb);
					if (ret == FALSE) {
						priv->ext_stats.rx_data_drops++;
						DEBUG_ERR("RX DROP: BWC tx queue full!\n");
						dev_kfree_skb_any(pskb);
					}
					else {
#ifdef ENABLE_RTL_SKB_STATS
						rtl_atomic_inc(&priv->rtl_tx_skb_cnt);
#endif
					}
					return 1;
				}
				else {
					// can forward directly
					priv->GBWC_rx_count += pskb->len;
				}
			}
		}
	}

	return 0;
}
#endif

#ifdef SBWC
/*static*/ int SBWC_forward_check(struct rtl8192cd_priv *priv, struct sk_buff *pskb, struct stat_info *pstat)
{
	SBWC_MODE mode;
	unsigned int rx_count;
	unsigned int rx_limit;
	int *head, *tail;
	struct sk_buff *pSkb;
	BOOLEAN ret=0;
	int ret_val=0;
	unsigned long flags=0;

	if (!pstat)
		return ret_val;

	if (!(mode & SBWC_MODE_LIMIT_STA_RX))
		return ret_val;

	SMP_LOCK_SBWC(flags);
	mode = pstat->SBWC_mode;
	rx_count = (pstat->SBWC_rx_count + pskb->len);
	rx_limit = pstat->SBWC_rx_limit_byte;

	if (rx_count > rx_limit) 
	{
		// over the bandwidth
		ret = enque(priv, &(pstat->SBWC_rxq.head), &(pstat->SBWC_rxq.tail), (unsigned long)(pstat->SBWC_rxq.pSkb), NUM_TXPKT_QUEUE, (void *)pskb);

		if (ret == FALSE) 
		{
			priv->ext_stats.rx_data_drops++;
			DEBUG_ERR("RX DROP: SBWC rx queue full!\n");
			dev_kfree_skb_any(pskb);
		}
		else 
		{
			#ifdef ENABLE_RTL_SKB_STATS
			rtl_atomic_inc(&priv->rtl_tx_skb_cnt);
			#endif
		}
		ret_val = 1;
	}
	else 
	{
		// not over the bandwidth
		if (CIRC_CNT(pstat->SBWC_rxq.head, pstat->SBWC_rxq.tail, NUM_TXPKT_QUEUE)) 
		{
			// there are already packets in queue, put in queue too for order
			ret = enque(priv, &(pstat->SBWC_rxq.head), &(pstat->SBWC_rxq.tail), (unsigned long)(pstat->SBWC_rxq.pSkb), NUM_TXPKT_QUEUE, (void *)pskb);

			if (ret == FALSE) 
			{
				priv->ext_stats.rx_data_drops++;
				DEBUG_ERR("RX DROP: SBWC rx queue full!\n");
				dev_kfree_skb_any(pskb);
			}
			else 
			{
				#ifdef ENABLE_RTL_SKB_STATS
				rtl_atomic_inc(&priv->rtl_tx_skb_cnt);
				#endif
			}
			ret_val = 1;
		}
		else 
		{
			// can forward directly
			pstat->SBWC_rx_count = rx_count;
		}
	}

	SMP_UNLOCK_SBWC(flags);

	return ret_val;
}
#endif



#if (BEAMFORMING_SUPPORT == 1) //eric-txbf get report api

/*this function is for BFer bug workaround*/
void phydm_8822b_sutxbfer_workaroud(
	IN PVOID		p_dm_void,
	IN BOOLEAN	EnableSUBfer,
	IN u1Byte	Nc,
	IN u1Byte	Nr,
	IN u1Byte	Ng,
	IN u1Byte	CB,
	IN u1Byte	BW,
	IN BOOLEAN	isVHT
	)
{
	struct dm_struct*	p_dm_odm = (struct dm_struct*)p_dm_void;

	if (EnableSUBfer) {
		odm_set_bb_reg(p_dm_odm, 0x19f8, BIT22|BIT21|BIT20, 0x1);
		odm_set_bb_reg(p_dm_odm, 0x19f8, BIT25|BIT24|BIT23, 0x0);
		odm_set_bb_reg(p_dm_odm, 0x19f8, BIT16, 0x1);

		if (isVHT)
			odm_set_bb_reg(p_dm_odm, 0x19f0, BIT5|BIT4|BIT3|BIT2|BIT1|BIT0, 0x1f);
		else
			odm_set_bb_reg(p_dm_odm, 0x19f0, BIT5|BIT4|BIT3|BIT2|BIT1|BIT0, 0x22);
		
		odm_set_bb_reg(p_dm_odm, 0x19f0, BIT7|BIT6, Nc);
		odm_set_bb_reg(p_dm_odm, 0x19f0, BIT9|BIT8, Nr);
		odm_set_bb_reg(p_dm_odm, 0x19f0, BIT11|BIT10, Ng);
		odm_set_bb_reg(p_dm_odm, 0x19f0, BIT13|BIT12, CB);

		odm_set_bb_reg(p_dm_odm, 0xb58, BIT3|BIT2, BW);
		odm_set_bb_reg(p_dm_odm, 0xb58, BIT7|BIT6|BIT5|BIT4, 0x0);
		odm_set_bb_reg(p_dm_odm, 0xb58, BIT9|BIT8, BW);
		odm_set_bb_reg(p_dm_odm, 0xb58, BIT13|BIT12|BIT11|BIT10, 0x0);
	} else
		odm_set_bb_reg(p_dm_odm, 0x19f8, BIT16, 0x0);

	PHYDM_DBG(p_dm_odm, DBG_TXBF, "[%s] EnableSUBfer = %d, isVHT = %d\n", __func__, EnableSUBfer, isVHT);
	PHYDM_DBG(p_dm_odm, DBG_TXBF, "[%s] Nc = %d, Nr = %d, Ng = %d, CB = %d, BW = %d\n", __func__, Nc, Nr, Ng, CB, BW);
	

}

//eric-6s
unsigned char get_sta_nss(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	unsigned char nss = 0;
#ifdef RTK_AC_SUPPORT
	if(pstat->vht_cap_len && (priv->pmib->dot11BssType.net_work_type & WIRELESS_11AC)) {

		if(((HAL_le32_to_cpu(pstat->vht_cap_buf.vht_support_mcs[0])>>8)&3)==3) // no support RX 5ss
			nss = 4;
		if(((HAL_le32_to_cpu(pstat->vht_cap_buf.vht_support_mcs[0])>>6)&3)==3) // no support RX 4ss
			nss = 3;
		if(((HAL_le32_to_cpu(pstat->vht_cap_buf.vht_support_mcs[0])>>4)&3)==3) // no support RX 3ss
			nss = 2;
		if(((HAL_le32_to_cpu(pstat->vht_cap_buf.vht_support_mcs[0])>>2)&3)==3) // no support RX 2ss
			nss = 1;
	}
	else		
#endif		
	if ((priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) && pstat->ht_cap_len) {

		if (pstat->ht_cap_buf.support_mcs[4] == 0) // no support RX 5ss
			nss = 4;
		if (pstat->ht_cap_buf.support_mcs[3] == 0) // no support RX 4ss
			nss = 3;
		if (pstat->ht_cap_buf.support_mcs[2] == 0) // no support RX 3ss
			nss = 2;
		if (pstat->ht_cap_buf.support_mcs[1] == 0) // no support RX 2ss
			nss = 1;	
	}

	return nss;

}

void check_csi_report_info(struct rtl8192cd_priv *priv, unsigned char *pframe, struct rx_frinfo *pfrinfo, unsigned int pktlen)
{
	unsigned char				*pMIMOCtrlField, *pCSIMatrix;
	unsigned char				Idx=0, Nc=0;
	unsigned int				pktType, CSIMatrixLen = 0;   
	unsigned char				csi_nss, sta_nss = 0;
	unsigned char				support_nss = 2;
	unsigned int				i;
	PRT_BEAMFORMING_ENTRY	pBfeeEntry, pEntry; 	
#ifdef CONFIG_VERIWAVE_MU_CHECK
	unsigned char isVerwaveSTA;
#endif

	struct stat_info *pstat = NULL;

	PRT_BEAMFORMING_INFO	pBeamInfo = &(priv->pshare->BeamformingInfo);
	PRT_SOUNDING_INFOV2			pSoundingInfo = &(pBeamInfo->SoundingInfoV2);

#if defined(WDS) || defined(CONFIG_RTK_MESH) || defined(A4_STA)
	if (get_tofr_ds((unsigned char *)get_pframe(pfrinfo)) == 3) {
		pstat = get_stainfo(priv, (unsigned char *)GetAddr2Ptr((unsigned char *)get_pframe(pfrinfo)));
	} else
#endif
	{
#ifdef HW_FILL_MACID
		pstat = get_stainfo_fast(priv,(unsigned char *)get_sa((unsigned char *)get_pframe(pfrinfo)),pfrinfo->macid);	
#else
		pstat = get_stainfo(priv,(unsigned char *)get_sa((unsigned char *)get_pframe(pfrinfo)));
#endif //#ifdef HW_FILL_MACID	  
	}

	if(!pstat)
		return;

	pktType = pframe[24];

	if (pktType == 0x15) { //ACT_PKT_VHT_COMPRESSED_BEAMFORMING
		pMIMOCtrlField = pframe + 26; 
		Nc = ((*pMIMOCtrlField) & 0x7);

	} else	{
		pMIMOCtrlField = pframe + 26; 
		Nc = ((*pMIMOCtrlField) & 0x3);
	}

	sta_nss = get_sta_nss(priv, pstat);

	if((get_rf_mimo_mode(priv) == RF_3T3R) || (get_rf_mimo_mode(priv) == RF_3T4R))
		support_nss = 3;
	else if(get_rf_mimo_mode(priv) == RF_4T4R)
		support_nss = 4;

	if(support_nss < sta_nss)
		csi_nss = support_nss;
	else
		csi_nss = sta_nss;

	pBfeeEntry = Beamforming_GetBFeeEntryByAddr(priv, pstat->cmn_info.mac_addr, &Idx);
	if(!pBfeeEntry)
			return;
	
#ifdef CONFIG_VERIWAVE_MU_CHECK		
	if(!pstat->isVeriwaveInValidSTA) {
		unsigned char invalid_csi[2][3] = {{0x08,0x80,0x00},
										{0x89,0x92,0x28}};
		memcpy(pstat->mu_csi_data, pframe+30, 3);
		isVerwaveSTA = 1;
		for(i=0;i<5;i++) {
			if(memcmp(pstat->mu_csi_data,pframe+33+3*i,3)) {
				isVerwaveSTA = 0;
				break;
			}
		}		

		if(isVerwaveSTA) {
			pstat->isVeriwaveSTA = 1;
			if(pBfeeEntry->is_mu_sta == TXBF_TYPE_MU) {
				priv->pshare->rf_ft_var.isExistVeriwaveSTA = 1;
				priv->pshare->rf_ft_var.qlmt = 200;
				pSoundingInfo->MinSoundingPeriod = 1000;
				
				for(i=0;i<2;i++) {
					if(!memcmp(pstat->mu_csi_data,invalid_csi[i], 3)) {
						pstat->isVeriwaveInValidSTA = i+1;
						odm_set_bb_reg(ODMPTR, 0x1c94, 0xFFFFFFFF, 0xafffafff); //always MU
					}
				}
			}

		}

	}
#endif

	if((Nc+1) != csi_nss){															
#if (MU_BEAMFORMING_SUPPORT == 1)
		if(pBfeeEntry->is_mu_sta == TXBF_TYPE_MU)
			return;
#endif
		//panic_printk("[%s]Error CSI !!! Nc = %d sta_nss = %d, Remove TxBFee Entry\n", Nc, sta_nss);

		pstat->error_csi = 1;
		pstat->csi_nc = Nc;
		
		#if defined (CONFIG_WLAN_HAL_8814AE) || defined (CONFIG_WLAN_HAL_8822BE)
		if (GET_CHIP_VER(priv) == VERSION_8814A || GET_CHIP_VER(priv) == VERSION_8822B)			
			return;
		#endif
		
		//Beamforming_Leave(priv, pstat->cmn_info.mac_addr);				
		pBeamInfo->CurDelBFerBFeeEntrySel = BFeeEntry;
		
		if(Beamforming_DeInitEntry(priv, pstat->cmn_info.mac_addr))
			Beamforming_Notify(priv);
	}
	else
		pstat->error_csi = 0;

}

void set_csi_report_info(struct rtl8192cd_priv *priv, unsigned char *pframe, unsigned int pktlen)
{
	unsigned char				*pMIMOCtrlField, *pCSIMatrix;
	unsigned char				Idx, Nc=0, Nr=0, CH_W=0, Ng=0, CodeBook=0, is_vht=0;
	unsigned int				pktType, CSIMatrixLen = 0;

	struct _RT_BEAMFORMING_INFO* 	pBeamInfo = &(priv->pshare->BeamformingInfo);

	pktType = pframe[24];
	
#if (MU_BEAMFORMING_SUPPORT == 1)	
	if(pBeamInfo->SoundingInfoV2.CandidateMUBFeeCnt){
		
		phydm_8822b_sutxbfer_workaroud(
		ODMPTR,
		bDisable,
		Nc,
		Nr,
		Ng,
		CodeBook,
		CH_W,
		is_vht);

		return;
	}
#endif	
#if 0
	panic_printk("[%s][%d] pframe = 0x%x 0x%x pktType = 0x%x\n", 
		__FUNCTION__, __LINE__, 
		pframe[0], pframe[1], pktType);
#endif

	if (pktType == 0x15) { //ACT_PKT_VHT_COMPRESSED_BEAMFORMING
		pMIMOCtrlField = pframe + 26; 
		Nc = ((*pMIMOCtrlField) & 0x7);
		Nr = (((*pMIMOCtrlField) & 0x38) >> 3);
		CH_W =	(((*pMIMOCtrlField) & 0xC0) >> 6);
		Ng = (*(pMIMOCtrlField+1)) & 0x3;
		CodeBook = ((*(pMIMOCtrlField+1)) & 0x4) >> 2;
		pCSIMatrix = pMIMOCtrlField + 3 + Nc; //24+(1+1+3)+2  MAC header+(Category+ActionCode+MIMOControlField) +SNR(Nc=2)
		CSIMatrixLen = pktlen  - 26 -3 -Nc;
		is_vht = 1;
		//pBeamInfo->TargetCSIInfo.bVHT = TRUE;
	} else  {
		pMIMOCtrlField = pframe + 26; 
		Nc = ((*pMIMOCtrlField) & 0x3);
		Nr =  (((*pMIMOCtrlField) & 0xC) >> 2);
		CH_W =	(((*pMIMOCtrlField) & 0x10) >> 4);
		Ng = ((*pMIMOCtrlField) & 0x60) >> 5;
		CodeBook = ((*(pMIMOCtrlField+1)) & 0x6) >> 1;
		pCSIMatrix = pMIMOCtrlField + 6 + Nr;	//24+(1+1+6)+2	MAC header+(Category+ActionCode+MIMOControlField) +SNR(Nc=2)
		CSIMatrixLen = pktlen  - 26 -6 -Nr;
		is_vht = 0;
		//pBeamInfo->TargetCSIInfo.bVHT = FALSE;
	}

#if 0
	panic_printk("pMIMOCtrlField[0] = 0x%x Nc =%d Nr=%d \n", *pMIMOCtrlField, Nc, Nr);
	panic_printk("CH_W = %d Ng =%d CodeBook=%d \n", CH_W, Ng, CodeBook);
	panic_printk("pCSIMatrix = 0x%x CSIMatrixLen =%d \n", pCSIMatrix, CSIMatrixLen);
#endif

	phydm_8822b_sutxbfer_workaroud(
		ODMPTR,
		bEnable,
		Nc,
		Nr,
		Ng,
		CodeBook,
		CH_W,
		is_vht);
	
}
#endif









#if 0//def DRVMAC_LB
void lb_convert(struct rtl8192cd_priv *priv, unsigned char *pframe)
{
	unsigned char *addr1, *addr2, *addr3;

	if (get_tofr_ds(pframe) == 0x01)
	{
		SetToDs(pframe);
		ClearFrDs(pframe);
		addr1 = GetAddr1Ptr(pframe);
		addr2 = GetAddr2Ptr(pframe);
		addr3 = GetAddr3Ptr(pframe);
		if (addr1[0] & 0x01) {
			memcpy(addr3, addr1, 6);
			memcpy(addr1, addr2, 6);
			memcpy(addr2, priv->pmib->miscEntry.lb_da, 6);
		}
		else {
			memcpy(addr1, addr2, 6);
			memcpy(addr2, priv->pmib->miscEntry.lb_da, 6);
		}
	}
}
#endif


/*
	Strip from "validate_mpdu()"

	0:	no reuse, allocate new skb due to the current is queued.
	1:	reuse! due to error pkt or short pkt.
*/
int rtl8192cd_rx_procCtrlPkt(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo
#ifdef MBSSID
				,int vap_idx
#endif
				)
{
	unsigned char *pframe = get_pframe(pfrinfo);
	struct stat_info *pstat = NULL;

#if defined(SMP_SYNC)
	unsigned long x;
#endif
	if (((GetFrameSubType(pframe)) != WIFI_PSPOLL) ||
			(pfrinfo->to_fr_ds != 0x00))
		return 1;

#ifdef MBSSID
	if (GET_ROOT(priv)->pmib->miscEntry.vap_enable && (vap_idx >= 0))
	{
		priv = priv->pvap_priv[vap_idx];
		if (!(OPMODE & WIFI_AP_STATE))
			return 1;
	}
	else
#endif
	{
		if (!(OPMODE & WIFI_AP_STATE)) {
#ifdef UNIVERSAL_REPEATER
			if (IS_DRV_OPEN(GET_VXD_PRIV(priv)))
				priv = GET_VXD_PRIV(priv);
			else
				return 1;
#else
			return 1;
#endif
		}
	}

	if (!IS_BSSID(priv, GetAddr1Ptr(pframe)))
		return 1;


#ifdef HW_FILL_MACID
    pstat = get_stainfo_fast(priv,GetAddr2Ptr(pframe),pfrinfo->macid);
#else
    pstat = get_stainfo(priv,GetAddr2Ptr(pframe));
#endif //#ifdef HW_FILL_MACID
	// check power save state

	if (pstat != NULL)
	{
        pwr_state(priv, pfrinfo);
	}
	else
		return 1;

#ifdef RTL8190_DIRECT_RX
	rtl8192cd_rx_ctrlframe(priv, NULL, pfrinfo);
#else
	SMP_LOCK_RX_CTRL(x);
	list_add_tail(&(pfrinfo->rx_list), &(priv->rx_ctrllist));
	SMP_UNLOCK_RX_CTRL(x);
	rtl_atomic_inc(&priv->pshare->rx_enqueue_cnt);
#endif
	return 0;
}


void translate_rssi_sq_outsrc(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo, char rate)
{
	struct phydm_phyinfo_struct *pPhyInfo = &pfrinfo->phy_info;
	struct phydm_perpkt_info_struct pktinfo;
	int i;
	unsigned char 		*frame;
	struct stat_info 	*pstat = NULL;
	unsigned char		*addr2;

#if defined(CONFIG_PCI_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
		if (OFFLOAD_ENABLE(priv)) {
			frame = pfrinfo->rxpkt_info->frame_ctrl;
#ifdef CLIENT_MODE
			if (OPMODE & WIFI_STATION_STATE) {
				addr2 = BSSID;
			}else
#endif
			if (pfrinfo->rxpkt_info->pkt_type==RXPKTINFO_TYPE_8023 || pfrinfo->rxpkt_info->pkt_type==RXPKTINFO_TYPE_AMSDU) {
				addr2 = ((unsigned char *)(pfrinfo->pskb->data + pfrinfo->rxbuf_shift + pfrinfo->driver_info_size + ETH_ALEN));
			}
			else
				addr2 = GetAddr2Ptr(frame);
		} else
#endif
		{
			frame = get_pframe(pfrinfo) + (pfrinfo->rxbuf_shift + pfrinfo->driver_info_size);
			addr2 = GetAddr2Ptr(frame);
		}
	}
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
		frame = get_pframe(pfrinfo);
		addr2 = GetAddr2Ptr(frame);
	}
#endif

	//if((GetFrameSubType(frame) & WIFI_DATA_TYPE) == WIFI_DATA_TYPE) {
#ifdef RSSI_MONITOR_NCR
	if(1)
#else
	if(*frame & WIFI_DATA_TYPE) 
#endif
	{
		int i=0;

		//pstat = get_stainfo(priv, GetAddr2Ptr(frame));
		for(i=0; i<NUM_STAT; i++) {
			if (priv->pshare->aidarray[i] && priv->pshare->aidarray[i]->used == TRUE) {
				if(isEqualMACAddr(priv->pshare->aidarray[i]->station.cmn_info.mac_addr, addr2)) {
					pstat =  &(priv->pshare->aidarray[i]->station);
					break;
				}
			}
		}
	}
	
	//struct stat_info 	*pstat = get_stainfo(priv, GetAddr2Ptr(frame));

#ifdef HW_FILL_MACID //eric-8813
    if (IS_SUPPORT_HW_FILL_MACID(priv)) {
        // temp 8814B FPGA
        //pstat = get_HW_mapping_sta(priv,pfrinfo->macid); //this will caue core dump in 8814B, because macid is 0 when sta is not associated, the address cam macid is not updated yet.
        //printk("%s %d HW MACID = %x pstat = %x \n",__FUNCTION__,__LINE__,pfrinfo->macid,pstat);
    }
#endif 

	pktinfo.data_rate = rate;
	pktinfo.is_packet_to_self = 1;
	pktinfo.is_packet_match_bssid = 1;
	pktinfo.station_id = (pstat ? ((u1Byte)(pstat->cmn_info.aid)) : 0);	

#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL_8881A)/* || defined(CONFIG_WLAN_HAL_8814AE)*/
    if ((GET_CHIP_VER(priv) == VERSION_8812E) || (GET_CHIP_VER(priv) == VERSION_8881A)) {
        unsigned short *t = (unsigned short *)(get_pframe(pfrinfo)+2);
        *t = le16_to_cpu(*t);
    }
#endif

	odm_phy_status_query(ODMPTR, pPhyInfo, (u1Byte *)pfrinfo->driver_info, &pktinfo);
#if defined(MP_TEST)
	for (i=0;i<4;i++)
		priv->pshare->mimorssi[i] = pfrinfo->rf_info.mimorssi[i];
#endif

	pfrinfo->rx_bw	  = pPhyInfo->band_width;
}





#ifdef RTK_NL80211 // wrt-adhoc
#ifdef UNIVERSAL_REPEATER
unsigned char is_vxd_bssid(struct rtl8192cd_priv *priv, unsigned char *bssid)
{
	struct rtl8192cd_priv * priv_root = NULL;
	struct rtl8192cd_priv * priv_vxd = NULL;
	unsigned char ret = 0;
	
	if(!IS_VXD_INTERFACE(priv))
	{
		priv_root = GET_ROOT(priv);
		priv_vxd = GET_VXD_PRIV(priv_root);
	}

	if((netif_running(priv_vxd->dev)) && 
		(priv_vxd->pmib->dot11OperationEntry.opmode & WIFI_ADHOC_STATE))
	{
		if(!memcmp(bssid, priv_vxd->pmib->dot11StationConfigEntry.dot11Bssid, 6))
			ret = 1;
	}

	return ret;

}
#endif
#endif

/*
	Check the "to_fr_ds" field:

						FromDS = 0
						ToDS = 0
*/
int rtl8192cd_rx_dispatch_mgmt_adhoc(struct rtl8192cd_priv **priv_p, struct rx_frinfo *pfrinfo
#ifdef MBSSID
				,int vap_idx
#endif
				)
{
	int reuse = 1;
	struct rtl8192cd_priv *priv = *priv_p;
	unsigned int opmode = OPMODE;
	unsigned char *pframe = get_pframe(pfrinfo);
	unsigned int frtype = GetFrameType(pframe);
	unsigned char *da = pfrinfo->da;
	unsigned char *bssid = GetAddr3Ptr(pframe);
	unsigned short frame_type = GetFrameSubType(pframe);
#ifdef MBSSID
	int i;
#endif

#if defined(SMP_SYNC)
	unsigned long x;
#endif
	if ((GetMFrag(pframe)) && (frtype == WIFI_MGT_TYPE))
		goto out;

#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
	// If mgt packet & (beacon or prob-rsp), put in root interface Q
	//		then it will be handled by root & virtual interface
	// If mgt packet & (prob-req), put in AP interface Q
	// If mgt packet & (others), check BSSID (addr3) for matched interface

	if (frtype == WIFI_MGT_TYPE) {
		int vxd_interface_ready=1, vap_interface_ready=0;

#ifdef UNIVERSAL_REPEATER
		if (!IS_DRV_OPEN(GET_VXD_PRIV(priv)) ||
			((opmode & WIFI_STATION_STATE) && !(GET_VXD_PRIV(priv)->drv_state & DRV_STATE_VXD_AP_STARTED)))
			vxd_interface_ready = 0;
#endif

#ifdef MBSSID
//		if (opmode & WIFI_AP_STATE) {
			if (GET_ROOT(priv)->pmib->miscEntry.vap_enable) {
				for (i=0; i<RTL8192CD_NUM_VWLAN; i++) {
					if (IS_DRV_OPEN(priv->pvap_priv[i]))
						vap_interface_ready = 1;
				}
			}
//		}
#endif

		if (!vxd_interface_ready && !vap_interface_ready)
			goto put_in_que;

#if 0
		if (frame_type == WIFI_BEACON || frame_type == WIFI_PROBERSP) {
			pfrinfo->is_br_mgnt = 1;
			goto put_in_que;
		}
#endif

		if (frame_type == WIFI_PROBEREQ) {
#ifdef MBSSID
			if (GET_ROOT(priv)->pmib->miscEntry.vap_enable)
			{
				if (vap_interface_ready) {
					pfrinfo->is_br_mgnt = 1;
					goto put_in_que;
				}
			}
#endif
#ifdef UNIVERSAL_REPEATER
			if (opmode & WIFI_STATION_STATE) {
				if (!vxd_interface_ready) goto out;
				priv = GET_VXD_PRIV(priv);
				opmode = OPMODE;
				goto put_in_que;
			}
#endif
		}
		else { // not (Beacon, Probe-rsp, probe-rsp)
#ifdef MBSSID
			if (GET_ROOT(priv)->pmib->miscEntry.vap_enable)
			{
				if (vap_idx >= 0) {
					if (frame_type == WIFI_BEACON && priv->ss_req_ongoing)
					{
						pfrinfo->is_br_mgnt = 1;
						goto put_in_que;//goto root interface
					}

					priv = priv->pvap_priv[vap_idx];
					opmode = OPMODE;
					goto put_in_que;
				}
			}
#endif
			if (OPMODE & WIFI_AP_STATE) {
				if (frame_type == WIFI_BEACON || frame_type == WIFI_PROBERSP) {
					pfrinfo->is_br_mgnt = 1;
					goto put_in_que;
				}
			}

#ifdef UNIVERSAL_REPEATER
			if (vxd_interface_ready && isEqualMACAddr(GET_VXD_PRIV(priv)->pmib->dot11Bss.bssid, bssid)) {
				priv = GET_VXD_PRIV(priv);
				opmode = OPMODE;
			}
#endif
		}
	}

#ifdef RTK_NL80211 // wrt-adhoc
#ifdef UNIVERSAL_REPEATER
	if (frtype != WIFI_MGT_TYPE)
	if (memcmp(bssid, "\x0\x0\x0\x0\x0\x0", MACADDRLEN) &&
			is_vxd_bssid(priv, bssid))
	{
		unsigned char *sa = pfrinfo->sa;
		
		priv = GET_VXD_PRIV(priv);	
#if 0
		printk(" VXD_ADHOC Rx data packets !! \n");
		dump_mac_rx(da);
		dump_mac_rx(sa);
#endif

#ifdef RTL8190_DIRECT_RX
		rtl8192cd_rx_dataframe(priv, NULL, pfrinfo);
#else
		SMP_LOCK_RX_DATA(x);
		list_add_tail(&(pfrinfo->rx_list), &(priv->rx_datalist));
		SMP_UNLOCK_RX_DATA(x);
		rtl_atomic_inc(&priv->pshare->rx_enqueue_cnt);
#endif
		reuse = 0;

		goto out;
	}
#endif
#endif

put_in_que:
#endif // UNIVERSAL_REPEATER || MBSSID

	if (opmode & WIFI_AP_STATE)
	{
		if (IS_MCAST(da))
		{
			// For AP mode, if DA == MCAST, then BSSID should be also MCAST
			if (IS_MCAST(bssid))
				reuse = 0;

			// support Beacon
			else if (frame_type == WIFI_BEACON)
				reuse = 0;

#ifdef WDS
			else if (priv->pmib->dot11WdsInfo.wdsEnabled)
				reuse = 0;
#endif

#ifdef CONFIG_RTK_MESH
			else if (GET_MIB(priv)->dot1180211sInfo.mesh_enable)
				reuse = 0;
#endif

			else if (opmode & WIFI_SITE_MONITOR)
				reuse = 0;

#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
			else if (pfrinfo->is_br_mgnt && reuse)
				reuse = 0;
#endif

			else
				{}
		}
		else
		{
			/*
			 *	For AP mode, if DA is non-MCAST, then it must be BSSID, and bssid == BSSID
			 *	Action frame is an exception (for bcm iot), do not check bssid
			 */
#ifdef WLAN_DIAGNOSTIC
			if(frame_type == WIFI_PROBEREQ && priv->pmib->dot11StationConfigEntry.probe_info_collecting){
				reuse = 0;	// For customer requirement, receive all Probe request for statistic, unicast case here
//				printk("Receive unicast Probe req da %02X%02X%02X%02X%02X%02X bssid %02X%02X%02X%02X%02X%02X\n", 
//					da[0], da[1], da[2], da[3], da[4], da[5], bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
			}else 
#endif
			if (IS_BSSID(priv, da) && ((IS_BSSID(priv, bssid) || (frame_type == WIFI_WMM_ACTION) || (frame_type == WIFI_PROBEREQ))
#ifdef CONFIG_RTK_MESH 
					|| (GET_MIB(priv)->dot1180211sInfo.mesh_enable
							&& (!memcmp(bssid, pfrinfo->sa, MACADDRLEN)) /*mesh's management frame: A3 = A2 = TA*/
							&& (frtype == WIFI_MGT_TYPE))
#endif
			)) {


				reuse = 0;
}
			else if (opmode & WIFI_SITE_MONITOR)
				reuse = 0;

#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
			else if (pfrinfo->is_br_mgnt && reuse)
				reuse = 0;
#endif
#ifdef WDS
			else if (priv->pmib->dot11WdsInfo.wdsEnabled && priv->pmib->dot11WdsInfo.wdsNum)
				reuse = 0;
#endif

			else
				{}
		}

		if (!reuse) {
			if (frtype == WIFI_MGT_TYPE)
			{
				// check power save state
				if (IS_BSSID(priv, GetAddr1Ptr(pframe))) {
					struct stat_info *pstat = get_stainfo(priv, pfrinfo->sa);
					if (pstat) {
#ifdef HW_DETEC_POWER_STATE
						if (IS_SUPPORT_HW_DETEC_POWER_STATE(priv)) {
							// 8814 power state control only by HW, not by SW.
							// Only if HW detect macid not ready, SW patch this packet
							if(pfrinfo->macid == HW_MACID_SEARCH_NOT_READY)
							{
								printk("%s %d HW_MACID_SEARCH_NOT_READY",__FUNCTION__,__LINE__);
								if(priv->pshare->HWPwrStateUpdate[pstat->cmn_info.aid]==false)   
								{
									printk("%s %d HW not update By SW Aid = %x \n",__FUNCTION__,__LINE__, pstat->cmn_info.aid);
									pwr_state(priv, pfrinfo);
								}
							}
							else if (pfrinfo->macid > HW_MACID_SEARCH_SUPPORT_NUM)
							{
								pwr_state(priv, pfrinfo);
							}
						} else 
#endif // HW_DETEC_POWER_STATE
						{
							pwr_state(priv, pfrinfo);
						}
					}
				}
				
#ifdef RTL8190_DIRECT_RX
	#if defined(CONFIG_PCI_HCI)
				if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
					rtl8192cd_rx_mgntframe(priv, NULL, pfrinfo);
				}
	#endif
	#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
				if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
					notify_recv_mgnt_frame(priv, pfrinfo);
				}
	#endif
#else
				SMP_LOCK_RX_MGT(x);
				list_add_tail(&(pfrinfo->rx_list), &(priv->rx_mgtlist));
				SMP_UNLOCK_RX_MGT(x);
				rtl_atomic_inc(&priv->pshare->rx_enqueue_cnt);
#endif
			}
			else
				reuse = 1;
		}
	}
#ifdef CLIENT_MODE
	else if (opmode & WIFI_STATION_STATE)
	{
		// For Station mode, sa and bssid should always be BSSID, and DA is my mac-address
		// in case of to_fr_ds = 0x00, then it must be mgt frame type

#ifdef MULTI_MAC_CLONE
		if (IS_MCAST(da) || mclone_find_address(priv, da, pfrinfo->pskb, MAC_CLONE_DA_FIND) >= 0)
#else
		unsigned char *myhwaddr = priv->pmib->dot11OperationEntry.hwaddr;
		if (IS_MCAST(da) || isEqualMACAddr(da, myhwaddr))
#endif
			reuse = 0;

		if (!reuse) {
			if (frtype == WIFI_MGT_TYPE)
			{
#ifdef RTL8190_DIRECT_RX
	#if defined(CONFIG_PCI_HCI)
				if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
					rtl8192cd_rx_mgntframe(priv, NULL, pfrinfo);
				}
	#endif
	#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
				if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
					notify_recv_mgnt_frame(priv, pfrinfo);
				}
	#endif
#else
				SMP_LOCK_RX_MGT(x);
				list_add_tail(&(pfrinfo->rx_list), &(priv->rx_mgtlist));
				SMP_UNLOCK_RX_MGT(x);
				rtl_atomic_inc(&priv->pshare->rx_enqueue_cnt);
#endif
			}
			else
				reuse = 1;
		}
	}
	else if (opmode & WIFI_ADHOC_STATE)
	{
		unsigned char *myhwaddr = priv->pmib->dot11OperationEntry.hwaddr;
		if (IS_MCAST(da) || isEqualMACAddr(da, myhwaddr))
		{
			if (frtype == WIFI_MGT_TYPE)
			{
#ifdef RTL8190_DIRECT_RX
	#if defined(CONFIG_PCI_HCI)
				if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
					rtl8192cd_rx_mgntframe(priv, NULL, pfrinfo);
				}
	#endif
	#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
				if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
					notify_recv_mgnt_frame(priv, pfrinfo);
				}
	#endif
#else
				SMP_LOCK_RX_MGT(x);
				list_add_tail(&(pfrinfo->rx_list), &(priv->rx_mgtlist));
				SMP_UNLOCK_RX_MGT(x);
				rtl_atomic_inc(&priv->pshare->rx_enqueue_cnt);
#endif
				reuse = 0;
			}
			else
			{	// data frames
				if (memcmp(bssid, "\x0\x0\x0\x0\x0\x0", MACADDRLEN) &&
						memcmp(BSSID, "\x0\x0\x0\x0\x0\x0", MACADDRLEN) &&
						isEqualMACAddr(bssid, BSSID))
				{
#ifdef RTL8190_DIRECT_RX
					rtl8192cd_rx_dataframe(priv, NULL, pfrinfo);
#else
					SMP_LOCK_RX_DATA(x);
					list_add_tail(&(pfrinfo->rx_list), &(priv->rx_datalist));
					SMP_UNLOCK_RX_DATA(x);
					rtl_atomic_inc(&priv->pshare->rx_enqueue_cnt);
#endif
					reuse = 0;
				}
			}
		}
	}
#endif // CLIENT_MODE
	else
		reuse = 1;

out:

	/* update priv's point */
	*priv_p = priv;
	rx_sum_up(priv, NULL, pfrinfo);

	return reuse;
}


/*
	Check the "to_fr_ds" field:

						FromDS != 0
						ToDS = 0
*/
#if defined(CLIENT_MODE) || defined(CONFIG_RTK_MESH)
int rtl8192cd_rx_dispatch_fromDs(struct rtl8192cd_priv **priv_p, struct rx_frinfo *pfrinfo
#ifdef MBSSID
				,int vap_idx
#endif
)
{
	int reuse = 1;
	struct rtl8192cd_priv *priv = *priv_p;
	unsigned int opmode = OPMODE;
	unsigned char *pframe = get_pframe(pfrinfo);
	unsigned int frtype = GetFrameType(pframe);
	unsigned char *da = pfrinfo->da;
#ifdef MULTI_MAC_CLONE
	unsigned char *myhwaddr = GET_MY_HWADDR;
#else
	unsigned char *myhwaddr = priv->pmib->dot11OperationEntry.hwaddr;
#endif
	unsigned char *bssid = GetAddr2Ptr(pframe);
#if defined(SMP_SYNC)
	unsigned long x;
#endif
#ifdef BSS_MONITOR_SUPPORT
//	collect_monitor_info(priv, pfrinfo);
#endif
#ifdef MULTI_MAC_CLONE
	if(ACTIVE_ID == 0)
		myhwaddr = GET_VXD_PRIV(priv)->pmib->dot11OperationEntry.hwaddr;
#endif
#ifdef MBSSID
	if (GET_ROOT(priv)->pmib->miscEntry.vap_enable && (vap_idx >= 0))
	{
		priv = priv->pvap_priv[vap_idx];
		opmode = OPMODE;
		myhwaddr = GET_MY_HWADDR;
	}
#endif
#ifdef CLIENT_MODE	 //(add for Mesh)
	if (frtype == WIFI_MGT_TYPE)
		goto out;

	if ((opmode & (WIFI_STATION_STATE | WIFI_ASOC_STATE)) ==
				  (WIFI_STATION_STATE | WIFI_ASOC_STATE))
	{
		// For Station mode,
		// da should be for me, bssid should be BSSID
		if (IS_BSSID(priv, bssid)) {
#ifdef A4_STA
			if(priv->pmib->miscEntry.a4_enable) {
				reuse = a4_rx_check_reuse(priv, pfrinfo, myhwaddr);
			}
			else
#endif            
            if (IS_MCAST(da) || isEqualMACAddr(da, myhwaddr)) {
				reuse = 0;
			}

			if(reuse == 0)
			{
#ifdef RTL8190_DIRECT_RX
				rtl8192cd_rx_dataframe(priv, NULL, pfrinfo);
#else
#if defined(__OSK__) && defined(RX_BUFFER_GATHER)
				if (pfrinfo->gather_flag & GATHER_FIRST){
					rtl8192cd_rx_dataframe(priv, NULL, pfrinfo);
				}else
#endif
				{
				SMP_LOCK_RX_DATA(x);
				list_add_tail(&(pfrinfo->rx_list), &(priv->rx_datalist));
				SMP_UNLOCK_RX_DATA(x);
				rtl_atomic_inc(&priv->pshare->rx_enqueue_cnt);
				}
#endif
			}
		}
	}
#ifdef UNIVERSAL_REPEATER
	else if ((opmode & WIFI_AP_STATE) && IS_DRV_OPEN(GET_VXD_PRIV(priv)))
	{
#ifdef SDIO_2_PORT
		myhwaddr = GET_VXD_PRIV(priv)->pmib->dot11OperationEntry.hwaddr;
#endif
		if (IS_BSSID(GET_VXD_PRIV(priv), bssid)) {
#ifdef A4_STA
			if(GET_VXD_PRIV(priv)->pmib->miscEntry.a4_enable) {
				reuse = a4_rx_check_reuse(GET_VXD_PRIV(priv), pfrinfo, myhwaddr);
			}
			else
#endif    
			if (IS_MCAST(da) || isEqualMACAddr(da, myhwaddr)) {
				reuse = 0;
			}

			if (reuse == 0)
			{
				priv = GET_VXD_PRIV(priv);
#ifdef RTL8190_DIRECT_RX
				rtl8192cd_rx_dataframe(priv, NULL, pfrinfo);
#else
#if defined(__OSK__) && defined(RX_BUFFER_GATHER)
				if (pfrinfo->gather_flag & GATHER_FIRST){
					rtl8192cd_rx_dataframe(priv, NULL, pfrinfo);
				}else
#endif
				{
				SMP_LOCK_RX_DATA(x);
				list_add_tail(&(pfrinfo->rx_list), &priv->rx_datalist);
				SMP_UNLOCK_RX_DATA(x);
				rtl_atomic_inc(&priv->pshare->rx_enqueue_cnt);
				}
#endif
			}
		}
	}
#endif
	else
		reuse = 1;

#endif	// CLIENT_MODE (add for Mesh)
out:

	/* update priv's point */
	*priv_p = priv;

#ifdef TRX_DATA_LOG
	pfrinfo->bdata = (reuse ? 0: 1);
#endif
	rx_sum_up(priv, NULL, pfrinfo);


	return reuse;
}
#endif	// defined(CLIENT_MODE) || defined(CONFIG_RTK_MESH)





/*
	Check the "to_fr_ds" field:

						FromDS != 0
						ToDS != 0
	NOTE: The functuion duplicate to rtl8190_rx_dispatch_mesh (mesh_rx.c)
*/
#ifdef WDS
int rtl8192cd_rx_dispatch_wds(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
	int reuse = 1;
	unsigned char *pframe = get_pframe(pfrinfo);
	unsigned int frtype = GetFrameType(pframe);
	struct net_device *dev;
	int fragnum;
#if defined(SMP_SYNC)
	unsigned long x;
#endif


	rx_sum_up(priv, NULL, pfrinfo);

	if (memcmp(GET_MY_HWADDR, GetAddr1Ptr(pframe), MACADDRLEN)) {
		DEBUG_INFO("Rx a WDS packet but which addr1 is not matched own, drop it!\n");
		goto out;
	}

	if (!priv->pmib->dot11WdsInfo.wdsEnabled) {
		DEBUG_ERR("Rx a WDS packet but WDS is not enabled in local mib, drop it!\n");
		goto out;
	}
	dev = getWdsDevByAddr(priv, GetAddr2Ptr(pframe));
	if (dev==NULL) {
#ifdef LAZY_WDS
		if (priv->pmib->dot11WdsInfo.wdsEnabled == WDS_LAZY_ENABLE) {
			if (add_wds_entry(priv, 0, GetAddr2Ptr(pframe))) {
				dev = getWdsDevByAddr(priv, GetAddr2Ptr(pframe));
				if (dev == NULL) {
					DEBUG_ERR("Rx a WDS packet but which TA is not valid, drop it!\n");
					goto out;
				}
				LOG_MSG("Add a wds entry - %02X:%02X:%02X:%02X:%02X:%02X\n",
					*GetAddr2Ptr(pframe), *(GetAddr2Ptr(pframe)+1), *(GetAddr2Ptr(pframe)+2), *(GetAddr2Ptr(pframe)+3),
					*(GetAddr2Ptr(pframe)+4), *(GetAddr2Ptr(pframe)+5));
			}
			else {
				DEBUG_ERR("Rx a WDS packet but wds table is full, drop it!\n");
				goto out;
			}
		}
		else
#endif
		{
					DEBUG_ERR("Rx a WDS packet but which TA is not valid, drop it!\n");
					goto out;
				}
	}
	if (!netif_running(dev)) {
		DEBUG_ERR("Rx a WDS packet but which interface is not up, drop it!\n");
		goto out;
	}
	fragnum = GetFragNum(pframe);
	if (GetMFrag(pframe) || fragnum) {
		DEBUG_ERR("Rx a fragment WDS packet, drop it!\n");
		goto out;
	}
	if (frtype != WIFI_DATA_TYPE) {
		DEBUG_ERR("Rx a WDS packet but which type is not DATA, drop it!\n");
		goto out;
	}
#ifdef __LINUX_2_6__
	pfrinfo->pskb->dev=dev;
#endif

#ifdef RTL8190_DIRECT_RX
	rtl8192cd_rx_dataframe(priv, NULL, pfrinfo);
#else
#if defined(__OSK__) && defined(RX_BUFFER_GATHER)
	if (pfrinfo->gather_flag & GATHER_FIRST){
		rtl8192cd_rx_dataframe(priv, NULL, pfrinfo);
	}else
#endif
	{
	SMP_LOCK_RX_DATA(x);
	list_add_tail(&pfrinfo->rx_list, &priv->rx_datalist);
	SMP_UNLOCK_RX_DATA(x);
	rtl_atomic_inc(&priv->pshare->rx_enqueue_cnt);
	}
#endif
	reuse = 0;
	goto out;

out:

	return reuse;
}
#endif	// WDS

#ifdef SUPPORT_MONITOR
unsigned char getPktChanInfo(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo, unsigned int sub_frame_type)
{
	unsigned char *pframe = get_pframe(pfrinfo);	
	unsigned char channel = 0;
	unsigned char *p = NULL;
	int len=0;

	if(sub_frame_type == WIFI_BEACON) 
		p = get_ie(pframe + WLAN_HDR_A3_LEN + _BEACON_IE_OFFSET_, _DSSET_IE_, &len, pfrinfo->pktlen - WLAN_HDR_A3_LEN - _BEACON_IE_OFFSET_);
	else if(sub_frame_type == WIFI_PROBEREQ) 
		p = get_ie(pframe + WLAN_HDR_A3_LEN + _PROBEREQ_IE_OFFSET_, _DSSET_IE_, &len, pfrinfo->pktlen - WLAN_HDR_A3_LEN - _PROBEREQ_IE_OFFSET_);
	else if(sub_frame_type == WIFI_PROBERSP) 
		p = get_ie(pframe + WLAN_HDR_A3_LEN + _PROBERSP_IE_OFFSET_, _DSSET_IE_, &len, pfrinfo->pktlen - WLAN_HDR_A3_LEN - _PROBERSP_IE_OFFSET_);
	
	if( p != NULL)
		channel = (unsigned char)*(p+2);
	else if(!priv->pmib->miscEntry.chan_switch_disable)
		channel = priv->available_chnl[priv->chan_num];
	else
		channel = priv->pmib->dot11RFEntry.dot11channel;

	//panic_printk("(%s)line=%d, channel = %d, sub_frame_type = %x\n", __FUNCTION__, __LINE__, channel, sub_frame_type);
	return channel;
}

static void rtl_ieee80211_radiotap_header(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo, unsigned int frtype, unsigned int subfrtype, unsigned char chan)
{
	struct rtl_wifi_header *rthdr;
	struct sk_buff *pskb = pfrinfo->pskb;
	unsigned char *pframe = get_pframe(pfrinfo);	//skb data
	unsigned int pkt_len = pfrinfo->pktlen;
	
	rthdr = (void *)skb_push(pskb, sizeof(*rthdr));
	if(!rthdr) {
		panic_printk("(%s)line=%d,!!! failed to allocate memory !!!\n", __FUNCTION__, __LINE__); 
		ASSERT(rthdr);
	}
	memset(rthdr, 0, sizeof(*rthdr));
	
#ifdef CONFIG_PKT_FILTER
	rthdr->rt_pkt_len = pkt_len;
	if(is_qos_data(pframe))
		rthdr->rt_payload_len = pkt_len - WLAN_HDR_A3_QOS_LEN - WLAN_802_2_LEN - WLAN_IP_HDR_LEN - WLAN_UDP_LEN;

	//panic_printk("rt_pkt_len = %d, rt_payload_len = %d\n", rthdr->rt_pkt_len, rthdr->rt_payload_len);
	if(priv->pmib->miscEntry.pkt_filter_len > 0 
	&& priv->pmib->miscEntry.pkt_filter_len <= PTK_LEN_UPPER_BOUND)
		memcpy(rthdr->pkt, pframe, priv->pmib->miscEntry.pkt_filter_len);
#else
	rthdr->rt_sub_frame_type = subfrtype;
	rthdr->rt_rssi = pfrinfo->rf_info.rssi;
	memcpy((rthdr->rt_addr1),GetAddr1Ptr(pframe), MACADDRLEN);
	memcpy((rthdr->rt_addr2),GetAddr2Ptr(pframe), MACADDRLEN);
	rthdr->rt_rate= pfrinfo->rx_rate;
	rthdr->rt_channel_num = chan;
#endif
}
#endif
 






#ifdef CONFIG_PCI_HCI
#if defined(RX_TASKLET) || defined(__ECOS)
__IRAM_IN_865X
void rtl8192cd_rx_tkl_isr(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv;
#if defined(__LINUX_2_6__) || defined(__ECOS)
	unsigned long flags = 0;
#endif
#if defined(SMP_SYNC)
	unsigned long x = 0;
#endif

	priv = (struct rtl8192cd_priv *)task_priv;

//	printk("=====>> INSIDE rtl8192cd_rx_tkl_isr <<=====\n");
#ifdef PCIE_POWER_SAVING_TEST //yllin
    if((priv->pwr_state >= L2) || (priv->pwr_state == L1)) {
            return;
    }
#endif

#ifdef PCIE_POWER_SAVING
	if ((priv->pwr_state == L2) || (priv->pwr_state == L1)) {
		priv->pshare->has_triggered_rx_tasklet = 0;
		return;
	}
#endif

#if defined(__LINUX_2_6__) || defined(__ECOS)
	SMP_LOCK(x);
	//RTL_W32(HIMR, 0);
#else
	SAVE_INT_AND_CLI(x);
	SMP_LOCK(x);

#ifdef DELAY_REFILL_RX_BUF
	priv->pshare->has_triggered_rx_tasklet = 2; // indicate as ISR in service
#endif

#ifdef CONFIG_RTL_88E_SUPPORT
	if (GET_CHIP_VER(priv)==VERSION_8188E) {
		RTL_W32(REG_88E_HIMR, priv->pshare->InterruptMask);
		RTL_W32(REG_88E_HIMRE, priv->pshare->InterruptMaskExt);
	} else
#endif
	{
#ifdef  CONFIG_WLAN_HAL
		if (IS_HAL_CHIP(priv)) {
            GET_HAL_INTERFACE(priv)->EnableRxRelatedInterruptHandler(priv);
		} else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif
		{//not HAL
		//RTL_W32(HIMR, RTL_R32(HIMR) | (HIMR_RXFOVW| HIMR_RDU | HIMR_ROK));
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT) 
		if(GET_CHIP_VER(priv)== VERSION_8812E || GET_CHIP_VER(priv)== VERSION_8723B){
			RTL_W32(REG_92E_HIMR, RTL_R32(REG_92E_HIMR) | HIMR_ROK);
			RTL_W32(REG_92E_HIMRE, RTL_R32(REG_92E_HIMRE) | HIMRE_92E_RXFOVW);
		}
		else
#endif
		{
			RTL_W32(HIMR, RTL_R32(HIMR) | (HIMR_RXFOVW | HIMR_ROK));
		}
		}
	}
#endif

//#ifdef DELAY_REFILL_RX_BUF
//	priv->pshare->has_triggered_rx_tasklet = 2; // indicate as ISR in service
//#endif

	rtl8192cd_rx_isr(priv);

#if defined(__LINUX_2_6__) || defined(__ECOS)

    SAVE_INT_AND_CLI(flags);
    //Filen: "has_triggered_rx_tasklet" & "HIMR Enable" should be atomic
    // To avoid that "RX interrupt" is gone
    priv->pshare->has_triggered_rx_tasklet = 0;

#ifdef CONFIG_RTL_88E_SUPPORT
	if (GET_CHIP_VER(priv)==VERSION_8188E) {
		RTL_W32(REG_88E_HIMR, priv->pshare->InterruptMask);
		RTL_W32(REG_88E_HIMRE, priv->pshare->InterruptMaskExt);
	} else
#endif
	{
#ifdef  CONFIG_WLAN_HAL
		if (IS_HAL_CHIP(priv)) {
        	GET_HAL_INTERFACE(priv)->EnableRxRelatedInterruptHandler(priv);
        } else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif		
		{//not HAL
#if defined(CONFIG_RTL_8812_SUPPORT) 
			if(GET_CHIP_VER(priv) == VERSION_8812E){
				RTL_W32(REG_92E_HIMR, priv->pshare->InterruptMask);
				RTL_W32(REG_92E_HIMRE, priv->pshare->InterruptMaskExt);
			}
			else
#endif
#if defined(CONFIG_RTL_8723B_SUPPORT) 
			if(GET_CHIP_VER(priv) == VERSION_8723B){
				RTL_W32(REG_8723B_HIMR, priv->pshare->InterruptMask);
				RTL_W32(REG_8723B_HIMRE, priv->pshare->InterruptMaskExt);
			}
			else
#endif
		{
			RTL_W32(HIMR, priv->pshare->InterruptMask);
		}
	}
	}

	RESTORE_INT(flags);
	SMP_UNLOCK(x);
#else

	priv->pshare->has_triggered_rx_tasklet = 0;

	RESTORE_INT(x);
	SMP_UNLOCK(x);
#endif
}
#endif
#endif // CONFIG_PCI_HCI

#ifdef CONFIG_WLAN_HAL
#ifdef DELAY_REFILL_RX_BUF
#ifndef __LINUX_2_6__
__MIPS16
#endif
__IRAM_IN_865X
int refill_rx_ring_88XX(
    struct rtl8192cd_priv           *priv, 
    struct sk_buff                  *skb, 
    unsigned char                   *data, 
    unsigned int                    q_num,
    PHCI_RX_DMA_QUEUE_STRUCT_88XX   cur_q
)
{
	int iRet=0;

	/* return 0 means can't refill (because interface be closed or not opened yet) to rx ring but relesae to skb_poll*/	
    if (!(priv->drv_state & DRV_STATE_OPEN)){	
        return 0;   
	}
	struct rtl8192cd_hw *phw=GET_HW(priv);
	struct sk_buff *new_skb;

#if (defined(__LINUX_2_6__) || defined(__ECOS)) && defined(RX_TASKLET)
	unsigned long x;
#endif
	extern struct sk_buff *dev_alloc_8190_skb(unsigned char *data, int size);
#if (defined(__LINUX_2_6__) || defined(__ECOS)) && defined(RX_TASKLET)
	SAVE_INT_AND_CLI(x);
#endif
	if (skb || (priv->pshare->has_triggered_rx_tasklet != 2 &&
                (((cur_q->host_idx + cur_q->rxbd_ok_cnt)%cur_q->total_rxbd_num) != cur_q->cur_host_idx) )  ) {

		if (skb) {
			new_skb = skb;
		} else {
#ifdef CONFIG_RTL8190_PRIV_SKB			
			new_skb = dev_alloc_8190_skb(data, RX_BUF_LEN);
#else
			new_skb = dev_alloc_skb(RX_BUF_LEN);
#endif
			if (new_skb == NULL) {
				DEBUG_ERR("%s:out of skb struct!\n",__func__);
				goto exit;
			}
			
#ifdef ENABLE_RTL_SKB_STATS
			rtl_atomic_inc(&priv->rtl_rx_skb_cnt);
#endif
		}

		{
            GET_HAL_INTERFACE(priv)->UpdateRXBDInfoHandler(priv, q_num,
                (cur_q->host_idx+cur_q->rxbd_ok_cnt)%cur_q->total_rxbd_num, (pu1Byte)new_skb, init_rxdesc_88XX, _FALSE);

            cur_q->rxbd_ok_cnt++;
        }
        
		iRet = 1;
	}

exit:
#if (defined(__LINUX_2_6__) || defined(__ECOS)) && defined(RX_TASKLET)
		RESTORE_INT(x);
#endif

	return iRet;
}
#endif
#endif // CONFIG_WLAN_HAL

#ifdef DELAY_REFILL_RX_BUF
#if (!defined(CONFIG_WLAN_HAL) || CONFIG_WLAN_NOT_HAL_EXIST)
#ifndef __LINUX_2_6__
__MIPS16
#endif
#ifndef __ECOS
__IRAM_IN_865X
#endif
int refill_rx_ring(struct rtl8192cd_priv *priv, struct sk_buff *skb, unsigned char *data)
{
	struct rtl8192cd_hw *phw=GET_HW(priv);
	struct sk_buff *new_skb;
#if (defined(__LINUX_2_6__) || defined(__ECOS)) && defined(RX_TASKLET)
	unsigned long x;
#endif
	int iRet=0;

	extern struct sk_buff *dev_alloc_8190_skb(unsigned char *data, int size);
#if (defined(__LINUX_2_6__) || defined(__ECOS)) && defined(RX_TASKLET)
	SAVE_INT_AND_CLI(x);
#endif
	if (skb ||
			//(priv->pshare->has_triggered_rx_tasklet != 2 && phw->cur_rx_refill != phw->cur_rx)) {
			  phw->cur_rx_refill != phw->cur_rx) {
		if (skb)
			new_skb = skb;
		else {
#ifdef CONFIG_RTL8190_PRIV_SKB				
			new_skb = dev_alloc_8190_skb(data, RX_BUF_LEN);
#else
			new_skb = dev_alloc_skb(RX_BUF_LEN);
#endif
			if (new_skb == NULL) {
				DEBUG_ERR("out of skb struct!\n");
				goto exit;
			}
			
#ifdef ENABLE_RTL_SKB_STATS
			rtl_atomic_inc(&priv->rtl_rx_skb_cnt);
#endif
		}

		init_rxdesc(new_skb, phw->cur_rx_refill, priv);
		//phw->cur_rx_refill = (phw->cur_rx_refill + 1) % NUM_RX_DESC;
		phw->cur_rx_refill = ((phw->cur_rx_refill+1)==NUM_RX_DESC_IF(priv))?0:phw->cur_rx_refill+1;
#ifdef CONFIG_RTL_88E_SUPPORT
		if (GET_CHIP_VER(priv)==VERSION_8188E) {
			RTL_W32(REG_88E_HISR, HIMR_88E_RDU);
			RTL_W32(REG_88E_HISRE, HIMRE_88E_RXFOVW);
		} else
#endif
#ifdef CONFIG_RTL_8812_SUPPORT /*8812_client*/
		if (GET_CHIP_VER(priv)==VERSION_8812E) {
			RTL_W32(REG_HISR0_8812, BIT(1));
			RTL_W32(REG_HISR1_8812, BIT(8));
		} else
#endif
#if defined(CONFIG_RTL_8723B_SUPPORT)	
		if (GET_CHIP_VER(priv)==VERSION_8723B) {
			RTL_W32(REG_8723B_HISR, BIT(1));
			RTL_W32(REG_8723B_HISRE, BIT(8));
		} else
#endif
		{
			RTL_W32(HISR,(HIMR_RXFOVW | HIMR_RDU));
		}

		iRet = 1;
	}

exit:
#if (defined(__LINUX_2_6__) || defined(__ECOS)) && defined(RX_TASKLET)
		RESTORE_INT(x);
#endif
	return iRet;
}
#else
int refill_rx_ring(struct rtl8192cd_priv *priv, struct sk_buff *skb, unsigned char *data)
{
	return 0;
}
#endif // !CONFIG_WLAN_HAL || CONFIG_WLAN_NOT_HAL_EXIST
#endif // DELAY_REFILL_RX_BUF


#ifdef CONFIG_PCI_HCI

#if (defined(__ECOS) ||defined(__LINUX_2_6__)) && defined(RX_TASKLET)
#define	RTL_WLAN_RX_ATOMIC_PROTECT_ENTER	do {SAVE_INT_AND_CLI(x);} while(0)
#define	RTL_WLAN_RX_ATOMIC_PROTECT_EXIT		do {RESTORE_INT(x);} while(0)
#else
#define	RTL_WLAN_RX_ATOMIC_PROTECT_ENTER
#define	RTL_WLAN_RX_ATOMIC_PROTECT_EXIT
#endif


#ifdef CONFIG_WLAN_HAL




#ifdef AP_SWPS_OFFLOAD_WITH_AMPDU_VERI

void
SWPS_RPT_AMPDU_VERI_Handler(
    struct rtl8192cd_priv   *priv, 
    struct rx_frinfo        *pfrinfo
)
{
    pu1Byte	pBuf = get_pframe(pfrinfo);
    u1Byte  bitmap_i,bitmap_val,shift_i,bitmap_valid,handle_macid,qos_case,AMPDU_bitmap_val;

    u8 entry_size,bitmap_size;
    pu1Byte reportbody;
    u8 one_reocrd_size;
    u8 AMPDU_valid;
    unsigned short RPT_DropByID_SEQ[4],RPT_TxDone_SEQ[4],EndSequence[4];
    unsigned char       AMPDU_status[4][8];
    u4Byte dump_i=0;
#if IS_RTL8197G_SERIES
    if(IS_HARDWARE_TYPE_8197G(priv)){
        bitmap_size = SWPSRPT_BITMAP_SIZE_V1;
        reportbody = pBuf+SWPSRPT_BITMAP_SIZE_V1*2; //rxdesc+bitmap+AMPDUbitmap
        entry_size = SWPSRPT_ENTRY_SIZE_V1;
        one_reocrd_size = SWPSRPT_ONE_RECORD_SIZE_V1;
    }
#endif

    handle_macid = 0;

    for(bitmap_i=0;bitmap_i<bitmap_size;bitmap_i++){    
        bitmap_val=*(pBuf+bitmap_i);
        for(shift_i=0;shift_i<8;shift_i++){
            bitmap_valid = (bitmap_val >> shift_i) & BIT(0);
            qos_case = shift_i;
            if(shift_i>3){
                if(shift_i==4){
                    
                    handle_macid++;
                }
                qos_case = shift_i-4;
            }


            if(bitmap_valid !=0x0){//have new report

                     //2 Endian ???
                    //BK, //BE = BK+4 bytes //VI = BK+8 bytes //VO = BK+12 bytes
                    RPT_DropByID_SEQ[qos_case] = *(reportbody+handle_macid*entry_size+qos_case*one_reocrd_size) + ((*(reportbody+handle_macid*entry_size+1+qos_case*one_reocrd_size) & 0xf) << 8);
                    RPT_TxDone_SEQ[qos_case] = ((*(reportbody+handle_macid*entry_size+2+qos_case*one_reocrd_size)) << 4) + ((*(reportbody+handle_macid*entry_size+1+qos_case*one_reocrd_size) & 0xf0) >> 4);
                    
                    
                    AMPDU_bitmap_val = *(pBuf+bitmap_size+bitmap_i);
                    AMPDU_valid = (AMPDU_bitmap_val >> shift_i) & BIT(0);
                    if(AMPDU_valid!=0x0){
                        EndSequence[qos_case] = *(reportbody+handle_macid*entry_size+qos_case*one_reocrd_size+6) + ((*(reportbody+handle_macid*entry_size+qos_case*one_reocrd_size+7) & 0xf) << 8);
                        memcpy(AMPDU_status[qos_case],(reportbody+handle_macid*entry_size+qos_case*one_reocrd_size+8),8);
                    }


                    printk("bitmap=1, handle_macid=%d,qos_case =%d, RPT_TxDone_SEQ[qos_case]=%x,RPT_DropByID_SEQ[qos_case]=%x,EndSequence[qos_case]=%x\n",handle_macid,qos_case,RPT_TxDone_SEQ[qos_case],RPT_DropByID_SEQ[qos_case],EndSequence[qos_case]);//yllinSWPS

                    printk("AMPDU_valid=%x\n",AMPDU_valid);
                    if(AMPDU_valid){
                        if(RPT_TxDone_SEQ[qos_case]!=EndSequence[qos_case] ){
                            u16 start_seq = RPT_TxDone_SEQ[qos_case];
                            
                            if(start_seq!=0xFFF)
                                start_seq = (RPT_TxDone_SEQ[qos_case]+1)&0xFFF;
                            
                            printk("AMPDU status: start from %x to %x\n",start_seq,EndSequence[qos_case] );
                        }
                        else
                            printk("AMPDU status: start from %x to %x\n",RPT_TxDone_SEQ[qos_case],EndSequence[qos_case] );
                        for(dump_i=0;dump_i<8;dump_i++)
                            printk("0x%x ",AMPDU_status[qos_case][dump_i]);
                        printk("\n");
                    }




            }
        }

        handle_macid++;
    }

}
#endif


#if defined(AP_SWPS_OFFLOAD)
void
ReadQinfo(
    IN  HAL_PADAPTER    Adapter, 
    u8 macid,
    u8 pkt_head_or_tail
){
    u32 qinfo;
    u8 Qmacid,Qactype,Qindex;
    u16 pkt_page,txdesc_SWPS_seq,qinfo_reg;
    u32 switchHaddr,switchLaddr;
    u8 qi;
    u16 Qempty;
    
    struct stat_info * pstat = get_stainfo_fast(Adapter,NULL,macid);
    if(pstat != NULL){
#ifdef AP_SWPS_OFFLOAD_VERI        
        printk("-------------ReadQinfo--------------\n");        
#endif
    //2 use reg number instead of naming due to 'undecalred error'
        Qempty = HAL_RTL_R16(0x410);
        for(qi=0;qi<NUM_AC_QUEUE;qi++){//16 ACQ
            if(!(Qempty & 0x1)){
                HAL_RTL_W8(0x40c,qi);
                qinfo = HAL_RTL_R32(0x400);
                if(!(qinfo & BIT22)){
                    Qmacid = (qinfo >> 25) & 0x7F;
                    Qactype = (qinfo >> 23) & 0x3;
                    if(Qactype==0)//BK
                        Qindex = 3;
                    else if(Qactype==1)
                        Qindex = 2;
                    else if(Qactype==2)
                        Qindex = 1;
                    else if(Qactype==3)//VO
                        Qindex = 0;
                    
                    if(Qmacid == macid){
                        if(pkt_head_or_tail==1)
                            pkt_page = (qinfo >> 0) & 0x7ff;
                        else
                            pkt_page = (qinfo >> 11) & 0x7ff;
                        switchHaddr = (pkt_page * HAL_PAGE_SIZE) / 0x1000;
                        switchLaddr = (pkt_page * HAL_PAGE_SIZE) % 0x1000;
                        HAL_RTL_W32(0x140, 0x780+switchHaddr);
                        txdesc_SWPS_seq = HAL_RTL_R16(0x8000+switchLaddr+24) & 0xFFF; //get TXDESC SWPS seq
                        if(Qindex > 3)
                            printk("[%s][%d]wrong AC number\n", __FUNCTION__,__LINE__);//debug
                        else{    
                            if(pkt_head_or_tail==0)//to record last packet in AC queue
                                pstat->SWPS_last_seq[Qindex] = txdesc_SWPS_seq;
                        }
#ifdef AP_SWPS_OFFLOAD_VERI                    
                        printk("AC = %d, RPT_reprepare_SEQ = 0x%x, in ACQ txdesc SWPS = 0x%x, headortail = %d\n",Qindex,pstat->RPT_reprepare_SEQ[Qindex],txdesc_SWPS_seq,pkt_head_or_tail);
#endif
                    }
                }

                
            }
            Qempty = Qempty >> 1;
        }
        
#ifdef AP_SWPS_OFFLOAD_VERI        
        printk("-------------finish--------------\n");
#endif
    }
}

void
Process_SWPSRPT(
    struct stat_info * pstat,
    u1Byte PSflag,
    u1Byte qos_case,
    u1Byte * pneed_recycle,
    u4Byte getRPT_TSF

)
{
    u1Byte dropid_i;
    u2Byte reprepare_seq;

                    if(PSflag && (pstat->RPT_PSflag==0)){ //after this client goto sleep, first time get report 

                        //init, clear to 0
                        pstat->RPT_reprepare_SEQ[qos_case]=0;
                        pstat->SWPS_last_seq[qos_case]=0; 
                        pstat->prepare_done[qos_case]=0;

                        
                        *pneed_recycle = 1;
                        if(pstat->RPT_TxDone_SEQ[qos_case] == MAX_SWPS_SEQ) //max=4095
                            reprepare_seq = 1;
                        else
                            reprepare_seq = (pstat->RPT_TxDone_SEQ[qos_case]+1) % (MAX_SWPS_SEQ+1);

                        if(pstat->SWPS_seq[qos_case]!=reprepare_seq)
                            pstat->RPT_reprepare_SEQ[qos_case] = reprepare_seq;
                        else{
                            //printk("macid %d AC %d has no pkt need to reprepare.\n",handle_macid,qos_case);//debug
                        }
                            
#ifdef  AP_SWPS_OFFLOAD_VERI
                        //ReadQinfo(priv,handle_macid,0);
                        //ReadQinfo(priv,handle_macid,1);
#else
                        //ReadQinfo(priv,handle_macid,0); //need to read the last pkt //--> move to sleep INT, need to check if INT is faster than report or vice versa

#endif

#if 0 //use sw maintained swps_seq as reference of the last seq which need to be reprepared
                        if((pstat->SWPS_last_seq[qos_case]==0) && (pstat->SWPS_seq[qos_case]!=reprepare_seq)){ //for the case that pkt are all droped done when read Qinfo, so we get no last pkt seq
                            //assume swps report has the last seq //need check if any corner case cause bugs....
                            pstat->SWPS_last_seq[qos_case]=pstat->RPT_DropByID_SEQ[qos_case];
                        }
#else   
                        //should already stop tx to this STA, so the last seq = SWPS_seq -1
                        if(pstat->SWPS_seq[qos_case]!=reprepare_seq)
{
                            if(pstat->SWPS_seq[qos_case]==1)
                                pstat->SWPS_last_seq[qos_case]= MAX_SWPS_SEQ;
                            else
                            pstat->SWPS_last_seq[qos_case]=pstat->SWPS_seq[qos_case]-1;
                            
                        }
#endif
                        if(pstat->SWPS_last_seq[qos_case]!=0){
#if 0  //if use RPT_DropByID_SEQ, will need this section to check if it is for this time update, or it is old info                                                          
                            if(pstat->SWPS_last_seq[qos_case] >= pstat->RPT_reprepare_SEQ[qos_case]){

                                if((pstat->SWPS_last_seq[qos_case] - pstat->RPT_reprepare_SEQ[qos_case]) > (MAX_SWPS_SEQ/2)){
                                    pstat->SWPS_last_seq[qos_case] = 0; //ex. seq=3, drop=4000 , then drop seq in this report is not pkt droped after this pwr=1
                                    //printk("report drop seq >>> swps seq, no pkt to prepare, set last seq = 0....\n");//debug    
                                    pstat->reprepare_num[qos_case]=0;
                                    pstat->SWPS_last_seq[qos_case]=0;
                                }
                                else                                        
                                    pstat->reprepare_num[qos_case] = pstat->SWPS_last_seq[qos_case] - pstat->RPT_reprepare_SEQ[qos_case] + 1;
                            }
                            else{
                                if((pstat->RPT_reprepare_SEQ[qos_case] - pstat->SWPS_last_seq[qos_case]) > (MAX_SWPS_SEQ/2))
                                    pstat->reprepare_num[qos_case] = MAX_SWPS_SEQ - pstat->RPT_reprepare_SEQ[qos_case] + pstat->SWPS_last_seq[qos_case] + 1;
                                else{
                                    //ex. seq=10,drop=3, then drop seq in this report is not pkt droped after this pwr=1
                                    pstat->reprepare_num[qos_case]=0;
                                    pstat->SWPS_last_seq[qos_case]=0;
                                }
                            }
#else
                            if(pstat->SWPS_last_seq[qos_case] >= pstat->RPT_reprepare_SEQ[qos_case]){
                                pstat->reprepare_num[qos_case] = pstat->SWPS_last_seq[qos_case] - pstat->RPT_reprepare_SEQ[qos_case] + 1;
                            }else{
                                pstat->reprepare_num[qos_case] = MAX_SWPS_SEQ - pstat->RPT_reprepare_SEQ[qos_case] + pstat->SWPS_last_seq[qos_case] + 1;
                            }
#endif                            
#ifdef AP_SWPS_DEBUG_PRINT
                            printk("pstat->reprepare_num[%d]=%x\n",qos_case,pstat->reprepare_num[qos_case]); //yllinSWPS
                            printk("pstat->SWPS_last_seq[%d]=%x\n",qos_case,pstat->SWPS_last_seq[qos_case]);
                            //printk("pstat->RPT_reprepare_SEQ[%d]=%x\n",qos_case,pstat->RPT_reprepare_SEQ[qos_case]);
                            
#endif                          
#ifdef AP_SWPS_VERI_ANALYZE
                            printk("pkt num in TXBUF beore sleep drop 0x1a6 = 0x%x\n",RTL_R8(0x1a6));
                            pstat->drop_pkt_num = RTL_R8(0x1a6);

                            if((pstat->drop_pkt_num > 0) && (qos_case == 1) && (pstat->sleep_ary_idx < 100)){
                                pstat->TSF_record_flag = 1;
                                pstat->DropbyID_start[pstat->sleep_ary_idx]=pstat->SWPS_last_seq[qos_case];
                                //printk("agree to fill c2h sleep_times=%x\n",pstat->Sleep_times);
                            }
                            else{
                                //printk("get report, not fill table. sleep_times=%x\n",pstat->Sleep_times);
                            }
#endif

                        }else{
                            //printk("last seq = 0....maybe RPT is faster than INT, or just no packet to prepare\n");//yllinSWPS
#ifdef AP_SWPS_VERI_ANALYZE
                            pstat->drop_pkt_num = RTL_R8(0x1a6);
#endif
                        }
                        
                    
                        if(pstat->get_dropid_flag){
                            if(pstat->Drop_ID == 0)
                                dropid_i = 3;
                            else
                                dropid_i = pstat->Drop_ID-1;
                            pstat->DropDone_seq[qos_case][dropid_i] = pstat->SWPS_last_seq[qos_case];
                            #ifdef AP_SWPS_VERI_ANALYZE                             
                            pstat->record_drop_afterc2h_cnt++;
                            #endif
                        }else{
                            dropid_i = pstat->Drop_ID;
                            pstat->DropDone_seq[qos_case][dropid_i] = pstat->SWPS_last_seq[qos_case];
                            #ifdef AP_SWPS_VERI_ANALYZE
                            pstat->record_drop_beforec2h_cnt++;
                            #endif
                        }


                        
                    }
                    
                    //if wakeup then goto sleep, between get no report, then pstat->RPT_PSflag will not update to 0
                    //still need, because driver can loss NULL(1)(0), so clear RPT_PSflag will not happen when loss
                    if(PSflag==0){
                        pstat->RPT_PSflag = 0;
                        pstat->PS_get_SWPSRPT = 0;
                        #ifdef AP_SWPS_VERI_ANALYZE                    

                            //printk("MACID = %x, sleeptimes = %x, RPT_DropByID_SEQ[%x] = %x, TSF = 0x%x\n",handle_macid,pstat->Sleep_times,qos_case,pstat->RPT_DropByID_SEQ[qos_case],getRPT_TSF);
                            if(qos_case == 1){
                                u8 find_i;
                                for(find_i = 0;find_i<100;find_i++){
                                    if((pstat->sleeptimes_idx[find_i]==pstat->Sleep_times) && (pstat->Sleep_times!=0) && (pstat->record_finish[find_i]!=1)){
                                        pstat->driverTSF[find_i]=getRPT_TSF;
                                        pstat->DropbyID_result[find_i]=pstat->RPT_DropByID_SEQ[qos_case];
                                        pstat->record_finish[find_i]=1;

                                    }
                                }
                            }

                        #endif
                    }
                    #ifdef AP_SWPS_VERI_ANALYZE                    

                    if(qos_case == 1){
                        u8 find_i;
                        for(find_i = 0;find_i<100;find_i++){
                            if((pstat->sleeptimes_idx[find_i]==pstat->Sleep_times) && (pstat->Sleep_times!=0) && (pstat->record_finish[find_i]!=1) && (pstat->RPT_DropByID_SEQ[qos_case]==pstat->DropbyID_start[find_i])){
                                pstat->driverTSF[find_i]=getRPT_TSF;
                                pstat->DropbyID_result[find_i]=pstat->RPT_DropByID_SEQ[qos_case];
                                pstat->record_finish[find_i]=1;

                            }
                        }
                    }
                    #endif

}
void
SWPS_RPT_Handler(
    struct rtl8192cd_priv   *priv, 
    struct rx_frinfo        *pfrinfo
)
{

    pu1Byte	pBuf = get_pframe(pfrinfo);
    u1Byte	length = pfrinfo->pktlen;
    u1Byte  bitmap_i,bitmap_val,shift_i,bitmap_valid,handle_macid,qos_case,AMPDU_bitmap_val;
    pu1Byte reportbody;
    struct stat_info * pstat;
    u1Byte PSflag;
    u2Byte reprepare_seq;
    u4Byte dump_i=0;
    u1Byte need_recycle = 0;
    u1Byte recycle_idx;
    u1Byte recycle_queue[4]={0};
    unsigned char qos_i = 0;
    u1Byte dropid_i;
    u4Byte getRPT_TSF;
    u8 entry_size,bitmap_size;
    u8 one_reocrd_size;
    u8 AMPDU_valid;
#ifdef AP_SWPS_VERI_ANALYZE      
    
    getRPT_TSF = RTL_R32(0x568);
#endif    

#if IS_RTL8198F_SERIES || IS_RTL8814B_SERIES
if(IS_HARDWARE_TYPE_8198F(priv) || IS_HARDWARE_TYPE_8814B(priv)){
    bitmap_size = SWPSRPT_BITMAP_SIZE;
    reportbody = pBuf+SWPSRPT_BITMAP_SIZE; //rxdesc+bitmap
    entry_size = SWPSRPT_ENTRY_SIZE;
    one_reocrd_size = SWPSRPT_ONE_RECORD_SIZE;
}
#endif
#if IS_RTL8197G_SERIES
if(IS_HARDWARE_TYPE_8197G(priv)){
    bitmap_size = SWPSRPT_BITMAP_SIZE_V1;
    reportbody = pBuf+SWPSRPT_BITMAP_SIZE_V1*2; //rxdesc+bitmap+AMPDUbitmap
    entry_size = SWPSRPT_ENTRY_SIZE_V1;
    one_reocrd_size = SWPSRPT_ONE_RECORD_SIZE_V1;
}
#endif

#if 0    
    printk("dump rpt:");
    //for(dump_i=0;dump_i<2138;dump_i++)
    for(dump_i=0;dump_i<256;dump_i++)
    {
        if(dump_i%16==0)
            printk("\n");
        printk("%x ",*(pBuf+dump_i));
        
    }
        printk("\n");
#endif        
    handle_macid = 0;
    
    for(bitmap_i=0;bitmap_i<bitmap_size;bitmap_i++){    
        bitmap_val=*(pBuf+bitmap_i);
        for(shift_i=0;shift_i<8;shift_i++){
            bitmap_valid = (bitmap_val >> shift_i) & BIT(0);
            qos_case = shift_i;
            if(shift_i>3){
                if(shift_i==4){
                    if(pstat){
                        
                        if(PSflag && (pstat->RPT_PSflag==0)){//to update flag before next macid
                            pstat->PS_get_SWPSRPT = 1;
                            pstat->RPT_PSflag = 1;
                        }
                        
                    }
                    //else
                     //printk("[%s][%d] no STA info!!! bitmap_i= %x\n", __FUNCTION__,__LINE__,bitmap_i);
                    
                    handle_macid++;
                }
                qos_case = shift_i-4;
            }

            if(handle_macid > 0)
                pstat = get_stainfo_fast(priv,NULL,handle_macid); //&(priv->pshare->aidarray[handle_macid-1]->station);

            if(bitmap_valid !=0x0){//have new report

                if(pstat){

                    recycle_queue[qos_case] = 1;
                    
                     //2 Endian ???
                    //BK, //BE = BK+4 bytes //VI = BK+8 bytes //VO = BK+12 bytes
                    pstat->RPT_DropByID_SEQ[qos_case] = *(reportbody+handle_macid*entry_size+qos_case*one_reocrd_size) + ((*(reportbody+handle_macid*entry_size+1+qos_case*one_reocrd_size) & 0xf) << 8);
                    pstat->RPT_TxDone_SEQ[qos_case] = ((*(reportbody+handle_macid*entry_size+2+qos_case*one_reocrd_size)) << 4) + ((*(reportbody+handle_macid*entry_size+1+qos_case*one_reocrd_size) & 0xf0) >> 4);
                    pstat->RPT_DropBit = (*(reportbody+handle_macid*entry_size+3+qos_case*one_reocrd_size) & (BIT0|BIT1|BIT2|BIT3));
                    PSflag = (*(reportbody+handle_macid*entry_size+3+qos_case*one_reocrd_size) & BIT7) >> 7;
                    #ifdef AP_SWPS_OFFLOAD_WITH_AMPDU
                    AMPDU_bitmap_val = *(pBuf+bitmap_size+bitmap_i);
                    AMPDU_valid = (AMPDU_bitmap_val >> shift_i) & BIT(0);
                    if(AMPDU_valid!=0x0){
                        pstat->EndSequence[qos_case] = *(reportbody+handle_macid*entry_size+qos_case*one_reocrd_size+6) + ((*(reportbody+handle_macid*entry_size+qos_case*one_reocrd_size+7) & 0xf) << 8);
                        memcpy(pstat->AMPDU_status[qos_case],(reportbody+handle_macid*entry_size+qos_case*one_reocrd_size+8),8);
                    }
                    #endif
#ifdef AP_SWPS_DEBUG_PRINT
                    printk("bitmap=1, PSflag=%x, handle_macid=%d, pstat->RPT_TxDone_SEQ[%d]=%x,pstat->RPT_DropByID_SEQ[qos_case]=%x,pstat->RPT_DropBit=%x\n",PSflag,handle_macid,qos_case,pstat->RPT_TxDone_SEQ[qos_case],pstat->RPT_DropByID_SEQ[qos_case],pstat->RPT_DropBit);//yllinSWPS
                    #ifdef AP_SWPS_OFFLOAD_WITH_AMPDU
                    printk("AMPDU_valid=%x\n",AMPDU_valid);
                    if(AMPDU_valid){
                        if(pstat->RPT_TxDone_SEQ[qos_case]!=pstat->EndSequence[qos_case] )
                            printk("AMPDU status: start from %x to %x\n",(pstat->RPT_TxDone_SEQ[qos_case]+1)&0xFFF,pstat->EndSequence[qos_case] );
                        else
                            printk("AMPDU status: start from %x to %x\n",pstat->RPT_TxDone_SEQ[qos_case],pstat->EndSequence[qos_case] );
                        for(dump_i=0;dump_i<8;dump_i++)
                            printk("0x%x ",pstat->AMPDU_status[qos_case][dump_i]);
                        printk("\n");
                    }
                    #endif
#endif
                    Process_SWPSRPT(pstat,PSflag,qos_case,&need_recycle,getRPT_TSF);
                }
                else
                    printk("[%s][%d]get STA info fail!!!\n", __FUNCTION__,__LINE__);
            }
        }

        if(pstat){
            if(PSflag && (pstat->RPT_PSflag==0)){//to update flag before next macid
                pstat->PS_get_SWPSRPT = 1;
                pstat->RPT_PSflag = 1;
            }
        }

        handle_macid++;
    }
    if(need_recycle){
        for(recycle_idx = 0; recycle_idx <4; recycle_idx++){
            if(recycle_queue[recycle_idx]){
                priv->swps_recycle_queue[recycle_idx]=1; 
            }
        }
#if defined(__KERNEL__)
        if (!priv->pshare->has_triggered_forcerecycle_tasklet){
            tasklet_hi_schedule(&priv->pshare->forcerecycle_tasklet);
            priv->pshare->has_triggered_forcerecycle_tasklet = 1;
        }
#endif        
    }   

}
#endif


#define CFG_UPDATE_RX_SWBD_IDX_EARLY    1

#endif // CONFIG_WLAN_HAL

#undef	RTL_WLAN_RX_ATOMIC_PROTECT_ENTER
#undef	RTL_WLAN_RX_ATOMIC_PROTECT_EXIT

#endif // CONFIG_PCI_HCI


void SendQosNullData(struct rtl8192cd_priv *priv, unsigned char *da)
{
	struct wifi_mib *pmib;
	unsigned char *hwaddr;
	unsigned char tempQosControl[2];
	DECLARE_TXINSN(txinsn);

	txinsn.retry = priv->pmib->dot11OperationEntry.dot11ShortRetryLimit;

	pmib = GET_MIB(priv);

	hwaddr = pmib->dot11OperationEntry.hwaddr;

	txinsn.q_num = MANAGE_QUE_NUM;
	txinsn.tx_rate = find_rate(priv, NULL, 0, 1);
	txinsn.fixed_rate = 1;
	txinsn.fr_type = _PRE_ALLOCHDR_;
	txinsn.phdr = get_wlanhdr_from_poll(priv);
	txinsn.pframe = NULL;

	if (txinsn.phdr == NULL)
		goto send_qos_null_fail;

	memset((void *)(txinsn.phdr), 0, sizeof (struct	wlan_hdr));

	SetFrameSubType(txinsn.phdr, BIT(7) | WIFI_DATA_NULL);
	SetFrDs(txinsn.phdr);
	memcpy((void *)GetAddr1Ptr((txinsn.phdr)), da, MACADDRLEN);
	memcpy((void *)GetAddr2Ptr((txinsn.phdr)), hwaddr, MACADDRLEN);
	memcpy((void *)GetAddr3Ptr((txinsn.phdr)), hwaddr, MACADDRLEN);
	txinsn.hdr_len = WLAN_HDR_A3_QOS_LEN;

	memset(tempQosControl, 0, 2);
	tempQosControl[0] = 0x07;		//set priority to VO
	tempQosControl[0] |= BIT(4);	//set EOSP
	memcpy((void *)GetQosControl((txinsn.phdr)), tempQosControl, 2);

	if ((rtl8192cd_firetx(priv, &txinsn)) == SUCCESS)
		return;

send_qos_null_fail:

	if (txinsn.phdr)
		release_wlanhdr_to_poll(priv, txinsn.phdr);
}



#ifdef CONFIG_PCI_HCI
void process_APSD_dz_queue_pci(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned short tid)
{
	unsigned int deque_level = 1;		// deque pkts level, VO = 4, VI = 3, BE = 2, BK = 1
	struct sk_buff *pskb = NULL;
	int round = 0;
#if defined(AP_SWPS_OFFLOAD)
    struct reprepare_info pkt_info;
    unsigned int queueIndex;
#endif
    
	DECLARE_TXINSN(txinsn);

	if ((((tid == 7) || (tid == 6)) && !(pstat->apsd_bitmap & 0x01))
		|| (((tid == 5) || (tid == 4)) && !(pstat->apsd_bitmap & 0x02))
		|| (((tid == 3) || (tid == 0)) && !(pstat->apsd_bitmap & 0x08))
		|| (((tid == 2) || (tid == 1)) && !(pstat->apsd_bitmap & 0x04))) {
		DEBUG_INFO("RcvQosNull legacy ps tid=%d\n", tid);
		return;
	}
#if defined(AP_SWPS_OFFLOAD)
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv)) {
        if(pstat->process_dz_ok == pstat->next_prepare_ok){
            //prepare done, proceed
        }else
            return;
    }
#endif

	if (pstat->apsd_pkt_buffering == 0)
		goto sendQosNull;

	if ((pstat->apsd_bitmap & 0x01) && (!isFFempty(pstat->VO_dz_queue->head, pstat->VO_dz_queue->tail)))
		deque_level = 4;
	else if ((pstat->apsd_bitmap & 0x02) && (!isFFempty(pstat->VI_dz_queue->head, pstat->VI_dz_queue->tail)))
		deque_level = 3;
	else if ((pstat->apsd_bitmap & 0x08) && (!isFFempty(pstat->BE_dz_queue->head, pstat->BE_dz_queue->tail)))
		deque_level = 2;
	else if ((!(pstat->apsd_bitmap & 0x04)) || (isFFempty(pstat->BK_dz_queue->head, pstat->BK_dz_queue->tail))) {
		//send QoS Null packet
sendQosNull:
		SendQosNullData(priv, pstat->cmn_info.mac_addr);
		DEBUG_INFO("sendQosNull  tid=%d\n", tid);
		return;
	}

	while (1) {
		if (deque_level == 4) {
			pskb = (struct sk_buff *)deque(priv, 
#if defined(AP_SWPS_OFFLOAD)
                    pstat->VO_dz_queue,
                    &pkt_info,
#endif
                    &(pstat->VO_dz_queue->head), &(pstat->VO_dz_queue->tail),
					(unsigned long)(pstat->VO_dz_queue->pSkb), NUM_APSD_TXPKT_QUEUE);
#if defined(AP_SWPS_OFFLOAD)
            queueIndex = VO_QUEUE; // 4
#endif
			if (pskb == NULL) {
				if ((pstat->apsd_bitmap & 0x02) && (!isFFempty(pstat->VI_dz_queue->head, pstat->VI_dz_queue->tail)))
					deque_level--;
				else if ((pstat->apsd_bitmap & 0x08) && (!isFFempty(pstat->BE_dz_queue->head, pstat->BE_dz_queue->tail)))
					deque_level = 2;
				else if ((pstat->apsd_bitmap & 0x04) && (!isFFempty(pstat->BK_dz_queue->head, pstat->BK_dz_queue->tail)))
					deque_level = 1;
				else
					deque_level = 0;
			}
			else {
				DEBUG_INFO("deque VO pkt\n");
			}
		}
		else if (deque_level == 3) {
			pskb = (struct sk_buff *)deque(priv, 
#if defined(AP_SWPS_OFFLOAD)
                    pstat->VI_dz_queue,
                    &pkt_info,
#endif                                
                    &(pstat->VI_dz_queue->head), &(pstat->VI_dz_queue->tail),
					(unsigned long)(pstat->VI_dz_queue->pSkb), NUM_APSD_TXPKT_QUEUE);
#if defined(AP_SWPS_OFFLOAD)
            queueIndex = VI_QUEUE; // 3
#endif            
			if (pskb == NULL) {
				if ((pstat->apsd_bitmap & 0x08) && (!isFFempty(pstat->BE_dz_queue->head, pstat->BE_dz_queue->tail)))
					deque_level--;
				else if ((pstat->apsd_bitmap & 0x04) && (!isFFempty(pstat->BK_dz_queue->head, pstat->BK_dz_queue->tail)))
					deque_level = 1;
				else
					deque_level = 0;
			}
			else {
				DEBUG_INFO("deque VI pkt\n");
			}
		}
		else if (deque_level == 2) {
			pskb = (struct sk_buff *)deque(priv, 
#if defined(AP_SWPS_OFFLOAD)
                    pstat->BE_dz_queue,
                    &pkt_info,
#endif 
                    &(pstat->BE_dz_queue->head), &(pstat->BE_dz_queue->tail),
					(unsigned long)(pstat->BE_dz_queue->pSkb), NUM_APSD_TXPKT_QUEUE);
#if defined(AP_SWPS_OFFLOAD)
            queueIndex = BE_QUEUE; // 2
#endif
			if (pskb == NULL) {
				if ((pstat->apsd_bitmap & 0x04) && (!isFFempty(pstat->BK_dz_queue->head, pstat->BK_dz_queue->tail)))
					deque_level--;
				else
					deque_level = 0;
			}
			else {
				DEBUG_INFO("deque BE pkt\n");
			}
		}
		else if (deque_level == 1) {
			pskb = (struct sk_buff *)deque(priv, 
#if defined(AP_SWPS_OFFLOAD)
                    pstat->BK_dz_queue,
                    &pkt_info,
#endif
                    &(pstat->BK_dz_queue->head), &(pstat->BK_dz_queue->tail),
					(unsigned long)(pstat->BK_dz_queue->pSkb), NUM_APSD_TXPKT_QUEUE);
#if defined(AP_SWPS_OFFLOAD)
            queueIndex = BK_QUEUE; // 1
#endif     

			if(pskb)
				DEBUG_INFO("deque BK pkt\n");
		}

        
#ifdef AP_SWPS_OFFLOAD
        if(IS_SUPPORT_AP_SWPS_OFFLOAD(priv) && pskb && pstat->reprepare_num[queueIndex-1]){
            tx_reprepare_pkt(priv, pstat, queueIndex, &pkt_info);
            pstat->reprepare_num[queueIndex-1]--;
            
        }else
#endif
		if (pskb) {
			txinsn.q_num   = BE_QUEUE;
			txinsn.fr_type = _SKB_FRAME_TYPE_;
			txinsn.pframe  = pskb;
			txinsn.phdr	   = (UINT8 *)get_wlanllchdr_from_poll(priv);
			pskb->cb[1] = 0;

			if (pskb->len > priv->pmib->dot11OperationEntry.dot11RTSThreshold)
				txinsn.retry = priv->pmib->dot11OperationEntry.dot11LongRetryLimit;
			else
				txinsn.retry = priv->pmib->dot11OperationEntry.dot11ShortRetryLimit;

			if (txinsn.phdr == NULL) {
				DEBUG_ERR("Can't alloc wlan header!\n");
				goto xmit_skb_fail;
			}

			memset((void *)txinsn.phdr, 0, sizeof(struct wlanllc_hdr));

			SetFrDs(txinsn.phdr);
			SetFrameSubType(txinsn.phdr, WIFI_QOS_DATA);
			if (((deque_level == 4) && (!isFFempty(pstat->VO_dz_queue->head, pstat->VO_dz_queue->tail)) && (pstat->apsd_bitmap & 0x01)) ||
				((deque_level >= 3) && (!isFFempty(pstat->VI_dz_queue->head, pstat->VI_dz_queue->tail)) && (pstat->apsd_bitmap & 0x02)) ||
				((deque_level >= 2) && (!isFFempty(pstat->BE_dz_queue->head, pstat->BE_dz_queue->tail)) && (pstat->apsd_bitmap & 0x08)) ||
				((deque_level >= 1) && (!isFFempty(pstat->BK_dz_queue->head, pstat->BK_dz_queue->tail)) && (pstat->apsd_bitmap & 0x04)))
				SetMData(txinsn.phdr);

			if(pstat->txpause_flag){
#ifdef CONFIG_WLAN_HAL
				if(IS_HAL_CHIP(priv))
					GET_HAL_INTERFACE(priv)->SetMACIDSleepHandler(priv, 0 , REMAP_AID(pstat));
#endif
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
				if (GET_CHIP_VER(priv)==VERSION_8812E || GET_CHIP_VER(priv) == VERSION_8723B)
					RTL8812_MACID_PAUSE(priv, 0, REMAP_AID(pstat));
#endif
#ifdef CONFIG_RTL_88E_SUPPORT
				if(GET_CHIP_VER(priv)== VERSION_8188E)
					RTL8188E_MACID_PAUSE(priv, 0, REMAP_AID(pstat));
#endif
				priv->pshare->paused_sta_num--;		
				pstat->txpause_flag = 0;
			}

			if(pstat->txpdrop_flag){
#ifdef CONFIG_WLAN_HAL
				if(IS_HAL_CHIP(priv))
					GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, pstat, INCREASE);
#endif
#if defined(CONFIG_RTL_8812_SUPPORT)
				if(GET_CHIP_VER(priv)==VERSION_8812E)
					UpdateHalMSRRPT8812(priv, pstat, INCREASE);	
#endif
				pstat->txpdrop_flag = 0;
			}

			if (rtl8192cd_wlantx(priv, &txinsn) == CONGESTED) {

xmit_skb_fail:
				priv->ext_stats.tx_drops++;
				DEBUG_WARN("TX DROP: Congested!\n");
				if (txinsn.phdr)
					release_wlanllchdr_to_poll(priv, txinsn.phdr);
				if (pskb)
					rtl_kfree_skb(priv, pskb, _SKB_TX_);
			}
		}
		else if (deque_level <= 1) {
			if ((pstat->apsd_pkt_buffering) &&
				(isFFempty(pstat->VO_dz_queue->head, pstat->VO_dz_queue->tail)) &&
				(isFFempty(pstat->VI_dz_queue->head, pstat->VI_dz_queue->tail)) &&
				(isFFempty(pstat->BE_dz_queue->head, pstat->BE_dz_queue->tail)) &&
				(isFFempty(pstat->BK_dz_queue->head, pstat->BK_dz_queue->tail)))
				pstat->apsd_pkt_buffering = 0;
			break;
		}

		if (++round > 10000) {
			panic_printk("%s[%d] while (1) goes too many\n", __FUNCTION__, __LINE__);
			break;
		}
	}
}
#endif // CONFIG_PCI_HCI















#if defined(DRVMAC_LB) && defined(WIFI_WMM)
static void process_lb_qos_null(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
//	unsigned char *pframe;
//	unsigned int aid;
//	struct stat_info *pstat = NULL;

//	pframe  = get_pframe(pfrinfo);
//	aid = GetAid(pframe);
//	pstat = get_aidinfo(priv, aid);

//	if ((!(OPMODE & WIFI_AP_STATE)) || (pstat == NULL) || (memcmp(pstat->cmn_info.mac_addr, get_sa(pframe), MACADDRLEN))) {
//		rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
//		return;
//	}
//	process_APSD_dz_queue(priv, pstat, pfrinfo->tid);

#if 0
	if (pfrinfo->pskb && pfrinfo->pskb->data) {
		unsigned int *p_skb_int = (unsigned int *)pfrinfo->pskb->data;
		printk("LB RX FRAME =====>>\n");
		printk("0x%08x 0x%08x 0x%08x 0x%08x\n", *p_skb_int, *(p_skb_int+1), *(p_skb_int+2), *(p_skb_int+3));
		printk("0x%08x 0x%08x 0x%08x 0x%08x\n", *(p_skb_int+4), *(p_skb_int+5), *(p_skb_int+6), *(p_skb_int+7));
		printk("LB RX FRAME <<=====\n");
	}
#endif
	rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
}


static void process_lb_qos(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
//	unsigned char *pframe;
//	unsigned int aid;
//	struct stat_info *pstat = NULL;

//	pframe  = get_pframe(pfrinfo);
//	aid = GetAid(pframe);
//	pstat = get_aidinfo(priv, aid);

//	if ((!(OPMODE & WIFI_AP_STATE)) || (pstat == NULL) || (memcmp(pstat->cmn_info.mac_addr, get_sa(pframe), MACADDRLEN))) {
//		rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
//		return;
//	}
//	process_APSD_dz_queue(priv, pstat, pfrinfo->tid);

#if 1
	if (pfrinfo->pskb && pfrinfo->pskb->data) {
		unsigned char *p_skb_int = (unsigned char *)pfrinfo->pskb->data;
		unsigned int payload_length = 0, i = 0, mismatch = 0;
		unsigned char matching = 0;

		if (pfrinfo->pktlen && pfrinfo->hdr_len && pfrinfo->pktlen > pfrinfo->hdr_len) {
			payload_length = pfrinfo->pktlen - pfrinfo->hdr_len;
			if (payload_length >= 2048)
				printk("LB Qos RX, payload max hit!\n");
//			if (payload_length > 32)
//				payload_length = 32;
		}
		else {
			if (!pfrinfo->pktlen)
				printk("LB Qos RX, zero pktlen!!!\n");
			else if (!pfrinfo->hdr_len)
				printk("LB Qos RX, zero hdr_len!!!\n");
			else if (pfrinfo->pktlen < pfrinfo->hdr_len)
				printk("LB Qos RX, pktlen < hdr_len!!!\n");
			else
				printk("LB Qos RX, empty payload.\n");
			goto out;
		}

		p_skb_int += pfrinfo->hdr_len;

//		printk("LB RX >> ");
//		for (i = 0; i < payload_length; i++) {
//			if (i>0 && !(i%4))
//				printk(" ");
//			if (!(i%4))
//				printk("0x");
//			printk("%02x", *(p_skb_int+i));
//		}
//		printk(" <<\n");

		for (i = 0; i < payload_length; i++) {
			if (priv->pmib->miscEntry.lb_mlmp == 1) {
				matching = 0;
				if (memcmp((p_skb_int+i), &matching, 1)) {
					mismatch++;
					break;
				}
			}
			else if (priv->pmib->miscEntry.lb_mlmp == 2) {
				matching = 0xff;
				if (memcmp((p_skb_int+i), &matching, 1)) {
					mismatch++;
					break;
				}
			}
			else if ((priv->pmib->miscEntry.lb_mlmp == 3) || (priv->pmib->miscEntry.lb_mlmp == 4)) {
				matching = i%0x100;
				if (memcmp((p_skb_int+i), &matching, 1)) {
					mismatch++;
					break;
				}
			}
			else {
				printk("LB Qos RX, wrong mlmp setting!\n");
				goto out;
			}
		}

		if (mismatch) {
			printk("LB Qos RX, rx pattern mismatch!!\n");
			priv->pmib->miscEntry.drvmac_lb = 0;	// stop the test
		}
	}
#endif

out:
	rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
}
#endif


#if (defined(__LINUX_2_6__) || defined(__ECOS)) && defined(CONFIG_RTL_IGMP_SNOOPING)
	/*added by qinjunjie,to avoid igmpv1/v2 report suppress*/
int rtl8192cd_isIgmpV1V2Report(unsigned char *macFrame)
{
	unsigned char *ptr;
#ifdef __ECOS
	struct ip *iph=NULL;
#else
	struct iphdr *iph=NULL;
#endif
	unsigned int payloadLen;

	if((macFrame[0]!=0x01) || (macFrame[1]!=0x00) || (macFrame[2]!=0x5e))
	{
		return FALSE;
	}

	ptr=macFrame+12;
	if(*(u16 *)(ptr)==htons(0x8100))
	{
		ptr=ptr+4;
	}

	/*it's not ipv4 packet*/
	if(*(u16 *)(ptr)!=htons(0x0800))
	{
		return FALSE;
	}
	ptr=(ptr+2);
#ifdef __ECOS
	iph=(struct ip *)ptr;
	if(iph->ip_p!=0x02)
#else
	iph=(struct iphdr *)ptr;
	if(iph->protocol!=0x02)
#endif
	{
		return FALSE;
	}

#ifdef __ECOS
	payloadLen=(iph->ip_len-((iph->ip_hl&0x0f)<<2));
#else
	payloadLen=(iph->tot_len-((iph->ihl&0x0f)<<2));
#endif
	if(payloadLen>8)
	{
		return FALSE;
	}

#ifdef __ECOS
	ptr=ptr+(((unsigned int)iph->ip_hl)<<2);
#else
	ptr=ptr+(((unsigned int)iph->ihl)<<2);
#endif
	if((*ptr==0x11) ||(*ptr==0x16))
	{
		return TRUE;
	}
	return FALSE;

}
#if defined (CONFIG_RTL_MLD_SNOOPING)
#define IPV6_ROUTER_ALTER_OPTION 0x05020000
#define  HOP_BY_HOP_OPTIONS_HEADER 0
#define ROUTING_HEADER 43
#define  FRAGMENT_HEADER 44
#define DESTINATION_OPTION_HEADER 60

#define ICMP_PROTOCOL 58

#define MLD_QUERY 130
#define MLDV1_REPORT 131
#define MLDV1_DONE 132
#define MLDV2_REPORT 143

int rtl8192cd_isMldV1V2Report(unsigned char *macFrame)
{
	unsigned char *ptr;
#ifdef __ECOS
	struct ip6_hdr* ipv6h;
#else
	struct ipv6hdr* ipv6h;
#endif
	unsigned char *startPtr=NULL;
	unsigned char *lastPtr=NULL;
	unsigned char nextHeader=0;
	unsigned short extensionHdrLen=0;

	unsigned char  optionDataLen=0;
	unsigned char  optionType=0;
	unsigned int ipv6RAO=0;

	if((macFrame[0]!=0x33) || (macFrame[1]!=0x33))
	{
		return FALSE;
	}

	if(macFrame[2]==0xff)
	{
		return FALSE;
	}

	ptr=macFrame+12;
	if(*(u16 *)(ptr)==htons(0x8100))
	{
		ptr=ptr+4;
	}

	/*it's not ipv6 packet*/
	if(*(u16 *)(ptr)!=htons(0x86dd))
	{
		return FALSE;
	}

	ptr=(ptr+2);

#ifdef __ECOS
	ipv6h= (struct ip6_hdr *) ptr;
	if(ipv6h->ip6_vfc!=IPV6_VERSION)
#else
	ipv6h= (struct ipv6hdr *) ptr;
	if(ipv6h->version!=6)
#endif
	{
		return FALSE;
	}

	startPtr= (unsigned char *)ptr;
#ifdef __ECOS
	lastPtr=startPtr+sizeof(struct ip6_hdr)+(ipv6h->ip6_plen);
	nextHeader= ipv6h ->ip6_nxt;
	ptr=startPtr+sizeof(struct ip6_hdr);
#else
	lastPtr=startPtr+sizeof(struct ipv6hdr)+(ipv6h->payload_len);
	nextHeader= ipv6h ->nexthdr;
	ptr=startPtr+sizeof(struct ipv6hdr);
#endif

	while(ptr<lastPtr)
	{
		switch(nextHeader)
		{
			case HOP_BY_HOP_OPTIONS_HEADER:
				/*parse hop-by-hop option*/
				nextHeader=ptr[0];
				extensionHdrLen=((u16)(ptr[1])+1)*8;
				ptr=ptr+2;

#ifdef __ECOS
				while(ptr<(startPtr+extensionHdrLen+sizeof(struct ip6_hdr)))
#else
				while(ptr<(startPtr+extensionHdrLen+sizeof(struct ipv6hdr)))
#endif
				{
					optionType=ptr[0];
					/*pad1 option*/
					if(optionType==0)
					{
						ptr=ptr+1;
						continue;
					}

					/*padN option*/
					if(optionType==1)
					{
						optionDataLen=ptr[1];
						ptr=ptr+optionDataLen+2;
						continue;
					}

					/*router altert option*/
					if(ntohl(*(u32 *)(ptr))==IPV6_ROUTER_ALTER_OPTION)
					{
						ipv6RAO=IPV6_ROUTER_ALTER_OPTION;
						ptr=ptr+4;
						continue;
					}

					/*other TLV option*/
					if((optionType!=0) && (optionType!=1))
					{
						optionDataLen=ptr[1];
						ptr=ptr+optionDataLen+2;
						continue;
					}


				}

				break;

			case ROUTING_HEADER:
				nextHeader=ptr[0];
				extensionHdrLen=((u16)(ptr[1])+1)*8;
                            	ptr=ptr+extensionHdrLen;
				break;

			case FRAGMENT_HEADER:
				nextHeader=ptr[0];
				ptr=ptr+8;
				break;

			case DESTINATION_OPTION_HEADER:
				nextHeader=ptr[0];
				extensionHdrLen=((u16)(ptr[1])+1)*8;
				ptr=ptr+extensionHdrLen;
				break;

			case ICMP_PROTOCOL:
				if ((ptr[0] == MLDV2_REPORT) || (ptr[0] == MLDV1_REPORT) || (ptr[0] == MLDV1_DONE))
				{
					return TRUE;
				}
				else
				{
					return FALSE;
				}
				break;

			default:
				return FALSE;
		}

	}
	return FALSE;

}
#endif
#endif

#ifdef PREVENT_ARP_SPOOFING
static int check_arp_spoofing(struct rtl8192cd_priv *priv, struct sk_buff *pskb)
{
	struct net_bridge_port *br_port;
	struct arphdr *arp;
	unsigned char *arp_ptr;
	unsigned short protocol;
	__be32 ipaddr = 0;

	br_port = GET_BR_PORT(priv->dev);
	if (!br_port)
		return 0;

	protocol = *((unsigned short *)(pskb->data + 2 * ETH_ALEN));
	if (protocol == __constant_htons(ETH_P_ARP)) {
		arp = (struct arphdr *)(pskb->data + ETH_HLEN);
		if (arp->ar_pro == __constant_htons(ETH_P_IP)) {
			arp_ptr = (unsigned char *)(arp + 1);
			arp_ptr += arp->ar_hln;
	
			ipaddr = inet_select_addr(br_port->br->dev, 0, RT_SCOPE_LINK);
			if (!memcmp(&ipaddr, arp_ptr, arp->ar_pln))
				return -1;
		}
	}

	return 0;
}
#endif




/*
	Actually process RX management frame:

		Process management frame stored in "inputPfrinfo" or gotten from "list",
		only one of them is used to get Frame information.

			Note that:
			1. If frame information is gotten from "list", "inputPfrinfo" MUST be NULL.
			2. If frame information is gotten from "inputPfrinfo", "list" MUST be NULL
*/
 void rtl8192cd_rx_mgntframe(struct rtl8192cd_priv *priv, struct list_head *list,
				struct rx_frinfo *inputPfrinfo)
{
	struct rx_frinfo *pfrinfo = NULL;

	// for SW LED
	if (priv->pshare->LED_cnt_mgn_pkt)
		priv->pshare->LED_rx_cnt++;

	/* Get RX frame info */
	if (list) {
		/* Indicate the frame information can be gotten from "list" */
		pfrinfo = list_entry(list, struct rx_frinfo, rx_list);
	}
	else {
		/* Indicate the frame information is stored in "inputPfrinfo" */
		pfrinfo = inputPfrinfo;
	}

	if (pfrinfo == NULL)
		goto out;

	mgt_handler(priv, pfrinfo);

out:
	return;
}


/*
	Actually process RX control frame:

		Process control frame stored in "inputPfrinfo" or gotten from "list",
		only one of them is used to get Frame information.

			Note that:
			1. If frame information is gotten from "list", "inputPfrinfo" MUST be NULL.
			2. If frame information is gotten from "inputPfrinfo", "list" MUST be NULL
*/
static void rtl8192cd_rx_ctrlframe(struct rtl8192cd_priv *priv, struct list_head *list,
				struct rx_frinfo *inputPfrinfo)
{
	struct rx_frinfo *pfrinfo = NULL;

	/* Get RX frame info */
	if (list) {
		/* Indicate the frame information can be gotten from "list" */
		pfrinfo = list_entry(list, struct rx_frinfo, rx_list);
	}
	else {
		/* Indicate the frame information is stored in "inputPfrinfo " */
		pfrinfo = inputPfrinfo;
	}

	if (pfrinfo == NULL)
		goto out;

	ctrl_handler(priv, pfrinfo);

out:
	return;
}




#ifdef CONFIG_PCI_HCI
#if !(defined(RTL8190_ISR_RX) && defined(RTL8190_DIRECT_RX))
void process_all_queues(struct rtl8192cd_priv *priv)
{
	struct list_head *list = NULL;
	struct rx_frinfo *pfrinfo = NULL;

#if defined(SMP_SYNC)
	unsigned long x;
#endif
	// processing data frame first...
	while(1)
	{
		SMP_LOCK_RX_DATA(x);
		list = dequeue_frame(priv, &(priv->rx_datalist));
		SMP_UNLOCK_RX_DATA(x);

		if (list == NULL)
			break;

		rtl_atomic_dec(&priv->pshare->rx_enqueue_cnt);
		
#if defined(CONFIG_WLAN_HAL_8814BE) && defined(CONFIG_RTL9607C)
		if (GET_CHIP_VER(priv) == VERSION_8814B)
		{
            pfrinfo = list_entry(list, struct rx_frinfo, rx_list);		
            if (pfrinfo->rxpkt_info->pkt_type == RXPKTINFO_TYPE_8023
                    || pfrinfo->rxpkt_info->pkt_type == RXPKTINFO_TYPE_AMSDU) {
                if (rtl8192cd_rx_dispatch_offload(priv, pfrinfo) == FAIL)
                    rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
			} else {
                rtl8192cd_rx_dataframe(priv, list, NULL);
            }
		}
		else
			rtl8192cd_rx_dataframe(priv, list, NULL);
#else
		rtl8192cd_rx_dataframe(priv, list, NULL);
#endif

#ifdef CONFIG_RG_NETIF_RX_QUEUE_SUPPORT
        process_rg_netif_rx_queue(priv);
#endif
	}

	// going to process management frame
	while(1)
	{
		SMP_LOCK_RX_MGT(x);
		list = dequeue_frame(priv, &(priv->rx_mgtlist));
		SMP_UNLOCK_RX_MGT(x);

		if (list == NULL)
			break;

		rtl_atomic_dec(&priv->pshare->rx_enqueue_cnt);
		rtl8192cd_rx_mgntframe(priv, list, NULL);
	}

	while(1)
	{
		SMP_LOCK_RX_CTRL(x);
		list = dequeue_frame(priv, &(priv->rx_ctrllist));
		SMP_UNLOCK_RX_CTRL(x);

		if (list == NULL)
			break;

		rtl_atomic_dec(&priv->pshare->rx_enqueue_cnt);
		rtl8192cd_rx_ctrlframe(priv, list, NULL);
	}

	if (!list_empty(&priv->wakeup_list))
		process_dzqueue(priv);
}

#ifdef CONFIG_WLAN_HAL
__IRAM_IN_865X
static void rtl88XX_rx_dsr(unsigned long task_priv)
{
	struct rtl8192cd_priv	*priv = (struct rtl8192cd_priv *)task_priv;
#ifndef SMP_SYNC
	unsigned long           flags;
#endif
	unsigned long           mask, mask_rx;
#ifdef MBSSID
	int                     i;
#endif

#ifndef __ASUS_DVD__
	extern int r3k_flush_dcache_range(int, int);
#endif

#if defined(__KERNEL__) || defined(__OSK__)
	// disable rx interrupt in DSR
	SAVE_INT_AND_CLI(flags);
    GET_HAL_INTERFACE(priv)->DisableRxRelatedInterruptHandler(priv);
#endif

	rtl8192cd_rx_isr(priv);

#if defined(__KERNEL__) || defined(__OSK__)
	RESTORE_INT(flags);
#endif

	process_all_queues(priv);

#ifdef UNIVERSAL_REPEATER
	if (IS_DRV_OPEN(GET_VXD_PRIV(priv)))
		process_all_queues(GET_VXD_PRIV(priv));
#endif

#ifdef MBSSID
	if (GET_ROOT(priv)->pmib->miscEntry.vap_enable) {
		for (i=0; i<RTL8192CD_NUM_VWLAN; i++) {
			if (IS_DRV_OPEN(priv->pvap_priv[i]))
				process_all_queues(priv->pvap_priv[i]);
		}
	}
#endif

#if defined(__KERNEL__) || defined(__OSK__)
    GET_HAL_INTERFACE(priv)->EnableRxRelatedInterruptHandler(priv);
#endif
}
#endif // CONFIG_WLAN_HAL

__IRAM_IN_865X
void rtl8192cd_rx_dsr(unsigned long task_priv)
{
	struct rtl8192cd_priv	*priv = (struct rtl8192cd_priv *)task_priv;
#ifndef SMP_SYNC
	unsigned long flags;
#endif
#ifdef MBSSID
	int i;
#endif

#ifndef __ASUS_DVD__
	extern int r3k_flush_dcache_range(int, int);
#endif

#ifdef CONFIG_WLAN_HAL
    if (IS_HAL_CHIP(priv)) {
        rtl88XX_rx_dsr(task_priv);
        return;
    } else if(CONFIG_WLAN_NOT_HAL_EXIST)
#endif //CONFIG_WLAN_HAL
	{//not HAL
#if defined(__KERNEL__) || defined(__OSK__)
		// disable rx interrupt in DSR
		SAVE_INT_AND_CLI(flags);
#ifdef CONFIG_RTL_88E_SUPPORT
		if (GET_CHIP_VER(priv)==VERSION_8188E) {
			RTL_W32(REG_88E_HIMR, priv->pshare->InterruptMask & ~HIMR_88E_ROK);
			RTL_W32(REG_88E_HIMRE, priv->pshare->InterruptMaskExt & ~HIMRE_88E_RXFOVW);
		} else
#endif
#ifdef CONFIG_RTL_8812_SUPPORT
		if(GET_CHIP_VER(priv)== VERSION_8812E)
		{
			RTL_W32(REG_92E_HIMR, priv->pshare->InterruptMask & ~HIMR_92E_ROK);
			RTL_W32(REG_92E_HIMRE, priv->pshare->InterruptMask & ~HIMRE_92E_RXFOVW);
		}
		else
#endif
#if defined(CONFIG_RTL_8723B_SUPPORT) 
		if(GET_CHIP_VER(priv)== VERSION_8723B)
		{
			RTL_W32(REG_8723B_HIMR, priv->pshare->InterruptMask & ~HIMR_8723B_ROK);
			RTL_W32(REG_8723B_HIMRE, priv->pshare->InterruptMask & ~HIMRE_8723B_RXFOVW);
		}
		else
#endif
		{
			RTL_W32(HIMR, priv->pshare->InterruptMask & ~(HIMR_RXFOVW | HIMR_ROK));
		}
#endif

		rtl8192cd_rx_isr(priv);

#if defined(__KERNEL__) || defined(__OSK__)
		RESTORE_INT(flags);
#endif

		process_all_queues(priv);

#ifdef UNIVERSAL_REPEATER
		if (IS_DRV_OPEN(GET_VXD_PRIV(priv)))
			process_all_queues(GET_VXD_PRIV(priv));
#endif

#ifdef MBSSID
		if (GET_ROOT(priv)->pmib->miscEntry.vap_enable) {
			for (i=0; i<RTL8192CD_NUM_VWLAN; i++) {
				if (IS_DRV_OPEN(priv->pvap_priv[i]))
					process_all_queues(priv->pvap_priv[i]);
			}
		}
#endif

#if defined(__KERNEL__) || defined(__OSK__)
#ifdef CONFIG_RTL_88E_SUPPORT
		if (GET_CHIP_VER(priv)==VERSION_8188E) {
			RTL_W32(REG_88E_HIMR, priv->pshare->InterruptMask);
			RTL_W32(REG_88E_HIMRE, priv->pshare->InterruptMaskExt);
		} 
		else
#endif
#ifdef CONFIG_RTL_8812_SUPPORT
		if(GET_CHIP_VER(priv)== VERSION_8812E)
		{
			RTL_W32(REG_92E_HIMR, priv->pshare->InterruptMask);
			RTL_W32(REG_92E_HIMRE, priv->pshare->InterruptMaskExt);
		}
		else
#endif
#if defined(CONFIG_RTL_8723B_SUPPORT) 
		if(GET_CHIP_VER(priv)== VERSION_8723B)
		{
			RTL_W32(REG_8723B_HIMR, priv->pshare->InterruptMask);
			RTL_W32(REG_8723B_HIMRE, priv->pshare->InterruptMaskExt);
		}
		else
#endif
		{
			RTL_W32(HIMR, priv->pshare->InterruptMask);
		}
#endif
	}
}
#endif // !(defined(RTL8190_ISR_RX) && defined(RTL8190_DIRECT_RX))
#endif // CONFIG_PCI_HCI

#if !(defined(RTL8190_ISR_RX) && defined(RTL8190_DIRECT_RX))
void flush_rx_queue(struct rtl8192cd_priv *priv)
{
	struct list_head *list = NULL;
	struct rx_frinfo *pfrinfo = NULL;

	while(1)
	{
		list = dequeue_frame(priv, &(priv->rx_datalist));

		if (list == NULL)
			break;

		rtl_atomic_dec(&priv->pshare->rx_enqueue_cnt);
		pfrinfo = list_entry(list, struct rx_frinfo, rx_list);
		rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
	}
	
	while(1)
	{
		list = dequeue_frame(priv, &(priv->rx_mgtlist));

		if (list == NULL)
			break;

		rtl_atomic_dec(&priv->pshare->rx_enqueue_cnt);
		pfrinfo = list_entry(list, struct rx_frinfo, rx_list);
		rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
	}
	
	while(1)
	{
		list = dequeue_frame(priv, &(priv->rx_ctrllist));

		if (list == NULL)
			break;

		rtl_atomic_dec(&priv->pshare->rx_enqueue_cnt);
		pfrinfo = list_entry(list, struct rx_frinfo, rx_list);
		rtl_kfree_skb(priv, pfrinfo->pskb, _SKB_RX_);
	}
}
#endif // !(defined(RTL8190_ISR_RX) && defined(RTL8190_DIRECT_RX))


#if defined(CONFIG_WLAN_HAL) || defined(CONFIG_RTL_8812_SUPPORT)|| defined(CONFIG_RTL_88E_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
static __inline__ unsigned char parse_ps_poll(struct rtl8192cd_priv *priv, struct stat_info *pstat) {
    unsigned char Q_id[16];
    unsigned char Q_pktnum[16];    

    int i=0;
    unsigned int packet_num=0;
    unsigned char hw_queue_num = 0;

#if IS_RTL88XX_MAC_V3_V4
    if(_GET_HAL_DATA(priv)->MacVersion.is_MAC_v3_v4) {
            hw_queue_num = 16;
        }
#endif // IS_RTL88XX_MAC_V3_V4

#if IS_RTL88XX_MAC_V1_V2
    if(_GET_HAL_DATA(priv)->MacVersion.is_MAC_v1_v2) {    
        hw_queue_num = 8;
    }
#endif //IS_RTL88XX_MAC_V1_V2

#ifdef CONFIG_WLAN_HAL_8192FE
		if (GET_CHIP_VER(priv) == VERSION_8192F)
			hw_queue_num = 16;
#endif

#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
    if(GET_CHIP_VER(priv)== VERSION_8812E || GET_CHIP_VER(priv) == VERSION_8723B){
        hw_queue_num = 8;
    }
#endif

#ifdef CONFIG_RTL_88E_SUPPORT
    if(GET_CHIP_VER(priv)== VERSION_8188E){
        hw_queue_num = 4;
    }
#endif

    if (hw_queue_num) {
        if (!pstat->cmn_info.ra_info.disable_ra)
        {    
            /* check how many packet in hw queue now*/   
#if defined (CONFIG_WLAN_HAL_8814AE) || defined (CONFIG_WLAN_HAL_8822BE) || defined (CONFIG_WLAN_HAL_8822CE) || defined (CONFIG_WLAN_HAL_8197F) || defined(CONFIG_WLAN_HAL_8812FE)
		if (GET_CHIP_VER(priv) == VERSION_8814A || GET_CHIP_VER(priv) == VERSION_8822B || GET_CHIP_VER(priv) == VERSION_8822C || GET_CHIP_VER(priv) == VERSION_8197F || GET_CHIP_VER(priv) == VERSION_8812F) {
				for(i=0; i<4; i++) {
                    Q_pktnum[i] = (RTL_R16(0x1400+i*2) & 0xFFF);     
                    if(Q_pktnum[i]) { 
                        Q_id[i] = (RTL_R8(0x400+i*4+3)>>1) & 0x7f;       //31:25     7b
                    } else {
                        Q_id[i] = 0;
                    }
                    Q_pktnum[i+4] = (RTL_R16(0x1408+i*2) & 0xFFF);
                    if(Q_pktnum[i+4]) { 
                        Q_id[i+4] = (RTL_R8(0x468+i*4+3)>>1) & 0x7f; //31:25     7b
                    } else {
                        Q_id[i+4] = 0;
                    }	
                }
				
			}
			else
#endif			
#if defined(CONFIG_WLAN_HAL_8198F) || defined(CONFIG_WLAN_HAL_8197G)
            if(GET_CHIP_VER(priv)== VERSION_8198F || GET_CHIP_VER(priv)== VERSION_8197G){
                for(i=0;i<16;i++){
                    RTL_W8(0x40C,i);
                    Q_pktnum[i] = RTL_R16(0x404) & 0xFFF;
                    if(Q_pktnum[i])
                        Q_id[i] = RTL_R32(0x400) & 0xFE000000;
                    else
                        Q_id[i] = 0;
                }
            }
            else
#endif
#if defined(CONFIG_WLAN_HAL_8192FE)
			if (GET_CHIP_VER(priv) == VERSION_8192F)
			{
				if (hw_queue_num == 16) {
					unsigned char tmpb = RTL_R8(0x414) & ~0x1f;
					for (i=0; i<16; i++) {
						RTL_W8(0x414, tmpb|i);
						Q_pktnum[i] = RTL_R8(0x401);
						Q_id[i] = RTL_R8(0x404) & 0x7f;
					}
				}
			}
			else
#endif                

			{
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_WLAN_HAL) || defined(CONFIG_RTL_8723B_SUPPORT)
	            if(hw_queue_num == 8) { /* for 92E, 8812, & 8881A*/
	                for(i=0; i<4; i++) {
	                    Q_pktnum[i] = (RTL_R8(0x400+i*4+1) & 0x7f);   // 14:8     7b
	                    if(Q_pktnum[i]) { 
	                        Q_id[i] = (RTL_R8(0x400+i*4+3)>>1) & 0x7f;       //31:25     7b
	                    } else {
	                        Q_id[i] = 0;
	                    }
	                    Q_pktnum[i+4] = (RTL_R8(0x468+i*4+1) & 0x7f);   // 14:8     7b
	                    if(Q_pktnum[i+4]) { 
	                        Q_id[i+4] = (RTL_R8(0x468+i*4+3)>>1) & 0x7f; //31:25     7b
	                    } else {
	                        Q_id[i+4] = 0;
	                    }
	                }
	            }
#endif                
#ifdef CONFIG_RTL_88E_SUPPORT
	            if(hw_queue_num == 4) { /* 88E */
	                for(i=0; i<4; i++) {  
	                    Q_pktnum[i] = RTL_R8(0x400+i*4+1);
	                    if(Q_pktnum[i]) {             // 15:8     7b
	                        Q_id[i] = (RTL_R8(0x400+i*4+3)>>2) & 0x3f;       //31:26     7b                           
	                    } else {
	                        Q_id[i] = 0;
	                    }
	                }
	            }
#endif                
			}
            for(i=0; i<hw_queue_num; i++) {
                if (REMAP_AID(pstat) == Q_id[i]) {
                    packet_num += Q_pktnum[i];
                }
            }

#if defined(CONFIG_WLAN_HAL_8814AE) || defined(CONFIG_WLAN_HAL_8822BE) || defined(CONFIG_WLAN_HAL_8822CE) || defined(CONFIG_WLAN_HAL_8197F) || defined(CONFIG_WLAN_HAL_8198F) || defined(CONFIG_WLAN_HAL_8812FE) || defined (CONFIG_WLAN_HAL_8197G)
			if (GET_CHIP_VER(priv) == VERSION_8814A || GET_CHIP_VER(priv) == VERSION_8822B || GET_CHIP_VER(priv) == VERSION_8822C || GET_CHIP_VER(priv) == VERSION_8197F || GET_CHIP_VER(priv) == VERSION_8198F || GET_CHIP_VER(priv) == VERSION_8812F|| GET_CHIP_VER(priv) == VERSION_8197G) {

				// Release one packet			
				if(packet_num >=1) {	
                    /*if(pstat->txpdrop_flag==1){
                        RT_TRACE(COMP_DBG,DBG_LOUD,("[%s] tx drop flag is 1 when want to ReleaseOnePacket\n", __func__));
                        if(pstat->txpause_flag==0)   
                            RT_TRACE(COMP_DBG,DBG_LOUD,("[%s] and tx pause flag is 0 when want to ReleaseOnePacket\n", __func__));
                    }*/
					GET_HAL_INTERFACE(priv)->ReleaseOnePacketHandler(priv,REMAP_AID(pstat));
					return 1;
				} else {
					if(pstat->txpdrop_flag == 1) {
						GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, pstat, INCREASE);
						pstat->txpdrop_flag = 0;
					}
					if(pstat->txpause_flag){
						/*relase tx pause*/
						GET_HAL_INTERFACE(priv)->SetMACIDSleepHandler(priv, 0 , REMAP_AID(pstat));
					    pstat->txpause_flag = 0;
						if(priv->pshare->paused_sta_num)
							priv->pshare->paused_sta_num--;		
					}
					return 0;
				}
			} else
#endif // #ifdef CONFIG_WLAN_HAL_8814AE                           
 
			{
			if(pstat->txpdrop_flag == 1) {
#ifdef CONFIG_WLAN_HAL			
				if(IS_HAL_CHIP(priv))
					GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, pstat, INCREASE);
#endif
#ifdef CONFIG_RTL_8812_SUPPORT
				if(GET_CHIP_VER(priv)==VERSION_8812E)
					UpdateHalMSRRPT8812(priv, pstat, INCREASE);			
#endif
				pstat->txpdrop_flag = 0;
			} 

            if(pstat->txpause_flag == 1) {
                if(packet_num > 1) {    
                    /* drop packet in hw queue for 11n logo test 4.2.47,
                                        DO NOT change the sequence!!*/

                    /*1. drop packet command*/
                    #ifdef CONFIG_WLAN_HAL
                    if(IS_HAL_CHIP(priv))
                        GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, pstat, DECREASE); 
                    #endif    

                    #if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
                    if(GET_CHIP_VER(priv)== VERSION_8812E || GET_CHIP_VER(priv) == VERSION_8723B)
                        UpdateHalMSRRPT8812(priv, pstat, DECREASE);
                    #endif
                    #ifdef CONFIG_RTL_88E_SUPPORT
                    if(GET_CHIP_VER(priv)== VERSION_8188E)
                        RTL8188E_MACID_NOLINK(priv, 1, REMAP_AID(pstat));
                    #endif

                    /*2. wait 500 us to make sure the H2C command is sent to hw, only 92E, 8812, and 8881A needed*/
                    if(hw_queue_num == 8) 
                        delay_us(500);


                    /*3. relase tx pause*/
                    #ifdef CONFIG_WLAN_HAL
                    if(IS_HAL_CHIP(priv))
                        GET_HAL_INTERFACE(priv)->SetMACIDSleepHandler(priv, 0 , REMAP_AID(pstat));
                    #endif
                    #if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
                    if (GET_CHIP_VER(priv)==VERSION_8812E || GET_CHIP_VER(priv) == VERSION_8723B)
                        RTL8812_MACID_PAUSE(priv, 0, REMAP_AID(pstat));
                    #endif
                    #ifdef CONFIG_RTL_88E_SUPPORT
                    if(GET_CHIP_VER(priv)== VERSION_8188E)
                        RTL8188E_MACID_PAUSE(priv, 0, REMAP_AID(pstat));
                    #endif

					if(priv->pshare->paused_sta_num)
						priv->pshare->paused_sta_num--;		
                    pstat->txpause_flag = 0;


                    /*4. wait 50*hwq_num us  for hw to deque*/
                    if(hw_queue_num == 8) 
                        delay_us(50*packet_num); 
                    else
                        delay_us(120*packet_num);  /* for 88E*/


                    /*5. cancel drop packet command*/ 
                    #ifdef CONFIG_WLAN_HAL
                    if(IS_HAL_CHIP(priv))
                        GET_HAL_INTERFACE(priv)->UpdateHalMSRRPTHandler(priv, pstat, INCREASE); 
                    #endif    
                    #if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
                    if(GET_CHIP_VER(priv)== VERSION_8812E || GET_CHIP_VER(priv) == VERSION_8723B)
                        UpdateHalMSRRPT8812(priv, pstat, INCREASE);
                    #endif      
                    #ifdef CONFIG_RTL_88E_SUPPORT
                    if(GET_CHIP_VER(priv)== VERSION_8188E)
                        RTL8188E_MACID_NOLINK(priv, 0, REMAP_AID(pstat));
                    #endif
                }                    

                if(pstat->txpause_flag){
                    /*relase tx pause*/
                    #ifdef CONFIG_WLAN_HAL
                    if(IS_HAL_CHIP(priv))
                        GET_HAL_INTERFACE(priv)->SetMACIDSleepHandler(priv, 0 , REMAP_AID(pstat));
                    #endif
                    #if defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
                    if (GET_CHIP_VER(priv)==VERSION_8812E || GET_CHIP_VER(priv) == VERSION_8723B)
                        RTL8812_MACID_PAUSE(priv, 0, REMAP_AID(pstat));
                    #endif
                    #ifdef CONFIG_RTL_88E_SUPPORT
                    if(GET_CHIP_VER(priv)== VERSION_8188E)
                        RTL8188E_MACID_PAUSE(priv, 0, REMAP_AID(pstat));
                    #endif                    
                    pstat->txpause_flag = 0;
					if(priv->pshare->paused_sta_num)
						priv->pshare->paused_sta_num--;		

                }

                if(packet_num == 1) {
                    return 1;
                }                
            }
            else {
                if(packet_num > 0)
                    return 1;
            }
        }

            DEBUG_WARN("%s %d OnPsPoll, set MACID 0 AID = %x \n",__FUNCTION__,__LINE__,REMAP_AID(pstat));
        }
        else
        {
            DEBUG_WARN(" MACID sleep only support %d STA \n", priv->pshare->fw_support_sta_num-1);
        }
    }    
    return 0;

}
#endif

static void ctrl_handler(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
	struct sk_buff *pskbpoll, *pskb;
	unsigned char *pframe;
	struct stat_info *pstat;
	unsigned short aid;

// 2009.09.08
#ifndef SMP_SYNC
	unsigned long flags;
#endif

	DECLARE_TXINSN(txinsn);

	pframe  = get_pframe(pfrinfo);
	pskbpoll = get_pskb(pfrinfo);

	aid = GetAid(pframe);

	pstat = get_aidinfo(priv, aid);

	if (pstat == NULL)
		goto end_ctrl;

	// check if hardware address matches...
	if (!isEqualMACAddr(pstat->cmn_info.mac_addr, (void *)(pframe + 10)))
		goto end_ctrl;

#if defined(CONFIG_PCI_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
#if defined(CONFIG_WLAN_HAL) || defined(CONFIG_RTL_8812_SUPPORT) || defined(CONFIG_RTL_88E_SUPPORT) || defined(CONFIG_RTL_8723B_SUPPORT)
	    if ((GetFrameSubType(pframe)) == WIFI_PSPOLL)
	    {
	        if(parse_ps_poll(priv, pstat) == 1)
	            goto end_ctrl;
	    }
#endif

		SAVE_INT_AND_CLI(flags);

		// now dequeue from the pstat's dz_queue
		pskb = (struct sk_buff *)skb_dequeue(&pstat->dz_queue);
#if defined(AP_SWPS_OFFLOAD)
	    if (IS_SUPPORT_AP_SWPS_OFFLOAD(priv)) {
	        if((pstat->dz_list_head.next != &pstat->dz_list_head) && (pskb != NULL)){ //delete the swps seq, no use now
	            struct list_head* dznode;
	            struct dz_seq_node* node_tofree= NULL;
	            dznode = pstat->dz_list_head.next;
	            list_del_init(dznode);
	            node_tofree = list_entry(dznode,struct dz_seq_node,node);
	            kfree(node_tofree);
	            node_tofree = NULL;
	        }
	    }
#endif  

		RESTORE_INT(flags);


		if (pskb == NULL)
			goto end_ctrl;

		txinsn.q_num   = BE_QUEUE; //using low queue for data queue
		txinsn.fr_type = _SKB_FRAME_TYPE_;
		txinsn.pframe  = pskb;
		txinsn.phdr	   = (UINT8 *)get_wlanllchdr_from_poll(priv);
		pskb->cb[1] = 0;

		if (pskb->len > priv->pmib->dot11OperationEntry.dot11RTSThreshold)
			txinsn.retry = priv->pmib->dot11OperationEntry.dot11LongRetryLimit;
		else
			txinsn.retry = priv->pmib->dot11OperationEntry.dot11ShortRetryLimit;

		if (txinsn.phdr == NULL) {
			DEBUG_ERR("Can't alloc wlan header!\n");
			goto xmit_skb_fail;
		}

		memset((void *)txinsn.phdr, 0, sizeof(struct wlanllc_hdr));

		SetFrDs(txinsn.phdr);
#ifdef WIFI_WMM
		if (pstat && (QOS_ENABLE) && (pstat->QosEnabled))
			SetFrameSubType(txinsn.phdr, WIFI_QOS_DATA);
		else
#endif
		SetFrameSubType(txinsn.phdr, WIFI_DATA);

		if (skb_queue_len(&pstat->dz_queue))
			SetMData(txinsn.phdr);

#ifdef A4_STA
		if ((pstat->state & WIFI_A4_STA) && IS_MCAST(pskb->data)) {
			txinsn.pstat = pstat;
			SetToDs(txinsn.phdr);
		}
#endif

		if (rtl8192cd_wlantx(priv, &txinsn) == CONGESTED)
		{

xmit_skb_fail:

			priv->ext_stats.tx_drops++;
			DEBUG_WARN("TX DROP: Congested!\n");
			if (txinsn.phdr)
				release_wlanllchdr_to_poll(priv, txinsn.phdr);
			if (pskb)
				rtl_kfree_skb(priv, pskb, _SKB_TX_);
		}

#if 0
	//#ifdef CONFIG_WLAN_HAL
	//    if ((GetFrameSubType(pframe)) == WIFI_PSPOLL)
	    {
	        if(IS_HAL_CHIP(priv))
	        {
#ifdef SUPPORT_RELEASE_ONE_PACKET            
	            if (IS_SUPPORT_RELEASE_ONE_PACKET(priv)) {
	                // Release one packet                
	                GET_HAL_INTERFACE(priv)->ReleaseOnePacketHandler(priv,REMAP_AID(pstat));               
	            } 
#endif // #ifdef SUPPORT_RELEASE_ONE_PACKET                           
	        }
	     }
#endif
	}
#endif // CONFIG_PCI_HCI

#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
		pstat->sleep_time = jiffies;
		if (tx_servq_len(&pstat->tx_queue[BE_QUEUE])) {
			rtw_pspoll_sta_enqueue(priv, pstat, ENQUEUE_TO_TAIL);
#ifdef __ECOS
			triggered_wlan_tx_tasklet(priv);
#else
			tasklet_hi_schedule(&priv->pshare->xmit_tasklet);
#endif
		}
	}
#endif

end_ctrl:

	if (pskbpoll) {
		rtl_kfree_skb(priv, pskbpoll, _SKB_RX_);
	}

	return;
}


/*
typedef struct tx_sts_struct
{
	// DW 1
	UINT8	TxRateid;
	UINT8	TxRate;
} tx_sts;

typedef struct tag_Tx_Status_Feedback
{
	// For endian transfer --> for driver
	// DW 0
	UINT16	Length;					// Command packet length
	UINT8 	Reserve1;
	UINT8 	Element_ID;			// Command packet type

	tx_sts    Tx_Sts[NUM_STAT];
} CMPK_TX_STATUS_T;
*/



