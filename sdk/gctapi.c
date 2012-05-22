// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <assert.h>
#include <wchar.h>

#include "gctapi.h"
#include "error.h"
#include "handle.h"
#include "sdk.h"
#include "wimax.h"
#include "wm_ioctl.h"
#include "device.h"
#include "log.h"


typedef struct gct_ind_s {
	GIndRcvHCIPacket			recv_hci;
	GIndPowerModeChange			power_mode;
	GIndDeviceInsertRemove		insert_remove;
	GIndControlPowerManagement	pow_mng;
	GIndDeviceStatusUpdate		stat_update;
	GIndConnectToNetwork		connect_net;
	GIndDisconnectFromNetwork	disconnect_net;
	GIndNetworkSearchWideScan	net_search_wscan;
	GIndProvisioningOperation	provisioning;
	GIndPackageUpdate			package_update;
	GIndNotification			notification;

	#if defined(CONFIG_ENABLE_SERVICE_FLOW)
	GIndNotiServiceFlow			noti_service_flow;
	#endif // CONFIG_ENABLE_SERVICE_FLOW

} gct_ind_t;

typedef struct gct_api_s {
	u32 struct_size;
	GCT_WIMAX_API_OPEN_MODE mode;
	gct_ind_t ind;

} gct_api_t;

GCT_API_RET GAPI_CheckHandle(APIHAND hapi)
{
	api_hand_t *api = (api_hand_t *) hapi;

	if (api == NULL) {
		xprintf(SDK_ERR, "API handle is NULL\n");
		return GCT_API_RET_INVALID_PARAMETER;
	}
	if (api->struct_size != sizeof(api_hand_t)) {
		xprintf(SDK_ERR, "Wrong structure size(%d!=%d)\n",
			api->struct_size, sizeof(api_hand_t));
		return GCT_API_RET_INVALID_PARAMETER;
	}

	return GCT_API_RET_SUCCESS;
}

GCT_API_RET GAPI_Initialize(GCT_WIMAX_SDK_MODE mode, GCT_WIMAX_API_PARAM *param)
{
	if (sdk_init(mode, param) < 0)
		return GCT_API_RET_FAILED;

	xprintf(SDK_NOTICE, "SDK Version:%s\n", sdk_version());
	xprintf(SDK_INFO, "BUILT TIME: %s, %s\n", __DATE__, __TIME__);
	return GCT_API_RET_SUCCESS;
}

GCT_API_RET GAPI_DeInitialize(void)
{
	if (sdk_deinit() < 0)
		return GCT_API_RET_FAILED;
	
	return GCT_API_RET_SUCCESS;
}

GCT_API_RET GAPI_WiMaxAPIOpen(APIHAND *phAPI, GCT_WIMAX_API_OPEN_MODE mode)
{
	api_hand_t *api_hand;
	gct_api_t *gcti;
	GCT_API_RET gret = GCT_API_RET_SUCCESS;
	int sdk_mode = 0;

	if (mode & GCT_WIMAX_API_PRIVILEGE_READ_ONLY)
		sdk_mode |= sdk_read_only;
	if (mode & GCT_WIMAX_API_OPEN_MODE_ENG)
		sdk_mode |= sdk_eng_mode;

	api_hand = sdk_api_open(sdk_mode);
	if (api_hand) {
		gcti = (gct_api_t *) sdk_malloc(sizeof(gct_api_t));
		assert(gcti != NULL);
		memset(gcti, 0, sizeof(gct_api_t));
		gcti->struct_size = sizeof(gct_api_t);
		gcti->mode = mode;
		api_hand->api = gcti;
	}
	else
		gret = GCT_API_RET_FAILED;

	*phAPI = (APIHAND) api_hand;
	return gret;
}

GCT_API_RET GAPI_WiMaxAPIClose(APIHAND hAPI)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;

	gcti = (gct_api_t *) api_hand->api;

	if (sdk_api_close(api_hand) < 0)
		return GCT_API_RET_FAILED;
	else {
		sdk_free(gcti);
		return GCT_API_RET_SUCCESS;
	}
}

GCT_API_RET GAPI_SetDebugLevel(APIHAND hAPI, int level, int *prev_level)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;

	if ((ret = sdk_debug_level(api_hand, level)) < 0)
		return GCT_API_RET_FAILED;

	if (prev_level)
		*prev_level = ret;
	return GCT_API_RET_SUCCESS;
}

