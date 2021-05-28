
#include "constable.h"
#include "comm.h"
#include "space.h"
#include <pthread.h>

/* cesty sa do space pridavaju naraz.. nestane sa, ze by sa pridavali do dvoch
   na striedacku. Je to zabezpecene c language/conf_lang.c
 */

struct space_s *global_spaces=NULL;

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
            *errstr=Out_of_memory;
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

static int is_excluded( struct tree_s *test, struct space_s *space );

/* is_included: 0 - not, 1 - yes */
static int is_included( struct tree_s *test, struct space_s *space, int force_recursive )
{ ltree_t *l;
    for(l=space->ltree;l!=NULL;l=l->prev)
    {	switch( l->type&LTREE_T_MASK )
        { case LTREE_T_TREE:
            if( !(l->type&LTREE_EXCLUDE) )
            {	if( test==l->path_or_space )
                    return(!is_excluded(test,space));
                if( (force_recursive || l->type&LTREE_RECURSIVE)
                        && tree_is_offspring(test,l->path_or_space) )
                    return(!is_excluded(test,space));
            }
            break;
        case LTREE_T_SPACE:
            if( !(l->type&LTREE_EXCLUDE) )
            {	if( is_included(test,l->path_or_space,
                                (l->type&LTREE_RECURSIVE)?1:force_recursive) )
                    return(1);
            }
            break;
        }
    }
    return(0);
}

/* is_excluded: 0 - not, 1 - yes, 2 - recursive */
static int is_excluded( struct tree_s *test, struct space_s *space )
{ ltree_t *l;
    for(l=space->ltree;l!=NULL;l=l->prev)
    {	switch( l->type&LTREE_T_MASK )
        { case LTREE_T_TREE:
            if( (l->type&LTREE_EXCLUDE) )
            {	if( test==l->path_or_space )
                    return((l->type&LTREE_RECURSIVE)?2:1);
                if( (l->type&LTREE_RECURSIVE)
                        && tree_is_offspring(test,l->path_or_space) )
                    return((l->type&LTREE_RECURSIVE)?2:1);
            }
            break;
        case LTREE_T_SPACE:
            if( (l->type&LTREE_EXCLUDE) )
            {	if( is_included(test,l->path_or_space,
                                (l->type&LTREE_RECURSIVE)) )
                    return((l->type&LTREE_RECURSIVE)?2:1);
            }
            break;
        }
    }
    return(0);
}

void space_for_one_path( struct tree_s *t, struct space_s *space, int recursive, void(*func)(struct tree_s *t, int recursive, void *arg), void *arg )
{ int r;
    struct tree_s *p;
    if( !(r=is_excluded(t,space)) )
        func(t,recursive,arg);
    if( recursive && r!=2 )
    {	for(p=t->child;p!=NULL;p=p->next)
            space_for_one_path(p,space,1,func,arg);
        for(p=t->reg;p!=NULL;p=p->next)
            space_for_one_path(p,space,1,func,arg);
    }
}

typedef void(*fepf_t)(struct tree_s *t, int recursive, void *arg);

/* space_for_each_path: */
int space_for_each_path( struct space_s *space, int force_recursive, void(*func)(struct tree_s *t, int recursive, void *arg), void *arg )
{ ltree_t *l;
    for(l=space->ltree;l!=NULL;l=l->prev)
    {	switch( l->type&LTREE_T_MASK )
        { case LTREE_T_TREE:
            if( !(l->type&LTREE_EXCLUDE) )
                space_for_one_path(l->path_or_space,space,
                                   (force_recursive||(l->type&LTREE_RECURSIVE))?1:0,
                                   func,arg);
            break;
        case LTREE_T_SPACE:
            if( !(l->type&LTREE_EXCLUDE) )
                space_for_each_path(l->path_or_space,
                                    (l->type&LTREE_RECURSIVE)?1:force_recursive,
                                    func,arg);
            break;
        }
    }
    return(0);
}

static void set_primary_space_do( struct tree_s *t, int recursive, struct space_s *space )
{
    if( recursive )
    {	if( t->recursive_primary_space!=NULL && t->recursive_primary_space!=space )
            warning("Redefinition of recursive primary space %s to %s",
                    t->recursive_primary_space->name,
                    space->name);
        t->recursive_primary_space=space;
    }
    else
    {	if( t->primary_space!=NULL && t->primary_space!=space )
            warning("Redefinition of primary space %s to %s",
                    t->primary_space->name,
                    space->name);
        t->primary_space=space;
    }
}

static void unset_primary_space_do( struct tree_s *t, int recursive, struct space_s *space )
{
    if( recursive )
    {	if( t->recursive_primary_space==space )
            t->recursive_primary_space=NULL;
    }
    else
    {	if( t->primary_space==space )
            t->primary_space=NULL;
    }
}

/* ----------------------------------- */

struct tree_add_event_mask_do_s {
    int conn;
    struct event_type_s *type;
};
static void tree_add_event_mask_do( struct tree_s *p, int recursive, struct tree_add_event_mask_do_s *arg )
{
    if( arg->type->object==p->type->class_handler->classname->classes[arg->conn] )
        event_mask_or2(p->events[arg->conn].event,arg->type->mask);
}

