/* SPDX-License-Identifier: GPL-2.0 */
#ifndef CONSTABLE_FALLBACK_POLICY_H
#define CONSTABLE_FALLBACK_POLICY_H

#include <stddef.h>
#include <stdint.h>

#include "medusa_object.h"

struct fallback_policy_config {
	char event[MEDUSA_OPNAME_MAX];
	uint8_t policy;
};

int fallback_policy_configure(char *const specs[], unsigned int count);
unsigned int fallback_policy_count(void);
const struct fallback_policy_config *fallback_policy_at(unsigned int index);
int fallback_policy_parse(const char *spec, struct fallback_policy_config *out);
uint8_t fallback_policy_for_event(const char *event);

#endif /* CONSTABLE_FALLBACK_POLICY_H */
