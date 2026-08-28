#ifndef _8192CD_CORE_UTIL_H_
#define _8192CD_CORE_UTIL_H_

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/circ_buf.h>
#include <asm/io.h>
#elif defined(TAROKO_0)
#include <linux/pci.h>
#endif
#ifdef __KERNEL__
#include <asm/cacheflush.h>
#endif
#include "../8192cd_cfg.h"
#include "../8192cd.h"
#include "../wifi.h"
#include "../8192cd_hw.h"

#if !defined(NOT_RTK_BSP)
#if defined(__LINUX_2_6__)
//#include <bsp/bspchip.h>
#else
#if !defined(__ECOS) && !defined(__OSK__)
	#include <asm/rtl865x/platform.h>
#endif	
#endif
#endif

#ifdef CONFIG_RTK_MESH
#include "../mesh_ext/mesh_util.h"
#endif

#ifdef CONFIG_USB_HCI
#ifdef CONFIG_RTL_88E_SUPPORT
#include "../usb/8188eu/8192cd_usb.h"
#endif

#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
  #if defined(CONFIG_RTL_92E_SUPPORT)
    #include "./usb/8192eu/8192cd_usb.h"
  #elif defined(CONFIG_RTL_92F_SUPPORT)
    #include "./usb/8192fu/8192cd_usb.h"
  #else
    #error "triband undefined!!"
  #endif
#endif /* defined(CONFIG_RTL_TRIBAND_SUPPORT) */
#endif

#ifdef CONFIG_SDIO_HCI
#ifdef CONFIG_RTL_88E_SUPPORT
#include "../sdio/8189es/8188e_sdio.h"
#endif
#ifdef CONFIG_WLAN_HAL_8192EE
#include "../sdio/8192es/8192e_sdio.h"
#endif
#endif

#ifdef RTK_129X_PLATFORM
#ifdef CONFIG_RTK_SW_LOCK_API
#include <soc/realtek/rtd129x_lockapi.h>
#endif
#endif



#ifdef __LINUX_2_6__
#ifdef __MIPSEB__
#ifdef virt_to_bus
	#undef virt_to_bus
	#define virt_to_bus			CPHYSADDR
#endif
#else
#ifndef virt_to_bus
#define virt_to_bus	virt_to_phys
#endif
#ifndef bus_to_virt
#define bus_to_virt phys_to_virt 
#endif
#endif
#endif


#ifdef __KERNEL__
#ifdef NOT_RTK_BSP
#define __skb_dequeue(skb_queue)			skb_dequeue(skb_queue)
#define __skb_queue_tail(skb_queue, skb)	skb_queue_tail(skb_queue, skb)
#endif
#endif

#if defined(CONFIG_NET_PCI) || defined(CONFIG_RTL_8198F)
#define IS_PCIBIOS_TYPE		(((priv->pshare->type >> TYPE_SHIFT) & TYPE_MASK) == TYPE_PCI_BIOS)
#define IS_PCIBIOS_DT_TYPE		(((priv->pshare->type >> TYPE_SHIFT) & TYPE_MASK) == TYPE_PCI_BIOS_DT)
#endif

extern void release_buf_to_poll(struct rtl8192cd_priv *priv, unsigned char *pbuf, struct list_head	*phead, unsigned int *count);
extern unsigned char *get_buf_from_poll(struct rtl8192cd_priv *priv, struct list_head *phead, unsigned int *count);


#define rtl_atomic_inc(ptr_atomic_t)	atomic_inc(ptr_atomic_t)
#define rtl_atomic_dec(ptr_atomic_t)	atomic_dec(ptr_atomic_t)
#define rtl_atomic_read(ptr_atomic_t)	atomic_read(ptr_atomic_t)
#define rtl_atomic_set(ptr_atomic_t, i)	atomic_set(ptr_atomic_t,i)

#define RTL_SECONDS_TO_JIFFIES(x) ((x)*HZ)
#define RTL_MILISECONDS_TO_JIFFIES(x) (((x)*HZ-1)/1000+1)
#define RTL_MICROSECONDS_TO_GTIMERCOUNTER(x) ((x)*100/3125 + 1)


#define CIRC_CNT_RTK(head,tail,size)	((head>=tail)?(head-tail):(size-tail+head))


#define REMAP_AID(p)   p->cmn_info.mac_id
#define get_tofr_ds(pframe)	((GetToDs(pframe) << 1) | GetFrDs(pframe))

#define isEqualIPAddr(addr1,addr2) ((((unsigned long)(addr1)&0x01) == 0 && ((unsigned long)(addr2)&0x01) == 0)? \
                                     ((*(unsigned short*)(addr1) == *(unsigned short*)(addr2)) && (*(unsigned short*)(addr1+2) == *(unsigned short*)(addr2+2))) : \
                                     !memcmp(addr1, addr2, 4)\
                                   )

#if defined(CONFIG_RTL_TRIBAND_SUPPORT) && defined(SMP_SYNC)
#define SMP_LOCK_ASOC_LIST(__x__) \
	do { \
            if (priv->hci_type == RTL_HCI_PCIE) { \
    			__u32 _cpu_id = get_cpu(); \
    			if (priv->asoc_list_lock_owner != _cpu_id) { \  				
    					spin_lock_irqsave(&priv->asoc_list_lock, __x__); \
    			} \
    			else { \
    				panic_printk("[%s %d] recursion detection, caller=%p\n",__FUNCTION__,__LINE__,__builtin_return_address(0)); \
    				panic_printk("Previous Lock Function is %s\n",priv->asoc_list_lock_func); \
    			} \
    			strcpy(priv->asoc_list_lock_func, __FUNCTION__); \
    			priv->asoc_list_lock_owner = _cpu_id; \
			    put_cpu(); \
            } \
            else \
                spin_lock_bh(&priv->asoc_list_lock); \
	} while(0)

#define SMP_UNLOCK_ASOC_LIST(__x__)	\
	do { \
			if (priv->hci_type == RTL_HCI_PCIE) { \
                priv->asoc_list_lock_owner = -1; \
				spin_unlock_irqrestore(&priv->asoc_list_lock, __x__); \
            } \
			else \
				spin_unlock_bh(&priv->asoc_list_lock); \
	} while(0)

#define SMP_TRY_LOCK_ASOC_LIST(__x__, __y__) \
	do { \	
            if (priv->hci_type == RTL_HCI_PCIE) { \
    			__u32 _cpu_id = get_cpu(); \
    			if(priv->asoc_list_lock_owner != _cpu_id) { \
    				SMP_LOCK_ASOC_LIST(__x__); \
    				__y__ = 1; \
    			} else \
    				__y__ = 0; \
    			put_cpu(); \
            } \
	} while(0)
#else
#define DEFRAG_LOCK(__x__)		spin_lock_irqsave(&priv->defrag_lock, (__x__))
#define DEFRAG_UNLOCK(__x__)		spin_unlock_irqrestore(&priv->defrag_lock, (__x__))


//#define SAVE_INT_AND_CLI(__x__)		spin_lock_irqsave(&priv->pshare->lock, (__x__))
//#define RESTORE_INT(__x__)		spin_unlock_irqrestore(&priv->pshare->lock, (__x__))

//#define SMP_LOCK_XMIT(__x__)		
//#define SMP_UNLOCK_XMIT(__x__)
//#define SMP_LOCK_RECV(__x__)
//#define SMP_UNLOCK_RECV(__x__)
//#define SMP_TRY_LOCK_REORDER_CTRL(__x__,__y__)
//#define SMP_UNLOCK_REORDER_CTRL(__x__)
//#define SMP_LOCK_ASOC_LIST(__x__)
//#define SMP_UNLOCK_ASOC_LIST(__x__)
#endif /* defined(CONFIG_RTL_TRIBAND_SUPPORT) && defined(SMP_SYNC) */

#define UINT32_DIFF(a, b)		((a >= b)? (a - b):(0xffffffff - b + a + 1))
#define IS_HAL_CHIP(priv)	(priv->pshare->use_hal)

#define get_desc(val)	((priv->pshare->type & ACCESS_SWAP_MEM) ? le32_to_cpu(val) : val)
#define set_desc(val)	((priv->pshare->type & ACCESS_SWAP_MEM) ? cpu_to_le32(val) : val)

#define is_qos_data(pframe)	((GetFrameSubType(pframe) & (WIFI_DATA_TYPE | BIT(7))) == (WIFI_DATA_TYPE | BIT(7)))

#define get_pskb(pfrinfo)		(pfrinfo->pskb)


