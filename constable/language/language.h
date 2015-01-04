/**
 * @file language.h
 * @short Main header file for compiling configuration file.
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: language.h,v 1.3 2002/10/23 10:25:43 marek Exp $
 */

#ifndef _LANGUAGE_H
#define _LANGUAGE_H

#include <mcompiler/compiler.h>
#include <mcompiler/lex.h>

#define	_AS	0x80
enum {
    Tpp = T|0x100,		/* ++ */
    Tmm,			/* -- */
    TshiftL,		/* << */
    TshiftR,		/* >> */
    Tle,			/* <= */
    Tge,			/* >= */
    Teq,			/* == */
    Tne,			/* != */
    Tand,			/* && */
    Txor,			/* ^^ */
    Tor,			/* || */

    Tif,			/* if */
    Telse,			/* else */
    Twhile,			/* while */
    Tdo,			/* do */
    Tfor,			/* for */
    Tbreak,			/* break */
    Tcontinue,		/* continue */
    Tswitch,		/* switch */
    Tcase,			/* case */
    Tdefault,		/* default */
    Treturn,		/* return */
    Tgoto,			/* goto */

    Tfunction,		/* function */
    Tcallfunc,		/* callfunc */
    //	Tforce,			/* force */
    Tfetch,			/* fetch */
    Tupdate,		/* update */
    Ttransparent,		/* transparent */
    Tlocal,			/* local */
    Talias,			/* alias */
    Ttypeof,		/* typeof */
    Tcommof,		/* commof */

    Tbuildin,		/* buildin */

    /* conf. language */
    Ttree,			/* tree */
    Tprimary,		/* primary */
    Tspace,			/* space */
    Trecursive,		/* recursive */
    Taccess,		/* <access type> */
    Tehhlist,		/* <ehh_list> */
    //	Tobject,		/* object */
    Tby,			/* by */
    Tof,			/* of */

    T_id,			/* <identifikator> */
    T_str,
    T_path,
    T_num,
    T_arg,
};


enum {
#ifdef DEBUG_TRACE
    oNOP = PO|0x100,
#else
    oNOP = O|0x100,
#endif
    oLD0,
    oLD1,
    oLDI,
    oLTI,
    oLTS,
    oLTP,
    oSTO,
    oSWP,
    oDEL,
    oDEn,
    oIMM,
    oLDC,
    oLDV,
    oLDS,
    oATR,
    oNEW,
    oALI,
    oVRL,
    oVRT,
    oTOF,
    oCOF,
    oS2C,
    oSCS,
    oSCD,
    oARG,
    oDUP,
    oADD,	/* poradie oADD - oXOR sa nesmie zmenit! */
    oSUB,
    oMUL,
    oDIV,
    oMOD,
    oSHL,
    oSHR,
    oLT,
    oGT,
    oLE,
    oGE,
    oEQ,
    oNE,
    oOR,
    oAND,
    oXOR,
    oNOT,
    oNEG,

    oJR,
    oJIZ,
    oJNZ,
    oJSR,
    oRET,

    oBIN,	/* buildin */
    //	oFRC,	/* force */
    oFET,	/* fetch */
    oUPD,	/* update */

    oILL=O|0x0fff	/* illegal */
};

#define	START	N|0x0a00
#define	SRET0	N|0x0aff
#define	CMD	N|0x0c00
#define	CMDS	N|0x0c30
#define	S_val	N|0x0d00
#define	S_exp	N|0x0e00
#define	S_exp2	N|0x0e01
/* N|0x0e00-0x0e40 - reserved for expressions */
#define	Pid2class	PO|0x0a20
//#define	Pforce		PO|0x0a40

extern struct compile_tab_s conf_lang[];
extern struct compile_tab_s language[];
extern struct compile_tab_s value[];
extern struct compile_tab_s expression[];

extern lextab_t operators[];
extern lextab_t keywords[];
extern lextab_t *keywords2;
extern lexstattab_t lex_tab[];

uintptr_t lex_getkeyword( char *keyword, sym_t sym );
int lex_updatekeyword( char *keyword, sym_t sym, uintptr_t data );
int lex_addkeyword( char *keyword, sym_t sym, uintptr_t data );

extern struct compiler_err_class s_error;

#endif /* _LANGUAGE_H */
