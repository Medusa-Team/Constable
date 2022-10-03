/* SPDX-License-Identifier: GPL-2.0 */
/**
 * @file mcp.h
 * @short Header for Medusa Communication Protocol handler
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#ifndef _MCP_H
#define	_MCP_H
#include "../comm.h"
#include <netinet/in.h>

/* For MED_* constants see <kernel_sources>/security/medusa/include/l3/constants.h */
#define MED_ERR -1
#define MED_FORCE_ALLOW 0
#define MED_DENY 1
#define MED_FAKE_ALLOW 2
#define MED_ALLOW 3

int mcp_language_do(char *filename);
int mcp_receive_greeting(struct comm_s *c);
struct comm_s *mcp_alloc_comm(char *name);
int mcp_open(struct comm_s *c, char *filename);
struct comm_s *mcp_listen(in_port_t port);
int mcp_to_accept(struct comm_s *c, struct comm_s *listen, in_addr_t ip, in_addr_t mask, in_port_t port);

#endif /* _MCP_H */

