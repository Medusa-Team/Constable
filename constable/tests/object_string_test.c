// SPDX-License-Identifier: GPL-2.0

#include "object.h"

#include <stdio.h>
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

static void test_get_string(void)
{
	char object_data[4] = { 'a', 'b', 'c', 'd' };
	char output[3] = { 'x', 'x', 'x' };
	struct object_s object = {
		.data = object_data,
	};
	struct medusa_attribute_s attribute = {
		.offset = 0,
		.length = sizeof(object_data),
		.type = MED_TYPE_STRING,
		.name = "name",
	};

	EXPECT_TRUE(object_get_val(&object, &attribute, output,
				   sizeof(output)) == 0,
		    "an unterminated object string is read within its field");
	EXPECT_TRUE(memcmp(output, "ab\0", sizeof(output)) == 0,
		    "object string output reserves its final NUL");
}

static void test_set_string(void)
{
	char object_data[5] = { 'x', 'x', 'x', 'x', 'G' };
	char input[] = "abcdef";
	struct object_s object = {
		.data = object_data,
	};
	struct medusa_attribute_s attribute = {
		.offset = 0,
		.length = 4,
		.type = MED_TYPE_STRING,
		.name = "name",
	};

	EXPECT_TRUE(object_set_val(&object, &attribute, input,
				   sizeof(input)) == 0,
		    "an oversized object string is safely truncated");
	EXPECT_TRUE(memcmp(object_data, "abc\0", 4) == 0,
		    "a stored object string is always terminated");
	EXPECT_TRUE(object_data[4] == 'G',
		    "storing an object string preserves the guard byte");

	attribute.length = 0;
	EXPECT_TRUE(object_set_val(&object, &attribute, input,
				   sizeof(input)) == -1,
		    "a zero-width string attribute is rejected");
	EXPECT_TRUE(object_data[4] == 'G',
		    "a rejected zero-width write preserves the guard byte");
}

int main(void)
{
	test_get_string();
	test_set_string();

	if (failures) {
		fprintf(stderr, "object strings: %d failure(s)\n", failures);
		return 1;
	}

	printf("object strings: %d checks passed\n", checks);
	return 0;
}
