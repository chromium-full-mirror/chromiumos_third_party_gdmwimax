// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HCI_H_20080718
#define HCI_H_20080718
#include "global.h"
#include "wimax.h"
#include "msg.h"
#include "msg_thread.h"


#define HCI_HEADER_SIZE		4
#define HCI_VALUE_OFFS		(HCI_HEADER_SIZE)
#define HCI_MAX_PACKET		2048
#define HCI_MAX_PARAM		(HCI_MAX_PACKET-HCI_HEADER_SIZE)
#define HCI_MAX_TLV			32

#define W_ERR_NOTI_CATE_EAP			0
#define W_ERR_NOTI_TYPE_CODE		0
#define W_ERR_NOTI_TYPE_TEXT		1

/* CMD-EVT */

/* Category 0 */
#define WIMAX_RESET					0x0000
#define WIMAX_SET_INFO				0x0001
#define WIMAX_GET_INFO				0x0002
#define WIMAX_GET_INFO_RESULT		0x8003
#define WIMAX_RADIO_OFF				0x0004
#define WIMAX_RADIO_ON				0x0006
#define WIMAX_WIMAX_RESET			0x0007	/* Is this still here */

/* Category 1 */
#define WIMAX_NET_ENTRY				0x0100
#define WIMAX_NET_DISCONN			0x0102
#define WIMAX_ENTER_SLEEP			0x0103
#define WIMAX_EXIT_SLEEP			0x0104
#define WIMAX_ENTER_IDLE			0x0105
#define WIMAX_EXIT_IDLE				0x0106
#define WIMAX_MODE_CHANGE			0x8108
	#define MODE_W_NULL				0x00
	#define MODE_W_STANDBY			0x01
	#define MODE_W_OUT_OF_ZONE		0x02
	#define MODE_W_AWAKE			0x03
	#define MODE_W_IDLE				0x04
	#define MODE_W_SLEEP			0x05
	#define MODE_W_WAIT				0x06
#define WIMAX_HANDOVER				0x8109	/* obsolete */

#define WIMAX_SCAN					0x010d
#define WIMAX_SCAN_COMPLETE			0x810e
#define WIMAX_SCAN_RESULT			0x810f

#define WIMAX_CONNECT				0x0110
#define WIMAX_CONNECT_START			0x8111
#define WIMAX_CONNECT_COMPLETE		0x8112
#define WIMAX_ASSOC_START			0x8113
#define WIMAX_ASSOC_COMPLETE		0x8114
#define WIMAX_DISCONN_IND			0x8115
#define WIMAX_ENTRY_IND				0x8116
#define WIMAX_HO_START				0x8117
#define WIMAX_HO_COMPLETE			0x8118
#define WIMAX_RADIO_STATE_IND		0x8119
#define WIMAX_IP_RENEW_IND			0x811a

#define WIMAX_DISCOVER_NSP			0x011d
#define WIMAX_DISCOVER_NSP_RESULT	0x811e

#define WIMAX_SCAN_CANCEL		0x0122
#define WIMAX_AIRPLANE_MODE_IND	0x8123
#define WIMAX_GET_AIRPLANE_MODE	0x0124
#define WIMAX_FULL_REENTRY		0x8126

/* Category 2 */
#define WIMAX_TX_EAP				0x0200
#define WIMAX_RX_EAP				0x8201
#define WIMAX_TX_SDU				0x0202
#define WIMAX_RX_SDU				0x8203

/* Category 3 */
#define WIMAX_DM_CMD				0x030a
#define WIMAX_DM_RSP				0x830b

#define WIMAX_CLI_CMD				0x030c
#define WIMAX_CLI_RSP				0x830d

#define WIMAX_DL_IMAGE				0x0310
#define WIMAX_DL_IMAGE_STATUS		0x8311
#define WIMAX_UL_IMAGE				0x0312
#define WIMAX_UL_IMAGE_RESULT		0x8313
#define WIMAX_UL_IMAGE_STATUS		0x0314

#define WIMAX_NIC_CMD				0x0315
#define WIMAX_IMAGE_CMD				0x0316
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

#define WIMAX_NOTIFICATION			0x831F
#define WIMAX_ARM_CAPABILITY		0x0320
#define WIMAX_ARM_CAPABILITY_RESULT	0x8321
	#define W_CAPABILITY_ENC_XML	(1<<0)
	#define W_CAPABILITY_E_EAP_TLS	(1<<1)
	#define W_CAPABILITY_ODM		(1<<2)
	#define W_CAPABILITY_E_EAP_AKA	(1<<3)
	#define W_CAPABILITY_CAPL_INFO	(1<<4)

