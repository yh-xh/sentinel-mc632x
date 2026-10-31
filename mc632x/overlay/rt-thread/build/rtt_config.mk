-include $(SDKROOT)/.config

OUT ?= $(SDKROOT)/out

TOOLS_DIR := $(SDKROOT)/build/tools
export SOC_ID := $(shell python $(TOOLS_DIR)/extract_id.py chip $(SDKROOT)/.config)
export SOC_ID_LOWER = $(shell echo $(SOC_ID) | tr '[:upper:]' '[:lower:]')

export ARCH_ID := $(shell python $(TOOLS_DIR)/extract_id.py arch $(SDKROOT)/.config)
export ARCH_ID_LOWER = $(shell echo $(ARCH_ID) | tr '[:upper:]' '[:lower:]')

# sentinel needs this CHIP_ID
export CHIP_ID = $(ARCH_ID)
export CHIP_ID_LOWER = $(ARCH_ID_LOWER)

SOC_NAME = $(SOC_ID_LOWER)
SOC_LINK = $(ARCH_ID_LOWER)

# RTT_INCDIR is used for library includes,
# that is, pathes to be searched when building a library
RTT_INCDIR = $(OUT)/rtconfig/
SYSCONFIG = $(OUT)/rtconfig/rtconfig.h
RTT_INCDIR += $(SDKROOT)/platform/$(SOC_ID_LOWER)

# SYS_INCDIR is used for application includes.
SYS_INCDIR = $(SDKROOT)/lib/inc
SYS_INCDIR += $(SDKROOT)/lib/inc/bsp
SYS_INCDIR += $(SDKROOT)/lib/inc/algo
SYS_INCDIR += $(SDKROOT)/lib/$(SOC_LINK)/inc
SYS_INCDIR += $(SDKROOT)/lib/$(SOC_LINK)/inc/dsp
SYS_INCDIR += $(SDKROOT)/lib/$(SOC_LINK)/inc/isp
SYS_INCDIR += $(SDKROOT)/lib/$(SOC_LINK)/inc/advapi
# VIDEO_INC is used for video relevant includes.
VIDEO_INC  = -I$(SDKROOT)/lib/$(SOC_LINK)/inc
VIDEO_INC += -I$(SDKROOT)/lib/$(SOC_LINK)/inc/advapi
VIDEO_INC += -I$(SDKROOT)/lib/$(SOC_LINK)/inc/dsp
VIDEO_INC += -I$(SDKROOT)/lib/$(SOC_LINK)/inc/isp
VIDEO_INC += -I$(SDKROOT)/lib/inc/types

# FLAGS
ASFLAGS := -D__ASSEMBLY__ -x assembler-with-cpp
CFLAGS  := -mno-unaligned-access -Wno-error=sequence-point -fno-strict-aliasing
CFLAGS  += -Wno-address -fdata-sections -ffunction-sections -Wall
LDFLAGS = -Wl,--gc-sections,-cref -nostartfiles
LDFLAGS += -T$(SDKROOT)/build/link.ld
LDFLAGS += -L$(OUT)/lib -L$(SDKROOT)/build
LDFLAGS +=-Wl,--wrap,_dtoa_r
LDFLAGS +=-Wl,--wrap,strtod
LDFLAGS +=-Wl,--wrap,strtof
LDFLAGS +=-Wl,-u__wrap__dtoa_r
LDFLAGS +=-Wl,-u__wrap_strtod
LDFLAGS +=-Wl,-u__wrap_strtod

ifeq ($(DEBUG), 1)
    CFLAGS += -O0 -gdwarf-2
    ASFLAGS += -gdwarf-2
else
    CFLAGS += -O2 -fno-omit-frame-pointer -mapcs
endif

ifeq ($(CONFIG_ARCH_ARM_CORTEX_A7),y)
    CPU := cortex-a
    CFLAGS += -mcpu=cortex-a7
    LDFLAGS += -mcpu=cortex-a7
    ifeq ($(CONFIG_RT_USING_VFP),y)
        CFLAGS += -mfloat-abi=hard -mfpu=neon-vfpv4
        LDFLAGS += -mfloat-abi=hard -mfpu=neon-vfpv4
    else
        CFLAGS += -mfloat-abi=soft
        LDFLAGS += -mfloat-abi=soft
    endif
