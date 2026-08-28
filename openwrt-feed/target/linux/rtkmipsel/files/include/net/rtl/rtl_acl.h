/*
* Copyright c                  Realtek Semiconductor Corporation, 2016
* All rights reserved.
*
* Program : acl header file
* Abstract :
* Author : shirley (shirley_xie@realsil.com.cn)
*/

#ifndef RTL819X_ACL_H
#define RTL819X_ACL_H
//#include <net/rtl/rtl_types.h>
#include <net/rtl/rtl_queue.h>
#include <net/rtl/rtl_glue.h>

#ifndef NULL
#define NULL 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef SUCCESS
#define SUCCESS 	0
#endif

#ifndef FAILED
#define FAILED -1
#endif

#if defined(CONFIG_RTL_83XX_ACL_SUPPORT)
#define INVALID_MAC(mac) ((mac[0]==0)&&(mac[1]==0)&&(mac[2]==0)&&(mac[3]==0)&&(mac[4]==0)&&(mac[5]==0))
//define act_type

enum RTL83XX_ACL_ACTION
{
	RTL83XX_ACL_COPY_CPU = 0,
	RTL83XX_ACL_DROP,
	RTL83XX_ACL_REDIRECT,
	RTL83XX_ACL_MIRROR,
	RTL83XX_ACL_TRAP_CPU,
	RTL83XX_ACL_PRIORITY,
	RTL83XX_ACL_DSCP_REMARK,
	RTL83XX_ACL_1P_REMARK,
	RTL83XX_ACL_POLICING0,
	RTL83XX_ACL_END,
};

#define COMPARE_FLAG_PRIO       0x01
#define COMPARE_FLAG_MEMBER     0x02
#define COMPARE_FLAG_FILTER    	0x04
#define COMPARE_FLAG_ACTION    	0x08
#define COMPARE_FLAG_INVERT		0x10
#define COMPARE_FLAG_ALL       	0xFF

#define MAX_FILTER_NUM 16

//define acl rule prio, the smaller, the higher priority
#if defined(CONFIG_RTL_SUPPORT_DUAL_WAN_PORTS)
#define RTL83XX_ACL_DUAL_WAN_USED				-30001
#endif
#if defined(IMPROVE_MCAST_PERFORMANCE_WITH_RTL8367)
#define RTL83XX_ACL_PRIO_MCAST_HIGH_PRIO 		-30000
#endif
#define RTL83XX_ACL_PRIO_MCBC_TO_CPU   	-20004
#define RTL83XX_ACL_PRIO_UC_TO_CPU		-20003
#define RTL83XX_ACL_PRIO_DNS_TO_CPU		-20002
#define RTL83XX_ACL_PRIO_ALL_TO_CPU   	-20001
#define RTL83XX_ACL_PRIO_HW_BRIDGE_QOS 	-20000

typedef enum rtl83xx_filter_type_s
{
	FILTER_NONE,
	FILTER_MASK,
    FILTER_RANGE,
    FILTER_END
} rtk_filter_type_t;

typedef struct rtl83xx_acl_filter_s
{
	rtk_mac_t 	smac;
	rtk_mac_t  	smac_mask;
    rtk_mac_t 	dmac;
    rtk_mac_t 	dmac_mask;
	rtk_uint32 	ethertype;
	rtk_uint32 	ethertype_mask;
	rtk_uint32  vlanpri;
	rtk_uint32  vlanpri_mask;
	rtk_uint32 	sip_type;
	rtk_uint32 	sip; //lower ip
	rtk_uint32 	sip_mask; //upper ip
	rtk_uint32 	dip_type;
	rtk_uint32 	dip;
	rtk_uint32 	dip_mask;
	rtk_uint32 	ip_tos;
	rtk_uint32 	ip_tos_mask;
	rtk_uint32  protocal;
	rtk_uint32  protocal_mask;
	rtk_uint32 	sport_type;
	rtk_uint32 	sport;
	rtk_uint32 	sport_mask;
	rtk_uint32 	dport_type;
	rtk_uint32 	dport;
	rtk_uint32 	dport_mask;
}rtl83xx_acl_filter_t;

