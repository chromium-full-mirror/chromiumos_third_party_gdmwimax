// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <math.h>
#include <zlib.h>

#include "cm.h"
#include "profilestring.h"
#include "param.h"
#include "sem.h"

extern int get_first_odev(void);
extern int cm_odev_cnt;
extern char *StatusStr[8];
extern char *ConnStatusStr[8];
static sem_t hci_data_sem[MAX_DEV];

typedef struct hci_data_s {
	char data[HCI_MAX_PACKET];
	int len;
} hci_data_t;
static hci_data_t hci_data[MAX_DEV];

#define NO_CM_CMD		(-2)
#define EXIT_CM_CMD		(-100)

#define KEY_auth_pkm_enable					"auth_pkm_enable"
#define KEY_eap_type						"eap_type"
#define KEY_eap_tls_use_nvram_info			"eap_tls_use_nvram_info"
#define KEY_eap_tls_userid					"eap_tls_userid"
#define KEY_eap_tls_userpasswd				"eap_tls_userpasswd"
#define KEY_eap_tls_anonyid					"eap_tls_anonyid"
#define KEY_eap_tls_privateKeyPwd			"eap_tls_pri_passwd"
#define KEY_eap_tls_fragsize				"eap_tls_fragsize"
#define KEY_eap_tls_delimiter_enable		"eap_tls_delimiter_enable"
#define KEY_eap_tls_resumption_disable		"eap_tls_resumption_disable"
#define KEY_eap_tls_cr801_enable			"eap_tls_cr801_enable"
#define KEY_eap_tls_decoration				"eap_tls_decoration"
#define KEY_eap_tls_sessionticket_disable	"eap_tls_sessionticket_disable"
#define KEY_eap_tls_dev_cert_null			"eap_tls_dev_cert_null"
#define KEY_eap_tls_ca_cert_null			"eap_tls_ca_cert_null"
#define KEY_eap_tls_use_nvram_cert			"eap_tls_use_nvram_cert"
#define KEY_eap_tls_wm_serv_root			"eap_tls_wm_serv_root"
#define KEY_eap_tls_serv_root_ca1			"eap_tls_serv_root_ca1"
#define KEY_eap_tls_serv_root_ca2			"eap_tls_serv_root_ca2"
#define KEY_eap_tls_serv_root_ca3			"eap_tls_serv_root_ca3"
#define KEY_eap_tls_wm_dev_root				"eap_tls_wm_dev_root"
#define KEY_eap_tls_dev_root_ca1			"eap_tls_dev_root_ca1"
#define KEY_eap_tls_dev_sub_ca				"eap_tls_dev_sub_ca"
#define KEY_eap_tls_dev_cert				"eap_tls_dev_cert"
#define KEY_eap_tls_dev_cert_key			"eap_tls_dev_cert_key"

#define desc_eap_tls_wm_serv_root			"wimax server root"
#define desc_eap_tls_serv_root_ca1			"server root ca1"
#define desc_eap_tls_serv_root_ca2			"server root ca2"
#define desc_eap_tls_serv_root_ca3			"server root ca3"
#define desc_eap_tls_wm_dev_root			"wimax device root"
#define desc_eap_tls_dev_root_ca1			"device root ca1"
#define desc_eap_tls_dev_sub_ca				"device sub ca"
#define desc_eap_tls_dev_cert				"device cert"
#define desc_eap_tls_dev_cert_key			"device cert private key"

dev_conf_t default_dev_conf;
WIMAX_API_PROFILE_INFO profile_list[MAX_PROFILE_LIST];
int profile_list_cnt;

typedef int cmd_fun(int argc, char *argv[]);

typedef struct cmd_s {
	const char	*cmd;
	const char	*desc;
	const char	*param;
	cmd_fun	*func;

} cmd_t;

#define DEVIATION_CNT	10

typedef struct deviation_s {
	float value[DEVIATION_CNT];
	int cnt;

} deviation_t;

typedef struct dev_info_s {
	deviation_t cinr1;
	deviation_t cinr2;
	deviation_t rssi1;
	deviation_t rssi2;

} dev_info_t;

static dev_info_t dev_info[MAX_DEV];


static int cm_uicc_power(int dev_idx, int on);
static void print_cmd_usage(const char *cmd);
static void print_cmd_list(const char *cmd);
static int do_cmd(char *cmd_line, int len);

static __inline bool __is_delimiter(char ch)
{
   return (ch == ' ');
}

static __inline bool __is_binder(char ch)
{
   return (ch == '"');
}

static void __get_token_arg(char *cmd_line, int *argc, char *argv[])
{
	int found_arg = 1,
		bind_on = 0;
	int argn = 0;

	while (*cmd_line) {
		if (*cmd_line == '\n') {
			*cmd_line = '\0';
			break;
		}
		if (0 == bind_on && __is_delimiter(*cmd_line)) {
			found_arg = 1;
			*cmd_line = '\0';
		}
		else if (__is_binder(*cmd_line)) {
			bind_on ^= 1;
			*cmd_line = '\0';
		}
		else if (found_arg) {
			argv[argn++] = cmd_line;
			found_arg = 0;
		}

		cmd_line++;
	}

	*argc = argn;
}

static int find_device_conf(const char *mac)
{
	char section[256];
	int tmp;

	assert(mac);
	
	sprintf(section, "device-%s", mac);

	if (get_profile_section(section, (char *)&tmp, 1, CONF_FILE))
		return 1;
	else
		return 0;
}

void load_device_conf(dev_conf_t *conf, const char *mac)
{
	cm_common_conf_t *pconf = &cm_common_conf;
	char section[256];
	char str[256];

	memset(conf, 0, sizeof(*conf));

	if (!mac) {
		memset(conf->mac_str, 0, sizeof(conf->mac_str));
		mac = "default";
	}
	else
		memcpy(conf->mac_str, mac, sizeof(conf->mac_str));
	sprintf(section, "device-%s", mac);

	get_profile_string(section, KEY_auth_pkm_enable, "n", str, sizeof(str), CONF_FILE);
	if (!strcasecmp(str, "n"))
		conf->eapp.type = GCT_WIMAX_NO_EAP;
	else {
		get_profile_string(section, KEY_eap_type, "TTLS_MSCHAPV2", 
			str, sizeof(str), CONF_FILE);
		strcpy(conf->type_str, str);
		if (!strcasecmp(str, "AKA"))
			conf->eapp.type = GCT_WIMAX_EAP_AKA;
		else if (!strcasecmp(str, "TLS"))
			conf->eapp.type = GCT_WIMAX_EAP_TLS;
		else if (!strcasecmp(str, "TTLS_MD5"))
			conf->eapp.type = GCT_WIMAX_EAP_TTLS_MD5;
		else if (!strcasecmp(str, "TTLS_MSCHAPV2"))
			conf->eapp.type = GCT_WIMAX_EAP_TTLS_MSCHAPV2;
		else if (!strcasecmp(str, "TTLS_CHAP"))
			conf->eapp.type = GCT_WIMAX_EAP_TTLS_CHAP;
	}
	get_profile_string(section, KEY_eap_tls_use_nvram_info, "n", str,
						sizeof(str), CONF_FILE);
	if (!strcasecmp(str, "y"))
		conf->eapp.useNvramParam = 1;
	else {
		conf->eapp.useNvramParam = 0;

		get_profile_string(section, KEY_eap_tls_userid, "",
			(char *) conf->eapp.userId, sizeof(conf->eapp.userId), CONF_FILE);
		get_profile_string(section, KEY_eap_tls_userpasswd, "",
			(char *) conf->eapp.userIdPwd, sizeof(conf->eapp.userIdPwd), CONF_FILE);
		get_profile_string(section, KEY_eap_tls_anonyid, "",
			(char *) conf->eapp.anonymousId, sizeof(conf->eapp.anonymousId), CONF_FILE);
		get_profile_string(section, KEY_eap_tls_privateKeyPwd, "",
			(char *) conf->eapp.privateKeyPwd, sizeof(conf->eapp.privateKeyPwd), CONF_FILE);
	}
	
	get_profile_string(section, KEY_eap_tls_fragsize, "1300",
		str, sizeof(str), CONF_FILE);
	conf->eapp.fragSize = atoi(str);

	get_profile_string(section, KEY_eap_tls_delimiter_enable, "n",
		str, sizeof(str), CONF_FILE);
	if (!strcasecmp(str, "y"))
		conf->eapp.useDelimiter = 1;
	else
		conf->eapp.useDelimiter = 0;

	get_profile_string(section, KEY_eap_tls_resumption_disable, "n",
		str, sizeof(str), CONF_FILE);
	if (!strcasecmp(str, "y"))
		conf->eapp.disableResumption = 1;
	else
		conf->eapp.disableResumption = 0;
	
	get_profile_string(section, KEY_eap_tls_cr801_enable, "n",
		str, sizeof(str), CONF_FILE);
	if (!strcasecmp(str, "y"))
		conf->eapp.cr801Enable = 1;
	else
		conf->eapp.cr801Enable = 0;
	
	get_profile_string(section, KEY_eap_tls_decoration, "",
		(char *) conf->eapp.decoration, sizeof(conf->eapp.decoration), CONF_FILE);
	
	get_profile_string(section, KEY_eap_tls_sessionticket_disable, "n",
		str, sizeof(str), CONF_FILE);
	if (!strcasecmp(str, "y"))
		conf->eapp.disableSessionTicket = 1;
	else
		conf->eapp.disableSessionTicket = 0;

	get_profile_string(section, KEY_eap_tls_dev_cert_null, "n",
		str, sizeof(str), CONF_FILE);
	if (!strcasecmp(str, "y"))
		conf->eapp.devCertNULL = 1;
	else
		conf->eapp.devCertNULL = 0;

	get_profile_string(section, KEY_eap_tls_ca_cert_null, "n",
		str, sizeof(str), CONF_FILE);
	if (!strcasecmp(str, "y"))
		conf->eapp.caCertNULL = 1;
	else
		conf->eapp.caCertNULL = 0;

	conf->eapp.logEnable = pconf->eap_log_enable;
}

static void print_device_conf(dev_conf_t *conf)
{
	cm_common_conf_t *pconf = &cm_common_conf;
	const char *type;

	cm_printf("--------------- Begin loaded configurations ---------------\n");

	cm_printf("[common]\n");
	cm_printf("Log path: %s\n", pconf->log_path);
	cm_printf("Log level: %d\n", pconf->log_level);
	cm_printf("EAP log: %s\n", pconf->eap_log_enable ? "enabled" : "disabled");
	cm_printf("Embedded EAP: %s\n", pconf->embedded_eap_enable ? "enabled" : "disabled");
	cm_printf("OMA-DM: %s\n", pconf->oma_dm_enable ? "enabled" : "disabled");
	cm_printf("Non-volatile path: %s\n", pconf->nonvolatile_dir);
	cm_printf("Default script file: %s\n", pconf->run_script_file);
	cm_printf("Auto connection: %s\n", pconf->auto_connect_enable ? "enabled" : "disabled");
	if (pconf->auto_connect_enable)
		cm_printf("Auto connection retry count: %u\n", pconf->auto_connect_retry_count);
	cm_printf("Auto selecting profile index: %d\n", pconf->auto_select_profile_index);
	cm_printf("Unknown network auto connection: %s\n",
		pconf->unknown_net_auto_connect_enable ? "enabled" : "disabled");
	cm_printf("IP allocation timeout: %d sec.\n", pconf->ip_allocation_timeout_sec);
	cm_printf("Disconnect on ip failure: %s\n",
		pconf->disconnct_on_ip_failure ? "enabled" : "disabled");
	cm_printf("\n");

	if (!conf->mac_str[0])
		cm_printf("[device-default]\n");
	else
		cm_printf("[device-%s]\n", conf->mac_str);

	if (conf->eapp.type == GCT_WIMAX_NO_EAP)
		cm_printf("AUTH PKM: disabled\n");
	else {
		cm_printf("AUTH PKM: enabled\n");
		cm_printf("EAP type: %s\n", conf->type_str);
		if (conf->eapp.type >= GCT_WIMAX_EAP_TLS &&
			conf->eapp.type <= GCT_WIMAX_EAP_TTLS_CHAP) {
			if (conf->eapp.useNvramParam)
				cm_printf("NVRAM parameter for TLS/TTLS enabled\n");

			if (conf->eapp.type != GCT_WIMAX_EAP_TLS) {
				type = "TTLS";
				if (!conf->eapp.useNvramParam) {
					cm_printf("%s user id: %s\n", type, conf->eapp.userId);
					cm_printf("%s user passwd: %s\n", type, conf->eapp.userIdPwd);
				}
			}
			else
				type = "TLS";

			if (!conf->eapp.useNvramParam) {
				cm_printf("%s anonymous id: %s\n", type, conf->eapp.anonymousId);
				cm_printf("%s private key passwd: %s\n", type, conf->eapp.privateKeyPwd);
			}
			cm_printf("%s frag size: %d\n", type, conf->eapp.fragSize);
			cm_printf("%s delimiter: %s\n", type,
				conf->eapp.useDelimiter ? "enabled" : "disabled");
			cm_printf("%s null dev cert: %s\n", type,
				conf->eapp.devCertNULL ? "enabled" : "disabled");
			cm_printf("%s null ca cert: %s\n", type,
				conf->eapp.caCertNULL ? "enabled" : "disabled");
			cm_printf("%s resumption: %s\n", type,
				conf->eapp.disableResumption ? "disabled" : "enabled");
			cm_printf("%s cr801: %s\n", type,
				conf->eapp.cr801Enable ? "enabled" : "disabled");
			cm_printf("%s decoration: %s\n", type, conf->eapp.decoration);
			cm_printf("%s session-ticket: %s\n", type,
				conf->eapp.disableSessionTicket ? "disabled" : "enabled");
		}
	}

	cm_printf("---------------- End loaded configurations ----------------\n");
}

