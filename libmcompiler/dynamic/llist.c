/* Library of dynamic structures  V1.0   6.12.1997	*/
/*	copyright (c)1997 by Marek Zelem		*/ 
/*	e-mail: marek@fornax.elf.stuba.sk		*/
/* $Id: llist.c,v 1.2 2002/10/23 10:25:44 marek Exp $	*/

#include <stdlib.h>
#include <mcompiler/dynamic.h>

void *ll_alloc(size_t len)
{ ll_t *l;
	if( (l=malloc(len+sizeof(ll_t)))==NULL )
		return(NULL);
	l->next=NULL;
	l->prev=NULL;
	l->type=0;
	l->len=len;
	return((void*)l);
}

void *ll_add(void *ll, size_t len)
{ ll_t *l2;
	if( (l2=malloc(len+sizeof(ll_t)))==NULL )
		return(NULL);
	l2->next=((ll_t *)ll)->next;
	l2->prev=(ll_t *)ll;
	((ll_t *)ll)->next=l2;
	if( l2->next!=NULL )
		l2->next->prev=l2;
	l2->type=0;
	l2->len=len;
	return((void*)l2);
}

void *ll_ins(void *ll, size_t len)
{ ll_t *l2;
	if( (l2=malloc(len+sizeof(ll_t)))==NULL )
		return(NULL);
	l2->next=(ll_t *)ll;
	l2->prev=((ll_t *)ll)->prev;
	((ll_t *)ll)->prev=l2;
	if( l2->prev!=NULL )
		l2->prev->next=l2;
	l2->type=0;
	l2->len=len;
	return((void*)l2);
}

void *ll_del(void *ll)
{ ll_t *l2;
	l2=NULL;
	if( ((ll_t *)ll)->prev!=NULL )
	{	((ll_t *)ll)->prev->next=((ll_t *)ll)->next;
		l2=((ll_t *)ll)->prev;
	}
	if( ((ll_t *)ll)->next!=NULL )
	{	((ll_t *)ll)->next->prev=((ll_t *)ll)->prev;
		l2=((ll_t *)ll)->next;
	}
	free(ll);
	return(l2);
}

void *ll_getfirst(void *ll)
{ ll_t *l2;
	l2=(ll_t *)ll;
	while( l2->prev!=NULL )	l2=l2->prev;
	return(l2);
}

void *ll_getlast(void *ll)
{ ll_t *l2;
	l2=(ll_t *)ll;
	while( l2->next!=NULL )	l2=l2->next;
	return(l2);
}

int ll_free(void *ll)
{ ll_t *l2;
	l2=ll_getfirst(ll);
	while( (l2=ll_del(l2))!=NULL );
	return(0);
}

