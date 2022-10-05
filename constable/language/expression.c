// SPDX-License-Identifier: GPL-2.0
/**
 * @file expression.c
 * @short Grammar for C like expressions.
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#include "language.h"

#define	SV1a	(N | 0x0e03)
#define	SV2	(N | 0x0e04)
#define	SV2a	(N | 0x0e05)
#define	SV3	(N | 0x0e06)
#define	SV3d	(N | 0x0e07)
#define	SV4	(N | 0x0e08)
#define	SV4d	(N | 0x0e09)
#define	SV5	(N | 0x0e0a)
#define	SV5d	(N | 0x0e0b)
#define	SV6	(N | 0x0e0c)
#define	SV6d	(N | 0x0e0d)
#define	SV7	(N | 0x0e0e)
#define	SV7d	(N | 0x0e0f)
#define	SV8	(N | 0x0e10)
#define	SV8d	(N | 0x0e11)
#define	SV9	(N | 0x0e12)
#define	SV9d	(N | 0x0e13)
#define	SV10	(N | 0x0e14)
#define	SV10d	(N | 0x0e15)
#define	SV11	(N | 0x0e16)
#define	SV11d	(N | 0x0e17)
#define	SV11e	(N | 0x0e18)
#define	SV12	(N | 0x0e19)
#define	SV12d	(N | 0x0e1a)
#define	SV13	(N | 0x0e1b)
#define	SV13d	(N | 0x0e1c)
#define	SV13e	(N | 0x0e1d)
#define	SV14	(N | 0x0e1e)
#define	SV14d	(N | 0x0e1f)
#define	SV15	(N | 0x0e20)
#define	SV15d	(N | 0x0e21)
#define	S_exp2d	(N | 0x0e22)

/*
 *   Operatory
 * C:	() [] . ->			LP
 *	! ~ - ++ -- & * cast sizeof	PL
 *	* / %				LP
 *	+ -				LP
 *	<< >>				LP
 *	< <= > >=			LP
 *	== !=				LP
 *	&				LP
 *	^				LP
 *	|				LP
 *	&&				LP
 *	||				LP
 *	?:				PL
 *	= += -= ...			PL
 *	,				LP
 */

/*
 *	sekcie : e pre ?:
 *		 f pre && a ||
 */

