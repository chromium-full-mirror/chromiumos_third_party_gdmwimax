// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * sf.c - WIMAX service flow manipulation
 */


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ctype.h>

#include <sys/sem.h>
#include <sys/shm.h>
#include "gcttype.h"
#include "device.h"
#include "sdk.h"
#include "sf.h"

#define DSX_REQUEST_TIMEOUT	10

/*----------------------------------------------------------------------
 *
 * Common to SDK server and client
 *
 *----------------------------------------------------------------------*/
const WIMAX_CLFR_RULE classifier_rule_init = {
	.IPTypeOfService.low		= 1,
	.Protocol			= 255,
	.IPv4MaskedSourceAddress = {
		.address.s_addr 	= 0xffffffffUL,
	},
	.IPv4MaskedDestAddress = {
		.address.s_addr 	= 0xffffffffUL,
	},
	.IPv6MaskedSourceAddress = {
		.address 	= IN6ADDR_LOOPBACK_INIT,
	},
	.IPv6MaskedDestAddress = {
		.address 	= IN6ADDR_LOOPBACK_INIT,
	},
	.ProtocolSourcePort.low		= 1,
	.ProtocolDestPort.low		= 1,
	.EthernetDestMACAddress = {
		.addr 			= {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	},
	.EthernetSourceMACAddress = {
		.addr 			= {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	},
	.EtherType.type 		= 4,
	.IEEE802_1D_UserPriority.low	= 8,
	.IEEE802_1Q_VLANID 		= 0xfff,
	.AssociatedPHSI			= 0,
};


const WIMAX_PHS_RULE phs_rule_init = {
	.PHSI	= 0,
	.PHSS	= 0,
	.PHSV 	= 0,
};

/*
 * TLV related routines
 */

/*
 * ptr - points where L is expected.
 * ll - actual length of this TLV.
 * returns a pointer to the location where V starts.
 */ 
static uint8_t *sf_hci_calc_length(uint8_t *ptr, uint32_t *l)
{
	uint8_t ll = *ptr;	/* length of length */
	int i;
	
	if (ll < 0x80) {
		*l = (uint32_t) ll;
		return ptr+1;	/* V starts right after ptr */
	}
	
	assert(ll != 0x80);
	ll &= ~0x80;

	/* Here, l is the length (in byte) of L */
	*l = 0;
	for (i = 1; i <= ll; i++) {
		*l = (*l << 8) + ptr[i];
	}
	/* assert(ll == 2); */
	/*(*l) = ntohs(*l);*/

	return ptr + ll + 1; /* V starts ll bytes after ptr */
}

/* Find a TLV within [start, end)
 *
 * 
 * return 1 if found, 0, otherwise.
 */

static int32_t sf_hci_find_tlv_range(struct hci *pkt, uint8_t T, uint32_t *L, uint8_t **V,
			    uint32_t start, uint32_t end)
{
	uint8_t *ptr = pkt->data + start;
	
	while (ptr < pkt->data + end) {
		if (*ptr == T) {
			*V = sf_hci_calc_length(ptr + 1, L);
			return 1;
		}
		ptr = sf_hci_calc_length(ptr + 1, L);

		/* ptr points to the start of V */
		ptr += *L; /* T: 1 byte, L: 1 byte: V: L byte */
	}
	
	return 0;
}

/*
 * hci_get_tlv() obtains T, L, and V of a TLV variable at
 * the offset @oft.
 *
 * It is up to the caller to provide a valid @oft. Otherwise,
 * this function won't work correctly.
 */
static uint32_t sf_hci_get_tlv(struct hci *pkt, uint8_t *T, uint32_t *L, uint8_t **V, uint32_t oft)
{
	uint8_t *ptr = pkt->data + oft;
	
	*T = *ptr++;
	*V = ptr = sf_hci_calc_length(ptr, L);

	return ptr + *L - pkt->data;
}

/*
 * Currently, l < 0x10000 is only supported.
 *
 * In addition, this routine blindly sets LL to be 2 regardless of
 * actual value of l.
 *
 */
static uint8_t *sf_hci_put_length(uint8_t *ptr, uint32_t l)
{
	if (l < 0x80) {
		*ptr = (uint8_t)(l & 0xff);
		return ptr+1;
	}

	assert(l < 0x10000);
	*ptr++ = 0x82;
	*ptr++ = (l >> 8) & 0xff;
	*ptr++ = (l & 0xff);

	return ptr;
}

/*
 * sf_hci_append_tlv() - appends given (T, L, V) to the current hci packet pkt.
 *
 * pkt->length is increased appropriately.
 * The function returns the last offset of hci packet after appending the given
 * (T, L, V).
 */
static void sf_hci_append_tlv(struct hci *pkt, uint8_t T, uint32_t L, void *V)
{
	unsigned oft = pkt->length;
	unsigned char *ptr = pkt->data + oft;
	
	*ptr++ = T;
	
	ptr = sf_hci_put_length(ptr, L);
	memcpy(ptr, V, L);

	pkt->length = (ptr + L) - pkt->data;

	return;
}

/*
 * hci->cmd_evt: host byte order
 * hci->length: host byte order
 */
static int sf_send_hci(int dev_idx, struct hci *hci)
{
	return hci_send(dev_idx, WIMAX_DSX_REQUEST, hci->data, hci->length);
}

static inline unsigned get_unaligned_u32(void *ptr)
{
	unsigned char *p = (unsigned char *)ptr;
	
	return ntohl(((p[0] << 24)|(p[1] << 16)|(p[2] << 8)|p[3]));
}

static inline unsigned get_unaligned_u16(void *ptr)
{
	unsigned char *p = (unsigned char *)ptr;
	
	return ntohs((u_short)((p[0] << 8) | p[1]));
}

static inline void put_unaligned_u32(void *ptr, unsigned le_val)
{
	unsigned char *p = (unsigned char *)ptr;
	le_val = htonl(le_val);
	
	*p++ = (le_val >> 24) & 0xff;
	*p++ = (le_val >> 16) & 0xff;
	*p++ = (le_val >> 8) & 0xff;
	*p++ = le_val & 0xff;
}

static inline void put_unaligned_u16(void *ptr, unsigned le_val)
{
	unsigned char *p = (unsigned char *)ptr;
	
	le_val = htons((u_short)le_val);
	
	*p++ = (le_val >> 8) & 0xff;
	*p++ = le_val & 0xff;	
}

/*
 * find_service_flow() attempts to find the service flow
 * that matches the given @sfid.
 */
struct wimax_service_flow *find_service_flow(int dev_idx,
					     uint32_t sfid)
{
	device_t *dev;
	struct wimax_service_flow *sf;
	int i;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return NULL;
	
	for (i = 0; i < WIMAX_MAX_SERVICE_FLOW; i++) {
		sf = dev->wimax->dev_info.service_flow.sf + i;
		if (sf->valid && sf->param.SFID == sfid)
			goto out;
	}
	sf = NULL;

out:
	dm_put_dev(dev_idx);
	xfunc_out();
	return sf;
}


struct wimax_classification_rule *
find_classification_rule(struct wimax_service_flow *sf, uint16_t index)
{
	struct wimax_classification_rule *rule;
	int i;

	rule = sf->classification_rule;
	for (i = 0; i < WIMAX_MAX_RULES; i++, rule++) {
		if (rule->valid && rule->PacketClassifierRuleIndex == index) {
			return rule;
		}
	}
	return NULL;
}

static struct wimax_classification_rule *
next_classification_rule(struct wimax_service_flow *sf,
			 struct wimax_classification_rule *start)
{
	struct wimax_classification_rule *rule;

	if (start == NULL)
		rule = sf->classification_rule;
	else
		rule = start + 1;

	for (; rule - sf->classification_rule < WIMAX_MAX_RULES; rule++)
		if (rule->valid)
			return rule;

	return NULL;
}



struct wimax_phs_rule *find_phs_rule(struct wimax_service_flow *sf,
				     uint16_t index)
{
	struct wimax_phs_rule *rule;
	int i;

	rule = sf->phs_rule;
	for (i = 0; i < WIMAX_MAX_RULES; i++, rule++) {
		if (rule->valid && rule->PHSI == index) {
			return rule;
		}
	}
	return NULL;
}

static struct wimax_phs_rule *
next_phs_rule(struct wimax_service_flow *sf,
	      struct wimax_phs_rule *start)
{
	struct wimax_phs_rule *rule;

	if (start == NULL)
		rule = sf->phs_rule;
	else
		rule = start + 1;

	for (; rule - sf->phs_rule < WIMAX_MAX_RULES; rule++)
		if (rule->valid)
			return rule;

	return NULL;
}


static char *schedstr[] = {"Reserved", "Undefined", "BE", "nrtPS", "rtPS", "ertPS", "UGS"};
static char *ddsstr[] = {"UGS", "RT-VR", "NRT-VR", "BE", "ERT-VR"};
static char *csstr[] = {
	"Reserved",
	"Packet, IPv4",
	"Packet, IPv6",
	"Packet, IEEE802.3/Ethernet",
	"Packet, IEEE 802 1Q VLAN",
	"Packet, IPv4 over IEEE 802.3/Ethernet",
	"Packet, IPv6 over IEEE 802.3/Ethernet",
	"Packet, IPv4 over IEEE 802 1Q VLAN",
	"Packet, IPv6 over IEEE 802 1Q VLAN",
	"ATM",
};

#define print_field(obj, x, fmt)	snprintf(str+oft, (size > oft)? size-oft: 0, "\t" #x "=" fmt "\n", obj->x)
#define nnsize	(size > oft)? (size-oft): 0

int sprintf_sf_param(char *str, unsigned int size, struct wimax_sf_param *sfp)
{
	int oft = 0;
	
	oft += print_field(sfp, DL, "%d");
	oft += print_field(sfp, SFID, "%08x");
	oft += print_field(sfp, CID, "%04x");
	oft += print_field(sfp, ServiceClassName, "'%s'");
	oft += print_field(sfp, QosParamSetType, "%d");
	oft += print_field(sfp, TrafficPriority, "%d");
	oft += print_field(sfp, MaxSustainedTrafficRate, "%d");
	oft += print_field(sfp, MaxTrafficBurst, "%d");
	oft += print_field(sfp, MinReservedTrafficRate, "%d");
	oft += print_field(sfp, MinTolerableRate, "%d");
	/* Even for DL, we add ULGrantSchedulingType (some BS needs this) */
	oft += snprintf(str+oft, size-oft, "\tULGrantSchedulingType='%s'\n",
				schedstr[sfp->ULGrantSchedulingType]);
#if 0
	if (!sfp->DL) {
		oft += print_field(sfp, RequestTransmissionPolicy, "0x%02x");	    
	}
#endif
	oft += print_field(sfp, RequestTransmissionPolicy, "0x%02x");	    
	
	oft += print_field(sfp, ToleratedJitter, "%d");
	oft += print_field(sfp, MaxLatency, "%d");
	oft += print_field(sfp, FixedLengthSDUIndicator, "%d");
	if (sfp->FixedLengthSDUIndicator)
		oft += print_field(sfp, SDUSize, "%d");
	if (sfp->TargetSAID != 0xffff)
		oft += print_field(sfp, TargetSAID, "%02x");

	oft += print_field(sfp, ARQEnable, "%d");
	if (sfp->ARQEnable) {
		oft += print_field(sfp, ARQWindowSize, "%d");
		oft += print_field(sfp, ARQTransmitterDelay, "%d");
		oft += print_field(sfp, ARQReceiverDelay, "%d");
		oft += print_field(sfp, ARQBlockLifeTime, "%d");
		oft += print_field(sfp, ARQSyncLossTimeout, "%d");
		oft += print_field(sfp, ARQDeliverInOrder, "%d");
		oft += print_field(sfp, ARQRxPurgeTimeout, "%d");
		oft += print_field(sfp, ARQBlockSize, "%d");
		oft += print_field(sfp, ReceiverARQAckProcessingTime, "%d");
	}

	oft += snprintf(str+oft, nnsize, "\tCSSpecification='%s'\n",
			csstr[sfp->CSSpecification]);
	if (sfp->DL) 
	    oft += snprintf(str+oft, nnsize, "\tTypeOfDataDeliveryServices='%s'\n",
			    ddsstr[sfp->TypeOfDataDeliveryServices]);

	oft += print_field(sfp, SDUInterArrivalInterval, "%d");
	oft += print_field(sfp, TimeBase, "%d");
	oft += print_field(sfp, PagingPreference, "%d");
	oft += print_field(sfp, TrafficPreferenceIndication, "%d");
	oft += print_field(sfp, SNFeedbackEnabled, "%d");
	oft += print_field(sfp, FSNSize, "%d");
	
	switch (sfp->ULGrantSchedulingType) {
		case UL_SCHED_TYPE_UGS:
		case UL_SCHED_TYPE_ertPS:
			oft += print_field(sfp, UnsolicitedGrantInterval, "%d");
			break;
		case UL_SCHED_TYPE_rtPS:
		case UL_SCHED_TYPE_nrtPS:
			oft += print_field(sfp, UnsolicitedPollingInterval, "%d");
			break;
		default:
			break;
	}
	
	oft += print_field(sfp, HARQServiceFlows, "%d");
	if (sfp->HARQServiceFlows) {
		oft += print_field(sfp, PDUSNExtendedSubheaderForHARQReordering, "%d");
	}
	return oft;
}


#define etherstr	"%02x:%02x:%02x:%02x:%02x:%02x"
#define ether(x)	x[0], x[1], x[2], x[3], x[4], x[5]

int sprintf_classifier_rule(char *str, unsigned int size, struct wimax_classification_rule *c)
{
	int oft = 0;
	char addr[INET6_ADDRSTRLEN], mask[INET6_ADDRSTRLEN];
	
	if (c == NULL)
		return 0;

	oft += print_field(c, ClassifierRulePriority, "%d");

	if (c->IPTypeOfService.low <= c->IPTypeOfService.high) {
		oft += snprintf(str+oft, size, "\tToS=[%02x-%02x]/%02x\n",
				c->IPTypeOfService.low, c->IPTypeOfService.high,
				c->IPTypeOfService.mask);
	}
	oft += print_field(c, PacketClassifierRuleIndex, "%d");

	if (c->Protocol != 255)
		oft += print_field(c, Protocol, "%d");

	/* IPv4 */
	if (memcmp(&c->IPv4MaskedSourceAddress,
		   &classifier_rule_init.IPv4MaskedSourceAddress, 8)) {
		inet_ntop(AF_INET, &c->IPv4MaskedSourceAddress.address,
			  addr, sizeof(addr));
		inet_ntop(AF_INET, &c->IPv4MaskedSourceAddress.mask,
			  mask, sizeof(mask));
		oft += snprintf(str+oft, nnsize, "\tIPv4MaskedSourceAddress=%s/%s\n",
				addr, mask);
	}

	if (memcmp(&c->IPv4MaskedDestAddress,
		   &classifier_rule_init.IPv4MaskedDestAddress, 8)) {
		inet_ntop(AF_INET, &c->IPv4MaskedDestAddress.address,
			  addr, sizeof(addr));
		inet_ntop(AF_INET, &c->IPv4MaskedDestAddress.mask,
			  mask, sizeof(mask));
		oft += snprintf(str+oft, nnsize, "\tIPv4MaskedDestAddress=%s/%s\n",
				addr, mask);
	}
		    
	/* IPv6 */
	if (memcmp(&c->IPv6MaskedSourceAddress,
		   &classifier_rule_init.IPv6MaskedSourceAddress, 32)) {
		inet_ntop(AF_INET6, &c->IPv6MaskedSourceAddress.address,
			  addr, sizeof(addr));
		inet_ntop(AF_INET6, &c->IPv6MaskedSourceAddress.mask,
			  mask, sizeof(mask));
		oft += snprintf(str+oft, nnsize, "\tIPv6MaskedSourceAddress=%s/%s\n",
				addr, mask);
	}


	if (memcmp(&c->IPv6MaskedDestAddress,
		   &classifier_rule_init.IPv6MaskedDestAddress, 32)) {
		inet_ntop(AF_INET6, &c->IPv6MaskedDestAddress.address,
			  addr, sizeof(addr));
		inet_ntop(AF_INET6, &c->IPv6MaskedDestAddress.mask,
			  mask, sizeof(mask));
		oft += snprintf(str+oft, nnsize, "\tIPv6MaskedDestAddress=%s/%s\n",
				addr, mask);
	}

	if (c->Protocol != 255) {
		if (c->ProtocolSourcePort.low <= c->ProtocolSourcePort.high) {
			oft += snprintf(str+oft, nnsize, "\tSourcePort=[%d-%d]\n",
					c->ProtocolSourcePort.low, c->ProtocolSourcePort.high);
		}
	
		if (c->ProtocolDestPort.low <= c->ProtocolDestPort.high) {
			oft += snprintf(str+oft, nnsize, "\tDestPort=[%d-%d]\n",
					c->ProtocolDestPort.low, c->ProtocolDestPort.high);
		}
	}

	/* Ether */
	if (memcmp(&c->EthernetDestMACAddress,
		   &classifier_rule_init.EthernetDestMACAddress, 12)) {
		oft += snprintf(str+oft, nnsize, "\tEthernetDestMACAddress=" etherstr "/" etherstr"\n",
				ether(c->EthernetDestMACAddress.addr),
				ether(c->EthernetDestMACAddress.mask));		
	}

	if (memcmp(&c->EthernetSourceMACAddress,
		   &classifier_rule_init.EthernetSourceMACAddress, 12)) {
		oft += snprintf(str+oft, nnsize, "\tEthernetSourceMACAddress=" etherstr "/" etherstr"\n",
				ether(c->EthernetSourceMACAddress.addr),
				ether(c->EthernetSourceMACAddress.mask));
	}

	if (c->EtherType.type != 4) {
		oft += snprintf(str+oft, nnsize, "\tEtherType={type=%d, %02x%02x}\n",
				c->EtherType.type, c->EtherType.eprot1, c->EtherType.eprot2);
	}

	if (c->IEEE802_1D_UserPriority.low < 8)
		oft += snprintf(str+oft, nnsize, "\tIEEE802_1D_UserPriority=[%d-%d]\n",
				c->IEEE802_1D_UserPriority.low,
				c->IEEE802_1D_UserPriority.high);

	if (c->IEEE802_1Q_VLANID != 0xfff)
		oft += snprintf(str+oft, nnsize, "\tIEEE802_1Q_VLANID=%04x\n",
				c->IEEE802_1Q_VLANID);

	if (c->AssociatedPHSI != 0)
		oft += print_field(c, AssociatedPHSI, "%d");

	return oft;
}

int sprintf_phs_rule(char *str, unsigned int size, struct wimax_phs_rule *p)
{
	int oft = 0, i = 0, phsm_size;

	if ((p == NULL) || !p->PHSS)
		return 0;
	
	if (p->PHSI)
		oft += print_field(p, PHSI, "%d");

	oft += print_field(p, PHSS, "%d");
	
	/* PHSM */
	if (p->PHSM) {
		phsm_size = (p->PHSS + 7)/8;
		oft += snprintf(str+oft, size, "\tPHSM=");
		for (i=0; i < phsm_size; i++) {
			oft += snprintf(str+oft, nnsize, "%02x", p->PHSM[i]);
		}
		oft += snprintf(str+oft, nnsize, "\n");
	}
	
	/* PHSF */
	if (p->PHSF) {
		oft += snprintf(str+oft, nnsize, "\tPHSF=");
		for (i=0; i < p->PHSS; i++) {
			oft += snprintf(str+oft, nnsize, "%02x", p->PHSF[i]);
		}
		oft += snprintf(str+oft, nnsize, "\n");
	}
	
	oft += print_field(p, PHSV, "%d");

	return oft;
}

static const char *ccstr[] = {
	"OK/Success",
	"reject-other",
	"reject-unrecoginized-configuration-setting",
	"reject-temporary/reject-resource",
	"reject-permanent/reject-admin",
	"reject-not-owner",
	"reject-service-flow-not-found",
	"reject-service-flow-exists",
	"reject-required-parameter-not-present",
	"reject-header-suppression",
	"reject-unknown-transaction-id",
	"reject-authentication-failure",
	"reject-add-aborted",
	"reject-exceeded-dynamic-service-limit",
	"reject-not-authorized-for-requested-SAID",
	"reject-fail-to-establish-the-requested-SA",
	"reject-not-supported-parameter",
	"reject-not-supported-parameter-value",
};

const char *wimax_stringify_dsx_cc(WIMAX_SF_CC cc)
{
	if (cc >= WIMAX_SF_SUCCESS &&
	    cc <= WIMAX_SF_NOT_SUPPORTED_PARAMETER_VALUE)
		return ccstr[cc];
	else if (cc == 128)
		return "reject-time-out";
	else
		return "reject-unknown";
}

/*----------------------------------------------------------------------
 *
 * SDK server routines
 *
 *----------------------------------------------------------------------*/

#define C_RULE_PARAM(i, l, x) {i, l, offsetof(struct wimax_classification_rule, x)}
#define PHS_RULE_PARAM(i, x) {i, 0, offsetof(struct wimax_phs_rule, x)}
#define SF_PARAM(i, x)	{i, 0, offsetof(struct wimax_sf_param, x)}

struct tlv_offset {
	uint8_t		T;
	uint32_t	L;
	uint32_t	offset;
};

struct tlv_offset c_rule_oft_tbl[] = {
	C_RULE_PARAM(1, 1, ClassifierRulePriority),
	C_RULE_PARAM(2, 3, IPTypeOfService),
	C_RULE_PARAM(3, 1, Protocol),
	C_RULE_PARAM(4, 8, IPv4MaskedSourceAddress),
	C_RULE_PARAM(5, 8, IPv4MaskedDestAddress),
	C_RULE_PARAM(6, 4, ProtocolSourcePort),
	C_RULE_PARAM(7, 4, ProtocolDestPort),
	C_RULE_PARAM(8, 12, EthernetDestMACAddress),
	C_RULE_PARAM(9, 12, EthernetSourceMACAddress),
	C_RULE_PARAM(10, 3, EtherType),
	C_RULE_PARAM(11, 2, IEEE802_1D_UserPriority),
	C_RULE_PARAM(12, 2, IEEE802_1Q_VLANID),
	C_RULE_PARAM(13, 1, AssociatedPHSI),
	C_RULE_PARAM(14, 2, PacketClassifierRuleIndex),
	C_RULE_PARAM(15, 3, IPv6FlowLabel),
	C_RULE_PARAM(18, 1, ContextID),
};

struct tlv_offset phs_tlv_oft_tbl[] = {
	PHS_RULE_PARAM(1, PHSI),
	PHS_RULE_PARAM(2, PHSF),
	PHS_RULE_PARAM(3, PHSM),
	PHS_RULE_PARAM(4, PHSS),
	PHS_RULE_PARAM(5, PHSV),
};

struct tlv_offset sf_tlv_oft_tbl[] = {
	SF_PARAM(1, SFID),
	SF_PARAM(2, CID),
	SF_PARAM(3, ServiceClassName),
	SF_PARAM(4, MBSService),
	SF_PARAM(5, QosParamSetType),
	SF_PARAM(6, TrafficPriority),
	SF_PARAM(7, MaxSustainedTrafficRate),
	SF_PARAM(8, MaxTrafficBurst),
	SF_PARAM(9, MinReservedTrafficRate),
	SF_PARAM(10, MinTolerableRate),
	SF_PARAM(11, ULGrantSchedulingType),
	SF_PARAM(12, RequestTransmissionPolicy),
	SF_PARAM(13, ToleratedJitter),
	SF_PARAM(14, MaxLatency),
	SF_PARAM(15, FixedLengthSDUIndicator),
	SF_PARAM(16, SDUSize),
	SF_PARAM(17, TargetSAID),
	SF_PARAM(18, ARQEnable),
	SF_PARAM(19, ARQWindowSize),
	SF_PARAM(20, ARQTransmitterDelay),
	SF_PARAM(21, ARQReceiverDelay),
	SF_PARAM(22, ARQBlockLifeTime),
	SF_PARAM(23, ARQSyncLossTimeout),
	SF_PARAM(24, ARQDeliverInOrder),
	SF_PARAM(25, ARQRxPurgeTimeout),
	SF_PARAM(26, ARQBlockSize),
	SF_PARAM(27, ReceiverARQAckProcessingTime),
	SF_PARAM(28, CSSpecification),
	SF_PARAM(29, TypeOfDataDeliveryServices),	// 29
	SF_PARAM(30, SDUInterArrivalInterval),	// 30
	SF_PARAM(31, TimeBase),			// 31
	SF_PARAM(32, PagingPreference),		// 32
	SF_PARAM(33, MBSZoneID),			// 33
	SF_PARAM(34, TrafficPreferenceIndication),	// 34
	SF_PARAM(35, GlobalServiceClassName),	// 35
	SF_PARAM(36, SNFeedbackEnabled),		// 37
	SF_PARAM(38, FSNSize),			// 38
	SF_PARAM(39, CIDAllocForActiveBSs),	// 39 ??
	SF_PARAM(40, UnsolicitedGrantInterval),	// 40
	SF_PARAM(41, UnsolicitedPollingInterval),	// 41
	SF_PARAM(42, PDUSNExtendedSubheaderForHARQReordering),	// 42
	SF_PARAM(43, MBSContentsIDs),	// 43
	SF_PARAM(44, HARQServiceFlows),		// 44
	SF_PARAM(45, AuthorizationToken),
	SF_PARAM(46, HARQChannelMapping),
};

static int32_t param_offset(struct tlv_offset *table, uint32_t nr_entry, uint8_t T)
{
	uint32_t i;

	for (i = 0; i < nr_entry; i++) {
		if (table[i].T == T)
			return table[i].offset;
	}

	return -1;
}


/*----------------------------------------------------------------------
 *
 * CLASSIFICATION RULE
 *
 *----------------------------------------------------------------------*/
struct wimax_classification_rule *
create_classification_rule(struct wimax_service_flow *sf)
{
	struct wimax_classification_rule *rule;
	int i;

	rule = sf->classification_rule;
	for (i = 0; i < WIMAX_MAX_RULES; i++, rule++) {
		if (!rule->valid) {
			*rule = classifier_rule_init;
			rule->valid = 1;
			return rule;
		}
	}
	return NULL;	
}

void
remove_classification_rule(struct wimax_service_flow *sf,
			   struct wimax_classification_rule *rule)
{
	rule->valid = 0;
}

void
remove_all_classification_rule(struct wimax_service_flow *sf)
{
	struct wimax_classification_rule *rule;
	int i;

	rule = sf->classification_rule;
	for (i = 0; i < WIMAX_MAX_RULES; i++, rule++)
		remove_classification_rule(sf, rule);
}


/*
 * crule_param_offset() gives the offset ofthe member of the
 * struct wimax_classification_rule, which corresponds to the given @T.
 *
 * For example, T=2 corresponds to struct wimax_classification_rule:IPTypeofService,
 * whose offset is XXX in struct wimax_classification_rule.
 */
static inline int32_t crule_param_offset(uint8_t T) {
	
	return param_offset(c_rule_oft_tbl,
			    sizeof(c_rule_oft_tbl)/sizeof(c_rule_oft_tbl[0]),
			    T);
}


/*
 * update_classification_rule() updates a particular classifier rule
 * pointed to by @rule with the content in DSx_Complete HCI packet @pkt.
 *
 * @start and @end are the offsets in @pkt->data where service flow TLV
 * [145/146].cst.3.xxx starts and ends, respectively.
 *
 * Currently, we simply update individual members of
 * struct wimax_classification_rule depending on the TLVs included in @pkt.
 */
void update_classification_rule(struct wimax_service_flow *sf,
				struct wimax_classification_rule *rule,
				struct hci *pkt,
				uint32_t start, uint32_t end)
{
	uint8_t T, *V;
	uint32_t L;

	while (start < end) {
		start = sf_hci_get_tlv(pkt, &T, &L, &V, start);
		memcpy((void *)rule + crule_param_offset(T), V, L);
		rule->mask.all |= (1<<T);
	}

	return;
}

/*
 * update_classification() updates (add/replace/delete) a particular
 * classifier rules associated with given @sf.
 *
 * @sf: a service flow to work upon.
 * @pkt: DSX_Complete HCI message received from the device.
 * @start, @end: [@start, @end) contains the value of the
 *  compound TLV [145/146].cst.
 *
 */

int update_classification(struct wimax_service_flow *sf,
			  struct hci *pkt,
			  uint32_t start, uint32_t end,
			  WIMAX_SF_EVENT_P event)
{
	uint32_t L, pstart, pend;
	uint8_t *param, *V;
	uint16_t index;
	struct wimax_classification_rule *rule;

	while (start < end) {

		/* Search for [145/146].cst.3 - Classification rule parameter (compound) */
		if (!sf_hci_find_tlv_range(pkt, 3, &L, &param, start, end))
			break; /* No classifier rules - go out */

		pstart = param - pkt->data;
		pend = pstart + L;
		
		/* Search for [145/146].cst.3.14 - ClassifierRuleIndex */
		if (!sf_hci_find_tlv_range(pkt, 14, &L, &V, pstart, pend)) {
			xprintf(SDK_DBG, "Cannot find RuleIndex\n");
			return -1;
		}
		index = get_unaligned_u16(V);
	
		if (!(rule = find_classification_rule(sf, index)))
			rule = create_classification_rule(sf);
		if (!rule) {
			xprintf(SDK_ERR, "Too many classification rules\n");
			return -1;
		}

		update_classification_rule(sf, rule, pkt, pstart, pend);

		start = pend;
	}

	return 0;
}

/*----------------------------------------------------------------------
 *
 * PHS RULE
 *
 *----------------------------------------------------------------------*/
struct wimax_phs_rule *create_phs_rule(struct wimax_service_flow *sf)
{
	struct wimax_phs_rule *rule;
	int i;
	
	rule = sf->phs_rule;
	for (i = 0; i < WIMAX_MAX_RULES; i++, rule++) {
		if (!rule->valid) {
			*rule = phs_rule_init;
			rule->valid = 1;
			return rule;
		}
	}
	return NULL;	
}

void remove_phs_rule(struct wimax_service_flow *sf,
		     struct wimax_phs_rule *rule)
{
	rule->valid = 0;
}


void remove_all_phs_rule(struct wimax_service_flow *sf)
{
	struct wimax_phs_rule *rule;
	int i;

	rule = sf->phs_rule;
	for (i = 0; i < WIMAX_MAX_RULES; i++, rule++)
		remove_phs_rule(sf, rule);
}



static inline int32_t phs_rule_param_offset(uint8_t T) {
	
	return param_offset(phs_tlv_oft_tbl,
			    sizeof(phs_tlv_oft_tbl)/sizeof(phs_tlv_oft_tbl[0]),
			    T);
}

/*
 * [145/146].cst.6.xxx
 */
uint32_t update_phs_rule(struct wimax_service_flow *sf,
			 struct wimax_phs_rule *rule,
			 struct hci *pkt,
			 uint32_t start, uint32_t end)
{
	uint8_t T, *V;
	uint32_t L;

	while (start < end) {
		start = sf_hci_get_tlv(pkt, &T, &L, &V, start);
		memcpy((void *)rule + phs_rule_param_offset(T), V, L);
	}

	return 0;
}


int update_phs(struct wimax_service_flow *sf,
	       struct hci *pkt,
	       uint32_t start, uint32_t end,
	       WIMAX_SF_EVENT_P event)
{
	uint8_t *V;
	uint32_t L, pstart, pend;
	uint8_t *param;
	struct wimax_phs_rule *rule;
	uint8_t phsi;

	while (start < end) {
	    
		/* Search for [145/146].cst.6 - PHS rule parameter (compound) */
		if (!sf_hci_find_tlv_range(pkt, 6, &L, &param, start, end)) {
			xprintf(SDK_DBG, "Could not find PHS Rule parameter\n");
			goto out;
		}

		pstart = param - pkt->data;
		pend = pstart + L;

		/* Search for [145/146].cst.6.1 */
		if (!sf_hci_find_tlv_range(pkt, 1, &L, &V, pstart, pend)) {
			xprintf(SDK_DBG, "Cannot find PHSI\n");
			goto out;
		}
		phsi = *V;

		if (!(rule = find_phs_rule(sf, phsi)))
			rule = create_phs_rule(sf);
		if (!rule) {
			xprintf(SDK_ERR, "Too many PHS rules\n");
			return -1;
		}
			
		update_phs_rule(sf, rule, pkt, pstart, pend);

		start = pend;
	}
out:
	return 0;
}



/*----------------------------------------------------------------------
 *
 * SERVICE FLOW
 *
 *----------------------------------------------------------------------*/

static inline int32_t sf_param_offset(uint8_t T) {
	
	return param_offset(sf_tlv_oft_tbl,
			    sizeof(sf_tlv_oft_tbl)/sizeof(sf_tlv_oft_tbl[0]),
			    T);
}


struct wimax_service_flow *create_service_flow(int dev_idx,
					       uint32_t sfid,
					       uint8_t direction)
{
	device_t *dev;
	struct wimax_service_flow *sf;
	int i;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return NULL;
	
	for (i = 0; i < WIMAX_MAX_SERVICE_FLOW; i++) {
		sf = dev->wimax->dev_info.service_flow.sf + i;
		if (!sf->valid) {
			/* Clear the service flow parameter */
			memset(&sf->param, 0, sizeof(sf->param));
			sf->param.SFID = sfid;
			sf->param.DL = direction;
			sf->param.TargetSAID = 0xffff;
			sf->valid = 1;			
			goto out;
		}
	}
	sf = NULL;

out:
	dm_put_dev(dev_idx);
	xfunc_out();
	return sf;
}

void remove_service_flow(int dev_idx,
			 struct wimax_service_flow *sf)
{
	sf->valid = 0;
}

void remove_all_service_flow(int dev_idx)
{
	device_t *dev;
	struct wimax_service_flow *sf;
	int i;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return;
	
	for (i = 0; i < WIMAX_MAX_SERVICE_FLOW; i++) {
		sf = dev->wimax->dev_info.service_flow.sf + i;
		
		remove_all_classification_rule(sf);
		remove_all_phs_rule(sf);
		remove_service_flow(dev_idx, sf);
	}

	dm_put_dev(dev_idx);
	xfunc_out();	
}


/*
 * @sf (in): service flow to fill
 * @payload: payload of DSx_Complete message that starts with 145/146.
 */
void update_service_flow(struct wimax_service_flow *sf,
			 struct hci *pkt,
			 uint32_t start, uint32_t end, uint8_t change,
			 WIMAX_SF_EVENT_P event)
{
	uint8_t T, *V, cs;
	uint32_t L, cstart, cend, oft = start;

	while (oft < end) {
		oft = sf_hci_get_tlv(pkt, &T, &L, &V, oft);
		switch (T) {
		default:
			memcpy((void *)(&sf->param) + sf_param_offset(T), V, L);
			break;
		case 46:
			/* TODO */
			break;
		case 99:	/* ATM */
			cs = 9;
			/* fall-through */
		case 100:	/* Packet, IPv4 */
		case 101:	/* Packet, IPv6 */
		case 102:	/* Packet, IEEE 802.3/Ethernet */
		case 103:	/* Packet, IEEE 802 1Q VLAN */
		case 104:	/* Packet IPv4 over IEEE 802.3/Ethernet */ 
		case 105:	/* Packet IPv6 over IEEE 802.3/Ethernet */
		case 106:	/* Packet IPv4 over IEEE 802.1Q VLAN */
		case 107:	/* Packet IPv6 over IEEE 802.1Q VLAN */
			cs = T - 99;
			if (sf->param.CSSpecification != cs) {
				xprintf(SDK_NOTICE, "CS specification mismatch!\n");
				break;
			}
			cstart = V - pkt->data;
			cend = cstart + L;
			update_classification(sf, pkt, cstart, cend, event);
			update_phs(sf, pkt, cstart, cend, event);
			break;
		}
	}
}

static int dev_notify_event(int dev_idx, WIMAX_SF_EVENT_P pSfEvent)
{
	sdk_internal_t *sdk = sdk_get_rw_handle();
	dev_hand_t dev_hand;

	xfunc_in("sfEvent->code=%d, confirm code=%d", pSfEvent->code, pSfEvent->sf.cc);

	if (sdk->ind.noti_service_flow) {
		dev_hand.api = sdk->api;
		dev_hand.dev_idx = dev_idx;
		xprintf(SDK_DBG, "Call callback(%s)\n", __FUNCTION__);
		sdk->ind.noti_service_flow(&dev_hand, pSfEvent);
	}
	
	xfunc_out();

	return 0;
}

/* 
 * compare the received service flow with the previous one to generate the event 
 * Event generated:
 *   - SF changed (classification/phs rule added/removed/changed)
 */
int compare_service_flow(struct wimax_service_flow *sf, 
			 struct wimax_service_flow *old, 
			 int dev_idx, 
			 WIMAX_SF_EVENT_P event)
{
	struct wimax_classification_rule *clfr = NULL, *clfr_old = NULL;
	struct wimax_phs_rule *phs = NULL, *phs_old = NULL;
	uint8_t found = 0;

	event->sf.phs_action = DSC_NOP_PHS;
	event->sf.PHSI = 0;
	
	/* classification rule */
	while ((clfr = next_classification_rule(sf, clfr))) {
		while ((clfr_old = next_classification_rule(old, clfr_old))) {
			if (clfr->PacketClassifierRuleIndex == clfr_old->PacketClassifierRuleIndex) {
				found = 1;
				/* replace */
				if (memcmp(clfr, clfr_old, sizeof(struct wimax_classification_rule)) != 0) {
					event->sf.clfr_action = DSC_REPLACE_CLASSIFIER;
					event->sf.PacketClassificationRuleIndex = clfr->PacketClassifierRuleIndex;
					dev_notify_event(dev_idx, event);
				}
				break;
			}
		}
		
		/* If the clfr of the received sf is not matched to any rule in the previous sf, 
		 * it is regarded as newly added.
		 * If it is matched to any rule, delete clfr in the previous sf.
		 */
		if (!found) {
			event->sf.clfr_action = DSC_ADD_CLASSIFIER;
			event->sf.PacketClassificationRuleIndex = clfr->PacketClassifierRuleIndex;
			dev_notify_event(dev_idx, event);
		} else 
			remove_classification_rule(old, clfr_old);	/* delete the found one for replace */
		
		found = 0;
	}
	
	/* find the removed classification rule index
	 * If there remains any clfr rule in the previous service flow, it must be the deleted one.
	 */
	clfr_old = NULL;
	while ((clfr_old = next_classification_rule(old, clfr_old))) {
		event->sf.clfr_action = DSC_DELETE_CLASSIFIER;
		event->sf.PacketClassificationRuleIndex = clfr_old->PacketClassifierRuleIndex;
		dev_notify_event(dev_idx, event);
	}
	
	event->sf.clfr_action = DSC_NOP_CLASSIFIER;
	event->sf.PacketClassificationRuleIndex = 0;

	/* phs rule */
	found = 0;
	while ((phs = next_phs_rule(sf, phs))) {
		while ((phs_old = next_phs_rule(old, phs_old))) {
			if (phs->PHSI == phs_old->PHSI) {
				found = 1;
				break;
			}
		}
		if (!found) {
			event->sf.phs_action = DSC_ADD_PHS;
			event->sf.PHSI = phs->PHSI;
			dev_notify_event(dev_idx, event);
		} else
			remove_phs_rule(old, phs_old);	/* remove the found phs rule */
		
		found = 0;
	}
	
	/* find the removed phs rule */
	phs_old = NULL;
	while ((phs_old = next_phs_rule(old, phs_old))) {
		event->sf.phs_action = DSC_DELETE_PHS;
		event->sf.PHSI = phs_old->PHSI;
		dev_notify_event(dev_idx, event);
	}

	return 0;
}

/*
 * dev_update_service_flow() - Handles DSx_Complete HCI message.
 * 
 * Event generated:
 *   - SF added (result, sfid)
 *   - SF changed (result, sfid, classification/phs rule added/removed/changed)
 *   - SF deleted (result, sfid)
 */
void dev_update_service_flow(int dev_idx, struct hci *pkt)
{
	device_t *dev;
	struct wimax_service_flow *sf, sf_old;
	uint8_t T, *V,  action = pkt->data[0];
	uint8_t status = pkt->data[1], direction;
	uint32_t sfid;
	uint32_t L, start, end, oft = 2;
	WIMAX_SF_EVENT levent, *event;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return;
	
	if ((action & 0x80) == 0x80) 
		event = &dev->wimax->dev_info.service_flow.last_sf_event;
	else
		event = &levent;

	xprintf(SDK_DBG, "DSX(len=%d)\n", pkt->length);
	xprintf_hex(SDK_DBG, SDK_LOG_TITLE, pkt->data, pkt->length);
	
	/* DSX related event is generated only by server. */
	memset(event, 0, sizeof(*event));
	
	/* initialize clfr and phs */
	event->sf.clfr_action = DSC_NOP_CLASSIFIER;
	event->sf.phs_action = DSC_NOP_PHS;
	event->sf.PacketClassificationRuleIndex = 0;
	event->sf.PHSI = 0;

	/* filling event */
	event->sf.cc = status;
	event->sf.ms_initiated = ((action & 0x80) == 0x80);	
	event->code = WIMAX_EVT_SERVICE_FLOW_ADDED + (action & 0x7f);
	
	if (status)
		goto out;
	
	/* If action&0x7f is 7, it is only two-byte packet. */
	if (pkt->length > 2) {
		sf_hci_get_tlv(pkt, &T, &L, &V, oft);
		if (T != 145 && T != 146)
			T = 145; /* Workaround */
		
		direction = T - 145;
		start = V - pkt->data;
		end = start + L;
		
		/* Find SFID TLV */
		if (!sf_hci_find_tlv_range(pkt, 1, &L, &V, start, end)) {
			xprintf(SDK_ERR, "Warning: SFID missing\n");
			goto out;
		}
		memcpy(&sfid, V, L);
		event->sf.sfid = sfid;
	}

	pthread_mutex_lock(&dev->wimax->dev_info.service_flow.sfwritelock);
	switch (action & 0x7f) {
	case 0: /* DSA */
		sf = find_service_flow(dev_idx, sfid);
		if (sf == NULL) {
			sf = create_service_flow(dev_idx, sfid, direction);
			if (sf == NULL) {
				xprintf(SDK_ERR, "DSA : cannot create service flow %d\n", sfid);
				goto out2;
			}
		}
		remove_all_classification_rule(sf);
		remove_all_phs_rule(sf);
		update_service_flow(sf, pkt, start, end, 0, event);
		break;
	case 1: /* DSC */
		sf = find_service_flow(dev_idx, sfid);
		if (sf == NULL) {
			xprintf(SDK_ERR, "DSC : cannot find service flow %d\n", sfid);
			goto out2;
		}
		/* Check if this DSC is for CID update */
		if (pkt->data[3] == 0xa) {
			if (!sf_hci_find_tlv_range(pkt, 2, &L, &V, start, end)) 
				xprintf(SDK_ERR, "Warning: CID missing\n");
			else {
				memcpy(&sf->param.CID, V, L);
				event->code = WIMAX_EVT_CID_UPDATE;
				dev_notify_event(dev_idx, event);
			}
			pthread_mutex_unlock(&dev->wimax->dev_info.service_flow.sfwritelock);
			goto out2;
		}

		memcpy(&sf_old, sf, sizeof(struct wimax_service_flow));
		remove_all_classification_rule(sf);
		remove_all_phs_rule(sf);
		update_service_flow(sf, pkt, start, end, 0, event);
		compare_service_flow(sf, &sf_old, dev_idx, event);
		break;
	case 2: /* DSD */
		sf = find_service_flow(dev_idx, sfid);
		if (sf)
			remove_service_flow(dev_idx, sf);
		else
			xprintf(SDK_NOTICE, "DSD for non-existing SFID (%08x)\n", sfid);
		break;
	case 7: /* Delete all service flows */
		remove_all_service_flow(dev_idx);
		pthread_mutex_unlock(&dev->wimax->dev_info.service_flow.sfwritelock);
		xprintf(SDK_DBG, "DSX : Remove all service flow\n");
		goto out2;
	default:
		xprintf(SDK_NOTICE, "Unknown DSx_Complete mode %d\n", action);
		pthread_mutex_unlock(&dev->wimax->dev_info.service_flow.sfwritelock);
		goto out2;
	}
	pthread_mutex_unlock(&dev->wimax->dev_info.service_flow.sfwritelock);
	
out:	
	if (event->sf.ms_initiated) {
	    pthread_cond_signal(&dev->wimax->dev_info.service_flow.dsx_cond);
	}
	
	if ((action & 0x7f) != 1) 
		dev_notify_event(dev_idx, event);

out2:
	
	dm_put_dev(dev_idx);
	xfunc_out();
	return;
}

struct dl_sf_stat {
	uint32_t	sfid;
	uint16_t	cid;
	uint16_t	feature;

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
} __attribute__((packed));

struct ul_sf_stat {
	uint32_t	sfid;
	uint16_t	cid;
	uint16_t	feature;

	uint32_t	sdu_packets;
	uint32_t	sdu_packets_dropped;
	uint32_t	sdu_bytes;
	uint32_t	sdu_bytes_dropped;	
	uint32_t	pdu_packets;
	uint32_t	pdu_bytes;
	
	uint32_t	bwreq;
	uint32_t	gmsh;

	uint32_t	arq_blocks;
	uint32_t	arq_blocks_retry;
	uint16_t	arq_discard;
	uint16_t	arq_reset;
	uint16_t	arq_sync_loss;	
} __attribute__((packed)) ;

/*----------------------------------------------------------------------
 *
 * Client API for SF information retrieval
 *
 *----------------------------------------------------------------------*/

void BeginSFRead(int dev_idx)
{
	device_t *dev;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return;

	pthread_mutex_lock(&dev->wimax->dev_info.service_flow.sfreadlock);

	dm_put_dev(dev_idx);
	xfunc_out();
}


void EndSFRead(int dev_idx)
{
	device_t *dev;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return;

	pthread_mutex_unlock(&dev->wimax->dev_info.service_flow.sfreadlock);

	dm_put_dev(dev_idx);
	xfunc_out();
}

/*
 * GetNextSF() - returns a pointer to the vailable service flow
 * in the device that apears right after pSFParam.
 *
 * Direction: 0 (UL), 1 (DL), 2 (DL + UL)
 *
 * To get the first service flow, set pSFParam to NULL.
 * If the function returns NULL, we are done.
 *
 * SYNOPSIS:
 *
 * WIMAX_SERVICE_FLOW_P sf;
 *
 * sf = BeginSF(pDeviceID, direction);
 * while (sf) {
 *	DO ACTION on sf
 *  	sf = GetNextSF(pDeviceID, sf, direction)));
 * }
 * EndSF(pDeviceID);
 * 
 *
 * NOTE:
 * The object referenced by the return value of GetNextSF is
 * read-only. Do not modify its content.
 *
 * Limitation:
 * It is not allowed to break inside the above while loop.
 * 
 */
WIMAX_SERVICE_FLOW *GetNextSF(int dev_idx,
			      WIMAX_SERVICE_FLOW *pSF,
			      UINT8 Direction)
{
	device_t *dev;
	WIMAX_SERVICE_FLOW *sf;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return NULL;

	if (pSF == NULL)
		sf = dev->wimax->dev_info.service_flow.sf;
	else
		sf = pSF + 1;

	for (; sf - dev->wimax->dev_info.service_flow.sf < WIMAX_MAX_SERVICE_FLOW; sf++)
		if (sf->valid) {
			if (Direction == 2)
				goto out;
			else if (Direction == sf->param.DL)
				goto out;
			continue;
		}
	sf = NULL;

out:
	dm_put_dev(dev_idx);
	xfunc_out();
	return sf;
}


/*
 * GetServiceFlow() returns a pointer to WIMAX_SERVICE_FLOW that
 * matches the given SFID.
 *
 * The object referenced by the returned pointer is read-only.
 */
WIMAX_SERVICE_FLOW *GetServiceFlow(int dev_idx,
				   UINT32 SFID)

{
	device_t *dev;
	WIMAX_SERVICE_FLOW *sf;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return NULL;

	sf = find_service_flow(dev_idx, SFID);

	dm_put_dev(dev_idx);
	xfunc_out();
	return sf;
}


/*
 * GetCalssificationRule() - Get a single classification rule.
 *
 * pSFParam->SFID, pCLFRRule->ClassficationRuleIndex should be
 * set by caller.
 *  
 */
WIMAX_CLFR_RULE *GetNextClfrRule(WIMAX_SERVICE_FLOW *pSF,
				 WIMAX_CLFR_RULE *pCLFRRule)
{
	return next_classification_rule(pSF, pCLFRRule);
}


WIMAX_CLFR_RULE *GetClfrRule(WIMAX_SERVICE_FLOW *pSF,
			     UINT16 PacketClassfierRuleIndex)
{
	return find_classification_rule(pSF, PacketClassfierRuleIndex);
}


WIMAX_PHS_RULE *GetNextPHSRule(WIMAX_SERVICE_FLOW *pSF,
			       WIMAX_PHS_RULE *pPHSRule)
{

	return next_phs_rule(pSF, pPHSRule);
}

WIMAX_PHS_RULE *GetPHSRule(WIMAX_SERVICE_FLOW *pSF,
			   UINT8 PHSI)
{
	return find_phs_rule(pSF, PHSI);
}

/*----------------------------------------------------------------------
 *
 * SDK Client API for dynamic service flow management
 *
 *----------------------------------------------------------------------*/

const WIMAX_SF_PARAM sf_param_init = {
	.QosParamSetType 		= 0x7, 	/* admitted, active, and provisioned */
	.TrafficPriority		= 0, 	/* lowest priority - default */
	.ULGrantSchedulingType		= UL_SCHED_TYPE_BE,
	.RequestTransmissionPolicy	= 0x10,	/* No PHS */
	.TargetSAID			= 0xffff,
	.ARQEnable			= 0,
	.ARQWindowSize			= 1024,
	.ARQTransmitterDelay		= 750,	/* 750usec */
	.ARQReceiverDelay		= 750,	/* 750usec */
	.ARQBlockLifeTime		= 0,	/* ? */
	.ARQSyncLossTimeout		= 0,	/* ? */
	.ARQDeliverInOrder		= 1,	
	.ARQRxPurgeTimeout		= 0,	/* ? */
	.ARQBlockSize			= 0x60, /* ? */
	.ReceiverARQAckProcessingTime	= 2, 	/* ? */
	.CSSpecification		= 1,	/* ? */
	.TypeOfDataDeliveryServices	= DATA_SERVICE_BE,
	.PagingPreference		= 0, 	/* ? */
	.TrafficPreferenceIndication	= 0, 	/* ? */
	.SNFeedbackEnabled		= 0, 	/* ? */
	.FSNSize			= 1, 	/* default */
	.HARQServiceFlows		= 0, 	/* default */	
	.PDUSNExtendedSubheaderForHARQReordering = 2, /* default is 0, our default is 2 */
};

void InitClassificationRule(uint32_t sfid, WIMAX_CLFR_RULE_P pClfrRule)
{
	memset(pClfrRule, 0, sizeof(WIMAX_CLFR_RULE));

	pClfrRule->IPTypeOfService.low = 1; /* invalid: low > high */
	pClfrRule->Protocol = 255; 	/* invalid: which is reserved */

	/* The trick here is we set address mask to 0x0 and address to a non-zero value, so that
	 * ANDing address and mask always produces zero address, making the test always fails.
	 * The routines that adds TLV to DSx_Request uses the same criterion to check whether
	 * these masked source/destination address is actually set.
	 */
	pClfrRule->IPv4MaskedSourceAddress.address.s_addr = htonl(INADDR_NONE); /* ip-src & mask != INADDR_NONE */
	pClfrRule->IPv4MaskedDestAddress.address.s_addr = htonl(INADDR_NONE);   /* ip-dst & mask != INADDR_NONE */
	
	pClfrRule->IPv6MaskedSourceAddress.address = in6addr_loopback;
	pClfrRule->IPv6MaskedDestAddress.address = in6addr_loopback;

	/* The trick here is to make the port range [low, high] illegal by setting low=1 and high=0 */

	pClfrRule->ProtocolSourcePort.low = 1;  /* low > high */
	pClfrRule->ProtocolDestPort.low = 1;	/* low > high */

	memset(pClfrRule->EthernetDestMACAddress.addr, 0xff, 6);
	memset(pClfrRule->EthernetSourceMACAddress.addr, 0xff, 6);

	pClfrRule->EtherType.type = 4; /* which is invalid */
	pClfrRule->IEEE802_1D_UserPriority.low = 8; /* which is out of range */

	pClfrRule->IEEE802_1Q_VLANID = 0xfff; /* which is reserved */
	pClfrRule->AssociatedPHSI = 0; /* not used - we support less than 255 PHSI */
	pClfrRule->PacketClassifierRuleIndex = 0;
}


#define append_non_zero_tlv(h, t, l, v) \
	if ((v)) sf_hci_append_tlv((h), (t), (l), &(v))

#define append_tlv(h, t, l, v)	sf_hci_append_tlv((h), (t), (l), &(v))


static uint32_t add_one_classification_rule(struct hci *hci,
					    WIMAX_SF_PARAM_P pSFParam,
					    WIMAX_CLFR_RULE_P pClfrRule)
{
	uint8_t cs = pSFParam->CSSpecification;
	uint32_t offset;

	hci->data[hci->length++] = 3; /* Packet classification rules */
	offset = hci->length; /* at most 127 byte long, hopefully */
	hci->length++;

	append_tlv(hci, 1, 1, pClfrRule->ClassifierRulePriority);

	if (pClfrRule->IPTypeOfService.low <= pClfrRule->IPTypeOfService.high)
		append_tlv(hci, 2, 3, pClfrRule->IPTypeOfService);
	
	append_tlv(hci, 14, 2, pClfrRule->PacketClassifierRuleIndex);

	if (pClfrRule->Protocol != 255)
		append_tlv(hci, 3, 1, pClfrRule->Protocol);

	/* IP */
	if (cs !=2 && cs != 6 && cs != 8) {
		/* IPv4 */
		if (memcmp(&pClfrRule->IPv4MaskedSourceAddress,
			   &classifier_rule_init.IPv4MaskedSourceAddress, 8))
			append_tlv(hci, 4, 8, pClfrRule->IPv4MaskedSourceAddress);

		if (memcmp(&pClfrRule->IPv4MaskedDestAddress,
			   &classifier_rule_init.IPv4MaskedDestAddress, 8))
			append_tlv(hci, 5, 8, pClfrRule->IPv4MaskedDestAddress);
		    
	} else if (cs != 1 && cs != 5 && cs != 7) {
		/* IPv6 */
		if (memcmp(&pClfrRule->IPv6MaskedSourceAddress,
			    &classifier_rule_init.IPv6MaskedSourceAddress, 32))
			append_tlv(hci, 4, 32, pClfrRule->IPv6MaskedSourceAddress);

		if (memcmp(&pClfrRule->IPv6MaskedDestAddress,
			    &classifier_rule_init.IPv6MaskedDestAddress, 32))
			append_tlv(hci, 5, 32, pClfrRule->IPv6MaskedDestAddress);
	}

	if (pClfrRule->Protocol != 255) {
		if (pClfrRule->ProtocolSourcePort.low <= pClfrRule->ProtocolSourcePort.high)
			append_tlv(hci, 6, 4, pClfrRule->ProtocolSourcePort);
	
		if (pClfrRule->ProtocolDestPort.low <= pClfrRule->ProtocolDestPort.high)
			append_tlv(hci, 7, 4, pClfrRule->ProtocolDestPort);
	}
	
	/* Ether */
	if (cs >= 3 && cs <= 8) {

		if (memcmp(&pClfrRule->EthernetDestMACAddress,
			   &classifier_rule_init.EthernetDestMACAddress, 12))
			append_tlv(hci, 8, 12, pClfrRule->EthernetDestMACAddress);

		if (memcmp(&pClfrRule->EthernetSourceMACAddress,
			   &classifier_rule_init.EthernetSourceMACAddress, 12))
			append_tlv(hci, 9, 12, pClfrRule->EthernetSourceMACAddress);

		if (pClfrRule->EtherType.type != 4)
			append_tlv(hci, 10, 3, pClfrRule->EtherType);
		
		if (pClfrRule->IEEE802_1D_UserPriority.low < 8)
			append_tlv(hci, 11, 2, pClfrRule->IEEE802_1D_UserPriority); 

		if (cs == 4 || cs == 7 || cs == 8)
			if (pClfrRule->IEEE802_1Q_VLANID != 0xfff)
				append_tlv(hci, 12, 2, pClfrRule->IEEE802_1Q_VLANID);
	}
	
	if (pClfrRule->AssociatedPHSI != 0)
		append_tlv(hci, 13, 1, pClfrRule->AssociatedPHSI);

	hci->data[offset] = hci->length - offset - 1;
	return 2 + hci->data[offset];
}

#define ceiling(x, y)	(((x) + (y)-1)/(y))

static uint32_t add_one_phs_rule(struct hci *hci,
				 WIMAX_SF_PARAM_P pSFParam,
				 WIMAX_PHS_RULE_P pPHSRule)
{
	uint32_t oft, L;
	
	if ( (pPHSRule == NULL) || !pPHSRule->PHSS)
		return 0;

	hci->data[hci->length++] = 6;
	oft = hci->length;
	hci->length += 3;
	
	/* Do not append PHSI here as PHS rule does not support replace */
	append_tlv(hci, 2, pPHSRule->PHSS, pPHSRule->PHSF);
	append_tlv(hci, 3, ceiling(pPHSRule->PHSS, 8), pPHSRule->PHSM);
	append_tlv(hci, 4, 1, pPHSRule->PHSS);
	append_tlv(hci, 5, 1, pPHSRule->PHSV);

	L = hci->length - oft - 3;
	hci->data[oft] = 0x82;
	hci->data[oft+1] = (L >> 8) & 0xff;
	hci->data[oft+2] = L & 0xff;

	return L;
}

static void add_sf_param(struct hci *hci,
			 WIMAX_SF_PARAM_P sfp)
{
	if (!hci->data[0])
		sfp->SFID = 0;
	append_tlv(hci, 1, 4, sfp->SFID);

	if (sfp->ServiceClassName[0])
		sf_hci_append_tlv(hci, 3, strlen((const char*)sfp->ServiceClassName)+1,
			       sfp->ServiceClassName);

	append_tlv(hci, 4, 1, sfp->MBSService);

	/* QoS */
	append_tlv(hci, 5, 1, sfp->QosParamSetType);
	append_tlv(hci, 6, 1, sfp->TrafficPriority);
	append_tlv(hci, 7, 4, sfp->MaxSustainedTrafficRate);
	append_non_zero_tlv(hci, 8, 4, sfp->MaxTrafficBurst);
	append_tlv(hci, 9, 4, sfp->MinReservedTrafficRate);
	append_non_zero_tlv(hci, 10, 4, sfp->MinTolerableRate);
	/* This one is needed for DL as well */
	append_tlv(hci, 11, 1, sfp->ULGrantSchedulingType);
	append_tlv(hci, 12, 1, sfp->RequestTransmissionPolicy);
	append_tlv(hci, 13, 4, sfp->ToleratedJitter);
	append_tlv(hci, 14, 4, sfp->MaxLatency);
	append_tlv(hci, 15, 1, sfp->FixedLengthSDUIndicator);
	if (sfp->FixedLengthSDUIndicator)
		append_tlv(hci, 16, 1, sfp->SDUSize);

	if (sfp->TargetSAID != 0xffff)
		append_tlv(hci, 17, 2, sfp->TargetSAID);
	
	append_tlv(hci, 18, 1, sfp->ARQEnable);
	if (sfp->ARQEnable) {
		append_tlv(hci, 19, 2, sfp->ARQWindowSize);
		append_tlv(hci, 20, 2, sfp->ARQTransmitterDelay);
		append_tlv(hci, 21, 2, sfp->ARQReceiverDelay);
		append_tlv(hci, 22, 2, sfp->ARQBlockLifeTime);
		append_tlv(hci, 23, 2, sfp->ARQSyncLossTimeout);
		append_tlv(hci, 24, 1, sfp->ARQDeliverInOrder);
		append_tlv(hci, 25, 2, sfp->ARQRxPurgeTimeout);
		append_tlv(hci, 26, 2, sfp->ARQBlockSize);
		append_tlv(hci, 27, 1, sfp->ReceiverARQAckProcessingTime);
	}
	append_tlv(hci, 28, 1, sfp->CSSpecification);

	if (sfp->DL)
		append_tlv(hci, 29, 1, sfp->TypeOfDataDeliveryServices);

	append_non_zero_tlv(hci, 30, 2, sfp->SDUInterArrivalInterval);
	append_non_zero_tlv(hci, 31, 2, sfp->TimeBase);
	append_tlv(hci, 32, 1, sfp->PagingPreference);
	if (sfp->MBSService)
		append_tlv(hci, 33, 8, sfp->MBSZoneID);
	append_tlv(hci, 34, 1, sfp->TrafficPreferenceIndication);
	append_tlv(hci, 35, strlen((const char*)sfp->ServiceClassName)+1,
			       sfp->ServiceClassName);
	append_tlv(hci, 37, 1, sfp->SNFeedbackEnabled);
	append_tlv(hci, 38, 1, sfp->FSNSize);

	switch (sfp->ULGrantSchedulingType) {
		case UL_SCHED_TYPE_UGS:
		case UL_SCHED_TYPE_ertPS:
			append_tlv(hci, 40, 2, sfp->UnsolicitedGrantInterval);
			break;
		case UL_SCHED_TYPE_rtPS:
		case UL_SCHED_TYPE_nrtPS:
			append_tlv(hci, 41, 2, sfp->UnsolicitedPollingInterval);
			break;
		default:
			break;
	}
	
	if (sfp->HARQServiceFlows)
		append_tlv(hci, 42, 1, sfp->PDUSNExtendedSubheaderForHARQReordering);

	append_tlv(hci, 44, 1, sfp->HARQServiceFlows);
}


/*
 * CmdAddSF() - Add a new service flow
 *
 * Limitation:
 * 1. Currently, only one classification rule and phs rule can
 *    be specified in CmdAddSF().
 *
 */ 

WIMAX_SF_CC CmdAddSF(int dev_idx,
		     WIMAX_SF_PARAM_P pSFParam,
		     WIMAX_CLFR_RULE_P pClfrRule,
		     WIMAX_PHS_RULE_P pPHSRule)
{
#define timeval2timespec(tv,ts) \
		((ts)->tv_sec = (tv)->tv_sec, (ts)->tv_nsec = (tv)->tv_usec * 1000)

	device_t *dev;
	struct timeval tv;
	struct timespec ts;
	int ret = 0;

	struct hci *hci;
	uint16_t cst_len = 0;
	uint16_t cst_oft, dsx_oft;
	WIMAX_SF_CC cc;
	char buf[2048], *ptr = buf;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	if (dev->fsm.m_status != M_CONNECTED) {
		cc = WIMAX_SF_OTHER;
		goto out;
	}

	hci = malloc(sizeof(struct hci) + 1024); /* large enough? */
	if (!hci) {
		cc = -1; //WIMAX_API_RET_FAILED;
		goto out;
	}

	xprintf(SDK_INFO, "CmdAddSF:");
	ptr += snprintf(ptr, buf + sizeof(buf)-ptr, "SF={\n");
	ptr += sprintf_sf_param(ptr, sizeof(buf), pSFParam);
	if (pClfrRule) {
		ptr += snprintf(ptr, buf + sizeof(buf)-ptr, "\tClassifierRule={\n");
		ptr += sprintf_classifier_rule(ptr, buf + sizeof(buf) - ptr, pClfrRule);	
	}
	if (pPHSRule) {
		ptr += snprintf(ptr, buf + sizeof(buf)-ptr, "\tPHSRule={\n");
		ptr += sprintf_phs_rule(ptr, buf + sizeof(buf) - ptr, pPHSRule);	
	}
	xprintf(SDK_INFO, "%s\t}\n}", buf);
	
	hci->cmd_evt = htons(WIMAX_DSX_REQUEST);
	hci->length = 0;
	hci->data[hci->length++] = 0; /* DSA-REQ */
	hci->data[hci->length++] = 1; /* Use parameter in this hci */
	
	hci->data[hci->length++] = 145 + pSFParam->DL;
	dsx_oft = hci->length;
	hci->length += 3;

	add_sf_param(hci, pSFParam);
	
	if (pClfrRule || pPHSRule) {
		hci->data[hci->length++] = pSFParam->CSSpecification + 99; /* cst */
		cst_oft = hci->length;
		hci->length += 3;
		
		if (pClfrRule)
			cst_len = add_one_classification_rule(hci, pSFParam, pClfrRule);
		
		if (pPHSRule)
			cst_len += add_one_phs_rule(hci, pSFParam, pPHSRule);

		hci->data[cst_oft] = 0x82;
		hci->data[cst_oft+1] = (cst_len >> 8) & 0xff;
		hci->data[cst_oft+2] = cst_len & 0xff;
		/*sf_hci_put_length(hci->data + cst_oft, cst_len);*/
	}

	hci->data[dsx_oft] = 0x82;
	hci->data[dsx_oft+1] = ((hci->length - dsx_oft - 3) >> 8) & 0xff;
	hci->data[dsx_oft+2] = (hci->length - dsx_oft - 3) & 0xff;
	/*sf_hci_put_length(hci->data + dsx_oft, hci->length - dsx_oft - 1);*/

	/* Prevent other clients from sending a DSx request */
	pthread_mutex_lock(&dev->wimax->dev_info.service_flow.dsxlock);

	xprintf(SDK_DBG, "DSA-REQ(len=%d)\n", hci->length);
	xprintf_hex(SDK_DBG, SDK_LOG_TITLE, hci->data, hci->length);

	sf_send_hci(dev_idx, hci);
	free(hci);

	gettimeofday(&tv, NULL);
	timeval2timespec(&tv, &ts);
	ts.tv_sec += DSX_REQUEST_TIMEOUT;
	ret = pthread_cond_timedwait(&dev->wimax->dev_info.service_flow.dsx_cond, &dev->wimax->dev_info.service_flow.dsxlock, &ts);
	if (ret) {
		xprintf(SDK_ERR, "CmdAddSF Error (pthread_cond_timedwait=%d)\n", ret);
		pthread_mutex_unlock(&dev->wimax->dev_info.service_flow.dsxlock);
		cc = -1; //WIMAX_API_RET_FAILED;
		goto out;
	}
	//pthread_cond_wait(&dev->wimax->dev_info.service_flow.dsx_cond, &dev->wimax->dev_info.service_flow.dsxlock);

	/* Figure out the SFID of the newly created one */
	/* We need another bookkeeping information */
	if (dev->wimax->dev_info.service_flow.last_sf_event.code != WIMAX_EVT_SERVICE_FLOW_ADDED) 
		assert(dev->wimax->dev_info.service_flow.last_sf_event.code == WIMAX_EVT_SERVICE_FLOW_ADDED);
	
	pSFParam->SFID = dev->wimax->dev_info.service_flow.last_sf_event.sf.sfid;
	cc = dev->wimax->dev_info.service_flow.last_sf_event.sf.cc;
	
	pthread_mutex_unlock(&dev->wimax->dev_info.service_flow.dsxlock);

	xprintf(SDK_INFO, "DSA-REQ: %s\n", wimax_stringify_dsx_cc(cc));

out:

	dm_put_dev(dev_idx);
	xfunc_out("ret=%d", cc);
	return cc;
}

/* Change a service flow */

WIMAX_SF_CC CmdChangeSF(int dev_idx,
			WIMAX_SF_PARAM_P pSFParam,
			WIMAX_CLFR_DSC_ACTION CLFRDSCAction,
			WIMAX_CLFR_RULE_P pClfrRule,
			WIMAX_PHS_DSC_ACTION PHSDSCAction,
			WIMAX_PHS_RULE_P pPHSRule)
{
#define timeval2timespec(tv,ts) \
		((ts)->tv_sec = (tv)->tv_sec, (ts)->tv_nsec = (tv)->tv_usec * 1000)
	
	device_t *dev;
	struct timeval tv;
	struct timespec ts;
	int ret = 0;

	struct hci *hci;
	uint16_t cst_len = 0;
	uint16_t cst_oft, dsx_oft, tmp_oft;
	WIMAX_SF_CC cc;
	char buf[2048], *ptr = buf;	

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	if (dev->fsm.m_status != M_CONNECTED) {
		cc = WIMAX_SF_OTHER;
		goto out;
	}

	if (!pSFParam || !find_service_flow(dev_idx, pSFParam->SFID)) {
		cc = WIMAX_SF_NOT_FOUND;
		goto out;
	}
	
	hci = (struct hci *)malloc(sizeof(struct hci) + 1024); /* large enough? */
	if (!hci) {
		cc = -1; //WIMAX_API_RET_FAILED;
		goto out;
	}

	xprintf(SDK_INFO, "CmdChangeSF:");
	ptr += snprintf(ptr, buf + sizeof(buf)-ptr, "SF={\n");
	ptr += sprintf_sf_param(ptr, sizeof(buf), pSFParam);
	if (pClfrRule) {
		ptr += snprintf(ptr, buf + sizeof(buf)-ptr, "\tClassifierRule={\n");
		ptr += sprintf_classifier_rule(ptr, buf + sizeof(buf) - ptr, pClfrRule);	
	}
	if (pPHSRule) {
		ptr += snprintf(ptr, buf + sizeof(buf)-ptr, "\tPHSRule={\n");
		ptr += sprintf_phs_rule(ptr, buf + sizeof(buf) - ptr, pPHSRule);
	}
	xprintf(SDK_INFO, "%s\t}\n}", buf);	

	hci->cmd_evt = htons(WIMAX_DSX_REQUEST);
	hci->length = 0;
	hci->data[hci->length++] = 1; /* DSC-REQ */
	hci->data[hci->length++] = 1; /* Use parameter in this hci */
	
	hci->data[hci->length++] = 145 + pSFParam->DL;
	dsx_oft = hci->length;
	hci->length += 3;

	add_sf_param(hci, pSFParam);
	
//	if (pClfrRule || pPHSRule) {
	if (CLFRDSCAction != DSC_NOP_CLASSIFIER || PHSDSCAction != DSC_NOP_PHS) {
		hci->data[hci->length++] = pSFParam->CSSpecification + 99; /* cst */
		cst_oft = hci->length;
		hci->length += 3;
		
		if (pClfrRule) {
			hci->data[hci->length++] = 1;
			hci->data[hci->length++] = 1;
			hci->data[hci->length++] = CLFRDSCAction;
			cst_len += 3;
			if (CLFRDSCAction == DSC_DELETE_CLASSIFIER) {
				hci->data[hci->length++] = 3;
				tmp_oft = hci->length;
				hci->length++;
				append_tlv(hci, 14, 2, pClfrRule->PacketClassifierRuleIndex);
				hci->data[tmp_oft] = hci->length - tmp_oft -1;
				cst_len += (2 + hci->data[tmp_oft]);
			} else 
				cst_len += add_one_classification_rule(hci, pSFParam, pClfrRule);
		}
		/* PHS Rule DSC */
		// in case of phs_deleteall, pPHSRule may not need.
		if (PHSDSCAction == DSC_DELETE_ALL_PHS) {
			hci->data[hci->length++] = 4;
			hci->data[hci->length++] = 1;
			hci->data[hci->length++] = PHSDSCAction;
			cst_len += 3 ;
			goto phs_done;
		} 
		if (pPHSRule) {
			hci->data[hci->length++] = 4;
			hci->data[hci->length++] = 1;
			hci->data[hci->length++] = PHSDSCAction;
			cst_len += 3;
			if (PHSDSCAction == DSC_DELETE_PHS) {
				hci->data[hci->length++] = 6;	// PHS rule filed
				tmp_oft = hci->length;
				hci->length++;
				append_tlv(hci, 1, 1, pPHSRule->PHSI);
				hci->data[tmp_oft] = hci->length - tmp_oft -1;
				cst_len += (2 + hci->data[tmp_oft]);
			} else 
				cst_len += add_one_phs_rule(hci, pSFParam, pPHSRule);
		}
phs_done:
		hci->data[cst_oft] = 0x82;
		hci->data[cst_oft+1] = (cst_len >> 8) & 0xff;
		hci->data[cst_oft+2] = cst_len & 0xff;
		/* sf_hci_put_length(hci->data + cst_oft, cst_len); */
	} 
	
	hci->data[dsx_oft] = 0x82;
	hci->data[dsx_oft+1] = ((hci->length - dsx_oft - 3) >> 8) & 0xff;
	hci->data[dsx_oft+2] = (hci->length - dsx_oft - 3) & 0xff;
	/* sf_hci_put_length(hci->data + dsx_oft, hci->length - dsx_oft - 1); */

	/* Prevent other clients from sending a DSx request */
	pthread_mutex_lock(&dev->wimax->dev_info.service_flow.dsxlock);

	xprintf(SDK_DBG, "DSC-REQ(len=%d)\n", hci->length);
	xprintf_hex(SDK_DBG, SDK_LOG_TITLE, hci->data, hci->length);
	
	sf_send_hci(dev_idx, hci);
	free(hci);

	gettimeofday(&tv, NULL);
	timeval2timespec(&tv, &ts);
	ts.tv_sec += DSX_REQUEST_TIMEOUT;
	ret = pthread_cond_timedwait(&dev->wimax->dev_info.service_flow.dsx_cond, &dev->wimax->dev_info.service_flow.dsxlock, &ts);
	if (ret) {
		xprintf(SDK_ERR, "CmdChangeSF Error (pthread_cond_timedwait=%d)\n", ret);
		pthread_mutex_unlock(&dev->wimax->dev_info.service_flow.dsxlock);
		cc = -1; //WIMAX_API_RET_FAILED;
		goto out;
	}
	//pthread_cond_wait(&dev->wimax->dev_info.service_flow.dsx_cond, &dev->wimax->dev_info.service_flow.dsxlock);
	
	cc = dev->wimax->dev_info.service_flow.last_sf_event.sf.cc;
	pthread_mutex_unlock(&dev->wimax->dev_info.service_flow.dsxlock);

	xprintf(SDK_INFO, "DSC-REQ: %s\n", wimax_stringify_dsx_cc(cc));

out:
	dm_put_dev(dev_idx);
	xfunc_out("ret=%d", cc);
 	return cc;
}


/* Delete a service flow */

WIMAX_SF_CC CmdDeleteSF(int dev_idx,
			WIMAX_SF_PARAM_P pSFParam)
{
#define timeval2timespec(tv,ts) \
		((ts)->tv_sec = (tv)->tv_sec, (ts)->tv_nsec = (tv)->tv_usec * 1000)
	
	device_t *dev;
	struct timeval tv;
	struct timespec ts;
	int ret = 0;

	struct hci *hci;
	WIMAX_SF_CC cc;	

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	if (dev->fsm.m_status != M_CONNECTED) {
		cc = WIMAX_SF_OTHER;
		goto out;
	}

	if (!pSFParam || !find_service_flow(dev_idx, pSFParam->SFID)) {
		cc = WIMAX_SF_NOT_FOUND;
		goto out;
	}

	xprintf(SDK_INFO, "CmdDeleteSF: SFID=%08x", pSFParam->SFID);

	hci = malloc(sizeof(struct hci) + 128);
	if (!hci) {
		cc = -1;//WIMAX_API_RET_FAILED;
		goto out;
	}

	hci->cmd_evt = htons(WIMAX_DSX_REQUEST);
	hci->length = 0;
	hci->data[hci->length++] = 2; /* DSD */
	hci->data[hci->length++] = 1; /* Use the following parameter */

	/* Start of TLV */
	hci->data[hci->length++] = 145 + pSFParam->DL;
	hci->data[hci->length++] = 1 + 1 + 4; /* T(1) + L (1) + V(4) */
	append_tlv(hci, 1, 4, pSFParam->SFID);

	pthread_mutex_lock(&dev->wimax->dev_info.service_flow.dsxlock);

	xprintf(SDK_DBG, "DSD-REQ(len=%d)\n", hci->length);
	xprintf_hex(SDK_DBG, SDK_LOG_TITLE, hci->data, hci->length);
	
	sf_send_hci(dev_idx, hci);
	free(hci);

	gettimeofday(&tv, NULL);
	timeval2timespec(&tv, &ts);
	ts.tv_sec += DSX_REQUEST_TIMEOUT;
	ret = pthread_cond_timedwait(&dev->wimax->dev_info.service_flow.dsx_cond, &dev->wimax->dev_info.service_flow.dsxlock, &ts);
	if (ret) {
		xprintf(SDK_ERR, "CmdDeleteSF Error (pthread_cond_timedwait=%d)\n", ret);
		pthread_mutex_unlock(&dev->wimax->dev_info.service_flow.dsxlock);
		cc = -1; //WIMAX_API_RET_FAILED;
		goto out;
	}
	//pthread_cond_wait(&dev->wimax->dev_info.service_flow.dsx_cond, &dev->wimax->dev_info.service_flow.dsxlock);

	cc = dev->wimax->dev_info.service_flow.last_sf_event.sf.cc;		
	pthread_mutex_unlock(&dev->wimax->dev_info.service_flow.dsxlock);

	xprintf(SDK_INFO, "DSD-REQ: %s\n", wimax_stringify_dsx_cc(cc));

out:
	dm_put_dev(dev_idx);
	xfunc_out("ret=%d", cc);
	return cc;
}


#define ARRAY_SIZE(x)	sizeof(x)/sizeof(x[0])

struct parse_data {
	char 		*name;
	unsigned int		offset;
	unsigned int 		size;
	int 		(*parser)(const struct parse_data *data, void *ptr,
				  int line, const char *value);
};


#define ENDMARKER	{.name = NULL, }

#define SFP(f)		#f, offsetof(struct wimax_sf_param, f), sizeof(((struct wimax_sf_param *)0)->f)
#define SFP_INT(f)	{SFP(f), parse_int}
#define SFP_STR(f)	{SFP(f), parse_str}
#define SFP_FUNC(f)	{SFP(f), parse_##f}
#define SFP_FUNC1(f, p) {SFP(f), p}

#define CLRP(f)		#f, offsetof(struct wimax_classification_rule, f), sizeof(((struct wimax_classification_rule *)0)->f)
#define CLR_INT(f)	{CLRP(f), parse_int}
#define CLR_STR(f)	{CLRP(f), parse_str}
#define CLR_FUNC(f)	{CLRP(f), parse_##f}
#define CLR_FUNC1(f, p) {CLRP(f), p}

#define PHSP(f)		#f, offsetof(struct wimax_phs_rule, f), sizeof(((struct wimax_phs_rule *)0)->f)
#define PHS_INT(f)	{PHSP(f), parse_int}
#define PHS_STR(f)	{PHSP(f), parse_str}

static int parse_int(const struct parse_data *data,
		     void *ptr, int line, const char *value)
{
	void *dst = ptr + data->offset;
	int32_t val = strtol(value, NULL, 0);

	switch (data->size) {
	case 1:
		*(uint8_t *)dst = (uint8_t) val;
		break;
	case 2:
		put_unaligned_u16(dst, (uint16_t)val);
		break;
	case 4:
		put_unaligned_u32(dst, val);
		break;
	default:
		return -1;
	}

	return 0;
}

static int hex2num(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}


static int hex2byte(const char *hex)
{
	int a, b;
	a = hex2num(*hex++);
	if (a < 0)
		return -1;
	b = hex2num(*hex++);
	if (b < 0)
		return -1;
	return (a << 4) | b;
}

int hexstr2bin(const char *hex, uint8_t *buf, unsigned int len)
{
        unsigned int i;
        int a;
        const char *ipos = hex;
        uint8_t *opos = buf;

        for (i = 0; i < len; i++) {
                a = hex2byte(ipos);
                if (a < 0)
                        return -1;
                *opos++ = a;
                ipos += 2;
        }
        return 0;
}


static char *parse_string(const char *value, unsigned int *len)
{
	if (*value == '"') {
		char *pos;
		value++;
		pos = strrchr(value, '"');
		if (pos == NULL || pos[1] != '\0')
			return NULL;
		*pos = '\0';
		*len = strlen(value);
		return strdup(value);
	} else {
		uint8_t *str;
		unsigned int hlen = strlen(value);
		if (hlen & 1)
			return NULL;
		*len = hlen / 2;
		str = malloc(*len);
		if (str == NULL)
			return NULL;
		if (hexstr2bin(value, str, *len)) {
			free(str);
			return NULL;
		}
		return (char *) str;
	}
}

static int parse_str(const struct parse_data *data,
		     void *ptr, int line, const char *value)
{
	unsigned int res_len;
	char *dst, *tmp;

	tmp = parse_string(value, &res_len);
	if (tmp == NULL) {
		printf("Line %d: failed to parse %s '%s'\n", line, data->name, value);
		return -1;
	}
	
	dst = (char *)(ptr + data->offset);
	memcpy(dst, tmp, data->size);
	free(tmp);
	return 0;
}

static int parse_TypeOfDataDeliveryServices(const struct parse_data *data,
					    void *ptr, int line, const char *value)
{
	uint8_t tdds = DATA_SERVICE_BE;
	struct wimax_sf_param *p = ptr;
	
	if (strcmp(value, "UGS") == 0)
		tdds = DATA_SERVICE_UGS;
	else if (strcmp(value, "RT-VR") == 0)
		tdds = DATA_SERVICE_RT_VR;
	else if (strcmp(value, "NRT-VR") == 0)
		tdds = DATA_SERVICE_NRT_VR;
	else if (strcmp(value, "BE") == 0)
		tdds = DATA_SERVICE_BE;
	else if (strcmp(value, "ERT-VR") == 0)
		tdds = DATA_SERVICE_ERT_VR;

	p->TypeOfDataDeliveryServices = tdds;

	return 0;
}

static int parse_ULGrantSchedulingType(const struct parse_data *data,
				       void *ptr, int line, const char *value)
{
	uint8_t sched = UL_SCHED_TYPE_BE; 
	struct wimax_sf_param *p = ptr;
	
	if (strcmp(value, "UGS") == 0)
		sched = UL_SCHED_TYPE_UGS;
	else if (strcmp(value, "nrtPS") == 0)
		sched = UL_SCHED_TYPE_nrtPS;
	else if (strcmp(value, "rtPS") == 0)
		sched = UL_SCHED_TYPE_rtPS;
	else if (strcmp(value, "BE") == 0)
		sched = UL_SCHED_TYPE_BE;
	else if (strcmp(value, "ertPS") == 0)
		sched = UL_SCHED_TYPE_ertPS;

	p->ULGrantSchedulingType = sched;

	return 0;
}

static const struct parse_data sf_param_table[] = {
	SFP_INT(DL),
	SFP_INT(SFID),
	SFP_INT(CID),
	SFP_STR(ServiceClassName),
	SFP_INT(QosParamSetType),
	SFP_INT(TrafficPriority),
	SFP_INT(MaxSustainedTrafficRate),
	SFP_INT(MaxTrafficBurst),
	SFP_INT(MinReservedTrafficRate),
	SFP_INT(MinTolerableRate),
	SFP_FUNC(ULGrantSchedulingType),
	SFP_INT(RequestTransmissionPolicy),
	SFP_INT(ToleratedJitter),
	SFP_INT(MaxLatency),
	SFP_INT(FixedLengthSDUIndicator),
	SFP_INT(SDUSize),
	SFP_INT(TargetSAID),
	SFP_INT(ARQEnable),
	SFP_INT(CSSpecification),
	SFP_FUNC(TypeOfDataDeliveryServices),
	SFP_INT(SDUInterArrivalInterval),
	SFP_INT(TimeBase),
	SFP_INT(PagingPreference),
	SFP_INT(TrafficPreferenceIndication),
	SFP_STR(GlobalServiceClassName),
	SFP_INT(SNFeedbackEnabled),
	SFP_INT(FSNSize),
	SFP_INT(UnsolicitedGrantInterval),
	SFP_INT(UnsolicitedPollingInterval),
	SFP_INT(PDUSNExtendedSubheaderForHARQReordering),
	SFP_INT(HARQServiceFlows),
	/*ENDMARKER,*/
};

static int parse_ipv4(const struct parse_data *data,
		     void *ptr, int line, const char *value)
{
	char *pos;
	struct ipv4_addr am;

	pos = strchr(value, '/');
	*pos++ = '\0';

	if (!inet_pton(AF_INET, value, &am.address) ||
	    !inet_pton(AF_INET, pos, &am.mask)) {
		printf("Lined %d: invalid IPv4 address/mask\n", line);
		return -1;
	}

	memcpy(ptr + data->offset, &am, sizeof(am));

	return 0;
}

static int in_ether(const char *bufp, uint8_t *mac)
{
	unsigned char *ptr;
	char c;
	const char *orig;
	int i;
	unsigned val;

	i = 0;
	orig = bufp;
	ptr = mac;

	while ((*bufp != '\0') && (i < 6)) {
		val = 0;
		c = *bufp++;
		if (isdigit(c))
			val = c - '0';
		else if (c >= 'a' && c <= 'f')
			val = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			val = c - 'A' + 10;
		else 
			return (-1);
	
		val <<= 4;
		c = *bufp;
		if (isdigit(c))
			val |= c - '0';
		else if (c >= 'a' && c <= 'f')
			val |= c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			val |= c - 'A' + 10;
		else if (c == ':' || c == 0)
			val >>= 4;
		else 
			return (-1);
	
		if (c != 0)
			bufp++;
		*ptr++ = (unsigned char) (val & 0377);
		i++;

		/* We might get a semicolon here - not required. */
		if (*bufp == ':') {
			if (i == 6) 
				printf("in_ether(%s): trailing : ignored!\n", orig);
			bufp++;
		}
	}

	/* That's it.  Any trailing junk? */
	if ((i == 6) && (*bufp != '\0')) 
		return -1;

	return 0;
}


static int parse_ether(const struct parse_data *data,
		     void *ptr, int line, const char *value)
{
	char *mask;
	int ret;

	mask = strchr(value, '/');
	if (mask == NULL)
		return -1;

	*mask++ = '\0';
	
	ret = in_ether(value, ptr + data->offset);
	if (ret < 0)
		return ret;

	ret = in_ether(mask, ptr + data->offset + 6);
	return ret;
}


static const struct parse_data classifier_table[] = {
	CLR_INT(ClassifierRulePriority),
	CLR_INT(PacketClassifierRuleIndex),
	
	CLR_INT(IPTypeOfService.low),
	CLR_INT(IPTypeOfService.high),
	CLR_INT(IPTypeOfService.mask),

	CLR_INT(Protocol),
	
	CLR_FUNC1(IPv4MaskedSourceAddress, parse_ipv4),
	CLR_FUNC1(IPv4MaskedDestAddress, parse_ipv4),
	
	CLR_INT(ProtocolSourcePort.low),
	CLR_INT(ProtocolSourcePort.high),
	
	CLR_INT(ProtocolDestPort.low),
	CLR_INT(ProtocolDestPort.high),

	CLR_FUNC1(EthernetDestMACAddress, parse_ether),
	CLR_FUNC1(EthernetSourceMACAddress, parse_ether),

	CLR_INT(EtherType.type),
	CLR_INT(EtherType.eprot1),
	CLR_INT(EtherType.eprot2),
	
	CLR_INT(IEEE802_1D_UserPriority.low),
	CLR_INT(IEEE802_1D_UserPriority.high),
	
	CLR_INT(IEEE802_1Q_VLANID),

	CLR_INT(AssociatedPHSI), 
	/*ENDMARKER,*/
};

static const struct parse_data phs_table[] = {
	PHS_INT(PHSI),
	PHS_INT(PHSS),
	PHS_STR(PHSM),
	PHS_STR(PHSF),
	PHS_INT(PHSV),
	/*ENDMARKER,*/
};

static char *get_line(char *s, int size, FILE *stream, int *line,
		      char **_pos)
{
	char *pos, *end, *sstart;

	while (fgets(s, size, stream)) {
		(*line)++;
		s[size - 1] = '\0';
		pos = s;

		/* Skip white space from the beginning of line. */
		while (*pos == ' ' || *pos == '\t' || *pos == '\r')
			pos++;

		/* Skip comment lines and empty lines */
		if (*pos == '#' || *pos == '\n' || *pos == '\0')
			continue;

		/*
		 * Remove # comments unless they are within a double quoted
		 * string.
		 */
		sstart = strchr(pos, '"');
		if (sstart)
			sstart = strrchr(sstart + 1, '"');
		if (!sstart)
			sstart = pos;
		end = strchr(sstart, '#');
		if (end)
			*end-- = '\0';
		else
			end = pos + strlen(pos) - 1;

		/* Remove trailing white space. */
		while (end > pos &&
		       (*end == '\n' || *end == ' ' || *end == '\t' ||
			*end == '\r'))
			*end-- = '\0';

		if (*pos == '\0')
			continue;

		if (_pos)
			*_pos = pos;
		return pos;
	}

	if (_pos)
		*_pos = NULL;
	return NULL;
}

int parse_sf_param(struct wimax_sf_param *sfp,
		   const char *var, const char *value, int line)
{
	int i, ret = 0;
	
	if (sfp == NULL || var == NULL || value == NULL)
		return -1;

	for (i = 0; i < ARRAY_SIZE(sf_param_table); i++) {
		const struct parse_data *field = &sf_param_table[i];
		if (strcasecmp(var, field->name) != 0)
			continue;

		if (field->parser(field, sfp, line, value)) {
			if (line) {
				printf("Line %d: failed to parse %s '%s'\n",
				       line, var, value);
			}
			ret = -1;
		}
		break;
	}
	if (i == ARRAY_SIZE(sf_param_table)) {
		if (line) {
			printf("Line %d: unknown service flow parameter '%s'\n",
			       line, var);
		}
		ret = -1;
	}

	return ret;
}



int parse_classifier(struct wimax_classification_rule *c,
		     const char *var, const char *value, int line)
{
	int i, ret = 0;
	
	if (c == NULL || var == NULL || value == NULL)
		return -1;

	for (i = 0; i < ARRAY_SIZE(classifier_table); i++) {
		const struct parse_data *field = &classifier_table[i];
		if (strcasecmp(var, field->name) != 0)
			continue;

		if (field->parser(field, c, line, value)) {
			if (line) {
				printf("Line %d: failed to parse %s '%s'\n",
				       line, var, value);
			}
			ret = -1;
		}
		break;
	}
	if (i == ARRAY_SIZE(classifier_table)) {
		if (line) {
			printf("Line %d: unknown classifier rule '%s'\n",
			       line, var);
		}
		ret = -1;
	}

	return ret;	
}

static int read_classifier(struct wimax_classification_rule *c, FILE *f, int *line)
{
	int errors = 0, end = 0;
	char buf[256], *pos, *pos2;

	*c = classifier_rule_init;

	while (get_line(buf, sizeof(buf), f, line, &pos)) {
		if (strcmp(pos, "}") == 0) {
			end = 1;
			break;
		}

		pos2 = strchr(pos, '=');
		if (pos2 == NULL) {
			printf("Line %d: invalid classifier rule line '%s'\n", *line, pos);
			errors++;
			continue;
		}
		*pos2++ = '\0';
		if (*pos2 == '"') {
			if (strchr(pos2+1, '"') == NULL) {
				printf("Line %d: unterminated string '%s'\n", *line, pos2);
				errors++;
				continue;
			}
		}

		if (parse_classifier(c, pos, pos2, *line) < 0) {
			errors++;
		}			
	}

	if (!end) {
		printf("Line %d: classifier rule block was not terminated properly\n", *line);
		errors++;
	}

	return (errors? -1: 0);
}



int parse_phs(struct wimax_phs_rule *p,
		     const char *var, const char *value, int line)
{
	int i, ret = 0;
	
	if (p == NULL || var == NULL || value == NULL)
		return -1;

	for (i = 0; i < ARRAY_SIZE(phs_table); i++) {
		const struct parse_data *field = &phs_table[i];
		if (strcasecmp(var, field->name) != 0)
			continue;
		if (field->parser(field, p, line, value)) {
			if (line) {
				printf("Line %d: failed to parse %s '%s'\n",
				       line, var, value);
			}
			ret = -1;
		}
		break;
	}
	if (i == ARRAY_SIZE(phs_table)) {
		if (line) {
			printf("Line %d: unknown phs rule '%s'\n",
			       line, var);
		}
		ret = -1;
	}

	return ret;
}

static int read_phs(struct wimax_phs_rule *p, FILE *f, int *line)
{
	int errors = 0, end = 0;
	char buf[256], *pos, *pos2;

	*p = phs_rule_init;

	while (get_line(buf, sizeof(buf), f, line, &pos)) {
		if (strcmp(pos, "}") == 0) {
			end = 1;
			break;
		}

		pos2 = strchr(pos, '=');
		if (pos2 == NULL) {
			printf("Line %d: invalid classifier rule line '%s'\n", *line, pos);
			errors++;
			continue;
		}
		*pos2++ = '\0';
		if (*pos2 == '"') {
			if (strchr(pos2+1, '"') == NULL) {
				printf("Line %d: unterminated string '%s'\n", *line, pos2);
				errors++;
				continue;
			}
		}
		
		if (parse_phs(p, pos, pos2, *line) < 0) {
			errors++;
		}			
	}

	if (!end) {
		printf("Line %d: classifier rule block was not terminated properly\n", *line);
		errors++;
	}

	return (errors? -1: 0);
	return -1; 
}


static int read_service_flow(struct wimax_sf_param *param, FILE *f, int *line)
{
	int errors = 0, end = 0;
	char buf[256], *pos, *pos2;

	*param = sf_param_init;

	while (get_line(buf, sizeof(buf), f, line, &pos)) {
		if (strcmp(pos, "}") == 0) {
			end = 1;
			break;
		} else {
			pos2 = strchr(pos, '=');
			if (pos2 == NULL) {
				printf("Line %d: invalid sf parameter line '%s'\n", *line, pos);
				errors++;
				continue;
			}
			*pos2++ = '\0';
			if (*pos2 == '"') {
				if (strchr(pos2+1, '"') == NULL) {
					printf("Line %d: unterminated string '%s'\n", *line, pos2);
					errors++;
					continue;
				}
			}

			if (parse_sf_param(param, pos, pos2, *line) < 0) {
				errors++;
			}
		}
	}

	if (!end) {
		printf("Line %d: service flow block was not terminated properly\n", *line);
		errors++;
	}

	return errors? -1 : 0;
}

int load_sf(const char *name,
	    WIMAX_SF_PARAM **sfp,
	    WIMAX_CLFR_RULE **classifier,
	    WIMAX_PHS_RULE **phs)
{
	FILE *f;
	char buf[1024], *pos;
	int line = 0, errors = 0;
	WIMAX_SF_PARAM *tsfp = *sfp;
	WIMAX_CLFR_RULE *tclr = *classifier;
	WIMAX_PHS_RULE *tphs = *phs;

	*sfp = NULL;
	*classifier = NULL;
	*phs = NULL;

	f = fopen(name, "r");
	if (f == NULL)
		return -1;

	while (get_line(buf, sizeof(buf), f, &line, &pos)) {
		if (strcmp(pos, "service_flow={") == 0) {
			if (read_service_flow(tsfp, f, &line) < 0) {
				*sfp = NULL;
				errors++;
				continue;
			}
			*sfp = tsfp;
		} else if (strcmp(pos, "classifier={") == 0) {
			if (read_classifier(tclr, f, &line) < 0) {
				*classifier = NULL;
				errors++;
				continue;
			}
			*classifier = tclr;
		} else if (strcmp(pos, "phs={") == 0) {
			if (read_phs(tphs, f, &line) < 0) {
				*phs = NULL;
				errors++;
				continue;
			}
			*phs = tphs;
		}
	}
	fclose(f);

	return (errors > 0)? -1: 0;
}

void sf_init(int dev_idx)
{
	device_t *dev;

	xfunc_in();

	if (!(dev = dm_get_dev(dev_idx)))
		return;

	pthread_mutex_init(&dev->wimax->dev_info.service_flow.sfreadlock, NULL);
	pthread_mutex_init(&dev->wimax->dev_info.service_flow.sfwritelock, NULL);
	pthread_mutex_init(&dev->wimax->dev_info.service_flow.dsxlock, NULL);
	pthread_cond_init(&dev->wimax->dev_info.service_flow.dsx_cond, NULL);

	dm_put_dev(dev_idx);
	xfunc_out();
}

void sf_deinit(int dev_idx)
{
	device_t *dev;

	xfunc_in();

	if (!(dev = dm_get_dev(dev_idx)))
		return;

	pthread_mutex_destroy(&dev->wimax->dev_info.service_flow.sfreadlock);
	pthread_mutex_destroy(&dev->wimax->dev_info.service_flow.sfwritelock);
	pthread_mutex_destroy(&dev->wimax->dev_info.service_flow.dsxlock);
	pthread_cond_destroy(&dev->wimax->dev_info.service_flow.dsx_cond);

	dm_put_dev(dev_idx);
	xfunc_out();
}

