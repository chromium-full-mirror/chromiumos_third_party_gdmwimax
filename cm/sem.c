// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sem.h"
#include <sys/time.h>

static struct timespec *sem_get_timespec(struct timespec *ts, int timeout_sec)
{
	#define ps_timeval2timespec(tv,ts)	\
		((ts)->tv_sec = (tv)->tv_sec, (ts)->tv_nsec = (tv)->tv_usec * 1000)
	struct timeval tv;

	gettimeofday(&tv, NULL);
	ps_timeval2timespec(&tv, ts);
	ts->tv_sec += timeout_sec;
	return ts;
}

int sem_init(sem_t *psem, int count)
{
	int ret = 0;

	psem->ps_count = count;

	ret = pthread_mutex_init(&psem->ps_lock, NULL);
	ret |= pthread_cond_init(&psem->ps_cond, NULL);

	return ret;
}

int sem_destroy(sem_t *psem)
{
	int ret = 0;

	psem->ps_count = 0;

	ret = pthread_mutex_destroy(&psem->ps_lock);
	ret |= pthread_cond_destroy(&psem->ps_cond);

	return ret;
}

int sem_wait(sem_t *psem)
{
	int ret = 0;

	pthread_mutex_lock(&psem->ps_lock);

	if (--psem->ps_count < 0)
		ret = pthread_cond_wait(&psem->ps_cond, &psem->ps_lock);

	pthread_mutex_unlock(&psem->ps_lock);

	return ret;
}

int sem_timedwait(sem_t *psem, int timeout_sec)
{
	struct timespec ts;
	int ret = 0;

	pthread_mutex_lock(&psem->ps_lock);

	if (--psem->ps_count < 0) {
		if (timeout_sec) {
			sem_get_timespec(&ts, timeout_sec);
			ret = pthread_cond_timedwait(&psem->ps_cond, &psem->ps_lock, &ts);
		}
		else
			ret = pthread_cond_wait(&psem->ps_cond, &psem->ps_lock);
	}

	pthread_mutex_unlock(&psem->ps_lock);

	return ret;
}

int sem_signal(sem_t *psem)
{
	int ret = 0;

	pthread_mutex_lock(&psem->ps_lock);

	if (psem->ps_count++ < 0) {
		pthread_mutex_unlock(&psem->ps_lock);
		ret = pthread_cond_signal(&psem->ps_cond);
	}
	else
		pthread_mutex_unlock(&psem->ps_lock);

	return ret;
}