GCT_API_RET GAPI_PrintLog(APIHAND hAPI, int flag, const char *title, const char *fmt, ...)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	char buf[2048];
	va_list args;
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	ret = sdk_print_log(api_hand, flag, title, buf);
	va_end(args);

	if (ret < 0)
		return GCT_API_RET_FAILED;
	return GCT_API_RET_SUCCESS;
}

static void Ind_RcvHCIPacketFunc(dev_hand_t *hand,
	char *buf, int len)
{
	api_hand_t *api_hand = (api_hand_t *) hand->api;
	gct_api_t *gcti;
	GDEV_ID id;

	xfunc_in();
	id.apiHandle = api_hand;
	id.deviceIndex = hand->dev_idx;
	gcti = (gct_api_t *) api_hand->api;
	assert(gcti->ind.recv_hci != NULL);
	gcti->ind.recv_hci(&id, buf, len);
	xfunc_out();
}

GCT_API_RET GAPI_RegRcvHCIPacketFunc(APIHAND hAPI, GIndRcvHCIPacket pFunc)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;

	gcti = (gct_api_t *) api_hand->api;
	gcti->ind.recv_hci = pFunc;

	if (sdk_reg_recv_hci_packet(api_hand, Ind_RcvHCIPacketFunc) < 0)
		return GCT_API_RET_FAILED;
	else
		return GCT_API_RET_SUCCESS;
}

GCT_API_RET GAPI_DeRegRcvHCIPacketFunc(APIHAND hAPI)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;

	gcti = (gct_api_t *) api_hand->api;
	gcti->ind.recv_hci = NULL;

	if (sdk_unreg_recv_hci_packet(api_hand) < 0)
		return GCT_API_RET_FAILED;
	else
		return GCT_API_RET_SUCCESS;
}

static void Ind_PowerModeChange(dev_hand_t *hand, GCT_API_POWER_MODE nPowerMode)
{
	api_hand_t *api_hand = (api_hand_t *) hand->api;
	gct_api_t *gcti;
	GDEV_ID id;

	xfunc_in();
	id.apiHandle = api_hand;
	id.deviceIndex = hand->dev_idx;
	gcti = (gct_api_t *) api_hand->api;
	assert(gcti->ind.power_mode != NULL);
	gcti->ind.power_mode(&id, nPowerMode);
	xfunc_out();
}

GCT_API_RET GAPI_RegPowerModeChange(APIHAND hAPI, GIndPowerModeChange pFunc)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;

	gcti = (gct_api_t *) api_hand->api;
	gcti->ind.power_mode= pFunc;

	if (sdk_reg_mode_change(api_hand, Ind_PowerModeChange) < 0)
		return GCT_API_RET_FAILED;
	else
		return GCT_API_RET_SUCCESS;
}

GCT_API_RET GAPI_DeRegPowerModeChange(APIHAND hAPI)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;

	gcti = (gct_api_t *) api_hand->api;
	gcti->ind.power_mode = NULL;

	if (sdk_unreg_recv_hci_packet(api_hand) < 0)
		return GCT_API_RET_FAILED;
	else
		return GCT_API_RET_SUCCESS;
}

GCT_API_RET GAPI_WiMaxDeviceOpen(GDEV_ID_P pID)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_device_open(pID->apiHandle, pID->deviceIndex);
	if (!ret)
		return GCT_API_RET_SUCCESS;
	else
		return GCT_API_RET_FAILED;
}

GCT_API_RET GAPI_WiMaxDeviceClose(GDEV_ID_P pID)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_device_close(pID->apiHandle, pID->deviceIndex);
	if (!ret)
		return GCT_API_RET_SUCCESS;
	else
		return GCT_API_RET_FAILED;
}

GCT_API_RET GAPI_GetDeviceStatus(GDEV_ID_P pID, WIMAX_API_DEVICE_STATUS_P pDeviceStatus,
	WIMAX_API_CONNECTION_PROGRESS_INFO_P pConnectionProgressInfo)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_get_device_status(pID->apiHandle, pID->deviceIndex,
			pDeviceStatus, pConnectionProgressInfo);
	if (!ret)
		return GCT_API_RET_SUCCESS;
	else
		return GCT_API_RET_FAILED;
}

