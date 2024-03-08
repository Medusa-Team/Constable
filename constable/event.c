// SPDX-License-Identifier: GPL-2.0
/*
 * Constable: event.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#include "constable.h"
#include "event.h"
#include "tree.h"
#include "comm.h"
#include <sys/param.h>

#include <stdio.h>
#include <pthread.h>

int event_mask_clear(struct event_mask_s *e)
{
	memset(e->bitmap, 0, sizeof(e->bitmap));
	return 0;
}

int event_mask_clear2(struct event_mask_s *e)
{
	memset(e->bitmap, 0, 2*sizeof(e->bitmap));
	return 0;
}

int event_mask_setbit(struct event_mask_s *e, int b)
{
	if (b < 0 || b >= sizeof(e->bitmap)*8)
		return -1;
	setbit(e->bitmap, b);
	return 0;
}

int event_mask_clrbit(struct event_mask_s *e, int b)
{
	if (b < 0 || b >= sizeof(e->bitmap)*8)
		return -1;
	clrbit(e->bitmap, b);
	return 0;
}

int event_mask_copy(struct event_mask_s *e, struct event_mask_s *f)
{
	memcpy(e->bitmap, f->bitmap, sizeof(e->bitmap));
	return 0;
}

int event_mask_copy2(struct event_mask_s *e, struct event_mask_s *f)
{
	memcpy(e->bitmap, f->bitmap, 2*sizeof(e->bitmap));
	return 0;
}

int event_mask_and(struct event_mask_s *e, struct event_mask_s *f)
{
	int i;

	for (i = sizeof(e->bitmap)-1; i >= 0; i--)
		e->bitmap[i] &= f->bitmap[i];
	return 0;
}

int event_mask_or(struct event_mask_s *e, struct event_mask_s *f)
{
	int i;

	for (i = sizeof(e->bitmap)-1; i >= 0; i--)
		e->bitmap[i] |= f->bitmap[i];
	return 0;
}

int event_mask_or2(struct event_mask_s *e, struct event_mask_s *f)
{
	int i;

	for (i = 2*sizeof(e->bitmap)-1; i >= 0; i--)
		e->bitmap[i] |= f->bitmap[i];
	return 0;
}

int event_mask_sub(struct event_mask_s *e, struct event_mask_s *f)
{
	int i;

	for (i = sizeof(e->bitmap)-1; i >= 0; i--)
		e->bitmap[i] &= ~(f->bitmap[i]);
	return 0;
}

static pthread_mutex_t events_lock = PTHREAD_MUTEX_INITIALIZER;
static struct event_names_s *events;

int event_free_all_events(struct comm_s *comm)
{
	struct event_names_s *e;

	pthread_mutex_lock(&events_lock);
	for (e = events; e != NULL; e = e->next) {
		if (e->events[comm->conn] != NULL) {
			free(e->events[comm->conn]);
			e->events[comm->conn] = NULL;
		}
	}
	pthread_mutex_unlock(&events_lock);

	return 0;
}

/*
 * Called from mcp/mcp.c:mcp_r_acctypedef_attr() after reading all data
 * related to a new access/event type which has to be registered.
 */
struct event_type_s *event_type_add(struct comm_s *comm, struct medusa_acctype_s *m, struct medusa_attribute_s *a)
{
	struct event_type_s *e;
	struct event_names_s *evname;
	int l;

	//printf("ZZZ event_from_medusa:\n");
	for (l = 0; a[l].type != MED_TYPE_END; l++)
		;
	l++;

	l *= sizeof(struct medusa_attribute_s);
	e = malloc(sizeof(struct event_type_s) + l);
	if (e == NULL)
		return NULL;
	memcpy(&(e->acctype), m, sizeof(struct medusa_acctype_s));
	memset(&(e->operation_class), 0, sizeof(e->operation_class));
	//	e->operation_class.next=NULL;
	//	e->operation_class.cinfo_offset=e->operation_class.cinfo_size=0;
	//	e->operation_class.cinfo_mask=0;
	//	e->operation_class.set=NULL;
	//	e->operation_class.event_offset=e->operation_class.event_size=0;
	//	e->operation_class.m.classid=0;
	e->operation_class.m.size = e->acctype.size;
	e->operation_class.m.name[0] = 0;
	strncpy(e->operation_class.m.name, e->acctype.name, MIN(MEDUSA_CLASSNAME_MAX, MEDUSA_OPNAME_MAX));
	memcpy(e->operation_class.attr, a, l);
	e->operation_class.comm = comm;

