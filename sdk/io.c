// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "global.h"
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "error.h"
#include "wimax.h"
#include "wm_ioctl.h"
#include "io.h"
#include "device.h"
#include "hci.h"
#include "log.h"

int get_iface_index(int fd, const char *device)
{
	struct ifreq ifr;  

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, device, IFNAMSIZ);
 	ifr.ifr_name[strlen(ifr.ifr_name)] = '\0';

    	if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
		xprintf(SDK_STD_ERR, "SIOCGIFINDEX(%d)\n", fd);
        	return sdk_set_errno(ERR_STD);
	}
	
	return ifr.ifr_ifindex;
}

static int net_open(device_t *dev)
{
	int fd;
	char *ifname = dev->name;
	int ifindex;

	fd = socket(PF_PACKET, SOCK_DGRAM, IPPROTO_IP);
	if (fd < 0) {
		xprintf(SDK_STD_ERR, "socket\n");
		return sdk_set_errno(ERR_STD);
	}

	ifindex = get_iface_index(fd, ifname);

	if (ifindex < 0) {
		xprintf(SDK_ERR, "Cannot find the interface\n");
		return sdk_set_errno(ERR_ENV);
	}

	xprintf(SDK_DBG, "net open, fd=%d, ifindex=%d, name=%s\n", fd, ifindex, ifname);
	dev->ifindex = ifindex;
	dev->net_fd = fd;
	return 0;
}

int net_updown(int dev_idx, bool up)
{
	device_t *dev;
	struct ifreq ifr;
	int fd;
	int ret = 0;

	xfunc_in();

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;
	
	if (!dev->inserted) {
		xprintf(SDK_DBG, "device(%s) has been removed\n", dev->name);
		goto out;
	}

	fd = dev->net_fd;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, dev->name, IFNAMSIZ);
	if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
		xprintf(SDK_STD_ERR, "ioctl SIOCGIFFLAGS(%d) %s\n", fd, up ? "Up" : "Down");
		ret = sdk_set_errno(ERR_STD);
		goto out;
	}

	if (up)
		ifr.ifr_flags |= (IFF_UP|IFF_RUNNING);
	else
		ifr.ifr_flags &= ~IFF_UP;
	
	if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
		xprintf(SDK_STD_ERR, "ioctl SIOCSIFFLAGS(%d) %s\n", fd, up ? "Up" : "Down");
		ret = sdk_set_errno(ERR_STD);
	}
out:
	dm_put_dev(dev_idx);
	xfunc_out();
	return ret;
}

#define RESOLV_CONF_PATH    "/etc/resolv.conf"

unsigned int net_get_dns_info_mtime(void)
{
	const char *file = RESOLV_CONF_PATH;
	struct stat stat_buf;

	if (stat(RESOLV_CONF_PATH, &stat_buf) < 0) {
		if (errno == ENOENT)
			xprintf(SDK_DBG, "%s has been removed!\n", RESOLV_CONF_PATH);
		else {
			xprintf(SDK_STD_ERR, "stat(%s)\n", file);
			return 0;
		}
	}
	xprintf(SDK_DBG, "%s: st_mtime=0x%08X\n", file, (int)stat_buf.st_mtime);
	return stat_buf.st_mtime;
}

int net_get_dns_info(dns_info_t *dns_info)
{
	FILE *file;
	int ret, i, svr_cnt, df;
	char name[16], domain[128];

	xfunc_in();

	file = fopen(RESOLV_CONF_PATH, "r");
	if (!file) {
		xprintf(SDK_ERR, "fopen fail = %s(%d)\n", strerror(errno), errno);
		return -errno;
	}

	i = svr_cnt = df = 0;
	while ((ret = fscanf(file, "%s %s", name, domain)) != EOF) {
		if (ret < 2) {
			xprintf(SDK_STD_ERR, "fscanf ret=%d\n", ret);
			ret = -1;
			break;
		}

		if (svr_cnt < MAX_DNS_SERVERS && !strcmp(name, "nameserver")) {
			strcpy(dns_info->dns_svr[svr_cnt++], domain);
			xprintf(SDK_INFO, "dns_svr=%s\n", domain);
		}
		else if (!strcmp(name, "search")) {
			strcpy(dns_info->domain, domain);
			xprintf(SDK_INFO, "domain=%s\n", domain);
			df++;
		}
	}

	fclose(file);

	xprintf(SDK_INFO, "svr_cnt=%d, df=%d\n", svr_cnt, df);

	if (svr_cnt && df)
		ret = 0;
	else
		ret = -1;
	xfunc_out("ret=%d", ret);
	return ret;
}

