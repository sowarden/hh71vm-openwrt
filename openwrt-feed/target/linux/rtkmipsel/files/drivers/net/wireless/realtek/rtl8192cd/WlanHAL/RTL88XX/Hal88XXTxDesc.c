/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	Hal88XXTxDesc.c

Abstract:
	Defined RTL88XX HAL tx desc common function

Major Change History:
	When            Who                         What
	---------- ---------------   -------------------------------
	2012-03-29  Lun-Wu Yeh            Add PrepareTxDesc88XX().
--*/

#if !defined(__ECOS) && !defined(CPTCFG_CFG80211_MODULE)
#include "HalPrecomp.h"
#else
#include "../HalPrecomp.h"
#endif
#if defined(__ECOS)
#include "halmac_api.h"
#endif

#ifdef __ECOS
#undef printk
#define printk	ecos_pr_fun
//typedef void pr_fun(char *fmt, ...);
extern pr_fun *ecos_pr_fun;

#if defined(CONFIG_RTL_OFFLOAD_DRIVER) && defined(CONFIG_WLAN_HAL_8814BE)
#define __ALIGN_MASK(x,mask)	(((x)+(mask))&~(mask))
#define ALIGN(x,a)		__ALIGN_MASK((x),(typeof(x))(a)-1)
#include "../core/WlanHAL/RTL88XX/RTL8814B/Hal8814HTxDesc_core.h"
#endif

#endif
#ifdef TRXBD_CACHABLE_REGION
#if !defined(__ECOS) && !defined(__OSK__)
#include <asm/cacheflush.h>
#endif
#endif

#ifdef CONFIG_RTL_PROC_NEW
#define PROC_PRINT(fmt, arg...)	seq_printf(s, fmt, ## arg)
#else
#define PROC_PRINT	printk
#endif


#if (HAL_DEV_BUS_TYPE & (HAL_RT_EMBEDDED_INTERFACE | HAL_RT_PCI_INTERFACE))
void DumpTxBDesc88XX(
    IN      HAL_PADAPTER    Adapter,
#ifdef CONFIG_RTL_PROC_NEW
    IN      struct seq_file *s,
#endif
    IN      u4Byte          q_num 
)
{
    u4Byte                      TXBD_RWPtr_Reg_CMDQ;
    u4Byte                      TXBD_Reg_CMDQ;

    // setting CMDQ register by chip
#if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE || IS_EXIST_RTL8814AE)
    if ( IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8192FE(Adapter) || IS_HARDWARE_TYPE_8814AE(Adapter)) {
        TXBD_RWPtr_Reg_CMDQ = 0;
        TXBD_Reg_CMDQ       = 0;
    }
#endif  //#if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM)
    
#if IS_EXIST_RTL8822BE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE 
        if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter))  {
            TXBD_RWPtr_Reg_CMDQ = REG_H2CQ_TXBD_IDX;
            TXBD_Reg_CMDQ       = REG_H2CQ_TXBD_DESA;
        }
#endif //IS_EXIST_RTL8822BE

#if IS_EXIST_RTL8881AEM
    u4Byte TXBD_NUM_8881A[HCI_TX_DMA_QUEUE_MAX_NUM] =
    {
        TX_MGQ_TXBD_NUM,
        TX_BKQ_TXBD_NUM, TX_BEQ_TXBD_NUM, TX_VIQ_TXBD_NUM, TX_VOQ_TXBD_NUM,
        TX_HI0Q_TXBD_NUM, TX_HI1Q_TXBD_NUM, TX_HI2Q_TXBD_NUM, TX_HI3Q_TXBD_NUM,
        TX_HI4Q_TXBD_NUM, TX_HI5Q_TXBD_NUM, TX_HI6Q_TXBD_NUM, TX_HI7Q_TXBD_NUM,
        TX_BCNQ_TXBD_NUM_V2
    };
#endif  //IS_EXIST_RTL8881AEM

#if (IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE || IS_EXIST_RTL8197GEM)
    u4Byte TXBD_NUM_V1[HCI_TX_DMA_QUEUE_MAX_NUM] =
    {
        TX_MGQ_TXBD_NUM,
        TX_BKQ_TXBD_NUM, TX_BEQ_TXBD_NUM, TX_VIQ_TXBD_NUM, TX_VOQ_TXBD_NUM,
        TX_HI0Q_TXBD_NUM, TX_HI1Q_TXBD_NUM, TX_HI2Q_TXBD_NUM, TX_HI3Q_TXBD_NUM,
        TX_HI4Q_TXBD_NUM, TX_HI5Q_TXBD_NUM, TX_HI6Q_TXBD_NUM, TX_HI7Q_TXBD_NUM,
        TX_BCNQ_TXBD_NUM_V1
    };
#endif // #if (IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822BE || IS_EXIST_RTL8197GEM)

#if (IS_EXIST_RTL8198FEM)  
    u4Byte TXBD_NUM_V2[HCI_TX_DMA_QUEUE_MAX_NUM_V1] =
    {    
        TX_MGQ_TXBD_NUM,
        TX_BKQ_TXBD_NUM, TX_BEQ_TXBD_NUM, TX_VIQ_TXBD_NUM, TX_VOQ_TXBD_NUM,
        TX_HI0Q_TXBD_NUM, TX_HI1Q_TXBD_NUM, TX_HI2Q_TXBD_NUM, TX_HI3Q_TXBD_NUM,
        TX_HI4Q_TXBD_NUM, TX_HI5Q_TXBD_NUM, TX_HI6Q_TXBD_NUM, TX_HI7Q_TXBD_NUM,
        TX_HI8Q_TXBD_NUM, TX_HI9Q_TXBD_NUM, TX_HI10Q_TXBD_NUM, TX_HI11Q_TXBD_NUM,
        TX_HI12Q_TXBD_NUM, TX_HI13Q_TXBD_NUM, TX_HI14Q_TXBD_NUM, TX_HI15Q_TXBD_NUM,
        TX_BCNQ_TXBD_NUM_V1
    };
#endif // (IS_EXIST_RTL8198FEM)

#if (IS_EXIST_RTL8814BE)
    u4Byte TXBD_NUM_V3[HCI_TX_DMA_QUEUE_MAX_NUM_V2] =
    {
        TX_MGQ_TXBD_NUM_8814B,   TX_BKQ_TXBD_NUM_8814B,   TX_BEQ_TXBD_NUM_8814B,   TX_VIQ_TXBD_NUM_8814B, 
        TX_VOQ_TXBD_NUM_8814B,   TX_ACH4_TXBD_NUM_8814B,    TX_ACH5_TXBD_NUM_8814B, TX_ACH6_TXBD_NUM_8814B,
        TX_ACH7_TXBD_NUM_8814B,   TX_ACH8_TXBD_NUM_8814B,   TX_ACH9_TXBD_NUM_8814B,   TX_ACH10_TXBD_NUM_8814B,
        TX_ACH11_TXBD_NUM_8814B,  TX_ACH12_TXBD_NUM_8814B,  TX_ACH13_TXBD_NUM_8814B,  TX_HI0Q_TXBD_NUM_8814B,  
        TX_HI1Q_TXBD_NUM_8814B,  TX_HI2Q_TXBD_NUM_8814B,    TX_HI3Q_TXBD_NUM_8814B,   TX_HI4Q_TXBD_NUM_8814B,  
        TX_HI5Q_TXBD_NUM_8814B,  TX_HI6Q_TXBD_NUM_8814B,    TX_HI7Q_TXBD_NUM_8814B,  TX_HI8Q_TXBD_NUM_8814B,  
        TX_HI9Q_TXBD_NUM_8814B,  TX_HI10Q_TXBD_NUM_8814B, TX_HI11Q_TXBD_NUM_8814B, TX_HI12Q_TXBD_NUM_8814B, 
        TX_HI13Q_TXBD_NUM_8814B, TX_HI14Q_TXBD_NUM_8814B, TX_HI15Q_TXBD_NUM_8814B, TX_BCNQ_TXBD_NUM_V1

    };
#endif  //IS_EXIST_RTL8814BE 
#if (IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822BE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE || IS_EXIST_RTL8197GEM)
    // TXBD_RWPtr_Reg: no entry for beacon queue, set NULL here
    u4Byte  TXBD_RWPtr_Reg[HCI_TX_DMA_QUEUE_MAX_NUM] =
    {
        REG_MGQ_TXBD_IDX,
        REG_BKQ_TXBD_IDX, REG_BEQ_TXBD_IDX, REG_VIQ_TXBD_IDX, REG_VOQ_TXBD_IDX,
        REG_HI0Q_TXBD_IDX, REG_HI1Q_TXBD_IDX, REG_HI2Q_TXBD_IDX, REG_HI3Q_TXBD_IDX,
        REG_HI4Q_TXBD_IDX, REG_HI5Q_TXBD_IDX, REG_HI6Q_TXBD_IDX, REG_HI7Q_TXBD_IDX,        
        TXBD_RWPtr_Reg_CMDQ,0
    };

    u4Byte  TXBD_Reg[HCI_TX_DMA_QUEUE_MAX_NUM] =
    {
        REG_MGQ_TXBD_DESA,
        REG_BKQ_TXBD_DESA, REG_BEQ_TXBD_DESA, REG_VIQ_TXBD_DESA, REG_VOQ_TXBD_DESA,
        REG_HI0Q_TXBD_DESA, REG_HI1Q_TXBD_DESA, REG_HI2Q_TXBD_DESA, REG_HI3Q_TXBD_DESA,
        REG_HI4Q_TXBD_DESA, REG_HI5Q_TXBD_DESA, REG_HI6Q_TXBD_DESA, REG_HI7Q_TXBD_DESA,
        TXBD_Reg_CMDQ,REG_BCNQ_TXBD_DESA
    };
#endif //#if (IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822BE || IS_EXIST_RTL8197GEM)

#if IS_EXIST_RTL8198FEM
    // TXBD_RWPtr_Reg: no entry for beacon queue, set NULL here

    u4Byte  TXBD_RWPtr_Reg_V1[HCI_TX_DMA_QUEUE_MAX_NUM_V1] =
    {
        REG_MGQ_TXBD_IDX,
        REG_BKQ_TXBD_IDX, REG_BEQ_TXBD_IDX, REG_VIQ_TXBD_IDX, REG_VOQ_TXBD_IDX,
        REG_HI0Q_TXBD_IDX, REG_HI1Q_TXBD_IDX, REG_HI2Q_TXBD_IDX, REG_HI3Q_TXBD_IDX,
        REG_HI4Q_TXBD_IDX, REG_HI5Q_TXBD_IDX, REG_HI6Q_TXBD_IDX, REG_HI7Q_TXBD_IDX,        
        REG_HI8Q_TXBD_IDX, REG_HI9Q_TXBD_IDX, REG_HI10Q_TXBD_IDX, REG_HI11Q_TXBD_IDX,
        REG_HI12Q_TXBD_IDX, REG_HI13Q_TXBD_IDX, REG_HI14Q_TXBD_IDX, REG_HI15Q_TXBD_IDX, 
        0
    };

    u4Byte  TXBD_Reg_V1[HCI_TX_DMA_QUEUE_MAX_NUM_V1] =
    {
        REG_MGQ_TXBD_DESA,
        REG_BKQ_TXBD_DESA, REG_BEQ_TXBD_DESA, REG_VIQ_TXBD_DESA, REG_VOQ_TXBD_DESA,
        REG_HI0Q_TXBD_DESA, REG_HI1Q_TXBD_DESA, REG_HI2Q_TXBD_DESA, REG_HI3Q_TXBD_DESA,
        REG_HI4Q_TXBD_DESA, REG_HI5Q_TXBD_DESA, REG_HI6Q_TXBD_DESA, REG_HI7Q_TXBD_DESA,
        REG_HI8Q_TXBD_DESA, REG_HI9Q_TXBD_DESA, REG_HI10Q_TXBD_DESA, REG_HI11Q_TXBD_DESA,
        REG_HI12Q_TXBD_DESA, REG_HI13Q_TXBD_DESA, REG_HI14Q_TXBD_DESA, REG_HI15Q_TXBD_DESA,
        REG_BCNQ_TXBD_DESA
    };
#endif  //IS_EXIST_RTL8198FEM

#if (IS_EXIST_RTL8814BE)
    
    // TXBD_RWPtr_Reg: no entry for beacon queue, set NULL here
    u4Byte  TXBD_RWPtr_Reg_V2[HCI_TX_DMA_QUEUE_MAX_NUM_V2] =
    {
            REG_P0MGQ_TXBD_IDX,
            REG_ACH3_TXBD_IDX, REG_ACH2_TXBD_IDX, REG_ACH1_TXBD_IDX, REG_ACH0_TXBD_IDX,
            REG_ACH4_TXBD_IDX, REG_ACH5_TXBD_IDX, REG_ACH6_TXBD_IDX, REG_ACH7_TXBD_IDX,
            REG_ACH8_TXBD_IDX, REG_ACH9_TXBD_IDX, REG_ACH10_TXBD_IDX, REG_ACH11_TXBD_IDX,
            REG_ACH12_TXBD_IDX, REG_ACH13_TXBD_IDX,
            REG_P0HI0Q_TXBD_IDX, REG_P0HI1Q_TXBD_IDX, REG_P0HI2Q_TXBD_IDX, REG_P0HI3Q_TXBD_IDX,
            REG_P0HI4Q_TXBD_IDX, REG_P0HI5Q_TXBD_IDX, REG_P0HI6Q_TXBD_IDX, REG_P0HI7Q_TXBD_IDX,        
            REG_P0HI8Q_TXBD_IDX, REG_P0HI9Q_TXBD_IDX, REG_P0HI10Q_TXBD_IDX, REG_P0HI11Q_TXBD_IDX,
            REG_P0HI12Q_TXBD_IDX, REG_P0HI13Q_TXBD_IDX, REG_P0HI14Q_TXBD_IDX, REG_P0HI15Q_TXBD_IDX, 
            0
        };

    u4Byte  TXBD_Reg_H[HCI_TX_DMA_QUEUE_MAX_NUM_V2] =
    {
            REG_P0MGQ_TXBD_DESA_H,
            REG_ACH3_TXBD_DESA_H, REG_ACH2_TXBD_DESA_H, REG_ACH1_TXBD_DESA_H, REG_ACH0_TXBD_DESA_H,
            REG_ACH4_TXBD_DESA_H, REG_ACH5_TXBD_DESA_H, REG_ACH6_TXBD_DESA_H, REG_ACH7_TXBD_DESA_H,
            REG_ACH8_TXBD_DESA_H, REG_ACH9_TXBD_DESA_H, REG_ACH10_TXBD_DESA_H, REG_ACH11_TXBD_DESA_H,
            REG_ACH12_TXBD_DESA_H, REG_ACH13_TXBD_DESA_H,
            REG_HI0Q_TXBD_DESA_H, REG_HI1Q_TXBD_DESA_H, REG_HI2Q_TXBD_DESA_H, REG_HI3Q_TXBD_DESA_H,
            REG_HI4Q_TXBD_DESA_H, REG_HI5Q_TXBD_DESA_H, REG_HI6Q_TXBD_DESA_H, REG_HI7Q_TXBD_DESA_H,
            REG_HI8Q_TXBD_DESA_H, REG_HI9Q_TXBD_DESA_H, REG_HI10Q_TXBD_DESA_H, REG_HI11Q_TXBD_DESA_H,
            REG_HI12Q_TXBD_DESA_H, REG_HI13Q_TXBD_DESA_H, REG_HI14Q_TXBD_DESA_H, REG_HI15Q_TXBD_DESA_H,
            REG_P0BCNQ_TXBD_DESA_H
        };

    u4Byte  TXBD_Reg_L[HCI_TX_DMA_QUEUE_MAX_NUM_V2] =
    {
            REG_P0MGQ_TXBD_DESA_L,
            REG_ACH3_TXBD_DESA_L, REG_ACH2_TXBD_DESA_L, REG_ACH1_TXBD_DESA_L, REG_ACH0_TXBD_DESA_L,
            REG_ACH4_TXBD_DESA_L, REG_ACH5_TXBD_DESA_L, REG_ACH6_TXBD_DESA_L, REG_ACH7_TXBD_DESA_L,
            REG_ACH8_TXBD_DESA_L, REG_ACH9_TXBD_DESA_L, REG_ACH10_TXBD_DESA_L, REG_ACH11_TXBD_DESA_L,
            REG_ACH12_TXBD_DESA_L, REG_ACH13_TXBD_DESA_L,  
            REG_HI0Q_TXBD_DESA_L, REG_HI1Q_TXBD_DESA_L, REG_HI2Q_TXBD_DESA_L, REG_HI3Q_TXBD_DESA_L,
            REG_HI4Q_TXBD_DESA_L, REG_HI5Q_TXBD_DESA_L, REG_HI6Q_TXBD_DESA_L, REG_HI7Q_TXBD_DESA_L,
            REG_HI8Q_TXBD_DESA_L, REG_HI9Q_TXBD_DESA_L, REG_HI10Q_TXBD_DESA_L, REG_HI11Q_TXBD_DESA_L,
            REG_HI12Q_TXBD_DESA_L, REG_HI13Q_TXBD_DESA_L, REG_HI14Q_TXBD_DESA_L, REG_HI15Q_TXBD_DESA_L,
            REG_P0BCNQ_TXBD_DESA_L
        };
#endif

	PHCI_TX_DMA_MANAGER_88XX    ptx_dma         = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);;
	int                         i               = 0;
	PTX_DESC_88XX               ptx_desc_head   = ptx_dma->tx_queue[q_num].ptx_desc_head;
	PTX_BUFFER_DESCRIPTOR       ptxbd = ptx_dma->tx_queue[q_num].pTXBD_head;
    pu4Byte                     pTXBD_NUM = NULL;
	pu4Byte                     pTXBD_RWPtr_Reg = NULL;
	pu4Byte                     pTXBD_Reg = NULL;

#ifdef NOT_RTK_BSP
	if ((NULL == ptx_desc_head) || (NULL == ptxbd))
		return;
#endif

#if IS_EXIST_RTL8881AEM
    if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
        pTXBD_NUM           = TXBD_NUM_8881A;
		pTXBD_RWPtr_Reg		= TXBD_RWPtr_Reg;
		pTXBD_Reg			= TXBD_Reg;
    }
#endif  //IS_EXIST_RTL8881AEM

#if (IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE || IS_EXIST_RTL8197GEM)
    if ( IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8192FE(Adapter) || IS_HARDWARE_TYPE_8814AE(Adapter)|| IS_HARDWARE_TYPE_8197F(Adapter)|| IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter)) {
        pTXBD_NUM           = TXBD_NUM_V1;
		pTXBD_RWPtr_Reg		= TXBD_RWPtr_Reg;
		pTXBD_Reg			= TXBD_Reg;		
    }
#endif //#if (IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822BE || IS_EXIST_RTL8197GEM)

#if (IS_EXIST_RTL8198FEM)
	if (IS_HARDWARE_TYPE_8198F(Adapter)) {
        pTXBD_NUM           = TXBD_NUM_V2;
		pTXBD_RWPtr_Reg		= TXBD_RWPtr_Reg_V1;
		pTXBD_Reg			= TXBD_Reg_V1;	
    }
#endif //#if (IS_EXIST_RTL8198FEM)

#if IS_EXIST_RTL8814BE
    if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
        pTXBD_NUM           = TXBD_NUM_V3;
		pTXBD_RWPtr_Reg		= TXBD_RWPtr_Reg_V2;
		pTXBD_Reg			= TXBD_Reg_L;			
    }
#endif  //IS_EXIST_RTL8814BE
	PROC_PRINT("q_num:%d, hw_idx/host_idx= %d/%d\n", q_num, ptx_dma->tx_queue[q_num].hw_idx, ptx_dma->tx_queue[q_num].host_idx);

	PROC_PRINT("total_txbd_num=%d,avail_txbd_num= %d,reg_rwptr_idx:%x\n",
		ptx_dma->tx_queue[q_num].total_txbd_num, ptx_dma->tx_queue[q_num].avail_txbd_num, ptx_dma->tx_queue[q_num].reg_rwptr_idx);

	PROC_PRINT("RWreg(%x):%08x\n", pTXBD_RWPtr_Reg[q_num], HAL_RTL_R32(pTXBD_RWPtr_Reg[q_num]));


#ifdef CONFIG_NET_PCI
	if (HAL_IS_PCIBIOS_TYPE(Adapter)) {
#if IS_EXIST_RTL8814BE
    if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
        PROC_PRINT("pTXBD_head=%p, %08lx, regH(%x):%08x , regL(%x):%08x\n",
			ptx_dma->tx_queue[q_num].pTXBD_head , 
			_GET_HAL_DATA(Adapter)->txBD_dma_ring_addr[q_num],
			TXBD_Reg_H[q_num], HAL_RTL_R32(TXBD_Reg_H[q_num]), TXBD_Reg_L[q_num], HAL_RTL_R32(TXBD_Reg_L[q_num]));
    }
#endif
#if IS_RTL88XX_MAC_V1_V2_V3
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3) {
		PROC_PRINT("pTXBD_head=%p, %08lx, reg(%x):%08x\n",
			ptx_dma->tx_queue[q_num].pTXBD_head , 
			_GET_HAL_DATA(Adapter)->txBD_dma_ring_addr[q_num],
			TXBD_Reg[q_num], HAL_RTL_R32(TXBD_Reg[q_num]));
    }
#endif
		for (i=0;i<pTXBD_NUM[q_num];i++ ){
			PROC_PRINT("ptxbd[%d], addr:%p,%08lx\n[0] 0x%08x 0x%08x [1] 0x%08x 0x%08x\n",
				 i,
				&ptxbd[i],
				_GET_HAL_DATA(Adapter)->txBD_dma_ring_addr[q_num] + i * sizeof(TX_BUFFER_DESCRIPTOR),
				(u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[0].Dword0), 
				(u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[0].Dword1),
				(u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[1].Dword0),
				(u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[1].Dword1)  );
			PROC_PRINT("[2] 0x%08x 0x%08x [3] 0x%08x 0x%08x\n",
				(u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[2].Dword0), 
				(u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[2].Dword1),
				(u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[3].Dword0),
				(u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[3].Dword1)  );

			PROC_PRINT("ptx_desc_head[%d], addr:%p,%08lx\n",
				i,
				&ptx_desc_head[i],
				_GET_HAL_DATA(Adapter)->txDesc_dma_ring_addr[q_num] + i * sizeof(TX_DESC_88XX));
			PROC_PRINT("%08x %08x %08x %08x ",  (u4Byte)GET_DESC(ptx_desc_head[i].Dword0),  (u4Byte)GET_DESC(ptx_desc_head[i].Dword1),  (u4Byte)GET_DESC(ptx_desc_head[i].Dword2),  (u4Byte)GET_DESC(ptx_desc_head[i].Dword3));
			PROC_PRINT("%08x %08x %08x %08x ",  (u4Byte)GET_DESC(ptx_desc_head[i].Dword4), (u4Byte)GET_DESC( ptx_desc_head[i].Dword5),  (u4Byte)GET_DESC(ptx_desc_head[i].Dword6),  (u4Byte)GET_DESC(ptx_desc_head[i].Dword7));
			PROC_PRINT("%08x %08x\n", (u4Byte)GET_DESC(ptx_desc_head[i].Dword8),  (u4Byte)GET_DESC(ptx_desc_head[i].Dword9));
		}
	}else 
#endif
	{
#if IS_RTL88XX_MAC_V4
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4) {
        PROC_PRINT("pTXBD_head=%p, %08lx, regH(%x):%08x, regL(%x):%08x\n",
			ptx_dma->tx_queue[q_num].pTXBD_head , 
			HAL_VIRT_TO_BUS1(Adapter, (PVOID)ptx_dma->tx_queue[q_num].pTXBD_head,sizeof(TX_BUFFER_DESCRIPTOR) * pTXBD_NUM[q_num], HAL_PCI_DMA_TODEVICE),
			TXBD_Reg_H[q_num], HAL_RTL_R32(TXBD_Reg_H[q_num]),TXBD_Reg_L[q_num], HAL_RTL_R32(TXBD_Reg_L[q_num]));
    }        
#endif	//IS_RTL88XX_MAC_V4
#if IS_RTL88XX_MAC_V1_V2_V3
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3) {
		PROC_PRINT("pTXBD_head=%p, %08lx, reg(%x):%08x\n",
			ptx_dma->tx_queue[q_num].pTXBD_head , 
			HAL_VIRT_TO_BUS1(Adapter, (PVOID)ptx_dma->tx_queue[q_num].pTXBD_head,sizeof(TX_BUFFER_DESCRIPTOR) * pTXBD_NUM[q_num], HAL_PCI_DMA_TODEVICE),
		pTXBD_Reg[q_num], HAL_RTL_R32(pTXBD_Reg[q_num]));
    }
#endif    
		if (q_num != HCI_TX_DMA_QUEUE_BCN) {
#if IS_RTL88XX_MAC_V1_V2_V3
		if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3) {
			PROC_PRINT("pTXBD_head=%p, %08lx, reg(%x):%08x\n",
			ptx_dma->tx_queue[q_num].pTXBD_head , 
			HAL_VIRT_TO_BUS1(Adapter, (PVOID)ptx_dma->tx_queue[q_num].pTXBD_head,sizeof(TX_BUFFER_DESCRIPTOR) * pTXBD_NUM[q_num], HAL_PCI_DMA_TODEVICE),
			pTXBD_Reg[q_num], HAL_RTL_R32(pTXBD_Reg[q_num]));
		}
#endif

			for (i=0;i<pTXBD_NUM[q_num];i++ ){
				PROC_PRINT("ptxbd[%d], addr:%08x,%08x: Dword0: 0x%x, Dword1: 0x%x, [1] 0x%x 0x%x,\n",
                    i,
                    (u4Byte)&ptxbd[i],
                    (u4Byte)HAL_VIRT_TO_BUS1(Adapter, (PVOID)&ptxbd[i],sizeof(TX_BUFFER_DESCRIPTOR), HAL_PCI_DMA_TODEVICE),
                    (u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[0].Dword0), 
                    (u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[0].Dword1),
                    (u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[1].Dword0),
                    (u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[1].Dword1)  );
				PROC_PRINT("[2]: Dword0: 0x%x, Dword1: 0x%x, [3] 0x%x 0x%x,\n",
                    (u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[2].Dword0), 
                    (u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[2].Dword1),
                    (u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[3].Dword0),
                    (u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[3].Dword1)  );

				PROC_PRINT("ptx_desc_head[%03d],",  i  );
				PROC_PRINT("%08x %08x %08x %08x ",  
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword0),  
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword1), 
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword2),  
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword3));
				PROC_PRINT("%08x %08x %08x %08x ",  
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword4),  
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword5),  
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword6),  
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword7));
				PROC_PRINT("%08x %08x %08x\n", 
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword8),  
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword9),
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword10));
			}
		} else {
		    u4Byte beacon_offset = 128;
#if IS_RTL8881A_SERIES
			if (IS_HARDWARE_TYPE_8881A(Adapter)) {
				beacon_offset = TXBD_BEACON_OFFSET_V2;
			}
#endif
#if IS_RTL8192E_SERIES || IS_RTL8192F_SERIES || IS_RTL8814A_SERIES || IS_RTL8197F_SERIES || IS_RTL8822B_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES|| IS_RTL8197G_SERIES
			if (IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8192F(Adapter) || IS_HARDWARE_TYPE_8814A(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter)|| IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)|| IS_HARDWARE_TYPE_8197G(Adapter)) {
				beacon_offset = TXBD_BEACON_OFFSET_V1;
			}
#endif
#if IS_RTL88XX_MAC_V1_V2_V3
		if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3) {
			PROC_PRINT("beacon pTXBD_head=%p, %08lx, reg(%x):%08x\n",
			        ptx_dma->tx_queue[q_num].pTXBD_head , HAL_VIRT_TO_BUS(ptx_dma->tx_queue[q_num].pTXBD_head),
			        pTXBD_Reg[q_num], HAL_RTL_R32(pTXBD_Reg[q_num]));
		}
#endif
			for (i = 0; i < 1+HAL_NUM_VWLAN; i++) {
				PROC_PRINT("ptxbd[%d], addr:%08x,%08x: Dword0: 0x%x, Dword1: 0x%x, [1] 0x%x 0x%x,\n",
                    i,
                    (u4Byte)ptxbd,
                    (u4Byte)HAL_VIRT_TO_BUS(ptxbd),
                    (u4Byte)GET_DESC(ptxbd->TXBD_ELE[0].Dword0), 
                    (u4Byte)GET_DESC(ptxbd->TXBD_ELE[0].Dword1),
                    (u4Byte)GET_DESC(ptxbd->TXBD_ELE[1].Dword0),
                    (u4Byte)GET_DESC(ptxbd->TXBD_ELE[1].Dword1)  );
				PROC_PRINT("[2]: Dword0: 0x%x, Dword1: 0x%x, [3] 0x%x 0x%x,\n",
                    (u4Byte)GET_DESC(ptxbd->TXBD_ELE[2].Dword0), 
                    (u4Byte)GET_DESC(ptxbd->TXBD_ELE[2].Dword1),
                    (u4Byte)GET_DESC(ptxbd->TXBD_ELE[3].Dword0),
                    (u4Byte)GET_DESC(ptxbd->TXBD_ELE[3].Dword1)  );

				PROC_PRINT("ptx_desc_head[%03d],",  i  );
				PROC_PRINT("%08x %08x %08x %08x ",  
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword0),  
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword1), 
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword2),  
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword3));
				PROC_PRINT("%08x %08x %08x %08x ",  
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword4),  
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword5),  
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword6),  
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword7));
				PROC_PRINT("%08x %08x %08x\n", 
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword8),  
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword9),
					(u4Byte)GET_DESC(ptx_desc_head[i].Dword10));
				ptxbd = (PTX_BUFFER_DESCRIPTOR)((pu1Byte)ptxbd + beacon_offset);
            }
		}
	}
	PROC_PRINT("\n");
}

#if 0//def __ECOS by taro
#undef printk
#endif


#if IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE || IS_EXIST_RTL8881AEM || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822BE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE 
// TODO: 
//Note: PrepareTXBD88XX is necessary to be done after calling PrepareRXBD88XX
enum rt_status
PrepareTXBD88XX(
    IN      HAL_PADAPTER Adapter
)
{    
    PHCI_RX_DMA_MANAGER_88XX    prx_dma;
    PHCI_TX_DMA_MANAGER_88XX    ptx_dma;
    PTX_BUFFER_DESCRIPTOR       ptxbd_head;
    PTX_DESC_88XX               ptx_desc_head;
    PTX_BUFFER_DESCRIPTOR       ptxbd;
    
    PTX_BUFFER_DESCRIPTOR       ptxbd_bcn_head;
    PTX_DESC_88XX               ptxdesc_bcn_head = NULL;
    PTX_BUFFER_DESCRIPTOR       ptxbd_bcn_cur;    

    PTX_DESC_88XX               ptx_desc;

    
    u4Byte                      q_num;
    pu4Byte                     pTXBD_NUM = NULL;
    u4Byte                      i;
    u4Byte                      beacon_offset;
    u4Byte                      TotalTXBDNum_NoBcn;
    u4Byte                      TXDESCSize;
    u4Byte                      HCI_TX_DMA_QUEUE_MAX;
    u4Byte                      TXBD_RWPtr_Reg_CMDQ;
    u4Byte                      TXBD_Reg_CMDQ;    

    //HCI_TX_DMA_QUEUE_MAX = HCI_TX_DMA_QUEUE_MAX_NUM;
        
#if CFG_HAL_TX_AMSDU
    pu1Byte                         pdesc_dma_buf_amsdu, desc_dma_buf_start_amsdu;
    PHCI_TX_AMSDU_DMA_MANAGER_88XX  ptx_dma_amsdu;
    PTX_BUFFER_DESCRIPTOR_AMSDU     ptxbd_head_amsdu;
#endif

#ifdef WLAN_SUPPORT_H2C_PACKET
    PH2C_PAYLOAD_88XX   h2c_buf_start = (PH2C_PAYLOAD_88XX)_GET_HAL_DATA(Adapter)->h2c_buf;
    //pu1Byte  h2c_buf_start = _GET_HAL_DATA(Adapter)->h2c_buf;
    PTX_DESC_88XX               ph2c_tx_desc;
#endif //WLAN_SUPPORT_H2C_PACKET


#if IS_EXIST_RTL8881AEM
    u4Byte TXBD_NUM_8881A[HCI_TX_DMA_QUEUE_MAX_NUM] =
    {
        TX_MGQ_TXBD_NUM,
        TX_BKQ_TXBD_NUM, TX_BEQ_TXBD_NUM, TX_VIQ_TXBD_NUM, TX_VOQ_TXBD_NUM,
        TX_HI0Q_TXBD_NUM, TX_HI1Q_TXBD_NUM, TX_HI2Q_TXBD_NUM, TX_HI3Q_TXBD_NUM,
        TX_HI4Q_TXBD_NUM, TX_HI5Q_TXBD_NUM, TX_HI6Q_TXBD_NUM, TX_HI7Q_TXBD_NUM,
        TX_CMDQ_TXBD_NUM, TX_BCNQ_TXBD_NUM_V2
    };
#endif  //IS_EXIST_RTL8881AEM

#if (IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8822CE  || IS_EXIST_RTL8812FE )
    u4Byte TXBD_NUM_V1[HCI_TX_DMA_QUEUE_MAX_NUM] =
    {
        TX_MGQ_TXBD_NUM,
        TX_BKQ_TXBD_NUM, TX_BEQ_TXBD_NUM, TX_VIQ_TXBD_NUM, TX_VOQ_TXBD_NUM,
        TX_HI0Q_TXBD_NUM, TX_HI1Q_TXBD_NUM, TX_HI2Q_TXBD_NUM, TX_HI3Q_TXBD_NUM,
        TX_HI4Q_TXBD_NUM, TX_HI5Q_TXBD_NUM, TX_HI6Q_TXBD_NUM, TX_HI7Q_TXBD_NUM,
        TX_CMDQ_TXBD_NUM, TX_BCNQ_TXBD_NUM_V1
    };
#endif  //#if (IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE)

    // setting CMDQ register by chip
#if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE || IS_EXIST_RTL8814AE)
        if ( IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8192FE(Adapter) || IS_HARDWARE_TYPE_8814AE(Adapter)) {
            TXBD_RWPtr_Reg_CMDQ = 0;
            TXBD_Reg_CMDQ       = 0;
        }
#endif  //#if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM)
    
#if IS_EXIST_RTL8822BE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8821CE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE
        if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter))  {
            TXBD_RWPtr_Reg_CMDQ = REG_H2CQ_TXBD_IDX;
            TXBD_Reg_CMDQ       = REG_H2CQ_TXBD_DESA;
        }
#endif //IS_EXIST_RTL8822BE || IS_EXIST_RTL8197FEM 


    // TXBD_RWPtr_Reg: no entry for beacon queue, set NULL here
    u4Byte  TXBD_RWPtr_Reg[HCI_TX_DMA_QUEUE_MAX_NUM] =
    {
        REG_MGQ_TXBD_IDX,
        REG_BKQ_TXBD_IDX, REG_BEQ_TXBD_IDX, REG_VIQ_TXBD_IDX, REG_VOQ_TXBD_IDX,
        REG_HI0Q_TXBD_IDX, REG_HI1Q_TXBD_IDX, REG_HI2Q_TXBD_IDX, REG_HI3Q_TXBD_IDX,
        REG_HI4Q_TXBD_IDX, REG_HI5Q_TXBD_IDX, REG_HI6Q_TXBD_IDX, REG_HI7Q_TXBD_IDX,        
        TXBD_RWPtr_Reg_CMDQ,0
    };

    u4Byte  TXBD_Reg[HCI_TX_DMA_QUEUE_MAX_NUM] =
    {
        REG_MGQ_TXBD_DESA,
        REG_BKQ_TXBD_DESA, REG_BEQ_TXBD_DESA, REG_VIQ_TXBD_DESA, REG_VOQ_TXBD_DESA,
        REG_HI0Q_TXBD_DESA, REG_HI1Q_TXBD_DESA, REG_HI2Q_TXBD_DESA, REG_HI3Q_TXBD_DESA,
        REG_HI4Q_TXBD_DESA, REG_HI5Q_TXBD_DESA, REG_HI6Q_TXBD_DESA, REG_HI7Q_TXBD_DESA,
        TXBD_Reg_CMDQ,REG_BCNQ_TXBD_DESA
    };

#if IS_EXIST_RTL8881AEM
    if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
        pTXBD_NUM           = TXBD_NUM_8881A;
        TotalTXBDNum_NoBcn  = TOTAL_NUM_TXBD_NO_BCN;
    }
#endif  //IS_EXIST_RTL8881AEM

#if (IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE)
    if ( IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8192FE(Adapter) || IS_HARDWARE_TYPE_8814AE(Adapter) ||  IS_HARDWARE_TYPE_8197F(Adapter) ||IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)) {
        pTXBD_NUM           = TXBD_NUM_V1;
        TotalTXBDNum_NoBcn  = TOTAL_NUM_TXBD_NO_BCN;        
    }
#endif  //(IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE)

// setting TXDESC size by chip
#if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM)
    if ( IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8192FE(Adapter) || IS_HARDWARE_TYPE_8814AE(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter) ) {
        TXDESCSize = SIZE_TXDESC_88XX;
    }
#endif  //#if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM)

#if (IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE) 
    if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter))  {
        TXDESCSize = SIZE_TXDESC_88XX_V1;
    }
#endif // (IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE)



#if 0
#if IS_EXIST_RTL8192EE
    if ( IS_HARDWARE_TYPE_8192EE(Adapter) ) {
        pTXBD_NUM           = TXBD_NUM_8192E;
        TotalTXBDNum_NoBcn  = TOTAL_NUM_TXBD_NO_BCN;        
    }
#endif  //IS_EXIST_RTL8192EE

#if IS_EXIST_RTL8814AE
    if ( IS_HARDWARE_TYPE_8814AE(Adapter) ) {
        pTXBD_NUM           = TXBD_NUM_8814AE;
        TotalTXBDNum_NoBcn  = TOTAL_NUM_TXBD_NO_BCN;        
    }
#endif  //IS_EXIST_RTL8814AE

#if IS_EXIST_RTL8197FEM
    if ( IS_HARDWARE_TYPE_8197F(Adapter) ) {
        pTXBD_NUM           = TXBD_NUM_8814AE;
        TotalTXBDNum_NoBcn  = TOTAL_NUM_TXBD_NO_BCN;        
    }
#endif  //IS_EXIST_RTL8814AE

#if IS_EXIST_RTL8822BE
    if ( IS_HARDWARE_TYPE_8822(Adapter) ) {
        pTXBD_NUM           = TXBD_NUM_8814AE;
        TotalTXBDNum_NoBcn  = TOTAL_NUM_TXBD_NO_BCN;        
    }
#endif  //IS_EXIST_RTL8814AE
#endif // if 0

    prx_dma         = (PHCI_RX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PRxDMA88XX);
    ptx_dma         = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);

#if CFG_HAL_TX_AMSDU
    ptx_dma_amsdu   = (PHCI_TX_AMSDU_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMAAMSDU88XX);
#endif

#if IS_EXIST_RTL8192FE
	if (GET_CHIP_VER(Adapter) == VERSION_8192F) {
		HAL_RTL_W32(0x39c, 0x3fffffff);
	}
#endif

#ifdef CONFIG_NET_PCI
    unsigned long tmp_tx_dma_ring_addr =0, tmp_tx_dma_ring_addr2=0;
    unsigned long tmp_tx_dma_ring_addr3 =0, tmp_tx_dma_ring_addr4=0;

    if (!HAL_IS_PCIBIOS_TYPE(Adapter))
        goto original;

    //No Beacon
    printk("head:%p, ring_dma_addr:%08lx, size:%x\n", prx_dma->rx_queue[HCI_RX_DMA_QUEUE_MAX_NUM-1].pRXBD_head, 
        _GET_HAL_DATA(Adapter)->ring_dma_addr, prx_dma->rx_queue[HCI_RX_DMA_QUEUE_MAX_NUM-1].total_rxbd_num * sizeof(RX_BUFFER_DESCRIPTOR));
        tmp_tx_dma_ring_addr4 = (unsigned long)prx_dma->rx_queue[HCI_RX_DMA_QUEUE_MAX_NUM-1].pRXBD_head +  prx_dma->rx_queue[HCI_RX_DMA_QUEUE_MAX_NUM-1].total_rxbd_num * sizeof(RX_BUFFER_DESCRIPTOR);
    ptxbd_head  = (PTX_BUFFER_DESCRIPTOR)( tmp_tx_dma_ring_addr4);

    for (i=0;i<HCI_RX_DMA_QUEUE_MAX_NUM;i++){
        tmp_tx_dma_ring_addr = _GET_HAL_DATA(Adapter)->ring_dma_addr + prx_dma->rx_queue[i].total_rxbd_num * sizeof(RX_BUFFER_DESCRIPTOR);
    }


    printk("ptxbd_head:%p, tmp_tx_dma_ring_addr:%08lx\n", ptxbd_head, tmp_tx_dma_ring_addr);
    tmp_tx_dma_ring_addr4 =0;
    //No Beacon
    // TODO: need to bug fix
    ptx_desc_head   = (PTX_DESC_88XX)((pu1Byte)ptxbd_head + \
                        sizeof(TX_BUFFER_DESCRIPTOR) * TotalTXBDNum_NoBcn);

    tmp_tx_dma_ring_addr2 = tmp_tx_dma_ring_addr + sizeof(TX_BUFFER_DESCRIPTOR) * TotalTXBDNum_NoBcn;
    printk("ptx_desc_head:%p, tmp_tx_dma_ring_addr2:%08lx, size: %x, %x\n", ptx_desc_head, tmp_tx_dma_ring_addr2, sizeof(TX_BUFFER_DESCRIPTOR) , sizeof(TX_DESC_88XX) );
    ptxbd_bcn_head  = (PTX_BUFFER_DESCRIPTOR)((pu1Byte)ptx_desc_head + \
                        sizeof(TX_DESC_88XX) * TotalTXBDNum_NoBcn);

    tmp_tx_dma_ring_addr3 = tmp_tx_dma_ring_addr2 + sizeof(TX_DESC_88XX) * TotalTXBDNum_NoBcn;
 printk("ptxbd_bcn_head:%p, tmp_tx_dma_ring_addr3:%08lx,\n", ptxbd_bcn_head, tmp_tx_dma_ring_addr3);
#if IS_RTL8881A_SERIES
    if (IS_HARDWARE_TYPE_8881A(Adapter)) {
        ptxdesc_bcn_head = (PTX_DESC_88XX)((pu1Byte)ptxbd_bcn_head + \
                                (1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V2);
        beacon_offset = TXBD_BEACON_OFFSET_V2;
        tmp_tx_dma_ring_addr4 = tmp_tx_dma_ring_addr3+(1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V2;
    }
#endif
#if IS_RTL8192E_SERIES || IS_RTL8192F_SERIES
    if (IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8192F(Adapter)) {
        ptxdesc_bcn_head = (PTX_DESC_88XX)((pu1Byte)ptxbd_bcn_head + \
                                (1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1);
        beacon_offset = TXBD_BEACON_OFFSET_V1;
        tmp_tx_dma_ring_addr4 = tmp_tx_dma_ring_addr3+(1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1;
    }
#endif
#if IS_RTL8814A_SERIES
    if (IS_HARDWARE_TYPE_8814A(Adapter)) {
        ptxdesc_bcn_head = (PTX_DESC_88XX)((pu1Byte)ptxbd_bcn_head + \
                                (1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1);
        beacon_offset = TXBD_BEACON_OFFSET_V1;
        tmp_tx_dma_ring_addr4 = tmp_tx_dma_ring_addr3+(1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1;
    }
#endif

#if IS_RTL8197F_SERIES
    if (IS_HARDWARE_TYPE_8197F(Adapter)) {
        ptxdesc_bcn_head = (PTX_DESC_88XX)((pu1Byte)ptxbd_bcn_head + \
                                (1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_8197F);
        beacon_offset = TXBD_BEACON_OFFSET_8197F;
        tmp_tx_dma_ring_addr4 = tmp_tx_dma_ring_addr3+(1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_8197F;
    }
#endif

#if IS_RTL8822B_SERIES
	if (IS_HARDWARE_TYPE_8822B(Adapter)) {
		ptxdesc_bcn_head = (PTX_DESC_88XX)((pu1Byte)ptxbd_bcn_head + \
				(1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1);
	beacon_offset = TXBD_BEACON_OFFSET_V1;
	tmp_tx_dma_ring_addr4 = tmp_tx_dma_ring_addr3+(1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1;
	}
#endif

#if IS_RTL8822C_SERIES
	if (IS_HARDWARE_TYPE_8822C(Adapter)) {
		ptxdesc_bcn_head = (PTX_DESC_88XX)((pu1Byte)ptxbd_bcn_head + \
				(1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1);
	beacon_offset = TXBD_BEACON_OFFSET_V1;
	tmp_tx_dma_ring_addr4 = tmp_tx_dma_ring_addr3+(1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1;
	}
#endif

#if IS_RTL8812F_SERIES
	if (IS_HARDWARE_TYPE_8812F(Adapter)) {
		ptxdesc_bcn_head = (PTX_DESC_88XX)((pu1Byte)ptxbd_bcn_head + \
				(1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1);
	beacon_offset = TXBD_BEACON_OFFSET_V1;
	tmp_tx_dma_ring_addr4 = tmp_tx_dma_ring_addr3+(1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1;
	}
#endif

#if IS_RTL8821C_SERIES
	if (IS_HARDWARE_TYPE_8821C(Adapter)) {
		ptxdesc_bcn_head = (PTX_DESC_88XX)((pu1Byte)ptxbd_bcn_head + \
				(1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1);
	beacon_offset = TXBD_BEACON_OFFSET_V1;
	tmp_tx_dma_ring_addr4 = tmp_tx_dma_ring_addr3+(1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1;
	}
#endif

	printk("ptxdesc_bcn_head:%p, tmp_tx_dma_ring_addr4:%08lx, \n", ptxdesc_bcn_head, tmp_tx_dma_ring_addr4);
    // initiate all tx queue data structures 
    for (q_num = 0; q_num < HCI_TX_DMA_QUEUE_MAX_NUM; q_num++)
    {
        ptx_dma->tx_queue[q_num].hw_idx         = 0;
        ptx_dma->tx_queue[q_num].host_idx       = 0;
        ptx_dma->tx_queue[q_num].total_txbd_num = pTXBD_NUM[q_num];
        ptx_dma->tx_queue[q_num].avail_txbd_num = pTXBD_NUM[q_num];
        ptx_dma->tx_queue[q_num].reg_rwptr_idx  = TXBD_RWPtr_Reg[q_num];

        if ( 0 == q_num ) {
            ptx_dma->tx_queue[q_num].pTXBD_head    = ptxbd_head;
            ptx_dma->tx_queue[q_num].ptx_desc_head = ptx_desc_head;
        }
        else {
            if ( HCI_TX_DMA_QUEUE_BCN != q_num ) {
                ptx_dma->tx_queue[q_num].pTXBD_head    = (PTX_BUFFER_DESCRIPTOR)((pu1Byte)ptx_dma->tx_queue[q_num-1].pTXBD_head + sizeof(TX_BUFFER_DESCRIPTOR) * pTXBD_NUM[q_num-1]);
                ptx_dma->tx_queue[q_num].ptx_desc_head = (PTX_DESC_88XX)((pu1Byte)ptx_dma->tx_queue[q_num-1].ptx_desc_head + sizeof(TX_DESC_88XX)*pTXBD_NUM[q_num-1]);

                tmp_tx_dma_ring_addr += sizeof(TX_BUFFER_DESCRIPTOR) *pTXBD_NUM[q_num-1] ;
                tmp_tx_dma_ring_addr2 += sizeof(TX_DESC_88XX) *pTXBD_NUM[q_num-1] ;
            }
            else {
                ptx_dma->tx_queue[q_num].pTXBD_head    = ptxbd_bcn_head;
                ptx_dma->tx_queue[q_num].ptx_desc_head = ptxdesc_bcn_head;
                tmp_tx_dma_ring_addr = tmp_tx_dma_ring_addr3;
                tmp_tx_dma_ring_addr2 = tmp_tx_dma_ring_addr4;
            }
        }
        
        ptxbd    = ptx_dma->tx_queue[q_num].pTXBD_head;
        ptx_desc = ptx_dma->tx_queue[q_num].ptx_desc_head;
	
		if(TXBD_RWPtr_Reg[q_num])
	        HAL_RTL_W32(TXBD_RWPtr_Reg[q_num], 0);
		if(TXBD_Reg[q_num])
        	HAL_RTL_W32(TXBD_Reg[q_num], tmp_tx_dma_ring_addr);
        _GET_HAL_DATA(Adapter)->txBD_dma_ring_addr[q_num] = tmp_tx_dma_ring_addr;
        _GET_HAL_DATA(Adapter)->txDesc_dma_ring_addr[q_num] = tmp_tx_dma_ring_addr2;
    
		printk("%s(%d), q_num:%d, TXBD_RWPtr_Reg:0x%x, TXBD_Reg:0x%x, ptxbd:%p %08lx, ptx_desc:%p %08lx\n", __FUNCTION__, __LINE__, q_num, TXBD_RWPtr_Reg[q_num], TXBD_Reg[q_num], ptxbd, tmp_tx_dma_ring_addr, ptx_desc, tmp_tx_dma_ring_addr2);

        // assign LowAddress and TxDescLength to each TXBD element
        if (q_num != HCI_TX_DMA_QUEUE_BCN) {
            for(i = 0; i < pTXBD_NUM[q_num]; i++)        
            {   
                SET_DESC_FIELD_CLR(ptxbd[i].TXBD_ELE[0].Dword0, TXDESCSize, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
                SET_DESC_FIELD_CLR(ptxbd[i].TXBD_ELE[0].Dword1, tmp_tx_dma_ring_addr2 + sizeof(TX_DESC_88XX)*i,TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
            }
        } else {
            // beacon...
            for (i = 0; i < 1+HAL_NUM_VWLAN; i++)
            {
                ptxbd_bcn_cur = (pu1Byte)ptxbd + beacon_offset * i;
                SET_DESC_FIELD_CLR(ptxbd_bcn_cur->TXBD_ELE[0].Dword0, \
                        TXDESCSize, \
                        TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
               
               SET_DESC_FIELD_CLR(ptxbd_bcn_cur->TXBD_ELE[0].Dword1, tmp_tx_dma_ring_addr2 + sizeof(TX_DESC_88XX)*i, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
#if 1
                printk ("ptxbd_bcn[%d]: 0x%x, Dword0: 0x%x, Dword1: 0x%x \n", \
                                                i, (u4Byte)ptxbd_bcn_cur, \
                                                (u4Byte)GET_DESC(ptxbd_bcn_cur->TXBD_ELE[0].Dword0), \
                                                (u4Byte)GET_DESC(ptxbd_bcn_cur->TXBD_ELE[0].Dword1)
                                                );             
#endif
            }
        }        
    }   

#if CFG_HAL_TX_AMSDU
if(IS_SUPPORT_TX_AMSDU(Adapter)){
    desc_dma_buf_start_amsdu = _GET_HAL_DATA(Adapter)->desc_dma_buf_amsdu;
    platform_zero_memory(desc_dma_buf_start_amsdu, _GET_HAL_DATA(Adapter)->desc_dma_buf_len_amsdu);
    
    pdesc_dma_buf_amsdu = (pu1Byte)(_GET_HAL_DATA(Adapter)->desc_dma_buf_virt_amsdu);

    // BK, BE, VI, VO
    ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_BK].pTXBD_head_amsdu = pdesc_dma_buf_amsdu;
    ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_BE].pTXBD_head_amsdu = (pu1Byte)ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_BK].pTXBD_head_amsdu + TX_BKQ_TXBD_NUM * sizeof(TXBD_ELEMENT)* MAX_NUM_OF_MSDU_IN_AMSDU;
    ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_VI].pTXBD_head_amsdu = (pu1Byte)ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_BE].pTXBD_head_amsdu + TX_BEQ_TXBD_NUM * sizeof(TXBD_ELEMENT)* MAX_NUM_OF_MSDU_IN_AMSDU;
    ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_VO].pTXBD_head_amsdu = (pu1Byte)ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_VI].pTXBD_head_amsdu + TX_VIQ_TXBD_NUM * sizeof(TXBD_ELEMENT)* MAX_NUM_OF_MSDU_IN_AMSDU;
    _GET_HAL_DATA(Adapter)->desc_dma_buf_addr_amsdu[1] = _GET_HAL_DATA(Adapter)->desc_dma_buf_addr_amsdu[0] + TX_BKQ_TXBD_NUM * sizeof(TXBD_ELEMENT)* MAX_NUM_OF_MSDU_IN_AMSDU;
    _GET_HAL_DATA(Adapter)->desc_dma_buf_addr_amsdu[2] = _GET_HAL_DATA(Adapter)->desc_dma_buf_addr_amsdu[1] + TX_BEQ_TXBD_NUM * sizeof(TXBD_ELEMENT)* MAX_NUM_OF_MSDU_IN_AMSDU;
    _GET_HAL_DATA(Adapter)->desc_dma_buf_addr_amsdu[3] = _GET_HAL_DATA(Adapter)->desc_dma_buf_addr_amsdu[2] + TX_VIQ_TXBD_NUM * sizeof(TXBD_ELEMENT)* MAX_NUM_OF_MSDU_IN_AMSDU;    
}
#endif 

    return RT_STATUS_SUCCESS;
#endif

original:

#if CFG_HAL_TX_AMSDU
    if ( IS_SUPPORT_TX_AMSDU(Adapter) ) {
        desc_dma_buf_start_amsdu = _GET_HAL_DATA(Adapter)->desc_dma_buf_amsdu;
        platform_zero_memory(desc_dma_buf_start_amsdu, _GET_HAL_DATA(Adapter)->desc_dma_buf_len_amsdu);

#ifdef TRXBD_CACHABLE_REGION

#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
		// With CCI400 ,Do nothing for cache coherent code
#else
		//add by eric //error: too few arguments to function 'rtl_cache_sync_wback'
		rtl_cache_sync_wback(Adapter,(unsigned long)((PVOID)(desc_dma_buf_start_amsdu)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), _GET_HAL_DATA(Adapter)->desc_dma_buf_len_amsdu,PCI_DMA_TODEVICE);
#endif //CONFIG_ENABLE_CCI400
#else
        _dma_cache_wback((unsigned long)((PVOID)(desc_dma_buf_start_amsdu)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), _GET_HAL_DATA(Adapter)->desc_dma_buf_len_amsdu);
#endif //CONFIG_RTL_8198F
#endif //TRXBD_CACHABLE_REGION

        pdesc_dma_buf_amsdu = (pu1Byte)(((unsigned long)desc_dma_buf_start_amsdu) + \
            (HAL_PAGE_SIZE - (((unsigned long)desc_dma_buf_start_amsdu) & (HAL_PAGE_SIZE-1))));
        
        //Transfer to Non-cachable address
#ifdef TRXBD_CACHABLE_REGION    
        // Do nothing for un-cachable      
#else
        pdesc_dma_buf_amsdu = (pu1Byte)HAL_TO_NONCACHE_ADDR((unsigned long)pdesc_dma_buf_amsdu);
#endif
    }
#endif // CFG_HAL_TX_AMSDU
    //No Beacon
#ifdef NEW_BUUFER_ALLOCATION
    ptxbd_head      =   _GET_HAL_DATA(Adapter)->desc_dma_buf;
	platform_zero_memory(ptxbd_head, _GET_HAL_DATA(Adapter)->desc_dma_buf_len);

	
#else    
    ptxbd_head      = (PTX_BUFFER_DESCRIPTOR)(prx_dma->rx_queue[HCI_RX_DMA_QUEUE_MAX_NUM-1].pRXBD_head + \
                        prx_dma->rx_queue[HCI_RX_DMA_QUEUE_MAX_NUM-1].total_rxbd_num);
#endif

    //No Beacon
    ptx_desc_head   = (PTX_DESC_88XX)((pu1Byte)ptxbd_head + \
                        sizeof(TX_BUFFER_DESCRIPTOR) * TotalTXBDNum_NoBcn);
    
    ptxbd_bcn_head  = (PTX_BUFFER_DESCRIPTOR)((pu1Byte)ptx_desc_head + \
                        sizeof(TX_DESC_88XX) * TotalTXBDNum_NoBcn);

#if IS_RTL8881A_SERIES
    if (IS_HARDWARE_TYPE_8881A(Adapter)) {
        ptxdesc_bcn_head = (PTX_DESC_88XX)((pu1Byte)ptxbd_bcn_head + \
                                (1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V2);
        beacon_offset = TXBD_BEACON_OFFSET_V2;
    }
#endif
#if IS_RTL8192E_SERIES || IS_RTL8192F_SERIES || IS_RTL8814A_SERIES || IS_RTL8197F_SERIES || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES
    if (IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8192F(Adapter) || IS_HARDWARE_TYPE_8814A(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter)|| IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)) {
        ptxdesc_bcn_head = (PTX_DESC_88XX)((pu1Byte)ptxbd_bcn_head + \
                                (1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1);
        beacon_offset = TXBD_BEACON_OFFSET_V1;
    }
#endif

    // initiate all tx queue data structures 
    for (q_num = 0; q_num < HCI_TX_DMA_QUEUE_MAX_NUM; q_num++)
    {
        ptx_dma->tx_queue[q_num].hw_idx         = 0;
        ptx_dma->tx_queue[q_num].host_idx       = 0;
        ptx_dma->tx_queue[q_num].total_txbd_num = pTXBD_NUM[q_num];
        ptx_dma->tx_queue[q_num].avail_txbd_num = pTXBD_NUM[q_num];
        ptx_dma->tx_queue[q_num].reg_rwptr_idx  = TXBD_RWPtr_Reg[q_num];

        if ( 0 == q_num ) {
            ptx_dma->tx_queue[q_num].pTXBD_head    = ptxbd_head;
            ptx_dma->tx_queue[q_num].ptx_desc_head = ptx_desc_head;
        }
        else {
            if ( HCI_TX_DMA_QUEUE_BCN != q_num ) {
                ptx_dma->tx_queue[q_num].pTXBD_head    = ptx_dma->tx_queue[q_num-1].pTXBD_head + pTXBD_NUM[q_num-1];
                ptx_dma->tx_queue[q_num].ptx_desc_head = ((PTX_DESC_88XX)ptx_dma->tx_queue[q_num-1].ptx_desc_head) + pTXBD_NUM[q_num-1];
            }
            else {
                ptx_dma->tx_queue[q_num].pTXBD_head    = ptxbd_bcn_head;
                ptx_dma->tx_queue[q_num].ptx_desc_head = ptxdesc_bcn_head;
            }
        }
        
        ptxbd    = ptx_dma->tx_queue[q_num].pTXBD_head;
        ptx_desc = ptx_dma->tx_queue[q_num].ptx_desc_head;

		if(TXBD_RWPtr_Reg[q_num])
        	HAL_RTL_W32(TXBD_RWPtr_Reg[q_num], 0);
		if(TXBD_Reg[q_num]){        
            HAL_RTL_W32(TXBD_Reg[q_num], HAL_VIRT_TO_BUS1(Adapter,(u4Byte)ptxbd,sizeof(TX_BUFFER_DESCRIPTOR) * pTXBD_NUM[q_num],HAL_PCI_DMA_TODEVICE) + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL);

#ifdef PCIE_POWER_SAVING_TEST            
            _GET_HAL_DATA(Adapter)->txBD_dma_ring_addr[q_num] = HAL_VIRT_TO_BUS((u4Byte)ptxbd) + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL;
#endif
        }

#if 0

        RT_TRACE_F(COMP_INIT, DBG_TRACE, ("QNum: %ld, TXBDHead: 0x%lx, TXDESCHead: 0x%lx\n", \
                                            (u4Byte)q_num, \
                                            (u4Byte)ptx_dma->tx_queue[q_num].pTXBD_head, \
                                            (u4Byte)ptx_dma->tx_queue[q_num].ptx_desc_head
                                            ));
#endif

        // assign LowAddress and TxDescLength to each TXBD element
        if (q_num != HCI_TX_DMA_QUEUE_BCN) {
            for(i = 0; i < pTXBD_NUM[q_num]; i++)        
            {   
                SET_DESC_FIELD_CLR(ptxbd[i].TXBD_ELE[0].Dword0, \
                        TXDESCSize, \
                        TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
                SET_DESC_FIELD_CLR(ptxbd[i].TXBD_ELE[0].Dword1, \
                        HAL_VIRT_TO_BUS1(Adapter,(u4Byte)&ptx_desc[i],sizeof(TX_BUFFER_DESCRIPTOR),HAL_PCI_DMA_TODEVICE) + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL, \
                        TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);

                platform_zero_memory(&(ptxbd[i].TXBD_ELE[1]), sizeof(TXBD_ELEMENT)*(TXBD_ELE_NUM-1));
#ifdef WLAN_SUPPORT_H2C_PACKET
                if(HCI_TX_DMA_QUEUE_CMD == q_num)
                {
                    SET_DESC_FIELD_CLR(ptxbd[i].TXBD_ELE[1].Dword0, \
                        H2C_PACKET_PAYLOAD_MAX_SIZE, \
                        TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
                    SET_DESC_FIELD_CLR(ptxbd[i].TXBD_ELE[1].Dword1, \
                        HAL_VIRT_TO_BUS(&(h2c_buf_start[i])) + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL, \
                        TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
                    // h2c TXBD just use 1 page
                    SET_DESC_FIELD_NO_CLR(ptxbd[i].TXBD_ELE[0].Dword0, 1, TXBD_DW0_PSLEN_MSK, TXBD_DW0_PSLEN_SH);   

                    ph2c_tx_desc = &ptx_desc[i];

                    SET_TX_DESC_TXPKTSIZE_NO_CLR(ph2c_tx_desc, 32);
                    SET_TX_DESC_OFFSET_NO_CLR(ph2c_tx_desc, 0);
                    SET_TX_DESC_QSEL(ph2c_tx_desc, TXDESC_QSEL_CMD); 
#ifdef TRXBD_CACHABLE_REGION
                    _dma_cache_wback((unsigned long)(&(ptxbd[i].TXBD_ELE[1])-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), 
                            sizeof(TX_BUFFER_DESCRIPTOR));
                    _dma_cache_wback((unsigned long)(&ptx_desc[i]-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), 
                            TXDESCSize);
#endif						                   
                }

                
#endif //WLAN_SUPPORT_H2C_PACKET
#ifdef TRXBD_CACHABLE_REGION

#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
		// With CCI400 ,Do nothing for cache coherent code
#else
		///add by eric //error: too few arguments to function 'rtl_cache_sync_wback'
		rtl_cache_sync_wback(Adapter,(unsigned long)(&(ptxbd[i].TXBD_ELE[0])-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), 
						sizeof(TX_BUFFER_DESCRIPTOR),PCI_DMA_TODEVICE);
#endif //CONFIG_ENABLE_CCI400
#else
                _dma_cache_wback((unsigned long)(&(ptxbd[i].TXBD_ELE[0])-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), 
                        sizeof(TX_BUFFER_DESCRIPTOR));
				
#endif //CONFIG_RTL_8198F
#endif						
#if 0
               if(HCI_TX_DMA_QUEUE_CMD == q_num)
               {
               RT_TRACE_F(COMP_INIT, DBG_TRACE, ("ptxbd[%ld]:TXBD_ELE[0] Dword0: 0x%lx, Dword1: 0x%lx\n", \
                                                i, \
                                                (u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[0].Dword0), \
                                                (u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[0].Dword1)
                                                ));
               RT_TRACE_F(COMP_INIT, DBG_TRACE, ("ptxbd[%ld]:TXBD_ELE[1] Dword0: 0x%lx, Dword1: 0x%lx\n", \
                                                i, \
                                                (u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[1].Dword0), \
                                                (u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[1].Dword1)
                                                ));               
                }
#endif

            }
        } else {
            // beacon...
            //for (i = 0; i < 1+4; i++)
            for (i = 0; i < 1+HAL_NUM_VWLAN; i++)
            {
                ptxbd_bcn_cur = (PTX_BUFFER_DESCRIPTOR)((pu1Byte)ptxbd + beacon_offset * i);
                // clear own_bit for initial random value                
                SET_DESC_FIELD_CLR(ptxbd_bcn_cur->TXBD_ELE[0].Dword0, 0, TXBD_DW0_BCN_OWN_MSK, TXBD_DW0_BCN_OWN_SH);
                SET_DESC_FIELD_CLR(ptxbd_bcn_cur->TXBD_ELE[0].Dword0, \
                        TXDESCSize, \
                        TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
                SET_DESC_FIELD_CLR(ptxbd_bcn_cur->TXBD_ELE[0].Dword1, \
                        HAL_VIRT_TO_BUS1(Adapter,(u4Byte)&ptx_desc[i],sizeof(TX_BUFFER_DESCRIPTOR),HAL_PCI_DMA_TODEVICE) + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL, \
                        TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
                // clear for initial random value
                platform_zero_memory(&(ptxbd_bcn_cur->TXBD_ELE[1]), sizeof(TXBD_ELEMENT)*(TXBD_ELE_NUM-1));
#ifdef TRXBD_CACHABLE_REGION

#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
			// With CCI400 ,Do nothing for cache coherent code
#else
			///add by eric //error: too few arguments to function 'rtl_cache_sync_wback'
			rtl_cache_sync_wback(Adapter,(unsigned long)(&(ptxbd_bcn_cur->TXBD_ELE[0])-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), 
			sizeof(TXBD_ELEMENT),PCI_DMA_TODEVICE);

#endif //CONFIG_ENABLE_CCI400
#else
                _dma_cache_wback((unsigned long)(&(ptxbd_bcn_cur->TXBD_ELE[0])-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), 
                    sizeof(TXBD_ELEMENT));
#endif //CONFIG_RTL_8198F
#endif //#ifdef TRXBD_CACHABLE_REGION                
#if 1
                RT_TRACE_F(COMP_INIT, DBG_TRACE, ("ptxbd_bcn[%ld]: 0x%lx, Dword0: 0x%lx, Dword1: 0x%lx \n", \
                                                i, (u4Byte)ptxbd_bcn_cur, \
                                                (u4Byte)GET_DESC(ptxbd_bcn_cur->TXBD_ELE[0].Dword0), \
                                                (u4Byte)GET_DESC(ptxbd_bcn_cur->TXBD_ELE[0].Dword1)
                                                ));             
#endif
            }
        }        
    }   


#if CFG_HAL_TX_AMSDU
    if ( IS_SUPPORT_TX_AMSDU(Adapter) ) {

    //ptxbd_head_amsdu    = ptx_dma_amsdu;
    //ptx_dma_amsdu = pdesc_dma_buf_amsdu;

    // BK, BE, VI, VO
    ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_BK].pTXBD_head_amsdu = pdesc_dma_buf_amsdu;
    ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_BE].pTXBD_head_amsdu = (pu1Byte)ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_BK].pTXBD_head_amsdu + TX_BKQ_TXBD_NUM * sizeof(TXBD_ELEMENT)* MAX_NUM_OF_MSDU_IN_AMSDU;
    ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_VI].pTXBD_head_amsdu = (pu1Byte)ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_BE].pTXBD_head_amsdu + TX_BEQ_TXBD_NUM * sizeof(TXBD_ELEMENT)* MAX_NUM_OF_MSDU_IN_AMSDU;
    ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_VO].pTXBD_head_amsdu = (pu1Byte)ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_VI].pTXBD_head_amsdu + TX_VIQ_TXBD_NUM * sizeof(TXBD_ELEMENT)* MAX_NUM_OF_MSDU_IN_AMSDU;
    }
#endif

    return RT_STATUS_SUCCESS;
}

#endif //#if IS_EXIST_RTL8192EE || IS_EXIST_RTL8881AEM || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822BE

static BOOLEAN 
IsTXBDFull88XX(
    IN   HAL_PADAPTER               Adapter,
    IN   u4Byte                     queueIndex  //HCI_TX_DMA_QUEUE_88XX
)
{
    PHCI_TX_DMA_MANAGER_88XX        ptx_dma;
    PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q;    

    ptx_dma     = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);
    cur_q       = &(ptx_dma->tx_queue[queueIndex]);
    
    if (HAL_CIRC_SPACE_RTK(cur_q->host_idx, cur_q->hw_idx, cur_q->total_txbd_num) == 0) {
        // case: full
        RT_TRACE(COMP_SEND, DBG_LOUD, ("TXBD is Full !!!\n") );
        return _TRUE;
    }
    return _FALSE;
}


#if IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE || IS_EXIST_RTL8881AEM || IS_EXIST_RTL8814AE 

static VOID
SetTxDescQSel88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN  PTX_DESC_88XX   ptx_desc,
    IN  u1Byte          drvTID
)
{
    u1Byte  q_select;
    //u4Byte  val=0;
    
	switch (queueIndex) {
    	case HCI_TX_DMA_QUEUE_HI0:
    		q_select = TXDESC_QSEL_HIGH;
    		break;
#if  CFG_HAL_SUPPORT_MBSSID
    	case HCI_TX_DMA_QUEUE_HI1:
            q_select = TXDESC_QSEL_HIGH;
    		break;
    	case HCI_TX_DMA_QUEUE_HI2:
            q_select = TXDESC_QSEL_HIGH;
    		break;
    	case HCI_TX_DMA_QUEUE_HI3:	
            q_select = TXDESC_QSEL_HIGH;
    		break;
    	case HCI_TX_DMA_QUEUE_HI4:
            q_select = TXDESC_QSEL_HIGH;
    		break;
    	case HCI_TX_DMA_QUEUE_HI5:
            q_select = TXDESC_QSEL_HIGH;
    		break;
    	case HCI_TX_DMA_QUEUE_HI6:
            q_select = TXDESC_QSEL_HIGH;
    		break;
    	case HCI_TX_DMA_QUEUE_HI7:
            q_select = TXDESC_QSEL_HIGH;
    		break;
#endif  //CFG_HAL_SUPPORT_MBSSID

    	case HCI_TX_DMA_QUEUE_MGT:
    		q_select = TXDESC_QSEL_MGT;
    		break;
            
#if CFG_HAL_MAC_LOOPBACK && CFG_HAL_WIFI_WMM
    	case HCI_TX_DMA_QUEUE_BE:
    		q_select = TXDESC_QSEL_TID0;
    		break;
#endif  //CFG_HAL_MAC_LOOPBACK && CFG_HAL_WIFI_WMM

    	default:
    		// data packet
#if CFG_HAL_RTL_MANUAL_EDCA
    		if (HAL_VAR_MANUAL_EDCA) {
    			switch (queueIndex) {
        			case HCI_TX_DMA_QUEUE_VO:
        				q_select = TXDESC_QSEL_TID6;
        				break;
        			case HCI_TX_DMA_QUEUE_VI:
        				q_select = TXDESC_QSEL_TID4;
                		break;
        			case HCI_TX_DMA_QUEUE_BE:
        				q_select = TXDESC_QSEL_TID0;
        	    		break;
        		    default:
        				q_select = TXDESC_QSEL_TID1;
        				break;
    			}
    		}
    		else {
                q_select = drvTID;
    		}
#else
            q_select = drvTID;
#endif  //CFG_HAL_RTL_MANUAL_EDCA
            break;
	}    

    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, q_select, TX_DW1_QSEL_MSK, TX_DW1_QSEL_SH);
    //ptx_desc->Dword1 |= val;
}

static VOID
SetSecType(
    IN  HAL_PADAPTER    Adapter,
    IN  PTX_DESC_88XX   ptx_desc,
    IN  PVOID           pDescData 
)
{
    PTX_DESC_DATA_88XX  pdesc_data  = (PTX_DESC_DATA_88XX)pDescData;
    
    switch(pdesc_data->secType) {
    case _WEP_40_PRIVACY_:
    case _WEP_104_PRIVACY_:
    case _TKIP_PRIVACY_:
        SET_DESC_FIELD_CLR(ptx_desc->Dword1, TXDESC_SECTYPE_WEP40_OR_TKIP,
                                        TX_DW1_SECTYPE_MSK, TX_DW1_SECTYPE_SH);
        break;
#if CFG_HAL_RTL_HW_WAPI_SUPPORT
    case _WAPI_SMS4_:
        SET_DESC_FIELD_CLR(ptx_desc->Dword1, TXDESC_SECTYPE_WAPI,
                                        TX_DW1_SECTYPE_MSK, TX_DW1_SECTYPE_SH);
        break;
#endif        
    case _CCMP_PRIVACY_:
        SET_DESC_FIELD_CLR(ptx_desc->Dword1, TXDESC_SECTYPE_AES,
                                        TX_DW1_SECTYPE_MSK, TX_DW1_SECTYPE_SH);
        break;
    default:
#if 0
        SET_DESC_FIELD_CLR(ptx_desc->Dword1, TXDESC_SECTYPE_NO_ENCRYPTION,
                                        TX_DW1_SECTYPE_MSK, TX_DW1_SECTYPE_SH);        
#endif
        break;
    }   
}

#endif //#if IS_EXIST_RTL8192EE || IS_EXIST_RTL8881AEM || IS_EXIST_RTL8814AE 

#if IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE || IS_EXIST_RTL8881AEM || IS_EXIST_RTL8814AE 

VOID
FillTxDesc88XX (
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN  PVOID           pDescData
)
{
    PHCI_TX_DMA_MANAGER_88XX        ptx_dma;
    PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q;
    PTX_DESC_88XX                   ptx_desc;
    PTX_DESC_DATA_88XX              pdesc_data = (PTX_DESC_DATA_88XX)pDescData;
    u4Byte                          val;
    u4Byte                          tmp,tmpCache;
    u4Byte                          TXDESCSize;
    u2Byte                          *tx_head;
    struct tx_desc_info *pswdescinfo, *pdescinfo;

    TXDESCSize = SIZE_TXDESC_88XX;
 
    //Dword 0
    u2Byte  TX_DESC_TXPKTSIZE        = pdesc_data->hdrLen + pdesc_data->llcLen + pdesc_data->frLen;
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
    u1Byte	TX_DESC_OFFSET			 = ((pdesc_data->smhEn == TRUE) ? (HAL_HW_TXSC_HDR_CONV_ADD_OFFSET+TXDESCSize) : TXDESCSize);
#else
    u1Byte  TX_DESC_OFFSET           = TXDESCSize; //HAL_TXDESC_OFFSET_SIZE;
#endif

#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
    BOOLEAN TX_DESC_BMC              = (pdesc_data->smhEn == TRUE) ? 
    					((HAL_IS_MCAST(GetEthDAPtr((pu1Byte)pdesc_data->pHdr))) ? 1 : 0) : 
				        ((HAL_IS_MCAST(GetAddr1Ptr((pu1Byte)pdesc_data->pHdr))) ? 1 : 0);   // when multicast or broadcast, BMC = 1
#else
    BOOLEAN TX_DESC_BMC              = ((HAL_IS_MCAST(GetAddr1Ptr((pu1Byte)pdesc_data->pHdr))) ? 1 : 0);   // when multicast or broadcast, BMC = 1        
#endif

//    BOOLEAN TX_DESC_HT               = 0;
//    BOOLEAN TX_DESC_LINIP            = 0;
//    BOOLEAN TX_DESC_NOACM            = 0;
//    BOOLEAN TX_DESC_GF               = 0;

    //Dword 1 
    u1Byte  TX_DESC_MACID            = pdesc_data->macId; // MACID/MBSSID ?
    u1Byte  TX_DESC_RATE_ID          = pdesc_data->rateId;
    BOOLEAN TX_DESC_MORE_DATA        = pdesc_data->moreData;    
    BOOLEAN TX_DESC_EN_DESC_ID       = pdesc_data->enDescId;
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
    u1Byte  TX_DESC_PKT_OFFSET       = 0; // unit: 8 bytes. Early mode: 1 units, (8 * 1 = 8 bytes for early mode info)
#endif
    
    //Dword 2
    BOOLEAN TX_DESC_AGG_EN           = pdesc_data->aggEn; 
    BOOLEAN TX_DESC_BK               = pdesc_data->bk;
    BOOLEAN TX_DESC_MOREFRAG         = pdesc_data->frag;
    u1Byte  TX_DESC_AMPDU_DENSITY    = pdesc_data->ampduDensity;
    u4Byte  TX_DESC_P_AID      		 = pdesc_data->p_aid;
    u1Byte  TX_DESC_G_ID      		 = pdesc_data->g_id;
#if CFG_HAL_HW_AES_IV
    BOOLEAN TX_DESC_HW_AES_IV        = pdesc_data->hwAESIv;
#endif // CFG_HAL_HW_AES_IV
    u1Byte	TX_DESC_CCA_RTS			 = pdesc_data->cca_rts;


    //Dword 3
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV_LLC
    u1Byte  TX_DESC_WHEADER_LEN      = ((pdesc_data->smhEn == TRUE) ? (HAL_ETH_HEADER_LEN_MAX >> 1) : ((pdesc_data->hdrLen) >> 1)); // unit: 2 bytes
#else
    u1Byte  TX_DESC_WHEADER_LEN      = ((pdesc_data->smhEn == TRUE) ? (HAL_ETH_HEADER_LEN_MAX >> 1) : ((pdesc_data->hdrLen+pdesc_data->llcLen+pdesc_data->iv) >> 1)); // unit: 2 bytes
#endif // CFG_HAL_HW_TX_SHORTCUT_HDR_CONV_LLC
#endif // CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
    BOOLEAN TX_DESC_USERATE          = pdesc_data->useRate;    
    BOOLEAN TX_DESC_DISRTSFB         = pdesc_data->disRTSFB;
    BOOLEAN TX_DESC_DISDATAFB        = pdesc_data->disDataFB;
    BOOLEAN TX_DESC_CTS2SELF         = pdesc_data->CTS2Self;
    BOOLEAN TX_DESC_RTS_EN           = pdesc_data->RTSEn;
    BOOLEAN TX_DESC_HW_RTS_EN        = pdesc_data->HWRTSEn;
    BOOLEAN TX_DESC_NAVUSEHDR        = pdesc_data->navUseHdr;
    u1Byte  TX_DESC_MAX_AGG_NUM      = pdesc_data->maxAggNum;
    BOOLEAN TX_DESC_NDPA		     = pdesc_data->ndpa;

    //Dword 4
    u1Byte  TX_DESC_DATERATE         = pdesc_data->dataRate;
    u1Byte  TX_DESC_DATA_RATEFB_LMT  = pdesc_data->dataRateFBLmt;
    u1Byte  TX_DESC_RTS_RATEFB_LMT   = pdesc_data->RTSRateFBLmt;
    BOOLEAN TX_DESC_RTY_LMT_EN       = pdesc_data->rtyLmtEn;   
    u1Byte  TX_DESC_DATA_RT_LMT      = pdesc_data->dataRtyLmt;
    u1Byte  TX_DESC_RTSRATE          = pdesc_data->RTSRate;
    u1Byte  TX_DESC_BMCRtyLmt        = pdesc_data->BMCRtyLmt;

    //Dword 5
    u1Byte  TX_DESC_DATA_SC          = pdesc_data->dataSC;
    u1Byte  TX_DESC_DATA_SHORT       = pdesc_data->dataShort;
    u1Byte  TX_DESC_DATA_BW          = pdesc_data->dataBW;
    u1Byte  TX_DESC_DATA_STBC        = pdesc_data->dataStbc;
	u1Byte  TX_DESC_DATA_LDPC        = pdesc_data->dataLdpc;
    u1Byte  TX_DESC_RTS_SHORT        = pdesc_data->RTSShort;
    u1Byte  TX_DESC_RTS_SC           = pdesc_data->RTSSC;   
    u1Byte	TX_DESC_POWER_OFFSET	 = pdesc_data->TXPowerOffset;
    u1Byte	TX_ANT					 = pdesc_data->TXAnt;
#if defined(AP_SWPS_OFFLOAD)
    u1Byte  TX_DESC_DROP_ID          = pdesc_data->DropID;
#endif 
	
    //Dword 6
#if defined(AP_SWPS_OFFLOAD)
    u2Byte  TX_DESC_SWPS_SEQ         = pdesc_data->SWPS_sequence;
#endif
#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))
    u1Byte  TX_DESC_ANTSEL			 = pdesc_data->ant_sel;	
    u1Byte  TX_DESC_ANTSEL_A			 = pdesc_data->ant_sel_a;	
    u1Byte  TX_DESC_ANTSEL_B			 = pdesc_data->ant_sel_b;	
    u1Byte  TX_DESC_ANTSEL_C			 = pdesc_data->ant_sel_c;	
#endif	//#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))

    //Dword 7
    // use for CFG_HAL_TX_SHORTCUT
    u2Byte  TX_DESC_TXBUFF           = pdesc_data->frLen;

	// Dword 8
#if CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC
	BOOLEAN	TX_DESC_TXWIFI_CP		= pdesc_data->txwifiCp;
#endif // CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
	BOOLEAN TX_DESC_MAC_CP			= pdesc_data->macCp;
	BOOLEAN TX_DESC_SMH_EN			= pdesc_data->smhEn;
#endif // CFG_HAL_HW_TX_SHORTCUT_HDR_CONV

    //Dword 9
#if CFG_HAL_HW_SEQ
    u2Byte TX_DESC_SEQ               = 0;
#else
    u2Byte TX_DESC_SEQ               = GetSequence(pdesc_data->pHdr);
#endif

#if 0 //(CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC||CFG_HAL_HW_TX_SHORTCUT_HDR_CONV)
	if (pdesc_data->smhEn != 0 || pdesc_data->macCp != 0)
		printk("%s(%d): macCp:0x%x, smhEn:0x%x \n", __FUNCTION__, __LINE__, pdesc_data->macCp, pdesc_data->smhEn);

	if (pdesc_data->txwifiCp != 0)
		printk("%s(%d): txwifiCp:0x%x \n", __FUNCTION__, __LINE__, pdesc_data->txwifiCp);
#endif

#if 0 // show TX Rpt Info
    {
        int baseReg = 0x8080, offset, i;
        int lenTXDESC = 10, lenHdrInfo = 20;
    
        if (pdesc_data->smhEn != 0) {
            HAL_RTL_W8(0x106, 0x7F);
            HAL_RTL_W32(0x140, 0x662);
        
            for(i = 0; i < lenTXDESC; i++) {
                printk("%08X ", (u4Byte)GET_DESC(HAL_RTL_R32(baseReg)));
                if (i%4==3)
                    printk("\n");
                baseReg += 4;
            }
            printk("\n");
        
            for(i = 0; i < lenHdrInfo; i++) {
                printk("%08X ", (u4Byte)GET_DESC(HAL_RTL_R32(baseReg)));
                if (i%4==3)
                    printk("\n");
                baseReg += 4;
            }
            printk("\n");
        }
    }
#endif

	if(Adapter->pshare->rf_ft_var.manual_ampdu)
		TX_DESC_MAX_AGG_NUM = 0;

#if CFG_HAL_MULTICAST_BMC_ENHANCE
    if(TX_DESC_BMC)
    {
        TX_DESC_BMCRtyLmt = pdesc_data->BMCRtyLmt;
    }
#endif


    ptx_dma     = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);
    cur_q       = &(ptx_dma->tx_queue[queueIndex]);
    ptx_desc    = ((PTX_DESC_88XX)(cur_q->ptx_desc_head)) + cur_q->host_idx;

    //Clear All Bit
    platform_zero_memory((PVOID)ptx_desc, TXDESCSize);

    if (pdesc_data->secType != _NO_PRIVACY_) {
        if (pdesc_data->swCrypt == FALSE) {
            SetSecType(Adapter, ptx_desc, pdesc_data);
            // for hw sec: 1) WEP's iv, 2) TKIP's iv and eiv, 3) CCMP's ccmp header are all in pdesc_data->iv
            TX_DESC_TXPKTSIZE += pdesc_data->iv;
        } else {
            // for sw sec
            TX_DESC_TXPKTSIZE += (pdesc_data->iv + pdesc_data->icv + pdesc_data->mic);
        }
    }


    //4 Set Dword0
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword0, TX_DESC_TXPKTSIZE, TX_DW0_TXPKSIZE_MSK, TX_DW0_TXPKSIZE_SH);
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword0, TX_DESC_OFFSET, TX_DW0_OFFSET_MSK, TX_DW0_OFFSET_SH);
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword0, TX_DESC_BMC, TX_DW0_BMC_MSK, TX_DW0_BMC_SH);

    //4 Set Dword1
    SetTxDescQSel88XX(Adapter, queueIndex, ptx_desc, pdesc_data->tid);        
    if ( (queueIndex >= HCI_TX_DMA_QUEUE_HI0) && (queueIndex <= HCI_TX_DMA_QUEUE_HI7) ) {
        //MacID has written in SetTxDescQSel88XX()
#ifdef HW_ENC_FOR_GROUP_CIPHER
        SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, TX_DESC_MACID, TX_DW1_MACID_MSK, TX_DW1_MACID_SH);
#endif
    } else {
        SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, TX_DESC_MACID, TX_DW1_MACID_MSK, TX_DW1_MACID_SH);
    }
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, TX_DESC_RATE_ID, TX_DW1_RATE_ID_MSK, TX_DW1_RATE_ID_SH);
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, TX_DESC_MORE_DATA, TX_DW1_MOREDATA_MSK, TX_DW1_MOREDATA_SH);
    if (TX_DESC_EN_DESC_ID)
        SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, 1, TX_DW1_EN_DESC_ID_MSK, TX_DW1_EN_DESC_ID_SH);

    //4 Set Dword2
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword2, TX_DESC_AGG_EN, TX_DW2_AGG_EN_MSK, TX_DW2_AGG_EN_SH);    
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword2, TX_DESC_BK, TX_DW2_BK_MSK, TX_DW2_BK_SH);
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword2, TX_DESC_MOREFRAG, TX_DW2_MOREFRAG_MSK, TX_DW2_MOREFRAG_SH);
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword2, TX_DESC_AMPDU_DENSITY, TX_DW2_AMPDU_DENSITY_MSK, TX_DW2_AMPDU_DENSITY_SH);    
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword2, TX_DESC_P_AID, TX_DW2_P_AID_MSK, TX_DW2_P_AID_SH);    
#if IS_EXIST_RTL8192FE
	if (GET_CHIP_VER(Adapter) == VERSION_8192F)
		;
	else
#endif
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword2, TX_DESC_G_ID, TX_DW2_G_ID_MSK, TX_DW2_G_ID_SH);
#if IS_EXIST_RTL8814AE
    if (IS_HARDWARE_TYPE_8814AE(Adapter)) {
#if CFG_HAL_HW_AES_IV
        SET_DESC_FIELD_NO_CLR(ptx_desc->Dword2, TX_DESC_HW_AES_IV, TX_DW2_HW_AES_IV_MSK, TX_DW2_HW_AES_IV_SH);
#endif // CFG_HAL_HW_AES_IV
    }
#endif
#if 1 /*eric-ac2*/
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword2, TX_DESC_CCA_RTS, TX_DW2_CCA_RTS_MSK, TX_DW2_CCA_RTS_SH);
#endif


    //4 Set Dword3
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword3, TX_DESC_USERATE, TX_DW3_USERATE_MSK, TX_DW3_USERATE_SH);
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword3, TX_DESC_DISRTSFB, TX_DW3_DISRTSFB_MSK, TX_DW3_DISRTSFB_SH);
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword3, TX_DESC_DISDATAFB, TX_DW3_DISDATAFB_MSK, TX_DW3_DISDATAFB_SH);

    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword3, TX_DESC_CTS2SELF, TX_DW3_CTS2SELF_MSK, TX_DW3_CTS2SELF_SH);
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword3, TX_DESC_RTS_EN, TX_DW3_RTSEN_MSK, TX_DW3_RTSEN_SH);
   
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword3, TX_DESC_HW_RTS_EN, TX_DW3_HW_RTS_EN_MSK, TX_DW3_HW_RTS_EN_SH);
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword3, TX_DESC_NAVUSEHDR, TX_DW3_NAVUSEHDR_MSK, TX_DW3_NAVUSEHDR_SH);
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword3, TX_DESC_MAX_AGG_NUM, TX_DW3_MAX_AGG_NUM_MSK, TX_DW3_MAX_AGG_NUM_SH);
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword3, TX_DESC_NDPA, TX_DW3_NDPA_MSK, TX_DW3_NDPA_SH);

    //4 Set Dword4
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, TX_DESC_DATERATE, TX_DW4_DATARATE_MSK, TX_DW4_DATARATE_SH);
#if IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE
	if (IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8192FE(Adapter)) {
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, TX_DESC_DATA_RATEFB_LMT, TX_DW4_DATA_RATEFB_LMT_MSK, TX_DW4_DATA_RATEFB_LMT_SH);
	    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, TX_DESC_RTS_RATEFB_LMT, TX_DW4_RTS_RATEFB_LMT_MSK, TX_DW4_RTS_RATEFB_LMT_SH);
	}
#endif
#if IS_RTL88XX_MAC_V2
    if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2) {
		if (TX_DESC_BMC) { 
		    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, 0, TX_DW4_DATA_RTY_LOWEST_RATE_MSK, TX_DW4_DATA_RTY_LOWEST_RATE_SH);
	    } else {
		    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, TX_DESC_DATA_RATEFB_LMT, TX_DW4_DATA_RTY_LOWEST_RATE_MSK, TX_DW4_DATA_RTY_LOWEST_RATE_SH);
	    }
	    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, TX_DESC_RTS_RATEFB_LMT, TX_DW4_RTS_RTY_LOWEST_RATE_MSK, TX_DW4_RTS_RTY_LOWEST_RATE_SH);

#if CFG_HAL_MULTICAST_BMC_ENHANCE
        if(TX_DESC_BMC) {
    	    SET_DESC_FIELD_CLR(ptx_desc->Dword4, TX_DESC_BMCRtyLmt, TX_DW4_DATA_RTY_LOWEST_RATE_MSK, TX_DW4_DATA_RTY_LOWEST_RATE_SH);    
        }
#endif //#if CFG_HAL_MULTICAST_BMC_ENHANCE        
	}
#endif //IS_RTL88XX_MAC_V2

    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, TX_DESC_RTY_LMT_EN, TX_DW4_RTY_LMT_EN_MSK, TX_DW4_RTY_LMT_EN_SH);
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, TX_DESC_DATA_RT_LMT, TX_DW4_DATA_RT_LMT_MSK, TX_DW4_DATA_RT_LMT_SH);
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, TX_DESC_RTSRATE, TX_DW4_RTSRATE_MSK, TX_DW4_RTSRATE_SH);

    //4 Set Dword5
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword5, TX_DESC_DATA_SC, TX_DW5_DATA_SC_MSK, TX_DW5_DATA_SC_SH);
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword5, TX_DESC_DATA_SHORT, TX_DW5_DATA_SHORT_MSK, TX_DW5_DATA_SHORT_SH);
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword5, TX_DESC_DATA_BW, TX_DW5_DATA_BW_MSK, TX_DW5_DATA_BW_SH);
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword5, TX_DESC_DATA_STBC, TX_DW5_DATA_STBC_MSK, TX_DW5_DATA_STBC_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword5, TX_DESC_DATA_LDPC, TX_DW5_DATA_LDPC_MSK, TX_DW5_DATA_LDPC_SH);
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword5, TX_DESC_RTS_SHORT, TX_DW5_RTS_SHORT_MSK, TX_DW5_RTS_SHORT_SH);
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword5, TX_DESC_RTS_SC, TX_DW5_RTS_SC_MSK, TX_DW5_RTS_SC_SH);
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword5, TX_DESC_POWER_OFFSET, TX_DW5_TXPWR_OFSET_MSK, TX_DW5_TXPWR_OFSET_SH);
	if (Adapter->pmib->dot11RFEntry.bcn2path || Adapter->pmib->dot11RFEntry.tx2path)
		SET_DESC_FIELD_NO_CLR(ptx_desc->Dword5, TX_ANT, TX_DW5_TX_ANT_MSK, TX_DW5_TX_ANT_SH);

#if defined(AP_SWPS_OFFLOAD) && IS_EXIST_RTL8192FE
	if ( IS_HARDWARE_TYPE_8192FE(Adapter) ) {
      if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter))
        SET_TX_DESC_DROP_ID(ptx_desc,TX_DESC_DROP_ID);
	}
#endif	

    //4 Set Dword6
#if CFG_HAL_SUPPORT_MBSSID
    if (HAL_IS_VAP_INTERFACE(Adapter)) {
    // set MBSSID for each VAP_ID
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword6,HAL_VAR_VAP_INIT_SEQ, TX_DW6_MBSSID_MSK, TX_DW6_MBSSID_SH);  
    }         
#endif //#if CFG_HAL_SUPPORT_MBSSID

#if defined(AP_SWPS_OFFLOAD) && IS_EXIST_RTL8192FE
	if ( IS_HARDWARE_TYPE_8192FE(Adapter) ) {
      if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter))
        SET_TX_DESC_SWPS_SEQ(ptx_desc, TX_DESC_SWPS_SEQ);
	}
#endif


#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))
   if(TX_DESC_ANTSEL & BIT(0))
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword6, TX_DESC_ANTSEL_A, TX_DW6_ANTSEL_A_MSK, TX_DW6_ANTSEL_A_SH);
   if(TX_DESC_ANTSEL & BIT(1))
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword6, TX_DESC_ANTSEL_B, TX_DW6_ANTSEL_B_MSK, TX_DW6_ANTSEL_B_SH);
   if(TX_DESC_ANTSEL & BIT(2))
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword6, TX_DESC_ANTSEL_C, TX_DW6_ANTSEL_C_MSK, TX_DW6_ANTSEL_C_SH);
#endif	//#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))

    //4 Set Dword7
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword7, TX_DESC_TXBUFF, TX_DW7_SW_TXBUFF_MSK, TX_DW7_SW_TXBUFF_SH);

	//4 Set Dword8
    //4 Set Dword9
#if CFG_HAL_HW_SEQ
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, 1, TX_DW8_EN_HWSEQ_MSK, TX_DW8_EN_HWSEQ_SH);
#else
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword9, TX_DESC_SEQ, TX_DW9_SEQ_MSK, TX_DW9_SEQ_SH);    
#endif

#if IS_RTL88XX_MAC_V2
    if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2) {
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
		SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, TX_DESC_PKT_OFFSET, TX_DW1_PKT_OFFSET_MSK, TX_DW1_PKT_OFFSET_SH);
		SET_DESC_FIELD_NO_CLR(ptx_desc->Dword3, TX_DESC_WHEADER_LEN, TX_DW3_WHEADER_V1_MSK, TX_DW3_WHEADER_V1_SH);
		SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, TX_DESC_MAC_CP, TX_DW8_MAC_CP_MSK, TX_DW8_MAC_CP_SH);
		SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, TX_DESC_SMH_EN, TX_DW8_SMH_EN_MSK, TX_DW8_SMH_EN_SH);
#endif
#if CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC
        if(IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC(Adapter)) {
		    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, TX_DESC_TXWIFI_CP, TX_DW8_TXWIFI_CP_MSK, TX_DW8_TXWIFI_CP_SH);
        }
#endif
	}
#endif // IS_RTL88XX_MAC_V2


#if 0
	3.) STW_ANT_DIS:  
	ant_mapA, ant_mapB, ant_mapC, ant_mapD, ANTSEL_A, ANTSEL_B, Ntx_map, TXPWR_OFFSET 
	4.) STW_RATE_DIS:  
	USE_RATE, Data rate, DATA_SHORT, DATA_BW, TRY_RATE
	5.) STW_RB_DIS:  
	RATE_ID, DISDATAFB, DISRTSFB, RTS_RATEFB_LMT, DATA_RATEFB_LMT
	6.) STW_PKTRE_DIS:	
	RTY_LMT_EN,  DATA_RT_LMT,  BAR_RTY_TH
#endif

#if 0
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword5, 0x7, TX_DW5_TXPWR_OFSET_MSK, TX_DW5_TXPWR_OFSET_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword6, 0x7, TX_DW6_ANTSEL_A_MSK, TX_DW6_ANTSEL_A_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword6, 0x7, TX_DW6_ANTSEL_B_MSK, TX_DW6_ANTSEL_B_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword6, 0x3, TX_DW6_ANT_MAP_A_MSK, TX_DW6_ANT_MAP_A_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword6, 0x3, TX_DW6_ANT_MAP_B_MSK, TX_DW6_ANT_MAP_B_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword6, 0x3, TX_DW6_ANT_MAP_C_MSK, TX_DW6_ANT_MAP_C_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword6, 0x3, TX_DW6_ANT_MAP_D_MSK, TX_DW6_ANT_MAP_D_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword7, 0xF, TX_DW7_NTX_MAP_MSK, TX_DW7_NTX_MAP_SH);
#endif

#if 0
	if(queueIndex == HCI_TX_DMA_QUEUE_BE) {
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword3, 0x1, TX_DW3_USERATE_MSK, TX_DW3_USERATE_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, 0x7F, TX_DW4_DATARATE_MSK, TX_DW4_DATARATE_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, 0x1, TX_DW4_TRY_RATE_MSK, TX_DW4_TRY_RATE_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword5, 0x1, TX_DW5_DATA_SHORT_MSK, TX_DW5_DATA_SHORT_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword5, 0x3, TX_DW5_DATA_BW_MSK, TX_DW5_DATA_BW_SH);
	}
#endif

#if 0
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, 0x1F, TX_DW1_RATE_ID_MSK, TX_DW1_RATE_ID_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword3, 0x1, TX_DW3_DISDATAFB_MSK, TX_DW3_DISDATAFB_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword3, 0x1, TX_DW3_DISRTSFB_MSK, TX_DW3_DISRTSFB_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, 0xF, TX_DW4_RTS_RATEFB_LMT_MSK, TX_DW4_RTS_RATEFB_LMT_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, 0x1F, TX_DW4_DATA_RATEFB_LMT_MSK, TX_DW4_DATA_RATEFB_LMT_SH);
#endif

#if 0
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, 0x1, TX_DW4_RTY_LMT_EN_MSK, TX_DW4_RTY_LMT_EN_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, 0x3F, TX_DW4_DATA_RT_LMT_MSK, TX_DW4_DATA_RT_LMT_SH);
	// TODO: bug ? still copy this field into desc
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, 0x3, TX_DW8_BAR_RTY_TH_MSK, TX_DW8_BAR_RTY_TH_SH);
#endif

#ifdef AP_SWPS_OFFLOAD
    tx_head     = &(cur_q->host_idx);
    pswdescinfo = get_txdesc_info(Adapter->pshare->pdesc_info, queueIndex);
    pdescinfo = pswdescinfo + *tx_head;  

    if(pdescinfo->ptxdesc == NULL)
        pdescinfo->ptxdesc = kmalloc(sizeof(TX_DESC_88XX),GFP_KERNEL);
    
    memcpy((PVOID)pdescinfo->ptxdesc, (PVOID)ptx_desc, sizeof(TX_DESC_88XX));
#endif


#ifdef TRXBD_CACHABLE_REGION
//    _dma_cache_wback((unsigned long)((PVOID)ptx_desc-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(TX_DESC_88XX));
#endif //#ifdef TRXBD_CACHABLE_REGION
}

#endif //#if IS_EXIST_RTL8192EE || IS_EXIST_RTL8881AEM || IS_EXIST_RTL8814AE 

VOID
UpdateSWTXBDHostIdx88XX (
    IN      HAL_PADAPTER                    Adapter,
    IN      PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q
)
{
    cur_q->host_idx++;
    cur_q->host_idx = cur_q->host_idx % cur_q->total_txbd_num;
    cur_q->avail_txbd_num--;
}

enum _TxPktFinalIO88XX_FLAG_{
    TxPktFinalIO88XX_WRITE = 0,
    TxPktFinalIO88XX_CHECK = 1
};

static enum rt_status
TxPktFinalIO88XX(
    IN      HAL_PADAPTER                    Adapter,
    IN      PTX_BUFFER_DESCRIPTOR           cur_txbd,
    IN      u4Byte                          CtrlFlag,   //enum _TxPktFinalIO88XX_FLAG_{
    IN      u4Byte                          DwordSettingValue
)
{
    switch(CtrlFlag) {
        case TxPktFinalIO88XX_WRITE:
            SET_DESC_FIELD_CLR(cur_txbd->TXBD_ELE[0].Dword0, DwordSettingValue, 
                TXBD_DW0_PSLEN_MSK, TXBD_DW0_PSLEN_SH);
            return RT_STATUS_SUCCESS;
            break;

        case TxPktFinalIO88XX_CHECK:
            if (0 == GET_DESC_FIELD(cur_txbd->TXBD_ELE[0].Dword0, 
                        TXBD_DW0_PSLEN_MSK, TXBD_DW0_PSLEN_SH)) {
                RT_TRACE(COMP_SEND, DBG_WARNING, 
                    ("cur_txbd->TXBD_ELE[0].Dword0 value(0x%lx) error\n", cur_txbd->TXBD_ELE[0].Dword0));
                return RT_STATUS_FAILURE;
            } else {
                return RT_STATUS_SUCCESS;
            }
            break;

        default:
            // Error Case
            RT_TRACE(COMP_SEND, DBG_SERIOUS, ("TxPktFinalIO88XX setting error: 0x%x \n", CtrlFlag));
            return RT_STATUS_FAILURE;
            break;
    }
}


HAL_IMEM
enum rt_status
SyncSWTXBDHostIdxToHW88XX (
    IN      HAL_PADAPTER    Adapter,
    IN      u4Byte          queueIndex  //HCI_TX_DMA_QUEUE_88XX
)
{
    PHCI_TX_DMA_MANAGER_88XX        ptx_dma;
    PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q;
    u2Byte                          LastHostIdx;

    ptx_dma = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);
    cur_q   = &(ptx_dma->tx_queue[queueIndex]);

    
    if ( cur_q->host_idx == 0 ) {
        LastHostIdx = cur_q->total_txbd_num - 1;
    }
    else {
        LastHostIdx = cur_q->host_idx - 1;
    }
#if WLAN_HAL_TXDESC_CHECK_ADDR_LEN
#if IS_EXIST_RTL8881AEM
    if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
    pu4Byte pTxDescPhyAddr, pTxDescLen, pTxPSBLen;
    PTX_BUFFER_DESCRIPTOR cur_txbd;

    cur_txbd       = (PTX_BUFFER_DESCRIPTOR)(_GET_HAL_DATA(Adapter)->cur_txbd);
#if 0
    pTxDescLen     = HAL_TO_NONCACHE_ADDR((unsigned long)(&(_GET_HAL_DATA(Adapter)->cur_tx_desc_len)));
    pTxDescPhyAddr = HAL_TO_NONCACHE_ADDR((unsigned long)(&(_GET_HAL_DATA(Adapter)->cur_tx_desc_phy_addr)));
#endif
    pTxPSBLen      = HAL_TO_NONCACHE_ADDR((unsigned long)(&(_GET_HAL_DATA(Adapter)->cur_tx_psb_len)));


    if (*pTxPSBLen != GET_DESC_FIELD(cur_txbd->TXBD_ELE[0].Dword0, TXBD_DW0_PSLEN_MSK, TXBD_DW0_PSLEN_SH)) {
        panic_printk("%s(%d): Fail: CopyPSBLen:0x%x, PSBLen:0x%x \n", __FUNCTION__, __LINE__, *pTxPSBLen, GET_DESC_FIELD(cur_txbd->TXBD_ELE[0].Dword0, TXBD_DW0_PSLEN_MSK, TXBD_DW0_PSLEN_SH));
    } else {
        //panic_printk("%s(%d): Pass: CopyPSBLen:0x%x, PSBLen:0x%x \n", __FUNCTION__, __LINE__, *pTxPSBLen, GET_DESC_FIELD(cur_txbd->TXBD_ELE[0].Dword0, TXBD_DW0_PSLEN_MSK, TXBD_DW0_PSLEN_SH));    
    }

#if 0
    if (*pTxDescPhyAddr != (cur_txbd->TXBD_ELE[0].Dword1)) {
        panic_printk("%s(%d): Fail: copyPhyAddr:0x%x, phyAddr:0x%x \n", __FUNCTION__, __LINE__, *pTxDescPhyAddr, cur_txbd->TXBD_ELE[0].Dword1);
    } else {  
      //  panic_printk("%s(%d): Pass: copyPhyAddr:0x%x, phyAddr:0x%x \n", __FUNCTION__, __LINE__, *pTxDescPhyAddr, cur_txbd->TXBD_ELE[0].Dword1);
    }

    if (*pTxDescLen != (cur_txbd->TXBD_ELE[0].Dword0)) {
         panic_printk("%s(%d): Fail: copyTxDescLen:0x%x, txDescLen:0x%x \n", __FUNCTION__, __LINE__, *pTxDescLen, cur_txbd->TXBD_ELE[0].Dword0);
    } else {
       //  panic_printk("%s(%d): Pass: copyTxDescLen:0x%x, txDescLen:0x%x \n", __FUNCTION__, __LINE__, *pTxDescLen, cur_txbd->TXBD_ELE[0].Dword0);
    }
#endif
   }
#endif // IS_EXIST_RTL8881AEM
#endif // WLAN_HAL_TXDESC_CHECK_ADDR_LEN
    //Avoid that IO operation is completed before Packet is put into DRAM
    //So, add one read operation
    if (RT_STATUS_SUCCESS == TxPktFinalIO88XX(Adapter, (cur_q->pTXBD_head + LastHostIdx), TxPktFinalIO88XX_CHECK, 0)) {
        HAL_RTL_W16(cur_q->reg_rwptr_idx, cur_q->host_idx);

    } else {
        RT_TRACE_F(COMP_SEND, DBG_WARNING, ("TxPktFinalIO88XX check fail !!! cur_q->host_idx:0x%lx \n", cur_q->host_idx));
    }
//    RT_TRACE_F(COMP_SEND, DBG_TRACE, ("cur_q->host_idx:0x%lx \n", cur_q->host_idx))

    return RT_STATUS_SUCCESS;
}
#if defined(AP_SWPS_OFFLOAD)

#if IS_RTL8192F_SERIES
VOID ReprepareFillTxDescSetTxBD_92f(
    IN      HAL_PADAPTER    Adapter,
    IN      struct stat_info *pstat,
    IN      u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN      struct reprepare_info* pkt_info
){
    PHCI_TX_DMA_MANAGER_88XX        ptx_dma;
    PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q;
    PTX_BUFFER_DESCRIPTOR           cur_txbd;
    u4Byte                          TXDESCSize;
    unsigned long                   dma_addr;
    u4Byte                          TotalLen    = 0;
    PTX_DESC_88XX                   ptx_desc;
    struct tx_desc_info	*pswdescinfo, *pdescinfo;
    u2Byte              *tx_head;
    unsigned char		*pwlhdr;
#ifdef WLAN_HAL_TX_AMSDU    
    void                *buf_amsdu[WLAN_HAL_TX_AMSDU_MAX_NUM];
#endif
#if CFG_HAL_TX_AMSDU
    PHCI_TX_AMSDU_DMA_MANAGER_88XX  ptx_dma_amsdu;
    PTX_BUFFER_DESCRIPTOR_AMSDU     cur_txbd_amsdu;
#endif
#ifndef SMP_SYNC
    //unsigned long flags;
#endif
    struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)Adapter;

    ptx_dma     = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);
    cur_q       = &(ptx_dma->tx_queue[queueIndex]);
    cur_txbd    = cur_q->pTXBD_head + cur_q->host_idx;
    ptx_desc    = ((PTX_DESC_88XX)(cur_q->ptx_desc_head)) + cur_q->host_idx;

//3 SET TXDESC
    memcpy((PVOID)ptx_desc, (PVOID)pkt_info->ptxdesc, sizeof(TX_DESC_88XX));
    tx_head     = &(cur_q->host_idx);
    pswdescinfo = get_txdesc_info(Adapter->pshare->pdesc_info, queueIndex);
    pdescinfo = pswdescinfo + *tx_head;
    
    if(pdescinfo->ptxdesc!=NULL)
        kfree(pdescinfo->ptxdesc);

    pdescinfo->ptxdesc = pkt_info->ptxdesc;
    if(pkt_info->llchdr){
        pdescinfo->type = _PRE_ALLOCLLCHDR_;
        pwlhdr = pkt_info->llchdr;
        //printk("[%s]pkt type = _PRE_ALLOCLLCHDR_\n",__FUNCTION__);
    }
    else if(pkt_info->hdr){
        pdescinfo->type = _PRE_ALLOCHDR_;
        pwlhdr = pkt_info->hdr;
        //printk("[%s]pkt type = _PRE_ALLOCHDR_\n",__FUNCTION__);
    }
#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV    
    else if(pkt_info->isWiFiHdr == FALSE){
        //no wlan header
    }
#endif    
    else    
        GDEBUG("[%s][%s]wrong header type!!\n",__FUNCTION__,__LINE__);//debug

#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
	if (pkt_info->isWiFiHdr == FALSE) {
		pdescinfo->pframe = NULL;
	} else
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
	{
	    pdescinfo->pframe = pwlhdr;
	}

#if defined(WIFI_WMM) && defined(WMM_APSD)
	pdescinfo->priv = Adapter;
#ifndef TXDESC_INFO
	pdescinfo->pstat = pstat;
#endif		
#endif

    if(pkt_info->mic_icv_len && pkt_info->icv){
        pdescinfo->buf_type[1]   = _PRE_ALLOCICVHDR_;
        pdescinfo->buf_pframe[1] = pkt_info->icv;
//        pdescinfo->buf_len[1] = pkt_info->mic_icv_len;
    }else if(pkt_info->mic_icv_len && pkt_info->mic){
        pdescinfo->buf_type[1]   = _PRE_ALLOCMICHDR_;
        pdescinfo->buf_pframe[1] = pkt_info->mic;
//        pdescinfo->buf_len[1] = pkt_info->mic_icv_len;
    }
    
    pdescinfo->swps_pstat = pstat;

    if(pstat->EnSWPS){
                
        int pkt_Qos = queueIndex-1;
        
        if(pkt_Qos < 0)
        {
            pdescinfo->SWPS_pkt_Qos = pkt_Qos;
        }
        else
        {
            SET_TX_DESC_SWPS_SEQ(ptx_desc, pstat->SWPS_seq[pkt_Qos]); 
    
            pdescinfo->SWPS_sequence = pstat->SWPS_seq[pkt_Qos];
            //pdescinfo->SWPS_pktQ = queueIndex;
            pdescinfo->SWPS_pkt_Qos = pkt_Qos;
            

        	//SAVE_INT_AND_CLI(flags); //may not need
            pstat->SWPS_seq_head[pkt_Qos] = pdescinfo->SWPS_sequence;
   
            if(pstat->SWPS_seq[pkt_Qos] == 0xFFF)
                pstat->SWPS_seq[pkt_Qos] = 1;
            else        
                pstat->SWPS_seq[pkt_Qos]++;
            //RESTORE_INT(flags);
            GDEBUG("assign new swps seq = %x\n",pdescinfo->SWPS_sequence);
        }
    }
    else{
        SET_TX_DESC_SWPS_SEQ(ptx_desc, 0); 
        pdescinfo->SWPS_sequence = 0;
        //pdescinfo->SWPS_pktQ = 0; 
        pdescinfo->SWPS_pkt_Qos = 0; // no use, just to prevent get no qos situation.
    }
        
    pdescinfo->Drop_ID = pstat->Drop_ID;
    SET_TX_DESC_DROP_ID(ptx_desc,pstat->Drop_ID);

    
    if(pkt_info->amsdu_flag){
#ifdef WLAN_HAL_TX_AMSDU           
        *(&buf_amsdu[0]) = (void*)pkt_info->pSkb; //need to check if this equotion is ok
        int amsdu_i = 0;
        for(amsdu_i=0;amsdu_i<pkt_info->amsdu_num;amsdu_i++){
            
            pdescinfo->buf_pframe_amsdu[amsdu_i] = buf_amsdu[amsdu_i];
            pdescinfo->buf_type_amsdu[amsdu_i] = _SKB_FRAME_TYPE_;
            pdescinfo->amsdubuf_len[amsdu_i] = pkt_info->amsdubuf_len[amsdu_i];
            
        }
        pdescinfo->amsdu_num = pkt_info->amsdu_num;
        pdescinfo->buf_pframe[0] = NULL;
        pdescinfo->buf_type[0] = _RESERVED_FRAME_TYPE_;
//        pdescinfo->buf_len[0] = 0;
#endif                
    }else{
        pdescinfo->buf_pframe[0] = pkt_info->pSkb;
        pdescinfo->buf_type[0] = _SKB_FRAME_TYPE_;
//        pdescinfo->buf_len[0]  = pkt_info->skbbuf_len; 
#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
        if (pkt_info->isWiFiHdr == FALSE){
            pdescinfo->type          = _SKB_FRAME_TYPE_; // 802.3 header + payload
            pdescinfo->pframe        = pkt_info->pSkb; // skb
           
            pdescinfo->buf_type[0]   = 0;
            pdescinfo->buf_pframe[0] = 0; // no packet payload
        }
#endif
    }
    

    //pdescinfo->buf_paddr[cnt] = ??
    //pdescinfo->paddr = ??
    //pdescinfo->len
    
    //2 need add AMSDU, shortcut HDR/TXDESC
    

//3 FILL TXBD

#if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8192FE)
    if ( IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8814AE(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8192F(Adapter) ) {
        TXDESCSize = SIZE_TXDESC_88XX;
    }
#endif  //#if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM  || IS_EXIST_RTL8192FE)

#if (IS_EXIST_RTL8822BE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE) 
    if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter) )  {
        TXDESCSize = SIZE_TXDESC_88XX_V1;
    }
#endif //(IS_EXIST_RTL8822BE )

#if CFG_HAL_TX_AMSDU
    if ( IS_SUPPORT_TX_AMSDU(Adapter) && pkt_info->amsdu_flag ) {
        ptx_dma_amsdu  = (PHCI_TX_AMSDU_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMAAMSDU88XX);
        cur_txbd_amsdu = ptx_dma_amsdu->tx_amsdu_queue[queueIndex-1].pTXBD_head_amsdu + cur_q->host_idx;
    }

    //if (pdesc_data->aggreEn <= FG_AGGRE_MSDU_FIRST)
#endif
    {
        platform_zero_memory(&(cur_txbd->TXBD_ELE[1]), sizeof(TXBD_ELEMENT)*(TXBD_ELE_NUM-1));
    }

#if CFG_HAL_TX_AMSDU
    //if (pdesc_data->aggreEn <= FG_AGGRE_MSDU_FIRST)
#endif
    {
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[1].Dword0,
                            pkt_info->hdr_len, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
        if (pkt_info->isWiFiHdr == FALSE){
            dma_addr = HAL_VIRT_TO_BUS1(Adapter, (PVOID)((struct sk_buff *)pkt_info->pSkb)->data, pkt_info->hdr_len, HAL_PCI_DMA_TODEVICE);
            SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[1].Dword1,
                        dma_addr + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                        TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
        }else
#endif        
        {
        
            dma_addr = HAL_VIRT_TO_BUS1(Adapter, (PVOID)pwlhdr, pkt_info->hdr_len, HAL_PCI_DMA_TODEVICE);
            SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[1].Dword1,
                        dma_addr + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                        TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
        }
    }

#if (TXBD_ELE_NUM >= 4)

#if CFG_HAL_TX_AMSDU
    if ( IS_SUPPORT_TX_AMSDU(Adapter) && pkt_info->amsdu_flag) {
        // postpone fill this field until the last payload (i.e., FG_AGGRE_MSDU_LAST), because we don't know the exact numbers of the MSDU in this AMSDU
        //SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword0, sizeof(TX_BUFFER_DESCRIPTOR_AMSDU), TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword0,
                    1, TXBD_DW0_EXTENDTXBUF_MSK, TXBD_DW0_EXTENDTXBUF_SH);

        // (queueIndex-1) for mapping HCI_TX_DMA_QUEUE_88XX to HCI_TX_AMSDU_DMA_QUEUE_88XX
        ptx_dma_amsdu->tx_amsdu_queue[queueIndex-1].cur_txbd_element = pkt_info->amsdu_num;

        // fill the exact MSDU numbers into TXBD
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword0, 
            ptx_dma_amsdu->tx_amsdu_queue[queueIndex-1].cur_txbd_element * sizeof(TXBD_ELEMENT), 
            TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);

        
        platform_zero_memory(cur_txbd_amsdu, sizeof(TX_BUFFER_DESCRIPTOR_AMSDU));

        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword1,
                HAL_VIRT_TO_BUS1(Adapter, (PVOID)cur_txbd_amsdu, sizeof(TX_BUFFER_DESCRIPTOR_AMSDU), HAL_PCI_DMA_TODEVICE) + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
    }
#endif // CFG_HAL_TX_AMSDU


#if CFG_HAL_TX_AMSDU
    if ( IS_SUPPORT_TX_AMSDU(Adapter) && pkt_info->amsdu_flag) {
        int amsdu_i=0;
        for(amsdu_i = 0;amsdu_i< pkt_info->amsdu_num;amsdu_i++){
        SET_DESC_FIELD_NO_CLR(cur_txbd_amsdu->TXBD_ELE[amsdu_i].Dword0,
                    pkt_info->amsdubuf_len[amsdu_i], TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        
        SET_DESC_FIELD_NO_CLR(cur_txbd_amsdu->TXBD_ELE[amsdu_i].Dword1,
                HAL_VIRT_TO_BUS1(Adapter, (PVOID)((struct sk_buff *)buf_amsdu[amsdu_i])->data, pkt_info->amsdubuf_len[amsdu_i], HAL_PCI_DMA_TODEVICE) + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);

		
#if defined(CONFIG_NET_PCI) && defined(NOT_RTK_BSP)
		HAL_CACHE_SYNC_WBACK(Adapter, 
		GET_DESC_FIELD(cur_txbd_amsdu->TXBD_ELE[amsdu_i].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL, 
		GET_DESC_FIELD(cur_txbd_amsdu->TXBD_ELE[amsdu_i].Dword0, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH), 
		HAL_PCI_DMA_TODEVICE);
#else
	if(pkt_info->amsdubuf_len[amsdu_i])

		_dma_cache_wback(((GET_DESC_FIELD(cur_txbd_amsdu->TXBD_ELE[amsdu_i].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
            pkt_info->amsdubuf_len[amsdu_i]);

#endif
		
        }
        //if (pdesc_data->aggreEn != FG_AGGRE_MSDU_LAST) {
            //for FG_AGGRE_MSDU_FIRST or FG_AGGRE_MSDU_MIDDLE
            //return;
        //}
    } else
#endif
    {
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword0,
                    pkt_info->skbbuf_len, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);

        if (pkt_info->skbbuf_len) {
            dma_addr = HAL_VIRT_TO_BUS1(Adapter, (PVOID)((struct sk_buff *)pkt_info->pSkb)->data, pkt_info->skbbuf_len, HAL_PCI_DMA_TODEVICE);
            SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword1,
                   dma_addr + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                    TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
        }
    }
    // for sw encryption: 1) WEP's icv and TKIP's icv, 2) CCMP's mic, 3) no security
    if (pkt_info->icv != NULL) {
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[3].Dword0,
                    pkt_info->mic_icv_len,
                    TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        
        dma_addr = HAL_VIRT_TO_BUS1(Adapter, (PVOID)pkt_info->icv, pkt_info->mic_icv_len, HAL_PCI_DMA_TODEVICE);
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[3].Dword1,
                    dma_addr + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                    TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);

    } else if (pkt_info->mic != NULL) {
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[3].Dword0,
                    pkt_info->mic_icv_len,
                    TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        
        dma_addr = HAL_VIRT_TO_BUS1(Adapter, (PVOID)pkt_info->mic, pkt_info->mic_icv_len, HAL_PCI_DMA_TODEVICE);
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[3].Dword1,
                    dma_addr + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                    TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);

    } else {
        cur_txbd->TXBD_ELE[3].Dword0 = SET_DESC(0);

    }
#else
    #error "Error, TXBD_ELE_NUM<4 is invalid Setting unless we modify overall architecture"
#endif  //   (TXBD_ELE_NUM >= 4) 

    //3 Final one HW IO of Tx Pkt
    TxPktFinalIO88XX(Adapter, cur_txbd, TxPktFinalIO88XX_WRITE, pkt_info->PSB_len);

#ifdef TRXBD_CACHABLE_REGION
    _dma_cache_wback((unsigned long)((PVOID)(cur_txbd)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(TX_BUFFER_DESCRIPTOR));

    _dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[0].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),TXDESCSize);

    _dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),(u4Byte)pkt_info->hdr_len);

#if CFG_HAL_TX_AMSDU
    if ( IS_SUPPORT_TX_AMSDU(Adapter) && pkt_info->amsdu_flag) {
    	_dma_cache_wback((unsigned long)((PVOID)(cur_txbd_amsdu)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(TX_BUFFER_DESCRIPTOR_AMSDU));
    }//else
#endif // CFG_HAL_TX_AMSDU
    {

        {
			if (pkt_info->hdr_len) {
			    _dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		   		    (u4Byte)pkt_info->hdr_len);
			}

			if (pkt_info->skbbuf_len) {
			    _dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[2].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		   		    (u4Byte)pkt_info->skbbuf_len);
			} 

			if (pkt_info->icv) {
				_dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[3].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		   		    pkt_info->mic_icv_len);
			} else if (pkt_info->mic) {
				_dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[3].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		   		    pkt_info->mic_icv_len);
			}

        }
    }

// TODO:  write back icv/mic on 8198C

// TODO: consider both enable TX_AMSDU and HW_TX_SHORTCUT...

#else // !TRXBD_CACHABLE_REGION
#if defined(CONFIG_NET_PCI) && defined(NOT_RTK_BSP)
        // hdr/payload/icv/mic has sync via pci_map_single in get_physical_addr
     if (!HAL_IS_PCIBIOS_TYPE(Adapter))
#endif
     {
        if (pkt_info->hdr_len) {
            HAL_CACHE_SYNC_WBACK(Adapter, 
                GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL, 
                pkt_info->hdr_len, 
                HAL_PCI_DMA_TODEVICE);
        }
        
        if (pkt_info->skbbuf_len) {
            HAL_CACHE_SYNC_WBACK(Adapter, 
                GET_DESC_FIELD(cur_txbd->TXBD_ELE[2].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL, 
                pkt_info->skbbuf_len, 
                HAL_PCI_DMA_TODEVICE);
        }

        if (pkt_info->icv != NULL) {
            HAL_CACHE_SYNC_WBACK(Adapter, 
                GET_DESC_FIELD(cur_txbd->TXBD_ELE[3].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL, 
                pkt_info->mic_icv_len, 
                HAL_PCI_DMA_TODEVICE);
        } else if (pkt_info->mic != NULL) {
            HAL_CACHE_SYNC_WBACK(Adapter, 
                GET_DESC_FIELD(cur_txbd->TXBD_ELE[3].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                pkt_info->mic_icv_len, 
                HAL_PCI_DMA_TODEVICE);
        }
    }
#ifdef CONFIG_NET_PCI
    if (HAL_IS_PCIBIOS_TYPE(Adapter)) {
#if CFG_HAL_TX_AMSDU
	if (pkt_info->amsdu_flag)
		HAL_CACHE_SYNC_WBACK(Adapter, _GET_HAL_DATA(Adapter)->desc_dma_buf_addr_amsdu[queueIndex-1] + cur_q->host_idx * sizeof(TX_BUFFER_DESCRIPTOR_AMSDU), sizeof(TX_BUFFER_DESCRIPTOR_AMSDU), HAL_PCI_DMA_TODEVICE);
#endif
        HAL_CACHE_SYNC_WBACK(Adapter, _GET_HAL_DATA(Adapter)->txDesc_dma_ring_addr[queueIndex] + cur_q->host_idx * sizeof(TX_DESC_88XX), sizeof(TX_DESC_88XX), HAL_PCI_DMA_TODEVICE);
        HAL_CACHE_SYNC_WBACK(Adapter, _GET_HAL_DATA(Adapter)->txBD_dma_ring_addr[queueIndex] + cur_q->host_idx * sizeof(TX_BUFFER_DESCRIPTOR), sizeof(TX_BUFFER_DESCRIPTOR), HAL_PCI_DMA_TODEVICE);
    }
#endif
#endif // TRXBD_CACHABLE_REGION

    UpdateSWTXBDHostIdx88XX(Adapter, cur_q);

#if 1 //eric-8822 CFG_HAL_DBG

    RT_TRACE_F(COMP_SEND, DBG_TRACE, ("\nq_idx: %d, txbd[%d], \n", queueIndex, cur_q->host_idx));

    //TXDESC
    RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%08lx, Dword1: 0x%08lx\n", 0, GET_DESC(cur_txbd->TXBD_ELE[0].Dword0), GET_DESC(cur_txbd->TXBD_ELE[0].Dword1)));
    //PTX_DESC_88XX   ptx_desc = ((PTX_DESC_88XX)(cur_q->ptx_desc_head)) + cur_q->host_idx;
    RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "TXDESC:\n", ptx_desc, TXDESCSize);

    //Header
    RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%08lx, Dword1: 0x%08lx\n", 1, GET_DESC(cur_txbd->TXBD_ELE[1].Dword0), GET_DESC(cur_txbd->TXBD_ELE[1].Dword1)));
    
    RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "Header:\n", pwlhdr,pkt_info->hdr_len );
    

#if 1
    //Payload
    RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%08lx, Dword1: 0x%08lx\n", 2, GET_DESC(cur_txbd->TXBD_ELE[2].Dword0), GET_DESC(cur_txbd->TXBD_ELE[2].Dword1)));
    RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "Payload:\n",(PVOID)((struct sk_buff *)pkt_info->pSkb)->data, pkt_info->skbbuf_len);    

    //MIC or ICV
    //RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%08lx, Dword1: 0x%08lx\n", 3, GET_DESC(cur_txbd->TXBD_ELE[3].Dword0), GET_DESC(cur_txbd->TXBD_ELE[3].Dword1)));
    //RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "Icv:\n", pdesc_data->pIcv, pdesc_data->icv);
#endif
#endif  //CFG_HAL_DBG

}

#endif

#if IS_RTL88XX_MAC_V3
VOID
ReprepareFillTxDescSetTxBD(
    IN      HAL_PADAPTER    Adapter,
    IN      struct stat_info *pstat,
    IN      u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN      struct reprepare_info* pkt_info
    //IN      struct apsd_pkt_queue* AC_dz_queue,
    //IN      int             pkt_position
){
    PHCI_TX_DMA_MANAGER_88XX        ptx_dma;
    PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q;
    PTX_BUFFER_DESCRIPTOR           cur_txbd;
    u4Byte                          TXDESCSize;
    unsigned long                   dma_addr;
    u4Byte                          TotalLen    = 0;
    PTX_DESC_88XX                   ptx_desc;
    struct tx_desc_info	*pswdescinfo, *pdescinfo;
    u2Byte              *tx_head;
    unsigned char		*pwlhdr;
#ifdef WLAN_HAL_TX_AMSDU    
    void                *buf_amsdu[WLAN_HAL_TX_AMSDU_MAX_NUM];
#endif
#if CFG_HAL_TX_AMSDU
    PHCI_TX_AMSDU_DMA_MANAGER_88XX  ptx_dma_amsdu;
    PTX_BUFFER_DESCRIPTOR_AMSDU     cur_txbd_amsdu;
#endif
#ifndef SMP_SYNC
    //unsigned long flags;
#endif
    struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)Adapter;

    ptx_dma     = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);
    cur_q       = &(ptx_dma->tx_queue[queueIndex]);
    cur_txbd    = cur_q->pTXBD_head + cur_q->host_idx;
    ptx_desc    = ((PTX_DESC_88XX)(cur_q->ptx_desc_head)) + cur_q->host_idx;

//3 SET TXDESC
    memcpy((PVOID)ptx_desc, (PVOID)pkt_info->ptxdesc, sizeof(TX_DESC_88XX));
    tx_head     = &(cur_q->host_idx);
    pswdescinfo = get_txdesc_info(Adapter,Adapter->pshare->pdesc_info, queueIndex);
    pdescinfo = pswdescinfo + *tx_head;
    
    if(pdescinfo->ptxdesc!=NULL)
        kfree(pdescinfo->ptxdesc);

    pdescinfo->ptxdesc = pkt_info->ptxdesc;
    if(pkt_info->llchdr){
        pdescinfo->type = _PRE_ALLOCLLCHDR_;
        pwlhdr = pkt_info->llchdr;
        //printk("[%s]pkt type = _PRE_ALLOCLLCHDR_\n",__FUNCTION__);
    }
    else if(pkt_info->hdr){
        pdescinfo->type = _PRE_ALLOCHDR_;
        pwlhdr = pkt_info->hdr;
        //printk("[%s]pkt type = _PRE_ALLOCHDR_\n",__FUNCTION__);
    }
#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV    
    else if(pkt_info->isWiFiHdr == FALSE){
        //no wlan header
    }
#endif    
    else    
        printk("[%s][%s]wrong header type!!\n",__FUNCTION__,__LINE__);//debug

#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
	if (pkt_info->isWiFiHdr == FALSE) {
		pdescinfo->pframe = NULL;
	} else
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
	{
	    pdescinfo->pframe = pwlhdr;
	}

#if defined(WIFI_WMM) && defined(WMM_APSD)
	pdescinfo->priv = Adapter;
#ifndef TXDESC_INFO
	pdescinfo->pstat = pstat;
#endif		
#endif

    if(pkt_info->mic_icv_len && pkt_info->icv){
        pdescinfo->buf_type[1]   = _PRE_ALLOCICVHDR_;
        pdescinfo->buf_pframe[1] = pkt_info->icv;
        pdescinfo->buf_len[1] = pkt_info->mic_icv_len;
    }else if(pkt_info->mic_icv_len && pkt_info->mic){
        pdescinfo->buf_type[1]   = _PRE_ALLOCMICHDR_;
        pdescinfo->buf_pframe[1] = pkt_info->mic;
        pdescinfo->buf_len[1] = pkt_info->mic_icv_len;
    }
    
    pdescinfo->swps_pstat = pstat;

    if(pstat->EnSWPS){
                
        int pkt_Qos = queueIndex-1;
        
        if(pkt_Qos < 0)
        {
            pdescinfo->SWPS_pkt_Qos = pkt_Qos;
        }
        else
        {
            SET_TX_DESC_SWPS_SEQ(ptx_desc, pstat->SWPS_seq[pkt_Qos]); 
    
            pdescinfo->SWPS_sequence = pstat->SWPS_seq[pkt_Qos];
            //pdescinfo->SWPS_pktQ = queueIndex;
            pdescinfo->SWPS_pkt_Qos = pkt_Qos;
            

        	//SAVE_INT_AND_CLI(flags); //may not need
            pstat->SWPS_seq_head[pkt_Qos] = pdescinfo->SWPS_sequence;
   
            if(pstat->SWPS_seq[pkt_Qos] == 0xFFF)
                pstat->SWPS_seq[pkt_Qos] = 1;
            else        
                pstat->SWPS_seq[pkt_Qos]++;
            //RESTORE_INT(flags);
            //printk("assign new swps seq = %x\n",pdescinfo->SWPS_sequence);
        }
    }
    else{
        SET_TX_DESC_SWPS_SEQ(ptx_desc, 0); 
        pdescinfo->SWPS_sequence = 0;
        //pdescinfo->SWPS_pktQ = 0; 
        pdescinfo->SWPS_pkt_Qos = 0; // no use, just to prevent get no qos situation.
    }
        
    pdescinfo->Drop_ID = pstat->Drop_ID;
    SET_TX_DESC_DROP_ID(ptx_desc,pstat->Drop_ID);

    
    if(pkt_info->amsdu_flag){
#ifdef WLAN_HAL_TX_AMSDU           
        *(&buf_amsdu[0]) = (void*)pkt_info->pSkb; //need to check if this equotion is ok
        int amsdu_i = 0;
        for(amsdu_i=0;amsdu_i<pkt_info->amsdu_num;amsdu_i++){
            
            pdescinfo->buf_pframe_amsdu[amsdu_i] = buf_amsdu[amsdu_i];
            pdescinfo->buf_type_amsdu[amsdu_i] = _SKB_FRAME_TYPE_;
            pdescinfo->amsdubuf_len[amsdu_i] = pkt_info->amsdubuf_len[amsdu_i];
            
        }
        pdescinfo->amsdu_num = pkt_info->amsdu_num;
        pdescinfo->buf_pframe[0] = NULL;
        pdescinfo->buf_type[0] = _RESERVED_FRAME_TYPE_;
        pdescinfo->buf_len[0] = 0;
#endif                
    }else{
        pdescinfo->buf_pframe[0] = pkt_info->pSkb;
        pdescinfo->buf_type[0] = _SKB_FRAME_TYPE_;
        pdescinfo->buf_len[0]  = pkt_info->skbbuf_len; 
#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
        if (pkt_info->isWiFiHdr == FALSE){
            pdescinfo->type          = _SKB_FRAME_TYPE_; // 802.3 header + payload
            pdescinfo->pframe        = pkt_info->pSkb; // skb
           
            pdescinfo->buf_type[0]   = 0;
            pdescinfo->buf_pframe[0] = 0; // no packet payload
        }
#endif
    }
    

    //pdescinfo->buf_paddr[cnt] = ??
    //pdescinfo->paddr = ??
    //pdescinfo->len
    
    //2 need add AMSDU, shortcut HDR/TXDESC
    

//3 FILL TXBD

#if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM)
    if ( IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8814AE(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) ) {
        TXDESCSize = SIZE_TXDESC_88XX;
    }
#endif  //#if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM  || IS_EXIST_RTL8198FEM)

#if (IS_EXIST_RTL8822BE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE || IS_EXIST_RTL8197GEM) 
    if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter) ||  IS_HARDWARE_TYPE_8197G(Adapter))  {
        TXDESCSize = SIZE_TXDESC_88XX_V1;
    }
#endif //(IS_EXIST_RTL8822BE || IS_EXIST_RTL8197GEM)

#if CFG_HAL_TX_AMSDU
    if ( IS_SUPPORT_TX_AMSDU(Adapter) && pkt_info->amsdu_flag ) {
        ptx_dma_amsdu  = (PHCI_TX_AMSDU_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMAAMSDU88XX);
        cur_txbd_amsdu = ptx_dma_amsdu->tx_amsdu_queue[queueIndex-1].pTXBD_head_amsdu + cur_q->host_idx;
    }

    //if (pdesc_data->aggreEn <= FG_AGGRE_MSDU_FIRST)
#endif
    {
        platform_zero_memory(&(cur_txbd->TXBD_ELE[1]), sizeof(TXBD_ELEMENT)*(TXBD_ELE_NUM-1));
    }

#if CFG_HAL_TX_AMSDU
    //if (pdesc_data->aggreEn <= FG_AGGRE_MSDU_FIRST)
#endif
    {
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[1].Dword0,
                            pkt_info->hdr_len, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
        if (pkt_info->isWiFiHdr == FALSE){
            dma_addr = HAL_VIRT_TO_BUS1(Adapter, (PVOID)((struct sk_buff *)pkt_info->pSkb)->data, pkt_info->hdr_len, HAL_PCI_DMA_TODEVICE);
            SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[1].Dword1,
                        dma_addr + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                        TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
        }else
#endif        
        {
        
            dma_addr = HAL_VIRT_TO_BUS1(Adapter, (PVOID)pwlhdr, pkt_info->hdr_len, HAL_PCI_DMA_TODEVICE);
            SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[1].Dword1,
                        dma_addr + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                        TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
        }
    }

#if (TXBD_ELE_NUM >= 4)

#if CFG_HAL_TX_AMSDU
    if ( IS_SUPPORT_TX_AMSDU(Adapter) && pkt_info->amsdu_flag) {
        // postpone fill this field until the last payload (i.e., FG_AGGRE_MSDU_LAST), because we don't know the exact numbers of the MSDU in this AMSDU
        //SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword0, sizeof(TX_BUFFER_DESCRIPTOR_AMSDU), TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword0,
                    1, TXBD_DW0_EXTENDTXBUF_MSK, TXBD_DW0_EXTENDTXBUF_SH);

        // (queueIndex-1) for mapping HCI_TX_DMA_QUEUE_88XX to HCI_TX_AMSDU_DMA_QUEUE_88XX
        ptx_dma_amsdu->tx_amsdu_queue[queueIndex-1].cur_txbd_element = pkt_info->amsdu_num;

        // fill the exact MSDU numbers into TXBD
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword0, 
            ptx_dma_amsdu->tx_amsdu_queue[queueIndex-1].cur_txbd_element * sizeof(TXBD_ELEMENT), 
            TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);

        
        platform_zero_memory(cur_txbd_amsdu, sizeof(TX_BUFFER_DESCRIPTOR_AMSDU));

        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword1,
                HAL_VIRT_TO_BUS1(Adapter, (PVOID)cur_txbd_amsdu, sizeof(TX_BUFFER_DESCRIPTOR_AMSDU), HAL_PCI_DMA_TODEVICE) + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
    }
#endif // CFG_HAL_TX_AMSDU


#if CFG_HAL_TX_AMSDU
    if ( IS_SUPPORT_TX_AMSDU(Adapter) && pkt_info->amsdu_flag) {
        int amsdu_i=0;
        for(amsdu_i = 0;amsdu_i< pkt_info->amsdu_num;amsdu_i++){
        SET_DESC_FIELD_NO_CLR(cur_txbd_amsdu->TXBD_ELE[amsdu_i].Dword0,
                    pkt_info->amsdubuf_len[amsdu_i], TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        
        SET_DESC_FIELD_NO_CLR(cur_txbd_amsdu->TXBD_ELE[amsdu_i].Dword1,
                HAL_VIRT_TO_BUS1(Adapter, (PVOID)((struct sk_buff *)buf_amsdu[amsdu_i])->data, pkt_info->amsdubuf_len[amsdu_i], HAL_PCI_DMA_TODEVICE) + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);

		
#if defined(CONFIG_NET_PCI) && defined(NOT_RTK_BSP)
		HAL_CACHE_SYNC_WBACK(Adapter, 
		GET_DESC_FIELD(cur_txbd_amsdu->TXBD_ELE[amsdu_i].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL, 
		GET_DESC_FIELD(cur_txbd_amsdu->TXBD_ELE[amsdu_i].Dword0, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH), 
		HAL_PCI_DMA_TODEVICE);
#else
	if(pkt_info->amsdubuf_len[amsdu_i])
#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
        // With CCI400 ,Do nothing for cache coherent code
#else
        rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD((unsigned long)bus_to_virt(cur_txbd_amsdu->TXBD_ELE[amsdu_i].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
            (u4Byte)pkt_info->amsdubuf_len[amsdu_i],PCI_DMA_TODEVICE);
#endif  //CONFIG_ENABLE_CCI400
#else
		_dma_cache_wback(((GET_DESC_FIELD(cur_txbd_amsdu->TXBD_ELE[amsdu_i].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
            pkt_info->amsdubuf_len[amsdu_i]);
#endif
#endif
		
        }
        //if (pdesc_data->aggreEn != FG_AGGRE_MSDU_LAST) {
            //for FG_AGGRE_MSDU_FIRST or FG_AGGRE_MSDU_MIDDLE
            //return;
        //}
    } else
#endif
    {
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword0,
                    pkt_info->skbbuf_len, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);

        if (pkt_info->skbbuf_len) {
            dma_addr = HAL_VIRT_TO_BUS1(Adapter, (PVOID)((struct sk_buff *)pkt_info->pSkb)->data, pkt_info->skbbuf_len, HAL_PCI_DMA_TODEVICE);
            SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword1,
                   dma_addr + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                    TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
        }
    }
    // for sw encryption: 1) WEP's icv and TKIP's icv, 2) CCMP's mic, 3) no security
    if (pkt_info->icv != NULL) {
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[3].Dword0,
                    pkt_info->mic_icv_len,
                    TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        
        dma_addr = HAL_VIRT_TO_BUS1(Adapter, (PVOID)pkt_info->icv, pkt_info->mic_icv_len, HAL_PCI_DMA_TODEVICE);
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[3].Dword1,
                    dma_addr + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                    TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);

    } else if (pkt_info->mic != NULL) {
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[3].Dword0,
                    pkt_info->mic_icv_len,
                    TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        
        dma_addr = HAL_VIRT_TO_BUS1(Adapter, (PVOID)pkt_info->mic, pkt_info->mic_icv_len, HAL_PCI_DMA_TODEVICE);
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[3].Dword1,
                    dma_addr + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                    TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);

    } else {
        cur_txbd->TXBD_ELE[3].Dword0 = SET_DESC(0);

    }
#else
    #error "Error, TXBD_ELE_NUM<4 is invalid Setting unless we modify overall architecture"
#endif  //   (TXBD_ELE_NUM >= 4) 

    //3 Final one HW IO of Tx Pkt
    TxPktFinalIO88XX(Adapter, cur_txbd, TxPktFinalIO88XX_WRITE, pkt_info->PSB_len);

#ifdef TRXBD_CACHABLE_REGION
#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
        // With CCI400 ,Do nothing for cache coherent code
#else
        rtl_cache_sync_wback(Adapter,(unsigned long)((PVOID)(cur_txbd)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(TX_BUFFER_DESCRIPTOR),PCI_DMA_TODEVICE);

        rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD((unsigned long)bus_to_virt(cur_txbd->TXBD_ELE[0].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
            TXDESCSize,PCI_DMA_TODEVICE);
        
#if 0//CFG_HAL_TX_AMSDU //no use in this case
            if ( IS_SUPPORT_TX_AMSDU(Adapter) ) {
                if (pdesc_data->aggreEn == FG_AGGRE_MSDU_LAST) {
                    hdrLen = GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword0, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
                }
            }
#endif // CFG_HAL_TX_AMSDU


        rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD((unsigned long)bus_to_virt(cur_txbd->TXBD_ELE[1].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
            (u4Byte)pkt_info->hdr_len,PCI_DMA_TODEVICE);

#if CFG_HAL_TX_AMSDU
        if ( IS_SUPPORT_TX_AMSDU(Adapter) && pkt_info->amsdu_flag) {
            rtl_cache_sync_wback(Adapter,(unsigned long)((PVOID)(cur_txbd_amsdu)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(TX_BUFFER_DESCRIPTOR_AMSDU),PCI_DMA_TODEVICE);
        }else
#endif // CFG_HAL_TX_AMSDU
        {
            if (pkt_info->hdr_len) {
                rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD((unsigned long)bus_to_virt(cur_txbd->TXBD_ELE[1].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
                    (u4Byte)pkt_info->hdr_len,PCI_DMA_TODEVICE);
            }
        
            if (pkt_info->skbbuf_len) {
                rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD((unsigned long)bus_to_virt(cur_txbd->TXBD_ELE[2].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
                    (u4Byte)pkt_info->skbbuf_len,PCI_DMA_TODEVICE);
            } 
            if (pkt_info->mic_icv_len && pkt_info->icv) {
                rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD((unsigned long)bus_to_virt(cur_txbd->TXBD_ELE[3].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
                    pkt_info->mic_icv_len,PCI_DMA_TODEVICE);
            } else if (pkt_info->mic_icv_len && pkt_info->mic) {
                rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD((unsigned long)bus_to_virt(cur_txbd->TXBD_ELE[3].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
                    pkt_info->mic_icv_len,PCI_DMA_TODEVICE);
            }
        }
#endif //CONFIG_ENABLE_CCI400
#else

    _dma_cache_wback((unsigned long)((PVOID)(cur_txbd)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(TX_BUFFER_DESCRIPTOR));

    _dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[0].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
        TXDESCSize);


#if 0//CFG_HAL_TX_AMSDU //no use in this case
    if ( IS_SUPPORT_TX_AMSDU(Adapter) ) {
        if (pdesc_data->aggreEn == FG_AGGRE_MSDU_LAST) {
            hdrLen = GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword0, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        }
    }
#endif // CFG_HAL_TX_AMSDU

    _dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
        (u4Byte)pkt_info->hdr_len);

#if CFG_HAL_TX_AMSDU
    if ( IS_SUPPORT_TX_AMSDU(Adapter) && pkt_info->amsdu_flag) {
    	_dma_cache_wback((unsigned long)((PVOID)(cur_txbd_amsdu)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(TX_BUFFER_DESCRIPTOR_AMSDU));
    }//else
#endif // CFG_HAL_TX_AMSDU
    {
#if 0//CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
        if (pdesc_data->smhEn == TRUE) {
			if (hdrLen) {
		    	_dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		        	(u4Byte)hdrLen);
			}
            // Do nothing here ...
        } else
#endif
        {
			if (pkt_info->hdr_len) {
			    _dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		   		    (u4Byte)pkt_info->hdr_len);
			}

			if (pkt_info->skbbuf_len) {
			    _dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[2].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		   		    (u4Byte)pkt_info->skbbuf_len);
			} 

			if (pkt_info->icv) {
				_dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[3].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		   		    pkt_info->mic_icv_len);
			} else if (pkt_info->mic) {
				_dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[3].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		   		    pkt_info->mic_icv_len);
			}

        }
    }
#endif
// TODO:  write back icv/mic on 8198C

// TODO: consider both enable TX_AMSDU and HW_TX_SHORTCUT...

#else // !TRXBD_CACHABLE_REGION
#if defined(CONFIG_NET_PCI) && defined(NOT_RTK_BSP)
        // hdr/payload/icv/mic has sync via pci_map_single in get_physical_addr
     if (!HAL_IS_PCIBIOS_TYPE(Adapter))
#endif
     {
        if (pkt_info->hdr_len) {
            HAL_CACHE_SYNC_WBACK(Adapter, 
                GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL, 
                pkt_info->hdr_len, 
                HAL_PCI_DMA_TODEVICE);
        }
        
        if (pkt_info->skbbuf_len) {
            HAL_CACHE_SYNC_WBACK(Adapter, 
                GET_DESC_FIELD(cur_txbd->TXBD_ELE[2].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL, 
                pkt_info->skbbuf_len, 
                HAL_PCI_DMA_TODEVICE);
        }

        if (pkt_info->icv != NULL) {
            HAL_CACHE_SYNC_WBACK(Adapter, 
                GET_DESC_FIELD(cur_txbd->TXBD_ELE[3].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL, 
                pkt_info->mic_icv_len, 
                HAL_PCI_DMA_TODEVICE);
        } else if (pkt_info->mic != NULL) {
            HAL_CACHE_SYNC_WBACK(Adapter, 
                GET_DESC_FIELD(cur_txbd->TXBD_ELE[3].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                pkt_info->mic_icv_len, 
                HAL_PCI_DMA_TODEVICE);
        }
    }
#ifdef CONFIG_NET_PCI
    if (HAL_IS_PCIBIOS_TYPE(Adapter)) {
#if CFG_HAL_TX_AMSDU
	if (pkt_info->amsdu_flag)
		HAL_CACHE_SYNC_WBACK(Adapter, _GET_HAL_DATA(Adapter)->desc_dma_buf_addr_amsdu[queueIndex-1] + cur_q->host_idx * sizeof(TX_BUFFER_DESCRIPTOR_AMSDU), sizeof(TX_BUFFER_DESCRIPTOR_AMSDU), HAL_PCI_DMA_TODEVICE);
#endif
        HAL_CACHE_SYNC_WBACK(Adapter, _GET_HAL_DATA(Adapter)->txDesc_dma_ring_addr[queueIndex] + cur_q->host_idx * sizeof(TX_DESC_88XX), sizeof(TX_DESC_88XX), HAL_PCI_DMA_TODEVICE);
        HAL_CACHE_SYNC_WBACK(Adapter, _GET_HAL_DATA(Adapter)->txBD_dma_ring_addr[queueIndex] + cur_q->host_idx * sizeof(TX_BUFFER_DESCRIPTOR), sizeof(TX_BUFFER_DESCRIPTOR), HAL_PCI_DMA_TODEVICE);
    }
#endif
#endif // TRXBD_CACHABLE_REGION

    UpdateSWTXBDHostIdx88XX(Adapter, cur_q);

#if 1 //eric-8822 CFG_HAL_DBG

    RT_TRACE_F(COMP_SEND, DBG_TRACE, ("\nq_idx: %d, txbd[%d], \n", queueIndex, cur_q->host_idx));

    //TXDESC
    RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%08lx, Dword1: 0x%08lx\n", 0, GET_DESC(cur_txbd->TXBD_ELE[0].Dword0), GET_DESC(cur_txbd->TXBD_ELE[0].Dword1)));
    //PTX_DESC_88XX   ptx_desc = ((PTX_DESC_88XX)(cur_q->ptx_desc_head)) + cur_q->host_idx;
    RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "TXDESC:\n", ptx_desc, TXDESCSize);

    //Header
    RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%08lx, Dword1: 0x%08lx\n", 1, GET_DESC(cur_txbd->TXBD_ELE[1].Dword0), GET_DESC(cur_txbd->TXBD_ELE[1].Dword1)));
    
    RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "Header:\n", pwlhdr,pkt_info->hdr_len );
    

#if 1
    //Payload
    RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%08lx, Dword1: 0x%08lx\n", 2, GET_DESC(cur_txbd->TXBD_ELE[2].Dword0), GET_DESC(cur_txbd->TXBD_ELE[2].Dword1)));
    RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "Payload:\n",(PVOID)((struct sk_buff *)pkt_info->pSkb)->data, pkt_info->skbbuf_len);    

    //MIC or ICV
    //RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%08lx, Dword1: 0x%08lx\n", 3, GET_DESC(cur_txbd->TXBD_ELE[3].Dword0), GET_DESC(cur_txbd->TXBD_ELE[3].Dword1)));
    //RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "Icv:\n", pdesc_data->pIcv, pdesc_data->icv);
#endif
#endif  //CFG_HAL_DBG

}
#endif //IS_RTL88XX_MAC_V3
#if IS_RTL88XX_MAC_V4
VOID
ReprepareFillTxDescSetTxBD_V1(
    IN      HAL_PADAPTER    Adapter,
    IN      struct stat_info *pstat,
    IN      u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN      struct reprepare_info* pkt_info
    //IN      struct apsd_pkt_queue* AC_dz_queue,
    //IN      int             pkt_position
)
{
    PHCI_TX_DMA_MANAGER_88XX        ptx_dma;
    PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q;
    PTX_BUFFER_DESCRIPTOR           cur_txbd;
    u4Byte                          TXDESCSize;
    unsigned long                   dma_addr;
    u4Byte                          TotalLen    = 0;
    PTX_DESC_88XX                   ptx_desc;
    struct tx_desc_info	*pswdescinfo, *pdescinfo;
    u2Byte              *tx_head;
    unsigned char		*pwlhdr;
#ifdef WLAN_HAL_TX_AMSDU    
    void                *buf_amsdu[WLAN_HAL_TX_AMSDU_MAX_NUM];
#endif
#if CFG_HAL_TX_AMSDU
    PHCI_TX_AMSDU_DMA_MANAGER_88XX  ptx_dma_amsdu;
    PTX_BUFFER_DESCRIPTOR_AMSDU     cur_txbd_amsdu;
#endif
#ifndef SMP_SYNC
    //unsigned long flags;
#endif
    u1Byte*             IE0_head;
    u1Byte*             IE1_head;
    u1Byte*             IE2_head;
    u1Byte*             IE3_head;
    u1Byte*             IE4_head;
    u1Byte  IE_cnt = 2;//default need IE0,IE1    
    u1Byte  need_IE2 = 0;    
    u1Byte  need_IE3 = 0;    
    u1Byte  need_IE4 = 0;

    struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)Adapter;

    ptx_dma     = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);
    cur_q       = &(ptx_dma->tx_queue[queueIndex]);
    cur_txbd    = cur_q->pTXBD_head + cur_q->host_idx;
    ptx_desc    = ((PTX_DESC_88XX)(cur_q->ptx_desc_head)) + cur_q->host_idx;

    IE0_head = ((u1Byte*)ptx_desc)+SIZE_TXDESC_BODY;    
    IE1_head = IE0_head+SIZE_TXDESC_IE;    
    TXDESCSize = SIZE_TXDESC_BODY+SIZE_TXDESC_IE*2;//IE_body+IE0+IE1    

    if(pkt_info->IE_bitmap & BIT0)
        need_IE2=1;
    if(pkt_info->IE_bitmap & BIT1)
        need_IE3=1;
    if(pkt_info->IE_bitmap & BIT2)
        need_IE4=1;

    if (need_IE2){        
        IE2_head = IE1_head+SIZE_TXDESC_IE;        
        IE_cnt++;        
        TXDESCSize+=SIZE_TXDESC_IE;    
    }    
    else        
        IE2_head = IE1_head; //then, should not fill IE2    

    if(need_IE3){        
        IE3_head = IE2_head+SIZE_TXDESC_IE;        
        IE_cnt++;        
        TXDESCSize+=SIZE_TXDESC_IE;    
    }    
    else        
        IE3_head = IE2_head; //then, should not fill IE3   
        
    if(need_IE4){        
        IE4_head = IE3_head+SIZE_TXDESC_IE;        
        IE_cnt++;        
        TXDESCSize+=SIZE_TXDESC_IE;    
    }    
    else        
        IE4_head = IE3_head; //then, should not fill IE4


//3 SET TXDESC
    memcpy((PVOID)ptx_desc, (PVOID)pkt_info->ptxdesc, TXDESCSize);
    tx_head     = &(cur_q->host_idx);
    pswdescinfo = get_txdesc_info(Adapter,Adapter->pshare->pdesc_info, queueIndex);
    pdescinfo = pswdescinfo + *tx_head;
    
    if(pdescinfo->ptxdesc!=NULL)
        kfree(pdescinfo->ptxdesc);

    pdescinfo->ptxdesc = pkt_info->ptxdesc;
    pdescinfo->IE_bitmap = pkt_info->IE_bitmap;
    
    if(pkt_info->llchdr){
        pdescinfo->type = _PRE_ALLOCLLCHDR_;
        pwlhdr = pkt_info->llchdr;
        //printk("[%s]pkt type = _PRE_ALLOCLLCHDR_\n",__FUNCTION__);
    }
    else if(pkt_info->hdr){
        pdescinfo->type = _PRE_ALLOCHDR_;
        pwlhdr = pkt_info->hdr;
        //printk("[%s]pkt type = _PRE_ALLOCHDR_\n",__FUNCTION__);
    }
#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV    
    else if(pkt_info->isWiFiHdr == FALSE){
        //no wlan header
    }
#endif    
    else    
        printk("[%s][%s]wrong header type!!\n",__FUNCTION__,__LINE__);//debug

#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
	if (pkt_info->isWiFiHdr == FALSE) {
		pdescinfo->pframe = NULL;
	} else
#endif // WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
	{
	    pdescinfo->pframe = pwlhdr;
	}

#if defined(WIFI_WMM) && defined(WMM_APSD)
	pdescinfo->priv = Adapter;
#ifndef TXDESC_INFO
	pdescinfo->pstat = pstat;
#endif		
#endif

    if(pkt_info->mic_icv_len && pkt_info->icv){
        pdescinfo->buf_type[1]   = _PRE_ALLOCICVHDR_;
        pdescinfo->buf_pframe[1] = pkt_info->icv;
        pdescinfo->buf_len[1] = pkt_info->mic_icv_len;
    }else if(pkt_info->mic_icv_len && pkt_info->mic){
        pdescinfo->buf_type[1]   = _PRE_ALLOCMICHDR_;
        pdescinfo->buf_pframe[1] = pkt_info->mic;
        pdescinfo->buf_len[1] = pkt_info->mic_icv_len;
    }
    
    pdescinfo->swps_pstat = pstat;

    if(pstat->EnSWPS){
                
        int pkt_Qos = queueIndex-1;
        
        if(pkt_Qos < 0)
        {
            pdescinfo->SWPS_pkt_Qos = pkt_Qos;
        }
        else
        {
            IE2_SET_TX_DESC_SW_DEFINE(IE2_head, pstat->SWPS_seq[pkt_Qos]); 
    
            pdescinfo->SWPS_sequence = pstat->SWPS_seq[pkt_Qos];
            //pdescinfo->SWPS_pktQ = queueIndex;
            pdescinfo->SWPS_pkt_Qos = pkt_Qos;
            

        	//SAVE_INT_AND_CLI(flags); //may not need
            pstat->SWPS_seq_head[pkt_Qos] = pdescinfo->SWPS_sequence;
   
            if(pstat->SWPS_seq[pkt_Qos] == 0xFFF)
                pstat->SWPS_seq[pkt_Qos] = 1;
            else        
                pstat->SWPS_seq[pkt_Qos]++;
            //RESTORE_INT(flags);
            //printk("assign new swps seq = %x\n",pdescinfo->SWPS_sequence);
        }
    }
    else{
        IE2_SET_TX_DESC_SW_DEFINE(IE2_head,0); 
        pdescinfo->SWPS_sequence = 0;
        //pdescinfo->SWPS_pktQ = 0; 
        pdescinfo->SWPS_pkt_Qos = 0; // no use, just to prevent get no qos situation.
    }
        
    pdescinfo->Drop_ID = pstat->Drop_ID;
    IE2_SET_TX_DESC_DROP_ID(IE2_head,pstat->Drop_ID);

    
    if(pkt_info->amsdu_flag){
#ifdef WLAN_HAL_TX_AMSDU           
        *(&buf_amsdu[0]) = (void*)pkt_info->pSkb; //need to check if this equotion is ok
        int amsdu_i = 0;
        for(amsdu_i=0;amsdu_i<pkt_info->amsdu_num;amsdu_i++){
            
            pdescinfo->buf_pframe_amsdu[amsdu_i] = buf_amsdu[amsdu_i];
            pdescinfo->buf_type_amsdu[amsdu_i] = _SKB_FRAME_TYPE_;
            pdescinfo->amsdubuf_len[amsdu_i] = pkt_info->amsdubuf_len[amsdu_i];
            
        }
        pdescinfo->amsdu_num = pkt_info->amsdu_num;
        pdescinfo->buf_pframe[0] = NULL;
        pdescinfo->buf_type[0] = _RESERVED_FRAME_TYPE_;
        pdescinfo->buf_len[0] = 0;
#endif                
    }else{
        pdescinfo->buf_pframe[0] = pkt_info->pSkb;
        pdescinfo->buf_type[0] = _SKB_FRAME_TYPE_;
        pdescinfo->buf_len[0]  = pkt_info->skbbuf_len; 
#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
        if (pkt_info->isWiFiHdr == FALSE){
            pdescinfo->type          = _SKB_FRAME_TYPE_; // 802.3 header + payload
            pdescinfo->pframe        = pkt_info->pSkb; // skb
           
            pdescinfo->buf_type[0]   = 0;
            pdescinfo->buf_pframe[0] = 0; // no packet payload
        }
#endif
    }
    

    //pdescinfo->buf_paddr[cnt] = ??
    //pdescinfo->paddr = ??
    //pdescinfo->len
    
    //2 need add AMSDU, shortcut HDR/TXDESC
    

//3 FILL TXBD


#if CFG_HAL_TX_AMSDU
    if ( IS_SUPPORT_TX_AMSDU(Adapter) && pkt_info->amsdu_flag ) {
        ptx_dma_amsdu  = (PHCI_TX_AMSDU_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMAAMSDU88XX);
        cur_txbd_amsdu = ptx_dma_amsdu->tx_amsdu_queue[queueIndex-1].pTXBD_head_amsdu + cur_q->host_idx;
    }

    //if (pdesc_data->aggreEn <= FG_AGGRE_MSDU_FIRST)
#endif
    {
        platform_zero_memory(&(cur_txbd->TXBD_ELE[1]), sizeof(TXBD_ELEMENT)*(TXBD_ELE_NUM-1));
    }

#if CFG_HAL_TX_AMSDU
    //if (pdesc_data->aggreEn <= FG_AGGRE_MSDU_FIRST)
#endif
    {
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[1].Dword0,
                            pkt_info->hdr_len, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
#ifdef WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV
        if (pkt_info->isWiFiHdr == FALSE){
            dma_addr = HAL_VIRT_TO_BUS1(Adapter, (PVOID)((struct sk_buff *)pkt_info->pSkb)->data, pkt_info->hdr_len, HAL_PCI_DMA_TODEVICE);
            SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[1].Dword1,
                        dma_addr + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                        TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
        }else
#endif        
        {
        
            dma_addr = HAL_VIRT_TO_BUS1(Adapter, (PVOID)pwlhdr, pkt_info->hdr_len, HAL_PCI_DMA_TODEVICE);
            SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[1].Dword1,
                        dma_addr + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                        TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
        }
    }

#if (TXBD_ELE_NUM >= 4)

#if CFG_HAL_TX_AMSDU
    if ( IS_SUPPORT_TX_AMSDU(Adapter) && pkt_info->amsdu_flag) {
        // postpone fill this field until the last payload (i.e., FG_AGGRE_MSDU_LAST), because we don't know the exact numbers of the MSDU in this AMSDU
        //SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword0, sizeof(TX_BUFFER_DESCRIPTOR_AMSDU), TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword0,
                    1, TXBD_DW0_EXTENDTXBUF_MSK, TXBD_DW0_EXTENDTXBUF_SH);

        // (queueIndex-1) for mapping HCI_TX_DMA_QUEUE_88XX to HCI_TX_AMSDU_DMA_QUEUE_88XX
        ptx_dma_amsdu->tx_amsdu_queue[queueIndex-1].cur_txbd_element = pkt_info->amsdu_num;

        // fill the exact MSDU numbers into TXBD
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword0, 
            ptx_dma_amsdu->tx_amsdu_queue[queueIndex-1].cur_txbd_element * sizeof(TXBD_ELEMENT), 
            TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);

        
        platform_zero_memory(cur_txbd_amsdu, sizeof(TX_BUFFER_DESCRIPTOR_AMSDU));

        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword1,
                HAL_VIRT_TO_BUS1(Adapter, (PVOID)cur_txbd_amsdu, sizeof(TX_BUFFER_DESCRIPTOR_AMSDU), HAL_PCI_DMA_TODEVICE) + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
    }
#endif // CFG_HAL_TX_AMSDU


#if CFG_HAL_TX_AMSDU
    if ( IS_SUPPORT_TX_AMSDU(Adapter) && pkt_info->amsdu_flag) {
        int amsdu_i=0;
        for(amsdu_i = 0;amsdu_i< pkt_info->amsdu_num;amsdu_i++){
        SET_DESC_FIELD_NO_CLR(cur_txbd_amsdu->TXBD_ELE[amsdu_i].Dword0,
                    pkt_info->amsdubuf_len[amsdu_i], TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        
        SET_DESC_FIELD_NO_CLR(cur_txbd_amsdu->TXBD_ELE[amsdu_i].Dword1,
                HAL_VIRT_TO_BUS1(Adapter, (PVOID)((struct sk_buff *)buf_amsdu[amsdu_i])->data, pkt_info->amsdubuf_len[amsdu_i], HAL_PCI_DMA_TODEVICE) + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);

		
#if defined(CONFIG_NET_PCI) && defined(NOT_RTK_BSP)
		HAL_CACHE_SYNC_WBACK(Adapter, 
		GET_DESC_FIELD(cur_txbd_amsdu->TXBD_ELE[amsdu_i].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL, 
		GET_DESC_FIELD(cur_txbd_amsdu->TXBD_ELE[amsdu_i].Dword0, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH), 
		HAL_PCI_DMA_TODEVICE);
#else
	if(pkt_info->amsdubuf_len[amsdu_i])
#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
        // With CCI400 ,Do nothing for cache coherent code
#else
        rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD(bus_to_virt(cur_txbd_amsdu->TXBD_ELE[amsdu_i].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
            (u4Byte)pkt_info->amsdubuf_len[amsdu_i],PCI_DMA_TODEVICE);
#endif //CONFIG_ENABLE_CCI400
#else
		_dma_cache_wback(((GET_DESC_FIELD(cur_txbd_amsdu->TXBD_ELE[amsdu_i].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
            pkt_info->amsdubuf_len[amsdu_i]);
#endif
#endif
		
        }
        //if (pdesc_data->aggreEn != FG_AGGRE_MSDU_LAST) {
            //for FG_AGGRE_MSDU_FIRST or FG_AGGRE_MSDU_MIDDLE
            //return;
        //}
    } else
#endif
    {
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword0,
                    pkt_info->skbbuf_len, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);

        if (pkt_info->skbbuf_len) {
            dma_addr = HAL_VIRT_TO_BUS1(Adapter, (PVOID)((struct sk_buff *)pkt_info->pSkb)->data, pkt_info->skbbuf_len, HAL_PCI_DMA_TODEVICE);
            SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword1,
                   dma_addr + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                    TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
        }
    }
    // for sw encryption: 1) WEP's icv and TKIP's icv, 2) CCMP's mic, 3) no security
    if (pkt_info->icv != NULL) {
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[3].Dword0,
                    pkt_info->mic_icv_len,
                    TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        
        dma_addr = HAL_VIRT_TO_BUS1(Adapter, (PVOID)pkt_info->icv, pkt_info->mic_icv_len, HAL_PCI_DMA_TODEVICE);
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[3].Dword1,
                    dma_addr + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                    TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);

    } else if (pkt_info->mic != NULL) {
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[3].Dword0,
                    pkt_info->mic_icv_len,
                    TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        
        dma_addr = HAL_VIRT_TO_BUS1(Adapter, (PVOID)pkt_info->mic, pkt_info->mic_icv_len, HAL_PCI_DMA_TODEVICE);
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[3].Dword1,
                    dma_addr + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                    TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);

    } else {
        cur_txbd->TXBD_ELE[3].Dword0 = SET_DESC(0);

    }
#else
    #error "Error, TXBD_ELE_NUM<4 is invalid Setting unless we modify overall architecture"
#endif  //   (TXBD_ELE_NUM >= 4) 


           
            SET_DESC_FIELD_CLR(cur_txbd->TXBD_ELE[0].Dword0, \
                        TXDESCSize, \
                        TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
            printk("reset txdesc len, cur_txbd->TXBD_ELE[0].Dword0=0x%x\n",cur_txbd->TXBD_ELE[0].Dword0);
    


    //3 Final one HW IO of Tx Pkt
    TxPktFinalIO88XX(Adapter, cur_txbd, TxPktFinalIO88XX_WRITE, pkt_info->PSB_len);

#ifdef TRXBD_CACHABLE_REGION
#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
        // With CCI400 ,Do nothing for cache coherent code
#else
        rtl_cache_sync_wback(Adapter,(unsigned long)((PVOID)(cur_txbd)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(TX_BUFFER_DESCRIPTOR),PCI_DMA_TODEVICE);

        rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD((unsigned long)bus_to_virt(cur_txbd->TXBD_ELE[0].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
            TXDESCSize,PCI_DMA_TODEVICE);
        
#if 0//CFG_HAL_TX_AMSDU //no use in this case
            if ( IS_SUPPORT_TX_AMSDU(Adapter) ) {
                if (pdesc_data->aggreEn == FG_AGGRE_MSDU_LAST) {
                    hdrLen = GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword0, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
                }
            }
#endif // CFG_HAL_TX_AMSDU


        rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD((unsigned long)bus_to_virt(cur_txbd->TXBD_ELE[1].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
            (u4Byte)pkt_info->hdr_len,PCI_DMA_TODEVICE);

#if CFG_HAL_TX_AMSDU
        if ( IS_SUPPORT_TX_AMSDU(Adapter) && pkt_info->amsdu_flag) {
            rtl_cache_sync_wback(Adapter,(unsigned long)((PVOID)(cur_txbd_amsdu)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(TX_BUFFER_DESCRIPTOR_AMSDU),PCI_DMA_TODEVICE);
        }else
#endif // CFG_HAL_TX_AMSDU
        {
            if (pkt_info->hdr_len) {
                rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD(bus_to_virt(cur_txbd->TXBD_ELE[1].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
                    (u4Byte)pkt_info->hdr_len,PCI_DMA_TODEVICE);
            }
        
            if (pkt_info->skbbuf_len) {
                rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD(bus_to_virt(cur_txbd->TXBD_ELE[2].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
                    (u4Byte)pkt_info->skbbuf_len,PCI_DMA_TODEVICE);
            } 
            if (pkt_info->mic_icv_len && pkt_info->icv) {
                rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD(bus_to_virt(cur_txbd->TXBD_ELE[3].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
                    pkt_info->mic_icv_len,PCI_DMA_TODEVICE);
            } else if (pkt_info->mic_icv_len && pkt_info->mic) {
                rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD(bus_to_virt(cur_txbd->TXBD_ELE[3].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
                    pkt_info->mic_icv_len,PCI_DMA_TODEVICE);
            }
        }
#endif //CONFIG_ENABLE_CCI400
#else

    _dma_cache_wback((unsigned long)((PVOID)(cur_txbd)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(TX_BUFFER_DESCRIPTOR));

    _dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[0].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
        TXDESCSize);


#if 0//CFG_HAL_TX_AMSDU //no use in this case
    if ( IS_SUPPORT_TX_AMSDU(Adapter) ) {
        if (pdesc_data->aggreEn == FG_AGGRE_MSDU_LAST) {
            hdrLen = GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword0, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        }
    }
#endif // CFG_HAL_TX_AMSDU

    _dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
        (u4Byte)pkt_info->hdr_len);

#if CFG_HAL_TX_AMSDU
    if ( IS_SUPPORT_TX_AMSDU(Adapter) && pkt_info->amsdu_flag) {
    	_dma_cache_wback((unsigned long)((PVOID)(cur_txbd_amsdu)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(TX_BUFFER_DESCRIPTOR_AMSDU));
    }//else
#endif // CFG_HAL_TX_AMSDU
    {
#if 0//CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
        if (pdesc_data->smhEn == TRUE) {
			if (hdrLen) {
		    	_dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		        	(u4Byte)hdrLen);
			}
            // Do nothing here ...
        } else
#endif
        {
			if (pkt_info->hdr_len) {
			    _dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		   		    (u4Byte)pkt_info->hdr_len);
			}

			if (pkt_info->skbbuf_len) {
			    _dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[2].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		   		    (u4Byte)pkt_info->skbbuf_len);
			} 

			if (pkt_info->icv) {
				_dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[3].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		   		    pkt_info->mic_icv_len);
			} else if (pkt_info->mic) {
				_dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[3].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		   		    pkt_info->mic_icv_len);
			}

        }
    }
#endif
// TODO:  write back icv/mic on 8198C

// TODO: consider both enable TX_AMSDU and HW_TX_SHORTCUT...

#else // !TRXBD_CACHABLE_REGION
#if defined(CONFIG_NET_PCI) && defined(NOT_RTK_BSP)
        // hdr/payload/icv/mic has sync via pci_map_single in get_physical_addr
     if (!HAL_IS_PCIBIOS_TYPE(Adapter))
#endif
     {
        if (pkt_info->hdr_len) {
            HAL_CACHE_SYNC_WBACK(Adapter, 
                GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL, 
                pkt_info->hdr_len, 
                HAL_PCI_DMA_TODEVICE);
        }
        
        if (pkt_info->skbbuf_len) {
            HAL_CACHE_SYNC_WBACK(Adapter, 
                GET_DESC_FIELD(cur_txbd->TXBD_ELE[2].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL, 
                pkt_info->skbbuf_len, 
                HAL_PCI_DMA_TODEVICE);
        }

        if (pkt_info->icv != NULL) {
            HAL_CACHE_SYNC_WBACK(Adapter, 
                GET_DESC_FIELD(cur_txbd->TXBD_ELE[3].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL, 
                pkt_info->mic_icv_len, 
                HAL_PCI_DMA_TODEVICE);
        } else if (pkt_info->mic != NULL) {
            HAL_CACHE_SYNC_WBACK(Adapter, 
                GET_DESC_FIELD(cur_txbd->TXBD_ELE[3].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                pkt_info->mic_icv_len, 
                HAL_PCI_DMA_TODEVICE);
        }
    }
#ifdef CONFIG_NET_PCI
    if (HAL_IS_PCIBIOS_TYPE(Adapter)) {
#if CFG_HAL_TX_AMSDU
	if (pkt_info->amsdu_flag)
		HAL_CACHE_SYNC_WBACK(Adapter, _GET_HAL_DATA(Adapter)->desc_dma_buf_addr_amsdu[queueIndex-1] + cur_q->host_idx * sizeof(TX_BUFFER_DESCRIPTOR_AMSDU), sizeof(TX_BUFFER_DESCRIPTOR_AMSDU), HAL_PCI_DMA_TODEVICE);
#endif
        HAL_CACHE_SYNC_WBACK(Adapter, _GET_HAL_DATA(Adapter)->txDesc_dma_ring_addr[queueIndex] + cur_q->host_idx * sizeof(TX_DESC_88XX), sizeof(TX_DESC_88XX), HAL_PCI_DMA_TODEVICE);
        HAL_CACHE_SYNC_WBACK(Adapter, _GET_HAL_DATA(Adapter)->txBD_dma_ring_addr[queueIndex] + cur_q->host_idx * sizeof(TX_BUFFER_DESCRIPTOR), sizeof(TX_BUFFER_DESCRIPTOR), HAL_PCI_DMA_TODEVICE);
    }
#endif
#endif // TRXBD_CACHABLE_REGION

    UpdateSWTXBDHostIdx88XX(Adapter, cur_q);

#if 1 //eric-8822 CFG_HAL_DBG

    RT_TRACE_F(COMP_SEND, DBG_TRACE, ("\nq_idx: %d, txbd[%d], \n", queueIndex, cur_q->host_idx));

    //TXDESC
    RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%08lx, Dword1: 0x%08lx\n", 0, GET_DESC(cur_txbd->TXBD_ELE[0].Dword0), GET_DESC(cur_txbd->TXBD_ELE[0].Dword1)));
    //PTX_DESC_88XX   ptx_desc = ((PTX_DESC_88XX)(cur_q->ptx_desc_head)) + cur_q->host_idx;
    RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "TXDESC:\n", ptx_desc, TXDESCSize);

    //Header
    RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%08lx, Dword1: 0x%08lx\n", 1, GET_DESC(cur_txbd->TXBD_ELE[1].Dword0), GET_DESC(cur_txbd->TXBD_ELE[1].Dword1)));
    
    RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "Header:\n", pwlhdr,pkt_info->hdr_len );
    

#if 1
    //Payload
    RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%08lx, Dword1: 0x%08lx\n", 2, GET_DESC(cur_txbd->TXBD_ELE[2].Dword0), GET_DESC(cur_txbd->TXBD_ELE[2].Dword1)));
    RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "Payload:\n",(PVOID)((struct sk_buff *)pkt_info->pSkb)->data, pkt_info->skbbuf_len);    

    //MIC or ICV
    //RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%08lx, Dword1: 0x%08lx\n", 3, GET_DESC(cur_txbd->TXBD_ELE[3].Dword0), GET_DESC(cur_txbd->TXBD_ELE[3].Dword1)));
    //RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "Icv:\n", pdesc_data->pIcv, pdesc_data->icv);
#endif
#endif  //CFG_HAL_DBG

}
#endif //IS_RTL88XX_MAC_V4
#endif //defined(AP_SWPS_OFFLOAD)

//Note: This function can't be used by Beacon
VOID
SetTxBufferDesc88XX (
    IN      HAL_PADAPTER    Adapter,
    IN      u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN      PVOID           pDescData,
    IN      u1Byte          setTxbdSource
)
{
    PTX_DESC_DATA_88XX              pdesc_data = (PTX_DESC_DATA_88XX)pDescData;
    PHCI_TX_DMA_MANAGER_88XX        ptx_dma;
    PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q;
    PTX_BUFFER_DESCRIPTOR           cur_txbd;
    u1Byte                          i;
    u4Byte                          TotalLen    = 0;
    u4Byte                          PSBLen;    
    // if each queue num is different, need modify this number....
    u4Byte                          TXBDSegNum  = TXBD_ELE_NUM; 
    u4Byte                          hdrLen      = pdesc_data->hdrLen + pdesc_data->llcLen;
    u4Byte                          payloadLen  = pdesc_data->frLen;
    unsigned long dma_addr;
    u4Byte                          TXDESCSize;
    u2Byte                          *tx_head;
    struct tx_desc_info	*pswdescinfo, *pdescinfo;
    
#if CFG_HAL_TX_AMSDU
    PHCI_TX_AMSDU_DMA_MANAGER_88XX  ptx_dma_amsdu;
    PTX_BUFFER_DESCRIPTOR_AMSDU     cur_txbd_amsdu;
#endif

#ifdef MERGE_TXDESC_HEADER_PAYLOAD
    PTX_DESC_88XX                   cur_txdesc;
#endif




    ptx_dma     = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);
    cur_q       = &(ptx_dma->tx_queue[queueIndex]);
    cur_txbd    = cur_q->pTXBD_head + cur_q->host_idx;
    tx_head     = &(cur_q->host_idx);

#ifdef AP_SWPS_OFFLOAD 
    pswdescinfo = get_txdesc_info(Adapter,Adapter->pshare->pdesc_info, queueIndex);
    pdescinfo = pswdescinfo + *tx_head;
#endif //#ifdef AP_SWPS_OFFLOAD

#if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM)
    if ( IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8192FE(Adapter)  || IS_HARDWARE_TYPE_8814AE(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter)|| IS_HARDWARE_TYPE_8198F(Adapter) ) {
        TXDESCSize = SIZE_TXDESC_88XX;
    }
#endif  //#if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM)

#if (IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE|| IS_EXIST_RTL8197GEM)
    if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter))  {
        TXDESCSize = SIZE_TXDESC_88XX_V1;
    }
#endif // (IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8197GEM)

#if (IS_EXIST_RTL8814BE)
    if ( IS_HARDWARE_TYPE_8814B(Adapter))  {
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
		if (pdesc_data->ph2d_txdma_info) {
			PH2D_TXDMA_INFO_DATA pinfo = (PH2D_TXDMA_INFO_DATA)pdesc_data->ph2d_txdma_info;
#ifdef TX_SHORTCUT			
			if (setTxbdSource == SET_TXBD_SOURCE_SHORTCUT) {
				unsigned int info = le32_to_cpu(*(unsigned int *)pinfo);
				TXDESCSize = (info & 0xff0000) >> 16; //pinfo->dw0_offset;
				//TXDESCSize = 32;
				//printk("TXDescsize = %d\n", TXDESCSize);
			}
			else
#endif
			TXDESCSize = pinfo->dw0_offset;
		} else
#endif
        if(setTxbdSource == SET_TXBD_SOURCE_NORMAL)
            TXDESCSize = SIZE_TXDESC_BODY+SIZE_TXDESC_IE*5;//IE_body+IE0-IE4
        else if(setTxbdSource == SET_TXBD_SOURCE_SHORTCUT)
            TXDESCSize = SIZE_TXDESC_BODY+SIZE_TXDESC_IE*2;//IE_body+IE0+IE1
#ifdef TX_SHORTCUT
        else if(setTxbdSource == SET_TXBD_SOURCE_OFLD_SHORTCUT)    
			TXDESCSize = SIZE_TXDESC_BODY+SIZE_TXDESC_IE;//IE_body+IE0
#endif
    }
#endif //IS_EXIST_RTL8814BE

#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
    // TODO: check it....right or wrong..
	if (pdesc_data->smhEn == FALSE)
#endif
    if (pdesc_data->secType != _NO_PRIVACY_) {
		// TODO: wapi...
        // WEP:  1) icv for hw encrypt
        // TKIP: 1) iv contains eiv,   2) payload contains mic, 3) icv for hw encrypt
        // CCMP: 1) iv is CCMP header, 2) mic for hw encrypt
        hdrLen += pdesc_data->iv;
    }

#ifdef MERGE_TXDESC_HEADER_PAYLOAD

//if (queueIndex == HCI_TX_DMA_QUEUE_BE) {
    //memcpy((skb->data - (txcfg->hdr_len + txcfg->llc + txcfg->iv)), txcfg->phdr, (txcfg->hdr_len + txcfg->llc + txcfg->iv));
    //release_wlanllchdr_to_poll(Adapter, txcfg->phdr);
    //txcfg->phdr = skb->data - (txcfg->hdr_len + txcfg->llc + txcfg->iv);

    platform_zero_memory(&(cur_txbd->TXBD_ELE[1]), sizeof(TXBD_ELEMENT)*(TXBD_ELE_NUM-1));

    //cur_txdesc  = (PTX_DESC_88XX)cur_q->ptx_desc_head + cur_q->host_idx;


    HAL_memcpy((pdesc_data->pBuf - hdrLen), pdesc_data->pHdr, hdrLen);
    //release_wlanllchdr_to_poll(Adapter, pdesc_data->pHdr);
#if 1
    pdesc_data->pHdr = pdesc_data->pBuf - hdrLen;
#else
    HAL_memcpy((pdesc_data->pBuf - hdrLen - SIZE_TXDESC_88XX), cur_txdesc, SIZE_TXDESC_88XX);
    pdesc_data->pHdr = pdesc_data->pBuf - hdrLen - SIZE_TXDESC_88XX;
#endif

    TotalLen = SIZE_TXDESC_88XX + hdrLen + payloadLen;
    PSBLen   = (TotalLen%PBP_PSTX_SIZE_V1) == 0 ? (TotalLen/PBP_PSTX_SIZE_V1):((TotalLen/PBP_PSTX_SIZE_V1)+1);

    tempDW0Value = GET_DESC_FIELD(cur_txbd->TXBD_ELE[0].Dword0, 0xFFFFFFFF, 0);
    tempDW0Value = GET_DESC_FIELD(cur_txbd->TXBD_ELE[0].Dword1, 0xFFFFFFFF, 0); 

    SET_DESC_FIELD_CLR(cur_txbd->TXBD_ELE[0].Dword0, PSBLen, TXBD_DW0_PSLEN_MSK, TXBD_DW0_PSLEN_SH);

  	SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[1].Dword0, (hdrLen + payloadLen), TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
    SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[1].Dword1,
        HAL_VIRT_TO_BUS1(Adapter, (PVOID)pdesc_data->pHdr, (hdrLen + payloadLen), HAL_PCI_DMA_TODEVICE) + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
        TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);

    cur_txbd->TXBD_ELE[2].Dword0 = SET_DESC(0);
    cur_txbd->TXBD_ELE[3].Dword0 = SET_DESC(0);

    _dma_cache_wback((unsigned long)((PVOID)(cur_txbd)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), 
        sizeof(TX_BUFFER_DESCRIPTOR));

    _dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[0].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
        SIZE_TXDESC_88XX);

    _dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
        (hdrLen + payloadLen));

    UpdateSWTXBDHostIdx88XX(Adapter, cur_q);

    return;
//}
#endif

#if CFG_HAL_TX_AMSDU
    if (pdesc_data->aggreEn >= FG_AGGRE_MSDU_FIRST) {
        ptx_dma_amsdu  = (PHCI_TX_AMSDU_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMAAMSDU88XX);
        cur_txbd_amsdu = ptx_dma_amsdu->tx_amsdu_queue[queueIndex-1].pTXBD_head_amsdu + cur_q->host_idx;
    }

    if (pdesc_data->aggreEn <= FG_AGGRE_MSDU_FIRST)
#endif
    {
        platform_zero_memory(&(cur_txbd->TXBD_ELE[1]), sizeof(TXBD_ELEMENT)*(TXBD_ELE_NUM-1));
    }
    
#if CFG_HAL_TX_AMSDU
    if (pdesc_data->aggreEn <= FG_AGGRE_MSDU_FIRST)
#endif
    {

        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[1].Dword0,
                    hdrLen, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);

        dma_addr = HAL_VIRT_TO_BUS1(Adapter, (PVOID)pdesc_data->pHdr, hdrLen, HAL_PCI_DMA_TODEVICE);
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[1].Dword1,
                    dma_addr + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                    TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
#ifdef AP_SWPS_OFFLOAD
        pdescinfo->hdr_len = hdrLen;
#endif
    }

#if (TXBD_ELE_NUM >= 4)

#if CFG_HAL_TX_AMSDU
    if (pdesc_data->aggreEn == FG_AGGRE_MSDU_FIRST) {
        // postpone fill this field until the last payload (i.e., FG_AGGRE_MSDU_LAST), because we don't know the exact numbers of the MSDU in this AMSDU
        //SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword0, sizeof(TX_BUFFER_DESCRIPTOR_AMSDU), TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword0,
                    1, TXBD_DW0_EXTENDTXBUF_MSK, TXBD_DW0_EXTENDTXBUF_SH);

        // (queueIndex-1) for mapping HCI_TX_DMA_QUEUE_88XX to HCI_TX_AMSDU_DMA_QUEUE_88XX
        ptx_dma_amsdu->tx_amsdu_queue[queueIndex-1].cur_txbd_element = 0;
        platform_zero_memory(cur_txbd_amsdu, sizeof(TX_BUFFER_DESCRIPTOR_AMSDU));

#if defined(CONFIG_NET_PCI) && defined(NOT_RTK_BSP)
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword1,
                (_GET_HAL_DATA(Adapter)->desc_dma_buf_addr_amsdu[queueIndex-1] + cur_q->host_idx*sizeof(TX_BUFFER_DESCRIPTOR_AMSDU)) + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
#else
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword1,
                HAL_VIRT_TO_BUS1(Adapter, (PVOID)cur_txbd_amsdu, sizeof(TX_BUFFER_DESCRIPTOR_AMSDU), HAL_PCI_DMA_TODEVICE) + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
#endif
    }
#endif // CFG_HAL_TX_AMSDU


#if CFG_HAL_TX_AMSDU
    if (pdesc_data->aggreEn >= FG_AGGRE_MSDU_FIRST) {
    // for FG_AGGRE_MSDU_FIRST, FG_AGGRE_MSDU_MIDDLE, and FG_AGGRE_MSDU_LAST
        // current AMSDU TXBD
        SET_DESC_FIELD_NO_CLR(cur_txbd_amsdu->TXBD_ELE[ptx_dma_amsdu->tx_amsdu_queue[queueIndex-1].cur_txbd_element].Dword0,
                    payloadLen, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        
        SET_DESC_FIELD_NO_CLR(cur_txbd_amsdu->TXBD_ELE[ptx_dma_amsdu->tx_amsdu_queue[queueIndex-1].cur_txbd_element].Dword1,
                HAL_VIRT_TO_BUS1(Adapter, (PVOID)pdesc_data->pBuf, payloadLen, HAL_PCI_DMA_TODEVICE) + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);

		i=ptx_dma_amsdu->tx_amsdu_queue[queueIndex-1].cur_txbd_element;
#if defined(CONFIG_NET_PCI) && defined(NOT_RTK_BSP)
		HAL_CACHE_SYNC_WBACK(Adapter, 
		GET_DESC_FIELD(cur_txbd_amsdu->TXBD_ELE[i].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL, 
		GET_DESC_FIELD(cur_txbd_amsdu->TXBD_ELE[i].Dword0, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH), 
		HAL_PCI_DMA_TODEVICE);
#else
	if(payloadLen){
#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
        // With CCI400 ,Do nothing for cache coherent code
#else
        rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD((unsigned long)bus_to_virt(cur_txbd_amsdu->TXBD_ELE[i].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
            (u4Byte)payloadLen,PCI_DMA_TODEVICE);
#endif //CONFIG_ENABLE_CCI400
#else
		_dma_cache_wback(((GET_DESC_FIELD(cur_txbd_amsdu->TXBD_ELE[i].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
            payloadLen);
#endif
    }
#endif
#ifdef AP_SWPS_OFFLOAD		
        pdescinfo->amsdubuf_len[ptx_dma_amsdu->tx_amsdu_queue[queueIndex-1].cur_txbd_element] = payloadLen;
        pdescinfo->amsdu_num++;
#endif        
        ptx_dma_amsdu->tx_amsdu_queue[queueIndex-1].cur_txbd_element++;

        if (pdesc_data->aggreEn != FG_AGGRE_MSDU_LAST) {
            //for FG_AGGRE_MSDU_FIRST or FG_AGGRE_MSDU_MIDDLE
            return;
        }
    } else
#endif
    {
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword0,
                    payloadLen, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);

        if (payloadLen) {
            dma_addr = HAL_VIRT_TO_BUS1(Adapter, (PVOID)pdesc_data->pBuf, payloadLen, HAL_PCI_DMA_TODEVICE);
            SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword1,
                   dma_addr + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                    TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
        }
#ifdef AP_SWPS_OFFLOAD
        pdescinfo->skbbuf_len = payloadLen;
#endif
    }
    
    // for sw encryption: 1) WEP's icv and TKIP's icv, 2) CCMP's mic, 3) no security
    if (pdesc_data->pIcv != NULL) {
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[3].Dword0,
                    pdesc_data->icv,
                    TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        
        dma_addr = HAL_VIRT_TO_BUS1(Adapter, (PVOID)pdesc_data->pIcv, pdesc_data->icv, HAL_PCI_DMA_TODEVICE);
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[3].Dword1,
                    dma_addr + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                    TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
#ifdef AP_SWPS_OFFLOAD        
        pdescinfo->mic_icv_len = pdesc_data->icv;
#endif
    } else if (pdesc_data->pMic != NULL) {
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[3].Dword0,
                    pdesc_data->mic,
                    TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        
        dma_addr = HAL_VIRT_TO_BUS1(Adapter, (PVOID)pdesc_data->pMic, pdesc_data->mic, HAL_PCI_DMA_TODEVICE);
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[3].Dword1,
                    dma_addr + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                    TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
#ifdef AP_SWPS_OFFLOAD        
        pdescinfo->mic_icv_len = pdesc_data->mic;
#endif
    } else {
        cur_txbd->TXBD_ELE[3].Dword0 = SET_DESC(0);
#ifdef AP_SWPS_OFFLOAD        
        pdescinfo->mic_icv_len = 0;
#endif
    }
#else
    #error "Error, TXBD_ELE_NUM<4 is invalid Setting unless we modify overall architecture"
#endif  //   (TXBD_ELE_NUM >= 4) 

#if CFG_HAL_SUPPORT_TXDESC_IE
	if(IS_SUPPORT_TXDESC_IE(Adapter)){ 
			
			SET_DESC_FIELD_CLR(cur_txbd->TXBD_ELE[0].Dword0, \
						TXDESCSize, \
						TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
	}
#endif

#if CFG_HAL_TX_AMSDU
    if (pdesc_data->aggreEn == FG_AGGRE_MSDU_LAST) {
        // fill AMSDU packet total size into TXDESC->DW0 TX_DESC_TXPKTSIZE field
        PTX_DESC_88XX ptx_desc = ((PTX_DESC_88XX)(cur_q->ptx_desc_head)) + cur_q->host_idx;
        SET_DESC_FIELD_CLR(ptx_desc->Dword0, pdesc_data->amsduLen, TX_DW0_TXPKSIZE_MSK, TX_DW0_TXPKSIZE_SH);

#if defined(AP_SWPS_OFFLOAD)
        if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter)){
            PTX_DESC_88XX ptx_desc_info = (PTX_DESC_88XX) pdescinfo->ptxdesc;
        SET_DESC_FIELD_CLR(ptx_desc_info->Dword0,pdesc_data->amsduLen, TX_DW0_TXPKSIZE_MSK, TX_DW0_TXPKSIZE_SH);
        }
#endif

        // fill the exact MSDU numbers into TXBD
        SET_DESC_FIELD_NO_CLR(cur_txbd->TXBD_ELE[2].Dword0, 
            ptx_dma_amsdu->tx_amsdu_queue[queueIndex-1].cur_txbd_element * sizeof(TXBD_ELEMENT), 
            TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);

        // count total length for "dword0 Length0"
        TotalLen += GET_DESC_FIELD(cur_txbd->TXBD_ELE[0].Dword0, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        TotalLen += GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword0, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        
        // cur_txbd->TXBD_ELE[2].Dword1 point to the AMSDU TXBD
        for (i = 0; i < ptx_dma_amsdu->tx_amsdu_queue[queueIndex-1].cur_txbd_element; i++) {
            TotalLen += GET_DESC_FIELD(cur_txbd_amsdu->TXBD_ELE[i].Dword0, 
                        TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        }
        TotalLen += GET_DESC_FIELD(cur_txbd->TXBD_ELE[3].Dword0, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
    } else
#endif
    {
        // count total length for "dword0 Length0"
        for (i = 0; i < TXBDSegNum; i++) {
            TotalLen += GET_DESC_FIELD(cur_txbd->TXBD_ELE[i].Dword0, 
                        TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
        }
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
        if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(Adapter) && (pdesc_data->smhEn == TRUE)) {
            //TotalLen += (HAL_HW_TXSC_HDR_CONV_OFFSET - HAL_TXDESC_OFFSET_SIZE);
            TotalLen += HAL_HW_TXSC_HDR_CONV_ADD_OFFSET;
        }
#endif		
    }

#if (IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE || IS_EXIST_RTL8197FEM)
    if ( IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8192FE(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter) ) {
        PSBLen   = (TotalLen%PBP_PSTX_SIZE) == 0 ? (TotalLen/PBP_PSTX_SIZE):((TotalLen/PBP_PSTX_SIZE)+1);        
    }
#endif
#if (IS_EXIST_RTL8814AE || IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE|| IS_EXIST_RTL8198FEM || IS_EXIST_RTL8814BE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE|| IS_EXIST_RTL8197GEM) 
    if (IS_HARDWARE_TYPE_8814AE(Adapter) || IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8814B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)|| IS_HARDWARE_TYPE_8197G(Adapter)) {
        PSBLen   = (TotalLen%PBP_PSTX_SIZE_V1) == 0 ? (TotalLen/PBP_PSTX_SIZE_V1):((TotalLen/PBP_PSTX_SIZE_V1)+1);
    }
#endif
#if IS_EXIST_RTL8881AEM
    if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
        if (IS_HAL_TEST_CHIP(Adapter)) {
            PSBLen   = TotalLen;
        }
        else {
            PSBLen   = (TotalLen%PBP_PSTX_SIZE) == 0 ? (TotalLen/PBP_PSTX_SIZE):((TotalLen/PBP_PSTX_SIZE)+1);                    
        }
    }
#endif


    //3 Final one HW IO of Tx Pkt
    TxPktFinalIO88XX(Adapter, cur_txbd, TxPktFinalIO88XX_WRITE, PSBLen);
#ifdef AP_SWPS_OFFLOAD
    pdescinfo->PSB_len = PSBLen;
#endif

#if WLAN_HAL_TXDESC_CHECK_ADDR_LEN
#if IS_EXIST_RTL8881AEM
    if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
    _GET_HAL_DATA(Adapter)->cur_txbd                = cur_txbd;
#if 0
    _GET_HAL_DATA(Adapter)->cur_tx_desc_phy_addr    = cur_txbd->TXBD_ELE[0].Dword1;
    _GET_HAL_DATA(Adapter)->cur_tx_desc_len         = cur_txbd->TXBD_ELE[0].Dword0;
#endif
    _GET_HAL_DATA(Adapter)->cur_tx_psb_len          = PSBLen;
    }
#endif //IS_EXIST_RTL8881AEM
#endif // WLAN_HAL_TXDESC_CHECK_ADDR_LEN

    //4 Cache flush
#ifdef TRXBD_CACHABLE_REGION

#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
    // With CCI400 ,Do nothing for cache coherent code
#else
    rtl_cache_sync_wback(Adapter,(unsigned long)((PVOID)(cur_txbd)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), 
        sizeof(TX_BUFFER_DESCRIPTOR),PCI_DMA_TODEVICE);
    rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD((unsigned long)bus_to_virt(cur_txbd->TXBD_ELE[0].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
        TXDESCSize,PCI_DMA_TODEVICE);
#endif //CONFIG_ENABLE_CCI400
#else
    _dma_cache_wback((unsigned long)((PVOID)(cur_txbd)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(TX_BUFFER_DESCRIPTOR));

    _dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[0].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
        TXDESCSize);
 
#endif //CONFIG_RTL_8198F

#if CFG_HAL_TX_AMSDU
    if (pdesc_data->aggreEn == FG_AGGRE_MSDU_LAST) {
        hdrLen = GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword0, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
    }
#endif // CFG_HAL_TX_AMSDU


#if CFG_HAL_TX_AMSDU
    if (pdesc_data->aggreEn == FG_AGGRE_MSDU_LAST) {
#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
        // With CCI400 ,Do nothing for cache coherent code
#else
        rtl_cache_sync_wback(Adapter,(unsigned long)((PVOID)(cur_txbd_amsdu)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), 
        sizeof(TX_BUFFER_DESCRIPTOR_AMSDU),PCI_DMA_TODEVICE);
#endif //CONFIG_ENABLE_CCI400
#else
    	_dma_cache_wback((unsigned long)((PVOID)(cur_txbd_amsdu)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(TX_BUFFER_DESCRIPTOR_AMSDU));

    	if (hdrLen) {
    	    	_dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
    	    	(u4Byte)hdrLen);
    	}
#endif  
    }else
#endif // CFG_HAL_TX_AMSDU
    {
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
        if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(Adapter) && (pdesc_data->smhEn == TRUE)) {
			if (hdrLen) {
		    	_dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		        	(u4Byte)hdrLen);
			}
            // Do nothing here ...
        } else
#endif
        {
#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
        // With CCI400 ,Do nothing for cache coherent code
#else
    		if (hdrLen) {
                rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD((unsigned long)bus_to_virt(cur_txbd->TXBD_ELE[1].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		   		    (u4Byte)hdrLen,PCI_DMA_TODEVICE);
			}

			if (payloadLen) {
                rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD((unsigned long)bus_to_virt(cur_txbd->TXBD_ELE[2].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		   		    (u4Byte)payloadLen,PCI_DMA_TODEVICE);
		   		 //dma_sync_single_for_device((Adapter->dev), (GET_DESC_FIELD(cur_txbd->TXBD_ELE[2].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)), sizeof(TX_BUFFER_DESCRIPTOR), DMA_TO_DEVICE); 
			} 

			if (pdesc_data->icv) {
                rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD((unsigned long)bus_to_virt(cur_txbd->TXBD_ELE[3].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		   		    pdesc_data->icv,PCI_DMA_TODEVICE);
			} else if (pdesc_data->mic) {

                rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD((unsigned long)bus_to_virt(cur_txbd->TXBD_ELE[3].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		   		    pdesc_data->mic,PCI_DMA_TODEVICE);
			}

#endif //CONFIG_ENABLE_CCI400
#else
			if (hdrLen) {
			    _dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		   		    (u4Byte)hdrLen);
			}

			if (payloadLen) {
			    _dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[2].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		   		    (u4Byte)payloadLen);
			} 

			if (pdesc_data->icv) {
				_dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[3].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		   		    pdesc_data->icv);
			} else if (pdesc_data->mic) {
				_dma_cache_wback(((GET_DESC_FIELD(cur_txbd->TXBD_ELE[3].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
		   		    pdesc_data->mic);
			}
#endif //#ifdef CONFIG_RTL_8198F            

        }
    }

// TODO:  write back icv/mic on 8198C

// TODO: consider both enable TX_AMSDU and HW_TX_SHORTCUT...

#else // !TRXBD_CACHABLE_REGION
#if defined(CONFIG_NET_PCI) && defined(NOT_RTK_BSP)
        // hdr/payload/icv/mic has sync via pci_map_single in get_physical_addr
     if (!HAL_IS_PCIBIOS_TYPE(Adapter))
#endif
     {
#if CFG_HAL_TX_AMSDU
	if (pdesc_data->aggreEn == FG_AGGRE_MSDU_LAST) {
		hdrLen = GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword0, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
	}
#endif // CFG_HAL_TX_AMSDU
        if (hdrLen) {
            HAL_CACHE_SYNC_WBACK(Adapter, 
                GET_DESC_FIELD(cur_txbd->TXBD_ELE[1].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL, 
                hdrLen, 
                HAL_PCI_DMA_TODEVICE);
        }
        
        if (payloadLen) {
            HAL_CACHE_SYNC_WBACK(Adapter, 
                GET_DESC_FIELD(cur_txbd->TXBD_ELE[2].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL, 
                payloadLen, 
                HAL_PCI_DMA_TODEVICE);
        }

        if (pdesc_data->pIcv != NULL) {
            HAL_CACHE_SYNC_WBACK(Adapter, 
                GET_DESC_FIELD(cur_txbd->TXBD_ELE[3].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL, 
                pdesc_data->icv, 
                HAL_PCI_DMA_TODEVICE);
        } else if (pdesc_data->pMic != NULL) {
            HAL_CACHE_SYNC_WBACK(Adapter, 
                GET_DESC_FIELD(cur_txbd->TXBD_ELE[3].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                pdesc_data->mic, 
                HAL_PCI_DMA_TODEVICE);
        }
    }
#ifdef CONFIG_NET_PCI
    if (HAL_IS_PCIBIOS_TYPE(Adapter)) {
#if CFG_HAL_TX_AMSDU
	if (pdesc_data->aggreEn == FG_AGGRE_MSDU_LAST)
		HAL_CACHE_SYNC_WBACK(Adapter, _GET_HAL_DATA(Adapter)->desc_dma_buf_addr_amsdu[queueIndex-1] + cur_q->host_idx * sizeof(TX_BUFFER_DESCRIPTOR_AMSDU), sizeof(TX_BUFFER_DESCRIPTOR_AMSDU), HAL_PCI_DMA_TODEVICE);
#endif
        HAL_CACHE_SYNC_WBACK(Adapter, _GET_HAL_DATA(Adapter)->txDesc_dma_ring_addr[queueIndex] + cur_q->host_idx * sizeof(TX_DESC_88XX), sizeof(TX_DESC_88XX), HAL_PCI_DMA_TODEVICE);
        HAL_CACHE_SYNC_WBACK(Adapter, _GET_HAL_DATA(Adapter)->txBD_dma_ring_addr[queueIndex] + cur_q->host_idx * sizeof(TX_BUFFER_DESCRIPTOR), sizeof(TX_BUFFER_DESCRIPTOR), HAL_PCI_DMA_TODEVICE);
    }
#endif
#endif // TRXBD_CACHABLE_REGION

#if 0//eric-8822 CFG_HAL_DBG

    RT_TRACE_F(COMP_SEND, DBG_TRACE, ("\nq_idx: %d, txbd[%d], \n", queueIndex, cur_q->host_idx));

    //TXDESC
    RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%08lx, Dword1: 0x%08lx\n", 0, GET_DESC(cur_txbd->TXBD_ELE[0].Dword0), GET_DESC(cur_txbd->TXBD_ELE[0].Dword1)));
    PTX_DESC_88XX   ptx_desc = ((PTX_DESC_88XX)(cur_q->ptx_desc_head)) + cur_q->host_idx;
    RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "TXDESC:\n", ptx_desc, TXDESCSize);

    //Header
    RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%08lx, Dword1: 0x%08lx\n", 1, GET_DESC(cur_txbd->TXBD_ELE[1].Dword0), GET_DESC(cur_txbd->TXBD_ELE[1].Dword1)));
    if(pdesc_data->iv != 0) {
        RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "Header(+iv):\n", pdesc_data->pHdr, pdesc_data->hdrLen + pdesc_data->iv + pdesc_data->llcLen);
    } else {
        RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "Header:\n", pdesc_data->pHdr, pdesc_data->hdrLen + pdesc_data->llcLen);
    }

    //Payload
    RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%08lx, Dword1: 0x%08lx\n", 2, GET_DESC(cur_txbd->TXBD_ELE[2].Dword0), GET_DESC(cur_txbd->TXBD_ELE[2].Dword1)));
    RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "Payload:\n", pdesc_data->pBuf, pdesc_data->frLen);    

    //MIC or ICV
    RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%08lx, Dword1: 0x%08lx\n", 3, GET_DESC(cur_txbd->TXBD_ELE[3].Dword0), GET_DESC(cur_txbd->TXBD_ELE[3].Dword1)));
    RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "Icv:\n", pdesc_data->pIcv, pdesc_data->icv);

#endif  //CFG_HAL_DBG

    UpdateSWTXBDHostIdx88XX(Adapter, cur_q);

}


BOOLEAN
FillTxHwCtrl88XX(
    IN      HAL_PADAPTER    Adapter,
    IN      u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN      PVOID           pDescData
)
{
#if CFG_HAL_TX_AMSDU
    PTX_DESC_DATA_88XX  pdesc_data = (PTX_DESC_DATA_88XX)pDescData;
     
    if (pdesc_data->aggreEn <= FG_AGGRE_MSDU_FIRST)
#endif
    {
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
		if (OFFLOAD_ENABLE(Adapter) && ((PTX_DESC_DATA_88XX)pDescData)->ph2d_txdma_info)
			FillH2D_TxHwCtrl88XX(Adapter, queueIndex, pDescData);
		else
#endif
        GET_HAL_INTERFACE(Adapter)->FillTxDescHandler(Adapter, queueIndex, pDescData);
    }
    SetTxBufferDesc88XX(Adapter, queueIndex, pDescData,SET_TXBD_SOURCE_NORMAL);
    
    return _TRUE;
}

HAL_IMEM
BOOLEAN
QueryTxConditionMatch88XX(
    IN	HAL_PADAPTER        Adapter
)
{
    PHCI_TX_DMA_MANAGER_88XX    ptx_dma;
    u4Byte                      QueueIdx;   //HCI_TX_DMA_QUEUE_88XX
    u4Byte                      q_max;      //HCI_TX_DMA_QUEUE_88XX
    u32                         count = TX_CONDITION_MATCH_TXBD_CNT;


#if CFG_HAL_SUPPORT_MBSSID
    // excluding beacon queue...
#if IS_RTL88XX_MAC_V1_V2 || IS_RTL8197G_SERIES
	if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2 || IS_HARDWARE_TYPE_8197G(Adapter))     
    q_max = HCI_TX_DMA_QUEUE_HI7;
#endif   
#if  IS_RTL8198F_SERIES
	if (IS_HARDWARE_TYPE_8198F(Adapter)) {
        q_max = HCI_TX_DMA_QUEUE_HI15; 
    }
#endif     
#if  IS_RTL88XX_MAC_V4
	if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4) {
        q_max = HCI_TX_DMA_QUEUE_HI15_V1;

    }
#endif     


#else
    // only check MGT, VO, VI, BE, BK, HI0 queue
    q_max = HCI_TX_DMA_QUEUE_HI0;
#endif

    ptx_dma = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);

    for (QueueIdx = 0; QueueIdx <= q_max; QueueIdx++)
    {
        if (HAL_CIRC_CNT_RTK(ptx_dma->tx_queue[QueueIdx].host_idx, \
                    ptx_dma->tx_queue[QueueIdx].hw_idx, \
                    ptx_dma->tx_queue[QueueIdx].total_txbd_num) > count)
            return _TRUE;
    }
    return _FALSE;
}


VOID
TxPolling88XX(
    IN	HAL_PADAPTER        Adapter,
    IN	u1Byte              QueueIndex
)
{
    //QueueIndex, ex. TXPOLL_BEACON_QUEUE

    if ( TXPOLL_BEACON_QUEUE == QueueIndex ) {
#if IS_RTL88XX_MAC_V4
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)
        platform_efio_write_4byte(Adapter, REG_P0MGQ_RXQ_TXRXBD_NUM, platform_efio_read_4byte(Adapter, REG_P0MGQ_RXQ_TXRXBD_NUM) | BIT_PCIE_P0BCNQ_FLAG);
#endif
#if IS_RTL88XX_MAC_V1_V2_V3
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3)
        platform_efio_write_2byte(Adapter, REG_RX_RXBD_NUM, platform_efio_read_2byte(Adapter, REG_RX_RXBD_NUM) | BIT12);
#endif
    }
    else {
        //Do Nothing
    }
}

#if IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE || IS_EXIST_RTL8881AEM || IS_EXIST_RTL8814AE 

VOID
FillBeaconDesc88XX
(
    IN	HAL_PADAPTER        Adapter,
    IN  PVOID               _pdesc,
    IN  PVOID               data_content,
    IN  u2Byte              txLength,
    IN  BOOLEAN             bForceUpdate
)
{
    u4Byte  TXDESCSize;
    PTX_DESC_88XX       pdesc = (PTX_DESC_88XX)_pdesc;

    TXDESCSize = SIZE_TXDESC_88XX;

    platform_zero_memory(pdesc, TXDESCSize);

    //Dword0
    SET_DESC_FIELD_CLR(pdesc->Dword0, 1, TX_DW0_BMC_MSK, TX_DW0_BMC_SH);
    SET_DESC_FIELD_CLR(pdesc->Dword0, TXDESCSize, TX_DW0_OFFSET_MSK, TX_DW0_OFFSET_SH);
    SET_DESC_FIELD_CLR(pdesc->Dword0, txLength, TX_DW0_TXPKSIZE_MSK, TX_DW0_TXPKSIZE_SH);

    //Dword1
    SET_DESC_FIELD_CLR(pdesc->Dword1, TXDESC_QSEL_BCN, TX_DW1_QSEL_MSK, TX_DW1_QSEL_SH);


#if CFG_HAL_SUPPORT_MBSSID
        if (HAL_IS_VAP_INTERFACE(Adapter)) {
    
        // set MBSSID for each VAP_ID
      
        SET_DESC_FIELD_CLR(pdesc->Dword1, HAL_VAR_VAP_INIT_SEQ, TX_DW1_MACID_MSK, TX_DW1_MACID_SH);
        SET_DESC_FIELD_CLR(pdesc->Dword6, HAL_VAR_VAP_INIT_SEQ, TX_DW6_MBSSID_MSK, TX_DW6_MBSSID_SH);  
    
        }         
#endif //#if CFG_HAL_SUPPORT_MBSSID


    SET_DESC_FIELD_CLR(pdesc->Dword9, GetSequence(data_content), TX_DW9_SEQ_MSK, TX_DW9_SEQ_SH);

    SET_DESC_FIELD_CLR(pdesc->Dword3, 1, TX_DW3_DISDATAFB_MSK, TX_DW3_DISDATAFB_SH);
    SET_DESC_FIELD_CLR(pdesc->Dword3, 1, TX_DW3_USERATE_MSK, TX_DW3_USERATE_SH);

/*cy wang cfg p2p rm*/
    if (HAL_VAR_IS_40M_BW) {
        if (HAL_VAR_OFFSET_2ND_CHANNEL == HT_2NDCH_OFFSET_BELOW) {
            SET_DESC_FIELD_CLR(pdesc->Dword5, TXDESC_DATASC_LOWER, TX_DW5_DATA_SC_MSK, TX_DW5_DATA_SC_SH);
        }
		else {
            SET_DESC_FIELD_CLR(pdesc->Dword5, TXDESC_DATASC_UPPER, TX_DW5_DATA_SC_MSK, TX_DW5_DATA_SC_SH);
		}
	}

    //Dword4    /*cy wang cfg p2p , 6m rate beacon*//*cy wang cfg p2p*/
    #ifdef P2P_SUPPORT	// 2014-0328 use 6m rate send beacon
    if(Adapter->pmib->p2p_mib.p2p_enabled){          
        SET_DESC_FIELD_CLR(pdesc->Dword4, 4, TX_DW4_RTSRATE_MSK, TX_DW4_RTSRATE_SH);        
        SET_DESC_FIELD_CLR(pdesc->Dword4, 4, TX_DW4_DATARATE_MSK, TX_DW4_DATARATE_SH);        
    }    
    #endif   
/*cy wang cfg p2p*/
/*
		 * Intel IOT, dynamic enhance beacon tx AGC
*/

	if (Adapter->pmib->dot11StationConfigEntry.beacon_rate != 0xff)
		SET_DESC_FIELD_CLR(pdesc->Dword4, Adapter->pmib->dot11StationConfigEntry.beacon_rate, TX_DW4_DATARATE_MSK, TX_DW4_DATARATE_SH);

	if(Adapter->pmib->dot11RFEntry.bcnagc==1) {
		if(Adapter->pshare->rf_ft_var.bcn_pwr_idex+6 <= Adapter->pshare->rf_ft_var.bcn_pwr_max)			
			SET_DESC_FIELD_CLR(pdesc->Dword5, 4, TX_DW5_TXPWR_OFSET_MSK, TX_DW5_TXPWR_OFSET_SH);	// +3dB
	} else if (Adapter->pmib->dot11RFEntry.bcnagc==2)  {
		if(Adapter->pshare->rf_ft_var.bcn_pwr_idex+12 <= Adapter->pshare->rf_ft_var.bcn_pwr_max) {
			SET_DESC_FIELD_CLR(pdesc->Dword5, 5, TX_DW5_TXPWR_OFSET_MSK, TX_DW5_TXPWR_OFSET_SH);	// +6dB
		} else if(Adapter->pshare->rf_ft_var.bcn_pwr_idex+6 <= Adapter->pshare->rf_ft_var.bcn_pwr_max) {
			SET_DESC_FIELD_CLR(pdesc->Dword5, 4, TX_DW5_TXPWR_OFSET_MSK, TX_DW5_TXPWR_OFSET_SH);	// +3dB
		}
	} else {
		if (Adapter->bcnTxAGC ==1 ) {
			SET_DESC_FIELD_CLR(pdesc->Dword5, 4, TX_DW5_TXPWR_OFSET_MSK, TX_DW5_TXPWR_OFSET_SH);	// +3dB
		} else if (Adapter->bcnTxAGC ==2) {
			SET_DESC_FIELD_CLR(pdesc->Dword5, 5, TX_DW5_TXPWR_OFSET_MSK, TX_DW5_TXPWR_OFSET_SH);	// +6dB
		} else {
			SET_DESC_FIELD_CLR(pdesc->Dword5, 0, TX_DW5_TXPWR_OFSET_MSK, TX_DW5_TXPWR_OFSET_SH);
		}

#ifdef RF_MIMO_SWITCH
		if (Adapter->pshare->rf_ft_var.idle_ps_ext && Adapter->pshare->idle_txpwr_reduction) {
			if (Adapter->pshare->idle_txpwr_reduction <= 3)
				SET_DESC_FIELD_CLR(pdesc->Dword5, Adapter->pshare->idle_txpwr_reduction, TX_DW5_TXPWR_OFSET_MSK, TX_DW5_TXPWR_OFSET_SH);
		}
#endif
		if (Adapter->pmib->dot11RFEntry.txpwr_reduction) {
			if (Adapter->pmib->dot11RFEntry.txpwr_reduction <= 3)
				if(GET_DESC_FIELD(pdesc->Dword5, TX_DW5_TXPWR_OFSET_MSK, TX_DW5_TXPWR_OFSET_SH) < Adapter->pmib->dot11RFEntry.txpwr_reduction)
					SET_DESC_FIELD_CLR(pdesc->Dword5, Adapter->pmib->dot11RFEntry.txpwr_reduction, TX_DW5_TXPWR_OFSET_MSK, TX_DW5_TXPWR_OFSET_SH);
		}
	}
	if (Adapter->pmib->dot11RFEntry.bcn2path && IS_HARDWARE_TYPE_8192EE(Adapter) )
		SET_DESC_FIELD_CLR(pdesc->Dword5, 3, TX_DW5_TX_ANT_MSK, TX_DW5_TX_ANT_SH);

    // TODO: Why ?
    HAL_VAR_IS_40M_BW_BAK   = HAL_VAR_IS_40M_BW;
    HAL_VAR_TX_BEACON_LEN   = txLength;

    SET_DESC_FIELD_CLR(pdesc->Dword7, txLength, TX_DW7_SW_TXBUFF_MSK, TX_DW7_SW_TXBUFF_SH);


#if 0   // TODO: Filen: test code ?
#if (defined(UNIVERSAL_REPEATER) || defined(MBSSID))
    if (IS_ROOT_INTERFACE(Adapter)) {
		if (Adapter->pshare->rf_ft_var.swq_dbg	== 30) {
			pdesc->Dword9 |= set_desc((1122 & TXdesc_92E_TX_SeqMask)  << TXdesc_92E_TX_SeqSHIFT);
		}
		else {
			pdesc->Dword9 |= set_desc((5566 & TXdesc_92E_TX_SeqMask)  << TXdesc_92E_TX_SeqSHIFT);
		}
    }
    else {
        pdesc->Dword9 |= set_desc((GetSequence(data_content) & TXdesc_92E_TX_SeqMask)  << TXdesc_92E_TX_SeqSHIFT);        
    }
#else
    if (Adapter->pshare->rf_ft_var.swq_dbg == 30) {
        pdesc->Dword9 |= set_desc((1122 & TXdesc_92E_TX_SeqMask)  << TXdesc_92E_TX_SeqSHIFT);
    }
    else {
        pdesc->Dword9 |= set_desc((5566 & TXdesc_92E_TX_SeqMask)  << TXdesc_92E_TX_SeqSHIFT);
    }
#endif  //(defined(UNIVERSAL_REPEATER) || defined(MBSSID))
#endif

    // Group Bit Control
    SET_DESC_FIELD_CLR(pdesc->Dword9, (HAL_VAR_TIM_OFFSET-24), TX_DW9_GROUPBIT_IE_OFFSET_MSK, TX_DW9_GROUPBIT_IE_OFFSET_SH);
    // Auto set bitmap control by HW
    if (HAL_OPMODE & WIFI_ADHOC_STATE) {
        SET_DESC_FIELD_CLR(pdesc->Dword9, 0, TX_DW9_GROUPBIT_IE_ENABLE_MSK, TX_DW9_GROUPBIT_IE_ENABLE_SH);
    } else {
        SET_DESC_FIELD_CLR(pdesc->Dword9, 1, TX_DW9_GROUPBIT_IE_ENABLE_MSK, TX_DW9_GROUPBIT_IE_ENABLE_SH);
    }
    // TODO: Check with Button


#ifdef TRXBD_CACHABLE_REGION
    _dma_cache_wback((unsigned long)((PVOID)(pdesc)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), TXDESCSize);
#endif //#ifdef TRXBD_CACHABLE_REGION
}

#endif //#if IS_EXIST_RTL8192EE || IS_EXIST_RTL8881AEM || IS_EXIST_RTL8814AE 

VOID
GetBeaconTXBDTXDESC88XX(
    IN	HAL_PADAPTER                Adapter,
    OUT PTX_BUFFER_DESCRIPTOR       *pTXBD,
    OUT PTX_DESC_88XX               *ptx_desc
)
{
    PHCI_TX_DMA_MANAGER_88XX        ptx_dma = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);
    u4Byte                          TXBDBeaconOffset;
    u4Byte                          DMA_QUEUE_BCN;  

    //3 Get TXBD PTR & Get TXDESC PTR
#if (IS_EXIST_RTL8192EE || IS_RTL8192F_SERIES || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8198FEM || IS_EXIST_RTL8814BE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE|| IS_EXIST_RTL8197GEM )
    if (IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8192F(Adapter) || IS_HARDWARE_TYPE_8814A(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8814B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)|| IS_HARDWARE_TYPE_8197G(Adapter)) {    
        TXBDBeaconOffset = TXBD_BEACON_OFFSET_V1;        
    }
#endif  //(IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822BE || IS_EXIST_RTL8198FEM || IS_EXIST_RTL8814BE || IS_EXIST_RTL8197GEM)
    
#if IS_EXIST_RTL8881AEM
    if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
        TXBDBeaconOffset = TXBD_BEACON_OFFSET_V2;        
    }
#endif  //IS_EXIST_RTL8881AEM

#if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822BE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE|| IS_EXIST_RTL8197GEM)
    if (IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8192F(Adapter) || IS_HARDWARE_TYPE_8814A(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)|| IS_HARDWARE_TYPE_8197G(Adapter)) {    
        DMA_QUEUE_BCN = HCI_TX_DMA_QUEUE_BCN;  
    }
#endif  //(IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822BE  || IS_EXIST_RTL8197GEM)

#if IS_EXIST_RTL8198FEM
    if ( IS_HARDWARE_TYPE_8198F(Adapter) ) {
        DMA_QUEUE_BCN = HCI_TX_DMA_QUEUE_BCN_V1; 
    }
#endif  //IS_EXIST_RTL8198FEM

#if IS_EXIST_RTL8814BE
    if ( IS_HARDWARE_TYPE_8814B(Adapter) ) {
        DMA_QUEUE_BCN = HCI_TX_DMA_QUEUE_BCN_V2; 

    }
#endif  //IS_EXIST_RTL8814BE

    #if CFG_HAL_DBG
    //Error Check
    if ( TXBDBeaconOffset % sizeof(TX_BUFFER_DESCRIPTOR) != 0 ) {
        RT_TRACE(COMP_SEND, DBG_SERIOUS, ("TXBDBeaconOffset is mismatched\n")); 
        return;
    }
    #endif  //CFG_HAL_DBG
    
#if CFG_HAL_SUPPORT_MBSSID
    if (HAL_IS_VAP_INTERFACE(Adapter)) {
        *pTXBD       = (PTX_BUFFER_DESCRIPTOR)((dma_addr_t)ptx_dma->tx_queue[DMA_QUEUE_BCN].pTXBD_head +
                (HAL_VAR_VAP_INIT_SEQ * TXBDBeaconOffset));

        *ptx_desc    = (PTX_DESC_88XX)((dma_addr_t)ptx_dma->tx_queue[DMA_QUEUE_BCN].ptx_desc_head + 
                    HAL_VAR_VAP_INIT_SEQ*sizeof(TX_DESC_88XX));
    } else {
        *pTXBD       = ptx_dma->tx_queue[DMA_QUEUE_BCN].pTXBD_head;
        *ptx_desc    = (PTX_DESC_88XX)ptx_dma->tx_queue[DMA_QUEUE_BCN].ptx_desc_head;
    }
#else
    *pTXBD       = ptx_dma->tx_queue[DMA_QUEUE_BCN].pTXBD_head;
    *ptx_desc    = (PTX_DESC_88XX)ptx_dma->tx_queue[DMA_QUEUE_BCN].ptx_desc_head;
#endif

}

VOID
SetBeaconDownload88XX (
    IN	HAL_PADAPTER        Adapter,
    IN  u4Byte              Value
) 
{
    PTX_BUFFER_DESCRIPTOR       pTXBD;
    PTX_DESC_88XX               ptx_desc;
	u4Byte						TXBDBeaconOffset;

#ifdef CONFIG_RTL_OFFLOAD_DRIVER
	if (IS_HARDWARE_TYPE_8814B(Adapter))
		return;
#endif

	//3 Get TXBD PTR & Get TXDESC PTR
#if (IS_RTL8192E_SERIES || IS_RTL8192F_SERIES  || IS_RTL8814A_SERIES || IS_RTL8197F_SERIES || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES || IS_RTL8198F_SERIES || IS_RTL8814B_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES|| IS_RTL8197G_SERIES)
    if (IS_HARDWARE_TYPE_8192E(Adapter) || IS_HARDWARE_TYPE_8192F(Adapter)  || IS_HARDWARE_TYPE_8814A(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8814B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)|| IS_HARDWARE_TYPE_8197G(Adapter)) {    	
		TXBDBeaconOffset = TXBD_BEACON_OFFSET_V1;		
	}
#endif  //IS_RTL8192E_SERIES || IS_RTL8814A_SERIES || IS_RTL8197F_SERIES
	
#if IS_RTL8881A_SERIES
	if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
		TXBDBeaconOffset = TXBD_BEACON_OFFSET_V2;		
	}
#endif  //IS_RTL8881A_SERIES

    GetBeaconTXBDTXDESC88XX(Adapter, &pTXBD, &ptx_desc);

    switch(Value) {
        case HW_VAR_BEACON_ENABLE_DOWNLOAD:
            SET_DESC_FIELD_CLR(pTXBD->TXBD_ELE[0].Dword0, 1, TXBD_DW0_BCN_OWN_MSK, TXBD_DW0_BCN_OWN_SH);
            break;
        case HW_VAR_BEACON_DISABLE_DOWNLOAD:
            SET_DESC_FIELD_CLR(pTXBD->TXBD_ELE[0].Dword0, 0, TXBD_DW0_BCN_OWN_MSK, TXBD_DW0_BCN_OWN_SH);
            break;
        default:
            RT_TRACE(COMP_BEACON, DBG_SERIOUS, ("SetBeaconDownload88XX setting error: 0x%x \n", Value));
            break;
    }

    //write back cache: TXBD
#ifdef CONFIG_NET_PCI
    if (HAL_IS_PCIBIOS_TYPE(Adapter)) {
        u4Byte uiTmp=0;
#if CFG_HAL_SUPPORT_MBSSID
        if (HAL_IS_VAP_INTERFACE(Adapter)) {
            uiTmp=HAL_VAR_VAP_INIT_SEQ * TXBDBeaconOffset;
        }
#endif
        HAL_CACHE_SYNC_WBACK(Adapter, _GET_HAL_DATA(Adapter)->txBD_dma_ring_addr[HCI_TX_DMA_QUEUE_BCN] + uiTmp, sizeof(TX_BUFFER_DESCRIPTOR), HAL_PCI_DMA_TODEVICE);
    } else 
 #endif
    {
#ifdef TRXBD_CACHABLE_REGION
#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
        // With CCI400 ,Do nothing for cache coherent code
#else
        rtl_cache_sync_wback(Adapter,(unsigned long)((PVOID)(&(pTXBD->TXBD_ELE[0].Dword0))-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), 
            sizeof(pTXBD->TXBD_ELE[0].Dword0),PCI_DMA_TODEVICE);
#endif //CONFIG_ENABLE_CCI400
#else
        _dma_cache_wback((unsigned long)((PVOID)(&(pTXBD->TXBD_ELE[0].Dword0))-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), 
            sizeof(pTXBD->TXBD_ELE[0].Dword0));
#endif //CONFIG_RTL_8198F
#else
        HAL_CACHE_SYNC_WBACK(Adapter, HAL_VIRT_TO_BUS1(Adapter, (PVOID)pTXBD, sizeof(TX_BUFFER_DESCRIPTOR), HAL_PCI_DMA_TODEVICE),
                        sizeof(TX_BUFFER_DESCRIPTOR), HAL_PCI_DMA_TODEVICE);
#endif //#ifdef TRXBD_CACHABLE_REGION
    }
}

VOID
SigninBeaconTXBD88XX
(
    IN	HAL_PADAPTER        Adapter,
    IN  pu4Byte             beaconbuf,
    IN  u2Byte              frlen
)
{
    PTX_BUFFER_DESCRIPTOR           pTXBD;
    PTX_DESC_88XX                   ptx_desc;
    u4Byte                          TotalLen;
    u4Byte                          PSBLen;
    u4Byte                          TXDESCSize;
    
    GetBeaconTXBDTXDESC88XX(Adapter, &pTXBD, &ptx_desc);

#if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE  || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM)
    if ( IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8192FE(Adapter)  || IS_HARDWARE_TYPE_8814AE(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter)|| IS_HARDWARE_TYPE_8198F(Adapter) ) {
        TXDESCSize = SIZE_TXDESC_88XX;
    }
#endif  //#if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM)

#if (IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE|| IS_EXIST_RTL8197GEM)
    if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)|| IS_HARDWARE_TYPE_8197G(Adapter))  {
        TXDESCSize = SIZE_TXDESC_88XX_V1;
    }
#endif //IS_EXIST_RTL8822BE || IS_EXIST_RTL8198FEM ||  IS_EXIST_RTL8197GEM

#if (IS_EXIST_RTL8814BE)
    if ( IS_HARDWARE_TYPE_8814B(Adapter))  {
        TXDESCSize = SIZE_TXDESC_88XX_V2_BCN; //IE_body+IE0~IE4
    }
#endif //IS_EXIST_RTL8814BE


#if 0 // CFG_HAL_DBG
        //TXDESC
        RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%lx, Dword1: 0x%lx\n", 0, GET_DESC(pTXBD->TXBD_ELE[0].Dword0), GET_DESC(pTXBD->TXBD_ELE[0].Dword1)));
        RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "TXDESC:\n", ptx_desc, TXDESCSize);

        //Header + Payload
        RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%lx, Dword1: 0x%lx\n", 1, GET_DESC(pTXBD->TXBD_ELE[1].Dword0), GET_DESC(pTXBD->TXBD_ELE[1].Dword1)));
        RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "Content:\n", beaconbuf, frlen);

        RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%lx, Dword1: 0x%lx\n", 2, GET_DESC(pTXBD->TXBD_ELE[2].Dword0), GET_DESC(pTXBD->TXBD_ELE[2].Dword1)));
        RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%lx, Dword1: 0x%lx\n\n\n", 3, GET_DESC(pTXBD->TXBD_ELE[3].Dword0), GET_DESC(pTXBD->TXBD_ELE[3].Dword1)));
#if 0
        DumpTxPktBuf(Adapter);

        PHCI_RX_DMA_MANAGER_88XX prx_dma = (PHCI_RX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PRxDMA88XX);
        PHCI_TX_DMA_MANAGER_88XX ptx_dma = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);

        //No Beacon
        PTX_BUFFER_DESCRIPTOR ptxbd_head = (PTX_BUFFER_DESCRIPTOR)(prx_dma->rx_queue[HCI_RX_DMA_QUEUE_MAX_NUM-1].pRXBD_head +
        prx_dma->rx_queue[HCI_RX_DMA_QUEUE_MAX_NUM-1].total_rxbd_num);

        PTX_DESC_88XX ptx_desc_head = (PTX_DESC_88XX)((pu1Byte)ptxbd_head +
        sizeof(TX_BUFFER_DESCRIPTOR) * TOTAL_NUM_TXBD_NO_BCN);

        PTX_BUFFER_DESCRIPTOR ptxbd_bcn_head  = (PTX_BUFFER_DESCRIPTOR)((pu1Byte)ptx_desc_head +
        TXDESCSize * TOTAL_NUM_TXBD_NO_BCN);

        PTX_DESC_88XX ptxdesc_bcn_head = (PTX_DESC_88XX)((pu1Byte)ptxbd_bcn_head +
          (1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1);
          
        PTX_BUFFER_DESCRIPTOR ptxbd_bcn_cur;

        PTX_BUFFER_DESCRIPTOR ptxbd = ptx_dma->tx_queue[HCI_TX_DMA_QUEUE_BCN].pTXBD_head;
        PTX_DESC_88XX ptx_desc      = ptx_dma->tx_queue[HCI_TX_DMA_QUEUE_BCN].ptx_desc_head;
        u4Byte i;

        for (i = 0; i < 1+HAL_NUM_VWLAN; i++)
        {
           ptxbd_bcn_cur = (pu1Byte)ptxbd + TXBD_BEACON_OFFSET_V1 * i;
               RT_TRACE_F(COMP_INIT, DBG_TRACE, ("ptxbd_bcn[%ld]: Dword0: 0x%lx, Dword1: 0x%lx\n",
                          i,
                          (u4Byte)GET_DESC(ptxbd_bcn_cur->TXBD_ELE[0].Dword0),
                          (u4Byte)GET_DESC(ptxbd_bcn_cur->TXBD_ELE[0].Dword1)
                          ));             
        }
#endif
#endif

#if 0   // TODO: Filen
#ifdef DFS
        if (!priv->pmib->dot11DFSEntry.disable_DFS &&
            (timer_pending(&GET_ROOT(priv)->ch_avail_chk_timer))) {
            pdesc->Dword0 &= set_desc(~(TX_OWN));
            RTL_W16(PCIE_CTRL_REG, RTL_R16(PCIE_CTRL_REG)| (BCNQSTOP));
    
            return;
        }
#endif
#endif

    GET_HAL_INTERFACE(Adapter)->FillBeaconDescHandler(Adapter, ptx_desc, (PVOID)beaconbuf, frlen, _FALSE);
    //FillBeaconDesc88XX(Adapter, ptx_desc, (PVOID)beaconbuf, frlen, _FALSE);

    //Segment 1: Payload
    SET_DESC_FIELD_CLR(pTXBD->TXBD_ELE[1].Dword0, frlen, TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
    SET_DESC_FIELD_CLR(pTXBD->TXBD_ELE[1].Dword1,
                HAL_VIRT_TO_BUS1(Adapter, (PVOID)beaconbuf, frlen, HAL_PCI_DMA_TODEVICE) + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
                TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);

    //Segment 0: Wifi Info
    //PrepareTxDesc88XX has done    
    TotalLen = TXDESCSize + frlen;

#if IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE || IS_EXIST_RTL8197FEM 
    if ( IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8192FE(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter)) {
        PSBLen   = (TotalLen%PBP_PSTX_SIZE) == 0 ? (TotalLen/PBP_PSTX_SIZE):((TotalLen/PBP_PSTX_SIZE)+1);        
    }
#endif //IS_EXIST_RTL8192EE || IS_EXIST_RTL8197FEM

#if IS_EXIST_RTL8814AE || IS_RTL8822B_SERIES || IS_RTL8821C_SERIES|| IS_EXIST_RTL8198FEM  || IS_EXIST_RTL8814BE || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES|| IS_EXIST_RTL8197GEM
    if ( IS_HARDWARE_TYPE_8814AE(Adapter)|| IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8814B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter)) {
        PSBLen   = (TotalLen%PBP_PSTX_SIZE_V1) == 0 ? (TotalLen/PBP_PSTX_SIZE_V1):((TotalLen/PBP_PSTX_SIZE_V1)+1);        
    }
#endif //#if IS_EXIST_RTL8814AE || IS_RTL8822B_SERIES || IS_EXIST_RTL8198FEM || IS_EXIST_RTL8814BE || IS_EXIST_RTL8197GEM 

#if IS_EXIST_RTL8881AEM
    if ( IS_HARDWARE_TYPE_8881A(Adapter) ) {
        PSBLen   = TotalLen;
    }
#endif

    SET_DESC_FIELD_CLR(pTXBD->TXBD_ELE[0].Dword0, PSBLen, TXBD_DW0_BCN_PSLEN_MSK, TXBD_DW0_BCN_PSLEN_SH);

    pTXBD->TXBD_ELE[2].Dword0 = SET_DESC(0);
    pTXBD->TXBD_ELE[2].Dword1 = SET_DESC(0);
    pTXBD->TXBD_ELE[3].Dword0 = SET_DESC(0);    
    pTXBD->TXBD_ELE[3].Dword1 = SET_DESC(0);

    //3 Write Cache Sync Back
#if 0
    static int ki = 0;
    if(ki < 10)
    {
        RT_TRACE_F(COMP_SEND, DBG_TRACE, ("pTXBD = %x \n", pTXBD));            
        RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "Beacon TXBD\n", pTXBD, 40);
    
        RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%lx, Dword1: 0x%lx\n", 0, GET_DESC(pTXBD->TXBD_ELE[0].Dword0), GET_DESC(pTXBD->TXBD_ELE[0].Dword1)));
        RT_TRACE_F(COMP_SEND, DBG_TRACE, ("ptx_desc = %x,beaconbuf = %x \n", ptx_desc,beaconbuf));        
        RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "TXDESC:\n", ptx_desc, TXDESCSize);

        //Header + Payload
        RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%lx, Dword1: 0x%lx\n", 1, GET_DESC(pTXBD->TXBD_ELE[1].Dword0), GET_DESC(pTXBD->TXBD_ELE[1].Dword1)));
        RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "Content:\n", beaconbuf, frlen);

        ki++;
    }
        //RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%lx, Dword1: 0x%lx\n", 2, GET_DESC(pTXBD->TXBD_ELE[2].Dword0), GET_DESC(pTXBD->TXBD_ELE[2].Dword1)));
        //RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%lx, Dword1: 0x%lx\n\n\n", 3, GET_DESC(pTXBD->TXBD_ELE[3].Dword0), GET_DESC(pTXBD->TXBD_ELE[3].Dword1)));
#endif
        
#ifdef TRXBD_CACHABLE_REGION
#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
        // With CCI400 ,Do nothing for cache coherent code
#else
        rtl_cache_sync_wback(Adapter,(unsigned long)((PVOID)(pTXBD)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(TX_BUFFER_DESCRIPTOR),PCI_DMA_TODEVICE);

        rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD((unsigned long)bus_to_virt(pTXBD->TXBD_ELE[0].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
        TXDESCSize,PCI_DMA_TODEVICE);

        rtl_cache_sync_wback(Adapter,((GET_DESC_FIELD((unsigned long)bus_to_virt(pTXBD->TXBD_ELE[1].Dword1), TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
        (u4Byte)frlen,PCI_DMA_TODEVICE);       
#endif //CONFIG_ENABLE_CCI400
#else
    _dma_cache_wback((unsigned long)((PVOID)(pTXBD)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(TX_BUFFER_DESCRIPTOR));

    _dma_cache_wback(((GET_DESC_FIELD(pTXBD->TXBD_ELE[0].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
        TXDESCSize);

    _dma_cache_wback(((GET_DESC_FIELD(pTXBD->TXBD_ELE[1].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
        (u4Byte)frlen);
#endif //CONFIG_RTL_8198F
#else
    //write back cache: TXDESC    
    HAL_CACHE_SYNC_WBACK(Adapter,
        GET_DESC_FIELD(pTXBD->TXBD_ELE[0].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
        TXDESCSize, HAL_PCI_DMA_TODEVICE);
    
    //write back cache: Payload    
    HAL_CACHE_SYNC_WBACK(Adapter,
        GET_DESC_FIELD(pTXBD->TXBD_ELE[1].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL,
        (u4Byte)frlen, HAL_PCI_DMA_TODEVICE);
#endif //#ifdef TRXBD_CACHABLE_REGION

#if 0 //eric-8822 CFG_HAL_DBG
     
    //TXDESC
    RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%08lx, Dword1: 0x%08lx\n", 0, GET_DESC(pTXBD->TXBD_ELE[0].Dword0), GET_DESC(pTXBD->TXBD_ELE[0].Dword1)));
    
    RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "TXDESC:\n", ptx_desc, TXDESCSize);

    //Payload
    RT_TRACE_F(COMP_SEND, DBG_TRACE, ("TXBD_ELE[%d], Dword0: 0x%08lx, Dword1: 0x%08lx\n", 1, GET_DESC(pTXBD->TXBD_ELE[1].Dword0), GET_DESC(pTXBD->TXBD_ELE[1].Dword1)));
    RT_PRINT_DATA(COMP_SEND, DBG_TRACE, "Payload:\n", beaconbuf, frlen);
 
#endif
}



u2Byte
GetTxQueueHWIdx88XX
(
    IN	HAL_PADAPTER        Adapter,
    IN  u4Byte              q_num       //enum _TX_QUEUE_
)
{
    PHCI_TX_DMA_MANAGER_88XX    ptx_dma;

    ptx_dma = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);

    return (BIT_MASK_QUEUE_IDX &
        (HAL_RTL_R32(ptx_dma->tx_queue[GET_HAL_INTERFACE(Adapter)->MappingTxQueueHandler(Adapter, q_num)].reg_rwptr_idx)>>BIT_SHIFT_QUEUE_HW_IDX));    
}

#if CFG_HAL_TX_SHORTCUT

#if 0
PVOID
GetShortCutTxDesc88XX(
    IN  HAL_PADAPTER    Adapter
)
{
    // TODO: pre-allocate a TXDESC pool when system startup
    return (PVOID)HALMalloc(Adapter, sizeof(TX_DESC_88XX));
}

VOID
ReleaseShortCutTxDesc88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  PVOID           pTxDesc
)
{
    // TODO: right ?, release to the TXDESC pool
    HAL_free(pTxDesc);
}
#endif

/**
 * direction: 
 *      1) 0x01: store current TXBD's txdesc to driver layer
 *      2) 0x02: copy backup txdesc from driver layer to current TXBD's TXDESC
 */
HAL_IMEM
PVOID
CopyShortCutTxDesc88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          queueIndex, //HCI_TX_DMA_QUEUE_88XX    
    IN  PVOID           pTxDesc,
    IN  u4Byte          direction
)
{
    PHCI_TX_DMA_MANAGER_88XX        ptx_dma;
    PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q;
    PTX_DESC_88XX                   cur_txdesc;
    u4Byte                          TXDESCSize;
#if 0// CFG_HAL_DBG
    PTX_BUFFER_DESCRIPTOR           cur_txbd;
#endif // CFG_HAL_DBG

    ptx_dma     = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);
    cur_q       = &(ptx_dma->tx_queue[queueIndex]);
    cur_txdesc  = (PTX_DESC_88XX)cur_q->ptx_desc_head + cur_q->host_idx;

#if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE  || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM)
    if ( IS_HARDWARE_TYPE_8881A(Adapter) || IS_HARDWARE_TYPE_8192EE(Adapter) || IS_HARDWARE_TYPE_8192FE(Adapter)  || IS_HARDWARE_TYPE_8814AE(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) ) {
        TXDESCSize = SIZE_TXDESC_88XX;
    }
#endif  //#if (IS_EXIST_RTL8881AEM || IS_EXIST_RTL8192EE || IS_EXIST_RTL8814AE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM)

#if (IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE|| IS_EXIST_RTL8197GEM)
    if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)|| IS_HARDWARE_TYPE_8197G(Adapter))  {
        TXDESCSize = SIZE_TXDESC_88XX_V1;
    }
#endif //IS_EXIST_RTL8822BE  || IS_EXIST_RTL8197GEM

#if (IS_EXIST_RTL8814BE)
    if ( IS_HARDWARE_TYPE_8814B(Adapter))  {
        TXDESCSize = SIZE_TXDESC_BODY+SIZE_TXDESC_IE*2; //IE_body+IE0+IE1
    }
#endif // (IS_EXIST_RTL8814BE)

#if 0 // CFG_HAL_DBG
    cur_txbd    = cur_q->pTXBD_head + cur_q->host_idx;

    if (HAL_VIRT_TO_BUS((u4Byte)cur_txdesc) != GET_DESC(cur_txbd->TXBD_ELE[0].Dword1)) {
        printk("%s(%d): cur_txdesc: 0x%08x, cur_txbd[0].Dword1: 0x%08x \n", __FUNCTION__, __LINE__,
            HAL_VIRT_TO_BUS((u4Byte)cur_txdesc), GET_DESC(cur_txbd->TXBD_ELE[0].Dword1));
    }    
#endif // CFG_HAL_DBG

    if (0x01 == direction) {
#if (IS_EXIST_RTL8812FE || IS_EXIST_RTL8197GEM)
		if (IS_HARDWARE_TYPE_8812F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter)) {				
			PTX_DESC_88XX		ptx_desc	= (PTX_DESC_88XX)cur_txdesc;
			SET_TX_DESC_SHCUT_CAM_NO_CLR(ptx_desc,GET_TX_DESC_MACID(ptx_desc));
		}
#endif					
        HAL_memcpy(pTxDesc, cur_txdesc, TXDESCSize);

    } else { // 0x02 == direction
        HAL_memcpy(cur_txdesc, pTxDesc, TXDESCSize);
    }

    return cur_txdesc;
}


#if IS_EXIST_RTL8192EE || IS_EXIST_RTL8192FE || IS_EXIST_RTL8881AEM || IS_EXIST_RTL8814AE 

HAL_IMEM
VOID
SetShortCutTxBuffSize88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  PVOID           pTxDesc,
    IN  u2Byte          txBuffSize
)
{
    PTX_DESC_88XX   ptx_desc = (PTX_DESC_88XX) pTxDesc;
    SET_DESC_FIELD_CLR(ptx_desc->Dword7, txBuffSize, TX_DW7_SW_TXBUFF_MSK, TX_DW7_SW_TXBUFF_SH);
}

HAL_IMEM
u2Byte
GetShortCutTxBuffSize88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  PVOID           pTxDesc
)
{
    PTX_DESC_88XX   ptx_desc = (PTX_DESC_88XX) pTxDesc;
    return  (u2Byte)GET_DESC_FIELD(ptx_desc->Dword7, TX_DW7_SW_TXBUFF_MSK, TX_DW7_SW_TXBUFF_SH);
}


HAL_IMEM
VOID
FillShortCutTxDesc88XX(
    IN      HAL_PADAPTER    Adapter,
    IN      u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN      PVOID           pDescData,
    IN      PVOID           pTxDesc
)
{
    PTX_DESC_DATA_88XX  pdesc_data  = (PTX_DESC_DATA_88XX)pDescData;
    PTX_DESC_88XX       ptx_desc    = (PTX_DESC_88XX)pTxDesc;
    PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q;
    PHCI_TX_DMA_MANAGER_88XX        ptx_dma;   
    // tx shortcut can reuse TXDESC while 1) no security or 2) hw security
    // if no security iv == 0, so adding iv is ok for no security and hw security
    u2Byte  TX_DESC_TXPKTSIZE   = pdesc_data->hdrLen + pdesc_data->llcLen + pdesc_data->frLen + pdesc_data->iv;
    BOOLEAN TX_DESC_BK          = pdesc_data->bk;
    BOOLEAN TX_DESC_NAVUSEHDR   = pdesc_data->navUseHdr;
    u2Byte  TX_DESC_SEQ         = GetSequence(pdesc_data->pHdr);
    u1Byte  TX_DESC_DATA_STBC   = pdesc_data->dataStbc;
    BOOLEAN TX_DESC_RTY_LMT_EN  = pdesc_data->rtyLmtEn;   
    u1Byte  TX_DESC_DATA_RT_LMT = pdesc_data->dataRtyLmt;
    u4Byte  TXDESCSize = SIZE_TXDESC_88XX;
    u2Byte                          *tx_head;
    struct tx_desc_info *pswdescinfo, *pdescinfo;

    //Dword5
#if defined(AP_SWPS_OFFLOAD)
    u1Byte  TX_DESC_DROP_ID          = pdesc_data->DropID;
#endif
    //Dword 6
#if defined(AP_SWPS_OFFLOAD)
    u2Byte  TX_DESC_SWPS_SEQ         = pdesc_data->SWPS_sequence;
#endif   
#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))
    u1Byte  TX_DESC_ANTSEL			 = pdesc_data->ant_sel;	
    u1Byte  TX_DESC_ANTSEL_A			 = pdesc_data->ant_sel_a;
    u1Byte  TX_DESC_ANTSEL_B			 = pdesc_data->ant_sel_b;
    u1Byte  TX_DESC_ANTSEL_C			 = pdesc_data->ant_sel_c;
#endif	//#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))

    SET_DESC_FIELD_CLR(ptx_desc->Dword0, TX_DESC_TXPKTSIZE, TX_DW0_TXPKSIZE_MSK, TX_DW0_TXPKSIZE_SH);
    SET_DESC_FIELD_CLR(ptx_desc->Dword2, TX_DESC_BK, TX_DW2_BK_MSK, TX_DW2_BK_SH);
    SET_DESC_FIELD_CLR(ptx_desc->Dword3, TX_DESC_NAVUSEHDR, TX_DW3_NAVUSEHDR_MSK, TX_DW3_NAVUSEHDR_SH);
    SET_DESC_FIELD_CLR(ptx_desc->Dword9, TX_DESC_SEQ, TX_DW9_SEQ_MSK, TX_DW9_SEQ_SH);

    if (TX_DESC_RTY_LMT_EN) {
        SET_DESC_FIELD_CLR(ptx_desc->Dword4, TX_DESC_RTY_LMT_EN, TX_DW4_RTY_LMT_EN_MSK, TX_DW4_RTY_LMT_EN_SH);
        SET_DESC_FIELD_CLR(ptx_desc->Dword4, TX_DESC_DATA_RT_LMT, TX_DW4_DATA_RT_LMT_MSK, TX_DW4_DATA_RT_LMT_SH);
    } else if (TX_DESC_DATA_RT_LMT) {
        SET_DESC_FIELD_CLR(ptx_desc->Dword4, 0, TX_DW4_RTY_LMT_EN_MSK, TX_DW4_RTY_LMT_EN_SH);
        SET_DESC_FIELD_CLR(ptx_desc->Dword4, 0, TX_DW4_DATA_RT_LMT_MSK, TX_DW4_DATA_RT_LMT_SH);
    }

    // for force tx rate
    if (HAL_VAR_TX_FORCE_RATE != 0xff) {
        SET_DESC_FIELD_CLR(ptx_desc->Dword3, pdesc_data->useRate, TX_DW3_USERATE_MSK, TX_DW3_USERATE_SH);
        SET_DESC_FIELD_CLR(ptx_desc->Dword3, pdesc_data->disRTSFB, TX_DW3_DISRTSFB_MSK, TX_DW3_DISRTSFB_SH);
        SET_DESC_FIELD_CLR(ptx_desc->Dword3, pdesc_data->disDataFB, TX_DW3_DISDATAFB_MSK, TX_DW3_DISDATAFB_SH);
        SET_DESC_FIELD_CLR(ptx_desc->Dword4, pdesc_data->dataRate, TX_DW4_DATARATE_MSK, TX_DW4_DATARATE_SH);
    }

#if (BEAMFORMING_SUPPORT == 1)
	SET_DESC_FIELD_CLR(ptx_desc->Dword5, TX_DESC_DATA_STBC, TX_DW5_DATA_STBC_MSK, TX_DW5_DATA_STBC_SH);
#endif

	//4 Set Dword5
#if defined(AP_SWPS_OFFLOAD) && IS_EXIST_RTL8192FE
	if ( IS_HARDWARE_TYPE_8192FE(Adapter) ) {
      if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter))
        SET_TX_DESC_DROP_ID(ptx_desc,TX_DESC_DROP_ID);
	}
#endif	
    //4 Set Dword6
#if defined(AP_SWPS_OFFLOAD) && IS_EXIST_RTL8192FE
	if ( IS_HARDWARE_TYPE_8192FE(Adapter) ) {
      if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter))
        SET_TX_DESC_SWPS_SEQ(ptx_desc, TX_DESC_SWPS_SEQ); 
	}
#endif


#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))
   if(TX_DESC_ANTSEL & BIT(0))
	SET_DESC_FIELD_CLR(ptx_desc->Dword6, TX_DESC_ANTSEL_A, TX_DW6_ANTSEL_A_MSK, TX_DW6_ANTSEL_A_SH);
   if(TX_DESC_ANTSEL & BIT(1))
	SET_DESC_FIELD_CLR(ptx_desc->Dword6, TX_DESC_ANTSEL_B, TX_DW6_ANTSEL_B_MSK, TX_DW6_ANTSEL_B_SH);
   if(TX_DESC_ANTSEL & BIT(2))
	SET_DESC_FIELD_CLR(ptx_desc->Dword6, TX_DESC_ANTSEL_C, TX_DW6_ANTSEL_C_MSK, TX_DW6_ANTSEL_C_SH);
#endif	//#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))


#ifdef AP_SWPS_OFFLOAD
    ptx_dma     = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);
    cur_q       = &(ptx_dma->tx_queue[queueIndex]);
    tx_head     = &(cur_q->host_idx);
    pswdescinfo = get_txdesc_info(Adapter->pshare->pdesc_info, queueIndex);
    pdescinfo = pswdescinfo + *tx_head;  

    if(pdescinfo->ptxdesc == NULL)
        pdescinfo->ptxdesc = kmalloc(sizeof(TX_DESC_88XX),GFP_KERNEL);
    
    memcpy((PVOID)pdescinfo->ptxdesc, (PVOID)ptx_desc, sizeof(TX_DESC_88XX));
#endif


#ifdef TRXBD_CACHABLE_REGION
    _dma_cache_wback((unsigned long)((PVOID)ptx_desc-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), TXDESCSize);
#endif //#ifdef TRXBD_CACHABLE_REGION
}

#if CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC
// 8814A used 
HAL_IMEM
VOID
FillHwShortCutTxDesc88XX (
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN  PVOID           pDescData
)
{
    PHCI_TX_DMA_MANAGER_88XX        ptx_dma;
    PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q;
    PTX_DESC_88XX                   ptx_desc;
    PTX_DESC_DATA_88XX              pdesc_data = (PTX_DESC_DATA_88XX)pDescData;
    u4Byte                          TXDESCSize;

    TXDESCSize = SIZE_TXDESC_88XX;          

	// Dword 0
	u2Byte  TX_DESC_TXPKTSIZE		= pdesc_data->hdrLen + pdesc_data->llcLen + pdesc_data->frLen + pdesc_data->iv;
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
    u1Byte	TX_DESC_OFFSET			= ((pdesc_data->smhEn == TRUE) ? (HAL_HW_TXSC_HDR_CONV_ADD_OFFSET+TXDESCSize) : TXDESCSize);
#else
	u1Byte	TX_DESC_OFFSET			= TXDESCSize;
#endif

	// Dword 1
	u1Byte  TX_DESC_MACID       	= pdesc_data->macId;
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
    u1Byte  TX_DESC_PKT_OFFSET      = 0; // unit: 8 bytes. Early mode: 1 units, (8 * 1 = 8 bytes for early mode info)
#endif

	// Dword 3
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
    // Actually, no any conditions can run the (pdesc_data->hdrLen >> 1)  case in our driver.
    // Because, while calling this function, it must enable reuse TXDESC and hdr conv.
	u1Byte	TX_DESC_WHEADER_LEN		= ((pdesc_data->smhEn == TRUE) ? HAL_HW_TXSC_WHEADER_LEN : (pdesc_data->hdrLen >> 1)); // unit: 2 bytes
#endif // CFG_HAL_HW_TX_SHORTCUT_HDR_CONV

	// Dword 8
	//BOOLEAN	TX_DESC_STW_ANT_DIS		= pdesc_data->stwAntDis;
	//BOOLEAN	TX_DESC_STW_RATE_DIS	= pdesc_data->stwRateDis;
	//BOOLEAN	TX_DESC_STW_RB_DIS		= pdesc_data->stwRbDis;
	//BOOLEAN	TX_DESC_STW_PKTRE_DIS 	= pdesc_data->stwPktReDis;
#if 0
3.)	STW_ANT_DIS:  
ant_mapA, ant_mapB, ant_mapC, ant_mapD, ANTSEL_A, ANTSEL_B, Ntx_map, TXPWR_OFFSET 
4.)	STW_RATE_DIS:  
USE_RATE, Data rate, DATA_SHORT, DATA_BW, TRY_RATE
5.)	STW_RB_DIS:  
RATE_ID, DISDATAFB, DISRTSFB, RTS_RATEFB_LMT, DATA_RATEFB_LMT
6.)	STW_PKTRE_DIS:  
RTY_LMT_EN,  DATA_RT_LMT,  BAR_RTY_TH
#endif

	BOOLEAN	TX_DESC_STW_EN			 = pdesc_data->stwEn;
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
	BOOLEAN	TX_DESC_SMH_EN 			 = pdesc_data->smhEn;
#endif // CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
	
	//Dword 9
#if CFG_HAL_HW_SEQ
    u2Byte TX_DESC_SEQ               = 0;
#else
    u2Byte TX_DESC_SEQ               = GetSequence(pdesc_data->pHdr);
#endif

    //Dword 6
#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))
    u1Byte  TX_DESC_ANTSEL			 = pdesc_data->ant_sel;	
    u1Byte  TX_DESC_ANTSEL_A			 = pdesc_data->ant_sel_a;
    u1Byte  TX_DESC_ANTSEL_B			 = pdesc_data->ant_sel_b;
    u1Byte  TX_DESC_ANTSEL_C			 = pdesc_data->ant_sel_c;
#endif	//#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))

#if 0 //(CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC||CFG_HAL_HW_TX_SHORTCUT_HDR_CONV)
    if (pdesc_data->stwEn != 0 || pdesc_data->smhEn != 0)
        printk("%s(%d): stwEn:0x%x, smhEn:0x%x \n", __FUNCTION__, __LINE__, pdesc_data->stwEn, pdesc_data->smhEn);
#endif

#if 0 // show TX Rpt Info
{
    int baseReg = 0x8080, offset, i;
    int lenTXDESC = 10, lenHdrInfo = 20;

	HAL_RTL_W8(0x106, 0x7F);
	HAL_RTL_W32(0x140, 0x662);

    for(i = 0; i < lenTXDESC; i++) {
#if 1
        printk("%08X ", (u4Byte)GET_DESC(HAL_RTL_R32(baseReg)));
#else
        printk("0x%x:%08X ", baseReg, (u4Byte)GET_DESC(HAL_RTL_R32(baseReg)));
#endif
        if (i%4==3)
            printk("\n");
        baseReg += 4;
    }
    printk("\n");

    for(i = 0; i < lenHdrInfo; i++) {
#if 1
        printk("%08X ", (u4Byte)GET_DESC(HAL_RTL_R32(baseReg)));
#else
        printk("0x%x:%08X ", baseReg, (u4Byte)GET_DESC(HAL_RTL_R32(baseReg)));
#endif

        if (i%4==3)
            printk("\n");
        baseReg += 4;
    }
    printk("\n");

#if 0
	printk("%08X %08X %08X %08X \n%08X %08X %08X %08X \n%08X %08X \n", 
                (u4Byte)GET_DESC(HAL_RTL_R32(0x8080)), (u4Byte)GET_DESC(HAL_RTL_R32(0x8084)), 
                (u4Byte)GET_DESC(HAL_RTL_R32(0x8088)), (u4Byte)GET_DESC(HAL_RTL_R32(0x808c)),
                (u4Byte)GET_DESC(HAL_RTL_R32(0x8090)), (u4Byte)GET_DESC(HAL_RTL_R32(0x8094)),
                (u4Byte)GET_DESC(HAL_RTL_R32(0x8098)), (u4Byte)GET_DESC(HAL_RTL_R32(0x809c)),
                (u4Byte)GET_DESC(HAL_RTL_R32(0x80a0)), (u4Byte)GET_DESC(HAL_RTL_R32(0x80a4)));
#endif
}
#endif

    ptx_dma     = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);
    cur_q       = &(ptx_dma->tx_queue[queueIndex]);
    ptx_desc    = ((PTX_DESC_88XX)(cur_q->ptx_desc_head)) + cur_q->host_idx;

    //Clear All Bit
    platform_zero_memory((PVOID)ptx_desc, sizeof(TX_DESC_88XX));

    //4 Set Dword0
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword0, TX_DESC_TXPKTSIZE, TX_DW0_TXPKSIZE_MSK, TX_DW0_TXPKSIZE_SH);
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword0, TX_DESC_OFFSET, TX_DW0_OFFSET_MSK, TX_DW0_OFFSET_SH);

	//4 Set Dword1
    SetTxDescQSel88XX(Adapter, queueIndex, ptx_desc, pdesc_data->tid);        
    if ( (queueIndex >= HCI_TX_DMA_QUEUE_HI0) && (queueIndex <= HCI_TX_DMA_QUEUE_HI7) ) {
        //MacID has written in SetTxDescQSel88XX()
    } else {
        SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, TX_DESC_MACID, TX_DW1_MACID_MSK, TX_DW1_MACID_SH);
    }
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
    SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, TX_DESC_PKT_OFFSET, TX_DW1_PKT_OFFSET_MSK, TX_DW1_PKT_OFFSET_SH);
#endif

	//4 Set Dword3
	
    //4 Set Dword8
	//4 Set Dword9    
#if CFG_HAL_HW_SEQ
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, 1, TX_DW8_EN_HWSEQ_MSK, TX_DW8_EN_HWSEQ_SH);
#else
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword9, TX_DESC_SEQ, TX_DW9_SEQ_MSK, TX_DW9_SEQ_SH);	
#endif


#if IS_RTL88XX_MAC_V2
    if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2) {


		SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, TX_DESC_STW_EN, TX_DW8_STW_EN_MSK, TX_DW8_STW_EN_SH);

		//SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, TX_DESC_STW_ANT_DIS, TX_DW8_STW_ANT_DIS_MSK, TX_DW8_STW_ANT_DIS_SH);
		//SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, TX_DESC_STW_RATE_DIS, TX_DW8_STW_RATE_DIS_MSK, TX_DW8_STW_RATE_DIS_SH);
		//SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, TX_DESC_STW_RB_DIS, TX_DW8_STW_RB_DIS_MSK, TX_DW8_STW_RB_DIS_SH);
		//SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, TX_DESC_STW_PKTRE_DIS, TX_DW8_STW_PKTRE_DIS_MSK, TX_DW8_STW_PKTRE_DIS_SH);

        // for force tx rate
#if 0
        if (HAL_VAR_TX_FORCE_RATE != 0xff) {
        /*                    
             STW_RATE_DIS:  USE_RATE, Data rate, DATA_SHORT, DATA_BW, TRY_RATE
             STW_RB_DIS:     RATE_ID, DISDATAFB, DISRTSFB, RTS_RATEFB_LMT, DATA_RATEFB_LMT
            */            
            SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, 1, TX_DW8_STW_RATE_DIS_MSK, TX_DW8_STW_RATE_DIS_SH);
            SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, 1, TX_DW8_STW_RB_DIS_MSK, TX_DW8_STW_RB_DIS_SH);
            
            SET_DESC_FIELD_NO_CLR(ptx_desc->Dword3, pdesc_data->useRate, TX_DW3_USERATE_MSK, TX_DW3_USERATE_SH);
            SET_DESC_FIELD_NO_CLR(ptx_desc->Dword3, pdesc_data->disRTSFB, TX_DW3_DISRTSFB_MSK, TX_DW3_DISRTSFB_SH);
            SET_DESC_FIELD_NO_CLR(ptx_desc->Dword3, pdesc_data->disDataFB, TX_DW3_DISDATAFB_MSK, TX_DW3_DISDATAFB_SH);
            SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, pdesc_data->dataRate, TX_DW4_DATARATE_MSK, TX_DW4_DATARATE_SH);
            
            SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, pdesc_data->rateId, TX_DW1_RATE_ID_MSK, TX_DW1_RATE_ID_SH);
            SET_DESC_FIELD_NO_CLR(ptx_desc->Dword5, pdesc_data->dataBW, TX_DW5_DATA_BW_MSK, TX_DW5_DATA_BW_SH);

            SET_DESC_FIELD_NO_CLR(ptx_desc->Dword5, 1, TX_DW5_DATA_SHORT_MSK, TX_DW5_DATA_SHORT_SH);            
        }
#endif 

#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
		SET_DESC_FIELD_NO_CLR(ptx_desc->Dword3, TX_DESC_WHEADER_LEN, TX_DW3_WHEADER_V1_MSK, TX_DW3_WHEADER_V1_SH);
		SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, TX_DESC_SMH_EN, TX_DW8_SMH_EN_MSK, TX_DW8_SMH_EN_SH);
#endif // CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
	}
#endif // IS_RTL88XX_MAC_V2

#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))
   if(TX_DESC_ANTSEL & BIT(0))
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword6, TX_DESC_ANTSEL_A, TX_DW6_ANTSEL_A_MSK, TX_DW6_ANTSEL_A_SH);
   if(TX_DESC_ANTSEL & BIT(1))
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword6, TX_DESC_ANTSEL_B, TX_DW6_ANTSEL_B_MSK, TX_DW6_ANTSEL_B_SH);
   if(TX_DESC_ANTSEL & BIT(2))
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword6, TX_DESC_ANTSEL_C, TX_DW6_ANTSEL_C_MSK, TX_DW6_ANTSEL_C_SH);
#endif	//#if (defined(CONFIG_PHYDM_ANTENNA_DIVERSITY))

}
#endif //CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC

#endif //#if IS_EXIST_RTL8192EE || IS_EXIST_RTL8881AEM || IS_EXIST_RTL8814AE 

HAL_IMEM
BOOLEAN
FillShortCutTxHwCtrl88XX(
    IN      HAL_PADAPTER    Adapter,
    IN      u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN      PVOID           pDescData,
    IN      PVOID           pTxDesc,
    IN      u4Byte          direction,
    IN      BOOLEAN         useHW     
)
{
    PVOID       ptx_desc;

    if (0x01 == direction) {
        GET_HAL_INTERFACE(Adapter)->FillTxDescHandler(Adapter, queueIndex, pDescData);
        CopyShortCutTxDesc88XX(Adapter, queueIndex, pTxDesc, direction);
        //printk("\n copy TO sw sc entry\n");
        SetTxBufferDesc88XX(Adapter, queueIndex, pDescData,SET_TXBD_SOURCE_NORMAL);

    } else {    // 0x02 == direction

        #if (CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC || CFG_HAL_SUPPORT_TXDESC_IE)
        if(useHW) {
        #if CFG_HAL_SUPPORT_TXDESC_IE
            if(IS_SUPPORT_TXDESC_IE(Adapter)){ 
                
                ptx_desc = CopyShortCutTxDesc88XX(Adapter, queueIndex, pTxDesc, direction); //fix to copy IE_body+IE0+IE1 now
                //printk("\n copy FROM sw sc entry\n");
                GET_HAL_INTERFACE(Adapter)->FillHwShortCutIEHandler(Adapter, queueIndex, pDescData, ptx_desc);
            }
            else
        #endif                
                GET_HAL_INTERFACE(Adapter)->FillHwShortCutTxDescHandler(Adapter, queueIndex, pDescData);
        }
        else
        #endif // CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC || CFG_HAL_SUPPORT_TXDESC_IE
        {
        
            ptx_desc = CopyShortCutTxDesc88XX(Adapter, queueIndex, pTxDesc, direction);
            GET_HAL_INTERFACE(Adapter)->FillShortCutTxDescHandler(Adapter, queueIndex, pDescData, ptx_desc);
        }

        SetTxBufferDesc88XX(Adapter, queueIndex, pDescData,SET_TXBD_SOURCE_SHORTCUT);
    }

    
    return _TRUE;
}
#endif // CFG_HAL_TX_SHORTCUT

#endif // (HAL_DEV_BUS_TYPE & (HAL_RT_EMBEDDED_INTERFACE | HAL_RT_PCI_INTERFACE))

#if (IS_EXIST_RTL8822BE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8821CE || IS_EXIST_RTL8198FEM || IS_EXIST_RTL8814BE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE|| IS_EXIST_RTL8197GEM)

HAL_IMEM
u2Byte
GetShortCutTxBuffSize88XX_V1(
    IN  HAL_PADAPTER    Adapter,
    IN  PVOID           pTxDesc
)
{
    PTX_DESC_88XX   ptx_desc = (PTX_DESC_88XX) pTxDesc;
    u2Byte value = 0;
#ifdef SUPPORT_TXDESC_IE
    if(IS_SUPPORT_TXDESC_IE(Adapter))
        value = (u2Byte)GET_TX_DESC_TXDESC_CHECKSUM_V1(ptx_desc);    
#endif        
#if IS_RTL88XX_MAC_V2_V3 || IS_EXIST_RTL8192FE
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3){
        value = (u2Byte)GET_TX_DESC_TIMESTAMP(ptx_desc);
    }
#endif    
    return value;
}

HAL_IMEM
VOID
SetShortCutTxBuffSize88XX_V1(
    IN  HAL_PADAPTER    Adapter,
    IN  PVOID           pTxDesc,
    IN  u2Byte          txBuffSize
)
{
    PTX_DESC_88XX   ptx_desc = (PTX_DESC_88XX) pTxDesc;
#ifdef SUPPORT_TXDESC_IE
    if(IS_SUPPORT_TXDESC_IE(Adapter))
        SET_TX_DESC_TXDESC_CHECKSUM_V1(ptx_desc, txBuffSize);    
#endif 
#if IS_RTL88XX_MAC_V2_V3 || IS_EXIST_RTL8192FE
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3){
        SET_TX_DESC_TXDESC_CHECKSUM(ptx_desc, txBuffSize);  
    }
#endif //IS_RTL88XX_MAC_V1_V2_V3
}

#if CFG_HAL_SUPPORT_TXDESC_IE
static VOID
SetSecType_V2(
    IN  HAL_PADAPTER    Adapter,
    IN  PTX_DESC_88XX   ptx_desc,
    IN  PVOID           pDescData 
)
{

    PTX_DESC_DATA_88XX  pdesc_data  = (PTX_DESC_DATA_88XX)pDescData;
    u1Byte* IE1_head = ((u1Byte*)ptx_desc)+16+8;
    
    switch(pdesc_data->secType) {
    case _WEP_40_PRIVACY_:
    case _WEP_104_PRIVACY_:
    case _TKIP_PRIVACY_:
        IE1_SET_TX_DESC_SECTYPE(IE1_head,TXDESC_SECTYPE_WEP40_OR_TKIP);
        break;
#if CFG_HAL_RTL_HW_WAPI_SUPPORT
    case _WAPI_SMS4_:
        IE1_SET_TX_DESC_SECTYPE(IE1_head,TXDESC_SECTYPE_WAPI);        
        break;
#endif        
    case _CCMP_PRIVACY_:
        IE1_SET_TX_DESC_SECTYPE(IE1_head,TXDESC_SECTYPE_AES);                
        break;
    default:
        break;
    }   
}
#endif
static VOID
SetSecType_V1(
    IN  HAL_PADAPTER    Adapter,
    IN  PTX_DESC_88XX   ptx_desc,
    IN  PVOID           pDescData 
)
{
    PTX_DESC_DATA_88XX  pdesc_data  = (PTX_DESC_DATA_88XX)pDescData;
    
    switch(pdesc_data->secType) {
    case _WEP_40_PRIVACY_:
    case _WEP_104_PRIVACY_:    
#if CFG_HAL_RTL_HW_WAPI_SUPPORT
    case _WAPI_SMS4_:
        SET_TX_DESC_SEC_TYPE(ptx_desc,TXDESC_SECTYPE_WAPI);        
        break;
#endif        
#if IS_RTL88XX_MAC_V2_V3
    case _TKIP_PRIVACY_:
        SET_TX_DESC_SEC_TYPE(ptx_desc,TXDESC_SECTYPE_WEP40_OR_TKIP);
        break;
    case _CCMP_PRIVACY_:
        SET_TX_DESC_SEC_TYPE(ptx_desc,TXDESC_SECTYPE_AES);                
        break;
#endif //IS_RTL88XX_MAC_V2_V3 
    default:
#if 0
        SET_DESC_FIELD_CLR(ptx_desc->Dword1, TXDESC_SECTYPE_NO_ENCRYPTION,
                                        TX_DW1_SECTYPE_MSK, TX_DW1_SECTYPE_SH);        
#endif
        break;
    }   
}

#if IS_RTL88XX_MAC_V2_V3 || IS_EXIST_RTL8192FE
VOID
SetTxDescQSel88XX_V1(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN  PTX_DESC_88XX   ptx_desc,
    IN  u1Byte          drvTID
)
{
    u1Byte  q_select;
    //u4Byte  val=0;
#if CFG_HAL_SUPPORT_TXDESC_IE    
    u1Byte* IE1_head = ptx_desc+16+8;
#endif    
	switch (queueIndex) {
    	case HCI_TX_DMA_QUEUE_HI0:
            
#if CFG_HAL_SUPPORT_TXDESC_IE
            if(IS_SUPPORT_TXDESC_IE(Adapter))
                IE1_SET_TX_DESC_MOREDATA_NO_CLR(IE1_head,1);
            else
#endif
            SET_TX_DESC_MOREDATA_NO_CLR(ptx_desc,1);
            //SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, 1, TX_DW1_MOREDATA_MSK, TX_DW1_MOREDATA_SH);
            //Set MACIDMask to zero, but we have memset before
    		q_select = TXDESC_QSEL_HIGH;
    		break;
#if  CFG_HAL_SUPPORT_MBSSID
    	case HCI_TX_DMA_QUEUE_HI1:
#if CFG_HAL_SUPPORT_TXDESC_IE
            if(IS_SUPPORT_TXDESC_IE(Adapter))
                IE1_SET_TX_DESC_MOREDATA_NO_CLR(IE1_head,1);
            else
#endif
            SET_TX_DESC_MOREDATA_NO_CLR(ptx_desc,1);
            //SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, 1, TX_DW1_MOREDATA_MSK, TX_DW1_MOREDATA_SH);
            //SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, 1, TX_DW1_MACID_MSK, TX_DW1_MACID_SH);
            q_select = TXDESC_QSEL_HIGH;
    		break;
    	case HCI_TX_DMA_QUEUE_HI2:
#if CFG_HAL_SUPPORT_TXDESC_IE
            if(IS_SUPPORT_TXDESC_IE(Adapter))
                IE1_SET_TX_DESC_MOREDATA_NO_CLR(IE1_head,1);
            else
#endif            
            SET_TX_DESC_MOREDATA_NO_CLR(ptx_desc,1);
            //SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, 1, TX_DW1_MOREDATA_MSK, TX_DW1_MOREDATA_SH);
            //SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, 2, TX_DW1_MACID_MSK, TX_DW1_MACID_SH);
            q_select = TXDESC_QSEL_HIGH;
    		break;
    	case HCI_TX_DMA_QUEUE_HI3:	
#if CFG_HAL_SUPPORT_TXDESC_IE
            if(IS_SUPPORT_TXDESC_IE(Adapter))
                IE1_SET_TX_DESC_MOREDATA_NO_CLR(IE1_head,1);
            else
#endif            
            SET_TX_DESC_MOREDATA_NO_CLR(ptx_desc,1);            
            //SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, 1, TX_DW1_MOREDATA_MSK, TX_DW1_MOREDATA_SH);
            //SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, 3, TX_DW1_MACID_MSK, TX_DW1_MACID_SH);
            q_select = TXDESC_QSEL_HIGH;
    		break;
    	case HCI_TX_DMA_QUEUE_HI4:
#if CFG_HAL_SUPPORT_TXDESC_IE
            if(IS_SUPPORT_TXDESC_IE(Adapter))
                IE1_SET_TX_DESC_MOREDATA_NO_CLR(IE1_head,1);
            else
#endif            
            SET_TX_DESC_MOREDATA_NO_CLR(ptx_desc,1);               
            //SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, 1, TX_DW1_MOREDATA_MSK, TX_DW1_MOREDATA_SH);
            //SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, 4, TX_DW1_MACID_MSK, TX_DW1_MACID_SH);
            q_select = TXDESC_QSEL_HIGH;
    		break;
    	case HCI_TX_DMA_QUEUE_HI5:
#if CFG_HAL_SUPPORT_TXDESC_IE
            if(IS_SUPPORT_TXDESC_IE(Adapter))
                IE1_SET_TX_DESC_MOREDATA_NO_CLR(IE1_head,1);
            else
#endif            
            SET_TX_DESC_MOREDATA_NO_CLR(ptx_desc,1);               
            //SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, 1, TX_DW1_MOREDATA_MSK, TX_DW1_MOREDATA_SH);
            //SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, 5, TX_DW1_MACID_MSK, TX_DW1_MACID_SH);
            q_select = TXDESC_QSEL_HIGH;
    		break;
    	case HCI_TX_DMA_QUEUE_HI6:
#if CFG_HAL_SUPPORT_TXDESC_IE
            if(IS_SUPPORT_TXDESC_IE(Adapter))
                IE1_SET_TX_DESC_MOREDATA_NO_CLR(IE1_head,1);
            else
#endif            
            SET_TX_DESC_MOREDATA_NO_CLR(ptx_desc,1);               
            //SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, 1, TX_DW1_MOREDATA_MSK, TX_DW1_MOREDATA_SH);
            //SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, 6, TX_DW1_MACID_MSK, TX_DW1_MACID_SH);
            q_select = TXDESC_QSEL_HIGH;
    		break;
    	case HCI_TX_DMA_QUEUE_HI7:
#if CFG_HAL_SUPPORT_TXDESC_IE
            if(IS_SUPPORT_TXDESC_IE(Adapter))
                IE1_SET_TX_DESC_MOREDATA_NO_CLR(IE1_head,1);
            else
#endif            
            SET_TX_DESC_MOREDATA_NO_CLR(ptx_desc,1);   
            //SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, 1, TX_DW1_MOREDATA_MSK, TX_DW1_MOREDATA_SH);
            //SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, 7, TX_DW1_MACID_MSK, TX_DW1_MACID_SH);
            q_select = TXDESC_QSEL_HIGH;
    		break;
#endif  //CFG_HAL_SUPPORT_MBSSID
        case HCI_TX_DMA_QUEUE_CMD:
            q_select = TXDESC_QSEL_CMD;
            break;     
    	case HCI_TX_DMA_QUEUE_MGT:
#if 0 //eric-8822 ?? TX HANG
			//SET_TX_DESC_MOREDATA_NO_CLR(ptx_desc,1);  
			q_select = TXDESC_QSEL_HIGH;
#else
    		q_select = TXDESC_QSEL_MGT;
#endif
    		break;
            
#if CFG_HAL_MAC_LOOPBACK && CFG_HAL_WIFI_WMM
    	case HCI_TX_DMA_QUEUE_BE:
    		q_select = TXDESC_QSEL_TID0;
    		break;
        case HCI_TX_DMA_QUEUE_VO:
			q_select = TXDESC_QSEL_TID6;
			break;
		case HCI_TX_DMA_QUEUE_VI:
			q_select = TXDESC_QSEL_TID4;
    		break;
    	case HCI_TX_DMA_QUEUE_BK:
			q_select = TXDESC_QSEL_TID1;
			break;            
#endif  //CFG_HAL_MAC_LOOPBACK && CFG_HAL_WIFI_WMM

    	default:

    		// data packet
#if CFG_HAL_RTL_MANUAL_EDCA
    		if (HAL_VAR_MANUAL_EDCA) {
    			switch (queueIndex) {
        			case HCI_TX_DMA_QUEUE_VO:
        				q_select = TXDESC_QSEL_TID6;
        				break;
        			case HCI_TX_DMA_QUEUE_VI:
        				q_select = TXDESC_QSEL_TID4;
                		break;
        			case HCI_TX_DMA_QUEUE_BE:
        				q_select = TXDESC_QSEL_TID0;
        	    		break;
        		    default:
        				q_select = TXDESC_QSEL_TID1;
        				break;
    			}
    		}
    		else {
                q_select = drvTID;
    		}
#else
            q_select = drvTID;
#endif  //CFG_HAL_RTL_MANUAL_EDCA
            break;
	}    
    SET_TX_DESC_QSEL_NO_CLR(ptx_desc,q_select);   
    //SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, q_select, TX_DW1_QSEL_MSK, TX_DW1_QSEL_SH);
    //ptx_desc->Dword1 |= val;
}
#endif //#if IS_RTL88XX_MAC_V1_V2_V3

#if IS_EXIST_RTL8822BE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE|| IS_EXIST_RTL8197GEM 
VOID
FillBeaconDesc88XX_V1
(
    IN	HAL_PADAPTER        Adapter,
    IN  PVOID               _pdesc,
    IN  PVOID               data_content,
    IN  u2Byte              txLength,
    IN  BOOLEAN             bForceUpdate
)
{
    u4Byte  TXDESCSize;
    PTX_DESC_88XX       pdesc = (PTX_DESC_88XX)_pdesc;

#if (IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM)
    if ( IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) ) {
        TXDESCSize = SIZE_TXDESC_88XX;
    }
#endif  //IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM

#if (IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8822CE  || IS_EXIST_RTL8812FE|| IS_EXIST_RTL8197GEM)
    if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter))  {
        TXDESCSize = SIZE_TXDESC_88XX_V1;
    }
#endif //IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8197GEM

    platform_zero_memory(pdesc, TXDESCSize);

    //Dword0
    SET_TX_DESC_BMC(pdesc, 1);
    SET_TX_DESC_OFFSET(pdesc, TXDESCSize);
    SET_TX_DESC_TXPKTSIZE(pdesc, txLength);

    //Dword1
    SET_TX_DESC_QSEL(pdesc,TXDESC_QSEL_BCN);  


#if CFG_HAL_SUPPORT_MBSSID
        if (HAL_IS_VAP_INTERFACE(Adapter)) {
    
        // set MBSSID for each VAP_ID
        SET_TX_DESC_MACID(pdesc, HAL_VAR_VAP_INIT_SEQ);
        SET_TX_DESC_MBSSID(pdesc, HAL_VAR_VAP_INIT_SEQ);        
   
        }         
#endif //#if CFG_HAL_SUPPORT_MBSSID

    SET_TX_DESC_SW_SEQ(pdesc,GetSequence(data_content));
    SET_TX_DESC_DISDATAFB(pdesc,1);
    SET_TX_DESC_USE_RATE(pdesc,1);


/*cy wang cfg p2p rm*/
    if (HAL_VAR_IS_40M_BW == 1) {
        if (HAL_VAR_OFFSET_2ND_CHANNEL == HT_2NDCH_OFFSET_BELOW) {
            SET_TX_DESC_DATA_SC(pdesc,TXDESC_DATASC_LOWER);
        }
		else {
            SET_TX_DESC_DATA_SC(pdesc,TXDESC_DATASC_UPPER);
		}
	}

    //Dword4    /*cy wang cfg p2p , 6m rate beacon*//*cy wang cfg p2p*/
    #ifdef P2P_SUPPORT	// 2014-0328 use 6m rate send beacon
    if(Adapter->pmib->p2p_mib.p2p_enabled){          
        SET_TX_DESC_RTSRATE(pdesc,4);
        SET_TX_DESC_DATARATE(pdesc,4);
    }    
    #endif   
/*cy wang cfg p2p*/
/*
		 * Intel IOT, dynamic enhance beacon tx AGC
*/

	if (Adapter->pmib->dot11StationConfigEntry.beacon_rate != 0xff)
        SET_TX_DESC_DATARATE(pdesc,Adapter->pmib->dot11StationConfigEntry.beacon_rate);

#if IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE
    if(IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)) {
    	SET_TX_DESC_RTSRATE(pdesc,4);
    	SET_TX_DESC_DATARATE(pdesc,4);
    }
#endif

#if (IS_EXIST_RTL8822BE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM || IS_EXIST_RTL8821CE)
    if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter)  || IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter)) 
	{
	if (Adapter->bcnTxAGC ==1) {
        SET_TX_DESC_TXPWR_OFSET(pdesc,4); // +3dB
	} else if (Adapter->bcnTxAGC ==2) {
        SET_TX_DESC_TXPWR_OFSET(pdesc,5); // +6dB	
	} else {
        SET_TX_DESC_TXPWR_OFSET(pdesc,0);
	}

	if (Adapter->pmib->dot11RFEntry.txpwr_reduction) {
		if (Adapter->pmib->dot11RFEntry.txpwr_reduction <= 3)
            SET_TX_DESC_TXPWR_OFSET(pdesc,Adapter->pmib->dot11RFEntry.txpwr_reduction); 
	}
	}
#endif 

#if (IS_EXIST_RTL8822BE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE)
    if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter))  {
        // TODO : 92E is not in this function
	if (Adapter->pmib->dot11RFEntry.bcn2path && IS_HARDWARE_TYPE_8192EE(Adapter) )
        SET_TX_DESC_TX_ANT(pdesc,3);
    }
#endif //(IS_EXIST_RTL8822BE || IS_EXIST_RTL8197FEM)

    // TODO: Why ?
    HAL_VAR_IS_40M_BW_BAK   = HAL_VAR_IS_40M_BW;
    HAL_VAR_TX_BEACON_LEN   = txLength;

    // no use ?
    //SET_DESC_FIELD_CLR(pdesc->Dword7, txLength, TX_DW7_SW_TXBUFF_MSK, TX_DW7_SW_TXBUFF_SH);


#if 0   // TODO: Filen: test code ?
#if (defined(UNIVERSAL_REPEATER) || defined(MBSSID))
    if (IS_ROOT_INTERFACE(Adapter)) {
		if (Adapter->pshare->rf_ft_var.swq_dbg	== 30) {
			pdesc->Dword9 |= set_desc((1122 & TXdesc_92E_TX_SeqMask)  << TXdesc_92E_TX_SeqSHIFT);
		}
		else {
			pdesc->Dword9 |= set_desc((5566 & TXdesc_92E_TX_SeqMask)  << TXdesc_92E_TX_SeqSHIFT);
		}
    }
    else {
        pdesc->Dword9 |= set_desc((GetSequence(data_content) & TXdesc_92E_TX_SeqMask)  << TXdesc_92E_TX_SeqSHIFT);        
    }
#else
    if (Adapter->pshare->rf_ft_var.swq_dbg == 30) {
        pdesc->Dword9 |= set_desc((1122 & TXdesc_92E_TX_SeqMask)  << TXdesc_92E_TX_SeqSHIFT);
    }
    else {
        pdesc->Dword9 |= set_desc((5566 & TXdesc_92E_TX_SeqMask)  << TXdesc_92E_TX_SeqSHIFT);
    }
#endif  //(defined(UNIVERSAL_REPEATER) || defined(MBSSID))
#endif

    // Group Bit Control
    SET_TX_DESC_GROUP_BIT_IE_OFFSET(pdesc,(HAL_VAR_TIM_OFFSET-24));
    
    // Auto set bitmap control by HW, no present in TXDESC document so reserved this code
    if (HAL_OPMODE & WIFI_ADHOC_STATE) {
        SET_DESC_FIELD_CLR(pdesc->Dword9, 0, TX_DW9_GROUPBIT_IE_ENABLE_MSK, TX_DW9_GROUPBIT_IE_ENABLE_SH);
    } else {
        SET_DESC_FIELD_CLR(pdesc->Dword9, 1, TX_DW9_GROUPBIT_IE_ENABLE_MSK, TX_DW9_GROUPBIT_IE_ENABLE_SH);
    }
    // TODO: Check with Button


#ifdef TRXBD_CACHABLE_REGION
#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
    // With CCI400 ,Do nothing for cache coherent code
#else
    rtl_cache_sync_wback(Adapter,(unsigned long)((PVOID)(pdesc)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), TXDESCSize,PCI_DMA_TODEVICE);
#endif //CONFIG_ENABLE_CCI400
#else
    _dma_cache_wback((unsigned long)((PVOID)(pdesc)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), TXDESCSize);
#endif //CONFIG_RTL_8198F
#endif //#ifdef TRXBD_CACHABLE_REGION
}


// 8197F, 8822B , 8198F used 
VOID
FillTxDesc88XX_V1 (
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN  PVOID           pDescData
)
{
    PHCI_TX_DMA_MANAGER_88XX        ptx_dma;
    PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q;
    PTX_DESC_88XX                   ptx_desc;
    PTX_DESC_DATA_88XX              pdesc_data = (PTX_DESC_DATA_88XX)pDescData;
    u4Byte                          val;
    u4Byte                          tmp,tmpCache;
    u4Byte                          TXDESCSize;
    u2Byte                          *tx_head;
    struct tx_desc_info *pswdescinfo, *pdescinfo;
    struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)Adapter;

#if CFG_HAL_HW_SEQ
    u1Byte                          max_queueidx;
#endif

#if (IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM)
        if ( IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) ) {
            TXDESCSize = SIZE_TXDESC_88XX;
        }
#endif  //IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM)
    
#if (IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE|| IS_EXIST_RTL8197GEM)
        if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter))  {
            TXDESCSize = SIZE_TXDESC_88XX_V1;
        }
#endif //IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8197GEM

    //Dword 0
    u2Byte  TX_DESC_TXPKTSIZE        = pdesc_data->hdrLen + pdesc_data->llcLen + pdesc_data->frLen;
    u1Byte  TX_DESC_OFFSET           = TXDESCSize;

    BOOLEAN TX_DESC_BMC              = (HAL_IS_MCAST(GetAddr1Ptr((pu1Byte)pdesc_data->pHdr))) ? 1 : 0;   // when multicast or broadcast, BMC = 1        

//    BOOLEAN TX_DESC_HT               = 0;
//    BOOLEAN TX_DESC_LINIP            = 0;
//    BOOLEAN TX_DESC_NOACM            = 0;
//    BOOLEAN TX_DESC_GF               = 0;

    //Dword 1 
    u1Byte  TX_DESC_MACID            = pdesc_data->macId; // MACID/MBSSID ?
    u1Byte  TX_DESC_RATE_ID          = pdesc_data->rateId;
    BOOLEAN TX_DESC_MORE_DATA        = pdesc_data->moreData;    
    BOOLEAN TX_DESC_EN_DESC_ID       = pdesc_data->enDescId;
    
    //Dword 2
    BOOLEAN TX_DESC_AGG_EN           = pdesc_data->aggEn; 
    BOOLEAN TX_DESC_BK               = pdesc_data->bk;
    BOOLEAN TX_DESC_MOREFRAG         = pdesc_data->frag;
    u1Byte  TX_DESC_AMPDU_DENSITY    = pdesc_data->ampduDensity;
    u4Byte  TX_DESC_P_AID      		 = pdesc_data->p_aid;
    u1Byte  TX_DESC_G_ID      		 = pdesc_data->g_id;
#if CFG_HAL_HW_AES_IV
    BOOLEAN TX_DESC_HW_AES_IV        = pdesc_data->hwAESIv;
#endif // CFG_HAL_HW_AES_IV

#if 1 //eric-ac2
	u1Byte	TX_DESC_CCA_RTS 		 = pdesc_data->cca_rts;
#endif


    //Dword 3
    BOOLEAN TX_DESC_USERATE          = pdesc_data->useRate;    
    BOOLEAN TX_DESC_DISRTSFB         = pdesc_data->disRTSFB;
    BOOLEAN TX_DESC_DISDATAFB        = pdesc_data->disDataFB;
    BOOLEAN TX_DESC_CTS2SELF         = pdesc_data->CTS2Self;
    BOOLEAN TX_DESC_RTS_EN           = pdesc_data->RTSEn;
    BOOLEAN TX_DESC_HW_RTS_EN        = pdesc_data->HWRTSEn;
    BOOLEAN TX_DESC_NAVUSEHDR        = pdesc_data->navUseHdr;
    u1Byte  TX_DESC_MAX_AGG_NUM      = pdesc_data->maxAggNum;
    BOOLEAN TX_DESC_NDPA		     = pdesc_data->ndpa;

    //Dword 4
    u1Byte  TX_DESC_DATERATE         = pdesc_data->dataRate;
    u1Byte  TX_DESC_DATA_RATEFB_LMT  = pdesc_data->dataRateFBLmt;
    u1Byte  TX_DESC_RTS_RATEFB_LMT   = pdesc_data->RTSRateFBLmt;
    BOOLEAN TX_DESC_RTY_LMT_EN       = pdesc_data->rtyLmtEn;   
    u1Byte  TX_DESC_DATA_RT_LMT      = pdesc_data->dataRtyLmt;
    u1Byte  TX_DESC_RTSRATE          = pdesc_data->RTSRate;
    u1Byte  TX_DESC_BMCRtyLmt        = pdesc_data->BMCRtyLmt;

    //Dword 5
    u1Byte  TX_DESC_DATA_SC          = pdesc_data->dataSC;
    u1Byte  TX_DESC_DATA_SHORT       = pdesc_data->dataShort;
    u1Byte  TX_DESC_DATA_BW          = pdesc_data->dataBW;
    u1Byte  TX_DESC_DATA_STBC        = pdesc_data->dataStbc;
	u1Byte  TX_DESC_DATA_LDPC        = pdesc_data->dataLdpc;
    u1Byte  TX_DESC_RTS_SHORT        = pdesc_data->RTSShort;
    u1Byte  TX_DESC_RTS_SC           = pdesc_data->RTSSC;   
    u1Byte	TX_DESC_POWER_OFFSET	 = pdesc_data->TXPowerOffset;
    u1Byte	TX_ANT					 = pdesc_data->TXAnt;	
#if defined(AP_SWPS_OFFLOAD)
    u1Byte  TX_DESC_DROP_ID          = pdesc_data->DropID;
#endif    
    //Dword6
#if defined(AP_SWPS_OFFLOAD)
    u2Byte  TX_DESC_SWPS_SEQ         = pdesc_data->SWPS_sequence;
#endif

    //Dword 7
    // use for CFG_HAL_TX_SHORTCUT
    u2Byte  TX_DESC_TXBUFF           = pdesc_data->frLen;

	// Dword 8
	BOOLEAN	TX_DESC_TXWIFI_CP;
    BOOLEAN	TX_DESC_STW_EN;
    BOOLEAN TX_DESC_MAC_CP;
    BOOLEAN TX_DESC_SMH_EN;
    u1Byte  TX_DESC_WHEADER_LEN;
    u1Byte  TX_DESC_PKT_OFFSET;
    u1Byte  TX_DESC_SHCUT_CAM;



#if IS_EXIST_RTL8197GEM && (CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC || CFG_HAL_HW_TX_SHORTCUT_HDR_CONV || CFG_HAL_HW_SEQ)
    if(IS_HARDWARE_TYPE_8197G(Adapter)){
        if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC(Adapter) || IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(Adapter) || IS_SUPPORT_HW_SEQ(Adapter)){
            #if defined(TXSC_MULTI_ENTRY_VERI) || defined(NEW_HWSEQ_VERI)
            pdesc_data->shcut_cam = priv->cam_idx;
            #endif
            
            TX_DESC_SHCUT_CAM = pdesc_data->shcut_cam;
        }
    }
#endif


#if CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC
    if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC(Adapter)) {
        #ifdef TXSC_MULTI_ENTRY_VERI
        if(priv->store_TXDESC)
            pdesc_data->txwifiCp = 1;
        else
            pdesc_data->txwifiCp = 0;
        if(priv->use_TXDESC)
            pdesc_data->stwEn = 1;
        else
            pdesc_data->stwEn = 0;
        
        TX_DESC_STW_EN          = pdesc_data->stwEn;
        #endif
	    TX_DESC_TXWIFI_CP		= pdesc_data->txwifiCp;
    }
#endif // CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
    if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(Adapter)) {
       	TX_DESC_MAC_CP			= pdesc_data->macCp;
    	TX_DESC_SMH_EN			= pdesc_data->smhEn;
        TX_DESC_OFFSET			 = ((pdesc_data->smhEn == TRUE) ? (HAL_HW_TXSC_HDR_CONV_ADD_OFFSET+TXDESCSize) : TXDESCSize);
        TX_DESC_BMC              = (pdesc_data->smhEn == TRUE) ? 
    					((HAL_IS_MCAST(GetEthDAPtr((pu1Byte)pdesc_data->pHdr))) ? 1 : 0) : 
				        ((HAL_IS_MCAST(GetAddr1Ptr((pu1Byte)pdesc_data->pHdr))) ? 1 : 0);   // when multicast or broadcast, BMC = 1        
        TX_DESC_PKT_OFFSET       = 0; // unit: 8 bytes. Early mode: 1 units, (8 * 1 = 8 bytes for early mode info)				        

        if(IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV_LLC(Adapter)) {
            TX_DESC_WHEADER_LEN      = ((pdesc_data->smhEn == TRUE) ? (HAL_ETH_HEADER_LEN_MAX >> 1) : ((pdesc_data->hdrLen) >> 1)); // unit: 2 bytes
        }else {
            TX_DESC_WHEADER_LEN      = ((pdesc_data->smhEn == TRUE) ? (HAL_ETH_HEADER_LEN_MAX >> 1) : ((pdesc_data->hdrLen+pdesc_data->llcLen+pdesc_data->iv) >> 1)); // unit: 2 bytes
        }

    }

#endif // CFG_HAL_HW_TX_SHORTCUT_HDR_CONV

#ifdef TXSC_MULTI_ENTRY_VERI
            if(priv->store_HDR)
                pdesc_data->macCp = 1;
            else
                pdesc_data->macCp = 0;
            if(priv->use_HDR)
                pdesc_data->smhEn = 1;
            else
                pdesc_data->smhEn = 0;
            
            TX_DESC_MAC_CP          = pdesc_data->macCp;
            TX_DESC_SMH_EN          = pdesc_data->smhEn;
            TX_DESC_PKT_OFFSET       = 0; 
    
            TX_DESC_WHEADER_LEN      = (pdesc_data->hdrLen) >> 1; // unit: 2 bytes
#endif


    //Dword 9
    u2Byte TX_DESC_SEQ               = GetSequence(pdesc_data->pHdr);
#ifdef AP_SWPS_OFFLOAD_WITH_AMPDU_VERI
    if(pdesc_data->macId ==1 || pdesc_data->macId==2) 
        printk("pdesc_data->macId=%x, TX_DESC_SEQ = %d \n",pdesc_data->macId,TX_DESC_SEQ);
#endif

    //printk("TX_DESC_SEQ = %d \n",TX_DESC_SEQ);
#if 0//CFG_HAL_HW_SEQ
    if(IS_SUPPORT_HW_SEQ(Adapter)) {
        //TX_DESC_SEQ               = 0;
    }
#endif // CFG_HAL_HW_SEQ
    
#if 1 //eric-8822 add SND_pkt_sel
	//Dword 10
	u1Byte SND_PKT_SEL               = pdesc_data->SND_pkt_sel;
	u1Byte IS_GID               	 = pdesc_data->is_GID;
#endif

#if 0 //(CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC||CFG_HAL_HW_TX_SHORTCUT_HDR_CONV)
	if (pdesc_data->smhEn != 0 || pdesc_data->macCp != 0)
		printk("%s(%d): macCp:0x%x, smhEn:0x%x \n", __FUNCTION__, __LINE__, pdesc_data->macCp, pdesc_data->smhEn);

	if (pdesc_data->txwifiCp != 0)
		printk("%s(%d): txwifiCp:0x%x \n", __FUNCTION__, __LINE__, pdesc_data->txwifiCp);
#endif

#if 0 // show TX Rpt Info
    {
        int baseReg = 0x8080, offset, i;
        int lenTXDESC = 10, lenHdrInfo = 20;
    
        if (pdesc_data->smhEn != 0) {
            HAL_RTL_W8(0x106, 0x7F);
            HAL_RTL_W32(0x140, 0x662);
        
            for(i = 0; i < lenTXDESC; i++) {
                printk("%08X ", (u4Byte)GET_DESC(HAL_RTL_R32(baseReg)));
                if (i%4==3)
                    printk("\n");
                baseReg += 4;
            }
            printk("\n");
        
            for(i = 0; i < lenHdrInfo; i++) {
                printk("%08X ", (u4Byte)GET_DESC(HAL_RTL_R32(baseReg)));
                if (i%4==3)
                    printk("\n");
                baseReg += 4;
            }
            printk("\n");
        }
    }
#endif

	if(Adapter->pshare->rf_ft_var.manual_ampdu)
		TX_DESC_MAX_AGG_NUM = 0;

#if CFG_HAL_MULTICAST_BMC_ENHANCE
    if(TX_DESC_BMC)
    {
        TX_DESC_BMCRtyLmt = pdesc_data->BMCRtyLmt;
    }
#endif


    ptx_dma     = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);
    cur_q       = &(ptx_dma->tx_queue[queueIndex]);
    ptx_desc    = ((PTX_DESC_88XX)(cur_q->ptx_desc_head)) + cur_q->host_idx;

    //Clear All Bit
    platform_zero_memory((PVOID)ptx_desc, sizeof(TX_DESC_88XX));

    if (pdesc_data->secType != _NO_PRIVACY_) {
        if (pdesc_data->swCrypt == FALSE) {
            SetSecType_V1(Adapter, ptx_desc, pdesc_data);
            // for hw sec: 1) WEP's iv, 2) TKIP's iv and eiv, 3) CCMP's ccmp header are all in pdesc_data->iv
            TX_DESC_TXPKTSIZE += pdesc_data->iv;
        } else {
            // for sw sec
            TX_DESC_TXPKTSIZE += (pdesc_data->iv + pdesc_data->icv + pdesc_data->mic);
        }
    }

    //4 Set Dword0
    SET_TX_DESC_TXPKTSIZE_NO_CLR(ptx_desc, TX_DESC_TXPKTSIZE);
    SET_TX_DESC_OFFSET_NO_CLR(ptx_desc, TX_DESC_OFFSET);
    SET_TX_DESC_BMC_NO_CLR(ptx_desc, TX_DESC_BMC);

    //4 Set Dword1
#if 1 /*eric-8822 ?? tx hang*/
	if (TX_DESC_NDPA) {
		//printk("[%s] tid = %d, TX_DESC_P_AID = 0x%x SND_PKT_SEL = 0x%x \n", __FUNCTION__, 
			//pdesc_data->tid, TX_DESC_P_AID, SND_PKT_SEL);

		TX_DESC_RTY_LMT_EN = 1; 
		TX_DESC_DATA_RT_LMT = 4;
		
		SET_TX_DESC_QSEL_NO_CLR(ptx_desc, TXDESC_QSEL_MGT);  //eric-mu 
	}
	else
#endif
    {
#if IS_RTL88XX_MAC_V1_V2
    if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2) {   
    SetTxDescQSel88XX_V1(Adapter, queueIndex, ptx_desc, pdesc_data->tid);        
        if ( (queueIndex >= HCI_TX_DMA_QUEUE_HI0) && (queueIndex <= HCI_TX_DMA_QUEUE_HI7) ) {
        #ifdef HW_ENC_FOR_GROUP_CIPHER
            SET_TX_DESC_MACID_NO_CLR(ptx_desc, TX_DESC_MACID);
        #endif
        } else {
            SET_TX_DESC_MACID_NO_CLR(ptx_desc, TX_DESC_MACID);
        }
    }
#endif // IS_RTL88XX_MAC_V1_V2

#if IS_RTL88XX_MAC_V3
    if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v3) {   
        SetTxDescQSel88XX_V2(Adapter, queueIndex, ptx_desc, pdesc_data->tid);        

        if ( (queueIndex >= HCI_TX_DMA_QUEUE_HI0_V1) && (queueIndex <= HCI_TX_DMA_QUEUE_HI15) ) {
#ifdef HW_ENC_FOR_GROUP_CIPHER
        SET_TX_DESC_MACID_NO_CLR(ptx_desc, TX_DESC_MACID);
#endif
    } else {
        SET_TX_DESC_MACID_NO_CLR(ptx_desc, TX_DESC_MACID);
    }
    }
#endif // IS_RTL8198F_SERIES
    }
  
    SET_TX_DESC_RATE_ID_NO_CLR(ptx_desc, TX_DESC_RATE_ID);
    SET_TX_DESC_MOREDATA_NO_CLR(ptx_desc, TX_DESC_MORE_DATA);    
    if (TX_DESC_EN_DESC_ID)
        SET_TX_DESC_EN_DESC_ID_NO_CLR(ptx_desc, 1);        

	
#if (IS_EXIST_RTL8822BE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE)
	if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter))
		TX_DESC_BK = 0;
#endif //IS_EXIST_RTL8822BE

    //4 Set Dword2
    SET_TX_DESC_AGG_EN_NO_CLR(ptx_desc, TX_DESC_AGG_EN);
    SET_TX_DESC_BK_NO_CLR(ptx_desc, TX_DESC_BK);
    SET_TX_DESC_MOREFRAG_NO_CLR(ptx_desc, TX_DESC_MOREFRAG);
    SET_TX_DESC_AMPDU_DENSITY_NO_CLR(ptx_desc, TX_DESC_AMPDU_DENSITY);
#ifdef VERI_CSI_REPORT
    if (TX_DESC_NDPA){
        TX_DESC_P_AID = 2;
        printk("%s tx ndpa pkt\n",__FUNCTION__);
    }
#endif    
    SET_TX_DESC_P_AID_NO_CLR(ptx_desc, TX_DESC_P_AID);
#if (IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822BE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE) //98F doesn't have this field
	if ( IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter))
    SET_TX_DESC_G_ID_NO_CLR(ptx_desc, TX_DESC_G_ID);

#ifdef VERI_CSI_REPORT
    if (TX_DESC_NDPA){
        TX_DESC_G_ID = 63;
        SET_TX_DESC_G_ID_NO_CLR(ptx_desc, TX_DESC_G_ID);
    }
#endif    

#endif

#if CFG_HAL_HW_AES_IV
    SET_TX_DESC_HW_AES_IV_NO_CLR(ptx_desc, TX_DESC_HW_AES_IV);
#endif // CFG_HAL_HW_AES_IV

	
#if 1 //eric-ac2
	SET_TX_DESC_CCA_RTS_NO_CLR(ptx_desc, TX_DESC_CCA_RTS);

	if(IS_GID)
	SET_TX_DESC_SPE_RPT_NO_CLR(ptx_desc, 1);
#endif


    //4 Set Dword3
    SET_TX_DESC_USE_RATE_NO_CLR(ptx_desc, TX_DESC_USERATE);
    SET_TX_DESC_DISRTSFB_NO_CLR(ptx_desc, TX_DESC_DISRTSFB);
    SET_TX_DESC_DISDATAFB_NO_CLR(ptx_desc, TX_DESC_DISDATAFB);

    SET_TX_DESC_CTS2SELF_NO_CLR(ptx_desc, TX_DESC_CTS2SELF);
    SET_TX_DESC_RTSEN_NO_CLR(ptx_desc, TX_DESC_RTS_EN);

    SET_TX_DESC_HW_RTS_EN_NO_CLR(ptx_desc, TX_DESC_HW_RTS_EN);
    SET_TX_DESC_NAVUSEHDR_NO_CLR(ptx_desc, TX_DESC_NAVUSEHDR);
    SET_TX_DESC_MAX_AGG_NUM_NO_CLR(ptx_desc, TX_DESC_MAX_AGG_NUM);
    SET_TX_DESC_NDPA_NO_CLR(ptx_desc, TX_DESC_NDPA);    
   

    //4 Set Dword4
    SET_TX_DESC_DATARATE_NO_CLR(ptx_desc, TX_DESC_DATERATE);    

    SET_TX_DESC_DATA_RTY_LOWEST_RATE_NO_CLR(ptx_desc, TX_DESC_DATA_RATEFB_LMT);    
    SET_TX_DESC_RTS_RTY_LOWEST_RATE_NO_CLR(ptx_desc, TX_DESC_RTS_RATEFB_LMT);        
    
#if 1 //eric-8822 tx hang
	//if(GET_TX_DESC_QSEL(ptx_desc) == TXDESC_QSEL_MGT)
	if(TX_DESC_BMC)
	SET_TX_DESC_DATA_RTY_LOWEST_RATE(ptx_desc, 0);
#else
#if CFG_HAL_MULTICAST_BMC_ENHANCE
        if(TX_DESC_BMC) {
            SET_TX_DESC_DATA_RTY_LOWEST_RATE(ptx_desc, TX_DESC_BMCRtyLmt);
        }
#endif //#if CFG_HAL_MULTICAST_BMC_ENHANCE        
#endif

#if (IS_EXIST_RTL8822BE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE)
	if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter))  {
		if(TX_DESC_RTY_LMT_EN && (!TX_DESC_NDPA)){
			if(TX_DESC_DATA_RT_LMT <= Adapter->pshare->rf_ft_var.mu_retry)
			TX_DESC_DATA_RT_LMT = (Adapter->pshare->rf_ft_var.mu_retry + 1);
		}
	}
#endif //IS_EXIST_RTL8822BE

    SET_TX_DESC_RTY_LMT_EN_NO_CLR(ptx_desc, TX_DESC_RTY_LMT_EN);    
    SET_TX_DESC_RTS_DATA_RTY_LMT_NO_CLR(ptx_desc, TX_DESC_DATA_RT_LMT);    
    SET_TX_DESC_RTSRATE_NO_CLR(ptx_desc, TX_DESC_RTSRATE);    


#if 1 //eric-8822 ?? No retry for broadcast
	if(TX_DESC_BMC){
		SET_TX_DESC_RTY_LMT_EN(ptx_desc, 1);    
        	SET_TX_DESC_RTS_DATA_RTY_LMT(ptx_desc, 0);  
	}
#endif


    //4 Set Dword5

#if 0
	if(SND_PKT_SEL == 1) {
    SET_TX_DESC_DATA_SC_NO_CLR(ptx_desc, 9);    
    SET_TX_DESC_DATA_SHORT_NO_CLR(ptx_desc, TX_DESC_DATA_SHORT);    
    SET_TX_DESC_DATA_BW_NO_CLR(ptx_desc, 1); 
	}
	else
#endif
{
    SET_TX_DESC_DATA_SC_NO_CLR(ptx_desc, TX_DESC_DATA_SC);    
    SET_TX_DESC_DATA_SHORT_NO_CLR(ptx_desc, TX_DESC_DATA_SHORT);    
    SET_TX_DESC_DATA_BW_NO_CLR(ptx_desc, TX_DESC_DATA_BW);  
}
    SET_TX_DESC_DATA_STBC_NO_CLR(ptx_desc, TX_DESC_DATA_STBC);     
    SET_TX_DESC_DATA_LDPC_NO_CLR(ptx_desc, TX_DESC_DATA_LDPC); 
    SET_TX_DESC_RTS_SHORT_NO_CLR(ptx_desc, TX_DESC_RTS_SHORT); 

#if (IS_RTL8814A_SERIES || IS_RTL8197F_SERIES || IS_RTL8198F_SERIES || IS_RTL8192F_SERIES)
    if ( IS_HARDWARE_TYPE_8814A(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter)  || IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8192F(Adapter))
    SET_TX_DESC_RTS_SC_NO_CLR(ptx_desc, TX_DESC_RTS_SC); 
#endif

#if (IS_EXIST_RTL8822BE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM || IS_EXIST_RTL8821CE)
    if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter)  || IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter)) 
    SET_TX_DESC_TXPWR_OFSET_NO_CLR(ptx_desc, TX_DESC_POWER_OFFSET); 
#endif

#if IS_EXIST_RTL8197GEM
    if(IS_HARDWARE_TYPE_8197G(Adapter)){///TX_DESC_MACID
		u1Byte	TX_DESC_MAPC=0,TX_DESC_MAPD=0;
        if(HAL_RTL_R32(0x1D90) & BIT(15)){
			if(TX_DESC_MACID<256){
				TX_DESC_MAPC = priv->pmib->dot11RFEntry.dbf_rotation[TX_DESC_MACID] & 0x3;
				TX_DESC_MAPD = (priv->pmib->dot11RFEntry.dbf_rotation[TX_DESC_MACID]>>2) & 0x3;
			}
        }
		SET_TX_DESC_ANT_MAPC_V1_NO_CLR(ptx_desc, TX_DESC_MAPC);
		SET_TX_DESC_ANT_MAPD_V1_NO_CLR(ptx_desc, TX_DESC_MAPD);	
    }
#endif


#if (IS_EXIST_RTL8822BE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE)
    if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter))  {
	if (Adapter->pmib->dot11RFEntry.bcn2path || Adapter->pmib->dot11RFEntry.tx2path)
        SET_TX_DESC_TX_ANT_NO_CLR(ptx_desc, TX_ANT); 
    }
#endif // (IS_EXIST_RTL8822BE || IS_EXIST_RTL8197FEM)

#if defined(AP_SWPS_OFFLOAD) && IS_RTL88XX_MAC_V3
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter))
        SET_TX_DESC_DROP_ID(ptx_desc,TX_DESC_DROP_ID);
#endif
  
    //4 Set Dword6
#if CFG_HAL_SUPPORT_MBSSID
    if (HAL_IS_VAP_INTERFACE(Adapter)) {
    // set MBSSID for each VAP_ID
    SET_TX_DESC_MBSSID_NO_CLR(ptx_desc, HAL_VAR_VAP_INIT_SEQ);     
    }         
#endif //#if CFG_HAL_SUPPORT_MBSSID

#if IS_RTL88XX_MAC_V1_V2
    if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2){
		if(IS_GID)
		SET_TX_DESC_SW_DEFINE_NO_CLR(ptx_desc, 2);
    }
#endif

#if IS_RTL88XX_MAC_V1_V2
    if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2){
		if(TX_DESC_USERATE)
		SET_TX_DESC_SW_DEFINE_NO_CLR(ptx_desc, 1);
    }
#endif

#if defined(AP_SWPS_OFFLOAD) && IS_RTL88XX_MAC_V3
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter))
        SET_TX_DESC_SWPS_SEQ(ptx_desc, TX_DESC_SWPS_SEQ);   
#elif defined(AP_SWPS_OFFLOAD_WITH_AMPDU_VERI) 
    if(pdesc_data->macId ==1 || pdesc_data->macId==2){
    SET_TX_DESC_SWPS_SEQ(ptx_desc, priv->test_SWPSseq); 
    printk("TX_DESC_SWPS_SEQ=%d\n",priv->test_SWPSseq);
    }
#endif

    //4 Set Dword7
    SET_TX_DESC_TIMESTAMP_NO_CLR(ptx_desc, TX_DESC_TXBUFF);         

	//4 Set Dword8
    //4 Set Dword9
     SET_TX_DESC_SW_SEQ_NO_CLR(ptx_desc, TX_DESC_SEQ);
    
#if CFG_HAL_HW_SEQ
    if(IS_SUPPORT_HW_SEQ(Adapter)) { 
#if IS_RTL88XX_MAC_V1_V2 || IS_RTL8197G_SERIES
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2 || IS_HARDWARE_TYPE_8197G(Adapter))
            max_queueidx = HCI_TX_DMA_QUEUE_HI7;
#endif
#if IS_RTL8198F_SERIES
        if(IS_HARDWARE_TYPE_8198F(Adapter))
            max_queueidx = HCI_TX_DMA_QUEUE_HI15;
#endif
        
        if( (queueIndex >= HCI_TX_DMA_QUEUE_BK) && (queueIndex <= max_queueidx) ) {
#if IS_RTL88XX_MAC_V1_V2 || IS_RTL8198F_SERIES         
            if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2 || IS_HARDWARE_TYPE_8198F(Adapter)){
            SET_TX_DESC_EN_HWSEQ_NO_CLR(ptx_desc, 1);
            SET_TX_DESC_EN_HWEXSEQ_NO_CLR(ptx_desc, 0);
            SET_TX_DESC_DISQSELSEQ_NO_CLR(ptx_desc, 0);
        }
#endif      
#if IS_RTL8197G_SERIES
            if(IS_HARDWARE_TYPE_8197G(Adapter)){
                SET_TX_DESC_EN_HWSEQ_MODE(ptx_desc,1);
                
#ifdef NEW_HWSEQ_VERI
                if(priv->hwseq_type == 4){
                    SET_TX_DESC_EN_HWSEQ_MODE(ptx_desc,2);
                    SET_TX_DESC_HW_SSN_SEL(ptx_desc,priv->hwseq_ssn_sel);
                }else if(priv->hwseq_type == 3){
                    SET_TX_DESC_DISQSELSEQ(ptx_desc, 1);
                    SET_TX_DESC_HW_SSN_SEL(ptx_desc,priv->hwseq_ssn_sel);
                }
                //printk("[%s]GET_TX_DESC_EN_HWSEQ_MODE=%x\n",__FUNCTION__, GET_TX_DESC_EN_HWSEQ_MODE(ptx_desc));
#endif
            }
#endif

        }


    }
#else
    SET_TX_DESC_SW_SEQ_NO_CLR(ptx_desc, TX_DESC_SEQ);
#endif

#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
    if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(Adapter)) {
        SET_TX_DESC_PKT_OFFSET_NO_CLR(ptx_desc, TX_DESC_PKT_OFFSET);
        SET_TX_DESC_WHEADER_LEN_NO_CLR(ptx_desc, TX_DESC_WHEADER_LEN);
        SET_TX_DESC_MAC_CP_NO_CLR(ptx_desc, TX_DESC_MAC_CP);    
        SET_TX_DESC_SMH_EN_NO_CLR(ptx_desc, TX_DESC_SMH_EN);
    }
#endif

#ifdef TXSC_MULTI_ENTRY_VERI
    if(priv->store_HDR || priv->use_HDR){
        SET_TX_DESC_PKT_OFFSET_NO_CLR(ptx_desc, TX_DESC_PKT_OFFSET);
        SET_TX_DESC_WHEADER_LEN_NO_CLR(ptx_desc, TX_DESC_WHEADER_LEN);
        SET_TX_DESC_MAC_CP_NO_CLR(ptx_desc, TX_DESC_MAC_CP);    
        SET_TX_DESC_SMH_EN_NO_CLR(ptx_desc, TX_DESC_SMH_EN);
    }
#endif

#if CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC
    if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC(Adapter)) {
        SET_TX_DESC_TXWIFI_CP(ptx_desc, TX_DESC_TXWIFI_CP);
        #ifdef TXSC_MULTI_ENTRY_VERI
        SET_TX_DESC_STW_EN(ptx_desc, TX_DESC_STW_EN);
        #endif
    }
#endif

#if IS_EXIST_RTL8197GEM && (CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC || CFG_HAL_HW_TX_SHORTCUT_HDR_CONV || CFG_HAL_HW_SEQ)
    if(IS_HARDWARE_TYPE_8197G(Adapter)){
        if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_REUSE_TXDESC(Adapter) || IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(Adapter) || IS_SUPPORT_HW_SEQ(Adapter)) {
            SET_TX_DESC_SHCUT_CAM(ptx_desc, TX_DESC_SHCUT_CAM);
        }
    }
#endif

#if IS_EXIST_RTL8822BE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE
    if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter) ) {
    //4 Set Dword10
        if(SND_PKT_SEL) {
	SET_TX_DESC_SND_PKT_SEL_NO_CLR(ptx_desc, SND_PKT_SEL);
        }

	SET_TX_DESC_MU_DATARATE_NO_CLR(ptx_desc, TX_DESC_DATERATE);  

    #ifdef VERI_CSI_REPORT
        if(TX_DESC_NDPA)
        SET_TX_DESC_SND_PKT_SEL(ptx_desc, 0);
    #endif    
    }

#endif




#if 0
	3.) STW_ANT_DIS:  
	ant_mapA, ant_mapB, ant_mapC, ant_mapD, ANTSEL_A, ANTSEL_B, Ntx_map, TXPWR_OFFSET 
	4.) STW_RATE_DIS:  
	USE_RATE, Data rate, DATA_SHORT, DATA_BW, TRY_RATE
	5.) STW_RB_DIS:  
	RATE_ID, DISDATAFB, DISRTSFB, RTS_RATEFB_LMT, DATA_RATEFB_LMT
	6.) STW_PKTRE_DIS:	
	RTY_LMT_EN,  DATA_RT_LMT,  BAR_RTY_TH
#endif

#if 0
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword5, 0x7, TX_DW5_TXPWR_OFSET_MSK, TX_DW5_TXPWR_OFSET_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword6, 0x7, TX_DW6_ANTSEL_A_MSK, TX_DW6_ANTSEL_A_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword6, 0x7, TX_DW6_ANTSEL_B_MSK, TX_DW6_ANTSEL_B_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword6, 0x3, TX_DW6_ANT_MAP_A_MSK, TX_DW6_ANT_MAP_A_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword6, 0x3, TX_DW6_ANT_MAP_B_MSK, TX_DW6_ANT_MAP_B_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword6, 0x3, TX_DW6_ANT_MAP_C_MSK, TX_DW6_ANT_MAP_C_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword6, 0x3, TX_DW6_ANT_MAP_D_MSK, TX_DW6_ANT_MAP_D_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword7, 0xF, TX_DW7_NTX_MAP_MSK, TX_DW7_NTX_MAP_SH);
#endif

#if 0
	if(queueIndex == HCI_TX_DMA_QUEUE_BE) {
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword3, 0x1, TX_DW3_USERATE_MSK, TX_DW3_USERATE_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, 0x7F, TX_DW4_DATARATE_MSK, TX_DW4_DATARATE_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, 0x1, TX_DW4_TRY_RATE_MSK, TX_DW4_TRY_RATE_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword5, 0x1, TX_DW5_DATA_SHORT_MSK, TX_DW5_DATA_SHORT_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword5, 0x3, TX_DW5_DATA_BW_MSK, TX_DW5_DATA_BW_SH);
	}
#endif

#if 0
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword1, 0x1F, TX_DW1_RATE_ID_MSK, TX_DW1_RATE_ID_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword3, 0x1, TX_DW3_DISDATAFB_MSK, TX_DW3_DISDATAFB_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword3, 0x1, TX_DW3_DISRTSFB_MSK, TX_DW3_DISRTSFB_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, 0xF, TX_DW4_RTS_RATEFB_LMT_MSK, TX_DW4_RTS_RATEFB_LMT_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, 0x1F, TX_DW4_DATA_RATEFB_LMT_MSK, TX_DW4_DATA_RATEFB_LMT_SH);
#endif

#if 0
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, 0x1, TX_DW4_RTY_LMT_EN_MSK, TX_DW4_RTY_LMT_EN_SH);
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword4, 0x3F, TX_DW4_DATA_RT_LMT_MSK, TX_DW4_DATA_RT_LMT_SH);
	// TODO: bug ? still copy this field into desc
	SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, 0x3, TX_DW8_BAR_RTY_TH_MSK, TX_DW8_BAR_RTY_TH_SH);
#endif

#ifdef AP_SWPS_OFFLOAD
    tx_head     = &(cur_q->host_idx);
    pswdescinfo = get_txdesc_info(Adapter,Adapter->pshare->pdesc_info, queueIndex);
    pdescinfo = pswdescinfo + *tx_head;  

    if(pdescinfo->ptxdesc == NULL)
        pdescinfo->ptxdesc = kmalloc(sizeof(TX_DESC_88XX),GFP_KERNEL);
    
    memcpy((PVOID)pdescinfo->ptxdesc, (PVOID)ptx_desc, sizeof(TX_DESC_88XX));
#endif

#ifdef TRXBD_CACHABLE_REGION
//    _dma_cache_wback((unsigned long)((PVOID)ptx_desc-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), sizeof(TX_DESC_88XX));
#endif //#ifdef TRXBD_CACHABLE_REGION
}    

// 8197F, 8822B , 8198F used 
HAL_IMEM
VOID
FillShortCutTxDesc88XX_V1(
    IN      HAL_PADAPTER    Adapter,
    IN      u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN      PVOID           pDescData,
    IN      PVOID           pTxDesc
)
{
    PTX_DESC_DATA_88XX  pdesc_data  = (PTX_DESC_DATA_88XX)pDescData;
    PTX_DESC_88XX       ptx_desc    = (PTX_DESC_88XX)pTxDesc;
    PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q;
    PHCI_TX_DMA_MANAGER_88XX        ptx_dma;
    // tx shortcut can reuse TXDESC while 1) no security or 2) hw security
    // if no security iv == 0, so adding iv is ok for no security and hw security
    u2Byte  TX_DESC_TXPKTSIZE   = pdesc_data->hdrLen + pdesc_data->llcLen + pdesc_data->frLen + pdesc_data->iv;
    BOOLEAN TX_DESC_BK          = pdesc_data->bk;
    BOOLEAN TX_DESC_NAVUSEHDR   = pdesc_data->navUseHdr;
    u2Byte  TX_DESC_SEQ         = GetSequence(pdesc_data->pHdr);
    u1Byte  TX_DESC_DATA_STBC   = pdesc_data->dataStbc;
    BOOLEAN TX_DESC_RTY_LMT_EN  = pdesc_data->rtyLmtEn;   
    u1Byte  TX_DESC_DATA_RT_LMT = pdesc_data->dataRtyLmt;
    u4Byte  TXDESCSize;
    u2Byte                          *tx_head;
    struct tx_desc_info *pswdescinfo, *pdescinfo;

#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
    BOOLEAN TX_DESC_BMC              = (pdesc_data->smhEn == TRUE) ? 
    					((HAL_IS_MCAST(GetEthDAPtr((pu1Byte)pdesc_data->pHdr))) ? 1 : 0) : 
				        ((HAL_IS_MCAST(GetAddr1Ptr((pu1Byte)pdesc_data->pHdr))) ? 1 : 0);   // when multicast or broadcast, BMC = 1
#else
    BOOLEAN TX_DESC_BMC              = (HAL_IS_MCAST(GetAddr1Ptr((pu1Byte)pdesc_data->pHdr))) ? 1 : 0;   // when multicast or broadcast, BMC = 1        
#endif

    //Dword5
#if defined(AP_SWPS_OFFLOAD)
    u1Byte  TX_DESC_DROP_ID          = pdesc_data->DropID;
#endif

    //Dword6
#if defined(AP_SWPS_OFFLOAD)
    u2Byte  TX_DESC_SWPS_SEQ         = pdesc_data->SWPS_sequence;
#endif    

#if (IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM)
    if (IS_HARDWARE_TYPE_8197F(Adapter) || IS_HARDWARE_TYPE_8198F(Adapter) ) {
        TXDESCSize = SIZE_TXDESC_88XX;
    }
#endif  //IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM)

#if (IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE|| IS_EXIST_RTL8197GEM)
    if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)|| IS_HARDWARE_TYPE_8197G(Adapter))  {
        TXDESCSize = SIZE_TXDESC_88XX_V1;
		
		TX_DESC_BK = 0;
		
		if(TX_DESC_RTY_LMT_EN){
			if(TX_DESC_DATA_RT_LMT <= Adapter->pshare->rf_ft_var.mu_retry)
			TX_DESC_DATA_RT_LMT = (Adapter->pshare->rf_ft_var.mu_retry + 1);
		}
    }
#endif //IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8197GEM

    SET_TX_DESC_TXPKTSIZE(ptx_desc, TX_DESC_TXPKTSIZE);
    SET_TX_DESC_BK(ptx_desc, TX_DESC_BK);
    SET_TX_DESC_NAVUSEHDR(ptx_desc, TX_DESC_NAVUSEHDR);
    SET_TX_DESC_SW_SEQ(ptx_desc, TX_DESC_SEQ);

    if (TX_DESC_RTY_LMT_EN) {
        SET_TX_DESC_RTY_LMT_EN(ptx_desc, TX_DESC_RTY_LMT_EN);    
        SET_TX_DESC_RTS_DATA_RTY_LMT(ptx_desc, TX_DESC_DATA_RT_LMT);  
    } else if (TX_DESC_DATA_RT_LMT) {
        SET_TX_DESC_RTY_LMT_EN(ptx_desc, 0);    
        SET_TX_DESC_RTS_DATA_RTY_LMT(ptx_desc, 0);  
    }

#if 1 /*eric-8822 ??  No retry for broadcast & avoid tx hang */
	if(TX_DESC_BMC){
		SET_TX_DESC_DATA_RTY_LOWEST_RATE(ptx_desc, 0);
		SET_TX_DESC_RTY_LMT_EN(ptx_desc, 1);    
        	SET_TX_DESC_RTS_DATA_RTY_LMT(ptx_desc, 0);  
	}
#endif

    // for force tx rate
    if (HAL_VAR_TX_FORCE_RATE != 0xff) {       
        SET_TX_DESC_USE_RATE(ptx_desc, pdesc_data->useRate);
        #if IS_RTL88XX_MAC_V1_V2
        if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2 ){        
			SET_TX_DESC_SW_DEFINE(ptx_desc, pdesc_data->useRate);
        }
#endif
        SET_TX_DESC_DISRTSFB(ptx_desc, pdesc_data->disRTSFB);
        SET_TX_DESC_DISDATAFB(ptx_desc, pdesc_data->disDataFB);
        SET_TX_DESC_DATARATE(ptx_desc, pdesc_data->dataRate);

#if (IS_EXIST_RTL8822BE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE)
		if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)) 
		SET_TX_DESC_MU_DATARATE(ptx_desc, pdesc_data->dataRate); 
#endif
    }

#if (BEAMFORMING_SUPPORT == 1)
    SET_TX_DESC_DATA_STBC(ptx_desc, TX_DESC_DATA_STBC);     
#endif
	//4 Set Dword5
#if defined(AP_SWPS_OFFLOAD) && IS_RTL88XX_MAC_V3
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter))
        SET_TX_DESC_DROP_ID(ptx_desc,TX_DESC_DROP_ID);
#endif	
    //4 Set Dword6
#if defined(AP_SWPS_OFFLOAD) && IS_RTL88XX_MAC_V3
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter))
        SET_TX_DESC_SWPS_SEQ(ptx_desc, TX_DESC_SWPS_SEQ);   
#endif

#ifdef TRXBD_CACHABLE_REGION

#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
    // With CCI400 ,Do nothing for cache coherent code
#else
    rtl_cache_sync_wback(Adapter,(unsigned long)((PVOID)ptx_desc-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), TXDESCSize,PCI_DMA_TODEVICE);
#endif //CONFIG_ENABLE_CCI400
#else
    _dma_cache_wback((unsigned long)((PVOID)ptx_desc-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), TXDESCSize);
#endif //CONFIG_RTL_8198F
#endif //#ifdef TRXBD_CACHABLE_REGION

#ifdef AP_SWPS_OFFLOAD
    ptx_dma     = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);
    cur_q       = &(ptx_dma->tx_queue[queueIndex]);
    tx_head     = &(cur_q->host_idx);
    pswdescinfo = get_txdesc_info(Adapter,Adapter->pshare->pdesc_info, queueIndex);
    pdescinfo = pswdescinfo + *tx_head;  

    if(pdescinfo->ptxdesc == NULL)
        pdescinfo->ptxdesc = kmalloc(sizeof(TX_DESC_88XX),GFP_KERNEL);
    
    memcpy((PVOID)pdescinfo->ptxdesc, (PVOID)ptx_desc, sizeof(TX_DESC_88XX));
#endif


}

#endif //#if IS_EXIST_RTL8822BE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM


#if CFG_HAL_SUPPORT_TXDESC_IE
//this feature is only used in MAC V4 
HAL_IMEM
VOID
FillHwShortCutIE88XX(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN  PVOID           pDescData,
    IN  PVOID           pTxDesc
)
{
    PHCI_TX_DMA_MANAGER_88XX        ptx_dma;
    PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q;
    PTX_DESC_DATA_88XX              pdesc_data = (PTX_DESC_DATA_88XX)pDescData;
    PTX_DESC_88XX       ptx_desc    = (PTX_DESC_88XX)pTxDesc;
    u4Byte                          TXDESCSize;
    u2Byte                          *tx_head;
    struct tx_desc_info *pswdescinfo, *pdescinfo;
    u1Byte*             IE0_head;    
    u1Byte*             IE1_head;    
    u1Byte*             IE2_head;    
    u1Byte*             IE3_head;    
    u1Byte*             IE4_head;
    u1Byte  IE_cnt = 2;//default need IE0,IE1    
    u1Byte  need_IE2 = 0;    
    u1Byte  need_IE3 = 0;    
    u1Byte  need_IE4 = 0;
    u1Byte  TX_DESC_OFFSET;

#if defined(AP_SWPS_OFFLOAD)    
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter))        
        need_IE2 = 1;
#endif

    ptx_dma     = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);
    cur_q       = &(ptx_dma->tx_queue[queueIndex]);
    ptx_desc    = ((PTX_DESC_88XX)(cur_q->ptx_desc_head)) + cur_q->host_idx;

    IE0_head = ((u1Byte*)ptx_desc)+SIZE_TXDESC_BODY;    
    IE1_head = IE0_head+SIZE_TXDESC_IE;    
    TXDESCSize = SIZE_TXDESC_BODY+SIZE_TXDESC_IE*2;//IE_body+IE0+IE1    

    if (need_IE2){        
        IE2_head = IE1_head+SIZE_TXDESC_IE;        
        IE_cnt++;        
        TXDESCSize+=SIZE_TXDESC_IE;    
    }    
    else        
        IE2_head = IE1_head; //then, should not fill IE2    

    if(need_IE3){        
        IE3_head = IE2_head+SIZE_TXDESC_IE;        
        IE_cnt++;        
        TXDESCSize+=SIZE_TXDESC_IE;    
    }    
    else        
        IE3_head = IE2_head; //then, should not fill IE3   
    if(need_IE4){        
        IE4_head = IE3_head+SIZE_TXDESC_IE;        
        IE_cnt++;        
        TXDESCSize+=SIZE_TXDESC_IE;    
    }    
    else        
        IE4_head = IE3_head; //then, should not fill IE4

#if (IS_RTL88XX_MAC_V4)
    if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)  {
        TX_DESC_OFFSET = SIZE_TXDESC_88XX_V1; 

        switch(queueIndex) //set DMA channel by VO/VI/BE/BK //yllin8814B
        {
            case HCI_TX_DMA_QUEUE_BK_V2:
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL3;
            break;
            case HCI_TX_DMA_QUEUE_BE_V2:   
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL2;
            break;
            case HCI_TX_DMA_QUEUE_VI_V2:
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL1;
            break;
            case HCI_TX_DMA_QUEUE_VO_V2:
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL0;
            break;
            case HCI_TX_DMA_QUEUE_MGT_V2: 
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL14;
            break;    
            case HCI_TX_DMA_QUEUE_CMD_V2:
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL20;
            break;    
            case HCI_TX_DMA_QUEUE_FWCMD:
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL16;
            break;             
            case HCI_TX_DMA_QUEUE_BCN_V2:
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL19;
            break;              
            case HCI_TX_DMA_QUEUE_HI0_V2: 
            case HCI_TX_DMA_QUEUE_HI1_V2:     
            case HCI_TX_DMA_QUEUE_HI2_V2: 
            case HCI_TX_DMA_QUEUE_HI3_V2: 
            case HCI_TX_DMA_QUEUE_HI4_V2: 
            case HCI_TX_DMA_QUEUE_HI5_V2:     
            case HCI_TX_DMA_QUEUE_HI6_V2: 
            case HCI_TX_DMA_QUEUE_HI7_V2: 
            case HCI_TX_DMA_QUEUE_HI8_V1: 
            case HCI_TX_DMA_QUEUE_HI9_V1:     
            case HCI_TX_DMA_QUEUE_HI10_V1: 
            case HCI_TX_DMA_QUEUE_HI11_V1: 
            case HCI_TX_DMA_QUEUE_HI12_V1:
            case HCI_TX_DMA_QUEUE_HI13_V1: 
            case HCI_TX_DMA_QUEUE_HI14_V1: 
            case HCI_TX_DMA_QUEUE_HI15_V1:                
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL15;
            break;    
            default: //take as BE
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL2;
            break;
        }
    }
#endif //(IS_EXIST_RTL88142BE)

     u1Byte  TX_DESC_CHANNEL_DMA = pdesc_data->dma_channel;


	// Dword 0
	u2Byte  TX_DESC_TXPKTSIZE		= pdesc_data->hdrLen + pdesc_data->llcLen + pdesc_data->frLen + pdesc_data->iv;


	// Dword 1
	u1Byte  TX_DESC_MACID       	= pdesc_data->macId;
#if CFG_HAL_HW_TX_AMSDU
    BOOLEAN TX_DESC_HW_AMSDU        = pdesc_data->HW_AMSDU;
    u1Byte  TX_DESC_HW_AMSDU_SIZE   = pdesc_data->HW_AMSDU_size;

#endif


	// Dword 3

    //Dword5
#if defined(AP_SWPS_OFFLOAD)
    u1Byte  TX_DESC_DROP_ID          = pdesc_data->DropID;
#endif

    //Dword6
#if defined(AP_SWPS_OFFLOAD)
    u2Byte  TX_DESC_SWPS_SEQ         = pdesc_data->SWPS_sequence;
#endif        


	//Dword 9

    u2Byte TX_DESC_SEQ               = GetSequence(pdesc_data->pHdr);

#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV    
    u1Byte  TX_DESC_SMH_CAM         = pdesc_data->smhCamIdx;
#endif


#if CFG_HAL_HW_SEQ
        if(IS_SUPPORT_HW_SEQ(Adapter)) {
            //TX_DESC_SEQ               = 0;
        }
#endif // CFG_HAL_HW_SEQ
    
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
        u1Byte  TX_DESC_PKT_OFFSET;
        BOOLEAN TX_DESC_SMH_EN ;
        u1Byte  TX_DESC_WHEADER_LEN;
        u1Byte  TX_DESC_MAC_CP;
        if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(Adapter)) {
        TX_DESC_MAC_CP          = pdesc_data->macCp;
        TX_DESC_OFFSET          = ((pdesc_data->smhEn == TRUE) ? (HAL_HW_TXSC_HDR_CONV_ADD_OFFSET+TX_DESC_OFFSET) : TX_DESC_OFFSET);
        //TX_DESC_PKT_OFFSET      = 0; // unit: 8 bytes. Early mode: 1 units, (8 * 1 = 8 bytes for early mode info)
        TX_DESC_SMH_EN           = pdesc_data->smhEn;
    
        // Actually, no any conditions can run the (pdesc_data->hdrLen >> 1)  case in our driver.
        // Because, while calling this function, it must enable reuse TXDESC and hdr conv.
        TX_DESC_WHEADER_LEN     = ((pdesc_data->smhEn == TRUE) ? HAL_HW_TXSC_WHEADER_LEN : (pdesc_data->hdrLen >> 1)); // unit: 2 bytes
    
           // printk("%s %d Debug: TX_DESC_OFFSET = %d \n",__func__,__LINE__,TX_DESC_OFFSET);
           // printk("%s %d Debug: TX_DESC_SMH_EN = %d \n",__func__,__LINE__,TX_DESC_SMH_EN);
           // printk("%s %d Debug: TX_DESC_WHEADER_LEN = %d \n",__func__,__LINE__,TX_DESC_WHEADER_LEN);     
           
        }
    
      
#endif // CFG_HAL_HW_TX_SHORTCUT_HDR_CONV

    

    //Clear All Bit
    //platform_zero_memory((PVOID)ptx_desc, sizeof(TX_DESC_88XX));

    //4 Set Dword0
    SET_TX_DESC_TXPKTSIZE(ptx_desc, TX_DESC_TXPKTSIZE);
    SET_TX_DESC_OFFSET(ptx_desc, TX_DESC_OFFSET);    
    SET_TX_DESC_PKT_OFFSET_V1(ptx_desc, (TXDESCSize-SIZE_TXDESC_BODY)/SIZE_TXDESC_IE);//IE_body will not be ripped off
    SET_TX_DESC_CHANNEL_DMA(ptx_desc,TX_DESC_CHANNEL_DMA);    
    SET_TX_DESC_IE_CNT(ptx_desc,IE_cnt);
    SET_TX_DESC_IE_CNT_EN(ptx_desc,1);

	//4 Set Dword1

    

    SetTxDescQSel88XX_V2(Adapter, queueIndex, ptx_desc, pdesc_data->tid);        
    if ( (queueIndex >= HCI_TX_DMA_QUEUE_HI0_V2) && (queueIndex <= HCI_TX_DMA_QUEUE_HI15_V1) ) {
        
    } else {

        SET_TX_DESC_MACID_V1(ptx_desc,TX_DESC_MACID);

    } 
	
    
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
    //SET_TX_DESC_PKT_OFFSET_NO_CLR(ptx_desc, TX_DESC_PKT_OFFSET); //already set
#endif

	//4 Set Dword3
	//4 Set Dword5
#if defined(AP_SWPS_OFFLOAD)
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter)) 
        IE2_SET_TX_DESC_DROP_ID(IE2_head,TX_DESC_DROP_ID);
#endif	
    //4 Set Dword6
#if defined(AP_SWPS_OFFLOAD)
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter)) 
        IE2_SET_TX_DESC_SW_DEFINE(IE2_head, TX_DESC_SWPS_SEQ);   
#endif

    //4 Set Dword8
	//4 Set Dword9    

#if CFG_HAL_HW_SEQ
    if(IS_SUPPORT_HW_SEQ(Adapter)) {
        //use shortcut buffer seq
        IE1_SET_TX_DESC_EN_HWSEQ(IE1_head,0);
        IE1_SET_TX_DESC_EN_HWEXSEQ(IE1_head,1);
    }
    else
#endif //#if CFG_HAL_HW_SEQ
    IE1_SET_TX_DESC_SW_SEQ(IE1_head,TX_DESC_SEQ);


        //SET_TX_DESC_STW_EN_NO_CLR(ptx_desc,TX_DESC_STW_EN); //no this field

		//SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, TX_DESC_STW_ANT_DIS, TX_DW8_STW_ANT_DIS_MSK, TX_DW8_STW_ANT_DIS_SH);
		//SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, TX_DESC_STW_RATE_DIS, TX_DW8_STW_RATE_DIS_MSK, TX_DW8_STW_RATE_DIS_SH);
		//SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, TX_DESC_STW_RB_DIS, TX_DW8_STW_RB_DIS_MSK, TX_DW8_STW_RB_DIS_SH);
		//SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, TX_DESC_STW_PKTRE_DIS, TX_DW8_STW_PKTRE_DIS_MSK, TX_DW8_STW_PKTRE_DIS_SH);

        // for force tx rate
        if (HAL_VAR_TX_FORCE_RATE != 0xff) {
        /*                    
             STW_RATE_DIS:  USE_RATE, Data rate, DATA_SHORT, DATA_BW, TRY_RATE
             STW_RB_DIS:     RATE_ID, DISDATAFB, DISRTSFB, RTS_RATEFB_LMT, DATA_RATEFB_LMT
            */            

            //SET_TX_DESC_STW_ANT_DIS_NO_CLR(ptx_desc, 1); //no this field
            //SET_TX_DESC_STW_RB_DIS_NO_CLR(ptx_desc, 1);  //no this field
        
            IE0_SET_TX_DESC_USERATE(IE0_head, pdesc_data->useRate);
            IE0_SET_TX_DESC_DISRTSFB(IE0_head, pdesc_data->disRTSFB);
            IE0_SET_TX_DESC_DISDATAFB(IE0_head, pdesc_data->disDataFB);
            IE0_SET_TX_DESC_DATARATE(IE0_head, pdesc_data->dataRate);

            //SET_TX_DESC_RATE_ID_NO_CLR(ptx_desc,pdesc_data->rateId); //yllin8814B, change to ARFR_Table_sel ??
            IE0_SET_TX_DESC_DATA_BW(IE0_head,pdesc_data->dataBW);

            IE0_SET_TX_DESC_DATA_SHORT(IE0_head,1);
}

#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
        if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(Adapter)) {
            SET_TX_DESC_MHR_CP(ptx_desc, TX_DESC_MAC_CP);
            SET_TX_DESC_WHEADER_LEN_V1(ptx_desc,TX_DESC_WHEADER_LEN);
            SET_TX_DESC_SMH_EN_V1(ptx_desc,TX_DESC_SMH_EN);
            if(TX_DESC_SMH_CAM != 0xFF)
                SET_TX_DESC_SMH_CAM(ptx_desc,TX_DESC_SMH_CAM);
        }
#if CFG_HAL_HW_TX_AMSDU
        if(IS_SUPPORT_WLAN_HAL_HW_TX_AMSDU(Adapter)){
            SET_TX_DESC_AMSDU(ptx_desc, TX_DESC_HW_AMSDU);
            SET_TX_DESC_MAX_AMSDU_MODE(ptx_desc,TX_DESC_HW_AMSDU_SIZE); 
        }
#endif
#endif // CFG_HAL_HW_TX_SHORTCUT_HDR_CONV

#ifdef CONFIG_8814_AP_MAC_VERI
        if(Adapter->set_hw_amsdu==1)//tingchu
           SET_TX_DESC_AMSDU(ptx_desc,1);
#endif


    //IE0_SET_TX_DESC_IE_UP(IE0_head,1);            
    IE0_SET_TX_DESC_IE_NUM(IE0_head,0);            
    //IE1_SET_TX_DESC_IE_UP(IE1_head,1);            
    IE1_SET_TX_DESC_IE_NUM(IE1_head,1);     
    
    if(need_IE2){        
        //IE2_SET_TX_DESC_IE_UP(IE2_head,1);                
        IE2_SET_TX_DESC_IE_NUM(IE2_head,2);    
    }    
    if(need_IE3){        
        //IE3_SET_TX_DESC_IE_UP(IE3_head,1);                
        IE3_SET_TX_DESC_IE_NUM(IE3_head,3);    
    }    
    if(need_IE4){        
        //IE4_SET_TX_DESC_IE_UP(IE4_head,1);                
        IE4_SET_TX_DESC_IE_NUM(IE4_head,4);             
    }    
    
    if(need_IE4)        
        IE4_SET_TX_DESC_IE_END(IE4_head,1);    

    else if((!need_IE4) && need_IE3)        
        IE3_SET_TX_DESC_IE_END(IE3_head,1);        

    else if((!need_IE4) && (!need_IE3) && need_IE2)        
        IE2_SET_TX_DESC_IE_END(IE2_head,1);

    else
        IE1_SET_TX_DESC_IE_END(IE1_head,1);


#ifdef AP_SWPS_OFFLOAD
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter)) {

        tx_head     = &(cur_q->host_idx);
        pswdescinfo = get_txdesc_info(Adapter,Adapter->pshare->pdesc_info, queueIndex);
        pdescinfo = pswdescinfo + *tx_head;  

        pdescinfo->IE_bitmap = BIT0; //IE2

        if(pdescinfo->ptxdesc == NULL)
            pdescinfo->ptxdesc = kmalloc(sizeof(TX_DESC_88XX),GFP_KERNEL);
        
        memcpy((PVOID)pdescinfo->ptxdesc, (PVOID)ptx_desc, sizeof(TX_DESC_88XX));
    }
#endif


}
#endif

#if CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC
#if IS_RTL88XX_MAC_V2_V3 || IS_EXIST_RTL8192FE
// 8197F, 8198F, 8822B used  
HAL_IMEM
VOID
FillHwShortCutTxDesc88XX_V1(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN  PVOID           pDescData
)
{
    PHCI_TX_DMA_MANAGER_88XX        ptx_dma;
    PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q;
    PTX_DESC_88XX                   ptx_desc;
    PTX_DESC_DATA_88XX              pdesc_data = (PTX_DESC_DATA_88XX)pDescData;
    u4Byte                          TXDESCSize;
    u2Byte                          *tx_head;
    struct tx_desc_info *pswdescinfo, *pdescinfo;
    
#if (IS_EXIST_RTL8197FEM  || IS_EXIST_RTL8198FEM)
        if (IS_HARDWARE_TYPE_8197F(Adapter)|| IS_HARDWARE_TYPE_8198F(Adapter)) {
            TXDESCSize = SIZE_TXDESC_88XX;
        }
#endif  //IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM)
    
#if (IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8822CE || IS_EXIST_RTL8812FE|| IS_EXIST_RTL8197GEM)
        if ( IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8821C(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)|| IS_HARDWARE_TYPE_8197G(Adapter))  {
            TXDESCSize = SIZE_TXDESC_88XX_V1;
        }
#endif //IS_EXIST_RTL8822BE || IS_EXIST_RTL8821CE || IS_EXIST_RTL8197GEM

	// Dword 0
	u2Byte  TX_DESC_TXPKTSIZE		= pdesc_data->hdrLen + pdesc_data->llcLen + pdesc_data->frLen + pdesc_data->iv;
	u1Byte	TX_DESC_OFFSET			= TXDESCSize;

	// Dword 1
	u1Byte  TX_DESC_MACID       	= pdesc_data->macId;
	u1Byte  TX_DESC_RATE_ID          = pdesc_data->rateId;
	
	// Dword 2
	u4Byte  TX_DESC_PAID       	= pdesc_data->p_aid;

	// Dword 3

    //Dword5
#if defined(AP_SWPS_OFFLOAD)
    u1Byte  TX_DESC_DROP_ID          = pdesc_data->DropID;
#endif

    //Dword6
#if defined(AP_SWPS_OFFLOAD)
    u2Byte  TX_DESC_SWPS_SEQ         = pdesc_data->SWPS_sequence;
#endif        

	// Dword 8
	//BOOLEAN	TX_DESC_STW_ANT_DIS		= pdesc_data->stwAntDis;
	//BOOLEAN	TX_DESC_STW_RATE_DIS	= pdesc_data->stwRateDis;
	//BOOLEAN	TX_DESC_STW_RB_DIS		= pdesc_data->stwRbDis;
	//BOOLEAN	TX_DESC_STW_PKTRE_DIS 	= pdesc_data->stwPktReDis;
#if 0
3.)	STW_ANT_DIS:  
ant_mapA, ant_mapB, ant_mapC, ant_mapD, ANTSEL_A, ANTSEL_B, Ntx_map, TXPWR_OFFSET 
4.)	STW_RATE_DIS:  
USE_RATE, Data rate, DATA_SHORT, DATA_BW, TRY_RATE
5.)	STW_RB_DIS:  
RATE_ID, DISDATAFB, DISRTSFB, RTS_RATEFB_LMT, DATA_RATEFB_LMT
6.)	STW_PKTRE_DIS:  
RTY_LMT_EN,  DATA_RT_LMT,  BAR_RTY_TH
#endif

	BOOLEAN	TX_DESC_STW_EN			 = pdesc_data->stwEn;
	
	//Dword 9
    u2Byte TX_DESC_SEQ               = GetSequence(pdesc_data->pHdr);

#if CFG_HAL_HW_SEQ
    if(IS_SUPPORT_HW_SEQ(Adapter)) {
        //TX_DESC_SEQ               = 0;
    }
#endif // CFG_HAL_HW_SEQ

#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
    u1Byte  TX_DESC_PKT_OFFSET;
    BOOLEAN	TX_DESC_SMH_EN ;
    u1Byte	TX_DESC_WHEADER_LEN;
    if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(Adapter)) {

	TX_DESC_OFFSET			= ((pdesc_data->smhEn == TRUE) ? (HAL_HW_TXSC_HDR_CONV_ADD_OFFSET+TXDESCSize) : TXDESCSize);
    TX_DESC_PKT_OFFSET      = 0; // unit: 8 bytes. Early mode: 1 units, (8 * 1 = 8 bytes for early mode info)
	TX_DESC_SMH_EN 			 = pdesc_data->smhEn;

    // Actually, no any conditions can run the (pdesc_data->hdrLen >> 1)  case in our driver.
    // Because, while calling this function, it must enable reuse TXDESC and hdr conv.
	TX_DESC_WHEADER_LEN		= ((pdesc_data->smhEn == TRUE) ? HAL_HW_TXSC_WHEADER_LEN : (pdesc_data->hdrLen >> 1)); // unit: 2 bytes

       // printk("%s %d Debug: TX_DESC_OFFSET = %d \n",__func__,__LINE__,TX_DESC_OFFSET);
       // printk("%s %d Debug: TX_DESC_SMH_EN = %d \n",__func__,__LINE__,TX_DESC_SMH_EN);
       // printk("%s %d Debug: TX_DESC_WHEADER_LEN = %d \n",__func__,__LINE__,TX_DESC_WHEADER_LEN);           
    }

  
#endif // CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
 
#if 0 //(CFG_HAL_HW_TX_SHORTCUT_REUSE_TXDESC||CFG_HAL_HW_TX_SHORTCUT_HDR_CONV)
    if (pdesc_data->stwEn != 0 || pdesc_data->smhEn != 0)
        printk("%s(%d): stwEn:0x%x, smhEn:0x%x \n", __FUNCTION__, __LINE__, pdesc_data->stwEn, pdesc_data->smhEn);
#endif

#if 0 // show TX Rpt Info
{
    int baseReg = 0x8080, offset, i;
    int lenTXDESC = 10, lenHdrInfo = 20;

	HAL_RTL_W8(0x106, 0x7F);
	HAL_RTL_W32(0x140, 0x662);

    for(i = 0; i < lenTXDESC; i++) {
#if 1
        printk("%08X ", (u4Byte)GET_DESC(HAL_RTL_R32(baseReg)));
#else
        printk("0x%x:%08X ", baseReg, (u4Byte)GET_DESC(HAL_RTL_R32(baseReg)));
#endif
        if (i%4==3)
            printk("\n");
        baseReg += 4;
    }
    printk("\n");

    for(i = 0; i < lenHdrInfo; i++) {
#if 1
        printk("%08X ", (u4Byte)GET_DESC(HAL_RTL_R32(baseReg)));
#else
        printk("0x%x:%08X ", baseReg, (u4Byte)GET_DESC(HAL_RTL_R32(baseReg)));
#endif

        if (i%4==3)
            printk("\n");
        baseReg += 4;
    }
    printk("\n");

#if 0
	printk("%08X %08X %08X %08X \n%08X %08X %08X %08X \n%08X %08X \n", 
                (u4Byte)GET_DESC(HAL_RTL_R32(0x8080)), (u4Byte)GET_DESC(HAL_RTL_R32(0x8084)), 
                (u4Byte)GET_DESC(HAL_RTL_R32(0x8088)), (u4Byte)GET_DESC(HAL_RTL_R32(0x808c)),
                (u4Byte)GET_DESC(HAL_RTL_R32(0x8090)), (u4Byte)GET_DESC(HAL_RTL_R32(0x8094)),
                (u4Byte)GET_DESC(HAL_RTL_R32(0x8098)), (u4Byte)GET_DESC(HAL_RTL_R32(0x809c)),
                (u4Byte)GET_DESC(HAL_RTL_R32(0x80a0)), (u4Byte)GET_DESC(HAL_RTL_R32(0x80a4)));
#endif
}
#endif

    ptx_dma     = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);
    cur_q       = &(ptx_dma->tx_queue[queueIndex]);
    ptx_desc    = ((PTX_DESC_88XX)(cur_q->ptx_desc_head)) + cur_q->host_idx;

    //Clear All Bit
    platform_zero_memory((PVOID)ptx_desc, sizeof(TX_DESC_88XX));

    //4 Set Dword0
    SET_TX_DESC_TXPKTSIZE_NO_CLR(ptx_desc, TX_DESC_TXPKTSIZE);
    SET_TX_DESC_OFFSET_NO_CLR(ptx_desc, TX_DESC_OFFSET);    


	//4 Set Dword1

    SetTxDescQSel88XX_V1(Adapter, queueIndex, ptx_desc, pdesc_data->tid);        

	if ( (queueIndex >= HCI_TX_DMA_QUEUE_HI0) && (queueIndex <= HCI_TX_DMA_QUEUE_HI7) ) {
        //MacID has written in SetTxDescQSel88XX()
    } else {
        SET_TX_DESC_MACID_NO_CLR(ptx_desc, TX_DESC_MACID);
    }
	//4 Set Dword2

	SET_TX_DESC_P_AID_NO_CLR(ptx_desc, TX_DESC_PAID);
	
	//4 Set Dword3
	//4 Set Dword5
#if defined(AP_SWPS_OFFLOAD)
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter)) 
        SET_TX_DESC_DROP_ID(ptx_desc,TX_DESC_DROP_ID);
#endif	
    //4 Set Dword6
#if defined(AP_SWPS_OFFLOAD)
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter)) 
        SET_TX_DESC_SWPS_SEQ(ptx_desc, TX_DESC_SWPS_SEQ);   
#endif
	
    //4 Set Dword8
	//4 Set Dword9    
     SET_TX_DESC_SW_SEQ_NO_CLR(ptx_desc, TX_DESC_SEQ);

#if CFG_HAL_HW_SEQ
    if(IS_SUPPORT_HW_SEQ(Adapter)) {    
        //SET_TX_DESC_EN_HWSEQ_NO_CLR(ptx_desc, 1);
    }
#endif //#if CFG_HAL_HW_SEQ
		
#if (IS_EXIST_RTL8812FE || IS_EXIST_RTL8197GEM)
	if (IS_HARDWARE_TYPE_8812F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter))
		SET_TX_DESC_SHCUT_CAM_NO_CLR(ptx_desc,GET_TX_DESC_MACID(ptx_desc));
#endif

#if (IS_RTL88XX_MAC_V2_V3)
    if ( _GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v2_v3) {


        SET_TX_DESC_STW_EN_NO_CLR(ptx_desc,TX_DESC_STW_EN);

		//SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, TX_DESC_STW_ANT_DIS, TX_DW8_STW_ANT_DIS_MSK, TX_DW8_STW_ANT_DIS_SH);
		//SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, TX_DESC_STW_RATE_DIS, TX_DW8_STW_RATE_DIS_MSK, TX_DW8_STW_RATE_DIS_SH);
		//SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, TX_DESC_STW_RB_DIS, TX_DW8_STW_RB_DIS_MSK, TX_DW8_STW_RB_DIS_SH);
		//SET_DESC_FIELD_NO_CLR(ptx_desc->Dword8, TX_DESC_STW_PKTRE_DIS, TX_DW8_STW_PKTRE_DIS_MSK, TX_DW8_STW_PKTRE_DIS_SH);

#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
        if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(Adapter)) {
            SET_TX_DESC_PKT_OFFSET_NO_CLR(ptx_desc, TX_DESC_PKT_OFFSET);
            SET_TX_DESC_WHEADER_LEN_NO_CLR(ptx_desc,TX_DESC_WHEADER_LEN);
            SET_TX_DESC_SMH_EN_NO_CLR(ptx_desc,TX_DESC_SMH_EN);
        }
#endif // CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
	}
#endif // (IS_RTL88XX_MAC_V2_V3)

#ifdef AP_SWPS_OFFLOAD
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter)) {

        tx_head     = &(cur_q->host_idx);
        pswdescinfo = get_txdesc_info(Adapter,Adapter->pshare->pdesc_info, queueIndex);
        pdescinfo = pswdescinfo + *tx_head;  

        if(pdescinfo->ptxdesc == NULL)
            pdescinfo->ptxdesc = kmalloc(sizeof(TX_DESC_88XX),GFP_KERNEL);
        
        memcpy((PVOID)pdescinfo->ptxdesc, (PVOID)ptx_desc, sizeof(TX_DESC_88XX));
    }
#endif


}
#endif
#endif

#endif // #if IS_EXIST_RTL8822BE || IS_EXIST_RTL8197FEM || IS_EXIST_RTL8198FEM || IS_EXIST_RTL8814BE|| IS_EXIST_RTL8197GEM

#if IS_RTL88XX_MAC_V4

VOID
FillBeaconDesc88XX_V2
(
    IN	HAL_PADAPTER        Adapter,
    IN  PVOID               _pdesc,
    IN  PVOID               data_content,
    IN  u2Byte              txLength,
    IN  BOOLEAN             bForceUpdate
)
{
    u4Byte  TXDESCSize, TXDESCSize_inTXBUF;
    PTX_DESC_88XX       pdesc = (PTX_DESC_88XX)_pdesc;
#if CFG_HAL_SUPPORT_TXDESC_IE    
    u1Byte*             IE0_head = ((u1Byte*)pdesc)+16;
    u1Byte*             IE1_head = IE0_head+8;
    u1Byte*             IE2_head = IE1_head+8;
    u1Byte*             IE3_head = IE2_head+8;
    u1Byte*             IE4_head = IE3_head+8;
        
#if (IS_EXIST_RTL8814BE)
    if ( IS_HARDWARE_TYPE_8814B(Adapter))  {
        TXDESCSize = SIZE_TXDESC_88XX_V2_BCN;
        TXDESCSize_inTXBUF = SIZE_TXDESC_88XX_V2_BCN;
    }
#endif //(IS_EXIST_RTL8814BE)

    platform_zero_memory(pdesc, TXDESCSize);

    if ( IS_SUPPORT_TXDESC_IE(Adapter))  {

        //IE_body    
        SET_TX_DESC_OFFSET(pdesc, TXDESCSize_inTXBUF); //only TXDESC, no burst mode, no HW AESIV, no TCP/IP checksum, no HW shortcut append MAC header 
        SET_TX_DESC_PKT_OFFSET_V1(pdesc, (TXDESCSize-SIZE_TXDESC_BODY)/SIZE_TXDESC_IE);//IE_body will not be ripped off
        SET_TX_DESC_TXPKTSIZE(pdesc, txLength);
        SET_TX_DESC_QSEL(pdesc,TXDESC_QSEL_BCN);  
        SET_TX_DESC_CHANNEL_DMA(pdesc,TXDESC_DMA_CHANNEL19); //bcn
        //SET_TX_DESC_IE_CNT(pdesc,5);//IE0~IE4
        //SET_TX_DESC_IE_CNT_EN(pdesc,1);

        //IE1
        IE1_SET_TX_DESC_BMC(IE1_head, 1);
#if CFG_HAL_SUPPORT_MBSSID
        if (HAL_IS_VAP_INTERFACE(Adapter)) {

        // set MBSSID for each VAP_ID
        //IE_body
        SET_TX_DESC_MACID_V1(pdesc, HAL_VAR_VAP_INIT_SEQ);
        //IE2
        IE2_SET_TX_DESC_MBSSID(IE2_head, HAL_VAR_VAP_INIT_SEQ); 
        }         
#endif //#if CFG_HAL_SUPPORT_MBSSID

        IE1_SET_TX_DESC_SW_SEQ(IE1_head,GetSequence(data_content));
        IE0_SET_TX_DESC_DISDATAFB(IE0_head,1);
        IE0_SET_TX_DESC_USERATE(IE0_head,1);
        /*cy wang cfg p2p rm*/
        if (HAL_VAR_IS_40M_BW == 1) {
            if (HAL_VAR_OFFSET_2ND_CHANNEL == HT_2NDCH_OFFSET_BELOW) {
                IE3_SET_TX_DESC_DATA_SC(IE3_head,TXDESC_DATASC_LOWER);
            }
            else {
                IE3_SET_TX_DESC_DATA_SC(IE3_head,TXDESC_DATASC_UPPER);
            }
        }
    
        //Dword4    /*cy wang cfg p2p , 6m rate beacon*//*cy wang cfg p2p*/
#ifdef P2P_SUPPORT	// 2014-0328 use 6m rate send beacon
        if(Adapter->pmib->p2p_mib.p2p_enabled){          
            IE0_SET_TX_DESC_RTSRATE(IE0_head,4);
            IE0_SET_TX_DESC_DATARATE(IE0_head,4);
        }    
#endif   
        /*cy wang cfg p2p*/
/*
         * Intel IOT, dynamic enhance beacon tx AGC
*/
    
        if (Adapter->pmib->dot11StationConfigEntry.beacon_rate != 0xff)
            IE0_SET_TX_DESC_DATARATE(IE0_head,Adapter->pmib->dot11StationConfigEntry.beacon_rate);
        
#if 0//IS_EXIST_RTL8814BE //yllin8814B, do we need this?
        if(IS_HARDWARE_TYPE_8814B(Adapter)) {
            IE0_SET_TX_DESC_RTSRATE(IE0_head,4);
            IE0_SET_TX_DESC_DATARATE(IE0_head,4);
        }
#endif

#if 0 //yllin8814B where is TXPWR_OFSET ??
	if (Adapter->bcnTxAGC ==1) {
        SET_TX_DESC_TXPWR_OFSET(pdesc,4); // +3dB
	} else if (Adapter->bcnTxAGC ==2) {
        SET_TX_DESC_TXPWR_OFSET(pdesc,5); // +6dB	
	} else {
        SET_TX_DESC_TXPWR_OFSET(pdesc,0);
	}

	if (Adapter->pmib->dot11RFEntry.txpwr_reduction) {
		if (Adapter->pmib->dot11RFEntry.txpwr_reduction <= 3)
            SET_TX_DESC_TXPWR_OFSET(pdesc,Adapter->pmib->dot11RFEntry.txpwr_reduction); 
	}
#endif
    // TODO: Why ?
    HAL_VAR_IS_40M_BW_BAK   = HAL_VAR_IS_40M_BW;
    HAL_VAR_TX_BEACON_LEN   = txLength;
    
    // Group Bit Control
    IE2_SET_TX_DESC_GROUP_BIT_IE_OFFSET(IE2_head,(HAL_VAR_TIM_OFFSET-24));
    
    // Auto set bitmap control by HW, no present in TXDESC document so reserved this code
    if (HAL_OPMODE & WIFI_ADHOC_STATE) {
        IE2_SET_TX_DESC_GROUP_BIT_IE_OFFSET(IE2_head,0);
    } else {
        IE2_SET_TX_DESC_GROUP_BIT_IE_OFFSET(IE2_head,1);
    }
    
    IE0_SET_TX_DESC_IE_NUM(IE0_head,0);
    IE1_SET_TX_DESC_IE_NUM(IE1_head,1);
    IE2_SET_TX_DESC_IE_NUM(IE2_head,2);
    IE3_SET_TX_DESC_IE_NUM(IE3_head,3);
    IE4_SET_TX_DESC_IE_NUM(IE4_head,4); 
    IE4_SET_TX_DESC_IE_END(IE4_head,1); 

    IE0_SET_TX_DESC_IE_UP(IE0_head, 1);
    IE1_SET_TX_DESC_IE_UP(IE1_head, 1);
    IE2_SET_TX_DESC_IE_UP(IE2_head, 1);
    IE3_SET_TX_DESC_IE_UP(IE3_head, 1);
    IE4_SET_TX_DESC_IE_UP(IE4_head, 1);
    
    }
#endif //SUPPORT_TXDESC_IE      
        
#ifdef TRXBD_CACHABLE_REGION
#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
        // With CCI400 ,Do nothing for cache coherent code
#else
        rtl_cache_sync_wback(Adapter,(unsigned long)((PVOID)(pdesc)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), TXDESCSize,PCI_DMA_TODEVICE);
#endif //CONFIG_ENABLE_CCI400
#else
        _dma_cache_wback((unsigned long)((PVOID)(pdesc)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), TXDESCSize);
#endif //CONFIG_RTL_8198F
#endif //#ifdef TRXBD_CACHABLE_REGION

}
#if defined(CONFIG_8814_AP_MAC_VERI)
static u8 test_seq;
#endif
VOID
FillTxDesc88XX_V2 (
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN  PVOID           pDescData
)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)Adapter;
    PHCI_TX_DMA_MANAGER_88XX        ptx_dma;
    PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q;
    PTX_DESC_88XX                   ptx_desc;
    PTX_DESC_DATA_88XX              pdesc_data = (PTX_DESC_DATA_88XX)pDescData;
    u4Byte                          val;
    u4Byte                          tmp,tmpCache;
    u4Byte                          TXDESCSize,TXDESCSize_inTXBUF;
    u2Byte                          *tx_head;
    struct tx_desc_info *pswdescinfo, *pdescinfo;
    u1Byte*             IE0_head;
    u1Byte*             IE1_head;
    u1Byte*             IE2_head;
    u1Byte*             IE3_head;
    u1Byte*             IE4_head;
    u4Byte              dma_queueIndex;
	u1Byte*				phdr;
    
#if CFG_HAL_SUPPORT_TXDESC_IE
//3 will fill all IEs

        {
            dma_queueIndex = queueIndex;

        }


#if (IS_RTL88XX_MAC_V4)
    if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)  {
        TXDESCSize = SIZE_TXDESC_BODY+SIZE_TXDESC_IE*5;//IE_body+IE0~4, no IE5
        switch(queueIndex) //set DMA channel by VO/VI/BE/BK //yllin8814B
        {
            case HCI_TX_DMA_QUEUE_BK_V2:
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL3;
            break;
            case HCI_TX_DMA_QUEUE_BE_V2:   
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL2;
            break;
            case HCI_TX_DMA_QUEUE_VI_V2:
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL1;
            break;
            case HCI_TX_DMA_QUEUE_VO_V2:
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL0;
            break;
            case HCI_TX_DMA_QUEUE_MGT_V2: 
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL14;
            break;    
            case HCI_TX_DMA_QUEUE_CMD_V2:
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL20;
            break;    
            case HCI_TX_DMA_QUEUE_FWCMD:
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL16;
            break;             
            case HCI_TX_DMA_QUEUE_BCN_V2:
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL19;
            break;              
            case HCI_TX_DMA_QUEUE_ACH11:
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL11;
            break;              
            case HCI_TX_DMA_QUEUE_HI0_V2: 
            case HCI_TX_DMA_QUEUE_HI1_V2:     
            case HCI_TX_DMA_QUEUE_HI2_V2: 
            case HCI_TX_DMA_QUEUE_HI3_V2: 
            case HCI_TX_DMA_QUEUE_HI4_V2: 
            case HCI_TX_DMA_QUEUE_HI5_V2:     
            case HCI_TX_DMA_QUEUE_HI6_V2: 
            case HCI_TX_DMA_QUEUE_HI7_V2: 
            case HCI_TX_DMA_QUEUE_HI8_V1: 
            case HCI_TX_DMA_QUEUE_HI9_V1:     
            case HCI_TX_DMA_QUEUE_HI10_V1: 
            case HCI_TX_DMA_QUEUE_HI11_V1: 
            case HCI_TX_DMA_QUEUE_HI12_V1:
            case HCI_TX_DMA_QUEUE_HI13_V1: 
            case HCI_TX_DMA_QUEUE_HI14_V1: 
            case HCI_TX_DMA_QUEUE_HI15_V1:                
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL15;
            break;    
            default: //take as BE
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL2;
            break;
        }
    }
#endif //(IS_EXIST_RTL8814BE)

#ifdef CONFIG_RTL_OFFLOAD_DRIVER
	if (OFFLOAD_ENABLE(priv))
		phdr = pdesc_data->pHdr + sizeof(struct h2d_txpkt_info);
	else
#endif
		phdr = pdesc_data->pHdr;

    //Dword 0
    u2Byte  TX_DESC_TXPKTSIZE        = pdesc_data->hdrLen + pdesc_data->llcLen + pdesc_data->frLen;


    u1Byte  TX_DESC_OFFSET           = TXDESCSize; 



    BOOLEAN TX_DESC_BMC              = HAL_IS_MCAST(GetAddr1Ptr(phdr)) ? 1 : 0;   // when multicast or broadcast, BMC = 1        

    
    u1Byte  TX_DESC_CHANNEL_DMA = pdesc_data->dma_channel;

#ifdef CONFIG_8814_AP_MAC_VERI
    BOOLEAN TX_DESC_DMA_PRI          = (Adapter->dma_priority) ? 1 : 0; 
#else
    BOOLEAN TX_DESC_DMA_PRI          = 0;
#endif 

#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV    
    u1Byte  TX_DESC_SMH_CAM = pdesc_data->smhCamIdx;
#endif
//    BOOLEAN TX_DESC_HT               = 0;
//    BOOLEAN TX_DESC_LINIP            = 0;
//    BOOLEAN TX_DESC_NOACM            = 0;
//    BOOLEAN TX_DESC_GF               = 0;

    //Dword 1 
    u1Byte  TX_DESC_MACID            = pdesc_data->macId; // MACID/MBSSID ?
    u1Byte  TX_DESC_RATE_ID          = pdesc_data->rateId;
    BOOLEAN TX_DESC_MORE_DATA        = pdesc_data->moreData;    
    BOOLEAN TX_DESC_EN_DESC_ID       = pdesc_data->enDescId;
#if CFG_HAL_HW_TX_AMSDU
    BOOLEAN TX_DESC_HW_AMSDU        = pdesc_data->HW_AMSDU;
    u1Byte  TX_DESC_HW_AMSDU_SIZE   = pdesc_data->HW_AMSDU_size;
#endif

    
    //Dword 2
    BOOLEAN TX_DESC_AGG_EN           = pdesc_data->aggEn; 
    BOOLEAN TX_DESC_BK               = pdesc_data->bk;
    BOOLEAN TX_DESC_MOREFRAG         = pdesc_data->frag;
    u1Byte  TX_DESC_AMPDU_DENSITY    = pdesc_data->ampduDensity;
    u4Byte  TX_DESC_P_AID      		 = pdesc_data->p_aid;
    u1Byte  TX_DESC_G_ID      		 = pdesc_data->g_id;
#if CFG_HAL_HW_AES_IV
    BOOLEAN TX_DESC_HW_AES_IV        = pdesc_data->hwAESIv;
#endif // CFG_HAL_HW_AES_IV

#if 1 //eric-ac2
	u1Byte	TX_DESC_CCA_RTS 		 = pdesc_data->cca_rts;
#endif


    //Dword 3
    BOOLEAN TX_DESC_USERATE          = pdesc_data->useRate;    
    BOOLEAN TX_DESC_DISRTSFB         = pdesc_data->disRTSFB;
    BOOLEAN TX_DESC_DISDATAFB        = pdesc_data->disDataFB;
    BOOLEAN TX_DESC_CTS2SELF         = pdesc_data->CTS2Self;
    BOOLEAN TX_DESC_RTS_EN           = pdesc_data->RTSEn;
    BOOLEAN TX_DESC_HW_RTS_EN        = pdesc_data->HWRTSEn;
    BOOLEAN TX_DESC_NAVUSEHDR        = pdesc_data->navUseHdr;
    u1Byte  TX_DESC_MAX_AGG_NUM      = pdesc_data->maxAggNum;
    BOOLEAN TX_DESC_NDPA		     = pdesc_data->ndpa;

    //Dword 4
    u1Byte  TX_DESC_DATERATE         = pdesc_data->dataRate;
    u1Byte  TX_DESC_DATA_RATEFB_LMT  = pdesc_data->dataRateFBLmt;
    u1Byte  TX_DESC_RTS_RATEFB_LMT   = pdesc_data->RTSRateFBLmt;
    BOOLEAN TX_DESC_RTY_LMT_EN       = pdesc_data->rtyLmtEn;   
    u1Byte  TX_DESC_DATA_RT_LMT      = pdesc_data->dataRtyLmt;
    u1Byte  TX_DESC_RTSRATE          = pdesc_data->RTSRate;
    u1Byte  TX_DESC_BMCRtyLmt        = pdesc_data->BMCRtyLmt;

    //Dword 5
    u1Byte  TX_DESC_DATA_SC          = pdesc_data->dataSC;
    u1Byte  TX_DESC_DATA_SHORT       = pdesc_data->dataShort;
    u1Byte  TX_DESC_DATA_BW          = pdesc_data->dataBW;
    u1Byte  TX_DESC_DATA_STBC        = pdesc_data->dataStbc;
	u1Byte  TX_DESC_DATA_LDPC        = pdesc_data->dataLdpc;
    u1Byte  TX_DESC_RTS_SHORT        = pdesc_data->RTSShort;
    u1Byte  TX_DESC_RTS_SC           = pdesc_data->RTSSC;   
    u1Byte	TX_DESC_POWER_OFFSET	 = pdesc_data->TXPowerOffset;
    u1Byte	TX_ANT					 = pdesc_data->TXAnt;	
#if defined(AP_SWPS_OFFLOAD)
    u1Byte  TX_DESC_DROP_ID          = pdesc_data->DropID;
#endif    
    //Dword6
#if defined(AP_SWPS_OFFLOAD)
    u2Byte  TX_DESC_SWPS_SEQ         = pdesc_data->SWPS_sequence;
#endif

    //Dword 7
    // use for CFG_HAL_TX_SHORTCUT
    u2Byte  TX_DESC_TXBUFF           = pdesc_data->frLen;

	// Dword 8
    BOOLEAN	TX_DESC_TXWIFI_CP;
    BOOLEAN TX_DESC_MAC_CP;
    BOOLEAN TX_DESC_SMH_EN;
    u1Byte  TX_DESC_WHEADER_LEN;
    //u1Byte  TX_DESC_PKT_OFFSET;
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
    if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(Adapter)) {
        TX_DESC_MAC_CP          = pdesc_data->macCp;
        TX_DESC_SMH_EN          = pdesc_data->smhEn;
        TX_DESC_OFFSET           = ((pdesc_data->smhEn == TRUE) ? (HAL_HW_TXSC_HDR_CONV_ADD_OFFSET+TXDESCSize) : TXDESCSize);
        TX_DESC_BMC              = (pdesc_data->smhEn == TRUE) ? 
                        ((HAL_IS_MCAST(GetEthDAPtr((pu1Byte)pdesc_data->pHdr))) ? 1 : 0) : 
                        ((HAL_IS_MCAST(GetAddr1Ptr((pu1Byte)pdesc_data->pHdr))) ? 1 : 0);   // when multicast or broadcast, BMC = 1        
        //TX_DESC_PKT_OFFSET       = 0; // unit: 8 bytes. Early mode: 1 units, (8 * 1 = 8 bytes for early mode info)                      

        if(IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV_LLC(Adapter)) {
            TX_DESC_WHEADER_LEN      = ((pdesc_data->smhEn == TRUE) ? (HAL_ETH_HEADER_LEN_MAX >> 1) : ((pdesc_data->hdrLen) >> 1)); // unit: 2 bytes
        }else {
            TX_DESC_WHEADER_LEN      = ((pdesc_data->smhEn == TRUE) ? (HAL_ETH_HEADER_LEN_MAX >> 1) : ((pdesc_data->hdrLen+pdesc_data->llcLen+pdesc_data->iv) >> 1)); // unit: 2 bytes
        }

    }
    
#endif // CFG_HAL_HW_TX_SHORTCUT_HDR_CONV

    //Dword 9
#if CFG_HAL_HW_SEQ
    u2Byte TX_DESC_SEQ               = 0;
#elif defined(CONFIG_8814_AP_MAC_VERI)
    u2Byte TX_DESC_SEQ               = test_seq++; //to test dma channel weight
#else
    u2Byte TX_DESC_SEQ               = GetSequence(pdesc_data->pHdr);
#endif

	//Dword 10
	u1Byte SND_PKT_SEL               = pdesc_data->SND_pkt_sel;;
	u1Byte IS_GID               	 = pdesc_data->is_GID;
	u1Byte SND_TARGET				 = pdesc_data->SND_target;

    if(Adapter->pshare->rf_ft_var.manual_ampdu)
		TX_DESC_MAX_AGG_NUM = 0;

#if CFG_HAL_MULTICAST_BMC_ENHANCE
    if(TX_DESC_BMC)
    {
        TX_DESC_BMCRtyLmt = pdesc_data->BMCRtyLmt;
    }
#endif


    ptx_dma     = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);
    cur_q       = &(ptx_dma->tx_queue[dma_queueIndex]);
    ptx_desc    = ((PTX_DESC_88XX)(cur_q->ptx_desc_head)) + cur_q->host_idx;
    IE0_head    = ((u1Byte*)ptx_desc)+16;
    IE1_head    = IE0_head+8;
    IE2_head    = IE1_head+8;
    IE3_head    = IE2_head+8;
    IE4_head    = IE3_head+8;

    
    //Clear All Bit
    platform_zero_memory((PVOID)ptx_desc, sizeof(TX_DESC_88XX));


    if(IS_SUPPORT_TXDESC_IE(Adapter)){
    if (pdesc_data->secType != _NO_PRIVACY_) {
        if (pdesc_data->swCrypt == FALSE) {
            SetSecType_V2(Adapter, ptx_desc, pdesc_data);
            // for hw sec: 1) WEP's iv, 2) TKIP's iv and eiv, 3) CCMP's ccmp header are all in pdesc_data->iv
            TX_DESC_TXPKTSIZE += pdesc_data->iv;
        } else {
            // for sw sec
            TX_DESC_TXPKTSIZE += (pdesc_data->iv + pdesc_data->icv + pdesc_data->mic);
        }
    }

    //IE_body
    SET_TX_DESC_TXPKTSIZE_NO_CLR(ptx_desc, TX_DESC_TXPKTSIZE);
    SET_TX_DESC_OFFSET_NO_CLR(ptx_desc, TX_DESC_OFFSET); //only TXDESC, no burst mode, no HW AESIV, no TCP/IP checksum, no HW shortcut append MAC header 
    SET_TX_DESC_PKT_OFFSET_V1_NO_CLR(ptx_desc, (TXDESCSize-SIZE_TXDESC_BODY)/SIZE_TXDESC_IE);//IE_body will not be ripped off
    SET_TX_DESC_CHANNEL_DMA_NO_CLR(ptx_desc,TX_DESC_CHANNEL_DMA);
    SET_TX_DESC_IE_CNT_NO_CLR(ptx_desc,5);//IE0~IE4, no burst mode
    SET_TX_DESC_IE_CNT_EN_NO_CLR(ptx_desc,1);
    SET_TX_DESC_DMA_PRI_NO_CLR(ptx_desc,TX_DESC_DMA_PRI);

    //IE1
    IE1_SET_TX_DESC_BMC_NO_CLR(IE1_head, TX_DESC_BMC);
    
#if 1 //eric-8822 ?? tx hang
    if (TX_DESC_NDPA) {
        //printk("[%s] tid = %d, TX_DESC_P_AID = 0x%x SND_PKT_SEL = 0x%x \n", __FUNCTION__, 
            //pdesc_data->tid, TX_DESC_P_AID, SND_PKT_SEL);

        TX_DESC_RTY_LMT_EN = 1; 
        TX_DESC_DATA_RT_LMT = 0x10; //eric-txbf
        
        SET_TX_DESC_QSEL_NO_CLR(ptx_desc, TXDESC_QSEL_MGT);  //eric-mu 
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
		if (OFFLOAD_ENABLE(Adapter))
			SET_TX_DESC_OFFLOAD_SIZE_NO_CLR(ptx_desc, TX_DESC_OFFSET + TX_DESC_TXPKTSIZE);
#endif  
    }
    else
#endif
    {
    
#if IS_RTL88XX_MAC_V4
        if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4) {   
            SetTxDescQSel88XX_V3(Adapter, queueIndex, ptx_desc, pdesc_data->tid);        
            if ( (queueIndex >= HCI_TX_DMA_QUEUE_HI0_V2) && (queueIndex <= HCI_TX_DMA_QUEUE_HI15_V1) ) {
            #ifdef HW_ENC_FOR_GROUP_CIPHER
                SET_TX_DESC_MACID_V1_NO_CLR(ptx_desc,TX_DESC_MACID);

            #endif
            } else {
                SET_TX_DESC_MACID_V1_NO_CLR(ptx_desc,TX_DESC_MACID);
            } 
        
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
			if (OFFLOAD_ENABLE(Adapter))
				SET_TX_DESC_OFFLOAD_SIZE_NO_CLR(ptx_desc, TX_DESC_OFFSET + TX_DESC_TXPKTSIZE);
#endif        
        }
#endif //IS_RTL88XX_MAC_V3_V4
        }

        
        //SET_TX_DESC_RATE_ID_NO_CLR(ptx_desc, TX_DESC_RATE_ID); //yllin8814B, change to ARFR_Table_sel ??
        IE1_SET_TX_DESC_MOREDATA_NO_CLR(IE1_head, TX_DESC_MORE_DATA);    
        if (TX_DESC_EN_DESC_ID)
            IE1_SET_TX_DESC_EN_DESC_ID_NO_CLR(IE1_head, 1); 
    
        //4 Set Dword2
        SET_TX_DESC_AGG_EN_V1_NO_CLR(ptx_desc, TX_DESC_AGG_EN);
        SET_TX_DESC_BK_V1_NO_CLR(ptx_desc, TX_DESC_BK);
        IE1_SET_TX_DESC_MOREFRAG_NO_CLR(IE1_head, TX_DESC_MOREFRAG);
        IE1_SET_TX_DESC_AMPDU_DENSITY_NO_CLR(IE1_head, TX_DESC_AMPDU_DENSITY);
        IE1_SET_TX_DESC_P_AID_NO_CLR(IE1_head, TX_DESC_P_AID);
        IE3_SET_TX_DESC_G_ID_NO_CLR(IE3_head, TX_DESC_G_ID);

#if CFG_HAL_HW_AES_IV
        SET_TX_DESC_HW_AES_IV_V1_NO_CLR(ptx_desc, TX_DESC_HW_AES_IV);
#endif // CFG_HAL_HW_AES_IV

    	
#if 1 //eric-ac2
    	IE3_SET_TX_DESC_CCA_RTS_NO_CLR(IE3_head, TX_DESC_CCA_RTS);
#endif


        //4 Set Dword3
        IE0_SET_TX_DESC_USERATE_NO_CLR(IE0_head, TX_DESC_USERATE);
        IE0_SET_TX_DESC_DISRTSFB_NO_CLR(IE0_head, TX_DESC_DISRTSFB);
        IE0_SET_TX_DESC_DISDATAFB_NO_CLR(IE0_head, TX_DESC_DISDATAFB);

        IE0_SET_TX_DESC_CTS2SELF_NO_CLR(IE0_head, TX_DESC_CTS2SELF);
        IE0_SET_TX_DESC_RTS_EN_NO_CLR(IE0_head, TX_DESC_RTS_EN);

        IE0_SET_TX_DESC_HW_RTS_EN_NO_CLR(IE0_head, TX_DESC_HW_RTS_EN);
        IE1_SET_TX_DESC_NAVUSEHDR_NO_CLR(IE1_head, TX_DESC_NAVUSEHDR);
        IE1_SET_TX_DESC_MAX_AGG_NUM_NO_CLR(IE1_head, TX_DESC_MAX_AGG_NUM);
        IE3_SET_TX_DESC_NDPA_NO_CLR(IE3_head, TX_DESC_NDPA);    

    
        //4 Set Dword4
        IE0_SET_TX_DESC_DATARATE_NO_CLR(IE0_head, TX_DESC_DATERATE);    
    
        IE0_SET_TX_DESC_DATA_RTY_LOWEST_RATE_NO_CLR(IE0_head, TX_DESC_DATA_RATEFB_LMT);    
        IE0_SET_TX_DESC_RTS_RTY_LOWEST_RATE_NO_CLR(IE0_head, TX_DESC_RTS_RATEFB_LMT);        
        
#if 1 //eric-8822 tx hang
        //if(GET_TX_DESC_QSEL(ptx_desc) == TXDESC_QSEL_MGT)
        if(TX_DESC_BMC)
        IE0_SET_TX_DESC_RTS_RTY_LOWEST_RATE(IE0_head, 0);
#else
#if CFG_HAL_MULTICAST_BMC_ENHANCE
            if(TX_DESC_BMC) {
                SET_TX_DESC_DATA_RTY_LOWEST_RATE(ptx_desc, TX_DESC_BMCRtyLmt);
            }
#endif //#if CFG_HAL_MULTICAST_BMC_ENHANCE        
#endif
        IE0_SET_TX_DESC_RTY_LMT_EN_NO_CLR(IE0_head, TX_DESC_RTY_LMT_EN);    
        IE0_SET_TX_DESC_RTS_DATA_RTY_LMT_NO_CLR(IE0_head, TX_DESC_DATA_RT_LMT);    
        IE0_SET_TX_DESC_RTSRATE_NO_CLR(IE0_head, TX_DESC_RTSRATE);    
    
    
#if 1 //eric-8822 ?? No retry for broadcast
        if(TX_DESC_BMC){
            IE0_SET_TX_DESC_RTY_LMT_EN(IE0_head, 1);    
            IE0_SET_TX_DESC_RTS_DATA_RTY_LMT(IE0_head, 0);  
        }
#endif

    
    
        //4 Set Dword5
    
#if 0
        if(SND_PKT_SEL == 1) {
        SET_TX_DESC_DATA_SC_NO_CLR(ptx_desc, 9);    
        SET_TX_DESC_DATA_SHORT_NO_CLR(ptx_desc, TX_DESC_DATA_SHORT);    
        SET_TX_DESC_DATA_BW_NO_CLR(ptx_desc, 1); 
        }
        else
#endif
    {
        IE3_SET_TX_DESC_DATA_SC_NO_CLR(IE3_head, TX_DESC_DATA_SC);    
        IE0_SET_TX_DESC_DATA_SHORT_NO_CLR(IE0_head, TX_DESC_DATA_SHORT);    
        IE0_SET_TX_DESC_DATA_BW_NO_CLR(IE0_head, TX_DESC_DATA_BW);  
    }
        IE4_SET_TX_DESC_DATA_STBC_NO_CLR(IE4_head, TX_DESC_DATA_STBC);     
        IE4_SET_TX_DESC_DATA_LDPC_NO_CLR(IE4_head, TX_DESC_DATA_LDPC); 
        IE0_SET_TX_DESC_RTS_SHORT_NO_CLR(IE0_head, TX_DESC_RTS_SHORT); 
        //SET_TX_DESC_RTS_SC_NO_CLR(ptx_desc, TX_DESC_RTS_SC); //yllin8814B, change to Signaling_TA_PKT_SC ??
        //SET_TX_DESC_TXPWR_OFSET_NO_CLR(ptx_desc, TX_DESC_POWER_OFFSET);  //yllin8814B, no this field
    
#if (IS_EXIST_RTL8814BE)
        if ( IS_HARDWARE_TYPE_8814B(Adapter) )  {
        //if (Adapter->pmib->dot11RFEntry.bcn2path || Adapter->pmib->dot11RFEntry.tx2path)
            //SET_TX_DESC_TX_ANT_NO_CLR(ptx_desc, TX_ANT);  //yllin8814B, no this field
        }
#endif // (IS_EXIST_RTL8822BE || IS_EXIST_RTL8197FEM)
    
#if defined(AP_SWPS_OFFLOAD)
        if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter))
            IE2_SET_TX_DESC_DROP_ID(IE2_head,TX_DESC_DROP_ID);
#endif

        //4 Set Dword6
#if CFG_HAL_SUPPORT_MBSSID
        if (HAL_IS_VAP_INTERFACE(Adapter)) {
        // set MBSSID for each VAP_ID
        IE2_SET_TX_DESC_MBSSID_NO_CLR(IE2_head, HAL_VAR_VAP_INIT_SEQ);     
        }         
#endif //#if CFG_HAL_SUPPORT_MBSSID
    
#if defined(AP_SWPS_OFFLOAD)
        if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter))
            IE2_SET_TX_DESC_SW_DEFINE(IE2_head, TX_DESC_SWPS_SEQ);   
#endif

        //4 Set Dword7
        //SET_TX_DESC_TXDESC_CHECKSUM_V1_NO_CLR(ptx_desc, TX_DESC_TXBUFF);         //for shortcut use, keep this, hw will overwrite timestamp, its ok to fill len here.  yllin8814B
        SET_TX_DESC_TXDESC_CHECKSUM_V1_NO_CLR(ptx_desc, 0);
    
        //4 Set Dword8
        //4 Set Dword9
#ifdef CONFIG_RTL_OFFLOAD_DRIVER
		if (OFFLOAD_ENABLE(Adapter)) {
			IE1_SET_TX_DESC_EN_HWSEQ_NO_CLR(IE1_head, 1);
			IE1_SET_TX_DESC_EN_HWEXSEQ_NO_CLR(IE1_head, 0); 		
		} else
#endif
		{
#if CFG_HAL_HW_SEQ
        //use shortcut buffer seq
        IE1_SET_TX_DESC_EN_HWSEQ_NO_CLR(IE1_head, 0);
        IE1_SET_TX_DESC_EN_HWEXSEQ_NO_CLR(IE1_head, 1);
#else
        IE1_SET_TX_DESC_SW_SEQ_NO_CLR(IE1_head, TX_DESC_SEQ);
#endif
		}
    
#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
    if (IS_SUPPORT_WLAN_HAL_HW_TX_SHORTCUT_HDR_CONV(Adapter)) {
        //SET_TX_DESC_PKT_OFFSET_NO_CLR(ptx_desc, TX_DESC_PKT_OFFSET); //already filled
        SET_TX_DESC_WHEADER_LEN_V1_NO_CLR(ptx_desc, TX_DESC_WHEADER_LEN);
        SET_TX_DESC_MHR_CP_NO_CLR(ptx_desc, TX_DESC_MAC_CP);    
        SET_TX_DESC_SMH_EN_V1_NO_CLR(ptx_desc, TX_DESC_SMH_EN);
        if(TX_DESC_SMH_CAM != 0xFF)
            SET_TX_DESC_SMH_CAM_NO_CLR(ptx_desc,TX_DESC_SMH_CAM);

#if CFG_HAL_HW_TX_AMSDU
    if (IS_SUPPORT_WLAN_HAL_HW_TX_AMSDU(Adapter)) {
        SET_TX_DESC_AMSDU_NO_CLR(ptx_desc, TX_DESC_HW_AMSDU);
        SET_TX_DESC_MAX_AMSDU_MODE(ptx_desc,TX_DESC_HW_AMSDU_SIZE );
    }
#endif
    }
#endif
    
#if IS_EXIST_RTL8814BE //yllin8814B, do we need this??
        if ( IS_HARDWARE_TYPE_8814B(Adapter)) {
       	    if (TX_DESC_NDPA) {
                if(SND_PKT_SEL) {
                    IE3_SET_TX_DESC_SND_PKT_SEL_NO_CLR(IE3_head, SND_PKT_SEL);
                }
                IE3_SET_TX_DESC_SND_TARGET(IE3_head, SND_TARGET);
       	    }
        
            IE3_SET_TX_DESC_MU_DATARATE_NO_CLR(IE3_head, TX_DESC_DATERATE);  
			IE2_SET_TX_DESC_SPE_RPT_NO_CLR(IE2_head, IS_GID);
        }
#endif

	if(IS_GID)
		IE2_SET_TX_DESC_SW_DEFINE_NO_CLR(IE2_head, 2);
	else if(TX_DESC_USERATE)
		IE2_SET_TX_DESC_SW_DEFINE_NO_CLR(IE2_head, 1);
		

#ifdef AP_SWPS_OFFLOAD
        if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter)){

            tx_head     = &(cur_q->host_idx);
            pswdescinfo = get_txdesc_info(Adapter,Adapter->pshare->pdesc_info, dma_queueIndex);
            pdescinfo = pswdescinfo + *tx_head;  
            pdescinfo->IE_bitmap = BIT0 | BIT1 | BIT2; //IE2+IE3+IE4
                
            if(pdescinfo->ptxdesc == NULL)
                pdescinfo->ptxdesc = kmalloc(sizeof(TX_DESC_88XX),GFP_KERNEL);
            
            memcpy((PVOID)pdescinfo->ptxdesc, (PVOID)ptx_desc, TXDESCSize);
        }
#endif

        //3 this function will be called when go slow path, should update record in ctrlinfo
        IE0_SET_TX_DESC_IE_UP(IE0_head,1);
        IE0_SET_TX_DESC_IE_NUM(IE0_head,0);
        IE1_SET_TX_DESC_IE_UP(IE1_head,1);
        IE1_SET_TX_DESC_IE_NUM(IE1_head,1);
        IE2_SET_TX_DESC_IE_UP(IE2_head,1);
        IE2_SET_TX_DESC_IE_NUM(IE2_head,2);
        IE3_SET_TX_DESC_IE_UP(IE3_head,1);
        IE3_SET_TX_DESC_IE_NUM(IE3_head,3);
        IE4_SET_TX_DESC_IE_UP(IE4_head,1);
        IE4_SET_TX_DESC_IE_NUM(IE4_head,4); 
        IE4_SET_TX_DESC_IE_END(IE4_head,1); 


    }

#endif //SUPPORT_TXDESC_IE    
}

HAL_IMEM
VOID
FillShortCutTxDesc88XX_V2(
    IN      HAL_PADAPTER    Adapter,
    IN      u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN      PVOID           pDescData,
    IN      PVOID           pTxDesc
)
{
    PTX_DESC_DATA_88XX  pdesc_data  = (PTX_DESC_DATA_88XX)pDescData;
    PTX_DESC_88XX       ptx_desc    = (PTX_DESC_88XX)pTxDesc;
    PHCI_TX_DMA_QUEUE_STRUCT_88XX   cur_q;
    PHCI_TX_DMA_MANAGER_88XX        ptx_dma;
    // tx shortcut can reuse TXDESC while 1) no security or 2) hw security
    // if no security iv == 0, so adding iv is ok for no security and hw security
    u2Byte  TX_DESC_TXPKTSIZE   = pdesc_data->hdrLen + pdesc_data->llcLen + pdesc_data->frLen + pdesc_data->iv;
    BOOLEAN TX_DESC_BK          = pdesc_data->bk;
    BOOLEAN TX_DESC_NAVUSEHDR   = pdesc_data->navUseHdr;
    u2Byte  TX_DESC_SEQ         = GetSequence(pdesc_data->pHdr);
    u1Byte  TX_DESC_DATA_STBC   = pdesc_data->dataStbc;
    BOOLEAN TX_DESC_RTY_LMT_EN  = pdesc_data->rtyLmtEn;   
    u1Byte  TX_DESC_DATA_RT_LMT = pdesc_data->dataRtyLmt;
    u4Byte  TXDESCSize;
    u1Byte  TX_DESC_OFFSET; 
    u2Byte                          *tx_head;
    struct tx_desc_info *pswdescinfo, *pdescinfo;
    u1Byte*             IE0_head;
    u1Byte*             IE1_head;
    u1Byte*             IE2_head;
    u1Byte*             IE3_head;
    u1Byte*             IE4_head;
    u1Byte  IE_cnt = 2;//default need IE0,IE1
    u1Byte  need_IE2 = 0;
    u1Byte  need_IE3 = 0;
    u1Byte  need_IE4 = 0;
    u4Byte              dma_queueIndex;

#if CFG_HAL_HW_TX_SHORTCUT_HDR_CONV
    BOOLEAN TX_DESC_BMC              = (pdesc_data->smhEn == TRUE) ? 
                        ((HAL_IS_MCAST(GetEthDAPtr((pu1Byte)pdesc_data->pHdr))) ? 1 : 0) : 
                        ((HAL_IS_MCAST(GetAddr1Ptr((pu1Byte)pdesc_data->pHdr))) ? 1 : 0);   // when multicast or broadcast, BMC = 1
#else
    BOOLEAN TX_DESC_BMC              = (HAL_IS_MCAST(GetAddr1Ptr((pu1Byte)pdesc_data->pHdr))) ? 1 : 0;   // when multicast or broadcast, BMC = 1        
#endif

#if CFG_HAL_SUPPORT_TXDESC_IE

    u1Byte  TX_DESC_CHANNEL_DMA;


#if (BEAMFORMING_SUPPORT == 1)
    need_IE4 = 1;
#endif
#if defined(AP_SWPS_OFFLOAD)
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter))
        need_IE2 = 1;
#endif

    IE0_head = ((u1Byte*)ptx_desc)+16;
    IE1_head = IE0_head+8;
    TXDESCSize = 16+8+8;//IE_body+IE0+IE1

    if (need_IE2){
        IE2_head = IE1_head+8;
        IE_cnt++;
        TXDESCSize+=8;
    }
    else
        IE2_head = IE1_head; //then, should not fill IE2

    if(need_IE3){
        IE3_head = IE2_head+8;
        IE_cnt++;
        TXDESCSize+=8;
    }
    else
        IE3_head = IE2_head; //then, should not fill IE3

    if(need_IE4){
        IE4_head = IE3_head+8;
        IE_cnt++;
        TXDESCSize+=8;
    }
    else
        IE4_head = IE3_head; //then, should not fill IE4
        
    //Dword5
#if defined(AP_SWPS_OFFLOAD)
    u1Byte  TX_DESC_DROP_ID          = pdesc_data->DropID;
#endif
    
    //Dword6
#if defined(AP_SWPS_OFFLOAD)
    u2Byte  TX_DESC_SWPS_SEQ         = pdesc_data->SWPS_sequence;
#endif    


#if (IS_RTL88XX_MAC_V4)
    if (_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v4)  {
        TX_DESC_OFFSET = SIZE_TXDESC_88XX_V1; 
        switch(queueIndex) //temp set DMA channel by VO/VI/BE/BK //yllin8814B
        {
            case HCI_TX_DMA_QUEUE_BK_V2:
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL3;
            break;
            case HCI_TX_DMA_QUEUE_BE_V2:   
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL2;
            break;
            case HCI_TX_DMA_QUEUE_VI_V2:
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL1;
            break;
            case HCI_TX_DMA_QUEUE_VO_V2:
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL0;
            break;
            case HCI_TX_DMA_QUEUE_MGT_V2: 
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL14;
            break;    
            case HCI_TX_DMA_QUEUE_CMD_V2:
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL20;
            break;    
            case HCI_TX_DMA_QUEUE_FWCMD:
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL16;
            break;             
            case HCI_TX_DMA_QUEUE_BCN_V2:
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL19;
            break;              
            case HCI_TX_DMA_QUEUE_HI0_V2: 
            case HCI_TX_DMA_QUEUE_HI1_V2:     
            case HCI_TX_DMA_QUEUE_HI2_V2: 
            case HCI_TX_DMA_QUEUE_HI3_V2: 
            case HCI_TX_DMA_QUEUE_HI4_V2: 
            case HCI_TX_DMA_QUEUE_HI5_V2:     
            case HCI_TX_DMA_QUEUE_HI6_V2: 
            case HCI_TX_DMA_QUEUE_HI7_V2: 
            case HCI_TX_DMA_QUEUE_HI8_V1: 
            case HCI_TX_DMA_QUEUE_HI9_V1:     
            case HCI_TX_DMA_QUEUE_HI10_V1: 
            case HCI_TX_DMA_QUEUE_HI11_V1: 
            case HCI_TX_DMA_QUEUE_HI12_V1:
            case HCI_TX_DMA_QUEUE_HI13_V1: 
            case HCI_TX_DMA_QUEUE_HI14_V1: 
            case HCI_TX_DMA_QUEUE_HI15_V1:                
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL15;
            break;    
            default: //take as BE
                pdesc_data->dma_channel = TXDESC_DMA_CHANNEL2;
            break;
        }

    }
#endif //(IS_RTL88XX_MAC_V4)

    TX_DESC_CHANNEL_DMA = pdesc_data->dma_channel;


    //IE_body
    SET_TX_DESC_TXPKTSIZE(ptx_desc, TX_DESC_TXPKTSIZE);
    SET_TX_DESC_OFFSET(ptx_desc, TX_DESC_OFFSET); 
    SET_TX_DESC_PKT_OFFSET_V1(ptx_desc, (TXDESCSize-SIZE_TXDESC_BODY)/SIZE_TXDESC_IE);//IE_body will not be ripped off
    SET_TX_DESC_CHANNEL_DMA(ptx_desc,TX_DESC_CHANNEL_DMA);
    SET_TX_DESC_IE_CNT(ptx_desc,IE_cnt);
    SET_TX_DESC_IE_CNT_EN(ptx_desc,1);

    SET_TX_DESC_BK_V1(ptx_desc, TX_DESC_BK);
    IE1_SET_TX_DESC_NAVUSEHDR(IE1_head, TX_DESC_NAVUSEHDR);
    IE1_SET_TX_DESC_SW_SEQ(IE1_head, TX_DESC_SEQ);

    if (TX_DESC_RTY_LMT_EN) {
        IE0_SET_TX_DESC_RTY_LMT_EN(IE0_head, TX_DESC_RTY_LMT_EN);    
        IE0_SET_TX_DESC_RTS_DATA_RTY_LMT(IE0_head, TX_DESC_DATA_RT_LMT);  
    } else if (TX_DESC_DATA_RT_LMT) {
        IE0_SET_TX_DESC_RTY_LMT_EN(IE0_head, 0);    
        IE0_SET_TX_DESC_RTS_DATA_RTY_LMT(IE0_head, 0);  
    }

#if 1 /*eric-8822 ??  No retry for broadcast & avoid tx hang */
	if(TX_DESC_BMC){
		IE0_SET_TX_DESC_DATA_RTY_LOWEST_RATE(IE0_head, 0);
		IE0_SET_TX_DESC_RTY_LMT_EN(IE0_head, 1);    
        IE0_SET_TX_DESC_RTS_DATA_RTY_LMT(IE0_head, 0);  
	}
#endif


    // for force tx rate
    if (HAL_VAR_TX_FORCE_RATE != 0xff) {       
        IE0_SET_TX_DESC_USERATE(IE0_head, pdesc_data->useRate);
        IE0_SET_TX_DESC_DISRTSFB(IE0_head, pdesc_data->disRTSFB);
        IE0_SET_TX_DESC_DISDATAFB(IE0_head, pdesc_data->disDataFB);
        IE0_SET_TX_DESC_DATARATE(IE0_head, pdesc_data->dataRate);
    }

#if (BEAMFORMING_SUPPORT == 1)
    IE4_SET_TX_DESC_DATA_STBC(IE4_head, TX_DESC_DATA_STBC);     
#endif
	//4 Set Dword5
#if defined(AP_SWPS_OFFLOAD)
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter))
        IE2_SET_TX_DESC_DROP_ID(IE2_head,TX_DESC_DROP_ID);
#endif	
    //4 Set Dword6
#if defined(AP_SWPS_OFFLOAD)
    if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter))
        IE2_SET_TX_DESC_SW_DEFINE(IE2_head, TX_DESC_SWPS_SEQ);   
#endif

    //3 no update record, to follow old flow
    //IE0_SET_TX_DESC_IE_UP(IE0_head,1);        
    IE0_SET_TX_DESC_IE_NUM(IE0_head,0);        
    //IE1_SET_TX_DESC_IE_UP(IE1_head,1);        
    IE1_SET_TX_DESC_IE_NUM(IE1_head,1);        
    if(need_IE2){
        //IE2_SET_TX_DESC_IE_UP(IE2_head,1);        
        IE2_SET_TX_DESC_IE_NUM(IE2_head,2);
    }
    if(need_IE3){
        //IE3_SET_TX_DESC_IE_UP(IE3_head,1);        
        IE3_SET_TX_DESC_IE_NUM(IE3_head,3);
    }
    if(need_IE4){
        //IE4_SET_TX_DESC_IE_UP(IE4_head,1);        
        IE4_SET_TX_DESC_IE_NUM(IE4_head,4);         
    }

    if(need_IE4)
        IE4_SET_TX_DESC_IE_END(IE4_head,1);

    else if((!need_IE4) && need_IE3)
        IE3_SET_TX_DESC_IE_END(IE3_head,1);
    
    else if((!need_IE4) && (!need_IE3) && need_IE2)
        IE2_SET_TX_DESC_IE_END(IE2_head,1);
    
    else
        IE1_SET_TX_DESC_IE_END(IE1_head,1);
    
        
#ifdef TRXBD_CACHABLE_REGION

#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
    // With CCI400 ,Do nothing for cache coherent code
#else
    rtl_cache_sync_wback(Adapter,(unsigned long)((PVOID)ptx_desc-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), TXDESCSize,PCI_DMA_TODEVICE);
#endif //CONFIG_ENABLE_CCI400
#else
    _dma_cache_wback((unsigned long)((PVOID)ptx_desc-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), TXDESCSize);
#endif //CONFIG_RTL_8198F
#endif //#ifdef TRXBD_CACHABLE_REGION



#if defined(AP_SWPS_OFFLOAD)

        {
            dma_queueIndex = queueIndex;

        }

    if (IS_SUPPORT_AP_SWPS_OFFLOAD(Adapter)){

        ptx_dma     = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);
        cur_q       = &(ptx_dma->tx_queue[dma_queueIndex]);
        tx_head     = &(cur_q->host_idx);
        pswdescinfo = get_txdesc_info(Adapter,Adapter->pshare->pdesc_info, dma_queueIndex);
        pdescinfo = pswdescinfo + *tx_head;  

        pdescinfo->IE_bitmap = BIT0; //IE2
        
#if (BEAMFORMING_SUPPORT == 1)
        pdescinfo->IE_bitmap = pdescinfo->IE_bitmap | BIT2; //IE4
#endif

        if(pdescinfo->ptxdesc == NULL)
            pdescinfo->ptxdesc = kmalloc(sizeof(TX_DESC_88XX),GFP_KERNEL);
        
        memcpy((PVOID)pdescinfo->ptxdesc, (PVOID)ptx_desc, TXDESCSize);
    }
#endif    


#endif //#ifdef SUPPORT_TXDESC_IE

}


VOID
SetTxDescQSel88XX_V3(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN  PTX_DESC_88XX   ptx_desc,
    IN  u1Byte          drvTID
)
{
    u1Byte  q_select;
#if CFG_HAL_SUPPORT_TXDESC_IE    
    u1Byte* IE1_head = ptx_desc+16+8;
#endif    
	switch (queueIndex) {    	
        case HCI_TX_DMA_QUEUE_CMD_V2:
            q_select = TXDESC_QSEL_CMD;
            break;     
    	case HCI_TX_DMA_QUEUE_MGT_V2:
    		q_select = TXDESC_QSEL_MGT;
    		break;
//swps test need fill vo vi be bk qsel            
#if (CFG_HAL_MAC_LOOPBACK && CFG_HAL_WIFI_WMM) || defined(AP_SWPS_OFFLOAD_VERI) || defined(CONFIG_8814_AP_MAC_VERI)
    	case HCI_TX_DMA_QUEUE_BE_V2:
    		q_select = TXDESC_QSEL_TID0;
    		break;
        case HCI_TX_DMA_QUEUE_VO_V2:
			q_select = TXDESC_QSEL_TID6;
			break;
		case HCI_TX_DMA_QUEUE_VI_V2:
			q_select = TXDESC_QSEL_TID4;
    		break;
    	case HCI_TX_DMA_QUEUE_BK_V2:
			q_select = TXDESC_QSEL_TID1;
			break;         
#endif  //CFG_HAL_MAC_LOOPBACK && CFG_HAL_WIFI_WMM

    	default:
    		// data packet
#if CFG_HAL_RTL_MANUAL_EDCA
    		if (HAL_VAR_MANUAL_EDCA) {
    			switch (queueIndex) {
        			case HCI_TX_DMA_QUEUE_VO_V2:
        				q_select = TXDESC_QSEL_TID6;
        				break;
        			case HCI_TX_DMA_QUEUE_VI_V2:
        				q_select = TXDESC_QSEL_TID4;
                		break;
        			case HCI_TX_DMA_QUEUE_BE_V2:
        				q_select = TXDESC_QSEL_TID0;
        	    		break;
        		    default:
        				q_select = TXDESC_QSEL_TID1;
        				break;
    			}
    		}
    		else {
                q_select = drvTID;
    		}
#else
            q_select = drvTID;
#endif  //CFG_HAL_RTL_MANUAL_EDCA
           // HIQ packet
            if((queueIndex >= HCI_TX_DMA_QUEUE_HI0_V2) && (queueIndex<=HCI_TX_DMA_QUEUE_HI15_V1)) {
#if CFG_HAL_SUPPORT_TXDESC_IE
                if(IS_SUPPORT_TXDESC_IE(Adapter)){
                    IE1_SET_TX_DESC_MOREDATA(IE1_head,1);                        
                }
#endif            
                q_select = TXDESC_QSEL_HIGH;
            }               
            break;
	}    
    SET_TX_DESC_QSEL(ptx_desc,q_select);    
}
#endif //#if IS_RTL88XX_MAC_V4



#if IS_RTL88XX_MAC_V3_V4
// TODO: 
//Note: PrepareTXBD88XX is necessary to be done after calling PrepareRXBD88XX
enum rt_status
PrepareTXBD88XX_V1(
    IN      HAL_PADAPTER Adapter
)
{    
    PHCI_RX_DMA_MANAGER_88XX    prx_dma;
    PHCI_TX_DMA_MANAGER_88XX    ptx_dma;
    PTX_BUFFER_DESCRIPTOR       ptxbd_head;
    PTX_DESC_88XX               ptx_desc_head;
    PTX_BUFFER_DESCRIPTOR       ptxbd;
    
    PTX_BUFFER_DESCRIPTOR       ptxbd_bcn_head;
    PTX_DESC_88XX               ptxdesc_bcn_head;
    PTX_BUFFER_DESCRIPTOR       ptxbd_bcn_cur;    

    PTX_DESC_88XX               ptx_desc;

    
    u4Byte                      q_num;
    pu4Byte                     pTXBD_NUM;
    u4Byte                      i;
    u4Byte                      beacon_offset;
    u4Byte                      TotalTXBDNum_NoBcn;
    u4Byte                      TXDESCSize;
    u4Byte                      HCI_TX_DMA_QUEUE_MAX;
    u4Byte                      TXBD_RWPtr_Reg_CMDQ;
    u4Byte                      TXBD_Reg_CMDQ;    
    u4Byte                      DMA_MAX_QUEUE_NUM;  
    u4Byte                      DMA_QUEUE_BCN;      
    u4Byte                      DMA_QUEUE_CMD;
    u4Byte                      DMA_QUEUE_FWCMD;
    u4Byte                      TXBD_Reg_CMDQ_L,TXBD_Reg_CMDQ_H;  
    u4Byte                      bcn_TXDESCSize;
    pu4Byte                     pTXBD_RWPtr_Reg;
    pu4Byte                     pTXBD_Reg;

    //HCI_TX_DMA_QUEUE_MAX = HCI_TX_DMA_QUEUE_MAX_NUM;
        
#if CFG_HAL_TX_AMSDU
    pu1Byte                         pdesc_dma_buf_amsdu, desc_dma_buf_start_amsdu;
    PHCI_TX_AMSDU_DMA_MANAGER_88XX  ptx_dma_amsdu;
    PTX_BUFFER_DESCRIPTOR_AMSDU     ptxbd_head_amsdu;
#endif

#ifdef WLAN_SUPPORT_H2C_PACKET
    PH2C_PAYLOAD_88XX   h2c_buf_start = (PH2C_PAYLOAD_88XX)_GET_HAL_DATA(Adapter)->h2c_buf;
    //pu1Byte  h2c_buf_start = _GET_HAL_DATA(Adapter)->h2c_buf;
    PTX_DESC_88XX               ph2c_tx_desc;
#endif //WLAN_SUPPORT_H2C_PACKET

#ifdef WLAN_SUPPORT_FW_CMD
    PFWCMD_PAYLOAD_88XX   fwcmd_buf_start = (PFWCMD_PAYLOAD_88XX)_GET_HAL_DATA(Adapter)->fwcmd_buf;
    //pu1Byte  h2c_buf_start = _GET_HAL_DATA(Adapter)->h2c_buf;
    PTX_DESC_88XX               pfwcmd_tx_desc;
#endif //WLAN_SUPPORT_FW_CMD


#if IS_EXIST_RTL8814BE
        if ( IS_HARDWARE_TYPE_8814B(Adapter))  {
            TXBD_RWPtr_Reg_CMDQ = REG_H2CQ_TXBD_IDX;
            TXBD_Reg_CMDQ_L       = REG_H2CQ_TXBD_DESA_L;
            TXBD_Reg_CMDQ_H       = REG_H2CQ_TXBD_DESA_H;
        }
#endif //IS_EXIST_RTL8814BE

#if IS_EXIST_RTL8198FEM || IS_EXIST_RTL8197GEM
        if ( IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter))  {
            TXBD_RWPtr_Reg_CMDQ = REG_H2CQ_TXBD_IDX;
            TXBD_Reg_CMDQ       = REG_H2CQ_TXBD_DESA;
        }
#endif //IS_EXIST_RTL8198FEM || IS_EXIST_RTL8197GEM

#if (IS_EXIST_RTL8814BE)
        u4Byte TXBD_NUM_V3[HCI_TX_DMA_QUEUE_MAX_NUM_V2] =
        {
            TX_MGQ_TXBD_NUM_8814B,   TX_BKQ_TXBD_NUM_8814B,   TX_BEQ_TXBD_NUM_8814B,   TX_VIQ_TXBD_NUM_8814B, 
            TX_VOQ_TXBD_NUM_8814B,   TX_ACH4_TXBD_NUM_8814B,    TX_ACH5_TXBD_NUM_8814B, TX_ACH6_TXBD_NUM_8814B,
            TX_ACH7_TXBD_NUM_8814B,   TX_ACH8_TXBD_NUM_8814B,   TX_ACH9_TXBD_NUM_8814B,   TX_ACH10_TXBD_NUM_8814B,
            TX_ACH11_TXBD_NUM_8814B,  TX_ACH12_TXBD_NUM_8814B,  TX_ACH13_TXBD_NUM_8814B,  TX_HI0Q_TXBD_NUM_8814B,  
            TX_HI1Q_TXBD_NUM_8814B,  TX_HI2Q_TXBD_NUM_8814B,    TX_HI3Q_TXBD_NUM_8814B,   TX_HI4Q_TXBD_NUM_8814B,  
            TX_HI5Q_TXBD_NUM_8814B,  TX_HI6Q_TXBD_NUM_8814B,    TX_HI7Q_TXBD_NUM_8814B,  TX_HI8Q_TXBD_NUM_8814B,  
            TX_HI9Q_TXBD_NUM_8814B,  TX_HI10Q_TXBD_NUM_8814B, TX_HI11Q_TXBD_NUM_8814B, TX_HI12Q_TXBD_NUM_8814B, 
            TX_HI13Q_TXBD_NUM_8814B, TX_HI14Q_TXBD_NUM_8814B, TX_HI15Q_TXBD_NUM_8814B, TX_FWCMDQ_TXBD_NUM_8814B,
            TX_CMDQ_TXBD_NUM_8814B,  TX_BCNQ_TXBD_NUM_V1
        };
#endif  //IS_EXIST_RTL8814BE 


#if (IS_EXIST_RTL8198FEM)
        u4Byte TXBD_NUM_V2[HCI_TX_DMA_QUEUE_MAX_NUM_V1] =
        {
            TX_MGQ_TXBD_NUM_8198F,   TX_BKQ_TXBD_NUM_8198F,   TX_BEQ_TXBD_NUM_8198F,   TX_VIQ_TXBD_NUM_8198F, 
            TX_VOQ_TXBD_NUM_8198F,   NUM_TX_DESC_HQ_8198F,  NUM_TX_DESC_HQ_8198F,  NUM_TX_DESC_HQ_8198F, 
            NUM_TX_DESC_HQ_8198F,  NUM_TX_DESC_HQ_8198F,  NUM_TX_DESC_HQ_8198F,  NUM_TX_DESC_HQ_8198F, 
            NUM_TX_DESC_HQ_8198F,  NUM_TX_DESC_HQ_8198F,  NUM_TX_DESC_HQ_8198F,  NUM_TX_DESC_HQ_8198F, 
            NUM_TX_DESC_HQ_8198F, NUM_TX_DESC_HQ_8198F, NUM_TX_DESC_HQ_8198F, NUM_TX_DESC_HQ_8198F, 
            NUM_TX_DESC_HQ_8198F, TX_CMDQ_TXBD_NUM_8198F,  TX_BCNQ_TXBD_NUM_V1
        };
#endif  //IS_EXIST_RTL8198FEM 
    
 #if (IS_EXIST_RTL8197GEM)
        u4Byte TXBD_NUM_V4[HCI_TX_DMA_QUEUE_MAX_NUM] =
        {
            TX_MGQ_TXBD_NUM_8197G,   TX_BKQ_TXBD_NUM_8197G,   TX_BEQ_TXBD_NUM_8197G,   TX_VIQ_TXBD_NUM_8197G, 
            TX_VOQ_TXBD_NUM_8197G,   NUM_TX_DESC_HQ_8197G,  NUM_TX_DESC_HQ_8197G,  NUM_TX_DESC_HQ_8197G, 
            NUM_TX_DESC_HQ_8197G,  NUM_TX_DESC_HQ_8197G,  NUM_TX_DESC_HQ_8197G,  NUM_TX_DESC_HQ_8197G, 
            NUM_TX_DESC_HQ_8197G,  TX_CMDQ_TXBD_NUM_8197G,  TX_BCNQ_TXBD_NUM_V1
        };
#endif  //IS_EXIST_RTL8197GEM  
   
#if (IS_EXIST_RTL8814BE)
    // TXBD_RWPtr_Reg: no entry for beacon queue, set NULL here
    u4Byte  TXBD_RWPtr_Reg_V1[HCI_TX_DMA_QUEUE_MAX_NUM_V2] =
    {
            REG_P0MGQ_TXBD_IDX,
            REG_ACH3_TXBD_IDX, REG_ACH2_TXBD_IDX, REG_ACH1_TXBD_IDX, REG_ACH0_TXBD_IDX,
            REG_ACH4_TXBD_IDX, REG_ACH5_TXBD_IDX, REG_ACH6_TXBD_IDX, REG_ACH7_TXBD_IDX,
            REG_ACH8_TXBD_IDX, REG_ACH9_TXBD_IDX, REG_ACH10_TXBD_IDX, REG_ACH11_TXBD_IDX,
            REG_ACH12_TXBD_IDX, REG_ACH13_TXBD_IDX,
            REG_P0HI0Q_TXBD_IDX, REG_P0HI1Q_TXBD_IDX, REG_P0HI2Q_TXBD_IDX, REG_P0HI3Q_TXBD_IDX,
            REG_P0HI4Q_TXBD_IDX, REG_P0HI5Q_TXBD_IDX, REG_P0HI6Q_TXBD_IDX, REG_P0HI7Q_TXBD_IDX,        
            REG_P0HI8Q_TXBD_IDX, REG_P0HI9Q_TXBD_IDX, REG_P0HI10Q_TXBD_IDX, REG_P0HI11Q_TXBD_IDX,
            REG_P0HI12Q_TXBD_IDX, REG_P0HI13Q_TXBD_IDX, REG_P0HI14Q_TXBD_IDX, REG_P0HI15Q_TXBD_IDX, 
            REG_FWCMDQ_TXBD_IDX, TXBD_RWPtr_Reg_CMDQ,0
        };

    u4Byte  TXBD_Reg_H[HCI_TX_DMA_QUEUE_MAX_NUM_V2] =
    {
            REG_P0MGQ_TXBD_DESA_H,
            REG_ACH3_TXBD_DESA_H, REG_ACH2_TXBD_DESA_H, REG_ACH1_TXBD_DESA_H, REG_ACH0_TXBD_DESA_H,
            REG_ACH4_TXBD_DESA_H, REG_ACH5_TXBD_DESA_H, REG_ACH6_TXBD_DESA_H, REG_ACH7_TXBD_DESA_H,
            REG_ACH8_TXBD_DESA_H, REG_ACH9_TXBD_DESA_H, REG_ACH10_TXBD_DESA_H, REG_ACH11_TXBD_DESA_H,
            REG_ACH12_TXBD_DESA_H, REG_ACH13_TXBD_DESA_H,
            REG_HI0Q_TXBD_DESA_H, REG_HI1Q_TXBD_DESA_H, REG_HI2Q_TXBD_DESA_H, REG_HI3Q_TXBD_DESA_H,
            REG_HI4Q_TXBD_DESA_H, REG_HI5Q_TXBD_DESA_H, REG_HI6Q_TXBD_DESA_H, REG_HI7Q_TXBD_DESA_H,
            REG_HI8Q_TXBD_DESA_H, REG_HI9Q_TXBD_DESA_H, REG_HI10Q_TXBD_DESA_H, REG_HI11Q_TXBD_DESA_H,
            REG_HI12Q_TXBD_DESA_H, REG_HI13Q_TXBD_DESA_H, REG_HI14Q_TXBD_DESA_H, REG_HI15Q_TXBD_DESA_H,
            REG_FWCMDQ_TXBD_DESA_H, TXBD_Reg_CMDQ_H,REG_P0BCNQ_TXBD_DESA_H
        };

    u4Byte  TXBD_Reg_L[HCI_TX_DMA_QUEUE_MAX_NUM_V2] =
    {
            REG_P0MGQ_TXBD_DESA_L,
            REG_ACH3_TXBD_DESA_L, REG_ACH2_TXBD_DESA_L, REG_ACH1_TXBD_DESA_L, REG_ACH0_TXBD_DESA_L,
            REG_ACH4_TXBD_DESA_L, REG_ACH5_TXBD_DESA_L, REG_ACH6_TXBD_DESA_L, REG_ACH7_TXBD_DESA_L,
            REG_ACH8_TXBD_DESA_L, REG_ACH9_TXBD_DESA_L, REG_ACH10_TXBD_DESA_L, REG_ACH11_TXBD_DESA_L,
            REG_ACH12_TXBD_DESA_L, REG_ACH13_TXBD_DESA_L,            
            REG_HI0Q_TXBD_DESA_L, REG_HI1Q_TXBD_DESA_L, REG_HI2Q_TXBD_DESA_L, REG_HI3Q_TXBD_DESA_L,
            REG_HI4Q_TXBD_DESA_L, REG_HI5Q_TXBD_DESA_L, REG_HI6Q_TXBD_DESA_L, REG_HI7Q_TXBD_DESA_L,
            REG_HI8Q_TXBD_DESA_L, REG_HI9Q_TXBD_DESA_L, REG_HI10Q_TXBD_DESA_L, REG_HI11Q_TXBD_DESA_L,
            REG_HI12Q_TXBD_DESA_L, REG_HI13Q_TXBD_DESA_L, REG_HI14Q_TXBD_DESA_L, REG_HI15Q_TXBD_DESA_L,
            REG_FWCMDQ_TXBD_DESA_L, TXBD_Reg_CMDQ_L,REG_P0BCNQ_TXBD_DESA_L
        };
#endif
#if (IS_EXIST_RTL8198FEM)

    // TXBD_RWPtr_Reg: no entry for beacon queue, set NULL here
    u4Byte  TXBD_RWPtr_Reg[HCI_TX_DMA_QUEUE_MAX_NUM_V1] =
    {
            REG_MGQ_TXBD_IDX,
            REG_BKQ_TXBD_IDX, REG_BEQ_TXBD_IDX, REG_VIQ_TXBD_IDX, REG_VOQ_TXBD_IDX,
            REG_HI0Q_TXBD_IDX, REG_HI1Q_TXBD_IDX, REG_HI2Q_TXBD_IDX, REG_HI3Q_TXBD_IDX,
            REG_HI4Q_TXBD_IDX, REG_HI5Q_TXBD_IDX, REG_HI6Q_TXBD_IDX, REG_HI7Q_TXBD_IDX,        
            REG_HI8Q_TXBD_IDX, REG_HI9Q_TXBD_IDX, REG_HI10Q_TXBD_IDX, REG_HI11Q_TXBD_IDX,
            REG_HI12Q_TXBD_IDX, REG_HI13Q_TXBD_IDX, REG_HI14Q_TXBD_IDX, REG_HI15Q_TXBD_IDX, 
            TXBD_RWPtr_Reg_CMDQ,0
        };

    u4Byte  TXBD_Reg[HCI_TX_DMA_QUEUE_MAX_NUM_V1] =
    {
            REG_MGQ_TXBD_DESA,
            REG_BKQ_TXBD_DESA, REG_BEQ_TXBD_DESA, REG_VIQ_TXBD_DESA, REG_VOQ_TXBD_DESA,
            REG_HI0Q_TXBD_DESA, REG_HI1Q_TXBD_DESA, REG_HI2Q_TXBD_DESA, REG_HI3Q_TXBD_DESA,
            REG_HI4Q_TXBD_DESA, REG_HI5Q_TXBD_DESA, REG_HI6Q_TXBD_DESA, REG_HI7Q_TXBD_DESA,
            REG_HI8Q_TXBD_DESA, REG_HI9Q_TXBD_DESA, REG_HI10Q_TXBD_DESA, REG_HI11Q_TXBD_DESA,
            REG_HI12Q_TXBD_DESA, REG_HI13Q_TXBD_DESA, REG_HI14Q_TXBD_DESA, REG_HI15Q_TXBD_DESA,
            TXBD_Reg_CMDQ,REG_BCNQ_TXBD_DESA
        };
#endif

#if (IS_EXIST_RTL8197GEM)

    // TXBD_RWPtr_Reg: no entry for beacon queue, set NULL here
    u4Byte  TXBD_RWPtr_Reg_V2[HCI_TX_DMA_QUEUE_MAX_NUM] =
    {
            REG_MGQ_TXBD_IDX,
            REG_BKQ_TXBD_IDX, REG_BEQ_TXBD_IDX, REG_VIQ_TXBD_IDX, REG_VOQ_TXBD_IDX,
            REG_HI0Q_TXBD_IDX, REG_HI1Q_TXBD_IDX, REG_HI2Q_TXBD_IDX, REG_HI3Q_TXBD_IDX,
            REG_HI4Q_TXBD_IDX, REG_HI5Q_TXBD_IDX, REG_HI6Q_TXBD_IDX, REG_HI7Q_TXBD_IDX,         
            TXBD_RWPtr_Reg_CMDQ,0
        };

    u4Byte  TXBD_Reg_V1[HCI_TX_DMA_QUEUE_MAX_NUM_V1] =
    {
            REG_MGQ_TXBD_DESA,
            REG_BKQ_TXBD_DESA, REG_BEQ_TXBD_DESA, REG_VIQ_TXBD_DESA, REG_VOQ_TXBD_DESA,
            REG_HI0Q_TXBD_DESA, REG_HI1Q_TXBD_DESA, REG_HI2Q_TXBD_DESA, REG_HI3Q_TXBD_DESA,
            REG_HI4Q_TXBD_DESA, REG_HI5Q_TXBD_DESA, REG_HI6Q_TXBD_DESA, REG_HI7Q_TXBD_DESA,
            TXBD_Reg_CMDQ,REG_BCNQ_TXBD_DESA
        };
#endif



#if IS_EXIST_RTL8814BE
        if ( IS_HARDWARE_TYPE_8814B(Adapter))  {
            pTXBD_RWPtr_Reg       = TXBD_RWPtr_Reg_V1;

        }
#endif //IS_EXIST_RTL8814BE

#if IS_EXIST_RTL8198FEM
        if ( IS_HARDWARE_TYPE_8198F(Adapter))  {
            pTXBD_RWPtr_Reg     = TXBD_RWPtr_Reg;
            pTXBD_Reg           = TXBD_Reg;
        }
#endif //IS_EXIST_RTL8198FEM

#if IS_EXIST_RTL8197GEM
        if ( IS_HARDWARE_TYPE_8197G(Adapter))  {
            pTXBD_RWPtr_Reg     = TXBD_RWPtr_Reg_V2;
            pTXBD_Reg           = TXBD_Reg_V1;
        }
#endif // IS_EXIST_RTL8197GEM


#if (IS_RTL8814B_SERIES) 

       if(IS_HARDWARE_TYPE_8814B(Adapter))  {
        pTXBD_NUM           = TXBD_NUM_V3;
        TXDESCSize = SIZE_TXDESC_88XX_V2-8; //IE_body+IE0-IE4   
        bcn_TXDESCSize = SIZE_TXDESC_88XX_V2_BCN; //IE_body+IE0~IE4
        

        TotalTXBDNum_NoBcn  = TOTAL_NUM_TXBD_NO_BCN_8814B;
        DMA_MAX_QUEUE_NUM = HCI_TX_DMA_QUEUE_MAX_NUM_V2;  
        DMA_QUEUE_BCN = HCI_TX_DMA_QUEUE_BCN_V2;      
        DMA_QUEUE_CMD = HCI_TX_DMA_QUEUE_CMD_V2;
        DMA_QUEUE_FWCMD = HCI_TX_DMA_QUEUE_FWCMD;

       }
#endif //IS_RTL8814B_SERIES

#if (IS_RTL8198F_SERIES) //IS_EXIST_RTL8198FEM

       if(IS_HARDWARE_TYPE_8198F(Adapter))  {
        pTXBD_NUM           = TXBD_NUM_V2;
        DMA_MAX_QUEUE_NUM = HCI_TX_DMA_QUEUE_MAX_NUM_V1;  
        DMA_QUEUE_BCN = HCI_TX_DMA_QUEUE_BCN_V1;      
        DMA_QUEUE_CMD = HCI_TX_DMA_QUEUE_CMD_V1;
        TotalTXBDNum_NoBcn  = TOTAL_NUM_TXBD_NO_BCN_V1;
        TXDESCSize = SIZE_TXDESC_88XX;   
        bcn_TXDESCSize = SIZE_TXDESC_88XX;

       }
#endif //IS_RTL8198F_SERIES

#if (IS_RTL8197G_SERIES)
    
           if( IS_HARDWARE_TYPE_8197G(Adapter))  {
            pTXBD_NUM           = TXBD_NUM_V4;
            DMA_MAX_QUEUE_NUM = HCI_TX_DMA_QUEUE_MAX_NUM;  
            DMA_QUEUE_BCN = HCI_TX_DMA_QUEUE_BCN;      
            DMA_QUEUE_CMD = HCI_TX_DMA_QUEUE_CMD;
            TotalTXBDNum_NoBcn  = TOTAL_NUM_TXBD_NO_BCN_8197G;
	    TXDESCSize = SIZE_TXDESC_88XX_V1;//kkbomb 
	    bcn_TXDESCSize = SIZE_TXDESC_88XX_V1;//kkbomb
           }
#endif //IS_RTL8197G_SERIES

    prx_dma         = (PHCI_RX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PRxDMA88XX);
    ptx_dma         = (PHCI_TX_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMA88XX);

#if CFG_HAL_TX_AMSDU
    ptx_dma_amsdu   = (PHCI_TX_AMSDU_DMA_MANAGER_88XX)(_GET_HAL_DATA(Adapter)->PTxDMAAMSDU88XX);
#endif

#ifdef CONFIG_NET_PCI
    unsigned long tmp_tx_dma_ring_addr =0, tmp_tx_dma_ring_addr2=0;
    unsigned long tmp_tx_dma_ring_addr3 =0, tmp_tx_dma_ring_addr4=0;

    if (!HAL_IS_PCIBIOS_TYPE(Adapter))
        goto original;

    //No Beacon
    printk("head:%p, ring_dma_addr:%08lx, size:%x\n", prx_dma->rx_queue[HCI_RX_DMA_QUEUE_MAX_NUM-1].pRXBD_head, 
        _GET_HAL_DATA(Adapter)->ring_dma_addr, prx_dma->rx_queue[HCI_RX_DMA_QUEUE_MAX_NUM-1].total_rxbd_num * sizeof(RX_BUFFER_DESCRIPTOR));
        tmp_tx_dma_ring_addr4 = (unsigned long)prx_dma->rx_queue[HCI_RX_DMA_QUEUE_MAX_NUM-1].pRXBD_head +  prx_dma->rx_queue[HCI_RX_DMA_QUEUE_MAX_NUM-1].total_rxbd_num * sizeof(RX_BUFFER_DESCRIPTOR);
    ptxbd_head  = (PTX_BUFFER_DESCRIPTOR)( tmp_tx_dma_ring_addr4);

    for (i=0;i<HCI_RX_DMA_QUEUE_MAX_NUM;i++){
        tmp_tx_dma_ring_addr = _GET_HAL_DATA(Adapter)->ring_dma_addr + prx_dma->rx_queue[i].total_rxbd_num * sizeof(RX_BUFFER_DESCRIPTOR);
    }


    printk("ptxbd_head:%p, tmp_tx_dma_ring_addr:%08lx\n", ptxbd_head, tmp_tx_dma_ring_addr);
    tmp_tx_dma_ring_addr4 =0;
    //No Beacon
    // TODO: need to bug fix
    ptx_desc_head   = (PTX_DESC_88XX)((pu1Byte)ptxbd_head + \
                        sizeof(TX_BUFFER_DESCRIPTOR) * TotalTXBDNum_NoBcn);

    tmp_tx_dma_ring_addr2 = tmp_tx_dma_ring_addr + sizeof(TX_BUFFER_DESCRIPTOR) * TotalTXBDNum_NoBcn;
    printk("ptx_desc_head:%p, tmp_tx_dma_ring_addr2:%08lx, size: %x, %x\n", ptx_desc_head, tmp_tx_dma_ring_addr2, sizeof(TX_BUFFER_DESCRIPTOR) , sizeof(TX_DESC_88XX) );
    ptxbd_bcn_head  = (PTX_BUFFER_DESCRIPTOR)((pu1Byte)ptx_desc_head + \
                        sizeof(TX_DESC_88XX) * TotalTXBDNum_NoBcn);

    tmp_tx_dma_ring_addr3 = tmp_tx_dma_ring_addr2 + sizeof(TX_DESC_88XX) * TotalTXBDNum_NoBcn;
 printk("ptxbd_bcn_head:%p, tmp_tx_dma_ring_addr3:%08lx,\n", ptxbd_bcn_head, tmp_tx_dma_ring_addr3);

#if IS_RTL8192E_SERIES
    if (IS_HARDWARE_TYPE_8192E(Adapter)) {
        ptxdesc_bcn_head = (PTX_DESC_88XX)((pu1Byte)ptxbd_bcn_head + \
                                (1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1);
        beacon_offset = TXBD_BEACON_OFFSET_V1;
        tmp_tx_dma_ring_addr4 = tmp_tx_dma_ring_addr3+(1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1;
    }
#endif
#if IS_RTL8814A_SERIES
    if (IS_HARDWARE_TYPE_8814A(Adapter)) {
        ptxdesc_bcn_head = (PTX_DESC_88XX)((pu1Byte)ptxbd_bcn_head + \
                                (1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1);
        beacon_offset = TXBD_BEACON_OFFSET_V1;
        tmp_tx_dma_ring_addr4 = tmp_tx_dma_ring_addr3+(1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1;
    }
#endif

#if IS_RTL8197F_SERIES
    if (IS_HARDWARE_TYPE_8197F(Adapter)) {
        ptxdesc_bcn_head = (PTX_DESC_88XX)((pu1Byte)ptxbd_bcn_head + \
                                (1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_8197F);
        beacon_offset = TXBD_BEACON_OFFSET_8197F;
        tmp_tx_dma_ring_addr4 = tmp_tx_dma_ring_addr3+(1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_8197F;
    }
#endif

#if IS_RTL8822B_SERIES || IS_RTL8822C_SERIES || IS_RTL8812F_SERIES
	if (IS_HARDWARE_TYPE_8822B(Adapter) || IS_HARDWARE_TYPE_8822C(Adapter) || IS_HARDWARE_TYPE_8812F(Adapter)) {

		ptxdesc_bcn_head = (PTX_DESC_88XX)((pu1Byte)ptxbd_bcn_head + \
				(1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1);
	beacon_offset = TXBD_BEACON_OFFSET_V1;
	tmp_tx_dma_ring_addr4 = tmp_tx_dma_ring_addr3+(1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1;
	}
#endif

#if IS_RTL8814B_SERIES
	if (IS_HARDWARE_TYPE_8814B(Adapter)) {
		ptxdesc_bcn_head = (PTX_DESC_88XX)((pu1Byte)ptxbd_bcn_head + \
				(1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1);
	beacon_offset = TXBD_BEACON_OFFSET_V1;
	tmp_tx_dma_ring_addr4 = tmp_tx_dma_ring_addr3+(1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1;
	}
#endif

printk("ptxdesc_bcn_head:%p, tmp_tx_dma_ring_addr4:%08lx, \n", ptxdesc_bcn_head, tmp_tx_dma_ring_addr4);
    // initiate all tx queue data structures 
    for (q_num = 0; q_num < DMA_MAX_QUEUE_NUM; q_num++)
    {
        ptx_dma->tx_queue[q_num].hw_idx         = 0;
        ptx_dma->tx_queue[q_num].host_idx       = 0;
        ptx_dma->tx_queue[q_num].total_txbd_num = pTXBD_NUM[q_num];
        ptx_dma->tx_queue[q_num].avail_txbd_num = pTXBD_NUM[q_num];
        ptx_dma->tx_queue[q_num].reg_rwptr_idx  = pTXBD_RWPtr_Reg[q_num];

        if ( 0 == q_num ) {
            ptx_dma->tx_queue[q_num].pTXBD_head    = ptxbd_head;
            ptx_dma->tx_queue[q_num].ptx_desc_head = ptx_desc_head;
        }
        else {
            if ( DMA_QUEUE_BCN != q_num ) {
                ptx_dma->tx_queue[q_num].pTXBD_head    = (PTX_BUFFER_DESCRIPTOR)((pu1Byte)ptx_dma->tx_queue[q_num-1].pTXBD_head + sizeof(TX_BUFFER_DESCRIPTOR) * pTXBD_NUM[q_num-1]);
                ptx_dma->tx_queue[q_num].ptx_desc_head = (PTX_DESC_88XX)((pu1Byte)ptx_dma->tx_queue[q_num-1].ptx_desc_head + sizeof(TX_DESC_88XX)*pTXBD_NUM[q_num-1]);

                tmp_tx_dma_ring_addr += sizeof(TX_BUFFER_DESCRIPTOR) *pTXBD_NUM[q_num-1] ;
                tmp_tx_dma_ring_addr2 += sizeof(TX_DESC_88XX) *pTXBD_NUM[q_num-1] ;
            }
            else {
                ptx_dma->tx_queue[q_num].pTXBD_head    = ptxbd_bcn_head;
                ptx_dma->tx_queue[q_num].ptx_desc_head = ptxdesc_bcn_head;
                tmp_tx_dma_ring_addr = tmp_tx_dma_ring_addr3;
                tmp_tx_dma_ring_addr2 = tmp_tx_dma_ring_addr4;
            }
        }
        
        ptxbd    = ptx_dma->tx_queue[q_num].pTXBD_head;
        ptx_desc = ptx_dma->tx_queue[q_num].ptx_desc_head;
	
		if(pTXBD_RWPtr_Reg[q_num])
	        HAL_RTL_W32(pTXBD_RWPtr_Reg[q_num], 0);
#if IS_RTL8814B_SERIES
        if(IS_HARDWARE_TYPE_8814B(Adapter)){
            HAL_RTL_W32(TXBD_Reg_L[q_num], tmp_tx_dma_ring_addr & 0xFFFFFFFF);
            HAL_RTL_W32(TXBD_Reg_H[q_num], (tmp_tx_dma_ring_addr & 0xFFFFFFFF00000000) >> 32);
        }
#endif
#if IS_EXIST_RTL8198FEM
        if(IS_HARDWARE_TYPE_8198F(Adapter)){
            HAL_RTL_W32(pTXBD_Reg[q_num], tmp_tx_dma_ring_addr);
        }
#endif
        
        _GET_HAL_DATA(Adapter)->txBD_dma_ring_addr[q_num] = tmp_tx_dma_ring_addr;
        _GET_HAL_DATA(Adapter)->txDesc_dma_ring_addr[q_num] = tmp_tx_dma_ring_addr2;

#if IS_RTL8814B_SERIES
        if(IS_HARDWARE_TYPE_8814B(Adapter))
            printk("%s(%d), q_num:%d, TXBD_RWPtr_Reg:0x%x, TXBD_Reg_H:0x%x, TXBD_Reg_L:0x%x, ptxbd:%p %08lx, ptx_desc:%p %08lx\n", __FUNCTION__, __LINE__, q_num, pTXBD_RWPtr_Reg[q_num], TXBD_Reg_H[q_num], TXBD_Reg_L[q_num], ptxbd, tmp_tx_dma_ring_addr, ptx_desc, tmp_tx_dma_ring_addr2);
#endif
#if IS_EXIST_RTL8198FEM
        if(IS_HARDWARE_TYPE_8198F(Adapter))
            printk("%s(%d), q_num:%d, TXBD_RWPtr_Reg:0x%x, TXBD_Reg:0x%x, ptxbd:%p %08lx, ptx_desc:%p %08lx\n", __FUNCTION__, __LINE__, q_num, pTXBD_RWPtr_Reg[q_num], TXBD_Reg[q_num], ptxbd, tmp_tx_dma_ring_addr, ptx_desc, tmp_tx_dma_ring_addr2);
#endif

        // assign LowAddress and TxDescLength to each TXBD element
        if (q_num != DMA_QUEUE_BCN) {
            for(i = 0; i < pTXBD_NUM[q_num]; i++)        
            {   
                SET_DESC_FIELD_CLR(ptxbd[i].TXBD_ELE[0].Dword0, sizeof(TX_DESC_88XX), TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
                SET_DESC_FIELD_CLR(ptxbd[i].TXBD_ELE[0].Dword1, tmp_tx_dma_ring_addr2 + sizeof(TX_DESC_88XX)*i,TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
            }
        } else {
            // beacon...
            for (i = 0; i < 1+HAL_NUM_VWLAN; i++)
            {
                ptxbd_bcn_cur = (pu1Byte)ptxbd + beacon_offset * i;
                SET_DESC_FIELD_CLR(ptxbd_bcn_cur->TXBD_ELE[0].Dword0, \
                        bcn_TXDESCSize, \
                        TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
               
               SET_DESC_FIELD_CLR(ptxbd_bcn_cur->TXBD_ELE[0].Dword1, tmp_tx_dma_ring_addr2 + sizeof(TX_DESC_88XX)*i, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
#if 1
                printk ("ptxbd_bcn[%d]: 0x%x, Dword0: 0x%x, Dword1: 0x%x \n", \
                                                i, (u4Byte)ptxbd_bcn_cur, \
                                                (u4Byte)GET_DESC(ptxbd_bcn_cur->TXBD_ELE[0].Dword0), \
                                                (u4Byte)GET_DESC(ptxbd_bcn_cur->TXBD_ELE[0].Dword1)
                                                );             
#endif
            }
        }        
    }   

#if CFG_HAL_TX_AMSDU
    desc_dma_buf_start_amsdu = _GET_HAL_DATA(Adapter)->desc_dma_buf_amsdu;
    platform_zero_memory(desc_dma_buf_start_amsdu, _GET_HAL_DATA(Adapter)->desc_dma_buf_len_amsdu);
    
    pdesc_dma_buf_amsdu = (pu1Byte)(_GET_HAL_DATA(Adapter)->desc_dma_buf_virt_amsdu);

    // BK, BE, VI, VO
    ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_BK].pTXBD_head_amsdu = pdesc_dma_buf_amsdu;
    ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_BE].pTXBD_head_amsdu = (pu1Byte)ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_BK].pTXBD_head_amsdu + TX_BKQ_TXBD_NUM * sizeof(TXBD_ELEMENT)* MAX_NUM_OF_MSDU_IN_AMSDU;
    ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_VI].pTXBD_head_amsdu = (pu1Byte)ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_BE].pTXBD_head_amsdu + TX_BEQ_TXBD_NUM * sizeof(TXBD_ELEMENT)* MAX_NUM_OF_MSDU_IN_AMSDU;
    ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_VO].pTXBD_head_amsdu = (pu1Byte)ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_VI].pTXBD_head_amsdu + TX_VIQ_TXBD_NUM * sizeof(TXBD_ELEMENT)* MAX_NUM_OF_MSDU_IN_AMSDU;
    _GET_HAL_DATA(Adapter)->desc_dma_buf_addr_amsdu[1] = _GET_HAL_DATA(Adapter)->desc_dma_buf_addr_amsdu[0] + TX_BKQ_TXBD_NUM * sizeof(TXBD_ELEMENT)* MAX_NUM_OF_MSDU_IN_AMSDU;
    _GET_HAL_DATA(Adapter)->desc_dma_buf_addr_amsdu[2] = _GET_HAL_DATA(Adapter)->desc_dma_buf_addr_amsdu[1] + TX_BEQ_TXBD_NUM * sizeof(TXBD_ELEMENT)* MAX_NUM_OF_MSDU_IN_AMSDU;
    _GET_HAL_DATA(Adapter)->desc_dma_buf_addr_amsdu[3] = _GET_HAL_DATA(Adapter)->desc_dma_buf_addr_amsdu[2] + TX_VIQ_TXBD_NUM * sizeof(TXBD_ELEMENT)* MAX_NUM_OF_MSDU_IN_AMSDU;    
#endif 

    return RT_STATUS_SUCCESS;
#endif

original:

#if CFG_HAL_TX_AMSDU
    if ( IS_SUPPORT_TX_AMSDU(Adapter) ) {
        desc_dma_buf_start_amsdu = _GET_HAL_DATA(Adapter)->desc_dma_buf_amsdu;
        platform_zero_memory(desc_dma_buf_start_amsdu, _GET_HAL_DATA(Adapter)->desc_dma_buf_len_amsdu);

#ifdef TRXBD_CACHABLE_REGION
#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
        // With CCI400 ,Do nothing for cache coherent code
#else
        rtl_cache_sync_wback(Adapter,(unsigned long)((PVOID)(desc_dma_buf_start_amsdu)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), 
            _GET_HAL_DATA(Adapter)->desc_dma_buf_len_amsdu,PCI_DMA_TODEVICE);
#endif  //CONFIG_ENABLE_CCI400
#else
        _dma_cache_wback((unsigned long)((PVOID)(desc_dma_buf_start_amsdu)-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), _GET_HAL_DATA(Adapter)->desc_dma_buf_len_amsdu);
#endif
#endif

        pdesc_dma_buf_amsdu = (pu1Byte)(((unsigned long)desc_dma_buf_start_amsdu) + \
            (HAL_PAGE_SIZE - (((unsigned long)desc_dma_buf_start_amsdu) & (HAL_PAGE_SIZE-1))));
        
        //Transfer to Non-cachable address
#ifdef TRXBD_CACHABLE_REGION    
        // Do nothing for un-cachable      
#else
        pdesc_dma_buf_amsdu = (pu1Byte)HAL_TO_NONCACHE_ADDR((unsigned long)pdesc_dma_buf_amsdu);
#endif
    }
#endif // CFG_HAL_TX_AMSDU
    //No Beacon
#ifdef NEW_BUUFER_ALLOCATION
    ptxbd_head      =   _GET_HAL_DATA(Adapter)->desc_dma_buf;
    platform_zero_memory(ptxbd_head, _GET_HAL_DATA(Adapter)->desc_dma_buf_len);
#else
    ptxbd_head      = (PTX_BUFFER_DESCRIPTOR)(prx_dma->rx_queue[HCI_RX_DMA_QUEUE_MAX_NUM-1].pRXBD_head + \
                        prx_dma->rx_queue[HCI_RX_DMA_QUEUE_MAX_NUM-1].total_rxbd_num);
#endif
    //No Beacon
    ptx_desc_head   = (PTX_DESC_88XX)((pu1Byte)ptxbd_head + \
                        sizeof(TX_BUFFER_DESCRIPTOR) * TotalTXBDNum_NoBcn);
    
    ptxbd_bcn_head  = (PTX_BUFFER_DESCRIPTOR)((pu1Byte)ptx_desc_head + \
                        sizeof(TX_DESC_88XX) * TotalTXBDNum_NoBcn);


#if (IS_RTL8198F_SERIES || IS_RTL8197G_SERIES)
    if (IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter)) {
        ptxdesc_bcn_head = (PTX_DESC_88XX)((pu1Byte)ptxbd_bcn_head + \
                                (1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1);
        beacon_offset = TXBD_BEACON_OFFSET_V1;
    }
#endif
#if IS_RTL8814B_SERIES
    if (IS_HARDWARE_TYPE_8814B(Adapter)) {
        ptxdesc_bcn_head = (PTX_DESC_88XX)((pu1Byte)ptxbd_bcn_head + \
                                (1+HAL_NUM_VWLAN) * TXBD_BEACON_OFFSET_V1);
        beacon_offset = TXBD_BEACON_OFFSET_V1;
    }
#endif

    // initiate all tx queue data structures 
    for (q_num = 0; q_num < DMA_MAX_QUEUE_NUM; q_num++)
    {
        ptx_dma->tx_queue[q_num].hw_idx         = 0;
        ptx_dma->tx_queue[q_num].host_idx       = 0;
        ptx_dma->tx_queue[q_num].total_txbd_num = pTXBD_NUM[q_num];
        ptx_dma->tx_queue[q_num].avail_txbd_num = pTXBD_NUM[q_num];
        ptx_dma->tx_queue[q_num].reg_rwptr_idx  = pTXBD_RWPtr_Reg[q_num];

        if ( 0 == q_num ) {
            ptx_dma->tx_queue[q_num].pTXBD_head    = ptxbd_head;
            ptx_dma->tx_queue[q_num].ptx_desc_head = ptx_desc_head;
        }
        else {
            if ( DMA_QUEUE_BCN != q_num ) {
                ptx_dma->tx_queue[q_num].pTXBD_head    = ptx_dma->tx_queue[q_num-1].pTXBD_head + pTXBD_NUM[q_num-1];
                ptx_dma->tx_queue[q_num].ptx_desc_head = ((PTX_DESC_88XX)ptx_dma->tx_queue[q_num-1].ptx_desc_head) + pTXBD_NUM[q_num-1];
            }
            else {
                ptx_dma->tx_queue[q_num].pTXBD_head    = ptxbd_bcn_head;
                ptx_dma->tx_queue[q_num].ptx_desc_head = ptxdesc_bcn_head;
            }
        }
        
        ptxbd    = ptx_dma->tx_queue[q_num].pTXBD_head;
        ptx_desc = ptx_dma->tx_queue[q_num].ptx_desc_head;

		if(pTXBD_RWPtr_Reg[q_num])
        	HAL_RTL_W32(pTXBD_RWPtr_Reg[q_num], 0);
#if (IS_RTL8198F_SERIES || IS_RTL8197G_SERIES)
        if (IS_HARDWARE_TYPE_8198F(Adapter) || IS_HARDWARE_TYPE_8197G(Adapter)) {
    		if(pTXBD_Reg[q_num]){        
                HAL_RTL_W32(pTXBD_Reg[q_num], HAL_VIRT_TO_BUS((u4Byte)ptxbd));

            
#ifdef PCIE_POWER_SAVING_TEST            
                _GET_HAL_DATA(Adapter)->txBD_dma_ring_addr[q_num] = HAL_VIRT_TO_BUS((u4Byte)ptxbd) + CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL;
#endif
            }
        }
#endif
#if IS_RTL8814B_SERIES
        if (IS_HARDWARE_TYPE_8814B(Adapter)) {

            if(TXBD_Reg_L[q_num]){        
            HAL_RTL_W32(TXBD_Reg_L[q_num], (HAL_VIRT_TO_BUS((u4Byte)ptxbd)) & 0xFFFFFFFF);
            HAL_RTL_W32(TXBD_Reg_H[q_num], ((HAL_VIRT_TO_BUS((u4Byte)ptxbd)) & 0xFFFFFFFF00000000) >> 32);
            }
        }
#endif

#if 0

        RT_TRACE_F(COMP_INIT, DBG_TRACE, ("QNum: %ld, TXBDHead: 0x%lx, TXDESCHead: 0x%lx pTXBD_NUM[q_num]:%x\n", \
                                            (u4Byte)q_num, \
                                            (u4Byte)ptx_dma->tx_queue[q_num].pTXBD_head, \
                                            (u4Byte)ptx_dma->tx_queue[q_num].ptx_desc_head,
                                            pTXBD_NUM[q_num]
                                            ));
#endif

        // assign LowAddress and TxDescLength to each TXBD element
        if (q_num != DMA_QUEUE_BCN) {
            for(i = 0; i < pTXBD_NUM[q_num]; i++)        
            {   
                SET_DESC_FIELD_CLR(ptxbd[i].TXBD_ELE[0].Dword0, \
                        TXDESCSize, \
                        TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
                SET_DESC_FIELD_CLR(ptxbd[i].TXBD_ELE[0].Dword1, \
                        HAL_VIRT_TO_BUS((u4Byte)&ptx_desc[i]), \
                        TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);

                platform_zero_memory(&(ptxbd[i].TXBD_ELE[1]), sizeof(TXBD_ELEMENT)*(TXBD_ELE_NUM-1));
#ifdef WLAN_SUPPORT_H2C_PACKET
                if(DMA_QUEUE_CMD == q_num)
                {
                    SET_DESC_FIELD_CLR(ptxbd[i].TXBD_ELE[1].Dword0, \
                        H2C_PACKET_PAYLOAD_MAX_SIZE, \
                        TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
                    SET_DESC_FIELD_CLR(ptxbd[i].TXBD_ELE[1].Dword1, \
                        HAL_VIRT_TO_BUS(&(h2c_buf_start[i])), \
                        TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
                    // h2c TXBD just use 1 page
                    SET_DESC_FIELD_NO_CLR(ptxbd[i].TXBD_ELE[0].Dword0, 1, TXBD_DW0_PSLEN_MSK, TXBD_DW0_PSLEN_SH);   

                    ph2c_tx_desc = &ptx_desc[i];

                    SET_TX_DESC_TXPKTSIZE_NO_CLR(ph2c_tx_desc, 32);
                    SET_TX_DESC_OFFSET_NO_CLR(ph2c_tx_desc, 0);
                    SET_TX_DESC_QSEL(ph2c_tx_desc, TXDESC_QSEL_CMD); 
#ifdef TRXBD_CACHABLE_REGION
                    _dma_cache_wback((unsigned long)(&(ptxbd[i].TXBD_ELE[1])), 
                            sizeof(TX_BUFFER_DESCRIPTOR));
                    _dma_cache_wback((unsigned long)(&ptx_desc[i]), 
                            TXDESCSize);
#endif						                   
                }

                
#endif //WLAN_SUPPORT_H2C_PACKET
#ifdef WLAN_SUPPORT_FW_CMD
                if(DMA_QUEUE_FWCMD == q_num)
                {
                    SET_DESC_FIELD_CLR(ptxbd[i].TXBD_ELE[1].Dword0, \
                        FWCMD_PACKET_PAYLOAD_MAX_SIZE, \
                        TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
                    SET_DESC_FIELD_CLR(ptxbd[i].TXBD_ELE[1].Dword1, \
                        HAL_VIRT_TO_BUS((u4Byte)&(fwcmd_buf_start[i])), \
                        TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
                    // FWCMD only use 1 page temperarily
                    SET_DESC_FIELD_NO_CLR(ptxbd[i].TXBD_ELE[0].Dword0, 1, TXBD_DW0_PSLEN_MSK, TXBD_DW0_PSLEN_SH);   

                    pfwcmd_tx_desc = &ptx_desc[i];
#if 1
                    SET_TX_DESC_TXPKTSIZE_NO_CLR(pfwcmd_tx_desc, FWCMD_PACKET_PAYLOAD_MAX_SIZE);
                    SET_TX_DESC_OFFSET_NO_CLR(pfwcmd_tx_desc, TXDESCSize);
                    SET_TX_DESC_PKT_OFFSET_V1_NO_CLR(pfwcmd_tx_desc, (TXDESCSize-SIZE_TXDESC_BODY)/SIZE_TXDESC_IE);//IE_body will not be ripped off
                    SET_TX_DESC_QSEL(pfwcmd_tx_desc, TXDESC_QSEL_CMD+1);//temp
                    SET_TX_DESC_CHANNEL_DMA_NO_CLR(pfwcmd_tx_desc,TXDESC_DMA_CHANNEL16);
                    SET_TX_DESC_IE_CNT_NO_CLR(pfwcmd_tx_desc,5);//IE0~IE4, no burst mode
                    SET_TX_DESC_IE_CNT_EN_NO_CLR(pfwcmd_tx_desc,1);                 
#else // disable Pkt Transfer machine
                    SET_DESC_FIELD_CLR(ptxbd[i].TXBD_ELE[0].Dword0, \
                        48, \
                        TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
#endif
#ifdef TRXBD_CACHABLE_REGION
                    _dma_cache_wback((unsigned long)(&(ptxbd[i].TXBD_ELE[1])), 
                            sizeof(TX_BUFFER_DESCRIPTOR));
                    /*_dma_cache_wback((unsigned long)(&(ptx_desc[i])-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), 
                            TXDESCSize);*/
                    _dma_cache_wback(((GET_DESC_FIELD(ptxbd[i].TXBD_ELE[0].Dword1, TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH)|0x80000000) - CONFIG_LUNA_SLAVE_PHYMEM_OFFSET_HAL),
        TXDESCSize);
#endif                                         
                }

                
#endif //WLAN_SUPPORT_FW_CMD

#ifdef TRXBD_CACHABLE_REGION

#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
        // With CCI400 ,Do nothing for cache coherent code
#else
                rtl_cache_sync_wback(Adapter,(unsigned long)(&(ptxbd[i].TXBD_ELE[0])), 
                        sizeof(TX_BUFFER_DESCRIPTOR),PCI_DMA_TODEVICE);
#endif //CONFIG_ENABLE_CCI400
#else
                _dma_cache_wback((unsigned long)(&(ptxbd[i].TXBD_ELE[0])), 
                        sizeof(TX_BUFFER_DESCRIPTOR));

#endif //CONFIG_RTL_8198F
              
#endif	//TRXBD_CACHABLE_REGION					
#if 0
               if(DMA_QUEUE_CMD == q_num)
               {
               RT_TRACE_F(COMP_INIT, DBG_TRACE, ("ptxbd[%ld]:TXBD_ELE[0] Dword0: 0x%lx, Dword1: 0x%lx\n", \
                                                i, \
                                                (u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[0].Dword0), \
                                                (u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[0].Dword1)
                                                ));
               RT_TRACE_F(COMP_INIT, DBG_TRACE, ("ptxbd[%ld]:TXBD_ELE[1] Dword0: 0x%lx, Dword1: 0x%lx\n", \
                                                i, \
                                                (u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[1].Dword0), \
                                                (u4Byte)GET_DESC(ptxbd[i].TXBD_ELE[1].Dword1)
                                                ));               
                }
#endif

            }
        } else {
            // beacon...
            //for (i = 0; i < 1+4; i++)
            for (i = 0; i < 1+HAL_NUM_VWLAN; i++)
            {
                ptxbd_bcn_cur = (PTX_BUFFER_DESCRIPTOR)((pu1Byte)ptxbd + beacon_offset * i);

                // clear own_bit for initial random value                
                SET_DESC_FIELD_CLR(ptxbd_bcn_cur->TXBD_ELE[0].Dword0, 0, TXBD_DW0_BCN_OWN_MSK, TXBD_DW0_BCN_OWN_SH);

                SET_DESC_FIELD_CLR(ptxbd_bcn_cur->TXBD_ELE[0].Dword0, \
                        bcn_TXDESCSize, \
                        TXBD_DW0_TXBUFSIZE_MSK, TXBD_DW0_TXBUFSIZE_SH);
                SET_DESC_FIELD_CLR(ptxbd_bcn_cur->TXBD_ELE[0].Dword1, \
                        HAL_VIRT_TO_BUS((u4Byte)&ptx_desc[i]), \
                        TXBD_DW1_PHYADDR_LOW_MSK, TXBD_DW1_PHYADDR_LOW_SH);
                // clear for initial random value
                platform_zero_memory(&(ptxbd_bcn_cur->TXBD_ELE[1]), sizeof(TXBD_ELEMENT)*(TXBD_ELE_NUM-1));
#ifdef TRXBD_CACHABLE_REGION

#ifdef CONFIG_RTL_8198F
#ifdef CONFIG_ENABLE_CCI400
        // With CCI400 ,Do nothing for cache coherent code
#else
                rtl_cache_sync_wback(Adapter,(unsigned long)(&(ptxbd_bcn_cur->TXBD_ELE[0])), 
                    sizeof(TXBD_ELEMENT),PCI_DMA_TODEVICE);
#endif //CONFIG_ENABLE_CCI400
#else
                _dma_cache_wback((unsigned long)(&(ptxbd_bcn_cur->TXBD_ELE[0])), 
                    sizeof(TXBD_ELEMENT));
#endif //CONFIG_RTL_8198F

#endif //#ifdef TRXBD_CACHABLE_REGION                
#if 1
                RT_TRACE_F(COMP_INIT, DBG_TRACE, ("ptxbd_bcn[%ld]: 0x%lx, Dword0: 0x%lx, Dword1: 0x%lx \n", \
                                                i, (u4Byte)ptxbd_bcn_cur, \
                                                (u4Byte)GET_DESC(ptxbd_bcn_cur->TXBD_ELE[0].Dword0), \
                                                (u4Byte)GET_DESC(ptxbd_bcn_cur->TXBD_ELE[0].Dword1)
                                                ));    
                printk("test 0x%x,0x%x,0x%x,0x%x, \n",ptxbd_bcn_cur,*ptxbd_bcn_cur, ((u4Byte*)ptxbd_bcn_cur)+1,*(((u4Byte*)ptxbd_bcn_cur)+1));
#endif
            }
        }        
    }   


#if CFG_HAL_TX_AMSDU
    if ( IS_SUPPORT_TX_AMSDU(Adapter) ) {

    //ptxbd_head_amsdu    = ptx_dma_amsdu;
    //ptx_dma_amsdu = pdesc_dma_buf_amsdu;

    // BK, BE, VI, VO
    ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_BK].pTXBD_head_amsdu = pdesc_dma_buf_amsdu;
    ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_BE].pTXBD_head_amsdu = (pu1Byte)ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_BK].pTXBD_head_amsdu + TX_BKQ_TXBD_NUM * sizeof(TXBD_ELEMENT)* MAX_NUM_OF_MSDU_IN_AMSDU;
    ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_VI].pTXBD_head_amsdu = (pu1Byte)ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_BE].pTXBD_head_amsdu + TX_BEQ_TXBD_NUM * sizeof(TXBD_ELEMENT)* MAX_NUM_OF_MSDU_IN_AMSDU;
    ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_VO].pTXBD_head_amsdu = (pu1Byte)ptx_dma_amsdu->tx_amsdu_queue[HCI_TX_AMSDU_DMA_QUEUE_VI].pTXBD_head_amsdu + TX_VIQ_TXBD_NUM * sizeof(TXBD_ELEMENT)* MAX_NUM_OF_MSDU_IN_AMSDU;
    }
#endif

    return RT_STATUS_SUCCESS;
}

VOID
SetTxDescQSel88XX_V2(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          queueIndex,  //HCI_TX_DMA_QUEUE_88XX
    IN  PTX_DESC_88XX   ptx_desc,
    IN  u1Byte          drvTID
)
{
    u1Byte  q_select;
#if CFG_HAL_SUPPORT_TXDESC_IE    
    u1Byte* IE1_head = ptx_desc+16+8;
#endif    
	switch (queueIndex) {    	
        case HCI_TX_DMA_QUEUE_CMD_V1:
            q_select = TXDESC_QSEL_CMD;
            break;     
    	case HCI_TX_DMA_QUEUE_MGT_V1:
    		q_select = TXDESC_QSEL_MGT;
    		break;
//swps test need fill vo vi be bk qsel            
#if (CFG_HAL_MAC_LOOPBACK && CFG_HAL_WIFI_WMM) || defined(CONFIG_8814_AP_MAC_VERI)
    	case HCI_TX_DMA_QUEUE_BE:
    		q_select = TXDESC_QSEL_TID0;
    		break;
        case HCI_TX_DMA_QUEUE_VO:
			q_select = TXDESC_QSEL_TID6;
			break;
		case HCI_TX_DMA_QUEUE_VI:
			q_select = TXDESC_QSEL_TID4;
    		break;
    	case HCI_TX_DMA_QUEUE_BK:
			q_select = TXDESC_QSEL_TID1;
			break;            
#endif  //CFG_HAL_MAC_LOOPBACK && CFG_HAL_WIFI_WMM

    	default:
    		// data packet
#if CFG_HAL_RTL_MANUAL_EDCA
    		if (HAL_VAR_MANUAL_EDCA) {
    			switch (queueIndex) {
        			case HCI_TX_DMA_QUEUE_VO:
        				q_select = TXDESC_QSEL_TID6;
        				break;
        			case HCI_TX_DMA_QUEUE_VI:
        				q_select = TXDESC_QSEL_TID4;
                		break;
        			case HCI_TX_DMA_QUEUE_BE:
        				q_select = TXDESC_QSEL_TID0;
        	    		break;
        		    default:
        				q_select = TXDESC_QSEL_TID1;
        				break;
    			}
    		}
    		else {
                q_select = drvTID;
    		}
#else
            q_select = drvTID;
#endif  //CFG_HAL_RTL_MANUAL_EDCA
           // HIQ packet
            if((queueIndex >= HCI_TX_DMA_QUEUE_HI0_V1) && (queueIndex<=HCI_TX_DMA_QUEUE_HI15)) {
#ifdef SUPPORT_TXDESC_IE
                if(IS_SUPPORT_TXDESC_IE(Adapter)){
                    IE1_SET_TX_DESC_MOREDATA(IE1_head,1);            
                }
#endif 
#if IS_RTL88XX_MAC_V2_V3 || IS_EXIST_RTL8192FE
                if(_GET_HAL_DATA(Adapter)->MacVersion.is_MAC_v1_v2_v3){
                    SET_TX_DESC_MOREDATA_NO_CLR(ptx_desc,1); 
                }
#endif                
                q_select = TXDESC_QSEL_HIGH;
            }
               
            break;
	}    
    SET_TX_DESC_QSEL(ptx_desc,q_select);    
}

#endif //#if IS_RTL88XX_MAC_V3_V4


