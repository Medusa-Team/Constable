// SPDX-License-Identifier: GPL-2.0

#include <mcompiler/checked_math.h>

#include <stdint.h>
#include <stdio.h>

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

int main(void)
{
	size_t result = 0;

	EXPECT_TRUE(checked_size_add(7, 9, &result) && result == 16,
		    "bounded addition returns its exact result");
	EXPECT_TRUE(!checked_size_add(SIZE_MAX, 1, &result),
		    "addition overflow is rejected");
	EXPECT_TRUE(checked_size_multiply(7, 9, &result) && result == 63,
		    "bounded multiplication returns its exact result");
	EXPECT_TRUE(checked_size_multiply(0, SIZE_MAX, &result) &&
		    result == 0,
		    "zero multiplication remains valid");
	EXPECT_TRUE(!checked_size_multiply(SIZE_MAX, 2, &result),
		    "multiplication overflow is rejected");

	if (failures) {
		fprintf(stderr, "checked arithmetic: %d failure(s)\n", failures);
		return 1;
	}
	printf("checked arithmetic: %d checks passed\n", checks);
	return 0;
}
