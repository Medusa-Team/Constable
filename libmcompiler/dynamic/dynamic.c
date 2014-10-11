/* Library of dynamic structures  V1.0   11.2.1998	*/
/*	copyright (c)1998 by Marek Zelem		*/ 
/*	e-mail: marek@fornax.elf.stuba.sk		*/
/* $Id: dynamic.c,v 1.3 2002/10/23 10:25:44 marek Exp $	*/

#include <string.h>
#include <mcompiler/dynamic.h>

int lds_verlib(char *ver)
{
	if(ver!=NULL)
		strcpy(ver,"Library of dynamic structures V1.3 (c)16.10.1999 by Marek Zelem");
	return(0x0013);
}

/* --- historical ---- */

int dl_verlib(char *ver)
{	return(lds_verlib(ver));
}