#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
#define get_pfrinfo(pskb)		(struct rx_frinfo *)PTR_ALIGN(((unsigned long)(pskb->data) - sizeof(struct rx_frinfo)-4), 4)
#else
#define get_pfrinfo(pskb)		((struct rx_frinfo *)((unsigned long)(pskb->data) - sizeof(struct rx_frinfo)))
#endif
#define get_pframe(pfrinfo)		((unsigned char *)((unsigned long)(pfrinfo->pskb->data)))

/*check addr1, addr2 is 2 byte alignment first,  
    to prevent 97D and older CUP which do not support unalignment access have kernel unaligned access core dump*/
#if 0
#define isEqualMACAddr(addr1,addr2) ((((unsigned long)(addr1)&0x01) == 0 && ((unsigned long)(addr2)&0x01) == 0)? \
                                      ((*(unsigned short*)(addr1) == *(unsigned short*)(addr2)) && (*(unsigned short*)(addr1+2) == *(unsigned short*)(addr2+2)) && (*(unsigned short*)(addr1+4) == *(unsigned short*)(addr2+4))) : \
                                       !memcmp(addr1, addr2, MACADDRLEN)  \
                                     )
#else 
#define isEqualMACAddr(addr1,addr2) ((((unsigned long)(addr1)|(unsigned long)(addr2))&0x01) ? \
									((*(unsigned char*)(addr1) ^ (*(unsigned char*)(addr2))) | (*(unsigned char*)(addr1+1) ^ (*(unsigned char*)(addr2+1))) | (*(unsigned char*)(addr1+2) ^ (*(unsigned char*)(addr2+2)))|\
									(*(unsigned char*)(addr1+3) ^ (*(unsigned char*)(addr2+3))) | (*(unsigned char*)(addr1+4) ^ (*(unsigned char*)(addr2+4))) | (*(unsigned char*)(addr1+5) ^ (*(unsigned char*)(addr2+5))))==0 :\
									((*(unsigned short*)(addr1) ^ (*(unsigned short*)(addr2))) | (*(unsigned short*)(addr1+2) ^ (*(unsigned short*)(addr2+2))) | (*(unsigned short*)(addr1+4) ^ (*(unsigned short*)(addr2+4))))==0  \
                                     )
                                     
static inline int rtk_memcmp(unsigned char *addr1, unsigned char *addr2, unsigned int len)                         
{
	unsigned int k, result=0;
	if(((unsigned long)(addr1)|(unsigned long)(addr2)| len) &1) {
		for(k = 0; k<len; k++)
			result |= (addr1[k]^addr2[k]);
		return result;
	} else {
		unsigned short *s1 = (unsigned short *)addr1;
		unsigned short *s2 = (unsigned short *)addr2;		
		for(k = 0; k<(len/2); k++, s1++, s2++)
			result |= ((*s1) ^(*s2));
		return result;		
	}
}

#if 1                                    
#define copyMACAddr(addr1,addr2)  {\
									if(((unsigned long)(addr1)|(unsigned long)(addr2))&0x01) { \
									(*(unsigned char*)(addr1) = (*(unsigned char*)(addr2))) ; (*(unsigned char*)(addr1+1) = (*(unsigned short*)(addr2+1))) ; (*(unsigned char*)(addr1+2) = (*(unsigned char*)(addr2+2)));\
									(*(unsigned char*)(addr1+3) = (*(unsigned char*)(addr2+3))) ; (*(unsigned char*)(addr1+4) = (*(unsigned short*)(addr2+4))) ; (*(unsigned char*)(addr1+5) = (*(unsigned char*)(addr2+5))); }else {\
									(*(unsigned short*)(addr1) = (*(unsigned short*)(addr2))) ; (*(unsigned short*)(addr1+2) = (*(unsigned short*)(addr2+2))) ; (*(unsigned short*)(addr1+4) = (*(unsigned short*)(addr2+4))); } \
									};
#else
#define copyMACAddr(addr1,addr2)	memcpy(addr1, addr2, MACADDRLEN);	
#endif
#endif

#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
#define GET_HCI_TYPE(priv)		(priv->hci_type)
#elif defined(CONFIG_PCI_HCI)
#define GET_HCI_TYPE(priv)		RTL_HCI_PCIE
#elif defined(CONFIG_USB_HCI)
#define GET_HCI_TYPE(priv)		RTL_HCI_USB
#elif defined(CONFIG_SDIO_HCI)
#define GET_HCI_TYPE(priv)		RTL_HCI_SDIO
#endif


#if defined(CONFIG_PCI_HCI) || defined(CONFIG_RTL_TRIBAND_SUPPORT)
#define GET_CHIP_VER(priv)		((priv->pshare->version_id&VERSION_MASK))

#define BIT_RTL_ID                            	BIT(23)
#define BIT_SHIFT_CHIP_VER                    	12
#define BIT_MASK_CHIP_VER                     	0xf
#define GET_BIT_CHIP_VER(x)  (((x)>>BIT_SHIFT_CHIP_VER) & BIT_MASK_CHIP_VER)

#define IS_TEST_CHIP_8814(priv)     (RTL_R32(0x0F0) & BIT_RTL_ID)
#define GET_CHIP_VER_8814(priv)     (GET_BIT_CHIP_VER(RTL_R32(0x0F0)))
#define GET_CHIP_VER_8822(priv)     (GET_BIT_CHIP_VER(RTL_R32(0x0F0)))


#elif  defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
#if defined(CONFIG_RTL_92C_SUPPORT)
#define GET_CHIP_VER(priv)		VERSION_8192C
#elif defined(CONFIG_RTL_88E_SUPPORT)
#define GET_CHIP_VER(priv)		VERSION_8188E
#elif defined(CONFIG_WLAN_HAL_8192EE)
#define GET_CHIP_VER(priv)		VERSION_8192E
#elif defined(CONFIG_WLAN_HAL_8822BE)
#define GET_CHIP_VER(priv)		VERSION_8822B
#elif defined(CONFIG_WLAN_HAL_8821CE)
#define GET_CHIP_VER(priv)		VERSION_8821C
#endif
#define GET_BIT_CHIP_VER(x)  (((x)>>BIT_SHIFT_CHIP_VER) & BIT_MASK_CHIP_VER)
#define GET_CHIP_VER_8822(priv)     (GET_BIT_CHIP_VER(RTL_R32(0x0F0)))
#endif



#if defined(CONFIG_PCI_HCI) || defined(CONFIG_RTL_TRIBAND_SUPPORT)
#if defined(USE_IO_OPS) && !defined(CONFIG_RTL_TRIBAND_SUPPORT)
#define get_desc(val)           (val)
#define set_desc(val)           (val)

#define RTL_R8(reg)             inb(((unsigned long)priv->pshare->ioaddr) + (reg))
#define RTL_R16(reg)            inw(((unsigned long)priv->pshare->ioaddr) + (reg))
#define RTL_R32(reg)            ((unsigned long)inl(((unsigned long)priv->pshare->ioaddr) + (reg)))
#define RTL_W8(reg, val8)       outb((val8), ((unsigned long)priv->pshare->ioaddr) + (reg))
#define RTL_W16(reg, val16)     outw((val16), ((unsigned long)priv->pshare->ioaddr) + (reg))
#define RTL_W32(reg, val32)     outl((val32), ((unsigned long)priv->pshare->ioaddr) + (reg))
#define RTL_W8_F                RTL_W8
#define RTL_W16_F               RTL_W16
#define RTL_W32_F               RTL_W32
#undef readb
#undef readw
#undef readl
#undef writeb
#undef writew
#undef writel
#define readb(addr)             inb((unsigned long)(addr))
#define readw(addr)             inw((unsigned long)(addr))
#define readl(addr)             inl((unsigned long)(addr))
#define writeb(val,addr)        outb((val), (unsigned long)(addr))
#define writew(val,addr)        outw((val), (unsigned long)(addr))
#define writel(val,addr)        outl((val), (unsigned long)(addr))

#else // !USE_IO_OPS

#define PAGE_NUM 15

#if defined(__LINUX_2_6__) || defined(__OSK__) || defined(TAROKO_0)
	#define IO_TYPE_CAST	(unsigned char *)
#else
	#define IO_TYPE_CAST	(unsigned int)
#endif

#if defined(CONFIG_RTL_8198) || defined(CONFIG_WLAN_HAL_8197F) || defined(CONFIG_WLAN_HAL_8821CE)
#ifndef REG32
    #define REG32(reg)      (*(volatile unsigned int *)(reg))
#endif
#endif

