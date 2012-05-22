// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "global.h"
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include "error.h"
#include "wimax.h"
#include "wm_ioctl.h"
#include "io.h"
#include "device.h"
#include "sdk.h"
#include "log.h"

#define dm_dev_ptr		dev_mng.devices

dev_mng_t dev_mng;


static void dm_cleanup_dev(int dev_idx)
{
	xfunc_in("dev=%d", dev_idx);
	dm_device_lock(dev_idx);
	if (dm_dev_ptr[dev_idx]) {
		#if defined(FREE_DEVICE)
		dm_dev_ptr[dev_idx]->ref_cnt = -1;
		#endif
		wm_cleanup_device(dev_idx);

		sdk_free(dm_dev_ptr[dev_idx]);
		dm_dev_ptr[dev_idx] = NULL;
	}
	dm_device_unlock(dev_idx);
	xfunc_out();
}

static int dm_insert_device(int dev_idx, const char *dev_name, bool ind)
{
	dev_mng_t *dm = &dev_mng;
	device_t *dev;

	xfunc_in("dev=%d, ind=%d", dev_idx, ind);

	pthread_mutex_lock(&dm->detect_lock);
	dev = dm_dev_ptr[dev_idx];
	
	if (dev && dev->inserted) {
		xprintf(SDK_ERR, "%s was already inserted\n", dev_name);
		pthread_mutex_unlock(&dm->detect_lock);
		return -1;
	}

	if (dm_dev_ptr[dev_idx] == NULL)
		dm_dev_ptr[dev_idx] = (device_t *) sdk_malloc(sizeof(device_t));
	dev = dm_dev_ptr[dev_idx];
	memset(dev, 0, sizeof(device_t));
	dev->inserted = TRUE;
	strcpy(dev->name, dev_name);
	dev->ind_thr = (pthread_t) NULL;
	dev->net_fd = 0;
	dev->io_nl.fd = 0;

	dm->dev_cnt++;
	pthread_mutex_unlock(&dm->detect_lock);

	pthread_mutex_init(&dev->hci_wait_signal, NULL);
	INIT_LIST_HEAD(&dev->hci_wait_list);

	pthread_mutex_init(&dev->load_lock, NULL);

	pthread_sem_init(&dev->sync_lock, 0);

	if (ind)
		sdk_ind_dev_insert_remove(dev_idx, TRUE);

	xfunc_out("dev_cnt=%d", dm->dev_cnt);
	return 0;
}

static int dm_remove_device(int dev_idx, const char *dev_name)
{
	dev_mng_t *dm = &dev_mng;
	device_t *dev = dm_dev_ptr[dev_idx];
	sdk_internal_t *sdk;

	if (!dev) {
		xprintf(SDK_ERR, "%s was not inserted!\n", dev->name);
		return sdk_set_errno(ERR_INVALID);
	}

	xfunc_in("dev=%d", dev_idx);

	dev->inserted = FALSE;

	if ((sdk = sdk_get_rw_handle()))
		sdk_internal_device_close(sdk, dev_idx);

	sdk_ind_dev_insert_remove(dev_idx, FALSE);

	hci_destroy_resp(dev_idx);
	pthread_mutex_destroy(&dev->hci_wait_signal);

	io_receiver_delete(dev_idx);

	pthread_sem_destroy(&dev->sync_lock);

	pthread_mutex_lock(&dm->detect_lock);
	dm_device_lock(dev_idx);
	#if defined(FREE_DEVICE)
	if (!dev->ref_cnt)
		dm_cleanup_dev(dev_idx);
	#endif
	dm_device_unlock(dev_idx);
	dm->dev_cnt--;
	pthread_mutex_unlock(&dm->detect_lock);

	xfunc_out("dev_cnt=%d", dm->dev_cnt);
	return 0;
}

static int dm_device_listup(void)
{
	FILE *fp;
	char *dev_file = "/proc/net/dev";
	char buf[512], *p_dev;
	int index, ret = 0;
	int dev_delimiter_pos = 6;

	xfunc_in();
	fp = fopen(dev_file, "rt");
	if (fp == NULL) {
		xprintf(SDK_STD_ERR, "%s fopen NULL\n", dev_file);
		ret = sdk_set_errno(ERR_STD);
		goto out;
	}

	while (fgets(buf, sizeof(buf), fp)) {
		buf[dev_delimiter_pos] = '\0';
		p_dev = strstr(buf, WM_DEV);
		if (p_dev) {
			if (sscanf(p_dev, WM_DEV"%d", &index) == 1) {
				dm_insert_device(DEV_BASE_IDX+index, p_dev, FALSE);
				ret++;
			}
		}
	}

	fclose(fp);
out:
	xfunc_out();
	return ret;
}

