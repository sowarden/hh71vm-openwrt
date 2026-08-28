#ifndef _8192CD_TRIBNAD_H_
#define _8192CD_TRIBNAD_H_

#ifdef CONFIG_RTL8190_PRIV_SKB
#error "triband undefined!!"
#endif

#if defined(_8192CD_TRIBNAD_UTIL_LOCK_H_)

#define SAVE_INT_AND_CLI(__x__)		do { } while (0)
#define RESTORE_INT(__x__)			do { } while (0)

#define SMP_LOCK(__x__)	\
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			__u32 _cpu_id = get_cpu(); \
			if(priv->pshare->lock_owner != _cpu_id) \
				spin_lock_irqsave(&priv->pshare->lock, priv->pshare->lock_flags); \
			else {\
				panic_printk("[%s %d] recursion detection, caller=%p\n",__FUNCTION__,__LINE__,__builtin_return_address(0)); \
				panic_printk("Previous Lock Function is %s\n",priv->pshare->lock_func); \
			} \
			strcpy(priv->pshare->lock_func, __FUNCTION__);\
			priv->pshare->lock_owner = _cpu_id;\
			put_cpu(); \
		} \
	} while(0)

#define SMP_UNLOCK(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			priv->pshare->lock_owner=-1; \
			spin_unlock_irqrestore(&priv->pshare->lock, priv->pshare->lock_flags); \
		} \
	} while(0)

#define SMP_TRY_LOCK(__x__,__y__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			__u32 _cpu_id = get_cpu(); \
			if(priv->pshare->lock_owner != _cpu_id) { \
				SMP_LOCK(__x__); \
				__y__ = 1; \
			} else \
				__y__ = 0; \
			put_cpu();\
		} \
	} while(0)

#define SMP_LOCK_ASSERT() \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
		__u32 _cpu_id = get_cpu(); \
		if(priv->pshare->lock_owner != _cpu_id) { \
				panic_printk("ERROR: Without obtaining SMP_LOCK(). Please calling SMP_LOCK() before entering into %s()\n\n\n",__FUNCTION__); \
				put_cpu(); \
				return; \
		} \
		put_cpu(); \
		} \
	} while(0)

#define SMP_LOCK_XMIT(__x__)	\
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			__u32 _cpu_id = get_cpu(); \
			if(priv->pshare->lock_xmit_owner != _cpu_id) \
				spin_lock_irqsave(&priv->pshare->lock_xmit, priv->pshare->lock_xmit_flags); \
			else {\
				panic_printk("[%s %d] recursion detection, caller=%p\n",__FUNCTION__,__LINE__,__builtin_return_address(0)); \
				panic_printk("Previous Lock Function is %s\n",priv->pshare->lock_xmit_func); \
			}\
			strcpy(priv->pshare->lock_xmit_func, __FUNCTION__);\
			priv->pshare->lock_xmit_owner = _cpu_id;\
			put_cpu(); \
		} \
	} while(0)

#define SMP_UNLOCK_XMIT(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			priv->pshare->lock_xmit_owner=-1; \
			spin_unlock_irqrestore(&priv->pshare->lock_xmit, priv->pshare->lock_xmit_flags); \
		} \
	} while(0)

#define SMP_TRY_LOCK_XMIT(__x__,__y__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			__u32 _cpu_id = get_cpu(); \
			if(priv->pshare->lock_xmit_owner != _cpu_id) { \
				SMP_LOCK_XMIT(__x__); \
				__y__ = 1; \
			} else \
				__y__ = 0; \
			put_cpu(); \
		} \
	} while(0)

#define SMP_LOCK_GLOBAL(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			__u32 _cpu_id = get_cpu(); \
			if(global_lock_owner != _cpu_id) \
				spin_lock_irqsave(&global_lock, global_lock_flag); \
			else {\
				panic_printk("[%s %d] recursion detection, caller=%p\n",__FUNCTION__,__LINE__,__builtin_return_address(0)); \
				panic_printk("Previous Lock Function is %s\n",global_lock_func); \
			} \
			strcpy(global_lock_func, __FUNCTION__);\
			global_lock_owner = _cpu_id;\
			put_cpu(); \
		} \
	} while(0)

#define SMP_UNLOCK_GLOBAL(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			global_lock_owner=-1; \
			spin_unlock_irqrestore(&global_lock, global_lock_flag); \
		} \
	} while(0)

#define SMP_LOCK_HASH_LIST(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_lock_irqsave(&priv->hash_list_lock, (__x__)); \
		} \
		else { \
			spin_lock_bh(&priv->hash_list_lock); (void)(__x__); \
		} \
	} while(0)

