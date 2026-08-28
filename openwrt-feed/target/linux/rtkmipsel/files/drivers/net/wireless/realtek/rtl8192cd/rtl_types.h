/*
 *	The header file of realtek type definition
 *
 *	Copyright (c) 2017 Realtek Semiconductor Corp.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 */


#ifndef _RTL_TYPES_H
#define _RTL_TYPES_H

#define RTL_LAYERED_DRIVER_DEBUG 0

#if 0
#ifndef RTL865X_OVER_KERNEL
	#undef __KERNEL__
#endif

#ifndef RTL865X_OVER_LINUX
	#undef __linux__
#endif
#endif

/*
 * Internal names for basic integral types.  Omit the typedef if
 * not possible for a machine/compiler combination.
 */
#ifdef __linux__
#ifdef __KERNEL__
#include <linux/version.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
#include <linux/config.h>
#endif
//#include <linux/ctype.h>
#include <linux/module.h>
//#include <linux/string.h>

#endif /*__KERNEL__*/
#endif /*__linux__*/

/* ===============================================================================
		IRAM / DRAM definition
    =============================================================================== */
#undef __DRAM_GEN
#undef __DRAM_FWD
#undef __DRAM_L2_FWD
#undef __DRAM_L34_FWD
#undef __DRAM_EXTDEV
#undef __DRAM_AIRGO
#undef __DRAM_RTKWLAN
#undef __DRAM_CRYPTO
#undef __DRAM_VOIP
#undef __DRAM_TX
#undef __DRAM

#undef __IRAM_GEN
#undef __IRAM_FWD
#undef __IRAM_L2_FWD
#undef __IRAM_L34_FWD
#undef __IRAM_EXTDEV
#undef __IRAM_AIRGO
#undef __IRAM_RTKWLAN
#undef __IRAM_CRYPTO
#undef __IRAM_VOIP
#undef __IRAM_TX
#undef __IRAM

#if defined(__linux__)&&defined(__KERNEL__)&&defined(CONFIG_RTL_819X)&&!defined(CONFIG_RTL_8198C)
	#define __DRAM_GEN			__attribute__  ((section(".dram-gen")))
	#define __DRAM_FWD			__attribute__  ((section(".dram-fwd")))
	#define __DRAM_L2_FWD		__attribute__  ((section(".dram-l2-fwd")))
	#define __DRAM_L34_FWD	__attribute__  ((section(".dram-l34-fwd")))
	#define __DRAM_EXTDEV		__attribute__  ((section(".dram-extdev")))
	#define __DRAM_AIRGO		__attribute__  ((section(".dram-airgo")))
	#define __DRAM_RTKWLAN	__attribute__  ((section(".dram-rtkwlan")))
	#define __DRAM_CRYPTO		__attribute__  ((section(".dram-crypto")))
	#define __DRAM_VOIP			__attribute__  ((section(".dram-voip")))
	#define __DRAM_TX			__attribute__  ((section(".dram-tx")))
	#define __DRAM				__attribute__  ((section(".dram")))

	#define __IRAM_GEN			__attribute__  ((section(".iram-gen")))
	#define __IRAM_FWD			__attribute__  ((section(".iram-fwd")))
	#define __IRAM_L2_FWD		__attribute__  ((section(".iram-l2-fwd")))
	#define __IRAM_L34_FWD		__attribute__  ((section(".iram-l34-fwd")))
	#define __IRAM_EXTDEV		__attribute__  ((section(".iram-extdev")))
	#define __IRAM_AIRGO		__attribute__  ((section(".iram-airgo")))
	#define __IRAM_RTKWLAN		__attribute__  ((section(".iram-rtkwlan")))
	#define __IRAM_CRYPTO		__attribute__  ((section(".iram-crypto")))
	#define __IRAM_VOIP			__attribute__  ((section(".iram-voip")))
	#define __IRAM_TX			__attribute__  ((section(".iram-tx")))
	#define __IRAM				__attribute__  ((section(".iram")))
