// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(NDS_H_20080710)
#define NDS_H_20080710
#include "global.h"

void nds_init(int dev_idx);
void nds_deinit(int dev_idx);
int nds_req_scan(int dev_idx, u8 scan_type, u8 *home_nsp_id, u8 *usr_home_nai);
void nds_clean_scan_list(struct wimax_s *wm);
int nds_update_scan_result(int dev_idx, u8 *buf, int len);
int nds_connect_complete(int dev_idx, u8 *buf, int len);
int nds_associate_start(int dev_idx, u8 *buf, int len);
int nds_associate_complete(int dev_idx, u8 *buf, int len);
int nds_disconnect_ind(int dev_idx, u8 *buf, int len);
int nds_ho_start(int dev_idx, u8 *buf, int len);
int nds_ho_complete(int dev_idx, u8 *buf, int len);
int nds_connection_stage(int dev_idx, u8 *buf, int len);
#if defined(CONFIG_ENABLE_BW_SWITCHING_FOR_KT)
void nds_update_switching_bw(int dev_idx);
#endif
#endif