#define SMP_UNLOCK_HASH_LIST(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_unlock_irqrestore(&priv->hash_list_lock, (__x__)); \
		} \
		else { \
			spin_unlock_bh(&priv->hash_list_lock); (void)(__x__); \
		} \
	} while(0)

#define SMP_LOCK_STACONTROL_LIST(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_lock_irqsave(&priv->stactrl.stactrl_lock, (__x__)); \
		} \
	} while(0)

#define SMP_UNLOCK_STACONTROL_LIST(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_unlock_irqrestore(&priv->stactrl.stactrl_lock, (__x__)); \
		} \
	} while(0)

#define SMP_LOCK_TRANSITION_LIST(__x__) \
    do { \
    	if (priv->hci_type == RTL_HCI_PCIE) { \
    		spin_lock_irqsave(&priv->transition_list_lock, (__x__)); \
    	} \
    } while(0)

#define SMP_UNLOCK_TRANSITION_LIST(__x__) \
    do { \
        if (priv->hci_type == RTL_HCI_PCIE) { \
            spin_unlock_irqrestore(&priv->transition_list_lock, (__x__)); \
        } \
    } while(0)

#define SMP_LOCK_WAITING_LIST(__x__) \
    do { \
        if (priv->hci_type == RTL_HCI_PCIE) { \
            spin_lock_irqsave(&priv->waiting_list_lock, (__x__)); \
        } \
    } while(0)

#define SMP_UNLOCK_WAITING_LIST(__x__) \
    do { \
        if (priv->hci_type == RTL_HCI_PCIE) { \
            spin_unlock_irqrestore(&priv->waiting_list_lock, (__x__)); \
        } \
    } while(0)

#define SMP_LOCK_BLOCK_LIST(__x__) \
    do { \
        if (priv->hci_type == RTL_HCI_PCIE) { \
            spin_lock_irqsave(&priv->block_list_lock, (__x__)); \
        } \
    } while(0)

#define SMP_UNLOCK_BLOCK_LIST(__x__) \
    do { \
        if (priv->hci_type == RTL_HCI_PCIE) { \
            spin_unlock_irqrestore(&priv->block_list_lock, (__x__)); \
        } \
    } while(0)

#define SMP_LOCK_SR_BLOCK_LIST(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_lock_irqsave(&priv->sr_block.sr_block_lock, (__x__)); \
		} \
	} while(0)

#define SMP_UNLOCK_SR_BLOCK_LIST(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_unlock_irqrestore(&priv->sr_block.sr_block_lock, (__x__)); \
		} \
	} while(0)

#define SMP_LOCK_ACL(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_USB) { \
			spin_lock(&priv->wlan_acl_list_lock); (void)(__x__); \
		} \
	} while(0)

#define SMP_UNLOCK_ACL(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_USB) { \
			spin_unlock(&priv->wlan_acl_list_lock); (void)(__x__); \
		} \
	} while(0)

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

#define SMP_LOCK_SLEEP_LIST(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_lock_irqsave(&priv->sleep_list_lock, (__x__)); \
		} \
		else { \
			spin_lock_bh(&priv->sleep_list_lock); (void)(__x__); \
		} \
	} while(0)

#define SMP_UNLOCK_SLEEP_LIST(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_unlock_irqrestore(&priv->sleep_list_lock, (__x__)); \
		} \
		else { \
			spin_unlock_bh(&priv->sleep_list_lock); (void)(__x__); \
		} \
	} while(0)

#define SMP_LOCK_AUTH_LIST(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_lock_irqsave(&priv->auth_list_lock, (__x__)); \
		} \
		else { \
			spin_lock_bh(&priv->auth_list_lock); (void)(__x__); \
		} \
	} while(0)

#define SMP_UNLOCK_AUTH_LIST(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_unlock_irqrestore(&priv->auth_list_lock, (__x__)); \
		} \
		else { \
			spin_unlock_bh(&priv->auth_list_lock); (void)(__x__); \
		} \
	} while(0)

#define SMP_LOCK_WAKEUP_LIST(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_lock_irqsave(&priv->wakeup_list_lock, (__x__)); \
		} \
		else { \
			spin_lock_bh(&priv->wakeup_list_lock); (void)(__x__); \
		} \
	} while(0)

#define SMP_UNLOCK_WAKEUP_LIST(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_unlock_irqrestore(&priv->wakeup_list_lock, (__x__)); \
		} \
		else { \
			spin_unlock_bh(&priv->wakeup_list_lock); (void)(__x__); \
		} \
	} while(0)

