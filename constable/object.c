/*
 * Constable: object.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: object.c,v 1.3 2002/10/23 10:25:43 marek Exp $
 */

#include "constable.h"
#include "object.h"
#include <sys/param.h>

int object_is_invalid( struct object_s *o )
{
	if( o->class->cinfo_size>0 && ((u_int32_t*)(o->data+o->class->cinfo_offset))[0]!=0 )
		return(0);
	if( o->class->subject.cinfo_size>0 && ((u_int32_t*)(o->data+o->class->subject.cinfo_offset))[0]!=0 )
		return(0);
	return(1);
}

int object_do_sethandler( struct object_s *o )
{ struct class_handler_s *h;
	object_clear_all_vs(o);
	object_clear_event(o);
	for(h=o->class->classname->class_handler;h!=NULL;h=h->next)
	{	if( h->set_handler )
			h->set_handler(h,o->class->comm,o);
	}
	return(0);
}

int object_is_same( struct object_s *a, struct object_s *b )
{ struct class_s *c;
  struct medusa_attribute_s *t;
	if( (c=a->class) != b->class )
		return(0);
	for(t=c->attr;t->type!=MED_TYPE_END;t++)
//{if( (t->type & MED_TYPE_PRIMARY_KEY) )
//printf("PK \"%s\" %d ? %d\n",t->name,
//*((int*)(a->data+t->offset)),
//*((int*)(b->data+t->offset)) );
		if( (t->type & MED_TYPE_PRIMARY_KEY)
		     && memcmp(a->data+t->offset,b->data+t->offset,t->length) )
			return(0);
//}
	return(1);
}


int object_clear_event( struct object_s *o )
{
	if( o->class->event_size>0 )
		memset(o->data + o->class->event_offset,0,o->class->event_size);
	/* subject */
	if( o->class->subject.event_size>0 )
		memset(o->data + o->class->subject.event_offset,0,o->class->subject.event_size);
	return(0);
}

int object_add_event( struct object_s *o, event_mask_t *e )
{ int i,n,r;
  char *f,*t;
	r= -1;
	/* object */
	if( (n=o->class->event_size)>0 )
	{
		if( n>sizeof(e->bitmap) )
			n=sizeof(e->bitmap);
		f=e[1].bitmap;
		t=o->data + o->class->event_offset;
		for(i=0;i<n;i++)
			t[i] |= f[i];
		r=0;
	}
	/* subject */
	if( (n=o->class->subject.event_size)>0 )
	{
		if( n>sizeof(e->bitmap) )
			n=sizeof(e->bitmap);
		f=e[0].bitmap;
		t=o->data + o->class->subject.event_offset;
		for(i=0;i<n;i++)
			t[i] |= f[i];
		r=0;
	}
	return(r);
}

int object_get_vs( vs_t *vs, int n, struct object_s *o )
{ struct class_handler_s *h;
	if( n<0 || n>=NR_ACCESS_TYPES )
	{	vs_clear(vs);
		return(-1);
	}
	if( o->class->vs_attr[n]!=NULL )
	{	object_get_val(o,o->class->vs_attr[n],vs,MAX_VS_BITS/8);
		return(0);
	}
	vs_clear(vs);
	for(h=o->class->classname->class_handler;h!=NULL;h=h->next)
	{	if( h->get_vs )
			h->get_vs(h,o->class->comm,o,vs,n);
	}
	return(0);
}

int object_cmp_vs( vs_t *vs, int n, struct object_s *o )
{ struct class_handler_s *h;
  vs_t vs_tmp[MAX_VS_BITS/32];
  int i;
	if( n<0 || n>=NR_ACCESS_TYPES )
		return(0);
	if( o->class->vs_attr[n]!=NULL )
	{	i=MIN(o->class->vs_attr[n]->length/sizeof(vs_t),MAX_VS_BITS/(8*sizeof(vs_t)));
		while(i>0)
		{	i--;
			if( ((vs_t *)(o->data+o->class->vs_attr[n]->offset))[i]
				& vs[i] )
				 return(1);
		}
		return(0);
	}
	vs_clear(vs_tmp);
	for(h=o->class->classname->class_handler;h!=NULL;h=h->next)
	{	if( h->get_vs )
			h->get_vs(h,o->class->comm,o,vs_tmp,n);
	}
	return(vs_test(vs,vs_tmp));
}