int net_get_dhcp_info(int dev_idx, dhcp_info_t *dhcp_info)
{
#define sizeof_sa_sin_port	2
#define p_inaddrr(x) (&ifr.x[sizeof_sa_sin_port])
	device_t *dev;
	struct ifreq ifr;
	struct in_addr inaddr;
	char *dev_name;
	int fd;
	int ret = 0;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	dev_name = dev->name;
	
	if (!dev->inserted) {
		xprintf(SDK_ERR, "device(%s) has been removed\n", dev_name);
		goto out;
	}

	fd = dev->net_fd;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, dev_name, IFNAMSIZ);
	if (ioctl(fd, SIOCGIFADDR, &ifr) < 0) {
		xprintf(SDK_DBG, "ioctl SIOCGIFFLAGS(%d)\n", fd);
		ret = sdk_set_errno(ERR_STD);
		goto out;
	}
	memcpy(&inaddr, p_inaddrr(ifr_addr.sa_data), sizeof(inaddr));
	strcpy(dhcp_info->ip, inet_ntoa(inaddr));
	xprintf(SDK_INFO, "ip: %s\n", dhcp_info->ip);


	ret = ioctl(fd, SIOCGIFNETMASK, &ifr);
	memcpy(&inaddr, p_inaddrr(ifr_addr.sa_data), sizeof(inaddr));

	if (ret < 0) {
		xprintf(SDK_STD_ERR, "ioctl SIOCGIFNETMASK(%d): netmask(%s)\n",
			fd, inet_ntoa(inaddr));
		ret = sdk_set_errno(ERR_STD);
		goto out;
	}

	strcpy(dhcp_info->netmask, inet_ntoa(inaddr));
	xprintf(SDK_INFO, "netmask: %s\n", dhcp_info->netmask);

out:
	xprintf(SDK_DBG, "net_get_dhcp_info: ret=%d\n", ret);
	dm_put_dev(dev_idx);
	return ret;
}

int net_ioctl(int dev_idx, wm_req_t *wmr, int cmd)
{
	device_t *dev;
	int ret = 0;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	wmr->cmd = cmd;
	xfunc_in("cmd=0x%X", wmr->cmd);

	if (dev->inserted && ioctl(dev->net_fd, SIOCWMIOCTL, wmr) < 0) {
		xprintf(SDK_STD_ERR, "[%d] cmd=%d\n", dev_idx, wmr->cmd);
		ret = sdk_set_errno(ERR_STD);
	}

	xfunc_out("ret=%d", ret);
	dm_put_dev(dev_idx);
	return ret;
}

int net_ioctl_data(int dev_idx, bool get, u16 data_id, void *buf, int size)
{
	device_t *dev;
	wm_req_t wmr;
	int ret = -1, cmd;

	xfunc_in("dev=%d", dev_idx);

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	if (data_id >= SIOC_DATA_MAX) {
		xprintf(SDK_ERR, "invalid data_id=%d\n", data_id);
		goto out;
	}
	if (!buf) {
		xprintf(SDK_ERR, "buffer is null\n");
		goto out;
	}
	if (size <= 0) {
		xprintf(SDK_ERR, "invalid size=%d\n", size);
		goto out;
	}

	cmd = get ? SIOCG_DATA : SIOCS_DATA;
	strncpy(wmr.ifr_name, dev->name, sizeof(wmr.ifr_name)-1);
	wmr.data_id = data_id;
	wmr.data.buf = buf;
	wmr.data.size = size;

	xprintf(SDK_INFO, "net ioctl: cmd=%s, id=%d\n", get ? "Get" : "Set", data_id);
	if ((ret = net_ioctl(dev_idx, &wmr, cmd)) < 0) {
		xprintf(SDK_STD_ERR, "[%d] cmd(0x%04x) data id=%d\n", dev_idx, cmd, data_id);
		sdk_set_errno(ERR_STD);
	}
out:
	dm_put_dev(dev_idx);
	xfunc_out("ret=%d", ret);
	return ret < 0 ? -1 : wmr.data.size;
}

static void net_close(device_t *dev)
{
	xfunc_in();
	if (dev->net_fd > 0) {
		xprintf(SDK_DBG, "close(%d)\n", dev->net_fd);
		close(dev->net_fd);
		dev->net_fd = 0;
	}
	xfunc_out();
}

static int io_open(device_t *dev)
{
	int idx, ret;

	sscanf(dev->name, "wm%d", &idx);
	ret = nl_open(&dev->io_nl, NETLINK_WIMAX, dev->ifindex, idx);

	if (ret < 0) {
		xprintf(SDK_ERR, "Open net-link failure\n");
		return sdk_set_errno(ERR_ENV);
	}

	xprintf(SDK_DBG, "io open\n");
	return ret;
}

