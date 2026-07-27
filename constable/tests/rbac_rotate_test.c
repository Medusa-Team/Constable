// SPDX-License-Identifier: GPL-2.0

#include "../rbac/rotate.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

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

static int write_text(const char *path, const char *text)
{
	FILE *file = fopen(path, "w");
	int failed;

	if (!file)
		return -1;
	failed = fputs(text, file) == EOF;
	if (fclose(file) != 0)
		failed = 1;
	return failed ? -1 : 0;
}

static int file_contains(const char *path, const char *expected)
{
	char buffer[64];
	FILE *file = fopen(path, "r");

	if (!file)
		return 0;
	if (!fgets(buffer, sizeof(buffer), file)) {
		fclose(file);
		return 0;
	}
	fclose(file);
	return strcmp(buffer, expected) == 0;
}

static void test_real_rotation(void)
{
	char directory[] = "/tmp/constable-rbac-rotate-XXXXXX";
	char base[128];
	char first[256];
	char second[256];
	char *created;

	created = mkdtemp(directory);
	EXPECT_TRUE(created != NULL,
		    "a temporary rotation directory is created");
	if (!created)
		return;
	snprintf(base, sizeof(base), "%s/policy", directory);
	snprintf(first, sizeof(first), "%s.1", base);
	snprintf(second, sizeof(second), "%s.2", base);
	EXPECT_TRUE(write_text(base, "current\n") == 0 &&
		    write_text(first, "previous-1\n") == 0 &&
		    write_text(second, "expired\n") == 0,
		    "rotation fixtures are written");
	EXPECT_TRUE(rbac_rotate_files(base, 2) == 0,
		    "two generations are rotated");
	EXPECT_TRUE(access(base, F_OK) != 0,
		    "the current file moves out of the base path");
	EXPECT_TRUE(file_contains(first, "current\n"),
		    "the current file becomes generation one");
	EXPECT_TRUE(file_contains(second, "previous-1\n"),
		    "generation one becomes generation two");

	EXPECT_TRUE(write_text(base, "discard\n") == 0 &&
		    rbac_rotate_files(base, 0) == 0 &&
		    access(base, F_OK) != 0,
		    "zero generations removes the current file");
	(void)unlink(first);
	(void)unlink(second);
	(void)rmdir(directory);
}

static void test_bounds(void)
{
	static const size_t long_length = 384 * 1024;
	struct rlimit stack_limit = {
		.rlim_cur = 256 * 1024,
		.rlim_max = 256 * 1024,
	};
	char *long_name;

	errno = 0;
	EXPECT_TRUE(rbac_rotate_files(NULL, 1) == -1 && errno == EINVAL,
		    "a null filename is rejected");
	errno = 0;
	EXPECT_TRUE(rbac_rotate_files("", 1) == -1 && errno == EINVAL,
		    "an empty filename is rejected");
	errno = 0;
	EXPECT_TRUE(rbac_rotate_files("policy", RBAC_ROTATION_LIMIT + 1) == -1 &&
		    errno == E2BIG,
		    "an excessive generation count is rejected");

	EXPECT_TRUE(setrlimit(RLIMIT_STACK, &stack_limit) == 0,
		    "the long-name regression uses a bounded stack");
	long_name = malloc(long_length + 1);
	EXPECT_TRUE(long_name != NULL, "a long filename is allocated");
	if (!long_name)
		return;
	memset(long_name, 'x', long_length);
	long_name[long_length] = '\0';
	errno = 0;
	EXPECT_TRUE(rbac_rotate_files(long_name, 1) == -1 &&
		    errno == ENAMETOOLONG,
		    "a filename larger than the stack limit fails without exhausting it");
	free(long_name);
}

int main(void)
{
	test_real_rotation();
	test_bounds();

	if (failures) {
		fprintf(stderr, "RBAC rotation: %d failure(s)\n", failures);
		return EXIT_FAILURE;
	}
	printf("RBAC rotation: %d checks passed\n", checks);
	return EXIT_SUCCESS;
}
