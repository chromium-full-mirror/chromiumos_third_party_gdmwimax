// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <zlib.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <stdarg.h>
#include <wchar.h>
#include <linux/version.h>
#include <signal.h>

#include "cm.h"
#include "exitcb.h"
#include "profilestring.h"
#include "dhclient.h"

extern void reg_indications(APIHAND *api_handle);
extern void kill_dhclient(int dev_idx);
extern int setup_device_conf(GDEV_ID *ID, const unsigned char *mac);
extern void load_device_conf(dev_conf_t *conf, const char *mac);
extern dev_conf_t default_dev_conf;

int cm_init(int read_only);
int cm_deinit(void);

APIHAND cm_api_handle;
char cm_odev_list[MAX_DEV];
dev_conf_t cm_dev_conf[MAX_DEV];
int cm_odev_cnt;
cm_common_conf_t cm_common_conf;


int get_first_odev(void)
{
	int i;

	if (cm_odev_cnt) {
		for (i = 0; i < MAX_DEV; i++) {
			if (cm_odev_list[i])
				return i;
		}
	}
	return 0;
}

void add_odev_list(int dev_idx)
{
	assert(!cm_odev_list[dev_idx]);
	cm_odev_list[dev_idx] = 1;
	cm_odev_cnt++;
}


int del_odev_list(int dev_idx)
{
	if (cm_odev_list[dev_idx]) {
		cm_odev_list[dev_idx] = 0;
		cm_odev_cnt--;
		return 1;
	}
	return 0;
}

int cm_get_filesize(const char *file)
{
	int fd, len;

	if ((fd = open(file, O_RDONLY)) < 0) {
		cm_printf("open(%s) fail. %s(%d)\n", file, strerror(errno), errno);
		return -1;
	}

	len = lseek(fd, 0, SEEK_END);
	close(fd);
	return len;
}

int cm_read_file(const char *file, char *buf, int buf_size)
{
	int fd, len, total = 0;

	if ((fd = open(file, O_RDONLY)) < 0) {
		cm_printf("open(%s) fail. %s(%d)\n", file, strerror(errno), errno);
		return -1;
	}
	while ((len = read(fd, &buf[total], buf_size-total)) > 0) {
		total += len;
		if (buf_size-total == 0)
			break;
	}

	if (len < 0)
		cm_printf("read(%s) fail. %s(%d)\n", file, strerror(errno), errno);

	close(fd);
	return total;
}

int cm_write_file(const char *file, char *buf, int buf_size, int flags)
{
	int fd, len = 0, total = 0;
	mode_t mode = 0;

	if (flags & O_CREAT)
		mode = 0644;

	if ((fd = open(file, flags, mode)) < 0) {
		cm_printf("open(%s) fail. %s(%d)\n", file, strerror(errno), errno);
		return -1;
	}

	if (buf_size) {
		while ((len = write(fd, &buf[total], buf_size-total)) > 0) {
			total += len;
			if (buf_size-total == 0)
				break;
		}
	}

	if (len < 0)
		cm_printf("write(%s) fail\n", file);

	close(fd);
	return total;
}

int cm_cmp_file(const char *file, const char *file2)
{
	int fd = -1, fd2 = -1;
	int ret = -1;
	int len, len2;
	char buf[4096], buf2[4096];
	
	if ((fd = open(file, O_RDONLY)) < 0) {
		cm_printf("open(%s) fail. %s(%d)\n", file, strerror(errno), errno);
		goto out;
	}
	if ((fd2 = open(file2, O_RDONLY)) < 0) {
		cm_printf("open(%s) fail. %s(%d)\n", file2, strerror(errno), errno);
		goto out;
	}

	do {
		if ((len = read(fd, buf, sizeof(buf))) < 0) {
			cm_printf("read(%d:%s) fail. %s(%d)\n", fd, file, strerror(errno), errno);
			break;
		}
		if ((len2 = read(fd2, buf2, sizeof(buf2))) < 0) {
			cm_printf("read(%d:%s) fail. %s(%d)\n", fd2, file2, strerror(errno), errno);
			break;
		}

		if (len != len2)
			break;
		if (len < 0 || len2 < 0)
			break;
			
		if (len == 0 && len2 == 0) {
			ret = 0;
			break;
		}

		if (memcmp(buf, buf2, len))
			break;
	} while (1);

out:
	if (fd > 0)
		close(fd);
	if (fd2 > 0)
		close(fd2);
	return ret;
}

