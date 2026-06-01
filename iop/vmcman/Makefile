# wLaunchELF-owned VMC manager module.
#
# This is built from a vendored PS2SDK mcman/vmcman source snapshot so the
# app no longer depends on live PS2SDK source changes for VMC behavior.

PS2SDKSRC ?= $(PS2SDK)

ifeq ($(strip $(PS2SDKSRC)),)
$(error Set PS2SDKSRC or PS2SDK to a PS2SDK source tree)
endif

IOP_BIN ?= vmcman.irx
IOP_SRC_DIR ?= src/
IOP_INC_DIR ?= include/

MCMAN_BUILDING_XMCMAN ?= 1
MCMAN_BUILDING_XMCMAN_V2 ?= 1
MCMAN_BUILDING_XFROMMAN ?= 0
MCMAN_BUILDING_VMCMAN ?= 1
MCMAN_BUILDING_DONGLEMAN ?= 0

IOP_IMPORT_INCS += \
	cdvd/cdvdman \
	debug/sior \
	security/secrman \
	sio/sio2man \
	system/intrman \
	system/ioman \
	system/iomanx \
	system/loadcore \
	system/modload \
	system/stdio \
	system/sysclib \
	system/threadman \
	system/timrman

IOP_OBJS = \
	exports.o \
	main.o \
	mcdev.o \
	mciomanx_backing.o \
	mcsio2.o \
	ps2mc_fio.o \
	ps1mc_fio.o \
	imports.o

ifneq (x$(MCMAN_BUILDING_XMCMAN),x0)
IOP_CFLAGS += -DBUILDING_XMCMAN
endif

ifneq (x$(MCMAN_BUILDING_XMCMAN_V2),x0)
IOP_CFLAGS += -DSIO2MAN_V2
endif

ifneq (x$(MCMAN_BUILDING_XFROMMAN),x0)
IOP_CFLAGS += -DBUILDING_XFROMMAN
IOP_IMPORT_INCS += dev9/extflash
endif

ifneq (x$(MCMAN_BUILDING_VMCMAN),x0)
IOP_CFLAGS += -DBUILDING_VMCMAN
IOP_CFLAGS += -DIOMANX_OLD_NAME_COMPATIBILITY=0
IOP_CFLAGS += -DIOMANX_OLD_NAME_ADDDELDRV=0
endif

ifneq (x$(MCMAN_BUILDING_DONGLEMAN),x0)
IOP_CFLAGS += -DBUILDING_DONGLEMAN
endif

include $(PS2SDKSRC)/Defs.make
include $(PS2SDKSRC)/iop/Rules.bin.make
include $(PS2SDKSRC)/iop/Rules.make
include $(PS2SDKSRC)/iop/Rules.release
