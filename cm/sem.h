// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(SEM_H_20081208)
#define SEM_H_20081208

#include <pthread.h>

typedef struct sem_s {
	int ps_count;
	pthread_mutex_t	ps_lock;
	pthread_cond_t	ps_cond;

} sem_t;

int sem_init(sem_t *psem, int count);
int sem_destroy(sem_t *psem);
int sem_wait(sem_t *psem);
int sem_timedwait(sem_t *psem, int timeout_sec);
int sem_signal(sem_t *psem);

#endif

