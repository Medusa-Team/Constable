/**
 * @file lex.c
 * @short Lexical analyser for configuration file.
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: lex.c,v 1.3 2002/10/23 10:25:43 marek Exp $
 */

#include "language.h"
#include "error.h"
#include "../access_types.h"
#include "../event.h"
#include "../tree.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

enum {
    LS_start=LS|1,
    LS_ident,
    LS_num,
    LS_arg,
    LS_string,
    LS_char,
    LS_et,
    LS_et2,
    LS_comment,
    LS_comment2,
    LS_comment_line,
};

lextab_t operators[]={
    {"{",T|'{',0},	{"}",T|'}',0},
    {",",T|',',0},	{";",T|';',0},
    {".",T|'.',0},
    {"(",T|'(',0},	{")",T|')',0},
    {"[",T|'[',0},	{"]",T|']',0},
    {"!",T|'!',0},	{"~",T|'~',0},
    {"--",Tmm,0},	{"++",Tpp,0},
    {"-",T|'-',0},	{"+",T|'+',0},
    {"*",T|'*',0},	{"/",T|'/',0},
    {"%",T|'%',0},
    {"<<",TshiftL,0},{">>",TshiftR,0},
    {"<=",Tle,0},	{">=",Tge,0},
    {"<",T|'<',0},	{">",T|'>',0},
    {"==",Teq,0},	{"!=",Tne,0},
    {"&",T|'&',0},	{"|",T|'|',0},
    {"^",T|'^',0},
    {"&&",Tand,0},	{"||",Tor,0},
    {"^^",Txor,0},
    {"?",T|'?',0},	{":",T|':',0},
    {"=",T|'=',0},		{"+=",T|'+'|_AS,0},
    {"-=",T|'-'|_AS,0},	{"*=",T|'*'|_AS,0},
    {"/=",T|'/'|_AS,0},	{"%=",T|'%'|_AS,0},
    {"&=",T|'&'|_AS,0},	{"|=",T|'|'|_AS,0},
    {"^=",T|'^'|_AS,0},
    {"<<=",TshiftL|_AS,0},{">>=",TshiftR|_AS,0},
    {"/*",LS_comment,LEX_END},
    {"//",LS_comment_line,LEX_END},
    {NULL,END,0}
};

lextab_t *keywords2=NULL;
int keywords2_nr=0;

lextab_t keywords[]={
    {"if",Tif,0},
    {"else",Telse,0},
    {"while",Twhile,0},
    {"do",Tdo,0},
    {"for",Tfor,0},
    {"break",Tbreak,0},
    {"continue",Tcontinue,0},
    {"switch",Tswitch,0},
    {"case",Tcase,0},
    {"default",Tdefault,0},
    {"return",Treturn,0},
    {"goto",Tgoto,0},

    {"function",Tfunction,0},
    //	{"force",Tforce,0},
    {"fetch",Tfetch,0},
    {"update",Tupdate,0},
    {"transparent",Ttransparent,0},
    {"local",Tlocal,0},
    {"alias",Talias,0},
    {"typeof",Ttypeof,0},
    {"commof",Tcommof,0},

    /* conf. language */
    {"tree",Ttree,0},
    {"primary",Tprimary,0},
    {"space",Tspace,0},
    {"recursive",Trecursive,0},
    //	{"object",Tobject,0},
    {"by",Tby,0},
    {"of",Tof,0},

    {"MEMBER",Taccess,AT_MEMBER},
    {"READ",Taccess,AT_RECEIVE},
    {"RECEIVE",Taccess,AT_RECEIVE},
    {"WRITE",Taccess,AT_SEND},
    {"SEND",Taccess,AT_SEND},
    {"SEE",Taccess,AT_SEE},
    {"CREATE",Taccess,AT_CREATE},
    {"ERASE",Taccess,AT_ERASE},
    {"ENTER",Taccess,AT_ENTER},
    {"CONTROL",Taccess,AT_CONTROL},

    {"FORCE_ALLOW",T_num,RESULT_FORCE_ALLOW},
    {"DENY",T_num,RESULT_DENY},
    {"FAKE_ALLOW",T_num,RESULT_FAKE_ALLOW},
    {"ALLOW",T_num,RESULT_ALLOW},

    {"VS_ALLOW",Tehhlist,EHH_VS_ALLOW},
    {"VS_DENY",Tehhlist,EHH_VS_DENY},
    {"NOTIFY_ALLOW",Tehhlist,EHH_NOTIFY_ALLOW},
    {"NOTIFY_DENY",Tehhlist,EHH_NOTIFY_DENY},

    #include "osdep.h"

    //	{"NULL",T_str,0},
    {NULL,END,0}
};

