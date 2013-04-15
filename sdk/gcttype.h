// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(GCTTYPE_H_20100310)
#define GCTTYPE_H_20100310

#include "WiMaxType.h"

#if !defined(SDK_TYPEDEF)
#define SDK_TYPEDEF
#if !defined(__cplusplus)
typedef char                           bool;
#endif
typedef unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned int		u32;
typedef unsigned long long	u64;
typedef	signed char			s8;
typedef signed short		s16;
typedef int				s32;
typedef long long			s64;
#if 1
typedef wchar_t				uchar;
#else
typedef unsigned short		uchar;
#endif
#endif

#define GCT_API 
#define INVALID_GHANDLE		0x000000000

typedef enum _GCT_API_RET
{
	GCT_API_RET_FIRST_COMMON_ERROR = 0,
	/// No errors
	GCT_API_RET_SUCCESS,
	/// Fail to obtain results
	GCT_API_RET_FAILED,	
	/// Buffer size pass in parameter is too small.
	GCT_API_RET_BUFFER_SIZE_TOO_SMALL,	
	/// Invalid privileges to execute the command
	GCT_API_RET_PERMISSION_DENIED,	
	/// Device specified is invalid.
	GCT_API_RET_INVALID_DEVICE,	
	/// Invalid Parameter Passed
	GCT_API_RET_INVALID_PARAMETER,
	/// Cannot perform the command mobile connection.
	GCT_API_RET_ALREADY_CONNECTED,
	/// Link is not connected, cannot obtain result.
	GCT_API_RET_LINK_NOT_CONNECTED,
	/// Connection to the network is prohibited
	GCT_API_RET_NETWORK_PROHIBITED,
	/// Device specified is missing because of just being removed!
	GCT_API_RET_DEVICE_MISSING,
	/// Profile specified is invalid.
	GCT_API_RET_INVALID_PROFILE,	
	/// Connection to a roaming network is not allowed.
	GCT_API_RET_ROAMING_NOT_ALLOWED,	
	/// Connection is already in progress
	GCT_API_RET_CONNECTION_IN_PROGRESS,
	/// Function not implemented
	GCT_API_RET_NOT_IMPLEMENTED,
	/// SDK is busy
	GCT_API_RET_BUSY,

	/// last error code
	GCT_API_RET_LAST_COMMON_ERROR							
} GCT_API_RET, *PGCT_API_RET;

typedef void * APIHAND;

typedef struct GDEV_ID_S {
	APIHAND apiHandle;
	u8 deviceIndex;

} GDEV_ID, *GDEV_ID_P;

typedef enum _GCT_WIMAX_SDK_MODE
{
	GCT_WIMAX_SDK_EMBEDDED_EAP_ENABLED = (1<<0),
	GCT_WIMAX_SDK_OMA_DM_ENABLED = (1<<1),
	GCT_WIMAX_SDK_MODE_MAX

} GCT_WIMAX_SDK_MODE;

typedef enum _GCT_WIMAX_API_OPEN_MODE
{
	GCT_WIMAX_API_PRIVILEGE_READ_WRITE = (1<<0), /**< Read Write privilege access.(default) */
	GCT_WIMAX_API_PRIVILEGE_READ_ONLY = (1<<1),  /**< Read Only privilege access. */
	
	GCT_WIMAX_API_OPEN_MODE_NORMAL = (1<<2),
	GCT_WIMAX_API_OPEN_MODE_ENG = (1<<3),
	
	GCT_WIMAX_API_OPEN_MODE_MAX

} GCT_WIMAX_API_OPEN_MODE, *PGCT_WIMAX_API_OPEN_MODE;


typedef struct _GCT_WIMAX_API_PARAM {
	char nonvolatile_dir[256];
	char log_path[256];
	int log_level;

} GCT_WIMAX_API_PARAM;

typedef enum _GCT_API_POWER_MODE
{
	WiMAXPowerModeIdle,
	WiMAXPowerModeSleep,
	WiMAXPowerModeNormal,

	WiMAXPowerModeMaximum

} GCT_API_POWER_MODE, *GCT_API_POWER_MODE_P;

typedef struct _GCT_API_NEIGHBOR_LIST
{
	UINT32	structureSize;		/**< size of this structure. */
	UINT8	bsId[6];			/**< BS ID of the current serving BS as received from the network */
	UINT8	rssi;				/**< RSSI value */
	UINT8	cinr;				/**< CINR value */
	UINT8	preamble;			/** preamble value */
	//UINT16	roundTripTime;		/** round trip time from the base station measured in nanoseconds. */                 
	UINT32	frequency;

} GCT_API_NEIGHBOR_LIST, *GCT_API_NEIGHBOR_LIST_P;

