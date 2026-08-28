
#if defined(CONFIG_POWER_SAVE) || defined(CONFIG_PCIE_POWER_SAVING)
#define _8192CD_PWRCTRL_C_

#include <linux/irq.h>
#include <linux/gpio.h>
#ifdef CONFIG_WAKELOCK
#include <linux/wakelock.h>
#endif

#include "8192cd_headers.h"
#include "8192cd_debug.h"
#include "8192cd_pwrctrl.h"
#ifdef PLATFORM_ARM_BALONG
#include <linux/platform_device.h>
#endif
#if defined(CONFIG_WLAN_HAL)
#include "./WlanHAL/HalMac88XX/halmac_reg2.h"
#endif
#endif

#ifdef CONFIG_POWER_SAVE
#ifdef USE_WAKELOCK_MECHANISM
static struct wakeup_source *ws_wifi;
#endif

u16 temp_608;

int rtw_sdio_prepare(struct device *dev)
{
	struct sdio_func *func = dev_to_sdio_func(dev);
	struct net_device *netdev = sdio_get_drvdata(func);
	struct rtl8192cd_priv *priv = GET_DEV_PRIV(netdev);
	struct priv_shared_info *pshare = priv->pshare;

	if (!IS_DRV_OPEN(priv)) {
		DEBUG_INFO("[%s] driver closed, return.\n", __FUNCTION__);
		return 0;
	}

	// check tx pending queue is empty.
	if (pshare->pending_xmitbuf_queue.qlen)	{
		DEBUG_INFO("[%s] tx pending queue not empty.\n", __FUNCTION__);
		rtw_lock_suspend_timeout(priv, 2*priv->pmib->dot11OperationEntry.ps_timeout);
		return -1;
	}

	if (GET_HAL_INTF_DATA(priv)->SdioTxIntStatus) {
		DEBUG_INFO("[%s] SdioTxIntStatus is not idle.\n", __FUNCTION__);
		rtw_lock_suspend_timeout(priv, 2*priv->pmib->dot11OperationEntry.ps_timeout);
		return -1;
	}

	RTL_W8(0x286, RTL_R8(0x286)|BIT2);
	
	if (RTL_R8(0x286) & BIT1) {
		DEBUG_INFO("[%s] rx dma is idle.\n", __FUNCTION__);
	} else {
		DEBUG_INFO("[%s] rx dma is not idle.\n", __FUNCTION__);
	}

	pshare->ps_xfer_seq = pshare->xfer_seq;

	return 0;
}

void rtw_sdio_complete(struct device *dev)
{
	struct sdio_func *func = dev_to_sdio_func(dev);
	struct net_device *netdev = sdio_get_drvdata(func);
	struct rtl8192cd_priv *priv = GET_DEV_PRIV(netdev);

	if (!IS_DRV_OPEN(priv)) {
		DEBUG_INFO("[%s] driver closed, return.\n", __FUNCTION__);
		return;
	}

	RTL_W8(0x286 , (RTL_R8(0x286) & (~BIT2)));
}

