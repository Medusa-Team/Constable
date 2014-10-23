/**
 * @file execute_stack.c
 * @short Functions for stack handling for runtime interpreter
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: execute_stack.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include "../constable.h"
#include "execute.h"
#include <stdlib.h>

static struct stack_s *free_stack=NULL;

#define STACKLEN	4096
struct stack_s *execute_get_stack( void )
{ struct stack_s *p;
	if( (p=free_stack)!=NULL )
	{	free_stack=p->next;
		p->prev=p->next=NULL;
		p->my_offset=0;
		return(p);
	}
	if( (p=malloc(sizeof(struct stack_s)+STACKLEN*sizeof(uintptr_t)))==NULL )
		return(NULL);
	p->prev=p->next=NULL;
	p->size=STACKLEN;
	p->my_offset=0;
	return(p);
}

void execute_put_stack( struct stack_s *stack )
{
	stack->next=free_stack;
	free_stack=stack;
}

struct execute_s *execute_alloc_execute( void )
{ struct execute_s *e;
	if( (e=malloc(sizeof(struct execute_s)))==NULL )
		return(NULL);
	e->stack=execute_get_stack();
	e->pos=0;
	e->base=0;
	e->h=NULL;
	e->c=NULL;
	e->keep_stack=0;
	return(e);
}

void execute_push( struct execute_s *e, uintptr_t data )
{
	while( e->pos >= e->stack->my_offset + e->stack->size )
	{	if( e->stack->next==NULL
		    && (e->stack->next=execute_get_stack())==NULL )
		{	runtime("push: %s",Out_of_memory);
			return;
		}
		e->stack->next->my_offset=e->stack->my_offset+e->stack->size;
		e->stack->next->prev=e->stack;
		e->stack=e->stack->next;
	}
	e->stack->stack[(e->pos)++ - e->stack->my_offset]=data;
}

uintptr_t execute_pop( struct execute_s *e )
{
	if( e->pos<=0 )
	{	runtime("pop: Stack underrun");
		return(0);
	}
	while( e->pos <= e->stack->my_offset )
	{	if( e->stack->next==NULL && !(e->keep_stack) )
		{	e->stack=e->stack->prev;
			execute_put_stack(e->stack->next);
			e->stack->next=NULL;
		}
		else	e->stack=e->stack->prev;
	}
	return( e->stack->stack[ --(e->pos) - e->stack->my_offset ] );
}

uintptr_t execute_readstack( struct execute_s *e, int pos )
{ struct stack_s *s;
	if( pos<0 )
	{	runtime("readstack: Stack underrun");
		return(0);
	}
	s=e->stack;
	while( pos <= s->my_offset )
		s=s->prev;
	while( s!=NULL && pos >= s->my_offset + s->size )
		s=s->next;
	if( s==NULL )
	{	runtime("readstack: Stack overrun");
		return(0);
	}
	return( s->stack[ pos - s->my_offset ] );
}

uintptr_t *execute_stack_pointer( struct execute_s *e, int pos )
{ struct stack_s *s;
	if( pos<0 )
	{	runtime("readstack: Stack underrun");
		return(0);
	}
	s=e->stack;
	while( pos <= s->my_offset )
		s=s->prev;
	while( s!=NULL && pos >= s->my_offset + s->size )
		s=s->next;
	if( s==NULL )
	{	runtime("readstack: Stack overrun");
		return(0);
	}
	return( &(s->stack[ pos - s->my_offset ]) );
}

int execute_init_stacks( int n )
{ struct stack_s *p=NULL,*q;
	while( n>0 && (q=execute_get_stack())!=NULL )
	{	q->next=p;
		p=q;
		n--;
	}
	while( p!=NULL )
	{	p=(q=p)->next;
		execute_put_stack(q);
	}
	return(0);
}