GCT_API_RET GAPI_WriteHCIPacket(GDEV_ID_P pID, char *szBuf, int nBufSize)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_write_hci_packet(pID->apiHandle, pID->deviceIndex, szBuf, nBufSize);
	if (!ret)
		return GCT_API_RET_SUCCESS;
	else
		return GCT_API_RET_FAILED;
}

GCT_API_RET GAPI_GetListDevice(APIHAND hAPI, WIMAX_API_HW_DEVICE_ID_P pHwDeviceIdList,
				UINT32* pHwDeviceIdListSize)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;

	if (*pHwDeviceIdListSize == 0)
		return GCT_API_RET_INVALID_PARAMETER;

	ret = sdk_get_device_list(hAPI, pHwDeviceIdList, pHwDeviceIdListSize);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_CmdControlPowerManagement(GDEV_ID_P pID, WIMAX_API_RF_STATE powerState)
{	
	rf_stat_t rf_stat = (rf_stat_t) powerState;
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;
	
	ret = sdk_set_power_control(pID->apiHandle, pID->deviceIndex, rf_stat);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_SetServiceProviderUnLock(GDEV_ID_P pID, WIMAX_API_WSTRING lockCode)
{
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;
	#if defined( NO_IMPLIMENT )
	#else
	gret = GCT_API_RET_NOT_IMPLEMENTED;
	#endif
	return gret;
}

GCT_API_RET GAPI_GetServiceProviderLockStatus(GDEV_ID_P pID, 
				WIMAX_API_LOCK_STATUS_P pLockStatus, UCHAR16 *NSPName)
{
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;
	#if defined( NO_IMPLIMENT )
	#else
	gret = GCT_API_RET_NOT_IMPLEMENTED;
	#endif
	wcscpy((wchar_t *)NSPName, L"NOT Implemented!");

	return gret;
}

GCT_API_RET GAPI_GetContactInformation(GDEV_ID_P pID, WIMAX_API_WSTRING nspName,
				WIMAX_API_CONTACT_INFO_P pContactInfo, 	UINT32* pSizeOfContactList)
{
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;
	#if defined( NO_IMPLIMENT )
	#else
	*pSizeOfContactList = 0;
	gret = GCT_API_RET_NOT_IMPLEMENTED;
	#endif

	return gret;
}

GCT_API_RET GAPI_GetStatistics(GDEV_ID_P pID, WIMAX_API_CONNECTION_STAT_P pStatistics)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_get_statistics(pID->apiHandle, pID->deviceIndex, pStatistics);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else {
		gret = GCT_API_RET_SUCCESS;
	}

	return gret;
}

GCT_API_RET GAPI_GetLinkStatus(GDEV_ID_P pID, WIMAX_API_LINK_STATUS_INFO_P pLinkStatus)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_get_linkstatus(pID->apiHandle, pID->deviceIndex, pLinkStatus);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else {
		gret = GCT_API_RET_SUCCESS;
	}

	return gret;
}

GCT_API_RET GAPI_GetConnectedNSP(GDEV_ID_P pID, WIMAX_API_CONNECTED_NSP_INFO_P pNspInfo)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_get_connected_nsp(pID->apiHandle, pID->deviceIndex, pNspInfo);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else {
		gret = GCT_API_RET_SUCCESS;
	}

	return gret;
}

