// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "global.h"
#include "wm_ioctl.h"
#include "error.h"
#include "handle.h"
#include "sdk.h"
#include "wimax.h"
#include "nds.h"
#include "device.h"
#include "io.h"
#include "fload.h"
#include "hci.h"
#include "log.h"
#if defined(CONFIG_ENABLE_SERVICE_FLOW)
#include "sf.h"
extern void dev_update_service_flow(int dev_idx, struct hci *pkt);
#endif // CONFIG_ENABLE_SERVICE_FLOW
#include "eap.h"

#define LOG_FILE_PREFIX		"sdk"
#define LOG_FILE_EXT		"log"
#define LOG_FILE			"sdk.log"
#define LOCK_PATH			"/var/lock/gct/"
#define LOCK_FILE			LOCK_PATH "gct_sdk.lock"

sdk_mng_t sdk_mng;

extern void array_DB2H(u32 *arr, int n);
extern void array_B2H(u16 *arr, int n);

int sdk_on_ind_event(sdk_internal_t *sdk, int dev_idx, fsm_event_t event);
int sdk_on_app_event(sdk_internal_t *sdk, int dev_idx, fsm_event_t event);
static int sdk_set_ioctl_profile_id(int dev_idx, u32 profile_id);

const char *sdk_version(void)
{
	return SDK_VERSION;
}

static int sdk_check_lock_file(void)
{
	int fd;

	if (access(LOCK_PATH, F_OK))
		mkdir(LOCK_PATH, 0755);
	if (access(LOCK_FILE, F_OK) < 0) {
		if ((fd = open(LOCK_FILE, O_CREAT|O_WRONLY|O_TRUNC, 0755)) < 0) {
			xprintf(SDK_STD_ERR, "Creating fail(%s)\n", LOCK_FILE);
			return -1;
		}
		close(fd);
		return 0;
	}
	xprintf(SDK_STD_ERR, "API is locked.(%s)\n", LOCK_FILE);
	return -1;
}

static int sdk_delete_lock_file(void)
{
	xprintf(SDK_DBG, "unlink %s\n", LOCK_FILE);
	return unlink(LOCK_FILE);
}

static void sdk_init_seed(void)
{
	struct timeval now;

	gettimeofday(&now, NULL);
	srand(now.tv_sec+now.tv_usec);
}

sdk_internal_t *sdk_get_rw_handle(void)
{
	struct list_head *head = hand_get_api_handle_list();
	api_hand_t *handle;
	sdk_internal_t *ret = NULL;

	pthread_mutex_lock(&api_handle_lock);
	list_for_each_entry(handle, head, list) {
		if (!handle->sdk->ro) {
			ret = handle->sdk;
			break;
		}
	}
	pthread_mutex_unlock(&api_handle_lock);
	return ret;
}

static void sdk_set_logpath(char *buf, const char *dir)
{
	time_t t;
	struct tm *tms;
	char time_buf[256];

	time(&t);
	tms = localtime(&t);

	sprintf(time_buf, "%04d%02d%02d-%02d.%02d.%02d", 
		tms->tm_year+1900, tms->tm_mon+1, tms->tm_mday,
		tms->tm_hour, tms->tm_min, tms->tm_sec);
	
	sprintf(buf, "%s/%s_%s.%s", dir, LOG_FILE_PREFIX, time_buf, LOG_FILE_EXT);
}

static void path_trim(char *path)
{
	int len;
	len = strlen(path) - 1/*move to last char*/;
	while (path[len] == '/' || path[len] == '\\')
		path[len--] = 0;
}

int sdk_init(GCT_WIMAX_SDK_MODE mode, GCT_WIMAX_API_PARAM *sdk_param)
{
	int log_level;
	char log_file[512];
	int ret;

	assert(sizeof(struct wm_req_s) <= sizeof(struct ifreq));
	assert(SIOC_DATA_END <= SIOC_DATA_MAX);

	if (sdk_param) {
		strcpy(sdk_mng.nonvolatile_dir, sdk_param->nonvolatile_dir);
		strcpy(sdk_mng.log_path, sdk_param->log_path);
		path_trim(sdk_mng.nonvolatile_dir);
		path_trim(sdk_mng.log_path);
		log_level = sdk_param->log_level;
	}
	else {
		strcpy(sdk_mng.nonvolatile_dir, "./");
		strcpy(sdk_mng.log_path, "./sdklog");
		log_level = 1;
	}

	if (mode & GCT_WIMAX_SDK_EMBEDDED_EAP_ENABLED)
		sdk_mng.eeap_enabled = TRUE;
	if (mode & GCT_WIMAX_SDK_OMA_DM_ENABLED)
		sdk_mng.odm_enabled = TRUE;

	sdk_init_seed();

	if ((ret = mkdir(sdk_mng.log_path, 0644)) < 0 && errno != EEXIST) {
		fprintf(stderr, "Make directory(%s) failed %s(%d)\n",
			sdk_mng.log_path, strerror(errno), errno);
	}

	sdk_set_logpath(log_file, sdk_mng.log_path);
	
	#if defined(SDK_TEST)
	unlink(LOG_FILE);
	strcpy(log_file, LOG_FILE);
	printf("####################################################\n");
	printf("############## NOTE: SDK_TEST Enabled ##############\n");
	printf("############## LOG FILE PATH: %s ##############\n", log_file);
	printf("####################################################\n");
	#endif

	timer_module_init();
	log_init(log_file, log_level);
	hand_init();

	ret = dm_init();
	
	hci_receiver_create(&sdk_mng.hci_recvr);

	return ret;
}

int sdk_deinit(void)
{
	int ret;

	hci_receiver_delete(&sdk_mng.hci_recvr);

	ret = dm_deinit();
	log_deinit();
	hand_deinit();
 	timer_module_deinit();
 
 	if (sdk_get_rw_handle())
 		sdk_delete_lock_file();

	return ret;
}

int sdk_check_handle(api_hand_t *api_hand, int dev_idx)
{
	api_hand_t *api = api_hand;
	sdk_internal_t *sdk = api->sdk;
	int ret = 0;

	if (api_hand == NULL) {
		xprintf(SDK_ERR, "API handle is NULL\n", api_hand);
		ret = sdk_set_errno(ERR_INVALID);
		goto out;
	}
	if (sdk->struct_size != sizeof(sdk_internal_t)) {
		xprintf(SDK_ERR, "Wrong structure size(%d!=%d)\n", sdk->struct_size, sizeof(api_hand_t));
		ret = sdk_set_errno(ERR_INVALID);
		goto out;
	}
	if (NO_DEV != dev_idx) {
		if (dev_idx < DEV_BASE_IDX || dev_idx >= MAX_DEVICE) {
			xprintf(SDK_ERR, "Device index %d is wrong\n", dev_idx);
			ret = sdk_set_errno(ERR_INVALID_DEV);
			goto out;
		}
		if (!sdk->dev_open[dev_idx]) {
			xprintf(SDK_ERR, "Device(%d) was not opened!\n", dev_idx);
			ret = sdk_set_errno(ERR_INVALID);
			goto out;
		}
		if (!dm_tst_dev(dev_idx)) {
			xprintf(SDK_ERR, "Device(%d) was not inserted!\n", dev_idx);
			ret = sdk_set_errno(ERR_INVALID_DEV);
			goto out;
		}
	}

out:
	return ret;
}

void *sdk_ind_thread(void *sdk_ptr)
{
	sdk_internal_t *sdk = (sdk_internal_t *) sdk_ptr;
	int prim;
	ind_msg_t *msg;
	dev_hand_t dev_hand;
	bool dev_opened;
	SDKIndRcvHCIPacket				recv_hci;
	SDKIndDeviceInsertRemove		insert_remove;
	SDKIndControlPowerManagement	pow_mng;
	SDKIndDeviceStatusUpdate		stat_update;
	SDKIndConnectToNetwork			connect_net;
	SDKIndDisconnectFromNetwork		disconnect_net;
	SDKIndNetworkSearchWideScan		net_search_wscan;
	SDKIndProvisioningOperation		provisioning;
	SDKIndPackageUpdate				package_update;
	SDKIndNotification				notification;
	SDKIndModeChange				power_mode;

	dev_hand.api = sdk->api;

	xfunc_in();

	while (1) {
		if (msg_recv(&sdk->ind.msg_cb, &prim, (void **)&msg) < 0) {
			xprintf(SDK_ERR, "msg_recv error\n");
			break;
		}
		if (msg == THREAD_EXIT_MSG) {
			xprintf(SDK_INFO, "%s thread exit...\n", __FUNCTION__);
			break;
		}
		dev_hand.dev_idx = msg->dev_idx;
		dev_opened = sdk->dev_open[msg->dev_idx] && dm_tst_dev(msg->dev_idx);

		xprintf(SDK_DBG, "recv ind msg: prim=%d, dev_opened=%d\n", prim, dev_opened);

		switch (prim) {
			case ind_recv_hci:
				recv_hci = sdk->ind.recv_hci;
				if (dev_opened && recv_hci)
					recv_hci(&dev_hand, msg->u.recv_hci.buf, msg->u.recv_hci.len);
				sdk_free(msg->u.recv_hci.buf);
				break;
			case ind_insert_remove:
				insert_remove =  sdk->ind.insert_remove;
				if (insert_remove)
					insert_remove(&dev_hand, msg->u.insert_remove.presence);
				break;
			case ind_pow_mng:
				pow_mng =  sdk->ind.pow_mng;
				if (dev_opened && pow_mng)
					pow_mng(&dev_hand, msg->u.pow_mng.state);
				break;
			case ind_stat_update:
				stat_update = sdk->ind.stat_update;
				if (dev_opened && stat_update)
					stat_update(&dev_hand, msg->u.stat_update.device_status,
						msg->u.stat_update.status_reason, msg->u.stat_update.progress_info);
				break;
			case ind_connect_net:
				connect_net =  sdk->ind.connect_net;
				if (dev_opened && connect_net)
					connect_net(&dev_hand, msg->u.connect_net.response);
				break;
			case ind_disconnect_net:
				disconnect_net =  sdk->ind.disconnect_net;
				if (dev_opened && disconnect_net)
					disconnect_net(&dev_hand, msg->u.disconnect_net.response);
				break;
			case ind_net_search_wscan:
				net_search_wscan =  sdk->ind.net_search_wscan;
				if (dev_opened && net_search_wscan)
					net_search_wscan(&dev_hand,
						msg->u.net_search_wscan.nsp_list, msg->u.net_search_wscan.list_cnt);
				break;
			case ind_provisioning:
				provisioning =  sdk->ind.provisioning;
				if (dev_opened && provisioning)
					provisioning(&dev_hand,
						msg->u.provisioning.operation, msg->u.provisioning.contact_type);
				break;
			case ind_package_update:
				package_update =  sdk->ind.package_update;
				if (dev_opened && package_update)
					package_update(&dev_hand, msg->u.package_update.update);
				break;
			case ind_notification:
				notification =  sdk->ind.notification;
				if (dev_opened && notification)
					notification(&dev_hand,
						msg->u.notification.category, msg->u.notification.type,
						msg->u.notification.buf_len, msg->u.notification.buf);
				break;
			case ind_mode_change:
				power_mode =  sdk->ind.power_mode;
				if (dev_opened && power_mode)
					power_mode(&dev_hand, msg->u.power_mode.power_mode);
				break;
			default:
				break;
		}

		sdk_free(msg);
	}

	msg_deinit(&sdk->ind.msg_cb);
	xprintf(SDK_INFO, "Exit sdk-ind-thread\n");

	xfunc_out();
	return NULL;
}

