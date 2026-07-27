/* SPDX-License-Identifier: GPL-2.0 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

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
	EXPECT_TRUE(config.policy == MEDUSA_COMM_FALLBACK_BASELINE_ALLOW);
	EXPECT_TRUE(fallback_policy_parse("open=baseline_deny", &config) == 0);
	EXPECT_TRUE(config.policy == MEDUSA_COMM_FALLBACK_BASELINE_DENY);
	EXPECT_TRUE(fallback_policy_parse("ptrace=online_required", &config) == 0);
	EXPECT_TRUE(config.policy == MEDUSA_COMM_FALLBACK_ONLINE_REQUIRED);
}

static void parser_rejects_ambiguous_input(void)
{
	struct fallback_policy_config config;
	char overlong[MEDUSA_COMM_OPNAME_MAX + 32];

	memset(overlong, 'a', sizeof(overlong));
	overlong[sizeof(overlong) - 3] = '=';
	overlong[sizeof(overlong) - 2] = 'x';
	overlong[sizeof(overlong) - 1] = '\0';
	EXPECT_TRUE(fallback_policy_parse(NULL, &config) == -EINVAL);
	EXPECT_TRUE(fallback_policy_parse("=baseline_deny", &config) == -EINVAL);
	EXPECT_TRUE(fallback_policy_parse("exec=", &config) == -EINVAL);
	EXPECT_TRUE(fallback_policy_parse("exec=baseline_deny=x", &config) ==
		    -EINVAL);
	EXPECT_TRUE(fallback_policy_parse("exec=allow", &config) == -EINVAL);
	EXPECT_TRUE(fallback_policy_parse(overlong, &config) == -ENAMETOOLONG);
}

static void configuration_rejects_duplicate_events(void)
{
	char *specs[] = {
		"exec=baseline_allow",
		"exec=online_required",
	};

	EXPECT_TRUE(fallback_policy_configure(specs, 2) == -EEXIST);
	EXPECT_TRUE(fallback_policy_count() == 0);
}

static void frame_encoder_is_exact_and_bounded(void)
{
	unsigned char frame[sizeof(MCPptr_t) +
			    sizeof(struct medusa_comm_fallback_policy_s)];
	MCPptr_t command = 0x0102030405060708ULL;
	MCPptr_t event = 0x1112131415161718ULL;
	MCPptr_t decoded;

	EXPECT_TRUE(fallback_policy_frame_encode(
			    command, event, MEDUSA_COMM_FALLBACK_BASELINE_DENY,
			    frame, sizeof(frame)) == 0);
	memcpy(&decoded, frame, sizeof(decoded));
	EXPECT_TRUE(decoded == command);
	memcpy(&decoded, frame + sizeof(MCPptr_t), sizeof(decoded));
	EXPECT_TRUE(decoded == event);
	EXPECT_TRUE(frame[sizeof(MCPptr_t) * 2] ==
		    MEDUSA_COMM_FALLBACK_BASELINE_DENY);
	EXPECT_TRUE(fallback_policy_frame_encode(
			    command, event, MEDUSA_COMM_FALLBACK_BASELINE_DENY,
			    frame, sizeof(frame) - 1) == -EINVAL);
	EXPECT_TRUE(fallback_policy_frame_encode(command, event, 3, frame,
						 sizeof(frame)) == -EINVAL);
}

int main(void)
{
	parser_accepts_documented_policies();
	parser_rejects_ambiguous_input();
	configuration_rejects_duplicate_events();
	frame_encoder_is_exact_and_bounded();

	if (failures) {
		fprintf(stderr, "fallback policy: %d failure(s)\n", failures);
		return 1;
	}
	puts("fallback policy: all checks passed");
	return 0;
}