GCT_API_RET GAPI_GetNetworkList(GDEV_ID_P pID, WIMAX_API_NSP_INFO_P pNSPInfo,
				UINT32* pListCnt)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_get_network_list(pID->apiHandle, pID->deviceIndex, pNSPInfo, pListCnt);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_CmdConnectToNetwork(GDEV_ID_P pID, WIMAX_API_WSTRING nspName,
				WIMAX_API_PROFILE_ID profileID)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;
				
	ret = sdk_connect_network(pID->apiHandle, pID->deviceIndex, nspName, profileID);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_CmdDisconnectFromNetwork(GDEV_ID_P pID)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_disconnect_network(pID->apiHandle, pID->deviceIndex);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_GetSelectProfileList(GDEV_ID_P pID,
				WIMAX_API_PROFILE_INFO_P pProfileList, UINT32* pListCnt)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_get_profile_list(pID->apiHandle, pID->deviceIndex,
			pProfileList, (int *)pListCnt);

	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_SetProfile(GDEV_ID_P pID, WIMAX_API_PROFILE_ID profileID)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_set_profile(pID->apiHandle, pID->deviceIndex, profileID);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_SetScanInterval(GDEV_ID_P pID, UINT32 intervalSec)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_set_scan_interval(pID->apiHandle, pID->deviceIndex, intervalSec);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_SetEap(GDEV_ID_P pID, GCT_API_EAP_PARAM_P eap)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_set_eap(pID->apiHandle, pID->deviceIndex, eap);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_GetDeviceInformation(GDEV_ID_P pID, WIMAX_API_DEVICE_INFO_P pDeviceInfo)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_get_device_info(pID->apiHandle, pID->deviceIndex, pDeviceInfo);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_SetFrequency(GDEV_ID_P pID, UINT32 nFreq, UINT32 nBandWidth,
	UINT32 nFFTSize)
{
	return GCT_API_RET_FAILED;
}

GCT_API_RET GAPI_GetRFInform(GDEV_ID_P pID, GCT_API_RF_INFORM_P pRFInform)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_get_rf_info(pID->apiHandle, pID->deviceIndex, pRFInform);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_GetBootloaderVersion(GDEV_ID_P pID, char *str, int size)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_get_bl_ver(pID->apiHandle, pID->deviceIndex, str, size);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_GetCapability(GDEV_ID_P pID, CAPABILITY_BIT_P cap)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_get_capability(pID->apiHandle, pID->deviceIndex, (u32 *) cap);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_SetCapability(GDEV_ID_P pID, CAPABILITY_BIT cap)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_set_capability(pID->apiHandle, pID->deviceIndex, (u32) cap);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_GetNeighborList (GDEV_ID_P pID, 
				GCT_API_NEIGHBOR_LIST_P pNeighborList, UINT32* pArrayLength)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_get_neighbor_list(pID->apiHandle, pID->deviceIndex,
			pNeighborList, (int *) pArrayLength);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_NetworkSearchScan(GDEV_ID_P pID, GCT_API_SCAN_TYPE type)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_net_search_scan(pID->apiHandle, pID->deviceIndex, type);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_CancelWideScan(GDEV_ID_P pID)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_cancel_scan(pID->apiHandle, pID->deviceIndex);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_CmdMACState(GDEV_ID_P pID, GCT_API_CMD_MAC_STATE_TYPE type)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_cmd_mac_state(pID->apiHandle, pID->deviceIndex, type);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_SetIdleModeTimeout(GDEV_ID_P pID, UINT16 timeoutSec)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_set_idle_mode_timeout(pID->apiHandle, pID->deviceIndex, timeoutSec);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_GetPHY_MAC_Basic(GDEV_ID_P pID, GCT_API_MAC_PHY_MAC_BASIC_P pData)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_get_phy_mac_basic(pID->apiHandle, pID->deviceIndex, pData);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_GetPHY_MCS(GDEV_ID_P pID, GCT_API_MAC_PHY_MCS_P pData)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_get_phy_mcs(pID->apiHandle, pID->deviceIndex, pData);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_GetPHY_CINR_RSSI(GDEV_ID_P pID, GCT_API_MAC_PHY_CINR_RSSI_P pData)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_get_phy_cinr_rssi(pID->apiHandle, pID->deviceIndex, pData);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

#if defined(CONFIG_ENABLE_SERVICE_FLOW)
GCT_API_RET GAPI_BeginSFRead(GDEV_ID_P pID)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_sf_BeginSFRead(pID->apiHandle, pID->deviceIndex);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_EndSFRead(GDEV_ID_P pID)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_sf_EndSFRead(pID->apiHandle, pID->deviceIndex);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_GetNextSF(GDEV_ID_P pID,
				WIMAX_SERVICE_FLOW *pSF,
				UINT8 Direction,
				WIMAX_SERVICE_FLOW **ppRetSF)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_sf_GetNextSF(pID->apiHandle, pID->deviceIndex, pSF, Direction, ppRetSF);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_GetServiceFlow(GDEV_ID_P pID,
				UINT32 SFID,
				WIMAX_SERVICE_FLOW **ppRetSF)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_sf_GetServiceFlow(pID->apiHandle, pID->deviceIndex, SFID, ppRetSF);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}	

