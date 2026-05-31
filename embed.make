ifeq ($(SIO2MAN),0) # no custom SIO2 stack? use ps2dev:1.0 drivers
  $(info using ps2dev:1.0 mc drivers)
  MCMAN_SOURCE = $(PS2SDK)/iop/irx/mcman.irx
  MCSERV_SOURCE = $(PS2SDK)/iop/irx/mcserv.irx
  SIO2MAN_SOURCE = $(PS2SDK)/iop/irx/sio2man.irx
else # custom SIO2 stack (MMCE/MX4SIO/manual): use newer IRX to avoid deadlocks
  $(info using latest mc drivers (prefer PS2SDK IRX, fallback bundled))
  MCMAN_SOURCE = iop/__precompiled/mcman.irx
  MCSERV_SOURCE = iop/__precompiled/mcserv.irx
  SIO2MAN_SOURCE = iop/__precompiled/sio2man.irx
  ifneq ($(wildcard $(PS2SDK)/iop/irx/mcman.irx),)
    MCMAN_SOURCE = $(PS2SDK)/iop/irx/mcman.irx
  endif
  ifneq ($(wildcard $(PS2SDK)/iop/irx/mcserv.irx),)
    MCSERV_SOURCE = $(PS2SDK)/iop/irx/mcserv.irx
  endif
  ifneq ($(wildcard $(PS2SDK)/iop/irx/sio2man.irx),)
    SIO2MAN_SOURCE = $(PS2SDK)/iop/irx/sio2man.irx
  endif
endif

PADMAN_SOURCE = $(PS2SDK)/iop/irx/padman.irx
ifneq ($(wildcard iop/__precompiled/padman.irx),)
PADMAN_SOURCE = iop/__precompiled/padman.irx
endif

# Prefer newer storage stack modules:
# 1) installed PS2SDK IRX
# 2) auto-build from PS2SDK sources
# 3) bundled fallback IRX
BDM_SOURCE :=
BDM_AUTOGEN := iop/__generated/bdm.irx
BDM_SDK_ROOT :=
BDM_SDK_MODULE_DIR :=

BDMFS_FATFS_SOURCE :=
BDMFS_FATFS_AUTOGEN := iop/__generated/bdmfs_fatfs.irx
BDMFS_FATFS_SDK_ROOT :=
BDMFS_FATFS_SDK_MODULE_DIR :=

USBMASS_BD_SOURCE :=
USBMASS_BD_AUTOGEN := iop/__generated/usbmass_bd.irx
USBMASS_BD_SDK_ROOT :=
USBMASS_BD_SDK_MODULE_DIR :=

IOMANX_SOURCE := iop/__precompiled/iomanX.irx
FILEXIO_SOURCE := iop/__precompiled/fileXio.irx
VMCMAN_SOURCE :=
VMCMAN_AUTOGEN := iop/__generated/vmcman.irx
VMCMAN_SDK_ROOT :=
VMCMAN_SDK_MODULE_DIR :=
VMCMAN_PATCH_DIR := iop/__generated/vmcman_patched
VMCMAN_PATCH_SRC_DIR := $(VMCMAN_PATCH_DIR)/src
VMCMAN_PATCH_INC_DIR := $(VMCMAN_PATCH_DIR)/include
VMCMAN_PATCH_STAMP := $(VMCMAN_PATCH_DIR)/.patched

ifneq ($(wildcard $(PS2SDK)/iop/irx/bdm.irx),)
BDM_SOURCE := $(PS2SDK)/iop/irx/bdm.irx
endif
ifneq ($(wildcard $(PS2SDK)/iop/irx/bdmfs_fatfs.irx),)
BDMFS_FATFS_SOURCE := $(PS2SDK)/iop/irx/bdmfs_fatfs.irx
endif
ifneq ($(wildcard $(PS2SDK)/iop/irx/usbmass_bd.irx),)
USBMASS_BD_SOURCE := $(PS2SDK)/iop/irx/usbmass_bd.irx
endif

ifeq ($(strip $(BDM_SOURCE)),)
ifneq ($(PS2SDKSRC),)
ifneq ($(wildcard $(PS2SDKSRC)/iop/fs/bdm/Makefile),)
BDM_SDK_ROOT := $(PS2SDKSRC)
BDM_SDK_MODULE_DIR := $(PS2SDKSRC)/iop/fs/bdm
endif
endif
endif

