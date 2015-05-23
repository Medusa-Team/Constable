/* Library of dynamic structures  V1.0   6.12.1997	*/
/*	copyright (c)1997 by Marek Zelem		*/ 
/*	e-mail: marek@fornax.elf.stuba.sk		*/
/* $Id: ddata.c,v 1.2 2002/10/23 10:25:44 marek Exp $	*/

#include <mcompiler/dynamic.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

ddata_t *dd_alloc( int size, int minlen )
{ ddata_t *dd;
    if( (dd=malloc(sizeof(ddata_t)))==NULL )
        return(NULL);
    if( (dd->buf=malloc(minlen*size))==NULL )
    { free(dd); return(NULL); }
    dd->size=size; dd->ml=minlen; dd->len=minlen; dd->n=0;
    return(dd);
}

int dd_free( ddata_t *dd )
{
    if( dd==NULL )
        return(2);
    if( dd->buf != NULL )
        free(dd->buf);
    free(dd);
    return(1);
}

void *dd_extract( ddata_t *dd )
{ char *buf;
    if( dd==NULL )
        return(NULL);
    buf=dd->buf;
    dd->buf=NULL;
    dd_free(dd);
    return((void*)buf);
}

int dd_append( ddata_t *dd, void *buf, int len )
{
    if( (dd->len-dd->n)<len )
    {
        dd->len= dd->n+len+dd->ml;
        if( (dd->buf=realloc(dd->buf,(dd->len*dd->size)))==NULL )
        { return(0); }
    }
    memcpy(dd->buf+((dd->n)*(dd->size)),buf,len*(dd->size));
    dd->n+=len;
    return(1);
}

ddata_t *dd_cappend( ddata_t *dd, int size, int minlen, void *buf, int len )
{
    if( dd==NULL )
    { if( (dd=dd_alloc(size,minlen))==NULL )
            return(NULL);
    }
    if( !dd_append(dd,buf,len) )
    { dd_free(dd); return(NULL); }
    return(dd);
}

int dd_insert( ddata_t *dd, int pos, void *buf, int len )
{
    if( pos > dd->n )	return(-1);
    if( (dd->len-dd->n)<len )
    {
        dd->len= dd->n+len+dd->ml;
        if( (dd->buf=realloc(dd->buf,(dd->len)*(dd->size)))==NULL )
        { return(0); }
    }
    if( pos < dd->n )
        memmove(dd->buf+((pos+len)*(dd->size)),
                dd->buf+(pos*(dd->size)),(dd->n-pos)*(dd->size));
    memcpy(dd->buf+(pos*(dd->size)),buf,len*(dd->size));
    dd->n+=len;
    return(1);
}

int dd_delete( ddata_t *dd, int pos, int len )
{
    memmove( dd->buf+((pos)*(dd->size)),dd->buf+((pos+len)*(dd->size)),
             (dd->n-pos-len)*(dd->size) );
    dd->n-=len;
    if( (dd->len-dd->n)>(dd->ml) )
    { dd->len=dd->n + dd->ml;
        if( (dd->buf=realloc(dd->buf,(dd->len)*(dd->size)))==NULL )
            return(0);
    }
    return(1);
}

int dd_putc( ddata_t *dd, char c )
{ 
    return( dd_append(dd,&c,1) );
    return(1);
}

int dd_tostring( ddata_t *dd )
{ char c;
    c=0;
    if( dd->n >0 )
    {	return( dd_append(dd,&c,1) ); }
    else
    {	free(dd->buf);	dd->buf=NULL;
    }
    return(1);
}

ddata_t *dd_printf( ddata_t *dd, char *fmt, ...)
{ va_list ap;
    int len;
    char *buf;

    len=8192;
    if( dd==NULL )
    {  if( (dd=dd_alloc(1,128))==NULL )	return(NULL);
    }
    if( dd->size != 1 )	return(dd);
    if( (dd->len-dd->n)<len )
    {
        dd->len= dd->n+len+dd->ml;
        if( (dd->buf=realloc(dd->buf,(dd->len*dd->size)))==NULL )
        { return(NULL); }
    }
    buf=dd->buf+((dd->n)*(dd->size));
    va_start(ap,fmt);
    len=vsnprintf(buf,len,fmt,ap);
    va_end(ap);
    dd->n+=len;
    if( (dd->len-dd->n)>dd->ml )
    {
        dd->len= dd->n+dd->ml;
        if( (dd->buf=realloc(dd->buf,(dd->len*dd->size)))==NULL )
        { return(NULL); }
    }
    return(dd);
}

int dd_getdd( ddata_t *dd, FILE *f, void *eol, int include_eol )
{ int r,cnt;
    cnt=0;
    while(1)
    {
        if( (dd->len-dd->n)<1 )
        {
            dd->len= dd->n+128+dd->ml;
            if( (dd->buf=realloc(dd->buf,(dd->len*dd->size)))==NULL )
            { return(-1); }
        }
        if( (r=fread(dd->buf+((dd->n)*(dd->size)),dd->size,1,f))!=1 )
            return((r<0?r:-1));
        if( eol!=NULL && !memcmp(dd->buf+((dd->n)*(dd->size)),eol,dd->size) )
        {	if( include_eol )	{dd->n+=1; cnt++;}
            return(cnt);
        }
        dd->n+=1; cnt++;
    }
}

