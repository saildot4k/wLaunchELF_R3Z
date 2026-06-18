//---------------------------------------------------------------------------
// File name:   config_network.c
//---------------------------------------------------------------------------
#include "config_private.h"
#include <stdlib.h>

//---------------------------------------------------------------------------
// Network settings GUI by Slam-Tilt
//---------------------------------------------------------------------------
static void saveNetworkSettings(char *Message, const char *target_path)
{
	char firstline[50];
	int out_fd, in_fd;
	int ret = 0, i = 0;
	int size, sizeleft = 0;
	char *ipconfigfile = 0;
	char path[MAX_PATH];

	// Default message, will get updated if save is sucessfull
	sprintf(Message, "%s", LNG(Saved_Failed));

	sprintf(firstline, "%s %s %s\n\r", ip, netmask, gw);

	if (target_path == NULL || target_path[0] == '\0')
		return;



	// Preserve any existing data beyond the first line of the selected target.

	if (genFixPath(target_path, path) >= 0)
		in_fd = genOpen(path, FIO_O_RDONLY);
	else
		in_fd = -1;

	if (in_fd >= 0) {

		size = genLseek(in_fd, 0, SEEK_END);
		DPRINTF("%s: size of existing file is %ibytes\n\r", __func__, size);

		if (size > 0 && (ipconfigfile = (char *)memalign(64, size)) != NULL) {
			int read_size;

			genLseek(in_fd, 0, SEEK_SET);
			read_size = genRead(in_fd, ipconfigfile, size);
			if (read_size > 0) {
				size = read_size;
				for (i = 0; i < size && ipconfigfile[i] != '\r' && ipconfigfile[i] != '\n'; i++)
					;
				while (i < size && (ipconfigfile[i] == '\r' || ipconfigfile[i] == '\n'))
					i++;
				sizeleft = size - i;
			}
		}

		genClose(in_fd);
	}

	// Writing the data out

	configEnsureSysconfDir(target_path);
	if (genFixPath(target_path, path) < 0) {
		if (ipconfigfile != NULL)
			free(ipconfigfile);
		return;
	}
	out_fd = genOpen(path, FIO_O_WRONLY | FIO_O_TRUNC | FIO_O_CREAT);
	if (out_fd >= 0) {
		mcSync(0, NULL, &ret);
		genWrite(out_fd, firstline, strlen(firstline));
		mcSync(0, NULL, &ret);

		// If we have any extra data, spit that out too.
		if (sizeleft > 0) {
			mcSync(0, NULL, &ret);
			genWrite(out_fd, &ipconfigfile[i], sizeleft);
			mcSync(0, NULL, &ret);
		}

		snprintf(Message, MAX_PATH, "%s %.*s", LNG(Saved), MAX_PATH - 4, target_path);
		snprintf(LoadedIPConfigPath, sizeof(LoadedIPConfigPath), "%s", target_path);

		genClose(out_fd);
	}
	if (ipconfigfile != NULL)
		free(ipconfigfile);
}
//---------------------------------------------------------------------------
// Convert IP string to numbers
//---------------------------------------------------------------------------
static void ipStringToOctet(char *ip, int ip_octet[4])
{

	// This takes a string (ip) representing an IP address and converts it
	// into an array of ints (ip_octet)
	// Rewritten 22/10/05

	char oct_str[5];
	int oct_cnt, i, oct_len;

	oct_cnt = 0;
	oct_len = 0;
	oct_str[0] = '\0';

	for (i = 0; ((i <= strlen(ip)) && (oct_cnt < 4)); i++) {
		if ((ip[i] == '.') || (i == strlen(ip))) {
			ip_octet[oct_cnt] = atoi(oct_str);
			oct_cnt++;
			oct_len = 0;
			oct_str[0] = '\0';
		} else if (oct_len < (int)sizeof(oct_str) - 1) {
			oct_str[oct_len++] = ip[i];
			oct_str[oct_len] = '\0';
		}
	}
}
//---------------------------------------------------------------------------
static data_ip_struct BuildOctets(char *ip, char *nm, char *gw)
{

	// Populate 3 arrays with the ip address (as ints)

	data_ip_struct iplist;

	ipStringToOctet(ip, iplist.ip);
	ipStringToOctet(nm, iplist.nm);
	ipStringToOctet(gw, iplist.gw);

	return (iplist);
}
//---------------------------------------------------------------------------
enum CONFIG_NET {
	CONFIG_NET_FIRST = 1,
	CONFIG_NET_IP = CONFIG_NET_FIRST,
	CONFIG_NET_NM,
	CONFIG_NET_GW,

	//Settings after IP addresses
	CONFIG_NET_AFT_IP,
	CONFIG_NET_SAVE_OVERRIDE = CONFIG_NET_AFT_IP,
	CONFIG_NET_SAVE_CWD,
	CONFIG_NET_SAVE_SYSCONF,
	CONFIG_NET_RETURN,

	CONFIG_NET_COUNT
};

