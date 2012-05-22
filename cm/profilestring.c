// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

#if defined(_WIN32) || defined(_WIN64)
#pragma warning(disable : 4996)
#define LINE_FEED_STR		"\r\n"
#define BINARY_MODE			"b"
#else
#define LINE_FEED_STR		"\n"
#define BINARY_MODE
#endif
#define LSECT_DELIMITER		'['
#define RSECT_DELIMITER		']'
#define KEY_DELIMITER		'='
#define IS_LINEFEED(c)		((c) == '\r' || (c) == '\n')
#define IS_TAB_SPACE(c)		((c) == ' ' || (c) == '\t')
#define IS_COMMENT(c)		((c) == '#' || (c) == ';')
#define BASE_BUF_SIZE		1024

#if 0
#define chrcmp(c1,c2)		((c1)==(c2)
#else
#define chrcmp(c1,c2)		(toupper(c1)==toupper(c2))
#endif

static int cp_key_value(char *buf, const char *line, int n)
{
	char *d = buf;

	while (n--) {
		if (IS_TAB_SPACE(*line)) {
			line++;
			continue;
		}
		if (IS_LINEFEED(*line)) {
			if (n)
				*d++ = 0;
			break;
		}
		if (!(*d = *line)) 
			break;
		d++;
		line++;
	}
	return d - buf;
}

static int cp_value(char *buf, const char *value, int n)
{
	char *d = buf;

	while (n--) {
		if (IS_TAB_SPACE(*value)) {
			value++;
			continue;
		}
		if (IS_LINEFEED(*value)) {
			if (n)
				*d = 0;
			break;
		}
		if (!(*d = *value)) 
			break;
		d++;
		value++;
	}
	return d - buf;
}

static int get_section(const char *line, char *section)
{
	char *pos = section;

	if (*line++ != LSECT_DELIMITER)
		return 0;

	while (*line && !IS_LINEFEED(*line) && *line != RSECT_DELIMITER)
		*pos++ = *line++;

	if (*line == RSECT_DELIMITER) {
		*pos++ = 0;
		return pos-section;
	}
	else
		return 0;
}

static const char * find_section(const char *line, const char *section)
{
	if (*line++ != LSECT_DELIMITER)
		return NULL;

	while (chrcmp(*line, *section)) {
		line++;
		section++;
	}

	if (!*section && *line == RSECT_DELIMITER)
		return section;
	else
		return NULL;
}

static const char * find_key(const char *line, const char *key)
{
	while (chrcmp(*line, *key)) {
		line++;
		key++;
	}

	if (!*key) {
		if (*line != KEY_DELIMITER && !IS_TAB_SPACE(*line))
			return NULL;
		while (*line && *line != KEY_DELIMITER)
			line++;
		if (*line == KEY_DELIMITER) {
			return ++line;
		}
	}
	return NULL;
}

int get_profile_section(const char *section, char *buf, int buf_size, const char *file)
{
	FILE *fp;
	char line[BASE_BUF_SIZE];
	const char *val = NULL;
	char *pos = buf;
	int available = buf_size;
	int len;

	if (!buf || !buf_size || !file) {
		errno = EINVAL;
		return 0;
	}

	if ((fp = fopen(file, "r"BINARY_MODE)) == NULL)
		return 0;

	while (fgets(line, sizeof(line), fp)) {
		if ((val = find_section(line, section)))
			break;
	}

	if (val) {
		val = NULL;
		while (available > 0 && fgets(line, sizeof(line), fp)) {
			if (*line == LSECT_DELIMITER)
				break;
			if (IS_COMMENT(*line) || IS_TAB_SPACE(*line))
				continue;
			len = cp_key_value(pos, line, available);
			available -= len;
			pos += len;
		}
		if (available > 0)
			*pos++ = 0;
	}

	fclose(fp);
	return pos - buf;
}

