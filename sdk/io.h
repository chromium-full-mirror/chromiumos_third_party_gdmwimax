// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(IO_H_20080709)
#define IO_H_20080709
#include "global.h"
#include "wm_ioctl.h"

#define IO_TIMEOUT_SEC		10

#define MAX_DNS_SERVERS		2
#define MAX_NET_STR_LEN		256

typedef struct dns_info_s {
	char dns_svr[MAX_DNS_SERVERS][MAX_NET_STR_LEN];
	char domain[MAX_NET_STR_LEN];

} dns_info_t;

typedef struct dhcp_info_s {
	char ip[MAX_NET_STR_LEN];
	char netmask[MAX_NET_STR_LEN];

} dhcp_info_t;

int io_send(int dev_idx, void *buf, int len);
int net_updown(int dev_idx, bool up);
unsigned int net_get_dns_info_mtime(void);
int net_get_dns_info(dns_info_t *dns_info);
int net_get_dhcp_info(int dev_idx, dhcp_info_t *dhcp_info);
int net_ioctl(int dev_idx, wm_req_t *wmr, int cmd);
int net_ioctl_data(int dev_idx, bool get, u16 data_id, void *buf, int size);
#define net_ioctl_get_data(dev_idx, data_id, buf, size)	\
	net_ioctl_data(dev_idx, TRUE, data_id, buf, size)
#define net_ioctl_set_data(dev_idx, data_id, buf, size)	\
	net_ioctl_data(dev_idx, FALSE, data_id, buf, size)

int io_receiver_create(int dev_idx);
int io_receiver_delete(int dev_idx);

#endif
