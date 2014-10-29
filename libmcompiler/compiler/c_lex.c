/* C - Lexikalny analyzator
 * by Marek Zelem
 * (c)13.6.1999
 * $Id: c_lex.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
//#include <math.h>
#include <mcompiler/c_language.h>
#include <mcompiler/backslash.h>

lextab_t clex_operators[]={
	{"+",T|'+',0}, {"-",T|'-',0}, {"*",T|'*',0}, {"/",T|'/',0},
	{"%",T|'%',0},
	{"&",T|'&',0}, {"|",T|'|',0}, {"^",T|'^',0}, {"~",T|'~',0},
	{"!",T|'!',0},
	{".",T|'.',0}, {",",T|',',0}, {";",T|';',0}, {":",T|':',0},
	{"{",T|'{',0}, {"}",T|'}',0}, {"(",T|'(',0}, {")",T|')',0},
	{"[",T|'[',0}, {"]",T|']',0}, {"?",T|'?',0},
	{"++",CL_inc,0}, {"--",CL_dec,0},
	{"->",CL_arrow,0},
	{"<",CL_relopL,0}, {">",CL_relopG,0},
	{"<=",CL_relopLE,0}, {">=",CL_relopGE,0},
	{"==",CL_relopEQ,0}, {"!=",CL_relopNE,0},
	{"&&",CL_and,0}, {"||",CL_or,0}, {"^^",CL_xor,0},
	{"==",CL_minus,0},
	{"<<",CL_rol,0}, {">>",CL_ror,0},
	{"=",T|'=',0},
	{"+=",CL_asgn_plus,0}, {"-=",CL_asgn_minus,0},
	{"*=",CL_asgn_mul,0}, {"/=",CL_asgn_div,0},
	{"%=",CL_asgn_mod,0},
	{"<<=",CL_asgn_rol,0}, {">>=",CL_asgn_ror,0},
	{"&=",CL_asgn_and,0}, {"^=",CL_asgn_xor,0}, {"|=",CL_asgn_or,0},
	{"/*",LSC_comment,LEX_END}, {"//",LSC_comment_line,LEX_END},
	{NULL,END,0}
			};
lextab_t clex_comment_op[]={
	{"*",LSC_comment,LEX_END},
	{"*/",LSC_start,LEX_END},
	{NULL,END,0}
			};
lextab_t clex_keywords[]={
	{"void",CL_void,0},
	{"int",CL_int,0},
	{"char",CL_char,0},
	{"long",CL_long,0},
	{"short",CL_long,0},
	{"signed",CL_signed,0},
	{"unsigned",CL_unsigned,0},
	{"float",CL_float,0},
	{"double",CL_double,0},
	{"struct",CL_struct,0},
	{"union",CL_union,0},
	{"enum",CL_enum,0},
	{"typedef",CL_typedef,0},
	{"auto",CL_auto,0},
	{"extern",CL_extern,0},
	{"register",CL_register,0},
	{"static",CL_static,0},
	{"const",CL_const,0},
	{"volatile",CL_volatile,0},
	{"inline",CL_inline,0},
	{"goto",CL_goto,0},
	{"return",CL_return,0},
	{"sizeof",CL_sizeof,0},
	{"break",CL_break,0},
	{"continue",CL_continue,0},
	{"if",CL_if,0},
	{"else",CL_else,0},
	{"for",CL_for,0},
	{"do",CL_do,0},
	{"while",CL_while,0},
	{"switch",CL_switch,0},
	{"case",CL_case,0},
	{"default",CL_default,0},
	{"entry",CL_entry,0},
	{NULL,END,0}
			};

/* definicia pravidiel pre lexikalny analyzator */

lextab_t clex_rules_start[]={
	{" \t\n\r",LSC_start,LEX_VOID},	/*!!!dalo by sa urychlit inym stavom */
	{"a-zA-Z_",LSC_ident,LEX_CONT},
	{"0-9",LSC_numb,LEX_CONT},
	{"\"",LSC_string,LEX_VOID},
	{"'",LSC_apos,LEX_VOID},
	{NULL,END,0}	};
lextab_t clex_rules_ident[]={
	{"a-zA-Z_0-9",0,LEX_CONT},
	{"!",LSC_start,LEX_END},
	{NULL,END,0}	};