/* definicia pravidiel pre lexikalny analyzator */

static lextab_t rules_start[]= {
    {" \t\n\r",LS_start,LEX_VOID},
    {"a-zA-Z_",LS_ident,LEX_CONT},
    {"0-9",LS_num,LEX_CONT},
    {"$",LS_arg,LEX_VOID},
    {"\"",LS_string,LEX_VOID},
    {"'",LS_char,LEX_VOID},
    {"@",LS_et,LEX_VOID},
    {"#",LS_comment_line,LEX_VOID},
    {NULL,END,0}
};
static lextab_t rules_ident[]= {
    {"a-zA-Z_0-9",0,LEX_CONT},
    {"!",LS_start,LEX_END},
    {NULL,END,0}
};
static lextab_t rules_num[]= {
    {"0-9xa-fA-F",0,LEX_CONT},
    {"!",LS_start,LEX_END},
    {NULL,END,0}
};
static lextab_t rules_arg[]= {
    {"0-9",0,LEX_CONT},
    {"!",LS_start,LEX_END},
    {NULL,END,0}
};
static lextab_t rules_string[]={
    {"\"",LS_start,LEX_VOID|LEX_END},
    {"\\",0,LEX_BSLASH|LEX_CONT},
    {"!",0,LEX_CONT},
    {NULL,END,0}
};
static lextab_t rules_char[]={
    {"'",LS_start,LEX_VOID|LEX_END},
    {"\\",0,LEX_BSLASH|LEX_CONT},
    {"!",0,LEX_CONT},
    {NULL,END,0}
};
static lextab_t rules_et[]={
    {"\"",LS_et2,LEX_VOID},
    {"\\",0,LEX_BSLASH|LEX_CONT},
    {"a-zA-Z_0-9/*.?",0,LEX_CONT},
    {"!",LS_start,LEX_END},
    {NULL,END,0}
};
static lextab_t rules_et2[]={
    {"\"",LS_start,LEX_VOID|LEX_END},
    {"\\",0,LEX_BSLASH|LEX_CONT},
    {"!",0,LEX_CONT},
    {NULL,END,0}
};
static lextab_t rules_comment[]={
    {"*",LS_comment2,LEX_VOID},
    {"!",0,LEX_VOID},
    {NULL,END,0}
};
static lextab_t rules_comment2[]={
    {"*",LS_comment2,LEX_VOID},
    {"/",LS_start,LEX_VOID},
    {"!",LS_comment,LEX_VOID},
    {NULL,END,0}
};
static lextab_t rules_comment_line[]={
    {"\n",LS_start,LEX_VOID},
    {"!",0,LEX_VOID},
    {NULL,END,0}
};

static void gen_lex_ident( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want );
static void gen_lex_string( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want );
static void gen_lex_char( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want );
static void gen_lex_et( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want );
static void gen_lex_num( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want );
static void gen_lex_arg( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want );
lexstattab_t lex_tab[]={
    {LS_start,operators,rules_start,NULL,NULL},
    {LS_ident,NULL,rules_ident,keywords,gen_lex_ident},
    {LS_string,NULL,rules_string,NULL,gen_lex_string},
    {LS_char,NULL,rules_char,NULL,gen_lex_char},
    {LS_num,NULL,rules_num,NULL,gen_lex_num},
    {LS_et,NULL,rules_et,NULL,gen_lex_et},
    {LS_et2,NULL,rules_et2,NULL,gen_lex_et},
    {LS_arg,NULL,rules_arg,NULL,gen_lex_arg},
    {LS_comment,NULL,rules_comment,NULL,NULL},
    {LS_comment2,NULL,rules_comment2,NULL,NULL},
    {LS_comment_line,NULL,rules_comment_line,NULL,NULL},
    {END}
};

