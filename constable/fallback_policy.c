/* SPDX-License-Identifier: GPL-2.0 */

#include <errno.h>
#include <string.h>

#include "cli_options.h"
#include "fallback_policy.h"

static struct fallback_policy_config
	configured_policies[CONSTABLE_MAX_FALLBACK_POLICIES];
static unsigned int configured_policy_count;

static int parse_policy(const char *name, uint8_t *policy)
{
	if (!strcmp(name, "baseline_allow"))
		*policy = MEDUSA_COMM_FALLBACK_BASELINE_ALLOW;
	else if (!strcmp(name, "baseline_deny"))
		*policy = MEDUSA_COMM_FALLBACK_BASELINE_DENY;
	else if (!strcmp(name, "online_required"))
		*policy = MEDUSA_COMM_FALLBACK_ONLINE_REQUIRED;
	else
		return -EINVAL;
	return 0;
}

int fallback_policy_parse(const char *spec, struct fallback_policy_config *out)
{
	const char *separator;
	size_t event_length;

	if (!spec || !out)
		return -EINVAL;
	separator = strchr(spec, '=');
	if (!separator || separator == spec || !separator[1] ||
	    strchr(separator + 1, '='))
		return -EINVAL;
	event_length = (size_t)(separator - spec);
	if (event_length >= sizeof(out->event))
		return -ENAMETOOLONG;
	memset(out, 0, sizeof(*out));
	memcpy(out->event, spec, event_length);
	return parse_policy(separator + 1, &out->policy);
}

int fallback_policy_configure(char *const specs[], unsigned int count)
{
	struct fallback_policy_config
		next[CONSTABLE_MAX_FALLBACK_POLICIES];
	unsigned int index;
	unsigned int previous;
	int error;

	if (count > CONSTABLE_MAX_FALLBACK_POLICIES)
		return -E2BIG;
	for (index = 0; index < count; index++) {
		error = fallback_policy_parse(specs[index], &next[index]);
		if (error)
			return error;
		for (previous = 0; previous < index; previous++)
			if (!strcmp(next[previous].event, next[index].event))
				return -EEXIST;
	}
	memcpy(configured_policies, next,
	       count * sizeof(configured_policies[0]));
	configured_policy_count = count;
	return 0;
}

unsigned int fallback_policy_count(void)
{
	return configured_policy_count;
}

const struct fallback_policy_config *fallback_policy_at(unsigned int index)
{
	if (index >= configured_policy_count)
		return NULL;
	return &configured_policies[index];
}

int fallback_policy_frame_encode(
	MCPptr_t command_wire, MCPptr_t event_wire, uint8_t policy,
	unsigned char *frame, size_t frame_size)
{
	if (!frame ||
	    frame_size != sizeof(MCPptr_t) +
			  sizeof(struct medusa_comm_fallback_policy_s) ||
	    policy > MEDUSA_COMM_FALLBACK_ONLINE_REQUIRED)
		return -EINVAL;
	memcpy(frame, &command_wire, sizeof(command_wire));
	memcpy(frame + sizeof(command_wire), &event_wire, sizeof(event_wire));
	frame[sizeof(command_wire) + sizeof(event_wire)] = policy;
	return 0;
}
