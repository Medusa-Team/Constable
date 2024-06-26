// SPDX-License-Identifier: GPL-2.0
/*
 * Constable: generic.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#include "constable.h"
#include "generic.h"
#include "comm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

struct medusa_attribute_s *execute_get_last_attr(void);
char *execute_get_last_data(void);

struct tree_s *generic_get_tree_node(struct class_handler_s *h, struct comm_s *comm, struct object_s *o)
{
	struct tree_s *t;

	t = (struct tree_s *)CINFO(o, h, comm);
	if (t == NULL)
		object_do_sethandler(o);

	t = (struct tree_s *)CINFO(o, h, comm);
	return t;
}

int generic_set_handler(struct class_handler_s *h, struct comm_s *comm, struct object_s *o)
{
	struct tree_s *t;
	int a, inh;
	uintptr_t *cinfo;

	/* musim get_tree_node robit sam, aby sa nezacyklilo */
	cinfo = PCINFO(o, h, comm);
	//printf("ZZZ: aaaaaaaaaaaaaaaaaaaaaaaaa cinfo=%p stack=%p\n",cinfo,&t);

	t = (struct tree_s *)(*cinfo);
	// ak je CINFO neplatne, musi ostat neplatne
	if (t == NULL)
		return -1;

	//printf("ZZZ: riesim generic_set_subject pre %s\n",t->type->name);
	inh = 0;
#ifdef USE_ALT
	for (; t != NULL; t = t->alt) {
#endif
		for (a = 0; a < NR_ACCESS_TYPES; a++) {
			object_add_vs(o, a, t->vs[a]);
			/* !!!! no_vs !!!! */
		}
		object_add_event(o, &(t->events[comm->conn].event[0]));
		if (t->child != NULL || t->regex_child != NULL)
			inh = 1;
#ifdef USE_ALT
	}
#endif

	if (inh && h->user != NULL && ((struct event_names_s *)(h->user))->events[comm->conn]->monitored_operand == o->class)
		object_add_event(o, ((struct event_names_s *)(h->user))->events[comm->conn]->mask);

	return 0;
}

int generic_test_vs(int acctype, struct event_context_s *c)
{
	vs_t vs_s[VS_WORDS];
	vs_t vs_o[VS_WORDS];

	if (c->object.class == NULL)
		return 0;
	if (object_is_invalid(&(c->object)))
		object_do_sethandler(&(c->object));

	object_get_vs(vs_s, acctype, &(c->subject));
	object_get_vs(vs_o, AT_MEMBER, &(c->object));

	return vs_test(vs_o, vs_s);
}

int generic_test_vs_tree(int acctype, struct event_context_s *c, struct tree_s *t)
{
	vs_t vs_s[VS_WORDS];

	object_get_vs(vs_s, acctype, &(c->subject));
	if (vs_test(t->vs[AT_MEMBER], vs_s)) /*!!! no_vs !!!*/
		return 1;
	return 0;
}