GCT_API_RET GAPI_GetNextClfrRule(GDEV_ID_P pID, 
				WIMAX_SERVICE_FLOW *pSF,
				WIMAX_CLFR_RULE *pCLFRRule,
				WIMAX_CLFR_RULE **ppRetCLFRRule)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_sf_GetNextClfrRule(pID->apiHandle, pID->deviceIndex, pSF, pCLFRRule, ppRetCLFRRule);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_GetClfrRule(GDEV_ID_P pID, 
				WIMAX_SERVICE_FLOW *pSF,
				UINT16 PacketClassfierRuleIndex,
				WIMAX_CLFR_RULE **ppRetCLFRRule)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_sf_GetClfrRule(pID->apiHandle, pID->deviceIndex, pSF, PacketClassfierRuleIndex, ppRetCLFRRule);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_GetNextPHSRule(GDEV_ID_P pID, 
				WIMAX_SERVICE_FLOW *pSF,
				WIMAX_PHS_RULE *pPHSRule,
				WIMAX_PHS_RULE **ppRetPHSRule)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_sf_GetNextPHSRule(pID->apiHandle, pID->deviceIndex, pSF, pPHSRule, ppRetPHSRule);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_GetPHSRule(GDEV_ID_P pID, 
				WIMAX_SERVICE_FLOW *pSF, 
				UINT8 PHSI,
				WIMAX_PHS_RULE **ppRetPHSRule)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_sf_GetPHSRule(pID->apiHandle, pID->deviceIndex, pSF, PHSI, ppRetPHSRule);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_CmdAddSF(GDEV_ID_P pID,
				WIMAX_SF_PARAM_P pSFParam,
				WIMAX_CLFR_RULE_P pClfrRule,
				WIMAX_PHS_RULE_P pPHSRule)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_sf_CmdAddSF(pID->apiHandle, pID->deviceIndex, pSFParam, pClfrRule, pPHSRule);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;
}

GCT_API_RET GAPI_CmdChangeSF(GDEV_ID_P pID,
				WIMAX_SF_PARAM_P pSFParam,
				WIMAX_CLFR_DSC_ACTION CLFRDSCAction,
				WIMAX_CLFR_RULE_P pClfrRule,
				WIMAX_PHS_DSC_ACTION PHSDSCAction,
				WIMAX_PHS_RULE_P pPHSRule)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_sf_CmdChangeSF(pID->apiHandle, pID->deviceIndex, pSFParam, CLFRDSCAction, pClfrRule, PHSDSCAction, pPHSRule);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;

}	

GCT_API_RET GAPI_CmdDeleteSF(GDEV_ID_P pID,
				WIMAX_SF_PARAM_P pSFParam)
{
	GCT_API_RET gret;
	int ret;

	if ((gret = GAPI_CheckHandle(pID->apiHandle)) != GCT_API_RET_SUCCESS)
		return gret;

	ret = sdk_sf_CmdDeleteSF(pID->apiHandle, pID->deviceIndex, pSFParam);
	if (ret < 0)
		gret = GCT_API_RET_FAILED;
	else
		gret = GCT_API_RET_SUCCESS;

	return gret;

}
#endif // CONFIG_ENABLE_SERVICE_FLOW

static void Ind_DeviceInsertRemove(dev_hand_t *hand,
	BOOL cardPresence)
{
	api_hand_t *api_hand = (api_hand_t *) hand->api;
	gct_api_t *gcti;
	GDEV_ID id;

	xfunc_in();
	id.apiHandle = api_hand;
	id.deviceIndex = hand->dev_idx;
	gcti = (gct_api_t *) api_hand->api;
	assert(gcti->ind.insert_remove != NULL);
	gcti->ind.insert_remove(&id, cardPresence);
	xfunc_out();
}

GCT_API_RET GAPI_SubscribeDeviceInsertRemove(APIHAND hAPI,
				GIndDeviceInsertRemove pCallbackFunc)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;

	if (sdk_reg_dev_insert_remove(api_hand, Ind_DeviceInsertRemove) < 0)
		return GCT_API_RET_FAILED;
	else {
		gcti = (gct_api_t *) api_hand->api;
		gcti->ind.insert_remove = pCallbackFunc;
		return GCT_API_RET_SUCCESS;
	}
}