typedef struct gct_harq_statistics_s
{
	UINT32 NumDlHarqRetry[8];
	UINT32 NumUlHarqRetry[8];

} gct_harq_statistics_t;

typedef struct gct_mcs_stats_s {
	UINT8 ModulationFEC;
	UINT8 RepetitionCode;
	UINT8 IUC;
	UINT8 Feature;
	UINT8 NumBurst;
	UINT8 NumBurstError;
	UINT8 LenPDU;
	UINT8 NumPDU;

} gct_mcs_stats_t;

typedef enum _GCT_API_EAP_TYPE
{
	GCT_WIMAX_NO_EAP = 0,
	GCT_WIMAX_EAP_TLS = 1,
	GCT_WIMAX_EAP_TTLS_MD5 = 2,
	GCT_WIMAX_EAP_TTLS_MSCHAPV2 = 3,
	GCT_WIMAX_EAP_TTLS_CHAP = 4,
	GCT_WIMAX_EAP_AKA = 5

} GCT_API_EAP_TYPE;
#define GCT_API_IS_EAP_TLS(t)	((t)>=GCT_WIMAX_EAP_TLS && (t)<=GCT_WIMAX_EAP_TTLS_CHAP)
#define GCT_API_IS_VALID_EAP(t)	((t)>=GCT_WIMAX_EAP_TLS && (t)<=GCT_WIMAX_EAP_AKA)

typedef struct _GCT_API_EAP_PARAM
{
#define WIMAX_EAP_STR_LEN		256
#define WIMAX_EAP_FILE_LEN		256
#define WIMAX_EAP_DECO_LEN		128	/*decoration*/
	GCT_API_EAP_TYPE	type;

	UINT16		fragSize;
	UINT8		useDelimiter;
	UINT8		devCertNULL;
	UINT8		caCertNULL;
	UINT8		disableResumption;
	UINT8		cr801Enable;
	UINT8		disableSessionTicket;

	UINT8		useNvramParam;
	UINT8		userId[WIMAX_EAP_STR_LEN];
	UINT8		userIdPwd[WIMAX_EAP_STR_LEN];
	UINT8		anonymousId[WIMAX_EAP_STR_LEN];
	UINT8		privateKeyPwd[WIMAX_EAP_STR_LEN];

	UINT8		decoration[WIMAX_EAP_DECO_LEN];

#define CACERT_NUM				10

	UINT8		logEnable;
} GCT_API_EAP_PARAM, *GCT_API_EAP_PARAM_P;

typedef struct _GCT_API_ODM_PARAM
{
	UINT8		useODM;

} GCT_API_ODM_PARAM, *GCT_API_ODM_PARAM_P;

typedef struct _GCT_API_CERT_INFO
{
#define WIMAX_CERT_STR_LEN		256
	UINT32		source;
	UINT8		cert_type[WIMAX_CERT_STR_LEN];
	UINT8		subject_cn[WIMAX_CERT_STR_LEN];
	UINT8		issuer_cn[WIMAX_CERT_STR_LEN];
	UINT8		expire_date[WIMAX_CERT_STR_LEN];

} GCT_API_CERT_INFO, *GCT_API_CERT_INFO_P;

typedef struct _GCT_API_RF_INFORM {
	INT32		nPERReceiveCount;
	INT32		nPERErrorCount;
	UINT8		bsId[6];
	UINT8		ULPermBase;
	UINT8		DLPermBase;
	UINT8		CurrentPreambleIndex;
	UINT8		PreviousPreambleIndex;
	UINT16		HandOverCount;
	UINT16		HandOverFailCount;
	UINT16		ResyncCount;	
	UINT16		HoSignalLatency;
	UINT8		CINR;
	UINT8		CINR2;
	UINT8		RSSI;
	UINT8		RSSI2;
	UINT16		PER;
	UINT8		PowerControlMode;
	UINT8		TxPower;
	UINT8		TxPowerMax;
	UINT8		ULBurstDataFECScheme;
	UINT8		DLBurstDataFECScheme;
	UINT8		ULBurstDataUIUC;
	UINT8		DLBurstDataDIUC;
	UINT32		Frequency;

} GCT_API_RF_INFORM, *GCT_API_RF_INFORM_P;

typedef enum _GCT_API_SCAN_TYPE {
	GCT_API_SCAN_WIDE,
	GCT_API_SCAN_ALL_SUBSCRIPTIONS,
	GCT_API_SCAN_CURR_SUBSCRIPTION

} GCT_API_SCAN_TYPE, *GCT_API_SCAN_TYPE_P;

typedef enum _GCT_API_NOTI_CATEGORY {
	GCT_API_ERROR_NOTI_EAP,
	GCT_API_NOTI_EAP,
	GCT_API_NOTI_ODM_NOTI,
	GCT_API_NOTI_ODM_ERROR

} GCT_API_NOTI_CATEGORY, *GCT_API_NOTI_CATEGORY_P;

