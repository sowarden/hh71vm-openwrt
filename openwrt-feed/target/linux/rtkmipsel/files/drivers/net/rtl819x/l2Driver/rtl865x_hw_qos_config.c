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
#include <linux/inetdevice.h>
#include <linux/icmp.h>
#include <net/udp.h>
#include <net/tcp.h>

#include <net/rtl/rtl_types.h>
#ifdef CONFIG_NETFILTER
#include <net/netfilter/nf_conntrack.h>
#include <net/rtl/fastpath/fastpath_core.h>
#endif
#include <net/rtl/rtl865x_netif.h>
#include <net/rtl/rtl_nic.h>

#ifdef CONFIG_RTL_LAYERED_ASIC_DRIVER
#include <AsicDriver/asicRegs.h>
#include <AsicDriver/rtl865x_asicCom.h>
#include <AsicDriver/rtl865x_asicL2.h>
#else
#include <AsicDriver/asicRegs.h>
#include <AsicDriver/rtl8651_tblAsicDrv.h>
#endif
#ifdef __KERNEL__
#include <linux/version.h>
#endif

#ifdef CONFIG_RTL_8197F
#include <common/rtl865x_vlan.h>
#include <common/rtl_errno.h>
#endif

#if defined(CONFIG_RTL_HW_QOS_SUPPORT)
#include <net/rtl/rtl865x_outputQueue.h>
#endif

#define SET_QUEUE_FLOW_CONTROL

struct sock *hw_qos_sk = NULL;

struct qos_cmd_info_s{
	int action;
	union{
		struct{
			char port[8];
			unsigned int portmask;
		} port_based_priority, queue_num;
		struct{
			char vlan[8];
			unsigned int vlanmask;
		}vlan_based_priority;
		struct{
			char dscp[64];
			unsigned int dscpmask1;
			unsigned int dscpmask2;
		}dscp_based_priority;
		#if defined(CONFIG_RTL_8197F)
		struct{
			unsigned int vid[8];
			unsigned char pri[8];
		}vid_based_priority;
		#endif
		struct{
		#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
			unsigned char queue[8][8];
		#else
			unsigned char queue[8][6];
		#endif
			unsigned int portmask;
			unsigned int queuemask;
		}queue_type
	#if defined(SET_QUEUE_FLOW_CONTROL)
		, queue_flow_control
	#endif
		;
		struct{
			char priority[8];
			unsigned int prioritymask;
		}sys_priority;
		struct{
			unsigned char priority[8];
			unsigned int prioritymask;
		}cpu_priority;
	#if defined(CONFIG_RTL_HW_QOS_SUPPORT)
		struct{
			rtk_cmd_mark_t markInfo[MAX_MARK_NUM_PER_DEV];
			unsigned char netifName[16];
			unsigned short countmask;
		}mark_to_prio;
	#endif	
		struct{
			#if defined(CONFIG_RTL_8197F)
			char decision[6];
			#else
			char decision[5];
			#endif
		}pri_decision;
		struct{
			char remark[8][8];
			unsigned int portmask;
			unsigned int prioritymask;
		}vlan_remark, dscp_remark;
		struct{
		#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
			unsigned int apr[8][8];
			unsigned char burst[8][8];
			unsigned char ppr[8][8];
		#else
			unsigned int apr[8][6];
			unsigned char burst[8][6];
			unsigned char ppr[8][6];
		#endif
		
			unsigned int portmask;
			unsigned int queuemask;
		}queue_rate;
		struct{
			unsigned int portmask;
			unsigned int apr[8];
		}port_rate;
		struct{
			unsigned int portmask;
			unsigned short pri[8];
		}port_pri;
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
		struct{
			char dscpv6[64];
			unsigned int dscpv6mask1;
			unsigned int dscpv6mask2;
		}dscpv6_based_priority;
		struct{
			char dscpv6_decision;
		}dscpv6_pri_decision;
#endif
	} qos_data;
};

