#
# drivers/net/rtl8192cd
#
# Makefile for the Linux Wireless network device drivers.
#

# HH71VM: block with CPTCFG_* has been removed.
# Was:
#   ifeq ($(CONFIG_OPENWRT_SDK),y)
#   obj-$(CPTCFG_RTL8192CD) +=rtl8192cd.o
#   obj-$(CPTCFG_RTL8192CD) +=rtl8192cd-objs
#   endif
# CPTCFG_* — config prefix backports (compat-wireless). The vendor and the fork have AC10 driver
# is collected INSIDE the mac80211 package, hence the prefix. We collect kernels in a tree, where
# the symbol is called CONFIG_RTL8192CD, and CPTCFG_RTL8192CD is never defined - that is
# the lines turned into meaningless `obj- += ...` and only got confused.
# The second line is also erroneous in itself: `rtl8192cd-objs` is a list of objects
# and not an object, you cannot add it to obj- (see the edit below, the vendor has the same typo there).

ifeq ($(CONFIG_RTL_92C_SUPPORT),y)
RTL_WLAN_DATA_DIR := data
endif
  
ifeq ($(CONFIG_RTL_92D_SUPPORT),y)
RTL_WLAN_DATA_DIR_D := data_92d
endif

ifeq ($(CONFIG_RTL_88E_SUPPORT),y)
  RTL_WLAN_DATA_DIR_E := data_88e
endif

ifeq ($(CONFIG_RTL_8812_SUPPORT),y)
  RTL_WLAN_DATA_DIR_8812 := data_8812
endif

ifeq ($(CONFIG_RTL_8723B_SUPPORT),y)
  RTL_WLAN_DATA_DIR_8723 := data_8723
endif

ifeq ($(CONFIG_WLAN_HAL),y)
	ifeq ($(CONFIG_WLAN_HAL_8814AE),y)
		RTL_WLAN_DATA_DIR_8814 := WlanHAL/Data/8814A
	endif	
	ifeq ($(CONFIG_WLAN_HAL_8192EE),y)
		RTL_WLAN_DATA_DIR_92E := WlanHAL/Data/8192E
	endif
	ifeq ($(CONFIG_WLAN_HAL_8881A),y)
		ifeq ($(CONFIG_MAC_PHY_RF_Parameter_V700),y)
			RTL_WLAN_DATA_DIR_8881A := WlanHAL/Data/8881A/V700
		endif
		ifeq ($(CONFIG_MAC_PHY_RF_Parameter_V702B),y)
			RTL_WLAN_DATA_DIR_8881A := WlanHAL/Data/8881A/V702B
		endif
		ifeq ($(CONFIG_MAC_PHY_RF_Parameter_V702B_Skyworth),y)
			RTL_WLAN_DATA_DIR_8881A := WlanHAL/Data/8881A/V702B_Skyworth
		endif
#		ifeq ($(CONFIG_MAC_PHY_RF_Parameter_V702B_MP),y)
#			RTL_WLAN_DATA_DIR_8881A := WlanHAL/Data/8881A/V702B_MP
#		endif
		RTL_WLAN_DATA_DIR_8812 := WlanHAL/Data/8881A/V702B_MP
	endif
	ifeq ($(CONFIG_WLAN_HAL_8197F),y)
		RTL_WLAN_DATA_DIR_97F := WlanHAL/Data/8197F
	endif
        ifeq ($(CONFIG_WLAN_HAL_8198F),y)
		RTL_WLAN_DATA_DIR_98F := WlanHAL/Data/8198F
	endif	
	ifeq ($(CONFIG_WLAN_HAL_8197G),y)
		RTL_WLAN_DATA_DIR_97G := WlanHAL/Data/8197G
	endif
	ifeq ($(CONFIG_WLAN_HAL_8822BE),y)
		RTL_WLAN_DATA_DIR_8822B := WlanHAL/Data/8822B
	endif		
	ifeq ($(CONFIG_WLAN_HAL_8822CE),y)
		RTL_WLAN_DATA_DIR_8822C := WlanHAL/Data/8822C
	endif
	ifeq ($(CONFIG_WLAN_HAL_8812FE),y)
		RTL_WLAN_DATA_DIR_8812F := WlanHAL/Data/8812F
	endif
	ifeq ($(CONFIG_WLAN_HAL_8821CE),y)
		RTL_WLAN_DATA_DIR_8821C := WlanHAL/Data/8821C
	endif
        ifeq ($(CONFIG_WLAN_HAL_8814BE),y)
		RTL_WLAN_DATA_DIR_8814B := WlanHAL/Data/8814B
		RTL_WLAN_DATA_DIR_PE := pe
	endif
ifeq ($(CONFIG_WLAN_HAL_8192FE),y)
		RTL_WLAN_DATA_DIR_8192F := WlanHAL/Data/8192F
	endif	
endif

ifeq ($(CONFIG_RTL_MESH_SUPPORT),y)
        EXTRA_CFLAGS += -DCONFIG_RTK_MESH #-DMESH_USE_METRICOP
#        ifeq ($(CONFIG_11S_TEST_MODE),y)
#        EXTRA_CFLAGS += -D_11s_TEST_MODE_
#        endif
        obj-mesh = mesh_ext/mesh_proc.o\
        mesh_ext/mesh_route.o\
        mesh_ext/mesh_rx.o\
        mesh_ext/mesh_sme.o\
        mesh_ext/mesh_security.o\
        mesh_ext/mesh_tx.o\
        mesh_ext/mesh_util.o\
        mesh_ext/mesh_11kv.o\
        hash_table.o
endif

