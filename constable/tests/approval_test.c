/* SPDX-License-Identifier: GPL-2.0 */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>

#include "../approval.h"
#include "../comm.h"
#include "../decision.h"
#include "../mcp/mcp.h"

static int failures;

#define EXPECT_TRUE(condition)						\
	do {								\
		if (!(condition)) {					\
			fprintf(stderr, "%s:%d: expected %s\n",		\
				__FILE__, __LINE__, #condition);		\
			failures++;					\
		}							\
	} while (0)

int mcp_renew_authrequest(struct comm_buffer_s *request)
{
	(void)request;
	return 0;
}

bool mcp_authrequest_cancelled(struct comm_buffer_s *request)
{
	(void)request;
	return false;
}

int main(void)
{
	char long_path[sizeof(((struct sockaddr_un *)0)->sun_path) + 1];
	char long_events[1025];
	char uid[32];

	memset(long_path, 'p', sizeof(long_path) - 1);
	long_path[sizeof(long_path) - 1] = '\0';
	memset(long_events, 'e', sizeof(long_events) - 1);
	long_events[sizeof(long_events) - 1] = '\0';
	snprintf(uid, sizeof(uid), "%lu", (unsigned long)getuid());

	EXPECT_TRUE(approval_configure(NULL, NULL, NULL, NULL) == 0);
	EXPECT_TRUE(!approval_is_configured());
	EXPECT_TRUE(approval_configure_file(NULL, "exec", getuid(), 30) ==
		    -EINVAL);
	EXPECT_TRUE(approval_configure_file("", "exec", getuid(), 30) ==
		    -EINVAL);
	EXPECT_TRUE(approval_configure_file("/tmp/approval.sock", "", getuid(),
					    30) == -EINVAL);
	EXPECT_TRUE(approval_configure_file(long_path, "exec", getuid(), 30) ==
		    -EINVAL);
	EXPECT_TRUE(approval_configure_file("/tmp/approval.sock", long_events,
					    getuid(), 30) == -EINVAL);
	EXPECT_TRUE(approval_configure_file("/tmp/approval.sock", "exec",
					    getuid(), 0) == -EINVAL);
	EXPECT_TRUE(approval_configure_file("/tmp/approval.sock", "exec",
					    getuid(), 3601) == -EINVAL);

	EXPECT_TRUE(approval_configure("/tmp/approval.sock", "exec,ptrace",
				       NULL, NULL) == -EINVAL);
	EXPECT_TRUE(approval_configure("/tmp/approval.sock", "exec,ptrace",
				       "invalid", NULL) == -EINVAL);
	EXPECT_TRUE(approval_configure("/tmp/approval.sock", "exec,ptrace", uid,
				       "0") == -EINVAL);
	EXPECT_TRUE(approval_configure("/tmp/approval.sock", "exec,ptrace", uid,
				       "3601") == -EINVAL);
	EXPECT_TRUE(approval_configure("/tmp/approval.sock", "exec,ptrace", uid,
				       "invalid") == -EINVAL);
	EXPECT_TRUE(approval_configure("/tmp/approval.sock",
				       "exec,ptrace,exec\n", uid,
				       "30") == 0);
	EXPECT_TRUE(approval_is_configured());
	EXPECT_TRUE(approval_enabled_for("exec"));
	EXPECT_TRUE(approval_enabled_for("ptrace"));
	EXPECT_TRUE(!approval_enabled_for("unlink"));
	EXPECT_TRUE(!approval_enabled_for(NULL));

	/* A CLI configuration has precedence over later file configuration. */
	EXPECT_TRUE(approval_configure_file("/tmp/other.sock", "*", getuid(),
					    60) == 0);
	EXPECT_TRUE(approval_enabled_for("exec"));
	EXPECT_TRUE(!approval_enabled_for("unlink"));

	/* Unselected events retain the policy decision without agent I/O. */
	EXPECT_TRUE(approval_decide(NULL, 7, "unlink", MED_ALLOW) == MED_ALLOW);
	EXPECT_TRUE(approval_decide(NULL, 7, "unlink", MED_DENY) == MED_DENY);
	/* Selected malformed events and unavailable agents fail closed. */
	EXPECT_TRUE(approval_decide(NULL, 7, "exec\n", MED_ALLOW) == MED_DENY);
	EXPECT_TRUE(approval_decide(NULL, 7, "exec", MED_ALLOW) == MED_DENY);

	if (failures) {
		fprintf(stderr, "approval: %d failure(s)\n", failures);
		return 1;
	}
	puts("approval: all checks passed");
	return 0;
}