typedef enum _GCT_API_ODM_NOTI_TYPE_P {
	GCT_API_ODM_NOTI_EXEC_REGISTRATION_PAGE_OPEN,
	GCT_API_ODM_NOTI_CHANGE_ACTIVATED,				
	GCT_API_ODM_NOTI_UNKNOWN_SERVER_REALM			// for CR1074

} GCT_API_ODM_NOTI_TYPE, *GCT_API_ODM_NOTI_TYPE_P;

typedef enum _GCT_API_NOTI_TYPE {
	GCT_API_NOTI_TYPE_CODE,
	GCT_API_NOTI_TYPE_TEXT	

} GCT_API_NOTI_TYPE, *GCT_API_NOTI_TYPE_P;

typedef enum _IMAGE_TYPE {
	IMG_KERNEL = 0,
	IMG_FS = 1,
	IMG_CFG = 2,
	IMG_CFG_RAM = 3,
	IMG_FS2 = 4,
	IMG_CLR_CFG = 5,
	IMG_EAP_PARAM = 6,
	IMG_BL_EEPROM = 7,
	IMG_BL_FLASH = 8,

	IMG_OMA_XML = 0x100,
	IMG_DEV_CERT = 0x101,
	IMG_CERT1_U = 0x102,
	IMG_CERT1_L = 0x103,
	IMG_CERT2_U = 0x104,
	IMG_CERT2_L = 0x105,
	IMG_CERT_BIG = 0x106,

	IMG_END
	
} IMAGE_TYPE;

typedef enum _CAPABILITY_BIT {
	CAPABILITY_ENC_XML		= (1<<0),
	CAPABILITY_E_EAP_TLS	= (1<<1),
	CAPABILITY_ODM			= (1<<2),
	CAPABILITY_E_EAP_AKA	= (1<<3),
	CAPABILITY_CAPL_INFO	= (1<<4),

	CAPABILITY_END

} CAPABILITY_BIT, *CAPABILITY_BIT_P;

#if defined(CONFIG_ENABLE_SERVICE_FLOW)
/*----------------------------------------------------------------------
 *
 * Service Flow Management
 *
 *----------------------------------------------------------------------*/

#include <netinet/in.h>

#define WIMAX_MAX_SERVICE_FLOW		16
#define WIMAX_MAX_RULES			16	/* MAX rules per SF */
#define MAX_SERVICE_CLASS_NAME	128


/* UL Grant Scheduling Type Parameter */
#define UL_SCHED_TYPE_RESERVED		0
#define UL_SCHED_TYPE_UNDEF		1
#define UL_SCHED_TYPE_BE		2
#define UL_SCHED_TYPE_nrtPS		3
#define UL_SCHED_TYPE_rtPS		4
#define UL_SCHED_TYPE_ertPS		5
#define UL_SCHED_TYPE_UGS		6


/* Type of Data Delivery Services parameter */
#define DATA_SERVICE_UGS		0
#define DATA_SERVICE_RT_VR		1
#define DATA_SERVICE_NRT_VR		2
#define DATA_SERVICE_BE			3
#define DATA_SERVICE_ERT_VR		4

#define MAX_PHSS	128

struct wimax_sf_event_param {
	uint8_t		cc;		/* confirm code */
	uint8_t		ms_initiated;	/* MS initiated DSx */
	uint32_t	sfid; 		/* SFID SF event took place on */
	int8_t		clfr_action;	/* classification rule DSC action */
	uint16_t	PacketClassificationRuleIndex; /* changed classfication rule index */
	int8_t		phs_action;	/* phs rule changed DSC action */
	uint16_t	PHSI;		/* changed PHSI */
};

typedef enum _WIMAX_SF_EVENT_TYPE {
	/* Service flow-related events */
	WIMAX_EVT_SERVICE_FLOW_ADDED,
	WIMAX_EVT_SERVICE_FLOW_CHANGED,	
	WIMAX_EVT_SERVICE_FLOW_DELETED,
	WIMAX_EVT_CID_UPDATE,
} WIMAX_SF_EVENT_TYPE;

typedef struct _wimax_sf_event {
	WIMAX_SF_EVENT_TYPE	code;
	struct wimax_sf_event_param sf;
} __attribute__((packed)) WIMAX_SF_EVENT, *WIMAX_SF_EVENT_P;

