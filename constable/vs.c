/*
 * Constable: vs.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: vs.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include "constable.h"
#include "vs.h"
#include "space.h" // only for ANON_SPACE_NAME
#include <pthread.h>

//static int number_of_vs=MAX_VS_BITS/32;
static struct vs_s vs_tab;
static int vs;

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
    for(i=0;i<NUMBER_OF_VS;i++)
        vs_tab.vs[i]=0;
    /*
    if( max_vs > MAX_VS_BITS )
    {	number_of_vs=MAX_VS_BITS/32;
        char **errstr = (char**) pthread_getspecific(errstr_key);
        *errstr=Out_of_memory;
        return(1);
    }
    number_of_vs= max_vs/32;
*/
    vs=0;
    return(0);
}

/* ---------- vs alloc/free ---------- */
struct vs_s *vs_alloc( char *name )
{ struct vs_s *p;
    if( vs >= MAX_VS_BITS )
    {
        char **errstr = (char**) pthread_getspecific(errstr_key);
        *errstr=Out_of_memory;
        return(NULL);
    }
    for(p=&vs_tab;p->next!=NULL;p=p->next)
    {	if( name[0]!=ANON_SPACE_NAME[0] && !strcmp(name,p->next->name) )
        {	char **errstr = (char**) pthread_getspecific(errstr_key);
            *errstr=Space_already_defined;
            return(NULL);
        }
    }
    if( (p->next=calloc(1,sizeof(struct vs_s)+strlen(name)+1))==NULL )
    {	char **errstr = (char**) pthread_getspecific(errstr_key);
        *errstr=Out_of_memory;
        return(NULL);
    }
    p->next->next=NULL;
    strcpy(p->next->name,name);
    p->next->vs[vs/BITS_PER_VS] |= 1 << (vs % BITS_PER_VS);
    vs += 1;
    return(p->next);
}

int vs_is_enough( int bites )
{ struct vs_s *p;
    int i,n;
    n=bites/32;
    for(p=&vs_tab;p->next!=NULL;p=p->next);
    for(i=0;i<NUMBER_OF_VS;i++,n--)
    {	if( n<=0 && p->vs[i]!=0 )
            return(0);
    }
    return(1);
}

void vs_clear( vs_t *to )
{ int i;
    for(i=0;i<NUMBER_OF_VS;i++)
        to[i]= 0;
}

void vs_fill( vs_t *to )
{ int i;
    for(i=0;i<NUMBER_OF_VS;i++)
        to[i]= ~((vs_t)0);
}

void vs_invert( vs_t *to )
{ int i;
    for(i=0;i<NUMBER_OF_VS;i++)
        to[i]= ~(to[i]);
}

void vs_set( const vs_t *from, vs_t *to )
{ int i;
    for(i=0;i<NUMBER_OF_VS;i++)
        to[i]= from[i];
}

/**
 * Add virtual spaces, no virtual spaces will be removed.
 * \param from Set of virtual spaces that will be added
 * \param to Set of virtual spaces that will be changed
 */
void vs_add( const vs_t *from, vs_t *to )
{ int i;
    for(i=0;i<NUMBER_OF_VS;i++)
        to[i]|= from[i];
}

void vs_sub( const vs_t *from, vs_t *to )
{ int i;
    for(i=0;i<NUMBER_OF_VS;i++)
        to[i]&= ~(from[i]);
}

void vs_mask( const vs_t *from, vs_t *to )
{ int i;
    for(i=0;i<NUMBER_OF_VS;i++)
        to[i]&= from[i];
}

int vs_test( const vs_t *test, const vs_t *vs )
{ int i;
    /* ATENTION, see also object_cmp_vs() in object.c */
    for(i=0;i<NUMBER_OF_VS;i++)
        if( vs[i] & test[i] )
            return(1);
    return(0);
}

int vs_issub( const vs_t *test, vs_t *vs )
{ int i;
    for(i=0;i<NUMBER_OF_VS;i++)
        if( (vs[i] & test[i]) != test[i] )
            return(0);
    return(1);
}


int vs_isclear( const vs_t *vs )
{ int i;
    for(i=0;i<NUMBER_OF_VS;i++)
        if( vs[i] )
            return(0);
    return(1);
}

int vs_isfull( const vs_t *vs )
{ int i;
    for(i=0;i<NUMBER_OF_VS;i++)
        if( vs[i]!=~((vs_t)0) )
            return(0);
    return(1);
}


