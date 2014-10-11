/* C Preprocessor
 * by Marek Zelem
 * (c)28.6.1999
 * $Id: c_preprocesor.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <mcompiler/dynamic.h>
#include <mcompiler/c_language.h>

#define	MAX_LINE_LEN	2048
#define	MAX_ID_LEN	64

struct define_s
{	
	char *text;	/* za co sa ma nahradit */
	darg_t *args;	/* ak NULL, tak bez argumentov a zatvoriek */
	char name[MAX_ID_LEN];	/* meno define */
};


struct filestack_s
{	struct filestack_s *next;
	int fd,col,row;
	char *filename;
};

struct read_s
{	struct read_s *prev;
	struct define_s *def;	/* kvoli argumentom */
	char *to_out;
	int pos;
};

typedef struct	{
	struct compiler_preprocessor_class meta;
	int fd;
	struct filestack_s *filestack;
	int coment;
	tHash *defines;		/* zahashovane defines */
	struct read_s *read;
	int lp;
	char line[MAX_LINE_LEN];
		} pre_t;

/* spacovanie defines */

#define is_white(c)	((c)==' ' || (c)=='\t' || (c)=='\n')
#define	skip_white(p)	while( is_white(*(p)) )	(p)++
static darg_t *gen_args( char **s )
{ darg_t *a;
  char *p,*r,c;
  int zatv;
  char uv;
	skip_white(*s);
	if( **s!='(' )	return(NULL);
	(*s)++;
	if( (a=darg_alloc(1))==NULL )
		return(NULL);	/* out of mem !!! */
	while( **s!=')' )
	{	if( **s==0 )	goto Err;
		skip_white(*s);
		p=*s;
		zatv=0; uv=0;
		while( (!zatv && !uv && (*p!=',' || *p!=')')) || *p!=0 )
		{	if( uv=='"' || uv=='\'' )
				if( *p==uv && *(p-1)!='\\' )	uv=0;
			else if( *p=='(' || *p=='[' || *p=='{' )	zatv++;
			else if( *p==')' || *p==']' || *p=='}' )	zatv--;
			else if( *p=='"' || *p=='\'' )	uv=*p;
		}
		r=p-1;
		while( r>*s && is_white(*(r)) )	(r)--;
		r++;	c=*r;	*r=0;
		darg_append(a,*s);
		*r=c; *s=p;
	}
	*s++;
	return(a);
Err:	return(NULL);	/* !!! najak vypisat error, alebo co... */
}

static char *get_word( char **s )
{ char *p;
	skip_white(*s);
	p=*s;
	while( (**s>='a' && **s<='z') || (**s>='A' && **s<='Z') || **s=='_' )
		(*s)++;
}

static int get_define( struct define_s *d, char **s )
{ char *p;
  int l;
	p=get_word(s);	l=(*s)-p;
	if( l>MAX_ID_LEN-1 )	l=MAX_ID_LEN-1;
	memcpy(&(d->name),p,l);
	d->name[l]=0;
	d->args=gen_args(s);
	d->text=NULL;
	return(0);
}

static int compare_numargs( darg_t *def, darg_t *use )
{ char *s;
  int l;
	if( darg_argc(def)==darg_argc(use) )	return(1);
	if( darg_argc(def)<1 )			return(0);
	s=darg_argv(def)[darg_argc(def)-1];
	l=strlen(s);
	if( l<=3 || strcmp(s[l-3],"...") )	return(0);
	if( darg_argc(use)>=darg_argc(def)-1 )	return(1);
	return(0);
}


static int cmp_defines( void *a, void *b )
{
   return(strcmp(((struct define_s*)a)->name,((struct define_s*)b)->name));
}

static unsigned int gen_hash( void *a )
{
   return( Hash_Hash1(((struct define_s*)a)->name) );
}






static void pre_destroy( pre_t *p )
{ struct filestack_s *s;
  struct read_s *r;
	while( (s=p->filestack)!=NULL )
	{	p->filestack=s->next;
		if( s->filename!=NULL )	free(s->filename);
		close(s->fd);
		free(s);
	}
	if( p->meta.filename!=NULL )	free(p->meta.filename);
	close(p->fd);
	while( (r=p->read)!=NULL )
	{	p->read=r->prev;
		free(r);
	}
	free(p);
}