#ifndef REG32
    #define REG32(reg)      (*(volatile unsigned int *)(reg))
#endif

#ifdef RTK_129X_PLATFORM
#define PCIE_SLOT1_MEM_START	0x9804F000
#define PCIE_SLOT1_MEM_LEN	0x1000
#define PCIE_SLOT1_CTRL_START	0x9804EC00

#define PCIE_SLOT2_MEM_START	0x9803C000
#define PCIE_SLOT2_MEM_LEN	0x1000
#define PCIE_SLOT2_CTRL_START	0x9803BC00

#define PCIE_MASK_OFFSET	0x100 //offset from ctrl start
#define PCIE_TRANSLATE_OFFSET	0x104 //offset from ctrl start
#define IO_2K_MASK		0xFFFFF800
#define IO_4K_MASK		0xFFFFF000
#define MAX_RETRY		5



static unsigned int pci_io_read_129x(struct rtl8192cd_priv *priv, unsigned int addr, unsigned char size)
{
	unsigned long ioaddr = priv->pshare->ioaddr;
	unsigned long mask_addr = priv->pshare->mask_addr;
	unsigned long tran_addr = priv->pshare->tran_addr;
	unsigned char busnumber = priv->pshare->pdev->bus->number;
	unsigned int rval = 0;
	unsigned int mask;
	unsigned int translate_val = 0;
	unsigned int tmp_addr = addr & 0xFFF;
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	unsigned long pci_error_reg = priv->pshare->ctrl_start + 0x7C;
	int retry_cnt = 0;
	u32 pci_error_status;
	unsigned long emmc_flags;

#ifdef SMP_SYNC
	SMP_LOCK_IO_129X(flags);
#endif

	//PCIE1.1 0x9804FCEC, PCIE2.0 0x9803CCEC & 0x9803CC68 can't be used because of 1295 hardware issue.
	if((tmp_addr==0xCEC) || ((busnumber==0x01) && (tmp_addr==0xC68))) {
		mask = IO_2K_MASK;
		writel( 0xFFFFF800, IO_TYPE_CAST(mask_addr));
		translate_val = readl(IO_TYPE_CAST(tran_addr));
		writel( translate_val|(addr&mask), IO_TYPE_CAST(tran_addr));
	} else if(addr >= 0x1000) {
		mask = IO_4K_MASK;
		translate_val = readl(IO_TYPE_CAST(tran_addr));
		writel( translate_val|(addr&mask), IO_TYPE_CAST(tran_addr));
	} else
		mask = 0x0;

pci_read_129x_retry:

#ifdef CONFIG_RTK_SW_LOCK_API	
	//All RBUS1 driver need to have a workaround for emmc hardware error.
	//Need to protect 0xXXXX_X8XX~ 0xXXXX_X9XX.
	if((tmp_addr>0x7FF) && (tmp_addr<0xA00))
		rtk_lockapi_lock(emmc_flags, __FUNCTION__);
#endif

	switch(size)
	{
	case 1:
		rval = readb(IO_TYPE_CAST(ioaddr + (addr&~mask)) );
		break;
	case 2:
		rval = readw(IO_TYPE_CAST(ioaddr + (addr&~mask)) );
		break;
	case 4:
		rval = readl(IO_TYPE_CAST(ioaddr + (addr&~mask)) );
		break;
	default:
		panic_printk("RTD129X: %s: wrong size %d\n", __func__, size);
		break;
	}

#ifdef CONFIG_RTK_SW_LOCK_API
	if((tmp_addr>0x7FF) && (tmp_addr<0xA00))
		rtk_lockapi_unlock(emmc_flags, __FUNCTION__);
#endif

	//DLLP error patch
	pci_error_status = readl(IO_TYPE_CAST(pci_error_reg));
	if(pci_error_status & 0x1F) {
		writel(pci_error_status, IO_TYPE_CAST(pci_error_reg));
		panic_printk("RTD129X: %s: DLLP: (#%d)=0x%x reg=%x val=%x\n", __FUNCTION__, retry_cnt, pci_error_status, addr, rval);

		if(retry_cnt < MAX_RETRY) {
			retry_cnt++;
			goto pci_read_129x_retry;
		}
	}

	//PCIE1.1 0x9804FCEC, PCIE2.0 0x9803CCEC & 0x9803CC68 can't be used because of 1295 hardware issue.
	if((tmp_addr==0xCEC) || ((busnumber==0x01) && (tmp_addr==0xC68))) {
		writel( translate_val, IO_TYPE_CAST(tran_addr));
		writel( 0xFFFFF000, IO_TYPE_CAST(mask_addr));
	} else if(addr >= 0x1000) {
		writel( translate_val, IO_TYPE_CAST(tran_addr));
	}

#ifdef SMP_SYNC
	SMP_UNLOCK_IO_129X(flags);
#endif
	return rval;
}

static void pci_io_write_129x(struct rtl8192cd_priv *priv, unsigned int addr, unsigned char size, unsigned int wval)
{
	unsigned long ioaddr = priv->pshare->ioaddr;
	unsigned long mask_addr = priv->pshare->mask_addr;
	unsigned long tran_addr = priv->pshare->tran_addr;
	unsigned char busnumber = priv->pshare->pdev->bus->number;
	unsigned int mask;
	unsigned int translate_val = 0;
	unsigned int tmp_addr = addr & 0xFFF;
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif

	unsigned long emmc_flags;

#ifdef SMP_SYNC
	SMP_LOCK_IO_129X(flags);
#endif
	//PCIE1.1 0x9804FCEC, PCIE2.0 0x9803CCEC & 0x9803CC68 can't be used because of 1295 hardware issue.
	if((tmp_addr==0xCEC) || ((busnumber==0x01) && (tmp_addr==0xC68))) {
		mask = IO_2K_MASK;
		writel( 0xFFFFF800, IO_TYPE_CAST(mask_addr));
		translate_val = readl(IO_TYPE_CAST(tran_addr));
		writel( translate_val|(addr&mask), IO_TYPE_CAST(tran_addr));
	} else if(addr >= 0x1000) {
		mask = IO_4K_MASK;
		translate_val = readl(IO_TYPE_CAST(tran_addr));
		writel( translate_val|(addr&mask), IO_TYPE_CAST(tran_addr));
	} else
		mask = 0x0;

#ifdef CONFIG_RTK_SW_LOCK_API
	//All RBUS1 driver need to have a workaround for emmc hardware error.
	//Need to protect 0xXXXX_X8XX~ 0xXXXX_X9XX.
	if((tmp_addr>0x7FF) && (tmp_addr<0xA00))
		rtk_lockapi_lock(emmc_flags, __FUNCTION__);
#endif

	switch(size)
	{
	case 1:
		writeb( (unsigned char)wval, IO_TYPE_CAST(ioaddr + (addr&~mask)) );
		break;
	case 2:
		writew( (unsigned short)wval, IO_TYPE_CAST(ioaddr + (addr&~mask)) );
		break;
	case 4:
		writel( (unsigned int)wval, IO_TYPE_CAST(ioaddr + (addr&~mask)) );
		break;
	default:
		panic_printk("RTD129X: %s: wrong size %d\n", __func__, size);
		break;
	}

#ifdef CONFIG_RTK_SW_LOCK_API
	if((tmp_addr>0x7FF) && (tmp_addr<0xA00))
		rtk_lockapi_unlock(emmc_flags, __FUNCTION__);
#endif

	//PCIE1.1 0x9804FCEC, PCIE2.0 0x9803CCEC & 0x9803CC68 can't be used because of 1295 hardware issue.
	if((tmp_addr==0xCEC) || ((busnumber==0x01) && (tmp_addr==0xC68))) {
		writel( translate_val, IO_TYPE_CAST(tran_addr));
		writel( 0xFFFFF000, IO_TYPE_CAST(mask_addr));
	} else if(addr >= 0x1000) {
		writel( translate_val, IO_TYPE_CAST(tran_addr));
	}

#ifdef SMP_SYNC
	SMP_UNLOCK_IO_129X(flags);
#endif
}

#endif //RTK_129X_PLATFORM



static inline void asoc_list_unref(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#if defined(CONFIG_PCI_HCI)
	//nothing
#endif //CONFIG_PCI_HCI

#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
#ifdef SMP_SYNC
		unsigned long flags = 0;

		SMP_LOCK_ASOC_LIST(flags);
#endif

		BUG_ON(0 == pstat->asoc_list_refcnt);

		pstat->asoc_list_refcnt--;
		if (0 == pstat->asoc_list_refcnt) {
			list_del_init(&pstat->asoc_list);
#ifdef __ECOS
			cyg_flag_setbits(&pstat->asoc_unref_done, 0x1);
#else
			complete(&pstat->asoc_unref_done);
#endif
		}

#ifdef SMP_SYNC
		SMP_UNLOCK_ASOC_LIST(flags);
#endif
	}
