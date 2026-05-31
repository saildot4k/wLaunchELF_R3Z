# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id$


IOP_CC_VERSION := $(shell $(IOP_CC) -dumpversion 2>/dev/null)

ASFLAGS_TARGET = -mcpu=r3000

ifeq ($(IOP_CC_VERSION),3.2.2)
ASFLAGS_TARGET = -march=r3000
endif

ifeq ($(IOP_CC_VERSION),3.2.3)
ASFLAGS_TARGET = -march=r3000
endif

DEBUG ?= 0
ifneq ($(DEBUG), 0)
  $(info -- $(IOP_BIN): compiling with debug level $(DEBUG))
  IOP_CFLAGS += -DDEBUG=$(DEBUG)
else
.SILENT:
endif

IOP_INCS := $(IOP_INCS) -I$(PS2SDK)/iop/include -I$(PS2SDK)/common/include -Iinclude

IOP_CFLAGS  := -D_IOP -fno-builtin -O2 -G0 $(IOP_INCS) $(IOP_CFLAGS)
ifneq ($(IOP_CC_VERSION),3.2.2)
ifneq ($(IOP_CC_VERSION),3.2.3)
IOP_CFLAGS += -msoft-float -mno-explicit-relocs
IOP_IETABLE_CFLAGS := -fno-toplevel-reorder
endif
endif
IOP_ASFLAGS := $(ASFLAGS_TARGET) -EL -G0 $(IOP_ASFLAGS)

IOP_LDFLAGS_USER := $(IOP_LDFLAGS)
IOP_USE_SRXFIXUP := 0

ifneq ($(IOP_CC_VERSION),3.2.2)
ifneq ($(IOP_CC_VERSION),3.2.3)
ifeq ($(strip $(IOP_LINKFILE)),)
ifneq ($(wildcard $(PS2SDKSRC)/iop/startup/src/linkfile),)
IOP_LINKFILE := $(PS2SDKSRC)/iop/startup/src/linkfile
else ifneq ($(wildcard $(PS2SDK)/iop/startup/src/linkfile),)
IOP_LINKFILE := $(PS2SDK)/iop/startup/src/linkfile
endif
endif

ifeq ($(strip $(IOP_SRXFIXUP_CMD)),)
ifneq ($(wildcard $(PS2SDK)/bin/srxfixup),)
IOP_SRXFIXUP_CMD := $(PS2SDK)/bin/srxfixup
else ifneq ($(wildcard $(PS2SDKSRC)/tools/srxfixup),)
IOP_SRXFIXUP_CMD := $(PS2SDKSRC)/tools/srxfixup/bin/srxfixup
IOP_SRXFIXUP_DEP := $(IOP_SRXFIXUP_CMD)
IOP_SRXFIXUP_SRC_DIR := $(PS2SDKSRC)/tools/srxfixup
else ifneq ($(wildcard $(PS2SDK)/tools/srxfixup),)
IOP_SRXFIXUP_CMD := $(PS2SDK)/tools/srxfixup/bin/srxfixup
IOP_SRXFIXUP_DEP := $(IOP_SRXFIXUP_CMD)
IOP_SRXFIXUP_SRC_DIR := $(PS2SDK)/tools/srxfixup
else
IOP_SRXFIXUP_CMD := $(shell command -v srxfixup 2>/dev/null)
endif
endif

ifneq ($(strip $(IOP_LINKFILE)),)
ifneq ($(strip $(IOP_SRXFIXUP_CMD)),)
IOP_USE_SRXFIXUP := 1
endif
endif
endif
endif

ifeq ($(IOP_USE_SRXFIXUP),1)
IOP_LDFLAGS := -nostdlib -dc -r $(filter-out -s,$(IOP_LDFLAGS_USER))
else
IOP_LDFLAGS := -nostdlib $(IOP_LDFLAGS_USER)
ifneq ($(IOP_CC_VERSION),3.2.2)
ifneq ($(IOP_CC_VERSION),3.2.3)
$(error Modern IOP compiler detected, but srxfixup/linkfile was not found. Set PS2SDKSRC or install srxfixup to build loadable IRX modules)
endif
endif
endif

BIN2C = $(PS2SDK)/bin/bin2c
BIN2S = $(PS2SDK)/bin/bin2s
BIN2O = $(PS2SDK)/bin/bin2o

# Externally defined variables: IOP_BIN, IOP_OBJS, IOP_LIB

%.o : %.c
	@echo IOPCC $@
	@$(IOP_CC) $(IOP_CFLAGS) -c $< -o $@

%.o : %.S
	@echo IOPCC $@
	@$(IOP_CC) $(IOP_CFLAGS) $(IOP_INCS) -c $< -o $@

%.o : %.s
	@echo IOPAS $@
	@$(IOP_AS) $(IOP_ASFLAGS) $< -o $@

#Rules for the PS2SDK-styled projects (objects in objs/, binary in bin/)
$(IOP_OBJS_DIR)%.o : $(IOP_SRC_DIR)%.c
	@echo IOPCC $@
	@$(IOP_CC) $(IOP_CFLAGS) -c $< -o $@