/* ----------------------------------- */

struct tree_add_vs_do_s {
    int which;
    const vs_t *vs;
};
static void tree_add_vs_do( struct tree_s *p, int recursive, struct tree_add_vs_do_s *arg )
{
    if( recursive )
        vs_add(arg->vs,p->recursive_vs[arg->which]);
    else
        vs_add(arg->vs,p->vs[arg->which]);
}

/* ----------------------------------- */



int space_add_path( struct space_s *space, int type, void *path_or_space )
{
    //  int a;
    if( path_or_space==NULL )	return(-1);
    if( (type & LTREE_EXCLUDE) )
    {	if( space->primary )
            space_for_each_path(space,0,(fepf_t)unset_primary_space_do,space);
        /* FIXME: cele tieto dynamicke veci opravit */
        //		for(a=0;a<NR_ACCESS_TYPES;a++)	/* 2*ZLE!!! */
        //			path->type->sub_vs(path,recursive,a,space->vs[a]);
    }
    if( (space->ltree=new_path(space->ltree,path_or_space))==NULL )
        return(-1);
    space->ltree->type=type;
    if( space->primary )
        space_for_each_path(space,0,(fepf_t)set_primary_space_do,space);
    /* toto je uz mozno zbytocne !!!
    for(a=0;a<NR_ACCESS_TYPES;a++)
    { struct tree_add_vs_do_s arg;
        arg.which=s;
        arg.vs=space->vs[a];
        space_for_each_path(space,0,tree_add_vs_do,&arg);
    }
*/
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
        register_event_handler(handler,&(subj_node->subject_handlers[ehh_list]),space_get_vs(subject),space_get_vs(object));
    }
    else if( subj_node )
        //{printf("ZZZ: registrujem pri subjekte %s\n",subj_node->name);
        register_event_handler(handler,&(subj_node->subject_handlers[ehh_list]),space_get_vs(subject),space_get_vs(object));
    //}
    else if( obj_node )
        //{printf("ZZZ: registrujem pri objekte %s\n",obj_node->name);
        register_event_handler(handler,&(obj_node->object_handlers[ehh_list]),space_get_vs(subject),space_get_vs(object));
    //}
    else
        //{printf("ZZZ: registrujem podla svetov\n");
        register_event_handler(handler,&(type->handlers_hash[ehh_list]),space_get_vs(subject),space_get_vs(object));
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
                if( type->object==type->op[0] && type->object==t->type->class_handler->classname->classes[comm->conn] )
                    event_mask_or2(t->events[comm->conn].event,type->mask);
                else	comm->conf_error(comm,"event %s subject class mismatch",hh->handler->op_name);
                //printf("ZZZ: mismas type->object=%p type->op[0]=%p %p\n",type->object,type->op[0],t->type->class_handler->classname->classes[comm->conn]);
            }
            evhash_foreach(hh,t->object_handlers[ehh_list])
            {	if( (en=event_type_find_name(hh->handler->op_name))==NULL || (type=en->events[comm->conn])==NULL )
                {	comm->conf_error(comm,"Unknown event %s\n",hh->handler->op_name);
                    continue;
                }
                if( type->object==type->op[1] && type->object==t->type->class_handler->classname->classes[comm->conn] )
                    event_mask_or2(t->events[comm->conn].event,type->mask);
                else	comm->conf_error(comm,"event %s object class mismatch",hh->handler->op_name);
            }
        }
    }
    for(p=t->child;p!=NULL;p=p->next)
        tree_comm_reinit(comm,p);
    for(p=t->reg;p!=NULL;p=p->next)
        tree_comm_reinit(comm,p);
}

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
            if( le->subject!=NULL && le->subject!=ALL_OBJ && type->object==type->op[0] )
            { struct tree_add_event_mask_do_s arg;
                arg.conn=comm->conn;
                arg.type=type;
                space_for_each_path(le->subject,0,(fepf_t)tree_add_event_mask_do,&arg);
            }
            if( le->object!=NULL && le->object!=ALL_OBJ && type->object==type->op[1] )
            { struct tree_add_event_mask_do_s arg;
                arg.conn=comm->conn;
                arg.type=type;
                space_for_each_path(le->object,0,(fepf_t)tree_add_event_mask_do,&arg);
            }
        }
    }
    return(0);
}

int space_add_vs( struct space_s *space, int which, vs_t *vs )
{
    if( which<0 || which>=NR_ACCESS_TYPES )
        return(-1);
    vs_add(vs,space->vs[which]);
    { struct tree_add_vs_do_s arg;
        arg.which=which;
        arg.vs=vs;
        space_for_each_path(space,0,(fepf_t)tree_add_vs_do,&arg);
    }
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
    { struct tree_add_vs_do_s arg;
        arg.which=AT_MEMBER;
        arg.vs=space->vs[AT_MEMBER];
        space_for_each_path(space,0,(fepf_t)tree_add_vs_do,&arg);
    }
    return(space->my_vs->vs);
}

