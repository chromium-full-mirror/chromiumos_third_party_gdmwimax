// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(CONFIG_DM_INTERFACE)
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <wchar.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15)
#include <linux/types.h>
#endif
#include <linux/if_packet.h>
#include <assert.h>
#include <pthread.h>
#include <netinet/in.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "cm.h"
#include "exitcb.h"

#if defined(CONFIG_DM_NET_DEVICE)
#define ETHERNET_DEVICE		CONFIG_DM_NET_DEVICE
#else
#define ETHERNET_DEVICE		"eth0"
#endif
#define DM_TOOL_PORT		9801

//#define DM_IF_DEBUG

typedef struct dm_sub_s {
	unsigned short code;
	unsigned short op;
	unsigned short result;
} __attribute__((packed)) dm_sub_t;

/*HCI*/
#define WIMAX_MON_TOOL_REQUEST			0x0326
#define WIMAX_MON_TOOL_REPORT			0x8327
	#define MANUAL_CONNECT				0x0014
	#define CODE_CONNECT				0x0015
	#define CODE_DISCONNECT				0x0016
	#define OP_REQUEST					0x0000
	#define OP_RESPONSE					0x0001
	#define RET_SUCCESS					0x0000
	#define RET_FAILURE					0x0001

extern int get_first_odev(void);
extern APIHAND cm_api_handle;
static pthread_t dm_thread;
static int dm_listener;

int dmif_init(void);
int dmif_deinit(void);

static int get_ip(const char *dev_name, char *ip)
{
#define sizeof_sa_sin_port	2
#define inaddrr(x) (*(struct in_addr *) &ifr.x[sizeof_sa_sin_port])
	struct ifreq ifr;
	int fd;
	int ret = 0;

	fd = socket(PF_PACKET, SOCK_DGRAM, IPPROTO_IP);
	if (fd < 0) {
		cm_printf("Socket error %s(%d)\n", strerror(errno), errno);
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, dev_name, IFNAMSIZ);
	if ((ret = ioctl(fd, SIOCGIFADDR, &ifr)) < 0) {
		cm_printf("ioctl SIOCGIFFLAGS %s(%d)\n", strerror(errno), errno);
		goto out;
	}
	strcpy(ip, inet_ntoa(inaddrr(ifr_addr.sa_data)));
	
	#if defined(DM_IF_DEBUG)
	cm_printf("ip: %s\n", ip);
	#endif

out:
	close(fd);
	return ret;
}

int send_dm_tool(void *data, int len)
{
	cm_common_conf_t *pconf = &cm_common_conf;
	#if defined(DM_IF_DEBUG)
	hci_t *hci = (hci_t *) data;
	#endif
	int cfd = pconf->dm_interface_cfd;
	int ret;

	if (cfd <= 0) {
		cm_printf("Invalied DM Tool's file descriptor\n");
		return 0;
	}

	ret = send(cfd, data, len, 0);
	#if defined(DM_IF_DEBUG)
	cm_printf("Sent: 0x%04X, %d(%d)\n", _B2H(hci->cmd_evt), _B2H(hci->length), ret);
	#endif
	return ret;
}

int dm_ind_connetion(int connect, int success)
{
	char buf[HCI_MAX_PACKET] __attribute__((aligned(4)));
	hci_t *hci = (hci_t *) buf;
	dm_sub_t *dsub = (dm_sub_t *) hci->data;

	hci->cmd_evt = _H2B(WIMAX_MON_TOOL_REPORT);
	hci->length = _H2B(sizeof(dm_sub_t));

	if (connect)
		dsub->code = _H2B(CODE_CONNECT);
	else
		dsub->code = _H2B(CODE_DISCONNECT);

	dsub->op = _H2B(OP_RESPONSE);

	if (success)
		dsub->result = _H2B(RET_SUCCESS);
	else
		dsub->result = _H2B(RET_FAILURE);

	return send_dm_tool(buf, HCI_HEADER_SIZE+sizeof(dm_sub_t));
}

static int dm_handle_host_cmd(GDEV_ID_P pID, hci_t *hci)
{
	dm_sub_t *dsub = (dm_sub_t *) hci->data;
	WIMAX_API_NSP_INFO NSPInfo;
	int list_cnt = 1;
	int ret = 0;
	
	if (_B2H(hci->cmd_evt) == WIMAX_MON_TOOL_REQUEST) {
		switch (_B2H(dsub->code)) {
			case CODE_CONNECT:			
				if (GAPI_GetNetworkList(pID, &NSPInfo, (UINT32 *) &list_cnt)
					!= GCT_API_RET_SUCCESS) {
					cm_eprintf("Get network list failure\n");
					ret = -1;
				}
				else if (!list_cnt) {
					cm_eprintf("Network list is 0\n");
					ret = -1;
				}
				else if (GAPI_CmdConnectToNetwork(pID, NSPInfo.NSPName, 0)
					!= GCT_API_RET_SUCCESS) {
					cm_eprintf("Connect failure\n");
					ret = -1;
				}

				if (ret < 0)
					dm_ind_connetion(1, 0);
				break;
			case CODE_DISCONNECT:
				if (GAPI_CmdDisconnectFromNetwork(pID) != GCT_API_RET_SUCCESS) {
					cm_eprintf("Disconnect failure\n");
					dm_ind_connetion(0, 0);
				}
				break;
			case MANUAL_CONNECT:
				if (_B2H(dsub->op) == 0)
					cm_enable_auto_connet(1, get_first_odev());
				else
					cm_enable_auto_connet(0, 0);
				break;
		}
		ret = 1;
	}

	return ret;
}

