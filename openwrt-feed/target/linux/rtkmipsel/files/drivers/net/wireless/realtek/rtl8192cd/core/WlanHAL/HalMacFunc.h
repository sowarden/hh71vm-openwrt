
#ifndef __HALMACFUNC_H__
#define __HALMACFUNC_H__
/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	HalMacFunc.h
	
Abstract:
	Define MAC function support 
	for Driver
	    
Major Change History:
	When       Who               What
	---------- ---------------   -------------------------------
	2015-04-29 Eric            Create.	
--*/
#include "GeneralDef.h"
#include "WlanHAL/HalTxDutySetting.h"

void 
MACFM_software_init(
    struct rtl8192cd_priv *priv
);

void 
MACHAL_version_init(
struct rtl8192cd_priv * Adapter
);

typedef struct _MAC_VERSION_
{
    unsigned char is_MAC_v1;
    unsigned char is_MAC_v2;    
    unsigned char is_MAC_v1_v2;    
    unsigned char is_MAC_v3;        
    unsigned char is_MAC_v2_v3;   
    unsigned char is_MAC_v1_v2_v3;
    unsigned char is_MAC_v4;            
    unsigned char is_MAC_v3_v4;
    unsigned char is_MAC_v2_v3_v4;
	unsigned char is_MAC_v1_v2_v4;
    unsigned char MACHALSupport;        
}MAC_VERSION,*PMAC_VERSION;

typedef enum _MACFun_Support_Ability_Definition
{
	//
	// MAC Func section BIT 0-19
	//
	MAC_FUN_HW_TX_SHORTCUT_REUSE_TXDESC         = BIT0,
	MAC_FUN_HW_TX_SHORTCUT_HDR_CONV             = BIT1,
	MAC_FUN_HW_TX_SHORTCUT_HDR_CONV_LLC         = BIT2,	
	MAC_FUN_HW_SUPPORT_EACH_VAP_INT		        = BIT3,
	MAC_FUN_HW_SUPPORT_RELEASE_ONE_PACKET		= BIT4,
	MAC_FUN_HW_HW_FILL_MACID			        = BIT5,
	MAC_FUN_HW_HW_DETEC_POWER_STATE				= BIT6,
	MAC_FUN_HW_SUPPORT_MULTICAST_BMC_ENHANCE	= BIT7,
	MAC_FUN_HW_SUPPORT_TX_AMSDU             	= BIT8,
	MAC_FUN_HW_SUPPORT_H2C_PACKET             	= BIT9,	
    MAC_FUN_HW_SUPPORT_AXI_EXCEPTION           	= BIT10,		
	MAC_FUN_HW_SUPPORT_AP_OFFLOAD               = BIT11,
	MAC_FUN_HW_HW_SEQ                           = BIT12,	
	MAC_FUN_HW_HW_AES_IV                        = BIT13,		
	MAC_FUN_HW_SUPPORT_AP_SWPS_OFFLOAD          = BIT14,
	MAC_FUN_HW_SUPPORT_ENHANCED_EDCA            = BIT15,	
	MAC_FUN_HW_SUPPORT_TXDESC_IE                = BIT16,
	MAC_FUN_HW_SUPPORT_ADDR_CAM                 = BIT17,
	MAC_FUN_HW_SUPPORT_FW_CMD                   = BIT18,
	MAC_FUN_HW_SUPPORT_AGING_FUNC_OFFLOAD       = BIT19,
	MAC_FUN_HW_SUPPORT_HW_TX_AMSDU              = BIT20,
}MACFUN_SUPPORT_ABILITY,*PMACFUN_SUPPORT_ABILITY;

#define IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC(priv)	\
    (priv->pshare->hal_SupportMACfunction & MAC_FUN_HW_TX_SHORTCUT_REUSE_TXDESC)

#define IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(priv)	\
    (priv->pshare->hal_SupportMACfunction & MAC_FUN_HW_TX_SHORTCUT_HDR_CONV)    

