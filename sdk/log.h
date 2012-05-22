// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(LOG_H_07092008)
#define LOG_H_07092008
#include "global.h"

#define SDK_LOG_TITLE	"SDK"

#define SDK_NOW				SDK_FORCE
#define SDK_FORCE			(1<<0)
#define SDK_ERR				(1<<1)
#define SDK_STD_ERR			(1<<2)
#define SDK_NOTICE			(1<<3)
#define SDK_INFO			(1<<4)
#define SDK_DBG				(1<<5)
#define SDK_FUNC			(1<<6)
#define SDK_API_LOG			(1<<7)

#define SDK_LOG_STDOUT		(1<<31)
#define SDK_LOG_FILE		(1<<30)
#define SDK_LOG_FILE_FLUSH	(1<<28)	/*Flush log buffer to file*/

#define _LOG_LEVEL_0		(SDK_FORCE|SDK_ERR|SDK_STD_ERR|SDK_API_LOG)
#define _LOG_LEVEL_1		(_LOG_LEVEL_0|SDK_NOTICE|SDK_API_LOG)
#define _LOG_LEVEL_2		(_LOG_LEVEL_1|SDK_INFO|SDK_API_LOG)
#define _LOG_LEVEL_3		(_LOG_LEVEL_2|SDK_DBG|SDK_API_LOG)
#define _LOG_LEVEL_4		(_LOG_LEVEL_3|SDK_FUNC|SDK_API_LOG)

#define SDK_LOG_LEVEL_0		(_LOG_LEVEL_0|SDK_LOG_STDOUT)
#define SDK_LOG_LEVEL_1		(_LOG_LEVEL_1|SDK_LOG_STDOUT)
#define SDK_LOG_LEVEL_2		(_LOG_LEVEL_2|SDK_LOG_STDOUT)
#define SDK_LOG_LEVEL_3		(_LOG_LEVEL_3|SDK_LOG_STDOUT)
#define SDK_LOG_LEVEL_4		(_LOG_LEVEL_4|SDK_LOG_STDOUT)
#define SDK_LOG_LEVEL_5		(_LOG_LEVEL_0|SDK_LOG_FILE)
#define SDK_LOG_LEVEL_6		(_LOG_LEVEL_1|SDK_LOG_FILE)
#define SDK_LOG_LEVEL_7		(_LOG_LEVEL_2|SDK_LOG_FILE)
#define SDK_LOG_LEVEL_8		(_LOG_LEVEL_3|SDK_LOG_FILE)
#define SDK_LOG_LEVEL_9		(_LOG_LEVEL_4|SDK_LOG_FILE)
#define SDK_LOG_LEVEL_10	(SDK_LOG_FILE_FLUSH)

#define LOG_FILE_FLUSH_LEVEL	10

#define SDK_LOG_FLAG_NO_STDOUT_IN_LOGFILE	(1<<0)

#define xfunc_in(fmt, args...)\
	log_printf(SDK_FUNC, SDK_LOG_TITLE, "+%s[lr:%x] " fmt "\n",\
		__FUNCTION__, __builtin_return_address(0), ## args)
#define xfunc_out(fmt, args...)\
	log_printf(SDK_FUNC, SDK_LOG_TITLE, "-%s " fmt "\n",\
		__FUNCTION__, ## args)

#define xprintf(mask, fmt, args...)	do {\
			if ((mask) & SDK_ERR)\
				log_printf(mask, SDK_LOG_TITLE, "#ERROR: in %s [lr:%x]\n\t:" fmt,\
				__FUNCTION__, __builtin_return_address(0), ## args);\
			else if ((mask) & SDK_STD_ERR)\
				log_printf(mask, SDK_LOG_TITLE, "#ERROR: in %s [lr:%x] err=%s(%d)\n\t:" fmt,\
				__FUNCTION__, __builtin_return_address(0), strerror(errno), errno, ## args);\
			else\
				log_printf(mask, SDK_LOG_TITLE, fmt, ## args);\
		} while (0)

#define xprintf_hex(mask, title, buf, len)	log_printf_hex(mask, title, buf, len)
#define xprintf_mac(mask, msg, buf)\
	log_printf(mask, SDK_LOG_TITLE, "%s=%02x:%02x:%02x:%02x:%02x:%02x\n",\
		msg, (u8)(buf)[0], (u8)(buf)[1], (u8)(buf)[2],\
		(u8)(buf)[3], (u8)(buf)[4], (u8)(buf)[5])

extern bool log_print_all;
#define log_dbg_start()		(log_print_all=TRUE)
#define log_dbg_end()		(log_print_all=FALSE)

void log_init(const char *logfile, int log_level);
void log_deinit(void);
int log_set_level(int level);
int log_printf(unsigned mask, const char *title, const char *fmt, ...);
int log_printf_string(unsigned mask, int flag, const char *title, const char *str);
void log_printf_hex(unsigned mask, const char *title, void *buf, int len);

#endif
