/**
  *  @file 8192cd_h2d_tx.c of H2D Tx process from HostCPU to Data-CPU  (Use 8814A to simulation)
  *  @brief Handle Tx-processes in HostCPU
  *
  *  Packet Offload Engine will help RTK WiFi Chip to decrease host platform CPU utilization.
  *  This functon will handle Tx-processes in Host-CPU
  *
  *  Copyright (c) 2015 Realtek Semiconductor Corp.
  *
  *  @author Peter Yu
  *  @date 2015/10/21
 **/

#define _8192CD_TX_H2D_C_

#ifdef __KERNEL__
#include <linux/if_arp.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/tcp.h>
#endif

#include "./8192cd_cfg.h"
#include "./8192cd.h"
#include "./8192cd_hw.h"
#include "./8192cd_headers.h"
#include "./8192cd_debug.h"

#if !defined(__KERNEL__) && !defined(__ECOS)
#include "./sys-support.h"
#endif

#ifdef RTL8192CD_VARIABLE_USED_DMEM
#include "./8192cd_dmem.h"
#endif

#if defined(CONFIG_RTL_WAPI_SUPPORT)
#include "wapiCrypto.h"
#endif
#if defined (CONFIG_RTL_VLAN_8021Q) || defined (CONFIG_PUMA_VLAN_8021Q)
#include <linux/if_vlan.h>
#endif

#ifdef CONFIG_RTL867X_VLAN_MAPPING
#include "../../re_vlan.h"
#endif

#ifdef PERF_DUMP
#include "romeperf.h"
#endif

#include "HalHeader/HalComTXDesc.h"
#include "WlanHAL/HalPrecomp.h"

extern int get_tx_sc_free_entry(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *hdr, int aggre_en);


void dump_txcfg(struct rtl8192cd_priv *priv, struct tx_insn *txcfg)
{
    int i=0;

    printk("==== dump_txcfg start ==== \n");
    if (txcfg) {

        printk("phdr:%p, q_num:%d, pframe:%p, hdr_len:%d\n",
            txcfg->phdr, txcfg->q_num, txcfg->pframe, txcfg->hdr_len);

        printk("iv:%d, llc:%d, icv:0x%d, mic:%d\n",
            txcfg->iv, txcfg->llc, txcfg->icv, txcfg->mic);

        printk("fr_len:%d, fr_type:%d, frg_num:%d, need_ack:%d\n",
            txcfg->fr_len, txcfg->fr_type, txcfg->frg_num, txcfg->need_ack);

        printk("rts_thrshld:%d, frag_thrshld:%d, privacy:%d, aggre_en:%d\n",
            txcfg->rts_thrshld, txcfg->frag_thrshld, txcfg->privacy, txcfg->aggre_en);

        printk("one_txdesc:%d, pstat:%p, fixed_rate:%d, tx_rate:0x%02X, lowest_tx_rate:0x%02X\n",
            txcfg->one_txdesc, txcfg->pstat, txcfg->fixed_rate, txcfg->tx_rate, txcfg->lowest_tx_rate);

        printk("retry:%d, h2d_type:%d, h2d_tid:%d\n",
            txcfg->retry, txcfg->h2d_type, txcfg->h2d_tid);
    
    }
    printk("==== dump_txcfg End ==== \n");
}

/** @brief -- fill content of info_station
  *
  * @param param_out None
  * @param param_in  priv:  which Wi-Fi interface.
  *                  pstat: tx information
  *                  PINFO_INTERFACE: which type
  * @return 0: OK, 1: others
 **/
