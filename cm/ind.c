// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>

#include "cm.h"
#include "dhclient.h"

extern int device_open(GDEV_ID_P pID);
extern int device_close(GDEV_ID_P pID);
extern void cmd_signal_event(int dev_idx, unsigned short event, char *data, int len);
#if defined(CONFIG_DM_INTERFACE)
int send_dm_tool(void *data, int len);
int dm_ind_connetion(int connect, int success);
#endif

char *StatusStr[] = {
	"UnInitialized",
	"RF_OFF_HW_SW",
	"RF_OFF_HW",
	"RF_OFF_SW",
	"Ready",
	"Scanning",
	"Connecting",
	"Data_Connected"
};

char *ConnStatusStr[] = {
	"Ranging",
	"SBC",
	"EAP_authentication_Device",
	"EAP_authentication_User",
	"3_way_handshake",
	"Registration",
	"De_registration",
	"Registered",
	"DSX"
};

static char *StatusReasonStr[] = {
	"Normal",
	"Fail_to_connect_to_NW",
	"Fail_to_connect_Ranging",
	"Fail_to_connect_SBC",
	"Fail_to_connect_EAP_AUTH_Device",
	"Fail_to_connect_EAP_AUTH_user",
	"Fail_to_connect_3_Way_Handshake",
	"Fail_to_connect_REG",
	"Fail_to_connect_datapath"
};

static void print_disconnect_reason(u8 *data, int len)
{
	char *msg = "disconnected by unknown reason";

	/*
	Reason(1 byte) : 
	Disconnect Reason
	0x00 : disconnected by DREG
	0x01 : disconnected by MS
	0x02 : disconnected by OoZ
	0x03 : disconnected by all other reason
	*/
	switch (*data) {
		case 0x00:
			msg = "disconnected by DREG";
			break;
		case 0x01:
			msg = "disconnected by MS";
			break;
		case 0x02:
			msg = "disconnected by OoZ";
			break;
		case 0x03:
			msg = "disconnected by all other reason";
			break;
	}

	cm_printf("%s\n", msg);
}

static void ind_recv_hci_packet(GDEV_ID_P pID, char *buf, int len)
{
	unsigned short event;
	unsigned short length;
	unsigned char *data, *p;
	hci_t *hci = (hci_t *) buf;
	cm_common_conf_t *pconf = &cm_common_conf;

	#if defined(CONFIG_DM_INTERFACE)
	if (pconf->dm_interface_enable && pconf->dm_interface_cfd > 0)
		send_dm_tool(buf, len);
	#endif

	event = _B2H(hci->cmd_evt);
	length = _B2H(hci->length);
	data = hci->data;

	switch (event) {
		case WIMAX_DISCONN_IND:
			print_disconnect_reason(data, length);
			break;
		case WIMAX_IP_RENEW_IND:
			dh_stop_dhclient(pID->deviceIndex);
			dh_start_dhclient(pID->deviceIndex);
			break;
		case WIMAX_CLI_RSP:
		case WIMAX_MANAGER_MSG:
			#if 1
			data[length] = 0;
			p = &data[WIMAX_PRINT_PAD];
			cm_flag_printf("%s", p);
			#else
			length -= WIMAX_PRINT_PAD;
			data += WIMAX_PRINT_PAD;
			cm_printf("%s", data);
			//fwrite(data, 1, length, stdout);
			//fflush(stdout);
			#endif
			#if (CM_LOG_FLAG & LOG_FLAG_NO_STDOUT_IN_LOGFILE)
			if (GAPI_LOG_LEVEL_IS_FILE(pconf->log_level)) {
				const char dm_prompt[] = "DM> ";
				if (*(int *) p == *(int *) dm_prompt) {
					printf("%s", p);
					fflush(stdout);
				}
			}
			#endif
			break;
		case WIMAX_DL_IMAGE_STATUS:
		case WIMAX_UL_IMAGE_RESULT:
		case WIMAX_FILE_RESULT:
		case WIMAX_IMAGE_CMD_STATUS:
			cmd_signal_event(pID->deviceIndex, event, (char *)data, length);
			break;
	}
}

static void ind_powermode_change(GDEV_ID_P pID, GCT_API_POWER_MODE nPowerMode)
{
	char *power_mode = "Unknown";

	switch (nPowerMode) {
		case WiMAXPowerModeIdle:
			power_mode = "Idle";
			break;
		case WiMAXPowerModeSleep:
			power_mode = "Sleep";
			break;
		case WiMAXPowerModeNormal:
			power_mode = "Normal";
			break;
		case WiMAXPowerModeMaximum:
			power_mode = "Maximum";
			break;
	}

	cm_printf("device[%d] power mode: %s\n", pID->deviceIndex, power_mode);
}