endif

ifeq ($(CONFIG_ARCH_ARM_ARM11), y)
    CPU := armv6
    CFLAGS += -mcpu=arm1176jzf-s
    LDFLAGS += -mcpu=arm1176jzf-s
    ifeq ($(CONFIG_RT_USING_VFP),y)
        CFLAGS += -mfloat-abi=softfp -mfpu=vfp
        LDFLAGS += -mfloat-abi=softfp -mfpu=vfp
    else
        CFLAGS += -mfloat-abi=soft
        LDFLAGS += -mfloat-abi=soft
    endif
endif

# 1. OS
LNKLIBS = -lkernel -lcpu -lplatform
LIBDIRS = kernel platform platform/cpu
DEPLIBS = $(OUT)/lib/libkernel.a
DEPLIBS += $(OUT)/lib/libcpu.a
DEPLIBS += $(OUT)/lib/libplatform.a
RTT_INCDIR += $(SDKROOT)/kernel/include
RTT_INCDIR += $(SDKROOT)/kernel/libc/time

ifeq ($(CONFIG_RT_USING_PM), y)
    RTT_INCDIR += $(SDKROOT)/kernel/include/pm
endif

# platform path
RTT_INCDIR += $(SDKROOT)/platform
RTT_INCDIR += $(SDKROOT)/platform/include
RTT_INCDIR += $(SDKROOT)/platform/cpu/$(CPU)

# if new board type added, modify here and Kconfig
ifeq ($(CONFIG_CONFIG_BOARD_APP), y)
    BOARD_NAME = app_board
endif

ifeq ($(CONFIG_CONFIG_BOARD_TEST), y)
    BOARD_NAME = test_board
endif

ifeq ($(CONFIG_CONFIG_BOARD_FAST), y)
    BOARD_NAME = fast_board
endif

ifeq ($(CONFIG_CONFIG_BOARD_FAST), y)
LNKLIBS += -lps5270_mipi_rtt -lovos04c10_mipi_rtt
endif

################ config specific #################
ifeq ($(CONFIG_RT_USING_CPLUSPLUS),y)
    LNKLIBS += -lstdc++
endif
# VFP only differs libispcore_rtt.a
LDFLAGS += -L$(SDKROOT)/lib/$(SOC_LINK)/lib
LDFLAGS += -L$(SDKROOT)/lib/$(SOC_LINK)/lib/advapi
LDFLAGS += -L$(SDKROOT)/lib/$(SOC_LINK)/lib/isp
LDFLAGS += -L$(SDKROOT)/lib/$(SOC_LINK)/lib/dsp
LDFLAGS += -L$(SDKROOT)/lib/$(SOC_LINK)/lib/bsp
LDFLAGS += -L$(SDKROOT)/lib/$(SOC_LINK)/lib/algo
LDFLAGS += -L$(SDKROOT)/lib/$(SOC_LINK)/lib/sensors
# for cpu specific library[armv6/a7]
# now, assume all CPU specific Library has no dependance on VFP
#      ONLY ISP/DSP depend on VFP
LDFLAGS += -L$(SDKROOT)/lib/$(CPU)
LDFLAGS += -L$(SDKROOT)/lib/$(CPU)/bsp
LDFLAGS += -L$(SDKROOT)/lib/$(CPU)/algo
LDFLAGS += -L$(SDKROOT)/lib/$(CPU)/osal

ifeq ($(CONFIG_RT_USING_TIMEKEEPING), y)
    RTT_INCDIR += $(SDKROOT)/kernel/libc/time/hrtime
endif

ifeq ($(CONFIG_RT_USING_NEWLIB),y)
    RTT_INCDIR += $(SDKROOT)/kernel/libc/compilers/newlib
endif

ifeq ($(CONFIG_RT_USING_POSIX_TERMIOS), y)
    RTT_INCDIR += $(SDKROOT)/kernel/libc/termios
endif

