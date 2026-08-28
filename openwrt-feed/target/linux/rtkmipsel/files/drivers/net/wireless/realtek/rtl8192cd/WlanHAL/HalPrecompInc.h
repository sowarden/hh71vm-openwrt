#ifndef __INC_PRECOMPINC_H
#define __INC_PRECOMPINC_H

#if !defined(__ECOS) && !defined(CPTCFG_CFG80211_MODULE)
//HAL Shared with Driver
#include "StatusCode.h"
#include "HalDbgCmd.h"

#ifdef CONFIG_WLAN_MACHAL_API
#include "HalMacAPI.h"
#endif

//MAC function support
#include "HalMacFunc.h"

//Prototype
#include "HalDef.h"

//MAC Header provided by SD1 HWSD
#include "HalHWCfg.h"
#include "HalComTXDesc.h"
#include "HalComRXDesc.h"
//#include "HalComBit.h"
//#include "HalComReg.h"
#include "HalComPhyBit.h"
#include "HalComPhyReg.h"
//#include "HalRxDescAp.h"
//#include "HalTxDescAp.h"
#include "halmac_api.h"

//#include "halmac_reg2.h"
//#include "halmac_bit2.h"
//#include "halmac_rx_desc_ap.h"
#include "halmac_tx_desc_ap.h"




//Instance
#include "HalCommon.h"

 


#if IS_RTL88XX_GENERATION
#include "Hal88XXPwrSeqCmd.h"
#include "Hal88XXReg.h"
#include "Hal88XXDesc.h"
#include "Hal88XXTxDesc.h"
#include "Hal88XXRxDesc.h"
#include "Hal88XXFirmware.h"
#include "Hal88XXIsr.h"
#include "Hal88XXDebug.h"
#include "Hal88XXPhyCfg.h"
#include "Hal88XXDM.h"

#if IS_RTL8881A_SERIES
#include "Hal8881APwrSeqCmd.h"
#include "Hal8881ADef.h"
#include "Hal8881APhyCfg.h"
#endif

#if IS_RTL8192E_SERIES
#include "Hal8192EPwrSeqCmd.h"
#include "Hal8192EDef.h"
#include "Hal8192EPhyCfg.h"
#endif

#if IS_RTL8197F_SERIES
#include "RTL88XX/RTL8197F/Hal8197FPwrSeqCmd.h"
#include "RTL88XX/RTL8197F/Hal8197FDef.h"
#include "RTL88XX/RTL8197F/Hal8197FPhyCfg.h"
#include "RTL88XX/RTL8197F/Hal8197FCfg.h"

#endif //#if IS_RTL8197F_SERIES

#if IS_RTL8198F_SERIES
#include "RTL88XX/RTL8198F/Hal8198FPwrSeqCmd.h"
#include "RTL88XX/RTL8198F/Hal8198FDef.h"
#include "RTL88XX/RTL8198F/Hal8198FCfg.h"
#include "RTL88XX/RTL8198F/Hal8198FPhyCfg.h"
#include "RTL88XX/RTL8198F/Hal8198FFirmware.h"
#endif //#if IS_RTL8198F_SERIES

#if IS_RTL8822B_SERIES
#include "RTL88XX/RTL8822B/Hal8822BPwrSeqCmd.h"
#include "RTL88XX/RTL8822B/Hal8822BDef.h"
#include "RTL88XX/RTL8822B/Hal8822BPhyCfg.h"
#include "RTL88XX/RTL8822B/Hal8822BCfg.h"

#if IS_EXIST_RTL8822BE
#include "RTL88XX/RTL8822B/RTL8822BE/Hal8822BEDef.h"
#endif
#endif //#if IS_RTL8822B_SERIES

#if IS_RTL8822C_SERIES
#include "RTL88XX/RTL8822C/Hal8822CPwrSeqCmd.h"
#include "RTL88XX/RTL8822C/Hal8822CDef.h"
#include "RTL88XX/RTL8822C/Hal8822CPhyCfg.h"
#if IS_EXIST_RTL8822CE
#include "RTL88XX/RTL8822C/RTL8822CE/Hal8822CEDef.h"
#endif
#endif //#if IS_RTL8822C_SERIES

