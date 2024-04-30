// SPDX-License-Identifier: GPL-2.0
/*
 * Constable: space.c
 * (c) 2002 by Marek Zelem <marek@terminus.sk>
 * (c) 2024 by Matus Jokay <matus.jokay@gmail.com>
 */

#include "constable.h"
#include "comm.h"
#include "space.h"

#include <stdio.h>
#include <pthread.h>

/* FIXME: check infinite loops between space_for_every_path, is_included and is_excluded! */
#define	MAX_SPACE_PATH	32

/*
 * Auxiliary structure for detecting looping of spaces when evaluating space
 * members.
 *
 * @path: Trace of spaces examinated during space members evaluation. Pointers
 *	to examinated spaces are stored in the @path array starting at index 0.
 * @n: Actual count of the elements in the @path.
 */
struct space_path_s {
	struct space_s *path[MAX_SPACE_PATH];
	int n;
};

typedef void (*fepf_t)(struct tree_s *t, void *arg);
static int space_path_exclude(struct space_path_s *sp, struct tree_s *t);

/* Linked list of all spaces. */
struct space_s *global_spaces;

/*
 * new_path() create a new element of a linked list of paths in some space:
 * fill the data pointer by @path_or_space, fill the element data type by @type
 * and append it before the @head, so the new element becomes a new head of the
 * linked list.
 *
 * @head: List head address; value on it can be %NULL, if the list is empty.
 * @path_or_space: Data stored in the newly created list element.
 * @type: Data type stored in the newly created list element (%LTREE_T_TREE
 *	or %LTREE_T_SPACE, see space.h for details)./
 */
static struct ltree_s *new_path(struct ltree_s **head, void *path_or_space,
			        int type)
{
	char **errstr;
	struct ltree_s *l;

	l = malloc(sizeof(struct ltree_s));
	if (l == NULL) {
		errstr = (char **) pthread_getspecific(errstr_key);
		*errstr = Out_of_memory;
		return NULL;
	}

	/* Set list element's data pointer and type. */
	l->path_or_space = path_or_space;
	l->type = type;

	/* Set the new list element as a new head of the list. */
	l->prev =  *head;
	*head = l;

	return l;
}

/*
 * new_levent() create a new element of a linked list of levent_s structs. The
 * new element will be filled with the data (@handler, @subject, @object) and
 * appended before the @head, so the new element becomes a new head of the
 * linked list.
 *
 * @head: List head address; value on it can be %NULL, if the list is empty.
 * @handler: Handler of an event with corresponding @subject and @object.
 * @subject: %NULL or space representing subject of the event.
 * @object: %NULL or space representing object of the event.
 *
 * Note: At least one of @subject/@object should be not %NULL.
 *
 * For more details see documentation of `struct levent_s' in space.h.
 */
static struct levent_s *new_levent(struct levent_s **head,
				   struct event_handler_s *handler,
				   struct space_s *subject,
				   struct space_s *object)
{
	char **errstr;
	struct levent_s *l;

	l = malloc(sizeof(struct levent_s));
	if (l == NULL) {
		errstr = (char **) pthread_getspecific(errstr_key);
		*errstr = Out_of_memory;
		return NULL;
	}

	/* Set list element's data pointers. */
	l->handler = handler;
	l->subject = subject;
	l->object = object;

	/* Set the new list element as a new head of the list. */
	l->prev =  *head;
	*head = l;

	return l;
}

/*
 * space_find() find a space in the list `global_spaces' based on the @name.
 *
 * @name: The name of a space to find.
 *
 * Returns %NULL if no space with @name is found.
 */
struct space_s *space_find(char *name)
{
	struct space_s *t;

	for (t = global_spaces; t != NULL; t = t->next)
		if (!strcmp(t->name, name))
			return t;
	return NULL;
}

/*
 * space_create() create a new space with a @name. If a space with the @name
 * already exists or there is no memory to allocate a new space, %NULL is
 * returned. Otherwise the newly created space is returned.
 *
 * @name: The name of a space to be created. If a space with this @name exists
 *	in the global list of spaces, %NULL is returned. The @name can be
 *	%NULL. In this case is created anonymous space with %ANON_SPACE_NAME
 *	name. There can be multiple anonymous spaces in the global space list.
 * @primary: If set, the space will be primary. For explanation, see the
 *	documentation of `struct space_s' in space.h.
 *
 * This function creates a space, but the space will be only declared (empty
 * and not used yet). For explanation of allocation, declaration and definition
 * of a space see documentation of `struct space_s' in space.h.
 */
