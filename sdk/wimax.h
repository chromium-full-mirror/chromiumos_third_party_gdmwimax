// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(WIMAX_H_20080710)
#define WIMAX_H_20080710
#include "gcttype.h"
#include "global.h"
#include "timer.h"
#include "eap.h"
#include "io.h"
#include "log.h"
#if defined(CONFIG_ENABLE_SERVICE_FLOW)
#include "sf.h"
#endif // CONFIG_ENABLE_SERVICE_FLOW

#define WM_MAX_VENDOR_NAME_LEN			128
#define WM_MAX_MODEL_NAME_LEN			128
#define WM_MAX_NAI_LEN					128
#define WM_MAX_NSP_NAME_LEN				128
#define WM_MAX_SUBSCRIPTION_NAME_LEN	128
#define WM_MAX_NSP						64
#define WM_MAX_NAP						64

#if defined(CONFIG_ENABLE_BW_SWITCHING_FOR_KT)
#define WM_DEFAULT_SCAN_INTERVAL_SEC		2
#else
#define WM_DEFAULT_SCAN_INTERVAL_SEC		5
#endif

// common data structures
#define NSP_ID_SIZE     3
#define NAP_ID_SIZE		3
#define BS_ID_SIZE		6

#define WIDE_SCAN_HNSP_ID	0xfffffd

#define DEFAULT_CAPABILITY	(0xfffffffflu)

typedef enum {
	rf_on,
	rf_off

} rf_stat_t;

typedef enum wimax_scan_type_e { 
	wm_scan_all_channels,	/*wide scan*/
	wm_scan_all_subscriptions,
	wm_scan_curr_subscription

} wimax_scan_type_t;

typedef enum { 
	wm_phy_min,
	wm_phy_sc, 
	wm_phy_sca, 
	wm_phy_ofdm, 
	wm_phy_ofdma, 
	wm_phy_max

} wm_phy_type_t;

typedef enum { 
	wm_assoc_success,
	wm_assoc_ranging_abort_by_bs,
	wm_assoc_ranging_timeout,
	wm_assoc_ranging_max_retry_exceeded,
	wm_assoc_ranging_other_failure,
	wm_assoc_sbc_timeout,
	wm_assoc_incompatible_sbc,
	wm_assoc_sbc_other_failure,
	wm_assoc_auth_eap_timeout,
	wm_assoc_auth_eap_failure,
	wm_assoc_auth_satek_timeout,
	wm_assoc_auth_satek_failure,
	wm_assoc_auth_key_req_timeout,
	wm_assoc_auth_key_req_failure,
	wm_assoc_req_msg_auth_failure,
	wm_assoc_req_timeout,
	wm_assoc_req_other_failure,
	wm_assoc_isf_creat_timeout,
	wm_assoc_isf_creat_failure

} wm_association_status_t;

typedef enum { 
	wm_connect_success, 
	wm_connect_success_activating, 
	wm_connect_success_provisioning,
	wm_connect_max_assoc_retry_exceeded, 
	wm_connect_other_failure

} wm_connection_status_t;

typedef enum {
	wm_ho_initiated_by_BS,
	wm_ho_initiated_by_MS

} wm_handover_reason_t;

typedef enum {
	wm_disconnect_by_DREZ,
	wm_disconnect_by_MS,
	wm_disconnect_by_OoZ,
	wm_disconnect_by_Others

} wm_disconnect_reason_t;

typedef enum {
	wm_addition_ind,
	wm_deletion_ind,
	wm_change_ind,

} wm_change_type_t;

enum { S_RNG, S_SBC, S_PKM, S_REG, S_DSX };
enum { S_START, S_COMPLETE };

typedef struct wm_nap_s {
	/* id */
	u8 		bsid[6];

	/* RSSI and CINR */
	u8		cinr;
	u8		rssi;

	/* Pointers to linked list of either visible_nap_list or cached_nap_list */
	struct list_head list;

} wm_nap_t;

typedef struct wm_nsp_s {
	u32		id;
	u8		name[WM_MAX_NSP_NAME_LEN];
	u8 		cinr;
	u8		rssi;
	u8		type;

	/* Chain list of hash table */
	struct list_head list;

	struct wm_nap_s *nap_array[WM_MAX_NAP];
	u8		nr_nap;

	struct wm_nap_s *capl_array[WM_MAX_NAP];
	u8		nr_capl;

} wm_nsp_t;