#define WIMAX_DSX_REQUEST			0x0322
#define WIMAX_DSX_COMPLETE			0x8323

#define WIMAX_MODEM_REQUEST			0x0324
	#define MODE_MODEM_REPORT		0
	#define MODE_MODEM_REQUEST		1
	#define W_PDU_STATISTIC			0x50
	#define W_PHY_MAC_BASIC			0x51
	#define W_POWER_CONTROL			0x52
	#define W_PHY_BURST				0x53
	#define W_PHY_MSC				0x54
	#define W_PHY_ZONE				0x55
	#define W_PHY_CINR_RSSI			0x56
	#define W_TEMP_ADC				0x57
	#define W_PHY_CTRL_CHANNEL		0x58
	#define W_DL_SF_STATISTICS		0x59
	#define W_UL_SF_STATISTICS		0x5a
	#define W_HANDOVER				0x5b
	#define W_NEIGHBOR_LIST			0x5c
	#define W_HARQ_STATISTICS		0x5d
	#define W_NETENTRY_STATISTICS	0x5e
#define WIMAX_MODEM_REPORT			0x8325

#define WIMAX_START_TRANSPORT_TEST	0x8330
#define WIMAX_DL_TRANSPORT_TEST		0x8331
#define WIMAX_UL_TRANSPORT_TEST		0x0332

#define WIMAX_NOTIFICATION			0x831F

#define WIMAX_MANAGER_MSG			0x8333
#define WIMAX_SF_IND				0x8334
	#define WIMAX_SF_ENABLE			1
	#define WIMAX_SF_DISABLE		0

#define WIMAX_SET_EAP_INFO			0x0340
	#define W_NO_EAP				0
	#define W_EAP_TLS				1
	#define W_EAP_TTLS_MD5			2
	#define W_EAP_TTLS_MSCHAPV2		3
	#define W_EAP_TTLS_CHAP			4
	#define W_EAP_AKA				5
	#define IS_EAP_TLS(type)		((type)>=W_EAP_TLS && (type)<=W_EAP_TTLS_CHAP)
	#define T_USER_ID				(1	| ( 1 << 16 ))
	#define T_USER_PASSWD			(2	| ( 1 << 16 ))
	#define T_ANONYMOUS_ID			(3	| ( 1 << 16 ))
	#define T_PRIV_KEY_PASSWD		(4	| ( 1 << 16 ))
	#define T_EAP_FORCE(x)			((x)+100)
	#define T_CR801_MODE			(5	| ( 1 << 16 ))
	#define T_DEV_CERT_NULL			(6	| ( 1 << 16 ))
	#define T_CA_CERT_NULL			(7	| ( 1 << 16 ))
	#define T_EAP_PARAM				(8	| ( 1 << 16 ))
		#define V_PARAM_NV			0
		#define V_PARAM_XML			1
		#define V_PARAM_USER		2
	#define T_EAP_DEBUG_ON			(9	| ( 1 << 16 ))
	#define T_EAP_RESUMPTION_OFF	(10	| ( 1 << 16 ))
	#define T_EAP_CW_DEV_CATEGORY	(11	| ( 1 << 16 ))
	#define T_EAP_DECORATION		(12	| ( 0 << 16 ))
	#define T_EAP_RAND_SEED			(13	| ( 4 << 16 ))
	#define T_EAP_SESSIONTICKET_OFF	(14	| ( 1 << 16 ))
#define WIMAX_SET_EAP_INFO_RESULT	0x8341
#define WIMAX_GET_CERT_INFO			0x0342
#define WIMAX_GET_CERT_INFO_RESULT	0x8343
	#define LEVEL_CA_CERT			(1<<0)
	#define LEVEL_CLIENT_CERT		(1<<1)
	#define LEVEL_SERVER_CERT		(1<<2)
	#define T_SUBJECT_CN			1
	#define T_ISSUER_CN				2
	#define T_EXPIRE_DATE			3
#define WIMAX_DELETE_CERT			0x0344
#define WIMAX_DELETE_CERT_RESULT	0x8345

#define WIMAX_READ_FILE				0x0350
#define WIMAX_WRITE_FILE			0x0351
#define WIMAX_DELETE_FILE			0x0352
#define WIMAX_FILE_RESULT			0x8354

/* Download image type */
#define DLIMG_KERNEL		0
#define DLIMG_FS			1
#define DLIMG_CFG			2
#define DLIMG_CFG_RAM		3
#define DLIMG_FS2			4
#define DLIMG_CLR_CFG		5
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

