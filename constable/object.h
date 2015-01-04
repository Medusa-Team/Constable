/*
 * Constable: object.h
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: object.h,v 1.3 2002/10/23 10:25:43 marek Exp $
 */

#ifndef _OBJECT_H
#define _OBJECT_H

#include "types.h"
#include "medusa_object.h"
#include "vs.h"
#include "hash.h"
#include "access_types.h"
#include <byteswap.h>

struct comm_s;
struct comm_buffer_s;

struct object_s;

struct class_s;
struct class_names_s;

struct class_handler_s {
    struct class_handler_s *next;	/* for class */
    struct class_names_s	*classname;
    int		*cinfo_offset;
    int		comm_buf_temp_offset;
    int		flags;
    struct tree_s	*root;
    void		*user;	/* struct event_names_s * for generic */
    int(*init_comm)(struct class_handler_s *,struct comm_s *);
    int(*set_handler)(struct class_handler_s *,struct comm_s *,struct object_s *);
    int(*get_vs)(struct class_handler_s *,struct comm_s *,struct object_s *,vs_t *,int);
    struct tree_s *(*get_tree_node)(struct class_handler_s *,struct comm_s *,struct object_s *);
    struct space_s *(*get_primary_space)(struct class_handler_s *,struct comm_s *,struct object_s *);
    int(*enter_tree_node)(struct class_handler_s *,struct comm_s *,struct object_s *,struct tree_s *);
};


struct class_s {
    struct hash_ent_s		hashent;
    u_int16_t			cinfo_offset;
    u_int16_t			cinfo_size;	/* v 32 bit slovach */
    uintptr_t			cinfo_mask;
    struct class_names_s		*classname;
    struct comm_s			*comm;
    u_int16_t			name_offset;
    u_int16_t			name_size;	/* v bajtoch */
    u_int16_t			event_offset;
    u_int16_t			event_size;	/* v bajtoch */
    struct medusa_attribute_s	*vs_attr[NR_ACCESS_TYPES];
    struct {
        u_int16_t	event_offset;
        u_int16_t	event_size;	/* v bajtoch */
        u_int16_t	cinfo_offset;
        u_int16_t	cinfo_size;	/* v 32 bit slovach */
        uintptr_t	cinfo_mask;
    }				subject;
    struct medusa_class_s		m;
    struct medusa_attribute_s	attr[0];
};

struct class_names_s {
    struct class_names_s	*next;
    char			*name;
    struct class_s		**classes;
    struct class_handler_s	*class_handler;
};

struct object_s {
    struct object_s *next;
    struct medusa_attribute_s attr;
    int	flags;
    struct class_s	*class;
    char	*data;
};

#define OBJECT_FLAG_LOCAL	0x01
#define OBJECT_FLAG_CHENDIAN	0x10	/* big<->little */
struct class_names_s *get_class_by_name( char *name );
struct medusa_attribute_s *get_attribute( struct class_s *c, char *name );
int class_free_all_clases( struct comm_s *comm );
struct class_s *add_class( struct comm_s *comm, struct medusa_class_s *mc, struct medusa_attribute_s *a );

/* vrati offset (u_int16_t), alebo -1 ak chyba */
int class_alloc_object_cinfo( struct class_s *c );
int class_alloc_subject_cinfo( struct class_s *c );

int class_add_handler( struct class_names_s *c, struct class_handler_s *handler );

int class_comm_init( struct comm_s *comm );

#define	PCINFO(object,ch,comm)	((int*)((object)->data+(ch)->cinfo_offset[(comm)->conn])) // corrected by Matus - should be int
#define	CINFO(object,ch,comm)	(*(PCINFO(object,ch,comm)))

int object_get_val( struct object_s *o, struct medusa_attribute_s *a, void *buf, int maxlen );
int object_set_val( struct object_s *o, struct medusa_attribute_s *a, void *buf, int maxlen );
int object_resize_data( void *buf, struct medusa_attribute_s *a, int newlen );
int object_set_byte_order( struct object_s *o, int flags );
int object_copy( struct object_s *d, struct object_s *s );
void byte_reorder_attrs( int flags, struct medusa_attribute_s *a );
void byte_reorder_class( int flags, struct medusa_class_s *c );
void byte_reorder_acctype( int flags, struct medusa_acctype_s *a );

#define byte_reorder_get_uintptr_t(flag,val) ((uint32_t)val) //#	TODO distinguish 32 versus 64 bit swap!
#define byte_reorder_put_uintptr_t(flag,val) (val) //#	TODO distinguish 32 versus 64 bit swap!
#define byte_reorder_get_int64(flag,val) (val) //#	((flag)&OBJECT_FLAG_CHENDIAN?bswap_64(val):(val))
#define byte_reorder_put_int64(flag,val) (val) //#	((flag)&OBJECT_FLAG_CHENDIAN?bswap_64(val):(val))
#define byte_reorder_get_int32(flag,val) (val) //#	((flag)&OBJECT_FLAG_CHENDIAN?bswap_32(val):(val))
#define byte_reorder_put_int32(flag,val) (val) //#	((flag)&OBJECT_FLAG_CHENDIAN?bswap_32(val):(val))
#define byte_reorder_get_int16(flag,val) (val) //#	((flag)&OBJECT_FLAG_CHENDIAN?bswap_16(val):(val))
#define byte_reorder_put_int16(flag,val) (val) //#	((flag)&OBJECT_FLAG_CHENDIAN?bswap_16(val):(val))

int object_is_same( struct object_s *a, struct object_s *b );

int object_clear_event( struct object_s *o );
int object_add_event( struct object_s *o, event_mask_t *e );

int object_cmp_vs( vs_t *vs, int n, struct object_s *o );
int object_get_vs( vs_t *vs, int n, struct object_s *o );
int object_clear_all_vs( struct object_s *o );
int object_add_vs( struct object_s *o, int n, vs_t *vs );

int object_is_invalid( struct object_s *o );
int object_do_sethandler( struct object_s *o );

void attr_print( struct medusa_attribute_s *a, void(*out)(int arg, char *), int arg ); 
void class_print( struct class_s *c, void(*out)(int arg, char *), int arg );
void object_print( struct object_s *o, void(*out)(int arg, char *), int arg );

#endif /* _OBJECT_H */

