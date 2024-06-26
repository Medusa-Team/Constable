/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Constable: tree.h
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#ifndef _TREE_H
#define _TREE_H

#include "access_types.h"
#include "vs.h"
#include "event.h"

struct tree_s;
struct space_s;

struct tree_type_s {
	int	size;		/* size of node */
	struct class_handler_s *class_handler;
	int (*init)(struct tree_s *t);
	struct tree_type_s *child_type;
	char	name[0];
};

struct tree_event_s {
	struct event_mask_s	event[2];
};

struct tree_s {
	struct tree_s	*parent;
	struct tree_s	*child;		/* normalny potomkovia */
	struct tree_s	*regex_child;	/* potomkovia s regexpami */
	struct tree_s	*next;
	struct tree_s	*alt;		/* alternative */
	struct tree_type_s *type;
	void		*compiled_regex;
	struct space_s	*primary_space;
	struct tree_event_s	*events;
	vs_t	vs[NR_ACCESS_TYPES][VS_WORDS];
	vs_t	no_vs[NR_ACCESS_TYPES][VS_WORDS];  /**< not used */
	struct event_hadler_hash_s	*subject_handlers[EHH_LISTS];
	struct event_hadler_hash_s	*object_handlers[EHH_LISTS];
	bool	visited; /* used for counting space members */
	char	name[0];
};

extern struct tree_s *global_root;

int tree_set_default_path(char *new);

struct tree_s *register_tree_type(struct tree_type_s *type);
struct tree_s *create_path(char *path);
struct tree_s *find_one(struct tree_s *base, char *name);
struct tree_s *find_path(char *path);
int tree_is_equal(struct tree_s *test, struct tree_s *p);
int tree_is_offspring(struct tree_s *offspring, struct tree_s *ancestor);

void tree_for_alternatives(struct tree_s *p, void (*func)(struct tree_s *t, void *arg), void *arg);

int tree_expand_alternatives(void);

//int path_add_vs(char *path, int recursive, int which, const vs_t *vs);

char *tree_get_path(struct tree_s *t);
void tree_print_node(struct tree_s *t, int level, void (*out)(int arg, char *), int arg);

int tree_init(void);

#endif
