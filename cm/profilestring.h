// Copyright (c) 2012 GCT Semiconductor, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(PROFILE_H_20070923)
#define PROFILE_H_20070923

int get_profile_section(const char *section, char *buf, int buf_size, const char *file);
int get_profile_section_names(char *buf, int buf_size, const char *file);
int get_profile_string(const char *section, const char *key, const char *default_str,
	char *buf, int buf_size, const char *file);
int write_profile_string(const char *section, const char *key, const char *str,
	const char *file);

#endif