int rtw_sdio_suspend(struct device *dev)
{
	struct sdio_func *func = dev_to_sdio_func(dev);
	struct net_device *netdev = sdio_get_drvdata(func);
	struct rtl8192cd_priv *priv = GET_DEV_PRIV(netdev);
	struct priv_shared_info *pshare = priv->pshare;
	struct ap_pwrctrl_priv *ps_handle = &pshare->ap_ps_handle;
	const int ps_level = priv->pmib->dot11OperationEntry.ps_level;
#if (BEAMFORMING_SUPPORT == 1) && (defined(CONFIG_WLAN_HAL_8192EE) || defined(CONFIG_WLAN_HAL_8822BE))
	PRT_BEAMFORMING_INFO pBeamformingInfo = &(priv->pshare->BeamformingInfo);
#endif
	
	unsigned char loc_bcn[3] = {0};
	unsigned char loc_probe[3] = {0};
	unsigned int i, hidden, wait_cnt = 0;
	u1Byte IsLinked = 0;
	int err = -1;
	_irqL irqL;

	if (!IS_DRV_OPEN(priv)) {
		DEBUG_INFO("[%s] driver closed, return.\n", __FUNCTION__);
#ifdef USE_WAKELOCK_MECHANISM
		mmc_pm_flag_t pm_flag = 0;
		pm_flag = sdio_get_host_pm_caps(func);

		pm_flag |= MMC_PM_KEEP_POWER;
		sdio_set_host_pm_flags(func, pm_flag);
		DEBUG_INFO("[%s] driver closed, but need keep wifi power.\n", __FUNCTION__);
#endif
		return 0;
	}

	if (pshare->offload_prohibited) {
		DEBUG_ERR("[%s] power save disabled ,return\n", __FUNCTION__);
		pshare->offload_function_ctrl = RTW_PM_AWAKE;
#ifdef USE_WAKELOCK_MECHANISM
		rtw_lock_suspend(priv);
		RTL_W8(0x286 , (RTL_R8(0x286) & (~BIT2)));
#endif
		return -1;
	}

	_enter_critical_mutex(&pshare->apps_lock, &irqL);
	
	if (RTW_PM_SUSPEND == pshare->offload_function_ctrl) {
		_exit_critical_mutex(&pshare->apps_lock, &irqL);
		return 0;
	}

	DEBUG_INFO("%s ===>\n", __FUNCTION__);

	DEBUG_INFO("[%s,%d] name = %s\n", __FUNCTION__, __LINE__, priv->dev->name);
	DEBUG_INFO("[%s,%d] ps_level = %d\n", __FUNCTION__, __LINE__, ps_level);
	DEBUG_INFO("[%s,%d] ps_timeout = %d\n", __FUNCTION__, __LINE__, priv->pmib->dot11OperationEntry.ps_timeout);

	DEBUG_INFO("[%s] total_assoc_num=%d\n", __FUNCTION__, pshare->total_assoc_num);

	temp_608 = RTL_R16(0x608);
	RTL_W16(0x608, ((temp_608|BIT7)&0xfcff));

#ifdef CONFIG_RTL_88E_SUPPORT
	if (pshare->total_assoc_num)
		RTL8188E_SuspendTxReport(priv);
#endif

	if (timer_pending(&pshare->beacon_timer))
		del_timer_sync(&pshare->beacon_timer);
	if (timer_pending(&pshare->ps_timer))
		del_timer_sync(&pshare->ps_timer);
	if (timer_pending(&pshare->xmit_check_timer))
		del_timer_sync(&pshare->xmit_check_timer);
	if (timer_pending(&pshare->LED_Timer))
		del_timer_sync(&pshare->LED_Timer);
#if (BEAMFORMING_SUPPORT == 1) && (defined(CONFIG_WLAN_HAL_8192EE) || defined(CONFIG_WLAN_HAL_8822BE))
	if (timer_pending(&pBeamformingInfo->BeamformingTimer))
		del_timer_sync(&pBeamformingInfo->BeamformingTimer);
#endif

	pshare->ps_timer_expires = 0;
	
	/* start beacon offload */
#ifdef USE_WAKELOCK_MECHANISM
	if (pshare->offload_function_ctrl == RTW_PM_AWAKE)
	{
		pshare->offload_function_ctrl = RTW_PM_PREPROCESS;
		pshare->ps_ctrl = RTW_ACT_POWERDOWN;
		update_beacon(pshare->bcn_priv[0]);
		pshare->ps_ctrl = RTW_ACT_POWERON;
	}
#endif
	
#if defined(CONFIG_RTL_88E_SUPPORT)
	loc_bcn[0] = TX_TOTAL_PAGE_NUMBER_88E + 1;
	loc_probe[0] = loc_bcn[0] + pshare->ap_offload_res[0].probe_offset;
#elif defined(CONFIG_WLAN_HAL_8192EE)
	loc_bcn[0] = 0xf6;
	loc_probe[0] = loc_bcn[0] + pshare->ap_offload_res[0].probe_offset;
#elif defined(CONFIG_WLAN_HAL_8822BE) || defined(CONFIG_WLAN_HAL_8821CE)
	loc_bcn[0] = 0;
	loc_probe[0] = loc_bcn[0] + pshare->ap_offload_res[0].probe_offset;
#endif

	hidden = 0;
	if (pshare->bcn_priv[0]->pmib->dot11OperationEntry.hiddenAP)
		hidden |= BIT0;

#ifdef MBSSID
	for (i = 1; i < pshare->nr_bcn; i++) {
		loc_bcn[i] = loc_bcn[0] + pshare->ap_offload_res[i].beacon_offset;
		loc_probe[i] = loc_bcn[0] + pshare->ap_offload_res[i].probe_offset;
		
		if (pshare->bcn_priv[i]->pmib->dot11OperationEntry.hiddenAP)
			hidden |= BIT(i);
	}
	
	DEBUG_INFO("clif loc: bcn[0]=%x probe[0]=%x bcn[1]=%x probe[1]=%x, hidden=%x\n",
			loc_bcn[0], loc_probe[0], loc_bcn[1], loc_probe[1], hidden);
#else
	DEBUG_INFO("clif loc: bcn[0]=%x probe[0]=%x, hidden=%x\n",
			loc_bcn[0], loc_probe[0], hidden);
#endif

	DEBUG_INFO("[%s] total tx bcn inrerface : %d\n", __FUNCTION__, pshare->nr_bcn);

#ifdef PLATFORM_ARM_BALONG
	if (pshare->wake_irq > 0)
		enable_irq_wake(pshare->wake_irq);
#endif

#if defined(CONFIG_WLAN_HAL_8192EE) || defined(CONFIG_WLAN_HAL_8822BE) || defined(CONFIG_WLAN_HAL_8821CE)
	RTL_W8(0x1c , (RTL_R8(0x1c) & (~(BIT1|BIT0))));
#endif
	//stop rx
	RTL_W8(0x286, RTL_R8(0x286)|BIT2);
	//polling rx dma idle
	while (!(RTL_R8(0x286) & BIT1)) {
		if (++wait_cnt > 100 || pshare->ps_ctrl == RTW_ACT_POWERON)
		{
			DEBUG_INFO("[%s] rx dma is not idle.\n", __FUNCTION__);
			goto fail;
		}
		msleep(1);
	}
	DEBUG_INFO("[%s] rx dma is idle.\n", __FUNCTION__);

	DisableSdioInterrupt( priv);
	ClearSdioInterrupt(priv);
	 
	ps_handle->suspend_processing = 1;

	if (pshare->total_assoc_num > 0)
		IsLinked = 1;

	DEBUG_INFO("[%s,%d] IsLinked = %x, hidAP = %x\n", __FUNCTION__, __LINE__, IsLinked, hidden);
	DEBUG_INFO("[%s,%d] offload duration = %d us\n", __FUNCTION__, __LINE__, OFFLOAD_DURATION);
	DEBUG_INFO("[%s,%d] Repeat trigger duration = %d %s\n", __FUNCTION__, __LINE__, 
				REPEAT_TRIGGER_DURATION * ((REPEAT_TRIGGER_UNIT==TRIGGER_TIME_2SEC)?2:8),
				(REPEAT_TRIGGER_UNIT==TRIGGER_TIME_2SEC)?"sec":"msec");

	RTL_W8(0x1c7, 0x0); //check send pulse or not.

	pshare->pwr_state = RTW_STS_SUSPEND;
	pshare->offload_function_ctrl = RTW_PM_SUSPEND;
	ps_handle->en_sapps = ((ps_level == 1) || (ps_level == 2));
	ps_handle->en_32k = (ps_level == 2);

	if (ps_handle->en_sapps) {
		rtw_ap_start_fw_ps(priv, ps_handle->en_32k, 1);
		delay_us(10);
	}
#if defined(CONFIG_RTL_88E_SUPPORT)
	set_wakeup_pin(priv, PULSE_DURATION, 1, 1, 1, 7);
	delay_us(10);
	//set_repeat_wake_pulse(priv, 1, REPEAT_TRIGGER_UNIT, REPEAT_TRIGGER_DURATION);
	//delay_us(10);
	set_bcn_resv_page(priv, loc_bcn[0], loc_bcn[1], loc_bcn[2]);
	delay_us(10);
	set_probe_res_resv_page(priv, loc_probe[0], loc_probe[1], loc_probe[2]);
	delay_us(10);

	set_ap_offload(priv, 0, hidden, 1, IsLinked);
#elif defined(CONFIG_WLAN_HAL_8192EE) || defined(CONFIG_WLAN_HAL_8822BE) || defined(CONFIG_WLAN_HAL_8821CE)
	GET_HAL_INTERFACE(priv)->SetAPOffloadHandler(priv, 1, IsLinked, 
				pshare->nr_bcn, hidden, 0, loc_bcn, loc_probe);
#endif
	delay_us(10);
	ps_handle->h2c_done = 1;

	wait_cnt = 0;
	// check firmware is receive H2C command for AP offload
#if defined(CONFIG_WLAN_HAL_8822BE) || defined(CONFIG_WLAN_HAL_8821CE)
	while (!(RTL_R32(0x120)&BIT16))
#else
	while (!(RTL_R8(0x130)&BIT3))
#endif
	{
		if (++wait_cnt > 20) {
			DEBUG_ERR("[%s] firmware no receive H2C command or already leave AP offload mode! \n", __FUNCTION__);
			goto fail;
		}
		delay_ms(1);
	}

	if (ps_handle->en_32k)
	set_ap_32k(priv, 1);

#ifdef USE_WAKELOCK_MECHANISM
	mmc_pm_flag_t pm_flag = 0;
	pm_flag = sdio_get_host_pm_caps(func);

	if (!(pm_flag & MMC_PM_KEEP_POWER)) {
		DEBUG_ERR("%s: cannot remain alive while host is suspended\n", sdio_func_id(func));
		err = -ENOSYS;
		goto fail;
	}
	
	DEBUG_INFO("cmd: suspend with MMC_PM_KEEP_POWER\n");
	pm_flag |= MMC_PM_KEEP_POWER;
	sdio_set_host_pm_flags(func, pm_flag);
#endif

	if (pshare->ps_xfer_seq != pshare->xfer_seq) {
		DEBUG_INFO("[%s] Detect traffic.\n", __FUNCTION__);
			goto fail;
		}

#if defined(CONFIG_WLAN_HAL_8822BE)
	RTL_W8(0x4fc, RTL_R8(0x4fc) | BIT0);
#endif

#ifdef PLATFORM_ARM_BALONG
	extern int BSP_PWRCTRL_WIFI_LowPowerEnter(void);
	BSP_PWRCTRL_WIFI_LowPowerEnter();
#endif

	DEBUG_INFO("<=== %s\n", __FUNCTION__);
	_exit_critical_mutex(&pshare->apps_lock, &irqL);

	return 0;

fail:
	pshare->offload_function_ctrl = RTW_PM_SUSPEND;
	__rtw_sdio_resume(priv);
	_exit_critical_mutex(&pshare->apps_lock, &irqL);
	
	return err;
}

