// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <stdarg.h>
#include <wchar.h>
#include <linux/version.h>
#include <signal.h>

#include "cm.h"

//#define PARALLEL_DHCLIENT_THREAD
//#define FORCE_SYNCHRONOUS_DHCLIENT

#define IP_ACQUISITION_TIMER_MS			1000
#define DISCONNECT_ON_IP_ALLOCATION_TIMEOUT

#define DHCLIENT_APP	"/sbin/dhclient"

static void start_dhclient(int dev_idx)
{
	char *devname = cm_get_dev_idx2name(dev_idx);
	char argv[4][64];
	#if defined(FORCE_SYNCHRONOUS_DHCLIENT)
	char cmd_line[256];
	#endif

	#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,25)
	char ifcfg_file[256] = "/etc/sysconfig/network-scripts/ifcfg-";
	int fd;

	strcat(ifcfg_file, devname);
	if (access(ifcfg_file, F_OK)) {
		if ((fd = open(ifcfg_file, O_CREAT|O_WRONLY|O_TRUNC, 0755)) < 0) {
			cm_eprintf("cannot create ifcfg-%s!!!\n", devname);
			return;
		}
		close(fd);
	}
	#endif

	strcpy(argv[0], DHCLIENT_APP);
	strcpy(argv[1], "-pf");
	sprintf(argv[2], "/var/run/dhclient.%s.pid", devname);
	if (!access(argv[2], F_OK))
		unlink(argv[2]);
	strcpy(argv[3], devname);

	#if defined(FORCE_SYNCHRONOUS_DHCLIENT)
	sprintf(cmd_line, "%s %s %s %s", argv[0], argv[1], argv[2], argv[3]);
	system(cmd_line);
	exit(0);
	#else
	if (execl(argv[0], argv[0], argv[1], argv[2], argv[3], NULL) < 0) {
		cm_eprintf("execl failed\n");
		exit(1);
	}
	#endif
}

static void stop_dhclient(int dev_idx)
{
	char *devname = cm_get_dev_idx2name(dev_idx);
	char dhclient_lock[256];
	FILE *fp;
	pid_t pid;

	sprintf(dhclient_lock, "/var/run/dhclient.%s.pid", devname);
	if ((fp = fopen(dhclient_lock, "r")) != NULL) {
		#if 0
		dev_conf_t *dconf = &cm_dev_conf[dev_idx];
		char dhclient_start[256];
		cm_printf("Release ip ...\n");
		sprintf(dhclient_start, "%s -r -pf %s %s 2>/dev/null",
			DHCLIENT_APP, dhclient_lock, devname);
		system(dhclient_start);
		#endif
		fscanf(fp, "%d", &pid);
		fclose(fp);
		cm_printf("Kill dhclient...\n");
		kill(pid, SIGTERM);
		unlink(dhclient_lock);
	}
	else if (errno != ENOENT) {
		cm_eprintf("Open fail(%s) %s(%d)\n", dhclient_lock, strerror(errno), errno);
	}
}

static void job_after_ip_allocated(int dev_idx)
{
}

static void job_timeout_ip_allocating(int dev_idx)
{
	cm_common_conf_t *pconf = &cm_common_conf;

	cm_printf("Timeout(%d sec.) ip allocating!\n", pconf->ip_allocation_timeout_sec);
	if (pconf->disconnct_on_ip_failure) {
		cm_printf("Disconnect network due to ip allocation failure!\n");
		cm_disconnect_net(dev_idx);
	}
}

static void ip_acquisition_timer_callback(void *data)
{
	cm_common_conf_t *pconf = &cm_common_conf;
	int dev_idx = (int) data;
	dev_conf_t *dconf = &cm_dev_conf[dev_idx];
	char ip[64];
	int restart_timer_ms = IP_ACQUISITION_TIMER_MS;

	if (!cm_get_dhcp_ip(dev_idx, ip)) {
		cm_printf("device[%d] ip=%s\n", dev_idx, ip);
		job_after_ip_allocated(dev_idx);
	}
	else {
		if (dconf->ip_acq_timed < pconf->ip_allocation_timeout_sec*1000) {
			dconf->ip_acq_timed += IP_ACQUISITION_TIMER_MS;
			if (dconf->wimax_status == WIMAX_API_DEVICE_STATUS_Data_Connected) {
				cm_printf("Restart ip acq timer(%d ms.)\n", restart_timer_ms);
				cm_start_timer(&dconf->ip_acq_timer, restart_timer_ms);
			}
		}
		else
			job_timeout_ip_allocating(dev_idx);
	}
}

static void ip_acquisition_timer_start(int dev_idx)
{
	dev_conf_t *dconf = &cm_dev_conf[dev_idx];
	cm_init_timer(&dconf->ip_acq_timer, ip_acquisition_timer_callback,
		(void *)dev_idx);
	dconf->ip_acq_timed = 1;
	cm_start_timer(&dconf->ip_acq_timer, IP_ACQUISITION_TIMER_MS);
}