struct str_archive_s {
    struct str_archive_s *next;
    char str[0];
};

static struct str_archive_s *str_archive;

static char *store_string( char *s )
{ struct str_archive_s **p,*n;
    int r=1;
    for(p=&str_archive;(*p)!=NULL;p=&((*p)->next))
        if( (r=strcmp((*p)->str,s))>=0 )
            break;
    if( r==0 )
        return((*p)->str);
    if( (n=malloc(sizeof(struct str_archive_s)+strlen(s)+1))==NULL )
    {	char **errstr = (char**) pthread_getspecific(errstr_key);
        *errstr=Out_of_memory;
        return(NULL);
    }
    strcpy(n->str,s);
    n->next=(*p);
    n->next=(*p);
    (*p)=n;
    return(n->str);
}

static void gen_lex_ident( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want )
{ lextab_t *l=keywords2;
    if( l!=NULL && want!=T_id )
        while(l->keyword!=NULL)
        {	if( !strcmp(l->keyword,buf) )
            {	*sym=l->sym;
                *data=l->data;
                return;
            }
            l++;
        }
    *sym=T_id;
    if( (*data=(uintptr_t)store_string(buf))==0 )
        *sym=E|1;
}

static void gen_lex_string( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want )
{	*sym=T_str;
    if( (*data=(uintptr_t)store_string(buf))==0 )
        *sym=E|1;
}

static void gen_lex_et( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want )
{	*sym=T_path;
    if( (*data=(uintptr_t)(create_path(buf)))==0 )
        *sym=E|1;
}

static void gen_lex_char( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want )
{	*sym=T_num;
    *data=(uintptr_t)(buf[0]);
    if( buf[0]==0 || buf[1]!=0 )
    {	char **errstr = (char**) pthread_getspecific(errstr_key);
        *errstr=Out_of_memory;
        *sym=E|1;
    }
}

static void gen_lex_num( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want )
{	*sym=T_num;
    errno=0;
    *data=(uintptr_t)strtol(buf,NULL,0);
    if( errno==ERANGE )
    {	char **errstr = (char**) pthread_getspecific(errstr_key);
        *errstr=Out_of_memory;
        *sym=E|1;
    }
}

static void gen_lex_arg( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want )
{	*sym=T_arg;
    errno=0;
    *data=(uintptr_t)strtol(buf,NULL,0);
    if( errno==ERANGE )
    {	char **errstr = (char**) pthread_getspecific(errstr_key);
        *errstr=Out_of_memory;
        *sym=E|1;
    }
}

uintptr_t lex_getkeyword( char *keyword, sym_t sym )
{ lextab_t *l=keywords2;
    while( l && l->keyword!=NULL )
    {	if( l->sym==sym && !strcmp(l->keyword,keyword) )
        {
            return(l->data);
        }
        l++;
    }
    return(0);
}

int lex_updatekeyword( char *keyword, sym_t sym, uintptr_t data )
{ lextab_t *l=keywords2;
    while( l && l->keyword!=NULL )
    {	if( l->data==0 && !strcmp(l->keyword,keyword) )
        {	l->sym=sym;
            l->data=data;
            return(0);
        }
        l++;
    }
    return(-1);
}

int lex_addkeyword( char *keyword, sym_t sym, uintptr_t data )
{ lextab_t *l=keywords2;
    if( l )
    {	while(l->keyword!=NULL)
        {	if( !strcmp(l->keyword,keyword) )
                return(-1);
            l++;
        }
    }
    keywords2_nr++;
    keywords2=realloc(keywords2,(keywords2_nr+1)*sizeof(lextab_t));
    keywords2[keywords2_nr-1].keyword=keyword;
    keywords2[keywords2_nr-1].sym=sym;
    keywords2[keywords2_nr-1].data=data;
    keywords2[keywords2_nr].keyword=NULL;
    keywords2[keywords2_nr].sym=0;
    keywords2[keywords2_nr].data=0;
    return(0);
}