#define SMP_LOCK_MESH_MP_HDR(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_USB) { \
			spin_lock_bh(&priv->mesh_mp_hdr_lock); (void)(__x__); \
		} \
		else { \
			spin_lock_bh(&priv->mesh_mp_hdr_lock); (void)(__x__); \
		} \
	} while(0)

#define SMP_UNLOCK_MESH_MP_HDR(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_USB) { \
			spin_unlock_bh(&priv->mesh_mp_hdr_lock); (void)(__x__); \
		} \
		else { \
			spin_unlock_bh(&priv->mesh_mp_hdr_lock); (void)(__x__); \
		} \
	} while(0)

#define SMP_LOCK_MESH_ACL(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_USB) { \
			spin_lock(&priv->mesh_acl_list_lock); (void)(__x__); \
		} \
		else { \
			spin_lock(&priv->mesh_acl_list_lock); (void)(__x__); \
		} \
	} while(0)

#define SMP_UNLOCK_MESH_ACL(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_USB) { \
			spin_unlock(&priv->mesh_acl_list_lock); (void)(__x__); \
		} \
		else { \
			spin_unlock(&priv->mesh_acl_list_lock); (void)(__x__); \
		} \
	} while(0)

#define SMP_LOCK_SKB(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_lock_irqsave(&priv->pshare->lock_skb, (__x__)); \
		} \
		else { \
			spin_lock_irqsave(&priv->pshare->lock_skb, (__x__)); \
		} \
	} while(0)

#define SMP_UNLOCK_SKB(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_unlock_irqrestore(&priv->pshare->lock_skb, (__x__)); \
		} \
		else { \
			spin_unlock_irqrestore(&priv->pshare->lock_skb, (__x__)); \
		} \
	} while(0)

#define SMP_LOCK_BUF(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_lock_irqsave(&priv->pshare->lock_buf, (__x__)); \
		} \
		else { \
			spin_lock_irqsave(&priv->pshare->lock_buf, (__x__)); \
		} \
	} while(0)

#define SMP_UNLOCK_BUF(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_unlock_irqrestore(&priv->pshare->lock_buf, (__x__)); \
		} \
		else { \
			spin_unlock_irqrestore(&priv->pshare->lock_buf, (__x__)); \
		} \
	} while(0)

#define SMP_LOCK_RECV(__x__) \
	do { \
		__u32 _cpu_id = get_cpu(); \
		if (priv->pshare->lock_recv_owner != _cpu_id) \
			spin_lock_irqsave(&priv->pshare->lock_recv, (__x__)); \
		else \
			panic_printk("[%s %d] recursion detection\n",__FUNCTION__,__LINE__); \
		priv->pshare->lock_recv_owner = _cpu_id;\
		put_cpu(); \
	} while(0)

#define SMP_UNLOCK_RECV(__x__) \
	do { \
		priv->pshare->lock_recv_owner=-1; \
		spin_unlock_irqrestore(&priv->pshare->lock_recv, (__x__)); \
	} while(0)

#define SMP_TRY_LOCK_RECV(__x__,__y__) \
	do { \
		__u32 _cpu_id = get_cpu(); \
		if (priv->pshare->lock_recv_owner != _cpu_id) { \
			SMP_LOCK_RECV(__x__); \
			__y__ = 1; \
		} else \
			__y__ = 0; \
		put_cpu(); \
	} while(0)

#define SMP_LOCK_RX_DATA(__x__) \
	do { \
		spin_lock_irqsave(&priv->rx_datalist_lock, (__x__)); \
	} while(0)

#define SMP_UNLOCK_RX_DATA(__x__) \
	do { \
		spin_unlock_irqrestore(&priv->rx_datalist_lock, (__x__)); \
	} while(0)

#define SMP_LOCK_RX_MGT(__x__) \
	do { \		
		spin_lock_irqsave(&priv->rx_mgtlist_lock, (__x__)); \
	} while(0)

#define SMP_UNLOCK_RX_MGT(__x__) \
	do { \
		spin_unlock_irqrestore(&priv->rx_mgtlist_lock, (__x__)); \
	} while(0)

#define SMP_LOCK_RX_CTRL(__x__) \
	do { \
		spin_lock_irqsave(&priv->rx_ctrllist_lock, (__x__)); \
	} while(0)

#define SMP_UNLOCK_RX_CTRL(__x__) \
	do { \
		spin_unlock_irqrestore(&priv->rx_ctrllist_lock, (__x__)); \
	} while(0)

