// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(CM_H_20081007)
#define CM_H_20081007

#include <netinet/in.h>
#include "../sdk/gctapi.h"
#include "cm_timer.h"
#include "cm_msg.h"
#define MAX_DEV				8
#define MAX_PROFILE_LIST	8
#define MAX_NETWORK_LIST	16

#define GCT_WM_PREFIX		"wm"

#define DEFAULT_DEVICE		(int)(1)

#define HCI_MAX_PACKET				2048
#define HCI_HEADER_SIZE				4

#define WIMAX_DISCONN_IND			0x8115
#define WIMAX_IP_RENEW_IND			0x811a

#define WIMAX_DM_CMD				0x030a
#define WIMAX_DM_RSP				0x830b

#define WIMAX_CLI_CMD				0x030c
#define WIMAX_CLI_RSP				0x830d

#define END_OF_BLOCK	(-1)
#define END_OF_IMAGE	(-2)
#define SEQUENCE_READ	(-1)
#define DL_DATA_CHUCK		1024

#define ENC_HEAD_SIZE	4

#define WIMAX_DL_IMAGE				0x0310
#define WIMAX_DL_IMAGE_STATUS		0x8311
#define WIMAX_UL_IMAGE				0x0312
#define WIMAX_UL_IMAGE_RESULT		0x8313
#define WIMAX_UL_IMAGE_STATUS		0x0314
#define WIMAX_IMAGE_CMD				0x0316
	#define ICMD_INVALIDATE			0
	#define ICMD_GET_SIZE			1
	#define ICMD_GET_CRC32			2
#define WIMAX_IMAGE_CMD_STATUS		0x8317

#define WIMAX_UICC_STATUS_IND		0x8318
#define WIMAX_UICC_EAP_IND			0x8319
#define WIMAX_UICC_CMD				0x031A
	#define UICC_CMD_PIN_ENABLE		0x00
	#define UICC_CMD_PIN_DISABLE	0x01
	#define UICC_CMD_PIN_CHANGE		0x02
	#define UICC_CMD_PIN_VERIFY		0x03
	#define UICC_CMD_PUK_INPUT		0x04
	#define UICC_CMD_UICC_STATUS	0x05
	#define UICC_CMD_SUBSCRIBERID	0x06
	#define UICC_CMD_ICCID			0x07
	#define UICC_CMD_POWER_DOWN		0x08
	#define UICC_CMD_POWER_UP		0x09
#define WIMAX_UICC_CMD_RESULT		0x831B
	#define UICC_CMD_RESULT_SUCCESS	0x00
	#define UICC_CMD_RESULT_FAIL	0x01

#define EOF_OFFSET					-1
#define WIMAX_READ_FILE				0x0350
#define WIMAX_WRITE_FILE			0x0351
#define WIMAX_DELETE_FILE			0x0352
#define WIMAX_FILE_RESULT			0x8354

#define WIMAX_MANAGER_MSG			0x8333

#define WIMAX_EAP_STR_LEN			256
#define MAX_EAP_BUF_SIZE			2048

/* Download image type */
#define DLIMG_KERNEL		0
#define DLIMG_FS			1
#define DLIMG_CFG			2
#define DLIMG_EAP_PARM		6
#define DLIMG_BL_EEPROM		7
#define DLIMG_BL_FLASH		8
#define DLIMG_OMA_XML		0x100
#define DLIMG_DEV_CERT		0x101
#define DLIMG_CERT1_U		0x102
#define DLIMG_CERT1_L		0x103
#define DLIMG_CERT2_U		0x104
#define DLIMG_EAP_PARAM		0x105
#define DLIMG_CERT_BIG		0x106

#define WIMAX_PRINT_PAD				8

typedef struct hci {
	unsigned short 	cmd_evt;
	unsigned short 	length;
	unsigned char 	data[0];	
} __attribute__((packed)) hci_t;

typedef struct hci_image_payload {
	unsigned short	type;
	unsigned int	offset;
	char 			data[0];
} __attribute__((packed)) hci_image_payload_t;

typedef struct hci_image_response {
	unsigned short 	type;
	unsigned int	offset;
	int			result;
} __attribute__((packed)) hci_image_response_t;