#define DLIMG_TLS_PCAP		0x200
#define DLIMG_TLS_DEC		0x201
#define DLIMG_TLS_DEV_CERT	0x202
#define DLIMG_TLS_CA_CERT	0x203

/* Category 0xF */
#define WIMAX_FSM_UPDATE			0x8F01
#define WIMAX_IF_UPDOWN				0x8F02
	#define WIMAX_IF_UP				1
	#define WIMAX_IF_DOWN			2

/* WIMAX mode */
#define W_NULL				0
#define W_STANDBY			1
#define W_OOZ				2
#define W_AWAKE				3
#define W_IDLE				4
#define W_SLEEP				5
#define W_WAIT				6

#define W_NET_ENTRY_RNG		0x80
#define W_NET_ENTRY_SBC		0x81
#define W_NET_ENTRY_PKM		0x82
#define W_NET_ENTRY_REG		0x83
#define W_NET_ENTRY_DSX		0x84

#define W_NET_ENTRY_RNG_FAIL	0x1100100
#define W_NET_ENTRY_SBC_FAIL	0x1100200
#define W_NET_ENTRY_PKM_FAIL	0x1102000
#define W_NET_ENTRY_REG_FAIL	0x1103000
#define W_NET_ENTRY_DSX_FAIL	0x1104000

/* Scan Type */
#define W_SCAN_ALL_CHANNEL				0
#define W_SCAN_ALL_SUBSCRIPTION			1
#define W_SCAN_SPECIFIED_SUBSCRIPTION	2

/*
 * TLV
 *
 * [31:31] indicates the type is composite.
 * [30:16] is the length of the type. 0 length means length is variable.
 * [15:0] is the actual type.
 * 
 */
#define TLV_L(x)		(((x) >> 16) & 0xff)
#define TLV_T(x)			((x) & 0xff)
#define TLV_COMPOSITE(x)	((x) >> 31)

/* GENERAL */
#define T_MAC_ADDRESS			(0x00	| ( 6 << 16 ))
#define T_BSID					(0x01 	| ( 6 << 16 ))
#define T_MSK					(0x02	| ( 64 << 16 ))
#define T_RSSI_THRSHLD			(0x03	| ( 1 << 16 ))
#define T_FREQUENCY				(0x04	| ( 4 << 16 ))
#define T_CONN_CS_TYPE			(0x05	| ( 1 << 16 ))
#define T_HOST_IP_VER			(0x06	| ( 1 << 16 ))
#define T_STBY_SCAN_INTERVAL	(0x07	| ( 4  << 16 ))
#define T_OOZ_SCAN_INTERVAL		(0x08	| ( 4 << 16 ))
#define T_IMEI					(0x09	| ( 8 << 16 ))
#define T_PID					(0x0a	| ( 12 << 16 ))

#define T_BOOTLOAD_VER			(0x19	| ( 4 << 16 ))
#define T_CAPABILITY			(0x1a	| ( 4 << 16 ))
#define T_RELEASE_NUMBER		(0x1b	| ( 4 << 16 ))
#define T_DRIVER_REVISION		(0x1c	| ( 4 << 16 ))
#define T_FW_REVISION			(0x1d	| ( 4 << 16 ))
#define T_MAC_HW_REVISION		(0x1e	| ( 4 << 16 ))
#define T_PHY_HW_REVISION		(0x1f	| ( 4 << 16 ))

/* HANDOVER */
#define T_SCAN_INTERVAL			(0x20	| ( 1 << 16 ))

#define T_RSC_RETAIN_TIME		(0x2f	| ( 2 << 16 ))

/* SLEEP */
#define T_TYPE1_ISW				(0x40	| ( 1 << 16 ))

#define T_SLP_START_TO			(0x4a 	| ( 2 << 16 ))

/* IDLE */
#define T_IDLE_MODE_TO			(0x50	| ( 2 << 16 ))

#define T_IDLE_START_TO			(0x54	| ( 2 << 16 ))

