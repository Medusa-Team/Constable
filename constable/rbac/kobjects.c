/**
 * @file kobjects.c
 * @short 
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: kobjects.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include "rbac.h"
#include "../medusa_object.h"
#include "../event.h"
#include "../object.h"
#include "../space.h"
#include "../generic.h"
#include "../constable.h"
#include "../language/error.h"
#include "../comm.h"
#include "../init.h"

struct class_s *rbac_user_class,*rbac_perm_class,*rbac_role_class,*rbac_ROLE_class;

static struct medusa_comm_class_s rbac_user_mclass={
	1,sizeof(struct user_s),"rbac_user"
};

static struct medusa_comm_attribute_s rbac_user_attr[]={
	MED_ATTR_x(struct user_s,uid,"uid",MED_TYPE_UNSIGNED|MED_TYPE_PRIMARY_KEY|MED_TYPE_READ_ONLY),
	MED_ATTR_BITMAP(struct user_s,vs,"vs"),
	MED_ATTR_BITMAP(struct user_s,cinfo,"o_cinfo"),
	MED_ATTR_STRING(struct user_s,name,"name"),
	MED_ATTR_END
};

static struct medusa_comm_class_s rbac_perm_mclass={
	2,sizeof(struct user_s),"rbac_perm"
};

static struct medusa_comm_attribute_s rbac_perm_attr[]={
	MED_ATTR_x(struct perm_s,access,"access",MED_TYPE_UNSIGNED|MED_TYPE_PRIMARY_KEY|MED_TYPE_READ_ONLY),
	MED_ATTR_x(struct perm_s,space,"space",MED_TYPE_STRING|MED_TYPE_PRIMARY_KEY|MED_TYPE_READ_ONLY),
	MED_ATTR_BITMAP(struct perm_s,vs,"vs"),
	MED_ATTR_BITMAP(struct perm_s,cinfo,"o_cinfo"),
	MED_ATTR_END
};

static struct medusa_comm_class_s rbac_role_mclass={
	3,sizeof(struct role_s),"rbac_role"
};

static struct medusa_comm_attribute_s rbac_role_attr[]={
	MED_ATTR_BITMAP(struct role_s,vs_sub,"vs"),
	MED_ATTR_BITMAP(struct role_s,vs[AT_RECEIVE],"vsr"),
	MED_ATTR_BITMAP(struct role_s,vs[AT_SEND],"vsw"),
	MED_ATTR_BITMAP(struct role_s,vs[AT_SEE],"vss"),
	MED_ATTR_BITMAP(struct role_s,vs[AT_CREATE],"vsc"),
	MED_ATTR_BITMAP(struct role_s,vs[AT_ERASE],"vsd"),
	MED_ATTR_BITMAP(struct role_s,vs[AT_ENTER],"vse"),
	MED_ATTR_BITMAP(struct role_s,vs[AT_CONTROL],"vsx"),
	MED_ATTR_BITMAP(struct role_s,cinfo_sub,"o_cinfo"),
	MED_ATTR_x(struct role_s,name,"name",MED_TYPE_STRING|MED_TYPE_PRIMARY_KEY|MED_TYPE_READ_ONLY),
	MED_ATTR_END
};

static struct medusa_comm_class_s rbac_ROLE_mclass={
	4,sizeof(struct role_s),"rbac_ROLE"
};

static struct medusa_comm_attribute_s rbac_ROLE_attr[]={
	MED_ATTR_BITMAP(struct role_s,vs[AT_MEMBER],"vs"),
	MED_ATTR_BITMAP(struct role_s,vs[AT_RECEIVE],"vsr"),
	MED_ATTR_BITMAP(struct role_s,vs[AT_SEND],"vsw"),
	MED_ATTR_BITMAP(struct role_s,vs[AT_SEE],"vss"),
	MED_ATTR_BITMAP(struct role_s,vs[AT_CREATE],"vsc"),
	MED_ATTR_BITMAP(struct role_s,vs[AT_ERASE],"vsd"),
	MED_ATTR_BITMAP(struct role_s,vs[AT_ENTER],"vse"),
	MED_ATTR_BITMAP(struct role_s,vs[AT_CONTROL],"vsx"),
	MED_ATTR_BITMAP(struct role_s,cinfo_sup,"o_cinfo"),
	MED_ATTR_x(struct role_s,name,"name",MED_TYPE_STRING|MED_TYPE_PRIMARY_KEY|MED_TYPE_READ_ONLY),
	MED_ATTR_END
};

static struct medusa_comm_acctype_s rbac_acc_create_role={
	1,0,0xffff,{0,0},"create_role",{"process","process"}
};

static struct medusa_comm_attribute_s rbac_acc_attr_create_role[]={
	MED_ATTR_END
};

static struct medusa_comm_acctype_s rbac_acc_delete_role={
	2,0,0xffff,{0,3},"delete_role",{"process","role"}
};

static struct medusa_comm_attribute_s rbac_acc_attr_none[]={
	MED_ATTR_END
};

static struct medusa_comm_acctype_s rbac_acc_role_hierarchy={
	3,0,0xffff,{0,3},"role_hierarchy",{"process","role"}
};

static struct medusa_comm_acctype_s rbac_acc_user_assign={
	4,0,0xffff,{0,1},"uses_assign",{"process","user"}
};

static struct medusa_comm_acctype_s rbac_acc_role_assign={
	5,0,0xffff,{0,3},"role_assign",{"process","role"}
};

static struct medusa_comm_acctype_s rbac_acc_perm_assign={
	6,0,0xffff,{0,2},"permission_assign",{"process","permission"}
};

static int rbac_set_roles( struct class_handler_s *h, struct comm_s *comm, struct object_s *o )
{
	VALIDATE_ROLES();
	return(0);
}

static int rbac_set_perm( struct class_handler_s *h, struct comm_s *comm, struct object_s *o )
{ int access;
  char space[64];
  struct space_s *s;
  struct tree_s *t;
	if( object_get_val(o,get_attribute(o->class,"access"),&access,sizeof(access))<0
	   || object_get_val(o,get_attribute(o->class,"space"),&space,sizeof(space))<0 )
		return(-1);
	if( access<0 || access>=NR_ACCESS_TYPES )
		return(-1);
	if( (s=space_find(space))==NULL )
		return(-1);
	if( (t=find_one(rbac_t_perm.class_handler->root,s->name))==NULL )
		return(0);
	if( (t=find_one(t,access_names[access]))==NULL )
		return(0);

	object_add_vs(o,AT_MEMBER,t->vs[AT_MEMBER]);
	return(0);
}

static int rbac_enter_tree_node( struct class_handler_s *h, struct comm_s *comm, struct object_s *o, struct tree_s *node )
{
	errstr="rbac objects cannot be moved";
	return(-1);
}

struct comm_s *rbac_comm;

int rbac_comm_answer( struct comm_s *c, struct comm_buffer_s *b, int result );

int rbac_comm_alloc( struct module_s *m )
{
	if( (rbac_comm=comm_new("_RBAC",sizeof(struct comm_s)))==NULL )
		return(-1);
	rbac_comm->state=0;
	rbac_comm->read=NULL;
	rbac_comm->write=NULL;
	rbac_comm->answer=rbac_comm_answer;
	rbac_comm->fetch_object=NULL;
	rbac_comm->update_object=NULL;
	rbac_comm->conf_error=NULL;
	return(0);
}

int rbac_comm_init( struct module_s *m )
{
	if( (rbac_user_class=add_class(rbac_comm,&rbac_user_mclass,rbac_user_attr))==NULL )
		return(init_error("rbac: Can't register user class"));
	if( (rbac_perm_class=add_class(rbac_comm,&rbac_perm_mclass,rbac_perm_attr))==NULL )
		return(init_error("rbac: Can't register perm class"));
	if( (rbac_role_class=add_class(rbac_comm,&rbac_role_mclass,rbac_role_attr))==NULL )
		return(init_error("rbac: Can't register role class"));
	if( (rbac_ROLE_class=add_class(rbac_comm,&rbac_ROLE_mclass,rbac_ROLE_attr))==NULL )
		return(init_error("rbac: Can't register ROLE class"));
	if( event_type_add(rbac_comm,&rbac_acc_create_role,rbac_acc_attr_create_role)==NULL )
		return(init_error("rbac: Can't register create_role access type"));
	if( event_type_add(rbac_comm,&rbac_acc_delete_role,rbac_acc_attr_none)==NULL )
		return(init_error("rbac: Can't register delete_role access type"));
	if( event_type_add(rbac_comm,&rbac_acc_role_hierarchy,rbac_acc_attr_none)==NULL )
		return(init_error("rbac: Can't register role_hierarchy access type"));
	if( event_type_add(rbac_comm,&rbac_acc_user_assign,rbac_acc_attr_none)==NULL )
		return(init_error("rbac: Can't register user_assign access type"));
	if( event_type_add(rbac_comm,&rbac_acc_role_assign,rbac_acc_attr_none)==NULL )
		return(init_error("rbac: Can't register role_assign access type"));
	if( event_type_add(rbac_comm,&rbac_acc_perm_assign,rbac_acc_attr_none)==NULL )
		return(init_error("rbac: Can't register perm_assign access type"));
	return(0);
}

struct tree_type_s rbac_t_user,rbac_t_perm,rbac_t_role,rbac_t_ROLE;

int rbac_object_init( void )
{
  struct class_handler_s *ch;

  	if( (ch=generic_get_new_class_handler_s(get_class_by_name("rbac_user"),-1))==NULL )
	{	init_error(Out_of_memory);
		return(-1);
	}
//	ch->set_handler=rbac_set_roles;
	ch->enter_tree_node=rbac_enter_tree_node;
	rbac_t_user.name="user";
	rbac_t_user.size=sizeof(struct tree_s);
	rbac_t_user.class_handler=ch;
	rbac_t_user.init=NULL;
	rbac_t_user.child_type=&rbac_t_user;
	if( (ch->root=register_tree_type(&rbac_t_user))==NULL )
	{	free(ch->cinfo_offset);
		free(ch);
		return(init_error("Can't register tree user"));
	}
	if( class_add_handler(ch->classname,ch)<0 )
	{	free(ch->cinfo_offset);
		free(ch);
		return(init_error("Can't add classhandler for user/"));
	}

  	if( (ch=generic_get_new_class_handler_s(get_class_by_name("rbac_perm"),-1))==NULL )
	{	init_error(Out_of_memory);
		return(-1);
	}
	ch->set_handler=rbac_set_perm;
	ch->enter_tree_node=rbac_enter_tree_node;
	rbac_t_perm.name="perm";
	rbac_t_perm.size=sizeof(struct tree_s);
	rbac_t_perm.class_handler=ch;
	rbac_t_perm.init=NULL;
	rbac_t_perm.child_type=&rbac_t_perm;
	if( (ch->root=register_tree_type(&rbac_t_perm))==NULL )
	{	free(ch->cinfo_offset);
		free(ch);
		return(init_error("Can't register tree perm"));
	}
	if( class_add_handler(ch->classname,ch)<0 )
	{	free(ch->cinfo_offset);
		free(ch);
		return(init_error("Can't add classhandler for perm/"));
	}

  	if( (ch=generic_get_new_class_handler_s(get_class_by_name("rbac_role"),-1))==NULL )
	{	init_error(Out_of_memory);
		return(-1);
	}
	ch->set_handler=rbac_set_roles;
	ch->enter_tree_node=rbac_enter_tree_node;
	rbac_t_role.name="role";
	rbac_t_role.size=sizeof(struct tree_s);
	rbac_t_role.class_handler=ch;
	rbac_t_role.init=NULL;
	rbac_t_role.child_type=&rbac_t_role;
	if( (ch->root=register_tree_type(&rbac_t_role))==NULL )
	{	free(ch->cinfo_offset);
		free(ch);
		return(init_error("Can't register tree role"));
	}
	if( class_add_handler(ch->classname,ch)<0 )
	{	free(ch->cinfo_offset);
		free(ch);
		return(init_error("Can't add classhandler for role/"));
	}

  	if( (ch=generic_get_new_class_handler_s(get_class_by_name("rbac_ROLE"),-1))==NULL )
	{	init_error(Out_of_memory);
		return(-1);
	}
	ch->set_handler=rbac_set_roles;
	ch->enter_tree_node=rbac_enter_tree_node;
	rbac_t_ROLE.name="ROLE";
	rbac_t_ROLE.size=sizeof(struct tree_s);
	rbac_t_ROLE.class_handler=ch;
	rbac_t_ROLE.init=NULL;
	rbac_t_ROLE.child_type=&rbac_t_ROLE;
	if( (ch->root=register_tree_type(&rbac_t_ROLE))==NULL )
	{	free(ch->cinfo_offset);
		free(ch);
		return(init_error("Can't register tree ROLE"));
	}
	if( class_add_handler(ch->classname,ch)<0 )
	{	free(ch->cinfo_offset);
		free(ch);
		return(init_error("Can't add classhandler for ROLE/"));
	}

	comm_conn_init(rbac_comm);
	return(0);
}

int rbac_init( struct module_s *m );
int rbac_load( struct module_s *m );

struct module_s rbac_module={
	NULL,
	"RBAC",
	"/etc/rbac.conf",
	rbac_comm_alloc,
	rbac_comm_init,
	rbac_init,
	rbac_load
};

int _rbac_init( void )
{
	return(add_module(&rbac_module));
}

