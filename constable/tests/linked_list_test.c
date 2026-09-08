// SPDX-License-Identifier: GPL-2.0

#include <mcompiler/dynamic.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks;
static int failures;

#define EXPECT_TRUE(condition, description)				\
	do {								\
		checks++;						\
		if (!(condition)) {					\
			fprintf(stderr, "FAIL: %s\n", description);	\
			failures++;					\
		}							\
	} while (0)

int main(void)
{
	ll_t *first = ll_alloc(8);
	ll_t *last;
	ll_t *middle;

	EXPECT_TRUE(ll_alloc(SIZE_MAX) == NULL,
		    "an overflowing node allocation is rejected");
	EXPECT_TRUE(ll_add(NULL, 1) == NULL,
		    "append requires an existing node");
	EXPECT_TRUE(ll_ins(NULL, 1) == NULL,
		    "insert requires an existing node");
	EXPECT_TRUE(first != NULL, "the first node is allocated");
	if (!first)
		return EXIT_FAILURE;

	memcpy(ll_data(first), "first", sizeof("first"));
	last = ll_add(first, 7);
	EXPECT_TRUE(last != NULL, "a node can be appended");
	if (!last) {
		ll_free(first);
		return EXIT_FAILURE;
	}
	memcpy(ll_data(last), "last", sizeof("last"));
	middle = ll_ins(last, 9);
	EXPECT_TRUE(middle != NULL, "a node can be inserted");
	if (!middle) {
		ll_free(first);
		return EXIT_FAILURE;
	}
	memcpy(ll_data(middle), "middle", sizeof("middle"));

	EXPECT_TRUE(ll_len(first) == 8 && ll_len(middle) == 9 &&
		    ll_len(last) == 7,
		    "each node retains its requested payload length");
	EXPECT_TRUE(ll_next(first) == middle && ll_prev(middle) == first &&
		    ll_next(middle) == last && ll_prev(last) == middle,
		    "insert and append preserve bidirectional links");
	EXPECT_TRUE(ll_getfirst(last) == first && ll_getlast(first) == last,
		    "first and last traversal reaches both endpoints");
	EXPECT_TRUE(strcmp(ll_data(first), "first") == 0 &&
		    strcmp(ll_data(middle), "middle") == 0 &&
		    strcmp(ll_data(last), "last") == 0,
		    "node payloads remain independent");
	EXPECT_TRUE(ll_del(middle) == last,
		    "deleting a middle node returns its following neighbor");
	EXPECT_TRUE(ll_next(first) == last && ll_prev(last) == first,
		    "deletion reconnects neighboring nodes");
	EXPECT_TRUE(ll_free(first) == 0, "the remaining list is released");

	if (failures) {
		fprintf(stderr, "linked list: %d failure(s)\n", failures);
		return EXIT_FAILURE;
	}
	printf("linked list: %d checks passed\n", checks);
	return EXIT_SUCCESS;
}
