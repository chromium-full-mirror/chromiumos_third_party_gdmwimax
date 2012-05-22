// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(NETLINK_U_H_20081203)
#define NETLINK_U_H_20081203

typedef struct hnetlink_s {
	int fd;
	int ifindex;

} hnetlink_t;

int nl_open(hnetlink_t *hnl, int unit, int ifindex, unsigned int group);
int nl_close(hnetlink_t *hnl);
int nl_send(hnetlink_t *hnl, unsigned short type, void *buf, int len);
int nl_recv(hnetlink_t *hnl, char *buf, int len, int flags);

#endif
