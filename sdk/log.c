// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "global.h"
#include "log.h"
#include <time.h>

#define FILE_LOG_TIME_INFO
//#define STDOUT_LOG_TIME_INFO
#define FORCE_ENABLE_STDOUT_API_LOG

bool log_print_all;

#if defined(FORCE_ENABLE_STDOUT_API_LOG)
#define FORCE_ENABLED_STDOUT_MASK	\
	(SDK_FORCE|SDK_ERR|SDK_STD_ERR|SDK_NOTICE|SDK_API_LOG)
#else
#define FORCE_ENABLED_STDOUT_MASK	\
	(SDK_FORCE|SDK_ERR|SDK_STD_ERR|SDK_NOTICE)
#endif

#if (CONFIG_LOG_FILE_BUF_SIZE > 0)
#define LOG_FILE_BUF_SIZE			(CONFIG_LOG_FILE_BUF_SIZE)
#endif

#define MAX_LOG_BUF	4096
static char log_file[256];
static int log_mask_idx = 0;
#if !defined(LOG_FILE_BUF_SIZE)
static int log_fd = 0;
#endif

static const unsigned log_mask[] = {
	/*STDOUT*/
	SDK_LOG_LEVEL_0,
	SDK_LOG_LEVEL_1,
	SDK_LOG_LEVEL_2,
	SDK_LOG_LEVEL_3,
	SDK_LOG_LEVEL_4,
	/*FILE*/
	SDK_LOG_LEVEL_5,
	SDK_LOG_LEVEL_6,
	SDK_LOG_LEVEL_7,
	SDK_LOG_LEVEL_8,
	SDK_LOG_LEVEL_9,

	/*Flush log buffer to file*/
	SDK_LOG_LEVEL_10,
};

#define cur_log_mask()			(log_mask[log_mask_idx])
#define get_log_mask(level)		(log_mask[level])

#if defined(LOG_FILE_BUF_SIZE)
typedef struct log_buf_mng_s {
	char		*buffer;
	unsigned	put_idx,
				count;

	pthread_mutex_t	log_lock;

} log_buf_mng_t;

static log_buf_mng_t log_buf_mnger;

static void log_buffer_init(void)
{
	log_buf_mng_t *mnger = &log_buf_mnger;

	mnger->put_idx = mnger->count = 0;
	pthread_mutex_init(&mnger->log_lock, NULL);
	mnger->buffer = sdk_malloc(LOG_FILE_BUF_SIZE);
}

static int log_buffer_write(const char *str)
{
	log_buf_mng_t *mnger = &log_buf_mnger;
	int len, put_idx, available;

	len = strlen(str);

	pthread_mutex_lock(&mnger->log_lock);
	put_idx = mnger->put_idx % LOG_FILE_BUF_SIZE;
	available = LOG_FILE_BUF_SIZE - put_idx;

	if (available >= len)
		memcpy(&mnger->buffer[put_idx], str, len);
	else {
		memcpy(&mnger->buffer[put_idx], str, available);
		memcpy(&mnger->buffer[0], &str[available], len-available);
	}

	mnger->put_idx += len;
	if (mnger->count <= LOG_FILE_BUF_SIZE)
		mnger->count += len;
	pthread_mutex_unlock(&mnger->log_lock);
	return len;
}
#define log_fprintf		log_buffer_write

