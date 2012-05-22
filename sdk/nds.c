// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "global.h"
#include "error.h"
#include "device.h"
#include "io.h"
#include "sdk.h"
#include "nds.h"
#include "hci.h"
#include "log.h"

#if defined(CONFIG_ENABLE_BW_SWITCHING_FOR_KT)
#define FILE_BW_PATH	"/etc/gct/bw"

static u32 nds_get_switching_bw(void)
{
	char *file = FILE_BW_PATH;
	u32 bw = BW_8750KHz;
	int ret;

	ret = sdk_read_file(file, &bw, sizeof(bw));
	xprintf(SDK_DBG, "get switching_bw=%d, ret=%d\n", bw, ret);
	return bw;
}

static int nds_set_switching_bw(int bw)
{
	char *file = FILE_BW_PATH;
	int ret;

	ret = sdk_write_file(file, &bw, sizeof(bw), O_WRONLY);
	xprintf(SDK_DBG, "set switching_bw=%d, ret=%d\n", bw, ret);
	return ret;
}

void nds_update_switching_bw(int dev_idx)
{
	device_t *dev;
	wimax_t *wm;
	bool scaned;

	xfunc_in();
	if (!(dev = dm_get_dev(dev_idx)))
		return;
	wm = dev->wimax;
	scaned = wm->scan_list.cnt;
	
	if (scaned) {
		wm->scan.nr_no_scaned = 0;
		wm->scan.scan_interval_sec = WM_DEFAULT_SCAN_INTERVAL_SEC;
		nds_set_switching_bw(wm->scan.scan_bw);
	}
	else {
		wm->scan.nr_no_scaned++;
		/*Toggle BW*/
		if (wm->scan.scan_bw == BW_8750KHz)
			wm->scan.scan_bw = BW_10MHz;
		else
			wm->scan.scan_bw = BW_8750KHz;
		if (wm->scan.nr_no_scaned == 1)
			wm_req_scan(dev_idx);
	}

	if (wm->scan.nr_no_scaned >= 2) {
		if (wm->scan.scan_interval_sec < SWITCHING_BW_MAX_SCAN_INTERVAL_SEC)
			wm->scan.scan_interval_sec *= 2;
		wm->scan.nr_no_scaned = 0;
	}
	xprintf(SDK_DBG, "nr_no_scaned=%d, scan_bw=%d, interval_sec=%d\n",
		wm->scan.nr_no_scaned, wm->scan.scan_bw, wm->scan.scan_interval_sec);
	dm_put_dev(dev_idx);
	xfunc_out();
}

static void nds_init_switching_bw(int dev_idx)
{
	device_t *dev;
	wimax_t *wm;
	char *file = FILE_BW_PATH;
	u32 bw = BW_8750KHz;

	xfunc_in();
	if (!(dev = dm_get_dev(dev_idx)))
		return;
	wm = dev->wimax;

	if (access(file, 0) < 0)
		sdk_creat_file(file, &bw, sizeof(bw));
	else
		bw = nds_get_switching_bw();

	if (bw == BW_10MHz)
		wm->scan.scan_bw = BW_10MHz;
	else
		wm->scan.scan_bw = BW_8750KHz;
	dm_put_dev(dev_idx);
	xfunc_out();
}
#endif

void nds_init(int dev_idx)
{
	device_t *dev;
	wimax_t *wm;

	if (!(dev = dm_get_dev(dev_idx)))
		return;
	wm = dev->wimax;

	xfunc_in();

	pthread_mutex_init(&wm->scan_list.lock, NULL);
	INIT_LIST2(&wm->scan_list);

	pthread_mutex_init(&wm->subs_list.lock, NULL);
	INIT_LIST2(&wm->subs_list);

	assert(wm->subs_list.head.prev && wm->subs_list.head.next);
	#if defined(CONFIG_ENABLE_BW_SWITCHING_FOR_KT)
	nds_init_switching_bw(dev_idx);
	#endif
	dm_put_dev(dev_idx);
	xfunc_out();
}