static void rtl88XX_fill_h2d_info_if(struct rtl8192cd_priv *priv, PINFO_INTERFACE pInfoIf)
{
	/* DWORD0/1 */
    pInfoIf->if_id = priv->if_id;
#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
    pInfoIf->vap_init_seq = priv->vap_init_seq;
#endif
    memcpy(pInfoIf->mac_addr, priv->pmib->dot11OperationEntry.hwaddr, 6);

	/* DWORD2 */
    pInfoIf->band = priv->pmib->dot11BssType.net_work_type;
    pInfoIf->oper_mode = OPMODE;
    pInfoIf->auth_type = priv->pmib->dot1180211AuthEntry.dot11AuthAlgrthm;
    pInfoIf->enc_type = priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm;

	/* DWORD3 */
    pInfoIf->rts_thrshld = cpu_to_le16(priv->pmib->dot11OperationEntry.dot11RTSThreshold);
    pInfoIf->frag_thrshold = cpu_to_le16(priv->pmib->dot11OperationEntry.dot11FragmentationThreshold);

	/* DWORD4 */
    pInfoIf->ht_protection = priv->ht_protection;
    pInfoIf->erp_protection = priv->pmib->dot11ErpInfo.protection;
    pInfoIf->cts2self = priv->pmib->dot11ErpInfo.ctsToSelf;
    pInfoIf->tx_no_ack = priv->pmib->dot11nConfigEntry.dot11nTxNoAck;
    pInfoIf->shortpreamble = priv->pmib->dot11RFEntry.shortpreamble;
    pInfoIf->current_bw = priv->pshare->CurrentChannelBW;
    pInfoIf->long_retry = priv->pmib->dot11OperationEntry.dot11LongRetryLimit;
    pInfoIf->short_retry = priv->pmib->dot11OperationEntry.dot11ShortRetryLimit;
    pInfoIf->longpreamble_sta_num = priv->pmib->dot11ErpInfo.longPreambleStaNum;
    pInfoIf->rsvd = 0;

	/* DWORD5 */
    pInfoIf->ampdu = AMPDU_ENABLE;
    pInfoIf->amsdu = AMSDU_ENABLE;

 	/* DWORD6 */
#ifdef CONFIG_DYNAMIC_SELF_MACID
    pInfoIf->self_macid = cpu_to_le16(CONFIG_MACID_NUM + priv->if_id);
#endif
}

/** @brief -- fill content of info_station
  *
  * @param param_out None
  * @param param_in  priv:  which Wi-Fi interface.
  *                  pstat: tx information
  *                  PINFO_STATION: the information of info_station
  * @return 0: OK, 1: others
 **/
void rtl88XX_fill_h2d_info_sta(struct rtl8192cd_priv *priv, struct stat_info *pstat, PINFO_STATION pInfoSta)
{
 	/* DWORD0 */
    pInfoSta->aid = cpu_to_le16(pstat->cmn_info.aid);
    pInfoSta->macid = cpu_to_le16(REMAP_AID(pstat));

 	/* DWORD1 */
    pInfoSta->if_id = priv->if_id;
    pInfoSta->auth_type = pstat->AuthAlgrthm;
	pInfoSta->max_agg_num = cpu_to_le16(pstat->maxAggNum);

 	/* DWORD2 */
    pInfoSta->state = cpu_to_le32(pstat->state);

 	/* DWORD3 */
#if (BEAMFORMING_SUPPORT == 1)
    pInfoSta->p_aid = cpu_to_le32(pstat->p_aid);
#else
    pInfoSta->p_aid = 0;
#endif

 	/* DWORD4 */
#if (BEAMFORMING_SUPPORT == 1)
    pInfoSta->g_id = pstat->g_id;
#else
    pInfoSta->g_id = 0;
#endif
    pInfoSta->aggre_type  = pstat->aggre_mthd;
	pInfoSta->no_rts = pstat->no_rts;
	pInfoSta->cts2self = pstat->useCts2self;
	//pInfoSta->leave= pstat->leave;
	pInfoSta->keyid= pstat->keyid;

 	/* DWORD5/6 */
	memcpy(pInfoSta->mac_addr, pstat->cmn_info.mac_addr, 6);
	//pInfoSta->vht_cap_len = 0;
	//pInfoSta->ampdu_density = 0;

 	/* DWORD7 */
    pInfoSta->enc_type = get_sta_encrypt_algthm(priv, pstat);
	pInfoSta->qos_enabled = pstat->QosEnabled;
}

/** @brief -- fill content of H2D command to data-CPU.
  *
  * @param param_out None
  * @param param_in  priv:  which Wi-Fi interface.
  *                  pstat: tx information
  *                  h2d_type: which type
  * @return 0: OK, 1: others
 **/