#define PORT_BASED_PRIORITY_ASSIGN 1
#define VLAN_BASED_PRIORITY_ASSIGN 2
#define DSCP_BASED_PRIORITY_ASSIGN 3
#define QUEUE_NUMBER 4
#define QUEUE_TYPE_STRICT 5
#define QUEUE_TYPE_WEIGHTED 6
#define PRIORITY_TO_QID 7
#define PRIORITY_DECISION 8
#define VLAN_REMARK 9
#define DSCP_REMARK 10
#define PORT_BASED_PRIORITY_SHOW 11
#define VLAN_BASED_PRIORITY_SHOW 12
#define DSCP_BASED_PRIORITY_SHOW 13
#define QUEUE_NUMBER_SHOW 14
#define QUEUE_TYPE_STRICT_SHOW 15
#define QUEUE_TYPE_WEIGHTED_SHOW 16
#define PRIORITY_TO_QID_SHOW 17
#define PRIORITY_DECISION_SHOW 18
#define VLAN_REMARK_SHOW 19
#define DSCP_REMARK_SHOW 20
#define QUEUE_RATE 21
#define QUEUE_RATE_SHOW 22
#define FLOW_CONTROL_ENABLE 23
#define FLOW_CONTROL_DISABLE 24
#define FLOW_CONTROL_CONFIGURATION_SHOW 25
#define PORT_RATE 26
#define PORT_RATE_SHOW 27
#define PORT_DEFAULT_8021P 28
#define PORT_DEFAULT_8021P_SHOW 29
#if defined(CONFIG_RTL_8197F)
#define VID_BASED_PRIORITY_ASSIGN 30
#define VID_BASED_PRIORITY_SHOW 31
#endif

#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
#define MAX_QUEUE_NUM		8
#define DSCPV6_BASED_PRIORITY_ASSIGN 32
#define DSCPV6_PRIORITY_DECISION 33
#define DSCPV6_BASED_PRIORITY_SHOW 34
#define DSCPV6_PRIORITY_DECISION_SHOW 35
#else
#define MAX_QUEUE_NUM		6
#endif
#define CPU_PRIORITY_TO_QID 36
#define CPU_PRIORITY_TO_QID_SHOW 37

#if defined(SET_QUEUE_FLOW_CONTROL)
#define QUEUE_FLOW_CONTROL 					38
#define QUEUE_FLOW_CONTROL_SHOW 			39
#endif

#if defined(CONFIG_RTL_HW_QOS_SUPPORT)
#define TC_COMMAND_ENABLE					40
#define TC_COMMAND_DISABLE					41
#define MARK_TO_PRIORITY					42
#endif

#define DISABLE_DSCP_REMARK					43
#define DISABLE_DOT1P_REMARK				44

int32 rtl8651_setAsicPortDefPri(uint32 port, uint32 pri);
int32 rtl8651_getAsicPortDefPri(uint32 port, uint32 *pri) ;

static inline void port_based_priority_show(void)
{
	int i, ret;
	enum PRIORITYVALUE priority;

	printk("PORT_BASED_PRIORITY: \n");
	for(i=0; i<9; i++){
		ret = rtl8651_getAsicPortBasedPriority(i, &priority);
		if(ret == SUCCESS)
			printk("    Priority of port[%d] is %d\n", i, priority);
	}

	return;
}

static inline void vlan_based_priority_show(void)
{
	int i, ret;
	enum PRIORITYVALUE priority;

	printk("VLAN_BASED_PRIORITY: \n");
	for(i=0; i<9; i++){
		ret = rtl8651_getAsicDot1qAbsolutelyPriority(i, &priority);
		if(ret == SUCCESS)
			printk("    Priority of vlan_pri[%d] is %d\n", i, priority);
	}

	return;
}

static inline void dscp_based_priority_show(void)
{
	int i, ret;
	enum PRIORITYVALUE priority;

	printk("DSCP_BASED_PRIORITY: \n");
	for(i=0; i<64; i++){
		ret = rtl8651_getAsicDscpPriority(i, &priority);
		if(ret == SUCCESS)
			printk("    Priority of dscp[%d] is %d\n", i, priority);
	}

	return;
}

#if defined(CONFIG_RTL_8197F)
static inline void vid_based_priority_show(void)
{
	int i;
	int ret;
	uint8 priority;	

	printk("VID_BASED_PRIORITY: \n");
		
	for(i=0; i<VLAN_NUMBER; i++){
		ret = rtl865x_getVlanHp(i, &priority);
		if((ret == SUCCESS))
			printk("    Priority of vid[%d] is %d\n", i, priority);
	}

	return;
}
#endif

static inline void queue_number_show(void)
{
	int i, ret;
	enum QUEUENUM qnum;

	printk("QUEUE_NUMBER: \n");
	for(i=0; i<8; i++){
		ret = rtl8651_getAsicOutputQueueNumber(i, &qnum);
		if(ret == SUCCESS)
			printk("    Queue number of port[%d] is %d\n", i, qnum);
	}

	return;
}

