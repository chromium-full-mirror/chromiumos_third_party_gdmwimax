// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef	__GCT_API_H__
#define	__GCT_API_H__

#include "gcttype.h"

//
// GCT APIs
//
GCT_API_RET GAPI_Initialize(GCT_WIMAX_SDK_MODE mode, GCT_WIMAX_API_PARAM *param);
GCT_API_RET GAPI_DeInitialize(void);
GCT_API_RET GAPI_WiMaxAPIOpen(APIHAND *phAPI, GCT_WIMAX_API_OPEN_MODE mode);
GCT_API_RET GAPI_WiMaxAPIClose(APIHAND hAPI);
GCT_API_RET GAPI_SetDebugLevel(APIHAND hAPI, int level, int *prev_level);
#define LOG_FLAG_NO_STDOUT_IN_LOGFILE	(1<<0)
GCT_API_RET GAPI_PrintLog(APIHAND hAPI, int flag, const char *title, const char *fmt, ...);
GCT_API_RET GAPI_RegRcvHCIPacketFunc(APIHAND hAPI, GIndRcvHCIPacket pFunc);
GCT_API_RET GAPI_DeRegRcvHCIPacketFunc(APIHAND hAPI);
GCT_API_RET GAPI_RegPowerModeChange(APIHAND hAPI, GIndPowerModeChange pFunc);
GCT_API_RET GAPI_DeRegPowerModeChange(APIHAND hAPI);
GCT_API_RET GAPI_WiMaxDeviceOpen(GDEV_ID_P pID);
GCT_API_RET GAPI_WiMaxDeviceClose(GDEV_ID_P pID);
GCT_API_RET GAPI_GetDeviceStatus(GDEV_ID_P pID, WIMAX_API_DEVICE_STATUS_P pDeviceStatus,
				WIMAX_API_CONNECTION_PROGRESS_INFO_P pConnectionProgressInfo);
GCT_API_RET GAPI_WriteHCIPacket(GDEV_ID_P pID, char *szBuf, int nBufSize);
GCT_API_RET GAPI_GetListDevice(APIHAND hAPI, WIMAX_API_HW_DEVICE_ID_P pHwDeviceIdList,
				UINT32* pHwDeviceIdListSize);
GCT_API_RET GAPI_CmdControlPowerManagement(GDEV_ID_P pID, WIMAX_API_RF_STATE powerState);
GCT_API_RET GAPI_SetServiceProviderUnLock(GDEV_ID_P pID, WIMAX_API_WSTRING lockCode);
GCT_API_RET GAPI_GetServiceProviderLockStatus(GDEV_ID_P pID, 
				WIMAX_API_LOCK_STATUS_P pLockStatus, UCHAR16 *NSPName);
GCT_API_RET GAPI_GetContactInformation(GDEV_ID_P pID, WIMAX_API_WSTRING nspName,
				WIMAX_API_CONTACT_INFO_P pContactInfo, 	UINT32* pSizeOfContactList);
GCT_API_RET GAPI_GetStatistics(GDEV_ID_P pID, WIMAX_API_CONNECTION_STAT_P pStatistics);
GCT_API_RET GAPI_GetLinkStatus(GDEV_ID_P pID, WIMAX_API_LINK_STATUS_INFO_P pLinkStatus);
GCT_API_RET GAPI_GetConnectedNSP(GDEV_ID_P pID, WIMAX_API_CONNECTED_NSP_INFO_P pNspInfo);
GCT_API_RET GAPI_GetNetworkList(GDEV_ID_P pID, WIMAX_API_NSP_INFO_P pNSPInfo,
				UINT32* pListCnt);
GCT_API_RET GAPI_CmdConnectToNetwork(GDEV_ID_P pID, WIMAX_API_WSTRING nspName,
				WIMAX_API_PROFILE_ID profileID);
