/* SPDX-License-Identifier: GPL-2.0 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../cli_options.h"

static int failures;

#define EXPECT_TRUE(condition)                                                   \
	do {                                                                      \
		if (!(condition)) {                                                 \
			fprintf(stderr, "%s:%d: expected %s\n", __FILE__, __LINE__, \
				#condition);                                            \
			failures++;                                                   \
		}                                                                 \
	} while (0)

#define EXPECT_STRING(expected, actual)                                         \
	do {                                                                      \
		const char *actual_value = (actual);                                \
		if (!actual_value || strcmp((expected), actual_value) != 0) {       \
			fprintf(stderr, "%s:%d: expected \"%s\", got \"%s\"\n",    \
				__FILE__, __LINE__, (expected),                        \
				actual_value ? actual_value : "(null)");               \
			failures++;                                                   \
		}                                                                 \
	} while (0)

static enum constable_cli_result
parse(int argc, char *argv[], struct constable_cli_options *options,
      const char **problem_argument)
{
	return constable_cli_parse(argc, argv, options, problem_argument);
}

static void defaults_are_explicit(void)
{
	char *argv[] = { "constable" };
	struct constable_cli_options options;
	const char *problem;

	EXPECT_TRUE(parse(1, argv, &options, &problem) == CONSTABLE_CLI_OK);
	EXPECT_STRING("/etc/constable.conf", options.config_file);
	EXPECT_STRING("/etc/medusa.conf", options.medusa_config_file);
	EXPECT_TRUE(!options.test_only);
	EXPECT_TRUE(!options.policy_self_test);
	EXPECT_TRUE(!options.medusa_config_file_explicit);
	EXPECT_TRUE(problem == NULL);
}

static void documented_options_are_parsed(void)
{
	char *argv[] = {
		"constable", "-t", "-T", "-E", "event", "-H", "history",
		"-I", "policy.json", "-V", "/securityfs", "-d", "tree.log",
		"-DD", "events.log", "-c", "medusa.conf",
		"-F", "exec=baseline_deny",
		"--fallback", "ptrace=online_required",
		"--approval-socket", "/run/user/1000/medusa.sock",
		"--approval-events", "exec,ptrace",
		"--approval-uid", "1000", "--approval-timeout", "90",
		"constable.conf",
	};
	struct constable_cli_options options;
	const char *problem;

	EXPECT_TRUE(parse((int)(sizeof(argv) / sizeof(argv[0])), argv, &options,
			  &problem) == CONSTABLE_CLI_OK);
	EXPECT_TRUE(options.test_only);
	EXPECT_TRUE(options.policy_self_test);
	EXPECT_TRUE(options.debug_events);
	EXPECT_TRUE(options.medusa_config_file_explicit);
	EXPECT_STRING("event", options.policy_event_self_test_comm);
	EXPECT_STRING("history", options.policy_historical_event_test_comm);
	EXPECT_STRING("policy.json", options.policy_inspection_file);
	EXPECT_STRING("/securityfs", options.policy_validation_file);
	EXPECT_STRING("tree.log", options.tree_debug_file);
	EXPECT_STRING("events.log", options.definition_debug_file);
	EXPECT_STRING("medusa.conf", options.medusa_config_file);
	EXPECT_TRUE(options.fallback_policy_count == 2);
	EXPECT_STRING("exec=baseline_deny", options.fallback_policy_specs[0]);
	EXPECT_STRING("ptrace=online_required",
		      options.fallback_policy_specs[1]);
	EXPECT_STRING("/run/user/1000/medusa.sock", options.approval_socket);
	EXPECT_STRING("exec,ptrace", options.approval_events);
	EXPECT_STRING("1000", options.approval_uid);
	EXPECT_STRING("90", options.approval_timeout);
	EXPECT_STRING("constable.conf", options.config_file);
	EXPECT_TRUE(problem == NULL);
}

static void options_require_exact_names(void)
{
	char *argv[] = { "constable", "-trash" };
	struct constable_cli_options options;
	const char *problem;

	EXPECT_TRUE(parse(2, argv, &options, &problem) ==
		    CONSTABLE_CLI_UNKNOWN_OPTION);
	EXPECT_STRING("-trash", problem);
}

static void value_options_require_an_argument(void)
{
	char *argv[] = { "constable", "-V" };
	struct constable_cli_options options;
	const char *problem;

	EXPECT_TRUE(parse(2, argv, &options, &problem) ==
		    CONSTABLE_CLI_MISSING_ARGUMENT);
	EXPECT_STRING("-V", problem);
}

static void help_and_positional_separator_are_supported(void)
{
	char *help[] = { "constable", "--help" };
	char *positional[] = { "constable", "--", "-policy.conf" };
	struct constable_cli_options options;
	const char *problem;

	EXPECT_TRUE(parse(2, help, &options, &problem) == CONSTABLE_CLI_HELP);
	EXPECT_TRUE(parse(3, positional, &options, &problem) == CONSTABLE_CLI_OK);
	EXPECT_STRING("-policy.conf", options.config_file);
}

int main(void)
{
	defaults_are_explicit();
	documented_options_are_parsed();
	options_require_exact_names();
	value_options_require_an_argument();
	help_and_positional_separator_are_supported();

	if (failures) {
		fprintf(stderr, "cli options: %d failure(s)\n", failures);
		return 1;
	}
	puts("cli options: all checks passed");
	return 0;
}