int __rtw_sdio_resume(struct rtl8192cd_priv *priv)
{
	struct priv_shared_info *pshare = priv->pshare;
	struct ap_pwrctrl_priv *ps_handle = &pshare->ap_ps_handle;
	int i, wait_cnt = 0;

	// Don't do AP offload exit when not in AP offload state. otherwise it may cause TXDMA error.
	if (RTW_PM_SUSPEND != pshare->offload_function_ctrl)
		return 0;

	DEBUG_INFO("%s ===>\n", __FUNCTION__);

#ifdef PLATFORM_ARM_BALONG
	if (pshare->wake_irq > 0)
		disable_irq_wake(pshare->wake_irq);
#endif

#if defined(CONFIG_WLAN_HAL_8822BE)
	RTL_W8(0x4fc, RTL_R8(0x4fc) & ~BIT0);
#endif

	// Don't do H2C commands to exit AP offload during error recovery when H2C command which enable AP offload is not done.
	// Otherwise it will cause TXDMA error 0x14.
	if (ps_handle->h2c_done) {
		ps_handle->h2c_done = 0;
		if (ps_handle->en_32k) {
	set_ap_32k(priv, 0);
			delay_ms(1);
		}
#if defined(CONFIG_RTL_88E_SUPPORT)
	set_ap_offload(priv, 0, 0, 0, 0);
#elif defined(CONFIG_WLAN_HAL_8192EE) || defined(CONFIG_WLAN_HAL_8822BE) || defined(CONFIG_WLAN_HAL_8821CE)
	GET_HAL_INTERFACE(priv)->SetAPOffloadHandler(priv, 0, 0, 0, 0, 0, 0, 0);
#endif
		delay_ms(1);
		if (ps_handle->en_sapps)
			rtw_ap_stop_fw_ps(priv);

#ifdef CONFIG_RTL_88E_SUPPORT
		RTL_W8(REG_MBID_NUM, RTL_R8(REG_MBID_NUM)& (~BIT(3)));
#endif

		// check firmware is leave AP offload mode
#if defined(CONFIG_WLAN_HAL_8822BE) || defined(CONFIG_WLAN_HAL_8821CE)
		while ((RTL_R32(0x120)&BIT16))
#else
		while ((RTL_R8(0x130)&BIT3))
#endif
		{
			if (++wait_cnt > 20) {
				DEBUG_ERR("[%s] firmware no leave AP offload mode! \n", __FUNCTION__);
				break;
			}
			delay_ms(1);
		}
	}

	RTL_W8(0x286 , (RTL_R8(0x286) & (~BIT2)));
	for (i=0; i<10; i++) 
	{
		if(RTL_R8(0x286)&BIT2) 
		{
			DEBUG_ERR("[%s] resume 0x286 clear BIT2 fail\n", __FUNCTION__); 
			RTL_W8(0x286 , (RTL_R8(0x286) & (~BIT2)));
			delay_ms(1);
		}
		else 
			break;
	}

	ps_handle->suspend_processing = 0;
	RTL_W16(0x608,temp_608);
#if defined(CONFIG_WLAN_HAL_8822BE) || defined(CONFIG_WLAN_HAL_8821CE)
	atomic_set(&pshare->phw->seq, RTL_R16(0x4DA));
#endif
#ifdef CONFIG_RTL_88E_SUPPORT
	if (pshare->total_assoc_num)
		RTL8188E_ResumeTxReport(priv);
#endif

	// Update current Tx FIFO page & Tx OQT space
	WARN_ON(GET_HAL_INTF_DATA(priv)->SdioTxIntStatus);
	sdio_query_txbuf_status(priv);
	sdio_query_txoqt_status(priv);

	pshare->offload_function_ctrl = RTW_PM_AWAKE;
	pshare->pwr_state = RTW_STS_NORMAL;
	pshare->ps_ctrl = RTW_ACT_IDLE;

	if (priv->drv_state & DRV_STATE_OPEN) {
		rtw_offload_reinit_timer(priv);
		rtw_lock_suspend_timeout(priv, 2*priv->pmib->dot11OperationEntry.ps_timeout);
	}
	// Interrupt enable must be last step of the resume to avoid interfering with resume process.
	EnableSdioInterrupt(priv);
	
	DEBUG_INFO("<=== %s\n", __FUNCTION__);

	return 0;
} 