#if IS_RTL8812F_SERIES
#include "RTL88XX/RTL8812F/Hal8812FPwrSeqCmd.h"
#include "RTL88XX/RTL8812F/Hal8812FDef.h"
#include "RTL88XX/RTL8812F/Hal8812FPhyCfg.h"
#include "RTL88XX/RTL8812F/Hal8812FCfg.h"
#if IS_EXIST_RTL8812FE
#include "RTL88XX/RTL8812F/RTL8812FE/Hal8812FEDef.h"
#endif
#endif //#if IS_RTL8812F_SERIES

#if IS_RTL8821C_SERIES
#include "RTL88XX/RTL8821C/Hal8821CPwrSeqCmd.h"
#include "RTL88XX/RTL8821C/Hal8821CDef.h"
#include "RTL88XX/RTL8821C/Hal8821CPhyCfg.h"
#if IS_EXIST_RTL8821CE
#include "RTL88XX/RTL8821C/RTL8821CE/Hal8821CEDef.h"
#endif
#endif //#if IS_RTL8821C_SERIES

#if IS_RTL8814B_SERIES
#include "RTL88XX/RTL8814B/Hal8814BFirmware.h"
#include "RTL88XX/RTL8814B/Hal8814BPwrSeqCmd.h"
#include "RTL88XX/RTL8814B/Hal8814BDef.h"
#include "RTL88XX/RTL8814B/Hal8814BCfg.h"
#include "RTL88XX/RTL8814B/Hal8814BPhyCfg.h"
#include "RTL88XX/RTL8814B/RTL8814BE/Hal8814BEDef.h"
#include "RTL88XX/RTL8814B/Hal8814BTxDutyCycleSetting.h"

#if defined(CONFIG_RTL_OFFLOAD_DRIVER)
#include "RTL88XX/RTL8814B/Hal8814HTxDesc_core.h"
#include "RTL88XX/RTL8814B/Hal8814HRxDesc_core.h"
#include "RTL88XX/RTL8814B/Hal8814BFirmware_core.h"
#include "RTL88XX/RTL8814B/Hal8814HTxDesc.h"
#include "RTL88XX/RTL8814B/Hal8814HRxDesc.h"

#endif
#endif //#if IS_RTL8814B_SERIES

#if IS_RTL8197G_SERIES
#include "RTL88XX/RTL8197G/Hal8197GPwrSeqCmd.h"
#include "RTL88XX/RTL8197G/Hal8197GDef.h"
#include "RTL88XX/RTL8197G/Hal8197GCfg.h"
#include "RTL88XX/RTL8197G/Hal8197GPhyCfg.h"
#include "RTL88XX/RTL8197G/Hal8197GFirmware.h"
#endif //#if IS_RTL8197G_SERIES


#if IS_RTL8814A_SERIES
#include "Hal8814APwrSeqCmd.h"
#include "Hal8814ADef.h"
#include "Hal8814APhyCfg.h"
#include "Hal8814AFirmware.h"
#endif

#if IS_RTL8192F_SERIES
#include "Hal8192FPwrSeqCmd.h"
#include "Hal8192FDef.h"
#include "Hal8192FPhyCfg.h"
#endif

#if IS_EXIST_RTL8192EE
#include "Hal8192EEDef.h"
#endif
#if IS_EXIST_RTL8192EU
#include "Hal8192EUDef.h"
#endif

#if IS_EXIST_RTL8814AE
#include "Hal8814AEDef.h"
#endif

#if IS_EXIST_RTL8192FE
#if defined(CONFIG_USB_HCI)
#include "Hal8192FUDef.h" /* CONFIG_RTL_92F_SUPPORT */
#elif defined(CONFIG_PCI_HCI)
#include "Hal8192FEDef.h"
#endif
#endif

#include "Hal88XXDef.h"

#endif  //IS_RTL88XX_GENERATION