int generic_hierarchy_handler_decide(struct comm_buffer_s *cb, struct event_handler_s *h, struct event_context_s *c)
{
	uintptr_t *cinfo;
	struct tree_s *t;
	char *searched_node;
	int r;
	struct class_handler_s *ch;
	vs_t vse[VS_WORDS];

	ch = ((struct g_event_handler_s *)h)->class_handler;
	//printf("generic_hierarchy_handler_decide %s\n", ch->root->type->name);
	cinfo = PCINFO(&(c->subject), ch, cb->comm);

	t = (struct tree_s *)(*cinfo);
	if (t == NULL) {
		if ((ch->flags & GFL_FROM_OBJECT)
			&& c->subject.class == c->object.class)
			// if clone flag is set, copy cinfo from object (presumably parent
			// object)
			t = (struct tree_s *)CINFO(&(c->object), ch, cb->comm);
		if (t == NULL)
			t = ch->root;
		*cinfo = (uintptr_t)t;
		object_do_sethandler(&(c->subject));
	}

	//printf("ZZZ: riesim generic_hierarchy_handler pre %s\n", t->type->name);
	r = ((struct g_event_handler_s *)h)->subhandler->handler(cb, ((struct g_event_handler_s *)h)->subhandler, c);
	if (r > 0)
		return r;

	if ((execute_get_last_attr()->type & 0x0f) != MED_TYPE_STRING)
		searched_node = "";
	else
		searched_node = execute_get_last_data();

	//printf("ZZZ: strom \"%s\": hladam pod uzlom \"%s\" potomka \"%s\"",
	//	t->type->name, t->name, searched_node);
	t = find_one(t, searched_node);
	if (t == NULL) {
		c->result = RESULT_DENY;
		//printf(", nenasiel som NIC\n");
		*PUPTR_COMM_BUF_VAR_DATA(cb, ch->comm_buf_var_data_offset) = *cinfo;
		return -1;
	}

	//printf(", nasiel som \"%s\"\n", t->name);
	//{
	//	int ino;
	//	struct medusa_attribute_s *a;

	//	a = get_attribute(c->subject.class, "dev");
	//	if (a) {
	//		object_get_val(&(c->subject), a, &ino, sizeof(ino));
	//		printf("0x%x:", ino);
	//	}
	//	a = get_attribute(c->subject.class, "ino");
	//	if (a) {
	//		object_get_val(&(c->subject), a, &ino, sizeof(ino));
	//		printf("%d", ino);
	//	}
	//	printf("\n");
	//}

	if ((ch->flags & GFL_USE_VSE)) {
		object_get_vs(vse, AT_ENTER, &(c->subject));

		if (vs_test(t->vs[AT_MEMBER], vse)) /*!!! no_vs !!! */
			*PUPTR_COMM_BUF_VAR_DATA(cb, ch->comm_buf_var_data_offset) = (uintptr_t)t;
		else {
			c->result = RESULT_DENY;
			//printf("ZZZ: nevnaram sa, lebo nemam ENTER!\n");
			*PUPTR_COMM_BUF_VAR_DATA(cb, ch->comm_buf_var_data_offset) = *cinfo;
			return 0;
		}
	} else
		*PUPTR_COMM_BUF_VAR_DATA(cb, ch->comm_buf_var_data_offset) = (uintptr_t)t;

	c->result = RESULT_ALLOW;
	return 0;
}

int generic_hierarchy_handler_notify(struct comm_buffer_s *cb, struct event_handler_s *h, struct event_context_s *c)
{
	uintptr_t *cinfo; // 64-bit or 32-bit size because a pointer will be stored here
	struct class_handler_s *ch;

	ch = ((struct g_event_handler_s *)h)->class_handler;
	//printf("generic_hierarchy_handler_notify %s\n", ch->root->type->name);
	cinfo = PCINFO(&(c->subject), ch, cb->comm);

	//printf("generic_hierarchy_handler_notify --- c->result=%d\n", c->result);
	//if( c->result == RESULT_ALLOW || c->result == RESULT_FORCE_ALLOW ) {
	*cinfo = *PUPTR_COMM_BUF_VAR_DATA(cb, ch->comm_buf_var_data_offset);
	object_do_sethandler(&(c->subject));
	//}

	return 0;
}

int generic_get_vs(struct class_handler_s *h, struct comm_s *comm, struct object_s *o, vs_t *vs, int n)
{
	struct tree_s *t;

	t = h->get_tree_node(h, comm, o);
	if (t == NULL)
		return -1;
	//printf("ZZZ: riesim generic_get_vs pre %s\n", t->type->name);
	vs_add(t->vs[n], vs);
	/*!!! no_vs !!! */
	return 0;
}