typedef struct hci_image_cmd {
	unsigned short	cmd;
	unsigned short	type;
} __attribute__((packed)) hci_image_cmd_t;

typedef struct hci_image_cmd_result {
	unsigned short	cmd;
	unsigned short	type;
	unsigned		status;
	unsigned char	data[0];
} __attribute__((packed)) hci_image_cmd_result_t;

typedef struct hci_file_write {
	unsigned int	offset;
	unsigned short	path_len;	/*Include null char*/
	char 			path[0];
	char 			data[0];
} __attribute__((packed)) hci_file_write_t;
#define hci_file_data(p)	(p->path+p->path_len)

typedef struct hci_file_delete {
	char 			path[0];
	char 			data[0];
} __attribute__((packed)) hci_file_delete_t;

typedef struct hci_file_read {
	unsigned int	offset;
	char 			path[0];
	char 			data[0];
} __attribute__((packed)) hci_file_read_t;

typedef struct hci_file_response {
	int			result;
	unsigned short	path_len;
	char 			path[0];
	char 			data[0];
} __attribute__((packed)) hci_file_response_t;
#define file_response_data(r)	\
	(((hci_file_response_t*)(r))->path+_B2H(((hci_file_response_t*)(r))->path_len))

#define _B2H(x)			ntohs(x)
#define _H2B(x)			htons(x)
#define _DB2H(x)		ntohl(x)
#define _DH2B(x)		htonl(x)

#define _U82U16(b)	((((u16)((u8*)b)[0])<<8)|((u16)((u8*)b)[1]))
#define _U82U24(b)	((((u32)((u8*)b)[0])<<16)|(((u32)((u8*)b)[1])<<8)|((u32)((u8*)b)[2]))
#define _U82U32(b)	(((u32)_U82U16(b)<<16)|(u32)_U82U16((u8*)(b)+2))
#define _U162U8(b,v)	((((u8*)b)[0])=((u16)(v)>>8),(((u8*)b)[1])=(u8)((u16)(v)))
#define _U242U8(b,v)	((((u8*)b)[0])=(u8)((u32)(v)>>16),(((u8*)b)[1])=(u8)((u32)(v)>>8),\
						(((u8*)b)[2])=(u8)((u32)(v)))
#define _U322U8(b,v)	(_U162U8((u8*)(b)+2, v), _U162U8(b, (u32)v>>16))

#define _set_msb(x32, x8)	(x32 = ((u32)x32 & 0x00ffffff) | ((u32)x8<<24))
#define _get_msb(x32)		((u8)((u32)x32>>24))

#ifndef offsetof
#define	offsetof(type, field)	((int)(&((type*)0)->field))
#endif

#define _numof_array(arr)	(sizeof(arr)/sizeof(arr[0]))

#define FW_UPDATE_REPORT_TRY_CNT		5
#define DRMD_REPORT_TRY_CNT				FW_UPDATE_REPORT_TRY_CNT

/*Auto connection*/
#define NETWORK_LIST_TIMEOUT_SEC		(0x7fffffff)

typedef struct dev_conf_s {
	char mac_str[32];
	char type_str[256];
	CAPABILITY_BIT cap;
	GCT_API_EAP_PARAM eapp;
	GCT_API_ODM_PARAM odmp;

	WIMAX_API_DEVICE_STATUS wimax_status;
	u16 nsp_name16_before[MAX_SIZE_OF_NSP_NAME];
	u16 nsp_name16_current[MAX_SIZE_OF_NSP_NAME];

	pid_t			dhclient_pid;
	cm_timer_obj_t	ip_acq_timer;
	int				ip_acq_timed;

	unsigned		auto_conn_retry_cnt;

	unsigned fw_app_version;
	unsigned fw_mac_version;

} dev_conf_t;

typedef struct cm_common_conf_s {
	char		log_path[256];
	int			log_level;
	int			eap_log_enable;
	int			embedded_eap_enable;
	int			oma_dm_enable;
	char		nonvolatile_dir[128];
	char		run_script_file[128];
	int			auto_connect_enable;
	unsigned	auto_connect_retry_count;
	int			auto_select_profile_index;
	int			unknown_net_auto_connect_enable;
	int			ip_allocation_timeout_sec;
	int			disconnct_on_ip_failure;

	pthread_t		auto_conn_thr;
	cm_msg_cb_t		auto_conn_msg;

	pthread_t		dhclient_thr;
	cm_msg_cb_t		dhclient_msg;

	GCT_WIMAX_API_OPEN_MODE api_mode;
	#if defined(CONFIG_DM_INTERFACE)
	int		dm_interface_enable;
	int		dm_interface_cfd;	/*socket fd*/
	#endif

} cm_common_conf_t;

