/*
 * Constable: access_types.h
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: access_types.h,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#ifndef _ACCESS_TYPES_H
#define _ACCESS_TYPES_H

#define	NR_ACCESS_TYPES	8

#define	AT_MEMBER	0
#define	AT_RECEIVE	1
#define	AT_SEND		2
#define	AT_SEE		3
#define	AT_CREATE	4
#define	AT_ERASE	5
#define	AT_ENTER	6
#define	AT_CONTROL	7

//	vs_t		vs[NR_ACCESS_TYPES][NUMBER_OF_VS];

extern char *access_names[NR_ACCESS_TYPES];

#endif /* _ACCESS_TYPES_H */

