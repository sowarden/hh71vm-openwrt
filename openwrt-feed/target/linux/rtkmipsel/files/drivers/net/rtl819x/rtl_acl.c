#ifdef __KERNEL__
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
#define CONFIG_RTL_PROC_NEW	1
#endif
#endif

#include <linux/fs.h>

#if defined(CONFIG_RTL_83XX_ACL_SUPPORT)
#include <asm/uaccess.h>
#if defined(CONFIG_RTL_8367R_SUPPORT)
#include "rtl8367r/rtk_types.h"
#include "rtl8367r/rtl8367b_asicdrv_acl.h"
#include "rtl8367r/rtk_api.h"
#include "rtl8367r/rtk_api_ext.h"
#else
#if defined(CONFIG_RTL_83XX_API_V1_4)
#include "rtl83xx_v1dot4/rtk_types.h"
#include "rtl83xx_v1dot4/dal/rtl8367c/rtl8367c_asicdrv_acl.h"
#include "rtl83xx_v1dot4/dal/rtl8367d/dal_rtl8367d_acl.h"
#include "rtl83xx_v1dot4/rtk_api.h"
#else
#include "rtl83xx/rtk_types.h"
#include "rtl83xx/rtl8367c_asicdrv_acl.h"
#include "rtl83xx/rtk_api.h"
#endif
extern rtk_uint32 vutpportmask;
#endif
#include "net/rtl/rtl_acl.h"
#endif

#if defined(CONFIG_RTL_83XX_ACL_SUPPORT)
static rtl83xx_acl_rule_header_t rtl83xx_acl_tbl;
static rtl83xx_acl_rule_header_t rtl83xx_free_acl_entry_header;
static rtl83xx_acl_rule_t *rtl83xx_acl_entry_pool=NULL;
rtk_uint8 free_ip_range_index_pool[RTL_RANGE_MAX] = {0};
rtk_uint8 free_vid_range_index_pool[RTL_RANGE_MAX] = {0};
rtk_uint8 free_port_range_index_pool[RTL_RANGE_MAX] = {0};
rtk_filter_field_t	filter_field[MAX_FILTER_NUM];

//check ok
static rtk_int32 _rtl83xx_initAclRulePool(void)
{
	rtk_int32 i;


	CLIST_INIT(&rtl83xx_free_acl_entry_header);

	TBL_MEM_ALLOC(rtl83xx_acl_entry_pool, rtl83xx_acl_rule_t, RTL83XX_ACLRULENO);
	
	if(rtl83xx_acl_entry_pool!=NULL)
	{
		memset(rtl83xx_acl_entry_pool, 0, RTL83XX_ACLRULENO * sizeof(rtl83xx_acl_rule_t));	
	}
	else
	{
		return FAILED;
	}
	
	for(i = 0; i<RTL83XX_ACLRULENO;i++)
	{
		CLIST_INSERT_HEAD(&rtl83xx_free_acl_entry_header, &rtl83xx_acl_entry_pool[i], next_rule);
	}
	
	return SUCCESS;
}
//check ok
static rtk_int32 _rtl83xx_isSameAcl(rtl83xx_acl_rule_t *rule1, rtl83xx_acl_rule_t *rule2, rtk_uint32 flag)
{
	rtk_int32 ret=1, checked=0;
	
	if(flag&COMPARE_FLAG_PRIO)
	{
		if(rule1->prio != rule2->prio)
			ret = 0;
		checked = 1;
	}
	if(flag&COMPARE_FLAG_MEMBER)
	{
		if(rule1->member != rule2->member)
			ret = 0;
		checked = 1;
	}

	if(flag&COMPARE_FLAG_INVERT)
	{
		if(rule1->invert != rule2->invert)
			ret = 0;
		checked = 1;
	}
	
	if(flag&COMPARE_FLAG_FILTER)
	{
		if(memcmp(&rule1->filter, &rule2->filter, sizeof(rtl83xx_acl_filter_t)))
			ret = 0;
		checked = 1;
	}
	
	if(flag&COMPARE_FLAG_ACTION)
	{
		if(memcmp(&rule1->action, &rule2->action, sizeof(rtl83xx_acl_action_t)))
			ret = 0;
		checked = 1;
	}

	if(checked)
		return ret;
	else
		return 0;
}
//check ok
static rtl83xx_acl_rule_t *_rtl83xx_allocAclRuleEntry(void)
{
	rtl83xx_acl_rule_t *newRule=NULL;
	newRule = CLIST_FIRST(&rtl83xx_free_acl_entry_header);
	if(newRule!=NULL)
	{
		CLIST_REMOVE(&rtl83xx_free_acl_entry_header, newRule, next_rule);
		memset(newRule,0,sizeof(rtl83xx_acl_rule_t));
	}
	return newRule;
}
//check ok
static rtk_int32 _rtl83xx_freeAclRuleEntry(rtl83xx_acl_rule_t *aclPtr)
{
	if(aclPtr==NULL)
	{
		return FAILED;
	}
	memset(aclPtr,0,sizeof(rtl83xx_acl_rule_t));
	CLIST_INSERT_HEAD(&rtl83xx_free_acl_entry_header, aclPtr, next_rule);
	
	return SUCCESS;
}
//check ok
static rtk_int32 _rtl83xx_flushAclRuleEntry(rtl83xx_acl_rule_header_t * aclChainHead)
{
	rtl83xx_acl_rule_t *curAcl,*nextAcl;
	
	if(aclChainHead==NULL)
	{
		return FAILED;
	}
	
	curAcl=CLIST_FIRST(aclChainHead);
	while(curAcl)
	{
		nextAcl=CLIST_NEXT(curAcl, next_rule);
		/*remove from the old descriptor chain*/
		CLIST_REMOVE(aclChainHead, curAcl, next_rule);
		/*return to the free descriptor chain*/
		_rtl83xx_freeAclRuleEntry(curAcl);
		curAcl = nextAcl;
	}

	return SUCCESS;
}
//check ok
static rtk_int32 _rtl83xx_flushAclRuleEntrybyPrio(rtl83xx_acl_rule_header_t *aclChainHead, rtk_int32 prio)
{
	rtl83xx_acl_rule_t *curAcl,*nextAcl;
	
	if(aclChainHead==NULL)
	{
		return FAILED;
	}
	
	curAcl=CLIST_FIRST(aclChainHead);
	while(curAcl)
	{
		nextAcl=CLIST_NEXT(curAcl, next_rule);
		if(curAcl->prio == prio)
		{
			CLIST_REMOVE(aclChainHead, curAcl, next_rule);
			_rtl83xx_freeAclRuleEntry(curAcl);
		}
		curAcl = nextAcl;
	}

	return SUCCESS;
}
static rtk_int32 _rtl83xx_flushAclRuleEntrybyFlag(rtl83xx_acl_rule_header_t *aclChainHead, rtk_uint32 flag, rtl83xx_acl_rule_t *compareAcl)
{
	rtl83xx_acl_rule_t *curAcl,*nextAcl;
	
	if(aclChainHead==NULL)
	{
		return FAILED;
	}
	
	curAcl=CLIST_FIRST(aclChainHead);
	while(curAcl)
	{
		nextAcl=CLIST_NEXT(curAcl, next_rule);
		if(_rtl83xx_isSameAcl(curAcl, compareAcl, flag))
		{
			CLIST_REMOVE(aclChainHead, curAcl, next_rule);
			_rtl83xx_freeAclRuleEntry(curAcl);
		}
		curAcl = nextAcl;
	}

	return SUCCESS;
}

