/**
 * @file conf_lang.c
 * @short Grammar and functions for configuration language
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: conf_lang.c,v 1.9 2003/12/30 18:08:18 marek Exp $
 */

#include "language.h"
#include "execute.h"
#include "../constable.h"
#include "../event.h"
#include "../tree.h"
#include "../space.h"
#include "../generic.h"

#include "conf_lang.h"

struct event_handler_s *function_init=NULL;
struct event_handler_s *function_debug=NULL;

compiler_class_t *global_compiler;

#define	STARGET	N|0x0a01
#define	STARGET1	N|0x0a02
#define	STARGET2	N|0x0a03
#define	STARGET3	N|0x0a04
#define	SPRIMARY	N|0x0a05
#define	S2	N|0x0a08
#define	SOBJ	N|0x0a09
#define	S3	N|0x0a0a
#define	SA	N|0x0a0b
#define	SGT	N|0x0a0c
#define	S4	N|0x0a0d
#define	S5	N|0x0a0e
#define	S6	N|0x0a0f
#define	SPATH	N|0x0a10
#define	SLEV	N|0x0a11
#define	SSP	N|0x0a12
#define	SPSP	N|0x0a13

/*

    tree "meno" [test_enter|clone ...] of <class> [by <op> <exp>] ;
    primary tree "meno" ;

    function <meno> ;
    function <meno> { <cmd> ... }

    [primary] space <meno> [recursive] "cesta" , [-] [recursive] "cesta" ... ;

    <space> <op> [:ehh_list] [<space>] { <cmd> ... }

    <space> <acces_type> <space> , [<acces_type>] <space> ... ;

 */

struct compile_tab_s conf_lang[]={
    //	{stav,{terminaly,END},{stack,END}},
{SPATH,{T_str,END},{T_str,Pstr2path,END}},
{SPATH,{END},{T_path,END}},
{START,{Tfunction,END},{Tfunction,T_id,S4,START,END}},
{S4,{T|';',END},{Pfuncdec,T|';',END}},
{S4,{END},{Pfuncdec,Pprogstart,T|'{',CMDS,T|'}',Pfuncend,END}},
{START,{Tprimary,END},{Tprimary,SPRIMARY,END}},
{SPRIMARY,{Tspace,END},{Tspace,T_id,SPSP,END}},
{SPRIMARY,{Ttree,END},{Ttree,T_str,Pptree,T|';',START,END}},
{START,{Tspace,END},{Tspace,T_id,SSP,END}},
{SSP,{T|'+',END},{Pfindspace,STARGET,START,END}},
{SSP,{T|'-',END},{Pfindspace,STARGET,START,END}},
{SSP,{T|';',END},{Pspace,T|';',START,END}},
{SSP,{END},{Pspace,T|'=',STARGET,START,END}},
{SPSP,{T|';',END},{Ppspace,T|';',START,END}},
{SPSP,{END},{Ppspace,T|'=',STARGET,START,END}},
{START,{TEND,END},{END}},

{STARGET,{T|'-',END},{T|'-',Pptnull,Pptexcl,STARGET1,END}},
{STARGET,{T|'+',END},{T|'+',Pptnull,STARGET1,END}},
{STARGET,{END},{Pptnull,STARGET1,END}},
{STARGET1,{Trecursive,END},{Trecursive,Pptrecur,STARGET2,END}},
{STARGET1,{END},{STARGET2,END}},
{STARGET2,{T_str,T_path,END},{SPATH,Pptpath,Paddpath,STARGET3,END}},
{STARGET2,{Tspace,END},{Tspace,T_id,Pfindspa2,Pptspace,Paddpath,STARGET3,END}},
{STARGET3,{T|',',END},{T|',',STARGET,END}},
{STARGET3,{T|'+',T|'-',END},{STARGET,END}},
{STARGET3,{END},{T|';',END}},

{START,{T_id,T_str,T_path,Trecursive,T|'*',END},{SGT,Psubject,S2,START,END}},

{SGT,{T|'*',END},{T|'*',Pnullpath,Pallspaces,END}},
{SGT,{T_id,END},{Pnullpath,T_id,Pfindspace,END}},
//	{SGT,{T_str,T_path,END},{Pnullpath,VAL|0,Pspace,Pptnull,SPATH,Paddpath,END}},
{SGT,{T_str,T_path,END},{SPATH,Psetpath,Pallspaces,END}},
{SGT,{Trecursive,END},{Pnullpath,Trecursive,VAL|0,Pspace,Pptnull,Pptrecur,SPATH,Paddpath,END}},

{S2,{T_id,END},{T_id,Pprogstart,SLEV,END}},
{SLEV,{T|':',END},{T|':',Tehhlist,Pehh_list,SOBJ,END}},
{SLEV,{END},{SOBJ,END}},
{SOBJ,{T|'{',END},{T|'{',CMDS,T|'}',Preg,END}},
{SOBJ,{END},{SGT,Pobject,T|'{',CMDS,T|'}',Preg,END}},
{S2,{Taccess,END},{SA,END}},
{SA,{Taccess,END},{Taccess,Paccess,SGT,Pobject,Paddvs,S3,END}},
{SA,{END},{SGT,Pobject,Paddvs,S3,END}},
{S3,{T|',',END},{T|',',SA,END}},
{S3,{END},{T|';',END}},
{SRET0,{END},{oLDI,VAL|RESULT_OK,OUT_VAL,oLTI,END}},
{START,{Ttree,END},{Ttree,T_str,Ptreetype,S5,END}},
{S5,{T_id,END},{T_id,Ptreeflags,S5,END}},
{S5,{END},{Tof,T_id,Pid2class,Psetclass,S6,END}},
{S6,{Tby,END},{Tby,T_id,Pprogstart,S_exp,oRET,Ptreereg,T|';',START,END}},
{S6,{END},{Ptreereg,T|';',START,END}},

{END}
};