# 2. drivers
# kernel driver arch
RTT_INCDIR += $(SDKROOT)/drivers/arch
RTT_INCDIR += $(SDKROOT)/drivers/include
RTT_INCDIR += $(SDKROOT)/lib/inc
RTT_INCDIR += $(SDKROOT)/lib/inc/bsp
RTT_INCDIR += $(SDKROOT)/lib/inc/osal/inc

ifeq ($(CONFIG_FH_USING_AES), y)
    LNKLIBS += -laes
    LIBDIRS += drivers/aes
    DEPLIBS += $(OUT)/lib/libaes.a
endif

ifeq ($(CONFIG_FH_USING_CESA), y)
    LNKLIBS += -lcesa
    LIBDIRS += drivers/cesa
    DEPLIBS += $(OUT)/lib/libcesa.a
endif

ifeq ($(CONFIG_FH_USING_EFUSE), y)
    LNKLIBS += -lefuse
    LIBDIRS += drivers/efuse
    DEPLIBS += $(OUT)/lib/libefuse.a
endif

ifeq ($(CONFIG_FH_USING_CLOCK), y)
    LNKLIBS += -lclock
    LIBDIRS += drivers/clock
    DEPLIBS += $(OUT)/lib/libclock.a
endif

ifeq ($(CONFIG_FH_USING_DMA), y)
    LNKLIBS += -ldma
    LIBDIRS += drivers/dma
    DEPLIBS += $(OUT)/lib/libdma.a
endif

ifeq ($(CONFIG_FH_USING_AXI_DMA), y)
    LNKLIBS += -ldma
    LIBDIRS += drivers/dma
    DEPLIBS += $(OUT)/lib/libdma.a
endif

ifeq ($(CONFIG_FH_USING_MOL_DMA), y)
    LNKLIBS += -ldma
    LIBDIRS += drivers/dma
    DEPLIBS += $(OUT)/lib/libdma.a
endif


ifeq ($(CONFIG_FH_USING_FLASH), y)
    LNKLIBS += -lflash
    LIBDIRS += drivers/flash
    DEPLIBS += $(OUT)/lib/libflash.a
    RTT_INCDIR += $(SDKROOT)/drivers/flash/sfud/inc
endif

ifeq ($(CONFIG_FH_USING_GMAC), y)
    LNKLIBS += -lgmac
    LIBDIRS += drivers/gmac
    DEPLIBS += $(OUT)/lib/libgmac.a
endif
ifeq ($(CONFIG_FH_USING_QOS_GMAC), y)
    LNKLIBS += -lgmac
    LIBDIRS += drivers/gmac
    DEPLIBS += $(OUT)/lib/libgmac.a
endif

ifeq ($(CONFIG_RT_USING_GPIO), y)
    LNKLIBS += -lgpio
    LIBDIRS += drivers/gpio
    DEPLIBS += $(OUT)/lib/libgpio.a
endif

ifeq ($(CONFIG_RT_USING_I2C), y)
    LNKLIBS += -li2c
    LIBDIRS += drivers/i2c
    DEPLIBS += $(OUT)/lib/libi2c.a
endif

ifeq ($(CONFIG_FH_USING_I2S)$(CONFIG_FH_USING_MOL_I2S), y)
    LNKLIBS += -li2s
    LIBDIRS += drivers/i2s
    DEPLIBS += $(OUT)/lib/libi2s.a
endif

# ifeq ($(CONFIG_ARCH_ARM_ARM11), y)
    LNKLIBS += -linterrupt
    LIBDIRS += drivers/interrupt
    DEPLIBS += $(OUT)/lib/libinterrupt.a
# endif

# ifeq ($(CONFIG_ARCH_ARM_CORTEX_A7), y)
#    LNKLIBS += -lgic
#    LIBDIRS += drivers/gic
#    DEPLIBS += $(OUT)/lib/libgic.a
# endif

ifeq ($(CONFIG_RT_USING_SDIO), y)
    LNKLIBS += -lmmc
    LIBDIRS += drivers/mmc
    DEPLIBS += $(OUT)/lib/libmmc.a
endif

