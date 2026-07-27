/* SPDX-License-Identifier: GPL-2.0 */

#include <stddef.h>
#include <string.h>

#include "cli_options.h"

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
			if (options->fallback_policy_count >=
			    CONSTABLE_MAX_FALLBACK_POLICIES) {
				*problem_argument = argument;
				return CONSTABLE_CLI_TOO_MANY_FALLBACKS;
			}
			result = option_argument(
				argc, argv, &index,
				&options->fallback_policy_specs[
					options->fallback_policy_count],
				problem_argument);
			if (result != CONSTABLE_CLI_OK)
				return result;
			options->fallback_policy_count++;
			continue;
		}

		*problem_argument = argument;
		return CONSTABLE_CLI_UNKNOWN_OPTION;
	}

	return CONSTABLE_CLI_OK;
}
