/* Lexikalny analyzator
 * by Marek Zelem
 * (c)11.6.1999
 * $Id: lex.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include <stdlib.h>
#include <string.h>
#include <mcompiler/lex.h>

static lexstattab_t *find_end_of_tab( lexstattab_t *t )
{	while( t->state!=END )	t++;
	return(t);
}

static int lex_add_tab( struct lexstruct_s *l, lexstattab_t *stattab )
{
	find_end_of_tab(stattab)->operators=
		find_end_of_tab(l->stattab)->operators;
	find_end_of_tab(l->stattab)->operators=(lextab_t*)stattab;
	return(0);
}

static int lex_del_tab( struct lexstruct_s *l, lexstattab_t *stattab )
{ lexstattab_t *p=l->stattab,*q;
	while( (q=find_end_of_tab(p))->operators!=NULL )
	{	if( q->operators == (lextab_t*)stattab )
		{	q->operators=find_end_of_tab(stattab)->operators;
			find_end_of_tab(stattab)->operators=NULL;
			if( l->current>=stattab
			    && l->current<find_end_of_tab(stattab) )
				l->current=l->stattab;
			return(1);
		}
		p=(lexstattab_t*)(q->operators);
	}
	return(0);
}

static void lex_destroy( struct lexstruct_s *l )
{
	if( l->buf!=NULL )	free(l->buf);
	if( l->pre )	dec_use(l->pre);
	if( l->meta.filename!=NULL )	free(l->meta.filename);
	free(l);
}

static lexstattab_t *find_state( lexstattab_t *stattab, sym_t state )
{
	do {
		while( stattab->state!=END )
		{	if( stattab->state == state )
				return(stattab);
			stattab++;
		}
		stattab=(lexstattab_t*)(stattab->operators);
	   } while( stattab!=NULL );
	return(NULL);
}

static lextab_t *find_keyword( char *word, lextab_t *tab, int len, sym_t in )
{ lextab_t *n=NULL;
	if( tab==NULL )	return(NULL);
	while( tab->keyword!=NULL )
	{	if( len && !strncmp(word,tab->keyword,len) )
		{	/* if( in==END || (n=tab)->sym==in ) */
				return(tab);
		}
		if( !len && !strcmp(word,tab->keyword) )
		{	if( in==END || (n=tab)->sym==in )
				return(tab);
		}
		tab++;
	}
	return(n);
}

static int test_char( char c, char *test )
{ char p=0;
  int az=0;
  int positive=1;
	if( test==NULL )	return(0);
	while( *test!=0 )
	{	if( *test=='\\' && test[1]!=0 )
		{	test++;
			if( c==*test )	return(positive);
			if( az && p<=c && c<=*test )	return(positive);
			p=*test;
		}
		else if( *test=='-' )	az=2; 
		else if( *test=='!' )	positive=0; 
		else if( c==*test )	return(positive);
		else if( az && p<=c && c<=*test )	return(positive);
		else p=*test;
		if( az>0 )	az--;
		test++;
	}
	return(!positive);
}


#define	CHARtoBUF(inc)	\
	do { int st=0;	\
		l->buf[(l->buf_len)inc]=l->c;	\
		if( l->pre==NULL || (st=l->pre->get_char(l->pre,&(l->c))) )	\
		{	if( st < -1 )	\
			{ l->meta.col=l->pre->col; l->meta.row=l->pre->row; \
			   	out->sym=ePREERR;	return;	\
			}	\
			l->c=' ';	goto Eoi;	\
		}	\
	   } while(0)

#define	CHARbackSLASH	do { \
	if( l->c=='\\' )	CHARtoBUF( );	\
        switch( l->c )				\
	{	case 'a':     l->c='\a'; break;	\
		case 'b':     l->c='\b'; break;	\
		case 'e':     l->c='\e'; break;	\
		case 'f':     l->c='\f'; break;	\
		case 'n':     l->c='\n'; break;	\
		case 'r':     l->c='\r'; break;	\
		case 't':     l->c='\t'; break;	\
		case 'v':     l->c='\v'; break;	\
		case '\\':    l->c='\\'; break;	\
	}					\
	} while(0)