int setup_device_conf(GDEV_ID *ID, const unsigned char *mac)
{
	cm_common_conf_t *pconf = &cm_common_conf;
	dev_conf_t *conf = &cm_dev_conf[ID->deviceIndex];
	char mac_str[32];

	memset(conf, 0, sizeof(*conf));

	assert(mac);

	sprintf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x",
		mac[0], mac[1],	mac[2], mac[3], mac[4], mac[5]);

	if (find_device_conf(mac_str))
		load_device_conf(conf, mac_str);
	else
		memcpy(conf, &default_dev_conf, sizeof(dev_conf_t));

	print_device_conf(conf);
	if (conf->eapp.type != GCT_WIMAX_NO_EAP) {
		if (pconf->api_mode != GCT_WIMAX_API_PRIVILEGE_READ_ONLY) {
			if (GAPI_SetEap(ID, &conf->eapp) != GCT_API_RET_SUCCESS) {
				cm_printf("Auth configuration failure\n");
				return -1;
			}
		}
	}

	if (GAPI_GetCapability(ID, &conf->cap) != GCT_API_RET_SUCCESS) {
		conf->cap = 0;
		cm_printf("Getting capability failure\n");
		return -1;
	}
	if (pconf->api_mode != GCT_WIMAX_API_PRIVILEGE_READ_ONLY) {
		if (CAP_EEAP_AKA_ENABLED(ID->deviceIndex))
			cm_uicc_power(ID->deviceIndex, 1);
	}
	return ID->deviceIndex;
}

static int send_print_string(GDEV_ID_P pID, void *buf, int len)
{
	char hci_buf[HCI_MAX_PACKET];
	hci_t *hci = (hci_t *) hci_buf;

	if (len > (HCI_MAX_PACKET-HCI_HEADER_SIZE))
		len = HCI_MAX_PACKET-HCI_HEADER_SIZE;

	hci->cmd_evt = _H2B(WIMAX_CLI_CMD);
	hci->length = _H2B(len);
	memcpy(hci->data, buf, len);

	len += HCI_HEADER_SIZE;

	if (GAPI_WriteHCIPacket(pID, hci_buf, len) != GCT_API_RET_SUCCESS) {
		cm_printf("Write HCI failure\n");
		return -1;
	}

	return 0;
}

static int cmd_get_dev_list(int argc, char *argv[])
{
	WIMAX_API_HW_DEVICE_ID list[256];
	UINT32 cnt = 256;
	int i;

	if (GAPI_GetListDevice(cm_api_handle, list, &cnt) != GCT_API_RET_SUCCESS) {
		cm_printf("Get device list failure\n");
		return -1;
	}

	if (cnt) {
		cm_printf("[Device index] [Device name]\n");
		for (i = 0; i < (int) cnt; i++) {
			cm_printf("          %3d%14S\n",
				list[i].deviceIndex,	(wchar_t *)list[i].deviceName);
		}
		return list[0].deviceIndex;
	}
	else
		cm_printf("Not found device\n");
	return 0;
}

static int cmd_get_status(int argc, char *argv[])
{
	WIMAX_API_DEVICE_STATUS	nDevStatus = 0;
	WIMAX_API_CONNECTION_PROGRESS_INFO nConnectionInfo = 0;
	GDEV_ID ID;

	ID.apiHandle = cm_api_handle;
	if (argc == 1)
		ID.deviceIndex = DEFAULT_DEVICE;
	else
		ID.deviceIndex = atoi(argv[1]);

	if (GAPI_GetDeviceStatus(&ID, &nDevStatus, &nConnectionInfo) == GCT_API_RET_SUCCESS) {
		cm_printf("device[%d] status [%s]\n", ID.deviceIndex, StatusStr[nDevStatus]);
		if (nDevStatus == WIMAX_API_DEVICE_STATUS_Connecting)
			cm_printf("Connection status [%s]\n", ConnStatusStr[nConnectionInfo]);
	}
	else
		cm_printf("Get device status failure\n");
	return ID.deviceIndex;
}

static int cm_rf_up(int dev_idx)
{
	GDEV_ID ID;

	ID.apiHandle = cm_api_handle;
	ID.deviceIndex = dev_idx;

	if (GAPI_CmdControlPowerManagement(&ID, WIMAX_API_RF_ON) != GCT_API_RET_SUCCESS) {
		cm_eprintf("device[%d] rf up failure\n", ID.deviceIndex);
		return -1;
	}
	return dev_idx;
}

static int cmd_rf_up(int argc, char *argv[])
{
	int dev_idx;

	if (argc == 1)
		dev_idx = DEFAULT_DEVICE;
	else
		dev_idx = atoi(argv[1]);

	return cm_rf_up(dev_idx);
}

static int cmd_rf_down(int argc, char *argv[])
{
	GDEV_ID ID;

	ID.apiHandle = cm_api_handle;
	if (argc == 1)
		ID.deviceIndex = DEFAULT_DEVICE;
	else
		ID.deviceIndex = atoi(argv[1]);

	if (GAPI_CmdControlPowerManagement(&ID, WIMAX_API_RF_OFF) != GCT_API_RET_SUCCESS) {
		cm_printf("device[%d] rf down failure\n", ID.deviceIndex);
		return -1;
	}
	return ID.deviceIndex;
}

static int cm_get_profile_list(int dev_idx)
{
	GDEV_ID ID;
	int i;

	ID.apiHandle = cm_api_handle;
	ID.deviceIndex = dev_idx;

	profile_list_cnt = _numof_array(profile_list);
	if (GAPI_GetSelectProfileList(&ID, profile_list, (UINT32 *) &profile_list_cnt)
		!= GCT_API_RET_SUCCESS) {
		cm_printf("Get profile failure\n");
		return -1;
	}

	cm_printf("[Profile-Index] [Profile-ID] [Profile-Name]\n");
	for (i = 0; i < profile_list_cnt; i++) {
		cm_printf("%14d     %08X   %S\n",
			i, (int)profile_list[i].profileID,	(wchar_t *)profile_list[i].profileName);
	}

	return profile_list_cnt;
}

static int cmd_get_profile_list(int argc, char *argv[])
{
	int dev_idx;

	if (argc == 1)
		dev_idx = DEFAULT_DEVICE;
	else
		dev_idx = atoi(argv[1]);

	if (cm_get_profile_list(dev_idx) < 0)
		return -1;

	return dev_idx;
}

static int cm_set_profile(int dev_idx, int profile_idx)
{
	GDEV_ID ID;

	ID.apiHandle = cm_api_handle;
	ID.deviceIndex = dev_idx;

	if (profile_idx >= profile_list_cnt)
		cm_eprintf("index[%d] is invalid\n", profile_idx);
	else {
		if (GAPI_SetProfile(&ID, profile_list[profile_idx].profileID)
			!= GCT_API_RET_SUCCESS ) {
			cm_eprintf("Set profile failure\n");
			return -1;
		}
	}

	return 0;
}

static int cmd_set_profile(int argc, char *argv[])
{
	int dev_idx, profile_idx;

	if (argc < 2) {
		cm_printf("Invalid parameter.\n");
		return -1;
	}

	if (argc == 2)
		dev_idx = DEFAULT_DEVICE;
	else
		dev_idx = atoi(argv[2]);
	profile_idx = atoi(argv[1]);

	return cm_set_profile(dev_idx, profile_idx);
}

static int cm_get_net_list(int dev_idx, WIMAX_API_NSP_INFO_P list, int *list_cnt)
{
	GDEV_ID ID;

	ID.apiHandle = cm_api_handle;
	ID.deviceIndex = dev_idx;

	if (GAPI_GetNetworkList(&ID, list, (UINT32 *) list_cnt)
		!= GCT_API_RET_SUCCESS) {
		cm_eprintf("Get network list failure\n");
		return -1;
	}

	return ID.deviceIndex;
}

static void cm_print_net_list(WIMAX_API_NSP_INFO_P list, int list_cnt)
{
	int i;

	cm_printf("[NSP-Name] [NSP-ID] [RSSI] [CINR]\n");
	for (i = 0; i < list_cnt; i++) {
		cm_printf("%9S   %06X %6d %6d\n",
			(wchar_t *) list[i].NSPName,
			(int) list[i].NSPid,
			(int) list[i].RSSI-123,
			(int) list[i].CINR-10);
	}
}

static int cmd_get_net_list(int argc, char *argv[])
{
	WIMAX_API_NSP_INFO nsp[MAX_NETWORK_LIST];
	int dev_idx;
	int list_cnt = MAX_NETWORK_LIST;

	if (argc == 1)
		dev_idx = DEFAULT_DEVICE;
	else
		dev_idx = atoi(argv[1]);

	if (cm_get_net_list(dev_idx, nsp, &list_cnt) < 0)
		return -1;

	cm_print_net_list(nsp, list_cnt);

	return dev_idx;
}

static int cmd_get_neighbor_list(int argc, char *argv[])
{
	GCT_API_NEIGHBOR_LIST Neighbor[64], *n;
	GDEV_ID ID;
	UINT32 list_cnt = _numof_array(Neighbor), i;

	ID.apiHandle = cm_api_handle;
	if (argc == 1)
		ID.deviceIndex = DEFAULT_DEVICE;
	else
		ID.deviceIndex = atoi(argv[1]);

	if (GAPI_GetNeighborList(&ID, Neighbor, &list_cnt)
		!= GCT_API_RET_SUCCESS) {
		cm_printf("Get neighbor list failure\n");
		return -1;
	}

	if (!list_cnt) {
		cm_printf("Not found neighbor!!\n");
		goto out;
	}

	n = Neighbor;
	cm_printf("[Frequency] [Preamble] [CINR] [RSSI]         [BSID]\n");
	for (i = 0; i < list_cnt; i++, n++) {
		cm_printf(" %9d        %3d   %4d   %4d  %02x%02x%02x:%02x%02x%02x\n",
				(int)n->frequency, n->preamble, (s8)n->cinr-10, (s8)n->rssi-123,
				n->bsId[0], n->bsId[1], n->bsId[2],
				n->bsId[3], n->bsId[4], n->bsId[5]);
	}

out:
	return ID.deviceIndex;
}

static int combine_level(int dev_idx, int c0, int c1)
{
	dev_conf_t *conf = &cm_dev_conf[dev_idx];
	int max, diff, offset;

	if (c0 > c1) {
		max = c0;
		diff = c0 - c1;
	}
	else {
		max = c1;
		diff = c1 - c0;
	}

	if (conf->fw_mac_version < 0x01090104/*1.9.1.4*/) {
		/*floor*/
		if (diff >= 6) offset = 0;
		else if (diff >= 3)  offset = 1;
		else if (diff >= 1)  offset = 2;
		else     offset = 3; 
	}
	else {
		/*round*/
		if (diff >= 10) offset = 0;
		else if (diff >= 4)  offset = 1;
		else if (diff >= 2)  offset = 2;
		else     offset = 3; 
	}

	max += offset;

	return max;
}

static float calc_deviation(float *fArr, int cnt)
{
	float sum, dsum, avg;
	int i;

	if (cnt <= 0)
		return 0;
	if (cnt > DEVIATION_CNT)
		cnt = DEVIATION_CNT;

	sum = 0;
	for (i = 0; i < cnt; i++)
		sum += fArr[i];
	avg = sum/cnt;

	for (i = dsum = 0; i < cnt; i++)
		dsum += (fArr[i]-avg)*(fArr[i]-avg);

	return (float) sqrt(dsum/cnt);
}

