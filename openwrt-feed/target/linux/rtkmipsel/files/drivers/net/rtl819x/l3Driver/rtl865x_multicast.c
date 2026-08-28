/*
 *
 *  Copyright (c) 2011 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifdef __linux__
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
#include <linux/kconfig.h>
#else
#include <linux/config.h>
#endif
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#endif



#include <net/rtl/rtl_types.h>
#include <net/rtl/rtl_glue.h>
//#include "common/rtl_utils.h"
//#include "common/assert.h"

#ifdef CONFIG_RTL_LAYERED_ASIC_DRIVER
#include "AsicDriver/rtl865x_asicCom.h"
#include "AsicDriver/rtl865x_asicL3.h"
#else
#include "AsicDriver/rtl865xC_tblAsicDrv.h" 
#include "common/rtl865x_tblDrvPatch.h"
#endif

#include "AsicDriver/asicRegs.h"
#include "AsicDriver/asicTabs.h"

#include "common/rtl8651_tblDrvProto.h"

#include "common/rtl865x_eventMgr.h"
#include "common/rtl865x_vlan.h"
#include <net/rtl/rtl865x_netif.h>

#include "l3Driver/rtl865x_ip.h"

#ifdef RTL865X_TEST
#include <string.h>
#endif

#include <net/rtl/rtl865x_multicast.h>
#include <net/rtl/rtl865x_igmpsnooping.h>

#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
int32 rtl8198C_initMulticastv6(void);
int32 rtl8198C_reinitMulticastv6(void);
#endif

#if defined(CONFIG_RTL_HARDWARE_MULTICAST_API)
static int32 _rtl865x_kernelMCastUnRegisterEvent(void);
static int32 _rtl865x_kernelMCastRegisterEvent(void);
static int32 rtl_initIgmpKernelMCast(void);
static int32 rtl_initMldKernelMCast(void);
#endif

/********************************************************/
/*			Multicast Related Global Variable			*/
/********************************************************/

static rtl865x_mcast_fwd_descriptor_t *rtl865x_mcastFwdDescPool=NULL;
static mcast_fwd_descriptor_head_t  free_mcast_fwd_descriptor_head;
static struct rtl865x_multicastTable mCastTbl;
static uint32 rtl865x_externalMulticastPortMask = 0;
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
static rtl8198c_mcast_fwd_descriptor6_t *rtl8198c_mcastFwdDescPool6=NULL;
static mcast_fwd_descriptor_head6_t free_mcast_fwd_descriptor_head6;
static struct rtl8198c_multicastv6Table mCastTbl6;
static uint32 rtl8198c_externalMulticastPortMask6 = 0;
#endif
#if defined(__linux__) && defined(__KERNEL__)
static struct timer_list rtl865x_mCastSysTimer;	/*igmp timer*/
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
static struct timer_list rtl8198c_mCast6SysTimer;/*MLD timer*/
#endif
#endif
#if defined(CONFIG_RTL_HARDWARE_MULTICAST_API)
static rtl_kernelGroupEntry_t *rtl_igmp_kernelGroupEntryPool = NULL;
static rtl_kernelGroupEntry_t *rtl_mld_kernelGroupEntryPool = NULL;
static rtl_kernelPortEntry_t *rtl_igmp_kernelPortEntryPool = NULL;
static rtl_kernelPortEntry_t *rtl_mld_kernelPortEntryPool = NULL;
static rtl_kernelSourceEntry_t *rtl_igmp_kernelSrcEntryPool = NULL;
static rtl_kernelSourceEntry_t *rtl_mld_kernelSrcEntryPool = NULL;

static rtl_kernelGroupEntry_head_t rtl_igmp_free_kernelGroupEntry_head;
static rtl_kernelGroupEntry_head_t rtl_igmp_kernelGroupEntry_list;
static rtl_kernelGroupEntry_head_t rtl_mld_free_kernelGroupEntry_head;
static rtl_kernelGroupEntry_head_t rtl_mld_kernelGroupEntry_list;
static rtl_kernelPortEntry_head_t rtl_igmp_free_kernelPortEntry_head;
static rtl_kernelPortEntry_head_t rtl_mld_free_kernelPortEntry_head;
static rtl_kernelSourceEntry_head_t rtl_igmp_free_kernelSrcEntry_head;
static rtl_kernelSourceEntry_head_t rtl_mld_free_kernelSrcEntry_head;

static rtl_multicastEventContext_t rtl_kernel_report_event;

#endif
#if defined (CONFIG_RTL_HW_MCAST_WIFI)
extern int hwwifiEnable;
#endif


#if defined (CONFIG_RT_MULTIPLE_BR_SUPPORT)
extern int rtl_get_brIgmpModuleIndexbyName(char *name, int * index);
extern struct net_bridge * rtl_get_brByIndex(int index, char *name);
#endif

static int32 _rtl865x_initMCastFwdDescPool(void)
{
	int32 i;


	MC_LIST_INIT(&free_mcast_fwd_descriptor_head);

	TBL_MEM_ALLOC(rtl865x_mcastFwdDescPool, rtl865x_mcast_fwd_descriptor_t,MAX_MCAST_FWD_DESCRIPTOR_CNT);
	
	if(rtl865x_mcastFwdDescPool!=NULL)
	{
	
		memset( rtl865x_mcastFwdDescPool, 0, MAX_MCAST_FWD_DESCRIPTOR_CNT * sizeof(rtl865x_mcast_fwd_descriptor_t));	
	}
	else
	{
		return FAILED;
	}
	

	for(i = 0; i<MAX_MCAST_FWD_DESCRIPTOR_CNT;i++)
	{
		MC_LIST_INSERT_HEAD(&free_mcast_fwd_descriptor_head, &rtl865x_mcastFwdDescPool[i], next);
	}
	
	return SUCCESS;
}

static rtl865x_mcast_fwd_descriptor_t *_rtl865x_allocMCastFwdDesc(void)
{
	rtl865x_mcast_fwd_descriptor_t *retDesc=NULL;
	retDesc = MC_LIST_FIRST(&free_mcast_fwd_descriptor_head);
	if(retDesc!=NULL)
	{
		MC_LIST_REMOVE(retDesc, next);
		memset(retDesc,0,sizeof(rtl865x_mcast_fwd_descriptor_t));
	}
	return retDesc;
}

static int32 _rtl865x_freeMCastFwdDesc(rtl865x_mcast_fwd_descriptor_t *descPtr)
{
	if(descPtr==NULL)
	{
		return SUCCESS;
	}
	memset(descPtr,0,sizeof(rtl865x_mcast_fwd_descriptor_t));
	MC_LIST_INSERT_HEAD(&free_mcast_fwd_descriptor_head, descPtr, next);
	
	return SUCCESS;
}

static int32 _rtl865x_flushMCastFwdDescChain(mcast_fwd_descriptor_head_t * descChainHead)
{
	rtl865x_mcast_fwd_descriptor_t * curDesc,*nextDesc;
	
	if(descChainHead==NULL)
	{
		return SUCCESS;
	}
	
	curDesc=MC_LIST_FIRST(descChainHead);
	while(curDesc)
	{
		nextDesc=MC_LIST_NEXT(curDesc, next );
		/*remove from the old descriptor chain*/
		MC_LIST_REMOVE(curDesc, next);
		/*return to the free descriptor chain*/
		_rtl865x_freeMCastFwdDesc(curDesc);
		curDesc = nextDesc;
	}

	return SUCCESS;
}



static int32 _rtl865x_mCastFwdDescEnqueue(mcast_fwd_descriptor_head_t * queueHead,
												rtl865x_mcast_fwd_descriptor_t * enqueueDesc)
{

	rtl865x_mcast_fwd_descriptor_t *newDesc;
	rtl865x_mcast_fwd_descriptor_t *curDesc,*nextDesc;
	if(queueHead==NULL)
	{
		return FAILED;
	}
	
	if(enqueueDesc==NULL)
	{
		return SUCCESS;
	}
	
	/*multicast forward descriptor is internal maintained,always alloc new one*/
	newDesc=_rtl865x_allocMCastFwdDesc();
	
	if(newDesc!=NULL)
	{
		memcpy(newDesc, enqueueDesc,sizeof(rtl865x_mcast_fwd_descriptor_t ));
		//memset(&(newDesc->next), 0, sizeof(MC_LIST_ENTRY(rtl865x_mcast_fwd_descriptor_s)));
		newDesc->next.le_next=NULL;
		newDesc->next.le_prev=NULL;
	}
	else
	{
		/*no enough memory*/
		return FAILED;
	}
	

	for(curDesc=MC_LIST_FIRST(queueHead);curDesc!=NULL;curDesc=nextDesc)
	{

		nextDesc=MC_LIST_NEXT(curDesc, next);
		
		/*merge two descriptor*/
	//	if((strcmp(curDesc->netifName,newDesc->netifName)==0) && (curDesc->vid==newDesc->vid))
		if(strcmp(curDesc->netifName,newDesc->netifName)==0)
		{	
			
			if(newDesc->descPortMask==0)
			{
				newDesc->descPortMask=curDesc->descPortMask;
			}
			MC_LIST_REMOVE(curDesc, next);
			_rtl865x_freeMCastFwdDesc(curDesc);
			
		}
	}

	/*not matched descriptor is found*/
	MC_LIST_INSERT_HEAD(queueHead, newDesc, next);

	return SUCCESS;
	
}


static int32 _rtl865x_mergeMCastFwdDescChain(mcast_fwd_descriptor_head_t * targetChainHead ,
													rtl865x_mcast_fwd_descriptor_t *srcChain)
{
	rtl865x_mcast_fwd_descriptor_t *curDesc;

	if(targetChainHead==NULL)
	{
		return FAILED;
	}
	
	for(curDesc=srcChain; curDesc!=NULL; curDesc=MC_LIST_NEXT(curDesc,next))
	{
		
		_rtl865x_mCastFwdDescEnqueue(targetChainHead, curDesc);
		
	}
	
	return SUCCESS;
}




static int32 _rtl865x_initMCastEntryPool(void)
{
	int32 index;
	rtl865x_tblDrv_mCast_t *multiCast_t;
	struct MCast_hash_head *mCast_hash_head;
	
	TBL_MEM_ALLOC(multiCast_t, rtl865x_tblDrv_mCast_t ,MAX_MCAST_TABLE_ENTRY_CNT);
	TAILQ_INIT(&mCastTbl.freeList.freeMultiCast);
	for(index=0; index<MAX_MCAST_TABLE_ENTRY_CNT; index++)
	{
		memset( &multiCast_t[index], 0, sizeof(rtl865x_tblDrv_mCast_t));
		TAILQ_INSERT_HEAD(&mCastTbl.freeList.freeMultiCast, &multiCast_t[index], nextMCast);
	}

	//TBL_MEM_ALLOC(multiCast_t, rtl865x_tblDrv_mCast_t, RTL8651_MULTICASTTBL_SIZE);
	TBL_MEM_ALLOC(mCast_hash_head, struct MCast_hash_head, RTL8651_MULTICASTTBL_SIZE);
	memset(mCast_hash_head, 0,RTL8651_MULTICASTTBL_SIZE* sizeof(struct MCast_hash_head));
	mCastTbl.inuseList.mCastTbl = (void *)mCast_hash_head;

	for(index=0; index<RTL8651_MULTICASTTBL_SIZE; index++)
	{
		TAILQ_INIT(&mCastTbl.inuseList.mCastTbl[index]);
	}

	return SUCCESS;
}

static rtl865x_tblDrv_mCast_t * _rtl865x_allocMCastEntry(uint32 hashIndex)
{
	rtl865x_tblDrv_mCast_t *newEntry;
	newEntry=TAILQ_FIRST(&mCastTbl.freeList.freeMultiCast);
	if (newEntry == NULL)
	{
		return NULL;
	}		
	
	TAILQ_REMOVE(&mCastTbl.freeList.freeMultiCast, newEntry, nextMCast);

	
	/*initialize it*/
	if(MC_LIST_FIRST(&newEntry->fwdDescChain)!=NULL)
	{
		_rtl865x_flushMCastFwdDescChain(&newEntry->fwdDescChain);
	}
	MC_LIST_INIT(&newEntry->fwdDescChain);
	
	memset(newEntry, 0, sizeof(rtl865x_tblDrv_mCast_t));

	TAILQ_INSERT_TAIL(&mCastTbl.inuseList.mCastTbl[hashIndex], newEntry, nextMCast);
	
	return newEntry;
}

static int32 _rtl865x_flushMCastEntry(rtl865x_tblDrv_mCast_t *mCastEntry)
{
	if(mCastEntry==NULL)
	{
		return SUCCESS;
	}
	
	_rtl865x_flushMCastFwdDescChain(&mCastEntry->fwdDescChain);
	
	memset(mCastEntry, 0, sizeof(rtl865x_tblDrv_mCast_t));
	return SUCCESS;
}

static int32 _rtl865x_freeMCastEntry(rtl865x_tblDrv_mCast_t * mCastEntry, uint32 hashIndex)
{
	if(mCastEntry==NULL)
	{
		return SUCCESS;
	}
	
	TAILQ_REMOVE(&mCastTbl.inuseList.mCastTbl[hashIndex], mCastEntry, nextMCast);
	_rtl865x_flushMCastEntry(mCastEntry);
	TAILQ_INSERT_HEAD(&mCastTbl.freeList.freeMultiCast, mCastEntry, nextMCast);
	return SUCCESS;
}


static uint32 _rtl865x_doMCastEntrySrcVlanPortFilter(rtl865x_tblDrv_mCast_t *mCastEntry)
{
	rtl865x_mcast_fwd_descriptor_t * curDesc,*nextDesc;
	if(mCastEntry==NULL)
	{
		return SUCCESS;
	}
	
	for(curDesc=MC_LIST_FIRST(&mCastEntry->fwdDescChain);curDesc!=NULL;curDesc=nextDesc)
	{
		nextDesc=MC_LIST_NEXT(curDesc, next);
		{
			curDesc->fwdPortMask=curDesc->fwdPortMask & (~(1<<mCastEntry->port));
			if(curDesc->fwdPortMask==0)
			{
				/*remove from the old chain*/
				MC_LIST_REMOVE(curDesc, next);
				/*return to the free descriptor chain*/
				_rtl865x_freeMCastFwdDesc(curDesc);

			}
		}
		
	}

	return SUCCESS;
}
#if (defined(CONFIG_RTL_MULTI_LAN_DEV) && defined(CONFIG_RTL_HW_MCAST_WIFI))||defined(CONFIG_RTL_HARDWARE_MULTICAST_API)
static uint32 rtl865x_genMCastEntryAsicFwdMaskIgnoreCpuFlag(rtl865x_tblDrv_mCast_t *mCastEntry)
{
	uint32 asicFwdPortMask=0;
	rtl865x_mcast_fwd_descriptor_t * curDesc;
	if(mCastEntry==NULL)
	{
		return 0;
	}
	
	MC_LIST_FOREACH(curDesc, &(mCastEntry->fwdDescChain), next)
	{
		asicFwdPortMask |= curDesc->fwdPortMask;
	}
	asicFwdPortMask = asicFwdPortMask & (~(1<<mCastEntry->port)); 
	return asicFwdPortMask;
}
#endif

 
static uint32 rtl865x_genMCastEntryAsicFwdMask(rtl865x_tblDrv_mCast_t *mCastEntry)
{
	uint32 asicFwdPortMask=0;
	rtl865x_mcast_fwd_descriptor_t * curDesc;
	if(mCastEntry==NULL)
	{
		return 0;
	}
	
	MC_LIST_FOREACH(curDesc, &(mCastEntry->fwdDescChain), next)
	{
		if(curDesc->toCpu==0)
		{
			asicFwdPortMask|=(curDesc->fwdPortMask & ((1<<RTL8651_MAC_NUMBER)-1));
		}
		else
		{
			asicFwdPortMask|=( 0x01<<RTL8651_MAC_NUMBER);
		}
	}
	asicFwdPortMask = asicFwdPortMask & (~(1<<mCastEntry->port)); 
	return asicFwdPortMask;
}

static uint16 rtl865x_genMCastEntryCpuFlag(rtl865x_tblDrv_mCast_t *mCastEntry)
{
	uint16 cpuFlag=FALSE;
	rtl865x_mcast_fwd_descriptor_t * curDesc;
	if(mCastEntry==NULL)
	{
		return 0;
	}

	if(mCastEntry->cpuHold==TRUE)
	{
		cpuFlag=TRUE;
	}
	
	MC_LIST_FOREACH(curDesc, &(mCastEntry->fwdDescChain), next)
	{
		if(	(curDesc->toCpu==TRUE)	||
			(memcmp(curDesc->netifName, RTL_WLAN_NAME,4)==0)	)
		{
			cpuFlag=TRUE;
		}
	}
	
	return cpuFlag;
}

#if defined (CONFIG_RTL_IGMP_SNOOPING)
/*for linux bridge level igmp snooping usage*/
#if !defined(CONFIG_RTL_MULTI_LAN_DEV)
static uint32 rtl865x_getMCastEntryDescPortMask(rtl865x_tblDrv_mCast_t *mCastEntry)
{
	uint32 descPortMask=0;
	rtl865x_mcast_fwd_descriptor_t * curDesc;
	if(mCastEntry==NULL)
	{
		return 0;
	}
	
	MC_LIST_FOREACH(curDesc, &(mCastEntry->fwdDescChain), next)
	{
		descPortMask=descPortMask | curDesc->descPortMask;
	}
	
	return descPortMask;
}
#endif
#endif
/*=======================================
  * Multicast Table APIs
  *=======================================*/
#define RTL865X_MULTICASE_TABLE_APIs

static void  _rtl865x_setASICMulticastPortStatus(void) {
	uint32 index;

	for (index=0; index<RTL8651_PORT_NUMBER+rtl8651_totalExtPortNum; index++) {
		rtl8651_setAsicMulticastPortInternal(index, (rtl865x_externalMulticastPortMask&(1<<index))?FALSE:TRUE);
	}
}

void rtl865x_arrangeMulticastPortStatus(void) {

	rtl865x_externalMulticastPortMask=rtl865x_getExternalPortMask();
	_rtl865x_setASICMulticastPortStatus();
}

/*
@func int32	| rtl865x_addMulticastExternalPort	| API to add a hardware multicast external port.
@parm  uint32 | extPort	| External port number to be added. 
@rvalue SUCCESS	|Add hardware multicast external port successfully.
@rvalue FAILED	|Add hardware multicast external port failed.
*/
int32 rtl865x_addMulticastExternalPort(uint32 extPort)
{
	rtl865x_externalMulticastPortMask |= (1<<extPort);
	_rtl865x_setASICMulticastPortStatus();
	return SUCCESS;
}

/*
@func int32	| rtl865x_delMulticastExternalPort	| API to delete a hardware multicast external port.
@parm  uint32 | extPort	| External port number to be deleted.
@rvalue SUCCESS	|Delete external port successfully.
@rvalue FAILED	|Delete external port failed.
*/
int32 rtl865x_delMulticastExternalPort(uint32 extPort)
{
	rtl865x_externalMulticastPortMask &= ~(1<<extPort);
	_rtl865x_setASICMulticastPortStatus();
	return SUCCESS;
}

/*
@func int32	| rtl865x_setMulticastExternalPortMask	| API to set hardware multicast external port mask.
@parm  uint32 | extPortMask	| External port mask to be set.
@rvalue SUCCESS	|Set external port mask successfully.
@rvalue FAILED	|Set external port mask failed.
*/
int32 rtl865x_setMulticastExternalPortMask(uint32 extPortMask)
{
	rtl865x_externalMulticastPortMask =extPortMask;
	_rtl865x_setASICMulticastPortStatus();
	return SUCCESS;
}

/*
@func int32	| rtl865x_addMulticastExternalPortMask	| API to add hardware multicast external port mask.
@parm  uint32 | extPortMask	| External port mask to be added.
@rvalue SUCCESS	|Add external port mask successfully.
@rvalue FAILED	|Add external port mask failed.
*/
int32 rtl865x_addMulticastExternalPortMask(uint32 extPortMask)
{
	rtl865x_externalMulticastPortMask|= extPortMask;
	_rtl865x_setASICMulticastPortStatus();
	return SUCCESS;
}

/*
@func int32	| rtl865x_delMulticastExternalPortMask	|  API to delete hardware multicast external port mask.
@parm  uint32 | extPortMask	| External port mask to be deleted.
@rvalue SUCCESS	|Delete external port mask successfully.
@rvalue FAILED	|Delete external port mask failed.
*/
int32 rtl865x_delMulticastExternalPortMask(uint32 extPortMask)
{
	rtl865x_externalMulticastPortMask &= ~extPortMask;
	_rtl865x_setASICMulticastPortStatus();
	return SUCCESS;
}

int32 rtl865x_getMulticastExternalPortMask(void)
{
	return rtl865x_externalMulticastPortMask ;
}

static inline void _rtl865x_patchPppoeWeak(rtl865x_tblDrv_mCast_t *mCast_t)
{
	rtl865x_mcast_fwd_descriptor_t * curDesc;
	uint32 netifType;
	/* patch: keep cache in software if one vlan's interface is pppoe */
	MC_LIST_FOREACH(curDesc, &(mCast_t->fwdDescChain), next)
	{
		if(rtl865x_getNetifType(curDesc->netifName, &netifType)==SUCCESS)
		{
			/*how about pptp,l2tp?*/
			if(netifType==IF_PPPOE)
			{
				mCast_t->flag |= RTL865X_MULTICAST_PPPOEPATCH_CPUBIT;
				return;
			}
		}
		
	}

	mCast_t->flag &= ~RTL865X_MULTICAST_PPPOEPATCH_CPUBIT;
}
#if 0
static int _rtl865x_checkMulticastEntryEqual(rtl865x_tblDrv_mCast_t * mCastEntry1, rtl865x_tblDrv_mCast_t * mCastEntry2)
{
	if((mCastEntry1==NULL) && (mCastEntry2==NULL))
	{
		return TRUE;
	}
	
	if((mCastEntry1==NULL) && (mCastEntry2!=NULL))
	{
		return FALSE;
	}

	if((mCastEntry1!=NULL) && (mCastEntry2==NULL))
	{
		return FALSE;
	}
	
	if(mCastEntry1->sip!=mCastEntry2->sip)
	{
		return FALSE;
	}

	if(mCastEntry1->dip!=mCastEntry2->dip)
	{
		return FALSE;
	}

	if(mCastEntry1->svid!=mCastEntry2->svid)
	{
		return FALSE;
	}
	
	if(mCastEntry1->port!=mCastEntry2->port)
	{
		return FALSE;
	}

	if(mCastEntry1->mbr!=mCastEntry2->mbr)
	{
		return FALSE;
	}
	
	if(mCastEntry1->cpu!=mCastEntry2->cpu)
	{
		return FALSE;
	}
	
	if(mCastEntry1->extIp!=mCastEntry2->extIp)
	{
		return FALSE;
	}

	if(mCastEntry1->flag!=mCastEntry2->flag)
	{
		return FALSE;
	}

	
	if(mCastEntry1->inAsic!=mCastEntry2->inAsic)
	{
		return FALSE;
	}			

	return TRUE;
}
#endif
#ifdef CONFIG_PROC_FS
static unsigned int mcastAddOpCnt=0;
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
static unsigned int mcastAddOpCnt6=0;
#endif
unsigned int _rtl865x_getAddMcastOpCnt(void)
{
	return mcastAddOpCnt;
}
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
unsigned int _rtl8198C_getAddMcastv6OpCnt(void)
{
	return mcastAddOpCnt6;
}
#endif

static unsigned int mcastDelOpCnt=0;
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
static unsigned int mcastDelOpCnt6=0;
#endif
unsigned int _rtl865x_getDelMcastOpCnt(void)
{
	return mcastDelOpCnt;
}
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
unsigned int _rtl8198C_getDelMcastv6OpCnt(void)
{
	return mcastDelOpCnt6;
}

#endif
#endif
/* re-select Multicast entry to ASIC for the index ""entryIndex */
static void _rtl865x_arrangeMulticast(uint32 entryIndex)
{
	rtl865x_tblAsicDrv_multiCastParam_t asic_mcast;
	rtl865x_tblDrv_mCast_t *mCast_t=NULL;
	rtl865x_tblDrv_mCast_t *select_t=NULL;
	rtl865x_tblDrv_mCast_t *swapOutEntry=NULL;
	int32 retval;
	
	TAILQ_FOREACH(mCast_t, &mCastTbl.inuseList.mCastTbl[entryIndex], nextMCast) 
	{
	
		#ifdef CONFIG_RTL_HW_MCAST_WIFI
		if(((hwwifiEnable)||((hwwifiEnable==0)&&(mCast_t->cpu == 0)))
			&& !(mCast_t->flag & RTL865X_MULTICAST_PPPOEPATCH_CPUBIT))
		#else
		if ((mCast_t->cpu == 0) && !(mCast_t->flag & RTL865X_MULTICAST_PPPOEPATCH_CPUBIT)) 
		#endif
		{ /* Ignore cpu=1 */

			if(mCast_t->inAsic==TRUE)
			{
				if(swapOutEntry==NULL)
				{
					swapOutEntry=mCast_t;
				}
				else
				{
					/*impossible, two flow in one asic entry*/
					swapOutEntry->inAsic=FALSE;
					mCast_t->inAsic = FALSE;
				}
			}
			
			if (select_t) 
			{


				if ((mCast_t->unKnownMCast==FALSE) && (select_t->unKnownMCast==TRUE))
				{
					/*replace unknown multicast*/
					select_t = mCast_t;
				}
				else
				{
					/*select the heavy load*/
					if ((mCast_t->count) > (select_t->count))
					{
						select_t = mCast_t;
					}
				}
				
			}
			else 
			{
				select_t = mCast_t;
			}
			
			
		}
		else
		{
			mCast_t->inAsic = FALSE;	/* reset "inAsic" bit */
		} 


	}
	
	if(select_t && swapOutEntry)
	{
		if ((swapOutEntry->unKnownMCast==FALSE) && (select_t->unKnownMCast==TRUE))
		{
			/*replace unknown multicast*/
			select_t = swapOutEntry;
		}
		else
		{
			if((select_t->count <= (swapOutEntry->count+RTL865X_HW_MCAST_SWAP_GAP)))
				select_t = swapOutEntry;
		}
	}	
	

	
	/*
	if(swapOutEntry)
	{
		printk("%s:%d,swapOutEntry->count:%d,swapOutEntry->dip is 0x%x,swapOutEntry->mbr is 0x%x\n",__FUNCTION__,__LINE__,swapOutEntry->count,swapOutEntry->dip,swapOutEntry->mbr);

	}
	
	if (select_t) 
	{
		printk("%s:%d,select_t->count:%d,select_t->dip is 0x%x,select_t->mbr is 0x%x\n",__FUNCTION__,__LINE__,select_t->count,select_t->dip,select_t->mbr);
	}
	*/
	if (select_t) 
	{
		if((swapOutEntry==NULL) ||(select_t==swapOutEntry))
		{
			select_t->age = RTL865X_MULTICAST_TABLE_ASIC_AGE;
			bzero(&asic_mcast, sizeof(rtl865x_tblAsicDrv_multiCastParam_t));
			memcpy(&asic_mcast, select_t, (uint32)&(((rtl865x_tblDrv_mCast_t *)0)->extIp));
			if (select_t->extIp)
			{
			
#ifdef CONFIG_RTL_LAYERED_DRIVER_L3
				int32 ipIdx;
				if(rtl865x_getIpIdxByExtIp(select_t->extIp, &ipIdx)==SUCCESS)
				{
					asic_mcast.extIdx=(uint16)ipIdx;
				}
#else
				asic_mcast.extIdx=0;
#endif
		
			}
	#if defined (CONFIG_RTL_HARDWARE_MULTICAST_CAM)
			asic_mcast.idx=(uint16)entryIndex;
	#endif
			retval = rtl8651_setAsicIpMulticastTable(&asic_mcast);
			
#ifdef CONFIG_PROC_FS
			mcastAddOpCnt++;
#endif
			assert(retval == SUCCESS);
			if(retval==SUCCESS)
			{
				select_t->inAsic = TRUE;
			}
			else
			{
				select_t->inAsic = FALSE;
				rtl8651_delAsicIpMulticastTable(entryIndex);
#ifdef CONFIG_PROC_FS
				mcastDelOpCnt++;
#endif
			}
				
			assert(retval == SUCCESS);
			TAILQ_REMOVE(&mCastTbl.inuseList.mCastTbl[entryIndex], select_t, nextMCast);
			TAILQ_INSERT_HEAD(&mCastTbl.inuseList.mCastTbl[entryIndex], select_t, nextMCast);
		}
		else/*(swapOutEntry!=NULL) && (select_t!=swapOutEntry)*/
		{
			/*disable swap and only explicit joined mulicast flow can replace unknown multicast flow*/
			if(1)
			{
				/*don't forget to set swapOutEntry's inAsic flag*/
				swapOutEntry->inAsic=FALSE;
				
				select_t->age = RTL865X_MULTICAST_TABLE_ASIC_AGE;
				bzero(&asic_mcast, sizeof(rtl865x_tblAsicDrv_multiCastParam_t));
				memcpy(&asic_mcast, select_t, (uint32)&(((rtl865x_tblDrv_mCast_t *)0)->extIp));

				if (select_t->extIp)
				{
#ifdef CONFIG_RTL_LAYERED_DRIVER_L3		
					int32 ipIdx;
					if(rtl865x_getIpIdxByExtIp(select_t->extIp, &ipIdx)==SUCCESS)
					{
						asic_mcast.extIdx=(uint16)ipIdx;
					}
#else
					asic_mcast.extIdx=0;
#endif
				}

				retval = rtl8651_setAsicIpMulticastTable(&asic_mcast);
#ifdef CONFIG_PROC_FS
				mcastAddOpCnt++;
#endif
				assert(retval == SUCCESS);
				if(retval==SUCCESS)
				{
					select_t->inAsic = TRUE;
				}
				else
				{
					select_t->inAsic = FALSE;
					rtl8651_delAsicIpMulticastTable(entryIndex);
#ifdef CONFIG_PROC_FS
					mcastDelOpCnt++;
#endif
				}
				
				TAILQ_REMOVE(&mCastTbl.inuseList.mCastTbl[entryIndex], select_t, nextMCast);
				TAILQ_INSERT_HEAD(&mCastTbl.inuseList.mCastTbl[entryIndex], select_t, nextMCast);

			}
			#if 0
			else
			{			
				if(swapOutEntry->inAsic == FALSE)
				{
					/*maybe something is wrong, we remove the asic entry*/
					rtl8651_delAsicIpMulticastTable(entryIndex);
#ifdef CONFIG_PROC_FS
					mcastDelOpCnt++;
#endif
				}
				
			}	
			#endif
			
		}
		
	}
	else 	
	{
		if(swapOutEntry!=NULL)
		{
			swapOutEntry->inAsic=FALSE;
		}
		rtl8651_delAsicIpMulticastTable(entryIndex);
#ifdef CONFIG_PROC_FS
		mcastDelOpCnt++;
#endif
	}
}


static void _rtl865x_mCastEntryReclaim(void)
{
	uint32 index;
	uint32 freeCnt=0;
	uint32 asicFwdPortMask=0;
	uint32 needReArrange=FALSE;
	rtl865x_tblDrv_mCast_t *curMCastEntry, *nextMCastEntry;

	/*free unused software forward entry*/
	for(index=0; index<RTL8651_MULTICASTTBL_SIZE; index++) 
	{
		curMCastEntry = TAILQ_FIRST(&mCastTbl.inuseList.mCastTbl[index]);
		while (curMCastEntry)
		{
			nextMCastEntry = TAILQ_NEXT(curMCastEntry, nextMCast);
			if((curMCastEntry->inAsic==FALSE)  && (curMCastEntry->count==0))
			{
				_rtl865x_freeMCastEntry(curMCastEntry, index);
				freeCnt++;
			}
			curMCastEntry = nextMCastEntry;
		}
		
	}

	if(freeCnt>0)
	{
		return;
	}
	
	for(index=0; index<RTL8651_MULTICASTTBL_SIZE; index++) 
	{
		curMCastEntry = TAILQ_FIRST(&mCastTbl.inuseList.mCastTbl[index]);
		needReArrange=FALSE;
		while (curMCastEntry)
		{
			nextMCastEntry = TAILQ_NEXT(curMCastEntry, nextMCast);
			if(curMCastEntry->inAsic)
			{
				asicFwdPortMask=rtl865x_genMCastEntryAsicFwdMask(curMCastEntry);
				if(asicFwdPortMask==0) 
				{
					_rtl865x_freeMCastEntry(curMCastEntry, index);
					needReArrange=TRUE;
				}
			}
			curMCastEntry = nextMCastEntry;
		}
		
		if(needReArrange==TRUE)
		{
			_rtl865x_arrangeMulticast(index);
		}
	}

	return;
}
/*
@func rtl865x_tblDrv_mCast_t *	| rtl865x_findMCastEntry	|  API to find a hardware multicast entry.
@parm  ipaddr_t 	| mAddr	| Multicast stream destination group address. 
@parm  ipaddr_t	|  sip	| Multicast stream source ip address.
@parm  uint16		| svid	| Multicast stream input vlan index.
@parm  uint16 	| sport	| Multicast stream input port number.
*/
rtl865x_tblDrv_mCast_t *rtl865x_findMCastEntry(ipaddr_t mAddr, ipaddr_t sip, uint16 svid, uint16 sport)
{
	rtl865x_tblDrv_mCast_t *mCast_t;
	uint32 entry = rtl8651_ipMulticastTableIndex(sip, mAddr);
	TAILQ_FOREACH(mCast_t, &mCastTbl.inuseList.mCastTbl[entry], nextMCast) {
		if (mCast_t->dip==mAddr && mCast_t->sip==sip && mCast_t->svid==svid && mCast_t->port==sport)
		{
			if (mCast_t->inAsic == FALSE) 
			{
				mCast_t->age = RTL865X_MULTICAST_TABLE_AGE;
				mCast_t->count ++;
			}

			return mCast_t;
		}
	}
#if defined (CONFIG_RTL_HARDWARE_MULTICAST_CAM)
	for(entry=RTL8651_IPMULTICASTTBL_SIZE; entry<RTL8651_MULTICASTTBL_SIZE; entry++)
	{
		TAILQ_FOREACH(mCast_t, &mCastTbl.inuseList.mCastTbl[entry], nextMCast) {
			if (mCast_t->dip==mAddr && mCast_t->sip==sip && mCast_t->svid==svid && mCast_t->port==sport)
			{
				if (mCast_t->inAsic == FALSE) 
				{
					mCast_t->age = RTL865X_MULTICAST_TABLE_AGE;
					mCast_t->count ++;
				}

				return mCast_t;
			}
					
		}
	}
#endif	
	return (rtl865x_tblDrv_mCast_t *)NULL;	
}


