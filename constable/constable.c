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
	vsnprintf(buf, 4000, fmt, ap);
	va_end(ap);
	sprintf(buf + strlen(buf), "\n");
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

#ifndef DEBUG_TRACE
	sprintf(buf, "Runtime error : ");
#else
	char *runtime_file;
	char *runtime_pos;

	runtime_file = (char *) pthread_getspecific(runtime_file_key);
	runtime_pos = (char *) pthread_getspecific(runtime_pos_key);
	sprintf(buf, "Runtime error [\"%s\" %s]: ", runtime_file, runtime_pos);
#endif
	va_start(ap, fmt);
	vsnprintf(buf + strlen(buf), 4000, fmt, ap);
	va_end(ap);
	//medusa_printlog("%s", buf);
	sprintf(buf + strlen(buf), "\n");
	write(1, buf, strlen(buf));

	return 0;
}

int fatal(const char *fmt, ...)
{
	va_list ap;
	char buf[4096];

	sprintf(buf, "Fatal error : ");
	va_start(ap, fmt);
	vsnprintf(buf + strlen(buf), 4000, fmt, ap);
	va_end(ap);
	//medusa_printlog("%s", buf);
	sprintf(buf + strlen(buf), "\n");
	write(1, buf, strlen(buf));

	exit(-1);
	return 0;
}
