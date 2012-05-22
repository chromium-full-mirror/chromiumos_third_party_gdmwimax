// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(DHCLIENT_H_20100409)
#define DHCLIENT_H_20100409

#include <netinet/in.h>
#include "../sdk/gctapi.h"
#include "cm_msg.h"

void dh_start_dhclient(int dev_idx);
void dh_stop_dhclient(int dev_idx);
void dh_create_dhclient(void);
void dh_delete_dhclient(void);

#endif