static const char *fec_string(int fec)
{
	#define _T(s)	s
	const char *str;

	switch(fec) {
		case 0:
			str = _T("QPSK(CC) 1/2");
			break;
		case 1:
			str = _T("QPSK(CC) 3/4");
			break;
		case 2:
			str = _T("16-QAM(CC) 1/2");
			break;
		case 3:
			str = _T("16-QAM(CC) 3/4");
			break;
		case 4:
			str = _T("64-QAM(CC) 1/2");
			break;
		case 5:
			str = _T("64-QAM(CC) 2/3");
			break;
		case 6:
			str = _T("64-QAM(CC) 3/4");
			break;
		case 7:
			str = _T("QPSK(BTC) 1/2");
			break;
		case 8:
			str = _T("QPSK(BTC) 3/4 or 2/3");
			break;
		case 9:
			str = _T("16-QAM(BTC) 3/5");
			break;
		case 10:
			str = _T("16-QAM(BTC) 4/5");
			break;
		case 11:
			str = _T("64-QAM(BTC) 2/3 or 5/8");
			break;
		case 12:
			str = _T("64-QAM(BTC) 5/6 or 4/5");
			break;
		case 13:
			str = _T("QPSK(CTC) 1/2");
			break;
		/*case 14:
			str = _T("Reserved");
			break;*/
		case 15:
			str = _T("QPSK(CTC) 3/4");
			break;
		case 16:
			str = _T("16-QAM(CTC) 1/2");
			break;
		case 17:
			str = _T("16-QAM(CTC) 3/4");
			break;
		case 18:
			str = _T("64-QAM(CTC) 1/2");
			break;
		case 19:
			str = _T("64-QAM(CTC) 2/3");
			break;
		case 20:
			str = _T("64-QAM(CTC) 3/4");
			break;
		case 21:
			str = _T("64-QAM(CTC) 5/6");
			break;
		case 22:
			str = _T("QPSK(ZT CC) 1/2");
			break;
		case 23:
			str = _T("QPSK(ZT CC) 3/4");
			break;
		case 24:
			str = _T("16-QAM(ZT CC) 1/2");
			break;
		case 25:
			str = _T("16-QAM(ZT CC) 3/4");
			break;
		case 26:
			str = _T("64-QAM(ZT CC) 1/2");
			break;
		case 27:
			str = _T("64-QAM(ZT CC) 2/3");
			break;
		case 28:
			str = _T("64-QAM(ZT CC) 3/4");
			break;
		case 29:
			str = _T("QPSK(LDPC) 1/2");
			break;
		case 30:
			str = _T("QPSK(LDPC) 2/3 A code");
			break;
		case 31:
			str = _T("QPSK(LDPC) 3/4 A code");
			break;
		case 32:
			str = _T("16-QAM(LDPC) 1/2");
			break;
		case 33:
			str = _T("16-QAM(LDPC) 2/3 A code");
			break;
		case 34:
			str = _T("16-QAM(LDPC) 3/4 A code");
			break;
		case 35:
			str = _T("64-QAM(LDPC) 1/2");
			break;
		case 36:
			str = _T("64-QAM(LDPC) 2/3 A code");
			break;
		case 37:
			str = _T("64-QAM(LDPC) 3/4 A code");
			break;
		case 38:
			str = _T("QPSK(LDPC) 2/3 B code");
			break;
		case 39:
			str = _T("QPSK(LDPC) 3/4 B code");
			break;
		case 40:
			str = _T("16-QAM(LDPC) 2/3 B code");
			break;
		case 41:
			str = _T("16-QAM(LDPC) 3/4 B code");
			break;
		case 42:
			str = _T("64-QAM(LDPC) 2/3 B code");
			break;
		case 43:
			str = _T("64-QAM(LDPC) 3/4 B code");
			break;
		case 44:
			str = _T("QPSK(CC with optional interleaver) 1/2");
			break;
		case 45:
			str = _T("QPSK(CC with optional interleaver) 3/4");
			break;
		case 46:
			str = _T("16-QAM(CC with optional interleaver) 1/2");
			break;
		case 47:
			str = _T("16-QAM(CC with optional interleaver) 3/4");
			break;
		case 48:
			str = _T("64-QAM(CC with optional interleaver) 2/3");
			break;
		case 49:
			str = _T("64-QAM(CC with optional interleaver) 3/4");
			break;
		case 50:
			str = _T("QPSK(LDPC) 5/6");
			break;
		case 51:
			str = _T("16-QAM(LDPC) 5/6");
			break;
		case 52:
			str = _T("64-QAM(LDPC) 5/6");
			break;
		default:
			str = _T("Unknown");
			break;
	}

	return str;
}

static int refresh_rf_info(GDEV_ID *ID)
{
	static int last_recv_PERErrorCount;
	static int last_recv_PERReceiveCount;
	static int base_PERErrorCount;
	static int base_PERReceiveCount;

	dev_info_t *di = &dev_info[ID->deviceIndex];
	GCT_API_RF_INFORM rf_info;
	s8 cinr1, cinr2, rssi1, rssi2;
	float per;

	if (GAPI_GetRFInform(ID, &rf_info) != GCT_API_RET_SUCCESS) {
		cm_printf("Get rf info fail\n");
		return -1;
	}

	cinr1 = (s8) rf_info.CINR-10;
	cinr2 = (s8) rf_info.CINR2-10;
	rssi1 = (s8) rf_info.RSSI-123;
	rssi2 = (s8) rf_info.RSSI2-123;

	cm_printf("[RF Information]\n");

	cm_printf("BSID: %02X %02X %02X %02X %02X %02X\n",
		rf_info.bsId[0], rf_info.bsId[1], rf_info.bsId[2],
		rf_info.bsId[3], rf_info.bsId[4], rf_info.bsId[5]);
	cm_printf("UL PermBase: %d\n", rf_info.ULPermBase);
	cm_printf("DL PermBase: %d\n", rf_info.DLPermBase);
	cm_printf("Current preamble index: %d\n", rf_info.CurrentPreambleIndex);
	cm_printf("Previous preamble index: %d\n", rf_info.PreviousPreambleIndex);
	cm_printf("HO count: %d\n", rf_info.HandOverCount);
	cm_printf("HO fail count: %d\n", rf_info.HandOverFailCount);
	cm_printf("Resync count: %d\n", rf_info.ResyncCount);
	cm_printf("HO signal latency: %d\n", rf_info.HoSignalLatency);
	cm_printf("Combined CINR: %d\n", combine_level(ID->deviceIndex, cinr1, cinr2));
	cm_printf("CINR: %d\n", cinr1);
	cm_printf("CINR deviation: %.4f\n", calc_deviation(di->cinr1.value, ++di->cinr1.cnt));
	cm_printf("CINR2: %d\n", cinr2);
	cm_printf("CINR2 deviation: %.4f\n", calc_deviation(di->cinr2.value, ++di->cinr2.cnt));
	cm_printf("Combined RSSI: %d\n", combine_level(ID->deviceIndex, rssi1, rssi2));
	cm_printf("RSSI: %d\n", rssi1);
	cm_printf("RSSI deviation: %.4f\n", calc_deviation(di->rssi1.value, ++di->rssi1.cnt));
	cm_printf("RSSI2: %d\n", rssi2);
	cm_printf("RSSI2 deviation: %.4f\n", calc_deviation(di->rssi2.value, ++di->rssi2.cnt));
	
	if (rf_info.nPERReceiveCount) {
		last_recv_PERErrorCount = rf_info.nPERErrorCount;
		last_recv_PERReceiveCount = rf_info.nPERReceiveCount;
		if (base_PERErrorCount > last_recv_PERErrorCount)
			base_PERErrorCount = last_recv_PERErrorCount;
		if (base_PERReceiveCount > last_recv_PERReceiveCount)
			base_PERReceiveCount = last_recv_PERReceiveCount;

		if (last_recv_PERReceiveCount-base_PERReceiveCount) {
			per = ((float)last_recv_PERErrorCount-(float)base_PERErrorCount)/
				((float)last_recv_PERReceiveCount-(float)base_PERReceiveCount);
		} else
			per = 0;

		cm_printf("PER: %.6f [%d/%d]\n", per, 
						last_recv_PERErrorCount-base_PERErrorCount, 
						last_recv_PERReceiveCount-base_PERReceiveCount);
	}
	else
		cm_printf("PER: --\n");
	
	cm_printf("Power control mode: %d\n", rf_info.PowerControlMode);
	cm_printf("Tx power: %d\n", (s8) (rf_info.TxPower/2-84));
	cm_printf("Tx power maximum: %d\n", (s8) (rf_info.TxPowerMax/2-84));
	cm_printf("Tx power headroom: %d\n", (s8)rf_info.TxPowerMax-(s8)rf_info.TxPower);
	cm_printf("UL burst data FEC scheme: %s\n", fec_string(rf_info.ULBurstDataFECScheme));
	cm_printf("DL burst data FEC scheme: %s\n", fec_string(rf_info.DLBurstDataFECScheme));
	cm_printf("UL burst data UIUC: %02d\n", rf_info.ULBurstDataUIUC);
	cm_printf("DL burst data DIUC: %02d\n", rf_info.DLBurstDataDIUC);
	cm_printf("Frequency: %d\n", (int)rf_info.Frequency);

	return 0;
}

static int cmd_get_link_status(int argc, char *argv[])
{
	WIMAX_API_LINK_STATUS_INFO LinkStatus;
	GDEV_ID ID;

	ID.apiHandle = cm_api_handle;
	if (argc == 1)
		ID.deviceIndex = DEFAULT_DEVICE;
	else
		ID.deviceIndex = atoi(argv[1]);

	if (GAPI_GetLinkStatus(&ID, &LinkStatus) != GCT_API_RET_SUCCESS) {
		cm_printf("Get link status fail\n");
		return -1;
	}

	cm_printf("[RSSI] [CINR] [TX-PWR] [NAP-ID]\n");
	cm_printf("%5d %6d %8d   %02X%02X%02X\n",
		LinkStatus.RSSI-123,
		LinkStatus.CINR-10,
		(s8) (LinkStatus.txPWR/2-84),
		LinkStatus.bsId[0], LinkStatus.bsId[1], LinkStatus.bsId[2]);

	return refresh_rf_info(&ID);
}

static int cm_connect_net(int dev_idx, WIMAX_API_WSTRING nsp_name,
	WIMAX_API_PROFILE_ID profileID)
{
	GDEV_ID ID;

	ID.apiHandle = cm_api_handle;
	ID.deviceIndex = dev_idx;
	dev_conf_t *conf = &cm_dev_conf[dev_idx];

	wcscpy((wchar_t*)conf->nsp_name16_before, (wchar_t*)conf->nsp_name16_current);

	if (GAPI_CmdConnectToNetwork(&ID, nsp_name, profileID) != GCT_API_RET_SUCCESS) {
		cm_eprintf("Connect failure\n");
		return -1;
	}

	wcscpy((wchar_t*)conf->nsp_name16_current, (wchar_t*)nsp_name);

	return ID.deviceIndex;
}

static int cmd_connect_net(int argc, char *argv[])
{
	u16 nsp_name16[MAX_SIZE_OF_NSP_NAME];
	int dev_idx = DEFAULT_DEVICE;
	WIMAX_API_PROFILE_ID profileID = 0;

	if (argc < 2 && argc > 4) {
		cm_eprintf("Invalid parameter.\n");
		return -1;
	}

	if (argc > 2) {
		if (strlen(argv[2]) <= 1)
			dev_idx = atoi(argv[2]);
		else {
			profileID = strtoul(argv[2], NULL, 16);
			if (argc == 4)
				dev_idx = atoi(argv[3]);
		}
	}
	mbstowcs((wchar_t*)nsp_name16, argv[1], strlen(argv[1])+1);

	return cm_connect_net(dev_idx, (WIMAX_API_WSTRING)nsp_name16, profileID);
}

void cm_enable_auto_connet(int enable, int dev_idx)
{
	cm_common_conf_t *pconf = &cm_common_conf;
	pconf->auto_connect_enable = enable;
	if (enable)
		cm_request_auto_connection(dev_idx);
}

static int cmd_auto_connect_net(int argc, char *argv[])
{
	int dev_idx = DEFAULT_DEVICE;
	int enable = 0;

	if (argc < 2) {
invalid_param:
		cm_eprintf("Invalid parameter\n");
		return -1;
	}
	if (!strcasecmp(argv[1], "on"))
		enable = 1;
	else if (strcasecmp(argv[1], "off"))
		goto invalid_param;

	if (argc > 2)
		dev_idx = atoi(argv[1]);

	cm_enable_auto_connet(enable, dev_idx);
	return dev_idx;
}

static WIMAX_API_NSP_INFO_P select_nsp(WIMAX_API_NSP_INFO_P nsp, int nsp_cnt)
{
	cm_common_conf_t *pconf = &cm_common_conf;
	WIMAX_API_NSP_INFO_P mnsp;
	WIMAX_API_NSP_INFO_P home, partner, rpartner;
	UINT8 cinr = 0;
	int i;

	mnsp = nsp;
	rpartner = partner = home = NULL;
	for (i = 0; i < nsp_cnt; i++, nsp++) {
		switch (nsp->networkType) {
			case WIMAX_API_HOME:
				if (!home)
					home = nsp;
				break;
			case WIMAX_API_PARTNER:
				if (!partner)
					partner = nsp;
				break;
			case WIMAX_API_ROAMING_PARTNER:
				if (!rpartner)
					rpartner = nsp;
				break;
			case WIMAX_API_UNKNOWN:
				if (nsp->CINR > cinr) {
					cinr = nsp->CINR;
					mnsp = nsp;
				}
				break;
			default:
				cm_printf("Unknown Network Type");
				break;
		}
	}

	if (home != NULL) {
		cm_printf("HOME Operator %S is selected\n", (wchar_t *)home->NSPName);
	   	return home;
	}
	if (partner != NULL) {
		cm_printf("CAPL Operator %S is selected\n", (wchar_t *)partner->NSPName);
	   	return partner;
	}
	if (rpartner != NULL) {
		cm_printf("RAPL Operator %S is selected\n", (wchar_t *)rpartner->NSPName);
	   	return rpartner;
	}

	if (pconf->unknown_net_auto_connect_enable) {
		cm_printf("No known network is found, Selecting NSP with better CINR\n");
		return mnsp;
	}
	else {
		cm_printf("No known network is not selected!\n");
		return NULL;
	}
}

