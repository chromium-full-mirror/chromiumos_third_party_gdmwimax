// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(TIMER_H_07282008)
#define TIMER_H_07282008
#include <pthread.h>
#include <sys/time.h>

#define PTHREAD_TIMER

#if defined(PTHREAD_TIMER)
#include "list.h"

typedef struct timer_obj_s {
	struct list_head	to_list;
	unsigned			to_created;
	pthread_mutex_t		to_lock;
	struct timespec		to_ts;
	int					to_active;

	void *to_data;
	void (*to_callback)(void *data);

	/*debug info*/
	void *to_caller;	
} timer_obj_t;
#else
typedef struct timer_obj_s {
	unsigned to_created;
	void *to_id;
	void *to_data;
	void (*to_callback)(void *data);
	
} timer_obj_t;
#endif

static inline const char *get_cur_time(void)
{
	static char buf[64];
	time_t t;
	struct tm *tm;
	struct timeval tv;

	time(&t);
	tm = localtime(&t);
	gettimeofday(&tv, NULL);

	sprintf(buf, "%02d:%02d:%02d:%03d",
		tm->tm_hour, tm->tm_min, tm->tm_sec, (int)(tv.tv_usec/1000));
	return buf;
}

static inline const char *get_date(time_t *t)
{
	static char buf[64];
	struct tm *tm;
	
	tm = localtime(t);
	sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
		tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
	return buf;
}

static inline const char *get_cur_date(void)
{
	time_t t;

	time(&t);
	return get_date(&t);
}

#define time2ms(time_ptr)	(((time_ptr)->tv_sec * 1000) + ((time_ptr)->tv_usec / 1000))

static inline int gettimemsofday(void)
{
	struct timeval time;

	gettimeofday(&time, NULL);
	return time2ms(&time);
}

#if defined(PTHREAD_TIMER)
int timer_module_init(void);
void timer_module_deinit(void);
#else
#define timer_module_init()
#define timer_module_deinit()
#endif

int init_timer(timer_obj_t *timer, void (*callback)(void *), void *data);
int start_timer(timer_obj_t *timer, int expire_milisec);
#if defined(PTHREAD_TIMER)
int stop_timer(timer_obj_t *timer);
#else
#define stop_timer(timer)	start_timer(timer, 0)
#endif
int del_timer(timer_obj_t *timer);

#endif

