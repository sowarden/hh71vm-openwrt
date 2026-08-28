/*
 * Copyright c                Realtek Semiconductor Corporation, 2003
 * All rights reserved.
 *
 */
#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/netdevice.h>
#include <linux/spinlock.h>
#include <linux/skbuff.h>
#include <linux/mempool.h>
#include <net/rtl/rtl_mempool.h>

#define TRUE 					(0)
#define FALSE	 				(-1)

#define RTW_MEM_MAGIC 			(0x23798190)
#define RTW_MEM_POOL_MAX 		(8)
#define RTW_MEM_POOL_NAME_SZ 	(32)
#define RTW_MEM_POOL_BATCH		(16)
#define RTW_MEM_IDLE_TIMEOUT	(HZ*3) /*3s in jiffies.*/
#define RTW_THRU_SHIFT			(3)

//#define RTW_MEM_POOL_DEBUG

#ifdef RTW_MEM_POOL_DEBUG
#define RTW_JUST_IN_FREELIST (0x01010101)
#define RTW_IN_USED		(0x02020202)
#define RTW_FREED		(0x03030303)
#define RTW_FREED_TO_FREELIST	(0x04040404)
#define RTW_FREED_TO_LINUX_POOL	(0x05050505)
#endif

typedef enum  rtw_mem_pool_match_e {
	RTW_MEM_POOL_UNMATCHED=0,
	RTW_MEM_POOL_SIZE_MATCHED,
	RTW_MEM_POOL_SIZE_NAME_MATCHED,
	RTW_MEM_POOL_SIZE_UNMATCH_NAME_MATCHED,
} RTW_MEM_POOL_MATCH_TYPE;

typedef struct rtw_mem_pool {
	char name[RTW_MEM_POOL_NAME_SZ];
	int max;		/*up to max can be used*/
	int min;		/*at least min in free_list when created*/
	int used;		/*used by system*/
	int rpu;		/*rtw rx packet number underflow*/
	int rmu;		/*rtw rx memory underflow */
	int buf_returned;
	int buf_alloced;
	int mem_used;
	int buf_alloc_failed;
	int batch;		/*alloc batch elements from linux mempool once */
	int thru;		/*if free count > thru for a period time, try to resize*/
	int obj_size;
	spinlock_t	lock;
	struct list_head free_list;
	int free;		/*free buf count in free_list*/
	unsigned long timestamp;
	int freed; 	    /*counting of buffer freed during idle timeout*/
	int resized;    /*resized count*/
	struct kmem_cache *slab;
	struct work_struct fill_work;
	struct shrinker shrinker;
	bool shrinker_enabled;
	int shrinked;
	int (*free_cb)(void *);
} rtw_mem_pool_t, *rtw_mem_pool_tp;


typedef struct rtw_priv_buffer {
	unsigned int reserved[3]; /*keep skb->head at cache line aligned just as kernel native done*/
	unsigned int flag;
	rtw_mem_pool_t *pool;
	unsigned int magic;
	struct list_head list;
	unsigned char data[0];
} rtw_priv_buffer_t, *rtw_priv_buffer_tp;

typedef struct rtw_pool_info {
	int size;
	rtw_mem_pool_t *pool;
} rtw_pool_info_t;

rtw_pool_info_t rtw_pool_info_array[RTW_MEM_POOL_MAX];
static struct timer_list rtw_pool_timer;
static unsigned long total_mem_used;
static unsigned long total_mem_thru;
extern struct proc_dir_entry proc_root;

void rtw_mem_pool_return_buffer(rtw_mem_pool_t *pool, int count);

static inline int rtw_add_mem_pool(int size,rtw_mem_pool_t *pool)
{
	int i=0;
	for(i=0;i<RTW_MEM_POOL_MAX;i++) {
		if(rtw_pool_info_array[i].size == 0)
		{
			rtw_pool_info_array[i].size = size;
			rtw_pool_info_array[i].pool = pool;
			return TRUE;
		}
	}
	return FALSE;
}