GCT_API_RET GAPI_CmdDisconnectFromNetwork(GDEV_ID_P pID);
GCT_API_RET GAPI_GetSelectProfileList(GDEV_ID_P pID,
				WIMAX_API_PROFILE_INFO_P pProfileList, UINT32* pListCnt);
GCT_API_RET GAPI_SetProfile(GDEV_ID_P pID, WIMAX_API_PROFILE_ID profileID);
GCT_API_RET GAPI_SetScanInterval(GDEV_ID_P pID, UINT32 intervalSec);
GCT_API_RET GAPI_SetEap(GDEV_ID_P pID, GCT_API_EAP_PARAM_P eap);
GCT_API_RET GAPI_GetDeviceInformation(GDEV_ID_P pID, WIMAX_API_DEVICE_INFO_P pDeviceInfo);
GCT_API_RET GAPI_SetFrequency(GDEV_ID_P pID, UINT32 nFreq, UINT32 nBandWidth,
				UINT32 nFFTSize);
GCT_API_RET GAPI_GetRFInform(GDEV_ID_P pID, GCT_API_RF_INFORM_P pRFInform);
GCT_API_RET GAPI_GetBootloaderVersion(GDEV_ID_P pID, char *str, int size);
GCT_API_RET GAPI_GetCapability(GDEV_ID_P pID, CAPABILITY_BIT_P cap);
GCT_API_RET GAPI_SetCapability(GDEV_ID_P pID, CAPABILITY_BIT cap);
GCT_API_RET GAPI_GetNeighborList (GDEV_ID_P pID, 
				GCT_API_NEIGHBOR_LIST_P pNeighborList, UINT32* pArrayLength);
GCT_API_RET GAPI_NetworkSearchScan(GDEV_ID_P pID, GCT_API_SCAN_TYPE type);
#define GAPI_NetworkSearchWideScan(pID)	GAPI_NetworkSearchScan(pID, GCT_API_SCAN_WIDE)
GCT_API_RET GAPI_CancelWideScan(GDEV_ID_P pID);
GCT_API_RET GAPI_CmdMACState(GDEV_ID_P pID, GCT_API_CMD_MAC_STATE_TYPE type);
GCT_API_RET GAPI_SetIdleModeTimeout(GDEV_ID_P pID, UINT16 timeoutSec);
GCT_API_RET GAPI_GetPHY_MAC_Basic(GDEV_ID_P pID, GCT_API_MAC_PHY_MAC_BASIC_P pData);
GCT_API_RET GAPI_GetPHY_MCS(GDEV_ID_P pID, GCT_API_MAC_PHY_MCS_P pData);
GCT_API_RET GAPI_GetPHY_CINR_RSSI(GDEV_ID_P pID, GCT_API_MAC_PHY_CINR_RSSI_P pData);

#if defined(CONFIG_ENABLE_SERVICE_FLOW)
GCT_API_RET GAPI_BeginSFRead(GDEV_ID_P pID);
GCT_API_RET GAPI_EndSFRead(GDEV_ID_P pID);
GCT_API_RET GAPI_GetNextSF(GDEV_ID_P pID,
				WIMAX_SERVICE_FLOW *pSF,
				UINT8 Direction,
				WIMAX_SERVICE_FLOW **ppRetSF);
GCT_API_RET GAPI_GetServiceFlow(GDEV_ID_P pID,
				UINT32 SFID,
				WIMAX_SERVICE_FLOW **ppRetSF);
GCT_API_RET GAPI_GetNextClfrRule(GDEV_ID_P pID, 
				WIMAX_SERVICE_FLOW *pSF,
				WIMAX_CLFR_RULE *pCLFRRule,
				WIMAX_CLFR_RULE **ppRetCLFRRule);
GCT_API_RET GAPI_GetClfrRule(GDEV_ID_P pID, 
				WIMAX_SERVICE_FLOW *pSF,
				UINT16 PacketClassfierRuleIndex,
				WIMAX_CLFR_RULE **ppRetCLFRRule);