#else
	#define __DRAM_GEN
	#define __DRAM_FWD
	#define __DRAM_L2_FWD
	#define __DRAM_L34_FWD
	#define __DRAM_EXTDEV
	#define __DRAM_AIRGO
	#define __DRAM_RTKWLAN
	#define __DRAM_CRYPTO
	#define __DRAM_VOIP
	#define __DRAM_TX
	#define __DRAM

	#define __IRAM_GEN
	#define __IRAM_FWD
	#define __IRAM_L2_FWD
	#define __IRAM_L34_FWD
	#define __IRAM_EXTDEV
	#define __IRAM_AIRGO
	#define __IRAM_RTKWLAN
	#define __IRAM_CRYPTO
	#define __IRAM_VOIP
	#define __IRAM_TX
	#define __IRAM
#endif

#if defined(CONFIG_RTL_DYNAMIC_IRAM_MAPPING_FOR_WAPI) &&!defined(CONFIG_RTL_8198C)
	#define __IRAM_GEN_WAPI			__attribute__  ((section(".iram-gen-wapi")))
	#define __IRAM_FWD_WAPI			__attribute__  ((section(".iram-fwd-wapi")))
	#define __IRAM_L2_FWD_WAPI		__attribute__  ((section(".iram-l2-fwd-wapi")))
	#define __IRAM_L34_FWD_WAPI		__attribute__  ((section(".iram-l34-fwd-wapi")))
	#define __IRAM_RTKWLAN_WAPI		__attribute__  ((section(".iram-rtkwlan-wapi")))
	#define __IRAM_TX_WAPI			__attribute__  ((section(".iram-tx-wapi")))

	#define FUNCTION_CHECK(x)	do \
							{ \
								if((x)==NULL) \
									printk("---%s %s(%d) function is NULL!!\n",__FILE__,__FUNCTION__,__LINE__); \
							} while(0)
#endif


/* ===============================================================================
		Additional GCC attribute
    =============================================================================== */

#undef __NOMIPS16
#undef __MIPS16

#if defined(__linux__)&&defined(__KERNEL__)&&defined(CONFIG_RTL_819X) && !defined(CONFIG_RTL_8196C)
#ifndef CONFIG_WIRELESS_LAN_MODULE  /*mark_wrt eric-sync ??*/
	#define __NOMIPS16			__attribute__((nomips16))	/* Inidcate to prevent from MIPS16 */
	#define __MIPS16			__attribute__((mips16))		/* Inidcate to use MIPS16 */
#else
	#define __NOMIPS16
	#define __MIPS16
#endif
#else
	#define __NOMIPS16
	#define __MIPS16
#endif

/* ===============================================================================
		print macro
    =============================================================================== */
#if	defined(__linux__)&&defined(__KERNEL__)
	
	#define rtlglue_printf	panic_printk

#else	/* defined(__linux__)&&defined(__KERNEL__) */

#ifdef	RTL865X_TEST
	#include <ctype.h>
#endif	/* RTL865X_TEST */

#define rtlglue_printf	printf

#endif	/* defined(__linux__)&&defined(__KERNEL__) */

/* ===============================================================================
		Type definition
    =============================================================================== */
#if 1
typedef unsigned long long	uint64;
typedef signed long long	int64;
typedef unsigned int	uint32;

#ifdef int32
#undef int32
#endif
typedef signed int		int32;

typedef unsigned short	uint16;
typedef signed short	int16;
typedef unsigned char	uint8;
typedef signed char		int8;

#else
typedef __u64 uint64;
typedef __s64	int64;
typedef __u32	uint32;
#ifndef int32
typedef __s32			int32;
#endif
typedef __u16	uint16;
typedef __s16	int16;
typedef __u8		uint8;
typedef __s8			int8;
#endif

typedef uint32		memaddr;	
typedef uint32          ipaddr_t;
typedef struct {
    uint16      mac47_32;
    uint16      mac31_16;
    uint16      mac15_0;
} macaddr_t;

