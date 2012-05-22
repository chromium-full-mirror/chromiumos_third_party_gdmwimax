// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(ERROR_H_20080715)
#define ERROR_H_20080715


enum
{
	ERR_STD = 1,
	ERR_INVALID,
	ERR_INVALID_DEV,
	ERR_REMOVED_DEV,
	ERR_PERM,
	ERR_BUSY,
	ERR_ENV,
	ERR_ALREADY,
	ERR_UNKNOWN_STAT,
	ERR_NO_NSPID,
	ERR_UNEXPECTED_TLV,
	ERR_NORESP_CMD,
	ERR_HCI,
	
	END_ERR
};


int sdk_set_errno(int err_no);
int sdk_get_errno(void);
#endif
