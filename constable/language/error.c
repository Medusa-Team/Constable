/**
 * @file error.c
 * @short Functions for error and warning reporting during compilation
 * of configuration file. 
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: error.c,v 1.3 2003/12/30 18:08:18 marek Exp $
 */

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>
#include "../constable.h"
#include "error.h"
#include "language.h"

extern compiler_class_t *global_compiler;

pthread_key_t errstr_key;
char *Out_of_memory="Out of memory";
char *Space_already_defined="Space already defined";

int error( const char *fmt, ... )
{ va_list ap;
  char buf[4096];

	sprintf(buf,"%.40s [%d,%d]: Error: ",
		global_compiler->lex->filename,
		global_compiler->lex->row,
		global_compiler->lex->col);

	va_start(ap,fmt);
	vsnprintf(buf+strlen(buf), 4000, fmt, ap);
	va_end(ap);
	sprintf(buf+strlen(buf),"\n");
	write(1,buf,strlen(buf));
	global_compiler->err->errors++;
	if( global_compiler->err->errors>15 )
		fatal("Too many errors");
	return(0);
}

int warning( const char *fmt, ... )
{ va_list ap;
  char buf[4096];

	sprintf(buf,"%.40s [%d,%d]: Warning: ",
		global_compiler->lex->filename,
		global_compiler->lex->row,
		global_compiler->lex->col);

	va_start(ap,fmt);
	vsnprintf(buf+strlen(buf), 4000, fmt, ap);
	va_end(ap);
	sprintf(buf+strlen(buf),"\n");
	write(1,buf,strlen(buf));
	global_compiler->err->warnings++;
	return(0);
}



static char *sym2str( sym_t sym )
{ lextab_t *l=keywords;
  static char buf[16];
	if( sym==TEND )	return("End of file");
	if( sym==T_id )	return("identifier");
	if( sym==T_str )	return("string");
	if( sym==T_num )	return("constant");
	if( sym==Taccess )	return("access type");
	while(l->keyword!=NULL)
	{	if( l->sym==sym )
			return(l->keyword);
		l++;
	}
	l=keywords2;
	while(l->keyword!=NULL)
	{	if( l->sym==sym )
			return(l->keyword);
		l++;
	}
	l=operators;
	while(l->keyword!=NULL)
	{	if( l->sym==sym )
			return(l->keyword);
		l++;
	}
	sprintf(buf,"?%04x?",sym);
	return(buf);
}
static sym_t err_warning( struct compiler_err_class *this, sym_t errsym, sym_t info )
{
	if( errsym==TEND )	return(0);
	if( errsym==END && (info&TYP)==E )
		warning("Lexical warning %04x",info&~TYP);
	else if( (errsym&TYP)==W && info==END )
		warning("%04x",errsym&~TYP);
	else	warning("%04x %04x",errsym,info);
	return(0);
}
static sym_t err_error( struct compiler_err_class *this, sym_t errsym, sym_t info )
{
	if( errsym==TEND )	return(0);
	if( errsym==END && info==eLEXERR )
	{	error("Lexical error");
		return(-1);
	}
	if( errsym==END && info==ePREERR )
	{	error("Preprocessor error");
		return(-1);
	}
	else if( errsym==END && (info&TYP)==E )
	{	error("Lexical error %04x",info&~TYP);
		return(-1);
	}
	else if( errsym==END && ((info&TYP)==T || info==TEND) )
		error("Unexpected %s",sym2str(info));
	else if( (errsym&TYP)==T && ((info&TYP)==T || info==TEND) )
		error("Missing %s",sym2str(errsym));
	else if( errsym==(E|1) ) {
        char **errstr = (char**) pthread_getspecific(errstr_key);
		error("%s",*errstr);
    }
	else if( errsym==(E|0x0d01) )
		error("Undefined function");
	else if( errsym==(SEC_START|3) && info==END )
		error("Incorrect use of continue");
	else if( errsym==(SEC_START|2) && info==END )
		error("Incorrect use of break");
	else if( (errsym&TYP)==E && info==END )
		error("%04x",errsym&~TYP);
	else	error("%04x %04x",errsym,info);
	if( global_compiler->err->errors>20 )
	{	error("Too many errors");
		return(-1);
	}
	return(0);
}

static void err_destroy( struct compiler_err_class *this )
{	return;
}

struct compiler_err_class s_error={
		err_destroy,
		0,
		err_warning,err_error,0,0
};