int cm_profiling_rf_up(int dev_idx)
{
	cm_common_conf_t *pconf = &cm_common_conf;
	int profile_idx = pconf->auto_select_profile_index;
	int ret;

	if ((ret = cm_get_profile_list(dev_idx)) <= 0) {
		cm_printf("There is no proile!\n");
		goto out;
	}

	cm_printf("Profile index is %d\n", profile_idx);
	if ((ret = cm_set_profile(dev_idx, profile_idx)) < 0)
		goto out;

	if ((ret = cm_rf_up(dev_idx)) < 0)
		goto out;
out:
	return ret;
}

int cm_auto_connect(int dev_idx)
{
	cm_common_conf_t *pconf = &cm_common_conf;
	WIMAX_API_NSP_INFO nsp[MAX_NETWORK_LIST], *selected_nsp = NULL;
	WIMAX_API_PROFILE_ID profileID = 0;
	int list_cnt, waiting_timeout_sec = NETWORK_LIST_TIMEOUT_SEC;
	unsigned connect_retries = pconf->auto_connect_retry_count;
	GDEV_ID ID;
	WIMAX_API_DEVICE_STATUS DeviceStatus;
	WIMAX_API_CONNECTION_PROGRESS_INFO ConnectionProgressInfo;
	cm_msg_cb_t *msg_cb;
	time_t start_time, cur_time;
	int ret = -1;

	msg_cb = &pconf->auto_conn_msg;
	ID.apiHandle = cm_api_handle;
	ID.deviceIndex = dev_idx;

	if (waiting_timeout_sec == 0)
		waiting_timeout_sec = 1;
	if (connect_retries == 0)
		connect_retries = 1;

	if (GAPI_GetDeviceStatus(&ID, &DeviceStatus, &ConnectionProgressInfo)
		!= GCT_API_RET_SUCCESS) {
		cm_eprintf("GAPI_GetDeviceStatus Failure\n");
		goto out;
	}

	if (DeviceStatus == WIMAX_API_DEVICE_STATUS_Connecting ||
		DeviceStatus == WIMAX_API_DEVICE_STATUS_Data_Connected) {
		cm_eprintf("DeviceStatus is not connectable state(%d)\n\n", DeviceStatus);
		goto out;
	}

	if (cm_profiling_rf_up(dev_idx) <= 0)
		goto out;

get_net_list:
	time(&start_time);
	do {
		sleep(1);
		if (!pconf->auto_connect_enable)
			goto out;
		list_cnt = MAX_NETWORK_LIST;
		if (cm_get_net_list(dev_idx, nsp, &list_cnt) < 0)
			goto out;
		time(&cur_time);
		if (list_cnt == 0)
			cm_printf("network list=%d (elapsed %dsec.)\n",
				list_cnt, (unsigned)cur_time-(unsigned)start_time);
		if (list_cnt) {
			cm_print_net_list(nsp, list_cnt);
			if ((selected_nsp = select_nsp(nsp, list_cnt)))
				goto connect;
		}
	} while (--waiting_timeout_sec);

	if (waiting_timeout_sec == 0) {
		cm_eprintf("There is no scan list\n");
		goto out;
	}
connect:
	if (selected_nsp && pconf->auto_connect_enable) {
		cm_printf("Connect to %S\n", (wchar_t *) selected_nsp->NSPName);
		if (cm_connect_net(dev_idx, selected_nsp->NSPName, profileID) < 0) {
			if (--connect_retries)
				goto get_net_list;
			ret = -1;
		}
		ret = 0;
	}
out:
	cm_printf("Auto connect done\n");
	return ret;
}

int cm_disconnect_net(int dev_idx)
{
	GDEV_ID ID;

	ID.apiHandle = cm_api_handle;
	ID.deviceIndex = dev_idx;

	if (GAPI_CmdDisconnectFromNetwork(&ID) != GCT_API_RET_SUCCESS) {
		cm_printf("Disconnect failure\n");
		return -1;
	}

	return ID.deviceIndex;
}

static int cmd_disconnect_net(int argc, char *argv[])
{
	int dev_idx;

	if (argc == 1)
		dev_idx = DEFAULT_DEVICE;
	else
		dev_idx = atoi(argv[1]);

	return cm_disconnect_net(dev_idx);
}

static int cmd_set_autoscan_interval(int argc, char *argv[])
{
	GDEV_ID ID;
	int interval_sec;

	if (argc < 2) {
		cm_printf("Command usage: %s interval(second)\n", argv[0]);
		return -1;
	}

	interval_sec = atoi(argv[1]);

	ID.apiHandle = cm_api_handle;
	if (argc == 2)
		ID.deviceIndex = DEFAULT_DEVICE;
	else
		ID.deviceIndex = atoi(argv[1]);

	if (GAPI_SetScanInterval(&ID, interval_sec) != GCT_API_RET_SUCCESS) {
		cm_printf("Setting Scan-Interval failure!\n");
		return -1;
	}
	else
		cm_printf("Set autoscan interval(%d second)\n", interval_sec);

	return ID.deviceIndex;
}

static int cmd_scan(int argc, char *argv[])
{
	GDEV_ID ID;
	int scan_mode;
	GCT_API_SCAN_TYPE type;
	#define WIDE_SCAN			0
	#define ALL_SUBS_SCAN		1
	#define CURR_SUBS_SCAN		2
	const char *scan_type = NULL;

	if (argc < 2 || (scan_mode = atoi(argv[1])) > CURR_SUBS_SCAN) {
		cm_printf("Command usage: %s scan-mode\n"
			"                       scan-mode 0: wide scan\n"
			"                       scan-mode 1: all subscriptions scan\n"
			"                       scan-mode 2: current subscription scan\n", argv[0]);
		return -1;
	}

	ID.apiHandle = cm_api_handle;
	if (argc == 2)
		ID.deviceIndex = DEFAULT_DEVICE;
	else
		ID.deviceIndex = atoi(argv[2]);

	if (scan_mode == WIDE_SCAN) {
		type = GCT_API_SCAN_WIDE;
		scan_type = "wide";
	}
	else if (scan_mode == ALL_SUBS_SCAN) {
		type = GCT_API_SCAN_ALL_SUBSCRIPTIONS;
		scan_type = "all subscriptions";
	}
	else {
		type = GCT_API_SCAN_CURR_SUBSCRIPTION;
		scan_type = "current subscription";
	}

	if (GAPI_NetworkSearchScan(&ID, type) != GCT_API_RET_SUCCESS) {
		cm_printf("Scaning network failure\n");
		return -1;
	}
	else
		cm_printf("Request %s scan!\n", scan_type);

	return ID.deviceIndex;
}

static int cmd_auth(int argc, char *argv[])
{
	cm_printf("This command is not used no more.\n");
	cm_printf("Authentication will be automatically set according to cm.conf configuration.\n");
	return 0;
}

static int cmd_delete_cert(int argc, char *argv[])
{
	cm_printf("%s: not supported!\n", __func__);
		return -1;
	}

static int cmd_get_cert_status(int argc, char *argv[])
{
	cm_printf("%s: not supported!\n", __func__);
		return -1;
	}

static int cmd_get_cert_mask(int argc, char *argv[])
{
	cm_printf("%s: not supported!\n", __func__);
		return -1;
	}

static int cmd_set_cert_mask(int argc, char *argv[])
{
	cm_printf("%s: not supported!\n", __func__);
		return -1;
	}

static int cmd_get_cert_info(int argc, char *argv[])
{
	cm_printf("%s: not supported!\n", __func__);
		return -1;
	}

static int cmd_debug_level(int argc, char *argv[])
{
	cm_common_conf_t *pconf = &cm_common_conf;
	int level, prev_level;

	if (argc < 2) {
		cm_printf("Invalid parameter.\n");
		return -1;
	}

	level = atoi(argv[1]);

	if (GAPI_SetDebugLevel(cm_api_handle, level, &prev_level) != GCT_API_RET_SUCCESS) {
		cm_printf("Debug level failure\n");
		return -1;
	}
	if (level != GAPI_LOG_FLUSH_LEVEL)
		pconf->log_level = level;
	return get_first_odev();
}

static int cmd_help(int argc, char *argv[])
{
	if (argc == 2)
		print_cmd_list(argv[1]);
	else
		print_cmd_list(NULL);
	return 0;
}

static void request_dm_prompt(void)
{
	GDEV_ID ID;

	ID.apiHandle = cm_api_handle;
	ID.deviceIndex = get_first_odev();

	if (ID.deviceIndex)
		send_print_string(&ID, "\n", 1);
}

static void do_shell(const char *shell)
{
	#define DISABLE_SIGINT
	pid_t pid;
	int status, i;
	#if defined(DISABLE_SIGINT)
	struct sigaction act, oldact;

	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;
	sigaction(SIGINT, &act, &oldact);
	#endif

	pid = fork();
	if (pid == 0) {
		/*Close all FDs*/
		for (i = 3; i < 64; i++)
			close(i);
		#if defined(DISABLE_SIGINT)
		sigaction(SIGINT, &oldact, NULL);
		#endif
		/* Invokes a shell */
		execl(shell, shell, NULL);
		cm_eprintf("execl failed\n");
		exit(1);
	}
	else if (pid != -1) {
		waitpid(pid, &status, 0);
		#if defined(DISABLE_SIGINT)
		sigaction(SIGINT, &oldact, NULL);
		#endif
		if ((!WIFEXITED(status) || WEXITSTATUS(status)))
			cm_printf("waitpid: status=%d\n", WEXITSTATUS(status));
		request_dm_prompt();
	}
	else
		cm_eprintf("fork failed\n");
}

static int cmd_shell(int argc, char *argv[])
{
	#define MY_SHELL	"/bin/sh"

	do_shell(MY_SHELL);
	return 0;
}

static int cmd_exit(int argc, char *argv[])
{
	return EXIT_CM_CMD;
}

static void cmd_init_event(int dev_idx)
{
	sem_init(&hci_data_sem[dev_idx], 0);
}

static char *cmd_wait_event(int dev_idx, unsigned short event, int second, int *len)
{
	int ret;

	ret = sem_timedwait(&hci_data_sem[dev_idx], second);
	if (ret) {
		cm_printf("Event(0x%04X) time out(%d)\n", event, ret);
		return NULL;
	}

	if (len)
		*len = hci_data[dev_idx].len;
	return hci_data[dev_idx].data;
}

void cmd_signal_event(int dev_idx, unsigned short event, char *data, int len)
{
	memcpy(hci_data[dev_idx].data, data, len);
	hci_data[dev_idx].len = len;
	sem_signal(&hci_data_sem[dev_idx]);
}

#define HCI_FILE_BUF_CHUNK	1024

static int check_file_arg(int argc, char *file)
{
	if (argc < 3) {
		cm_printf("Invalid parameter.\n");
		return -1;
	}

	if (file && access(file, 0)) {
		cm_printf("%s is not found\n", file);
		return -1;
	}

	return 0;
}

int cm_read_fw_file(GDEV_ID_P pID, char *host_file, char *target_file)
{
	int dev_idx = pID->deviceIndex;
	char hci_buf[HCI_MAX_PACKET];
	hci_t *hci = (hci_t *) hci_buf;
	int hci_length;
	hci_file_response_t *rsp;
	hci_file_read_t *p = (hci_file_read_t *) hci->data;
	int len;
	unsigned offset = 0;
	int fd, ret = -1;

	if ((fd = open(host_file, O_CREAT|O_WRONLY|O_TRUNC, 0644)) < 0) {
		cm_printf("Open failed, %s, %s(%d)\n", host_file, strerror(errno), errno);
		return -1;
	}

	hci->cmd_evt = _H2B(WIMAX_READ_FILE);
	strcpy(p->path, target_file);
	hci_length = HCI_HEADER_SIZE + sizeof(hci_file_read_t) + strlen(p->path) + 1;
	hci->length = _H2B(hci_length-HCI_HEADER_SIZE);

	cmd_init_event(dev_idx);

	while (1) {
		p->offset = _DH2B(offset);

		if (GAPI_WriteHCIPacket(pID, hci_buf, hci_length) != GCT_API_RET_SUCCESS) {
			cm_printf("Write HCI failed! (Reading %s)\n", target_file);
			goto out;
		}

		rsp = (hci_file_response_t *) cmd_wait_event(dev_idx, WIMAX_FILE_RESULT, 5, NULL);
		if (!rsp)
			break;
		
		ret = _DB2H(rsp->result);
		if (!ret)
			break;
		if (ret < 0) {
			cm_printf("Read file failed(%d)\n", ret);
			break;
		}
		len = write(fd, file_response_data(rsp), ret);
		if (len <= 0) {
			cm_printf("Write failed, %s, %s(%d)\n", host_file, strerror(errno), errno);
			ret = -1;
			break;
		}
		offset += ret;
		if (len < ret)
			lseek(fd, offset, SEEK_SET);
	}

	if (!ret)
		cm_printf("\t%s <= %s\n", host_file, target_file);
out:
	close(fd);
	return pID->deviceIndex;
}