static void ind_connect_network(GDEV_ID_P pID,
	WIMAX_API_NETWORK_CONNECTION_RESP networkConnectionResponse)
{
	cm_common_conf_t *pconf = &cm_common_conf;

	#if defined(CONFIG_DM_INTERFACE)
	if (pconf->dm_interface_enable && pconf->dm_interface_cfd > 0)
		dm_ind_connetion(1, networkConnectionResponse==WIMAX_API_CONNECTION_SUCCESS);
	#endif

	cm_printf("device[%d] connection %s\n", pID->deviceIndex,
		networkConnectionResponse==WIMAX_API_CONNECTION_SUCCESS
		? "SUCCESS" : "FAIL");

	if (pconf->api_mode != GCT_WIMAX_API_PRIVILEGE_READ_ONLY) {
		if (networkConnectionResponse==WIMAX_API_CONNECTION_SUCCESS)
			dh_start_dhclient(pID->deviceIndex);
		#if defined(CONFIG_DM_INTERFACE)
		else if (pconf->auto_connect_enable)
			cm_retry_auto_connection(pID->deviceIndex);
		#endif
	}
}

static void ind_dev_disconnect_network(GDEV_ID_P pID,
	WIMAX_API_NETWORK_CONNECTION_RESP networkDisconnectResponse)
{
	cm_common_conf_t *pconf = &cm_common_conf;

	#if defined(CONFIG_DM_INTERFACE)
	if (pconf->dm_interface_enable && pconf->dm_interface_cfd > 0)
		dm_ind_connetion(0, networkDisconnectResponse==WIMAX_API_CONNECTION_SUCCESS);
	#endif

	cm_printf("device[%d] disconnection %s\n", pID->deviceIndex,
		networkDisconnectResponse==WIMAX_API_CONNECTION_SUCCESS
		? "SUCCESS" : "FAIL");
	if (networkDisconnectResponse==WIMAX_API_CONNECTION_SUCCESS)
		dh_stop_dhclient(pID->deviceIndex);

	if (pconf->api_mode != GCT_WIMAX_API_PRIVILEGE_READ_ONLY) {
		if (pconf->auto_connect_enable)
			cm_request_auto_connection(pID->deviceIndex);
	}
}

static void ind_dev_network_search_widescan(GDEV_ID_P pID,
	WIMAX_API_NSP_INFO_P pNspList, UINT32 listSize)
{
	int i;

	cm_printf("[NSP-Name] [NSP-ID] [RSSI] [CINR]\n");
	for (i = 0; i < listSize; i++) {
		cm_printf("%9S   %06X %6d %6d\n",
			(wchar_t *) pNspList[i].NSPName,
			(int) pNspList[i].NSPid,
			(int) pNspList[i].RSSI-123,
			(int) pNspList[i].CINR-10);
	}
}

static void ind_dev_provisioning_operation(GDEV_ID_P pID, 
				WIMAX_API_PROV_OPERATION provisioningOperation,
				WIMAX_API_CONTACT_TYPE contactType)
{
	char *op = NULL;
	switch (provisioningOperation) {
		case WIMAX_API_PROV_OPERATION_CFG_UPDATE_STARTED:
			op = "STARTED";
			break;
		case WIMAX_API_PROV_OPERATION_CFG_UPDATE_COMPLETED:
			op = "COMPLETED";
			break;
		case WIMAX_API_PROV_OPERATION_CFG_UPDATE_FAILED_NETWORK_DISCONNECT:
			op = "FAILED_NETWORK_DISCONNECT";
			break;
		case WIMAX_API_PROV_OPERATION_CFG_UPDATE_FAILED:
			op = "FAILED";
			break;
		case WIMAX_API_PROV_OPERATION_CFG_UPDATE_FAILED_INVALID_PROVISIONING:
			op = "FAILED_INVALID_PROVISIONING";
			break;
		case WIMAX_API_PROV_OPERATION_CFG_UPDATE_FAILED_BAD_AUTHENTICATION:
			op = "FAILED_BAD_AUTHENTICATION";
			break;
		case WIMAX_API_PROV_OPERATION_CFG_UPDATE_REQUEST_INITIAL_PROVISIONING:
			op = "REQUEST_INITIAL_PROVISIONING";
			break;
		case WIMAX_API_PROV_OPERATION_CFG_UPDATE_REQUEST_ONGOING_PROVISIONING:
			op = "REQUEST_ONGOING_PROVISIONING";
			break;
		case WIMAX_API_PROV_OPERATION_CFG_UPDATE_REQUEST_RESET_PROVISIONING:
			op = "REQUEST_RESET_PROVISIONING";
			break;
		case WIMAX_API_PROV_OPERATION_CFG_UPDATE_TRIGGER_CONTACT:
			op = "TRIGGER_CONTACT";
			break;
	}

	if (op)
		cm_printf("Provisioning:%s, contact-type=%d\n", op, contactType);
	else
		cm_printf("Provisioning:Unknown(%d), contact-type=%d\n", provisioningOperation, contactType);
}