static int pre_get_line( pre_t *p )
{ int i;
	i=0;
	while( read(p->fd,&(p->line[i]),1)==1 )
	{	if( p->line[i]=='\n' )
		{	if( i>0 && p->line[i-1]=='\\' )
			{	p->line[i-1]='\n';
				continue;
			}
			i++;
			p->line[i]=0;
			return(i);
		}
		i++;
		if( i>=MAX_LINE_LEN-1 )
		{	p->line[MAX_LINE_LEN-1]=0;
			return(MAX_LINE_LEN-1);
		}
	}
	p->line[i]=0;
	if( i==0 )	return(-1);
	return(i);
}

enum {	NORMAL=0,
	COMLINE=1,
	COMENT=2,
	STRING=3
     };
static int remove_comments( pre_t *p )
{ int i;
  char *l=&(p->line[0]);
   i=0;
   while( l[i]!=0 )
   {
	if( p->coment==NORMAL )
	{	while( l[i]!=0 )
		{	if( l[i]=='/' && l[i+1]=='/' )
			{	p->coment=COMLINE; break;	}
			else if( l[i]=='/' && l[i+1]=='*' )
			{	p->coment=COMENT; break;	}
			if( l[i]=='"' && (i==0 || l[i-1]!='\\') )
			{	i++; p->coment=STRING; break;	}
			else	i++;
		}
	}
	if( p->coment==COMLINE )
	{	while( l[i]!=0 )
		{	if( l[i]=='\n' )
			{	p->coment=NORMAL; break;	}
			else if( l[i]=='\t' )	i++;
			else	l[i++]=' ';
		}
	}
	if( p->coment==COMENT )
	{	while( l[i]!=0 )
		{	if( l[i]=='*' && l[i+1]=='/' )
			{	l[i++]=' '; l[i++]=' ';
				p->coment=NORMAL; break;
			}
			else if( l[i]=='\n' || l[i]=='\t' )	i++;
			else	l[i++]=' ';
		}
	}
	if( p->coment==STRING )
	{	while( l[i]!=0 )
		{	if( l[i]=='"' && (i==0 || l[i-1]!='\\') )
			{	i++; p->coment=NORMAL; break;	}
			else	i++;
		}
	}
   }
   return(0);
}

static int do_pre_cmds( pre_t *p )
{ char *s;
  struct define_s *d;
	s=&(p->line[0]);
	while( *s==' ' || *s=='\t' )	s++;
	if( *s=='#' )
	{	while( *s==' ' || *s=='\t' )	s++;
		if( !strncmp(s,"define") && (s[6]==' ' || s[6]=='\t') )
		{	s+=6;
			if( (d=malloc(sizeof(struct define_s)))==NULL )
				return(-1);	/* !!! out of mem */
			get_define(d,&s);
			skip_white(s);
			if( (d->test=malloc(strlen(s)))==NULL )
			{	free(d);
				return(-1);	/* !!! out of mem */
			}
			HashIns(p->defines,d);
			return(1);
		}
		return(1);
	}
	return(0);
}

static int pre_get_char( pre_t *p, char *c )
{
	if( p->lp>0 && p->line[p->lp-1]=='\n' )
	{	p->meta.col=1; p->meta.row++;	}
	else	p->meta.col++;
	if( p->line[p->lp]==0 )
Nxt:	{	if( pre_get_line(p)<0 )
			return(-1);
		p->lp=0;
		remove_comments(p);
		if( do_pre_cmds(p) )
		{ int i;
			for(i=0;p->line[i]!=0;i++)
				if( p->line[i]=='\n' )	p->meta.row++;
			p->meta.col=1;
			goto Nxt;
		}
	}
	*c=p->line[p->lp++];
	return(0);
}

struct compiler_preprocessor_class *c_preprocessor_create( char *filename )
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
	p->filestack=NULL;
	p->coment=0;
	p->read=NULL;
	p->lp=0; p->line[0]=0;
	return((struct compiler_preprocessor_class*)p);
}