ifeq ($(CONFIG_RT_USING_MTD), y)
    LNKLIBS += -lmtd
    LIBDIRS += drivers/mtd
    DEPLIBS += $(OUT)/lib/libmtd.a
endif

ifeq ($(CONFIG_RT_USING_NET), y)
    LNKLIBS += -lnet
    LIBDIRS += drivers/net
    DEPLIBS += $(OUT)/lib/libnet.a
endif

ifeq ($(CONFIG_RT_USING_PWM), y)
    LNKLIBS += -lpwm
    LIBDIRS += drivers/pwm
    DEPLIBS += $(OUT)/lib/libpwm.a
endif

ifeq ($(CONFIG_RT_USING_RTC), y)
    LNKLIBS += -lrtc
    LIBDIRS += drivers/rtc
    DEPLIBS += $(OUT)/lib/librtc.a
endif

ifeq ($(CONFIG_FH_USING_SADC), y)
    LNKLIBS += -lsadc
    LIBDIRS += drivers/sadc
    DEPLIBS += $(OUT)/lib/libsadc.a
endif

ifeq ($(CONFIG_RT_USING_SERIAL), y)
    LNKLIBS += -lserial
    LIBDIRS += drivers/serial
    DEPLIBS += $(OUT)/lib/libserial.a
endif

ifeq ($(CONFIG_RT_USING_SPI), y)
    LNKLIBS += -lspi
    LIBDIRS += drivers/spi
    DEPLIBS += $(OUT)/lib/libspi.a
endif

ifeq ($(CONFIG_RT_USING_TIMER), y)
    LNKLIBS += -ltimer
    LIBDIRS += drivers/timer
    DEPLIBS += $(OUT)/lib/libtimer.a
endif

ifeq ($(CONFIG_FH_USING_UART0), y)
    LNKLIBS += -luart
    LIBDIRS += drivers/uart
    DEPLIBS += $(OUT)/lib/libuart.a
endif

ifeq ($(CONFIG_FH_USING_UART1), y)
    LNKLIBS += -luart
    LIBDIRS += drivers/uart
    DEPLIBS += $(OUT)/lib/libuart.a
endif

ifeq ($(CONFIG_RT_USING_WDT), y)
    LNKLIBS += -lwdt
    LIBDIRS += drivers/wdt
    DEPLIBS += $(OUT)/lib/libwdt.a
endif

ifeq ($(CONFIG_FH_USING_FH_PERF), y)
    LNKLIBS += -lfh_perf
    LIBDIRS += drivers/fh_perf
    DEPLIBS += $(OUT)/lib/libfh_perf.a
endif

ifeq ($(CONFIG_RT_USING_PM), y)
    LNKLIBS += -lfh_pm
    LIBDIRS += drivers/fh_pm
    DEPLIBS += $(OUT)/lib/libfh_pm.a
endif

ifeq ($(CONFIG_FH_USING_FH_STEPMOTOR), y)
    LNKLIBS += -lfh_smt
    LIBDIRS += drivers/fh_smt
    DEPLIBS += $(OUT)/lib/libfh_smt.a
endif

ifeq ($(CONFIG_FH_USING_FH_HASH), y)
    LNKLIBS += -lfh_hash
    LIBDIRS += drivers/fh_hash
    DEPLIBS += $(OUT)/lib/libfh_hash.a
endif

ifeq ($(CONFIG_RT_USING_USB), y)
    LNKLIBS += -lusb
    LIBDIRS += drivers/usb
    DEPLIBS += $(OUT)/lib/libusb.a
endif

ifeq ($(CONFIG_FH_USING_MODULE_CASE_LOCAL), y)
    LNKLIBS += -lmodule_case -lmcdb_api_arm_rtt
endif

ifeq ($(CONFIG_FH_USING_MODULE_CASE_RPC), y)
    LNKLIBS += -lmcdb_api_rpc_arm_rtt
endif