typedef struct wm_nsp_identifier_s {
	u8 id[NSP_ID_SIZE];
	u8 name[WM_MAX_NSP_NAME_LEN];

} wm_nsp_identifier_t;

typedef struct wm_nap_identifier_s {
	u8	id[NAP_ID_SIZE];

} wm_nap_identifier_t;

typedef struct wm_bs_identifier_s {
	u8	id[BS_ID_SIZE];

} wm_bs_identifier_t;

typedef struct wm_subscription_identifier_s {
	u8					oper_x[WM_MAX_NAI_LEN];/*Operator/X node*/
	wm_nsp_identifier_t	hnspid;	/*hnspid.name is operator name*/
	u8					user_hnai[WM_MAX_NAI_LEN];/*user home NAI*/

} wm_subscription_identifier_t;

typedef union wm_pkm_s {
	u8	pkm_support;
	struct {
		u8	pkm_v1: 1;
		u8	pkm_v2: 1;
	};

} wm_pkm_t;

typedef union wm_auth_policy_s { 
	u8 auth_policy_support; 
	struct { 
		u8 rsa_initial_entry : 1; 
		u8 eap_initial_entry : 1; 
		u8 authenticated_eap_entry : 1; 
		u8 reserved : 1; 
		u8 rsa_reentry : 1; 
		u8 eap_reentry : 1; 
		u8 authenticated_eap_reentry : 1; 
	};

} wm_auth_policy_t;

typedef union wm_cs_type_supported_s { 
	u32 supported_cs; 
	struct { 
		u32 ATMCS : 1; 
		u32 IPv4CS : 1; 
		u32 IPv6CS : 1; 
		u32 EthernetCS : 1; 
		u32 _8021QCS : 1; 
		u32 IPv4OverEthernetCS : 1; 
		u32 IPv6OverEthernetCS : 1; 
		s32 IPv4Over8021QCS : 1;
		u32 IPv6Over8021QCS : 1; 
		u32 EthernetWithROHCCS : 1; 
		u32 EthernetWithECRTPCS : 1; 
		u32 IPWithROHCCS : 1; 
		u32 IPWithECRTPCS : 1; 
	};
} wm_cs_type_supported_t;

typedef struct wm_rev_info_s {
	u32	bl_ver;
	u32	rel_ver;
	u32	fw_ver;
	u32	phy_hw_ver;

} wm_rev_info_t;

typedef struct wm_device_information_s {
	u32		max_subs_supported;
	u32		max_service_flow_supported;

	wm_phy_type_t phy_type;
	wm_pkm_t pkm_support;
	wm_auth_policy_t auth_support;
	wm_eap_param_t	eapp;

	dhcp_info_t	dhcp_info;

	#if defined(CONFIG_ENABLE_SERVICE_FLOW)
	wm_service_flow_t service_flow;
	#endif // CONFIG_ENABLE_SERVICE_FLOW

	wm_cs_type_supported_t cs_type_supported;
	bool	msihv_extensibility_security;
	bool	device_locked;

	wm_rev_info_t revision;

	u8	device_mac[6];
	u8	vendor_name[WM_MAX_VENDOR_NAME_LEN];
	u8	model_name[WM_MAX_MODEL_NAME_LEN];

} wm_device_info_t;

typedef struct bs_info_s {
	u8 hnspid[NSP_ID_SIZE];
	u8 nspid[NSP_ID_SIZE];
	u8 napid[NAP_ID_SIZE];
	u8 bsid[BS_ID_SIZE];

} bs_info_t;

typedef struct wm_net_id_s {
	wm_nsp_identifier_t	nsp;
	wm_nap_identifier_t	nap;
	wm_bs_identifier_t	bs;

} wm_net_id_t;

typedef struct wm_connect_s {
	u32 hnspid;
	u32 vnspid;

} wm_connect_t;
#define IS_VISITED_NSP(wm)	((wm)->conn.hnspid != (wm)->conn.vnspid)