struct space_s *space_create(char *name, bool primary)
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

	strcpy(t->name, name);	/* the space will be declared */
	for (a = 0; a < NR_ACCESS_TYPES; a++)
		vs_clear(t->vs[a]);
	vs_clear(t->vs_id);	/* the space is not defined yet */
	t->levent = NULL;	/* the space is not used in any event yet */
	t->ltree = NULL;	/* the space is without any member yet */
	t->primary = primary;
	t->processed = false;
	t->used = false;
	t->next = global_spaces;
	global_spaces = t;	/* set a new head of the global space list */

	return t;
}

/*
 * space_path_add() add another @space into array @sp, if it's not in the @sp
 * yet. If it's already in the array, a loop in examined trace is detected.
 *
 * @sp: An 'array' of examined spaces where @space will be insert to.
 * @space: A space to be inserted into @sp.
 *
 * Function keeps trace of all examined spaces and checks for an eventual loop
 * in it.
 */
static int space_path_add(struct space_path_s *sp, struct space_s *space)
{
	int i;

	/* set a space as visited, as it's being used (maybe by another space) */
	space->used = true;

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

/*
 * is_included_i() test whether the @test node is member of a @space. If yes,
 * %true is returned, %false otherwise.
 *
 * @test: The node whose membership in a @space is tested.
 * @sp: An auxiliary structure that is used to detect cycles when evaluating
 *	@test's membership in the spaces. See the documentation of struct
 *	space_path_s. Each space visited in the process of membership
 *	evaluation is stored into @sp and forms a trace of the evaluation;
 *	see space_path_add() function.
 * @space: A space within which the @test node membership is evaluated.
 * @force_recursive: A flag specifying recursive membership evaluation. If
 *	set, @test node can be a descendant of some node member of the @space.
 *
 * The membership evaluation algorithm consist of these steps:
 * 1) Do a loop detection for the @space.
 * 2) For each member of the @space:
 *	a) If a member of the @space is another space, evaluate membership of
 *	   @test in it. IOW, apply the recursion.
 *	b) If a member of the @space is a node of UNST, @test belongs to the
 *	   @space, if:
 *		i) @test is the member itself, or
 *		ii) @test is a descendant of the member and recursion flag is
 *		    set on @test or on the member.
 *	   Each of above mentioned conditions is necessary but no sufficient.
 *	   At the end, there should be made a test whether the @test node is
 *	   not excluded from the spaces in actual evaluation trace recorded
 *	   in @sp.
 *
 * Note: Excluding a @test from @sp takes precedence over including it. This
 * means that it is enough to find a single case of its exclusion and further
 * inclusions are not taken into consideration.
 */
/* is_included_i: 0 - not, 1 - yes */
// TODO: 1) zmenit typ navratovej hodnoty na boolean
//	 2) zmenit typ force_recursive na boolean, aj v dalsich funkciach
static int is_included_i(struct tree_s *test, struct space_path_s *sp,
			 struct space_s *space, bool force_recursive)
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
				if ((force_recursive || l->type & LTREE_RECURSIVE)
						&& tree_is_offspring(test, l->path_or_space))
					return !space_path_exclude(sp, test);
			}
			break;
		case LTREE_T_SPACE:
			if (!(l->type&LTREE_EXCLUDE)) {
				if (is_included_i(test, sp, l->path_or_space,
						l->type & LTREE_RECURSIVE || force_recursive))
					return 1;
			}
			break;
		}
	}

	return 0;
}

/*
 * is_included() return an indication whether the @test node is included in
 * the @space.
 *
 * @test: The node whose membership in a @space is tested.
 * @space: A space within which the @test node membership is evaluated.
 * @force_recursive: A flag specifying recursive membership evaluation. If
 *	set, @test node can be a descendant of some node member of the @space.
 *
 * Return values:
 *	0 if the @test is not included in the @space,
 *	1 if the @test is included in the @space.
 */
static int is_included(struct tree_s *test, struct space_s *space,
		       bool force_recursive)
{
	struct space_path_s sp;

	sp.n = 0;
	return is_included_i(test, &sp, space, force_recursive);
}

/*
 * is_excluded_i() return an indication whether the @test node is excluded
 * from the @space.
 *
 * @test: The node whose membership in a @space is tested.
 * @space: A space within which the @test node membership is evaluated.
 *
 * Return values:
 *	0 if the @test is not excluded from the @space,
 *	1 if the @test is excluded from the @space,
 *	2 if the @test and its childs are excluded from the @space.
 */
static int is_excluded_i(struct tree_s *test, struct space_s *space)
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
					// a node is excluded recursively
					return 2;
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