static inline int rtw_del_mem_pool(rtw_mem_pool_t *pool)
{
	int i=0;
	for(i=0;i<RTW_MEM_POOL_MAX;i++) {
		if(rtw_pool_info_array[i].pool == pool) {
			rtw_pool_info_array[i].size = 0;
			rtw_pool_info_array[i].pool = NULL;
			return TRUE;
		}	
	}
	printk("!!Error,pool %s not exist!\n",pool->name);
	return FALSE;
}

static inline rtw_mem_pool_t *rtw_find_mem_pool(int size)
{
	int i=0;
	for(i=0;i<RTW_MEM_POOL_MAX;i++)
		if(rtw_pool_info_array[i].size == size)
			return rtw_pool_info_array[i].pool;
	return NULL;
}

static inline rtw_mem_pool_t *rtw_find_mem_pool_by_name(char *name)
{
	int i=0;
	for(i=0;i<RTW_MEM_POOL_MAX;i++) {
		if(rtw_pool_info_array[i].size) {
			if(0==strcmp(name,rtw_pool_info_array[i].pool->name))
				return rtw_pool_info_array[i].pool;
		}	
	}		
	return NULL;
}

static inline rtw_mem_pool_t *rtw_find_mem_pool_by_name_sz(char *name,int size, RTW_MEM_POOL_MATCH_TYPE *type)
{
	int i = 0;
	*type = RTW_MEM_POOL_UNMATCHED;

	for(i=0;i<RTW_MEM_POOL_MAX;i++) {
		if(rtw_pool_info_array[i].size) {
			if(0==strcmp(name,rtw_pool_info_array[i].pool->name)) {
				if(rtw_pool_info_array[i].size != size) {		
					*type = RTW_MEM_POOL_SIZE_UNMATCH_NAME_MATCHED;
					printk("Error!!! found pool with same name(%s) but size (%d %d)mismatch\n",name,size,rtw_pool_info_array[i].size);
					return NULL;
				}
			}
		}
	}
	
	for(i=0;i<RTW_MEM_POOL_MAX;i++) {
		if(rtw_pool_info_array[i].size) {
			if(rtw_pool_info_array[i].size == size) {
				*type=RTW_MEM_POOL_SIZE_MATCHED;
				if(0==strcmp(name,rtw_pool_info_array[i].pool->name)) {					
					*type=RTW_MEM_POOL_SIZE_NAME_MATCHED;
				}	
				return rtw_pool_info_array[i].pool;
			}

		}	
	}		
	return NULL;
}


void rtw_mempool_set_key_value(rtw_mem_pool_t *pool, char *key, int value)
{
	if(!strcmp(key,"max")) {
		pool->max = value;
	} else if(!strcmp(key,"min")) {
		pool->min = value;
	} else if(!strcmp(key,"thru")) {
		pool->thru = value;
	} else if(!strcmp(key,"batch")) {
		pool->batch = value;
	} else {
		printk("unkown key %s\n",key);
	}
	return;
}


/*echo "poolname,max,xxx > /proc/rtw_mempool*/
/*echo "poolname,max,xxx,batch,xxx > /proc/rtw_mempool*/

static unsigned int rtw_mempool_entry_write(struct file *file, const char *buffer,
		      unsigned long len, void *data)
{
	char *strptr;
	char *tokptr;
	int value;
	char *valueptr;
	char tmpbuf[128];
	rtw_mem_pool_t *pool;
	char name[RTW_MEM_POOL_NAME_SZ];
	
	if(len > 128)
		len = 128;

	memset(name,0,sizeof(name));
	if (buffer && !copy_from_user(tmpbuf, buffer, len)) {
		strptr = tmpbuf;

		tokptr = strsep(&strptr,",");
		if (tokptr == NULL)
			goto errout;
		
		strncpy(name, tokptr, RTW_MEM_POOL_NAME_SZ);
		if(strcmp(name,"total_mem_thru") == 0) {
			valueptr=strsep(&strptr,",");
			value=simple_strtol(valueptr, NULL, 0);
			total_mem_thru = value;
		}
		else {
			pool = rtw_find_mem_pool_by_name(name);
			if(pool == NULL) {
				printk("can not find pool by name %s\n",name);
				goto errout;
			}
			/*handle key,value pair*/
			while((tokptr = strsep(&strptr,",")) != NULL)
			{
					valueptr=strsep(&strptr,",");
					if(valueptr == NULL)
						goto errout;
					value=simple_strtol(valueptr, NULL, 0);
					rtw_mempool_set_key_value(pool,tokptr,value);
			}
		}
	}
	return len;
errout:
	printk("error parameter\n");
	return len;
}