typedef enum {
	WIMAX_SF_SUCCESS = 0,
	WIMAX_SF_OTHER,
	WIMAX_SF_UNRECOGNIZED_CONF_SETTING,
	WIMAX_SF_TEMPORARY,		// reject-resource
	WIMAX_SF_PERMANANT,		// reject-admin
	WIMAX_SF_NOT_OWNER,
	WIMAX_SF_NOT_FOUND,
	WIMAX_SF_EXISTS,
	WIMAX_SF_REQIRED_PARAM_NOT_PRESENT,
	WIMAX_SF_HEADER_SUPRESSION,
	WIMAX_SF_UNKNOWN_TRANSACTION_ID,
	WIMAX_SF_AUTH_FAILURE,
	WIMAX_SF_ADD_ABORTED,
	WIMAX_SF_EXCEEDED_DYNAMIC_SERVICE_LIMIT,
	WIMAX_SF_NOT_AUTHORIZED_FOR_THE_REQUESTED_SAID,
	WIMAX_SF_FAIL_TO_ESTABLISH_THE_REQUESTED_SA,
	WIMAX_SF_NOT_SUPPORTED_PARAMETER,
	WIMAX_SF_NOT_SUPPORTED_PARAMETER_VALUE
} WIMAX_SF_CC;


typedef enum {
	DSC_ADD_CLASSIFIER = 0,
	DSC_REPLACE_CLASSIFIER,
	DSC_DELETE_CLASSIFIER,
	DSC_NOP_CLASSIFIER,
} WIMAX_CLFR_DSC_ACTION;


typedef enum {
	DSC_ADD_PHS = 0,
	DSC_SET_PHS,		//TODO not tested
	DSC_DELETE_PHS,
	DSC_DELETE_ALL_PHS,	//TODO not supported
	DSC_NOP_PHS,
} WIMAX_PHS_DSC_ACTION;


struct ipv4_addr {
	struct in_addr	address;
	struct in_addr  mask;
} __attribute__((packed));


struct ipv6_addr {
	struct in6_addr address;
	struct in6_addr mask;
} __attribute__((packed));

typedef struct wimax_classification_rule {
	uint32_t	valid;

	union {
		uint16_t all;
		struct {
			uint16_t reserived:1;
			uint16_t IPTypeOfService:1;
			uint16_t Protocol:1;
			uint16_t IPMaskedSrcAddress:1;
			uint16_t IPMaskedDstAddress:1;
			uint16_t ProtocolSrcPortRange:1;
			uint16_t ProtocolDstPortRange:1;
			uint16_t DstMacAddr:1;
			uint16_t SrcMacAddr:1;
			uint16_t Ethertype:1;
			uint16_t IEEE802_1DUserPriority:1;
			uint16_t IEEE802_1QVLANID:1;
			uint16_t Reserved:5;
		} fields;
	} mask;    

	/* Packet Classification Rule */
	uint8_t 	ClassifierRulePriority;
	struct {
		uint8_t low;
		uint8_t high;
		uint8_t mask;
	} __attribute__((packed)) IPTypeOfService;
	
	uint8_t	Protocol;
	
	struct ipv4_addr 	IPv4MaskedSourceAddress;
	struct ipv6_addr	IPv6MaskedSourceAddress;
	struct ipv4_addr	IPv4MaskedDestAddress;
	struct ipv6_addr	IPv6MaskedDestAddress;	

	struct {
		uint16_t low;
		uint16_t high;
	} __attribute__((packed)) ProtocolSourcePort, ProtocolDestPort;

	struct {
		uint8_t	addr[6];
		uint8_t	mask[6];
	} __attribute__((packed)) EthernetDestMACAddress, EthernetSourceMACAddress;

	struct {
		uint8_t	type;
		uint8_t	eprot1;
		uint8_t	eprot2;
	} __attribute__((packed)) EtherType;

	struct {
		uint8_t	low;
		uint8_t high;
	} __attribute__((packed)) IEEE802_1D_UserPriority;

	uint16_t IEEE802_1Q_VLANID;

	uint8_t 	AssociatedPHSI;
	uint16_t 	PacketClassifierRuleIndex;
	uint8_t		IPv6FlowLabel[3];
	uint8_t		ContextID;
} WIMAX_CLFR_RULE, *WIMAX_CLFR_RULE_P;


typedef struct wimax_phs_rule {
	uint32_t	valid;
	struct wimax_phs_rule *next;	/* not used */
	uint8_t		state;
	uint8_t		PHSI;
	uint8_t		PHSF[MAX_PHSS];
	uint8_t		PHSM[(MAX_PHSS + 7) >> 3];
	uint8_t		PHSS;
	uint8_t		PHSV;
} WIMAX_PHS_RULE, *WIMAX_PHS_RULE_P;