/*
 * space_path_exclude() test whether the @test node is excluded from the
 * trace of the evaluation @sp.
 *
 * @test: The node whose exclusion from the @sp is tested.
 * @sp: An auxiliary structure that is used to detect cycles when evaluating
 *	@test's membership in the spaces. See the documentation of struct
 *	space_path_s. Each space visited in the process of membership
 *	evaluation is stored into @sp and forms a trace of the evaluation;
 *	see space_path_add() function.
 *
 * Return values:
 *	0 if the @test is not excluded from the @sp,
 *	1 if the @test is excluded from the @sp,
 *	2 if the @test and its childs are excluded from the @sp.
 */
static int space_path_exclude(struct space_path_s *sp, struct tree_s *test)
{
	int i, r, rn;

	r = 0;
	for (i = 0; i < sp->n; i++) {
		rn = is_excluded_i(test, sp->path[i]);
		r = rn > r ? rn : r;
		if (r > 1)
			break;
	}
	return r;
}

/*
 * Auxiliary structure to hold arguments while using tree_for_alternatives()
 * function call.
 *
 * @sp: A trace of the evaluation. See the documentation of struct space_path_s
 *	and space_path_add() function.
 * @recursive: A flag specifying recursive membership evaluation.
 * @func: A function to be applied to a node of UNST, if it is not excluded
 *	from the @sp. The tree node is produced by tree_for_alternatives().
 * @arg: Arguments of the @func.
 */
struct space_for_one_path_s {
	struct space_path_s *sp;
	bool recursive;
	fepf_t func;
	void *arg;
};

/*
 * space_for_one_path_i() is applied to each node @t of UNST produced by
 * tree_for_alternatives() function.
 *
 * @t: A node of UNST produced by tree_for_alternatives(). On this node is
 *	applied @a->func function with @a->arg arguments, if the node is not
 *	excluded from the @a->sp trace. If @a->recursive is set, the function
 *	space_for_one_path_i() is applied for the childs of @t, too.
 * @a: Auxiliary structure holding arguments for space_for_one_path_i():
 *	-> sp: A trace of the evaluation.
 *	-> recursive: A flag specifying recursive evaluation.
 *	-> func: A function to be applied to a node @t of UNST.
 *	-> arg: Arguments of the ->func.
 *
 * Function has no return value.
 */
static void space_for_one_path_i(struct tree_s *t,
				 struct space_for_one_path_s *a)
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

/*
 * space_for_one_path() call space_for_one_path_i() for each alternative of a
 * @t node of UNST. In other words, @func(@arg) is applied to @t and all
 * alternatives of it, if @t is not excluded from @sp. Furthermore, @func(@arg)
 * is applied to all children of @t, if @recursive is set and a respective
 * child is not excluded from @sp.
 *
 * @t: A node of UNST, for which (alternatives and children if @recursive is
 *	set) @func(@arg) will be applied (if the related node candidate is not
 *	excluded from @sp).
 * @sp: A trace of the space evaluation.
 * @recursive: A flag specifying recursive evaluation.
 * @func: A function to be applied to a node @t of UNST.
 * @arg: Arguments of the @func.
 */
static void space_for_one_path(struct tree_s *t, struct space_path_s *sp,
			       bool recursive, fepf_t func, void *arg)
{
	struct space_for_one_path_s a;

	a.sp = sp;
	a.recursive = recursive;
	a.func = func;
	a.arg = arg;
	tree_for_alternatives(t, (fepf_t)space_for_one_path_i, &a);
}

/*
 * space_for_every_path_i() apply @func(@arg) to each member of the @space.
 * If a member of @space is another space, space_for_every_path_i() is called
 * recursively. Every time the function is called, @space is stored in the
 * trace @sp.
 *
 * @space: A space to whose node members the @func(@arg) should be applied.
 * @sp: A trace of the space evaluation.
 * @force_recursive: A flag specifying recursive evaluation.
 * @func: A function to be applied to each node member of @space.
 * @arg: Arguments of the @func.
 *
 * Return -1 on error, 0 otherwise.
 */
static int space_for_every_path_i(struct space_s *space, struct space_path_s *sp,
				  bool force_recursive, fepf_t func, void *arg)
{
	struct ltree_s *l;

	if (!space_path_add(sp, space))
		return -1;

	for (l = space->ltree; l != NULL; l = l->prev) {
		switch (l->type & LTREE_T_MASK) {
		case LTREE_T_TREE:
			if (!(l->type & LTREE_EXCLUDE))
				space_for_one_path(l->path_or_space, sp,
					l->type & LTREE_RECURSIVE || force_recursive,
					func, arg);
			break;
		case LTREE_T_SPACE:
			if (!(l->type & LTREE_EXCLUDE))
				space_for_every_path_i(l->path_or_space, sp,
					l->type & LTREE_RECURSIVE || force_recursive,
					func, arg);
			break;
		}
	}

	sp->n--;
	return 0;
}