#define RTW_MEMPOOL_PRINT(s,fmt,args...) if(s) seq_printf(s,fmt,##args); else printk(fmt,##args);
void rtw_mempool_print_entry(void *s)
{
	int i;
	RTW_MEMPOOL_PRINT(s,"mempool: mem_used %lu(0x%lx)bytes mem_thru %lu(0x%lx)\n", total_mem_used,total_mem_used,
		total_mem_thru,total_mem_thru);
	for(i=0;i<RTW_MEM_POOL_MAX;i++) {
		if(rtw_pool_info_array[i].size != 0)
		{
			RTW_MEMPOOL_PRINT(s,"name %s pool %p size %d  obj_size %d\n",rtw_pool_info_array[i].pool->name, 
				rtw_pool_info_array[i].pool, rtw_pool_info_array[i].size, rtw_pool_info_array[i].pool->obj_size);
			RTW_MEMPOOL_PRINT(s,"\tmax %d used %d min %d free %d thru %d\n",rtw_pool_info_array[i].pool->max, 
				rtw_pool_info_array[i].pool->used, rtw_pool_info_array[i].pool->min,
				rtw_pool_info_array[i].pool->free, rtw_pool_info_array[i].pool->thru);
			RTW_MEMPOOL_PRINT(s,"\trpu %d resized %d shrinked %d timestamp %lu freed %d\n",rtw_pool_info_array[i].pool->rpu, 
				rtw_pool_info_array[i].pool->resized, rtw_pool_info_array[i].pool->shrinked,
				rtw_pool_info_array[i].pool->timestamp, rtw_pool_info_array[i].pool->freed);
			RTW_MEMPOOL_PRINT(s,"\trmu %d mem_used %d(0x%x)bytes\n",rtw_pool_info_array[i].pool->rmu,
				rtw_pool_info_array[i].pool->mem_used,rtw_pool_info_array[i].pool->mem_used);
			RTW_MEMPOOL_PRINT(s,"\talloced %d returned %d failed %d\n",rtw_pool_info_array[i].pool->buf_alloced, 
				rtw_pool_info_array[i].pool->buf_returned, rtw_pool_info_array[i].pool->buf_alloc_failed);
		}
	}
}
static int rtw_mempool_entry_read(struct seq_file *s, void *v)
{
	rtw_mempool_print_entry(s);
	return 0;
}

void rtw_mempool_entry_dump(void)
{
	rtw_mempool_print_entry(NULL);
}

int rtw_mempool_single_open(struct inode *inode, struct file *file)
{
        return (single_open(file, rtw_mempool_entry_read, NULL));
}
static ssize_t rtw_mempool_single_write(struct file * file, const char __user * userbuf,
		     size_t count, loff_t * off)
{
	    return rtw_mempool_entry_write(file, userbuf,count, off);
}
			 
struct file_operations rtw_mempool_proc_fops= {
        .open           = rtw_mempool_single_open,
        .write		    = rtw_mempool_single_write,
        .read           = seq_read,
        .llseek         = seq_lseek,
        .release        = single_release,
};

static inline rtw_priv_buffer_t *rtw_mem_pool_get_from_list(rtw_mem_pool_t *pool)
{	
	rtw_priv_buffer_t *buf;
	struct list_head *plist;
	spin_lock_bh(&pool->lock);
	if(!list_empty(&pool->free_list)) {
		plist= pool->free_list.next;
		list_del_init(plist);
		pool->free--;
		buf = list_entry(plist, rtw_priv_buffer_t, list);
		spin_unlock_bh(&pool->lock);
		return buf;
	}
	spin_unlock_bh(&pool->lock);
	return NULL;
}