typedef struct wm_association_start_s {
	wm_net_id_t net_id;

} wm_association_start_t;

typedef struct wm_association_complete_s {
	wm_association_status_t status;
	wm_net_id_t net_id;

} wm_association_complete_t;

typedef struct wm_disconnect_ind_s {	
	wm_disconnect_reason_t	reason;

} wm_disconnect_ind_t;

typedef struct wm_handover_start_s {
	wm_handover_reason_t	reason;

} wm_handover_start_t;

typedef struct wm_handover_complete_s {	
	wm_connection_status_t	status;
	wm_net_id_t net_id;

} wm_handover_complete_t;

typedef struct wn_connect_complete_s {
	wm_connection_status_t	status;
	wm_subscription_identifier_t subscription;
	wm_net_id_t net_id;

} wm_connect_complete_t;

typedef struct nsp_list_data_s {
	wm_nsp_identifier_t	NspIdentifier;
	struct list_head	list;

} nsp_list_data_t;

typedef struct nap_list_data_s {
	wm_nap_identifier_t nap_id;
	struct list_head	list;

} nap_list_data_t;

typedef struct wm_subscription_info_s {
	u8		idx;
	struct list_head	list;
	struct list_head	nsp_data_head;
	struct list_head	nap_data_head;/*CAPL*/
	
	wm_subscription_identifier_t	subscription_id;
	bool							auto_selection;
	bool							auto_selection_configurable;
	bool							auto_connect_at_home;
	bool							auto_connect_when_roam;
	bool							subs_locked;
	bool							activated;
	u32								num_operator_preferred_nsps;
	u32								nsp_array_offs;

} wm_subscription_info_t;

typedef struct wm_subs_id_info_s {
	struct list_head				list;
	bool							activated;
	wm_subscription_identifier_t	subs_id;

} wm_subs_id_info_t;

#if defined(CONFIG_ENABLE_BW_SWITCHING_FOR_KT)
#define SWITCHING_BW_MAX_SCAN_INTERVAL_SEC		64
#define BW_10MHz		(10*1000)
#define BW_8750KHz		(8750)
#endif
typedef struct wm_scan_s {
	wimax_scan_type_t	type;
	wm_subscription_info_t	*selected_subs;
	timer_obj_t			timer;
	bool				sf_mode;
	int					scan_interval_sec;
	int					last_scan_ms;
	bool				on_scanning;
	#if defined(CONFIG_ENABLE_BW_SWITCHING_FOR_KT)
	u32					scan_bw;
	int					nr_no_scaned;
	#endif

} wm_scan_t;

typedef struct wm_net_info_s {
	u32	rx_bytes;
	u32	rx_packets;
	u32	tx_bytes;
	u32	tx_packets;

} wm_net_info_t;

typedef struct wm_link_status_s {
	u8	rssi;
	u8	cinr;
	u8	tx_power;
	u32	cur_freq;

} wm_link_status_t;

typedef struct bs_neigh_s {
	unsigned char	frequency[3];
	unsigned char	preamble_idx;
	char	cinr;
	char	rssi;
	unsigned char	bsid[3];
	unsigned char	bandwidth;
	//signed short	rtd;

} __attribute__((packed)) bs_neigh_t;

typedef struct wm_bs_neigh_s {
	u32	frequency;
	u8	preamble;
	u8	cinr;
	u8	rssi;
	u8	bsid[BS_ID_SIZE];
	u8	bandwidth;
	//s16	rtd;

} wm_bs_neigh_t;

typedef enum wm_using_lock_e {
	WM_USING_LOCK_ODM,
	WM_USING_LOCK_ENTRIES

} wm_using_lock_t;

typedef struct mac_pdu_statistics_s {	/*0x50*/
	u32	timestamp;
	
	u8	start[0];

	u32	num_tx_sdu;
	u32	num_tx_sdu_drop;
	u32	num_rx_sdu;
	u32	num_rx_sdu_drop;
	u32	len_tx_sdu;
	u32	len_tx_sdu_drop;
	u32	len_rx_sdu;
	u32	len_rx_sdu_drop;
	u32	num_tx_pdu;
	u32	num_rx_pdu;
	u32	len_tx_pdu;
	u32	len_rx_pdu;
	u32	num_crc_error;
	u32	num_dec_error;
	u32	num_hcs_error;
	u32	num_key_error;
	u32	num_len_error;
	u32	bw_req_retries;
	u32	bw_req_count;
	
	u8	end[0];

} mac_pdu_statistics_t;

