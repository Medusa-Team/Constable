/* backslash parser
 * from text library ver.0.1
 * (c) by Marek Zelem
 * $Id: backslash.h,v 1.3 2002/10/23 10:25:44 marek Exp $
 */

#ifndef _BACKSLASH_H
#define _BACKSLASH_H

/* bslash - pointer to \\ , if len!=NULL then returns length of sequence */
/* RETURN VALUE: ascii value */
char backslash_parse( const char *bslash, long *len );

#endif /* _BACKSLASH_H */

