// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "global.h"
#include "gcttype.h"
#include "wimax.h"
#include "nds.h"
#include "error.h"
#include "device.h"
#include "io.h"
#include "hci.h"
#include "sdk.h"
#include "log.h"


static void wm_scan_timer_callback(void *data);
static void wm_clean_subscription(struct wimax_s *wm);
static int get_neighbor_info(u8 *bsid, u8 *buf, int len,
	wm_bs_neigh_t *list, int *list_cnt);

static void wm_setup_device(int dev_idx)
{
	device_t *dev;
	int i;

	xfunc_in("dev=%d", dev_idx);
	if (!(dev = dm_get_dev(dev_idx)))
		return;

	dev->wimax = (wimax_t *) sdk_malloc(sizeof(wimax_t));
	memset(dev->wimax, 0, sizeof(wimax_t));

	for (i = 0; i < WM_USING_LOCK_ENTRIES; i++)
		pthread_mutex_init(&dev->wimax->using_lock[i], NULL);
	pthread_mutex_init(&dev->wimax->fsm_lock, NULL);

	dm_put_dev(dev_idx);
	xfunc_out();
}

void wm_cleanup_device(int dev_idx)
{
	device_t *dev;
	int i;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return;
	if (dev->wimax) {
		for (i = 0; i < WM_USING_LOCK_ENTRIES; i++)
			wm_in_wimax(dev->wimax, i);
		nds_deinit(dev_idx);
		wm_clean_subscription(dev->wimax);

		for (i = 0; i < WM_USING_LOCK_ENTRIES; i++)
			pthread_mutex_destroy(&dev->wimax->using_lock[i]);
		pthread_mutex_destroy(&dev->wimax->fsm_lock);

		sdk_free(dev->wimax);
		dev->wimax = NULL;
	}
	dm_put_dev(dev_idx);
	xfunc_out();
}

void wm_open_device(int dev_idx)
{
	xfunc_in("dev=%d", dev_idx);

	wm_setup_device(dev_idx);

	nds_init(dev_idx);

	dm_put_dev(dev_idx);
	xfunc_out();
}

int wm_set_scan_type(int dev_idx, wimax_scan_type_t type)
{
	device_t *dev;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	dev->wimax->scan.type = type;
	dm_put_dev(dev_idx);
	return 0;
}

static int wm_set_scan_interval(int dev_idx, u32 interval_sec)
{
	device_t *dev;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	dev->wimax->scan.scan_interval_sec = interval_sec;
	dm_put_dev(dev_idx);
	return 0;
}

void wm_init_scan(int dev_idx)
{
	device_t *dev;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return;

	init_timer(&dev->wimax->scan.timer, wm_scan_timer_callback, (void *)dev_idx);

	wm_set_scan_interval(dev_idx, WM_DEFAULT_SCAN_INTERVAL_SEC);
	wm_set_scan_type(dev_idx, wm_scan_all_subscriptions);

	dm_put_dev(dev_idx);
	xfunc_out();
}

void wm_deinit_scan(int dev_idx)
{
	device_t *dev;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return;

	del_timer(&dev->wimax->scan.timer);
	dm_put_dev(dev_idx);
	xfunc_out();
}

void wm_close_device(int dev_idx)
{
	xfunc_in("dev=%d", dev_idx);

	wm_cleanup_device(dev_idx);
	xfunc_out();
	dm_put_dev(dev_idx);
}

int wm_update_device_info(int dev_idx, tlv_t *tlv, int nr_tlv)
{
	device_t *dev;
	wimax_t *wm;
	wm_rev_info_t *revi;
	u8 buf[HCI_MAX_PACKET];
	u8 T;
	u16 L;
	u8 *V;
	int ret, i, lmask = SDK_DBG;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	assert(dev->wimax->subs_list.head.prev && dev->wimax->subs_list.head.next);
	wm = dev->wimax;
	revi = &dev->wimax->dev_info.revision;
	
	ret = hci_req_getinfo(dev_idx, buf, tlv, nr_tlv);
	if (ret < 0)
		goto out;

	for (i = 0; i < nr_tlv; i++) {
		T = tlv[i].T;
		L = tlv[i].L;
		V = tlv[i].V;
		switch (T) {
			case TLV_T(T_MAX_SUBSCRIPTION):
				wm->dev_info.max_subs_supported = *V;
				xprintf(lmask, "T_MAX_SUBSCRIPTION=%d\n", *V);
				break;
			case TLV_T(T_MAX_SF):
				wm->dev_info.max_service_flow_supported = *V;
				xprintf(lmask, "T_MAX_SF=%d\n", *V);
				break;
			case TLV_T(T_PHY_TYPE):
				wm->dev_info.phy_type = *V;
				xprintf(lmask, "T_PHY_TYPE=%d\n", *V);
				break;
			case TLV_T(T_PKM):
				wm->dev_info.pkm_support.pkm_support = *V;
				xprintf(lmask, "T_PKM=%d\n", *V);
				break;
			case TLV_T(T_AUTH_POLICY):
				wm->dev_info.auth_support.auth_policy_support = *V;
				xprintf(lmask, "T_AUTH_POLICY=%d\n", *V);
				break;
			case TLV_T(T_CS_TYPE):
				wm->dev_info.cs_type_supported.supported_cs = U82U16(V);
				xprintf(lmask, "T_CS_TYPE=%d\n", U82U16(V));
				break;
			case TLV_T(T_VENDOR_NAME):
				memcpy(&wm->dev_info.vendor_name, V, L);
				wm->dev_info.vendor_name[L] = 0;
				xprintf(lmask, "T_VENDOR_NAME=%s\n", &wm->dev_info.vendor_name);
				break;
			case TLV_T(T_MOD_NAME):
				memcpy(&wm->dev_info.model_name, V, L);
				wm->dev_info.model_name[L] = 0;
				xprintf(lmask, "T_MOD_NAME=%s\n", &wm->dev_info.model_name);
				break;
			case TLV_T(T_SUBSCRIPTION_LIST):
				xprintf(lmask, "T_SUBSCRIPTION_LIST: L=%d\n", L);
				ret = wm_update_subscription(dev_idx, V, L);
				break;
			case TLV_T(T_MAC_ADDRESS):
				memcpy(&wm->dev_info.device_mac, V, L);
				xprintf(lmask, "T_MAC_ADDRESS=%02x:%02x:%02x:%02x:%02x:%02x\n",
					V[0],V[1],V[2],V[3],V[4],V[5]);
				break;
			case TLV_T(T_BOOTLOAD_VER):
				memcpy(&revi->bl_ver, V, TLV_L(T_BOOTLOAD_VER));
				xprintf(SDK_INFO, "BL: %d.%d.%d.%d\n", V[0], V[1], V[2], V[3]);
				break;
			case TLV_T(T_RELEASE_NUMBER):
				memcpy(&revi->rel_ver, V, TLV_L(T_RELEASE_NUMBER));
				xprintf(SDK_INFO, "REL: %d.%d.%d.%d\n", V[0], V[1], V[2], V[3]);
				break;
			default:
				xprintf(SDK_ERR, "Unknown Get-Info type(0x%02x)\n", T);
				ret = -1;
				goto out;
		}
	}

out:
	dm_put_dev(dev_idx);
	xfunc_out();
	return ret;
}

static int wm_get_mac_rev_info(int dev_idx)
{
	device_t *dev;
	wm_rev_info_t *revi;
	u8 buf[HCI_MAX_PACKET];
	tlv_t tlv[4];
	u8 T;
	u16 L;
	u8 *V;
	int ret, i, pos = 0, tmp, lmask = SDK_INFO;

	xfunc_in();

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	revi = &dev->wimax->dev_info.revision;

	tlv[pos++].T = TLV_T(T_FW_REVISION);
	tlv[pos++].T = TLV_T(T_PHY_HW_REVISION);
	tlv[pos++].T = TLV_T(T_CAPABILITY);

	assert(pos <= numof_array(tlv));

	ret = hci_req_getinfo(dev_idx, buf, tlv, pos);

	if (ret < 0) {
		if (-ETIMEDOUT == ret) {
			xprintf(SDK_NOTICE, "MAC GetInfo Timeout\n");	
			ret = 0;
			goto out;
		}
		
		xprintf(SDK_ERR, "MAC GetInfo Error\n");
		goto out;
	}

	for (i = 0; i < pos; i++) {
		T = tlv[i].T;
		L = tlv[i].L;
		V = tlv[i].V;
		switch (T) {
			case TLV_T(T_CAPABILITY):
				memcpy(&tmp, V, TLV_L(T_CAPABILITY));
				xprintf(lmask, "T_CAPABILITY=0x%08X\n", DB2H(tmp));
				break;
			case TLV_T(T_FW_REVISION):
				memcpy(&revi->fw_ver, V, TLV_L(T_FW_REVISION));
				xprintf(lmask, "T_FW_REVISION=0x%08X\n", revi->fw_ver);
				break;
			case TLV_T(T_PHY_HW_REVISION):
				memcpy(&revi->phy_hw_ver, V, TLV_L(T_PHY_HW_REVISION));
				xprintf(lmask, "T_PHY_HW_REVISION=0x%08X\n", revi->phy_hw_ver);
				break;
			default:
				xprintf(SDK_ERR, "Unknown Get-Info type(0x%02x)\n", T);
				ret = -1;
				goto out;
		}
	}
out:
	xfunc_out("ret=%d", ret);
	dm_put_dev(dev_idx);
	return ret;
}

int wm_init_device_info(int dev_idx)
{
	tlv_t tlv[16];
	int ret, pos = 0;

	tlv[pos++].T = TLV_T(T_MAX_SUBSCRIPTION);
	tlv[pos++].T = TLV_T(T_MAX_SF);
	tlv[pos++].T = TLV_T(T_PHY_TYPE);
	tlv[pos++].T = TLV_T(T_PKM);
	tlv[pos++].T = TLV_T(T_AUTH_POLICY);
	tlv[pos++].T = TLV_T(T_CS_TYPE);
	tlv[pos++].T = TLV_T(T_VENDOR_NAME);
	tlv[pos++].T = TLV_T(T_MOD_NAME);
	tlv[pos++].T = TLV_T(T_SUBSCRIPTION_LIST);
	tlv[pos++].T = TLV_T(T_MAC_ADDRESS);
	tlv[pos++].T = TLV_T(T_BOOTLOAD_VER);
	tlv[pos++].T = TLV_T(T_RELEASE_NUMBER);

	assert(pos <= numof_array(tlv));

	if ((ret = wm_update_device_info(dev_idx, tlv, pos)) < 0)
		return -1;

	ret = wm_get_mac_rev_info(dev_idx);

	return ret;
}

static bool compare_nspid(wm_nsp_identifier_t *s1, wm_nsp_identifier_t *s2)
{
	if (!memcmp(s1->id, s2->id, NSP_ID_SIZE)
		#if 0
		&& 	!strcasecmp((char *)s1->name, (char *)s2->name)
		#endif
	)
		return TRUE;
	return FALSE;
}