void sdk_init_ind(sdk_internal_t *sdk)
{
	msg_init(&sdk->ind.msg_cb);
	pthread_create(&sdk->ind.thread, NULL, sdk_ind_thread, sdk);
}

void sdk_deinit_ind(sdk_internal_t *sdk)
{
	pthread_t thread;

	xfunc_in("thread=0x%08X", (int) sdk->ind.thread);

	if ((thread = sdk->ind.thread)) {
		sdk->ind.thread = (pthread_t) NULL;
		msg_send(&sdk->ind.msg_cb, 0, THREAD_EXIT_MSG);
		pthread_join(thread, NULL);
	}

	xfunc_out();
}

api_hand_t *sdk_api_open(int mode)
{
	api_hand_t *api_hand = NULL;;
	sdk_internal_t *sdk;
	int ro = mode & sdk_read_only;

	xfunc_in("mode=0x%X", mode);

	if (!ro && (sdk_check_lock_file() < 0)) {
		sdk_set_errno(ERR_PERM);
		goto out;
	}

	api_hand = hand_alloc_api();
	if (api_hand) {
		sdk = api_hand->sdk = (sdk_internal_t *) sdk_malloc(sizeof(sdk_internal_t));
		assert(sdk != NULL);
		memset(sdk, 0, sizeof(sdk_internal_t));
		sdk->struct_size = sizeof(sdk_internal_t);
		sdk->api = api_hand;
		sdk->ro = ro;
		sdk->mode = mode;
		sdk_init_ind(sdk);

	}
out:
	xfunc_out("api_hand=0x%08X", api_hand);
	return api_hand;
}

int sdk_api_close(api_hand_t *api_hand)
{
	sdk_internal_t *sdk = api_hand->sdk;
	int ret, i;

	xfunc_in();

	if (sdk_check_handle(api_hand, NO_DEV) < 0)
		return -1;

	for (i = 1; i < MAX_DEVICE; i++) {
		if (sdk->dev_open[i]) {
			ret = sdk_device_close(api_hand, i);
			xprintf(SDK_DBG, "device(%d) has been closed with %d.\n", i, ret);
		}
	}

	if (!sdk->ro)
		sdk_delete_lock_file();

	sdk_deinit_ind(sdk);

	ret = hand_free_api(api_hand);
	sdk_free(sdk);
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_debug_level(api_hand_t *api_hand, int level)
{
	int ret;

	xfunc_in();

	if (sdk_check_handle(api_hand, NO_DEV) < 0)
		ret = -1;
	else
		ret = log_set_level(level);

	xfunc_out();
	return ret;
}

int sdk_print_log(api_hand_t *api_hand, int flag, const char *title, const char *str)
{
	int ret;

	if (sdk_check_handle(api_hand, NO_DEV) < 0)
		ret = -1;
	else
		ret = log_printf_string(SDK_API_LOG, flag, title, str);
	return ret;
}

int sdk_get_device_list(api_hand_t *api_hand, WIMAX_API_HW_DEVICE_ID *dev_list,
		u32 *list_cnt)
{
	dev_mng_t *dm = dm_get_dev_mng();
	int cnt = 0, i;

	xfunc_in("list_cnt=%d", *list_cnt);
	
	if (sdk_check_handle(api_hand, NO_DEV) < 0)
		return -1;

	pthread_mutex_lock(&dm->detect_lock);
	for (i = DEV_BASE_IDX; i < MAX_DEVICE; i++) {
		if (dm->devices[i] && dm->devices[i]->inserted) {
			if (cnt >= *list_cnt)
				break;
			dev_list[cnt].deviceIndex = i;
			mbstowcs((wchar_t *)dev_list[cnt].deviceName, dm->devices[i]->name,
				strlen(dm->devices[i]->name)+1);
			dev_list[cnt].deviceType = WIMAX_API_DEV_TYPE_WIMAX;
			cnt++;
		}
		if (cnt == dm->dev_cnt)
			break;
	}
	*list_cnt = cnt;
	pthread_mutex_unlock(&dm->detect_lock);

	xfunc_out("list_cnt=%d", *list_cnt);
	return 0;
}

int sdk_device_open(api_hand_t *api_hand, int dev_idx)
{
	sdk_internal_t *sdk = api_hand->sdk;
	device_t *dev;
	int ret;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, NO_DEV) < 0)
		return -1;
	if (dev_idx < DEV_BASE_IDX || dev_idx >= MAX_DEVICE)
		return sdk_set_errno(ERR_INVALID_DEV);

	ret = dm_open_device(dev_idx);

	if (!ret) {
		if (!(dev = dm_get_dev(dev_idx)))
			goto out;

		sdk->dev_open[dev_idx] = TRUE;

		if (dev->open_cnt == 1) {
			if (!sdk->ro) {
				ret = wm_set_capability(dev_idx, DEFAULT_CAPABILITY);
				if (ret < 0)
					goto put;
			}

			ret = wm_init_device_info(dev_idx);
			if (ret < 0)
				goto put;

			if (!sdk->ro) {
#if defined(CONFIG_ENABLE_SERVICE_FLOW)
				sf_init(dev_idx);
#endif // CONFIG_ENABLE_SERVICE_FLOW
				ret = wm_set_eap(dev_idx, FALSE);
				if (ret < 0)
					goto put;

				wm_init_scan(dev_idx);

				if (!api_hand->sdk->ro)
					dm_set_status(dev_idx, M_INIT, C_INIT);

				ret = wm_set_run_mode(dev_idx, sdk->mode);
				if (ret < 0)
					goto put;
			}
		}

		ret = sdk_on_app_event(sdk, dev_idx, AM_Open);
put:
		if (ret < 0)
			sdk->dev_open[dev_idx] = FALSE;
		dm_put_dev(dev_idx);
	}