GCT_API_RET GAPI_GetNextPHSRule(GDEV_ID_P pID, 
				WIMAX_SERVICE_FLOW *pSF,
				WIMAX_PHS_RULE *pPHSRule,
				WIMAX_PHS_RULE **ppRetPHSRule);
GCT_API_RET GAPI_GetPHSRule(GDEV_ID_P pID, 
				WIMAX_SERVICE_FLOW *pSF, 
				UINT8 PHSI,
				WIMAX_PHS_RULE **ppRetPHSRule);
GCT_API_RET GAPI_CmdAddSF(GDEV_ID_P pID,
				WIMAX_SF_PARAM_P pSFParam,
				WIMAX_CLFR_RULE_P pClfrRule,
				WIMAX_PHS_RULE_P pPHSRule);
GCT_API_RET GAPI_CmdChangeSF(GDEV_ID_P pID,
				WIMAX_SF_PARAM_P pSFParam,
				WIMAX_CLFR_DSC_ACTION CLFRDSCAction,
				WIMAX_CLFR_RULE_P pClfrRule,
				WIMAX_PHS_DSC_ACTION PHSDSCAction,
				WIMAX_PHS_RULE_P pPHSRule);
GCT_API_RET GAPI_CmdDeleteSF(GDEV_ID_P pID,
				WIMAX_SF_PARAM_P pSFParam);
#endif // CONFIG_ENABLE_SERVICE_FLOW

GCT_API_RET GAPI_SubscribeDeviceInsertRemove(APIHAND hAPI,
						GIndDeviceInsertRemove pCallbackFunc);
GCT_API_RET GAPI_SubscribeControlPowerManagement(APIHAND hAPI,
						GIndControlPowerManagement pCallbackFunc);
GCT_API_RET GAPI_SubscribeConnectToNetwork(APIHAND hAPI,
						GIndConnectToNetwork pCallbackFunc);
GCT_API_RET GAPI_SubscribeDisconnectFromNetwork(APIHAND hAPI,
						GIndDisconnectFromNetwork pCallbackFunc);
GCT_API_RET GAPI_SubscribeNetworkSearchWideScan(APIHAND hAPI,
						GIndNetworkSearchWideScan pCallbackFunc);
GCT_API_RET GAPI_SubscribeProvisioningOperation(APIHAND hAPI,
						GIndProvisioningOperation pCallbackFunc);
GCT_API_RET GAPI_SubscribeDeviceStatusChange(APIHAND hAPI,
						GIndDeviceStatusUpdate pCallbackFunc);
GCT_API_RET GAPI_SubscribeNotiFunc(APIHAND hAPI,
						GIndNotification pCallbackFunc);
#if defined(CONFIG_ENABLE_SERVICE_FLOW)
GCT_API_RET GAPI_SubscribeNotiServiceFlow(APIHAND hAPI,
						GIndNotiServiceFlow pCallbackFunc);
#endif // CONFIG_ENABLE_SERVICE_FLOW

GCT_API_RET GAPI_UnSubscribeDeviceInsertRemove(APIHAND hAPI);
GCT_API_RET GAPI_UnSubscribeControlPowerManagement(APIHAND hAPI);
GCT_API_RET GAPI_UnSubscribeConnectToNetwork(APIHAND hAPI);
GCT_API_RET GAPI_UnSubscribeDisconnectToNetwork(APIHAND hAPI);
GCT_API_RET GAPI_UnSubscribeNetworkSearchWideScan(APIHAND hAPI);
GCT_API_RET GAPI_UnSubscribeProvisioningOperation(APIHAND hAPI);
GCT_API_RET GAPI_UnSubscribeDeviceStatusChange(APIHAND hAPI);
GCT_API_RET GAPI_UnSubscribeNotiFunc(APIHAND hAPI);
#if defined(CONFIG_ENABLE_SERVICE_FLOW)
GCT_API_RET GAPI_UnSubscribeNotiServiceFlow(APIHAND hAPI);
#endif // CONFIG_ENABLE_SERVICE_FLOW

#endif