//check ok
static rtk_int32 _rtl83xx_aclRuleEntryEnqueue(rtl83xx_acl_rule_header_t *aclChainHead,
												 rtl83xx_acl_rule_t *enqueueAcl)
{

	rtl83xx_acl_rule_t *newAcl;
	rtl83xx_acl_rule_t *curAcl,*nextAcl;
	rtl83xx_acl_rule_t *lastAcl;

	if(aclChainHead==NULL)
	{
		return FAILED;
	}
	
	if(enqueueAcl==NULL)
	{
		return FAILED;
	}

	for(curAcl=CLIST_FIRST(aclChainHead); curAcl!=NULL; curAcl=nextAcl)
	{

		nextAcl=CLIST_NEXT(curAcl, next_rule);

		if(_rtl83xx_isSameAcl(curAcl, enqueueAcl, COMPARE_FLAG_ALL))			
			break;
	}
	if(curAcl)
	{
		//already exist
		return SUCCESS;
	}
	
	newAcl=_rtl83xx_allocAclRuleEntry();
	
	if(newAcl!=NULL)
	{
		memcpy(newAcl, enqueueAcl,sizeof(rtl83xx_acl_rule_t ));
		newAcl->next_rule.le_next=NULL;
		newAcl->next_rule.le_prev=NULL;
	}
	else
	{
		/*no enough memory*/
		printk("not enough 83xx acl entry!\n");
		return FAILED;
	}
	
	for(curAcl=CLIST_FIRST(aclChainHead); curAcl!=NULL; curAcl=nextAcl)
	{

		nextAcl=CLIST_NEXT(curAcl, next_rule);
		lastAcl=curAcl;
		
		if(curAcl->prio > newAcl->prio)
		{	
			break;
		}
	}

	if(curAcl)
	{
		//find a entry has larger prio value(lower priority)
		CLIST_INSERT_BEFORE(aclChainHead,curAcl,newAcl,next_rule);
	}
	else
	{
		
		if(CLIST_EMPTY(aclChainHead))
		{
			CLIST_INSERT_HEAD(aclChainHead, newAcl, next_rule);
		}
		else
		{
			CLIST_INSERT_AFTER(aclChainHead,lastAcl,newAcl,next_rule);
		}
	}

	return SUCCESS;
	
}
//check ok
static rtk_int32 _rtl83xx_aclRuleEntryDequeue(rtl83xx_acl_rule_header_t *aclChainHead,
												 rtl83xx_acl_rule_t *dequeueAcl)
{
	rtl83xx_acl_rule_t *curAcl,*nextAcl;

	if(aclChainHead==NULL)
	{
		return FAILED;
	}
	
	if(dequeueAcl==NULL)
	{
		return FAILED;
	}	

	for(curAcl=CLIST_FIRST(aclChainHead); curAcl!=NULL; curAcl=nextAcl)
	{

		nextAcl=CLIST_NEXT(curAcl, next_rule);

		if(_rtl83xx_isSameAcl(curAcl, dequeueAcl, COMPARE_FLAG_ALL))			
			break;
	}

	if(curAcl)
	{
		//find the delete entry in aclChainHeadList
		CLIST_REMOVE(aclChainHead,curAcl,next_rule);
		_rtl83xx_freeAclRuleEntry(curAcl);
	}

	return SUCCESS;
	
}
//check ok
static rtk_int32 _rtl83xx_allocIpRangeIndex(void)
{
	rtk_int32 i;
	for(i=0; i<RTL83XX_ACLRANGEMAX; i++)
	{
		if(free_ip_range_index_pool[i]==0)
		{
			free_ip_range_index_pool[i]=1;
			return i;
		}
	}
	return -1;
}
//check ok
static rtk_int32 _rtl83xx_freeIpRangeIndex(rtk_uint8 ip_range_index)
{
	if(ip_range_index>=RTL83XX_ACLRANGEMAX)
		return FAILED;

	free_ip_range_index_pool[ip_range_index] = 0;
	rtk_filter_iprange_set(ip_range_index, IPRANGE_UNUSED, 0, 0);
	return SUCCESS;
}
//check ok
static rtk_int32 _rtl83xx_resetIpRangeIndex(void)
{
	rtk_uint32 index;
	memset(free_ip_range_index_pool, 0, RTL83XX_ACLRANGEMAX*sizeof(rtk_uint8));
	for(index=0; index<=RTL83XX_ACLRANGEMAX; index++)
	{
		rtk_filter_iprange_set(index, IPRANGE_UNUSED, 0, 0);
	}
	return SUCCESS;
}
//check ok
#if 0//complier error:defined but not used
static rtk_int32 _rtl83xx_allocVidRangeIndex(void)
{
	rtk_int32 i;
	for(i=0; i<RTL8367B_ACLRANGEMAX; i++)
	{
		if(free_vid_range_index_pool[i]==0)
		{
			free_vid_range_index_pool[i]=1;
			return i;
		}
	}
	return -1;
}
//check ok
static rtk_int32 _rtl83xx_freeVidRangeIndex(rtk_uint8 vid_range_index)
{
	if(vid_range_index>=RTL8367B_ACLRANGEMAX)
		return FAILED;

	free_vid_range_index_pool[vid_range_index] = 0;
	rtk_filter_vidrange_set(vid_range_index, VIDRANGE_UNUSED, 0, 0);
	return SUCCESS;
}
#endif
//check ok
static rtk_int32 _rtl83xx_resetVidRangeIndex(void)
{
	rtk_uint32 index=0;

	memset(free_vid_range_index_pool, 0, RTL83XX_ACLRANGEMAX*sizeof(rtk_uint8));
	for(index=0; index<=RTL83XX_ACLRANGEMAX; index++)
	{
		rtk_filter_vidrange_set(index, VIDRANGE_UNUSED, 0, 0);
	}
	return SUCCESS;
}
//check ok
static rtk_int32 _rtl83xx_allocPortRangeIndex(void)
{
	rtk_int32 i;
	for(i=0; i<RTL83XX_ACLRANGEMAX; i++)
	{
		if(free_port_range_index_pool[i]==0)
		{
			free_port_range_index_pool[i]=1;
			return i;
		}
	}
	return -1;
}
//check ok
static rtk_int32 _rtl83xx_freePortRangeIndex(rtk_uint8 port_range_index)
{
	if(port_range_index>=RTL83XX_ACLRANGEMAX)
		return FAILED;

	free_port_range_index_pool[port_range_index] = 0;
	rtk_filter_portrange_set(port_range_index, PORTRANGE_UNUSED, 0, 0);
	return SUCCESS;
}
//check ok
static rtk_int32 _rtl83xx_resetPortRangeIndex(void)
{
	rtk_uint32 index;
	memset(free_port_range_index_pool, 0, RTL83XX_ACLRANGEMAX*sizeof(rtk_uint8));
	for(index=0; index<=RTL83XX_ACLRANGEMAX; index++)
	{
		rtk_filter_portrange_set(index, PORTRANGE_UNUSED, 0, 0);
	}
	return SUCCESS;
}
//check ok
static rtk_filter_act_enable_t _rtl83xx_getActionType(rtk_uint32 type)
{
	rtk_filter_act_enable_t rtl83xx_type;
	if(type >= RTL83XX_ACL_END)
		return FILTER_ENACT_END;
	
	switch(type)
	{
		case RTL83XX_ACL_COPY_CPU:
			rtl83xx_type =  FILTER_ENACT_COPY_CPU;
			break;
		case RTL83XX_ACL_DROP:
			rtl83xx_type =  FILTER_ENACT_DROP;
			break;
		case RTL83XX_ACL_REDIRECT:
			rtl83xx_type =  FILTER_ENACT_REDIRECT;
			break;
		case RTL83XX_ACL_MIRROR:
			rtl83xx_type =  FILTER_ENACT_MIRROR;
			break;
		case RTL83XX_ACL_TRAP_CPU:
			rtl83xx_type =  FILTER_ENACT_TRAP_CPU;
			break;
		case RTL83XX_ACL_PRIORITY:
			rtl83xx_type =  FILTER_ENACT_PRIORITY;
			break;
		case RTL83XX_ACL_DSCP_REMARK:
			rtl83xx_type =  FILTER_ENACT_DSCP_REMARK;
			break;		
		case RTL83XX_ACL_1P_REMARK:
			rtl83xx_type =  FILTER_ENACT_1P_REMARK;
			break;
		case RTL83XX_ACL_POLICING0:
			rtl83xx_type =  FILTER_ENACT_POLICING_0;
			break;
		default:
			rtl83xx_type =  FILTER_ENACT_END;
			break;
	}

	return rtl83xx_type; 
			
}
//check ok
static rtk_int32 _rtl83xx_isValidFilterIndex(rtk_filter_id_t filter_id)
{
	if(filter_id<RTL83XX_ACLRULENO)
		return 1;
	else
		return 0;
}

