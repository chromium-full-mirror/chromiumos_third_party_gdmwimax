// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <unistd.h>
#include <net/if.h>
#include <assert.h>

#include "error.h"
#include "sdk.h"
#include "device.h"
#include "io.h"
#include "hci.h"
#include "wimax.h"
#include "nds.h"
#include "log.h"

#if defined(TRANSPORT_TEST)
static int dl_transport_handler(int dev_idx, char *buf, int len);
static int start_transport_test(int dev_idx, char *buf, int len);
#endif

int hci_get_tlv(u8 *buf, u8 *T, u16 *L, u8 **V)
{
	int next_pos;

	*T = buf[0];
	if (buf[1] == 0x82) {
		*L = U82U16(&buf[2]);
		next_pos = 1/*type*/+3/*len*/;
	}
	else {
		*L = buf[1];
		next_pos = 1/*type*/+1/*len*/;
	}
	*V = &buf[next_pos];

	next_pos += *L/*length of val*/;
	return next_pos;
}

int hci_set_tlv(u8 *buf, u8 T, u16 L, u8 *V)
{
	int pos = 0;

	buf[pos++] = T;

	if (L < 0x80) {
		buf[pos++] = L;
	}
	else {
		buf[pos++] = 0x82;
		U162U8(&buf[pos], L);
		pos+=2;
	}

	memcpy(&buf[pos], V, L);
	pos += L;

	return pos;
}

int hci_send(int dev_idx, u16 cmd, u8 *data, int len)
{
	u16 buf[HCI_MAX_PACKET / sizeof(u16)];
	u8 *data_buf = (u8 *)&buf[0] + HCI_HEADER_SIZE;
	int ret;

	xfunc_in("cmd=0x%04x, len=%d", cmd, len);

	buf[0] = H2B(cmd);
	buf[1] = H2B(len);

	memcpy(data_buf, data, len);

	ret = io_send(dev_idx, buf, len+HCI_HEADER_SIZE);
	if (ret == len+HCI_HEADER_SIZE)
		ret = len;
	else if (ret >= 0) {
		xprintf(SDK_ERR, "Wrong sent size=%d\n", ret);
		ret = -1;
	}

	xfunc_out("ret=%d", ret);
	return ret;
}

#define DESTROYED_HCI		-2

int hci_send_wait(int dev_idx, u16 s_cmd, u8 *data, int plen, 
	u16 w_cmd, void *buf, int buf_len, int chk_len, u32 flag, int timeout)
{
	#define timeval2timespec(tv,ts)	\
		((ts)->tv_sec = (tv)->tv_sec, (ts)->tv_nsec = (tv)->tv_usec * 1000)
	device_t *dev;
	hci_req_t hcir;
	struct list_head *head;
	struct timeval tv;
	struct timespec ts;
	int ret;

	xfunc_in("cmd=0x%04x", s_cmd);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	pthread_mutex_lock(&dev->hci_wait_signal);

	if ((ret = hci_send(dev_idx, s_cmd, data, plen)) < 0)
		goto out;

	INIT_LIST_HEAD(&hcir.list);
	hcir.cmd_evt = w_cmd;
	hcir.buf = buf;
	hcir.len = buf_len;
	hcir.chk_len = chk_len;
	hcir.flag = flag;

	pthread_cond_init(&hcir.cond, NULL);

	if (timeout) {
		gettimeofday(&tv, NULL);
		timeval2timespec(&tv, &ts);
		ts.tv_sec += timeout;
	}

	head = &dev->hci_wait_list;
	list_add_tail(&hcir.list, head);

	xprintf(SDK_DBG, "Wait cmd(0x%04x)\n", hcir.cmd_evt);
	ret = pthread_cond_timedwait(&hcir.cond, &dev->hci_wait_signal, &ts);
	if (ret == ETIMEDOUT) {
		list_del(&hcir.list);
		if (w_cmd != WIMAX_ARM_CAPABILITY_RESULT)
			xprintf(SDK_ERR, "hci_send_wait(0x%04X TIMEOUT(%d)\n", w_cmd, timeout);
		ret = - ETIMEDOUT;
	}
	else {
		ret = hcir.len;
		if (ret == DESTROYED_HCI)
			xprintf(SDK_NOTICE, "HCI(%04x) has been destroyed\n", w_cmd);
	}

out:
	pthread_mutex_unlock(&dev->hci_wait_signal);
	xfunc_out("ret=%d", ret);
	dm_put_dev(dev_idx);
	return ret;
}

