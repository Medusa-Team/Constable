/*
 * Constable: types.h
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: types.h,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#ifndef _TYPES_H
#define _TYPES_H

#include <sys/types.h>
#include <stdint.h>

typedef struct { char bitmap[8]; } event_mask_t;

#ifndef NULL
#define	NULL	((void*)0)
#endif

#endif /* _TYPES_H */
