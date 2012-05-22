// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(TIMER_H_07282008)
#define TIMER_H_07282008
#include <pthread.h>
#include <sys/time.h>

#define CM_PTHREAD_TIMER

#if defined(CM_PTHREAD_TIMER)
#include "list.h"

typedef struct cm_timer_obj_s {
	struct list_head	to_list;
	unsigned			to_created;
	pthread_mutex_t		to_lock;
	struct timespec		to_ts;
	int					to_active;

	void *to_data;
	void (*to_callback)(void *data);

	/*debug info*/
	void *to_caller;	
} cm_timer_obj_t;
#else
typedef struct cm_timer_obj_s {
	unsigned to_created;
	void *to_id;
	void *to_data;
	void (*to_callback)(void *data);
	
} cm_timer_obj_t;
#endif

#define cm_time2ms(time_ptr)	(((time_ptr)->tv_sec * 1000) + ((time_ptr)->tv_usec / 1000))

static inline const char *cm_get_date(time_t *t)
{
	static char buf[64];
	struct tm *tm;
	
	tm = localtime(t);
	sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
		tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
	return buf;
}

static inline const char *cm_get_cur_date(void)
{
	time_t t;

	time(&t);
	return cm_get_date(&t);
}

static inline int cm_gettimemsofday(void)
{
	struct timeval time;

	gettimeofday(&time, NULL);
	return cm_time2ms(&time);
}

#if defined(CM_PTHREAD_TIMER)
int cm_timer_module_init(void);
void cm_timer_module_deinit(void);
#else
#define cm_timer_module_init()
#define cm_timer_module_deinit()
#endif

int cm_init_timer(cm_timer_obj_t *timer, void (*callback)(void *), void *data);
int cm_start_timer(cm_timer_obj_t *timer, int expire_milisec);
#if defined(CM_PTHREAD_TIMER)
int cm_stop_timer(cm_timer_obj_t *timer);
#else
#define cm_stop_timer(timer)	cm_start_timer(timer, 0)
#endif
int cm_del_timer(cm_timer_obj_t *timer);

#endif