	e->op[0] = (struct class_s *)hash_find(&(comm->classes), e->acctype.op_class[0]);
	if (e->op[0] == NULL)
		init_error("Subject of access %s does not exist!", e->acctype.name);

	if (e->acctype.op_class[0] == e->acctype.op_class[1] && !strncmp(e->acctype.op_name[0], e->acctype.op_name[1], MEDUSA_ATTRNAME_MAX))
		e->op[1] = NULL;
	else
		e->op[1] = (struct class_s *)hash_find(&(comm->classes), e->acctype.op_class[1]);

	event_mask_clear2(&(e->mask[0]));

	// if event is TRIGERED_AT_OBJECT
	if (e->acctype.actbit&0x8000)
		e->monitored_operand = e->op[1];	/* object */
	else
		e->monitored_operand = e->op[0];	/* subject */
	// if event is TRIGGERED_BY_OBJECT_BIT
	if (e->acctype.actbit&0x4000)
		// 0x00ff --> maximum event id number (act bit) is 255
		event_mask_setbit(&(e->mask[1]), (e->acctype.actbit)&0x00ff);
	else
		// 0x00ff --> maximum event id number (act bit) is 255
		event_mask_setbit(&(e->mask[0]), (e->acctype.actbit)&0x00ff);

	if (debug_def_out != NULL) {
		char buf[16];

		pthread_mutex_lock(&debug_def_lock);

		debug_def_out(debug_def_arg, "REGISTER [\"");
		debug_def_out(debug_def_arg, comm->name);
		debug_def_out(debug_def_arg, "\"] event ");

		if (e->op[0] != NULL) {
			debug_def_out(debug_def_arg, e->acctype.op_name[0]);
			debug_def_out(debug_def_arg, ":");
			debug_def_out(debug_def_arg, e->op[0]->m.name);
		} else
			debug_def_out(debug_def_arg, "-");

		debug_def_out(debug_def_arg, " ");
		debug_def_out(debug_def_arg, m->name);
		debug_def_out(debug_def_arg, " ");

		if (e->op[1] != NULL) {
			debug_def_out(debug_def_arg, e->acctype.op_name[1]);
			debug_def_out(debug_def_arg, ":");
			debug_def_out(debug_def_arg, e->op[1]->m.name);
		} else
			debug_def_out(debug_def_arg, "-");

		debug_def_out(debug_def_arg, " (");
		sprintf(buf, "%d", (e->acctype.actbit)&0x00ff);
		debug_def_out(debug_def_arg, buf);
		debug_def_out(debug_def_arg, ")");

		if (e->acctype.actbit&0x8000)
			debug_def_out(debug_def_arg, " to object");
		else
			debug_def_out(debug_def_arg, " to subject");
		if (e->acctype.actbit&0x4000)
			debug_def_out(debug_def_arg, " object's event");
		else
			debug_def_out(debug_def_arg, " subject's event");

		debug_def_out(debug_def_arg, " {\n");
		attr_print(&(e->operation_class.attr[0]), debug_def_out, debug_def_arg);
		debug_def_out(debug_def_arg, "}\n");

		pthread_mutex_unlock(&debug_def_lock);
	}

	/* allocate a new event descriptor, if it doesn't exist yet */
	evname = event_type_find_name(e->acctype.name, true);
	if (evname == NULL) {
		free(e);
		return NULL;
	}
	evname->events[comm->conn] = e;
	e->evname = evname;
	hash_add(&(comm->events), &(e->hashent), e->acctype.opid);

	return e;
}

