/**
 * @file conf_lang.c
 * @short Grammar and functions for execute main constable configuration file
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: conf_lang.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include "../constable.h"
#include "../comm.h"
#include "../init.h"
#include "mcp.h"
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <mcompiler/compiler.h>
#include <mcompiler/lex.h>

extern char *medusa_config_file;

enum {
	START = N|0x0000,
	S1,
	S1m,
	S1n,
	S1p,
	SM,
};

enum {
	T_id = T|100,
	T_num,
	T_ip,
	T_str,
	Ttcp,		/* tcp */
	Tfile,		/* file */
	Tmodule,	/* module */
	Tchdir,		/* chdir */
	Tconfig,	/* config */
	Tsystem,	/* system */
};

enum {
	Pname = PO|0x0000,
	Pcommfile,
	Pcommbind,
	Pbind,
	Pip,
	Pmask,
	Pmasknum,
	Pmaskfull,
	Pport,
	Pport0,
	Pchdir,
	Pconfig,
	Pmodule,
	Pmodfile,
	Psystem,
};

/*
	chdir "dir";

	system "command";

	config "medusa.conf";
	
	"name" file "filename";

	"name" tcp:<port> <ip>[/<mask>][:<port>];

	module "name" [file "filename"];

 */

struct compile_tab_s mcp_conf_lang[]={
//	{stav,{terminaly,END},{stack,END}},
	{START,{Tchdir,END},{Tchdir,T_str,Pchdir,T|';',START,END}},
	{START,{Tsystem,END},{Tsystem,T_str,Psystem,T|';',START,END}},
	{START,{Tmodule,END},{Tmodule,T_str,Pmodule,SM,T|';',START,END}},
	{SM,{Tfile,END},{Tfile,T_str,Pmodfile,END}},
	{SM,{END},{END}},
	{START,{Tconfig,END},{Tconfig,T_str,Pconfig,T|';',START,END}},
	{START,{T_str,END},{T_str,Pname,S1,T|';',START,END}},
	{START,{TEND,END},{END}},
	{S1,{Tfile,END},{Tfile,T_str,Pcommfile,END}},
	{S1,{Ttcp,END},{Ttcp,T|':',T_num,Pbind,T_ip,Pip,S1m,S1p,Pcommbind,END}},
	{S1m,{T|'/',END},{T|'/',S1n,END}},
	{S1n,{T_ip,END},{T_ip,Pmask,END}},
	{S1n,{T_num,END},{T_num,Pmasknum,END}},
	{S1m,{END},{Pmaskfull,END}},
	{S1p,{T|':',END},{T|':',T_num,Pport,END}},
	{S1p,{END},{Pport0,END}},
	{END}
};


enum {
	LS_start=LS|1,
	LS_ident,
	LS_numip,
	LS_num,
	LS_ip,
	LS_str,
	LS_comment,
	LS_comment2,
	LS_comment_line,
};

static lextab_t operators[]={
	{":",T|':',0},
	{"/",T|'/',0},
	{";",T|';',0},
	{"/*",LS_comment,LEX_END},
	{"//",LS_comment_line,LEX_END},
	{NULL,END,0}
};

static lextab_t keywords[]={
	{"file",Tfile,0},
	{"module",Tmodule,0},
	{"tcp",Ttcp,0},
	{"chdir",Tchdir,0},
	{"config",Tconfig,0},
	{"system",Tsystem,0},
	{NULL,END,0},
};