#if defined (CONFIG_RTL_HARDWARE_MULTICAST_CAM)

int rtl865x_findEmptyCamEntry(void)
{
	int index=-1;

	for(index=RTL8651_IPMULTICASTTBL_SIZE; index<RTL8651_MULTICASTTBL_SIZE; index++)
	{

		if(TAILQ_EMPTY(&mCastTbl.inuseList.mCastTbl[index]))
		{
			return index;
		}
	}
	
	return -1;
}
#endif

/*
@func int32	| rtl865x_addMulticastEntry	|  API to add a hardwawre multicast forwarding entry.
@parm  ipaddr_t 	| mAddr	| Multicast flow Destination group address. 
@parm  ipaddr_t 	| sip	| Multicast flow source ip address. 
@parm  uint16 	| svid	| Multicast flow input vlan index. 
@parm  uint16		| sport	| Multicast flow input port number. 
@parm  rtl865x_mcast_fwd_descriptor_t *	| newFwdDescChain	| Multicast flow forwarding descriptor chain to be added. 
@parm  int32 	| flushOldChain	| Flag to indicate to flush old mulicast forwarding descriptor chain or not. 1 is to flush old chain, and 0 is not to. 
@parm  ipaddr_t 	| extIp	| External source ip address used when forward multicast data from lan to wan. 
@parm  int8	| toCpu	| Cpu forwarding flag, 1 is to forward multicast data by cpu,and  0 is not.
@parm  int8	| flag	| For future usage, set to 0 at present.
@rvalue SUCCESS	|Add hardware multicast forwarding entry successfully. 
@rvalue FAILED	|Add hardware multicast forwarding entry failed.
*/
#if defined CONFIG_RTL_MULTICAST_PORT_MAPPING
int32 rtl865x_addMulticastEntry(ipaddr_t mAddr, ipaddr_t sip, uint16 svid, uint16 sport, 
									rtl865x_mcast_fwd_descriptor_t * newFwdDescChain, 
									int32 flushOldChain, ipaddr_t extIp, char cpuHold, uint8 flag, unsigned int mapPortMask)
#else
int32 rtl865x_addMulticastEntry(ipaddr_t mAddr, ipaddr_t sip, uint16 svid, uint16 sport, 
									rtl865x_mcast_fwd_descriptor_t * newFwdDescChain, 
									int32 flushOldChain, ipaddr_t extIp, char cpuHold, uint8 flag)
#endif
{

	rtl865x_tblDrv_mCast_t *mCast_t;
	uint32 hashIndex = rtl8651_ipMulticastTableIndex(sip, mAddr);
	#if defined (CONFIG_RTL_HARDWARE_MULTICAST_CAM)
	uint32 emptyCamIndex=-1; 
	#endif
	#if defined (CONFIG_RTL_IGMP_SNOOPING)
	struct rtl_groupInfo groupInfo;
	#endif	
#if defined(CONFIG_RTL_HARDWARE_MULTICAST_API)&&defined(CONFIG_RTL_8197F)
	int32 wanOps, wanPPP;
	uint32 groupAddr[1] = {0};
#endif
	/*windows xp upnp:239.255.255.0*/
	if(mAddr==0xEFFFFFFA)
	{
		return FAILED;
	}
#if 0
	/*reserved multicast address 224.0.0.x*/
	if((mAddr & 0xFFFFFF00) == 0xE0000000)
	{
		return FAILED;
	}
#endif
#if defined(CONFIG_RTL_HARDWARE_MULTICAST_API)&&defined(CONFIG_RTL_8197F)
	groupAddr[0] = mAddr;
	if(SUCCESS!=rtl_getKernelMCastWanType(groupAddr,IP_VERSION4, &wanOps,&wanPPP))
		return FAILED;		
#endif


#if defined (CONFIG_RTL_HARDWARE_MULTICAST_CAM)
	mCast_t=rtl865x_findMCastEntry(mAddr, sip, svid, sport);
	if(mCast_t==NULL)
	{
		/*table entry collided*/
		if(!TAILQ_EMPTY(&mCastTbl.inuseList.mCastTbl[hashIndex]))
		{
			emptyCamIndex=rtl865x_findEmptyCamEntry();
			if(emptyCamIndex!=-1)
			{
				hashIndex=emptyCamIndex;
			}
		}
	}
	else
	{
		hashIndex=mCast_t->hashIndex;
	}
#else
	/*try to match hash line*/
	TAILQ_FOREACH(mCast_t, &mCastTbl.inuseList.mCastTbl[hashIndex], nextMCast) 
	{
		if (mCast_t->sip==sip && mCast_t->dip==mAddr && mCast_t->svid==svid && mCast_t->port==sport)
			break;
	}
#endif	
	
	if (mCast_t == NULL) 
	{
		mCast_t=_rtl865x_allocMCastEntry(hashIndex);
		if (mCast_t == NULL)
		{
			_rtl865x_mCastEntryReclaim();
			mCast_t=_rtl865x_allocMCastEntry(hashIndex);
			if(mCast_t == NULL)
			{
				return FAILED;
			}
		}
		mCast_t->sip			= sip;
		mCast_t->dip			= mAddr;
		mCast_t->svid		= svid;
		mCast_t->port		= sport;
		mCast_t->mbr		= 0;
		mCast_t->count		= 0;
		//mCast_t->maxPPS		= 0;
		
		#if defined CONFIG_RTL_MULTICAST_PORT_MAPPING
		mCast_t->mapPortMbr = mapPortMask;
		#endif
		
		mCast_t->inAsic		= FALSE;
	}
	
	if(flushOldChain)
	{
		_rtl865x_flushMCastFwdDescChain(&mCast_t->fwdDescChain);
		
	}
	
	_rtl865x_mergeMCastFwdDescChain(&mCast_t->fwdDescChain,newFwdDescChain);
	_rtl865x_doMCastEntrySrcVlanPortFilter(mCast_t);
	
	mCast_t->mbr			= rtl865x_genMCastEntryAsicFwdMask(mCast_t);
	mCast_t->extIp			= extIp;

	mCast_t->age			= RTL865X_MULTICAST_TABLE_AGE;
#if 0
	mCast_t->cpu			= (toCpu==TRUE? 1: 0);
#else
	mCast_t->cpuHold			= cpuHold;
	mCast_t->cpu 			= rtl865x_genMCastEntryCpuFlag(mCast_t);
#endif	
	mCast_t->flag			= flag;
	
	if (extIp)
		mCast_t->flag |= RTL865X_MULTICAST_EXTIP_SET;
	else
		mCast_t->flag &= ~RTL865X_MULTICAST_EXTIP_SET;
	#if defined (CONFIG_RTL_IGMP_SNOOPING)
	rtl_getGroupInfo(mAddr, &groupInfo);
	if(groupInfo.ownerMask==0)
	{
		mCast_t->unKnownMCast=TRUE;
	}
	else
	{
		mCast_t->unKnownMCast=FALSE;
	}
	#endif
#if defined (CONFIG_RTL_HARDWARE_MULTICAST_CAM)
	mCast_t->hashIndex=hashIndex;
#endif

#if defined(CONFIG_RTL_8197F)
#if defined(CONFIG_RTL_HARDWARE_MULTICAST_API)
	mCast_t->destinterface = rtl_getKernelMCastDifbyWanOps(wanOps);
#else
	mCast_t->destinterface = 0;
#endif
#endif
	

	_rtl865x_patchPppoeWeak(mCast_t);
	_rtl865x_arrangeMulticast(hashIndex);
	return SUCCESS;	
}


/*
@func int32	| rtl865x_delMulticastEntry	|  API to delete multicast forwarding entry related with a certain group address.
@parm  ipaddr_t 	| mcast_addr	| Group address to be mached in deleting hardware multicast forwarding entry. 
@rvalue SUCCESS	|Delete hardware multicast forwarding entry successfully. 
@rvalue FAILED	|Delete hardware multicast forwarding entry failed.
*/
int32 rtl865x_delMulticastEntry(ipaddr_t mcast_addr)
{

	rtl865x_tblDrv_mCast_t *mCastEntry, *nextMCastEntry;
	uint32 entry;
	uint32 deleteFlag=FALSE;

	for(entry=0; entry<RTL8651_MULTICASTTBL_SIZE; entry++) 
	{
		deleteFlag=FALSE;
		mCastEntry = TAILQ_FIRST(&mCastTbl.inuseList.mCastTbl[entry]);
		while (mCastEntry)
		{
			nextMCastEntry = TAILQ_NEXT(mCastEntry, nextMCast);
			if (!mcast_addr || mCastEntry->dip == mcast_addr) 
			{
				deleteFlag=TRUE;
				_rtl865x_freeMCastEntry(mCastEntry, entry);
			}
			
			mCastEntry = nextMCastEntry;
		}
		
		if(deleteFlag==TRUE)
		{
			_rtl865x_arrangeMulticast(entry);
		}
	}

	return SUCCESS;
}

/*
@func int32	| rtl865x_updateMulticastEntry	|  API to update multicast forwarding entry related with a certain couple of group and source address.
@parm  ipaddr_t 	| mAddr	| Group address to be mached in updating hardware multicast forwarding entry. 
@parm  ipaddr_t 	| sip	| Source address to be mached in updating hardware multicast forwarding entry. 
@parm  rtl865x_mcast_fwd_descriptor_t * 	| fwdDescChain	| Multicast flow forwarding descriptor chain to be updated. . 
@rvalue SUCCESS	|Update hardware multicast forwarding entry successfully. 
@rvalue FAILED	|Update hardware multicast forwarding entry failed.
*/
int rtl865x_updateMulticastEntry(ipaddr_t mAddr, ipaddr_t sip, rtl865x_mcast_fwd_descriptor_t * newFwdDesc)
{
	rtl865x_tblDrv_mCast_t	*mCastEntry,*nextMCastEntry;
	int index;
	unsigned int oldAsicFwdPortMask, newAsicFwdPortMask;
	int oldCpuFlag, newCpuFlag;
	
	for (index=0; index< RTL8651_MULTICASTTBL_SIZE; index++)
	{
		for (mCastEntry = TAILQ_FIRST(&mCastTbl.inuseList.mCastTbl[index]); mCastEntry; mCastEntry = nextMCastEntry)
		{
			nextMCastEntry=TAILQ_NEXT(mCastEntry, nextMCast);
			//this function will be called when time out or receive a igmp report, the src may not be known
			if ( (mAddr!=0 && mCastEntry->dip != mAddr) || (sip!=0 && mCastEntry->sip != sip) )
			{
				continue;
			}

			oldAsicFwdPortMask=mCastEntry->mbr;
			oldCpuFlag=mCastEntry->cpu;

			//printk("oldFwdPortMask:%x, oldCpuFlag:%d, [%s:%d]\n", oldAsicFwdPortMask, oldCpuFlag,__FUNCTION__, __LINE__);
			/*update/replace old forward descriptor*/
			_rtl865x_mergeMCastFwdDescChain(&mCastEntry->fwdDescChain,newFwdDesc);

			mCastEntry->mbr 		= rtl865x_genMCastEntryAsicFwdMask(mCastEntry);
			mCastEntry->cpu		= rtl865x_genMCastEntryCpuFlag(mCastEntry);
			newAsicFwdPortMask	= mCastEntry->mbr ;
			newCpuFlag			= mCastEntry->cpu;

			//printk("newFwdPortMask:%x, newCpuFlag:%d, [%s:%d]\n", newAsicFwdPortMask, newCpuFlag,__FUNCTION__, __LINE__);

			if((oldCpuFlag != newCpuFlag)||(newAsicFwdPortMask!=oldAsicFwdPortMask)) 
			{
				_rtl865x_patchPppoeWeak(mCastEntry);
				
				/*reset inAsic flag to re-select or re-write this hardware asic entry*/
				if(newAsicFwdPortMask==0)
				{
					_rtl865x_freeMCastEntry(mCastEntry, index);
				}
			
				_rtl865x_arrangeMulticast(index);
			}
		}
	}
	
	//DBG_MCAST_PRK("Leave %s with SUCCES\n",__func__);
	return SUCCESS; 	
}

#if 0
/*the following function maybe used in future*/

int32 rtl865x_addMulticastFwdDesc(ipaddr_t mcast_addr, rtl865x_mcast_fwd_descriptor_t * newFwdDesc)
{

	rtl865x_tblDrv_mCast_t *mCast_t;
	uint32 entry, matchedIdx = 0;
	uint32 oldFwdPortMask,newFwdPortMask;
	if(newFwdDesc==NULL)
	{
		return SUCCESS;
	}

	for (entry=0; entry< RTL8651_MULTICASTTBL_SIZE; entry++)
	{
		TAILQ_FOREACH(mCast_t, &mCastTbl.inuseList.mCastTbl[entry], nextMCast)
		{
			if (mCast_t->dip != mcast_addr)
				continue;

			oldFwdPortMask=mCast_t->mbr;

			_rtl865x_mergeMCastFwdDescChain(&mCast_t->fwdDescChain,newFwdDesc);
			_rtl865x_doMCastEntrySrcVlanPortFilter(mCast_t);
			
			mCast_t->mbr 		= rtl865x_genMCastEntryFwdMask(mCast_t);
			newFwdPortMask		= mCast_t->mbr ;
#ifndef RTL8651_MCAST_ALWAYS2UPSTREAM
			if (mCast_t->flag & RTL865X_MULTICAST_UPLOADONLY)
			{	/* remove upload term*/
				if(oldFwdPortMask!=newFwdPortMask)
				{
					mCast_t->flag &= ~RTL865X_MULTICAST_UPLOADONLY;
					/* we assume multicast member will NEVER in External interface, so we remove
					     external ip now */
					mCast_t->flag &= ~RTL865X_MULTICAST_EXTIP_SET;
					mCast_t->extIp= 0;
				}
			}
#endif /* RTL8651_MCAST_ALWAYS2UPSTREAM */

			_rtl865x_patchPppoeWeak(mCast_t);
			_rtl865x_arrangeMulticast(entry);
			matchedIdx = entry;
		}
	}

	if (matchedIdx) 
	{
		return SUCCESS;
	}
	return FAILED;
}

int32 rtl865x_delMulticastFwdDesc(ipaddr_t mcast_addr,  rtl865x_mcast_fwd_descriptor_t * deadFwdDesc)
{

	uint32 index;
	rtl865x_tblDrv_mCast_t  *mCastEntry, *nextMCastEntry;
	uint32 oldFwdPortMask,newFwdPortMask;
	
	for(index=0; index<RTL8651_MULTICASTTBL_SIZE; index++) 
	{

		for (mCastEntry = TAILQ_FIRST(&mCastTbl.inuseList.mCastTbl[index]); mCastEntry; mCastEntry = nextMCastEntry)
		{
			nextMCastEntry=TAILQ_NEXT(mCastEntry, nextMCast);
			
			if ((mcast_addr) && (mCastEntry->dip != mcast_addr))
			{
				continue;
			}
			
			oldFwdPortMask=mCastEntry->mbr;
		
			_rtl865x_subMCastFwdDescChain(&mCastEntry->fwdDescChain, deadFwdDesc);
			
			mCastEntry->mbr=rtl865x_genMCastEntryFwdMask(mCastEntry);
			newFwdPortMask=mCastEntry->mbr; 	
			if (mCastEntry->mbr == 0)
			{
				/*to-do:unknown multicast hardware blocking*/
				_rtl865x_freeMCastEntry(mCastEntry, index);
				mCastEntry=NULL;
				_rtl865x_arrangeMulticast(index);
			}
			else
			{
			
				_rtl865x_patchPppoeWeak(mCastEntry);
			}
			
		}
			
		_rtl865x_arrangeMulticast(index);
	}

	return SUCCESS;
}

int32 rtl865x_delMulticastUpStream(ipaddr_t mcast_addr, ipaddr_t sip, uint16 svid, uint16 sport)
{
	uint32 index;
	rtl865x_tblDrv_mCast_t *mCast_t;
	
	for(index=0; index<RTL8651_MULTICASTTBL_SIZE; index++) 
	{
		TAILQ_FOREACH(mCast_t, &mCastTbl.inuseList.mCastTbl[index], nextMCast) 
		{
			if ((!mcast_addr || mCast_t->dip == mcast_addr) && 
				(!sip || mCast_t->sip==sip) && 
				(!svid || mCast_t->svid==svid) && 
				mCast_t->port==sport)
			{
				_rtl865x_freeMCastEntry(mCast_t, index);
				_rtl865x_arrangeMulticast(index);
				return SUCCESS;
			}
		}
	}
	return FAILED;
}

int32 rtl865x_delMulticastByVid(uint32 vid)
{
	uint16 sport;
	uint32 sportMask;
	rtl865x_mcast_fwd_descriptor_t vlanFwdDesc;
	memset(&vlanFwdDesc,0,sizeof(rtl865x_mcast_fwd_descriptor_t));
	
	/* delete all upstream related to vid */
	sport = 0;
	sportMask=rtl865x_getVlanPortMask(vid);
	while (sportMask) 
	{
		if (sportMask & 1)
		{
			rtl865x_delMulticastUpStream(0, 0, vid, sport);
		}
		
		sportMask = sportMask >> 1;
		sport ++;
	}
	
	/* delete all downstream related to vid*/
	vlanFwdDesc.vid=vid;
	vlanFwdDesc.fwdPortMask=rtl865x_getVlanPortMask(vid);
	rtl865x_delMulticastFwdDesc(0, &vlanFwdDesc);

	return FAILED;
}

int32 rtl865x_delMulticastByPort(uint32 port)
{

	rtl865x_mcast_fwd_descriptor_t portFwdDesc;
	memset(&portFwdDesc,0,sizeof(rtl865x_mcast_fwd_descriptor_t));
	
	/* delete all upstream related to this port */
	rtl865x_delMulticastUpStream(0, 0, 0, port);

	/* delete all downstream related to this port*/
	portFwdDesc.vid=0;
	portFwdDesc.fwdPortMask=1<<port;
	rtl865x_delMulticastFwdDesc(0, &portFwdDesc);

	return SUCCESS;
}

int32 rtl865x_setMGroupAttribute(ipaddr_t groupIp, int8 toCpu)
{
	uint32 index;
	rtl865x_tblDrv_mCast_t *mCast_t;

	for(index=0; index<RTL8651_MULTICASTTBL_SIZE; index++) 
	{
		TAILQ_FOREACH(mCast_t, &mCastTbl.inuseList.mCastTbl[index], nextMCast) 
		{
			if (mCast_t->dip == groupIp)
			{
				mCast_t->cpu = (toCpu==TRUE? 1: 0);
			}
		}
		_rtl865x_arrangeMulticast(index);
	}
	return SUCCESS;
}


static int32 _rtl865x_subMCastFwdDescChain(mcast_fwd_descriptor_head_t * targetChainHead,rtl865x_mcast_fwd_descriptor_t *srcChain)
{
	rtl865x_mcast_fwd_descriptor_t *curDesc;
	if(targetChainHead==NULL)
	{
		return FAILED;
	}
	
	for(curDesc=srcChain; curDesc!=NULL; curDesc=MC_LIST_NEXT(curDesc,next))
	{
		_rtl865x_mCastFwdDescDequeue(targetChainHead, curDesc);
	}

	return SUCCESS;
}

static int32 _rtl865x_mCastFwdDescDequeue(mcast_fwd_descriptor_head_t * queueHead,rtl865x_mcast_fwd_descriptor_t * dequeueDesc)
{
	rtl865x_mcast_fwd_descriptor_t *curDesc,*nextDesc;
	
	if(queueHead==NULL)
	{
		return FAILED;
	}
	
	if(dequeueDesc==NULL)
	{
		return FAILED;
	}

	for(curDesc=MC_LIST_FIRST(queueHead);curDesc!=NULL;curDesc=nextDesc)
	{
		nextDesc=MC_LIST_NEXT(curDesc, next );
		if((strcmp(curDesc->netifName,dequeueDesc->netifName)==0) ||
			((dequeueDesc->vid==0 ) ||(curDesc->vid==dequeueDesc->vid)))
		{
			curDesc->fwdPortMask &= (~dequeueDesc->fwdPortMask);
			if(curDesc->fwdPortMask==0)
			{
				/*remove from the old descriptor chain*/
				MC_LIST_REMOVE(curDesc, next);
				/*return to the free descriptor chain*/
				_rtl865x_freeMCastFwdDesc(curDesc);

			}

			return SUCCESS;
		}
	}

	/*never reach here*/
	return SUCCESS;
}

#endif

#if defined(CONFIG_BRIDGE_IGMP_SNOOPING)
int32 rtl865x_multicastUpdate(rtl865x_mcast_fwd_descriptor_t* desc)
{
	uint32 index;
	//uint32 oldDescPortMask,newDescPortMask;/*for device decriptor forwarding usage*/
	uint32 oldAsicFwdPortMask,newAsicFwdPortMask;/*for physical port forwarding usage*/
	uint32 oldCpuFlag,newCpuFlag;
	
	rtl865x_mcast_fwd_descriptor_t newFwdDesc;
	rtl865x_tblDrv_mCast_t	*mCastEntry,*nextMCastEntry;
	//printk("[%s:%d]\n",__FUNCTION__,__LINE__);
	if(desc==NULL)
	{
		//printk("[%s:%d]failed\n",__FUNCTION__,__LINE__);
		return FAILED;
	}
	/*check device name's validity*/
	if(strlen(desc->netifName)==0)
	{
		//printk("[%s:%d]failed\n",__FUNCTION__,__LINE__);
		return FAILED;
	}
#ifdef CONFIG_RTK_VLAN_WAN_TAG_SUPPORT
	if(memcmp(desc->netifName,RTL_BR_NAME,3)==0 || memcmp(desc->devName,RTL_BR1_NAME,3)==0)
#else
	if(memcmp(desc->netifName,RTL_BR_NAME,3)==0)
#endif
	{
		for (index=0; index< RTL8651_MULTICASTTBL_SIZE; index++)
		{
			for (mCastEntry = TAILQ_FIRST(&mCastTbl.inuseList.mCastTbl[index]); mCastEntry; mCastEntry = nextMCastEntry)
			{
				nextMCastEntry=TAILQ_NEXT(mCastEntry, nextMCast);
				//printk("[%s:%d]hw dip is:0x%x,cur dip is 0x%x\n",__FUNCTION__,__LINE__,mCastEntry->dip,desc->dip);	
				if (mCastEntry->dip != desc->dip)
				{
					continue;
				}
				memset(&newFwdDesc, 0 ,sizeof(rtl865x_mcast_fwd_descriptor_t));
				strcpy(newFwdDesc.netifName,desc->netifName);
	
				/*save old multicast entry forward port mask*/
				oldAsicFwdPortMask=mCastEntry->mbr;
				oldCpuFlag=mCastEntry->cpu;
	
				/*sync with control plane*/
				newFwdDesc.fwdPortMask=desc->fwdPortMask & (~(1<<mCastEntry->port));
				newFwdDesc.toCpu=desc->toCpu;
				
				/*update/replace old forward descriptor*/
					
				_rtl865x_mergeMCastFwdDescChain(&mCastEntry->fwdDescChain,&newFwdDesc);
				mCastEntry->mbr 	= rtl865x_genMCastEntryAsicFwdMask(mCastEntry);
				mCastEntry->cpu 	= rtl865x_genMCastEntryCpuFlag(mCastEntry);
					
				newAsicFwdPortMask	= mCastEntry->mbr ;
				newCpuFlag			=mCastEntry->cpu;
				/*printk("[%s:%d]old fwPortMask is %d,newfwPortMask is %d\n",
					   __FUNCTION__,
					   __LINE__,
					   oldAsicFwdPortMask,
					   newAsicFwdPortMask);*/	
#ifndef RTL8651_MCAST_ALWAYS2UPSTREAM
				if (mCastEntry->flag & RTL865X_MULTICAST_UPLOADONLY)
				{	/* remove upload term*/
					if((newAsicFwdPortMask!=0) && (newAsicFwdPortMask!=oldAsicFwdPortMask))
					{
						mCastEntry->flag &= ~RTL865X_MULTICAST_UPLOADONLY;
						/* we assume multicast member will NEVER in External interface, so we remove
								 external ip now */
						mCastEntry->flag &= ~RTL865X_MULTICAST_EXTIP_SET;
						mCastEntry->extIp= 0;
					}
				}
#endif /* RTL8651_MCAST_ALWAYS2UPSTREAM */
	
				mCastEntry->unKnownMCast=FALSE;
					
				if((oldCpuFlag != newCpuFlag)||(newAsicFwdPortMask!=oldAsicFwdPortMask)) 
				{
					_rtl865x_patchPppoeWeak(mCastEntry);
						
					/*reset inAsic flag to re-select or re-write this hardware asic entry*/
					if(newAsicFwdPortMask==0)
					{
						_rtl865x_freeMCastEntry(mCastEntry, index);
					}
						
					_rtl865x_arrangeMulticast(index);
				}
			}
				
		}
	}
	return SUCCESS;
}
#endif
#if defined(CONFIG_RTL_MULTI_LAN_DEV)
extern struct net_bridge* bridge0;
extern unsigned int rtl865x_getPhyFwdPortMask(struct net_bridge *br,unsigned int brFwdPortMask);
#endif
#if defined(CONFIG_RT_MULTIPLE_BR_SUPPORT)
extern int rtl_get_brIgmpModuleIndexbyId(int idx,char *name);
#if defined(CONFIG_RTL_VLAN_8021Q)&&defined(CONFIG_RTL_MULTICAST_PORT_MAPPING)
unsigned int rtl865x_getWlanPhyFwdPortMaskbyVlan(struct net_bridge *br,unsigned int brFwdPortMask);
#endif
#endif