int rtw_sdio_resume(struct device *dev)
{
	struct sdio_func *func = dev_to_sdio_func(dev);
	struct net_device *netdev = sdio_get_drvdata(func);
	struct rtl8192cd_priv *priv;
	int err = 0;
	_irqL irqL;
	
	if (NULL == netdev)
		return 0;

	priv = GET_DEV_PRIV(netdev);

	if (!IS_DRV_OPEN(priv)) {
		DEBUG_INFO("[%s] driver closed, return.\n", __FUNCTION__);
		return 0;
	}

	_enter_critical_mutex(&priv->pshare->apps_lock, &irqL);
	
	err = __rtw_sdio_resume(priv);
	
	_exit_critical_mutex(&priv->pshare->apps_lock, &irqL);

	return err;
} 
/********************************************************/

#ifdef PLATFORM_ARM_BALONG
#include <linux/irq.h>

enum {
    GPIO_NORMAL=0,
    GPIO_INTERRUPT=0,    
};

#define GPIO_MAXIMUM (12)
#define GPIO_MAX_PINS (8)

#define BALONG_GPIO_0(_nr) (_nr)
#define BALONG_GPIO_1(_nr) (BALONG_GPIO_0(GPIO_MAX_PINS - 1) + (_nr) + 1 )
#define BALONG_GPIO_2(_nr) (BALONG_GPIO_1(GPIO_MAX_PINS - 1) + (_nr) + 1 )
#define BALONG_GPIO_3(_nr) (BALONG_GPIO_2(GPIO_MAX_PINS - 1) + (_nr) + 1 )
#define BALONG_GPIO_4(_nr) (BALONG_GPIO_3(GPIO_MAX_PINS - 1) + (_nr) + 1 )
#define BALONG_GPIO_5(_nr) (BALONG_GPIO_4(GPIO_MAX_PINS - 1) + (_nr) + 1 )
#define BALONG_GPIO_6(_nr) (BALONG_GPIO_5(GPIO_MAX_PINS - 1) + (_nr) + 1 )
#define BALONG_GPIO_7(_nr) (BALONG_GPIO_6(GPIO_MAX_PINS - 1) + (_nr) + 1 )