void nds_deinit(int dev_idx)
{
	device_t *dev;
	wimax_t *wm;

	if (!(dev = dm_get_dev(dev_idx)))
		return;
	wm = dev->wimax;

	nds_clean_scan_list(wm);
	dm_put_dev(dev_idx);
}

int nds_req_scan(int dev_idx, u8 scan_type, u8 *hnspid, u8 *usr_hnai)
{
	u8 param[HCI_MAX_PARAM];
	int len = 0, str_len;
	int ret;
	#if defined(CONFIG_ENABLE_BW_SWITCHING_FOR_KT)
	device_t *dev;
	wimax_t *wm;
	u32 bw;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	wm = dev->wimax;
	#endif

	xfunc_in("dev=%d, type=%d", dev_idx, scan_type);
	assert(scan_type <= W_SCAN_SPECIFIED_SUBSCRIPTION);

	param[len++] = scan_type;
	if (scan_type == W_SCAN_SPECIFIED_SUBSCRIPTION) {
		len += hci_set_tlv(&param[len], TLV_T(T_H_NSPID), TLV_L(T_H_NSPID), hnspid);
		str_len = strlen((char *)usr_hnai) + 1/*null*/;
		len += hci_set_tlv(&param[len], TLV_T(T_SUBSCRIPTION_NAME), str_len, usr_hnai);
		xprintf(SDK_INFO, "Scan hnspid=%06X, user_hnai=%s\n", U82U24(hnspid), usr_hnai);
	}
	#if defined(CONFIG_ENABLE_BW_SWITCHING_FOR_KT)
	xprintf(SDK_NOTICE, "Scan BW=%d\n", wm->scan.scan_bw);
	bw = DH2B(wm->scan.scan_bw);
	len += hci_set_tlv(&param[len], TLV_T(T_BW), TLV_L(T_BW), (u8*)&bw);
	#endif

	if ((ret = hci_send(dev_idx, WIMAX_SCAN, param, len)) == len)
		ret = 0;
	else
		ret = -1;
	#if defined(CONFIG_ENABLE_BW_SWITCHING_FOR_KT)
	dm_put_dev(dev_idx);
	#endif
	xfunc_out("ret=%d", ret);
	return ret;
}

