// SPDX-License-Identifier: GPL-2.0
/**
 * @file data.c
 * @short Grammar for values, variables, ...
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#include "language.h"
#include "variables.h"
#include "../constable.h"
#include "../types.h"
#include "../event.h"

#include "conf_lang.h"

#define	SV1a	(N | 0x0d01)
#define	SV2	(N | 0x0d02)
#define	SV3	(N | 0x0d03)
#define	SV4	(N | 0x0d04)
#define	SV5	(N | 0x0d05)
#define	SV6	(N | 0x0d06)
#define	SV7	(N | 0x0d07)
#define	SV8	(N | 0x0d08)
#define	SS1	(N | 0x0d10)
#define	SA	(N | 0x0d11)

struct compile_tab_s value[] = {
	{S_val, {T_num, END}, {oLDI, T_num, OUT_VAL, oLTI, END}},
	{S_val, {T_arg, END}, {oARG, T_arg, OUT_VAL, SA, END}},
	{S_val, {T_str, END}, {T_str, SS1, END}},
	//{SS1, {T|'(', END}, {SEC_START|4, Pforce, SEC_SETVAL|1, SV2, oFRC, OUT_VAL4|1, oDEn, SEC_END, END}},
	{SS1, {END}, {oLDI, OUT_VAL, oLTS, END}},
	{S_val, {T_path, END}, {T_path, oLDI, OUT_VAL, oLTP, END}},
	{S_val, {Tspace, END}, {Tspace, oLDC, OUT_LEX, T_id, OUT_VAL, Pfindspa2, Psgvs, END}},
	{S_val, {T_id, END}, {T_id, SV1a, END}},
	{SV1a, {T|'.', END}, {oLDS, OUT_VAL, T|'.', T_id, OUT_VAL, END}},
	//{SV1a, {T|'(', END}, {E|0x0d01, SV2, END}},
	{SV1a, {END}, {oLDV, OUT_VAL, END}},
	{SA, {T|'.', END}, {oATR, T|'.', T_id, OUT_VAL, END}},
	{SA, {END}, {END}},

	/* buildin command */
	{S_val, {Tbuildin, END}, {SEC_START|4, Tbuildin, SEC_SETVAL|1, SV2, oBIN, OUT_VAL4|1, oDEn, SEC_END, SA, END}},
	{S_val, {Tcallfunc, END}, {SEC_START|4, Tcallfunc, SEC_SETVAL|1, SV2, oJSR, OUT_VAL4|1, oDEn, SEC_END, SA, END}},

	/* fetch + update */
	{S_val, {Tfetch, END}, {Tfetch, T_id, oFET, OUT_VAL, END}},
	{S_val, {Tupdate, END}, {Tupdate, T_id, oUPD, OUT_VAL, END}},

	/* typeof + commof */
	{S_val, {Ttypeof, END}, {Ttypeof, T|'(', S_exp, T|')', oTOF, END}},
	{S_val, {Tcommof, END}, {Tcommof, T|'(', S_exp, T|')', oCOF, END}},

	/* arguments for force, buildin, callfunc */
	{SV2, {T|';', T|',', END}, {SEC_INCVAL|0, SEC_DECVAL|0, oLDI, OUT_VAL4|0, END}},
	{SV2, {END}, {T|'(', SV3, T|')', oLDI, OUT_VAL4|0, END}},
	{SV3, {T|')', END}, {SEC_INCVAL|0, SEC_DECVAL|0, END}},
	{SV3, {END}, {SV5, END}},
	{SV4, {T|',', END}, {T|',', SV5, END}},
	{SV4, {END}, {END}},
	{SV5, {END}, {S_exp, SEC_INCVAL|0, SV4, END}},

	/* local variables */
	{S_val, {Tlocal, END}, {Tlocal, SV6, oVRL, SA, END}},

	/* transparent variables */
	{S_val, {Ttransparent, END}, {Ttransparent, SV6, oVRT, SA, END}},
	{SV6, {Ttypeof, END}, {Ttypeof, T|'(', S_exp, T|')', oDUP, oCOF, oSCS, oTOF, oS2C, oNEW, T_id, OUT_VAL, oSCD, END}}, /* ???? mozno netreba - to iste ako [commof(o)] typeof(x) */
	{SV6, {T|'[', END}, {T|'[', S_exp, oSCS, T|']', SV7, oSCD, END}},
	{SV6, {Talias, END}, {Talias, SV8, END}},
	{SV6, {END}, {SV7, END}},
	{SV7, {T_id, END}, {oLDI, T_id, Pid2class, OUT_VAL, oLTP, oNEW, T_id, OUT_VAL, END}},
	{SV7, {END}, {S_exp, oS2C, oNEW, T_id, OUT_VAL, END}},
	{SV8, {T_id, END}, {T_id, oLDI, OUT_VAL, oLTS, oALI, T_id, OUT_VAL, END}},
	{SV8, {END}, {S_exp, oALI, T_id, OUT_VAL, END}},
	{END}
};

