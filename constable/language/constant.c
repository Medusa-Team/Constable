/**
 * @file constant.c
 * @short Functions for language constants
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: constant.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include "language.h"
#include "execute.h"
#include "../constable.h"
#include "../space.h"

int load_constant( struct register_s *r , uintptr_t typ, char * name )
{
	/* init register */
	r->flags=0;
	r->object=NULL;
	r->data=r->buf;
	r->attr= &(r->tmp_attr);
	r->attr->offset=0;
	r->attr->length=sizeof(uintptr_t);
	r->attr->type=MED_TYPE_UNSIGNED;
	*((uintptr_t*)(r->data))=0;
	r->attr->name[0]=0;
	if( typ==Tspace )
	{ struct space_s *t;
		if( (t=space_find(name))==NULL )
		{	runtime("Undefined space %s",name);
			return(0);
		}
		r->data= (char*)(t->vs_id);
		r->attr->length=(VS_WORDS)*4;
		r->attr->type=MED_TYPE_BITMAP|MED_TYPE_READ_ONLY;
		return(1);
	}
	runtime("Unknown type of constant!");
	return(0);
}

