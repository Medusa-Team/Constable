/**
 * @file load.c
 * @short
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: load.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include "rbac.h"
#include "../language/language.h"
#include "../constable.h"
#include "../space.h"
#include "../init.h"
#include <ctype.h>

extern compiler_class_t *global_compiler;

int load_users( char *file )
{ FILE *f;
    char s[1024],*p;
    int uid;
    if( (f=fopen(file,"r"))==NULL )
        return(init_error("Can't open user database \"%s\"\n",file));
    while( fgets(s,1024,f)!=NULL )
    {	p=s;
        if( *p=='#' || isspace(*p) )
            continue;
        while( *p!=0 && *p!=':' )
            p++;
        if( *p==0 )
            continue;
        *p++=0;
        while( *p!=0 && *p!=':' )
            p++;
        if( *p==0 )
            continue;
        p++;
        uid=atoi(p);
        rbac_user_add(s,uid);
    }
    fclose(f);
    return(0);
}

#define	R1	N|0x0010
#define	R2	N|0x0020
#define	R21	N|0x0021
#define	R22	N|0x0022
#define	R23	N|0x0023

enum {	oROLEDEF= O|0x0200,
        oACCESSTYPE,
        oADDPERM,
        oADDUSER,
        oADDSUBROLE
     };

static void rbac_out( struct compiler_out_class *o, sym_t s, unsigned long d )
{ static struct role_s *r=NULL;
    static int a=0;
    struct user_s *u;
    struct role_s *p;
    struct space_s *t;
    //  vs_t *v;
    switch( s )
    {	case TEND:
        return;
    case oROLEDEF:
        if( (r=rbac_role_add((char *)d))==NULL ) {
            char **errstr = (char**) pthread_getspecific(errstr_key);
            error("%s",*errstr);
        }
        break;
    case oACCESSTYPE:
        a=d;
        break;
    case oADDPERM:
        if( (t=space_find((char*)d))==NULL )
        {	error("Undefined space '%s'",(char*)d);
            break;
        }
        if( rbac_role_add_perm(r,a,t)<0 ) {
            char **errstr = (char**) pthread_getspecific(errstr_key);
            error("%s",*errstr);
        }
        break;
    case oADDUSER:
        if( (u=rbac_user_find((char *)d))==NULL )
        {	error("Undefined user name '%s'",(char*)d);
            break;
        }
        if( rbac_add_ua(u,r)<0 )
            error("Can't add user");
        break;
    case oADDSUBROLE:
        if( (p=rbac_role_find((char *)d))==NULL )
        {	error("Undefined role '%s'",(char*)d);
            break;
        }
        rbac_set_hierarchy(r,p);
        break;
    }
    return;
}

static void out_destroy( struct compiler_out_class *this )
{	return;
}

static struct compiler_out_class s_rbac_out={
    out_destroy,
    0,
    rbac_out
};

/*
    <rola> { <access> <space> , <space> ... ;
         \"user\" , \"user\" ... ;
         <subrola> , <subrola> ... ;
    }

 */

struct compile_tab_s rbac_tab[]={
    //	{stav,{terminaly,END},{stack,END}},
{R1,{T_id,END},{T_id,oROLEDEF,T|'{',R2,T|'}',R1,END}},
{R1,{TEND,END},{END}},
{R2,{T|'}',END},{END}},
{R2,{Taccess,END},{Taccess,oACCESSTYPE,T_id,oADDPERM,R21,T|';',R2,END}},
{R21,{T|',',END},{T|',',T_id,oADDPERM,R21,END}},
{R21,{T|';',END},{END}},
{R2,{T_str,END},{T_str,oADDUSER,R22,T|';',R2,END}},
{R22,{T|',',END},{T|',',T_str,oADDUSER,R22,END}},
{R22,{T|';',END},{END}},
{R2,{T_id,END},{T_id,oADDSUBROLE,R23,T|';',R2,END}},
{R23,{T|',',END},{T|',',T_id,oADDSUBROLE,R23,END}},
{R23,{T|';',END},{END}},
{END}
};


static int load_roles( char *file )
{ struct compiler_preprocessor_class *prep;

    if( (global_compiler=compiler_create())==NULL )
        return(init_error("RBAC: Can't initialize compiler"));
    if( (prep=nul_preprocessor_create(file))==NULL )
    {	global_compiler->destroy(global_compiler);
        return(init_error("RBAC: Can't open file %s",file));
    }
    inc_use(prep);
    if( (global_compiler->lex=lex_create(lex_tab,prep))==NULL )
    {	global_compiler->destroy(global_compiler);
        dec_use(prep);
        return(init_error("RBAC: Can't initialize lexical analyzer"));
    }
    global_compiler->out=&s_rbac_out;
    global_compiler->err=&s_error;
    global_compiler->add_table(global_compiler,rbac_tab);

    global_compiler->compile(global_compiler,R1);
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

char *rbac_conffile=NULL;

int rbac_load( struct module_s *m )
{
    rbac_conffile=m->filename;
    if( load_users("/etc/passwd")<0 )
        return(-1);
    if( load_roles(m->filename)<0 )
        return(-1);
    return(0);
}
