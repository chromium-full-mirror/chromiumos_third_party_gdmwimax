// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(SDK_H_20080715)
#define SDK_H_20080715
#include "global.h"
#include "gcttype.h"
#include "hci.h"
#include "handle.h"
#include "msg.h"
#include "msg_thread.h"

#define SDK_VERSION	"1.6.8"

#if !defined(MAX_DEVICE)
#define MAX_DEVICE 256
#endif
#define NO_DEV	0

#define SDK_RF_UPDOWN_RESP_TIMEOUT_SEC		5
#define SDK_DISCONN_RESP_TIMEOUT_SEC		5

typedef void (*SDKIndRcvHCIPacket) (dev_hand_t *hand, char *buf, int len);
typedef void (*SDKIndDeviceInsertRemove)(dev_hand_t *hand, BOOL presence);
typedef void (*SDKIndControlPowerManagement)(dev_hand_t *hand,
				WIMAX_API_RF_STATE power_state);
typedef void (*SDKIndDeviceStatusUpdate)(dev_hand_t *hand,
				WIMAX_API_DEVICE_STATUS device_status, 
				WIMAX_API_STATUS_REASON status_reason, 
				WIMAX_API_CONNECTION_PROGRESS_INFO progress_info);
typedef void (*SDKIndConnectToNetwork) (dev_hand_t *hand,
				WIMAX_API_NETWORK_CONNECTION_RESP connection_response);
typedef void (*SDKIndDisconnectFromNetwork) (dev_hand_t *hand,
				WIMAX_API_NETWORK_CONNECTION_RESP disconnect_response);
typedef void (*SDKIndNetworkSearchWideScan) (dev_hand_t *hand,
				WIMAX_API_NSP_INFO_P nsp_list, UINT32 list_cnt);
typedef void (*SDKIndProvisioningOperation) (dev_hand_t *hand,
				WIMAX_API_PROV_OPERATION provisioning_operation,
				WIMAX_API_CONTACT_TYPE contact_type);
typedef void (*SDKIndPackageUpdate) (dev_hand_t *hand,
				WIMAX_API_PACK_UPDATE package_update);
typedef void (*SDKIndNotification) (dev_hand_t *hand,
				GCT_API_NOTI_CATEGORY category,
				GCT_API_NOTI_TYPE type, int buf_size, char *buf);
typedef void (*SDKIndModeChange) (dev_hand_t *hand,
				GCT_API_POWER_MODE power_mode);

#if defined(CONFIG_ENABLE_SERVICE_FLOW)
typedef void (*SDKIndNotiServiceFlow)(dev_hand_t *hand, WIMAX_SF_EVENT_P pSfEvent);
#endif // CONFIG_ENABLE_SERVICE_FLOW

typedef struct sdk_ind_s {
	pthread_t						thread;
	msg_cb_t						msg_cb;

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

	#if defined(CONFIG_ENABLE_SERVICE_FLOW)
	SDKIndNotiServiceFlow			noti_service_flow;
	#endif // CONFIG_ENABLE_SERVICE_FLOW
} sdk_ind_t;

typedef enum {
	ind_recv_hci,
	ind_insert_remove,
	ind_pow_mng,
	ind_stat_update,
	ind_connect_net,
	ind_disconnect_net,
	ind_net_search_wscan,
	ind_provisioning,
	ind_package_update,
	ind_notification,
	ind_mode_change,

	ind_end

} ind_type_t;

typedef struct {
	char *buf;
	int len;

} ind_recv_hci_t;

typedef struct {
	bool presence;

} ind_insert_remove_t;

typedef struct {
	WIMAX_API_RF_STATE state;

} ind_pow_mng_t;

typedef struct {
	WIMAX_API_DEVICE_STATUS device_status;
	WIMAX_API_STATUS_REASON status_reason;
	WIMAX_API_CONNECTION_PROGRESS_INFO progress_info;

} ind_stat_update_t;

typedef struct {
	WIMAX_API_NETWORK_CONNECTION_RESP response;

} ind_connect_net_t;

