// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EAP_H_20081010
#define EAP_H_20081010

#define CR801_SERVER_REJECT_LIMIT	3

#define WIMAX_EAP_STR_LEN			256
#define WIMAX_EAP_FILE_LEN			256
#define WIMAX_EAP_MSK_LEN			64
#define WIMAX_EAP_EMSK_LEN			64

#define MAX_DECORATION_NR			2
#define DECORATION_IDX1				0
#define DECORATION_IDX2				1
#define WIMAX_EAP_DECO_LEN			128

#define MAX_EAP_PARAM_SIZE			256
#define EAP_CHECK_REALM_LIST_SIZE	8

#define MAX_EAP_BUF_SIZE			2048

#define E_WM_CR801_EAP_FAILURE		-201
#define E_WM_UNKNOWN_SERVER_REALM	-202

#define OMA_EAP_SERVER_REALMS	2
#define CR801_DISABLE		0
#define CR801_TTLS			1
#define CR801_TLS			2

typedef struct wm_eap_tls_param_s {
	/*Parameters*/
	u8		type;
	u16		frag_size;
	char	use_delimiter;
	bool	dev_cert_null;
	bool	ca_cert_null;
	bool	disable_resumptoin;
	bool	disable_sessionticket;

	bool	use_nv_info;
	char	userid[WIMAX_EAP_STR_LEN];		/*Inner NAI*/
	char	userid_pwd[WIMAX_EAP_STR_LEN];	/*Inner NAI password*/
	char	anony_id[WIMAX_EAP_STR_LEN];	/*Outer NAI*/
	char	pri_key_pwd[WIMAX_EAP_STR_LEN];
	char	visited_realm[WIMAX_EAP_STR_LEN];

	char	decoration[MAX_DECORATION_NR][WIMAX_EAP_DECO_LEN];

	bool	cr801_enabled;
	u8		cr801_mode;
	int		cr801_server_reject_cnt;

	bool	cr901_enabled;
	bool	cr1074_enabled;
	bool	check_realm;
	int		configured_realm_list_cnt;
	char	configured_realm_list[EAP_CHECK_REALM_LIST_SIZE][MAX_EAP_PARAM_SIZE];

	bool	log_enabled;
} wm_eap_param_t;

void eap_start_e_eaplog_thread(int dev_idx);
void eap_prepare_log_env(int dev_idx, bool enable);

unsigned int ParsingOuterNAI(const char *Outer_NAI,			// IN
						  char		 *routing_info,			// OUT
						  char		 *wm_decoration,		// OUT
						  char		 *userid,				// OUT
						  char		 *realm);
#endif