/*
 * Find event description by @name in the global list `events' and returns it.
 *
 * If a corresponding event description is not found and @alloc_new is:
 * 1) %true: a new event description is created and pointer to it is returned;
 *	     in case of an error (i.e. memory allocation failure) %NULL is
 *	     returned;
 * 2) %false: %NULL is returned.
 */
struct event_names_s *event_type_find_name(char *name, bool alloc_new)
{
	struct event_names_s *e;
	int i;

	pthread_mutex_lock(&events_lock);
	for (e = events; e != NULL; e = e->next) {
		if (!strcmp(e->name, name)) {
			pthread_mutex_unlock(&events_lock);
			return e;
		}
	}
	pthread_mutex_unlock(&events_lock);

	if (alloc_new == false)
		return NULL;

	e = malloc(sizeof(struct event_names_s) + strlen(name) + 1);
	if (e == NULL)
		return NULL;
	e->events = (struct event_type_s **)(comm_new_array(sizeof(struct event_type_s *)));
	if (e->events == NULL) {
		free(e);
		return NULL;
	}
	for (i = 0; i < EHH_LISTS; i++)
		e->handlers_hash[i] = NULL;
	e->name = (char *)(e + 1);
	strcpy(e->name, name);

	pthread_mutex_lock(&events_lock);
	e->next = events;
	events = e;
	pthread_mutex_unlock(&events_lock);

	return e;
}

struct event_hadler_hash_s *evhash_add(struct event_hadler_hash_s **hash, struct event_handler_s *handler, struct event_names_s *evname)
{
	struct event_hadler_hash_s *l;

	l = malloc(sizeof(struct event_hadler_hash_s));
	if (l == NULL) {
		char **errstr = (char **) pthread_getspecific(errstr_key);
		*errstr = Out_of_memory;
		return NULL;
	}

	for (;  *hash != NULL; hash =  &((*hash)->next)) {
		/* Totalne blbost. Aspon dufam ;-)
		 * if( (*hash)->handler==handler ) {
		 *     free(l);
		 *     return *hash;
		 * }
		 */
	}

	vs_clear(l->subject_vs);
	vs_clear(l->object_vs);
	l->next = NULL;
	l->handler = handler;
	l->evname = evname;
	*hash = l;

	return l;
}


int register_event_handler(struct event_handler_s *h, struct event_names_s *evname, struct event_hadler_hash_s **hash, vs_t *subject_vs, vs_t *object_vs)
{
	struct event_hadler_hash_s *l;

	l = evhash_add(hash, h, evname);
	if (l == NULL)
		return -1;
	if (subject_vs == ALL_OBJ)
		vs_fill(l->subject_vs);
	else if (subject_vs)
		vs_add(subject_vs, l->subject_vs);
	// else	vs_clear(l->subject_vs);

	if (object_vs == ALL_OBJ)
		vs_fill(l->object_vs);
	else if (object_vs)
		vs_add(object_vs, l->object_vs);
	// else	vs_clear(l->object_vs);

	printf("Zaregistrovane %p [%s]\n", l, l->evname->name);
	return 0;
}


int evaluate_result(int old, int new)
{
	/*
	 * if( new==RESULT_FORCE_ALLOW && (old==RESULT_ERR || old==RESULT_ALLOW) )
	 *	return new;
	 * else if( new==RESULT_DENY )
	 *	return new;
	 * else if( new==RESULT_FAKE_ALLOW && old!=RESULT_DENY )
	 *	return new;
	 * else if( new==RESULT_ALLOW && old==RESULT_ERR )
	 *	return new;
	 */

	if (new != RESULT_ALLOW && new != RESULT_DENY
		&& new != RESULT_FAKE_ALLOW && new != RESULT_FORCE_ALLOW)
		new = RESULT_DENY;

	if (old == RESULT_DENY || new == RESULT_DENY)
		return RESULT_DENY;
	if (old == RESULT_ERR || new == RESULT_FAKE_ALLOW)
		return new;
	if (old == RESULT_ALLOW && new == RESULT_FORCE_ALLOW)
		return new;

	return old;
}

