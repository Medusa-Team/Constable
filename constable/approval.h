/* SPDX-License-Identifier: GPL-2.0 */
#ifndef CONSTABLE_APPROVAL_H
#define CONSTABLE_APPROVAL_H

#include <stdint.h>
#include <sys/types.h>

struct comm_buffer_s;

int approval_configure(const char *socket_path, const char *events,
		       const char *uid_text, const char *timeout_text);
int approval_configure_file(const char *socket_path, const char *events,
			    uid_t uid, unsigned int timeout);
int approval_enabled_for(const char *event_name);
int approval_is_configured(void);
int approval_decide(struct comm_buffer_s *request, uint64_t request_id,
		    const char *event_name, int policy_result);

#endif
