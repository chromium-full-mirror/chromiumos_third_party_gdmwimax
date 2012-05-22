// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(CM_MSG_H_20100407)
#define CM_MSG_H_20100407
#include <pthread.h>

typedef struct cm_msg_s {
	struct cm_msg_s	*ms_next;
	int				ms_prim;
	void			*ms_data;

} cm_msg_t;

typedef struct cm_msg_cb_s {
	struct cm_msg_s	*mc_msg_pool;
	int				mc_pool_cnt;

	struct cm_msg_s	*mc_head, 
					*mc_tail;

	pthread_mutex_t	mc_lock;
	pthread_cond_t	mc_cond;

} cm_msg_cb_t;

void cm_msg_init(cm_msg_cb_t *msg_cb);
void cm_msg_deinit(cm_msg_cb_t *msg_cb);
void cm_msg_send(cm_msg_cb_t *msg_cb, int prim, void *data);
int cm_msg_recv(cm_msg_cb_t *msg_cb, int *prim, void **data);

#endif

