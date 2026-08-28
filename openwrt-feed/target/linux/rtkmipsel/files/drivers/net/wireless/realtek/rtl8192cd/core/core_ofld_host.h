#ifndef _CORE_HOST_DESC_H
#define _CORE_HOST_DESC_H


/* define TX_PKT_TYPE*/
enum txpktinfo_type_e {
    TXPKTINFO_TYPE_RAW = 0,     /* raw forward */
    TXPKTINFO_TYPE_80211_PROTO, /* 802.11 protocol frame */
    TXPKTINFO_TYPE_80211,       /* 802.11 data frame */
    TXPKTINFO_TYPE_8023,        /* 802.3 frame */ 
    TXPKTINFO_TYPE_H2D,         /* H2D */
    TXPKTINFO_TYPE_H2C,         /* H2C */
    TXPKTINFO_TYPE_AMSDU,       /* 802.3 frame in AMSDU */
    TXPKTINFO_TYPE_RSVD,        /* reserved */

    /* keep last */
    TXPKTINFO_TYPE_LAST,
    TXPKTINFO_TYPE_INVALID = TXPKTINFO_TYPE_LAST,

    /* L2/L3/L4 Offload TX */
    TXPKTINFO_TYPE_OFLD_80211_PROTO = TXPKTINFO_TYPE_80211_PROTO, /* L2 Offload */
    TXPKTINFO_TYPE_OFLD_8023 = TXPKTINFO_TYPE_8023,               /* L3/L4 Offload */
};

/** @brief H2D_TX_PKT used for Host-CPU fill to Data-CPU
  *  pktType:  000:TR-RAW Forward           001:TP-802.11Protocol
  *            010:TW-802.11Data            011:TE-802.3
  *            100:TD-H2D                   101:TC-H2C
  *            110:TA-802.3 in AMSDU        111:RESERVED
  *  isIncludeLLC   : Inlcude LLC header or not
  *  firstInAMSDU   : If PktType=100, this's first packet or not
  *  lastInAMSDU    : If PktType=100, this's last packet or not
  *  ifID           : InterfaceID (Priv)
  *  macID          : MACID in station info
  *  txPktInfoLen   : RWPtr IDX Reg
  *  hdrLen         : If PktType=000, it's 802.11 header length
  *  dummyLen       : Dummy Bytes for glance mode
  *  padding        : for dummybytes, header, llc header used
 **/
struct h2d_txpkt_info
{
#ifdef _LITTLE_ENDIAN_
	u32 ver: 4;
	u32 resv: 4;
	u32 dummy_len: 7;
	u32 dw0_resv1: 17;

	u32 pkt_type: 3;
	u32 ifid: 5;
	u32 macid: 8;
	u32 tid: 5;
	u32 last_msdu: 1;
	u32 dw1_resv2: 10;

	u32 dw2_resv1;

	u32 dw3_resv1;
#else /* _BIG_ENDIAN_ */
	u32 dw0_resv1: 17;
	u32 dummy_len: 7;
	u32 resv: 4;
	u32 ver: 4;

	u32 dw1_resv2: 10;
	u32 last_msdu: 1;
	u32 tid: 5;
	u32 macid: 8;
	u32 ifid: 5;
	u32 pkt_type: 3;

	u32 dw2_resv1;

	u32 dw3_resv1;
#endif
} __WLAN_ATTRIB_PACK__;


/* it should be bigger than txDMAinfo+txpktinfo+header+payload*/
#define D2H_RX_BUF_LEN             2400

/* define RX_PKT_TYPE*/
enum rxpktinfo_type_e {
    RXPKTINFO_TYPE_RAW = 0,     /* raw forward */
    RXPKTINFO_TYPE_80211_PROTO, /* 802.11 protocol frame */
    RXPKTINFO_TYPE_80211,       /* 802.11 data frame */
    RXPKTINFO_TYPE_8023,        /* 802.3 frame */ 
    RXPKTINFO_TYPE_D2H,         /* D2H */
    RXPKTINFO_TYPE_C2H,         /* C2H */
    RXPKTINFO_TYPE_AMSDU,       /* 802.3 frame in AMSDU */
    RXPKTINFO_TYPE_UNKNOWN,     /* unknown */

    /* keep last */
    RXPKTINFO_TYPE_LAST,
    RXPKTINFO_TYPE_INVALID = RXPKTINFO_TYPE_LAST,

    /* C2D */
    RXPKTINFO_TYPE_OFLD_C2D = RXPKTINFO_TYPE_C2H,
};

/** @brief D2H_RX_PKT used for Data-CPU fill to Host-CPU
 **/
struct d2h_rxpkt_desc
{
    u32 word0;
    u32 word1;
    u32 word2;
    u32 word3;
    u32 word4;
    u32 word5;
} __WLAN_ATTRIB_PACK__;

struct d2h_rxpkt_info {
#ifdef _LITTLE_ENDIAN_
	u32 ver:4;
	u32 pseudo_len:4;
	u32 dummy_len:7;
	u32 dw0_resv1:1;
	u32 header_len:6;
	u32 dw0_resv2:6;
	u32 include_header:1;
	u32 include_rxdesc:1;
	u32 pkt_offload_proc:1;
	u32 rx_error:1;

	u32 pkt_type:3;
	u32 ifid:5;
	u32 macid:8;
	u32 dw1_resv2:16;

	u32 rx_raw_len:16;
	u32 rx_raw_cookie:16;
#else /* _BIG_ENDIAN_ */
	u32 rx_error:1;
	u32 pkt_offload_proc:1;
	u32 include_rxdesc:1;
	u32 include_header:1;
	u32 dw0_resv2:6;
	u32 header_len:6;
	u32 dw0_resv1:1;
	u32 dummy_len:7;
	u32 pseudo_len:4;
	u32 ver:4;

	u32 dw1_resv2:16;
	u32 macid:8;
	u32 ifid:5;
	u32 pkt_type:3;

	u32 rx_raw_cookie:16;
	u32 rx_raw_len:16;
#endif

	u8 frame_ctrl[2]; /* bit-field cannot take address operation */
	u8 dw3_resv[2];
} __WLAN_ATTRIB_PACK__;

#endif