typedef struct wimax_sf_param {
	uint8_t		DL;
	/* TLV - 11.13 */
	uint32_t	SFID;
	uint16_t	CID;
	uint8_t		ServiceClassName[MAX_SERVICE_CLASS_NAME];
	uint8_t		MBSService;
	
	uint8_t		QosParamSetType; /* Admitted, Active, Provisioned */
	uint8_t		TrafficPriority; 		/* 6 */
	uint32_t	MaxSustainedTrafficRate;
	uint32_t	MaxTrafficBurst;
	uint32_t	MinReservedTrafficRate;
	uint32_t	MinTolerableRate;		/* 10 */
	uint8_t		ULGrantSchedulingType;		/* 11 */
	uint8_t		RequestTransmissionPolicy;	/* 12 */
	uint32_t	ToleratedJitter;		/* 13 */
	uint32_t	MaxLatency;			/* 14 */
	
	uint8_t		FixedLengthSDUIndicator;	/* 15 */
	uint8_t		SDUSize;			/* 16 */
	uint16_t	TargetSAID;			/* 17 */
	
	uint8_t		ARQEnable;			/* 18 */
	uint16_t	ARQWindowSize;			/* 19 */
	uint16_t	ARQTransmitterDelay;		/* 20 */
	uint16_t	ARQReceiverDelay;		/* 21 */
	uint16_t	ARQBlockLifeTime;		/* 22 */
	uint16_t	ARQSyncLossTimeout;		/* 23 */
	uint8_t		ARQDeliverInOrder;		/* 24 */
	uint16_t	ARQRxPurgeTimeout;		/* 25 */
	uint16_t	ARQBlockSize;			/* 26 */
	uint8_t		ReceiverARQAckProcessingTime;	/* 27 */
	
	uint8_t		CSSpecification;		/* 28 */

	uint8_t 	TypeOfDataDeliveryServices;	// 29
	uint16_t 	SDUInterArrivalInterval;	// 30
	uint16_t 	TimeBase;			// 31
	uint8_t 	PagingPreference;		// 32
	uint8_t 	MBSZoneID[8];			// 33
	uint8_t 	TrafficPreferenceIndication;	// 34
	uint8_t 	GlobalServiceClassName[6];	// 35
	uint8_t 	SNFeedbackEnabled;		// 37
	uint8_t 	FSNSize;			// 38
	uint8_t 	Num_CIDAllocForActiveBSs;
	uint16_t 	CIDAllocForActiveBSs[8];	// 39 ??
	uint16_t 	UnsolicitedGrantInterval;	// 40
	uint16_t 	UnsolicitedPollingInterval;	// 41
	uint8_t 	PDUSNExtendedSubheaderForHARQReordering;	// 42
	uint8_t 	Num_MBSContentsIDs;		// 43
	uint16_t 	MBSContentsIDs[8];		// 43
	uint8_t 	HARQServiceFlows;		// 44
	uint8_t		AuthorizationToken;		// 45
	uint8_t 	Num_HARQChannelMapping;		// 46
	uint8_t 	HARQChannelMapping[16];		// 46
} WIMAX_SF_PARAM, *WIMAX_SF_PARAM_P;

typedef union wimax_sf_stats {
    struct {
	uint32_t	sdu_packets;
	uint32_t	sdu_packets_dropped;
	uint32_t	sdu_bytes;
	uint32_t	sdu_bytes_dropped;
	uint32_t	pdu_packets;
	uint32_t	pdu_bytes;
	
	uint32_t	bw_req;
	uint32_t	gmsh;
	
	uint32_t	arq_blocks;
	uint32_t	arq_blocks_retry;
	uint16_t	arq_discard;
	uint16_t	arq_reset;
	uint16_t	arq_sync_loss;		
    } ul;
    struct {
	uint32_t	sdu_packets;
	uint32_t	sdu_packets_dropped;
	uint32_t	sdu_bytes;
	uint32_t	sdu_bytes_dropped;
	uint32_t	pdu_packets;
	uint32_t	pdu_bytes;
	
	uint32_t	arq_blocks;
	uint32_t	arq_blocks_retry;
	uint16_t	arq_discard;
	uint16_t	arq_reset;
	uint16_t	arq_sync_loss;	
    } dl;
} WIMAX_SF_STATS, *WIMAX_SF_STATS_P;

typedef struct wimax_service_flow {
	uint8_t			valid;
	struct wimax_sf_param 	param;
	struct wimax_classification_rule classification_rule[WIMAX_MAX_RULES];
	struct wimax_phs_rule	phs_rule[WIMAX_MAX_RULES];
	union wimax_sf_stats	stats;
} WIMAX_SERVICE_FLOW, *WIMAX_SERVICE_FLOW_P;


extern const struct wimax_sf_param sf_param_init;
extern const struct wimax_classification_rule classifier_rule_init;
extern const struct wimax_phs_rule phs_rule_init;

#endif // CONFIG_ENABLE_SERVICE_FLOW