static void ind_power_mng(
	GDEV_ID_P pID,
	WIMAX_API_RF_STATE powerState)
{
	cm_printf("device[%d] Power %s\n", pID->deviceIndex,
		powerState==WIMAX_API_RF_ON ? "ON" : "OFF");
}

static void ind_dev_insert_remove(GDEV_ID_P pID, BOOL cardPresence)
{
	cm_printf("device[%d] %s\n", pID->deviceIndex,
		cardPresence ? "INSERT" : "REMOVE");

	if (cardPresence)
		device_open(pID);
	else
		device_close(pID);
}

static void ind_dev_status(GDEV_ID_P pID,
	WIMAX_API_DEVICE_STATUS deviceStatus, WIMAX_API_STATUS_REASON statusReason, 
	WIMAX_API_CONNECTION_PROGRESS_INFO connectionProgressInfo)
{
	dev_conf_t *dconf = &cm_dev_conf[pID->deviceIndex];

	dconf->wimax_status = deviceStatus;

	if (deviceStatus > WIMAX_API_DEVICE_STATUS_Data_Connected) {
		cm_printf("device[%d] Unknown device status[%d]\n",
			pID->deviceIndex, deviceStatus);
		return;
	}

	cm_printf("device[%d] status [%s]\n", pID->deviceIndex, StatusStr[deviceStatus]);

	if (dconf->ip_acq_timed && deviceStatus != WIMAX_API_DEVICE_STATUS_Data_Connected) {
		cm_stop_timer(&dconf->ip_acq_timer);
		dconf->ip_acq_timed = 0;
	}

	if (deviceStatus == WIMAX_API_DEVICE_STATUS_Connecting) {
		if (connectionProgressInfo <= WIMAX_API_DEVICE_CONNECTION_PROGRESS_Registration_DSX) {
			cm_printf("device[%d] Connection Progress Info [%s]\n", 
				pID->deviceIndex, ConnStatusStr[connectionProgressInfo]);
		} else {
			cm_printf("device[%d] Unknown connection Progress Info [%d]\n",
				pID->deviceIndex, connectionProgressInfo);
			return;
		}
	}

	if (statusReason != WIMAX_API_STATUS_REASON_Normal) {
		if (statusReason <= WIMAX_API_STATUS_REASON_Fail_to_connect_datapath) {
			cm_printf("device[%d] Status Reason [%s]\n",
				pID->deviceIndex, StatusReasonStr[statusReason]);
		} else {
			cm_printf("device[%d] Unknown status Reason [%d]\n", 
				pID->deviceIndex, statusReason);
		}
	}
}

#if defined(CONFIG_ENABLE_SERVICE_FLOW)
static void ind_noti_service_flow(GDEV_ID_P pID, WIMAX_SF_EVENT_P pSfEvent)
{
	cm_printf("device[%d] WIMAX_SF_EVENT_TYPE[%d], CC=[%d]\n", pID->deviceIndex, pSfEvent->code, pSfEvent->sf.cc);
}
#endif // CONFIG_ENABLE_SERVICE_FLOW

#define E_WM_CR801_EAP_FAILURE		-201
#define E_WM_UNKNOWN_SERVER_REALM	-202

