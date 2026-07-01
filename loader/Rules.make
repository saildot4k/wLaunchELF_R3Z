# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.

EE_CC_VERSION := $(shell $(EE_CC) --version 2>&1 | sed -n 's/^.*(GCC) //p')

# Include directories
EE_INCS := -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I. $(EE_INCS)

# C compiler flags
EE_CFLAGS := -D_EE -O2 -G0 -Wall $(EE_CFLAGS)

# C++ compiler flags
EE_CXXFLAGS := -D_EE -O2 -G0 -Wall $(EE_CXXFLAGS)

# Linker flags
EE_LDFLAGS := -L$(PS2SDK)/ee/lib $(EE_LDFLAGS)

# Helpers for compact embedded binaries. This mirrors the PS2SDK rules used by
# OSDMenu while keeping this legacy local rules file self-contained.
EE_NEWLIB_NANO ?= 0
EE_COMPACT_EXECUTABLE ?= 0

ifneq (x$(EE_COMPACT_EXECUTABLE), x0)
EE_CFLAGS += -fdata-sections -ffunction-sections
EE_CXXFLAGS += -fdata-sections -ffunction-sections
EE_LDFLAGS += -Wl,-zmax-page-size=128 -s -Wl,--gc-sections
endif

# Assembler flags
EE_ASFLAGS := -G0 $(EE_ASFLAGS)

# Linker script path changed across PS2SDK layouts.
# Prefer installed release path, then source-tree path.
EE_LINKFILE ?= $(PS2SDK)/ee/startup/linkfile
ifeq ($(wildcard $(EE_LINKFILE)),)
EE_LINKFILE := $(PS2SDK)/ee/startup/src/linkfile
endif

# Link with following libraries. Some PS2SDK builds no longer ship
# libkernel-nopatch, so fall back to libkernel when needed.
EE_KERNEL_LIB := -lkernel-nopatch
ifeq ($(wildcard $(PS2SDK)/ee/lib/libkernel-nopatch.a),)
EE_KERNEL_LIB := -lkernel
endif
ifneq (x$(EE_NEWLIB_NANO), x0)
EE_LDFLAGS += -nodefaultlibs -lm_nano -lgcc -Wl,--start-group -lc_nano $(EE_KERNEL_LIB) -Wl,--end-group
else
EE_LIBS += -lc $(EE_KERNEL_LIB)
endif

# Externally defined variables: EE_BIN, EE_OBJS, EE_LIB

# These macros can be used to simplify certain build rules.
EE_C_COMPILE = $(EE_CC) $(EE_CFLAGS) $(EE_INCS)
EE_CXX_COMPILE = $(EE_CXX) $(EE_CXXFLAGS) $(EE_INCS)

# Extra macro for disabling the automatic inclusion of the built-in CRT object(s)
ifeq ($(EE_CC_VERSION),3.2.2)
	EE_NO_CRT = -mno-crt0
else ifeq ($(EE_CC_VERSION),3.2.3)
	EE_NO_CRT = -mno-crt0
else
	EE_NO_CRT = -nostartfiles
	CRTBEGIN_OBJ := $(shell $(EE_CC) $(CFLAGS) -print-file-name=crtbegin.o)
	CRTEND_OBJ := $(shell $(EE_CC) $(CFLAGS) -print-file-name=crtend.o)
	CRTI_OBJ := $(shell $(EE_CC) $(CFLAGS) -print-file-name=crti.o)
	CRTN_OBJ := $(shell $(EE_CC) $(CFLAGS) -print-file-name=crtn.o)
endif

%.o: %.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

%.o: %.cc
	$(EE_CXX) $(EE_CXXFLAGS) $(EE_INCS) -c $< -o $@

%.o: %.cpp
	$(EE_CXX) $(EE_CXXFLAGS) $(EE_INCS) -c $< -o $@

%.o: %.S
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

%.o: %.s
	$(EE_AS) $(EE_ASFLAGS) $< -o $@

$(EE_BIN): $(EE_OBJS)
	$(EE_CC) -T$(EE_LINKFILE) $(EE_CFLAGS) \
		-o $(EE_BIN) $(EE_OBJS) $(EE_LDFLAGS) $(EE_LIBS)

$(EE_ERL): $(EE_OBJS)
	$(EE_CC) $(EE_NO_CRT) -o $(EE_ERL) $(EE_OBJS) $(EE_CFLAGS) $(EE_LDFLAGS) -Wl,-r -Wl,-d
	$(EE_STRIP) --strip-unneeded -R .mdebug.eabi64 -R .reginfo -R .comment $(EE_ERL)

$(EE_LIB): $(EE_OBJS)
	$(EE_AR) cru $(EE_LIB) $(EE_OBJS)
