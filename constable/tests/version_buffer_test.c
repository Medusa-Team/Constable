// SPDX-License-Identifier: GPL-2.0

#include <mcompiler/compiler.h>
#include <mcompiler/dynamic.h>

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

int main(void)
{
	struct {
		char output[8];
		char guard;
	} compiler = { .guard = 'C' }, dynamic = { .guard = 'D' };

	EXPECT_TRUE(compiler_verlib(compiler.output,
				    sizeof(compiler.output)) == 0x0100,
		    "compiler version number remains stable");
	EXPECT_TRUE(compiler.output[sizeof(compiler.output) - 1] == '\0',
		    "compiler version text is terminated when truncated");
	EXPECT_TRUE(compiler.guard == 'C',
		    "compiler version text preserves its guard byte");

	EXPECT_TRUE(lds_verlib(dynamic.output,
			       sizeof(dynamic.output)) == 0x0013,
		    "dynamic-library version number remains stable");
	EXPECT_TRUE(dynamic.output[sizeof(dynamic.output) - 1] == '\0',
		    "dynamic-library version text is terminated when truncated");
	EXPECT_TRUE(dynamic.guard == 'D',
		    "dynamic-library version text preserves its guard byte");

	EXPECT_TRUE(dl_verlib(NULL, 0) == 0x0013,
		    "the historical alias accepts no output buffer");

	if (failures) {
		fprintf(stderr, "version buffers: %d failure(s)\n", failures);
		return 1;
	}

	printf("version buffers: %d checks passed\n", checks);
	return 0;
}