ifeq ($(CONFIG_FH_USING_WIFI), y)
    LNKLIBS += -lwifi
    LIBDIRS += drivers/wifi
    DEPLIBS += $(OUT)/lib/libwifi.a
    LDFLAGS += -L$(SDKROOT)/lib/$(CPU)/wifi
    RTT_INCDIR += $(SDKROOT)/drivers/wifi
    SYS_INCDIR += $(SDKROOT)/drivers/wifi/pub/

    ifeq ($(CONFIG_WIFI_USING_USBWIFI_8188F), y)
        LNKLIBS += -l8188f -leapol
    endif

    ifeq ($(CONFIG_WIFI_USING_USBWIFI_MTK7603U), y)
        LNKLIBS += -lmt7603u_sta
    endif

    ifeq ($(CONFIG_WIFI_USING_USBWIFI_RTL8192F), y)
        LNKLIBS += -l8192f -leapol
    endif

    ifeq ($(CONFIG_WIFI_USING_USBWIFI_RTL8192E), y)
        ifeq ($(CONFIG_WIFI_STA_ONLY), y)
        LNKLIBS += -l8192eu_sta -leapol_sta
        else
        LNKLIBS += -l8192eu -leapol
        endif
    endif

    ifeq ($(CONFIG_WIFI_USING_USBWIFI_RTL8731BU), y)
        ifeq ($(CONFIG_WIFI_STA_ONLY), y)
        LNKLIBS += -l8733bu_sta -leapol_sta
        else
        LNKLIBS += -l8733bu -leapol
        endif
    endif

    ifeq ($(CONFIG_WIFI_USING_RTL8192FS), y)
        ifeq ($(CONFIG_WIFI_STA_ONLY), y)
        LNKLIBS += -l8192fs_sta -leapol_sta
        else
        LNKLIBS += -l8192fs -leapol
        endif
    endif

    ifeq ($(CONFIG_WIFI_USING_MARVEL), y)
        LNKLIBS += -lmw8801
    endif

    ifeq ($(CONFIG_WIFI_USING_HI3861L), y)
        LNKLIBS += -lw_hi3861L_hich
    endif

    ifeq ($(CONFIG_WIFI_USING_CYPRESS), y)
        ifeq ($(CONFIG_WIFI_USING_CY43455), y)
            ifeq ($(CONFIG_WIFI_USING_CY43455_WLTOOL), y)
                LNKLIBS += -lCy43455_nor_WL
            else
                LNKLIBS += -lCy43455_nor
            endif
        endif

        ifeq ($(CONFIG_WIFI_USING_CY43438_NORMAL), y)
            ifeq ($(CONFIG_WIFI_USING_CY43438_WLTOOL), y)
                LNKLIBS += -lCy43438_nor_WL
            else
                LNKLIBS += -lCy43438_nor
            endif
        endif

        ifeq ($(CONFIG_WIFI_USING_CY43438_LOWPOWER), y)
            ifeq ($(CONFIG_WIFI_USING_CY43438_WLTOOL), y)
                LNKLIBS += -lCy43438_lowpower_WL
            else
                LNKLIBS += -lCy43438_lowpower
            endif
        endif
    endif
endif

ifeq ($(CONFIG_FH_USING_NAND_FLASH), y)
    LNKLIBS += -lnand
    LIBDIRS += drivers/nand
    DEPLIBS += $(OUT)/lib/libnand.a
    RTT_INCDIR += $(SDKROOT)/drivers/nand/inc
endif

# 3. compat
ifeq ($(CONFIG_RT_USING_COMPONENTS_LINUX_ADAPTER), y)
    LNKLIBS += -llinux
    LIBDIRS += compat/linux
    DEPLIBS += $(OUT)/lib/liblinux.a
endif

ifeq ($(CONFIG_RT_USING_PTHREADS), y)
    LNKLIBS += -lposix
    LIBDIRS += compat/posix
    DEPLIBS += $(OUT)/lib/libposix.a
endif

ifeq (y, $(findstring y,$(CONFIG_RT_USING_PTHREADS)$(CONFIG_RT_USING_COMPONENTS_LINUX_ADAPTER)))
    RTT_INCDIR += $(SDKROOT)/compat/include
    SYS_INCDIR += $(SDKROOT)/compat/include
endif