typedef struct rtl83xx_acl_action_s
{
	rtk_int32 act_type;
	rtk_int32 priority; //act_type==RTL83XX_ACL_PRIORITY
	rtk_int32 policing;			// act_type==RTL83XX_ACL_POLICING0
	rtk_uint32 meter_index;		// act_type==RTL83XX_ACL_POLICING0
}rtl83xx_acl_action_t;

typedef struct rtl83xx_acl_rule_s
{
	//unsigned char valid;
	rtk_int32 prio;
	rtk_uint32 index;
	rtk_uint32 member;
	rtk_uint8 invert;

	rtk_uint32 flag;
	rtl83xx_acl_filter_t filter;
	rtl83xx_acl_action_t action;

	//return
	rtk_uint32 size;
	rtk_uint8 in_asic;
	CLIST_ENTRY(rtl83xx_acl_rule_s) next_rule;
	
}rtl83xx_acl_rule_t;

typedef CLIST_HEAD(rtl83xx_acl_rule_header_s, rtl83xx_acl_rule_s) rtl83xx_acl_rule_header_t;
rtk_int32 rtl83xx_init_acl(void);
rtk_int32 rtl83xx_addAclRule(rtl83xx_acl_rule_t *aclRule);
rtk_int32 rtl83xx_deleteAclRule(rtl83xx_acl_rule_t *aclRule);
rtk_int32 rtl83xx_flushAclRule(void);
rtk_int32 rtl83xx_flushAclRulebyPrio(rtk_int32 prio);
rtk_int32 rtl83xx_flushAclRulebyFlag(rtk_uint32 flag, rtl83xx_acl_rule_t *compareAcl);
rtk_int32 rtl83xx_syncAclTblToAsic(void);

