#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/net.h>
#include <linux/socket.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/string.h>
#include <net/ip.h>
#include <net/protocol.h>
#include <net/route.h>
#include <net/sock.h>
#include <net/arp.h>
#include <net/raw.h>
#include <net/checksum.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netlink.h>

#include <net/rtl/rtl_types.h>
#include <net/rtl/rtl865x_netif.h>

#include <net/rtl/rtl865x_outputQueue.h>

#ifdef __KERNEL__
#include <linux/version.h>
#endif

#ifndef RT_ERR_OK
#define RT_ERR_OK							0
#endif

#ifndef RTL8367B_QUEUENO
#define RTL8367B_QUEUENO                    8
#endif

#ifndef RTK_MAX_NUM_OF_PORT
#define RTK_MAX_NUM_OF_PORT                 8
#endif

#ifndef QOS_PORT_ID_MAX
#define QOS_PORT_ID_MAX     				6
#endif

#ifndef RTL8367B_PRIMAX
#define RTL8367B_PRIMAX                     7
#endif

#ifndef RTL8367B_QOS_RATE_INPUT_MAX
#define RTL8367B_QOS_RATE_INPUT_MAX         (0x1FFFF * 8)
#endif

#ifndef RTL8367B_METERNO
#define RTL8367B_METERNO                    32
#endif


#define PORT_BASED_PRIORITY_ASSIGN_83XX 		1
#define PORT_BASED_PRIORITY_SHOW_83XX 			2

#define DOT1P_BASED_PRIORITY_ASSIGN_83XX		3
#define DOT1P_BASED_PRIORITY_SHOW_83XX 			4

#define DSCP_BASED_PRIORITY_ASSIGN_83XX 		5
#define DSCP_BASED_PRIORITY_SHOW_83XX 			6

#define CVLAN_BASED_PRIORITY_ASSIGN_83XX 		7
#define CVLAN_BASED_PRIORITY_SHOW_83XX 			8

#define SVLAN_BASED_PRIORITY_ASSIGN_83XX		9
#define SVLAN_BASED_PRIORITY_SHOW_83XX			10

#define DMAC_BASED_PRIORITY_ASSIGN_83XX 		11
#define DMAC_BASED_PRIORITY_SHOW_83XX 			12

#define SMAC_BASED_PRIORITY_ASSIGN_83XX 		13
#define SMAC_BASED_PRIORITY_SHOW_83XX 			14

#define QUEUE_NUMBER_83XX 						15
#define QUEUE_NUMBER_SHOW_83XX 					16

#define QUEUE_TYPE_STRICT_83XX 					17
#define QUEUE_TYPE_STRICT_SHOW_83XX 			18

#define QUEUE_TYPE_WEIGHTED_83XX 				19
#define QUEUE_TYPE_WEIGHTED_SHOW_83XX 			20

#define PRIORITY_TO_QID_83XX 					21
#define PRIORITY_TO_QID_SHOW_83XX 				22

#define PRIORITY_DECISION_83XX					23
#define PRIORITY_DECISION_SHOW_83XX				24

#define DOT1P_REMARK_83XX 						25
#define DOT1P_REMARK_SHOW_83XX 					26

#define DSCP_REMARK_83XX 						27
#define DSCP_REMARK_SHOW_83XX					28

#define QUEUE_RATE_83XX 						29
#define QUEUE_RATE_SHOW_83XX 					30

#define PORT_RATE_83XX 							31
#define PORT_RATE_SHOW_83XX 					32

#define QOS_ENABLE_83XX							33	
#define QOS_DISABLE_83XX						34
#define QOS_STATUS_SHOW_83XX					35

#define DISABLE_83XX_DSCP_REMARK				36
#define DISABLE_83XX_DOT1P_REMARK				37


