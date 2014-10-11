/*
 * Pokus ;-)
 * $Id: pokus_pre.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#define A(a,b...)	xxxx##a##_c##b.d
//#define A(a)		x2xx##a.c##b.d

A( [w,z],x);