#if defined(CONFIG_RTL_8367R_SUPPORT)
#define RTL83XX_ACLRULENO					RTL8367B_ACLRULENO
#define RTL83XX_ACLRULEMAX					RTL8367B_ACLRULEMAX					
#define RTL83XX_ACLRULEFIELDNO				RTL8367B_ACLRULEFIELDNO	
#define RTL83XX_ACLTEMPLATENO				RTL8367B_ACLTEMPLATENO
#define RTL83XX_ACLTYPEMAX					RTL8367B_ACLTYPEMAX	
#define RTL83XX_ACLRULETBLEN				RTL8367B_ACLRULETBLEN
#define RTL83XX_ACLACTTBLEN					RTL8367B_ACLACTTBLEN	
#define RTL83XX_ACLRULETBADDR(type, rule)	RTL8367B_ACLRULETBADDR(type, rule)
#define RTL83XX_ACLRULETBADDR2(type, rule)	RTL8367B_ACLRULETBADDR2(type, rule)	
#define RTL83XX_ACLRULETAGBITS				RTL8367B_ACLRULETAGBITS
#define RTL83XX_ACLRANGENO					RTL8367B_ACLRANGENO		
#define RTL83XX_ACLRANGEMAX					RTL8367B_ACLRANGEMAX				
#define RTL83XX_ACL_PORTRANGEMAX			RTL8367B_ACL_PORTRANGEMAX 
#define RTL83XX_ACL_ACT_TABLE_LEN			RTL8367B_ACL_ACT_TABLE_LEN
#define RTL83XX_METERMAX					RTL8367B_METERMAX
#else
#define RTL_RANGE_MAX						16
#if defined(CONFIG_RTL_83XX_API_V1_4)
#define CHIPTYPE (rtk_switch_chipType_get())
#define RTL83XX_ACLRULENO					((CHIPTYPE==4) ? RTL8367D_ACLRULENO:RTL8367C_ACLRULENO)
#define RTL83XX_ACLRULEMAX					((CHIPTYPE==4) ? RTL8367D_ACLRULEMAX:RTL8367C_ACLRULEMAX)					
#define RTL83XX_ACLRULEFIELDNO				RTK_MAX_NUM_OF_FILTER_FIELD	
#define RTL83XX_ACLTEMPLATENO				RTK_MAX_NUM_OF_FILTER_TYPE
#define RTL83XX_ACLTYPEMAX					(RTK_MAX_NUM_OF_FILTER_TYPE-1)
#define RTL83XX_ACLRULETBLEN				((CHIPTYPE==4) ? RTL8367D_ACLRULETBLEN:RTL8367C_ACLRULETBLEN)
#define RTL83XX_ACLACTTBLEN					((CHIPTYPE==4) ? RTL8367D_ACLACTTBLEN:RTL8367C_ACLACTTBLEN)	
#define RTL83XX_ACLRULETBADDR(type, rule)	((CHIPTYPE==4) ? RTL8367D_ACLRULETBADDR(type, rule):RTL8367C_ACLRULETBADDR(type, rule))
#define RTL83XX_ACLRULETBADDR2(type, rule)	((CHIPTYPE==4) ? RTL8367D_ACLRULETBADDR2(type, rule):RTL8367C_ACLRULETBADDR2(type, rule))	
#define RTL83XX_ACLRULETAGBITS				((CHIPTYPE==4) ? RTL8367D_ACLRULETAGBITS:RTL8367C_ACLRULETAGBITS)
#define RTL83XX_ACLRANGENO					((CHIPTYPE==4) ? RTL8367D_ACLRANGENO:RTL8367C_ACLRANGENO)		
#define RTL83XX_ACLRANGEMAX					((CHIPTYPE==4) ? RTL8367D_ACLRANGEMAX:RTL8367C_ACLRANGEMAX)				
#define RTL83XX_ACL_PORTRANGEMAX			((CHIPTYPE==4) ? RTL8367D_ACL_PORTRANGEMAX:RTL8367C_ACL_PORTRANGEMAX) 
#define RTL83XX_ACL_ACT_TABLE_LEN			((CHIPTYPE==4) ? RTL8367D_ACL_ACT_TABLE_LEN:RTL8367C_ACL_ACT_TABLE_LEN)
#define RTL83XX_METERMAX					RTK_MAX_METER_ID
#else
#define RTL83XX_ACLRULENO					RTL8367C_ACLRULENO
#define RTL83XX_ACLRULEMAX					RTL8367C_ACLRULEMAX					
#define RTL83XX_ACLRULEFIELDNO				RTL8367C_ACLRULEFIELDNO	
#define RTL83XX_ACLTEMPLATENO				RTL8367C_ACLTEMPLATENO
#define RTL83XX_ACLTYPEMAX					RTL8367C_ACLTYPEMAX	
#define RTL83XX_ACLRULETBLEN				RTL8367C_ACLRULETBLEN
#define RTL83XX_ACLACTTBLEN					RTL8367C_ACLACTTBLEN	
#define RTL83XX_ACLRULETBADDR(type, rule)	RTL8367C_ACLRULETBADDR(type, rule)
#define RTL83XX_ACLRULETBADDR2(type, rule)	RTL8367C_ACLRULETBADDR2(type, rule)	
#define RTL83XX_ACLRULETAGBITS				RTL8367C_ACLRULETAGBITS
#define RTL83XX_ACLRANGENO					RTL8367C_ACLRANGENO		
#define RTL83XX_ACLRANGEMAX					RTL8367C_ACLRANGEMAX				
#define RTL83XX_ACL_PORTRANGEMAX			RTL8367C_ACL_PORTRANGEMAX 
#define RTL83XX_ACL_ACT_TABLE_LEN			RTL8367C_ACL_ACT_TABLE_LEN
#define RTL83XX_METERMAX					RTL8367C_METERMAX
#endif
#endif
#endif//CONFIG_RTL_83XX_ACL_SUPPORT

#endif//RTL819X_ACL_H