static inline void queue_type_strict_show(void)
{
	int i, ret, j;
	enum QUEUETYPE queueType;

	printk("QUEUE_TYPE_STRICT: \n");
	for(i=0; i<8; i++){
		for(j=0; j < MAX_QUEUE_NUM; j++){
			ret = rtl8651_getAsicQueueStrict(i, j, &queueType);
			if((ret == SUCCESS) && (queueType == 0))
				printk("    Port[%d]'s  Queue[%d] type is STRICT\n", i, j);
		}
	}

	return;
}

static inline void queue_type_weighted_show(void)
{
	int i, ret, j, weight;
	enum QUEUETYPE queueType;

	printk("QUEUE_TYPE_WEIGHTED: \n");
	for(i=0; i<8; i++){
		for(j=0; j < MAX_QUEUE_NUM; j++){
			ret = rtl8651_getAsicQueueWeight(i, j, &queueType, &weight);
			if((ret == SUCCESS) && (queueType == 1))
				printk("    Port[%d]'s  Queue[%d] type is WEIGHTED, and weight is %u\n", i, j, weight);
		}
	}

	return;
}

#if defined(SET_QUEUE_FLOW_CONTROL)
static inline void queue_flow_control_show(void)
{
	int i, ret, j, status;

	printk("QUEUE_FLOW_CONTROL: \n");
	for(i=0; i<8; i++){
		for(j=0; j < MAX_QUEUE_NUM; j++){
			ret = rtl8651_getAsicQueueFlowControlConfigureRegister(i, j, &status);
			if(ret == SUCCESS){
				if(status > 0){
					printk("    Port[%d]'s  Queue[%d] Flow Control is Enabled.\n", i, j);
				}
				else{
					printk("    Port[%d]'s  Queue[%d] Flow Control is Disabled.\n", i, j);
				}
			}
		}
	}

	return;
}
#endif

static inline void priority_to_qid_show(void)
{
	int i, j, ret;
	enum QUEUEID qid;

	printk("PRIORITY_TO_QID: \n");
	for(i=1; i < (MAX_QUEUE_NUM+1); i++){
		for(j=0; j<8; j++){
			ret = rtl8651_getAsicPriorityToQIDMappingTable(i, j, &qid);
			if(ret == SUCCESS)
				printk("    For Queue number[%d], priority[%d] is mapping to qid[%d]\n", i, j, qid);
		}
	}

	return;
}

static inline void CPU_priority_to_qid_show(void)
{
	int j, ret;
	enum QUEUEID qid;

	printk("CPU_PRIORITY_TO_QID: \n");

	for(j=0; j<8; j++){
		ret = rtl8651_getAsicCPUPriorityToQIDMappingTable(CPU, j, &qid);
		if(ret == SUCCESS)
			printk("    For CUP port, priority[%d] is mapping to qid[%d]\n", j, qid);
	}

	return;

}

static inline void priority_decision_show(void)
{
	unsigned int portpri, dot1qpri, dscppri, aclpri, natpri;
	#if defined(CONFIG_RTL_8197F)
	unsigned int vidpri;
	#endif
	int ret;

	printk("PRIORITY_DECISION: \n");
	#if defined(CONFIG_RTL_8197F)
	ret = rtl8651_getAsicPriorityDecision(&portpri, &dot1qpri, &dscppri, &aclpri, &natpri, &vidpri);
	#else
	ret = rtl8651_getAsicPriorityDecision(&portpri, &dot1qpri, &dscppri, &aclpri, &natpri);
	#endif
	if(ret == SUCCESS){
		printk("    Port   based decision priority is %d\n", portpri);
		printk("    Vlan  based decision priority is %d\n", dot1qpri);
		printk("    Dscp based decision priority is %d\n", dscppri);
		printk("    Acl    based decision priority is %d\n", aclpri);
		printk("    Nat    based decision priority is %d\n", natpri);
		#if defined(CONFIG_RTL_8197F)
		printk("    VID    based decision priority is %d\n", vidpri);
		#endif
	}

	return;
}
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
static inline void dscpv6_based_priority_show(void)
{
	int i, ret;
	enum PRIORITYVALUE priority;

	printk("IPV6_DSCP_BASED_PRIORITY: \n");
	for(i=0; i<64; i++){
		ret = rtl819x_getAsicv6DscpPriority(i, &priority);
		if(ret == SUCCESS)
			printk("    Priority of dscp[%d] is %d\n", i, priority);
	}

	return;
}
static inline void dscpv6_priority_decision_show(void)
{
	unsigned int v6dscppri;
	int ret;

	printk("IPV6 DSCP BASED QOS PRIORITY DECISION: \n");
	ret = rtl819x_getAsicv6DscpPriorityDecision(&v6dscppri);
	if(ret == SUCCESS){
		printk("	Ipv6 dscp  based decision priority is %d\n", v6dscppri);
	}

	return;
}
#endif
static inline void vlan_remark_show(void)
{
	int i, j, ret, remark;

	printk("VLAN_REMARK: \n");
	for(i=0; i<8; i++){
		for(j=0; j<8; j++){
			ret = rtl8651_getAsicVlanRemark(i, j, &remark);
			if(ret == SUCCESS)
				printk("    Port[%d]'s sys_pri[%d] is remarked as vlan_pri[%d]\n", i, j, remark);
		}

	}

	return;
}