# 4. fs
ifeq ($(CONFIG_RT_USING_DFS), y)
    LNKLIBS += -lfs
    LIBDIRS += fs
    DEPLIBS += $(OUT)/lib/libfs.a
    RTT_INCDIR += $(SDKROOT)/fs/include
    ifeq ($(CONFIG_RT_USING_DFS_RAMFS),y)
        RTT_INCDIR += $(SDKROOT)/fs/filesystems/ramfs
    endif
    ifeq ($(CONFIG_RT_USING_DFS_ROMFS),y)
        RTT_INCDIR += $(SDKROOT)/fs/filesystems/romfs
    endif
    ifeq ($(CONFIG_RT_USING_DFS_JFFS2),y)
        RTT_INCDIR += $(SDKROOT)/fs/filesystems/jffs2
    endif
    ifeq ($(CONFIG_RT_USING_DFS_ELMFAT),y)
        RTT_INCDIR += $(SDKROOT)/fs/filesystems/elmfat/inc
    endif
    ifeq ($(CONFIG_RT_USING_DFS_UFFS),y)
        RTT_INCDIR += $(SDKROOT)/fs/filesystems/uffs
        RTT_INCDIR += $(SDKROOT)/fs/filesystems/uffs/src/inc
    endif
    ifeq ($(CONFIG_RT_USING_DFS_NFS),y)
        RTT_INCDIR += $(SDKROOT)/fs/filesystems/nfs
    endif
    ifeq ($(CONFIG_RT_USING_MTD_UBI),y)
        RTT_INCDIR += $(SDKROOT)/fs/filesystems/ubifs
    endif
endif

# 5. net
ifeq ($(CONFIG_RT_USING_LWIP), y)
    LNKLIBS += -llwip-2.1.2
    LIBDIRS += net/lwip-2.1.2
    DEPLIBS += $(OUT)/lib/liblwip-2.1.2.a
    RTT_INCDIR += $(SDKROOT)/net/lwip-2.1.2/src
    RTT_INCDIR += $(SDKROOT)/net/lwip-2.1.2/src/include
    RTT_INCDIR += $(SDKROOT)/net/lwip-2.1.2/src/arch/include
    RTT_INCDIR += $(SDKROOT)/net/lwip-2.1.2/src/include/netif
endif

ifeq ($(CONFIG_FH_USING_PROFILING), y)
    LNKLIBS += -lprofiling
    LIBDIRS += utils/profiling
    DEPLIBS += $(OUT)/lib/libprofiling.a
endif

ifeq ($(CONFIG_FH_USING_RSHELL), y)
    LNKLIBS += -lrshell
    LIBDIRS += utils/rshell
    DEPLIBS += $(OUT)/lib/librshell.a
endif

ifeq ($(CONFIG_PKG_USING_OPTPARSE), y)
    LNKLIBS += -loptparse
    LIBDIRS += utils/optparse
    DEPLIBS += $(OUT)/lib/liboptparse.a
    RTT_INCDIR += $(SDKROOT)/utils/optparse
    SYS_INCDIR += $(SDKROOT)/utils/optparse
endif

ifeq ($(CONFIG_PKG_USING_VI), y)
    LNKLIBS += -lvi
    LIBDIRS += utils/vi
    DEPLIBS += $(OUT)/lib/libvi.a
endif


ifeq ($(CONFIG_RT_USING_ULOG), y)
    LNKLIBS += -lulog
    LIBDIRS += utils/ulog
    DEPLIBS += $(OUT)/lib/libulog.a
    RTT_INCDIR += $(SDKROOT)/utils/ulog
    ifeq ($(CONFIG_ULOG_USING_SYSLOG), y)
        RTT_INCDIR += $(SDKROOT)/utils/ulog/syslog
    endif
endif

ifeq ($(CONFIG_FH_USING_YMODEM), y)
    LNKLIBS += -lymodem
    LIBDIRS += utils/ymodem
    DEPLIBS += $(OUT)/lib/libymodem.a
endif

ifeq ($(CONFIG_FH_USING_ZMODEM), y)
    LNKLIBS += -lzmodem
    LIBDIRS += utils/zmodem
    DEPLIBS += $(OUT)/lib/libzmodem.a