void rtl88XX_signin_h2d_cmd(
		struct rtl8192cd_priv *priv,
		void *data,
		enum h2d_cmd_type cmd_type)
{
    unsigned char *pbuf = NULL;
    unsigned long flags = 0;  //lock flags
    struct stat_info *pstat = NULL;
    struct cil_pkt_t *cil_pkt;
    DECLARE_TXINSN(txinsn);

    SAVE_INT_AND_CLI(flags);
    SMP_LOCK_XMIT(flags);
    pbuf = txinsn.pframe = get_mgtbuf_from_poll(priv);
    if (pbuf == NULL) {
        printk("%s: buf is not enough\n", __func__);
        goto fail;
    }
    cil_pkt = (struct cil_pkt_t *)pbuf;

    txinsn.phdr = get_wlanhdr_from_poll(priv);
    if (txinsn.phdr == NULL){
        printk("%s: wlanhdr is not enough\n", __func__);
		goto fail;
	}

	switch (cmd_type) {
	case h2d_intf_add:
	case h2d_intf_update:
	case h2d_intf_remove:
	{
		struct datacpu_info_if *infoInterface;
		struct cil_h2d_pe_update_sil_t *h2d_hdr;
		infoInterface = (struct datacpu_info_if *)(pbuf + CIL_HDR_LEN + CIL_H2D_HDR_LEN);
		h2d_hdr = (struct cil_h2d_pe_update_sil_t *)(pbuf + CIL_HDR_LEN);
		memset(infoInterface, 0, sizeof(*infoInterface));
		rtl88XX_fill_h2d_info_if(priv, infoInterface);

		if (cmd_type == h2d_intf_remove)
			infoInterface->if_id = _SIL_NULL_ID;

	    CIL_PKT_SET_CTRL(pbuf,
				CIL_PKT_TYPE_H2D_CMD,
				CIL_H2D_C_PE,
				CIL_H2D_I_PE_UPDATE_SIL,
				sizeof(*infoInterface),
				0, 0, NULL, NULL);

		h2d_hdr->word0 = OFFSET_VALUE(priv->if_id, CIL_H2D_PE_UPDATE_SIL_SID) |
						OFFSET_VALUE(CIL_H2D_PE_OPCODE_UPDATE, CIL_H2D_PE_UPDATE_SIL_OPCODE);

		/* change endian */
		h2d_hdr->word0 = cpu_to_le32(h2d_hdr->word0);
		h2d_hdr->word1 = cpu_to_le32(h2d_hdr->word1);

		txinsn.fr_len = CIL_HDR_LEN + CIL_H2D_HDR_LEN + sizeof(*infoInterface);
		//mem_dump("intf_add_update_rm", pbuf, txinsn.fr_len);
	}
	    break;

	case h2d_sta_add:
	case h2d_sta_update:
	{
		struct datacpu_info_sta *infoSta;
		struct cil_h2d_pe_update_macid_t *h2d_hdr;
		pstat = (struct stat_info *)data;
		infoSta = (struct datacpu_info_sta *)(pbuf + CIL_HDR_LEN + CIL_H2D_HDR_LEN);
		h2d_hdr = (struct cil_h2d_pe_update_macid_t *)(pbuf + CIL_HDR_LEN);
		memset(infoSta, 0, sizeof(*infoSta));
		rtl88XX_fill_h2d_info_sta(priv, pstat, infoSta);

		CIL_PKT_SET_CTRL(pbuf,
				CIL_PKT_TYPE_H2D_CMD,
				CIL_H2D_C_PE,
				CIL_H2D_I_PE_UPDATE_MACID,
				sizeof(*infoSta),
				0, 0, NULL, NULL);
		h2d_hdr->word0 = OFFSET_VALUE(priv->if_id, CIL_H2D_PE_UPDATE_MACID_SID) |
						OFFSET_VALUE(CIL_H2D_PE_OPCODE_UPDATE, CIL_H2D_PE_UPDATE_MACID_OPCODE);

		/* change endian */
		h2d_hdr->word0 = cpu_to_le32(h2d_hdr->word0);
		h2d_hdr->word1 = cpu_to_le32(h2d_hdr->word1);

		txinsn.fr_len = CIL_HDR_LEN + CIL_H2D_HDR_LEN + sizeof(*infoSta);
		//mem_dump("sta_add_update", pbuf, txinsn.fr_len);
	}
		break;

	case h2d_sta_remove:
	{
		struct datacpu_info_sta *infoSta;
		struct cil_h2d_pe_update_macid_t *h2d_hdr;
		pstat = (struct stat_info *)data;
		infoSta = (struct datacpu_info_sta *)(pbuf + CIL_HDR_LEN + CIL_H2D_HDR_LEN);
		h2d_hdr = (struct cil_h2d_pe_update_macid_t *)(pbuf + CIL_HDR_LEN);
		memset(infoSta, 0, sizeof(*infoSta));
		rtl88XX_fill_h2d_info_sta(priv, pstat, infoSta);

		CIL_PKT_SET_CTRL(pbuf,
				CIL_PKT_TYPE_H2D_CMD,
				CIL_H2D_C_PE,
				CIL_H2D_I_PE_REMOVE_MACID,
				sizeof(*infoSta),
				0, 0, NULL, NULL);
		h2d_hdr->word0 = OFFSET_VALUE(priv->if_id, CIL_H2D_PE_UPDATE_MACID_SID) |
						OFFSET_VALUE(CIL_H2D_PE_OPCODE_DEL, CIL_H2D_PE_UPDATE_MACID_OPCODE);

		/* change endian */
		h2d_hdr->word0 = cpu_to_le32(h2d_hdr->word0);
		h2d_hdr->word1 = cpu_to_le32(h2d_hdr->word1);

		txinsn.fr_len = CIL_HDR_LEN + CIL_H2D_HDR_LEN + sizeof(*infoSta);
		//mem_dump("sta_rm", pbuf, txinsn.fr_len);
	}
		break;

	case h2d_sta_remove_all:
	{
		struct cil_h2d_pe_update_macid_t *h2d_hdr;
		h2d_hdr = (struct cil_h2d_pe_update_macid_t *)(pbuf + CIL_HDR_LEN);

		CIL_PKT_SET_CTRL(pbuf,
				CIL_PKT_TYPE_H2D_CMD,
				CIL_H2D_C_PE,
				CIL_H2D_I_PE_REMOVE_ALL_MACID,
				0,
				0, 0, NULL, NULL);
		h2d_hdr->word0 = OFFSET_VALUE(priv->if_id, CIL_H2D_PE_UPDATE_MACID_SID) |
						OFFSET_VALUE(CIL_H2D_PE_OPCODE_CLEAN, CIL_H2D_PE_UPDATE_MACID_OPCODE);

		/* change endian */
		h2d_hdr->word0 = cpu_to_le32(h2d_hdr->word0);
		h2d_hdr->word1 = cpu_to_le32(h2d_hdr->word1);

		txinsn.fr_len = CIL_HDR_LEN + CIL_H2D_HDR_LEN;
		//mem_dump("sta_rmall", pbuf, txinsn.fr_len);
	}
		break;

	case h2d_sys_config:
	{
		struct cil_h2d_sys_config_t *sys_config;
		char cmd, *pparam;
		int val32;

		sys_config = (struct cil_h2d_sys_config_t *)&pbuf[CIL_HDR_LEN];
		memset(sys_config, 0, sizeof(*sys_config));

		/* config type */
		sys_config->cil_hostVer = CIL_VERSION;
		sys_config->conf_mask   = cpu_to_le32(_H2D_SYS_CONF_FWLOG);

		/* parse FWLOG config */
		pparam = (char *)data;
		if (pparam) {
			/* parse category mask */
			if (pparam[0]=='0' && pparam[1]=='x')
				val32 = _atoi(pparam+2, 16);
			else
				val32 = _atoi(pparam, 10);
			if (val32)
				sys_config->fwlog.en_cat_mask = val32;
			/* parse timestamp */
			if (strstr(pparam, "ts"))
				sys_config->fwlog.en_ts = 1;
			else
				sys_config->fwlog.en_ts = 0;
			if (strstr(pparam, "pm"))
				sys_config->fwlog.en_pm = 1;
			else
				sys_config->fwlog.en_pm = 0;			
			/* change endian */
			sys_config->fwlog.en_cat_mask = cpu_to_le32(sys_config->fwlog.en_cat_mask);
			sys_config->fwlog.en_ts       = cpu_to_le32(sys_config->fwlog.en_ts);
			sys_config->fwlog.en_pm       = cpu_to_le32(sys_config->fwlog.en_pm);
		}

		printk("H2D: mBSP SYS_CONFIG: mask=%08X ts=%d\n",
				sys_config->fwlog.en_cat_mask, sys_config->fwlog.en_ts);

		CIL_PKT_SET_CTRL(pbuf,
				CIL_PKT_TYPE_H2D_CMD,
				CIL_H2D_C_SYS,
				CIL_H2D_I_SYS_CONFIG,
				sizeof(struct cil_h2d_sys_config_t),
				0, 0, NULL, NULL);
		txinsn.fr_len = CIL_HDR_LEN + sizeof(struct cil_h2d_sys_config_t);
		//mem_dump("sys_config", pbuf, txinsn.fr_len);
	}
		break;

	case h2d_sys_start_fw:
	{
		struct cil_h2d_sys_config_t *sys_config;

		sys_config = (struct cil_h2d_sys_config_t *)&pbuf[CIL_HDR_LEN];
		memset(sys_config, 0, sizeof(*sys_config));

		sys_config->cil_hostVer = CIL_VERSION;
		sys_config->conf_mask           = cpu_to_le32(_H2D_SYS_CONF_INIT);
		sys_config->init.sys_feature    = cpu_to_le32(0xfbadbeef);
		sys_config->init.pe_ofldFunc    = cpu_to_le32(0xd1f0d1f0);
		sys_config->init.phydm_regd[0] = 0x54;    /* 'T' */
		sys_config->init.phydm_regd[1] = 0x57;    /* 'W' */
		sys_config->init.phydm_ofldFunc = cpu_to_le16(0x0f1d);

		CIL_PKT_SET_CTRL(pbuf,
				CIL_PKT_TYPE_H2D_CMD,
				CIL_H2D_C_SYS,
				CIL_H2D_I_SYS_CONFIG,
				sizeof(struct cil_h2d_sys_config_t),
				0, 0, NULL, NULL);
		txinsn.fr_len = CIL_HDR_LEN + sizeof(struct cil_h2d_sys_config_t);
		//mem_dump("start_fw", pbuf, txinsn.fr_len);
	}
		break;

	case h2d_sys_stop_fw:
	{
		struct cil_h2d_sys_config_t *sys_config;

		sys_config = (struct cil_h2d_sys_config_t *)&pbuf[CIL_HDR_LEN];
		memset(sys_config, 0, sizeof(*sys_config));

		sys_config->cil_hostVer = CIL_VERSION;
		sys_config->conf_mask    = cpu_to_le32(_H2D_SYS_CONF_DEINIT);
		sys_config->deinit.dummy = cpu_to_le32(0xdead9527);

		CIL_PKT_SET_CTRL(pbuf,
				CIL_PKT_TYPE_H2D_CMD,
				CIL_H2D_C_SYS,
				CIL_H2D_I_SYS_CONFIG,
				sizeof(struct cil_h2d_sys_config_t),
				1, 0, NULL/*__stop_fw_acked*/, NULL);
		txinsn.fr_len = CIL_HDR_LEN + sizeof(struct cil_h2d_sys_config_t);
		//mem_dump("stop_fw", pbuf, txinsn.fr_len);
	}
		break;

	case h2d_pe_debug:
	{
		struct cil_h2d_pe_dbgcmd_t *sys_dbgcmd;
		unsigned char cmd, param=0, *pparam;
		unsigned char sync=0;

		sys_dbgcmd = (struct cil_h2d_pe_dbgcmd_t *)&pbuf[CIL_HDR_LEN];
		memset(sys_dbgcmd, 0, sizeof(*sys_dbgcmd));

		if (strstr(data, "intf")) {
			cmd = CIL_H2D_PE_DBGCMD_DUMP_IF;
			printk("H2D: dump interface\n");
		} else if (strstr(data, "sta")) {
			cmd = CIL_H2D_PE_DBGCMD_DUMP_STA;
			pparam = strstr(data, ",");
			if (pparam)
				param = _atoi(pparam+1, 10);
			printk("H2D: dump station %d\n", param);
		} else if (strstr(data, "macid")) {
			pparam = strstr(data, ",");
			if (pparam)
				param = _atoi(pparam+1, 10);
			cmd = CIL_H2D_PE_DBGCMD_DUMP_MACID;
			printk("H2D: dump macid %d\n", param);
		} else if (strstr(data, "self")) {
			cmd = CIL_H2D_PE_DBGCMD_DUMP_SMACID;
			printk("H2D: dump self-macid\n");
		} else if (strstr(data, "ring")) {
			cmd = CIL_H2D_PE_DBGCMD_DUMP_RING;
			pparam = strstr(data, ",");
			if (pparam) /* 1: HCI, 2: AXI */
				param = _atoi(pparam+1, 10);
			printk("H2D: dump ring, opt=%d\n", param);
		} else if (strstr(data, "dbglvl")) {
			cmd = CIL_H2D_PE_DBGCMD_GDBGLVL;
			pparam = strstr(data, ",");
			if (pparam) /* debug level */
				param = _atoi(pparam+1, 10);
			printk("H2D: set debug level to %d\n", param);
		}else if (strstr(data, "bcn")) {
        	cmd = CIL_H2D_PE_DBGCMD_BCN_MODE;
        	pparam = strstr(data, ",");
        	if (pparam)
        	    param = _atoi(pparam+1, 10);
       	 	printk("H2D: set beacon mode to %d\n", param);			
		} else {
			DEBUG_ERR("Error h2d commmand\n");
			goto fail;
		}

		/* cil command header */
		CIL_PKT_SET_CTRL(pbuf,
				CIL_PKT_TYPE_H2D_CMD,   /* Type */
				CIL_H2D_C_PE,           /* Category */
				CIL_H2D_I_PE_DEBUG,     /* ID */
				0,                      /* Length */
				sync,                   /* Sync */
				0,                      /* SyncDone */
				NULL,
				NULL);

		/* sub-command header */
		sys_dbgcmd->word0 = OFFSET_VALUE(cmd, CIL_H2D_PE_DBGCMD_TYPE) |
							OFFSET_VALUE(priv->if_id, CIL_H2D_PE_DBGCMD_SID);
		sys_dbgcmd->word1 = OFFSET_VALUE(param, CIL_H2D_PE_DBGCMD_PARAM);

		/* change endian */
		sys_dbgcmd->word0 = cpu_to_le32(sys_dbgcmd->word0);
		sys_dbgcmd->word1 = cpu_to_le32(sys_dbgcmd->word1);

		txinsn.fr_len = CIL_HDR_LEN + CIL_H2D_HDR_LEN;
		//mem_dump("pe_debug", pbuf, txinsn.fr_len);
	}
		break;

	default:
		DEBUG_ERR("Error h2d commmand:%d\n", cmd_type);
		goto fail;
	}

