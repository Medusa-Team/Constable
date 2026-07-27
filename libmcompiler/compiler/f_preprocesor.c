/* File Preprocessor
 * by Marek Zelem
 * (c)5.7.2002
 * $Id: f_preprocesor.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <mcompiler/compiler.h>
#include <mcompiler/dynamic.h>

#include <mcompiler/checked_math.h>

struct pre_file_s {
    struct pre_file_s *prev;
    char *filename;
    int fd;
    int col,row;
    int buf_len;
    int buf_pos;
    char last_char;
    char buf[4096];
};

typedef struct	{
    struct compiler_preprocessor_class meta;
    struct pre_file_s *file;
    dfifo_t *fifo;
} pre_t;

static int open_file( pre_t *p, char *filename )
{ struct pre_file_s *f;
    size_t filename_length;
    size_t allocation;
    if( p==NULL || filename==NULL )
        return(-1);
    filename_length=strlen(filename);
    if( !checked_size_add(sizeof(struct pre_file_s),filename_length,
                          &allocation) ||
        !checked_size_add(allocation,1,&allocation) ||
        (f=malloc(allocation))==NULL )
        return(-1);
    f->filename=(char*)(f+1);
    memcpy(f->filename,filename,filename_length+1);
    if( strcmp(f->filename,"-") )
    {	if( (f->fd=open(f->filename,O_RDONLY))<0 )
        {	free(f);
            return(-1);
        }
    }
    else	f->fd=0;
    if( p->file!=NULL )
    {	p->file->col=p->meta.col;
        p->file->row=p->meta.row;
    }
    p->meta.col=0; p->meta.row=0;
    f->last_char='\n';
    f->buf_len=0;
    f->buf_pos=0;
    f->prev=p->file;
    p->file=f;
    p->meta.filename=p->file->filename;
    return(0);
}

static int open_file_relative( pre_t *p, char *filename )
{ char *buf;
    char *l;
    size_t prefix_length;
    size_t filename_length;
    size_t allocation;
    int result;
    if( p==NULL || p->file==NULL || filename==NULL )
        return(-1);
    if( filename[0]=='/' )
        return(open_file(p,filename));
    if( (l=strrchr(p->file->filename,'/'))==NULL )
        return(open_file(p,filename));
    prefix_length=(size_t)(l-p->file->filename)+1;
    filename_length=strlen(filename);
    if( !checked_size_add(prefix_length,filename_length,&allocation) ||
        !checked_size_add(allocation,1,&allocation) )
        return(-1);
    buf=malloc(allocation);
    if( buf==NULL )
        return(-1);
    memcpy(buf,p->file->filename,prefix_length);
    memcpy(buf+prefix_length,filename,filename_length+1);
    result=open_file(p,buf);
    free(buf);
    return(result);
}

static int close_file( pre_t *p )
{ struct pre_file_s *f;
    f=p->file;
    if( (p->file=f->prev)!=NULL )
    {	p->meta.col=p->file->col;
        p->meta.row=p->file->row;
        p->meta.filename=p->file->filename;
    }
    else
    {	p->meta.col= 0;
        p->meta.row= 0;
        p->meta.filename= "EOF";
    }
    close(f->fd);
    free(f);
    return(0);
}

static void pre_destroy( pre_t *p )
{
    while( p->file!=NULL )
        close_file(p);
    dfifo_delete(p->fifo);
    free(p);
}

static int get_char( pre_t *p )
{
Retry:
    if( p->file==NULL )
        return(-1);
    if( p->file->buf_pos >= p->file->buf_len )
    {	if( (p->file->buf_len=read(p->file->fd,p->file->buf,sizeof(p->file->buf)))<=0 )
        {	close_file(p);
            goto Retry;
        }
        p->file->buf_pos=0;
    }
    if( p->file->last_char=='\n' )
    {	p->meta.col=1; p->meta.row++;	}
    else	p->meta.col++;
    return( (p->file->last_char=p->file->buf[p->file->buf_pos++]) );
}

#define	GET_WORD	do {						\
    i=0;								\
    while( (buf[i]=get_char(p))==' ' || buf[i]=='\t' );		\
    if( buf[i]=='\n' )						\
        goto Err;							\
    i++;								\
    for(;;) {							\
        if( (z=get_char(p))<0 )					\
            goto Err;						\
        if( isspace((buf[i++]=(char)z)) )			\
            break;							\
    }									\
    x=buf[i-1];							\
    buf[i-1]=0;							\
    } while(0)
static int get_cmd( pre_t *p )
{ char buf[4096];
    int z;
    char x;
    int i;
    GET_WORD;
    if( !strcmp(buf,"include") )
    {	if( x=='\n' )
            goto Err;
        GET_WORD;
        if( buf[0]=='"' && buf[i-2]=='"' && x=='\n' )
        {	buf[i-2]=0;
            if( open_file_relative(p,buf+1)<0 )
                goto Err;
            return(0);
        }
        else	goto Err;
    }
Err:
    //	while( p->file!=NULL )
    //		close_file(p);
    return(-2);
}

static int pre_get_char( pre_t *p, char *c )
{ int z;
Retry:
    if( dfifo_read(p->fifo,c,1)>0 )
        return(0);
    if( (z=get_char(p))<0 )
    {	*c=0;
        return(z);
    }
    *c=(char)z;
    if( p->meta.col==1 && *c=='#' )
    {	if( (z=get_cmd(p))<0 )
            return(z);
        goto Retry;
    }
    return(0);
}

struct compiler_preprocessor_class *f_preprocessor_create( char *filename )
{ pre_t *p;
    if( (p=malloc(sizeof(pre_t)))==NULL )
        return(NULL);
    p->meta.destroy=(void(*)(struct compiler_preprocessor_class*))
            pre_destroy;
    p->meta.usecount=0;
    p->meta.get_char=(int(*)(struct compiler_preprocessor_class*,char*))
            pre_get_char;
    if( (p->fifo=dfifo_create(1,64))==NULL )
    {	free(p);
        return(NULL);
    }
    p->file=NULL;
    if( filename==NULL )
        filename="-";
    if( open_file(p,filename)<0 )
    {	dfifo_delete(p->fifo);
        free(p);
        return(NULL);
    }
    return((struct compiler_preprocessor_class*)p);
}