static int32 rtl865x_multicastCallbackFn(void *param)
{
#if defined (CONFIG_RTL_IGMP_SNOOPING)
	uint32 index;
#if !defined(CONFIG_RTL_MULTI_LAN_DEV)
	uint32 oldDescPortMask=0,newDescPortMask=0;/*for device decriptor forwarding usage*/
#endif	
	uint32 oldAsicFwdPortMask=0,newAsicFwdPortMask=0;/*for physical port forwarding usage*/
	uint32 oldCpuFlag=0,newCpuFlag=0;
	
	rtl_multicastEventContext_t mcastEventContext;

	rtl865x_mcast_fwd_descriptor_t newFwdDesc;
	struct rtl_multicastDataInfo multicastDataInfo;
	struct rtl_multicastFwdInfo multicastFwdInfo;
	rtl865x_tblDrv_mCast_t  *mCastEntry,*nextMCastEntry;
#if !defined(CONFIG_RTL_MULTI_LAN_DEV) && !defined(CONFIG_RT_MULTIPLE_BR_SUPPORT)
	struct rtl_multicastDeviceInfo_s bridgeMCastDev;
#endif

#if defined (CONFIG_RT_MULTIPLE_BR_SUPPORT)
unsigned char br_name[16]={0};
int br_index;

#if defined (CONFIG_RTL_MULTI_LAN_DEV)
	struct net_bridge* br_dev;
#else
	unsigned int br_moduleIndex = 0xFFFFFFFF;
#if defined (CONFIG_RTL_HW_MCAST_WIFI)
	unsigned int currentCpuFlag = 0;
#endif
#if defined(CONFIG_RTL_VLAN_8021Q)&&defined(CONFIG_RTL_MULTICAST_PORT_MAPPING)
struct net_bridge* br_tmp;
unsigned int wlanPhyPortMask;
#endif
#endif
#endif


#if 0//defined(CONFIG_RTL_ISP_MULTIPLE_BR_SUPPORT) && defined(CONFIG_RTL_MULTI_LAN_DEV)
//need to check, some are used only for CONFIG_RTL_MULTI_LAN_DEV
	struct net_bridge* br_dev;
	int  br_indextmp;
	int br_moduleIndex = 0xFFFFFFFF;
	unsigned char br_name[16]={0};
	unsigned int fwdPortMask_tmp = 0xFFFFFFFF;
	unsigned int cpuTrueNum = 0, bridgeNum = 0;
	unsigned int joinBridgeNum = 0;
#endif
	struct rtl_groupInfo groupInfo;
	int32 retVal=FAILED;

	if(param==NULL)
	{
		return EVENT_CONTINUE_EXECUTE;
	}
	memcpy(&mcastEventContext,param,sizeof(rtl_multicastEventContext_t));
	/*check device name's validity*/
	if(strlen(mcastEventContext.devName)==0)
	{
		return EVENT_CONTINUE_EXECUTE;
	}
	#ifdef CONFIG_RTL865X_MUTLICAST_DEBUG
	printk("%s:%d,mcastEventContext.devName is %s,moduleindex:%x, mcastEventContext.groupAddr is 0x%x,mcastEventContext.sourceAdd is 0x%x,mcastEventContext.portMask is 0x%x\n",__FUNCTION__,__LINE__,mcastEventContext.devName, mcastEventContext.moduleIndex,mcastEventContext.groupAddr[0], mcastEventContext.sourceAddr[0], mcastEventContext.portMask);
	#endif

#if defined (CONFIG_RTL_MULTI_LAN_DEV)
#if defined (CONFIG_RT_MULTIPLE_BR_SUPPORT)
	if(memcmp(mcastEventContext.devName,RTL_BR_NAME,2)==0)
#else
	if(memcmp(mcastEventContext.devName,RTL_BR_NAME,3)==0)
#endif
	{
		#ifdef CONFIG_RTL865X_MUTLICAST_DEBUG
		printk("%s:%d,multicast event from %s,mcastEventContext.groupAddr[0] is 0x%x\n",__FUNCTION__,__LINE__,mcastEventContext.devName,mcastEventContext.groupAddr[0]);
		#endif
		
		for (index=0; index< RTL8651_MULTICASTTBL_SIZE; index++)
		{
			for (mCastEntry = TAILQ_FIRST(&mCastTbl.inuseList.mCastTbl[index]); mCastEntry; mCastEntry = nextMCastEntry)
			{
				nextMCastEntry=TAILQ_NEXT(mCastEntry, nextMCast);
				
				if ((mcastEventContext.groupAddr!=0) && (mCastEntry->dip != mcastEventContext.groupAddr[0]))
				{
					continue;
				}
				
				memset(&newFwdDesc, 0 ,sizeof(rtl865x_mcast_fwd_descriptor_t));
				strcpy(newFwdDesc.netifName,mcastEventContext.devName);

				/*save old multicast entry forward port mask*/
				oldAsicFwdPortMask=mCastEntry->mbr;
				oldCpuFlag=mCastEntry->cpu;

				/*sync with control plane*/
				multicastDataInfo.ipVersion=4;
				multicastDataInfo.sourceIp[0]=  mCastEntry->sip;
				multicastDataInfo.groupAddr[0]= mCastEntry->dip;

				#if 0
				//#if defined (CONFIG_RTL_ISP_MULTIPLE_BR_SUPPORT)
				for(br_index = 0; br_index<RTL_IMGP_MAX_BRMODULE; br_index++)
				{
					br_dev = rtl_get_brByIndex(br_index, br_name);
					if(br_dev)
					{
						bridgeNum ++;
						br_moduleIndex = rtl_get_brIgmpModuleIndexbyName(br_name,&br_indextmp);//br_indextmep should be equal to br_index
						retVal= rtl_getMulticastDataFwdInfo(br_moduleIndex, &multicastDataInfo, &multicastFwdInfo);
						if (multicastFwdInfo.unknownMCast == TRUE)
							cpuTrueNum ++;
						fwdPortMask_tmp = rtl865x_getPhyFwdPortMask(br_dev,multicastFwdInfo.fwdPortMask);
						newFwdDesc.fwdPortMask |= fwdPortMask_tmp;
						#ifdef CONFIG_RTL865X_MUTLICAST_DEBUG
						printk("dev:%s, multicastFwdInfo.fwdPortMask:%x, fwdPortMask_tmp:%x, in_port:%x, newFwdPortMask:%x, [%s:%d]\n",
							br_name, multicastFwdInfo.fwdPortMask, fwdPortMask_tmp, mCastEntry->port, newFwdDesc.fwdPortMask, __FUNCTION__, __LINE__);
						#endif
					}
					newFwdDesc.fwdPortMask &= (~(1<<mCastEntry->port));
				}
				if(bridgeNum == cpuTrueNum)
					multicastFwdInfo.cpuFlag = TRUE;
					
				#else
				retVal= rtl_getMulticastDataFwdInfo(mcastEventContext.moduleIndex, &multicastDataInfo, &multicastFwdInfo);

				/*fix jwj*/
				if (multicastFwdInfo.unknownMCast == TRUE)
					multicastFwdInfo.cpuFlag = TRUE;

				if(retVal !=0)
				{
					multicastFwdInfo.fwdPortMask=0;
					multicastFwdInfo.cpuFlag = TRUE;
				}
				#endif

				#if defined CONFIG_RT_MULTIPLE_BR_SUPPORT
				br_dev = NULL;
				for(br_index = 0; br_index<RTL_IMGP_MAX_BRMODULE; br_index++)
				{
					br_dev = rtl_get_brByIndex(br_index, br_name);
					if(strcmp(br_name, mcastEventContext.devName)==0)
						break;
				}
				if(br_dev==NULL)
					continue;

				#if defined (CONFIG_RTL_HW_MCAST_WIFI)
				if(hwwifiEnable)
				{
					newFwdDesc.fwdPortMask=rtl865x_getPhyFwdPortMask(br_dev,multicastFwdInfo.fwdPortMask) & (~(1<<mCastEntry->port));
					if(retVal == SUCCESS)
					{
						if(multicastFwdInfo.cpuFlag == TRUE)
						{
							//wifi entry exist
							newFwdDesc.fwdPortMask |= (0x01<<6);
						}					
					}
				}
				else
				{
					newFwdDesc.fwdPortMask=rtl865x_getPhyFwdPortMask(br_dev,multicastFwdInfo.fwdPortMask) & (~(1<<mCastEntry->port));
				}
				#else
				newFwdDesc.fwdPortMask=rtl865x_getPhyFwdPortMask(br_dev,multicastFwdInfo.fwdPortMask) & (~(1<<mCastEntry->port));
				#endif
				#else
				
				#if defined (CONFIG_RTL_HW_MCAST_WIFI)
				if(hwwifiEnable)
				{

					newFwdDesc.fwdPortMask=rtl865x_getPhyFwdPortMask(bridge0,multicastFwdInfo.fwdPortMask) & (~(1<<mCastEntry->port));
					if(retVal == SUCCESS)
					{
						if(multicastFwdInfo.cpuFlag == TRUE)
						{
							//wifi entry exist
							newFwdDesc.fwdPortMask |= (0x01<<6);
						}					
					}
						
				}
				else
				{
					newFwdDesc.fwdPortMask=rtl865x_getPhyFwdPortMask(bridge0,multicastFwdInfo.fwdPortMask) & (~(1<<mCastEntry->port));
				}
				#else
				newFwdDesc.fwdPortMask=rtl865x_getPhyFwdPortMask(bridge0,multicastFwdInfo.fwdPortMask) & (~(1<<mCastEntry->port));
				#endif
				#endif
				
				newFwdDesc.toCpu=multicastFwdInfo.cpuFlag;
		#if defined	CONFIG_RTL865X_MUTLICAST_DEBUG
				panic_printk("newFwdDesc.tocpu %d, newFwdDesc.fwdPortMask:%x,[%s:%d]\n", multicastFwdInfo.cpuFlag, newFwdDesc.fwdPortMask, __FUNCTION__, __LINE__);
		#endif	
				/*update/replace old forward descriptor*/
				_rtl865x_mergeMCastFwdDescChain(&mCastEntry->fwdDescChain,&newFwdDesc);

				/*tocpu flag=1, DescFwdMask will be ignored, to cpu flag*/
			#if defined (CONFIG_RTL_HW_MCAST_WIFI)
				if(hwwifiEnable)
				{
					mCastEntry->mbr  	= rtl865x_genMCastEntryAsicFwdMaskIgnoreCpuFlag(mCastEntry);
				}
				else
				{
					mCastEntry->mbr 		= rtl865x_genMCastEntryAsicFwdMask(mCastEntry);
				}
			#else
				mCastEntry->mbr 		= rtl865x_genMCastEntryAsicFwdMask(mCastEntry);
			#endif
				mCastEntry->cpu		= rtl865x_genMCastEntryCpuFlag(mCastEntry);
				newAsicFwdPortMask	= mCastEntry->mbr ;
				newCpuFlag			=mCastEntry->cpu;
				
				#ifdef CONFIG_RTL865X_MUTLICAST_DEBUG
				printk("%s:%d,oldAsicFwdPortMask is %d,newAsicFwdPortMask is %d\n",__FUNCTION__,__LINE__,oldAsicFwdPortMask,newAsicFwdPortMask);
				#endif
				
#ifndef RTL8651_MCAST_ALWAYS2UPSTREAM
				if (mCastEntry->flag & RTL865X_MULTICAST_UPLOADONLY)
				{	/* remove upload term*/
					if((newAsicFwdPortMask!=0) && (newAsicFwdPortMask!=oldAsicFwdPortMask))
					{
						mCastEntry->flag &= ~RTL865X_MULTICAST_UPLOADONLY;
						/* we assume multicast member will NEVER in External interface, so we remove
						     external ip now */
						mCastEntry->flag &= ~RTL865X_MULTICAST_EXTIP_SET;
						mCastEntry->extIp= 0;
					}
				}
#endif /* RTL8651_MCAST_ALWAYS2UPSTREAM */

				rtl_getGroupInfo(mCastEntry->dip, &groupInfo);
				if(groupInfo.ownerMask==0)
				{
					mCastEntry->unKnownMCast=TRUE;
				}
				else
				{
					mCastEntry->unKnownMCast=FALSE;
				}
#if defined CONFIG_RTL865X_MUTLICAST_DEBUG				
				printk("old FwdPortMask:%x, newAsicFwdPortMask:%x, [%s:%d]\n", oldAsicFwdPortMask, newAsicFwdPortMask, __FUNCTION__, __LINE__);
#endif

#if defined CONFIG_RTL_MULTICAST_PORT_MAPPING
				newAsicFwdPortMask &= (mCastEntry->mapPortMbr |(0x01<<6));
#endif

#if defined(CONFIG_RTL_HW_MCAST_WIFI)
				if(hwwifiEnable)
				{
					if(((newAsicFwdPortMask)&(~(1<<6)))==0)
						newAsicFwdPortMask =0;
				}
#endif

#if defined CONFIG_RTL865X_MUTLICAST_DEBUG
				printk("old FwdPortMask:%x, newAsicFwdPortMask:%x, [%s:%d]\n", oldAsicFwdPortMask, newAsicFwdPortMask, __FUNCTION__, __LINE__);
#endif
				if((oldCpuFlag != newCpuFlag)||(newAsicFwdPortMask!=oldAsicFwdPortMask)) 
				{
					_rtl865x_patchPppoeWeak(mCastEntry);
					
					/*reset inAsic flag to re-select or re-write this hardware asic entry*/
					if(newAsicFwdPortMask==0)
					{
						_rtl865x_freeMCastEntry(mCastEntry, index);
					}

					_rtl865x_arrangeMulticast(index);
				}
			}

				
			
		}
	}

#else  //CONFIG_RTL_MULTI_LAN_DEV
    /*case 1:this is multicast event from bridge */
	/*sync wlan and ethernet*/
	//hyking:[Fix me] the RTL_BR_NAME...
#if defined (CONFIG_OPENWRT_SDK)
#ifdef CONFIG_RTK_VLAN_WAN_TAG_SUPPORT
    if(memcmp(mcastEventContext.devName,RTL_BR_NAME,6)==0 || memcmp(mcastEventContext.devName,RTL_BR1_NAME,3)==0)
#else
	if(memcmp(mcastEventContext.devName,RTL_BR_NAME,6)==0)
#endif
#else
#ifdef CONFIG_RTK_VLAN_WAN_TAG_SUPPORT
    if(memcmp(mcastEventContext.devName,RTL_BR_NAME,3)==0 || memcmp(mcastEventContext.devName,RTL_BR1_NAME,3)==0)
#elif defined (CONFIG_RT_MULTIPLE_BR_SUPPORT)
	if(memcmp(mcastEventContext.devName,RTL_BR_NAME,2)==0)
#else
	if(memcmp(mcastEventContext.devName,RTL_BR_NAME,3)==0)
#endif
#endif
	{

		for (index=0; index< RTL8651_MULTICASTTBL_SIZE; index++)
		{
			for (mCastEntry = TAILQ_FIRST(&mCastTbl.inuseList.mCastTbl[index]); mCastEntry; mCastEntry = nextMCastEntry)
			{
				nextMCastEntry=TAILQ_NEXT(mCastEntry, nextMCast);
				
				if ((mcastEventContext.groupAddr!=0) && (mCastEntry->dip != mcastEventContext.groupAddr[0]))
				{
					continue;
				}

				rtl_getGroupInfo(mCastEntry->dip, &groupInfo);
				if(groupInfo.ownerMask==0)
				{
					mCastEntry->unKnownMCast=TRUE;
				}
				else
				{
					mCastEntry->unKnownMCast=FALSE;
				}

				oldDescPortMask=rtl865x_getMCastEntryDescPortMask( mCastEntry);	

		#if defined (CONFIG_RTL_HW_MCAST_WIFI)
				if(hwwifiEnable)
				{
					oldAsicFwdPortMask=mCastEntry->mbr;
					oldCpuFlag = mCastEntry->cpu;
				}
		#endif
		
		#if defined CONFIG_RT_MULTIPLE_BR_SUPPORT
				newDescPortMask = 0;
				newCpuFlag = 0;
		#if defined CONFIG_RTL_HW_MCAST_WIFI
				currentCpuFlag = 0;
		#endif
				for(br_index = 0; br_index<RTL_IMGP_MAX_BRMODULE; br_index++)
				{	
					br_moduleIndex=rtl_get_brIgmpModuleIndexbyId(br_index,br_name);
					if(br_moduleIndex==0xFFFFFFFF)
						continue;

					/*sync with control plane*/
					memset(&newFwdDesc, 0 ,sizeof(rtl865x_mcast_fwd_descriptor_t));
					strcpy(newFwdDesc.netifName,br_name);
				
					multicastDataInfo.ipVersion=4;
					multicastDataInfo.sourceIp[0]=	mCastEntry->sip;
					multicastDataInfo.groupAddr[0]= mCastEntry->dip;
				
				
					retVal= rtl_getMulticastDataFwdInfo(br_moduleIndex, &multicastDataInfo, &multicastFwdInfo);
					
				#if defined (CONFIG_RTL_HW_MCAST_WIFI)
					if(hwwifiEnable)
					{
						if(retVal != SUCCESS)
						{
							if(multicastFwdInfo.unknownMCast == 1)
								multicastFwdInfo.cpuFlag = 0;
							else
								multicastFwdInfo.cpuFlag = 1;
						}

						currentCpuFlag = multicastFwdInfo.cpuFlag;

						if(currentCpuFlag)
							newAsicFwdPortMask = (1<<RTL8651_MAC_NUMBER); //cpu port
						else
							newAsicFwdPortMask = 0;

						newFwdDesc.fwdPortMask = newAsicFwdPortMask;
						newFwdDesc.toCpu = currentCpuFlag;
					#if defined CONFIG_RTL865X_MUTLICAST_DEBUG
						printk("newFwdDesc.fwdPortMask = %x, newFwdDesc.toCpu = %d, [%s:%d]\n", newFwdDesc.fwdPortMask, newFwdDesc.toCpu, __FUNCTION__, __LINE__);
					#endif
						_rtl865x_mergeMCastFwdDescChain(&mCastEntry->fwdDescChain,&newFwdDesc);
					}
				#endif

					if(retVal == SUCCESS)
					{
		#if defined(CONFIG_RTL_VLAN_8021Q)&&defined(CONFIG_RTL_MULTICAST_PORT_MAPPING)
						if(multicastFwdInfo.cpuFlag)
						{
							br_tmp = rtl_get_brByIndex(br_index, br_name);
							wlanPhyPortMask = rtl865x_getWlanPhyFwdPortMaskbyVlan(br_tmp, multicastFwdInfo.fwdPortMask);
							if((wlanPhyPortMask&(mCastEntry->mapPortMbr))==0) //forward to wlan in other vlan
								multicastFwdInfo.cpuFlag=0;
						}			
		#endif
						newCpuFlag |= multicastFwdInfo.cpuFlag;
					}
					
				}
				//printk("br:%s :oldAsicPortMask:%x,newAsicPortMask:%x,oldCpuFlag:%d,newCpuFlag:%d,[%s]:[%d].\n",devName,oldAsicPortMask,newAsicPortMask,oldCpuFlag,newCpuFlag,__FUNCTION__,__LINE__);
			
				#if defined (CONFIG_RTL_HW_MCAST_WIFI)
				if(hwwifiEnable)
				{
				
					mCastEntry->mbr 		= rtl865x_genMCastEntryAsicFwdMask(mCastEntry);
					mCastEntry->cpu 	= rtl865x_genMCastEntryCpuFlag(mCastEntry);
					newAsicFwdPortMask =mCastEntry->mbr;
					newCpuFlag=mCastEntry->cpu;
					if(mCastEntry->unKnownMCast==TRUE)
					{
						newAsicFwdPortMask = 0;
						newCpuFlag = 0;
					}
						
					if(((newAsicFwdPortMask) & (~(1<<6)))==0)
					{
						//only cpu port
						newAsicFwdPortMask = 0;
					}
							
					if((oldCpuFlag != newCpuFlag)||(newAsicFwdPortMask!=oldAsicFwdPortMask))
					{
						if(newAsicFwdPortMask == 0){
							//printk("[%s]:[%d].\n",__FUNCTION__,__LINE__);
							_rtl865x_freeMCastEntry(mCastEntry, index);
						}	
						
						_rtl865x_arrangeMulticast(index);
					}
				}
				else
				{
					if(mCastEntry->unKnownMCast==TRUE)
						newCpuFlag = 1;	

					if(oldCpuFlag != newCpuFlag)
					{
						_rtl865x_freeMCastEntry(mCastEntry, index);
						_rtl865x_arrangeMulticast(index);
					}
				}
				#else
				if(mCastEntry->unKnownMCast==TRUE)
					newCpuFlag = 1;	
				if(oldCpuFlag != newCpuFlag)
				{
					_rtl865x_freeMCastEntry(mCastEntry, index);
					_rtl865x_arrangeMulticast(index);
				}

				#endif
		#else  //CONFIG_RT_MULTIPLE_BR_SUPPORT
				
				/*sync with control plane*/
				memset(&newFwdDesc, 0 ,sizeof(rtl865x_mcast_fwd_descriptor_t));
				strcpy(newFwdDesc.netifName,mcastEventContext.devName);
				multicastDataInfo.ipVersion=4;
				multicastDataInfo.sourceIp[0]=  mCastEntry->sip;
				multicastDataInfo.groupAddr[0]= mCastEntry->dip;
				retVal= rtl_getMulticastDataFwdInfo(mcastEventContext.moduleIndex, &multicastDataInfo, &multicastFwdInfo);
				
			#if defined (CONFIG_RTL_HW_MCAST_WIFI)
				if(hwwifiEnable)
				{
					if(retVal==SUCCESS)
					{
						newCpuFlag = multicastFwdInfo.cpuFlag;
						
						if(newCpuFlag)
							newAsicFwdPortMask = (1<<RTL8651_MAC_NUMBER);
						else
							newAsicFwdPortMask = 0;
						
					}	
					else
					{
						newAsicFwdPortMask=0;
						newCpuFlag =1;
					}
					//this fwdDesc is used to set cpu port, here to update br fwdDesc
					newFwdDesc.fwdPortMask = newAsicFwdPortMask;
					newFwdDesc.toCpu = newCpuFlag;
					
					//printk("br:%s :oldAsicPortMask:%x,newAsicPortMask:%x,oldCpuFlag:%d,newCpuFlag:%d,[%s]:[%d].\n",mcastEventContext.devName,oldAsicPortMask,newAsicPortMask,oldCpuFlag,newCpuFlag,__FUNCTION__,__LINE__);
					_rtl865x_mergeMCastFwdDescChain(&mCastEntry->fwdDescChain,&newFwdDesc);
					mCastEntry->mbr 		= rtl865x_genMCastEntryAsicFwdMask(mCastEntry);
					mCastEntry->cpu		= rtl865x_genMCastEntryCpuFlag(mCastEntry);
					
					newAsicFwdPortMask =mCastEntry->mbr;
					newCpuFlag =mCastEntry->cpu;
					//printk("br:%s :oldAsicPortMask:%x,newAsicPortMask:%x,oldCpuFlag:%d,newCpuFlag:%d,[%s]:[%d].\n",mcastEventContext.devName,oldAsicPortMask,newAsicPortMask,oldCpuFlag,newCpuFlag,__FUNCTION__,__LINE__);
					//just cpu port not add hw entry
					if((newAsicFwdPortMask&(~(1<<6)))==0){
						newAsicFwdPortMask =0;
					}
					if((oldCpuFlag != newCpuFlag)||(newAsicFwdPortMask!=oldAsicFwdPortMask))
					{
						if(newAsicFwdPortMask == 0)
							_rtl865x_freeMCastEntry(mCastEntry, index);
						_rtl865x_arrangeMulticast(index);
					}
				}
				else
				{
					if(retVal!=SUCCESS)
					{
						continue;
					}
					
					retVal= rtl_getIgmpSnoopingModuleDevInfo(mcastEventContext.moduleIndex, &bridgeMCastDev);
					if(retVal!=SUCCESS)
					{
						continue;
					}
					newDescPortMask=multicastFwdInfo.fwdPortMask;
					//shirley note:oldDescPortMask is always 0, the judge condision is equal to (newDescPortMask & bridgeMCastDev.swPortMask)!=0
					if(	(oldDescPortMask != newDescPortMask) &&
						(	((newDescPortMask & bridgeMCastDev.swPortMask)!=0) ||
							(((oldDescPortMask & bridgeMCastDev.swPortMask) !=0) && ((newDescPortMask & bridgeMCastDev.swPortMask)==0)))	)
					{
						/*this multicast entry should be re-generated at linux protocol stack bridge level*/
						_rtl865x_freeMCastEntry(mCastEntry, index);
						_rtl865x_arrangeMulticast(index);
					}
				}
			#else

				if(retVal!=SUCCESS)
				{
					continue;
				}
				
				retVal= rtl_getIgmpSnoopingModuleDevInfo(mcastEventContext.moduleIndex, &bridgeMCastDev);
				if(retVal!=SUCCESS)
				{
					continue;
				}
				newDescPortMask=multicastFwdInfo.fwdPortMask;
				//shirley note:oldDescPortMask is always 0, the judge condision is equal to (newDescPortMask & bridgeMCastDev.swPortMask)!=0
				if(	(oldDescPortMask != newDescPortMask) &&
					(	((newDescPortMask & bridgeMCastDev.swPortMask)!=0) ||
						(((oldDescPortMask & bridgeMCastDev.swPortMask) !=0) && ((newDescPortMask & bridgeMCastDev.swPortMask)==0)))	)
				{
					/*this multicast entry should be re-generated at linux protocol stack bridge level*/
					_rtl865x_freeMCastEntry(mCastEntry, index);
					_rtl865x_arrangeMulticast(index);
				}
			#endif	
		#endif
			}
		}
		
		return EVENT_CONTINUE_EXECUTE;
	}		

/*update ethernet forwarding port mask*/

#ifdef CONFIG_RTK_VLAN_WAN_TAG_SUPPORT
    if(memcmp(mcastEventContext.devName,"eth*",4)==0 || memcmp(mcastEventContext.devName,RTL_PS_ETH_NAME_ETH2,4)==0)
#else
	if(memcmp(mcastEventContext.devName,"eth*",4)==0)
#endif
	{
		#ifdef CONFIG_RTL865X_MUTLICAST_DEBUG
		printk("%s:%d,multicast event from %s,mcastEventContext.groupAddr[0] is 0x%x\n",__FUNCTION__,__LINE__,mcastEventContext.devName,mcastEventContext.groupAddr[0]);
		#endif
		
		for (index=0; index< RTL8651_MULTICASTTBL_SIZE; index++)
		{
			for (mCastEntry = TAILQ_FIRST(&mCastTbl.inuseList.mCastTbl[index]); mCastEntry; mCastEntry = nextMCastEntry)
			{
				nextMCastEntry=TAILQ_NEXT(mCastEntry, nextMCast);
				
				if ((mcastEventContext.groupAddr!=0) && (mCastEntry->dip != mcastEventContext.groupAddr[0]))
				{
					continue;
				}
				
				memset(&newFwdDesc, 0 ,sizeof(rtl865x_mcast_fwd_descriptor_t));
				strcpy(newFwdDesc.netifName,mcastEventContext.devName);

				/*save old multicast entry forward port mask*/
				oldAsicFwdPortMask=mCastEntry->mbr;
				oldCpuFlag=mCastEntry->cpu;

				/*sync with control plane*/
				multicastDataInfo.ipVersion=4;
				multicastDataInfo.sourceIp[0]=  mCastEntry->sip;
				multicastDataInfo.groupAddr[0]= mCastEntry->dip;

				retVal= rtl_getMulticastDataFwdInfo(mcastEventContext.moduleIndex, &multicastDataInfo, &multicastFwdInfo);
				
#if defined (CONFIG_RTL_HW_MCAST_WIFI)
				if(hwwifiEnable)
				{
					if(retVal != SUCCESS)
					{
						newFwdDesc.fwdPortMask=0;
						newFwdDesc.toCpu =1;
					}
					else
					{
						newFwdDesc.fwdPortMask=multicastFwdInfo.fwdPortMask & (~(1<<mCastEntry->port));
						newFwdDesc.toCpu=multicastFwdInfo.cpuFlag;
					}
				}
				else
#endif
				{
					/*fix jwj*/
					if (multicastFwdInfo.unknownMCast == TRUE)
						multicastFwdInfo.cpuFlag = TRUE;
					newFwdDesc.fwdPortMask=multicastFwdInfo.fwdPortMask & (~(1<<mCastEntry->port));
					newFwdDesc.toCpu=multicastFwdInfo.cpuFlag;
				}
				#if defined CONFIG_RTL_MULTICAST_PORT_MAPPING
					newFwdDesc.fwdPortMask &= mCastEntry->mapPortMbr;
				#endif

			
				/*update/replace old forward descriptor*/
				
				_rtl865x_mergeMCastFwdDescChain(&mCastEntry->fwdDescChain,&newFwdDesc);
				mCastEntry->mbr 		= rtl865x_genMCastEntryAsicFwdMask(mCastEntry);
				mCastEntry->cpu		= rtl865x_genMCastEntryCpuFlag(mCastEntry);
				
				newAsicFwdPortMask	= mCastEntry->mbr ;
				newCpuFlag			=mCastEntry->cpu;
				
#if defined (CONFIG_RTL_HW_MCAST_WIFI)
				if(hwwifiEnable)
				{
					if((newAsicFwdPortMask&(~(1<<6)))==0)
						newAsicFwdPortMask =0;
				}
#endif
				#ifdef CONFIG_RTL865X_MUTLICAST_DEBUG
				printk("%s:%d,oldAsicFwdPortMask is %d,newAsicFwdPortMask is %d\n",__FUNCTION__,__LINE__,oldAsicFwdPortMask,newAsicFwdPortMask);
				#endif
				
#ifndef RTL8651_MCAST_ALWAYS2UPSTREAM
				if (mCastEntry->flag & RTL865X_MULTICAST_UPLOADONLY)
				{	/* remove upload term*/
					if((newAsicFwdPortMask!=0) && (newAsicFwdPortMask!=oldAsicFwdPortMask))
					{
						mCastEntry->flag &= ~RTL865X_MULTICAST_UPLOADONLY;
						/* we assume multicast member will NEVER in External interface, so we remove
						     external ip now */
						mCastEntry->flag &= ~RTL865X_MULTICAST_EXTIP_SET;
						mCastEntry->extIp= 0;
					}
				}
#endif /* RTL8651_MCAST_ALWAYS2UPSTREAM */

				rtl_getGroupInfo(mCastEntry->dip, &groupInfo);
				if(groupInfo.ownerMask==0)
				{
					mCastEntry->unKnownMCast=TRUE;
				}
				else
				{
					mCastEntry->unKnownMCast=FALSE;
				}
	
				
				if((oldCpuFlag != newCpuFlag)||(newAsicFwdPortMask!=oldAsicFwdPortMask)) 
				{
					_rtl865x_patchPppoeWeak(mCastEntry);
					
					/*reset inAsic flag to re-select or re-write this hardware asic entry*/
					if(newAsicFwdPortMask==0)
					{
						_rtl865x_freeMCastEntry(mCastEntry, index);
					}

					_rtl865x_arrangeMulticast(index);
				}
			}

				
			
		}
	}
#endif
#endif
	return EVENT_CONTINUE_EXECUTE;
}

static int32 _rtl865x_multicastUnRegisterEvent(void)
{
	rtl865x_event_Param_t eventParam;

	eventParam.eventLayerId=DEFAULT_LAYER3_EVENT_LIST_ID;
	eventParam.eventId=EVENT_UPDATE_MCAST;
	eventParam.eventPriority=0;
	eventParam.event_action_fn=rtl865x_multicastCallbackFn;
	rtl865x_unRegisterEvent(&eventParam);

	return SUCCESS;
}

static int32 _rtl865x_multicastRegisterEvent(void)
{
	rtl865x_event_Param_t eventParam;

	eventParam.eventLayerId=DEFAULT_LAYER3_EVENT_LIST_ID;
	eventParam.eventId=EVENT_UPDATE_MCAST;
	eventParam.eventPriority=0;
	eventParam.event_action_fn=rtl865x_multicastCallbackFn;
	rtl865x_registerEvent(&eventParam);

	return SUCCESS;
}

#if defined CONFIG_RTL_L2_HW_MULTICAST_SUPPORT
extern void ConvertMulticatIPtoMacAddr(__u32 group, unsigned char *gmac);
extern int rtl_delFilterDatabaseEntryforMCast(unsigned char *mac);
extern int rtl_addFilterDatabaseEntryforMCast(unsigned char *mac, unsigned int fwdPortMask);
extern int nicIgmpModuleIndex;

static int32 rtl865x_L2multicastCallbackFn(void *param)
{
	rtl_multicastEventContext_t mcastEventContext;
	struct rtl_multicastDataInfo multicastDataInfo;
	struct rtl_multicastFwdInfo multicastFwdInfo;
	int32 retVal=FAILED, ret;
	unsigned char mac[6]={0};

	if(param==NULL)
	{
		return EVENT_CONTINUE_EXECUTE;
	}
	memcpy(&mcastEventContext,param,sizeof(rtl_multicastEventContext_t));

	if(strlen(mcastEventContext.devName)==0)
	{
		return EVENT_CONTINUE_EXECUTE;
	}

	if(mcastEventContext.moduleIndex != nicIgmpModuleIndex)
	{
		//only deal with eth event.
		return EVENT_CONTINUE_EXECUTE;
	}

	if(mcastEventContext.ipVersion != 4)
	{
		//only deal with eth event.
		return EVENT_CONTINUE_EXECUTE;
	}

	multicastDataInfo.ipVersion=4;
	//l2 multicast only support igmpv1 and igmpv2, sourceip is not meaningful
	multicastDataInfo.sourceIp[0]=	mcastEventContext.sourceAddr[0];
	multicastDataInfo.groupAddr[0]= mcastEventContext.groupAddr[0];
	ConvertMulticatIPtoMacAddr(mcastEventContext.groupAddr[0], mac);
	retVal= rtl_getMulticastDataFwdInfo(mcastEventContext.moduleIndex, &multicastDataInfo, &multicastFwdInfo);

	if(retVal!=SUCCESS)
	{
		//need to delete l2 table entry
		ret=rtl_delFilterDatabaseEntryforMCast(mac);
		//printk("delFilter return:%d\n", ret);
	}
	else
	{
		//update l2 table entry
		ret=rtl_addFilterDatabaseEntryforMCast(mac, multicastFwdInfo.fwdPortMask);
		//printk("addFilter return:%d\n", ret);
	}
	return 0;
}
static int32 _rtl865x_L2multicastUnRegisterEvent(void)
{
	rtl865x_event_Param_t eventParam;

	eventParam.eventLayerId=DEFAULT_LAYER2_EVENT_LIST_ID;
	eventParam.eventId=EVENT_UPDATE_L2_MCAST;
	eventParam.eventPriority=0;
	eventParam.event_action_fn=rtl865x_L2multicastCallbackFn;
	rtl865x_unRegisterEvent(&eventParam);

	return SUCCESS;
}

static int32 _rtl865x_L2multicastRegisterEvent(void)
{
	rtl865x_event_Param_t eventParam;

	eventParam.eventLayerId=DEFAULT_LAYER2_EVENT_LIST_ID;
	eventParam.eventId=EVENT_UPDATE_L2_MCAST;
	eventParam.eventPriority=0;
	eventParam.event_action_fn=rtl865x_L2multicastCallbackFn;
	rtl865x_registerEvent(&eventParam);

	return SUCCESS;
}

#endif
void _rtl865x_timeUpdateMulticast(uint32 secPassed)
{

	rtl865x_tblDrv_mCast_t *mCast_t, *nextMCast_t;
	uint32 entry;
	uint32 needReArrange=FALSE;
	uint32 hashLineCnt=0;
	/* check to Aging and HW swapping */
	for (entry=0; entry< RTL8651_MULTICASTTBL_SIZE; entry++) {
		hashLineCnt=0;
		needReArrange=FALSE;
		mCast_t = TAILQ_FIRST(&mCastTbl.inuseList.mCastTbl[entry]);
		while (mCast_t) {
			/*save the next entry first*/
			nextMCast_t=TAILQ_NEXT(mCast_t, nextMCast);
			
			if (mCast_t->inAsic == TRUE)
			{
				/* Entry is in the ASIC */
				if (mCast_t->age <= secPassed) 
				{
					if(mCast_t->mbr==0)
					{
						_rtl865x_freeMCastEntry(mCast_t, entry);
						needReArrange=TRUE;
					}
					else
					{
						mCast_t->age = RTL865X_MULTICAST_TABLE_AGE;
					}
				}
				else
				{
					mCast_t->age -= secPassed;
				}
			}
			else 
			{
				/* Entry is not in the ASIC */
				if (mCast_t->age <= secPassed)
				{ /* aging out */
					_rtl865x_freeMCastEntry(mCast_t, entry);
				}
				else 
				{
					mCast_t->age -= secPassed;
				}
			}
			
			/*won't count multicast entry forwarded by cpu*/
			if(mCast_t->cpu==0)
			{
				
				hashLineCnt++;
				//printk("------------hashLineCnt:%d,[%s]:[%d].\n",hashLineCnt,__FUNCTION__,__LINE__);
				if(hashLineCnt>=2)
				{
					needReArrange=TRUE;
				}
			}
		
			//mCast_t->count = 0;
			mCast_t = nextMCast_t;
		}

		if(needReArrange==TRUE)
		{
			//printk("------------entry:%d,hashLineCnt:%d,[%s]:[%d].\n",entry,hashLineCnt,__FUNCTION__,__LINE__);
			_rtl865x_arrangeMulticast(entry);
		}
		mCast_t = TAILQ_FIRST(&mCastTbl.inuseList.mCastTbl[entry]);
		
		while (mCast_t) {
			/*save the next entry first*/
			nextMCast_t=TAILQ_NEXT(mCast_t, nextMCast);
			
			if (mCast_t->inAsic == FALSE)
				mCast_t->count=0;
			
			mCast_t = nextMCast_t;
		}
		
	}
}

#if defined(__linux__) && defined(__KERNEL__)
static void _rtl865x_mCastSysTimerExpired(uint32 expireDada)
{

	_rtl865x_timeUpdateMulticast(1);
	mod_timer(&rtl865x_mCastSysTimer, jiffies+HZ);
	
}

static void _rtl865x_initMCastSysTimer(void)
{

	init_timer(&rtl865x_mCastSysTimer);
	rtl865x_mCastSysTimer.data=rtl865x_mCastSysTimer.expires;
	rtl865x_mCastSysTimer.expires=jiffies+HZ;
	rtl865x_mCastSysTimer.function=(void*)_rtl865x_mCastSysTimerExpired;
	add_timer(&rtl865x_mCastSysTimer);
}

static void _rtl865x_destroyMCastSysTimer(void)
{
	del_timer(&rtl865x_mCastSysTimer);
}

#endif