static int run_dhclient(int dev_idx)
{
	dev_conf_t *dconf = &cm_dev_conf[dev_idx];
	pid_t pid;
	int ret = 0, i, status;

	pid = fork();
	if (pid == 0) {
		/*Close all FDs*/
		for (i = 3; i < 64; i++)
			close(i);

		cm_printf("Starting dhclient...\n");
		start_dhclient(dev_idx);
		cm_printf("Nerver returns...\n");
	}
	else if (pid != -1) {
		dconf->dhclient_pid = pid;
		ip_acquisition_timer_start(dev_idx);
		cm_dprintf("+waitpid(%d)\n", pid);
		waitpid(pid, &status, 0);
		cm_dprintf("-waitpid(%d)\n", pid);
		dconf->dhclient_pid = 0;
		if ((!WIFEXITED(status) || WEXITSTATUS(status))) {
			cm_eprintf("waitpid: status=%d!\n", WEXITSTATUS(status));
			return -1;
		}
	}
	else
		cm_printf("fork failed\n");
	return ret;
}

#if defined(PARALLEL_DHCLIENT_THREAD)
static void *run_dhclient_thread(void *data)
{
	int dev_idx = (int) data;

	cm_dprintf("Start dhclient thread(%d)\n", dev_idx);
	run_dhclient(dev_idx);
	cm_dprintf("End dhclient thread(%d)\n", dev_idx);
	return NULL;
}
#endif

static int test_kill_dhclient_pid(int dev_idx)
{
	dev_conf_t *dconf = &cm_dev_conf[dev_idx];

	if (dconf->dhclient_pid > 0) {
		cm_dprintf("kill dhclient(pid=%d)\n", dconf->dhclient_pid);
		kill(dconf->dhclient_pid, SIGTERM);
		dconf->dhclient_pid = 0;
		return 1;
	}
	return 0;
}

static void kill_dhclient(int dev_idx)
{
	test_kill_dhclient_pid(dev_idx);
	stop_dhclient(dev_idx);
}

#define DHCLIENT_ON		(int)(1)
#define DHCLIENT_OFF	(int)(0)
#define DHCLIENT_EXIT	(int)(-1)

static void *cm_dhclient_thread(void *data)
{
	cm_common_conf_t *pconf = &cm_common_conf;
	dev_conf_t *dconf;
	cm_msg_cb_t *msg_cb;
	int dev_idx = DEFAULT_DEVICE, event;

	msg_cb = &pconf->dhclient_msg;
	cm_msg_init(msg_cb);

	cm_printf("dhclient thread is ready!\n");
	stop_dhclient(dev_idx);
	while (1) {
		dev_idx = DEFAULT_DEVICE;
		if (cm_msg_recv(msg_cb, &dev_idx, (void **) &event) < 0) {
			cm_eprintf("dhclient cm_msg_recv error\n");
			break;
		}

		if (event == DHCLIENT_ON) {
			dconf = &cm_dev_conf[dev_idx];
			#if defined(PARALLEL_DHCLIENT_THREAD)
			{
				pthread_t thread;
				pthread_create(&thread, NULL, run_dhclient_thread, (void *)dev_idx);
				pthread_detach(thread);
			}
			#else
			run_dhclient(dev_idx);
			#endif
		}
		else if (event == DHCLIENT_OFF)
			kill_dhclient(dev_idx);
		else {	/*DHCLIENT_EXIT*/
			break;
		}
	}
	
	cm_msg_deinit(msg_cb);
	pconf->dhclient_thr = (pthread_t) NULL;
	cm_printf("dhclient thread finished!\n");
	return NULL;
}

void dh_create_dhclient(void)
{
	cm_common_conf_t *pconf = &cm_common_conf;

	if (pconf->dhclient_thr) {
		cm_eprintf("dhclient thread has been started already!\n");
		return;
	}
	pthread_create(&pconf->dhclient_thr, NULL, cm_dhclient_thread, NULL);
}

void dh_delete_dhclient(void)
{
	cm_common_conf_t *pconf = &cm_common_conf;
	pthread_t thread;

	if ((thread = pconf->dhclient_thr)) {
		cm_msg_send(&pconf->dhclient_msg, DEFAULT_DEVICE, (void *) DHCLIENT_EXIT);
		pthread_join(thread, NULL);
		cm_dprintf("dhclient thread deleted!\n");
	}
}

void dh_start_dhclient(int dev_idx)
{
	cm_common_conf_t *pconf = &cm_common_conf;

	if (pconf->api_mode == GCT_WIMAX_API_PRIVILEGE_READ_ONLY)
		return;

	test_kill_dhclient_pid(dev_idx);
	cm_msg_send(&pconf->dhclient_msg, dev_idx, (void *) DHCLIENT_ON);
}

void dh_stop_dhclient(int dev_idx)
{
	cm_common_conf_t *pconf = &cm_common_conf;

	if (pconf->api_mode == GCT_WIMAX_API_PRIVILEGE_READ_ONLY)
		return;

	test_kill_dhclient_pid(dev_idx);
	cm_msg_send(&pconf->dhclient_msg, dev_idx, (void *) DHCLIENT_OFF);
}
