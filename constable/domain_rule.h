/* SPDX-License-Identifier: GPL-2.0 */
#ifndef CONSTABLE_DOMAIN_RULE_H
#define CONSTABLE_DOMAIN_RULE_H

#include <stddef.h>
#include <stdint.h>

#include "medusa_object.h"

struct domain_rule_config {
	char event[MEDUSA_OPNAME_MAX];
	uint64_t subject_domain;
	uint64_t object_domain;
	uint64_t selector;
	uint8_t answer;
};

int domain_rule_configure(char *const specs[], unsigned int count);
unsigned int domain_rule_count(void);
const struct domain_rule_config *domain_rule_at(unsigned int index);
int domain_rule_parse(const char *spec, struct domain_rule_config *out);
unsigned int domain_rule_count_for_event(const char *event);

#endif
