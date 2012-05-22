// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXITCB_H_04212008
#define EXITCB_H_04212008

int register_exit_cb(void (*cb)(int sig));
int unregister_exit_cb(void (*cb)(int sig));
void init_exit_cb(void);
void process_exit_cb(void);

#endif
