/*
 * Constable: space.h
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: space.h,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#ifndef _TARGET_H
#define _TARGET_H

#include "tree.h"
#include "event.h"
#include "access_types.h"

extern struct space_s *global_spaces;

#define ANON_SPACE_NAME "\x1A" // name of an internal anonymous space

typedef struct ltree_s {
    struct ltree_s	*prev;
    void		*path_or_space;
    int		type;
} ltree_t;
#define	LTREE_EXCLUDE	0x01
#define	LTREE_T_MASK	0x02
#define	LTREE_T_TREE	0x00
#define	LTREE_T_NTREE	(LTREE_T_TREE|LTREE_T_EXCLUDE)
#define	LTREE_T_SPACE	0x02
#define	LTREE_T_NSPACE	(LTREE_T_SPACE|LTREE_T_EXCLUDE)
#define	LTREE_RECURSIVE	0x80

typedef struct levent_s {
    struct levent_s		*prev;
    struct event_handler_s	*handler;
    struct space_s		*subject;
    struct space_s		*object;
} levent_t;

struct space_s {
    struct space_s	*next;
    struct vs_s	*my_vs;
    vs_t		vs[NR_ACCESS_TYPES][MAX_VS_BITS/32];
    levent_t	*levent;
    ltree_t		*ltree;
    int		primary;
    char		name[0];
};

struct space_s *space_create( char *name, int primary );
struct space_s *space_find( char *name );
int space_add_path( struct space_s *space, int type, void *path_or_space );
int space_add_event( struct event_handler_s *handler, int level, struct space_s *subject, struct space_s *object, struct tree_s *subj_node, struct tree_s *obj_node );

/* space_init_event_mask must be called after connection was estabilished */
int space_init_event_mask( struct comm_s *comm );

int space_add_vs( struct space_s *space, int which, vs_t *vs );
vs_t *space_get_vs( struct space_s *space );

/* space_apply_all must be called after all spaces are defined */
int space_apply_all( void );

int space_vs_to_str( vs_t *vs, char *out, int size );

#endif /* _TARGET_H */

