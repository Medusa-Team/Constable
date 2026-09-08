/* SPDX-License-Identifier: GPL-2.0 */
#ifndef CONSTABLE_CLI_OPTIONS_H
#define CONSTABLE_CLI_OPTIONS_H

#include <stdbool.h>

#define CONSTABLE_MAX_DOMAIN_RULES 256

enum constable_cli_result {
	CONSTABLE_CLI_OK = 0,
	CONSTABLE_CLI_HELP,
	CONSTABLE_CLI_UNKNOWN_OPTION,
	CONSTABLE_CLI_MISSING_ARGUMENT,
	CONSTABLE_CLI_OUT_OF_MEMORY,
	CONSTABLE_CLI_TOO_MANY_DOMAIN_RULES,
	CONSTABLE_CLI_INVALID_WORKER_COUNT,
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
	char *approval_socket;
	char *approval_events;
	char *approval_uid;
	char *approval_timeout;
	unsigned int worker_count;
	char **fallback_policy_specs;
	unsigned int fallback_policy_count;
	unsigned int fallback_policy_capacity;
	char *domain_rule_specs[CONSTABLE_MAX_DOMAIN_RULES];
	unsigned int domain_rule_count;
	bool medusa_config_file_explicit;
	bool test_only;
	bool policy_self_test;
	bool debug_events;
};

enum constable_cli_result
constable_cli_parse(int argc, char *const argv[],
		    struct constable_cli_options *options,
		    const char **problem_argument);
void constable_cli_options_destroy(struct constable_cli_options *options);

#endif /* CONSTABLE_CLI_OPTIONS_H */