typedef enum _GCT_API_CMD_MAC_STATE_TYPE {
	GCT_API_CMD_MAC_STATE_ENTER_SLEEP_MODE,
	GCT_API_CMD_MAC_STATE_EXIT_SLEEP_MODE,
	GCT_API_CMD_MAC_STATE_ENTER_IDLE_MODE,
	GCT_API_CMD_MAC_STATE_EXIT_IDLE_MODE
} GCT_API_CMD_MAC_STATE_TYPE, *GCT_API_CMD_MAC_STATE_TYPE_P;

typedef enum _FEC_MOD {
	OFDMA_FEC_MOD_QPSK_CC_H				= 0,
	OFDMA_FEC_MOD_QPSK_CC_3Q			= 1,
	OFDMA_FEC_MOD_QAM16_CC_H			= 2,
	OFDMA_FEC_MOD_QAM16_CC_3Q			= 3,
	OFDMA_FEC_MOD_QAM64_CC_H			= 4,
	OFDMA_FEC_MOD_QAM64_CC_2O3			= 5,
	OFDMA_FEC_MOD_QAM64_CC_3Q			= 6,
	OFDMA_FEC_MOD_QPSK_BTC_H			= 7,
	OFDMA_FEC_MOD_QPSK_BTC_3Q			= 8,
	OFDMA_FEC_MOD_QAM16_BTC_3O5			= 9,
	OFDMA_FEC_MOD_QAM16_BTC_4O5			= 10,
	OFDMA_FEC_MOD_QAM64_BTC_5O8			= 11,
	OFDMA_FEC_MOD_QAM64_BTC_4O5			= 12,
	OFDMA_FEC_MOD_QPSK_CTC_H			= 13,
	OFDMA_FEC_MOD_RESERVED14			= 14,
	OFDMA_FEC_MOD_QPSK_CTC_3Q			= 15,
	OFDMA_FEC_MOD_QAM16_CTC_H			= 16,
	OFDMA_FEC_MOD_QAM16_CTC_3Q			= 17,
	OFDMA_FEC_MOD_QAM64_CTC_H			= 18,
	OFDMA_FEC_MOD_QAM64_CTC_2O3			= 19,
	OFDMA_FEC_MOD_QAM64_CTC_3Q			= 20,
	OFDMA_FEC_MOD_QAM64_CTC_5O6			= 21,
	OFDMA_FEC_MOD_QPSK_ZTCC_H			= 22,
	OFDMA_FEC_MOD_QPSK_ZTCC_3Q			= 23,
	OFDMA_FEC_MOD_QAM16_ZTCC_H			= 24,
	OFDMA_FEC_MOD_QAM16_ZTCC_3Q			= 25,
	OFDMA_FEC_MOD_QAM64_ZTCC_H			= 26,
	OFDMA_FEC_MOD_QAM64_ZTCC_2O3		= 27,
	OFDMA_FEC_MOD_QAM64_ZTCC_3Q			= 28,
	OFDMA_FEC_MOD_QPSK_SDPC_H			= 29,
	OFDMA_FEC_MOD_QPSK_SDPC_2O3_A_CODE	= 30,
	OFDMA_FEC_MOD_QPSK_LDPC_3Q_A_CODE	= 31,
	OFDMA_FEC_MOD_QAM16_LDPC_H			= 32,
	OFDMA_FEC_MOD_QAM16_LDPC_2O3_A_CODE	= 33,
	OFDMA_FEC_MOD_QAM16_LDPC_3Q_A_CODE	= 34,
	OFDMA_FEC_MOD_QAM64_LDPC_H			= 35,
	OFDMA_FEC_MOD_QAM64_LDPC_2O3_A_CODE	= 36,
	OFDMA_FEC_MOD_QAM64_LDPC_3Q_A_CODE	= 37,
	OFDMA_FEC_MOD_QPSK_LDPC_2O3_B_CODE	= 38,
	OFDMA_FEC_MOD_QPSK_LDPC_3Q_B_CODE	= 39,
	OFDMA_FEC_MOD_QAM16_LDPC_2O3_B_CODE	= 40,
	OFDMA_FEC_MOD_QAM16_LDPC_3Q_B_CODE	= 41,
	OFDMA_FEC_MOD_QAM64_LDPC_2O3_B_CODE	= 42,
	OFDMA_FEC_MOD_QAM64_LDPC_3Q_B_CODE	= 43,
	OFDMA_FEC_MOD_QPSK_CC_OI_H			= 44,
	OFDMA_FEC_MOD_QPSK_CC_OI_3Q			= 45,
	OFDMA_FEC_MOD_QAM16_CC_OI_H			= 46,
	OFDMA_FEC_MOD_QAM16_CC_OI_3Q		= 47,
	OFDMA_FEC_MOD_QAM64_CC_OI_2O3		= 48,
	OFDMA_FEC_MOD_QAM64_CC_OI_3Q		= 49,
	OFDMA_FEC_MOD_QPSK_LDPC_5O6			= 50,
	OFDMA_FEC_MOD_QAM16_LDPC_5O6		= 51,
	OFDMA_FEC_MOD_QAM64_LDPC_5O6		= 52,
	OFDMA_FEC_MODE_CNT,
} FEC_MOD;