static int reset_selected_subsctiption(int dev_idx, wm_nsp_identifier_t *hnspid)
{
	device_t *dev;
	wimax_t *wm;
	struct list_head *head;
	wm_subscription_info_t *ss_info;
	int ret = 0;

	xfunc_in();
	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	wm = dev->wimax;

	xprintf(SDK_DBG, "OLD: H-NSP-ID %02X%02X%02X(%s)\n",
		hnspid->id[0], hnspid->id[1], hnspid->id[2], hnspid->name);
	/*Look up new subscription.*/
	pthread_mutex_lock(&wm->subs_list.lock);
	head = &wm->subs_list.head;
	wm->scan.selected_subs = NULL;
	list_for_each_entry(ss_info, head, list) {
		xprintf(SDK_DBG, "NEW: H-NSP-ID %02X%02X%02X(%s)\n",
			ss_info->subscription_id.hnspid.id[0],
			ss_info->subscription_id.hnspid.id[1],
			ss_info->subscription_id.hnspid.id[2],
			ss_info->subscription_id.hnspid.name);
		if (compare_nspid(hnspid, &ss_info->subscription_id.hnspid)) {
			wm->scan.selected_subs = ss_info;
			break;
		}
	}
	if (!wm->scan.selected_subs) {
		wm->scan.selected_subs = list_entry(head->next, wm_subscription_info_t, list);
		xprintf(SDK_DBG, "First subscription list has been set(%s:%s)\n",
			wm->scan.selected_subs->subscription_id.user_hnai,
			wm->scan.selected_subs->subscription_id.hnspid.name);
	}
	pthread_mutex_unlock(&wm->subs_list.lock);

	dm_put_dev(dev_idx);
	xfunc_out("ret=%d", ret);
	return 0;
}

int wm_sync_subscription(int dev_idx)
{
	device_t *dev;
	wimax_t *wm;
	wm_nsp_identifier_t hnspid;
	tlv_t tlv[1];
	int ret, pos = 0;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;
	wm = dev->wimax;

	if (wm->scan.selected_subs)
		memcpy(&hnspid, &wm->scan.selected_subs->subscription_id.hnspid, sizeof(hnspid));
	else
		memset(&hnspid, 0, sizeof(hnspid));

	tlv[pos++].T = TLV_T(T_SUBSCRIPTION_LIST);

	if (!(ret = wm_update_device_info(dev_idx, tlv, pos)))
		ret = reset_selected_subsctiption(dev_idx, &hnspid);

	return ret;
}

int wm_active_scan_interval(int dev_idx, u32 interval_sec)
{
	device_t *dev;

	xfunc_in("interval_sec=%d", interval_sec);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	if (interval_sec)
		start_timer(&dev->wimax->scan.timer, interval_sec*1000);
	else
		stop_timer(&dev->wimax->scan.timer);

	wm_set_scan_interval(dev_idx, interval_sec);
	xfunc_out();
	dm_put_dev(dev_idx);
	return 0;
}

int wm_req_scan(int dev_idx)
{
	device_t *dev;
	wimax_t *wm;
	wm_subscription_info_t *ss_info;
	u8 scan_type;
	u8 *home_nsp_id = NULL, *usr_home_nai = NULL;
	int ret = 0;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;
	wm = dev->wimax;

	xfunc_in("m_s=%d, c_s=%d", dev->fsm.m_status, dev->fsm.c_status);

	if (dev->fsm.m_status != M_OPEN_ON && dev->fsm.m_status != M_SCAN)
		goto out;

	switch (dev->wimax->scan.type) {
		case wm_scan_all_channels:
			scan_type = W_SCAN_ALL_CHANNEL;
			break;
		case wm_scan_all_subscriptions:
			scan_type = W_SCAN_ALL_SUBSCRIPTION;
			break;
		case wm_scan_curr_subscription:
			scan_type = W_SCAN_SPECIFIED_SUBSCRIPTION;
			if (!(ss_info = dev->wimax->scan.selected_subs)) {
				xprintf(SDK_DBG, "There is no selected subscription.");
				ret = -EAGAIN;
				goto out;
			}
			home_nsp_id = ss_info->subscription_id.hnspid.id;
			usr_home_nai = ss_info->subscription_id.user_hnai;
			xprintf(SDK_INFO, "[%d] H_NSPID=0x%08X, H_NAI=%s\n",
				dev_idx, U82U24(home_nsp_id), usr_home_nai);
			break;
		default:
			xprintf(SDK_ERR, "[%d] Unknown scan type(%d)\n",
				dev_idx, dev->wimax->scan.type);
			ret = -1;
			goto out;
	}

	if ((ret = sdk_set_status(NULL, dev_idx, M_SCAN, C_INIT)) < 0)
		goto out;

	ret = nds_req_scan(dev_idx, scan_type, home_nsp_id, usr_home_nai);
	dev->wimax->scan.last_scan_ms = (unsigned int)gettimemsofday();
out:
	dm_put_dev(dev_idx);
	xfunc_out("ret=%d", ret);
	return ret;
}

int wm_net_search_scan(int dev_idx, wimax_scan_type_t scantype)
{
	device_t *dev;
	wimax_t *wm;
	wm_scan_t *scan;
	int m_status, c_status;
	int ret;

	xfunc_in("scantype=%d", scantype);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;
	wm = dev->wimax;

	wm_active_scan_interval(dev_idx, 0);

	ret = wm_set_scan_type(dev_idx, scantype);

	scan = &dev->wimax->scan;

	sdk_get_status(NULL, dev_idx, &m_status, &c_status);
	switch (m_status) {
		case M_SCAN:
			ret = start_timer(&scan->timer, 500);
			break;
		case M_OPEN_ON:
			pthread_mutex_lock(&wm->fsm_lock);
			ret = wm_req_scan(dev_idx);
			pthread_mutex_unlock(&wm->fsm_lock);
			break;
		default:
			xprintf(SDK_ERR, "Invalid Connection Status(%d)!\n", m_status);
			ret = -1;
	}

	dm_put_dev(dev_idx);
	xfunc_out();
	return ret;
}

int wm_cancel_scan(int dev_idx)
{
	int ret;

	if (!sdk_get_rw_handle())
		return 0;

	ret = hci_send(dev_idx, WIMAX_SCAN_CANCEL, NULL, 0);
	if (!ret) {
		xprintf(SDK_STD_ERR, "[%d] hci_send(%d)\n", dev_idx, ret);
		ret = sdk_set_errno(ERR_STD);
	}
	return ret;
}

void wm_clean_scan_info(int dev_idx)
{
	device_t *dev;

	if (!(dev = dm_get_dev(dev_idx)))
		return;

	nds_clean_scan_list(dev->wimax);
	dm_put_dev(dev_idx);
}

static void wm_scan_timer_callback(void *data)
{
	int dev_idx = (int) data;
	device_t *dev;
	wm_scan_t *scan;
	int m_status, c_status;

	if (!(dev = dm_get_dev(dev_idx)))
		return;

	xfunc_in("dev=%d", dev_idx);

	scan = &dev->wimax->scan;

	if (dev && dev->inserted && dev->open_cnt) {
		sdk_get_status(NULL, dev_idx, &m_status, &c_status);
		if (m_status == M_SCAN) {
			xprintf(SDK_DBG, "m_status=%d, c_status=%d\n", m_status, c_status);
			start_timer(&scan->timer, 500);
			goto out;
		}
		if (m_status == M_OPEN_ON && wm_req_scan(dev_idx) == -EAGAIN) {
			xprintf(SDK_DBG, "wm_req_scan EAGAIN %d sec.\n", scan->scan_interval_sec);
			wm_req_interval_scan(dev_idx, scan->scan_interval_sec*1000);
		}
	}
out:
	xfunc_out();
	dm_put_dev(dev_idx);
}

int wm_req_interval_scan(int dev_idx, int interval_milisec)
{
	device_t *dev;
	wm_scan_t *scan;
	int elapsed_ms, interval_sec;
	int ret = 0;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	xfunc_in("dev=%d, ms=%d", dev_idx, interval_milisec);
	if (dev->a_status == AM_Close) {
		xprintf(SDK_DBG, "SDK is in closing\n");
		goto out;
	}
	
	scan = &dev->wimax->scan;
	interval_sec = scan->scan_interval_sec;

	if (interval_sec == 0) {
		xprintf(SDK_DBG, "interval_ms is 0\n");
		goto out;
	}

	if (interval_milisec > 0) {
		ret = start_timer(&scan->timer, interval_milisec);
		goto out;
	}

	interval_milisec = interval_sec*1000;

	elapsed_ms = (unsigned int)gettimemsofday() - scan->last_scan_ms;
	if (elapsed_ms < interval_milisec)
		ret = start_timer(&scan->timer, interval_milisec-elapsed_ms);
	else
		ret = wm_req_scan(dev_idx);
out:
	xfunc_out("ret=%d", ret);
	dm_put_dev(dev_idx);
	return ret;
}

void wm_clean_subscription(struct wimax_s *wm)
{
	wm_subscription_info_t *ss_info, *tmp;
	struct list_head *head = &wm->subs_list.head;
	nsp_list_data_t *nsp_data, *tmp2;
	nap_list_data_t *nap_data, *tmp3;

	pthread_mutex_lock(&wm->subs_list.lock);
	list_for_each_entry_safe(ss_info, tmp, head, list) {
		xprintf(SDK_DBG, "Clean user_hnai=%s\n", 
			ss_info->subscription_id.user_hnai);
		list_for_each_entry_safe(nsp_data, tmp2, &ss_info->nsp_data_head, list) {
			list_del(&nsp_data->list);
			sdk_free(nsp_data);
		}
		INIT_LIST_HEAD(&ss_info->nap_data_head);

		list_for_each_entry_safe(nap_data, tmp3, &ss_info->nap_data_head, list) {
			list_del(&nap_data->list);
			sdk_free(nap_data);
		}
		INIT_LIST_HEAD(&ss_info->nap_data_head);

		list_del(&ss_info->list);
			sdk_free(ss_info);
	}
	INIT_LIST2(&wm->subs_list);
	pthread_mutex_unlock(&wm->subs_list.lock);
}

