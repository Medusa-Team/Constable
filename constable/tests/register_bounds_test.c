// SPDX-License-Identifier: GPL-2.0

#include "language/execute.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static int checks;
static int runtime_errors;

#define EXPECT_TRUE(condition, description)					\
	do {									\
		checks++;							\
		if (!(condition)) {						\
			fprintf(stderr, "FAIL: %s\n", description);		\
			failures++;						\
		}								\
	} while (0)

int runtime(const char *format, ...)
{
	(void)format;
	runtime_errors++;
	return -1;
}

static void test_large_external_register(void)
{
	struct medusa_attribute_s attribute = {
		.offset = 0,
		.length = UINT16_MAX,
		.type = MED_TYPE_STRING,
		.name = "large",
	};
	struct register_s reg = {
		.attr = &attribute,
	};
	char *source = malloc(UINT16_MAX);

	EXPECT_TRUE(source != NULL, "large source allocation succeeds");
	if (!source)
		return;
	memset(source, 'x', UINT16_MAX);
	reg.data = source;

	runtime_errors = 0;
	r_imm(&reg);
	EXPECT_TRUE(runtime_errors == 1,
		    "oversized register materialization is diagnosed");
	EXPECT_TRUE(reg.data == reg.buf,
		    "oversized external data is materialized locally");
	EXPECT_TRUE(reg.attr == &reg.tmp_attr &&
		    reg.attr->length == MAX_REG_SIZE,
		    "materialized register length is capped to its real buffer");
	EXPECT_TRUE(reg.buf[MAX_REG_SIZE - 1] == '\0',
		    "capped string materialization remains terminated");
	free(source);
}

static void test_resize_rejection(void)
{
	struct medusa_attribute_s attribute = {
		.offset = 0,
		.length = 4,
		.type = MED_TYPE_UNSIGNED,
		.name = "number",
	};
	struct register_s reg = {
		.attr = &attribute,
	};

	memset(reg.buf, 0x5a, sizeof(reg.buf));
	reg.data = reg.buf;
	runtime_errors = 0;
	r_resize(&reg, MAX_REG_SIZE + 1);
	EXPECT_TRUE(runtime_errors == 1,
		    "an oversized register resize is diagnosed");
	EXPECT_TRUE((unsigned char)reg.buf[MAX_REG_SIZE - 1] == 0x5a,
		    "a rejected resize preserves the register guard byte");
}

int main(void)
{
	test_large_external_register();
	test_resize_rejection();

	if (failures) {
		fprintf(stderr, "register bounds: %d failure(s)\n", failures);
		return 1;
	}
	printf("register bounds: %d checks passed\n", checks);
	return 0;
}
