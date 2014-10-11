/*
 * test_c_lex
 * $Id: test_c_lex.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include <stdio.h>
#include <unistd.h>
#include <mcompiler/lex.h>
#include <mcompiler/c_language.h>

int main( void )
{ struct compiler_lex_class *l;
  struct lex_s lj;
	l=lex_create(clex_states,c_preprocessor_create(NULL));
	while(1)
	{	l->lex(l,&lj,END);
		if( (lj.sym&TYP)==E )
		{	printf("Error: %x\n",lj.sym);
			break;
		}
		else if( (lj.sym&TYP)==W )
		{	printf("Warning: %x\n",lj.sym);
		}
		else if( lj.sym==TEND )
		{	printf("TEND\n"); break;	}
		else if( lj.sym==END )
		{	printf("END\n"); break;	}
		else if( (lj.sym&TYP)!=T )
		{	printf("not TERMINAL %x\n",lj.sym); break;	}
		else if( (lj.sym&~TYP)<256 )
			printf("\'%c\'\n",lj.sym&~TYP);
		else if( lj.sym==CL_ID )
			printf("ID:\"%s\"\n",((val_t*)(lj.data))->value );
		else if( lj.sym==CL_VAL )
			printf("VAL:typ=%x\n",((val_t*)(lj.data))->typ );
		else if( lj.sym==CL_STR )
			printf("STR:\"%s\"\n",((val_t*)(lj.data))->value );
		else if( lj.sym==CL_CHAR )
			printf("CHAR:\'%s\'\n",((val_t*)(lj.data))->value );
		else
		{ lextab_t *p;
			p=clex_operators;
			while(p->keyword!=NULL)
			{	if( p->sym==lj.sym )	goto mam;
				p++;
			}
			p=clex_keywords;
			while(p->keyword!=NULL)
			{	if( p->sym==lj.sym )	goto mam;
				p++;
			}
			printf("?????\n"); goto cnt;
mam:			printf("\"%s\"\n",p->keyword);
cnt:
		}
	}
	l->destroy(l);
	return(0);
}