static int cmd_read_file(int argc, char *argv[])
{
	GDEV_ID ID;
	char *host_file = argv[1];
	char *target_file = argv[2];

	if (check_file_arg(argc, NULL) < 0)
		return -1;

	ID.apiHandle = cm_api_handle;
	ID.deviceIndex = (argc == 4 ? atoi(argv[3]) : DEFAULT_DEVICE);

	return cm_read_fw_file(&ID, host_file, target_file);
}

static int cmd_write_file(int argc, char *argv[])
{
	GDEV_ID ID;
	int dev_idx;
	char hci_buf[HCI_MAX_PACKET];
	hci_t *hci = (hci_t *) hci_buf;
	int fix_length, hci_length;
	hci_file_response_t *rsp;
	hci_file_write_t *p = (hci_file_write_t *) hci->data;
	char *buf;
	int buf_size = HCI_FILE_BUF_CHUNK;
	int len;
	unsigned offset = 0, total;
	char *host_file = argv[1];
	char *target_file = argv[2];
	int fd, ret = -1;

	if (check_file_arg(argc, host_file) < 0)
		return -1;

	ID.apiHandle = cm_api_handle;
	ID.deviceIndex = dev_idx = (argc == 4 ? atoi(argv[3]) : DEFAULT_DEVICE);

	if ((fd = open(host_file, O_RDONLY)) < 0) {
		cm_printf("Open failed, %s, %s(%d)\n", host_file, strerror(errno), errno);
		return -1;
	}

	total = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	hci->cmd_evt = _H2B(WIMAX_WRITE_FILE);
	strcpy(p->path, target_file);
	p->path_len = strlen(p->path) + 1;
	fix_length = sizeof(hci_file_write_t) + p->path_len;
	buf = hci_file_data(p);
	p->path_len = _H2B(p->path_len);

	cmd_init_event(dev_idx);
	
	while ((len = read(fd, buf, buf_size)) > 0) {
		p->offset = _DH2B(offset);
		hci->length = _H2B(fix_length + len);
		hci_length = HCI_HEADER_SIZE + fix_length + len;

		if (GAPI_WriteHCIPacket(&ID, hci_buf, hci_length) != GCT_API_RET_SUCCESS) {
			cm_printf("Write-HCI failed\n");
			goto out;
		}

		rsp = (hci_file_response_t *) cmd_wait_event(dev_idx, WIMAX_FILE_RESULT, 5, NULL);
		if (!rsp)
			break;
		
		ret = _DB2H(rsp->result);
		if (!ret)
			break;
		if (ret < 0) {
			cm_printf("Write-file failed(%d)\n", ret);
			break;
		}
		offset += ret;
		if (len > ret)
			lseek(fd, offset, SEEK_SET);
	}

	if (len < 0)
		cm_printf("Read failed, %s, %s(%d)\n", host_file, strerror(errno), errno);
	else {
		/*Notify EOF*/
		p->offset = _DH2B(EOF_OFFSET);
		hci->length = _H2B(fix_length);
		hci_length = HCI_HEADER_SIZE + fix_length;

		if (GAPI_WriteHCIPacket(&ID, hci_buf, hci_length) != GCT_API_RET_SUCCESS) {
			cm_printf("Write-HCI failed\n");
			return -1;
		}

		rsp = (hci_file_response_t *) cmd_wait_event(dev_idx, WIMAX_FILE_RESULT, 5, NULL);
		if (rsp) {
			ret = _DB2H(rsp->result);
			if (ret < 0)
				cm_printf("Write-file failed(%d)\n", ret);
		}

		if (total == offset)
			cm_printf("\t%s => %s\n", host_file, target_file);
		else
			cm_printf("\tWrite-file mismatch(%d!=%d)\n", total, offset);
	}
out:
	close(fd);
	return ID.deviceIndex;
}

static int cmd_delete_file(int argc, char *argv[])
{
	GDEV_ID ID;
	int dev_idx;
	char hci_buf[HCI_MAX_PACKET];
	hci_t *hci = (hci_t *) hci_buf;
	int hci_length;
	hci_file_response_t *rsp;
	hci_file_delete_t *p = (hci_file_delete_t *) hci->data;
	char *target_file = argv[1];
	int path_len;
	int ret = -1;

	ID.apiHandle = cm_api_handle;
	ID.deviceIndex = dev_idx = (argc == 4 ? atoi(argv[3]) : DEFAULT_DEVICE);

	hci->cmd_evt = _H2B(WIMAX_DELETE_FILE);
	strcpy(p->path, target_file);
	path_len = strlen(p->path) + 1;
	hci_length = sizeof(hci_file_delete_t) + path_len;
	hci->length = _H2B(hci_length);
	hci_length +=HCI_HEADER_SIZE;

	cmd_init_event(dev_idx);

	if (GAPI_WriteHCIPacket(&ID, hci_buf, hci_length) != GCT_API_RET_SUCCESS) {
		cm_printf("Write-HCI failed\n");
		return -1;
	}

	rsp = (hci_file_response_t *) cmd_wait_event(dev_idx, WIMAX_FILE_RESULT, 5, NULL);
	if (rsp) {
		ret = _DB2H(rsp->result);
		if (ret < 0)
			cm_printf("Delete-file failed(%d)\n", ret);
		else
			cm_printf("\t%s deleted\n", target_file);
	}

	return ID.deviceIndex;
}

static int read_image(GDEV_ID_P pID, int type, const char *file)
{
	cm_printf("%s: not supported!\n", __func__);
			return -1;
		}

static int cmd_read_image(int argc, char *argv[])
{
	GDEV_ID ID;
	int type;
	char *file;
	int ret;

	type = strtoul(argv[1], NULL, 0);
	file = argv[2];

	ID.apiHandle = cm_api_handle;
	ID.deviceIndex = (argc == 4 ? atoi(argv[3]) : DEFAULT_DEVICE);

	switch (type) {
		case DLIMG_BL_EEPROM:
		case DLIMG_OMA_XML:
		case DLIMG_DEV_CERT:
		case DLIMG_CERT1_U:
		case DLIMG_CERT1_L:
		case DLIMG_CERT2_U:
		case DLIMG_CERT_BIG:
			break;
		default:
			cm_printf("0x%x is unknown type\n", type);
			return 0;
			break;
	}

	ret = read_image(&ID, type, file);
	if (ret >= 0)
		ret = ID.deviceIndex;
	return ret;
}

static int cmd_write_image(int argc, char *argv[])
{
	cm_printf("%s: not supported!\n", __func__);
		return -1;
	}

static int control_image(GDEV_ID_P pID, int type, int cmd, void *buffer, int length)
{
	int dev_idx = pID->deviceIndex;
	char hci_buf[HCI_MAX_PACKET];
	hci_t *hci = (hci_t *) hci_buf;
	int hci_length;
	int data_len = 0, len;
	hci_image_cmd_t *p = (hci_image_cmd_t *) hci->data;
	hci_image_cmd_result_t *rsp;
	int ret = 0;
	
	hci->cmd_evt = _H2B(WIMAX_IMAGE_CMD);
	hci_length = sizeof(*p);
	hci->length = _H2B(hci_length);
	p->cmd = _H2B(cmd);
	p->type = _H2B(type);

	cm_dprintf("cmd=%d, type=0x%x\n", cmd, type);

	hci_length += HCI_HEADER_SIZE;
	if (GAPI_WriteHCIPacket(pID, hci_buf, hci_length) != GCT_API_RET_SUCCESS) {
		cm_printf("Write-HCI failed\n");
		ret = -1;
		goto out;
	}

	rsp = (hci_image_cmd_result_t *) cmd_wait_event(dev_idx, WIMAX_IMAGE_CMD_STATUS,
		3, &data_len);
	if (!rsp) {
		ret = -1;
		goto out;
	}

	ret = _DB2H(rsp->status);
	if (ret) {
		if (cmd == ICMD_GET_SIZE) {
			if (buffer)
				memcpy(buffer, &rsp->status, sizeof(rsp->status));
			ret = 0;
		}
		else {
			cm_printf("Image result failed(%d)\n", ret);
			ret = -1;
			goto out;
		}
	}
	else if ((len = data_len-sizeof(hci_image_cmd_result_t))) {
		if (len > length)  {
			cm_eprintf("Buffer length is too small(%d)\n", length);
			ret = -1;
		}
		else if (buffer)
			memcpy(buffer, rsp->data, len);
	}
out:
	return ret;
}

static int cmd_control_image(int argc, char *argv[])
{
	GDEV_ID ID;
	int type, cmd;
	char buffer[16];
	int ret;

	if (argc < 3) {
		cm_eprintf("Invalid parameter\n");
		return 0;
	}

	type = strtoul(argv[1], NULL, 0);
	cmd = strtoul(argv[2], NULL, 0);

	ID.apiHandle = cm_api_handle;
	ID.deviceIndex = (argc == 4 ? atoi(argv[3]) : DEFAULT_DEVICE);

	switch (type) {
		case DLIMG_DEV_CERT:
		case DLIMG_CERT1_U:
		case DLIMG_CERT1_L:
		case DLIMG_CERT2_U:
		case DLIMG_EAP_PARAM:
		case DLIMG_CERT_BIG:
			break;
		default:
			cm_printf("0x%x is unknown type\n", type);
			goto out;
	}

	if (cmd == ICMD_INVALIDATE) {
		cm_printf("0x%x will be deleted! Are you sure (y/n) ? ", type);
		if (!fgets(buffer, sizeof(buffer), stdin)) {
			cm_eprintf("fgets error: %s(%d)\n", strerror(errno), errno);
			goto out;
		}
		if (buffer[0] != 'y')
			goto out;
	}

	ret = control_image(&ID, type, cmd, buffer, sizeof(buffer));
	if (!ret) {
		switch (cmd) {
			case ICMD_INVALIDATE:
				cm_printf("Type(0x%x) Deleted!\n", type);
				break;
			case ICMD_GET_SIZE:
				cm_printf("Type(0x%x) Size: %u.\n", type, _U82U32(buffer));
				break;
			case ICMD_GET_CRC32:
				cm_printf("Type(0x%x) CRC32: 0x%x.\n", type, _U82U32(buffer));
				break;
		}
	}
out:
	return ID.deviceIndex;
}

static int read_param_block(GDEV_ID_P pID, void *buf, int offset, int length)
{
	int dev_idx = pID->deviceIndex;
	char hci_buf[HCI_MAX_PACKET];
	hci_t *hci = (hci_t *) hci_buf;
	int hci_length;
	int data_len = 0;
	hci_image_payload_t *p = (hci_image_payload_t *) hci->data;
	int ret = 0;

	cmd_init_event(dev_idx);
	
	hci->cmd_evt = _H2B(WIMAX_UL_IMAGE);
	hci->length = _H2B(sizeof(*p));
	hci_length = HCI_HEADER_SIZE + sizeof(*p);
	p->type = _H2B(DLIMG_CFG);
	p->offset = _DH2B(offset);

	if (GAPI_WriteHCIPacket(pID, hci_buf, hci_length) != GCT_API_RET_SUCCESS) {
		cm_printf("Write-HCI failed\n");
		ret = -1;
		goto out;
	}

	p = (hci_image_payload_t *) cmd_wait_event(dev_idx, WIMAX_UL_IMAGE_RESULT, 5, &data_len);
	if (!p) {
		ret = -1;
		goto out;
	}

	if (data_len != sizeof(hci_image_payload_t)+length) {
		cm_printf("Length mismatch(%d!=%d)\n", data_len, length);
		ret = -1;
		goto out;
	}

	memcpy(buf, p->data, data_len);

out:
	return ret;
}

static int write_param_block(GDEV_ID_P pID, void *buf, int offset, int length)
{
	int dev_idx = pID->deviceIndex;
	char hci_buf[HCI_MAX_PACKET];
	hci_t *hci = (hci_t *) hci_buf;
	int hci_length;
	int data_len = 0;
	hci_image_payload_t *p = (hci_image_payload_t *) hci->data;
	hci_image_response_t *rsp;
	int ret = 0;

	cmd_init_event(dev_idx);
	
	hci->cmd_evt = _H2B(WIMAX_DL_IMAGE);
	hci_length = sizeof(*p) + length;
	hci->length = _H2B(hci_length);
	p->type = _H2B(DLIMG_CFG);
	p->offset = _DH2B(offset);
	memcpy(p->data, buf, length);

	hci_length += HCI_HEADER_SIZE;
	if (GAPI_WriteHCIPacket(pID, hci_buf, hci_length) != GCT_API_RET_SUCCESS) {
		cm_printf("Write-HCI failed\n");
		ret = -1;
		goto out;
	}

	rsp = (hci_image_response_t *) cmd_wait_event(dev_idx, WIMAX_DL_IMAGE_STATUS, 5, &data_len);
	if (!rsp) {
		ret = -1;
		goto out;
	}

	ret = _DB2H(rsp->result);
	if (ret) {
		cm_printf("Image result failed(%d)\n", ret);
		ret = -1;
		goto out;
	}

out:
	return ret;
}