static void lex_lex( struct lexstruct_s *l, struct lex_s *out , sym_t in )
{ lexstattab_t *s,*c;
  int oper;
  lextab_t *lt;
	if( (in & TYP)==LS )
	{	s=find_state(l->stattab,in);
		if( s!=NULL )
			l->current=s;
		return;
	}
	if( in==TEND )
		return;
	oper=0; s=NULL; c=l->current; /* toto je iba aby nehadzal warning */
	if( l->pre==NULL )	goto Eoi;
Recursive:
	if( l->buf==NULL )
	{	l->buf_size=l->buf_len=0;	}
	if( (c=l->current)->operators!=NULL )	oper=1;
	else					oper=0;
	while(1)
	{	if( l->buf_len==0 )
		{	if(l->meta.filename!=NULL)
			{	if(strcmp(l->meta.filename,l->pre->filename))
				{  free(l->meta.filename);
				   l->meta.filename=strdup(l->pre->filename);
				}
			}
			else if( l->pre->filename!=NULL )
				l->meta.filename=strdup(l->pre->filename);
			l->meta.col=l->pre->col; l->meta.row=l->pre->row;
		}
		if( l->buf_len+1>=l->buf_size )
		{	l->buf_size=l->buf_len+1+16;
			l->buf=realloc(l->buf,l->buf_size);
			if( l->buf==NULL )
			{	out->sym=eNOMEM;
				return;
			}
		}
		if( oper )
		{	l->buf[l->buf_len]=l->c;
			l->buf[l->buf_len+1]=0;
			if( find_keyword(l->buf,c->operators,l->buf_len+1,END)
					==NULL )
			{  l->buf[l->buf_len]=0;
			   if( l->buf_len>0 )
			   {	if( (lt=find_keyword(l->buf,c->operators,0,in))
			   		!=NULL )
			   	{	s=NULL; goto MAM_lt;	}
				else	goto Err;
			   }
			   else	oper=0;
			}
			else
			{	CHARtoBUF(++);
				continue;
			}
		}
		if( (lt=c->rules)==NULL )	goto Err;
		while( lt->keyword!=NULL )
		{	if( test_char(l->c,lt->keyword) )
			{	if( lt->data & LEX_BSLASH )
					CHARbackSLASH;
				if( lt->data & LEX_VOID )
				{	CHARtoBUF( );	}
				if( lt->data & LEX_END )
				{	if( lt->sym==0 )	s=NULL;
					else s=find_state(l->stattab,lt->sym);
					goto MAM_buf;
				}
				else if( !(lt->data & LEX_VOID) )
					CHARtoBUF(++);
				break;
			}
			lt++;
		}
		if( lt->keyword==NULL )	goto Err;
		if( lt->sym!=0 )
		{	s=NULL;
			goto MAM_lt;
		}
	}
Eoi:	if( l->pre!=NULL )
	{	dec_use(l->pre); l->pre=NULL;	}
	if( l->buf_len==0 )
	{	out->sym=TEND;	return;	}
	l->buf[l->buf_len]=0;
	if( oper && l->buf_len>0 && (lt=find_keyword(l->buf,c->operators,0,in))
	  )
		goto MAM_lt;
MAM_buf:
	l->buf[l->buf_len]=0;
	if( (lt=find_keyword(l->buf,c->keywords,0,in))==NULL )
	{	out->sym=c->state;
		if( c->gen_lex!=NULL )
			c->gen_lex(l->buf,l->buf_len,&(out->sym),&(out->data),
					in);
		else	out->sym=eLEXERR;
	}
	else
	{
		if( in!=END && lt->sym!=in && c->gen_lex!=NULL )
		{	out->sym=c->state;
			c->gen_lex(l->buf,l->buf_len,&(out->sym),&(out->data),in);
		}
		else
		{
MAM_lt:	 		out->sym=lt->sym;
			out->data=lt->data;
		}
	}
	if( s!=NULL )	l->current=s;
	if( (out->sym & TYP)==LS )
	{	s=find_state(l->stattab,lt->sym);
		if( s!=NULL )	l->current=s;
		if( out->data & LEX_END )
			l->buf_len=0;
		goto Recursive;
	}
	l->buf_len=0;
	return;
Err:	out->sym=eLEXERR;
	CHARtoBUF( );
	l->buf_len=0;
	return;
}

struct compiler_lex_class *lex_create( lexstattab_t *stattab, struct compiler_preprocessor_class *pre )
{ struct lexstruct_s *l;
	if( (l=malloc(sizeof(struct lexstruct_s)))==NULL )
		return(NULL);
	l->meta.usecount=0;
	l->meta.destroy=(void(*)(struct compiler_lex_class*))lex_destroy;
	l->meta.lex=(void(*)(struct compiler_lex_class*,struct lex_s*,sym_t))
			lex_lex;
	l->meta.filename=strdup(pre->filename);
	l->meta.col=pre->col;
	l->meta.row=pre->row;
	l->add_tab=lex_add_tab;
	l->del_tab=lex_del_tab;
	l->current=l->stattab=stattab;
	find_end_of_tab(stattab)->operators=NULL;
	l->pre=pre; inc_use(pre);
	l->buf=NULL;
	l->buf_size=l->buf_len=0;
	if( l->pre->get_char(l->pre,&(l->c)) )
	{	dec_use(l->pre);	l->pre=NULL;	}
	return((struct compiler_lex_class*)l);
}

