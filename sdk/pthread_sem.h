// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(PTHREAD_SEM_H_20081208)
#define PTHREAD_SEM_H_20081208

#include <pthread.h>

typedef struct pthread_sem_s {
	int ps_count;
	pthread_mutex_t	ps_lock;
	pthread_cond_t	ps_cond;

} pthread_sem_t;

int pthread_sem_init(pthread_sem_t *psem, int count);
int pthread_sem_destroy(pthread_sem_t *psem);
int pthread_sem_wait(pthread_sem_t *psem);
int pthread_sem_timedwait(pthread_sem_t *psem, int timeout_sec);
int pthread_sem_signal(pthread_sem_t *psem);

#endif