static const char nv_param_usage[] = "<operation> [opeion]"
								"\n            <operation>"
								"\n              w.info	Write NV info block parameters to NVRAM."
								"\n              v.info	Print NV info block parameters."
								"\n              [options]"
								"\n                macaddr [6 bytes hex digits (Mac Address)]";

static int is_valid_mac(char *mac)
{
	return (!(mac[0] & 0x1)
			&& (mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5])
		   );
}

static int cmd_nv_param(int argc, char *argv[])
{
	GDEV_ID ID;
	char buf[HCI_MAX_PACKET];
	char *operation = argv[1];
	char *param = argv[2];
	char *value = argv[3];
	int is_invalid_block = 0;
	int i, ret = -1;

	if (argc < 2)
		return -1;

	ID.apiHandle = cm_api_handle;
	ID.deviceIndex = 1; /*Support for only single device.*/
	
	if (!strcasecmp(operation, "w.info") || !strcasecmp(operation, "v.info")) {
		struct wb_info_block_v2 *info = (struct wb_info_block_v2 *) buf;
		ret = read_param_block(&ID, buf, WB_INFO_BLOCK_OFFSET_V2, sizeof(*info));
		if (ret < 0)
			goto out;

		if (_H2B(info->magic) != MAGIC || info->version != MAP_VERSION_V2)
			is_invalid_block = 1;

		if (!strcasecmp(operation, "v.info")) {
			if (is_invalid_block) {
				cm_printf("Info block is invalid(MAGIC:0x%04X, Version:%d)\n",
					_H2B(info->magic), info->version);
				goto out;
			}
			if (!strcasecmp(param, "macaddr")) {
				unsigned char *p = info->mac_address;
				cm_printf("%02X:%02X:%02X:%02X:%02X:%02X\n", p[0], p[1], p[2], p[3], p[4], p[5]);
				goto out;
			}
			goto err;
		}
		else if (!strcasecmp(operation, "w.info")) {
			if (is_invalid_block) {
				memset(info, 0, sizeof(*info));
				info->magic = _H2B(MAGIC);
				info->version = MAP_VERSION_V2;
			}

			if (!strcasecmp(param, "macaddr")) {
				char mac[6], ch[3] = {0};

				if (strlen(value) != 12)
					goto err;

				for (i = 0; i < 6; i++) {
					memcpy(ch, value + 2 * i, 2);
					mac[i] = strtoul(ch, NULL, 16);
				}
				if (!is_valid_mac(mac)) {
					cm_printf("Invalid MAC address\n");
					goto out;
				}
				memcpy((char *) info->mac_address, mac, 6);
			}
			else
				goto err;

			ret = write_param_block(&ID, buf, WB_INFO_BLOCK_OFFSET_V2, sizeof(*info));
			if (!ret)
				cm_printf("Writing is success!\n");
			goto out;
		}
	}

err:
	print_cmd_usage(argv[0]);

out:
	return ret;
}

static int run_script(int dev_idx, const char *scr_file)
{
	#define IS_COMMENT(ch)		((ch)=='#' || (ch)=='\n')
	GDEV_ID ID;
	char file[256];
	FILE *fp;
	char buf[1024];
	int ret, readn;

	if (strstr(scr_file, ".scr"))
		strcpy(file, scr_file);
	else
		sprintf(file, "%s.scr", scr_file);

	ID.apiHandle = cm_api_handle;
	ret = ID.deviceIndex = dev_idx;

	if (!(fp = fopen(file, "rt"))) {
		cm_eprintf("fopen(%s) failed!\n", file);
		return -1;
	}

	cm_printf("Begin script(%s)\n", file);

	while (fgets(buf, sizeof(buf), fp)) {
		readn = strlen(buf);
		if (IS_COMMENT(*buf))
			continue;
		cm_printf("%s", buf);
		if ((ret = do_cmd(buf, readn)) == NO_CM_CMD)
			ret = send_print_string(&ID, buf, readn);
	}
	cm_printf("End script(%s)\n", file);

	fclose(fp);
	return ret;
}

static int cmd_script(int argc, char *argv[])
{
	int ret, dev_idx;

	dev_idx = (argc == 3 ? atoi(argv[2]) : DEFAULT_DEVICE);
	ret = run_script(dev_idx, argv[1]);
	return ret;
}

static int cmd_date(int argc, char *argv[])
{
	cm_printf("NOW: %s\n", cm_get_cur_date());
	return 1;
}

static int cmd_sleep(int argc, char *argv[])
{
	int second;

	second = (argc == 2 ? atoi(argv[1]) : 1);
	cm_printf("Sleep %d...\n", second);
	sleep(second);

	return 1;
}

#define MALLOC_CHECK
#if defined(MALLOC_CHECK)
#include <malloc.h>
static void malloc_check(void)
{
	static struct mallinfo mem_info_st;
	struct mallinfo mem_info;

	mem_info = mallinfo();

	if (!memcmp(&mem_info_st, &mem_info, sizeof(struct mallinfo))) {
		cm_printf("#malloc status has not been chagned#\n");
	}
	else {
		memcpy(&mem_info_st, &mem_info, sizeof(struct mallinfo));
		cm_printf("(%d) This is the total size of memory allocated with sbrk by malloc, in bytes.\n", mem_info.arena);
		cm_printf("(%d) This is the number of chunks not in use.\n", mem_info.ordblks);
		cm_printf("(%d) This field is unused.\n", mem_info.smblks);
		cm_printf("(%d) This is the total number of chunks allocated with mmap.\n", mem_info.hblks);
		cm_printf("(%d) This is the total size of memory allocated with mmap, in bytes.\n", mem_info.hblkhd);
		cm_printf("(%d) This field is unused.\n", mem_info.usmblks);
		cm_printf("(%d) This field is unused.\n", mem_info.fsmblks);
	}
	cm_printf("(%d) This is the total size of memory occupied by chunks handed out by malloc.\n", mem_info.uordblks);
	cm_printf("(%d) This is the total size of memory occupied by free (not in use) chunks.\n", mem_info.fordblks);
	cm_printf("(%d) This is the size of the top-most releasable chunk that normally borders the end of the heap.\n", mem_info.keepcost);
}
#endif

static int cmd_chk(int argc, char *argv[])
{
	GDEV_ID ID;
	int ret;

	ID.apiHandle = cm_api_handle;
	ret = ID.deviceIndex = (argc == 2 ? atoi(argv[1]) : DEFAULT_DEVICE);

	#if defined(MALLOC_CHECK)
	malloc_check();
	#endif

	return ret;
}

static int cmd_test(int argc, char *argv[])
{
	GDEV_ID ID;
	int ret;

	ID.apiHandle = cm_api_handle;
	ret = ID.deviceIndex = (argc == 2 ? atoi(argv[1]) : DEFAULT_DEVICE);

	return ID.deviceIndex;
}

static int cmd_get_statistics(int argc, char *argv[])
{
	GDEV_ID ID;
	WIMAX_API_CONNECTION_STAT stat;
	int ret;

	ID.apiHandle = cm_api_handle;
	ret = ID.deviceIndex = (argc == 2 ? atoi(argv[1]) : DEFAULT_DEVICE);

	if (GAPI_GetStatistics(&ID, &stat) == GCT_API_RET_SUCCESS) {
		cm_printf("\tTotal Rx Byte: %u\n", stat.totalRxByte);
		cm_printf("\tTotal Tx Byte: %u\n", stat.totalTxByte);
		cm_printf("\tTotal Rx Packets: %u\n", stat.totalRxPackets);
		cm_printf("\tTotal Tx Packets: %u\n", stat.totalTxPackets);
	}
	else
		cm_eprintf("GAPI_GetStatistics failed\n");

	return ID.deviceIndex;
}

static int cm_uicc_power(int dev_idx, int on)
{
	GDEV_ID ID;
	char hci_buf[HCI_MAX_PACKET];
	hci_t *hci = (hci_t *) hci_buf;
	int len;

	ID.apiHandle = cm_api_handle;
	ID.deviceIndex = dev_idx;

	hci->cmd_evt = _H2B(WIMAX_UICC_CMD);
	len = HCI_HEADER_SIZE;

	if (on) {
		hci->data[0] = UICC_CMD_POWER_UP;
		len += 1;
		cm_printf("uicc on\n");
	}
	else {
		hci->data[0] = UICC_CMD_POWER_DOWN;
		len += 1;
		cm_printf("uicc off\n");
	}
	hci->length = _H2B(len-HCI_HEADER_SIZE);

	if (GAPI_WriteHCIPacket(&ID, hci_buf, HCI_HEADER_SIZE+len) != GCT_API_RET_SUCCESS) {
		cm_printf("Write HCI failure\n");
		return -1;
	}
	return ID.deviceIndex;
}

static int cmd_uicc(int argc, char *argv[])
{
	int dev_idx;
	int ret = -1;
	
	dev_idx = (argc == 2 ? atoi(argv[1]) : DEFAULT_DEVICE);

	if (!strcmp(argv[1], "on"))
		ret = cm_uicc_power(DEFAULT_DEVICE, 1);
	else if (!strcmp(argv[1], "off"))
		ret = cm_uicc_power(DEFAULT_DEVICE, 0);
	else {
		cm_eprintf("Not supported command: %s\n", argv[1]);
		ret = -1;
	}
	return ret;
}

#if defined(CONFIG_DM_INTERFACE)
static int cmd_dmif(int argc, char *argv[])
{
	cm_common_conf_t *pconf = &cm_common_conf;
	GDEV_ID ID;
	WIMAX_API_PROFILE_INFO profile;
	int ret, enabled = 0, profile_cnt = 1;

	if (argc < 2) {
invalid_param:
		cm_printf("Invalid parameter\n");
		return -1;
	}

	if (pconf->api_mode == GCT_WIMAX_API_PRIVILEGE_READ_ONLY) {
		cm_eprintf("Permission denied\n");
		return -1;
	}

	if (!strcasecmp(argv[1], "on"))
		enabled = 1;
	else if (strcasecmp(argv[1], "off"))
		goto invalid_param;

	ID.apiHandle = cm_api_handle;
	ret = ID.deviceIndex = (argc == 3? atoi(argv[2]) : DEFAULT_DEVICE);

	if (enabled) {
		if (pconf->dm_interface_enable) {
			cm_printf("DM Interface has been enabled already!\n");
			goto out;
		}
		if (dmif_init() < 0) {
			ret = -1;
			goto out;
		}
		pconf->dm_interface_enable = 1;
		
		if (GAPI_CmdControlPowerManagement(&ID, WIMAX_API_RF_ON) != GCT_API_RET_SUCCESS) {
			cm_printf("device[%d] rf up failure\n", ID.deviceIndex);
			ret = -1;
			goto out;
		}

		if (GAPI_GetSelectProfileList(&ID, &profile, (UINT32 *) &profile_cnt)
			!= GCT_API_RET_SUCCESS) {
			cm_printf("Get profile failure\n");
			ret = -1;
			goto out;
		}
		
		if (GAPI_SetProfile(&ID, profile.profileID) != GCT_API_RET_SUCCESS ) {
			cm_printf("Set profile failure\n");
			ret = -1;
			goto out;
		}
	}
	else {
		pconf->dm_interface_enable = 0;
		if (dmif_deinit() < 0)
			ret = -1;
	}
out:
	return ret;
}
#endif

#if defined(CONFIG_ENABLE_SERVICE_FLOW)
#include <arpa/inet.h>
static int cmd_gsf(int argc, char *argv[])
{
	GDEV_ID ID;
	WIMAX_API_DEVICE_STATUS DeviceStatus;
	WIMAX_API_CONNECTION_PROGRESS_INFO ConnectionProgressInfo;
	WIMAX_SERVICE_FLOW *pServiceFlow;
	WIMAX_CLFR_RULE *pCrRule;
	WIMAX_PHS_RULE *pPhsRule;	
	int count;
	int ret;

	ID.apiHandle = cm_api_handle;
	ret = ID.deviceIndex = (argc == 2 ? atoi(argv[1]) : DEFAULT_DEVICE);

	if (GAPI_GetDeviceStatus(&ID, &DeviceStatus, &ConnectionProgressInfo)
		!= GCT_API_RET_SUCCESS) {
		cm_eprintf("GAPI_GetDeviceStatus Failure\n");
		return -1;
	}

	if (DeviceStatus != WIMAX_API_DEVICE_STATUS_Data_Connected) {
		cm_eprintf("DeviceStatus is not connected state(%d)\n", DeviceStatus);
		return 0;
	}

	// Enumerate service flow
	ret = GAPI_BeginSFRead(&ID);
	if (GCT_API_RET_SUCCESS != ret) {
		cm_printf("Failed to GAPI_BeginSFRead=%d\n",ret);
	}

	pServiceFlow = NULL;
	pCrRule = NULL;
	pPhsRule = NULL;
	count = 0;
	while ( GCT_API_RET_SUCCESS == (ret = GAPI_GetNextSF(&ID, pServiceFlow, 2, &pServiceFlow)) && pServiceFlow )
	{
		cm_printf("SFID=0x%08x\n", ntohl(pServiceFlow->param.SFID));

		while ( GCT_API_RET_SUCCESS == (ret = GAPI_GetNextClfrRule(&ID, pServiceFlow, pCrRule, &pCrRule)) && pCrRule )
		{
			cm_printf("\tClfrIndex=%d\n", ntohs(pCrRule->PacketClassifierRuleIndex));
		}

		while ( GCT_API_RET_SUCCESS == (ret = GAPI_GetNextPHSRule(&ID, pServiceFlow, pPhsRule, &pPhsRule)) && pPhsRule )
		{
			cm_printf("\tPHSI=%d\n", pPhsRule->PHSI);
		}

		count++;
	}

	if (!count) {
		cm_printf("Service Flow does not exist.\n");
	}
	else {
		cm_printf("Service Flow count=[%d]\n", count);
	}

	ret = GAPI_EndSFRead(&ID);
	if (GCT_API_RET_SUCCESS != ret) {
		cm_printf("Failed to GAPI_EndSFRead=%d\n",ret);
	}

	return ID.deviceIndex;
}

