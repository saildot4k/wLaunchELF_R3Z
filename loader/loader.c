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
#include "stdio.h"
#include "iopheap.h"
#include "errno.h"
//--------------------------------------------------------------

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
	char alt_target[1025];
	const char *targets[2];
	int target_count = 1, target_ix;
	char dbg[192];

	// Initialize
	SifInitRpc(0);
	/*
	 * In DEBUG builds this loader can be linked above 0x100000.
	 * Avoid wiping memory in that case, or we may erase the currently
	 * running loader image before SifLoadElf executes.
	 */
	loader_epc = (u32)&main;
	if (loader_epc < 0x100000)
		wipeUserMem();

	if (argc < 2) {  // arg1=path to ELF, arg2=partition to mount
		sio_puts("# wle: argc < 2\n");
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

	targets[0] = target;
	alt_target[0] = '\0';
	{
		char *sep;
		strncpy(alt_target, target, sizeof(alt_target) - 1);
		alt_target[sizeof(alt_target) - 1] = '\0';
		sep = strchr(alt_target, ':');
		if (sep != NULL && sep[1] != '\0') {
			if (sep[1] == '/' || sep[1] == '\\') {
				memmove(sep + 1, sep + 2, strlen(sep + 2) + 1);
			} else if (strlen(alt_target) + 1 < sizeof(alt_target)) {
				memmove(sep + 2, sep + 1, strlen(sep + 1) + 1);
				sep[1] = '/';
			}
		}
		if (strcmp(alt_target, target) != 0) {
			targets[target_count++] = alt_target;
		}
	}

	//Writeback data cache before loading ELF.
	FlushCache(0);

	ret = -ENOENT;
	for (target_ix = 0; target_ix < target_count; target_ix++) {
		memset(&elfdata, 0, sizeof(elfdata));
		snprintf(dbg, sizeof(dbg), "# wle: try target[%d] %s mode=%s\n", target_ix, targets[target_ix], prefer_encrypted ? "enc-first" : "plain");
		sio_puts(dbg);
		if (prefer_encrypted) {
			ret = SifLoadElfEncrypted(targets[target_ix], &elfdata);
			snprintf(dbg, sizeof(dbg), "# wle: SifLoadElfEncrypted ret=%d epc=0x%08lx gp=0x%08lx\n", ret, (unsigned long)elfdata.epc, (unsigned long)elfdata.gp);
			sio_puts(dbg);
			if (ret != 0)
				ret = SifLoadElf(targets[target_ix], &elfdata);
			snprintf(dbg, sizeof(dbg), "# wle: SifLoadElf ret=%d epc=0x%08lx gp=0x%08lx\n", ret, (unsigned long)elfdata.epc, (unsigned long)elfdata.gp);
			sio_puts(dbg);
		} else {
			/* Normal ELF path: do not use encrypted loader fallback. */
			ret = SifLoadElf(targets[target_ix], &elfdata);
			snprintf(dbg, sizeof(dbg), "# wle: SifLoadElf ret=%d epc=0x%08lx gp=0x%08lx\n", ret, (unsigned long)elfdata.epc, (unsigned long)elfdata.gp);
			sio_puts(dbg);
		}

		if (ret == 0 && elfdata.epc != 0 && (elfdata.epc & 0x3) == 0) {
			target = (char *)targets[target_ix];
			break;
		}

		ret = -ENOENT;
	}

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
			sio_puts("# wle: rst iop\n");
			while (!SifIopReset("", 0));
			while (!SifIopSync());
		}

		SifExitRpc();

		FlushCache(0);
		FlushCache(2);

		ExecPS2((void *)elfdata.epc, (void *)elfdata.gp, 1, args);
		sio_puts("# wle: post ExecPS2\n");
		return 0;
	} else {
		sio_puts("# wle: SifLoadElf fail\n");
		SifExitRpc();
		return -ENOENT;
	}
}
//--------------------------------------------------------------
//End of func:  int main(int argc, char *argv[])
//--------------------------------------------------------------
//End of file:  loader.c
//--------------------------------------------------------------