static void Ind_ControlPowerManagement(dev_hand_t *hand,
	WIMAX_API_RF_STATE powerState)
{
	api_hand_t *api_hand = (api_hand_t *) hand->api;
	gct_api_t *gcti;
	GDEV_ID id;

	xfunc_in();
	id.apiHandle = api_hand;
	id.deviceIndex = hand->dev_idx;
	gcti = (gct_api_t *) api_hand->api;
	gcti->ind.pow_mng(&id, powerState);
	xfunc_out();
}

GCT_API_RET GAPI_SubscribeControlPowerManagement(APIHAND hAPI,
				GIndControlPowerManagement pCallbackFunc)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;

	if (sdk_reg_ctrl_power_mng(api_hand, Ind_ControlPowerManagement) < 0)
		return GCT_API_RET_FAILED;
	else {
		gcti = (gct_api_t *) api_hand->api;
		gcti->ind.pow_mng = pCallbackFunc;
		return GCT_API_RET_SUCCESS;
	}
}

static void Ind_ConnectToNetwork(dev_hand_t *hand,
	WIMAX_API_NETWORK_CONNECTION_RESP networkConnectionResponse)
{
	api_hand_t *api_hand = (api_hand_t *) hand->api;
	gct_api_t *gcti;
	GDEV_ID id;

	xfunc_in();
	id.apiHandle = api_hand;
	id.deviceIndex = hand->dev_idx;
	gcti = (gct_api_t *) api_hand->api;
	gcti->ind.connect_net(&id, networkConnectionResponse);
	xfunc_out();
}

GCT_API_RET GAPI_SubscribeConnectToNetwork(APIHAND hAPI,
				GIndConnectToNetwork pCallbackFunc)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;

	if (sdk_reg_connect_network(api_hand, Ind_ConnectToNetwork) < 0)
		return GCT_API_RET_FAILED;
	else {
		gcti = (gct_api_t *) api_hand->api;
		gcti->ind.connect_net = pCallbackFunc;
		return GCT_API_RET_SUCCESS;
	}
}

static void Ind_DisconnectFromNetwork(dev_hand_t *hand,
	WIMAX_API_NETWORK_CONNECTION_RESP networkDisconnectResponse)
{
	api_hand_t *api_hand = (api_hand_t *) hand->api;
	gct_api_t *gcti;
	GDEV_ID id;

	xfunc_in();
	id.apiHandle = api_hand;
	id.deviceIndex = hand->dev_idx;
	gcti = (gct_api_t *) api_hand->api;
	gcti->ind.disconnect_net(&id, networkDisconnectResponse);
	xfunc_out();
}

GCT_API_RET GAPI_SubscribeDisconnectFromNetwork(APIHAND hAPI,
				GIndDisconnectFromNetwork pCallbackFunc)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;

	if (sdk_reg_disconnect_network(api_hand, Ind_DisconnectFromNetwork) < 0)
		return GCT_API_RET_FAILED;
	else {
		gcti = (gct_api_t *) api_hand->api;
		gcti->ind.disconnect_net = pCallbackFunc;
		return GCT_API_RET_SUCCESS;
	}
}

static void Ind_NetworkSearchWideScan(dev_hand_t *hand,
	WIMAX_API_NSP_INFO_P pNspInfo, UINT32 nListSize)
{
	api_hand_t *api_hand = (api_hand_t *) hand->api;
	gct_api_t *gcti;
	GDEV_ID id;

	xfunc_in();
	id.apiHandle = api_hand;
	id.deviceIndex = hand->dev_idx;
	gcti = (gct_api_t *) api_hand->api;
	gcti->ind.net_search_wscan(&id, pNspInfo, nListSize);
	xfunc_out();
}

GCT_API_RET GAPI_SubscribeNetworkSearchWideScan(APIHAND hAPI,
				GIndNetworkSearchWideScan pCallbackFunc)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;

	if (sdk_reg_network_search_wscn(api_hand, Ind_NetworkSearchWideScan) < 0)
		return GCT_API_RET_FAILED;
	else {
		gcti = (gct_api_t *) api_hand->api;
		gcti->ind.net_search_wscan = pCallbackFunc;
		return GCT_API_RET_SUCCESS;
	}
}

