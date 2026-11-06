IOP_BIN ?= udpfs_smap.irx
IOP_SRC_DIR := src/
IOP_OBJS_DIR ?= obj/
IOP_CFLAGS += -mno-check-zero-division
IOP_INCS := -Iinclude -Isrc
# Keep the export table ahead of _retonly for PS2SDK iopfixup.
IOP_OBJS = exports.o main.o smap.o xfer.o imports.o

all:: $(IOP_BIN)

# Older SDK rules create the object directory only for build-imports.c.
# exports.o is first, so its generated source needs the directory too.
$(IOP_OBJS_DIR):
	mkdir -p $@
$(IOP_OBJS_DIR)build-exports.c $(IOP_OBJS_DIR)build-imports.c: | $(IOP_OBJS_DIR)

# The SDK's implicit rules do not track included headers or ABI tables.
UDPFS_OBJ_DEPS := $(wildcard include/*.h) src/imports.lst src/exports.tab Makefile
$(addprefix $(IOP_OBJS_DIR),$(IOP_OBJS)): $(UDPFS_OBJ_DEPS)

clean::
	rm -f $(IOP_BIN)
	rm -rf $(IOP_OBJS_DIR)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
