// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(MSG_H_20080815)
#define MSG_H_20080815
#include <pthread.h>

typedef struct msg_s {
	struct msg_s	*ms_next;
	int				ms_prim;
	void			*ms_data;

} msg_t;

typedef struct msg_cb_s {
	struct msg_s	*mc_msg_pool;
	int				mc_pool_cnt;

	struct msg_s	*mc_head, 
					*mc_tail;

	pthread_mutex_t	mc_lock;
	pthread_cond_t	mc_cond;

} msg_cb_t;

void msg_init(msg_cb_t *msg_cb);
void msg_deinit(msg_cb_t *msg_cb);
void msg_send(msg_cb_t *msg_cb, int prim, void *data);
int msg_recv(msg_cb_t *msg_cb, int *prim, void **data);

#endif