int generic_enter_tree_node(struct class_handler_s *h, struct comm_s *comm, struct object_s *o, struct tree_s *node)
{
	int perm;
	vs_t vse[VS_WORDS];
	char **errstr;

	if (node->type->class_handler != h) {
		errstr = (char **) pthread_getspecific(errstr_key);
		*errstr = Out_of_memory;
		return -1;
	}
	if (h->classname->classes[comm->conn] != o->class) {
		errstr = (char **) pthread_getspecific(errstr_key);
		*errstr = Out_of_memory;
		return -1;
	}
	if (h->cinfo_offset[comm->conn] < 0) {
		errstr = (char **) pthread_getspecific(errstr_key);
		*errstr = Out_of_memory;
		return -1;
	}

	perm = 1;
	if ((h->flags & GFL_USE_VSE)) {
		object_get_vs(vse, AT_ENTER, o);
		if (!vs_test(node->vs[AT_MEMBER], vse)) /*!!! no_vs !!! */
			perm = 0;
	}
	if (perm) {
		CINFO(o, h, comm) = (uintptr_t)node;
		object_do_sethandler(o);
		return 1;
	}

	errstr = (char **) pthread_getspecific(errstr_key);
	*errstr = Out_of_memory;

	return 0;
}

struct space_s *generic_get_primary_space(struct class_handler_s *h, struct comm_s *comm, struct object_s *o)
{
	struct tree_s *t;
	char **errstr;

	if (h->classname->classes[comm->conn] != o->class) {
		errstr = (char **) pthread_getspecific(errstr_key);
		*errstr = Out_of_memory;
		return NULL;
	}
	if (h->cinfo_offset[comm->conn] < 0) {
		errstr = (char **) pthread_getspecific(errstr_key);
		*errstr = Out_of_memory;
		return NULL;
	}

	t = h->get_tree_node(h, comm, o);
	if (t == NULL) {
		errstr = (char **) pthread_getspecific(errstr_key);
		*errstr = Out_of_memory;
		return NULL;
	}

	return t->primary_space;
}


int generic_init_comm(struct class_handler_s *h, struct comm_s *comm)
{
	struct event_type_s *event = NULL;
	struct class_s *class;
	int x;

	//printf("generic init\n");
	h->cinfo_offset[comm->conn] = -1;
	class = h->classname->classes[comm->conn];
	if (class == NULL) {
		comm->conf_error(comm, "Undefined class %s", h->classname->name);
		return 0;
	}
	if (h->user != NULL) {
		event = ((struct event_names_s *)(h->user))->events[comm->conn];
		if (event != NULL) {
			if (class != event->op[0])
				comm->conf_error(comm, "%s: class of tree %s is not a class of subject of operation %s",
					__func__, h->root->type->name, event->acctype.name);
		} else {
			comm->conf_error(comm, "Undefined event %s", ((struct event_names_s *)(h->user))->name);
			return -1;
		}
	}

	// if event is TRIGGERED_AT_SUBJECT | TRIGGERED_BY_OBJECT_BIT
	if (event != NULL && ((event->acctype.actbit & 0xc000) == 0x4000))
		x = class_alloc_object_cinfo(class);
	else {
		x = class_alloc_subject_cinfo(class);
		if (x < 0)
			x = class_alloc_object_cinfo(class);
	}
	if (x < 0) {
		comm->conf_error(comm, "Can't alloc cinfo for %s", h->root->type->name);
		return -1;
	}

	h->cinfo_offset[comm->conn] = x;
	return 0;
}

struct class_handler_s *generic_get_new_class_handler_s(struct class_names_s *class, int size)
{
	struct class_handler_s *ch;

	if (size < (int)(sizeof(struct class_handler_s)))
		size = sizeof(struct class_handler_s);

	ch = malloc(size);
	if (ch == NULL)
		return NULL;

	ch->cinfo_offset = (int *)comm_new_array(sizeof(int));
	if (ch->cinfo_offset == NULL) {
		free(ch);
		return NULL;
	}

