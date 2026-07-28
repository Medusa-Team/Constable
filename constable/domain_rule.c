/* SPDX-License-Identifier: GPL-2.0 */

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <linux/medusa.h>

#include "cli_options.h"
#include "domain_rule.h"

static struct domain_rule_config
	configured_rules[CONSTABLE_MAX_DOMAIN_RULES];
static unsigned int configured_rule_count;

static int parse_key(const char *text, uint64_t wildcard, uint64_t *value)
{
	char *end;
	unsigned long long parsed;

	if (!strcmp(text, "*")) {
		*value = wildcard;
		return 0;
	}
	errno = 0;
	parsed = strtoull(text, &end, 0);
	if (errno || end == text || *end)
		return -EINVAL;
	*value = (uint64_t)parsed;
	return 0;
}

int domain_rule_parse(const char *spec, struct domain_rule_config *out)
{
	char copy[MEDUSA_OPNAME_MAX + 96];
	char *answer;
	char *cursor;
	char *field;
	char *save;
	size_t length;
	int error;

	if (!spec || !out)
		return -EINVAL;
	length = strlen(spec);
	if (length >= sizeof(copy))
		return -ENAMETOOLONG;
	memcpy(copy, spec, length + 1);
	answer = strchr(copy, '=');
	if (!answer || answer == copy || !answer[1] || strchr(answer + 1, '='))
		return -EINVAL;
	*answer++ = '\0';
	if (!strcmp(answer, "allow"))
		out->answer = MEDUSA_ANSWER_ALLOW;
	else if (!strcmp(answer, "deny"))
		out->answer = MEDUSA_ANSWER_DENY;
	else
		return -EINVAL;

	memset(out->event, 0, sizeof(out->event));
	cursor = copy;
	field = strtok_r(cursor, ":", &save);
	if (!field || !field[0] || strlen(field) >= sizeof(out->event))
		return -EINVAL;
	strcpy(out->event, field);
	field = strtok_r(NULL, ":", &save);
	error = field ? parse_key(field, MEDUSA_POLICY_DOMAIN_ANY,
				  &out->subject_domain) : -EINVAL;
	field = strtok_r(NULL, ":", &save);
	if (!error)
		error = field ?
			parse_key(field, MEDUSA_POLICY_DOMAIN_ANY,
				  &out->object_domain) : -EINVAL;
	field = strtok_r(NULL, ":", &save);
	if (!error)
		error = field ?
			parse_key(field, MEDUSA_POLICY_SELECTOR_ANY,
				  &out->selector) : -EINVAL;
	if (error || strtok_r(NULL, ":", &save))
		return -EINVAL;
	return 0;
}

int domain_rule_configure(char *const specs[], unsigned int count)
{
	struct domain_rule_config next[CONSTABLE_MAX_DOMAIN_RULES];
	unsigned int index;
	unsigned int previous;
	int error;

	if (count > CONSTABLE_MAX_DOMAIN_RULES)
		return -E2BIG;
	for (index = 0; index < count; index++) {
		error = domain_rule_parse(specs[index], &next[index]);
		if (error)
			return error;
		for (previous = 0; previous < index; previous++)
			if (!strcmp(next[previous].event, next[index].event) &&
			    next[previous].subject_domain ==
				    next[index].subject_domain &&
			    next[previous].object_domain ==
				    next[index].object_domain &&
			    next[previous].selector == next[index].selector)
				return -EEXIST;
	}
	memcpy(configured_rules, next, count * sizeof(configured_rules[0]));
	configured_rule_count = count;
	return 0;
}

unsigned int domain_rule_count(void)
{
	return configured_rule_count;
}

const struct domain_rule_config *domain_rule_at(unsigned int index)
{
	if (index >= configured_rule_count)
		return NULL;
	return &configured_rules[index];
}

unsigned int domain_rule_count_for_event(const char *event)
{
	unsigned int count = 0;
	unsigned int index;

	for (index = 0; index < configured_rule_count; index++)
		if (!strcmp(configured_rules[index].event, event))
			count++;
	return count;
}
