IOP_BIN = ps2hdd-osd.irx
IOP_OBJS = hdd_fio.o hdd_blkio.o hdd.o imports.o password.o misc.o cache.o apa.o journal.o free.o

IOP_CFLAGS += -Wall -DAPA_OSD_VER -DAPA_USE_DEV9 -DAPA_USE_ATAD
IOP_CFLAGS += -DIOMANX_OLD_NAME_COMPATIBILITY=0 -DIOMANX_OLD_NAME_ADDDELDRV=0
IOP_INCS += -I.

all: $(IOP_BIN)

clean:
	-rm -f $(IOP_OBJS) $(IOP_BIN) $(IOP_BIN:.irx=.notiopmod.elf) $(IOP_BIN:.irx=.notiopmod.stripped.elf)

include $(PS2SDK)/Defs.make
include ../Rules.make
