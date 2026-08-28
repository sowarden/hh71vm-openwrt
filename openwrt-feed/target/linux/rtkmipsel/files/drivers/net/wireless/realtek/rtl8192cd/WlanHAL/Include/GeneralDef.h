#ifndef __INC_GENERALDEF_H
#define __INC_GENERALDEF_H

/*
 *	Note:	1.Only pure value definition can be put here.
 *			2.The definition here should be hardware and platform independent.
 *
*/

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

// Two Marco below are used to this case :"IN" & "OUT" are used to other purpose
#ifndef INPUT
#define INPUT
#endif

#ifndef OUTPUT
#define OUTPUT
#endif



#ifndef TRUE
    #define TRUE                1
#endif
#ifndef FALSE	
    #define FALSE               0
#endif

#ifndef _TRUE
    #define _TRUE               1
#endif
#ifndef _FALSE	
    #define _FALSE              0
#endif

#ifndef BIT
#define BIT(x)		(1 << (x))
#endif

#ifndef BIT0
#define BIT0		0x00000001
#define BIT1		0x00000002
#define BIT2		0x00000004
#define BIT3		0x00000008
#define BIT4		0x00000010
#define BIT5		0x00000020
#define BIT6		0x00000040
#define BIT7		0x00000080
#define BIT8		0x00000100
#define BIT9		0x00000200
#define BIT10		0x00000400								
#define BIT11		0x00000800								
#define BIT12		0x00001000								
#define BIT13		0x00002000								
#define BIT14		0x00004000								
#define BIT15		0x00008000								
#define BIT16		0x00010000								
#define BIT17		0x00020000								
#define BIT18		0x00040000								
#define BIT19		0x00080000								
#define BIT20		0x00100000								
#define BIT21		0x00200000								
#define BIT22		0x00400000								
#define BIT23		0x00800000								
#define BIT24		0x01000000								
#define BIT25		0x02000000								
#define BIT26		0x04000000								
#define BIT27		0x08000000								
#define BIT28		0x10000000								
#define BIT29		0x20000000								
#define BIT30		0x40000000								
#define BIT31		0x80000000								
#endif

#ifndef BIT32
#define BIT32	UINT64_C(0x0000000100000000)
#define BIT33	UINT64_C(0x0000000200000000)
#define BIT34	UINT64_C(0x0000000400000000)
#define BIT35	UINT64_C(0x0000000800000000)
#define BIT36	UINT64_C(0x0000001000000000)
#define BIT37	UINT64_C(0x0000002000000000)
#define BIT38	UINT64_C(0x0000004000000000)
#define BIT39	UINT64_C(0x0000008000000000)
#define BIT40	UINT64_C(0x0000010000000000)
#define BIT41	UINT64_C(0x0000020000000000)
#define BIT42	UINT64_C(0x0000040000000000)
#define BIT43	UINT64_C(0x0000080000000000)
#define BIT44	UINT64_C(0x0000100000000000)
#define BIT45	UINT64_C(0x0000200000000000)
#define BIT46	UINT64_C(0x0000400000000000)
#define BIT47	UINT64_C(0x0000800000000000)
#define BIT48	UINT64_C(0x0001000000000000)
#define BIT49	UINT64_C(0x0002000000000000)
#define BIT50	UINT64_C(0x0004000000000000)
#define BIT51	UINT64_C(0x0008000000000000)
#define BIT52	UINT64_C(0x0010000000000000)
#define BIT53	UINT64_C(0x0020000000000000)
#define BIT54	UINT64_C(0x0040000000000000)
#define BIT55	UINT64_C(0x0080000000000000)
#define BIT56	UINT64_C(0x0100000000000000)
#define BIT57	UINT64_C(0x0200000000000000)
#define BIT58	UINT64_C(0x0400000000000000)
#define BIT59	UINT64_C(0x0800000000000000)
#define BIT60	UINT64_C(0x1000000000000000)
#define BIT61	UINT64_C(0x2000000000000000)
#define BIT62	UINT64_C(0x4000000000000000)
#define BIT63	UINT64_C(0x8000000000000000)
#endif

//-----------------------------------------------------------------------------------------
// Use one or multiple of the following value to define the flag, HAL_CODE_BASE.
//-----------------------------------------------------------------------------------------

//Element
#define RTL8881AEM      BIT0
#define RTL8192EE       BIT1
#define RTL8192EU       BIT2
#define RTL8192ES	    BIT3
#define RTL8814AE       BIT4
#define RTL8814AU       BIT5
#define RTL8197FEM      BIT6

#define RTL8822BE       BIT7
#define RTL8822BU       BIT8
#define RTL8822BS       BIT9

#define RTL8821CE       BIT10
#define RTL8821CS       BIT11
#define RTL8192FE		BIT12

