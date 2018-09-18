/**
 * @file mcp.h
 * @short Header for Medusa Communication Protocol handler
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: mcp.h,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#ifndef _MCP_H
#define	_MCP_H
#include "../comm.h"
#include <netinet/in.h>

int mcp_receive_greeting(struct comm_s *c);
struct comm_s *mcp_alloc_comm( char *name );
int mcp_open( struct comm_s *c, char *filename );
struct comm_s *mcp_listen( in_port_t port );
int mcp_to_accept( struct comm_s *c, struct comm_s *listen, in_addr_t ip, in_addr_t mask, in_port_t port );

#endif /* _MCP_H */

