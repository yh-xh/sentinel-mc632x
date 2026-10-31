export ROOT := $(shell pwd)
export SDKROOT := $(shell pwd)/../..

CONFIG_PATH = .config
ifneq (.config, $(wildcard .config))
CONFIG_PATH = .def.config
endif

-include $(CONFIG_PATH)
-include $(SDKROOT)/build/config.mk

export CONFIG_ARCH_$(CHIP_ID) = y
CFLAGS += -D__RTTHREAD_OS__
CFLAGS += -Wall -Werror

A7_CPU_FLAGS = -mcpu=cortex-a7
A7_VFP_FLAGS = -mfloat-abi=hard -mfpu=neon-vfpv4

ARM_CORTEXA7_VFP ?= 1

ifeq ($(DEBUG),1)
CFLAGS += -g
endif

LIBS = -ldbi_rtt -lm -ladvapi_osd -ladvapi_isp -ladvapi_smartir -ladvapi_md -ladvapi

#sample_demo start
SRCS = $(wildcard startup/demo_entry_rtt.c)
SRCS += $(wildcard vlcview/*.c)

#kernel include
INCS = -I./
INCS += -I$(SDKROOT)/kernel/include/

#driver include src
INCS = -I./
INCS += -I$(SDKROOT)/lib/inc
INCS += -I$(SDKROOT)/lib/inc/advapi
INCS += -I$(SDKROOT)/lib/inc/mpp
INCS += -I$(SDKROOT)/lib/inc/types
INCS += -I$(SDKROOT)/lib/$(CHIP_ID_LOWER)/inc
INCS += -I$(SDKROOT)/lib/$(CHIP_ID_LOWER)/inc/dsp
INCS += -I$(SDKROOT)/lib/$(CHIP_ID_LOWER)/inc/dsp_ext
INCS += -I$(SDKROOT)/lib/$(CHIP_ID_LOWER)/inc/isp
INCS += -I$(SDKROOT)/lib/$(CHIP_ID_LOWER)/inc/isp_ext
INCS += -I$(SDKROOT)/lib/$(CHIP_ID_LOWER)/inc/vicap
# DFS / rtdevice（snap_store 用来判断 SD 是否真的挂在 /mnt）
INCS += -I$(SDKROOT)/kernel/include
INCS += -I$(SDKROOT)/shell
INCS += -I$(SDKROOT)/out/rtconfig
INCS += -I$(SDKROOT)/fs/include
INCS += -I$(SDKROOT)/drivers/arch
INCS += -I$(SDKROOT)/compat/include

#components include src
COMPONENTS_DIR = components
INCS += -I$(COMPONENTS_DIR)
ifeq ($(FH_APP_USING_COOLVIEW), y)
SRCS += $(wildcard components/libdbi_over_tcp_rtt/src/*.c)
SRCS += $(wildcard components/libdbi_over_usb_rtt/src/*.c)
SRCS += $(wildcard $(SAMPLE_DEMO_DIR)/coolview/src/*.c)
endif

SRCS += $(wildcard components/libdmc/src/libdmc.c)
SRCS += $(wildcard components/libmisc/src/*.c)

FH_APP_USING_MJPEG = n
ifneq (, $(findstring y, $(CH0_MJPEG_G0) $(CH1_MJPEG_G0) $(CH2_MJPEG_G0) $(CH3_MJPEG_G0)))
FH_APP_USING_MJPEG = y
else ifneq (, $(findstring y, $(CH0_MJPEG_G1) $(CH1_MJPEG_G1) $(CH2_MJPEG_G1) $(CH3_MJPEG_G1)))
FH_APP_USING_MJPEG = y
else ifneq (, $(findstring y, $(CH0_MJPEG_G2) $(CH1_MJPEG_G2) $(CH2_MJPEG_G2) $(CH3_MJPEG_G2)))
FH_APP_USING_MJPEG = y
else ifneq (, $(findstring y, $(CH0_MJPEG_G3) $(CH1_MJPEG_G3) $(CH2_MJPEG_G3) $(CH3_MJPEG_G3)))
FH_APP_USING_MJPEG = y
endif

ifeq ($(FH_APP_USING_MJPEG), y)
SRCS += $(wildcard components/libmjpeg_over_http/src/*.c)
SRCS += $(wildcard components/libdmc/src/libdmc_http_mjpeg.c)
CFLAGS += -DFH_MJPEG_ENABLE
endif

FH_APP_USING_PES = n
ifeq ($(FH_APP_USING_PES_G0), y)
FH_APP_USING_PES = y
else ifeq ($(FH_APP_USING_PES_G1), y)
FH_APP_USING_PES = y
else ifeq ($(FH_APP_USING_PES_G2), y)
FH_APP_USING_PES = y
else ifeq ($(FH_APP_USING_PES_G3), y)
FH_APP_USING_PES = y
endif

ifeq ($(FH_APP_USING_PES), y)
SRCS += $(wildcard components/libpes/src/*.c)
SRCS += $(wildcard components/libdmc/src/libdmc_pes.c)
CFLAGS += -DFH_PES_ENABLE
endif

SRCS += $(wildcard components/librect_merge_by_gaus/src/*.c)

FH_APP_USING_RTSP = n
ifeq ($(FH_APP_USING_RTSP_G0), y)
FH_APP_USING_RTSP = y
else ifeq ($(FH_APP_USING_RTSP_G1), y)
FH_APP_USING_RTSP = y
else ifeq ($(FH_APP_USING_RTSP_G2), y)
FH_APP_USING_RTSP = y
else ifeq ($(FH_APP_USING_RTSP_G3), y)
FH_APP_USING_RTSP = y
endif

ifeq ($(FH_APP_USING_RTSP), y)
SRCS += $(wildcard components/librtsp/src/*.c)
SRCS += $(wildcard components/libdmc/src/libdmc_rtsp.c)
CFLAGS += -DFH_RTSP_ENABLE
endif

FH_APP_USING_RECORD = n
ifeq ($(FH_APP_RECORD_RAW_STREAM_G0), y)
FH_APP_USING_RECORD = y
else ifeq ($(FH_APP_RECORD_RAW_STREAM_G1), y)
FH_APP_USING_RECORD = y
else ifeq ($(FH_APP_RECORD_RAW_STREAM_G2), y)
FH_APP_USING_RECORD = y
else ifeq ($(FH_APP_RECORD_RAW_STREAM_G3), y)
FH_APP_USING_RECORD = y
endif

ifeq ($(FH_APP_USING_RECORD), y)
SRCS += $(wildcard components/libdmc/src/libdmc_record_raw.c)
CFLAGS += -DFH_RECORD_ENABLE
endif

SRCS += $(wildcard components/libsample_common/src/*.c)
SRCS += $(wildcard components/libscaler/src/*.c)

#sample_demo include src
SAMPLE_DEMO_DIR = sample_demo
INCS += -I$(SAMPLE_DEMO_DIR)
SRCS += $(wildcard $(SAMPLE_DEMO_DIR)/*.c)
SRCS += $(wildcard $(SAMPLE_DEMO_DIR)/stream/src/*.c)

INCS += -I$(CHIP_DIR)/uvc/include
ifeq ($(UVC_ENABLE), y)
SRCS += $(wildcard $(CHIP_DIR)/uvc/src/*.c)
SRCS += $(wildcard $(SAMPLE_DEMO_DIR)/uvc_rtt/src/*.c)
ifeq ($(USB_AUDIO_ENABLE), y)
SRCS += $(wildcard components/libusb_rtt/usb_audio/src/*.c)
endif
ifeq ($(USB_HID_ENABLE), y)
SRCS += $(wildcard components/libusb_rtt/usb_hid/src/*.c)
endif
SRCS += $(wildcard components/libusb_rtt/usb_update/src/*.c)
ifeq ($(USB_CDC_ENABLE), y)
SRCS += $(wildcard components/libusb_rtt/usb_vcom/src/*.c)
endif
SRCS += $(wildcard components/libusb_rtt/usb_video/src/*.c)
endif

#sample_common include src
SAMPLE_COMMON_DIR = sample_common
CHIP_DIR = $(SAMPLE_COMMON_DIR)/$(CHIP_ID)
DSP_DIR = $(CHIP_DIR)/dsp
ISP_DIR = $(CHIP_DIR)/isp
DSP_MODEL_DIR = $(shell find $(DSP_DIR) -maxdepth 1 -type d)
ISP_MODEL_DIR = $(shell find $(ISP_DIR) -maxdepth 1 -type d)
DSP_INC = $(foreach dir,$(DSP_MODEL_DIR),-I$(dir)/include)
ISP_INC = $(foreach dir,$(ISP_MODEL_DIR),-I$(dir)/include)
DSP_SRC = $(foreach dir,$(DSP_MODEL_DIR),$(wildcard $(dir)/src/*.c))
ISP_SRC = $(foreach dir,$(ISP_MODEL_DIR),$(wildcard $(dir)/src/*.c))
INCS += -I$(SAMPLE_COMMON_DIR) -I$(CHIP_DIR) -I$(DSP_DIR) -I$(ISP_DIR) $(DSP_INC) $(ISP_INC)
SRCS += $(wildcard $(SAMPLE_COMMON_DIR)/*.c) $(wildcard $(CHIP_DIR)/*.c) $(wildcard $(DSP_DIR)/*.c) $(wildcard $(ISP_DIR)/*.c) $(DSP_SRC) $(ISP_SRC)

export FH_SLT_TEST = y
ifeq ($(FH_SLT_TEST), y)
INCS += -I$(SAMPLE_COMMON_DIR)/slt_test/include
SRCS += $(wildcard $(SAMPLE_COMMON_DIR)/slt_test/src/*.c)
endif

-include script/rtt/chip/$(CHIP_ID)/media.mk

ifeq ($(ARM_CORTEXA7_VFP), 1)
LDFLAGS += $(A7_CPU_FLAGS) $(A7_VFP_FLAGS)
CFLAGS += $(A7_CPU_FLAGS) $(A7_VFP_FLAGS)
endif

CFLAGS += $(INCS)

SAMPNAME := $(shell basename `pwd`)
SAMP_SRCS := $(SRCS)
LOCAL_CFLAGS := $(CFLAGS)
LOCAL_LIBS := $(LIBS)

include $(SDKROOT)/build/apps.mk

all:

sensor_hex:
	@chmod +x script/rtt/sensor_hex_rtt.py
	@script/rtt/sensor_hex_rtt.py config.h $(CHIP_ID)
	@chmod 644 script/rtt/sensor_hex_rtt.py

clean:
	rm -f $(BIN)
	find . -name \*.o -exec rm -rf {} \;
	find . -name \*.d -exec rm -rf {} \;

clean_hex:
	@if [ -e sample_common/all_sensor_array.h ]; then rm -f sample_common/all_sensor_array.h; fi
	@if [ -e sample_common/all_sensor_hex.bin ]; then rm -f sample_common/all_sensor_hex.bin; fi

distclean: clean clean_hex
	-rm config.h
	-rm .config
	-rm .config.old

%_defconfig:
	@if [ -e script/rtt/chip/$(CHIP_ID)/defconfig/$@ ];then \
		cp script/rtt/chip/$(CHIP_ID)/defconfig/$@ .config; \
	else \
		@echo "$@ not exists"; \
	fi
	@python script/rtt/menu_anno.py .config config.h $(CHIP_ID)
	@rm -f .config.old .config.cmd

menuconfig:
	@chmod +x script/rtt/mconf
	@script/rtt/mconf script/rtt/Kconfig
	@python script/rtt/menu_anno.py .config config.h $(CHIP_ID)
	@rm -f .config.old .config.cmd
	@chmod +x script/rtt/sensor_hex_rtt.py
	@script/rtt/sensor_hex_rtt.py config.h $(CHIP_ID) $(SDKROOT)
	@chmod 644 script/rtt/sensor_hex_rtt.py
	
.PHONY: clean