/*
@func int32	| rtl865x_initMulticast	|  Init hardware ip multicast module.
@parm  rtl865x_mCastConfig_t *	| mCastConfigPtr	| Pointer of hardware multicast configuration. 
@rvalue SUCCESS	|Initialize successfully.
@rvalue FAILED	|Initialize failed.
*/
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
static void _rtl8198C_initMCastv6SysTimer(void);
#endif
int32 rtl865x_initMulticast(rtl865x_mCastConfig_t * mCastConfigPtr)
{
	_rtl865x_multicastUnRegisterEvent();
#if defined CONFIG_RTL_L2_HW_MULTICAST_SUPPORT
	_rtl865x_L2multicastUnRegisterEvent();
#endif
	_rtl865x_initMCastEntryPool();
	_rtl865x_initMCastFwdDescPool();
#if defined(CONFIG_RTL_HARDWARE_MULTICAST_API)
	_rtl865x_kernelMCastUnRegisterEvent();
	rtl_initIgmpKernelMCast();
#endif

	rtl865x_setMulticastExternalPortMask(0);
	if(mCastConfigPtr!=NULL)
	{
		rtl865x_setMulticastExternalPortMask(mCastConfigPtr->externalPortMask);
	}
	#if defined(__linux__) && defined(__KERNEL__)
	_rtl865x_initMCastSysTimer();
	#endif
#if defined CONFIG_RTL_L2_HW_MULTICAST_SUPPORT
	rtl8651_setAsicOperationLayer(2);
#else
	rtl8651_setAsicOperationLayer(3);
#endif
	rtl8651_setAsicMulticastMTU(1522); 
#if defined CONFIG_RTL_L2_HW_MULTICAST_SUPPORT
	rtl8651_setAsicMulticastEnable(FALSE);
#else
	rtl8651_setAsicMulticastEnable(TRUE);
#endif
	rtl865x_setAsicMulticastAging(TRUE);
	_rtl865x_multicastRegisterEvent();
#if defined CONFIG_RTL_L2_HW_MULTICAST_SUPPORT
	_rtl865x_L2multicastRegisterEvent();
#endif
#if defined(CONFIG_RTL_HARDWARE_MULTICAST_API)
	_rtl865x_kernelMCastRegisterEvent();
#endif
#if (defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F))
	rtl8198C_initMulticastv6();
#endif
	return SUCCESS;
}

/*
@func int32	| rtl865x_reinitMulticast	|  Re-init hardware ip multicast module.
@rvalue SUCCESS	|Re-initialize successfully.
@rvalue FAILED	|Re-initialize failed.
*/
int32 rtl865x_reinitMulticast(void)
{
	_rtl865x_multicastUnRegisterEvent();
#if defined CONFIG_RTL_L2_HW_MULTICAST_SUPPORT
	_rtl865x_L2multicastUnRegisterEvent();
#endif
#if defined(CONFIG_RTL_HARDWARE_MULTICAST_API)
	_rtl865x_kernelMCastUnRegisterEvent();
#endif
	/*delete all multicast entry*/
	rtl8651_setAsicMulticastEnable(FALSE);
	rtl865x_delMulticastEntry(0);
	
	#if defined(__linux__) && defined(__KERNEL__)
	_rtl865x_destroyMCastSysTimer();
	_rtl865x_initMCastSysTimer();
	#endif
	
	/*regfster twice won't cause any side-effect, 
	because event management module will handle duplicate event issue*/
	rtl8651_setAsicMulticastMTU(1522); 
	
#if !defined CONFIG_RTL_L2_HW_MULTICAST_SUPPORT
	rtl8651_setAsicMulticastEnable(TRUE);
#endif
	rtl865x_setAsicMulticastAging(TRUE);
	_rtl865x_multicastRegisterEvent();
#if defined CONFIG_RTL_L2_HW_MULTICAST_SUPPORT
	_rtl865x_L2multicastRegisterEvent();
#endif
#if defined(CONFIG_RTL_HARDWARE_MULTICAST_API)
	_rtl865x_kernelMCastRegisterEvent();
#endif
#if 0//defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
	rtl8198C_reinitMulticastv6();
#endif
	return SUCCESS;
}	



#ifdef CONFIG_PROC_FS
extern int32 rtl8651_getAsicMulticastSpanningTreePortState(uint32 port, uint32 *portState);
#ifdef CONFIG_RTL_PROC_NEW
int32 rtl_dumpSwMulticastInfo(struct seq_file *s)
{
	uint32 mCastMtu=0;
	uint32 mCastEnable=FALSE;
	uint32 index;
	int8 isInternal;
	uint32 portStatus;
	uint32 internalPortMask=0;
	uint32 externalPortMask=0;
	int32 ret=FAILED;
	
	rtl865x_tblDrv_mCast_t *mCast_t, *nextMCast_t;
	rtl865x_mcast_fwd_descriptor_t *curDesc,*nextDesc;
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
	uint32 mCastAging=FALSE;
    rtl8198c_tblDrv_mCastv6_t *mCastv6_t, *nextMCastv6_t;
    rtl8198c_mcast_fwd_descriptor6_t *curDescv6, *nextDescv6;
#endif
	uint32 entry;
	uint32 cnt;
	seq_printf(s,"----------------------------------------------------\n");
	seq_printf(s,"Asic Operation Layer :%d\n", rtl8651_getAsicOperationLayer());
	
	ret=rtl8651_getAsicMulticastEnable(&mCastEnable);
	if(ret==SUCCESS)
	{
		seq_printf(s,"Asic Multicast Table:%s\n", (mCastEnable==TRUE)?"Enable":"Disable");
	}
	else
	{
		seq_printf(s,"Read Asic Multicast Table Enable Bit Error\n");
	}
	ret=rtl8651_getAsicMulticastMTU(&mCastMtu); 
	if(ret==SUCCESS)
	{
		seq_printf(s,"Asic Multicast MTU:%d\n", mCastMtu);
	}
	else
	{
		seq_printf(s,"Read Asic Multicast MTU Error\n");
	}
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
	ret=rtl8198C_getAsicMulticastv6Enable(&mCastEnable);
	if(ret==SUCCESS)
	{
		seq_printf(s,"Asic IPV6 Multicast Table:%s\n", (mCastEnable==TRUE)?"Enable":"Disable");
	}
	else
	{
		seq_printf(s,"Read Asic IPV6 Multicast Table Enable Bit Error\n");
	}
	ret=rtl8198C_getAsicMulticastv6MTU(&mCastMtu); 
	if(ret==SUCCESS)
	{
		seq_printf(s,"Asic IPV6 Multicast MTU:%d\n", mCastMtu);
	}
	else
	{
		seq_printf(s,"Read Asic IPV6 Multicast MTU Error\n");
	}
	ret=rtl8198C_getAsicMulticastv6Aging(&mCastAging);
	if(ret==SUCCESS)
	{
		seq_printf(s,"Asic IPV6 Multicast Table Aging:%s\n", (mCastAging==TRUE)?"Enable":"Disable");
	}
	else
	{
		seq_printf(s,"Read Asic IPV6 Multicast Table Aging Enable Bit Error\n");
	}
#endif
	for (index=0; index<RTL8651_PORT_NUMBER+rtl8651_totalExtPortNum; index++)
	{
		ret=rtl8651_getAsicMulticastPortInternal(index, &isInternal);
		if(ret==SUCCESS)
		{
			if(isInternal==TRUE)
			{
				internalPortMask |= 1<<index;
			}
			else
			{
				externalPortMask |= 1<<index;
			}
		}
	
	}

	seq_printf(s,"Internal Port Mask:0x%x\nExternal Port Mask:0x%x\n", internalPortMask,externalPortMask);
	seq_printf(s,"----------------------------------------------------\n");
	seq_printf(s,"Multicast STP State:\n");
	for (index=0; index<RTL8651_PORT_NUMBER+rtl8651_totalExtPortNum; index++)
	{
		ret= rtl8651_getAsicMulticastSpanningTreePortState(index, &portStatus);
		if(ret==SUCCESS)
		{
			seq_printf(s,"port[%d]:",index);
			if(portStatus==RTL8651_PORTSTA_DISABLED)
			{
				seq_printf(s,"disabled\n");
			}
			else if(portStatus==RTL8651_PORTSTA_BLOCKING)
			{
				seq_printf(s,"blocking\n");
			}
			else if(portStatus==RTL8651_PORTSTA_LEARNING)
			{
				seq_printf(s,"learning\n");
			}
			else if(portStatus==RTL8651_PORTSTA_FORWARDING)
			{
				seq_printf(s,"forwarding\n");
			}
		}
		
	}
	seq_printf(s,"----------------------------------------------------\n");
	seq_printf(s,"Software Multicast Table:\n");
	/* check to Aging and HW swapping */
	for (entry=0; entry< RTL8651_MULTICASTTBL_SIZE; entry++) {
		#if defined(CONFIG_RTL_WTDOG)
		rtl_periodic_watchdog_kick(entry, WATCHDOG_NUM_OF_TIMES);
		#endif
		mCast_t = TAILQ_FIRST(&mCastTbl.inuseList.mCastTbl[entry]);
		while (mCast_t) {
			/*save the next entry first*/
			nextMCast_t=TAILQ_NEXT(mCast_t, nextMCast);
			seq_printf(s,"\t[%2d]  dip:%d.%d.%d.%d, sip:%d.%d.%d.%d, mbr:0x%x, svid:%d, spa:%d, \n", entry,
				mCast_t->dip>>24, (mCast_t->dip&0x00ff0000)>>16, (mCast_t->dip&0x0000ff00)>>8, (mCast_t->dip&0xff), 
				mCast_t->sip>>24, (mCast_t->sip&0x00ff0000)>>16, (mCast_t->sip&0x0000ff00)>>8, (mCast_t->sip&0xff),
				mCast_t->mbr,mCast_t->svid, mCast_t->port);
		#if defined CONFIG_RTL_MULTICAST_PORT_MAPPING
			seq_printf(s,"\t      extIP:0x%x,age:%d, cpu:%d, count:%d, inAsic:%d, (%s), portMappingMask:0x%x\n", 
				mCast_t->extIp,mCast_t->age, mCast_t->cpu,mCast_t->count,mCast_t->inAsic,mCast_t->unKnownMCast?"UnknownMCast":"KnownMCast", mCast_t->mapPortMbr);
		#else
			seq_printf(s,"\t      extIP:0x%x,age:%d, cpu:%d, count:%d, inAsic:%d, (%s)\n", 
				mCast_t->extIp,mCast_t->age, mCast_t->cpu,mCast_t->count,mCast_t->inAsic,mCast_t->unKnownMCast?"UnknownMCast":"KnownMCast");
		#endif
			cnt=0;
			curDesc=MC_LIST_FIRST(&mCast_t->fwdDescChain);
			while(curDesc)
			{
				nextDesc=MC_LIST_NEXT(curDesc, next );
				seq_printf(s,"\t      netif(%s),descPortMask(0x%x),toCpu(%d),fwdPortMask(0x%x)\n",curDesc->netifName,curDesc->descPortMask,curDesc->toCpu,curDesc->fwdPortMask);
				curDesc = nextDesc;
			}
			
			seq_printf(s,"\n");
			mCast_t = nextMCast_t;
		}
		
	}
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
	seq_printf(s,"----------------------------------------------------\n");
	seq_printf(s,"IPV6 Software Multicast Table:\n");
	for (entry=0; entry< RTL8651_MULTICASTTBL_SIZE; entry++) {
		#if defined(CONFIG_RTL_WTDOG)
		rtl_periodic_watchdog_kick(entry, WATCHDOG_NUM_OF_TIMES);
		#endif
		mCastv6_t = TAILQ_FIRST(&mCastTbl6.inuseList.mCastTbl[entry]);
		while (mCastv6_t) {
			/*save the next entry first*/
			nextMCastv6_t=TAILQ_NEXT(mCastv6_t, nextMCast); 		
			seq_printf(s,"\t[%2d]  dip:%08x:%08x%08x:%08x\n\t      sip:%08x:%08x:%08x:%08x\n\t      mbr:0x%x, svid:%d, spa:%d, ",
				entry,
				mCastv6_t->dip.v6_addr32[0],mCastv6_t->dip.v6_addr32[1],
				mCastv6_t->dip.v6_addr32[2],mCastv6_t->dip.v6_addr32[3],
				mCastv6_t->sip.v6_addr32[0],mCastv6_t->sip.v6_addr32[1],
				mCastv6_t->sip.v6_addr32[2],mCastv6_t->sip.v6_addr32[3],

				mCastv6_t->mbr,
				mCastv6_t->svid,
				mCastv6_t->port);
			
#if defined CONFIG_RTL_MULTICAST_PORT_MAPPING
			seq_printf(s,"age:%d, cpu:%d, count:%d, inAsic:%d, (%s), portMappingMask:0x%x\n", 
				mCastv6_t->age, mCastv6_t->cpu,mCastv6_t->count,mCastv6_t->inAsic,mCastv6_t->unKnownMCast?"UnknownMCast":"KnownMCast", mCastv6_t->mapPortMbr);
#else
			seq_printf(s,"age:%d, cpu:%d, count:%d, inAsic:%d, (%s)\n", 
				mCastv6_t->age, mCastv6_t->cpu,mCastv6_t->count,mCastv6_t->inAsic,mCastv6_t->unKnownMCast?"UnknownMCast":"KnownMCast");
#endif
			cnt=0;
			curDescv6=MC_LIST_FIRST(&mCastv6_t->fwdDescChain);
			while(curDescv6)
			{
				nextDescv6=MC_LIST_NEXT(curDescv6, next );
				seq_printf(s,"\t      netif(%s),descPortMask(0x%x),toCpu(%d),fwdPortMask(0x%x)\n",curDescv6->netifName,curDescv6->descPortMask,curDescv6->toCpu,curDescv6->fwdPortMask);
				curDescv6 = nextDescv6;
			}
				
			seq_printf(s,"\n");
			mCastv6_t = nextMCastv6_t;
		}
			
	}
#endif

	return SUCCESS;
}
#else
int32 rtl_dumpSwMulticastInfo(char *page, int *ret_len)
{
    int len = *ret_len;
	uint32 mCastMtu=0;
	uint32 mCastEnable=FALSE;
	uint32 index;
	int8 isInternal;
	uint32 portStatus;
	uint32 internalPortMask=0;
	uint32 externalPortMask=0;
	int32 ret=FAILED;
	
	rtl865x_tblDrv_mCast_t *mCast_t, *nextMCast_t;
	rtl865x_mcast_fwd_descriptor_t *curDesc,*nextDesc;
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
    rtl8198c_tblDrv_mCastv6_t *mCastv6_t, *nextMCastv6_t;
    rtl8198c_mcast_fwd_descriptor6_t *curDescv6, *nextDescv6;
#endif
	uint32 entry;
	uint32 cnt;
	len += sprintf(page+len, "----------------------------------------------------\n");
	len += sprintf(page+len,"Asic Operation Layer :%d\n", rtl8651_getAsicOperationLayer());
	
	ret=rtl8651_getAsicMulticastEnable(&mCastEnable);
	if(ret==SUCCESS)
	{
		len += sprintf(page+len,"Asic Multicast Table:%s\n", (mCastEnable==TRUE)?"Enable":"Disable");
	}
	else
	{
		len += sprintf(page+len,"Read Asic Multicast Table Enable Bit Error\n");
	}
	ret=rtl8651_getAsicMulticastMTU(&mCastMtu); 
	if(ret==SUCCESS)
	{
		len += sprintf(page+len,"Asic Multicast MTU:%d\n", mCastMtu);
	}
	else
	{
		len += sprintf(page+len,"Read Asic Multicast MTU Error\n");
	}
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
	ret=rtl8198C_getAsicMulticastv6Enable(&mCastEnable);
	if(ret==SUCCESS)
	{
		len += sprintf(page+len,"Asic IPV6 Multicast Table:%s\n", (mCastEnable==TRUE)?"Enable":"Disable");
	}
	else
	{
		len += sprintf(page+len,"Read Asic IPV6 Multicast Table Enable Bit Error\n");
	}
	ret=rtl8198C_getAsicMulticastv6MTU(&mCastMtu); 
	if(ret==SUCCESS)
	{
		len += sprintf(page+len,"Asic IPV6 Multicast MTU:%d\n", mCastMtu);
	}
	else
	{
		len += sprintf(page+len,"Read Asic IPV6 Multicast MTU Error\n");
	}
#endif
	for (index=0; index<RTL8651_PORT_NUMBER+rtl8651_totalExtPortNum; index++)
	{
		ret=rtl8651_getAsicMulticastPortInternal(index, &isInternal);
		if(ret==SUCCESS)
		{
			if(isInternal==TRUE)
			{
				internalPortMask |= 1<<index;
			}
			else
			{
				externalPortMask |= 1<<index;
			}
		}
	
	}

	len += sprintf(page+len,"Internal Port Mask:0x%x\nExternal Port Mask:0x%x\n", internalPortMask,externalPortMask);
	len += sprintf(page+len,"----------------------------------------------------\n");
	len += sprintf(page+len,"Multicast STP State:\n");
	for (index=0; index<RTL8651_PORT_NUMBER+rtl8651_totalExtPortNum; index++)
	{
		ret= rtl8651_getAsicMulticastSpanningTreePortState(index, &portStatus);
		if(ret==SUCCESS)
		{
			len += sprintf(page+len,"port[%d]:",index);
			if(portStatus==RTL8651_PORTSTA_DISABLED)
			{
				len += sprintf(page+len,"disabled\n");
			}
			else if(portStatus==RTL8651_PORTSTA_BLOCKING)
			{
				len += sprintf(page+len,"blocking\n");
			}
			else if(portStatus==RTL8651_PORTSTA_LEARNING)
			{
				len += sprintf(page+len,"learning\n");
			}
			else if(portStatus==RTL8651_PORTSTA_FORWARDING)
			{
				len += sprintf(page+len,"forwarding\n");
			}
		}
		
	}
	len += sprintf(page+len,"----------------------------------------------------\n");
	len += sprintf(page+len,"Software Multicast Table:\n");
	/* check to Aging and HW swapping */
	for (entry=0; entry< RTL8651_MULTICASTTBL_SIZE; entry++) {
		#if defined(CONFIG_RTL_WTDOG)
		rtl_periodic_watchdog_kick(entry, WATCHDOG_NUM_OF_TIMES);
		#endif
		mCast_t = TAILQ_FIRST(&mCastTbl.inuseList.mCastTbl[entry]);
		while (mCast_t) {
			/*save the next entry first*/
			nextMCast_t=TAILQ_NEXT(mCast_t, nextMCast);
			len += sprintf(page+len,"\t[%2d]  dip:%d.%d.%d.%d, sip:%d.%d.%d.%d, mbr:0x%x, svid:%d, spa:%d, \n", entry,
				mCast_t->dip>>24, (mCast_t->dip&0x00ff0000)>>16, (mCast_t->dip&0x0000ff00)>>8, (mCast_t->dip&0xff), 
				mCast_t->sip>>24, (mCast_t->sip&0x00ff0000)>>16, (mCast_t->sip&0x0000ff00)>>8, (mCast_t->sip&0xff),
				mCast_t->mbr,mCast_t->svid, mCast_t->port);
			
#if defined CONFIG_RTL_MULTICAST_PORT_MAPPING
			len += sprintf(page+len,"\t	  extIP:0x%x,age:%d, cpu:%d, count:%d, inAsic:%d, (%s), portMappingMask:0x%x\n", 
				mCast_t->extIp,mCast_t->age, mCast_t->cpu,mCast_t->count,mCast_t->inAsic,mCast_t->unKnownMCast?"UnknownMCast":"KnownMCast", mCast_t->mapPortMbr);
#else
			len += sprintf(page+len,"\t      extIP:0x%x,age:%d, cpu:%d, count:%d, inAsic:%d, (%s)\n", 
				mCast_t->extIp,mCast_t->age, mCast_t->cpu,mCast_t->count,mCast_t->inAsic,mCast_t->unKnownMCast?"UnknownMCast":"KnownMCast");
#endif			
			cnt=0;
			curDesc=MC_LIST_FIRST(&mCast_t->fwdDescChain);
			while(curDesc)
			{
				nextDesc=MC_LIST_NEXT(curDesc, next );
				len += sprintf(page+len,"\t      netif(%s),descPortMask(0x%x),toCpu(%d),fwdPortMask(0x%x)\n",curDesc->netifName,curDesc->descPortMask,curDesc->toCpu,curDesc->fwdPortMask);
				curDesc = nextDesc;
			}
			
			len += sprintf(page+len,"\n");
			mCast_t = nextMCast_t;
		}
		
	}
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
	len += sprintf(page+len,"----------------------------------------------------\n");
	len += sprintf(page+len,"IPV6 Software Multicast Table:\n");
	for (entry=0; entry< RTL8651_MULTICASTTBL_SIZE; entry++) {
		#if defined(CONFIG_RTL_WTDOG)
		rtl_periodic_watchdog_kick(entry, WATCHDOG_NUM_OF_TIMES);
		#endif
		mCastv6_t = TAILQ_FIRST(&mCastTbl6.inuseList.mCastTbl[entry]);
		while (mCastv6_t) {
			/*save the next entry first*/
			nextMCastv6_t=TAILQ_NEXT(mCastv6_t, nextMCast); 		
			len += sprintf(page+len,"\t[%2d]  dip:%08x:%08x:%08x:%08x\n\t      sip:%08x:%08x:%08x:%08x\n\t      mbr:0x%x, svid:%d, spa:%d, ",
				entry,
				mCastv6_t->dip.v6_addr32[0],mCastv6_t->dip.v6_addr32[1],
				mCastv6_t->dip.v6_addr32[2],mCastv6_t->dip.v6_addr32[3],
				mCastv6_t->sip.v6_addr32[0],mCastv6_t->sip.v6_addr32[1],
				mCastv6_t->sip.v6_addr32[2],mCastv6_t->sip.v6_addr32[3],
				mCastv6_t->mbr,
				mCastv6_t->svid,
				mCastv6_t->port);
		#if defined CONFIG_RTL_MULTICAST_PORT_MAPPING
			len += sprintf(page+len,"age:%d, cpu:%d, count:%d, inAsic:%d, (%s), portMappingMask:0x%x\n", 
				mCastv6_t->age, mCastv6_t->cpu,mCastv6_t->count,mCastv6_t->inAsic,mCastv6_t->unKnownMCast?"UnknownMCast":"KnownMCast", mCastv6_t->mapPortMbr);
		#else
			len += sprintf(page+len,"age:%d, cpu:%d, count:%d, inAsic:%d, (%s)\n", 
				mCastv6_t->age, mCastv6_t->cpu,mCastv6_t->count,mCastv6_t->inAsic,mCastv6_t->unKnownMCast?"UnknownMCast":"KnownMCast");
		#endif
			cnt=0;
			curDescv6=MC_LIST_FIRST(&mCastv6_t->fwdDescChain);
			while(curDescv6)
			{
				nextDescv6=MC_LIST_NEXT(curDescv6, next );
				len += sprintf(page+len,"\t      netif(%s),descPortMask(0x%x),toCpu(%d),fwdPortMask(0x%x)\n",curDescv6->netifName,curDescv6->descPortMask,curDescv6->toCpu,curDescv6->fwdPortMask);
				curDescv6 = nextDescv6;
			}
				
			len += sprintf(page+len,"\n");
			mCastv6_t = nextMCastv6_t;
		}
			
	}
#endif
    *ret_len = len;
	return SUCCESS;
}
#endif
#endif

int rtl865x_genVirtualMCastFwdDescriptor(unsigned int forceToCpu, uint32 fwdPortMask, rtl865x_mcast_fwd_descriptor_t *fwdDescriptor)
{
	
	if(fwdDescriptor==NULL)
	{
		return FAILED;
	}
	
	memset(fwdDescriptor, 0, sizeof(rtl865x_mcast_fwd_descriptor_t ));
	fwdDescriptor->toCpu=forceToCpu;
	fwdDescriptor->fwdPortMask=fwdPortMask;
	return SUCCESS;

}
#if defined CONFIG_RTL_MULTICAST_PORT_MAPPING
int rtl865x_blockMulticastFlow(unsigned int srcVlanId, unsigned int srcPort,unsigned int srcIpAddr, unsigned int destIpAddr, unsigned int mapPortMask)
#else
int rtl865x_blockMulticastFlow(unsigned int srcVlanId, unsigned int srcPort,unsigned int srcIpAddr, unsigned int destIpAddr)
#endif
{
	rtl865x_mcast_fwd_descriptor_t fwdDescriptor;
	rtl865x_tblDrv_mCast_t * existMCastEntry=NULL;
	existMCastEntry=rtl865x_findMCastEntry(destIpAddr, srcIpAddr, (uint16)srcVlanId, (uint16)srcPort);
	if(existMCastEntry!=NULL)
	{
		if(existMCastEntry->mbr==0)
		{
			return SUCCESS;
		}
	}
	memset(&fwdDescriptor, 0, sizeof(rtl865x_mcast_fwd_descriptor_t ));
#if defined CONFIG_RTL_MULTICAST_PORT_MAPPING
	rtl865x_addMulticastEntry(destIpAddr, srcIpAddr, (unsigned short)srcVlanId, (unsigned short)srcPort, &fwdDescriptor, TRUE, 0, 0, 0, mapPortMask);
#else
	rtl865x_addMulticastEntry(destIpAddr, srcIpAddr, (unsigned short)srcVlanId, (unsigned short)srcPort, &fwdDescriptor, TRUE, 0, 0, 0);
#endif
	return SUCCESS;
}

/*
@func int32	| rtl865x_flushHWMulticastEntry	|  API to delete all multicast 
forwarding entry
@rvalue SUCCESS	|Delete hardware multicast forwarding entry successfully. 
@rvalue FAILED	|Delete hardware multicast forwarding entry failed.
*/

int32 rtl865x_flushHWMulticastEntry(void)
{

	rtl865x_tblDrv_mCast_t *mCastEntry, *nextMCastEntry;
	uint32 entry;
	

	for(entry=0; entry<RTL8651_MULTICASTTBL_SIZE; entry++) 
	{
		
		mCastEntry = TAILQ_FIRST(&mCastTbl.inuseList.mCastTbl[entry]);
		while (mCastEntry)
		{
			nextMCastEntry = TAILQ_NEXT(mCastEntry, nextMCast);
			if(mCastEntry->dip)
			{
				
				_rtl865x_freeMCastEntry(mCastEntry, entry);
				_rtl865x_arrangeMulticast(entry);
			}
			
			mCastEntry = nextMCastEntry;
		}
		


		
	}

	return SUCCESS;
}

int rtl865x_getMCastHashMethod(unsigned int *hashMethod)
{
	return rtl865x_getAsicMCastHashMethod(hashMethod);
}

int rtl865x_setMCastHashMethod(unsigned int hashMethod)
{
	uint32 oldHashMethod = 0;
	rtl865x_getAsicMCastHashMethod(&oldHashMethod);
	hashMethod&=0x3;
	if(hashMethod > 3)
	{
		return -1;
	}
	if(oldHashMethod != hashMethod)
	{
		rtl865x_setAsicMCastHashMethod(hashMethod);		
        /* exclude 0->1 and 1->0 */
        if(!((hashMethod == HASH_METHOD_SIP_DIP0 && oldHashMethod == HASH_METHOD_SIP_DIP1) || 
            (hashMethod == HASH_METHOD_SIP_DIP1 && oldHashMethod == HASH_METHOD_SIP_DIP0)))
        {

            rtl865x_flushHWMulticastEntry();                 
        }
	}
	return 0;
}

#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
static int is_ip6_addr_equal(inv6_addr_t addr1,inv6_addr_t addr2)
{
	if(addr1.v6_addr32[0]==addr2.v6_addr32[0]&&
	   addr1.v6_addr32[1]==addr2.v6_addr32[1]&&
	   addr1.v6_addr32[2]==addr2.v6_addr32[2]&&
	   addr1.v6_addr32[3]==addr2.v6_addr32[3])
	   return 1;
	else
	   return 0;
}
static int32 _rtl8198C_initMCastv6EntryPool(void)
{
	int32 index;
	rtl8198c_tblDrv_mCastv6_t *multiCast_t;
	struct MCast6_hash_head *mCast6_hash_head;
	
	TBL_MEM_ALLOC(multiCast_t, rtl8198c_tblDrv_mCastv6_t ,MAX_MCASTV6_TABLE_ENTRY_CNT);
	TAILQ_INIT(&mCastTbl6.freeList.freeMultiCast);
	for(index=0; index<MAX_MCASTV6_TABLE_ENTRY_CNT; index++)
	{
		memset( &multiCast_t[index], 0, sizeof(rtl8198c_tblDrv_mCastv6_t));
		TAILQ_INSERT_HEAD(&mCastTbl6.freeList.freeMultiCast, &multiCast_t[index], nextMCast);
	}

	TBL_MEM_ALLOC(mCast6_hash_head, struct MCast6_hash_head, RTL8651_MULTICASTTBL_SIZE);
	memset(mCast6_hash_head, 0,RTL8651_MULTICASTTBL_SIZE* sizeof(struct MCast6_hash_head));
	mCastTbl6.inuseList.mCastTbl = (void *)mCast6_hash_head;

	for(index=0; index<RTL8651_MULTICASTTBL_SIZE; index++)
	{
		TAILQ_INIT(&mCastTbl6.inuseList.mCastTbl[index]);
	}
	return SUCCESS;
}

static int32 _rtl8198C_freeMCastv6FwdDesc(rtl8198c_mcast_fwd_descriptor6_t *descPtr)
{
	if(descPtr==NULL)
	{
		return SUCCESS;
	}
	memset(descPtr,0,sizeof(rtl8198c_mcast_fwd_descriptor6_t));
	MC_LIST_INSERT_HEAD(&free_mcast_fwd_descriptor_head6, descPtr, next);
	
	return SUCCESS;
}


static rtl8198c_mcast_fwd_descriptor6_t *_rtl8198C_allocMCastv6FwdDesc(void)
{
	rtl8198c_mcast_fwd_descriptor6_t *retDesc=NULL;
	retDesc = MC_LIST_FIRST(&free_mcast_fwd_descriptor_head6);
	if(retDesc!=NULL)
	{
		MC_LIST_REMOVE(retDesc, next);
		memset(retDesc,0,sizeof(rtl8198c_mcast_fwd_descriptor6_t));
	}
	return retDesc;
}

static int32 _rtl8198C_mCastv6FwdDescEnqueue(mcast_fwd_descriptor_head6_t * queueHead,
												rtl8198c_mcast_fwd_descriptor6_t * enqueueDesc)
{
	rtl8198c_mcast_fwd_descriptor6_t *newDesc;
	rtl8198c_mcast_fwd_descriptor6_t *curDesc,*nextDesc;
	if(queueHead==NULL)
	{
		return FAILED;
	}
	
	if(enqueueDesc==NULL)
	{
		return SUCCESS;
	}
	
	/*multicast forward descriptor is internal maintained,always alloc new one*/
	newDesc=_rtl8198C_allocMCastv6FwdDesc();
	
	if(newDesc!=NULL)
	{
		memcpy(newDesc, enqueueDesc,sizeof(rtl8198c_mcast_fwd_descriptor6_t));
		memset(&(newDesc->next), 0, sizeof(MC_LIST_ENTRY(rtl8198c_mcast_fwd_descriptor6_t)));
		newDesc->next.le_next=NULL;
		newDesc->next.le_prev=NULL;
	}
	else
	{
		/*no enough memory*/
		return FAILED;
	}
	

	for(curDesc=MC_LIST_FIRST(queueHead);curDesc!=NULL;curDesc=nextDesc)
	{

		nextDesc=MC_LIST_NEXT(curDesc, next);
		
		/*merge two descriptor*/
		if((strcmp(curDesc->netifName,newDesc->netifName)==0) && (curDesc->vid==newDesc->vid))
		if(strcmp(curDesc->netifName,newDesc->netifName)==0)
		{	
			if(newDesc->descPortMask==0)
			{
				newDesc->descPortMask=curDesc->descPortMask;
			}
			MC_LIST_REMOVE(curDesc, next);
			_rtl8198C_freeMCastv6FwdDesc(curDesc);
			
		}
	}

	/*not matched descriptor is found*/
	MC_LIST_INSERT_HEAD(queueHead, newDesc, next);

	return SUCCESS;
	
}

static int32 _rtl8198C_flushMCastv6FwdDescChain(mcast_fwd_descriptor_head6_t * descChainHead)
{
	rtl8198c_mcast_fwd_descriptor6_t * curDesc,*nextDesc;
	
	if(descChainHead==NULL)
	{
		return SUCCESS;
	}
	
	curDesc=MC_LIST_FIRST(descChainHead);
	while(curDesc)
	{
		nextDesc=MC_LIST_NEXT(curDesc, next );
		/*remove from the old descriptor chain*/
		MC_LIST_REMOVE(curDesc, next);
		/*return to the free descriptor chain*/
		_rtl8198C_freeMCastv6FwdDesc(curDesc);
		curDesc = nextDesc;
	}

	return SUCCESS;
}


static int32 _rtl8198C_flushMCastv6Entry(rtl8198c_tblDrv_mCastv6_t *mCastEntry)
{
	if(mCastEntry==NULL)
	{
		return SUCCESS;
	}
	
	_rtl8198C_flushMCastv6FwdDescChain(&mCastEntry->fwdDescChain);
	
	memset(mCastEntry, 0, sizeof(rtl8198c_tblDrv_mCastv6_t));
	return SUCCESS;
}


static int32 _rtl8198C_freeMCastv6Entry(rtl8198c_tblDrv_mCastv6_t * mCastEntry, uint32 hashIndex)
{
	if(mCastEntry==NULL)
	{
		return SUCCESS;
	}
	
	TAILQ_REMOVE(&mCastTbl6.inuseList.mCastTbl[hashIndex], mCastEntry, nextMCast);
	_rtl8198C_flushMCastv6Entry(mCastEntry);
	TAILQ_INSERT_HEAD(&mCastTbl6.freeList.freeMultiCast, mCastEntry, nextMCast);
	return SUCCESS;
}


