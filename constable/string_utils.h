/* SPDX-License-Identifier: GPL-2.0 */
#ifndef CONSTABLE_STRING_UTILS_H
#define CONSTABLE_STRING_UTILS_H

#include <stdarg.h>
#include <stddef.h>

/*
 * Copy a regular C string.  The destination is left as an empty string when
 * the source does not fit.
 */
int string_copy(char *destination, size_t destination_size,
		const char *source);

/*
 * Copy a string received in a fixed-width field.  Unlike strncpy(), this
 * rejects a source field that contains no terminating NUL.
 */
int string_copy_field(char *destination, size_t destination_size,
		      const char *source, size_t source_size);

/*
 * Format one diagnostic line from a prefix and printf-style message.
 * The result is always NUL-terminated when destination_size is non-zero and
 * ends in a newline even when the message has to be truncated.
 */
int string_vformat_line(char *destination, size_t destination_size,
			const char *prefix, const char *format,
			va_list arguments);
int string_format_line(char *destination, size_t destination_size,
		       const char *prefix, const char *format, ...);

/* Encode one byte as two lowercase hexadecimal digits plus a trailing NUL. */
void string_hex_byte(char destination[3], unsigned char value);

#endif /* CONSTABLE_STRING_UTILS_H */
