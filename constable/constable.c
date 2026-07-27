// SPDX-License-Identifier: GPL-2.0
/*
 * Constable: constable.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>
#include "constable.h"
#include "string_utils.h"

char *retprintf(const char *fmt, ...)
{
	va_list ap;
	static char buf[4096];

	va_start(ap, fmt);
	vsnprintf(buf, 4000, fmt, ap);
	va_end(ap);

	return buf;
}

int init_error(const char *fmt, ...)
{
	va_list ap;
	char buf[4096];

	va_start(ap, fmt);
	string_vformat_line(buf, sizeof(buf), "", fmt, ap);
	va_end(ap);
	write(1, buf, strlen(buf));

	return -1;
}

#ifdef DEBUG_TRACE
pthread_key_t runtime_file_key;
pthread_key_t runtime_pos_key;
#endif
int runtime(const char *fmt, ...)
{
	va_list ap;
	char buf[4096];
	const char *prefix = "Runtime error : ";

#ifdef DEBUG_TRACE
	char *runtime_file;
	char *runtime_pos;
	char runtime_prefix[160];

	runtime_file = (char *) pthread_getspecific(runtime_file_key);
	runtime_pos = (char *) pthread_getspecific(runtime_pos_key);
	if (runtime_file && runtime_pos && runtime_file[0] && runtime_pos[0]) {
		snprintf(runtime_prefix, sizeof(runtime_prefix),
			 "Runtime error [\"%.64s\" %.12s]: ",
			 runtime_file, runtime_pos);
		prefix = runtime_prefix;
	}
#endif
	va_start(ap, fmt);
	string_vformat_line(buf, sizeof(buf), prefix, fmt, ap);
	va_end(ap);
	//medusa_printlog("%s", buf);
	write(1, buf, strlen(buf));

	return 0;
}

int fatal(const char *fmt, ...)
{
	va_list ap;
	char buf[4096];

	va_start(ap, fmt);
	string_vformat_line(buf, sizeof(buf), "Fatal error : ", fmt, ap);
	va_end(ap);
	//medusa_printlog("%s", buf);
	write(1, buf, strlen(buf));

	exit(-1);
	return 0;
}
