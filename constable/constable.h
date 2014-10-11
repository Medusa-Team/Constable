/*
 * Constable: constable.h
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: constable.h,v 1.3 2002/10/23 10:25:43 marek Exp $
 */

#ifndef _CONSTABLE_H
#define _CONSTABLE_H

#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "language/error.h"

//#define MAX(a,b)	((a)>(b)?(a):(b))
//#define MIN(a,b)	((a)<(b)?(a):(b))

extern void(*debug_def_out)( int arg, char *str ); 
extern int debug_def_arg;
extern void(*debug_do_out)( int arg, char *str );
extern int debug_do_arg;

int medusa_printlog(char *format, ...);

char *retprintf( const char *fmt, ... );
int init_error( const char *fmt, ... );
#ifdef DEBUG_TRACE
extern char runtime_file[64];
extern char runtime_pos[12];
#endif
int runtime( const char *fmt, ... );
int fatal( const char *fmt, ... );

#endif /* _CONSTABLE_H */
