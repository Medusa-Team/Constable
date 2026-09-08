/* SPDX-License-Identifier: GPL-2.0 */

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cli_options.h"
#include "threading.h"

static bool option_is(const char *argument, const char *option)
{
	return strcmp(argument, option) == 0;
}

static enum constable_cli_result
option_argument(int argc, char *const argv[], int *index, char **value,
		const char **problem_argument)
{
	if (*index + 1 >= argc) {
		*problem_argument = argv[*index];
		return CONSTABLE_CLI_MISSING_ARGUMENT;
	}

	*value = argv[++(*index)];
	return CONSTABLE_CLI_OK;
}

static enum constable_cli_result
worker_count_argument(int argc, char *const argv[], int *index,
		      unsigned int *value, const char **problem_argument)
{
	char *argument;
	char *end;
	unsigned long parsed;
	enum constable_cli_result result;

	result = option_argument(argc, argv, index, &argument, problem_argument);
	if (result != CONSTABLE_CLI_OK)
		return result;
	if (strcmp(argument, "auto") == 0) {
		*value = CONSTABLE_WORKERS_AUTO;
		return CONSTABLE_CLI_OK;
	}

	errno = 0;
	parsed = strtoul(argument, &end, 10);
	if (errno || end == argument || *end != '\0' || parsed == 0 ||
	    parsed > CONSTABLE_MAX_WORKERS) {
		*problem_argument = argument;
		return CONSTABLE_CLI_INVALID_WORKER_COUNT;
	}
	*value = (unsigned int)parsed;
	return CONSTABLE_CLI_OK;
}

static enum constable_cli_result
fallback_argument(int argc, char *const argv[], int *index,
		  struct constable_cli_options *options,
		  const char **problem_argument)
{
	char **resized;
	char *value;
	size_t allocation;
	unsigned int capacity;
	enum constable_cli_result result;

	result = option_argument(argc, argv, index, &value, problem_argument);
	if (result != CONSTABLE_CLI_OK)
		return result;
	if (options->fallback_policy_count ==
	    options->fallback_policy_capacity) {
		if (options->fallback_policy_capacity > UINT_MAX / 2U)
			return CONSTABLE_CLI_OUT_OF_MEMORY;
		capacity = options->fallback_policy_capacity ?
			options->fallback_policy_capacity * 2U : 8U;
		if (__builtin_mul_overflow((size_t)capacity, sizeof(*resized),
				   &allocation))
			return CONSTABLE_CLI_OUT_OF_MEMORY;
		resized = realloc(options->fallback_policy_specs, allocation);
		if (!resized)
			return CONSTABLE_CLI_OUT_OF_MEMORY;
		options->fallback_policy_specs = resized;
		options->fallback_policy_capacity = capacity;
	}
	options->fallback_policy_specs[options->fallback_policy_count++] = value;
	return CONSTABLE_CLI_OK;
}

void constable_cli_options_destroy(struct constable_cli_options *options)
{
	if (!options)
		return;
	free(options->fallback_policy_specs);
	options->fallback_policy_specs = NULL;
	options->fallback_policy_count = 0;
	options->fallback_policy_capacity = 0;
}