typedef struct {
	int action;
	union{
		struct{
			char val[8];
			unsigned int valmask;
		} port_based_priority, dot1p_based_priority, queue_num, sys_priority;
		struct{
			unsigned char dscp[64];
			unsigned int dscpmask1;
			unsigned int dscpmask2;
		}dscp_based_priority;
		struct{
			unsigned char queue[8][8];
			unsigned int portmask;
			unsigned int queuemask;
		}queue_type;
		struct{
			unsigned int decision[8];
		}pri_decision;
		struct{
			unsigned char remark[8];
			unsigned int portmask;
			unsigned int prioritymask;
		}dot1p_remark, dscp_remark;
		struct{
			unsigned int portmask;
			unsigned int queuemask;
			unsigned int rate[8][8];
		}queue_rate;
		struct{
			unsigned int portmask;
			unsigned int rate[8];
		}port_rate;
	} qos_data;
} qos83xx_cmd_info_t, *qos83xx_cmd_info_p;


struct sock *hw_83xx_qos_sk = NULL;
unsigned int g83xxQosEnabled = 0;

static inline void port_based_priority_show_83xx(void)
{
	int ret;
	unsigned int i, priority;

	printk("83XX PORT_BASED_PRIORITY: \n");
	for(i = 0; i <= QOS_PORT_ID_MAX; i++){
		ret = rtl83xx_qosGetPortBasePri(i, &priority);
		if(ret == RT_ERR_OK)
			printk("    83xx Priority of port[%d] is %d\n", i, priority);
	}

	return;
}

static inline void dot1p_based_priority_show_83xx(void)
{
	int ret;
	unsigned int i, priority;

	printk("83XX DOT1P_BASED_PRIORITY: \n");
	for(i = 0; i < 8; i++){
		ret = rtl83xx_qosGet1pBasePri(i, &priority);
		if(ret == RT_ERR_OK)
			printk("    83XX Priority of 802.1p[%d] is %d\n", i, priority);
	}

	return;
}

static inline void dscp_based_priority_show_83xx(void)
{
	int ret;
	unsigned int i, priority;

	printk("83XX DSCP_BASED_PRIORITY: \n");
	for(i = 0; i < 64; i++){
		ret = rtl83xx_qosGetDscpBasePri(i, &priority);
		if(ret == RT_ERR_OK)
			printk("    83XX Priority of dscp[%d] is %d\n", i, priority);
	}

	return;
}

static inline void queue_number_show_83xx(void)
{
	int ret;
	unsigned int i, qnum;

	printk("83XX QUEUE_NUMBER: \n");
	for(i = 0; i <= QOS_PORT_ID_MAX; i++){
		ret = rtl83xx_qosGetQueueNum(i, &qnum);
		if(ret == RT_ERR_OK)
			printk("    83XX Queue number of port[%d] is %d\n", i, qnum);
	}

	return;
}

static inline void queue_type_strict_show_83xx(void)
{
	int ret;
	unsigned int i, j, queueType;

	printk("83XX QUEUE_TYPE_STRICT: \n");
	for(i = 0; i <= QOS_PORT_ID_MAX; i++){
		for(j=0; j < RTL8367B_QUEUENO; j++){
			ret = rtl83xx_qosGetQueueType(i, j, &queueType);
			if((ret == RT_ERR_OK) && (queueType == 0))
				printk("    83XX Port[%d]'s  Queue[%d] type is STRICT\n", i, j);
		}
	}

	return;
}

static inline void queue_type_weighted_show_83xx(void)
{
	int ret;
	unsigned int i, j, queueType;

	printk("83XX QUEUE_TYPE_WEIGHTED: \n");
	for(i = 0; i <= QOS_PORT_ID_MAX; i++){
		for(j = 0; j < RTL8367B_QUEUENO; j++){
			ret = rtl83xx_qosGetQueueType(i, j, &queueType);
			if((ret == RT_ERR_OK) && (queueType > 0))
				printk("    83XX Port[%d]'s  Queue[%d] type is WEIGHTED, and weight is %u\n", i, j, queueType);
		}
	}

	return;
}

