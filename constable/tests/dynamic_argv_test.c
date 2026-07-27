// SPDX-License-Identifier: GPL-2.0

#include <mcompiler/dynamic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void expect(int condition, const char *message)
{
	if (condition)
		return;
	fprintf(stderr, "dynamic argv: %s\n", message);
	failures++;
}

static void test_contiguous_storage_growth(void)
{
	char expected[80][48];
	char large[601];
	darg_t *args;
	int i;

	args = darg_alloc(1);
	expect(args != NULL, "failed to allocate contiguous argv");
	if (!args)
		return;

	for (i = 0; i < 80; i++) {
		snprintf(expected[i], sizeof(expected[i]),
			 "argument-%02d-with-relocation-padding", i);
		expect(darg_append(args, expected[i]) == 1,
		       "append failed while growing contiguous argv");
	}

	for (i = 0; i < 80; i++)
		expect(strcmp(darg_argv(args)[i], expected[i]) == 0,
		       "buffer relocation changed an existing argument");

	memset(large, 'x', sizeof(large) - 1);
	large[sizeof(large) - 1] = '\0';
	expect(darg_append(args, large) == 1,
	       "argument larger than one buffer growth quantum was rejected");
	expect(strcmp(darg_argv(args)[80], large) == 0,
	       "large argument was truncated or corrupted");

	expect(darg_insert(args, 17, "inserted") == 1,
	       "insert failed after pointer-array growth");
	expect(strcmp(darg_argv(args)[17], "inserted") == 0,
	       "inserted argument was not placed at the requested position");
	expect(strcmp(darg_argv(args)[18], expected[17]) == 0,
	       "insert did not preserve the following argument");

	expect(darg_delete(args, 17) == 1, "delete failed");
	expect(strcmp(darg_argv(args)[17], expected[17]) == 0,
	       "delete did not restore argument ordering");
	expect(darg_argv(args)[darg_argc(args)] == NULL,
	       "argv is not NULL terminated");

	darg_free(args);
}

static void test_individual_storage_growth(void)
{
	darg_t *args;
	int i;

	args = darg_alloc(0);
	expect(args != NULL, "failed to allocate individual-storage argv");
	if (!args)
		return;

	for (i = 0; i < 80; i++)
		expect(darg_append(args, "separate") == 1,
		       "append failed while growing pointer array");
	for (i = 0; i < 80; i++)
		expect(strcmp(darg_argv(args)[i], "separate") == 0,
		       "pointer-array growth changed an argument");

	darg_free(args);
}

int main(void)
{
	test_contiguous_storage_growth();
	test_individual_storage_growth();

	if (failures) {
		fprintf(stderr, "dynamic argv: %d failure(s)\n", failures);
		return EXIT_FAILURE;
	}

	puts("dynamic argv: growth and relocation checks passed");
	return EXIT_SUCCESS;
}