int hci_wait(int dev_idx, u16 cmd_evt,
			void *buf, int len, int chk_len, u32 flag, int timeout)
{
	#define timeval2timespec(tv,ts)	\
		((ts)->tv_sec = (tv)->tv_sec, (ts)->tv_nsec = (tv)->tv_usec * 1000)
	device_t *dev;
	hci_req_t hcir;
	struct list_head *head;
	struct timeval tv;
	struct timespec ts;
	int ret;

	xfunc_in("cmd=0x%04x", cmd_evt);
	
	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	INIT_LIST_HEAD(&hcir.list);
	hcir.cmd_evt = cmd_evt;
	hcir.buf = buf;
	hcir.len = len;
	hcir.chk_len = chk_len;
	hcir.flag = flag;

	pthread_cond_init(&hcir.cond, NULL);

	if (timeout) {
		gettimeofday(&tv, NULL);
		timeval2timespec(&tv, &ts);
		ts.tv_sec += timeout;
	}

	pthread_mutex_lock(&dev->hci_wait_signal);
	head = &dev->hci_wait_list;
	list_add_tail(&hcir.list, head);

	ret = pthread_cond_timedwait(&hcir.cond, &dev->hci_wait_signal, &ts);
	if (ret == ETIMEDOUT) {
		list_del(&hcir.list);
		xprintf(SDK_ERR, "hci_wait(0x%04X TIMEOUT(%d)\n", cmd_evt, timeout);
		ret = - 1;
	}
	else {
		ret = hcir.len;
		if (ret == DESTROYED_HCI)
			xprintf(SDK_NOTICE, "HCI(%04x) has been destroyed\n", cmd_evt);
	}

	pthread_mutex_unlock(&dev->hci_wait_signal);
	xfunc_out("ret=%d", ret);
	dm_put_dev(dev_idx);
	return ret;
}

int hci_destroy_resp(int dev_idx)
{
	device_t *dev;
	struct list_head *head;
	hci_req_t *hcir, *tmp;
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	head = &dev->hci_wait_list;

	pthread_mutex_lock(&dev->hci_wait_signal);
	list_for_each_entry_safe(hcir, tmp, head, list) {
		hcir->len = DESTROYED_HCI;
		pthread_cond_signal(&hcir->cond);
		ret++;
	}
	pthread_mutex_unlock(&dev->hci_wait_signal);

	xfunc_out("ret=%d", ret);
	dm_put_dev(dev_idx);
	return ret;
}

static int hci_unsolicited_getinfo_result(int dev_idx, u8 *buf, int len)
{
	device_t *dev;
	u8 T, *V;
	u16 L;
	int pos = 0, ret = 0;

	xfunc_in();

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;
	while (pos < len) {
		pos += hci_get_tlv(&buf[pos], &T, &L, &V);
		switch (T) {
			case TLV_T(T_REALM):
				memcpy(dev->wimax->dev_info.eapp.visited_realm, V, L);
				dev->wimax->dev_info.eapp.visited_realm[L] = 0;
				xprintf(SDK_NOTICE, "Network Realm=%s\n",
					dev->wimax->dev_info.eapp.visited_realm);
				break;
			case TLV_T(T_BSID):
				xprintf(SDK_INFO, "BSID=%06X:%06X\n", U82U24(&V[0]), U82U24(&V[3]));
				break;
			case TLV_T(T_RSSI):
				xprintf(SDK_INFO, "T_RSSI(%d)\n", wm_convert_rssi(*V));
				break;
			default:
				xprintf(SDK_DBG, "Unknown Type=0x%02x\n", T);
				ret = -1;
				goto out;
		}
	}
out:
	dm_put_dev(dev_idx);
	xfunc_out("ret=%d", ret);
	return ret;
}

