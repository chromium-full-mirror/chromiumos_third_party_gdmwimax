// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(DEVICE_H_20080709)
#define DEVICE_H_20080709
#include "global.h"
#include "pthread_sem.h"
#include "hci.h"
#include "wm_ioctl.h"
#include "netlink_u.h"

#define MAX_DEVICE		256
#define DEV_BASE_IDX	1
#define WM_DEV			"wm"

/* #define FREE_DEVICE */

typedef struct device_s {
	bool inserted;
	#if defined(FREE_DEVICE)
	int ref_cnt;
	#endif

	int open_cnt;
	char name[256];

	int ifindex;

	/* Indicator */
	pthread_t ind_thr;

	/*Network*/
	int net_fd;

	/*In/Out*/
	hnetlink_t io_nl;

	/*The list of entries to wait hci.*/
	struct list_head hci_wait_list;
	/*The signal to wake up entries that are waiting for hci.*/
	pthread_mutex_t hci_wait_signal;

	/*For UL/DL Image(or Param)*/
	pthread_mutex_t load_lock;

	/*App status*/
	int a_status;

	fsm_t fsm;

	/*Synchronizer*/
	pthread_sem_t sync_lock;

	struct wimax_s *wimax;

	u32 capability;

	pthread_t dm_thr;
	int dm_listener;
} device_t;

#define ENC_XML_ENABLED(dev)		(dev->capability & W_CAPABILITY_ENC_XML)
#define E_EAP_TLS_ENABLED(dev)		(dev->capability & W_CAPABILITY_E_EAP_TLS)
#define ODM_ENABLED(dev)			(dev->capability & W_CAPABILITY_ODM)
#define E_EAP_AKA_ENABLED(dev)		(dev->capability & W_CAPABILITY_E_EAP_AKA)
#define CAPL_INFO_ENABLED(dev)		(dev->capability & W_CAPABILITY_CAPL_INFO)
#define E_EAP_ENABLED(dev)			(E_EAP_TLS_ENABLED(dev) || E_EAP_AKA_ENABLED(dev))

typedef struct dev_mng_s {
	int dev_cnt;
	device_t *devices[MAX_DEVICE];

	pthread_t detect_thr;
	pthread_mutex_t detect_lock;
	int det_fd;

} dev_mng_t;

extern dev_mng_t dev_mng;


static __inline dev_mng_t *dm_get_dev_mng(void)
{
	return &dev_mng;
}

static __inline bool dm_isopen_dev(device_t *dev)
{
	if (dev->open_cnt == 0)
		return FALSE;
	else
		return TRUE;
}

int dm_init(void);
int dm_deinit(void);
#define dm_tst_dev(dev_idx)		(dev_mng.devices[dev_idx])
device_t *dm_get_dev(int dev_idx);
#if defined(FREE_DEVICE)
void dm_put_dev(int dev_idx);
#define dm_device_lock(dev_idx)
#define dm_device_unlock(dev_idx)
#else
#define dm_put_dev(dev_idx)	do { } while(0)
#define dm_device_lock(dev_idx)
#define dm_device_unlock(dev_idx)
#endif
int dm_open_device(int dev_idx);
int dm_close_device(int dev_idx);
int dm_get_status(int dev_idx, int *m_status, int *c_status);
int dm_set_status(int dev_idx, int m_status, int c_status);

#endif