static lextab_t rules_start[]= {
	{" \t\n\r",LS_start,LEX_VOID},
	{"a-zA-Z_",LS_ident,LEX_CONT},
	{"0-9",LS_numip,LEX_CONT},
	{"\"",LS_str,LEX_VOID},
	{"#",LS_comment_line,LEX_VOID},
	{NULL,END,0}
};
static lextab_t rules_ident[]= {
	{"a-zA-Z_0-9",0,LEX_CONT},
	{"!",LS_start,LEX_END},
	{NULL,END,0}
};
static lextab_t rules_numip[]= {
	{"0-9",0,LEX_CONT},
	{"xa-fA-F",LS_num,LEX_CONT},
	{".",LS_ip,LEX_CONT},
	{"!",LS_start,LEX_END},
	{NULL,END,0}
};
static lextab_t rules_num[]= {
	{"0-9xa-fA-F",0,LEX_CONT},
	{"!",LS_start,LEX_END},
	{NULL,END,0}
};
static lextab_t rules_ip[]= {
	{"0-9.",0,LEX_CONT},
	{"!",LS_start,LEX_END},
	{NULL,END,0}
};
static lextab_t rules_str[]={
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
static void gen_lex_str( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want );
static void gen_lex_num( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want );
static void gen_lex_ip( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want );

static lexstattab_t mcp_lex_tab[]={
	{LS_start,operators,rules_start,NULL,NULL},
	{LS_ident,NULL,rules_ident,keywords,gen_lex_ident},
	{LS_str,NULL,rules_str,NULL,gen_lex_str},
	{LS_numip,NULL,rules_numip,NULL,gen_lex_num},
	{LS_num,NULL,rules_num,NULL,gen_lex_num},
	{LS_ip,NULL,rules_ip,NULL,gen_lex_ip},
	{LS_comment,NULL,rules_comment,NULL,NULL},
	{LS_comment2,NULL,rules_comment2,NULL,NULL},
	{LS_comment_line,NULL,rules_comment_line,NULL,NULL},
	{END}
};

static int mcp_error( const char *fmt, ... );
static int mcp_warning( const char *fmt, ... );

static void gen_lex_ident( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want )
{	*sym=T_id; *data=(uintptr_t)strdup(buf);	}
static void gen_lex_str( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want )
{	*sym=T_str; *data=(uintptr_t)strdup(buf);	}
static void gen_lex_num( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want )
{	*sym=T_num; *data=(uintptr_t)strtol(buf,NULL,0);
	if( errno==ERANGE ) { mcp_error("Constant out of range"); }
}
static void gen_lex_ip( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want )
{	*sym=T_ip;
	*data=(uintptr_t)inet_addr(buf);
}

static compiler_class_t *mcp_compiler;

static int mcp_error( const char *fmt, ... )
{ va_list ap;
  char buf[2048];
	sprintf(buf,"%.40s [%d,%d]: Error: ",
		mcp_compiler->lex->filename,
		mcp_compiler->lex->row,
		mcp_compiler->lex->col);
	va_start(ap,fmt);
	vsnprintf(buf+strlen(buf), 1000, fmt, ap);
	va_end(ap);
	sprintf(buf+strlen(buf),"\n");
	write(1,buf,strlen(buf));
	mcp_compiler->err->errors++;
	return(0);
}
static int mcp_warning( const char *fmt, ... )
{ va_list ap;
  char buf[2048];
	sprintf(buf,"%.40s [%d,%d]: Warning: ",
		mcp_compiler->lex->filename,
		mcp_compiler->lex->row,
		mcp_compiler->lex->col);
	va_start(ap,fmt);
	vsnprintf(buf+strlen(buf), 1000, fmt, ap);
	va_end(ap);
	sprintf(buf+strlen(buf),"\n");
	write(1,buf,strlen(buf));
	mcp_compiler->err->warnings++;
	return(0);
}

static char *sym2str( sym_t sym )
{ lextab_t *l=keywords;
  static char buf[16];
	if( sym==TEND )		return("End of file");
	if( sym==T_id )		return("identifier");
	if( sym==T_str )	return("string");
	if( sym==T_num )	return("number");
	if( sym==T_ip )		return("IP adress");
	while(l->keyword!=NULL)
	{	if( l->sym==sym )	return(l->keyword);
		l++;
	}
	l=operators;
	while(l->keyword!=NULL)
	{	if( l->sym==sym )	return(l->keyword);
		l++;
	}
	sprintf(buf,"?%04x?",sym);
	return(buf);
}
static sym_t err_warning( struct compiler_err_class *this, sym_t errsym, sym_t info )
{
	if( errsym==TEND )	return(0);
	if( errsym==END && (info&TYP)==E )
		mcp_warning("Lexical warning %04x",info&~TYP);
	else if( (errsym&TYP)==W && info==END )
		mcp_warning("%04x",errsym&~TYP);
	else	mcp_warning("%04x %04x",errsym,info);
	return(0);
}
static sym_t err_error( struct compiler_err_class *this, sym_t errsym, sym_t info )
{
	if( errsym==TEND )	return(0);
	if( errsym==END && info==eLEXERR )
		mcp_error("Lexical error");
	else if( errsym==END && (info&TYP)==E )
		mcp_error("Lexical error %04x",info&~TYP);
	else if( errsym==END && ((info&TYP)==T || info==TEND) ) 
		mcp_error("Unexpected %s",sym2str(info));
	else if( (errsym&TYP)==T && ((info&TYP)==T || info==TEND) )
		mcp_error("Missing %s",sym2str(errsym));
	else if( errsym==(E|1) )
		mcp_error("%s",errstr);
	else if( (errsym&TYP)==E && info==END )
		mcp_error("%04x",errsym&~TYP);
	else	mcp_error("%04x %04x",errsym,info);
	return(0);
}
static void err_destroy( struct compiler_err_class *this )
{	return;
}
struct compiler_err_class mcp_s_error={
	err_destroy,
	0,
	err_warning,err_error,0,0
};

static void canf_lang_out( struct compiler_out_class *o, sym_t s, unsigned long d )
{	return;
}

static void out_destroy( struct compiler_out_class *this )
{	return;
}

struct compiler_out_class mcp_s_canf_lang_out={
	out_destroy,
	0,
	canf_lang_out
};

static void mcp_conf_lang_param_out( struct compiler_class *c, sym_t s )
{ static char *name;
  static struct comm_s *c_listen;
  static in_addr_t ip;
  static in_addr_t mask;
  static in_port_t port;
  static struct module_s *module;
  struct comm_s *comm;
	switch( s )
	{
		case Pname:	name=strdup((char*)(c->l.data));
				break;
		case Pcommfile:
				if( (comm=mcp_alloc_comm(name))==NULL )
				{	error("Out of memory");
					free(name); name=NULL;
					break;
				}
				free(name); name=NULL;
				mcp_open(comm,(char*)(c->l.data));
				break;
		case Pcommbind:
				if( (comm=mcp_alloc_comm(name))==NULL )
				{	error("Out of memory");
					free(name); name=NULL;
					break;
				}
				free(name); name=NULL;
				mcp_to_accept(comm,c_listen,ip,mask,port);
				break;
		case Pbind:
				c_listen=mcp_listen(c->l.data);
				break;
		case Pip:
				ip=c->l.data;
				break;
		case Pmask:
				mask=c->l.data;
				break;
		case Pmaskfull:
				c->l.data=32;
		case Pmasknum:
				if( c->l.data < 32 )
					mask= ((1<<(c->l.data))-1)<<(32-(c->l.data));
				else	mask= ~0;
				break;
		case Pport0:
				c->l.data=0;
		case Pport:
				port=(in_port_t)(c->l.data);
				break;

		case Pchdir:
				if( chdir((char*)(c->l.data))<0 )
					mcp_error("%s: %s",(char*)(c->l.data),strerror(errno));
				break;
		case Psystem:
				if( system((char*)(c->l.data))<0 )
					mcp_error("%s: %s",(char*)(c->l.data),strerror(errno));
				break;
		case Pconfig:
				medusa_config_file=strdup((char*)(c->l.data));
				break;
		case Pmodule:
				if( (module=activate_module((char*)(c->l.data)))==NULL )
					mcp_error("There is no %s module",(char*)(c->l.data));
				break;
		case Pmodfile:	if( module!=NULL )
					module->filename=strdup((char*)(c->l.data));
				break;
		case TEND:	break;
		default:
				mcp_error("mcp language error");
				break;
	}
}

int mcp_language_do( char *filename )
{
  struct compiler_preprocessor_class *mcp_prep;
	if( (mcp_compiler=compiler_create())==NULL )
		return(init_error("mcp: Can't initialize compiler"));
	if( (mcp_prep=nul_preprocessor_create(filename))==NULL )
	{	mcp_compiler->destroy(mcp_compiler);
		return(init_error("mcp: Can't open file %s",filename));
	}
	inc_use(mcp_prep);
	if( (mcp_compiler->lex=lex_create(mcp_lex_tab,mcp_prep))==NULL )
	{	mcp_compiler->destroy(mcp_compiler);
		dec_use(mcp_prep);
		return(init_error("mcp: Can't initialize lexical analyzer"));
	}
	mcp_compiler->out=&mcp_s_canf_lang_out;
	mcp_compiler->err=&mcp_s_error;
	mcp_compiler->param_out=mcp_conf_lang_param_out;
	mcp_compiler->add_table(mcp_compiler,mcp_conf_lang);

	mcp_compiler->compile(mcp_compiler,START);
	if( mcp_compiler->err->errors > 0 || mcp_compiler->err->warnings > 0 )
		fprintf(stderr,"Errors: %d\nWarnings: %d\n",mcp_compiler->err->errors,mcp_compiler->err->warnings);
	if( mcp_compiler->err->errors > 0 )
	{ 	mcp_compiler->destroy(mcp_compiler);
		dec_use(mcp_prep);
		return(-1);
	}
	mcp_compiler->destroy(mcp_compiler);
	dec_use(mcp_prep);
	return(0);
}