static void eap_get_error_string(int err_code, char *err_str)
{
	switch (err_code) {
		case 0:
			strcpy(err_str, "Success.");
			break;
		case -1:
			strcpy(err_str, "CA Certification File Load Failed");
			break;
		case -2:
			strcpy(err_str, "Client Cert. or Private Key File Load Failed");
			break;
		case -3:
			strcpy(err_str, "Mutual Authentication Failed");
			break;
		case -4:
			strcpy(err_str, "Phase 2 Authentication Failed");
			break;
		case -5:
			strcpy(err_str, "Insufficient Memory");
			break;
		case -6:
			strcpy(err_str, "Not Supported Functionality");
			break;
		case -7:
			strcpy(err_str, "Invalid Parameter Setting");
			break;
		case -8:
			strcpy(err_str, "EAP Configuration was not setted.");
			break;
		case -9:
			strcpy(err_str, "Internal Callback Function failed.");
			break;
		case -10:
			strcpy(err_str, "TLS Initialization failed.");
			break;
		case -11:
			strcpy(err_str, "Internal Static Buffer is too small.");
			break;
		case -12:
			strcpy(err_str, "Getting the MSK was failed.");
			break;
		case -13:
			strcpy(err_str, "There's no more date from TLSDealEAPPayload().");
			break;
		case -14:
			strcpy(err_str, "Invalid EAP Packet Frame.");
			break;
		case -15:
			strcpy(err_str, "Invalid TLS from server.");
			break;
		case -16:
			strcpy(err_str, "Invalid TLS to server.");
			break;
		case -17:
			strcpy(err_str, "There's no certification file.");
			break;
		case -18:
			strcpy(err_str, "CTX Allocation failed.");
			break;
		case -19:
			strcpy(err_str, "Loading Certification File in TLSGetCertInfo()");
			break;
		case -20:
			strcpy(err_str, "p_szRtnBuf, p_nRtnBufSize is NULL.");
			break;
		case -21:
			strcpy(err_str, "Writing the P12 File is failed in TLSConvertP12().");
			break;
		case -22:
			strcpy(err_str, "Invalid Password for P12 File in TLSConvertP12().");
			break;
		case -23:
			strcpy(err_str, "There's no ID (InnerNAI) for EAP-Response_Identity.");
			break;
		case -24:
			strcpy(err_str, "Invalid AVP in TTLS-MSCHAPv2.");
			break;
		case -25:
			strcpy(err_str, "Received Invalid AVP from AAA in TTLS-MSCHAPv2.");
			break;
		case -26:
			strcpy(err_str, "Internal Buffer Pointer is NULL.");
			break;
		case -27:
			strcpy(err_str, "There's no or invalid 'S=' Value in MSCHAPv2.");
			break;
		case -28:
			strcpy(err_str, "Invalid Identifier in MSCHAPv2.");
			break;
		case -29:
			strcpy(err_str, "There's no SSL Context.");
			break;
		case -30:
			strcpy(err_str, "TLS Record Length is over than internal buffer size.");
			break;
		case -31:
			strcpy(err_str, "There's misorder in TTLS's internal authentication.");
			break;
		case E_WM_CR801_EAP_FAILURE:
			strcpy(err_str, "Receive failure packet.");
			break;
		case E_WM_UNKNOWN_SERVER_REALM:
			strcpy(err_str, "Unknown server realm");
			break;
		case 1:
			strcpy(err_str, "ID not found");
			break;
		case 2:
			strcpy(err_str, "Password not found");
			break;
		case 3:
			strcpy(err_str, "CA certificate error");
			break;
		case 4:
			strcpy(err_str, "Private certificate error");
			break;
		case 5:
			strcpy(err_str, "Server rejected");
			break;
		default:
			strcpy(err_str, "Unknown Error.");
			break;
	}
}

static void odm_get_error_string(int err_code, char *err_str)
{
	static const char *odm_err_str[] = {
		"NONE",
		"INVALID_BOOTSTRAP_MESSAGE",
		"CANNOT_CONNECT_ODM_SERVER",
		"HTTP_ERROR",
		"HTTP_RESPONSE_TIMEOUT",
		"STATUS_ERROR",
		"DNS_QUERY_FAIL",
		"CANNOT_CONNECT_WIB_SERVER"
	};

	if (_numof_array(odm_err_str) < err_code)
		cm_printf("OMA-DM invalid error code=%d\n", err_code);
	else
		strcpy(err_str, odm_err_str[err_code]);
}

