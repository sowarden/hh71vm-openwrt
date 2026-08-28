#ifndef __HAL8812FE_DEF_H__
#define __HAL8812FE_DEF_H__

/*++
Copyright (c) Realtek Semiconductor Corp. All rights reserved.

Module Name:
	Hal8812FEDef.h
	
Abstract:
	Defined HAL 8812FE data structure & Define
	    
Major Change History:
	When       Who               What
	---------- ---------------   -------------------------------
	2015-06-25 Eric             Create.	
--*/


/*enum rt_status
InitPON8812FE(
    IN  HAL_PADAPTER    Adapter,
    IN  u4Byte          ClkSel        
);

enum rt_status
StopHW8812FE(
    IN  HAL_PADAPTER    Adapter
);


enum rt_status
ResetHWForSurprise8812FE(
    IN  HAL_PADAPTER Adapter
);
*/

enum rt_status	
hal_Associate_8812FE(
	struct rtl8192cd_priv *priv,
	BOOLEAN             IsDefaultAdapter
);

#if (BEAMFORMING_SUPPORT == 1)
VOID
SetBeamformInit8812F(
	struct rtl8192cd_priv *priv
	);

VOID
SetBeamformEnter8812F(
	struct rtl8192cd_priv *priv,
	IN u1Byte				BFerBFeeIdx
	);

VOID
SetBeamformLeave8812F(
	struct rtl8192cd_priv *priv,
	IN u1Byte				Idx
	);

VOID
SetBeamformStatus8812F(
	struct rtl8192cd_priv *priv,
	IN u1Byte				Idx
	);

void
Beamforming_dym_disable_bf_coeff_8812F(
	struct rtl8192cd_priv *priv,
	u1Byte				   enable
);


#ifdef CONFIG_VERIWAVE_MU_CHECK	
void Beamforming_set_csi_data(
	struct rtl8192cd_priv *priv, 
	int idx, 
	int reg_idx);
#endif

void Beamforming_CSIRate_8812F(
	struct rtl8192cd_priv *priv
);


VOID Beamforming_NDPARate_8812F(
	struct rtl8192cd_priv *priv,
	BOOLEAN		Mode,
	u1Byte		BW,
	u1Byte		Rate
	);
	

#if (MU_BEAMFORMING_SUPPORT == 1)
VOID
HalTxbf8812F_ConfigGtab(
	struct rtl8192cd_priv *priv
	);
#endif

VOID
C2HTxBeamformingHandler_8812F(
	struct rtl8192cd_priv *priv,
		pu1Byte			CmdBuf,
		u1Byte			CmdLen
);
#endif 


#endif  //__HAL8812FE_DEF_H__

