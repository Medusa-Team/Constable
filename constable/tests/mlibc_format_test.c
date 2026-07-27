// SPDX-License-Identifier: GPL-2.0

#include "Mlibc/mlibc_format.h"

#include <limits.h>
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

static void test_supported_formats(void)
{
	char output[128];
	int count = -1;
	int result;

	result = mlibc_snprintf(output, sizeof(output),
				"x:%08x:%-5s:%d:%u:%%%n",
				0xafU, "ok", -7, 9U, &count);
	EXPECT_TRUE(!strcmp(output, "x:000000af:ok   :-7:9:%"),
		    "legacy integer, string, padding, and percent formats agree");
	EXPECT_TRUE(result == (int)strlen(output),
		    "a fitting format returns its stored length");
	EXPECT_TRUE(count == result, "%n records the generated length");

	result = mlibc_snprintf(output, sizeof(output), "%ld", LONG_MIN);
	EXPECT_TRUE(!strcmp(output, "-9223372036854775808"),
		    "the minimum signed long formats without overflow");
	EXPECT_TRUE(result == 20, "minimum signed long reports 20 characters");

	result = mlibc_snprintf(output, sizeof(output), "%#x %#X %#o",
				0x2aU, 0x2aU, 012U);
	EXPECT_TRUE(!strcmp(output, "0x2a 0X2A 012"),
		    "alternate hexadecimal and octal prefixes are preserved");
	EXPECT_TRUE(result == (int)strlen(output),
		    "alternate formats report their generated length");

	result = mlibc_snprintf(output, sizeof(output), "%5c:%.3s:%s",
				'x', "abcdef", NULL);
	EXPECT_TRUE(!strcmp(output, "    x:abc:<NULL>"),
		    "character width, string precision, and NULL strings work");
	EXPECT_TRUE(result == (int)strlen(output),
		    "string and character formats report their generated length");
}

static void test_truncation(void)
{
	struct {
		char output[8];
		char guard;
	} guarded = {
		.guard = 'G',
	};
	int result;

	result = mlibc_snprintf(guarded.output, sizeof(guarded.output),
				"0123456789");
	EXPECT_TRUE(result == 10, "truncation returns the required length");
	EXPECT_TRUE(!strcmp(guarded.output, "0123456"),
		    "truncation preserves room for a final NUL");
	EXPECT_TRUE(guarded.guard == 'G', "truncation preserves the guard byte");

	guarded.output[0] = 'x';
	result = mlibc_snprintf(guarded.output, 1, "content");
	EXPECT_TRUE(result == 7, "a one-byte buffer reports the required length");
	EXPECT_TRUE(guarded.output[0] == '\0',
		    "a one-byte buffer contains only its terminator");
	EXPECT_TRUE(guarded.guard == 'G',
		    "a one-byte buffer preserves its guard byte");

	result = mlibc_snprintf(NULL, 0, "%1000000u", 1U);
	if (result != 1000000)
		fprintf(stderr, "large-width result: %d\n", result);
	EXPECT_TRUE(result == 1000000,
		    "a large width is measured without an output buffer");
}

int main(void)
{
	test_supported_formats();
	test_truncation();

	if (failures) {
		fprintf(stderr, "Mlibc formatter: %d failure(s)\n", failures);
		return 1;
	}

	printf("Mlibc formatter: %d checks passed\n", checks);
	return 0;
}