void Config_Network(void)
{
	// Menu System for Network Settings Page.

	int s, l;
	int x, y;
	int event, post_event = 0;
	int len;
	char c[MAX_PATH];
	char value[MAX_PATH];
	data_ip_struct ipdata;
	char NetMsg[MAX_PATH] = "";
	char save_override_path[MAX_PATH];
	char save_cwd_path[MAX_PATH];
	char save_sysconf_path[MAX_PATH];
	int has_override_path;

	event = 1;  //event = initial entry
	s = CONFIG_NET_FIRST;
	l = 1;
	ipdata = BuildOctets(ip, netmask, gw);
	has_override_path = (setting->CNF_Path[0] != '\0');
	if (has_override_path)
		configAppendPathFile(save_override_path, sizeof(save_override_path), setting->CNF_Path, "IPCONFIG.DAT");
	else
		save_override_path[0] = '\0';
	configAppendPathFile(save_cwd_path, sizeof(save_cwd_path), LaunchElfBootDir[0] ? LaunchElfBootDir : LaunchElfDir, "IPCONFIG.DAT");
	configBuildSysconfPath(save_sysconf_path, sizeof(save_sysconf_path), "IPCONFIG.DAT");

	while (1) {
		if (!has_override_path && s == CONFIG_NET_SAVE_OVERRIDE)
			s = CONFIG_NET_SAVE_CWD;

		//Pad response section
		waitPadReady(0, 0);
		if (readpad()) {
			if (new_pad & PAD_UP) {
				event |= 2;  //event |= valid pad command
				if (s != CONFIG_NET_FIRST)
					s--;
				else {
					s = CONFIG_NET_RETURN;
					l = 1;
				}
				if (!has_override_path && s == CONFIG_NET_SAVE_OVERRIDE)
					s = CONFIG_NET_GW;
			} else if (new_pad & PAD_DOWN) {
				event |= 2;  //event |= valid pad command
				if (s != CONFIG_NET_COUNT - 1)
					s++;
				else
					s = CONFIG_NET_FIRST;
				if (!has_override_path && s == CONFIG_NET_SAVE_OVERRIDE)
					s = CONFIG_NET_SAVE_CWD;
				if (s >= CONFIG_NET_AFT_IP)
					l = 1;
			} else if (new_pad & PAD_LEFT) {
				event |= 2;  //event |= valid pad command
				if (s < CONFIG_NET_AFT_IP)
					if (l > 1)
						l--;
			} else if (new_pad & PAD_RIGHT) {
				event |= 2;  //event |= valid pad command
				if (s < CONFIG_NET_AFT_IP)
					if (l < 5)
						l++;
			} else if ((!swapKeys && new_pad & PAD_CROSS) || (swapKeys && new_pad & PAD_CIRCLE)) {
				event |= 2;  //event |= valid pad command
				if ((s < CONFIG_NET_AFT_IP) && (l > 1)) {
					if (s == CONFIG_NET_IP) {
						if (ipdata.ip[l - 2] > 0) {
							ipdata.ip[l - 2]--;
						}
					} else if (s == CONFIG_NET_NM) {
						if (ipdata.nm[l - 2] > 0) {
							ipdata.nm[l - 2]--;
						}
					} else if (s == CONFIG_NET_GW) {
						if (ipdata.gw[l - 2] > 0) {
							ipdata.gw[l - 2]--;
						}
					}
				}
			} else if ((swapKeys && new_pad & PAD_CROSS) || (!swapKeys && new_pad & PAD_CIRCLE)) {
				event |= 2;  //event |= valid pad command
				if ((s < CONFIG_NET_AFT_IP) && (l > 1)) {
					if (s == CONFIG_NET_IP) {
						if (ipdata.ip[l - 2] < 255) {
							ipdata.ip[l - 2]++;
						}
					} else if (s == CONFIG_NET_NM) {
						if (ipdata.nm[l - 2] < 255) {
							ipdata.nm[l - 2]++;
						}
					} else if (s == CONFIG_NET_GW) {
						if (ipdata.gw[l - 2] < 255) {
							ipdata.gw[l - 2]++;
						}
					}

					} else if ((s >= CONFIG_NET_SAVE_OVERRIDE) && (s <= CONFIG_NET_SAVE_SYSCONF)) {
						sprintf(ip, "%i.%i.%i.%i", ipdata.ip[0], ipdata.ip[1], ipdata.ip[2], ipdata.ip[3]);
						sprintf(netmask, "%i.%i.%i.%i", ipdata.nm[0], ipdata.nm[1], ipdata.nm[2], ipdata.nm[3]);
						sprintf(gw, "%i.%i.%i.%i", ipdata.gw[0], ipdata.gw[1], ipdata.gw[2], ipdata.gw[3]);

						if (s == CONFIG_NET_SAVE_OVERRIDE) {
							if (has_override_path)
								saveNetworkSettings(NetMsg, save_override_path);
						} else if (s == CONFIG_NET_SAVE_CWD)
							saveNetworkSettings(NetMsg, save_cwd_path);
						else
							saveNetworkSettings(NetMsg, save_sysconf_path);
					} else  //s == CONFIG_NET_RETURN
						return;
			} else if (new_pad & PAD_TRIANGLE)
				return;
		}

		if (event || post_event) {  //NB: We need to update two frame buffers per event

			//Display section
			clrScr(setting->color[COLOR_BACKGR]);

			x = Menu_start_x;
			y = Menu_start_y;

			printXY(LNG(NETWORK_SETTINGS), x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			y += FONT_HEIGHT / 2;

			len = (strlen(LNG(IP_Address)) + 5 > strlen(LNG(Netmask)) + 5) ?
			          strlen(LNG(IP_Address)) + 5 :
			          strlen(LNG(Netmask)) + 5;
			len = (len > strlen(LNG(Gateway)) + 5) ? len : strlen(LNG(Gateway)) + 5;
			sprintf(c, "%s:", LNG(IP_Address));
			printXY(c, x + 2 * FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
			sprintf(c, "%.3i . %.3i . %.3i . %.3i", ipdata.ip[0], ipdata.ip[1], ipdata.ip[2], ipdata.ip[3]);
			printXY(c, x + len * FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			sprintf(c, "%s:", LNG(Netmask));
			printXY(c, x + 2 * FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
			sprintf(c, "%.3i . %.3i . %.3i . %.3i", ipdata.nm[0], ipdata.nm[1], ipdata.nm[2], ipdata.nm[3]);
			printXY(c, x + len * FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			sprintf(c, "%s:", LNG(Gateway));
			printXY(c, x + 2 * FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
			sprintf(c, "%.3i . %.3i . %.3i . %.3i", ipdata.gw[0], ipdata.gw[1], ipdata.gw[2], ipdata.gw[3]);
			printXY(c, x + len * FONT_WIDTH, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			y += FONT_HEIGHT / 2;

			configFormatSavePathValue(value, sizeof(value), has_override_path ? save_override_path : "NOT SET", has_override_path ? LoadedIPConfigPath : NULL);
			configFormatLabelValue(c, sizeof(c), "Save to override path", value);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			configFormatSavePathValue(value, sizeof(value), save_cwd_path, LoadedIPConfigPath);
			configFormatLabelValue(c, sizeof(c), LNG(Save_to), value);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;
			configFormatSavePathValue(value, sizeof(value), save_sysconf_path, LoadedIPConfigPath);
			configFormatLabelValue(c, sizeof(c), LNG(Save_to), value);
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			y += FONT_HEIGHT / 2;
			sprintf(c, "  %s", LNG(RETURN));
			printXY(c, x, y, setting->color[COLOR_TEXT], TRUE, 0);
			y += FONT_HEIGHT;

			//Cursor positioning section
			y = Menu_start_y + s * FONT_HEIGHT + FONT_HEIGHT / 2;

			if (s >= CONFIG_NET_AFT_IP)
				y += FONT_HEIGHT / 2;
			if (s >= CONFIG_NET_RETURN)
				y += FONT_HEIGHT / 2;
			if (l > 1)
				x += (len - 1) * FONT_WIDTH - 1 + (l - 2) * 6 * FONT_WIDTH;
			drawChar(LEFT_CUR, x, y, setting->color[COLOR_TEXT]);

			//Tooltip section
			if ((s < CONFIG_NET_AFT_IP) && (l == 1)) {
				len = sprintf(c, "%s", LNG(Right_DPad_to_Edit));
			} else if (s < CONFIG_NET_AFT_IP) {
				if (swapKeys)
					len = sprintf(c, "\xFF"
					                 "1:%s \xFF"
					                 "0:%s",
					              LNG(Add), LNG(Subtract));
				else
					len = sprintf(c, "\xFF"
					                 "0:%s \xFF"
					                 "1:%s",
					              LNG(Add), LNG(Subtract));
				} else if ((s >= CONFIG_NET_SAVE_OVERRIDE) && (s <= CONFIG_NET_SAVE_SYSCONF)) {
					if (swapKeys)
						len = sprintf(c, "\xFF"
						                 "1:%s",
						              LNG(Save));
					else
						len = sprintf(c, "\xFF"
						                 "0:%s",
						              LNG(Save));
			} else {
				if (swapKeys)
					len = sprintf(c, "\xFF"
					                 "1:%s",
					              LNG(OK));
				else
					len = sprintf(c, "\xFF"
					                 "0:%s",
					              LNG(OK));
			}
			sprintf(&c[len], " \xFF"
			                 "3:%s",
			        LNG(Return));
			setScrTmp(NetMsg, c);
		}  //ends if(event||post_event)
		drawScr();
		post_event = event;
		event = 0;

	}  //ends while
}  //ends Config_Network
//---------------------------------------------------------------------------
// End of file: config_network.c
//---------------------------------------------------------------------------