out:
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_internal_device_close(struct sdk_internal_s *sdk, int dev_idx)
{
	int ret = 1;

	xfunc_in("dev=%d", dev_idx);

	if (sdk->dev_open[dev_idx]) {
		if (!sdk->ro) {
			wm_deinit_scan(dev_idx);
#if defined(CONFIG_ENABLE_SERVICE_FLOW)
			sf_deinit(dev_idx);
#endif // CONFIG_ENABLE_SERVICE_FLOW
		}

		sdk_on_app_event(sdk, dev_idx, AM_Close);
		sdk->dev_open[dev_idx] = FALSE;

		ret = dm_close_device(dev_idx);
	}

	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_device_close(api_hand_t *api_hand, int dev_idx)
{
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);


	if (api_hand && api_hand->sdk && !api_hand->sdk->dev_open[dev_idx]) {
		xprintf(SDK_DBG, "device(%d) has been closed or was not opened!\n");
		goto out;
	}

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	ret = sdk_internal_device_close(api_hand->sdk, dev_idx);
out:
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_get_status(sdk_internal_t *sdk, int dev_idx, int *m_status, int *c_status)
{
	device_t *dev;
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	if (sdk && sdk->ro)
		ret = dm_get_status(dev_idx, m_status, c_status);
	else {
		*m_status = dev->fsm.m_status;
		*c_status = dev->fsm.c_status;
	}

	dm_put_dev(dev_idx);
	xfunc_out("m_status=%d, c_status=%d", *m_status, *c_status);
	return ret;
}

int sdk_set_status(sdk_internal_t *sdk, int dev_idx, int m_status, int c_status)
{
	device_t *dev;
	int ret = 0;
	fsm_t fsm;

	xfunc_in("dev=%d, m_status=%d, c_status=%d", dev_idx, m_status, c_status);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	if (!sdk || !sdk->ro) {
		fsm = dev->fsm;
		dev->fsm.m_status = m_status;
		dev->fsm.c_status = c_status;

		ret = dm_set_status(dev_idx, m_status, c_status);

		if (!ret) {
			if (m_status == M_CONNECTED)
				net_updown(dev_idx, TRUE);
			else if (fsm.m_status == M_CONNECTED)
				net_updown(dev_idx, FALSE);
		}
	}

	dm_put_dev(dev_idx);
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_convert_status(int dev_idx, WIMAX_API_DEVICE_STATUS_P status,
						WIMAX_API_CONNECTION_PROGRESS_INFO_P connect_prog_info,
						int m_status, int c_status)
{
	/*
		return value
		0:	ok. Status-Indication is called.
		1:	ok. Status-Indication is NOT called.
		-1:	error.
	*/
	int ret = 0;

	if ((u32)m_status >= M_FSM_END || (u32)c_status >= C_FSM_END) {
		xprintf(SDK_ERR, "Unknown status: m_s=%d, c_s=%d\n", m_status, c_status);
		return sdk_set_errno(ERR_UNKNOWN_STAT);
	}

	switch(m_status) {
		case M_INIT:
			*status = WIMAX_API_DEVICE_STATUS_UnInitialized;
			break;
		case M_OPEN_OFF:
			*status = WIMAX_API_DEVICE_STATUS_RF_OFF_HW_SW;
			break;
		case M_OPEN_ON:
			*status = WIMAX_API_DEVICE_STATUS_Ready;
			break;
		case M_SCAN:
			*status = WIMAX_API_DEVICE_STATUS_Scanning;
			break;
		case M_CONNECTING:
			*status = WIMAX_API_DEVICE_STATUS_Connecting;
			switch(c_status) {
				case C_INIT:
					*connect_prog_info = WIMAX_API_DEVICE_CONNECTION_PROGRESS_Ranging;
					ret = 1;
					break;
				case C_CONNSTART:
					*connect_prog_info = WIMAX_API_DEVICE_CONNECTION_PROGRESS_Ranging;
					break;
				case C_ASSOCSTART:
					*connect_prog_info = WIMAX_API_DEVICE_CONNECTION_PROGRESS_Ranging;
					ret = 1;
					break;
				case C_RNG:
					*connect_prog_info = WIMAX_API_DEVICE_CONNECTION_PROGRESS_Ranging;
					ret = 1;
					break;
				case C_SBC:
					*connect_prog_info = WIMAX_API_DEVICE_CONNECTION_PROGRESS_SBC;
					break;
				case C_AUTH:
					*connect_prog_info = WIMAX_API_DEVICE_CONNECTION_PROGRESS_EAP_authentication_User;
					break;
				case C_REG:
					*connect_prog_info = WIMAX_API_DEVICE_CONNECTION_PROGRESS_Registration;
					break;
				case C_DSX:
					*connect_prog_info = WIMAX_API_DEVICE_CONNECTION_PROGRESS_Registration_DSX;
					break;
				case C_ASSOCCOMPLETE:
					*connect_prog_info = WIMAX_API_DEVICE_CONNECTION_PROGRESS_Registered;
					ret = 1;
					break;
				case C_CONNCOMPLETE:
					*connect_prog_info = WIMAX_API_DEVICE_CONNECTION_PROGRESS_Registered;
					break;
				default:
					ret = -1;
					break;
			}
			break;
		case M_CONNECTED:
			*status = WIMAX_API_DEVICE_STATUS_Data_Connected;
			break;
		default:
			ret = -1;
			break;
	}
	return ret;
}

int sdk_get_device_status(api_hand_t *api_hand, int dev_idx,
		WIMAX_API_DEVICE_STATUS_P status,
		WIMAX_API_CONNECTION_PROGRESS_INFO_P connect_prog_info)
{
	int m_status, c_status;
	int ret;

	xfunc_in();
	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	ret = sdk_get_status(api_hand->sdk, dev_idx, &m_status, &c_status);
	if (!ret) {
		ret = sdk_convert_status(dev_idx, status, connect_prog_info, m_status, c_status);
		if (ret >= 0)
			ret = 0;
	}

	xfunc_out("ret=%d, m_s=%d, c_s=%d", ret, m_status, c_status);
	return ret;
}

static void sdk_hook_writing_hci(int dev_idx, void *buf, int len)
{
	hci_t *hci = (hci_t *) buf;
	hci_image_payload_t *img;
	unsigned short cmd_evt;

	cmd_evt = B2H(hci->cmd_evt);
	switch (cmd_evt) {
		case WIMAX_DL_IMAGE:
			img = (hci_image_payload_t *) hci->data;
			switch (B2H(img->type)) {
				case DLIMG_OMA_XML:
					if (DB2H(img->offset) == -1/*EOF*/) {
						xprintf(SDK_DBG, "Hooked HCI(%04x)\n", cmd_evt);
						wm_sync_subscription(dev_idx);
					}
					break;
			}
			break;
	}
}

int sdk_write_hci_packet(api_hand_t *api_hand, int dev_idx, void *buf, int len)
{
	int ret;

	xfunc_in("[%d] len=%d", dev_idx, len);
	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	ret = io_send(dev_idx, buf, len);
	if (ret == len) {
		sdk_hook_writing_hci(dev_idx, buf, len);
		ret = 0;
	}
	else
		ret = -1;
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_set_power_control(api_hand_t *api_hand, int dev_idx, rf_stat_t rf_stat)
{
	//#define NO_CHECK_POWER_CONTROL
	sdk_internal_t *sdk = api_hand->sdk;
	#if !defined(NO_CHECK_POWER_CONTROL)
	rf_stat_t get_rf_stat;
	#endif
	int ret = -1;

	xfunc_in("rf=%s", (rf_stat==rf_on) ? "On" : "Off");
	if (sdk_check_handle(api_hand, dev_idx) < 0)
		goto out;

	if (sdk->ro) {
		ret = sdk_set_errno(ERR_PERM);
		goto out;
	}

	#if defined(NO_CHECK_POWER_CONTROL)
	ret = wm_set_rf_state(dev_idx, rf_stat, SDK_RF_UPDOWN_RESP_TIMEOUT_SEC);
	#else
	ret = wm_get_rf_state(dev_idx, &get_rf_stat);

	if (!ret && get_rf_stat != rf_stat)
		ret = wm_set_rf_state(dev_idx, rf_stat, SDK_RF_UPDOWN_RESP_TIMEOUT_SEC);
	#endif
out:
	xfunc_out();
	return ret;
}

int sdk_get_device_info(api_hand_t *api_hand, int dev_idx,
		WIMAX_API_DEVICE_INFO_P dev_info)
{
	device_t *dev;
	wimax_t *wm;
	wm_device_info_t *devi;
	wm_rev_info_t *rev;
	char str[256];
	int ret;
	u8 *p, *p2;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	wm = dev->wimax;
	devi = &wm->dev_info;
	rev = &devi->revision;

	dev_info->structureSize = sizeof(WIMAX_API_DEVICE_INFO);

	dev_info->hwVersion.structureSize = sizeof(WIMAX_API_DEVICE_VERSION);
	wcscpy((wchar_t *)dev_info->hwVersion.name, L"GCT H/W");
	p = (u8 *) &rev->phy_hw_ver;
	ret = sprintf(str, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
	mbstowcs((wchar_t *)dev_info->hwVersion.version, str, ret+1);
		
	dev_info->swVersion.structureSize = sizeof(WIMAX_API_DEVICE_VERSION);
	wcscpy((wchar_t *)dev_info->swVersion.name, L"GCT S/W");
	p = (u8 *) &rev->rel_ver;
	p2 = (u8 *) &rev->fw_ver;
	ret = sprintf(str, "FW(%d.%d.%d.%d : %d.%d.%d.%d)",
		p[0], p[1], p[2], p[3], p2[0], p2[1], p2[2], p2[3]);
	mbstowcs((wchar_t *)dev_info->swVersion.version, str, ret+1);

	dev_info->rfVersion.structureSize = sizeof(WIMAX_API_DEVICE_VERSION);
	wcscpy((wchar_t *)dev_info->rfVersion.name, L"GCT RF");
	wcscpy((wchar_t *)dev_info->rfVersion.version, (wchar_t *)dev_info->hwVersion.version);
	dev_info->asicVersion.structureSize = sizeof(WIMAX_API_DEVICE_VERSION);
	wcscpy((wchar_t *)dev_info->asicVersion.name, L"GCT ASIC");
	wcscpy((wchar_t *)dev_info->asicVersion.version, (wchar_t *)dev_info->hwVersion.version);

	memcpy(dev_info->macAddress, devi->device_mac, sizeof(devi->device_mac));
	
	mbstowcs((wchar_t*)dev_info->vendorName, (char *)devi->vendor_name,
		strlen((char *)devi->vendor_name)+1);
	dev_info->vendorSpecificInfoIncl = FALSE;
	memset(dev_info->vendorSpecificInfo, 0, sizeof(dev_info->vendorSpecificInfo));

	dm_put_dev(dev_idx);
	xfunc_out();
	return 0;
}

int sdk_get_profile_list(api_hand_t *api_hand, int dev_idx,
		WIMAX_API_PROFILE_INFO_P list, int *list_cnt)
{
	device_t *dev;
	wimax_t *wm;
	struct list_head *head;
	wm_subscription_info_t *ss_info;
	u8 profile_name[MAX_SIZE_OF_STRING_BUFFER];
	int cnt = 0;

	xfunc_in("dev=%d, list_cnt=%d", dev_idx, *list_cnt);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	wm = dev->wimax;
	head = &wm->subs_list.head;
	xprintf(SDK_DBG, "subs_list.cnt=%d\n", wm->subs_list.cnt);
	list_for_each_entry(ss_info, head, list) {
		if (cnt >= *list_cnt)
			break;
		list[cnt].structureSize = sizeof(WIMAX_API_PROFILE_INFO);
 		list[cnt].profileID = U82U24(ss_info->subscription_id.hnspid.id);
		set_msb(list[cnt].profileID, ss_info->idx);
		sprintf((char *)profile_name, "%s:%s", 
			ss_info->subscription_id.user_hnai,
			ss_info->subscription_id.hnspid.name);
		mbstowcs((wchar_t*)list[cnt].profileName, (char *)profile_name,
			strlen((char *)profile_name)+1);
		cnt++;
	}
	*list_cnt = cnt;

	xfunc_out("list_cnt=%d", *list_cnt);
	dm_put_dev(dev_idx);
	return 0;
}

int sdk_set_profile(api_hand_t *api_hand, int dev_idx, u32 profile_id)
{
	sdk_internal_t *sdk = api_hand->sdk;
	device_t *dev;
	u8 nspid[NSP_ID_SIZE];
	u8 subs_idx;
	int ret = 0;

	xfunc_in("dev=%d, profile_id=0x%08x", dev_idx, profile_id);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	if (sdk->ro)
		return sdk_set_errno(ERR_PERM);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	subs_idx = get_msb(profile_id);
	U242U8(nspid, profile_id);
	if (!(dev->wimax->scan.selected_subs = wm_get_subscription(dev_idx, subs_idx, nspid))) {
		ret = -1;
		goto out;
	}

	wm_set_scan_type(dev_idx, wm_scan_curr_subscription);

	ret = sdk_set_ioctl_profile_id(dev_idx, profile_id);
out:
	dm_put_dev(dev_idx);
	xfunc_out();
	return ret;
}

int sdk_set_scan_interval(api_hand_t *api_hand, int dev_idx, u32 interval_sec)
{
	device_t *dev;
	int ret;

	xfunc_in("dev=%d, interval=%d", dev_idx, interval_sec);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	if (dev->wimax->scan.selected_subs)
		wm_set_scan_type(dev_idx, wm_scan_curr_subscription);
	else
		wm_set_scan_type(dev_idx, wm_scan_all_subscriptions);

	ret = wm_active_scan_interval(dev_idx, interval_sec);

	dm_put_dev(dev_idx);
	xfunc_out();
	return ret;
}

static void sdk_init_cr801(wimax_t *wm)
{
	wm_eap_param_t *eapp;

	eapp = &wm->dev_info.eapp;
	
	if (eapp->cr801_enabled)
		eapp->cr801_mode = CR801_TTLS;
	else
		eapp->cr801_mode = CR801_DISABLE;
	xprintf(SDK_DBG, "cr801_mode=%d\n", eapp->cr801_mode);
}

int sdk_set_eap(api_hand_t *api_hand, int dev_idx, GCT_API_EAP_PARAM_P eap)
{
	sdk_internal_t *sdk = api_hand->sdk;
	device_t *dev;
	wm_eap_param_t *eapp;
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	eapp = &dev->wimax->dev_info.eapp;
	memset(eapp, 0, sizeof(*eapp));
	eapp->type = eap->type;

	if (eapp->type == GCT_WIMAX_NO_EAP) {
		if (!sdk->ro)
			ret = wm_set_eap(dev_idx, FALSE);
		return ret;
	}

	assert(GCT_WIMAX_EAP_TLS == W_EAP_TLS);

	if (eap && GCT_API_IS_EAP_TLS(eap->type)) {
		if (!(eapp->use_nv_info = eap->useNvramParam)) {
			strcpy(eapp->userid, (char *) eap->userId);
			strcpy(eapp->userid_pwd, (char *) eap->userIdPwd);
			strcpy(eapp->anony_id, (char *) eap->anonymousId);
			strcpy(eapp->pri_key_pwd, (char *) eap->privateKeyPwd);
		}
		eapp->frag_size = eap->fragSize;
		eapp->use_delimiter = eap->useDelimiter;
		eapp->dev_cert_null = eap->devCertNULL;
		eapp->ca_cert_null = eap->caCertNULL;
		eapp->disable_resumptoin = eap->disableResumption;
		eapp->cr801_enabled = eap->cr801Enable;
		eapp->disable_sessionticket = eap->disableSessionTicket;
		strcpy(eapp->decoration[DECORATION_IDX1], (char *) eap->decoration);
		eapp->log_enabled = eap->logEnable;
		if (!sdk->ro)
			eap_prepare_log_env(dev_idx, eapp->log_enabled);
	}
	
	sdk_init_cr801(dev->wimax);
	
	if (!sdk->ro)
		ret = wm_set_eap(dev_idx, TRUE);

	dm_put_dev(dev_idx);
	xfunc_out();
	return ret;
}

int sdk_get_statistics(api_hand_t *api_hand, int dev_idx,
		WIMAX_API_CONNECTION_STAT_P statistics)
{
	wm_net_info_t net_info;
	int ret;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	ret = wm_get_netinfo(dev_idx, &net_info);
	if (!ret) {
		statistics->structureSize = sizeof(WIMAX_API_CONNECTION_STAT);
		statistics->totalRxByte = net_info.rx_bytes;
		statistics->totalTxByte = net_info.tx_bytes;
		statistics->totalRxPackets = net_info.rx_packets;
		statistics->totalTxPackets = net_info.tx_packets;
	}
	xfunc_out();
	return ret;
}

int sdk_get_linkstatus(api_hand_t *api_hand, int dev_idx,
		WIMAX_API_LINK_STATUS_INFO_P link_status)
{
	device_t *dev;
	struct wimax_s *wm;
	wm_link_status_t link_s;
	int ret;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	wm = dev->wimax;

	ret = wm_get_linkinfo(dev_idx, &link_s);
	if (!ret) {
		link_status->structureSize = sizeof(WIMAX_API_LINK_STATUS_INFO);
		link_status->centerFrequency = link_s.cur_freq;
		link_status->RSSI = link_s.rssi;
		link_status->CINR = link_s.cinr;
		link_status->txPWR = link_s.tx_power;
		memcpy(link_status->bsId, wm->conn_comp.net_id.bs.id, BS_ID_SIZE);
	}

	dm_put_dev(dev_idx);
	xfunc_out();
	return ret;
}

static int get_connected_nsp(int dev_idx, WIMAX_API_CONNECTED_NSP_INFO_P nsp_info)
{
	device_t *dev;
	wimax_t *wm;
	wm_nsp_t *nsp;
	struct list_head *head;
	int ret = -1;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;
	
	wm = dev->wimax;
	head = &wm->scan_list.head;

	nsp_info->structureSize = sizeof(WIMAX_API_CONNECTED_NSP_INFO);
	nsp_info->activated = TRUE;
	nsp_info->NSPid = U82U24(wm->conn_comp.net_id.nsp.id);

	pthread_mutex_lock(&wm->scan_list.lock);
	list_for_each_entry(nsp, head, list) {
		if (nsp_info->NSPid == nsp->id) {
			mbstowcs((wchar_t *)nsp_info->NSPName, (char *)nsp->name,
				strlen((char *)nsp->name)+1);
			nsp_info->RSSI = nsp->rssi;
			nsp_info->CINR = nsp->cinr;
			nsp_info->networkType = nsp->type;
			#if defined( NO_IMPLIMENT )
			nsp_info->NSPRealm
			#else
			memset(nsp_info->NSPRealm, 0, sizeof(nsp_info->NSPRealm));
			#endif
			ret = 0;
			break;
		}
	}
	pthread_mutex_unlock(&wm->scan_list.lock);

	xfunc_out("ret=%d", ret);
	dm_put_dev(dev_idx);
	return ret;
}

int sdk_set_ioctl_connected_nsp(sdk_internal_t *sdk, int dev_idx)
{
	WIMAX_API_CONNECTED_NSP_INFO nsp_info;
	int ret = -1;

	xfunc_in("dev=%d, ro=%d", dev_idx, sdk->ro);

	if (sdk->ro)
		return 0;

	if (!(ret = get_connected_nsp(dev_idx, &nsp_info))) {
		ret = net_ioctl_set_data(dev_idx, SIOC_DATA_CONNNSP, &nsp_info, sizeof(nsp_info));
		if (ret > 0) ret = 0;
	}
	
	xfunc_out("ret=%d", ret);
	dm_put_dev(dev_idx);
	return ret;
}

int sdk_get_ioctl_connected_nsp(int dev_idx, WIMAX_API_CONNECTED_NSP_INFO_P nsp_info)
{
	int ret;

	xfunc_in("dev=%d", dev_idx);

	ret = net_ioctl_get_data(dev_idx, SIOC_DATA_CONNNSP, nsp_info, sizeof(*nsp_info));
	if (ret > 0) ret = 0;
	
	xfunc_out("ret=%d", ret);
	dm_put_dev(dev_idx);
	return ret;
}

int sdk_get_connected_nsp(api_hand_t *api_hand, int dev_idx, 
		WIMAX_API_CONNECTED_NSP_INFO_P nsp_info)
{
	#define GET_CURRENT_LINK
	#if defined(GET_CURRENT_LINK)
	wm_link_status_t link_s;
	#endif
	int ret;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;
	
	if (api_hand->sdk->ro)
		ret = sdk_get_ioctl_connected_nsp(dev_idx, nsp_info);
	else
		ret = get_connected_nsp(dev_idx, nsp_info);
	
	#if defined(GET_CURRENT_LINK)
	if (!wm_get_linkinfo(dev_idx, &link_s)) {
		nsp_info->RSSI = link_s.rssi;
		nsp_info->CINR = link_s.cinr;
	}
	#endif

	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_set_ioctl_complete_info(int dev_idx)
{
	device_t *dev;
	wimax_t *wm;
	int ret = -1;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;
	wm = dev->wimax;

	ret = net_ioctl_set_data(dev_idx, SIOC_DATA_CONNCOMP,
		&wm->conn_comp, sizeof(wm->conn_comp));
	if (ret > 0) ret = 0;
	
	dm_put_dev(dev_idx);
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_load_ioctl_complete_info(int dev_idx)
{
	device_t *dev;
	wimax_t *wm;
	int ret;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;
	wm = dev->wimax;

	ret = net_ioctl_get_data(dev_idx, SIOC_DATA_CONNCOMP,
		&wm->conn_comp, sizeof(wm->conn_comp));
	if (ret > 0) ret = 0;

	dm_put_dev(dev_idx);
	xfunc_out("ret=%d", ret);
	return ret;
}

static int sdk_set_ioctl_profile_id(int dev_idx, u32 profile_id)
{
	int ret = -1;

	xfunc_in("dev=%d", dev_idx);

	ret = net_ioctl_set_data(dev_idx, SIOC_DATA_PROFILEID,
		&profile_id, sizeof(profile_id));
	if (ret > 0) ret = 0;

	dm_put_dev(dev_idx);
	xfunc_out("ret=%d", ret);
	return ret;
}

static int sdk_load_ioctl_profile_id(int dev_idx)
{
	device_t *dev;
	u32 profile_id = 0;
	u8 nspid[NSP_ID_SIZE];
	u8 subs_idx;
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	if (dev->wimax->scan.type != wm_scan_curr_subscription) {
		xprintf(SDK_DBG, "Scan type(%d) does not need loading profile_id\n",
			dev->wimax->scan.type);
		goto out;
	}

	if ((ret = net_ioctl_get_data(dev_idx, SIOC_DATA_PROFILEID,
		&profile_id, sizeof(profile_id))) < 0)
		goto out;
	ret = 0;

	subs_idx = get_msb(profile_id);
	U242U8(nspid, profile_id);
	if (!(dev->wimax->scan.selected_subs = wm_get_subscription(dev_idx, subs_idx, nspid)))
		ret = -1;

out:
	dm_put_dev(dev_idx);
	xfunc_out("ret=%d, *profile_id=0x%08X", ret, profile_id);
	return ret;
}

static int get_network_list(int dev_idx, WIMAX_API_NSP_INFO_P list, u32 *list_cnt)
{
	device_t *dev;
	wimax_t *wm;
	wm_nsp_t *nsp;
	struct list_head *head;
	int cnt = 0;

	xfunc_in("dev=%d, list_cnt=%d", dev_idx, *list_cnt);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	wm = dev->wimax;
	head = &wm->scan_list.head;

	pthread_mutex_lock(&wm->scan_list.lock);
	xprintf(SDK_DBG, "scan_list.cnt=%d\n", wm->scan_list.cnt);
	list_for_each_entry(nsp, head, list) {
		if (cnt >= *list_cnt)
			break;
		list[cnt].structureSize = sizeof(WIMAX_API_NSP_INFO);
		mbstowcs((wchar_t *)list[cnt].NSPName, (char *)nsp->name,
			strlen((char *)nsp->name)+1);
		list[cnt].NSPid = nsp->id;
		list[cnt].RSSI = nsp->rssi;
		list[cnt].CINR = nsp->cinr;
		list[cnt].networkType = nsp->type;
		cnt++;
	}
	*list_cnt = cnt;
	pthread_mutex_unlock(&wm->scan_list.lock);

	xfunc_out("cnt=%d", cnt);
	dm_put_dev(dev_idx);
	return 0;
}

int sdk_set_ioctl_network_list(sdk_internal_t *sdk, int dev_idx)
{
	device_t *dev;
	wimax_t *wm;
	WIMAX_API_NSP_INFO *nsp_info = NULL;
	int ret = 0, cnt, cnt2;

	xfunc_in("dev=%d, ro=%d", dev_idx, sdk->ro);

	if (sdk->ro)
		return 0;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;
	
	wm = dev->wimax;
	cnt = cnt2 = wm->scan_list.cnt;

	if (!cnt)
		goto out;

	nsp_info = (WIMAX_API_NSP_INFO *) sdk_malloc(sizeof(WIMAX_API_NSP_INFO) * cnt);
	assert(nsp_info);

	ret = get_network_list(dev_idx, nsp_info, (u32 *)&cnt2);
	if (ret < 0)
		goto out;
	assert(cnt == cnt2);

	ret = net_ioctl_set_data(dev_idx, SIOC_DATA_NETLIST,
		nsp_info, sizeof(*nsp_info) * cnt);
	if (ret > 0) ret = 0;
out:
	if (nsp_info)
		sdk_free(nsp_info);
	
	xfunc_out("ret=%d", ret);
	dm_put_dev(dev_idx);
	return ret;
}

int sdk_get_ioctl_network_list(int dev_idx, WIMAX_API_NSP_INFO_P list, u32 *list_cnt)
{
	device_t *dev;
	int ret;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	if ((ret = net_ioctl_get_data(dev_idx, SIOC_DATA_NETLIST,
		list, sizeof(*list) * *list_cnt)) > 0) {
		assert(ret % sizeof(WIMAX_API_NSP_INFO) == 0);
		*list_cnt = ret / sizeof(WIMAX_API_NSP_INFO);
	}

	xfunc_out("ret=%d", ret);
	dm_put_dev(dev_idx);
	return ret;
}

int sdk_get_network_list(api_hand_t *api_hand, int dev_idx, WIMAX_API_NSP_INFO_P list,
		u32 *list_cnt)
{
	int ret;

	xfunc_in("dev=%d, list_cnt=%d", dev_idx, *list_cnt);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	if (api_hand->sdk->ro)
		ret = sdk_get_ioctl_network_list(dev_idx, list, list_cnt);
	else
		ret = get_network_list(dev_idx, list, list_cnt);

	xfunc_out("ret=%d, cnt=%d", ret, *list_cnt);
	return ret;
}

static bool is_connection_ready(sdk_internal_t *sdk, int dev_idx, char *state)
{
	int m_status, c_status;
	bool ret = FALSE;
	
	sdk_get_status(sdk, dev_idx, &m_status, &c_status);

	switch (m_status) {
		case M_INIT:
		case M_OPEN_OFF:
			strcpy(state, "RF-off");
			break;
		case M_OPEN_ON:
			strcpy(state, "Ready");
			ret = TRUE;
			break;
		case M_SCAN:
			strcpy(state, "Scanning");
			ret = TRUE;
			break;
		case M_CONNECTING:
			strcpy(state, "Connecting");
			break;
		case M_CONNECTED:
			strcpy(state, "Connected");
			break;
		default:
			strcpy(state, "Unknown");
			break;
	}

	return ret;
}

int sdk_connect_network(api_hand_t *api_hand, int dev_idx, uchar *nsp_name, u32 profile_id)
{
	device_t *dev = NULL;
	wimax_t *wm;
	wm_nsp_t *nsp;
	u8 hnspid[NSP_ID_SIZE], vnspid[NSP_ID_SIZE];
	u8 srched_nspid[NSP_ID_SIZE];
	u8 nspname[WM_MAX_NSP_NAME_LEN];
	char conn_state[128];
	wm_subscription_info_t	*subs;
	int ret;

	xfunc_in("[%d], nsp=%S, 0x%08x", dev_idx, nsp_name, profile_id);

	if ((ret = sdk_check_handle(api_hand, dev_idx)) < 0)
		goto out;

	if (api_hand->sdk->ro) {
		ret = sdk_set_errno(ERR_PERM);
		goto out;
	}

	if (!(dev = dm_get_dev(dev_idx))) {
		ret = -1;
		goto out;
	}

	if (!is_connection_ready(api_hand->sdk, dev_idx, conn_state)) {
		xprintf(SDK_ERR, "Current state(%s) is not connectable state!!\n", conn_state);
		ret = -1;
		goto out;
	}

	wm = dev->wimax;

	if (wm->scan.sf_mode) {
		u8 nspid[NSP_ID_SIZE];
		WIMAX_API_NSP_INFO net_info;
		u32 cnt = 1;

		if (!get_network_list(dev_idx, &net_info, &cnt)) {
			if (cnt == 1) {
				U242U8(nspid, net_info.NSPid);
				if ((subs = wm_get_subscription(dev_idx, 0, nspid))) {
					wm->scan.selected_subs = subs;
					xprintf(SDK_INFO, "user_hnai=%s\n",
						wm->scan.selected_subs->subscription_id.hnspid.name);
				}
			}
			else
				xprintf(SDK_ERR, "get_network_list is %d\n", cnt);
		}
	}
	
	if (!wm->scan.selected_subs && wm->scan.type != wm_scan_all_channels && !profile_id) {
		xprintf(SDK_ERR, "There is no selected subscription\n");
		goto out;
	}

	wcstombs((char *)nspname, (wchar_t *)nsp_name, wcslen((wchar_t *)nsp_name)+1);
	xprintf(SDK_DBG, "nspname=%s(%d)\n", nspname, strlen((char *)nspname));
	
	if (wm->scan.type == wm_scan_all_channels)
		U242U8(hnspid, WIDE_SCAN_HNSP_ID);
	else {
		if (profile_id) {
			if (!wm->scan.selected_subs) {
				u8 subs_idx = get_msb(profile_id);
				U242U8(hnspid, profile_id);
				wm->scan.selected_subs = wm_get_subscription(dev_idx, subs_idx, hnspid);
			}
		}
		if (!wm->scan.selected_subs) {
			xprintf(SDK_ERR, "selected_subs is NULL\n");
			ret = -1;
			goto out;
		}
		memcpy(hnspid, wm->scan.selected_subs->subscription_id.hnspid.id, NSP_ID_SIZE);
	}

	if (profile_id) {
		U242U8(srched_nspid, profile_id);
		if ((nsp = wm_lookup_nsp_by_id(wm, srched_nspid)) == NULL) {
			xprintf(SDK_ERR, "[%d] NSP(%06x) is not found\n",
				dev_idx, U82U24(srched_nspid));
			ret = sdk_set_errno(ERR_NO_NSPID);
			goto out;
		}
	}
	else {
		if ((nsp = wm_lookup_nsp_by_name(wm, nspname)) == NULL) {
			xprintf(SDK_ERR, "[%d] NSP(%s) is not found\n", dev_idx, nspname);
			ret = sdk_set_errno(ERR_NO_NSPID);
			goto out;
		}
	}
	U242U8(vnspid, nsp->id);
	
	if (wm->dev_info.eapp.cr801_enabled) {
		wm->dev_info.eapp.cr801_server_reject_cnt = 0;
		sdk_init_cr801(wm);
		if (E_EAP_TLS_ENABLED(dev))
			wm_set_eap(dev_idx, TRUE);
	}

	ret = wm_connect_network(dev_idx, hnspid, vnspid);
out:
	xfunc_out("ret=%d", ret);
	if (dev)
		dm_put_dev(dev_idx);
	return ret;
}

int sdk_disconnect_network(api_hand_t *api_hand, int dev_idx)
{
	int ret;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	if (api_hand->sdk->ro)
		return sdk_set_errno(ERR_PERM);

	ret = wm_disconnect_network(dev_idx, SDK_DISCONN_RESP_TIMEOUT_SEC);
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_get_rf_info(api_hand_t *api_hand, int dev_idx, GCT_API_RF_INFORM_P rf_info)
{
	int ret;

	xfunc_in("dev=%d rf_info=0x%08x", dev_idx, rf_info);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	ret = wm_get_rf_info(dev_idx, rf_info);

	dm_put_dev(dev_idx);
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_get_bl_ver(api_hand_t *api_hand, int dev_idx, char *buf, int size)
{
	device_t *dev;
	char str[64];
	u8 *ver;
	int ret;

	xfunc_in("dev=%d size=%d", dev_idx, size);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	ver = (u8 *) &dev->wimax->dev_info.revision.bl_ver;
	ret = sprintf(str, "%d.%d.%d.%d", ver[0], ver[1], ver[2], ver[3]);

	xprintf(SDK_DBG, "%s\n", str);
	if (size < ret)
		ret = -1;
	else {
		strcpy(buf, str);
		ret = 0;
	}

	dm_put_dev(dev_idx);
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_get_capability(api_hand_t *api_hand, int dev_idx, u32 *cap)
{
	device_t *dev;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	*cap = dev->capability;

	dm_put_dev(dev_idx);
	xfunc_out("dev->capability=%d", dev->capability);
	return 0;
}

int sdk_set_capability(api_hand_t *api_hand, int dev_idx, u32 cap)
{
	device_t *dev;
	int ret;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	ret = wm_set_capability(dev_idx, cap);

	dm_put_dev(dev_idx);
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_get_neighbor_list(api_hand_t *api_hand, int dev_idx,
		GCT_API_NEIGHBOR_LIST_P list, int *list_cnt)
{
	device_t *dev;
	wm_bs_neigh_t *neigh = NULL;
	int cnt, ret, i;

	xfunc_in("dev=%d, list_cnt=%d", dev_idx, *list_cnt);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	cnt = *list_cnt;
	neigh = (wm_bs_neigh_t *) sdk_malloc(sizeof(wm_bs_neigh_t) * cnt);
	assert(neigh);
	if (!(ret = wm_get_neighbor_list(dev_idx, neigh, &cnt))) {
		for (i = 0; i < cnt; i++) {
			list[i].structureSize = sizeof(*list);
			memcpy(list[i].bsId, neigh[i].bsid, sizeof(list[i].bsId));
			list[i].rssi = neigh[i].rssi;
			list[i].cinr = neigh[i].cinr;
			list[i].preamble = neigh[i].preamble;
			list[i].frequency = neigh[i].frequency;
		}
		*list_cnt = cnt;
	}
	else
		*list_cnt = 0;

	sdk_free(neigh);
	dm_put_dev(dev_idx);
	xfunc_out("ret=%d, cnt=%d", ret, cnt);
	return ret;
}

int sdk_net_search_scan(api_hand_t *api_hand, int dev_idx, GCT_API_SCAN_TYPE type)
{
	sdk_internal_t *sdk;
	int ret;

	xfunc_in("dev=%d, type=%d", dev_idx, type);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	sdk = api_hand->sdk;

	if (sdk->ro)
		return sdk_set_errno(ERR_PERM);

	ret = wm_net_search_scan(dev_idx, type);
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_cancel_scan(api_hand_t *api_hand, int dev_idx)
{
	int ret;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	if (api_hand->sdk->ro)
		return sdk_set_errno(ERR_PERM);

	ret = wm_cancel_scan(dev_idx);
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_cmd_mac_state(api_hand_t *api_hand, int dev_idx, GCT_API_CMD_MAC_STATE_TYPE type)
{
	int ret;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	if (api_hand->sdk->ro)
		return sdk_set_errno(ERR_PERM);

	ret = wm_cmd_mac_state(dev_idx, type);
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_set_idle_mode_timeout(api_hand_t *api_hand, int dev_idx, u16 timeoutSec)
{
	int ret;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	if (api_hand->sdk->ro)
		return sdk_set_errno(ERR_PERM);

	ret = wm_set_idle_mode_timeout(dev_idx, timeoutSec);
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_get_phy_mac_basic(api_hand_t *api_hand, int dev_idx, GCT_API_MAC_PHY_MAC_BASIC_P pData)
{
	int ret;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	ret = wm_get_phy_mac_basic(dev_idx, pData);
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_get_phy_mcs(api_hand_t *api_hand, int dev_idx, GCT_API_MAC_PHY_MCS_P pData)
{
	int ret;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	ret = wm_get_phy_mcs(dev_idx, pData);
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_get_phy_cinr_rssi(api_hand_t *api_hand, int dev_idx, GCT_API_MAC_PHY_CINR_RSSI_P pData)
{
	int ret;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	ret = wm_get_phy_cinr_rssi(dev_idx, pData);
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_on_app_event(sdk_internal_t *sdk, int dev_idx, fsm_event_t event)
{
	device_t *dev;
	wimax_t *wm;
	int m_status, c_status;
	rf_stat_t rf_stat;
	int ret = 0;
	int evt_mask = SDK_INFO;

	xfunc_in("dev=%d, event=%d", dev_idx, event);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;
	wm = dev->wimax;

	#if 0
	if ((ret = dm_get_status(dev_idx, &m_status, &c_status)) < 0) {
		ret = -1;
		goto out;
	}
	#else
	if ((ret = sdk_get_status(sdk, dev_idx, &m_status, &c_status)) < 0) {
		ret = -1;
		goto out;
	}
	#endif

	dev->a_status = event;

	switch ((int)event) {
		case AM_Open:
			xprintf(evt_mask, "[%d] %s: m_s=%d, c_s=%d\n",
				dev_idx, STR(AM_Open), m_status, c_status);
			if (sdk->ro) {
				if (m_status == M_CONNECTED)
					sdk_load_ioctl_complete_info(dev_idx);
				break;
			}
			if (m_status == M_INIT) {
				sdk_set_status(sdk, dev_idx, M_OPEN_OFF, C_INIT);
				ret = wm_get_rf_state(dev_idx, &rf_stat);
				if (ret < 0)
					break;
				if (rf_stat == rf_on) {
					#if 1
					ret = wm_set_rf_state(dev_idx, rf_off, SDK_RF_UPDOWN_RESP_TIMEOUT_SEC);
					#else
					ret = sdk_on_ind_event(sdk, dev_idx, NM_RadioOn);
					#endif
				}
			}
			break;
		case AM_Scan:
			xprintf(evt_mask, "[%d] %s: m_s=%d, c_s=%d\n",
				dev_idx, STR(AM_Scan), m_status, c_status);
			if (sdk->ro)
				break;
			if (m_status == M_OPEN_ON) {
				ret = sdk_set_status(sdk, dev_idx, M_SCAN, C_INIT);
				wm_set_scan_type(dev_idx, wm_scan_all_channels);
			}
			break;
		case AM_Close:
			xprintf(evt_mask, "[%d] %s: m_s=%d, c_s=%d\n",
				dev_idx, STR(AM_Close), m_status, c_status);
			if (sdk->ro)
				break;
			sdk_set_status(sdk, dev_idx, M_INIT, C_INIT);
			switch (m_status) {
				case M_CONNECTING:
				case M_CONNECTED:
					#if 0
					if (dev->inserted)
						wm_disconnect_network(dev_idx, SDK_DISCONN_RESP_TIMEOUT_SEC);
					#endif
				case M_OPEN_ON:
				case M_SCAN:
					if (dev->inserted) {
						sdk_set_status(sdk, dev_idx, M_INIT, C_INIT);
						#if 1
						wm_set_rf_state(dev_idx, rf_off, 0);
						#else
						wm_set_rf_state(dev_idx, rf_off, SDK_RF_UPDOWN_RESP_TIMEOUT_SEC);
						#endif
					}
					break;
			}
			break;
	}

out:
	dm_put_dev(dev_idx);
	xfunc_out("m_s=%d, c_s=%d", m_status, c_status);
	return ret;
}

int sdk_on_ind_event(sdk_internal_t *sdk, int dev_idx, fsm_event_t event)
{
	device_t *dev;
	wimax_t *wm;
	int m_status, c_status;
	int ret = 0;
	int evt_mask = SDK_INFO;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;
	wm = dev->wimax;

	pthread_mutex_lock(&wm->fsm_lock);

	sdk_get_status(sdk, dev_idx, &m_status, &c_status);

	xfunc_in("dev=%d, event=%d, m_s=%d, c_s=%d", dev_idx, event, m_status, c_status);

	switch ((int)event) {
		case NM_RadioOn:
			xprintf(evt_mask, "[%d] %s\n", dev_idx, STR(NM_RadioOn));
			if (m_status == M_OPEN_OFF) {
				sdk_set_status(sdk, dev_idx, M_OPEN_ON, C_INIT);
				sdk_ind_power_ctrl(sdk, dev_idx, TRUE);
				if (!sdk->ro)
					ret = wm_req_scan(dev_idx);
			}
			break;
		case NM_RadioOff:
			xprintf(evt_mask, "[%d] %s\n", dev_idx, STR(NM_RadioOff));
			switch (m_status) {
				case M_OPEN_ON:
				case M_SCAN:
				case M_CONNECTING:
				case M_CONNECTED:
					sdk_set_status(sdk, dev_idx, M_OPEN_OFF, C_INIT);
					sdk_ind_power_ctrl(sdk, dev_idx, FALSE);
					if (!sdk->ro)
						wm_clean_scan_info(dev_idx);
					break;
			}
			break;
		case NM_ScanComplete:
			xprintf(evt_mask, "[%d] %s\n", dev_idx, STR(NM_ScanComplete));
			if (!sdk->ro) {
				if (m_status != M_CONNECTING) {
					#if defined(CONFIG_ENABLE_BW_SWITCHING_FOR_KT)
					nds_update_switching_bw(dev_idx);
					#endif
					sdk_set_ioctl_network_list(sdk, dev_idx);
					if ((ret = sdk_set_status(sdk, dev_idx, M_OPEN_ON, C_INIT)) < 0)
						break;
					if ((ret = wm_req_interval_scan(dev_idx, 0)) < 0)
						break;
					if (wm->scan.type == wm_scan_all_channels)
						ret = sdk_ind_net_search_wscan(sdk, dev_idx);
				}
			}
			break;
		case NC_ConnectStart:
			xprintf(evt_mask, "[%d] %s\n", dev_idx, STR(NC_ConnectStart));
			if (sdk->ro)
				sdk_load_ioctl_profile_id(dev_idx);
			break;
		case NC_ConnectComplete:
			xprintf(evt_mask, "[%d] %s(%d)\n", dev_idx, STR(NC_ConnectComplete), c_status);
			if (c_status == C_ASSOCCOMPLETE) {
				if (!sdk->ro) {
					sdk_set_ioctl_connected_nsp(sdk, dev_idx);
					sdk_set_ioctl_complete_info(dev_idx);
					sdk_set_status(sdk, dev_idx, M_CONNECTED, C_CONNCOMPLETE);
				}
				sdk_ind_connect_net(sdk, dev_idx, TRUE);
			}
			else if (!sdk->ro)
				wm_req_interval_scan(dev_idx, 100);
			break;
		case NC_ConnectFail:
			xprintf(evt_mask, "[%d] %s(%d)\n", dev_idx, STR(NC_ConnectFail), c_status);
			if (c_status == C_CONNSTART || c_status == C_ASSOCCOMPLETE) {
				if (!sdk->ro && wm->dev_info.eapp.cr801_enabled) {
					pthread_mutex_unlock(&wm->fsm_lock);
					if (!wm_cr801_re_connect(dev_idx))
						goto out_no_unlock;
				}
				sdk_set_status(sdk, dev_idx, M_OPEN_ON, C_INIT);
				sdk_ind_connect_net(sdk, dev_idx, FALSE);
				if (!sdk->ro) {
					wm_req_interval_scan(dev_idx, 100);
				}
			}
			break;
		case NC_Disconnect:
			xprintf(evt_mask, "[%d] %s\n", dev_idx, STR(NC_Disconnect));
			if (m_status == M_CONNECTED) {
				if (!sdk->ro) {
					sdk_ind_disconnect_net(sdk, dev_idx, TRUE);
					/*We should change the state after calling indication.*/
					sdk_set_status(sdk, dev_idx, M_OPEN_ON, C_INIT);
					wm_req_interval_scan(dev_idx, 100);
				}
				else
					sdk_ind_disconnect_net(sdk, dev_idx, TRUE);
			}
		case NC_AssocStart:
			xprintf(evt_mask, "[%d] %s\n", dev_idx, STR(NC_AssocStart));
			if (c_status == C_CONNSTART || c_status == C_ASSOCCOMPLETE)
				sdk_set_status(sdk, dev_idx, m_status, C_ASSOCSTART);
			break;
		case NC_AssocSuccess:
			xprintf(evt_mask, "[%d] %s\n", dev_idx, STR(NC_AssocSuccess));
			if (c_status == C_REG || c_status == C_DSX)
				sdk_set_status(sdk, dev_idx, m_status, C_ASSOCCOMPLETE);
			break;
		case NC_AssocFail:
			xprintf(evt_mask, "[%d] %s\n", dev_idx, STR(NC_AssocFail));
			switch (c_status) {
				case C_ASSOCSTART:
				case C_RNG:
				case C_SBC:
				case C_AUTH:
				case C_REG:
				case C_DSX:
					sdk_set_status(sdk, dev_idx, m_status, C_ASSOCCOMPLETE);
					break;
			}
			break;
		case NC_RangingStart:
			xprintf(evt_mask, "[%d] %s\n", dev_idx, STR(NC_RangingStart));
			if (c_status == C_ASSOCSTART)
				sdk_set_status(sdk, dev_idx, m_status, C_RNG);
			break;
		case NC_RangingComplete:
			xprintf(evt_mask, "[%d] %s\n", dev_idx, STR(NC_RangingComplete));
			break;
		case NC_SbcStart:
			xprintf(evt_mask, "[%d] %s\n", dev_idx, STR(NC_SbcStart));
			if (c_status == C_RNG)
				sdk_set_status(sdk, dev_idx, m_status, C_SBC);
			break;
		case NC_SbcComplete:
			xprintf(evt_mask, "[%d] %s\n", dev_idx, STR(NC_SbcComplete));
			break;
		case NC_AuthStart:
			xprintf(evt_mask, "[%d] %s\n", dev_idx, STR(NC_AuthStart));
			if (c_status == C_RNG || c_status == C_SBC)
				sdk_set_status(sdk, dev_idx, m_status, C_AUTH);
			break;
		case NC_AuthComplete:
			xprintf(evt_mask, "[%d] %s\n", dev_idx, STR(NC_AuthComplete));
			if (E_EAP_TLS_ENABLED(dev) && wm->dev_info.eapp.log_enabled)
				eap_start_e_eaplog_thread(dev_idx);
			break;
		case NC_RegStart:
			xprintf(evt_mask, "[%d] %s\n", dev_idx, STR(NC_RegStart));
			if (c_status == C_RNG || c_status == C_SBC || c_status == C_AUTH)
				sdk_set_status(sdk, dev_idx, m_status, C_REG);
			break;
		case NC_RegComplete:
			xprintf(evt_mask, "[%d] %s\n", dev_idx, STR(NC_RegComplete));
			break;
		case NC_DsxStart:
			xprintf(evt_mask, "[%d] %s\n", dev_idx, STR(NC_DsxStart));
			if (c_status == C_REG)
				sdk_set_status(sdk, dev_idx, m_status, C_DSX);
			break;
		case NC_DsxComplete:
			xprintf(evt_mask, "[%d] %s\n", dev_idx, STR(NC_DsxComplete));
			break;
	}

	pthread_mutex_unlock(&wm->fsm_lock);
out_no_unlock:
	dm_put_dev(dev_idx);
	xfunc_out();
	return ret;
}

int sdk_ind_event(int dev_idx, fsm_event_t event)
{
	struct list_head *head = hand_get_api_handle_list();
	api_hand_t *handle;

	pthread_mutex_lock(&api_handle_lock);
	list_for_each_entry(handle, head, list) {
		if (handle->sdk && handle->sdk->dev_open[dev_idx] && dm_tst_dev(dev_idx))
			sdk_on_ind_event(handle->sdk, dev_idx, event);
	}
	pthread_mutex_unlock(&api_handle_lock);
	return 0;
}

#if defined(CONFIG_ENABLE_SERVICE_FLOW)
int sdk_sf_BeginSFRead(api_hand_t *api_hand, int dev_idx)
{
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	BeginSFRead(dev_idx);

	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_sf_EndSFRead(api_hand_t *api_hand, int dev_idx)
{
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	EndSFRead(dev_idx);
	
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_sf_GetNextSF(api_hand_t *api_hand, int dev_idx,
				WIMAX_SERVICE_FLOW *pSF,
				UINT8 Direction,
				WIMAX_SERVICE_FLOW **ppRetSF)
{
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	*ppRetSF = GetNextSF(dev_idx, pSF, Direction);
	
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_sf_GetServiceFlow(api_hand_t *api_hand, int dev_idx,
				UINT32 SFID,
				WIMAX_SERVICE_FLOW **ppRetSF)
{
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	*ppRetSF = GetServiceFlow(dev_idx, SFID);
	
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_sf_GetNextClfrRule(api_hand_t *api_hand, int dev_idx, 
				WIMAX_SERVICE_FLOW *pSF,
				WIMAX_CLFR_RULE *pCLFRRule,
				WIMAX_CLFR_RULE **ppRetCLFRRule)
{
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	*ppRetCLFRRule = GetNextClfrRule(pSF, pCLFRRule);

	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_sf_GetClfrRule(api_hand_t *api_hand, int dev_idx, 
				WIMAX_SERVICE_FLOW *pSF,
				UINT16 PacketClassfierRuleIndex,
				WIMAX_CLFR_RULE **ppRetCLFRRule)
{
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	*ppRetCLFRRule = GetClfrRule(pSF, PacketClassfierRuleIndex);
	
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_sf_GetNextPHSRule(api_hand_t *api_hand, int dev_idx, 
				WIMAX_SERVICE_FLOW *pSF,
				WIMAX_PHS_RULE *pPHSRule,
				WIMAX_PHS_RULE **ppRetPHSRule)
{
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	*ppRetPHSRule = GetNextPHSRule(pSF, pPHSRule);
	
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_sf_GetPHSRule(api_hand_t *api_hand, int dev_idx, 
				WIMAX_SERVICE_FLOW *pSF, 
				UINT8 PHSI,
				WIMAX_PHS_RULE **ppRetPHSRule)
{
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	*ppRetPHSRule = GetPHSRule(pSF, PHSI);
	
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_sf_CmdAddSF(api_hand_t *api_hand, int dev_idx,
				WIMAX_SF_PARAM_P pSFParam,
				WIMAX_CLFR_RULE_P pClfrRule,
				WIMAX_PHS_RULE_P pPHSRule)
{
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	if (WIMAX_SF_SUCCESS != CmdAddSF(dev_idx, pSFParam, pClfrRule, pPHSRule)) {
		ret = -1;
	}
	
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_sf_CmdChangeSF(api_hand_t *api_hand, int dev_idx,
				WIMAX_SF_PARAM_P pSFParam,
				WIMAX_CLFR_DSC_ACTION CLFRDSCAction,
				WIMAX_CLFR_RULE_P pClfrRule,
				WIMAX_PHS_DSC_ACTION PHSDSCAction,
				WIMAX_PHS_RULE_P pPHSRule)
{
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	if (WIMAX_SF_SUCCESS != CmdChangeSF(dev_idx, pSFParam, CLFRDSCAction, pClfrRule, PHSDSCAction, pPHSRule)) {
		ret = -1;
	}
	
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_sf_CmdDeleteSF(api_hand_t *api_hand, int dev_idx,
				WIMAX_SF_PARAM_P pSFParam)
{
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);

	if (sdk_check_handle(api_hand, dev_idx) < 0)
		return -1;

	if (WIMAX_SF_SUCCESS != CmdDeleteSF(dev_idx, pSFParam)) {
		ret = -1;
	}
	
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_sf_recv_dsx_complete(int dev_idx, u8 *buf, int len)
{
	struct hci *hci = (struct hci *)buf;
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);
	
	hci->cmd_evt = ntohs(hci->cmd_evt);
	hci->length = ntohs(hci->length);

	dev_update_service_flow(dev_idx, hci);

	xfunc_out("ret=%d", ret);
	return ret;
}
#endif // CONFIG_ENABLE_SERVICE_FLOW

int sdk_ind_recv_hci_pkt(int dev_idx, char *buf, int len)
{
	struct list_head *head = hand_get_api_handle_list();
	api_hand_t *handle;

	xfunc_in("dev=%d, len=%d", dev_idx, len);

	pthread_mutex_lock(&api_handle_lock);
	list_for_each_entry(handle, head, list) {
		if (handle->sdk->dev_open[dev_idx] && handle->sdk->ind.recv_hci
			&& dm_tst_dev(dev_idx)) {
			ind_msg_t *msg = (ind_msg_t *) sdk_malloc(sizeof(ind_msg_t));
			msg->dev_idx = dev_idx;
			msg->u.recv_hci.buf = sdk_malloc(len+1/*null char*/);
			memcpy(msg->u.recv_hci.buf, buf, len);
			#if 0
			msg->u.recv_hci.buf[len] = 0;	/*set null char*/
			#endif
			msg->u.recv_hci.len = len;
			msg_send(&handle->sdk->ind.msg_cb, ind_recv_hci, msg);
		}
	}
	pthread_mutex_unlock(&api_handle_lock);

	xfunc_out();
	return 0;
}

int sdk_ind_dev_insert_remove(int dev_idx, bool is_insert)
{
	struct list_head *head = hand_get_api_handle_list();
	api_hand_t *handle;

	xfunc_in("dev=%d, %s", dev_idx, is_insert ? "insert" : "remove");

	pthread_mutex_lock(&api_handle_lock);
	list_for_each_entry(handle, head, list) {
		if (handle->sdk->ind.insert_remove) {
			ind_msg_t *msg;
			msg = (ind_msg_t *) sdk_malloc(sizeof(ind_msg_t));
			msg->dev_idx = dev_idx;
			msg->u.insert_remove.presence = is_insert;
			msg_send(&handle->sdk->ind.msg_cb, ind_insert_remove, msg);
		}
	}
	pthread_mutex_unlock(&api_handle_lock);

	xfunc_out();
	return 0;
}

int sdk_ind_status_update(int dev_idx, u8 *buf, int len)
{
	device_t *dev;
	struct list_head *head = hand_get_api_handle_list();
	api_hand_t *handle;
	WIMAX_API_DEVICE_STATUS status;
	WIMAX_API_STATUS_REASON reason = WIMAX_API_STATUS_REASON_Normal;
	WIMAX_API_CONNECTION_PROGRESS_INFO conn_prog = 0;
	int m_status, c_status;
	int ret = 0;

	if (len < (int) sizeof(int)*2) {
		xprintf(SDK_ERR, "[%d] HCI length < %d\n", dev_idx, sizeof(int)*2);
		return sdk_set_errno(ERR_HCI);
	}

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	memcpy(&m_status, buf, sizeof(int));
	memcpy(&c_status, &buf[sizeof(int)], sizeof(int));
			
	xfunc_in("dev=%d, m_s=%d, c_s=%d", dev_idx, m_status, c_status);

	if (dev->a_status == AM_Close) {
		xprintf(SDK_DBG, "APP is in cloasing.\n");
		goto out;
	}

	ret = sdk_convert_status(dev_idx, &status, &conn_prog, m_status, c_status);

	if (!ret) {
		pthread_mutex_lock(&api_handle_lock);
		list_for_each_entry(handle, head, list) {
			if (handle->sdk->dev_open[dev_idx] && handle->sdk->ind.stat_update
				&& dm_tst_dev(dev_idx)) {
				ind_msg_t *msg;
				msg = (ind_msg_t *) sdk_malloc(sizeof(ind_msg_t));
				msg->dev_idx = dev_idx;
				msg->u.stat_update.device_status = status;
				msg->u.stat_update.status_reason = reason;
				msg->u.stat_update.progress_info = conn_prog;
				msg_send(&handle->sdk->ind.msg_cb, ind_stat_update, msg);
			}
		}
		pthread_mutex_unlock(&api_handle_lock);
	}
out:
	dm_put_dev(dev_idx);
	xfunc_out();
	return ret;
}

int sdk_ind_if_updown(int dev_idx, u8 *buf, int len)
{
	sdk_internal_t *sdk = sdk_get_rw_handle();
	device_t *dev;
	bool if_updown;
	int m_status, c_status;
	int ret = 0;

	xfunc_in("dev=%d, buf[0]=%d\n", dev_idx, *buf);

	if (len < sizeof(if_updown)) {
		xprintf(SDK_ERR, "[%d] HCI length < %d\n", dev_idx, sizeof(if_updown));
		return sdk_set_errno(ERR_HCI);
	}

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	if_updown = *buf;

	sdk_get_status(sdk, dev_idx, &m_status, &c_status);

	xprintf(SDK_INFO, "dev inserted=%d, if=%s, m_status=%d\n",
		dev->inserted, if_updown==WIMAX_IF_UP ? "up" : "down", m_status);

	if (dev->inserted && m_status == M_CONNECTED && if_updown == WIMAX_IF_DOWN) {
		net_updown(dev_idx, TRUE);
		xprintf(SDK_INFO, "Re-up network interface(%s)\n", dev->name);
	}

	dm_put_dev(dev_idx);
	xfunc_out("ret=%d", ret);
	return ret;
}

int sdk_ind_power_ctrl(sdk_internal_t *sdk, int dev_idx, bool on)
{
	WIMAX_API_RF_STATE rf_state;
	int ret = 0;

	xfunc_in("dev=%d, power=%s", dev_idx, on ? "On" : "Off");

	if (on)
		rf_state = WIMAX_API_RF_ON;
	else
		rf_state = WIMAX_API_RF_OFF;

	if (sdk->dev_open[dev_idx] && sdk->ind.pow_mng && dm_tst_dev(dev_idx)) {
		ind_msg_t *msg = (ind_msg_t *) sdk_malloc(sizeof(ind_msg_t));
		msg->dev_idx = dev_idx;
		msg->u.pow_mng.state = rf_state;
		msg_send(&sdk->ind.msg_cb, ind_pow_mng, msg);
	}

	xfunc_out();
	return ret;
}

int sdk_ind_connect_net(sdk_internal_t *sdk, int dev_idx, bool success)
{
	WIMAX_API_NETWORK_CONNECTION_RESP conn_status;
	int ret = 0;

	xfunc_in("dev=%d, connected=%d", dev_idx, success);

	if (success)
		conn_status = WIMAX_API_CONNECTION_SUCCESS;
	else
		conn_status = WIMAX_API_CONNECTION_FAILURE;

	if (sdk->dev_open[dev_idx] && sdk->ind.connect_net && dm_tst_dev(dev_idx)) {
		ind_msg_t *msg = (ind_msg_t *) sdk_malloc(sizeof(ind_msg_t));
		msg->dev_idx = dev_idx;
		msg->u.connect_net.response = conn_status;
		msg_send(&sdk->ind.msg_cb, ind_connect_net, msg);
	}

	xfunc_out();
	return ret;
}

int sdk_ind_disconnect_net(sdk_internal_t *sdk, int dev_idx, bool success)
{
	WIMAX_API_NETWORK_CONNECTION_RESP conn_status;
	int ret = 0;

	xfunc_in("dev=%d, connected=%d", dev_idx, success);

	if (success)
		conn_status = WIMAX_API_CONNECTION_SUCCESS;
	else
		conn_status = WIMAX_API_CONNECTION_FAILURE;

	if (sdk->dev_open[dev_idx] && sdk->ind.disconnect_net && dm_tst_dev(dev_idx)) {
		ind_msg_t *msg = (ind_msg_t *) sdk_malloc(sizeof(ind_msg_t));
		msg->dev_idx = dev_idx;
		msg->u.disconnect_net.response = conn_status;
		msg_send(&sdk->ind.msg_cb, ind_disconnect_net, msg);
	}

	xfunc_out();
	return ret;
}

int sdk_ind_net_search_wscan(sdk_internal_t *sdk, int dev_idx)
{
	device_t *dev;
	wimax_t *wm;
	WIMAX_API_NSP_INFO_P nsp_list;
	int ret = 0;
	u32 nsp_cnt;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;
	wm = dev->wimax;

	nsp_cnt = wm->scan_list.cnt;
	nsp_list = (WIMAX_API_NSP_INFO_P) sdk_malloc(sizeof(WIMAX_API_NSP_INFO)*nsp_cnt);

	if ((ret = get_network_list(dev_idx, nsp_list, &nsp_cnt)) < 0)
		goto out;

	if (sdk->dev_open[dev_idx] && sdk->ind.net_search_wscan && dm_tst_dev(dev_idx)) {
		ind_msg_t *msg = (ind_msg_t *) sdk_malloc(sizeof(ind_msg_t));
		msg->dev_idx = dev_idx;
		msg->u.net_search_wscan.nsp_list = nsp_list;
		msg->u.net_search_wscan.list_cnt = nsp_cnt;
		msg_send(&sdk->ind.msg_cb, ind_net_search_wscan, msg);
	}
out:
	xfunc_out();
	return ret;
}

int sdk_ind_rf_state(int dev_idx, u8 *buf, int len)
{
	rf_stat_t rf_stat = buf[0];
	fsm_event_t event;

	xfunc_in("dev=%d, rf=%s", dev_idx, rf_stat==rf_on ? "On" : "Off");

	event = (rf_stat==rf_on) ? NM_RadioOn : NM_RadioOff;
	sdk_ind_event(dev_idx, event);

	xfunc_out();
	return 0;
}

int sdk_ind_provisioning_op(int dev_idx, WIMAX_API_PROV_OPERATION operation,
	WIMAX_API_CONTACT_TYPE contact_type)
{
	struct list_head *head = hand_get_api_handle_list();
	api_hand_t *handle;

	xfunc_in("dev=%d, %d, %d", dev_idx, operation, contact_type);

	pthread_mutex_lock(&api_handle_lock);
	list_for_each_entry(handle, head, list) {
		if (handle->sdk->ind.provisioning) {
			ind_msg_t *msg;
			msg = (ind_msg_t *) sdk_malloc(sizeof(ind_msg_t));
			msg->dev_idx = dev_idx;
			msg->u.provisioning.operation = operation;
			msg->u.provisioning.contact_type = contact_type;
			msg_send(&handle->sdk->ind.msg_cb, ind_provisioning, msg);
		}
	}
	pthread_mutex_unlock(&api_handle_lock);

	xfunc_out();
	return 0;
}

static void sdk_chk_cr801(int dev_idx, GCT_API_NOTI_CATEGORY category,
						GCT_API_NOTI_TYPE type, char *buf, int len)
{
	device_t *dev;
	short code;

	if (!(dev = dm_get_dev(dev_idx)))
		return;

	xfunc_in("dev=%d, category=%d, type=%d", dev_idx, category, type);

	assert(dev->wimax->dev_info.eapp.cr801_enabled);

	if (category == GCT_API_ERROR_NOTI_EAP && type == GCT_API_NOTI_TYPE_CODE) {
		memcpy(&code, buf, sizeof(code));
		code = B2H(code);
		if (code == E_WM_CR801_EAP_FAILURE) {
			dev->wimax->dev_info.eapp.cr801_server_reject_cnt++;
			xprintf(SDK_DBG, "cr801_server_reject_cnt=%d\n",
				dev->wimax->dev_info.eapp.cr801_server_reject_cnt);
		}

		dm_put_dev(dev);
	}
	xfunc_out();
}

int sdk_ind_noti(int dev_idx, GCT_API_NOTI_CATEGORY category,
						GCT_API_NOTI_TYPE type, char *buf, int len)
{
	device_t *dev;
	struct list_head *head = hand_get_api_handle_list();
	api_hand_t *handle;
	int ret = 0;

	xfunc_in("dev=%d, %d, %d", dev_idx, category, type);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	if (E_EAP_TLS_ENABLED(dev)) {
		if (dev->wimax->dev_info.eapp.cr801_enabled) {
			xprintf(SDK_DBG, "cr801_enabled=%d\n", dev->wimax->dev_info.eapp.cr801_enabled);
			sdk_chk_cr801(dev_idx, category, type, buf, len);
		}
	}

	pthread_mutex_lock(&api_handle_lock);
	list_for_each_entry(handle, head, list) {
		if (handle->sdk->ind.notification) {
			ind_msg_t *msg;
			msg = (ind_msg_t *) sdk_malloc(sizeof(ind_msg_t));
			msg->dev_idx = dev_idx;
			msg->u.notification.category = category;
			msg->u.notification.type = type;
			if (len > sizeof(msg->u.notification.buf)) {
				xprintf(SDK_ERR, "Notification buffer is too small(%d > %d).",
					len, sizeof(msg->u.notification.buf));
				ret = -1;
				break;
			}
			assert(len < sizeof(msg->u.notification.buf));
			memcpy(msg->u.notification.buf, buf, len);
			msg->u.notification.buf[len] = 0;
			msg->u.notification.buf_len = len;
			msg_send(&handle->sdk->ind.msg_cb, ind_notification, msg);
		}
	}
	pthread_mutex_unlock(&api_handle_lock);

	xfunc_out();
	dm_put_dev(dev);
	return ret;
}

static GCT_API_POWER_MODE convert_hci_power_mode(u8 mode)
{
	GCT_API_POWER_MODE m;

	switch (mode) {
		case MODE_W_AWAKE:
			m = WiMAXPowerModeNormal;
			break;
		case MODE_W_IDLE:
			m = WiMAXPowerModeIdle;
			break;
		case MODE_W_SLEEP:
			m = WiMAXPowerModeSleep;
			break;
		default:
			m = WiMAXPowerModeMaximum;
			break;
	}

	return m;
}

int sdk_ind_power_mode_change(int dev_idx, u8 *buf, int len)
{
	struct list_head *head = hand_get_api_handle_list();
	api_hand_t *handle;
	hci_mode_change_t *hci = (hci_mode_change_t *) buf;

	xfunc_in("dev=%d", dev_idx);

	xprintf(SDK_INFO, "status=%d, power_state=%d\n",
		U82U24(hci->status), convert_hci_power_mode(hci->power_state));

	pthread_mutex_lock(&api_handle_lock);
	list_for_each_entry(handle, head, list) {
		if (handle->sdk->ind.power_mode) {
			ind_msg_t *msg;
			msg = (ind_msg_t *) sdk_malloc(sizeof(ind_msg_t));
			msg->dev_idx = dev_idx;
			msg->u.power_mode.status = U82U24(hci->status);
			msg->u.power_mode.power_mode = convert_hci_power_mode(hci->power_state);
			msg_send(&handle->sdk->ind.msg_cb, ind_mode_change, msg);
		}
	}
	pthread_mutex_unlock(&api_handle_lock);

	xfunc_out();
	return 0;
}

int sdk_reg_ind(const char *name, void_func_t *reg, void_func_t callback)
{
	if (*reg && callback) {
		xprintf(SDK_ERR, "%s(0x%08X-0x%08X) was registered already\n", name, *reg, callback);
		return -ERR_ALREADY;
	}
	else if (!*reg && !callback) {
		xprintf(SDK_ERR, "%s(NULL) was removed already\n", name);
		return -ERR_ALREADY;
	}

	*reg = callback;
	return 0;
}

int sdk_reg_recv_hci_packet(api_hand_t *api_hand, SDKIndRcvHCIPacket callback)
{
	void_func_t *reg = (void_func_t *) &api_hand->sdk->ind.recv_hci;
	if (sdk_check_handle(api_hand, NO_DEV) < 0)
		return -1;
	assert(api_hand->sdk != NULL && api_hand->sdk->struct_size == sizeof(sdk_internal_t));
	return sdk_reg_ind(STR(SDKIndRcvHCIPacket), reg, (void_func_t) callback);
}

int sdk_reg_dev_insert_remove(api_hand_t *api_hand, SDKIndDeviceInsertRemove callback)
{
	void_func_t *reg = (void_func_t *) &api_hand->sdk->ind.insert_remove;
	if (sdk_check_handle(api_hand, NO_DEV) < 0)
		return -1;
	assert(api_hand->sdk != NULL && api_hand->sdk->struct_size == sizeof(sdk_internal_t));
	return sdk_reg_ind(STR(SDKIndDeviceInsertRemove), reg, (void_func_t) callback);
}

int sdk_reg_ctrl_power_mng(api_hand_t *api_hand, SDKIndControlPowerManagement callback)
{
	void_func_t *reg = (void_func_t *) &api_hand->sdk->ind.pow_mng;
	if (sdk_check_handle(api_hand, NO_DEV) < 0)
		return -1;
	assert(api_hand->sdk != NULL && api_hand->sdk->struct_size == sizeof(sdk_internal_t));
	return sdk_reg_ind(STR(SDKIndControlPowerManagement), reg, (void_func_t) callback);
}

int sdk_reg_dev_stat_update(api_hand_t *api_hand, SDKIndDeviceStatusUpdate callback)
{
	void_func_t *reg = (void_func_t *) &api_hand->sdk->ind.stat_update;
	if (sdk_check_handle(api_hand, NO_DEV) < 0)
		return -1;
	assert(api_hand->sdk != NULL && api_hand->sdk->struct_size == sizeof(sdk_internal_t));
	return sdk_reg_ind(STR(SDKIndDeviceStatusUpdate), reg, (void_func_t) callback);
}

int sdk_reg_connect_network(api_hand_t *api_hand, SDKIndConnectToNetwork callback)
{
	void_func_t *reg = (void_func_t *) &api_hand->sdk->ind.connect_net;
	if (sdk_check_handle(api_hand, NO_DEV) < 0)
		return -1;
	assert(api_hand->sdk != NULL && api_hand->sdk->struct_size == sizeof(sdk_internal_t));
	return sdk_reg_ind(STR(SDKIndConnectToNetwork), reg, (void_func_t) callback);
}

int sdk_reg_disconnect_network(api_hand_t *api_hand,
		SDKIndDisconnectFromNetwork callback)
{
	void_func_t *reg = (void_func_t *) &api_hand->sdk->ind.disconnect_net;
	if (sdk_check_handle(api_hand, NO_DEV) < 0)
		return -1;
	assert(api_hand->sdk != NULL && api_hand->sdk->struct_size == sizeof(sdk_internal_t));
	return sdk_reg_ind(STR(SDKIndDisconnectFromNetwork), reg, (void_func_t) callback);
}

int sdk_reg_network_search_wscn(api_hand_t *api_hand,
		SDKIndNetworkSearchWideScan callback)
{
	void_func_t *reg = (void_func_t *) &api_hand->sdk->ind.net_search_wscan;
	if (sdk_check_handle(api_hand, NO_DEV) < 0)
		return -1;
	assert(api_hand->sdk != NULL && api_hand->sdk->struct_size == sizeof(sdk_internal_t));
	return sdk_reg_ind(STR(SDKIndNetworkSearchWideScan), reg, (void_func_t) callback);
}

int sdk_reg_provisioning_op(api_hand_t *api_hand, SDKIndProvisioningOperation callback)
{
	void_func_t *reg = (void_func_t *) &api_hand->sdk->ind.provisioning;
	if (sdk_check_handle(api_hand, NO_DEV) < 0)
		return -1;
	assert(api_hand->sdk != NULL && api_hand->sdk->struct_size == sizeof(sdk_internal_t));
	return sdk_reg_ind(STR(SDKIndProvisioningOperation), reg, (void_func_t) callback);
}

int sdk_reg_package_update(api_hand_t *api_hand, SDKIndPackageUpdate callback)
{
	void_func_t *reg = (void_func_t *) &api_hand->sdk->ind.package_update;
	if (sdk_check_handle(api_hand, NO_DEV) < 0)
		return -1;
	assert(api_hand->sdk != NULL && api_hand->sdk->struct_size == sizeof(sdk_internal_t));
	return sdk_reg_ind(STR(SDKIndPackageUpdate), reg, (void_func_t) callback);
}

int sdk_reg_notification(api_hand_t *api_hand, SDKIndNotification callback)
{
	void_func_t *reg = (void_func_t *) &api_hand->sdk->ind.notification;
	if (sdk_check_handle(api_hand, NO_DEV) < 0)
		return -1;
	assert(api_hand->sdk != NULL && api_hand->sdk->struct_size == sizeof(sdk_internal_t));
	return sdk_reg_ind(STR(SDKIndNotification), reg, (void_func_t) callback);
}

int sdk_reg_mode_change(api_hand_t *api_hand, SDKIndModeChange callback)
{
	void_func_t *reg = (void_func_t *) &api_hand->sdk->ind.power_mode;
	if (sdk_check_handle(api_hand, NO_DEV) < 0)
		return -1;
	assert(api_hand->sdk != NULL && api_hand->sdk->struct_size == sizeof(sdk_internal_t));
	return sdk_reg_ind(STR(SDKIndModeChange), reg, (void_func_t) callback);
}

#if defined(CONFIG_ENABLE_SERVICE_FLOW)
int sdk_reg_noti_service_flow(api_hand_t *api_hand,
				SDKIndNotiServiceFlow callback)
{
	void_func_t *reg = (void_func_t *) &api_hand->sdk->ind.noti_service_flow;
	if (sdk_check_handle(api_hand, NO_DEV) < 0)
		return -1;
	assert(api_hand->sdk != NULL && api_hand->sdk->struct_size == sizeof(sdk_internal_t));
	return sdk_reg_ind(STR(SDKIndNotiServiceFlow), reg, (void_func_t) callback);
}
#endif // CONFIG_ENABLE_SERVICE_FLOW

int sdk_read_file(const char *file, void *buf, int size)
{
	char *pbuf = (char *) buf;
	int fd, len, total = 0;

	if ((fd = open(file, O_RDONLY)) < 0) {
		xprintf(SDK_STD_ERR, "open(%s) failed\n", file);
		return -1;
	}
	while ((len = read(fd, &pbuf[total], size-total)) > 0) {
		total += len;
		if (size-total == 0)
			break;
	}

	if (len < 0)
		xprintf(SDK_STD_ERR,"read(%s) failed. ret=%d\n", file, len);

	close(fd);
	return total;
}

int sdk_write_file(const char *file, void *buf, int size, int flags)
{
	char *pbuf = (char *) buf;
	int fd, len = 0, total = 0;
	mode_t mode = 0;

	if (flags & O_CREAT)
		mode = 0644;

	if ((fd = open(file, flags, mode)) < 0) {
		xprintf(SDK_STD_ERR, "open(%s) failed\n", file);
		return -1;
	}

	if (size) {
		while ((len = write(fd, &pbuf[total], size-total)) > 0) {
			total += len;
			if (size-total == 0)
				break;
		}
	}

	if (len < 0)
		xprintf(SDK_STD_ERR, "write(%s) failed, ret=%d\n", file, len);

	close(fd);
	return total;
}
