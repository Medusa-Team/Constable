/* Marek's Universal Compiler
 * by Marek Zelem
 * copyright (c)1999
 * All rights reserved.
 * $Id: compiler.h,v 1.3 2002/10/23 10:25:44 marek Exp $
 */

#ifndef _COMPILER_H
#define _COMPILER_H

#include <stdint.h>

int compiler_verlib(char *ver);

typedef unsigned short sym_t;

struct lex_s	{
    sym_t		sym;
    uintptr_t	data;
};

#define	COMPILE_TAB_TERMINALS	7
#define	COMPILE_TAB_SIZE	16
struct compile_tab_s	{
    sym_t	stav;					/* riadok */
    sym_t	terminal[COMPILE_TAB_TERMINALS];	/* stlpec */
    sym_t	stack[COMPILE_TAB_SIZE];		/* obsah tabulky */
};

#define TYP	0xf000
#define SYM_VAL	0x0fff
#define	T	0x0000	/* Vstupne Terminaly */
/* 0x0000-0x00ff pre jednoznakove symboly */
/* 0x0f00-0x0fff pre jazyk C alebo P ... */
#define	N	0x1000	/* Neterminaly (stavy) */
#define	O	0x2000	/* Vystupne Terminaly (priame) */
#define	LS	0x3000	/* Lexikalny stav (pre lexikalny analyzator) */
#define	SO1	0x4000	/* Specialne vystupne terminaly (pre specialout) */
#define	SO2	0x5000	/* Specialne vystupne terminaly (pre specialout) */
#define	SO3	0x6000	/* Specialne vystupne terminaly (pre specialout) */
#define	VAL	0x7000	/* Nastavy hodnotu na cislo (0-fff) (pre specialout) */
#define	PO	0x8000	/* Parametricke vystupne terminaly (pre paramout) */
#define	IO	0x9000	/* Informacne vystupne terminaly (pre infout) */

#define	W	0xd000	/* Varovania */
#define	E	0xe000	/* Chyby */

#define	TEND	0xfff0	/* Koniec vstupneho suboru (len pre gramatiku) */
#define	TTT	0xfffd	/* az, pri terminaloch v tabulke */
#define	NEXTLINE 0xfffe	/* Pokracovanie pravidla z predchadzajuceho riadku */
#define	END	0xffff	/* Koniec */

/* niektore chyby, ktore generuje compiler */
#define	eNOMEM	(E|0x0fff)	/* fatalna chyba - nedostatok pamati */
#define	eLEXERR	(E|0x0ffe)	/* fatalna chyba lexikalneho analyzatora */
#define	ePREERR	(E|0x0ffd)	/* fatalna chyba preprocesora */


/*  Specialne vystupne terminaly (vstavane moznosti) */
#define	SEC_START	(SO1|0x010)	/* zaciatok sekcie typu (0-f) */
#define	SEC_END		(SO1|0x020)	/* koniec sekcie */
#define	OUT_LEX		(SO1|0x030)	/* na vystup typ vstupneho terminalu */
#define	OUT_VAL		(SO1|0x031)	/* na vystup hodnotu vstupneho term. */
/* pouzitie hodnoty navesia (0-ff) z najblizsej sekcie typu (0-f) */
#define	OUT_VAL0	(SO2|0x000)
#define	OUT_VAL1	(SO2|0x100)
#define	OUT_VAL2	(SO2|0x200)
#define	OUT_VAL3	(SO2|0x300)
#define	OUT_VAL4	(SO2|0x400)
#define	OUT_VAL5	(SO2|0x500)
#define	OUT_VAL6	(SO2|0x600)
#define	OUT_VAL7	(SO2|0x700)
#define	OUT_VAL8	(SO2|0x800)
#define	OUT_VAL9	(SO2|0x900)
#define	OUT_VALa	(SO2|0xa00)
#define	OUT_VALb	(SO2|0xb00)
#define	OUT_VALc	(SO2|0xc00)
#define	OUT_VALd	(SO2|0xd00)
#define	OUT_VALe	(SO2|0xe00)
#define	OUT_VALf	(SO2|0xf00)
#define	GET_VAL0	(SO3|0x000)
#define	GET_VAL1	(SO3|0x100)
#define	GET_VAL2	(SO3|0x200)
#define	GET_VAL3	(SO3|0x300)
#define	GET_VAL4	(SO3|0x400)
#define	GET_VAL5	(SO3|0x500)
#define	GET_VAL6	(SO3|0x600)
#define	GET_VAL7	(SO3|0x700)
#define	GET_VAL8	(SO3|0x800)
#define	GET_VAL9	(SO3|0x900)
#define	GET_VALa	(SO3|0xa00)
#define	GET_VALb	(SO3|0xb00)
#define	GET_VALc	(SO3|0xc00)
#define	GET_VALd	(SO3|0xd00)
#define	GET_VALe	(SO3|0xe00)
#define	GET_VALf	(SO3|0xf00)
/* nastavenie hodnoty navesia (0-ff) aktualnej sekcie */
#define	SEC_SETPOS	(SO1|0x100)	/* podla pozicie */
#define	SEC_SETLEX	(SO1|0x200)	/* podla vstupneho terminalu */
#define	SEC_SETVAL	(SO1|0x300)	/* podla hodnoty vstupneho terminalu */
/* prve nasledujuce navestie    */
#define	SEC_SEFPOS	(SO1|0x400)	/* podla pozicie - forward */
#define	SEC_SEFLEX	(SO1|0x500)	/* podla vstupneho terminalu */
#define	SEC_SEFVAL	(SO1|0x600)	/* podla hodnoty vstupneho terminalu */
/* inc a dec hodnoty navestia	*/
#define	SEC_INCVAL	(SO1|0x700)	/* VAL=VAL+1 */
#define	SEC_DECVAL	(SO1|0x800)	/* VAL=VAL-1 */

