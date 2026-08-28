#ifndef _8192CD_CORE_TX_H_
#define _8192CD_CORE_TX_H_



#ifndef WLAN_HAL_INTERNAL_USED

#include "../8192cd_cfg.h"
#include "../8192cd.h"
#include "../8192cd_util.h"
#include "./8192cd_core_util.h"

#ifdef WDS
#define DECLARE_TXINSN(A)	struct tx_insn A; \
	do {	\
		memset(&A, 0, sizeof(struct tx_insn)); \
		A.wdsIdx  = -1; \
	} while (0)

#define DECLARE_TXCFG(P, TEMPLATE)	struct tx_insn *P = &(TEMPLATE); \
	do {	\
		memset(P, 0, sizeof(struct tx_insn)); \
		P->wdsIdx  = -1; \
	} while (0)

#else
#define DECLARE_TXINSN(A)	struct tx_insn A; \
	do {	\
		memset(&A, 0, sizeof(struct tx_insn)); \
	} while (0)

#define DECLARE_TXCFG(P, TEMPLATE)	struct tx_insn* P = &(TEMPLATE); \
	do {	\
		memset(P, 0, sizeof(struct tx_insn)); \
	} while (0)

#endif // WDS

#ifdef OSK_LOW_TX_DESC
static __inline__ void init_nonbe_txdesc(struct rtl8192cd_priv *priv, struct tx_desc *pdesc,
				unsigned long ringaddr, unsigned int i)
{
#ifdef CONFIG_RTL_8812_SUPPORT
	if(GET_CHIP_VER(priv)== VERSION_8812E){
		if (i == (NONBE_TXDESC - 1))
			(pdesc + i)->Dword12 = set_desc(ringaddr); // NextDescAddress
		else
			(pdesc + i)->Dword12 = set_desc(ringaddr + (i+1) * sizeof(struct tx_desc)); // NextDescAddress
	} else
#endif
	{
	if (i == (NONBE_TXDESC- 1))
		(pdesc + i)->Dword10 = set_desc(ringaddr); // NextDescAddress
	else
		(pdesc + i)->Dword10 = set_desc(ringaddr + (i+1) * sizeof(struct tx_desc)); // NextDescAddress
	}
}

#define txdesc_rollover_nonbe(ptxdesc, ptxhead)	(*ptxhead = (*ptxhead + 1) % NONBE_TXDESC)

#define txdesc_rollback_nonbe(ptxhead)			(*ptxhead = (*ptxhead == 0)? (NONBE_TXDESC - 1) : (*ptxhead - 1))

#define txdesc_rollover(ptxdesc, ptxhead)	(*ptxhead = (*ptxhead + 1) % BE_TXDESC)

#define txdesc_rollback(ptxhead)			(*ptxhead = (*ptxhead == 0)? (BE_TXDESC - 1) : (*ptxhead - 1))

#else
#define txdesc_rollover(ptxdesc, ptxhead)	(*ptxhead = (*ptxhead + 1) % CURRENT_NUM_TX_DESC)

#define txdesc_rollback(ptxhead)			(*ptxhead = (*ptxhead == 0)? (CURRENT_NUM_TX_DESC - 1) : (*ptxhead - 1))
#endif

static __inline__ unsigned int get_mpdu_len(struct tx_insn *txcfg, unsigned int fr_len)
{
	return (txcfg->hdr_len + txcfg->llc + txcfg->iv + txcfg->icv + txcfg->mic + _CRCLNG_ + fr_len);
}

#ifdef CONFIG_WLAN_HAL_8814BE
enum _TX_QUEUE_EXTEND_ {
    ACCHANNEL4      = 5,
    ACCHANNEL5      = 6,
    ACCHANNEL6      = 7,
    ACCHANNEL7      = 8,
    ACCHANNEL8      = 9,
    ACCHANNEL9      = 10,
    ACCHANNEL10     = 11,
    ACCHANNEL11     = 12,
    ACCHANNEL12     = 13,
    ACCHANNEL13     = 14,
    HIGH_QUEUE_V2   = 15,
    HIGH_QUEUE1_V2     = 16,
    HIGH_QUEUE2_V2     = 17,
    HIGH_QUEUE3_V2     = 18,
    HIGH_QUEUE4_V2     = 19,
    HIGH_QUEUE5_V2     = 20,
    HIGH_QUEUE6_V2     = 21,
    HIGH_QUEUE7_V2     = 22,
    HIGH_QUEUE8_V1     = 23,
    HIGH_QUEUE9_V1     = 24,
    HIGH_QUEUE10_V1    = 25,
    HIGH_QUEUE11_V1    = 26,
    HIGH_QUEUE12_V1    = 27,
    HIGH_QUEUE13_V1    = 28,
    HIGH_QUEUE14_V1    = 29,
    HIGH_QUEUE15_V1    = 30,
    HIGH_QUEUE16_V1    = 31,
    HIGH_QUEUE17_V1    = 32,
    HIGH_QUEUE18_V1    = 33,
    HIGH_QUEUE19_V1    = 34,
    CMD_QUEUE_V2       = 35,   
    FWCMD_QUEUE        = 36,
    BEACON_QUEUE_V2    = 37
};