/*	sync acl rule to asic
	input: aclRule->index...
	output: 	aclRule->size, aclRule->in_asic
*/
//check ok
static rtk_int32 _rtl83xx_syncAclRuleToAsic(rtl83xx_acl_rule_t *aclRule)
{
	rtk_api_ret_t retVal;
	//rtk_filter_field_t	filter_field[10];
	rtk_filter_cfg_t	cfg;
	rtk_filter_action_t act;
	rtk_filter_number_t filterNum=0;
	rtk_filter_act_enable_t act_enable;
	
	memset(filter_field, 0, MAX_FILTER_NUM*sizeof(rtk_filter_field_t));
	memset(&cfg, 0, sizeof(rtk_filter_cfg_t));
	memset(&act, 0, sizeof(rtk_filter_action_t));

	filterNum = 0;
		//all packets
	if((INVALID_MAC(aclRule->filter.smac_mask.octet))&&
	    (INVALID_MAC(aclRule->filter.dmac_mask.octet))&&
	    (aclRule->filter.ethertype_mask==0)&&
	    (aclRule->filter.dip_type==FILTER_NONE)&&
	    (aclRule->filter.sip_type==FILTER_NONE)&&
	    (aclRule->filter.ip_tos_mask==0)&&
	    (aclRule->filter.protocal_mask==0)&&
	    (aclRule->filter.sport_type==FILTER_NONE)&&
	    (aclRule->filter.dport_type==FILTER_NONE)&&
	    (aclRule->filter.vlanpri_mask==0))
	{
		filter_field[0].fieldType=FILTER_FIELD_DMAC;
		if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field[0])) != RT_ERR_OK)
			return retVal;
	}
	else
	{
		//1.src mac
		if(!INVALID_MAC(aclRule->filter.smac_mask.octet))
		{
			if(filterNum>=MAX_FILTER_NUM)
			{
				printk("too much filter!\n");
				return RT_ERR_OUT_OF_RANGE;
			}
			
			filter_field[filterNum].fieldType = FILTER_FIELD_SMAC;
			filter_field[filterNum].filter_pattern_union.smac.dataType = FILTER_FIELD_DATA_MASK;
			filter_field[filterNum].filter_pattern_union.smac.value.octet[0] = aclRule->filter.smac.octet[0];
			filter_field[filterNum].filter_pattern_union.smac.value.octet[1] = aclRule->filter.smac.octet[1];
			filter_field[filterNum].filter_pattern_union.smac.value.octet[2] = aclRule->filter.smac.octet[2];
			filter_field[filterNum].filter_pattern_union.smac.value.octet[3] = aclRule->filter.smac.octet[3];
			filter_field[filterNum].filter_pattern_union.smac.value.octet[4] = aclRule->filter.smac.octet[4];
			filter_field[filterNum].filter_pattern_union.smac.value.octet[5] = aclRule->filter.smac.octet[5];
		
			filter_field[filterNum].filter_pattern_union.smac.mask.octet[0] = aclRule->filter.smac_mask.octet[0];
			filter_field[filterNum].filter_pattern_union.smac.mask.octet[1] = aclRule->filter.smac_mask.octet[1];
			filter_field[filterNum].filter_pattern_union.smac.mask.octet[2] = aclRule->filter.smac_mask.octet[2];
			filter_field[filterNum].filter_pattern_union.smac.mask.octet[3] = aclRule->filter.smac_mask.octet[3];
			filter_field[filterNum].filter_pattern_union.smac.mask.octet[4] = aclRule->filter.smac_mask.octet[4];
			filter_field[filterNum].filter_pattern_union.smac.mask.octet[5] = aclRule->filter.smac_mask.octet[5];
			filter_field[filterNum].next = NULL;	

			if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field[filterNum])) != RT_ERR_OK)
				return retVal;

			filterNum++;
		}

		//2.dest mac
		if(!INVALID_MAC(aclRule->filter.dmac_mask.octet))
		{
			if(filterNum>=MAX_FILTER_NUM)
			{
				printk("too much filter!\n");
				return RT_ERR_OUT_OF_RANGE;
			}
			
			filter_field[filterNum].fieldType = FILTER_FIELD_DMAC;
			filter_field[filterNum].filter_pattern_union.dmac.dataType = FILTER_FIELD_DATA_MASK;
			filter_field[filterNum].filter_pattern_union.dmac.value.octet[0] = aclRule->filter.dmac.octet[0];
			filter_field[filterNum].filter_pattern_union.dmac.value.octet[1] = aclRule->filter.dmac.octet[1];
			filter_field[filterNum].filter_pattern_union.dmac.value.octet[2] = aclRule->filter.dmac.octet[2];
			filter_field[filterNum].filter_pattern_union.dmac.value.octet[3] = aclRule->filter.dmac.octet[3];
			filter_field[filterNum].filter_pattern_union.dmac.value.octet[4] = aclRule->filter.dmac.octet[4];
			filter_field[filterNum].filter_pattern_union.dmac.value.octet[5] = aclRule->filter.dmac.octet[5];
		
			filter_field[filterNum].filter_pattern_union.dmac.mask.octet[0] = aclRule->filter.dmac_mask.octet[0];
			filter_field[filterNum].filter_pattern_union.dmac.mask.octet[1] = aclRule->filter.dmac_mask.octet[1];
			filter_field[filterNum].filter_pattern_union.dmac.mask.octet[2] = aclRule->filter.dmac_mask.octet[2];
			filter_field[filterNum].filter_pattern_union.dmac.mask.octet[3] = aclRule->filter.dmac_mask.octet[3];
			filter_field[filterNum].filter_pattern_union.dmac.mask.octet[4] = aclRule->filter.dmac_mask.octet[4];
			filter_field[filterNum].filter_pattern_union.dmac.mask.octet[5] = aclRule->filter.dmac_mask.octet[5];
			filter_field[filterNum].next = NULL;	

			if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field[filterNum])) != RT_ERR_OK)
				return retVal;

			filterNum++;
		}

		//3.ethernet type
		if(aclRule->filter.ethertype_mask)
		{		
			if(filterNum>=MAX_FILTER_NUM)
			{
				printk("too much filter!\n");
				return RT_ERR_OUT_OF_RANGE;
			}
			
			filter_field[filterNum].fieldType = FILTER_FIELD_ETHERTYPE;
			filter_field[filterNum].filter_pattern_union.etherType.dataType = FILTER_FIELD_DATA_MASK;
			filter_field[filterNum].filter_pattern_union.etherType.value = aclRule->filter.ethertype;
			filter_field[filterNum].filter_pattern_union.etherType.mask = aclRule->filter.ethertype_mask;
			filter_field[filterNum].next = NULL;
			if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field[filterNum])) != RT_ERR_OK)
				return retVal;
			filterNum++;
		}
		
		//4.sip
		if(aclRule->filter.sip_type==FILTER_MASK && aclRule->filter.sip_mask)
		{
			if(filterNum>=MAX_FILTER_NUM)
			{
				printk("too much filter!\n");
				return RT_ERR_OUT_OF_RANGE;
			}
			filter_field[filterNum].fieldType = FILTER_FIELD_IPV4_SIP;
			filter_field[filterNum].filter_pattern_union.sip.dataType = FILTER_FIELD_DATA_MASK;
			filter_field[filterNum].filter_pattern_union.sip.value = aclRule->filter.sip;
			filter_field[filterNum].filter_pattern_union.sip.mask = aclRule->filter.sip_mask;
			filter_field[filterNum].next = NULL;
			if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field[filterNum])) != RT_ERR_OK)
				return retVal;
			filterNum++;
		}
		
		//5.dip
		if(aclRule->filter.dip_type==FILTER_MASK && aclRule->filter.dip_mask)
		{
			if(filterNum>=MAX_FILTER_NUM)
			{
				printk("too much filter!\n");
				return RT_ERR_OUT_OF_RANGE;
			}
			filter_field[filterNum].fieldType = FILTER_FIELD_IPV4_DIP;
			filter_field[filterNum].filter_pattern_union.dip.dataType = FILTER_FIELD_DATA_MASK;
			filter_field[filterNum].filter_pattern_union.dip.value = aclRule->filter.dip;
			filter_field[filterNum].filter_pattern_union.dip.mask = aclRule->filter.dip_mask;
			filter_field[filterNum].next = NULL;
			if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field[filterNum])) != RT_ERR_OK)
				return retVal;
			filterNum++;
		}

		//6.ip range
		if((aclRule->filter.sip_type==FILTER_RANGE)||(aclRule->filter.dip_type==FILTER_RANGE))
		{
			rtk_int32 range_sindex=-1, range_dindex=-1, range_mask=0;

			if(filterNum>=MAX_FILTER_NUM)
			{
				printk("too much filter!\n");
				return RT_ERR_OUT_OF_RANGE;
			}

			if(aclRule->filter.sip_type==FILTER_RANGE && aclRule->filter.sip<=aclRule->filter.sip_mask)
			{
				range_sindex = _rtl83xx_allocIpRangeIndex();
				if(range_sindex == -1)
					return RT_ERR_OUT_OF_RANGE;

				if ((retVal = rtk_filter_iprange_set(range_sindex, IPRANGE_IPV4_SIP, aclRule->filter.sip_mask, aclRule->filter.sip)) != RT_ERR_OK)
				{
					_rtl83xx_freeIpRangeIndex(range_sindex);
					return retVal;
				}
				range_mask |= 1<<range_sindex;
			}

			if(aclRule->filter.dip_type==FILTER_RANGE && aclRule->filter.dip<=aclRule->filter.dip_mask)
			{
				range_dindex = _rtl83xx_allocIpRangeIndex();

				if(range_dindex == -1)
					return RT_ERR_OUT_OF_RANGE;
			
				if ((retVal = rtk_filter_iprange_set(range_dindex, IPRANGE_IPV4_DIP, aclRule->filter.dip_mask, aclRule->filter.dip)) != RT_ERR_OK)
				{
					_rtl83xx_freeIpRangeIndex(range_dindex);
					return retVal;
				}
				range_mask |= 1<<range_dindex;
			}		

			if(range_mask==0)
			{
				printk("wrong ip range parameters!\n");
				return RT_ERR_INPUT;
			}

			filter_field[filterNum].fieldType = FILTER_FIELD_IP_RANGE;
			filter_field[filterNum].filter_pattern_union.inData.dataType = FILTER_FIELD_DATA_MASK;
			filter_field[filterNum].filter_pattern_union.inData.value = range_mask; //bit mask
			filter_field[filterNum].filter_pattern_union.inData.mask = range_mask;
			filter_field[filterNum].next = NULL;
			if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field[filterNum])) != RT_ERR_OK)
			{	
				if(range_sindex!=-1)
					_rtl83xx_freeIpRangeIndex(range_sindex);
				if(range_dindex!=-1)
					_rtl83xx_freeIpRangeIndex(range_dindex);
				return retVal;
			}
			filterNum++;
		}

		//7.ip tos(dhcp)
		if(aclRule->filter.ip_tos_mask)
		{
			if(filterNum>=MAX_FILTER_NUM)
			{
				printk("too much filter!\n");
				return RT_ERR_OUT_OF_RANGE;
			}
			filter_field[filterNum].fieldType = FILTER_FIELD_IPV4_TOS;
			filter_field[filterNum].filter_pattern_union.ipTos.dataType = FILTER_FIELD_DATA_MASK;
			filter_field[filterNum].filter_pattern_union.ipTos.value = aclRule->filter.ip_tos;
			filter_field[filterNum].filter_pattern_union.ipTos.mask = aclRule->filter.ip_tos_mask;
			filter_field[filterNum].next = NULL;
			if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field[filterNum])) != RT_ERR_OK)
				return retVal;
			filterNum++;
		}

		//8.protocol
		if(aclRule->filter.protocal_mask)
		{
			if(filterNum>=MAX_FILTER_NUM)
			{
				printk("too much filter!\n");
				return RT_ERR_OUT_OF_RANGE;
			}
			filter_field[filterNum].fieldType = FILTER_FIELD_IPV4_PROTOCOL;
			filter_field[filterNum].filter_pattern_union.protocol.dataType = FILTER_FIELD_DATA_MASK;
			filter_field[filterNum].filter_pattern_union.protocol.value = aclRule->filter.protocal;
			filter_field[filterNum].filter_pattern_union.protocol.mask = aclRule->filter.protocal_mask;
			filter_field[filterNum].next = NULL;
			if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field[filterNum])) != RT_ERR_OK)
				return retVal;
			filterNum++;
		}

		//9.src port
		if(aclRule->filter.sport_type==FILTER_MASK && aclRule->filter.sport_mask)
		{
			if(filterNum>=MAX_FILTER_NUM)
			{
				printk("too much filter!\n");
				return RT_ERR_OUT_OF_RANGE;
			}
			filter_field[filterNum].fieldType = FILTER_FIELD_UDP_SPORT;
			filter_field[filterNum].filter_pattern_union.udpSrcPort.dataType = FILTER_FIELD_DATA_MASK;
			filter_field[filterNum].filter_pattern_union.udpSrcPort.value = aclRule->filter.sport;
			filter_field[filterNum].filter_pattern_union.udpSrcPort.mask = aclRule->filter.sport_mask;
			filter_field[filterNum].next = NULL;
			if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field[filterNum])) != RT_ERR_OK)
				return retVal;
			filterNum++;
		}

		//10.dest port
		if(aclRule->filter.dport_type==FILTER_MASK && aclRule->filter.dport_mask)
		{
			if(filterNum>=MAX_FILTER_NUM)
			{
				printk("too much filter!\n");
				return RT_ERR_OUT_OF_RANGE;
			}
			filter_field[filterNum].fieldType = FILTER_FIELD_UDP_DPORT;
			filter_field[filterNum].filter_pattern_union.udpDstPort.dataType = FILTER_FIELD_DATA_MASK;
			filter_field[filterNum].filter_pattern_union.udpDstPort.value = aclRule->filter.dport;
			filter_field[filterNum].filter_pattern_union.udpDstPort.mask = aclRule->filter.dport_mask;
			filter_field[filterNum].next = NULL;
			if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field[filterNum])) != RT_ERR_OK)
				return retVal;
			filterNum++;
		}
		
		//11.port range
		if((aclRule->filter.sport_type==FILTER_RANGE) || (aclRule->filter.dport_type==FILTER_RANGE))
		{
			rtk_int32 range_sindex=-1, range_dindex=-1, range_mask=0;
			if(filterNum>=MAX_FILTER_NUM)
			{
				printk("too much filter!\n");
				return RT_ERR_OUT_OF_RANGE;
			}
			if((aclRule->filter.sport_type==FILTER_RANGE) && (aclRule->filter.sport<=aclRule->filter.sport_mask))
			{
				range_sindex = _rtl83xx_allocPortRangeIndex();
				if(range_sindex==-1)
					return RT_ERR_OUT_OF_RANGE;

				if ((retVal = rtk_filter_portrange_set(range_sindex, PORTRANGE_SPORT, aclRule->filter.sport_mask, aclRule->filter.sport)) != RT_ERR_OK)
				{
					_rtl83xx_freePortRangeIndex(range_sindex);
					return retVal;
				}
				range_mask |= 1<<range_sindex;
			}

			if((aclRule->filter.dport_type==FILTER_RANGE) && (aclRule->filter.dport<=aclRule->filter.dport_mask))
			{
				range_dindex = _rtl83xx_allocPortRangeIndex();
				if(range_dindex==-1)
					return RT_ERR_OUT_OF_RANGE;

				if ((retVal = rtk_filter_portrange_set(range_dindex, PORTRANGE_DPORT, aclRule->filter.dport_mask, aclRule->filter.dport)) != RT_ERR_OK)
				{
					_rtl83xx_freePortRangeIndex(range_dindex);
					return retVal;
				}
				range_mask |= 1<<range_dindex;
			}

			if(range_mask==0)
			{
				printk("wrong port range parameters!\n");
				return RT_ERR_INPUT;
			}

			filter_field[filterNum].fieldType = FILTER_FIELD_PORT_RANGE;
			filter_field[filterNum].filter_pattern_union.inData.dataType = FILTER_FIELD_DATA_MASK;
			filter_field[filterNum].filter_pattern_union.inData.value = range_mask; //bit mask
			filter_field[filterNum].filter_pattern_union.inData.mask = range_mask;
			filter_field[filterNum].next = NULL;
			if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field[filterNum])) != RT_ERR_OK)
			{	
				if(range_sindex!=-1)
					_rtl83xx_freePortRangeIndex(range_sindex);
				if(range_dindex!=-1)
					_rtl83xx_freePortRangeIndex(range_dindex);
					
				return retVal;
			}
			filterNum++;

		}

		//12.1p
		if(aclRule->filter.vlanpri_mask)
		{
			if(filterNum>=MAX_FILTER_NUM)
			{
				printk("too much filter!\n");
				return RT_ERR_OUT_OF_RANGE;
			}
			filter_field[filterNum].fieldType = FILTER_FIELD_CTAG;
			filter_field[filterNum].filter_pattern_union.ctag.pri.dataType=FILTER_FIELD_DATA_MASK;
			filter_field[filterNum].filter_pattern_union.ctag.pri.value=aclRule->filter.vlanpri;
			filter_field[filterNum].filter_pattern_union.ctag.pri.mask=aclRule->filter.vlanpri_mask;
			filter_field[filterNum].next = NULL;
			if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field[filterNum])) != RT_ERR_OK)
				return retVal;
			filterNum++;
		}
	}

	#if defined(CONFIG_RTL_8367R_SUPPORT)
	cfg.activeport.dataType = FILTER_FIELD_DATA_MASK;
	cfg.activeport.value = aclRule->member;
	cfg.activeport.mask = 0xFF;
	#else
	cfg.activeport.value.bits[0] = aclRule->member;
	#if defined(CONFIG_RTL_83XX_API_V1_4)
	cfg.activeport.mask.bits[0] = 0x3001F; //logical port, include logical port 16/17
	#else
	cfg.activeport.mask.bits[0] = vutpportmask;
	#endif
	#endif
	cfg.invert = aclRule->invert;

	act_enable = _rtl83xx_getActionType(aclRule->action.act_type);
	if(act_enable==FILTER_ENACT_END)
	{
		//printk("not support the acl type yet!")
		return RT_ERR_INPUT;
	}
	act.actEnable[act_enable] = TRUE;
	if(act_enable==FILTER_ENACT_PRIORITY)
		act.filterPriority = aclRule->action.priority;
	else if(act_enable==FILTER_ENACT_POLICING_0){
	#if defined(CONFIG_RTL_83XX_SUPPORT)
		if ((retVal = rtk_rate_shareMeter_set(aclRule->action.meter_index, METER_TYPE_KBPS, aclRule->action.policing, 1)) != RT_ERR_OK)
	#else
		if ((retVal = rtl8367b_setAsicShareMeter(aclRule->action.meter_index, aclRule->action.policing >> 3, 1)) != RT_ERR_OK)
	#endif
		{
			printk("[%s:%d] retVal: %d\n", __FUNCTION__, __LINE__, retVal);
			return retVal;
		}
		act.filterPolicingIdx[0] = aclRule->action.meter_index;
	}

