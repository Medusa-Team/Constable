/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>

#include "../approval.h"
#include "../comm.h"

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
	if (approval_configure("/tmp/approval.sock", "exec,ptrace", NULL,
			       NULL) == 0) {
		fprintf(stderr, "approval accepted a missing uid\n");
		return 1;
	}
	if (approval_configure("/tmp/approval.sock", "exec,ptrace", "1000",
			       "30") != 0) {
		fprintf(stderr, "approval rejected valid configuration\n");
		return 1;
	}
	if (!approval_enabled_for("exec") ||
	    !approval_enabled_for("ptrace") ||
	    approval_enabled_for("unlink")) {
		fprintf(stderr, "approval event selection is incorrect\n");
		return 1;
	}
	return 0;
}
