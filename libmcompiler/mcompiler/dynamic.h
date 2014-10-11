/* Library of dynamic structures  V1.0   6.12.1997	*/
/*	copyright (c)1997 by Marek Zelem		*/ 
/*	e-mail: marek@terminus.sk			*/
/* $Id: dynamic.h,v 1.3 2002/10/23 10:25:44 marek Exp $	*/

#ifndef _DYNAMIC_H
#define _DYNAMIC_H

#include <stdio.h>
#include <sys/types.h>

typedef struct _ll_t_ {	struct _ll_t_	*next;
			struct _ll_t_	*prev;
			int	type;
			size_t	len;
			char	data;
		      }	ll_t;

typedef struct { char *buf;
		 int len;
		 int n;
		 int size;
		 int ml;
	       }  ddata_t;

typedef struct { void *buf;
		 int len;
		 int n;
		 int size;
		 int ml;
		 int pos;
	       }  dfifo_t;


#define ll_data(ll)	((void *)(&(((ll_t *)ll)->data)))
#define	ll_next(ll)	(((ll_t *)ll)->next)
#define	ll_prev(ll)	(((ll_t *)ll)->prev)
#define	ll_len(ll)	(((ll_t *)ll)->len)
#define	ll_type(ll)	(((ll_t *)ll)->type)

#define dd_data(dd)	((void *)((dd)->buf))
#define	dd_n(dd)	((dd)->n)
#define dd_entry(dd,pos)	((void *)(((dd)->buf)+(pos)*((dd)->size)))

int lds_verlib(char *ver);

int dl_verlib(char *ver);	/* historical */

/* prototypy funkcii */

void *ll_alloc(size_t len);
void *ll_add(void *ll, size_t len);
void *ll_ins(void *ll, size_t len);
void *ll_del(void *ll);
void *ll_getfirst(void *ll);
void *ll_getlast(void *ll);
int ll_free(void *ll);

/* dynamicke pole */

ddata_t *dd_alloc( int size, int minlen );
int dd_free( ddata_t *dd );
void *dd_extract( ddata_t *dd );
int dd_append( ddata_t *dd, void *buf, int len );
ddata_t *dd_cappend( ddata_t *dd, int size, int minlen, void *buf, int len );
int dd_insert( ddata_t *dd, int pos, void *buf, int len );
int dd_delete( ddata_t *dd, int pos, int len );
int dd_putc( ddata_t *dd, char c );
int dd_tostring( ddata_t *dd );
ddata_t *dd_printf( ddata_t *dd, char *fmt, ...);
int dd_getdd( ddata_t *dd, FILE *f, void *eol, int include_eol );

/* Dynamic FIFO / LIFO  buffer */

#define dfifo_data(df)	((void*)(((char *)((df)->buf))+(((df)->pos)*((df)->size))))
#define	dfifo_n(df)	(((df)->n)-((df)->pos))
#define dfifo_entry(df,ent)	((void *)(((df)->buf)+((ent)+((df)->pos))*((df)->size)))

dfifo_t *dfifo_create( int size, int minlen );
int dfifo_delete( dfifo_t *df );
int dfifo_clear( dfifo_t *df );
int dfifo_write( dfifo_t *df, void *buf, int len );
int dfifo_read( dfifo_t *df, void *buf, int len );
int dfifo_normalize( dfifo_t *df );
int dfifo_unread( dfifo_t *df, void *buf, int len );
int dlifo_read( dfifo_t *df, void *buf, int len );
int dfifo_first( dfifo_t *df, void *buf, int len );
int dfifo_last( dfifo_t *df, void *buf, int len );
int dfifo_copy( dfifo_t *in, dfifo_t *out );

ddata_t *dfifo_to_ddata( dfifo_t *df );

/* Dynamic *argv[] */

typedef struct	{
			char	**argv;
			char	*buf;	/* if NULL then all agrs in each bufs */
			int	alen;
			int	an;
			int	blen;
			int	bn;
		} darg_t;

#define	darg_argv(darg)		((darg)->argv)
#define	darg_argc(darg)		((darg)->an)

darg_t *darg_alloc( int one_buffer );
int darg_free( darg_t *darg );
int darg_append( darg_t *darg, char *arg );
int darg_insert( darg_t *darg, int pos, char *arg );
int darg_set( darg_t *darg, int pos, char *new );
int darg_delete( darg_t *darg, int pos );

/* darg_argv2argv
 * n je maximalny pocet poloziek z argv. Ak n<0 tak ide az po NULL
 */
darg_t *darg_argv2argv( char **argv, int n );

/*
 * darg_str2argv
 * if separator is 0 then separator is all white - space  characters
 *    and multiple separators are understand as one
 * else words separate only one separator.
 */
darg_t *darg_str2argv( char *line, char separator );

darg_t *darg_line2argv( char *line );

/* darg_linevar2argv
 * skonvertuje riadok na pole argumentov darg_t
 * preklada backslash-e
 * preklada premenne pomovov getvar ak !=NULL inak nepreklada
 *	getvar vrati pointer na retazec,ktory predstavuje obsah premennej
 *		a do int* vrati pocet pouzitych znakov zo vstupneho char*
 *		alebo <0 ak je potrebne tuto funkciu este raz zavolat.
 *		pred prvym volanim musi byt *(int*)=0
 */
darg_t *darg_linevar2argv( char *line, char*(*getvar)(void*,char*,int*), void *gv_arg );

/* dynamic n-dimensional field */

struct dfield_dim_s;
typedef struct	{
	int	dimensions;
	int	unit_size;
	int(*del_func)(void*);
	struct dfield_dim_s *f;
		} dfield_t;

struct dfield_dim_s {
	dfield_t *df;
	struct dfield_dim_s **me;
	int	dim;
	int	n;
	struct dfield_dim_s *r[0];
		    };

dfield_t *dfield_create( int dim, int unit_size, int(*del_func)(void*) );
int dfield_delete_dim( struct dfield_dim_s *r );
int dfield_delete( dfield_t *df );
struct dfield_dim_s *dfield_first_dim( dfield_t *df );
struct dfield_dim_s *dfield_get( struct dfield_dim_s *r, int n );
void *dfield_get_data( struct dfield_dim_s *r, int n );
int dfield_dimsize( struct dfield_dim_s *r );
int dfield_add( struct dfield_dim_s *r );
int dfield_add_data( struct dfield_dim_s *r, void *data );

#endif /* _DYNAMIC_H */