static int do_event_handler(struct comm_buffer_s *cb)
{
	struct event_handler_s *h;
	int result, i;

	h = cb->hh->handler;
	if (cb->do_phase == 0) {
		//printf("do_event_handler: testujem handler %p [%s]\n",cb->hh,cb->hh->handler->op_name);
		//if( (cb->context.subject.class==NULL) != (vs_isclear(cb->hh->subject_vs)!=0) )
		//	return -1;
		//if( (cb->context.object.class==NULL) != (vs_isclear(cb->hh->object_vs)!=0) )
		//	return -1;

		if (cb->context.subject.class != NULL && !vs_isfull(cb->hh->subject_vs)) {
			if (!object_cmp_vs(cb->hh->subject_vs, AT_MEMBER, &(cb->context.subject)))
				return -1;
		}
		if (cb->context.object.class != NULL && !vs_isfull(cb->hh->object_vs)) {
			if (!object_cmp_vs(cb->hh->object_vs, AT_MEMBER, &(cb->context.object)))
				return -1;
		}
	}

	//printf("do_event_handler: event result=%d A\n",cb->context.result);
	result = cb->context.result;
	i = h->handler(cb, h, &(cb->context));
	if (i == 0) {
		cb->context.result = evaluate_result(result, cb->context.result);
		cb->context.unhandled = 0;
	} else
		cb->context.result = result;

	//printf("do_event_handler: event result=%d B\n",cb->context.result);
	return i;
}

static int do_init_handler(struct comm_buffer_s *cb)
{
	int i;

	if (cb->do_phase == 0) {
		cb->context.result = RESULT_ERR;
		cb->context.unhandled = 1;
		//cb->context.local_vars=NULL;	/* ???? */
	}

	i = cb->init_handler->handler(cb, cb->init_handler, &(cb->context));
	if (i > 0) {
		cb->do_phase = 1;
		return i;
	}

	cb->do_phase = 0;
	return 0;
}