typedef struct ori_mac_phy_mac_basic_s {	/*0x51*/
	u32	frame_number;
	u32	fch;
	u16	ttg;
	u16	rtg;
	u8	num_dl_symbol;
	u8	num_ul_symbol;
	u8	current_pi;	/*PreambleIndex*/
	u8	previous_pi;	/*PreambleIndex*/
	u8	ul_perm_base;
	u8	mac_state;
	u8	bsid[6];
	s32	ul_time;
	u32	frequency;
	u16	bandwidth;
	u32	time_active;
	u32	time_sleep;
	u32	time_idle;
	u16	basic_cid;
	u16	primary_cid;

} __attribute__((packed)) ori_mac_phy_mac_basic_t;

typedef struct mac_phy_mac_basic_s {	/*0x51*/
	u32	timestamp;
	
	u8	start[0];

	u32	frame_number;
	u32	fch;
	u16	ttg;
	u16	rtg;
	u8	num_dl_symbol;
	u8	num_ul_symbol;
	u8	current_pi;	/*PreambleIndex*/
	u8	previous_pi;	/*PreambleIndex*/
	u8	ul_perm_base;
	u8	mac_state;
	u8	bsid[6];
	s32	ul_time;
	u32	frequency;
	u16	bandwidth;
	u16	pad;
	u32	time_active;
	u32	time_sleep;
	u32	time_idle;
	u16	basic_cid;
	u16	primary_cid;
	
	u8	end[0];

} mac_phy_mac_basic_t;

typedef struct mac_power_ctrl_s {	/*0x52*/
	u32	timestamp;

	u8	start[0];

	u8	power_ctrl_mode;
	u8	tx_power;
	u8	tx_sc_power;
	u8	last_tx_power;
	u8	last_tx_sc_power;
	u8	tx_max_power;
	u8	tx_max_sc_power;
	u8	tx_min_power;
	u8	tx_min_sc_power;
	
	u8	end[0];

} mac_power_ctrl_t;

typedef struct mac_phy_burst_s {	/*0x53*/
	u32	timestamp;

	u8	start[0];

	u32	num_dl_map;
	u32	num_ul_map;
	u32	num_dl_map_error;
	u32	num_ul_map_error;
	u32	num_dl_burst;
	u32	num_dl_burst_error;
	u32	num_ul_burst;
	u32	num_harq_dl_burst;
	u32	num_harq_dl_burst_drop;
	u32	num_harq_ul_burst;
	u32	num_harq_ul_burst_retry;
	u8	last_dl_burst_msc;
	u8	last_dl_burst_rpt;
	u8	last_diuc;
	u8	last_ul_burst_msc;
	u8	last_ul_burst_rpt;
	u8	last_uiuc;
	u16	reserved;
	u32	num_harq_crc_error;
	
	u8	end[0];

} mac_phy_burst_t;

typedef struct ori_mac_phy_mcs_s {	/*0x54*/
	u8	modulation_fec;
	u8	repetition_code;
	u8	iuc;
	u8	feature;

	u32	num_burst;
	u32	num_burst_error;
	u32	len_pdu;
	u32	num_pdu;

} __attribute__((packed)) ori_mac_phy_mcs_t;

typedef struct mac_phy_mcs_s {	/*0x54*/
	u32	timestamp;
	
	u8	start[0];

struct struct_s {
		u32 num_burst;
		u32 num_burst_error;
		u32 len_pdu;
		u32 num_pdu;
	} dl[OFDMA_FEC_MODE_CNT][REPETITION_CODING_CNT][MIMO_TYPE_CNT], ul[OFDMA_FEC_MODE_CNT][REPETITION_CODING_CNT][MIMO_TYPE_CNT];

	bool dl_used[OFDMA_FEC_MODE_CNT][REPETITION_CODING_CNT][MIMO_TYPE_CNT];
	bool ul_used[OFDMA_FEC_MODE_CNT][REPETITION_CODING_CNT][MIMO_TYPE_CNT];

	u8	end[0];

} mac_phy_mcs_t;

