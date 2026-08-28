/*
 *  WPA PSK handling routines
 *
 *  Copyright (c) 2017 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define _8192CD_PSK_C_

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/timer.h>
#include <linux/random.h>
#elif defined(__ECOS)
#include <cyg/io/eth/rltk/819x/wrapper/sys_support.h>
#include <cyg/io/eth/rltk/819x/wrapper/skbuff.h>
#include <cyg/io/eth/rltk/819x/wrapper/timer.h>
#include <cyg/io/eth/rltk/819x/wrapper/wrapper.h>
#endif

#include "./8192cd_cfg.h"

#if (defined(INCLUDE_WPA_PSK) && !defined(WIFI_HAPD) && !defined(RTK_NL80211)) || defined(HAPD_DRV_PSK_WPS)

#include "./8192cd.h"
#include "./wifi.h"
#include "./8192cd_util.h"
#include "./8192cd_headers.h"
#include "./8192cd_security.h"
#if defined(__KERNEL__) || defined(__OSK__)
#include "./ieee802_mib.h"
#elif defined(__ECOS)
#include <cyg/io/eth/rltk/819x/wlan/ieee802_mib.h>
#endif
#include "./8192cd_debug.h"
#include "./8192cd_psk.h"
#include "./1x_rc4.h"

#if !defined(__KERNEL__) && !defined(__ECOS)
#include "./sys-support.h"
#endif


#if defined(__OSK__) && (defined(INCLUDE_WPA_PSK) && defined(CONFIG_AUTH))
extern int DecGTK(OCTET_STRING EAPOLMsgRecvd, unsigned char *kek, int keklen, int keylen, unsigned char *kout, int kout_len);
#endif

#ifdef CONFIG_IEEE80211R
#include "./sha256.h"
#ifdef CONFIG_IEEE80211R_CLI
#include "./8192cd_11r_cli.h"
#endif
#endif

//#define DEBUG_PSK

#define ETHER_ADDRLEN					6
#define PMK_EXPANSION_CONST 	        "Pairwise key expansion"
#define PMK_EXPANSION_CONST_SIZE		22
#ifdef RTL_WPA2
#define PMKID_NAME_CONST 	        	"PMK Name"
#define PMKID_NAME_CONST_SIZE			8
#endif /* RTL_WPA2 */
#define GMK_EXPANSION_CONST				"Group key expansion"
#define GMK_EXPANSION_CONST_SIZE		19
#define RANDOM_EXPANSION_CONST			"Init Counter"
#define RANDOM_EXPANSION_CONST_SIZE	12
#define PTK_LEN_CCMP            		48

#define IGMK_EXPANSION_CONST 	        "IGTK key expansion"
#define IGMK_EXPANSION_CONST_SIZE		18

#ifdef CONFIG_IEEE80211R
#define FT_PMKR0_CONST					"FT-R0"
#define FT_PMKR0_CONST_SIZE				5
#define FT_PMKR0_NAME_CONST				"FT-R0N"
#define FT_PMKR0_NAME_CONST_SIZE		6
#define FT_PMKR1_CONST					"FT-R1"
#define FT_PMKR1_CONST_SIZE				5
#define FT_PMKR1_NAME_CONST				"FT-R1N"
#define FT_PMKR1_NAME_CONST_SIZE		6
#define FT_PMK_EXPANSION_CONST			"FT-PTK"
#define FT_PMK_EXPANSION_CONST_SIZE		6
#endif

unsigned char GTK_KDE_TYPE[] = {0xDD, 0x16, 0x00, 0x0F, 0xAC, 0x01, 0x01, 0x00};

/*
	2008-12-16, For Corega CG-WLCB54GL 54Mbps NIC interoperability issue.
	The behavior of this NIC when it connect to the other AP with WPA/TKIP is:
		AP	<----------------------> 	STA
			....................
			------------> Assoc Rsp (ok)
			------------> EAPOL-key (4-way msg 1)
			<------------ unknown TKIP encryption data
			------------> EAPOL-key (4-way msg 1)
			<------------ unknown TKIP encryption data
			.....................
			<------------ disassoc (code=8, STA is leaving) when the 5 seconds timer timeout counting from Assoc_Rsp is got.
			....................
			------------> Assoc Rsp (ok)
			<-----------> EAPOL-key (4-way handshake success)

	If MAX_RESEND_NUM=3, our AP will send disassoc (code=15, 4-way timeout) to STA before STA sending disassoc to AP.
	And this NIC will always can not connect to our AP.
	set MAX_RESEND_NUM=5 can fix this issue.
 */
#if defined(__ECOS) && defined(CONFIG_SDIO_HCI)
#define MAX_RESEND_NUM					3
#define RESEND_TIME						RTL_MILISECONDS_TO_JIFFIES(500)
#else
//#define MAX_RESEND_NUM					3
#define MAX_RESEND_NUM					5
#define RESEND_TIME						RTL_SECONDS_TO_JIFFIES(1)	// in 10ms
#endif

#ifdef CLIENT_MODE
#define WAIT_EAP_TIME				RTL_SECONDS_TO_JIFFIES(5)
#endif

#define LargeIntegerOverflow(x) (x.field.HighPart == 0xffffffff) && \
								(x.field.LowPart == 0xffffffff)
#define LargeIntegerZero(x) memset(&x.charData, 0, 8);

#define Octet16IntegerOverflow(x) LargeIntegerOverflow(x.field.HighPart) && \
								  LargeIntegerOverflow(x.field.LowPart)
#define Octet16IntegerZero(x) memset(&x.charData, 0, 16);

#define SetNonce(ocDst, oc32Counter) SetEAPOL_KEYIV(ocDst, oc32Counter)


#if defined(CONFIG_RTL8186_KB_N)|| defined(CONFIG_AUTH_RESULT)
int authRes = 0;//0: success; 1: fail
#endif

extern void hmac_sha(
	unsigned char*	k,     /* secret key */
	int				lk,    /* length of the key in bytes */
	unsigned char*	d,     /* data */
	int				ld,    /* length of data in bytes */
	unsigned char*	out,   /* output buffer, at least "t" bytes */
	int				t
);

extern void hmac_sha1(unsigned char *text, int text_len, unsigned char *key,
					  int key_len, unsigned char *digest);

extern void hmac_md5(unsigned char *text, int text_len, unsigned char *key,
					 int key_len, void * digest);

#ifdef RTL_WPA2
extern void AES_WRAP(unsigned char *plain, int plain_len,
					 unsigned char *iv,	int iv_len,
					 unsigned char *kek,	int kek_len,
					 unsigned char *cipher, unsigned short *cipher_len);
#ifdef CLIENT_MODE
extern void AES_UnWRAP(unsigned char *cipher, int cipher_len, unsigned char *kek,
					   int kek_len, unsigned char *plain, int plain_len);
#endif
#endif

static void UpdateGK(struct rtl8192cd_priv *priv);
static void SendEAPOL(struct rtl8192cd_priv *priv, struct stat_info *pstat, int resend);
static void ResendTimeout(unsigned long task_psta);
static void reset_sta_info(struct rtl8192cd_priv *priv, struct stat_info *pstat);

#ifdef CONFIG_IEEE80211R
static void CalcFTPTK(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *keyout, int keyoutlen);
#endif

#ifdef DEBUG_PSK
static char *ID2STR(int id)
{
	switch (id) {
	case DOT11_EVENT_ASSOCIATION_IND:
		return("DOT11_EVENT_ASSOCIATION_IND");
	case DOT11_EVENT_REASSOCIATION_IND:
		return ("DOT11_EVENT_REASSOCIATION_IND");

	case DOT11_EVENT_DISASSOCIATION_IND:
		return ("DOT11_EVENT_DISASSOCIATION_IND");

	case DOT11_EVENT_EAP_PACKET:
		return ("DOT11_EVENT_EAP_PACKET");

	case DOT11_EVENT_MIC_FAILURE:
		return ("DOT11_EVENT_MIC_FAILURE");
	default:
		return ("Not support event");

	}
}

static char *STATE2RXMSG(struct stat_info *pstat)
{
	if (Message_KeyType(pstat->wpa_sta_info->EapolKeyMsgRecvd) == type_Pairwise) {
		if (pstat->wpa_sta_info->state == PSK_STATE_PTKSTART)
			return ("4-2");
		else if (pstat->wpa_sta_info->state == PSK_STATE_PTKINITNEGOTIATING) {
			if (Message_KeyDataLength(pstat->wpa_sta_info->EapolKeyMsgRecvd) != 0)
				return ("4-2 (duplicated)");
			else
				return ("4-4");
		} else if (pstat->wpa_sta_info->state == PSK_STATE_PTKINITDONE)
			return ("4-4 (duplicated)");
		else
			return ("invalid state");

	} else
		return ("2-2");

}
#endif // DEBUG_PSK

static OCTET_STRING SubStr(OCTET_STRING f, unsigned short s, unsigned short l)
{
	OCTET_STRING res;

	res.Length = l;
	res.Octet = f.Octet+s;
	return res;
}

static void i_P_SHA1(
	unsigned char*  key,                // pointer to authentication key
	int             key_len,            // length of authentication key
	unsigned char*  text,               // pointer to data stream
	int             text_len,           // length of data stream
	unsigned char*  digest,             // caller digest to be filled in
	int				digest_len			// in byte
)
{
	int i;
	int offset = 0;
	int step = 20;
	int IterationNum = (digest_len + step - 1) / step;

	for (i = 0; i < IterationNum; i++) {
		text[text_len] = (unsigned char)i;
		hmac_sha(key, key_len, text, text_len + 1, digest + offset, step);
		offset += step;
	}
}

static void i_PRF(
	unsigned char*	secret,
	int				secret_len,
	unsigned char*	prefix,
	int				prefix_len,
	unsigned char*	random,
	int				random_len,
	unsigned char*  digest,             // caller digest to be filled in
	int				digest_len			// in byte
)
{
	unsigned char data[1000];
	if (prefix_len+random_len+1 > sizeof(data)) {
		printk("[%s] exceed in size\n", __func__);
		return;
	}
	memcpy(data, prefix, prefix_len);
	data[prefix_len++] = 0;
	memcpy(data + prefix_len, random, random_len);
	i_P_SHA1(secret, secret_len, data, prefix_len + random_len, digest, digest_len);
}


/*
 * F(P, S, c, i) = U1 xor U2 xor ... Uc
 * U1 = PRF(P, S || Int(i))
 * U2 = PRF(P, U1)
 * Uc = PRF(P, Uc-1)
 */

static void F(
	char *password,
	int passwordlength,
	unsigned char *ssid,
	int ssidlength,
	int iterations,
	int count,
	unsigned char *output)
{
	unsigned char digest[36], digest1[A_SHA_DIGEST_LEN];
	int i, j;

	/* U1 = PRF(P, S || int(i)) */
	if ((ssidlength + 4) <= sizeof(digest))
	{
		memcpy(digest, ssid, ssidlength);
		digest[ssidlength] = (unsigned char)((count >> 24) & 0xff);
		digest[ssidlength + 1] = (unsigned char)((count >> 16) & 0xff);
		digest[ssidlength + 2] = (unsigned char)((count >> 8) & 0xff);
		digest[ssidlength + 3] = (unsigned char)(count & 0xff);
	}
	hmac_sha1(digest, ssidlength + 4,
			  (unsigned char*) password, (int)strlen(password),
			  digest1);

	/*
	hmac_sha1((unsigned char*) password, passwordlength,
	       digest, ssidlength+4, digest1);
	*/

	/* output = U1 */
	memcpy(output, digest1, A_SHA_DIGEST_LEN);

	for (i = 1; i < iterations; i++) {
		/* Un = PRF(P, Un-1) */
		hmac_sha1(digest1, A_SHA_DIGEST_LEN, (unsigned char*) password,
				  (int)strlen(password), digest);
		//hmac_sha1((unsigned char*) password, passwordlength,digest1, A_SHA_DIGEST_LEN, digest);
		memcpy(digest1, digest, A_SHA_DIGEST_LEN);

		/* output = output xor Un */
		for (j = 0; j < A_SHA_DIGEST_LEN; j++) {
			output[j] ^= digest[j];
		}
	}
}

/*
 * password - ascii string up to 63 characters in length
 * ssid - octet string up to 32 octets
 * ssidlength - length of ssid in octets
 * output must be 40 octets in length and outputs 256 bits of key
 */
static int PasswordHash (
	char *password,
	unsigned char *ssid,
	short ssidlength,
	unsigned char *output)
{
	int passwordlength = strlen(password);
//	int ssidlength = strlen(ssid);

	if ((passwordlength > 63) || (ssidlength > 32))
		return 0;

	F(password, passwordlength, ssid, ssidlength, 4096, 1, output);
	F(password, passwordlength, ssid, ssidlength, 4096, 2, &output[A_SHA_DIGEST_LEN]);
	return 1;
}

static void Message_ReplayCounter_OC2LI(OCTET_STRING f, LARGE_INTEGER * li)
{
	li->field.HighPart = ((unsigned int)(*(f.Octet + ReplayCounterPos + 3)))
						 + ((unsigned int)(*(f.Octet + ReplayCounterPos + 2)) << 8)
						 + ((unsigned int)(*(f.Octet + ReplayCounterPos + 1)) <<  16)
						 + ((unsigned int)(*(f.Octet + ReplayCounterPos + 0)) << 24);
	li->field.LowPart =  ((unsigned int)(*(f.Octet + ReplayCounterPos + 7)))
						 + ((unsigned int)(*(f.Octet + ReplayCounterPos + 6)) << 8)
						 + ((unsigned int)(*(f.Octet + ReplayCounterPos + 5)) <<  16)
						 + ((unsigned int)(*(f.Octet + ReplayCounterPos + 4)) << 24);
}

#if 1
/*-----------------------------------------------------------------------------------------------
	f is EAPOL-KEY message
------------------------------------------------------------------------------------------------*/
static int Message_EqualReplayCounter(LARGE_INTEGER li1, OCTET_STRING f)
{
	LARGE_INTEGER li2;
	Message_ReplayCounter_OC2LI(f, &li2);
	if (li1.field.HighPart == li2.field.HighPart && li1.field.LowPart == li2.field.LowPart)
		return 1;
	else
		return 0;
}
#endif

#ifdef CLIENT_MODE
/*-------------------------------------------------------------------------------------------
	li1 is recorded replay counter on STA
	f is the replay counter from EAPOL-KEY message
---------------------------------------------------------------------------------------------*/
static int Message_SmallerEqualReplayCounter(LARGE_INTEGER li1, OCTET_STRING f)
{
	LARGE_INTEGER li2;
	Message_ReplayCounter_OC2LI(f, &li2);
	if (li2.field.HighPart > li1.field.HighPart)
		return 0;
	else if (li2.field.HighPart < li1.field.HighPart)
		return 1;
	else if (li2.field.LowPart > li1.field.LowPart)
		return 0;
	else if (li2.field.LowPart <= li1.field.LowPart)
		return 1;
	else
		return 0;
}
#endif

/*---------------------------------------------------------------------------------------------
	li1 is recorded replay counter on STA
	f is the replay counter from EAPOL-KEY message
-----------------------------------------------------------------------------------------------*/
static int Message_LargerReplayCounter(LARGE_INTEGER li1, OCTET_STRING f)
{
	LARGE_INTEGER li2;
	Message_ReplayCounter_OC2LI(f, &li2);

	if (li2.field.HighPart > li1.field.HighPart)
		return 1;
	else if (li2.field.LowPart > li1.field.LowPart)
		return 1;
	else
		return 0;
}

static void Message_setReplayCounter(OCTET_STRING f, unsigned int h, unsigned int l)
{
	LARGE_INTEGER *li = (LARGE_INTEGER *)(f.Octet + ReplayCounterPos);
	li->charData[0] = (unsigned char)(h >> 24) & 0xff;
	li->charData[1] = (unsigned char)(h >> 16) & 0xff;
	li->charData[2] = (unsigned char)(h >>  8) & 0xff;
	li->charData[3] = (unsigned char)(h >>  0) & 0xff;
	li->charData[4] = (unsigned char)(l >> 24) & 0xff;
	li->charData[5] = (unsigned char)(l >> 16) & 0xff;
	li->charData[6] = (unsigned char)(l >>  8) & 0xff;
	li->charData[7] = (unsigned char)(l >>  0) & 0xff;
}

void ConstructIE(struct rtl8192cd_priv *priv, unsigned char *pucOut, int *usOutLen)
{
	DOT11_RSN_IE_HEADER dot11RSNIEHeader = { 0 };
	DOT11_RSN_IE_SUITE dot11RSNGroupSuite;
#ifdef CONFIG_IEEE80211W
	DOT11_RSN_IE_SUITE dot11RSNIGTKSuite;
#endif
	DOT11_RSN_IE_COUNT_SUITE *pDot11RSNPairwiseSuite = NULL;
	DOT11_RSN_IE_COUNT_SUITE *pDot11RSNAuthSuite = NULL;
	unsigned short usSuitCount;
	unsigned int ulIELength = 0;
	unsigned int ulIndex = 0;
	unsigned int ulPairwiseLength = 0;
	unsigned int ulAuthLength = 0;
	unsigned char *pucBlob;
	DOT11_RSN_IE_COUNT_SUITE countSuite, authCountSuite;
#ifdef RTL_WPA2
	DOT11_RSN_CAPABILITY dot11RSNCapability = { 0 };
	unsigned int uCipherAlgo = 0;
	int bCipherAlgoEnabled = FALSE;
	unsigned int uAuthAlgo = 0;
	int bAuthAlgoEnabled = FALSE;
	unsigned int ulRSNCapabilityLength = 0;
#endif

	unsigned char my_11w = priv->pmib->dot1180211AuthEntry.dot11IEEE80211W;
	unsigned char my_sha256 = priv->pmib->dot1180211AuthEntry.dot11EnableSHA256;
	unsigned char my_enable_psk = priv->pmib->dot1180211AuthEntry.dot11EnablePSK;
#ifdef CONFIG_IEEE80211W_CLI
	unsigned char peer_11w = priv->bss_support_pmf;
#endif
#ifdef AUTH_SAE_STA
	unsigned char pmkid_ie_len=0;
	struct stat_info *pstat=NULL;
	struct pmk_suite pmk_ie_suite;
	unsigned int t_stamp_1 = priv->pmib->dot11Bss.t_stamp[1];
	dump_security_mib(priv);
#endif
	*usOutLen = 0;
	/*----Construct WPA IE------begin*/
	if ( priv->pmib->dot1180211AuthEntry.dot11EnablePSK & PSK_WPA) {
		//
		// Construct Information Header
		//
		dot11RSNIEHeader.ElementID = RSN_ELEMENT_ID;
		dot11RSNIEHeader.OUI[0] = 0x00;
		dot11RSNIEHeader.OUI[1] = 0x50;
		dot11RSNIEHeader.OUI[2] = 0xf2;
		dot11RSNIEHeader.OUI[3] = 0x01;
		dot11RSNIEHeader.Version = cpu_to_le16(RSN_VER1);
		ulIELength += sizeof(DOT11_RSN_IE_HEADER);

		// Construct Cipher Suite:
		// - Multicast Suite:
		memset(&dot11RSNGroupSuite, 0, sizeof(dot11RSNGroupSuite));
		dot11RSNGroupSuite.OUI[0] = 0x00;
		dot11RSNGroupSuite.OUI[1] = 0x50;
		dot11RSNGroupSuite.OUI[2] = 0xF2;
		dot11RSNGroupSuite.Type = priv->wpa_global_info->MulticastCipher;
		ulIELength += sizeof(DOT11_RSN_IE_SUITE);

		// - UnicastSuite
		pDot11RSNPairwiseSuite = &countSuite;
		memset(pDot11RSNPairwiseSuite, 0, sizeof(DOT11_RSN_IE_COUNT_SUITE));
		usSuitCount = 0;

		for (ulIndex = 0; ulIndex < priv->wpa_global_info->NumOfUnicastCipher; ulIndex++) {
			pDot11RSNPairwiseSuite->dot11RSNIESuite[usSuitCount].OUI[0] = 0x00;
			pDot11RSNPairwiseSuite->dot11RSNIESuite[usSuitCount].OUI[1] = 0x50;
			pDot11RSNPairwiseSuite->dot11RSNIESuite[usSuitCount].OUI[2] = 0xF2;
			pDot11RSNPairwiseSuite->dot11RSNIESuite[usSuitCount].Type = priv->wpa_global_info->UnicastCipher[ulIndex];
			usSuitCount++;
		}

		pDot11RSNPairwiseSuite->SuiteCount = cpu_to_le16(usSuitCount);
		ulPairwiseLength = sizeof(pDot11RSNPairwiseSuite->SuiteCount) + usSuitCount * sizeof(DOT11_RSN_IE_SUITE);
		ulIELength += ulPairwiseLength;

		//
		// Construction of Auth Algo List
		//
		pDot11RSNAuthSuite = &authCountSuite;
		memset(pDot11RSNAuthSuite, 0, sizeof(DOT11_RSN_IE_COUNT_SUITE));
		usSuitCount = 0;
		pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].OUI[0] = 0x00;
		pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].OUI[1] = 0x50;
		pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].OUI[2] = 0xF2;
		pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].Type = DOT11_AuthKeyType_PSK;
		usSuitCount++;

		pDot11RSNAuthSuite->SuiteCount = cpu_to_le16(usSuitCount);
		ulAuthLength = sizeof(pDot11RSNAuthSuite->SuiteCount) + usSuitCount * sizeof(DOT11_RSN_IE_SUITE);
		ulIELength += ulAuthLength;

		//---------------------------------------------------------------------------------------------
		// Do not encapsulate capability field to solve TI WPA issue
		//---------------------------------------------------------------------------------------------
		/*
		    dot11RSNCapability.field.PreAuthentication = 0;//auth->RSNVariable.isSupportPreAuthentication
		    dot11RSNCapability.field.PairwiseAsDefaultKey = auth->RSNVariable.isSupportPairwiseAsDefaultKey;
		    switch(auth->RSNVariable.NumOfRxTSC)
		    {
		    case 1:
		        dot11RSNCapability.field.NumOfReplayCounter = 0;
		    	break;
		case 2:
			dot11RSNCapability.field.NumOfReplayCounter = 1;
			break;
		case 4:
			dot11RSNCapability.field.NumOfReplayCounter = 2;
			break;
		case 16:
			dot11RSNCapability.field.NumOfReplayCounter = 3;
		    	break;
		    default:
			dot11RSNCapability.field.NumOfReplayCounter = 0;
		    }

		    ulRSNCapabilityLength = sizeof(DOT11_RSN_CAPABILITY);
		    ulIELength += ulRSNCapabilityLength;
		*/

		pucBlob = pucOut;
		pucBlob += sizeof(DOT11_RSN_IE_HEADER);
		memcpy(pucBlob, &dot11RSNGroupSuite, sizeof(DOT11_RSN_IE_SUITE));
		pucBlob += sizeof(DOT11_RSN_IE_SUITE);
		memcpy(pucBlob, pDot11RSNPairwiseSuite, ulPairwiseLength);
		pucBlob += ulPairwiseLength;
		memcpy(pucBlob, pDot11RSNAuthSuite, ulAuthLength);
		pucBlob += ulAuthLength;

		*usOutLen = (int)ulIELength;
		pucBlob = pucOut;
		dot11RSNIEHeader.Length = (unsigned char)ulIELength - 2; //This -2 is to minus elementID and Length in OUI header
		memcpy(pucBlob, &dot11RSNIEHeader, sizeof(DOT11_RSN_IE_HEADER));

	}
	/*----Construct WPA IE------End*/

	/*----Construct WPA2 , WPA3 IE------Begin*/
#ifdef RTL_WPA2
	if ( (priv->pmib->dot1180211AuthEntry.dot11EnablePSK & PSK_WPA2)
#ifdef AUTH_SAE
			|| support_wpa3(priv)
#endif
	) {
		DOT11_WPA2_IE_HEADER dot11WPA2IEHeader = { 0 };
		ulIELength = 0;
		ulIndex = 0;
		ulPairwiseLength = 0;
		uCipherAlgo = 0;
		bCipherAlgoEnabled = FALSE;
		ulAuthLength = 0;
		uAuthAlgo = 0;
		bAuthAlgoEnabled = FALSE;
		ulRSNCapabilityLength = 0;

		//
		// Construct Information Header
		//
		dot11WPA2IEHeader.ElementID = WPA2_ELEMENT_ID;
		dot11WPA2IEHeader.Version = cpu_to_le16(RSN_VER1);
		ulIELength += sizeof(DOT11_WPA2_IE_HEADER);

		// Construct Cipher Suite:
		//      - Multicast Suite:
		//
		memset(&dot11RSNGroupSuite, 0, sizeof(dot11RSNGroupSuite));
		dot11RSNGroupSuite.OUI[0] = 0x00;
		dot11RSNGroupSuite.OUI[1] = 0x0F;
		dot11RSNGroupSuite.OUI[2] = 0xAC;
		dot11RSNGroupSuite.Type = priv->wpa_global_info->MulticastCipher;
		log("[WPA2]M Cipher =[%d]", dot11RSNGroupSuite.Type);
		ulIELength += sizeof(DOT11_RSN_IE_SUITE);

		//      - UnicastSuite
		pDot11RSNPairwiseSuite = &countSuite;
		memset(pDot11RSNPairwiseSuite, 0, sizeof(DOT11_RSN_IE_COUNT_SUITE));
		usSuitCount = 0;

		for (ulIndex = 0; ulIndex < priv->wpa_global_info->NumOfUnicastCipherWPA2; ulIndex++) {
			pDot11RSNPairwiseSuite->dot11RSNIESuite[usSuitCount].OUI[0] = 0x00;
			pDot11RSNPairwiseSuite->dot11RSNIESuite[usSuitCount].OUI[1] = 0x0F;
			pDot11RSNPairwiseSuite->dot11RSNIESuite[usSuitCount].OUI[2] = 0xAC;
			pDot11RSNPairwiseSuite->dot11RSNIESuite[usSuitCount].Type = priv->wpa_global_info->UnicastCipherWPA2[ulIndex];
			usSuitCount++;
		}

		pDot11RSNPairwiseSuite->SuiteCount = cpu_to_le16(usSuitCount);
		ulPairwiseLength = sizeof(pDot11RSNPairwiseSuite->SuiteCount) + usSuitCount * sizeof(DOT11_RSN_IE_SUITE);
		ulIELength += ulPairwiseLength;

		/*Construction of AKM suite
			Authentiction Key Management suite
		*/

		pDot11RSNAuthSuite = &authCountSuite;
		memset(pDot11RSNAuthSuite, 0, sizeof(DOT11_RSN_IE_COUNT_SUITE));
		usSuitCount = 0;
		pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].OUI[0] = 0x00;
		pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].OUI[1] = 0x0F;
		pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].OUI[2] = 0xAC;


#ifdef CONFIG_IEEE80211W_CLI
		if(OPMODE & WIFI_STATION_STATE) {
			log("DUT's PMF[%d],Peer's PMF[%d]",my_11w,peer_11w);

			if ((my_11w == REQ_PMF)	&& (peer_11w == REQ_PMF)) {
				/*8*/
				#ifdef AUTH_SAE_STA
				if (support_wpa3(priv)&&peer_support_wpa3(priv,t_stamp_1)){
					pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].Type = AKM_SAE_PSK;
					log("11w required + wpa3");
				}else
				#endif
				{
					/*6 ; non WPA3 , PMF*/
					pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].Type = AKM_PSK_SHA256;
					log("11w required + non wpa3");
				}

			}else
			if ( ((my_11w == REQ_PMF)&& (peer_11w == OPT_PMF))||
					((my_11w == OPT_PMF)&& (peer_11w == REQ_PMF)) ||
					((my_11w == OPT_PMF)&& (peer_11w == OPT_PMF)))
			{

				#ifdef AUTH_SAE_STA
				if (support_wpa3(priv)&&peer_support_wpa3(priv,t_stamp_1)){
					/*8*/
					pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].Type = AKM_SAE_PSK;
					log("peer support wpa3(Sha256 mandatory)");
				}
				else
				#endif
				{
					/* 2 ; peer 11w optional,DUT 11w required  use normal psk*/
					log("use AKM=2 #1");
					pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].Type = AKM_PSK;
				}

			}
			else {
				if (priv->bss_support_akmp & BIT(AKM_PSK_SHA256)) {
					/* 6 */
					pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].Type = AKM_PSK_SHA256;
					my_sha256 = 1;

				} else {	//if((priv->bss_support_akmp & BIT(AKM_PSK)) || (priv->bss_support_akmp == 0)) {
					/* 2 */
					pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].Type = AKM_PSK;
					log("use AKM=2 #2");
				}
			}
		}
		else
#endif
		{

#ifdef AUTH_SAE
			if(support_wpa3(priv) == WPA3_ONLY){
				/*8 only*/
				pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].Type = DOT11_AuthKeyType_SAE;
				log("8 only");
			}
			else if(support_wpa3(priv) == WPA3_WPA2_BOTH){
				/*2+8*/
				pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].Type = DOT11_AuthKeyType_PSK;
				usSuitCount++;
				/*next AKM suite */
				pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].OUI[0] = 0x00;
				pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].OUI[1] = 0x0F;
				pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].OUI[2] = 0xAC;
				pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].Type = DOT11_AuthKeyType_SAE;
				log("2+8");
			}else
#endif
#ifdef CONFIG_IEEE80211W
			if (priv->pmib->dot1180211AuthEntry.dot11IEEE80211W == MGMT_FRAME_PROTECTION_OPTIONAL &&
				 priv->pmib->dot1180211AuthEntry.dot11EnableSHA256 == 1){
				/*2+6*/
				pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].Type = DOT11_AuthKeyType_PSK;

				/*next AKM suite */
				usSuitCount++;
				pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].OUI[0] = 0x00;
				pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].OUI[1] = 0x0F;
				pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].OUI[2] = 0xAC;
				pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].Type = DOT11_AuthKeyType_PSK_SHA256;
				log("2+6");
			}else if(priv->pmib->dot1180211AuthEntry.dot11IEEE80211W == MGMT_FRAME_PROTECTION_REQUIRED){
				/*6 only*/
				pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].Type = DOT11_AuthKeyType_PSK_SHA256;
				log("6 only");
			}
			else
#endif
			{
				/*2 only*/
				pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].Type = DOT11_AuthKeyType_PSK;
				log("2 only");
			}
		}

#ifdef CONFIG_IEEE80211R
		if (FT_ENABLE && (OPMODE & WIFI_AP_STATE)) {
			/*next AKM suite */
			usSuitCount++;
			pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].OUI[0] = 0x00;
			pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].OUI[1] = 0x0F;
			pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].OUI[2] = 0xAC;
			pDot11RSNAuthSuite->dot11RSNIESuite[usSuitCount].Type = DOT11_AuthKeyType_FTPSK;
		}
#endif

		usSuitCount++;/*AKM cnt*/
		pDot11RSNAuthSuite->SuiteCount = cpu_to_le16(usSuitCount);
		ulAuthLength = sizeof(pDot11RSNAuthSuite->SuiteCount) + usSuitCount * sizeof(DOT11_RSN_IE_SUITE);
		ulIELength += ulAuthLength;


		/*---------Begin of Construction of RSN Capability ---------*/
		dot11RSNCapability.field.PreAuthentication = 0;

#ifdef CONFIG_IEEE80211W
		//Protected Managemenet Protection Capability (PMF)
		//printk("ConstructIE, dot11IEEE80211W=%d\n", priv->pmib->dot1180211AuthEntry.dot11IEEE80211W);
		if (priv->pmib->dot1180211AuthEntry.dot11IEEE80211W == NO_MGMT_FRAME_PROTECTION) {
			dot11RSNCapability.field.MFPC = 0;
			dot11RSNCapability.field.MFPR = 0;
		} else if (priv->pmib->dot1180211AuthEntry.dot11IEEE80211W == MGMT_FRAME_PROTECTION_OPTIONAL)
			dot11RSNCapability.field.MFPC = 1; // MFPC
		else if (priv->pmib->dot1180211AuthEntry.dot11IEEE80211W == MGMT_FRAME_PROTECTION_REQUIRED) {
			dot11RSNCapability.field.MFPR = 1; // MFPR
			dot11RSNCapability.field.MFPC = 1; // MFPC
		}
#endif

		ulRSNCapabilityLength = sizeof(DOT11_RSN_CAPABILITY);
		ulIELength += ulRSNCapabilityLength;
		/*---Capability---*/

#ifdef AUTH_SAE
		/*---PMKID cnt + PMKID lists---*/
		if (OPMODE & WIFI_STATION_STATE) {
			pstat = get_stainfo(priv,priv->pmib->dot11Bss.bssid);
			if(pstat && (pmkid_cached(pstat))){
				log("DUT has peer's pmkid cache, idx[%d]",pstat->pmkid_caching_idx);
				memset(&pmk_ie_suite,0,sizeof(struct pmk_suite));
				pmk_ie_suite.pmk_id_cnt=ieee_order(1);
				memcpy( pmk_ie_suite.pmk_list ,pstat->sae_pmkid,LEN_PMKID);
				//dump_hex("pmkid A",pstat->sae_pmkid,LEN_PMKID);
				//dump_hex("pmkid B",pmk_ie_suite.pmk_list,LEN_PMKID);
				pmkid_ie_len= sizeof(struct pmk_suite);
				ulIELength += pmkid_ie_len;
			}
			else {
				ulIELength += 2;	// add PMDID Count (2bytes)
			}
		} else {
			ulIELength += 2;	// add PMDID Count (2bytes)
		}
#endif

#ifdef CONFIG_IEEE80211W
		/*IGTK Suite ; Construct Group Management Cipher Suite*/
		{
			if ((priv->pmib->dot1180211AuthEntry.dot11IEEE80211W &&
				priv->pmib->dot1180211AuthEntry.dot11EnableSHA256 == TRUE) ) {
				memset(&dot11RSNIGTKSuite, 0, sizeof(dot11RSNIGTKSuite));
				dot11RSNIGTKSuite.OUI[0] = 0x00;
				dot11RSNIGTKSuite.OUI[1] = 0x0F;
				dot11RSNIGTKSuite.OUI[2] = 0xAC;
				dot11RSNIGTKSuite.Type = DOT11_ENC_BIP;
				ulIELength += sizeof(dot11RSNIGTKSuite);
			}
		}
#endif

		/*Copy all above WPA2 IE to pucBlob here--------Begin*/
		pucBlob = pucOut + *usOutLen;
		pucBlob += sizeof(DOT11_WPA2_IE_HEADER);
		memcpy(pucBlob, &dot11RSNGroupSuite, sizeof(DOT11_RSN_IE_SUITE));
		pucBlob += sizeof(DOT11_RSN_IE_SUITE);
		memcpy(pucBlob, pDot11RSNPairwiseSuite, ulPairwiseLength);
		pucBlob += ulPairwiseLength;
		memcpy(pucBlob, pDot11RSNAuthSuite, ulAuthLength);
		pucBlob += ulAuthLength;
		memcpy(pucBlob, &dot11RSNCapability, ulRSNCapabilityLength);
		pucBlob += ulRSNCapabilityLength;

		/*---PMKID + PMKID LISTs---*/
#ifdef AUTH_SAE
		if (OPMODE & WIFI_STATION_STATE) {
			if(pmkid_ie_len == sizeof(struct pmk_suite)){
				/*has PMKIS list*/
				log("include pmk suite");
				memcpy(pucBlob, &pmk_ie_suite, sizeof(struct pmk_suite));
				pucBlob += sizeof(struct pmk_suite);
			}
			else {
				pucBlob += 2;  // add PMDID Count (2bytes)
			}
		} else {
			pucBlob += 2; // add PMDID Count (2bytes)
		}
#endif
#ifdef CONFIG_IEEE80211W
		/*---IGTK---*/
		{
			if (priv->pmib->dot1180211AuthEntry.dot11IEEE80211W &&
			priv->pmib->dot1180211AuthEntry.dot11EnableSHA256 == TRUE) {
				memcpy(pucBlob, &dot11RSNIGTKSuite, sizeof(DOT11_RSN_IE_SUITE));
			}
		}
#endif // CONFIG_IEEE80211W
		/*Copy all above WPA2 IE to pucBlob here-----END*/

		pucBlob = pucOut + *usOutLen;
		dot11WPA2IEHeader.Length = (unsigned char)ulIELength - 2; //This -2 is to minus elementID and Length in OUI header
		memcpy(pucBlob, &dot11WPA2IEHeader, sizeof(DOT11_WPA2_IE_HEADER));
		*usOutLen = *usOutLen + (int)ulIELength;
	}
#endif // RTL_WPA2

}

static void INCLargeInteger(LARGE_INTEGER * x)
{
	if (x->field.LowPart == 0xffffffff) {
		if (x->field.HighPart == 0xffffffff) {
			x->field.HighPart = 0;
			x->field.LowPart = 0;
		} else {
			x->field.HighPart++;
			x->field.LowPart = 0;
		}
	} else
		x->field.LowPart++;
}

static void INCOctet16_INTEGER(OCTET16_INTEGER * x)
{
	if (LargeIntegerOverflow(x->field.LowPart)) {
		if (LargeIntegerOverflow(x->field.HighPart)) {
			LargeIntegerZero(x->field.HighPart);
			LargeIntegerZero(x->field.LowPart);
		} else {
			INCLargeInteger(&x->field.HighPart);
			LargeIntegerZero(x->field.LowPart);
		}
	} else
		INCLargeInteger(&x->field.LowPart);

}

static OCTET32_INTEGER *INCOctet32_INTEGER(OCTET32_INTEGER * x)
{
	if (Octet16IntegerOverflow(x->field.LowPart)) {
		if (Octet16IntegerOverflow(x->field.HighPart)) {
			Octet16IntegerZero(x->field.HighPart);
			Octet16IntegerZero(x->field.LowPart);
		} else {
			INCOctet16_INTEGER(&x->field.HighPart);
			Octet16IntegerZero( x->field.LowPart);
		}
	} else
		INCOctet16_INTEGER(&x->field.LowPart);
	return x;
}

static void SetEAPOL_KEYIV(OCTET_STRING ocDst, OCTET32_INTEGER oc32Counter)
{
	unsigned char *ptr = ocDst.Octet;
	unsigned long ulTmp;

	if (ocDst.Length != 16 && ocDst.Length != 32) {
#ifdef DEBUG_PSK
		printk("PSK: SetEAPOL_KEYIV -> lendgth should be 16 or 32, len:=%d\n", ocDst.Length);
#endif
		return;
	}

	if (ocDst.Length == 32) {
		ulTmp = cpu_to_le32(oc32Counter.field.HighPart.field.HighPart.field.HighPart);
		memcpy(ptr, (char *)&ulTmp, sizeof(ulTmp));
		ptr += 4;

		ulTmp = cpu_to_le32(oc32Counter.field.HighPart.field.HighPart.field.LowPart);
		memcpy(ptr, (char *)&ulTmp, sizeof(ulTmp));
		ptr += 4;

		ulTmp = cpu_to_le32(oc32Counter.field.HighPart.field.LowPart.field.HighPart);
		memcpy(ptr, (char *)&ulTmp, sizeof(ulTmp));
		ptr += 4;

		ulTmp = cpu_to_le32(oc32Counter.field.HighPart.field.LowPart.field.LowPart);
		memcpy(ptr, (char *)&ulTmp, sizeof(ulTmp));
		ptr += 4;
	}

	// for AES, 16 bytes should get lowpart
	ulTmp = cpu_to_le32(oc32Counter.field.LowPart.field.HighPart.field.HighPart);
	memcpy(ptr, (char *)&ulTmp, sizeof(ulTmp));
	ptr += 4;

	ulTmp = cpu_to_le32(oc32Counter.field.LowPart.field.HighPart.field.LowPart);
	memcpy(ptr, (char *)&ulTmp, sizeof(ulTmp));
	ptr += 4;

	ulTmp = cpu_to_le32(oc32Counter.field.LowPart.field.LowPart.field.HighPart);
	memcpy(ptr, (char *)&ulTmp, sizeof(ulTmp));
	ptr += 4;

	ulTmp = cpu_to_le32(oc32Counter.field.LowPart.field.LowPart.field.LowPart);
	memcpy(ptr, (char *)&ulTmp, sizeof(ulTmp));
}

static void EncGTK(struct rtl8192cd_priv *priv, struct stat_info *pstat,
				   unsigned char *kek, int keklen, unsigned char *key,
				   int keylen, unsigned char *out, unsigned short *outlen)
{

	unsigned char tmp1[257], tmp2[257];
	RC4_KEY	 *rc4key;
#ifdef RTL_WPA2
	unsigned char default_key_iv[] = { 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6 };
#endif
	OCTET_STRING EAPOLMsgSend;
	lib1x_eapol_key *eapolkey;

	rc4key = (RC4_KEY *)kmalloc(sizeof(RC4_KEY), GFP_ATOMIC);
	if (rc4key == NULL)
		return;

	EAPOLMsgSend.Octet = pstat->wpa_sta_info->EAPOLMsgSend.Octet;
	eapolkey = (lib1x_eapol_key *)(EAPOLMsgSend.Octet + ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN);

#ifdef CONFIG_IEEE80211R
	if (FT_ENABLE && pstat->wpa_sta_info->isFT && !priv->pmib->dot1180211AuthEntry.dot11EnablePSK) {
		AES_WRAP(key, keylen, default_key_iv, 8, kek, keklen, out, outlen);
	} else
#endif
// should refer tx packet, david+2006-04-06
//	if (Message_KeyDescVer(pstat->wpa_sta_info->EapolKeyMsgRecvd) == key_desc_ver1) {
	if (Message_KeyDescVer(pstat->wpa_sta_info->EapolKeyMsgSend) == key_desc_ver1) {

		memcpy(tmp1, eapolkey->key_iv, KEY_IV_LEN);
		if (keklen <= (sizeof(tmp1) - KEY_IV_LEN))
			memcpy(tmp1 + KEY_IV_LEN, kek, keklen);

		RC4_set_key(rc4key, KEY_IV_LEN + keklen, tmp1);

		//first 256 bytes are discarded
		RC4(rc4key, 256, (unsigned char *)tmp1, (unsigned char *)tmp2);
		RC4(rc4key, keylen, (unsigned char *)key, out);
		*outlen = keylen;
	}
#ifdef RTL_WPA2
	else
		//according to p75 of 11i/D3.0, the IV should be put in the least significant octecs of
		//KeyIV field which shall be padded with 0, so eapolkey->key_iv + 8
		AES_WRAP(key, keylen, default_key_iv, 8, kek, keklen, out, outlen);
#endif
	kfree(rc4key);
}

static int CheckMIC(OCTET_STRING EAPOLMsgRecvd, unsigned char *key, int keylen)
{
	int retVal = 0;
	OCTET_STRING EapolKeyMsgRecvd;
	unsigned char ucAlgo;
	OCTET_STRING tmp; //copy of overall 802.1x message
	unsigned char tmpbuf[512];
	struct lib1x_eapol *tmpeapol;
	lib1x_eapol_key *tmpeapolkey;
	unsigned char sha1digest[20];

	EapolKeyMsgRecvd.Octet = EAPOLMsgRecvd.Octet +
							 ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN;
	EapolKeyMsgRecvd.Length = EAPOLMsgRecvd.Length -
							  (ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN);
	ucAlgo = Message_KeyDescVer(EapolKeyMsgRecvd);

	tmp.Length = EAPOLMsgRecvd.Length;
	tmp.Octet = tmpbuf;
	memcpy(tmp.Octet, EAPOLMsgRecvd.Octet, EAPOLMsgRecvd.Length);
	tmpeapol = (struct lib1x_eapol *)(tmp.Octet + ETHER_HDRLEN);
	tmpeapolkey = (lib1x_eapol_key *)(tmp.Octet + ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN);
	memset(tmpeapolkey->key_mic, 0, KEY_MIC_LEN);

	if (ucAlgo == key_desc_ver1) {
		hmac_md5((unsigned char*)tmpeapol, LIB1X_EAPOL_HDRLEN + ntohs(tmpeapol->packet_body_length) ,
				 key, keylen, tmpeapolkey->key_mic);
#if 0
		lib1x_hexdump2(MESS_DBG_KEY_MANAGE, "CheckMIC", EapolKeyMsgRecvd.Octet +
					   KeyMICPos, KEY_MIC_LEN, "Original");
		lib1x_hexdump2(MESS_DBG_KEY_MANAGE, "CheckMIC", tmpeapolkey->key_mic,
					   KEY_MIC_LEN, "Calculated");
#endif
		if (!memcmp(tmpeapolkey->key_mic, EapolKeyMsgRecvd.Octet + KeyMICPos, KEY_MIC_LEN))
			retVal = 1;
	} else if (ucAlgo == key_desc_ver2) {
		hmac_sha1((unsigned char*)tmpeapol, LIB1X_EAPOL_HDRLEN + ntohs(tmpeapol->packet_body_length) ,
					key, keylen, sha1digest);
		if (!memcmp(sha1digest, EapolKeyMsgRecvd.Octet + KeyMICPos, KEY_MIC_LEN))
			retVal = 1;
	}
#if defined(CONFIG_IEEE80211W) || defined(CONFIG_IEEE80211R)
	else if (ucAlgo == key_desc_ver3 || ucAlgo == key_desc_ver0)
	{
		omac1_aes_128(key, (unsigned char*)tmpeapol, LIB1X_EAPOL_HDRLEN + ntohs(tmpeapol->packet_body_length), tmpeapolkey->key_mic);
		if (!memcmp(tmpeapolkey->key_mic, EapolKeyMsgRecvd.Octet + KeyMICPos, KEY_MIC_LEN))
			retVal = 1;
	}
#endif
	return retVal;
}

static void CalcMIC(OCTET_STRING EAPOLMsgSend, int algo, unsigned char *key, int keylen)
{
	struct lib1x_eapol *eapol = (struct lib1x_eapol *)(EAPOLMsgSend.Octet + ETHER_HDRLEN);
	lib1x_eapol_key *eapolkey = (lib1x_eapol_key *)(EAPOLMsgSend.Octet + ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN);
	unsigned char sha1digest[20];

	memset(eapolkey->key_mic, 0, KEY_MIC_LEN);

	if (algo == key_desc_ver1)
		hmac_md5((unsigned char*)eapol, EAPOLMsgSend.Length - ETHER_HDRLEN ,
				 key, keylen, eapolkey->key_mic);
	else if (algo == key_desc_ver2) {
		hmac_sha1((unsigned char*)eapol, EAPOLMsgSend.Length - ETHER_HDRLEN ,
				  key, keylen, sha1digest);
		memcpy(eapolkey->key_mic, sha1digest, KEY_MIC_LEN);
	}
#if defined(CONFIG_IEEE80211W) || defined(CONFIG_IEEE80211R)
	else if (algo == key_desc_ver3 || algo == key_desc_ver0) {
		omac1_aes_128(key, (unsigned char*)eapol, EAPOLMsgSend.Length - ETHER_HDRLEN, eapolkey->key_mic);
	}
#endif
}

/* GTK-PRF-X
   X = 256 in TKIP
   X = 128 in CCMP, WRAP, and WEP
*/
static void CalcGTK(unsigned char *addr, unsigned char *nonce,
					unsigned char *keyin, int keyinlen,
					unsigned char *keyout, int keyoutlen, unsigned char *label)
{
	unsigned char data[ETHER_ADDRLEN + KEY_NONCE_LEN], tmp[64];

	memcpy(data, addr, ETHER_ADDRLEN);
	memcpy(data + ETHER_ADDRLEN, nonce, KEY_NONCE_LEN);
#ifdef CONFIG_IEEE80211W
	sha256_prf(keyin, keyinlen, label, data, sizeof(data), tmp, keyoutlen);
#else
	i_PRF(keyin, keyinlen, label,
		  GMK_EXPANSION_CONST_SIZE, data, sizeof(data),
		  tmp, keyoutlen);
#endif
	memcpy(keyout, tmp, keyoutlen);
}

static int MIN(unsigned char *ucStr1, unsigned char *ucStr2, unsigned int ulLen)
{
	int i;
	for (i = 0 ; i < ulLen ; i++) {
		if ((unsigned char)ucStr1[i] < (unsigned char)ucStr2[i])
			return -1;
		else if ((unsigned char)ucStr1[i] > (unsigned char)ucStr2[i])
			return 1;
		else if (i == ulLen - 1)
			return 0;
		else
			continue;
	}
	return 0;
}

static void CalcPTK(unsigned char *addr1, unsigned char *addr2,
					unsigned char *nonce1, unsigned char *nonce2,
					unsigned char * keyin, int keyinlen,
					unsigned char *keyout, int keyoutlen
#ifdef CONFIG_IEEE80211W
					, int use_sha256
#endif
				   )
{
	unsigned char data[2 * ETHER_ADDRLEN + 2 * KEY_NONCE_LEN], tmpPTK[128];

	if (MIN(addr1, addr2, ETHER_ADDRLEN) <= 0) {
		memcpy(data, addr1, ETHER_ADDRLEN);
		memcpy(data + ETHER_ADDRLEN, addr2, ETHER_ADDRLEN);
	} else {
		memcpy(data, addr2, ETHER_ADDRLEN);
		memcpy(data + ETHER_ADDRLEN, addr1, ETHER_ADDRLEN);
	}
	if (MIN(nonce1, nonce2, KEY_NONCE_LEN) <= 0) {
		memcpy(data + 2 * ETHER_ADDRLEN, nonce1, KEY_NONCE_LEN);
		memcpy(data + 2 * ETHER_ADDRLEN + KEY_NONCE_LEN, nonce2, KEY_NONCE_LEN);
	} else {
		memcpy(data + 2 * ETHER_ADDRLEN, nonce2, KEY_NONCE_LEN);
		memcpy(data + 2 * ETHER_ADDRLEN + KEY_NONCE_LEN, nonce1, KEY_NONCE_LEN);
	}
#ifdef CONFIG_IEEE80211W
	if (use_sha256) {
		//PMFDEBUG("sha256_prf\n");
		sha256_prf(keyin, keyinlen, (unsigned char*)PMK_EXPANSION_CONST, data, sizeof(data),
				   tmpPTK, keyoutlen);
	} else
#endif
	{

		i_PRF(keyin, keyinlen, (unsigned char*)PMK_EXPANSION_CONST,
			  PMK_EXPANSION_CONST_SIZE, data, sizeof(data),
			  tmpPTK, PTK_LEN_TKIP);
	}
	memcpy(keyout, tmpPTK, keyoutlen);
}

#ifdef CLIENT_MODE
/*
	decrypt WPA2 Message 3's Key Data
*/
// Use RC4 or AES to decode the keydata by checking desc-ver, david-2006-01-06
//int DecWPA2KeyData(u_char *key, int keylen, u_char *kek, int keklen, u_char *kout)
#if defined(__OSK__) && defined(CONFIG_WIFI_INCLUDE_WPA_PSK)
int DecWPA2KeyData_driver(WPA_STA_INFO* pStaInfo, unsigned char *key, int keylen, unsigned char *kek, int keklen, unsigned char *kout, int kout_len)
#else
int DecWPA2KeyData(WPA_STA_INFO* pStaInfo, unsigned char *key, int keylen, unsigned char *kek, int keklen, unsigned char *kout, int kout_len)
#endif
{
	int	retVal = 0;
	unsigned char		default_key_iv[] = { 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6 };
	unsigned char		tmp2[257];

// Use RC4 or AES to decode the keydata by checking desc-ver, david-2006-01-06
	unsigned char 	tmp1[257];
	RC4_KEY			*rc4key;

	lib1x_eapol_key *eapolkey = (lib1x_eapol_key *)(pStaInfo->EAPOLMsgRecvd.Octet + ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN);

	rc4key = (RC4_KEY *)kmalloc(sizeof(RC4_KEY), GFP_ATOMIC);
	if (rc4key == NULL)
		return 0;

	if (keylen > kout_len) {
		printk("%s Error: keylen exceeds!\n", __func__);
		goto out;
	}

#ifdef CONFIG_IEEE80211R_CLI
	if (pStaInfo->isFT && Message_KeyDescVer(pStaInfo->EapolKeyMsgRecvd) == key_desc_ver3) {
		AES_UnWRAP(key, keylen, kek, keklen, tmp2, sizeof(tmp2));
		if (memcmp(tmp2, default_key_iv, 8))
			retVal = 0;
		else {
			memcpy(kout, tmp2 + 8, keylen);
			retVal = 1;
		}
	}else
#endif
	if (Message_KeyDescVer(pStaInfo->EapolKeyMsgRecvd) == key_desc_ver1) {
		memcpy(tmp1, eapolkey->key_iv, KEY_IV_LEN);
		if (keklen > sizeof(tmp1) - KEY_IV_LEN) {
			printk("%s Error: keklen exceeds!\n", __func__);
			goto out;
		}
		memcpy(tmp1 + KEY_IV_LEN, kek, keklen);
		RC4_set_key(rc4key, KEY_IV_LEN + keklen, tmp1);

		//first 256 bits is discard
		RC4(rc4key, 256, (unsigned char*)tmp1, (unsigned char*)tmp2);

		//RC4(rc4key, keylen, eapol_key->key_data, global->skm_sm->GTK[Message_KeyIndex(global->EapolKeyMsgRecvd)]);
		RC4(rc4key, keylen, pStaInfo->EapolKeyMsgRecvd.Octet + KeyDataPos, (unsigned char*)tmp2);

		memcpy(kout, tmp2, keylen);
		//memcpy(&global->supp_kmsm->GTK[Message_KeyIndex(global->EapolKeyMsgRecvd)], tmp2, keylen);
		retVal = 1;
	} else {
//--------------------------------------------------------
		AES_UnWRAP(key, keylen, kek, keklen, tmp2, sizeof(tmp2));
		if (memcmp(tmp2, default_key_iv, 8))
			retVal = 0;
		else {
			memcpy(kout, tmp2 + 8, keylen);
			retVal = 1;
		}
	}
out:
	kfree(rc4key);
	return retVal;
}

#if !defined(__OSK__) || !defined(CONFIG_AUTH)
int DecGTK(OCTET_STRING EAPOLMsgRecvd, unsigned char *kek, int keklen, int keylen, unsigned char *kout, int kout_len)
{
	int		retVal = 0;
	unsigned char		tmp1[257], tmp2[257];
	RC4_KEY		*rc4key;
	lib1x_eapol_key * eapol_key = (lib1x_eapol_key *)(EAPOLMsgRecvd.Octet + ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN);
	OCTET_STRING	EapolKeyMsgRecvd;

	rc4key = (RC4_KEY *)kmalloc(sizeof(RC4_KEY), GFP_ATOMIC);
	if (rc4key == NULL)
		return 0;

	if (keylen > kout_len) {
		printk("%s Error: keylen exceeds!\n", __func__);
		goto out;
	}

	EapolKeyMsgRecvd.Octet = EAPOLMsgRecvd.Octet + ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN;
	EapolKeyMsgRecvd.Length = EAPOLMsgRecvd.Length - (ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN);

	if (Message_KeyDescVer(EapolKeyMsgRecvd) == key_desc_ver1) {
		memcpy(tmp1, eapol_key->key_iv, KEY_IV_LEN);
		if (keklen > sizeof(tmp1) - KEY_IV_LEN) {
			printk("%s Error: keklen exceeds!\n", __func__);
			goto out;
		}
		memcpy(tmp1 + KEY_IV_LEN, kek, keklen);
		RC4_set_key(rc4key, KEY_IV_LEN + keklen, tmp1);
		//first 256 bits is discard
		RC4(rc4key, 256, (unsigned char*)tmp1, (unsigned char*)tmp2);
		//RC4(rc4key, keylen, eapol_key->key_data, global->skm_sm->GTK[Message_KeyIndex(global->EapolKeyMsgRecvd)]);
		RC4(rc4key, keylen, EapolKeyMsgRecvd.Octet + KeyDataPos, (unsigned char*)tmp2);
		memcpy(kout, tmp2, keylen);
		//memcpy(&global->supp_kmsm->GTK[Message_KeyIndex(global->EapolKeyMsgRecvd)], tmp2, keylen);
		retVal = 1;
	} else if (Message_KeyDescVer(EapolKeyMsgRecvd) == key_desc_ver2
#ifdef CONFIG_IEEE80211W_CLI
		|| Message_KeyDescVer(EapolKeyMsgRecvd) == key_desc_ver3
#endif
	) {
		// kenny: should use default IV 0xA6A6A6A6A6A6A6A6
		unsigned char	default_key_iv[] = { 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6 };
// david, get key len from eapol packet
//			AES_UnWRAP(EapolKeyMsgRecvd.Octet + KeyDataPos, keylen + 8, kek, keklen, tmp2);

		keylen = Message_KeyDataLength(EapolKeyMsgRecvd);
		AES_UnWRAP(EapolKeyMsgRecvd.Octet + KeyDataPos, keylen, kek, keklen, tmp2, sizeof(tmp2));
//------------------------- 2005-08-01

		//if(memcmp(tmp2, eapol_key->key_iv + 8, 8))
		if (memcmp(tmp2, default_key_iv, 8))
			retVal = 0;
		else {
			//memcpy(kout, tmp2, keylen);
			//memcpy(global->supp_kmsm->GTK[Message_KeyIndex(global->EapolKeyMsgRecvd)], tmp2 + 8, keylen - 8);
			memcpy(kout, tmp2 + 8, ((keylen > GTK_LEN) ? GTK_LEN : keylen));
			retVal = 1;
		}
	}
out:
	kfree(rc4key);
	return retVal;
}
#endif //#ifndef __OSK__
#endif /* CLIENT_MODE */

static int parseIE(struct rtl8192cd_priv *priv, WPA_STA_INFO *pInfo,
				   unsigned char *pucIE, unsigned int ulIELength)
{
	unsigned short usSuitCount;
	DOT11_RSN_IE_HEADER *pDot11RSNIEHeader;
	DOT11_RSN_IE_SUITE *pDot11RSNIESuite;
	DOT11_RSN_IE_COUNT_SUITE *pDot11RSNIECountSuite;

	DOT11_RSN_CAPABILITY * pDot11RSNCapability = NULL;
	DEBUG_TRACE;

	if (ulIELength < sizeof(DOT11_RSN_IE_HEADER)) {
		DEBUG_WARN("parseIE err 1!\n");
		return ERROR_INVALID_RSNIE;
	}

	pDot11RSNIEHeader = (DOT11_RSN_IE_HEADER *)pucIE;
	if (le16_to_cpu(pDot11RSNIEHeader->Version) != RSN_VER1) {
		DEBUG_WARN("parseIE err 2!\n");
		return ERROR_UNSUPPORTED_RSNEVERSION;
	}

	if (pDot11RSNIEHeader->ElementID != RSN_ELEMENT_ID ||
			pDot11RSNIEHeader->Length != ulIELength - 2 ||
			pDot11RSNIEHeader->OUI[0] != 0x00 || pDot11RSNIEHeader->OUI[1] != 0x50 ||
			pDot11RSNIEHeader->OUI[2] != 0xf2 || pDot11RSNIEHeader->OUI[3] != 0x01 ) {
		DEBUG_WARN("parseIE err 3!\n");
		return ERROR_INVALID_RSNIE;
	}

	pInfo->RSNEnabled = PSK_WPA;	// wpa
	ulIELength -= sizeof(DOT11_RSN_IE_HEADER);
	pucIE += sizeof(DOT11_RSN_IE_HEADER);

	//----------------------------------------------------------------------------------
	// Multicast Cipher Suite processing
	//----------------------------------------------------------------------------------
	if (ulIELength < sizeof(DOT11_RSN_IE_SUITE))
		return 0;

	pDot11RSNIESuite = (DOT11_RSN_IE_SUITE *)pucIE;
	if (pDot11RSNIESuite->OUI[0] != 0x00 ||
			pDot11RSNIESuite->OUI[1] != 0x50 ||
			pDot11RSNIESuite->OUI[2] != 0xF2) {
		DEBUG_WARN("parseIE err 4!\n");
		return ERROR_INVALID_RSNIE;
	}

	if (pDot11RSNIESuite->Type > DOT11_ENC_WEP104) {
		DEBUG_WARN("parseIE err 5!\n");
		return ERROR_INVALID_MULTICASTCIPHER;
	}

	if (pDot11RSNIESuite->Type != priv->wpa_global_info->MulticastCipher) {
		DEBUG_WARN("parseIE err 6!\n");
		return ERROR_INVALID_MULTICASTCIPHER;
	}

#ifdef CONFIG_IEEE80211W
	if (priv->pmib->dot1180211AuthEntry.dot11IEEE80211W == MGMT_FRAME_PROTECTION_REQUIRED) {
		if (pDot11RSNIESuite->Type != DOT11_ENC_CCMP) {
			PMFDEBUG("Invalid WPA group cipher %d\n", pDot11RSNIESuite->Type);
			return ERROR_MGMT_FRAME_PROTECTION_VIOLATION;
		}
	}
#endif


	ulIELength -= sizeof(DOT11_RSN_IE_SUITE);
	pucIE += sizeof(DOT11_RSN_IE_SUITE);

	//----------------------------------------------------------------------------------
	// Pairwise Cipher Suite processing
	//----------------------------------------------------------------------------------
	if (ulIELength < 2 + sizeof(DOT11_RSN_IE_SUITE))
		return 0;

	pDot11RSNIECountSuite = (PDOT11_RSN_IE_COUNT_SUITE)pucIE;
	pDot11RSNIESuite = pDot11RSNIECountSuite->dot11RSNIESuite;
	usSuitCount = le16_to_cpu(pDot11RSNIECountSuite->SuiteCount);

	if (usSuitCount != 1 ||
			pDot11RSNIESuite->OUI[0] != 0x00 ||
			pDot11RSNIESuite->OUI[1] != 0x50 ||
			pDot11RSNIESuite->OUI[2] != 0xF2) {
		DEBUG_WARN("parseIE err 7!\n");
		return ERROR_INVALID_RSNIE;
	}

	if (pDot11RSNIESuite->Type > DOT11_ENC_WEP104) {
		DEBUG_WARN("parseIE err 8!\n");
		return ERROR_INVALID_UNICASTCIPHER;
	}

	if ((pDot11RSNIESuite->Type < DOT11_ENC_WEP40)
			|| (!(BIT(pDot11RSNIESuite->Type - 1) & priv->pmib->dot1180211AuthEntry.dot11WPACipher))) {
		DEBUG_WARN("parseIE err 9!\n");
		return ERROR_INVALID_UNICASTCIPHER;
	}

	pInfo->UnicastCipher = pDot11RSNIESuite->Type;

#ifdef DEBUG_PSK
	printk("PSK: ParseIE -> WPA UnicastCipher=%x\n", pInfo->UnicastCipher);
#endif

#ifdef CONFIG_IEEE80211W
	if (priv->pmib->dot1180211AuthEntry.dot11IEEE80211W == MGMT_FRAME_PROTECTION_REQUIRED) {
		if (pInfo->UnicastCipher == DOT11_ENC_TKIP) {
			PMFDEBUG("Management frame protection cannot use TKIP\n");
			return ERROR_MGMT_FRAME_PROTECTION_VIOLATION;
		}
	}
#endif
	ulIELength -= sizeof(pDot11RSNIECountSuite->SuiteCount) + sizeof(DOT11_RSN_IE_SUITE);
	pucIE += sizeof(pDot11RSNIECountSuite->SuiteCount) + sizeof(DOT11_RSN_IE_SUITE);

	//----------------------------------------------------------------------------------
	// Authentication suite
	//----------------------------------------------------------------------------------
	if (ulIELength < 2 + sizeof(DOT11_RSN_IE_SUITE))
		return 0;

	pDot11RSNIECountSuite = (PDOT11_RSN_IE_COUNT_SUITE)pucIE;
	pDot11RSNIESuite = pDot11RSNIECountSuite->dot11RSNIESuite;
	usSuitCount = le16_to_cpu(pDot11RSNIECountSuite->SuiteCount);

	if (usSuitCount != 1 ||
			pDot11RSNIESuite->OUI[0] != 0x00 ||
			pDot11RSNIESuite->OUI[1] != 0x50 ||
			pDot11RSNIESuite->OUI[2] != 0xF2 ) {
		DEBUG_WARN("parseIE err 10!\n");
		return ERROR_INVALID_RSNIE;
	}

#ifdef CONFIG_IEEE80211R
	if (FT_ENABLE && pInfo->isFT) {
		if (pDot11RSNIESuite->Type != DOT11_AuthKeyType_FTPSK) {
			DEBUG_WARN("ERROR_INVALID_AKMP !\n");
			return ERROR_INVALID_AKMP;
		}
	} else
#endif
	if ( pDot11RSNIESuite->Type < DOT11_AuthKeyType_RSN ||
			pDot11RSNIESuite->Type > DOT11_AuthKeyType_PSK) {
		DEBUG_WARN("parseIE err 11!\n");
		return ERROR_INVALID_AUTHKEYMANAGE;
	}

	if (pDot11RSNIESuite->Type != DOT11_AuthKeyType_PSK) {
		DEBUG_WARN("parseIE err 12!\n");
		return ERROR_INVALID_AUTHKEYMANAGE;
	}

	pInfo->AuthKeyMethod = pDot11RSNIESuite->Type;
	ulIELength -= sizeof(pDot11RSNIECountSuite->SuiteCount) + sizeof(DOT11_RSN_IE_SUITE);
	pucIE += sizeof(pDot11RSNIECountSuite->SuiteCount) + sizeof(DOT11_RSN_IE_SUITE);

	// RSN Capability
	if (ulIELength < sizeof(DOT11_RSN_CAPABILITY))
		return 0;

//#ifndef RTL_WPA2
#if 0
	//----------------------------------------------------------------------------------
	// Capability field
	//----------------------------------------------------------------------------------
	pDot11RSNCapability = (DOT11_RSN_CAPABILITY * )pucIE;
	pInfo->isSuppSupportPreAuthentication = pDot11RSNCapability->field.PreAuthentication;
	pInfo->isSuppSupportPairwiseAsDefaultKey = pDot11RSNCapability->field.PairwiseAsDefaultKey;

	switch (pDot11RSNCapability->field.NumOfReplayCounter) {
	case 0:
		pInfo->NumOfRxTSC = 1;
		break;
	case 1:
		pInfo->NumOfRxTSC = 2;
		break;
	case 2:
		pInfo->NumOfRxTSC = 4;
		break;
	case 3:
		pInfo->NumOfRxTSC = 16;
		break;
	default:
		pInfo->NumOfRxTSC = 1;
	}
#endif /* RTL_WPA2 */

#ifdef CONFIG_IEEE80211W
	pDot11RSNCapability = (DOT11_RSN_CAPABILITY * )pucIE;
	if (priv->pmib->dot1180211AuthEntry.dot11IEEE80211W == MGMT_FRAME_PROTECTION_REQUIRED) {
		if (!pDot11RSNCapability->field.MFPC) {
			PMFDEBUG("Management frame protection Required, but client did not enable it\n");
			return ERROR_MGMT_FRAME_PROTECTION_VIOLATION;
		}
	}

	if (priv->pmib->dot1180211AuthEntry.dot11IEEE80211W == NO_MGMT_FRAME_PROTECTION ||
			!(pDot11RSNCapability->field.MFPC))
		pInfo->mgmt_frame_prot = 0;
	else
		pInfo->mgmt_frame_prot = 1;

	PMFDEBUG("mgmt_frame_prot=%d\n", pInfo->mgmt_frame_prot);

#endif // CONFIG_IEEE80211W


	return 0;
}

#ifdef RTL_WPA2
static int parseIEWPA2(struct rtl8192cd_priv *priv, struct stat_info *pstat,
		WPA_STA_INFO *pInfo, unsigned char *pucIE, unsigned int ulIELength)
{
	unsigned short usSuitCount;
	DOT11_WPA2_IE_HEADER *pDot11WPA2IEHeader = NULL;
	DOT11_RSN_IE_SUITE  *pDot11RSNIESuite = NULL;
	DOT11_RSN_IE_COUNT_SUITE *pDot11RSNIECountSuite = NULL;
	DOT11_RSN_CAPABILITY *pDot11RSNCapability = NULL;

	DEBUG_TRACE;

	//dump_hex("Assoc-Req RSN",pucIE,ulIELength);
	//log("Uni Cipher=%d AKM=%d", pInfo->UnicastCipher, pInfo->AuthKeyMethod);

	if (ulIELength < sizeof(DOT11_WPA2_IE_HEADER)) {
		log("ERROR_INVALID_RSNIE, err 1");
		return ERROR_INVALID_RSNIE;
	}

	pDot11WPA2IEHeader = (DOT11_WPA2_IE_HEADER *)pucIE;
	if (le16_to_cpu(pDot11WPA2IEHeader->Version) != RSN_VER1) {
		log("ERROR_UNSUPPORTED_RSNEVERSION, err 2");
		return ERROR_UNSUPPORTED_RSNEVERSION;
	}

	if (pDot11WPA2IEHeader->ElementID != WPA2_ELEMENT_ID ||
			pDot11WPA2IEHeader->Length != ulIELength - 2 ) {
		log("ERROR_INVALID_RSNIE, err 3");
		return ERROR_INVALID_RSNIE;
	}

	pInfo->RSNEnabled = PSK_WPA2;
	pInfo->PMKCached = FALSE;

	ulIELength -= sizeof(DOT11_WPA2_IE_HEADER);
	pucIE += sizeof(DOT11_WPA2_IE_HEADER);

	//----------------------------------------------------------------------------------
	// Multicast Cipher Suite processing
	//----------------------------------------------------------------------------------
	if (ulIELength < sizeof(DOT11_RSN_IE_SUITE)){
		log("ERROR_INVALID_RSNIE, Mcast Suit truncated=%d", ulIELength);
		return ERROR_INVALID_RSNIE;
	}

	pDot11RSNIESuite = (DOT11_RSN_IE_SUITE *)pucIE;
	if (pDot11RSNIESuite->OUI[0] != 0x00 ||
			pDot11RSNIESuite->OUI[1] != 0x0F ||
			pDot11RSNIESuite->OUI[2] != 0xAC) {

		log("ERROR_INVALID_RSNIE, err 4");
		return ERROR_INVALID_RSNIE;
	}


	if (pDot11RSNIESuite->Type > DOT11_ENC_BIP){

		log("M cipher nosupport");
		return ERROR_INVALID_MULTICASTCIPHER;
	}

#ifdef CONFIG_IEEE80211R
	if (priv->pmib->dot1180211AuthEntry.dot11EnablePSK)
#endif
	if (pDot11RSNIESuite->Type != priv->wpa_global_info->MulticastCipher) {

		log("M Cipher mismatch,peer[%d] ,my[%d]",
			pDot11RSNIESuite->Type,priv->wpa_global_info->MulticastCipher);
		return ERROR_INVALID_MULTICASTCIPHER;
	}
#ifdef CONFIG_IEEE80211W
	if (priv->pmib->dot1180211AuthEntry.dot11IEEE80211W == MGMT_FRAME_PROTECTION_REQUIRED) {
		if (pDot11RSNIESuite->Type != DOT11_ENC_CCMP) {

			log("Invalid M Cipher [%d]", pDot11RSNIESuite->Type);
			return ERROR_MGMT_FRAME_PROTECTION_VIOLATION;
		}
	}
#endif

#ifdef CONFIG_IEEE80211R
	if (FT_ENABLE && !priv->pmib->dot1180211AuthEntry.dot11EnablePSK)
		pInfo->MulticastCipher_1x = pDot11RSNIESuite->Type;
#endif

	log("Mcast = 0x%x", pDot11RSNIESuite->Type);

	ulIELength -= sizeof(DOT11_RSN_IE_SUITE);
	pucIE += sizeof(DOT11_RSN_IE_SUITE);

	//----------------------------------------------------------------------------------
	// Pairwise Cipher Suite processing
	//----------------------------------------------------------------------------------
	if (ulIELength < (2 + sizeof(DOT11_RSN_IE_SUITE))){
		if(((ulIELength-2)%(sizeof(DOT11_RSN_IE_SUITE)))!=0){
			log("ERROR_INVALID_UNICASTCIPHER");
			return ERROR_INVALID_UNICASTCIPHER; //eric-wpa2 4.2.2 H
		}
		else{
			return 0;
		}
	}

	pDot11RSNIECountSuite = (PDOT11_RSN_IE_COUNT_SUITE)pucIE;
	pDot11RSNIESuite = pDot11RSNIECountSuite->dot11RSNIESuite;
	usSuitCount = le16_to_cpu(pDot11RSNIECountSuite->SuiteCount);

#ifdef WPA2_ENH //eric-wpa2 4.2.2 L
	if (ulIELength < (2 + (sizeof(DOT11_RSN_IE_SUITE)*usSuitCount))){
		//if(((ulIELength-2)%(sizeof(DOT11_RSN_IE_SUITE)))==0){
			//log("cnt=%d, but len=%d ok and return here",usSuitCount, ulIELength);
			//return 0;
		//}
		return ERROR_INVALID_UNICASTCIPHER;
	}
#endif

#ifdef WPA2_ENH //eric-wpa2 4.2.2 B, 4.2.2 D
	if(usSuitCount == 0){
		ulIELength -= sizeof(pDot11RSNIECountSuite->SuiteCount);
		pucIE += sizeof(pDot11RSNIECountSuite->SuiteCount);
		log("No Pairwise Suite, ERROR_INVALID_UNICASTCIPHER");
		return ERROR_INVALID_UNICASTCIPHER;
	}
	else
#endif
	{

		if((usSuitCount != 1
			#ifdef CONFIG_IEEE80211R
			&& FT_ENABLE == 0
			#endif
			) ||
			pDot11RSNIESuite->OUI[0] != 0x00 ||
			pDot11RSNIESuite->OUI[1] != 0x0F ||
			pDot11RSNIESuite->OUI[2] != 0xAC){
				log("ERROR_INVALID_RSNIE, err 7");
				return ERROR_INVALID_RSNIE;
		}

		if (pDot11RSNIESuite->Type > DOT11_ENC_WEP104) {
			log("ERROR_INVALID_UNICASTCIPHER, err 8");
			return ERROR_INVALID_UNICASTCIPHER;
		}

#ifdef CONFIG_IEEE80211R
		if (priv->pmib->dot1180211AuthEntry.dot11EnablePSK)
#endif
		if ((pDot11RSNIESuite->Type < DOT11_ENC_WEP40)
				|| (!(BIT(pDot11RSNIESuite->Type - 1) & priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher))) {
			log("ERROR_INVALID_UNICASTCIPHER, err 9");
			return ERROR_INVALID_UNICASTCIPHER;
		}

		pInfo->UnicastCipher = pDot11RSNIESuite->Type;
#ifdef CONFIG_IEEE80211W
		if (priv->pmib->dot1180211AuthEntry.dot11IEEE80211W == MGMT_FRAME_PROTECTION_REQUIRED)	{
			if (pInfo->UnicastCipher == DOT11_ENC_TKIP) {

				log("11W + TKIP not allowed");
				return ERROR_MGMT_FRAME_PROTECTION_VIOLATION;
			}
		}
#endif

		ulIELength -= sizeof(pDot11RSNIECountSuite->SuiteCount) + sizeof(DOT11_RSN_IE_SUITE);
		pucIE += sizeof(pDot11RSNIECountSuite->SuiteCount) + sizeof(DOT11_RSN_IE_SUITE);

#ifdef DEBUG_PSK
		printk("PSK: ParseIE -> WPA2 UnicastCipher=%x\n", pInfo->UnicastCipher);
#endif
	}
	//----------------------------------------------------------------------------------
	// Authentication suite
	//----------------------------------------------------------------------------------

	if (ulIELength < 2 + sizeof(DOT11_RSN_IE_SUITE)){
		if(((ulIELength-2)%(sizeof(DOT11_RSN_IE_SUITE)))!=0){
			log("len=%d, ERROR_INVALID_AUTHKEYMANAGE", ulIELength);
			return ERROR_INVALID_AUTHKEYMANAGE; //eric-wpa2 4.2.2 I
		}
		else {
			//log("len=%d, No AKM", ulIELength);
			return 0; //4.2.2 C
		}
	}

	pDot11RSNIECountSuite = (PDOT11_RSN_IE_COUNT_SUITE)pucIE;
	pDot11RSNIESuite = pDot11RSNIECountSuite->dot11RSNIESuite;
	usSuitCount = le16_to_cpu(pDot11RSNIECountSuite->SuiteCount);

#ifdef WPA2_ENH //wpa2 4.2.2 M
	if (ulIELength < 2 + (sizeof(DOT11_RSN_IE_SUITE)*usSuitCount)){
		if(((ulIELength-2)%(sizeof(DOT11_RSN_IE_SUITE)))==0)
			//log("AKM, cnt=%d len=%d skip & return", usSuitCount, ulIELength);
			return 0;
	}
#endif

#ifdef WPA2_ENH   //wpa2 4.2.2 D
	if(usSuitCount == 0){
		log("ERROR_INVALID_AUTHKEYMANAGE", ulIELength);
		return ERROR_INVALID_AUTHKEYMANAGE;
	}
	else
#endif
	{

		#ifdef CONFIG_IEEE80211R_CLI
		/*Don't check reassoc rsp in FT auth(will contain 2 akm suite types)*/
		if(priv->CliFTAuthState != state_ft_reassoc_rsp) {

		}else
		#endif
		if (usSuitCount != 1 ||
				pDot11RSNIESuite->OUI[0] != 0x00 ||
				pDot11RSNIESuite->OUI[1] != 0x0F ||
				pDot11RSNIESuite->OUI[2] != 0xAC ) {
			log("ERROR_INVALID_RSNIE, err 10");
			return ERROR_INVALID_RSNIE;
		}

#ifdef AUTH_SAE
		if(support_wpa3(priv) && pDot11RSNIESuite->Type == DOT11_AuthKeyType_SAE)
			pstat->is_sae_sta = 1;
		else if(support_wpa3(priv) == WPA3_ONLY) {
			log("ERROR_INVALID_AKMP");
			return ERROR_INVALID_AKMP;
		}
		else
#endif
#ifdef CONFIG_IEEE80211R
		if (FT_ENABLE && pInfo->isFT) {
			if (pDot11RSNIESuite->Type != DOT11_AuthKeyType_FTPSK &&
				pDot11RSNIESuite->Type != DOT11_AuthKeyType_FT8021x) {
				log("ERROR_INVALID_AKMP");
				return ERROR_INVALID_AKMP;
			}
		} else
#endif
		if (pDot11RSNIESuite->Type < DOT11_AuthKeyType_RSN ||
#ifdef CONFIG_IEEE80211W
				pDot11RSNIESuite->Type > DOT11_AuthKeyType_PSK_SHA256
#else
				pDot11RSNIESuite->Type > DOT11_AuthKeyType_PSK
#endif
		) {
			log("ERROR_INVALID_AUTHKEYMANAGE, err 11");
			return ERROR_INVALID_AUTHKEYMANAGE;
		}

#ifdef CONFIG_IEEE80211R
		if (priv->pmib->dot1180211AuthEntry.dot11EnablePSK)
#endif
		if (pDot11RSNIESuite->Type != DOT11_AuthKeyType_PSK
				&& pDot11RSNIESuite->Type != DOT11_AuthKeyType_FTPSK
				&& pDot11RSNIESuite->Type != DOT11_AuthKeyType_PSK_SHA256
				&& pDot11RSNIESuite->Type != DOT11_AuthKeyType_SAE
		){
			log("ERROR_INVALID_AUTHKEYMANAGE, err 12");
			return ERROR_INVALID_AUTHKEYMANAGE;
		}

		pInfo->AuthKeyMethod = pDot11RSNIESuite->Type;
		ulIELength -= sizeof(pDot11RSNIECountSuite->SuiteCount) + usSuitCount * sizeof(DOT11_RSN_IE_SUITE);
		pucIE += sizeof(pDot11RSNIECountSuite->SuiteCount) + usSuitCount * sizeof(DOT11_RSN_IE_SUITE);
		log("PSK: ParseIE -> WPA2 AKM=%x\n", pInfo->AuthKeyMethod);
	
	}

	// RSN Capability
	if (ulIELength < sizeof(DOT11_RSN_CAPABILITY)) {
		//log("Check RSN but len=%d ok", ulIELength);
		pInfo->NumOfRxTSC = 2;
		return 0;
	}

	//----------------------------------------------------------------------------------
	// Capability field
	//----------------------------------------------------------------------------------
	pDot11RSNCapability = (DOT11_RSN_CAPABILITY * )pucIE;
#if 0
	global->RSNVariable.isSuppSupportPreAuthentication = pDot11RSNCapability->field.PreAuthentication;
//#ifdef RTL_WPA2_PREAUTH  // kenny temp
	//wpa2_hexdump("WPA2 IE Capability", pucIE, 2);
	//global->RSNVariable.isSuppSupportPreAuthentication = (pDot11RSNCapability->charData[0] & 0x01)?TRUE:FALSE;
#endif

#if 1
	pInfo->NumOfRxTSC = 1;
#else
	global->RSNVariable.isSuppSupportPairwiseAsDefaultKey = pDot11RSNCapability->field.PairwiseAsDefaultKey;
	switch (pDot11RSNCapability->field.NumOfReplayCounter) {
	case 0:
		global->RSNVariable.NumOfRxTSC = 1;
		break;
	case 1:
		global->RSNVariable.NumOfRxTSC = 2;
		break;
	case 2:
		global->RSNVariable.NumOfRxTSC = 4;
		break;
	case 3:
		global->RSNVariable.NumOfRxTSC = 16;
		break;
	default:
		global->RSNVariable.NumOfRxTSC = 1;
	}
#endif


#ifdef CONFIG_IEEE80211W
	pDot11RSNCapability = (DOT11_RSN_CAPABILITY * )pucIE;
	if (priv->pmib->dot1180211AuthEntry.dot11IEEE80211W == MGMT_FRAME_PROTECTION_REQUIRED) {
		if (!pDot11RSNCapability->field.MFPC) {
			printk("Management frame protection Required, but client did not enable it\n");
			return ERROR_MGMT_FRAME_PROTECTION_VIOLATION;
		}
	}

	pInfo->rsn_cap_mfpr = pDot11RSNCapability->field.MFPR;
	pInfo->rsn_cap_mfpc = pDot11RSNCapability->field.MFPC;

	PMFDEBUG("peer MFPR=%d,MFPC=%d\n", pDot11RSNCapability->field.MFPR,pDot11RSNCapability->field.MFPC);

#ifdef AUTH_SAE
	if (((support_wpa3==WPA3_ONLY) || pstat->is_sae_sta) && (!pDot11RSNCapability->field.MFPC)) {
			printk("Management frame protection Required, but client did not enable it\n");
			return ERROR_MGMT_FRAME_PROTECTION_VIOLATION;
	}
#endif

	if (priv->pmib->dot1180211AuthEntry.dot11IEEE80211W == NO_MGMT_FRAME_PROTECTION ||
			!(pDot11RSNCapability->field.MFPC))
		pInfo->mgmt_frame_prot = 0;
	else
		pInfo->mgmt_frame_prot = 1;

	PMFDEBUG("mgmt_frame_prot=%d\n", pInfo->mgmt_frame_prot);

#endif // CONFIG_IEEE80211W


	pucIE += 2;
	ulIELength -= 2;

	// PMKID
	log("check PMKID,len=%d", ulIELength);

	if (ulIELength < 2)
		return 0;

	//----------------------------------------------------------------------------------
	// PMKID Count field
	//----------------------------------------------------------------------------------
	usSuitCount = le16_to_cpu(*((unsigned short *)pucIE));

	if(usSuitCount && (ulIELength < 2 + PMKID_LEN)){
		log("ERROR_INVALID_RSNIE,cnt=%d but len=%d", usSuitCount, ulIELength);
		return ERROR_INVALID_RSNIE;
	}

	//printf("PMKID Count = %d\n",usSuitCount);
	pucIE += 2;
	ulIELength -= 2;
#ifdef AUTH_SAE
	if (usSuitCount > 0) {
		int i;
		for (i=0; i < usSuitCount; i++) {
			pstat->pmkid_caching_idx = search_pmkid_cache(priv, pucIE+(PMKID_LEN*i));
			if (pmkid_cached(pstat)){
				log("found pmkid from cache \n");
				break;
			}
		}
	}
#endif
	/*
		if ( usSuitCount > 0) {
			struct _WPA2_PMKSA_Node* pmksa_node;
			int i;
			for (i=0; i < usSuitCount; i++) {
				pmksa_node = find_pmksa(pucIE+(PMKID_LEN*i));
				if ( pmksa_node != NULL) {
					//wpa2_hexdump("Cached PMKID found", pmksa_node->pmksa.pmkid, PMKID_LEN);
					global->RSNVariable.PMKCached = TRUE;
					global->RSNVariable.cached_pmk_node = pmksa_node;
					break;
				}
			}

		}
	*/

#ifdef CONFIG_IEEE80211W
	pucIE += PMKID_LEN * usSuitCount;
	log("cnt[%d],len[%d]", usSuitCount, ulIELength);
	ulIELength -= PMKID_LEN * usSuitCount;
	//----------------------------------------------------------------------------------
	// Group Management Cipher field (IGTK)
	//----------------------------------------------------------------------------------
	if ((ulIELength < sizeof(DOT11_RSN_IE_SUITE))) {
		//log("Check IGTK but ulIELength=%d return 0 ", ulIELength);
		return 0;
	}

	pDot11RSNIESuite = (DOT11_RSN_IE_SUITE*)pucIE;

	if (pDot11RSNIESuite->OUI[0] != 0x00 ||
			pDot11RSNIESuite->OUI[1] != 0x0F ||
			pDot11RSNIESuite->OUI[2] != 0xAC) {
		log("OUI chk fail");
		return ERROR_INVALID_RSNIE;
	}
	if (pDot11RSNIESuite->Type != DOT11_ENC_BIP) {
		log("ERROR_MGMT_FRAME_PROTECTION_VIOLATION ");
		return ERROR_MGMT_FRAME_PROTECTION_VIOLATION;
	}
#endif // CONFIG_IEEE80211W

	return 0;
}
#endif // RTL_WPA2

static void GenNonce(unsigned char *nonce, unsigned char *addr)
{
	unsigned char secret[256], random[256], result[256];

	get_random_bytes(random, 256);
	memset(secret, 0, sizeof(secret));

	i_PRF(secret, sizeof(secret), (unsigned char*)RANDOM_EXPANSION_CONST, RANDOM_EXPANSION_CONST_SIZE,
		  random, sizeof(random), result, KEY_NONCE_LEN);

	memcpy(nonce, result, KEY_NONCE_LEN);
}

static void IntegrityFailure(struct rtl8192cd_priv *priv)
{
	priv->wpa_global_info->IntegrityFailed = FALSE;

	if (priv->wpa_global_info->GKeyFailure) {
		priv->wpa_global_info->GTKRekey = TRUE;
		priv->wpa_global_info->GKeyFailure = FALSE;
	}

	//waitupto60;
	INCOctet32_INTEGER(&priv->wpa_global_info->Counter);
//	SetNonce(global->akm_sm->ANonce, global->auth->Counter);

	INCOctet32_INTEGER(&priv->wpa_global_info->Counter);
//	SetNonce(global->akm_sm->ANonce, global->auth->Counter);
}

static void ToDrv_RspAssoc(struct rtl8192cd_priv *priv, int id, unsigned char *mac, int status)
{
	DOT11_ASSOCIATIIN_RSP 	Association_Rsp;
	struct iw_point wrq;

	DEBUG_TRACE;

#ifdef DEBUG_PSK
	printk("PSK: Issue assoc-rsp [%x], mac:%02x:%02x:%02x:%02x:%02x:%02x\n",
		   status, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#endif

	wrq.pointer = (caddr_t)&Association_Rsp;
	wrq.length = sizeof(DOT11_ASSOCIATIIN_RSP);

	if (id == DOT11_EVENT_ASSOCIATION_IND)
		Association_Rsp.EventId = DOT11_EVENT_ASSOCIATION_RSP;
	else
		Association_Rsp.EventId = DOT11_EVENT_REASSOCIATION_RSP;

	Association_Rsp.IsMoreEvent = FALSE;
	Association_Rsp.Status = status;
	memcpy(Association_Rsp.MACAddr, mac, 6);

	rtl8192cd_ioctl_priv_req(priv->dev, &wrq);
}


static void ToDrv_RemovePTK(struct rtl8192cd_priv *priv, unsigned char *mac, int type)
{
	struct iw_point wrq;
	DOT11_DELETE_KEY Delete_Key;

#ifdef DEBUG_PSK
	printk("PSK: Remove PTK, mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
		   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#endif

	wrq.pointer = (caddr_t)&Delete_Key;
	wrq.length = sizeof(DOT11_DELETE_KEY);

	Delete_Key.EventId = DOT11_EVENT_DELETE_KEY;
	Delete_Key.IsMoreEvent = FALSE;
	Delete_Key.KeyType = type;
	memcpy(&Delete_Key.MACAddr, mac, 6);

	rtl8192cd_ioctl_priv_req(priv->dev, &wrq);
}

#ifdef CONFIG_IEEE80211W
static void ToDrv_SetPMF(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	int retVal = 0;
	struct iw_point wrq;
	DOT11_SET_11W_Flags flags;

	PMFDEBUG("Drv:setPMF=%d\n", pstat->wpa_sta_info->mgmt_frame_prot);
	flags.EventId = DOT11_EVENT_SET_PMF;
	flags.IsMoreEvent = FALSE;
	flags.isPMF = pstat->wpa_sta_info->mgmt_frame_prot;

	memcpy(flags.macAddr, pstat->cmn_info.mac_addr, 6);

	wrq.pointer = &flags;
	wrq.length = sizeof(flags);

	rtl8192cd_ioctl_priv_req(priv->dev, &wrq);
}

static void ToDrv_SetIGTK(struct rtl8192cd_priv *priv)
{
	int retVal = 0;
	u_long	ulKeyLength = 0;
	struct iw_point wrq;
	DOT11_SET_KEY  Set_Key;
	u_char	szBradcast[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	/*
	u_char	szDefaultKey[16] = {0x11, 0x11,0x11, 0x11,0x11, 0x11,0x11, 0x11,
					0x11, 0x11,0x11, 0x11,0x11, 0x11,0x11, 0x11};
	*/
	PMFDEBUG("\n");
	wrq.pointer = &Set_Key;

	Set_Key.EventId = DOT11_EVENT_SET_KEY;
	Set_Key.IsMoreEvent = FALSE;
	Set_Key.KeyIndex = priv->wpa_global_info->GN_igtk;
	Set_Key.KeyType = DOT11_KeyType_IGTK;
	memcpy(&Set_Key.MACAddr, szBradcast, 6);

	Set_Key.EncType = DOT11_ENC_BIP;
	//sc_yang
	memset(Set_Key.KeyMaterial, 0, 64);

	// Set IGTK
	memcpy(Set_Key.KeyMaterial ,
		   priv->wpa_global_info->IGTK[priv->wpa_global_info->GN_igtk - 4] ,
		   16);


	wrq.length = sizeof(DOT11_SET_KEY) - 1 + 16;

	rtl8192cd_ioctl_priv_req(priv->dev, &wrq);

}
#endif // CONFIG_IEEE80211W

#ifdef CONFIG_IEEE80211R
void FT_IndicateEvent(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char event, void *data)
{
	switch (event)
	{
		case DOT11_EVENT_FT_IMD_ASSOC_IND:
		{
			DOT11_FT_IMD_ASSOC_IND imd_assoc_ind;

			memset(&imd_assoc_ind, 0, sizeof(DOT11_FT_IMD_ASSOC_IND));

			imd_assoc_ind.EventId = event;
			imd_assoc_ind.IsMoreEvent = 0;
			memcpy(imd_assoc_ind.MACAddr, pstat->cmn_info.mac_addr, MACADDRLEN);

			DOT11_EnQueue((unsigned long)priv, priv->pftevent_queue, (unsigned char *)&imd_assoc_ind, sizeof(DOT11_FT_IMD_ASSOC_IND));
		}
		break;

		case DOT11_EVENT_FT_PULL_KEY_IND:
		{
			DOT11_FT_PULL_KEY_IND pull_key_ind;

			struct r1_key_holder *r1kh = (struct r1_key_holder *)data;
			if (!r1kh)
				return;

			memset(&pull_key_ind, 0, sizeof(DOT11_FT_PULL_KEY_IND));

			pull_key_ind.EventId = event;
			pull_key_ind.IsMoreEvent = 0;
			pull_key_ind.Type = FTKEY_TYPE_PULL;
			memcpy(pull_key_ind.r0kh_id, r1kh->r0kh_id, r1kh->r0kh_id_len);
			pull_key_ind.Length = sizeof(DOT11_FT_PULL_KEY_IND) - offsetof(DOT11_FT_PULL_KEY_IND, nonce);
			memset(pull_key_ind.nonce, 0, FT_R0KH_R1KH_PULL_NONCE_LEN);
			memcpy(pull_key_ind.pmk_r0_name, r1kh->pmk_r0_name, PMKID_LEN);
			memcpy(pull_key_ind.r1kh_id, r1kh->r1kh_id, MACADDRLEN);
			memcpy(pull_key_ind.s1kh_id, r1kh->sta_addr, MACADDRLEN);

			DOT11_EnQueue((unsigned long)priv, priv->pftevent_queue, (unsigned char *)&pull_key_ind, sizeof(DOT11_FT_PULL_KEY_IND));
		}
		break;

		case DOT11_EVENT_FT_ASSOC_IND:
		{
			DOT11_FT_ASSOC_IND ft_assoc_ind;

			memset(&ft_assoc_ind, 0, sizeof(DOT11_FT_ASSOC_IND));

			ft_assoc_ind.EventId = event;
			ft_assoc_ind.IsMoreEvent = 0;
			memcpy(ft_assoc_ind.MACAddr, pstat->cmn_info.mac_addr, MACADDRLEN);

			DOT11_EnQueue((unsigned long)priv, priv->pftevent_queue, (unsigned char *)&ft_assoc_ind, sizeof(DOT11_FT_ASSOC_IND));
		}
		break;

		case DOT11_EVENT_FT_KEY_EXPIRE_IND:
		{
			DOT11_FT_KEY_EXPIRE_IND key_expire_ind;
			unsigned char *sta_addr = (unsigned char *)data;

			memset(&key_expire_ind, 0, sizeof(DOT11_FT_KEY_EXPIRE_IND));
			key_expire_ind.EventId = event;
			key_expire_ind.IsMoreEvent = 0;
			memcpy(key_expire_ind.MACAddr, sta_addr, MACADDRLEN);

			DOT11_EnQueue((unsigned long)priv, priv->pftevent_queue, (unsigned char *)&key_expire_ind, sizeof(DOT11_FT_KEY_EXPIRE_IND));
		}
		break;

		case DOT11_EVENT_FT_ACTION_IND:
		{
			struct ft_action_param *param = (struct ft_action_param *)data;
			DOT11_FT_ACTION ft_action;

			memset(&ft_action, 0, sizeof(DOT11_FT_ACTION));

			ft_action.EventId = DOT11_EVENT_FT_ACTION_IND;
			ft_action.IsMoreEvent = 0;
			memcpy(ft_action.MACAddr, param->target_addr, MACADDRLEN);
			ft_action.ActionCode = param->action_code;
			ft_action.packet_len = param->length;
			memcpy(ft_action.packet, param->pdata, ft_action.packet_len);
			DOT11_EnQueue((unsigned long)priv, priv->pftevent_queue, (unsigned char*)&ft_action, sizeof(DOT11_FT_ACTION));
		}
		break;

		default:
			DEBUG_ERR("Not supported FT event : %d\n", event);
			break;
	}

	ft_event_indicate(priv);
}

#endif

#if defined(GENERAL_EVENT)
void general_IndicateEvent(struct rtl8192cd_priv *priv, unsigned char event, void *data)
{
	switch (event)
	{
#ifdef ROAMING_SUPPORT
		case DOT11_EVENT_ROAMING_STA_RSSI_ALARM:
		{
			DOT11_ROAMING_STA_RSSI_ALARM sta_rssi_alarm;
			struct stat_info *pstat = (struct stat_info *)data;
			memset(&sta_rssi_alarm, 0, sizeof(DOT11_ROAMING_STA_RSSI_ALARM));

			sta_rssi_alarm.EventId = event;
			memcpy(sta_rssi_alarm.hwaddr, pstat->cmn_info.mac_addr, MACADDRLEN);
			sta_rssi_alarm.rssi = pstat->rssi;
			sta_rssi_alarm.type = (pstat->roaming_indication == 1)? 0:1;
			if (priv->pshare->wlangeneral_pid > 0)
			{
				DOT11_EnQueue((unsigned long)priv, priv->pgeneralevent_queue, (unsigned char*)&sta_rssi_alarm, sizeof(DOT11_ROAMING_STA_RSSI_ALARM));
			}
		}
		break;

		case DOT11_EVENT_ROAMING_STA_INFO_REPORT:
		{
			DOT11_ROAMING_STA_INFO_REPORT sta_info_report;
			struct stat_info *pstat = (struct stat_info *)data;
			memset(&sta_info_report, 0, sizeof(DOT11_ROAMING_STA_INFO_REPORT));

			sta_info_report.EventId = event;
			sta_info_report.channel = priv->pmib->dot11RFEntry.dot11channel;
			memcpy(sta_info_report.hwaddr, pstat->cmn_info.mac_addr, MACADDRLEN);
#ifdef CONFIG_IEEE80211V
			sta_info_report.BSS_transmit_support = pstat->bssTransSupport;
#else
			sta_info_report.BSS_transmit_support = 0;
#endif
#ifdef DOT11K
			if(pstat->rm.rm_cap[0] & BIT(5))
				sta_info_report.beacon_report_support = 1;
			else
#endif
				sta_info_report.beacon_report_support = 0;
			DEBUG_INFO("[ROAMING] STA assoc MACaddr %02X%02X%02X%02X%02X%02X CH%d BSS_transmit_support:%d Bcn_report_support:%d\n",
			 	pstat->cmn_info.mac_addr[0],pstat->cmn_info.mac_addr[1],pstat->cmn_info.mac_addr[2],pstat->cmn_info.mac_addr[3],pstat->cmn_info.mac_addr[4],pstat->cmn_info.mac_addr[5],
			 	sta_info_report.channel,
			 	sta_info_report.BSS_transmit_support,
			 	sta_info_report.beacon_report_support
			  );
			if (priv->pshare->wlangeneral_pid > 0)
			{
				DOT11_EnQueue((unsigned long)priv, priv->pgeneralevent_queue, (unsigned char*)&sta_info_report, sizeof(DOT11_ROAMING_STA_INFO_REPORT));
			}
		}
		break;
#if 0
		case DOT11_EVENT_ROAMING_RSSI_QUERY_RESP:
		{

		}
		break;
#endif
		case DOT11_EVENT_ROAMING_BSS_TRANSMIT_RESP:
		{
			unsigned char* tmp = (unsigned char*)data;
			DOT11_ROAMING_BSS_TRANSMIT_RESP bss_tramsnit_resp;
			memset(&bss_tramsnit_resp, 0, sizeof(DOT11_ROAMING_BSS_TRANSMIT_RESP));
			bss_tramsnit_resp.EventId = event;
			memcpy(bss_tramsnit_resp.hwaddr, tmp+1, MACADDRLEN);
			bss_tramsnit_resp.resultCode = tmp[0];
			if (priv->pshare->wlangeneral_pid > 0)
			{
				DOT11_EnQueue((unsigned long)priv, priv->pgeneralevent_queue, (unsigned char*)&bss_tramsnit_resp, sizeof(DOT11_ROAMING_BSS_TRANSMIT_RESP));
			}
			DEBUG_INFO("[ROAMING] BSS trans_resp MACaddr: %02X%02X%02X%02X%02X%02X BSS_TRANSMIT_RESPONSE: %d\n", bss_tramsnit_resp.hwaddr[0],bss_tramsnit_resp.hwaddr[1],
					bss_tramsnit_resp.hwaddr[2],bss_tramsnit_resp.hwaddr[3],bss_tramsnit_resp.hwaddr[4],bss_tramsnit_resp.hwaddr[5], bss_tramsnit_resp.resultCode);
			switch(bss_tramsnit_resp.resultCode) {
				case WNM_BSS_TM_ACCEPT:
					DEBUG_INFO("Accept: WNM_BSS_TM_ACCEPT!\n");
					break;
				case WNM_BSS_TM_REJECT_UNSPECIFIED:
					DEBUG_INFO("Reject: WNM_BSS_TM_REJECT_UNSPECIFIED!\n");
					break;
				case WNM_BSS_TM_REJECT_INSUFFICIENT_BEACON:
					DEBUG_INFO("Reject: WNM_BSS_TM_REJECT_INSUFFICIENT_BEACON!\n");
					break;
				case WNM_BSS_TM_REJECT_INSUFFICIENT_CAPABITY:
					DEBUG_INFO("Reject: WNM_BSS_TM_REJECT_INSUFFICIENT_CAPABITY!\n");
					break;
				case WNM_BSS_TM_REJECT_UNDESIRED :
					DEBUG_INFO("Reject: WNM_BSS_TM_REJECT_UNDESIRED !\n");
					break;
				case WNM_BSS_TM_REJECT_DELAY_REQUEST :
					DEBUG_INFO("Reject: WNM_BSS_TM_REJECT_DELAY_REQUEST !\n");
					break;
				case WNM_BSS_TM_REJECT_STA_CANDIDATE_LIST_PROVIDED :
					DEBUG_INFO("Reject: WNM_BSS_TM_REJECT_STA_CANDIDATE_LIST_PROVIDED !\n");
					break;
				case WNM_BSS_TM_REJECT_NO_SUITABLE_CANDIDATES  :
					DEBUG_INFO("Reject: WNM_BSS_TM_REJECT_NO_SUITABLE_CANDIDATES  !\n");
					break;
				case WNM_BSS_TM_REJECT_LEAVING_ESS  :
					DEBUG_INFO("Reject: WNM_BSS_TM_REJECT_LEAVING_ESS  !\n");
					break;
				default:
					DEBUG_INFO("unknown type !\n");
					break;
			}
		}
		break;
#endif
#ifdef BEACON_VS_IE
		case EVENT_PROBE_RX_VSIE:
		{
			unsigned char* tmp = (unsigned char*)data;
			PROBE_VSIE_RECORD probe_vsie_record;
			memset(&probe_vsie_record, 0, sizeof(PROBE_VSIE_RECORD));
			probe_vsie_record.EventId = event;
			memcpy(probe_vsie_record.hwaddr, tmp, MACADDRLEN);
			probe_vsie_record.data_len = tmp[MACADDRLEN+1];
			memcpy(probe_vsie_record.ie_data, tmp + MACADDRLEN + 2, probe_vsie_record.data_len);
			DEBUG_INFO("[BEACON_VS_IE] Probe_req %02X%02X%02X%02X%02X%02X, OUI %02X-%02X-%02X, len %d\n",
					probe_vsie_record.hwaddr[0],probe_vsie_record.hwaddr[1],probe_vsie_record.hwaddr[2],
					probe_vsie_record.hwaddr[3],probe_vsie_record.hwaddr[4],probe_vsie_record.hwaddr[5],
					probe_vsie_record.ie_data[0],probe_vsie_record.ie_data[1],probe_vsie_record.ie_data[2],probe_vsie_record.data_len);
			if (priv->pshare->wlangeneral_pid > 0)
			{
				DOT11_EnQueue((unsigned long)priv, priv->pgeneralevent_queue, (unsigned char*)&probe_vsie_record, sizeof(PROBE_VSIE_RECORD));
			}

			break;
		}
#endif
		default:
			DEBUG_ERR("Not supported event : %d\n", event);
			break;
	}

	general_event_indicate(priv);
}
#endif

static void ToDrv_SetPTK(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	unsigned long ulKeyLength = 0;
	unsigned char *pucKeyMaterial = 0;
	struct iw_point wrq;
	DOT11_SET_KEY Set_Key;

#ifdef DEBUG_PSK
	printk("PSK: Set PTK, mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
		   pstat->cmn_info.mac_addr[0], pstat->cmn_info.mac_addr[1], pstat->cmn_info.mac_addr[2],
		   pstat->cmn_info.mac_addr[3], pstat->cmn_info.mac_addr[4], pstat->cmn_info.mac_addr[5]);
#endif

	wrq.pointer = (caddr_t)&Set_Key;
	Set_Key.EventId = DOT11_EVENT_SET_KEY;
	Set_Key.IsMoreEvent = FALSE;
	Set_Key.KeyIndex = 0;
	Set_Key.KeyType = DOT11_KeyType_Pairwise;

#ifdef MULTI_MAC_CLONE
	if ((OPMODE & WIFI_STATION_STATE) && priv->pmib->ethBrExtInfo.macclone_enable)
		memcpy(&Set_Key.MACAddr, pstat->sa_addr, 6);
	else
#endif
		memcpy(&Set_Key.MACAddr, pstat->cmn_info.mac_addr, 6);

	switch (pstat->wpa_sta_info->UnicastCipher) {
	case DOT11_ENC_TKIP:
		ulKeyLength =  PTK_LEN_TKIP - (PTK_LEN_EAPOLMIC + PTK_LEN_EAPOLENC);
		break;

		// Kenny
	case DOT11_ENC_CCMP:
		ulKeyLength =  PTK_LEN_CCMP - (PTK_LEN_EAPOLMIC + PTK_LEN_EAPOLENC);
		break;
	}
	pucKeyMaterial = pstat->wpa_sta_info->PTK + (PTK_LEN_EAPOLMIC + PTK_LEN_EAPOLENC);

	//sc_yang
	memset(Set_Key.KeyMaterial, 0, 64);
	if (ulKeyLength <= sizeof(Set_Key.KeyMaterial))
		memcpy(Set_Key.KeyMaterial, pucKeyMaterial, ulKeyLength);
	Set_Key.EncType = pstat->wpa_sta_info->UnicastCipher;
	Set_Key.KeyLen = ulKeyLength;
	wrq.length = sizeof(DOT11_SET_KEY) - 1 + ulKeyLength;

#ifdef DEBUG_PSK
	debug_out(NULL, Set_Key.KeyMaterial, ulKeyLength);
#endif

	rtl8192cd_ioctl_priv_req(priv->dev, &wrq);
}

void ToDrv_SetGTK(struct rtl8192cd_priv *priv)
{
	unsigned long ulKeyLength = 0;
	//unsigned long  ulKeyIndex = 0;
	struct iw_point wrq;
	DOT11_SET_KEY  Set_Key;
	unsigned char szBradcast[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	memset(&Set_Key, 0, sizeof(DOT11_SET_KEY));

#ifdef DEBUG_PSK
	printk("PSK: Set GTK\n");
#endif

	wrq.pointer = (caddr_t)&Set_Key;
	wrq.length = sizeof(DOT11_SET_KEY);

	Set_Key.EventId = DOT11_EVENT_SET_KEY;
	Set_Key.IsMoreEvent = FALSE;

	Set_Key.KeyType = DOT11_KeyType_Group;
	memcpy(&Set_Key.MACAddr, szBradcast, 6);

	Set_Key.EncType = priv->wpa_global_info->MulticastCipher;
	//sc_yang
	memset(Set_Key.KeyMaterial, 0, 64);
	ulKeyLength = 32;
#ifdef CLIENT_MODE
	if (OPMODE & WIFI_ADHOC_STATE) {
		memcpy(Set_Key.KeyMaterial, priv->wpa_global_info->PSK, ulKeyLength);
		Set_Key.KeyIndex = 0;
	} else
#endif
	{
		//ulKeyIndex = priv->wpa_global_info->GN;
#ifdef CONFIG_IEEE80211W
		if (priv->wpa_global_info->MulticastCipher == DOT11_ENC_BIP) {
			memcpy(Set_Key.KeyMaterial,
				   priv->wpa_global_info->IGTK[priv->wpa_global_info->GN_igtk],
				   ulKeyLength);
			Set_Key.KeyIndex = priv->wpa_global_info->GN_igtk;

		} else
#endif
		{
			memcpy(Set_Key.KeyMaterial,
				   priv->wpa_global_info->GTK[priv->wpa_global_info->GN],
				   ulKeyLength);
			Set_Key.KeyIndex = priv->wpa_global_info->GN;
		}
	}

	wrq.length = sizeof(DOT11_SET_KEY) - 1 + ulKeyLength;
	wrq.length = sizeof(DOT11_SET_KEY) - 1 + ulKeyLength;

#ifdef DEBUG_PSK
	debug_out(NULL, Set_Key.KeyMaterial, ulKeyLength);
#endif

	rtl8192cd_ioctl_priv_req(priv->dev, &wrq);
}



static void ToDrv_SetPort(struct rtl8192cd_priv *priv, struct stat_info *pstat, int status)
{
	struct iw_point wrq;
	DOT11_SET_PORT	Set_Port;

#ifdef DEBUG_PSK
	printk("PSK: Set PORT [%x], mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
		   status, pstat->cmn_info.mac_addr[0], pstat->cmn_info.mac_addr[1], pstat->cmn_info.mac_addr[2],
		   pstat->cmn_info.mac_addr[3], pstat->cmn_info.mac_addr[4], pstat->cmn_info.mac_addr[5]);
#endif

	wrq.pointer = (caddr_t)&Set_Port;
	wrq.length = sizeof(DOT11_SET_PORT);
	Set_Port.EventId = DOT11_EVENT_SET_PORT;
	Set_Port.PortStatus = status;
	memcpy(&Set_Port.MACAddr, pstat->cmn_info.mac_addr, 6);
	rtl8192cd_ioctl_priv_req(priv->dev, &wrq);
}

void ToDrv_SetIE(struct rtl8192cd_priv *priv)
{
	struct iw_point wrq;
	DOT11_SET_RSNIE Set_Rsnie;

	if (priv->wpa_global_info->AuthInfoElement.Length > sizeof(Set_Rsnie.RSNIE)) {
		DEBUG_ERR("[%s] exceed in size\n", __func__);
		return;
	}

#ifdef DEBUG_PSK
	debug_out("PSK: Set RSNIE", priv->wpa_global_info->AuthInfoElement.Octet,
			  priv->wpa_global_info->AuthInfoElement.Length);
#endif
    PMFDEBUG("dev[%s]\n",priv->dev->name);

	wrq.pointer = (caddr_t)&Set_Rsnie;
	wrq.length = sizeof(DOT11_SET_RSNIE);
	Set_Rsnie.EventId = DOT11_EVENT_SET_RSNIE;
	Set_Rsnie.IsMoreEvent = FALSE;
	Set_Rsnie.Flag = DOT11_Ioctl_Set;
	Set_Rsnie.RSNIELen = priv->wpa_global_info->AuthInfoElement.Length;
	memcpy(&Set_Rsnie.RSNIE,
		   priv->wpa_global_info->AuthInfoElement.Octet,
		   priv->wpa_global_info->AuthInfoElement.Length);

	rtl8192cd_ioctl_priv_req(priv->dev, &wrq);
}

static void ToDrv_DisconnectSTA(struct rtl8192cd_priv *priv, struct stat_info *pstat, int reason)
{
	struct iw_point wrq;
	DOT11_DISCONNECT_REQ	Disconnect_Req;

#ifdef DEBUG_PSK
	printk("PSK: disconnect sta, mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
		   pstat->cmn_info.mac_addr[0], pstat->cmn_info.mac_addr[1], pstat->cmn_info.mac_addr[2],
		   pstat->cmn_info.mac_addr[3], pstat->cmn_info.mac_addr[4], pstat->cmn_info.mac_addr[5]);
#endif

	wrq.pointer = (caddr_t)&Disconnect_Req;
	wrq.length = sizeof(DOT11_DISCONNECT_REQ);

	Disconnect_Req.EventId = DOT11_EVENT_DISCONNECT_REQ;
	Disconnect_Req.IsMoreEvent = FALSE;
	Disconnect_Req.Reason = (unsigned short)reason;
	memcpy(Disconnect_Req.MACAddr, pstat->cmn_info.mac_addr, 6);

	rtl8192cd_ioctl_priv_req(priv->dev, &wrq);
};

static void ToDrv_IndicateMICFail(struct rtl8192cd_priv *priv, unsigned char *mac)
{
	struct iw_point wrq;
	DOT11_MIC_FAILURE MIC_Failure;

	wrq.pointer = (caddr_t)&MIC_Failure;
	wrq.length = sizeof(DOT11_INIT_QUEUE);

	MIC_Failure.EventId = DOT11_EVENT_MIC_FAILURE;
	memcpy(MIC_Failure.MACAddr, mac, 6);

	rtl8192cd_ioctl_priv_req(priv->dev, &wrq);
}

static void reset_sta_info(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	WPA_STA_INFO *pInfo = pstat->wpa_sta_info;

	unsigned long flags;

	SAVE_INT_AND_CLI(flags);

	SMP_LOCK_PSK_RESEND(flags);
	if (timer_pending(&pInfo->resendTimer)) {
		del_timer(&pInfo->resendTimer);
	}
	SMP_UNLOCK_PSK_RESEND(flags);

	if (OPMODE & WIFI_AP_STATE) {
		ToDrv_RemovePTK(priv, pstat->cmn_info.mac_addr, DOT11_KeyType_Pairwise);
		ToDrv_SetPort(priv, pstat, DOT11_PortStatus_Unauthorized);
	}

	memset((char *)pInfo, '\0', sizeof(WPA_STA_INFO));

	pInfo->ANonce.Octet = pInfo->AnonceBuf;
	pInfo->ANonce.Length = KEY_NONCE_LEN;

	pInfo->SNonce.Octet = pInfo->SnonceBuf;
	pInfo->SNonce.Length = KEY_NONCE_LEN;

#ifdef CONFIG_IEEE80211R
	pInfo->isFT = !!pstat->ft_state;
#endif

	pInfo->EAPOLMsgSend.Octet = pInfo->eapSendBuf;
	pInfo->EapolKeyMsgSend.Octet = pInfo->EAPOLMsgSend.Octet + ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN;

	pInfo->EAPOLMsgRecvd.Octet = pInfo->eapRecvdBuf;
	pInfo->EapolKeyMsgRecvd.Octet = pInfo->EAPOLMsgRecvd.Octet + ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN;

	init_timer(&pInfo->resendTimer);
	pInfo->resendTimer.data = (unsigned long)pstat;
	pInfo->resendTimer.function = ResendTimeout;

	pInfo->priv = priv;

	if (OPMODE & WIFI_AP_STATE) {
		pInfo->state = PSK_STATE_IDLE;

#ifdef AUTH_SAE
		if (pstat->is_sae_sta) {
			log("is_sae_sta");
			memcpy(pInfo->PMK, pstat->sae_pmk, PMK_LEN);
		}
		log("reset peer's pmkid_caching_idx");
		pstat->pmkid_caching_idx = NUM_PMKID_CACHE; //NUM_PMKID_CACHE == invalid pmkid

#endif
	}
#ifdef CLIENT_MODE
	else if (OPMODE & WIFI_STATION_STATE) {
		pInfo->clientHndshkProcessing = FALSE;
		pInfo->clientHndshkDone = FALSE;
		pInfo->clientHndshkRekey = FALSE;

		pInfo->CurrentReplayCounter.field.HighPart = 0xffffffff;
		pInfo->CurrentReplayCounter.field.LowPart = 0xffffffff;

#ifdef AUTH_SAE_STA
		if (pstat->is_sae_sta) {
			log("use SAE PMK");
			memcpy(pInfo->PMK, pstat->sae_pmk, PMK_LEN);
		} else
#endif
		{
			log("use PSK per-generated PMK");
			memcpy(pInfo->PMK, priv->wpa_global_info->PSK, PMK_LEN);
		}
	}
#endif

#if	defined(AUTH_SAE) ||defined(AUTH_SAE_STA)
#ifdef WPA3_STA_DEBUG
	log("");
	dump_hex("PMK be used:", pInfo->PMK, 6);
#endif
#endif
	RESTORE_INT(flags);
}


static void ResendTimeout(unsigned long task_psta)
{
	struct stat_info *pstat = (struct stat_info *)task_psta;
	struct rtl8192cd_priv *priv;
#ifdef SMP_SYNC
	unsigned long flags;
#endif

	if (pstat == NULL)
		return;

	priv = pstat->wpa_sta_info->priv;

	DEBUG_TRACE;

	if (!(priv->drv_state & DRV_STATE_OPEN))
		return;

	SMP_LOCK_PSK_RESEND(flags);
#ifdef CLIENT_MODE
#ifdef MULTI_MAC_CLONE
	ACTIVE_ID = pstat->mclone_id;
#endif
	if ((OPMODE & WIFI_STATION_STATE) && !pstat->wpa_sta_info->clientHndshkProcessing) {
		DEBUG_ERR("Wait EAP timeout, disconnect  AP!\n");
		DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_SERIOUS, "Wait EAP timeout, disconnect  AP!\n");
		priv->dot114WayStatus = _RSON_4WAY_HNDSHK_TIMEOUT_;
		ToDrv_DisconnectSTA(priv, pstat, expire);
		SMP_UNLOCK_PSK_RESEND(flags);
		return;
	}
#endif

	if (++pstat->wpa_sta_info->resendCnt > MAX_RESEND_NUM)
	{

// When the case of group rekey timeout, update GTK to driver when it is
// the last one node
		if (OPMODE & WIFI_AP_STATE) {
			if (pstat->wpa_sta_info->state == PSK_STATE_PTKINITDONE &&
					pstat->wpa_sta_info->gstate == PSK_GSTATE_REKEYNEGOTIATING) {
				if (priv->wpa_global_info->GKeyDoneStations > 0)
					priv->wpa_global_info->GKeyDoneStations--;

				if (priv->wpa_global_info->GKeyDoneStations == 0 && !priv->wpa_global_info->GkeyReady) {
					ToDrv_SetGTK(priv);
#ifdef CONFIG_IEEE80211W
					if (priv->pmib->dot1180211AuthEntry.dot11IEEE80211W != NO_MGMT_FRAME_PROTECTION)
						ToDrv_SetIGTK(priv);
#endif
					priv->wpa_global_info->GkeyReady = TRUE;
					priv->wpa_global_info->GResetCounter = TRUE;

					// start groupkey rekey timer
					if (priv->pmib->dot1180211AuthEntry.dot11GKRekeyTime)
						mod_timer(&priv->wpa_global_info->GKRekeyTimer, jiffies + RTL_SECONDS_TO_JIFFIES(priv->pmib->dot1180211AuthEntry.dot11GKRekeyTime));

					// if only one sta and group key rekey fail, don't disconnect the sta
					if (priv->assoc_num == 1) {
#ifdef DEBUG_PSK
						printk("!!!!DO NOT disconnect STA under 1 client case!!!\n");
#endif
						SMP_UNLOCK_PSK_RESEND(flags);
						return;
					}

				}
			}
		}
#ifdef CLIENT_MODE
		else if (OPMODE & WIFI_STATION_STATE) {
			priv->dot114WayStatus = _RSON_4WAY_HNDSHK_TIMEOUT_;
		}
#endif
//--------------------------------------------------------- david+2006-04-06

		priv->ext_stats.rx_cnt_psk_to++;
		DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_SERIOUS, "RSN_4_way_handshake_timeout!\n");
		ToDrv_DisconnectSTA(priv, pstat, RSN_4_way_handshake_timeout);
#if defined(CONFIG_AUTH_RESULT)
		authRes = RSN_4_way_handshake_timeout;
#endif
// need not reset because ToDrv_DisconnectSTA() will take care of it, david+2006-04-06
//		reset_sta_info(priv, pstat);

#ifdef DEBUG_PSK
		printk("PSK: Exceed max retry, disconnect sta\n");
#endif
#if defined(__ECOS) && defined(CONFIG_SDIO_HCI) && defined(CLIENT_MODE)
		if (OPMODE & WIFI_STATION_STATE) {
			if (priv->link_status_cb_func)
				priv->link_status_cb_func("wlan0", WIFI_LINK_STATUS_HANDSHAKE_TIMEOUT);
		}
#endif
	}
	else {
		if (OPMODE & WIFI_AP_STATE)
			SendEAPOL(priv, pstat, 1);
#ifdef CLIENT_MODE
		else if (OPMODE & WIFI_STATION_STATE)
			ClientSendEAPOL(priv, pstat, 0);
#endif
	}

	SMP_UNLOCK_PSK_RESEND(flags);
}


static void GKRekeyTimeout(unsigned long task_priv)
{
	struct rtl8192cd_priv *priv = (struct rtl8192cd_priv *)task_priv;
	unsigned long flags = 0;
#ifdef SMP_SYNC
	unsigned long flags2;
#endif

	DEBUG_TRACE;

	if (!(priv->drv_state & DRV_STATE_OPEN))
		return;

	SAVE_INT_AND_CLI(flags);
//	SMP_LOCK(flags);

	SMP_LOCK_PSK_GKREKEY(flags2);
	priv->wpa_global_info->GTKRekey = TRUE;
	SMP_UNLOCK_PSK_GKREKEY(flags2);

	// reset TXPN here for GTK rekey 2-1 RSC
	priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TXPN48.val48 = 1;

	UpdateGK(priv);

	RESTORE_INT(flags);
//	SMP_UNLOCK(flags);
}

static void SendEAPOL(struct rtl8192cd_priv *priv, struct stat_info *pstat, int resend)
{
	OCTET_STRING	IV, RSC, KeyID, MIC, KeyData, EAPOLMsgSend, EapolKeyMsgSend;
	unsigned char	IV_buff[KEY_IV_LEN], RSC_buff[KEY_RSC_LEN];
	unsigned char	ID_buff[KEY_ID_LEN], MIC_buff[KEY_MIC_LEN], KEY_buff[INFO_ELEMENT_SIZE];
	unsigned short	tmpKeyData_Length;
	unsigned char	KeyDescriptorVer = key_desc_ver1;
	int 			IfCalcMIC = 0;
	struct wlan_ethhdr_t *eth_hdr;
	struct lib1x_eapol *eapol;
	struct sk_buff	*pskb;
	lib1x_eapol_key	*eapol_key;
	WPA_STA_INFO	*pStaInfo;
	WPA_GLOBAL_INFO *pGblInfo;
	unsigned char my_11w = priv->pmib->dot1180211AuthEntry.dot11IEEE80211W;
	unsigned char my_sha256 = priv->pmib->dot1180211AuthEntry.dot11EnableSHA256;

	DEBUG_TRACE;

	if (priv == NULL || pstat == NULL)
		return;

	pStaInfo = pstat->wpa_sta_info;
	pGblInfo = priv->wpa_global_info;

	if (pStaInfo->state == PSK_STATE_IDLE)
		return;

	memset(&EapolKeyMsgSend, 0, sizeof(EapolKeyMsgSend));
	EAPOLMsgSend.Octet = pStaInfo->EAPOLMsgSend.Octet;
	EapolKeyMsgSend.Octet = EAPOLMsgSend.Octet + ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN;
	pStaInfo->EapolKeyMsgSend.Octet = EapolKeyMsgSend.Octet;
	eapol_key = (lib1x_eapol_key *)EapolKeyMsgSend.Octet;

#ifdef CONFIG_IEEE80211W
	if (pStaInfo->AuthKeyMethod == DOT11_AuthKeyType_PSK_SHA256){
		KeyDescriptorVer = key_desc_ver3;
	}else
#endif
#ifdef CONFIG_IEEE80211R
	if (pStaInfo->AuthKeyMethod == DOT11_AuthKeyType_FT8021x || pStaInfo->AuthKeyMethod == DOT11_AuthKeyType_FTPSK)
		KeyDescriptorVer = key_desc_ver3;
	else
#endif
	if ((pGblInfo->MulticastCipher == _CCMP_PRIVACY_) || (pStaInfo->UnicastCipher == _CCMP_PRIVACY_))
		KeyDescriptorVer = key_desc_ver2;
#if 1   // Mark.CS_gk_rekey
#ifdef AUTH_SAE
	if(pstat->is_sae_sta)
		KeyDescriptorVer = 0;
#endif
#endif

	log("peer's akm[%d]; my 11w[%d] sha256[%d] keydecver[%d]",pStaInfo->AuthKeyMethod,my_11w,my_sha256,KeyDescriptorVer);


	if (resend) {
        DEBUG_INFO("PSK: ReSend (started:%d, replay:%d)\n", pStaInfo->ReplayCounterStarted.field.LowPart, pStaInfo->CurrentReplayCounter.field.LowPart);

		EAPOLMsgSend.Length = pStaInfo->EAPOLMsgSend.Length;
		EapolKeyMsgSend.Length = pStaInfo->EapolKeyMsgSend.Length;
		Message_setReplayCounter(EapolKeyMsgSend, pStaInfo->CurrentReplayCounter.field.HighPart, pStaInfo->CurrentReplayCounter.field.LowPart);
		memcpy(&pStaInfo->LatestGKReplayCounter,&pStaInfo->CurrentReplayCounter, sizeof(pStaInfo->CurrentReplayCounter));

        switch (pStaInfo->state) {
        case PSK_STATE_PTKSTART:
            memcpy(&pStaInfo->ReplayCounterStarted, &pStaInfo->CurrentReplayCounter, sizeof(LARGE_INTEGER)); // save started reply counter, david+1-12-2007
            break;
        default:
            break;
        }
		INCLargeInteger(&pStaInfo->CurrentReplayCounter);
#if defined(__ECOS) && defined(CONFIG_SDIO_HCI)
		if (pStaInfo->state != PSK_STATE_PTKSTART)
			IfCalcMIC = TRUE;
#else
		IfCalcMIC = TRUE;
#endif
		goto send_packet;
	}

	IV.Octet = IV_buff;
	IV.Length = KEY_IV_LEN;
	RSC.Octet = RSC_buff;
	RSC.Length = KEY_RSC_LEN;
	KeyID.Octet = ID_buff;
	KeyID.Length = KEY_ID_LEN;
	MIC.Octet = MIC_buff;
	MIC.Length = KEY_MIC_LEN;
	KeyData.Octet = KEY_buff;
	KeyData.Length = 0;

	switch (pStaInfo->state) {
	case PSK_STATE_PTKSTART:
		//send 1st message of 4-way handshake
		DEBUG_INFO("4-1 (started:%d, replay:%d)\n", pStaInfo->ReplayCounterStarted.field.LowPart, pStaInfo->CurrentReplayCounter.field.LowPart);
		PSKDEBUG("4-1\n");
		memset(EapolKeyMsgSend.Octet, 0, MAX_EAPOLKEYMSG_LEN);

#ifdef RTL_WPA2
		if (pStaInfo->RSNEnabled & PSK_WPA2)
			Message_setDescType(EapolKeyMsgSend, desc_type_WPA2);
		else
#endif
			Message_setDescType(EapolKeyMsgSend, desc_type_RSN);

#ifdef AUTH_SAE
		if(pstat->is_sae_sta)
			KeyDescriptorVer = 0;
#endif

		Message_setKeyDescVer(EapolKeyMsgSend, KeyDescriptorVer);
		Message_setKeyType(EapolKeyMsgSend, type_Pairwise);
		Message_setKeyIndex(EapolKeyMsgSend, 0);
		Message_setInstall(EapolKeyMsgSend, 0);
		Message_setKeyAck(EapolKeyMsgSend, 1);
		Message_setKeyMIC(EapolKeyMsgSend, 0);
		Message_setSecure(EapolKeyMsgSend, 0);
		Message_setError(EapolKeyMsgSend, 0);
		Message_setRequest(EapolKeyMsgSend, 0);
		Message_setReserved(EapolKeyMsgSend, 0);
		Message_setKeyLength(EapolKeyMsgSend, (pStaInfo->UnicastCipher  == DOT11_ENC_TKIP) ? 32 : 16);
//			Message_setKeyLength(EapolKeyMsgSend, 32);

		// make 4-1's ReplyCounter increased
		Message_setReplayCounter(EapolKeyMsgSend, pStaInfo->CurrentReplayCounter.field.HighPart, pStaInfo->CurrentReplayCounter.field.LowPart);
		memcpy(&pStaInfo->ReplayCounterStarted, &pStaInfo->CurrentReplayCounter, sizeof(LARGE_INTEGER)); // save started reply counter, david+1-12-2007

		INCLargeInteger(&pStaInfo->CurrentReplayCounter);

		INCOctet32_INTEGER(&pGblInfo->Counter);
//#ifndef RTL_WPA2_PREAUTH
		// ANonce is only updated in lib1x_init_authenticator()
		// or after 4-way handshake
		// To avoid different ANonce values among multiple issued 4-1 messages because of multiple association requests
		// Different ANonce values among multiple 4-1 messages induce 4-2 MIC failure.
		SetNonce(pStaInfo->ANonce, pGblInfo->Counter);
//#endif
		Message_setKeyNonce(EapolKeyMsgSend, pStaInfo->ANonce);

		memset(IV.Octet, 0, IV.Length);
		Message_setKeyIV(EapolKeyMsgSend, IV);
		memset(RSC.Octet, 0, RSC.Length);
		Message_setKeyRSC(EapolKeyMsgSend, RSC);
		memset(KeyID.Octet, 0, KeyID.Length);
		Message_setKeyID(EapolKeyMsgSend, KeyID);

// enable it to interoper with Intel 11n Centrino, david+2007-11-19
#ifdef AUTH_SAE //pmkid caching
		if(pmkid_cached(pstat)){
			log("add PMKID to 4-1");
			unsigned char tmp_pmkid[22]; // 2 + 4(RSN_SELECTOR_LEN) + 16(PMKID_LEN)
			tmp_pmkid[0] = 0xdd; //VENDOR_SPECIFIC_IE;
			tmp_pmkid[1] = 4 + 16; // RSN_SELECTOR_LEN) + PMKID_LEN
			tmp_pmkid[2] = 0x0;	//#define RSN_KEY_DATA_PMKID RSN_SELECTOR(0x00, 0x0f, 0xac, 4)
			tmp_pmkid[3] = 0xf;
			tmp_pmkid[4] = 0xac;
			tmp_pmkid[5] = 0x4;
			memcpy(&tmp_pmkid[6], &priv->wpa_global_info->pmkid_cache.pmkid[pstat->pmkid_caching_idx], LEN_PMKID);
			memcpy(KeyData.Octet, tmp_pmkid, 22);
			KeyData.Length = 22;

			Message_setKeyData(EapolKeyMsgSend, KeyData);
			Message_setKeyDataLength(EapolKeyMsgSend, KeyData.Length);
		}
		else
#endif
#if 1
		if ((pStaInfo->RSNEnabled & PSK_WPA2) && (pStaInfo->AuthKeyMethod == DOT11_AuthKeyType_PSK || pStaInfo->PMKCached) ) {
			/*otherwise PMK cache*/
			static char PMKID_KDE_TYPE[] = { 0xDD, 0x14, 0x00, 0x0F, 0xAC, 0x04 };
			Message_setKeyDataLength(EapolKeyMsgSend, 22);
			memcpy(EapolKeyMsgSend.Octet + KeyDataPos,
				   PMKID_KDE_TYPE, sizeof(PMKID_KDE_TYPE));
//				memcpy(EapolKeyMsgSend.Octet+KeyDataPos+sizeof(PMKID_KDE_TYPE),
//					global->akm_sm->PMKID, PMKID_LEN);
		} else
#endif
			Message_setKeyDataLength(EapolKeyMsgSend, 0);

		memset(MIC.Octet, 0, MIC.Length);
		Message_setMIC(EapolKeyMsgSend, MIC);

#ifdef RTL_WPA2
		if (pStaInfo->RSNEnabled & PSK_WPA2)
			EapolKeyMsgSend.Length = EAPOLMSG_HDRLEN + Message_KeyDataLength(EapolKeyMsgSend);
		else
#endif
			EapolKeyMsgSend.Length = EAPOLMSG_HDRLEN;
		EAPOLMsgSend.Length = ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN + EapolKeyMsgSend.Length;
		break;

	case PSK_STATE_PTKINITNEGOTIATING:

		//Construct Message3
		DEBUG_INFO("4-3 (started:%d, replay:%d)\n",
			pStaInfo->ReplayCounterStarted.field.LowPart, pStaInfo->CurrentReplayCounter.field.LowPart);
		PSKDEBUG("4-3\n");
		memset(EapolKeyMsgSend.Octet, 0, MAX_EAPOLKEYMSG_LEN);

#ifdef AUTH_SAE
		if(pstat->is_sae_sta)
			KeyDescriptorVer = key_desc_ver0;
#endif
		if(priv->pmib->miscEntry.disable_m3_retry){
			pStaInfo->resendCnt = MAX_RESEND_NUM; // sent 4-3 only one time to avoid key re-installation attack
		}
#ifdef RTL_WPA2
		if (pStaInfo->RSNEnabled & PSK_WPA2)
			Message_setDescType(EapolKeyMsgSend, desc_type_WPA2);
		else
#endif
			Message_setDescType(EapolKeyMsgSend, desc_type_RSN);

		Message_setKeyDescVer(EapolKeyMsgSend, Message_KeyDescVer(pStaInfo->EapolKeyMsgRecvd));
		Message_setKeyType(EapolKeyMsgSend, Message_KeyType(pStaInfo->EapolKeyMsgRecvd));
		Message_setKeyIndex(EapolKeyMsgSend, Message_KeyIndex(pStaInfo->EapolKeyMsgRecvd));

		//Message_setInstall(global->EapolKeyMsgSend, global->RSNVariable.isSuppSupportUnicastCipher ? 1:0);
		Message_setInstall(EapolKeyMsgSend, 1);
		Message_setKeyAck(EapolKeyMsgSend, 1);
		Message_setKeyMIC(EapolKeyMsgSend, 1);
		//??
		//	Message_setSecure(pStaInfo->EapolKeyMsgSend, pStaInfo->RSNVariable.isSuppSupportMulticastCipher ? 0:1);
		Message_setSecure(EapolKeyMsgSend, 0);
		//??
		Message_setError(EapolKeyMsgSend, 0);
		Message_setRequest(EapolKeyMsgSend, 0);
		Message_setReserved(EapolKeyMsgSend, 0);
		Message_setKeyLength(EapolKeyMsgSend, (pStaInfo->UnicastCipher  == DOT11_ENC_TKIP) ? 32 : 16);
		Message_setReplayCounter(EapolKeyMsgSend, pStaInfo->CurrentReplayCounter.field.HighPart, pStaInfo->CurrentReplayCounter.field.LowPart);
		Message_setKeyNonce(EapolKeyMsgSend, pStaInfo->ANonce);
		memset(IV.Octet, 0, IV.Length);
		Message_setKeyIV(EapolKeyMsgSend, IV);

#ifdef RTL_WPA2
		if (pStaInfo->RSNEnabled & PSK_WPA2) {
#ifdef CONFIG_IEEE80211R
			unsigned char key_data[384];
#else
			unsigned char key_data[128];
#endif
			unsigned char *key_data_pos = key_data;
			int i;
			int mcastcipher = (pGblInfo->MulticastCipher == DOT11_ENC_TKIP)?32:16;
			unsigned char GTK_KDE_TYPE[] = {0xDD, 0x16, 0x00, 0x0F, 0xAC, 0x01, 0x01, 0x00};
#ifdef CONFIG_IEEE80211W
			unsigned char IGTK_KDE_TYPE[] = {0xDD, 0x1C, 0x00, 0x0F, 0xAC, 0x09};
#endif
#ifdef CONFIG_IEEE80211R
			unsigned int frlen = 0;
#endif


			EapolKeyMsgSend.Octet[1] = 0x13;
//???
			if (KeyDescriptorVer == key_desc_ver2
			#ifdef CONFIG_IEEE80211W
				|| KeyDescriptorVer == key_desc_ver3
			#endif
			) {
				INCOctet32_INTEGER(&pGblInfo->Counter);
				SetEAPOL_KEYIV(IV, pGblInfo->Counter);
				//memset(IV.Octet, 0x0, IV.Length);
				Message_setKeyIV(EapolKeyMsgSend, IV);
			}
			// RSN IE
			if (pGblInfo->AuthInfoElement.Octet[0] == WPA2_ELEMENT_ID) {
				int len = (unsigned char)pGblInfo->AuthInfoElement.Octet[1] + 2;
				if ((len+sizeof(GTK_KDE_TYPE)+mcastcipher) > sizeof(key_data)) {
					DEBUG_ERR("invalid IE length!\n");
#if defined(CONFIG_WLAN_STATS_EXTENTION)
					priv->ext_stats.total_psk_fail++;
#endif
					return;
				}
				memcpy(key_data_pos, pGblInfo->AuthInfoElement.Octet, len);
				key_data_pos += len;
			} else {
				//find WPA2_ELEMENT_ID 0x30
				int len = (unsigned char)pGblInfo->AuthInfoElement.Octet[1] + 2;
				//printf("%s: global->auth->RSNVariable.AuthInfoElement.Octet[%d] = %02X\n", (char *)__FUNCTION__, len, global->auth->RSNVariable.AuthInfoElement.Octet[len]);
				if (pGblInfo->AuthInfoElement.Octet[len] == WPA2_ELEMENT_ID) {
					int len2 = (unsigned char)pGblInfo->AuthInfoElement.Octet[len + 1] + 2;
					if ((len2+sizeof(GTK_KDE_TYPE)+mcastcipher) > sizeof(key_data)) {
						DEBUG_ERR("invalid IE length!\n");
#if defined(CONFIG_WLAN_STATS_EXTENTION)
						priv->ext_stats.total_psk_fail++;
#endif
						return;
					}
					memcpy(key_data_pos, pGblInfo->AuthInfoElement.Octet + len, len2);
					key_data_pos += len2;
				} else {
					DEBUG_ERR("%d ERROR!\n", __LINE__);
#if defined(CONFIG_WLAN_STATS_EXTENTION)
					priv->ext_stats.total_psk_fail++;
#endif
				}
			}
#ifdef CONFIG_IEEE80211R
			if (FT_ENABLE && pStaInfo->isFT) {
				// Append PMK-R1Name to RSNIE
				struct r1_key_holder *r1kh = pStaInfo->r1kh;
				if (r1kh) {
					*((unsigned short *)key_data_pos) = cpu_to_le16(1);
					key_data_pos += 2;
					memcpy(key_data_pos, r1kh->pmk_r1_name, PMKID_LEN);
					key_data_pos += PMKID_LEN;
					key_data[1] += (2 + PMKID_LEN);
				}
			}
#endif

#ifdef CONFIG_IEEE80211R
			if (FT_ENABLE && pStaInfo->isFT) {
				key_data_pos = construct_mobility_domain_ie(priv, key_data_pos , &frlen);
			}
#endif

			if (sizeof(GTK_KDE_TYPE) <= (key_data + sizeof(key_data) - key_data_pos))
				memcpy(key_data_pos, GTK_KDE_TYPE, sizeof(GTK_KDE_TYPE));
			key_data_pos[1] = 6 + mcastcipher;
			key_data_pos += sizeof(GTK_KDE_TYPE);


			// FIX GROUPKEY ALL ZERO
// david+2006-04-04, fix the issue of re-generating group key
//				pGblInfo->GInitAKeys = TRUE;
			UpdateGK(priv);
			if (mcastcipher <= (key_data + sizeof(key_data) - key_data_pos))
				memcpy(key_data_pos, pGblInfo->GTK[pGblInfo->GN], mcastcipher);

			key_data_pos += mcastcipher;

			//=================================================
			// IGTK KDE
#ifdef CONFIG_IEEE80211W
			PMFDEBUG("mgmt_frame_prot=[%d]\n", pstat->wpa_sta_info->mgmt_frame_prot);
			if (pstat->wpa_sta_info->mgmt_frame_prot) {
				if ((unsigned long)key_data_pos+28 > (unsigned long)key_data+sizeof(key_data)) {
					DEBUG_ERR("[%s] exceed in size\n", __func__);
					return;
				}
				//PMFDEBUG("SendEAPOL,PATH5\n");
				memcpy(key_data_pos, IGTK_KDE_TYPE, sizeof(IGTK_KDE_TYPE));
				key_data_pos += sizeof(IGTK_KDE_TYPE);
				// Key ID
				*(key_data_pos) = (unsigned char)priv->wpa_global_info->GN_igtk;
				*(key_data_pos + 1) = 0;
				key_data_pos += 2;
				// IPN
				*(key_data_pos++) = priv->wpa_global_info->IGTK_PN._byte_.TSC0;
				*(key_data_pos++) = priv->wpa_global_info->IGTK_PN._byte_.TSC1;
				*(key_data_pos++) = priv->wpa_global_info->IGTK_PN._byte_.TSC2;
				*(key_data_pos++) = priv->wpa_global_info->IGTK_PN._byte_.TSC3;
				*(key_data_pos++) = priv->wpa_global_info->IGTK_PN._byte_.TSC4;
				*(key_data_pos++) = priv->wpa_global_info->IGTK_PN._byte_.TSC5;

				memcpy(key_data_pos, priv->wpa_global_info->IGTK[priv->wpa_global_info->GN_igtk - 4], 16);
                #if 0 // for debug
				PMFDEBUG("IGTK=");
				for (i = 0; i < 16; i++)
					panic_printk("%x", priv->wpa_global_info->IGTK[priv->wpa_global_info->GN_igtk - 4][i]);
				panic_printk("\n");
                #endif
				key_data_pos += 16;
			}
#endif
#ifdef CONFIG_IEEE80211R
			if (FT_ENABLE && pStaInfo->isFT) {
				key_data_pos = construct_fast_bss_transition_ie(priv, key_data_pos, &frlen, pstat);
				key_data_pos = construct_timeout_interval_ie(priv, key_data_pos, &frlen,
						TIE_TYPE_REASSOC_DEADLINE, (priv->pmib->dot11FTEntry.dot11FTReassociationDeadline * 1000) / 1024);
				key_data_pos = construct_timeout_interval_ie(priv, key_data_pos, &frlen,
						TIE_TYPE_KEY_LIFETIME, priv->pmib->dot11FTEntry.dot11FTR0KeyLifetime * 60);
			}
#endif

			i = (key_data_pos - key_data) % 8;
			if ( i != 0 ) {
				*key_data_pos = 0xdd;
				key_data_pos++;
				for (i = i + 1; i < 8; i++) {
					*key_data_pos = 0x0;
					key_data_pos++;
				}
			}
			EncGTK(priv, pstat,
				   pStaInfo->PTK + PTK_LEN_EAPOLMIC, PTK_LEN_EAPOLENC,
				   key_data,
				   (key_data_pos - key_data),
				   KeyData.Octet, &tmpKeyData_Length);
			KeyData.Length = (int)tmpKeyData_Length;
			Message_setKeyData(EapolKeyMsgSend, KeyData);
			Message_setKeyDataLength(EapolKeyMsgSend, KeyData.Length);

			EapolKeyMsgSend.Length = EAPOLMSG_HDRLEN +	KeyData.Length;
			RSC.Octet[0] = priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TXPN48._byte_.TSC0;
			RSC.Octet[1] = priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TXPN48._byte_.TSC1;
			RSC.Octet[2] = priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TXPN48._byte_.TSC2;
			RSC.Octet[3] = priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TXPN48._byte_.TSC3;
			RSC.Octet[4] = priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TXPN48._byte_.TSC4;
			RSC.Octet[5] = priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TXPN48._byte_.TSC5;
			RSC.Octet[6] = 0;
			RSC.Octet[7] = 0;
			Message_setKeyRSC(EapolKeyMsgSend, RSC);
		} else
#endif // RTL_WPA2
		{
			// WPA
			memset(RSC.Octet, 0, RSC.Length);
			Message_setKeyRSC(EapolKeyMsgSend, RSC);
			memset(KeyID.Octet, 0, KeyID.Length);
			Message_setKeyID(EapolKeyMsgSend, KeyID);
			//lib1x_hexdump2(MESS_DBG_KEY_MANAGE, "lib1x_akmsm_ProcessEAPOL_proc", global->auth->RSNVariable.AuthInfoElement.Octet, global->auth->RSNVariable.AuthInfoElement.Length,"Append Authenticator Information Element");

			{
				//WPA 0xDD
				//printf("%s: global->auth->RSNVariable.AuthInfoElement.Octet[0] = %02X\n", (char *)__FUNCTION__, global->auth->RSNVariable.AuthInfoElement.Octet[0]);

				int len = (unsigned char)pGblInfo->AuthInfoElement.Octet[1] + 2;
				len = (len<INFO_ELEMENT_SIZE)?len:INFO_ELEMENT_SIZE;

				if (pGblInfo->AuthInfoElement.Octet[0] == RSN_ELEMENT_ID) {
					memcpy(KeyData.Octet, pGblInfo->AuthInfoElement.Octet, len);
					KeyData.Length = len;
				} else {
					// impossible case??
					int len2 = (unsigned char)pGblInfo->AuthInfoElement.Octet[len + 1] + 2;
					len2 = (len2<INFO_ELEMENT_SIZE)?len2:INFO_ELEMENT_SIZE;
					memcpy(KeyData.Octet, pGblInfo->AuthInfoElement.Octet + len, len2);
					KeyData.Length = len2;
				}
			}
			Message_setKeyDataLength(EapolKeyMsgSend, KeyData.Length);
			Message_setKeyData(EapolKeyMsgSend, KeyData);
			EapolKeyMsgSend.Length = EAPOLMSG_HDRLEN + KeyData.Length;
		}

		INCLargeInteger(&pStaInfo->CurrentReplayCounter);

#if 0
		memset(RSC.Octet, 0, RSC.Length);
		Message_setKeyRSC(EapolKeyMsgSend, RSC);
		memset(KeyID.Octet, 0, KeyID.Length);
		Message_setKeyID(EapolKeyMsgSend, KeyID);
		//lib1x_hexdump2(MESS_DBG_KEY_MANAGE, "lib1x_akmsm_ProcessEAPOL_proc", global->auth->RSNVariable.AuthInfoElement.Octet, global->auth->RSNVariable.AuthInfoElement.Length,"Append Authenticator Information Element");
		Message_setKeyDataLength(EapolKeyMsgSend, pGblInfo->AuthInfoElement.Length);
		Message_setKeyData(EapolKeyMsgSend, pGblInfo->AuthInfoElement);
		//Message_setKeyDataLength(global->EapolKeyMsgSend, global->akm_sm->AuthInfoElement.Length);
		//Message_setKeyData(global->EapolKeyMsgSend, global->akm_sm->AuthInfoElement);
		EapolKeyMsgSend.Length = EAPOLMSG_HDRLEN + pGblInfo->AuthInfoElement.Length;
#endif /* RTL_WPA2 */

		EAPOLMsgSend.Length = ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN + EapolKeyMsgSend.Length;
		IfCalcMIC = TRUE;
		break;

	case PSK_STATE_PTKINITDONE:
		//send 1st message of 2-way handshake
		DEBUG_INFO("2-1 (started:%d, replay:%d)\n", pStaInfo->ReplayCounterStarted.field.LowPart, pStaInfo->CurrentReplayCounter.field.LowPart);
		PSKDEBUG("2-1\n");
		memset(EapolKeyMsgSend.Octet, 0, MAX_EAPOLKEYMSG_LEN);

#ifdef AUTH_SAE  // Mark.CS_gk_rekey
		if(pstat->is_sae_sta)
			KeyDescriptorVer = 0;
#endif

#ifdef RTL_WPA2
		if (pStaInfo->RSNEnabled & PSK_WPA2)
			Message_setDescType(EapolKeyMsgSend, desc_type_WPA2);
		else
#endif
			Message_setDescType(EapolKeyMsgSend, desc_type_RSN);

		Message_setKeyDescVer(EapolKeyMsgSend, KeyDescriptorVer);
		Message_setKeyType(EapolKeyMsgSend, type_Group);
		Message_setKeyIndex(EapolKeyMsgSend, 1);
		Message_setInstall(EapolKeyMsgSend, 1);
		Message_setKeyAck(EapolKeyMsgSend, 1);
		Message_setKeyMIC(EapolKeyMsgSend, 1);
		Message_setSecure(EapolKeyMsgSend, 1);
		Message_setError(EapolKeyMsgSend, 0);
		Message_setRequest(EapolKeyMsgSend, 0);
		Message_setReserved(EapolKeyMsgSend, 0);

		EapolKeyMsgSend.Octet[1] = 0x03;
		if (KeyDescriptorVer == key_desc_ver1)
			EapolKeyMsgSend.Octet[2] = 0x91;
		else if (KeyDescriptorVer == key_desc_ver2)
			EapolKeyMsgSend.Octet[2] = 0x92;
		#if 1  // Mark.CS_gk_rekey
#ifdef AUTH_SAE
		else if(pstat->is_sae_sta)
			EapolKeyMsgSend.Octet[2] = 0x80;
#endif			
		#endif		
		else
			EapolKeyMsgSend.Octet[2] = 0x93;

		Message_setKeyLength(EapolKeyMsgSend, (pGblInfo->MulticastCipher == DOT11_ENC_TKIP) ? 32 : 16);
		Message_setReplayCounter(EapolKeyMsgSend, pStaInfo->CurrentReplayCounter.field.HighPart, pStaInfo->CurrentReplayCounter.field.LowPart);
		memcpy(&pStaInfo->LatestGKReplayCounter,&pStaInfo->CurrentReplayCounter, sizeof(pStaInfo->CurrentReplayCounter));
		INCLargeInteger(&pStaInfo->CurrentReplayCounter);
		// kenny: n+2
		INCLargeInteger(&pStaInfo->CurrentReplayCounter);
		SetNonce(pGblInfo->GNonce, pGblInfo->Counter);
		Message_setKeyNonce(EapolKeyMsgSend, pGblInfo->GNonce);
		memset(IV.Octet, 0, IV.Length);
		Message_setKeyIV(EapolKeyMsgSend, IV);

		RSC.Octet[0] = priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TXPN48._byte_.TSC0;
		RSC.Octet[1] = priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TXPN48._byte_.TSC1;
		RSC.Octet[2] = priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TXPN48._byte_.TSC2;
		RSC.Octet[3] = priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TXPN48._byte_.TSC3;
		RSC.Octet[4] = priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TXPN48._byte_.TSC4;
		RSC.Octet[5] = priv->pmib->dot11GroupKeysTable.dot11EncryptKey.dot11TXPN48._byte_.TSC5;
		RSC.Octet[6] = 0;
		RSC.Octet[7] = 0;
		Message_setKeyRSC(EapolKeyMsgSend, RSC);

		memset(KeyID.Octet, 0, KeyID.Length);
		Message_setKeyID(EapolKeyMsgSend, KeyID);

#ifdef RTL_WPA2
		if (pStaInfo->RSNEnabled & PSK_WPA2) {
			unsigned char key_data[128];
			unsigned char *key_data_pos = key_data;
			int i;
			int mcastcipher = (pGblInfo->MulticastCipher == DOT11_ENC_TKIP)?32:16;
			unsigned char GTK_KDE_TYPE[] = { 0xDD, 0x16, 0x00, 0x0F, 0xAC, 0x01, 0x01, 0x00 };
#ifdef CONFIG_IEEE80211W
			unsigned char IGTK_KDE_TYPE[] = {0xDD, 0x1C, 0x00, 0x0F, 0xAC, 0x09};
#endif
			memcpy(key_data_pos, GTK_KDE_TYPE, sizeof(GTK_KDE_TYPE));

//fix the bug of using default KDE length -----------
			key_data_pos[1] = 6 + mcastcipher;
//------------------------------ david+2006-04-04

			key_data_pos += sizeof(GTK_KDE_TYPE);

			EapolKeyMsgSend.Octet[1] = 0x13;
// fill key-data length after encrypt --------------------
#if 0
			if (KeyDescriptorVer == key_desc_ver1) {
// david+2006-01-06, fix the bug of using 0 as group key id
//					EapolKeyMsgSend.Octet[2] = 0x81;
				Message_setKeyDescVer(EapolKeyMsgSend, key_desc_ver1);
				Message_setKeyDataLength(EapolKeyMsgSend,
										 (sizeof(GTK_KDE_TYPE) + ((pGblInfo->MulticastCipher == DOT11_ENC_TKIP) ? 32 : 16)));
			} else if (KeyDescriptorVer == key_desc_ver2) {
// david+2006-01-06, fix the bug of using 0 as group key id
//					EapolKeyMsgSend.Octet[2] = 0x82;
				Message_setKeyDescVer(EapolKeyMsgSend, key_desc_ver2);
				Message_setKeyDataLength(EapolKeyMsgSend,
										 (sizeof(GTK_KDE_TYPE) + (8 + ((pGblInfo->MulticastCipher == DOT11_ENC_TKIP) ? 32 : 16))));
			}
#endif
//------------------------------------- david+2006-04-04

			memcpy(key_data_pos, pGblInfo->GTK[pGblInfo->GN], mcastcipher);

			key_data_pos += mcastcipher;

			//=================================================
			// IGTK KDE
#ifdef CONFIG_IEEE80211W
			PMFDEBUG("SendEAPOL, 2-1, mgmt_frame_prot=%d\n", pstat->wpa_sta_info->mgmt_frame_prot);
			if (pstat->wpa_sta_info->mgmt_frame_prot) {
				PMFDEBUG("SendEAPOL,PATH 2-1\n");
				memcpy(key_data_pos, IGTK_KDE_TYPE, sizeof(IGTK_KDE_TYPE));
				key_data_pos += sizeof(IGTK_KDE_TYPE);
				// Key ID
				*(key_data_pos) = (unsigned char)priv->wpa_global_info->GN_igtk;
				*(key_data_pos + 1) = 0;
				key_data_pos += 2;
				// IPN
				*(key_data_pos++) = priv->wpa_global_info->IGTK_PN._byte_.TSC0;
				*(key_data_pos++) = priv->wpa_global_info->IGTK_PN._byte_.TSC1;
				*(key_data_pos++) = priv->wpa_global_info->IGTK_PN._byte_.TSC2;
				*(key_data_pos++) = priv->wpa_global_info->IGTK_PN._byte_.TSC3;
				*(key_data_pos++) = priv->wpa_global_info->IGTK_PN._byte_.TSC4;
				*(key_data_pos++) = priv->wpa_global_info->IGTK_PN._byte_.TSC5;
				memcpy(key_data_pos, priv->wpa_global_info->IGTK[priv->wpa_global_info->GN_igtk - 4], 16);

				PMFDEBUG("IGTK=");
				for (i = 0; i < 16; i++)
					PMFDEBUG("%x", priv->wpa_global_info->IGTK[priv->wpa_global_info->GN_igtk - 4][i]);
				PMFDEBUG("\n");
				key_data_pos += 16;
			}
#endif

			i = (key_data_pos - key_data) % 8;
			if ( i != 0 ) {
				*key_data_pos = 0xdd;
				key_data_pos++;
				for (i = i + 1; i < 8; i++) {
					*key_data_pos = 0x0;
					key_data_pos++;
				}
			}
			EncGTK(priv, pstat, pStaInfo->PTK + PTK_LEN_EAPOLMIC, PTK_LEN_EAPOLENC,
				   key_data,
				   (key_data_pos - key_data),
				   KeyData.Octet, &tmpKeyData_Length);
		} else
#endif // RTL_WPA2
		{
// fill key-data length after encrypt ---------------------
#if 0
			if (KeyDescriptorVer == key_desc_ver1)
				Message_setKeyDataLength(EapolKeyMsgSend,
										 ((pGblInfo->MulticastCipher == DOT11_ENC_TKIP) ? 32 : 16));
			else
				Message_setKeyDataLength(EapolKeyMsgSend,
										 (8 + ((pGblInfo->MulticastCipher == DOT11_ENC_TKIP) ? 32 : 16) ));
#endif
//------------------------------------- david+2006-04-04
			EncGTK(priv, pstat, pStaInfo->PTK + PTK_LEN_EAPOLMIC, PTK_LEN_EAPOLENC,
				   pGblInfo->GTK[pGblInfo->GN],
				   (pGblInfo->MulticastCipher == DOT11_ENC_TKIP) ? 32 : 16,
				   KeyData.Octet, &tmpKeyData_Length);
		}

		KeyData.Length = (int)tmpKeyData_Length;
		Message_setKeyData(EapolKeyMsgSend, KeyData);

//set keyData length after encrypt ------------------
		Message_setKeyDataLength(EapolKeyMsgSend, KeyData.Length);
//------------------------------- david+2006-04-04

		/* Kenny
					global->EapolKeyMsgSend.Length = EAPOLMSG_HDRLEN +
							((global->RSNVariable.MulticastCipher == DOT11_ENC_TKIP) ? 32:16);
		*/
		EapolKeyMsgSend.Length = EAPOLMSG_HDRLEN +	KeyData.Length;
		EAPOLMsgSend.Length = ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN + EapolKeyMsgSend.Length;
		IfCalcMIC = TRUE;
		break;
	default:
		DEBUG_ERR("Invalid wpa state [%x]\n", pStaInfo->state);
#if defined(CONFIG_WLAN_STATS_EXTENTION)
		priv->ext_stats.total_psk_fail++;
#endif
		return;
	}//switch

	pStaInfo->EAPOLMsgSend.Length = EAPOLMsgSend.Length;
	pStaInfo->EapolKeyMsgSend.Length = EapolKeyMsgSend.Length;

send_packet:
	eth_hdr = (struct wlan_ethhdr_t *)pStaInfo->EAPOLMsgSend.Octet;
	memcpy(eth_hdr->daddr, pstat->cmn_info.mac_addr, 6);
	memcpy(eth_hdr->saddr, GET_MY_HWADDR, 6);
	eth_hdr->type = htons(LIB1X_ETHER_EAPOL_TYPE);

	eapol = (struct lib1x_eapol *)(EAPOLMsgSend.Octet + ETHER_HDRLEN);

#ifdef AUTH_SAE
	if(pstat->is_sae_sta)
		eapol->protocol_version = LIB1X_EAPOL_LOGOFF;
	else
#endif
		eapol->protocol_version = LIB1X_EAPOL_VER;

	eapol->packet_type =  LIB1X_EAPOL_KEY;
	eapol->packet_body_length = htons(EapolKeyMsgSend.Length);

	if (IfCalcMIC)
		CalcMIC(EAPOLMsgSend, KeyDescriptorVer, pStaInfo->PTK, PTK_LEN_EAPOLMIC);

	if (EAPOLMsgSend.Length > MAX_EAPOLMSG_LEN) {
		printk("[%s] buffer overrun\n", __func__);
		return;
	}

	pskb = rtl_dev_alloc_skb(priv, MAX_EAPOLMSG_LEN, _SKB_TX_, 1);
	if (pskb == NULL) {
		DEBUG_ERR("Allocate EAP skb failed!\n");
#if defined(CONFIG_WLAN_STATS_EXTENTION)
		priv->ext_stats.total_psk_fail++;
#endif
		return;
	}
	memcpy(pskb->data, (char *)eth_hdr, EAPOLMsgSend.Length);
	skb_put(pskb, EAPOLMsgSend.Length);

#ifdef DEBUG_PSK
	{
		unsigned char *msg;
		if (pStaInfo->state == PSK_STATE_PTKSTART)
			msg = "4-1";
		else if (pStaInfo->state == PSK_STATE_PTKINITNEGOTIATING)
			msg = "4-3";
		else
			msg = "2-1";

		printk("PSK: Send a EAPOL %s, len=%x\n", msg, pskb->len);
		debug_out(NULL, pskb->data, pskb->len);
	}
#endif
#ifdef ENABLE_RTL_SKB_STATS
	rtl_atomic_dec(&priv->rtl_tx_skb_cnt);
#endif

//	pskb->cb[0] = 7;	// use highest priority to xmit
	if (rtl8192cd_start_xmit(pskb, priv->dev))
		rtl_kfree_skb(priv, pskb, _SKB_TX_);

	mod_timer(&pstat->wpa_sta_info->resendTimer, jiffies + RESEND_TIME);
}


#ifdef __ECOS
int ecos_send_wlan(struct net_device *dev, unsigned char *data, int size)
{
	struct rtl8192cd_priv *priv;
	struct sk_buff *pskb;

	priv = dev->priv;
	pskb = rtl_dev_alloc_skb(priv, size, _SKB_TX_, 1);
	if (pskb == NULL) {
		DEBUG_ERR("wps: allocate EAP skb failed!\n");
		return -1;
	}
	memcpy(pskb->data, data, size);
	skb_put(pskb, size);

#ifdef ENABLE_RTL_SKB_STATS
	rtl_atomic_dec(&priv->rtl_tx_skb_cnt);
#endif

	//pskb->cb[0] = 7;	// use highest priority to xmit
	if (rtl8192cd_start_xmit(pskb, priv->dev)) {
		DEBUG_ERR("tx from wps\n");
		rtl_kfree_skb(priv, pskb, _SKB_TX_);
	}
	return 0;
}
#ifdef __ECOS
int ecos_send_wlan_mesh(struct net_device *dev, unsigned char *data, int size)
{
	struct rtl8192cd_priv *priv;
	struct sk_buff *pskb;
	priv = dev->priv;
	pskb = rtl_dev_alloc_skb(priv, size, _SKB_TX_, 1);
	if (pskb == NULL) {
		DEBUG_ERR("wps: allocate EAP skb failed!\n");
		return -1;
	}
	memcpy(pskb->data, data, size);
	skb_put(pskb, size);
#ifdef ENABLE_RTL_SKB_STATS
	rtl_atomic_dec(&priv->rtl_tx_skb_cnt);
#endif
	if (mesh_start_xmit(pskb, dev)) {
		DEBUG_ERR("tx from wps\n");
		rtl_kfree_skb(priv, pskb, _SKB_TX_);
	}
	return 0;
}
#endif
#endif


#ifdef CLIENT_MODE
void ClientSendEAPOL(struct rtl8192cd_priv *priv, struct stat_info *pstat, int resend)
{
	OCTET_STRING	IV, RSC, KeyID, MIC, KeyData, EAPOLMsgSend, EapolKeyMsgSend, Nonce;
	unsigned char	IV_buff[KEY_IV_LEN], RSC_buff[KEY_RSC_LEN];
	unsigned char	ID_buff[KEY_ID_LEN], MIC_buff[KEY_MIC_LEN], KEY_buff[INFO_ELEMENT_SIZE], Nonce_buff[KEY_NONCE_LEN];
	struct wlan_ethhdr_t *eth_hdr;
	struct lib1x_eapol *eapol;
	struct sk_buff	*pskb;
	lib1x_eapol_key	*eapol_key;
	WPA_STA_INFO	*pStaInfo;
	WPA_GLOBAL_INFO *pGblInfo;
	unsigned char KeyDescriptorVer = key_desc_ver1;
	int send_report = 0;

#ifdef CONFIG_IEEE80211R_CLI
	int rsn_len;
#endif

	DEBUG_TRACE;

	if (priv == NULL || pstat == NULL)
		return;

	log("\n===========(TX)EAPOL KEY===========\n");
	pStaInfo = pstat->wpa_sta_info;
	pGblInfo = priv->wpa_global_info;

	EAPOLMsgSend.Octet = pStaInfo->EAPOLMsgSend.Octet;
	EapolKeyMsgSend.Octet = EAPOLMsgSend.Octet + ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN;
	pStaInfo->EapolKeyMsgSend.Octet = EapolKeyMsgSend.Octet;
	eapol_key = (lib1x_eapol_key *)EapolKeyMsgSend.Octet;

	if (resend) {
		EAPOLMsgSend.Length = pStaInfo->EAPOLMsgSend.Length;
		EapolKeyMsgSend.Length = pStaInfo->EapolKeyMsgSend.Length;
		if ( (pStaInfo->EAPOLMsgSend.Length == 0) || (pStaInfo->EapolKeyMsgSend.Length == 0) ) {
			DEBUG_ERR("(%s):The length of EAPOLMsg or EapolKeyMsg is error! EAPOLMsg:%d,  EapolKeyMsg:%d\n", __FUNCTION__, pStaInfo->EAPOLMsgSend.Length,  pStaInfo->EapolKeyMsgSend.Length);
			return;
		}
		//---goto send_packet
	} else {
		IV.Octet = IV_buff;
		IV.Length = KEY_IV_LEN;
		RSC.Octet = RSC_buff;
		RSC.Length = KEY_RSC_LEN;
		KeyID.Octet = ID_buff;
		KeyID.Length = KEY_ID_LEN;
		MIC.Octet = MIC_buff;
		MIC.Length = KEY_MIC_LEN;
		KeyData.Octet = KEY_buff;
		KeyData.Length = 0;

		Nonce.Octet = Nonce_buff;
		Nonce.Length = KEY_NONCE_LEN;

		if (!pStaInfo->clientHndshkDone) {
			if (pStaInfo->clientHndshkProcessing < fourWaystep3) {
				//send 2nd message of 4-way handshake
				DEBUG_INFO("client mode 4-2\n");
				PSKDEBUG("client mode 4-2\n");

				priv->is_4Way_finished= 0;
				pStaInfo->clientHndshkProcessing = fourWaystep2;

				memset(EapolKeyMsgSend.Octet, 0, MAX_EAPOLKEYMSG_LEN);

				Message_setDescType(EapolKeyMsgSend, Message_DescType(pStaInfo->EapolKeyMsgRecvd));
				Message_setKeyDescVer(EapolKeyMsgSend, Message_KeyDescVer(pStaInfo->EapolKeyMsgRecvd));
				Message_setKeyType(EapolKeyMsgSend, Message_KeyType(pStaInfo->EapolKeyMsgRecvd));
				Message_setKeyIndex(EapolKeyMsgSend, 0);
				Message_setInstall(EapolKeyMsgSend, Message_KeyIndex(pStaInfo->EapolKeyMsgRecvd));
				Message_setKeyAck(EapolKeyMsgSend, 0);
				Message_setKeyMIC(EapolKeyMsgSend, 1);
				Message_setSecure(EapolKeyMsgSend, Message_Secure(pStaInfo->EapolKeyMsgRecvd));
				Message_setError(EapolKeyMsgSend, Message_Error(pStaInfo->EapolKeyMsgRecvd));
				Message_setRequest(EapolKeyMsgSend, Message_Request(pStaInfo->EapolKeyMsgRecvd));
				Message_setReserved(EapolKeyMsgSend, 0);

				Message_setKeyLength(EapolKeyMsgSend, Message_KeyLength(pStaInfo->EapolKeyMsgRecvd));
				Message_CopyReplayCounter(EapolKeyMsgSend, pStaInfo->EapolKeyMsgRecvd);

				Message_setKeyNonce(EapolKeyMsgSend, pStaInfo->SNonce);
				memset(IV.Octet, 0, IV.Length);
				Message_setKeyIV(EapolKeyMsgSend, IV);
				memset(RSC.Octet, 0, RSC.Length);
				Message_setKeyRSC(EapolKeyMsgSend, RSC);
				memset(KeyID.Octet, 0, KeyID.Length);
				Message_setKeyID(EapolKeyMsgSend, KeyID);

				#ifdef CONFIG_IEEE80211R_CLI
				if(FT_ENABLE && pStaInfo->isFT) {
					Message_setKeyDataLength(EapolKeyMsgSend, pGblInfo->AuthInfoElement.Length + FT_PMK_ID_LEN +  MDID_IE_LEN + FT_IE_LEN);
					//since PMKID only exists in 4way, can't be generated in psk_init()
					pGblInfo->AuthInfoElement.Octet[1] += FT_PMK_ID_LEN;
					Message_setKeyData(EapolKeyMsgSend, pGblInfo->AuthInfoElement);
					setFTPMKIDData(EapolKeyMsgSend, priv, pstat);
					setFTMDIEData(EapolKeyMsgSend, priv);
					setFTIEData(EapolKeyMsgSend, priv, pstat);
					EapolKeyMsgSend.Length = EAPOLMSG_HDRLEN + pGblInfo->AuthInfoElement.Length + FT_PMK_ID_LEN + MDID_IE_LEN + FT_IE_LEN;
				} else
				#endif
				{
					Message_setKeyDataLength(EapolKeyMsgSend, pGblInfo->AuthInfoElement.Length);
					Message_setKeyData(EapolKeyMsgSend, pGblInfo->AuthInfoElement);
					EapolKeyMsgSend.Length = EAPOLMSG_HDRLEN + pGblInfo->AuthInfoElement.Length;
				}


				EAPOLMsgSend.Length = ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN + EapolKeyMsgSend.Length;

			}
			else {
				//Construct Message4
				DEBUG_INFO("client mode 4-4, key desc type[%d]\n",Message_KeyDescVer(pStaInfo->EapolKeyMsgRecvd));
				PSKDEBUG("client mode 4-4, key desc type[%d]\n",Message_KeyDescVer(pStaInfo->EapolKeyMsgRecvd));

				pStaInfo->clientHndshkDone++;
				pStaInfo->clientHndshkProcessing=fourWaystep4;
				priv->dot114WayStatus = 0;
#if defined(CONFIG_AUTH_RESULT)
				authRes = 0;
#endif

				memset(EapolKeyMsgSend.Octet, 0, MAX_EAPOLKEYMSG_LEN);

				Message_setDescType(EapolKeyMsgSend, Message_DescType(pStaInfo->EapolKeyMsgRecvd));
				Message_setKeyDescVer(EapolKeyMsgSend, Message_KeyDescVer(pStaInfo->EapolKeyMsgRecvd));
				Message_setKeyType(EapolKeyMsgSend, Message_KeyType(pStaInfo->EapolKeyMsgRecvd));
				Message_setKeyIndex(EapolKeyMsgSend, 0);
				Message_setInstall(EapolKeyMsgSend, 0);
				Message_setKeyAck(EapolKeyMsgSend, 0);
				Message_setKeyMIC(EapolKeyMsgSend, 1);
				Message_setSecure(EapolKeyMsgSend, Message_Secure(pStaInfo->EapolKeyMsgRecvd));
				Message_setError(EapolKeyMsgSend, 0);
				Message_setRequest(EapolKeyMsgSend, 0);
				Message_setReserved(EapolKeyMsgSend, 0);
				Message_setKeyLength(EapolKeyMsgSend, Message_KeyLength(pStaInfo->EapolKeyMsgRecvd));
				Message_CopyReplayCounter(EapolKeyMsgSend, pStaInfo->EapolKeyMsgRecvd);
				memset(IV.Octet, 0, IV.Length);
				Message_setKeyIV(EapolKeyMsgSend, IV);
				memset(RSC.Octet, 0, RSC.Length);
				Message_setKeyRSC(EapolKeyMsgSend, RSC);
				memset(KeyID.Octet, 0, KeyID.Length);
				Message_setKeyID(EapolKeyMsgSend, KeyID);
				Message_setKeyDataLength(EapolKeyMsgSend, 0);

				EapolKeyMsgSend.Length = EAPOLMSG_HDRLEN;
				EAPOLMsgSend.Length = ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN + EapolKeyMsgSend.Length;

				LOG_MSG( "Open and authenticated\n");
#if defined(__ECOS)
				extern void kick_event(unsigned int);
				kick_event(1 << (26)); //set BRIDGE_WLAN_CONNECT
#endif

				pStaInfo->clientMICReportReplayCounter.field.HighPart = 0;
				pStaInfo->clientMICReportReplayCounter.field.LowPart = 0;
				log("client mode 4-Way Message 4-4 done");
			}
		} else {
			if (pStaInfo->clientGkeyUpdate) {
				//send 2nd message of 2-way handshake
				DEBUG_INFO("client mode 2-2,key desc type[%d]\n", Message_KeyDescVer(pStaInfo->EapolKeyMsgRecvd));
				PSKDEBUG("client mode 2-2,key desc type[%d]\n", Message_KeyDescVer(pStaInfo->EapolKeyMsgRecvd));
				memset(EapolKeyMsgSend.Octet, 0, MAX_EAPOLKEYMSG_LEN);

				Message_setDescType(EapolKeyMsgSend, Message_DescType(pStaInfo->EapolKeyMsgRecvd));
				Message_setKeyDescVer(EapolKeyMsgSend, Message_KeyDescVer(pStaInfo->EapolKeyMsgRecvd));
				Message_setKeyType(EapolKeyMsgSend, Message_KeyType(pStaInfo->EapolKeyMsgRecvd));
				Message_setKeyIndex(EapolKeyMsgSend, pGblInfo->GN);
				Message_setInstall(EapolKeyMsgSend, 0);
				Message_setKeyAck(EapolKeyMsgSend, 0);
				Message_setKeyMIC(EapolKeyMsgSend, 1);
				Message_setSecure(EapolKeyMsgSend, 1);
				Message_setError(EapolKeyMsgSend, 0);
				Message_setRequest(EapolKeyMsgSend, 0);
				Message_setReserved(EapolKeyMsgSend, 0);

				Message_setKeyLength(EapolKeyMsgSend, Message_KeyLength(pStaInfo->EapolKeyMsgRecvd));
				Message_CopyReplayCounter(EapolKeyMsgSend, pStaInfo->EapolKeyMsgRecvd);
				memset(Nonce.Octet, 0, KEY_NONCE_LEN);
				Message_setKeyNonce(EapolKeyMsgSend, Nonce);
				memset(IV.Octet, 0, IV.Length);
				Message_setKeyIV(EapolKeyMsgSend, IV);
				memset(RSC.Octet, 0, RSC.Length);
				Message_setKeyRSC(EapolKeyMsgSend, RSC);
				memset(KeyID.Octet, 0, KeyID.Length);
				Message_setKeyID(EapolKeyMsgSend, KeyID);
				Message_setKeyDataLength(EapolKeyMsgSend, 0);

				EapolKeyMsgSend.Length = EAPOLMSG_HDRLEN;
				EAPOLMsgSend.Length = ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN + EapolKeyMsgSend.Length;

				//			printk("client mode Group Message 2-2 done\n");
				pStaInfo->clientGkeyUpdate = 0;
			} else {
				DEBUG_INFO("Send MIC Failure Report\n");
				PSKDEBUG("Send MIC Failure Report\n");
				memset(EapolKeyMsgSend.Octet, 0, MAX_EAPOLKEYMSG_LEN);
#ifdef RTL_WPA2
				if (pStaInfo->RSNEnabled & PSK_WPA2)
					Message_setDescType(EapolKeyMsgSend, desc_type_WPA2);
				else if (pStaInfo->RSNEnabled & PSK_WPA3)	/* AUTH_SAE_STA*/
					Message_setDescType(EapolKeyMsgSend, desc_type_WPA2);
				else
#endif
					Message_setDescType(EapolKeyMsgSend, desc_type_RSN);

#ifdef AUTH_SAE_STA
				if (pStaInfo->AuthKeyMethod == AKM_PSK_SHA256) {
					log("KeyDesc=[0]");
					KeyDescriptorVer = key_desc_ver0;
				} else
#endif
#ifdef CONFIG_IEEE80211W_CLI
				if (pStaInfo->AuthKeyMethod == DOT11_AuthKeyType_PSK_SHA256){
					KeyDescriptorVer = key_desc_ver3;
				}else
#endif
				if ((pGblInfo->MulticastCipher == _CCMP_PRIVACY_) || (pStaInfo->UnicastCipher == _CCMP_PRIVACY_)){
					KeyDescriptorVer = key_desc_ver2;
				}

				Message_setKeyDescVer(EapolKeyMsgSend, KeyDescriptorVer);
				Message_setKeyType(EapolKeyMsgSend, 0);
				Message_setKeyIndex(EapolKeyMsgSend, 0);
				Message_setInstall(EapolKeyMsgSend, 0);
				Message_setKeyAck(EapolKeyMsgSend, 0);
				Message_setKeyMIC(EapolKeyMsgSend, 1);
				Message_setSecure(EapolKeyMsgSend, 1);
				Message_setError(EapolKeyMsgSend, 1);
				Message_setRequest(EapolKeyMsgSend, 1);
				Message_setReserved(EapolKeyMsgSend, 0);
				Message_setKeyLength(EapolKeyMsgSend, 0);

				Message_setReplayCounter(EapolKeyMsgSend, pStaInfo->clientMICReportReplayCounter.field.HighPart, pStaInfo->clientMICReportReplayCounter.field.LowPart);
				INCLargeInteger(&pStaInfo->clientMICReportReplayCounter);

				memset(Nonce.Octet, 0, KEY_NONCE_LEN);
				Message_setKeyNonce(EapolKeyMsgSend, Nonce);
				memset(IV.Octet, 0, IV.Length);
				Message_setKeyIV(EapolKeyMsgSend, IV);
				memset(RSC.Octet, 0, RSC.Length);
				Message_setKeyRSC(EapolKeyMsgSend, RSC);
				memset(KeyID.Octet, 0, KeyID.Length);
				Message_setKeyID(EapolKeyMsgSend, KeyID);
				Message_setKeyDataLength(EapolKeyMsgSend, 0);

				EapolKeyMsgSend.Length = EAPOLMSG_HDRLEN;
				EAPOLMsgSend.Length = ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN + EapolKeyMsgSend.Length;
				send_report = 1;
			}
		}
		pStaInfo->EAPOLMsgSend.Length = EAPOLMsgSend.Length;
		pStaInfo->EapolKeyMsgSend.Length = EapolKeyMsgSend.Length;
	}

	//for record EAPOLMSG length
	pStaInfo->EAPOLMsgSend.Length = EAPOLMsgSend.Length;
	pStaInfo->EapolKeyMsgSend.Length = EapolKeyMsgSend.Length;
	//send_packet--------------------------------------------------------------

	eth_hdr = (struct wlan_ethhdr_t *)EAPOLMsgSend.Octet;
	memcpy(eth_hdr->daddr, pstat->cmn_info.mac_addr, 6);
	memcpy(eth_hdr->saddr, GET_MY_HWADDR, 6);
	eth_hdr->type = htons(LIB1X_ETHER_EAPOL_TYPE);

	eapol = (struct lib1x_eapol *)(EAPOLMsgSend.Octet + ETHER_HDRLEN);
	eapol->protocol_version = LIB1X_EAPOL_VER;
	eapol->packet_type =  LIB1X_EAPOL_KEY;
	eapol->packet_body_length = htons(EapolKeyMsgSend.Length);

	if (!resend) {
		if (send_report)
			CalcMIC(EAPOLMsgSend, KeyDescriptorVer, pStaInfo->PTK, PTK_LEN_EAPOLMIC);
		else
			CalcMIC(EAPOLMsgSend, Message_KeyDescVer(pStaInfo->EapolKeyMsgRecvd), pStaInfo->PTK, PTK_LEN_EAPOLMIC);
	}

	if (EAPOLMsgSend.Length > MAX_EAPOLMSG_LEN) {
		printk("[%s] buffer overrun\n", __func__);
		return;
	}

	pskb = rtl_dev_alloc_skb(priv, MAX_EAPOLMSG_LEN, _SKB_TX_, 1);

	if (pskb == NULL) {
		DEBUG_ERR("Allocate EAP skb failed!\n");
		printk("Allocate EAP skb failed!\n");
		return;
	}

	memcpy(pskb->data, (char *)eth_hdr, EAPOLMsgSend.Length);
	skb_put(pskb, EAPOLMsgSend.Length);

#ifdef ENABLE_RTL_SKB_STATS
	rtl_atomic_dec(&priv->rtl_tx_skb_cnt);
#endif

	if (rtl8192cd_start_xmit(pskb, priv->dev))
		rtl_kfree_skb(priv, pskb, _SKB_TX_);

	if (!pStaInfo->clientHndshkDone) {// only 4-2 need to check the time

	#ifdef CONFIG_IEEE80211R_CLI
		if(FT_ENABLE) {
#ifdef DEBUG_PSK
			panic_printk("FT CLI mode:  Extended mod_timer\n");
#endif
			mod_timer(&pStaInfo->resendTimer, jiffies + RTL_SECONDS_TO_JIFFIES(5));
		} else
	#endif
			mod_timer(&pStaInfo->resendTimer, jiffies + RESEND_TIME);
	}
	//--------------------------------------------------------------send_packet
}
#endif // CLIENT_MODE

static void AuthenticationRequest(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	static unsigned int RC_toggle = 0;

	DEBUG_TRACE;

	// For some STA that can only process if Replay Counter is not 0
	if ((++RC_toggle) % 2)
		INCLargeInteger(&pstat->wpa_sta_info->CurrentReplayCounter);

	INCOctet32_INTEGER(&priv->wpa_global_info->Counter);
	SetNonce(pstat->wpa_sta_info->ANonce, priv->wpa_global_info->Counter);

#ifdef AUTH_SAE
	/*AP mode set PMK here*/
	if(pstat->is_sae_sta || pmkid_cached(pstat)){
		log("is_sae_sta=%d pmkid_caching_idx=%d",pstat->is_sae_sta, pstat->pmkid_caching_idx);
		if(pmkid_cached(pstat)){
			log("use SAE PMK");
			memcpy(pstat->wpa_sta_info->PMK,
				&priv->wpa_global_info->pmkid_cache.pmk[pstat->pmkid_caching_idx], PMK_LEN); //eric-logo
		}
	}
	else
#endif
	{
		log("use pre-generated PMK");
		memcpy(pstat->wpa_sta_info->PMK, priv->wpa_global_info->PSK, PMK_LEN);
	}

//#ifdef RTL_WPA2
#if 0
	CalcPMKID(	akm_sm->PMKID,
				akm_sm->PMK, 	 // PMK
				global->theAuthenticator->global->TxRx->oursupp_addr,   // AA
				global->theAuthenticator->supp_addr); 			// SPA
#endif
	pstat->wpa_sta_info->state = PSK_STATE_PTKSTART;
	pstat->wpa_sta_info->resendCnt = 0;

#ifdef CONFIG_IEEE80211R
	if (pstat->ft_state == state_imd_assoc)
		pstat->ft_state = state_imd_4way;
#endif

	//send 1st message
	SendEAPOL(priv, pstat, 0);
}

//-------------------------------------------------------------
// Start 2-way handshake after receiving 4th message
//-------------------------------------------------------------
static void UpdateGK(struct rtl8192cd_priv *priv)
{
	WPA_GLOBAL_INFO *pGblInfo = priv->wpa_global_info;
	struct stat_info *pstat;
	int i;
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif

	SMP_LOCK_PSK_GKREKEY(flags);
	//------------------------------------------------------------
	// Execute Global Group key state machine
	//------------------------------------------------------------
	if (pGblInfo->GTKAuthenticator && (pGblInfo->GTKRekey || pGblInfo->GInitAKeys)) {
		pGblInfo->GTKRekey = FALSE;
		pGblInfo->GInitAKeys = FALSE; // david+2006-04-04, fix the issue of re-generating group key

		INCOctet32_INTEGER(&pGblInfo->Counter);

		// kenny:??? GNonce should be a random number ???
		SetNonce(pGblInfo->GNonce , pGblInfo->Counter);
		CalcGTK(GET_MY_HWADDR, pGblInfo->GNonce.Octet,
				pGblInfo->GMK, GMK_LEN, pGblInfo->GTK[pGblInfo->GN], GTK_LEN, GMK_EXPANSION_CONST);

#ifdef DEBUG_PSK
		debug_out("PSK: Generated GTK=", pGblInfo->GTK[pGblInfo->GN], GTK_LEN);
#endif

		pGblInfo->GUpdateStationKeys = TRUE;
		pGblInfo->GkeyReady = FALSE;

		if (timer_pending(&pGblInfo->GKRekeyTimer))
			del_timer_sync(&pGblInfo->GKRekeyTimer);

		//---- In the case of updating GK to all STAs, only the STA that has finished
		//---- 4-way handshake is needed to be sent with 2-way handshake
		//gkm_sm->GKeyDoneStations = auth->NumOfSupplicant;
		pGblInfo->GKeyDoneStations = 0;

		for (i = 0; i < NUM_STAT; i++) {
			if (priv->pshare->aidarray[i] && (priv->pshare->aidarray[i]->used == TRUE)) {
#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
				if (priv != priv->pshare->aidarray[i]->priv)
					continue;
#endif
				pstat = &priv->pshare->aidarray[i]->station;
				if (pstat->expire_to == 0) {
					free_stainfo(priv, pstat);
					continue;
				}

				if ((pstat->state & WIFI_ASOC_STATE) &&
						(pstat->wpa_sta_info->state == PSK_STATE_PTKINITDONE))
					pGblInfo->GKeyDoneStations++;
			}
		}
	}

#ifdef CONFIG_IEEE80211W
#ifdef CONFIG_IEEE80211R
	if (!FT_ENABLE)
#endif
	if (priv->pmib->dot1180211AuthEntry.dot11IEEE80211W != NO_MGMT_FRAME_PROTECTION) {
		//int ii;
		PMFDEBUG("Calculate IGTK\n");

		CalcGTK(GET_MY_HWADDR, pGblInfo->GNonce.Octet,
				pGblInfo->GMK, GMK_LEN, pGblInfo->IGTK[pGblInfo->GN_igtk - 4], IGTK_LEN, (u_char*)IGMK_EXPANSION_CONST);
        #ifdef CONFIG_IEEE80211W_AP_DEBUG
		panic_printk("IGTK[%d]\n", pGblInfo->GN_igtk - 4);
		dump_hex("IGTK",pGblInfo->IGTK[pGblInfo->GN_igtk - 4],IGTK_LEN);
        #endif
	}
#endif // CONFIG_IEEE80211W

	// PN initialized to 1 when temporal key is initialized or refreshed
	//GET_MIB(priv)->dot11GroupKeysTable.dot11EncryptKey.dot11TXPN48.val48 = 1;

	//------------------------------------------------------------
	// Execute Group key state machine of each STA
	//------------------------------------------------------------
	for (i = 0; i < NUM_STAT; i++) {
		if (priv->pshare->aidarray[i] == NULL)
			continue;
		pstat = &priv->pshare->aidarray[i]->station;
		if ((priv->pshare->aidarray[i]->used != TRUE) || !(pstat->state & WIFI_ASOC_STATE))
			continue;

#if defined(UNIVERSAL_REPEATER) || defined(MBSSID)
		if (priv != priv->pshare->aidarray[i]->priv)
			continue;
#endif

		//---- Group key handshake to only one supplicant ----
		if (pstat->wpa_sta_info->state == PSK_STATE_PTKINITDONE &&
				(pGblInfo->GkeyReady && pstat->wpa_sta_info->PInitAKeys)) {
			pstat->wpa_sta_info->PInitAKeys = FALSE;
			pstat->wpa_sta_info->gstate = PSK_GSTATE_REKEYNEGOTIATING; // set proper gstat, david+2006-04-06
			pstat->wpa_sta_info->resendCnt = 0;
			SendEAPOL(priv, pstat, 0);
		}
		//---- Updata group key to all supplicant----
		else if (pstat->wpa_sta_info->state == PSK_STATE_PTKINITDONE &&  //Done 4-way handshake
				 (pGblInfo->GUpdateStationKeys ||                     //When new key is generated
				  pstat->wpa_sta_info->gstate == PSK_GSTATE_REKEYNEGOTIATING))  { //1st message is not yet sent
			pstat->wpa_sta_info->PInitAKeys = FALSE;
			pstat->wpa_sta_info->gstate = PSK_GSTATE_REKEYNEGOTIATING; // set proper gstat, david+2006-04-06
			pstat->wpa_sta_info->resendCnt = 0;
			SendEAPOL(priv, pstat, 0);
		}
	}
	pGblInfo->GUpdateStationKeys = FALSE;

	SMP_UNLOCK_PSK_GKREKEY(flags);
};


int get_ptk_len(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{

	WPA_STA_INFO *pStaInfo = pstat->wpa_sta_info;
	unsigned char AKM = pStaInfo->AuthKeyMethod;
	unsigned char UC = pStaInfo->UnicastCipher;
	int kck_kek_len = 0;
	int tk_len = 0;

	if (AKM >= AKM_RSN || AKM <= AKM_TYPE_11) {
		kck_kek_len=32;
	} else if((AKM >= AKM_TYPE_12 || AKM <= AKM_TYPE_13)){
		kck_kek_len=56;
	}else{
		printk("unknow akm [%d]!\n",AKM);
	}

	if ( UC == ENC_WEP40 )
		tk_len = 5;
	 else if(UC == ENC_WEP104)
		tk_len = 13;
	else if(UC == ENC_CCMP128||UC == ENC_BIP_CMAC128||UC == ENC_GCMP128||UC == ENC_BIP_GMAC128)
		tk_len = 16;
	else if(UC == ENC_TKIP||UC == ENC_GCMP256||UC == ENC_CCMP256||UC == ENC_BIP_GMAC256)
		tk_len = 32;
	else
		printk("unknown UC[%d]!\n",UC);

	//log("AKM[%d] UC[%d]ptk len[%d]", AKM,UC,(kck_kek_len + tk_len));
	return (kck_kek_len + tk_len);
}


int use_sha256(unsigned char akm)
{
	/*AKM type 3~11 use sha256,IEEE80211(2016) Table 9-133*/
	if (akm >= AKM_FT_1X && akm <= AKM_TYPE_11)
		return 1;
	else
		return 0;
}


static void EAPOLKeyRecvd(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	WPA_GLOBAL_INFO *pGblInfo = priv->wpa_global_info;
	WPA_STA_INFO *pStaInfo = pstat->wpa_sta_info;
	LARGE_INTEGER recievedRC;
	struct lib1x_eapol *eapol;
	OCTET_STRING tmpSNonce;
#ifdef CONFIG_IEEE80211R
	OCTET_STRING tmpKeyData;
	unsigned int status, reason = unspec_reason;
#endif
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif

	DEBUG_TRACE;
	log("\n===========(RX)EAPOL KEY===========\n");

	eapol = ( struct lib1x_eapol * ) ( pstat->wpa_sta_info->EAPOLMsgRecvd.Octet + ETHER_HDRLEN );
	if (eapol->packet_type != LIB1X_EAPOL_KEY) {
#ifdef DEBUG_PSK
		printk("Not Eapol-key pkt (type %d), drop\n", eapol->packet_type);
#endif
#if defined(CONFIG_WLAN_STATS_EXTENTION)
		priv->ext_stats.total_psk_fail++;
#endif
		return;
	}

	pStaInfo->EapolKeyMsgRecvd.Octet = pStaInfo->EAPOLMsgRecvd.Octet + ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN;

#ifdef DEBUG_PSK
	LARGE_INTEGER li2;
	Message_ReplayCounter_OC2LI(pStaInfo->EapolKeyMsgRecvd, &li2);

	printk("PSK: Rx a EAPOL %s, len=%x, started=%d, replay=%d\n", STATE2RXMSG(pstat), pStaInfo->EAPOLMsgRecvd.Length,
        pStaInfo->ReplayCounterStarted.field.LowPart, li2.field.LowPart);
	debug_out(NULL, pstat->wpa_sta_info->EAPOLMsgRecvd.Octet, pStaInfo->EAPOLMsgRecvd.Length);
#endif

	//----IEEE 802.11-03/156r2. MIC report : (1)MIC bit (2)error bit (3) request bit
	//----Check if it is MIC failure report. If it is, indicate to driver
	if (Message_KeyMIC(pStaInfo->EapolKeyMsgRecvd) && Message_Error(pStaInfo->EapolKeyMsgRecvd)
			&& Message_Request(pStaInfo->EapolKeyMsgRecvd)) {
#ifdef DEBUG_PSK
		printk("PSK: Rx MIC errir report from client\n");
#endif
		ToDrv_IndicateMICFail(priv, pstat->cmn_info.mac_addr);
#if defined(CONFIG_WLAN_STATS_EXTENTION)
		priv->ext_stats.total_psk_fail++;
		priv->ext_stats.total_mic_fail++;
#endif
		return;
	}

	if (Message_KeyType(pStaInfo->EapolKeyMsgRecvd) == type_Pairwise) {
		switch (pStaInfo->state) {
		case PSK_STATE_PTKSTART:
			//---- Receive 2nd message and send third
			DEBUG_INFO("4-2\n");
			PSKDEBUG("4-2\n");

			//check replay counter
			Message_ReplayCounter_OC2LI(pStaInfo->EapolKeyMsgRecvd, &recievedRC);
			INCLargeInteger(&recievedRC);
#if 0
			if (!(pStaInfo->CurrentReplayCounter.field.HighPart == recievedRC.field.HighPart
					&& pStaInfo->CurrentReplayCounter.field.LowPart == recievedRC.field.LowPart)) {
#else
            // Only the latest two replay counters are accepted and going to 4-3.
            if (!((pStaInfo->ReplayCounterStarted.field.HighPart == recievedRC.field.HighPart
                    && pStaInfo->ReplayCounterStarted.field.LowPart == recievedRC.field.LowPart)
                || (pStaInfo->CurrentReplayCounter.field.HighPart == recievedRC.field.HighPart
                    && pStaInfo->CurrentReplayCounter.field.LowPart == recievedRC.field.LowPart))) {
#endif
				DEBUG_INFO("4-2: ERROR_NONEEQUL_REPLAYCOUNTER\n");
				log("4-2: ERROR_NONEEQUL_REPLAYCOUNTER\n");
				DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_SERIOUS, "4-2: ERROR_NONEEQUL_REPLAYCOUNTER\n");
				break;
			}

check_msg2:
			//pStaInfo->SNonce = Message_KeyNonce(pStaInfo->EapolKeyMsgRecvd);
			tmpSNonce = Message_KeyNonce(pStaInfo->EapolKeyMsgRecvd);
			memcpy(pStaInfo->SNonce.Octet, tmpSNonce.Octet, KEY_NONCE_LEN);
			pStaInfo->SNonce.Length = tmpSNonce.Length;
#ifdef CONFIG_IEEE80211R
			if (FT_ENABLE && pStaInfo->isFT) {
				CalcFTPTK(priv, pstat, pStaInfo->PTK,
					(pStaInfo->AuthKeyMethod >= DOT11_AuthKeyType_FT8021x) ? 48 : PTK_LEN_TKIP);
			} else
#endif
			CalcPTK(pStaInfo->EAPOLMsgRecvd.Octet, pStaInfo->EAPOLMsgRecvd.Octet + 6,
					pStaInfo->ANonce.Octet, pStaInfo->SNonce.Octet,
					pStaInfo->PMK, PMK_LEN,
					pStaInfo->PTK, get_ptk_len(priv,pstat)
#ifdef CONFIG_IEEE80211W
					, use_sha256(pStaInfo->AuthKeyMethod)
#endif
				   );
			#if 1     // Mark.CS_debug
			log("use_sha256:%d", use_sha256(pStaInfo->AuthKeyMethod));
			dump_hex("AP side PTK", pStaInfo->PTK, 32);
			#endif
#ifdef DEBUG_PSK
			debug_out("PSK: Generated PTK=", pStaInfo->PTK, PTK_LEN_TKIP);
#endif

#ifdef CONFIG_IEEE80211R
			if (FT_ENABLE && pStaInfo->isFT) {
				tmpKeyData = Message_KeyData(pStaInfo->EapolKeyMsgRecvd, Message_KeyDataLength(pStaInfo->EapolKeyMsgRecvd));
				if (ft_check_imd_4way(priv, pstat, tmpKeyData.Octet, tmpKeyData.Length, &status)) {
				switch (status)
				{
				case __STATS_INVALID_IE_:
					DEBUG_ERR("4-2: RSNIE not present in FT Message 2\n");
					DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_SERIOUS, "4-2: RSNIE not present in FT Message 2\n");
					reason = RSN_invalid_info_element;
					break;
				case _STATS_INVALID_MDIE_:
					DEBUG_ERR("4-2: Invalid MDIE in FT Message 2\n");
					DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_SERIOUS, "4-2: Invalid MDIE in FT Message 2\n");
					reason = RSN_invalid_info_element;
					break;
				case __STATS_INVALID_AKMP_:
					DEBUG_ERR("4-2: Invalid AKM Suite\n");
					DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_SERIOUS, "4-2: Invalid AKM Suite\n");
					reason = RSN_AKMP_not_valid;
					break;
				case _STATS_INVALID_PMKID_:
					DEBUG_ERR("4-2: PMKR1-Name not match\n");
					DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_SERIOUS, "4-2: PMKR1-Name not match\n");
					reason = RSN_invalid_info_element;
					break;
				default:
					break;
				}
					DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_SERIOUS, "FT connection fail, reason=%d\n", reason);
					ToDrv_DisconnectSTA(priv, pstat, reason);
					pStaInfo->state = PSK_STATE_IDLE;
#if defined(CONFIG_WLAN_STATS_EXTENTION)
					priv->ext_stats.total_psk_fail++;
#endif
					break;
				}
			}
#endif

			if (!CheckMIC(pStaInfo->EAPOLMsgRecvd, pStaInfo->PTK, PTK_LEN_EAPOLMIC)) {
				if (priv->pmib->dot1180211AuthEntry.dot11PassPhraseGuest[0]) {
					CalcPTK(pStaInfo->EAPOLMsgRecvd.Octet, pStaInfo->EAPOLMsgRecvd.Octet + 6,
							pStaInfo->ANonce.Octet, pStaInfo->SNonce.Octet,
							pGblInfo->PSKGuest, PMK_LEN,
							pStaInfo->PTK, get_ptk_len(priv,pstat)
#ifdef CONFIG_IEEE80211W
							, use_sha256(pStaInfo->AuthKeyMethod)
#endif
						);
					if (CheckMIC(pStaInfo->EAPOLMsgRecvd, pStaInfo->PTK, PTK_LEN_EAPOLMIC)) {
						pStaInfo->isGuest = 1;
						goto cont_msg;
					}
				}

				DEBUG_ERR("4-2: ERROR_MIC_FAIL\n");
				DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_SERIOUS, "4-2: ERROR_MIC_FAIL\n");
#if defined(CONFIG_WLAN_STATS_EXTENTION)
				priv->ext_stats.total_mic_fail++;
				priv->ext_stats.total_psk_fail++;
#endif
#if 0
				if (global->RSNVariable.PMKCached ) {
					printf("\n%s:%d del_pmksa due to 4-2 ERROR_MIC_FAIL\n", (char *)__FUNCTION__, __LINE__);
					global->RSNVariable.PMKCached = FALSE;
					del_pmksa_by_spa(global->theAuthenticator->supp_addr);
				}
#endif

				LOG_MSG("Authentication failled! (4-2: MIC error)\n");
				#ifdef RTK_MULTI_AP
				client_leave_notify(BSSID, pstat->cmn_info.mac_addr);
				#endif
#if defined(CONFIG_RTL8196B_TR) || defined(CONFIG_RTL865X_SC) || defined(CONFIG_RTL865X_AC) || defined(CONFIG_RTL865X_KLD) || defined(CONFIG_RTL8196B_KLD) || defined(CONFIG_RTL8196C_KLD) || defined(CONFIG_RTL8196C_EC)
				LOG_MSG_NOTICE("Authentication failed;note:%02x-%02x-%02x-%02x-%02x-%02x;\n",
							   pstat->cmn_info.mac_addr[0],
							   pstat->cmn_info.mac_addr[1],
							   pstat->cmn_info.mac_addr[2],
							   pstat->cmn_info.mac_addr[3],
							   pstat->cmn_info.mac_addr[4],
							   pstat->cmn_info.mac_addr[5]);
#elif defined(CONFIG_RTL8196B_TLD)
				LOG_MSG_DEL("[WLAN access rejected: incorrect security] from MAC address: %02x:%02x:%02x:%02x:%02x:%02x,\n",
							pstat->cmn_info.mac_addr[0],
							pstat->cmn_info.mac_addr[1],
							pstat->cmn_info.mac_addr[2],
							pstat->cmn_info.mac_addr[3],
							pstat->cmn_info.mac_addr[4],
							pstat->cmn_info.mac_addr[5]);
#elif defined(__ECOS) && defined(CONFIG_SDIO_HCI)
				if (priv->sta_status_cb_func)
					priv->sta_status_cb_func("wlan0", pstat->cmn_info.mac_addr, WIFI_STA_STATUS_AUTH_FAIL);
#endif
#ifdef RTK_WLAN_EVENT_INDICATE
				rtk_wlan_event_indicate(priv->dev->name, WIFI_CONNECT_FAIL, pstat->cmn_info.mac_addr, RSN_MIC_failure);
#endif
#ifdef RTK_SMART_ROAMING
#ifdef STA_ROAMING_CHECK
				if(priv->pmib->miscEntry.telco_selected == TELCO_CMCC)
                	notify_connect_fail(priv, pstat->cmn_info.mac_addr, RTL_LINK_CONNECT_FAIL);
#endif
#endif
#ifdef PROC_STA_CONN_FAIL_INFO
				{
					int i;
					for (i=0; i<64; i++) {
						if (!priv->sta_conn_fail[i].used) {
							priv->sta_conn_fail[i].used = TRUE;
							priv->sta_conn_fail[i].error_state = RSN_MIC_failure;
							memcpy(priv->sta_conn_fail[i].addr, pstat->cmn_info.mac_addr, MACADDRLEN);
							break;
						}
						else {
							if (!memcmp(priv->sta_conn_fail[i].addr, pstat->cmn_info.mac_addr, MACADDRLEN)) {
								priv->sta_conn_fail[i].error_state = RSN_MIC_failure;
								break;
							}
						}
					}
				}
#endif
#ifdef STA_ASSOC_STATISTIC
				add_sta_assoc_status(priv, pstat->cmn_info.mac_addr, pstat->rssi, RSN_MIC_failure | 0x300);
#endif
#ifdef ERR_ACCESS_CNTR
				{
					int i = 0, found = 0;
					for (i=0; i<MAX_ERR_ACCESS_CNTR; i++) {
						if (priv->err_ac_list[i].used) {
							if (!memcmp(pstat->cmn_info.mac_addr, priv->err_ac_list[i].mac, MACADDRLEN)) {
								priv->err_ac_list[i].num++;
								found++;
								break;
							}
						}
						else
							break;
					}
					if (!found && (i != MAX_ERR_ACCESS_CNTR)) {
						priv->err_ac_list[i].used = TRUE;
						memcpy(priv->err_ac_list[i].mac, pstat->cmn_info.mac_addr, MACADDRLEN);
						priv->err_ac_list[i].num++;
					}
				}
#endif
				/* Fix, to prevent prompt "Connection Timeout" when MacBook connect with incorrect PSK
				ToDrv_DisconnectSTA(priv, pstat, RSN_MIC_failure);
				pStaInfo->state = PSK_STATE_IDLE;
				*/
				break;
			}
cont_msg:
			pStaInfo->resendCnt = 0;
			/* Fix, to prevent display connection timeout when MacBook connect with incorrect PSK.
			   Relocate cancel timer here to make 4-1 retried while 4-2 MIC error, then MacBook will prompt "Incorrect Password" */
			// delete resend timer
			SMP_LOCK_PSK_RESEND(flags);
			if (timer_pending(&pStaInfo->resendTimer)){
				del_timer(&pStaInfo->resendTimer);
			}
			SMP_UNLOCK_PSK_RESEND(flags);

			pStaInfo->state = PSK_STATE_PTKINITNEGOTIATING;
			SendEAPOL(priv, pstat, 0);	// send msg 3
			break;

		case PSK_STATE_PTKINITNEGOTIATING:
			//---- Receive 4th message ----
			DEBUG_INFO("4-4\n");
			PSKDEBUG("4-4\n");

			// test 2nd or 4th message
// check replay counter to determine if msg 2 or 4 received --------------
			if (Message_KeyDataLength(pStaInfo->EapolKeyMsgRecvd) != 0) {
			    if (Message_EqualReplayCounter(pStaInfo->ReplayCounterStarted, pStaInfo->EapolKeyMsgRecvd)) {
//---------------------------------------------------- david+1-12-2007

				DEBUG_INFO("4-2 in akmsm_PTKINITNEGOTIATING: resend 4-3\n");
				PSKDEBUG("4-2 in akmsm_PTKINITNEGOTIATING: resend 4-3\n");

#if 0 /* Don't check replay counter during dup 4-2*/
#ifdef RTL_WPA2
				Message_ReplayCounter_OC2LI(global->EapolKeyMsgRecvd, &recievedRC);
				INCLargeInteger(&recievedRC);
				if ( !(global->akm_sm->CurrentReplayCounter.field.HighPart == recievedRC.field.HighPart
						&& global->akm_sm->CurrentReplayCounter.field.LowPart == recievedRC.field.LowPart))
#else
				if (!Message_EqualReplayCounter(global->akm_sm->CurrentReplayCounter, global->EapolKeyMsgRecvd))
#endif
				{
#ifdef FOURWAY_DEBUG
					printf("4-2: ERROR_NONEEQUL_REPLAYCOUNTER\n");
					printf("global->akm_sm->CurrentReplayCounter.field.LowPart = %d\n", global->akm_sm->CurrentReplayCounter.field.LowPart);
					printf("recievedRC.field.LowPart = %d\n", recievedRC.field.LowPart);
#endif
					retVal = ERROR_NONEEQUL_REPLAYCOUNTER;
					k
				} else
#endif // Don't check replay counter during dup 4-2
//#ifndef RTL_WPA2
#if 0
					// kenny: already increase CurrentReplayCounter after 4-1. Do it at the end of 4-2
					INCLargeInteger(&global->akm_sm->CurrentReplayCounter);
#endif

				    goto check_msg2;
    			} else {
#ifdef DEBUG_PSK
    			    printk("4-2 in akmsm_PTKINITNEGOTIATING: Ignored older 4-2 packets!\n");
#endif
    			    break;
                }
			}

			if (!CheckMIC(pStaInfo->EAPOLMsgRecvd, pStaInfo->PTK, PTK_LEN_EAPOLMIC)) { // errror
				DEBUG_ERR("4-4: RSN_MIC_failure\n");
				DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_SERIOUS, "4-4: RSN_MIC_failure\n");
#if defined(CONFIG_WLAN_STATS_EXTENTION)
				priv->ext_stats.total_mic_fail++;
				priv->ext_stats.total_psk_fail++;
#endif
#if 0
				if (global->RSNVariable.PMKCached ) {
					printf("\n%s:%d del_pmksa due to 4-4 RSN_MIC_failure\n", (char *)__FUNCTION__, __LINE__);
					global->RSNVariable.PMKCached = FALSE;
					del_pmksa_by_spa(global->theAuthenticator->supp_addr);
				}
#endif
				LOG_MSG("Authentication failled! (4-4: MIC error)\n");

#if defined(CONFIG_RTL8196B_TR) || defined(CONFIG_RTL865X_SC) || defined(CONFIG_RTL865X_AC) || defined(CONFIG_RTL865X_KLD) || defined(CONFIG_RTL8196B_KLD) || defined(CONFIG_RTL8196C_KLD) || defined(CONFIG_RTL8196C_EC)
				LOG_MSG_NOTICE("Authentication failed;note:%02x-%02x-%02x-%02x-%02x-%02x;\n",
							   pstat->cmn_info.mac_addr[0],
							   pstat->cmn_info.mac_addr[1],
							   pstat->cmn_info.mac_addr[2],
							   pstat->cmn_info.mac_addr[3],
							   pstat->cmn_info.mac_addr[4],
							   pstat->cmn_info.mac_addr[5]);
#elif defined(CONFIG_RTL8196B_TLD)
				LOG_MSG_DEL("[WLAN access rejected: incorrect security] from MAC address: %02x:%02x:%02x:%02x:%02x:%02x,\n",
							pstat->cmn_info.mac_addr[0],
							pstat->cmn_info.mac_addr[1],
							pstat->cmn_info.mac_addr[2],
							pstat->cmn_info.mac_addr[3],
							pstat->cmn_info.mac_addr[4],
							pstat->cmn_info.mac_addr[5]);
#elif defined(__ECOS) && defined(CONFIG_SDIO_HCI)
				if (priv->sta_status_cb_func)
					priv->sta_status_cb_func("wlan0", pstat->cmn_info.mac_addr, WIFI_STA_STATUS_AUTH_FAIL);
#endif
#ifdef RTK_WLAN_EVENT_INDICATE
				rtk_wlan_event_indicate(priv->dev->name, WIFI_CONNECT_FAIL, pstat->cmn_info.mac_addr, RSN_MIC_failure);
#endif
#ifdef RTK_SMART_ROAMING
#ifdef STA_ROAMING_CHECK
				if(priv->pmib->miscEntry.telco_selected == TELCO_CMCC)
                	notify_connect_fail(priv, pstat->cmn_info.mac_addr, RTL_LINK_CONNECT_FAIL);
#endif
#endif
#ifdef PROC_STA_CONN_FAIL_INFO
				{
					int i;
					for (i=0; i<64; i++) {
						if (!priv->sta_conn_fail[i].used) {
							priv->sta_conn_fail[i].used = TRUE;
							priv->sta_conn_fail[i].error_state = RSN_MIC_failure;
							memcpy(priv->sta_conn_fail[i].addr, pstat->cmn_info.mac_addr, MACADDRLEN);
							break;
						}
						else {
							if (!memcmp(priv->sta_conn_fail[i].addr, pstat->cmn_info.mac_addr, MACADDRLEN)) {
								priv->sta_conn_fail[i].error_state = RSN_MIC_failure;
								break;
							}
						}
					}
				}
#endif
#ifdef STA_ASSOC_STATISTIC
				add_sta_assoc_status(priv, pstat->cmn_info.mac_addr, pstat->rssi, RSN_MIC_failure | 0x300);
#endif
#ifdef ERR_ACCESS_CNTR
				{
					int i = 0, found = 0;
					for (i=0; i<MAX_ERR_ACCESS_CNTR; i++) {
						if (priv->err_ac_list[i].used) {
							if (!memcmp(pstat->cmn_info.mac_addr, priv->err_ac_list[i].mac, MACADDRLEN)) {
								priv->err_ac_list[i].num++;
								found++;
								break;
							}
						}
						else
							break;
					}
					if (!found && (i != MAX_ERR_ACCESS_CNTR)) {
						priv->err_ac_list[i].used = TRUE;
						memcpy(priv->err_ac_list[i].mac, pstat->cmn_info.mac_addr, MACADDRLEN);
						priv->err_ac_list[i].num++;
					}
				}
#endif
				/* Refine, retry 4-3 instead dropping connect directly
				ToDrv_DisconnectSTA(priv, pstat, RSN_MIC_failure);
				pStaInfo->state = PSK_STATE_IDLE;
				*/
				break;
			}

			pStaInfo->resendCnt = 0;
			/* Refine, relocate cancel timer here to make 4-3 retried while 4-4 MIC error */
			// delete resend timer
			SMP_LOCK_PSK_RESEND(flags);
			if (timer_pending(&pStaInfo->resendTimer)){
				del_timer(&pStaInfo->resendTimer);
			}
			SMP_UNLOCK_PSK_RESEND(flags);
			LOG_MSG("Open and authenticated\n");

#if defined(CONFIG_RTL8196B_TR) || defined(CONFIG_RTL865X_SC) || defined(CONFIG_RTL865X_AC) || defined(CONFIG_RTL865X_KLD) || defined(CONFIG_RTL8196B_KLD) || defined(CONFIG_RTL8196C_KLD) || defined(CONFIG_RTL8196C_EC)
			LOG_MSG_NOTICE("Authentication Success;note:%02x-%02x-%02x-%02x-%02x-%02x;\n",
						   pstat->cmn_info.mac_addr[0],
						   pstat->cmn_info.mac_addr[1],
						   pstat->cmn_info.mac_addr[2],
						   pstat->cmn_info.mac_addr[3],
						   pstat->cmn_info.mac_addr[4],
						   pstat->cmn_info.mac_addr[5]);
#elif defined(CONFIG_RTL8196B_TLD)
			if (!list_empty(&priv->wlan_acl_list)) {
				LOG_MSG_DEL("[WLAN access allowed] from MAC: %02x:%02x:%02x:%02x:%02x:%02x,\n",
							pstat->cmn_info.mac_addr[0],
							pstat->cmn_info.mac_addr[1],
							pstat->cmn_info.mac_addr[2],
							pstat->cmn_info.mac_addr[3],
							pstat->cmn_info.mac_addr[4],
							pstat->cmn_info.mac_addr[5]);
			}
#endif
#ifdef STA_ASSOC_STATISTIC
			add_sta_assoc_status(priv, pstat->cmn_info.mac_addr, pstat->rssi, 0x600);
#endif
#ifdef RTK_WLAN_EVENT_INDICATE
			rtk_wlan_event_indicate(priv->dev->name, WIFI_CONNECT_SUCCESS, pstat->cmn_info.mac_addr, 0);
#endif
//#ifdef RTL_WPA2_PREAUTH
#if 0
			// update ANonce for next 4-way handshake
			SetNonce(akm_sm->ANonce, global->auth->Counter);
#endif

			//MLME-SETKEYS.request
			INCLargeInteger(&pStaInfo->CurrentReplayCounter);
			// kenny: n+2
			INCLargeInteger(&pStaInfo->CurrentReplayCounter);

			ToDrv_SetPTK(priv, pstat);
			if (pStaInfo->isGuest)
				ToDrv_SetPort(priv, pstat, DOT11_PortStatus_Guest);
			else
				ToDrv_SetPort(priv, pstat, DOT11_PortStatus_Authorized);

#if defined(__ECOS) && defined(CONFIG_SDIO_HCI)
			if (priv->sta_status_cb_func)
				priv->sta_status_cb_func("wlan0", pstat->cmn_info.mac_addr, WIFI_STA_STATUS_PORT_AUTHORIZED);
#endif

// david+2006-04-04, fix the issue of re-generating group key, and need not
// update group key in WPA2
//			pGblInfo->GInitAKeys = TRUE;
#ifdef RTL_WPA2
			if (!(pStaInfo->RSNEnabled & PSK_WPA2))
#endif
				pStaInfo->PInitAKeys = TRUE;
			pStaInfo->state = PSK_STATE_PTKINITDONE;
			pStaInfo->gstate = PSK_GSTATE_REKEYNEGOTIATING;

			//lib1x_akmsm_UpdateGK_proc() calls lib1x_akmsm_SendEAPOL_proc for 2-way
			//if group key sent is needed, send msg 1 of 2-way handshake
#ifdef RTL_WPA2
			if (pStaInfo->RSNEnabled & PSK_WPA2) {
				//------------------------------------------------------
				// Only when the group state machine is in the state of
				// (1) The first STA Connected,
				// (2) UPDATE GK to all station
				// does the GKeyDoneStations needed to be decreased
				//------------------------------------------------------

				if (pGblInfo->GKeyDoneStations > 0)
					pGblInfo->GKeyDoneStations--;

				//Avaya akm_sm->TimeoutCtr = 0;
				//To Do : set port secure to driver
//				global->portSecure = TRUE;
				//akm_sm->state = akmsm_PTKINITDONE;

				pStaInfo->gstate = PSK_GSTATE_REKEYESTABLISHED;

				if (pGblInfo->GKeyDoneStations == 0 && !pGblInfo->GkeyReady) {
					ToDrv_SetGTK(priv);
#ifdef CONFIG_IEEE80211W
					/*check DUT 11w cap*/
					if (priv->pmib->dot1180211AuthEntry.dot11IEEE80211W != NO_MGMT_FRAME_PROTECTION){
						/*check Peer's 11w cap [SAE] [WPA3] S10*/
						if( pStaInfo->rsn_cap_mfpr ||  pStaInfo->rsn_cap_mfpc){
							ToDrv_SetIGTK(priv);
						}else{
							log("peer akm[%d] mfpc[%d] mfpr[%d]  don't SET IGTK",
								pStaInfo->AuthKeyMethod,pStaInfo->rsn_cap_mfpc,pStaInfo->rsn_cap_mfpr);
						}
					}
#endif
					pGblInfo->GkeyReady = TRUE;
					pGblInfo->GResetCounter = TRUE;

					// start groupkey rekey timer
					if (priv->pmib->dot1180211AuthEntry.dot11GKRekeyTime)
						mod_timer(&pGblInfo->GKRekeyTimer, jiffies + RTL_SECONDS_TO_JIFFIES(priv->pmib->dot1180211AuthEntry.dot11GKRekeyTime));
				}
#if 0
				if (global->RSNVariable.PMKCached) {
					global->RSNVariable.PMKCached = FALSE;  // reset
				}
#endif
#ifdef CONFIG_IEEE80211W
				/*[SAE] [WPA3] S10*/
				if( pStaInfo->rsn_cap_mfpr || pStaInfo->rsn_cap_mfpc){
					ToDrv_SetPMF(priv, pstat);
				}else{
					log("peer akm[%d] mfpc[%d] mfpr[%d]  don't SET PMF",
						pStaInfo->AuthKeyMethod,pStaInfo->rsn_cap_mfpc,pStaInfo->rsn_cap_mfpr);
				}
#endif
#ifdef CONFIG_IEEE80211R
				if (FT_ENABLE && pStaInfo->isFT) {
					FT_IndicateEvent(priv, pstat, DOT11_EVENT_FT_IMD_ASSOC_IND, NULL);

					// Set key expire timer
					if (priv->pmib->dot11FTEntry.dot11FTR0KeyLifetime) {
						set_r0key_expire_time(priv, pstat->cmn_info.mac_addr, priv->pmib->dot11FTEntry.dot11FTR0KeyLifetime * 60);
					}
				}
#endif

				DEBUG_INFO("\nWPA2: 4-way handshake done\n");
				PSKDEBUG("WPA2: 4-way handshake done\n");
				//Wifi Logo FFD fragment attack, clear fragment list after 4-way handshake.
				free_sta_frag_list(priv, pstat);

				#if 1  // Mark.CS_debug
				#ifdef AUTH_SAE
				log("WPA2: 4-way handshake done");
				
				unsigned char pmkid_cache_idx=0;
				struct candidate *peer = NULL;
				
				pmkid_cache_idx = search_by_mac_pmkid_cache(priv, pstat->cmn_info.mac_addr);
				if(pmkid_cache_idx<NUM_PMKID_CACHE && pstat->is_sae_sta==1){
					peer =
			    		create_candidate(priv, pstat->cmn_info.mac_addr, GET_MY_HWADDR, (priv->curr_open >= priv->open_threshold));
					if (peer == NULL) {
						log("create_candidate for peer[%pM] fail ", pstat->cmn_info.mac_addr);
						return -1;
					}
					peer->t0 = 0;
					peer->t1 = jiffies;
					log("add_peer[%pm]",pstat->cmn_info.mac_addr);
				}
				#endif
				#endif
				
#ifdef RTK_SMART_ROAMING
				if(SMART_ROAMING_ENABLE
#ifdef STA_ROAMING_CHECK
				|| RTL_LINK_ROAMING_ENABLE
#endif
				){
					DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_TRACE, "sta=%02x:%02x:%02x:%02x:%02x:%02x WPA2: 4-way handshake done\n",
						pstat->cmn_info.mac_addr[0], pstat->cmn_info.mac_addr[1], pstat->cmn_info.mac_addr[2],
						pstat->cmn_info.mac_addr[3], pstat->cmn_info.mac_addr[4], pstat->cmn_info.mac_addr[5]);
					notify_new_sta(priv, pstat->cmn_info.mac_addr, ASSOC_NEW_STA, pstat->rssi);
				}
#endif
#ifdef RTK_MULTI_AP
				client_join_notify(BSSID, pstat->cmn_info.mac_addr);
#endif
			}
#endif	// RTL_WPA2

			// start unicast key rekey timer
			if (priv->pmib->dot1180211AuthEntry.dot11UKRekeyTime)
				pstat->uk_timeout = priv->pmib->dot1180211AuthEntry.dot11UKRekeyTime;

			if (!Message_Secure(pStaInfo->EapolKeyMsgRecvd))
				UpdateGK(priv); // send 2-1
			break;

		case PSK_STATE_PTKINITDONE:
			// delete resend timer
			SMP_LOCK_PSK_RESEND(flags);
			if (timer_pending(&pStaInfo->resendTimer)) {
				del_timer(&pStaInfo->resendTimer);
			}
			SMP_UNLOCK_PSK_RESEND(flags);

#if 0
			//receive message [with request bit set]
			if (Message_Request(global->EapolKeyMsgRecvd))
				//supp request to initiate 4-way handshake
			{

			}
#endif

			//------------------------------------------------
			// Supplicant request to init 4 or 2 way handshake
			//------------------------------------------------
			if (Message_Request(pStaInfo->EapolKeyMsgRecvd)) {
				pStaInfo->state = PSK_STATE_PTKSTART;
				if (Message_KeyType(pStaInfo->EapolKeyMsgRecvd) == type_Pairwise) {
					if (Message_Error(pStaInfo->EapolKeyMsgRecvd))
						IntegrityFailure(priv);
					else {
						if (Message_KeyType(pStaInfo->EapolKeyMsgRecvd) == type_Group) {
							if (Message_Error(pStaInfo->EapolKeyMsgRecvd)) {
								//auth change group key, initilate 4-way handshake with supp and execute
								//the Group key handshake to all Supplicants
								pGblInfo->GKeyFailure = TRUE;
								IntegrityFailure(priv);
							}
						}
					}
					//---- Start 4-way handshake ----
					pStaInfo->resendCnt = 0;
					SendEAPOL(priv, pstat, 0);
				}
//#ifdef RTL_WPA2_PREAUTH
//				printf("kenny: %s() in akmsm_PTKINITDONE state. Call lib1x_akmsm_UpdateGK_proc()\n", (char *)__FUNCTION__);
//#endif
				//---- Execute Group Key state machine for each STA ----
				UpdateGK(priv);
			} else {
			}

			break;
		default:
			break;

		}//switch

	} else if (Message_KeyType(pStaInfo->EapolKeyMsgRecvd) == type_Group) {


		//---- Receive 2nd message of 2-way handshake ----
		DEBUG_INFO("2-2\n");
		PSKDEBUG("2-2\n");
		if (!Message_Request(pStaInfo->EapolKeyMsgRecvd)) {//2nd message of 2-way handshake
			//verify that replay counter maches one it has used in the Group Key handshake
			if (!Message_EqualReplayCounter(pStaInfo->LatestGKReplayCounter, pStaInfo->EapolKeyMsgRecvd)) {
				DEBUG_ERR("ERROR_LARGER_REPLAYCOUNTER\n");
				DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_SERIOUS, "ERROR_LARGER_REPLAYCOUNTER\n");
#if defined(CONFIG_WLAN_STATS_EXTENTION)
				priv->ext_stats.total_psk_fail++;
#endif
				return;
			}

			// delete resend timer
			SMP_LOCK_PSK_RESEND(flags);
			if (timer_pending(&pStaInfo->resendTimer)) {
				del_timer(&pStaInfo->resendTimer);
			}
			SMP_UNLOCK_PSK_RESEND(flags);

			if (!CheckMIC(pStaInfo->EAPOLMsgRecvd, pStaInfo->PTK, PTK_LEN_EAPOLMIC)) {
				DEBUG_ERR("ERROR_MIC_FAIL\n");
				DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_SERIOUS, "ERROR_MIC_FAIL\n");

				LOG_MSG("2-way handshake failled! (2-2: MIC error)\n");
#if defined(CONFIG_WLAN_STATS_EXTENTION)
				priv->ext_stats.total_mic_fail++;
				priv->ext_stats.total_psk_fail++;
#endif

#if defined(CONFIG_RTL8196B_TR) || defined(CONFIG_RTL865X_SC) || defined(CONFIG_RTL865X_AC) || defined(CONFIG_RTL865X_KLD) || defined(CONFIG_RTL8196B_KLD) || defined(CONFIG_RTL8196C_KLD) || defined(CONFIG_RTL8196C_EC)
				LOG_MSG_NOTICE("Authentication failed;note:%02x-%02x-%02x-%02x-%02x-%02x;\n",
							   pstat->cmn_info.mac_addr[0],
							   pstat->cmn_info.mac_addr[1],
							   pstat->cmn_info.mac_addr[2],
							   pstat->cmn_info.mac_addr[3],
							   pstat->cmn_info.mac_addr[4],
							   pstat->cmn_info.mac_addr[5]);
#elif defined(CONFIG_RTL8196B_TLD)
				LOG_MSG_DEL("[WLAN access rejected: incorrect security] from MAC address: %02x:%02x:%02x:%02x:%02x:%02x,\n",
							pstat->cmn_info.mac_addr[0],
							pstat->cmn_info.mac_addr[1],
							pstat->cmn_info.mac_addr[2],
							pstat->cmn_info.mac_addr[3],
							pstat->cmn_info.mac_addr[4],
							pstat->cmn_info.mac_addr[5]);
#elif defined(__ECOS) && defined(CONFIG_SDIO_HCI)
				if (priv->sta_status_cb_func)
					priv->sta_status_cb_func("wlan0", pstat->cmn_info.mac_addr, WIFI_STA_STATUS_AUTH_FAIL);
#endif
#ifdef RTK_WLAN_EVENT_INDICATE
				rtk_wlan_event_indicate(priv->dev->name, WIFI_CONNECT_FAIL, pstat->cmn_info.mac_addr, RSN_MIC_failure);
#endif
#ifdef RTK_SMART_ROAMING
#ifdef STA_ROAMING_CHECK
				if(priv->pmib->miscEntry.telco_selected == TELCO_CMCC)
	                notify_connect_fail(priv, pstat->cmn_info.mac_addr, RTL_LINK_CONNECT_FAIL);
#endif
#endif
#ifdef PROC_STA_CONN_FAIL_INFO
				{
					int i;
					for (i=0; i<64; i++) {
						if (!priv->sta_conn_fail[i].used) {
							priv->sta_conn_fail[i].used = TRUE;
							priv->sta_conn_fail[i].error_state = RSN_MIC_failure;
							memcpy(priv->sta_conn_fail[i].addr, pstat->cmn_info.mac_addr, MACADDRLEN);
							break;
						}
						else {
							if (!memcmp(priv->sta_conn_fail[i].addr, pstat->cmn_info.mac_addr, MACADDRLEN)) {
								priv->sta_conn_fail[i].error_state = RSN_MIC_failure;
								break;
							}
						}
					}
				}
#endif
#ifdef STA_ASSOC_STATISTIC
				add_sta_assoc_status(priv, pstat->cmn_info.mac_addr, pstat->rssi, RSN_MIC_failure | 0x300);
#endif
#ifdef ERR_ACCESS_CNTR
				{
					int i = 0, found = 0;
					for (i=0; i<MAX_ERR_ACCESS_CNTR; i++) {
						if (priv->err_ac_list[i].used) {
							if (!memcmp(pstat->cmn_info.mac_addr, priv->err_ac_list[i].mac, MACADDRLEN)) {
								priv->err_ac_list[i].num++;
								found++;
								break;
							}
						}
						else
							break;
					}
					if (!found && (i != MAX_ERR_ACCESS_CNTR)) {
						priv->err_ac_list[i].used = TRUE;
						memcpy(priv->err_ac_list[i].mac, pstat->cmn_info.mac_addr, MACADDRLEN);
						priv->err_ac_list[i].num++;
					}
				}
#endif
				DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_SERIOUS, "FT connection fail, reason=RSN_MIC_failure\n");
				ToDrv_DisconnectSTA(priv, pstat, RSN_MIC_failure);
				pStaInfo->state = PSK_STATE_IDLE;
				return;
			}
		} else //if(!Message_Request(global->EapolKeyMsgRecvd))
			//supp request to change group key
		{
		}

		//------------------------------------------------------
		// Only when the group state machine is in the state of
		// (1) The first STA Connected,
		// (2) UPDATE GK to all station
		// does the GKeyDoneStations needed to be decreased
		//------------------------------------------------------

		if (pGblInfo->GKeyDoneStations > 0)
			pGblInfo->GKeyDoneStations--;

		//Avaya akm_sm->TimeoutCtr = 0;
		//To Do : set port secure to driver
		pStaInfo->gstate = PSK_GSTATE_REKEYESTABLISHED;

		if (pGblInfo->GKeyDoneStations == 0 && !pGblInfo->GkeyReady) {
			ToDrv_SetGTK(priv);
			DEBUG_INFO("2-way Handshake is finished\n");
			PSKDEBUG("2-way Handshake is finished\n");
			pGblInfo->GkeyReady = TRUE;
			pGblInfo->GResetCounter = TRUE;

			// start groupkey rekey timer
			if (priv->pmib->dot1180211AuthEntry.dot11GKRekeyTime)
				mod_timer(&pGblInfo->GKRekeyTimer, jiffies + RTL_SECONDS_TO_JIFFIES(priv->pmib->dot1180211AuthEntry.dot11GKRekeyTime));
		} else {
			DEBUG_INFO(" Receive bad group key handshake");
			PSKDEBUG(" Receive bad group key handshake");
		}
	}
}

#ifdef CLIENT_MODE
static void ClientEAPOLKeyRecvd(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	WPA_GLOBAL_INFO *pGblInfo = priv->wpa_global_info;
	WPA_STA_INFO *pStaInfo = pstat->wpa_sta_info;
	LARGE_INTEGER recievedRC;
	struct lib1x_eapol *eapol;
	int toSetKey = 0, error_code = 0;
	OCTET_STRING tmpANonce;
#ifdef SMP_SYNC
	unsigned long flags = 0;
#endif
	unsigned char keydesctype = Message_KeyDescVer(pStaInfo->EapolKeyMsgRecvd);
	unsigned char *pKeyData=NULL;
	unsigned short keyDataLength=0;
	/*AUTH_SAE_STA*/
	int sae_cache_check=0;
	log("\n===========(RX)EAPOL KEY===========\n");

	eapol = ( struct lib1x_eapol * ) ( pstat->wpa_sta_info->EAPOLMsgRecvd.Octet + ETHER_HDRLEN );
	if (eapol->packet_type != LIB1X_EAPOL_KEY) {
#ifdef DEBUG_PSK
		printk("Not Eapol-key pkt (type %d), drop\n", eapol->packet_type);
#endif
		return;
	}

	pStaInfo->EapolKeyMsgRecvd.Octet = pStaInfo->EAPOLMsgRecvd.Octet + ETHER_HDRLEN + LIB1X_EAPOL_HDRLEN;

#ifdef DEBUG_PSK
	debug_out(NULL, pStaInfo->EAPOLMsgRecvd.Octet, pStaInfo->EAPOLMsgRecvd.Length);
#endif

	if (Message_KeyType(pStaInfo->EapolKeyMsgRecvd) == type_Pairwise) {

		if (Message_KeyMIC(pStaInfo->EapolKeyMsgRecvd) == FALSE) {
			//---- Receive 1st message and send 2nd
			DEBUG_INFO("client mode 4-1\n");
			PSKDEBUG("client mode 4-1\n");

			SMP_LOCK_PSK_RESEND(flags);
			if (timer_pending(&pStaInfo->resendTimer)) {
				del_timer(&pStaInfo->resendTimer);
			}
			SMP_UNLOCK_PSK_RESEND(flags);

			// for Security Vulnerability Detection Tool test --5.2.4, do not disconnect from AP durring the test
			if (Message_EqualKeyNonce(pStaInfo->EapolKeyMsgRecvd, pStaInfo->ANonce)) {
				PSKDEBUG("client mode 4-1: ANonce is equal to preANonce\n");
				return;
			}

			if (pStaInfo->clientHndshkDone && pStaInfo->clientHndshkProcessing == fourWaystep4) {
				DEBUG_INFO("AP trigger pairwise rekey\n");
				PSKDEBUG("AP trigger pairwise rekey\n");

				//reset the client info-------------------------------------------------------
				pStaInfo->CurrentReplayCounter.field.HighPart = 0xffffffff;
				pStaInfo->CurrentReplayCounter.field.LowPart = 0xffffffff;

				pStaInfo->clientHndshkProcessing = FALSE;
				pStaInfo->clientHndshkDone = FALSE;
				pStaInfo->clientHndshkRekey = TRUE;

				pGblInfo->GkeyReady = FALSE;

				//-------------------------------------------------------reset the client info
			}

			//check replay counter
			Message_ReplayCounter_OC2LI(pStaInfo->EapolKeyMsgRecvd, &recievedRC);
			if (!Message_DefaultReplayCounter(pStaInfo->CurrentReplayCounter) &&
					Message_SmallerEqualReplayCounter(pStaInfo->CurrentReplayCounter, pStaInfo->EapolKeyMsgRecvd) ) {
				DEBUG_INFO("client mode 4-1: ERROR_NONEEQUL_REPLAYCOUNTER\n");
                return;
			}

			pStaInfo->CurrentReplayCounter.field.HighPart = recievedRC.field.HighPart;
			pStaInfo->CurrentReplayCounter.field.LowPart = recievedRC.field.LowPart;

			//set wpa_sta_info parameter----------------------------------------------------
            pstat->wpa_sta_info->RSNEnabled = priv->pmib->dot1180211AuthEntry.dot11EnablePSK;

			log("RSNEnabled[%d]",pstat->wpa_sta_info->RSNEnabled);

            if (pstat->wpa_sta_info->RSNEnabled == 1) {
				if (priv->pmib->dot1180211AuthEntry.dot11WPACipher == 2)
					pstat->wpa_sta_info->UnicastCipher = 0x2;
				else if (priv->pmib->dot1180211AuthEntry.dot11WPACipher == 8)
					pstat->wpa_sta_info->UnicastCipher = 0x4;
                else
                {
#ifdef DEBUG_PSK                
                    printk("unicastcipher in wpa = nothing\n");
#endif
                }
			} else if (pstat->wpa_sta_info->RSNEnabled == 2) {
				if (priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher == 2)
					pstat->wpa_sta_info->UnicastCipher = 0x2;
				else if (priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher == 8)
					pstat->wpa_sta_info->UnicastCipher = 0x4;
                else
				{
#ifdef DEBUG_PSK
                    printk("unicastcipher in wpa = nothing\n");
#endif
				}
			}
#ifdef AUTH_SAE_STA
			else if (support_wpa3(priv)) {
				//log("CCMP");
				pstat->wpa_sta_info->UnicastCipher = _CCMP_PRIVACY_;
			}
#endif

	#ifdef CONFIG_IEEE80211W_CLI
			if(keydesctype == key_desc_ver3)
				pStaInfo->AuthKeyMethod = DOT11_AuthKeyType_PSK_SHA256;
	#endif
#ifdef AUTH_SAE
			if (pstat->is_sae_sta && keydesctype == key_desc_ver0) {
				pStaInfo->AuthKeyMethod = AKM_SAE_PSK;
			}
#endif
	#ifdef CONFIG_IEEE80211R_CLI
			if(FT_ENABLE && pStaInfo->isFT) {
				pStaInfo->AuthKeyMethod = DOT11_AuthKeyType_FTPSK;
				pstat->ft_state = state_imd_4way;
			}
	#endif
			log("peer[%pm] keydesctype[%d] akm[%d]",
				pstat->cmn_info.mac_addr, keydesctype, pStaInfo->AuthKeyMethod);
			//----------------------------------------------------set wpa_sta_info parameter

			INCOctet32_INTEGER(&pGblInfo->Counter);
			SetNonce(pStaInfo->SNonce, pGblInfo->Counter);

			//pStaInfo->ANonce = Message_KeyNonce(pStaInfo->EapolKeyMsgRecvd);
			tmpANonce = Message_KeyNonce(pStaInfo->EapolKeyMsgRecvd);
			memcpy(pStaInfo->ANonce.Octet, tmpANonce.Octet, KEY_NONCE_LEN);
			pStaInfo->ANonce.Length = tmpANonce.Length;
	#ifdef CONFIG_IEEE80211R_CLI
		if(FT_ENABLE && pStaInfo->isFT) {
			CalcFTPTK(priv, pstat, pStaInfo->PTK,
				(pStaInfo->AuthKeyMethod >= DOT11_AuthKeyType_FT8021x) ? 48 : PTK_LEN_TKIP);
		}else
	#endif
			CalcPTK(pStaInfo->EAPOLMsgRecvd.Octet, pStaInfo->EAPOLMsgRecvd.Octet + 6,
					pStaInfo->ANonce.Octet, pStaInfo->SNonce.Octet,
					pStaInfo->PMK, PMK_LEN, pStaInfo->PTK,get_ptk_len(priv,pstat)
#ifdef CONFIG_IEEE80211W
					, use_sha256(pStaInfo->AuthKeyMethod)
#endif
				   );

#ifdef DEBUG_PSK
			debug_out("PSK: Generated PTK=", pStaInfo->PTK, PTK_LEN_TKIP);
#endif
			#if 1     // Mark.CS_debug
			log("use_sha256:%d", use_sha256(pStaInfo->AuthKeyMethod));
			dump_hex("Client side PTK", pStaInfo->PTK, 32);
			#endif


#ifdef AUTH_SAE_STA
			/*check if peer's EAP include my pmkid, if no del this pmkid cache*/
			if(support_wpa3(priv) && pstat->is_sae_sta){
				keyDataLength = Message_KeyDataLength(pStaInfo->EapolKeyMsgRecvd);
				pKeyData = pStaInfo->EapolKeyMsgRecvd.Octet + KeyDataPos;
				sae_cache_check = sae_chk_peer_pmkid_cache(priv,pstat,pKeyData,keyDataLength);
			}
#endif

			/* Refine, make 4-2 only be sent 5 times
			to prevent MAYBE some AP keep sending out 4-1 because of incorrect PSK */
			pStaInfo->clientHndshkProcessing = fourWaystep1;
			ResendTimeout((unsigned long)pstat);
			//ClientSendEAPOL(priv, pstat, 0);	// send msg 2
		}
		else {

			/*---- Receive 3rd message ----*/
			DEBUG_INFO("client mode 4-3\n");
			PSKDEBUG("client mode 4-3\n");

			if (!pStaInfo->clientHndshkProcessing) {
				DEBUG_ERR("client mode 4-3: ERROR_MSG_1_ABSENT\n");
				error_code = _STATS_OUT_OF_AUTH_SEQ_;
				goto error_reconn;
			}

			pStaInfo->resendCnt = 0;
			pStaInfo->clientHndshkProcessing=fourWaystep3;

			if (pStaInfo->clientHndshkDone) {
				DEBUG_INFO("Received 4-3 again because maybe 4-4 missed.\n");
				pStaInfo->clientHndshkDone = FALSE;
            }

			// delete resend timer
			SMP_LOCK_PSK_RESEND(flags);
			if (timer_pending(&pStaInfo->resendTimer)) {
				del_timer(&pStaInfo->resendTimer);
			}
			SMP_UNLOCK_PSK_RESEND(flags);

			Message_ReplayCounter_OC2LI(pStaInfo->EapolKeyMsgRecvd, &recievedRC);
			if (!Message_DefaultReplayCounter(pStaInfo->CurrentReplayCounter) &&
					Message_SmallerEqualReplayCounter(pStaInfo->CurrentReplayCounter, pStaInfo->EapolKeyMsgRecvd) ) {
				DEBUG_INFO("client mode 4-3: ERROR_NONEEQUL_REPLAYCOUNTER\n");
				return;
			} else if (!Message_EqualKeyNonce(pStaInfo->EapolKeyMsgRecvd, pStaInfo->ANonce)) {
				DEBUG_ERR("client mode 4-3: ANonce not equal\n");

				// for Security Vulnerability Detection Tool test --5.2.5, do not disconnect from AP durring the test
				priv->is_4Way_finished = pstat->ieee8021x_ctrlport;
				if (priv->is_4Way_finished) {
					pStaInfo->clientHndshkProcessing = fourWaystep4;
					pStaInfo->clientHndshkDone++;
					pGblInfo->GkeyReady = TRUE;
					return;
				} else {
					error_code = _STATS_OUT_OF_AUTH_SEQ_;
					goto error_reconn;
				}
			} else if (!CheckMIC(pStaInfo->EAPOLMsgRecvd, pStaInfo->PTK, PTK_LEN_EAPOLMIC)) {
				DEBUG_ERR("client mode 4-3: PTK MIC ERROR\n");
				LOG_MSG("Authentication failled! (4-3: MIC error)\n");
#if defined(__ECOS) && defined(CONFIG_SDIO_HCI)
				if (priv->link_status_cb_func)
					priv->link_status_cb_func("wlan0", WIFI_LINK_STATUS_AUTH_FAIL);
#endif
				error_code = _RSON_MIC_FAILURE_;
				goto error_reconn;
			}

			pStaInfo->CurrentReplayCounter.field.HighPart = recievedRC.field.HighPart;
			pStaInfo->CurrentReplayCounter.field.LowPart = recievedRC.field.LowPart;

			if ((pStaInfo->RSNEnabled & PSK_WPA2) && (Message_DescType(pStaInfo->EapolKeyMsgRecvd) == desc_type_WPA2)) {
			#ifdef CONFIG_IEEE80211R_CLI
				unsigned char decrypted_data[384];
			#else
				unsigned char decrypted_data[128];
			#endif

				unsigned char GTK_KDE_OUI[] = { 0x00, 0x0F, 0xAC, 0x01 };
				unsigned char WPA_IE_OUI[] = { 0x00, 0x50, 0xF2, 0x01 };
				unsigned char *pGTK_KDE;
			#ifdef CONFIG_IEEE80211R_CLI
				unsigned char *pGTK;
				unsigned char GTK_len;
			#endif
				unsigned char *pKeyData;
				unsigned short keyDataLength;

				keyDataLength = Message_KeyDataLength(pStaInfo->EapolKeyMsgRecvd);
				pKeyData = pStaInfo->EapolKeyMsgRecvd.Octet + KeyDataPos;

#if defined(__OSK__) && defined(CONFIG_WIFI_INCLUDE_WPA_PSK)
				if(!DecWPA2KeyData_driver(pStaInfo, pKeyData, keyDataLength, pStaInfo->PTK + PTK_LEN_EAPOLMIC, PTK_LEN_EAPOLENC, decrypted_data, sizeof(decrypted_data)))
#else
				if(!DecWPA2KeyData(pStaInfo, pKeyData, keyDataLength, pStaInfo->PTK + PTK_LEN_EAPOLMIC, PTK_LEN_EAPOLENC, decrypted_data, sizeof(decrypted_data)))
#endif
				{
					DEBUG_ERR("client mode 4-3: ERROR_AESKEYWRAP_MIC_FAIL\n");
					LOG_MSG("Authentication failled! (4-3: ERROR_AESKEYWRAP_MIC_FAIL)\n");
#if defined(__ECOS) && defined(CONFIG_SDIO_HCI)
					if (priv->link_status_cb_func)
						priv->link_status_cb_func("wlan0", WIFI_LINK_STATUS_AUTH_FAIL);
#endif
					error_code = _RSON_MIC_FAILURE_;
					goto error_reconn;
				}

				//wpa2_hexdump("4-3 KeyData (Decrypted)",decrypted_data,keyDataLength);
				if ( decrypted_data[0] == WPA2_ELEMENT_ID) {
					pGTK_KDE = &decrypted_data[2] + (unsigned char)decrypted_data[1];
					if ( *pGTK_KDE == WPA2_ELEMENT_ID ) {
						// The second optional RSNIE is present
						DEBUG_ERR("client mode 4-3: The second optional RSNIE is present! Cannot handle it yet!\n");
						error_code=__STATS_INVALID_IE_;
						goto error_reconn;
					} else if ( *pGTK_KDE == WPA_ELEMENT_ID ) {
						// if contain RSN IE, skip it
						if (!memcmp((pGTK_KDE + 2), WPA_IE_OUI, sizeof(WPA_IE_OUI)))
							pGTK_KDE += (unsigned char) * (pGTK_KDE + 1) + 2;

						if (!memcmp((pGTK_KDE + 2), GTK_KDE_OUI, sizeof(GTK_KDE_OUI))) {
							// GTK Key Data Encapsulation Format
							unsigned char gtk_len = (unsigned char) * (pGTK_KDE + 1) - 6;
							unsigned char keyID = (unsigned char) * (pGTK_KDE + 6) & 0x03;
							if (gtk_len > GTK_LEN) {
								DEBUG_ERR("client mode 4-3: GTK length (%d) exceeds!\n", gtk_len);
								return;
							}
							if(keyID == pGblInfo->GN && !memcmp(pGblInfo->GTK[keyID], (pGTK_KDE + 8), gtk_len)) {
								PSKDEBUG("4-3: the same keydata, don't reinstall again\n");
								return;
							} else {
								pGblInfo->GN = keyID;
								memcpy(pGblInfo->GTK[keyID], (pGTK_KDE + 8), gtk_len);
								toSetKey = 1;
							}
							pGblInfo->GkeyReady = TRUE;
						#ifdef CONFIG_IEEE80211W_CLI
						if(priv->bss_support_pmf){
							unsigned char IGTK_KDE_TYPE[] = {0xDD, 0x1C, 0x00, 0x0F, 0xAC, 0x09};

							if(!memcmp((pGTK_KDE+8+gtk_len), IGTK_KDE_TYPE, sizeof(IGTK_KDE_TYPE))){
								#define IGTK_HEADER_LEN 8
								PMFDEBUG("IGTK KDE Exists \n");
								memcpy(priv->pmib->dot11IGTKTable.dot11EncryptKey.dot11TTKey.skey,
										(pGTK_KDE+8+gtk_len+sizeof(IGTK_KDE_TYPE)+IGTK_HEADER_LEN), IGTK_LEN);

							#ifdef CONFIG_IEEE80211W_CLI_DEBUG
								dump_hex("IGTK",priv->pmib->dot11IGTKTable.dot11EncryptKey.dot11TTKey.skey,16);
							#endif
							}
							else{
								PMFDEBUG("IGTK KDE Not Exists \n");
							}
						}
						#endif
						}
					}

					//check AP's RSNIE and set Group Key Chiper
					if (decrypted_data[7] == _TKIP_PRIVACY_)
						pGblInfo->MulticastCipher = _TKIP_PRIVACY_ ;
					else if (decrypted_data[7] == _CCMP_PRIVACY_)
						pGblInfo->MulticastCipher = _CCMP_PRIVACY_ ;

			#ifdef CONFIG_IEEE80211R_CLI
					if (FT_ENABLE && pStaInfo->isFT) {
						unsigned int status;
						if (ft_check_imd_4way(priv, pstat, decrypted_data, keyDataLength, &status)) {
							switch (status) {
								case __STATS_INVALID_IE_:
									panic_printk("4-3: RSNIE not present in FT Message 2\n");
									error_code=__STATS_INVALID_IE_;
									goto error_reconn;
									break;
								case _STATS_INVALID_MDIE_:
									panic_printk("4-3: Invalid MDIE in FT Message 2\n");
									error_code=_STATS_INVALID_MDIE_;
									goto error_reconn;
									break;
								case __STATS_INVALID_AKMP_:
									panic_printk("4-3: Invalid AKM Suite\n");
									error_code=__STATS_INVALID_AKMP_;
									goto error_reconn;
									break;
								case _STATS_INVALID_PMKID_:
									panic_printk("4-3: PMKR1-Name not match\n");
									error_code=_STATS_INVALID_PMKID_;
									goto error_reconn;
									break;
								default:
									break;
							}
						}
						pGTK = &decrypted_data[2] + ((unsigned char)decrypted_data[1] + MDID_IE_LEN);
						if(!memcmp(pGTK, GTK_KDE_TYPE, sizeof(GTK_KDE_TYPE))) {
							pGTK += sizeof(GTK_KDE_TYPE);
							unsigned char keyID = (unsigned char) *(pGTK) & 0x03;
							pGblInfo->GN = keyID;
							GTK_len = ((pGblInfo->MulticastCipher == DOT11_ENC_TKIP) ? 32 : 16);
							memcpy(pGblInfo->GTK[keyID], pGTK, GTK_len);
							pGblInfo->GkeyReady = TRUE;
							toSetKey = 1;
						#ifdef DEBUG_PSK
							panic_printk("(%s)line=%d, keyID  = %d\n", __FUNCTION__, __LINE__, keyID);
							debug_out("wpa_global_info->keyID:", priv->wpa_global_info->GTK[keyID], GTK_len);
						#endif
						}
				}
			#endif
				}
			} else if ((pStaInfo->RSNEnabled & PSK_WPA) && (Message_DescType(pStaInfo->EapolKeyMsgRecvd) == desc_type_RSN)) {
				unsigned char WPAkeyData[255];
				unsigned short DataLength;
				memset(WPAkeyData, 0, 255);
				DataLength = Message_KeyDataLength(pStaInfo->EapolKeyMsgRecvd);
				memcpy(WPAkeyData, pStaInfo->EapolKeyMsgRecvd.Octet + KeyDataPos, 255);

				//check AP's RSNIE and set Group Key Chiper
				if (WPAkeyData[11] == _TKIP_PRIVACY_)
					pGblInfo->MulticastCipher = _TKIP_PRIVACY_ ;
				else if (WPAkeyData[11] == _CCMP_PRIVACY_)
					pGblInfo->MulticastCipher = _CCMP_PRIVACY_ ;
			}

#ifdef RTK_WLAN_EVENT_INDICATE
			if (pStaInfo->clientHndshkProcessing && !pStaInfo->clientHndshkDone){
				rtk_wlan_event_indicate(priv->dev->name, WIFI_CONNECT_SUCCESS, pstat->cmn_info.mac_addr, 0);
			}
#endif

			/* Refine, make 4-4 only be sent 5 times
			   to prevent MAYBE some AP keep sending out 4-3 because of incorrect PSK */
			ResendTimeout((unsigned long)pstat);
			//ClientSendEAPOL(priv, pstat, 0);	// send msg 4

			if (toSetKey) {
				ToDrv_SetGTK(priv);
				toSetKey = 0;
			}
			ToDrv_SetPTK(priv, pstat);
			ToDrv_SetPort(priv, pstat, DOT11_PortStatus_Authorized);
#ifdef CONFIG_IEEE80211W_CLI
			if(priv->bss_support_pmf && (priv->pmib->dot1180211AuthEntry.dot11IEEE80211W != NO_MGMT_FRAME_PROTECTION))
				pstat->wpa_sta_info->mgmt_frame_prot = 1;
			else
				pstat->wpa_sta_info->mgmt_frame_prot = 0;

			ToDrv_SetPMF(priv,pstat);
#endif
			//Wifi Logo FFD fragment attack, clear fragment list after 4-way handshake.
			free_sta_frag_list(priv, pstat);
		}
	} else if (Message_KeyType(pStaInfo->EapolKeyMsgRecvd) == type_Group) {
		//---- Receive 1st message of 2-way handshake ----
		DEBUG_INFO("client mode receive 2-1\n");
		PSKDEBUG("client mode receive 2-1\n");

		pStaInfo->resendCnt = 0;

		Message_ReplayCounter_OC2LI(pStaInfo->EapolKeyMsgRecvd, &recievedRC);
		if (Message_SmallerEqualReplayCounter(pStaInfo->CurrentReplayCounter, pStaInfo->EapolKeyMsgRecvd) ) {
			DEBUG_INFO("client mode 2-1: ERROR_NONEEQUL_REPLAYCOUNTER\n");
            return;
		} else if (!CheckMIC(pStaInfo->EAPOLMsgRecvd, pStaInfo->PTK, PTK_LEN_EAPOLMIC)) {
			DEBUG_ERR("client mode 2-1: ERROR_MIC_FAIL\n");
			LOG_MSG("Authentication failled! (4-2: MIC error)\n");
#if defined(__ECOS) && defined(CONFIG_SDIO_HCI)
			if (priv->link_status_cb_func)
				priv->link_status_cb_func("wlan0", WIFI_LINK_STATUS_AUTH_FAIL);
#endif
#if defined(CONFIG_RTL8186_KB_N)|| defined(CONFIG_AUTH_RESULT)
			authRes = 1;//Auth failed
#endif
			error_code = _STATS_OUT_OF_AUTH_SEQ_;
			goto error_reconn;
		}

		if ((pStaInfo->RSNEnabled & PSK_WPA2) && (Message_DescType(pStaInfo->EapolKeyMsgRecvd) == desc_type_WPA2)) {
            unsigned char decrypted_data[128];
            unsigned char KDE_OUI[] = {0x00, 0x0F, 0xAC};
            unsigned short key_data_len = Message_KeyDataLength(pStaInfo->EapolKeyMsgRecvd);
            unsigned char *pKeyData = pStaInfo->EapolKeyMsgRecvd.Octet + KeyDataPos;
            struct KDE *pkde;
            unsigned char *pgtk_kde;

			if (key_data_len > sizeof(decrypted_data)) {
				DEBUG_ERR("client mode 2-1: keyID length (%d) exceeds!\n", key_data_len);
				return;
			}

#if defined(__OSK__) && defined(CONFIG_WIFI_INCLUDE_WPA_PSK)
			if(!DecWPA2KeyData_driver(pStaInfo, pKeyData, key_data_len, pStaInfo->PTK + PTK_LEN_EAPOLMIC, PTK_LEN_EAPOLENC, decrypted_data, sizeof(decrypted_data)))
#else
			if (!DecWPA2KeyData(pStaInfo, pKeyData, key_data_len, pStaInfo->PTK + PTK_LEN_EAPOLMIC, PTK_LEN_EAPOLENC, decrypted_data, sizeof(decrypted_data)))
#endif
			{
    			DEBUG_ERR("client mode 2-1: ERROR_AESKEYWRAP_MIC_FAIL\n");
    			error_code = _RSON_MIC_FAILURE_;
    			goto error_reconn;
    		}

            pkde = (struct KDE *)decrypted_data;
            while (((unsigned char *)pkde - decrypted_data) < key_data_len && pkde->type == WPA_ELEMENT_ID && pkde->length > 0) {
                if (!memcmp(pkde->oui, KDE_OUI, sizeof(KDE_OUI))) {
                    unsigned char gtk_len;
                    pgtk_kde = (unsigned char *)pkde + sizeof(struct KDE);

                    if (pkde->data_type == GTK_KDE) {
                        unsigned char key_id = pgtk_kde[0] & 0x03;
                        if ((pkde->length > 6+GTK_LEN) || (pkde->length < 6)) {
                            DEBUG_ERR("client mode 2-1 GTK_KDE: invalid KDE length (%d)!\n", pkde->length);
                            return;
                        }
                        gtk_len = pkde->length - 6;
                        if(key_id == pGblInfo->GN && !memcmp(pGblInfo->GTK[key_id], (pgtk_kde + 2), gtk_len)) {
					        PSKDEBUG("the same keydata, don't reinstall again\n");
					        pStaInfo->clientGkeyUpdate = 1;
					        ClientSendEAPOL(priv, pstat, 0); // send msg 2-2
					        return;
				        }

                        pGblInfo->GN = key_id;
                        memcpy(pGblInfo->GTK[key_id], (pgtk_kde + 2), gtk_len);
                    }
#ifdef CONFIG_IEEE80211W_CLI
                    else if (pkde->data_type == IGTK_KDE) {
                        unsigned short key_id = le16_to_cpu(*(unsigned short *)pgtk_kde);
                        if ((pkde->length > 12+IGTK_LEN) || (pkde->length < 12)) {
                            DEBUG_ERR("client mode 2-1 IGTK_KDE: invalid KDE length (%d)!\n", pkde->length);
                            return;
                        }
                        gtk_len = pkde->length - 12;
                        pGblInfo->GN_igtk = key_id;
                        memcpy(pGblInfo->IGTK[key_id-4], (pgtk_kde + 8), gtk_len); // should be shift 4??
                        memcpy(priv->pmib->dot11IGTKTable.dot11EncryptKey.dot11TTKey.skey, (pgtk_kde + 8), gtk_len);
                    }
#endif
                    else {
                        DEBUG_INFO("data_type=%d don't process\n", pkde->data_type);
                    }
                }

                /* HH71VM: added backcast to struct KDE *. Arithmetic by
                 * bytes is correct and left as is, but its result is unsigned char *,
                 * and it is assigned to the pointer struct KDE *. */
                pkde = (struct KDE *)((unsigned char *)pkde + (2 + pkde->length));
            }
		} else {
            if (!DecGTK(pStaInfo->EAPOLMsgRecvd, pStaInfo->PTK + PTK_LEN_EAPOLMIC, PTK_LEN_EAPOLENC,
                ((pGblInfo->MulticastCipher == DOT11_ENC_TKIP) ? 32 : 16),
                pGblInfo->GTK[Message_KeyIndex(pStaInfo->EapolKeyMsgRecvd)], GTK_LEN)) {
    			DEBUG_ERR("client mode 2-1: ERROR_AESKEYWRAP_MIC_FAIL\n");
    			error_code = _RSON_MIC_FAILURE_;
    			goto error_reconn;
    		}
			pGblInfo->GN = Message_KeyIndex(pStaInfo->EapolKeyMsgRecvd);
		}
		//MLME_SETKEYS.request() to set Group Key;
		pGblInfo->GkeyReady = TRUE;

		pStaInfo->CurrentReplayCounter.field.HighPart = recievedRC.field.HighPart;
		pStaInfo->CurrentReplayCounter.field.LowPart = recievedRC.field.LowPart;
		pStaInfo->clientGkeyUpdate = 1;

		ToDrv_SetGTK(priv);
		ClientSendEAPOL(priv, pstat, 0); // send msg 2-2
	} else {
		printk("Client EAPOL Key Receive ERROR!!\n");
		error_code = _RSON_UNSPECIFIED_;
	}

error_reconn:
	if (error_code) {
		unsigned int opmode = OPMODE;
		if (ACTIVE_ID == 0) {
			priv->dot114WayStatus = error_code;
		}
		DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_SERIOUS, "issue_deauth - %02X:%02X:%02X:%02X:%02X:%02X\n",
								pstat->cmn_info.mac_addr[0],pstat->cmn_info.mac_addr[1],pstat->cmn_info.mac_addr[2],pstat->cmn_info.mac_addr[3],pstat->cmn_info.mac_addr[4],pstat->cmn_info.mac_addr[5]);
		issue_deauth(priv, BSSID, error_code);
		free_stainfo(priv, pstat);
		opmode &= ~(WIFI_AUTH_SUCCESS | WIFI_ASOC_STATE);
		OPMODE_VAL(opmode);
		JOIN_RES_VAL(STATE_Sta_No_Bss);
#if defined(CONFIG_RTL8672) && !defined(__OSK__)
		wlan_sta_state_notify(priv, STATE_Sta_No_Bss);
#endif
		start_clnt_lookup(priv, RESCAN);
	}
}
#endif // CLIENT_MODE


void derivePSK(struct rtl8192cd_priv *priv)
{
	WPA_GLOBAL_INFO *pGblInfo = priv->wpa_global_info;
	//unsigned long x;
	//SAVE_INT_AND_CLI(x);

#if 1/*backup derivePSK*/
	if(!priv->is_PSK_backup){
		memset(priv->Backup_PSK, 0x0, sizeof(priv->Backup_PSK));
		memset(priv->Backup_PassPhrase, 0x0, sizeof(priv->Backup_PassPhrase));
		memset(priv->Backup_SSID, 0x0, sizeof(priv->Backup_SSID));
		priv->Backup_SSIDLen = 0;
	}else{
		if(!memcmp(SSID, priv->Backup_SSID, sizeof(priv->Backup_SSID)) &&
			SSID_LEN==priv->Backup_SSIDLen &&
			!memcmp(priv->Backup_PassPhrase, priv->pmib->dot1180211AuthEntry.dot11PassPhrase, sizeof(priv->Backup_PassPhrase))){
			//panic_printk(" ### hit PSK backup data!!!\n");
			memcpy(pGblInfo->PSK, priv->Backup_PSK, sizeof(priv->Backup_PSK));
			return;
		}else{
			//panic_printk(" ### update PSK backup data!!!\n");
			priv->is_PSK_backup = 0;
			memset(priv->Backup_PSK, 0x0, sizeof(priv->Backup_PSK));
			memset(priv->Backup_PassPhrase, 0x0, sizeof(priv->Backup_PassPhrase));
			memset(priv->Backup_SSID, 0x0, sizeof(priv->Backup_SSID));
			priv->Backup_SSIDLen = 0;
		}
	}
#endif

	if (strlen((char *)priv->pmib->dot1180211AuthEntry.dot11PassPhrase) == 64){ // hex
		get_array_val(pGblInfo->PSK, (char *)priv->pmib->dot1180211AuthEntry.dot11PassPhrase, 64);
		dump_hex("passphrase",priv->pmib->dot1180211AuthEntry.dot11PassPhrase,64);
		dump_hex("PSK",pGblInfo->PSK,32);
	}else
		PasswordHash((char *)priv->pmib->dot1180211AuthEntry.dot11PassPhrase, SSID, SSID_LEN,
					 pGblInfo->PSK);

	if (priv->pmib->dot1180211AuthEntry.dot11PassPhraseGuest[0]) {
		if (strlen((char *)priv->pmib->dot1180211AuthEntry.dot11PassPhraseGuest) == 64)
			get_array_val(pGblInfo->PSKGuest, (char *)priv->pmib->dot1180211AuthEntry.dot11PassPhraseGuest, 64);
		else
			PasswordHash((char *)priv->pmib->dot1180211AuthEntry.dot11PassPhraseGuest, SSID, SSID_LEN,
						 pGblInfo->PSKGuest);
	}

#if 1/*backup derivePSK*/
	//panic_printk(" ### backup PSK data\n");
	memcpy(priv->Backup_SSID, SSID, sizeof(priv->Backup_SSID));
	priv->Backup_SSIDLen = SSID_LEN;
	memcpy(priv->Backup_PassPhrase, priv->pmib->dot1180211AuthEntry.dot11PassPhrase, sizeof(priv->Backup_PassPhrase));
	memcpy(priv->Backup_PSK, pGblInfo->PSK, sizeof(priv->Backup_PSK));
	priv->is_PSK_backup = 1;
#endif

	//RESTORE_INT(x);
}

void psk_init(struct rtl8192cd_priv *priv)
{
	WPA_GLOBAL_INFO *pGblInfo = priv->wpa_global_info;
	int i, j, low_cipher = 0;
	DEBUG_TRACE;
	/*AUTH_SAE*/
	unsigned int len_of_pgblinfo=sizeof(struct _wpa_global_info);
	unsigned int len_of_pmkcach=0;
	#ifdef AUTH_SAE
	len_of_pmkcach = sizeof(struct pmkid_caching);
	#endif
	//log("	len_of_pgblinfo[%d]  len_of_pmkcach[%d]",len_of_pgblinfo,len_of_pmkcach);

	if(OPMODE&WIFI_AP_STATE){
		memset((char *)pGblInfo, 0, len_of_pgblinfo);
	}else if(OPMODE&WIFI_STATION_STATE){
		memset((char *)pGblInfo + len_of_pmkcach, 0, (len_of_pgblinfo - len_of_pmkcach));
	}

	//---- Counter is initialized whenever boot time ----
	GenNonce(pGblInfo->Counter.charData, (unsigned char*)"addr");

	if (OPMODE & WIFI_AP_STATE) {
		//---- Initialize Goup Key state machine ----
		pGblInfo->GNonce.Octet = pGblInfo->GNonceBuf;
		pGblInfo->GNonce.Length = KEY_NONCE_LEN;
		pGblInfo->GTKAuthenticator = TRUE;
		pGblInfo->GN = 1;
		pGblInfo->GM = 2;
#ifdef CONFIG_IEEE80211W
		pGblInfo->GN_igtk = 4;
		pGblInfo->GM_igtk = 5;
#endif /* CONFIG_IEEE80211W */
		pGblInfo->GInitAKeys = TRUE; // david+2006-04-04, fix the issue of re-generating group key

		init_timer(&pGblInfo->GKRekeyTimer);
		pGblInfo->GKRekeyTimer.data = (unsigned long)priv;
		pGblInfo->GKRekeyTimer.function = GKRekeyTimeout;
	}
#if 0
	if (strlen(priv->pmib->dot1180211AuthEntry.dot11PassPhrase) == 64) // hex
		get_array_val(pGblInfo->PSK, priv->pmib->dot1180211AuthEntry.dot11PassPhrase, 64);
	else
		PasswordHash(priv->pmib->dot1180211AuthEntry.dot11PassPhrase, priv->pmib->dot11StationConfigEntry.dot11DesiredSSID,
					 pGblInfo->PSK);

	if (priv->pmib->dot1180211AuthEntry.dot11PassPhraseGuest[0]) {
		if (strlen(priv->pmib->dot1180211AuthEntry.dot11PassPhraseGuest) == 64)
			get_array_val(pGblInfo->PSKGuest, priv->pmib->dot1180211AuthEntry.dot11PassPhraseGuest, 64);
		else
			PasswordHash(priv->pmib->dot1180211AuthEntry.dot11PassPhraseGuest, priv->pmib->dot11StationConfigEntry.dot11DesiredSSID,
						 pGblInfo->PSKGuest);
	}
#else
	derivePSK(priv);
#endif
#ifdef DEBUG_PSK
	log("Pre-Generated PMK:");
	dump_hex("PMK", pGblInfo->PSK, PMK_LEN);
	if (priv->pmib->dot1180211AuthEntry.dot11PassPhraseGuest[0])
		debug_out("PSK-Guest: PMK=", pGblInfo->PSKGuest, PMK_LEN);
#endif

	if ((priv->pmib->dot1180211AuthEntry.dot11EnablePSK & PSK_WPA) &&
			!priv->pmib->dot1180211AuthEntry.dot11WPACipher) {
		/*AUTH_SAE_STA*/
		priv->pmib->dot1180211AuthEntry.dot11EnablePSK &= ~(PSK_WPA);
		log("psk_enable has wpa;WPAcipher not include;clean WPAcipher\n");
		//DEBUG_ERR("psk_init failed, WPA cipher did not set!\n");
		//return;
	}

#ifdef RTL_WPA2
	if ((priv->pmib->dot1180211AuthEntry.dot11EnablePSK & PSK_WPA2) &&
			!priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher) {
		/*AUTH_SAE_STA*/
		priv->pmib->dot1180211AuthEntry.dot11EnablePSK &= ~(PSK_WPA2);
		log("psk_enable has wpa2;WPA2cipher not be included;clean WPAcipher\n");
		//DEBUG_ERR("psk_init failed, WPA2 cipher did not set!\n");
		//return;

	}
#endif

	if ((priv->pmib->dot1180211AuthEntry.dot11EnablePSK & PSK_WPA2) &&
			!(priv->pmib->dot1180211AuthEntry.dot11EnablePSK & PSK_WPA)) {
		if (priv->pmib->dot1180211AuthEntry.dot11WPACipher)
			priv->pmib->dot1180211AuthEntry.dot11WPACipher = 0;
	}
	if ((priv->pmib->dot1180211AuthEntry.dot11EnablePSK & PSK_WPA) &&
			!(priv->pmib->dot1180211AuthEntry.dot11EnablePSK & PSK_WPA2)) {
		if (priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher)
			priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher = 0;
	}

	if (priv->pmib->dot1180211AuthEntry.dot11WPACipher) {
		for (i = 0, j = 0; i < _WEP_104_PRIVACY_; i++) {
			if (priv->pmib->dot1180211AuthEntry.dot11WPACipher & (1 << i)) {
				pGblInfo->UnicastCipher[j] = i + 1;
				if (low_cipher == 0)
					low_cipher = pGblInfo->UnicastCipher[j];
				else {
					if (low_cipher == _WEP_104_PRIVACY_ &&
							pGblInfo->UnicastCipher[j] == _WEP_40_PRIVACY_)
						low_cipher = pGblInfo->UnicastCipher[j];
					else if (low_cipher == _TKIP_PRIVACY_ &&
							 (pGblInfo->UnicastCipher[j] == _WEP_40_PRIVACY_ ||
							  pGblInfo->UnicastCipher[j] == _WEP_104_PRIVACY_))
						low_cipher = pGblInfo->UnicastCipher[j];
					else if (low_cipher == _CCMP_PRIVACY_)
						low_cipher = pGblInfo->UnicastCipher[j];
				}
				if (++j >= MAX_UNICAST_CIPHER)
					break;
			}
		}
		pGblInfo->NumOfUnicastCipher = j;
	}

#ifdef CLIENT_MODE
	if ((OPMODE & WIFI_ADHOC_STATE) && (priv->pmib->dot1180211AuthEntry.dot11EnablePSK & 2)) // if WPA2
		low_cipher = 0;
#endif

#ifdef RTL_WPA2
	if (priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher) {
		for (i = 0, j = 0; i < _WEP_104_PRIVACY_; i++) {
			if (priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher & (1 << i)) {
				pGblInfo->UnicastCipherWPA2[j] = i + 1;
				if (low_cipher == 0)
					low_cipher = pGblInfo->UnicastCipherWPA2[j];
				else {
					if (low_cipher == _WEP_104_PRIVACY_ &&
							pGblInfo->UnicastCipherWPA2[j] == _WEP_40_PRIVACY_)
						low_cipher = pGblInfo->UnicastCipherWPA2[j];
					else if (low_cipher == _TKIP_PRIVACY_ &&
							 (pGblInfo->UnicastCipherWPA2[j] == _WEP_40_PRIVACY_ ||
							  pGblInfo->UnicastCipherWPA2[j] == _WEP_104_PRIVACY_))
						low_cipher = pGblInfo->UnicastCipherWPA2[j];
					else if (low_cipher == _CCMP_PRIVACY_)
						low_cipher = pGblInfo->UnicastCipherWPA2[j];
				}
				if (++j >= MAX_UNICAST_CIPHER)
					break;
			}
		}
		pGblInfo->NumOfUnicastCipherWPA2 = j;
	}
#endif

	pGblInfo->MulticastCipher = low_cipher;

#ifdef DEBUG_PSK
	printk("PSK: WPA unicast cipher= ");
	for (i = 0; i < pGblInfo->NumOfUnicastCipher; i++)
		printk("%x ", pGblInfo->UnicastCipher[i]);
	printk("\n");

#ifdef RTL_WPA2
	printk("PSK: WPA2 unicast cipher= ");
	for (i = 0; i < pGblInfo->NumOfUnicastCipherWPA2; i++)
		printk("%x ", pGblInfo->UnicastCipherWPA2[i]);
	printk("\n");
#endif

	printk("PSK: multicast cipher= %x\n", pGblInfo->MulticastCipher);
#endif

	pGblInfo->AuthInfoElement.Octet = pGblInfo->AuthInfoBuf;

	ConstructIE(priv, pGblInfo->AuthInfoElement.Octet,
				&pGblInfo->AuthInfoElement.Length);

	ToDrv_SetIE(priv);
}

#ifdef SUPPORT_CLIENT_MIXED_SECURITY
void client_psk_reConstructIE(struct rtl8192cd_priv *priv)
{
	WPA_GLOBAL_INFO *pGblInfo=priv->wpa_global_info;

	ConstructIE(priv, pGblInfo->AuthInfoElement.Octet,
					 &pGblInfo->AuthInfoElement.Length);

	ToDrv_SetIE(priv);
}
#endif

#if defined(WDS) && defined(INCLUDE_WPA_PSK)
void wds_psk_set(struct rtl8192cd_priv *priv, int idx, unsigned char *key)
{
	unsigned char pchar[40];

	if (key == NULL) {
		if (strlen((char *)priv->pmib->dot11WdsInfo.wdsPskPassPhrase) == 64) // hex
			get_array_val(priv->pmib->dot11WdsInfo.wdsMapingKey[idx], (char *)priv->pmib->dot11WdsInfo.wdsPskPassPhrase, 64);
		else {
			memset(pchar, 0, sizeof(unsigned char) * 40);
			PasswordHash((char *)priv->pmib->dot11WdsInfo.wdsPskPassPhrase, (unsigned char *)"REALTEK", strlen("REALTEK"), pchar);
			memcpy(priv->pmib->dot11WdsInfo.wdsMapingKey[idx], pchar, sizeof(unsigned char) * 32);
		}
	} else
		memcpy(priv->pmib->dot11WdsInfo.wdsMapingKey[idx], key, sizeof(unsigned char) * 32);

	priv->pmib->dot11WdsInfo.wdsMappingKeyLen[idx] = 32;
	priv->pmib->dot11WdsInfo.wdsMappingKeyLen[idx] |= 0x80000000;  //set bit to protect the key
}


void wds_psk_init(struct rtl8192cd_priv *priv)
{
	unsigned char *key;
	int i;

	if ( !(OPMODE & WIFI_AP_STATE))
		return;

	for (i = 0; i < priv->pmib->dot11WdsInfo.wdsNum; i++) {
		if (i == 0)
			key = NULL;
		else
			key = priv->pmib->dot11WdsInfo.wdsMapingKey[0];

		wds_psk_set(priv, i, key);
	}
}
#endif

#ifdef CONFIG_IEEE80211W

void ToDrv_SA_QUERY_RSP(struct rtl8192cd_priv *priv, const u8 *sa, const u8 *trans_id)
{
	struct iw_point wrq;
	DOT11_SA_QUERY_RSP resp;


	wrq.pointer = &resp;
	wrq.length = sizeof(resp);

	resp.EventId = DOT11_EVENT_SA_QUERY_RSP;
	resp.IsMoreEvent = FALSE;
	memcpy(resp.MACAddr, sa, 6);
	memcpy(resp.trans_id, trans_id, 2);

	rtl8192cd_ioctl_priv_req(priv->dev, &wrq);
}

void ieee802_11_sa_query_action(struct rtl8192cd_priv *priv, unsigned char *sa, unsigned char *pframe, int len)
{
	unsigned char Category_field = pframe[0];
	unsigned char Action_field = pframe[1];
	unsigned char trans_id[2];
	struct stat_info *pstat;

	memcpy(trans_id, pframe + 2, 2);

	if (Action_field == _SA_QUERY_REQ_ACTION_ID_) {
#ifdef DEBUG_PSK
		printk("IEEE 802.11: Received SA Query Request from %02x:%02x:%02x:%02x:%02x:%02x\n"
			   , sa[0], sa[1], sa[2], sa[3], sa[4], sa[5]);
		printk("IEEE 802.11: SA Query Transaction ID %d\n", le16_to_cpup((u16 *)(pframe + 2)));
#endif

		pstat = get_stainfo(priv, sa);
		if (pstat == NULL || pstat->wpa_sta_info->state != PSK_STATE_PTKINITDONE) {
#ifdef DEBUG_PSK
			printk("IEEE 802.11: Ignore SA Query Request from unassociated STA %02x:%02x:%02x:%02x:%02x:%02x\n"
				   , sa[0], sa[1], sa[2], sa[3], sa[4], sa[5]);
#endif
			return;
		}

#ifdef DEBUG_PSK
		printk("IEEE 802.11: Sending SA Query Response to %02x:%02x:%02x:%02x:%02x:%02x\n"
			   , sa[0], sa[1], sa[2], sa[3], sa[4], sa[5]);
#endif
		ToDrv_SA_QUERY_RSP(priv, sa, trans_id);
		return;
	}

	if (Action_field != _SA_QUERY_RSP_ACTION_ID_) {
#ifdef DEBUG_PSK
		printk("IEEE 802.11: Unexpected SA Query Action %d\n", Action_field);
#endif
		return;
	}


}

#endif  // CONFIG_IEEE80211W

int psk_indicate_evt(struct rtl8192cd_priv *priv, int id, unsigned char *mac, unsigned char *msg, int len)
{
	struct stat_info *pstat;
	static unsigned char tmpbuf[1024];
	int ret;
#ifdef RTL_WPA2
	int isWPA2 = 0;
#endif

#ifdef CONFIG_IEEE80211R
	if (id == DOT11_EVENT_FT_ASSOC_IND && !priv->pmib->dot1180211AuthEntry.dot11EnablePSK) {
		// pass the check
	} else
#endif
	if (!priv->pmib->dot1180211AuthEntry.dot11EnablePSK ||
			!((priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _TKIP_PRIVACY_) ||
			  (priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm == _CCMP_PRIVACY_)))
		return -1;

#ifdef DEBUG_PSK
	printk("PSK: Got evt:%s[%x], peer:[%pm], len=0x[%x]\n",ID2STR(id), id,mac, len);
#endif

	pstat = get_stainfo(priv, mac);
// button 2009.05.21
#if 0
	if (pstat == NULL)
#else
	if (pstat == NULL && id != DOT11_EVENT_WPA_MULTICAST_CIPHER && id != DOT11_EVENT_WPA2_MULTICAST_CIPHER)
#endif
	{
		DEBUG_INFO("Invalid mac address: %02x:%02x:%02x:%02x:%02x:%02x\n",
				  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		return -1;
	}

	switch (id) {
	case DOT11_EVENT_ASSOCIATION_IND:
	case DOT11_EVENT_REASSOCIATION_IND:
		reset_sta_info(priv, pstat);

		if (OPMODE & WIFI_AP_STATE) {
			// check RSNIE
			if (len > 2 && msg != NULL) {
#ifdef DEBUG_PSK
				debug_out("PSK: Rx Assoc-ind, RSNIE", msg, len);
#endif

#ifdef RTL_WPA2
				if (len <= sizeof(tmpbuf))
					memcpy(tmpbuf, msg, len);
				len -= 2;
#else
				tmpbuf[0] = RSN_ELEMENT_ID;
				tmpbuf[1] = len;
				if (len <= (sizeof(tmpbuf) - 2))
					memcpy(tmpbuf + 2, msg, len);
#endif

#ifdef RTL_WPA2
				isWPA2 = (tmpbuf[0] == WPA2_ELEMENT_ID) ? 1 : 0;
				if (isWPA2){
					log("peer's ie => wpa2_ie_check");
					wpa2_ie_check(priv,tmpbuf,len+2);
					ret = parseIEWPA2(priv, pstat, pstat->wpa_sta_info, tmpbuf, len + 2);
				}else
#endif
				{
					ret = parseIE(priv, pstat->wpa_sta_info, tmpbuf, len + 2);
				}

				if (ret != 0) {
					DEBUG_ERR("chk RSN IE err[%d]\n", ret);
				}
				else {
					//log("chk RSN IE");
					if (isWPA2){
						if(pstat->wpa_sta_info->UnicastCipher == 0){
							pstat->wpa_sta_info->UnicastCipher = _CCMP_PRIVACY_;
							log("Assign UnicastCipher");
						}

						if(pstat->wpa_sta_info->AuthKeyMethod == 0){
							pstat->wpa_sta_info->AuthKeyMethod = DOT11_AuthKeyType_PSK;
							log("Assign AuthKeyMethod");
						}

					}
				}

				// issue assoc-rsp successfully
				ToDrv_RspAssoc(priv, id, mac, -ret);

				if (ret == 0) {
#ifdef EVENT_LOG
					char *pmsg;
					switch (pstat->wpa_sta_info->UnicastCipher) {
					case DOT11_ENC_NONE:
						pmsg = "none";
						break;
					case DOT11_ENC_WEP40:
						pmsg = "WEP40";
						break;
					case DOT11_ENC_TKIP:
						pmsg = "TKIP";
						break;
					case DOT11_ENC_WRAP:
						pmsg = "AES";
						break;
					case DOT11_ENC_CCMP:
						pmsg = "AES";
						break;
					case DOT11_ENC_WEP104:
						pmsg = "WEP104";
						break;
					default:
						pmsg = "invalid algorithm";
						break;
					}
#ifdef RTL_WPA2
					LOG_MSG("%s-%s PSK authentication in progress...\n", (isWPA2 ? "WPA2" : "WPA"), pmsg);
#else
					LOG_MSG("%s-WPA PSK authentication in progress...\n",  pmsg);
#endif
#endif

#if defined(CONFIG_RTL8196B_TR) || defined(CONFIG_RTL865X_SC) || defined(CONFIG_RTL865X_AC) || defined(CONFIG_RTL865X_KLD) || defined(CONFIG_RTL8196B_KLD) || defined(CONFIG_RTL8196C_KLD) || defined(CONFIG_RTL8196C_EC)
					LOG_MSG_NOTICE("Authenticating......;note:%02x-%02x-%02x-%02x-%02x-%02x;\n",
								   pstat->cmn_info.mac_addr[0],
								   pstat->cmn_info.mac_addr[1],
								   pstat->cmn_info.mac_addr[2],
								   pstat->cmn_info.mac_addr[3],
								   pstat->cmn_info.mac_addr[4],
								   pstat->cmn_info.mac_addr[5]);
#endif

					AuthenticationRequest(priv, pstat); // send 4-1
				}
			} else { // RNSIE is null
				if (priv->pmib->dot1180211AuthEntry.dot11EnablePSK)
					ToDrv_RspAssoc(priv, id, mac, -ERROR_INVALID_RSNIE);
			}
		}
#ifdef  CLIENT_MODE
		else
			mod_timer(&pstat->wpa_sta_info->resendTimer, jiffies + WAIT_EAP_TIME);
#endif
		break;

	case DOT11_EVENT_DISASSOCIATION_IND:
		reset_sta_info(priv, pstat);
		break;

#ifdef CONFIG_IEEE80211W
	case DOT11_EVENT_SA_QUERY:
		ieee802_11_sa_query_action(priv, mac, msg, len);
		break;
#endif

#ifdef CONFIG_IEEE80211R
	case DOT11_EVENT_FT_ASSOC_IND:
		if (OPMODE & WIFI_AP_STATE) {
			if (len > 2 && msg != NULL) {
				memcpy(tmpbuf, msg, len);
				len -= 2;
				isWPA2 = (tmpbuf[0] == WPA2_ELEMENT_ID) ? 1 : 0;
				if (isWPA2)
					ret = parseIEWPA2(priv, pstat, pstat->wpa_sta_info, tmpbuf, len + 2); /*AUTH_SAE*/
				if (ret != 0) {
#if defined(CONFIG_WLAN_STATS_EXTENTION)
					priv->ext_stats.total_psk_fail++;
#endif
					DEBUG_ERR("parse IE error [%x]!\n", ret);
				}
				else {
					#ifdef RTK_SMART_ROAMING
					if(SMART_ROAMING_ENABLE
#ifdef STA_ROAMING_CHECK
						|| RTL_LINK_ROAMING_ENABLE
#endif
						){
					if(pstat && pstat->is_realtek_sta) //pstat->IOTPeer == HT_IOT_PEER_RTK_APCLIENT)
					{
#ifdef DEBUG_PSK
						panic_printk("rtk board connect\n");
#endif
					}
					else
							notify_new_sta(priv, pstat->cmn_info.mac_addr, ASSOC_NEW_STA, pstat->rssi);
					}
					#endif
					#ifdef RTK_MULTI_AP
					if(pstat && pstat->IOTPeer == HT_IOT_PEER_RTK_APCLIENT)
					{
#ifdef DEBUG_PSK					
						panic_printk("repeater sta connect\n");
#endif
					}
					else
						client_join_notify(BSSID, pstat->cmn_info.mac_addr);
					#endif
				}

				if (!priv->pmib->dot1180211AuthEntry.dot11EnablePSK)
					priv->wpa_global_info->MulticastCipher = pstat->wpa_sta_info->MulticastCipher_1x;
				install_ft_keys(priv, pstat);

				ToDrv_RspAssoc(priv, id, mac, -ret);
			} else { // RNSIE is null
				if (priv->pmib->dot1180211AuthEntry.dot11EnablePSK)
					ToDrv_RspAssoc(priv, id, mac, -ERROR_INVALID_RSNIE);
			}
		}
		break;
#endif

	case DOT11_EVENT_EAP_PACKET:
		if (OPMODE & WIFI_AP_STATE) {
			if (pstat->wpa_sta_info->state == PSK_STATE_IDLE) {
				DEBUG_ERR("Rx EAPOL packet but did not get Assoc-Ind yet!\n");
				break;
			}
		}

		if (len > MAX_EAPOLMSG_LEN) {
			DEBUG_ERR("Rx EAPOL packet which length is too long [%x]!\n", len);
#if defined(CONFIG_WLAN_STATS_EXTENTION)
			priv->ext_stats.total_psk_fail++;
#endif
			break;
		}

#ifdef CLIENT_MODE
		if ((OPMODE & WIFI_STATION_STATE) &&
				!(pstat->wpa_sta_info->clientHndshkProcessing || pstat->wpa_sta_info->clientHndshkDone)) {
#ifdef EVENT_LOG
			char *pmsg;
			switch (pstat->wpa_sta_info->UnicastCipher) {
			case DOT11_ENC_NONE:
				pmsg = "none";
				break;
			case DOT11_ENC_WEP40:
				pmsg = "WEP40";
				break;
			case DOT11_ENC_TKIP:
				pmsg = "TKIP";
				break;
			case DOT11_ENC_WRAP:
				pmsg = "AES";
				break;
			case DOT11_ENC_CCMP:
				pmsg = "AES";
				break;
			case DOT11_ENC_WEP104:
				pmsg = "WEP104";
				break;
			default:
				pmsg = "invalid algorithm";
				break;
			}
			LOG_MSG("%s-%s PSK authentication in progress...\n", (isWPA2 ? "WPA2" : "WPA"), pmsg);
#endif
			reset_sta_info(priv, pstat);
		}else if(OPMODE & WIFI_ADHOC_STATE){
			DEBUG_ERR("Rx EAPOL packet but not support 4 way handshark yet on Ad-hoc mode, block it!\n");
			break;
		}
#endif
		memcpy(pstat->wpa_sta_info->EAPOLMsgRecvd.Octet, msg, len);
		pstat->wpa_sta_info->EAPOLMsgRecvd.Length = len;
		if (OPMODE & WIFI_AP_STATE)
			EAPOLKeyRecvd(priv, pstat);
#ifdef CLIENT_MODE
		else if (OPMODE & WIFI_STATION_STATE)
			ClientEAPOLKeyRecvd(priv, pstat);
#endif
		break;

	case DOT11_EVENT_MIC_FAILURE:
#ifdef CLIENT_MODE
		if (OPMODE & WIFI_STATION_STATE)
			ClientSendEAPOL(priv, pstat, 0);
#endif
		break;

// button 2009.05.21
	case DOT11_EVENT_WPA_MULTICAST_CIPHER:
	case DOT11_EVENT_WPA2_MULTICAST_CIPHER:
#ifdef CLIENT_MODE
		if (OPMODE & WIFI_STATION_STATE) {
			priv->wpa_global_info->MulticastCipher = *msg;
			log("=> ConstructIE");
			ConstructIE(priv, priv->wpa_global_info->AuthInfoElement.Octet,
						&priv->wpa_global_info->AuthInfoElement.Length);
			if (priv->wpa_global_info->AuthInfoElement.Length > sizeof(priv->pmib->dot11RsnIE.rsnie)) {
				DEBUG_ERR("[%s] exceed in size\n", __func__);
				break;
			}
			memcpy((void *)priv->pmib->dot11RsnIE.rsnie, priv->wpa_global_info->AuthInfoElement.Octet
				   , priv->wpa_global_info->AuthInfoElement.Length);
			DEBUG_WARN("####### MulticastCipher=%d\n", priv->wpa_global_info->MulticastCipher);
			log("####### MulticastCipher=%d\n", priv->wpa_global_info->MulticastCipher);
		}
#endif
		break;
	}

	return 0;
}

void UKRekeyTimeout(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	AuthenticationRequest(priv, pstat);
}



#ifdef CONFIG_RTK_MESH
void dot11s_mp_set_key(DRV_PRIV *priv, unsigned char *mac)
{
	DOT11_SET_KEY Set_Key;
	struct Dot11EncryptKey	*pEncryptKey = &(priv->pmib->dot11sKeysTable.dot11EncryptKey);
	unsigned char key[40];

	if (pEncryptKey->dot11TTKeyLen == 0) {
		if (strlen((char *)priv->pmib->dot1180211sInfo.dot11PassPhrase) == 64) {// hex
			get_array_val(key, (char *)priv->pmib->dot1180211sInfo.dot11PassPhrase, 64);
		} else {
			PasswordHash(priv->pmib->dot1180211sInfo.dot11PassPhrase,
						 priv->pmib->dot1180211sInfo.mesh_id,
						 strlen(priv->pmib->dot1180211sInfo.mesh_id),
						 key);
		}
		pEncryptKey->dot11TTKeyLen = 16;
		memcpy(pEncryptKey->dot11TTKey.skey, key, pEncryptKey->dot11TTKeyLen);
	}

	memset(key, 0, sizeof(key));
	memcpy(key, pEncryptKey->dot11TTKey.skey, pEncryptKey->dot11TTKeyLen);
	memcpy(Set_Key.MACAddr, mac, MACADDRLEN);
	Set_Key.KeyType = DOT11_KeyType_Pairwise;
	Set_Key.EncType = DOT11_ENC_CCMP;
	Set_Key.KeyIndex = priv->pmib->dot11sKeysTable.keyid;
	DOT11_Process_Set_Key(priv->dev, NULL, &Set_Key, key);
}

#endif



#ifdef SUPPORT_CLIENT_MIXED_SECURITY
void choose_cipher(struct rtl8192cd_priv *priv, struct bss_desc *bss_target)
{
	unsigned int t_stamp=0,t_stamp1=0;
	int len;
	int dot11EnablePSK = 0;
	int dot11WPACipher = 0;
	int dot11WPA2Cipher = 0;
	unsigned char dot11PrivacyAlgrthm = _NO_PRIVACY_;

	t_stamp = bss_target->t_stamp[0];
	t_stamp1 = bss_target->t_stamp[1];

	/*Privacy bit*/
	if ( bss_target->capability & BIT4 ) {

#ifdef AUTH_SAE_STA
		/*fetch WPA3*/
		if (support_wpa3(priv) && peer_support_wpa3(priv,t_stamp1)) {
			log("DUT support_wpa3 && peer support WPA3");
			dot11EnablePSK = (PSK_WPA2|PSK_WPA3);
			dot11WPACipher = _WPAWPA2_CIPHER_NONE_;
			dot11WPA2Cipher = _WPAWPA2_CIPHER_CCMP_;
		}
		else
#endif
		{
			/*fetch WPA2*/
			if ( (t_stamp & BIT18) || (t_stamp & BIT20) ) {
				dot11EnablePSK |= PSK_WPA2;
				//  pairwise cipher
				if ( t_stamp & BIT24 )
					dot11WPA2Cipher |= BIT1;
				if ( t_stamp & BIT26 )
					dot11WPA2Cipher |= BIT3;
			}
			/*fetch WPA*/
			if ( (t_stamp & BIT2) || (t_stamp & BIT4) ) {
				dot11EnablePSK |= PSK_WPA;
				//  unicast cipher
				if ( t_stamp & BIT8 )
					dot11WPACipher |= BIT1;
				if ( t_stamp & BIT10 )
					dot11WPACipher |= BIT3;
			}
		}
	}

	/*AUTH_SAE_STA debug*/
	dump_security_mib(priv);

	if (priv->pshare->rf_ft_var.auto_cipher) {
		//  privacy enable and not exist WPA & WPA2 ie -> WEP encryption
		if ( dot11EnablePSK == 0 ) {
			len = strlen(priv->pmib->dot11DefaultKeysTable.keytype[0].skey);
			if ( len == 5 )
				dot11PrivacyAlgrthm = _WEP_40_PRIVACY_;
			else if (len == 13 )
				dot11PrivacyAlgrthm = _WEP_104_PRIVACY_;
		} else { // WPA or WPA2 encryption
			// Select the most higher security (AES > WPA2 > OTHER)
#ifdef AUTH_SAE_STA
			if (support_wpa3(priv) && peer_support_wpa3(priv,t_stamp1)) {
				/*STA mode under WPA3 */
				log("WPA3-AES");
				dot11EnablePSK = (PSK_WPA2|PSK_WPA3);
				dot11WPACipher = _WPAWPA2_CIPHER_NONE_;
				dot11WPA2Cipher = _WPAWPA2_CIPHER_CCMP_;
				dot11PrivacyAlgrthm = _CCMP_PRIVACY_;
			}else
#endif
			if ( (dot11WPACipher & BIT3) ||
					(dot11WPA2Cipher & BIT3) ) {
				if (dot11WPA2Cipher & BIT3 ) {
					dot11EnablePSK = PSK_WPA2;
					dot11WPACipher = 0;
					dot11WPA2Cipher = BIT3;

				} else {
					dot11EnablePSK = PSK_WPA;
					dot11WPACipher = BIT3;
					dot11WPA2Cipher = 0;
				}

				dot11PrivacyAlgrthm = _CCMP_PRIVACY_;
			} else {
				if ( dot11EnablePSK & PSK_WPA2 ) {
					dot11EnablePSK = PSK_WPA2;
					dot11WPACipher = 0;
					dot11WPA2Cipher = BIT1;
				} else {
					dot11EnablePSK = PSK_WPA;
					dot11WPACipher = BIT1;
					dot11WPA2Cipher = 0;
				}

				dot11PrivacyAlgrthm = _TKIP_PRIVACY_;
			}
		}
	}
	else {

#ifdef AUTH_SAE_STA
		if (support_wpa3(priv) && peer_support_wpa3(priv,t_stamp1)) {
			/*STA mode under WPA3 */
			log("DUT support WPA3,Peer support WPA3");
			dot11EnablePSK = (PSK_WPA2|PSK_WPA3);
			dot11WPACipher = _WPAWPA2_CIPHER_NONE_;
			dot11WPA2Cipher = _WPAWPA2_CIPHER_CCMP_;
			dot11PrivacyAlgrthm = _CCMP_PRIVACY_;
		} else
#endif
		if ((priv->pmib->dot1180211AuthEntry.dot11EnablePSK & dot11EnablePSK)
		&& ((priv->pmib->dot1180211AuthEntry.dot11WPACipher & dot11WPACipher)
		|| (priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher & dot11WPA2Cipher))
		) {

			if ((priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher & dot11WPA2Cipher) & BIT3) {
				DEBUG_INFO("\nWPA2-AES\n");
				dot11EnablePSK = PSK_WPA2;
				dot11WPACipher = 0;
				dot11WPA2Cipher = BIT3;
				dot11PrivacyAlgrthm = _CCMP_PRIVACY_;
			} else if ((priv->pmib->dot1180211AuthEntry.dot11WPACipher & dot11WPACipher) & BIT3) {
				DEBUG_INFO("\nWPA-AES\n");
				dot11EnablePSK = PSK_WPA;
				dot11WPACipher = BIT3;
				dot11WPA2Cipher = 0;
				dot11PrivacyAlgrthm = _CCMP_PRIVACY_;
			} else if ((priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher & dot11WPA2Cipher) & BIT1) {
				DEBUG_INFO("\nWPA2-TKIP\n");
				dot11EnablePSK = PSK_WPA2;
				dot11WPACipher = 0;
				dot11WPA2Cipher = BIT1;
				dot11PrivacyAlgrthm = _TKIP_PRIVACY_;
			} else if ((priv->pmib->dot1180211AuthEntry.dot11WPACipher & dot11WPACipher) & BIT1) {
				DEBUG_INFO("\nWPA-TKIP\n");
				dot11EnablePSK = PSK_WPA;
				dot11WPACipher = BIT1;
				dot11WPA2Cipher = 0;
				dot11PrivacyAlgrthm = _TKIP_PRIVACY_;
			} else {
				DEBUG_INFO("No match wpa cipher!!!\n");
				log("No match wpa cipher!");
				return;
			}
		} else {
			DEBUG_INFO("Not wpa enable!!!\n");
			log("Not wpa enable!");
			return;
		}
	}

	priv->pmib->dot1180211AuthEntry.dot11EnablePSK = dot11EnablePSK;
	priv->pmib->dot1180211AuthEntry.dot11WPACipher = dot11WPACipher;
	priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher = dot11WPA2Cipher;
	priv->pmib->dot1180211AuthEntry.dot11PrivacyAlgrthm = dot11PrivacyAlgrthm;

	//dump_security_mib(priv);

	if (priv->pmib->dot1180211AuthEntry.dot11EnablePSK) {
		log("=>psk_init");
		psk_init(priv);
		priv->pmib->dot118021xAuthEntry.dot118021xAlgrthm = 1;
	} else {
		priv->pmib->dot11RsnIE.rsnielen = 0;
		priv->pmib->dot118021xAuthEntry.dot118021xAlgrthm = 0;
	}

	if (should_forbid_Nmode(priv)) {
		if (priv->pmib->dot11BssType.net_work_type & WIRELESS_11N) {
#ifdef SUPPORT_MULTI_PROFILE
			if (!priv->mask_n_band)
				priv->mask_n_band = (priv->pmib->dot11BssType.net_work_type & (WIRELESS_11N | WIRELESS_11AC));
#endif
			priv->pmib->dot11BssType.net_work_type &= ~(WIRELESS_11N | WIRELESS_11AC);
		}
	}
#ifdef SUPPORT_MULTI_PROFILE
	else {
		if (priv->mask_n_band) {
			priv->pmib->dot11BssType.net_work_type |= priv->mask_n_band;
			priv->mask_n_band = 0;
		}
	}
#endif
}
#endif

#if defined(CONFIG_IEEE80211R) || defined(CONFIG_IEEE80211R_CLI)
int derive_ft_pmk_r1(
	struct r0_key_holder *r0kh, unsigned char *s1kh_id,
	unsigned char *r1kh_id, unsigned char *pmk,
	unsigned char *pmkid_in)
{
	unsigned char data[512];
	unsigned char *pos;
	unsigned char *data_vec[4];
	size_t len_vec[4];
	unsigned char tmpBuf[128];

	pos = data;
	memcpy(pos, r1kh_id, MACADDRLEN);
	pos += MACADDRLEN;
	memcpy(pos, s1kh_id, MACADDRLEN);
	pos += MACADDRLEN;
	sha256_prf(r0kh->pmk_r0, PMK_LEN, (unsigned char *)FT_PMKR1_CONST,
		data, pos - data, pmk, PMK_LEN);

	data_vec[0] = FT_PMKR1_NAME_CONST;
	len_vec[0] = FT_PMKR1_NAME_CONST_SIZE;
	data_vec[1] = r0kh->pmk_r0_name;
	len_vec[1] = PMKID_LEN;
	data_vec[2] = r1kh_id;
	len_vec[2] = MACADDRLEN;
	data_vec[3] = s1kh_id;
	len_vec[3] = MACADDRLEN;
	if (sha256_vector(4, data_vec, len_vec, tmpBuf)) {
		printk("Error: sha256 fail\n");
		return -1;
	}

	memcpy(pmkid_in, tmpBuf, PMKID_LEN);
	return 0;
}

void derive_ft_ptk(struct r1_key_holder *r1kh, unsigned char *snonce, unsigned char *anonce, unsigned char *keyout, unsigned int keyoutlen)
{
	unsigned char data[512];
	unsigned char *pos;

	pos = data;
	memcpy(pos, snonce, KEY_NONCE_LEN);
	pos += KEY_NONCE_LEN;
	memcpy(pos, anonce, KEY_NONCE_LEN);
	pos += KEY_NONCE_LEN;
	memcpy(pos, r1kh->r1kh_id, MACADDRLEN);
	pos += MACADDRLEN;
	memcpy(pos, r1kh->sta_addr, MACADDRLEN);
	pos += MACADDRLEN;
	sha256_prf(r1kh->pmk_r1, PMK_LEN, (unsigned char *)FT_PMK_EXPANSION_CONST,
		data, pos - data, keyout, keyoutlen);
}

void derive_ft_pmk_r1_id(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *pmk_r0_id, unsigned char *pmk_r1_id)
{
	unsigned char *data_vec[4];
	size_t len_vec[4];
	unsigned char tmpBuf[128];

	data_vec[0] = FT_PMKR1_NAME_CONST;
	len_vec[0] = FT_PMKR1_NAME_CONST_SIZE;
	data_vec[1] = pmk_r0_id;
	len_vec[1] = PMKID_LEN;
	data_vec[2] = BSSID;
	len_vec[2] = MACADDRLEN;
	data_vec[3] = pstat->cmn_info.mac_addr;
	len_vec[3] = MACADDRLEN;
	if (sha256_vector(4, data_vec, len_vec, tmpBuf)) {
		DEBUG_ERR("Error: sha256 fail\n");
		return;
	}
	memcpy(pmk_r1_id, tmpBuf, PMKID_LEN);
}

static void CalcFTPTK(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *keyout, int keyoutlen)
{
	WPA_STA_INFO *pStaInfo = pstat->wpa_sta_info;
	unsigned char data[512];
	unsigned char *pos;
	unsigned char tmpBuf[128];
	unsigned char *data_vec[4];
	size_t len_vec[4];
	unsigned char pmk_r0[PMK_LEN], pmk_r1[PMK_LEN];
	unsigned char pmk_r0_name[PMKID_LEN], pmk_r1_name[PMKID_LEN];
	unsigned char salt_buff[PMKID_LEN];
	unsigned long flags;

	// Calc PMK-R0
	pos = data;
	*pos++ = (unsigned char)SSID_LEN;
	memcpy(pos, SSID, SSID_LEN);
	pos += SSID_LEN;
	memcpy(pos, MDID, 2);
	pos += 2;
	*pos++ = R0KH_ID_LEN;
	memcpy(pos, R0KH_ID, R0KH_ID_LEN);
	pos += R0KH_ID_LEN;

#ifdef CONFIG_IEEE80211R_CLI
	if(OPMODE & WIFI_STATION_STATE)
		memcpy(pos, GET_MY_HWADDR, MACADDRLEN);
	else
#endif
		memcpy(pos, pstat->cmn_info.mac_addr, MACADDRLEN);

	pos += MACADDRLEN;
	sha256_prf(pStaInfo->PMK, PMK_LEN, (unsigned char*)FT_PMKR0_CONST, data, pos - data, tmpBuf, 48);
	memcpy(pmk_r0, tmpBuf, PMK_LEN);
	memcpy(salt_buff, tmpBuf + PMK_LEN, PMKID_LEN);

	// Calc PMK-R0Name
	data_vec[0] = FT_PMKR0_NAME_CONST;
	len_vec[0] = FT_PMKR0_NAME_CONST_SIZE;
	data_vec[1] = salt_buff;
	len_vec[1] = PMKID_LEN;
	if (sha256_vector(2, data_vec, len_vec, tmpBuf)) {
		DEBUG_ERR("Error: sha256 fail\n");
		return;
	}
	memcpy(pmk_r0_name, tmpBuf, PMKID_LEN);

#ifdef DEBUG_PSK
	debug_out("PSK: Generate PMK_R0_Name=", pmk_r0_name, PMKID_LEN);
#endif

	if (store_r0kh(priv, pstat->cmn_info.mac_addr, pmk_r0, pmk_r0_name)) {
		DEBUG_ERR("Error: can't store r0kh\n");
		return;
	}

	// Calc PMK-R1
	pos = data;
	memcpy(pos, BSSID, MACADDRLEN);
	pos += MACADDRLEN;

#ifdef CONFIG_IEEE80211R_CLI
	if(OPMODE & WIFI_STATION_STATE)
		memcpy(pos, GET_MY_HWADDR, MACADDRLEN);
	else
#endif
		memcpy(pos, pstat->cmn_info.mac_addr, MACADDRLEN);

	pos += MACADDRLEN;
	sha256_prf(pmk_r0, PMK_LEN, (unsigned char *)FT_PMKR1_CONST,
		data, pos - data, tmpBuf, 32);
	memcpy(pmk_r1, tmpBuf, PMK_LEN);

#ifdef DEBUG_PSK
	debug_out("PSK: Generate PMK_R1=", pmk_r1, PMK_LEN);
#endif

	// Calc PMK-R1Name
	data_vec[0] = FT_PMKR1_NAME_CONST;
	len_vec[0] = FT_PMKR1_NAME_CONST_SIZE;
	data_vec[1] = pmk_r0_name;
	len_vec[1] = PMKID_LEN;
	data_vec[2] = BSSID;
	len_vec[2] = MACADDRLEN;

#ifdef CONFIG_IEEE80211R_CLI
	if(OPMODE & WIFI_STATION_STATE)
		data_vec[3] = GET_MY_HWADDR;
	else
#endif
		data_vec[3] = pstat->cmn_info.mac_addr;

	len_vec[3] = MACADDRLEN;
	if (sha256_vector(4, data_vec, len_vec, tmpBuf)) {
		DEBUG_ERR("Error: sha256 fail\n");
		return;
	}
	memcpy(pmk_r1_name, tmpBuf, PMKID_LEN);

#ifdef DEBUG_PSK
	panic_printk("(%s)line=%d, stored addr: [%02x][%02x][%02x][%02x][%02x][%02x]\n", __FUNCTION__, __LINE__,
			pstat->cmn_info.mac_addr[0], pstat->cmn_info.mac_addr[1], pstat->cmn_info.mac_addr[2], pstat->cmn_info.mac_addr[3], pstat->cmn_info.mac_addr[4], pstat->cmn_info.mac_addr[5]);
	debug_out("CalcFTPTK: Generate PMK_R0_Name=", pmk_r0_name, PMKID_LEN);
	debug_out("CalcFTPTK: Generate PMK_R1_Name=", pmk_r1_name, PMKID_LEN);
	debug_out("CalcFTPTK: Generate PMK_R0=", pmk_r0, PMKID_LEN);
	debug_out("CalcFTPTK: Generate PMK_R1=", pmk_r1, PMK_LEN);
#endif

	// Calc PTK
	pos = data;
	memcpy(pos, pStaInfo->SNonce.Octet, KEY_NONCE_LEN);
	pos += KEY_NONCE_LEN;
	memcpy(pos, pStaInfo->ANonce.Octet, KEY_NONCE_LEN);
	pos += KEY_NONCE_LEN;
	memcpy(pos, BSSID, MACADDRLEN);
	pos += MACADDRLEN;

#ifdef CONFIG_IEEE80211R_CLI
	if(OPMODE & WIFI_STATION_STATE)
		memcpy(pos, GET_MY_HWADDR, MACADDRLEN);
	else
#endif
		memcpy(pos, pstat->cmn_info.mac_addr, MACADDRLEN);

	pos += MACADDRLEN;
	sha256_prf(pmk_r1, PMK_LEN, (unsigned char *)FT_PMK_EXPANSION_CONST,
		data, pos - data, tmpBuf, keyoutlen);
	memcpy(keyout, tmpBuf, keyoutlen);

	if (store_r1kh(priv, pstat->cmn_info.mac_addr, BSSID, R0KH_ID, R0KH_ID_LEN,
        pmk_r1, pmk_r1_name, pmk_r0_name, mapPairwise(pstat->wpa_sta_info->UnicastCipher))) {
		DEBUG_ERR("Error: fail to store r1kh\n");
		return;
	}
	pStaInfo->r1kh = search_r1kh(priv, pstat->cmn_info.mac_addr, BSSID);
}

static unsigned char *getPMKID(unsigned int index, unsigned char *rsnie, unsigned int rsnie_len)
{
	unsigned char *pos;
	unsigned int pmk_cnt;

	pos = rsnie + 8;
	pos += 2 + 4 * le16_to_cpu(*(unsigned short *)pos);
	pos += 2 + 4 * le16_to_cpu(*(unsigned short *)pos);
	pos += 2;
	pmk_cnt = le16_to_cpu(*(unsigned short *)pos);
	pos += 2;
	if (index < pmk_cnt && (pos + index * 16) < (rsnie + rsnie_len))
		return (pos + index * 16);
	return NULL;
}

static int isFTAuth(struct rtl8192cd_priv *priv, unsigned char *rsnie, unsigned int rsnie_len, int psk)
{
	unsigned int akm_cnt, i;
	unsigned char akm_ft[4] = {0x00, 0x0f, 0xac, 0x03};
	unsigned char akm_ft_psk[4] = {0x00, 0x0f, 0xac, 0x04};
	unsigned char *pos = rsnie;

	pos += 8;
	pos += 2 + 4 * le16_to_cpu(*(unsigned short *)pos);
	akm_cnt = le16_to_cpu(*(unsigned short *)pos);
	pos += 2;

	for (i = 0; i < akm_cnt; i++) {
		if ((!psk && !memcmp(pos + (i * 4), akm_ft, 4)) ||
			 ((psk & PSK_WPA2) && !memcmp(pos + (i * 4), akm_ft_psk, 4)))
			return 1;
	}

	return 0;
}

static int validateMDIE(struct rtl8192cd_priv *priv, unsigned char *pbuf)
{
	return ((memcmp(GetFTMDID(pbuf), MDID, 2) == 0) &&
        (GetFTOverDS(pbuf) == priv->pmib->dot11FTEntry.dot11FTOverDSEnabled) &&
        (GetFTResReq(pbuf) == priv->pmib->dot11FTEntry.dot11FTResourceRequestSupported));
}

unsigned char *getFTIESubElmt(unsigned char *ftie, unsigned int ftie_len, unsigned char elmt_id, unsigned int *outlen)
{
	unsigned char *pos;

	if (ftie_len <= sizeof(struct ft_ie_elmt_hdr) + 2)
		return NULL;

	pos = ftie + 2 + sizeof(struct ft_ie_elmt_hdr);
	while (pos - ftie < ftie_len) {
		if (*pos == elmt_id) {
			*outlen = *(pos + 1);
			return pos + 2;
		}
		else {
			pos += *(pos + 1) + 2;
		}
	}

	return NULL;
}

#define PairwiseEnc(_x_) (*((_x_) + 13))

int ft_check_imd_assoc(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *pbuf, unsigned int limit, unsigned int *status)
{
	unsigned char *p;
	unsigned int len;

//	panic_printk("==> %s\n", __FUNCTION__);

	// Check MDIE
	p = get_ie(pbuf, _MOBILITY_DOMAIN_IE_, &len, limit);
	if (!p || !validateMDIE(priv, p)) {
		*status = _STATS_INVALID_MDIE_;
		return -1;
	}

	// Check RSNIE
	if (IEEE8021X_FUN) {
		p = get_ie(pbuf, _RSN_IE_2_, &len, limit);
		if (p == NULL) {
			*status = __STATS_INVALID_IE_;
			return -1;
		}

		// Check AKM
		if (!isFTAuth(priv, p, len + 2, priv->pmib->dot1180211AuthEntry.dot11EnablePSK)) {
			*status = __STATS_INVALID_AKMP_;
			return -1;
		}
	}

	return 0;
}

int ft_check_imd_4way(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *pbuf, unsigned int limit, unsigned int *status)
{
	WPA_STA_INFO *pStaInfo = pstat->wpa_sta_info;
	unsigned char *p;
	unsigned int len;
	struct r1_key_holder *r1kh;

//	panic_printk("==> %s\n", __FUNCTION__);

	// Check MDIE
	p = get_ie(pbuf, _MOBILITY_DOMAIN_IE_, &len, limit);
	if (!p || !validateMDIE(priv, p)) {
		*status = _STATS_INVALID_MDIE_;
		return -1;
	}

	// Check RSNIE
	if (IEEE8021X_FUN) {
		p = get_ie(pbuf, _RSN_IE_2_, &len, limit);
		if (p == NULL) {
			*status = __STATS_INVALID_IE_;
			return -1;
		}

		// Check AKM
		if (!isFTAuth(priv, p, len + 2, priv->pmib->dot1180211AuthEntry.dot11EnablePSK)) {
			*status = __STATS_INVALID_AKMP_;
			return -1;
		}

		// Check PMK-R1-Name
		r1kh = pStaInfo->r1kh;
		if (!r1kh || memcmp(r1kh->pmk_r1_name, getPMKID(0, p, len + 2), PMKID_LEN)) {
			*status = _STATS_INVALID_PMKID_;
			return -1;
		}
	}

	return 0;
}

#ifdef SUPPORT_FAST_CONFIG

void ft_set_unicastCipher(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	if (priv->pmib->dot1180211AuthEntry.dot11EnablePSK & BIT(1)) {
		if (priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher == 2)
			pstat->wpa_sta_info->UnicastCipher = DOT11_ENC_TKIP;
		else if (priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher == 8)
			pstat->wpa_sta_info->UnicastCipher = DOT11_ENC_CCMP;
		else if (priv->pmib->dot1180211AuthEntry.dot11WPA2Cipher == 10)
			pstat->wpa_sta_info->UnicastCipher = DOT11_ENC_CCMP;
		else
			DEBUG_ERR("unicastcipher  nothing\n");
	}
}

void ft_auto_generate_pmk(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *r0kh_id,
    unsigned int r0kh_id_len, unsigned char *pmk_r0_name, unsigned char *cur_pmk_r1_name)
{
	WPA_STA_INFO *pStaInfo = pstat->wpa_sta_info;
	unsigned char *pos;
	unsigned char tmpBuf[128];
	unsigned char data[512];
	unsigned char pmk_r0[PMK_LEN], pmk_r1[PMK_LEN], cur_pmk_r1[PMK_LEN];
	unsigned char pmk_r1_name[PMKID_LEN];
	unsigned char *data_vec[4];
	size_t len_vec[4];
	unsigned pmkBuf[PMK_LEN];
    unsigned char bssid_buf[MACADDRLEN];

#ifdef DEBUG_PSK
	panic_printk("==> %s\n", __FUNCTION__);
#endif

	// Genererated Previous AP's Key info

	// Calc PMK-R0
	pos = data;
	*pos++ = (unsigned char)SSID_LEN;
	memcpy(pos, SSID, SSID_LEN);
	pos += SSID_LEN;
	memcpy(pos, MDID, 2);
	pos += 2;
	*pos++ = r0kh_id_len;
	memcpy(pos, r0kh_id, r0kh_id_len);
	pos += r0kh_id_len;
	memcpy(pos, pstat->cmn_info.mac_addr, MACADDRLEN);
	pos += MACADDRLEN;

	memcpy(pmkBuf, priv->wpa_global_info->PSK, PMK_LEN);
	sha256_prf(pmkBuf, PMK_LEN, (unsigned char*)FT_PMKR0_CONST, data, pos - data, tmpBuf, 48);
	memcpy(pmk_r0, tmpBuf, PMK_LEN);

	if (store_r0kh(priv, pstat->cmn_info.mac_addr, pmk_r0, pmk_r0_name)) {
		DEBUG_ERR("Error: can't store r0kh\n");
		return;
	}

    ft_set_unicastCipher(priv, pstat);

	// Get BSSID of the previous AP embedded in R0KH-ID(MAC.xxx.yyy.zzz)
	if(r0kh_id_len < MACADDRLEN*2 || !rtl_string_to_hex(r0kh_id, bssid_buf, MACADDRLEN*2)) {
		DEBUG_WARN("Not calculate PMK-R1 of the previous AP (%d, %s).\n", r0kh_id_len, r0kh_id);
	} else {
        // Calc PMK-R1
        pos = data;
        memcpy(pos, bssid_buf, MACADDRLEN);
        pos += MACADDRLEN;
        memcpy(pos, pstat->cmn_info.mac_addr, MACADDRLEN);
        pos += MACADDRLEN;
        sha256_prf(pmk_r0, PMK_LEN, (unsigned char *)FT_PMKR1_CONST, data, pos - data, tmpBuf, 32);
        memcpy(pmk_r1, tmpBuf, PMK_LEN);

        // Calc PMK-R1Name
        data_vec[0] = FT_PMKR1_NAME_CONST;
        len_vec[0] = FT_PMKR1_NAME_CONST_SIZE;
        data_vec[1] = pmk_r0_name;
        len_vec[1] = PMKID_LEN;
        data_vec[2] = bssid_buf;
        len_vec[2] = MACADDRLEN;
        data_vec[3] = pstat->cmn_info.mac_addr;
        len_vec[3] = MACADDRLEN;
        if (sha256_vector(4, data_vec, len_vec, tmpBuf)) {
            DEBUG_ERR("Error: sha256 fail\n");
            return;
        }
        memcpy(pmk_r1_name, tmpBuf, PMKID_LEN);

#ifdef DEBUG_PSK
        debug_out("ft_auto_generate_pmk: (Previous) PMK_R1_Name=", pmk_r1_name, PMKID_LEN);
        debug_out("ft_auto_generate_pmk: (Previous) PMK_R1=", pmk_r1, PMK_LEN);
#endif

    	//store previous AP R1KH
    	if (store_r1kh(priv, pstat->cmn_info.mac_addr, bssid_buf, r0kh_id, r0kh_id_len,
    		pmk_r1, pmk_r1_name, pmk_r0_name, mapPairwise(pstat->wpa_sta_info->UnicastCipher))) {
    		panic_printk("Error: fail to store r1kh\n");
    		return;
    	}
	}

	// Calc PMK-R1 of Current AP
	pos = data;
	memcpy(pos, BSSID, MACADDRLEN);
	pos += MACADDRLEN;
	memcpy(pos, pstat->cmn_info.mac_addr, MACADDRLEN);
	pos += MACADDRLEN;
	sha256_prf(pmk_r0, PMK_LEN, (unsigned char *)FT_PMKR1_CONST, data, pos - data, tmpBuf, 32);
	memcpy(cur_pmk_r1, tmpBuf, PMK_LEN);

#ifdef DEBUG_PSK
    debug_out("ft_auto_generate_pmk: (Current) PMK_R1=", cur_pmk_r1, PMK_LEN);
#endif
    //stored current AP R1KH
    if (store_r1kh(priv, pstat->cmn_info.mac_addr, BSSID, r0kh_id, r0kh_id_len,
        cur_pmk_r1, cur_pmk_r1_name, pmk_r0_name, mapPairwise(pstat->wpa_sta_info->UnicastCipher))) {
        panic_printk("Error: fail to store r1kh\n");
        return;
    }
}
#endif

#ifdef CONFIG_IEEE80211R_CLI
int ft_check_ft_auth_rsp(struct rtl8192cd_priv *priv, unsigned char *pbuf, unsigned int limit, unsigned int *status, struct rx_frinfo *pfrinfo)
{
	struct r1_key_holder *r1kh = NULL;
	struct r0_key_holder *r0kh = NULL;
	size_t len_vec[4];
	unsigned char *data_vec[4];
	unsigned char data[512];
	unsigned char tmpBuf[128];
	unsigned char tmp_ftie[512];
	unsigned char *p, *keyid, *r0kh_id, *r1kh_id;
	unsigned int len, r0kh_id_len, r1kh_id_len;
	unsigned char  cur_pmk_r1[PMK_LEN];
	unsigned char pmk_r0_name[PMKID_LEN], pmk_r1_name[PMKID_LEN];

	unsigned char *ftie, *mdie;
	unsigned int rsnie_len, ftie_len, mdie_len;
	struct ft_ie_elmt_hdr *ftie_hdr, *tmp_ftie_hdr;

	int pairwise;
	unsigned char *pos;

#ifdef DEBUG_PSK
	panic_printk("==> %s\n", __FUNCTION__);
#endif

	p = get_ie(pbuf, _MOBILITY_DOMAIN_IE_, &len, limit);
	if (!p || !validateMDIE(priv, p)) {
		*status = _STATS_INVALID_MDIE_;
		return -1;
	}

	mdie = p;
	mdie_len = len + 2;

	if (IEEE8021X_FUN) {
		p = get_ie(pbuf, _RSN_IE_2_, &len, limit);
		if (p == NULL) {
			*status = __STATS_INVALID_IE_;
			return -1;
		}

		// Check AKM
		if (!isFTAuth(priv, p, len + 2, priv->pmib->dot1180211AuthEntry.dot11EnablePSK)) {
			*status = __STATS_INVALID_AKMP_;
			return -1;
		}

		pairwise = PairwiseEnc(p);
		// Get PMK-R0-Name
		keyid = getPMKID(0, p, len + 2);
		if (!keyid) {
			*status = _STATS_INVALID_PMKID_;
			return -1;
		}

		//  Get FT Initial AP's PMK_R0
		r0kh = search_r0kh(priv, priv->init_ap);
		if(!memcmp(r0kh->pmk_r0_name, keyid, 16))
		{
#ifdef DEBUG_PSK		
			panic_printk("(%s)line=%d, pmk_r0_name check ok!\n", __FUNCTION__, __LINE__);
#endif
		}
		else {
#ifdef DEBUG_PSK
			debug_out("FT Initial_AP:", priv->init_ap, MACADDRLEN);
#endif
			*status = _STATS_INVALID_PMKID_;
			return -1;
		}

		// Calc Current AP's PMK-R1
		pos = data;
		memcpy(pos, pfrinfo->sa, MACADDRLEN);
		pos += MACADDRLEN;
		memcpy(pos, GET_MY_HWADDR, MACADDRLEN);
		pos += MACADDRLEN;
		sha256_prf(r0kh->pmk_r0, PMK_LEN, (unsigned char *)FT_PMKR1_CONST, data, pos - data, tmpBuf, 32);
		memcpy(cur_pmk_r1, tmpBuf, PMK_LEN);

		// Calc Current AP's PMK-R1Name
		data_vec[0] = FT_PMKR1_NAME_CONST;
		len_vec[0] = FT_PMKR1_NAME_CONST_SIZE;
		data_vec[1] = r0kh->pmk_r0_name;
		len_vec[1] = PMKID_LEN;
		data_vec[2] =  pfrinfo->sa;
		len_vec[2] = MACADDRLEN;
		data_vec[3] = GET_MY_HWADDR;
		len_vec[3] = MACADDRLEN;
		if (sha256_vector(4, data_vec, len_vec, tmpBuf)) {
			DEBUG_ERR("Error: sha256 fail\n");
			return -1;
		}
		memcpy(pmk_r1_name, tmpBuf, PMKID_LEN);

		r1kh = search_r1kh(priv, priv->init_ap, priv->init_ap);
		if(pfrinfo->sa, priv->init_ap, MACADDRLEN) {
			// Update initialized AP r1hk
			if (store_r1kh(priv,  pfrinfo->sa, pfrinfo->sa, priv->init_r0kh_id, priv->init_r0kh_id_len,
				cur_pmk_r1, pmk_r1_name, keyid, pairwise)) {
					panic_printk("Error: fail to store r1kh\n");
					return -1;
			}
		}else {
			// Stored Current AP's R1KH
			if (store_r1kh(priv,  pfrinfo->sa, pfrinfo->sa, r1kh->r0kh_id, r1kh->r0kh_id_len,
				cur_pmk_r1, pmk_r1_name, keyid, pairwise)) {
					panic_printk("Error: fail to store r1kh\n");
					return -1;
			}
		}
		//r1kh = search_r1kh_by_pmkid(priv, pmk_r1_name, 1);
		p = get_ie(pbuf, _FAST_BSS_TRANSITION_IE_, &len, limit);
		if (p == NULL) {
			*status = __STATS_INVALID_IE_;
			return -1;
		}
		if (len > 512) {
			*status = _STATS_INVALID_FTIE_;
			return -1;
		}

		memcpy(tmp_ftie, p, len + 2);
		memset(tmp_ftie + 4, 0, KEY_MIC_LEN);
		SetFTMICCtrl(tmp_ftie+2, 3);
		ftie = tmp_ftie;
		ftie_len = len + 2;
		//tmp_ftie_hdr = (struct ft_ie_elmt_hdr *)(tmp_ftie + 2);
		ftie_hdr = (struct ft_ie_elmt_hdr *)(p + 2);

		if (memcmp(ftie_hdr->SNonce, priv->ftie_SNonce.Octet, KEY_NONCE_LEN)) {
			*status = _STATS_INVALID_FTIE_;
			return -1;
		}

		r0kh_id = getFTIESubElmt(p, len + 2, _FT_R0KH_ID_SUB_IE_, &r0kh_id_len);
		if (!r0kh_id || memcmp(r0kh_id,  r1kh->r0kh_id, r1kh->r0kh_id_len)) {
			*status = _STATS_INVALID_FTIE_;
			return -1;
		}

		r1kh_id = getFTIESubElmt(p, len + 2, _FT_R1KH_ID_SUB_IE_, &r1kh_id_len);
		if (!r1kh_id || memcmp(r1kh_id, pfrinfo->sa, MACADDRLEN)) {
			*status = _STATS_INVALID_FTIE_;
			return -1;
		}

		// Copy ANonce from Current AP
		memcpy(priv->ftie_ANonce.Octet, ftie_hdr->ANonce, KEY_NONCE_LEN);

		// record Current AP's addr
		memcpy(priv->cur_ap, pfrinfo->sa, MACADDRLEN);

#if 0//def DEBUG_PSK
		debug_out(" Initial AP's r0kh->pmk_r0:", r0kh->pmk_r0, PMKID_LEN);
		debug_out(" cur_pmk_r1:", cur_pmk_r1, PMK_LEN);
		debug_out("pmk_r1_name=", pmk_r1_name, PMKID_LEN);
		debug_out("FT ANonce", ftie_hdr->ANonce, KEY_NONCE_LEN);
		debug_out("FT Snonce", ftie_hdr->SNonce, KEY_NONCE_LEN);
		debug_out("CLI Snonce", priv->ftie_SNonce.Octet, KEY_NONCE_LEN);
#endif
		unsigned char rsnie[64] = {0};
		unsigned int rsnie_len = 0;
		if(ft_auth_set_ft_rsnie_with_pmkid(priv, rsnie, &rsnie_len) == NULL) {
			panic_printk("(%s)line=%d, error\n", __FUNCTION__, __LINE__);
		}else {
			derive_ft_auth_keys(priv, pfrinfo);
			calc_ft_mic(priv, NULL, rsnie, rsnie_len, mdie, mdie_len, ftie, ftie_len, 5);
			memcpy(priv->FT_MIC, ftie+4, KEY_MIC_LEN);
		}
	}

	return 0;
}
#endif

int ft_check_ft_auth(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *pbuf, unsigned int limit, unsigned int *status)
{
	WPA_STA_INFO *pStaInfo = pstat->wpa_sta_info;
	WPA_GLOBAL_INFO *pGblInfo = priv->wpa_global_info;
	unsigned char *p, *pmkid, pmk_r1_name[PMKID_LEN];
	unsigned int len, r0kh_id_len;
	unsigned char *r0kh_id;
	struct r1_key_holder *r1kh = NULL, tmp;
	int pairwise;

#ifdef DEBUG_PSK
	panic_printk("==> %s\n", __FUNCTION__);
#endif

	// Check MDIE
	p = get_ie(pbuf, _MOBILITY_DOMAIN_IE_, &len, limit);
	if (!p || !validateMDIE(priv, p)) {
		*status = _STATS_INVALID_MDIE_;
		return -1;
	}

	// Check RSNIE
	if (IEEE8021X_FUN) {
	    unsigned long flags;
		reset_sta_info(priv, pstat);
		ft_init_1x(priv, pstat);

		p = get_ie(pbuf, _RSN_IE_2_, &len, limit);
		if (p == NULL) {
			*status = __STATS_INVALID_IE_;
			return -1;
		}

		// Check AKM
		if (!isFTAuth(priv, p, len + 2, priv->pmib->dot1180211AuthEntry.dot11EnablePSK)) {
			*status = __STATS_INVALID_AKMP_;
			return -1;
		}

		pairwise = PairwiseEnc(p);

		// Get PMK-R0-Name
		pmkid = getPMKID(0, p, len + 2);
		if (!pmkid) {
			*status = _STATS_INVALID_PMKID_;
			return -1;
		}
		memcpy(pStaInfo->cache_pmk_r0_name, pmkid, PMKID_LEN);
#ifdef DEBUG_PSK
        debug_out("cache_pmk_r0_name: ", pStaInfo->cache_pmk_r0_name, PMKID_LEN);
#endif
        // Drive PMK-R1-Name for the current AP
		derive_ft_pmk_r1_id(priv, pstat, pStaInfo->cache_pmk_r0_name, pmk_r1_name);
#ifdef DEBUG_PSK
        debug_out("(Current) PMK_R1_Name=", pmk_r1_name, PMKID_LEN);
#endif
		p = get_ie(pbuf, _FAST_BSS_TRANSITION_IE_, &len, limit);
		if (p == NULL) {
			*status = __STATS_INVALID_IE_;
			return -1;
		}

		INCOctet32_INTEGER(&pGblInfo->Counter);
		SetNonce(pStaInfo->ANonce, pGblInfo->Counter);

		memcpy(pStaInfo->SNonce.Octet, p + 52, KEY_NONCE_LEN);
		pStaInfo->SNonce.Length = KEY_NONCE_LEN;

		r0kh_id = getFTIESubElmt(p, len + 2, _FT_R0KH_ID_SUB_IE_, &r0kh_id_len);
		if (!r0kh_id) {
			panic_printk("%s: fail to get r0kh_id\n", __FUNCTION__);
			*status = _STATS_INVALID_FTIE_;
			DRV_RT_TRACE(priv, DRV_DBG_CONN_INFO, DRV_DBG_SERIOUS, "_STATS_INVALID_FTIE_\n");
			return -1;
		}
		memcpy(pStaInfo->cache_r0kh_id, r0kh_id, r0kh_id_len);
		pStaInfo->cache_r0kh_id_len = r0kh_id_len;
#ifdef DEBUG_PSK
        debug_out("cache_r0kh_id: ", pStaInfo->cache_r0kh_id, pStaInfo->cache_r0kh_id_len);
#endif

		/*search rdy exist r1KH*/
		r1kh = search_r1kh_by_pmkid(priv, pmk_r1_name, 1);
#if (SUPPORT_FAST_CONFIG == R1KH_CALC_ONLY) || (SUPPORT_FAST_CONFIG == R1KH_PSUH_CALC_MIX)
#if (SUPPORT_FAST_CONFIG == R1KH_PSUH_CALC_MIX)
		if (!r1kh)
#endif
		{   /*CALC R1HK DIY*/
		    if(FT_ENABLE && priv->pmib->dot11FTEntry.dot11FT4way)
		    {
    			ft_auto_generate_pmk(priv, pstat, pStaInfo->cache_r0kh_id, pStaInfo->cache_r0kh_id_len,
                    pStaInfo->cache_pmk_r0_name, pmk_r1_name);
		         /*search rdy exist r1KH*/
		         r1kh = search_r1kh_by_pmkid(priv, pmk_r1_name, 1);
    		}
		}
#endif
		SMP_LOCK_FT_R1KH(flags);
#if (SUPPORT_FAST_CONFIG == R1KH_PUSH_PULL)
		/*  not found r1KH
			means no one push r1KH to me
			going to pull r1KH*/
		if (!r1kh) {
			// pull from other AP.
#ifdef DEBUG_PSK
			panic_printk("dot11FT4way = %d, (pull from other AP ? ) \n", priv->pmib->dot11FTEntry.dot11FT4way);
#endif
			memset(&tmp, 0, sizeof(struct r1_key_holder));
			memcpy(tmp.r0kh_id, pStaInfo->cache_r0kh_id, pStaInfo->cache_r0kh_id_len);
			tmp.r0kh_id_len = pStaInfo->cache_r0kh_id_len;
			memcpy(tmp.sta_addr, pstat->cmn_info.mac_addr, MACADDRLEN);
			memcpy(tmp.r1kh_id, BSSID, MACADDRLEN);
			memcpy(tmp.pmk_r0_name, pStaInfo->cache_pmk_r0_name, PMKID_LEN);

            FT_IndicateEvent(priv, pstat, DOT11_EVENT_FT_PULL_KEY_IND, &tmp);

			//*status = _STATS_INVALID_PMKID_;
			//return -1;
			SMP_UNLOCK_FT_R1KH(flags);
			return 0;
		}
#endif

		if (r1kh->pairwise != mapPairwise(pairwise)) {
			DEBUG_WARN("Pairwise cipher %d is different with R0KH(%d)\n", pairwise, r1kh->pairwise);
		    SMP_UNLOCK_FT_R1KH(flags);
			*status = _STATS_INVALID_PAIRWISE_CIPHER_;
			return -1;
		}

		pStaInfo->r1kh = r1kh;
		memset(pStaInfo->r1kh->r0kh_id, 0, MAX_R0KHID_LEN);
		memcpy(pStaInfo->r1kh->r0kh_id, pStaInfo->cache_r0kh_id, pStaInfo->cache_r0kh_id_len);
		pStaInfo->r1kh->r0kh_id_len = pStaInfo->cache_r0kh_id_len;
		SMP_UNLOCK_FT_R1KH(flags);
		derive_ft_keys(priv, pstat);
	}

	return 0;
}

int ft_check_ft_auth_rrq(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *pbuf, unsigned int limit, unsigned int *status)
{
	panic_printk("==> %s\n", __FUNCTION__);
	panic_printk("FT Resource Request Protocol not support yet\n");
	*status = _STATS_FAILURE_;
	return -1;
}

int ft_check_ft_assoc(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *pbuf, unsigned int limit, unsigned int *status)
{
	WPA_STA_INFO *pStaInfo = pstat->wpa_sta_info;
	WPA_GLOBAL_INFO *pGblInfo = priv->wpa_global_info;
	unsigned char *p, *pmkid, *r0kh_id, *r1kh_id;
	unsigned int len, r0kh_id_len, r1kh_id_len;
	struct ft_ie_elmt_hdr *ftie_hdr, *tmp_ftie_hdr;
	unsigned char *rsnie, *ftie, *mdie;
	unsigned int rsnie_len, ftie_len, mdie_len;
	unsigned char tmp_ftie[514];

#ifdef DEBUG_PSK
	panic_printk("==> %s\n", __FUNCTION__);
#endif

	// Check MDIE
	p = get_ie(pbuf, _MOBILITY_DOMAIN_IE_, &len, limit);
	if (!p || !validateMDIE(priv, p)) {
		*status = _STATS_INVALID_MDIE_;
		return -1;
	}
	mdie = p;
	mdie_len = len + 2;

	// Check RSNIE
	if (IEEE8021X_FUN) {
		p = get_ie(pbuf, _RSN_IE_2_, &len, limit);
		if (p == NULL) {
			*status = __STATS_INVALID_IE_;
			return -1;
		}
		rsnie = p;
		rsnie_len = len + 2;

		// Check AKM
		if (!isFTAuth(priv, p, len + 2, priv->pmib->dot1180211AuthEntry.dot11EnablePSK)) {
			*status = __STATS_INVALID_AKMP_;
			return -1;
		}

		// Get PMK-R1-Name
		pmkid = getPMKID(0, p, len + 2);
		if (!pmkid) {
			*status = _STATS_INVALID_PMKID_;
			return -1;
		}

		// Check PMK ID
		if (!pStaInfo->r1kh || memcmp(pStaInfo->r1kh->pmk_r1_name, pmkid, PMKID_LEN)) {
			*status = _STATS_INVALID_PMKID_;
			return -1;
		}

		// Check FTIE
		p = get_ie(pbuf, _FAST_BSS_TRANSITION_IE_, &len, limit);
		if (p == NULL) {
			*status = __STATS_INVALID_IE_;
			return -1;
		}
		if (len > 512) {
			*status = _STATS_INVALID_FTIE_;
			return -1;
		}
		memcpy(tmp_ftie, p, len + 2);
		memset(tmp_ftie + 4, 0, KEY_MIC_LEN);
		ftie = tmp_ftie;
		ftie_len = len + 2;
		tmp_ftie_hdr = (struct ft_ie_elmt_hdr *)(tmp_ftie + 2);

		ftie_hdr = (struct ft_ie_elmt_hdr *)(p + 2);
		if (memcmp(ftie_hdr->ANonce, pStaInfo->ANonce.Octet, KEY_NONCE_LEN) ||
			memcmp(ftie_hdr->SNonce, pStaInfo->SNonce.Octet, KEY_NONCE_LEN)) {
			*status = _STATS_INVALID_FTIE_;
			return -1;
		}

		r0kh_id = getFTIESubElmt(p, len + 2, _FT_R0KH_ID_SUB_IE_, &r0kh_id_len);
		if (!r0kh_id || memcmp(r0kh_id, pStaInfo->cache_r0kh_id, pStaInfo->cache_r0kh_id_len)) {
			*status = _STATS_INVALID_FTIE_;
			return -1;
		}

		r1kh_id = getFTIESubElmt(p, len + 2, _FT_R1KH_ID_SUB_IE_, &r1kh_id_len);
		if (!r1kh_id || memcmp(r1kh_id, pStaInfo->r1kh->r1kh_id, MACADDRLEN)) {
			*status = _STATS_INVALID_FTIE_;
			return -1;
		}

		// Check MIC
		calc_ft_mic(priv, pstat, rsnie, rsnie_len, mdie, mdie_len, ftie, ftie_len, 5);
		if (memcmp(ftie_hdr->mic, tmp_ftie_hdr->mic, KEY_MIC_LEN)) {
			*status = _STATS_FAILURE_;
			return -1;
		}
	}

	return 0;
}

void calc_ft_mic(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *rsnie, unsigned int rsnie_len,
	unsigned char *mdie, unsigned int mdie_len, unsigned char *ftie, unsigned int ftie_len, unsigned int seq)
{
	WPA_STA_INFO *pStaInfo = NULL;
	unsigned char temp[512];
	unsigned char mic[KEY_MIC_LEN];
	unsigned char *pos;

	pos = temp;

#ifdef CONFIG_IEEE80211R_CLI
	if(priv->CliFTAuthState == state_ft_auth_req &&  (OPMODE & WIFI_STATION_STATE)) {
		memcpy(pos, GET_MY_HWADDR, MACADDRLEN);
		pos += MACADDRLEN;
		memcpy(pos, priv->cur_ap, MACADDRLEN);
		pos += MACADDRLEN;
	}else
#endif
	{
		memcpy(pos, pstat->cmn_info.mac_addr, MACADDRLEN);
		pos += MACADDRLEN;
		memcpy(pos, BSSID, MACADDRLEN);
		pos += MACADDRLEN;
	}

	*pos++ = seq;
	memcpy(pos, rsnie, rsnie_len);
	pos += rsnie_len;
	memcpy(pos, mdie, mdie_len);
	pos += mdie_len;
	memcpy(pos, ftie, ftie_len);
	pos += ftie_len;

#ifdef CONFIG_IEEE80211R_CLI
	if((priv->CliFTAuthState == state_ft_auth_req) &&  (OPMODE & WIFI_STATION_STATE)) {
		omac1_aes_128(priv->FT_PTK, temp, pos - temp, mic);
	}else
#endif
	{
		pStaInfo = pstat->wpa_sta_info;
		omac1_aes_128(pStaInfo->PTK, temp, pos - temp, mic);
	}
	memcpy(ftie + 4, mic, KEY_MIC_LEN);
}

void derive_ft_keys(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
#ifdef DEBUG_PSK
	printk("===> derive_ft_keys\n");
#endif
	derive_ft_ptk(pstat->wpa_sta_info->r1kh, pstat->wpa_sta_info->SNonce.Octet,
		pstat->wpa_sta_info->ANonce.Octet, pstat->wpa_sta_info->PTK, 48);

	UpdateGK(priv);
}

void install_ft_keys(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	if (pstat->ieee8021x_ctrlport == DOT11_PortStatus_Unauthorized) {
#ifdef DEBUG_PSK
		printk("===> install_ft_keys\n");
#endif

		ToDrv_SetPTK(priv, pstat);
		ToDrv_SetPort(priv, pstat, DOT11_PortStatus_Authorized);
		ToDrv_SetGTK(priv);
	} else {
#ifdef DEBUG_PSK
		printk("===> already install_ft_keys, do not re-install\n");
#endif
	}
}

void ft_enc_gtk(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *out, unsigned short *outlen)
{
	WPA_STA_INFO *pStaInfo = pstat->wpa_sta_info;
	WPA_GLOBAL_INFO *pGblInfo = priv->wpa_global_info;

	EncGTK(priv, pstat, pStaInfo->PTK + PTK_LEN_EAPOLMIC, PTK_LEN_EAPOLENC,
		pGblInfo->GTK[pGblInfo->GN], (pGblInfo->MulticastCipher == DOT11_ENC_TKIP) ? 32 : 16, out, outlen);
}

void ft_init_1x(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	if (IEEE8021X_FUN && !priv->pmib->dot1180211AuthEntry.dot11EnablePSK) {
		reset_sta_info(priv, pstat);
		pstat->wpa_sta_info->RSNEnabled = 3;
	}
}

#ifdef CONFIG_IEEE80211R_CLI
void derive_ft_auth_keys(struct rtl8192cd_priv *priv, struct rx_frinfo *pfrinfo)
{
	struct r1_key_holder *r1kh = search_r1kh(priv, pfrinfo->sa, pfrinfo->sa);
	unsigned char data[512] = {0};
	unsigned char *pos;

	pos = data;
	memcpy(pos, priv->ftie_SNonce.Octet, KEY_NONCE_LEN);
	pos += KEY_NONCE_LEN;
	memcpy(pos, priv->ftie_ANonce.Octet, KEY_NONCE_LEN);
	pos += KEY_NONCE_LEN;
	memcpy(pos, r1kh->r1kh_id, MACADDRLEN);
	pos += MACADDRLEN;
	memcpy(pos, GET_MY_HWADDR, MACADDRLEN);
	pos += MACADDRLEN;

	sha256_prf(r1kh->pmk_r1, PMK_LEN, (unsigned char *)FT_PMK_EXPANSION_CONST,
		data, pos - data, priv->FT_PTK, 48);

#ifdef DEBUG_PSK
	//debug_out("derive_ft_auth_keys: data: ",data, pos - data);
	debug_out("FT-Key CLI",  priv->FT_PTK, 48);
#endif
}

int ft_check_imd_assocrsp(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *pbuf, unsigned int limit,  unsigned int *status)
{
	unsigned char *p;
	unsigned int len;

#ifdef DEBUG_PSK
	panic_printk("==> %s\n", __FUNCTION__);
#endif

	// Check MDIE
	p = get_ie(pbuf, _MOBILITY_DOMAIN_IE_, &len, limit);
	if (!p || !validateMDIE(priv, p)) {
		return -1;
	}

	return 0;
}

void ft_set_reassoc_keyInfo(struct rtl8192cd_priv *priv, struct stat_info *pstat)
{
	//copy PTK to pstat
	memcpy(pstat->wpa_sta_info->PTK, priv->FT_PTK, PTK_LEN_TKIP);
#ifdef DEBUG_PSK
	debug_out("ft_set_reassoc_keyInfo: PTK", pstat->wpa_sta_info->PTK, PTK_LEN_TKIP);
#endif

}

#define FT_MIC_LEN	24
#define FT_RSC_LEN	8
#define FT_MIC_KEY_LEN	1
#define FT_KEY_ID_LEN		2
#define IV_KEY_LEN		8

int ft_check_ft_reassocrsp(struct rtl8192cd_priv *priv, struct stat_info *pstat, unsigned char *pbuf, unsigned int limit, unsigned int *status)
{
	WPA_GLOBAL_INFO *pGblInfo = priv->wpa_global_info;
	WPA_STA_INFO *pStaInfo = pstat->wpa_sta_info;
	unsigned char *p;
	unsigned char tmp_ftie[512];

	unsigned char *ftie;
	unsigned int ftie_len;
	struct ft_ie_elmt_hdr *ftie_hdr;
	unsigned char *r0kh_id, *r1kh_id;
	unsigned int len, gtk_len, gtk_ie_len, r0kh_id_len, r1kh_id_len;
	unsigned char keyID;
	unsigned char	tmp2[256] = {0};
	unsigned char tmp[64] = {0};

#ifdef DEBUG_PSK
	panic_printk("==> %s\n", __FUNCTION__);
#endif

	// Check MDIE
	p = get_ie(pbuf, _MOBILITY_DOMAIN_IE_, &len, limit);
	if (!p || !validateMDIE(priv, p)) {
		*status = __STATS_INVALID_IE_;
		return -1;
	}

	//Chcek FTIE
	p = get_ie(pbuf, _FAST_BSS_TRANSITION_IE_, &len, limit);

	if (p == NULL) {
		*status = __STATS_INVALID_IE_;
		return -1;
	}

	if (len > 512) {
			*status = _STATS_INVALID_FTIE_;
			return -1;
	}


	r0kh_id = getFTIESubElmt(p, len + 2, _FT_R0KH_ID_SUB_IE_, &r0kh_id_len);
	if (!r0kh_id) {	// || memcmp(r0kh_id,  r1kh->r0kh_id, r1kh->r0kh_id_len)) {
		*status = _STATS_INVALID_FTIE_;
		return -1;
	}

	r1kh_id = getFTIESubElmt(p, len + 2, _FT_R1KH_ID_SUB_IE_, &r1kh_id_len);
	if (!r1kh_id ) { //|| memcmp(r1kh_id, pfrinfo->sa, MACADDRLEN)) {
		*status = _STATS_INVALID_FTIE_;
		return -1;
	}

	memcpy(tmp_ftie, p, len + 2);
	ftie = tmp_ftie;
	ftie_len = len + 2;
	ftie_hdr = (struct ft_ie_elmt_hdr *)(p + 2);

	p = getFTIESubElmt(ftie, ftie_len, _FT_GTK_SUB_IE_, &gtk_ie_len);

	keyID = le16_to_cpu(*((unsigned short *)p));
	pGblInfo->GN = keyID;
	memcpy(tmp, p, gtk_ie_len);

	gtk_len = gtk_ie_len-FT_KEY_ID_LEN-FT_MIC_KEY_LEN-FT_RSC_LEN;

	AES_UnWRAP(&tmp[11], gtk_len, pStaInfo->PTK + PTK_LEN_EAPOLMIC, PTK_LEN_EAPOLENC, tmp2);
	memcpy(pGblInfo->GTK[keyID], tmp2+8, 16);

#ifdef DEBUG_PSK
	panic_printk("(%s) keyID = %d \n", __FUNCTION__, keyID);
	debug_out("FT KEK: ", pStaInfo->PTK + PTK_LEN_EAPOLMIC, 16);
	debug_out("FT: GTK :", pGblInfo->GTK[keyID], gtk_len);
#endif
	return 0;
}

void regenerate_nonce(struct rtl8192cd_priv *priv)
{
	WPA_GLOBAL_INFO *pGblInfo = priv->wpa_global_info;

	priv->ftie_ANonce.Octet = (unsigned char *)kmalloc(KEY_NONCE_LEN, GFP_ATOMIC);
	if(!priv->ftie_ANonce.Octet)
		panic_printk("(%s)line=%d, mallic failed\n", __FUNCTION__, __LINE__);
	else {
		memset(priv->ftie_ANonce.Octet, 0, KEY_NONCE_LEN);
		priv->ftie_ANonce.Length = KEY_NONCE_LEN;
	}

	priv->ftie_SNonce.Octet = (unsigned char *)kmalloc(KEY_NONCE_LEN, GFP_ATOMIC);
	if(!priv->ftie_SNonce.Octet)
		panic_printk("(%s)line=%d, mallic failed\n", __FUNCTION__, __LINE__);
	else {
		memset(priv->ftie_SNonce.Octet, 0, KEY_NONCE_LEN);
		priv->ftie_SNonce.Length = KEY_NONCE_LEN;
	}

	INCOctet32_INTEGER(&pGblInfo->Counter);
	SetNonce(priv->ftie_SNonce, pGblInfo->Counter);

	//debug_out("Snonce", priv->ftie_SNonce.Octet, KEY_NONCE_LEN);
}

#endif

#endif /*END  of defined(CONFIG_IEEE80211R) || defined(CONFIG_IEEE80211R_CLI)*/

/*AUTH_SAE_STA*/
void wpa2_err_num_to_string(int num)
{
	switch (num) {
	case WPA2IE_OK:
		printk("WPA2IE_OK\n");
		break;
	case WPA2IE_E1:
		printk("WPA2IE_E1\n");
		break;
	case WPA2IE_E2:
		printk("WPA2IE_E2\n");
		break;
	case WPA2IE_E3:
		printk("WPA2IE_E3\n");
		break;
	case WPA2IE_E522A:
		printk("WPA2IE_E522A\n");
		break;
	case WPA2IE_E522B:
		printk("WPA2IE_E522B/WPA2IE_E522D\n");
		break;
	case WPA2IE_E522C:
		printk("WPA2IE_E522C\n");
		break;
	case WPA2IE_E522D:
		printk("WPA2IE_E522D\n");
		break;
	case WPA2IE_E522E:
		printk("WPA2IE_E522E\n");
		break;
	case WPA2IE_E522F:
		printk("WPA2IE_E522F\n");
		break;
	case WPA2IE_E522G:
		printk("WPA2IE_E522G\n");
		break;
	case WPA2IE_E522H:
		printk("WPA2IE_E522 H/L\n");
		break;
	case WPA2IE_E522I:
		printk("WPA2IE_E522 I/M\n");
		break;
	case WPA2IE_E522J:
		printk("WPA2IE_E522J\n");
		break;
		break;
	case WPA2IE_E522L:
		printk("WPA2IE_E522L/WPA2IE_E522M\n");
		break;
	case WPA2IE_E524F:
		/*akm only psk, PMKID has value*/
		printk("WPA2IE_E524F\n");
		printk("akm only support psk,but PMKID cnt !=0\n");
		break;
	default:

		break;
	}
}

/*reference WPA2_Security_Improvements_Test_Plan_v1.0.pdf*/
int wpa2_ie_check(
	struct rtl8192cd_priv *priv,
	unsigned char *ie_ptr_in, int ie_len_in)
{
	unsigned char *ie_ptr = ie_ptr_in;
	int ie_len = ie_len_in;

	/*ID (1) |LEN (1) |Ver(2) */
	struct wpa2_header *ie_head_ptr = NULL;
	unsigned short ver = 0;
	/*group cipher */
	struct rsn_group_cipher *gc_ptr = NULL;
	/*pairwise cipher */
	unsigned short pc_cnt = 0;
	struct rsn_pairwise_cipher *pc_ptr = NULL;
	 /*AKM*/ unsigned short akm_cnt = 0;
	struct rsn_akm *akm_ptr = NULL;
	/*Capabilities */
	struct rsn_capability *cap_ptr;
	unsigned short cap = 0;
	 /*PMKID*/ unsigned short pmk_cnt = 0;
	struct rsn_pmk_id_set *pmk_ptr = NULL;
	/*group mgmt cipher */
	struct rsn_group_mgmt_cipher *gmc_ptr = NULL;
	unsigned char akm_psk_only = 0;

	/*---length check---*/
	if (ie_len < sizeof(struct wpa2_header))
		return WPA2IE_E522F;

	ie_head_ptr = (struct wpa2_header *)ie_ptr;
	if (ie_head_ptr->length != ie_len - 2)
		return WPA2IE_E1;

	//log("ie_len_in[%d],headlen[%d]",ie_len_in,ie_head_ptr->length);

	/*--- Ver---*/
	ver = __le16_to_cpu(ie_head_ptr->version);
	//dump_hex("ver", (unsigned char *)&ver, 2);
	if (ver != RSN_VER1)
		return WPA2IE_E522A;

	/*---Group Cipher Suite---*/
	ie_len -= sizeof(struct wpa2_header);
	ie_ptr += sizeof(struct wpa2_header);

	if (ie_len < sizeof(struct rsn_group_cipher)) {
		/*Group Cipher Suite be truncated */
		return WPA2IE_E522G;
	}
	gc_ptr = (struct rsn_group_cipher *)ie_ptr;
	dump_hex("gc", (unsigned char *)gc_ptr, 4);
	if (memcmp(gc_ptr->oui, RSN_OUI, 3))
		return WPA2IE_E2;

	if (gc_ptr->type > ENC_BIP_GMAC256) {
		log("unknown type[%u]", gc_ptr->type);
		return WPA2IE_E3;
	}

	/*---Pairwise Cipher Suite---*/
	ie_len -= sizeof(struct rsn_group_cipher);
	ie_ptr += sizeof(struct rsn_group_cipher);


	if (ie_len < 2) {
		return WPA2IE_E522H;
	}
	pc_ptr = (struct rsn_pairwise_cipher *)ie_ptr;
	pc_cnt = __le16_to_cpu(pc_ptr->count);
	//log("pc cnt[%d]", pc_cnt);
	if (pc_cnt == 0)
		return WPA2IE_E522B;	/*also WPA2IE_E522D */
	if (ie_len < (2 + pc_cnt * 4)) {
		return WPA2IE_E522H;/*also WPA2IE_E522L */
	}
	dump_hex("pc", (unsigned char *)pc_ptr+2, ( pc_cnt * 4));

	/*---AKM---*/
	ie_len -= (2 + pc_cnt * 4);
	ie_ptr += (2 + pc_cnt * 4);

	if (ie_len < 2) {
		return WPA2IE_E522I;
	}
	akm_ptr = (struct rsn_akm *)ie_ptr;
	akm_cnt = __le16_to_cpu(akm_ptr->count);
	//log("akm cnt[%d]", akm_cnt);
	if (akm_cnt == 0)
		return WPA2IE_E522C;
	if (ie_len < (2 + akm_cnt * 4)) {
		return WPA2IE_E522I;	/*also means WPA2IE_E522M */
	}
	dump_hex("akm", (unsigned char *)akm_ptr+2, (akm_cnt * 4));

	if((akm_cnt == 1) && (akm_ptr->akm[0].type == AKM_PSK))
		akm_psk_only=1;


	/*---Capability---it's not necessary,but if exist check it*/
	ie_len -= (2 + akm_cnt * 4);
	ie_ptr += (2 + akm_cnt * 4);
	if (ie_len == 0) {
		/*len =0 , no more chk*/
		return WPA2IE_OK;
	}
	if (ie_len < 2) {
		return WPA2IE_E522J;
	}
	cap_ptr = (struct rsn_capability *)ie_ptr;
	cap = __le16_to_cpu(cap_ptr->cap);
	dump_hex("Cap", (unsigned char *)cap_ptr, 2);
	if (cap & BIT(0)) {
		//log("preauthentication is supported 0x[%x]", cap);
		return WPA2IE_E522E;
	}

	/*---PMKID---it's not necessary,but if exist check it*/
	if (ie_len) {
		ie_len -= 2;
		ie_ptr += 2;

		if (ie_len == 0) {
			/*len =0 , no more chk*/
			return WPA2IE_OK;
		}
		pmk_ptr = (struct rsn_pmk_id_set *)ie_ptr;
		pmk_cnt = __le16_to_cpu(pmk_ptr->count);
		//log("pmk cnt[%d]", pmk_cnt);
		if (ie_len < (2 + pmk_cnt * 16)) {
			return WPA2IE_E522K;	/*truncated */
		}

		if(pmk_cnt)
			dump_hex("pmk", (unsigned char *)pmk_ptr+2, ( pmk_cnt * 16));

		if(pmk_cnt && akm_psk_only)
			return WPA2IE_E524F;
	}

	/*---group mgmt cipher---it's not necessary,but if exist check it*/
	if (ie_len) {

		ie_len -= (2 + pmk_cnt * 16);
		ie_ptr += (2 + pmk_cnt * 16);

		if(ie_len==0){
			/*len =0 , no more chk*/
			return WPA2IE_OK;
		}

		gmc_ptr = (struct rsn_group_mgmt_cipher *)ie_ptr;
		dump_hex("gmc", (unsigned char *)gmc_ptr, 4);
	}

	return WPA2IE_OK;
}

#endif // INCLUDE_WPA_PSK