typedef struct {
	WIMAX_API_NETWORK_CONNECTION_RESP response;

} ind_disconnect_net_t;

typedef struct {
	WIMAX_API_NSP_INFO_P nsp_list;
	UINT32 list_cnt;

} ind_net_search_wscan_t;

typedef struct {
	WIMAX_API_PROV_OPERATION operation;
	WIMAX_API_CONTACT_TYPE contact_type;

} ind_provisioning_t;

typedef struct {
	WIMAX_API_PACK_UPDATE update;

} ind_package_update_t;

#define MAX_NOTI_BUF	512
typedef struct {
	GCT_API_NOTI_CATEGORY category;
	GCT_API_NOTI_TYPE type;
	char buf[MAX_NOTI_BUF];
	int buf_len;

} ind_notification_t;

typedef struct {
	u32	status;
	GCT_API_POWER_MODE	power_mode;

} ind_mode_change_t;

typedef struct {
	int 				dev_idx;

	union {
		ind_recv_hci_t			recv_print;
		ind_recv_hci_t			recv_hci;
		ind_insert_remove_t		insert_remove;
		ind_pow_mng_t 			pow_mng;
		ind_stat_update_t		stat_update;
		ind_connect_net_t		connect_net;
		ind_disconnect_net_t	disconnect_net;
		ind_net_search_wscan_t	net_search_wscan;
		ind_provisioning_t		provisioning;
		ind_package_update_t	package_update;
		ind_notification_t		notification;
		ind_mode_change_t		power_mode;
	} u;

} ind_msg_t;

typedef struct sdk_internal_s {
	u32			struct_size;
	api_hand_t	*api;
	sdk_ind_t	ind;

	bool		dev_open[MAX_DEVICE];

	bool		ro;	/* Read-Only mdoe*/
	int			mode;

} sdk_internal_t;

/* FSM event */
typedef enum {
	AM_Open,
	AM_Scan,
	AM_Close,

	NM_ScanComplete,
	NM_RadioOn,
	NM_RadioOff,
	NM_ModeChange,
	NC_ConnectStart,
	NC_ConnectComplete,
	NC_ConnectFail,
	NC_AssocStart,
	NC_AssocSuccess,
	NC_AssocFail,
	NC_Disconnect,
	NC_RangingStart,
	NC_RangingComplete,
	NC_SbcStart,
	NC_SbcComplete,
	NC_AuthStart,
	NC_AuthComplete,
	NC_RegStart,
	NC_RegComplete,
	NC_DsxStart,
	NC_DsxComplete,

	fsm_ind_end
} fsm_event_t;

typedef enum {
	sdk_read_only = (1<<0),

	sdk_eng_mode = (1<<8)

} sdk_mode_t;;

typedef struct sdk_mng_s {
	char nonvolatile_dir[256];
	char log_path[256];
	char eap_pcap[256];
	char eap_dec[256];
	bool	eeap_enabled;
	bool	odm_enabled;
	msg_thr_t hci_recvr;
	msg_thr_t odm_evt;

} sdk_mng_t;

extern sdk_mng_t sdk_mng;

#define SDK_USE_EEAP	(sdk_mng.eeap_enabled)
#define SDK_USE_ODM	(sdk_mng.odm_enabled)

const char *sdk_version(void);
struct sdk_internal_s *sdk_get_rw_handle(void);
int sdk_init(GCT_WIMAX_SDK_MODE mode, GCT_WIMAX_API_PARAM *sdk_param);
int sdk_deinit(void);

api_hand_t *sdk_api_open(int mode);
int sdk_api_close(api_hand_t *api_hand);
int sdk_debug_level(api_hand_t *api_hand, int level);
int sdk_print_log(api_hand_t *api_hand, int flag, const char *title, const char *str);
int sdk_get_device_list(api_hand_t *api_hand, WIMAX_API_HW_DEVICE_ID *dev_list,
		u32 *list_cnt);
