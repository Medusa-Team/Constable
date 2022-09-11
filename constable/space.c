/*
 * Constable: space.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: space.c,v 1.4 2002/12/16 10:40:53 marek Exp $
 */

#include "constable.h"
#include "comm.h"
#include "space.h"

#include <stdio.h>
#include <pthread.h>

/* FIXME: check infinite loops between space_for_each_path, is_included and is_excluded!
*/
#define	MAX_SPACE_PATH	32

struct space_s *global_spaces=NULL;

/**
 * Create a new path and append it to the end of \p prev.
 * \param prev tree node to append to
 * \param path_or_space tree node to append
 */
static ltree_t *new_path( ltree_t *prev, void *path_or_space )
{ ltree_t *l;
    if( (l=malloc(sizeof(ltree_t)))==NULL )
    {	char **errstr = (char**) pthread_getspecific(errstr_key);
        *errstr=Out_of_memory;
        return(NULL);
    }
    l->prev=prev;
    l->path_or_space=path_or_space;
    return(l);
}

static levent_t *new_levent( levent_t **prev, struct event_handler_s *handler, struct space_s *subject, struct space_s *object )
{ levent_t *l;
    if( (l=malloc(sizeof(levent_t)))==NULL )
    {	char **errstr = (char**) pthread_getspecific(errstr_key);
        *errstr=Out_of_memory;
        return(NULL);
    }
    l->handler=handler;
    l->subject=subject;
    l->object=object;
    l->prev=*prev;
    *prev=l;
    return(l);
}

struct space_s *space_find( char *name )
{ struct space_s *t;
    for(t=global_spaces;t!=NULL;t=t->next)
        if( !strcmp(t->name,name) )
            return(t);
    return(NULL);
}

struct space_s *space_create( char *name, int primary )
{ struct space_s *t;
    int a;
    if( name!=NULL )
    {	if( space_find(name)!=NULL )
        {	char **errstr = (char**) pthread_getspecific(errstr_key);
            *errstr=Space_already_defined;
            return(NULL);
        }
    }
    else	name=" _";
    if( (t=malloc(sizeof(struct space_s)+strlen(name)+1))==NULL )
    {	char **errstr = (char**) pthread_getspecific(errstr_key);
        *errstr=Out_of_memory;
        return(NULL);
    }
    strcpy(t->name,name);
    t->my_vs=NULL;
    for(a=0;a<NR_ACCESS_TYPES;a++)
        vs_clear(t->vs[a]);
    t->levent=NULL;
    t->ltree=NULL;
    t->ltree_exclude=NULL;
    t->primary=primary;
    t->next=global_spaces;
    global_spaces=t;
    return(t);
}

struct space_path_s {
    struct space_s *path[MAX_SPACE_PATH];
    int	n;
};

static int space_path_add( struct space_path_s *sp, struct space_s *space )
{ int i;
    for(i=0;i<sp->n;i++)
    {	if( sp->path[i]==space )
        {	fatal("Infinite loop in spaces!");
            return(0);
        }
    }
    if( sp->n>=MAX_SPACE_PATH )
    {	fatal("Too many nested spaces!");
        return(0);
    }
    sp->path[sp->n++]=space;
    return(1);
}

static int is_excluded( struct tree_s *test, struct space_s *space );

static int space_path_exclude( struct space_path_s *sp, struct tree_s *t )
{ int i,r,rn;
    r=0;
    for(i=0;i<sp->n;i++)
    {	rn=is_excluded(t,sp->path[i]);
        r= rn>r ? rn : r ;
        if( r>1 )
            break;
    }
    return(r);
}

/* is_included: 0 - not, 1 - yes */
static int is_included_i( struct tree_s *test, struct space_path_s *sp, struct space_s *space, int force_recursive )
{ ltree_t *l;
    if( !space_path_add(sp,space) )
        return(0);
    for(l=space->ltree;l!=NULL;l=l->prev)
    {	switch( l->type&LTREE_T_MASK )
        { case LTREE_T_TREE:
            if( !(l->type&LTREE_EXCLUDE) )
            {	if( tree_is_equal(test,l->path_or_space) )
                    return(!space_path_exclude(sp,test));
                if( (force_recursive || l->type&LTREE_RECURSIVE)
                        && tree_is_offspring(test,l->path_or_space) )
                    return(!space_path_exclude(sp,test));
            }
            break;
        case LTREE_T_SPACE:
            if( !(l->type&LTREE_EXCLUDE) )
            {	if( is_included_i(test,sp,l->path_or_space,
                                  (l->type&LTREE_RECURSIVE)?1:force_recursive) )
                    return(1);
            }
            break;
        }
    }
    return(0);
}