static inline void dscp_remark_show(void)
{
	int i, j, ret, remark;

	printk("DSCP_REMARK: \n");
	for(i=0; i<8; i++){
		for(j=0; j<8; j++){
			ret = rtl8651_getAsicDscpRemark(i, j, &remark);
			if(ret == SUCCESS)
				printk("    Port[%d]'s sys_pri[%d] is remarked as dscp[%d]\n", i, j, remark);
		}

	}

	return;
}

static inline void queue_rate_show(void)
{
	int i, j, ret;
	unsigned int ppr, apr, burst;

	printk("QUEUE_RATE: \n");
	for(i=0; i<8; i++){
		for(j=0; j < MAX_QUEUE_NUM; j++){
			ret = rtl8651_getAsicQueueRate(i, j, &ppr, &burst, &apr);
			if(ret == SUCCESS)
				printk("    Port[%d] queue[%d]'s  ppr is %d, burst is %d, apr is %dkbps\n", i, j, ppr, burst, apr+1);
		}

	}

	return;
}

static inline void port_default_8021p_show(void)
{
	int i, ret;
	unsigned int pri;

	printk("PORT_DEFAULT_8021P: \n");
	for(i=0; i<8; i++){
		ret = rtl8651_getAsicPortDefPri(i, &pri);
		if(ret == SUCCESS)
			printk("    Port[%d]'s default 8021p is %d\n", i, pri);
	}

	return;
}

static inline void port_rate_show(void)
{
	int i, ret;
	unsigned int apr;

	printk("PORT_RATE: \n");
	for(i=0; i<8; i++){
		ret = rtl8651_getAsicPortEgressBandwidth(i, &apr);
		if(ret == SUCCESS)
			//printk("    Port[%d]'s apr is %dMbps\n", i, (apr+1)*1024);
			printk("    Port[%d]'s apr is %dKbps\n", i, apr+1);
	}

	return;
}

static inline void flow_control_config_show(void)
{
	unsigned int flow_control_enable = 0;
	int ret;

	ret = rtl8651_getAsicQueueFlowControlConfigureRegister(0, 0, &flow_control_enable);
	if(ret == SUCCESS){
		if(flow_control_enable == 1){
			printk("    QOS Flow Control is enabled!\n");
		}else{
			printk("    QOS Flow Control is disabled!\n");
		}
	}
}

int32 rtl8651_setAsicPortDefPri(uint32 port, uint32 pri)
{
	uint32 regValue,offset;
	
	if(port>=RTL8651_AGGREGATOR_NUMBER || pri>=8)
		return FAILED;

	offset=(port*2)&(~0x3);
	regValue=READ_MEM32(PVCR0+offset);
	if((port&0x1))
	{
		regValue=  ((pri &0x7) <<(16+12)) | (regValue&~0x70000000);
	}
	else
	{	
		regValue =  ((pri &0x7)<<12) | (regValue &~0x7000);
	}
	WRITE_MEM32(PVCR0+offset,regValue);
	return SUCCESS;
}

int32 rtl8651_getAsicPortDefPri(uint32 port, uint32 *pri) 
{
	uint16 offset;
	offset=(port*2)&(~0x3);
	if(port>=RTL8651_AGGREGATOR_NUMBER || pri == NULL)
		return FAILED;
	if((port&0x1))
	{
		*pri=(((READ_MEM32(PVCR0+offset)>>(16+12))&0x7));		
	}
	else
	{
		*pri=(((READ_MEM32(PVCR0+offset)>>12)&0x7));
	}
	return SUCCESS;
}

