// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <zlib.h>
#include "global.h"
#include "error.h"
#include "wimax.h"
#include "wm_ioctl.h"
#include "io.h"
#include "device.h"
#include "hci.h"
#include "log.h"
#include "fload.h"

typedef const char * ld_file_t;

typedef struct {
	void *p;
	int size;
} ld_buf_t;

typedef struct {
	bool	is_file;
	union {
		ld_file_t path;
		ld_buf_t buf;
	} u;
} ld_dst_t;

int bl_upload(int dev_idx, int type, void *buf, int size, void (*progress)(int bytes))
{
	xprintf(SDK_ERR, "%s: not supported!\n", __func__);
		return -1;
}

int bl_download(int dev_idx, int type, void *buf, int size, void (*progress)(int bytes))
{
	xprintf(SDK_ERR, "%s: not supported!\n", __func__);
		return -1;
}

static int do_read_file(int dev_idx, const char *target_file,
		ld_dst_t *dst, void (*progress)(int bytes))
{
	device_t *dev;
	u8 send_buf[HCI_MAX_PACKET];
	u8 recv_buf[HCI_MAX_PACKET];
	hci_file_read_t *file = (hci_file_read_t *) send_buf;
	hci_file_response_t *rsp;
	u32 offset = 0;
	int fd = 0;
	int ret = -1, len, max_size = 0;
	char *p = NULL;
	u16 cmd;
	void *param;
	int plen;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	xfunc_in("[%d]", dev_idx);

	if (dst->is_file) {
		if ((fd = open(dst->u.path, O_CREAT|O_WRONLY|O_TRUNC, 0644)) < 0) {
			xprintf(SDK_STD_ERR, "open(%s) fail\n", dst->u.path);
			goto out;
		}
	}
	else {
		if (!dst->u.buf.p || !dst->u.buf.size) {
			xprintf(SDK_ERR, "buf=%p, size=%d\n", dst->u.buf.p, dst->u.buf.size);
			goto out;
		}
		p = dst->u.buf.p;
		max_size = dst->u.buf.size;
	}

	strcpy((char *)file->path, target_file);
	cmd = WIMAX_READ_FILE;
	param = file;
	plen = sizeof(hci_file_read_t) + strlen((char *)file->path) + 1/*null*/;

	pthread_mutex_lock(&dev->load_lock);
	while (1) {
		file->offset = DH2B(offset);
		ret = hci_send_wait(dev_idx, cmd, param, plen,
				WIMAX_FILE_RESULT, recv_buf, sizeof(recv_buf), 0, 0, 3);
		if (ret < 0)
			break;

		rsp = (hci_file_response_t *) recv_buf;
		if (!(len = DB2H(rsp->result))) {
			ret = 0;
			break;
		}
		if (len < 0) {
			xprintf(SDK_ERR, "Read file failed(%d)\n", ret);
			ret = -1;
			break;
		}

		if (dst->is_file) {
			ret = write(fd, file_response_data(rsp), len);
			if (ret <= 0) {
				xprintf(SDK_ERR, "Write fail %s(%d!=%d)\n", dst->u.path, ret, len);
				ret = -1;
				break;
			}
		}
		else {
			if (offset + len > max_size) {
				xprintf(SDK_ERR, "buffer is too small(%d > %d)\n",
					offset + len, max_size);
				ret = -1;
				break;
			}
			memcpy(p+offset, file_response_data(rsp), len);
		}

		offset += len;
		if (progress)
			progress(offset);
	}
	pthread_mutex_unlock(&dev->load_lock);

out:
	if (fd > 0)
		close(fd);
	xprintf(SDK_INFO, "Read file size=%d\n", offset);
	if (!ret) {
		if (!offset)
			ret = -ENOENT;
		else
			ret = offset;
	}

	xfunc_out("ret=%d", ret);
	dm_put_dev(dev_idx);
	return ret;
}

int fl_read_file(int dev_idx, const char *target_file,
		const char *host_file, void (*progress)(int bytes))
{
	ld_dst_t dst;
	int ret;
	
	dst.is_file = TRUE;
	dst.u.path = host_file;
	ret = do_read_file(dev_idx, target_file, &dst, progress);

	return ret;
}