#endif //CONFIG_USB_HCI || CONFIG_SDIO_HCI
#endif //CONFIG_RTL_TRIBAND_SUPPORT
}


static inline struct list_head* asoc_list_get_next(struct rtl8192cd_priv *priv, struct list_head *plist)
{
#if defined(CONFIG_PCI_HCI)
		if (GET_HCI_TYPE(priv) == RTL_HCI_PCIE) {
			return plist->next;
		}
#endif //CONFIG_PCI_HCI

#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
#if defined(CONFIG_USB_HCI) || defined(CONFIG_SDIO_HCI)
	if (GET_HCI_TYPE(priv) == RTL_HCI_USB || GET_HCI_TYPE(priv) == RTL_HCI_SDIO) {
#ifdef SMP_SYNC
		unsigned long flags = 0;
#endif
		struct list_head *phead, *plist_next;
		struct stat_info *pstat;
		
		phead = &priv->asoc_list;

#ifdef SMP_SYNC
		SMP_LOCK_ASOC_LIST(flags);
#endif
		
		plist_next = plist->next;
		if (plist_next != phead) {
			pstat = list_entry(plist_next, struct stat_info, asoc_list);
			pstat->asoc_list_refcnt++;
#ifdef __ECOS
			cyg_flag_maskbits(&pstat->asoc_unref_done, ~0x1);
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0))
			reinit_completion(&pstat->asoc_unref_done);
#else
			INIT_COMPLETION(pstat->asoc_unref_done);
#endif
#endif
		}

#ifdef SMP_SYNC
		SMP_UNLOCK_ASOC_LIST(flags);
#endif
		
		if (plist != phead) {
			pstat =  list_entry(plist, struct stat_info, asoc_list);
			asoc_list_unref(priv, pstat);
		}

		return plist_next;
	}
#endif //CONFIG_USB_HCI || CONFIG_SDIO_HCI
#endif //CONFIG_RTL_TRIBAND_SUPPORT
}


#if defined(__ECOS) && defined(WLAN_REG_FW_RAM_REFINE)
extern unsigned char RTL_R8_F(struct rtl8192cd_priv *priv, unsigned int reg);
extern unsigned short RTL_R16_F(struct rtl8192cd_priv *priv, unsigned int reg);
extern unsigned int RTL_R32_F(struct rtl8192cd_priv *priv, unsigned int reg);
extern void RTL_W8_F(struct rtl8192cd_priv *priv, unsigned int reg, unsigned char val8);
extern void RTL_W16_F(struct rtl8192cd_priv *priv, unsigned int reg, unsigned short val16);
extern void RTL_W32_F(struct rtl8192cd_priv *priv, unsigned int reg, unsigned int val32);
#else
static __inline__ unsigned char RTL_R8_F(struct rtl8192cd_priv *priv, unsigned int reg)
{
	unsigned long ioaddr = priv->pshare->ioaddr;
	unsigned char val8 = 0;

#if defined(CONFIG_WLAN_HAL_8197F) || defined(CONFIG_WLAN_HAL_8197G)
	if ((GET_CHIP_VER(priv) == VERSION_8197F) || (GET_CHIP_VER(priv) == VERSION_8197G)){
		if(!(REG32(0xB8000064)&BIT0)){
			panic_printk("Should not access WiFi register since 0xB8000064[0]=0,reg=0x%x\n",reg);
			return;
	  } 			   
	}
#endif

#ifdef IO_MAPPING
	unsigned char page = ((unsigned char)(reg >> 8)) & PAGE_NUM;
	if (priv->pshare->io_mapping && page)
	{
#ifndef SMP_SYNC
		unsigned long x;
#endif
		SAVE_INT_AND_CLI(x);

		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) | page, IO_TYPE_CAST(ioaddr + _PSR_));
		val8 = readb(IO_TYPE_CAST(ioaddr + (reg & 0x000000ff)));
		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) & (~PAGE_NUM), IO_TYPE_CAST(ioaddr + _PSR_));

		RESTORE_INT(x);
	}
	else
#endif
	{
#ifdef CONFIG_RTL_8198
		unsigned int data=0;
		int swap[4]={0,8,16,24};
		int diff = reg&0x3;
		data=REG32((ioaddr + (reg&(0xFFFFFFFC)) ) );
		val8=(unsigned char)(( data>>swap[diff])&0xff);
#elif defined(RTK_129X_PLATFORM)
		val8 = pci_io_read_129x(priv, reg, 1);
#else
		val8 = readb(IO_TYPE_CAST(ioaddr + reg));
#endif
	}

	return val8;
}

static __inline__ unsigned short RTL_R16_F(struct rtl8192cd_priv *priv, unsigned int reg)
{
	unsigned long ioaddr = priv->pshare->ioaddr;
	unsigned short val16 = 0;

#if defined(CONFIG_WLAN_HAL_8197F) || defined(CONFIG_WLAN_HAL_8197G)
	if ((GET_CHIP_VER(priv) == VERSION_8197F) || (GET_CHIP_VER(priv) == VERSION_8197G)){
		if(!(REG32(0xB8000064)&BIT0)){
			panic_printk("Should not access WiFi register since 0xB8000064[0]=0,reg=0x%x\n",reg);
			return 0;
	  } 			   
	}
#endif

	if (reg & 0x00000001) {
		panic_printk("Unaligned read to reg 0x%08x!\n", reg);
        return 0;
	}

#ifdef IO_MAPPING
	unsigned char page = ((unsigned char)(reg >> 8)) & PAGE_NUM;
	if (priv->pshare->io_mapping && page)
	{
#ifndef SMP_SYNC
		unsigned long x;
#endif
		SAVE_INT_AND_CLI(x);

		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) | page, IO_TYPE_CAST(ioaddr + _PSR_));
		val16 = readw(IO_TYPE_CAST(ioaddr + (reg & 0x000000ff)));
		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) & (~PAGE_NUM), IO_TYPE_CAST(ioaddr + _PSR_));

		RESTORE_INT(x);
	}
	else
#endif
	{
#ifdef CONFIG_RTL_8198
		unsigned int data=0;
		int swap[4]={0,8,16,24};
		int diff = reg&0x3;
		data=REG32((ioaddr + (reg&(0xFFFFFFFC)) ) );
		val16=(unsigned short)(( data>>swap[diff])&0xffff);
#elif defined(RTK_129X_PLATFORM)
		val16 = pci_io_read_129x(priv, reg, 2);
#else
		val16 = readw(IO_TYPE_CAST(ioaddr + reg));
#endif
	}

#ifdef CHECK_SWAP
	if (priv->pshare->type & ACCESS_SWAP_IO)
		val16 = le16_to_cpu(val16);
#endif

	return val16;
}

static __inline__ unsigned int RTL_R32_F(struct rtl8192cd_priv *priv, unsigned int reg)
{
	unsigned long ioaddr = priv->pshare->ioaddr;
	unsigned int val32 = 0;

#if defined(CONFIG_WLAN_HAL_8197F) || defined(CONFIG_WLAN_HAL_8197G)
	if ((GET_CHIP_VER(priv) == VERSION_8197F) || (GET_CHIP_VER(priv) == VERSION_8197G)){
		if(!(REG32(0xB8000064)&BIT0)){
			panic_printk("Should not access WiFi register since 0xB8000064[0]=0,reg=0x%x\n",reg);
			return 0;
	  } 			   
	}
#endif

	if (reg & 0x00000003) {
		panic_printk("Unaligned read to reg 0x%08x!\n", reg);
        return 0;
	}

#ifdef IO_MAPPING
	unsigned char page = ((unsigned char)(reg >> 8)) & PAGE_NUM;
	if (priv->pshare->io_mapping && page)
	{
#ifndef SMP_SYNC
		unsigned long x;
#endif
		SAVE_INT_AND_CLI(x);

		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) | page, IO_TYPE_CAST(ioaddr + _PSR_));
		val32 = readl(IO_TYPE_CAST(ioaddr + (reg & 0x000000ff)));
		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) & (~PAGE_NUM), IO_TYPE_CAST(ioaddr + _PSR_));

		RESTORE_INT(x);
	}
	else
