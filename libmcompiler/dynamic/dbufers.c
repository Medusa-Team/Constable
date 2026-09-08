/* SPDX-License-Identifier: GPL-2.0 */
/* Dynamic FIFO/LIFO buffer, originally (c)1998 by Marek Zelem. */

#include <mcompiler/dynamic.h>

#include <limits.h>
#include <stdint.h>
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

static int resize_fifo(dfifo_t *fifo, int capacity)
{
	void *replacement;
	size_t bytes;

	if (!allocation_size(capacity, fifo->size, &bytes))
		return 0;
	replacement = realloc(fifo->buf, bytes);
	if (!replacement)
		return 0;
	fifo->buf = replacement;
	fifo->len = capacity;
	return 1;
}

static int fifo_valid(const dfifo_t *fifo)
{
	return fifo && fifo->size > 0 && fifo->ml > 0 && fifo->len > 0 &&
	       fifo->pos >= 0 && fifo->n >= fifo->pos &&
	       fifo->len >= fifo->n;
}

static int ensure_capacity(dfifo_t *fifo, int additional)
{
	int needed;
	int capacity;

	if (!fifo_valid(fifo) || additional < 0 ||
	    fifo->n > INT_MAX - additional)
		return 0;
	needed = fifo->n + additional;
	if (needed <= fifo->len)
		return 1;
	if (needed > INT_MAX - fifo->ml)
		return 0;
	capacity = needed + fifo->ml;
	return resize_fifo(fifo, capacity);
}

dfifo_t *dfifo_create(int size, int minlen)
{
	dfifo_t *fifo;
	size_t bytes;

	if (!allocation_size(minlen, size, &bytes))
		return NULL;
	fifo = malloc(sizeof(*fifo));
	if (!fifo)
		return NULL;
	fifo->buf = malloc(bytes);
	if (!fifo->buf) {
		free(fifo);
		return NULL;
	}
	fifo->size = size;
	fifo->ml = minlen;
	fifo->len = minlen;
	fifo->n = 0;
	fifo->pos = 0;
	return fifo;
}

int dfifo_delete(dfifo_t *fifo)
{
	if (!fifo)
		return 2;
	free(fifo->buf);
	free(fifo);
	return 1;
}

int dfifo_clear(dfifo_t *fifo)
{
	if (!fifo)
		return -1;
	fifo->pos = 0;
	fifo->n = 0;
	return 0;
}

int dfifo_write(dfifo_t *fifo, void *buffer, int length)
{
	if (!fifo || length < 0 || (length && !buffer) ||
	    !ensure_capacity(fifo, length))
		return -1;
	if (length)
		memcpy(fifo->buf + (size_t)fifo->n * (size_t)fifo->size,
		       buffer, (size_t)length * (size_t)fifo->size);
	fifo->n += length;
	return length;
}

int dfifo_read(dfifo_t *fifo, void *buffer, int length)
{
	int available;

	if (!fifo_valid(fifo) || length < 0)
		return -1;
	available = fifo->n - fifo->pos;
	if (length > available)
		length = available;
	if (!length)
		return 0;
	if (buffer)
		memcpy(buffer,
		       fifo->buf + (size_t)fifo->pos * (size_t)fifo->size,
		       (size_t)length * (size_t)fifo->size);
	fifo->pos += length;
	if (fifo->pos == fifo->n) {
		fifo->n = 0;
		fifo->pos = 0;
		(void)resize_fifo(fifo, fifo->ml);
	}
	return length;
}

int dfifo_normalize(dfifo_t *fifo)
{
	int active;

	if (!fifo_valid(fifo))
		return -1;
	active = fifo->n - fifo->pos;
	if (fifo->pos && active)
		memmove(fifo->buf,
			fifo->buf + (size_t)fifo->pos * (size_t)fifo->size,
			(size_t)active * (size_t)fifo->size);
	fifo->pos = 0;
	fifo->n = active;
	if (active <= INT_MAX - fifo->ml)
		(void)resize_fifo(fifo, active + fifo->ml);
	return active;
}