//	printk("index:%d, [%s:%d]\n", aclRule->index, __FUNCTION__, __LINE__);
	if ((retVal = rtk_filter_igrAcl_cfg_add(aclRule->index, &cfg, &act, &aclRule->size)) != RT_ERR_OK)
	{
		//printk("retVal:%d, [%s:%d]\n" , retVal, __FUNCTION__, __LINE__);
		return retVal;
	}

	aclRule->in_asic = 1;
	
	return RT_ERR_OK;

}

rtk_int32 rtl83xx_init_acl(void)
{
	CLIST_INIT(&rtl83xx_acl_tbl);
	_rtl83xx_initAclRulePool();
	_rtl83xx_resetIpRangeIndex();
	_rtl83xx_resetPortRangeIndex();
	_rtl83xx_resetVidRangeIndex();
	return 1;
}

rtk_int32 rtl83xx_addAclRule(rtl83xx_acl_rule_t *aclRule)
{
	return _rtl83xx_aclRuleEntryEnqueue(&rtl83xx_acl_tbl, aclRule);
}

rtk_int32 rtl83xx_deleteAclRule(rtl83xx_acl_rule_t *aclRule)
{
	return _rtl83xx_aclRuleEntryDequeue(&rtl83xx_acl_tbl, aclRule);
}

rtk_int32 rtl83xx_flushAclRule(void)
{
	return _rtl83xx_flushAclRuleEntry(&rtl83xx_acl_tbl);
}

rtk_int32 rtl83xx_flushAclRulebyPrio(rtk_int32 prio)
{
	return _rtl83xx_flushAclRuleEntrybyPrio(&rtl83xx_acl_tbl, prio);
}
rtk_int32 rtl83xx_flushAclRulebyFlag(rtk_uint32 flag, rtl83xx_acl_rule_t *compareAcl)
{
	return _rtl83xx_flushAclRuleEntrybyFlag(&rtl83xx_acl_tbl, flag, compareAcl);
}

rtk_int32 rtl83xx_syncAclTblToAsic(void)
{
	rtl83xx_acl_rule_t *curAcl;
	rtk_uint32 curfilter = 0;

	rtk_filter_igrAcl_cfg_delAll();
	_rtl83xx_resetPortRangeIndex();
	_rtl83xx_resetVidRangeIndex();
	_rtl83xx_resetIpRangeIndex();

	
	CLIST_FOREACH(curAcl,&rtl83xx_acl_tbl,next_rule)
	{
		if(_rtl83xx_isValidFilterIndex(curfilter)==0)
			return FAILED;
		
		curAcl->index = curfilter;
		curAcl->in_asic = 0;
		curAcl->size = 0;
		
		_rtl83xx_syncAclRuleToAsic(curAcl);
		if(curAcl->in_asic)
		{
			curfilter += curAcl->size;
		}
		else
			return FAILED;
	}

	return SUCCESS;
}
int rtl83xx_acl_test(void)
{
	int retVal;
	rtk_filter_field_t	filter_field[2];
	rtk_filter_cfg_t	cfg;
	rtk_filter_action_t act;
	rtk_filter_number_t ruleNum = 0;
	
	memset(filter_field, 0, 2*sizeof(rtk_filter_field_t));
	memset(&cfg, 0, sizeof(rtk_filter_cfg_t));
	memset(&act, 0, sizeof(rtk_filter_action_t));

#if 0
	//mac test
	filter_field[0].fieldType = FILTER_FIELD_DMAC;
	filter_field[0].filter_pattern_union.dmac.dataType = FILTER_FIELD_DATA_MASK;
	filter_field[0].filter_pattern_union.dmac.value.octet[0] = 0xF0;
	filter_field[0].filter_pattern_union.dmac.value.octet[1] = 0x1F;
	filter_field[0].filter_pattern_union.dmac.value.octet[2] = 0xAF;
	filter_field[0].filter_pattern_union.dmac.value.octet[3] = 0x67;
	filter_field[0].filter_pattern_union.dmac.value.octet[4] = 0x71;
	filter_field[0].filter_pattern_union.dmac.value.octet[5] = 0x99;
	
	filter_field[0].filter_pattern_union.dmac.mask.octet[0] = 0xFF;
	filter_field[0].filter_pattern_union.dmac.mask.octet[1] = 0xFF;
	filter_field[0].filter_pattern_union.dmac.mask.octet[2] = 0xFF;
	filter_field[0].filter_pattern_union.dmac.mask.octet[3] = 0xFF;
	filter_field[0].filter_pattern_union.dmac.mask.octet[4] = 0xFF;
	filter_field[0].filter_pattern_union.dmac.mask.octet[5] = 0xFF;
	filter_field[0].next = NULL;	

	if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field[0])) != RT_ERR_OK)
		return retVal;

	//sip
	filter_field[0].fieldType = FILTER_FIELD_IPV4_SIP;
	filter_field[0].filter_pattern_union.sip.dataType = FILTER_FIELD_DATA_MASK;
	filter_field[0].filter_pattern_union.sip.value = 0xc0a80196;
	filter_field[0].filter_pattern_union.sip.mask = 0xFFFFFFFF;
	filter_field[0].next = NULL;
	if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field[1])) != RT_ERR_OK)
		return retVal;

	//dhcp
	filter_field[0].fieldType = FILTER_FIELD_IPV4_TOS;
	filter_field[0].filter_pattern_union.ipTos.dataType = FILTER_FIELD_DATA_MASK;
	filter_field[0].filter_pattern_union.ipTos.value = 0xF0;
	filter_field[0].filter_pattern_union.ipTos.mask = 0xFC;
	filter_field[0].next = NULL;
	if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field[2])) != RT_ERR_OK)
		return retVal;

	//protocol
	filter_field[0].fieldType = FILTER_FIELD_IPV4_PROTOCOL;
	filter_field[0].filter_pattern_union.protocol.dataType = FILTER_FIELD_DATA_MASK;
	filter_field[0].filter_pattern_union.protocol.value = 0x11;
	filter_field[0].filter_pattern_union.protocol.mask = 0xFF;
	filter_field[0].next = NULL;
	if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field[3])) != RT_ERR_OK)
		return retVal;

	//port	
	/*if you want to identify tcp port or udp port ,the protocol must be used at the same time.
	  or use cared tag*/
	filter_field[0].fieldType = FILTER_FIELD_UDP_SPORT;
	filter_field[0].filter_pattern_union.udpSrcPort.dataType = FILTER_FIELD_DATA_MASK;
	filter_field[0].filter_pattern_union.udpSrcPort.value = 2000;
	filter_field[0].filter_pattern_union.udpSrcPort.mask = 0xFFFF;
	filter_field[0].next = NULL;
	if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field[4])) != RT_ERR_OK)
		return retVal;