/*
 * space_for_every_member() apply @func(@arg) to each member of a @space.
 *
 * @space: A space to whose members the @func(@arg) should be applied.
 * @func: A function to be applied to every member of @space.
 * @arg: Arguments of the @func.
 *
 * There are four @func used with space_for_every_member():
 * 1) tree_set_primary_space_do()
 * 2) tree_add_event_mask_do()
 * 3) tree_add_vs_do()
 * 4) tree_clear_visited_do()
 */
static void space_for_every_member(struct space_s *space, fepf_t func, void *arg)
{
	struct tree_s **members = space->members.array;

	for (int i = 0; i < space->members.count; i++)
		func(members[i], arg);
}

/*
 * space_for_every_path() initialize auxiliary structure for running internal
 * (recursive) execution function space_for_every_path_i(). The function should
 * apply @func(@arg) for every node member of @space.
 *
 * @space: A space to whose node members the @func(@arg) should be applied.
 * @func: A function to be applied to every node member of @space.
 * @arg: Arguments of the @func.
 *
 * There is only one @func used with space_for_every_path():
 * tree_get_visited_do(), which makes a list of members of a space.
 */
static int space_for_every_path(struct space_s *space, fepf_t func, void *arg)
{
	struct space_path_s sp;

	sp.n = 0;
	return space_for_every_path_i(space, &sp, false, func, arg);
}

/*
 * tree_set_primary_space_do() set the primary @space for a given node @t of
 * UNST.
 *
 * @t: A node of UNST for which will be set the primary space to @space.
 * @space: A space to be set as primary in a given node @t of UNST.
 */
static void tree_set_primary_space_do(struct tree_s *t, struct space_s *space)
{
	if (t->primary_space != NULL && t->primary_space != space)
		fprintf(stderr,
			"Warning: Redefinition of primary space '%s' to '%s'\n",
			t->primary_space->name, space->name);
	t->primary_space = space;
}

/*
 * Auxiliary structure which holds two arguments for tree_add_event_mask_do().
 *
 * @conn: Communication channel number (Constable can handle multiple
 *	connections to various applications).
 * @type: Event type.
 */
struct tree_add_event_mask_do_s {
	int conn;
	struct event_type_s *type;
};

/*
 * tree_add_event_mask_do() set @arg->type event to be monitored in the node @p
 * of UNST.
 *
 * @p: A node of UNST for which will be set @arg->type event.
 * @arg: Event type to be set and communication channel number.
 */
static void tree_add_event_mask_do(struct tree_s *p,
				   struct tree_add_event_mask_do_s *arg)
{
	if (arg->type->monitored_operand == p->type->class_handler->classname->classes[arg->conn])
		event_mask_or2(p->events[arg->conn].event, arg->type->mask);
	//else
	//	arg->type->monitored_operand->comm->conf_error(arg->type->monitored_operand->comm, "event %s object class mismatch", arg->type->evname->name);
}

/*
 * Auxiliary structure which holds two arguments for tree_add_vs_do().
 *
 * @which: Specifies which set of virtual spaces will be modified.
 * @vs: A set of virtual spaces to be set in the node of UNST.
 */
struct tree_add_vs_do_s {
	int which;
	const vs_t *vs;
};

/*
 * tree_add_vs_do() set virtual spaces @arg->vs in the node @p of UNST.
 * @arg->which specifies which set of virtual spaces will be modified.
 *
 * @p: A node of UNST in which will be set @arg->vs set of virtual spaces.
 * @arg: Specification of the set of virtual spaces to be modified and a set of
 *	virtual spaces to be set.
 */
static void tree_add_vs_do(struct tree_s *p, struct tree_add_vs_do_s *arg)
{
	vs_add(arg->vs, p->vs[arg->which]);
}

/*
 * tree_set_visited_do() set a @t->visited flag to %true.
 *
 * @p: A node of UNST which will be marked as visited.
 * @arg: Structure holding information about members of the traversed space:
 *	the count of the members and an array of pointers to each of them.
 *
 * Function is used for real member counting of a space. The real member is a
 * UNST node, not another space, so for counting is used space traversing
 * mechanism, the space_for_every_path() function call.
 *
 * Deduplication is guaranteed by @p->visited flag: inclusion of a node in
 * the group of space members is done only if the node has not yet been
 * visited.
 */
static void tree_get_visited_do(struct tree_s *p, struct members_s *arg)
{
	if (!p->visited) {
		p->visited = true;
		arg->count++;
		arg->array = reallocarray(arg->array, arg->count,
				 sizeof(struct tree_s *));
		if (!arg->array)
			fatal("Error: Can't alloc memory for space members.");
		arg->array[arg->count - 1] = p;
	}
}