#ifdef CONFIG_IEEE80211R
#define SMP_LOCK_FT_R0KH(__x__) \
	do { \
		__u32 _cpu_id = get_cpu(); \
		if (priv->ft_r0kh_lock_owner != _cpu_id) \
			spin_lock_irqsave(&priv->ft_r0kh_lock, __x__); \
		else { \
			panic_printk("[%s %d] recursion detection, caller=%p\n",__FUNCTION__,__LINE__,__builtin_return_address(0)); \
			panic_printk("Previous Lock Function is %s\n",priv->ft_r0kh_lock_func); \
		} \
		strcpy(priv->ft_r0kh_lock_func, __FUNCTION__); \
		priv->ft_r0kh_lock_owner = _cpu_id; \
		put_cpu(); \
	} while(0)
	
#define SMP_UNLOCK_FT_R0KH(__x__)		do {priv->ft_r0kh_lock_owner = -1; spin_unlock_irqrestore(&priv->ft_r0kh_lock, __x__);} while(0)
#define SMP_TRY_LOCK_FT_R0KH(__x__,__y__) \
	do { \
		__u32 _cpu_id = get_cpu(); \
		if (priv->ft_r0kh_lock_owner != _cpu_id) { \
			SMP_LOCK_FT_R0KH(__x__); \
			__y__ = 1; \
		} else \
			__y__ = 0; \
		put_cpu(); \
	} while(0)
	
#define SMP_LOCK_FT_R1KH(__x__) \
	do { \
		__u32 _cpu_id = get_cpu(); \
		if (priv->ft_r1kh_lock_owner != _cpu_id) \
			spin_lock_irqsave(&priv->ft_r1kh_lock, __x__); \
		else { \
			panic_printk("[%s %d] recursion detection, caller=%p\n",__FUNCTION__,__LINE__,__builtin_return_address(0)); \
			panic_printk("Previous Lock Function is %s\n",priv->ft_r1kh_lock_func); \
		} \
		strcpy(priv->ft_r1kh_lock_func, __FUNCTION__); \
		priv->ft_r1kh_lock_owner = _cpu_id; \
		put_cpu(); \
	} while(0)

#define SMP_UNLOCK_FT_R1KH(__x__)		do {priv->ft_r1kh_lock_owner = -1; spin_unlock_irqrestore(&priv->ft_r1kh_lock, __x__);} while(0)
#define SMP_TRY_LOCK_FT_R1KH(__x__,__y__) \
	do { \
		__u32 _cpu_id = get_cpu(); \
		if (priv->ft_r1kh_lock_owner != _cpu_id) { \
			SMP_LOCK_FT_R1KH(__x__); \
			__y__ = 1; \
		} else \
			__y__ = 0; \
		put_cpu(); \
	} while(0)
	
#endif

#ifdef SBWC
#define SMP_LOCK_SBWC(__x__)	\
	do { \
		__u32 _cpu_id = get_cpu(); \
		if(priv->pshare->lock_sbwc_owner != _cpu_id) \
			spin_lock_irqsave(&priv->pshare->lock_sbwc, (__x__)); \
		else \
			panic_printk("[%s %d] recursion detection\n",__FUNCTION__,__LINE__); \
		priv->pshare->lock_sbwc_owner = _cpu_id;\
		put_cpu(); \
	} while(0)

#define SMP_UNLOCK_SBWC(__x__) \
	do { \
		priv->pshare->lock_sbwc_owner=-1; \
		spin_unlock_irqrestore(&priv->pshare->lock_sbwc, (__x__)); \
	} while(0)

#define SMP_TRY_LOCK_SBWC(__x__,__y__) \
	do { \
		__u32 _cpu_id = get_cpu(); \
		if (priv->pshare->lock_sbwc_owner != _cpu_id) { \
			SMP_LOCK_SBWC(__x__); \
			__y__ = 1; \
		} else \
			__y__ = 0; \
		put_cpu(); \
	} while(0)
#endif /* SBWC */

#ifdef RTK_NL80211
#define SMP_LOCK_CFG80211(__x__) \
	do { \
		spin_lock_irqsave(&priv->cfg80211_lock, (__x__)); \
	} while(0)

#define SMP_UNLOCK_CFG80211(__x__) \
	do { \
		spin_unlock_irqrestore(&priv->cfg80211_lock, (__x__)); \
	} while(0)
#endif //RTK_NL80211