#define RTL8198FEM      BIT13
#define RTL8814BE       BIT14
#define RTL8814BU       BIT15
#define RTL8814BS       BIT16

#define RTL8822CE       BIT17
#define RTL8822CU       BIT18
#define RTL8822CS       BIT19

#define RTL8812FE       BIT20
#define RTL8812FU       BIT21
#define RTL8812FS       BIT22

#define RTL8197GEM      BIT23

//Chip Collection
#define RTL8881A        (RTL8881AEM)
#define RTL8192E        (RTL8192EE | RTL8192EU | RTL8192ES)
#define RTL8814A        (RTL8814AE | RTL8814AU)
#define RTL8197F        (RTL8197FEM)
#define RTL8822B        (RTL8822BE | RTL8822BU | RTL8822BS)
#define RTL8821C        (RTL8821CE | RTL8821CS)
#define RTL8192F		(RTL8192FE)
#define RTL8198F        (RTL8198FEM)
#define RTL8814B        (RTL8814BE | RTL8814BU | RTL8814BS)
#define RTL8822C        (RTL8822CE | RTL8822CU | RTL8822CS)
#define RTL8812F        (RTL8812FE | RTL8812FU | RTL8812FS)
#define RTL8197G        (RTL8197GEM)


//HAL Common Layer
#define RTL88XX         (RTL8881A | RTL8192E | RTL8814A | RTL8197F | RTL8822B | RTL8821C | RTL8192F | RTL8198F | RTL8814B | RTL8822C | RTL8812F | RTL8197G)
#define RTL88XX_N       (RTL8192E | RTL8197F | RTL8192F | RTL8197G)
#define RTL88XX_AC      (RTL8881A | RTL8814A | RTL8822B | RTL8821C | RTL8814B | RTL8198F | RTL8822C | RTL8812F)

//-----------------------------------------------------------------------
//  Note : 8881A,8192E , First generation MAC using TXBD architecture
//  Note : After 8814A, Some Register define re-allocate. 
//         And increase page.10 ~ page.17 for MAC register, 
//         so we diff to MAC version 2
//-----------------------------------------------------------------------

#define RTL88XX_MAC_V1  (RTL8881A | RTL8192E | RTL8192F)
#define RTL88XX_MAC_V2  (RTL8814A | RTL8197F | RTL8822B | RTL8821C | RTL8822C | RTL8812F)
#define RTL88XX_MAC_V1_V2   (RTL88XX_MAC_V1 | RTL88XX_MAC_V2) 
#define RTL88XX_MAC_V3  (RTL8198F | RTL8197G)
#define RTL88XX_MAC_V2_V3   (RTL88XX_MAC_V2 | RTL88XX_MAC_V3) 
#define RTL88XX_MAC_V1_V2_V3   (RTL88XX_MAC_V1 | RTL88XX_MAC_V2 | RTL88XX_MAC_V3) 
#define RTL88XX_MAC_V4  (RTL8814B)
#define RTL88XX_MAC_V3_V4   (RTL88XX_MAC_V3 | RTL88XX_MAC_V4) 
#define RTL88XX_MAC_V2_V3_V4   (RTL88XX_MAC_V2 | RTL88XX_MAC_V3 | RTL88XX_MAC_V4) 
#define RTL88XX_MAC_V1_V2_V4   (RTL88XX_MAC_V1 | RTL88XX_MAC_V2 | RTL88XX_MAC_V4)

//FW CPU type
#define RTL_FW_CPU_8051 (RTL8881A | RTL8192E | RTL8192F)
#define RTL_FW_CPU_MIPS (RTL8814A | RTL8197F | RTL8822B | RTL8821C | RTL8198F | RTL8814B | RTL8822C | RTL8812F | RTL8197G)

//HALMAC API
#define RTL_MACHAL_API  (RTL8822B | RTL8821C | RTL8814B | RTL8822C | RTL8812F)

//Marco Utility
#define IS_ONLY_RTL8881AEM              ((HAL_CODE_BASE == RTL8881AEM) ? _TRUE : _FALSE)
#define IS_ONLY_RTL8192EE               ((HAL_CODE_BASE == RTL8192EE) ? _TRUE : _FALSE)
#define IS_ONLY_RTL8192EU               ((HAL_CODE_BASE == RTL8192EU) ? _TRUE : _FALSE)
#define IS_ONLY_RTL8814AE               ((HAL_CODE_BASE == RTL8814AE) ? _TRUE : _FALSE)
#define IS_ONLY_RTL8814AU               ((HAL_CODE_BASE == RTL8814AU) ? _TRUE : _FALSE)
#define IS_ONLY_RTL8881AEM_RTL8192EE    ((HAL_CODE_BASE == (RTL8881AEM|RTL8192EE)) ? _TRUE : _FALSE)
#define IS_ONLY_RTL8197FEM              ((HAL_CODE_BASE == RTL8197FEM) ? _TRUE : _FALSE)
#define IS_ONLY_RTL8198FEM              ((HAL_CODE_BASE == RTL8198FEM) ? _TRUE : _FALSE)
#define IS_ONLY_RTL8814BE               ((HAL_CODE_BASE == RTL8814BE) ? _TRUE : _FALSE)
#define IS_ONLY_RTL8197GEM              ((HAL_CODE_BASE == RTL8814BE) ? _TRUE : _FALSE)


