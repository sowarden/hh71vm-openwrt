#ifndef _RTL_MEMPOOL_H
#define _RTL_MEMPOOL_H
#include <linux/types.h>
#include <linux/skbuff.h>

typedef enum  rtw_mem_pool_type_cb {
	RTW_MEM_POOL_FREE_CB=0,
	RTW_MEM_POOL_ALLOC_CB,
	RTW_MEM_POOL_MAX_CB,
} RTW_MEM_POOL_TYPE_CB;

struct sk_buff *rtw_mem_pool_alloc_skb(int size);
int rtw_remove_mem_pool_ex(char *name);
void rtw_create_mem_pool_ex(char *name, int max, int min, int size);
int rtw_mem_pool_register_cb(char *poolname, int type, int (*func)(void *));
#endif	/* _RTL_BRSC_H */
