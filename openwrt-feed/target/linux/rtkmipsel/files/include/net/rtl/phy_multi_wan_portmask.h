//redefine lan/wan port mask, use variable for change physical wan port


#ifndef	RTL_8021Q_VLAN_SUPPORT_MULTI_PHY_VIR_WAN
#define	RTL_8021Q_VLAN_SUPPORT_MULTI_PHY_VIR_WAN

#undef	RTL_WANPORT_MASK
#undef	RTL_LANPORT_MASK
#if defined(CONFIG_RTK_VLAN_SUPPORT) || defined (CONFIG_RTL_MULTI_LAN_DEV)
#undef 	RTL_LANPORT_MASK_1
#undef	RTL_LANPORT_MASK_2
#undef 	RTL_LANPORT_MASK_3
#undef 	RTL_LANPORT_MASK_4
#endif

extern int wanport_mask;
extern int lanport_mask;
#if defined(CONFIG_RTK_VLAN_SUPPORT) || defined (CONFIG_RTL_MULTI_LAN_DEV)
extern int lanport_mask_1;
extern int lanport_mask_2;
extern int lanport_mask_3;
extern int lanport_mask_4;
#endif

#define RTL_WANPORT_MASK wanport_mask
#define RTL_LANPORT_MASK lanport_mask
#if defined(CONFIG_RTK_VLAN_SUPPORT) || defined (CONFIG_RTL_MULTI_LAN_DEV)
#define RTL_LANPORT_MASK_1 lanport_mask_1
#define RTL_LANPORT_MASK_2 lanport_mask_2
#define RTL_LANPORT_MASK_3 lanport_mask_3
#define RTL_LANPORT_MASK_4 lanport_mask_4
#endif

#endif	//RTL_8021Q_VLAN_SUPPORT_MULTI_PHY_VIR_WAN
