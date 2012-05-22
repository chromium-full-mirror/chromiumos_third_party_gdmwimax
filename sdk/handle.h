// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(HANDLE_H_20080715)
#define HANDLE_H_20080715
#include "global.h"
#include <pthread.h>

#if !defined(MAX_DEVICE)
#define MAX_DEVICE 256
#endif

typedef struct dev_hand_s {
	u32 struct_size;
	struct api_hand_s *api;
	int dev_idx;

} dev_hand_t;

typedef struct api_hand_s {
	u32 struct_size;
	struct list_head list;
	struct sdk_internal_s *sdk;

	void *api;

} api_hand_t;

extern pthread_mutex_t		api_handle_lock;
extern struct list_head		api_handle_list;

static __inline struct list_head *hand_get_api_handle_list(void)
{
	return &api_handle_list;
}

void hand_init(void);
void hand_deinit(void);

api_hand_t *hand_alloc_api(void);
int hand_free_api(api_hand_t *handle);

dev_hand_t *hand_alloc_dev(int dev_idx);
int hand_free_dev(dev_hand_t *handle);
#endif