static void log_buffer_flush(void)
{
	log_buf_mng_t *mnger = &log_buf_mnger;
	int fd, ret, put_idx, to_write;

	if (!strlen(log_file)) {
		fprintf(stderr, "Log file was not set up!\n");
		return;
	}

	if ((fd = open(log_file, O_CREAT|O_WRONLY|O_TRUNC, 0644)) < 0) {
		fprintf(stderr, "open(%s) file failed(%s)\n", log_file, strerror(errno));
		return;
	}

	pthread_mutex_lock(&mnger->log_lock);
	if (mnger->count < LOG_FILE_BUF_SIZE) {
		to_write = mnger->count;
		ret = write(fd, mnger->buffer, to_write);
	}
	else {
		put_idx = mnger->put_idx % LOG_FILE_BUF_SIZE;
		to_write = LOG_FILE_BUF_SIZE - put_idx;
		ret = write(fd, &mnger->buffer[put_idx], to_write);
		if (ret == to_write) {
			to_write = LOG_FILE_BUF_SIZE - to_write;
			ret = write(fd, &mnger->buffer[0], to_write);
		}
	}
	pthread_mutex_unlock(&mnger->log_lock);

	if (ret != to_write) {
		fprintf(stderr, "ERROR write: %d!=%d, %s(%d)\n",
			to_write, ret, strerror(errno), errno);
	}
	else
		printf("log buffer has been flushed to %s(%d)!\n", log_file, ret);
	close(fd);
}

static void log_buffer_deinit(void)
{
	log_buf_mng_t *mnger = &log_buf_mnger;

	if (mnger->buffer) {
		//log_buffer_flush();
		sdk_free(mnger->buffer);
		mnger->buffer = NULL;
		pthread_mutex_destroy(&mnger->log_lock);
	}
}
#endif

int log_set_level(int level)
{
	int prev_level = log_mask_idx;
	#if !defined(LOG_FILE_BUF_SIZE)
	int fd;
	#endif

	if (level >= numof_array(log_mask))
		return log_mask_idx;
	
	#if defined(LOG_FILE_BUF_SIZE)
	if (cur_log_mask() & SDK_LOG_FILE) {
		if (get_log_mask(level) & SDK_LOG_FILE_FLUSH) {
			log_buffer_flush();
			return log_mask_idx;
		}
		if (!(get_log_mask(level) & SDK_LOG_FILE))
			log_buffer_deinit();
	}
	else if ((get_log_mask(level) & SDK_LOG_FILE))
		log_buffer_init();
	#else
	if ((get_log_mask(level) & SDK_LOG_FILE) && log_fd <= 0) {
		if (!strlen(log_file)) {
			fprintf(stderr, "Log file was not set up!\n");
			return -1;
		}
		if ((fd = open(log_file, O_RDWR|O_APPEND)) < 0 && errno == ENOENT) {
			if ((fd = open(log_file, O_CREAT|O_RDWR|O_APPEND, 0644)) < 0) {
				fprintf(stderr, "open(%s) file failed\n", log_file);
				return -1;
			}
		}
		log_fd = fd;
	}
	#endif
	
	log_mask_idx = level;
	return prev_level;
}

void log_init(const char *logfile, int log_level)
{
	#if !defined(LOG_FILE_BUF_SIZE)
	if (log_fd) {
		fprintf(stderr, "log_fd(%d) is already opened\n", log_fd);
		return;
	}
	#endif

	if (logfile)
		strcpy(log_file, logfile);

	log_set_level(log_level);

	xprintf(SDK_NOTICE, "SDK Log Level:%d\n", log_mask_idx);
}

void log_deinit(void)
{
	#if defined(LOG_FILE_BUF_SIZE)
	log_set_level(0);
	#else
	if (log_fd > 0) {
		close(log_fd);
		log_fd = 0;
	}
	#endif
}

#if !defined(LOG_FILE_BUF_SIZE)
static int log_fprintf(const char *str)
{
	int fd = log_fd;
	int ret = 0;

	if (fd > 0)
		ret = write(fd, str, strlen(str));
	return ret;
}
#endif

static int log_add_time_info(char *buf, const char *title)
{
	char *pos = buf;
	time_t t;
	struct tm *tm;
	struct timeval tv;

	time(&t);
	tm = localtime(&t);
	gettimeofday(&tv, NULL);

	if (title)
		pos += sprintf(pos, "[%02d:%02d:%02d:%03d %s] ",
			tm->tm_hour, tm->tm_min, tm->tm_sec, (int)(tv.tv_usec/1000), title);
	else
		pos += sprintf(pos, "[%02d:%02d:%02d:%03d] ",
			tm->tm_hour, tm->tm_min, tm->tm_sec, (int)(tv.tv_usec/1000));

	return pos - buf;
}

