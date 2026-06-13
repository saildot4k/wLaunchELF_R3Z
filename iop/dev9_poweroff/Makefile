IOP_BIN = dev9_poweroff.irx
IOP_OBJS = main.o imports.o

IOP_CFLAGS += -Wall
IOP_INCS += -I.

all: $(IOP_BIN)

clean:
	-rm -f $(IOP_OBJS) $(IOP_BIN) $(IOP_BIN:.irx=.notiopmod.elf) $(IOP_BIN:.irx=.notiopmod.stripped.elf)

include $(PS2SDK)/Defs.make
include ../Rules.make
