// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(GLOBAL_H_20080709)
#define GLOBAL_H_20080709

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

#include "list.h"

#if !defined(TRUE)
#define TRUE 1
#define FALSE 0
#endif

#if !defined(TRUE)
#define TRUE 1
#define FALSE 0
#endif

#if !defined(SDK_TYPEDEF)
#define SDK_TYPEDEF
typedef char 				bool;
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

typedef struct buf_s {
	void	*buf;
	int		size;

} buf_t;

inline static void bufset(buf_t *dst, buf_t *src)
{
	if (src) {
		dst->buf = src->buf;
		dst->size = src->size;
	}
	else {
		dst->buf = NULL;
		dst->size = 0;
	}
}

typedef void (*void_func_t) (void);

#define STR(s)		#s

#define B2H(x)		ntohs(x)
#define H2B(x)		htons(x)
#define DB2H(x)		ntohl(x)
#define DH2B(x)		htonl(x)

#define U82U16(b)	((((u16)((u8*)b)[0])<<8)|((u16)((u8*)b)[1]))
#define U82U24(b)	((((u32)((u8*)b)[0])<<16)|(((u32)((u8*)b)[1])<<8)|((u32)((u8*)b)[2]))
#define U82U32(b)	(((u32)U82U16(b)<<16)|(u32)U82U16((u8*)(b)+2))
#define U162U8(b,v)	((((u8*)b)[0])=((u16)(v)>>8),(((u8*)b)[1])=(u8)((u16)(v)))
#define U242U8(b,v)	((((u8*)b)[0])=(u8)((u32)(v)>>16),(((u8*)b)[1])=(u8)((u32)(v)>>8),\
						(((u8*)b)[2])=(u8)((u32)(v)))
#define U322U8(b,v)	(U162U8((u8*)(b)+2, v), U162U8(b, (u32)v>>16))

#define set_msb(x32, x8)	(x32 = ((u32)x32 & 0x00ffffff) | ((u32)x8<<24))
#define get_msb(x32)		((u8)((u32)x32>>24))

#define IS_BIG_ENDIAN()	(B2H(0x1122)==0x1122)


#define MAC_FORMAT_STR		"%02X%02X%02X%02X%02X%02X"
#define MAC_INPUT_STR(m)		((u8*)m)[0],((u8*)m)[1],((u8*)m)[2]\
								,((u8*)m)[3],((u8*)m)[4],((u8*)m)[5]

#define sdk_malloc(s)	malloc(s)
#define sdk_free(b)		free(b)

#ifndef offsetof
#define	offsetof(type, field)	((int)(&((type*)0)->field))
#endif

#define rangeof(type, start, end)	(offsetof(type, end)-offsetof(type, start)\
									+sizeof(((type*)0)->end))

#define numof_array(arr)	(sizeof(arr)/sizeof(arr[0]))

#define THREAD_EXIT_MSG	((void *) -1)

//#define SDK_TEST
#endif