static inline int log_add_decoration(char *buf, bool file, const char *title)
{
	char *pos = buf;

	if (file) {
		#if defined(FILE_LOG_TIME_INFO)
		pos += log_add_time_info(pos, title);
		#else
		if (title)
			pos += sprintf(pos, "[%s] ", title);
		#endif
	}
	else {
		#if defined(STDOUT_LOG_TIME_INFO)
		pos += log_add_time_info(pos, title);
		#else
		if (title)
			pos += sprintf(pos, "[%s] ", title);
		#endif
	}

	return pos - buf;
}

static int log_vfprintf(const char *title, const char *fmt, va_list args)
{
	char buf[MAX_LOG_BUF+256];
	char *pos = buf;
	int ret;

	pos += log_add_decoration(pos, TRUE, title);

	vsprintf(pos, fmt, args);
	ret = log_fprintf(buf);

	return ret;
}

static int log_vprintf(const char *title, const char *fmt, va_list args)
{
	char buf[MAX_LOG_BUF];
	char *pos = buf;
	int ret;

	pos += log_add_decoration(pos, FALSE, title);

	vsprintf(pos, fmt, args);
	#if 1
	ret = printf("%s", buf);
	fflush(stdout);
	#else
	ret = fprintf(stderr, buf);
	#endif

	return ret;
}

int log_printf(unsigned mask, const char *title, const char *fmt, ...)
{
	va_list args;
	int ret = 0;

	if ((cur_log_mask() & mask) || log_print_all) {
		va_start(args, fmt);
		if (cur_log_mask() & SDK_LOG_FILE)
			ret = log_vfprintf(title, fmt, args);
		if ((cur_log_mask() & SDK_LOG_STDOUT) || (mask & FORCE_ENABLED_STDOUT_MASK)
			|| log_print_all)
			ret = log_vprintf(title, fmt, args);
		va_end(args);
	}

	return ret;
}

int log_printf_string(unsigned mask, int flag, const char *title, const char *str)
{
	char buf[MAX_LOG_BUF+256];
	char *pos = buf;
	int ret = 0;

	if ((cur_log_mask() & mask) || log_print_all) {
		if (cur_log_mask() & SDK_LOG_FILE) {
			pos += log_add_decoration(pos, TRUE, title);
			strcpy(pos, str);
			ret = log_fprintf(buf);
			if (flag & SDK_LOG_FLAG_NO_STDOUT_IN_LOGFILE)
				goto out;
		}
		if ((cur_log_mask() & SDK_LOG_STDOUT) || (mask & FORCE_ENABLED_STDOUT_MASK)
			|| log_print_all) {
			pos = buf;
			#if 1
			pos += log_add_decoration(pos, FALSE, NULL);
			#else
			pos += log_add_decoration(pos, FALSE, title);
			#endif
			strcpy(pos, str);
			ret = printf("%s", buf);
			fflush(stdout);
		}
	}
out:
	return ret;
}

void log_printf_hex(unsigned mask, const char *title, void *buf, int len)
{
	char *mbuf, *pos;
	u8 *p = buf;
	int i;

	if ((cur_log_mask() & mask) || log_print_all) {
		if (len > MAX_LOG_BUF/4)
			len = MAX_LOG_BUF/4;

		mbuf = pos = sdk_malloc(len * 7);

		for (i = 0; i < len; i++) {
			if (i && (i % 16 == 0))
				pos += sprintf(pos, "\n");
			pos += sprintf(pos, "%02x ", *p++);
		};

		log_printf(mask, title, "\n%s\n", mbuf);
		sdk_free(mbuf);
	}
}

