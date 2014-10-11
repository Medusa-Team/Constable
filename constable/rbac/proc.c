/**
 * @file proc.c
 * @short 
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: proc.c,v 1.3 2002/12/13 16:10:32 marek Exp $
 */

#include "rbac.h"
#include "../event.h"
#include "../comm.h"
#include "../generic.h"
#include "../constable.h"
#include "../init.h"
#include "../language/error.h"
#include <stdlib.h>

struct proc_class_handler_s {
	struct class_handler_s ch;
	struct medusa_attribute_s **attr_uid;
	
};

static struct class_handler_s *rbac_proc_ch;

static struct tree_s *rbac_proc_get_tree_node( struct class_handler_s *h, struct comm_s *comm, struct object_s *o )
{
/*
	!!!!!!!! pozor, lebo ich je viacej, co chcem vratit !!!!!!!!!!!!
	tak nie.
	proces nie je rola! ale proces je v tych istych vs-kach ako rola ;-)
*/
	return(NULL);
}

static int rbac_proc_set_handler( struct class_handler_s *h, struct comm_s *comm, struct object_s *o )
{ struct user_s *u;
  struct role_s *r;
  uid_t uid;
  uintptr_t session;
  int i,a;

	VALIDATE_ROLES();
	if( ((struct proc_class_handler_s*)h)->attr_uid[comm->conn]==NULL )
		return(0);
	object_get_val(o,((struct proc_class_handler_s*)h)->attr_uid[comm->conn],&uid,sizeof(uid));
	if( (u=rbac_user_find_by_uid(uid))==NULL )
	{	runtime("RBAC: Unknown uid %d",uid);
		return(-1);
	}
	object_add_event(o,&(u->cinfo->events[comm->conn].event[0]));
	session=(uintptr_t)CINFO(o,h,comm);
	i=0;
	while( session && i<USER_MAX_ROLES )
	{	if( (session & 0x01) && (r=u->roles[i]) )
		{	for(a=0;a<NR_ACCESS_TYPES;a++)
				object_add_vs(o,a,r->vs[a]);
			object_add_vs(o,AT_MEMBER,r->vs_sub);
			object_add_event(o,&(r->cinfo_sub->events[comm->conn].event[0]));
			object_add_event(o,&(r->cinfo_sup->events[comm->conn].event[0]));
		}
		session>>=1;
		i++;
	}
	object_add_event(o,((struct event_names_s*)(h->user))->events[comm->conn]->mask);
	return(0);
}

static int rbac_proc_get_vs( struct class_handler_s *h, struct comm_s *comm, struct object_s *o, vs_t *vs, int n )
{ struct user_s *u;
  struct role_s *r;
  uid_t uid;
  uintptr_t session;
  int i;

	VALIDATE_ROLES();
	if( ((struct proc_class_handler_s*)h)->attr_uid[comm->conn]==NULL )
		return(0);
	object_get_val(o,((struct proc_class_handler_s*)h)->attr_uid[comm->conn],&uid,sizeof(uid));
	if( (u=rbac_user_find_by_uid(uid))==NULL )
	{	runtime("RBAC: Unknown uid %d",uid);
		return(-1);
	}
	session=(uintptr_t)CINFO(o,h,comm);
	i=0;
	while( session && i<USER_MAX_ROLES )
	{	if( (session & 0x01) && (r=u->roles[i]) )
		{	vs_add(r->vs[n],vs);
//			if( n==AT_MEMBER )
//				vs_add(r->vs_sub,vs);
		}
		session>>=1;
		i++;
	}
	return(0);
}

static struct space_s *rbac_proc_get_primary_space( struct class_handler_s *h, struct comm_s *comm, struct object_s *o )
{
	return(NULL);
}

static int rbac_proc_setuid_handler_notify( struct comm_buffer_s *cb, struct event_handler_s *h, struct event_context_s *c )
{
	CINFO(&(c->subject),rbac_proc_ch,cb->comm)=~(uintptr_t)0;
	object_do_sethandler(&(c->subject));
	c->result=RESULT_OK;
	return(0);
}

