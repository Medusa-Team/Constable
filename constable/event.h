/*
 * Constable: event.h
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: event.h,v 1.4 2002/12/13 16:10:32 marek Exp $
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
//#include "comm.h"

int event_mask_clear( event_mask_t *e );
int event_mask_clear2( event_mask_t *e );
int event_mask_setbit( event_mask_t *e, int b );
int event_mask_clrbit( event_mask_t *e, int b );
int event_mask_copy( event_mask_t *e, event_mask_t *f );
int event_mask_copy2( event_mask_t *e, event_mask_t *f );
int event_mask_and( event_mask_t *e, event_mask_t *f );
int event_mask_or( event_mask_t *e, event_mask_t *f );
int event_mask_or2( event_mask_t *e, event_mask_t *f );
int event_mask_sub( event_mask_t *e, event_mask_t *f );

struct event_handler_s;
struct event_hadler_hash_s;
struct event_names_s;

struct event_type_s {
    struct hash_ent_s	hashent;
    struct event_names_s	*evname;
    struct class_s		*op[2];  /**< subject and object */
    struct class_s		*monitored_operand; /**< points to op[0] or op[1] */
    event_mask_t		mask[2];
    //	struct event_type_s	*alt;
    struct medusa_acctype_s	acctype;
    struct class_s		operation_class;
};

#define	EHH_VS_ALLOW		0
#define	EHH_VS_DENY		1
#define	EHH_NOTIFY_ALLOW	2
#define	EHH_NOTIFY_DENY		3
#define	EHH_LISTS		4
struct event_names_s {
    struct event_names_s	*next;
    char			*name;
    struct event_type_s	**events;
    struct event_hadler_hash_s	*handlers_hash[EHH_LISTS];
};

int event_free_all_events( struct comm_s *comm );
struct event_type_s *event_type_add( struct comm_s *comm, struct medusa_acctype_s *m, struct medusa_attribute_s *a );
struct event_names_s *event_type_find_name( char *name );

struct event_context_s;
struct tree_s;

#ifdef DEBUG_TRACE
#define	DT_POS_MAX	64
#else
#define	DT_POS_MAX	0
#endif

struct event_handler_s {
#ifndef DEBUG_TRACE
    char	op_name[MEDUSA_OPNAME_MAX];
#else
    char	op_name[MEDUSA_OPNAME_MAX+DT_POS_MAX];
#endif

    /* if handler returns
        0 => completed  - result is valid
        1 => to be continue immediately (move to end of queue)
        >1 => to be continue later
        <0 => error
    */
    int(*handler)(struct comm_buffer_s *,struct event_handler_s *,struct event_context_s *);
    struct object_s		*local_vars;
    char			data[0];
};
#define	ALL_OBJ	((void*)1)

struct event_hadler_hash_s {
    struct event_hadler_hash_s	*next;
    struct event_handler_s		*handler;
    struct event_names_s		*evname;
    vs_t				subject_vs[NUMBER_OF_VS];
    vs_t				object_vs[NUMBER_OF_VS];
};

struct event_hadler_hash_s *evhash_add( struct event_hadler_hash_s **hash, struct event_handler_s *handler, struct event_names_s *evname );
#define	evhash_foreach(ev,hash)	for((ev)=(hash);(ev)!=NULL;(ev)=(ev)->next)
#define	evhash_foreach_first(ev,hash)	((ev)=(hash))
#define	evhash_foreach_next(ev)		((ev)=(ev)->next)

int register_event_handler( struct event_handler_s *h, struct event_names_s *evname, struct event_hadler_hash_s **hash, vs_t *subject_vs, vs_t *object_vs );

struct event_context_s {
    struct object_s operation;
    struct object_s subject;
    struct object_s object;
    int		result;
    struct comm_buffer_s *cb;	/* ??? asi zbytocne ??? */
    int		unhandled;      /**< 1 if event wasn't handled */
    struct object_s *local_vars;
};

#define	RESULT_UNKNOWN	-1
#define	RESULT_ALLOW	0	/* YES */
#define	RESULT_DENY	1	/* NO */
#define	RESULT_SKIP	2	/* SKIP */
#define	RESULT_OK	3	/* OK */
#define	RESULT_RETRY	4	/* RETRY */

/*
RESULT_UNKNOWN -> *
RESULT_ALLOW -> RESULT_DENY | RESULT_SKIP
RESULT_DENY -> -
RESULT_SKIP -> RESULT_DENY
RESULT_OK -> RESULT_ALLOW | RESULT_DENY | RESULT_SKIP
*/
int evaluate_result( int old, int new );
int do_event( struct comm_buffer_s *cb );

int get_empty_context( struct event_context_s *c );

extern struct event_handler_s *function_debug;

void event_context_print( struct event_context_s *c, void(*out)(int arg, char *), int arg ); 

#endif /* _EVENT_H */