#endif

#if 0
	//dip range
	/*inData.mask indicates the bits cared.
	   if the bit is 1, the packets within the ip range are matched, 
	   otherwise the packets beyond the ip range are matched*/

	ipaddr_t lowerip, upperip;
	lowerip = 0xc0a801a0;
	upperip = 0xc0a801b0;
	if ((retVal = rtk_filter_iprange_set(1, IPRANGE_IPV4_DIP, upperip, lowerip)) != RT_ERR_OK)
	{
		return retVal;
	}
	filter_field[0].fieldType = FILTER_FIELD_IP_RANGE;
	filter_field[0].filter_pattern_union.inData.dataType = FILTER_FIELD_DATA_MASK;
	filter_field[0].filter_pattern_union.inData.value = 0x0002; //bit mask
	filter_field[0].filter_pattern_union.inData.mask = 0xFFFF;
	filter_field[0].next = NULL;
	if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field[5])) != RT_ERR_OK)
	{	
		return retVal;
	}
#endif

#if 1

	filter_field[0].fieldType = FILTER_FIELD_CTAG;
	filter_field[0].filter_pattern_union.ctag.pri.dataType=FILTER_FIELD_DATA_MASK;
	filter_field[0].filter_pattern_union.ctag.pri.value=7;
	filter_field[0].filter_pattern_union.ctag.pri.mask=0xF;
	
	filter_field[0].next = NULL;
	if ((retVal = rtk_filter_igrAcl_field_add(&cfg, &filter_field[0])) != RT_ERR_OK)
		return retVal;

#endif
	
	#if defined(CONFIG_RTL_8367R_SUPPORT)
	cfg.activeport.dataType = FILTER_FIELD_DATA_MASK;
	cfg.activeport.value = 0x1F;
	cfg.activeport.mask = 0xFF;
	#else
	cfg.activeport.value.bits[0] = vutpportmask;
	#if defined(CONFIG_RTL_83XX_API_V1_4)
	cfg.activeport.mask.bits[0] = 0x3001F; //logical port, include logical port 16/17
	#else
	cfg.activeport.mask.bits[0] = vutpportmask;
	#endif
	#endif
	cfg.invert = FALSE;

	act.actEnable[FILTER_ENACT_DROP] = TRUE;

	if ((retVal = rtk_filter_igrAcl_cfg_add(2, &cfg, &act, &ruleNum)) != RT_ERR_OK)
	{
		printk("retVal:%x, [%s:%d]\n", retVal, __FUNCTION__, __LINE__);
		return retVal;
	}

	printk("ruleNum:%d, [%s %d]\n", ruleNum, __func__, __LINE__);
	return RT_ERR_OK;

}

/*for default acl filter selector*/
const unsigned char *type_string[FILTER_FIELD_RAW_END+1] = {
	"unused", 
	"dmac(0-15)",
	"dmac(16-31)",
	"dmac(32-47)",
	"smac(0-15)",
	"smac(16-31)",
	"smac(32-47)",
	"eth type",
	"stag",
	"ctag",
	"","","","","","",
	"v4 sip(0-15)",
	"v4 sip(16-31)",
	"v4 dip(0-15)",
	"v4 dip(16-31)",
 	"","","","","","","","","","","","",
	"v6 sip(0-15)",
	"v6 sip(16-31)",
	"","","","","","",
	"v6 dip(0-15)",
	"v6 dip(16-31)",
	"","","","","","",
	"vid range",
	"ip range",
	"port range",
	"vaild",
	"","","","","","","","","","","","",
	"select00",
	"select01", 
	"select02", 
	"select03", 
	"select04", 
	"select05", 
	"select06", 
	"select07", 
	"select08",
	"select09",
	"select10", 
	"select11", 
	"select12", 
	"select13", 
	"select14",
	"select15",
	"end"
};

const unsigned char *action_string[FILTER_ENACT_END+1] = {
#if defined(CONFIG_RTL_8367R_SUPPORT)
	"cvlan index",
	"cvlan vid",
	"cvlan ingress",
	"cvlan egress",
	"cvlan svid",
	"policing1",
	"svlan index",
	"svlan ingress",
	"svlan egress",
	"svlan cvid",
	"policing2",
	"policing0",
	"copy cpu",
	"drop",
	"add dst port",
	"redirect",
	"mirror",
	"trap to cpu",
	"priority",
	"dscp remark",
	"1p remark",
	"policing3",
	"interrupt",
	"gpo",
#else
	"cvlan ingress",
	"cvlan egress",
	"cvlan svid",
	"policing1",
	"svlan ingress",
	"svlan egress",
	"svlan cvid",
	"policing2",
	"policing0",
	"copy cpu",
	"drop",
	"add dst port",
	"redirect",
	"mirror",
	"trap to cpu",
	"isolation",
	"priority",
	"dscp remark",
	"1p remark",
	"policing3",
	"interrupt",
	"gpo",
	"egress ctag untag",
	"egress ctag tag",
	"egress ctag keep",
	"egress ctag keep and 1prmk",
#endif
	"end"	
};
const unsigned char *iprange_string[IPRANGE_END+1] = {
	"unused",
	"ipv4 sip",
	"ipv4 dip",
	"ipv6 sip",
	"ipv6 dip",
	"end"	
};
const unsigned char *portrange_string[PORTRANGE_END+1] = {
	"unused",
	"sport",
	"dport",
	"end"	
};
const unsigned char *vidrange_string[VIDRANGE_END+1] = {
	"unused",
	"cvid",
	"svid",
	"end"	
};