SRCS_TXT = $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR)/*.txt))))
SRCS_BIN = $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR)/*.bin))))
SRCS_BIN_U = $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR)/*.BIN))))
SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_D)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_D)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_D)/*.BIN))))
SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_E)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_E)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_E)/*.BIN))))
SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8812)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8812)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8812)/*.BIN))))
SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8723)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8723)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8723)/*.BIN))))

SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_92E)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_92E)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_92E)/*.BIN))))
SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8822B)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8822B)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8822B)/*.BIN))))
SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8822C)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8822C)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8822C)/*.BIN))))
SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8812F)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8812F)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8812F)/*.BIN))))
SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8821C)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8821C)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8821C)/*.BIN))))
SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8814B)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8814B)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8814B)/*.BIN))))
SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_97F)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_97F)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_97F)/*.BIN))))
SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_97G)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_97G)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_97G)/*.BIN))))
SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_98F)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_98F)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_98F)/*.BIN))))
SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8881A)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8881A)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8881A)/*.BIN))))
SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8814)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8814)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8814)/*.BIN))))
SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8192F)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8192F)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8192F)/*.BIN))))
SRCS_TXT += $(addprefix $(src)/data_,$(notdir $(patsubst %.txt,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_PE)/*.txt))))
SRCS_BIN += $(addprefix $(src)/data_,$(notdir $(patsubst %.bin,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_PE)/*.bin))))
SRCS_BIN_U += $(addprefix $(src)/data_,$(notdir $(patsubst %.BIN,%.c,$(wildcard $(src)/$(RTL_WLAN_DATA_DIR_PE)/*.BIN))))

TXPWR_HEADER = 
ifeq ($(CONFIG_RTL_92C_SUPPORT),y)
TXPWR_LMT_TXT_92C = $(addprefix $(src)/data/,$(notdir $(wildcard $(src)/$(RTL_WLAN_DATA_DIR)/TXPWR*.txt)))
TXPWR_LMT_FNAME_92C = $(basename $(notdir $(wildcard $(src)/$(RTL_WLAN_DATA_DIR)/TXPWR*.txt)))
TXPWR_HEADER += $(obj)/TXPWR_92C.h
endif

ifeq ($(CONFIG_RTL_92D_SUPPORT),y)
TXPWR_LMT_TXT_92D = $(addprefix $(src)/data_92d/,$(notdir $(wildcard $(src)/$(RTL_WLAN_DATA_DIR_D)/TXPWR*.txt)))
TXPWR_LMT_FNAME_92D = $(basename $(notdir $(wildcard $(src)/$(RTL_WLAN_DATA_DIR_D)/TXPWR*.txt)))
TXPWR_HEADER += $(obj)/TXPWR_92D.h
endif

ifeq ($(CONFIG_RTL_DOT11K_SUPPORT),y)
	rtl8192cd-objs += \
	dot11k/8192cd_11k.o \
	dot11k/8192cd_11k_neighbor.o\
	dot11k/8192cd_11k_link.o\
	dot11k/8192cd_11k_beacon.o
endif

ifeq ($(CONFIG_RTL_11V_SUPPORT),y)
	rtl8192cd-objs += 8192cd_11v.o
endif

ifeq ($(CONFIG_RTL_11R_SUPPORT),y)
	rtl8192cd-objs += 8192cd_11r_cli.o
endif

ifeq ($(CONFIG_RTL_STA_CONTROL_SUPPORT),y)
	rtl8192cd-objs += 8192cd_sta_control.o
endif

ifeq ($(CONFIG_RTK_MULTI_AP),y)
	rtl8192cd-objs += 8192cd_multi_ap.o
else
ifeq ($(CONFIG_RTK_SMART_ROAMING),y)
	rtl8192cd-objs += 8192cd_smart_roaming.o
endif
endif

ifeq ($(CONFIG_RTL_WAPI_SUPPORT),y)
	rtl8192cd-objs += wapi_wai.o wapiCrypto.o wapiRandom.o
endif

# HH71VM: was
#   ifeq ($(CONFIG_OPENWRT_SDK),y)
#   obj-$(CONFIG_RTL8192CD) += rtl8192cd-objs
#   else
#   obj-$(CONFIG_RTL8192CD) += rtl8192cd.o
#   endif
# The OPENWRT_SDK branch is incorrect: in obj- they put an object (`rtl8192cd.o`), and `rtl8192cd-objs` is
# VARIABLE with a list of its constituent parts. Kbuild would try to build a non-existent
# `rtl8192cd-objs.o`. This did not come up with the vendor, because when assembling via backports
# The line with CPTCFG_ at the beginning of the file worked, but this one did not.
obj-$(CONFIG_RTL8192CD) += rtl8192cd.o

ifeq ($(CONFIG_RTL_88E_SUPPORT),y)
	rtl8192cd-objs += HalPwrSeqCmd.o\
	Hal8188EPwrSeq.o\
	8188e_hw.o 
	ifeq ($(CONFIG_RTL_ODM_WLAN_DRIVER),y)
		#rtl8192cd-objs += \
        #phydm/rtl8188e/halhwimg8188e_bb.o\
        #phydm/rtl8188e/halhwimg8188e_mac.o\
        #phydm/rtl8188e/halhwimg8188e_rf.o\
        #phydm/rtl8188e/phydm_regconfig8188e.o\
        #phydm/rtl8188e/hal8188erateadaptive.o\
        #phydm/rtl8188e/phydm_rtl8188e.o\
        #phydm/rtl8188e/halphyrf_8188e_ap.o			
	else
		rtl8192cd-objs += Hal8192CDMOutSrc.o \
		rtl8192cd-objs += RateAdaptive.o 
	endif
endif

ifeq ($(CONFIG_RTL_8723B_SUPPORT),y)				
	rtl8192cd-objs += HalPwrSeqCmd.o\
	Hal8723BPwrSeq.o\
	8812_hw.o\
	phydm/rtl8723b/phydm_rtl8723b.o\
	phydm/rtl8723b/halhwimg8723b_bb.o\
	phydm/rtl8723b/halhwimg8723b_fw.o\
	phydm/rtl8723b/halhwimg8723b_mac.o\
	phydm/rtl8723b/halhwimg8723b_mp.o\
	phydm/rtl8723b/halhwimg8723b_rf.o\
	phydm/rtl8723b/phydm_regconfig8723b.o\
	phydm/rtl8723b/phydm_rtl8723b.o\
	phydm/rtl8723b/halphyrf_8723b_ap.o
endif

ifeq ($(CONFIG_RTL_8812_SUPPORT),y)
	rtl8192cd-objs += HalPwrSeqCmd.o\
	Hal8812PwrSeq.o\
	8812_hw.o
endif
ifeq ($(CONFIG_RTL_92C_SUPPORT),y) 
	rtl8192cd-objs += Hal8192CDMOutSrc.o
endif
ifeq ($(CONFIG_RTL_92D_SUPPORT),y)
        rtl8192cd-objs += 8192d_hw.o Hal8192CDMOutSrc.o
endif

EXTRA_CFLAGS += -DDM_ODM_SUPPORT_TYPE=0x01
ifeq ($(CONFIG_RTL_ODM_WLAN_DRIVER),y)
	include $(src)/phydm/ap_makefile.mk
	EXTRA_CFLAGS += -I$(src) -I$(src)/phydm
endif


ifeq ($(CONFIG_RTL_OFFLOAD_DRIVER),y)
	rtl8192cd-objs += \
		core/8192cd_core_rx_d2h.o \
		core/8192cd_core_tx_h2d.o 
endif										
ifeq ($(CONFIG_RTL_WPA3_SUPPORT),y)
obj-wpa3 =  wpa3/8192cd_sae.o  \
		wpa3/src_hapd/crypto/aes-ccm.o  \
		wpa3/src_hapd/crypto/aes-gcm.o  \
		wpa3/src_hapd/crypto/aes-internal.o \
		wpa3/src_hapd/crypto/aes-internal-dec.o \
		wpa3/src_hapd/crypto/aes-internal-enc.o \
		wpa3/src_mbedtls/lk.o\
		wpa3/src_mbedtls/mbedtls/library/aes.o\
		wpa3/src_mbedtls/mbedtls/library/aesni.o\
		wpa3/src_mbedtls/mbedtls/library/arc4.o\
		wpa3/src_mbedtls/mbedtls/library/asn1parse.o\
		wpa3/src_mbedtls/mbedtls/library/asn1write.o\
		wpa3/src_mbedtls/mbedtls/library/base64.o\
		wpa3/src_mbedtls/mbedtls/library/bignum.o\
		wpa3/src_mbedtls/mbedtls/library/blowfish.o\
		wpa3/src_mbedtls/mbedtls/library/camellia.o\
		wpa3/src_mbedtls/mbedtls/library/ccm.o\
		wpa3/src_mbedtls/mbedtls/library/certs.o\
		wpa3/src_mbedtls/mbedtls/library/cipher.o\
		wpa3/src_mbedtls/mbedtls/library/cipher_wrap.o\
		wpa3/src_mbedtls/mbedtls/library/ctr_drbg.o\
		wpa3/src_mbedtls/mbedtls/library/debug.o\
		wpa3/src_mbedtls/mbedtls/library/des.o\
		wpa3/src_mbedtls/mbedtls/library/dhm.o\
		wpa3/src_mbedtls/mbedtls/library/ecp.o\
		wpa3/src_mbedtls/mbedtls/library/ecp_curves.o\
		wpa3/src_mbedtls/mbedtls/library/ecdh.o\
		wpa3/src_mbedtls/mbedtls/library/ecdsa.o\
		wpa3/src_mbedtls/mbedtls/library/entropy.o\
		wpa3/src_mbedtls/mbedtls/library/entropy_poll.o\
		wpa3/src_mbedtls/mbedtls/library/error.o\
		wpa3/src_mbedtls/mbedtls/library/gcm.o\
		wpa3/src_mbedtls/mbedtls/library/havege.o\
		wpa3/src_mbedtls/mbedtls/library/hmac_drbg.o\
		wpa3/src_mbedtls/mbedtls/library/md.o\
		wpa3/src_mbedtls/mbedtls/library/md_wrap.o\
		wpa3/src_mbedtls/mbedtls/library/md2.o\
		wpa3/src_mbedtls/mbedtls/library/md4.o\
		wpa3/src_mbedtls/mbedtls/library/md5.o\
		wpa3/src_mbedtls/mbedtls/library/memory_buffer_alloc.o\
		wpa3/src_mbedtls/mbedtls/library/net_sockets.o\
		wpa3/src_mbedtls/mbedtls/library/oid.o\
		wpa3/src_mbedtls/mbedtls/library/padlock.o\
		wpa3/src_mbedtls/mbedtls/library/pem.o\
		wpa3/src_mbedtls/mbedtls/library/pkcs5.o\
		wpa3/src_mbedtls/mbedtls/library/pkcs11.o\
		wpa3/src_mbedtls/mbedtls/library/pkcs12.o\
		wpa3/src_mbedtls/mbedtls/library/pk.o\
		wpa3/src_mbedtls/mbedtls/library/pk_wrap.o\
		wpa3/src_mbedtls/mbedtls/library/pkparse.o\
		wpa3/src_mbedtls/mbedtls/library/pkwrite.o\
		wpa3/src_mbedtls/mbedtls/library/platform.o\
		wpa3/src_mbedtls/mbedtls/library/ripemd160.o\
		wpa3/src_mbedtls/mbedtls/library/rsa.o\
		wpa3/src_mbedtls/mbedtls/library/sha1.o\
		wpa3/src_mbedtls/mbedtls/library/sha256.o\
		wpa3/src_mbedtls/mbedtls/library/sha512.o\
		wpa3/src_mbedtls/mbedtls/library/ssl_cache.o\
		wpa3/src_mbedtls/mbedtls/library/ssl_ciphersuites.o\
		wpa3/src_mbedtls/mbedtls/library/ssl_cli.o\
		wpa3/src_mbedtls/mbedtls/library/ssl_srv.o\
		wpa3/src_mbedtls/mbedtls/library/ssl_tls.o\
		wpa3/src_mbedtls/mbedtls/library/threading.o\
		wpa3/src_mbedtls/mbedtls/library/timing.o\
		wpa3/src_mbedtls/mbedtls/library/version.o\
		wpa3/src_mbedtls/mbedtls/library/version_features.o\
		wpa3/src_mbedtls/mbedtls/library/x509.o\
		wpa3/src_mbedtls/mbedtls/library/x509_crt.o\
		wpa3/src_mbedtls/mbedtls/library/x509_crl.o\
		wpa3/src_mbedtls/mbedtls/library/x509_csr.o\
		wpa3/src_mbedtls/mbedtls/library/x509_create.o\
		wpa3/src_mbedtls/mbedtls/library/x509write_crt.o\
		wpa3/src_mbedtls/mbedtls/library/x509write_csr.o\
		wpa3/src_mbedtls/mbedtls/library/xtea.o
ifeq ($(CONFIG_RTL_MESH_SUPPORT),y)
else
    obj-wpa3 += hash_table.o
endif
endif
rtl8192cd-objs +=\
	core/core_osdep.o\
	core/core_hw.o\
	core/core_sme.o\
	core/8192cd_core_tx.o\
	core/8192cd_core_rx.o\
	core/8192cd_core_util.o\
	8192cd_tx.o\
	8192cd_rx.o\
	8192cd_osdep.o\
	8192cd_sme.o\
	8192cd_util.o\
	8192cd_hw.o\
	8192cd_security.o\
	8192cd_tkip.o\
	8192cd_aes.o\
	8192cd_debug.o\
	8192cd_proc.o\
	8192cd_br_ext.o\
	8192cd_eeprom.o\
	8192cd_mp.o\
	8192cd_psk.o\
	8192cd_ioctl.o\
	1x_kmsm_aes.o\
	1x_kmsm_hmac.o\
	1x_md5c.o\
	1x_rc4.o\
	8192cd_mib.o\
	8192cd_dmem.o\
	8192cd_led.o\
	8192cd_dfs.o\
	8192cd_dfs_det.o\
	8812_vht_gen.o\
	romeperf.o\
	HalDMOutSrc.o\
	Beamforming.o\
	8192cd_11h.o\
	$(obj-wpa3)\
	$(obj-mesh)

ifeq ($(CONFIG_ARCH_CORTINA_G3),y)
	EXTRA_CFLAGS += -DNOT_RTK_BSP
else ifeq ($(CONFIG_ARCH_CORTINA_G3HGU),y)
	EXTRA_CFLAGS += -DNOT_RTK_BSP
else
	rtl8192cd-objs += 8192cd_host.o
endif

ifeq ($(CONFIG_RG_WLAN_HWNAT_ACCELERATION),y)
	rtl8192cd-objs += rtk_rg_wlan.o
endif

rtl8192cd-objs += $(_PHYDM_FILES)
	
ifeq ($(CONFIG_RTL_A4_STA_SUPPORT),y)
rtl8192cd-objs += 8192cd_a4_sta.o
endif	

ifeq ($(CONFIG_SUPPORT_ROAMING),y)
rtl8192cd-objs +=\
	roaming/events.o \
	roaming/kernel_netlink_util.o \
	roaming/realtek_adapter.o
EXTRA_CFLAGS += -DSUPPORT_ROAMING -I$(src)
endif

ifeq ($(CONFIG_RTL_11W_SUPPORT),y)
	rtl8192cd-objs += sha256.o
else ifeq ($(CONFIG_RTL_11R_SUPPORT),y)
	rtl8192cd-objs += sha256.o
else ifeq ($(CONFIG_RTL_WAPI_SUPPORT),y)
	rtl8192cd-objs += sha256.o
endif

ifeq ($(CONFIG_RTL_SIMPLE_CONFIG),y)
	rtl8192cd-objs += 8192cd_profile.o
endif	
ifeq ($(CONFIG_RTL_COMAPI_CFGFILE),y)
	rtl8192cd-objs += 8192cd_comapi.o
endif
ifeq ($(CONFIG_RTL_P2P_SUPPORT),y)
ifeq ($(CONFIG_OPENWRT_SDK),y)
	rtl8192cd-objs += 8192cd_p2p.o
else
	rtl8192cd-objs += 8192cd_p2p.o
endif
endif
ifeq ($(CONFIG_RTL_COMAPI_WLTOOLS),y)
	rtl8192cd-objs += 8192cd_comapi.o
endif
ifeq ($(CONFIG_PACP_SUPPORT),y)
	rtl8192cd-objs += 8192cd_comapi.o
endif	

#For Hostapd
ifeq ($(CONFIG_RTL_HOSTAPD_SUPPORT),y)
	rtl8192cd-objs += 8192cd_comapi.o
endif
#ifeq ($(CONFIG_RTL8672),y)
	rtl8192cd-objs += rtl8672_port.o
#endif

ifeq ($(CONFIG_OPENWRT_SDK),y)
	rtl8192cd-objs += 8192cd_net80211.o
	rtl8192cd-objs += 8192cd_psk_hapd.o
#	rtl8192cd-objs += 8192cd_cfg80211.o
# HH71VM: 8192cd_cfg80211.o is disabled. The file definitely includes <net/cfg80211.h> and
# <net/mac80211.h>, but they are unavailable here: OpenWrt disables built-in CFG80211/MAC80211
# and delivers them in package kmod-mac80211 (backports), which is not selected in profile hh71vm
# (`# CONFIG_PACKAGE_kmod-mac80211 is not set`).
# Disabling is safe and does not “cut off” functionality: the entire nl80211 driver path is closed
# macro RTK_NL80211, and that is defined in 8192cd_cfg.h:920 only when
# `#if defined(CPTCFG_CFG80211_MODULE)`, that is, exclusively in the assembly through backports.
# Neighboring 8192cd_net80211.c and 8192cd_psk_hapd.c are left: their contents are completely covered
# `#if defined(WIFI_HAPD) || defined(RTK_NL80211)` even without these macros gives an empty object.
# REVIEWED AND REJECTED: Enable built-in CONFIG_CFG80211=y. This draws everything into the core
# net/wireless and conflicts with package mac80211 if it is ever needed; for the sake of
# file, which under our macros is still empty, is disproportionate.
ifeq ($(CONFIG_RTL_P2P_SUPPORT),y)
	rtl8192cd-objs += rtw_android.o
endif
ifeq ($(CONFIG_PACP_SUPPORT),y)
	rtl8192cd-objs += 8192cd_comapi.o
endif
endif

ifeq ($(CONFIG_WLAN_HAL),y)
	EXTRA_CFLAGS += -I$(src) -I$(src)/WlanHAL/ -I$(src)/WlanHAL/Include -I$(src)/WlanHAL/HalHeader -I$(src)/core/WlanHAL
	rtl8192cd-objs += WlanHAL/HalCommon.o \
										WlanHAL/HalCfg.o \
										WlanHAL/HalDbgCmd.o	\
										core/WlanHAL/HalMacFunc.o \
										WlanHAL/HalMacAPI.o 
										
endif

ifeq ($(CONFIG_WLAN_HAL_88XX),y)
	EXTRA_CFLAGS += -I$(src)/WlanHAL/RTL88XX -I$(src)/WlanHAL/HalMac88XX -I$(src)/core/WlanHAL/RTL88XX
	rtl8192cd-objs += WlanHAL/RTL88XX/Hal88XXFirmware.o \
										WlanHAL/RTL88XX/Hal88XXGen.o \
										WlanHAL/RTL88XX/Hal88XXHWImg.o \
										WlanHAL/RTL88XX/Hal88XXIsr.o	\
										WlanHAL/RTL88XX/Hal88XXPwrSeqCmd.o \
										WlanHAL/RTL88XX/Hal88XXRxDesc.o \
										WlanHAL/RTL88XX/Hal88XXTxDesc.o \
										WlanHAL/RTL88XX/Hal88XXVerify.o \
										WlanHAL/RTL88XX/Hal88XXPhyCfg.o \
										WlanHAL/RTL88XX/Hal88XXDM.o																
endif

ifeq ($(CONFIG_WLAN_HAL_8881A),y)
	EXTRA_CFLAGS += -I$(src)/WlanHAL/RTL88XX/RTL8881A
	rtl8192cd-objs += WlanHAL/RTL88XX/RTL8881A/Hal8881AFirmware.o \
										WlanHAL/RTL88XX/RTL8881A/Hal8881AGen.o \
										WlanHAL/RTL88XX/RTL8881A/Hal8881AHWImg.o \
										WlanHAL/RTL88XX/RTL8881A/Hal8881AIsr.o \
										WlanHAL/RTL88XX/RTL8881A/Hal8881APwrSeqCmd.o \
										WlanHAL/RTL88XX/RTL8881A/Hal8881ARxDesc.o \
										WlanHAL/RTL88XX/RTL8881A/Hal8881ATxDesc.o \
										WlanHAL/RTL88XX/RTL8881A/Hal8881AVerify.o \
										WlanHAL/RTL88XX/RTL8881A/Hal8881APhyCfg.o \
										WlanHAL/RTL88XX/RTL8881A/RTL8881AE/Hal8881AEGen.o \
										8812_hw.o									
endif

ifeq ($(CONFIG_WLAN_HAL_8192EE),y)
## CONFIG_RTL_TRIBAND_SUPPORT ##
ifeq ($(CONFIG_RTL_92E_SUPPORT),y)
	ifeq ($(CONFIG_USB_HCI),y)
		EXTRA_CFLAGS += -I$(src)/WlanHAL/RTL88XX/RTL8192E/RTL8192EU -I$(src)/WlanHAL/RTL88XX/RTL8192E
	else
	ifeq ($(CONFIG_PCI_HCI), y)
		EXTRA_CFLAGS += -I$(src)/WlanHAL/RTL88XX/RTL8192E/RTL8192EE -I$(src)/WlanHAL/RTL88XX/RTL8192E
	endif
	endif
		rtl8192cd-objs += WlanHAL/RTL88XX/RTL8192E/Hal8192EGen.o \
				  WlanHAL/RTL88XX/RTL8192E/Hal8192EPhyCfg.o \
				  WlanHAL/RTL88XX/RTL8192E/Hal8192EPwrSeqCmd.o	

	ifeq ($(CONFIG_USB_HCI),y)
		rtl8192cd-objs += WlanHAL/RTL88XX/RTL8192E/RTL8192EU/Hal8192EUGen.o

		rtl8192cd-objs += \
			usb/8192eu/8192cd_usb.o \
			usb/8192eu/8192cd_usb_hw.o \
			usb/8192eu/8192cd_usb_xmit.o \
			usb/8192eu/8192cd_usb_recv.o \
			usb/8192eu/8192cd_usb_cmd.o \
			hal_intf_xmit.o	
	else
	ifeq ($(CONFIG_PCI_HCI), y)
		rtl8192cd-objs += WlanHAL/RTL88XX/RTL8192E/RTL8192EE/Hal8192EEGen.o
	endif
	endif

	ifeq ($(CONFIG_USB_HCI_OLD),y)
		ifeq ($(CONFIG_RTL_ODM_WLAN_DRIVER),y)
		rtl8192cd-objs += \
			phydm/rtl8192e/halhwimg8192e_bb.o\
			phydm/rtl8192e/halhwimg8192e_mac.o\
			phydm/rtl8192e/halhwimg8192e_rf.o\
			phydm/rtl8192e/phydm_regconfig8192e.o\
			phydm/rtl8192e/halhwimg8192e_fw.o
		endif
	endif
else
		EXTRA_CFLAGS += -I$(src)/WlanHAL/RTL88XX/RTL8192E/RTL8192EE -I$(src)/WlanHAL/RTL88XX/RTL8192E
		rtl8192cd-objs += WlanHAL/RTL88XX/RTL8192E/RTL8192EE/Hal8192EEGen.o \
				  WlanHAL/RTL88XX/RTL8192E/Hal8192EGen.o \
				  WlanHAL/RTL88XX/RTL8192E/Hal8192EPhyCfg.o \
				  WlanHAL/RTL88XX/RTL8192E/Hal8192EPwrSeqCmd.o	
endif
endif

ifeq ($(CONFIG_WLAN_HAL_8197F),y)
	EXTRA_CFLAGS += -I$(src)/WlanHAL/RTL88XX/RTL8197F -I$(src)/efuse_97f
	rtl8192cd-objs += WlanHAL/RTL88XX/RTL8197F/Hal8197FFirmware.o \
										WlanHAL/RTL88XX/RTL8197F/Hal8197FGen.o \
										WlanHAL/RTL88XX/RTL8197F/Hal8197FHWImg.o \
										WlanHAL/RTL88XX/RTL8197F/Hal8197FIsr.o \
										WlanHAL/RTL88XX/RTL8197F/Hal8197FPwrSeqCmd.o \
										WlanHAL/RTL88XX/RTL8197F/Hal8197FRxDesc.o \
										WlanHAL/RTL88XX/RTL8197F/Hal8197FTxDesc.o \
										WlanHAL/RTL88XX/RTL8197F/Hal8197FVerify.o \
										WlanHAL/RTL88XX/RTL8197F/Hal8197FPhyCfg.o \
										WlanHAL/RTL88XX/RTL8197F/RTL8197FE/Hal8197FEGen.o 					
endif

ifeq ($(CONFIG_WLAN_HAL_8198F),y)
	EXTRA_CFLAGS += -I$(src)/WlanHAL/RTL88XX/RTL8198F 
	rtl8192cd-objs += WlanHAL/RTL88XX/RTL8198F/Hal8198FFirmware.o \
										WlanHAL/RTL88XX/RTL8198F/Hal8198FGen.o \
										WlanHAL/RTL88XX/RTL8198F/Hal8198FHWImg.o \
										WlanHAL/RTL88XX/RTL8198F/Hal8198FIsr.o \
										WlanHAL/RTL88XX/RTL8198F/Hal8198FPwrSeqCmd.o \
										WlanHAL/RTL88XX/RTL8198F/Hal8198FRxDesc.o \
										WlanHAL/RTL88XX/RTL8198F/Hal8198FTxDesc.o \
										WlanHAL/RTL88XX/RTL8198F/Hal8198FVerify.o \
										WlanHAL/RTL88XX/RTL8198F/RTL8198FE/Hal8198FEGen.o \
										WlanHAL/RTL88XX/RTL8198F/Hal8198FPhyCfg.o 
endif

ifeq ($(CONFIG_WLAN_HAL_8197G),y)
	EXTRA_CFLAGS += -I$(src)/efuse_97f
	rtl8192cd-objs += efuse_97f/efuse.o
	EXTRA_CFLAGS += -I$(src)/WlanHAL/RTL88XX/RTL8197G
	rtl8192cd-objs += WlanHAL/RTL88XX/RTL8197G/Hal8197GFirmware.o \
										WlanHAL/RTL88XX/RTL8197G/Hal8197GGen.o \
										WlanHAL/RTL88XX/RTL8197G/Hal8197GHWImg.o \
										WlanHAL/RTL88XX/RTL8197G/Hal8197GIsr.o \
										WlanHAL/RTL88XX/RTL8197G/Hal8197GPwrSeqCmd.o \
										WlanHAL/RTL88XX/RTL8197G/Hal8197GRxDesc.o \
										WlanHAL/RTL88XX/RTL8197G/Hal8197GTxDesc.o \
										WlanHAL/RTL88XX/RTL8197G/Hal8197GVerify.o \
										WlanHAL/RTL88XX/RTL8197G/Hal8197GPhyCfg.o \
										WlanHAL/RTL88XX/RTL8197G/RTL8197GE/Hal8197GEGen.o 					
endif

ifeq ($(CONFIG_WLAN_HAL_8814AE),y)
	EXTRA_CFLAGS += -I$(src)/WlanHAL/RTL88XX/RTL8814A/RTL8814AE -I$(src)/WlanHAL/RTL88XX/RTL8814A
	rtl8192cd-objs += WlanHAL/RTL88XX/RTL8814A/RTL8814AE/Hal8814AEGen.o \
	        WlanHAL/RTL88XX/RTL8814A/Hal8814AGen.o \
			WlanHAL/RTL88XX/RTL8814A/Hal8814APhyCfg.o \
			WlanHAL/RTL88XX/RTL8814A/Hal8814APwrSeqCmd.o \
			8812_hw.o					  
endif

ifeq ($(CONFIG_WLAN_HAL_8822BE),y)
	EXTRA_CFLAGS += -I$(src)/WlanHAL/RTL88XX/RTL8822B
	rtl8192cd-objs += WlanHAL/RTL88XX/RTL8822B/Hal8822BFirmware.o \
										WlanHAL/RTL88XX/RTL8822B/RTL8822BE/Hal8822BEGen.o \
										WlanHAL/RTL88XX/RTL8822B/Hal8822BGen.o \
										WlanHAL/RTL88XX/RTL8822B/Hal8822BPwrSeqCmd.o \
										WlanHAL/RTL88XX/RTL8822B/Hal8822BPhyCfg.o \
										8812_hw.o
endif

ifeq ($(CONFIG_WLAN_HAL_8822CE),y)
	EXTRA_CFLAGS += -I$(src)/WlanHAL/RTL88XX/RTL8822C
	rtl8192cd-objs += WlanHAL/RTL88XX/RTL8822C/Hal8822CFirmware.o \
										WlanHAL/RTL88XX/RTL8822C/RTL8822CE/Hal8822CEGen.o \
										WlanHAL/RTL88XX/RTL8822C/Hal8822CGen.o \
										WlanHAL/RTL88XX/RTL8822C/Hal8822CPwrSeqCmd.o \
										WlanHAL/RTL88XX/RTL8822C/Hal8822CPhyCfg.o \
										8812_hw.o
endif

ifeq ($(CONFIG_WLAN_HAL_8812FE),y)
	EXTRA_CFLAGS += -I$(src)/WlanHAL/RTL88XX/RTL8812F
	rtl8192cd-objs += WlanHAL/RTL88XX/RTL8812F/Hal8812FFirmware.o \
										WlanHAL/RTL88XX/RTL8812F/RTL8812FE/Hal8812FEGen.o \
										WlanHAL/RTL88XX/RTL8812F/Hal8812FGen.o \
										WlanHAL/RTL88XX/RTL8812F/Hal8812FPwrSeqCmd.o \
										WlanHAL/RTL88XX/RTL8812F/Hal8812FPhyCfg.o \
										8812_hw.o
endif

ifeq ($(CONFIG_WLAN_HAL_8814BE),y)
	EXTRA_CFLAGS += -I$(src)/WlanHAL/RTL88XX/RTL8814B -I$(src)/core/WlanHAL/RTL88XX/RTL8814B
	rtl8192cd-objs +=  \
		WlanHAL/RTL88XX/RTL8814B/Hal8814BGen.o \
		WlanHAL/RTL88XX/RTL8814B/Hal8814BPwrSeqCmd.o \
		WlanHAL/RTL88XX/RTL8814B/RTL8814BE/Hal8814BEGen.o \
		WlanHAL/RTL88XX/RTL8814B/Hal8814BPhyCfg.o 								
ifeq ($(CONFIG_RTL_OFFLOAD_DRIVER),y)
	EXTRA_CFLAGS += -I$(src)/core
	rtl8192cd-objs += \
		8192cd_rx_d2h.o \
		8192cd_tx_h2d.o \
		core/WlanHAL/RTL88XX/RTL8814B/Hal8814HTxDesc_core.o \
		WlanHAL/RTL88XX/RTL8814B/Hal8814BFirmware.o \
		core/WlanHAL/RTL88XX/RTL8814B/Hal8814HRxDesc_core.o \
		core/WlanHAL/RTL88XX/RTL8814B/Hal8814BFirmware_core.o \
		WlanHAL/RTL88XX/RTL8814B/Hal8814HTxDesc.o \
		WlanHAL/RTL88XX/RTL8814B/Hal8814HRxDesc.o 
endif					
ifeq ($(CONFIG_RTL_TAROKO_OFFLOAD),y)
	EXTRA_CFLAGS += -I$(srctree)/include/soc/cortina/
	EXTRA_CFLAGS += -I$(src)/pe/
	rtl8192cd-objs += \
		pe/pe_ipc.o \
		pe/pe_fw.o 
endif					
endif

ifeq ($(CONFIG_WLAN_MACHAL_API),y)
	EXTRA_CFLAGS += -I$(src)/WlanHAL/HalMac88XX 
	rtl8192cd-objs += WlanHAL/HalMac88XX/halmac_api.o 

ifeq ($(CONFIG_WLAN_MACHAL_API_V0),y)
#for halmac_88xx common
	rtl8192cd-objs += WlanHAL/HalMac88XX/halmac_88xx/halmac_init_88xx.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_pcie_88xx.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_sdio_88xx.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_usb_88xx.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_common_88xx.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_gpio_88xx.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_cfg_wmac_88xx.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_fw_88xx.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_mimo_88xx.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_bb_rf_88xx.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_efuse_88xx.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_flash_88xx.o
endif

ifeq ($(CONFIG_WLAN_MACHAL_API_V1),y)
#for halmac_88xx_v1 common
	rtl8192cd-objs += WlanHAL/HalMac88XX/halmac_88xx_v1/halmac_init_88xx_v1.o \
						WlanHAL/HalMac88XX/halmac_88xx_v1/halmac_pcie_88xx_v1.o \
						WlanHAL/HalMac88XX/halmac_88xx_v1/halmac_sdio_88xx_v1.o \
						WlanHAL/HalMac88XX/halmac_88xx_v1/halmac_usb_88xx_v1.o \
						WlanHAL/HalMac88XX/halmac_88xx_v1/halmac_common_88xx_v1.o \
						WlanHAL/HalMac88XX/halmac_88xx_v1/halmac_gpio_88xx_v1.o \
						WlanHAL/HalMac88XX/halmac_88xx_v1/halmac_cfg_wmac_88xx_v1.o \
						WlanHAL/HalMac88XX/halmac_88xx_v1/halmac_fw_88xx_v1.o \
						WlanHAL/HalMac88XX/halmac_88xx_v1/halmac_mimo_88xx_v1.o \
						WlanHAL/HalMac88XX/halmac_88xx_v1/halmac_bb_rf_88xx_v1.o \
						WlanHAL/HalMac88XX/halmac_88xx_v1/halmac_efuse_88xx_v1.o \
						WlanHAL/HalMac88XX/halmac_88xx_v1/halmac_flash_88xx_v1.o
endif

ifeq ($(CONFIG_WLAN_HAL_8821CE),y)
	EXTRA_CFLAGS += -I$(src)/WlanHAL/RTL88XX/RTL8821C
	rtl8192cd-objs += WlanHAL/RTL88XX/RTL8821C/Hal8821CFirmware.o \
									WlanHAL/RTL88XX/RTL8821C/Hal8821CPhyCfg.o \
										WlanHAL/RTL88XX/RTL8821C/Hal8821CPwrSeqCmd.o \
										WlanHAL/RTL88XX/RTL8821C/Hal8821CGen.o \
										8812_hw.o
	ifeq ($(CONFIG_PCI_HCI), y)
		rtl8192cd-objs += WlanHAL/RTL88XX/RTL8821C/RTL8821CE/Hal8821CEGen.o
	endif
										
endif

ifeq ($(CONFIG_WLAN_HAL_8822BE),y)
	rtl8192cd-objs += WlanHAL/HalMac88XX/halmac_88xx/halmac_8822b/halmac_pwr_seq_8822b.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8822b/halmac_init_8822b.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8822b/halmac_pcie_8822b.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8822b/halmac_sdio_8822b.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8822b/halmac_usb_8822b.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8822b/halmac_phy_8822b.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8822b/halmac_gpio_8822b.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8822b/halmac_common_8822b.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8822b/halmac_cfg_wmac_8822b.o 
endif

ifeq ($(CONFIG_WLAN_HAL_8822CE),y)
	rtl8192cd-objs += WlanHAL/HalMac88XX/halmac_88xx/halmac_8822c/halmac_pwr_seq_8822c.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8822c/halmac_init_8822c.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8822c/halmac_pcie_8822c.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8822c/halmac_sdio_8822c.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8822c/halmac_usb_8822c.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8822c/halmac_phy_8822c.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8822c/halmac_gpio_8822c.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8822c/halmac_common_8822c.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8822c/halmac_cfg_wmac_8822c.o 
endif

ifeq ($(CONFIG_WLAN_HAL_8812FE),y)
	rtl8192cd-objs += WlanHAL/HalMac88XX/halmac_88xx/halmac_8812f/halmac_pwr_seq_8812f.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8812f/halmac_init_8812f.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8812f/halmac_pcie_8812f.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8812f/halmac_sdio_8812f.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8812f/halmac_usb_8812f.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8812f/halmac_phy_8812f.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8812f/halmac_gpio_8812f.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8812f/halmac_common_8812f.o \
						WlanHAL/HalMac88XX/halmac_88xx/halmac_8812f/halmac_cfg_wmac_8812f.o 
endif

ifeq ($(CONFIG_WLAN_HAL_8814BE),y)
	rtl8192cd-objs += WlanHAL/HalMac88XX/halmac_88xx_v1/halmac_8814b/halmac_pwr_seq_8814b.o \
						WlanHAL/HalMac88XX/halmac_88xx_v1/halmac_8814b/halmac_init_8814b.o \
						WlanHAL/HalMac88XX/halmac_88xx_v1/halmac_8814b/halmac_pcie_8814b.o \
						WlanHAL/HalMac88XX/halmac_88xx_v1/halmac_8814b/halmac_sdio_8814b.o \
						WlanHAL/HalMac88XX/halmac_88xx_v1/halmac_8814b/halmac_usb_8814b.o \
						WlanHAL/HalMac88XX/halmac_88xx_v1/halmac_8814b/halmac_phy_8814b.o \
						WlanHAL/HalMac88XX/halmac_88xx_v1/halmac_8814b/halmac_gpio_8814b.o 
endif


endif

ifeq ($(CONFIG_WLAN_HAL_8192FE),y)
	## CONFIG_RTL_TRIBAND_SUPPORT ##
	ifeq ($(CONFIG_RTL_92F_SUPPORT),y)
	        EXTRA_CFLAGS += -I$(src)/WlanHAL/RTL88XX/RTL8192F/RTL8192FU -I$(src)/WlanHAL/RTL88XX/RTL8192F
	        rtl8192cd-objs += WlanHAL/RTL88XX/RTL8192F/RTL8192FU/Hal8192FUGen.o \
	                  WlanHAL/RTL88XX/RTL8192F/Hal8192FGen.o \
	                  WlanHAL/RTL88XX/RTL8192F/Hal8192FPhyCfg.o \
	                  WlanHAL/RTL88XX/RTL8192F/Hal8192FPwrSeqCmd.o  

		rtl8192cd-objs += \
			usb/8192fu/8192cd_usb.o \
			usb/8192fu/8192cd_usb_hw.o \
			usb/8192fu/8192cd_usb_xmit.o \
			usb/8192fu/8192cd_usb_recv.o \
			usb/8192fu/8192cd_usb_cmd.o \
			hal_intf_xmit.o	

	else
		EXTRA_CFLAGS += -I$(src)/WlanHAL/RTL88XX/RTL8192F/RTL8192FE -I$(src)/WlanHAL/RTL88XX/RTL8192F
		rtl8192cd-objs += WlanHAL/RTL88XX/RTL8192F/RTL8192FE/Hal8192FEGen.o \
				  WlanHAL/RTL88XX/RTL8192F/Hal8192FGen.o \
				  WlanHAL/RTL88XX/RTL8192F/Hal8192FPhyCfg.o \
				  WlanHAL/RTL88XX/RTL8192F/Hal8192FPwrSeqCmd.o	
	endif
endif

ifeq ($(CONFIG_RTL8672),y)
	ifeq ($(CONFIG_OPENWRT_SDK),y)
		ccflags-y += -DCONFIG_RTL8196B -DCONFIG_RTL8196C -DCONFIG_RTL8196B_GW -DCONFIG_RTL8196C_TESTCHIP_PATCH
	else
		EXTRA_CFLAGS += -DCONFIG_RTL8196B -DCONFIG_RTL8196C -DCONFIG_RTL8196B_GW -DCONFIG_RTL8196C_TESTCHIP_PATCH
		ifeq ($(CONFIG_DEFAULTS_KERNEL_2_6),y)
			EXTRA_CFLAGS += -DCONFIG_COMPAT_NET_DEV_OPS -D_MP_TELNET_SUPPORT_
		endif
	endif
endif

#CONFIG_SINUX_SUPPORT=1
ifeq ($(CONFIG_SINUX_SUPPORT),1)
EXTRA_CFLAGS  += -DOPENSSL_FIPS -D__linux__ -DRSDK_BUILT -DOPENSSL_NO_SPEED -DOPENSSL_THREADS -D_REENTRANT \
                        -DDSO_DLFCN -DHAVE_DLFCN_H -DOPENSSL_NO_KRB5 -DB_ENDIAN -DTERMIO \
			-fomit-frame-pointer 
# -save-temps                      
                        
obj-y  += wps/ssl/mem.o wps/ssl/mem_clr.o wps/ssl/bn_add.o wps/ssl/bn_lib.o \
			wps/ssl/bn_asm.o wps/ssl/bn_const.o wps/ssl/bn_ctx.o \
                wps/ssl/bn_div.o wps/ssl/bn_exp.o wps/ssl/bn_gcd.o \
				wps/ssl/bn_mod.o wps/ssl/bn_mont.o \
                wps/ssl/bn_mul.o wps/ssl/bn_prime.o wps/ssl/bn_rand.o \
				wps/ssl/bn_recp.o wps/ssl/bn_shift.o wps/ssl/bn_sqr.o \
                wps/ssl/bn_word.o wps/ssl/dh_check.o wps/ssl/dh_gen.o \
				wps/ssl/dh_key.o wps/ssl/dh_lib.o wps/ssl/digest.o \
                wps/ssl/m_sha1.o wps/ssl/hmac.o wps/ssl/md_rand.o \
				wps/ssl/rand_lib.o wps/ssl/sha1dgst.o wps/ssl/sha256.o \
                wps/ssl/aes_cbc.o wps/ssl/fips_aes_core.o
				
#EXTRA_CFLAGS += -I../../../lib
obj-y += wps/8192cd_wscd.o
obj-y += wps/sercomm_intf.o
endif

# HH71VM: removed `rtl8192cd-objs += ../rtl8192cd_dlink/8192cd_dlink_autoch.o`.
# Product add-on D-Link (auto channel selection) from the NEIGHBORING directory, which is in the GPL archive
# no at all - the path leads outside the driver tree. Has nothing to do with iron.
#
# HH71VM: two keys have been removed from EXTRA_CFLAGS.
# Was: EXTRA_CFLAGS += -I$(DIR_BOARD) -D'SVN_REV=...' -fsigned-char -w
#   -I$(DIR_BOARD) - the variable is set by the assembly system of the vendor SDK, we don’t have it.
#       An EMPTY variable leaves a single `-I` on the command line, which takes away under
#       directory NEXT compiler argument and breaks the assembly so that the reason is not visible in
#       error message. The rtknet Makefile applies the same correction.
#   -w suppresses all warnings. That is harmful when porting 2017 vendor code because
#       warnings about implicit function declarations exposed four missing vendor
#       declarations. It was removed intentionally so that such places remain visible.
EXTRA_CFLAGS += -D'SVN_REV="$(shell svnversion -n $(src))"' -fsigned-char
#
# HH71VM: keep incompatible-pointer-type diagnostics as warnings.
# warnings. The rationale is not “to get it together,” but this:
#   the 4.14 kernel in its Makefile (line 845) adds -Werror=incompatible-pointer-types.
#   This key appeared in the 4.9 core; the vendor code was compiled under 3.10 and 4.4, where it is not present,
#   and in addition under -w (muffles everything). That is, this entire class of messages in the vendor assembly
#   existed as warnings and never interfered - on serial devices with 8812F
#   This code works exactly. Checked: in a newer snapshot SDK (AX12 v3.6.0) are the same
#   the files of the layer HALMAC lie without a single edit.
#   Rewriting someone else's HAL by hand for the sake of compiler silence is a greater risk than leaving it
#   in the form in which it is debugged by the vendor. Warnings remain visible
#   but truly dangerous classes (implicit-function-declaration and others) remain
#   fatal, as decided on M5.
EXTRA_CFLAGS += -Wno-error=incompatible-pointer-types
#EXTRA_CFLAGS += -I$(DIR_BOARD) -Werror
#EXTRA_CFLAGS += -I$(DIR_LINUX)/drivers/net/rtl819x/
#EXTRA_CFLAGS += -DDEBUG -g


# ============================= DLINK START =======================
CUSTOM_PBR =
CUSTOM_TXPWR =

# 8197F
# TX Power Limit
CUSTOM_TXPWR_8197F = $(strip $(subst ",,$(CONFIG_DLINK_WIFI_8197F_CUSTOM_TXPWR)))
ifneq ($(CUSTOM_TXPWR_8197F),)
$(src)/$(RTL_WLAN_DATA_DIR_97F)/$(CUSTOM_TXPWR_8197F)/TXPWR_LMT_*.txt: FORCE
	@cp -p $@ $(@D)/../
CUSTOM_TXPWR += $(wildcard $(src)/$(RTL_WLAN_DATA_DIR_97F)/$(CUSTOM_TXPWR_8197F)/TXPWR_LMT_*.txt)
endif

# Power by Rate
CUSTOM_PBR_8197F = $(strip $(subst ",,$(CONFIG_DLINK_WIFI_8197F_CUSTOM_PBR)))
ifneq ($(CUSTOM_PBR_8197F),)
$(src)/$(RTL_WLAN_DATA_DIR_97F)/$(CUSTOM_PBR_8197F)/PHY_REG_PG_*.txt: FORCE
	@cp -p $@ $(@D)/../
CUSTOM_PBR += $(wildcard $(src)/$(RTL_WLAN_DATA_DIR_97F)/$(CUSTOM_PBR_8197F)/PHY_REG_PG_*.txt)
endif

# 8197G
# TX Power Limit
CUSTOM_TXPWR_8197G = $(strip $(subst ",,$(CONFIG_DLINK_WIFI_8197G_CUSTOM_TXPWR)))
ifneq ($(CUSTOM_TXPWR_8197G),)
$(src)/$(RTL_WLAN_DATA_DIR_97G)/$(CUSTOM_TXPWR_8197G)/TXPWR_LMT_*.txt: FORCE
	@cp -p $@ $(@D)/../
CUSTOM_TXPWR += $(wildcard $(src)/$(RTL_WLAN_DATA_DIR_97G)/$(CUSTOM_TXPWR_8197G)/TXPWR_LMT_*.txt)
endif

# Power by Rate
CUSTOM_PBR_8197G = $(strip $(subst ",,$(CONFIG_DLINK_WIFI_8197G_CUSTOM_PBR)))
ifneq ($(CUSTOM_PBR_8197G),)
$(src)/$(RTL_WLAN_DATA_DIR_97G)/$(CUSTOM_PBR_8197G)/PHY_REG_PG_*.txt: FORCE
	@cp -p $@ $(@D)/../
CUSTOM_PBR += $(wildcard $(src)/$(RTL_WLAN_DATA_DIR_97G)/$(CUSTOM_PBR_8197G)/PHY_REG_PG_*.txt)
endif

# 8822B (8812BRH)
# TX Power Limit
CUSTOM_TXPWR_8822B = $(strip $(subst ",,$(CONFIG_DLINK_WIFI_8822B_CUSTOM_TXPWR)))
ifneq ($(CUSTOM_TXPWR_8822B),)
$(src)/$(RTL_WLAN_DATA_DIR_8822B)/$(CUSTOM_TXPWR_8822B)/TXPWR_LMT_*.txt: FORCE
	@cp -p $@ $(@D)/../
CUSTOM_TXPWR += $(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8822B)/$(CUSTOM_TXPWR_8822B)/TXPWR_LMT_*.txt)
endif

# Power by Rate
CUSTOM_PBR_8822B = $(strip $(subst ",,$(CONFIG_DLINK_WIFI_8822B_CUSTOM_PBR)))
ifneq ($(CUSTOM_PBR_8822B),)
$(src)/$(RTL_WLAN_DATA_DIR_8822B)/$(CUSTOM_PBR_8822B)/PHY_REG_PG_*.txt: FORCE
	@cp -p $@ $(@D)/../
CUSTOM_PBR += $(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8822B)/$(CUSTOM_PBR_8822B)/PHY_REG_PG_*.txt)
endif

# 8812F
# TX Power Limit
CUSTOM_TXPWR_8812F = $(strip $(subst ",,$(CONFIG_DLINK_WIFI_8812F_CUSTOM_TXPWR)))
ifneq ($(CUSTOM_TXPWR_8812F),)
$(src)/$(RTL_WLAN_DATA_DIR_8812F)/$(CUSTOM_TXPWR_8812F)/TXPWR_LMT_*.txt: FORCE
	@cp -p $@ $(@D)/../
CUSTOM_TXPWR += $(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8812F)/$(CUSTOM_TXPWR_8812F)/TXPWR_LMT_*.txt)
endif

# Power by Rate
CUSTOM_PBR_8812F = $(strip $(subst ",,$(CONFIG_DLINK_WIFI_8812F_CUSTOM_PBR)))
ifneq ($(CUSTOM_PBR_8812F),)
$(src)/$(RTL_WLAN_DATA_DIR_8812F)/$(CUSTOM_PBR_8812F)/PHY_REG_PG_*.txt: FORCE
	@cp -p $@ $(@D)/../
CUSTOM_PBR += $(wildcard $(src)/$(RTL_WLAN_DATA_DIR_8812F)/$(CUSTOM_PBR_8812F)/PHY_REG_PG_*.txt)
endif
# ============================= DLINK END =======================

#
#Build C code for TXT or BIN file
#
$(obj)/8192cd_hw.o : $(CUSTOM_PBR) $(CUSTOM_TXPWR) $(SRCS_TXT) $(SRCS_BIN) $(SRCS_BIN_U) $(TXPWR_HEADER)

$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR)/%.txt
	rm -f $(obj)/TXPWR_92C.h
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR)/%.bin
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR)/%.BIN
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_D)/%.txt
	rm -f $(obj)/TXPWR_92D.h
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_D)/%.bin
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_D)/%.BIN
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@				
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_E)/%.txt
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_E)/%.bin
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_E)/%.BIN
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@	
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8812)/%.txt
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8812)/%.bin
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8812)/%.BIN 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@	
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8723)/%.txt 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8723)/%.bin 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8723)/%.BIN 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@		

$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_92E)/%.txt 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_92E)/%.bin 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_92E)/%.BIN 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@

$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_97F)/%.txt 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_97F)/%.bin 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_97F)/%.BIN 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@

$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_98F)/%.txt 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_98F)/%.bin 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_98F)/%.BIN 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@	

$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8822B)/%.txt 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8822B)/%.bin 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8822B)/%.BIN 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@

$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_97G)/%.txt 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_97G)/%.bin 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_97G)/%.BIN 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
	
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8822C)/%.txt 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8822C)/%.bin 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8822C)/%.BIN 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@

$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8812F)/%.txt 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8812F)/%.bin 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8812F)/%.BIN 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@	
	
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8821C)/%.txt 
	@perl -f $(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8821C)/%.bin 
	@perl -f $(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8821C)/%.BIN 
	@perl -f $(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@		

$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8814B)/%.txt 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8814B)/%.bin 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8814B)/%.BIN 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
	
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8881A)/%.txt 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8881A)/%.bin 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8881A)/%.BIN 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@

$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8814)/%.txt 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8814)/%.bin 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8814)/%.BIN 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
	
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8192F)/%.txt 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8192F)/%.bin 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_8192F)/%.BIN 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@

$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_PE)/%.txt 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_PE)/%.bin 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
$(obj)/data_%.c: $(src)/$(RTL_WLAN_DATA_DIR_PE)/%.BIN 
	$(obj)/bin2c.pl $(notdir $(basename $@)) < $< > $@
	
$(obj)/TXPWR_92C.h: $(TXPWR_LMT_TXT_92C) 
	@echo $(TXPWR_LMT_FNAME_92C) > $(obj)/tmp_TXPWR
	$(obj)/tplmt2h.pl < $(obj)/tmp_TXPWR > $@
	rm -f $(obj)/tmp_TXPWR
$(obj)/TXPWR_92D.h: $(TXPWR_LMT_TXT_92D) 
	@echo $(TXPWR_LMT_FNAME_92D) > $(obj)/tmp_TXPWR
	$(obj)/tplmt2h.pl < $(obj)/tmp_TXPWR > $@
	rm -f $(obj)/tmp_TXPWR	
