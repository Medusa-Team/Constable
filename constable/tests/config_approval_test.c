/* SPDX-License-Identifier: GPL-2.0 */
#include <stdarg.h>
#include <stdio.h>

#include "../approval.h"
#include "../comm.h"
#include "../init.h"
#include "../mcp/mcp.h"

char *medusa_config_file = "/etc/medusa.conf";
int medusa_config_file_explicit;
pthread_key_t errstr_key;

int init_error(const char *format, ...)
{
	(void)format;
	return -1;
}

int error(const char *format, ...)
{
	(void)format;
	return -1;
}

struct module_s *activate_module(char *name)
{
	(void)name;
	return NULL;
}

struct comm_s *mcp_alloc_comm(char *name)
{
	static struct comm_s comm;

	(void)name;
	return &comm;
}

int mcp_open(struct comm_s *comm, char *filename)
{
	(void)comm;
	(void)filename;
	return 0;
}

struct comm_s *mcp_listen(in_port_t port)
{
	(void)port;
	return NULL;
}

int mcp_to_accept(struct comm_s *comm, struct comm_s *listen, in_addr_t ip,
		  in_addr_t mask, in_port_t port)
{
	(void)comm;
	(void)listen;
	(void)ip;
	(void)mask;
	(void)port;
	return 0;
}

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

int main(int argc, char **argv)
{
	if (argc != 2 || mcp_language_do(argv[1]) != 0) {
		fprintf(stderr, "could not parse approval configuration\n");
		return 1;
	}
	if (!approval_enabled_for("socket_connect_access") ||
	    !approval_enabled_for("socket_bind_access") ||
	    approval_enabled_for("exec")) {
		fprintf(stderr, "parsed approval event set is incorrect\n");
		return 1;
	}
	return 0;
}