	ch->comm_buf_var_data_offset =  -1;
	ch->classname = class;
	ch->flags = 0;
	ch->user = NULL;
	ch->init_comm = generic_init_comm;
	ch->set_handler = generic_set_handler;
	ch->get_vs = generic_get_vs;
	ch->get_tree_node = generic_get_tree_node;
	ch->get_primary_space = generic_get_primary_space;
	ch->enter_tree_node = generic_enter_tree_node;
	ch->root = NULL;

	return ch;
}

/*
 * Called only from language/mcp.c:conf_lang_param_out() from
 * tree registration (`case Ptreereg').
 *
 * @name: Name of the tree to be registered.
 * @subhandler: The expression <exp> to be evaluated in tree definition:
 *	tree "name" [test_enter | clone ...] of <class> [by <op> <exp>];
 *	@subhandler's handler is set to execute_handler() function and
 *	it's name to the "name" of the tree to be registered.
 * @event: Event <op> triggering the @subhandler's evaluation.
 * @class: Name of object's type of the tree to be registered. One tree
 *	can hold only one type of objects. The name of this type is
 *	stored here (in @class->name field).
 * @flags: See generic.h for GFL_USE_VSE and GFL_FROM_OBJECT documentation.
 */
int generic_init(char *name, struct event_handler_s *subhandler, struct event_names_s *event, struct class_names_s *class, int flags)
{
	struct tree_type_s *type;
	struct g_event_handler_s *eh;
	struct class_handler_s *ch;

	//printf("ZZZZZZ generic_init: idem riesit %s\n", name);
	ch = generic_get_new_class_handler_s(class, -1);
	if (ch == NULL) {
		init_error(Out_of_memory);
		return -1;
	}
	ch->comm_buf_var_data_offset = comm_alloc_buf_var_data(sizeof(uintptr_t));
	ch->flags = flags;
	ch->user = (void *)event;

	type = malloc(sizeof(struct tree_type_s) + strlen(name) + 1);
	if (type == NULL) {
		init_error(Out_of_memory);
		free(ch->cinfo_offset);
		free(ch);
		return -1;
	}

	strcpy(type->name, name);
	type->size = sizeof(struct tree_s);
	type->class_handler = ch;
	type->init = NULL;
	type->child_type = type;

	ch->root = register_tree_type(type);
	if (ch->root == NULL) {
		free(ch->cinfo_offset);
		free(ch);
		free(type);
		return init_error("Can't register tree type %s", name);
	}

	if (class_add_handler(class, ch) < 0) {
		free(ch->cinfo_offset);
		free(ch);
		free(type);
		return init_error("Can't add classhandler for %s/", name);
	}

	//printf("ZZZZZZZZZZZZZZ name=%s subhandler=%p event=%p\n", name, subhandler, event);
	if (subhandler && event) {
		eh = malloc(2 * sizeof(struct g_event_handler_s));
		if (eh == NULL) {
			init_error(Out_of_memory);
			free(ch->cinfo_offset);
			free(ch);
			free(type);
			return -1;
		}

		eh->h.op_name[0] = ':';
		strncpy(eh->h.op_name + 1, type->name, MEDUSA_OPNAME_MAX - 1);
		eh->h.handler = generic_hierarchy_handler_decide;
		eh->h.local_vars = NULL;
		eh->subhandler = subhandler;
		eh->class_handler = ch;
		//printf("ZZZ: generic_init: registrujem event_handler\n");
		if (register_event_handler((struct event_handler_s *)eh, event, &(event->handlers_hash[EHH_VS_ALLOW]), ALL_OBJ, ALL_OBJ) < 0) {
			free(eh);
			return init_error("Can't register event handler for %s/", name);
		}

		eh[1] = eh[0];
		eh[1].h.op_name[0] = '.';
		eh[1].h.handler = generic_hierarchy_handler_notify;
		if (register_event_handler((struct event_handler_s *)(eh+1), event, &(event->handlers_hash[EHH_NOTIFY_ALLOW]), ALL_OBJ, ALL_OBJ) < 0) {
			free(eh);
			return init_error("Can't register event notify handler for %s/", name);
		}
	}

	return 0;
}