static bool hci_receive_resp(int dev_idx, u16 cmd_evt, u8 *buf, int len)
{
	device_t *dev;
	struct list_head *head;
	hci_req_t *hcir, *tmp;
	int handled_cnt = 0;

	xfunc_in("cmd=0x%04x", cmd_evt);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	head = &dev->hci_wait_list;

	pthread_mutex_lock(&dev->hci_wait_signal);
	list_for_each_entry_safe(hcir, tmp, head, list) {
		if (cmd_evt == hcir->cmd_evt) {
			if (hcir->chk_len) {
				if (memcmp(hcir->buf, buf, hcir->chk_len)) {
					xprintf_hex(SDK_DBG, "data to check is not matched: 1",
						buf, hcir->chk_len);
					xprintf_hex(SDK_DBG, "data to check is not matched: 2",
						hcir->buf, hcir->chk_len);
					continue;
				}
			}
			if (hcir->buf && hcir->len) {
				assert(hcir->len >= len);
				memcpy(hcir->buf, buf, len);
				hcir->len = len;
			}
			else
				hcir->len = 0;

			list_del(&hcir->list);
			xprintf(SDK_DBG, "Send signal(0x%04x(%d): %02x, %02x)\n",
				hcir->cmd_evt, hcir->len, buf[0], buf[1]);
			pthread_cond_signal(&hcir->cond);
			if (hcir->flag & HCI_INDICATION)
				handled_cnt = -1;
			if (handled_cnt >=0)
				handled_cnt++;
		}
	}
	pthread_mutex_unlock(&dev->hci_wait_signal);

	xfunc_out("handled_cnt=%d", handled_cnt);
	dm_put_dev(dev_idx);
	return handled_cnt <= 0 ? FALSE : TRUE;
}

static int hci_receive_ind(int dev_idx, void *buf, int len)
{
	device_t *dev;
	struct hci *hci = (struct hci *) buf;
	u16 cmd_evt, cmd_len, category;
	u8 *data_buf = (u8 *) hci->data;
	bool should_bypass = FALSE;
	int ret = -1;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;
	
	cmd_evt = B2H(hci->cmd_evt);
	cmd_len = B2H(hci->length);
	category = (cmd_evt >> 8) & 0xf;

	xfunc_in("0x%04x, %d, %02x, %02x, %02x, %02x", cmd_evt, cmd_len,
		hci->data[0], hci->data[1], hci->data[2], hci->data[3]);

	if (len > HCI_MAX_PACKET || (len-HCI_HEADER_SIZE) < cmd_len) {
		xprintf(SDK_ERR, "[%d] Invalid packet length(%d) (%d,%d)\n",
			dev_idx, len, (len-HCI_HEADER_SIZE), cmd_len);
		goto out;
	}

	if (hci_receive_resp(dev_idx, cmd_evt, hci->data, cmd_len))
		goto out;

	if (category == 3)
		should_bypass = TRUE;

	if (!dev->wimax) {
		xprintf(SDK_NOTICE, "Device has been closed!\n");
		goto out;
	}

	/*The following is a indication.*/
	switch (cmd_evt) {
		case WIMAX_FSM_UPDATE:
			ret = sdk_ind_status_update(dev_idx, data_buf, cmd_len);
			break;
		case WIMAX_IF_UPDOWN:
			ret = sdk_ind_if_updown(dev_idx, data_buf, cmd_len);
			break;
		case WIMAX_RADIO_STATE_IND:
			ret = sdk_ind_rf_state(dev_idx, data_buf, cmd_len);
			break;
		case WIMAX_SCAN_RESULT:
			ret = nds_update_scan_result(dev_idx, data_buf, cmd_len);
			break;
		case WIMAX_SCAN_COMPLETE:
			ret = sdk_ind_event(dev_idx, NM_ScanComplete);
			break;
		case WIMAX_CONNECT_START:
			ret = sdk_ind_event(dev_idx, NC_ConnectStart);
			break;
		case WIMAX_CONNECT_COMPLETE:
			ret = nds_connect_complete(dev_idx, data_buf, cmd_len);
			break;
		case WIMAX_ASSOC_START:
			ret = nds_associate_start(dev_idx, data_buf, cmd_len);
			break;
		case WIMAX_ASSOC_COMPLETE:
			ret = nds_associate_complete(dev_idx, data_buf, cmd_len);
			break;
		case WIMAX_DISCONN_IND:
			ret = nds_disconnect_ind(dev_idx, data_buf, cmd_len);
			ret |= sdk_ind_recv_hci_pkt(dev_idx, buf, len);
			break;
		case WIMAX_ENTRY_IND:
			ret = nds_connection_stage(dev_idx, data_buf, cmd_len);
			break;
		case WIMAX_HO_START:
			ret = nds_ho_start(dev_idx, data_buf, cmd_len);
			break;
		case WIMAX_HO_COMPLETE:
			ret = nds_ho_complete(dev_idx, data_buf, cmd_len);
			break;
		case WIMAX_IP_RENEW_IND:
			should_bypass = TRUE;
			break;
		case WIMAX_RX_EAP:
				xprintf(SDK_ERR, "WIMAX_RX_EAP is NOT supported.\n", cmd_evt);
			break;
		case WIMAX_NOTIFICATION:
			ret = wm_notification(dev_idx, (char *) data_buf, cmd_len);
			should_bypass = FALSE;
			break;
		#if defined(TRANSPORT_TEST)
		case WIMAX_START_TRANSPORT_TEST:
			start_transport_test(dev_idx, (char *) data_buf, cmd_len);
			should_bypass = FALSE;
			break;
		case WIMAX_DL_TRANSPORT_TEST:
			dl_transport_handler(dev_idx, (char *) data_buf, cmd_len);
			should_bypass = FALSE;
			break;
		#endif
		case WIMAX_ARM_CAPABILITY_RESULT:
			wm_report_capability(dev_idx, data_buf, cmd_len);
			should_bypass = FALSE;
			break;
		case WIMAX_MODE_CHANGE:
			ret = sdk_ind_power_mode_change(dev_idx, data_buf, cmd_len);
			break;
		case WIMAX_SF_IND:
			ret = wm_ind_sf_mode(dev_idx, data_buf, cmd_len);
			break;
		case WIMAX_GET_INFO_RESULT:
			ret = hci_unsolicited_getinfo_result(dev_idx, data_buf, cmd_len);
			if (!ret)
				break;
		case WIMAX_FULL_REENTRY:
			ret = wm_report_full_reentry(dev_idx, data_buf, cmd_len);
			break;
#if defined(CONFIG_ENABLE_SERVICE_FLOW)
		case WIMAX_DSX_COMPLETE:
			// Using original data
			ret = sdk_sf_recv_dsx_complete(dev_idx, buf, len);
			break;
#endif // CONFIG_ENABLE_SERVICE_FLOW
		default:
			if (!should_bypass)
				xprintf(SDK_INFO, "Unhandled HCI(0x%04x)\n", cmd_evt);
			break;
	}

	if (should_bypass) {
		xprintf(SDK_DBG, "Bypass HCI(0x%04x)\n", cmd_evt);
		ret = sdk_ind_recv_hci_pkt(dev_idx, buf, len);
	}
out:
	xfunc_out("ret=%d", ret);
	dm_put_dev(dev_idx);

	return ret;
}

