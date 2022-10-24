// SPDX-License-Identifier: GPL-2.0
/*
 * Constable: space.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#include "constable.h"
#include "comm.h"
#include "space.h"

#include <stdio.h>
#include <pthread.h>

/* FIXME: check infinite loops between space_for_every_path, is_included and is_excluded! */
#define	MAX_SPACE_PATH	32

typedef void (*fepf_t)(struct tree_s *t, void *arg);

struct space_s *global_spaces;

/**
 * Create a new path and append it to the end of \p prev.
 * \param prev tree node to append to
 * \param path_or_space tree node to append
 * \param type of the node to append: LTREE_T_TREE or LTREE_T_SPACE
 */
static struct ltree_s *new_path(struct ltree_s *prev, void *path_or_space, int type)
{
	char **errstr;
	struct ltree_s *l;

	l = malloc(sizeof(struct ltree_s));
	if (l == NULL) {
		errstr = (char **) pthread_getspecific(errstr_key);
		*errstr = Out_of_memory;
		return NULL;
	}

	l->prev = prev;
	l->path_or_space = path_or_space;
	l->type = type;

	return l;
}

static struct levent_s *new_levent(struct levent_s **prev, struct event_handler_s *handler, struct space_s *subject, struct space_s *object)
{
	char **errstr;
	struct levent_s *l;

	l = malloc(sizeof(struct levent_s));
	if (l == NULL) {
		errstr = (char **) pthread_getspecific(errstr_key);
		*errstr = Out_of_memory;
		return NULL;
	}

	l->handler = handler;
	l->subject = subject;
	l->object = object;
	l->prev =  *prev;
	*prev = l;

	return l;
}

struct space_s *space_find(char *name)
{
	struct space_s *t;

	for (t = global_spaces; t != NULL; t = t->next)
		if (!strcmp(t->name, name))
			return t;
	return NULL;
}

struct space_s *space_create(char *name, int primary)
{
	char **errstr;
	struct space_s *t;
	int a;

	if (name != NULL) {
		if (space_find(name) != NULL) {
			errstr = (char **) pthread_getspecific(errstr_key);
			*errstr = Space_already_defined;
			return NULL;
		}
	} else
		name = ANON_SPACE_NAME;

	t = malloc(sizeof(struct space_s)+strlen(name)+1);
	if (t == NULL) {
		errstr = (char **) pthread_getspecific(errstr_key);
		*errstr = Out_of_memory;
		return NULL;
	}

	strcpy(t->name, name);
	for (a = 0; a < NR_ACCESS_TYPES; a++)
		vs_clear(t->vs[a]);
	t->levent = NULL;
	t->ltree = NULL;
	t->primary = primary;
	t->next = global_spaces;
	global_spaces = t;

	return t;
}

struct space_path_s {
	struct space_s *path[MAX_SPACE_PATH];
	int n;
};

static int space_path_add(struct space_path_s *sp, struct space_s *space)
{
	int i;

	for (i = 0; i < sp->n; i++) {
		if (sp->path[i] == space) {
			fatal("Infinite loop in spaces!");
			return 0;
		}
	}
	if (sp->n >= MAX_SPACE_PATH) {
		fatal("Too many nested spaces!");
		return 0;
	}
	sp->path[sp->n++] = space;
	return 1;
}

static int is_excluded(struct tree_s *test, struct space_s *space);

static int space_path_exclude(struct space_path_s *sp, struct tree_s *t)
{
	int i, r, rn;

	r = 0;
	for (i = 0; i < sp->n; i++) {
		rn = is_excluded(t, sp->path[i]);
		r = rn > r ? rn : r;
		if (r > 1)
			break;
	}
	return r;
}

/* is_included: 0 - not, 1 - yes */
static int is_included_i(struct tree_s *test, struct space_path_s *sp, struct space_s *space, int force_recursive)
{
	struct ltree_s *l;

	if (!space_path_add(sp, space))
		return 0;

	for (l = space->ltree; l != NULL; l = l->prev) {
		switch (l->type&LTREE_T_MASK) {
		case LTREE_T_TREE:
			if (!(l->type&LTREE_EXCLUDE)) {
				if (tree_is_equal(test, l->path_or_space))
					return !space_path_exclude(sp, test);
				if ((force_recursive || l->type&LTREE_RECURSIVE)
						&& tree_is_offspring(test, l->path_or_space))
					return !space_path_exclude(sp, test);
			}
			break;
		case LTREE_T_SPACE:
			if (!(l->type&LTREE_EXCLUDE)) {
				if (is_included_i(test, sp, l->path_or_space,
						(l->type & LTREE_RECURSIVE) ? 1 : force_recursive))
					return 1;
			}
			break;
		}
	}

	return 0;
}

