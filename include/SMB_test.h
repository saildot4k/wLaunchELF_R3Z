//---------------------------------------------------------------------------
//File name:   SMB_test.h
//---------------------------------------------------------------------------
#ifndef SMB_TEST_H
#define SMB_TEST_H

#include <stddef.h>
#include <tamtypes.h>

#define SERVERLIST_MAX 16

typedef struct
{                          // size = 1148
	char Server_IP[16];    //IP address of this server
	int Server_Port;       //IP port for use with this server
	char Username[256];    //Username for login to this server (NUL if anonymous)
	char Password[256];    //Password for login to this server (ignore if anonymous)
	int PasswordType;      // PLAINTEXT_PASSWORD or HASHED_PASSWORD or NO_PASSWORD
	u8 PassHash[32];       //Hashed password for this server (unused if anonymous)
	int PassHash_f;        //Flags hashing already performed if non-zero
	int Server_Logon_f;    //Flags successful logon to this server
	char Client_ID[256];   //Unit name of ps2, in SMB traffic with this server
	char Server_ID[256];   //Unit name of this server, as defined for SMB traffic
	char Server_FBID[64];  //Name of this server for display in FileBrowser
} smbServerList_t;         //uLE SMB ServerList entry type

extern int smbCurrentServer;
extern int smbServerListCount;
extern smbServerList_t smbServerList[SERVERLIST_MAX];

int smbLogon_Server(int Index);
int smbLogon_CurrentServer(void);
size_t storeSMBCNF(char *cnf_buf);
int saveSMBCNF(char *CNFpath);
int scanSMBCNF(unsigned char *name, unsigned char *value);
int loadSMBCNF(char *path);
//---------------------------------------------------------------------------
// End of file
//---------------------------------------------------------------------------

#endif /* SMB_TEST_H */