int object_clear_all_vs( struct object_s *o )
{ int n;
	for(n=0;n<NR_ACCESS_TYPES;n++)
	{	if( o->class->vs_attr[n]!=NULL )
			memset(o->data+o->class->vs_attr[n]->offset,0,o->class->vs_attr[n]->length);
	}
	return(0);
}

int object_add_vs( struct object_s *o, int n, vs_t *vs )
{ int i;
	if( n<0 || n>=NR_ACCESS_TYPES )
		return(-1);
	if( o->class->vs_attr[n]!=NULL )
	{	i=MIN(o->class->vs_attr[n]->length/sizeof(vs_t),MAX_VS_BITS/(8*sizeof(vs_t)));
		while(i>0)
		{	i--;
			((vs_t *)(o->data+o->class->vs_attr[n]->offset))[i]
				|= vs[i];
		}
		return(0);
	}
	return(-1);
}

#include "comm.h"
#include <stdio.h>
void object_print( struct object_s *o, void(*out)(void *arg, char *), void *arg )
{ int i,j,bp;
  struct medusa_attribute_s *a;
  char buf[1024];
  unsigned long tmp;
  unsigned long long tmpl;
	out(arg,"[\"");
	out(arg,o->class->comm->name);
	out(arg,"\"]: object ");
	out(arg,o->attr.name);
	out(arg," of class ");
	out(arg,o->class->m.name);
	out(arg,"={\n");
	a=&(o->class->attr[0]);
	for(i=0;a[i].type!=MED_TYPE_END;i++)
	{	out(arg,"\t");
		out(arg,a[i].name);
		out(arg,"=");
		switch( a[i].type & 0x0f )
		{	case MED_COMM_TYPE_UNSIGNED:
				if( a[i].length > sizeof(tmp) )
				{	object_get_val(o,a+i,&tmpl,sizeof(tmpl));
					sprintf(buf,"0x%llx",tmpl);
				}
				else
				{	object_get_val(o,a+i,&tmp,sizeof(tmp));
					sprintf(buf,"0x%lx",tmp);
				}
				out(arg,buf);
				break;
			case MED_COMM_TYPE_SIGNED:
				if( a[i].length > sizeof(tmp) )
				{	object_get_val(o,a+i,&tmpl,sizeof(tmpl));
					sprintf(buf,"%lld",tmpl);
				}
				else
				{	object_get_val(o,a+i,&tmp,sizeof(tmp));
					sprintf(buf,"%ld",tmp);
				}
				out(arg,buf);
				break;
			case MED_COMM_TYPE_STRING:
				out(arg,"\"");
				for(j=0,bp=0;j<a[i].length;j++)
				{	if( bp>=sizeof(buf)-8 )
					{	buf[bp]=0;
						out(arg,buf);
						bp=0;
					}
					buf[bp]=(o->data+a[i].offset)[j];
					if( buf[bp]==0 )
						break;
					else if( buf[bp]=='\\' )
						buf[++bp]='\\';
					else if( buf[bp]<' ' || buf[bp]>'~' )
					{ switch( buf[bp] )
					  {	case '\n':
							buf[bp++]='\\';
							buf[bp]='n';
							break;
						case '\t':
							buf[bp++]='\\';
							buf[bp]='t';
							break;
						case '\a':
							buf[bp++]='\\';
							buf[bp]='a';
							break;
						default:
							sprintf(buf+bp,"\\x%02x",buf[bp]);
							bp+=3;
					  }
					}
					bp++;
				}
				if( bp>0 )
				{	buf[bp]=0;
					out(arg,buf);
				}
				out(arg,"\"");
				break;
			case MED_COMM_TYPE_BITMAP:
#ifdef BITMAP_DIPLAY_LEFT_RIGHT
				for(j=0;j<a[i].length;j++)
				{	if( j>0 && (j&0x03)==0 )
						out(arg,":");
					sprintf(buf,"%02x",((unsigned char*)(o->data+a[i].offset))[j]);
					out(arg,buf);
				}
#else
				for(j=a[i].length-1;j>=0;j--)
				{	sprintf(buf,"%02x",((unsigned char*)(o->data+a[i].offset))[j]);
					out(arg,buf);
					if( j>0 && (j&0x03)==0 )
						out(arg,":");
				}
#endif
				break;
		}
		out(arg,"\n");
	}
	out(arg,"}\n");
}



