/* C Language header
 * by Marek Zelem
 * (c) 13.6.1999
 * $Id: c_language.h,v 1.3 2002/10/23 10:25:44 marek Exp $
 */

#ifndef _C_LANGUAGE_H
#define _C_LANGUAGE_H

#include <mcompiler/lex.h>

enum {
    CL_inc=T|0x0f00,
    CL_dec,
    CL_arrow,
    CL_relopL,
    CL_relopG,
    CL_relopLE,
    CL_relopGE,
    CL_relopEQ,
    CL_relopNE,
    CL_and,
    CL_or,
    CL_xor,
    CL_minus,
    CL_rol,
    CL_ror,
    CL_asgn_plus,
    CL_asgn_minus,
    CL_asgn_mul,
    CL_asgn_div,
    CL_asgn_mod,
    CL_asgn_rol,
    CL_asgn_ror,
    CL_asgn_and,
    CL_asgn_xor,
    CL_asgn_or,

    CL_void,
    CL_int,
    CL_char,
    CL_long,
    CL_short,
    CL_signed,
    CL_unsigned,
    CL_float,
    CL_double,
    CL_struct,
    CL_union,
    CL_enum,
    CL_typedef,
    CL_auto,
    CL_extern,
    CL_register,
    CL_static,
    CL_const,
    CL_volatile,
    CL_inline,
    CL_goto,
    CL_return,
    CL_sizeof,
    CL_break,
    CL_continue,
    CL_if,
    CL_else,
    CL_for,
    CL_do,
    CL_while,
    CL_switch,
    CL_case,
    CL_default,
    CL_entry,

    CL_ID,
    CL_VAL,
    CL_STR,
    CL_CHAR
};

enum {
    LSC_start=LS|0x0f00,
    LSC_ident,
    LSC_numb,
    LSC_exponent,
    LSC_string,
    LSC_stringc,
    LSC_stringbs,
    LSC_apos,
    LSC_aposbs,
    LSC_comment,
    LSC_comment_line
};

typedef struct	{
    int	typ;
    int	size;
    char	value[0];
} val_t;

enum {
    VT_ID=0,
    VT_char,
    VT_int8,
    VT_int16,
    VT_int32,
    VT_int64,
    VT_float=8,
    VT_double,
};
#define	VT_typmask	0x0f
#define	VT_signed	0x00
#define	VT_unsigned	0x10
#define	VT_array	0x20

extern lextab_t clex_operators[];
extern lextab_t clex_keywords[];
extern lexstattab_t clex_states[];
struct compiler_preprocessor_class *c_preprocessor_create( char *filename );


#endif /* _C_LANGUAGE_H */

