/*
 * Constable: class.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: class.c,v 1.3 2002/10/23 10:25:43 marek Exp $
 */

#include "constable.h"
#include "object.h"
#include "comm.h"

#include <stdio.h>

static struct class_names_s *classes=NULL;

struct class_names_s *get_class_by_name( char *name )
{ struct class_names_s *c;
	for(c=classes;c!=NULL;c=c->next)
		if( !strncmp(c->name,name,MEDUSA_CLASSNAME_MAX) )
			return( c );
	if( (c=malloc(sizeof(struct class_names_s)+strlen(name)+1))==NULL )
		return(NULL);
	if( (c->classes=(struct class_s**)(comm_new_array(sizeof(struct class_s*))))==NULL )
	{	free(c);
		return(NULL);
	}
	c->name=(char*)(c+1);
	strcpy(c->name,name);
	c->class_handler=NULL;
	c->next=classes;
	classes=c;
	return(c);
}

int class_free_all_clases( struct comm_s *comm )
{ struct class_names_s *c;
	for(c=classes;c!=NULL;c=c->next)
	{	if( c->classes[comm->conn]!=NULL )
		{
			free( c->classes[comm->conn] );
			c->classes[comm->conn]=NULL;
		}
	}
	return(0);
}

struct class_s *add_class( struct comm_s *comm, struct medusa_class_s *mc, struct medusa_attribute_s *a )
{ struct class_s *c;
  int i,l;
  struct medusa_attribute_s *ci;
  struct class_names_s *p;
	if( a==NULL )	a=(struct medusa_attribute_s *)(mc+1);
	for(i=0;a[i].type!=MED_TYPE_END;i++);
	i++;
	l=sizeof(struct class_s)+i*sizeof(struct medusa_attribute_s);
	if( (c=malloc(l))==NULL )
		return(NULL);
	c->cinfo_mask = 0;
	c->m=*mc;
	memcpy(c->attr,a,i*sizeof(struct medusa_attribute_s));

	if( (ci=get_attribute(c,"o_cinfo"))!=NULL )
	{	c->cinfo_offset=ci->offset;
		c->cinfo_size=(ci->length)/sizeof(u_int32_t);
	}
	else	c->cinfo_offset=c->cinfo_size=0;
	if( (ci=get_attribute(c,"name"))!=NULL )
	{	c->name_offset=ci->offset;
		c->name_size=ci->length;
	}
	else	c->name_offset=c->name_size=0;
	if( (ci=get_attribute(c,"med_oact"))!=NULL )
	{	c->event_offset=ci->offset;
		c->event_size=ci->length;
	}
	else	c->event_offset=c->event_size=0;
	c->vs_attr[AT_MEMBER]=get_attribute(c,"vs");
	/* subject */
	c->vs_attr[AT_RECEIVE]=get_attribute(c,"vsr");
	c->vs_attr[AT_SEND]=get_attribute(c,"vsw");
	c->vs_attr[AT_SEE]=get_attribute(c,"vss");
	c->vs_attr[AT_CREATE]=get_attribute(c,"vsc");
	c->vs_attr[AT_ERASE]=get_attribute(c,"vsd");
	c->vs_attr[AT_ENTER]=get_attribute(c,"vse");
	c->vs_attr[AT_CONTROL]=get_attribute(c,"vsx");

	if( c->vs_attr[AT_MEMBER]!=NULL
	    && !vs_is_enough(c->vs_attr[AT_MEMBER]->length*8) )
		comm->conf_error(comm,"Not enough VSs");
	if( (ci=get_attribute(c,"med_sact"))!=NULL )
	{	c->subject.event_offset=ci->offset;
		c->subject.event_size=ci->length;
	}
	else	c->subject.event_offset=c->subject.event_size=0;

	if( (ci=get_attribute(c,"s_cinfo"))!=NULL )
	{	c->subject.cinfo_offset=ci->offset;
		c->subject.cinfo_size=(ci->length)/sizeof(u_int32_t);
	}
	else	c->subject.cinfo_offset=c->subject.cinfo_size=0;
	c->subject.cinfo_mask=0;
	
	if( (p=get_class_by_name(c->m.name))==NULL )
	{	free(c);
		return(NULL);
	}
	c->classname=p;
	c->comm=comm;
	p->classes[comm->conn]=c;
	hash_add(&(comm->classes),&(c->hashent),c->m.classid);
	if( debug_def_out!=NULL )
	{	debug_def_out(debug_def_arg,"REGISTER ");
		class_print(c,debug_def_out,debug_def_arg);
	}
	return(c);
}

/* vrati offset (u_int16_t), alebo -1 ak chyba */
int class_alloc_object_cinfo( struct class_s *c )
{ int i;
	for(i=0;i<c->cinfo_size && i<sizeof(c->cinfo_mask)*8;i++)
	{	if( !(c->cinfo_mask & (1<<i)) )
		{	c->cinfo_mask|= 1<<i;
			return(c->cinfo_offset+i*sizeof(u_int32_t));
		}
	}
	return(-1);
}

/* vrati offset (u_int16_t), alebo -1 ak chyba */
int class_alloc_subject_cinfo( struct class_s *c )
{ int i;
	for(i=0;i<c->subject.cinfo_size && i<sizeof(c->subject.cinfo_mask)*8;i++)
	{	if( !(c->subject.cinfo_mask & (1<<i)) )
		{	c->subject.cinfo_mask|= 1<<i;
			return(c->subject.cinfo_offset+i*sizeof(u_int32_t));
		}
	}
	return(-1);
}

int class_add_handler( struct class_names_s *c, struct class_handler_s *handler )
{ struct class_handler_s **p;
	for(p=&(c->class_handler);*p!=NULL;p=&((*p)->next));
	handler->next=NULL;
	*p=handler;
	return(0);
}

struct medusa_attribute_s *get_attribute( struct class_s *c, char *name )
{ struct medusa_attribute_s *a;
	if( c==NULL )	return(NULL);
	for(a=c->attr;a->type!=MED_TYPE_END;a++)
		if( !strncmp(a->name,name,MEDUSA_ATTRNAME_MAX) )
			return(a);
	return(NULL);
}

int class_comm_init( struct comm_s *comm )
{ struct class_names_s *c;
  struct class_handler_s *h;
	for(c=classes;c!=NULL;c=c->next)
		for(h=c->class_handler;h!=NULL;h=h->next)
			if( h->init_comm )
				if( h->init_comm(h,comm) < 0 )
					return(-1);
	return(0);
}

void attr_print( struct medusa_attribute_s *a, void(*out)(int arg, char *), int arg )
{ int i;
  static char *t[]={"END","unsigned","signed","string","bitmap"};
  char buf[32];
	for(i=0;a[i].type!=MED_TYPE_END;i++)
	{	out(arg,"\t");
		if( a[i].type&0x80 )
			out(arg,"readonly ");
		if( a[i].type&0x40 )
			out(arg,"primary ");
		out(arg,t[a[i].type & 0x0f]);
		out(arg,"\t");
		out(arg,a[i].name);
		out(arg,"\t");
		sprintf(buf,"(%d: %d)",a[i].offset,a[i].length);
		out(arg,buf);
		out(arg,"\n");
	}
}

void class_print( struct class_s *c, void(*out)(int arg, char *), int arg )
{
	out(arg,"[\"");
	out(arg,c->comm->name);
	out(arg,"\"] class ");
	out(arg,c->m.name);
	out(arg," {\n");
	attr_print(&(c->attr[0]),out,arg);
	out(arg,"}\n");
}