#define INT_GPIO_GP5 117

#define BALONG_GPIO_WIFI_WAKEUP_CHIP 5

//#define BALONG_GPIO_WIFI_WAKEUP_PIN 4
#define BALONG_GPIO_WIFI_WAKEUP_PIN 0
#define BALONG_GPIO_WIFI_WAKEUP BALONG_GPIO_5(BALONG_GPIO_WIFI_WAKEUP_PIN)

#define BALONG_GPIO_WIFI_PWR_PIN 2
#define BALONG_GPIO_WIFI_PWR BALONG_GPIO_5(BALONG_GPIO_WIFI_PWR_PIN) 

/*defined in  drivers/mmc/host/hisdio_sys_ctrl.h */
#define BALONG_GPIO_WIFI_RESET_PIN 6
#define BALONG_GPIO_WIFI_RESET BALONG_GPIO_5(BALONG_GPIO_WIFI_RESET_PIN)

extern int gpio_int_mask_set(unsigned int gpio);
extern int gpio_int_state_clear(unsigned int gpio);
extern int gpio_set_function(unsigned int gpio, unsigned function);
extern int gpio_int_trigger_set(unsigned int gpio, unsigned int trigger);
extern int gpio_int_unmask_set(unsigned int gpio);
extern int gpio_int_state_get(unsigned int gpio, unsigned *state);
extern int gpio_direction_input(unsigned int gpio);
extern int gpio_request(unsigned int gpio, const char *lebel);
extern void gpio_free(unsigned int gpio);
extern int gpio_direction_output(unsigned int gpio, int value);

extern void balong_wifi_vote(int element);
extern void balong_wifi_devote(int element);

#ifdef __LINUX_2_6__
irqreturn_t balong_gpio_wakeup_isr(int irq, void *dev_instance)
#else
void balong_gpio_wakeup_isr(int irq, void *dev_instance, struct pt_regs *regs)
#endif
{
	struct net_device *dev = NULL;
	struct rtl8192cd_priv *priv = NULL;
	unsigned int ucData;

	priv =(struct rtl8192cd_priv *) dev_instance;
	gpio_int_state_get(BALONG_GPIO_WIFI_WAKEUP, (unsigned int*)&ucData);
	printk("acli: get intr %d\n", ucData);
	if ( !ucData )
		return IRQ_NONE;

	gpio_int_state_clear(BALONG_GPIO_WIFI_WAKEUP);	 	  

	if ( priv->pshare->pwr_state == RTW_STS_SUSPEND ) {
		DEBUG_INFO("[%s,%d] RTW_STS_SUSPEND\n", __FUNCTION__, __LINE__);
		priv->pshare->pwr_state = RTW_STS_NORMAL;
		priv->pshare->ps_ctrl = RTW_ACT_IDLE;

		schedule_work(&priv->ap_cmd_queue);
	}

	return IRQ_HANDLED;
}