static int is_included(struct tree_s *test, struct space_s *space, int force_recursive)
{
	struct space_path_s sp;

	sp.n = 0;
	return is_included_i(test, &sp, space, force_recursive);
}

/* is_excluded: 0 - not, 1 - yes, 2 - recursive */
static int is_excluded(struct tree_s *test, struct space_s *space)
{
	struct ltree_s *l;
	int r = 0;

	for (l = space->ltree; l != NULL; l = l->prev) {
		switch (l->type&LTREE_T_MASK) {
		case LTREE_T_TREE:
			if ((l->type & LTREE_EXCLUDE)) {
				if (tree_is_equal(test, l->path_or_space))
					return (l->type & LTREE_RECURSIVE) ? 2 : 1;
				if ((l->type & LTREE_RECURSIVE)
						&& tree_is_offspring(test, l->path_or_space))
					r = (l->type & LTREE_RECURSIVE) ? 2 : 1;
			}
			break;
		case LTREE_T_SPACE:
			if ((l->type & LTREE_EXCLUDE)) {
				if (is_included(test, l->path_or_space,
						(l->type & LTREE_RECURSIVE)))
					r = (l->type & LTREE_RECURSIVE) ? 2 : 1;
			}
			break;
		}

		if (r > 1)
			break;
	}

	return r;
}

struct space_for_one_path_s {
	struct space_path_s *sp;
	int recursive;
	fepf_t func;
	void *arg;
};

static void space_for_one_path_i(struct tree_s *t, struct space_for_one_path_s *a)
{
	int r;
	struct tree_s *p;

	r = space_path_exclude(a->sp, t);
	if (!r)
		a->func(t, a->arg);

	if (a->recursive && r < 2) {
		for (p = t->child; p != NULL; p = p->next)
			space_for_one_path_i(p, a);
		for (p = t->regex_child; p != NULL; p = p->next)
			space_for_one_path_i(p, a);
	}
}

static void space_for_one_path(struct tree_s *t, struct space_path_s *sp, int recursive, fepf_t func, void *arg)
{
	struct space_for_one_path_s a;

	a.sp = sp;
	a.recursive = recursive;
	a.func = func;
	a.arg = arg;
	tree_for_alternatives(t, (fepf_t)space_for_one_path_i, &a);
}

static int space_for_every_path_i(struct space_s *space, struct space_path_s *sp, int force_recursive, fepf_t func, void *arg)
{
	struct ltree_s *l;

	if (!space_path_add(sp, space))
		return -1;

	for (l = space->ltree; l != NULL; l = l->prev) {
		switch (l->type & LTREE_T_MASK) {
		case LTREE_T_TREE:
			if (!(l->type & LTREE_EXCLUDE))
				space_for_one_path(l->path_or_space, sp,
					(force_recursive || (l->type & LTREE_RECURSIVE)) ? 1 : 0,
					func, arg);
			break;
		case LTREE_T_SPACE:
			if (!(l->type & LTREE_EXCLUDE))
				space_for_every_path_i(l->path_or_space, sp,
					(l->type & LTREE_RECURSIVE) ? 1 : force_recursive,
					func, arg);
			break;
		}
	}

	sp->n--;
	return 0;
}

static int space_for_every_path(struct space_s *space, fepf_t func, void *arg)
{
	struct space_path_s sp;

	sp.n = 0;
	return space_for_every_path_i(space, &sp, 0, func, arg);
}

static void set_primary_space_do(struct tree_s *t, struct space_s *space)
{
	if (t->primary_space != NULL && t->primary_space != space)
		warning("Redefinition of primary space %s to %s",
			t->primary_space->name, space->name);
	t->primary_space = space;
}

/* ----------------------------------- */

struct tree_add_event_mask_do_s {
	int conn;
	struct event_type_s *type;
};

static void tree_add_event_mask_do(struct tree_s *p, struct tree_add_event_mask_do_s *arg)
{
	if (arg->type->monitored_operand == p->type->class_handler->classname->classes[arg->conn])
		event_mask_or2(p->events[arg->conn].event, arg->type->mask);
}

/* ----------------------------------- */

struct tree_add_vs_do_s {
	int which;
	const vs_t *vs;
};

static void tree_add_vs_do(struct tree_s *p, struct tree_add_vs_do_s *arg)
{
	vs_add(arg->vs, p->vs[arg->which]);
}

/* ----------------------------------- */



/**
 * Add path node to the list of paths of space and set its type.
 * \param space New path will be added to this virtual space.
 * \param type of the new node (see macros LTREE_*).
 * \param path_or_space Pointer to the node that will be added.
 * \return 0 on success, -1 otherwise
 */
int space_add_path(struct space_s *space, int type, void *path_or_space)
{
	if (path_or_space == NULL)
		return -1;

	space->ltree = new_path(space->ltree, path_or_space, type);
	if (space->ltree == NULL)
		return -1;
	return 0;
}