void hw_qos_netlink_receive (struct sk_buff *__skb)
{
// to_be_checked !!!
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
#else
#if 1//ndef CONFIG_RTL_8198C
	unsigned int i, j;
	enum QUEUENUM queue_num;
	int ret;
	int pid = 0;
	struct qos_cmd_info_s send_data,recv_data;
	
	pid=rtk_nlrecvmsg(__skb, sizeof(struct qos_cmd_info_s), &recv_data);
	
	if(pid<0)
		return;
	
	switch(recv_data.action)
	{
		case PORT_BASED_PRIORITY_ASSIGN:
			for(i=0; i<8; i++){
				if((1<<i) & recv_data.qos_data.port_based_priority.portmask){
					ret = rtl8651_setAsicPortBasedPriority(i, recv_data.qos_data.port_based_priority.port[i]);
					if(ret == FAILED){
						printk("Port based priority set to PBPCR register failed\n ");
					}

				}
			}
			send_data.action = PORT_BASED_PRIORITY_ASSIGN;
			break;
		case VLAN_BASED_PRIORITY_ASSIGN:
			for(i=0; i<8; i++){
				if((1<<i) & recv_data.qos_data.vlan_based_priority.vlanmask){
					ret = rtl8651_setAsicDot1qAbsolutelyPriority(i, recv_data.qos_data.vlan_based_priority.vlan[i]);
					if(ret == FAILED){
						printk("Vlan based priority set to LPTM8021Q register failed\n ");
					}
				}
			}
			send_data.action = VLAN_BASED_PRIORITY_ASSIGN;
			break;
		case DSCP_BASED_PRIORITY_ASSIGN:
			for(i=0; i<64; i++){
				if(((i<32) && ((1<<i) & recv_data.qos_data.dscp_based_priority.dscpmask1)) ||
				   ((i>=32) && ((1<<(i-32)) & recv_data.qos_data.dscp_based_priority.dscpmask2))){
					ret = rtl8651_setAsicDscpPriority(i, recv_data.qos_data.dscp_based_priority.dscp[i]);
					if(ret == FAILED){
						printk("Dscp based priority set to DSCPCR register failed\n ");
					}
				}
			}
			send_data.action = DSCP_BASED_PRIORITY_ASSIGN;
			break;

		#if defined(CONFIG_RTL_8197F)
		case VID_BASED_PRIORITY_ASSIGN:
			for(i=0; i<8; i++){
				if(recv_data.qos_data.vid_based_priority.vid[i]){
					//panic_printk("vid%d is %d in kernel.\n", i, recv_data.qos_data.vid_based_priority.vid[i]);

					if(!_rtl8651_getVlanTableEntry(recv_data.qos_data.vid_based_priority.vid[i])){
						panic_printk("vlan table don't have the entry of vid 0x%x.\n", recv_data.qos_data.vid_based_priority.vid[i]);
						continue;
					}
					ret = rtl865x_setVlanHp(recv_data.qos_data.vid_based_priority.vid[i], recv_data.qos_data.vid_based_priority.pri[i]);
					if(ret == FAILED){
						panic_printk("Vid based priority set to vlan table failed\n ");
					}
					if(ret == RTL_EINVALIDVLANID){
						panic_printk("Invalid Vid 0x%x\n", recv_data.qos_data.vid_based_priority.vid[i]);
					}
				}
			}
			break;
		#endif

		case QUEUE_NUMBER:
			for(i=0; i<8; i++){
				if((1<<i) & recv_data.qos_data.queue_num.portmask){
					ret = rtl8651_setAsicOutputQueueNumber(i, recv_data.qos_data.queue_num.port[i]);
					if(ret == FAILED){
						printk("Queue number set to QNUMCR register failed\n ");
					}
				}
			}
			send_data.action = QUEUE_NUMBER;
			break;
	
	#if defined(SET_QUEUE_FLOW_CONTROL)
		case QUEUE_FLOW_CONTROL:
			rtl865xC_lockSWCore();
			for(i=0; i<8; i++){
				if((1<<i) & recv_data.qos_data.queue_type.portmask){
					for(j=0; j < MAX_QUEUE_NUM; j++){
						if((1<<j) & recv_data.qos_data.queue_type.queuemask){
							ret = rtl8651_setAsicQueueFlowControlConfigureRegister(i, j, recv_data.qos_data.queue_type.queue[i][j]);
							if(ret == FAILED){
								printk("Queue Flow Control set to Port %d Queue %d register failed\n", i, j);
							}
						}
					}
				}
			}
			rtl865xC_waitForOutputQueueEmpty();
			rtl8651_resetAsicOutputQueue();
			rtl865xC_unLockSWCore();

			send_data.action = QUEUE_FLOW_CONTROL;
			break;
	#endif
		case QUEUE_TYPE_STRICT:
			for(i=0; i<8; i++){
				if((1<<i) & recv_data.qos_data.queue_type.portmask){
					for(j=0; j < MAX_QUEUE_NUM; j++){
						if((1<<j) & recv_data.qos_data.queue_type.queuemask){
							if(recv_data.qos_data.queue_type.queue[i][j] == 255){
									ret = rtl8651_setAsicQueueStrict(i, j, STR_PRIO);
									if(ret == FAILED){
										printk("Queue type STRICT set to WFQWCR0P0 register failed\n ");
									}
								}
						}
					}
				}
			}
			send_data.action = QUEUE_TYPE_STRICT;
			break;
		case QUEUE_TYPE_WEIGHTED:
			for(i=0; i<8; i++){
				if((1<<i) & recv_data.qos_data.queue_type.portmask){
					for(j=0; j < MAX_QUEUE_NUM; j++){
						if((1<<j) & recv_data.qos_data.queue_type.queuemask){
							if((recv_data.qos_data.queue_type.queue[i][j] > 0) &&
					    		    (recv_data.qos_data.queue_type.queue[i][j] != 255)){
								ret = rtl8651_setAsicQueueWeight(i, j, WFQ_PRIO, recv_data.qos_data.queue_type.queue[i][j]);
								if(ret == FAILED){
									printk("Queue type WEIGHTED set to WFQWCR0P0 register failed\n ");
								}
							}
						}
					}
				}
			}
			send_data.action = QUEUE_TYPE_WEIGHTED;
			break;
		case PRIORITY_TO_QID:
			for(i=0; i<8; i++){
				ret = rtl8651_getAsicOutputQueueNumber(i, &queue_num);
				if(ret == SUCCESS){
					for(j=0; j<8; j++)
					{
						if((1<<j) & recv_data.qos_data.sys_priority.prioritymask){
							ret = rtl8651_setAsicPriorityToQIDMappingTable(queue_num, j, recv_data.qos_data.sys_priority.priority[j]);
							if(ret == FAILED){
								printk("Priority to qid set to UPTCMCR register failed\n ");
							}
						}
					}
				}
			}
			send_data.action = PRIORITY_TO_QID;
			break;
		case CPU_PRIORITY_TO_QID:
			for(j=0; j<8; j++)
			{
				if((1<<j) & recv_data.qos_data.cpu_priority.prioritymask){
					ret = rtl8651_setAsicCPUPriorityToQIDMappingTable(CPU, j, recv_data.qos_data.sys_priority.priority[j]);
					if(ret == FAILED){
						printk("CPU Priority to qid set to UPTCMCR register failed\n ");
					}
				}
			}
			send_data.action = CPU_PRIORITY_TO_QID;
			break;
	#if defined(CONFIG_RTL_HW_QOS_SUPPORT)
		case MARK_TO_PRIORITY:
			for(j = 0; j < MAX_MARK_NUM_PER_DEV; j++){
				if((1 << j) & recv_data.qos_data.mark_to_prio.countmask){
					if(recv_data.qos_data.mark_to_prio.markInfo[j].delflag || recv_data.qos_data.mark_to_prio.markInfo[j].mark){
						ret = rtk_cmd_config_mark2priority(recv_data.qos_data.mark_to_prio.netifName, j , &recv_data.qos_data.mark_to_prio.markInfo[j]);
						if(ret == FAILED){
							printk("Netif %s Number %d Mark to Priority set failed\n ", recv_data.qos_data.mark_to_prio.netifName, j);
						}
					}
				}
			}
			
			send_data.action = MARK_TO_PRIORITY;
			break;
	#endif		
		case PRIORITY_DECISION:	
			#if defined(CONFIG_RTL_8197F)
			ret = rtl8651_setAsicPriorityDecision(recv_data.qos_data.pri_decision.decision[0], recv_data.qos_data.pri_decision.decision[1],
										   recv_data.qos_data.pri_decision.decision[2], recv_data.qos_data.pri_decision.decision[3],
										   recv_data.qos_data.pri_decision.decision[4], recv_data.qos_data.pri_decision.decision[5]);
			#else
			ret = rtl8651_setAsicPriorityDecision(recv_data.qos_data.pri_decision.decision[0], recv_data.qos_data.pri_decision.decision[1],
										   recv_data.qos_data.pri_decision.decision[2], recv_data.qos_data.pri_decision.decision[3],
										   recv_data.qos_data.pri_decision.decision[4]);
			#endif
			if(ret == FAILED){
				printk("Priority decision set to QIDDPCR register failed\n ");
			}
			send_data.action = PRIORITY_DECISION;
			break;
		case VLAN_REMARK:
			for(i=0; i<8; i++){
				if((1<<i) & recv_data.qos_data.vlan_remark.portmask){
					for(j=0; j<8; j++){
						if((1<<j) & recv_data.qos_data.vlan_remark.prioritymask){
							ret = rtl8651_setAsicVlanRemark(i, j, recv_data.qos_data.vlan_remark.remark[i][j]);
							if(ret == FAILED){
								printk("Vlan remark set to RMCR1P register failed\n ");
							}
						}
					}
				}
			}
			send_data.action = VLAN_REMARK;
			break;
		case DSCP_REMARK:
			for(i=0; i<8; i++){
				if((1<<i) & recv_data.qos_data.dscp_remark.portmask){
					for(j=0; j<8; j++){
						if((1<<j) & recv_data.qos_data.dscp_remark.prioritymask){
							ret = rtl8651_setAsicDscpRemark(i, j, recv_data.qos_data.dscp_remark.remark[i][j]);
							if(ret == FAILED){
								printk("Dscp remark set to DSCPRM register failed\n ");
							}
						}
					}
				}
			}
			send_data.action = DSCP_REMARK;
			break;
		case QUEUE_RATE:
			for(i=0; i<8; i++){
				if((1<<i) & recv_data.qos_data.queue_rate.portmask){
					for(j=0; j < MAX_QUEUE_NUM; j++){
						if((1<<j) & recv_data.qos_data.queue_rate.queuemask){
							ret = rtl8651_setAsicQueueRate(i, j, recv_data.qos_data.queue_rate.ppr[i][j],
								                                      recv_data.qos_data.queue_rate.burst[i][j],
								                                      recv_data.qos_data.queue_rate.apr[i][j]);
							if(ret == FAILED){
								printk("Queue rate set to P0Q0RGCR register failed\n ");
							}
						}
					}
				}
			}
			send_data.action = QUEUE_RATE;
			break;
		case PORT_RATE:
			for(i=0; i<8; i++){
				if((1<<i) & recv_data.qos_data.port_rate.portmask){
					ret = rtl8651_setAsicPortEgressBandwidth(i, recv_data.qos_data.port_rate.apr[i]);
					if(ret == FAILED){
						printk("Port rate set to WFQRCRP0 register failed\n ");
					}
				}
			}
			send_data.action = PORT_RATE;
			break;
		case PORT_DEFAULT_8021P:
			for(i=0; i<8; i++){
				if((1<<i) & recv_data.qos_data.port_pri.portmask){
					ret = rtl8651_setAsicPortDefPri(i, recv_data.qos_data.port_pri.pri[i]);
					if(ret == FAILED){
						printk("Port default 8021p set to PVCRx register failed\n ");
					}
				}
			}
			send_data.action = PORT_DEFAULT_8021P;
			break;
		case FLOW_CONTROL_ENABLE:
			for(i=0; i<7; i++){
				for(j=0; j < MAX_QUEUE_NUM; j++){
					ret = rtl8651_setAsicQueueFlowControlConfigureRegister(i, j, 1);
					if(ret == FAILED){
						printk("Set Flow Control Enable failed\n ");
					}
				}
			}
			break;
		case FLOW_CONTROL_DISABLE:
			for(i=0; i<7; i++){
				for(j=0; j < MAX_QUEUE_NUM; j++){
					ret = rtl8651_setAsicQueueFlowControlConfigureRegister(i, j, 0);
					if(ret == FAILED){
						printk("Set Flow Control Disable failed\n ");
					}
				}
			}
			break;
		case DISABLE_DSCP_REMARK:
			rtl8651_disableDscpRemark();
			break;
			
		case DISABLE_DOT1P_REMARK:
			rtl8651_disableVlanRemark();
			break;
			
	#if defined(CONFIG_RTL_HW_QOS_SUPPORT)
		case TC_COMMAND_ENABLE:
			tc_hw_qos = 1;
			break;
		
		case TC_COMMAND_DISABLE:
			tc_hw_qos = 0;
			break;	
	#endif
	
		case PORT_BASED_PRIORITY_SHOW:
			port_based_priority_show();
			break;
		case VLAN_BASED_PRIORITY_SHOW:
			vlan_based_priority_show();
			break;
		case DSCP_BASED_PRIORITY_SHOW:
			dscp_based_priority_show();
			break;
		#if defined(CONFIG_RTL_8197F)
		case VID_BASED_PRIORITY_SHOW:
			vid_based_priority_show();
			break;
		#endif
		case QUEUE_NUMBER_SHOW:
			queue_number_show();
			break;
	#if defined(SET_QUEUE_FLOW_CONTROL)
		case QUEUE_FLOW_CONTROL_SHOW:
			queue_flow_control_show();
			break;
	#endif
		case QUEUE_TYPE_STRICT_SHOW:
			queue_type_strict_show();
			break;
		case QUEUE_TYPE_WEIGHTED_SHOW:
			queue_type_weighted_show();
			break;
		case PRIORITY_TO_QID_SHOW:
			priority_to_qid_show();
			break;
		case CPU_PRIORITY_TO_QID_SHOW:
			CPU_priority_to_qid_show();
			break;
		case PRIORITY_DECISION_SHOW:
			priority_decision_show();
			break;
		case VLAN_REMARK_SHOW:
			vlan_remark_show();
			break;
		case DSCP_REMARK_SHOW:
			dscp_remark_show();
			break;
		case QUEUE_RATE_SHOW:
			queue_rate_show();
			break;
		case PORT_RATE_SHOW:
			port_rate_show();
			break;
		case PORT_DEFAULT_8021P_SHOW:
			port_default_8021p_show();
			break;
		case FLOW_CONTROL_CONFIGURATION_SHOW:
			flow_control_config_show();
			break;
#if defined(CONFIG_RTL_8198C) || defined(CONFIG_RTL_8197F)
		case DSCPV6_BASED_PRIORITY_ASSIGN:
			for(i=0; i<64; i++){
				if(((i<32) && ((1<<i) & recv_data.qos_data.dscpv6_based_priority.dscpv6mask1)) ||
				   ((i>=32) && ((1<<(i-32)) & recv_data.qos_data.dscpv6_based_priority.dscpv6mask2))){
					ret = rtl819x_setAsicv6DscpPriority(i, recv_data.qos_data.dscpv6_based_priority.dscpv6[i]);
					if(ret == FAILED){
						printk("ipv6 Dscp based priority set to V6DSCPCR register failed\n ");
					}
				}
			}
			send_data.action = DSCPV6_BASED_PRIORITY_ASSIGN;
			break;
		case DSCPV6_PRIORITY_DECISION:
			ret = rtl819x_setAsicv6DscpPriorityDecision(recv_data.qos_data.dscpv6_pri_decision.dscpv6_decision);
			if(ret == FAILED){
				printk("ipv6 dscp based Priority decision set to IPV6CR1 register failed\n ");
			}
			send_data.action = DSCPV6_PRIORITY_DECISION;
			break;
		case DSCPV6_BASED_PRIORITY_SHOW:
			dscpv6_based_priority_show();
			break;
		case DSCPV6_PRIORITY_DECISION_SHOW:
			dscpv6_priority_decision_show();
			break;
#endif
		default:
			break;
	}

	rtk_nlsendmsg(pid, hw_qos_sk, sizeof(struct qos_cmd_info_s), &send_data);
#endif	
#endif
	return;

}


int hw_qos_init_netlink(void)
{
// to_be_checked !!!
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
  	hw_qos_sk = netlink_kernel_create(&init_net, NETLINK_RTK_HW_QOS, 0, hw_qos_netlink_receive, NULL, THIS_MODULE);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
	struct netlink_kernel_cfg cfg;
	memset(&cfg,0,sizeof(cfg));
	cfg.cb_mutex =NULL;
	cfg.groups =0;
	cfg.input = hw_qos_netlink_receive;
	hw_qos_sk = netlink_kernel_create(&init_net, NETLINK_RTK_HW_QOS,&cfg);
#endif
  	if (!hw_qos_sk) {
    		printk(KERN_ERR "Netlink[Kernel] Cannot create netlink socket for hw qos config.\n");
    		return -EIO;
  	}
  	printk("Netlink[Kernel] create socket for hw qos config ok.\n");
	
  	return 0;
}






