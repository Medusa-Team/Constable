/* SPDX-License-Identifier: GPL-2.0 */
/**
 * @file error.h
 * @short header file for error.c
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#ifndef _ERROR_H
#define _ERROR_H

#include <pthread.h>

extern pthread_key_t errstr_key;
extern char *Out_of_memory;
extern char *Space_already_defined;
extern char *Out_of_vs;

int error(const char *fmt, ...);
int warning(const char *fmt, ...);

#endif /* _ERROR_H */