static struct event_handler_s *handler=NULL;
static int handler_size=0;
static int handler_pos=0;

void canf_lang_out( struct compiler_out_class *o, sym_t s, uintptr_t d )
{
    if( s == TEND )
        return;
    if( handler==NULL )
    {	fatal("conf_lang_out");
        return;
    }
    if( handler_pos >= handler_size )
    {	handler_size= handler_pos + 8;
        if( (handler=realloc(handler,sizeof(struct event_handler_s)+handler_size*sizeof(uintptr_t)))==NULL )
        {	error(Out_of_memory);
            return;
        }
    }
    if( (s & TYP) == O )
        d=s;
    ((uintptr_t *)(handler->data))[handler_pos++]= d;
}

static void out_destroy( struct compiler_out_class *this )
{	return;
}

struct compiler_out_class s_canf_lang_out={
    out_destroy,
    0,
    canf_lang_out
};

void conf_lang_param_out( struct compiler_class *c, sym_t s )
{ static struct space_s *space=NULL;
    static struct space_s *space1=NULL;
    static struct space_s *space2=NULL;
    static struct tree_s *path=NULL;
    static struct tree_s *path1=NULL;
    static struct tree_s *path2=NULL;
    static char *op_name=NULL;
    static int access_type=0;
    static char *tree_name=NULL;
    static int flags,ehh_list;
    static struct class_names_s *class=NULL;
    static int path_type=0;
    struct event_names_s *event;
    uintptr_t x;
    int i=0;
    vs_t *v;
    if( (s&0x0f00) == 0x0100 )
    { unsigned long tmp;
        tmp=c->l.data;
        //		c->special_out(c,(s&0x0fff)|O);
        c->l.data= s;
        c->special_out(c,OUT_VAL);
#ifdef DEBUG_TRACE
        c->l.data= (global_compiler->lex->row)<<16 | (global_compiler->lex->col);
        c->special_out(c,OUT_VAL);
#endif
        c->l.data=tmp;
        return;
    }
    switch( s )
    {
    case Ppspace:	i=1;
    case Pspace:	if( c->l.data != 0 )
        {	space=space_find((char*)(c->l.data));
            if( space!=NULL && space->ltree==NULL && space->ltree_exclude==NULL && (!i == !(space->primary)) )
                break;
        }
        space=space_create((char*)(c->l.data),i);
        if( space==NULL ) {
            char **errstr = (char**) pthread_getspecific(errstr_key);
            error("space %s: %s",(char*)(c->l.data),*errstr);
        }
        break;
    case Pnullpath:
        path=NULL;
        break;
    case Psetpath:
        path=(struct tree_s*)(c->l.data);
        break;
    case Pstr2path:
        if( c->l.data == 0 )
        {	error("NULL path!");
            break;
        }
        c->l.data=(uintptr_t)create_path((char*)(c->l.data));
        break;
    case Pid2class:
        if( c->l.data == 0 )
        {	error("NULL class name!");
            break;
        }
        if( (x=(uintptr_t)(get_class_by_name((char*)(c->l.data))))==0 )
            error("Unknown class %s",(char*)(c->l.data));
        c->l.data=x;
        break;
    case Pptnull:	path_type=0;
        break;
    case Pptpath:	path_type=(path_type&~LTREE_T_MASK)|LTREE_T_TREE;
        break;
    case Pptspace:	path_type=(path_type&~LTREE_T_MASK)|LTREE_T_SPACE;
        break;
    case Pptrecur:	path_type|=LTREE_RECURSIVE;
        break;
    case Pptexcl:	path_type|=LTREE_EXCLUDE;
        break;
    case Paddpath:
        if( c->l.data == 0 || space==NULL )
        {	if( c->l.data == 0 )
                error("NULL path!");
            break;
        }
        if( space_add_path(space,path_type,(struct tree_s*)(c->l.data))<0 ) {
            char **errstr = (char**) pthread_getspecific(errstr_key);
            error("path : %s",*errstr);
        }
        break;
    case Pfindspace:
        if( c->l.data == 0 )
        {	error("NULL space name!");
            break;
        }
        if( (space=space_find((char*)(c->l.data)))==NULL )
            error("Undeclared space %s",(char*)(c->l.data));
        //				else if( space->ltree==NULL )
        //					error("Undefined space %s",(char*)(c->l.data));
        break;
    case Pfindspa2:
        if( (x=c->l.data) == 0 )
        {	error("NULL space name!");
            break;
        }
        if( (c->l.data=(uintptr_t)space_find((char*)(c->l.data)))==0 )
            error("Undeclared space %s",(char*)x);
        break;
    case Pallspaces:
        space=ALL_OBJ;
        break;
    case Psubject:
        space1=space;
        space2=NULL;
        path1=path;
        path2=NULL;
        break;
    case Pobject:
        space2=space;
        path2=path;
        break;
    case Pprogstart:
        ehh_list=EHH_VS_ALLOW;
        handler_size=8;
        handler_pos=0;
        if( (handler=malloc(sizeof(struct event_handler_s)+handler_size*sizeof(uintptr_t)))==NULL )
        {	error(Out_of_memory);
            break;
        }
        op_name=(char*)(c->l.data);
        handler->op_name[0]=0;
#ifdef DEBUG_TRACE
        snprintf(handler->op_name+MEDUSA_OPNAME_MAX,DT_POS_MAX,"%s:%d",c->lex->filename,c->lex->row);
#endif
        handler->handler=execute_handler;
        handler->local_vars=NULL; /* !!! zatial */
        break;
    case Pfuncend:
        if( op_name==NULL )
        {	error("NULL function name");
            break;
        }
        strcpy(handler->op_name,"func:");
        strncpy(handler->op_name+5,op_name,MEDUSA_OPNAME_MAX-5);
        if( !strcmp(op_name,"_init") )
            function_init=handler;
        else if( !strcmp(op_name,"_debug") )
            function_debug=handler;
        else
        {
            if( (x=lex_getkeyword(op_name,Tcallfunc))==0 )
            {	x=(uintptr_t)(malloc(sizeof(void *)));
                if( lex_addkeyword(op_name,Tcallfunc,x)<0 )
                    error("Duplicate definition of function %s",op_name);
            }
            else if( *((uintptr_t*)x)!=0 )
                error("Duplicate definition of function %s",op_name);
            *((uintptr_t*)x)=((uintptr_t)handler)+((uintptr_t)(((struct event_handler_s*)0)->data));
        }
        handler=NULL;
        break;
    case Pfuncdec:
        op_name=(char*)(c->l.data);
        if( op_name==NULL )
        {	error("NULL function name");
            break;
        }
        x=(uintptr_t)(malloc(sizeof(void *)));
        *((void**)x)=0;
        if( lex_addkeyword(op_name,Tcallfunc,x)<0 )
            //					warning("Duplicit declaration of function %s",op_name);
            ;
        break;
    case Pehh_list:
        ehh_list=c->l.data;
        break;
    case Preg:
        if( handler==NULL )
            break;
        if( op_name==NULL )
        {	error("NULL event name");
            break;
        }
        strncpy(handler->op_name,op_name,MEDUSA_OPNAME_MAX);
        op_name=NULL;
        if( space_add_event(handler,ehh_list,space1,space2,path1,path2)<0 )
            error("Can't register handler - Unknown operation %s",handler->op_name);
        handler=NULL;
        break;
    case Ptreereg:
        if( tree_name==NULL )
        {	error("NULL tree name");
            break;
        }
        event=NULL;
        if( handler!=NULL )
        {	strncpy(handler->op_name,tree_name,MEDUSA_OPNAME_MAX);
            if( op_name==NULL )
            {	error("NULL event name");
                break;
            }
            if( (event=event_type_find_name(op_name))==NULL )
                error("Unknown operation %s",op_name);
        }
        if( generic_init(tree_name,handler,event,class,flags)<0 )
            error("\nThe message has passed,\ngone up the screen this evening -\ndefinition failed",tree_name);
        /* The message has passed,
   gone up the screen this evening -
   the insertion failed
 */
        op_name=NULL;
        handler=NULL;
        class=NULL;
        tree_name=NULL;
        break;
    case Psetclass:
        if( c->l.data == 0 )
        {	error("NULL class!");
            break;
        }
        class=(struct class_names_s*)(c->l.data);
        break;
    case Ptreeflags:
        if( c->l.data==0 )
        {	error("NULL tree flags!");
            break;
        }
        if( !strcmp("test_enter",(char*)(c->l.data)) )
            flags|=GFL_USE_VSE;
        else if( !strcmp("clone",(char*)(c->l.data)) )
            flags|=GFL_FROM_OBJECT;
        else error("Invalid tree flag %s",(char*)(c->l.data));
        break;
    case Ptreetype:
        tree_name=(char*)(c->l.data);
        handler=NULL;
        class=NULL;
        flags=0;
        break;
    case Paccess:
        access_type=c->l.data;
        break;
    case Paddvs:
        //printf("ZZZ Paddvs %s <%d- %s\n",space1->name,access_type,space2->name);
        if( space1==ALL_OBJ || space2==ALL_OBJ )
        {	error("Space MUST be specified.");
            break;
        }
        if( (v=space_get_vs(space2))==NULL )
        {
            char **errstr = (char**) pthread_getspecific(errstr_key);
            error("%s",*errstr);
            break;
        }
        //printf("ZZZ Paddvs vs=0x%08x\n",*v);
        if( space_add_vs(space1,access_type,v)<0 )
            error("Can't add vs");
        break;
    case Pptree:
        if( c->l.data==0 )
        {	error("NULL primary tree!");
            break;
        }
        tree_set_default_path((char*)(c->l.data));
        break;
    case Psgvs:
        if( (c->l.data=(uintptr_t)(space_get_vs((struct space_s*)(c->l.data))))==0 )
        {
            char **errstr = (char**) pthread_getspecific(errstr_key);
            error("%s",*errstr);
            break;
        }
        break;
    }

}


