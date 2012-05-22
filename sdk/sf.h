// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(SERVICE_FLOW_H_20100827) && defined(CONFIG_ENABLE_SERVICE_FLOW)
#define SERVICE_FLOW_H_20100827

#include "gcttype.h"

typedef struct wm_service_flow_s {
	WIMAX_SERVICE_FLOW		sf[WIMAX_MAX_SERVICE_FLOW];
	pthread_mutex_t	sfreadlock;
	pthread_mutex_t	sfwritelock;
	pthread_mutex_t	dsxlock;
	pthread_cond_t dsx_cond;
	WIMAX_SF_EVENT last_sf_event;
} wm_service_flow_t;

void sf_init(int dev_idx);
void sf_deinit(int dev_idx);
void BeginSFRead(int dev_idx);
void EndSFRead(int dev_idx);
WIMAX_SERVICE_FLOW *GetNextSF(int dev_idx,
		      WIMAX_SERVICE_FLOW *pSF,
		      UINT8 Direction);
WIMAX_SERVICE_FLOW *GetServiceFlow(int dev_idx,
			   UINT32 SFID);
WIMAX_CLFR_RULE *GetNextClfrRule(WIMAX_SERVICE_FLOW *pSF,
			 WIMAX_CLFR_RULE *pCLFRRule);
WIMAX_CLFR_RULE *GetClfrRule(WIMAX_SERVICE_FLOW *pSF,
		     UINT16 PacketClassfierRuleIndex);
WIMAX_PHS_RULE *GetNextPHSRule(WIMAX_SERVICE_FLOW *pSF,
		       WIMAX_PHS_RULE *pPHSRule);
WIMAX_PHS_RULE *GetPHSRule(WIMAX_SERVICE_FLOW *pSF,
		   UINT8 PHSI);
WIMAX_SF_CC CmdAddSF(int dev_idx,
	     WIMAX_SF_PARAM_P pSFParam,
	     WIMAX_CLFR_RULE_P pClfrRule,
	     WIMAX_PHS_RULE_P pPHSRule);
WIMAX_SF_CC CmdChangeSF(int dev_idx,
		WIMAX_SF_PARAM_P pSFParam,
		WIMAX_CLFR_DSC_ACTION CLFRDSCAction,
		WIMAX_CLFR_RULE_P pClfrRule,
		WIMAX_PHS_DSC_ACTION PHSDSCAction,
		WIMAX_PHS_RULE_P pPHSRule);
WIMAX_SF_CC CmdDeleteSF(int dev_idx,
		WIMAX_SF_PARAM_P pSFParam);
#endif
 