static int dm_detector_open(dev_mng_t *dm)
{
	int fd;
	struct sockaddr_nl sa;

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (fd < 0) {
		xprintf(SDK_STD_ERR, "detector open\n");
		return sdk_set_errno(ERR_STD);
	}

	memset(&sa, 0, sizeof(sa));
	sa.nl_family = AF_NETLINK;
	sa.nl_groups = RTMGRP_LINK;
	sa.nl_pid = 0;
	if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		xprintf(SDK_STD_ERR, "detector binding\n", fd);
		close(fd);
		return sdk_set_errno(ERR_STD);
	}

	dm->det_fd = fd;

	xprintf(SDK_DBG, "detector open, fd=%d\n", fd);
	return fd;
}

static void dm_detector_close(dev_mng_t *dm)
{
	if (dm->det_fd > 0) {
		close(dm->det_fd);
		xprintf(SDK_DBG, "detector close(%d)\n", dm->det_fd);
		dm->det_fd = 0;
	}
}

static int dm_detect_device(int fd)
{
	#define NETDEV_REG_UNREG	(~0U)
	char buf[1024], *devname;
	struct iovec iov = {buf, sizeof(buf) };
	struct sockaddr_nl sa;
	struct msghdr msg = {(void *)&sa, sizeof(sa), &iov, 1, NULL, 0, 0};
	struct nlmsghdr *nlh;
	int len, index;
	struct ifinfomsg *ifi;
	struct rtattr *rta;
	int ret = 0;

	assert(fd > 0);

	len = recvmsg(fd, &msg, 0);

	if (len <= (sizeof(struct nlmsghdr)+sizeof(struct ifinfomsg))) {
		xprintf(SDK_STD_ERR, "recvmsg(%d), len=%d\n", fd, len);
		ret = sdk_set_errno(ERR_STD);
		goto out;
	}

	nlh = (struct nlmsghdr *)buf;
	ifi = NLMSG_DATA(nlh);
	if (ifi->ifi_change == NETDEV_REG_UNREG &&
		(nlh->nlmsg_type == RTM_NEWLINK || nlh->nlmsg_type == RTM_DELLINK)) {
		rta = IFLA_RTA(ifi);
		len = nlh->nlmsg_len;
		len -= NLMSG_LENGTH(sizeof(*ifi));
		while (RTA_OK(rta, len)) {
			if (rta->rta_type == IFLA_IFNAME) {
				devname =  RTA_DATA(rta);
				if (sscanf(devname, WM_DEV "%d", &index) == 1) {
					if (nlh->nlmsg_type == RTM_NEWLINK)
						ret = dm_insert_device(DEV_BASE_IDX+index, devname, TRUE);
					else { /*RTM_DELLINK*/
						ret = dm_remove_device(DEV_BASE_IDX+index, devname);
						/*
							If Suspend or Hibernate is issued,
							device is inserted immediately after removing.
							In that case, we need a time closing device(i.e. sdk_device_close).
						*/
						sleep(2);
					}
					if (ret < 0)
						goto out;
				}
				else
					xprintf(SDK_DBG, "Other device=%s\n", devname);
				break;
			}
			rta = RTA_NEXT(rta, len);
		}
	}
out:
	return ret;
}

static void *dm_detector_thread(void *data)
{
	dev_mng_t *dm = (dev_mng_t *) data;
	int fd, ret;

	pthread_mutex_init(&dm->detect_lock, NULL);
	fd = dm_detector_open(dm);
	if (fd < 0)
		goto out;

	while (1) {
		ret = dm_detect_device(fd);
		if (ret < 0)
			break;
	}

out:
	dm_detector_close(dm);
	return NULL;
}

