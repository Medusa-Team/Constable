// SPDX-License-Identifier: GPL-2.0

#include <mcompiler/dynamic.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static int checks;

#define EXPECT_TRUE(condition, description)					\
	do {									\
		checks++;							\
		if (!(condition)) {						\
			fprintf(stderr, "FAIL: %s\n", description);		\
			failures++;						\
		}								\
	} while (0)

static void test_dynamic_data(void)
{
	int initial[] = { 1, 2, 3 };
	int inserted[] = { 9, 8 };
	int expected[] = { 1, 9, 8, 2, 3 };
	ddata_t *data;
	ddata_t *empty;

	EXPECT_TRUE(dd_alloc(0, 4) == NULL,
		    "zero-width dynamic data is rejected");
	EXPECT_TRUE(dd_alloc(1, 0) == NULL,
		    "zero-capacity dynamic data is rejected");

	data = dd_alloc(sizeof(int), 2);
	EXPECT_TRUE(data != NULL, "dynamic data allocation succeeds");
	if (!data)
		return;
	EXPECT_TRUE(dd_append(data, initial, 3) == 1,
		    "dynamic data grows for an append");
	EXPECT_TRUE(dd_insert(data, 1, inserted, 2) == 1,
		    "dynamic data grows for an insertion");
	EXPECT_TRUE(data->n == 5 &&
		    !memcmp(data->buf, expected, sizeof(expected)),
		    "inserted elements preserve surrounding data");
	EXPECT_TRUE(dd_delete(data, 1, 2) == 1 && data->n == 3 &&
		    !memcmp(data->buf, initial, sizeof(initial)),
		    "deletion compacts the remaining elements");
	EXPECT_TRUE(dd_append(data, initial, -1) == 0 && data->n == 3,
		    "a negative append is rejected without changing length");
	EXPECT_TRUE(dd_delete(data, 2, 2) == 0 && data->n == 3,
		    "an out-of-range deletion is rejected");
	EXPECT_TRUE(dd_insert(data, 4, inserted, 1) == -1 && data->n == 3,
		    "an out-of-range insertion preserves its legacy error");
	data->n = INT_MAX;
	data->len = INT_MAX;
	EXPECT_TRUE(dd_append(data, initial, 1) == 0 && data->n == INT_MAX,
		    "dynamic data rejects element-count overflow");
	data->n = 3;
	data->len = 5;
	dd_free(data);

	empty = dd_alloc(1, 2);
	EXPECT_TRUE(empty != NULL && dd_tostring(empty) == 1 &&
		    empty->n == 1 && empty->buf[0] == '\0',
		    "an empty dynamic string receives a real terminator");
	dd_free(empty);
}

static void test_large_format(void)
{
	char *source = malloc(9001);
	ddata_t *data;

	EXPECT_TRUE(source != NULL, "large format source allocation succeeds");
	if (!source)
		return;
	memset(source, 'x', 9000);
	source[9000] = '\0';
	data = dd_printf(NULL, "%s", source);
	EXPECT_TRUE(data != NULL && data->n == 9000,
		    "dynamic formatting grows beyond the historical 8192 bytes");
	EXPECT_TRUE(data && data->buf[8999] == 'x' &&
		    data->buf[9000] == '\0',
		    "large dynamic formatting remains terminated");
	dd_free(data);
	free(source);
}

static void test_fifo(void)
{
	int initial[] = { 1, 2, 3 };
	int prefix[] = { 9, 8 };
	int output[4] = { 0 };
	int one;
	dfifo_t *fifo = dfifo_create(sizeof(int), 2);

	EXPECT_TRUE(dfifo_create(0, 2) == NULL,
		    "zero-width FIFO elements are rejected");
	EXPECT_TRUE(fifo != NULL, "FIFO allocation succeeds");
	if (!fifo)
		return;
	EXPECT_TRUE(dfifo_write(fifo, initial, 3) == 3,
		    "FIFO grows for a write");
	EXPECT_TRUE(dfifo_read(fifo, &one, 1) == 1 && one == 1,
		    "FIFO reads its first element");
	EXPECT_TRUE(dfifo_unread(fifo, prefix, 2) == 2,
		    "FIFO can prepend more elements than consumed slack");
	EXPECT_TRUE(dfifo_read(fifo, output, 4) == 4 &&
		    output[0] == 9 && output[1] == 8 &&
		    output[2] == 2 && output[3] == 3,
		    "FIFO prepend preserves the active sequence");
	EXPECT_TRUE(dfifo_write(fifo, initial, -1) == -1 && fifo->n == 0,
		    "a negative FIFO write is rejected without changing state");
	fifo->n = INT_MAX;
	fifo->len = INT_MAX;
	fifo->pos = 0;
	EXPECT_TRUE(dfifo_write(fifo, initial, 1) == -1 &&
		    fifo->n == INT_MAX,
		    "FIFO rejects element-count overflow");
	fifo->n = 0;
	fifo->len = 2;
	dfifo_delete(fifo);
}

static void test_dfield_bounds(void)
{
	dfield_t *field;
	struct dfield_dim_s *dimension;
	int value = 42;

	EXPECT_TRUE(dfield_create(0, sizeof(value), NULL) == NULL,
		    "a zero-dimensional field is rejected");
	field = dfield_create(1, sizeof(value), NULL);
	EXPECT_TRUE(field != NULL, "one-dimensional field allocation succeeds");
	if (!field)
		return;
	dimension = dfield_first_dim(field);
	EXPECT_TRUE(dfield_get_data(NULL, 0) == NULL,
		    "NULL field lookup is rejected before dereference");
	EXPECT_TRUE(dfield_add_data(dimension, &value) == 0,
		    "field data insertion succeeds");
	dimension = dfield_first_dim(field);
	EXPECT_TRUE(dfield_get_data(dimension, 1) == NULL,
		    "field lookup rejects the one-past-end index");
	EXPECT_TRUE(*(int *)dfield_get_data(dimension, -1) == value,
		    "negative-one field lookup selects the final element");
	dfield_delete(field);
}

int main(void)
{
	test_dynamic_data();
	test_large_format();
	test_fifo();
	test_dfield_bounds();

	if (failures) {
		fprintf(stderr, "dynamic buffers: %d failure(s)\n", failures);
		return 1;
	}

	printf("dynamic buffers: %d checks passed\n", checks);
	return 0;
}
