/* SPDX-License-Identifier: GPL-2.0 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "../cli_options.h"
#include "../fallback_policy.h"

static int failures;

#define EXPECT_TRUE(condition)						\
	do {								\
		if (!(condition)) {					\
			fprintf(stderr, "%s:%d: expected %s\n",		\
				__FILE__, __LINE__, #condition);		\
			failures++;					\
		}							\
	} while (0)

static void parser_accepts_documented_policies(void)
{
	struct fallback_policy_config config;

	EXPECT_TRUE(fallback_policy_parse("exec=baseline_allow", &config) == 0);
	EXPECT_TRUE(!strcmp(config.event, "exec"));
	EXPECT_TRUE(config.policy == MEDUSA_FALLBACK_BASELINE_ALLOW);
	EXPECT_TRUE(fallback_policy_parse("open=baseline_deny", &config) == 0);
	EXPECT_TRUE(config.policy == MEDUSA_FALLBACK_BASELINE_DENY);
	EXPECT_TRUE(fallback_policy_parse("ptrace=online_required", &config) == 0);
	EXPECT_TRUE(config.policy == MEDUSA_FALLBACK_ONLINE_REQUIRED);
}

static void parser_rejects_ambiguous_input(void)
{
	struct fallback_policy_config config;
	char overlong[MEDUSA_OPNAME_MAX + 32];

	memset(overlong, 'a', sizeof(overlong));
	overlong[sizeof(overlong) - 3] = '=';
	overlong[sizeof(overlong) - 2] = 'x';
	overlong[sizeof(overlong) - 1] = '\0';
	EXPECT_TRUE(fallback_policy_parse(NULL, &config) == -EINVAL);
	EXPECT_TRUE(fallback_policy_parse("exec=baseline_deny", NULL) ==
		    -EINVAL);
	EXPECT_TRUE(fallback_policy_parse("", &config) == -EINVAL);
	EXPECT_TRUE(fallback_policy_parse("=baseline_deny", &config) == -EINVAL);
	EXPECT_TRUE(fallback_policy_parse("exec=", &config) == -EINVAL);
	EXPECT_TRUE(fallback_policy_parse("exec=baseline_deny=x", &config) ==
		    -EINVAL);
	EXPECT_TRUE(fallback_policy_parse("exec=allow", &config) == -EINVAL);
	EXPECT_TRUE(fallback_policy_parse(overlong, &config) == -ENAMETOOLONG);
}

static void configuration_is_atomic_and_indexed(void)
{
	char *specs[] = {
		"exec=baseline_allow",
		"exec=online_required",
	};
	char *valid[] = {
		"exec=baseline_deny",
		"ptrace=online_required",
	};
	char *invalid[] = {
		"exec=baseline_allow",
		"broken",
	};
	const struct fallback_policy_config *config;

	EXPECT_TRUE(fallback_policy_configure(
			    NULL, CONSTABLE_MAX_FALLBACK_POLICIES + 1) ==
		    -E2BIG);
	EXPECT_TRUE(fallback_policy_configure(specs, 2) == -EEXIST);
	EXPECT_TRUE(fallback_policy_count() == 0);
	EXPECT_TRUE(fallback_policy_configure(valid, 2) == 0);
	EXPECT_TRUE(fallback_policy_count() == 2);
	config = fallback_policy_at(1);
	EXPECT_TRUE(config != NULL);
	EXPECT_TRUE(config && !strcmp(config->event, "ptrace"));
	EXPECT_TRUE(config &&
		    config->policy == MEDUSA_FALLBACK_ONLINE_REQUIRED);
	EXPECT_TRUE(fallback_policy_at(2) == NULL);
	EXPECT_TRUE(fallback_policy_at(UINT_MAX) == NULL);

	EXPECT_TRUE(fallback_policy_configure(invalid, 2) == -EINVAL);
	EXPECT_TRUE(fallback_policy_count() == 2);
	EXPECT_TRUE(fallback_policy_at(1) &&
		    fallback_policy_at(1)->policy ==
			    MEDUSA_FALLBACK_ONLINE_REQUIRED);

	EXPECT_TRUE(fallback_policy_configure(NULL, 0) == 0);
	EXPECT_TRUE(fallback_policy_count() == 0);
	EXPECT_TRUE(fallback_policy_at(0) == NULL);
}

static void event_lookup_has_explicit_default(void)
{
	char *specs[] = {
		"exec=baseline_deny",
	};

	EXPECT_TRUE(fallback_policy_configure(specs, 1) == 0);
	EXPECT_TRUE(fallback_policy_for_event("exec") ==
		    MEDUSA_FALLBACK_BASELINE_DENY);
	EXPECT_TRUE(fallback_policy_for_event("unconfigured") ==
		    MEDUSA_FALLBACK_BASELINE_ALLOW);
}

int main(void)
{
	parser_accepts_documented_policies();
	parser_rejects_ambiguous_input();
	configuration_is_atomic_and_indexed();
	event_lookup_has_explicit_default();

	if (failures) {
		fprintf(stderr, "fallback policy: %d failure(s)\n", failures);
		return 1;
	}
	puts("fallback policy: all checks passed");
	return 0;
}