/*
 * tree_clear_visited_do() clear a @t->visited flag to %false.
 */
static void tree_clear_visited_do(struct tree_s *t, void *)
{
	t->visited = false;
}

/*
 * space_add_path() add path/space node @path_or_space to the list of paths of
 * @space and set its @type.
 *
 * @space: A space into which @path_or_space of given @type will be inserted.
 * @type: Type of the new node: a space or a node of UNST.
 * @path_or_space: New path/space which will be added to virtual @space.
 *
 * Returns 0 on success, -1 otherwise (i.e. OOM error).
 */
int space_add_path(struct space_s *space, int type, void *path_or_space)
{
	if (path_or_space == NULL)
		return -1;

	if (new_path(&(space->ltree), path_or_space, type) == NULL)
		return -1;
	return 0;
}

/*
 * space_apply_all() set 1) membership of UNST nodes into virtual spaces and
 * 2) information about primary spaces.
 *
 * Note: space_apply_all() must be called after all spaces are defined.
 */
int space_apply_all(void)
{
	struct space_s *space, *space_prev, *space_next;
	struct tree_add_vs_do_s arg;
	int a;

	tree_expand_alternatives();

	space = global_spaces;
	while (space != NULL) {
		/* Apply only to used but not processed spaces yet. */
		if (space->used && !space->processed) {
			/*
			 * Get count of UNST nodes (i.e. real members) of the
			 * space.
			 */
			space->members.count = 0;
			space->members.array = NULL;
			space_for_every_path(space,
			    (fepf_t)tree_get_visited_do, &(space->members));

			/*
			 * Clear visited flags, as a node can belong to many
			 * spaces.
			 */
			space_for_every_member(space,
			    (fepf_t)tree_clear_visited_do, NULL);

#ifdef DEBUG_TRACE
			printf("space '%s' has %d member(s):", space->name,
				space->members.count);
			for (int i = 0; i < space->members.count; i++)
				printf(" '%s'", space->members.array[i]->name);
			printf("\n");
#endif // DEBUG_TRACE

			/* Space with no member but with ID can't be ignored. */
			if (!vs_isclear(space->vs_id) && !space->members.count)
				fatal("space '%s' has no path member(s)!",
				    space->name);
			/* Space with no member and no ID will be ignored. */
			if (!space->members.count) {
				fprintf(stderr, "Warning: Space '%s' has no "
				    "path member(s), will be ignored.\n",
				    space->name);
				space->used = false;
				goto init_restart;
			}
			/* Set space as processed. */
			space->processed = true;

init_restart:
			/*
			 * Restart processing of all spaces, as some space(s)
			 * may be marked as visited during last space traversal.
			 */
			space = global_spaces;
			continue;
		}

		space = space->next;
	}

	/* remove unused spaces from the memory */
	space_prev = NULL;
	space = global_spaces;
	while (space != NULL) {
		if (!space->processed) {
#ifdef DEBUG_TRACE
			fprintf(stderr, "Warning: Space '%s' is defined but "
			    "not used, will be ignored.\n", space->name);
#endif // DEBUG_TRACE

			/* it's error, if space is used but not processed */
			if (!vs_isclear(space->vs_id))
				fatal("Space '%s' is not processed, but still"
				    "used.", space->name);

			/* free the space members */
			if (space->ltree) {
				struct ltree_s *l_next, *l = space->ltree;
				while (l) {
					l_next = l->prev;
					free(l);
					l = l_next;
				}
				space->ltree = NULL;
			}

			/* remove unused space from the global space list */
			if (space_prev)
				space_prev->next = space->next;
			else
				global_spaces = space->next;

			/* and free it */
			space_next = space->next;
			free(space);
			space = space_next;
			continue;
		}

		space_prev = space;
		space = space->next;
	}

	/* process the remaining (active) spaces */
	for (space = global_spaces; space != NULL; space = space->next) {
		if (space->primary)
			space_for_every_member(space,
			    (fepf_t)tree_set_primary_space_do, space);
		for (a = 0; a < NR_ACCESS_TYPES; a++) {
			if (vs_isclear(space->vs[a]))
				continue;
			arg.which = a;
			arg.vs = space->vs[a];
			space_for_every_member(space,
			    (fepf_t)tree_add_vs_do, &arg);
		}
	}

	return 0;
}