int get_profile_section_names(char *buf, int buf_size, const char *file)
{
	FILE *fp;
	char line[BASE_BUF_SIZE];
	char *pos = buf;

	if (!buf || !buf_size || !file) {
		errno = EINVAL;
		return -1;
	}

	if ((fp = fopen(file, "r"BINARY_MODE)) == NULL)
		return -1;

	while (fgets(line, sizeof(line), fp))
		pos += get_section(line, pos);

	*pos++ = 0;

	fclose(fp);
	return pos - buf;
}

int get_profile_string(const char *section, const char *key, const char *default_str,
	char *buf, int buf_size, const char *file)
{
	FILE *fp;
	char line[BASE_BUF_SIZE];
	const char *val = NULL;
	int len;

	if (!section || !*section || !key || !*key || !buf || !buf_size || !file) {
		errno = EINVAL;
		return 0;
	}

	if ((fp = fopen(file, "r"BINARY_MODE)) == NULL)
		goto default_value;

	while (fgets(line, sizeof(line), fp)) {
		if ((val = find_section(line, section)))
			break;
	}

	if (val) {
		val = NULL;
		while (fgets(line, sizeof(line), fp)) {
			if (*line == LSECT_DELIMITER)
				break;
			if (IS_COMMENT(*line))
				continue;
			if ((val = find_key(line, key)))
				break;
		}
	}

	fclose(fp);
default_value:
	if (!val) {
		if (!default_str) {
			errno = EINVAL;
			return 0;
		}
		val = default_str;
	}

	len = cp_value(buf, val, buf_size);
	return len;
}

int write_profile_string(const char *section, const char *key, const char *str,
	const char *file)
{
	FILE *fp;
	char line[BASE_BUF_SIZE];
	const char *find = NULL;
	char *rest_data = NULL;
	int rpos = 0, wpos = 0, eof, len, ret = 0;

	if (!section || !*section || !key || !*key || !str || !file) {
		errno = EINVAL;
		return -1;
	}

	if (!(fp = fopen(file, "r+"BINARY_MODE))) {
		if (!(fp = fopen(file, "w"BINARY_MODE)))
			return -1;
		goto append_section;
	}

	while (fgets(line, sizeof(line), fp)) {
		if ((find = find_section(line, section)))
			break;
	}

	if (find) {
		find = NULL;
		while (fgets(line, sizeof(line), fp)) {
			if (*line == LSECT_DELIMITER) {
				find = line;
				if (!wpos)
					wpos = rpos = ftell(fp) - strlen(line);
				break;
			}
			if (IS_COMMENT(*line))
				continue;
			if (IS_LINEFEED(*line)) {
				if (!wpos)
					wpos = rpos = ftell(fp) - strlen(line);
				continue;
			}
			if ((find = find_key(line, key))) {
				rpos = ftell(fp);
				wpos = rpos - strlen(line);
				break;
			}
		}
		if (find == NULL) {
			len = strlen(line);
			if (!IS_LINEFEED(line[len-1]) && !IS_LINEFEED(line[len-2]))
				fputs(LINE_FEED_STR, fp);
			goto append_key;	/*end of file*/
		}
	}

	if (find) {
		fseek(fp, 0, SEEK_END);
		eof = ftell(fp);
		fseek(fp, rpos, SEEK_SET);

		len = eof - rpos;
		rest_data = malloc(len);
		assert(rest_data);
		if (len != fread(rest_data, 1, len, fp)) {
			ret = -1;
			goto close;
		}

		/*Overwrite*/
		fseek(fp, wpos, SEEK_SET);
		sprintf(line, "%s=%s%s", key, str, LINE_FEED_STR);
		fputs(line, fp);
		if (len != fwrite(rest_data, 1, len, fp)) {
			ret = -1;
			goto close;
		}
	}
	else {
		len = strlen(line);
		if (!IS_LINEFEED(line[len-1]) && !IS_LINEFEED(line[len-2]))
			fputs(LINE_FEED_STR, fp);
append_section:
		sprintf(line, "[%s]%s", section, LINE_FEED_STR);
		fputs(line, fp);
append_key:
		sprintf(line, "%s=%s%s", key, str, LINE_FEED_STR);
		fputs(line, fp);
	}
close:
	fclose(fp);
	if (rest_data)
		free(rest_data);
	return ret;
}