struct compile_tab_s expression[] = {
//	{stav, {terminaly, END}, {stack, END}},

/* value () .			LP */
	{S_val, {T|'(', END}, {T|'(', S_exp2, T|')', END}},

/* ! ~ - ++ --			PL */
	{SV2, {T|'!', END}, {T|'!', SV2, oNOT, END}},
	{SV2, {T|'~', END}, {T|'~', SV2, oNEG, END}},
	{SV2, {T|'-', END}, {oLD0, T|'-', SV2, oSUB, END}},
	{SV2, {Tpp, END}, {Tpp, SV2, oDUP, oLD1, oADD, oSTO, END}},
	{SV2, {Tmm, END}, {Tmm, SV2, oDUP, oLD1, oSUB, oSTO, END}},
	{SV2a, {Tpp, END}, {Tpp, oDUP, oIMM, oSWP, oDUP, oLD1, oADD, oSTO, oDEL, END}},
	{SV2a, {Tmm, END}, {Tmm, oDUP, oIMM, oSWP, oDUP, oLD1, oSUB, oSTO, oDEL, END}},
	{SV2a, {END}, {END}},
	{SV2, {END}, {S_val, SV2a, END}},

/* * / %			LP */
	{SV3, {END}, {SV2, SV3d, END}},
	{SV3d, {T|'*', END}, {T|'*', SV2, oMUL, SV3d, END}},
	{SV3d, {T|'/', END}, {T|'/', SV2, oDIV, SV3d, END}},
	{SV3d, {T|'%', END}, {T|'%', SV2, oMOD, SV3d, END}},
	{SV3d, {END}, {END}},

/* + -				LP */
	{SV4, {END}, {SV3, SV4d, END}},
	{SV4d, {T|'+', END}, {T|'+', SV3, oADD, SV4d, END}},
	{SV4d, {T|'-', END}, {T|'-', SV3, oSUB, SV4d, END}},
	{SV4d, {END}, {END}},

/* << >>			LP */
	{SV5, {END}, {SV4, SV5d, END}},
	{SV5d, {TshiftL, END}, {TshiftL, SV4, oSHL, SV5d, END}},
	{SV5d, {TshiftR, END}, {TshiftR, SV4, oSHR, SV5d, END}},
	{SV5d, {END}, {END}},

/* < <= > >=			LP */
	{SV6, {END}, {SV5, SV6d, END}},
	{SV6d, {T|'<', END}, {T|'<', SV5, oLT, SV6d, END}},
	{SV6d, {T|'>', END}, {T|'>', SV5, oGT, SV6d, END}},
	{SV6d, {Tle, END}, {Tle, SV5, oLE, SV6d, END}},
	{SV6d, {Tge, END}, {Tge, SV5, oGE, SV6d, END}},
	{SV6d, {END}, {END}},

/* == !=			LP */
	{SV7, {END}, {SV6, SV7d, END}},
	{SV7d, {Teq, END}, {Teq, SV6, oEQ, SV7d, END}},
	{SV7d, {Tne, END}, {Tne, SV6, oNE, SV7d, END}},
	{SV7d, {END}, {END}},

/* &				LP */
	{SV8, {END}, {SV7, SV8d, END}},
	{SV8d, {T|'&', END}, {T|'&', SV7, oAND, SV8d, END}},
	{SV8d, {END}, {END}},

/* ^				LP */
	{SV9, {END}, {SV8, SV9d, END}},
	{SV9d, {T|'^', END}, {T|'^', SV8, oXOR, SV9d, END}},
	{SV9d, {END}, {END}},

/* |				LP */
	{SV10, {END}, {SV9, SV10d, END}},
	{SV10d, {T|'|', END}, {T|'|', SV9, oOR, SV10d, END}},
	{SV10d, {END}, {END}},

/* &&				LP */
	{SV11, {END}, {SV10, SV11d, END}},
	{SV11d, {Tand, END}, {Tand, SEC_START|0xf, oJIZ, OUT_VALf|1, SV10, SV11e, END}},
	{SV11d, {END}, {END}},
	{SV11e, {Tand, END}, {Tand, oJIZ, OUT_VALf|1, SV10, SV11e, END}},
	{SV11e, {END}, {oJIZ, OUT_VALf|1, oLD1, oJR, OUT_VALf|2, SEC_SETPOS|1,
			oLD0, SEC_SETPOS|2, SEC_END, END}},

/* ^^				LP */
	{SV12, {END}, {SV11, SV12d, END}},
	{SV12d, {Txor, END}, {Txor, oNOT, SV11, oNOT, oNE, SV12d, END}},
	{SV12d, {END}, {END}},

/* ||				LP */
	{SV13, {END}, {SV12, SV13d, END}},
	{SV13d, {Tor, END}, {Tor, SEC_START|0xf, oJNZ, OUT_VALf|1, SV12, SV13e, END}},
	{SV13d, {END}, {END}},
	{SV13e, {Tor, END}, {Tor, oJNZ, OUT_VALf|1, SV12, SV13e, END}},
	{SV13e, {END}, {oJNZ, OUT_VALf|1, oLD0, oJR, OUT_VALf|2, SEC_SETPOS|1,
			oLD1, SEC_SETPOS|2, SEC_END, END}},

/* ?:				PL */
	{SV14, {END}, {SV13, SV14d, END}},
	{SV14d, {T|'?', END}, {T|'?', SEC_START|0xe, oJIZ, OUT_VALe|1, SV14, oJR, OUT_VALe|2,
				T|':', SEC_SETPOS|1, SV14, SEC_SETPOS|2, SEC_END, END}},
	{SV14d, {END}, {END}},

/* = += -= ...			PL */
	{SV15, {END}, {SV14, SV15d, END}},
	{SV15d, {T|'=', END}, {T|'=', SV15, oSTO, END}},
	{SV15d, {T|'+'|_AS, END}, {T|'+'|_AS, oDUP, SV15, oADD, oSTO, END}},
	{SV15d, {T|'-'|_AS, END}, {T|'-'|_AS, oDUP, SV15, oSUB, oSTO, END}},
	{SV15d, {T|'*'|_AS, END}, {T|'*'|_AS, oDUP, SV15, oMUL, oSTO, END}},
	{SV15d, {T|'/'|_AS, END}, {T|'/'|_AS, oDUP, SV15, oDIV, oSTO, END}},
	{SV15d, {T|'%'|_AS, END}, {T|'%'|_AS, oDUP, SV15, oMOD, oSTO, END}},
	{SV15d, {T|'&'|_AS, END}, {T|'&'|_AS, oDUP, SV15, oAND, oSTO, END}},
	{SV15d, {T|'|'|_AS, END}, {T|'|'|_AS, oDUP, SV15, oOR, oSTO, END}},
	{SV15d, {T|'^'|_AS, END}, {T|'^'|_AS, oDUP, SV15, oXOR, oSTO, END}},
	{SV15d, {TshiftL|_AS, END}, {T|'%'|_AS, oDUP, SV15, oSHL, oSTO, END}},
	{SV15d, {TshiftR|_AS, END}, {T|'%'|_AS, oDUP, SV15, oSHR, oSTO, END}},
	{SV15d, {END}, {END}},

	{S_exp, {END}, {SV15, END}},

/* ,				LP */
	{S_exp2, {END}, {SV15, S_exp2d, END}},
	{S_exp2d, {T|',', END}, {T|',', oDEL, SV15, S_exp2d, END}},
	{S_exp2d, {END}, {END}},
	{END}
};
