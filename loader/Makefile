EE_CFLAGS  := -G0 -Os -EL -Wall -D_EE
EE_ASFLAGS += -EL -G0

# Match OSDMenu's compact embedded-loader build so the image fits in the
# 0x84000..0xa0000 low-memory window.
EE_NEWLIB_NANO ?= 1
EE_COMPACT_EXECUTABLE ?= 1

# Match OSDMenu's embedded loader placement: reserve 0x80000..0x81500
# and link the loader body into the BIOS-unused 0x84000..0xa0000 window.
LOADADDR  = 0x84000
STACKADDR = auto
STACKSIZE = auto

# OSDMenu's bram window ends at 0xa0000; the loader image and auto stack must
# fit below that boundary.
LOW_MEM_LIMIT = 0xa0000
LAYOUT_CHECK_SCRIPT = ../scripts/check_loader_layout.sh
EE_NM ?= $(EE_TOOL_PREFIX)nm
EE_LINKFILE := linkfile

EE_LDFLAGS += -s

EE_BIN = loader.elf

EE_OBJS = loader.o

EE_LIBS =
ifeq ($(DEBUG),1)
EE_LIBS += -ldebug
endif

all: $(EE_BIN) check-layout

check-layout: $(EE_BIN)
	sh $(LAYOUT_CHECK_SCRIPT) "$(EE_BIN)" "$(EE_NM)" "$(LOADADDR)" "$(STACKADDR)" "$(STACKSIZE)" "$(LOW_MEM_LIMIT)"

clean:
	rm -f -r $(EE_OBJS) $(EE_BIN)

include $(PS2SDK)/Defs.make
include Rules.make