enum constable_cli_result
constable_cli_parse(int argc, char *const argv[],
		    struct constable_cli_options *options,
		    const char **problem_argument)
{
	bool positional_only = false;
	enum constable_cli_result result;
	int index;

	if (!options || !problem_argument)
		return CONSTABLE_CLI_UNKNOWN_OPTION;

	*options = (struct constable_cli_options) {
		.config_file = "/etc/constable.conf",
		.medusa_config_file = "/etc/medusa.conf",
	};
	*problem_argument = NULL;

	for (index = 1; index < argc; index++) {
		char *argument = argv[index];

		if (!positional_only && option_is(argument, "--")) {
			positional_only = true;
			continue;
		}
		if (positional_only || argument[0] != '-') {
			options->config_file = argument;
			continue;
		}
		if (option_is(argument, "-h") || option_is(argument, "--help"))
			return CONSTABLE_CLI_HELP;
		if (option_is(argument, "-t")) {
			options->test_only = true;
			continue;
		}
		if (option_is(argument, "-T")) {
			options->test_only = true;
			options->policy_self_test = true;
			continue;
		}
		if (option_is(argument, "-E")) {
			result = option_argument(argc, argv, &index,
						 &options->policy_event_self_test_comm,
						 problem_argument);
			if (result != CONSTABLE_CLI_OK)
				return result;
			options->test_only = true;
			continue;
		}
		if (option_is(argument, "-H")) {
			result = option_argument(argc, argv, &index,
						 &options->policy_historical_event_test_comm,
						 problem_argument);
			if (result != CONSTABLE_CLI_OK)
				return result;
			options->test_only = true;
			continue;
		}
		if (option_is(argument, "-I")) {
			result = option_argument(argc, argv, &index,
						 &options->policy_inspection_file,
						 problem_argument);
			if (result != CONSTABLE_CLI_OK)
				return result;
			options->test_only = true;
			continue;
		}
		if (option_is(argument, "-V")) {
			result = option_argument(argc, argv, &index,
						 &options->policy_validation_file,
						 problem_argument);
			if (result != CONSTABLE_CLI_OK)
				return result;
			continue;
		}
		if (option_is(argument, "-d")) {
			result = option_argument(argc, argv, &index,
						 &options->tree_debug_file,
						 problem_argument);
			if (result != CONSTABLE_CLI_OK)
				return result;
			options->test_only = true;
			continue;
		}
		if (option_is(argument, "-D") || option_is(argument, "-DD")) {
			options->debug_events = option_is(argument, "-DD");
			result = option_argument(argc, argv, &index,
						 &options->definition_debug_file,
						 problem_argument);
			if (result != CONSTABLE_CLI_OK)
				return result;
			continue;
		}
		if (option_is(argument, "-c")) {
			result = option_argument(argc, argv, &index,
						 &options->medusa_config_file,
						 problem_argument);
			if (result != CONSTABLE_CLI_OK)
				return result;
			options->medusa_config_file_explicit = true;
			continue;
		}
		if (option_is(argument, "-F") ||
		    option_is(argument, "--fallback")) {
			result = fallback_argument(argc, argv, &index, options,
					   problem_argument);
			if (result != CONSTABLE_CLI_OK)
				return result;
			continue;
		}
		if (option_is(argument, "-R") ||
		    option_is(argument, "--domain-rule")) {
			if (options->domain_rule_count >=
			    CONSTABLE_MAX_DOMAIN_RULES) {
				*problem_argument = argument;
				return CONSTABLE_CLI_TOO_MANY_DOMAIN_RULES;
			}
			result = option_argument(
				argc, argv, &index,
				&options->domain_rule_specs[
					options->domain_rule_count],
				problem_argument);
			if (result != CONSTABLE_CLI_OK)
				return result;
			options->domain_rule_count++;
			continue;
		}
		if (option_is(argument, "--approval-socket")) {
			result = option_argument(argc, argv, &index,
						 &options->approval_socket,
						 problem_argument);
			if (result != CONSTABLE_CLI_OK)
				return result;
			continue;
		}
		if (option_is(argument, "--approval-events")) {
			result = option_argument(argc, argv, &index,
						 &options->approval_events,
						 problem_argument);
			if (result != CONSTABLE_CLI_OK)
				return result;
			continue;
		}
		if (option_is(argument, "--approval-uid")) {
			result = option_argument(argc, argv, &index,
						 &options->approval_uid,
						 problem_argument);
			if (result != CONSTABLE_CLI_OK)
				return result;
			continue;
		}
		if (option_is(argument, "--approval-timeout")) {
			result = option_argument(argc, argv, &index,
						 &options->approval_timeout,
						 problem_argument);
			if (result != CONSTABLE_CLI_OK)
				return result;
			continue;
		}
		if (option_is(argument, "--workers")) {
			result = worker_count_argument(argc, argv, &index,
						       &options->worker_count,
						       problem_argument);
			if (result != CONSTABLE_CLI_OK)
				return result;
			continue;
		}

		*problem_argument = argument;
		return CONSTABLE_CLI_UNKNOWN_OPTION;
	}

	return CONSTABLE_CLI_OK;
}