int cm_get_dir_entry(const char *dir, int index, char *filepath, int size)
{
	DIR *dirp;
	struct dirent *dp;
	int idx = 0, ret = -1;
	char *file;

	dirp = opendir(dir);

	while ((dp = readdir(dirp)) != NULL) {
		file = dp->d_name;
		if (!strcmp(file, ".") || !strcmp(file, ".."))
			continue;

		if (idx++ == index) {
			strncpy(filepath, file, size);
			ret = 0;
			break;
		}
	}

	closedir(dirp);
	return ret;
}

int cm_load_file_lines(const char *file, int lines, ...)
{
	char line[1024];
	FILE *fp;
	va_list list;
	char *p;
	int i, len;

	if (!(fp = fopen(file, "rt"))) {
		cm_eprintf("fopen(%s) error\n", file);
		return -1;
	}

	va_start(list, lines);
	for (i = 0; i < lines; i++) {
		p = va_arg(list, char *);
		if (p) *p = 0;
		if (!fgets(line, sizeof(line), fp))
			break;
		len = strlen(line);
		if (len >= 2 && line[len-2] == '\r') line[len-2] = 0;
		if (len >= 1 && line[len-1] == '\n') line[len-1] = 0;
		//cm_printf("[%s]\n", line);
		if (p) strcpy(p, line);
	}
	va_end(list);

	fclose(fp);
	return i;
}

int cm_store_file_lines(const char *file, int lines, ...)
{
	FILE *fp;
	va_list list;
	char *p;
	int i;

	if (!(fp = fopen(file, "wt"))) {
		cm_eprintf("fopen(%s) error\n", file);
		return -1;
	}

	va_start(list, lines);
	for (i = 0; i < lines; i++) {
		p = va_arg(list, char *);
		if (fprintf(fp, "%s\n", p ? p : "") < 0) {
			cm_eprintf("fprintf(%s) error\n", file);
			break;
		}
		//cm_printf("[%s]\n", p);
	}
	va_end(list);

	fclose(fp);
	return i;
}

char *cm_get_dev_idx2name(int dev_idx)
{
	static char name_buf[MAX_DEV][8];
	char *name = name_buf[dev_idx];

	sprintf(name, GCT_WM_PREFIX "%d", dev_idx-1);
	return name;
}

int cm_get_dhcp_ip(int dev_idx, char *ip)
{
#define sizeof_sa_sin_port	2
#define p_inaddrr(x) (&ifr.x[sizeof_sa_sin_port])
	struct ifreq ifr;
	struct in_addr inaddr;
	int fd = 0;
	int ret = 0;

	fd = socket(PF_PACKET, SOCK_DGRAM, IPPROTO_IP);
	if (fd < 0) {
		cm_eprintf("socket\n");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, cm_get_dev_idx2name(dev_idx), IFNAMSIZ);
	if (ioctl(fd, SIOCGIFADDR, &ifr) < 0) {
		//cm_dprintf("ioctl SIOCGIFFLAGS(%d)\n", fd);
		ret = -1;
		goto out;
	}
	memcpy(&inaddr, p_inaddrr(ifr_addr.sa_data), sizeof(inaddr));
	if (ip)
		strcpy(ip, inet_ntoa(inaddr));
out:
	if (fd > 0)
		close(fd);
	return ret;
}

