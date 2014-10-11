/* dynamic n-dimensional field
 * (c)16.10.1999 by Marek Zelem
 * $Id: dfield.c,v 1.3 2002/10/23 10:25:44 marek Exp $
 */

#include <stdlib.h>
#include <string.h>
#include <mcompiler/dynamic.h>

static struct dfield_dim_s *dfield_new_dim( struct dfield_dim_s *p, struct dfield_dim_s **me )
{ struct dfield_dim_s *n;
	if( (n=malloc(sizeof(struct dfield_dim_s)))==NULL )
		return(NULL);
	if( p!=NULL )
	{	n->df=p->df;
		n->dim=p->dim-1;
	}
	n->me=me;
	n->n=0;
	return(n);
}

dfield_t *dfield_create( int dim, int unit_size, int(*del_func)(void*) )
{ dfield_t *df;
	if( (df=malloc(sizeof(dfield_t)))==NULL )
		return(NULL);
	df->dimensions=dim;
	df->unit_size=unit_size;
	df->del_func=del_func;
	df->f=dfield_new_dim(NULL,&(df->f));
	if( df->f==NULL )
	{	free(df);
		return(NULL);
	}
	df->f->df=df;
	df->f->dim=df->dimensions;
	return(df);
}

int dfield_delete_dim( struct dfield_dim_s *r )
{ int i;
	if( r==NULL || r->dim<=0 )	return(0);
	if( r->dim>1 )
	{	for(i=0;i<r->n;i++)
			dfield_delete_dim(r->r[i]);
	}
	else if( r->df->del_func!=NULL )
	{ char *p=(char*)(&(r->r[0]));
		for(i=0;i<r->n;i++)
			r->df->del_func(p+i*r->df->unit_size);
	}
	*(r->me)=NULL;
	free(r);
	return(0);
}

int dfield_delete( dfield_t *df )
{
	if( df==NULL )	return(0);
	dfield_delete_dim(df->f);
	free(df);
	return(0);
}

struct dfield_dim_s *dfield_first_dim( dfield_t *df )
{
	if( df->f==NULL )
	{	df->f=dfield_new_dim(NULL,&(df->f));
		if( df->f==NULL )	return(NULL);
		df->f->df=df;
		df->f->dim=df->dimensions;
	}
	return(df->f);
}

struct dfield_dim_s *dfield_get( struct dfield_dim_s *r, int n )
{
	if( n<0 )	n+=r->n;
	if( r==NULL || r->n<n || n<0 || r->dim<=1 )
		return(NULL);
	if( r->r[n]==NULL )
		r->r[n]=dfield_new_dim(r,&(r->r[n]));
	return(r->r[n]);
}

void *dfield_get_data( struct dfield_dim_s *r, int n )
{
	if( n<0 )	n+=r->n;
	if( r==NULL || r->n<n || n<0 || r->dim!=1 )
		return(NULL);
	return( ((char*)(&(r->r[0])))+n*r->df->unit_size );
}

int dfield_dimsize( struct dfield_dim_s *r )
{	if( r==NULL )	return(0);
	return(r->n);
}

int dfield_add( struct dfield_dim_s *r )
{ struct dfield_dim_s *old;
  int i;
	if( r==NULL || r->dim<=1 )
		return(-1);
	old=r;
	if( (r=realloc(r,sizeof(struct dfield_dim_s)+
			(r->n+1)*sizeof(void*)))==NULL )
		return(-1);
	r->r[r->n]=dfield_new_dim(r,&(r->r[r->n]));
	r->n++;
	if( r!=old )
	{	*(r->me)=r;
		for(i=0;i<r->n;i++)
		{	if( r->r[i]!=NULL )
				r->r[i]->me=&(r->r[i]);
		}
	}
	return(r->n-1);
}

int dfield_add_data( struct dfield_dim_s *r, void *data )
{ struct dfield_dim_s *old;
	if( r==NULL || r->dim!=1 )
		return(-1);
	old=r;
	if( (r=realloc(r,sizeof(struct dfield_dim_s)+
			(r->n+1)*r->df->unit_size))==NULL )
		return(-1);
	memcpy( ((char*)(&(r->r[0])))+r->n*r->df->unit_size,
	     	data,r->df->unit_size );
	r->n++;
	if( r!=old )
		*(r->me)=r;
	return(r->n-1);
}