#endif
	{
#if defined(RTK_129X_PLATFORM)
		val32 = pci_io_read_129x(priv, reg, 4);
#else
		val32 = readl(IO_TYPE_CAST(ioaddr + reg));
#endif
	}

#ifdef CHECK_SWAP
	if (priv->pshare->type & ACCESS_SWAP_IO)
		val32 = le32_to_cpu(val32);
#endif

	return val32;
}

static __inline__ void RTL_W8_F(struct rtl8192cd_priv *priv, unsigned int reg, unsigned char val8)
{
	unsigned long ioaddr = priv->pshare->ioaddr;
	
#if defined(CONFIG_WLAN_HAL_8197F) || defined(CONFIG_WLAN_HAL_8197G)
        if ((GET_CHIP_VER(priv) == VERSION_8197F) || (GET_CHIP_VER(priv) == VERSION_8197G)){
			if(!(REG32(0xB8000064)&BIT0)){
		panic_printk("Should not access WiFi register since 0xB8000064[0]=0,reg=0x%x\n",reg);
		return;
	  } 			   
	}
#endif

#ifdef IO_MAPPING
	unsigned char page = ((unsigned char)(reg >> 8)) & PAGE_NUM;
	if (priv->pshare->io_mapping && page)
	{
#ifndef SMP_SYNC
		unsigned long x;
#endif
		SAVE_INT_AND_CLI(x);

		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) | page, IO_TYPE_CAST(ioaddr + _PSR_));
		writeb(val8, IO_TYPE_CAST(ioaddr + (reg & 0x000000ff)));
		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) & (~PAGE_NUM), IO_TYPE_CAST(ioaddr + _PSR_));

		RESTORE_INT(x);
	}
	else
#endif
	{
#ifdef RTK_129X_PLATFORM
		pci_io_write_129x(priv, reg, 1, val8);
#else
		writeb(val8, IO_TYPE_CAST(ioaddr + reg));
#endif
	}
}

static __inline__ void RTL_W16_F(struct rtl8192cd_priv *priv, unsigned int reg, unsigned short val16)
{
	unsigned long ioaddr = priv->pshare->ioaddr;
	unsigned short val16_n = val16;
#ifdef IO_MAPPING
	unsigned char page;
#endif

#if defined(CONFIG_WLAN_HAL_8197F) || defined(CONFIG_WLAN_HAL_8197G)
	if ((GET_CHIP_VER(priv) == VERSION_8197F) || (GET_CHIP_VER(priv) == VERSION_8197G)){
		if(!(REG32(0xB8000064)&BIT0)){
		panic_printk("Should not access WiFi register since 0xB8000064[0]=0,reg=0x%x\n",reg);
		return;
	  } 			   
	}
#endif

	if (reg & 0x00000001) {
		panic_printk("Unaligned write to reg 0x%08x!, val16=0x%08x!\n", reg, val16);
        return;
	}

#ifdef CHECK_SWAP
	if (priv->pshare->type & ACCESS_SWAP_IO)
		val16_n = cpu_to_le16(val16);
#endif

#ifdef IO_MAPPING
	page = ((unsigned char)(reg >> 8)) & PAGE_NUM;
	if (priv->pshare->io_mapping && page)
	{
#ifndef SMP_SYNC
		unsigned long x;
#endif
		SAVE_INT_AND_CLI(x);

		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) | page, IO_TYPE_CAST(ioaddr + _PSR_));
		writew(val16_n, IO_TYPE_CAST(ioaddr + (reg & 0x000000ff)));
		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) & (~PAGE_NUM), IO_TYPE_CAST(ioaddr + _PSR_));

		RESTORE_INT(x);
	}
	else
#endif
	{
#ifdef RTK_129X_PLATFORM
		pci_io_write_129x(priv, reg, 2, val16_n);
#else
		writew(val16_n, IO_TYPE_CAST(ioaddr + reg));
#endif
	}
}

static __inline__ void RTL_W32_F(struct rtl8192cd_priv *priv, unsigned int reg, unsigned int val32)
{
	unsigned long ioaddr = priv->pshare->ioaddr;
	unsigned int val32_n = val32;
#ifdef IO_MAPPING
	unsigned char page;
#endif

#if defined(CONFIG_WLAN_HAL_8197F) || defined(CONFIG_WLAN_HAL_8197G)
	if ((GET_CHIP_VER(priv) == VERSION_8197F) || (GET_CHIP_VER(priv) == VERSION_8197G)){
		if(!(REG32(0xB8000064)&BIT0)){
		panic_printk("Should not access WiFi register since 0xB8000064[0]=0,reg=0x%x\n",reg);
		return;
	  } 			   
	}
#endif

	if (reg & 0x00000003) {
		panic_printk("Unaligned write to reg 0x%08x!, val32=0x%08x!\n", reg, val32);
        return;
	}

#ifdef CHECK_SWAP
	if (priv->pshare->type & ACCESS_SWAP_IO)
		val32_n = cpu_to_le32(val32);
#endif

#ifdef IO_MAPPING
	page = ((unsigned char)(reg >> 8)) & PAGE_NUM;
	if (priv->pshare->io_mapping && page)
	{
#ifndef SMP_SYNC
		unsigned long x;
#endif
		SAVE_INT_AND_CLI(x);

		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) | page, IO_TYPE_CAST(ioaddr + _PSR_));
		writel(val32_n, IO_TYPE_CAST(ioaddr + (reg & 0x000000ff)));
		writeb(readb(IO_TYPE_CAST(ioaddr + _PSR_)) & (~PAGE_NUM), IO_TYPE_CAST(ioaddr + _PSR_));

		RESTORE_INT(x);
	}
	else
#endif
	{
#ifdef RTK_129X_PLATFORM
		pci_io_write_129x(priv, reg, 4, val32_n);
#else
		writel(val32_n, IO_TYPE_CAST(ioaddr + reg));
#endif
	}
}
#endif

#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
#if 1//defined(_8192CD_TRIBNAD_UTIL_RW_H_)
static inline unsigned char __HAL_RTL_R8(struct rtl8192cd_priv *priv, u32 addr)
{
	unsigned char ret = 0;
	switch (GET_HCI_TYPE(priv)) {
#if defined(CONFIG_PCI_HCI)
	case RTL_HCI_PCIE:
		#ifdef PCIE_POWER_SAVING_TEST //yllin
		ret = (( priv->pwr_state==L2  || priv->pwr_state==L1) ? 0 :(RTL_R8_F(priv, reg)) );
		#else
		ret = RTL_R8_F(priv, addr);
		#endif
		break;
#endif
#if defined(CONFIG_USB_HCI)
	case RTL_HCI_USB:
		ret = usb_read8(priv, addr);
		break;
#endif
#if defined(CONFIG_SDIO_HCI)
	case RTL_HCI_SDIO:
		ret = sdio_read8(priv, addr);
		break;
#endif
	}
	return ret;
}

static inline unsigned short __HAL_RTL_R16(struct rtl8192cd_priv *priv, u32 addr)
{
	unsigned short ret = 0;
	switch (GET_HCI_TYPE(priv)) {
#if defined(CONFIG_PCI_HCI)
	case RTL_HCI_PCIE:
		#ifdef PCIE_POWER_SAVING_TEST //yllin
		ret = (( priv->pwr_state==L2  || priv->pwr_state==L1) ? 0 : (RTL_R16_F(priv, reg)));
		#else
		ret = RTL_R16_F(priv, addr);
		#endif
		break;
#endif
#if defined(CONFIG_USB_HCI)
	case RTL_HCI_USB:
		ret = usb_read16(priv, addr);
		break;
#endif
#if defined(CONFIG_SDIO_HCI)
	case RTL_HCI_SDIO:
		ret = sdio_read16(priv, addr);
		break;
#endif
	}
	return ret;
}

static inline unsigned int __HAL_RTL_R32(struct rtl8192cd_priv *priv, u32 addr)
{
	unsigned int ret = 0;
	switch (GET_HCI_TYPE(priv)) {
#if defined(CONFIG_PCI_HCI)
	case RTL_HCI_PCIE:
		#ifdef PCIE_POWER_SAVING_TEST //yllin
		ret = (( priv->pwr_state==L2  || priv->pwr_state==L1) ? 0 : (RTL_R32_F(priv, reg)));
		#else
		ret = RTL_R32_F(priv, addr);
		#endif
		break;
#endif
#if defined(CONFIG_USB_HCI)
	case RTL_HCI_USB:
		ret = usb_read32(priv, addr);
		break;
#endif
#if defined(CONFIG_SDIO_HCI)
	case RTL_HCI_SDIO:
		ret = sdio_read32(priv, addr);
		break;
#endif
	}
	return ret;
}