static void extract_fw_version(const char *fw_ver, unsigned *app_ver, unsigned *mac_ver)
{
	const char *fw_ver_fmt = "FW(%d.%d.%d.%d : %d.%d.%d.%d)";
	int v[8];

	sscanf(fw_ver, fw_ver_fmt, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]);

	*app_ver = ((char)v[0] << 24) | ((char)v[1] << 16) | ((char)v[2] << 8) | (char)v[3];
	*mac_ver = ((char)v[4] << 24) | ((char)v[5] << 16) | ((char)v[6] << 8) | (char)v[7];
}

static void extract_fw_w_version(const wchar_t *fw_ver, unsigned *app_ver, unsigned *mac_ver)
{
	char ch_fw_ver[128];
	
	wcstombs((char *)ch_fw_ver, fw_ver, wcslen(fw_ver)+1);

	extract_fw_version(ch_fw_ver, app_ver, mac_ver);
}

static void setup_fw_version(int dev_idx, wchar_t *fw_ver)
{
	dev_conf_t *conf = &cm_dev_conf[dev_idx];

	extract_fw_w_version(fw_ver, &conf->fw_app_version, &conf->fw_mac_version);

	#if 0
	cm_dprintf("fw_app_version=0x%08x\n", conf->fw_app_version);
	cm_dprintf("fw_mac_version=0x%08x\n", conf->fw_mac_version);
	#endif
}