int wm_update_subscription(int dev_idx, u8 *buf, int len)
{
	device_t *dev;
	struct wimax_s *wm;
	wm_subscription_info_t *ss_info;
	u8 subs_idx = 1;
	struct list_head *head;
	nsp_list_data_t *nsp_data;
	nap_list_data_t *nap_data;
	u8 T, diff_T, *V;
	u16 L;
	u16 flag;
	int pos = 0, getn, ret = 0;
	int lmask = SDK_INFO;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	wm = dev->wimax;
	head = &wm->subs_list.head;

	assert(head->prev && head->next);

	xfunc_in("dev=%d, len=%d", dev_idx, len);
	
	wm_clean_subscription(wm);

	pthread_mutex_lock(&wm->subs_list.lock);
	
	while (pos < len) {
		ss_info = (wm_subscription_info_t *) sdk_malloc(sizeof(wm_subscription_info_t));
		ss_info->idx = subs_idx++;

		pos += hci_get_tlv(&buf[pos], &T, &L, &V);
		if (T != (diff_T=TLV_T(T_H_NSPID))) goto diff_type;
		memcpy(&ss_info->subscription_id.hnspid.id, V, TLV_L(T_H_NSPID));
		xprintf(lmask, "T_H_NSPID=0x%08X\n", U82U24(V));

		pos += hci_get_tlv(&buf[pos], &T, &L, &V);
		if (T != (diff_T=TLV_T(T_NSP_NAME))) goto diff_type;
		memcpy(ss_info->subscription_id.hnspid.name, V, L);
		ss_info->subscription_id.hnspid.name[L] = 0;
		xprintf(lmask, "Operator Name=%s\n", ss_info->subscription_id.hnspid.name);

		pos += hci_get_tlv(&buf[pos], &T, &L, &V);
		if (T != (diff_T=TLV_T(T_SUBSCRIPTION_NAME))) goto diff_type;
		memcpy(&ss_info->subscription_id.user_hnai, V, L);
		ss_info->subscription_id.user_hnai[L] = 0;
		xprintf(lmask, "Subscription Name=%s\n", ss_info->subscription_id.user_hnai);

		xprintf(SDK_INFO, "[%d] H_NSPID=0x%08X, H_NSPNAME=%s, H_NAI=%s\n",
			dev_idx, 
			U82U24(ss_info->subscription_id.hnspid.id),
			&ss_info->subscription_id.hnspid.name,
			&ss_info->subscription_id.user_hnai);

		pos += hci_get_tlv(&buf[pos], &T, &L, &V);
		if (T != (diff_T=TLV_T(T_SUBSCRIPTION_FLAG))) goto diff_type;
		memcpy(&flag, V, TLV_L(T_SUBSCRIPTION_FLAG));
		flag = B2H(flag);
		if (flag & 0x0001 )
			ss_info->auto_selection = TRUE;
		if (flag & 0x0002 )
			ss_info->auto_selection_configurable = TRUE;
		if (flag & 0x0004 )
			ss_info->auto_connect_at_home = TRUE;
		if (flag & 0x0008 )
			ss_info->auto_connect_when_roam = TRUE;
		if (flag & 0x0010 )
			ss_info->subs_locked = TRUE;
		if (flag & 0x0020 )
			ss_info->activated = TRUE;

		INIT_LIST_HEAD(&ss_info->nsp_data_head);
		do {
			getn = hci_get_tlv(&buf[pos], &T, &L, &V);
			if (T != TLV_T(T_V_NSPID))
				break;
			pos += getn;
			nsp_data = (nsp_list_data_t *) sdk_malloc(sizeof(nsp_list_data_t));
			memcpy(&nsp_data->NspIdentifier.id, V, TLV_L(T_V_NSPID));
			xprintf(lmask, "T_V_NSPID=0x%08X\n", U82U24(V));

			pos += hci_get_tlv(&buf[pos], &T, &L, &V);
			if (T != (diff_T=TLV_T(T_NSP_NAME))) goto diff_type;
			memcpy(&nsp_data->NspIdentifier.name, V, L);
			nsp_data->NspIdentifier.name[L] = 0;
			xprintf(SDK_INFO, "name=%s\n", &nsp_data->NspIdentifier.name);

			list_add_tail(&nsp_data->list, &ss_info->nsp_data_head);
		} while(pos < len);

		INIT_LIST_HEAD(&ss_info->nap_data_head);
		do {
			getn = hci_get_tlv(&buf[pos], &T, &L, &V);
			if (T != TLV_T(T_NAP_ID))
				break;
			pos += getn;
			nap_data = (nap_list_data_t *) sdk_malloc(sizeof(nap_list_data_t));
			memcpy(&nap_data->nap_id.id, V, TLV_L(T_NAP_ID));
			xprintf(lmask, "T_NAP_ID=0x%08X\n", U82U24(V));

			list_add_tail(&nap_data->list, &ss_info->nap_data_head);
		} while(pos < len);

		list_add_tail(&ss_info->list, head);
		wm->subs_list.cnt++;
	}

	pthread_mutex_unlock(&wm->subs_list.lock);
	
	xfunc_out();
	dm_put_dev(dev_idx);
	return ret;

diff_type:
	pthread_mutex_unlock(&wm->subs_list.lock);
	xprintf(SDK_ERR, "[%d] Diff type(0x%02X != 0x%02X\n", dev_idx, T, diff_T);
	dm_put_dev(dev_idx);
	return -1;
}

wm_subscription_info_t *wm_get_subscription(int dev_idx, u8 subs_idx, u8 *hnspid)
{
	device_t *dev;
	wimax_t *wm;
	struct list_head *head;
	wm_subscription_info_t *ss_info, *ret_d_ss_info = NULL;

	if (!(dev = dm_get_dev(dev_idx)))
		return NULL;

	wm = dev->wimax;
	head = &wm->subs_list.head;

	xfunc_in("H_NSP_ID=0x%08X", U82U24(hnspid));

	pthread_mutex_lock(&wm->subs_list.lock);
	list_for_each_entry(ss_info, head, list) {
		if (subs_idx && ss_info->idx != subs_idx)
			continue;
		if (!memcmp(ss_info->subscription_id.hnspid.id, hnspid, NSP_ID_SIZE)) {
			ret_d_ss_info = ss_info;
			break;
		}
	}
	pthread_mutex_unlock(&wm->subs_list.lock);

	if (ret_d_ss_info == NULL) {
		xprintf(SDK_ERR, "[%d] H_NSP_ID(0x%08X) is not found\n", dev_idx, U82U24(hnspid));
		sdk_set_errno(ERR_NO_NSPID);
	}
	xfunc_out();
	dm_put_dev(dev_idx);
	return ret_d_ss_info;
}

wm_subscription_info_t *wm_get_activated_subs(int dev_idx)
{
	device_t *dev;
	wimax_t *wm;
	struct list_head *head;
	wm_subscription_info_t *ss_info, *ret_d_ss_info = NULL;

	if (!(dev = dm_get_dev(dev_idx)))
		return NULL;
	xfunc_in();

	wm = dev->wimax;
	head = &wm->subs_list.head;

	pthread_mutex_lock(&wm->subs_list.lock);
	list_for_each_entry(ss_info, head, list) {
		if (ss_info->activated) {
			ret_d_ss_info = ss_info;
			break;
		}
	}
	pthread_mutex_unlock(&wm->subs_list.lock);

	xfunc_out();
	dm_put_dev(dev_idx);
	return ret_d_ss_info;
}