static inline void __HAL_RTL_W8(struct rtl8192cd_priv *priv, u32 addr, u32 val)
{	
	switch (GET_HCI_TYPE(priv)) {
#if defined(CONFIG_PCI_HCI)
	case RTL_HCI_PCIE:
		#ifdef PCIE_POWER_SAVING_TEST //yllin
		if( priv->pwr_state==L2  || priv->pwr_state==L1)
		{  	printk("Error!!! w8:%x,%x in L%d\n", addr, val, priv->pwr_state);}
		else
			RTL_W8_F(priv, addr, val);
		#else
		RTL_W8_F(priv, addr, val);
		#endif
		break;
#endif
#if defined(CONFIG_USB_HCI)
	case RTL_HCI_USB:
		usb_write8(priv, addr, val);
		break;
#endif
#if defined(CONFIG_SDIO_HCI)
	case RTL_HCI_SDIO:
		sdio_read8(priv, addr, val);
		break;
#endif
	}
}

static inline void __HAL_RTL_W16(struct rtl8192cd_priv *priv, u32 addr, u32 val)
{
	switch (GET_HCI_TYPE(priv)) {
#if defined(CONFIG_PCI_HCI)
	case RTL_HCI_PCIE:
		#ifdef PCIE_POWER_SAVING_TEST //yllin
		if( priv->pwr_state==L2  || priv->pwr_state==L1)
		{  	printk("Error!!! w16:%x,%x in L%d\n", addr, val, priv->pwr_state);}
		else
			RTL_W16_F(priv, addr, val);
		#else
		RTL_W16_F(priv, addr, val);
		#endif
		break;
#endif
#if defined(CONFIG_USB_HCI)
	case RTL_HCI_USB:
		usb_write16(priv, addr, val);
		break;
#endif
#if defined(CONFIG_SDIO_HCI)
	case RTL_HCI_SDIO:
		sdio_write16(priv, addr, val);
		break;
#endif
	}
}

static inline void __HAL_RTL_W32(struct rtl8192cd_priv *priv, u32 addr, u32 val)
{
	switch (GET_HCI_TYPE(priv)) {
#if defined(CONFIG_PCI_HCI)
	case RTL_HCI_PCIE:
		#ifdef PCIE_POWER_SAVING_TEST //yllin
		if( priv->pwr_state==L2  || priv->pwr_state==L1)
		{  	printk("Error!!! w32:%x,%x in L%d\n", addr, val, priv->pwr_state);}
		else
			RTL_W32_F(priv, addr, val);
		#else
		RTL_W32_F(priv, addr, val);
		#endif
		break;
#endif
#if defined(CONFIG_USB_HCI)
	case RTL_HCI_USB:
		usb_write32(priv, addr, val);
		break;
#endif
#if defined(CONFIG_SDIO_HCI)
	case RTL_HCI_SDIO:
		sdio_write32(priv, addr, val);
		break;
#endif
	}
}

#endif /* defined(_8192CD_TRIBNAD_UTIL_RW_H_) */


#define RTL_R8(reg)		\
    (__HAL_RTL_R8(priv, reg))

#define RTL_R16(reg)	\
    (__HAL_RTL_R16(priv, reg))

#define RTL_R32(reg)	\
    (__HAL_RTL_R32(priv, reg))

#define RTL_W8(reg, val8)	\
    do { \
        __HAL_RTL_W8(priv, reg, val8); \
    } while (0)

#define RTL_W16(reg, val16)	\
    do { \
        __HAL_RTL_W16(priv, reg, val16); \
    } while (0)

#define RTL_W32(reg, val32)	\
    do { \
        __HAL_RTL_W32(priv, reg, val32) ; \
    } while (0)

#elif defined(PCIE_POWER_SAVING) || defined(PCIE_POWER_SAVING_TEST)

#define RTL_R8(reg)		\
	(( priv->pwr_state==L2  || priv->pwr_state==L1) ? 0 :(RTL_R8_F(priv, reg)) )

#define RTL_R16(reg)	\
	(( priv->pwr_state==L2  || priv->pwr_state==L1) ? 0 : (RTL_R16_F(priv, reg)))

#define RTL_R32(reg)	\
	(( priv->pwr_state==L2  || priv->pwr_state==L1) ? 0 : (RTL_R32_F(priv, reg)))

#define RTL_W8(reg, val8)	\
	do { \
	if( priv->pwr_state==L2  || priv->pwr_state==L1) \
		{  	printk("Error!!! w8:%x,%x in L%d\n", reg, val8, priv->pwr_state);} \
	else \
		RTL_W8_F(priv, reg, val8); \
	} while (0)

#define RTL_W16(reg, val16)	\
	do { \
	if( priv->pwr_state==L2  || priv->pwr_state==L1) \
		printk("Err!!! w16:%x,%x in L%d\n", reg, val16, priv->pwr_state); \
	else \
		RTL_W16_F(priv, reg, val16); \
	} while (0)

#define RTL_W32(reg, val32)	\
	do { \
	if( priv->pwr_state==L2  || priv->pwr_state==L1) \
		printk("Err!!! w32:%x,%x in L%d\n", reg, (unsigned int)val32, priv->pwr_state); \
	else \
		RTL_W32_F(priv, reg, val32) ; \
	} while (0)

#else

#define RTL_R8(reg)		\
	(RTL_R8_F(priv, reg))

#define RTL_R16(reg)	\
	(RTL_R16_F(priv, reg))

#define RTL_R32(reg)	\
	(RTL_R32_F(priv, reg))

#define RTL_W8(reg, val8)	\
	do { \
		RTL_W8_F(priv, reg, val8); \
	} while (0)

#define RTL_W16(reg, val16)	\
	do { \
		RTL_W16_F(priv, reg, val16); \
	} while (0)

#define RTL_W32(reg, val32)	\
	do { \
		RTL_W32_F(priv, reg, val32) ; \
	} while (0)

#endif

#if defined(CONFIG_RTL_TRIBAND_SUPPORT)
#define get_desc(val)	le32_to_cpu(val)
#define set_desc(val)	cpu_to_le32(val)
#elif defined(CHECK_SWAP)
//#define get_desc(val)	((priv->pshare->type & ACCESS_SWAP_MEM) ? le32_to_cpu(val) : val)
//#define set_desc(val)	((priv->pshare->type & ACCESS_SWAP_MEM) ? cpu_to_le32(val) : val)
#else
#define get_desc(val)	(val)
#define set_desc(val)	(val)
#endif

#endif // USE_IO_OPS
#endif // CONFIG_PCI_HCI


enum _skb_flag_ {
	_SKB_TX_ = 1,
	_SKB_RX_ = 2,
	_SKB_RX_IRQ_ = 4,
	_SKB_TX_IRQ_ = 8
};


#if defined(CONFIG_PCI_HCI) || defined(CONFIG_RTL_TRIBAND_SUPPORT)

static __inline__ unsigned long get_physical_addr(struct rtl8192cd_priv *priv, void *ptr,
				unsigned int size, int direction)
{
#if defined(CONFIG_USB_HCI) && defined(CONFIG_RTL_TRIBAND_SUPPORT)
  if (GET_HCI_TYPE(priv) == RTL_HCI_USB) {
	  return (unsigned long)ptr;
  }
#endif

#if defined(CONFIG_NET_PCI) && !defined(USE_RTL8186_SDK)
	if ((IS_PCIBIOS_TYPE) && (0 != size))
		return pci_map_single(priv->pshare->pdev, ptr, size, direction);
	else
#endif
		return (virt_to_bus(ptr)+CONFIG_LUNA_SLAVE_PHYMEM_OFFSET);
}

#endif // CONFIG_PCI_HCI




