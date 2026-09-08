// SPDX-License-Identifier: GPL-2.0

#include "string_utils.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static int copy_known_length(char *destination, size_t destination_size,
			     const char *source, size_t source_length)
{
	if (!destination || !destination_size || !source)
		return -EINVAL;
	if (source_length >= destination_size) {
		destination[0] = '\0';
		return -ENAMETOOLONG;
	}

	memcpy(destination, source, source_length);
	destination[source_length] = '\0';
	return 0;
}

int string_copy(char *destination, size_t destination_size,
		const char *source)
{
	const char *terminator;

	if (!destination || !destination_size || !source)
		return -EINVAL;

	terminator = memchr(source, '\0', destination_size);
	if (!terminator) {
		destination[0] = '\0';
		return -ENAMETOOLONG;
	}

	return copy_known_length(destination, destination_size, source,
				 (size_t)(terminator - source));
}

int string_copy_field(char *destination, size_t destination_size,
		      const char *source, size_t source_size)
{
	const char *terminator;

	if (!destination || !destination_size || !source || !source_size)
		return -EINVAL;

	terminator = memchr(source, '\0', source_size);
	if (!terminator) {
		destination[0] = '\0';
		return -EINVAL;
	}

	return copy_known_length(destination, destination_size, source,
				 (size_t)(terminator - source));
}

int string_vformat_line(char *destination, size_t destination_size,
			const char *prefix, const char *format,
			va_list arguments)
{
	size_t prefix_length;
	size_t used;
	int result;
	int truncated = 0;

	if (!destination || !destination_size)
		return -EINVAL;
	destination[0] = '\0';
	if (destination_size < 2 || !prefix || !format)
		return -EINVAL;

	prefix_length = strlen(prefix);
	if (prefix_length >= destination_size - 1) {
		memcpy(destination, prefix, destination_size - 2);
		used = destination_size - 2;
		truncated = 1;
	} else {
		memcpy(destination, prefix, prefix_length);
		used = prefix_length;
	}
	destination[used] = '\0';

	if (!truncated) {
		result = vsnprintf(destination + used, destination_size - used,
				   format, arguments);
		if (result < 0) {
			destination[used] = '\0';
			return -EINVAL;
		}
		if ((size_t)result >= destination_size - used) {
			used = destination_size - 1;
			truncated = 1;
		} else {
			used += (size_t)result;
		}
	}

	if (used < destination_size - 1) {
		destination[used++] = '\n';
	} else {
		destination[destination_size - 2] = '\n';
		used = destination_size - 1;
		truncated = 1;
	}
	destination[used] = '\0';

	return truncated ? -ENOSPC : 0;
}

int string_format_line(char *destination, size_t destination_size,
		       const char *prefix, const char *format, ...)
{
	va_list arguments;
	int result;

	va_start(arguments, format);
	result = string_vformat_line(destination, destination_size, prefix,
				     format, arguments);
	va_end(arguments);
	return result;
}

void string_hex_byte(char destination[3], unsigned char value)
{
	static const char digits[] = "0123456789abcdef";

	destination[0] = digits[value >> 4];
	destination[1] = digits[value & 0x0f];
	destination[2] = '\0';
}
