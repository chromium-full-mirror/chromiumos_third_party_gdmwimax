// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <assert.h>

#include "error.h"
#include "handle.h"
#include "sdk.h"
#include "wimax.h"
#include "device.h"
#include "log.h"


pthread_mutex_t		api_handle_lock;
struct list_head	api_handle_list;

void hand_init(void)
{
	INIT_LIST_HEAD(&api_handle_list);
	pthread_mutex_init(&api_handle_lock, NULL);
}

void hand_deinit(void)
{
	struct list_head *head = &api_handle_list;
	api_hand_t *handle, *tmp;

	pthread_mutex_lock(&api_handle_lock);
	list_for_each_entry_safe(handle, tmp, head, list) {
		list_del(&handle->list);
		if (handle->api)
			sdk_free(handle->api);
		sdk_free(handle);
	}
	pthread_mutex_unlock(&api_handle_lock);
	pthread_mutex_destroy(&api_handle_lock);
}

api_hand_t *hand_alloc_api(void)
{
	struct list_head *head = hand_get_api_handle_list();
	api_hand_t *handle;

	handle = (api_hand_t *) sdk_malloc(sizeof(api_hand_t));
	assert(handle != NULL);
	memset(handle, 0, sizeof(api_hand_t));

	handle->struct_size = sizeof(api_hand_t);
	pthread_mutex_lock(&api_handle_lock);
	list_add(&handle->list, head);
	pthread_mutex_unlock(&api_handle_lock);

	return handle;
}

int hand_free_api(api_hand_t *handle)
{
	pthread_mutex_lock(&api_handle_lock);
	list_del(&handle->list);
	pthread_mutex_unlock(&api_handle_lock);
	sdk_free(handle);

	return 0;
}

dev_hand_t *hand_alloc_dev(int dev_idx)
{
	device_t *dev;
	dev_hand_t *handle;

	if (!(dev = dm_get_dev(dev_idx)))
		return NULL;

	handle = (dev_hand_t *) sdk_malloc(sizeof(dev_hand_t));
	assert(handle != NULL);
	memset(handle, 0, sizeof(dev_hand_t));
	handle->dev_idx = dev_idx;
	handle->struct_size = sizeof(dev_hand_t);

	dm_put_dev(dev_idx);
	return handle;
}

int hand_free_dev(dev_hand_t *handle)
{
	if (handle == NULL) {
		xprintf(SDK_ERR, "handle is NULL\n");
		return sdk_set_errno(ERR_INVALID);
	}

	if (handle->struct_size != sizeof(dev_hand_t)) {
		xprintf(SDK_ERR, "handle->struct_size(0x%08X!=0x%08X) is invalid\n", 
			handle->struct_size, sizeof(dev_hand_t));
		return sdk_set_errno(ERR_INVALID);
	}

	sdk_free(handle);
	return 0;
}