static int cmd_dsa_test(int argc, char *argv[])
{
	GDEV_ID ID;
	WIMAX_API_DEVICE_STATUS DeviceStatus;
	WIMAX_API_CONNECTION_PROGRESS_INFO ConnectionProgressInfo;
	WIMAX_SF_PARAM lSfParam;
	WIMAX_CLFR_RULE lCrRule;
	WIMAX_PHS_RULE lPhsRule;
	int ret;

	ID.apiHandle = cm_api_handle;
	ret = ID.deviceIndex = (argc == 2 ? atoi(argv[1]) : DEFAULT_DEVICE);

	if (GAPI_GetDeviceStatus(&ID, &DeviceStatus, &ConnectionProgressInfo)
		!= GCT_API_RET_SUCCESS) {
		cm_eprintf("GAPI_GetDeviceStatus Failure\n");
		return -1;
	}

	if (DeviceStatus != WIMAX_API_DEVICE_STATUS_Data_Connected) {
		cm_eprintf("DeviceStatus is not connected state(%d)\n", DeviceStatus);
		return -1;
	}

	// DSA Test
	unsigned char phsmdat[3] = {0x00,0x0f,0xf0};
	unsigned char phsf[20] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0xc0, 0xa8, 0x01, 0x59, 0xc0, 0xa8, 0x01, 0xfd};

	lSfParam = sf_param_init;
	lCrRule = classifier_rule_init;
	lPhsRule = phs_rule_init;

	lSfParam.DL = 0; /* Uplink service flow */
	lSfParam.TrafficPriority = 1; 
	lSfParam.QosParamSetType = 7;
	lSfParam.MaxSustainedTrafficRate = 88000;
	lSfParam.MinReservedTrafficRate = 88000;
	lSfParam.ULGrantSchedulingType = UL_SCHED_TYPE_ertPS;
	lSfParam.RequestTransmissionPolicy = 0x02;
	lSfParam.ToleratedJitter = 20;
	lSfParam.MaxLatency = 60;
	lSfParam.TypeOfDataDeliveryServices = DATA_SERVICE_UGS;
	lSfParam.UnsolicitedGrantInterval = 20;

	lSfParam.CSSpecification = 1;
	lSfParam.ARQEnable = 0;
	lSfParam.HARQServiceFlows = 1;
	lSfParam.PDUSNExtendedSubheaderForHARQReordering = 2;
	lSfParam.FSNSize = 1;

	/* Classifier rule: UDP packets with destination port 5001 */
	lCrRule.ClassifierRulePriority = 1;
	lCrRule.Protocol = 17;
	lCrRule.ProtocolDestPort.low = 0;
	lCrRule.ProtocolDestPort.high = 65535;
	lCrRule.IPv4MaskedDestAddress.address.s_addr = inet_addr("192.168.1.253");
	lCrRule.IPv4MaskedDestAddress.mask.s_addr = inet_addr("255.255.255.255");

	lPhsRule.PHSI = 1;
	lPhsRule.PHSS = 20;

	memcpy(lPhsRule.PHSM, phsmdat, 3);
	memcpy(lPhsRule.PHSF, phsf, 20);

	lPhsRule.PHSV = 1;

	ret = GAPI_CmdAddSF(&ID, &lSfParam, &lCrRule, &lPhsRule); 
	if (GCT_API_RET_SUCCESS != ret) {
		cm_printf("Failed to GAPI_CmdAddSF=%d\n",ret);
	}
	else {
		cm_printf("Success! GAPI_CmdAddSF, SFID=0x%08x\n", ntohl(lSfParam.SFID));
	}

	return ID.deviceIndex;
}

static int cmd_dsc_test(int argc, char *argv[])
{
	GDEV_ID ID;
	WIMAX_API_DEVICE_STATUS DeviceStatus;
	WIMAX_API_CONNECTION_PROGRESS_INFO ConnectionProgressInfo;
	WIMAX_SERVICE_FLOW *sfp;
	uint32_t sfid;
	int ret;

	if (argc < 2) {
		cm_eprintf("Invalid parameter\n");
		return 0;
	}

	sfid = strtoul(argv[1], NULL, 16);
	sfid = htonl(sfid);

	ID.apiHandle = cm_api_handle;
	ret = ID.deviceIndex = (argc == 3 ? atoi(argv[2]) : DEFAULT_DEVICE);

	if (GAPI_GetDeviceStatus(&ID, &DeviceStatus, &ConnectionProgressInfo)
		!= GCT_API_RET_SUCCESS) {
		cm_eprintf("GAPI_GetDeviceStatus Failure\n");
		return -1;
	}

	if (DeviceStatus != WIMAX_API_DEVICE_STATUS_Data_Connected) {
		cm_eprintf("DeviceStatus is not connected state(%d)\n", DeviceStatus);
		return -1;
	}

	// DSC Test
	ret = GAPI_GetServiceFlow(&ID, sfid, &sfp);
	if (GCT_API_RET_SUCCESS != ret) {
		cm_printf("Failed to GAPI_GetServiceFlow=%d\n",ret);
	}

	if (sfp) {
		sfp->param.MaxSustainedTrafficRate = 100000;
		sfp->param.MinReservedTrafficRate = 100000;
	
		sfp->classification_rule[0].Protocol = 1; /* ICMP */
		
		ret = GAPI_CmdChangeSF(&ID, &sfp->param,
			DSC_ADD_CLASSIFIER, &sfp->classification_rule[0],
			DSC_NOP_PHS, NULL);
	
		if (GCT_API_RET_SUCCESS != ret) {
			cm_printf("Failed to GAPI_CmdChangeSF=%d\n",ret);
		}
		else {
			cm_printf("Success! GAPI_CmdChangeSF\n");
		}			
	} else {
		cm_printf("Not found Service Flow\n");
	}
	
	return ID.deviceIndex;
}

static int cmd_dsd_test(int argc, char *argv[])
{
	GDEV_ID ID;
	WIMAX_API_DEVICE_STATUS DeviceStatus;
	WIMAX_API_CONNECTION_PROGRESS_INFO ConnectionProgressInfo;
	WIMAX_SERVICE_FLOW *sfp;
	uint32_t sfid;
	int ret;

	if (argc < 2) {
		cm_eprintf("Invalid parameter\n");
		return 0;
	}

	sfid = strtoul(argv[1], NULL, 16);
	sfid = htonl(sfid);

	ID.apiHandle = cm_api_handle;
	ret = ID.deviceIndex = (argc == 3 ? atoi(argv[2]) : DEFAULT_DEVICE);

	if (GAPI_GetDeviceStatus(&ID, &DeviceStatus, &ConnectionProgressInfo)
		!= GCT_API_RET_SUCCESS) {
		cm_eprintf("GAPI_GetDeviceStatus Failure\n");
		return -1;
	}

	if (DeviceStatus != WIMAX_API_DEVICE_STATUS_Data_Connected) {
		cm_eprintf("DeviceStatus is not connected state(%d)\n", DeviceStatus);
		return -1;
	}

	// DSD Test
	ret = GAPI_GetServiceFlow(&ID, sfid, &sfp);
	if (GCT_API_RET_SUCCESS != ret) {
		cm_printf("Failed to GAPI_GetServiceFlow=%d\n",ret);
	}
	
	if (sfp) {
		ret = GAPI_CmdDeleteSF(&ID, &sfp->param);
		if (GCT_API_RET_SUCCESS != ret) {
			cm_printf("Failed to GAPI_CmdDeleteSF=%d\n",ret);
		}
		else {
			cm_printf("Success! GAPI_CmdDeleteSF\n");
		}			
	} else {
		cm_printf("Not found service flow\n");
	}

	return ID.deviceIndex;
}
#endif // CONFIG_ENABLE_SERVICE_FLOW

static int cmd_command_mac_state(int argc, char *argv[])
{
	GDEV_ID ID;
	int type;
	int ret;	

	if (argc < 2) {
		cm_printf("Invalid parameter.\n");
		return -1;
	}

	type = atoi(argv[1]);
	ID.apiHandle = cm_api_handle;
	ret = ID.deviceIndex = (argc == 3 ? atoi(argv[2]) : DEFAULT_DEVICE);

	if(GAPI_CmdMACState(&ID, type)
		!= GCT_API_RET_SUCCESS) {
		cm_printf("Failed to GAPI_CmdMACState=%d\n",ret);
	}

	return ID.deviceIndex;
}

static int cmd_set_idle_mode_timeout(int argc, char *argv[])
{
	GDEV_ID ID;
	int timeoutSec;
	int ret;	

	if (argc < 2) {
		cm_printf("Invalid parameter.\n");
		return -1;
	}

	timeoutSec = atoi(argv[1]);
	ID.apiHandle = cm_api_handle;
	ret = ID.deviceIndex = (argc == 3 ? atoi(argv[2]) : DEFAULT_DEVICE);

	if(GAPI_SetIdleModeTimeout(&ID, timeoutSec)
		!= GCT_API_RET_SUCCESS) {
		cm_printf("Failed to GAPI_SetIdleModeTimeout=%d\n",ret);
	}

	return ID.deviceIndex;

}

static int cmd_get_phy_mac_basic(int argc, char *argv[])
{
	GDEV_ID ID;
	GCT_API_MAC_PHY_MAC_BASIC phy_mac_basic;
	int ret;

	ID.apiHandle = cm_api_handle;
	ret = ID.deviceIndex = (argc == 2 ? atoi(argv[1]) : DEFAULT_DEVICE);

	if(GAPI_GetPHY_MAC_Basic(&ID, &phy_mac_basic)
		== GCT_API_RET_SUCCESS) {

		cm_printf("[PHY/MAC Basic]\n");
		cm_printf("FrameNumber: %u\n", phy_mac_basic.frame_number);
		cm_printf("FCH:         %u\n", phy_mac_basic.fch);
		cm_printf("TTG:         %u\n", phy_mac_basic.ttg);
		cm_printf("RTG:         %u\n", phy_mac_basic.rtg);
		cm_printf("NumDlSymbol: %u\n", phy_mac_basic.num_dl_symbol);
		cm_printf("NumUlSymbol: %u\n", phy_mac_basic.num_ul_symbol);
		cm_printf("CurrentPI:   %u\n", phy_mac_basic.current_pi);
		cm_printf("PerviousPI:  %u\n", phy_mac_basic.previous_pi);
		cm_printf("UlPermBase:  %u\n", phy_mac_basic.ul_perm_base);
		cm_printf("MAC State:   %u\n", phy_mac_basic.mac_state);
		cm_printf("BSID:        0x%02x%02x%02x%02x%02x%02x\n", 
			phy_mac_basic.bsid[0], phy_mac_basic.bsid[1], phy_mac_basic.bsid[2],
			phy_mac_basic.bsid[3], phy_mac_basic.bsid[4], phy_mac_basic.bsid[5]);
		cm_printf("ulTime:      %d\n", phy_mac_basic.ul_time);
		cm_printf("Frequency:   %u\n", phy_mac_basic.frequency);
		cm_printf("Bandwidth:   %u\n", phy_mac_basic.bandwidth);
		cm_printf("TimeActive:  %u\n", phy_mac_basic.time_active);
		cm_printf("TimeSleep:   %u\n", phy_mac_basic.time_sleep);
		cm_printf("TimeIdle:    %u\n", phy_mac_basic.time_idle);
		cm_printf("BasicCID:    %u\n", phy_mac_basic.basic_cid);
		cm_printf("PrimaryCID:  %u\n", phy_mac_basic.primary_cid);

	} else {
		cm_printf("Failed to GAPI_GetPHY_MAC_Basic=%d\n",ret);
	}

	return ID.deviceIndex;
}