//Chip Existence
#define IS_EXIST_RTL8881AEM                         ((HAL_CODE_BASE & RTL8881AEM) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8192EE                          ((HAL_CODE_BASE & RTL8192EE) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8192EU                          ((HAL_CODE_BASE & RTL8192EU) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8192ES                          ((HAL_CODE_BASE & RTL8192ES) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8814AE                          ((HAL_CODE_BASE & RTL8814AE) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8814AU                          ((HAL_CODE_BASE & RTL8814AU) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8881AEM_RTL8192EE               (IS_EXIST_RTL8881AEM && IS_EXIST_RTL8192EE)
#define IS_EXIST_RTL8197FEM                         ((HAL_CODE_BASE & RTL8197FEM) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8198FEM                         ((HAL_CODE_BASE & RTL8198FEM) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8822BE                          ((HAL_CODE_BASE & RTL8822BE) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8822BU                          ((HAL_CODE_BASE & RTL8822BU) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8822BS                          ((HAL_CODE_BASE & RTL8822BS) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8821CE                          ((HAL_CODE_BASE & RTL8821CE) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8821CS                          ((HAL_CODE_BASE & RTL8821CS) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8192FE                          ((HAL_CODE_BASE & RTL8192FE) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8814BE                          ((HAL_CODE_BASE & RTL8814BE) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8814BU                          ((HAL_CODE_BASE & RTL8814BU) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8814BS                          ((HAL_CODE_BASE & RTL8814BS) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8822CE                          ((HAL_CODE_BASE & RTL8822CE) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8822CU                          ((HAL_CODE_BASE & RTL8822CU) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8822CS                          ((HAL_CODE_BASE & RTL8822CS) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8812FE                          ((HAL_CODE_BASE & RTL8812FE) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8812FU                          ((HAL_CODE_BASE & RTL8812FU) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8812FS                          ((HAL_CODE_BASE & RTL8812FS) ? _TRUE : _FALSE)
#define IS_EXIST_RTL8197GEM                         ((HAL_CODE_BASE & RTL8197GEM) ? _TRUE : _FALSE)


//Chip Series
#define IS_RTL8192E_SERIES              ((HAL_CODE_BASE & RTL8192E) ? _TRUE : _FALSE)
#define IS_RTL8881A_SERIES              ((HAL_CODE_BASE & RTL8881A) ? _TRUE : _FALSE)
#define IS_RTL8814A_SERIES              ((HAL_CODE_BASE & RTL8814A) ? _TRUE : _FALSE)
#define IS_RTL8197F_SERIES              ((HAL_CODE_BASE & RTL8197F) ? _TRUE : _FALSE)
#define IS_RTL8822B_SERIES              ((HAL_CODE_BASE & RTL8822B) ? _TRUE : _FALSE)
#define IS_RTL8821C_SERIES              ((HAL_CODE_BASE & RTL8821C) ? _TRUE : _FALSE)
#define IS_RTL8192F_SERIES              ((HAL_CODE_BASE & RTL8192F) ? _TRUE : _FALSE)
#define IS_RTL8198F_SERIES              ((HAL_CODE_BASE & RTL8198F) ? _TRUE : _FALSE)
#define IS_RTL8814B_SERIES              ((HAL_CODE_BASE & RTL8814B) ? _TRUE : _FALSE)
#define IS_RTL8822C_SERIES              ((HAL_CODE_BASE & RTL8822C) ? _TRUE : _FALSE)
#define IS_RTL8812F_SERIES              ((HAL_CODE_BASE & RTL8812F) ? _TRUE : _FALSE)
#define IS_RTL8197G_SERIES              ((HAL_CODE_BASE & RTL8197G) ? _TRUE : _FALSE)