ifeq ($(strip $(BDM_SOURCE)),)
ifneq ($(wildcard $(PS2SDK)/iop/fs/bdm/Makefile),)
BDM_SDK_ROOT := $(PS2SDK)
BDM_SDK_MODULE_DIR := $(PS2SDK)/iop/fs/bdm
endif
endif

ifeq ($(strip $(BDM_SOURCE)),)
ifneq ($(strip $(BDM_SDK_MODULE_DIR)),)
BDM_SOURCE := $(BDM_AUTOGEN)
endif
endif

ifeq ($(strip $(BDM_SOURCE)),)
ifneq ($(wildcard iop/__precompiled/bdm.irx),)
BDM_SOURCE := iop/__precompiled/bdm.irx
endif
endif

ifeq ($(strip $(BDM_SOURCE)),)
$(error Missing bdm.irx. Update PS2SDK, add iop/__precompiled/bdm.irx, or provide PS2SDKSRC with iop/fs/bdm sources)
endif

ifeq ($(strip $(BDMFS_FATFS_SOURCE)),)
ifneq ($(PS2SDKSRC),)
ifneq ($(wildcard $(PS2SDKSRC)/iop/fs/bdmfs_fatfs/Makefile),)
BDMFS_FATFS_SDK_ROOT := $(PS2SDKSRC)
BDMFS_FATFS_SDK_MODULE_DIR := $(PS2SDKSRC)/iop/fs/bdmfs_fatfs
endif
endif
endif

ifeq ($(strip $(BDMFS_FATFS_SOURCE)),)
ifneq ($(wildcard $(PS2SDK)/iop/fs/bdmfs_fatfs/Makefile),)
BDMFS_FATFS_SDK_ROOT := $(PS2SDK)
BDMFS_FATFS_SDK_MODULE_DIR := $(PS2SDK)/iop/fs/bdmfs_fatfs
endif
endif

ifeq ($(strip $(BDMFS_FATFS_SOURCE)),)
ifneq ($(strip $(BDMFS_FATFS_SDK_MODULE_DIR)),)
BDMFS_FATFS_SOURCE := $(BDMFS_FATFS_AUTOGEN)
endif
endif

ifeq ($(strip $(BDMFS_FATFS_SOURCE)),)
ifneq ($(wildcard iop/__precompiled/bdmfs_fatfs.irx),)
BDMFS_FATFS_SOURCE := iop/__precompiled/bdmfs_fatfs.irx
endif
endif

ifeq ($(strip $(BDMFS_FATFS_SOURCE)),)
$(error Missing bdmfs_fatfs.irx. Update PS2SDK, add iop/__precompiled/bdmfs_fatfs.irx, or provide PS2SDKSRC with iop/fs/bdmfs_fatfs sources)
endif

ifeq ($(strip $(USBMASS_BD_SOURCE)),)
ifneq ($(PS2SDKSRC),)
ifneq ($(wildcard $(PS2SDKSRC)/iop/usb/usbmass_bd/Makefile),)
USBMASS_BD_SDK_ROOT := $(PS2SDKSRC)
USBMASS_BD_SDK_MODULE_DIR := $(PS2SDKSRC)/iop/usb/usbmass_bd
endif
endif
endif

ifeq ($(strip $(USBMASS_BD_SOURCE)),)
ifneq ($(wildcard $(PS2SDK)/iop/usb/usbmass_bd/Makefile),)
USBMASS_BD_SDK_ROOT := $(PS2SDK)
USBMASS_BD_SDK_MODULE_DIR := $(PS2SDK)/iop/usb/usbmass_bd
endif
endif

ifeq ($(strip $(USBMASS_BD_SOURCE)),)
ifneq ($(strip $(USBMASS_BD_SDK_MODULE_DIR)),)
USBMASS_BD_SOURCE := $(USBMASS_BD_AUTOGEN)
endif
endif

ifeq ($(strip $(USBMASS_BD_SOURCE)),)
ifneq ($(wildcard iop/__precompiled/usbmass_bd.irx),)
USBMASS_BD_SOURCE := iop/__precompiled/usbmass_bd.irx
endif
endif

