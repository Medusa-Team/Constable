/*
 * Constable: vs.h
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: vs.h,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#ifndef _VS_H
#define _VS_H

#include <asm/types.h>

typedef __u32	vs_t;

#define MAX_VS_BITS	64
#define BITS_PER_VS	(sizeof(vs_t) << 3)
#define	NUMBER_OF_VS	(MAX_VS_BITS/BITS_PER_VS)

struct vs_s {
    struct vs_s	*next;
    vs_t vs[NUMBER_OF_VS];
    char name[0];
};

int vs_init( void );
struct vs_s *vs_alloc( char *name );
int vs_is_enough( int bites );

void vs_clear( vs_t *to );
void vs_fill( vs_t *to );
void vs_invert( vs_t *to );
void vs_set( const vs_t *from, vs_t *to );
void vs_add( const vs_t *from, vs_t *to );
void vs_sub( const vs_t *from, vs_t *to );
void vs_mask( const vs_t *from, vs_t *to );

int vs_test( const vs_t *test, const vs_t *vs );
int vs_issub( const vs_t *test, vs_t *vs );
int vs_isclear( const vs_t *vs );
int vs_isfull( const vs_t *vs );






#endif