static void Ind_ProvisioningOperation(dev_hand_t *hand,
				WIMAX_API_PROV_OPERATION provisioning_operation,
				WIMAX_API_CONTACT_TYPE contact_type)
{
	api_hand_t *api_hand = (api_hand_t *) hand->api;
	gct_api_t *gcti;
	GDEV_ID id;

	xfunc_in();
	id.apiHandle = api_hand;
	id.deviceIndex = hand->dev_idx;
	gcti = (gct_api_t *) api_hand->api;
	gcti->ind.provisioning(&id, provisioning_operation, contact_type);
	xfunc_out();
}

GCT_API_RET GAPI_SubscribeProvisioningOperation(APIHAND hAPI,
				GIndProvisioningOperation pCallbackFunc)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;

	if (sdk_reg_provisioning_op(api_hand, Ind_ProvisioningOperation) < 0)
		return GCT_API_RET_FAILED;
	else {
		gcti = (gct_api_t *) api_hand->api;
		gcti->ind.provisioning = pCallbackFunc;
		return GCT_API_RET_SUCCESS;
	}
}

static void Ind_DeviceStatusChange(dev_hand_t *hand,
	WIMAX_API_DEVICE_STATUS deviceStatus, 
	WIMAX_API_STATUS_REASON statusReason, 
	WIMAX_API_CONNECTION_PROGRESS_INFO connectionProgressInfo)
{
	api_hand_t *api_hand = (api_hand_t *) hand->api;
	gct_api_t *gcti;
	GDEV_ID id;

	xfunc_in();
	id.apiHandle = api_hand;
	id.deviceIndex = hand->dev_idx;
	gcti = (gct_api_t *) api_hand->api;
	gcti->ind.stat_update(&id, deviceStatus, statusReason, connectionProgressInfo);
	xfunc_out();
}

static void Ind_Notification(dev_hand_t *hand,
	GCT_API_NOTI_CATEGORY nCategory,
	GCT_API_NOTI_TYPE nType, int nBufSize, char *szBuf)
{
	api_hand_t *api_hand = (api_hand_t *) hand->api;
	gct_api_t *gcti;
	GDEV_ID id;

	xfunc_in();
	id.apiHandle = api_hand;
	id.deviceIndex = hand->dev_idx;
	gcti = (gct_api_t *) api_hand->api;
	gcti->ind.notification(&id, nCategory, nType, nBufSize, szBuf);
	xfunc_out();
}

#if defined(CONFIG_ENABLE_SERVICE_FLOW)
static void Ind_NotiServiceFlow(dev_hand_t *hand, WIMAX_SF_EVENT_P pSfEvent)
{
	api_hand_t *api_hand = (api_hand_t *) hand->api;
	gct_api_t *gcti;
	GDEV_ID id;

	xfunc_in();
	id.apiHandle = api_hand;
	id.deviceIndex = hand->dev_idx;
	gcti = (gct_api_t *) api_hand->api;
	gcti->ind.noti_service_flow(&id, pSfEvent);
	xfunc_out();
}
#endif // CONFIG_ENABLE_SERVICE_FLOW

GCT_API_RET GAPI_SubscribeDeviceStatusChange(APIHAND hAPI,
				GIndDeviceStatusUpdate pCallbackFunc)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;

	if (sdk_reg_dev_stat_update(api_hand, Ind_DeviceStatusChange) < 0)
		return GCT_API_RET_FAILED;
	else {
		gcti = (gct_api_t *) api_hand->api;
		gcti->ind.stat_update = pCallbackFunc;
		return GCT_API_RET_SUCCESS;
	}
}

GCT_API_RET GAPI_SubscribeNotiFunc(APIHAND hAPI, GIndNotification pCallbackFunc)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;

	if (sdk_reg_notification(api_hand, Ind_Notification) < 0)
		return GCT_API_RET_FAILED;
	else {
		gcti = (gct_api_t *) api_hand->api;
		gcti->ind.notification = pCallbackFunc;
		return GCT_API_RET_SUCCESS;
	}
}

