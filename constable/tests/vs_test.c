// SPDX-License-Identifier: GPL-2.0

#include "../vs.h"
#include "../language/error.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

pthread_key_t errstr_key;
char *Out_of_vs = "Out of available VS";

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

static void expect_single_bit(const vs_t *set, unsigned int bit)
{
	size_t word;

	for (word = 0; word < VS_WORDS; word++) {
		vs_t expected = 0;

		if (word == bit / BITS_PER_VS_WORD)
			expected = (vs_t)1U << (bit % BITS_PER_VS_WORD);
		EXPECT_TRUE(set[word] == expected,
			    "an allocated space contains exactly its assigned bit");
	}
}

static void test_allocation_boundaries(void)
{
	char *thread_error = NULL;
	vs_t allocated[VS_WORDS];
	unsigned int bit;

	EXPECT_TRUE(pthread_key_create(&errstr_key, NULL) == 0,
		    "the allocation test creates its error key");
	EXPECT_TRUE(pthread_setspecific(errstr_key, &thread_error) == 0,
		    "the allocation test installs thread-local error storage");
	EXPECT_TRUE(vs_init() == 0, "virtual-space allocation initializes");

	for (bit = 0; bit < MAX_NUM_OF_VS; bit++) {
		EXPECT_TRUE(vs_alloc(allocated) == 0,
			    "every supported virtual-space bit is allocated");
		expect_single_bit(allocated, bit);
	}

	EXPECT_TRUE(allocated[VS_WORDS - 1] == UINT32_C(0x80000000),
		    "the final allocation uses the unsigned high bit");
	EXPECT_TRUE(vs_is_enough(MAX_NUM_OF_VS),
		    "the exact protocol space width is sufficient");
	EXPECT_TRUE(!vs_is_enough(MAX_NUM_OF_VS - 1),
		    "one fewer protocol bit is insufficient");
	EXPECT_TRUE(vs_alloc(allocated) == -1,
		    "allocation beyond the protocol limit is rejected");
	EXPECT_TRUE(thread_error == Out_of_vs,
		    "exhaustion reports the expected error");

	pthread_key_delete(errstr_key);
}

static void test_set_operations(void)
{
	vs_t first[VS_WORDS];
	vs_t second[VS_WORDS];
	vs_t result[VS_WORDS];

	vs_clear(first);
	vs_clear(second);
	first[0] = UINT32_C(0x80000001);
	second[0] = UINT32_C(0x80000000);
	second[VS_WORDS - 1] |= UINT32_C(0x00000002);

	vs_set(first, result);
	EXPECT_TRUE(result[0] == first[0], "space sets can be copied");
	vs_add(second, result);
	EXPECT_TRUE(result[VS_WORDS - 1] & UINT32_C(0x00000002),
		    "space sets can be combined");
	EXPECT_TRUE(vs_test(first, result),
		    "set intersection detects a shared high bit");
	EXPECT_TRUE(vs_issub(first, result),
		    "subset checks accept contained bits");

	vs_sub(first, result);
	EXPECT_TRUE(!vs_test(first, result),
		    "subtraction removes every source bit");
	vs_set(first, result);
	vs_mask(second, result);
	EXPECT_TRUE(result[0] == UINT32_C(0x80000000),
		    "masking keeps only intersecting bits");

	vs_clear(result);
	EXPECT_TRUE(vs_isclear(result), "a cleared set is empty");
	vs_fill(result);
	EXPECT_TRUE(vs_isfull(result), "a filled set is full");
	vs_invert(result);
	EXPECT_TRUE(vs_isclear(result), "inverting a full set clears it");
}

int main(void)
{
	test_allocation_boundaries();
	test_set_operations();

	if (failures) {
		fprintf(stderr, "virtual spaces: %d failure(s)\n", failures);
		return EXIT_FAILURE;
	}

	printf("virtual spaces: %d checks passed\n", checks);
	return EXIT_SUCCESS;
}