/*
 * space_add_event() register an event @handler for given operation mode
 * @ehh_list. The subject of the registered operation is stored in @subject
 * (or @subj_node) and the corresponding object in @object/@obj_node.
 *
 * Called from conf_lang.c:conf_lang_param_out() in event registration
 * case (see 'Preg' handling).
 *	<subj> <event> [:ehh_list] [<obj>] { <cmd> ... }
 *
 * @handler: Event name (@handler->op_name) of the event to be registered.
 * @ehh_list: Operation mode, in which the handler should be executed:
 *		1) EHH_VS_ALLOW
 *		2) EHH_VS_DENY
 *		3) EHH_NOTIFY_ALLOW
 *		4) EHH_NOTIFY_DENY
 *	New event is added to the appropriate list of events based on
 *	value of @ehh_list.
 * @subject: space identification (ALL_OBJ or subject's space). Never NULL.
 * @object: NULL or space identification (ALL_OBJ or object's space). Can be
 *	NULL only if <obj> is omitted in event definition.
 * @subj_node: NULL or subject's node in Unified Name Space Tree. If not NULL,
 *	@subject is set to ALL_OBJ.
 * @obj_node: NULL or object's node in Unified Name Space Tree. If not NULL,
 *	@object is set to ALL_OBJ.
 *
 * Note: This function does not activate triggering of registered event on all
 * relevant nodes in Unified Name Space Tree. That means, does not change event
 * mask of nodes. For this activity see space_init_event_mask() function.
 * Information (the triple <subject space, event, object space>) for later use
 * in space_init_event_mask() is stored in levent_s structure created and
 * filled by new_levent() function.
 */
int space_add_event(struct event_handler_s *handler, int ehh_list,
		    struct space_s *subject, struct space_s *object,
		    struct tree_s *subj_node, struct tree_s *obj_node)
{
	struct event_names_s *type;

	if (ehh_list < 0 || ehh_list >= EHH_LISTS) {
		error("Invalid ehh_list of handler");
		return -1;
	}

	type = event_type_find_name(handler->op_name, true);
	if (type == NULL) {
		error("Event %s does not exist\n", handler->op_name);
		return -1;
	}

	/*
	 * 1) @subject cannot be NULL; paranoYa check
	 * 2) @subject is ALL_OBJ if:
	 *    a) is explicitly defined as '*' (asterix) in event definition;
	 *       @subj_node in this case must be NULL
	 *    b) @subj_node is not NULL
	 */
	if (subject != NULL && subject != ALL_OBJ &&
	    new_levent(&(subject->levent), handler, subject, object) == NULL)
		return -1;
	/*
	 * 1) @object is NULL, if it's omitted in event definition;
	 *    @obj_node must be NULL, too.
	 * 2) @object is ALL_OBJ analogically as in case of @subject, see
	 *    comment above for @subject == ALL_OBJ
	 */
	if (object != NULL && object != ALL_OBJ &&
	    new_levent(&(object->levent), handler, subject, object) == NULL)
		return -1;

	// TODO: ked je v konfigu `* getprocess {...}', subject = ALL_OBJ, object = NULL,
	// subj_node = NULL, obj_node = NULL. Ale nevyhodnoti sa spravne, ze chyba objekt.
	// Vid tree_comm_reinit(), tam bY sa to malo...
	// Jaaaaaj ved to preto, lebo je to zavesene na type->handlers_hash a tam este
	// nemame spravenu kontrolu, vid TODO 3 v space_init_event_mask() ;)
	printf("YYY: %s subject=%p, object=%p, subj_node=%p, obj_node=%p\n", handler->op_name, subject, object, subj_node, obj_node);

	/* @subject and @object must be both ALL_OBJ */
	if (subj_node && obj_node) {
		/* create anonymous and not primary space */
		object = space_create(NULL, false);
		if (object == NULL)
			return -1;
		space_add_path(object, 0, obj_node);
		if (new_levent(&(object->levent), handler, subject, object) == NULL)
			return -1;
		//printf("ZZZ: subj=%s obj=%s tak registrujem podla svetov\n", subj_node->name, obj_node->name);
		register_event_handler(handler, type,
				       &(subj_node->subject_handlers[ehh_list]),
				       space_get_vs(subject),
				       space_get_vs(object));
	}
	/* @subject = ALL_OBJ, @object is arbitrary, @obj_node = NULL */
	else if (subj_node) {
		//printf("ZZZ: registrujem pri subjekte %s\n", subj_node->name);
		register_event_handler(handler, type,
				       &(subj_node->subject_handlers[ehh_list]),
				       space_get_vs(subject),
				       space_get_vs(object));
	}
	/* @subject != NULL, @object = ALL_OBJ, @subj_node = NULL */
	else if (obj_node) {
		//printf("ZZZ: registrujem pri objekte %s\n", obj_node->name);
		register_event_handler(handler, type,
				       &(obj_node->object_handlers[ehh_list]),
				       space_get_vs(subject),
				       space_get_vs(object));
	}
	/* @subject != NULL, @object is arbitrary, @subj_node = @obj_node = NULL */
	else {
		//printf("ZZZ: registrujem '%s' podla svetov\n", handler->op_name);
		register_event_handler(handler, type,
				       &(type->handlers_hash[ehh_list]),
				       space_get_vs(subject),
				       space_get_vs(object));
	}

