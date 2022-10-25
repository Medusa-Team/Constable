/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Constable: init.h
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#ifndef _INIT_H
#define _INIT_H

extern int language_do(void);
extern int _rbac_init(void);
extern int mcp_init(char *filename);
extern int language_init(char *filename);
extern int execute_init(int n);
extern int cmds_init(void);
//extern int force_init(void);

struct module_s {
	struct module_s *next;
	char name[64];
	char *filename;
	int (*create_comm)(struct module_s *m);
	int (*init_comm)(struct module_s *m);
	int (*init_namespace)(struct module_s *m);
	int (*init_rules)(struct module_s *m);
};

int add_module(struct module_s *module);
struct module_s *activate_module(char *name);

#endif /* _INIT_H */

