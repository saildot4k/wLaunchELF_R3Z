//--------------------------------------------------------------
//File name:    loader.c
//--------------------------------------------------------------
//dlanor: This subprogram has been modified to minimize the code
//dlanor: size of the resident loader portion. Some of the parts
//dlanor: that were moved into the main program include loading
//dlanor: of all IRXs and mounting pfs0: for ELFs on hdd.
//dlanor: Another change was to skip threading in favor of ExecPS2
/*==================================================================
==											==
==	Copyright(c)2004  Adam Metcalf(gamblore_@hotmail.com)		==
==	Copyright(c)2004  Thomas Hawcroft(t0mb0la@yahoo.com)		==
==	This file is subject to terms and conditions shown in the	==
==	file LICENSE which should be kept in the top folder of	==
==	this distribution.							==
==											==
==	Portions of this code taken from PS2Link:				==
==				pkoLoadElf						==
==				wipeUserMemory					==
==				(C) 2003 Tord Lindstrom (pukko@home.se)	==
==				(C) 2003 adresd (adresd_ps2dev@yahoo.com)	==
==	Portions of this code taken from Independence MC exploit	==
==				tLoadElf						==
==				LoadAndRunHDDElf					==
==				(C) 2003 Marcus Brown <mrbrown@0xd6.org>	==
==											==
==================================================================*/
#include "tamtypes.h"
#include "debug.h"
#include "kernel.h"
#include "iopcontrol.h"
#include "sifrpc.h"
#include "loadfile.h"
#include "sio.h"
#include "string.h"
#include "iopheap.h"
#include "errno.h"
//--------------------------------------------------------------

#define POPSTARTER_LOAD_ADDR_VALUE 0x00100000
#define POPSTARTER_MAX_PAYLOAD_SIZE 0x400000
#define POPSTARTER_BUFFER_MODE "-popstarter-buffer"

//--------------------------------------------------------------
//End of data declarations
//--------------------------------------------------------------
//Start of function code:
//--------------------------------------------------------------
// Clear user memory
// PS2Link (C) 2003 Tord Lindstrom (pukko@home.se)
//         (C) 2003 adresd (adresd_ps2dev@yahoo.com)
//--------------------------------------------------------------
static void wipeUserMem(void)
{
	int i;
	for (i = 0x100000; i < GetMemorySize(); i += 64) {
		asm volatile(
		    "\tsq $0, 0(%0) \n"
		    "\tsq $0, 16(%0) \n"
		    "\tsq $0, 32(%0) \n"
		    "\tsq $0, 48(%0) \n" ::"r"(i));
	}
}

static void wle_log(const char *msg)
{
	sio_putsn(msg);
}

static unsigned int parseHexArg(const char *text)
{
	unsigned int value = 0;
	unsigned char c;

	if (text == NULL)
		return 0;
	if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
		text += 2;

	while ((c = (unsigned char)*text++) != '\0') {
		value <<= 4;
		if (c >= '0' && c <= '9')
			value |= c - '0';
		else if (c >= 'a' && c <= 'f')
			value |= c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			value |= c - 'A' + 10;
		else
			return 0;
	}

	return value;
}

static int runPopstarterBuffer(int argc, char *argv[])
{
	static char popstarter_arg[1025];
	static char *args[1] = {popstarter_arg};
	unsigned int payload_addr;
	unsigned int payload_size;
	void *payload;

	if (argc < 4)
		return -EINVAL;

	payload_addr = parseHexArg(argv[1]);
	payload_size = parseHexArg(argv[2]);
	if (payload_addr == 0 || payload_size == 0 || payload_size >= POPSTARTER_MAX_PAYLOAD_SIZE)
		return -EINVAL;

	payload = (void *)payload_addr;
	strncpy(popstarter_arg, argv[3], sizeof(popstarter_arg) - 1);
	popstarter_arg[sizeof(popstarter_arg) - 1] = '\0';

	memmove((void *)POPSTARTER_LOAD_ADDR_VALUE, payload, payload_size);
	if (*(u32 *)POPSTARTER_LOAD_ADDR_VALUE != 0x0804000C)
		return -EINVAL;

	SifExitRpc();
	FlushCache(0);
	FlushCache(2);

	ExecPS2((void *)POPSTARTER_LOAD_ADDR_VALUE, 0, 1, args);
	return 0;
}

