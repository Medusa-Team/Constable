/**
 * @file adm.c
 * @short RBAC administration interface (constable side)
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: adm.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include "rbac.h"
#include "../space.h"
#include "../comm.h"
#include "../language/error.h"
#include "../language/language.h"
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

/*
	- vytvorit rolu			create <role>
	- hirerchicky usporiadat role	hierarchy <suprole> <subrole>
	- priradit uzivatela		user <role> <user>
	- pridat pravo			permission <role> <access> <space>
 */

int load_users( char *file );

static int get_event_context( struct comm_s *comm, struct event_context_s *c, struct event_type_s *t, void *op, struct object_s *subject, void *object )
{
	c->operation.next=&(c->subject);
	c->operation.attr.offset=0;
	c->operation.attr.length=t->m.size;
	c->operation.attr.type=MED_TYPE_END;
	strncpy(c->operation.attr.name,t->m.name,MIN(MEDUSA_ATTRNAME_MAX,MEDUSA_OPNAME_MAX));
	c->operation.flags=comm->flags;
	c->operation.class=&(t->operation_class);
	c->operation.data=(char*)(op);

	c->subject.next=&(c->object);
	c->subject.attr.offset=c->subject.attr.length=0;
	c->subject.attr.type=MED_TYPE_END;
	strncpy(c->subject.attr.name,t->m.op_name[0],MEDUSA_ATTRNAME_MAX);
	c->subject.flags=subject->flags;
	c->subject.class=subject->class;
	c->subject.data=subject->data;
	c->object.next=NULL;
	c->object.attr.offset=c->object.attr.length=0;
	c->object.attr.type=MED_TYPE_END;
	strncpy(c->object.attr.name,t->m.op_name[1],MEDUSA_ATTRNAME_MAX);
	c->object.flags=comm->flags;
	c->object.class=t->op[1];
	c->object.data=(char*)(object);
	if( c->subject.class!=NULL )
		c->subject.attr.length=c->subject.class->m.size;
	if( c->object.class!=NULL )
		c->object.attr.length=c->object.class->m.size;
	c->local_vars=NULL;
	return(0);
}

int rbac_comm_answer( struct comm_s *c, struct comm_buffer_s *b, int result )
{
	((struct comm_buffer_s*)(b->user1))->user_data=result;
	switch( result )
	{
		case RESULT_ALLOW:
		case RESULT_OK:
			return( b->completed(b) );
	}
	return(0);
}

int rbac_adm_create_role_do( struct comm_buffer_s *b );

int rbac_adm_create_role( struct comm_buffer_s *to_wait, char *name )
{ struct comm_buffer_s *p;

	if( (p=comm_buf_get(0,rbac_comm))==NULL )
	{	comm_error("Can't get comm buffer for rbac");
		return(-1);
	}
	to_wait->do_phase=2;
	comm_buf_to_queue(&(p->to_wake),to_wait);
	p->event=(event_type_find_name("create_role"))->events[rbac_comm->conn];
	p->completed=rbac_adm_create_role_do;
	p->user1=(void*)to_wait;
	p->user2=(void*)name;
	get_event_context(rbac_comm,&(p->context),p->event,p->buf,&(to_wait->context.subject),p->buf);
	comm_buf_todo(p);
	return(2);
}

int rbac_adm_create_role_do( struct comm_buffer_s *b )
{
  struct role_s *r;

	if( (r=rbac_role_add((char*)(b->user2)))==NULL )
		return(-1);
	return(0);
}

int rbac_adm_delete_role_do( struct comm_buffer_s *b );

int rbac_adm_delete_role( struct comm_buffer_s *to_wait, char *name )
{ struct comm_buffer_s *p;
  struct role_s *r;
	if( (r=rbac_role_find(name))==NULL )
	{	errstr="Undefined role";
		return(-1);
	}
	if( (p=comm_buf_get(0,rbac_comm))==NULL )
	{	comm_error("Can't get comm buffer for rbac");
		return(-1);
	}
	to_wait->do_phase=2;
	comm_buf_to_queue(&(p->to_wake),to_wait);
	p->event=(event_type_find_name("delete_role"))->events[rbac_comm->conn];
	p->completed=rbac_adm_delete_role_do;
	p->user1=(void*)to_wait;
	p->user2=(void*)r;	/* FIXME: usecount */
	get_event_context(rbac_comm,&(p->context),p->event,p->buf,&(to_wait->context.subject),r);
	comm_buf_todo(p);
	return(2);
}

