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
	EXPECT_TRUE(options.worker_count == 0);
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
		"-R", "ptrace:1:2:3=deny",
		"--domain-rule", "ptrace:*:*:*=allow",
		"--approval-socket", "/run/user/1000/medusa.sock",
		"--approval-events", "exec,ptrace",
		"--approval-uid", "1000", "--approval-timeout", "90",
		"--workers", "8",
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
	EXPECT_TRUE(options.domain_rule_count == 2);
	EXPECT_STRING("ptrace:1:2:3=deny", options.domain_rule_specs[0]);
	EXPECT_STRING("ptrace:*:*:*=allow", options.domain_rule_specs[1]);
	EXPECT_STRING("/run/user/1000/medusa.sock", options.approval_socket);
	EXPECT_STRING("exec,ptrace", options.approval_events);
	EXPECT_STRING("1000", options.approval_uid);
	EXPECT_STRING("90", options.approval_timeout);
	EXPECT_TRUE(options.worker_count == 8);
	EXPECT_STRING("constable.conf", options.config_file);
	EXPECT_TRUE(problem == NULL);
}

static void options_require_exact_names(void)
{
	char *argv[] = { "constable", "-trash" };
	struct constable_cli_options options;
	const char *problem;

	EXPECT_TRUE(parse(2, argv, NULL, &problem) ==
		    CONSTABLE_CLI_UNKNOWN_OPTION);
	EXPECT_TRUE(parse(2, argv, &options, NULL) ==
		    CONSTABLE_CLI_UNKNOWN_OPTION);
	EXPECT_TRUE(parse(2, argv, &options, &problem) ==
		    CONSTABLE_CLI_UNKNOWN_OPTION);
	EXPECT_STRING("-trash", problem);
}

static void value_options_require_an_argument(void)
{
	const char *options_requiring_values[] = {
		"-E", "-H", "-I", "-V", "-d", "-D", "-DD", "-c",
		"-F", "--fallback", "-R", "--domain-rule",
		"--approval-socket", "--approval-events", "--approval-uid",
		"--approval-timeout", "--workers",
	};
	struct constable_cli_options options;
	const char *problem;
	size_t index;

	for (index = 0;
	     index < sizeof(options_requiring_values) /
			     sizeof(options_requiring_values[0]);
	     index++) {
		char *argv[] = { "constable",
				 (char *)options_requiring_values[index] };

		EXPECT_TRUE(parse(2, argv, &options, &problem) ==
			    CONSTABLE_CLI_MISSING_ARGUMENT);
		EXPECT_STRING(options_requiring_values[index], problem);
	}
}

static void worker_count_is_bounded(void)
{
	char *automatic[] = { "constable", "--workers", "auto" };
	char *one[] = { "constable", "--workers", "1" };
	char *maximum[] = { "constable", "--workers", "32" };
	char *zero[] = { "constable", "--workers", "0" };
	char *too_many[] = { "constable", "--workers", "33" };
	char *invalid[] = { "constable", "--workers", "four" };
	struct constable_cli_options options;
	const char *problem;

	EXPECT_TRUE(parse(3, automatic, &options, &problem) == CONSTABLE_CLI_OK);
	EXPECT_TRUE(options.worker_count == 0);
	EXPECT_TRUE(parse(3, one, &options, &problem) == CONSTABLE_CLI_OK);
	EXPECT_TRUE(options.worker_count == 1);
	EXPECT_TRUE(parse(3, maximum, &options, &problem) == CONSTABLE_CLI_OK);
	EXPECT_TRUE(options.worker_count == 32);
	EXPECT_TRUE(parse(3, zero, &options, &problem) ==
		    CONSTABLE_CLI_INVALID_WORKER_COUNT);
	EXPECT_STRING("0", problem);
	EXPECT_TRUE(parse(3, too_many, &options, &problem) ==
		    CONSTABLE_CLI_INVALID_WORKER_COUNT);
	EXPECT_STRING("33", problem);
	EXPECT_TRUE(parse(3, invalid, &options, &problem) ==
		    CONSTABLE_CLI_INVALID_WORKER_COUNT);
	EXPECT_STRING("four", problem);
}

static void help_and_positional_separator_are_supported(void)
{
	char *short_help[] = { "constable", "-h" };
	char *long_help[] = { "constable", "--help" };
	char *positional[] = {
		"constable", "first.conf", "--", "-policy.conf", "last.conf",
	};
	char *debug[] = { "constable", "-D", "events.log" };
	struct constable_cli_options options;
	const char *problem;

	EXPECT_TRUE(parse(2, short_help, &options, &problem) ==
		    CONSTABLE_CLI_HELP);
	EXPECT_TRUE(parse(2, long_help, &options, &problem) ==
		    CONSTABLE_CLI_HELP);
	EXPECT_TRUE(parse(5, positional, &options, &problem) ==
		    CONSTABLE_CLI_OK);
	EXPECT_STRING("last.conf", options.config_file);
	EXPECT_TRUE(parse(3, debug, &options, &problem) == CONSTABLE_CLI_OK);
	EXPECT_STRING("events.log", options.definition_debug_file);
	EXPECT_TRUE(!options.debug_events);
}

static void repeated_policy_options_are_bounded(void)
{
	char *fallback_argv[2 * CONSTABLE_MAX_FALLBACK_POLICIES + 2];
	char *domain_argv[2 * CONSTABLE_MAX_DOMAIN_RULES + 2];
	struct constable_cli_options options;
	const char *problem;
	unsigned int index;

	fallback_argv[0] = "constable";
	for (index = 0; index < CONSTABLE_MAX_FALLBACK_POLICIES; index++) {
		fallback_argv[1 + 2 * index] = "-F";
		fallback_argv[2 + 2 * index] = "exec=baseline_allow";
	}
	fallback_argv[1 + 2 * CONSTABLE_MAX_FALLBACK_POLICIES] = "-F";
	EXPECT_TRUE(parse(2 * CONSTABLE_MAX_FALLBACK_POLICIES + 2,
			  fallback_argv, &options, &problem) ==
		    CONSTABLE_CLI_TOO_MANY_FALLBACKS);
	EXPECT_STRING("-F", problem);
	EXPECT_TRUE(options.fallback_policy_count ==
		    CONSTABLE_MAX_FALLBACK_POLICIES);

	domain_argv[0] = "constable";
	for (index = 0; index < CONSTABLE_MAX_DOMAIN_RULES; index++) {
		domain_argv[1 + 2 * index] = "-R";
		domain_argv[2 + 2 * index] = "exec:*:*:*=allow";
	}
	domain_argv[1 + 2 * CONSTABLE_MAX_DOMAIN_RULES] = "-R";
	EXPECT_TRUE(parse(2 * CONSTABLE_MAX_DOMAIN_RULES + 2, domain_argv,
			  &options, &problem) ==
		    CONSTABLE_CLI_TOO_MANY_DOMAIN_RULES);
	EXPECT_STRING("-R", problem);
	EXPECT_TRUE(options.domain_rule_count == CONSTABLE_MAX_DOMAIN_RULES);
}

int main(void)
{
	defaults_are_explicit();
	documented_options_are_parsed();
	options_require_exact_names();
	value_options_require_an_argument();
	worker_count_is_bounded();
	help_and_positional_separator_are_supported();
	repeated_policy_options_are_bounded();

	if (failures) {
		fprintf(stderr, "cli options: %d failure(s)\n", failures);
		return 1;
	}
	puts("cli options: all checks passed");
	return 0;
}