static int is_included( struct tree_s *test, struct space_s *space, int force_recursive )
{ struct space_path_s sp;
    sp.n=0;
    return(is_included_i(test,&sp,space,force_recursive));
}

/* is_excluded: 0 - not, 1 - yes, 2 - recursive */
static int is_excluded( struct tree_s *test, struct space_s *space )
{ ltree_t *l;
    int r=0;
    for(l=space->ltree;l!=NULL;l=l->prev)
    {	switch( l->type&LTREE_T_MASK )
        { case LTREE_T_TREE:
            if( (l->type&LTREE_EXCLUDE) )
            {	if( tree_is_equal(test,l->path_or_space) )
                    return((l->type&LTREE_RECURSIVE)?2:1);
                if( (l->type&LTREE_RECURSIVE)
                        && tree_is_offspring(test,l->path_or_space) )
                    r=((l->type&LTREE_RECURSIVE)?2:1);
            }
            break;
        case LTREE_T_SPACE:
            if( (l->type&LTREE_EXCLUDE) )
            {	if( is_included(test,l->path_or_space,
                                (l->type&LTREE_RECURSIVE)) )
                    r=((l->type&LTREE_RECURSIVE)?2:1);
            }
            break;
        }
        if( r>1 )
            break;
    }
    return(r);
}

struct space_for_one_path_s {
    struct space_path_s *sp;
    int recursive;
    void(*func)(struct tree_s *t, void *arg);
    void *arg;
};

static void space_for_one_path_i( struct tree_s *t, struct space_for_one_path_s *a )
{ int r;
    struct tree_s *p;

    r=space_path_exclude(a->sp,t);
    if( !r )
        a->func(t,a->arg);
    if( a->recursive && r<2 )
    {	for(p=t->child;p!=NULL;p=p->next)
            space_for_one_path_i(p,a);
        for(p=t->regex_child;p!=NULL;p=p->next)
            space_for_one_path_i(p,a);
    }
}

typedef void(*fepf_t)(struct tree_s *t, void *arg);

static void space_for_one_path( struct tree_s *t, struct space_path_s *sp, int recursive, void(*func)(struct tree_s *t, void *arg), void *arg )
{ struct space_for_one_path_s a;

    a.sp=sp;
    a.recursive=recursive;
    a.func=func;
    a.arg=arg;
    tree_for_alternatives(t,(fepf_t)space_for_one_path_i,&a);
}

/* space_for_each_path: */
int space_for_each_path_i( struct space_s *space, struct space_path_s *sp, int force_recursive, void(*func)(struct tree_s *t, void *arg), void *arg )
{ ltree_t *l;

    if( !space_path_add(sp,space) )
        return(-1);
    for(l=space->ltree;l!=NULL;l=l->prev)
    {	switch( l->type&LTREE_T_MASK )
        { case LTREE_T_TREE:
            if( !(l->type&LTREE_EXCLUDE) )
                space_for_one_path(l->path_or_space,sp,
                                   (force_recursive||(l->type&LTREE_RECURSIVE))?1:0,
                                   func,arg);
            break;
        case LTREE_T_SPACE:
            if( !(l->type&LTREE_EXCLUDE) )
                space_for_each_path_i(l->path_or_space,sp,
                                      (l->type&LTREE_RECURSIVE)?1:force_recursive,
                                      func,arg);
            break;
        }
    }
    sp->n--;
    return(0);
}

int space_for_each_path( struct space_s *space, void(*func)(struct tree_s *t, void *arg), void *arg )
{ struct space_path_s sp;
    sp.n=0;
    return(space_for_each_path_i(space,&sp,0,func,arg));
}

static void set_primary_space_do( struct tree_s *t, struct space_s *space )
{
    if( t->primary_space!=NULL && t->primary_space!=space )
        warning("Redefinition of primary space %s to %s",
                t->primary_space->name,
                space->name);
    t->primary_space=space;
}

/* ----------------------------------- */

struct tree_add_event_mask_do_s {
    int conn;
    struct event_type_s *type;
};
static void tree_add_event_mask_do( struct tree_s *p, struct tree_add_event_mask_do_s *arg )
{
    if( arg->type->monitored_operand==p->type->class_handler->classname->classes[arg->conn] )
        event_mask_or2(p->events[arg->conn].event,arg->type->mask);
}

/* ----------------------------------- */

struct tree_add_vs_do_s {
    int which;
    const vs_t *vs;
};
static void tree_add_vs_do( struct tree_s *p, struct tree_add_vs_do_s *arg )
{
    vs_add(arg->vs,p->vs[arg->which]);
}

/* ----------------------------------- */



/**
 * Add path node to the list of paths of space and set its type.
 * \param space New path will be added to this virtual space.
 * \param type Type of the new node (see macros LTREE_*).
 * \param path_or_space Pointer to the node that will be added.
 * \return 0 on success, -1 otherwise
 */