endif

ifeq ($(CONFIG_RT_USING_MOBILE_TELE), y)
    LNKLIBS += -lmobile_tele
    LIBDIRS += utils/mobile_tele
    DEPLIBS += $(OUT)/lib/libmobile_tele.a
endif

# 7. shell
ifeq ($(CONFIG_RT_USING_FINSH), y)
    LNKLIBS += -lshell
    LIBDIRS += shell/
    DEPLIBS += $(OUT)/lib/libshell.a
    RTT_INCDIR += $(SDKROOT)/shell
endif

# 8. external
ifeq ($(CONFIG_FH_USING_BONJOUR), y)
    LNKLIBS += -lbonjour
    LIBDIRS += external/bonjour
    DEPLIBS += $(OUT)/lib/libbonjour.a
    SYS_INCDIR += $(SDKROOT)/external/bonjour/mDNSCore
    SYS_INCDIR += $(SDKROOT)/external/bonjour/mDNSRtthread
    SYS_INCDIR += $(SDKROOT)/external/bonjour/mDNSShared
endif

ifeq ($(CONFIG_FH_USING_GUNZIP), y)
    LNKLIBS += -lgunzip
    LIBDIRS += external/gunzip
    DEPLIBS += $(OUT)/lib/libgunzip.a
endif

ifeq ($(CONFIG_FH_USING_MBEDTLS), y)
    LNKLIBS += -lmbedtls
    LIBDIRS += external/mbedtls
    DEPLIBS += $(OUT)/lib/libmbedtls.a
    RTT_INCDIR += $(SDKROOT)/external/mbedtls/include
    SYS_INCDIR += $(SDKROOT)/external/mbedtls/include
endif

ifeq ($(CONFIG_FH_USING_LIBCURL), y)
    LNKLIBS += -llibcurl
    LIBDIRS += external/libcurl
    DEPLIBS += $(OUT)/lib/liblibcurl.a
    SYS_INCDIR += $(SDKROOT)/external/libcurl
endif

ifeq ($(CONFIG_FH_USING_UDT), y)
    LNKLIBS += -ludt
    LIBDIRS += external/udt
    DEPLIBS += $(OUT)/lib/libudt.a
    SYS_INCDIR += $(SDKROOT)/external/udt
endif

ifeq ($(CONFIG_FH_USING_SRT), y)
    LNKLIBS += -lsrt
    LIBDIRS += external/srt
    DEPLIBS += $(OUT)/lib/libsrt.a
    SYS_INCDIR += $(SDKROOT)/external/srt/srtcore
    SYS_INCDIR += $(SDKROOT)/external/srt/haicrypt
endif

ifeq ($(CONFIG_FH_USING_MINIUPNPC), y)
    LNKLIBS += -lminiupnpc-2.0
    LIBDIRS += external/miniupnpc-2.0
    DEPLIBS += $(OUT)/lib/libminiupnpc-2.0.a
endif

ifeq ($(CONFIG_FH_USING_LZMA), y)
    LNKLIBS += -lunlzma
    LIBDIRS += external/unlzma
    DEPLIBS += $(OUT)/lib/libunlzma.a
endif

ifeq ($(CONFIG_FH_USING_LZOP), y)
    LNKLIBS += -lunlzo
    LIBDIRS += external/unlzo
    DEPLIBS += $(OUT)/lib/libunlzo.a
endif

ifeq ($(CONFIG_FH_USING_IPERF), y)
    LNKLIBS += -liperf
    LIBDIRS += external/iperf
    DEPLIBS += $(OUT)/lib/libiperf.a
endif

ifeq ($(CONFIG_FH_USING_ZLIB), y)
    LNKLIBS += -lzlib
    LIBDIRS += external/zlib
    DEPLIBS += $(OUT)/lib/libzlib.a
    RTT_INCDIR += $(SDKROOT)/external/zlib/include
endif

