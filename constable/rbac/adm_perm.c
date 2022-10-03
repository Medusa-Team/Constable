/**
 * @file adm_perm.c
 * @short
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: adm_perm.c,v 1.3 2002/12/13 16:10:32 marek Exp $
 */

#include "rbac.h"
#include "../space.h"
#include "../event.h"
#include "../generic.h"
#include "../constable.h"
#include "../language/error.h"
#include <stdlib.h>
#include <string.h>

static int rbac_h_create_role( struct comm_buffer_s *cb, struct event_handler_s *h, struct event_context_s *c )
{
    if( !generic_test_vs_tree(AT_CREATE,c,rbac_t_role->class_handler->root) &&
            !generic_test_vs_tree(AT_CREATE,c,rbac_t_ROLE->class_handler->root) )
        c->result=RESULT_DENY;
    else	c->result=RESULT_ALLOW;
    return(0);
}

static int rbac_h_delete_role( struct comm_buffer_s *cb, struct event_handler_s *h, struct event_context_s *c )
{
    if( !generic_test_vs(AT_ERASE,c) )
        c->result=RESULT_DENY;
    else	c->result=RESULT_ALLOW;
    return(0);
}

static int rbac_h_role_hierarchy( struct comm_buffer_s *cb, struct event_handler_s *h, struct event_context_s *c )
{
    if( !generic_test_vs(AT_CREATE,c) )
        c->result=RESULT_DENY;
    else	c->result=RESULT_ALLOW;
    return(0);
}

static int rbac_h_user_assign( struct comm_buffer_s *cb, struct event_handler_s *h, struct event_context_s *c )
{
    if( !generic_test_vs(AT_CONTROL,c) )
        c->result=RESULT_DENY;
    else	c->result=RESULT_ALLOW;
    return(0);
}

static int rbac_h_role_assign( struct comm_buffer_s *cb, struct event_handler_s *h, struct event_context_s *c )
{
    if( !generic_test_vs(AT_CONTROL,c) )
        c->result=RESULT_DENY;
    else	c->result=RESULT_ALLOW;
    return(0);
}

static int rbac_h_perm_assign( struct comm_buffer_s *cb, struct event_handler_s *h, struct event_context_s *c )
{
    if( !generic_test_vs(AT_CONTROL,c) )
        c->result=RESULT_DENY;
    else	c->result=RESULT_ALLOW;
    return(0);
}

int rbac_adm_perm_init( void )
{ struct event_handler_s *eh;
    struct event_names_s *event;

    if( (eh=malloc(6*sizeof(struct event_handler_s)))==NULL )
    {	init_error(Out_of_memory);
        return(-1);
    }
    /* create_role */
    event=event_type_find_name("create_role");
    strncpy(eh[0].op_name,"rbac_h_create_role",MEDUSA_OPNAME_MAX);
    eh[0].handler=rbac_h_create_role;
    eh[0].local_vars=NULL;
    if( register_event_handler(&(eh[0]),event,&(event->handlers_hash[EHH_VS_ALLOW]),ALL_OBJ,ALL_OBJ)<0 )
        return(init_error("rbac: Can't register handler for create_role"));
    /* delete_role */
    event=event_type_find_name("delete_role");
    strncpy(eh[1].op_name,"rbac_h_delete_role",MEDUSA_OPNAME_MAX);
    eh[1].handler=rbac_h_delete_role;
    eh[1].local_vars=NULL;
    if( register_event_handler(&(eh[1]),event,&(event->handlers_hash[EHH_VS_ALLOW]),ALL_OBJ,ALL_OBJ)<0 )
        return(init_error("rbac: Can't register handler for delete_role"));
    /* role_hierarchy */
    event=event_type_find_name("role_hierarchy");
    strncpy(eh[2].op_name,"rbac_h_role_hierarchy",MEDUSA_OPNAME_MAX);
    eh[2].handler=rbac_h_role_hierarchy;
    eh[2].local_vars=NULL;
    if( register_event_handler(&(eh[2]),event,&(event->handlers_hash[EHH_VS_ALLOW]),ALL_OBJ,ALL_OBJ)<0 )
        return(init_error("rbac: Can't register handler for role_hierarchy"));
    /* uses_assign */
    event=event_type_find_name("uses_assign");
    strncpy(eh[3].op_name,"rbac_h_user_assign",MEDUSA_OPNAME_MAX);
    eh[3].handler=rbac_h_user_assign;
    eh[3].local_vars=NULL;
    if( register_event_handler(&(eh[3]),event,&(event->handlers_hash[EHH_VS_ALLOW]),ALL_OBJ,ALL_OBJ)<0 )
        return(init_error("rbac: Can't register handler for uses_assign"));
    /* role_assign */
    event=event_type_find_name("role_assign");
    strncpy(eh[4].op_name,"rbac_h_role_assign",MEDUSA_OPNAME_MAX);
    eh[4].handler=rbac_h_role_assign;
    eh[4].local_vars=NULL;
    if( register_event_handler(&(eh[4]),event,&(event->handlers_hash[EHH_VS_ALLOW]),ALL_OBJ,ALL_OBJ)<0 )
        return(init_error("rbac: Can't register handler for role_assign"));
    /* permission_assign */
    event=event_type_find_name("permission_assign");
    strncpy(eh[5].op_name,"rbac_h_perm_assign",MEDUSA_OPNAME_MAX);
    eh[5].handler=rbac_h_perm_assign;
    eh[5].local_vars=NULL;
    if( register_event_handler(&(eh[5]),event,&(event->handlers_hash[EHH_VS_ALLOW]),ALL_OBJ,ALL_OBJ)<0 )
        return(init_error("rbac: Can't register handler for permission_assign"));
    return(0);
}