bool hci_parse_lv(u8 *buf, u16 event, u8 T, u8 *L, u8 **V)
{
	u8 type;
	u16 cmd_evt;

	cmd_evt = B2H(*(u16 *)&buf[0]);

	if (cmd_evt == event) {
		type = buf[HCI_HEADER_SIZE];

		if (T == type) {
			*L = buf[HCI_HEADER_SIZE+1];
			*V = &buf[HCI_HEADER_SIZE+2];
			return TRUE;
		}
		else
			xprintf(SDK_DBG, "receive another type(0x%02X!=0x%02X)\n", type, T);
	}
	else
		xprintf(SDK_DBG, "receive another event(0x%04X!=0x%04X)\n", cmd_evt, event);

	return FALSE;
}

int hci_req_getinfo(int dev_idx, u8 *buf, tlv_t *tlv, int cnt)
{
	u8 param[HCI_MAX_TLV];
	int pos = 0, i;
	int chk_len;
	u8 T;
	int ret = -1;

	xfunc_in("dev=%d, cnt=%d", dev_idx, cnt);
	assert(HCI_MAX_TLV >= cnt);

	for (i = 0; i < cnt; i++)
		param[i] = tlv[i].T;

	buf[0] = tlv[0].T;
	chk_len = 1;
	ret = hci_send_wait(dev_idx, WIMAX_GET_INFO, param, cnt,
		WIMAX_GET_INFO_RESULT, buf, HCI_MAX_PACKET, chk_len, 0, IO_TIMEOUT_SEC);
	if (ret > cnt/*min. size*/) {
		for (i = 0; i < cnt; i++) {
			pos += hci_get_tlv(&buf[pos], &T, &tlv[i].L, &tlv[i].V);
			if (T != tlv[i].T) {
				xprintf_hex(SDK_ERR, "GET_INFO", buf, ret);
				xprintf(SDK_ERR, "hci_get_tlv mismatch 0x%02X!=0x%02X\n", T, tlv[i].T);
				ret = -1;
				goto out;
			}
		}
		ret = 0;
	}
	else if (ret >= 0){
		xprintf(SDK_ERR, "hci_send_wait mismatch: %d<=%d\n", ret, cnt);
		ret = -1;
	}
out:
	xfunc_out("ret=%d", ret);
	return ret;
}

