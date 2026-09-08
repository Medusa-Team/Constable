/* SPDX-License-Identifier: GPL-2.0 */
/* Dynamic data buffer, originally (c)1998 by Marek Zelem. */

#include <mcompiler/dynamic.h>

#include <limits.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static int allocation_size(int elements, int unit_size, size_t *bytes)
{
	if (elements <= 0 || unit_size <= 0 ||
	    (size_t)elements > SIZE_MAX / (size_t)unit_size)
		return 0;
	*bytes = (size_t)elements * (size_t)unit_size;
	return 1;
}

static int resize_data(ddata_t *data, int capacity)
{
	void *replacement;
	size_t bytes;

	if (!allocation_size(capacity, data->size, &bytes))
		return 0;
	replacement = realloc(data->buf, bytes);
	if (!replacement)
		return 0;
	data->buf = replacement;
	data->len = capacity;
	return 1;
}

static int ensure_capacity(ddata_t *data, int additional)
{
	int needed;
	int capacity;

	if (!data || data->size <= 0 || data->ml <= 0 || data->n < 0 ||
	    data->len < data->n || additional < 0 ||
	    data->n > INT_MAX - additional)
		return 0;
	needed = data->n + additional;
	if (needed <= data->len)
		return 1;
	if (needed > INT_MAX - data->ml)
		return 0;
	capacity = needed + data->ml;
	return resize_data(data, capacity);
}

ddata_t *dd_alloc(int size, int minlen)
{
	ddata_t *data;
	size_t bytes;

	if (!allocation_size(minlen, size, &bytes))
		return NULL;
	data = malloc(sizeof(*data));
	if (!data)
		return NULL;
	data->buf = malloc(bytes);
	if (!data->buf) {
		free(data);
		return NULL;
	}
	data->size = size;
	data->ml = minlen;
	data->len = minlen;
	data->n = 0;
	return data;
}

int dd_free(ddata_t *data)
{
	if (!data)
		return 2;
	free(data->buf);
	free(data);
	return 1;
}

void *dd_extract(ddata_t *data)
{
	void *buffer;

	if (!data)
		return NULL;
	buffer = data->buf;
	data->buf = NULL;
	dd_free(data);
	return buffer;
}

int dd_append(ddata_t *data, void *buffer, int length)
{
	if (!data || length < 0 || (length && !buffer) ||
	    !ensure_capacity(data, length))
		return 0;
	if (length)
		memcpy(data->buf + (size_t)data->n * (size_t)data->size,
		       buffer, (size_t)length * (size_t)data->size);
	data->n += length;
	return 1;
}

ddata_t *dd_cappend(ddata_t *data, int size, int minlen, void *buffer,
		    int length)
{
	int created = 0;

	if (!data) {
		data = dd_alloc(size, minlen);
		if (!data)
			return NULL;
		created = 1;
	}
	if (!dd_append(data, buffer, length)) {
		if (created)
			dd_free(data);
		return NULL;
	}
	return data;
}

int dd_insert(ddata_t *data, int position, void *buffer, int length)
{
	size_t unit_size;

	if (!data || length < 0 || (length && !buffer))
		return 0;
	if (position < 0 || position > data->n)
		return -1;
	if (!ensure_capacity(data, length))
		return 0;
	unit_size = (size_t)data->size;
	if (length && position < data->n)
		memmove(data->buf + (size_t)(position + length) * unit_size,
			data->buf + (size_t)position * unit_size,
			(size_t)(data->n - position) * unit_size);
	if (length)
		memcpy(data->buf + (size_t)position * unit_size, buffer,
		       (size_t)length * unit_size);
	data->n += length;
	return 1;
}

int dd_delete(ddata_t *data, int position, int length)
{
	size_t unit_size;

	if (!data || position < 0 || length < 0 || position > data->n ||
	    length > data->n - position)
		return 0;
	unit_size = (size_t)data->size;
	if (length)
		memmove(data->buf + (size_t)position * unit_size,
			data->buf + (size_t)(position + length) * unit_size,
			(size_t)(data->n - position - length) * unit_size);
	data->n -= length;

	if (data->len - data->n > data->ml &&
	    data->n <= INT_MAX - data->ml)
		(void)resize_data(data, data->n + data->ml);
	return 1;
}

int dd_putc(ddata_t *data, char character)
{
	return dd_append(data, &character, 1);
}

int dd_tostring(ddata_t *data)
{
	char terminator = '\0';

	return dd_append(data, &terminator, 1);
}

ddata_t *dd_printf(ddata_t *data, char *format, ...)
{
	va_list arguments;
	va_list copy;
	int created = 0;
	int length;

	if (!format)
		return NULL;
	if (!data) {
		data = dd_alloc(1, 128);
		if (!data)
			return NULL;
		created = 1;
	}
	if (data->size != 1)
		return data;

	va_start(arguments, format);
	va_copy(copy, arguments);
	length = vsnprintf(NULL, 0, format, copy);
	va_end(copy);
	if (length < 0 || length == INT_MAX ||
	    !ensure_capacity(data, length + 1)) {
		va_end(arguments);
		if (created)
			dd_free(data);
		return NULL;
	}
	if (vsnprintf(data->buf + data->n, (size_t)length + 1, format,
		      arguments) != length) {
		va_end(arguments);
		if (created)
			dd_free(data);
		return NULL;
	}
	va_end(arguments);
	data->n += length;
	return data;
}

int dd_getdd(ddata_t *data, FILE *file, void *end_of_line, int include_eol)
{
	int count = 0;

	if (!data || !file || data->size <= 0)
		return -1;
	for (;;) {
		void *destination;

		if (!ensure_capacity(data, 1))
			return -1;
		destination =
			data->buf + (size_t)data->n * (size_t)data->size;
		if (fread(destination, (size_t)data->size, 1, file) != 1)
			return -1;
		if (end_of_line &&
		    !memcmp(destination, end_of_line, (size_t)data->size)) {
			if (include_eol) {
				data->n++;
				count++;
			}
			return count;
		}
		data->n++;
		count++;
	}
}
