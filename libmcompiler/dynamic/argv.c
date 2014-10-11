/* Library of dynamic structures  V1.0   11.2.1998	*/
/*	copyright (c)1998 by Marek Zelem		*/ 
/*	e-mail: marek@fornax.elf.stuba.sk		*/
/* $Id: argv.c,v 1.3 2002/10/23 10:25:43 marek Exp $	*/

#include <stdlib.h>
#include <string.h>
#include <mcompiler/dynamic.h>

int _darg_resizebuf( darg_t *darg, int addlen )
{ char *p,*pe;
  long d;
	p=darg->buf;	pe=p+darg->blen;
	darg->blen+=addlen;
	if( (darg->buf=realloc(p,(darg->blen)*sizeof(char)))
		==NULL )
	{	darg->buf=p;
		darg->blen-=addlen;
		return(0);
	}
	d=(darg->buf)-p;
	if( d!=0 )
	{ int i;
		for(i=0;i<darg->an;i++)
		  if( darg->argv[i]>=p && darg->argv[i]<pe )
			  darg->argv[i]+=d;
	}
	return(addlen);
}

int _darg_resizeargv( darg_t *darg, int addlen )
{ char **z;
	   darg->alen+=addlen;
	   if((darg->argv=realloc((z=darg->argv),(darg->alen)*sizeof(char*)))==NULL)
	   {	darg->argv=z;
	   	darg->alen-=addlen;
		return(0);
	   }
	   return(addlen);
}

/* ----------------------------------------------------------------------- */

darg_t *darg_alloc( int one_buffer )
{ darg_t *darg;
	if( (darg=malloc(sizeof(darg_t)))==NULL )	return(NULL);
	darg->alen=32;
	if( (darg->argv=malloc((darg->alen)*sizeof(char*)))==NULL )
	{	free(darg);	return(NULL);	}
	if( one_buffer )
	{	darg->blen=128;
		if( (darg->buf=malloc((darg->blen)*sizeof(char)))==NULL )
		{   free(darg->argv);	free(darg);	return(NULL);	}
	}
	else
	{	darg->buf=NULL;
		darg->blen= -1;
	}
	darg->an=0;
	darg->bn=0;
	darg->argv[darg->an]=NULL;
	return(darg);
}

int darg_free( darg_t *darg )
{ int i;
	if( darg!=NULL )
	{	if( darg->buf!=NULL )
		{	free( darg->buf );
			if( darg->argv!=NULL )	free( darg->argv );
		}
		else if( darg->argv!=NULL )
		{	i=0;
			while( i < darg->an )
				if( darg->argv[i]!=NULL )
					free(darg->argv[i++]);
			free( darg->argv );
		}
		free( darg );
	}
	return(0);
}

int darg_append( darg_t *darg, char *arg )
{ int len;
	if( darg->argv==NULL )	darg->an=darg->alen=0;
	if( (darg->an)+1 >= darg->alen )
		if( _darg_resizeargv(darg,32)!=32 )
			return(0);
	if( arg!=NULL )		len=strlen(arg)+1;
	else			len=0;
	if( arg==NULL )	
		darg->argv[darg->an]=NULL;
	else if( darg->buf==NULL )
	{	if( (darg->argv[darg->an]=malloc(len*sizeof(char)))==NULL )
			return(0);
	}
	else
	{	if( (darg->bn)+len > darg->blen )
			if( _darg_resizebuf(darg,128)!=128 )
				return(0);
		darg->argv[darg->an]=(darg->buf)+(darg->bn);
		darg->bn+=len;
	}
	if( arg!=NULL )	memcpy(darg->argv[darg->an],arg,len);
	darg->an++;
	darg->argv[darg->an]=NULL;
	return(1);
}

int darg_insert( darg_t *darg, int pos, char *arg )
{ int i,len;
  char *a;
	if( pos < 0 || pos > darg->an )
		return(-1);
	if( darg->argv==NULL )	darg->an=darg->alen=0;
	if( (darg->an)+1 >= darg->alen )
		if( _darg_resizeargv(darg,32)!=32 )
			return(0);
	if( arg!=NULL )		len=strlen(arg)+1;
	else			len=0;
	if( arg==NULL )	
		a=NULL;
	else if( darg->buf==NULL )
	{	if( (a=malloc(len*sizeof(char)))==NULL )
			return(0);
	}
	else
	{	if( (darg->bn)+len > darg->blen )
			if( _darg_resizebuf(darg,128)!=128 )
				return(0);
		a=(darg->buf)+(darg->bn);
		darg->bn+=len;
	}
	if( arg!=NULL )	memcpy(a,arg,len);
	for(i=darg->an;i>=pos;i--)
		darg->argv[i+1]=darg->argv[i];
	darg->argv[pos]=a;
	darg->an++;
	return(1);
}

int darg_set( darg_t *darg, int pos, char *new )
{
	if( pos < 0 || pos >= darg->an )
		return(-1);
	if( darg->argv==NULL )	return(-1);
	if( darg->buf==NULL )
	{	if( darg->argv[pos]!=NULL )
			free(darg->argv[pos]);
	}
	else
	{	/* !!! asi by bolo dobre zmazat to co nepotrebujem !!! */
	}
	darg->argv[pos]=new;
	return(1);
}

int darg_delete( darg_t *darg, int pos )
{ int i;
	if( pos < 0 || pos >= darg->an )
		return(-1);
	if( darg->argv==NULL )	return(1);
	if( darg_set(darg,pos,NULL)<=0 )	return(0);
	for(i=pos;i<darg->an;i++)
		darg->argv[i]=darg->argv[i+1];
	darg->an--;
	if( (darg->alen)-(darg->an) > 32 )
	_darg_resizeargv(darg,-32);
	return(1);
}