#define ZONE_NUMBER	1
typedef struct mac_phy_zone_s {	/*0x55*/
	u32	timestamp;
	
	u8	start[0];

	u32	zone[ZONE_NUMBER];
	
	u8	end[0];

} mac_phy_zone_t;

typedef struct mac_phy_cinr_rssi_s {	/*0x56*/
	u32	timestamp;
	
	u8	start[0];

	s8	cinr_mean;
	s8	cinr_std_dev;
	s8	rssi_mean;
	s8	rssi_std_dev;
	s8	cinr_a_mean;
	s8	cinr_b_mean;
	s8	cinr_main;
	s8	cinr_diversity;
	s8	rssi_main;
	s8	rssi_diversity;
	s8	preamble_cinr_reuse3;
	s8	preamble_cinr_reuse1;
	
	u8	end[0];

} mac_phy_cinr_rssi_t;

typedef struct mac_phy_temp_adc_s {	/*0x57*/
	u32	timestamp;
	
	u8	start[0];

	s8	temperature;
	u8	adc;
	
	u8	end[0];

} mac_phy_temp_adc_t;

typedef struct mac_phy_ctrl_chan_s {	/*0x58*/
	u32	timestamp;
	
	u8	start[0];

	u32	num_ffb_sent;
	u32	num_ack_sent;
	u32	num_irng_sent;
	u32	num_prng_sent;
	u32	num_bw_req_rng_sent;
	u32	num_ho_rng_sent;
	u32	num_irng_rsp;
	u32	num_prng_rsp;
	u32	num_horng_rsp;
	u32	num_irng_req;
	u32	num_horng_req;
	u32	num_irng_req_success;
	u32	num_horng_req_success;
	u32	num_cdma_alloc_ie;
	
	u8	end[0];

} mac_phy_ctrl_chan_t;

typedef struct mac_phy_dl_statistics_s {	/*0x59*/
	u32	timestamp;
	
	u8	start[0];

	u32	sfid;
	u16	cid;
	u16	feature_flag;
	u32	num_rx_sdu;
	u32	num_rx_sdu_drop;
	u32	len_rx_sdu;
	u32	len_rx_sdu_drop;
	u32	num_rx_pdu;
	u32	len_rx_pdu;
	u32	arq_rx_blocks;
	u32	arq_rx_blocks_retries;
	u16	arq_discard_msgs;
	u16	arq_reset_msgs;
	u16	arq_sync_losses;
	
	u8	end[0];

} mac_phy_dl_statistics_t;

typedef struct mac_phy_ul_statistics_s {	/*0x5a*/
	u32	timestamp;
	
	u8	start[0];

	u32	sfid;
	u16	cid;
	u16	feature_flag;
	u32	num_tx_sdu;
	u32	num_tx_sdu_drop;
	u32	len_tx_sdu;
	u32	len_tx_sdu_drop;
	u32	num_tx_pdu;
	u32	len_tx_pdu;
	u32	bw_req_count;
	u32	gmsh_out;
	u32	arq_tx_blocks;
	u32	arq_tx_blocks_retries;
	u16	arq_discard_msgs;
	u16	arq_reset_msgs;
	u16	arq_sync_losses;
	
	u8	end[0];

} mac_phy_ul_statistics_t;

typedef struct mac_handover_s {	/*0x5b*/
	u32	timestamp;
	
	u8	start[0];

	u16	ho_count;
	u16	ho_fail_count;
	u16	resync_count;
	u16	ho_latency;
	
	u8	end[0];

} mac_handover_t;

typedef struct ori_mac_neighbor_s {
	u8	frequency[3];
	u8	preamble_idx;
	s8	cinr;
	s8	rssi;
	u8	bsid[3];
	u8	bandwidth;
	s16	rtd;
} ori_mac_neighbor_t;

#define MAX_NEIGHBOT_LIST		16
typedef struct mac_neighbor_s {	/*0x5c*/
	u32	timestamp;

	u32	nr_list;

	u8	start[0];

	wm_bs_neigh_t list[MAX_NEIGHBOT_LIST];
	
	u8	end[0];

} mac_neighbor_t;