static int do_event_list(struct comm_buffer_s *cb)
{
	struct event_context_s *c;
	struct tree_s *t;
	int i;

	/* This is just for execution of the _init() handler function from the
	 * configuration file, see mcp_r_query().
	 */
	if (cb->event == NULL)
		return do_init_handler(cb);

	c =  &(cb->context);
	if (debug_do_out != NULL && cb->do_phase == 0) {
		pthread_mutex_lock(&debug_do_lock);
		event_context_print(c, debug_do_out, debug_do_arg);
		pthread_mutex_unlock(&debug_do_lock);
	}

	//#if 0
	//printf("ZZZ cb->ehh_list=%d\n",cb->ehh_list);
	//if (strcmp(cb->event->acctype.name, "getfile")) { // nie iget na subory
	//	printf("ZZZ: event: op=%s (%d) ", cb->event->acctype.name, cb->do_phase);
	//	if (c->subject.class != NULL)
	//		printf("subject=%s ", c->subject.class->m.name);
	//	else
	//		printf("subject=NONE ");
	//	if (c->object.class != NULL)
	//		printf("object=%s ", c->object.class->m.name);
	//	else
	//		printf("object=NONE ");
	//	printf("\n");
	//}
	//#endif

	if (cb->do_phase == 0 && (cb->ehh_list == EHH_VS_ALLOW || cb->ehh_list == EHH_VS_DENY)) {
		c->result = RESULT_ERR;
		c->unhandled = 1;
		//c->local_vars=NULL;	/* ???? */
		if (object_is_invalid(&(c->subject)))
			object_do_sethandler(&(c->subject));	/* !!! ??? ZZZ */
			/* ??? zeby aj objekt ??? je to dobry napad ??? get_file */
	}

	cb->do_phase &= 0x0f;

	if (cb->do_phase == 0 || cb->do_phase == 1) {
		if (cb->do_phase == 0)
			evhash_foreach_first(cb->hh, cb->event->evname->handlers_hash[cb->ehh_list]);
		//printf("eeeeeeeeee cb->hh=%p\n",cb->hh);

		/* If handler requires continuation from interrupted activity (operation
		 * fetch or update invoked from the handler), this cycle continues with
		 * the same handler that was interrupted.
		 */
		for (; cb->hh != NULL; evhash_foreach_next(cb->hh)) {
			/* If there was an error during the execution of the handler, the
			 * error is ignored and cycle continues with the next handler.
			 */
			i = do_event_handler(cb);
			if (i > 0) {
				cb->do_phase = 1;
				return i;
			}
			cb->do_phase = 0;
		}
	}

	if ((cb->do_phase == 0 && c->subject.class != NULL) || cb->do_phase == 2) {
		if (cb->do_phase == 0)
			cb->ch = c->subject.class->classname->class_handler;
		for (; cb->ch != NULL; cb->ch = cb->ch->next) {
			t = cb->ch->get_tree_node(cb->ch, cb->comm, &(c->subject));
			if (t == NULL) {
				/* Setting `do_phase` to zero means, that the inner `for` cycle
				 * will be restarted. But this is ok because `get_tree_node`
				 * returned NULL for the subject and its class handler from the
				 * previous run of this code. So if the tree node is NULL, the
				 * inner `for` cycle shouldn't continue and we try to found
				 * another valid tree node for the same subject, but different
				 * class handler.
				 */
				cb->do_phase = 0;
				continue;
			}
			if (cb->do_phase == 0)
				evhash_foreach_first(cb->hh, t->subject_handlers[cb->ehh_list]);
			for (; cb->hh != NULL; evhash_foreach_next(cb->hh)) {
				if (cb->hh->evname == cb->event->evname) {
					i = do_event_handler(cb);
					if (i > 0) {
						cb->do_phase = 2;
						return i;
					}
				}
				cb->do_phase = 0;
			}
			cb->do_phase = 0;
		}
	}

	if ((cb->do_phase == 0 && c->object.class != NULL) || cb->do_phase == 3) {
		if (cb->do_phase == 0)
			cb->ch = c->object.class->classname->class_handler;
		for (; cb->ch != NULL; cb->ch = cb->ch->next) {
			t = cb->ch->get_tree_node(cb->ch, cb->comm, &(c->object));
			if (t == NULL) {
				/* See comments for the corresponding subject code above. */
				cb->do_phase = 0;
				continue;
			}
			if (cb->do_phase == 0)
				evhash_foreach_first(cb->hh, t->object_handlers[cb->ehh_list]);
			for (; cb->hh != NULL; evhash_foreach_next(cb->hh)) {
				if (cb->hh->evname == cb->event->evname) {
					i = do_event_handler(cb);
					if (i > 0) {
						cb->do_phase = 3;
						return i;
					}
				}
				cb->do_phase = 0;
			}
			cb->do_phase = 0;
		}
	}

	//#if 0
	//if (function_debug != NULL) {
	//	int result;

	//	/* FIXME: uff */
	//	strcpy(cb->context.operation.attr.name, "operation");
	//	strcpy(cb->context.subject.attr.name, "subject");
	//	strcpy(cb->context.object.attr.name, "object");

	//	if (cb->do_phase == 0 || cb->do_phase == 4) {
	//		result = cb->context.result;
	//		i = h->handler(cb, function_debug, &(cb->context));
	//		if (i == 0)
	//			cb->context.result = evaluate_result(result, cb->context.result);
	//		else
	//			cb->context.result = result;
	//	if (i > 0) {
	//		cb->do_phase = 4;
	//		return i;
	//	}
	//	}
	//}
	//#endif

	cb->do_phase = 0;		/* len tak pre istotu */

	/* If this is an event that doesn't have any registered handler (event
	 * handler, class handler for object or subject).
	 */
	if (c->unhandled) {
		//printf("do_event_list: No event executed! [%s]\n",cb->event->acctype.name);
		return -1;
	}

	//printf("do_event_list: return 0, event result is %d\n",c->result);
	return 0;
}