static inline int rtw_mem_pool_need_shrink(rtw_mem_pool_t *pool)
{
	if(0 == pool->timestamp) {
		return 0;
	}	
	/*there are enough free buffers over one second*/
	if(time_after(jiffies, pool->timestamp+HZ)) {
		return 1;
	}
	return 0;
}

static unsigned long rtw_mempool_shrinker_scan(struct shrinker *shrinker,
		struct shrink_control *sc)
{
	unsigned long freed;
	rtw_mem_pool_t *pool = container_of(shrinker, struct rtw_mem_pool, shrinker);
	if(rtw_mem_pool_need_shrink(pool)) {
		freed = (pool->used + pool->free) - (pool->min + pool->batch);
		pool->shrinked++;
		rtw_mem_pool_return_buffer(pool,freed);
	}	
	return SHRINK_STOP;
}

static unsigned long rtw_mempool_shrinker_count(struct shrinker *shrinker,
		struct shrink_control *sc)
{
	unsigned long to_free = 0;
	rtw_mem_pool_t *pool = container_of(shrinker, struct rtw_mem_pool, shrinker);
	if(pool->free > pool->thru)
		to_free= pool->free - pool->thru;
	return to_free;
}

static void rtw_mempool_unregister_shrinker(rtw_mem_pool_t *pool)
{
	if (pool->shrinker_enabled) {
		unregister_shrinker(&pool->shrinker);
		pool->shrinker_enabled = false;
	}
}

static int rtw_mempool_register_shrinker(rtw_mem_pool_t *pool)
{
	pool->shrinker.scan_objects = rtw_mempool_shrinker_scan;
	pool->shrinker.count_objects = rtw_mempool_shrinker_count;
	pool->shrinker.batch = 0;
	pool->shrinker.seeks = DEFAULT_SEEKS;

	return register_shrinker(&pool->shrinker);
}

static inline void rtw_mem_pool_add_to_list(rtw_mem_pool_t *pool, rtw_priv_buffer_t *buf)
{	
	spin_lock_bh(&pool->lock);
	list_add_tail(&buf->list,&pool->free_list);
	pool->free++;
	spin_unlock_bh(&pool->lock);
}

#ifdef RTW_MEM_POOL_DEBUG
static inline rtw_mem_pool_dump_buf(rtw_priv_buffer_t *buf)
{
	printk("buf %p flag 0x%x magic 0x%x\n",buf,buf->flag,buf->magic);
	printk("next %p prev %p\n",buf->list.next,buf->list.prev);
}

extern char _end[];

int rtw_mem_find_in_all_buffer(void)
{
	unsigned long start, end;
	unsigned long *ptr;
	rtw_priv_buffer_t *buf;
	start = (unsigned long)&_end;
	end = 0x88000000;
	printk("start 0x%x end 0x%x\n",start,end);
	for(start;start<end;start+=4)
	{
		ptr=(unsigned long *)start;
		if(*ptr == RTW_MEM_MAGIC) {
			buf = container_of(ptr, rtw_priv_buffer_t, magic);
			printk("buf %p magic 0x%x buf->pool %p\n",buf,buf->magic,buf->pool);
		}	
	}
}


static inline void rtw_mem_pool_find_list(rtw_mem_pool_t *pool, rtw_priv_buffer_t *buf)
{	
	int found=0;
	struct list_head *plist;
	spin_lock_irq(&pool->lock);
	
	rtw_priv_buffer_t *pbuf;
	list_for_each(plist, &pool->free_list) {
		pbuf = list_entry(plist, rtw_priv_buffer_t, list);
		if(pbuf == buf)
		{
			found = 1;
			rtw_mem_pool_dump_buf(buf);
			break;			
		}	
	}
	spin_unlock_irq(&pool->lock);
	if(found == 0)
		printk("not found buf %p in freelist\n",buf);
}