typedef struct mac_harq_statistics_s {	/*0x5d*/
	u32	timestamp;
	
	u8	start[0];

	u32	num_dl_harq_retry[8];
	u32	num_ul_harq_retry[8];
	
	u8	end[0];

} mac_harq_statistics_t;

typedef struct mac_net_entry_statistics_s {	/*0x5e*/
	u32	timestamp;
	
	u8	start[0];

	u16	cdma_rng_time;
	u16	rng_time;
	u16	sbc_time;
	u16	pkm_time;
	u16	reg_time;
	u16	dsa_time;
	u16	total_net_entry_time;
	
	u8	end[0];

} mac_net_entry_statistics_t;

typedef struct mac_report_s {
	mac_pdu_statistics_t			pdu_statistics;
	mac_phy_mac_basic_t			phy_mac_basic;
	mac_power_ctrl_t				power_ctrl;
	mac_phy_burst_t				phy_burst;
	mac_phy_mcs_t					phy_mcs;
	mac_phy_zone_t				phy_zone;
	mac_phy_cinr_rssi_t			phy_cinr_rssi;
	mac_phy_temp_adc_t			phy_temp_adc;
	mac_phy_ctrl_chan_t			phy_ctrl_chan;
	mac_phy_dl_statistics_t		phy_dl_stat;
	mac_phy_ul_statistics_t		phy_ul_stat;
	mac_handover_t				handover;
	mac_neighbor_t				neighbor;
	mac_harq_statistics_t			harq_stat;
	mac_net_entry_statistics_t	net_entry_stat;

} mac_report_t;

typedef struct wimax_s {
	pthread_mutex_t		using_lock[WM_USING_LOCK_ENTRIES];
	pthread_mutex_t		fsm_lock;

	wm_device_info_t	dev_info;

	struct list2_head	scan_list;	/*link wm_nsp_s->list*/
	struct list2_head	subs_list;	/*link wm_subscription_info_t->list*/

	wm_scan_t 					scan;
	wm_connect_t				conn;
	wm_association_start_t		asso_start;
	wm_association_complete_t	asso_comp;
	wm_disconnect_ind_t			disconn_ind;
	wm_handover_start_t			ho_start;
	wm_handover_complete_t		ho_comp;
	wm_connect_complete_t		conn_comp;

} wimax_t;

inline static void wm_in_wimax(struct wimax_s *wm, int entry)
{
	xfunc_in("entry=%d", entry);
	pthread_mutex_lock(&wm->using_lock[entry]);
	xfunc_out("entry=%d", entry);
}
inline static void wm_out_wimax(struct wimax_s *wm, int entry)
{
	xfunc_in("entry=%d", entry);
	pthread_mutex_unlock(&wm->using_lock[entry]);
	xfunc_out("entry=%d", entry);
}

