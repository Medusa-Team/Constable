/* SPDX-License-Identifier: GPL-2.0 */
#ifndef CONSTABLE_MLIBC_FORMAT_H
#define CONSTABLE_MLIBC_FORMAT_H

#include <stdarg.h>
#include <stddef.h>

int mlibc_vsnprintf(char *buffer, size_t capacity, const char *format,
		    va_list arguments);
int mlibc_snprintf(char *buffer, size_t capacity, const char *format, ...);

#endif /* CONSTABLE_MLIBC_FORMAT_H */