static int cmd_get_phy_mcs(int argc, char *argv[])
{
	GDEV_ID ID;
	GCT_API_MAC_PHY_MCS phy_mcs;
	int i, j, k;
	int ret;

	ID.apiHandle = cm_api_handle;
	ret = ID.deviceIndex = (argc == 2 ? atoi(argv[1]) : DEFAULT_DEVICE);

	if(GAPI_GetPHY_MCS(&ID, &phy_mcs)
		== GCT_API_RET_SUCCESS) {

		cm_printf("[PHY MCS: DL]\n");
		cm_printf("[OFDMA_FEC] [REPETITION] [MIMO] [num_burst] [num_burst_err] [len_pdu] [num_pdu]\n");

		for (i = 0 ; i < OFDMA_FEC_MODE_CNT ; i++)
		{
			for (j = 0 ; j < REPETITION_CODING_CNT ; j++)
			{
				for (k = 0 ; k < MIMO_TYPE_CNT ; k++)
				{
					if (phy_mcs.dl_used[i][j][k])
					{
						cm_printf("%10d %12d %6d %11u %15u %9u %9u\n",
							i, j, k, 
							phy_mcs.dl[i][j][k].num_burst, phy_mcs.dl[i][j][k].num_burst_error, phy_mcs.dl[i][j][k].len_pdu, phy_mcs.dl[i][j][k].num_pdu);
					}
				}
			}
		}

		cm_printf("[PHY MCS: UL]\n");
		cm_printf("[OFDMA_FEC] [REPETITION] [MIMO] [num_burst] [num_burst_err] [len_pdu] [num_pdu]\n");

		for (i = 0 ; i < OFDMA_FEC_MODE_CNT ; i++)
		{
			for (j = 0 ; j < REPETITION_CODING_CNT ; j++)
			{
				for (k = 0 ; k < MIMO_TYPE_CNT ; k++)
				{
					if (phy_mcs.ul_used[i][j][k])
					{
						cm_printf("%10d %12d %6d %11u %15u %9u %9u\n",
							i, j, k, 
							phy_mcs.ul[i][j][k].num_burst, phy_mcs.ul[i][j][k].num_burst_error, phy_mcs.ul[i][j][k].len_pdu, phy_mcs.ul[i][j][k].num_pdu);
					}
				}
			}
		}
	
	} else {
		cm_printf("Failed to GAPI_GetPHY_MCS=%d\n",ret);
	}

	return ID.deviceIndex;
}

static int cmd_get_phy_cinr_rssi(int argc, char *argv[])
{
	GDEV_ID ID;
	GCT_API_MAC_PHY_CINR_RSSI phy_cinr_rssi;
	int ret;

	ID.apiHandle = cm_api_handle;
	ret = ID.deviceIndex = (argc == 2 ? atoi(argv[1]) : DEFAULT_DEVICE);

	if(GAPI_GetPHY_CINR_RSSI(&ID, &phy_cinr_rssi)
		== GCT_API_RET_SUCCESS) {

		cm_printf("[PHY CINR/RSSI]\n");
		cm_printf("CINR-mean:             %d\n", phy_cinr_rssi.cinr_mean);
		cm_printf("CINR-std-dev:          %d\n", phy_cinr_rssi.cinr_std_dev);
		cm_printf("RSSI-mean:             %d\n", phy_cinr_rssi.rssi_mean);
		cm_printf("RSSI-std-dev:          %d\n", phy_cinr_rssi.rssi_std_dev);
		cm_printf("CINR_A_mean:           %d\n", phy_cinr_rssi.cinr_a_mean);
		cm_printf("CINR_B_mean:           %d\n", phy_cinr_rssi.cinr_b_mean);
		cm_printf("CINR-main:             %d\n", phy_cinr_rssi.cinr_main);
		cm_printf("CINR-diversity:        %d\n", phy_cinr_rssi.cinr_diversity);
		cm_printf("RSSI-main:             %d\n", phy_cinr_rssi.rssi_main);
		cm_printf("RSSI-diversity:        %d\n", phy_cinr_rssi.rssi_diversity);
		cm_printf("Preamble CINR(reuse3): %d\n", phy_cinr_rssi.preamble_cinr_reuse3);
		cm_printf("Preamble CINR(reuse1): %d\n", phy_cinr_rssi.preamble_cinr_reuse1);
		
	} else {
		cm_printf("Failed to GAPI_GetPHY_CINR_RSSI=%d\n",ret);
	}

	return ID.deviceIndex;
}

static cmd_t cmd_list[] = {
	{"cm_gdl", "It shows device list", NULL, cmd_get_dev_list},
	{"cm_gs", "It shows the current WiMax state", "[device index: default 1]",
								cmd_get_status},
	{"cm_ru", "RF up.", "[device index: default 1]", cmd_rf_up},
	{"cm_rd", "RF down.", "[device index: default 1]", cmd_rf_down},
	{"cm_gl", "Get link status.", "[device index: default 1]", cmd_get_link_status},
	{"cm_gp", "Get profile list.", "[device index: default 1]", cmd_get_profile_list},
	{"cm_sp", "Set profile.", "profile-index [device index: default 1]", cmd_set_profile},
	{"cm_gnl", "It shows network list.", "[device index: default 1]", cmd_get_net_list},
	{"cm_gnbl", "It shows neighbor list.", "[device index: default 1]",
								cmd_get_neighbor_list},
	{"cm_cn", "It connects to network", "[profile-ID] [device index: default 1]",
								cmd_connect_net},
	{"cm_auto_cn", "Auto connection on/off", "on|off [device index: default 1]",
								cmd_auto_connect_net},
	{"cm_dn", "It disconnects from network", "[device index: default 1]",
								cmd_disconnect_net},
	{"cm_scit", "Set scan interval(second)", "interval [device index: default 1]",
								cmd_set_autoscan_interval},
	{"cm_scan", "It scan network", "scan-mode [device index: default 1]"
								"\n              scan-mode 0: wide scan"
								"\n              scan-mode 1: all subscriptions scan"
								"\n              scan-mode 2: current subscription scan",
								cmd_scan},
	{"cm_auth", "This command is not used no more.", NULL, cmd_auth},
	{"cm_dc", "Delete Cert.", "cert-index", cmd_delete_cert},
	{"cm_gcs", "Get Cert.'s status", NULL, cmd_get_cert_status},
	{"cm_gcm", "Get Cert.'s mask", NULL, cmd_get_cert_mask},
	{"cm_scm", "Set Cert.'s mask.", "cert-mask", cmd_set_cert_mask},
	{"cm_gci", "Get Cert.'s information.", NULL, cmd_get_cert_info},
	{"cm_sd", "Set debug level.", "level", cmd_debug_level},
	{"cm_rfile", "Read file.", "host-file target-file [device index: default 1]",
								cmd_read_file},
	{"cm_wfile", "Write file.", "host-file target-file [device index: default 1]",
								cmd_write_file},
	{"cm_dfile", "Delete file.", "target-file [device index: default 1]",
								cmd_delete_file},
	{"cm_rimg", "Read image.", "type image-path [device index: default 1]"
								"\n              type 7: EEPROM Bootloader (All bootloader block: 12KB)"
								"\n              type 0x100: oma-xml"
								"\n              type 0x101: device Cert(private-key should be merged)"
								"\n              type 0x102: server root CA"
								"\n              type 0x103: sub CA #1"
								"\n              type 0x104: sub CA #2"
								"\n              type 0x106: combined CA",
								cmd_read_image},
	{"cm_wimg", "Write image.", "type image-path [device index: default 1]"
								"\n              type 0: kernel"
								"\n              type 1: filesystem"
								"\n              type 7: EEPROM Bootloader"
								"\n              type 0x100: oma-xml"
								"\n              type 0x101: device Cert(private-key should be merged)"
								"\n              type 0x102: server root CA"
								"\n              type 0x103: sub CA #1"
								"\n              type 0x104: sub CA #2"
								"\n              type 0x106: combined CA",
								cmd_write_image},
	{"cm_cimg", "Control image.", "type command [device index: default 1]"
								"\n              type 0x101: device Cert(private-key should be merged)"
								"\n              type 0x102: server root CA"
								"\n              type 0x103: sub CA #1"
								"\n              type 0x104: sub CA #2"
								"\n              type 0x105: eap parameter"
								"\n              type 0x106: combined CA"
								"\n              command 0: delete image"
								"\n              command 1: get size of image"
								"\n              command 2: get crc32 of image",
								cmd_control_image},
	{"cm_nv", "Read/Write NV parameters.", nv_param_usage, cmd_nv_param},
	{"cm_scr", "Run script", "script-file [device index: default 1]", cmd_script},
	{"cm_date", "Print current time", NULL, cmd_date},
	{"cm_sleep", "Sleep function for script test", NULL, cmd_sleep},
	{"cm_chk", "Check cm status", NULL, cmd_chk},
	{"cm_test", "Test function", NULL, cmd_test},
	{"cm_gst", "Get statistics", NULL, cmd_get_statistics},
	{"cm_uicc", "UICC Test function", "on|off [device index: default 1]", cmd_uicc},
	{"cm_macst", "Command MAC State", "type [device index: default 1]"
								"\n              type 0: Enter Sleep Mode"
								"\n              type 1: Exit Sleep Mode"
								"\n              type 2: Enter Idle Mode"
								"\n              type 3: Exit Idle Mode",
								cmd_command_mac_state},
	{"cm_idletm", "Set Idle Mode Timeout", "timeout-sec [device index: default 1]", cmd_set_idle_mode_timeout},
	{"cm_gpmac", "Get PHY MAC Basic status", "[device index: default 1]", cmd_get_phy_mac_basic},
	{"cm_gpmcs", "Get PHY MCS status", "[device index: default 1]", cmd_get_phy_mcs},
	{"cm_gpcinr", "Get PHY CINR/RSSI", "[device index: default 1]", cmd_get_phy_cinr_rssi},	
	#if defined(CONFIG_DM_INTERFACE)
	{"cm_dmif", "DM interface on/off", "on/off", cmd_dmif},
	#endif
	#if defined(CONFIG_ENABLE_SERVICE_FLOW)
	{"cm_gsf", "Get current service flow list", "[device index: default 1]", cmd_gsf},
	{"cm_dsa_test", "Service Flow DSA Test function", "[device index: default 1]", cmd_dsa_test},
	{"cm_dsc_test", "Service Flow DSC Test function", "sfid [device index: default 1]", cmd_dsc_test},
	{"cm_dsd_test", "Service Flow DSD Test function", "sfid [device index: default 1]", cmd_dsd_test},
	#endif // CONFIG_ENABLE_SERVICE_FLOW
	{"shell", "Execute shell prompt", NULL, cmd_shell},
	{"cm_exit", "Exit CM.", NULL, cmd_exit},
	{"cm_help", "Print command list.", NULL, cmd_help},
	{NULL, NULL, NULL, NULL}
};

static void print_cmd(cmd_t *cmd)
{
	cm_printf("%s: %s\n", cmd->cmd, cmd->desc ? cmd->desc : "");
	cm_printf("    =>%s %s\n", cmd->cmd, cmd->param ? cmd->param : "");
}

static void print_cmd_usage(const char *cmd)
{
	cmd_t *list = cmd_list;

	while (list->cmd) {
		if (!strcmp(list->cmd, cmd)) {
			print_cmd(list);
			return;
		}
		list++;
	}
}

static void print_cmd_list(const char *cmd)
{
	cmd_t *list = cmd_list;

	if (!cmd)
		cm_printf("----------------------[ Command list ]----------------------\n");
	else
		cm_printf("------------------------------------------------------------\n");

	while (list->cmd) {
		if (!cmd || !strcmp(cmd, list->cmd)) {
			print_cmd(list);
			if (cmd)
				break;
		}
		list++;
	}
	cm_printf("------------------------------------------------------------\n");
}

static int do_cmd(char *cmd_line, int len)
{
	char buf[1024];
	cmd_t *list = cmd_list;
	int argc;
	char *argv[16];

	memcpy(buf, cmd_line, len);
	__get_token_arg(buf, &argc, argv);

	if (argc) {
		while (list->cmd) {
			if (!strcmp(list->cmd, argv[0])) {
				return list->func(argc, argv);
			}
			list++;
		}
	}

	return NO_CM_CMD;
}

static int is_foreground_process(void)
{
	if (tcgetpgrp(STDIN_FILENO) == getpgrp())
		return 1;
	else
		return 0;
}

void cmd_run(void)
{
	GDEV_ID ID;
	cm_common_conf_t *pconf = &cm_common_conf;
	char buf[1024];
	int readn, ret;
	GCT_API_RET gret = 0;

	ID.apiHandle = cm_api_handle;

	if (pconf->run_script_file[0] && !access(pconf->run_script_file, 0))
		run_script(DEFAULT_DEVICE, pconf->run_script_file);

	if (!isatty(STDIN_FILENO) || !is_foreground_process()) {
		cm_printf("Background!\n");
		while (1);
	}

	while (fgets(buf, sizeof(buf), stdin)) {
		readn = strlen(buf);
		ret = do_cmd(buf, readn);
		if (ret == EXIT_CM_CMD)
			break;
		if (cm_odev_cnt) {
			if (ret == NO_CM_CMD || ret > 0) {
				if (ret == NO_CM_CMD) {
					if (!(ret = get_first_odev()))
						continue;
				}
				else
					readn = 1;
				buf[readn-1] = '\n';
				ID.deviceIndex = ret;
				gret = send_print_string(&ID, buf, readn);
			}
		}
	}
}