int space_add_path( struct space_s *space, int type, void *path_or_space )
{
    if( path_or_space==NULL )	return(-1);
    if( (space->ltree=new_path(space->ltree,path_or_space))==NULL )
        return(-1);
    space->ltree->type=type;
    return(0);
}

/* space_apply_all must be called after all spaces are defined */
int space_apply_all( void )
{ struct space_s *space;
    int a;
    tree_expand_alternatives();
    for(space=global_spaces;space!=NULL;space=space->next)
    {
        if( space->primary )
            space_for_each_path(space,(fepf_t)set_primary_space_do,space);
        for(a=0;a<NR_ACCESS_TYPES;a++)
        { struct tree_add_vs_do_s arg;
            arg.which=a;
            arg.vs=space->vs[a];
            space_for_each_path(space,(fepf_t)tree_add_vs_do,&arg);
        }

    }
    return(0);
}

/* neprida event_mask */
int space_add_event( struct event_handler_s *handler, int ehh_list, struct space_s *subject, struct space_s *object, struct tree_s *subj_node, struct tree_s *obj_node )
{ struct event_names_s *type;
    if( ehh_list<0 || ehh_list>=EHH_LISTS )
    {	error("Invalid ehh_list of handler");
        return(-1);
    }
    if( (type=event_type_find_name(handler->op_name))==NULL )
    {	error("Event %s does not exist\n",handler->op_name);
        return(-1);
    }
    if( subject!=NULL && subject!=ALL_OBJ && new_levent(&(subject->levent),handler,subject,object)==NULL )
        return(-1);
    if( object!=NULL && object!=ALL_OBJ && new_levent(&(object->levent),handler,subject,object)==NULL )
        return(-1);

    if( subj_node && obj_node )
    {	if( (object=space_create(NULL,0))==NULL )
            return(-1);
        space_add_path(object,0,obj_node);
        if( new_levent(&(object->levent),handler,subject,object)==NULL )
            return(-1);
        //printf("ZZZ: subj=%s obj=%s tak registrujem podla svetov\n",subj_node->name,obj_node->name);
        register_event_handler(handler,type,&(subj_node->subject_handlers[ehh_list]),space_get_vs(subject),space_get_vs(object));
    }
    else if( subj_node )
        //{printf("ZZZ: registrujem pri subjekte %s\n",subj_node->name);
        register_event_handler(handler,type,&(subj_node->subject_handlers[ehh_list]),space_get_vs(subject),space_get_vs(object));
    //}
    else if( obj_node )
        //{printf("ZZZ: registrujem pri objekte %s\n",obj_node->name);
        register_event_handler(handler,type,&(obj_node->object_handlers[ehh_list]),space_get_vs(subject),space_get_vs(object));
    //}
    else
        //{printf("ZZZ: registrujem podla svetov\n");
        register_event_handler(handler,type,&(type->handlers_hash[ehh_list]),space_get_vs(subject),space_get_vs(object));
    //}
    return(0);
}

static void tree_comm_reinit( struct comm_s *comm, struct tree_s *t )
{ struct tree_s *p;
    struct event_hadler_hash_s *hh;
    struct event_names_s *en;
    struct event_type_s *type;
    int ehh_list;

    if( t->events )
    {	event_mask_clear2(t->events[comm->conn].event);
        for(ehh_list=0;ehh_list<EHH_LISTS;ehh_list++)
        {
            evhash_foreach(hh,t->subject_handlers[ehh_list])
            {	if( (en=event_type_find_name(hh->handler->op_name))==NULL || (type=en->events[comm->conn])==NULL )
                {	comm->conf_error(comm,"Unknown event %s\n",hh->handler->op_name);
                    continue;
                }
                if( type->monitored_operand==type->op[0] )
                {	if( type->monitored_operand==t->type->class_handler->classname->classes[comm->conn] )
                        event_mask_or2(t->events[comm->conn].event,type->mask);
                    else	comm->conf_error(comm,"event %s subject class mismatch",hh->handler->op_name);
                }
                //printf("ZZZ: mismas type->object=%p type->op[0]=%p %p\n",type->object,type->op[0],t->type->class_handler->classname->classes[comm->conn]);
            }
            evhash_foreach(hh,t->object_handlers[ehh_list])
            {	if( (en=event_type_find_name(hh->handler->op_name))==NULL || (type=en->events[comm->conn])==NULL )
                {	comm->conf_error(comm,"Unknown event %s\n",hh->handler->op_name);
                    continue;
                }
                if( type->monitored_operand==type->op[1] )
                {	if( type->monitored_operand==t->type->class_handler->classname->classes[comm->conn] )
                        event_mask_or2(t->events[comm->conn].event,type->mask);
                    else	comm->conf_error(comm,"event %s object class mismatch",hh->handler->op_name);
                }
            }
        }
    }
    for(p=t->child;p!=NULL;p=p->next)
        tree_comm_reinit(comm,p);
    for(p=t->regex_child;p!=NULL;p=p->next)
        tree_comm_reinit(comm,p);
}

