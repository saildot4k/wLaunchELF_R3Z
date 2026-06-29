EE_CFLAGS  := -G0 -O2 -EL -Wall -D_EE
EE_ASFLAGS += -EL -G0

# Match OSDMenu's embedded loader placement: reserve 0x80000..0x81500
# and link the loader body into the BIOS-unused 0x84000..0x100000 window.
LOADADDR  = 0x84000
STACKADDR = auto
STACKSIZE = auto

# EE kernel occupies first MiB. For low-loader mode, keep image below this.
LOW_MEM_LIMIT = 0x100000
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