int hci_req_getinfo32(int dev_idx, u8 type, u32 *val)
{
	u8 buf[HCI_MAX_PACKET];
	tlv_t tlv;
	int ret;

	xfunc_in();
	
	tlv.T = type;
	ret = hci_req_getinfo(dev_idx, buf, &tlv, 1);
	if (!ret) {
		if (tlv.L == sizeof(u32)) {
			memcpy(val, tlv.V, sizeof(u32));
			*val = DB2H(*val);
		}
		else {
			xprintf(SDK_ERR, "length mismatch(%d!=%d)\n", tlv.L, sizeof(u32));
			ret = -1;
		}
	}

	xfunc_out("ret=%d, *val=0x%08X", ret, *val);
	return ret;
}

int hci_req_setinfo32(int dev_idx, u8 type, u32 val)
{
	u8 buf[HCI_MAX_PARAM];
	u8 *pos = buf;
	u8 T, *V;
	u16 L;
	int len, ret;

	xfunc_in("val=%d", val);

	val = DH2B(val);
	T = type;
	L = (u16) sizeof(u32);
	V = (u8 *) &val;
	pos += hci_set_tlv(pos, T, L, V);

	len = pos - buf;
	ret = hci_send(dev_idx, WIMAX_SET_INFO, buf, len);
	if (len == ret)
		ret = 0;
	else {
		xprintf(SDK_STD_ERR, "[%d] hci_send(%d!=%d)\n", dev_idx, len, ret);
		ret = sdk_set_errno(ERR_STD);
	}
	xfunc_out("ret=%d", ret);
	return ret;
}

int hci_send_msg(int dev_idx, hci_msg_t *msg)
{
	msg_thr_t *hci_recvr = &sdk_mng.hci_recvr;

	xfunc_in("%02x%02x", (u8)msg->buf[0], (u8)msg->buf[1]);

	msg_send(&hci_recvr->msg_cb, dev_idx, msg);

	xfunc_out();
	return 0;
}

static void *hci_receiver_thread(void *data)
{
	msg_thr_t *hci_recvr = (msg_thr_t *) data;
	hci_msg_t *msg;
	int dev_idx;
	char *buf;
	int len;

	xfunc_in();

	while (1) {
		xprintf(SDK_DBG, "[%d] In hci_receiver\n", dev_idx);
		if (msg_recv(&hci_recvr->msg_cb, &dev_idx, (void **)&msg) < 0) {
			xprintf(SDK_ERR, "hci msg_recv error\n");
			break;
		}

		if (msg == THREAD_EXIT_MSG) {
			xprintf(SDK_INFO, "%s thread exit...\n", __FUNCTION__);
			break;
		}

		buf = msg->buf;
		len = msg->len;
		hci_receive_ind(dev_idx, buf, len);
		sdk_free(msg);
	}

	msg_deinit(&hci_recvr->msg_cb);
	xfunc_out();
	return NULL;
}

int hci_receiver_create(msg_thr_t *hci_recvr)
{	
	xfunc_in();

	msg_init(&hci_recvr->msg_cb);

	pthread_create(&hci_recvr->thread, NULL, hci_receiver_thread, (void *)hci_recvr);

	xfunc_out();
	return 0;
}

int hci_receiver_delete(msg_thr_t *hci_recvr)
{
	pthread_t thread;

	xfunc_in("thread=0x%08X", (int) hci_recvr->thread);

	if ((thread = hci_recvr->thread)) {
		hci_recvr->thread = (pthread_t) NULL;
		msg_send(&hci_recvr->msg_cb, 0, THREAD_EXIT_MSG);
		pthread_join(thread, NULL);
	}

	xfunc_out();
	return 0;
}

#if defined(TRANSPORT_TEST)
#define TYPE_NONE		0
#define TYPE_DL			1
#define TYPE_UL			2
#define TYPE_LOOPBACK	3
#define TYPE_BI_DIR		4
#define TYPE_MAX		4

#define PATTERN_ZERO_FILLED	0
#define PATTERN_INCREMENTAL	1
#define PATTERN_RANDOM		2
#define PATTERN_MAX			2

