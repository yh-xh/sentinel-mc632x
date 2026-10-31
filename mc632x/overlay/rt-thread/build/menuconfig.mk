MCONF_DIR := $(SDKROOT)/build/kconfig-frontends
MCONF := $(MCONF_DIR)/kconfig-mconf

TOOLS_DIR := $(SDKROOT)/build/tools/
MEDIA_DIR := $(SDKROOT)/app/sentinel/

menuconfig: menuprepare
	$(QUIET)$(MCONF) build/Kconfig
	$(QUIET)if [ ! -e $(OUT)/rtconfig ]; then mkdir -p $(OUT)/rtconfig; fi
	$(QUIET)python $(TOOLS_DIR)/cfg2hdr.py .config $(OUT)/rtconfig/rtconfig.h
	$(QUIET)python ./build/tools/mkscript.py
	$(QUIET)python $(TOOLS_DIR)/extract_id.py merge $(SDKROOT)/.config $(MEDIA_DIR)/Makefile

appconfig: menuprepare
	@echo "*"$(SAMPLE_DIR)"*"
	$(QUIET)cd $(SAMPLE_DIR);make menuconfig OS=RTT;cd -

fastconfig: menuprepare
	$(QUIET)cd $(SDKROOT)/platform/$(SOC_ID_LOWER)/fastboot/;make menuconfig;cd -

menuprepare:
	$(QUIET)if [ ! -e $(MCONF) ]; then make -C $(MCONF_DIR); fi

.PHONY: menuconfig menuprepare