#if defined(CONFIG_RTK_SOC_RTL8198D) && defined(__LINUX_4_4__)
#define SMP_LOCK_IQK(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_lock_irqsave(&priv->pshare->lock_iqk, (__x__)); \
		} \
	} while(0)

#define SMP_UNLOCK_IQK(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_unlock_irqrestore(&priv->pshare->lock_iqk, (__x__)); \
		} \
	} while(0)
#else
#define SMP_LOCK_IQK(__x__) \
	do { \
		spin_lock_irqsave(&priv->pshare->lock_iqk, (__x__)); \
	} while(0)

#define SMP_UNLOCK_IQK(__x__) \
	do { \
		spin_unlock_irqrestore(&priv->pshare->lock_iqk, (__x__)); \
	} while(0)
#endif

#define SMP_LOCK_REORDER_CTRL(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			__u32 _cpu_id = get_cpu(); \
			if (priv->pshare->rc_packet_q_lock_owner != _cpu_id) \
				spin_lock_irqsave(&priv->pshare->rc_packet_q_lock, (__x__)); \
			else \
				panic_printk("[%s %d] recursion detection\n",__FUNCTION__,__LINE__); \
			priv->pshare->rc_packet_q_lock_owner = _cpu_id;\
			put_cpu(); \
		} \
		else { \
			spin_lock_bh(&priv->pshare->rc_packet_q_lock); (void)(__x__); \
		} \
	} while(0)

#define SMP_UNLOCK_REORDER_CTRL(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			priv->pshare->rc_packet_q_lock_owner=-1; \
			spin_unlock_irqrestore(&priv->pshare->rc_packet_q_lock, (__x__)); \
		} \
		else { \
			spin_unlock_bh(&priv->pshare->rc_packet_q_lock); (void)(__x__); \
		} \
	} while(0)

#define SMP_TRY_LOCK_REORDER_CTRL(__x__,__y__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			__u32 _cpu_id = get_cpu(); \
			if (priv->pshare->rc_packet_q_lock_owner != _cpu_id) { \
				SMP_LOCK_REORDER_CTRL(__x__); \
				__y__ = 1; \
			} else \
				__y__ = 0; \
			put_cpu(); \
		} \
	} while(0)

#define DEFRAG_LOCK(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_lock_irqsave(&priv->defrag_lock, (__x__)); \
		} \
		else { \
			spin_lock_bh(&priv->defrag_lock); (void)(__x__); \
		} \
	} while (0)

#define DEFRAG_UNLOCK(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_unlock_irqrestore(&priv->defrag_lock, (__x__)); \
		} \
		else { \
			spin_unlock_bh(&priv->defrag_lock); (void)(__x__); \
		} \
	} while (0)

#define SMP_LOCK_PSK_RESEND(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_lock_irqsave(&priv->psk_resend_lock, (__x__)); \
		} \
	} while (0)

#define SMP_UNLOCK_PSK_RESEND(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_unlock_irqrestore(&priv->psk_resend_lock, (__x__)); \
		} \
	} while (0)

#define SMP_LOCK_PSK_GKREKEY(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_lock_irqsave(&priv->psk_gkrekey_lock, (__x__)); \
		} \
	} while (0)

#define SMP_UNLOCK_PSK_GKREKEY(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_PCIE) { \
			spin_unlock_irqrestore(&priv->psk_gkrekey_lock, (__x__)); \
		} \
	} while (0)

#define SMP_LOCK_MBSSID(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_USB) { \
			_enter_critical_mutex(&priv->pshare->mbssid_lock, (__x__)); \
		} \
	} while (0)

#define SMP_UNLOCK_MBSSID(__x__) \
	do { \
		if (priv->hci_type == RTL_HCI_USB) { \
			_exit_critical_mutex(&priv->pshare->mbssid_lock, (__x__)); \
		} \
	} while (0)
#endif //_8192CD_TRIBNAD_UTIL_LOCK_H_


//////////////////////////////////////////////////////////////////////////////////
#if 0//defined(_8192CD_TRIBNAD_UTIL_RW_H_)
static inline unsigned char __HAL_RTL_R8(struct rtl8192cd_priv *priv, u32 addr)
{
	unsigned char ret = 0;
	switch (priv->hci_type) {
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
	switch (priv->hci_type) {
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
	switch (priv->hci_type) {
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
	switch (priv->hci_type) {
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
	switch (priv->hci_type) {
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
	switch (priv->hci_type) {
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

#endif //_8192CD_TRIBNAD_UTIL_RW_H_

//////////////////////////////////////////////////////////////////////////////////

#endif //_8192CD_TRIBNAD_H_
