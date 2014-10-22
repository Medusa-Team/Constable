/* Lexikalny analyzator
 * by Marek Zelem
 * (c)11.6.1999
 * $Id: lex.h,v 1.3 2002/10/23 10:25:44 marek Exp $
 */

#ifndef _LEX_H
#define _LEX_H

#include <mcompiler/compiler.h>

typedef struct	{
	char	*keyword;
	sym_t	sym;		/* stav - vtedy sa neposiela lex. jed. */
	unsigned long	data;
		} lextab_t;

typedef struct	{
	sym_t		state;		/* ak ==END a operators je rezervovany*/
					/* pre interne pouzitie na zretazenie*/
	lextab_t	*operators;	/* unseparated */
	lextab_t	*rules;		/* keyword su platne znaky */
					/* sym je stav do ktoreho ma prejst */
					/* 	0-tento */
					/* data je podla define */
	lextab_t	*keywords;	/* separated by separators */
	void(*gen_lex)(char*,int,sym_t*,unsigned long*,sym_t);
		} lexstattab_t;
#define	LEX_CONT	0x0000		/* pokracuj v nacitavani slova */
#define	LEX_END		0x0001		/* slovo nacitane (znak nepridavaj)*/
#define	LEX_VOID	0x0002		/* zahod tento znak */
#define	LEX_BSLASH	0x0004		/* vstup je backslash */


struct lexstruct_s	{
	struct compiler_lex_class meta;
	int(*add_tab)( struct lexstruct_s *l, lexstattab_t *stattab );
	int(*del_tab)( struct lexstruct_s *l, lexstattab_t *stattab );
	lexstattab_t	*stattab;
	lexstattab_t	*current;
	struct compiler_preprocessor_class *pre;
	char		c;		/* predvybrany jeden znak */
	char		*buf;
	int		buf_size,buf_len;
			};

struct compiler_lex_class *lex_create( lexstattab_t *stattab, struct compiler_preprocessor_class *pre );
#define	lexstruct(l)	((struct lexstruct_s*)(l))

#endif /* _LEX_H */