int nds_parse_scan_list(int dev_idx, struct wimax_s *wm, u8 *buf, int len)
{
	struct list2_head *scan_list = &wm->scan_list;
	struct wm_nsp_s *nsp = NULL;
	struct wm_nap_s *nap;
	u8 T, diff_T, *V;
	u16 L;
	int pos = 0, nr_nap, getn;

	xfunc_in();
	pthread_mutex_lock(&wm->scan_list.lock);

	while (pos < len) {
		pos += hci_get_tlv(&buf[pos], &T, &L, &V);
		if (T == TLV_T(T_H_NSPID)) {
			xprintf(SDK_INFO, "[%d] SCAN H_NSPID=0x%08X\n", dev_idx, U82U24(V));
			pos += hci_get_tlv(&buf[pos], &T, &L, &V);
		}
		if (T != (diff_T=TLV_T(T_V_NSPID))) goto diff_type;

		nsp = (struct wm_nsp_s *) sdk_malloc(sizeof(struct wm_nsp_s));
		INIT_LIST_HEAD(&nsp->list);
		nsp->id = U82U24(V);

		nsp->type = wm_get_network_type(wm, V);

		pos += hci_get_tlv(&buf[pos], &T, &L, &V);
		if (T != (diff_T=TLV_T(T_NSP_NAME))) goto diff_type;
		memcpy(nsp->name, V, L);
		nsp->name[L] = 0;

		pos += hci_get_tlv(&buf[pos], &T, &L, &V);
		if (T != (diff_T=TLV_T(T_CINR))) goto diff_type;
		nsp->cinr = wm_convert_cinr(*V);

		pos += hci_get_tlv(&buf[pos], &T, &L, &V);
		if (T != (diff_T=TLV_T(T_RSSI))) goto diff_type;
		nsp->rssi = wm_convert_rssi(*V);

		xprintf(SDK_INFO, "[%d] V_NSPID=0x%08X, V_NSPNAME=%s, cinr=%d, rssi=%d\n", 
			dev_idx, nsp->id, nsp->name, nsp->cinr, nsp->rssi);

		nr_nap = 0;
		do {
			getn = hci_get_tlv(&buf[pos], &T, &L, &V);
			if (T == TLV_T(T_BSID)) {
				assert(WM_MAX_NAP > nr_nap);
				pos += getn;
				nap = (struct wm_nap_s *) sdk_malloc(sizeof(struct wm_nap_s));
				nsp->nap_array[nr_nap] = nap;
				memcpy(&nap->bsid, V, TLV_L(T_BSID));

				pos += hci_get_tlv(&buf[pos], &T, &L, &V);
				if (T != (diff_T=TLV_T(T_CINR))) {
					sdk_free(nsp->nap_array[nr_nap]);
					nsp->nap_array[nr_nap] = NULL;
					goto diff_type;
				}
				nap->cinr = wm_convert_cinr(*V);

				pos += hci_get_tlv(&buf[pos], &T, &L, &V);
				if (T != (diff_T=TLV_T(T_RSSI))) {
					sdk_free(nsp->nap_array[nr_nap]);
					nsp->nap_array[nr_nap] = NULL;
					goto diff_type;
				}
				nap->rssi = wm_convert_rssi(*V);

				xprintf(SDK_INFO, "[%d] BS(%d): cinr=%d, rssi=%d\n",
					dev_idx, nr_nap, nap->cinr, nap->rssi);
				nr_nap++;
			}
			else
				break;
		} while(pos < len);
		nsp->nr_nap = nr_nap;

		list_add_tail(&nsp->list, &scan_list->head);
		scan_list->cnt++;
	}

	pthread_mutex_unlock(&wm->scan_list.lock);
	xfunc_out();
	return 0;

diff_type:
	pthread_mutex_unlock(&wm->scan_list.lock);
	if (nsp) {
		list_del(&nsp->list);
		sdk_free(nsp);
	}
	xprintf(SDK_ERR, "Diff type(0x%02X != 0x%02X)\n", T, diff_T);
	return -1;
}

void nds_clean_scan_list(struct wimax_s *wm)
{
	struct list_head *head = &wm->scan_list.head;
	struct wm_nsp_s *nsp, *tmp;
	int i;

	pthread_mutex_lock(&wm->scan_list.lock);
	list_for_each_entry_safe(nsp, tmp, head, list) {
		list_del(&nsp->list);
		for (i = 0; i < nsp->nr_nap; i++) {
			sdk_free(nsp->nap_array[i]);
			nsp->nap_array[i] = NULL;
		}
		sdk_free(nsp);
	}
	INIT_LIST2(&wm->scan_list);
	pthread_mutex_unlock(&wm->scan_list.lock);
}

int nds_update_scan_result(int dev_idx, u8 *buf, int len)
{
	device_t *dev;
	wimax_t *wm;
	int ret;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	wm = dev->wimax;
	
	xfunc_in("dev=%d, len=%d", dev_idx, len);

	nds_clean_scan_list(wm);

	ret = nds_parse_scan_list(dev_idx, wm, buf, len);
	if (ret < 0)
		nds_clean_scan_list(wm);

	xfunc_out();
	dm_put_dev(dev_idx);
	return ret;
}

static void nds_print_bs_info(int dev_idx, struct bs_info_s *bsinfo, const char *title)
{
	xprintf(SDK_INFO, "[%d] %s\n", dev_idx, title);
	xprintf(SDK_INFO, "H_NSPID[0x%08X], V_NSPID[0x%08X], NAPID[0x%08X]\n",
		U82U24(bsinfo->hnspid), U82U24(bsinfo->nspid), U82U24(bsinfo->napid));
		
	xprintf(SDK_INFO, "id[%02X:%02X:%02X:%02X:%02X:%02X]\n",
		bsinfo->bsid[0], bsinfo->bsid[1], bsinfo->bsid[2],
		bsinfo->bsid[3], bsinfo->bsid[4], bsinfo->bsid[5]);
}