/* space_apply_all must be called after all spaces are defined */
int space_apply_all(void)
{
	struct space_s *space;
	struct tree_add_vs_do_s arg;
	int a;

	tree_expand_alternatives();

	for (space = global_spaces; space != NULL; space = space->next) {
		if (space->primary)
			space_for_every_path(space, (fepf_t)set_primary_space_do, space);
		for (a = 0; a < NR_ACCESS_TYPES; a++) {
			arg.which = a;
			arg.vs = space->vs[a];
			space_for_every_path(space, (fepf_t)tree_add_vs_do, &arg);
		}
	}

	return 0;
}

/* neprida event_mask */
int space_add_event(struct event_handler_s *handler, int ehh_list, struct space_s *subject, struct space_s *object, struct tree_s *subj_node, struct tree_s *obj_node)
{
	struct event_names_s *type;

	if (ehh_list < 0 || ehh_list >= EHH_LISTS) {
		error("Invalid ehh_list of handler");
		return -1;
	}

	type = event_type_find_name(handler->op_name);
	if (type == NULL) {
		error("Event %s does not exist\n", handler->op_name);
		return -1;
	}

	if (subject != NULL && subject != ALL_OBJ && new_levent(&(subject->levent), handler, subject, object) == NULL)
		return -1;
	if (object != NULL && object != ALL_OBJ && new_levent(&(object->levent), handler, subject, object) == NULL)
		return -1;

	if (subj_node && obj_node) {
		object = space_create(NULL, 0);
		if (object == NULL)
			return -1;
		space_add_path(object, 0, obj_node);
		if (new_levent(&(object->levent), handler, subject, object) == NULL)
			return -1;
		//printf("ZZZ: subj=%s obj=%s tak registrujem podla svetov\n", subj_node->name, obj_node->name);
		register_event_handler(handler, type, &(subj_node->subject_handlers[ehh_list]), space_get_vs(subject), space_get_vs(object));
	} else if (subj_node) {
		//printf("ZZZ: registrujem pri subjekte %s\n", subj_node->name);
		register_event_handler(handler, type, &(subj_node->subject_handlers[ehh_list]), space_get_vs(subject), space_get_vs(object));
	} else if (obj_node) {
		//printf("ZZZ: registrujem pri objekte %s\n", obj_node->name);
		register_event_handler(handler, type, &(obj_node->object_handlers[ehh_list]), space_get_vs(subject), space_get_vs(object));
	} else {
		//printf("ZZZ: registrujem podla svetov\n");
		register_event_handler(handler, type, &(type->handlers_hash[ehh_list]), space_get_vs(subject), space_get_vs(object));
	}

	return 0;
}

static void tree_comm_reinit(struct comm_s *comm, struct tree_s *t)
{
	struct tree_s *p;
	struct event_hadler_hash_s *hh;
	struct event_names_s *en;
	struct event_type_s *type;
	int ehh_list;

	if (t->events) {
		event_mask_clear2(t->events[comm->conn].event);

		for (ehh_list = 0; ehh_list < EHH_LISTS; ehh_list++) {
			evhash_foreach(hh, t->subject_handlers[ehh_list]) {
				en = event_type_find_name(hh->handler->op_name);
				if (en == NULL) {
					comm->conf_error(comm, "Unknown event name %s\n", hh->handler->op_name);
					continue;
				}
				type = en->events[comm->conn];
				if (type == NULL) {
					comm->conf_error(comm, "Unknown event type %s\n", hh->handler->op_name);
					continue;
				}

				if (type->monitored_operand == type->op[0]) {
					if (type->monitored_operand == t->type->class_handler->classname->classes[comm->conn])
						event_mask_or2(t->events[comm->conn].event, type->mask);
					else
						comm->conf_error(comm, "event %s subject class mismatch", hh->handler->op_name);
				}
			}

			evhash_foreach(hh, t->object_handlers[ehh_list]) {
				en = event_type_find_name(hh->handler->op_name);
				if (en == NULL) {
					comm->conf_error(comm, "Unknown event name %s\n", hh->handler->op_name);
					continue;
				}
				type = en->events[comm->conn];
				if (type == NULL) {
					comm->conf_error(comm, "Unknown event type %s\n", hh->handler->op_name);
					continue;
				}

				if (type->monitored_operand == type->op[1]) {
					if (type->monitored_operand == t->type->class_handler->classname->classes[comm->conn])
						event_mask_or2(t->events[comm->conn].event, type->mask);
					else
						comm->conf_error(comm, "event %s object class mismatch", hh->handler->op_name);
				}
			}
		} // for ehh_list
	} // if t->events

	for (p = t->child; p != NULL; p = p->next)
		tree_comm_reinit(comm, p);
	for (p = t->regex_child; p != NULL; p = p->next)
		tree_comm_reinit(comm, p);
}