typedef enum _REPETITION_CODE
{
	REPETITION_CODING_NOT_SPECIFIED		= 0,
	NO_REPETITION_CODING				= 1,
	REPETITION_CODING_OF_2_USED			= 2,
	REPETITION_CODING_OF_4_USED			= 3,
	REPETITION_CODING_OF_6_USED			= 4,
	REPETITION_CODING_CNT,
} REPETITION_CODE;

typedef enum _MIMO_TYPE
{
	MINO_TYPE_NOT_SPEC					= 0,
	MINO_TYPE2_ANT_STC_MATRIX_A			= 1,
	MINO_TYPE2_ANT_STC_MATRIX_B_VC		= 2,
	/*
	MINO_TYPE2_ANT_STC_MATRIX_B_HC		= 3,
	MINO_TYPE4_ANT_STC_MATRIX_A			= 4,
	MINO_TYPE4_ANT_STC_MATRIX_B_VC		= 5,
	MINO_TYPE4_ANT_STC_MATRIX_B_HC		= 6,
	MINO_TYPE4_ANT_STC_MATRIX_C_VC		= 7,
	MINO_TYPE4_ANT_STC_MATRIX_C_HC		= 8,
	MINO_TYPE3_ANT_STC_MATRIX_A			= 9,
	MINO_TYPE3_ANT_STC_MATRIX_B			= 10,
	MINO_TYPE3_ANT_STC_MATRIX_C_VC		= 11,
	MINO_TYPE3_ANT_STC_MATRIX_C_HC		= 12,
	MIMO_NOT_USD						= 13,
	*/
	MIMO_TYPE_CNT,
} MIMO_TYPE;

typedef struct _GCT_API_MAC_PHY_MAC_BASIC {
	u32	frame_number;
	u32	fch;
	u16	ttg;
	u16	rtg;
	u8	num_dl_symbol;
	u8	num_ul_symbol;
	u8	current_pi;	/*PreambleIndex*/
	u8	previous_pi;	/*PreambleIndex*/
	u8	ul_perm_base;
	u8	mac_state;
	u8	bsid[6];
	s32	ul_time;
	u32	frequency;
	u16	bandwidth;
	u16	pad;	// padding
	u32	time_active;
	u32	time_sleep;
	u32	time_idle;
	u16	basic_cid;
	u16	primary_cid;
} GCT_API_MAC_PHY_MAC_BASIC, *GCT_API_MAC_PHY_MAC_BASIC_P;

typedef struct _GCT_API_MAC_PHY_MCS {
struct struct_mac_phy_mcs_s {
		u32 num_burst;
		u32 num_burst_error;
		u32 len_pdu;
		u32 num_pdu;
	} dl[OFDMA_FEC_MODE_CNT][REPETITION_CODING_CNT][MIMO_TYPE_CNT], ul[OFDMA_FEC_MODE_CNT][REPETITION_CODING_CNT][MIMO_TYPE_CNT];

	bool dl_used[OFDMA_FEC_MODE_CNT][REPETITION_CODING_CNT][MIMO_TYPE_CNT];
	bool ul_used[OFDMA_FEC_MODE_CNT][REPETITION_CODING_CNT][MIMO_TYPE_CNT];
} GCT_API_MAC_PHY_MCS, *GCT_API_MAC_PHY_MCS_P;

typedef struct _GCT_API_MAC_PHY_CINR_RSSI {
	s8	cinr_mean;
	s8	cinr_std_dev;
	s8	rssi_mean;
	s8	rssi_std_dev;
	s8	cinr_a_mean;
	s8	cinr_b_mean;
	s8	cinr_main;
	s8	cinr_diversity;
	s8	rssi_main;
	s8	rssi_diversity;
	s8	preamble_cinr_reuse3;
	s8	preamble_cinr_reuse1;
} GCT_API_MAC_PHY_CINR_RSSI, *GCT_API_MAC_PHY_CINR_RSSI_P;