#if defined(__ECOS) && defined(WLAN_REG_FW_RAM_REFINE)
extern int get_rf_mimo_mode(struct rtl8192cd_priv *priv);
#else
static __inline__ int get_rf_mimo_mode(struct rtl8192cd_priv *priv)
{
#if 0 //defined(CONFIG_WLAN_HAL_8822BE)
	if(GET_CHIP_VER(priv) == VERSION_8822B)
		return RF_2T2R;
#endif

#if defined(CONFIG_WLAN_HAL_8821CE)
	if(GET_CHIP_VER(priv) == VERSION_8821C)
		return RF_1T1R;
#endif

	if ((priv->pshare->phw->MIMO_TR_hw_support == RF_1T1R) ||
		(priv->pmib->dot11RFEntry.MIMO_TR_mode == RF_1T1R))
		return RF_1T1R;
#ifdef CONFIG_RTL_92D_SUPPORT
	else if ((priv->pshare->phw->MIMO_TR_hw_support == RF_1T2R) ||
		(priv->pmib->dot11RFEntry.MIMO_TR_mode == RF_1T2R)) 
		return RF_1T2R;
#endif
	else if (priv->pshare->phw->MIMO_TR_hw_support == RF_2T2R || 
		priv->pmib->dot11RFEntry.MIMO_TR_mode == RF_2T2R ||
		priv->pmib->dot11RFEntry.MIMO_TR_mode == RF_2T4R)
		return RF_2T2R;
	else if (priv->pshare->phw->MIMO_TR_hw_support == RF_3T3R ||
		priv->pmib->dot11RFEntry.MIMO_TR_mode == RF_3T3R)
		return RF_3T3R;
	else if (priv->pshare->phw->MIMO_TR_hw_support == RF_4T4R)
		return RF_4T4R;

	return RF_2T2R; //2ss as default
}
#endif


// Free net device socket buffer
#ifdef __OSK__
extern
__IRAM_WIFI_PRI2
void rtl_kfree_skb(struct rtl8192cd_priv *priv, struct sk_buff *skb, int flag);
#else
static __inline__ void rtl_kfree_skb(struct rtl8192cd_priv *priv, struct sk_buff *skb, int flag)
{
#ifdef ENABLE_RTL_SKB_STATS
	if (flag & (_SKB_TX_ | _SKB_TX_IRQ_))
		rtl_atomic_dec(&priv->rtl_tx_skb_cnt);
	else
		rtl_atomic_dec(&priv->rtl_rx_skb_cnt);
#endif

	dev_kfree_skb_any(skb);
}
#endif

#if defined(CONFIG_WLAN_HAL_8197F) 
extern
#ifdef __OSK__
	__IRAM_WIFI_PRI1
#else
	__MIPS16
	__IRAM_IN_865X
#endif
struct stat_info *get_stainfo_hash(struct rtl8192cd_priv *priv, unsigned char *hwaddr);

#if defined(__ECOS) && defined(WLAN_REG_FW_RAM_REFINE)
extern struct stat_info *get_stainfo(struct rtl8192cd_priv *priv, unsigned char *hwaddr);
#else
static inline struct stat_info *get_stainfo(struct rtl8192cd_priv *priv, unsigned char *hwaddr)
{
	struct stat_info	*pstat;

#ifdef RTK_NL80211
	if(hwaddr == NULL)
		return (struct stat_info *)NULL;
#endif

	//if (!memcmp(hwaddr, priv->stainfo_cache.hwaddr, MACADDRLEN) &&  priv->stainfo_cache.pstat)
	pstat = priv->pstat_cache;

#ifdef MULTI_MAC_CLONE
	if ((priv->pmib->dot11OperationEntry.opmode & WIFI_STATION_STATE) && MCLONE_NUM > 0) {
	    if (pstat && !memcmp(hwaddr, pstat->cmn_info.mac_addr, MACADDRLEN) && pstat->mclone_id == ACTIVE_ID)
		return pstat;
	}
	else
#endif
	{
	    if(pstat && isEqualMACAddr(hwaddr, pstat->cmn_info.mac_addr))
			return pstat;
	}
	
	return get_stainfo_hash(priv, hwaddr);
}
#endif

#if !defined(CONFIG_WLAN_STATS_EXTENTION)
static inline int IS_BSSID(struct rtl8192cd_priv *priv, unsigned char *da)
{
	unsigned char *bssid;
	bssid = priv->pmib->dot11StationConfigEntry.dot11Bssid;

	//if (!memcmp(da, bssid, 6))
	if (isEqualMACAddr(da, bssid))
		return TRUE;
	else
		return FALSE;
}

static inline int IS_MCAST(unsigned char *da)
{
	if ((*da) & 0x01)
		return TRUE;
	else
		return FALSE;
}

static inline int IS_BCAST2(unsigned char *da)
{
     if ((*da) == 0xff)
         return TRUE;
     else
         return FALSE;
}
#endif
#endif


// Allocate net device socket buffer
#ifdef __OSK__
extern
__IRAM_WIFI_PRI2
struct sk_buff *rtl_dev_alloc_skb(struct rtl8192cd_priv *priv,
				unsigned int length, int flag, int could_alloc_from_kerenl);
#else
extern __MIPS16 __IRAM_IN_865X struct sk_buff *alloc_skb_from_queue(struct rtl8192cd_priv *priv);
static __inline__ struct sk_buff *rtl_dev_alloc_skb(struct rtl8192cd_priv *priv,
				unsigned int length, int flag, int could_alloc_from_kerenl)
{
	struct sk_buff *skb = NULL;

#if defined(__ECOS) && defined(CONFIG_SDIO_HCI)
	skb = dev_alloc_skb(length);
#else
	skb = alloc_skb_from_queue(priv);

	if (skb == NULL && could_alloc_from_kerenl)
		skb = dev_alloc_skb(length);
#endif

#ifdef ENABLE_RTL_SKB_STATS
	if (NULL != skb) {
		if (flag & (_SKB_TX_ | _SKB_TX_IRQ_))
			rtl_atomic_inc(&priv->rtl_tx_skb_cnt);
		else
			rtl_atomic_inc(&priv->rtl_rx_skb_cnt);
	}
#endif

	return skb;
}
#endif



#ifdef __OSK__
extern __IRAM_WIFI_PRI3 void rtl_cache_sync_wback(struct rtl8192cd_priv *priv, unsigned int start,
				unsigned int size, int direction);
#else
#if defined(__MIPSEB__) || defined(CONFIG_RTL_8197F) || defined(CONFIG_RTL_8197G)

static __inline__ void rtl_cache_sync_wback(struct rtl8192cd_priv *priv, unsigned long start,
				unsigned int size, int direction)
{
  #if defined(CONFIG_USB_HCI) && defined(CONFIG_RTL_TRIBAND_SUPPORT)
	if (GET_HCI_TYPE(priv) == RTL_HCI_USB) {
		return;
	}
  #endif

		if (0 == size) return;	// if the size of cache sync is equal to zero, don't do sync action

#ifdef __LINUX_2_6__
		start = CPHYSADDR(start)+CONFIG_LUNA_SLAVE_PHYMEM_OFFSET;//CPHYSADDR is virt_to_bus
#endif //__LINUX_2_6__
#if defined(CONFIG_NET_PCI) && !defined(USE_RTL8186_SDK)
		if (IS_PCIBIOS_TYPE) {
#ifdef __LINUX_2_6__
			if (direction == PCI_DMA_FROMDEVICE)
				pci_dma_sync_single_for_cpu(priv->pshare->pdev, start, size, direction);
			else if (direction == PCI_DMA_TODEVICE)
				pci_dma_sync_single_for_device(priv->pshare->pdev, start, size, direction);
#else
			pci_dma_sync_single(priv->pshare->pdev, start, size, direction);
#endif
		}
		else
			dma_cache_wback_inv((unsigned long)bus_to_virt(start-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), size);
#else

#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F) || defined(CONFIG_RTL_8197G)
		if (direction == PCI_DMA_FROMDEVICE)
		    _dma_cache_inv((unsigned long)bus_to_virt(start-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), size);
		else
#endif		
    		_dma_cache_wback_inv((unsigned long)bus_to_virt(start-CONFIG_LUNA_SLAVE_PHYMEM_OFFSET), size);
#endif //#if defined(CONFIG_NET_PCI) && !defined(USE_RTL8186_SDK)

}

#elif defined(CONFIG_RTL_8198F)
 static __inline__ void rtl_cache_sync_wback(struct rtl8192cd_priv *priv, unsigned long start,
				unsigned int size, int direction)
{
    if (direction == PCI_DMA_FROMDEVICE){
#ifdef TAROKO_0      
        dcache_invalidate_range(start,start + size);
#else
        //outer_inv_range((unsigned long)(start), (unsigned long)(start) + size);  
        dmac_flush_range((unsigned long)(start), (unsigned long)(start)+size);
#endif
    }
	else	
	{
#ifdef TAROKO_0
        dcache_flush_range(start,start + size);
#else
        //printk("%s %d 8198F debug caller = %p start = %x size = %x !!!!! \n",__func__,__LINE__,__builtin_return_address(0),start,size);	
	    __cpuc_flush_dcache_area((unsigned long)(start), size);
#endif
	}
		//flush_kern_dcache_area((unsigned long)(start), size);
} 