int set_balong_wakeup_pin(struct net_device *dev, struct rtl8192cd_priv *priv)
{
	int rc;
	int gpio_num =7;

	RTL_W32(GPIO_PIN_CTRL, RTL_R32(GPIO_PIN_CTRL) & ~BIT(gpio_num+8));

	msleep(10);

	gpio_int_mask_set(BALONG_GPIO_WIFI_WAKEUP);
	gpio_int_state_clear(BALONG_GPIO_WIFI_WAKEUP);
	// gpio_set_function(BALONG_GPIO_WIFI_WAKEUP);

	gpio_set_function(BALONG_GPIO_WIFI_WAKEUP, GPIO_INTERRUPT);
	gpio_int_trigger_set(BALONG_GPIO_WIFI_WAKEUP, IRQ_TYPE_EDGE_RISING);

	gpio_int_state_clear(BALONG_GPIO_WIFI_WAKEUP);
	gpio_int_unmask_set(BALONG_GPIO_WIFI_WAKEUP);

	dev->irq = INT_GPIO_GP5;
	priv->pshare->wake_irq = dev->irq;
	rc = request_irq(dev->irq, balong_gpio_wakeup_isr, IRQF_SHARED, dev->name, priv);
	if ( rc )
	{
		printk("some issue in wake-up irq, rx=%d\n", rc);
		return -1;
	}

	gpio_int_state_clear(BALONG_GPIO_WIFI_WAKEUP);
	gpio_int_unmask_set(BALONG_GPIO_WIFI_WAKEUP);

	return 0;
}
#endif // PLATFORM_ARM_BALONG

/********************************************************/
int init_wifi_wakeup_gpio(struct net_device *dev, struct rtl8192cd_priv *priv)
{
	int err = 0;
	DEBUG_INFO("[%s] ENTRY \n", __FUNCTION__);
#ifdef PLATFORM_ARM_BALONG	
	err = set_balong_wakeup_pin(dev, priv);
#endif
	return err;
}

void free_wifi_wakeup_gpio(struct net_device *dev, struct rtl8192cd_priv *priv)
{
	DEBUG_INFO("[%s] ENTRY \n", __FUNCTION__);
#ifdef PLATFORM_ARM_BALONG
	gpio_free(BALONG_GPIO_WIFI_WAKEUP_PIN);
#endif
}


#define RTW_SUSPEND_LOCK_NAME "rtw_wifi"

#ifdef USE_WAKELOCK_MECHANISM
#ifdef CONFIG_WAKELOCK
static struct wake_lock rtw_suspend_lock;
#endif
#endif

void rtw_suspend_lock_init(void)
{
	PRINT_INFO("[%s] ENTRY \n", __FUNCTION__);
	
#ifdef USE_WAKELOCK_MECHANISM
#ifdef CONFIG_WAKELOCK
	wake_lock_init(&rtw_suspend_lock, WAKE_LOCK_SUSPEND, RTW_SUSPEND_LOCK_NAME);
#elif defined(CONFIG_PM_WAKELOCKS)
	ws_wifi = wakeup_source_register("rtl8192 wake");
#endif
#endif
}

void rtw_suspend_lock_deinit(void)
{
	PRINT_INFO("[%s] ENTRY \n", __FUNCTION__);
	
#ifdef USE_WAKELOCK_MECHANISM
#ifdef CONFIG_WAKELOCK
	wake_lock_destroy(&rtw_suspend_lock);
#elif defined(CONFIG_PM_WAKELOCKS)
	wakeup_source_unregister(ws_wifi);
#endif
#endif
}

void rtw_lock_suspend(struct rtl8192cd_priv *priv)
{
	PRINT_INFO("[%s] ENTRY \n", __FUNCTION__);
	
#ifdef USE_WAKELOCK_MECHANISM
#ifdef CONFIG_WAKELOCK
	wake_lock(&rtw_suspend_lock);
#elif defined(CONFIG_PM_WAKELOCKS)
	__pm_stay_awake(ws_wifi);
#endif
#endif
}

void rtw_unlock_suspend(struct rtl8192cd_priv *priv)
{
	PRINT_INFO("[%s] ENTRY \n", __FUNCTION__);
	
#ifdef USE_WAKELOCK_MECHANISM
#ifdef CONFIG_WAKELOCK
	wake_unlock(&rtw_suspend_lock);
#elif defined(CONFIG_PM_WAKELOCKS)
	__pm_relax(ws_wifi);
#endif
#endif
}

