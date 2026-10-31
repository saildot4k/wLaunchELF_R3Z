#ifndef SMB2MAN_H
#define SMB2MAN_H

// devctl interface of libsmb2's smb2man.irx, as in libsmb2's lib/ps2/ps2smb2.h

#define SMB2_DEVCTL_CONNECT        0xC0DE0001
#define SMB2_DEVCTL_DISCONNECT_ALL 0xC0DE0002

#define SMB2_MAX_NAME_LEN 32
typedef struct
{
	char name[SMB2_MAX_NAME_LEN];
	char username[32];
	char password[32];
	char url[256];
} smb2Connect_in_t;

typedef struct
{
	void *ctx;
} smb2Connect_out_t;

#endif