#if defined (CONFIG_RTL_PROC_NEW)
int rtl_83xxACLReadProc(struct seq_file *s, void *v)
{
	rtk_filter_id_t filter_id;
	rtk_int32 range_id;
	rtk_filter_act_enable_t i;
	for(filter_id=0; filter_id<=RTL83XX_ACLRULEMAX; filter_id++)
	{
		rtk_filter_cfg_raw_t pFilter_cfg;
		rtk_filter_action_t pAction;
		
		rtk_filter_igrAcl_cfg_get(filter_id, &pFilter_cfg, &pAction);
		{
			if(pFilter_cfg.valid)
			{	
				seq_printf(s,"[%d]", filter_id);
				seq_printf(s,"\tActivePort:%x, PMask:%x, Invert:%d\n", pFilter_cfg.activeport.value, pFilter_cfg.activeport.mask, pFilter_cfg.invert);
				seq_printf(s,"\tData:\t%4x\t%4x\t%4x\t%4x\t%4x\t%4x\t%4x\t%4x\n", 
					pFilter_cfg.dataFieldRaw[0],
					pFilter_cfg.dataFieldRaw[1],
					pFilter_cfg.dataFieldRaw[2],
					pFilter_cfg.dataFieldRaw[3],
					pFilter_cfg.dataFieldRaw[4],
					pFilter_cfg.dataFieldRaw[5],
					pFilter_cfg.dataFieldRaw[6],
					pFilter_cfg.dataFieldRaw[7]);
				seq_printf(s,"\tDMask:\t%4x\t%4x\t%4x\t%4x\t%4x\t%4x\t%4x\t%4x\n", 
					pFilter_cfg.careFieldRaw[0],
					pFilter_cfg.careFieldRaw[1],
					pFilter_cfg.careFieldRaw[2],
					pFilter_cfg.careFieldRaw[3],
					pFilter_cfg.careFieldRaw[4],
					pFilter_cfg.careFieldRaw[5],
					pFilter_cfg.careFieldRaw[6],
					pFilter_cfg.careFieldRaw[7]);
				seq_printf(s,"\ttype:\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
					type_string[pFilter_cfg.fieldRawType[0]],
					type_string[pFilter_cfg.fieldRawType[1]],
					type_string[pFilter_cfg.fieldRawType[2]],
					type_string[pFilter_cfg.fieldRawType[3]],
					type_string[pFilter_cfg.fieldRawType[4]],
					type_string[pFilter_cfg.fieldRawType[5]],
					type_string[pFilter_cfg.fieldRawType[6]],
					type_string[pFilter_cfg.fieldRawType[7]]);
				seq_printf(s, "\tAction:");
				for(i=0; i<FILTER_ENACT_END; i++)
				{
					if(pAction.actEnable[i])
					{
						seq_printf(s, "%s ", action_string[i]);
						if(i==FILTER_ENACT_PRIORITY)
							seq_printf(s, "%d ", pAction.filterPriority);
						else if(i == FILTER_ENACT_POLICING_0)
							seq_printf(s, "meter %d ", pAction.filterPolicingIdx[0]);
						/*to do:other type paramater*/
					}
				}
				seq_printf(s, "\n");
			}
		}
	}
	
	seq_printf(s, "vid range list:\n");
	for(range_id=0; range_id<=RTL83XX_ACLRANGEMAX; range_id++)
	{
		rtk_filter_vidrange_t pType;
		rtk_uint32 pUpperVid, pLowerVid;

		if((rtk_filter_vidrange_get(range_id, &pType, &pUpperVid, &pLowerVid)) == RT_ERR_OK)
			if(pType>VIDRANGE_UNUSED && pType<VIDRANGE_END)
				seq_printf(s,"\tvid range:%d, type:%s, upper:%d, lower:%d\n", 1<<range_id, vidrange_string[pType], pUpperVid, pLowerVid);

	}

	seq_printf(s, "port range list:\n");
	for(range_id=0; range_id<=RTL83XX_ACLRANGEMAX; range_id++)
	{
		rtk_filter_portrange_t pType;
		rtk_uint32 pUpperPort, pLowerPort;

		if((rtk_filter_portrange_get(range_id, &pType, &pUpperPort, &pLowerPort)) == RT_ERR_OK)
			if(pType>PORTRANGE_UNUSED && pType<PORTRANGE_END)
				seq_printf(s,"\tport range:%d, type:%s, upper:%x, lower:%x\n", 1<<range_id, portrange_string[pType], pUpperPort, pLowerPort);
	}

	seq_printf(s, "ip range list:\n");
	for(range_id=0; range_id<=RTL83XX_ACLRANGEMAX; range_id++)
	{
		rtk_filter_iprange_t pType;
		ipaddr_t pUpperIp[4], pLowerIp[4];

		rtk_filter_iprange_get(range_id, &pType, pUpperIp, pLowerIp);

		if(pType>IPRANGE_UNUSED && pType<=IPRANGE_IPV4_DIP)
		{
			seq_printf(s, "\tip range:%d, type:%s, upper:%x, lower:%x\n", 1<<range_id, iprange_string[pType], pUpperIp[0], pLowerIp[0]);
		}
		else if(pType>IPRANGE_IPV4_DIP && pType<IPRANGE_END)
		{
			seq_printf(s, "\tip range:%d, type:%s, upper:%x-%x-%x-%x, lower:%x-%x-%x-%x",
				1<<range_id, iprange_string[pType], 
				pUpperIp[0], pUpperIp[1], pUpperIp[2], pUpperIp[3],
				pLowerIp[0], pLowerIp[1], pLowerIp[2], pLowerIp[3]);
		}
		else
		{
		}
	}
	return 0;
}
int rtl_83xxSwACLReadProc(struct seq_file *s, void *v)
{
	rtl83xx_acl_rule_t *curAcl;
	rtk_int32 cnt=0;
	
	unsigned char *actionT[RTL83XX_ACL_END] = 
		{"copy cpu",
		 "drop",
		 "redirect",
		 "mirror",
		 "trap to cpu",
		 "priority",
		 "dscp remark",
		 "1p remark",
		 "policing0"
		};
	unsigned char *filterT[FILTER_END]=
		{"none",
		 "mask",
		 "range"};
	
	seq_printf(s, "rtl8367 sw acl rule:\n");	
	CLIST_FOREACH(curAcl,&rtl83xx_acl_tbl,next_rule)
	{
		
		seq_printf(s, "[%d]\tprio:%d, member:%x, invert:%d, size:%d, in_asic:%d, index:%d\n", 
			cnt, curAcl->prio, curAcl->member, curAcl->invert, 
			curAcl->size, curAcl->in_asic, curAcl->index);

		seq_printf(s,"\tether type: %x	 ether type mask: %x\n", curAcl->filter.ethertype, curAcl->filter.ethertype_mask);
		seq_printf(s,"\tvlan prio: %x	 vlan prio mask: %x\n", curAcl->filter.vlanpri, curAcl->filter.vlanpri_mask);
		seq_printf(s,"\tSMAC: %x:%x:%x:%x:%x:%x  SMACM: %x:%x:%x:%x:%x:%x\n",
			curAcl->filter.smac.octet[0],curAcl->filter.smac.octet[1],curAcl->filter.smac.octet[2],
			curAcl->filter.smac.octet[3],curAcl->filter.smac.octet[4],curAcl->filter.smac.octet[5],
			curAcl->filter.smac_mask.octet[0],curAcl->filter.smac_mask.octet[1],curAcl->filter.smac_mask.octet[2],
			curAcl->filter.smac_mask.octet[3],curAcl->filter.smac_mask.octet[4],curAcl->filter.smac_mask.octet[5]);
		seq_printf(s,"\tDMAC: %x:%x:%x:%x:%x:%x  DMACM: %x:%x:%x:%x:%x:%x\n",
			curAcl->filter.dmac.octet[0],curAcl->filter.dmac.octet[1],curAcl->filter.dmac.octet[2],
			curAcl->filter.dmac.octet[3],curAcl->filter.dmac.octet[4],curAcl->filter.dmac.octet[5],
			curAcl->filter.dmac_mask.octet[0],curAcl->filter.dmac_mask.octet[1],curAcl->filter.dmac_mask.octet[2],
			curAcl->filter.dmac_mask.octet[3],curAcl->filter.dmac_mask.octet[4],curAcl->filter.dmac_mask.octet[5]);
		seq_printf(s,"\tip filter type:%s sip: %d.%d.%d.%d sipM: %d.%d.%d.%d\n", 
			filterT[curAcl->filter.sip_type],
			(curAcl->filter.sip>>24),((curAcl->filter.sip&0x00ff0000)>>16), 
			((curAcl->filter.sip&0x0000ff00)>>8),(curAcl->filter.sip&0xff), 
			(curAcl->filter.sip_mask>>24), ((curAcl->filter.sip_mask&0x00ff0000)>>16),
			((curAcl->filter.sip_mask&0x0000ff00)>>8), (curAcl->filter.sip_mask&0xff));
		seq_printf(s,"\tip filter type:%s dip: %d.%d.%d.%d dipM: %d.%d.%d.%d\n", 
			filterT[curAcl->filter.dip_type],
			(curAcl->filter.dip>>24),((curAcl->filter.dip&0x00ff0000)>>16), 
			((curAcl->filter.dip&0x0000ff00)>>8),(curAcl->filter.dip&0xff), 
			(curAcl->filter.dip_mask>>24), ((curAcl->filter.dip_mask&0x00ff0000)>>16),
			((curAcl->filter.dip_mask&0x0000ff00)>>8), (curAcl->filter.dip_mask&0xff));
		seq_printf(s,"\tTos: %x   TosM: %x   ipProto: %x   ipProtoM: %x\n",
			curAcl->filter.ip_tos, curAcl->filter.ip_tos_mask, curAcl->filter.protocal, curAcl->filter.protocal_mask);
		seq_printf(s,"\tSport filter type:%s Sport:%x   SportM:%x\n",
			filterT[curAcl->filter.sport_type],
			curAcl->filter.sport, curAcl->filter.sport_mask);
		seq_printf(s,"\tDport filter type:%s Dport:%x   DportM:%x\n",
			filterT[curAcl->filter.dport_type],
			curAcl->filter.dport, curAcl->filter.dport_mask);
		seq_printf(s,"\tAction:%s",actionT[curAcl->action.act_type]);
		if(curAcl->action.act_type==RTL83XX_ACL_PRIORITY)
			seq_printf(s,"\t%d\n",curAcl->action.priority);
		else if(curAcl->action.act_type == RTL83XX_ACL_POLICING0)
			seq_printf(s,"\tmeter: %d rate: %d\n", curAcl->action.meter_index, curAcl->action.policing);
		else
			seq_printf(s, "\n");
		cnt++;
	}		
	return 0;
}
#else
int rtl_83xxACLReadProc(char *page, char **start, off_t off,
		     int count, int *eof, void *data)
{
	rtk_filter_id_t filter_id;
	rtk_int32 range_id;
	rtk_filter_act_enable_t i;	
	int len=0;
	for(filter_id=0; filter_id<=RTL83XX_ACLRULEMAX; filter_id++)
	{
		rtk_filter_cfg_raw_t pFilter_cfg;
		rtk_filter_action_t pAction;
		
		rtk_filter_igrAcl_cfg_get(filter_id, &pFilter_cfg, &pAction);
		{
			if(pFilter_cfg.valid)
			{	
				len += sprintf(page+len,"[%d]", filter_id);
				len += sprintf(page+len,"\tActivePort:%x, PMask:%x, Invert:%d\n", pFilter_cfg.activeport.value, pFilter_cfg.activeport.mask, pFilter_cfg.invert);
				len += sprintf(page+len,"\tData:\t%4x\t%4x\t%4x\t%4x\t%4x\t%4x\t%4x\t%4x\n", 
					pFilter_cfg.dataFieldRaw[0],
					pFilter_cfg.dataFieldRaw[1],
					pFilter_cfg.dataFieldRaw[2],
					pFilter_cfg.dataFieldRaw[3],
					pFilter_cfg.dataFieldRaw[4],
					pFilter_cfg.dataFieldRaw[5],
					pFilter_cfg.dataFieldRaw[6],
					pFilter_cfg.dataFieldRaw[7]);
				len += sprintf(page+len,"\tDMask:\t%4x\t%4x\t%4x\t%4x\t%4x\t%4x\t%4x\t%4x\n", 
					pFilter_cfg.careFieldRaw[0],
					pFilter_cfg.careFieldRaw[1],
					pFilter_cfg.careFieldRaw[2],
					pFilter_cfg.careFieldRaw[3],
					pFilter_cfg.careFieldRaw[4],
					pFilter_cfg.careFieldRaw[5],
					pFilter_cfg.careFieldRaw[6],
					pFilter_cfg.careFieldRaw[7]);
				len += sprintf(page+len,"\ttype:\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
					type_string[pFilter_cfg.fieldRawType[0]],
					type_string[pFilter_cfg.fieldRawType[1]],
					type_string[pFilter_cfg.fieldRawType[2]],
					type_string[pFilter_cfg.fieldRawType[3]],
					type_string[pFilter_cfg.fieldRawType[4]],
					type_string[pFilter_cfg.fieldRawType[5]],
					type_string[pFilter_cfg.fieldRawType[6]],
					type_string[pFilter_cfg.fieldRawType[7]]);
				len += sprintf(page+len, "\tAction:");
				for(i=0; i<FILTER_ENACT_END; i++)
				{
					if(pAction.actEnable[i])
					{
						len += sprintf(page+len, "%s ", action_string[i]);
						if(i==FILTER_ENACT_PRIORITY)
							len += sprintf(page+len, "%d ", pAction.filterPriority);
						/*to do:other type paramater*/
					}
				}
				len += sprintf(page+len, "\n");
			}
		}
	}
	
	len += sprintf(page+len, "vid range list:\n");
	for(range_id=0; range_id<=RTL83XX_ACLRANGEMAX; range_id++)
	{
		rtk_filter_vidrange_t pType;
		rtk_uint32 pUpperVid, pLowerVid;

		rtk_filter_vidrange_get(range_id, &pType, &pUpperVid, &pLowerVid);
		
		if(pType>VIDRANGE_UNUSED && pType<VIDRANGE_END)
			len += sprintf(page+len,"\tvid range:%d, type:%s, upper:%d, lower:%d\n", 1<<range_id, vidrange_string[pType], pUpperVid, pLowerVid);

	}

	len += sprintf(page+len, "port range list:\n");
	for(range_id=0; range_id<=RTL83XX_ACLRANGEMAX; range_id++)
	{
		rtk_filter_portrange_t pType;
		rtk_uint32 pUpperPort, pLowerPort;

		rtk_filter_portrange_get(range_id, &pType, &pUpperPort, &pLowerPort);
		
		if(pType>PORTRANGE_UNUSED && pType<PORTRANGE_END)
			len += sprintf(page+len,"\tport range:%d, type:%s, upper:%x, lower:%x\n", 1<<range_id, portrange_string[pType], pUpperPort, pLowerPort);
	}

	len += sprintf(page+len, "ip range list:\n");
	for(range_id=0; range_id<=RTL83XX_ACLRANGEMAX; range_id++)
	{
		rtk_filter_iprange_t pType;
		ipaddr_t pUpperIp[4], pLowerIp[4];

		rtk_filter_iprange_get(range_id, &pType, pUpperIp, pLowerIp);

		if(pType>IPRANGE_UNUSED && pType<=IPRANGE_IPV4_DIP)
		{
			len += sprintf(page+len, "\tip range:%d, type:%s, upper:%x, lower:%x\n", 1<<range_id, iprange_string[pType], pUpperIp[0], pLowerIp[0]);
		}
		else if(pType>IPRANGE_IPV4_DIP && pType<IPRANGE_END)
		{
			len += sprintf(page+len, "\tip range:%d, type:%s, upper:%x-%x-%x-%x, lower:%x-%x-%x-%x",
				1<<range_id, iprange_string[pType], 
				pUpperIp[0], pUpperIp[1], pUpperIp[2], pUpperIp[3],
				pLowerIp[0], pLowerIp[1], pLowerIp[2], pLowerIp[3]);
		}
		else
		{
		}
	}

	if (len <= off+count)
		*eof = 1;

	*start = page + off;
	len -= off;

	if (len>count)
		len = count;

	if (len<0) 
		len = 0;

	return len;
}
int rtl_83xxSwACLReadProc(char *page, char **start, off_t off,
		     int count, int *eof, void *data)
{
	rtl83xx_acl_rule_t *curAcl;
	rtk_int32 cnt=0;
	
	int len=0;
	
	unsigned char *actionT[RTL83XX_ACL_END] = 
		{"copy cpu",
		 "drop",
		 "redirect",
		 "mirror",
		 "trap to cpu",
		 "priority",
		 "dscp remark",
		 "1p remark"};
	unsigned char *filterT[FILTER_END]=
		{"none",
		 "mask",
		 "range"};
	
	len += sprintf(page+len, "rtl8367 sw acl rule:\n");	
	CLIST_FOREACH(curAcl,&rtl83xx_acl_tbl,next_rule)
	{
		
		len += sprintf(page+len, "[%d]\tprio:%d, member:%x, invert:%d, size:%d, in_asic:%d, index:%d\n", 
			cnt, curAcl->prio, curAcl->member, curAcl->invert, 
			curAcl->size, curAcl->in_asic, curAcl->index);

		len += sprintf(page+len,"\tether type: %x	 ether type mask: %x\n", curAcl->filter.ethertype, curAcl->filter.ethertype_mask);
		len += sprintf(page+len,"\tvlan prio: %x	 vlan prio mask: %x\n", curAcl->filter.vlanpri, curAcl->filter.vlanpri_mask);
		len += sprintf(page+len,"\tSMAC: %x:%x:%x:%x:%x:%x  SMACM: %x:%x:%x:%x:%x:%x\n",
			curAcl->filter.smac.octet[0],curAcl->filter.smac.octet[1],curAcl->filter.smac.octet[2],
			curAcl->filter.smac.octet[3],curAcl->filter.smac.octet[4],curAcl->filter.smac.octet[5],
			curAcl->filter.smac_mask.octet[0],curAcl->filter.smac_mask.octet[1],curAcl->filter.smac_mask.octet[2],
			curAcl->filter.smac_mask.octet[3],curAcl->filter.smac_mask.octet[4],curAcl->filter.smac_mask.octet[5]);
		len += sprintf(page+len,"\tDMAC: %x:%x:%x:%x:%x:%x  DMACM: %x:%x:%x:%x:%x:%x\n",
			curAcl->filter.dmac.octet[0],curAcl->filter.dmac.octet[1],curAcl->filter.dmac.octet[2],
			curAcl->filter.dmac.octet[3],curAcl->filter.dmac.octet[4],curAcl->filter.dmac.octet[5],
			curAcl->filter.dmac_mask.octet[0],curAcl->filter.dmac_mask.octet[1],curAcl->filter.dmac_mask.octet[2],
			curAcl->filter.dmac_mask.octet[3],curAcl->filter.dmac_mask.octet[4],curAcl->filter.dmac_mask.octet[5]);
		len += sprintf(page+len,"\tip filter type:%s sip: %d.%d.%d.%d sipM: %d.%d.%d.%d\n", 
			filterT[curAcl->filter.sip_type],
			(curAcl->filter.sip>>24),((curAcl->filter.sip&0x00ff0000)>>16), 
			((curAcl->filter.sip&0x0000ff00)>>8),(curAcl->filter.sip&0xff), 
			(curAcl->filter.sip_mask>>24), ((curAcl->filter.sip_mask&0x00ff0000)>>16),
			((curAcl->filter.sip_mask&0x0000ff00)>>8), (curAcl->filter.sip_mask&0xff));
		len += sprintf(page+len,"\tip filter type:%s dip: %d.%d.%d.%d dipM: %d.%d.%d.%d\n", 
			filterT[curAcl->filter.dip_type],
			(curAcl->filter.dip>>24),((curAcl->filter.dip&0x00ff0000)>>16), 
			((curAcl->filter.dip&0x0000ff00)>>8),(curAcl->filter.dip&0xff), 
			(curAcl->filter.dip_mask>>24), ((curAcl->filter.dip_mask&0x00ff0000)>>16),
			((curAcl->filter.dip_mask&0x0000ff00)>>8), (curAcl->filter.dip_mask&0xff));
		len += sprintf(page+len,"\tTos: %x   TosM: %x	 ipProto: %x   ipProtoM: %x\n",
			curAcl->filter.ip_tos, curAcl->filter.ip_tos_mask, curAcl->filter.protocal, curAcl->filter.protocal_mask);
		len += sprintf(page+len,"\tSport filter type:%s Sport:%x	SportM:%x\n",
			filterT[curAcl->filter.sport_type],
			curAcl->filter.sport, curAcl->filter.sport_mask);
		len += sprintf(page+len,"\tDport filter type:%s Dport:%x	DportM:%x\n",
			filterT[curAcl->filter.dport_type],
			curAcl->filter.dport, curAcl->filter.dport_mask);
		len += sprintf(page+len,"\tAction:%s",actionT[curAcl->action.act_type]);
		if(curAcl->action.act_type==RTL83XX_ACL_PRIORITY)
			len += sprintf(page+len,"\t%d\n",curAcl->action.priority);
		else
			len += sprintf(page+len, "\n");
		cnt++;
	}		

	if (len <= off+count)
		*eof = 1;

	*start = page + off;
	len -= off;

	if (len>count)
		len = count;

	if (len<0) 
		len = 0;

	return len;
}