#define ETHER_ADDR_LEN				6
typedef struct ether_addr_s {
	uint8 octet[ETHER_ADDR_LEN];
} ether_addr_t;

#define RX_OFFSET	2
#define MBUF_LEN	1700
#define CROSS_LAN_MBUF_LEN		(MBUF_LEN+RX_OFFSET+10)

#if defined(CONFIG_RTL_819X)
	#if defined(CONFIG_RTL_ETH_PRIV_SKB)
	#define DELAY_REFILL_ETH_RX_BUF	1
	#endif
#endif

/* 
	CN SD6 Mantis issue #1085: NIC RX can't work correctly after runout.
	this bug still happened in RTL8196B
 */

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



#define DEBUG_P(args...) while(0);
#ifndef OK
#define OK		0
#endif
#ifndef NOT_OK
#define NOT_OK  1
#endif

#ifndef CLEARBITS
#define CLEARBITS(a,b)	((a) &= ~(b))
#endif

#ifndef SETBITS
#define SETBITS(a,b)		((a) |= (b))
#endif

#ifndef ISSET
#define ISSET(a,b)		(((a) & (b))!=0)
#endif

#ifndef ISCLEARED
#define ISCLEARED(a,b)	(((a) & (b))==0)
#endif

#ifndef max
#define max(a,b)  (((a) > (b)) ? (a) : (b))
#endif			   /* max */

#ifndef min
#define min(a,b)  (((a) < (b)) ? (a) : (b))
#endif			   /* min */

//round down x to multiple of y.  Ex: ROUNDDOWN(20, 7)=14
#ifndef ROUNDDOWN
#define	ROUNDDOWN(x, y)	(((x)/(y))*(y))
#endif

//round up x to multiple of y. Ex: ROUNDUP(11, 7) = 14
#ifndef ROUNDUP
#define	ROUNDUP(x, y)	((((x)+((y)-1))/(y))*(y))  /* to any y */
#endif

#ifndef ROUNDUP2
#define	ROUNDUP2(x, y)	(((x)+((y)-1))&(~((y)-1))) /* if y is powers of two */
#endif

#ifndef ROUNDUP4
#define	ROUNDUP4(x)		((1+(((x)-1)>>2))<<2)
#endif

#ifndef IS4BYTEALIGNED
#define IS4BYTEALIGNED(x)	 ((((x) & 0x3)==0)? 1 : 0)
#endif

#ifndef __offsetof
#define __offsetof(type, field) ((unsigned long)(&((type *)0)->field))
#endif

#ifndef offsetof
#define offsetof(type, field) __offsetof(type, field)
#endif

#ifndef RTL_PROC_CHECK
#define RTL_PROC_CHECK(expr, success) \
	do {\
			int __retval; \
			if ((__retval = (expr)) != (success))\
			{\
				rtlglue_printf("ERROR >>> [%s]:[%d] failed -- return value: %d\n", __FUNCTION__,__LINE__, __retval);\
				return __retval; \
			}\
		}while(0)
#endif

#ifndef RTL_STREAM_SAME
#define RTL_STREAM_SAME(s1, s2) \
	((strlen(s1) == strlen(s2)) && (strcmp(s1, s2) == 0))
#endif

#define ASSERT_ISR(x) if(!(x)) {while(1);}
#define RTL_STATIC_INLINE   static __inline__

#define ASSERT_CSP(x) if (!(x)) {rtlglue_printf("\nAssert Fail: %s %d", __FILE__, __LINE__); while(1);}
 

#if defined(RTL865X_TEST)||defined(RTL865X_MODEL_USER)
#define UNCACHE_MASK		0
#define UNCACHE(addr)		(addr)
#define CACHED(addr)			((uint32)(addr))
#else
#define UNCACHE_MASK		0x20000000
#define UNCACHE(addr)		((UNCACHE_MASK)|(uint32)(addr))
#define CACHED(addr)			((uint32)(addr) & ~(UNCACHE_MASK))
#endif