static void _rtl8198C_arrangeMulticastv6(uint32 entryIndex)
{
	rtl8198C_tblAsicDrv_multiCastv6Param_t asic_mcast;
	rtl8198c_tblDrv_mCastv6_t *mCast_t=NULL;
	rtl8198c_tblDrv_mCastv6_t *select_t=NULL;
	rtl8198c_tblDrv_mCastv6_t *swapOutEntry=NULL;
	int32 retval;
	int32 hashMethod=0;
	rtl8198C_getMCastv6HashMethod(&hashMethod);
	TAILQ_FOREACH(mCast_t, &mCastTbl6.inuseList.mCastTbl[entryIndex], nextMCast) 
	{
		if ((mCast_t->cpu == 0) && !(mCast_t->flag & RTL865X_MULTICAST_PPPOEPATCH_CPUBIT)) 
		{ /* Ignore cpu=1 */

			if(mCast_t->inAsic==TRUE)
			{
				if(swapOutEntry==NULL)
				{
					swapOutEntry=mCast_t;
				}
				else
				{
					/*impossible, two flow in one asic entry*/
					swapOutEntry->inAsic=FALSE;
					mCast_t->inAsic = FALSE;
				}
			}
		
			if (select_t) 
			{
				if ((mCast_t->unKnownMCast==FALSE) && (select_t->unKnownMCast==TRUE))
				{
					/*replace unknown multicast*/
					select_t = mCast_t;
				}
				else
				{
					/*select the heavy load*/
					if ((mCast_t->count) > (select_t->count))
					{
						select_t = mCast_t;
					}
				}
				
			}
			else 
			{
				select_t = mCast_t;
			}
			
			
		}
		else
		{
			mCast_t->inAsic = FALSE;	/* reset "inAsic" bit */
		} 
	}
	
	if(select_t && swapOutEntry)
	{
		if ((swapOutEntry->unKnownMCast==FALSE) && (select_t->unKnownMCast==TRUE))
		{
			/*replace unknown multicast*/
			select_t = swapOutEntry;
		}
		else
		{
			if((select_t->count <= (swapOutEntry->count+RTL865X_HW_MCAST_SWAP_GAP)))
				select_t = swapOutEntry;
		}
	}	
	
	if (select_t) 
	{
		if((swapOutEntry==NULL) ||(select_t==swapOutEntry))
		{
			select_t->age = RTL865X_MULTICAST_TABLE_ASIC_AGE;
			bzero(&asic_mcast, sizeof(rtl8198C_tblAsicDrv_multiCastv6Param_t));
//			memcpy(&asic_mcast, select_t, (uint32)&(((rtl8198c_tblDrv_mCastv6_t *)0)->six_rd_idx));
			memcpy(&asic_mcast, select_t, sizeof(rtl8198C_tblAsicDrv_multiCastv6Param_t));
			
			retval = rtl8198C_setAsicIpMulticastv6Table(hashMethod,&asic_mcast);
			
#ifdef CONFIG_PROC_FS
			mcastAddOpCnt6++;
#endif
			assert(retval == SUCCESS);
			if(retval==SUCCESS)
			{
				select_t->inAsic = TRUE;
			}
			else
			{
				select_t->inAsic = FALSE;
				rtl8198C_delAsicIpMulticastv6Table(entryIndex);
#ifdef CONFIG_PROC_FS
				mcastDelOpCnt6++;
#endif
			}
				
			assert(retval == SUCCESS);
			TAILQ_REMOVE(&mCastTbl6.inuseList.mCastTbl[entryIndex], select_t, nextMCast);
			TAILQ_INSERT_HEAD(&mCastTbl6.inuseList.mCastTbl[entryIndex], select_t, nextMCast);
		}
		else/*(swapOutEntry!=NULL) && (select_t!=swapOutEntry)*/
		{
			
			/*disable swap and only explicit joined mulicast flow can replace unknown multicast flow*/
			if(1)
			{
				/*don't forget to set swapOutEntry's inAsic flag*/
				swapOutEntry->inAsic=FALSE;
				
				select_t->age = RTL865X_MULTICAST_TABLE_ASIC_AGE;
				bzero(&asic_mcast, sizeof(rtl8198C_tblAsicDrv_multiCastv6Param_t));
				memcpy(&asic_mcast, select_t, (uint32)&(((rtl8198C_tblAsicDrv_multiCastv6Param_t *)0)->six_rd_idx));
				retval = rtl8198C_setAsicIpMulticastv6Table(hashMethod,&asic_mcast);
				//printk("asic_mcast:dip:%x,[%s]:[%d].\n",asic_mcast.dip,__FUNCTION__,__LINE__);
#ifdef CONFIG_PROC_FS
				mcastAddOpCnt6++;
				
#endif
				assert(retval == SUCCESS);
				if(retval==SUCCESS)
				{
					select_t->inAsic = TRUE;
				}
				else
				{
					select_t->inAsic = FALSE;
					rtl8198C_delAsicIpMulticastv6Table(entryIndex);
#ifdef CONFIG_PROC_FS
					mcastDelOpCnt6++;
#endif
				}
				
				TAILQ_REMOVE(&mCastTbl6.inuseList.mCastTbl[entryIndex], select_t, nextMCast);
				TAILQ_INSERT_HEAD(&mCastTbl6.inuseList.mCastTbl[entryIndex], select_t, nextMCast);

			}
			#if 0
			else
			{	
				//printk("swapOutEntry:%d,select:%d,[%s]:[%d].\n",swapOutEntry->unKnownMCast,select_t->unKnownMCast,__FUNCTION__,__LINE__);
				if(swapOutEntry->inAsic == FALSE)
				{
					/*maybe something is wrong, we remove the asic entry*/
					rtl8651_delAsicIpMulticastTable(entryIndex);
#ifdef CONFIG_PROC_FS
					mcastDelOpCnt6++;
#endif
				}
				
			}	
			#endif
			
		}
		
	}
	else 	
	{
		if(swapOutEntry!=NULL)
		{
			swapOutEntry->inAsic=FALSE;
		}
		rtl8198C_delAsicIpMulticastv6Table(entryIndex);
#ifdef CONFIG_PROC_FS
		mcastDelOpCnt6++;
#endif
	}
}

void _rtl8198C_timeUpdateMulticastv6(uint32 secPassed)
{
	rtl8198c_tblDrv_mCastv6_t *mCast_t, *nextMCast_t;
	uint32 entry;
	uint32 needReArrange=FALSE;
	uint32 hashLineCnt=0;
	//printk("[%s:%d]\n",__FUNCTION__,__LINE__);
	/* check to Aging and HW swapping */
	for (entry=0; entry< RTL8651_MULTICASTTBL_SIZE; entry++) {
		hashLineCnt=0;
		needReArrange=FALSE;
		mCast_t = TAILQ_FIRST(&mCastTbl6.inuseList.mCastTbl[entry]);
		while (mCast_t) {
			/*save the next entry first*/
			nextMCast_t=TAILQ_NEXT(mCast_t, nextMCast);
			
			if (mCast_t->inAsic == TRUE)
			{
				/* Entry is in the ASIC */
				if (mCast_t->age <= secPassed) 
				{
					if(mCast_t->mbr==0)
					{
						_rtl8198C_freeMCastv6Entry(mCast_t,entry);
						needReArrange=TRUE;
					}
					else
					{
						mCast_t->age = RTL865X_MULTICAST_TABLE_AGE;
					}
				}
				else
				{
					mCast_t->age -= secPassed;
				}
			}
			else 
			{
				//printk("------------mCast_t->count:%d,[%s]:[%d].\n",mCast_t->count,__FUNCTION__,__LINE__);
			
				//mCast_t->count=0;
			
				/* Entry is not in the ASIC */
				if (mCast_t->age <= secPassed)
				{ /* aging out */
					_rtl8198C_freeMCastv6Entry(mCast_t, entry);
				}
				else 
				{
					mCast_t->age -= secPassed;
				}
			}
			
			/*won't count multicast entry forwarded by cpu*/
			if(mCast_t->cpu==0)
			{
				
				hashLineCnt++;
				//printk("------------hashLineCnt:%d,[%s]:[%d].\n",hashLineCnt,__FUNCTION__,__LINE__);
				if(hashLineCnt>=2)
				{
					needReArrange=TRUE;
				}
			}
		
			//mCast_t->count = 0;
			mCast_t = nextMCast_t;
		}
		
		if(needReArrange==TRUE)
		{
			//printk("------------entry:%d,hashLineCnt:%d,[%s]:[%d].\n",entry,hashLineCnt,__FUNCTION__,__LINE__);
			_rtl8198C_arrangeMulticastv6(entry);
		}
		mCast_t = TAILQ_FIRST(&mCastTbl6.inuseList.mCastTbl[entry]);
		
		while (mCast_t) {
			/*save the next entry first*/
			nextMCast_t=TAILQ_NEXT(mCast_t, nextMCast);
			
			if (mCast_t->inAsic == FALSE)
				mCast_t->count=0;
			
			mCast_t = nextMCast_t;
		}
		
	}
}

#if defined(__linux__) && defined(__KERNEL__)
static void _rtl8198C_mCastv6SysTimerExpired(uint32 expireDada)
{

	_rtl8198C_timeUpdateMulticastv6(1);
	mod_timer(&rtl8198c_mCast6SysTimer, jiffies+HZ);
	
}

static void _rtl8198C_initMCastv6SysTimer(void)
{
	init_timer(&rtl8198c_mCast6SysTimer);
	rtl8198c_mCast6SysTimer.data=rtl8198c_mCast6SysTimer.expires;
	rtl8198c_mCast6SysTimer.expires=jiffies+HZ;
	rtl8198c_mCast6SysTimer.function=(void*)_rtl8198C_mCastv6SysTimerExpired;
	add_timer(&rtl8198c_mCast6SysTimer);
}
static void _rtl8198C_destroyMCastv6SysTimer(void)
{
	del_timer(&rtl8198c_mCast6SysTimer);
}
#endif


#if defined (CONFIG_RTL_HARDWARE_MULTICAST_CAM)

int rtl8198C_findEmptyCamEntryv6(void)
{
	int index=-1;

	for(index=RTL8651_IPMULTICASTTBL_SIZE; index<RTL8651_MULTICASTTBL_SIZE; index++)
	{

		if(TAILQ_EMPTY(&mCastTbl6.inuseList.mCastTbl[index]))
		{
			return index;
		}
	}
	
	return -1;
}
#endif


static int32 _rtl8198C_initMCastv6FwdDescPool(void)
{
	int32 i;
	MC_LIST_INIT(&free_mcast_fwd_descriptor_head6);
	TBL_MEM_ALLOC(rtl8198c_mcastFwdDescPool6, rtl8198c_mcast_fwd_descriptor6_t,MAX_MCASTV6_FWD_DESCRIPTOR_CNT);
	
	if(rtl8198c_mcastFwdDescPool6!=NULL)
	{
		memset( rtl8198c_mcastFwdDescPool6, 0, MAX_MCASTV6_FWD_DESCRIPTOR_CNT * sizeof(rtl8198c_mcast_fwd_descriptor6_t));	
	}
	else
	{
		return FAILED;
	}
	
	for(i = 0; i<MAX_MCASTV6_FWD_DESCRIPTOR_CNT;i++)
	{
		MC_LIST_INSERT_HEAD(&free_mcast_fwd_descriptor_head6, &rtl8198c_mcastFwdDescPool6[i], next);
	}
	
	return SUCCESS;
}

static rtl8198c_tblDrv_mCastv6_t * _rtl8198C_allocMCastv6Entry(uint32 hashIndex)
{
	rtl8198c_tblDrv_mCastv6_t *newEntry;
	newEntry=TAILQ_FIRST(&mCastTbl6.freeList.freeMultiCast);
	if (newEntry == NULL)
	{
		return NULL;
	}		
	
	TAILQ_REMOVE(&mCastTbl6.freeList.freeMultiCast, newEntry, nextMCast);

	
	/*initialize it*/
	if(MC_LIST_FIRST(&newEntry->fwdDescChain)!=NULL)
	{
		_rtl8198C_flushMCastv6FwdDescChain(&newEntry->fwdDescChain);
	}
	MC_LIST_INIT(&newEntry->fwdDescChain);
	
	memset(newEntry, 0, sizeof(rtl8198c_tblDrv_mCastv6_t));

	TAILQ_INSERT_TAIL(&mCastTbl6.inuseList.mCastTbl[hashIndex], newEntry, nextMCast);
	
	return newEntry;
}

static int32 _rtl8198C_mergeMCastv6FwdDescChain(mcast_fwd_descriptor_head6_t * targetChainHead ,
													rtl8198c_mcast_fwd_descriptor6_t *srcChain)
{
	rtl8198c_mcast_fwd_descriptor6_t *curDesc;

	if(targetChainHead==NULL)
	{
		return FAILED;
	}
	
	for(curDesc=srcChain; curDesc!=NULL; curDesc=MC_LIST_NEXT(curDesc,next))
	{
		
		_rtl8198C_mCastv6FwdDescEnqueue(targetChainHead, curDesc);
		
	}
	
	return SUCCESS;
}

static uint32 _rtl8198C_doMCastv6EntrySrcVlanPortFilter(rtl8198c_tblDrv_mCastv6_t *mCastEntry)
{
	rtl8198c_mcast_fwd_descriptor6_t * curDesc,*nextDesc;
	if(mCastEntry==NULL)
	{
		return SUCCESS;
	}
	
	for(curDesc=MC_LIST_FIRST(&mCastEntry->fwdDescChain);curDesc!=NULL;curDesc=nextDesc)
	{
		nextDesc=MC_LIST_NEXT(curDesc, next);
		{
			curDesc->fwdPortMask=curDesc->fwdPortMask & (~(1<<mCastEntry->port));
			if(curDesc->fwdPortMask==0)
			{
				/*remove from the old chain*/
				MC_LIST_REMOVE(curDesc, next);
				/*return to the free descriptor chain*/
				_rtl8198C_freeMCastv6FwdDesc(curDesc);

			}
		}
		
	}

	return SUCCESS;
}

#if defined(CONFIG_RTL_HARDWARE_MULTICAST_API)
static uint32 rtl8198C_genMCastv6EntryAsicFwdMaskIgnoreCpuFlag(rtl8198c_tblDrv_mCastv6_t *mCastEntry)
{
	uint32 asicFwdPortMask=0;
	rtl8198c_mcast_fwd_descriptor6_t * curDesc;
	if(mCastEntry==NULL)
	{
		return 0;
	}
	
	MC_LIST_FOREACH(curDesc, &(mCastEntry->fwdDescChain), next)
	{
		asicFwdPortMask|=(curDesc->fwdPortMask & ((1<<RTL8651_MAC_NUMBER)-1));
	}
	asicFwdPortMask = asicFwdPortMask & (~(1<<mCastEntry->port)); 
	return asicFwdPortMask;
}

#endif
static uint32 rtl8198C_genMCastv6EntryAsicFwdMask(rtl8198c_tblDrv_mCastv6_t *mCastEntry)
{
	uint32 asicFwdPortMask=0;
	rtl8198c_mcast_fwd_descriptor6_t * curDesc;
	if(mCastEntry==NULL)
	{
		return 0;
	}
	
	MC_LIST_FOREACH(curDesc, &(mCastEntry->fwdDescChain), next)
	{
		if(curDesc->toCpu==0)
		{
			asicFwdPortMask|=(curDesc->fwdPortMask & ((1<<RTL8651_MAC_NUMBER)-1));
		}
		else
		{
			asicFwdPortMask|=( 0x01<<RTL8651_MAC_NUMBER);
		}
	}
	asicFwdPortMask = asicFwdPortMask & (~(1<<mCastEntry->port)); 
	return asicFwdPortMask;
}

static uint16 rtl8198C_genMCastv6EntryCpuFlag(rtl8198c_tblDrv_mCastv6_t *mCastEntry)
{
	uint16 cpuFlag=FALSE;
	rtl8198c_mcast_fwd_descriptor6_t * curDesc;
	if(mCastEntry==NULL)
	{
		return 0;
	}

	if(mCastEntry->cpuHold==TRUE)
	{
		cpuFlag=TRUE;
	}
	
	MC_LIST_FOREACH(curDesc, &(mCastEntry->fwdDescChain), next)
	{
		if(	(curDesc->toCpu==TRUE)	||
			(memcmp(curDesc->netifName, RTL_WLAN_NAME,4)==0)	)
		{
			cpuFlag=TRUE;
		}
	}
	
	return cpuFlag;
}

static void  _rtl8198C_setASICMulticastv6PortStatus(void) {
	uint32 index;
	for (index=0; index<RTL8651_PORT_NUMBER+rtl8651_totalExtPortNum; index++) {
		rtl8651_setAsicMulticastPortInternal(index, (rtl865x_externalMulticastPortMask&(1<<index))?FALSE:TRUE);
	}
}


static void _rtl8198C_mCastv6EntryReclaim(void)
{
	uint32 index;
	uint32 freeCnt=0;
	uint32 asicFwdPortMask=0;
	uint32 needReArrange=FALSE;
	rtl8198c_tblDrv_mCastv6_t *curMCastEntry, *nextMCastEntry;

	/*free unused software forward entry*/
	for(index=0; index<RTL8651_MULTICASTTBL_SIZE; index++) 
	{
		curMCastEntry = TAILQ_FIRST(&mCastTbl6.inuseList.mCastTbl[index]);
		while (curMCastEntry)
		{
			nextMCastEntry = TAILQ_NEXT(curMCastEntry, nextMCast);
			if((curMCastEntry->inAsic==FALSE)  && (curMCastEntry->count==0))
			{
				_rtl8198C_freeMCastv6Entry(curMCastEntry, index);
				freeCnt++;
			}
			curMCastEntry = nextMCastEntry;
		}
		
	}

	if(freeCnt>0)
	{
		return;
	}
	
	for(index=0; index<RTL8651_MULTICASTTBL_SIZE; index++) 
	{
		curMCastEntry = TAILQ_FIRST(&mCastTbl6.inuseList.mCastTbl[index]);
		needReArrange=FALSE;
		while (curMCastEntry)
		{
			nextMCastEntry = TAILQ_NEXT(curMCastEntry, nextMCast);
			if(curMCastEntry->inAsic)
			{
				asicFwdPortMask=rtl8198C_genMCastv6EntryAsicFwdMask(curMCastEntry);
				if(asicFwdPortMask==0) 
				{
					_rtl8198C_freeMCastv6Entry(curMCastEntry,index);
					needReArrange=TRUE;
				}
			}
			curMCastEntry = nextMCastEntry;
		}
		
		if(needReArrange==TRUE)
		{
			_rtl8198C_arrangeMulticastv6(index);
		}
	}

	return;
}

static inline void _rtl8198C_patchPppoeWeakv6(rtl8198c_tblDrv_mCastv6_t *mCast_t)
{
	rtl8198c_mcast_fwd_descriptor6_t * curDesc;
	uint32 netifType;
	/* patch: keep cache in software if one vlan's interface is pppoe */
	MC_LIST_FOREACH(curDesc, &(mCast_t->fwdDescChain), next)
	{
		if(rtl865x_getNetifType(curDesc->netifName, &netifType)==SUCCESS)
		{
			/*how about pptp,l2tp?*/
			if(netifType==IF_PPPOE)
			{
				mCast_t->flag |= RTL865X_MULTICAST_PPPOEPATCH_CPUBIT;
				return;
			}
		}
		
	}

	mCast_t->flag &= ~RTL865X_MULTICAST_PPPOEPATCH_CPUBIT;
}
//static 
uint32 rtl8198C_getMCastv6EntryDescPortMask(rtl8198c_tblDrv_mCastv6_t *mCastEntry)
{
	uint32 descPortMask=0;
	rtl8198c_mcast_fwd_descriptor6_t * curDesc;
	if(mCastEntry==NULL)
	{
		return 0;
	}
	
	MC_LIST_FOREACH(curDesc, &(mCastEntry->fwdDescChain), next)
	{
		descPortMask=descPortMask | curDesc->descPortMask;
	}
	
	return descPortMask;
}

static int32 rtl8198C_multicastv6CallbackFn(void *param)
{
	#if defined (CONFIG_RTL_MLD_SNOOPING)
	inv6_addr_t groupAddr;
	uint32 index;
#if !defined(CONFIG_RTL_MULTI_LAN_DEV)
	uint32 oldDescPortMask;
#if !defined(CONFIG_RT_MULTIPLE_BR_SUPPOR)
	uint32 newDescPortMask;/*for device decriptor forwarding usage*/
#endif
#endif	
	uint32 oldAsicFwdPortMask,newAsicFwdPortMask;/*for physical port forwarding usage*/
	uint32 oldCpuFlag = 0,newCpuFlag = 0;
		
	rtl_multicastEventContext_t mcastEventContext;
	
	rtl8198c_mcast_fwd_descriptor6_t newFwdDesc;
	struct rtl_multicastDataInfo multicastDataInfo;
	struct rtl_multicastFwdInfo multicastFwdInfo;
	rtl8198c_tblDrv_mCastv6_t *mCastEntry,*nextMCastEntry;
#if !defined(CONFIG_RTL_MULTI_LAN_DEV) && !defined(CONFIG_RT_MULTIPLE_BR_SUPPORT)
	struct rtl_multicastDeviceInfo_s bridgeMCastDev;
#endif
#ifdef CONFIG_RTL_MLD_SNOOPING
	struct rtl_groupInfo groupInfo;
#endif

#if defined(CONFIG_RT_MULTIPLE_BR_SUPPORT)
	unsigned char br_name[16]={0};
	int br_index;
#if defined(CONFIG_RTL_MULTI_LAN_DEV)
	struct net_bridge* br_dev = NULL;
#else
	unsigned int br_moduleIndex = 0xFFFFFFFF;
	#if defined(CONFIG_RTL_VLAN_8021Q)&&defined(CONFIG_RTL_MULTICAST_PORT_MAPPING)
	struct net_bridge* br_tmp;
	unsigned int wlanPhyPortMask;
	#endif
#endif
#endif
	int32 retVal=FAILED;
	
	if(param==NULL)
	{
		return EVENT_CONTINUE_EXECUTE;
	}
	memcpy(&mcastEventContext,param,sizeof(rtl_multicastEventContext_t));
	/*check device name's validity*/
	if(strlen(mcastEventContext.devName)==0)
	{
		return EVENT_CONTINUE_EXECUTE;
	}
	#ifdef CONFIG_RTL865X_MUTLICAST_DEBUG
	printk("%s:%d,mcastEventContext.devName is %s, mcastEventContext.groupAddr is 0x%x,mcastEventContext.sourceIP is 0x%x,mcastEventContext.portMask is 0x%x\n",__FUNCTION__,__LINE__,mcastEventContext.devName, mcastEventContext.groupAddr[0], mcastEventContext.sourceAddr[0], mcastEventContext.portMask);
	#endif
	bzero(&groupAddr,sizeof(inv6_addr_t));
	memcpy(&groupAddr,mcastEventContext.groupAddr,sizeof(inv6_addr_t));
	/*case 1:this is multicast event from bridge(br0) */
	/*sync wlan and ethernet*/
	//hyking:[Fix me] the RTL_BR_NAME...
#if !defined(CONFIG_RTL_MULTI_LAN_DEV)
#if defined(CONFIG_OPENWRT_SDK)
#ifdef CONFIG_RTK_VLAN_WAN_TAG_SUPPORT
    if(memcmp(mcastEventContext.devName,RTL_BR_NAME,6)==0 || memcmp(mcastEventContext.devName,RTL_BR1_NAME,3)==0)
#else
	if(memcmp(mcastEventContext.devName,RTL_BR_NAME,6)==0)
#endif
#else
#ifdef CONFIG_RTK_VLAN_WAN_TAG_SUPPORT
	if(memcmp(mcastEventContext.devName,RTL_BR_NAME,3)==0 || memcmp(mcastEventContext.devName,RTL_BR1_NAME,3)==0)
#elif defined(CONFIG_RT_MULTIPLE_BR_SUPPORT)
	if(memcmp(mcastEventContext.devName,RTL_BR_NAME,2)==0)
#else
	if(memcmp(mcastEventContext.devName,RTL_BR_NAME,3)==0)
#endif
#endif
	{
		for (index=0; index< RTL8651_MULTICASTTBL_SIZE; index++)
		{
			for (mCastEntry = TAILQ_FIRST(&mCastTbl6.inuseList.mCastTbl[index]); mCastEntry; mCastEntry = nextMCastEntry)
			{
				nextMCastEntry=TAILQ_NEXT(mCastEntry, nextMCast);
				if (!is_ip6_addr_equal(mCastEntry->dip,groupAddr))
				{
					continue;
				}
	
				rtl_getGroupInfov6((uint32*)&mCastEntry->dip, &groupInfo);
				if(groupInfo.ownerMask==0)
				{
					mCastEntry->unKnownMCast=TRUE;
				}
				else
				{
					mCastEntry->unKnownMCast=FALSE;
				}
	
				oldDescPortMask=rtl8198C_getMCastv6EntryDescPortMask( mCastEntry); 
					
				/*sync with control plane*/
				#if defined CONFIG_RT_MULTIPLE_BR_SUPPORT
				oldCpuFlag=mCastEntry->cpu;
				newDescPortMask = 0;
				newCpuFlag = 0;
				
				for(br_index = 0; br_index<RTL_IMGP_MAX_BRMODULE; br_index++)
				{	
					br_moduleIndex=rtl_get_brIgmpModuleIndexbyId(br_index,br_name);
					if(br_moduleIndex==0xFFFFFFFF)
						continue;

					/*sync with control plane*/
					memset(&newFwdDesc, 0 ,sizeof(rtl8198c_mcast_fwd_descriptor6_t));
					strcpy(newFwdDesc.netifName,br_name);
				
					multicastDataInfo.ipVersion=6;
					memcpy(multicastDataInfo.sourceIp, &mCastEntry->sip, sizeof(struct inv6_addr_s));
					memcpy(multicastDataInfo.groupAddr, &mCastEntry->dip, sizeof(struct inv6_addr_s));
				
				
					retVal= rtl_getMulticastDataFwdInfo(br_moduleIndex, &multicastDataInfo, &multicastFwdInfo);
					

					if(retVal == SUCCESS)
					{
#if defined(CONFIG_RTL_VLAN_8021Q)&&defined(CONFIG_RTL_MULTICAST_PORT_MAPPING)
						if(multicastFwdInfo.cpuFlag)
						{
							br_tmp = rtl_get_brByIndex(br_index, br_name);
							wlanPhyPortMask = rtl865x_getWlanPhyFwdPortMaskbyVlan(br_tmp, multicastFwdInfo.fwdPortMask);
							if((wlanPhyPortMask&(mCastEntry->mapPortMbr))==0) //forward to wlan in other vlan
								multicastFwdInfo.cpuFlag=0;
						}			
#endif
						newCpuFlag |= multicastFwdInfo.cpuFlag;
					}
					
				}

				if(mCastEntry->unKnownMCast==TRUE)
					newCpuFlag = 1;	

				if(oldCpuFlag != newCpuFlag)
				{
					_rtl8198C_freeMCastv6Entry(mCastEntry,index);
					_rtl8198C_arrangeMulticastv6(index);
				}
				//printk("br:%s :oldAsicPortMask:%x,newAsicPortMask:%x,oldCpuFlag:%d,newCpuFlag:%d,[%s]:[%d].\n",devName,oldAsicPortMask,newAsicPortMask,oldCpuFlag,newCpuFlag,__FUNCTION__,__LINE__);
				#else
				memset(&newFwdDesc, 0 ,sizeof(rtl8198c_mcast_fwd_descriptor6_t));
				strcpy(newFwdDesc.netifName,mcastEventContext.devName);
				multicastDataInfo.ipVersion=6;
				memcpy(multicastDataInfo.sourceIp,&mCastEntry->sip,sizeof(inv6_addr_t));
				memcpy(multicastDataInfo.groupAddr,&mCastEntry->dip,sizeof(inv6_addr_t));
				retVal= rtl_getMulticastDataFwdInfo(mcastEventContext.moduleIndex, &multicastDataInfo, &multicastFwdInfo);
					
	
				if(retVal!=SUCCESS)
				{
					continue;
				}
					
				retVal= rtl_getIgmpSnoopingModuleDevInfo(mcastEventContext.moduleIndex, &bridgeMCastDev);
				if(retVal!=SUCCESS)
				{
					continue;
				}
				newDescPortMask=multicastFwdInfo.fwdPortMask;
				if( (oldDescPortMask != newDescPortMask) &&
					(	((newDescPortMask & bridgeMCastDev.swPortMask)!=0) ||
						(((oldDescPortMask & bridgeMCastDev.swPortMask) !=0) && ((newDescPortMask & bridgeMCastDev.swPortMask)==0)))	)
				{
					/*this multicast entry should be re-generated at linux protocol stack bridge level*/
					_rtl8198C_freeMCastv6Entry(mCastEntry,index);
					_rtl8198C_arrangeMulticastv6(index);
				}
				#endif
					
			}
		}
			
		return EVENT_CONTINUE_EXECUTE;
	}

	/*case 2:this is multicast event from ethernet (eth0)*/
	/*update ethernet forwarding port mask*/

	#ifdef CONFIG_RTK_VLAN_WAN_TAG_SUPPORT
	if(memcmp(mcastEventContext.devName,"eth*",4)==0 || memcmp(mcastEventContext.devName,RTL_PS_ETH_NAME_ETH2,4)==0)
	#else
	if(memcmp(mcastEventContext.devName,"eth*",4)==0)
	#endif

	{
	#ifdef CONFIG_RTL865X_MUTLICAST_DEBUG
		printk("%s:%d,multicast event from ethernet (%s),mcastEventContext.groupAddr[0] is 0x%x\n",__FUNCTION__,__LINE__,mcastEventContext.devName,mcastEventContext.groupAddr[0]);
	#endif
			
		for (index=0; index< RTL8651_MULTICASTTBL_SIZE; index++)
		{
			for (mCastEntry = TAILQ_FIRST(&mCastTbl6.inuseList.mCastTbl[index]); mCastEntry; mCastEntry = nextMCastEntry)
			{
				nextMCastEntry=TAILQ_NEXT(mCastEntry, nextMCast);
#if	0				
				printk("mCastEntry->dip:%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n",
					mCastEntry->dip.v6_addr16[0],mCastEntry->dip.v6_addr16[1],
					mCastEntry->dip.v6_addr16[2],mCastEntry->dip.v6_addr16[3],
					mCastEntry->dip.v6_addr16[4],mCastEntry->dip.v6_addr16[5],
					mCastEntry->dip.v6_addr16[6],mCastEntry->dip.v6_addr16[7]);
				printk("groupAddr:%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n",
					groupAddr.v6_addr16[0],groupAddr.v6_addr16[1],
					groupAddr.v6_addr16[2],groupAddr.v6_addr16[3],
					groupAddr.v6_addr16[4],groupAddr.v6_addr16[5],
					groupAddr.v6_addr16[6],groupAddr.v6_addr16[7]);
#endif
				if (!is_ip6_addr_equal(mCastEntry->dip,groupAddr))
				{
					continue;
				}
					
				memset(&newFwdDesc, 0 ,sizeof(rtl8198c_mcast_fwd_descriptor6_t));
				strcpy(newFwdDesc.netifName,mcastEventContext.devName);
	
				/*save old multicast entry forward port mask*/
				oldAsicFwdPortMask=mCastEntry->mbr;
				oldCpuFlag=mCastEntry->cpu;
	
				/*sync with control plane*/
				multicastDataInfo.ipVersion=6;
				memcpy(multicastDataInfo.sourceIp,&mCastEntry->sip,sizeof(inv6_addr_t));
				memcpy(multicastDataInfo.groupAddr,&mCastEntry->dip,sizeof(inv6_addr_t));
				retVal= rtl_getMulticastDataFwdInfo(mcastEventContext.moduleIndex, &multicastDataInfo, &multicastFwdInfo);
				if (multicastFwdInfo.unknownMCast == TRUE)
					multicastFwdInfo.cpuFlag = TRUE;

				newFwdDesc.fwdPortMask=multicastFwdInfo.fwdPortMask & (~(1<<mCastEntry->port));
				newFwdDesc.toCpu=multicastFwdInfo.cpuFlag;
				
#if defined CONFIG_RTL_MULTICAST_PORT_MAPPING
				newFwdDesc.fwdPortMask &= mCastEntry->mapPortMbr;
#endif
				
				/*update/replace old forward descriptor*/

				_rtl8198C_mergeMCastv6FwdDescChain(&mCastEntry->fwdDescChain,&newFwdDesc);
				mCastEntry->mbr 		= rtl8198C_genMCastv6EntryAsicFwdMask(mCastEntry);
				mCastEntry->cpu 	= rtl8198C_genMCastv6EntryCpuFlag(mCastEntry);
					
				newAsicFwdPortMask	= mCastEntry->mbr ;
				newCpuFlag			=mCastEntry->cpu;
					
			#ifdef CONFIG_RTL865X_MUTLICAST_DEBUG
				printk("%s:%d,oldAsicFwdPortMask is %d,newAsicFwdPortMask is %d\n",__FUNCTION__,__LINE__,oldAsicFwdPortMask,newAsicFwdPortMask);
			#endif
					
#ifndef RTL8651_MCAST_ALWAYS2UPSTREAM
				if (mCastEntry->flag & RTL865X_MULTICAST_UPLOADONLY)
				{	/* remove upload term*/
					if((newAsicFwdPortMask!=0) && (newAsicFwdPortMask!=oldAsicFwdPortMask))
					{
						mCastEntry->flag &= ~RTL865X_MULTICAST_UPLOADONLY;
						/* we assume multicast member will NEVER in External interface, so we remove
								external ip now */
						mCastEntry->flag &= ~RTL865X_MULTICAST_EXTIP_SET;
					}
				}
#endif /* RTL8651_MCAST_ALWAYS2UPSTREAM */
#ifdef CONFIG_RTL_MLD_SNOOPING	
				rtl_getGroupInfov6((uint32*)&mCastEntry->dip, &groupInfo);
				if(groupInfo.ownerMask==0)
				{
					mCastEntry->unKnownMCast=TRUE;
				}
				else
				{
					mCastEntry->unKnownMCast=FALSE;
				}
#endif		
				if((oldCpuFlag != newCpuFlag)||(newAsicFwdPortMask!=oldAsicFwdPortMask)) 
				{
					_rtl8198C_patchPppoeWeakv6(mCastEntry);
						
					/*reset inAsic flag to re-select or re-write this hardware asic entry*/
					if(newAsicFwdPortMask==0)
					{
						_rtl8198C_freeMCastv6Entry(mCastEntry,index);
					}
					_rtl8198C_arrangeMulticastv6(index);
				}
			}	
				
		}
	}

#else /*CONFIG_RTL_MULTI_LAN_DEV*/
#if defined (CONFIG_RT_MULTIPLE_BR_SUPPORT)
	if(memcmp(mcastEventContext.devName,RTL_BR_NAME,2)==0)
#else
	if(memcmp(mcastEventContext.devName,RTL_BR_NAME,3)==0)
#endif

	{
	#ifdef CONFIG_RTL865X_MUTLICAST_DEBUG
		printk("%s:%d,multicast event from ethernet (%s),mcastEventContext.groupAddr[0] is 0x%x\n",__FUNCTION__,__LINE__,mcastEventContext.devName,mcastEventContext.groupAddr[0]);
	#endif
			
		for (index=0; index< RTL8651_MULTICASTTBL_SIZE; index++)
		{
			for (mCastEntry = TAILQ_FIRST(&mCastTbl6.inuseList.mCastTbl[index]); mCastEntry; mCastEntry = nextMCastEntry)
			{
				nextMCastEntry=TAILQ_NEXT(mCastEntry, nextMCast);
#if	0				
				printk("mCastEntry->dip:%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n",
					mCastEntry->dip.v6_addr16[0],mCastEntry->dip.v6_addr16[1],
					mCastEntry->dip.v6_addr16[2],mCastEntry->dip.v6_addr16[3],
					mCastEntry->dip.v6_addr16[4],mCastEntry->dip.v6_addr16[5],
					mCastEntry->dip.v6_addr16[6],mCastEntry->dip.v6_addr16[7]);
				printk("groupAddr:%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n",
					groupAddr.v6_addr16[0],groupAddr.v6_addr16[1],
					groupAddr.v6_addr16[2],groupAddr.v6_addr16[3],
					groupAddr.v6_addr16[4],groupAddr.v6_addr16[5],
					groupAddr.v6_addr16[6],groupAddr.v6_addr16[7]);
#endif
				if (!is_ip6_addr_equal(mCastEntry->dip,groupAddr))
				{
					continue;
				}
					
				memset(&newFwdDesc, 0 ,sizeof(rtl8198c_mcast_fwd_descriptor6_t));
				strcpy(newFwdDesc.netifName,mcastEventContext.devName);
	
				/*save old multicast entry forward port mask*/
				oldAsicFwdPortMask=mCastEntry->mbr;
				oldCpuFlag=mCastEntry->cpu;
	
				/*sync with control plane*/
				multicastDataInfo.ipVersion=6;
				memcpy(multicastDataInfo.sourceIp,&mCastEntry->sip,sizeof(inv6_addr_t));
				memcpy(multicastDataInfo.groupAddr,&mCastEntry->dip,sizeof(inv6_addr_t));
				retVal= rtl_getMulticastDataFwdInfo(mcastEventContext.moduleIndex, &multicastDataInfo, &multicastFwdInfo);
				if (multicastFwdInfo.unknownMCast == TRUE)
					multicastFwdInfo.cpuFlag = TRUE;

				#if defined CONFIG_RT_MULTIPLE_BR_SUPPORT
				br_dev = NULL;
				for(br_index = 0; br_index<RTL_IMGP_MAX_BRMODULE; br_index++)
				{
					br_dev = rtl_get_brByIndex(br_index, br_name);
					if(strcmp(br_name, mcastEventContext.devName)==0)
						break;
				}
				if(br_dev==NULL)
					continue;
				newFwdDesc.fwdPortMask=rtl865x_getPhyFwdPortMask(br_dev,multicastFwdInfo.fwdPortMask) & (~(1<<mCastEntry->port));
				#else
				newFwdDesc.fwdPortMask=rtl865x_getPhyFwdPortMask(bridge0,multicastFwdInfo.fwdPortMask) & (~(1<<mCastEntry->port));
				#endif 
				newFwdDesc.toCpu=multicastFwdInfo.cpuFlag;
				
				/*update/replace old forward descriptor*/

				_rtl8198C_mergeMCastv6FwdDescChain(&mCastEntry->fwdDescChain,&newFwdDesc);
				mCastEntry->mbr 		= rtl8198C_genMCastv6EntryAsicFwdMask(mCastEntry);
				mCastEntry->cpu 	= rtl8198C_genMCastv6EntryCpuFlag(mCastEntry);
					
				newAsicFwdPortMask	= mCastEntry->mbr ;
				newCpuFlag			=mCastEntry->cpu;
					
			#ifdef CONFIG_RTL865X_MUTLICAST_DEBUG
				printk("%s:%d,oldAsicFwdPortMask is %d,newAsicFwdPortMask is %d\n",__FUNCTION__,__LINE__,oldAsicFwdPortMask,newAsicFwdPortMask);
			#endif
					
#ifndef RTL8651_MCAST_ALWAYS2UPSTREAM
				if (mCastEntry->flag & RTL865X_MULTICAST_UPLOADONLY)
				{	/* remove upload term*/
					if((newAsicFwdPortMask!=0) && (newAsicFwdPortMask!=oldAsicFwdPortMask))
					{
						mCastEntry->flag &= ~RTL865X_MULTICAST_UPLOADONLY;
							/* we assume multicast member will NEVER in External interface, so we remove
								 external ip now */
						mCastEntry->flag &= ~RTL865X_MULTICAST_EXTIP_SET;
					}
				}
#endif /* RTL8651_MCAST_ALWAYS2UPSTREAM */
#ifdef CONFIG_RTL_MLD_SNOOPING	
				rtl_getGroupInfov6((uint32*)&mCastEntry->dip, &groupInfo);
				if(groupInfo.ownerMask==0)
				{
					mCastEntry->unKnownMCast=TRUE;
				}
				else
				{
					mCastEntry->unKnownMCast=FALSE;
				}
#endif	

#if defined CONFIG_RTL_MULTICAST_PORT_MAPPING
				newAsicFwdPortMask &= (mCastEntry->mapPortMbr |(0x01<<6));
#endif

				if((oldCpuFlag != newCpuFlag)||(newAsicFwdPortMask!=oldAsicFwdPortMask)) 
				{
					_rtl8198C_patchPppoeWeakv6(mCastEntry);
						
					/*reset inAsic flag to re-select or re-write this hardware asic entry*/
					if(newAsicFwdPortMask==0)
					{
						_rtl8198C_freeMCastv6Entry(mCastEntry,index);
					}
					_rtl8198C_arrangeMulticastv6(index);
				}
			}	
				
		}
	}
#endif
#endif
	return EVENT_CONTINUE_EXECUTE;
}

