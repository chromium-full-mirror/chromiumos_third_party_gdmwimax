// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15)
#include <linux/types.h>
#endif
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include "netlink_u.h"

#if !defined(NLMSG_HDRLEN)
#define NLMSG_HDRLEN	 ((int) NLMSG_ALIGN(sizeof(struct nlmsghdr)))
#endif

#define ND_MAX_GROUP			30
#define ND_IFINDEX_LEN			sizeof(int)
#define ND_NLMSG_SPACE(len)		(NLMSG_SPACE(len) + ND_IFINDEX_LEN)
#define ND_NLMSG_DATA(nlh)		((void *)((char *)NLMSG_DATA(nlh) + ND_IFINDEX_LEN))
#define ND_NLMSG_S_LEN(len)		(len+ND_IFINDEX_LEN)
#define ND_NLMSG_R_LEN(nlh)		(nlh->nlmsg_len-ND_IFINDEX_LEN)
#define ND_NLMSG_IFIDX(nlh)		NLMSG_DATA(nlh)
#define ND_MAX_MSG_LEN			8096

int nl_open(hnetlink_t *hnl, int unit, int ifindex, unsigned int group)
{
	struct sockaddr_nl nladdr;
	int fd;

	if (group > ND_MAX_GROUP) {
		fprintf(stderr, "ERROR: Group %d is invalied.\n", group);
		fprintf(stderr, "ERROR: Valid group is 0 ~ %d.\n", ND_MAX_GROUP);
		return -1;
	}

	fd = socket(AF_NETLINK, SOCK_RAW, unit);
	if (fd < 0) {
		fprintf(stderr, "ERROR: Cannot open netlink socket(%d). %s(%d)\n",
			unit, strerror(errno), errno);
		return -1;
	}

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;
	nladdr.nl_pid = 0;   /* For Linux Kernel */
	nladdr.nl_groups = 1;	/*For Multicase*/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
	nladdr.nl_groups = group+1;
#else
	nladdr.nl_groups = 1 << group;
#endif

	if (bind(fd, (struct sockaddr *)&nladdr, sizeof(nladdr)) < 0) {
		fprintf(stderr, "ERROR: Cannot bind netlink socket. %s(%d)\n", strerror(errno), errno);
		close(fd);
		return -1;
	}

	hnl->fd = fd;
	hnl->ifindex = ifindex;
	return 0;
}

int nl_close(hnetlink_t *hnl)
{
	if (!hnl) {
		errno = EINVAL;
		return -1;
	}

	close(hnl->fd);
	memset(hnl, 0, sizeof(hnetlink_t));
	return 0;
}

int nl_send(hnetlink_t *hnl, unsigned short type, void *buf, int len)
{
	struct nlmsghdr *nlh;
	struct sockaddr_nl nladdr;
	struct iovec iov;
	struct msghdr msg = {(void *)&nladdr, sizeof(nladdr), &iov, 1, NULL, 0, 0};
	int fd, ret = -1;

	if (!hnl || !buf || !len) {
		errno = EINVAL;
		return -1;
	}

	fd = hnl->fd;

	if (ND_NLMSG_SPACE(len) > ND_MAX_MSG_LEN) {
		errno = EINVAL;
		return -1;
	}

	nlh = (struct nlmsghdr *) malloc(ND_NLMSG_SPACE(len));
 	nlh->nlmsg_len = ND_NLMSG_S_LEN(len);
	nlh->nlmsg_flags = 0;
	nlh->nlmsg_type = type;
	nlh->nlmsg_pid = 0;
	memcpy(ND_NLMSG_IFIDX(nlh), &hnl->ifindex, ND_IFINDEX_LEN);
	memcpy(ND_NLMSG_DATA(nlh), buf, len);

 	iov.iov_base = (void *) nlh;
	iov.iov_len = ND_NLMSG_SPACE(len);

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;

	ret = sendmsg(fd, &msg, 0);

	free(nlh);

	if (ret <= 0)
		return -1;

	return len;
}

int nl_recv(hnetlink_t *hnl, char *buf, int len, int flags)
{
	struct nlmsghdr *nlh;
	struct sockaddr_nl nladdr;
	struct iovec iov;
	struct msghdr msg = {(void *)&nladdr, sizeof(nladdr), &iov, 1, NULL, 0, 0};
	int fd, ret;

	if (!hnl || !buf || !len) {
		errno = EINVAL;
		return -1;
	}

	fd = hnl->fd;

	nlh = (struct nlmsghdr *) malloc(NLMSG_SPACE(len));
	memset(nlh, 0, NLMSG_SPACE(len));

	iov.iov_base = (void *)nlh;
	iov.iov_len = NLMSG_SPACE(len);

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;

	ret = recvmsg(fd, &msg, flags);

	if (ret > 0) {
		ret = nlh->nlmsg_len - NLMSG_HDRLEN;
		memcpy(buf, NLMSG_DATA(nlh), ret);
	}

	free(nlh);
	return ret;
}
