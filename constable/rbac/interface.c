/**
 * @file interface.c
 * @short
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: interface.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

static int rbac_adm_interf_handler_notify( struct comm_buffer_s *cb, struct event_handler_s *h, struct event_context_s *c )
{ struct medusa_attribute_s *a;
    int pid,x;
    void *xp;
    if( (a=get_attribute(c->operation.class,"sysnr"))==NULL )
        return(-1);
    object_get_val(&(c->operation),a,&x,sizeof(x));
    if( x != SYS_RBACADM )
        return(0);
    if( (a=get_attribute(c->operation.class,"pid"))==NULL )
        return(-1);
    object_get_val(&(c->operation),a,&pid,sizeof(pid));
    if( (a=get_attribute(mem,"pid"))==NULL )
        return(-1);
    object_set_val(mem,a,&pid,sizeof(pid));

    if( (a=get_attribute(c->operation.class,"arg1"))==NULL )
        return(-1);
    object_get_val(&(c->operation),a,&xp,sizeof(xp));
    if( (a=get_attribute(mem->class,"address"))==NULL )
        return(-1);
    object_set_val(mem,a,&xp,sizeof(xp));


}

