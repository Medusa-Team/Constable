/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Constable: space.h
 * (c) 2002 by Marek Zelem <marek@terminus.sk>
 * (c) 2024 by Matus Jokay <matus.jokay@gmail.com>
 */

#ifndef _TARGET_H
#define _TARGET_H

#include "tree.h"
#include "event.h"
#include "access_types.h"

extern struct space_s *global_spaces;

// TODO: nahrad ascii symbol "substitute" za tlacitelny znak
//#define ANON_SPACE_NAME "?" // name of an internal anonymous space
#define ANON_SPACE_NAME "\x1A" // name of an internal anonymous space

/*
 * ltree_s: List (->prev) of elements (nodes of Unified Name Space Tree or
 * spaces; ->path_or_space of given ->type) defining the members of a space.
 *
 * @prev: Next element in a list or %NULL if last.
 * @path_or_space: Data pointer of the list element. Can be a node of UNST or
 *	an another space.
 * @type: Type of the list element: %LTREE_T_TREE for a node of UNST or
 *	%LTREE_T_SPACE for an another space.
 *
 * The head of the list is stored in struct space_s->ltree. See documentation
 * of struct space_s below.
 */
struct ltree_s {
	struct ltree_s	*prev;
	void		*path_or_space;
	int		type;
};

/* %LTREE_EXCLUDE: if set, a space element should be excluded from the space */
#define	LTREE_EXCLUDE	0x01
/*
 * %LTREE_T_MASK: the bitmask for type of a space element. The masked bit
 * represents a type of the space element. If set, space element is an another
 * space (see %LTREE_T_SPACE), a node of UNST otherwise (see %LTREE_T_TREE).
 */
#define	LTREE_T_MASK	0x02
/* %LTREE_T_TREE: space element is a node of UNST, if not set %LTREE_T_SPACE. */
#define	LTREE_T_TREE	0x00
/* %LTREE_T_NTREE: space element is a excluded node of the UNST. */
#define	LTREE_T_NTREE	(LTREE_T_TREE|LTREE_T_EXCLUDE)
/* %LTREE_T_SPACE: space element is a space. */
#define	LTREE_T_SPACE	0x02
/* %LTREE_T_NSPACE: space element is an excluded space. */
#define	LTREE_T_NSPACE	(LTREE_T_SPACE|LTREE_T_EXCLUDE)
/*
 * %LTREE_RECURSIVE: space element represents not only itself, but also its
 * descendants (if it's a node of UNST) or the contents of another space
 * recursively (if it's a space).
 */
#define	LTREE_RECURSIVE	0x80

/*
 * levent_s: List (->prev) of events (->handler), where ->subject and/or
 * ->object is/are a space(s).
 *
 *  @prev: next element in a list or %NULL
 *  @handler: event related to @subject and/or @object
 *  @subject: subject of the event
 *  @object: object of the event
 *
 * Elements of the list are used to keep track of all events relevant to a
 * space. IOW, each `struct space_s' contains a head of the list and if
 * it's not %NULL, the space is used as subject/object in some event(s).
 * If the list is empty, the space is defined but really not used.
 *
 * But the main purpose of the list is to store the information necessary to
 * activate triggering of events in the list on all members of the space.
 * Activation of triggering cannot be done at event definition time:
 * 1) Triggering is set on subject or object of the event based on the flag
 *    in the event byte representation.
 * 2) The event byte representation is not available at event definition time;
 *    it is obtained by Medusa communication protocol at the initial phase
 *    of communication between Constable and a monitored application.
 * So the activation of triggering must be called after connection was
 * established and the process of event's (byte representation) definition was
 * finished.
 *
 * An instance of this struct is allocated by space_add_event()->new_levent().
 * Space_add_event() is called from configuration file in case of event/access
 * <op> definition:
 *	<subject> <op> [:ehh_list] [<object>] { <cmd> ... }
 *
 * Function new_levent() is called for <subject> and/or <object>, if it's
 * a space. Both <subject> and <object> can be of two types:
 *	1) space identificator (or '*' (star) representing all spaces) or
 *	2) node in Unified Name Space Tree.
 * Moreover, <object> can be %NULL (it is optional from the definition).
 * So new_levent() is called for <subject> and/or <object>, if at least
 * one of them is a valid space identificator (i.e. not a node, not a
 * special value %ALL_OBJ representing all spaces, not %NULL).
 */
struct levent_s {
	struct levent_s		*prev;
	struct event_handler_s	*handler;
	struct space_s		*subject;
	struct space_s		*object;
};

/*
 * struct members_s stores information about real members (nodes of UNST) of
 * a space.
 *
 * @count: Stores count of the members.
 * @array: Consists of @count elements. Each element is pointer to one real
 *	member (i.e. address of a node of UNST) of the space.
 *
 * The struct is filled in space_apply_all() function, which is calle after all
 * spaces are defined.
 */
