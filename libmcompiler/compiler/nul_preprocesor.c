/* Null Preprocessor
 * by Marek Zelem
 * (c)29.6.1999
 * $Id: nul_preprocesor.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <mcompiler/compiler.h>

typedef struct	{
	struct compiler_preprocessor_class meta;
	int fd;
	char c;
		} pre_t;

static void pre_destroy( pre_t *p )
{
	if( p->meta.filename!=NULL )	free(p->meta.filename);
	close(p->fd);
	free(p);
}

static int pre_get_char( pre_t *p, char *c )
{
	if( p->c=='\n' )
	{	p->meta.col=1; p->meta.row++;	}
	else	p->meta.col++;
	if( read(p->fd,c,1)!=1 )
	{	p->c=0;	return(-1);	}
	p->c=*c;
	return(0);
}

struct compiler_preprocessor_class *nul_preprocessor_create( char *filename )
{ pre_t *p;
	if( (p=malloc(sizeof(pre_t)))==NULL )
		return(NULL);
	p->meta.destroy=(void(*)(struct compiler_preprocessor_class*))
				pre_destroy;
	p->meta.usecount=0;
	p->meta.get_char=(int(*)(struct compiler_preprocessor_class*,char*))
				pre_get_char;
	if( filename!=NULL )
	{	if( (p->fd=open(filename,O_RDONLY))<0 )
		{	free(p); return(NULL);	}
		p->meta.filename=strdup(filename);
	}
	else
	{	p->fd=0;
		p->meta.filename=strdup("StdIN");
	}
	p->meta.col=0; p->meta.row=1;
	p->c=' ';
	return((struct compiler_preprocessor_class*)p);
}