ifeq ($(strip $(USBMASS_BD_SOURCE)),)
$(error Missing usbmass_bd.irx. Update PS2SDK, add iop/__precompiled/usbmass_bd.irx, or provide PS2SDKSRC with iop/usb/usbmass_bd sources)
endif

ifneq ($(wildcard $(PS2SDK)/iop/irx/iomanX.irx),)
IOMANX_SOURCE := $(PS2SDK)/iop/irx/iomanX.irx
endif
ifneq ($(wildcard $(PS2SDK)/iop/irx/fileXio.irx),)
FILEXIO_SOURCE := $(PS2SDK)/iop/irx/fileXio.irx
endif

ifneq ($(PS2SDKSRC),)
ifneq ($(wildcard $(PS2SDKSRC)/iop/memorycard/vmcman/Makefile),)
VMCMAN_SDK_ROOT := $(PS2SDKSRC)
VMCMAN_SDK_MODULE_DIR := $(PS2SDKSRC)/iop/memorycard/vmcman
endif
endif

ifeq ($(strip $(VMCMAN_SDK_MODULE_DIR)),)
ifneq ($(wildcard $(PS2SDK)/iop/memorycard/vmcman/Makefile),)
VMCMAN_SDK_ROOT := $(PS2SDK)
VMCMAN_SDK_MODULE_DIR := $(PS2SDK)/iop/memorycard/vmcman
endif
endif

ifneq ($(strip $(VMCMAN_SDK_MODULE_DIR)),)
VMCMAN_SOURCE := $(VMCMAN_AUTOGEN)
endif

ifeq ($(strip $(VMCMAN_SOURCE)),)
$(error Missing vmcman sources. Set PS2SDKSRC to a ps2sdk checkout with iop/memorycard/vmcman; prebuilt vmcman.irx is not supported because wLaunchELF patches its backing geometry)
endif