int dfifo_unread(dfifo_t *fifo, void *buffer, int length)
{
	int active;

	if (!fifo_valid(fifo) || length < 0 || (length && !buffer))
		return -1;
	if (!length)
		return 0;

	if (fifo->pos >= length) {
		fifo->pos -= length;
	} else {
		int additional = length - fifo->pos;

		active = fifo->n - fifo->pos;
		if (!ensure_capacity(fifo, additional))
			return -1;
		memmove(fifo->buf + (size_t)length * (size_t)fifo->size,
			fifo->buf + (size_t)fifo->pos * (size_t)fifo->size,
			(size_t)active * (size_t)fifo->size);
		fifo->pos = 0;
		fifo->n = active + length;
	}
	memcpy(fifo->buf + (size_t)fifo->pos * (size_t)fifo->size,
	       buffer, (size_t)length * (size_t)fifo->size);
	return length;
}

int dlifo_read(dfifo_t *fifo, void *buffer, int length)
{
	int available;
	int index;

	if (!fifo_valid(fifo) || length < 0)
		return -1;
	available = fifo->n - fifo->pos;
	if (length > available)
		length = available;
	if (!length)
		return 0;
	if (buffer)
		for (index = 0; index < length; index++)
			memcpy((char *)buffer +
			       (size_t)index * (size_t)fifo->size,
			       fifo->buf +
			       (size_t)(fifo->n - index - 1) *
			       (size_t)fifo->size,
			       (size_t)fifo->size);
	fifo->n -= length;
	if (fifo->pos == fifo->n) {
		fifo->n = 0;
		fifo->pos = 0;
		(void)resize_fifo(fifo, fifo->ml);
	}
	return length;
}

int dfifo_first(dfifo_t *fifo, void *buffer, int length)
{
	int available;

	if (!fifo_valid(fifo) || length < 0 || (length && !buffer))
		return -1;
	available = fifo->n - fifo->pos;
	if (length > available)
		length = available;
	if (length)
		memcpy(buffer,
		       fifo->buf + (size_t)fifo->pos * (size_t)fifo->size,
		       (size_t)length * (size_t)fifo->size);
	return length;
}

int dfifo_last(dfifo_t *fifo, void *buffer, int length)
{
	int available;
	int index;

	if (!fifo_valid(fifo) || length < 0 || (length && !buffer))
		return -1;
	available = fifo->n - fifo->pos;
	if (length > available)
		length = available;
	for (index = 0; index < length; index++)
		memcpy((char *)buffer + (size_t)index * (size_t)fifo->size,
		       fifo->buf +
		       (size_t)(fifo->n - index - 1) * (size_t)fifo->size,
		       (size_t)fifo->size);
	return length;
}

int dfifo_copy(dfifo_t *input, dfifo_t *output)
{
	size_t available_bytes;
	size_t output_units;

	if (!fifo_valid(input) || !fifo_valid(output))
		return -1;
	available_bytes =
		(size_t)(input->n - input->pos) * (size_t)input->size;
	output_units = available_bytes / (size_t)output->size;
	if (output_units > INT_MAX ||
	    !ensure_capacity(output, (int)output_units))
		return -1;
	if (output_units)
		memcpy(output->buf +
		       (size_t)output->n * (size_t)output->size,
		       input->buf +
		       (size_t)input->pos * (size_t)input->size,
		       output_units * (size_t)output->size);
	output->n += (int)output_units;
	return (int)output_units;
}

ddata_t *dfifo_to_ddata(dfifo_t *fifo)
{
	ddata_t *data;
	int active;

	if (!fifo_valid(fifo))
		return NULL;
	active = fifo->n - fifo->pos;
	if (fifo->pos && active)
		memmove(fifo->buf,
			fifo->buf + (size_t)fifo->pos * (size_t)fifo->size,
			(size_t)active * (size_t)fifo->size);
	data = malloc(sizeof(*data));
	if (!data)
		return NULL;
	data->buf = fifo->buf;
	data->len = fifo->len;
	data->n = active;
	data->size = fifo->size;
	data->ml = fifo->ml;
	free(fifo);
	return data;
}
