// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>

#include "cm.h"
#include "exitcb.h"

#define CM_VERSION "1.4.5"
extern int cm_init(int read_only);
extern int cm_deinit(void);
extern void cmd_run(void);

int main(int argc, char *argv[])
{
	int ret;

	printf("\n");
	printf("***************************************************************************\n");
	printf("*                                                                         *\n");
	printf("*                 WiMax CM Text Version %5s                             *\n", CM_VERSION);
	printf("*                 Copyright (c) 2008-2010 GCT Research, Inc.              *\n");
	printf("*                                                                         *\n");
	printf("***************************************************************************\n\n");	

	init_exit_cb();

	if (argc == 1)
		ret = cm_init(0);	/*Read/Write Mode*/
	else
		ret = cm_init(1);	/*Read Only Mode*/

	cmd_run();

	cm_deinit();

	process_exit_cb();

	printf("Exit CM main\n");

	return ret;
}