/* MONITOR */
#define T_RSSI						(0x60	| ( 1 << 16 ))
#define T_CINR						(0x61	| ( 1 << 16 ))
#define T_TX_POWER          	    (0x6a   | ( 1 << 16 ))
#define T_PER						(0x6b   | ( 1 << 16 ))
#define T_UL_PERM_BASE				(0x6c   | ( 1 << 16 ))
#define T_DL_PERM_BASE				(0x6d   | ( 1 << 16 ))
#define T_CURR_PREAMBLE_IDX			(0x6e   | ( 1 << 16 ))
#define T_PREV_PREAMBLE_IDX			(0x6f   | ( 1 << 16 ))
#define T_HO_SIGNAL_LATENCY			(0x70   | ( 2 << 16 ))
#define T_RSSI_2					(0x71   | ( 1 << 16 ))
#define T_CINR_2					(0x72   | ( 1 << 16 ))
#define T_POWER_CONTROL_MODE		(0x73   | ( 1 << 16 ))
#define T_PER_RECEIVE_COUNT			(0x74   | ( 4 << 16 ))
#define T_PER_ERROR_COUNT			(0x75   | ( 4 << 16 ))
#define T_UL_BURST_DATA_FEC_SCHEME	(0x76   | ( 1 << 16 ))
#define T_DL_BURST_DATA_FEC_SCHEME	(0x77   | ( 1 << 16 ))
#define T_UL_BURST_DATA_UIUC		(0x78   | ( 1 << 16 ))
#define T_DL_BURST_DATA_UIUC		(0x79   | ( 1 << 16 ))
#define T_TX_POWER_MAX				(0x7a   | ( 1 << 16 ))
#define T_HO_CNT					(0x7c   | ( 2 << 16 ))
#define T_HO_FAIL_CNT				(0x7d   | ( 2 << 16 ))
#define T_RESYNC_CNT				(0x7e   | ( 2 << 16 ))
#define T_CUR_FREQ					(0x7f	| ( 4 << 16 ))


/* WIMAX */
#define T_MAX_SUBSCRIPTION		(0xa1	| ( 1 << 16 ))
#define T_MAX_SF				(0xa2	| ( 1 << 16 ))
#define T_PHY_TYPE				(0xa3	| ( 1 << 16 ))
#define T_PKM					(0xa4	| ( 1 << 16 ))
#define T_AUTH_POLICY			(0xa5	| ( 1 << 16 ))
#define T_CS_TYPE				(0xa6	| ( 2 << 16 ))
#define T_VENDOR_NAME			(0xa7	| ( 0 << 16 ))
#define T_MOD_NAME				(0xa8	| ( 0 << 16 ))
#define T_PACKET_FILTER			(0xa9	| ( 1 << 16 ))
#define T_NSP_CHANGE_COUNT		(0xaa	| ( 4 << 16 ))
#define T_RADIO_STATE			(0xab	| ( 1 << 16 ))
#define T_URI_CONTACT_TYPE		(0xac	| ( 1 << 16 ))
#define T_URI_TEXT				(0xad	| ( 0 << 16 ))
#define T_URI					(0xae 	| ( 0 << 16 ))
#define T_ENABLE_AUTH			(0xaf	| ( 1 << 16 ))
#define T_TIMEOUT               (0xb0   | ( 2 << 16 ))
#define T_RUN_MODE				(0xb1	| ( 1 << 16 ))
			#define T_NORMAL_MODE			0
			#define T_ENGINEERING_MODE		1
#define T_OMADMT_VER			(0xb2	| ( 4 << 16 ))
#define T_RTC_TIME				(0xb3	| ( 4 << 16 ))	/*This is measured in seconds from 00:00:00 GMT January 1, 1970.*/
#define T_CERT_STATUS			(0xb4	| ( 4 << 16 ))
#define T_CERT_MASK				(0xb5	| ( 4 << 16 ))
#define T_EMSK					(0xb6	| ( 64 << 16 ))

/* Subscription TLV */
#define T_SUBSCRIPTION_LIST		(0xd1	| ( 0 << 16 ) | (1 << 31))
#define T_H_NSPID				(0xd2	| ( 3 << 16 ))
#define T_NSP_NAME				(0xd3	| ( 0 << 16 ))
#define T_SUBSCRIPTION_NAME		(0xd4	| ( 0 << 16 ))
#define T_SUBSCRIPTION_FLAG		(0xd5	| ( 2 << 16 ))
#define T_V_NSPID				(0xd6	| ( 3 << 16 ))
#define T_NAP_ID				(0xd7	| ( 3 << 16 ))
#define T_PREAMBLES				(0xd8	| ( 15 << 16 ))
#define T_BW					(0xd9	| ( 4 << 16 ))
#define T_FFTSIZE				(0xda	| ( 4 << 16 ))
#define T_DUPLEX_MODE			(0xdb	| ( 4 << 16 ))
#define T_REALM					(0xdc	| ( 0 << 16 ))
#define T_MO_REALM				(0xdd	| ( 0 << 16 ))

