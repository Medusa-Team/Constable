/* SPDX-License-Identifier: GPL-2.0 */
#ifndef CONSTABLE_CLI_OPTIONS_H
#define CONSTABLE_CLI_OPTIONS_H

#include <stdbool.h>

enum constable_cli_result {
	CONSTABLE_CLI_OK = 0,
	CONSTABLE_CLI_HELP,
	CONSTABLE_CLI_UNKNOWN_OPTION,
	CONSTABLE_CLI_MISSING_ARGUMENT,
};

struct constable_cli_options {
	char *config_file;
	char *medusa_config_file;
	char *tree_debug_file;
	char *definition_debug_file;
	char *policy_event_self_test_comm;
	char *policy_historical_event_test_comm;
	char *policy_inspection_file;
	char *policy_validation_file;
	bool medusa_config_file_explicit;
	bool test_only;
	bool policy_self_test;
	bool debug_events;
};

enum constable_cli_result
constable_cli_parse(int argc, char *const argv[],
		    struct constable_cli_options *options,
		    const char **problem_argument);

#endif /* CONSTABLE_CLI_OPTIONS_H */