int sdk_device_open(api_hand_t *api_hand, int dev_idx);
int sdk_internal_device_close(struct sdk_internal_s *sdk, int dev_idx);
int sdk_device_close(api_hand_t *api_hand, int dev_idx);
int sdk_get_status(sdk_internal_t *sdk, int dev_idx, int *m_status, int *c_status);
int sdk_set_status(sdk_internal_t *sdk, int dev_idx, int m_status, int c_status);
int sdk_get_device_status(api_hand_t *api_hand, int dev_idx,
		WIMAX_API_DEVICE_STATUS_P status,
		WIMAX_API_CONNECTION_PROGRESS_INFO_P connect_prog_info);
int sdk_write_hci_packet(api_hand_t *api_hand, int dev_idx, void *buf, int len);
int sdk_set_power_control(api_hand_t *api_hand, int dev_idx, rf_stat_t rf_stat);
int sdk_get_profile_list(api_hand_t *api_hand, int dev_idx,
		WIMAX_API_PROFILE_INFO_P list, int *list_cnt);
int sdk_set_profile(api_hand_t *api_hand, int dev_idx, u32 profile_id);
int sdk_set_scan_interval(api_hand_t *api_hand, int dev_idx, u32 interval_sec);
int sdk_set_eap(api_hand_t *api_hand, int dev_idx, GCT_API_EAP_PARAM_P eap);
int sdk_get_statistics(api_hand_t *api_hand, int dev_idx,
		WIMAX_API_CONNECTION_STAT_P statistics);
int sdk_get_linkstatus(api_hand_t *api_hand, int dev_idx,
		WIMAX_API_LINK_STATUS_INFO_P link_status);
int sdk_get_connected_nsp(api_hand_t *api_hand, int dev_idx, 
		WIMAX_API_CONNECTED_NSP_INFO_P nsp_info);
int sdk_get_network_list(api_hand_t *api_hand, int dev_idx, WIMAX_API_NSP_INFO_P list,
		u32 *list_cnt);
int sdk_connect_network(api_hand_t *api_hand, int dev_idx, uchar *nsp_name, u32 profile_id);
int sdk_disconnect_network(api_hand_t *api_hand, int dev_idx);
int sdk_get_device_info(api_hand_t *api_hand, int dev_idx,
		WIMAX_API_DEVICE_INFO_P dev_info);
int sdk_get_rf_info(api_hand_t *api_hand, int dev_idx, GCT_API_RF_INFORM_P rf_info);
int sdk_get_bl_ver(api_hand_t *api_hand, int dev_idx, char *buf, int size);
int sdk_get_capability(api_hand_t *api_hand, int dev_idx, u32 *cap);
int sdk_set_capability(api_hand_t *api_hand, int dev_idx, u32 cap);
int sdk_get_neighbor_list(api_hand_t *api_hand, int dev_idx,
		GCT_API_NEIGHBOR_LIST_P list, int *list_cnt);
int sdk_net_search_scan(api_hand_t *api_hand, int dev_idx, GCT_API_SCAN_TYPE type);
int sdk_cancel_scan(api_hand_t *api_hand, int dev_idx);
int sdk_on_ind_event(sdk_internal_t *sdk, int dev_idx, fsm_event_t event);
int sdk_ind_event(int dev_idx, fsm_event_t event);
int sdk_cmd_mac_state(api_hand_t *api_hand, int dev_idx, GCT_API_CMD_MAC_STATE_TYPE type);
int sdk_set_idle_mode_timeout(api_hand_t *api_hand, int dev_idx, u16 timeoutSec);
int sdk_get_phy_mac_basic(api_hand_t *api_hand, int dev_idx, GCT_API_MAC_PHY_MAC_BASIC_P pData);
int sdk_get_phy_mcs(api_hand_t *api_hand, int dev_idx, GCT_API_MAC_PHY_MCS_P pData);
int sdk_get_phy_cinr_rssi(api_hand_t *api_hand, int dev_idx, GCT_API_MAC_PHY_CINR_RSSI_P pData);