static int rbac_proc_enter_tree_node( struct class_handler_s *h, struct comm_s *comm, struct object_s *o, struct tree_s *node )
{
	errstr="rbac objects cannot be moved";
	return(-1);
}

static int rbac_proc_init_comm( struct class_handler_s *h, struct comm_s *comm )
{ struct event_type_s *event=NULL;
  struct class_s *class;
  int x;
	
	h->cinfo_offset[comm->conn]= -1;
	((struct proc_class_handler_s*)h)->attr_uid[comm->conn]= NULL;
	if( (class=h->classname->classes[comm->conn])==NULL )
	{	comm->conf_error(comm,"Undefined class %s",h->classname->name);
		return(0);
	}
	if( (((struct proc_class_handler_s*)h)->attr_uid[comm->conn]=get_attribute(class,"uid"))==NULL )
	{	comm->conf_error(comm,"%s has no attribute uid",h->classname->name);
		return(0);
	}
	if( h->user!=NULL )
	{	if( (event=((struct event_names_s*)(h->user))->events[comm->conn])!=NULL )
		{	if( class!=event->op[0] )
				comm->conf_error(comm,"rbac_proc_init_comm: class mismatch");
		}
		else
		{	comm->conf_error(comm,"Undefined event %s",((struct event_names_s*)(h->user))->name);
			return(-1);
		}
	}
	if( (event!=NULL && ((event->m.actbit&0xc000)==0x4000))
		|| (x=class_alloc_subject_cinfo(class))<0 )
		x=class_alloc_object_cinfo(class);
	if( x<0 )
	{	comm->conf_error(comm,"Can't alloc %s cinfo for rbac",h->classname->name);
		return(-1);
	}
	h->cinfo_offset[comm->conn]=x;
	return(0);
}

int rbac_init( struct module_s *m )
{
  struct event_handler_s *eh;
  struct event_names_s *event;

	if( rbac_object_init()<0 )
		return(-1);
	if( rbac_adm_perm_init()<0 )
		return(-1);

	if( (rbac_proc_ch=generic_get_new_class_handler_s(get_class_by_name("process"),sizeof(struct proc_class_handler_s)))==NULL )
	{	init_error(Out_of_memory);
		return(-1);
	}
	rbac_proc_ch->user=(void*)(event=event_type_find_name("setuid"));
	if( (((struct proc_class_handler_s*)rbac_proc_ch)->attr_uid=(struct medusa_attribute_s**)comm_new_array(sizeof(struct medusa_attribute_s*)))==NULL )
	{	init_error(Out_of_memory);
		return(-1);
	}
	rbac_proc_ch->init_comm=rbac_proc_init_comm;
	rbac_proc_ch->set_handler=rbac_proc_set_handler;
	rbac_proc_ch->get_vs=rbac_proc_get_vs;
	rbac_proc_ch->get_tree_node=rbac_proc_get_tree_node;
	rbac_proc_ch->get_primary_space=rbac_proc_get_primary_space;
	rbac_proc_ch->enter_tree_node=rbac_proc_enter_tree_node;

	if( class_add_handler(rbac_proc_ch->classname,rbac_proc_ch)<0 )
	{	free(rbac_proc_ch->cinfo_offset);
		free(((struct proc_class_handler_s*)rbac_proc_ch)->attr_uid);
		return(init_error("rbac: Can't add classhandler to process"));
	}

	if( (eh=malloc(1*sizeof(struct event_handler_s)))==NULL )
	{	init_error(Out_of_memory);
		free(rbac_proc_ch->cinfo_offset);
		free(((struct proc_class_handler_s*)rbac_proc_ch)->attr_uid);
		return(-1);
	}
	strcpy(eh->op_name,"rbac:");
	strncpy(eh->op_name+5,event->name,MEDUSA_OPNAME_MAX-5);
	eh->handler=rbac_proc_setuid_handler_notify;
	eh->local_vars=NULL;
	if( register_event_handler(eh,event,&(event->handlers_hash[EHH_NOTIFY_ALLOW]),ALL_OBJ,ALL_OBJ)<0 )
	{	free(eh);
		return(init_error("rbac: Can't register setuid handler"));
	}
	return(0);
}