typedef enum _FUMO_RESULT {
	FUMO_SUCCESSFUL = 200, /*The Request has Succeeded*/
	FUMO_SUCCESSFUL_VENDOR_SPEC = 250, /*250 -299 Vendor Specified Successful Operation
										with Vendor Specified ResultCode*/
	FUMO_MNGMT_CLIENT_ERR = 400, /*Management Client error .
									based on User or Device behavior*/
	FUMO_USER_CANCELLED = 401, /*User chose not to accept the operation when prompted*/
	FUMO_CORRUPTED_FW_UPDATE = 402, /*Corrupted firmware update package,
									did not store correctly.*/
	FUMO_DEVICE_MISMATCH = 403, /*Firmware Update Package. Device Mismatch*/
	FUMO_PKG_VALIDATION_ERR = 404, /*Package Validation Failure to positively validate
									digital signature of firmware update package*/
	FUMO_PKG_NOT_ACCEPTABLE_FW = 405, /*Firmware Update Package is Not Acceptable*/
	FUMO_ALTERNATE_DL_AUTH_ERR = 406, /*Authentication was Required but Authentication
						Failure was encountered when downloading Firmware Update Package*/
	FUMO_ALTERNATE_DL_REQ_TIMEOUT = 407, /*Client has encountered a time-out
										when downloading Firmware Update Package*/
	FUMO_NOT_IMPLEMENTED = 408, /*The device does not support the requested operation.*/
	FUMO_UNDEFINED_ERR = 409, /*Indicates failure not defined by any other error code*/
	FUMO_FW_UPDATE_FAILED = 410, /*Firmware Update operation failed in device*/
	FUMO_MALFORMED_OR_BAD_URL = 411, /*The URL provided for alternate download is bad*/
	FUMO_ALT_DL_SERV_UNAVAILABLE = 412, /*The Alternate Download Server is 
										Unavailable or Does not Respond*/
	FUMO_CLIENT_ERR = 450, /*450-499: Client Error encountered for Operation with 
							Vendor Specified ResultCode*/
	FUMO_ALT_DL_SERV_ERR = 500, /*Alternate Download Server Error Encountered*/
	FUMO_DL_OUT_OF_MEM = 501, /*Download fails due to device is out of memory
								The download fails due insufficient memory in the device
								to save the firmware update package.*/
	FUMO_UPDATE_OUT_OF_MEM = 502, /*Firmware update fails due to device out of memory
				The update fails because there isn¡¯t sufficient memory to update the device.*/
	FUMO_DL_NETWORK_ERR = 503, /*Download fails due to network issues
								The download fails due to network/transport level errors*/
	FUMO_ALT_DL_SERV_ERR_VENDOR_SPEC = 550, /*550-599: Alternate Download Server Error.
								Vendor Specified
								Alternate Download Server Error encountered for Operation
								with Vendor Specified ResultCode*/

	FUMO_RESULT_END
} FUMO_RESULT;

#define GAPI_LOG_LEVEL_IS_FILE(level)		((level)>4)
#define GAPI_LOG_FLUSH_LEVEL		10

typedef void (*GIndRcvHCIPacket) (GDEV_ID_P pID, char *szBuf, int nBufSize);
typedef void (*GIndPowerModeChange) (GDEV_ID_P pID, GCT_API_POWER_MODE nPowerMode);
typedef void (*GIndDeviceStatusUpdate)(GDEV_ID_P pID,
				WIMAX_API_DEVICE_STATUS deviceStatus,
				WIMAX_API_STATUS_REASON statusReason,
				WIMAX_API_CONNECTION_PROGRESS_INFO connectionProgressInfo);
typedef void (*GIndDeviceInsertRemove)(GDEV_ID_P pID, BOOL cardPresence);
typedef void (*GIndControlPowerManagement)(GDEV_ID_P pID, WIMAX_API_RF_STATE powerState);
typedef void (*GIndConnectToNetwork) (GDEV_ID_P pID,
				WIMAX_API_NETWORK_CONNECTION_RESP networkConnectionResponse);
typedef void (*GIndDisconnectFromNetwork) (GDEV_ID_P pID,
				WIMAX_API_NETWORK_CONNECTION_RESP networkDisconnectResponse);
typedef void (*GIndNetworkSearchWideScan) (GDEV_ID_P pID, WIMAX_API_NSP_INFO_P pNspList,
				UINT32 listSize);
typedef void (*GIndProvisioningOperation) (GDEV_ID_P pID, 
				WIMAX_API_PROV_OPERATION provisioningOperation,
				WIMAX_API_CONTACT_TYPE contactType);
typedef void (*GIndPackageUpdate) (GDEV_ID_P pID, WIMAX_API_PACK_UPDATE packageUpdate);
typedef void (*GIndNotification) (GDEV_ID_P pID, GCT_API_NOTI_CATEGORY nCategory,
				GCT_API_NOTI_TYPE nType, int nBufSize, char *szBuf);

#if defined(CONFIG_ENABLE_SERVICE_FLOW)
typedef void (*GIndNotiServiceFlow)(GDEV_ID_P pID, WIMAX_SF_EVENT_P pSfEvent);
#endif // CONFIG_ENABLE_SERVICE_FLOW
#endif
