/* SPDX-License-Identifier: GPL-2.0 */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "approval.h"
#include "comm.h"
#include "decision.h"
#include "mcp/mcp.h"

#define APPROVAL_RENEW_MS 2000
#define APPROVAL_LINE_MAX 512
#define APPROVAL_EVENTS_MAX 1024

static char approval_socket[sizeof(((struct sockaddr_un *)0)->sun_path)];
static char approval_events[APPROVAL_EVENTS_MAX];
static uid_t approval_uid;
static int approval_timeout = 60;
static bool approval_cli_override;

static bool event_selected(const char *name)
{
	const char *start;

	if (!approval_events[0] || !name)
		return false;
	if (!strcmp(approval_events, "*"))
		return true;
	for (start = approval_events; *start;) {
		const char *end = strchr(start, ',');
		size_t length = end ? (size_t)(end - start) : strlen(start);

		if (strlen(name) == length && !strncmp(start, name, length))
			return true;
		if (!end)
			break;
		start = end + 1;
	}
	return false;
}

static int approval_store(const char *socket_path, const char *events,
			  uid_t uid, unsigned int timeout)
{
	if (!socket_path || !events || !*socket_path || !*events ||
	    strlen(socket_path) >= sizeof(approval_socket) ||
	    strlen(events) >= sizeof(approval_events) ||
	    timeout < 1 || timeout > 3600)
		return -EINVAL;
	strcpy(approval_socket, socket_path);
	strcpy(approval_events, events);
	approval_uid = uid;
	approval_timeout = (int)timeout;
	return 0;
}

int approval_configure(const char *socket_path, const char *events,
		       const char *uid_text, const char *timeout_text)
{
	char *end;
	unsigned int timeout = 60;
	uid_t uid;
	unsigned long value;

	if (!socket_path && !events && !uid_text && !timeout_text)
		return 0;
	if (!socket_path || !events || !uid_text)
		return -EINVAL;
	errno = 0;
	value = strtoul(uid_text, &end, 10);
	if (errno || *end || value > UINT_MAX)
		return -EINVAL;
	uid = (uid_t)value;
	if (timeout_text) {
		errno = 0;
		value = strtoul(timeout_text, &end, 10);
		if (errno || *end || value < 1 || value > 3600)
			return -EINVAL;
		timeout = (unsigned int)value;
	}
	if (approval_store(socket_path, events, uid, timeout))
		return -EINVAL;
	approval_cli_override = true;
	return 0;
}

int approval_configure_file(const char *socket_path, const char *events,
			    uid_t uid, unsigned int timeout)
{
	if (approval_cli_override)
		return 0;
	return approval_store(socket_path, events, uid, timeout);
}

int approval_enabled_for(const char *event_name)
{
	return approval_socket[0] && event_selected(event_name);
}

int approval_is_configured(void)
{
	return approval_socket[0] != '\0';
}

static int connect_agent(void)
{
	struct sockaddr_un address = { .sun_family = AF_UNIX };
	struct stat status;
	int fd;

	if (lstat(approval_socket, &status) || !S_ISSOCK(status.st_mode) ||
	    status.st_uid != approval_uid || (status.st_mode & 0022))
		return -1;
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;
	(void)fcntl(fd, F_SETFD, FD_CLOEXEC);
	strncpy(address.sun_path, approval_socket, sizeof(address.sun_path) - 1);
	if (connect(fd, (struct sockaddr *)&address, sizeof(address))) {
		close(fd);
		return -1;
	}
#ifdef SO_NOSIGPIPE
	{
		int enabled = 1;

		(void)setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enabled,
				 sizeof(enabled));
	}
#endif
#ifdef __linux__
	{
		struct ucred credentials;
		socklen_t credentials_size = sizeof(credentials);

		if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &credentials,
			       &credentials_size) ||
		    credentials.uid != approval_uid) {
			close(fd);
			return -1;
		}
	}
#endif
	return fd;
}

static int write_all(int fd, const char *data, size_t size)
{
	while (size) {
		ssize_t written;

#ifdef MSG_NOSIGNAL
		written = send(fd, data, size, MSG_NOSIGNAL);
#else
		written = write(fd, data, size);
#endif

		if (written < 0 && errno == EINTR)
			continue;
		if (written <= 0)
			return -1;
		data += written;
		size -= (size_t)written;
	}
	return 0;
}

int approval_decide(struct comm_buffer_s *request, uint64_t request_id,
		    const char *event_name, int policy_result)
{
	char request_line[APPROVAL_LINE_MAX];
	char response[APPROVAL_LINE_MAX];
	const char *policy = policy_result == MED_ALLOW ? "allow" : "deny";
	struct timespec started;
	size_t used = 0;
	int fd;
	int length;

	if (!approval_enabled_for(event_name))
		return policy_result;
	if (strpbrk(event_name, "\r\n\t"))
		return MED_DENY;
	length = snprintf(request_line, sizeof(request_line),
			  "REQ\t1\t%llu\t%s\t%s\n",
			  (unsigned long long)request_id, policy, event_name);
	if (length < 0 || (size_t)length >= sizeof(request_line))
		return MED_DENY;
	fd = connect_agent();
	if (fd < 0 || write_all(fd, request_line, (size_t)length)) {
		if (fd >= 0)
			close(fd);
		return MED_DENY;
	}
	clock_gettime(CLOCK_MONOTONIC, &started);
	while (used + 1 < sizeof(response)) {
		struct timespec now;
		struct pollfd poll_fd = { .fd = fd, .events = POLLIN };
		long elapsed;
		int ready;

		if (mcp_authrequest_cancelled(request))
			break;
		ready = poll(&poll_fd, 1, APPROVAL_RENEW_MS);

		if (ready < 0 && errno == EINTR)
			continue;
		if (ready < 0)
			break;
		if (!ready)
			(void)mcp_renew_authrequest(request);
		else {
			ssize_t count = read(fd, response + used,
					     sizeof(response) - used - 1);
			if (count <= 0)
				break;
			used += (size_t)count;
			if (memchr(response, '\n', used))
				break;
		}
		clock_gettime(CLOCK_MONOTONIC, &now);
		elapsed = now.tv_sec - started.tv_sec;
		if (elapsed >= approval_timeout)
			break;
	}
	close(fd);
	response[used] = '\0';
	{
		char expected[96];
		int prefix = snprintf(expected, sizeof(expected), "RES\t1\t%llu\t",
				      (unsigned long long)request_id);
		const char *answer = response + (prefix > 0 ? prefix : 0);

		if (prefix > 0 && !strncmp(response, expected, (size_t)prefix)) {
			if (!strncmp(answer, "allow\t", 6) ||
			    !strcmp(answer, "allow\n"))
				return MED_ALLOW;
			if (!strncmp(answer, "deny\t", 5) ||
			    !strcmp(answer, "deny\n"))
				return MED_DENY;
		}
	}
	return MED_DENY;
}