#else
static __inline__ void rtl_cache_sync_wback(struct rtl8192cd_priv *priv, unsigned long start,
				unsigned int size, int direction)
{
		if (0 == size) return;	// if the size of cache sync is equal to zero, don't do sync action

#ifdef __LINUX_2_6__
		start = virt_to_bus((void*)start);
		if (direction == PCI_DMA_FROMDEVICE)
			pci_dma_sync_single_for_cpu(priv->pshare->pdev, start, size, direction);
		else if (direction == PCI_DMA_TODEVICE)
			pci_dma_sync_single_for_device(priv->pshare->pdev, start, size, direction);
#else
		pci_dma_sync_single(priv->pshare->pdev, start, size, direction);
#endif //__LINUX_2_6__
}
#endif//#if defined(__MIPSEB__) || defined(CONFIG_RTL_8197F) 
#endif//__OSK__


#if defined(RTK_AC_SUPPORT) || defined(CONFIG_WLAN_HAL_8198F)
static __inline__ int is_VHT_rate(unsigned char rate)
{
	if (rate >= VHT_RATE_ID)
		return TRUE;
	else
		return FALSE;
}

static __inline__ int is_fixedVHTTxRate(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef WDS
	if (pstat->state & WIFI_WDS) 
		return ((priv->pmib->dot11WdsInfo.entry[pstat->wds_idx].txRate & BIT(31)) ? 1 : 0); 
	else			
#endif		
	return ((priv->pmib->dot11StationConfigEntry.fixedTxRate & BIT(31)) ? 1 : 0);
}
#endif



static __inline__ int is_auto_rate(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef WDS
		if (pstat->state & WIFI_WDS) 
			return ((priv->pmib->dot11WdsInfo.entry[pstat->wds_idx].txRate==0) ? 1: 0);
		else
#endif
			return (priv->pmib->dot11StationConfigEntry.autoRate);
}

static __inline__ int is_fixedMCSTxRate(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef WDS
	if (pstat->state & WIFI_WDS)
		return (priv->pmib->dot11WdsInfo.entry[pstat->wds_idx].txRate & 0xffff000) ;
	else			
#endif		
	return (priv->pmib->dot11StationConfigEntry.fixedTxRate & 0xffff000);
}


static __inline__ int is_CCK_rate(unsigned char rate)
{
	if ((rate == 2) || (rate == 4) || (rate == 11) || (rate == 22))
		return TRUE;
	else
		return FALSE;
}

static __inline__ int is_MCS_rate(unsigned char rate)
{
	if (rate >= HT_RATE_ID)
		return TRUE;
	else
		return FALSE;
}

static __inline__ int is_2T_rate(unsigned char rate)
{
#ifdef RTK_AC_SUPPORT
	if ((rate >= _NSS2_MCS0_RATE_) && (rate <= _NSS2_MCS9_RATE_)) 
		return TRUE;
	else
#endif
		return ((rate >= _MCS8_RATE_) && (rate <= _MCS16_RATE_)) ? TRUE : FALSE;
}

static __inline__ void tx_sum_up(struct rtl8192cd_priv *priv, struct stat_info *pstat, struct tx_insn* txcfg)
{
	struct net_device_stats *pnet_stats;
	unsigned int pktlen = txcfg->fr_len+txcfg->hdr_len+txcfg->iv+txcfg->llc+txcfg->mic+txcfg->icv;

	if (priv) {
#if defined(CONFIG_RTL8672) || defined(CONFIG_WLAN_STATS_EXTENTION)
		extern int IS_BCAST2(unsigned char *da);
		extern int IS_MCAST(unsigned char *da);
		extern unsigned char *get_da(unsigned char *pframe);
		unsigned char *da;
#endif
		pnet_stats = &(priv->net_stats);

#ifdef CONFIG_RTL8672
		if (txcfg->fr_type != _SKB_FRAME_TYPE_){
		    priv->ext_stats.tx_mgnt_pkts++;
		} else
#endif
		{
			pnet_stats->tx_packets++;
			pnet_stats->tx_bytes += pktlen;
		}

#if defined(CONFIG_RTL8672) || defined(CONFIG_WLAN_STATS_EXTENTION)
#ifdef SUPPORT_TX_AMSDU
		if ((txcfg->aggre_en == FG_AGGRE_MSDU_MIDDLE) || (txcfg->aggre_en == FG_AGGRE_MSDU_LAST))
			priv->ext_stats.tx_ucast_pkts_cnt++;
		else
#endif
		{
			da = get_da(txcfg->phdr);
			if (IS_BCAST2(da))
				priv->ext_stats.tx_bcast_pkts_cnt++;
			else if (IS_MCAST(da))
				priv->ext_stats.tx_mcast_pkts_cnt++;
			else
				priv->ext_stats.tx_ucast_pkts_cnt++;
		}
#endif

#ifdef TRX_DATA_LOG
		if (txcfg->fr_type == _SKB_FRAME_TYPE_)
			priv->ext_stats.tx_data_packets++;
#endif	
		priv->ext_stats.tx_byte_cnt += pktlen;

		// bcm old 11n chipset iot debug, and TXOP enlarge
		priv->pshare->current_tx_bytes += pktlen;

		if (pstat)
			priv->pshare->NumTxBytesUnicast += pktlen;
	}

	if (pstat) {

#if defined(TXREPORT)
#if defined(TESTCHIP_SUPPORT) && defined(CONFIG_RTL_92C_SUPPORT)
		if (IS_TEST_CHIP(priv) && (GET_CHIP_VER(priv) <= VERSION_8192C) ) {
			pstat->tx_pkts++;
		} else
#endif
        if(pstat->cmn_info.ra_info.disable_ra)
#endif //TXREPORT
		{
#ifdef CONFIG_RTL8672
			if (txcfg->fr_type == _SKB_FRAME_TYPE_
#ifdef SUPPORT_TX_MCAST2UNI
				&& !txcfg->isMC2UC
#endif		
			)
#endif
			pstat->tx_pkts++;
		}
		pstat->tx_pkts2++;
		pstat->tx_bytes += pktlen;
		pstat->tx_byte_cnt += pktlen;
//#ifdef CONFIG_VERIWAVE_CHECK
		if(pktlen > 600)
			pstat->tx_big_pkts++;
		else
			pstat->tx_sml_pkts++;
//#endif
	}
}

/*
 * rx_sum_up might called after the skb was freed, don't use get_pframe to retrieve any information in skb.
 */
static __inline__ void rx_sum_up(struct rtl8192cd_priv *priv, struct stat_info *pstat, struct rx_frinfo *pfrinfo)
{
	struct net_device_stats *pnet_stats;

	if (priv) {
		pnet_stats = &(priv->net_stats);       
#ifdef CONFIG_RTL8672
		if ((pfrinfo->to_fr_ds == 0)
#if defined(MP_TEST)
			&& !(OPMODE & WIFI_MP_STATE) 
#endif	    
		) {
			priv->ext_stats.rx_mgnt_pkts++;
		} else
#endif
		{
			pnet_stats->rx_packets++;
			pnet_stats->rx_bytes += pfrinfo->pktlen;
		}
#ifdef RX_CRC_EXPTIMER
        priv->ext_stats.rx_packets_exptimer++;
		priv->ext_stats.rx_packets_by_rate[pfrinfo->rx_rate]++;
#endif        

#ifdef TRX_DATA_LOG
		if (pfrinfo->bdata && !priv->pmib->miscEntry.func_off)
			priv->ext_stats.rx_data_packets++;
#endif	

		if (pfrinfo->retry)
			priv->ext_stats.rx_retrys++;
#if defined(CONFIG_RTL8672) || defined(CONFIG_WLAN_STATS_EXTENTION)
		if (pfrinfo->bcast)
			priv->ext_stats.rx_bcast_pkts_cnt++;
		else if (pfrinfo->mcast)
			priv->ext_stats.rx_mcast_pkts_cnt++;
		else
			priv->ext_stats.rx_ucast_pkts_cnt++;
#endif
		priv->ext_stats.rx_byte_cnt += pfrinfo->pktlen;

		// bcm old 11n chipset iot debug
		priv->pshare->current_rx_bytes += pfrinfo->pktlen;
	}

	if (pstat) {
		if (pfrinfo->to_fr_ds == 0)
			pstat->rx_mgnt_pkts++;

		pstat->rx_pkts++;
		pstat->rx_bytes += pfrinfo->pktlen;
		pstat->rx_byte_cnt += pfrinfo->pktlen;
	}
}

#endif