#if defined(CONFIG_ENABLE_SERVICE_FLOW)
GCT_API_RET GAPI_SubscribeNotiServiceFlow(APIHAND hAPI,
						GIndNotiServiceFlow pCallbackFunc)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;

	if (sdk_reg_noti_service_flow(api_hand, Ind_NotiServiceFlow) < 0)
		return GCT_API_RET_FAILED;
	else {
		gcti = (gct_api_t *) api_hand->api;
		gcti->ind.noti_service_flow = pCallbackFunc;
		return GCT_API_RET_SUCCESS;
	}
}
#endif // CONFIG_ENABLE_SERVICE_FLOW

GCT_API_RET GAPI_UnSubscribeDeviceInsertRemove(APIHAND hAPI)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;
	
	if (sdk_unreg_dev_insert_remove(api_hand) < 0)
		return GCT_API_RET_FAILED;
	else {
		gcti = (gct_api_t *) api_hand->api;
		gcti->ind.insert_remove = NULL;
		return GCT_API_RET_SUCCESS;
	}
}

GCT_API_RET GAPI_UnSubscribeControlPowerManagement(APIHAND hAPI)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;

	if (sdk_unreg_ctrl_power_mng(api_hand) < 0)
		return GCT_API_RET_FAILED;
	else {
		gcti = (gct_api_t *) api_hand->api;
		gcti->ind.pow_mng = NULL;
		return GCT_API_RET_SUCCESS;
	}
}

GCT_API_RET GAPI_UnSubscribeConnectToNetwork(APIHAND hAPI)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;

	if (sdk_unreg_connect_network(api_hand) < 0)
		return GCT_API_RET_FAILED;
	else {
		gcti = (gct_api_t *) api_hand->api;
		gcti->ind.connect_net = NULL;
		return GCT_API_RET_SUCCESS;
	}
}

GCT_API_RET GAPI_UnSubscribeDisconnectToNetwork(APIHAND hAPI)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;

	if (sdk_unreg_disconnect_network(api_hand) < 0)
		return GCT_API_RET_FAILED;
	else {
		gcti = (gct_api_t *) api_hand->api;
		gcti->ind.disconnect_net = NULL;
		return GCT_API_RET_SUCCESS;
	}
}

GCT_API_RET GAPI_UnSubscribeNetworkSearchWideScan(APIHAND hAPI)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;

	if (sdk_unreg_network_search_wscn(api_hand) < 0)
		return GCT_API_RET_FAILED;
	else {
		gcti = (gct_api_t *) api_hand->api;
		gcti->ind.net_search_wscan = NULL;
		return GCT_API_RET_SUCCESS;
	}
}

GCT_API_RET GAPI_UnSubscribeProvisioningOperation(APIHAND hAPI)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;

	if (sdk_unreg_provisioning_op(api_hand) < 0)
		return GCT_API_RET_FAILED;
	else {
		gcti = (gct_api_t *) api_hand->api;
		gcti->ind.provisioning = NULL;
		return GCT_API_RET_SUCCESS;
	}
}

GCT_API_RET GAPI_UnSubscribeDeviceStatusChange(APIHAND hAPI)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;

	if (sdk_unreg_dev_stat_update(api_hand) < 0)
		return GCT_API_RET_FAILED;
	else {
		gcti = (gct_api_t *) api_hand->api;
		gcti->ind.stat_update = NULL;
		return GCT_API_RET_SUCCESS;
	}
}

GCT_API_RET GAPI_UnSubscribeNotiFunc(APIHAND hAPI)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;

	if (sdk_unreg_notification(api_hand) < 0)
		return GCT_API_RET_FAILED;
	else {
		gcti = (gct_api_t *) api_hand->api;
		gcti->ind.notification = NULL;
		return GCT_API_RET_SUCCESS;
	}
}

#if defined(CONFIG_ENABLE_SERVICE_FLOW)
GCT_API_RET GAPI_UnSubscribeNotiServiceFlow(APIHAND hAPI)
{
	api_hand_t *api_hand = (api_hand_t *) hAPI;
	gct_api_t *gcti;
	GCT_API_RET gret;

	if ((gret = GAPI_CheckHandle(hAPI)) != GCT_API_RET_SUCCESS)
		return gret;

	if (sdk_unreg_noti_service_flow(api_hand) < 0)
		return GCT_API_RET_FAILED;
	else {
		gcti = (gct_api_t *) api_hand->api;
		gcti->ind.noti_service_flow = NULL;
		return GCT_API_RET_SUCCESS;
	}
}
#endif // CONFIG_ENABLE_SERVICE_FLOW
