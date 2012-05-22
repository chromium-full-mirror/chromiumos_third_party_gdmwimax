// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(FLOAD_H_20080930)
#define FLOAD_H_20080930
#include "global.h"

int bl_upload(int dev_idx, int type, void *buf, int size, void (*progress)(int bytes));
int bl_download(int dev_idx, int type, void *buf, int size, void (*progress)(int bytes));

int fl_read_file(int dev_idx, const char *target_file,
		const char *host_file, void (*progress)(int bytes));

#endif