    txinsn.fr_type = _PRE_ALLOCMEM_;
	txinsn.pstat = pstat;
    txinsn.aggre_en = 0;
    txinsn.hdr_len = 0;//sizeof(H2D_TXPKT_INFO);
    txinsn.h2d_type = TXPKTINFO_TYPE_H2D;
    txinsn.q_num = CMD_QUEUE_V2;

    // Check if we need to reclaim TX-ring before processing TX
	if (getAvailableTXBD(priv, txinsn.q_num) < 4) {
	   rtl8192cd_tx_queueDsr(priv, txinsn.q_num);
	   if (getAvailableTXBD(priv, txinsn.q_num) < 4){
			DEBUG_ERR("[%s] No AvailableTXBD!!! \n", __func__);
			priv->ext_stats.tx_cmd_queue_drops++;
	        goto fail;
	   }
	}

	/* add packet stamp */
    CIL_PKT_STAMP(pbuf, priv->pshare->cil_pkt_id_tx);

    /* change endian */
    cil_pkt->ctrl0 = cpu_to_le32(cil_pkt->ctrl0);
    cil_pkt->ctrl1 = cpu_to_le32(cil_pkt->ctrl1);

	if ((rtl88XX_signin_offload_htxdesc(priv, &txinsn)) != SUCCESS){
	    printk("%s rtl88XX_signin_offload_htxdesc failed \n", __func__);
		goto fail;
	}