//--------------------------------------------------------------
//End of func:  void wipeUserMem(void)
//--------------------------------------------------------------
// *** MAIN ***
//--------------------------------------------------------------
int main(int argc, char *argv[])
{
	static t_ExecData elfdata;
	char *target, *path;
	char *args[1];
	int ret, rebootiop = 0, prefer_encrypted = 0;
	u32 loader_epc;
	int popstarter_buffer_mode;

	// Initialize
	SifInitRpc(0);
	wle_log("# wle: loader start\n");
	popstarter_buffer_mode = (argc >= 4 && !strcmp(argv[0], POPSTARTER_BUFFER_MODE));
	/*
	 * In DEBUG builds this loader can be linked above 0x100000.
	 * Avoid wiping memory in that case, or we may erase the currently
	 * running loader image before SifLoadElf executes.
	 */
	loader_epc = (u32)&main;
	if (loader_epc < 0x100000 && !popstarter_buffer_mode)
		wipeUserMem();

	if (popstarter_buffer_mode)
		return runPopstarterBuffer(argc, argv);

	if (argc < 2) {  // arg1=path to ELF, arg2=partition to mount
		wle_log("# wle: argc < 2\n");
		SifExitRpc();
		return -EINVAL;
	}

	target = argv[0];
	path = argv[1];
	if (argc > 2) {
		if (!strcmp("-r", argv[2])) {
			rebootiop = 1;
		} else if (!strcmp("-nr", argv[2])) {
			rebootiop = 0;
		} else if (!strcmp("-er", argv[2])) {
			prefer_encrypted = 1;
			rebootiop = 1;
		} else if (!strcmp("-enr", argv[2])) {
			prefer_encrypted = 1;
			rebootiop = 0;
		}
	}

	//Writeback data cache before loading ELF.
	FlushCache(0);
	SifLoadFileInit();

	memset(&elfdata, 0, sizeof(elfdata));
	wle_log("# wle: try target\n");
	if (prefer_encrypted) {
		ret = SifLoadElfEncrypted(target, &elfdata);
		wle_log("# wle: tried SifLoadElfEncrypted\n");
		if (ret != 0 || elfdata.epc == 0)
			ret = SifLoadElf(target, &elfdata);
		wle_log("# wle: tried SifLoadElf\n");
	} else {
		ret = SifLoadElf(target, &elfdata);
		wle_log("# wle: tried SifLoadElf\n");
		if (ret != 0 || elfdata.epc == 0) {
			ret = SifLoadElfEncrypted(target, &elfdata);
			wle_log("# wle: tried SifLoadElfEncrypted\n");
		}
	}

	if (!(ret == 0 && elfdata.epc != 0 && (elfdata.epc & 0x3) == 0))
		ret = -ENOENT;
	SifLoadFileExit();

	if (ret == 0) {
		args[0] = path;
		///ISRA: based on config
		/* if (strncmp(path, "hdd", 3) == 0 && (path[3] >= '0' && path[3] <= ':')) { Final IOP reset, to fill the IOP with the default modules.
	               It appears that it was once a thing for the booting software to leave the IOP with the required IOP modules.
	               This can be seen in OSDSYS v1.0x (no IOP reboot) and the mechanism to boot DVD player updates (OSDSYS will get LoadExecPS2 to load SIO2 modules).
	               However, it changed with the introduction of the HDD unit, as the software booted may be built with a different SDK revision.

	               Reboot the IOP, to leave it in a clean & consistent state.
	               But do not do that for boot targets on other devices, for backward-compatibility with older (homebrew) software.
		} */
		if (rebootiop) {
			wle_log("# wle: rst iop\n");
			while (!SifIopReset("", 0));
			while (!SifIopSync());
		}

		SifExitRpc();

		FlushCache(0);
		FlushCache(2);

		ExecPS2((void *)elfdata.epc, (void *)elfdata.gp, 1, args);
		wle_log("# wle: post ExecPS2\n");
		return 0;
	} else {
		wle_log("# wle: SifLoadElf fail\n");
		SifExitRpc();
		return -ENOENT;
	}
}
//--------------------------------------------------------------
//End of func:  int main(int argc, char *argv[])
//--------------------------------------------------------------
//End of file:  loader.c
//--------------------------------------------------------------