static int32 _rtl8198C_multicastv6UnRegisterEvent(void)
{
	rtl865x_event_Param_t eventParam;
	eventParam.eventLayerId=DEFAULT_LAYER3_EVENT_LIST_ID;
	eventParam.eventId=EVENT_UPDATE_MCAST6;
	eventParam.eventPriority=0;
	eventParam.event_action_fn=rtl8198C_multicastv6CallbackFn;
	rtl865x_unRegisterEvent(&eventParam);
	return SUCCESS;
}
static int32 _rtl8198C_multicastv6RegisterEvent(void)
{
	rtl865x_event_Param_t eventParam;
	eventParam.eventLayerId=DEFAULT_LAYER3_EVENT_LIST_ID;
	eventParam.eventId=EVENT_UPDATE_MCAST6;
	eventParam.eventPriority=0;
	eventParam.event_action_fn=rtl8198C_multicastv6CallbackFn;
	rtl865x_registerEvent(&eventParam);
	return SUCCESS;
}

#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
uint32 mc_v6_aging = FALSE;
#else
uint32 mc_v6_aging = TRUE;			// for 97G and later series, aging should be enabled
#endif	

int32 rtl8198C_initMulticastv6(void)
{
	_rtl8198C_multicastv6UnRegisterEvent();
	_rtl8198C_initMCastv6EntryPool();
	_rtl8198C_initMCastv6FwdDescPool();
#if defined(CONFIG_RTL_HARDWARE_MULTICAST_API)
	rtl_initMldKernelMCast();
#endif
	#if defined(__linux__) && defined(__KERNEL__)
	_rtl8198C_initMCastv6SysTimer();
	#endif
	_rtl8198C_multicastv6RegisterEvent();
	rtl8198C_setAsicMulticastv6MTU(1522);
	rtl8198C_setAsicMulticastv6Enable(TRUE);
	rtl8198C_setAsicMulticastv6Aging(mc_v6_aging);
	return SUCCESS;
}

int32 rtl8198C_reinitMulticastv6(void)
{
	inv6_addr_t ip6addr0;
	_rtl8198C_multicastv6UnRegisterEvent();
	/*delete all multicast entry*/
	rtl8198C_setAsicMulticastv6Enable(FALSE);
	memset(&ip6addr0,0,sizeof(inv6_addr_t));
	rtl8198C_delMulticastv6Entry(ip6addr0);
	#if defined(__linux__) && defined(__KERNEL__)
	_rtl8198C_destroyMCastv6SysTimer();
	_rtl8198C_initMCastv6SysTimer();
	#endif
	
	/*regfster twice won't cause any side-effect, 
	because event management module will handle duplicate event issue*/

	rtl8198C_setAsicMulticastv6Enable(TRUE);
	rtl8198C_setAsicMulticastv6Aging(mc_v6_aging);

	_rtl8198C_multicastv6RegisterEvent();
	return SUCCESS;
}	

int32 rtl8198C_addMulticastv6ExternalPort(uint32 extPort)
{
	rtl8198c_externalMulticastPortMask6 |= (1<<extPort);
	_rtl8198C_setASICMulticastv6PortStatus();
	return SUCCESS;
}

int32 rtl8198C_delMulticastv6ExternalPort(uint32 extPort)
{
	rtl8198c_externalMulticastPortMask6 &= ~(1<<extPort);
	_rtl8198C_setASICMulticastv6PortStatus();
	return SUCCESS;
}

int32 rtl8198C_setMulticastv6ExternalPortMask(uint32 extPortMask)
{
	rtl8198c_externalMulticastPortMask6 =extPortMask;
	_rtl8198C_setASICMulticastv6PortStatus();
	return SUCCESS;
}

int32 rtl8198C_addMulticastv6ExternalPortMask(uint32 extPortMask)
{
	rtl8198c_externalMulticastPortMask6|= extPortMask;
	_rtl8198C_setASICMulticastv6PortStatus();
	return SUCCESS;
}

int32 rtl8198C_delMulticastv6ExternalPortMask(uint32 extPortMask)
{
	rtl8198c_externalMulticastPortMask6 &= ~extPortMask;
	_rtl8198C_setASICMulticastv6PortStatus();
	return SUCCESS;
}

int32 rtl8198C_getMulticastv6ExternalPortMask(void)
{
	return rtl8198c_externalMulticastPortMask6 ;
}

rtl8198c_tblDrv_mCastv6_t *rtl8198C_findMCastv6Entry(inv6_addr_t dip,inv6_addr_t sip, uint16 svid, uint16 sport)
{
	rtl8198c_tblDrv_mCastv6_t *mCast_t;
	uint32 entry=0;
	uint32 hashMethod=0;
	rtl8198C_getMCastv6HashMethod(&hashMethod);
	entry = rtl8198C_ipMulticastv6TableIndex(hashMethod,sip,dip);
	TAILQ_FOREACH(mCast_t, &mCastTbl6.inuseList.mCastTbl[entry], nextMCast) {
		if (is_ip6_addr_equal(mCast_t->dip,dip) && is_ip6_addr_equal(mCast_t->sip,sip) && mCast_t->port==sport)
		{
			if (mCast_t->inAsic == FALSE) 
			{
				mCast_t->age = RTL865X_MULTICAST_TABLE_AGE;
				mCast_t->count ++;
			}
			return mCast_t;
		}
	}
#if defined (CONFIG_RTL_HARDWARE_MULTICAST_CAM)
	for(entry=RTL8651_IPMULTICASTTBL_SIZE; entry<RTL8651_MULTICASTTBL_SIZE; entry++)
	{
		TAILQ_FOREACH(mCast_t, &mCastTbl6.inuseList.mCastTbl[entry], nextMCast) {
			if (is_ip6_addr_equal(mCast_t->dip,dip) && is_ip6_addr_equal(mCast_t->sip,sip) && mCast_t->port==sport)
			{
				if (mCast_t->inAsic == FALSE) 
				{
					mCast_t->age = RTL865X_MULTICAST_TABLE_AGE;
					mCast_t->count ++;
				}
				return mCast_t;
			}
					
		}
	}
#endif	
	return (rtl8198c_tblDrv_mCastv6_t *)NULL;	
}

#if defined CONFIG_RTL_MULTICAST_PORT_MAPPING
int rtl8198C_addMulticastv6Entry(inv6_addr_t dip,inv6_addr_t sip, unsigned short svid, unsigned short sport, 
									rtl8198c_mcast_fwd_descriptor6_t * newFwdDescChain, 
									int flushOldChain, unsigned int extIp, char cpuHold, unsigned char flag, unsigned int mapPortMask)

#else
int rtl8198C_addMulticastv6Entry(inv6_addr_t dip,inv6_addr_t sip, unsigned short svid, unsigned short sport, 
									rtl8198c_mcast_fwd_descriptor6_t * newFwdDescChain, 
									int flushOldChain, unsigned int extIp, char cpuHold, unsigned char flag)
#endif								
{
	rtl8198c_tblDrv_mCastv6_t *mCast_t;
	uint32 hashIndex;
#ifdef CONFIG_RTL_MLD_SNOOPING
	uint32 groupIpAddr[4];
#endif
	#if defined (CONFIG_RTL_HARDWARE_MULTICAST_CAM)
	uint32 emptyCamIndex=-1; 
	#endif
	uint32 hashMethod=0;
	#if defined (CONFIG_RTL_MLD_SNOOPING)
	struct rtl_groupInfo groupInfo;
	#endif
#if defined(CONFIG_RTL_HARDWARE_MULTICAST_API)&&defined(CONFIG_RTL_8197F)
	uint32 groupAddr[4] = {0};
	int32 wanOps, wanPPP;
	groupAddr[0] = dip.v6_addr32[0];
	groupAddr[1] = dip.v6_addr32[1];
	groupAddr[2] = dip.v6_addr32[2];
	groupAddr[3] = dip.v6_addr32[3];
	if(SUCCESS!=rtl_getKernelMCastWanType(groupAddr,IP_VERSION6,&wanOps,&wanPPP))
		return FAILED;
#endif

	rtl8198C_getMCastv6HashMethod(&hashMethod);
	hashIndex = rtl8198C_ipMulticastv6TableIndex(hashMethod,sip,dip);
#if defined (CONFIG_RTL_HARDWARE_MULTICAST_CAM)
	mCast_t=rtl8198C_findMCastv6Entry(dip, sip, svid, sport);
	if(mCast_t==NULL)
	{
		/*table entry collided*/
		if(!TAILQ_EMPTY(&mCastTbl6.inuseList.mCastTbl[hashIndex]))
		{
			emptyCamIndex=rtl8198C_findEmptyCamEntryv6();
			if(emptyCamIndex!=-1)
			{
				hashIndex=emptyCamIndex;
			}
		}
	}
	else
	{
		hashIndex=mCast_t->hashIndex;
	}
#else
	/*try to match hash line*/
	TAILQ_FOREACH(mCast_t, &mCastTbl6.inuseList.mCastTbl[hashIndex], nextMCast) 
	{
		if (is_ip6_addr_equal(mCast_t->sip,sip) && is_ip6_addr_equal(mCast_t->dip,dip) && mCast_t->port==sport)
			break;
	}
#endif	
	
	if (mCast_t == NULL) 
	{
		mCast_t=_rtl8198C_allocMCastv6Entry(hashIndex);
		if (mCast_t == NULL)
		{
			_rtl8198C_mCastv6EntryReclaim();
			mCast_t=_rtl8198C_allocMCastv6Entry(hashIndex);
			if(mCast_t == NULL)
			{
				return FAILED;
			}
		}
		mCast_t->sip		= sip;
		mCast_t->dip		= dip;
		mCast_t->port		= sport;
		mCast_t->mbr		= 0;
		mCast_t->count		= 0;
		mCast_t->inAsic		= FALSE;
		mCast_t->six_rd_eg  = 0;
		mCast_t->six_rd_idx = 0;
		mCast_t->svid		= svid;	
#if defined CONFIG_RTL_MULTICAST_PORT_MAPPING
		mCast_t->mapPortMbr = mapPortMask;
#endif
	}
	
	if(flushOldChain)
	{
		_rtl8198C_flushMCastv6FwdDescChain(&mCast_t->fwdDescChain);
		
	}
	
	_rtl8198C_mergeMCastv6FwdDescChain(&mCast_t->fwdDescChain,newFwdDescChain);
	_rtl8198C_doMCastv6EntrySrcVlanPortFilter(mCast_t);
	
	mCast_t->mbr			= rtl8198C_genMCastv6EntryAsicFwdMask(mCast_t);

	mCast_t->age			= RTL865X_MULTICAST_TABLE_AGE;
#if 0
	mCast_t->cpu			= (toCpu==TRUE? 1: 0);
#else
	mCast_t->cpuHold			= cpuHold;
	mCast_t->cpu 			= rtl8198C_genMCastv6EntryCpuFlag(mCast_t);
#endif	
	mCast_t->flag			= flag;

	
	if (extIp)
		mCast_t->flag |= RTL865X_MULTICAST_EXTIP_SET;
	else
		mCast_t->flag &= ~RTL865X_MULTICAST_EXTIP_SET;
	#if defined (CONFIG_RTL_MLD_SNOOPING)
	memcpy(groupIpAddr,&dip,sizeof(inv6_addr_t));
	rtl_getGroupInfov6(groupIpAddr, &groupInfo);
	if(groupInfo.ownerMask==0)
	{
		mCast_t->unKnownMCast=TRUE;
	}
	else
	{
		mCast_t->unKnownMCast=FALSE;
	}
	#endif
#if defined (CONFIG_RTL_HARDWARE_MULTICAST_CAM)
	mCast_t->hashIndex=hashIndex;
#endif

#if defined(CONFIG_RTL_8197F)
#if defined(CONFIG_RTL_HARDWARE_MULTICAST_API)
	mCast_t->destInterface = rtl_getKernelMCastDifbyWanOps(wanOps);
#else
	mCast_t->destInterface = 0;
#endif
#endif

	_rtl8198C_patchPppoeWeakv6(mCast_t);
	_rtl8198C_arrangeMulticastv6(hashIndex);
	return SUCCESS;	
}
int rtl8198C_delMulticastv6Entry(inv6_addr_t groupAddr)
{
	rtl8198c_tblDrv_mCastv6_t *mCastEntry, *nextMCastEntry;
	uint32 entry;
	uint32 deleteFlag=FALSE;
	for(entry=0; entry<RTL8651_MULTICASTTBL_SIZE; entry++) 
	{
		deleteFlag=FALSE;
		mCastEntry = TAILQ_FIRST(&mCastTbl6.inuseList.mCastTbl[entry]);
		while (mCastEntry)
		{
			nextMCastEntry = TAILQ_NEXT(mCastEntry, nextMCast);
			if (is_ip6_addr_equal(groupAddr,mCastEntry->dip)) 
			{
				deleteFlag=TRUE;
				_rtl8198C_freeMCastv6Entry(mCastEntry, entry);
			}
			
			mCastEntry = nextMCastEntry;
		}
		
		if(deleteFlag==TRUE)
		{
			_rtl8198C_arrangeMulticastv6(entry);
		}
	}
	return SUCCESS;
}
int rtl8198C_genVirtualMCastv6FwdDescriptor(unsigned int forceToCpu, unsigned int  fwdPortMask, rtl8198c_mcast_fwd_descriptor6_t *fwdDescriptor)
{
	if(fwdDescriptor==NULL)
	{
		return FAILED;
	}
	memset(fwdDescriptor, 0, sizeof(rtl8198c_mcast_fwd_descriptor6_t ));
	fwdDescriptor->toCpu=forceToCpu;
	fwdDescriptor->fwdPortMask=fwdPortMask;
	return SUCCESS;
}
#if defined CONFIG_RTL_MULTICAST_PORT_MAPPING
int rtl8198C_blockMulticastv6Flow(unsigned int srcVlanId,unsigned int srcPort,inv6_addr_t srcIpAddr,inv6_addr_t destIpAddr, unsigned int mapPortMask)
#else
int rtl8198C_blockMulticastv6Flow(unsigned int srcVlanId,unsigned int srcPort,inv6_addr_t srcIpAddr,inv6_addr_t destIpAddr)
#endif
{
	rtl8198c_mcast_fwd_descriptor6_t fwdDescriptor;
	rtl8198c_tblDrv_mCastv6_t * existMCastEntry=NULL;
	existMCastEntry=rtl8198C_findMCastv6Entry(destIpAddr, srcIpAddr, (uint16)srcVlanId, (uint16)srcPort);
	if(existMCastEntry!=NULL)
	{
		if(existMCastEntry->mbr==0)
		{
			return SUCCESS;
		}
	}
	memset(&fwdDescriptor, 0, sizeof(rtl8198c_mcast_fwd_descriptor6_t ));
#if defined CONFIG_RTL_MULTICAST_PORT_MAPPING
	rtl8198C_addMulticastv6Entry(destIpAddr,srcIpAddr,(unsigned short)srcVlanId,(unsigned short)srcPort,&fwdDescriptor,TRUE,0,0,0, mapPortMask);
#else
	rtl8198C_addMulticastv6Entry(destIpAddr,srcIpAddr,(unsigned short)srcVlanId,(unsigned short)srcPort,&fwdDescriptor,TRUE,0,0,0);
#endif
	return SUCCESS;
}

int32 rtl8198C_flushHWMulticastv6Entry(void)
{
	rtl8198c_tblDrv_mCastv6_t *mCastEntry, *nextMCastEntry;
	uint32 entry;
	inv6_addr_t ip0;
	memset(&ip0,0,sizeof(inv6_addr_t));
	for(entry=0; entry<RTL8651_MULTICASTTBL_SIZE; entry++) 
	{
		mCastEntry = TAILQ_FIRST(&mCastTbl6.inuseList.mCastTbl[entry]);
		while (mCastEntry)
		{
			nextMCastEntry = TAILQ_NEXT(mCastEntry, nextMCast);
			if(!is_ip6_addr_equal(ip0,mCastEntry->dip))
			{
				_rtl8198C_freeMCastv6Entry(mCastEntry, entry);
				_rtl8198C_arrangeMulticastv6(entry);
			}
			mCastEntry = nextMCastEntry;
		}	
	}
	return SUCCESS;
}
int rtl8198C_getMCastv6HashMethod(unsigned int *hashMethod)
{
	return rtl8198C_getAsicMCastv6HashMethod(hashMethod);
}
int rtl8198C_setMCastv6HashMethod(unsigned int hashMethod)
{
	uint32	  oldHashMethod = 0;
	rtl8198C_getAsicMCastv6HashMethod(&oldHashMethod);
	if(hashMethod >3)
	{
		return -1;
	}
	hashMethod &= 3;
					
	if (oldHashMethod != hashMethod) /* set IPV6CR Register bit 17~18 and flush multicastv6 table */
	{
		rtl8198C_setAsicMCastv6HashMethod(hashMethod);
		/* exclude 0->1 and 1->0 */
		if(!((hashMethod == HASH_METHOD_SIP_DIP0 && oldHashMethod == HASH_METHOD_SIP_DIP1) || 
			(hashMethod == HASH_METHOD_SIP_DIP1 && oldHashMethod == HASH_METHOD_SIP_DIP0)))
		{
			rtl8198C_flushHWMulticastv6Entry();   	
		}
				
	}
	return 0;
}

#endif

#if defined(CONFIG_RTL_HARDWARE_MULTICAST_API)
//extern int net_ratelimit(void);
static int32 _rtl865x_initIgmpKernelGroupEntryPool(void)
{
	int32 i;

	MC_LIST_INIT(&rtl_igmp_free_kernelGroupEntry_head);
	MC_LIST_INIT(&rtl_igmp_kernelGroupEntry_list);

	TBL_MEM_ALLOC(rtl_igmp_kernelGroupEntryPool, rtl_kernelGroupEntry_t, MAX_KERNEL_IGMP_GROUP_ENTRY_CNT);
	
	if(rtl_igmp_kernelGroupEntryPool!=NULL)
	{
		memset(rtl_igmp_kernelGroupEntryPool, 0, MAX_KERNEL_IGMP_GROUP_ENTRY_CNT * sizeof(rtl_kernelGroupEntry_t));	
	}
	else
	{
		return FAILED;
	}
	
	for(i = 0; i<MAX_KERNEL_IGMP_GROUP_ENTRY_CNT;i++)
	{
		MC_LIST_INSERT_HEAD(&rtl_igmp_free_kernelGroupEntry_head, &rtl_igmp_kernelGroupEntryPool[i], next);
	}

	return SUCCESS;
}
static int32 _rtl865x_initMldKernelGroupEntryPool(void)
{
	int32 i;

	MC_LIST_INIT(&rtl_mld_free_kernelGroupEntry_head);
	MC_LIST_INIT(&rtl_mld_kernelGroupEntry_list);

	TBL_MEM_ALLOC(rtl_mld_kernelGroupEntryPool, rtl_kernelGroupEntry_t, MAX_KERNEL_MLD_GROUP_ENTRY_CNT);
	
	if(rtl_mld_kernelGroupEntryPool!=NULL)
	{
		memset(rtl_mld_kernelGroupEntryPool, 0, MAX_KERNEL_MLD_GROUP_ENTRY_CNT * sizeof(rtl_kernelGroupEntry_t));	
	}
	else
	{
		return FAILED;
	}
	
	for(i = 0; i<MAX_KERNEL_MLD_GROUP_ENTRY_CNT;i++)
	{
		MC_LIST_INSERT_HEAD(&rtl_mld_free_kernelGroupEntry_head, &rtl_mld_kernelGroupEntryPool[i], next);
	}
	return SUCCESS;
}

static rtl_kernelGroupEntry_t *_rtl865x_allocKernelGroupEntry(rtl_kernelGroupEntry_head_t *freeEntryList)
{
	rtl_kernelGroupEntry_t *retEntry=NULL;

	if(freeEntryList==NULL)
		return NULL;

	retEntry = MC_LIST_FIRST(freeEntryList);
	if(retEntry!=NULL)
	{
		MC_LIST_REMOVE(retEntry, next);
		memset(retEntry,0,sizeof(rtl_kernelGroupEntry_t));
	}
	return retEntry;
}

static int32 _rtl865x_freeKernelGroupEntry(rtl_kernelGroupEntry_head_t *freeEntryList, rtl_kernelGroupEntry_t *entryPtr)
{
	if((entryPtr==NULL)||(freeEntryList==NULL))
	{
		return SUCCESS;
	}

	memset(entryPtr,0,sizeof(rtl_kernelGroupEntry_t));
	MC_LIST_INSERT_HEAD(freeEntryList, entryPtr, next);
	
	return SUCCESS;
}

static rtl_kernelGroupEntry_t * rtl_kernelGroupEntrySearch(uint32 *groupAddr, uint32 ipVersion)
{
	rtl_kernelGroupEntry_t *curEntry,*nextEntry;

	if(groupAddr==NULL)
		return NULL;

	if(ipVersion==IP_VERSION4)
	{
		curEntry=MC_LIST_FIRST(&rtl_igmp_kernelGroupEntry_list);
		while(curEntry)
		{
			nextEntry = MC_LIST_NEXT(curEntry, next);

			if(curEntry->groupAddr[0]==groupAddr[0])
				return curEntry;

			curEntry = nextEntry;
		}
	}
	else if(ipVersion==IP_VERSION6)
	{
		curEntry=MC_LIST_FIRST(&rtl_mld_kernelGroupEntry_list);
		
		while(curEntry)
		{
			nextEntry=MC_LIST_NEXT(curEntry, next);

			if((curEntry->groupAddr[0]==groupAddr[0])&&
			   (curEntry->groupAddr[1]==groupAddr[1])&&
			   (curEntry->groupAddr[2]==groupAddr[2])&&
			   (curEntry->groupAddr[3]==groupAddr[3]))
				return curEntry;

			curEntry = nextEntry;
		}
	}
	
	return NULL;
}

static rtl_kernelGroupEntry_t *rtl_kernelGroupEntryEnqueue(rtl_kernelGroupEntry_t* enqueueEntry, uint32 ipVersion)
{
	rtl_kernelGroupEntry_t *newEntry;
	
	if(enqueueEntry==NULL)
	{
		return NULL;
	}

	if((ipVersion!=IP_VERSION4)&&(ipVersion!=IP_VERSION6))
	{
		return NULL;
	}
	
	if(ipVersion==IP_VERSION4)
	{
		newEntry = _rtl865x_allocKernelGroupEntry(&rtl_igmp_free_kernelGroupEntry_head);
	}
	else 
	{
		newEntry = _rtl865x_allocKernelGroupEntry(&rtl_mld_free_kernelGroupEntry_head);
	}
	
	if(newEntry!=NULL)
	{
		memcpy(newEntry, enqueueEntry, sizeof(rtl_kernelGroupEntry_t));
		//memset(&(newDesc->next), 0, sizeof(MC_LIST_ENTRY(rtl865x_mcast_fwd_descriptor_s)));
		newEntry->next.le_next=NULL;
		newEntry->next.le_prev=NULL;
	}
	else
	{
		/*no enough memory*/
		return NULL;
	}
	
	if(ipVersion==IP_VERSION4)
	{
		MC_LIST_INSERT_HEAD(&rtl_igmp_kernelGroupEntry_list, newEntry, next);
	}
	else
	{
		MC_LIST_INSERT_HEAD(&rtl_mld_kernelGroupEntry_list, newEntry, next);
	}

	return newEntry;
}
static int32 rtl_kernelGroupEntryDequeue(rtl_kernelGroupEntry_t* dequeueEntry, uint32 ipVersion)
{
	if(dequeueEntry==NULL)
	{
		return SUCCESS;
	}
	
	if((ipVersion!=IP_VERSION4)&&(ipVersion!=IP_VERSION6))
	{
		return FAILED;
	}

	
	MC_LIST_REMOVE(dequeueEntry, next);
	if(ipVersion==IP_VERSION4)
	{
		_rtl865x_freeKernelGroupEntry(&rtl_igmp_free_kernelGroupEntry_head, dequeueEntry);
	}
	else
	{
		_rtl865x_freeKernelGroupEntry(&rtl_mld_free_kernelGroupEntry_head, dequeueEntry);
	}

	return SUCCESS;

}
#if 0//complier error: not used
static int32 rtl_kernelGroupEntryFlush(uint32 ipVersion)
{
	rtl_kernelGroupEntry_t *curEntry,*nextEntry;
	rtl_kernelGroupEntry_head_t *entryChainHead, *freeChainHead;

	if((ipVersion!=IP_VERSION4)&&(ipVersion!=IP_VERSION6))
	{
		return FAILED;
	}

	if(ipVersion==IP_VERSION4)
	{
		entryChainHead = &rtl_igmp_kernelGroupEntry_list;
		freeChainHead = &rtl_igmp_free_kernelGroupEntry_head;
	}
	else
	{
		entryChainHead = &rtl_mld_kernelGroupEntry_list;
		freeChainHead = &rtl_mld_free_kernelGroupEntry_head;
	}
	
	
	curEntry=MC_LIST_FIRST(entryChainHead);
	while(curEntry)
	{
		nextEntry=MC_LIST_NEXT(curEntry, next);
		/*remove from the old descriptor chain*/
		MC_LIST_REMOVE(curEntry, next);
		/*return to the free descriptor chain*/
		_rtl865x_freeKernelGroupEntry(freeChainHead, curEntry);
		curEntry = nextEntry;
	}

	return SUCCESS;
}
#endif