static inline void priority_to_qid_show_83xx(void)
{
	int ret;
	unsigned int i, j, qid;

	printk("83XX PRIORITY_TO_QID: \n");
	for(i = 1; i <= RTL8367B_QUEUENO; i++){
		for(j = 0; j <= RTL8367B_PRIMAX; j++){
			ret = rtl83xx_qosGetPriToQid(i, j, &qid);
			if(ret == RT_ERR_OK)
				printk("    For Queue number[%d], priority[%d] is mapping to qid[%d]\n", i, j, qid);
		}
	}

	return;
}

static inline void priority_decision_show_83xx(void)
{
	unsigned int pri_dec[8] = {0};
	int ret;

	printk("83XX PRIORITY_DECISION: \n");
	ret = rtl83xx_qosGetPriDecisionAll(pri_dec, 8);

	if(ret == RT_ERR_OK){
		printk("    83XX Port	based decision priority is %d\n", pri_dec[0]);
		printk("    83XX Acl 	based decision priority is %d\n", pri_dec[1]);
		printk("    83XX Dscp   based decision priority is %d\n", pri_dec[2]);
		printk("    83XX 802.1P	based decision priority is %d\n", pri_dec[3]);
		printk("    83XX SVlan	based decision priority is %d\n", pri_dec[4]);
		printk("    83XX CVlan	based decision priority is %d\n", pri_dec[5]);
		printk("    83XX Dmac   based decision priority is %d\n", pri_dec[6]);
		printk("    83XX Smac 	based decision priority is %d\n", pri_dec[7]);
	}

	return;
}

static inline void dscp_remark_show_83xx(void)
{
	int ret;
	unsigned int i, j, flag, remark;

	ret = rtl83xx_dscp_remark_status_get(&flag);
	if(ret == RT_ERR_OK){
		printk("83XX WHOLE_SYSTEM DSCP_REMARK is %s: \n", (flag > 0 ? "ENABLE" : "DISABLE"));

		for(i = 0; i <= QOS_PORT_ID_MAX; i++){
			for(j = 0; j <= RTL8367B_PRIMAX; j++){
				ret = rtl83xx_dscp_remark_get(j, &remark);
				if(ret == RT_ERR_OK){
					printk("    83XX Port[%d]'s sys_pri[%d] is remarked as dscp[%d]\n", i, j, remark);
				}
			}

		}
	}
	
	return;
}

static inline void dot1p_remark_show_83xx(void)
{
	int ret;
	unsigned int i, j, flag, remark;

	printk("83XX DOT1P_REMARK: \n");
	for(i = 0; i <= QOS_PORT_ID_MAX; i++){
		ret = rtl83xx_1p_remark_status_get(i, &flag);
		if(ret == RT_ERR_OK){
			printk("\n    83XX Port[%d] 802.1P_REMARK is %s\n", i, (flag > 0 ? "ENABLE" : "DISABLE"));
			
			for(j = 0; j <= RTL8367B_PRIMAX; j++){
				ret = rtl83xx_1p_remark_get(j, &remark);
				if(ret == RT_ERR_OK)
					printk("    83XX Port[%d]'s sys_pri[%d] is remarked as 802.1P[%d]\n", i, j, remark);
			}
		}
	}

	return;
}

static inline void queue_rate_show_83xx(void)
{
	int ret;
	unsigned int i, j, rate;

	printk("83XX QUEUE_RATE: \n");
	for(i = 0; i <= QOS_PORT_ID_MAX; i++){
		ret = rtl83xx_qosGetQueueBwCtrl(i, &rate);
		if(ret == RT_ERR_OK){
			printk("\n    Port[%d] queue bandwidth control is %s\n", i, (rate > 0 ? "ENABLE" : "DISABLE"));
		}
		
		for(j=0; j < RTL8367B_QUEUENO; j++){
			ret = rtl83xx_qosGetQueueRate(i, j, &rate);
			if(ret == RT_ERR_OK)
				printk("    Port[%d] queue[%d]'s rate is %dkbps\n", i, j, rate);
		}
	}

	return;
}