typedef struct hci {
	u16		cmd_evt;
	u16		length;
	u8		data[0];	
} __attribute__((packed)) hci_t;

typedef struct hci_image_payload {
	u16		type;
	u32		offset;
	u8		data[0];
} __attribute__((packed)) hci_image_payload_t;

typedef struct hci_image_response {
	u16		type;
	u32		offset;
	s32		result;
} __attribute__((packed)) hci_image_response_t;

typedef struct hci_file_write {
	u32		offset;
	u16		path_len;	/*Include null char*/
	u8		path[0];
	u8		data[0];
} __attribute__((packed)) hci_file_write_t;
#define hci_file_data(p)	(p->path+p->path_len)

typedef struct hci_file_delete {
	u8		path[0];
	u8		data[0];
} __attribute__((packed)) hci_file_delete_t;

typedef struct hci_file_read {
	u32		offset;
	u8	 	path[0];
	u8		data[0];
} __attribute__((packed)) hci_file_read_t;

typedef struct hci_file_response {
	s32		result;
	u16		path_len;
	u8		path[0];
	u8		data[0];
} __attribute__((packed)) hci_file_response_t;
#define file_response_data(r)	\
	(((hci_file_response_t*)(r))->path+B2H(((hci_file_response_t*)(r))->path_len))

typedef struct hci_notification {
	u8		category;
	u8		type;
	u8		value[0];
} __attribute__((packed)) hci_notification_t;

typedef struct hci_set_eap_info
{
	u8		type;
	u16		frag_size;
	u8		use_delimiter;
	u8		tlv[0];
} __attribute__((packed)) hci_set_eap_info_t;

typedef struct hci_set_eap_info_result
{
	u32 		code;
} __attribute__((packed)) hci_set_eap_info_result_t;

typedef struct hci_get_cert_info
{
	u8		index;
} __attribute__((packed)) hci_get_cert_info_t;

typedef struct hci_get_cert_info_result
{
	s32		result;
	u16		source;
	u8		level;
	u8		tlv[0];
} __attribute__((packed)) hci_get_cert_info_result_t;

typedef struct hci_delete_cert
{
	u8		index;
} __attribute__((packed)) hci_delete_cert_t;

typedef struct hci_delete_cert_result
{
	u32		 result;
} __attribute__((packed)) hci_delete_cert_result_t;

typedef struct hci_set_arm_capability_s
{
	u32		capability;
} __attribute__((packed)) hci_set_arm_capability_t;

typedef struct hci_set_arm_capability_result_s
{
	u32		capability;
} __attribute__((packed)) hci_set_arm_capability_result_t;

typedef struct hci_mode_change_s
{
	u8		status[3];
	u8		power_state;
} __attribute__((packed)) hci_mode_change_t;

typedef struct tlv_s {
	u8 T;
	u16 L;
	u8 *V;

} tlv_t;

#define HCI_INDICATION			(1<<0)

typedef struct hci_req_s {
	struct list_head list;
	u16 cmd_evt;
	
	void *buf;		/*this buffer is shared with data to check.*/
	int len;
	int chk_len;	/*length of data to check*/
	u32 flag;
	pthread_cond_t cond;

} hci_req_t;

typedef struct hci_msg_s {
	char buf[HCI_MAX_PACKET];
	int len;

} hci_msg_t;

int hci_get_tlv(u8 *buf, u8 *T, u16 *L, u8 **V);
int hci_set_tlv(u8 *buf, u8 T, u16 L, u8 *V);

int hci_send(int dev_idx, u16 cmd, u8 *data, int len);
int hci_send_wait(int dev_idx, u16 s_cmd, u8 *data, int plen, 
	u16 w_cmd, void *buf, int len, int chk_len, u32 flag, int timeout);
int hci_wait(int dev_idx, u16 cmd_evt,
	void *buf, int len, int chk_len, u32 flag, int timeout);
int hci_destroy_resp(int dev_idx);
int hci_req_getinfo(int dev_idx, u8 *buf, tlv_t *tlv, int cnt);
int hci_req_getinfo32(int dev_idx, u8 type, u32 *val);
int hci_req_setinfo32(int dev_idx, u8 type, u32 val);
int hci_send_msg(int dev_idx, hci_msg_t *msg);
int hci_receiver_create(msg_thr_t *hci_recvr);
int hci_receiver_delete(msg_thr_t *hci_recvr);

#endif /* __HCI_H__ */

