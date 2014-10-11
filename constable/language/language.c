/**
 * @file language.c
 * @short Grammar for C like decide functions.
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: language.c,v 1.3 2003/12/30 18:08:18 marek Exp $
 */

#include "language.h"

#define	C1	N|0x0c01
#define	C2	N|0x0c02
#define	C3	N|0x0c03
#define	C4	N|0x0c04
#define	C5	N|0x0c05
#define	C6	N|0x0c06
#define	C7	N|0x0c07
#define	Cw	N|0x0c10
#define	CNE	N|0x0c11
#define	CR	N|0x0c12
//#define	CMDS	N|0x0c30

/*
	sekcie : 1 pre if
		 2 pre break    - vnutro switch
		 3 pre continue - vnutro for,while,do
		     VAR ff - break/continue
		 4 argumenty pre force, buildin a callfunc
 */

struct compile_tab_s language[]={
//	{stav,{terminaly,END},{stack,END}},

/* if - else */
	{CMD,{Tif,END},{Tif,S_exp2,SEC_START|1,oJIZ,OUT_VAL1|1,CMD,C1,END}},
	{C1,{Telse,END},{Telse,oJR,OUT_VAL1|2,SEC_SETPOS|1,CMD,SEC_SETPOS|2,
			SEC_END,END}},
	{C1,{END},{SEC_SETPOS|1,SEC_END,END}},
/* do - while - else */
	{CMD,{Tdo,END},{Tdo,SEC_START|2,SEC_START|3,SEC_SETPOS|1,CMD,C2,END}},
//	{C2,{T???,END},{T???,SEC_SETPOS|0xff,SEC_END,SEC_SETPOS|0xff,SEC_END,END}},
	{C2,{END},{Cw,END}},
	{CMD,{Twhile,END},{SEC_START|2,SEC_START|3,SEC_SETPOS|1,Cw,END}},
	{Cw,{END},{Twhile,SEC_SETPOS|0xff,S_exp2,oJIZ,OUT_VAL3|2,CMD,C3,END}},
	{C3,{Telse,END},{Telse,oJR,OUT_VAL3|1,SEC_SETPOS|2,CMD,
			SEC_END,SEC_SETPOS|0xff,SEC_END,END}},
	{C3,{END},{oJR,OUT_VAL3|1,SEC_SETPOS|2,
			SEC_END,SEC_SETPOS|0xff,SEC_END,END}},
/* for */
	{CMD,{Tfor,END},{Tfor,SEC_START|2,SEC_START|3,
			T|'(',CNE,T|';',SEC_SETPOS|4,C4,oJR,OUT_VAL3|3,END}},
	{NEXTLINE,{},{	T|';',SEC_SETPOS|0xff,CNE,oJR,OUT_VAL3|4,T|')',
			SEC_SETPOS|3,CMD,oJR,OUT_VAL3|0xff,
			SEC_END,SEC_SETPOS|0xff,SEC_END,END}},
	{C4,{T|';',END},{END}},
	{C4,{END},{S_exp2,oJIZ,OUT_VAL2|0xff,END}},
	{CNE,{T|';',END},{END}},
	{CNE,{END},{S_exp2,oDEL,END}},
/* break */
	{CMD,{Tbreak,END},{Tbreak,T|';',oJR,OUT_VAL2|0xff,END}},
/* continue */
	{CMD,{Tcontinue,END},{Tcontinue,T|';',oJR,OUT_VAL3|0xff,END}},
/* switch - case , default */
	{CMD,{Tswitch,END},{Tswitch,S_exp2,SEC_START|2,T|'{',C7,T|'}',
			SEC_SETPOS|1,SEC_SETPOS|0xff,SEC_END,oDEL,END}},
	{C7,{Tcase,END},{Tcase,SEC_SEFPOS|0,oDUP,S_exp2,T|':',
			oEQ,oJIZ,OUT_VAL2|0,SEC_SEFPOS|1,C5,END}},
	{C7,{Tdefault,END},{oJR,OUT_VAL2|0,Tdefault,SEC_SETPOS|2,SEC_SEFPOS|1,T|':',C5,END}},
	{C5,{Tcase,Tdefault,END},{oJR,OUT_VAL2|1,C7,END}},
	{C5,{T|'}',END},{oJR,OUT_VAL2|0xff,SEC_SETPOS|0,oJR,OUT_VAL2|2,END}},
	{C5,{END},{CMD,C5,END}},
/* return */
	{CMD,{Treturn,END},{Treturn,C6,oRET,T|';',END}},
	{C6,{T|';',END},{SRET0,END}},
	{C6,{END},{S_exp2,END}},

/* ; {} exp. */
	{CMD,{T|';',END},{T|';',END}},
	{CMD,{T|'{',END},{T|'{',CR,T|'}',END}},
	{CR,{T|'}',END},{END}},
	{CR,{END},{CMD,CR,END}},
	{CMD,{END},{S_exp2,oDEL,T|';',END}},

	{CMDS,{END},{CR,SRET0,oRET,END}},

	{END}
};