int wm_combine_level(int dev_idx, int c0, int c1)
{
	device_t *dev;
	u32 fw_mac_ver;
	int max, diff, offset;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;
	fw_mac_ver = DB2H(dev->wimax->dev_info.revision.fw_ver);

	if (c0 > c1) {
		max = c0;
		diff = c0 - c1;
	}
	else {
		max = c1;
		diff = c1 - c0;
	}

	if (fw_mac_ver < 0x01090104/*1.9.1.4*/) {
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
	dm_put_dev(dev_idx);
	return max;
}

u8 wm_convert_txpower(int txpower)
{
	u8 ret_txpower;
	
	if (txpower <= -168)
		ret_txpower = 0x00;
#if 0
	else if (txpower >= 87)
		ret_txpower = 0xff;
#endif
	else
		ret_txpower = (u8)(txpower+168);
	
	return ret_txpower;
}

u8 wm_convert_cinr(s8 cinr)
{
	u8 ret_cinr = 0;

	if (cinr <= -10)
		ret_cinr = 0x00;
#if 0
	else if (cinr >= 53)
		ret_cinr = 0x3F;
#endif
	else
		ret_cinr = (char)(cinr+10);

	return ret_cinr;
}

u8 wm_convert_rssi(s8 rssi)
{
	u8 ret_rssi = 0;

	if (rssi <= -123)
		ret_rssi = 0x00;
#if 0
	else if (rssi >= -40)
		ret_rssi = 0x53;
#endif
	else
		ret_rssi = (char)(123+rssi);

	return ret_rssi;
}

int wm_get_network_type(struct wimax_s *wm, u8 *nspid)
{
	struct list_head *head = &wm->subs_list.head;
	wm_subscription_info_t *ss_info;
	nsp_list_data_t *nsp_data;
	nap_list_data_t *nap_data;
	int net_type = WIMAX_API_UNKNOWN;

	xfunc_in("NSPID=0x%02X%02X%02X", nspid[0], nspid[1], nspid[2]);

	pthread_mutex_lock(&wm->subs_list.lock);
	list_for_each_entry(ss_info, head, list) {
		if (!memcmp(ss_info->subscription_id.hnspid.id, nspid, NSP_ID_SIZE)) {
			net_type = WIMAX_API_HOME;
			goto out;
		}

		list_for_each_entry(nsp_data, &ss_info->nsp_data_head, list) {
			if (!memcmp(nsp_data->NspIdentifier.id, nspid, NSP_ID_SIZE)) {
				net_type = WIMAX_API_ROAMING_PARTNER;
				goto out;
			}
		}

		list_for_each_entry(nap_data, &ss_info->nap_data_head, list) {
			if (!memcmp(nap_data->nap_id.id, nspid, NAP_ID_SIZE)) {
				net_type = WIMAX_API_PARTNER;
				goto out;
			}
		}
	}
out:
	pthread_mutex_unlock(&wm->subs_list.lock);
	xfunc_out();
	return net_type;
}

struct wm_nsp_s *wm_lookup_nsp_by_name(struct wimax_s *wm, u8 *nsp_name)
{
	struct list_head *head = &wm->scan_list.head;
	wm_nsp_t *nsp, *rnsp = NULL;

	pthread_mutex_lock(&wm->scan_list.lock);
	list_for_each_entry(nsp, head, list) {
		if (!strcmp((char *)nsp->name, (char *)nsp_name))
			rnsp = nsp;
	}
	pthread_mutex_unlock(&wm->scan_list.lock);
	return rnsp;
}

struct wm_nsp_s *wm_lookup_nsp_by_id(struct wimax_s *wm, u8 *nspid)
{
	struct list_head *head = &wm->scan_list.head;
	wm_nsp_t *nsp, *rnsp = NULL;

	pthread_mutex_lock(&wm->scan_list.lock);
	list_for_each_entry(nsp, head, list) {
		if (nsp->id == U82U24(nspid))
			rnsp = nsp;
	}
	pthread_mutex_unlock(&wm->scan_list.lock);
	return rnsp;
}

int wm_connect_network(int dev_idx, u8 *hnspid, u8 *vnspid)
{
	device_t *dev;
	wimax_t *wm;
	u8 param[HCI_MAX_PARAM];
	u8 *usr_hnai = NULL;
	int usr_hnai_len = 0;
	int len = 0, ret = 0;
	int lmask = SDK_DBG;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	xfunc_in("dev=%d", dev_idx);

	wm = dev->wimax;
	if (wm->scan.selected_subs) {
		usr_hnai = wm->scan.selected_subs->subscription_id.user_hnai;
		usr_hnai_len = strlen((char *)usr_hnai) + 1/*null*/;
	}

	stop_timer(&wm->scan.timer);

	xprintf(lmask, "H_NSPID=0x%06X, V_NSPID=0x%06X\n", U82U24(hnspid), U82U24(vnspid));
	len += hci_set_tlv(&param[len], TLV_T(T_H_NSPID), TLV_L(T_H_NSPID), hnspid);
	len += hci_set_tlv(&param[len], TLV_T(T_V_NSPID), TLV_L(T_V_NSPID), vnspid);
	if (E_EAP_TLS_ENABLED(dev) && usr_hnai) {
		len += hci_set_tlv(&param[len], TLV_T(T_SUBSCRIPTION_NAME), usr_hnai_len, usr_hnai);
		xprintf(lmask, "SUBSCRIPTION_NAME=%s\n", usr_hnai);
	}

	pthread_mutex_lock(&wm->fsm_lock);
	if ((ret = sdk_set_status(NULL, dev_idx, M_CONNECTING, C_CONNSTART)) < 0) {
		pthread_mutex_unlock(&wm->fsm_lock);
		goto out;
	}

	wm->conn.hnspid = U82U24(hnspid);
	wm->conn.vnspid = U82U24(vnspid);
	if ((ret = hci_send(dev_idx, WIMAX_CONNECT, param, len)) == len)
		ret = 0;
	else {
		sdk_set_status(NULL, dev_idx, M_OPEN_ON, C_INIT);
		ret = -1;
	}
	pthread_mutex_unlock(&wm->fsm_lock);
out:
	xfunc_out("ret=%d", ret);
	dm_put_dev(dev_idx);
	return ret;
}

int wm_re_connect_network(int dev_idx)
{
	device_t *dev;
	wimax_t *wm;
	u8 *selected_hnsp_id;
	u8 *selected_vnsp_id;
	int ret = -1;

	xfunc_in();
	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	wm = dev->wimax;

	if (wm->scan.selected_subs) {
		selected_hnsp_id = wm->scan.selected_subs->subscription_id.hnspid.id;
		selected_vnsp_id = wm->asso_start.net_id.nsp.id;

		ret = wm_connect_network(dev_idx, selected_hnsp_id, selected_vnsp_id);
	}
	xfunc_out("ret=%d", ret);
	return ret;
}

int wm_cr801_re_connect(int dev_idx)
{
	device_t *dev;
	wimax_t *wm;
	u8 type = 0;
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	wm = dev->wimax;

	if (!wm->dev_info.eapp.cr801_server_reject_cnt || 
		wm->dev_info.eapp.cr801_server_reject_cnt > CR801_SERVER_REJECT_LIMIT)
		goto out;

	xprintf(SDK_DBG, "cr801_server_reject_cnt=%d\n",
		wm->dev_info.eapp.cr801_server_reject_cnt);

	type = wm->dev_info.eapp.type;
	if (wm->dev_info.eapp.cr801_server_reject_cnt == CR801_SERVER_REJECT_LIMIT) {
		wm->dev_info.eapp.cr801_mode = CR801_TLS;
		wm->dev_info.eapp.type = W_EAP_TLS;
		wm_set_eap(dev_idx, TRUE);
	}

	ret = wm_re_connect_network(dev_idx);
	wm->dev_info.eapp.type = type;
out:
	dm_put_dev(dev_idx);
	xfunc_out("ret=%d", ret);
	return ret;
}

int wm_get_dhcp_info(int dev_idx)
{
	device_t *dev;
	wimax_t *wm;
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	wm = dev->wimax;
	
	ret = net_get_dhcp_info(dev_idx, &wm->dev_info.dhcp_info);
	dm_put_dev(dev_idx);
	xfunc_out("ret=%d", ret);
	return ret;
}

int wm_disconnect_network(int dev_idx, int timeout_sec)
{
	int ret;

	xfunc_in("dev=%d", dev_idx);

	if (timeout_sec) {
		ret = hci_send_wait(dev_idx, WIMAX_NET_DISCONN, NULL, 0,
			WIMAX_DISCONN_IND, NULL, 0, 0, HCI_INDICATION, timeout_sec);
		if (ret > 0) ret = 0;
	}
	else
		ret = hci_send(dev_idx, WIMAX_NET_DISCONN, NULL, 0);
	
	xfunc_out("ret=%d", ret);
	return ret;
}

int wm_get_rf_state(int dev_idx, rf_stat_t *rf_stat)
{
	u8 buf[HCI_MAX_PACKET];
	tlv_t tlv;
	int ret;

	tlv.T = TLV_T(T_RADIO_STATE);
	ret = hci_req_getinfo(dev_idx, buf, &tlv, 1);
	if (!ret)
		*rf_stat = *tlv.V;
	
	xprintf(SDK_DBG, "ret=%d, rf_stat=%d\n", ret, !ret ? *tlv.V : -1);
	return ret;
}

int wm_set_rf_state(int dev_idx, rf_stat_t rf_stat, int timeout_sec)
{
	device_t *dev;
	u16 cmd;
	int ret;

	xfunc_in("dev=%d, %s", dev_idx, rf_stat==rf_on ? "On" : "Off");
	
	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	if (rf_stat == rf_on)
		cmd = WIMAX_RADIO_ON;
	else {
		cmd = WIMAX_RADIO_OFF;
		wm_set_scan_interval(dev_idx, WM_DEFAULT_SCAN_INTERVAL_SEC);
		wm_set_scan_type(dev_idx, wm_scan_all_subscriptions);
	}

	cmd = rf_stat==rf_on ? WIMAX_RADIO_ON : WIMAX_RADIO_OFF;

	if (timeout_sec) {
		ret = hci_send_wait(dev_idx, cmd, NULL, 0,
			WIMAX_RADIO_STATE_IND, NULL, 0, 0, HCI_INDICATION, timeout_sec);
		if (ret > 0) ret = 0;
	}
	else
		ret = hci_send(dev_idx, cmd, NULL, 0);

	xfunc_out("ret=%d", ret);
	dm_put_dev(dev_idx);
	return ret;
}

int wm_set_capability(int dev_idx, u32 capability)
{
	device_t *dev;
	hci_set_arm_capability_t cap = {0};
	u32 fw_cap = 0;
	int ret = 0, len = sizeof(cap);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	xfunc_in("dev=%d", dev_idx);

	if (capability == DEFAULT_CAPABILITY) {
		cap.capability = W_CAPABILITY_ENC_XML;
		if (SDK_USE_EEAP) {
			cap.capability |= W_CAPABILITY_E_EAP_TLS;
			cap.capability |= W_CAPABILITY_E_EAP_AKA;
		}
		cap.capability |= W_CAPABILITY_CAPL_INFO;
	}
	else
		cap.capability = capability;

	cap.capability = DH2B(cap.capability);
	ret = hci_send_wait(dev_idx, WIMAX_ARM_CAPABILITY, (u8 *)&cap, len,
		WIMAX_ARM_CAPABILITY_RESULT, &fw_cap, sizeof(fw_cap), 0, 0, 5);
	if (len == ret)
		ret = wm_report_capability(dev_idx, (u8 *)&fw_cap, sizeof(fw_cap));
	else {
		if (ret == - ETIMEDOUT) {
			xprintf(SDK_NOTICE, "ARM Capability HCI is not supported!!\n");
			ret = 0;
		}
		else {
			xprintf(SDK_STD_ERR, "[%d] hci_send(%d!=%d)\n", dev_idx, len, ret);
			ret = -1;
		}
	}
	
	xfunc_out("ret=%d", ret);
	dm_put_dev(dev_idx);
	return ret;
}

int wm_report_capability(int dev_idx, u8 *buf, int len)
{
	device_t *dev;
	int ret = 0;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	xfunc_in("dev=%d", dev_idx);

	memcpy(&dev->capability, buf, sizeof(dev->capability));
	dev->capability = DB2H(dev->capability);
	xprintf(SDK_INFO, "ARM Capability=0x%08x\n", dev->capability);
	
	dm_put_dev(dev_idx);
	xfunc_out();
	return ret;
}

int wm_set_eap(int dev_idx, bool enable)
{
	device_t *dev;
	wm_eap_param_t *eapp;
	u8 buf[HCI_MAX_PARAM];
	u8 *pos = buf;
	u8 enable_auth;
	time_t t;
	u8 eap_param = V_PARAM_NV;
	u8 T, *V;
	u16 L;
	int len, ret = 0, lmask = SDK_DBG;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	eapp = &dev->wimax->dev_info.eapp;

	xfunc_in("type=%d, %s", eapp->type, enable ? "On":"Off");

	if (enable) {
		if (IS_EAP_TLS(eapp->type)) {
			if (!SDK_USE_EEAP) {
				if (!E_EAP_TLS_ENABLED(dev))
					xprintf(SDK_ERR, "Embedded EAP TLS were disabled by CM whereas FW supports it!\n");
				else
					xprintf(SDK_ERR, "Both of Host/Embedded EAP TLS were disabled!\n");
				ret = -1;
				goto out;
			}
			else if (!E_EAP_TLS_ENABLED(dev)) {
				xprintf(SDK_ERR, "Embedded EAP TLS is NOT supported!\n");
				ret = -1;
				goto out;
			}
		}
		if (eapp->type == W_EAP_AKA && SDK_USE_EEAP && !E_EAP_AKA_ENABLED(dev)) {
			xprintf(SDK_ERR, "Embedded EAP AKA is NOT supported!\n");
			ret = -1;
			goto out;
		}
		enable_auth = 1;
	}
	else
		enable_auth = 0;

	T = TLV_T(T_ENABLE_AUTH);
	L = TLV_L(T_ENABLE_AUTH);
	V = &enable_auth;
	pos += hci_set_tlv(pos, T, L, V);

	if (E_EAP_TLS_ENABLED(dev)) {
		time(&t);
		xprintf(lmask, "RTC=0x%08x\n", t);
		t = DH2B(t);
		T = TLV_T(T_RTC_TIME);
		L = TLV_L(T_RTC_TIME);
		V = (u8 *) &t;
		pos += hci_set_tlv(pos, T, L, V);
	}
			
	len = pos - buf;
	assert(len < HCI_MAX_PARAM);
	ret = hci_send(dev_idx, WIMAX_SET_INFO, buf, len);
	if (len == ret)
		ret = 0;
	else {
		xprintf(SDK_STD_ERR, "[%d] hci_send(%d!=%d)\n", dev_idx, len, ret);
		ret = -1;
	}
	
	if (enable && E_EAP_TLS_ENABLED(dev) && IS_EAP_TLS(eapp->type)) {
		hci_set_eap_info_t *sei = (hci_set_eap_info_t *) buf;

		sei->type = eapp->type;
		sei->frag_size = H2B(eapp->frag_size);
		sei->use_delimiter = eapp->use_delimiter;
		pos = sei->tlv;

		T = TLV_T(T_DEV_CERT_NULL);
		L = TLV_L(T_DEV_CERT_NULL);
		V = (u8 *) &eapp->dev_cert_null;
		pos += hci_set_tlv(pos, T, L, V);

		T = TLV_T(T_CA_CERT_NULL);
		L = TLV_L(T_CA_CERT_NULL);
		V = (u8 *) &eapp->ca_cert_null;
		pos += hci_set_tlv(pos, T, L, V);

		if (eapp->cr801_enabled) {
			T = TLV_T(T_CR801_MODE);
			L = TLV_L(T_CR801_MODE);
			V = (u8 *) &eapp->cr801_mode;
			pos += hci_set_tlv(pos, T, L, V);
		}

		if (!eapp->use_nv_info) {
			if (strlen(eapp->userid)) {
				if (eap_param == V_PARAM_XML)
					T = T_EAP_FORCE(TLV_T(T_USER_ID));
				else {
					T = TLV_T(T_USER_ID);
					eap_param = V_PARAM_USER;
				}
				L = strlen(eapp->userid) + 1/*null*/;
				V = (u8 *) eapp->userid;
				pos += hci_set_tlv(pos, T, L, V);
				xprintf(lmask, "userid=%s\n", eapp->userid);
			}
			if (strlen(eapp->userid_pwd)) {
				if (eap_param == V_PARAM_XML)
					T = T_EAP_FORCE(TLV_T(T_USER_PASSWD));
				else {
					T = TLV_T(T_USER_PASSWD);
					eap_param = V_PARAM_USER;
				}
				L = strlen(eapp->userid_pwd) + 1/*null*/;
				V = (u8 *) eapp->userid_pwd;
				pos += hci_set_tlv(pos, T, L, V);
				xprintf(lmask, "userid_pwd=%s\n", eapp->userid_pwd);
			}
			if (strlen(eapp->anony_id)) {
				if (eap_param == V_PARAM_XML)
					T = T_EAP_FORCE(TLV_T(T_ANONYMOUS_ID));
				else {
					T = TLV_T(T_ANONYMOUS_ID);
					eap_param = V_PARAM_USER;
				}
				L = strlen(eapp->anony_id) + 1/*null*/;
				V = (u8 *) eapp->anony_id;
				pos += hci_set_tlv(pos, T, L, V);
				xprintf(lmask, "anony_id=%s\n", eapp->anony_id);
			}
			if (strlen(eapp->pri_key_pwd)) {
				if (eap_param == V_PARAM_XML)
					T = T_EAP_FORCE(TLV_T(T_PRIV_KEY_PASSWD));
				else {
					T = TLV_T(T_PRIV_KEY_PASSWD);
					eap_param = V_PARAM_USER;
				}
				L = strlen(eapp->pri_key_pwd) + 1/*null*/;
				V = (u8 *) eapp->pri_key_pwd;
				pos += hci_set_tlv(pos, T, L, V);
				xprintf(lmask, "pri_key_pwd=%s\n", eapp->pri_key_pwd);
			}
		}
		
		T = TLV_T(T_EAP_PARAM);
		L = TLV_L(T_EAP_PARAM);
		V = &eap_param;
		pos += hci_set_tlv(pos, T, L, V);
		xprintf(lmask, "eap_param=%d\n", eap_param);

		T = TLV_T(T_EAP_DEBUG_ON);
		L = TLV_L(T_EAP_DEBUG_ON);
		V = (u8 *) &eapp->log_enabled;
		pos += hci_set_tlv(pos, T, L, V);
		xprintf(lmask, "eap_debug=%d\n", eapp->log_enabled);

		T = TLV_T(T_EAP_RESUMPTION_OFF);
		L = TLV_L(T_EAP_RESUMPTION_OFF);
		V = (u8 *) &eapp->disable_resumptoin;
		pos += hci_set_tlv(pos, T, L, V);
		xprintf(lmask, "disable_resumptoin=%d\n", eapp->disable_resumptoin);

		T = TLV_T(T_EAP_SESSIONTICKET_OFF);
		L = TLV_L(T_EAP_SESSIONTICKET_OFF);
		V = (u8 *) &eapp->disable_sessionticket;
		pos += hci_set_tlv(pos, T, L, V);
		xprintf(lmask, "disable_sessionticket=%d\n", eapp->disable_sessionticket);

		if ((L = strlen(eapp->decoration[DECORATION_IDX1])) != 0) {
			T = TLV_T(T_EAP_DECORATION);
			V = (u8 *) eapp->decoration[DECORATION_IDX1];
			pos += hci_set_tlv(pos, T, L, V);
			xprintf(lmask, "decoration=%d\n", eapp->decoration[DECORATION_IDX1]);
		}

		#if 0
		{
			u32 rand_seed = 0x12345678;
			rand_seed = DH2B(rand_seed);
			T = TLV_T(T_EAP_RAND_SEED);
			L = TLV_L(T_EAP_RAND_SEED);
			V = (u8 *) &rand_seed;
			pos += hci_set_tlv(pos, T, L, V);
			xprintf(lmask, "Set random seed: 0x%08x\n", DH2B(rand_seed));
		}
		#endif
	
		len = pos - buf;
		assert(len < HCI_MAX_PARAM);
		if (eapp->cr801_enabled &&  eapp->cr801_mode == CR801_TLS)
			ret = hci_send(dev_idx, WIMAX_SET_EAP_INFO, buf, len);
		else {
			ret = hci_send_wait(dev_idx, WIMAX_SET_EAP_INFO, buf, len,
				WIMAX_SET_EAP_INFO_RESULT, buf, sizeof(buf), 0, 0, 3);
			if (ret > 0) {
				hci_set_eap_info_result_t *seir = (hci_set_eap_info_result_t *) buf;
				ret = DB2H(seir->code);
				if (ret)
					xprintf(SDK_STD_ERR, "[%d] Set-EAP-Info-Result=%d\n", dev_idx, ret);
			}
			else {
				xprintf(SDK_STD_ERR, "[%d] hci_send_wait(%d!=%d)\n", dev_idx, len, ret);
				ret = -1;
			}
		}
	}
out:
	xfunc_out("ret=%d", ret);
	dm_put_dev(dev_idx);
	return ret;
}

int wm_get_linkinfo(int dev_idx, struct wm_link_status_s *link_s)
{
	u8 buf[HCI_MAX_PACKET];
	tlv_t tlv[4];
	int pos = 0, ret;
	int lmask = SDK_INFO;
	
	xfunc_in("dev=%d", dev_idx);

	tlv[pos++].T = TLV_T(T_RSSI);
	tlv[pos++].T = TLV_T(T_CINR);
	tlv[pos++].T = TLV_T(T_TX_POWER);
	tlv[pos++].T = TLV_T(T_CUR_FREQ);
	
	ret = hci_req_getinfo(dev_idx, buf, tlv, pos);
	if (!ret) {
		pos = 0;
		link_s->rssi = wm_convert_rssi(*tlv[pos].V);
		xprintf(lmask, "[%d] T_RSSI=%d\n", dev_idx, *tlv[pos].V);
		pos++;
		link_s->cinr = wm_convert_cinr(*tlv[pos].V);
		xprintf(lmask, ", T_CINR=%d\n", *tlv[pos].V);
		pos++;
		link_s->tx_power = wm_convert_txpower(*tlv[pos].V);
		xprintf(lmask, ", T_TX_POWER=%d\n", *tlv[pos].V);
		pos++;
		link_s->cur_freq = U82U32(tlv[pos].V);
		xprintf(lmask, ", T_CUR_FREQ=%d\n", link_s->cur_freq);
	}
	
	xfunc_out("ret=%d", ret);
	return ret;
}

int wm_get_netinfo(int dev_idx, struct wm_net_info_s *net_info)
{
	device_t *dev;
	FILE *fp;
	char *dev_file = "/proc/net/dev";
	char buf[512], *p_srch;
	int ret = -1, lmask = SDK_INFO;
	u32 rx_bytes, rx_packets, rx_errs, rx_drop, rx_fifo, rx_frame,
		rx_compressed, rx_multicast, tx_bytes, tx_packets;
		/*tx_errs, tx_drop, tx_fifo, tx_colls, tx_carrier, tx_compressed*/

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	xfunc_in();
	fp = fopen(dev_file, "rt");
	if (fp == NULL) {
		xprintf(SDK_STD_ERR, "[%d] %s fopen NULL\n", dev_idx, dev_file);
		ret = sdk_set_errno(ERR_STD);
		goto out;
	}

	while (fgets(buf, sizeof(buf), fp)) {
		if ((p_srch = strstr(buf, dev->name))) {
			p_srch += strlen(dev->name) + 1/*':'*/;
			#if 0
			sscanf(p_srch, "%lu %lu %*lu %*lu %*lu %*lu %*lu %*lu %lu %lu",
				&rx_bytes, &rx_packets, &tx_bytes, &tx_packets);
			#else
			sscanf(p_srch, "%u %u %u %u %u %u %u %u %u %u",
				&rx_bytes, &rx_packets, &rx_errs, &rx_drop, &rx_fifo, &rx_frame,
				&rx_compressed, &rx_multicast, &tx_bytes, &tx_packets);
			#endif
			xprintf(lmask, "rx_bytes=%u\n", rx_bytes);
			xprintf(lmask, "rx_packets=%u\n", rx_packets);
			xprintf(lmask, "tx_bytes=%u\n", tx_bytes);
			xprintf(lmask, "tx_packets=%u\n", tx_packets);
			net_info->rx_bytes = rx_bytes;
			net_info->rx_packets = rx_packets;
			net_info->tx_bytes = tx_bytes;
			net_info->tx_packets = tx_packets;
			ret = 0;
			break;
		}
	}
	fclose(fp);
out:
	xfunc_out();
	dm_put_dev(dev_idx);
	return ret;
}

int wm_get_rf_info(int dev_idx, GCT_API_RF_INFORM_P rf_info)
{
	device_t *dev;
	wimax_t *wm;
	u8 buf[HCI_MAX_PACKET];
	tlv_t tlv[32];
	u8 T;
	u16 L;
	u8 *V;
	int ret, i, pos = 0;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	assert(dev->wimax->subs_list.head.prev && dev->wimax->subs_list.head.next);
	wm = dev->wimax;

	tlv[pos++].T = TLV_T(T_UL_PERM_BASE);
	tlv[pos++].T = TLV_T(T_PER_RECEIVE_COUNT);
	tlv[pos++].T = TLV_T(T_PER_ERROR_COUNT);
	tlv[pos++].T = TLV_T(T_DL_PERM_BASE);
	tlv[pos++].T = TLV_T(T_CURR_PREAMBLE_IDX);
	tlv[pos++].T = TLV_T(T_PREV_PREAMBLE_IDX);
	tlv[pos++].T = TLV_T(T_HO_CNT);
	tlv[pos++].T = TLV_T(T_HO_FAIL_CNT);
	tlv[pos++].T = TLV_T(T_RESYNC_CNT);
	tlv[pos++].T = TLV_T(T_HO_SIGNAL_LATENCY);
	tlv[pos++].T = TLV_T(T_CINR);
	tlv[pos++].T = TLV_T(T_CINR_2);
	tlv[pos++].T = TLV_T(T_RSSI);
	tlv[pos++].T = TLV_T(T_RSSI_2);
	tlv[pos++].T = TLV_T(T_PER);
	tlv[pos++].T = TLV_T(T_POWER_CONTROL_MODE);
	tlv[pos++].T = TLV_T(T_TX_POWER);
	tlv[pos++].T = TLV_T(T_TX_POWER_MAX);
	tlv[pos++].T = TLV_T(T_UL_BURST_DATA_FEC_SCHEME);
	tlv[pos++].T = TLV_T(T_DL_BURST_DATA_FEC_SCHEME);
	tlv[pos++].T = TLV_T(T_UL_BURST_DATA_UIUC);
	tlv[pos++].T = TLV_T(T_DL_BURST_DATA_UIUC);
	tlv[pos++].T = TLV_T(T_CUR_FREQ);

	assert(pos <= numof_array(tlv));

	ret = hci_req_getinfo(dev_idx, buf, tlv, pos);
	if (ret < 0)
		goto out;

	for (i = 0; i < pos; i++) {
		T = tlv[i].T;
		L = tlv[i].L;
		V = tlv[i].V;
		switch (T) {
			case TLV_T(T_UL_PERM_BASE):
				rf_info->ULPermBase = *V;
				xprintf(SDK_DBG, "T_UL_PERM_BASE=%d\n", rf_info->ULPermBase);
				break;
			case TLV_T(T_PER_RECEIVE_COUNT):
				memcpy(&rf_info->nPERReceiveCount, V, TLV_L(T_PER_RECEIVE_COUNT));
				rf_info->nPERReceiveCount = DB2H(rf_info->nPERReceiveCount);
				xprintf(SDK_DBG, "T_PER_RECEIVE_COUNT=%d\n", rf_info->nPERReceiveCount);
				break;
			case TLV_T(T_PER_ERROR_COUNT):
				memcpy(&rf_info->nPERErrorCount, V, TLV_L(T_PER_ERROR_COUNT));
				rf_info->nPERErrorCount = DB2H(rf_info->nPERErrorCount);
				xprintf(SDK_DBG, "T_PER_ERROR_COUNT=%d\n", rf_info->nPERErrorCount);
				break;
			case TLV_T(T_DL_PERM_BASE):
				rf_info->DLPermBase = *V;
				xprintf(SDK_DBG, "T_DL_PERM_BASE=%d\n", rf_info->DLPermBase);
				break;
			case TLV_T(T_CURR_PREAMBLE_IDX):
				rf_info->CurrentPreambleIndex = *V;
				xprintf(SDK_DBG, "T_CURR_PREAMBLE_IDX=%d\n",
					rf_info->CurrentPreambleIndex);
				break;
			case TLV_T(T_PREV_PREAMBLE_IDX):
				rf_info->PreviousPreambleIndex = *V;
				xprintf(SDK_DBG, "T_PREV_PREAMBLE_IDX=%d\n",
					rf_info->PreviousPreambleIndex);
				break;
			case TLV_T(T_HO_CNT):
				memcpy(&rf_info->HandOverCount, V, TLV_L(T_HO_CNT));
				rf_info->HandOverCount = H2B(rf_info->HandOverCount);
				xprintf(SDK_DBG, "T_HO_CNT=%d\n", rf_info->HandOverCount);
				break;
			case TLV_T(T_HO_FAIL_CNT):
				memcpy(&rf_info->HandOverFailCount, V, TLV_L(T_HO_FAIL_CNT));
				rf_info->HandOverFailCount = H2B(rf_info->HandOverFailCount);
				xprintf(SDK_DBG, "T_HO_FAIL_CNT=%d\n", rf_info->HandOverFailCount);
				break;
			case TLV_T(T_RESYNC_CNT):
				memcpy(&rf_info->ResyncCount, V, TLV_L(T_RESYNC_CNT));
				rf_info->ResyncCount = H2B(rf_info->ResyncCount);
				xprintf(SDK_DBG, "T_RESYNC_CNT=%d\n", rf_info->ResyncCount);
				break;
			case TLV_T(T_HO_SIGNAL_LATENCY):
				memcpy(&rf_info->HoSignalLatency, V, TLV_L(T_HO_SIGNAL_LATENCY));
				rf_info->HoSignalLatency = H2B(rf_info->HoSignalLatency);
				xprintf(SDK_DBG, "T_HO_SIGNAL_LATENCY=%d\n", rf_info->HoSignalLatency);
				break;
			case TLV_T(T_CINR):
				rf_info->CINR = wm_convert_cinr(*V);
				xprintf(SDK_DBG, "T_CINR=%d\n", (s8) *V);
				break;
			case TLV_T(T_CINR_2):
				rf_info->CINR2 = wm_convert_cinr(*V);
				xprintf(SDK_DBG, "T_CINR_2=%d\n", (s8) *V);
				break;
			case TLV_T(T_RSSI):
				rf_info->RSSI = wm_convert_rssi(*V);
				xprintf(SDK_DBG, "T_RSSI=%d\n", (s8) *V);
				break;
			case TLV_T(T_RSSI_2):
				rf_info->RSSI2 = wm_convert_rssi(*V);
				xprintf(SDK_DBG, "T_RSSI_2=%d\n", (s8) *V);
				break;
			case TLV_T(T_PER):
				memcpy(&rf_info->PER, V, TLV_L(T_PER));
				rf_info->PER = H2B(rf_info->PER);
				xprintf(SDK_DBG, "T_PER=%d\n", rf_info->PER);
				break;
			case TLV_T(T_POWER_CONTROL_MODE):
				rf_info->PowerControlMode = *V;
				xprintf(SDK_DBG, "T_POWER_CONTROL_MODE=%d\n", *V);
				break;
			case TLV_T(T_TX_POWER):
				rf_info->TxPower= wm_convert_txpower(*V);
				xprintf(SDK_DBG, "T_TX_POWER=%d\n", (s8) *V);
				break;
			case TLV_T(T_TX_POWER_MAX):
				rf_info->TxPowerMax = wm_convert_txpower(*V);
				xprintf(SDK_DBG, "T_TX_POWER_MAX=%d\n", (s8) *V);
				break;
			case TLV_T(T_UL_BURST_DATA_FEC_SCHEME):
				rf_info->ULBurstDataFECScheme = *V;
				xprintf(SDK_DBG, "T_UL_BURST_DATA_FEC_SCHEME=%d\n", *V);
				break;
			case TLV_T(T_DL_BURST_DATA_FEC_SCHEME):
				rf_info->DLBurstDataFECScheme = *V;
				xprintf(SDK_DBG, "T_DL_BURST_DATA_FEC_SCHEME=%d\n", *V);
				break;
			case TLV_T(T_UL_BURST_DATA_UIUC):
				rf_info->ULBurstDataUIUC = *V;
				xprintf(SDK_DBG, "T_UL_BURST_DATA_UIUC=%d\n", *V);
				break;
			case TLV_T(T_DL_BURST_DATA_UIUC):
				rf_info->DLBurstDataDIUC = *V;
				xprintf(SDK_DBG, "T_DL_BURST_DATA_UIUC=%d\n", *V);
				break;
			case TLV_T(T_CUR_FREQ):
				rf_info->Frequency = U82U32(V);
				xprintf(SDK_DBG, "T_CUR_FREQ=%d\n", rf_info->Frequency);
				break;
			default:
				xprintf(SDK_ERR, "Unknown Get-Info type(0x%02x)\n", T);
				ret = -1;
				goto out;
		}
	}

	memcpy(&rf_info->bsId, wm->conn_comp.net_id.bs.id, BS_ID_SIZE);
out:
	dm_put_dev(dev_idx);
	return ret;
}

void array_DB2H(u32 *arr, int n)
{
	int i;
	for (i = 0; i < n; i++, arr++)
		*arr = DB2H(*arr);
}

void array_B2H(u16 *arr, int n)
{
	int i;
	for (i = 0; i < n; i++, arr++)
		*arr = B2H(*arr);
}

int wm_retrieve_mac_pdu_statistics(mac_pdu_statistics_t *p, u8 *buf, int len)
{
	int data_size;

	xfunc_in();
	assert(p);
	data_size = p->end-p->start;
	p->timestamp = (unsigned int)gettimemsofday();
	memcpy(&p->start, buf, data_size);
	array_DB2H(&p->num_tx_sdu, data_size/4);
	xfunc_out();
	return data_size;
}

int wm_retrieve_mac_phy_mac_basic(mac_phy_mac_basic_t *p, u8 *buf, int len)
{
	ori_mac_phy_mac_basic_t *s = (ori_mac_phy_mac_basic_t *) buf;

	xfunc_in();
	assert(p);
	p->timestamp = (unsigned int)gettimemsofday();

	memcpy(&p->start, s, rangeof(ori_mac_phy_mac_basic_t, frame_number, bandwidth));
	memcpy(&p->time_active, &s->time_active,
		rangeof(ori_mac_phy_mac_basic_t, time_active, primary_cid));
	array_DB2H(&p->frame_number, 2);
	array_B2H(&p->ttg, 2);
	array_DB2H((u32*)&p->ul_time, 2);
	array_B2H(&p->bandwidth, 1);
	array_DB2H(&p->time_active, 3);
	array_B2H(&p->basic_cid, 2);
	xfunc_out();
	return sizeof(ori_mac_phy_mac_basic_t);
}

int wm_retrieve_mac_power_ctrl(mac_power_ctrl_t *p, u8 *buf, int len)
{
	int data_size;

	xfunc_in();
	assert(p);
	data_size = p->end-p->start;
	p->timestamp = (unsigned int)gettimemsofday();
	memcpy(&p->start, buf, data_size);
	xfunc_out();
	return data_size;
}

int wm_retrieve_mac_phy_burst(mac_phy_burst_t *p, u8 *buf, int len)
{
	int data_size;

	xfunc_in();
	assert(p);
	data_size = p->end-p->start;
	p->timestamp = (unsigned int)gettimemsofday();
	memcpy(&p->start, buf, data_size);
	array_DB2H(&p->num_dl_map,
		rangeof(mac_phy_burst_t, num_dl_map, num_harq_ul_burst_retry)/4);
	array_DB2H(&p->num_harq_crc_error, 1);
	xfunc_out();
	return data_size;
}

int wm_retrieve_mac_phy_mcs(mac_phy_mcs_t *p, u8 *buf, int len)
{
	int data_size;
	ori_mac_phy_mcs_t t;
	int nCurPos = 0;
	
	bool bDL = FALSE;
	int nMIMOType = MINO_TYPE_NOT_SPEC;

	xfunc_in();
	assert(p);
	data_size = p->end-p->start;
	p->timestamp = (unsigned int)gettimemsofday();

	while (nCurPos < len)
	{
		memcpy(&t, buf + nCurPos, sizeof(t));
		array_DB2H(&t.num_burst,
			rangeof(ori_mac_phy_mcs_t, num_burst, num_pdu)/4);
		nCurPos += sizeof(t);

		if (t.modulation_fec >= OFDMA_FEC_MODE_CNT)
		{
			xprintf(SDK_ERR, "Invalid modulation_fec(%d)\n", t.modulation_fec);
			return -1;
		}

		if (t.repetition_code >= REPETITION_CODING_CNT)
		{
			xprintf(SDK_ERR, "Invalid repetition_code(%d)\n", t.repetition_code);
			return -1;
		}

		bDL = (t.feature & 0x01) ? 1:0;
			
		if (t.feature & 0x02)
		{
			nMIMOType = MINO_TYPE2_ANT_STC_MATRIX_A;
		}
		else if (t.feature & 0x04)
		{
			nMIMOType = MINO_TYPE2_ANT_STC_MATRIX_B_VC;
		}
		else
			nMIMOType = MINO_TYPE_NOT_SPEC;

		// if DL
		if (bDL)
		{
			memcpy(&p->dl[t.modulation_fec][t.repetition_code][nMIMOType], &t.num_burst, 
				sizeof(p->dl[t.modulation_fec][t.repetition_code][nMIMOType]));
			p->dl_used[t.modulation_fec][t.repetition_code][nMIMOType] = TRUE;
		}
		else // else UL
		{
			memcpy(&p->ul[t.modulation_fec][t.repetition_code][nMIMOType], &t.num_burst, 
				sizeof(p->ul[t.modulation_fec][t.repetition_code][nMIMOType]));
			p->ul_used[t.modulation_fec][t.repetition_code][nMIMOType] = TRUE;
		}
	
	}
	
	xfunc_out();
	return data_size;
}

int wm_retrieve_mac_phy_zone(mac_phy_zone_t *p, u8 *buf, int len)
{
	xfunc_in();
	assert(p);
	p->timestamp = (unsigned int)gettimemsofday();
	xfunc_out();
	return sizeof(ori_mac_phy_mcs_t);
}

int wm_retrieve_mac_phy_cinr_rssi(mac_phy_cinr_rssi_t *p, u8 *buf, int len)
{
	int data_size;

	xfunc_in();
	assert(p);
	data_size = len; // Len is variable by Firmware version
	p->timestamp = (unsigned int)gettimemsofday();
	memcpy(&p->start, buf, data_size);
	xfunc_out();
	return data_size;
}

int wm_retrieve_mac_phy_temp_adc(mac_phy_temp_adc_t *p, u8 *buf, int len)
{
	int data_size;

	xfunc_in();
	assert(p);
	data_size = p->end-p->start;
	p->timestamp = (unsigned int)gettimemsofday();
	memcpy(&p->start, buf, data_size);
	xfunc_out();
	return data_size;
}

int wm_retrieve_mac_phy_ctrl_chan(mac_phy_ctrl_chan_t *p, u8 *buf, int len)
{
	int data_size;

	xfunc_in();
	assert(p);
	data_size = p->end-p->start;
	p->timestamp = (unsigned int)gettimemsofday();
	memcpy(&p->start, buf, data_size);
	array_DB2H(&p->num_ffb_sent, data_size/4);
	xfunc_out();
	return data_size;
}

int wm_retrieve_mac_phy_dl_statistics(mac_phy_dl_statistics_t *p, u8 *buf, int len)
{
	int data_size;

	xfunc_in();
	assert(p);
	data_size = p->end-p->start;
	p->timestamp = (unsigned int)gettimemsofday();
	memcpy(&p->start, buf, data_size);
	array_DB2H(&p->sfid, 1);
	array_B2H(&p->cid, 2);
	array_DB2H(&p->num_rx_sdu, 8);
	array_B2H(&p->arq_discard_msgs, 3);
	xfunc_out();
	return data_size;
}

int wm_retrieve_mac_phy_ul_statistics(mac_phy_ul_statistics_t *p, u8 *buf, int len)
{
	int data_size;

	xfunc_in();
	assert(p);
	data_size = p->end-p->start;
	p->timestamp = (unsigned int)gettimemsofday();
	memcpy(&p->start, buf, data_size);
	array_DB2H(&p->sfid, 1);
	array_B2H(&p->cid, 2);
	array_DB2H(&p->num_tx_sdu, 10);
	array_B2H(&p->arq_discard_msgs, 3);
	xfunc_out();
	return data_size;
}

int wm_retrieve_mac_handover(mac_handover_t *p, u8 *buf, int len)
{
	int data_size;

	xfunc_in();
	assert(p);
	data_size = p->end-p->start;
	p->timestamp = (unsigned int)gettimemsofday();
	memcpy(&p->start, buf, data_size);
	array_B2H(&p->ho_count, 4);
	xfunc_out();
	return data_size;
}

int wm_retrieve_mac_neighbor(mac_neighbor_t *p, u8 *buf, int len, u8 *bsid)
{
	int data_size;

	xfunc_in();
	assert(p);
	p->timestamp = (unsigned int)gettimemsofday();
	p->nr_list = MAX_NEIGHBOT_LIST;
	data_size = get_neighbor_info(bsid, buf, len, p->list, (int *)&p->nr_list);
	xfunc_out();
	return data_size;
}

int wm_retrieve_mac_harq_statistics(mac_harq_statistics_t *p, u8 *buf, int len)
{
	int data_size;

	xfunc_in();
	assert(p);
	data_size = p->end-p->start;
	p->timestamp = (unsigned int)gettimemsofday();
	memcpy(&p->start, buf, data_size);
	array_DB2H(p->num_dl_harq_retry, 16);
	xfunc_out();
	return data_size;
}

int wm_retrieve_mac_net_entry_statistics(mac_net_entry_statistics_t *p, u8 *buf, int len)
{
	int data_size;

	xfunc_in();
	assert(p);
	data_size = p->end-p->start;
	p->timestamp = (unsigned int)gettimemsofday();
	memcpy(&p->start, buf, data_size);
	array_B2H(&p->cdma_rng_time, 6);
	xfunc_out();
	return data_size;
}

int wm_set_run_mode(int dev_idx, int mode)
{
	u8 cnt = 0;
	u8 mode_val;
	int ret;
	u8 param[128];

	xfunc_in();

	if (mode & sdk_eng_mode)
		mode_val = T_ENGINEERING_MODE;
	else
		mode_val = T_NORMAL_MODE;

	param[cnt++] = TLV_T(T_RUN_MODE);
	param[cnt++] = TLV_L(T_RUN_MODE);
	param[cnt++] = mode_val;

	ret = hci_send(dev_idx, WIMAX_SET_INFO, param, cnt);
	if (cnt == ret)
		ret = 0;
	else {
		xprintf(SDK_STD_ERR, "[%d] hci_send(%d!=%d)\n", dev_idx, cnt, ret);
		ret = -1;
	}

	xfunc_out();
	return ret;
}

static int get_neighbor_info(u8 *bsid, u8 *buf, int len,
	wm_bs_neigh_t *list, int *list_cnt)
{
	u8 neighbors = *buf;
	bs_neigh_t *n;
	int i = 0, entry_size = 0;

	xfunc_in("list cnt=%d", *list_cnt);

	if (len < sizeof(bs_neigh_t)) {
		if ((u32)len <= 1)/*0 or 1*/
			xprintf(SDK_INFO, "Not found neighbor\n");
		else
			xprintf(SDK_ERR, "Wrong length=%d\n", len);
		goto out;
	}

	if (!neighbors) {
		xprintf(SDK_INFO, "Neighbor list is empty!\n");
		goto out;
	}

	entry_size = (len - 1) / neighbors;

	for (i = 0; i < neighbors; i++) {
		if (i+1 > *list_cnt)
			break;

		n = (bs_neigh_t *)(buf + 1 + i * entry_size);

		list[i].frequency = (n->frequency[0]<<16) | (n->frequency[1]<<8) | n->frequency[2];
		list[i].preamble = n->preamble_idx;
		list[i].cinr = wm_convert_cinr(n->cinr);
		list[i].rssi = wm_convert_rssi(n->rssi);

		list[i].bsid[0] = bsid[0];
		list[i].bsid[1] = bsid[1];
		list[i].bsid[2] = bsid[2];
		list[i].bsid[3] = n->bsid[0];
		list[i].bsid[4] = n->bsid[1];
		list[i].bsid[5] = n->bsid[2];

		#if 1
		if (entry_size >= sizeof(bs_neigh_t))
			list[i].bandwidth = n->bandwidth;

		xprintf(SDK_INFO, "%d %d %d %d %02x%02x%02x:%02x%02x%02x %d\n",
				list[i].frequency, n->preamble_idx, n->cinr, n->rssi,
				list[i].bsid[0], list[i].bsid[1], list[i].bsid[2],
				list[i].bsid[3], list[i].bsid[4], list[i].bsid[5],
				list[i].bandwidth);
		#else
		if (entry_size >= sizeof(bs_neigh_t)) {
			list[i].bandwidth = n->bandwidth;
			list[i].rtd = n->rtd;
		}

		xprintf(SDK_INFO, "%d %d %d %d %02x%02x%02x:%02x%02x%02x %d %d\n",
				list[i].frequency, n->preamble_idx, n->cinr, n->rssi,
				list[i].bsid[0], list[i].bsid[1], list[i].bsid[2],
				list[i].bsid[3], list[i].bsid[4], list[i].bsid[5],
				list[i].bandwidth, list[i].rtd);
		#endif
	}
out:
	*list_cnt = i;
	xfunc_out("list cnt=%d", *list_cnt);
	return *list_cnt ? (1 + *list_cnt * entry_size) : 0;
}

int wm_get_neighbor_list(int dev_idx, wm_bs_neigh_t *list, int *list_cnt)
{
	device_t *dev;
	wimax_t *wm;
	u8 buf[HCI_MAX_PACKET];
	u8 cnt = 0;
	u8 param[128];
	int ret, pos = 0;
	u8 T;
	u16 L;
	u8 *V;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;
	wm = dev->wimax;

	param[cnt++] = 0;
	param[cnt++] = W_NEIGHBOR_LIST;

	ret = hci_send_wait(dev_idx, WIMAX_MODEM_REQUEST, param, cnt,
		WIMAX_MODEM_REPORT, buf, HCI_MAX_PACKET, 0, 0, IO_TIMEOUT_SEC);

	while (pos < ret) {
		pos += hci_get_tlv(buf+pos, &T, &L, &V);

		switch (T) {
			case W_NEIGHBOR_LIST:
				ret = get_neighbor_info(wm->conn_comp.net_id.bs.id, V, L, list, list_cnt);
				break;
			default:
				xprintf(SDK_ERR, "Unknown type:0x%02X\n", T);
				ret = -1;
				goto out;
		}
	}
out:
	if (ret > 0) ret = 0;
	else *list_cnt = 0;

	dm_put_dev(dev_idx);
	xfunc_out("ret=%d", ret);
	return ret;
}

int wm_notification(int dev_idx, char *buf, int len)
{
	hci_notification_t *err_noti = (hci_notification_t *) buf;
	GCT_API_NOTI_CATEGORY category;
	GCT_API_NOTI_TYPE type;
	char *value;
	int vlen;
	int ret;

	xfunc_in("len=%d", len);

	category = err_noti->category;
	type = err_noti->type;
	value = (char *) err_noti->value;
	vlen = len - sizeof(hci_notification_t);

	ret = sdk_ind_noti(dev_idx, category, type, value, vlen);

	xfunc_out("ret=%d", ret);
	return ret;
}

int wm_ind_sf_mode(int dev_idx, u8 *buf, int len)
{
	device_t *dev;
	wimax_t *wm;
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	wm = dev->wimax;

	if (*buf == WIMAX_SF_ENABLE)
		wm->scan.sf_mode = TRUE;
	else
		wm->scan.sf_mode = FALSE;

	dm_put_dev(dev_idx);
	xfunc_out("ret=%d", ret);
	return ret;
}

int wm_disable_sf_mode(int dev_idx)
{
	device_t *dev;
	wimax_t *wm;
	char *cmd = "sfe";
	int cmd_len = strlen(cmd);
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	wm = dev->wimax;

	if (wm->scan.sf_mode) {
		if ((ret = hci_send(dev_idx, WIMAX_CLI_CMD, (u8 *)cmd, cmd_len)) == cmd_len)
			ret = 0;
		xprintf(SDK_NOTICE, "Disable SF Mode, ret=%d\n", ret);
		wm->scan.sf_mode = FALSE;
	}

	dm_put_dev(dev_idx);
	xfunc_out("ret=%d", ret);
	return ret;
}

bool wm_compare_nsp(wm_nsp_identifier_t *nsp, wm_nsp_identifier_t *nsp2)
{
	bool ret;
	int lmask = SDK_FORCE;

	if (U82U24(nsp->id) == U82U24(nsp2->id))
		ret = TRUE;
	else
		ret = FALSE;
	
	xprintf(lmask, "%s(%02x%02x%02x) & %s(%02x%02x%02x) : %s\n",
		nsp->name, nsp->id[0], nsp->id[1], nsp->id[2],
		nsp2->name, nsp2->id[0], nsp2->id[1], nsp2->id[2],
		ret ? "match" : "mismatch");

	return ret;
}

int wm_report_full_reentry(int dev_idx, u8 *buf, int len)
{
	device_t *dev;
	int ret = 0;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	xfunc_in("dev=%d", dev_idx);

	dm_put_dev(dev_idx);
	xfunc_out();
	return ret;
}

int wm_cmd_mac_state(int dev_idx, GCT_API_CMD_MAC_STATE_TYPE type)
{
	u16 cmd;
	int ret;

	if (!sdk_get_rw_handle())
		return 0;

	switch(type) {
		case GCT_API_CMD_MAC_STATE_ENTER_SLEEP_MODE:
			cmd = WIMAX_ENTER_SLEEP;
			break;

		case GCT_API_CMD_MAC_STATE_EXIT_SLEEP_MODE:
			cmd = WIMAX_EXIT_SLEEP;
			break;

		case GCT_API_CMD_MAC_STATE_ENTER_IDLE_MODE:
			cmd = WIMAX_ENTER_IDLE;
			break;
			
		case GCT_API_CMD_MAC_STATE_EXIT_IDLE_MODE:
			cmd = WIMAX_EXIT_IDLE;
			break;
	}

	ret = hci_send(dev_idx, cmd, NULL, 0);
	if (0 != ret) {
		xprintf(SDK_STD_ERR, "[%d] hci_send(%d)\n", dev_idx, ret);
		ret = sdk_set_errno(ERR_STD);
	}

	return ret;
}

int wm_set_idle_mode_timeout(int dev_idx, u16 timeoutSec)
{
	u8 buf[128];
	int pos;
	int ret;

	if (!sdk_get_rw_handle())
		return 0;

	timeoutSec = B2H(timeoutSec);

	pos = hci_set_tlv(buf, TLV_T(T_IDLE_MODE_TO), TLV_L(T_IDLE_MODE_TO), (u8*)&timeoutSec);

	ret = hci_send(dev_idx, WIMAX_SET_INFO, buf, pos);
	if (!ret) {
		xprintf(SDK_STD_ERR, "[%d] hci_send(%d)\n", dev_idx, ret);
		ret = sdk_set_errno(ERR_STD);
	}
	return ret;
}

int wm_get_phy_mac_basic(int dev_idx, GCT_API_MAC_PHY_MAC_BASIC_P pData)
{
	u8 buf[HCI_MAX_PACKET], *p = buf;
	u8 T, *V;
	u16 L;
	int pos = 0, len = 0, ret = -1;
	ori_mac_phy_mac_basic_t *s;

	*p++ = MODE_MODEM_REPORT;
	*p++ = W_PHY_MAC_BASIC;
	len = hci_send_wait(dev_idx, WIMAX_MODEM_REQUEST, buf, p-buf,
		WIMAX_MODEM_REPORT, buf, sizeof(buf), 0, 0, 5);
	if (len <= HCI_HEADER_SIZE) {
		xprintf(SDK_ERR, "HCI response error: len=%d\n", ret);
		ret = -1;
		goto out;
	}

	memset(pData, 0x00, sizeof(GCT_API_MAC_PHY_MAC_BASIC));

	while (pos < len) {
		pos += hci_get_tlv(&buf[pos], &T, &L, &V);
		switch (T) {
			case TLV_T(W_PHY_MAC_BASIC):
				s = (ori_mac_phy_mac_basic_t *) V;

				memcpy(&pData->frame_number, s, 
					rangeof(ori_mac_phy_mac_basic_t, frame_number, ul_time));

				array_DB2H(&pData->frame_number, 2);
				array_B2H(&pData->ttg, 2);
				array_DB2H((u32*)&pData->ul_time, 1);

				if (L > 28) {
					memcpy(&pData->frequency, &s->frequency, 
						rangeof(ori_mac_phy_mac_basic_t, frequency, bandwidth));
					memcpy(&pData->time_active, &s->time_active,
						rangeof(ori_mac_phy_mac_basic_t, time_active, primary_cid));

					array_DB2H((u32*)&pData->frequency, 1);
					array_B2H(&pData->bandwidth, 1);
					array_DB2H(&pData->time_active, 3);
					array_B2H(&pData->basic_cid, 2);
				}

				ret = L;
				break;
			default:
				ret = -1;
		}
		xprintf(SDK_DBG, "Type(%02x): L=%u, ret=%d\n", T, L, ret);
		if (ret < 0) {
			xprintf(SDK_NOTICE, "Unknown Type=0x%02x\n", T);
			break;
		}
	}

out:
	xfunc_out("ret=%d", ret);
	return ret;
}

int wm_get_phy_mcs(int dev_idx, GCT_API_MAC_PHY_MCS_P pData)
{
	u8 buf[HCI_MAX_PACKET], *p = buf;
	u8 T, *V;
	u16 L;
	int pos = 0, len = 0, nCurPos = 0, ret = -1;
	bool bDL = FALSE;
	int nMIMOType = MINO_TYPE_NOT_SPEC;
	ori_mac_phy_mcs_t t;

	*p++ = MODE_MODEM_REPORT;
	*p++ = W_PHY_MSC;
	len = hci_send_wait(dev_idx, WIMAX_MODEM_REQUEST, buf, p-buf,
		WIMAX_MODEM_REPORT, buf, sizeof(buf), 0, 0, 5);
	if (len <= HCI_HEADER_SIZE) {
		xprintf(SDK_ERR, "HCI response error: len=%d\n", ret);
		ret = -1;
		goto out;
	}

	memset(pData, 0x00, sizeof(GCT_API_MAC_PHY_MCS));

	while (pos < len) {
		pos += hci_get_tlv(&buf[pos], &T, &L, &V);
		switch (T) {
			case TLV_T(W_PHY_MSC):
				while (nCurPos < L)
				{
					memcpy(&t, V + nCurPos, sizeof(t));
					array_DB2H(&t.num_burst,
						rangeof(ori_mac_phy_mcs_t, num_burst, num_pdu)/4);
					nCurPos += sizeof(t);
				
					if (t.modulation_fec >= OFDMA_FEC_MODE_CNT)
					{
						xprintf(SDK_ERR, "Invalid modulation_fec(%d)\n", t.modulation_fec);
						ret = -1;
						goto out;
					}
				
					if (t.repetition_code >= REPETITION_CODING_CNT)
					{
						xprintf(SDK_ERR, "Invalid repetition_code(%d)\n", t.repetition_code);
						ret = -1;
						goto out;
					}
				
					bDL = (t.feature & 0x01) ? 1:0;
						
					if (t.feature & 0x02)
					{
						nMIMOType = MINO_TYPE2_ANT_STC_MATRIX_A;
					}
					else if (t.feature & 0x04)
					{
						nMIMOType = MINO_TYPE2_ANT_STC_MATRIX_B_VC;
					}
					else
						nMIMOType = MINO_TYPE_NOT_SPEC;
				
					// if DL
					if (bDL)
					{
						memcpy(&pData->dl[t.modulation_fec][t.repetition_code][nMIMOType], &t.num_burst, 
							sizeof(pData->dl[t.modulation_fec][t.repetition_code][nMIMOType]));
						pData->dl_used[t.modulation_fec][t.repetition_code][nMIMOType] = TRUE;
					}
					else // else UL
					{
						memcpy(&pData->ul[t.modulation_fec][t.repetition_code][nMIMOType], &t.num_burst, 
							sizeof(pData->ul[t.modulation_fec][t.repetition_code][nMIMOType]));
						pData->ul_used[t.modulation_fec][t.repetition_code][nMIMOType] = TRUE;
					}
				
				}

				ret = L;
				break;
			default:
				ret = -1;
		}
		xprintf(SDK_DBG, "Type(%02x): L=%u, ret=%d\n", T, L, ret);
		if (ret < 0) {
			xprintf(SDK_NOTICE, "Unknown Type=0x%02x\n", T);
			break;
		}
	}

out:
	xfunc_out("ret=%d", ret);
	return ret;
}

int wm_get_phy_cinr_rssi(int dev_idx, GCT_API_MAC_PHY_CINR_RSSI_P pData)
{
	u8 buf[HCI_MAX_PACKET], *p = buf;
	u8 T, *V;
	u16 L;
	int pos = 0, len = 0, ret = -1;

	*p++ = MODE_MODEM_REPORT;
	*p++ = W_PHY_CINR_RSSI;
	len = hci_send_wait(dev_idx, WIMAX_MODEM_REQUEST, buf, p-buf,
		WIMAX_MODEM_REPORT, buf, sizeof(buf), 0, 0, 5);
	if (len <= HCI_HEADER_SIZE) {
		xprintf(SDK_ERR, "HCI response error: len=%d\n", ret);
		ret = -1;
		goto out;
	}

	memset(pData, 0x00, sizeof(GCT_API_MAC_PHY_CINR_RSSI));

	while (pos < len) {
		pos += hci_get_tlv(&buf[pos], &T, &L, &V);
		switch (T) {
			case TLV_T(W_PHY_CINR_RSSI):
				memcpy(pData, V, L);

				ret = L;
				break;
			default:
				ret = -1;
		}
		xprintf(SDK_DBG, "Type(%02x): L=%u, ret=%d\n", T, L, ret);
		if (ret < 0) {
			xprintf(SDK_NOTICE, "Unknown Type=0x%02x\n", T);
			break;
		}
	}

out:
	xfunc_out("ret=%d", ret);
	return ret;
}