static int accept_dm_hci(int sfd)
{
	cm_common_conf_t *pconf = &cm_common_conf;
	char buf[HCI_MAX_PACKET] __attribute__((aligned(4)));
	hci_t *hci = (hci_t *) buf;
	GDEV_ID ID;
	struct sockaddr_in	saddr_c;
	int cfd, ret, len, hci_len;
	unsigned int ulen = sizeof(struct sockaddr);

	cm_printf("Wait for DM tool to be accepted...\n");
	if ((pconf->dm_interface_cfd = cfd = 
		accept(sfd, (struct sockaddr *)&saddr_c, &ulen)) < 0) {
		cm_printf("accept error %s(%d)\n", strerror(errno), errno);
		return -1;
	}
	cm_printf("Accepted %s\n", inet_ntoa(saddr_c.sin_addr));

	ID.apiHandle = cm_api_handle;

	while (1) {
		if ((len = ret = recv(cfd, buf, sizeof(buf), 0)) < 0) {
			if (errno == ECONNRESET) {
				ret = 0;
				break;
			}
			cm_printf("read error %s(%d)\n", strerror(errno), errno);
			break;
		}
		if (!ret)
			break;

		if (ret < HCI_HEADER_SIZE) {
			cm_printf("Invalid length(%d)\n", ret);
			continue;
		}

		if (!(ID.deviceIndex = get_first_odev())) {
			cm_printf("There is no any attached device!!\n");
			continue;
		}

		hci = (hci_t *) buf;
		while (len >= HCI_HEADER_SIZE) {
			hci_len = _B2H(hci->length);

			#if defined(DM_IF_DEBUG)
			cm_printf("DM: 0x%04X(%d), len=%d\n", _B2H(hci->cmd_evt), hci_len, len);
			#endif

			if (!dm_handle_host_cmd(&ID, hci)) {
				if (GAPI_WriteHCIPacket(&ID, (char *) hci, HCI_HEADER_SIZE+hci_len)
					!= GCT_API_RET_SUCCESS) {
					cm_eprintf("Write HCI Failed\n");
					goto out;
				}
			}

			hci = (hci_t *) ((char *) hci + HCI_HEADER_SIZE + hci_len);
			len -= HCI_HEADER_SIZE + hci_len;
		}

		if (len > 0)
			cm_printf("Unhandled length is %d\n", len);
	}
out:
	pconf->dm_interface_cfd = -1;
	close(cfd);
	return ret;
}

static void *dm_receiver_thread(void *data)
{
	const char *dev_name = (const char *) data;
	struct sockaddr_in	saddr_s;
	char ip[32];
	int port = DM_TOOL_PORT;
	int ret;

	if (get_ip(dev_name, ip) < 0)
		return NULL;

	memset(&saddr_s, 0, sizeof(saddr_s));

	saddr_s.sin_family = AF_INET;	/* AF_INET, PF_INET are same */
	if (inet_aton(ip, &saddr_s.sin_addr) < 0) {
		cm_printf("Fail: inet_aton %s\n", ip);
		ret = -1;
		goto out;
	}
	saddr_s.sin_port = htons(port);

	if ((dm_listener = ret = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0) {
		cm_printf("DM socket error %s(%d)\n", strerror(errno), errno);
		goto out;
	}
	if ((ret = bind(dm_listener, (struct sockaddr *) &saddr_s, 
		sizeof(struct sockaddr_in))) < 0) {
		cm_printf("DM bind error %s(%d)\n", strerror(errno), errno);
		goto out;
	}
	listen(dm_listener, 1);

	while (1) {
		if ((ret = accept_dm_hci(dm_listener)) < 0) {
			cm_printf("accept_dm_hci error\n");
			break;
		}
	}
out:
	if (dm_listener > 0) {
		close(dm_listener);
		dm_listener = 0;
	}
	return NULL;
}

static int dm_receiver_create(void)
{
	pthread_create(&dm_thread, NULL, dm_receiver_thread, ETHERNET_DEVICE);
	return 0;
}

static int dm_stop_bmode(void)
{
	const u8 stop_packet[] = {0x00, 0x04, 0x00, 0x02, 0xAD, 0xBC, 0x7E};
	GDEV_ID ID;
	char buf[HCI_MAX_PACKET] __attribute__((aligned(4)));
	hci_t *hci = (hci_t *) buf;
	int ret = 0, len;

	ID.apiHandle = cm_api_handle;
	if (!(ID.deviceIndex = get_first_odev())) {
		cm_printf("There is no any attached device!!\n");
		return -1;
	}

	len = sizeof(stop_packet);
	hci->cmd_evt = _H2B(WIMAX_DM_CMD);
	hci->length = _H2B(len);
	memcpy(hci->data, stop_packet, len);

	if (GAPI_WriteHCIPacket(&ID, buf, HCI_HEADER_SIZE+len) != GCT_API_RET_SUCCESS) {
		cm_printf("Write HCI Failed\n");
		ret = -1;
	}
	return ret;
}

static int dm_receiver_delete(void)
{
	pthread_t thread;
	int ret = 0;

	if ((thread = dm_thread)) {
		dm_thread = (pthread_t) NULL;
		pthread_cancel(thread);
		pthread_join(thread, NULL);
		if (dm_listener > 0) {
			close(dm_listener);
			dm_listener = 0;
		}

		ret = dm_stop_bmode();
	}

	return ret;
}

static void cb_dm_deinit(int signal/*, void *info*/)
{
	dmif_deinit();
}

int dmif_init(void)
{
	int ret;

	register_exit_cb(cb_dm_deinit);

	ret = dm_receiver_create();
	return ret;
}

int dmif_deinit(void)
{
	int ret;

	unregister_exit_cb(cb_dm_deinit);

	ret = dm_receiver_delete();
	return ret;
}
#endif

