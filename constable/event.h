/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Constable: event.h
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#ifndef _EVENT_H
#define _EVENT_H

#include <asm/types.h>
#include "types.h"
#include "object.h"
#include "medusa_object.h"
#include "access_types.h"
#include "hash.h"
#include "vs.h"

#ifdef DEBUG_TRACE
#define	DT_POS_MAX	64
#else
#define	DT_POS_MAX	0
#endif

#define	ALL_OBJ	((void *)1)

#define	EHH_VS_ALLOW		0
#define	EHH_VS_DENY		1
#define	EHH_NOTIFY_ALLOW	2
#define	EHH_NOTIFY_DENY		3
#define	EHH_LISTS		4

#define	RESULT_ERR		-1	/* ERROR */
#define	RESULT_FORCE_ALLOW	0	/* YES */
#define	RESULT_DENY		1	/* NO */
#define	RESULT_FAKE_ALLOW	2	/* SKIP */
#define	RESULT_ALLOW		3	/* OK */
#define	RESULT_RETRY		4	/* RETRY */

#define	evhash_foreach(ev, hash)	for ((ev) = (hash); (ev) != NULL; (ev) = (ev)->next)
#define	evhash_foreach_first(ev, hash)	((ev) = (hash))
#define	evhash_foreach_next(ev)		((ev) = (ev)->next)

struct event_context_s;

struct event_mask_s {
	char bitmap[8];
};

/*
 * Struct with variable length.
 * Allocated by event_type_add().
 */
struct event_type_s {
	struct hash_ent_s	hashent;
	struct event_names_s	*evname;
	struct class_s		*op[2];  /**< subject and object */
	struct class_s		*monitored_operand; /**< points to op[0] or op[1] */
	struct event_mask_s	mask[2];
	//	struct event_type_s	*alt;
	struct medusa_acctype_s	acctype;
	struct class_s		operation_class;
};

/*
 * Struct with variable length.
 * Allocated by event_type_find_name().
 *
 * @next: Linked list of all registered names of events.
 *	The last appended name event is the head of the global linked
 *	list `events'.
 * @name: Pointer to the name of the event. It's initialized one byte
 *	behind the struct itself.
 * @events: Set in event.c:event_type_add(). There can be various versions
 *	of the event through active connections handled by Constable,
 *	so each connection keep pointer to the its own definition
 *	of the event in this array. The byte layout of the event is
 *	unknown until the connection with a monitored application is
 *	established (the layout definition is part of the initialization
 *	phase of Medusa communication protocol).
 * @handlers_hash: Set by event.c:evhash_add(). Stores list heads for
 *	each operation mode of Constable. Each list contains handlers
 *	related to the event with @name.
 */
struct event_names_s {
	struct event_names_s	*next;
	char			*name;
	struct event_type_s	**events;
	struct event_hadler_hash_s *handlers_hash[EHH_LISTS];
};

struct event_handler_s {
#ifndef DEBUG_TRACE
	char		op_name[MEDUSA_OPNAME_MAX];
#else
	char		op_name[MEDUSA_OPNAME_MAX + DT_POS_MAX];
#endif

	/* if handler returns
	 * 0 => completed  - result is valid
	 * 1 => to be continue immediately (move to end of queue)
	 * >1 => to be continue later
	 * <0 => error
	 */
	int (*handler)(struct comm_buffer_s *, struct event_handler_s *, struct event_context_s *);
	struct object_s	*local_vars;
	char		data[0];
};

struct event_hadler_hash_s {
	struct event_hadler_hash_s	*next;
	struct event_handler_s		*handler;
	struct event_names_s		*evname;
	vs_t				subject_vs[VS_WORDS];
	vs_t				object_vs[VS_WORDS];
};

struct event_context_s {
	struct object_s operation;
	struct object_s subject;
	struct object_s object;
	int		result;
	struct comm_buffer_s *cb;	/* ??? asi zbytocne ??? */
	int		unhandled;      /**< 1 if event wasn't handled */
	struct object_s *local_vars;
};

/*
 * RESULT_ERR -> *
 * RESULT_FORCE_ALLOW -> RESULT_DENY | RESULT_FAKE_ALLOW
 * RESULT_DENY -> -
 * RESULT_FAKE_ALLOW -> RESULT_DENY
 * RESULT_ALLOW -> RESULT_FORCE_ALLOW | RESULT_DENY | RESULT_FAKE_ALLOW
 */
int evaluate_result(int old, int new);
int do_event(struct comm_buffer_s *cb);

int event_free_all_events(struct comm_s *comm);
struct event_type_s *event_type_add(struct comm_s *comm, struct medusa_acctype_s *m, struct medusa_attribute_s *a);
struct event_names_s *event_type_find_name(char *name, bool alloc_new);

int event_mask_clear(struct event_mask_s *e);
int event_mask_clear2(struct event_mask_s *e);
int event_mask_setbit(struct event_mask_s *e, int b);
int event_mask_clrbit(struct event_mask_s *e, int b);
int event_mask_copy(struct event_mask_s *e, struct event_mask_s *f);
int event_mask_copy2(struct event_mask_s *e, struct event_mask_s *f);
int event_mask_and(struct event_mask_s *e, struct event_mask_s *f);
int event_mask_or(struct event_mask_s *e, struct event_mask_s *f);
int event_mask_or2(struct event_mask_s *e, struct event_mask_s *f);
int event_mask_sub(struct event_mask_s *e, struct event_mask_s *f);

struct event_hadler_hash_s *evhash_add(struct event_hadler_hash_s **hash, struct event_handler_s *handler, struct event_names_s *evname);
int register_event_handler(struct event_handler_s *h, struct event_names_s *evname, struct event_hadler_hash_s **hash, vs_t *subject_vs, vs_t *object_vs);
int get_empty_context(struct event_context_s *c);
void event_context_print(struct event_context_s *c, void (*out)(int arg, char *), int arg);

#endif /* _EVENT_H */