#define PACKETSIZE_MAX		1400


typedef struct trans_test_s {
	unsigned char type;
	unsigned char pattern;
	unsigned short outer_loop;
	unsigned short inner_loop;
	unsigned short packet_size;

} __attribute__((packed)) trans_test_t;

typedef struct trans_mng_s {
	trans_test_t tt;

	int mismatches;

} trans_mng_t;

typedef struct trans_packet_s {
	unsigned short seq_nr;
	unsigned char packet[0];	
	
} __attribute__((packed)) trans_packet_t;

typedef struct trans_phase_s {
	int outer_loop;
	int inner_loop;

} trans_phase_t;

static trans_mng_t trans_mng;
static trans_phase_t ul_phase;
static trans_phase_t dl_phase;

static char send_hci[HCI_MAX_PACKET] __attribute__((aligned(1024)));
static unsigned char packet_data[HCI_MAX_PACKET];

static void fill_incremental_buf(void *buf, int len, unsigned char start)
{
	unsigned char *p = buf;
	int i;

	for (i = 0; i < len; i++)
		*p++ = start++;
}

static void fill_random_buf(void *buf, int len)
{
	unsigned short *p = (unsigned short *) buf;
	unsigned char *bp = (unsigned char *) buf;
	int slen = len / 2;
	int i;

	for (i = 0; i < slen; i++)
		*p++ = rand();

	if (len & 1)
		bp[len-1] = rand();
}

static void init_send_hci(trans_mng_t *ptm)
{
	struct hci *hci;
	trans_packet_t *data;
	int len = ptm->tt.packet_size;
	unsigned short length = sizeof(data->seq_nr) + len;

	if (ptm->tt.type == TYPE_UL || ptm->tt.type == TYPE_LOOPBACK) {
		hci = (struct hci *) send_hci;
		data = (trans_packet_t *) hci->data;

		hci->cmd_evt = H2B(WIMAX_UL_TRANSPORT_TEST);
		hci->length = H2B(length);
		if (ptm->tt.pattern == PATTERN_ZERO_FILLED)
			memset(data->packet, 0, len);
		else if (ptm->tt.pattern == PATTERN_INCREMENTAL)
			fill_incremental_buf(packet_data, sizeof(packet_data), 0);
		else if (ptm->tt.pattern == PATTERN_RANDOM)
			fill_random_buf(packet_data, sizeof(packet_data));
	}
}

static struct hci *get_hci_buf(trans_mng_t *ptm, unsigned char seq_nr)
{
	struct hci *hci = (struct hci *) send_hci;
	trans_packet_t *data = (trans_packet_t *) hci->data;

	if (ptm->tt.pattern == PATTERN_INCREMENTAL || ptm->tt.pattern == PATTERN_RANDOM)
		memcpy(data->packet, &packet_data[seq_nr], ptm->tt.packet_size);

	return hci;
}

static unsigned int elapsed_time_us(struct timeval *start)
{
	struct timeval tv, now;
	unsigned int us;

	gettimeofday(&now, NULL);

	timersub(&now, start, &tv);

	us = tv.tv_sec * 1000000 + tv.tv_usec;
	return us;
}

static unsigned int get_throughput(trans_mng_t *ptm, unsigned int us)
{
	int loop = ptm->tt.inner_loop;
	int packet_size = ptm->tt.packet_size;
	unsigned int sent;
	unsigned int bps;
	double sec;

	if (ptm->tt.type == TYPE_DL)
		loop--;

	sent = (HCI_HEADER_SIZE + sizeof(trans_packet_t) + packet_size)
			* 8/*bits*/ *loop;

	sec = (double)us / 1000000;
	bps = sent / sec;
	return bps;
}

static void print_throughput(const char *title, trans_mng_t *ptm, double bps)
{
	char str[1024], *p = str;
	#define Kbits		(1024)
	#define Mbits		(1024*Kbits)

	if (bps > Mbits)
		p += sprintf(p, "%s: %.2f Mbits", title, bps/Mbits);
	else if (bps > Kbits)
		p += sprintf(p, "%s: %.2f Kbits", title, bps/Kbits);
	else
		p += sprintf(p, "%s: %.2f bits", title, bps);

	if (ptm->mismatches)
		p += sprintf(p, ", mismatches=%d", ptm->mismatches);

	xprintf(SDK_NOTICE, "%s\n", str);
}

