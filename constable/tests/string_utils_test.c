// SPDX-License-Identifier: GPL-2.0

#include "string_utils.h"

#include <errno.h>
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

static void test_regular_copy(void)
{
	char destination[8];

	EXPECT_TRUE(string_copy(destination, sizeof(destination), "medusa") == 0,
		    "a fitting C string is accepted");
	EXPECT_TRUE(strcmp(destination, "medusa") == 0,
		    "a fitting C string is copied");
	EXPECT_TRUE(string_copy(destination, sizeof(destination), "constable") ==
		    -ENAMETOOLONG, "an oversized C string is rejected");
	EXPECT_TRUE(destination[0] == '\0',
		    "a rejected C string leaves an empty destination");
}

static void test_fixed_field_copy(void)
{
	const char valid[8] = { 'm', 'e', 'd', 'u', 's', 'a', '\0', 'x' };
	const char unterminated[8] = { 'm', 'e', 'd', 'u', 's', 'a', '!', '!' };
	char destination[8];

	EXPECT_TRUE(string_copy_field(destination, sizeof(destination), valid,
				      sizeof(valid)) == 0,
		    "a terminated fixed-width field is accepted");
	EXPECT_TRUE(strcmp(destination, "medusa") == 0,
		    "a fixed-width field stops at its first NUL");
	EXPECT_TRUE(string_copy_field(destination, sizeof(destination),
				      unterminated,
				      sizeof(unterminated)) == -EINVAL,
		    "an unterminated fixed-width field is rejected");
	EXPECT_TRUE(destination[0] == '\0',
		    "a rejected fixed-width field leaves an empty destination");
}

static void test_line_formatting(void)
{
	struct {
		char before;
		char output[12];
		char after;
	} guarded = {
		.before = 'L',
		.after = 'R',
	};

	EXPECT_TRUE(string_format_line(guarded.output, sizeof(guarded.output),
				       "E: ", "%s", "bad") == 0,
		    "a fitting diagnostic is accepted");
	EXPECT_TRUE(strcmp(guarded.output, "E: bad\n") == 0,
		    "a diagnostic receives one trailing newline");
	EXPECT_TRUE(string_format_line(guarded.output, sizeof(guarded.output),
				       "Error: ", "%s",
				       "a message that does not fit") == -ENOSPC,
		    "an oversized diagnostic reports truncation");
	EXPECT_TRUE(guarded.output[sizeof(guarded.output) - 2] == '\n',
		    "a truncated diagnostic still ends in a newline");
	EXPECT_TRUE(guarded.output[sizeof(guarded.output) - 1] == '\0',
		    "a truncated diagnostic remains NUL-terminated");
	EXPECT_TRUE(guarded.before == 'L' && guarded.after == 'R',
		    "diagnostic formatting does not overwrite guard bytes");

	guarded.output[0] = 'x';
	EXPECT_TRUE(string_format_line(guarded.output, 1, "", "x") == -EINVAL,
		    "a one-byte diagnostic buffer is rejected");
	EXPECT_TRUE(guarded.output[0] == '\0',
		    "a rejected one-byte diagnostic buffer is terminated");
}

static void test_hex_byte(void)
{
	char encoded[3];

	string_hex_byte(encoded, 0x00);
	EXPECT_TRUE(strcmp(encoded, "00") == 0, "zero has two hex digits");
	string_hex_byte(encoded, 0xaf);
	EXPECT_TRUE(strcmp(encoded, "af") == 0, "hex letters are lowercase");
	string_hex_byte(encoded, 0xff);
	EXPECT_TRUE(strcmp(encoded, "ff") == 0,
		    "a high-bit byte is not sign-extended");
}

int main(void)
{
	test_regular_copy();
	test_fixed_field_copy();
	test_line_formatting();
	test_hex_byte();

	if (failures) {
		fprintf(stderr, "string utilities: %d failure(s)\n", failures);
		return 1;
	}

	printf("string utilities: %d checks passed\n", checks);
	return 0;
}
