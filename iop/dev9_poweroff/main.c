#include <irx.h>
#include <loadcore.h>
#include <dev9.h>

IRX_ID("dev9_poweroff", 1, 0);

int _start(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	/*
	 * APA registers slot 0 for SMART attribute saving and ATAD registers
	 * slot 15 for STANDBY IMMEDIATE. PFS close has already handled the
	 * filesystem side; these drive-maintenance callbacks can stall final
	 * software poweroff for several seconds on some drives/adapters.
	 */
	Dev9RegisterPowerOffHandler(0, 0);
	Dev9RegisterPowerOffHandler(15, 0);

	return MODULE_NO_RESIDENT_END;
}