struct members_s {
	int count;
	struct tree_s **array;
};

/*
 * struct space_s reprezents a virtual space.
 * Struct with a variable length (see ->name).
 *
 * @next: Next element in the `global_spaces' list or %NULL if last.
 * @vs: Array of virtual spaces bit fields, for each access type one. Bits,
 *	that are set in an access bit field, represent the virtual spaces, over
 *	which this space is authorised to perform corresponding access. For
 *	more information about access types see access_types.h file.
 * @vs_id: Identification bit field of the space. Each space should have unique
 *	bit set in the VS model. See vs_alloc() function. If the space does not
 *	have an ID bit set, it means that the space was not used as a subject
 *	and/or object of any event (or operation) definition.
 * @levent: Linked list of events relevant to the space. If the list is empty,
 *      the space is defined but never used as a subject/object of any event.
 * @ltree: List of members of the space (nodes of UNST or another spaces). If
 *	the list is empty, the space has no member(s).
 * @primary: Set if the space is primary. This property has meaning only for
 *	the subject of an event/operation. The subject, as well as the object,
 *	can be a member of several spaces. When the subject performs an event
 *	(access, operation) on the object, it must be clear, whichs set of
 *	access rights of the subject has to be taken into account in the given
 *	operation. One set of permissions is always defined over one space.
 *	So if the subject belongs to several spaces, there must be a rule that
 *	determines which set of permissions will be applied. And the rule is:
 *	whenever the given entity acts as a subject, apply the permissions of
 *	the primary space.
 * @initialized: Set if the space is successfully validated (i.e. it has some
 *	member(s) and it's used).
 * @used: Set if the space is visited while traversing members of another
 *	space. IOW, the space is a member of another space, which members are
 *	examinated in space_for_every_path_i() function. This flag indicates,
 *	that the space is used. Spaces, which have not set this flag, are not
 *	used at all (nor as member(s) of another space(s), nor as subject(s)/
 *	object(s) of any event(s), nor as variables in some function(s)) and
 *	will be destroyed and removed from the memory (see space_apply_all()).
 * @members: Stores information about real members of the space and their
 *	count. For more information see definition of struct members_s.
 * @name: Name of the space.
 *
 * Allocation, declaration, definition, initialization and membership of a
 * space:
 * 1) An instance of `struct space_s' is created by space_create() function.
 *    It creates an empty instance of space. IOW, makes a declaration of the
 *    space without any member(s) and without an identification, i.e. ->vs_id
 *    will be empty.
 * 2) A real definition of the space is made by space_get_vs() function, which
 *    assigns an identification bit to the space; ->vs_id will be used and
 *    this identification is copied to ->vs[AT_MEMBER] bit field. At this
 *    moment the space is declared, defined, but still can be without any
 *    member(s).
 * 3) The initialization of the space is located in space_apply_all() function.
 *    It sets the ->initialized flag for each used space. There can be some
 *    unused space(s) as members of another space S, which is used. This unused
 *    spaces get their mark ->used set during the processing of the space S (see
 *    space_path_add() function). When an until now unused space becomes a part
 *    of the member's evaluation trace (of the space S), will be marked as
 *    used.
 * 4) language/conf_lang.c:'case Paddpath' calls space_add_path() -> new_path()
 *    and it adds a new member to the space (via ->ltree). The attribute
 *    ->ltree stores mixed information about space members: a member can be
 *    of struct tree_s or struct space_s type. Moreover, a member can be stored
 *    multiple times in the list and there may be members, which are marked as
 *    excluded from the space. So real (final) list of members is stored in
 *    ->members after evaluation of ->ltree in space_apply_all(). The final
 *    list ->members.array stores deduplicated pointers to UNST nodes (real
 *    members of the space) and their count in ->members.count.
 */
struct space_s {
	struct space_s	*next;
	vs_t		vs[NR_ACCESS_TYPES][VS_WORDS];
	vs_t		vs_id[VS_WORDS];
	struct levent_s	*levent;
	struct ltree_s	*ltree;
	int		primary; // TODO: pretypuj na bool
	bool		initialized;
	bool		used;
	struct members_s members;
	char		name[0];
};

struct space_s *space_create(char *name, int primary);
struct space_s *space_find(char *name);
int space_add_path(struct space_s *space, int type, void *path_or_space);
int space_add_event(struct event_handler_s *handler, int level,
		    struct space_s *subject, struct space_s *object,
		    struct tree_s *subj_node, struct tree_s *obj_node);

/* space_init_event_mask must be called after connection was estabilished */
int space_init_event_mask(struct comm_s *comm);

int space_add_vs(struct space_s *space, int which, vs_t *vs);
vs_t *space_get_vs(struct space_s *space);

/* space_apply_all must be called after all spaces are defined */
int space_apply_all(void);

int space_vs_to_str(vs_t *vs, char *out, int size);

#endif /* _TARGET_H */

