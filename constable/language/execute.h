/**
 * @file execute.h
 * @short Header file for runtime interpreter
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: execute.h,v 1.3 2004/04/24 15:33:36 marek Exp $
 */

#ifndef _EXECUTE_H
#define _EXECUTE_H

#include "../event.h"
#include "../object.h"

#define MAX_REG_SIZE	4096

struct register_s {
	struct medusa_attribute_s *attr;
	struct medusa_attribute_s tmp_attr;
	struct object_s	*object;	/* ak je to object */
	struct class_s *class;
	int flags;
	char *data;
	char buf[MAX_REG_SIZE];
};

#define	reg_data(r)	((r).data==(r).buf?(r).data:(r).data+(r).attr->offset)

struct stack_s {
	struct stack_s	*prev;
	struct stack_s	*next;
	int	size;
	int	my_offset;
	u_int32_t stack[0];
};

struct execute_s {
	struct stack_s	*stack;
	int	pos;
	int	base;
	int	start;
	u_int32_t *p;
	int	fn_getsval_p;
	int	cont;
	struct event_handler_s *h;
	struct event_context_s *c;
	int	keep_stack;		/* 1 - pop neuvolnuje stack */
	struct comm_s	*comm;
	struct comm_buffer_s	*my_comm_buff;
};

void obj_to_reg( struct register_s *r, struct object_s *o, char *attr );

typedef int(*buildin_t)( struct execute_s *e, struct register_s *ret, int(*getarg)(struct execute_s *,struct register_s*) );
#define	BUILDIN_FUNC(name)	int name( struct execute_s *e, struct register_s *ret, int(*getarg)(struct execute_s *,struct register_s*) )

extern struct medusa_attribute_s execute_attr_int;
extern struct medusa_attribute_s execute_attr_str;
extern struct medusa_attribute_s execute_attr_pointer;

struct object_s *r2o( struct register_s *r );
struct object_s *r2o2( struct register_s *r );

int r_int( struct register_s *r );
void r_imm( struct register_s *r );
void r_sto( struct register_s *v, struct register_s *d );
void r_resize( struct register_s *v, int size );

//mY int strnlen( char *s, int n );

void r_neg( struct register_s *v );
void r_not( struct register_s *v );
int r_nz( struct register_s *v );

void do_bin_op( int op, struct register_s *v, struct register_s *d );

int load_constant( struct register_s *r , u_int32_t typ, char * name );

int language_init_comm_datatypes( struct comm_s *comm );

int execute_handler( struct comm_buffer_s *comm_buff, struct event_handler_s *h, struct event_context_s *c );

struct stack_s *execute_get_stack( void );
void execute_put_stack( struct stack_s *stack );
struct execute_s *execute_alloc_execute( void );
void execute_push( struct execute_s *e, u_int32_t data );
u_int32_t execute_pop( struct execute_s *e );
u_int32_t execute_readstack( struct execute_s *e, int pos );
u_int32_t *execute_stack_pointer( struct execute_s *e, int pos );
int execute_init_stacks( int n );

#endif /* _EXECUTE_H */