#endif

void rtw_mem_pool_fill_list_internal(rtw_mem_pool_t *pool, int cnt, int flag)
{
	int i=0;
	gfp_t flags;
	rtw_priv_buffer_t *buf;
	
	if(0 == flag)
		flags = GFP_KERNEL;
	else
		flags = GFP_ATOMIC;
	
	for(i=0;i<cnt;i++)
	{
		
		spin_lock_bh(&pool->lock);
		if(total_mem_thru && (total_mem_used > total_mem_thru)) {
			pool->rmu++;
			spin_unlock_bh(&pool->lock);
			return;
		}	
		spin_unlock_bh(&pool->lock);
		buf = kmem_cache_alloc(pool->slab, flags);
		if(buf == NULL) {
			pool->buf_alloc_failed++;
			if(printk_ratelimit())
				printk("%s:%d mempool_alloc failed i %d cnt %d\n",__FUNCTION__,__LINE__,i,cnt);
			return;
		}
		spin_lock_bh(&pool->lock);
		pool->buf_alloced++;
		pool->mem_used += pool->obj_size;
		total_mem_used += pool->obj_size;
		spin_unlock_bh(&pool->lock);
		buf->pool = pool;
		buf->magic = RTW_MEM_MAGIC;
		INIT_LIST_HEAD(&buf->list);
#ifdef RTW_MEM_POOL_DEBUG
		buf->flag = RTW_JUST_IN_FREELIST;
#endif
		rtw_mem_pool_add_to_list(pool,buf);
	}
}

void rtw_mem_pool_fill_list_background(struct work_struct *work)
{
	rtw_mem_pool_t *pool = container_of(work, rtw_mem_pool_t, fill_work);
	if(pool->used > pool->max)
		return;
	rtw_mem_pool_fill_list_internal(pool,pool->batch,0);
}


inline void rtw_mem_pool_fill_list_delay(rtw_mem_pool_t *pool)
{
	schedule_work(&pool->fill_work);
}

void rtw_mem_pool_fill_list(rtw_mem_pool_t *pool, int cnt)
{
	if(pool->batch == cnt) {
		rtw_mem_pool_fill_list_internal(pool,2,1);
		rtw_mem_pool_fill_list_delay(pool);
	}	
	else
		rtw_mem_pool_fill_list_internal(pool,cnt,1);
}


void *rtw_mem_pool_alloc_buf(rtw_mem_pool_t *pool)
{
	rtw_priv_buffer_t *buf;

	if(pool->used > pool->max) {
		pool->rpu++;
		return NULL;
	}

	buf = rtw_mem_pool_get_from_list(pool);
	if(buf == NULL) {
		rtw_mem_pool_fill_list(pool,pool->batch);
		buf = rtw_mem_pool_get_from_list(pool);
		/*still failed ?*/
		if(buf == NULL) {
			if(printk_ratelimit())
				printk("%s:%d %s alloc from linux pool failed\n",__FUNCTION__,__LINE__,pool->name);
			return NULL;
		}	
	}	
	spin_lock_bh(&pool->lock);
	pool->used++;
	spin_unlock_bh(&pool->lock);
#ifdef RTW_MEM_POOL_DEBUG
	buf->flag = RTW_IN_USED;
#endif
	return (void *)buf->data;
}

bool is_rtw_mem(void *buf)
{
	rtw_priv_buffer_t *pbuf;
	pbuf = (rtw_priv_buffer_t *)(buf-sizeof(rtw_priv_buffer_t));
	return pbuf->magic == RTW_MEM_MAGIC;
}