static void report_throughput(trans_mng_t *ptm, char *name,
								struct timeval *start, int index)
{
	char title[128];
	unsigned int elapsed_us;
	double bps;

	elapsed_us = elapsed_time_us(start);
	sprintf(title, "%d. %s(bps)", index, name);

	bps = get_throughput(ptm, elapsed_us);

	print_throughput(title, ptm, bps);
}

static void ul_transport_send(int dev_idx, trans_mng_t *ptm, int seq_nr)
{
	struct hci *hci;
	trans_packet_t *data;
	int len;

	hci = get_hci_buf(ptm, seq_nr);
	data = (trans_packet_t *) hci->data;
	data->seq_nr = H2B(seq_nr);

	len = B2H(hci->length);

	io_send(dev_idx, hci, len+HCI_HEADER_SIZE);
}

static void ul_transport_test(int dev_idx, trans_mng_t *ptm)
{
	struct timeval start;
	int i, j;

	assert(ptm->tt.type == TYPE_UL);

	for (i = 0; i < ptm->tt.outer_loop; i++) {
		gettimeofday(&start, NULL);
		for (j = 0; j < ptm->tt.inner_loop; j++)
			ul_transport_send(dev_idx, ptm, j);
		report_throughput(ptm, "UL", &start, i);
		sleep(1);
	}
}

static void loopback_test(int dev_idx, trans_mng_t *ptm)
{
	assert(ptm->tt.type == TYPE_LOOPBACK);

	ul_transport_send(dev_idx, ptm, ul_phase.inner_loop);

	if (++ul_phase.inner_loop == ptm->tt.inner_loop) {
		ul_phase.inner_loop = 0;
		ul_phase.outer_loop++;
	}
}

static int dl_transport_handler(int dev_idx, char *buf, int len)
{
	trans_mng_t *ptm = &trans_mng;
	trans_packet_t *data = (trans_packet_t *) buf;
	static struct timeval start;
	unsigned short seq_nr = B2H(data->seq_nr);

	if (!dl_phase.inner_loop)
		gettimeofday(&start, NULL);

	if (ptm->tt.type == TYPE_LOOPBACK)
		loopback_test(dev_idx, ptm);

	if (seq_nr != dl_phase.inner_loop)
		ptm->mismatches++;

	if (++dl_phase.inner_loop == ptm->tt.inner_loop) {
		if (ptm->tt.type == TYPE_LOOPBACK)
			report_throughput(ptm, "Loopback", &start, dl_phase.outer_loop++);
		else
			report_throughput(ptm, "DL", &start, dl_phase.outer_loop++);
		dl_phase.inner_loop = 0;
	}
	
	return 0;
}

static int chk_trans_test_param(trans_mng_t *ptm)
{
	if (ptm->tt.type > TYPE_MAX)
		return 0;
	if (ptm->tt.pattern > PATTERN_MAX)
		return 0;
	if (!ptm->tt.inner_loop)
		return 0;
	if (ptm->tt.packet_size > PACKETSIZE_MAX)
		return 0;

	return 1;
}

static int start_transport_test(int dev_idx, char *buf, int len)
{
	trans_test_t *data = (trans_test_t *) buf;
	trans_mng_t *ptm = &trans_mng;

	memset(ptm, 0, sizeof(trans_mng_t));

	ptm->tt.type = data->type;
	ptm->tt.pattern = data->pattern;
	ptm->tt.outer_loop = B2H(data->outer_loop);
	ptm->tt.inner_loop = B2H(data->inner_loop);
	ptm->tt.packet_size = B2H(data->packet_size);

	xprintf(SDK_NOTICE, "transt %d %d %d %d %d\n",
		ptm->tt.type, ptm->tt.pattern, ptm->tt.outer_loop, ptm->tt.inner_loop, ptm->tt.packet_size);

	if (!chk_trans_test_param(ptm)) {
		xprintf(SDK_ERR, "transport test param is invalid\n");
		return -1;
	}

	memset(&dl_phase, 0, sizeof(dl_phase));
	memset(&ul_phase, 0, sizeof(ul_phase));
	init_send_hci(ptm);

	switch (ptm->tt.type) {
		case TYPE_UL:
			ul_transport_test(dev_idx, ptm);
			break;
		case TYPE_LOOPBACK:
			loopback_test(dev_idx, ptm);
			break;
	}
	return 0;
}
#endif