#if defined(CONFIG_ENABLE_SERVICE_FLOW)
int sdk_sf_BeginSFRead(api_hand_t *api_hand, int dev_idx);
int sdk_sf_EndSFRead(api_hand_t *api_hand, int dev_idx);
int sdk_sf_GetNextSF(api_hand_t *api_hand, int dev_idx,
				WIMAX_SERVICE_FLOW *pSF,
				UINT8 Direction,
				WIMAX_SERVICE_FLOW **ppRetSF);
int sdk_sf_GetServiceFlow(api_hand_t *api_hand, int dev_idx,
				UINT32 SFID,
				WIMAX_SERVICE_FLOW **ppRetSF);
int sdk_sf_GetNextClfrRule(api_hand_t *api_hand, int dev_idx, 
				WIMAX_SERVICE_FLOW *pSF,
				WIMAX_CLFR_RULE *pCLFRRule,
				WIMAX_CLFR_RULE **ppRetCLFRRule);
int sdk_sf_GetClfrRule(api_hand_t *api_hand, int dev_idx, 
				WIMAX_SERVICE_FLOW *pSF,
				UINT16 PacketClassfierRuleIndex,
				WIMAX_CLFR_RULE **ppRetCLFRRule);
int sdk_sf_GetNextPHSRule(api_hand_t *api_hand, int dev_idx, 
				WIMAX_SERVICE_FLOW *pSF,
				WIMAX_PHS_RULE *pPHSRule,
				WIMAX_PHS_RULE **ppRetPHSRule);
int sdk_sf_GetPHSRule(api_hand_t *api_hand, int dev_idx, 
				WIMAX_SERVICE_FLOW *pSF, 
				UINT8 PHSI,
				WIMAX_PHS_RULE **ppRetPHSRule);
int sdk_sf_CmdAddSF(api_hand_t *api_hand, int dev_idx,
				WIMAX_SF_PARAM_P pSFParam,
				WIMAX_CLFR_RULE_P pClfrRule,
				WIMAX_PHS_RULE_P pPHSRule);
int sdk_sf_CmdChangeSF(api_hand_t *api_hand, int dev_idx,
				WIMAX_SF_PARAM_P pSFParam,
				WIMAX_CLFR_DSC_ACTION CLFRDSCAction,
				WIMAX_CLFR_RULE_P pClfrRule,
				WIMAX_PHS_DSC_ACTION PHSDSCAction,
				WIMAX_PHS_RULE_P pPHSRule);
int sdk_sf_CmdDeleteSF(api_hand_t *api_hand, int dev_idx,
				WIMAX_SF_PARAM_P pSFParam);
int sdk_sf_recv_dsx_complete(int dev_idx, u8 *buf, int len);
#endif // CONFIG_ENABLE_SERVICE_FLOW

int sdk_ind_recv_hci_pkt(int dev_idx, char *buf, int len);
int sdk_ind_dev_insert_remove(int dev_idx, bool is_insert);
int sdk_ind_status_update(int dev_idx, u8 *buf, int len);
int sdk_ind_if_updown(int dev_idx, u8 *buf, int len);
int sdk_ind_power_ctrl(sdk_internal_t *sdk, int dev_idx, bool on);
int sdk_ind_connect_net(sdk_internal_t *sdk, int dev_idx, bool success);
int sdk_ind_disconnect_net(sdk_internal_t *sdk, int dev_idx, bool success);
int sdk_ind_net_search_wscan(sdk_internal_t *sdk, int dev_idx);
int sdk_ind_rf_state(int dev_idx, u8 *buf, int len);
int sdk_ind_provisioning_op(int dev_idx, WIMAX_API_PROV_OPERATION operation,
	WIMAX_API_CONTACT_TYPE contact_type);
int sdk_ind_noti(int dev_idx, GCT_API_NOTI_CATEGORY category,
						GCT_API_NOTI_TYPE type, char *buf, int len);
int sdk_ind_power_mode_change(int dev_idx, u8 *buf, int len);