//MAC version Series
#define IS_RTL88XX_MAC_V1               ((HAL_CODE_BASE & RTL88XX_MAC_V1) ? _TRUE : _FALSE)
#define IS_RTL88XX_MAC_V2               ((HAL_CODE_BASE & RTL88XX_MAC_V2) ? _TRUE : _FALSE)
#define IS_RTL88XX_MAC_V1_V2            ((HAL_CODE_BASE & RTL88XX_MAC_V1_V2) ? _TRUE : _FALSE)
#define IS_RTL88XX_MAC_V3               ((HAL_CODE_BASE & RTL88XX_MAC_V3) ? _TRUE : _FALSE)
#define IS_RTL88XX_MAC_V2_V3            ((HAL_CODE_BASE & RTL88XX_MAC_V2_V3) ? _TRUE : _FALSE)
#define IS_RTL88XX_MAC_V1_V2_V3         ((HAL_CODE_BASE & RTL88XX_MAC_V1_V2_V3) ? _TRUE : _FALSE)
#define IS_RTL88XX_MAC_V4               ((HAL_CODE_BASE & RTL88XX_MAC_V4) ? _TRUE : _FALSE)
#define IS_RTL88XX_MAC_V3_V4            ((HAL_CODE_BASE & RTL88XX_MAC_V3_V4) ? _TRUE : _FALSE)
#define IS_RTL88XX_MAC_V2_V3_V4         ((HAL_CODE_BASE & RTL88XX_MAC_V2_V3_V4) ? _TRUE : _FALSE)
#define IS_RTL88XX_MAC_V1_V2_V4         ((HAL_CODE_BASE & RTL88XX_MAC_V1_V2_V4) ? _TRUE : _FALSE)

//Chip Network Type
#define IS_RTL88XX_N                    ((HAL_CODE_BASE & RTL88XX_N) ? _TRUE : _FALSE)
#define IS_RTL88XX_AC                   ((HAL_CODE_BASE & RTL88XX_AC) ? _TRUE : _FALSE)


//Chip FW CPU Type
#define IS_RTL88XX_FW_8051              ((HAL_CODE_BASE & RTL_FW_CPU_8051) ? _TRUE : _FALSE)
#define IS_RTL88XX_FW_MIPS              ((HAL_CODE_BASE & RTL_FW_CPU_MIPS) ? _TRUE : _FALSE)


//Chip Generation
#define IS_RTL88XX_GENERATION           ((HAL_CODE_BASE & RTL88XX) ? _TRUE : _FALSE)

//Support MACHAL API
#define IS_SUPPORT_MACHAL_API           ((HAL_CODE_BASE & RTL_MACHAL_API) ? _TRUE : _FALSE)

//-----------------------------------------------------------------------------------------
// Use one or multiple of the following value to define the flag, HAL_DEV_BUS_TYPE.
// *** This Setting is necessary to be independent with Chip  ***
//-----------------------------------------------------------------------------------------
#define HAL_RT_EMBEDDED_INTERFACE           BIT0
#define HAL_RT_PCI_INTERFACE				BIT1
#define HAL_RT_USB_INTERFACE				BIT2
#define HAL_RT_SDIO_INTERFACE				BIT3

//Marco Utility
#define IS_ONLY_EMBEDDED              ((HAL_DEV_BUS_TYPE == HAL_RT_EMBEDDED_INTERFACE) ? _TRUE : _FALSE)
#define IS_ONLY_PCI                           ((HAL_DEV_BUS_TYPE == HAL_RT_PCI_INTERFACE) ? _TRUE : _FALSE)
#define IS_ONLY_USB                         ((HAL_DEV_BUS_TYPE == HAL_RT_USB_INTERFACE) ? _TRUE : _FALSE)
#define IS_ONLY_SDIO                        ((HAL_DEV_BUS_TYPE == HAL_RT_SDIO_INTERFACE) ? _TRUE : _FALSE)
#define IS_ONLY_EMBEDDED_PCI       ((HAL_DEV_BUS_TYPE == (HAL_RT_EMBEDDED_INTERFACE|HAL_RT_PCI_INTERFACE)) ? _TRUE : _FALSE)

#define IS_EXIST_EMBEDDED              ((HAL_DEV_BUS_TYPE & HAL_RT_EMBEDDED_INTERFACE) ? _TRUE : _FALSE)
#define IS_EXIST_PCI                           ((HAL_DEV_BUS_TYPE & HAL_RT_PCI_INTERFACE) ? _TRUE : _FALSE)
#define IS_EXIST_USB                         ((HAL_DEV_BUS_TYPE & HAL_RT_USB_INTERFACE) ? _TRUE : _FALSE)
#define IS_EXIST_SDIO                        ((HAL_DEV_BUS_TYPE & HAL_RT_SDIO_INTERFACE) ? _TRUE : _FALSE)
#define IS_EXIST_EMBEDDED_PCI        (IS_EXIST_EMBEDDED && IS_EXIST_PCI)



#endif // #ifndef __INC_GENERALDEF_H