static struct compiler_preprocessor_class *prep;

int language_init( char *filename )
{
    if( (global_compiler=compiler_create())==NULL )
        return(init_error("Can't initialize compiler"));
    if( (prep=f_preprocessor_create(filename))==NULL )
    {	global_compiler->destroy(global_compiler);
        return(init_error("Can't open file %s",filename));
    }
    inc_use(prep);
    if( (global_compiler->lex=lex_create(lex_tab,prep))==NULL )
    {	global_compiler->destroy(global_compiler);
        dec_use(prep);
        return(init_error("Can't initialize lexical analyzer"));
    }
    global_compiler->out=&s_canf_lang_out;
    global_compiler->err=&s_error;
    global_compiler->param_out=conf_lang_param_out;
    global_compiler->add_table(global_compiler,conf_lang);
    global_compiler->add_table(global_compiler,language);
    global_compiler->add_table(global_compiler,value);
    global_compiler->add_table(global_compiler,expression);
    return(0);
}

int language_do( void )
{
    lex_addkeyword("NULL",T_str,0);
    global_compiler->compile(global_compiler,START);
    if( global_compiler->err->errors > 0 || global_compiler->err->warnings > 0 )
        fprintf(stderr,"Errors: %d\nWarnings: %d\n",global_compiler->err->errors,global_compiler->err->warnings);
    if( global_compiler->err->errors > 0 )
    { 	global_compiler->destroy(global_compiler);
        dec_use(prep);
        return(-1);
    }
    global_compiler->destroy(global_compiler);
    dec_use(prep);
    return(0);
}