	return 0;
}

/*
 * tree_comm_reinit() reinitialize triggering of events on nodes of UNST.
 *
 * @comm: Identification of the established communication interface with a
 *	  supervised application.
 * @t: A node od UNST on which (and its children, too) the reinitialization will
 *     be applied.
 *
 * The reinitialization concerns those events that have an UNST node defined
 * as <subj> and/or <obj>, see event definition in conf_lang.c:
 *	<subj> <event> [:ehh_list] [<obj>] { <cmd> ... }
 *
 * As event's definitions in the configuration file precede their byte
 * representations (which are transfered after the connection with the
 * controlled application is established), there can be some invalid event
 * definition(s) from the PoV of subject/object types or event name(s) itself.
 * These checks can be made only after the connection was established.
 *
 * This function checks validity of all subject and object events in the node
 * @t of UNST as follows:
 * 1) the event name should be registered and there should be known an
 *    event's byte representation on the given communication channel;
 * 2) subject/object type validity: the type specified in the config file
 *    definition should be the same as in the byte representation send from
 *    the supervised application.
 */
// TODO: je mozne prerobit aj tuto funkciu, nech nema cele comm, ale iba
// comm->conn, comm->conf_error a comm->name (pre conf_error())?
static void tree_comm_reinit(struct comm_s *comm, struct tree_s *t)
{
	struct tree_s *p;
	struct event_hadler_hash_s *hh;
	struct event_names_s *en;
	struct event_type_s *type;
	int ehh_list;

	event_mask_clear2(t->events[comm->conn].event);

	for (ehh_list = 0; ehh_list < EHH_LISTS; ehh_list++) {
		evhash_foreach(hh, t->subject_handlers[ehh_list]) {
			en = event_type_find_name(hh->handler->op_name, false);
			if (en == NULL) {
				comm->conf_error(comm, "Unknown event name %s\n",
						 hh->handler->op_name);
				continue;
			}
			type = en->events[comm->conn];
			if (type == NULL) {
				comm->conf_error(comm, "Unknown event type %s\n",
						 hh->handler->op_name);
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
			en = event_type_find_name(hh->handler->op_name, false);
			if (en == NULL) {
				comm->conf_error(comm, "Unknown event name %s\n",
						 hh->handler->op_name);
				continue;
			}
			type = en->events[comm->conn];
			if (type == NULL) {
				comm->conf_error(comm, "Unknown event type %s\n",
						 hh->handler->op_name);
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

	for (p = t->child; p != NULL; p = p->next)
		tree_comm_reinit(comm, p);
	for (p = t->regex_child; p != NULL; p = p->next)
		tree_comm_reinit(comm, p);
}

/*
 * space_init_event_mask() activate triggering of events on all relevant nodes
 * in Unified Name Space Tree.
 *
 * @comm: Identification of the communication with a monitored client.
 *	@comm->conn: ID of the communication
 *	@comm->conf_error: private error printing function of the comm interface
 *
 * Global variables used by this function: `global_root' and `global_spaces'.
 *
 * Note: As this function uses event's byte layout definitions, which are set
 * in the initial phase of communication, the function must be called after
 * connection with supervised application was established and the process of
 * event's byte layout definitions was finished.
 */
// TODO: prerob rozhranie funkcie, aby mala namiesto `comm_s' 3 args:
// comm->name (pre conf_error), comm->conf_error a comm->conn (pre tree_comm_reinit)
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
			en = event_type_find_name(le->handler->op_name, false);
			if (en == NULL) {
				comm->conf_error(comm, "Unknown event name %s\n",
						 le->handler->op_name);
				continue;
			}
			type = en->events[comm->conn];
			if (type == NULL) {
				comm->conf_error(comm, "Unknown event type %s\n",
						 le->handler->op_name);
				continue;
			}

			// TODO 1: pridaj tento test aj do  tree_comm_reinit()
			// TODO 2: tento aj predosle testy daj do funkcie, nech sa neopakuje kod
			// TODO 3: za volanie tree_comm_reinit() pridaj volanie novej funkcie,
			//	ktora skontroluje type->handlers_hash zoznamy event handlerov;
			//	tam su tie, ktore maju ako subjekt aj ako objekt space, nie
			//	uzol UNST

			// it's an error, if subject/object of the event should have subject/object but doesn't
			if (((type->op[0] == NULL) != (le->subject == NULL)) ||
					((type->op[1] == NULL) != (le->object == NULL)))
				comm->conf_error(comm, "Invalid use of event %s\n",
						 le->handler->op_name);

			if (le->subject != NULL && le->subject != ALL_OBJ && type->monitored_operand == type->op[0]) {
				arg.conn = comm->conn;
				arg.type = type;
				// activate triggering of the event `type' on all nodes in ->subject space
				//printf("%s ->subject of %s\n", space->name, type->evname->name);
				space_for_every_member(le->subject, (fepf_t)tree_add_event_mask_do, &arg);
			}
			if (le->object != NULL && le->object != ALL_OBJ && type->monitored_operand == type->op[1]) {
				arg.conn = comm->conn;
				arg.type = type;
				// activate triggering of the event `type' on all nodes in ->object space
				//printf("%s ->object of %s\n", space->name, type->evname->name);
				space_for_every_member(le->object, (fepf_t)tree_add_event_mask_do, &arg);
			}
		} // for space->levent
	} // for space in global_spaces

	return 0;
}

/*
 * space_add_vs() place subject with virtual @subj_space into virtual spaces
 * defined by @obj_vs for a given @access_type. Original virtual spaces of
 * @subj_space won't be changed.
 *
 * @subj_space: A set of virtual spaces of each access type of a subject.
 * @access_type: Access types keywords are defined in language/lex.c:
 *	MEMBER, READ or RECEIVE, WRITE or SEND, SEE, CREATE, ERASE, ENTER,
 *	CONTROL.
 * @obj_vs: Virtual spaces of an object to add.
 *
 * Returns 0 on success, -1 on invalid @access_type value.
 *
 * Called from conf_lang.c:conf_lang_param_out() in subject's access type(s)
 * definition(s) case (see 'Paddvs' handling).
 *	<subj> <acces_type> <obj> , [<acces_type>] <obj> ... ;
 * The function is called to add the object's virtual spaces to the subject's
 * relevant permission set (i.e. to the subject's virtual spaces of a given
 * access type) in order to enable the subject to perform the given operation
 * on the object.
 */
int space_add_vs(struct space_s *subj_space, int access_type, vs_t *obj_vs)
{
	if (access_type < 0 || access_type >= NR_ACCESS_TYPES)
		return -1;
	vs_add(obj_vs, subj_space->vs[access_type]);
	return 0;
}

/*
 * space_get_vs() return an identification of the @space in the VS model.
 *
 * @space: A space which identification should be returned.
 *
 * This function provides a real definition of the @space. A space doesn't have
 * an identification (i.e. allocated its identification bit in VS model) until
 * this function is called. For more information about allocation, declaration
 * and definition of a space see space.h:struct space_s.
 *
 * Returns:
 * 1) %NULL: if @space does not represent a valid identificator in the VS model.
 * 2) A valid identificator of the @space in the VS model. That is a bit array,
 *    where only one bit is set. This bit will be identification of the @space
 *    in the VS model.
 *
 * Note: Cause a fatal failure if an error occured (i.e. OOM while allocating a
 * new @space's identificator in the VS model).
 */
vs_t *space_get_vs(struct space_s *space)
{
	if (space == NULL || space == ALL_OBJ)
		return (vs_t *)space;
	if (!vs_isclear(space->vs_id))
		return space->vs_id;

	// TODO 2: skontrolovat pri vytvoreni spojenia, kolko bitov na VS
	// ma jadro. Ak menej nez sa vyuziva v konfigu, treba zahlasit chybu
	// a vhodnym sposobom zareagovat. Pravdepodobne ukoncenim spojenia,
	// lebo nebude mozne vynucovat riadenie zo strany AS.
	//
	// TODO 3: bolo by fajn preskumat, preco segmentation fault pre:
	//	tree "domain" of process;
	//      space d;
	//      space c = "domain" - space d;
	//      space d = space d;
	if (vs_alloc(space->vs_id) < 0)
		fatal("OOM while allocating a new space identificator in the "
		    "VS model");

	space->used = true;
	vs_add(space->vs_id, space->vs[AT_MEMBER]);
	return space->vs_id;
}

/*
 * space_vs_to_str() translate identification bits in @vs to their names as
 * they are used in configuration file(s).
 *
 * @vs: Input set of virtual spaces to be translated.
 * @out: Output buffer to store names of virtual spaces in @vs.
 * @size: Maximum size of the output buffer.
 */
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
