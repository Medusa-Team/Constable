/* compiler.c
 * by Marek Zelem
 * copyright (c)1999
 * $Id: compiler.c,v 1.3 2002/10/23 10:25:43 marek Exp $
 */

#include <stdlib.h>
#include <string.h>
#include <mcompiler/compiler.h>

int compiler_verlib(char *ver)
{
	if(ver!=NULL)
		strcpy(ver,"Compiler library V1.0 (c)11.7.1999,9.6.2002 by Marek Zelem");
	return(0x0100);
}


static sym_t c_warn( struct compiler_err_class *this, sym_t errsym, sym_t info )
{	return(0);
}

static sym_t c_err( struct compiler_err_class *this, sym_t errsym, sym_t info )
{	return(errsym);
}

static void c_err_destroy( struct compiler_err_class *this )
{	return;
}

static struct compiler_err_class c_default_err=
			{c_err_destroy,0,c_warn,c_err,0,0};

static void c_default_param_out( struct compiler_class *c, sym_t sym )
{	return;
}

static void c_default_inf_out( struct compiler_class *c, sym_t sym,... )
{	return;
}

static void compiler_destroy( compiler_class_t *c )
{
	if( c==NULL )	return;
	if( c->lex )
	{	c->lex->lex(c->lex,NULL,TEND);
		dec_use(c->lex);
	}
	c->special_out(c,TEND);
	c->param_out(c,TEND);
	c->inf_out(c,TEND);
	if( c->out )
	{	c->out->out(c->out,TEND,0);
		dec_use(c->out);
	}
	c->err->warning(c->err,TEND,TEND);
	c->err->error(c->err,TEND,TEND);
	dec_use(c->err);
	dfifo_delete(c->sections);
	dfifo_delete(c->data);
	free(c);
}

static int compiler_add_table( compiler_class_t *c, struct compile_tab_s *tab )
{ int i,j;
  sym_t l,h;
	for(i=0;i<COMPILE_TABLES;i++)
	{	if( c->tab[i].tab!=NULL )	continue;
		c->tab[i].tab=tab;
		l=h=tab[0].stav;
		for(j=0;tab[j].stav!=END;j++)
		{	if( tab[j].stav==NEXTLINE )	continue;
			if( tab[j].stav < l )	l=tab[j].stav;
			if( tab[j].stav > h )	h=tab[j].stav;
		}
		c->tab[i].from=l;
		c->tab[i].to=h;
		return(0);
	}
	return(-1);
}

sym_t compiler_compile( compiler_class_t *compiler, sym_t start );
void compiler_buildin1( compiler_class_t *compiler, sym_t sym );

compiler_class_t *compiler_create( void )
{ compiler_class_t *c;
	if( (c=calloc(sizeof(compiler_class_t),1))==NULL )
		return(NULL);
	c->usecount=0;
	c->destroy=compiler_destroy;
	c->add_table=compiler_add_table;
	c->compile=compiler_compile;
	c->lex=NULL; c->out=NULL;
	c->err=&c_default_err;
	c->param_out=c_default_param_out;
	c->inf_out=c_default_inf_out;

	c->special_out=compiler_buildin1;
	c->sections=dfifo_create(sizeof(struct sect_s),16);
	c->data=dfifo_create(sizeof(struct to_out_s),128);
	c->out_pos=0;
	c->exit=0;
	c->l_rel=NULL;
	return(c);
}