$(IOP_OBJS_DIR)%.o : $(IOP_SRC_DIR)%.S
	@echo IOPCC $@
	@$(IOP_CC) $(IOP_CFLAGS) $(IOP_INCS) -c $< -o $@

$(IOP_OBJS_DIR)%.o : $(IOP_SRC_DIR)%.s
	@echo IOPAS $@
	@$(IOP_AS) $(IOP_ASFLAGS) $< -o $@

# A rule to build imports.lst.
$(IOP_OBJS_DIR)%.o : $(IOP_SRC_DIR)%.lst
	@echo IMPORT LIST
	@$(ECHO) "#include \"irx_imports.h\"" > $(IOP_OBJS_DIR)build-imports.c
	@cat $< >> $(IOP_OBJS_DIR)build-imports.c
	@$(IOP_CC) $(IOP_CFLAGS) $(IOP_IETABLE_CFLAGS) -I$(IOP_SRC_DIR) -c $(IOP_OBJS_DIR)build-imports.c -o $@
	@-rm -f $(IOP_OBJS_DIR)build-imports.c

# A rule to build exports.tab.
$(IOP_OBJS_DIR)%.o : $(IOP_SRC_DIR)%.tab
	@echo EXPORT TAB
	@$(ECHO) "#include \"irx.h\"" > $(IOP_OBJS_DIR)build-exports.c
	@cat $< >> $(IOP_OBJS_DIR)build-exports.c
	@$(IOP_CC) $(IOP_CFLAGS) $(IOP_IETABLE_CFLAGS) -I$(IOP_SRC_DIR) -c $(IOP_OBJS_DIR)build-exports.c -o $@
	@-rm -f $(IOP_OBJS_DIR)build-exports.c

$(IOP_OBJS_DIR):
	$(MKDIR) -p $(IOP_OBJS_DIR)

$(IOP_BIN_DIR):
	$(MKDIR) -p $(IOP_BIN_DIR)

$(IOP_LIB_DIR):
	$(MKDIR) -p $(IOP_LIB_DIR)

# A rule to build imports.lst.
%.o : %.lst
	@echo IMPORT LIST
	@$(ECHO) "#include \"irx_imports.h\"" > build-imports.c
	@cat $< >> build-imports.c
	@$(IOP_CC) $(IOP_CFLAGS) $(IOP_IETABLE_CFLAGS) -I. -c build-imports.c -o $@
	@-rm -f build-imports.c

# A rule to build exports.tab.
%.o : %.tab
	@echo EXPORT TAB
	@$(ECHO) "#include \"irx.h\"" > build-exports.c
	@cat $< >> build-exports.c
	@$(IOP_CC) $(IOP_CFLAGS) $(IOP_IETABLE_CFLAGS) -I. -c build-exports.c -o $@
	@-rm -f build-exports.c

ifeq ($(IOP_USE_SRXFIXUP),1)
IOP_BIN_ELF := $(IOP_BIN:.irx=.notiopmod.elf)
IOP_BIN_STRIPPED_ELF := $(IOP_BIN:.irx=.notiopmod.stripped.elf)
IOP_SRXFIXUP_FLAGS := --rb --irx1
ifeq ($(filter exports.o,$(notdir $(IOP_OBJS))),)
IOP_SRXFIXUP_FLAGS += --allow-zero-text
endif

.INTERMEDIATE: $(IOP_BIN_ELF) $(IOP_BIN_STRIPPED_ELF)

ifneq ($(strip $(IOP_SRXFIXUP_SRC_DIR)),)
$(IOP_SRXFIXUP_CMD): $(IOP_SRXFIXUP_SRC_DIR)
	$(MAKE) -C $<
endif

$(IOP_BIN_ELF): $(IOP_OBJS)
	$(IOP_CC) $(IOP_CFLAGS) -T$(IOP_LINKFILE) -o $@ $(IOP_OBJS) $(IOP_LDFLAGS) $(IOP_LIBS)

$(IOP_BIN_STRIPPED_ELF): $(IOP_BIN_ELF)
	$(IOP_STRIP) --strip-unneeded --remove-section=.pdr --remove-section=.comment --remove-section=.mdebug.abi32 --remove-section=.gnu.attributes -o $@ $<

$(IOP_BIN): $(IOP_BIN_STRIPPED_ELF) $(IOP_SRXFIXUP_DEP)
	$(IOP_SRXFIXUP_CMD) $(IOP_SRXFIXUP_FLAGS) -o $@ $<
ifneq (__,_$(IOP_BIN_DIR)_)
	@if [ "$(notdir $(IOP_BIN))" = "$(IOP_BIN)" ]; then cp $(IOP_BIN) $(IOP_BIN_DIR); fi
endif
else
$(IOP_BIN): $(IOP_OBJS)
	$(IOP_CC) $(IOP_CFLAGS) -o $@ $(IOP_OBJS) $(IOP_LDFLAGS) $(IOP_LIBS)
ifneq (__,_$(IOP_BIN_DIR)_)
	cp $(IOP_BIN) $(IOP_BIN_DIR)
endif
endif

$(IOP_LIB): $(IOP_OBJS)
	$(IOP_AR) cru $< $(IOP_OBJS)