int nds_get_bs_info(int dev_idx, u8 *buf, int len, struct bs_info_s *bsinfo)
{
	u8 T, *V;
	u16 L;
	int pos = 0;
	int ret = 0;

	xfunc_in("dev=%d, len=%d", dev_idx, len);

	while (pos < len) {
		pos += hci_get_tlv(&buf[pos], &T, &L, &V);
		switch (T) {
			case TLV_T(T_H_NSPID):
				ret = (TLV_L(T_H_NSPID) == L) ? 0 : sdk_set_errno(ERR_UNEXPECTED_TLV);
				memcpy(bsinfo->hnspid, V, TLV_L(T_H_NSPID));
				break;
			case TLV_T(T_V_NSPID):
				ret = (TLV_L(T_V_NSPID) == L) ? 0 : sdk_set_errno(ERR_UNEXPECTED_TLV);
				memcpy(bsinfo->nspid, V, TLV_L(T_V_NSPID));
				break;
			case TLV_T(T_BSID):
				ret = (TLV_L(T_BSID) == L) ? 0 : sdk_set_errno(ERR_UNEXPECTED_TLV);
				memcpy(bsinfo->bsid, V, TLV_L(T_BSID));
				memcpy(bsinfo->napid, V, NAP_ID_SIZE);
				break;
			case TLV_T(T_CUR_FREQ):
				xprintf(SDK_NOTICE, "Current Frequency is %d\n", U82U32(V));
				break;
			case TLV_T(T_BW):
				xprintf(SDK_NOTICE, "Current Bandwidth is %d\n", U82U32(V));
				break;
			default:
				xprintf(SDK_NOTICE, "Type(0x%02X) is unexpected\n", T);
				break;
		}
	}

	xfunc_out();
	return ret;
}

int nds_connect_complete(int dev_idx, u8 *buf, int len)
{
	device_t *dev;
	wimax_t *wm;
	wm_subscription_info_t *subs;
	bs_info_t bsinfo;
	wm_nsp_t *nsp;
	int pos = 0;
	fsm_event_t event = NC_ConnectComplete;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	wm = dev->wimax;
	xfunc_in("dev=%d, len=%d", dev_idx, len);

	wm->conn_comp.status = buf[pos++];
	if (wm->conn_comp.status != wm_connect_success) {
		event = NC_ConnectFail;
		xprintf(SDK_INFO, "NC_ConnectFail: status=%d\n", wm->conn_comp.status);
		goto out;
	}

	if (nds_get_bs_info(dev_idx, &buf[pos], len-pos, &bsinfo) < 0)
		goto out;
	nds_print_bs_info(dev_idx, &bsinfo, "Connect complete");
	memcpy(wm->conn_comp.subscription.hnspid.id, bsinfo.hnspid, TLV_L(T_H_NSPID));
	memcpy(wm->conn_comp.net_id.nsp.id, bsinfo.nspid, TLV_L(T_V_NSPID));
	memcpy(wm->conn_comp.net_id.nap.id, bsinfo.napid, TLV_L(T_NAP_ID));
	memcpy(wm->conn_comp.net_id.bs.id, bsinfo.bsid, TLV_L(T_BSID));

	if ((subs = wm->scan.selected_subs)) {
		if (memcmp(bsinfo.hnspid, subs->subscription_id.hnspid.id, NSP_ID_SIZE)) {
			xprintf(SDK_ERR, "Mismatch H_NSPID(0x%08X!=0x%08X)\n",
				U82U24(bsinfo.hnspid), U82U24(subs->subscription_id.hnspid.id));
			goto out;
		}

		memcpy(&wm->conn_comp.subscription,
			&subs->subscription_id,
			sizeof(wm_subscription_identifier_t));
	}
	else if (wm->scan.type != wm_scan_all_channels) {
			xprintf(SDK_ERR, "Selected subscription was not set up\n");
			goto out;
	}

	if (memcmp(bsinfo.hnspid, bsinfo.nspid, NSP_ID_SIZE)) {
		nsp = wm_lookup_nsp_by_id(wm, bsinfo.nspid);
		if (nsp)
			strcpy((char *)wm->conn_comp.net_id.nsp.name, (char *)nsp->name);
		else {
			memset(wm->conn_comp.net_id.nsp.name, 0, WM_MAX_NSP_NAME_LEN);
			xprintf(SDK_ERR, "T_V_NSPID(0x%08X)'s NSP list is not found\n",
				U82U24(wm->conn_comp.net_id.nsp.id));
			event = NC_ConnectFail;
			goto out;
		}
	}
out:
	sdk_ind_event(dev_idx, event);

	xfunc_out();
	dm_put_dev(dev_idx);
	return (event==NC_ConnectComplete) ? 0 : -1;
}