int do_event(struct comm_buffer_s *cb)
{
	int r;

	//printf("do_event: run do_event_list(buf) for buf id %u\n", cb->id);
	r = do_event_list(cb);
	//printf("do_event: buf_id=%u buf->ehh_list=%d do_event_list(buf) returned %d\n",
	//		cb->id, cb->ehh_list, r);

	if (r > 0) {
		//printf("do_event: do_event_list(buf)>0, return %d\n", r);
		return r;
	}

	if (cb->ehh_list == EHH_VS_ALLOW) {
		switch (cb->context.result) {
		case RESULT_FORCE_ALLOW:
		case RESULT_ALLOW:
		default:
			cb->ehh_list = EHH_NOTIFY_ALLOW;
			break;
		case RESULT_DENY:
		case RESULT_FAKE_ALLOW:
			cb->ehh_list = EHH_NOTIFY_DENY;
			break;
		case RESULT_RETRY:
			//printf("do_event: buf->ehh_list == EHH_VS_ALLOW, event result RETRY, do_phase = %d, return %d\n",
			//	cb->do_phase, r);
			return r;
		}

		r = do_event_list(cb);
		//printf("do_event: buf->ehh_list == EHH_VS_ALLOW, event result %d, do_phase = %d, return %d\n",
		//		cb->context.result, cb->do_phase, r);
		return r;
	} else if (cb->ehh_list == EHH_VS_DENY) {
		switch (cb->context.result) {
		case RESULT_FORCE_ALLOW:
			//cb->ehh_list=EHH_NOTIFY_ALLOW;
			//break;
		case RESULT_DENY:
		case RESULT_FAKE_ALLOW:
		case RESULT_ALLOW:
		default:
			cb->ehh_list = EHH_NOTIFY_DENY;
			break;
		case RESULT_RETRY:
			//printf("do_event: buf->ehh_list == EHH_VS_DENY, event result RETRY, do_phase = %d, return %d\n",
			//	cb->do_phase, r);
		return r;
		}

		r = do_event_list(cb);
		//printf("do_event: buf->ehh_list == EHH_VS_DENY, event result %d, do_phase = %d, return %d\n",
		//		cb->context.result, cb->do_phase, r);
		return r;
	}

	//printf("do_event: return %d\n", r);
	return r;
}

int get_empty_context(struct event_context_s *c)
{
	c->operation.next =  &(c->subject);
	c->operation.attr.offset = c->operation.attr.length = 0;
	c->operation.attr.type = MED_TYPE_END;
	c->operation.attr.name[0] = 0;
	c->operation.flags = 0;
	c->operation.class = NULL;
	c->operation.data = NULL;

	c->subject.next =  &(c->object);
	c->subject.attr.offset = c->subject.attr.length = 0;
	c->subject.attr.type = MED_TYPE_END;
	c->subject.attr.name[0] = 0;
	c->subject.flags = 0;
	c->subject.class = NULL;
	c->subject.data = NULL;
	c->object.next = NULL;
	c->object.attr.offset = c->object.attr.length = 0;
	c->object.attr.type = MED_TYPE_END;
	c->object.attr.name[0] = 0;
	c->object.flags = 0;
	c->object.class = NULL;
	c->object.data = NULL;
	c->local_vars = NULL;

	return 0;
}

void event_context_print(struct event_context_s *c, void(*out)(int arg, char *), int arg)
{
	if (c->operation.class == NULL || c->operation.class->comm == NULL)
		out(arg, "[internal]: event ");
	else {
		out(arg, "[\"");
		out(arg, c->operation.class->comm->name);
		out(arg, "\"]: event ");
	}
	out(arg, c->operation.attr.name);
	out(arg, " subject=");
	if (c->subject.class != NULL)
		out(arg, c->subject.class->m.name);
	else
		out(arg, "NONE");
	out(arg, " object=");
	if (c->object.class != NULL)
		out(arg, c->object.class->m.name);
	else
		out(arg, "NONE");
	out(arg, "\n");

	if (c->operation.class != NULL) {
		debug_do_out(debug_do_arg, "operation ");
		object_print(&(c->operation), out, arg);
	}
	if (c->subject.class != NULL) {
		debug_do_out(debug_do_arg, "subject ");
		object_print(&(c->subject), out, arg);
	}
	if (c->object.class != NULL) {
		debug_do_out(debug_do_arg, "object ");
		object_print(&(c->object), out, arg);
	}
	out(arg, "-\n");
}