#define IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV_LLC(priv)	\
    (priv->pshare->hal_SupportMACfunction & MAC_FUN_HW_TX_SHORTCUT_HDR_CONV_LLC) 
    
#define IS_SUPPORT_WLAN_HAL_HW_TX_AMSDU(priv)	\
        (priv->pshare->hal_SupportMACfunction & MAC_FUN_HW_SUPPORT_HW_TX_AMSDU)
    
#define IS_SUPPORT_EACH_VAP_INT(priv)	\
    (priv->pshare->hal_SupportMACfunction & MAC_FUN_HW_SUPPORT_EACH_VAP_INT)

#define IS_SUPPORT_RELEASE_ONE_PACKET(priv)	\
    (priv->pshare->hal_SupportMACfunction & MAC_FUN_HW_SUPPORT_RELEASE_ONE_PACKET)

#define IS_SUPPORT_HW_FILL_MACID(priv)	\
    (priv->pshare->hal_SupportMACfunction & MAC_FUN_HW_HW_FILL_MACID)

#define IS_SUPPORT_HW_DETEC_POWER_STATE(priv)	\
    (priv->pshare->hal_SupportMACfunction & MAC_FUN_HW_HW_DETEC_POWER_STATE)

#define IS_SUPPORT_MULTICAST_BMC_ENHANCE(priv)	\
    (priv->pshare->hal_SupportMACfunction & MAC_FUN_HW_SUPPORT_MULTICAST_BMC_ENHANCE)

#define IS_SUPPORT_TX_AMSDU(priv)	\
    (priv->pshare->hal_SupportMACfunction & MAC_FUN_HW_SUPPORT_TX_AMSDU)

#define IS_SUPPORT_H2C_PACKET(priv)	\
    (priv->pshare->hal_SupportMACfunction & MAC_FUN_HW_SUPPORT_H2C_PACKET)

#define IS_SUPPORT_FW_CMD(priv)	\
    (priv->pshare->hal_SupportMACfunction & MAC_FUN_HW_SUPPORT_FW_CMD)

#define IS_SUPPORT_AXI_EXCEPTION(priv)	\
    (priv->pshare->hal_SupportMACfunction & MAC_FUN_HW_SUPPORT_AXI_EXCEPTION)
    
#define IS_SUPPORT_AP_OFFLOAD(priv)	\
    (priv->pshare->hal_SupportMACfunction & MAC_FUN_HW_SUPPORT_AP_OFFLOAD)        

#define IS_SUPPORT_AP_SWPS_OFFLOAD(priv)	\
    (priv->pshare->hal_SupportMACfunction & MAC_FUN_HW_SUPPORT_AP_SWPS_OFFLOAD)    

#define IS_SUPPORT_AGING_FUNC_OFFLOAD(priv)	\
    (priv->pshare->hal_SupportMACfunction & MAC_FUN_HW_SUPPORT_AGING_FUNC_OFFLOAD)    

#define IS_SUPPORT_ENHANCED_EDCA(priv)	\
    (priv->pshare->hal_SupportMACfunction & MAC_FUN_HW_SUPPORT_ENHANCED_EDCA)   

#define IS_SUPPORT_TXDESC_IE(priv)	\
    (priv->pshare->hal_SupportMACfunction & MAC_FUN_HW_SUPPORT_TXDESC_IE)   
    
#define IS_SUPPORT_HW_SEQ(priv)	\
    (priv->pshare->hal_SupportMACfunction & MAC_FUN_HW_HW_SEQ)        

#define IS_SUPPORT_HW_AES_IV(priv)	\
    (priv->pshare->hal_SupportMACfunction & MAC_FUN_HW_HW_AES_IV) 

#define IS_SUPPORT_ADDR_CAM(priv)   \
        (priv->pshare->hal_SupportMACfunction & MAC_FUN_HW_SUPPORT_ADDR_CAM) 
    
#endif //__HALMACFUNC_H__