void wm_cleanup_device(int dev_idx);
void wm_open_device(int dev_idx);
void wm_close_device(int dev_idx);
void wm_init_scan(int dev_idx);
void wm_deinit_scan(int dev_idx);
int wm_init_device_info(int dev_idx);
int wm_sync_subscription(int dev_idx);
int wm_set_scan_type(int dev_idx, wimax_scan_type_t type);
int wm_active_scan_interval(int dev_idx, u32 interval_sec);
int wm_req_scan(int dev_idx);
int wm_net_search_scan(int dev_idx, wimax_scan_type_t scantype);
int wm_cancel_scan(int dev_idx);
void wm_clean_scan_info(int dev_idx);
int wm_req_interval_scan(int dev_idx, int interval_milisec);
int wm_update_subscription(int dev_idx, u8 *buf, int len);
wm_subscription_info_t *wm_get_subscription(int dev_idx, u8 subs_idx, u8 *hnspid);
wm_subscription_info_t *wm_get_activated_subs(int dev_idx);
int wm_combine_level(int dev_idx, int c0, int c1);
u8 wm_convert_cinr(s8 cinr);
u8 wm_convert_rssi(s8 rssi);
int wm_get_network_type(struct wimax_s *wm, u8 *nspid);
struct wm_nsp_s *wm_lookup_nsp_by_name(struct wimax_s *wm, u8 *nsp_name);
struct wm_nsp_s *wm_lookup_nsp_by_id(struct wimax_s *wm, u8 *nspid);
int wm_connect_network(int dev_idx, u8 *hnspid, u8 *vnspid);
int wm_re_connect_network(int dev_idx);
int wm_cr801_re_connect(int dev_idx);
int wm_get_dhcp_info(int dev_idx);
int wm_disconnect_network(int dev_idx, int timeout_sec);
int wm_get_rf_state(int dev_idx, rf_stat_t *rf_stat);
int wm_set_rf_state(int dev_idx, rf_stat_t rf_stat, int timeout_sec);
int wm_set_capability(int dev_idx, u32 capability);
int wm_report_capability(int dev_idx, u8 *buf, int len);
int wm_set_eap(int dev_idx, bool enable);
int wm_get_netinfo(int dev_idx, struct wm_net_info_s *net_info);
int wm_get_linkinfo(int dev_idx, struct wm_link_status_s *link_s);
int wm_get_rf_info(int dev_idx, GCT_API_RF_INFORM_P rf_info);
int wm_retrieve_mac_pdu_statistics(mac_pdu_statistics_t *p, u8 *buf, int len);
int wm_retrieve_mac_phy_mac_basic(mac_phy_mac_basic_t *p, u8 *buf, int len);
int wm_retrieve_mac_power_ctrl(mac_power_ctrl_t *p, u8 *buf, int len);
int wm_retrieve_mac_phy_burst(mac_phy_burst_t *p, u8 *buf, int len);
int wm_retrieve_mac_phy_mcs(mac_phy_mcs_t *p, u8 *buf, int len);
int wm_retrieve_mac_phy_zone(mac_phy_zone_t *p, u8 *buf, int len);
int wm_retrieve_mac_phy_cinr_rssi(mac_phy_cinr_rssi_t *p, u8 *buf, int len);
int wm_retrieve_mac_phy_temp_adc(mac_phy_temp_adc_t *p, u8 *buf, int len);
int wm_retrieve_mac_phy_ctrl_chan(mac_phy_ctrl_chan_t *p, u8 *buf, int len);
int wm_retrieve_mac_phy_dl_statistics(mac_phy_dl_statistics_t *p, u8 *buf, int len);
int wm_retrieve_mac_phy_ul_statistics(mac_phy_ul_statistics_t *p, u8 *buf, int len);
int wm_retrieve_mac_handover(mac_handover_t *p, u8 *buf, int len);
int wm_retrieve_mac_neighbor(mac_neighbor_t *p, u8 *buf, int len, u8 *bsid);
int wm_retrieve_mac_harq_statistics(mac_harq_statistics_t *p, u8 *buf, int len);
int wm_retrieve_mac_net_entry_statistics(mac_net_entry_statistics_t *p, u8 *buf, int len);
int wm_set_run_mode(int dev_idx, int mode);
int wm_get_neighbor_list(int dev_idx, wm_bs_neigh_t *list, int *list_cnt);
int wm_notification(int dev_idx, char *buf, int len);
int wm_ind_sf_mode(int dev_idx, u8 *buf, int len);
int wm_disable_sf_mode(int dev_idx);
bool wm_compare_nsp(wm_nsp_identifier_t *nsp, wm_nsp_identifier_t *nsp2);
int wm_reset_selected_subsctiption(int dev_idx, wm_nsp_identifier_t *hnspid);
int wm_report_full_reentry(int dev_idx, u8 *buf, int len);
int wm_cmd_mac_state(int dev_idx, GCT_API_CMD_MAC_STATE_TYPE type);
int wm_set_idle_mode_timeout(int dev_idx, u16 timeoutSec);
int wm_get_phy_mac_basic(int dev_idx, GCT_API_MAC_PHY_MAC_BASIC_P pData);
int wm_get_phy_mcs(int dev_idx, GCT_API_MAC_PHY_MCS_P pData);
int wm_get_phy_cinr_rssi(int dev_idx, GCT_API_MAC_PHY_CINR_RSSI_P pData);

#endif
