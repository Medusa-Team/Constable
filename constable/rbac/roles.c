/**
 * @file roles.c
 * @short
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: roles.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include "rbac.h"
#include "../space.h"
#include "../generic.h"
#include "../language/error.h"
#include <stdlib.h>
#include <string.h>

struct role_s *rbac_roles=NULL;

int rbac_roles_need_reinit=1;

struct role_s * rbac_role_add( char *name )
{ struct role_s *n,**p;
    int x=1;
    for(p=&rbac_roles;(*p)!=NULL;p=&((*p)->next))
        if( (x=strcmp((*p)->name,name))>=0 )
            break;
    if( x==0 )
    {	errstr="Redefinition of role";
        return(NULL);
    }
    if( (n=malloc(sizeof(struct role_s)))==NULL )
    {	errstr=Out_of_memory;
        return(NULL);
    }
    memset(n,0,sizeof(struct role_s));
    //	for(x=0;i<NR_ACCESS_TYPES;x++)
    //		vs_clear(n->vs[x]);
    strncpy(n->name,name,sizeof(n->name));
    n->ua=NULL;
    n->perm=NULL;
    n->sup=n->sub=NULL;
    n->next=*p;
    n->object.next=NULL;
    n->object.attr.offset=0;
    n->object.attr.length=sizeof(struct role_s);
    n->object.attr.type=MED_TYPE_END;
    n->object.flags=0;
    n->object.class=rbac_role_class;
    n->object.data=(char*)n;
    *p=n;
    return(n);
}

int rbac_role_del( struct role_s *role )
{ struct role_s **p,*o;
    struct permission_assignment_s *a;
    for(p=&rbac_roles;(*p)!=NULL;p=&((*p)->next))
        if( (*p)==role )
            break;
    if( (*p)!=role )
    {	errstr="Undefined role";
        return(-1);
    }

    o= (*p);
    while( o->ua!=NULL )
        rbac_del_ua(o->ua->user,o);
    while( o->perm!=NULL )
    {	a=o->perm;
        o->perm=o->perm->next;
        free(a);
    }
    while( o->sup!=NULL )
        rbac_del_hierarchy(o->sup->sup_role,o);
    while( o->sub!=NULL )
        rbac_del_hierarchy(o,o->sub->sub_role);

    *p= (*p)->next;
    free(o);
    return(0);
}

struct role_s * rbac_role_find( char *name )
{ struct role_s *p;
    for(p=rbac_roles;p!=NULL;p=p->next)
        if( strcmp(p->name,name)==0 )
            return(p);
    return(NULL);
}

int rbac_add_ua( struct user_s *user, struct role_s *role )
{ struct user_assignment_s *n;
    int i;
    if( (n=malloc(sizeof(struct user_assignment_s)))==NULL )
    {	errstr=Out_of_memory;
        return(-1);
    }
    for(i=0;i<user->nr_roles;i++)
    {	if( user->roles[i]==role )
        {	free(n);
            errstr="User already assignet to this role";
            return(-1);
        }
    }
    if( user->nr_roles >= USER_MAX_ROLES )
    {	for(i=0;i<USER_MAX_ROLES;i++)
            if( user->roles[i]==NULL )
                break;
        if( i>=USER_MAX_ROLES )
        {	errstr="Reached maximum roles per user";
            return(-1);
        }
        user->roles[i]=role;
    }
    else
        user->roles[user->nr_roles++]=role;
    n->user=user;
    n->role=role;
    n->next_role=user->ua;
    user->ua=n;
    n->next_user=role->ua;
    role->ua=n;
    return(0);
}

int rbac_del_ua( struct user_s *user, struct role_s *role )
{ struct user_assignment_s *a,**u,**r;
    int i;

    for(a=user->ua;a!=NULL;a=a->next_role)
    {	if( a->role==role )
            break;
    }
    for(i=0;i<user->nr_roles;i++)
    {	if( user->roles[i]==role )
        {	user->roles[i]=NULL;
            break;
        }
    }
    if( a==NULL )
    {	errstr="Not user of role";
        return(-1);
    }
    for(u=&(role->ua);*u!=a;u=&((*u)->next_user));
    for(r=&(user->ua);*r!=a;r=&((*r)->next_role));
    *u= (*u)->next_user;
    *r= (*r)->next_role;
    free(a);
    return(0);
}

/*     sup         sub		D:
    A -+     +- E		vs_sub = A | B | C
       |     |		VS[AT_MEMBER] = E | F | G
    B -+- D -+- F
       |     |		VS[AT_*] = E | F | G
    C -+     +- G

 */
void rbac_inherit_sub( struct role_s *r )
{ struct hierarchy_s *p;
    for(p=r->sub;p!=NULL;p=p->next_sub)
    {
        vs_add(r->vs_sub,p->sub_role->vs_sub);
        rbac_inherit_sub(p->sub_role);
    }
}

void rbac_inherit_sup( struct role_s *r )
{ struct hierarchy_s *p;
    int a;
    for(p=r->sup;p!=NULL;p=p->next_sup)
    {	for(a=0;a<NR_ACCESS_TYPES;a++)
            vs_add(r->vs[a],p->sup_role->vs[a]);
        rbac_inherit_sup(p->sup_role);
    }
}