int sdk_reg_recv_hci_packet(api_hand_t *api_hand, SDKIndRcvHCIPacket callback);
int sdk_reg_dev_insert_remove(api_hand_t *api_hand, SDKIndDeviceInsertRemove callback);
int sdk_reg_ctrl_power_mng(api_hand_t *api_hand, SDKIndControlPowerManagement callback);
int sdk_reg_dev_stat_update(api_hand_t *api_hand, SDKIndDeviceStatusUpdate callback);
int sdk_reg_connect_network(api_hand_t *api_hand, SDKIndConnectToNetwork callback);
int sdk_reg_disconnect_network(api_hand_t *api_hand,
		SDKIndDisconnectFromNetwork callback);
int sdk_reg_network_search_wscn(api_hand_t *api_hand,
		SDKIndNetworkSearchWideScan callback);
int sdk_reg_provisioning_op(api_hand_t *api_hand, SDKIndProvisioningOperation callback);
int sdk_reg_package_update(api_hand_t *api_hand, SDKIndPackageUpdate callback);
int sdk_reg_notification(api_hand_t *api_hand, SDKIndNotification callback);
int sdk_reg_mode_change(api_hand_t *api_hand, SDKIndModeChange callback);

#if defined(CONFIG_ENABLE_SERVICE_FLOW)
int sdk_reg_noti_service_flow(api_hand_t *api_hand,
				SDKIndNotiServiceFlow callback);
#endif // CONFIG_ENABLE_SERVICE_FLOW

#define sdk_unreg_recv_hci_packet(api_hand)\
			sdk_reg_recv_hci_packet(api_hand, (SDKIndRcvHCIPacket)0)
#define sdk_unreg_dev_insert_remove(api_hand)\
			sdk_reg_dev_insert_remove(api_hand, (SDKIndDeviceInsertRemove)0)
#define sdk_unreg_ctrl_power_mng(api_hand)\
			sdk_reg_ctrl_power_mng(api_hand, (SDKIndControlPowerManagement)0)
#define sdk_unreg_dev_stat_update(api_hand)\
			sdk_reg_dev_stat_update(api_hand, (SDKIndDeviceStatusUpdate)0)
#define sdk_unreg_connect_network(api_hand)\
			sdk_reg_connect_network(api_hand, (SDKIndConnectToNetwork)0)
#define sdk_unreg_disconnect_network(api_hand)\
			sdk_reg_disconnect_network(api_hand, (SDKIndDisconnectFromNetwork)0)
#define sdk_unreg_network_search_wscn(api_hand)\
			sdk_reg_network_search_wscn(api_hand, (SDKIndNetworkSearchWideScan)0)
#define sdk_unreg_provisioning_op(api_hand)\
			sdk_reg_provisioning_op(api_hand, (SDKIndProvisioningOperation)0)
#define sdk_unreg_package_update(api_hand)\
			sdk_reg_package_update(api_hand, (SDKIndPackageUpdate)0)
#define sdk_unreg_notification(api_hand)\
			sdk_reg_notification(api_hand, (SDKIndNotification)0)
#define sdk_unreg_mode_change(api_hand)\
			sdk_reg_mode_change(api_hand, (SDKIndModeChange)0)

#if defined(CONFIG_ENABLE_SERVICE_FLOW)
#define sdk_unreg_noti_service_flow(api_hand)\
			sdk_reg_noti_service_flow(api_hand, (SDKIndNotiServiceFlow)0)
#endif // CONFIG_ENABLE_SERVICE_FLOW

int sdk_set_errno(int err_no);
int sdk_get_errno(void);


int sdk_read_file(const char *file, void *buf, int size);
int sdk_write_file(const char *file, void *buf, int size, int flags);
#define sdk_creat_file(file, buf, size)	\
		sdk_write_file(file, buf, size, O_CREAT|O_WRONLY|O_TRUNC)
#define sdk_append_file(file, buf, size)	\
		sdk_write_file(file, buf, size, O_APPEND)
#endif

