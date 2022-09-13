/*
 * Constable: vs.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: vs.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include "constable.h"
#include "vs.h"
#include <pthread.h>

//static int number_of_vs=MAX_NUM_OF_VS/32;
static struct vs_s vs_tab;
static int next_vs_id;

//int vs_init( int max_vs )
int vs_init( void )
{ int i;
    /*	if( (max_vs % 32) !=0 )
    {	char **errstr = (char**) pthread_getspecific(errstr_key);
        *errstr=Out_of_memory;
        return(-1);
    }
*/
    vs_tab.next=NULL;
    for(i=0;i<VS_WORDS;i++)
        vs_tab.vs[i]=0;
    /*
    if( max_vs > MAX_NUM_OF_VS )
    {	number_of_vs=MAX_NUM_OF_VS/32;
        char **errstr = (char**) pthread_getspecific(errstr_key);
        *errstr=Out_of_memory;
        return(1);
    }
    number_of_vs= max_vs/32;
*/
    next_vs_id=0;
    return(0);
}

/* ---------- vs alloc/free ---------- */
int vs_alloc( vs_t *id )
{
    if( next_vs_id >= MAX_NUM_OF_VS )
    {
        char **errstr = (char**) pthread_getspecific(errstr_key);
        *errstr=Out_of_vs;
        return(-1);
    }
    vs_clear(id);
    id[next_vs_id/BITS_PER_VS_WORD] |= 1 << (next_vs_id % BITS_PER_VS_WORD);
    next_vs_id += 1;
    return(0);
}

/**
 * Check whether a bit array of the size \param bites
 * can hold all of the allocated virtual spaces.
 * \param bites The available number of bits to be tested.
 */
int vs_is_enough( int bites )
{
    if ( next_vs_id > bites )
	    return(0);
    return(1);
}

void vs_clear( vs_t *to )
{ int i;
    for(i=0;i<VS_WORDS;i++)
        to[i]= 0;
}

void vs_fill( vs_t *to )
{ int i;
    for(i=0;i<VS_WORDS;i++)
        to[i]= ~((vs_t)0);
}

void vs_invert( vs_t *to )
{ int i;
    for(i=0;i<VS_WORDS;i++)
        to[i]= ~(to[i]);
}

void vs_set( const vs_t *from, vs_t *to )
{ int i;
    for(i=0;i<VS_WORDS;i++)
        to[i]= from[i];
}

/**
 * Add virtual spaces, no virtual spaces will be removed.
 * \param from Set of virtual spaces that will be added
 * \param to Set of virtual spaces that will be changed
 */
void vs_add( const vs_t *from, vs_t *to )
{ int i;
    for(i=0;i<VS_WORDS;i++)
        to[i]|= from[i];
}

void vs_sub( const vs_t *from, vs_t *to )
{ int i;
    for(i=0;i<VS_WORDS;i++)
        to[i]&= ~(from[i]);
}

void vs_mask( const vs_t *from, vs_t *to )
{ int i;
    for(i=0;i<VS_WORDS;i++)
        to[i]&= from[i];
}

int vs_test( const vs_t *test, const vs_t *vs )
{ int i;
    /* ATENTION, see also object_cmp_vs() in object.c */
    for(i=0;i<VS_WORDS;i++)
        if( vs[i] & test[i] )
            return(1);
    return(0);
}

int vs_issub( const vs_t *test, vs_t *vs )
{ int i;
    for(i=0;i<VS_WORDS;i++)
        if( (vs[i] & test[i]) != test[i] )
            return(0);
    return(1);
}


int vs_isclear( const vs_t *vs )
{ int i;
    for(i=0;i<VS_WORDS;i++)
        if( vs[i] )
            return(0);
    return(1);
}

int vs_isfull( const vs_t *vs )
{ int i;
    for(i=0;i<VS_WORDS;i++)
        if( vs[i]!=~((vs_t)0) )
            return(0);
    return(1);
}