void ind_notification(GDEV_ID_P pID, GCT_API_NOTI_CATEGORY category,
				GCT_API_NOTI_TYPE type, int buf_size, char *buf)
{
	char string[256];
	char *title = "Unknown";
	short code = 0;
	int odm_err = 0;

	switch (category) {
		case GCT_API_ERROR_NOTI_EAP:
			title = "EAP Error";
			if (type == GCT_API_NOTI_TYPE_CODE) {
				memcpy(&code, buf, sizeof(short));
				if (CAP_EEAP_ENABLED(pID->deviceIndex))
					code = _B2H(code);
				eap_get_error_string(code, string);
				cm_printf("\t%s: %s\n", title, string);
				if (code == E_WM_UNKNOWN_SERVER_REALM)
					cm_printf("\tserver realm: %s\n", &buf[sizeof(short)]);
			}
			break;
		case GCT_API_NOTI_EAP:
			title = "EAP Notification";
			break;
		case GCT_API_NOTI_ODM_NOTI:
			title = "ODM Notification";
			if (type == GCT_API_NOTI_TYPE_CODE) {
				code = buf[0];
				switch (code) {
					case GCT_API_ODM_NOTI_EXEC_REGISTRATION_PAGE_OPEN:
						cm_printf("\t%s EXEC_REGISTRATION_PAGE_OPEN: %s\n", title, &buf[1]);
						break;
					case GCT_API_ODM_NOTI_CHANGE_ACTIVATED:
						cm_printf("\t%s CHANGE_ACTIVATED\n", title);
						break;
					default:
						cm_printf("\t%s: code=%d\n", title, code);
						break;
				}
			}
			break;
		case GCT_API_NOTI_ODM_ERROR:
			memcpy(&odm_err, buf, sizeof(odm_err));
			odm_get_error_string(odm_err, string);
			cm_printf("OMA-DM Error: %s\n", string);
			break;
		default:
			title = "Unknown";
			break;
	}

	if (type == GCT_API_NOTI_TYPE_TEXT) 
		cm_printf("[%s]\n%s", title, buf);
}

void reg_indications(APIHAND *api_handle)
{
	GCT_API_RET ret;

	ret = GAPI_RegRcvHCIPacketFunc(api_handle, ind_recv_hci_packet);
	if (ret != GCT_API_RET_SUCCESS)
		cm_eprintf("GAPI_RegRcvHCIPacketFunc error=%d\n", ret);

	ret = GAPI_RegPowerModeChange(api_handle, ind_powermode_change);
	if (ret != GCT_API_RET_SUCCESS)
		cm_eprintf("GAPI_RegPowerModeChange error=%d\n", ret);

	ret = GAPI_SubscribeDeviceInsertRemove(api_handle, ind_dev_insert_remove);
	if (ret != GCT_API_RET_SUCCESS)
		cm_eprintf("GAPI_SubscribeDeviceInsertRemove error=%d\n", ret);
	
	ret = GAPI_SubscribeControlPowerManagement(api_handle, ind_power_mng);
	if (ret != GCT_API_RET_SUCCESS)
		cm_eprintf("GAPI_SubscribeControlPowerManagement error=%d\n", ret);

	ret = GAPI_SubscribeConnectToNetwork(api_handle, ind_connect_network);
	if (ret != GCT_API_RET_SUCCESS)
		cm_eprintf("GAPI_SubscribeConnectToNetwork error=%d\n", ret);
	
	ret = GAPI_SubscribeDisconnectFromNetwork(api_handle, ind_dev_disconnect_network);
	if (ret != GCT_API_RET_SUCCESS)
		cm_eprintf("GAPI_SubscribeDisconnectFromNetwork error=%d\n", ret);
	
	ret = GAPI_SubscribeNetworkSearchWideScan(api_handle, ind_dev_network_search_widescan);
	if (ret != GCT_API_RET_SUCCESS)
		cm_eprintf("GAPI_SubscribeNetworkSearchWideScan error=%d\n", ret);

	ret = GAPI_SubscribeProvisioningOperation(api_handle, ind_dev_provisioning_operation);
	if (ret != GCT_API_RET_SUCCESS)
		cm_eprintf("GAPI_SubscribeProvisioningOperation error=%d\n", ret);
	
	ret = GAPI_SubscribeDeviceStatusChange(api_handle, ind_dev_status);
	if (ret != GCT_API_RET_SUCCESS)
		cm_eprintf("GAPI_SubscribeDeviceStatusChange error=%d\n", ret);
	
	ret = GAPI_SubscribeNotiFunc(api_handle, ind_notification);
	if (ret != GCT_API_RET_SUCCESS)
		cm_eprintf("GAPI_SubscribeNotiFunc error=%d\n", ret);

	#if defined(CONFIG_ENABLE_SERVICE_FLOW)
	ret = GAPI_SubscribeNotiServiceFlow(api_handle, ind_noti_service_flow);
	if (ret != GCT_API_RET_SUCCESS)
		cm_eprintf("GAPI_SubscribeNotiFunc error=%d\n", ret);
	#endif // CONFIG_ENABLE_SERVICE_FLOW
	
}
