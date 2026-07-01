//--------------------------------------------------------------
// File name: loader.c
//--------------------------------------------------------------
// Reduced embedded loader for wLaunchELF R3Z.
// Based on the OSDMenu embeddable loader design, with wLE-specific
// argv handoff and without GSM, PATINFO, DEV9, title ID, or CNF support.

#include "tamtypes.h"
#include "kernel.h"
#include "iopcontrol.h"
#include "sifrpc.h"
#include "loadfile.h"
#include "string.h"
#include "errno.h"
#include <elf.h>

#define USER_MEM_START_ADDR 0x100000

void _libcglue_init(void)
{
}

void _libcglue_deinit(void)
{
}

void _libcglue_args_parse(int argc, char **argv)
{
	(void)argc;
	(void)argv;
}

static int parseHex8(const char *text, u32 *value)
{
	u32 out;
	int i;

	if (text == NULL || value == NULL)
		return -EINVAL;

	out = 0;
	for (i = 0; i < 8; i++) {
		char c = text[i];
		u32 digit;

		if (c >= '0' && c <= '9')
			digit = c - '0';
		else if (c >= 'A' && c <= 'F')
			digit = c - 'A' + 10;
		else if (c >= 'a' && c <= 'f')
			digit = c - 'a' + 10;
		else
			return -EINVAL;

		out = (out << 4) | digit;
	}

	*value = out;
	return 0;
}

static int parseMemPath(const char *path, u32 *addr, u32 *size)
{
	if (path == NULL || strncmp(path, "mem:", 4) != 0)
		return -EINVAL;
	if (parseHex8(path + 4, addr) < 0)
		return -EINVAL;
	if (path[12] != ':')
		return -EINVAL;
	if (parseHex8(path + 13, size) < 0)
		return -EINVAL;
	return 0;
}

static void clearUserMem(void)
{
	u32 addr, end;

	end = GetMemorySize();
	for (addr = USER_MEM_START_ADDR; addr < end; addr += 64) {
		asm volatile(
		    "\tsq $0, 0(%0) \n"
		    "\tsq $0, 16(%0) \n"
		    "\tsq $0, 32(%0) \n"
		    "\tsq $0, 48(%0) \n" ::"r"(addr));
	}
}

static void clearUserMemPreserving(u32 preserve_addr, u32 preserve_size)
{
	u32 end, preserve_end;

	end = GetMemorySize();
	if (preserve_addr < USER_MEM_START_ADDR || preserve_addr >= end || preserve_size == 0) {
		clearUserMem();
		return;
	}

	preserve_end = preserve_addr + preserve_size;
	if (preserve_end < preserve_addr || preserve_end > end)
		preserve_end = end;

	if (preserve_addr > USER_MEM_START_ADDR)
		memset((void *)USER_MEM_START_ADDR, 0, preserve_addr - USER_MEM_START_ADDR);
	if (preserve_end < end)
		memset((void *)preserve_end, 0, end - preserve_end);
}

static int loadELFImage(u32 elf_addr, u32 *entry)
{
	Elf32_Ehdr *eh;
	Elf32_Phdr *ph;
	void *src;
	int i;

	if (entry == NULL)
		return -EINVAL;

	eh = (Elf32_Ehdr *)elf_addr;
	if (memcmp(eh->e_ident, ELFMAG, SELFMAG) != 0)
		return -ENOEXEC;
	if ((eh->e_type != ET_EXEC) && (eh->e_type != ET_DYN))
		return -ENOEXEC;
	if (eh->e_machine != EM_MIPS)
		return -ENOEXEC;
	if (eh->e_entry == 0 || (eh->e_entry & 0x3) != 0)
		return -ENOEXEC;

	ph = (Elf32_Phdr *)(elf_addr + eh->e_phoff);
	for (i = 0; i < eh->e_phnum; i++) {
		if (ph[i].p_type != PT_LOAD)
			continue;

		src = (void *)(elf_addr + ph[i].p_offset);
		memcpy((void *)ph[i].p_vaddr, src, ph[i].p_filesz);
		if (ph[i].p_memsz > ph[i].p_filesz)
			memset((void *)(ph[i].p_vaddr + ph[i].p_filesz), 0, ph[i].p_memsz - ph[i].p_filesz);
	}

	*entry = eh->e_entry;
	FlushCache(0);
	FlushCache(2);
	return 0;
}

static void resetIOP(void)
{
	while (!SifIopReset("", 0)) {
	}
	while (!SifIopSync()) {
	}
	SifInitRpc(0);
}

static int loadEmbeddedPayload(char *elf_path, int argc, char *argv[], int reset_iop)
{
	u32 elf_addr, elf_size, entry;
	int ret;

	ret = parseMemPath(elf_path, &elf_addr, &elf_size);
	if (ret < 0)
		return ret;
	if (elf_addr < USER_MEM_START_ADDR || elf_size == 0)
		return -EINVAL;

	clearUserMemPreserving(elf_addr, elf_size);

	if (reset_iop)
		resetIOP();

	ret = loadELFImage(elf_addr, &entry);
	if (ret < 0) {
		entry = USER_MEM_START_ADDR;
		memcpy((void *)entry, (void *)elf_addr, elf_size);
		FlushCache(0);
		FlushCache(2);
	}

	SifExitRpc();
	ExecPS2((void *)entry, NULL, argc, argv);
	return 0;
}

static int loadFilePayload(char *elf_path, int argc, char *argv[], int reset_iop)
{
	static t_ExecData elfdata;
	int ret;

	clearUserMem();
	FlushCache(0);

	memset(&elfdata, 0, sizeof(elfdata));
	SifLoadFileInit();
	ret = SifLoadElf(elf_path, &elfdata);
	if (ret != 0 || elfdata.epc == 0)
		ret = SifLoadElfEncrypted(elf_path, &elfdata);
	SifLoadFileExit();

	if (ret != 0 || elfdata.epc == 0 || (elfdata.epc & 0x3) != 0)
		return -ENOENT;

	FlushCache(0);
	FlushCache(2);

	if (reset_iop)
		resetIOP();

	SifExitRpc();
	ExecPS2((void *)elfdata.epc, (void *)elfdata.gp, argc, argv);
	return 0;
}

int main(int argc, char *argv[])
{
	char *elf_path;
	int reset_iop, skip_argv0;

	if (argc < 1)
		return -EINVAL;

	SifInitRpc(0);

	elf_path = NULL;
	reset_iop = 0;
	skip_argv0 = 0;

	if (argc > 0 && !strncmp(argv[argc - 1], "-la=", 4)) {
		char *flags;
		int i;

		flags = argv[argc - 1] + 4;
		for (i = 0; flags[i] != '\0'; i++) {
			switch (flags[i]) {
			case 'R':
				reset_iop = 1;
				break;
			case 'E':
				if (argc < 2)
					return -EINVAL;
				elf_path = argv[argc - 2];
				argc--;
				break;
			case 'A':
				skip_argv0 = 1;
				break;
			default:
				break;
			}
		}
		argc--;
	}

	if (elf_path == NULL)
		elf_path = argv[0];

	if (skip_argv0) {
		if (argc < 1)
			return -EINVAL;
		argc--;
		argv = &argv[1];
	}

	if (!strncmp(elf_path, "mem:", 4))
		return loadEmbeddedPayload(elf_path, argc, argv, reset_iop);

	return loadFilePayload(elf_path, argc, argv, reset_iop);
}

//--------------------------------------------------------------
// End of file: loader.c
//--------------------------------------------------------------
