/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Constable: generic.h
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#ifndef _GENERIC_H
#define _GENERIC_H

#include "tree.h"
#include "event.h"
#include "object.h"

/*
 * Tree definition in language/conf_lang.c:
 *	tree "name" [test_enter | clone ...] of <class> [by <op> <exp>];
 *
 * GFL_USE_VSE - if set, test, whether the subject of the <op> is
 *	 allowed enter a given node of tree "name" (for more
 *	 details see functions generic_enter_tree_node() and
 *	 generic_hierarchy_handler_decide()). Set by language
 *	 keyword 'test_enter'.
 * GFL_FROM_OBJECT - if set, copy `cinfo' from object of the <op>
 *	 (presumably parent object); for more information see
 *	 generic_hierarchy_handler_decide(). Set by language
 *	 keyword 'clone'.
 */
#define GFL_USE_VSE	0x01	// language keyword 'test_enter'
#define GFL_FROM_OBJECT	0x02	// language keyword 'clone'

struct g_event_handler_s {
	struct event_handler_s h;
	struct event_handler_s *subhandler;
	struct class_handler_s *class_handler;
};

int generic_set_handler(struct class_handler_s *h, struct comm_s *comm, struct object_s *o);
struct tree_s *generic_get_tree_node(struct class_handler_s *h, struct comm_s *comm, struct object_s *o);
int generic_get_vs(struct class_handler_s *h, struct comm_s *comm, struct object_s *o, vs_t *vs, int n);
struct space_s *generic_get_primary_space(struct class_handler_s *h, struct comm_s *comm, struct object_s *o);
int generic_enter_tree_node(struct class_handler_s *h, struct comm_s *comm, struct object_s *o, struct tree_s *node);
int generic_test_vs(int acctype, struct event_context_s *c);
int generic_test_vs_tree(int acctype, struct event_context_s *c, struct tree_s *t);

int generic_init_comm(struct class_handler_s *h, struct comm_s *comm);
struct class_handler_s *generic_get_new_class_handler_s(struct class_names_s *class, int size);
int generic_init(char *name, struct event_handler_s *subhandler, struct event_names_s *event, struct class_names_s *class, int flags);

#endif /* _GENERIC_H */