int rbac_roles_reinit( void )
{ struct role_s *r;
    struct permission_assignment_s *p;
    int i;
    vs_t *v;
    for(r=rbac_roles;r!=NULL;r=r->next)
    {
        for(i=0;i<NR_ACCESS_TYPES;i++)
            vs_clear(r->vs[i]);
        vs_clear(r->vs_sub);
        r->object.class=rbac_ROLE_class;
        generic_set_handler(rbac_t_ROLE.class_handler,rbac_comm,&(r->object));
        r->object.class=rbac_role_class;
        generic_set_handler(rbac_t_role.class_handler,rbac_comm,&(r->object));
    }
    for(r=rbac_roles;r!=NULL;r=r->next)
    {	for(p=r->perm;p!=NULL;p=p->next)
        {	for(i=0;i<p->n;i++)
            {	if( (v=space_get_vs(p->space[i]))==NULL )
                    continue;	/* error */
                vs_add(v,r->vs[p->access[i]]);
            }
        }
    }
    for(r=rbac_roles;r!=NULL;r=r->next)
        rbac_inherit_sup(r);
    for(r=rbac_roles;r!=NULL;r=r->next)
        rbac_inherit_sub(r);
    rbac_roles_need_reinit=0;
    return(0);
}

static int is_subrole( struct role_s *role, struct role_s *test )
{ struct hierarchy_s *h;
    for(h=role->sub;h!=NULL;h=h->next_sub)
    {	if( h->sub_role==test )
            return(1);
        if( is_subrole(h->sub_role,test) )
            return(1);
    }
    return(0);
}

int rbac_set_hierarchy( struct role_s *sup_role, struct role_s *sub_role )
{ struct hierarchy_s *n;
    if( is_subrole(sub_role,sup_role) )
    {	errstr="Recursive subroles";
        return(-1);
    }
    if( (n=malloc(sizeof(struct hierarchy_s)))==NULL )
    {	errstr=Out_of_memory;
        return(-1);
    }
    n->sup_role= sup_role;
    n->sub_role= sub_role;
    n->next_sup= sub_role->sup;
    sub_role->sup= n;
    n->next_sub= sup_role->sub;
    sup_role->sub= n;
    rbac_inherit_sup(sub_role);
    rbac_inherit_sub(sup_role);
    return(0);
}

int rbac_del_hierarchy( struct role_s *sup_role, struct role_s *sub_role )
{ struct hierarchy_s *h,**p,**b;
    for(h=sup_role->sub;h!=NULL;h=h->next_sub)
    {	if( h->sub_role==sub_role )
            break;
    }
    if( h==NULL )
    {	errstr="Not subrole";
        return(-1);
    }
    for(b=&(sup_role->sub);*b!=h;b=&((*b)->next_sub));
    for(p=&(sub_role->sup);*p!=h;p=&((*p)->next_sup));
    *b= (*b)->next_sub;
    *p= (*p)->next_sup;
    free(h);
    rbac_roles_need_reinit=1;
    return(0);
}

int rbac_role_add_perm( struct role_s *role, int which, struct space_s *t )
{ struct permission_assignment_s **p;
    vs_t *v;
    if( which<0 || which>=NR_ACCESS_TYPES )
    {	errstr="Invalid access type";
        return(-1);
    }
    for(p=&(role->perm);*p!=NULL;p=&((*p)->next))
        if( (*p)->n < 8 )
            break;
    if( *p==NULL )
    {	if( (*p=malloc(sizeof(struct permission_assignment_s)))==NULL )
        {	errstr=Out_of_memory;
            return(-1);
        }
        (*p)->next=NULL;
        (*p)->n=0;
    }
    (*p)->access[(*p)->n]=which;
    (*p)->space[(*p)->n]=t;
    (*p)->n++;
    if( (v=space_get_vs(t))==NULL )
        return(-1);
    vs_add(v,role->vs[which]);
    rbac_inherit_sup(role);
    return(0);
}

int rbac_role_del_perm( struct role_s *role, int which, struct space_s *t )
{ struct permission_assignment_s **p,*o;
    int i;
    if( which<0 || which>=NR_ACCESS_TYPES )
    {	errstr="Invalid access type";
        return(-1);
    }
    for(p=&(role->perm);*p!=NULL;p=&((*p)->next))
    {	for(i=0;i<(*p)->n;i++)
        {	if( (*p)->access[i]==which && (*p)->space[i]==t )
            {	for(i++;i<(*p)->n;i++)
                {	(*p)->access[i-1]=(*p)->access[i];
                    (*p)->space[i-1]=(*p)->space[i];
                }
                (*p)->n--;
                if( (*p)->n==0 )
                {	o=(*p);
                    (*p)=(*p)->next;
                    free(o);
                }
                rbac_roles_need_reinit=1;
                return(0);
            }
        }
    }
    errstr="Not permissoion of role";
    return(-1);
}