void rtw_mem_pool_return_buffer(rtw_mem_pool_t *pool, int count)
{
	rtw_priv_buffer_t *pbuf;
	while(count--)
	{
		pbuf=rtw_mem_pool_get_from_list(pool);
		if(NULL==pbuf)
			break;
		pbuf->magic = 0x0;
		pbuf->pool = 0x0;
#ifdef RTW_MEM_POOL_DEBUG
		pbuf->flag = RTW_FREED_TO_LINUX_POOL;
#endif

		spin_lock_bh(&pool->lock);
		pool->buf_returned++;
		pool->mem_used -= pool->obj_size;
		total_mem_used -= pool->obj_size;
		spin_unlock_bh(&pool->lock);
		if(ksize(pbuf) != pool->obj_size)
			printk("Error: %s:%d return buf(%d) to un-match pool (%s-%d)\n",
				__FUNCTION__,__LINE__,ksize(pbuf),pool->name,pool->obj_size);
		kmem_cache_free(pool->slab, pbuf);
	}
	return;
}

static inline int rtw_mem_pool_need_resize(rtw_mem_pool_t *pool, int *level)
{
	*level = 0;
	if(0 == pool->timestamp) {
		return 0;
	}	
	
	/*there are enough free buffers over a period of time*/
	if(time_after(jiffies, pool->timestamp+RTW_MEM_IDLE_TIMEOUT)) {
		if((pool->freed/(RTW_MEM_IDLE_TIMEOUT/HZ)) > pool->thru)
				*level=1;
		return 1;
	}
	return 0;
}

/*level 0: keep "min+batch" buf*/
/*level 1: there are over "thru" packets rx/tx in one second, keep "thru+min+batch" buf*/
void rtw_mem_pool_resize(rtw_mem_pool_t *pool, int level)
{
	int shrink;
	 
	/*default keep min + batch free buf*/
	shrink = (pool->used + pool->free) - (pool->min + pool->batch);
	if(level==1)
		shrink -= pool->thru;
	
	if(shrink <= 0) {
		//printk("INFO:no need to shrink %s\n",pool->name);
		return;
	}	
	rtw_mem_pool_return_buffer(pool,shrink);
	pool->resized++;
	return;
}

#ifdef RTW_MEM_POOL_DEBUG
static memdump(unsigned char *buf, int len)
{
	int i;
	for(i=0;i<len;i++) {
		printk("%02x ",buf[i]);
		if(0 == (i+1)%16)
			printk("\n");
	}	
}
#endif

int rtw_mem_pool_free_buf(void *buf)
{
	rtw_priv_buffer_t *pbuf;
	rtw_mem_pool_t *pool;
	if(NULL == buf) {
		printk("%s:%d buf is null\n",__FUNCTION__,__LINE__);
		return FALSE;
	}
	pbuf = (rtw_priv_buffer_t *)(buf-sizeof(rtw_priv_buffer_t));
	pool = pbuf->pool;
	if(pool && ksize(buf) != pool->obj_size) {
			printk("!!!!!!!!!!!!!!!!!@@@ %s:%d\n",__FUNCTION__,__LINE__);
			printk("pool %p name %s ksize(buf) %d,pool->obj_size %d ksize(pbuf) %d\n",pool,pool->name,ksize(buf),pool->obj_size,ksize(pbuf));
			kfree(buf);
			return TRUE;
	}		
	if(pool) {
		spin_lock_bh(&pool->lock);
		pool->used--;
		spin_unlock_bh(&pool->lock);
		
#ifdef RTW_MEM_POOL_DEBUG
		pbuf->flag = RTW_FREED_TO_FREELIST;
#endif
		rtw_mem_pool_add_to_list(pool,pbuf);
		if(pool->free > pool->thru) {
			pool->freed++;
			if(0 == pool->timestamp) {
				pool->timestamp = jiffies;
			}
		}	
		else {
			pool->freed=0;
			pool->timestamp=0;
		}
		if(pool->free_cb)
			pool->free_cb(pool);
	}	
	else {
		printk("Err:%s pool %p\n",__FUNCTION__, pbuf->pool);
		return FALSE;
	}	
	return TRUE;
}