int nds_associate_start(int dev_idx, u8 *buf, int len)
{
	device_t *dev;
	struct wimax_s *wm;
	struct bs_info_s bsinfo;
	struct wm_nsp_s *nsp;
	int pos = 0;
	fsm_event_t event = NC_AssocStart;
	int ret = -1;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	wm = dev->wimax;

	xfunc_in("dev=%d, len=%d", dev_idx, len);

	if (nds_get_bs_info(dev_idx, &buf[pos], len-pos, &bsinfo) < 0)
		goto out;

	memcpy(wm->asso_start.net_id.nsp.id, bsinfo.nspid, TLV_L(T_V_NSPID));
	memcpy(wm->asso_start.net_id.nap.id, bsinfo.napid, TLV_L(T_NAP_ID));
	memcpy(wm->asso_start.net_id.bs.id, bsinfo.bsid, TLV_L(T_BSID));

	nsp = wm_lookup_nsp_by_id(wm, bsinfo.nspid);
	if (nsp) {
		strcpy((char *)wm->asso_start.net_id.nsp.name, (char *)nsp->name);
		ret = 0;
	}
	else {
		memset(wm->asso_start.net_id.nsp.name, 0, WM_MAX_NSP_NAME_LEN);
		xprintf(SDK_ERR, "T_V_NSPID(0x%08X)'s NSP list is not found\n",
			U82U24(bsinfo.nspid));
	}
out:
	sdk_ind_event(dev_idx, event);

	xfunc_out();
	dm_put_dev(dev_idx);
	return ret;
}

int nds_associate_complete(int dev_idx, u8 *buf, int len)
{
	device_t *dev;
	struct wimax_s *wm;
	struct bs_info_s bsinfo;
	struct wm_nsp_s *nsp;
	int pos = 0;
	fsm_event_t event = NC_AssocSuccess;
	int ret = -1;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	wm = dev->wimax;

	xfunc_in("dev=%d, len=%d", dev_idx, len);

	wm->asso_comp.status = buf[pos++];
	if (wm->asso_comp.status != wm_connect_success) {
		event = NC_AssocFail;
		xprintf(SDK_INFO, "NC_AssocFail: status=%d\n", wm->asso_comp.status);
		goto out;
	}

	if (nds_get_bs_info(dev_idx, &buf[pos], len-pos, &bsinfo) < 0)
		goto out;
	memcpy(wm->asso_comp.net_id.nsp.id, bsinfo.nspid, TLV_L(T_V_NSPID));
	memcpy(wm->asso_comp.net_id.nap.id, bsinfo.napid, TLV_L(T_NAP_ID));
	memcpy(wm->asso_comp.net_id.bs.id, bsinfo.bsid, TLV_L(T_BSID));

	nsp = wm_lookup_nsp_by_id(wm, bsinfo.nspid);
	if (nsp) {
		strcpy((char *)wm->asso_comp.net_id.nsp.name, (char *)nsp->name);
		ret = 0;
	}
	else {
		wm->asso_comp.status = wm_assoc_req_other_failure;
		memset(wm->asso_comp.net_id.nsp.name, 0, WM_MAX_NSP_NAME_LEN);
		xprintf(SDK_ERR, "T_V_NSPID(0x%08X)'s NSP list is not found\n",
			U82U24(bsinfo.nspid));
	}
out:
	sdk_ind_event(dev_idx, event);

	xfunc_out();
	dm_put_dev(dev_idx);
	return ret;
}

