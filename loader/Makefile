EE_CFLAGS  := -G0 -O2 -EL -Wall -D_EE
EE_ASFLAGS += -EL -G0

###dlanor: adjust these three, for PS2Link compatibility tests
###dlanor: old LaunchELF used LA=0x90000, SA=0xB0000, SS=0x08000
###dlanor: The values below were implemented in LaunchELF v3.50
LOADADDR  = 0x90000
# Keep stack near top of first MiB while avoiding overlap with current loader.
STACKADDR = 0x0F0000
STACKSIZE = 0x08000

# EE kernel occupies first MiB. For low-loader mode, keep image below this.
LOW_MEM_LIMIT = 0x100000
LAYOUT_CHECK_SCRIPT = ../scripts/check_loader_layout.sh
EE_NM ?= $(EE_TOOL_PREFIX)nm

LDPARAMS := -Wl,--defsym -Wl,_stack_size=$(STACKSIZE) -Wl,--defsym -Wl,_stack=$(STACKADDR)

EE_LDFLAGS += -Wl,-Ttext -Wl,$(LOADADDR) -s $(LDPARAMS)

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