static inline void rtw_mem_pool_set_max_min(rtw_mem_pool_t *pool, int max, int min)
{
	int div;
	pool->max = max;
	pool->min = min;
	pool->batch = RTW_MEM_POOL_BATCH;

	div = max/min;
	if(div <= 1) {
		printk("WARNING: max(%d) and min(%d) are too close\n",max,min);
	} else
	{
		pool->thru = min >> RTW_THRU_SHIFT;
	}
}

rtw_mem_pool_t *rtw_create_mem_pool(char *name, int max, int min, int size)
{
	rtw_mem_pool_t *pool;
	struct kmem_cache *mem_slab;
	int obj_size;
	
	/*create rtw pool*/
	pool = (rtw_mem_pool_t *)kmalloc(sizeof(rtw_mem_pool_t), GFP_KERNEL);
	if(!pool) {
		printk("!Err: %s %d Kmalloc Failed\n",__FUNCTION__,__LINE__);
		return NULL;
	}
	memset(pool,0x0,sizeof(rtw_mem_pool_t));
	strncpy(pool->name,name,RTW_MEM_POOL_NAME_SZ);
	rtw_mem_pool_set_max_min(pool,max,min);
	
	/*create linux pool with slab*/
	obj_size = SKB_DATA_ALIGN(size+sizeof(rtw_priv_buffer_t))+SKB_DATA_ALIGN(sizeof(struct skb_shared_info))+NET_SKB_PAD;
	mem_slab = kmem_cache_create(name,
								  obj_size,
								  0, SLAB_HWCACHE_ALIGN|SLAB_PANIC, NULL);	
	BUG_ON(mem_slab == NULL);
	
	pool->slab = mem_slab;
	pool->obj_size = obj_size;

	/*init list head & spinlock*/
	INIT_LIST_HEAD(&pool->free_list);
	spin_lock_init(&pool->lock);

	/*init fill work*/
	INIT_WORK(&pool->fill_work, rtw_mem_pool_fill_list_background);
	
	rtw_mem_pool_fill_list(pool,min);

	if(rtw_mempool_register_shrinker(pool) == 0)
		pool->shrinker_enabled = true;
	
	/*add to pool array*/
	if(rtw_add_mem_pool(size,pool) == FALSE)
		printk("%s:%d add mem pool failed\n",__FUNCTION__,__LINE__);
		
#ifdef RTW_MEM_POOL_DEBUG
	if(strcmp(name,"eth_buffer")==0)
	{	
		printk("jiffies s %d\n",jiffies);
		rtw_mem_find_in_all_buffer();
		printk("jiffies e %d\n",jiffies);
	}
#endif

	return pool;
}

void rtw_create_mem_pool_ex(char *name, int max, int min, int size)
{
	rtw_mem_pool_t *pool;
	RTW_MEM_POOL_MATCH_TYPE type=0;
	
	/*using existed pool as we can*/
	pool = rtw_find_mem_pool_by_name_sz(name,size,&type);
	
	BUG_ON(((type == RTW_MEM_POOL_SIZE_UNMATCH_NAME_MATCHED) 
		|| (type == RTW_MEM_POOL_SIZE_MATCHED)));
	
	if(NULL == pool) {
		if(NULL == rtw_create_mem_pool(name,max,min,size))
			printk("%s:%d create mem pool failed\n",__FUNCTION__,__LINE__);
	} else
	{
		if(pool->max != max || pool->min != min)
		{
			printk("%s:%d max min mismatch\n",__FUNCTION__,__LINE__);
			rtw_mem_pool_set_max_min(pool, max, min);
		}
	}
	return;
}

int rtw_remove_mem_pool(rtw_mem_pool_t *pool)
{
	int ret=0;
	
	/*del form pool array*/
	if(rtw_del_mem_pool(pool)==FALSE) {
		printk("%s %d del mem pool failed\n",__FUNCTION__,__LINE__);
		return FALSE;
	}

	cancel_work_sync(&pool->fill_work);

	rtw_mempool_unregister_shrinker(pool);
	
	/*return all buffer to linux pool*/
	rtw_mem_pool_return_buffer(pool,pool->max);

	/*destory slab*/
	kmem_cache_destroy(pool->slab);

	/*free self*/
	kfree(pool);
	
	return ret;
}

