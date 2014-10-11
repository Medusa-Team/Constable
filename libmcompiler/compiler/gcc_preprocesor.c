/* Gcc Preprocessor
 * by Marek Zelem
 * (c)9.7.2000
 * $Id: gcc_preprocesor.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <mcompiler/compiler.h>

typedef struct	{
	struct compiler_preprocessor_class meta;
	char *filename;
	int fd;
	pid_t pid;
	char c;
		} pre_t;

static void pre_destroy( pre_t *p )
{
	if( p->filename!=NULL )		free(p->filename);
	if( p->meta.filename!=NULL )	free(p->meta.filename);
	close(p->fd);
	if( p->pid>0 )
		waitpid(p->pid,NULL,0);
	free(p);
}

static int pre_get_char( pre_t *p, char *c )
{ int bp;
  char *b,*a,buf[128];
	if( p->c=='\n' )
	{	p->meta.col=1; p->meta.row++;	}
	else	p->meta.col++;
	if( read(p->fd,c,1)!=1 )
	{	p->c=0;	return(-1);	}
	if( p->meta.col==1 && *c=='#' )
	{	bp=0; buf[bp]=*c;
		while( buf[bp]!='\n' )
		{	if( bp<126 )	bp++;
			if( read(p->fd,buf+bp,1)!=1 )
			{	p->c=0;	return(-1);	}
		}
		buf[bp+1]=0;
		if( buf[1]==' ' )
		{	p->meta.row=atoi(buf+2);
			b=buf+2;
			while( *b!=0 && *b!=' ' )	b++;
			while( *b==' ' )		b++;
			if( p->meta.filename!=NULL )	free(p->meta.filename);
			if( b[0]=='"' && b[1]!='"' )
			{	b++; a=b;
				while( *a!=0 && *a!='"' )	a++;
				*a=0;
				p->meta.filename=strdup(b);
			}
			else	p->meta.filename=strdup(p->filename);
			p->meta.col=0;
			p->c=' ';
			return(pre_get_char(p,c));
		}
		else	*c='\n';
	}
	p->c=*c;
	return(0);
}

struct compiler_preprocessor_class *gcc_preprocessor_create( char *filename )
{ pre_t *p;
  int fd,s[2];
	if( (p=malloc(sizeof(pre_t)))==NULL )
		return(NULL);
	p->meta.destroy=(void(*)(struct compiler_preprocessor_class*))
				pre_destroy;
	p->meta.usecount=0;
	p->meta.get_char=(int(*)(struct compiler_preprocessor_class*,char*))
				pre_get_char;
	if( filename!=NULL )
	{	if( (fd=open(filename,O_RDONLY))<0 )
		{	free(p); return(NULL);	}
		p->filename=strdup(filename);
		p->meta.filename=strdup(filename);
	}
	else
	{	fd=0;
		p->filename=strdup("StdIN");
		p->meta.filename=strdup("StdIN");
	}
	p->fd= -1;
	p->pid= 0;
	if( (pipe(s))<0 )
	{	pre_destroy(p);
		close(fd);
		return(NULL);
	}	
	if( (p->pid=fork())==0 )
	{	close(s[0]);
		close(0);	dup(fd);	close(fd);
		close(1);	dup(s[1]);	close(s[1]);
		execlp("gcc","gcc","-E","-",NULL);
		exit(1);
	}
	close(s[1]);
	if( p->pid < 0 )
	{	pre_destroy(p);
		close(s[0]);
		close(fd);
		return(NULL);
	}
	p->fd=s[0];
	p->meta.col=0; p->meta.row=1;
	p->c=' ';
	return((struct compiler_preprocessor_class*)p);
}