static inline void port_rate_show_83xx(void)
{
	int ret;
	unsigned int i, bandwidth;

	printk("83XX PORT_RATE: \n");
	for(i = 0; i <= QOS_PORT_ID_MAX; i++){
		ret = rtl83xx_qosGetPortBandwidth(i, &bandwidth);
		if(ret == RT_ERR_OK)
			printk("    Port[%d]'s bandwidth is %dKbps\n", i, bandwidth);
	}

	return;
}

static inline void qos_83xx_control(unsigned int flag)
{
	rtl83xx_qos_init();
	g83xxQosEnabled = (flag > 0) ? 1 : 0;
	rtl83xx_rechange_qos(g83xxQosEnabled);
	
	return;
}

void hw_83xx_qos_netlink_receive (struct sk_buff *__skb)
{
// to_be_checked !!!
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
#else
	int ret, pid = 0;
	unsigned int i, j, queue_num;
	qos83xx_cmd_info_t send_data,recv_data;
	
	pid = rtk_nlrecvmsg(__skb, sizeof(qos83xx_cmd_info_t), &recv_data);
	
	if(pid<0)
		return;
	
	switch(recv_data.action){
	case PORT_BASED_PRIORITY_ASSIGN_83XX:
		for(i = 0; i < 8; i++){
			if((1<<i) & recv_data.qos_data.port_based_priority.valmask){
				ret = rtl83xx_qosSetPortBasePri(i, recv_data.qos_data.port_based_priority.val[i]);
				if(ret != RT_ERR_OK){
					printk("83XX Port based priority set to register failed. Error: 0x%x\n", ret);
				}

			}
		}
		send_data.action = PORT_BASED_PRIORITY_ASSIGN_83XX;
		break;

	case PORT_BASED_PRIORITY_SHOW_83XX:
		port_based_priority_show_83xx();
		break;
		
	case DOT1P_BASED_PRIORITY_ASSIGN_83XX:
		for(i = 0; i < 8; i++){
			if((1<<i) & recv_data.qos_data.dot1p_based_priority.valmask){
				ret = rtl83xx_qosSet1pBasePri(i, recv_data.qos_data.dot1p_based_priority.val[i]);
				if(ret != RT_ERR_OK){
					printk("83XX Vlan based priority set to register failed. Error: 0x%x\n", ret);
				}
			}
		}
		send_data.action = DOT1P_BASED_PRIORITY_ASSIGN_83XX;
		break;	

	case DOT1P_BASED_PRIORITY_SHOW_83XX:
		dot1p_based_priority_show_83xx();
		break;

	case DSCP_BASED_PRIORITY_ASSIGN_83XX:
		for(i = 0; i < 64; i++){
			if(((i<32) && ((1<<i) & recv_data.qos_data.dscp_based_priority.dscpmask1)) ||
			   ((i>=32) && ((1<<(i-32)) & recv_data.qos_data.dscp_based_priority.dscpmask2))){
				ret = rtl83xx_qosSetDscpBasePri(i, recv_data.qos_data.dscp_based_priority.dscp[i]);
				if(ret != RT_ERR_OK){
					printk("Dscp based priority set to register failed. Error: 0x%x\n", ret);
				}
			}
		}
		send_data.action = DSCP_BASED_PRIORITY_ASSIGN_83XX;
		break;

	case DSCP_BASED_PRIORITY_SHOW_83XX:
		dscp_based_priority_show_83xx();
		break;
	
	case QUEUE_NUMBER_83XX:
		for(i = 0; i < 8; i++){
			if((1 << i) & recv_data.qos_data.queue_num.valmask){
				ret = rtl83xx_qosSetQueueNum(i, recv_data.qos_data.queue_num.val[i]);
				if(ret != RT_ERR_OK){
					printk("83XX Queue number set to register failed. Error: 0x%x\n", ret);
				}
			}
		}
		send_data.action = QUEUE_NUMBER_83XX;
		break;

	case QUEUE_NUMBER_SHOW_83XX:
		queue_number_show_83xx();
		break;
	
	case QUEUE_TYPE_STRICT_83XX:
		for(i=0; i<8; i++){
			if((1<<i) & recv_data.qos_data.queue_type.portmask){
				for(j=0; j < RTL8367B_QUEUENO; j++){
					if((1<<j) & recv_data.qos_data.queue_type.queuemask){
						if(recv_data.qos_data.queue_type.queue[i][j] == 255){
								ret = rtl83xx_qosSetQueueType(i, j, 0);
								if(ret != RT_ERR_OK){
									printk("83XX Queue type STRICT set to register failed. Error: 0x%x\n", ret);
								}
							}
					}
				}
			}
		}
		send_data.action = QUEUE_TYPE_STRICT_83XX;
		break;
			
	case QUEUE_TYPE_STRICT_SHOW_83XX:
		queue_type_strict_show_83xx();
		break;
	
	case QUEUE_TYPE_WEIGHTED_83XX:
		for(i = 0; i < 8; i++){
			if((1 << i) & recv_data.qos_data.queue_type.portmask){
				for(j = 0; j < RTL8367B_QUEUENO; j++){
					if((1 << j) & recv_data.qos_data.queue_type.queuemask){
						if((recv_data.qos_data.queue_type.queue[i][j] > 0) && (recv_data.qos_data.queue_type.queue[i][j] != 255)){
							ret = rtl83xx_qosSetQueueType(i, j, recv_data.qos_data.queue_type.queue[i][j]);
							if(ret != RT_ERR_OK){
								printk("83XX Queue type WEIGHTED set to register failed. Error: 0x%x\n", ret);
							}
						}
					}
				}
			}
		}
		send_data.action = QUEUE_TYPE_WEIGHTED_83XX;
		break;

	case QUEUE_TYPE_WEIGHTED_SHOW_83XX:
		queue_type_weighted_show_83xx();
		break;
	
	case PRIORITY_TO_QID_83XX:
		for(i = 0; i < 8; i++){
			ret = rtl83xx_qosGetQueueNum(i, &queue_num);
			if(ret == RT_ERR_OK){
				for(j = 0; j < 8; j++)
				{
					if((1<<j) & recv_data.qos_data.sys_priority.valmask){
						ret = rtl83xx_qosSetPriToQid(queue_num, j, recv_data.qos_data.sys_priority.val[j]);
						if(ret != RT_ERR_OK){
							printk("83XX Priority to qid set to register failed. Error: 0x%x\n", ret);
						}
					}
				}
			}
		}
		send_data.action = PRIORITY_TO_QID_83XX;
		break;

	case PRIORITY_TO_QID_SHOW_83XX:
		priority_to_qid_show_83xx();
		break;
	
	case PRIORITY_DECISION_83XX:	
		for(i = PRIDEC_PORT; i < PRIDEC_END; i++){
			ret = rtl83xx_qosSetPriDecision(i, recv_data.qos_data.pri_decision.decision[i]);
			if(ret != RT_ERR_OK){
				printk("83XX Priority decision %d set to register failed. Error: 0x%x\n", i, ret);
			}
		}
		send_data.action = PRIORITY_DECISION_83XX;
		break;

	case PRIORITY_DECISION_SHOW_83XX:
		priority_decision_show_83xx();
		break;

	case DOT1P_REMARK_83XX:
		for(i = 0; i < 8; i++){
			if((1 << i) & recv_data.qos_data.dot1p_remark.portmask){
				for(j = 0; j < 8; j++){
					if(((1 << j) & recv_data.qos_data.dot1p_remark.prioritymask) && recv_data.qos_data.dot1p_remark.remark[j] != 255){
						ret = rtl83xx_1p_remark_set(i, j, recv_data.qos_data.dot1p_remark.remark[j]);
						if(ret != RT_ERR_OK){
							printk("83XX 802.1P remark set to register failed. Error: 0x%x\n", ret);
						}
					}
				}
			}
		}
		send_data.action = DOT1P_REMARK_83XX;
		break;

	case DOT1P_REMARK_SHOW_83XX:
		dot1p_remark_show_83xx();
		break;

	case DSCP_REMARK_83XX:
		for(j=0; j<8; j++){
			if(((1<<j) & recv_data.qos_data.dscp_remark.prioritymask) && recv_data.qos_data.dscp_remark.remark[j] != 255){
				ret = rtl83xx_dscp_remark_set(0xFF, j, recv_data.qos_data.dscp_remark.remark[j]);
				if(ret != RT_ERR_OK){
					printk("83XX Dscp remark set to register failed. Error: 0x%x\n", ret);
				}
			}
		}
		send_data.action = DSCP_REMARK_83XX;
		break;

	case DSCP_REMARK_SHOW_83XX:
		dscp_remark_show_83xx();
		break;

	case QUEUE_RATE_83XX:
		for(i = 0; i < 8; i++){
			if((1<<i) & recv_data.qos_data.queue_rate.portmask){
				for(j = 0; j < RTL8367B_QUEUENO; j++){
					if(((1<<j) & recv_data.qos_data.queue_rate.queuemask) && recv_data.qos_data.queue_rate.rate[i][j]) {
						ret = rtl83xx_qosSetQueueRate(i, RTL_WANPORT_MASK, j, recv_data.qos_data.queue_rate.rate[i][j]);
						if(ret != RT_ERR_OK){
							printk("83XX Queue rate set to register failed. Error: 0x%x\n", ret);
						}
					}
				}
			}
		}
		
		send_data.action = QUEUE_RATE_83XX;
		break;

	case QUEUE_RATE_SHOW_83XX:
		queue_rate_show_83xx();
		break;
			
	case PORT_RATE_83XX:
		for(i = 0; i < 8; i++){
			if((1<<i) & recv_data.qos_data.port_rate.portmask){
				ret = rtl83xx_qosSetPortBandwidth(i, recv_data.qos_data.port_rate.rate[i]);
				if(ret != RT_ERR_OK){
					printk("83XX Port rate set to register failed. Error: 0x%x\n", ret);
				}
			}
		}

		send_data.action = PORT_RATE_83XX;
		break;

	case PORT_RATE_SHOW_83XX:
		port_rate_show_83xx();
		break;

	case QOS_ENABLE_83XX:
		qos_83xx_control(1);
		break;

	case QOS_DISABLE_83XX:
		qos_83xx_control(0);
		break;

	case QOS_STATUS_SHOW_83XX:
		if(g83xxQosEnabled){
			printk("83XX HW QoS is ENABLE\n");
		}
		else{
			printk("83XX HW QoS is DISABLE\n");
		}
		break;

	case DISABLE_83XX_DSCP_REMARK:
		rtl83xx_flush_dscp_remark_setting();
	#if !defined(CONFIG_RTL_83XX_API_V1_4)
		rtl83xx_disable_dscp_remark_setting();
	#endif
		break;
		
	case DISABLE_83XX_DOT1P_REMARK:
		rtl83xx_flush_1p_remark_setting();
		rtl83xx_disable_1p_remark_setting();
		break;
		
	default:
		break;
	}
	
	rtk_nlsendmsg(pid, hw_83xx_qos_sk, sizeof(qos83xx_cmd_info_t), &send_data);
#endif

	return;
}


int hw_83xx_qos_init_netlink(void)
{
// to_be_checked !!!
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
  	hw_83xx_qos_sk = netlink_kernel_create(&init_net, NETLINK_RTK_HW_83XX_QOS, 0, hw_83xx_qos_netlink_receive, NULL, THIS_MODULE);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
	struct netlink_kernel_cfg cfg;
	memset(&cfg,0,sizeof(cfg));
	
	cfg.cb_mutex 	= NULL;
	cfg.groups 		= 0;
	cfg.input 		= hw_83xx_qos_netlink_receive;
	hw_83xx_qos_sk 	= netlink_kernel_create(&init_net, NETLINK_RTK_HW_83XX_QOS, &cfg);
#endif
  	if (!hw_83xx_qos_sk) {
    		printk(KERN_ERR "Netlink[Kernel] Cannot create netlink socket for hw 83xx qos config.\n");
    		return -EIO;
  	}
  	printk("Netlink[Kernel] create socket for hw 83xx qos config ok.\n");
	
  	return 0;
}