static int rtl_kernelGroupEntryFreeCnt(uint32 ipVersion)
{
	int i = 0;
	rtl_kernelGroupEntry_head_t *freeEntryList = NULL;
	rtl_kernelGroupEntry_t *curEntry=NULL, *nextEntry=NULL;

	if((ipVersion!=IP_VERSION4) &&(ipVersion!=IP_VERSION6))
		return 0;

	if(ipVersion==IP_VERSION4)
		freeEntryList = &rtl_igmp_free_kernelGroupEntry_head;
	else
		freeEntryList = &rtl_mld_free_kernelGroupEntry_head;

	curEntry = MC_LIST_FIRST(freeEntryList);
	while(curEntry)
	{
		nextEntry = MC_LIST_NEXT(curEntry, next);
		i++;
		curEntry = nextEntry;
	}
	return i;
}
static int32 _rtl865x_initIgmpKernelPortEntryPool(void)
{
	int32 i;

	MC_LIST_INIT(&rtl_igmp_free_kernelPortEntry_head);

	TBL_MEM_ALLOC(rtl_igmp_kernelPortEntryPool, rtl_kernelPortEntry_t, MAX_KERNEL_IGMP_PORT_ENTRY_CNT);
	
	if(rtl_igmp_kernelPortEntryPool!=NULL)
	{
		memset(rtl_igmp_kernelPortEntryPool, 0, MAX_KERNEL_IGMP_PORT_ENTRY_CNT * sizeof(rtl_kernelPortEntry_t));	
	}
	else
	{
		return FAILED;
	}
	
	for(i = 0; i<MAX_KERNEL_IGMP_PORT_ENTRY_CNT;i++)
	{
		MC_LIST_INSERT_HEAD(&rtl_igmp_free_kernelPortEntry_head, &rtl_igmp_kernelPortEntryPool[i], next);
	}

	return SUCCESS;
}
static int32 _rtl865x_initMldKernelPortEntryPool(void)
{
	int32 i;

	MC_LIST_INIT(&rtl_mld_free_kernelPortEntry_head);

	TBL_MEM_ALLOC(rtl_mld_kernelPortEntryPool, rtl_kernelPortEntry_t, MAX_KERNEL_MLD_PORT_ENTRY_CNT);
	
	if(rtl_mld_kernelPortEntryPool!=NULL)
	{
		memset(rtl_mld_kernelPortEntryPool, 0, MAX_KERNEL_MLD_PORT_ENTRY_CNT * sizeof(rtl_kernelPortEntry_t));	
	}
	else
	{
		return FAILED;
	}
	
	for(i = 0; i<MAX_KERNEL_MLD_PORT_ENTRY_CNT;i++)
	{
		MC_LIST_INSERT_HEAD(&rtl_mld_free_kernelPortEntry_head, &rtl_mld_kernelPortEntryPool[i], next);
	}

	return SUCCESS;
}


static rtl_kernelPortEntry_t *_rtl865x_allocKernelPortEntry(rtl_kernelPortEntry_head_t *freeEntryList)
{
	rtl_kernelPortEntry_t *retEntry=NULL;
	if(freeEntryList==NULL)
		return NULL;
	
	retEntry = MC_LIST_FIRST(freeEntryList);
	if(retEntry!=NULL)
	{
		MC_LIST_REMOVE(retEntry, next);
		memset(retEntry,0,sizeof(rtl_kernelPortEntry_t));
	}
	return retEntry;
}

static int32 _rtl865x_freeKernelPortEntry(rtl_kernelPortEntry_head_t *freeEntryList, rtl_kernelPortEntry_t *entryPtr)
{
	if((entryPtr==NULL)||(freeEntryList==NULL))
	{
		return SUCCESS;
	}
	
	memset(entryPtr,0,sizeof(rtl_kernelPortEntry_t));
	MC_LIST_INSERT_HEAD(freeEntryList, entryPtr, next);
	
	return SUCCESS;
}
static rtl_kernelPortEntry_t * rtl_kernelPortEntrySearch(rtl_kernelPortEntry_head_t *searchList, uint32 portNum)
{
	rtl_kernelPortEntry_t *curEntry,*nextEntry;
	if(searchList==NULL)
	{
		return NULL;
	}

	curEntry=MC_LIST_FIRST(searchList);
	while(curEntry)
	{
		nextEntry = MC_LIST_NEXT(curEntry, next);

		if(curEntry->portNum==portNum)
			return curEntry;

		curEntry = nextEntry;
	}	
	return NULL;
}

static rtl_kernelPortEntry_t * rtl_kernelPortEntryEnqueue(rtl_kernelPortEntry_head_t *entryList, rtl_kernelPortEntry_t* enqueueEntry, uint32 ipVersion, int32 *portCnt)
{
	rtl_kernelPortEntry_t *newEntry;
	
	if(enqueueEntry==NULL)
	{
		return NULL;
	}

	if((entryList==NULL)||((ipVersion!=IP_VERSION4)&&(ipVersion!=IP_VERSION6)))
	{
		return NULL;
	}
	
	if(ipVersion==IP_VERSION4)
	{
		newEntry = _rtl865x_allocKernelPortEntry(&rtl_igmp_free_kernelPortEntry_head);
	}
	else 
	{
		newEntry = _rtl865x_allocKernelPortEntry(&rtl_mld_free_kernelPortEntry_head);
	}
	
	if(newEntry!=NULL)
	{
		memcpy(newEntry, enqueueEntry, sizeof(rtl_kernelPortEntry_t));
		newEntry->next.le_next=NULL;
		newEntry->next.le_prev=NULL;
	}
	else
	{
		/*no enough memory*/
		return NULL;
	}
	
	MC_LIST_INSERT_HEAD(entryList, newEntry, next);
	*portCnt = *portCnt+1;
	
	return newEntry;
}
static int32 rtl_kernelPortEntryDequeue(rtl_kernelPortEntry_t* dequeueEntry, uint32 ipVersion, int32 *entryCnt)
{
	if(dequeueEntry==NULL)
	{
		return SUCCESS;
	}
	
	if((ipVersion!=IP_VERSION4)&&(ipVersion!=IP_VERSION6))
	{
		return FAILED;
	}

	
	MC_LIST_REMOVE(dequeueEntry, next);
	if(ipVersion==IP_VERSION4)
	{
		_rtl865x_freeKernelPortEntry(&rtl_igmp_free_kernelPortEntry_head, dequeueEntry);
	}
	else
	{
		_rtl865x_freeKernelPortEntry(&rtl_mld_free_kernelPortEntry_head, dequeueEntry);
	}

	*entryCnt = *entryCnt-1;

	return SUCCESS;

}
#if 0//complier error: not used
static int32 rtl_kernelPortEntryFlush(rtl_kernelPortEntry_head_t *entryList, uint32 ipVersion, int32 *entryCnt)
{
	rtl_kernelPortEntry_t *curEntry,*nextEntry;
	rtl_kernelPortEntry_head_t *freeChainHead;

	if((entryList==NULL)||((ipVersion!=IP_VERSION4)&&(ipVersion!=IP_VERSION6)))
	{
		return FAILED;
	}

	if(ipVersion==IP_VERSION4)
	{
		freeChainHead = &rtl_igmp_free_kernelPortEntry_head;
	}
	else
	{
		freeChainHead = &rtl_mld_free_kernelPortEntry_head;
	}
	
	
	curEntry=MC_LIST_FIRST(entryList);
	while(curEntry)
	{
		nextEntry=MC_LIST_NEXT(curEntry, next);
		/*remove from the old descriptor chain*/
		MC_LIST_REMOVE(curEntry, next);
		/*return to the free descriptor chain*/
		_rtl865x_freeKernelPortEntry(freeChainHead, curEntry);
		curEntry = nextEntry;
	}

	*entryCnt = 0;

	return SUCCESS;
}
#endif
static int rtl_kernelPortEntryFreeCnt(uint32 ipVersion)
{
	int i = 0;
	rtl_kernelPortEntry_head_t *freeEntryList = NULL;
	rtl_kernelPortEntry_t *curEntry=NULL, *nextEntry=NULL;

	if((ipVersion!=IP_VERSION4) &&(ipVersion!=IP_VERSION6))
		return 0;

	if(ipVersion==IP_VERSION4)
		freeEntryList = &rtl_igmp_free_kernelPortEntry_head;
	else
		freeEntryList = &rtl_mld_free_kernelPortEntry_head;

	curEntry = MC_LIST_FIRST(freeEntryList);
	while(curEntry)
	{
		nextEntry = MC_LIST_NEXT(curEntry, next);
		i++;
		curEntry = nextEntry;
	}
	return i;
}

static int32 _rtl865x_initIgmpKernelSourceEntryPool(void)
{
	int32 i;

	MC_LIST_INIT(&rtl_igmp_free_kernelSrcEntry_head);

	TBL_MEM_ALLOC(rtl_igmp_kernelSrcEntryPool, rtl_kernelSourceEntry_t, MAX_KERNEL_IGMP_SOURCE_ENTRY_CNT);
	
	if(rtl_igmp_kernelSrcEntryPool!=NULL)
	{
		memset(rtl_igmp_kernelSrcEntryPool, 0, MAX_KERNEL_IGMP_SOURCE_ENTRY_CNT * sizeof(rtl_kernelSourceEntry_t));	
	}
	else
	{
		return FAILED;
	}
	
	for(i = 0; i<MAX_KERNEL_IGMP_SOURCE_ENTRY_CNT;i++)
	{
		MC_LIST_INSERT_HEAD(&rtl_igmp_free_kernelSrcEntry_head, &rtl_igmp_kernelSrcEntryPool[i], next);
	}

	return SUCCESS;
}
static int32 _rtl865x_initMldKernelSourceEntryPool(void)
{
	int32 i;

	MC_LIST_INIT(&rtl_mld_free_kernelSrcEntry_head);

	TBL_MEM_ALLOC(rtl_mld_kernelSrcEntryPool, rtl_kernelSourceEntry_t, MAX_KERNEL_MLD_SOURCE_ENTRY_CNT);
	
	if(rtl_mld_kernelSrcEntryPool!=NULL)
	{
		memset(rtl_mld_kernelSrcEntryPool, 0, MAX_KERNEL_MLD_SOURCE_ENTRY_CNT * sizeof(rtl_kernelSourceEntry_t));	
	}
	else
	{
		return FAILED;
	}
	
	for(i = 0; i<MAX_KERNEL_MLD_SOURCE_ENTRY_CNT;i++)
	{
		MC_LIST_INSERT_HEAD(&rtl_mld_free_kernelSrcEntry_head, &rtl_mld_kernelSrcEntryPool[i], next);
	}

	return SUCCESS;
}


static rtl_kernelSourceEntry_t *_rtl865x_allocKernelSourceEntry(rtl_kernelSourceEntry_head_t *freeEntryList)
{
	rtl_kernelSourceEntry_t *retEntry=NULL;

	if(freeEntryList==NULL)
		return NULL;
	
	retEntry = MC_LIST_FIRST(freeEntryList);
	if(retEntry!=NULL)
	{
		MC_LIST_REMOVE(retEntry, next);
		memset(retEntry,0,sizeof(rtl_kernelSourceEntry_t));
	}
	return retEntry;
}

static int32 _rtl865x_freeKernelSourceEntry(rtl_kernelSourceEntry_head_t *freeEntryList, rtl_kernelSourceEntry_t *entryPtr)
{
	if((entryPtr==NULL)||(freeEntryList==NULL))
	{
		return SUCCESS;
	}
	memset(entryPtr,0,sizeof(rtl_kernelSourceEntry_t));
	MC_LIST_INSERT_HEAD(freeEntryList, entryPtr, next);
	
	return SUCCESS;
}
static rtl_kernelSourceEntry_t * rtl_kernelSourceEntrySearch(rtl_kernelSourceEntry_head_t *searchList, uint32 *srcAddr, uint32 ipVersion)
{
	rtl_kernelSourceEntry_t *curEntry,*nextEntry;
	if((searchList==NULL)||(srcAddr==NULL))
	{
		return NULL;
	}

	if((ipVersion!=IP_VERSION4)&&(ipVersion!=IP_VERSION6))
	{
		return NULL;
	}

	curEntry=MC_LIST_FIRST(searchList);

	while(curEntry)
	{
		nextEntry = MC_LIST_NEXT(curEntry, next);

		if(ipVersion==IP_VERSION4)
		{
		   if(curEntry->sourceAddr[0]==srcAddr[0])
			return curEntry;
		}
		else
		{
			if((curEntry->sourceAddr[0]==srcAddr[0])&&
			   (curEntry->sourceAddr[1]==srcAddr[1])&&
			   (curEntry->sourceAddr[2]==srcAddr[2])&&
			   (curEntry->sourceAddr[3]==srcAddr[3]))
			   return curEntry;
		}

		curEntry = nextEntry;
	}	
	return NULL;
}

static rtl_kernelSourceEntry_t * rtl_kernelSourceEntryEnqueue(rtl_kernelSourceEntry_head_t *entryList, rtl_kernelSourceEntry_t* enqueueEntry, uint32 ipVersion, int32 *entryCnt)
{
	rtl_kernelSourceEntry_t *newEntry;
	
	if(enqueueEntry==NULL)
	{
		return NULL;
	}

	if((entryList==NULL)||((ipVersion!=IP_VERSION4)&&(ipVersion!=IP_VERSION6)))
	{
		return NULL;
	}
	
	if(ipVersion==IP_VERSION4)
	{
		newEntry = _rtl865x_allocKernelSourceEntry(&rtl_igmp_free_kernelSrcEntry_head);
	}
	else 
	{
		newEntry = _rtl865x_allocKernelSourceEntry(&rtl_mld_free_kernelSrcEntry_head);
	}
	
	if(newEntry!=NULL)
	{
		memcpy(newEntry, enqueueEntry, sizeof(rtl_kernelSourceEntry_t));
		newEntry->next.le_next=NULL;
		newEntry->next.le_prev=NULL;
	}
	else
	{
		/*no enough memory*/
		return NULL;
	}
	
	MC_LIST_INSERT_HEAD(entryList, newEntry, next);
	*entryCnt = *entryCnt+1;
	
	return newEntry;
}
static int32 rtl_kernelSourceEntryDequeue(rtl_kernelSourceEntry_t* dequeueEntry, uint32 ipVersion, int32 *srcCnt)
{
	if(dequeueEntry==NULL)
	{
		return SUCCESS;
	}
	
	if((ipVersion!=IP_VERSION4)&&(ipVersion!=IP_VERSION6))
	{
		return FAILED;
	}
	
	MC_LIST_REMOVE(dequeueEntry, next);
	if(ipVersion==IP_VERSION4)
	{
		_rtl865x_freeKernelSourceEntry(&rtl_igmp_free_kernelSrcEntry_head, dequeueEntry);
	}
	else
	{
		_rtl865x_freeKernelSourceEntry(&rtl_mld_free_kernelSrcEntry_head, dequeueEntry);
	}
	*srcCnt = *srcCnt-1;

	return SUCCESS;

}
static int32 rtl_kernelSourceEntryFlush(rtl_kernelSourceEntry_head_t *entryList, uint32 ipVersion, int32 *entryCnt)
{
	rtl_kernelSourceEntry_t *curEntry,*nextEntry;
	rtl_kernelSourceEntry_head_t *freeChainHead;

	if((entryList==NULL)||((ipVersion!=IP_VERSION4)&&(ipVersion!=IP_VERSION6)))
	{
		return FAILED;
	}

	if(ipVersion==IP_VERSION4)
	{
		freeChainHead = &rtl_igmp_free_kernelSrcEntry_head;
	}
	else
	{
		freeChainHead = &rtl_mld_free_kernelSrcEntry_head;
	}
	
	
	curEntry=MC_LIST_FIRST(entryList);
	while(curEntry)
	{
		nextEntry=MC_LIST_NEXT(curEntry, next);
		/*remove from the old descriptor chain*/
		MC_LIST_REMOVE(curEntry, next);
		/*return to the free descriptor chain*/
		_rtl865x_freeKernelSourceEntry(freeChainHead, curEntry);
		curEntry = nextEntry;
	}

	*entryCnt = 0;

	return SUCCESS;
}

static int rtl_kernelSourceEntryFreeCnt(uint32 ipVersion)
{
	int i = 0;
	rtl_kernelSourceEntry_head_t *freeEntryList = NULL;
	rtl_kernelSourceEntry_t *curEntry=NULL, *nextEntry=NULL;

	if((ipVersion!=IP_VERSION4) &&(ipVersion!=IP_VERSION6))
		return 0;

	if(ipVersion==IP_VERSION4)
		freeEntryList = &rtl_igmp_free_kernelSrcEntry_head;
	else
		freeEntryList = &rtl_mld_free_kernelSrcEntry_head;

	curEntry = MC_LIST_FIRST(freeEntryList);
	while(curEntry)
	{
		nextEntry = MC_LIST_NEXT(curEntry, next);
		i++;
		curEntry = nextEntry;
	}
	return i;
}
static int32 rtl_initIgmpKernelMCast(void)
{
	_rtl865x_initIgmpKernelGroupEntryPool();
	_rtl865x_initIgmpKernelPortEntryPool();
	_rtl865x_initIgmpKernelSourceEntryPool();
	
#if defined(CONFIG_RTL_8197F)
	rtl_setAsicMCastDifEnable(TRUE);
#endif

	return SUCCESS;
}
static int32 rtl_initMldKernelMCast(void)
{
	_rtl865x_initMldKernelGroupEntryPool();
	_rtl865x_initMldKernelPortEntryPool();
	_rtl865x_initMldKernelSourceEntryPool();

#if defined(CONFIG_RTL_8197F)
	rtl_setAsicMCastV6DifEnable(TRUE);
#endif
	return SUCCESS;
}

static int32 rtl_kernelMCastCallbackFn(void *param)
{
	rtl_multicastEventContext_t mcastEventContext;
	int index;
	uint32 oldAsicFwdPortMask = 0xFFFFFFFF, newAsicFwdPortMask = 0xFFFFFFFF;
	uint32 oldCpuFlag=0, newCpuFlag=0;
	int retVal;
	rtl_kernelMCastDataInfo_t mCastDataInfo;
	rtl_kernelMCastFwdInfo_t mCastFwdInfo;

	if(param==NULL)
	{
		return EVENT_CONTINUE_EXECUTE;
	}
	memcpy(&mcastEventContext,param,sizeof(rtl_multicastEventContext_t));
	#if 0
	printk("[%s:%d], ipVersion:%d, group:%2x%2x%2x%2x, src:%2x%2x%2x%2x\n", 
		__FUNCTION__, __LINE__, mcastEventContext.ipVersion,
		mcastEventContext.groupAddr[0], mcastEventContext.groupAddr[1],mcastEventContext.groupAddr[2],mcastEventContext.groupAddr[3],
		mcastEventContext.sourceAddr[0], mcastEventContext.sourceAddr[1],mcastEventContext.sourceAddr[2],mcastEventContext.sourceAddr[3]);
	#endif


	if(memcmp(mcastEventContext.devName,"eth*",4)==0)
	{
		if(mcastEventContext.ipVersion==IP_VERSION4)
		{
			rtl865x_tblDrv_mCast_t	*mCastEntry,*nextMCastEntry;
			rtl865x_mcast_fwd_descriptor_t newFwdDesc;
			
			for (index=0; index< RTL8651_MULTICASTTBL_SIZE; index++)
			{
				for (mCastEntry = TAILQ_FIRST(&mCastTbl.inuseList.mCastTbl[index]); mCastEntry; mCastEntry = nextMCastEntry)
				{
					nextMCastEntry=TAILQ_NEXT(mCastEntry, nextMCast);
					
					if ((mcastEventContext.groupAddr[0]!=0) && (mCastEntry->dip != mcastEventContext.groupAddr[0]))
					{
						continue;
					}
					
					memset(&newFwdDesc, 0 ,sizeof(rtl865x_mcast_fwd_descriptor_t));
					strcpy(newFwdDesc.netifName,mcastEventContext.devName);
	
					/*save old multicast entry forward port mask*/
					oldAsicFwdPortMask=mCastEntry->mbr;
					oldCpuFlag=mCastEntry->cpu;
	
					memset(&mCastDataInfo, 0, sizeof(rtl_kernelMCastDataInfo_t));
					mCastDataInfo.ipVersion = IP_VERSION4;
					mCastDataInfo.groupAddr[0] = mCastEntry->dip;
					mCastDataInfo.sourceIp[0] = mCastEntry->sip;

					retVal = rtl_getKernelMCastDataFwdInfo(&mCastDataInfo, &mCastFwdInfo);
					if(retVal!=SUCCESS && mCastFwdInfo.unknownMCast!=TRUE)
					{
						continue;
					}
	
					newFwdDesc.fwdPortMask=mCastFwdInfo.fwdPortMask & (~(1<<mCastEntry->port));					
					newFwdDesc.toCpu=mCastFwdInfo.cpuFlag;

					/*update/replace old forward descriptor*/
					_rtl865x_mergeMCastFwdDescChain(&mCastEntry->fwdDescChain,&newFwdDesc);
	
					/*tocpu flag=1, DescFwdMask will be ignored, to cpu flag*/
					mCastEntry->mbr 		= rtl865x_genMCastEntryAsicFwdMaskIgnoreCpuFlag(mCastEntry);
					mCastEntry->cpu 	= rtl865x_genMCastEntryCpuFlag(mCastEntry);
					mCastEntry->unKnownMCast = mCastFwdInfo.unknownMCast;

					newAsicFwdPortMask	= mCastEntry->mbr ;
					newCpuFlag			= mCastEntry->cpu;
						

					if((oldCpuFlag != newCpuFlag)||(newAsicFwdPortMask!=oldAsicFwdPortMask)) 
					{
						_rtl865x_patchPppoeWeak(mCastEntry);
						
						/*reset inAsic flag to re-select or re-write this hardware asic entry*/
						if(newAsicFwdPortMask==0 || newCpuFlag==TRUE)
						{
							_rtl865x_freeMCastEntry(mCastEntry, index);
						}
	
						_rtl865x_arrangeMulticast(index);
					}
				}
			}
		}
	#if defined(CONFIG_RTL_8198C)||defined(CONFIG_RTL_8197F)
		else if(mcastEventContext.ipVersion==IP_VERSION6)
		{
			rtl8198c_tblDrv_mCastv6_t *mCastEntry,*nextMCastEntry;
			rtl8198c_mcast_fwd_descriptor6_t newFwdDesc;
			
			for (index=0; index<MAX_MCASTV6_TABLE_ENTRY_CNT; index++)
			{
				for (mCastEntry = TAILQ_FIRST(&mCastTbl6.inuseList.mCastTbl[index]); mCastEntry; mCastEntry = nextMCastEntry)
				{
					nextMCastEntry=TAILQ_NEXT(mCastEntry, nextMCast);
					if ((mCastEntry->dip.v6_addr32[0]!=mcastEventContext.groupAddr[0])||
						(mCastEntry->dip.v6_addr32[1]!=mcastEventContext.groupAddr[1])||
						(mCastEntry->dip.v6_addr32[2]!=mcastEventContext.groupAddr[2])||
						(mCastEntry->dip.v6_addr32[3]!=mcastEventContext.groupAddr[3]))
					{
						continue;
					}
				
					
					memset(&newFwdDesc, 0 ,sizeof(rtl865x_mcast_fwd_descriptor_t));
					strcpy(newFwdDesc.netifName,mcastEventContext.devName);
	
					/*save old multicast entry forward port mask*/
					oldAsicFwdPortMask=mCastEntry->mbr;
					oldCpuFlag=mCastEntry->cpu;
	
	
					memset(&mCastDataInfo, 0, sizeof(rtl_kernelMCastDataInfo_t));
					mCastDataInfo.ipVersion = IP_VERSION6;
					mCastDataInfo.groupAddr[0] = mCastEntry->dip.v6_addr32[0];
					mCastDataInfo.groupAddr[1] = mCastEntry->dip.v6_addr32[1];
					mCastDataInfo.groupAddr[2] = mCastEntry->dip.v6_addr32[2];
					mCastDataInfo.groupAddr[3] = mCastEntry->dip.v6_addr32[3];
					mCastDataInfo.sourceIp[0] = mCastEntry->sip.v6_addr32[0];
					mCastDataInfo.sourceIp[1] = mCastEntry->sip.v6_addr32[1];
					mCastDataInfo.sourceIp[2] = mCastEntry->sip.v6_addr32[2];
					mCastDataInfo.sourceIp[3] = mCastEntry->sip.v6_addr32[3];

					retVal = rtl_getKernelMCastDataFwdInfo(&mCastDataInfo, &mCastFwdInfo);
					if(retVal!=SUCCESS && mCastFwdInfo.unknownMCast!=TRUE)
					{
						continue;
					}
	
					newFwdDesc.fwdPortMask=mCastFwdInfo.fwdPortMask & (~(1<<mCastEntry->port));					
					newFwdDesc.toCpu=mCastFwdInfo.cpuFlag;

					_rtl8198C_mergeMCastv6FwdDescChain(&mCastEntry->fwdDescChain,&newFwdDesc);
					mCastEntry->mbr 		= rtl8198C_genMCastv6EntryAsicFwdMaskIgnoreCpuFlag(mCastEntry);
					mCastEntry->cpu 	= rtl8198C_genMCastv6EntryCpuFlag(mCastEntry);
					mCastEntry->unKnownMCast = mCastFwdInfo.unknownMCast;

					newAsicFwdPortMask	= mCastEntry->mbr ;
					newCpuFlag			=mCastEntry->cpu;
						

					if((oldCpuFlag != newCpuFlag)||(newAsicFwdPortMask!=oldAsicFwdPortMask)) 
					{	
						/*reset inAsic flag to re-select or re-write this hardware asic entry*/
						if(newAsicFwdPortMask==0 || newCpuFlag==TRUE)
						{
							_rtl8198C_freeMCastv6Entry(mCastEntry,index);
						}	
						_rtl8198C_arrangeMulticastv6(index);
					}
				}
			}
		}
	#endif
		else
		{
			printk("ip version%d is not supported now!\n", mcastEventContext.ipVersion);
			return EVENT_CONTINUE_EXECUTE;
		}
		
	}


	
	
	return 0;
}
static int32 _rtl865x_kernelMCastUnRegisterEvent(void)
{
	rtl865x_event_Param_t eventParam;

	eventParam.eventLayerId=DEFAULT_LAYER3_EVENT_LIST_ID;
	eventParam.eventId=EVENT_UPDATE_KERNEL_MCAST;
	eventParam.eventPriority=0;
	eventParam.event_action_fn=rtl_kernelMCastCallbackFn;
	rtl865x_unRegisterEvent(&eventParam);

	return SUCCESS;
}

static int32 _rtl865x_kernelMCastRegisterEvent(void)
{
	rtl865x_event_Param_t eventParam;

	eventParam.eventLayerId=DEFAULT_LAYER3_EVENT_LIST_ID;
	eventParam.eventId=EVENT_UPDATE_KERNEL_MCAST;
	eventParam.eventPriority=0;
	eventParam.event_action_fn=rtl_kernelMCastCallbackFn;
	rtl865x_registerEvent(&eventParam);

	return SUCCESS;
}

int rtl_mc_entry_add(uint32 *srcAddr, uint32 *grpAddr, int32 portNum, int8 filterMode, int32 wanOps, int32 wanPPP, int32 ipVersion)
{
	rtl_kernelGroupEntry_t	*groupEntryPtr=NULL, groupEntryNew;
	rtl_kernelPortEntry_t   *portEntryPtr=NULL, portEntryNew;
	rtl_kernelSourceEntry_t *sourceEntryPtr=NULL, sourceEntryNew;
	uint32 anySource[4] = {0};

#if defined(CONFIG_RTL_8198C)||defined(CONFIG_RTL_8197F)
	if((ipVersion!=IP_VERSION4)&&(ipVersion!=IP_VERSION6))
#else
	if(ipVersion!=IP_VERSION4)
#endif
	{
		printk("[%s:%d]\n, wrong ip version%d!", __FUNCTION__, __LINE__, ipVersion);
		return FAILED;
	}

#if 0	
	printk("src:%x%x%x%x, group:%x%x%x%x, portNum:%d, ops:%d, ppp:%d, ipVersion:%d\n",
		srcAddr[0], srcAddr[1], srcAddr[2], srcAddr[3], 
		grpAddr[0], grpAddr[1], grpAddr[2], grpAddr[3],
		portNum, wanOps, wanPPP, ipVersion);
#endif

	if((srcAddr==NULL) || (grpAddr==NULL))
		return FAILED;
	
	groupEntryPtr=rtl_kernelGroupEntrySearch(grpAddr,ipVersion);
	if(groupEntryPtr==NULL)
	{
		memset(&groupEntryNew, 0, sizeof(rtl_kernelGroupEntry_t));
		if(ipVersion==IP_VERSION4)
		{
			groupEntryNew.groupAddr[0] = grpAddr[0];
		}
	#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
		else
		{
			groupEntryNew.groupAddr[0] = grpAddr[0];
			groupEntryNew.groupAddr[1] = grpAddr[1];
			groupEntryNew.groupAddr[2] = grpAddr[2];
			groupEntryNew.groupAddr[3] = grpAddr[3];
		}
	#endif
		groupEntryNew.ipVersion = ipVersion;
		groupEntryNew.wanOps = wanOps;
		groupEntryNew.wanPPP = wanPPP;
		groupEntryPtr = rtl_kernelGroupEntryEnqueue(&groupEntryNew,ipVersion);
	}
	
	if(groupEntryPtr==NULL)
		return FAILED;

	if((groupEntryPtr->ipVersion!=ipVersion)||(groupEntryPtr->wanOps!=wanOps)||(groupEntryPtr->wanPPP!=wanPPP))
	{
		printk("\nipVersion/wan opmode/wan ppp mode won't change!");
	}

	portEntryPtr = rtl_kernelPortEntrySearch(&(groupEntryPtr->portList),portNum);
	if(portEntryPtr==NULL)
	{
		memset(&portEntryNew, 0, sizeof(rtl_kernelPortEntry_t));
		portEntryNew.portNum = portNum;
		portEntryNew.filterMode = filterMode;
		portEntryPtr = rtl_kernelPortEntryEnqueue(&(groupEntryPtr->portList),&portEntryNew,ipVersion, &(groupEntryPtr->portCnt));
	}
	if(portEntryPtr==NULL)
		return FAILED;

	if(portEntryPtr->filterMode != filterMode)
	{
		printk("filter Mode is not matched, please delete the old portEntry Filter!\n");
		return FAILED;
	}

	if(portEntryPtr->filterMode == HW_MC_API_EXCLUDE)
	{
		sourceEntryPtr = rtl_kernelSourceEntrySearch(&(portEntryPtr->srcList), anySource, ipVersion);
		if(sourceEntryPtr==NULL)
		{
			//new sourceEntry, or no (NULL,G)
			sourceEntryPtr = rtl_kernelSourceEntrySearch(&(portEntryPtr->srcList), srcAddr, ipVersion);
			if(sourceEntryPtr==NULL)
			{
				memset(&sourceEntryNew, 0, sizeof(rtl_kernelSourceEntry_t));
				if(ipVersion==IP_VERSION4)
				{
					sourceEntryNew.sourceAddr[0] = srcAddr[0];
				}
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
				else
				{
					sourceEntryNew.sourceAddr[0] = srcAddr[0];
					sourceEntryNew.sourceAddr[1] = srcAddr[1];
					sourceEntryNew.sourceAddr[2] = srcAddr[2];
					sourceEntryNew.sourceAddr[3] = srcAddr[3];
				}
#endif
				sourceEntryNew.ipVersion = ipVersion;

				if((srcAddr[0]==0)
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
					&&(srcAddr[1]==0)&&(srcAddr[2]==0)&&(srcAddr[3]==0)
#endif
				)
				{
					rtl_kernelSourceEntryFlush(&(portEntryPtr->srcList),ipVersion, &(portEntryPtr->srcCnt));
				}

				sourceEntryPtr = rtl_kernelSourceEntryEnqueue(&(portEntryPtr->srcList),&sourceEntryNew,ipVersion,&(portEntryPtr->srcCnt));
			}
		}
		else
		{
			//(NULL, G), no need to add specific (S,G) or (NULL,G) 
		}
	}
	else //filterMode = INCLUDE 
	{
		if((srcAddr[0]==0)
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
			&&(srcAddr[1]==0)&&(srcAddr[2]==0)&&(srcAddr[3]==0)
#endif
		)
		{
			printk("port %d can't include NULL\n", portEntryPtr->portNum);
			return FAILED;
		}

		sourceEntryPtr = rtl_kernelSourceEntrySearch(&(portEntryPtr->srcList), srcAddr, ipVersion);
		if(sourceEntryPtr==NULL)
		{
			memset(&sourceEntryNew, 0, sizeof(rtl_kernelSourceEntry_t));
			if(ipVersion==IP_VERSION4)
			{
				sourceEntryNew.sourceAddr[0] = srcAddr[0];
			}
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
			else
			{
				sourceEntryNew.sourceAddr[0] = srcAddr[0];
				sourceEntryNew.sourceAddr[1] = srcAddr[1];
				sourceEntryNew.sourceAddr[2] = srcAddr[2];
				sourceEntryNew.sourceAddr[3] = srcAddr[3];
			}
#endif
			sourceEntryNew.ipVersion = ipVersion;
			sourceEntryPtr = rtl_kernelSourceEntryEnqueue(&(portEntryPtr->srcList),&sourceEntryNew,ipVersion,&(portEntryPtr->srcCnt));
		}
	}
	
	strcpy(rtl_kernel_report_event.devName, "eth*");
	rtl_kernel_report_event.ipVersion = ipVersion;
	if(ipVersion==IP_VERSION4)
	{
		rtl_kernel_report_event.groupAddr[0] = grpAddr[0];
		rtl_kernel_report_event.sourceAddr[0] = srcAddr[0];
	}
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
	else
	{
		rtl_kernel_report_event.groupAddr[0] = grpAddr[0];
		rtl_kernel_report_event.groupAddr[1] = grpAddr[1];
		rtl_kernel_report_event.groupAddr[2] = grpAddr[2];
		rtl_kernel_report_event.groupAddr[3] = grpAddr[3];
		rtl_kernel_report_event.sourceAddr[0] = srcAddr[0];
		rtl_kernel_report_event.sourceAddr[1] = srcAddr[1];
		rtl_kernel_report_event.sourceAddr[2] = srcAddr[2];
		rtl_kernel_report_event.sourceAddr[3] = srcAddr[3];
	}
#endif
	rtl_kernel_report_event.moduleIndex = 0;//not care
	rtl_kernel_report_event.portMask = 0;//not care
	rtl865x_raiseEvent(EVENT_UPDATE_KERNEL_MCAST, &rtl_kernel_report_event);

	return SUCCESS;
}
int rtl_mc_entry_del(uint32 *srcAddr, uint32 *grpAddr, int32 portNum, int32 ipVersion) 
{
	rtl_kernelGroupEntry_t	*groupEntryPtr=NULL;
	rtl_kernelPortEntry_t	*portEntryPtr=NULL;
	rtl_kernelSourceEntry_t *sourceEntryPtr=NULL;

	
#if defined(CONFIG_RTL_8198C)||defined(CONFIG_RTL_8197F)
	if((ipVersion!=IP_VERSION4)&&(ipVersion!=IP_VERSION6))
#else
	if(ipVersion!=IP_VERSION4)
#endif
	{
		printk("[%s:%d]\n, wrong ip version%d!", __FUNCTION__, __LINE__, ipVersion);
		return FAILED;
	}

	if((srcAddr==NULL) || (grpAddr==NULL))
		return FAILED;

#if 0		
	printk("src:%x%x%x%x, group:%x%x%x%x, portNum:%d, ipVersion:%d",
		srcAddr[0], srcAddr[1], srcAddr[2], srcAddr[3], 
		grpAddr[0], grpAddr[1], grpAddr[2], grpAddr[3],
		portNum, ipVersion);
#endif
	
	groupEntryPtr=rtl_kernelGroupEntrySearch(grpAddr,ipVersion);		
	if(groupEntryPtr==NULL)
	{
	#if 0
		printk("[%s, %d] No entry group with:%x%x%x%x exist!\n", __FUNCTION__, __LINE__, 
			grpAddr[0], grpAddr[1], grpAddr[2], grpAddr[3]);
	#endif
		return SUCCESS;
	}

	portEntryPtr = rtl_kernelPortEntrySearch(&(groupEntryPtr->portList),portNum);
	if(portEntryPtr==NULL)
	{
	#if 0
		printk("[%s, %d] No entry with group:%x%x%x%x portNum:%d exist!\n", __FUNCTION__, __LINE__, 
			grpAddr[0], grpAddr[1], grpAddr[2], grpAddr[3], portNum);
	#endif
		return SUCCESS;
	}

	if((srcAddr[0]==0)
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
		&&(srcAddr[1]==0)&&(srcAddr[2]==0)&&(srcAddr[3]==0)
#endif
	)
	{
		//flush all srcList
		rtl_kernelSourceEntryFlush(&(portEntryPtr->srcList),ipVersion,&(portEntryPtr->srcCnt));
	}
	else
	{
		sourceEntryPtr = rtl_kernelSourceEntrySearch(&(portEntryPtr->srcList), srcAddr, ipVersion);
		if(sourceEntryPtr==NULL)
		{
		#if 0
			printk("[%s, %d] No entry with group:%x%x%x%x src:%x%x%x%x portNum:%d exist!\n", __FUNCTION__, __LINE__, 
				grpAddr[0], grpAddr[1], grpAddr[2], grpAddr[3], 
				srcAddr[0], srcAddr[1], srcAddr[2], srcAddr[3], portNum);
		#endif
			return SUCCESS;
		}
		rtl_kernelSourceEntryDequeue(sourceEntryPtr,ipVersion,&(portEntryPtr->srcCnt));
	}

	if(portEntryPtr->srcCnt==0)
	{
		rtl_kernelPortEntryDequeue(portEntryPtr,ipVersion,&(groupEntryPtr->portCnt));
		if(groupEntryPtr->portCnt==0)
			rtl_kernelGroupEntryDequeue(groupEntryPtr,ipVersion);
	}

	strcpy(rtl_kernel_report_event.devName, "eth*");
	rtl_kernel_report_event.ipVersion = ipVersion;
	if(ipVersion==IP_VERSION4)
	{
		rtl_kernel_report_event.groupAddr[0] = grpAddr[0];
		rtl_kernel_report_event.sourceAddr[0] = srcAddr[0];
	}
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
	else
	{
		rtl_kernel_report_event.groupAddr[0] = grpAddr[0];
		rtl_kernel_report_event.groupAddr[1] = grpAddr[1];
		rtl_kernel_report_event.groupAddr[2] = grpAddr[2];
		rtl_kernel_report_event.groupAddr[3] = grpAddr[3];
		rtl_kernel_report_event.sourceAddr[0] = srcAddr[0];
		rtl_kernel_report_event.sourceAddr[1] = srcAddr[1];
		rtl_kernel_report_event.sourceAddr[2] = srcAddr[2];
		rtl_kernel_report_event.sourceAddr[3] = srcAddr[3];
	}
#endif
	rtl_kernel_report_event.moduleIndex = 0;//not care
	rtl_kernel_report_event.portMask = 0;//not care
	rtl865x_raiseEvent(EVENT_UPDATE_KERNEL_MCAST, &rtl_kernel_report_event);

	return SUCCESS;

}