void rtw_lock_suspend_timeout(struct rtl8192cd_priv *priv, unsigned int timeout)
{
	unsigned long expires;
	_irqL irqL;
	
	if (priv->pshare->offload_prohibited)
		return;
	
	if (!IS_DRV_OPEN(GET_ROOT(priv)))
		return;
	
	PRINT_INFO("[%s] ENTRY TO %d\n" , __FUNCTION__, timeout);

#ifdef USE_WAKELOCK_MECHANISM
#ifdef CONFIG_WAKELOCK
	wake_lock_timeout(&rtw_suspend_lock, timeout);
#elif defined(CONFIG_PM_WAKELOCKS)
	__pm_wakeup_event(ws_wifi, timeout);
#endif
	timeout = timeout + 1000;
#endif

	_enter_critical(&priv->pshare->offload_lock, &irqL);
	
	if (!timeout) {
		del_timer(&priv->pshare->ps_timer);
		priv->pshare->ps_timer_expires = 0;
		goto unlock;
	}
	
	expires = jiffies + RTL_MILISECONDS_TO_JIFFIES(timeout);
	
	if (!priv->pshare->ps_timer_expires || time_after(expires, priv->pshare->ps_timer_expires)) {
		mod_timer(&priv->pshare->ps_timer, expires);
		priv->pshare->ps_timer_expires = expires;
	}

unlock:
	_exit_critical(&priv->pshare->offload_lock, &irqL);
}

int rtw_ap_ps_xmit_monitor(struct rtl8192cd_priv *priv)
{
	if (0 == priv->assoc_num) {
		//printk("[%s,%d] Detects have traffic and no STA link, drop the packet.\n", __FUNCTION__, __LINE__);
		return 1;
	}
	
	priv->pshare->xfer_seq++;

	if (RTW_PM_SUSPEND == priv->pshare->offload_function_ctrl) {
		if (RTW_STS_SUSPEND == priv->pshare->pwr_state) {
			//printk("[%s,%d] Detects have traffic sent to STA.\n", __FUNCTION__, __LINE__);
			priv->pshare->pwr_state = RTW_STS_NORMAL;
			priv->pshare->ps_ctrl = RTW_ACT_IDLE;
			schedule_work(&GET_ROOT(priv)->ap_cmd_queue);
		}
		//printk("[%s,%d] Entry queue.\n", __FUNCTION__, __LINE__);
	} else {
		rtw_lock_suspend_timeout(priv, 2*GET_ROOT(priv)->pmib->dot11OperationEntry.ps_timeout);
	}

	return 0;
}

void rtw_ap_ps_recv_monitor(struct rtl8192cd_priv *priv)
{
	rtw_lock_suspend_timeout(priv, 2*GET_ROOT(priv)->pmib->dot11OperationEntry.ps_timeout);
}


void rtw_ap_ps_init(struct rtl8192cd_priv *priv)
{
	DEBUG_INFO("[%s] ENTRY \n", __FUNCTION__);

	priv->pshare->ap_ps_handle.h2c_done = 0;
	priv->pshare->ap_ps_handle.sleep_time = 0;
	priv->pshare->ap_ps_handle.suspend_processing = 0;
}

void rtw_ap_ps_deinit(struct rtl8192cd_priv *priv)
{
	DEBUG_INFO("[%s] ENTRY \n", __FUNCTION__);
}

void rtw_ap_stop_fw_ps(struct rtl8192cd_priv *priv)
{
	DEBUG_INFO("[%s] ENTRY \n", __FUNCTION__);
#if defined(CONFIG_RTL_88E_SUPPORT)
	set_softap_ps(priv, 0, 0, 0, 0);
#elif defined(CONFIG_WLAN_HAL_8192EE) || defined(CONFIG_WLAN_HAL_8821CE)
	/* Set H2C Cmd to FW To leave PS */
	GET_HAL_INTERFACE(priv)->SetSAPPsHandler(priv, 0, 0, 0, 0);  //zyj test
#endif
}

void rtw_ap_start_fw_ps(struct rtl8192cd_priv *priv, u4Byte en_32k, u4Byte reason)
{
	u1Byte sleep_time;
	u1Byte beaconInterval = priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod;
	int ps_sleep_time = priv->pmib->dot11OperationEntry.ps_sleep_time;

	DEBUG_INFO("[%s] ENTRY \n", __FUNCTION__);

	/* Set H2C Cmd to FW To enter PS */
	switch (priv->pshare->nr_bcn) {
	case 2:
		sleep_time = 20;//35
		break;
	case 3:
		sleep_time = 20;
		break;
	default:
		sleep_time = 20;
		break;
	}

	if (ps_sleep_time != 0)
		sleep_time = ps_sleep_time;

	priv->pshare->ap_ps_handle.sleep_time = sleep_time;
#if defined(CONFIG_RTL_88E_SUPPORT) || defined(CONFIG_WLAN_HAL_8822BE) || defined(CONFIG_WLAN_HAL_8821CE)
	sleep_time = (sleep_time * beaconInterval) / (100 * priv->pshare->nr_bcn);
#elif defined(CONFIG_WLAN_HAL_8192EE)
	sleep_time = (sleep_time * beaconInterval) / 100;
#endif

	DEBUG_INFO("[%s] sleep_time = %d\n", __FUNCTION__, sleep_time);

#if defined(CONFIG_RTL_88E_SUPPORT)
	set_softap_ps(priv, 1, en_32k, 1, sleep_time);
#elif defined(CONFIG_WLAN_HAL_8192EE) || defined(CONFIG_WLAN_HAL_8822BE) || defined(CONFIG_WLAN_HAL_8821CE)
	GET_HAL_INTERFACE(priv)->SetSAPPsHandler(priv, 1, en_32k, 1, sleep_time);
#endif
}

