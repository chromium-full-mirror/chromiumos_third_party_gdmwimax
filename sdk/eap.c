// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "gcttype.h"
#include "error.h"
#include "device.h"
#include "wimax.h"
#include "hci.h"
#include "fload.h"
#include "log.h"
#include "eap.h"
#include "sdk.h"


#define EAP_TMP_DIR			"./eaptmp"
#define EAP_LOG_DIR			"eaplog"
#define PCAP_LOG_FILE		"pcap.pcap"
#define DEC_LOG_FILE		"dec.log"

unsigned int ParsingOuterNAI(const char *Outer_NAI,			// IN
						  char		 *routing_info,			// OUT
						  char		 *wm_decoration,		// OUT
						  char		 *userid,				// OUT
						  char		 *realm)				// OUT
{
	char		  ch;
	enum { OUT_SCOPE, IN_SCOPE };

	unsigned char fRoutingInfo		= OUT_SCOPE;
	unsigned char fWiMAXDecoration	= OUT_SCOPE;
	unsigned char fRealm			= OUT_SCOPE;

	unsigned int  i;
	unsigned int  nRoutingInfo		= 0;	//index for Routing Info.
	unsigned int  nWiMAXDecoration  = 0;	//index for wm_decoration.
	unsigned int  nUserID			= 0;
	unsigned int  nRealm			= 0;

	if( Outer_NAI == NULL )
		return -1;

	if( strlen(Outer_NAI) == 0 )
		return -2;

	if (routing_info)
		routing_info[0] = 0;
	if (wm_decoration)
		wm_decoration[0] = 0;
	if (userid)
		userid[0] = 0;
	if (realm)
		realm[0] = 0;

	for( i = 0; i < strlen(Outer_NAI) ; i++ )
	{
		ch = Outer_NAI[i];

		if( ( ch == ' ') || (ch == '\t') || (ch == '\n') || (ch == '\r')) // skip character.
			continue;

		if( ch == '[' )
		{
			fRoutingInfo = IN_SCOPE;
			if (routing_info)
				routing_info[nRoutingInfo++] = ch;
		}
		else if( (fRoutingInfo == IN_SCOPE ) && ( ch == ']') )
		{
			fRoutingInfo = OUT_SCOPE;
			if (routing_info)
				routing_info[nRoutingInfo++] = ch;
		}
		else if( fRoutingInfo == IN_SCOPE )
		{
			if (routing_info)
				routing_info[nRoutingInfo++] = ch;
		}
		else if( ch == '{')
		{
			fWiMAXDecoration = IN_SCOPE;
			if (wm_decoration)
				wm_decoration[nWiMAXDecoration++] = ch;
		}
		else if( (fWiMAXDecoration == IN_SCOPE) && ( ch == '}') )
		{
			fWiMAXDecoration = OUT_SCOPE;
			if (wm_decoration)
				wm_decoration[nWiMAXDecoration++] = ch;
		}
		else if( fWiMAXDecoration == IN_SCOPE )
		{
			if (wm_decoration)
				wm_decoration[nWiMAXDecoration++] = ch;
		}
		else if( ch == '@' )
		{
			fRealm = IN_SCOPE;
			if (realm)
				realm[nRealm++] = ch;
		}
		else if( fRealm == IN_SCOPE )
		{
			if (realm)
				realm[nRealm++] = ch;
		}
		else
		{
			if (userid)
				userid[nUserID++] = ch;
		}
	}

	if (userid)
		userid[nUserID]						= 0;
	if (realm)
		realm[nRealm]						= 0;
	if (wm_decoration)
		wm_decoration[nWiMAXDecoration]	= 0;
	if (routing_info)
		routing_info[nRoutingInfo]			= 0;	//NULL Terminated String.

	return i;
}

static void *get_eaplog_thread(void *data)
{
	#define FW_PCAP_LOG			"/log/pcap.pcap"
	#define FW_DEC_LOG			"/log/dec.log"

	int dev_idx = (int) data;
	const char *host_file, *target_file;
	int ret, lmask = SDK_INFO;

	xfunc_in("dev=%d", dev_idx);

	target_file = FW_PCAP_LOG;
	host_file = sdk_mng.eap_pcap;
	ret = fl_read_file(dev_idx, target_file, host_file, NULL);
	if (ret > 0)
		xprintf(lmask, "\t%s <= %s (%d bytes)\n", host_file, target_file, ret);

	target_file = FW_DEC_LOG;
	host_file = sdk_mng.eap_dec;
	ret = fl_read_file(dev_idx, target_file, host_file, NULL);
	if (ret > 0)
		xprintf(lmask, "\t%s <= %s (%d bytes)\n", host_file, target_file, ret);

	xfunc_out();
	return NULL;
}

void eap_start_e_eaplog_thread(int dev_idx)
{
	pthread_t pthread;

	xfunc_in();
	pthread_create(&pthread, NULL, get_eaplog_thread, (void *)dev_idx);
	pthread_detach(pthread);
	xfunc_out();
}

void eap_prepare_log_env(int dev_idx, bool enable)
{
	device_t *dev;
	static bool inited;

	if (!(dev = dm_get_dev(dev_idx)))
		return;

	xfunc_in();

	if (enable && !inited && sdk_get_rw_handle()) {
		char path[256];
		char *p_pcap = sdk_mng.eap_pcap;
		char *p_dec = sdk_mng.eap_dec;

		inited = TRUE;

		sprintf(path, "%s/%s", sdk_mng.log_path, EAP_LOG_DIR);
		mkdir(path, 0644);

		sprintf(p_pcap, "%s/%s", path, PCAP_LOG_FILE);
		sprintf(p_dec, "%s/%s", path, DEC_LOG_FILE);
	}

	dm_put_dev(dev_idx);
	xfunc_out();
}

