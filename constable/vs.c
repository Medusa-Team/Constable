/*
 * Constable: vs.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: vs.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include "constable.h"
#include "vs.h"
#include <pthread.h>

static int next_vs_id;

/**
 * Initialize the module responsible for virtual spaces handling.
 * Return value less than zero means that the initialization wasn't
 * executed successfully.
 */
int vs_init( void )
{
    next_vs_id=0;
    return(0);
}

/**
 * Allocate a bit number representing the virtual space identifier.
 * Return value less than zero is indicating a failure.
 * \param id Output variable storing the virtual space identifier.
 */
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
 * Check whether a bit array of the size \param bites can hold all of the
 * allocated virtual spaces. Return 1 if it can, 0 otherwise.
 * \param bites The available number of bits to be tested.
 */
int vs_is_enough( int bites )
{
    if ( next_vs_id > bites )
	    return(0);
    return(1);
}

/**
 * Remove all virtual spaces from the set.
 * \param to Set of virtual spaces that will be affected.
 */
void vs_clear( vs_t *to )
{
    memset((char*)to, 0x00, sizeof(vs_t)*VS_WORDS);
}

/**
 * Set each virtual space in the set.
 * \param to Set of virtual spaces that will be affected.
 */
void vs_fill( vs_t *to )
{
    memset((char*)to, 0xff, sizeof(vs_t)*VS_WORDS);
}

/**
 * Set the opposite value of each virtual space in the set.
 * \param to Set of virtual spaces that will be affected.
 */
void vs_invert( vs_t *to )
{
    for(int i=0;i<VS_WORDS;i++)
        to[i]= ~(to[i]);
}

/**
 * Copy one set of virtual spaces into another set.
 * \param from Set of virtual spaces that will be copied.
 * \param to Set of virtual spaces that will be set according to the \param from.
 */
void vs_set( const vs_t *from, vs_t *to )
{
    for(int i=0;i<VS_WORDS;i++)
        to[i]= from[i];
}

/**
 * Add virtual spaces; no virtual spaces will be removed.
 * \param from Set of virtual spaces that will be added.
 * \param to Set of virtual spaces that will be affected.
 */
void vs_add( const vs_t *from, vs_t *to )
{
    for(int i=0;i<VS_WORDS;i++)
        to[i]|= from[i];
}

/**
 * Remove one set of virtual spaces from another set.
 * \param from Set of virtual spaces that will be removed from the
 * set \param to.
 * \param to Set of virtual spaces that will be affected.
 */
void vs_sub( const vs_t *from, vs_t *to )
{
    for(int i=0;i<VS_WORDS;i++)
        to[i]&= ~(from[i]);
}

/**
 * Keep only those virtual spaces in the set \param to that are in the
 * set \param from.
 * \param from Set of virtual spaces that remains unchanged in the
 * set \param to.
 * \param to Set of virtual spaces that will be affected: all virtual
 * spaces that are not in the set \param from, are removed.
 */
void vs_mask( const vs_t *from, vs_t *to )
{
    for(int i=0;i<VS_WORDS;i++)
        to[i]&= from[i];
}

/**
 * Test the intersection of two sets of virtual spaces. If there is no
 * intersection, return 0, 1 otherwise (there is at least one common virtual
 * space).
 */
int vs_test( const vs_t *test, const vs_t *vs )
{
    /* ATENTION, see also object_cmp_vs() in object.c */
    for(int i=0;i<VS_WORDS;i++)
        if( vs[i] & test[i] )
            return(1);
    return(0);
}

int vs_issub( const vs_t *subset, const vs_t *set )
{
    for(int i=0;i<VS_WORDS;i++)
        if( (set[i] & subset[i]) != subset[i] )
            return(0);
    return(1);
}

/**
 * Test whether the set of virtual spaces is empty.
 * \param vs Set of virtual spaces that will be tested.
 */
int vs_isclear( const vs_t *vs )
{
    for(int i=0;i<VS_WORDS;i++)
        if( vs[i] )
            return(0);
    return(1);
}

/**
 * Test whether the set of virtual spaces is full.
 * \param vs Set of virtual spaces that will be tested.
 */
int vs_isfull( const vs_t *vs )
{
    for(int i=0;i<VS_WORDS;i++)
        if( vs[i]!=~((vs_t)0) )
            return(0);
    return(1);
}