lextab_t clex_rules_numb[]={
	{"0-9a-fA-FxulXUL.",0,LEX_CONT},
	{"!",LSC_start,LEX_END},
	{NULL,END,0}	};
lextab_t clex_rules_exponent[]={
	{"+\\-0-9",LSC_numb,LEX_CONT},
	{"!",LSC_start,LEX_END},
	{NULL,END,0}	};
lextab_t clex_rules_comm[]={
	{"!",LSC_comment,LEX_VOID},
	{NULL,END,0}	};
lextab_t clex_rules_comml[]={
	{"\n",LSC_start,LEX_VOID},
	{"!",0,LEX_VOID},
	{NULL,END,0}	};
lextab_t clex_rules_string[]={
	{"\"",LSC_stringc,LEX_VOID},
	{"\\",LSC_stringbs,LEX_CONT},
	{"!",0,LEX_CONT},
	{NULL,END,0}	};
lextab_t clex_rules_stringc[]={
	{"\"",LSC_string,LEX_VOID},
	{" \t\n\r",0,LEX_VOID},		/* !!! pozor na poznamky */
	{"!",LSC_start,LEX_END},
	{NULL,END,0}	};
lextab_t clex_rules_stringbs[]={
	{"!",LSC_string,LEX_CONT},
	{NULL,END,0}	};
lextab_t clex_rules_apos[]={
	{"\'",LSC_start,LEX_VOID|LEX_END},
	{"\\",LSC_aposbs,LEX_CONT},
	{"!",0,LEX_CONT},
	{NULL,END,0}	};
lextab_t clex_rules_aposbs[]={
	{"!",LSC_apos,LEX_CONT},
	{NULL,END,0}	};


void clex_gen_lex_ident( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want );
void clex_gen_lex_numb( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want );
void clex_gen_lex_string( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want );
void clex_gen_lex_apos( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want );

lexstattab_t clex_states[]={
	{LSC_start,clex_operators,clex_rules_start,NULL,NULL},
	{LSC_ident,NULL,clex_rules_ident,clex_keywords,clex_gen_lex_ident},
	{LSC_numb,NULL,clex_rules_numb,NULL,clex_gen_lex_numb},
	{LSC_exponent,NULL,clex_rules_exponent,NULL,clex_gen_lex_numb},
	{LSC_string,NULL,clex_rules_string,NULL,clex_gen_lex_string},
	{LSC_stringc,NULL,clex_rules_stringc,NULL,clex_gen_lex_string},
	{LSC_stringbs,NULL,clex_rules_stringbs,NULL,clex_gen_lex_string},
	{LSC_apos,NULL,clex_rules_apos,NULL,clex_gen_lex_apos},
	{LSC_aposbs,NULL,clex_rules_aposbs,NULL,clex_gen_lex_apos},
	{LSC_comment,clex_comment_op,clex_rules_comm,NULL,NULL},
	{LSC_comment_line,NULL,clex_rules_comml,NULL,NULL},
	{END,NULL,NULL,NULL,NULL}
			};

void clex_gen_lex_ident( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want )
{ val_t *v;
	if( (v=malloc(sizeof(val_t)+len+1))==NULL )
	{	*sym=eNOMEM;
		return;
	}
	v->typ=VT_ID;
	v->size=len;
	strncpy(v->value,buf,len);
	v->value[len]=0;
	*sym=CL_ID;
	*data=(uintptr_t)v;
}

double m_pow( float base , int exp )
{ double r=1;
	while( exp>0 )
	{	r *= base;
		exp--;
	}
	return(r);
}

static void parse_float( char **buf, double *f )
{ double d;
  //int sign=1; 
  int exp=0;
	if( **buf=='.' )
	{	(*buf)++;
		d=10.0;
		while( **buf>='0' && **buf<='9' )
		{	(*f)+= ((double)(**buf-'0'))/d;
			d*=10.0;
			(*buf)++;
		}
	}
	if( tolower(**buf)=='e' )
	{	(*buf)++;
		if( **buf=='+' )
		{/* sign=1; */(*buf)++; }
		else if( **buf=='-' )
		{ /* sign=-1;*/ (*buf)++; }
		while( **buf>='0' && **buf<='9' )
		{	exp*=10; exp+=**buf-'0';
			(*buf)++;
			/* !!! otestovat hranivu exponentu */
		}
		(*f) *= m_pow(10,exp);
	}
	return;
}