int32 rtl_getKernelMCastDataFwdInfo(rtl_kernelMCastDataInfo_t *mCastDataInfo, rtl_kernelMCastFwdInfo_t *mCastFwdInfo)
{
	rtl_kernelGroupEntry_t *groupEntryPtr=NULL;
	rtl_kernelPortEntry_t  *portEntryPtr=NULL, *portEntryPtrNext=NULL;
	rtl_kernelSourceEntry_t *sourceEntryPtr=NULL;
	uint32 anySource[4] = {0};
	uint32 fwdPortMask = 0;
	
	if(mCastDataInfo==NULL || mCastFwdInfo==NULL)
	{
		return FAILED;
	}

	memset(mCastFwdInfo, 0, sizeof(rtl_kernelMCastFwdInfo_t));

	groupEntryPtr = rtl_kernelGroupEntrySearch(mCastDataInfo->groupAddr,mCastDataInfo->ipVersion);
	if(groupEntryPtr==NULL)
	{
		mCastFwdInfo->unknownMCast = TRUE;
		mCastFwdInfo->cpuFlag = TRUE;
		return FAILED;
	}

	portEntryPtr = MC_LIST_FIRST(&(groupEntryPtr->portList));
	while(portEntryPtr)
	{
		portEntryPtrNext = MC_LIST_NEXT(portEntryPtr, next);

		if(portEntryPtr->filterMode==HW_MC_API_EXCLUDE)
		{
			sourceEntryPtr = rtl_kernelSourceEntrySearch(&(portEntryPtr->srcList), anySource, mCastDataInfo->ipVersion);
			if(sourceEntryPtr==NULL)
			{
				//without (*,G), search(S,G)
				sourceEntryPtr = rtl_kernelSourceEntrySearch(&(portEntryPtr->srcList),mCastDataInfo->sourceIp,mCastDataInfo->ipVersion);
				if(sourceEntryPtr==NULL)
					fwdPortMask |= (1<<portEntryPtr->portNum);
			}
			else 
			{
				//exclude(NULL, G)
				fwdPortMask |= (1<<portEntryPtr->portNum);
			}

		}
		else
		{
			sourceEntryPtr = rtl_kernelSourceEntrySearch(&(portEntryPtr->srcList), mCastDataInfo->sourceIp, mCastDataInfo->ipVersion);
			if(sourceEntryPtr)
				fwdPortMask |= (1<<portEntryPtr->portNum);
		}
		
		portEntryPtr = portEntryPtrNext;
	}

	if(fwdPortMask==0)
	{
		//unknown multicast
		mCastFwdInfo->unknownMCast = TRUE;
		mCastFwdInfo->cpuFlag = TRUE;
		return FAILED;
	}
	
	if(fwdPortMask>>RTL8651_MAC_NUMBER)
	{
		//with cpu port or wlan port
		mCastFwdInfo ->cpuFlag = TRUE;
	}
	
	mCastFwdInfo->fwdPortMask = fwdPortMask&((1<<RTL8651_MAC_NUMBER)-1);
	mCastFwdInfo->wanOps = groupEntryPtr->wanOps;
	mCastFwdInfo->wanPPP = groupEntryPtr->wanPPP;
	
	return SUCCESS;
}
#if defined(CONFIG_RTL_8197F)
extern int32 rtl_getLanIfAsicIndex(int32 * index);
int32 rtl_getKernelMCastWanType(uint32 *groupAddr, int32 ipVersion, int32 *wanOps, int32 *wanPPP)
{
	rtl_kernelGroupEntry_t *groupEntryPtr=NULL;

	groupEntryPtr = rtl_kernelGroupEntrySearch(groupAddr,ipVersion);
	if(groupEntryPtr==NULL)
	{
		return FAILED;
	}
	*wanOps = groupEntryPtr->wanOps;
	*wanPPP = groupEntryPtr->wanPPP;
	return SUCCESS;

}
int32 rtl_getKernelMCastDifbyWanOps(int32 wanOps)
{
	int dif = 1, ret=FAILED;
	if(wanOps==HW_MC_API_WAN_ROUTE)
	{
		ret = rtl_getLanIfAsicIndex(&dif);
		if(ret==SUCCESS)
		{
			return dif;
		}
	}
		
	return 1;
}
#endif
int32 rtl_kernelMCastHwAccelerateV4(uint32 srcPort, uint32 srcVlanId, uint32 srcIpAddr, uint32 destIpAddr)
{
	rtl865x_tblDrv_mCast_t *existMulticastEntry=NULL;
	rtl_kernelMCastDataInfo_t mCastDataInfo;
	rtl_kernelMCastFwdInfo_t  mCastFwdInfo;
	rtl865x_mcast_fwd_descriptor_t  fwdDescriptor;
	int ret = FAILED, add_ret = FAILED;

#if 0
	if(net_ratelimit())
		printk("srcPort:%d, srcVlanId:%d, srcIpAddr:%x, groupAddr:%x, [%s:%d]\n", srcPort, srcVlanId, srcIpAddr, destIpAddr, __FUNCTION__, __LINE__);
#endif

	existMulticastEntry=rtl865x_findMCastEntry(destIpAddr, srcIpAddr, (unsigned short)srcVlanId, (unsigned short)srcPort);
	if(existMulticastEntry!=NULL)
	{
		/*it's already in cache */
		//if(net_ratelimit()) printk("[%s:%d]already in cache\n",__FUNCTION__,__LINE__);
		return FAILED;
	}
	mCastDataInfo.ipVersion = IP_VERSION4;
	mCastDataInfo.groupAddr[0] = destIpAddr;
	mCastDataInfo.sourceIp[0] = srcIpAddr;
	
	ret = rtl_getKernelMCastDataFwdInfo(&mCastDataInfo,&mCastFwdInfo);

	if(ret==SUCCESS && mCastFwdInfo.cpuFlag!=TRUE)
	{
		memset(&fwdDescriptor, 0, sizeof(rtl865x_mcast_fwd_descriptor_t));
		strncpy(fwdDescriptor.netifName, "eth*", 16);
		fwdDescriptor.fwdPortMask = mCastFwdInfo.fwdPortMask & (~(1<<srcPort));
		
		if(fwdDescriptor.fwdPortMask!=0)
		{
		#if defined(CONFIG_RTL_MULTICAST_PORT_MAPPING)
			add_ret = rtl865x_addMulticastEntry(destIpAddr, srcIpAddr, (unsigned short)srcVlanId, (unsigned short)srcPort,
					&fwdDescriptor, 1, 0, 0, 0, 0xFFFFFFFF);
		#else			
			add_ret = rtl865x_addMulticastEntry(destIpAddr, srcIpAddr, (unsigned short)srcVlanId, (unsigned short)srcPort,
					&fwdDescriptor, 1, 0, 0, 0);
		#endif
		}
	}

	return add_ret;	
}
#if defined(CONFIG_RTL_8198C)||defined(CONFIG_RTL_8197F)
int32 rtl_kernelMCastHwAccelerateV6(uint32 srcPort,uint32 srcVlanId,uint32 *srcIpAddr,uint32 *destIpAddr)
{
	rtl8198c_tblDrv_mCastv6_t * existMulticastEntry=NULL;
	rtl8198c_mcast_fwd_descriptor6_t  fwdDescriptor;
	rtl_kernelMCastDataInfo_t mCastDataInfo;
	rtl_kernelMCastFwdInfo_t  mCastFwdInfo;
	inv6_addr_t sip,dip;
	int ret = FAILED, add_ret = FAILED;

	mCastDataInfo.sourceIp[0] = sip.v6_addr32[0] = srcIpAddr[0];	
	mCastDataInfo.sourceIp[1] = sip.v6_addr32[1] = srcIpAddr[1];
	mCastDataInfo.sourceIp[2] = sip.v6_addr32[2] = srcIpAddr[2];
	mCastDataInfo.sourceIp[3] = sip.v6_addr32[3] = srcIpAddr[3];
	mCastDataInfo.groupAddr[0] = dip.v6_addr32[0] = destIpAddr[0];	
	mCastDataInfo.groupAddr[1] = dip.v6_addr32[1] = destIpAddr[1];
	mCastDataInfo.groupAddr[2] = dip.v6_addr32[2] = destIpAddr[2];
	mCastDataInfo.groupAddr[3] = dip.v6_addr32[3] = destIpAddr[3];

#if 0
	if(net_ratelimit())
		printk("srcPort:%d, srcVlanId:%d, srcIpAddr:%x%x%x%x, groupAddr:%x%x%x%x, [%s:%d]\n", 
		srcPort, srcVlanId, 
		mCastDataInfo.sourceIp[0],  mCastDataInfo.sourceIp[1], mCastDataInfo.sourceIp[2], mCastDataInfo.sourceIp[3],
		mCastDataInfo.groupAddr[0],	mCastDataInfo.groupAddr[1], mCastDataInfo.groupAddr[2], mCastDataInfo.groupAddr[3] ,__FUNCTION__, __LINE__);
#endif

	existMulticastEntry=rtl8198C_findMCastv6Entry(dip,sip, (unsigned short)srcVlanId, (unsigned short)srcPort);
	if(existMulticastEntry!=NULL)
	{
		/*it's already in cache */
		//if(net_ratelimit()) printk("[%s:%d]already in cache\n",__FUNCTION__,__LINE__);
		return FAILED;
	}
	mCastDataInfo.ipVersion = IP_VERSION6;
	
	ret = rtl_getKernelMCastDataFwdInfo(&mCastDataInfo,&mCastFwdInfo);

	if(ret==SUCCESS && mCastFwdInfo.cpuFlag!=TRUE)
	{
		memset(&fwdDescriptor, 0, sizeof(rtl8198c_mcast_fwd_descriptor6_t));
		strncpy(fwdDescriptor.netifName, "eth*", 16);
		fwdDescriptor.fwdPortMask = mCastFwdInfo.fwdPortMask & (~(1<<srcPort));
		
		if(fwdDescriptor.fwdPortMask!=0)
		{		
#if defined(CONFIG_RTL_MULTICAST_PORT_MAPPING)
			add_ret=rtl8198C_addMulticastv6Entry(dip,sip, (unsigned short)srcVlanId, (unsigned short)srcPort,
							&fwdDescriptor, 1, 0, 0, 0, 0xFFFFFFFF);
#else
			add_ret=rtl8198C_addMulticastv6Entry(dip,sip, (unsigned short)srcVlanId, (unsigned short)srcPort,
							&fwdDescriptor, 1, 0, 0, 0);
#endif
		}
	}

	return add_ret;	

}
#endif

int rtl_kernelMCast_show(struct seq_file *s, void *v)
{
	rtl_kernelGroupEntry_t 	*groupEntryPtr = NULL, *groupEntryPtrNext = NULL;
	rtl_kernelPortEntry_t 	*portEntryPtr = NULL, *portEntryPtrNext = NULL;
	rtl_kernelSourceEntry_t	*sourceEntryPtr = NULL,*sourceEntryPtrNext = NULL;
	int groupCnt, portCnt, sourceCnt;

	seq_printf(s,"igmp list: Free Group:%d Port:%d Source:%d\n", 
		rtl_kernelGroupEntryFreeCnt(IP_VERSION4), rtl_kernelPortEntryFreeCnt(IP_VERSION4), rtl_kernelSourceEntryFreeCnt(IP_VERSION4));
	
	groupEntryPtr = MC_LIST_FIRST(&rtl_igmp_kernelGroupEntry_list);
	groupCnt = 0;
	while(groupEntryPtr)
	{
		groupEntryPtrNext = MC_LIST_NEXT(groupEntryPtr, next);
		groupCnt++;

		seq_printf(s, "    [%d] Group address:%d.%d.%d.%d BridgeWan:%d, PPPConnection:%d\n",groupCnt,
		groupEntryPtr->groupAddr[0]>>24, (groupEntryPtr->groupAddr[0]&0x00ff0000)>>16,
		(groupEntryPtr->groupAddr[0]&0x0000ff00)>>8, (groupEntryPtr->groupAddr[0]&0xff),
		(groupEntryPtr->wanOps==HW_MC_API_WAN_BRIDGE)?1:0, 
		(groupEntryPtr->wanPPP==HW_MC_API_WAN_PPP)?1:0);

		portEntryPtr = MC_LIST_FIRST(&(groupEntryPtr->portList));
		portCnt = 0;
		while(portEntryPtr)
		{
			portEntryPtrNext = MC_LIST_NEXT(portEntryPtr, next);
			portCnt++;
			seq_printf(s, "        <%d> port:%d %s",portCnt, portEntryPtr->portNum, (portEntryPtr->filterMode==HW_MC_API_INCLUDE)?"IN":"EX");

			sourceEntryPtr = MC_LIST_FIRST(&(portEntryPtr->srcList));
			sourceCnt=0;
			if(sourceEntryPtr)
			{
				seq_printf(s, "\\source list:");
			}
			while(sourceEntryPtr)
			{
				sourceEntryPtrNext = MC_LIST_NEXT(sourceEntryPtr, next);
				sourceCnt++;
				seq_printf(s, "%d.%d.%d.%d",
						sourceEntryPtr->sourceAddr[0]>>24, (sourceEntryPtr->sourceAddr[0]&0x00ff0000)>>16,
						(sourceEntryPtr->sourceAddr[0]&0x0000ff00)>>8, (sourceEntryPtr->sourceAddr[0]&0xff));

				sourceEntryPtr = sourceEntryPtrNext;
				if(sourceEntryPtr)
					seq_printf(s, " ,");
			}

			portEntryPtr = portEntryPtrNext;
			seq_printf(s, "\n");
		}

		groupEntryPtr = groupEntryPtrNext;
	}
#if defined(CONFIG_RTL_8198C)||defined(CONFIG_RTL_8197F)
	seq_printf(s,"\n");
	seq_printf(s,"mld list: Free Group:%d  Port:%d  Source:%d\n", 
		rtl_kernelGroupEntryFreeCnt(IP_VERSION6), rtl_kernelPortEntryFreeCnt(IP_VERSION6), rtl_kernelSourceEntryFreeCnt(IP_VERSION6));

	groupEntryPtr = MC_LIST_FIRST(&rtl_mld_kernelGroupEntry_list);
	groupCnt = 0;
	while(groupEntryPtr)
	{
		groupEntryPtrNext = MC_LIST_NEXT(groupEntryPtr, next);
		groupCnt++;

		seq_printf(s, "    [%d] Group address:%x%x%x%x%x%x%x%x-%x%x%x%x%x%x%x%x-%x%x%x%x%x%x%x%x-%x%x%x%x%x%x%x%x BridgeWan:%d, PPPConnection:%d\n",groupCnt,
			(groupEntryPtr->groupAddr[0])>>28,(groupEntryPtr->groupAddr[0]<<4)>>28, (groupEntryPtr->groupAddr[0]<<8)>>28,(groupEntryPtr->groupAddr[0]<<12)>>28, 
			(groupEntryPtr->groupAddr[0]<<16)>>28,(groupEntryPtr->groupAddr[0]<<20)>>28,(groupEntryPtr->groupAddr[0]<<24)>>28, (groupEntryPtr->groupAddr[0]<<28)>>28, 
			(groupEntryPtr->groupAddr[1])>>28,(groupEntryPtr->groupAddr[1]<<4)>>28, (groupEntryPtr->groupAddr[1]<<8)>>28,(groupEntryPtr->groupAddr[1]<<12)>>28, 
			(groupEntryPtr->groupAddr[1]<<16)>>28,(groupEntryPtr->groupAddr[1]<<20)>>28,(groupEntryPtr->groupAddr[1]<<24)>>28, (groupEntryPtr->groupAddr[1]<<28)>>28, 
			(groupEntryPtr->groupAddr[2])>>28,(groupEntryPtr->groupAddr[2]<<4)>>28, (groupEntryPtr->groupAddr[2]<<8)>>28,(groupEntryPtr->groupAddr[2]<<12)>>28, 
			(groupEntryPtr->groupAddr[2]<<16)>>28,(groupEntryPtr->groupAddr[2]<<20)>>28,(groupEntryPtr->groupAddr[2]<<24)>>28, (groupEntryPtr->groupAddr[2]<<28)>>28, 
			(groupEntryPtr->groupAddr[3])>>28,(groupEntryPtr->groupAddr[3]<<4)>>28, (groupEntryPtr->groupAddr[3]<<8)>>28,(groupEntryPtr->groupAddr[3]<<12)>>28, 
			(groupEntryPtr->groupAddr[3]<<16)>>28,(groupEntryPtr->groupAddr[3]<<20)>>28,(groupEntryPtr->groupAddr[3]<<24)>>28, (groupEntryPtr->groupAddr[3]<<28)>>28,
			(groupEntryPtr->wanOps==HW_MC_API_WAN_BRIDGE)?1:0, 
			(groupEntryPtr->wanPPP==HW_MC_API_WAN_PPP)?1:0);

		portEntryPtr = MC_LIST_FIRST(&(groupEntryPtr->portList));
		portCnt = 0;
		while(portEntryPtr)
		{
			portEntryPtrNext = MC_LIST_NEXT(portEntryPtr, next);
			portCnt++;
			seq_printf(s, " 	   <%d> port:%d %s",portCnt, portEntryPtr->portNum, (portEntryPtr->filterMode==HW_MC_API_INCLUDE)?"IN":"EX");

			sourceEntryPtr = MC_LIST_FIRST(&(portEntryPtr->srcList));
			sourceCnt=0;
			if(sourceEntryPtr)
			{
				seq_printf(s, "\\source list:");
			}
			while(sourceEntryPtr)
			{
				sourceEntryPtrNext = MC_LIST_NEXT(sourceEntryPtr, next);
				sourceCnt++;
				seq_printf(s, "%x%x%x%x%x%x%x%x-%x%x%x%x%x%x%x%x-%x%x%x%x%x%x%x%x-%x%x%x%x%x%x%x%x",
					(sourceEntryPtr->sourceAddr[0])>>28,(sourceEntryPtr->sourceAddr[0]<<4)>>28, (sourceEntryPtr->sourceAddr[0]<<8)>>28,(sourceEntryPtr->sourceAddr[0]<<12)>>28, 
					(sourceEntryPtr->sourceAddr[0]<<16)>>28,(sourceEntryPtr->sourceAddr[0]<<20)>>28,(sourceEntryPtr->sourceAddr[0]<<24)>>28, (sourceEntryPtr->sourceAddr[0]<<28)>>28, 
					(sourceEntryPtr->sourceAddr[1])>>28,(sourceEntryPtr->sourceAddr[1]<<4)>>28, (sourceEntryPtr->sourceAddr[1]<<8)>>28,(sourceEntryPtr->sourceAddr[1]<<12)>>28, 
					(sourceEntryPtr->sourceAddr[1]<<16)>>28,(sourceEntryPtr->sourceAddr[1]<<20)>>28,(sourceEntryPtr->sourceAddr[1]<<24)>>28, (sourceEntryPtr->sourceAddr[1]<<28)>>28, 
					(sourceEntryPtr->sourceAddr[2])>>28,(sourceEntryPtr->sourceAddr[2]<<4)>>28, (sourceEntryPtr->sourceAddr[2]<<8)>>28,(sourceEntryPtr->sourceAddr[2]<<12)>>28, 
					(sourceEntryPtr->sourceAddr[2]<<16)>>28,(sourceEntryPtr->sourceAddr[2]<<20)>>28,(sourceEntryPtr->sourceAddr[2]<<24)>>28, (sourceEntryPtr->sourceAddr[2]<<28)>>28, 
					(sourceEntryPtr->sourceAddr[3])>>28,(sourceEntryPtr->sourceAddr[3]<<4)>>28, (sourceEntryPtr->sourceAddr[3]<<8)>>28,(sourceEntryPtr->sourceAddr[3]<<12)>>28, 
					(sourceEntryPtr->sourceAddr[3]<<16)>>28,(sourceEntryPtr->sourceAddr[3]<<20)>>28,(sourceEntryPtr->sourceAddr[3]<<24)>>28, (sourceEntryPtr->sourceAddr[3]<<28)>>28);

				sourceEntryPtr = sourceEntryPtrNext;
				if(sourceEntryPtr)
					seq_printf(s, " ,");
			}

			portEntryPtr = portEntryPtrNext;
			seq_printf(s, "\n");
		}

		groupEntryPtr = groupEntryPtrNext;
	}

#endif
	return SUCCESS;

	
}
int rtl_kernelMCast_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char tmp[256];

	char		*strptr, *cmd_addr;
	char		*tokptr;
	uint32	ipAddr[4]={0}, groupAddr[4]={0}, srcAddr[4]={0};
	int32 	portNum = -1,  wanOps = 0, ipVersion, addFlag = 0, wanPPP = 0, filterMode = HW_MC_API_EXCLUDE;
	int cnt;	

	if (count < 5)
		return -EFAULT;


	if (buffer && !copy_from_user(tmp, buffer, count)) {

		tmp[count] = '\0';
		strptr=tmp;
		cmd_addr = strsep(&strptr," ");
		if (cmd_addr==NULL)
		{
			goto errout;
		}

		if (!memcmp(cmd_addr, "add", 3)||!memcmp(cmd_addr, "del", 3))
		{
			if(!memcmp(cmd_addr, "add", 3))
				addFlag = 1;
			
			tokptr = strsep(&strptr," ");
			if (tokptr==NULL)
			{
				goto errout;
			}
			
			if(!memcmp(tokptr, "ipv4", 4))	
			{
				ipVersion=IP_VERSION4;
			}
		#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
			else if (!memcmp(tokptr, "ipv6", 4))
			{
				ipVersion=IP_VERSION6;
			}
		#endif
			else 
			{
				goto errout;
			}
			
			if(ipVersion==IP_VERSION4)
			{
				tokptr = strsep(&strptr," ");
				if (tokptr==NULL)
				{
					goto errout;
				}
				
				cnt = sscanf(tokptr, "%d.%d.%d.%d", &ipAddr[0], &ipAddr[1], &ipAddr[2], &ipAddr[3]);
		
				groupAddr[0]=(ipAddr[0]<<24)|(ipAddr[1]<<16)|(ipAddr[2]<<8)|(ipAddr[3]);
				groupAddr[1]=0;
				groupAddr[2]=0;
				groupAddr[3]=0;
				
				tokptr = strsep(&strptr," ");
				if (tokptr==NULL)
				{			
					goto errout;
				}
				cnt = sscanf(tokptr, "%d.%d.%d.%d", &ipAddr[0], &ipAddr[1], &ipAddr[2], &ipAddr[3]);
				srcAddr[0]=(ipAddr[0]<<24)|(ipAddr[1]<<16)|(ipAddr[2]<<8)|(ipAddr[3]);
				srcAddr[1]=0;
				srcAddr[2]=0;
				srcAddr[3]=0;
			}
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
			else if (ipVersion==IP_VERSION6)
			{
				tokptr = strsep(&strptr," ");
				if (tokptr==NULL)
				{
					goto errout;
				}
				
				cnt = sscanf(tokptr, "0x%x-%x-%x-%x", &ipAddr[0], &ipAddr[1], &ipAddr[2], &ipAddr[3]);

				groupAddr[0]=ipAddr[0];
				groupAddr[1]=ipAddr[1];
				groupAddr[2]=ipAddr[2];
				groupAddr[3]=ipAddr[3];

				tokptr = strsep(&strptr," ");
				if (tokptr==NULL)
				{
					goto errout;
				}
				
				cnt = sscanf(tokptr, "0x%x-%x-%x-%x", &ipAddr[0], &ipAddr[1], &ipAddr[2], &ipAddr[3]);

				srcAddr[0]=ipAddr[0];
				srcAddr[1]=ipAddr[1];
				srcAddr[2]=ipAddr[2];
				srcAddr[3]=ipAddr[3];
				
			#if 0
				printk("src:%x%x%x%x, group:%x%x%x%x\n",
					srcAddr[0], srcAddr[1], srcAddr[2], srcAddr[3], 
					groupAddr[0], groupAddr[1], groupAddr[2], groupAddr[3]);
			#endif
				
			}
#endif

			tokptr = strsep(&strptr," ");
			if (tokptr==NULL)
			{
				goto errout;
			}
			
			portNum = simple_strtol(tokptr, NULL, 0);

			if(addFlag)
			{
				tokptr = strsep(&strptr," ");
				if (tokptr==NULL)
				{
					goto errout;
				}
				
				filterMode = simple_strtol(tokptr, NULL, 0);

				tokptr = strsep(&strptr," ");
				if (tokptr==NULL)
				{
					goto errout;
				}
				
				wanOps = simple_strtol(tokptr, NULL, 0);

				tokptr = strsep(&strptr," ");
				if (tokptr==NULL)
				{					
					goto errout;
				}
				
				wanPPP = simple_strtol(tokptr, NULL, 0);
				rtl_mc_entry_add(srcAddr , groupAddr, portNum, filterMode, wanOps, wanPPP, ipVersion);
			}
			else
			{
				rtl_mc_entry_del(srcAddr, groupAddr, portNum, ipVersion);
			}
			
		}
		else if(!memcmp(cmd_addr, "fwdInfo", 7))
		{
			rtl_kernelMCastFwdInfo_t mCastFwdInfo;
			rtl_kernelMCastDataInfo_t mCastDataInfo;
			int ret = FAILED;

			tokptr = strsep(&strptr," ");
			if (tokptr==NULL)
			{
				goto errout;
			}
			
			if(!memcmp(tokptr, "ipv4", 4))	
			{
				mCastDataInfo.ipVersion = IP_VERSION4;
			}
		#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
			else if (!memcmp(tokptr, "ipv6", 4))
			{
				mCastDataInfo.ipVersion = IP_VERSION6;
			}
		#endif
			else 
			{
				goto errout;
			}
			
			if(mCastDataInfo.ipVersion==IP_VERSION4)
			{
				tokptr = strsep(&strptr," ");
				if (tokptr==NULL)
				{
					goto errout;
				}
				
				cnt = sscanf(tokptr, "%d.%d.%d.%d", &ipAddr[0], &ipAddr[1], &ipAddr[2], &ipAddr[3]);
		
				mCastDataInfo.groupAddr[0]=(ipAddr[0]<<24)|(ipAddr[1]<<16)|(ipAddr[2]<<8)|(ipAddr[3]);
				mCastDataInfo.groupAddr[1]=0;
				mCastDataInfo.groupAddr[2]=0;
				mCastDataInfo.groupAddr[3]=0;
				
				tokptr = strsep(&strptr," ");
				if (tokptr==NULL)
				{			
					goto errout;
				}
				cnt = sscanf(tokptr, "%d.%d.%d.%d", &ipAddr[0], &ipAddr[1], &ipAddr[2], &ipAddr[3]);
				mCastDataInfo.sourceIp[0]=(ipAddr[0]<<24)|(ipAddr[1]<<16)|(ipAddr[2]<<8)|(ipAddr[3]);
				mCastDataInfo.sourceIp[1]=0;
				mCastDataInfo.sourceIp[2]=0;
				mCastDataInfo.sourceIp[3]=0;
			}
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
			else if (mCastDataInfo.ipVersion==IP_VERSION6)
			{
				tokptr = strsep(&strptr," ");
				if (tokptr==NULL)
				{
					goto errout;
				}
				
				cnt = sscanf(tokptr, "0x%x-%x-%x-%x", &ipAddr[0], &ipAddr[1], &ipAddr[2], &ipAddr[3]);

				mCastDataInfo.groupAddr[0]=ipAddr[0];
				mCastDataInfo.groupAddr[1]=ipAddr[1];
				mCastDataInfo.groupAddr[2]=ipAddr[2];
				mCastDataInfo.groupAddr[3]=ipAddr[3];

				tokptr = strsep(&strptr," ");
				if (tokptr==NULL)
				{
					goto errout;
				}
				
				cnt = sscanf(tokptr, "0x%x-%x-%x-%x", &ipAddr[0], &ipAddr[1], &ipAddr[2], &ipAddr[3]);

				mCastDataInfo.sourceIp[0]=ipAddr[0];
				mCastDataInfo.sourceIp[1]=ipAddr[1];
				mCastDataInfo.sourceIp[2]=ipAddr[2];
				mCastDataInfo.sourceIp[3]=ipAddr[3];
			}
#endif
			ret = rtl_getKernelMCastDataFwdInfo(&mCastDataInfo,&mCastFwdInfo);
			printk("ret:%d, fwdPortMask:%x, cpu:%d, wanOps:%d, wanPPP:%d\n", ret, mCastFwdInfo.fwdPortMask, mCastFwdInfo.cpuFlag, mCastFwdInfo.wanOps, mCastFwdInfo.wanPPP);
			
		}
		else
		{
	errout:
			printk("error input!\n");
		}	

	}
	return count;
}
#endif