/* space_init_event_mask must be called after connection was estabilished */
int space_init_event_mask(struct comm_s *comm)
{
	struct space_s *space;
	struct levent_s *le;
	struct event_names_s *en;
	struct event_type_s *type;
	struct tree_add_event_mask_do_s arg;

	tree_comm_reinit(comm, global_root);

	for (space = global_spaces; space != NULL; space = space->next) {
		for (le = space->levent; le != NULL; le = le->prev) {
			en = event_type_find_name(le->handler->op_name);
			if (en == NULL) {
				comm->conf_error(comm, "Unknown event name %s\n", le->handler->op_name);
				continue;
			}
			type = en->events[comm->conn];
			if (type == NULL) {
				comm->conf_error(comm, "Unknown event type %s\n", le->handler->op_name);
				continue;
			}

			if (((type->op[0] == NULL) != (le->subject == NULL)) ||
					((type->op[1] == NULL) != (le->object == NULL)))
				comm->conf_error(comm, "Invalid use of event %s\n", le->handler->op_name);

			if (le->subject != NULL && le->subject != ALL_OBJ && type->monitored_operand == type->op[0]) {
				arg.conn = comm->conn;
				arg.type = type;
				space_for_every_path(le->subject, (fepf_t)tree_add_event_mask_do, &arg);
			}
			if (le->object != NULL && le->object != ALL_OBJ && type->monitored_operand == type->op[1]) {
				arg.conn = comm->conn;
				arg.type = type;
				space_for_every_path(le->object, (fepf_t)tree_add_event_mask_do, &arg);
			}
		} // for space->levent
	} // for space in global_spaces

	return 0;
}

/**
 * Place subject with virtual space \p space into virtual spaces defined by \p
 * vs for access type \p which. Original virtual spaces of \p space won't be
 * changed.
 * \param space Virtual spaces of the subject
 * \param which Access type
 * \param vs Virtual spaces to add
 * \return 0 on success, -1 on invalid which value
 */
int space_add_vs(struct space_s *space, int which, vs_t *vs)
{
	if (which < 0 || which >= NR_ACCESS_TYPES)
		return -1;
	vs_add(vs, space->vs[which]);
	return 0;
}

vs_t *space_get_vs(struct space_s *space)
{
	if (space == NULL || space == ALL_OBJ)
		return (vs_t *)space;
	if (!vs_isclear(space->vs_id))
		return space->vs_id;
	if (vs_alloc(space->vs_id) < 0)
		return NULL;

	//printf("ZZZ:space_get_vs %s 0x%08x\n",space->name,*(space->vs_id));
	vs_add(space->vs_id, space->vs[AT_MEMBER]);
	return space->vs_id;
}

/* ----------------------------------- */

int space_vs_to_str(vs_t *vs, char *out, int size)
{
	struct space_s *space;
	vs_t tvs[VS_WORDS];
	int pos, l;

	vs_set(vs, tvs);
	pos = 0;
	for (space = global_spaces; space != NULL; space = space->next) {
		if (!vs_isclear(space->vs[AT_MEMBER])
				&& vs_issub(space->vs[AT_MEMBER], tvs)
				&& space->name[0] != 0
				&& space->name[0] != ANON_SPACE_NAME[0]) {
			vs_sub(space->vs[AT_MEMBER], tvs);

			l = strlen(space->name);
			if (l + 2 >= size)
				return -1;

			if (pos > 0) {
				out[pos] = '|';
				pos++;
				size--;
			}
			strcpy(out+pos, space->name);
			pos += l;
			size -= l;
		}
	} // for space in global spaces

	if (!vs_isclear(tvs)) {
		if ((sizeof(tvs)*9)/8+3 >= size)
			return -1;
		if (pos > 0) {
			out[pos] = '|';
			pos++;
			size--;
		}
#ifdef BITMAP_DIPLAY_LEFT_RIGHT
		for (l = 0; l < sizeof(tvs); l++) {
			if (l > 0 && (l & 0x03) == 0) {
				out[pos++] = ':';
				size--;
			}
			sprintf(out+pos, "%02x", ((char *)tvs)[l]);
			pos += 2;
			size -= 2;
		}
#else
		for (l = sizeof(tvs)-1; l >= 0; l--) {
			sprintf(out+pos, "%02x", ((char *)tvs)[l]);
			pos += 2;
			size -= 2;
			if (l > 0 && (l & 0x03) == 0) {
				out[pos++] = ':';
				size--;
			}
		}
#endif
	} // if !vs_isclear(tvs)

	out[pos] = 0;
	return pos;
}