void clex_gen_lex_numb( char *buf, int blen, sym_t *sym, uintptr_t *data, sym_t want )
{ val_t *v;
  int base=10;
  int n;
  int typ,len;
  unsigned long long l,b;
  double f;
	buf[blen]=0;
  	l=0; b=0; f=0;
	if( *buf==0 )
	{	buf++; base=8;
		if( tolower(*buf)=='x' )
		{ buf++; base=16; }
	}
	len=4;
	while( 1 )
	{	n= -1;
		if( *buf<='9' )	n=*buf-'0';
		if( tolower(*buf)>='a' && tolower(*buf)<='f' )
			n=10+tolower(*buf)-'a';
		if( n<0 || n>=base ) break;
		buf++;
		l*=base; l+=n;
		if( b==0 )	b=1;
		else		b*=base;
		if( len<8 && b >= 0x0100000000ll )	len=8;
		if( len==8 && b==0 )	goto Err;
	}
	typ=0;
	if( base==10 && *buf=='.' )
	{	f=l; parse_float(&buf,&f); typ=VT_double;	}
	else if( base==10 && tolower(*buf)=='e' )
	{	if( buf[1]==0 )
		{	if( *sym==LSC_exponent )	goto Err;
			*sym=LSC_exponent;
			*data=LEX_CONT;
			return;
		}
		f=l; parse_float(&buf,&f); typ=VT_double;
	}
	if( typ==VT_double )	len=sizeof(double);
	else
	{	typ=VT_signed;
		while( *buf!=0 )
		{	if( tolower(*buf)=='u' )	typ=VT_unsigned;
			else if( tolower(*buf)=='l' && tolower(buf[1])=='l' )
			{	buf++;	len=8;	}
			else if( tolower(*buf)=='l' )
			{	if( len>4 )	goto Err;
				len=4;
			}
			else	goto Err;
		}
		if( len<=4 )	typ|=VT_int32;
		else if( len<=8 )	typ|=VT_int64;
		else goto Err;
	}
	if( *buf!=0 )	goto Err;
	if( (v=malloc(sizeof(val_t)+len))==NULL )
	{	*sym=eNOMEM;
		return;
	}
	v->typ=typ;
	v->size=1;
	if( (typ&VT_typmask)==VT_int32 )
		*((unsigned long*)(v->value))=(unsigned long)l;
	else if( (typ&VT_typmask)==VT_int64 )
		*((unsigned long long*)(v->value))=l;
	else if( (typ&VT_typmask)==VT_double )
		*((double *)(v->value))=f;
	*sym=CL_VAL;
	*data=(uintptr_t)v;
	return;
Err:	*sym=eLEXERR;
	return;
}

void clex_gen_lex_string( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want )
{ val_t *v;
  int p;
  long l;
	if( (v=malloc(sizeof(val_t)+len+1))==NULL )
	{	*sym=eNOMEM;
		return;
	}
	v->typ=VT_char|VT_array;
	for(p=0;*buf!=0;)
	{	if( *buf=='\\' )
		{	v->value[p++]=backslash_parse(buf,&l);
			buf+=l;
		}
		else	v->value[p++]=*buf++;
	}
	v->value[p]=0;
	v->size=p;
	*sym=CL_STR;
	*data=(uintptr_t)v;
}

void clex_gen_lex_apos( char *buf, int len, sym_t *sym, uintptr_t *data, sym_t want )
{ val_t *v;
  int p;
  long l;
	if( (v=malloc(sizeof(val_t)+len+1))==NULL )
	{	*sym=eNOMEM;
		return;
	}
	v->typ=VT_char;
	for(p=0;*buf!=0;)
	{	if( *buf=='\\' )
		{	v->value[p++]=backslash_parse(buf,&l);
			buf+=l;
		}
		else	v->value[p++]=*buf++;
	}
	v->value[p]=0;
	v->size=p;
	if( p!=1 )
	{	free(v);
		*sym=eLEXERR;
		return;
	}
	*sym=CL_CHAR;
	*data=(uintptr_t)v;
}

