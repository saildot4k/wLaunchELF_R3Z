#ifndef MAIN_ACTIONS_H
#define MAIN_ACTIONS_H

typedef struct
{
	int boot_argc;
	char **boot_argv;
	const char *boot_path;
	char *main_msg;
	char *cnf_path;
	const char *default_esr_path;
	const char *default_osdsys_path;
	const char *default_osdsys_path2;
	char rough_region;
	char *romver_data;
} MainExecuteContext;

void ExecuteMainAction(char *pathin, const MainExecuteContext *ctx);

#endif
