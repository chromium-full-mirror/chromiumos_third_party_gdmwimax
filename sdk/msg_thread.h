// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(MSG_THREAD_H_20100407)
#define MSG_THREAD_H_20100407
#include <pthread.h>
#include "msg.h"

#define USE_PTHREAD_SEM
#if defined(USE_PTHREAD_SEM)
#include "pthread_sem.h"
#endif

typedef struct msg_thr_s {
	pthread_t			thread;
	msg_cb_t			msg_cb;
	#if defined(USE_PTHREAD_SEM)
	pthread_sem_t		sync_lock;
	#endif

} msg_thr_t;

#endif

