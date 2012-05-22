// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <assert.h>

#include "error.h"
#include "device.h"
#include "log.h"


static int sdk_errno;

int sdk_set_errno(int err_no)
{
	int lmask = SDK_ERR;

	switch (err_no) {
		case ERR_PERM:
			xprintf(lmask, "Permission denied.\n");
			break;
	}
	sdk_errno = err_no;

	return -1;
}

int sdk_get_errno(void)
{
	return sdk_errno;
}