int rbac_adm_delete_role_do( struct comm_buffer_s *b )
{
	return( rbac_role_del((struct role_s *)(b->user2)) );
}

int rbac_adm_hierarchy_do1( struct comm_buffer_s *b );
int rbac_adm_hierarchy_do2( struct comm_buffer_s *b );

int rbac_adm_hierarchy( int add, struct comm_buffer_s *to_wait, char *sup_name, char *sub_name )
{ struct comm_buffer_s *p;
  struct role_s *rp,*rb;
  	if( (rp=rbac_role_find(sup_name))==NULL
	    || (rb=rbac_role_find(sub_name))==NULL )
	{	errstr="Undefined role";
		return(-1);
	}
	if( (p=comm_buf_get(0,rbac_comm))==NULL )
	{	comm_error("Can't get comm buffer for rbac");
		return(-1);
	}
	to_wait->do_phase=2;
	comm_buf_to_queue(&(p->to_wake),to_wait);
	p->event=(event_type_find_name("role_hierarchy"))->events[rbac_comm->conn];
	p->completed=rbac_adm_hierarchy_do1;
	p->user1=(void*)to_wait;
	p->user2=(void*)rb;	/* FIXME: usecount */
	p->user_data=add;
	get_event_context(rbac_comm,&(p->context),p->event,p->buf,&(to_wait->context.subject),rp);
	comm_buf_todo(p);
	return(2);
}

int rbac_adm_hierarchy_do1( struct comm_buffer_s *b )
{ void *rp;
	rp=b->context.object.data;
	b->event=(event_type_find_name("role_hierarchy"))->events[rbac_comm->conn];
	get_event_context(rbac_comm,&(b->context),b->event,b->buf,&(((struct comm_buffer_s*)(b->user1))->context.subject),(b->user2));
	b->completed=rbac_adm_hierarchy_do2;
	b->user2=(void*)rp;
	b->do_phase=0;
	return(0);
}

int rbac_adm_hierarchy_do2( struct comm_buffer_s *b )
{
  struct role_s *rp,*rb;
	rp=(struct role_s *)(b->user2);
	rb=(struct role_s *)(b->context.object.data);
	if( b->user_data )
	{	if( rbac_set_hierarchy(rp,rb)<0 )
			return(-1);
	}
	else
	{	if( rbac_del_hierarchy(rp,rb)<0 )
			return(-1);
	}
	return(0);
}

int rbac_adm_user_do1( struct comm_buffer_s *b );
int rbac_adm_user_do2( struct comm_buffer_s *b );

int rbac_adm_user( int add, struct comm_buffer_s *to_wait, char *role, char *user )
{ struct comm_buffer_s *p;
  struct role_s *r;
  struct user_s *u;
  	if( (r=rbac_role_find(role))==NULL )
	{	errstr="Undefined role";
		return(-1);
	}
	if( (u=rbac_user_find(user))==NULL )
	{	errstr="Undefined user";
		return(-1);
	}
	if( (p=comm_buf_get(0,rbac_comm))==NULL )
	{	comm_error("Can't get comm buffer for rbac");
		return(-1);
	}
	to_wait->do_phase=2;
	comm_buf_to_queue(&(p->to_wake),to_wait);
	p->event=(event_type_find_name("uses_assign"))->events[rbac_comm->conn];
	p->completed=rbac_adm_user_do1;
	p->user1=(void*)to_wait;
	p->user2=(void*)r;	/* FIXME: usecount */
	p->user_data=add;
	get_event_context(rbac_comm,&(p->context),p->event,p->buf,&(to_wait->context.subject),u);
	comm_buf_todo(p);
	return(2);
}

int rbac_adm_user_do1( struct comm_buffer_s *b )
{ void *u;
	u=b->context.object.data;
	b->event=(event_type_find_name("role_assign"))->events[rbac_comm->conn];
	get_event_context(rbac_comm,&(b->context),b->event,b->buf,&(((struct comm_buffer_s*)(b->user1))->context.subject),(b->user2));
	b->completed=rbac_adm_user_do2;
	b->user2=(void*)u;
	b->do_phase=0;
	return(0);
}

int rbac_adm_user_do2( struct comm_buffer_s *b )
{
  struct role_s *r;
  struct user_s *u;
	r=(struct role_s *)(b->context.object.data);
	u=(struct user_s *)(b->user2);
	if( b->user_data )
	{	if( rbac_add_ua(u,r)<0 )
			return(-1);
	}
	else
	{	if( rbac_del_ua(u,r)<0 )
			return(-1);
	}
	return(0);
}