    if (++priv->pshare->cil_pkt_id_tx > CIL_MAX_PKT_ID)
        priv->pshare->cil_pkt_id_tx = 0;
    SMP_UNLOCK_XMIT(flags);
    RESTORE_INT(flags);
    return;

fail:
	if (txinsn.pframe)
		release_mgtbuf_to_poll(priv, txinsn.pframe);
	if (txinsn.phdr)
		release_wlanhdr_to_poll(priv, (UINT8 *)(txinsn.phdr));
    SMP_UNLOCK_XMIT(flags);
    RESTORE_INT(flags);
}

/** @brief -- fill content of H2D command to data-CPU.
  *
  * @param param_out None
  * @param param_in  priv:  which Wi-Fi interface.
  *                  pstat: tx information
  *                  h2d_type: which type
  * @return 0: OK, 1: others
 **/
void rtl88XX_signin_bcn(struct rtl8192cd_priv *priv, unsigned int *beaconbuf, unsigned int frlen)
{
    DECLARE_TXINSN(txinsn);
    unsigned char *pbuf = NULL;
	struct cil_h2d_wmac_ap_bcn_t *h2d_hdr;
    struct cil_pkt_t *cil_pkt;

    txinsn.phdr = get_wlanhdr_from_poll(priv);
    if (txinsn.phdr == NULL){
        printk("%s: wlanhdr is not enough\n", __func__);
		goto fail;
	}
    pbuf = txinsn.pframe = priv->h2d_header;
    /* pbuf: h2d_header[4] + beaconbuf[MAX_WSC_IE_LEN] */ 
    cil_pkt = (struct cil_pkt_t *)pbuf;

    /* fill CIL CTRL header */
    CIL_PKT_SET_CTRL(pbuf,
            CIL_PKT_TYPE_H2D_CMD,
            CIL_H2D_C_WMAC_AP,
            CIL_H2D_I_WMAC_AP_BCN,
            0,
            0, 0, NULL, NULL);

    /* fill H2D AP_BCN header */
    h2d_hdr = (struct cil_h2d_wmac_ap_bcn_t *)(&pbuf[CIL_HDR_LEN]);
    if (IS_ROOT_INTERFACE(priv))
        h2d_hdr->word0 = OFFSET_VALUE(0, CIL_H2D_WMAC_AP_BCN_SID);
#if defined(MBSSID)
    else
        h2d_hdr->word0 = OFFSET_VALUE(priv->if_id, CIL_H2D_WMAC_AP_BCN_SID);
#endif

    txinsn.fr_type = _RESERVED_FRAME_TYPE_; //do not need to free
    txinsn.fr_len = frlen + 16; // add 4-dword h2d header
	txinsn.pstat = NULL;
    txinsn.aggre_en = 0;
    txinsn.hdr_len = 0;
    txinsn.h2d_type = TXPKTINFO_TYPE_H2D;
    txinsn.q_num = CMD_QUEUE_V2;

    // Check if we need to reclaim TX-ring before processing TX
	if (getAvailableTXBD(priv, txinsn.q_num) < 4) {
	   rtl8192cd_tx_queueDsr(priv, txinsn.q_num);
	   if (getAvailableTXBD(priv, txinsn.q_num) < 4){
			DEBUG_ERR("[%s] No AvailableTXBD!!! \n", __func__);
			priv->ext_stats.tx_cmd_queue_drops++;
	        goto fail;
	   }
	}

    CIL_PKT_STAMP(pbuf, priv->pshare->cil_pkt_id_tx);

    cil_pkt->ctrl0 = cpu_to_le32(cil_pkt->ctrl0);
    cil_pkt->ctrl1 = cpu_to_le32(cil_pkt->ctrl1);
    h2d_hdr->word0 = cpu_to_le32(h2d_hdr->word0);
    h2d_hdr->word1 = cpu_to_le32(h2d_hdr->word1);

	if ((rtl88XX_signin_offload_htxdesc(priv, &txinsn)) != SUCCESS){
	    printk("%s rtl88XX_signin_offload_htxdesc failed \n", __func__);
		goto fail;
	}

    if (++priv->pshare->cil_pkt_id_tx > CIL_MAX_PKT_ID)
        priv->pshare->cil_pkt_id_tx = 0;
    return;

fail:
	if (txinsn.phdr)
		release_wlanhdr_to_poll(priv, (UINT8 *)(txinsn.phdr));

}