ifeq ($(CONFIG_FH_USING_CJSON), y)
    LNKLIBS += -lcJSON
    LIBDIRS += external/cJSON
    DEPLIBS += $(OUT)/lib/libcJSON.a
    ifeq ($(CONFIG_FH_CJSON_VERSION_RAW),y)
        RTT_INCDIR += $(SDKROOT)/external/cJSON/raw
        SYS_INCDIR += $(SDKROOT)/external/cJSON/raw
    else
        RTT_INCDIR += $(SDKROOT)/external/cJSON/v1.7.13
        SYS_INCDIR += $(SDKROOT)/external/cJSON/v1.7.13
    endif
endif

ifeq ($(CONFIG_FH_USING_FHCRC), y)
    LNKLIBS += -lfh_crc
    LIBDIRS += external/fh_crc
    DEPLIBS += $(OUT)/lib/libfh_crc.a
    RTT_INCDIR += $(SDKROOT)/external/fh_crc
endif

ifeq ($(CONFIG_FH_USING_MXML), y)
    LNKLIBS += -lmxml-3.1
    LIBDIRS += external/mxml-3.1
    DEPLIBS += $(OUT)/lib/libmxml-3.1.a
    RTT_INCDIR += $(SDKROOT)/external/mxml-3.1/inc
endif

ifeq ($(CONFIG_FH_USING_ZBAR), y)
    LNKLIBS += -lzbar
    LIBDIRS += external/zbar
    DEPLIBS += $(OUT)/lib/libzbar.a
    RTT_INCDIR += $(SDKROOT)/external/zbar/inc
endif

ifeq ($(CONFIG_FH_USING_TAR), y)
    LNKLIBS += -ltar
    LIBDIRS += external/tar
    DEPLIBS += $(OUT)/lib/libtar.a
endif

ifeq ($(CONFIG_FH_USING_VBUS),y)
    LNKLIBS += -lxbus_rpc_arm_rtt
endif

# example for a single library
ifeq ($(CONFIG_CONFIG_TEST_LIB), y)
    LNKLIBS += -ltestlib
    LIBDIRS += external/testlib
    DEPLIBS += $(OUT)/lib/libtestlib.a
endif

# all SDK link libs
LNKLIBS += -losal -lvmm_arm_rtt -lfh_crypto_api_rtt -lvb_mpi_arm_rtt

# depends on ACW
ifeq ($(CONFIG_FH_USING_ACW), y)
    LNKLIBS += -lacw_mpi_arm_rtt -laudio_codec_arm_rtt
endif
# LNKLIBS += -lm

# LNKLIBS += -lnna_rtt -lnnpost_rtt

# 9. sample
SAMPLE_DIR =

ifeq ($(CONFIG_APP_CONFIG_HELLOWORLD), y)
    SAMPLE_DIR = $(SDKROOT)/app/helloworld
endif

ifeq ($(CONFIG_APP_CONFIG_UVC_UPDATE), y)
    SAMPLE_DIR = $(SDKROOT)/app/uvc_update
endif

ifeq ($(CONFIG_APP_CONFIG_SENTINEL), y)
    # for local build, set SAMPLE_DIR to your local path to sentinel
    # which is located in media repository
    SAMPLE_DIR = $(SDKROOT)/app/sentinel
endif

ifeq ($(CONFIG_APP_CONFIG_AUDIO_DEMO), y)
    SAMPLE_DIR = $(SDKROOT)/app/audio_demo
endif

ifeq ($(CONFIG_APP_CONFIG_AOV_DEMO), y)
    SAMPLE_DIR = $(SDKROOT)/app/aov_demo
endif

ifeq ($(CONFIG_APP_CONFIG_RTHELLO),y)
    SAMPLE_DIR = $(SDKROOT)/app/rthello
endif

ifeq ($(CONFIG_APP_CONFIG_BSP_DEMO), y)
    SAMPLE_DIR = $(SDKROOT)/app/bsp_demo
endif

INCFLAGS=$(foreach incdir,$(RTT_INCDIR),$(addprefix -I,$(incdir)))
SYSFLAGS=$(foreach incdir,$(SYS_INCDIR),$(addprefix -I,$(incdir)))
INCFLAGS += $(VIDEO_INC)