static int str2at( char *str )
{ lextab_t *l=keywords;
	while(l->keyword!=NULL)
	{	if( l->sym==Taccess && !strcmp(str,l->keyword) )
		return(l->data);
		l++;
	}
	return(-1);
}

int rbac_adm_perm_do1( struct comm_buffer_s *b );
int rbac_adm_perm_do2( struct comm_buffer_s *b );

int rbac_adm_perm( int add, struct comm_buffer_s *to_wait, char *role, char *access, char *space )
{ struct comm_buffer_s *p;
  struct role_s *r;
  struct perm_s *perm;
  	if( (r=rbac_role_find(role))==NULL )
	{	errstr="Undefined role";
		return(-1);
	}
	if( (p=comm_buf_get(sizeof(struct perm_s),rbac_comm))==NULL )
	{	comm_error("Can't get comm buffer for rbac");
		return(-1);
	}
	to_wait->do_phase=2;
	comm_buf_to_queue(&(p->to_wake),to_wait);
	perm=(struct perm_s *)p->buf;
	memset(perm,0,sizeof(struct perm_s));
//	perm->object.
	perm->access=str2at(access);
	strncpy(perm->space,space,sizeof(perm->space)-1);

	p->event=(event_type_find_name("permission_assign"))->events[rbac_comm->conn];
	p->completed=rbac_adm_perm_do1;
	p->user1=(void*)to_wait;
	p->user2=(void*)r;	/* FIXME: usecount */
	p->user_data=add;
	get_event_context(rbac_comm,&(p->context),p->event,p->buf,&(to_wait->context.subject),perm);
	comm_buf_todo(p);
	return(2);
}

int rbac_adm_perm_do1( struct comm_buffer_s *b )
{ void *perm;
	perm=b->context.object.data;
	b->event=(event_type_find_name("role_assign"))->events[rbac_comm->conn];
	get_event_context(rbac_comm,&(b->context),b->event,b->buf,&(((struct comm_buffer_s*)(b->user1))->context.subject),(b->user2));
	b->completed=rbac_adm_perm_do2;
	b->user2=(void*)perm;
	b->do_phase=0;
	return(0);
}

int rbac_adm_perm_do2( struct comm_buffer_s *b )
{
  struct role_s *r;
  struct perm_s *perm;
  struct space_s *t;
	r=(struct role_s *)(b->context.object.data);
	perm=(struct perm_s *)(b->user2);
	if( (t=space_find(perm->space))==NULL )
	{	errstr="Undefined space";
		return(-1);
	}
	if( b->user_data )
	{	if( rbac_role_add_perm(r,perm->access,t)<0 )
			return(-1);
	}
	else
	{	if( rbac_role_del_perm(r,perm->access,t)<0 )
			return(-1);
	}
	return(0);
}

int rbac_adm( struct comm_buffer_s *to_wait, char *op, char *s1, char *s2, char *s3 )
{ int x;
	if( to_wait->do_phase>0 )
	{
		switch( to_wait->user_data )
		{	case RESULT_ALLOW:
			case RESULT_OK:
				return(0);
		}
		errstr="Permission denied";
		return(-1);
	}
	if( !strcmp(op,"lu") )
		return( load_users("/etc/passwd") );
	if( !strcmp(op,"cr") )
		return( rbac_adm_create_role(to_wait,s1) );
	if( !strcmp(op,"dr") )
		return( rbac_adm_delete_role(to_wait,s1) );
	if( !strcmp(op,"ra") )
		return( rbac_adm_hierarchy(1,to_wait,s1,s2) );
	if( !strcmp(op,"rd") )
		return( rbac_adm_hierarchy(0,to_wait,s1,s2) );
	if( !strcmp(op,"ua") )
		return( rbac_adm_user(1,to_wait,s1,s2) );
	if( !strcmp(op,"ud") )
		return( rbac_adm_user(0,to_wait,s1,s2) );
	if( !strcmp(op,"pa") )
		return( rbac_adm_perm(1,to_wait,s1,s2,s3) );
	if( !strcmp(op,"pd") )
		return( rbac_adm_perm(0,to_wait,s1,s2,s3) );
	if( !strcmp(op,"save") )
	{	/*if( (x=atoi(s2))==0 )*/	x=16;
		if( rbac_save(rbac_conffile /*s1*/,x)<0 )
		{	errstr="Can't write RBAC configuration file";
			return(-1);
		}
		return(0);
	}
	errstr="Unknown command";
	return(-1);
}