/*flush the pool but not remove really*/
int rtw_remove_mem_pool_ex(char *name)
{
	rtw_mem_pool_t *pool;
	pool=rtw_find_mem_pool_by_name(name);
	if(NULL == pool) {
		printk("%s:Err can not find pool by name %s\n",__FUNCTION__,name);
		return FALSE;
	}	
	/*flush mem pool*/
	rtw_mem_pool_return_buffer(pool,pool->max);
	return 0;
}

struct sk_buff *rtw_mem_pool_alloc_skb(int size)
{
	rtw_mem_pool_t *pool;
	void *data;
	struct sk_buff *skb;
	pool = rtw_find_mem_pool(size);
	if(pool == NULL) {
		printk("%s:%d Error. not found pool at %d\n", __FUNCTION__, __LINE__, size);
		return NULL;
	}
	data=rtw_mem_pool_alloc_buf(pool);
	if(data == NULL) {
		//printk("%s:%d alloc buf fail by %d\n", __FUNCTION__, __LINE__, size);
		return NULL;
	}
	/*data is slab alloced, ksize can be used, please refer to build_skb*/
	skb = build_skb(data,0);
	if(skb == NULL)
	{
		printk("%s:%d build skb fail by %d\n",__FUNCTION__,__LINE__,size);
		rtw_mem_pool_free_buf(data);
		return NULL;
	}
	skb_reserve(skb, NET_SKB_PAD);
	return skb;
}

int rtw_mem_pool_register_cb(char *poolname, int type, int (*func)(void *))
{
	rtw_mem_pool_t *pool=rtw_find_mem_pool_by_name(poolname);
	switch (type)
	{
	case RTW_MEM_POOL_FREE_CB:
		pool->free_cb = func;
		break;
	case RTW_MEM_POOL_ALLOC_CB:
		break;
	default:
		break;
	}
	return 0;
}
void rtw_mem_pool_timeout(unsigned long priv)
{
	int i;
	int level=0;
	for(i=0;i<RTW_MEM_POOL_MAX;i++) {
		if(rtw_pool_info_array[i].size != 0)
		{
			if(rtw_mem_pool_need_resize(rtw_pool_info_array[i].pool,&level))
				rtw_mem_pool_resize(rtw_pool_info_array[i].pool,level);
		}
	}
	mod_timer(&rtw_pool_timer, jiffies + RTW_MEM_IDLE_TIMEOUT);
	return;
}


int rtw_mem_pool_init(void)
{
	memset(&rtw_pool_info_array,0,sizeof(rtw_pool_info_array));
	
	/*init timer*/
	init_timer(&rtw_pool_timer);
	rtw_pool_timer.expires = jiffies + RTW_MEM_IDLE_TIMEOUT;
	rtw_pool_timer.data = (unsigned long)NULL;
	rtw_pool_timer.function = rtw_mem_pool_timeout;
	mod_timer(&rtw_pool_timer, jiffies + RTW_MEM_IDLE_TIMEOUT);

	/*add proc entry*/
	proc_create_data("rtw_mempool",0,&proc_root,&rtw_mempool_proc_fops,NULL);
	return 0;
}

void rtw_mem_pool_fini(void)
{
	/*del proc*/
	remove_proc_entry("rtw_mempool",&proc_root);

	/*del timer*/
	if (timer_pending(&rtw_pool_timer))
		del_timer_sync(&rtw_pool_timer);
	return;
}

//module_init(rtw_mem_pool_init);
/*fs_initcall priority is higher than device_initcall(module_init)
 *make sure rtw_mem_pool inited before device driver using it
 */
fs_initcall(rtw_mem_pool_init);
module_exit(rtw_mem_pool_fini);

EXPORT_SYMBOL(rtw_create_mem_pool_ex);
EXPORT_SYMBOL(rtw_remove_mem_pool_ex);
EXPORT_SYMBOL(rtw_mem_pool_alloc_skb);



