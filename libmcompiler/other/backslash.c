/* backslash parser
 * from text library ver.0.1
 * (c) by Marek Zelem
 * $Id: backslash.c,v 1.2 2002/10/23 10:25:44 marek Exp $
 */

#include <stdio.h>
#include <ctype.h>
#include <mcompiler/backslash.h>

/* bslash - pointer to \\ , if len!=NULL then returns length of sequence */
/* RETURN VALUE: ascii value */
char backslash_parse( const char *bslash, long *len )
{ int x;
    const char *p;
    p=bslash; x=-1;
    if( *p=='\\' )	p++;
    if( *p=='0' )
    {	p++; x=0;
        while( *p>='0' && *p<='7' )
        {	x*=010;
            x+=(*p)-'0';
            p++;
        }
    }
    else if( *p=='x' || *p=='X' )
    {	p++; x=0;
        while( (*p>='0' && *p<='9') || ( tolower(*p)>='a' || tolower(*p)<='f') )
        {	x*=0x10;
            if( *p>='0' && *p<='9' )
                x+=(*p)-'0';
            else
                x+=0x0a+toupper(*p)-'A';
            p++;
        }
    }
    else if( *p>'0' && *p<='9' )
    {	x=0;
        while( *p>='0' && *p<='9' )
        {	x*=10;
            x+=(*p)-'0';
            p++;
        }
    }
    else
    {	switch( *p )
        { case 'a':	x='\a';	p++; break;
        case 'b':	x='\b';	p++; break;
        case 'e':	x='\e';	p++; break;
        case 'f':	x='\f';	p++; break;
        case 'n':	x='\n';	p++; break;
        case 'r':	x='\r';	p++; break;
        case 't':	x='\t';	p++; break;
        case 'v':	x='\v';	p++; break;
        case '\\':	x='\\';	p++; break;
        case 0:	x= -1; break;
        default:	x=*p; p++; break;
        }
    }
    if( x<0 )	x=' ';
    if( len!=NULL )
    {	*len= p-bslash;		}
    return((char)(x&0xff));
}