/*	asic configuration	*/
#define RTL8651_OUTPUTQUEUE_SIZE		6
#define TOTAL_VLAN_PRIORITY_NUM	8
#define RTL8651_RATELIMITTBL_SIZE			32

#if defined(CONFIG_RTL_8196C)
#define CONFIG_RTL8196C_ETH_IOT         1
#ifdef CONFIG_MP_PSD_SUPPORT
#undef CONFIG_RTL8196C_GREEN_ETHERNET
#else
//#define CONFIG_RTL_8196C_ESD            1 
#endif
#endif

#if defined(CONFIG_RTL_8198) && !defined(CONFIG_RTL_819XD)
#define CONFIG_RTL_8198_ESD        1
#endif 

#if defined(CONFIG_RTL_8198)
#define RTL8198_EEE_MAC 	1
#endif

#if defined(CONFIG_RTL_819XD) || defined(CONFIG_RTL_8196E)
#define CONFIG_RTL_8197D_DYN_THR		1
#endif

#define DYN_THR_LINK_UP_PORTS			3

/* IC default value */
#define DYN_THR_DEF_fcON				0xac
#define DYN_THR_DEF_fcOFF				0xa0
#define DYN_THR_DEF_sharedON			0x62
#define DYN_THR_DEF_sharedOFF			0x4a

/* aggressive value */
#define DYN_THR_AGG_fcON				0xd0
#define DYN_THR_AGG_fcOFF				0xc0
#define DYN_THR_AGG_sharedON			0xc0
#define DYN_THR_AGG_sharedOFF			0xa8

#if defined(CONFIG_RTL_LOG_DEBUG)
extern int scrlog_printk(const char * fmt, ...);

extern struct RTL_LOG_PRINT_MASK
{
	uint32 ERROR:1;
	uint32 WARN:1;
	uint32 INFO:1;
}RTL_LogTypeMask;

extern struct RTL_LOG_ERROR_MASK
{
	uint32 MEM:1;
	uint32 SKB:1;
}RTL_LogErrorMask;
extern uint32 RTL_LogRatelimit;

extern struct RTL_LOG_MODULE_MASK
{
	uint8 NIC:1;
	uint8 WIRELESS:1;
	uint8 PROSTACK:1;
}RTL_LogModuleMask;


#define LOG_LIMIT (!RTL_LogRatelimit||net_ratelimit())


#define LOG_ERROR(fmt, args...) do{ \
	if(RTL_LogTypeMask.ERROR&&LOG_LIMIT)scrlog_printk("ERROR:"fmt, ## args); \
		}while(0)
		
#define LOG_MEM_ERROR(fmt, args...) do{ \
	if(RTL_LogTypeMask.ERROR&&RTL_LogErrorMask.MEM&&LOG_LIMIT)scrlog_printk("ERROR:"fmt, ## args); \
		}while(0)
		
#define LOG_SKB_ERROR(fmt, args...) do{ \
		if(RTL_LogTypeMask.ERROR&&RTL_LogErrorMask.SKB&&LOG_LIMIT)scrlog_printk("ERROR:"fmt, ## args); \
			}while(0)
			
#define LOG_WARN(fmt, args...) do{ \
		if(RTL_LogTypeMask.WARN&&LOG_LIMIT)scrlog_printk("WARN:"fmt, ## args); \
			}while(0)
			
#define LOG_INFO(fmt, args...) do{ \
		if(RTL_LogTypeMask.INFO&&LOG_LIMIT)scrlog_printk("INFO:"fmt, ## args); \
			}while(0)

#else

#define LOG_ERROR(fmt, args...) 
#define LOG_MEM_ERROR(fmt, args...) 
#define LOG_SKB_ERROR(fmt, args...)
#define LOG_WARN(fmt, args...)
#define LOG_INFO(fmt, args...)

#endif 

#endif 