/* space_init_event_mask must be called after connection was estabilished */
int space_init_event_mask( struct comm_s *comm )
{ struct space_s *space;
    levent_t *le;
    struct event_names_s *en;
    struct event_type_s *type;

    tree_comm_reinit(comm,&global_root);
    for( space=global_spaces;space!=NULL;space=space->next)
    {	for(le=space->levent;le!=NULL;le=le->prev)
        {
            if( (en=event_type_find_name(le->handler->op_name))==NULL || (type=en->events[comm->conn])==NULL )
            {	comm->conf_error(comm,"Unknown event %s\n",le->handler->op_name);
                continue;
            }
            if( ((type->op[0]==NULL) != (le->subject==NULL)) ||
                    ((type->op[1]==NULL) != (le->object==NULL)) )
            {	comm->conf_error(comm,"Invalid use of event %s\n",le->handler->op_name);
            }
            if( le->subject!=NULL && le->subject!=ALL_OBJ && type->monitored_operand==type->op[0] )
            { struct tree_add_event_mask_do_s arg;
                arg.conn=comm->conn;
                arg.type=type;
                space_for_each_path(le->subject,(fepf_t)tree_add_event_mask_do,&arg);
            }
            if( le->object!=NULL && le->object!=ALL_OBJ && type->monitored_operand==type->op[1] )
            { struct tree_add_event_mask_do_s arg;
                arg.conn=comm->conn;
                arg.type=type;
                space_for_each_path(le->object,(fepf_t)tree_add_event_mask_do,&arg);
            }
        }
    }
    return(0);
}

/**
 * Place subject with virtual space \p space into virtual spaces defined by \p
 * vs for access type \p which. Original virtual spaces of \p space won't be
 * changed.
 * \param space Virtual spaces of the subject
 * \param which Access type
 * \param vs Virtual spaces to add
 * \return 0 on success, -1 on invalid which value
 */
int space_add_vs( struct space_s *space, int which, vs_t *vs )
{
    if( which<0 || which>=NR_ACCESS_TYPES )
        return(-1);
    vs_add(vs,space->vs[which]);
    return(0);
}

vs_t *space_get_vs( struct space_s *space )
{
    if( space==NULL || space==ALL_OBJ)
        return((vs_t*)space);
    if( space->my_vs!=NULL )
        return(space->my_vs->vs);
    if( (space->my_vs=vs_alloc(space->name))==NULL )
        return(NULL);
    //printf("ZZZ:space_get_vs %s 0x%08x\n",space->name,*(space->my_vs->vs));
    vs_add(space->my_vs->vs,space->vs[AT_MEMBER]);
    return(space->my_vs->vs);
}

/* ----------------------------------- */

int space_vs_to_str( vs_t *vs, char *out, int size )
{ struct space_s *space;
    vs_t tvs[MAX_VS_BITS/32];
    int pos,l;
    vs_set(vs,tvs);
    pos=0;
    for(space=global_spaces;space!=NULL;space=space->next)
    {	if( !vs_isclear(space->vs[AT_MEMBER])
                && vs_issub(space->vs[AT_MEMBER],tvs)
                && space->name[0]!=0 && space->name[0]!=' ' )
        {	vs_sub(space->vs[AT_MEMBER],tvs);
            if( (l=strlen(space->name))+2>=size )
                return(-1);
            if( pos>0 )
            {	out[pos]='|';
                pos++;
                size--;
            }
            strcpy(out+pos,space->name);
            pos+=l;
            size-=l;
        }
    }
    if( !vs_isclear(tvs) )
    {	if( (sizeof(tvs)*9)/8+3>=size )
            return(-1);
        if( pos>0 )
        {	out[pos]='|';
            pos++;
            size--;
        }
#ifdef BITMAP_DIPLAY_LEFT_RIGHT
        for(l=0;l<sizeof(tvs);l++)
        {	if( l>0 && (l&0x03)==0 )
            {	out[pos++]=':';	size--;		}
            sprintf(out+pos,"%02x",((char*)tvs)[l]);
            pos+=2; size-=2;
        }
#else
        for(l=sizeof(tvs)-1;l>=0;l--)
        {	sprintf(out+pos,"%02x",((char*)tvs)[l]);
            pos+=2; size-=2;
            if( l>0 && (l&0x03)==0 )
            {	out[pos++]=':';	size--;		}
        }
#endif
    }
    out[pos]=0;
    return(pos);
}

