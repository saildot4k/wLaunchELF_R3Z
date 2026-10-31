//---------------------------------------------------------------------------
// SMB2/3 shares through libsmb2's smb2man.irx, as the smb: device.
// The shares are defined in SMB2.CNF, see README.md.
//---------------------------------------------------------------------------
#include "launchelf.h"
#include "smb2man.h"

static char *loadSMB2CNF(void)
{
	char path[MAX_PATH];
	char *cnf;
	int port, i;

	snprintf(path, sizeof(path), "%sSMB2.CNF", LaunchElfDir);
	if ((cnf = preloadCNF(path)) != NULL)
		return cnf;

	port = !strncmp(LaunchElfDir, "mc1", 3);
	for (i = 0; i < 2; i++, port ^= 1) {
		snprintf(path, sizeof(path), "mc%d:/SYS-CONF/SMB2.CNF", port);
		if ((cnf = preloadCNF(path)) != NULL)
			return cnf;
	}
	return NULL;
}

void connectSMB2Shares(void)
{
	smb2Connect_in_t in;
	smb2Connect_out_t out;
	char *RAM_p, *CNF_p, *name, *value;
	int ret;

	if ((RAM_p = loadSMB2CNF()) == NULL) {
		DPRINTF(" [SMB2]: no SMB2.CNF\n");
		return;
	}

	CNF_p = RAM_p;
	memset(&in, 0, sizeof(in));
	while (get_CNF_string(&CNF_p, &name, &value)) {
		if (!strcmp(name, "NAME"))
			snprintf(in.name, sizeof(in.name), "%s", value);
		else if (!strcmp(name, "USERNAME"))
			snprintf(in.username, sizeof(in.username), "%s", value);
		else if (!strcmp(name, "PASSWORD"))
			snprintf(in.password, sizeof(in.password), "%s", value);
		else if (!strcmp(name, "URL")) {
			snprintf(in.url, sizeof(in.url), "%s", value);
			ret = fileXioDevctl("smb:", SMB2_DEVCTL_CONNECT, &in, sizeof(in), &out, sizeof(out));
			DPRINTF(" [SMB2]: %s as smb:/%s, ret=%d\n", in.url, in.name, ret);
			(void)ret;
			memset(&in, 0, sizeof(in));
		}
	}
	free(RAM_p);
}
//---------------------------------------------------------------------------
// End of file: smb2.c
//---------------------------------------------------------------------------
