/* SPDX-License-Identifier: GPL-2.0 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <linux/medusa.h>

#include "../domain_rule.h"

static int failures;

#define EXPECT_TRUE(condition)						\
	do {								\
		if (!(condition)) {					\
			fprintf(stderr, "%s:%d: expected %s\n",		\
				__FILE__, __LINE__, #condition);		\
			failures++;					\
		}							\
	} while (0)

static void parser_accepts_exact_and_wildcard_keys(void)
{
	struct domain_rule_config rule;

	EXPECT_TRUE(domain_rule_parse("ptrace:7:9:0x100000002=deny",
				      &rule) == 0);
	EXPECT_TRUE(!strcmp(rule.event, "ptrace"));
	EXPECT_TRUE(rule.subject_domain == 7);
	EXPECT_TRUE(rule.object_domain == 9);
	EXPECT_TRUE(rule.selector == 0x100000002ULL);
	EXPECT_TRUE(rule.answer == MEDUSA_ANSWER_DENY);
	EXPECT_TRUE(domain_rule_parse("ptrace:*:*:*=allow", &rule) == 0);
	EXPECT_TRUE(rule.subject_domain == MEDUSA_POLICY_DOMAIN_ANY);
	EXPECT_TRUE(rule.object_domain == MEDUSA_POLICY_DOMAIN_ANY);
	EXPECT_TRUE(rule.selector == MEDUSA_POLICY_SELECTOR_ANY);
	EXPECT_TRUE(rule.answer == MEDUSA_ANSWER_ALLOW);
}

static void parser_rejects_malformed_rules(void)
{
	struct domain_rule_config rule;

	EXPECT_TRUE(domain_rule_parse(NULL, &rule) == -EINVAL);
	EXPECT_TRUE(domain_rule_parse("ptrace:1:2=deny", &rule) == -EINVAL);
	EXPECT_TRUE(domain_rule_parse("ptrace:1:2:3=maybe", &rule) == -EINVAL);
	EXPECT_TRUE(domain_rule_parse("ptrace:x:2:3=deny", &rule) == -EINVAL);
	EXPECT_TRUE(domain_rule_parse("ptrace:1:2:3:4=deny", &rule) == -EINVAL);
}

static void configuration_rejects_duplicate_keys(void)
{
	char *duplicates[] = {
		"ptrace:1:2:3=allow",
		"ptrace:1:2:3=deny",
	};
	char *distinct[] = {
		"ptrace:1:2:3=allow",
		"ptrace:1:2:4=deny",
		"fork:1:2:3=deny",
	};

	EXPECT_TRUE(domain_rule_configure(duplicates, 2) == -EEXIST);
	EXPECT_TRUE(domain_rule_count() == 0);
	EXPECT_TRUE(domain_rule_configure(distinct, 3) == 0);
	EXPECT_TRUE(domain_rule_count() == 3);
	EXPECT_TRUE(domain_rule_count_for_event("ptrace") == 2);
	EXPECT_TRUE(domain_rule_count_for_event("fork") == 1);
}

int main(void)
{
	parser_accepts_exact_and_wildcard_keys();
	parser_rejects_malformed_rules();
	configuration_rejects_duplicate_keys();

	if (failures) {
		fprintf(stderr, "domain rule: %d failure(s)\n", failures);
		return 1;
	}
	puts("domain rule: all checks passed");
	return 0;
}