#define MCAST_QNUM		HIGH_QUEUE_V2

#else
#define MCAST_QNUM		HIGH_QUEUE

#endif

enum _TX_QUEUE_ {
	MGNT_QUEUE		= 0,
	BK_QUEUE		= 1,
	BE_QUEUE		= 2,
	VI_QUEUE		= 3,
	VO_QUEUE		= 4,
	HIGH_QUEUE		= 5,
#if defined(CONFIG_PCI_HCI)
#if defined(CONFIG_WLAN_HAL)
	HIGH_QUEUE1		= 6,
	HIGH_QUEUE2		= 7,
	HIGH_QUEUE3		= 8,
	HIGH_QUEUE4		= 9,
	HIGH_QUEUE5		= 10,
	HIGH_QUEUE6		= 11,
	HIGH_QUEUE7		= 12,
	CMD_QUEUE		= 13,	
	BEACON_QUEUE	= 14,
#else
	BEACON_QUEUE	= 6,
#endif
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
	BEACON_QUEUE_USB	= 6,
	#else
	BEACON_QUEUE	= 6,
	#endif
	TXCMD_QUEUE	= 7,
	HW_QUEUE_ENTRY = 8
#endif
};


#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
#define TXDESC_SIZE 40

#define desc_copy(dst, src)		memcpy(dst, src, TXDESC_SIZE)

#define descinfo_copy(d, s)										\
	do {														\
		struct tx_desc_info	*dst = (struct tx_desc_info	*)d;	\
		struct tx_desc_info	*src = (struct tx_desc_info	*)s;	\
		dst->type  = src->type;									\
		dst->rate  = src->rate;									\
	} while (0)

#else /* !defined(CONFIG_RTL_TRIBAND_SUPPORT) */

#ifdef CONFIG_PCI_HCI
#define desc_copy(dst, src)		memcpy(dst, src, 32)
#ifdef TXDESC_INFO
#define descinfo_copy(d, s)										\
	do {														\
		struct tx_desc_info	*dst = (struct tx_desc_info	*)d;	\
		struct tx_desc_info	*src = (struct tx_desc_info	*)s;	\
		dst->type  = src->type;									\
	} while (0)
#else
#define descinfo_copy(d, s)										\
	do {														\
		struct tx_desc_info	*dst = (struct tx_desc_info	*)d;	\
		struct tx_desc_info	*src = (struct tx_desc_info	*)s;	\
		dst->type  = src->type;									\
		dst->len   = src->len;									\
		dst->rate  = src->rate;									\
	} while (0)
#endif
#endif
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
#define desc_copy(dst, src)		memcpy(dst, src, TXDESC_SIZE)

#define descinfo_copy(d, s)										\
	do {														\
		struct tx_desc_info	*dst = (struct tx_desc_info	*)d;	\
		struct tx_desc_info	*src = (struct tx_desc_info	*)s;	\
		dst->type  = src->type;									\
		dst->rate  = src->rate;									\
	} while (0)
#endif
#endif /* defined(CONFIG_RTL_TRIBAND_SUPPORT) */


#if !defined(TAROKO_0)
static __inline__ void tx_poll(struct rtl8192cd_priv *priv, int q_num)
{
	unsigned char val = 0;

#ifdef CONFIG_RTL8671
#ifdef CONFIG_CPU_RLX4181
	r3k_flush_dcache_range(0,0);
#endif
#endif

	switch (q_num) {
	case MGNT_QUEUE:
		val = MGQ_POLL;
		break;
	case BK_QUEUE:
		val = BKQ_POLL;
		break;
	case BE_QUEUE:
		val = BEQ_POLL;
		break;
	case VI_QUEUE:
		val = VIQ_POLL;
		break;
	case VO_QUEUE:
		val = VOQ_POLL;
		break;
	case HIGH_QUEUE:
		val = HQ_POLL;
		break;
	default:
		break;
	}
	RTL_W8(PCIE_CTRL_REG, val);
}
#endif



#endif
#define TX_NORMAL			0
#define TX_NO_MUL2UNI			1
#define TX_AMPDU_BUFFER_SIG		2
#define TX_AMPDU_BUFFER_FIRST		3
#define TX_AMPDU_BUFFER_MID		4
#define TX_AMPDU_BUFFER_LAST		5
#define TX_NO_TCPACK_ACC		6

#endif