#include <mcompiler/dynamic.h>

struct label_s	{
    unsigned char	label;	/* cislo navestia */
    char		typ;	/* 0-undef, 1-data, 2-navestie */
    unsigned short	used_count;	/* kolko smernikov na toto existuje */
    unsigned long	data;
    struct label_s	*next;
};

struct sect_s	{
    char		typ;		/* 0-f */
    struct label_s	*labels;
    unsigned long	param_out_data;
    void		*param_out_ptr;
};

struct to_out_s	{
    sym_t	sym;
    char	valid;	/* 0-label , 1-data valid */
    union {	unsigned long data;
            struct label_s *rel;
          } u;
};

/*
error:	errsym	info	- vyznam
  +W	END	sym	- sym je typu E a je to error z lexikalneho analyz.
    END	sym	- neocakavany terminalny symbol sym.
    sym1	sym2	- ocakavany sym1 a prisiel sym2
  +W	sym	END	- sym je typu E z pravidla.
 f 	sym	nr	- sym je SEC_END - nedefinovane navestie nr v sekcii.
 f	sym	END	- sym je SEC_START|typ - nedefinovana sekcia typu
    END	sym	- sym je SEC_END - bez SEC_START
  +W	TEND	TEND	- koniec prekladu

infout:	sym		data	- vyznam
    sym			- sym je typu IO a je to z pravidiel
    SEC_SETPOS	pos	- tu je definovane navestie
    OUT_VAL		pos	- tu je pouzite navestie
    TEND			- koniec prekladu

param_out:	sym	- vyznam
        SEC_END	- koniec sekcie - uvolni param_out_ptr v sekcii
        TEND	- koniec prekladu
 */

struct compiler_preprocessor_class
{	void(*destroy)(struct compiler_preprocessor_class*);
    int usecount;
    int(*get_char)(struct compiler_preprocessor_class*,char*);
    char *filename;
    int col,row;
};

struct compiler_lex_class
{	void(*destroy)( struct compiler_lex_class * );
    int usecount;
    /* void lex( this, struct lex_s *out, sym_t wanted ); */
    void(*lex)( struct compiler_lex_class*,struct lex_s*,sym_t);
    char *filename;
    int col,row;
};

struct compiler_err_class
{	void(*destroy)( struct compiler_err_class * );
    int usecount;
    /* sym_t warning/error( this, sym_t errsym, sym_t info ); !=0 -> end */
    sym_t(*warning)(struct compiler_err_class*,sym_t,sym_t);
    sym_t(*error)(struct compiler_err_class*,sym_t,sym_t);
    int warnings;
    int errors;
};

struct compiler_out_class
{	void(*destroy)( struct compiler_out_class * );
    int usecount;
    /* void out( this, sym_t outsym, unsigned long data ); */
    void(*out)(struct compiler_out_class*,sym_t,unsigned long);
};

#define	COMPILE_TABLES		8

typedef struct compiler_class	{
    void(*destroy)( struct compiler_class * );
    int usecount;
    int(*add_table)( struct compiler_class *, struct compile_tab_s *tab );
    sym_t(*compile)( struct compiler_class *, sym_t start );

    struct compiler_lex_class *lex;
    struct compiler_out_class *out;
    struct compiler_err_class *err;
    void(*param_out)(struct compiler_class *,sym_t);
    void(*inf_out)(struct compiler_class *,sym_t,...);

    struct lex_s	l;	/* predchadzajuca lexikalna jednotka */
    struct label_s *l_rel;
    unsigned long	out_pos;	/* pozicia vo vystupe */

    /* compile tables */
    struct	{
        struct compile_tab_s *tab;
        sym_t	from,to;
    } tab[COMPILE_TABLES];
    /* internal output process */
    void(*special_out)(struct compiler_class *,sym_t);
    dfifo_t	*sections;	/* struct sect_s */
    dfifo_t	*data;		/* data to out (struct to_out_s) */
    sym_t	exit;
} compiler_class_t;

compiler_class_t *compiler_create( void );
struct compiler_preprocessor_class *nul_preprocessor_create( char *filename );
struct compiler_preprocessor_class *f_preprocessor_create( char *filename );
struct compiler_preprocessor_class *gcc_preprocessor_create( char *filename );

#define	inc_use(x)	do { (x)->usecount++; } while(0)
#define	dec_use(x)	do { (x)->usecount--;	\
    if((x)->usecount<=0) (x)->destroy((x));	\
    } while(0)

#endif /* _COMPILER_H */

