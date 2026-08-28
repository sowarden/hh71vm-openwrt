#ifndef __HAL_TXDUTYSETTING_H__
#define __HAL_TXDUTYSETTING_H__

typedef struct _WL_TXDUTY_CFG_
{
	unsigned char 	valid;
	unsigned char 	rate; 		
	unsigned char 	duty; 		
} WLAN_TXDUTY_CFG, *PWLAN_TXDUTY_CFG;

#endif