static void io_close(device_t *dev)
{
	int ret;

	xfunc_in("dev=%s", dev->name);
	ret = nl_close(&dev->io_nl);
	xfunc_out("ret=%d", ret);
}

int io_send(int dev_idx, void *buf, int len)
{
	device_t *dev;
	int ret;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	ret = nl_send(&dev->io_nl, 0, buf, len);

	if (ret < 0) {
		if (!dev->inserted)
			xprintf(SDK_DBG, "device[%d] has been removed\n", dev_idx);
		else {
			xprintf(SDK_STD_ERR, "[%d] nl_send\n", dev_idx);
			sdk_set_errno(ERR_STD);
		}
	}
	else if (!ret) {
		xprintf(SDK_ERR, "nl_send length is 0.\n");
		ret = -1;
	}
	else
		ret = len;

	dm_put_dev(dev_idx);
	return ret;
}

static int io_recv(device_t *dev, char *buf, int len, int flags)
{
	hnetlink_t *hnl = &dev->io_nl;
	int ret;

	ret = nl_recv(hnl, buf, len, flags);

	if (ret < 0) {
		if (!dev->inserted) {
			xprintf(SDK_DBG, "device(%s) has been removed\n", dev->name);
			return -1;
		}
		xprintf(SDK_STD_ERR, "nl_recv\n");
		sdk_set_errno(ERR_STD);
	}
	else if (!ret) {
		xprintf(SDK_ERR, "Received length is 0.\n");
		ret = -1;
	}

	return ret;
}

void *io_receiver_thread(void *data)
{
	int dev_idx = (int) data;
	device_t *dev;
	hci_msg_t *msg;
	int fd, len = 0;

	if (!(dev = dm_get_dev(dev_idx)))
		goto end;

	xfunc_in("name=%s", dev->name);

	if ((fd = net_open(dev)) < 0)
		goto out;

	if ((fd = io_open(dev)) < 0)
		goto out;

	pthread_sem_signal(&dev->sync_lock);

	while (1) {
		msg = (hci_msg_t *) sdk_malloc(sizeof(hci_msg_t));

		len = io_recv(dev, msg->buf, sizeof(msg->buf), 0);
		xprintf(SDK_DBG, "io_recv: %02x%02x %02x%02x\n",
			(u8)msg->buf[0], (u8)msg->buf[1], (u8)msg->buf[2], (u8)msg->buf[3]);
		if (len < 0) {
			if (errno == ENOBUFS) {
				/*
					If rx-netlink is full, ENOBUFS is returned,
					thus, we will try to receive non-overflowed data,
					but overflowed data cannot be received.
				*/
				xprintf(SDK_NOTICE, "ENOBUFS has been returned and try to receive again!");
				continue;
			}
			sdk_free(msg);
			break;
		}

		msg->len = len;
		hci_send_msg(dev_idx, msg);
		xprintf(SDK_DBG, "[%d] msg sent, len=%d\n", dev_idx, len);
	}

out:
	io_close(dev);
	net_close(dev);
	xfunc_out("name=%s", dev->name);
	dm_put_dev(dev_idx);
end:
	#if 0
	if (!len)
		pthread_sem_signal(&dev->sync_lock);
	#endif
	return NULL;
}

int io_receiver_create(int dev_idx)
{
	device_t *dev;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;
	
	xfunc_in("dev=%d", dev_idx);

	pthread_create(&dev->ind_thr, NULL, io_receiver_thread, (void *)dev_idx);

	pthread_sem_wait(&dev->sync_lock);

	xfunc_out();
	dm_put_dev(dev_idx);
	return dev->io_nl.fd ? 0 : -1;
}

int io_receiver_delete(int dev_idx)
{
	device_t *dev;
	pthread_t thread;
	int ret;

	if (!(dev = dm_get_dev(dev_idx)))
		return -1;

	xfunc_in("name=%s", dev->name);

	if ((thread = dev->ind_thr)) {
		dev->ind_thr = (pthread_t) NULL;
		pthread_cancel(thread);
		xprintf(SDK_DBG, "+IO(%d) pthread_join\n", dev_idx);
		ret = pthread_join(thread, NULL);
		xprintf(SDK_DBG, "+IO(%d) pthread_join, ret=%d\n", dev_idx, ret);
		net_close(dev);
		io_close(dev);
	}

	xfunc_out("name=%s, thread=0x%08X", dev->name, (int) thread);
	dm_put_dev(dev_idx);
	return 0;
}