#else  

//HAL Shared with Driver
#include "Include/StatusCode.h"
#include "HalDbgCmd.h"


#ifdef CONFIG_WLAN_MACHAL_API
#include "HalMacAPI.h"
#endif

//MAC function support
#if !defined(__ECOS)
#include "HalMacFunc.h"
#endif
//Prototype
#include "HalDef.h"

//MAC Header provided by SD1 HWSD
#include "HalHeader/HalHWCfg.h"
#include "HalHeader/HalComTXDesc.h"
#include "HalHeader/HalComRXDesc.h"
//#include "HalHeader/HalComBit.h"
//#include "HalHeader/HalComReg.h"
#include "HalMac88XX/halmac_reg2.h"
#include "HalMac88XX/halmac_bit2.h"


#include "HalHeader/HalComPhyBit.h"
#include "HalHeader/HalComPhyReg.h"
#include "halmac_api.h"

#if !defined(__ECOS) && !defined(CPTCFG_CFG80211_MODULE)
#include "HalHeader/HalRxDescAp.h"
#include "HalHeader/HalTxDescAp.h"
#endif


//Instance
#include "HalCommon.h"

#if IS_RTL88XX_GENERATION

#include "RTL88XX/Hal88XXPwrSeqCmd.h"
#include "RTL88XX/Hal88XXReg.h"
#include "RTL88XX/Hal88XXDesc.h"
#include "RTL88XX/Hal88XXTxDesc.h"
#include "RTL88XX/Hal88XXRxDesc.h"
#include "RTL88XX/Hal88XXFirmware.h"
#include "RTL88XX/Hal88XXIsr.h"
#include "RTL88XX/Hal88XXDebug.h"
#include "RTL88XX/Hal88XXPhyCfg.h"
#include "RTL88XX/Hal88XXDM.h"


#if IS_RTL8881A_SERIES
#include "RTL88XX/RTL8881A/Hal8881APwrSeqCmd.h"
#include "RTL88XX/RTL8881A/Hal8881ADef.h"
#include "RTL88XX/RTL8881A/Hal8881APhyCfg.h"
#endif

#if IS_RTL8192E_SERIES
#include "RTL88XX/RTL8192E/Hal8192EPwrSeqCmd.h"
#include "RTL88XX/RTL8192E/Hal8192EDef.h"
#include "RTL88XX/RTL8192E/Hal8192EPhyCfg.h"
#endif


#if IS_RTL8192F_SERIES
#include "Hal8192FPwrSeqCmd.h"
#include "Hal8192FDef.h"
#include "Hal8192FPhyCfg.h"
#endif

#if IS_EXIST_RTL8192FE
#if defined(CONFIG_USB_HCI)
#include "Hal8192FUDef.h" /* CONFIG_RTL_92F_SUPPORT */
#elif defined(CONFIG_PCI_HCI)
#include "Hal8192FEDef.h"
#endif
#endif


#if IS_RTL8814A_SERIES
#include "RTL88XX/RTL8814A/Hal8814APwrSeqCmd.h"
#include "RTL88XX/RTL8814A/Hal8814ADef.h"
#include "RTL88XX/RTL8814A/Hal8814APhyCfg.h"
#include "RTL88XX/RTL8814A/Hal8814AFirmware.h"
#endif

#if IS_RTL8822B_SERIES
#include "RTL88XX/RTL8822B/Hal8822BPwrSeqCmd.h"
#include "RTL88XX/RTL8822B/Hal8822BDef.h"
#include "RTL88XX/RTL8822B/Hal8822BPhyCfg.h"
#include "RTL88XX/RTL8822B/RTL8822BE/Hal8822BEDef.h"
#endif //#if IS_RTL8822B_SERIES

#if IS_RTL8821C_SERIES
#include "RTL88XX/RTL8821C/Hal8821CPwrSeqCmd.h"
#include "RTL88XX/RTL8821C/Hal8821CDef.h"
#include "RTL88XX/RTL8821C/Hal8821CPhyCfg.h"
#include "RTL88XX/RTL8821C/RTL8821CE/Hal8821CEDef.h"
#endif //#if IS_RTL8821C_SERIES