int dm_init(void)
{
	dev_mng_t *dm = &dev_mng;
	int ret = 0;

	xfunc_in();

	memset(dm, 0, sizeof(dev_mng_t));

	ret = dm_device_listup();
	if (ret >= 0)
		pthread_create(&dm->detect_thr, NULL, dm_detector_thread, (void *)dm);

	xfunc_out();
	return ret;
}

int dm_deinit(void)
{
	dev_mng_t *dm = &dev_mng;
	pthread_t thread;
	int i;

	xfunc_in();

	pthread_mutex_lock(&dm->detect_lock);
	for (i = 0; i < MAX_DEVICE; i++) {
		if (dm_dev_ptr[i])
			dm_cleanup_dev(i);
	}
	pthread_mutex_unlock(&dm->detect_lock);

	if ((thread = dm->detect_thr)) {
		dm->detect_thr = (pthread_t) NULL;
		pthread_cancel(thread);
		pthread_join(thread, NULL);
	}

	xfunc_out();
	return 0;
}

device_t *dm_get_dev(int dev_idx)
{
	device_t *dev;

	dm_device_lock(dev_idx);
	if (!(dev = dm_dev_ptr[dev_idx])) {
		xprintf(SDK_ERR, "device(%d) is invalid\n", dev_idx);
		sdk_set_errno(ERR_INVALID_DEV);
	}
	#if defined(FREE_DEVICE)
	else
		dev->ref_cnt++;
	#endif
	dm_device_unlock(dev_idx);
	return dev;
}

#if defined(FREE_DEVICE)
void dm_put_dev(int dev_idx)
{
	device_t *dev;

	dm_device_lock(dev_idx);
	dev = dm_dev_ptr[dev_idx];
	assert(dev);
	if (--dev->ref_cnt == 0 && !dev->inserted)
		dm_cleanup_dev(dev_idx);
	dm_device_unlock(dev_idx);
}
#endif

int dm_open_device(int dev_idx)
{
	device_t *dev;
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	if (dev->open_cnt == 0) {
		wm_open_device(dev_idx);
		assert(dev->wimax->subs_list.head.prev && dev->wimax->subs_list.head.next);

		io_receiver_create(dev_idx);
	}

	dev->open_cnt++;
	xprintf(SDK_INFO, "[%d] open device, cnt=%d\n", dev_idx, dev->open_cnt);
	dm_put_dev(dev_idx);
	xfunc_out();
	return ret;
}

int dm_close_device(int dev_idx)
{
	device_t *dev;
	int ret = 0;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return 0;
		
	if (dev->open_cnt == 0) {
		xprintf(SDK_ERR, "dev(%d) was not opened!\n", dev_idx);
		ret = sdk_set_errno(ERR_INVALID);
		goto out;
	}
	
	dev->open_cnt--;

	if (dev->open_cnt == 0) {
		if (dev->inserted) {
			if (dev->wimax->scan.sf_mode && sdk_get_rw_handle())
				wm_disable_sf_mode(dev_idx);
			ret = io_receiver_delete(dev_idx);
		}
		wm_close_device(dev_idx);
	}
out:
	xprintf(SDK_INFO, "[%d] close device, cnt=%d\n", dev_idx, dev->open_cnt);
	dm_put_dev(dev_idx);
	xfunc_out();
	return ret;
}

int dm_get_status(int dev_idx, int *m_status, int *c_status)
{
	fsm_t fsm;
	int ret = 0;

	xfunc_in("m_s=%d, c_s=%d", *m_status, *c_status);

	if (net_ioctl_get_data(dev_idx, SIOC_DATA_FSM, &fsm, sizeof(fsm)) > 0) {
		*m_status = fsm.m_status;
		*c_status = fsm.c_status;
	}

	xfunc_out("m_s=%d, c_s=%d", *m_status, *c_status);
	dm_put_dev(dev_idx);
	return ret;
}

int dm_set_status(int dev_idx, int m_status, int c_status)
{
	fsm_t fsm;
	int ret = 0;
	
	xfunc_in("m_s=%d, c_s=%d", m_status, c_status);

	fsm.m_status = m_status;
	fsm.c_status = c_status;
	ret = net_ioctl_set_data(dev_idx, SIOC_DATA_FSM, &fsm, sizeof(fsm));
	if (ret > 0) ret = 0;

	xfunc_out("m_s=%d, c_s=%d", fsm.m_status, fsm.c_status);
	dm_put_dev(dev_idx);
	return ret;
}

