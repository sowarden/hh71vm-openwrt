/**
  *  @file 8192cd_d2h_rx.c of D2H Rx process from Data-CPU to Host-CPU  (Use 8814A to simulation)
  *  @brief Handle Rx-processes From Data-CPU
  *
  *  Packet Offload Engine will help RTK WiFi Chip to decrease host platform CPU utilization.
  *  This functon will handle Tx-processes in Host-CPU
  *
  *  Copyright (c) 2015 Realtek Semiconductor Corp.
  *
  *  @author Peter Yu
  *  @date 2015/11/9
 **/

#define _8192CD_RX_D2H_C_

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <net/ip.h>
#include <linux/ipv6.h>
#include <linux/icmpv6.h>
#endif

#include "./8192cd_cfg.h"
#include "./8192cd.h"
#include "./8192cd_hw.h"
#include "./8192cd_headers.h"
#include "./8192cd_rx.h"
#include "./8192cd_debug.h"

#ifdef __LINUX_2_6__
#ifdef CONFIG_RTL8672
#include "./romeperf.h"
#else
#if !defined(NOT_RTK_BSP) && !defined(CONFIG_RTL_8198F)
#ifdef CONFIG_OPENWRT_SDK
#include "./rtl_types.h" //mark_wrt
#else
#include <net/rtl/rtl_types.h>
#endif
#endif
#endif
#endif

/** @brief --Check the packet to Wi-Fi could offload to Data-CPU or not.
  * consider exception case :
  *      SW encryption/decryption
  *      TKIP MIC
  *      Frame for fragmentation/Defragmentation        : Fragment could do in data cpu
  *      Station Sleep: Enqueue frame per sleeping STA  : dequeue should call rtl8192cd_xmit_offload_check
  *      AMSDU is enable or not
  *      MAC-layer NAT translation (STA/Ad-hoc mode)


  * @param param_out None
  * @param param_in  skb : packets content
  *                  priv: to which interface.
  * @return 1: offload, 0: not offload, others: error
 **/
__IRAM_IN_865X
int rx_dataIsrtask(struct rtl8192cd_priv *priv)
{
	int ret=0;

    //1. Parsing RXDESC and RxFWInfo RxPktInfo,
    //   call QueryHRxDesc88XX
    //2. handle pkt
    //   2-1 c2h_event: call HAL88XXC2HEventHandler

    //   2-2 802.11 Frame : call original process
    //   2-3 802.3 Frame  : go reorder ctrl  ==> netif_rx                        
    //   2-4 any error             : should not happened,   drop ==> indicate rx to free memory
    //   2-5 update HRXBD 
    return 0;

	return ret;
}

