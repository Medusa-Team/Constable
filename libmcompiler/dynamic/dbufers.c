/* Library of dynamic structures  V1.0   6.12.1997	*/
/*	copyright (c)1997 by Marek Zelem		*/ 
/*	e-mail: marek@fornax.elf.stuba.sk		*/
/* $Id: dbufers.c,v 1.2 2002/10/23 10:25:44 marek Exp $	*/

#include <mcompiler/dynamic.h>
#include <stdlib.h>
#include <string.h>

/* Dynamic FIFO / LIFO	buffer */

dfifo_t *dfifo_create( int size, int minlen )
{ dfifo_t *df;
    if( (df=malloc(sizeof(dfifo_t)))==NULL )
        return(NULL);
    if( (df->buf=malloc(minlen*size))==NULL )
    { free(df); return(NULL); }
    df->size=size; df->ml=minlen; df->len=minlen;
    df->n=0; df->pos=0;
    return(df);
}

int dfifo_delete( dfifo_t *df )
{
    if( df==NULL )
        return(2);
    if( df->buf != NULL )
        free(df->buf);
    free(df);
    return(1);
}

int dfifo_clear( dfifo_t *df )
{
    df->pos=0;
    df->n=0;
    return(0);
}

int dfifo_write( dfifo_t *df, void *buf, int len )
{
    if( (df->len-df->n)<len )
    {
        df->len= df->n+len+df->ml;
        if( (df->buf=realloc(df->buf,(df->len*df->size)))==NULL )
        { return(-1); }
    }
    memcpy(df->buf+((df->n)*(df->size)),buf,len*(df->size));
    df->n+=len;
    return(len);
}

int dfifo_read( dfifo_t *df, void *buf, int len )
{
    if( ((df->pos)+len) > df->n )
    {	len=(df->n)-(df->pos);	}
    if( len<=0 )	return(0);
    if( buf!=NULL )
        memcpy(buf,df->buf+((df->pos)*(df->size)),len*(df->size));
    df->pos+=len;
    if( df->pos == df->n )
    {	df->len=df->ml;
        if( (df->buf=realloc(df->buf,(df->len*df->size)))==NULL )
        {	return(-1);	}
        df->n=0; df->pos=0;
    }
    return(len);
}

int dfifo_normalize( dfifo_t *df )
{ int i;
    i=df->pos;
    if( i<0 )	return(0);
    if( i>0 )
    {	memmove(df->buf,df->buf+(i*(df->size)),(df->n-i)*(df->size));
        df->pos=0; df->n-=i;
    }
    df->len=df->n + df->ml;
    if( (df->buf=realloc(df->buf,(df->len*df->size)))==NULL )
    {	return(-1);	}
    return(df->n);
}

int dfifo_unread( dfifo_t *df, void *buf, int len )
{ int i;
    if( (df->pos) < len )
    {	i=len-(df->pos);
        if( ((df->len)-(df->n))<i )
        { df->len=df->n + i + df->ml;
            if( (df->buf=realloc(df->buf,(df->len*df->size)))==NULL )
            { return(-1); }
        }
        memmove(df->buf+(len*df->size),df->buf+(df->pos*df->size),((df->n)-(df->pos))*(df->size));
        df->pos=len;
        df->n+=i;
    }
    df->pos-=len;
    memcpy(df->buf+(df->pos*df->size),buf,len*(df->size));
    return(len);
}

int dlifo_read( dfifo_t *df, void *buf, int len )
{ int i;
    if( ((df->n)-(df->pos)) < len )
    {	len=(df->n)-(df->pos);	}
    if( len<=0 )	return(0);
    if( buf!=NULL )
    {
        for(i=0;i<len;i++)
        {	memcpy(buf+i*(df->size),df->buf+((df->n-i-1)*(df->size)),(df->size));
        }
    }
    df->n-=len;
    if( df->pos == df->n && df->pos != 0 )
    {	df->len=df->ml;
        if( (df->buf=realloc(df->buf,(df->len*df->size)))==NULL )
        {	return(-1);	}
        df->n=0; df->pos=0;
    }
    return(len);
}

int dfifo_first( dfifo_t *df, void *buf, int len )
{
    if( ((df->pos)+len) > df->n )
    {	len=(df->n)-(df->pos);	}
    if( len<=0 )	return(0);
    memcpy(buf,df->buf+((df->pos)*(df->size)),len*(df->size));
    return(len);
}

int dfifo_last( dfifo_t *df, void *buf, int len )
{ int i;
    if( ((df->n)-(df->pos)) < len )
    {	len=(df->n)-(df->pos);	}
    if( len<=0 )	return(0);
    for(i=0;i<len;i++)
    {	memcpy(buf+i*(df->size),df->buf+((df->n-i-1)*(df->size)),(df->size));
    }
    return(len);
}

int dfifo_copy( dfifo_t *in, dfifo_t *out )
{ long len;
    len=in->n-in->pos;
    if( in->size != out->size )
        len=len*in->size / out->size; /* !!! POZOR nie vzdy korektne */
    if( (out->len-out->n)<len )
    {
        out->len= out->n+len+out->ml;
        if( (out->buf=realloc(out->buf,(out->len*out->size)))==NULL )
        { return(-1); }
    }
    memcpy(out->buf+((out->n)*(out->size)),in->buf+((in->pos)*(in->size))
           ,len*(out->size));
    out->n+=len;
    return(len);
}

ddata_t *dfifo_to_ddata( dfifo_t *df )
{ int i;
    ddata_t *dd;
    if( (dd=malloc(sizeof(ddata_t)))==NULL )
        return(NULL);
    i=df->pos;
    if( i>0 )
    {	memmove(df->buf,df->buf+(i*(df->size)),(df->n-i)*(df->size));
        df->pos=0; df->n-=i;
    }
    dd->buf=df->buf;
    dd->len=df->len;
    dd->n=df->n;
    dd->size=df->size;
    dd->ml=df->ml;
    free(df);
    return(dd);
}