void sdio_power_save_timer(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	struct priv_shared_info *pshare = priv->pshare;
	
	if (pshare->offload_prohibited)
		return;

	if (pshare->pending_xmitbuf_queue.qlen || (pshare->nr_bcn > 3)) {
		rtw_lock_suspend_timeout(priv, 2*priv->pmib->dot11OperationEntry.ps_timeout);
		return;
	}

	DEBUG_INFO("[%s] ENTRY \n", __FUNCTION__);

	if (RTW_PM_AWAKE == pshare->offload_function_ctrl) {
		pshare->offload_function_ctrl = RTW_PM_PREPROCESS;
		pshare->ps_xfer_seq = pshare->xfer_seq;
	}
}

#endif // CONFIG_POWER_SAVE

#if defined(CONFIG_PCIE_POWER_SAVING)
void rtw_ap_start_fw_ps(struct rtl8192cd_priv *priv, u4Byte en_32k, u4Byte reason)
{
	u1Byte sleep_time;
	u1Byte beaconInterval = priv->pmib->dot11StationConfigEntry.dot11BeaconPeriod;

	DEBUG_INFO("[%s] ENTRY \n", __FUNCTION__);

	/* Set H2C Cmd to FW To enter PS */
	switch (priv->pshare->nr_bcn) {
	case 2:
	case 4:
		sleep_time = 35;
		break;
	case 3:
		sleep_time = 20;
		break;
	default:
		sleep_time = 70;
		break;
	}

	sleep_time = (sleep_time * beaconInterval) / 100;

	DEBUG_INFO("[%s] sleep_time = %d\n", __FUNCTION__, sleep_time);

#if defined(CONFIG_RTL_88E_SUPPORT)
	set_softap_ps(priv, 1, en_32k, 1, sleep_time);
#elif defined(CONFIG_WLAN_HAL_8192EE) || defined(CONFIG_WLAN_HAL_8821CE)
	GET_HAL_INTERFACE(priv)->SetSAPPsHandler(priv, 1, en_32k, 1, sleep_time);
#endif
}

void rtw_ap_stop_fw_ps(struct rtl8192cd_priv *priv)
{
	DEBUG_INFO("[%s] ENTRY \n", __FUNCTION__);
#if defined(CONFIG_RTL_88E_SUPPORT)
	set_softap_ps(priv, 0, 0, 0, 0);
#elif defined(CONFIG_WLAN_HAL_8192EE) || defined(CONFIG_WLAN_HAL_8821CE)

	/* Set H2C Cmd to FW To leave PS */
	GET_HAL_INTERFACE(priv)->SetSAPPsHandler(priv, 0, 0, 0, 0);  //zyj test
#endif
}

void set_ap_32k(struct rtl8192cd_priv *priv, BOOLEAN en_32K)
{
	u1Byte PreRpwmVal =0 ;
	u1Byte PreCpwmVal = 0;
	u1Byte PreCpwmVal_org = 0;
	u1Byte wait_times = 0;

	PreCpwmVal_org = RTL_R8(REG_PCIE_HCPWM1_V1);
	//printk("[%s,%d] : PreCpwmVal_org=0x%02x\n", __FUNCTION__, __LINE__, PreCpwmVal_org);

	/* set rpwm */
	PreRpwmVal = RTL_R8(REG_PCIE_HRPWM1_V1);

	if (en_32K) { //enable
		PreRpwmVal = PreRpwmVal ^ BIT(7);	//toggle
		PreRpwmVal = PreRpwmVal & ~BIT(6);	//clear fw ack
		PreRpwmVal = PreRpwmVal | BIT(0);	//enter
	}
	else { //disable
		PreRpwmVal = PreRpwmVal ^ BIT(7);	//toggle
		PreRpwmVal = PreRpwmVal | BIT(6);	//need fw ack
		PreRpwmVal = PreRpwmVal & ~BIT(0);	//leave
	}
	RTL_W8(REG_PCIE_HRPWM1_V1, PreRpwmVal);

	if (en_32K)
		return;

	/* polling cpwm ack bit */
	while (wait_times <= 50) {
		PreCpwmVal = RTL_R8(REG_PCIE_HRPWM1_V1);
		//printk("[%s,%d] : PreCpwmVal=0x%02x\n", __FUNCTION__, __LINE__, PreCpwmVal);
		if (((PreCpwmVal&BIT(7)) ^(PreCpwmVal_org&BIT(7))) == BIT(7))
			break;
		udelay(500);
		wait_times++;
	}

	if (wait_times >= 50)
	{
		DEBUG_WARN("[%s,%d] : leave 32K fail!!\n", __FUNCTION__, __LINE__);
	}
	else
	{
		DEBUG_INFO("[%s,%d] : leave 32K success!\n", __FUNCTION__, __LINE__);
	}
}
#endif