#endif
rtk_int32 rtl_83xxACLWriteProc(struct file *file, const char *buffer,
		      unsigned long count, void *data)
{
	return count;	
}
/*1.add
	echo "add $member $rule_prio $invert $match1 $match2 ... $matchN action $action_para" > /proc/rtl_8367_sw_acl
	echo "add 1f -30000 0 smac 01:00:5e:01:02:03 ff:ff:ff:ff:ff:ff action 5 2" > /proc/rtl_8367_sw_acl
	$matchN:
	smac $smac $smask  	--  smac 08:ef:4c:00:00:01 ff:ff:ff:ff:ff:ff:ff
	dmac $dmac $dmask	--  dmac 01:00:5e:01:02:03 ff:ff:ff:ff:ff:ff:ff
	sip $filter_type $sip $sipmask	-- sip 1 C0A80164 ffffffff  //mask
								-- sip 2 C0A80164 C0A80174 //ip range
	dip $filter_type $dip $dipmask 	-- dip 1 C0A802A0 ffffffff 
								-- dip 2 C0A802A0 C0A802B0
	tos $tos(hex) $mask(hex)		-- tos C FC 	//dscp 3 
	proto $proto(hex) $mask(hex) 	-- proto 6 ff 	//tcp 6
								-- proto 11 ff	//udp 17
	sport $filter_type $sport(dec) $mask(dec)	-- sport 1 2000 65535 	//mask
										-- sport 2 2000 3000  	//port range
	dport $filter_type $dport(dec) $mask(dec) 	-- dport 1 3000 65535 	//mask
										-- dport 2 3000 4000   	//port range
	dot1p $dot1p $mask				--	dot1p 5 7										
	$action_para
	1		//drop
	4		//trap to cpu
	5 $prio 	//qos prio
	8 $meter_index $rate			// policing0: ratelimit, unit kbps
	
   2.delete   	
  	echo "delete $member $rule_prio $invert $match1 $match2 ... $matchN action $action_para" > /proc/rtl_8367_sw_acl

   3.sync to asic
   	echo "to_asic" > /proc/rtl_8367_sw_acl

   4.flush
   	echo "flush all" > /proc/rtl_8367_sw_acl
   	echo "flush prio 0" > /proc/rtl_8367_sw_acl
*/
int rtl_83xxSwACLWriteProc(struct file *file, const char *buffer,
	      unsigned long count, void *data)
{
	char tmpBuf[512] = {0};
	char *strptr = NULL, *tokptr = NULL;
	rtk_int32 add_flag = 0, cnt = 0, rulenum=0, flush_prio = -1;
	rtk_uint32 mac[6]={0}, mac_mask[6]={0};

	rtl83xx_acl_rule_t rule;
	memset(&rule, 0, sizeof(rtl83xx_acl_rule_t));
	

	if ((count < 2) || (count > 512)) 
	return -EFAULT;

	if (buffer && !copy_from_user(tmpBuf, buffer, count))
	{
		tmpBuf[count-1]='\0';
		
		strptr = tmpBuf;
		tokptr = strsep(&strptr," ");

		if(tokptr != NULL)
		{
			if(!memcmp(tokptr, "add", 3)||!memcmp(tokptr, "del", 3))
			{
				if(!memcmp(tokptr, "add", 3))
					add_flag = 1;
				else
					add_flag = 0;

				//member
				tokptr = strsep(&strptr, " ");
				if(tokptr==NULL)
					goto errout;
				cnt = sscanf(tokptr, "%x", &rule.member);

				//prio
				tokptr = strsep(&strptr, " ");
				if(tokptr==NULL)
					goto errout;
				cnt = sscanf(tokptr, "%d", &rule.prio);

				//invert
				tokptr = strsep(&strptr, " ");
				if(tokptr==NULL)
					goto errout;
				cnt = sscanf(tokptr, "%c", &rule.invert);
				rule.invert -= '0';
				
				tokptr = strsep(&strptr, " ");
				while(tokptr && memcmp(tokptr, "action", 6))
				{
					if(!memcmp(tokptr, "smac", 4))
					{	
						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;
						cnt = sscanf(tokptr, "%2x:%2x:%2x:%2x:%2x:%2x", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
						rule.filter.smac.octet[0] = mac[0];
						rule.filter.smac.octet[1] = mac[1];
						rule.filter.smac.octet[2] = mac[2];
						rule.filter.smac.octet[3] = mac[3];
						rule.filter.smac.octet[4] = mac[4];
						rule.filter.smac.octet[5] = mac[5];

						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;					
						cnt = sscanf(tokptr, "%2x:%2x:%2x:%2x:%2x:%2x", &mac_mask[0], &mac_mask[1], &mac_mask[2], &mac_mask[3], &mac_mask[4], &mac_mask[5]);
						rule.filter.smac_mask.octet[0] = mac_mask[0];
						rule.filter.smac_mask.octet[1] = mac_mask[1];
						rule.filter.smac_mask.octet[2] = mac_mask[2];
						rule.filter.smac_mask.octet[3] = mac_mask[3];
						rule.filter.smac_mask.octet[4] = mac_mask[4];
						rule.filter.smac_mask.octet[5] = mac_mask[5];

						rulenum ++;
					}
					else if(!memcmp(tokptr, "dmac", 4))
					{	
						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;
						cnt = sscanf(tokptr, "%2x:%2x:%2x:%2x:%2x:%2x", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
						rule.filter.dmac.octet[0] = mac[0];
						rule.filter.dmac.octet[1] = mac[1];
						rule.filter.dmac.octet[2] = mac[2];
						rule.filter.dmac.octet[3] = mac[3];
						rule.filter.dmac.octet[4] = mac[4];
						rule.filter.dmac.octet[5] = mac[5];

						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;					
						cnt = sscanf(tokptr, "%2x:%2x:%2x:%2x:%2x:%2x", &mac_mask[0], &mac_mask[1], &mac_mask[2], &mac_mask[3], &mac_mask[4], &mac_mask[5]);
						rule.filter.dmac_mask.octet[0] = mac_mask[0];
						rule.filter.dmac_mask.octet[1] = mac_mask[1];
						rule.filter.dmac_mask.octet[2] = mac_mask[2];
						rule.filter.dmac_mask.octet[3] = mac_mask[3];
						rule.filter.dmac_mask.octet[4] = mac_mask[4];
						rule.filter.dmac_mask.octet[5] = mac_mask[5];

						rulenum ++;
					}
					else if(!memcmp(tokptr, "sip", 3))
					{
					
						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;
						cnt = sscanf(tokptr, "%d", &rule.filter.sip_type);

						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;
						cnt = sscanf(tokptr, "%x", &rule.filter.sip);

						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;
						cnt = sscanf(tokptr, "%x", &rule.filter.sip_mask);

						rulenum ++;
					}
					else if(!memcmp(tokptr, "dip", 3))
					{
					
						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;
						cnt = sscanf(tokptr, "%d", &rule.filter.dip_type);

						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;
						cnt = sscanf(tokptr, "%x", &rule.filter.dip);

						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;
						cnt = sscanf(tokptr, "%x", &rule.filter.dip_mask);

						rulenum ++;
					}
					else if(!memcmp(tokptr, "tos", 3))
					{
					
						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;
						cnt = sscanf(tokptr, "%x", &rule.filter.ip_tos);

						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;
						cnt = sscanf(tokptr, "%x", &rule.filter.ip_tos_mask);

						rulenum ++;
					}
					else if(!memcmp(tokptr, "proto", 5))
					{
					
						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;
						cnt = sscanf(tokptr, "%x", &rule.filter.protocal);

						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;
						cnt = sscanf(tokptr, "%x", &rule.filter.protocal_mask);

						rulenum ++;
					}
					else if(!memcmp(tokptr, "sport", 5))
					{
					
						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;
						cnt = sscanf(tokptr, "%d", &rule.filter.sport_type);

						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;
						cnt = sscanf(tokptr, "%d", &rule.filter.sport);

						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;
						cnt = sscanf(tokptr, "%d", &rule.filter.sport_mask);

						rulenum ++;
					}
					else if(!memcmp(tokptr, "dport", 5))
					{
					
						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;
						cnt = sscanf(tokptr, "%d", &rule.filter.dport_type);

						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;
						cnt = sscanf(tokptr, "%d", &rule.filter.dport);

						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;
						cnt = sscanf(tokptr, "%d", &rule.filter.dport_mask);

						rulenum ++;
					}
					else if(!memcmp(tokptr, "dot1p", 5))
					{
						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;
						cnt = sscanf(tokptr, "%x", &rule.filter.vlanpri);

						tokptr = strsep(&strptr, " ");
						if(tokptr == NULL)
							goto errout;
						cnt = sscanf(tokptr, "%x", &rule.filter.vlanpri_mask);
						
						rulenum ++;
					}
					else
					{
						goto errout;
					}
					
					tokptr = strsep(&strptr, " ");

				}
				if(rulenum==0 || memcmp(tokptr, "action", 6))
					goto errout;

				tokptr = strsep(&strptr, " ");
				if(tokptr == NULL)
					goto errout;

				cnt = sscanf(tokptr, "%d", &rule.action.act_type);
				if(rule.action.act_type >= RTL83XX_ACL_END)
					goto errout;
					
				if(rule.action.act_type==RTL83XX_ACL_PRIORITY)
				{
					tokptr = strsep(&strptr, " ");
					if(tokptr == NULL)
						goto errout;
					
					cnt = sscanf(tokptr, "%d", &rule.action.priority);
				}
				else if(rule.action.act_type == RTL83XX_ACL_POLICING0)
				{
					tokptr = strsep(&strptr, " ");
					if(tokptr == NULL)
						goto errout;

					cnt = sscanf(tokptr, "%d", &rule.action.meter_index);
					if(rule.action.meter_index > RTL83XX_METERMAX)
						goto errout;
					
					tokptr = strsep(&strptr, " ");
					if(tokptr == NULL)
						goto errout;
					
					cnt = sscanf(tokptr, "%d", &rule.action.policing);
					if(rule.action.policing < 8 || rule.action.policing > (0x1FFFF*8))
						goto errout;
				}
				if(add_flag)
					rtl83xx_addAclRule(&rule);
				else
					rtl83xx_deleteAclRule(&rule);

			}
			else if(!memcmp(tokptr, "to_asic", 7))
			{
				rtl83xx_syncAclTblToAsic();
			}
			else if(!memcmp(tokptr, "flush", 5))
			{
				tokptr = strsep(&strptr, " ");
				if(tokptr==NULL)
					goto errout;

				if(!memcmp(tokptr, "all", 3))
				{
					rtl83xx_flushAclRule();
					rtl83xx_syncAclTblToAsic();
				}
				else if(!memcmp(tokptr, "prio", 4))
				{
					tokptr = strsep(&strptr, " ");
					if(tokptr==NULL)
						goto errout;
					cnt = sscanf(tokptr, "%d", &flush_prio);
					rtl83xx_flushAclRulebyPrio(flush_prio);
					rtl83xx_syncAclTblToAsic();
				}
				else
					goto errout;
			}
			else if(!memcmp(tokptr, "help", 4))
			{
				printk("1.add \n");
				printk("	[] is required, {} is optional:\n");
				printk("		echo \"add [$member] [$rule_prio] [$invert] {$match1} {$match2} ... {$matchN}\n");
				printk("		[action $action_para]\" > /proc/rtl_83xx_sw_acl\n");
				printk(" \n");
				
				printk("  	example: \n");
				printk("		echo \"add 1f -30000 0 smac 01:00:5e:01:02:03 ff:ff:ff:ff:ff:ff action 5 2\" > /proc/rtl_83xx_sw_acl\n");
				printk(" \n");

				printk("	$matchN:\n");
				printk("		smac $smac $smask  	--  smac 08:ef:4c:00:00:01 ff:ff:ff:ff:ff:ff:ff\n");
				printk("		dmac $dmac $dmask	--  dmac 01:00:5e:01:02:03 ff:ff:ff:ff:ff:ff:ff\n");
				printk("		sip  $filter_type $sip $sipmask	-- sip 1 C0A80164 ffffffff  //mask\n");
				printk("						-- sip 2 C0A80164 C0A80174  //ip range\n");
				printk("		dip  $filter_type $dip $dipmask -- dip 1 C0A802A0 ffffffff 	\n");
				printk("						-- dip 2 C0A802A0 C0A802B0	\n");
				printk("		tos  $tos(hex) $mask(hex)		-- tos C FC 	//dscp 3 \n");
				printk("		proto $proto(hex) $mask(hex) 	-- proto 6 ff 	//tcp 6\n");
				printk("						-- proto 11 ff	//udp 17\n");
				printk("		sport $filter_type $sport(dec) $mask(dec)	-- sport 1 2000 65535 	//mask\n");
				printk("								-- sport 2 2000 3000  	//port range\n");
				printk("		dport $filter_type $dport(dec) $mask(dec) 	-- dport 1 3000 65535 	//mask\n");
				printk("								-- dport 2 3000 4000   	//port range\n");
				printk(" \n");

				printk("	$action_para\n");
				printk("		1	--	drop\n");
				printk("		3	--	mirror\n");
				printk("		4	--	trap to CPU\n");
				printk("		5 $prio	--	qos prio\n");
				printk("		8 $meter_index $rate	--	policing: ratelimit, unit kbps\n");
				printk(" \n");

				printk("2.delete \n");
				printk("	[] is required, {} is optional:\n");
				printk("		echo \"delete [$member] [$rule_prio] [$invert] {$match1} {$match2} ... {$matchN}\n");
				printk("		[action $action_para]\" > /proc/rtl_83xx_sw_acl\n");
				printk(" \n");
				
				printk("3.sync to asic\n");
				printk("	echo \"to_asic\" > /proc/rtl_83xx_sw_acl\n");
				printk(" \n");

				printk("4.flush\n");
				printk("	echo \"flush all\" > /proc/rtl_83xx_sw_acl\n");
				printk("	echo \"flush prio 0\" > /proc/rtl_83xx_sw_acl\n");
				printk(" \n");
			}
			
			else
			{
errout:
				printk("error command!\n");
			}
		}
	}

	return count;
}
#endif
