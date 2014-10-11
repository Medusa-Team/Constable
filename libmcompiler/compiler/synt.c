/* syntakticky analyzator
 * by Marek Zelem
 * copyright (c)1999
 * $Id: synt.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include <stdlib.h>
#include <mcompiler/compiler.h>

static int patri_term( struct compile_tab_s *tab, sym_t term )
{ int i;
	if( tab->terminal[0]==END )	return(1);	/* vsetky */
	for(i=0;i<COMPILE_TAB_TERMINALS && tab->terminal[i]!=END;i++)
	{	if( tab->terminal[i]==term )	return(1);
		if( tab->terminal[i]==TTT && i>0 && i<COMPILE_TAB_TERMINALS-1 )
		{	if( tab->terminal[i-1]<=term
				&& tab->terminal[i+1]>=term )
					return(1);
		}
		if( term==END && tab->terminal[i]==TEND )	return(1);
	}
	return(0);
}

static struct compile_tab_s *najdi_tab( compiler_class_t *c, sym_t stav, sym_t term )
{ int i;
  struct compile_tab_s *tab;
	for(i=0;i<COMPILE_TABLES;i++)
	{	if( (tab=c->tab[i].tab)==NULL )	continue;
		if( stav < c->tab[i].from  ||  stav > c->tab[i].to )
			continue;
		while( tab->stav!=END )
		{	if( tab->stav==stav && patri_term(tab,term) )
				return(tab);
			tab++;
		}
	}
	return(NULL);
}

static sym_t get_want(sym_t *stack, int stacklen)
{
	while( stacklen>=0 )
	{	if( (stack[stacklen] & TYP)==T )
			return(stack[stacklen]);
		if( (stack[stacklen] & TYP)!=O )
			return(END);
		stacklen--;
	}
	return(END);
}

#define RESIZE_STACK	\
	if( (stack=realloc(stack,sizeof(sym_t)*stacksize))==NULL )\
		return(eNOMEM)
#define	GET_LEX(chcem)		{\
	compiler->l=l;					\
	if( compiler->l_rel!=NULL )			\
	{	compiler->l_rel->used_count--;		\
		compiler->l_rel=NULL;			\
	}						\
	compiler->lex->lex(compiler->lex,&l,chcem);	\
	while( (l.sym & TYP)==E || (l.sym & TYP)==W )	\
	{	if( (l.sym & TYP)==E && 		\
		    (r=compiler->err->error(compiler->err,END,l.sym)) )	\
		{	free(stack); return(r);		}	\
		if( (l.sym & TYP)==W &&			\
		    (r=compiler->err->warning(compiler->err,END,l.sym)) ) \
		{	free(stack); return(r);		}	\
		compiler->lex->lex(compiler->lex,&l,chcem);	\
	}	\
			}

sym_t compiler_compile( compiler_class_t *compiler, sym_t start )
{ struct lex_s l;
  int i,j;
  struct compile_tab_s *t;
  sym_t *stack,want,r,sym;
  int stacklen,stacksize;
	if( compiler->lex==NULL || compiler->out==NULL )
		return(E|0);
  	compiler->exit=0;
	stack=NULL;
	stacklen=0;
	stacksize=100;
	RESIZE_STACK;
	stack[stacklen++]=start;
	GET_LEX(END);
	while( stacklen>0 )
	{
		while( (t=najdi_tab(compiler,stack[stacklen-1],l.sym))==NULL )
		{
		    	if( (r=compiler->err->error(compiler->err,END,l.sym)) )
			{	free(stack); return(r);		}
			if( l.sym!=TEND )
				GET_LEX(END)
			else
			{	free(stack);
				return(E|0);
			}
		}
		stacklen--;
		for(j=1;t[j].stav==NEXTLINE;)	j++;
		for(j--;j>=0;j--)
		{	for(i=0;i<COMPILE_TAB_SIZE && t[j].stack[i]!=END;)
				i++;
			for(i--;i>=0;)
			{	stack[stacklen++]=t[j].stack[i--];
				if( stacklen>=stacksize )
				{	stacksize+=100;
					RESIZE_STACK;
				}
			}
		}
		while( stacklen>0 )
		{	sym=stack[stacklen-1];
			if( (sym & TYP)==T )
			{	if( l.sym!=sym )
				{	if( (r=compiler->err->error(
						compiler->err,sym,l.sym)) )
					{	free(stack); return(r);	}
				}
				else
				{	if( l.sym!=TEND )
					{	want=get_want(stack,stacklen-2);
						GET_LEX(want);
					}
				}
			}
			else if( (sym & TYP)==LS )
			{	compiler->lex->lex(compiler->lex,NULL,sym);
			}
			else if( (sym & TYP)==O )
			{	compiler->special_out(compiler,sym);
				/* compiler->out(compiler->out_arg,sym,0); */
			}
			else if( (sym & TYP)==SO1 || (sym & TYP)==SO2 )
			{	compiler->special_out(compiler,sym);
			}
			else if( (sym & TYP)==VAL || (sym & TYP)==SO3 )
			{	compiler->special_out(compiler,sym);
			}
			else if( (sym & TYP)==PO )
			{	compiler->param_out(compiler,sym);
			}
			else if( (sym & TYP)==IO )
			{	compiler->inf_out(compiler,sym);
			}
			else if( (sym & TYP)==E )
			{	if( (r=compiler->err->error(
						compiler->err,sym,END)) )
				{	free(stack); return(r);		}
			}
			else if( (sym & TYP)==W )
			{	if( (r=compiler->err->warning(
						compiler->err,sym,END)) )
				{	free(stack); return(r);		}
			}
			else	break;
			if( compiler->exit )
			{	free(stack); return(compiler->exit);	}
			stacklen--;
		}
	}
	free(stack);
	return(T);
}

