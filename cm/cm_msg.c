// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "cm_msg.h"

#define msg_malloc(s)	malloc(s)
#define msg_free(b)		free(b)

#define MAX_MSG_POOL		10

void cm_msg_init(cm_msg_cb_t *msg_cb)
{
	msg_cb->mc_msg_pool = NULL;
	msg_cb->mc_head = NULL;
	msg_cb->mc_tail = NULL;
	msg_cb->mc_pool_cnt = 0;

	pthread_mutex_init(&msg_cb->mc_lock, NULL);
	pthread_cond_init(&msg_cb->mc_cond, NULL);
}

void cm_msg_deinit(cm_msg_cb_t *msg_cb)
{
	int free_cnt = 0;
	cm_msg_t *msg;

	pthread_mutex_lock(&msg_cb->mc_lock);
	while (msg_cb->mc_msg_pool) {
		msg = msg_cb->mc_msg_pool;
		msg_cb->mc_msg_pool = msg_cb->mc_msg_pool->ms_next;
		msg_free(msg);
		free_cnt++;
	}
	msg_cb->mc_pool_cnt = 0;
	pthread_mutex_unlock(&msg_cb->mc_lock);

	pthread_mutex_destroy(&msg_cb->mc_lock);
	pthread_cond_destroy(&msg_cb->mc_cond);
	assert(free_cnt <= MAX_MSG_POOL);
}

static void put_msg_pool(cm_msg_cb_t *msg_cb, cm_msg_t *msg)
{
	if (!msg_cb->mc_msg_pool) {
		msg->ms_next = NULL;
		msg_cb->mc_msg_pool = msg;
		assert(msg_cb->mc_pool_cnt == 0);
	}
	else {
		msg->ms_next = msg_cb->mc_msg_pool;
		msg_cb->mc_msg_pool = msg;
		assert(msg_cb->mc_pool_cnt > 0);
	}
	msg_cb->mc_pool_cnt++;
}

static cm_msg_t *get_msg_pool(cm_msg_cb_t *msg_cb)
{
	cm_msg_t *msg;

	msg = msg_cb->mc_msg_pool;
	msg_cb->mc_msg_pool = msg->ms_next;
	msg_cb->mc_pool_cnt--;
	assert(msg_cb->mc_pool_cnt >= 0);

	return msg;
}

void cm_msg_send(cm_msg_cb_t *msg_cb, int prim, void *data)
{
	cm_msg_t *msg;

	pthread_mutex_lock(&msg_cb->mc_lock);
	if (!msg_cb->mc_msg_pool) {
		pthread_mutex_unlock(&msg_cb->mc_lock);
		msg = (cm_msg_t *) msg_malloc(sizeof(cm_msg_t));
		assert(msg);
		pthread_mutex_lock(&msg_cb->mc_lock);
	}
	else
		msg = get_msg_pool(msg_cb);

	msg->ms_prim = prim;
	msg->ms_data = data;
	msg->ms_next = NULL;

	if (!msg_cb->mc_head)
		msg_cb->mc_head = msg_cb->mc_tail = msg;
	else {
		msg_cb->mc_tail->ms_next = msg;
		msg_cb->mc_tail = msg;
	}
	pthread_cond_signal(&msg_cb->mc_cond);
	pthread_mutex_unlock(&msg_cb->mc_lock);
}

int cm_msg_recv(cm_msg_cb_t *msg_cb, int *prim, void **data)
{
	cm_msg_t *msg;
	int ret = 0;

	pthread_mutex_lock(&msg_cb->mc_lock);
	if (!msg_cb->mc_head) {
		ret = pthread_cond_wait(&msg_cb->mc_cond, &msg_cb->mc_lock);
		assert(msg_cb->mc_head);
	}

	msg = msg_cb->mc_head;
	msg_cb->mc_head = msg_cb->mc_head->ms_next;

	if (prim)
		*prim = msg->ms_prim;
	if (data)
		*data = msg->ms_data;

	if (msg_cb->mc_pool_cnt < MAX_MSG_POOL) {
		put_msg_pool(msg_cb, msg);
		pthread_mutex_unlock(&msg_cb->mc_lock);
	}
	else {
		pthread_mutex_unlock(&msg_cb->mc_lock);
		msg_free(msg);
	}

	return ret;
}

