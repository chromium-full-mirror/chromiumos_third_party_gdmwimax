// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#include "cm.h"

#define MAX_EXIT_CB_CNT		8

typedef void (*exit_cb_t)(int);

static int signal_active = 0;
static exit_cb_t exit_cb_list[MAX_EXIT_CB_CNT];
static int control_signals[] = {
#if 0
	SIGKILL,
	SIGQUIT,
	SIGTSTP,
	SIGABRT,
	SIGTERM,
	SIGSEGV,
#endif
	SIGINT
};

static int ignore_signals[] = {
	SIGPIPE
};


int register_exit_cb(void (*cb)(int sig))
{
	exit_cb_t *cb_list = exit_cb_list;
	int i;

	for (i = 0; i < MAX_EXIT_CB_CNT; i++) {
		if (cb_list[i] == cb)
			return -1;
	}
    
	for (i = 0; i < MAX_EXIT_CB_CNT; i++) {
		if (NULL == cb_list[i]) {
			cb_list[i] = (void *)cb;
			return 0;
		}
	}

	assert(0);
	return -1;
}

int unregister_exit_cb(void (*cb)(int sig))
{
	exit_cb_t *cb_list = exit_cb_list;
	int i;

	for (i = 0; i < MAX_EXIT_CB_CNT; i++) {
		if (cb_list[i] == cb) {
			cb_list[i] = NULL;
			return 0;
		}
	}

	return -1;
}

static void __do_exit_cb(int sig)
{
	exit_cb_t *cb_list = exit_cb_list;
	int nr_sigs = sizeof(control_signals)/sizeof(control_signals[0]);
	int i;

	for (i = 0; i < nr_sigs; i++) {
		if (control_signals[i] == sig)
			break;
	}
	assert(!sig || i < nr_sigs);

	if (0 == signal_active) {
		signal_active = 1;
		
		for (i = 0; i < MAX_EXIT_CB_CNT; i++) {
			if (cb_list[i]) {
				cb_list[i](sig);
				cb_list[i] = NULL;
			}
		}

		if (sig > 0) {
			cm_printf("signal(%d) issued\n", sig);
			exit(0);
		}

		signal_active = 0;
	}
}

static void __ignore_signal_cb(int sig)
{
	cm_printf("signal(%d) ignored\n", sig);
}

static void __exit_cb(void)
{
	if (!signal_active)
		cm_printf("exit() called\n");
}

void init_exit_cb(void)
{
	struct sigaction act;
	int i;

	atexit(__exit_cb);

	memset(exit_cb_list, 0, sizeof(exit_cb_list));
	memset(&act, 0, sizeof(act));

	sigemptyset(&act.sa_mask);
	act.sa_handler = __do_exit_cb;
	for (i = 0; i < sizeof(control_signals)/sizeof(control_signals[0]); i++)
		if (sigaction(control_signals[i], &act, NULL) < 0)
			cm_eprintf("sigaction(%d) error %s(%d)\n", control_signals[i], strerror(errno), errno);

	act.sa_handler = __ignore_signal_cb;
	act.sa_flags = SA_RESTART;
	for (i = 0; i < sizeof(ignore_signals)/sizeof(ignore_signals[0]); i++)
		if (sigaction(ignore_signals[i], &act, NULL) < 0)
			cm_eprintf("sigaction(%d) error %s(%d)\n", ignore_signals[i], strerror(errno), errno);
}

void process_exit_cb(void)
{
	__do_exit_cb(0);
}

