/**
 * @file variables.c
 * @short Functions for variables handling
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: variables.c,v 1.3 2002/10/23 10:25:43 marek Exp $
 */

#include "variables.h"
#include "../constable.h"

struct object_s *alloc_var( char *name, struct medusa_attribute_s *attr, struct class_s * class )
{ struct object_s *v;
    int l;
    if( !attr == !class )
        return(NULL);
    if( class==NULL )
        l=attr->length;
    else	l=class->m.size;
    if( (v=calloc(1,sizeof(struct object_s)+l))==NULL )
        return(NULL);
    v->next=NULL;
    v->flags=OBJECT_FLAG_LOCAL;
    if( class==NULL )
    {	v->attr = *attr;
        v->attr.offset=0;
        v->class=NULL;
        v->data= (char*)(v+1);
    }
    else
    {	v->attr.offset=0;
        v->attr.length=class->m.size;
        v->attr.type=MED_TYPE_END;
        v->class=class;
        v->data= (char*)(v+1);
    }
    //memset(v->data, 0, l); // Just for valgrind
    strncpy(v->attr.name,name,MEDUSA_ATTRNAME_MAX);
    return(v);
}

struct object_s *data_alias_var( char *name, struct object_s *o )
{ struct object_s *v;
    if( o==NULL )
        return(NULL);
    if( (v=malloc(sizeof(struct object_s)))==NULL )
        return(NULL);
    v->next=NULL;
    v->flags=o->flags;
    v->attr=o->attr;
    v->class=o->class;
    v->data=o->data;
    strncpy(v->attr.name,name,MEDUSA_ATTRNAME_MAX);
    return(v);
}

struct object_s *free_var( struct object_s *v )
{ struct object_s *r;
    r=v->next;
    free(v);
    return(r);
}

void free_vars( struct object_s **v )
{ struct object_s *i=*v;
    *v=NULL;
    while( i!=NULL )
        i=free_var(i);
}

struct object_s *var_link( struct object_s **v, struct object_s *var )
{ int r=1;
    for(;(*v)!=NULL;v=&((*v)->next))
        if( (r=strncmp((*v)->attr.name,var->attr.name,MEDUSA_ATTRNAME_MAX))>=0 )
            break;
    if( r==0 )
        return(NULL);
    var->next=*v;
    *v=var;
    return(var);
}

struct object_s *get_var( struct object_s *v, char *name )
{ int r;
    for(;v!=NULL;v=v->next)
        if( (r=strncmp(v->attr.name,name,MEDUSA_ATTRNAME_MAX))>=0 )
        {	if( r==0 )
                return(v);
            else	return(NULL);
        }
    return(NULL);
}