#define CM_USE_EEAP	(cm_common_conf.embedded_eap_enable)
#define CM_USE_ODM	(cm_common_conf.oma_dm_enable)

#define CAP_ENC_XML_ENABLED(dev_idx)		(cm_dev_conf[dev_idx].cap & CAPABILITY_ENC_XML)
#define CAP_EEAP_TLS_ENABLED(dev_idx)		(cm_dev_conf[dev_idx].cap & CAPABILITY_E_EAP_TLS)
#define CAP_ODM_ENABLED(dev_idx)			(cm_dev_conf[dev_idx].cap & CAPABILITY_ODM)
#define CAP_EEAP_AKA_ENABLED(dev_idx)		(cm_dev_conf[dev_idx].cap & CAPABILITY_E_EAP_AKA)
#define CAP_CAPLINFO_ENABLED(dev_idx)		(cm_dev_conf[dev_idx].cap & CAPABILITY_CAPL_INFO)
#define CAP_EEAP_ENABLED(dev_idx)			(CAP_EEAP_TLS_ENABLED(dev_idx) || CAP_EEAP_AKA_ENABLED(dev_idx))

extern dev_conf_t cm_dev_conf[];
extern cm_common_conf_t cm_common_conf;
extern APIHAND cm_api_handle;

#define CONF_FILE						"./cm.conf"

#define CM_LOG_FLAG						(LOG_FLAG_NO_STDOUT_IN_LOGFILE)
#if 1
#define cm_print_log(api, flag, fmt, args ...)	do {\
		if (api)\
			GAPI_PrintLog(api, flag, "CM", fmt, ## args);\
		else {\
			printf(fmt, ## args);\
			fflush(stdout);\
		}\
	} while (0)
#else
#define cm_print_log(api, fmt, args ...)	do {printf(fmt, ## args);fflush(stdout);} while (0)
#endif

#define cm_dprintf(fmt, args ...)	do {\
		if (cm_common_conf.log_level > 1)\
			cm_print_log(cm_api_handle, 0, "DBG " fmt, ## args);\
	} while (0)

#define cm_printf(fmt, args ...)		cm_print_log(cm_api_handle, 0, fmt, ## args)
#define cm_flag_printf(fmt, args ...)	cm_print_log(cm_api_handle, \
											CM_LOG_FLAG, fmt, ## args)
#define cm_eprintf(fmt, args ...)		cm_print_log(cm_api_handle, 0, \
											"ERR(%s) " fmt, __FUNCTION__, ## args)

#if defined(CONFIG_DM_INTERFACE)
int dmif_init(void);
int dmif_deinit(void);
#endif

char *cm_get_dev_idx2name(int dev_idx);
int cm_get_dhcp_ip(int dev_idx, char *ip);
int cm_get_filesize(const char *file);
int cm_read_file(const char *file, char *buf, int buf_size);
int cm_write_file(const char *file, char *buf, int buf_size, int flags);
#define cm_creat_file(file, buf, buf_size)	\
		cm_write_file(file, buf, buf_size, O_CREAT|O_WRONLY|O_TRUNC)
#define cm_append_file(file, buf, buf_size)	\
		cm_write_file(file, buf, buf_size, O_APPEND)
int cm_cmp_file(const char *file1, const char *file2);
int cm_get_dir_entry(const char *dir, int index, char *filepath, int size);
int cm_load_file_lines(const char *file, int lines, ...);
int cm_store_file_lines(const char *file, int lines, ...);
int cm_profiling_rf_up(int dev_idx);
int cm_auto_connect(int dev_idx);
int cm_disconnect_net(int dev_idx);
void cm_enable_auto_connet(int enable, int dev_idx);
void cm_request_auto_connection(int dev_idx);
void cm_retry_auto_connection(int dev_idx);

#endif