#if IS_RTL8814B_SERIES
#include "RTL88XX/RTL8814B/Hal8814BFirmware.h"
#include "RTL88XX/RTL8814B/Hal8814BPwrSeqCmd.h"
#include "RTL88XX/RTL8814B/Hal8814BDef.h"
#include "RTL88XX/RTL8814B/Hal8814BCfg.h"
#include "RTL88XX/RTL8814B/Hal8814BPhyCfg.h"
#include "RTL88XX/RTL8814B/RTL8814BE/Hal8814BEDef.h"
#include "RTL88XX/RTL8814B/Hal8814BTxDutyCycleSetting.h"

#if defined(CONFIG_RTL_OFFLOAD_DRIVER)
#include "../core/WlanHAL/RTL88XX/RTL8814B/Hal8814HTxDesc_core.h"
#include "../core/WlanHAL/RTL88XX/RTL8814B/Hal8814HRxDesc_core.h"
#include "../core/WlanHAL/RTL88XX/RTL8814B/Hal8814BFirmware_core.h"
#include "RTL88XX/RTL8814B/Hal8814HTxDesc.h"
#include "RTL88XX/RTL8814B/Hal8814HRxDesc.h"
#endif

#endif //#if IS_RTL8814B_SERIES

#if IS_RTL8197F_SERIES
#include "RTL88XX/RTL8197F/Hal8197FPwrSeqCmd.h"
#include "RTL88XX/RTL8197F/Hal8197FDef.h"
#include "RTL88XX/RTL8197F/Hal8197FPhyCfg.h"
#include "RTL88XX/RTL8197F/Hal8197FCfg.h"
#endif //#if IS_RTL8197F_SERIES

#if IS_RTL8198F_SERIES
#include "RTL88XX/RTL8198F/Hal8198FPwrSeqCmd.h"
#include "RTL88XX/RTL8198F/Hal8198FDef.h"
#include "RTL88XX/RTL8198F/Hal8198FPhyCfg.h"
#include "RTL88XX/RTL8198F/Hal8198FFirmware.h"
#include "RTL88XX/RTL8198F/Hal8198FCfg.h"
#endif //#if IS_RTL8198F_SERIES


#if IS_RTL8197G_SERIES
#include "RTL88XX/RTL8197G/Hal8197GPwrSeqCmd.h"
#include "RTL88XX/RTL8197G/Hal8197GDef.h"
#include "RTL88XX/RTL8197G/Hal8197GPhyCfg.h"
#include "RTL88XX/RTL8197G/Hal8197GFirmware.h"
#include "RTL88XX/RTL8197G/Hal8197GCfg.h"
#endif //#if IS_RTL8197G_SERIES


#if IS_RTL8812F_SERIES
#include "RTL88XX/RTL8812F/Hal8812FPwrSeqCmd.h"
#include "RTL88XX/RTL8812F/Hal8812FDef.h"
#include "RTL88XX/RTL8812F/Hal8812FPhyCfg.h"
#include "RTL88XX/RTL8812F/Hal8812FCfg.h"
#include "RTL88XX/RTL8812F/RTL8812FE/Hal8812FEDef.h"
#include "RTL88XX/RTL8812F/Hal8812FCfg.h"
#endif

#if IS_EXIST_RTL8192EE
#include "RTL88XX/RTL8192E/RTL8192EE/Hal8192EEDef.h"
#endif
#if IS_EXIST_RTL8192EU
#include "RTL88XX/RTL8192E/RTL8192EU/Hal8192EUDef.h"
#endif

#if IS_EXIST_RTL8814AE
#include "RTL88XX/RTL8814A/RTL8814AE/Hal8814AEDef.h"
#endif


#include "RTL88XX/Hal88XXDef.h"
#endif  //IS_RTL88XX_GENERATION

#endif  //ECOS

#endif  //#ifndef __INC_PRECOMPINC_H