static int send_print_log_on_eap(GDEV_ID_P pID)
{
	#define STR_LOG_ON_EAP "log on eap\n"
	
	char *buf = STR_LOG_ON_EAP;
	int len = strlen(STR_LOG_ON_EAP);
	char hci_buf[HCI_HEADER_SIZE + 32];
	hci_t *hci = (hci_t *) hci_buf;

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

int device_open(GDEV_ID_P pID)
{
	cm_common_conf_t *pconf = &cm_common_conf;
	WIMAX_API_DEVICE_INFO info;
	char bl_ver[64];

	if (GAPI_WiMaxDeviceOpen(pID) == GCT_API_RET_SUCCESS) {
		add_odev_list(pID->deviceIndex);
		GAPI_GetBootloaderVersion(pID, bl_ver, sizeof(bl_ver));
		cm_printf("BL version: %s\n", bl_ver);

		if (GAPI_GetDeviceInformation(pID, &info) != GCT_API_RET_SUCCESS)
			return -1;

		setup_fw_version(pID->deviceIndex, (wchar_t *)info.swVersion.version);

		cm_printf("HW version: %S\n", (wchar_t *)info.hwVersion.version);
		cm_printf("SW version: %S\n", (wchar_t *)info.swVersion.version);
		
		setup_device_conf(pID, info.macAddress);

		if (pconf->eap_log_enable && CM_USE_EEAP && CAP_EEAP_ENABLED(pID->deviceIndex))
			send_print_log_on_eap(pID);

		if (pconf->api_mode != GCT_WIMAX_API_PRIVILEGE_READ_ONLY) {
			if (pconf->auto_connect_enable)
				cm_request_auto_connection(pID->deviceIndex);
			#if defined(CONFIG_DM_INTERFACE)
			else
				cm_profiling_rf_up(pID->deviceIndex);
			#endif
		}
		return 0;
	}
	else {
		cm_printf("open fail device[%d]\n", pID->deviceIndex);
		return -1;
	}
}


int device_close(GDEV_ID_P pID)
{
	cm_common_conf_t *pconf = &cm_common_conf;
	
	if (del_odev_list(pID->deviceIndex)) {
		if (pconf->api_mode != GCT_WIMAX_API_PRIVILEGE_READ_ONLY)
			dh_stop_dhclient(pID->deviceIndex);

		if (GAPI_WiMaxDeviceClose(pID) == GCT_API_RET_SUCCESS)
			return 0;
		else {
			cm_printf("close fail device[%d]\n", pID->deviceIndex);
			return -1;
		}
	}
	cm_printf("Not found device[%d]\n", pID->deviceIndex);
	return -1;
}


int open_dev_list(void)
{
	WIMAX_API_HW_DEVICE_ID list[256];
	UINT32 cnt = 256;
	GDEV_ID ID;
	int i;

	ID.apiHandle = cm_api_handle;

	if (GAPI_GetListDevice(cm_api_handle, list, &cnt) == GCT_API_RET_SUCCESS) {
		for (i = 0; i < cnt; i++) {
			ID.deviceIndex = list[i].deviceIndex;
			if (device_open(&ID) < 0)
				return -1;
		}
		return 0;
	}
	else
		return -1;
}


void close_dev_list(void)
{
	GDEV_ID ID;
	int i;

	ID.apiHandle = cm_api_handle;

	for (i = 0; i < sizeof(cm_odev_list)/sizeof(cm_odev_list[0]); i++) {
		if (cm_odev_list[i]) {
			ID.deviceIndex = i;
			device_close(&ID);
		}
			
	}
}

static void cb_cm_deinit(int signal/*, void *info*/)
{
	cm_deinit();
}

#define KEY_log_path							"log_path"
#define KEY_log_level							"log_level"
#define KEY_eap_log_enable						"eap_log_enable"
#define KEY_embedded_eap_enable					"embedded_eap_enable"
#define KEY_oma_dm_enable						"oma_dm_enable"
#define KEY_nonvolatile_dir						"nonvolatile_dir"
#define KEY_run_script_file						"run_script_file"
#define KEY_auto_connect_enable					"auto_connect_enable"
#define KEY_auto_connect_retry_count			"auto_connect_retry_count"
#define KEY_auto_select_profile_index			"auto_select_profile_index"
#define KEY_unknown_net_auto_connect_enable		"unknown_net_auto_connect_enable"
#define KEY_ip_allocation_timeout_sec			"ip_allocation_timeout_sec"
#define KEY_disconnct_on_ip_failure				"disconnct_on_ip_failure"

static void cm_load_common(cm_common_conf_t *pconf)
{
	char *section = "common";
	char str[256], *p;
	int len;

	p = pconf->log_path;
	len = sizeof(pconf->log_path);
	get_profile_string(section, KEY_log_path, "./sdklog", p, len, CONF_FILE);

	get_profile_string(section, KEY_eap_log_enable, "n", str, sizeof(str), CONF_FILE);
	if (!strcasecmp(str, "y"))
		pconf->eap_log_enable = 1;
	else
		pconf->eap_log_enable = 0;

	get_profile_string(section, KEY_embedded_eap_enable, "n", str, sizeof(str), CONF_FILE);
	if (!strcasecmp(str, "y"))
		pconf->embedded_eap_enable = 1;
	else
		pconf->embedded_eap_enable = 0;

	pconf->oma_dm_enable = 0;
	
	p = pconf->nonvolatile_dir;
	len = sizeof(pconf->nonvolatile_dir);
	get_profile_string(section, KEY_nonvolatile_dir, "./", p, len, CONF_FILE);
	len = strlen(pconf->nonvolatile_dir);
	if (pconf->nonvolatile_dir[len-1] == '/' || pconf->nonvolatile_dir[len-1] == '\\')
		pconf->nonvolatile_dir[len-1] = 0;
	
	p = pconf->run_script_file;
	len = sizeof(pconf->run_script_file);
	get_profile_string(section, KEY_run_script_file, "", p, len, CONF_FILE);
	len = strlen(pconf->run_script_file);
	if (pconf->run_script_file[len-1] == '/' || pconf->run_script_file[len-1] == '\\')
		pconf->run_script_file[len-1] = 0;
	
	get_profile_string(section, KEY_log_level, "1", str, sizeof(str), CONF_FILE);
	pconf->log_level = strtoul(str, NULL, 0);
	
	get_profile_string(section, KEY_auto_connect_enable, "n", str, sizeof(str), CONF_FILE);
	if (!strcasecmp(str, "y"))
		pconf->auto_connect_enable = 1;
	else
		pconf->auto_connect_enable = 0;
	
	get_profile_string(section, KEY_auto_connect_retry_count, "0", str, sizeof(str), CONF_FILE);
	pconf->auto_connect_retry_count = strtoul(str, NULL, 0);
	
	get_profile_string(section, KEY_auto_select_profile_index, "0", str, sizeof(str), CONF_FILE);
	pconf->auto_select_profile_index = strtoul(str, NULL, 0);
	
	get_profile_string(section, KEY_unknown_net_auto_connect_enable, "n", str, sizeof(str), CONF_FILE);
	if (!strcasecmp(str, "y"))
		pconf->unknown_net_auto_connect_enable = 1;
	else
		pconf->unknown_net_auto_connect_enable = 0;

	get_profile_string(section, KEY_ip_allocation_timeout_sec, "30", str, sizeof(str), CONF_FILE);
	pconf->ip_allocation_timeout_sec = strtoul(str, NULL, 0);

	get_profile_string(section, KEY_disconnct_on_ip_failure, "n", str, sizeof(str), CONF_FILE);
	if (!strcasecmp(str, "y"))
		pconf->disconnct_on_ip_failure = 1;
	else
		pconf->disconnct_on_ip_failure = 0;
}

static void *cm_auto_connect_thread(void *data)
{
	cm_common_conf_t *pconf = &cm_common_conf;
	cm_msg_cb_t *msg_cb;
	int dev_idx;

	msg_cb = &pconf->auto_conn_msg;
	cm_msg_init(msg_cb);

	cm_printf("Auto connection is ready!\n");
	while (1) {
		dev_idx = DEFAULT_DEVICE;
		if (cm_msg_recv(msg_cb, &dev_idx, NULL) < 0) {
			cm_eprintf("auto connector cm_msg_recv error\n");
			break;
		}
		if (pconf->auto_connect_enable)
			cm_auto_connect(dev_idx);
	}
	
	cm_msg_deinit(msg_cb);
	pconf->auto_conn_thr = (pthread_t) NULL;
	cm_printf("Auto connection finished!\n");
	return NULL;
}

void cm_request_auto_connection(int dev_idx)
{
	cm_common_conf_t *pconf = &cm_common_conf;
	dev_conf_t *conf = &cm_dev_conf[dev_idx];

	if (pconf->api_mode == GCT_WIMAX_API_PRIVILEGE_READ_ONLY)
		return;

	conf->auto_conn_retry_cnt = 0;
	cm_printf("Request auto connection device(%d)\n", dev_idx);
	if (dev_idx > 0)
		cm_msg_send(&pconf->auto_conn_msg, dev_idx, NULL);
}

void cm_retry_auto_connection(int dev_idx)
{
	cm_common_conf_t *pconf = &cm_common_conf;
	dev_conf_t *conf = &cm_dev_conf[dev_idx];

	if (pconf->api_mode == GCT_WIMAX_API_PRIVILEGE_READ_ONLY)
		return;

	if (++conf->auto_conn_retry_cnt == pconf->auto_connect_retry_count) {
		cm_printf("Too many retries(%u), auto_connect_retry_count in cm.conf is %u\n",
			conf->auto_conn_retry_cnt, pconf->auto_connect_retry_count);
	}
	else if (conf->auto_conn_retry_cnt < pconf->auto_connect_retry_count) {
		cm_printf("Re-request(%u/%u) auto connection device(%d)\n",
			conf->auto_conn_retry_cnt, pconf->auto_connect_retry_count, dev_idx);
		if (dev_idx > 0)
			cm_msg_send(&pconf->auto_conn_msg, dev_idx, NULL);
	}
}

static void cm_create_auto_connector(void)
{
	cm_common_conf_t *pconf = &cm_common_conf;

	if (!pconf->auto_connect_enable)
		cm_printf("Auto connection flag was disabled!\n");
	if (pconf->auto_conn_thr) {
		cm_eprintf("Auto connector has been started already!\n");
		return;
	}
	pthread_create(&pconf->auto_conn_thr, NULL, cm_auto_connect_thread, NULL);
}

static void cm_delete_auto_connector(void)
{
	cm_common_conf_t *pconf = &cm_common_conf;
	pthread_t thread;

	if ((thread = pconf->auto_conn_thr)) {
		pconf->auto_conn_thr = (pthread_t) NULL;
		pthread_cancel(thread);
		pthread_join(thread, NULL);
		cm_printf("Auto connector deleted!\n");
	}
	pconf->auto_connect_enable = 0;
}

int cm_init(int read_only)
{
	cm_common_conf_t *pconf = &cm_common_conf;
	GCT_WIMAX_SDK_MODE sdk_mode = 0;
	GCT_WIMAX_API_PARAM param;
	GCT_API_RET ret;

	if (read_only) {
		pconf->api_mode = GCT_WIMAX_API_PRIVILEGE_READ_ONLY;
		pconf->eap_log_enable = 0;
	}
	else
		pconf->api_mode = GCT_WIMAX_API_OPEN_MODE_NORMAL;

	cm_timer_module_init();

	cm_load_common(pconf);

	if (!read_only) {
		cm_create_auto_connector();
		dh_create_dhclient();
	}

	load_device_conf(&default_dev_conf, NULL);

	#if defined(CONFIG_DM_INTERFACE)
	if (!read_only) {
		if (dmif_init() < 0)
			return -1;
		pconf->dm_interface_enable = 1;
	}
	#endif

	if (mkdir(pconf->log_path, 0644) < 0 && errno != EEXIST) {
		cm_eprintf("Access directory(%s) failed %s(%d)\n",
			pconf->log_path, strerror(errno), errno);
		return -1;
	}

	if (pconf->embedded_eap_enable)
		sdk_mode |= GCT_WIMAX_SDK_EMBEDDED_EAP_ENABLED;
	if (pconf->oma_dm_enable)
		sdk_mode |= GCT_WIMAX_SDK_OMA_DM_ENABLED;

	strcpy(param.nonvolatile_dir, pconf->nonvolatile_dir);
	strcpy(param.log_path, pconf->log_path);
	param.log_level = pconf->log_level;
	ret = GAPI_Initialize(sdk_mode, &param);
	if (ret != GCT_API_RET_SUCCESS) {
		cm_eprintf("GAPI_Initialize failed(%d)\n", ret);
		return -1;
	}

	register_exit_cb(cb_cm_deinit);

	ret = GAPI_WiMaxAPIOpen(&cm_api_handle, pconf->api_mode);
	if (ret != GCT_API_RET_SUCCESS) {
		cm_eprintf("GAPI_WiMaxAPIOpen failed(%d)\n", ret);
		return -1;
	}

	reg_indications(cm_api_handle);

	if (open_dev_list() < 0)
		return -1;
	return 0;
}

int cm_deinit(void)
{
	APIHAND apihand = cm_api_handle;
	GCT_API_RET ret;

	cm_delete_auto_connector();

	unregister_exit_cb(cb_cm_deinit);

	close_dev_list();

	dh_delete_dhclient();

	#if (CONFIG_LOG_FILE_BUF_SIZE > 0)
	GAPI_SetDebugLevel(apihand, GAPI_LOG_FLUSH_LEVEL, NULL);
	#endif

	cm_api_handle = NULL;
	GAPI_WiMaxAPIClose(apihand);

	ret = GAPI_DeInitialize();
	if (ret != GCT_API_RET_SUCCESS)
		return -1;

	cm_timer_module_deinit();
	return 0;
}