#---{ MC }---#
$(EE_ASM_DIR)mcman_irx.s: $(MCMAN_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ mcman_irx

$(EE_ASM_DIR)mcserv_irx.s: $(MCSERV_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ mcserv_irx

$(EE_ASM_DIR)sio2man.s: $(SIO2MAN_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ sio2man_irx

# MX4SIO_BD module selection:
# 1) installed PS2SDK IRX (preferred)
# 2) auto-build from PS2SDK sources (when available)
# 3) bundled fallback IRX
MX4SIO_BD_SOURCE :=
MX4SIO_BD_AUTOGEN := iop/__generated/mx4sio_bd.irx
MX4SIO_BD_SDK_ROOT :=
MX4SIO_BD_SDK_MODULE_DIR :=

ifneq ($(wildcard $(PS2SDK)/iop/irx/mx4sio_bd.irx),)
MX4SIO_BD_SOURCE := $(PS2SDK)/iop/irx/mx4sio_bd.irx
endif

ifeq ($(strip $(MX4SIO_BD_SOURCE)),)
ifneq ($(PS2SDKSRC),)
ifneq ($(wildcard $(PS2SDKSRC)/iop/sio/mx4sio_bd/Makefile),)
MX4SIO_BD_SDK_ROOT := $(PS2SDKSRC)
MX4SIO_BD_SDK_MODULE_DIR := $(PS2SDKSRC)/iop/sio/mx4sio_bd
endif
endif
endif

ifeq ($(strip $(MX4SIO_BD_SOURCE)),)
ifneq ($(wildcard $(PS2SDK)/iop/sio/mx4sio_bd/Makefile),)
MX4SIO_BD_SDK_ROOT := $(PS2SDK)
MX4SIO_BD_SDK_MODULE_DIR := $(PS2SDK)/iop/sio/mx4sio_bd
endif
endif

ifeq ($(strip $(MX4SIO_BD_SOURCE)),)
ifneq ($(strip $(MX4SIO_BD_SDK_MODULE_DIR)),)
MX4SIO_BD_SOURCE := $(MX4SIO_BD_AUTOGEN)
endif
endif

ifeq ($(strip $(MX4SIO_BD_SOURCE)),)
ifneq ($(wildcard iop/__precompiled/mx4sio_bd.irx),)
MX4SIO_BD_SOURCE := iop/__precompiled/mx4sio_bd.irx
endif
endif

ifeq ($(strip $(MX4SIO_BD_SOURCE)),)
$(error Missing mx4sio_bd.irx. Update PS2SDK, add iop/__precompiled/mx4sio_bd.irx, or provide PS2SDKSRC with iop/sio/mx4sio_bd sources)
endif

$(MX4SIO_BD_AUTOGEN): | iop/__generated
	$(MAKE) -C $(MX4SIO_BD_SDK_MODULE_DIR) \
		PS2SDKSRC=$(MX4SIO_BD_SDK_ROOT) \
		PS2SDK=$(MX4SIO_BD_SDK_ROOT) \
		IOP_BIN_DIR=$(abspath iop/__generated)/ \
		IOP_OBJS_DIR=$(abspath iop/__generated/mx4sio_bd_obj)/ \
		IOP_BIN=mx4sio_bd.irx

$(EE_ASM_DIR)mx4sio_bd.s: $(MX4SIO_BD_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ mx4sio_bd_irx
	
$(EE_ASM_DIR)mmceman_irx.s: iop/__precompiled/mmceman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ mmceman_irx
 
$(EE_ASM_DIR)extflash_irx.s: iop/__precompiled/extflash.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ extflash_irx

$(EE_ASM_DIR)xfromman_irx.s: iop/__precompiled/xfromman.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ xfromman_irx

#---{ USB }---#

$(EE_ASM_DIR)usbd_irx.s: $(PS2SDK)/iop/irx/usbd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ usbd_irx
ifeq ($(EXFAT),1)
$(BDM_AUTOGEN): | iop/__generated
	$(MAKE) -C $(BDM_SDK_MODULE_DIR) \
		PS2SDKSRC=$(BDM_SDK_ROOT) \
		PS2SDK=$(BDM_SDK_ROOT) \
		IOP_BIN_DIR=$(abspath iop/__generated)/ \
		IOP_OBJS_DIR=$(abspath iop/__generated/bdm_obj)/ \
		IOP_BIN=bdm.irx

$(BDMFS_FATFS_AUTOGEN): | iop/__generated
	$(MAKE) -C $(BDMFS_FATFS_SDK_MODULE_DIR) \
		PS2SDKSRC=$(BDMFS_FATFS_SDK_ROOT) \
		PS2SDK=$(BDMFS_FATFS_SDK_ROOT) \
		IOP_BIN_DIR=$(abspath iop/__generated)/ \
		IOP_OBJS_DIR=$(abspath iop/__generated/bdmfs_fatfs_obj)/ \
		IOP_BIN=bdmfs_fatfs.irx

$(USBMASS_BD_AUTOGEN): | iop/__generated
	$(MAKE) -C $(USBMASS_BD_SDK_MODULE_DIR) \
		PS2SDKSRC=$(USBMASS_BD_SDK_ROOT) \
		PS2SDK=$(USBMASS_BD_SDK_ROOT) \
		IOP_BIN_DIR=$(abspath iop/__generated)/ \
		IOP_OBJS_DIR=$(abspath iop/__generated/usbmass_bd_obj)/ \
		IOP_BIN=usbmass_bd.irx

$(EE_ASM_DIR)bdm_irx.s:$(BDM_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ bdm_irx

$(EE_ASM_DIR)bdmfs_fatfs_irx.s:$(BDMFS_FATFS_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ bdmfs_fatfs_irx

$(EE_ASM_DIR)usbmass_bd_irx.s:$(USBMASS_BD_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ usbmass_bd_irx

# ATA BDM module is not present in older PS2SDK releases.
# Preferred order:
# 1) installed PS2SDK IRX
# 2) bundled fallback IRX
# 3) auto-build from PS2SDK sources (when available)
ATA_BD_SOURCE :=
ATA_BD_AUTOGEN := iop/__generated/ata_bd.irx
ATA_BD_SDK_ROOT :=
ATA_BD_SDK_MODULE_DIR :=

ifneq ($(wildcard $(PS2SDK)/iop/irx/ata_bd.irx),)
ATA_BD_SOURCE := $(PS2SDK)/iop/irx/ata_bd.irx
endif

ifeq ($(strip $(ATA_BD_SOURCE)),)
ifneq ($(wildcard iop/__precompiled/ata_bd.irx),)
ATA_BD_SOURCE := iop/__precompiled/ata_bd.irx
endif
endif

ifeq ($(strip $(ATA_BD_SOURCE)),)
ifneq ($(PS2SDKSRC),)
ifneq ($(wildcard $(PS2SDKSRC)/iop/dev9/ata_bd/Makefile),)
ATA_BD_SDK_ROOT := $(PS2SDKSRC)
ATA_BD_SDK_MODULE_DIR := $(PS2SDKSRC)/iop/dev9/ata_bd
endif
endif
endif

ifeq ($(strip $(ATA_BD_SOURCE)),)
ifneq ($(wildcard $(PS2SDK)/iop/dev9/ata_bd/Makefile),)
ATA_BD_SDK_ROOT := $(PS2SDK)
ATA_BD_SDK_MODULE_DIR := $(PS2SDK)/iop/dev9/ata_bd
endif
endif

ifeq ($(strip $(ATA_BD_SOURCE)),)
ifneq ($(strip $(ATA_BD_SDK_MODULE_DIR)),)
ATA_BD_SOURCE := $(ATA_BD_AUTOGEN)
endif
endif

ifeq ($(strip $(ATA_BD_SOURCE)),)
$(error Missing ata_bd.irx. Update PS2SDK, add iop/__precompiled/ata_bd.irx, or provide PS2SDKSRC with iop/dev9/ata_bd sources)
endif

$(ATA_BD_AUTOGEN): | iop/__generated
	$(MAKE) -C $(ATA_BD_SDK_MODULE_DIR) \
		PS2SDKSRC=$(ATA_BD_SDK_ROOT) \
		PS2SDK=$(ATA_BD_SDK_ROOT) \
		IOP_BIN_DIR=$(abspath iop/__generated)/ \
		IOP_OBJS_DIR=$(abspath iop/__generated/ata_bd_obj)/ \
		IOP_BIN=ata_bd.irx

$(EE_ASM_DIR)ata_bd_irx.s:$(ATA_BD_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ata_bd_irx
else
$(EE_ASM_DIR)usbhdfsd_irx.s: $(PS2SDK)/iop/irx/usbhdfsd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ usb_mass_irx
endif

# ----- #

iop/cdvd.irx: iop/oldlibs/libcdvd
	$(MAKE) -C $<

$(EE_ASM_DIR)cdvd_irx.s: $(CDVD_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ cdvd_irx

$(EE_ASM_DIR)ioptrap_irx.s: $(PS2SDK)/iop/irx/ioptrap.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ioptrap_irx

$(EE_ASM_DIR)poweroff_irx.s: $(PS2SDK)/iop/irx/poweroff.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ poweroff_irx

$(EE_ASM_DIR)iomanx_irx.s: $(IOMANX_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ iomanx_irx

$(EE_ASM_DIR)filexio_irx.s: $(FILEXIO_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ filexio_irx

iop/__generated:
	mkdir -p $@

$(VMCMAN_PATCH_STAMP): scripts/patch_vmcman_backing.awk | iop/__generated
	rm -rf $(VMCMAN_PATCH_DIR)
	mkdir -p $(VMCMAN_PATCH_SRC_DIR) $(VMCMAN_PATCH_INC_DIR)
	cp -R $(VMCMAN_SDK_ROOT)/iop/memorycard/mcman/src/. $(VMCMAN_PATCH_SRC_DIR)/
	cp -R $(VMCMAN_SDK_ROOT)/iop/memorycard/mcman/include/. $(VMCMAN_PATCH_INC_DIR)/
	awk -f scripts/patch_vmcman_backing.awk \
		$(VMCMAN_PATCH_SRC_DIR)/mciomanx_backing.c > $(VMCMAN_PATCH_SRC_DIR)/mciomanx_backing.c.tmp
	mv $(VMCMAN_PATCH_SRC_DIR)/mciomanx_backing.c.tmp $(VMCMAN_PATCH_SRC_DIR)/mciomanx_backing.c
	grep -q 'cardinfo->mounted = 1;' $(VMCMAN_PATCH_SRC_DIR)/mciomanx_backing.c
	grep -q 'vmcman: mount request' $(VMCMAN_PATCH_SRC_DIR)/mciomanx_backing.c
	grep -q 'superblock.pages_per_cluster \* superblock.clusters_per_card' $(VMCMAN_PATCH_SRC_DIR)/mciomanx_backing.c
	touch $@

$(VMCMAN_AUTOGEN): $(VMCMAN_PATCH_STAMP)
	$(MAKE) -C $(VMCMAN_SDK_MODULE_DIR) \
		PS2SDKSRC=$(VMCMAN_SDK_ROOT) \
		PS2SDK=$(VMCMAN_SDK_ROOT) \
		IOP_SRC_DIR=$(abspath $(VMCMAN_PATCH_SRC_DIR))/ \
		IOP_INC_DIR=$(abspath $(VMCMAN_PATCH_INC_DIR))/ \
		IOP_BIN_DIR=$(abspath iop/__generated)/ \
		IOP_OBJS_DIR=$(abspath iop/__generated/vmcman_obj)/ \
		IOP_BIN=vmcman.irx

$(EE_ASM_DIR)vmcman_irx.c: $(VMCMAN_SOURCE) scripts/bin2c-fallback.sh | $(EE_ASM_DIR)
	sh scripts/bin2c-fallback.sh $< $@ vmcman_irx

$(EE_OBJS_DIR)vmcman_irx.o: $(EE_ASM_DIR)vmcman_irx.c | $(EE_OBJS_DIR)
	@echo -e "\033[1m CC  - $@\033[0m"
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_ASM_DIR)ps2dev9_irx.s: $(PS2SDK)/iop/irx/ps2dev9.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2dev9_irx

ifeq ($(ETH),1)
$(EE_ASM_DIR)ps2ip_irx.s: $(PS2SDK)/iop/irx/ps2ip.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2ip_irx

$(EE_ASM_DIR)udptty.s: $(PS2SDK)/iop/irx/udptty.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ udptty_irx

# ETH stack policy:
# Preserve historical wLaunchELF SMAP preference/order first, then fallback
# to current PS2SDK variants.
PS2SMAP_SOURCE :=
ifneq ($(wildcard $(PS2DEV)/ps2eth/smap/ps2smap.irx),)
PS2SMAP_SOURCE := $(PS2DEV)/ps2eth/smap/ps2smap.irx
else ifneq ($(wildcard iop/__precompiled/ps2smap.irx),)
PS2SMAP_SOURCE := iop/__precompiled/ps2smap.irx
else ifneq ($(wildcard $(PS2SDK)/iop/irx/ps2smap.irx),)
PS2SMAP_SOURCE := $(PS2SDK)/iop/irx/ps2smap.irx
else ifneq ($(wildcard $(PS2SDK)/iop/irx/smap-ps2ip.irx),)
PS2SMAP_SOURCE := $(PS2SDK)/iop/irx/smap-ps2ip.irx
endif

ifeq ($(strip $(PS2SMAP_SOURCE)),)
$(error Missing SMAP driver. Provide $(PS2DEV)/ps2eth/smap/ps2smap.irx, iop/__precompiled/ps2smap.irx, or PS2SDK smap modules.)
endif

$(EE_ASM_DIR)ps2smap_irx.s: $(PS2SMAP_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2smap_irx

$(EE_ASM_DIR)ps2ftpd_irx.s: iop/ps2ftpd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2ftpd_irx

$(EE_ASM_DIR)ps2netfs_irx.s: $(PS2SDK)/iop/irx/ps2netfs.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2netfs_irx

$(EE_ASM_DIR)ps2host_irx.s: iop/ps2host.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2host_irx
endif

ifeq ($(UDPFS),1)
UDPFS_THIRDPARTY_ROOT := thirdparty/nhddl-legacy/iop/udpfs
UDPFS_SMAP_SOURCE :=
UDPFS_MINISTACK_SOURCE :=
UDPFS_IOMAN_SOURCE :=

ifneq ($(wildcard $(UDPFS_THIRDPARTY_ROOT)/smap/Makefile),)
UDPFS_SMAP_SOURCE := iop/__generated/udpfs_smap.irx
endif
ifneq ($(wildcard $(UDPFS_THIRDPARTY_ROOT)/ministack/Makefile),)
UDPFS_MINISTACK_SOURCE := iop/__generated/udpfs_ministack.irx
endif
ifneq ($(wildcard $(UDPFS_THIRDPARTY_ROOT)/udpfs/Makefile),)
UDPFS_IOMAN_SOURCE := iop/__generated/udpfs_ioman.irx
endif

ifeq ($(strip $(UDPFS_SMAP_SOURCE)),)
ifneq ($(wildcard iop/__precompiled/udpfs_smap.irx),)
UDPFS_SMAP_SOURCE := iop/__precompiled/udpfs_smap.irx
endif
endif
ifeq ($(strip $(UDPFS_MINISTACK_SOURCE)),)
ifneq ($(wildcard iop/__precompiled/udpfs_ministack.irx),)
UDPFS_MINISTACK_SOURCE := iop/__precompiled/udpfs_ministack.irx
endif
endif
ifeq ($(strip $(UDPFS_IOMAN_SOURCE)),)
ifneq ($(wildcard iop/__precompiled/udpfs_ioman.irx),)
UDPFS_IOMAN_SOURCE := iop/__precompiled/udpfs_ioman.irx
endif
endif

ifeq ($(strip $(UDPFS_SMAP_SOURCE)),)
$(error Missing udpfs_smap.irx. Provide $(UDPFS_THIRDPARTY_ROOT)/smap sources or iop/__precompiled/udpfs_smap.irx)
endif
ifeq ($(strip $(UDPFS_MINISTACK_SOURCE)),)
$(error Missing udpfs_ministack.irx. Provide $(UDPFS_THIRDPARTY_ROOT)/ministack sources or iop/__precompiled/udpfs_ministack.irx)
endif
ifeq ($(strip $(UDPFS_IOMAN_SOURCE)),)
$(error Missing udpfs_ioman.irx. Provide $(UDPFS_THIRDPARTY_ROOT)/udpfs sources or iop/__precompiled/udpfs_ioman.irx)
endif

ifneq ($(filter iop/__generated/udpfs_smap.irx,$(UDPFS_SMAP_SOURCE)),)
$(UDPFS_SMAP_SOURCE): | iop/__generated
	$(MAKE) -C $(UDPFS_THIRDPARTY_ROOT)/smap \
		IOP_BIN_DIR=$(abspath iop/__generated)/ \
		IOP_OBJS_DIR=$(abspath iop/__generated/udpfs_smap_obj)/ \
		IOP_BIN=udpfs_smap.irx
endif

ifneq ($(filter iop/__generated/udpfs_ministack.irx,$(UDPFS_MINISTACK_SOURCE)),)
$(UDPFS_MINISTACK_SOURCE): | iop/__generated
	$(MAKE) -C $(UDPFS_THIRDPARTY_ROOT)/ministack \
		IOP_BIN_DIR=$(abspath iop/__generated)/ \
		IOP_OBJS_DIR=$(abspath iop/__generated/udpfs_ministack_obj)/ \
		IOP_BIN=udpfs_ministack.irx
endif

ifneq ($(filter iop/__generated/udpfs_ioman.irx,$(UDPFS_IOMAN_SOURCE)),)
$(UDPFS_IOMAN_SOURCE): | iop/__generated
	$(MAKE) -C $(UDPFS_THIRDPARTY_ROOT)/udpfs UDPFS_IOMAN=1 \
		IOP_BIN_DIR=$(abspath iop/__generated)/ \
		IOP_OBJS_DIR=$(abspath iop/__generated/udpfs_ioman_obj)/ \
		IOP_BIN=udpfs_ioman.irx
endif

$(EE_ASM_DIR)udpfs_smap_irx.s: $(UDPFS_SMAP_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ udpfs_smap_irx

$(EE_ASM_DIR)udpfs_ministack_irx.s: $(UDPFS_MINISTACK_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ udpfs_ministack_irx

$(EE_ASM_DIR)udpfs_ioman_irx.s: $(UDPFS_IOMAN_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ udpfs_ioman_irx
endif

iop/ps2ftpd.irx: iop/oldlibs/ps2ftpd
	$(MAKE) -C $<

$(EE_ASM_DIR)ps2hdd_irx.s: $(PS2SDK)/iop/irx/ps2hdd-osd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2hdd_irx

$(EE_ASM_DIR)ps2fs_irx.s: $(PS2SDK)/iop/irx/ps2fs.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2fs_irx
	
ifeq ($(DVRP),1)
$(EE_ASM_DIR)dvrdrv_irx.s:iop/__precompiled/dvrdrv.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ dvrdrv_irx

$(EE_ASM_DIR)dvrfile_irx.s:iop/__precompiled/dvrfile.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ dvrfile_irx
endif

iop/hdl_info.irx: iop/hdl_info
	$(MAKE) -C $<

$(EE_ASM_DIR)hdl_info_irx.s: iop/hdl_info.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ hdl_info_irx

iop/ps2host.irx: iop/ps2host
	$(MAKE) -C $<

iop/ds34usb/ee/libds34usb.a: iop/ds34usb/ee
	$(MAKE) -C $<

iop/ds34usb.irx: iop/ds34usb/iop
	$(MAKE) -C $<

iop/ds34bt/ee/libds34bt.a: iop/ds34bt/ee
	$(MAKE) -C $<

iop/ds34bt.irx: iop/ds34bt/iop
	$(MAKE) -C $<

$(EE_ASM_DIR)ds34usb.s: iop/ds34usb.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ds34usb_irx

$(EE_OBJS_DIR)libds34usb.a: iop/ds34usb/ee/libds34usb.a
	cp $< $@	

$(EE_OBJS_DIR)libds34bt.a: iop/ds34bt/ee/libds34bt.a
	cp $< $@

$(EE_ASM_DIR)ds34bt.s: iop/ds34bt.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ds34bt_irx

$(EE_ASM_DIR)padman.s: $(PADMAN_SOURCE) | $(EE_ASM_DIR)
	$(BIN2S) $< $@ padman_irx

loader/loader.elf: loader
	$(MAKE) -C $<

$(EE_ASM_DIR)loader_elf.s: loader/loader.elf | $(EE_ASM_DIR)
	$(BIN2S) $< $@ loader_elf

$(EE_ASM_DIR)ps2kbd_irx.s: $(PS2SDK)/iop/irx/ps2kbd.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ps2kbd_irx

$(EE_ASM_DIR)sior_irx.s: $(PS2SDK)/iop/irx/sior.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ sior_irx

$(EE_ASM_DIR)ppctty.s:iop/__precompiled/ppctty.irx | $(EE_ASM_DIR)
	$(BIN2S) $< $@ ppctty_irx

iop/AllowDVDV.irx: iop/AllowDVDV
	$(MAKE) -C $<

$(EE_ASM_DIR)allowdvdv_irx.s: iop/AllowDVDV.irx
	$(BIN2S) $< $@ allowdvdv_irx

ISO_BIN_DIR ?= iso/
ISO_BIN ?= wLE_ISR$(BIN_NAME).iso
ISO_BIN_ELF ?= ISRA_000.00
SYSTEMCNF_VERSION ?= 1.00
SYSTEMCNF_VMODE ?= PAL
ISO_BIN_DUMMYSIZE ?= 64
iso: $(ISO_BIN_DIR) $(ISO_BIN_DIR)system.cnf $(EE_BIN_PKD)
	cp $(EE_BIN_PKD) $(ISO_BIN_DIR)$(ISO_BIN_ELF)
	echo "Extra Build features: $(BIN_NAME)" > "$(ISO_BIN_DIR)BUILD_OPT.TXT"
	dd if=/dev/zero  of="$(ISO_BIN_DIR)DUMMY.BIN"  bs=1M  count=$(ISO_BIN_DUMMYSIZE)
	mkisofs -o "$(ISO_BIN)" "$(ISO_BIN_DIR)"
	$(info ISO generation finished.)
ifeq ($(PACK_ISO), YES)
	$(info ISO will be compressed)
	zip -q -j -9 wLE_ISR_ISO$(BIN_NAME).zip $(ISO_BIN)
endif

isoclean:
	rm -rf $(ISO_BIN_DIR)

$(ISO_BIN_DIR):
	mkdir $@

$(ISO_BIN_DIR)system.cnf: $(ISO_BIN_DIR)
	$(info - generating 'system.cnf' ...)
	echo "BOOT2 = cdrom0:\$(ISO_BIN_ELF);1" >"$@"
	echo VER = $(SYSTEMCNF_VERSION)>>"$@"
	echo VMODE = $(SYSTEMCNF_VMODE)>>"$@"