int nds_disconnect_ind(int dev_idx, u8 *buf, int len)
{
	device_t *dev;
	struct wimax_s *wm;
	fsm_event_t event = NC_Disconnect;
	int ret;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	wm = dev->wimax;
	
	xfunc_in("dev=%d, len=%d", dev_idx, len);

	wm->disconn_ind.reason = buf[0];
	ret = sdk_ind_event(dev_idx, event);

	xfunc_out();
	dm_put_dev(dev_idx);
	return ret;
}

int nds_ho_start(int dev_idx, u8 *buf, int len)
{
	device_t *dev;
	wimax_t *wm;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	wm = dev->wimax;

	xfunc_in("dev=%d, len=%d", dev_idx, len);

	wm->ho_start.reason = buf[0];

	xfunc_out();
	dm_put_dev(dev_idx);
	return 0;
}

int nds_ho_complete(int dev_idx, u8 *buf, int len)
{
	device_t *dev;
	wimax_t *wm;
	bs_info_t bsinfo;
	int pos = 0;
	int ret = -1;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	wm = dev->wimax;

	xfunc_in("dev=%d, len=%d", dev_idx, len);

	wm->ho_comp.status = buf[pos++];
	if (wm->ho_comp.status > wm_connect_success_provisioning)
		goto out;

	if (nds_get_bs_info(dev_idx, &buf[pos], len-pos, &bsinfo) < 0)
		goto out;
	nds_print_bs_info(dev_idx, &bsinfo, "Handover complete");
	memcpy(wm->ho_comp.net_id.nsp.id, bsinfo.nspid, TLV_L(T_V_NSPID));
	memcpy(wm->ho_comp.net_id.nap.id, bsinfo.napid, TLV_L(T_NAP_ID));
	memcpy(wm->ho_comp.net_id.bs.id, bsinfo.bsid, TLV_L(T_BSID));

	if (wm->ho_comp.status == wm_connect_success) {
		wm->conn_comp.status = wm_connect_success;
		memcpy(wm->conn_comp.net_id.nsp.id, bsinfo.nspid, TLV_L(T_V_NSPID));
		memcpy(wm->conn_comp.net_id.nap.id, bsinfo.napid, TLV_L(T_NAP_ID));
		memcpy(wm->conn_comp.net_id.bs.id, bsinfo.bsid, TLV_L(T_BSID));
	}

out:
	xfunc_out();
	dm_put_dev(dev_idx);
	return ret;
}

int nds_connection_stage(int dev_idx, u8 *buf, int len)
{
	char *stage_str = "Unknown";
	u8 stage = buf[0];
	u8 start_complete = buf[1];
	fsm_event_t event = 0;
	int ret = 0;

	switch (stage) {
		case S_RNG:
			stage_str = STR(S_RNG);
			if (start_complete == S_START)
				event = NC_RangingStart;
			else
				event = NC_RangingComplete;
			break;
		case S_SBC:
			stage_str = STR(S_SBC);
			#if 1
			if (start_complete == S_START)
				event = NC_SbcStart;
			else
				event = NC_SbcComplete;
			#endif
			break;
		case S_PKM:
			stage_str = STR(S_PKM);
			if (start_complete == S_START)
				event = NC_AuthStart;
			else
				event = NC_AuthComplete;
			break;
		case S_REG:
			stage_str = STR(S_REG);
			if (start_complete == S_START)
				event = NC_RegStart;
			else
				event = NC_RegComplete;
			break;
		case S_DSX:
			#if 1
			stage_str = STR(S_DSX);
			if (start_complete == S_START)
				event = NC_DsxStart;
			else
				event = NC_DsxComplete;
			#endif
			break;
	}

	xprintf(SDK_DBG, "Connecting %s(%s)\n",
		stage_str, start_complete==S_START ? "Start" : "Complete");

	if (event)
		sdk_ind_event(dev_idx, event);
	return ret;
}
