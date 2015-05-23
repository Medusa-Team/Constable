/**
 * @file users.c
 * @short
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: users.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include "rbac.h"
#include "../language/error.h"
#include <stdlib.h>
#include <string.h>

struct user_s *rbac_users=NULL;

struct user_s * rbac_user_add( char *name, uid_t uid )
{ struct user_s *n,**p;
    int x=1;
    for(p=&rbac_users;(*p)!=NULL;p=&((*p)->next))
        if( (x=strcmp((*p)->name,name))>=0 )
            break;
    if( x==0 )
    {	errstr="Redefinition of user";
        return(NULL);
    }
    if( (n=malloc(sizeof(struct user_s)))==NULL )
    {	errstr=Out_of_memory;
        return(NULL);
    }
    memset(n,0,sizeof(struct user_s));
    //	vs_clear(n->vs);
    strncpy(n->name,name,sizeof(n->name));
    n->uid=uid;
    n->ua=NULL;
    n->nr_roles=0;
    n->next=*p;
    n->object.next=NULL;
    n->object.attr.offset=0;
    n->object.attr.length=sizeof(struct user_s);
    n->object.attr.type=MED_TYPE_END;
    n->object.flags=0;
    n->object.class=rbac_user_class;
    n->object.data=(char*)n;
    *p=n;
    return(n);
}

struct user_s * rbac_user_find( char *name )
{ struct user_s *p;
    for(p=rbac_users;p!=NULL;p=p->next)
        if( strcmp(p->name,name)==0 )
            return(p);
    return(NULL);
}

/* spravit hash na uidy */
struct user_s * rbac_user_find_by_uid( uid_t uid )
{ struct user_s *p;
    for(p=rbac_users;p!=NULL;p=p->next)
        if( p->uid==uid )
            return(p);
    return(NULL);
}

