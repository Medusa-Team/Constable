/**
 * @file force.c
 * @short Main file for preparing code for forcing
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: force.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include "../constable.h"
#include "../language/language.h"
#include "../language/execute.h"
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "linker.h"

extern char dloader_old[];
extern const int dloader_old_size;

extern char dloader[];
extern const int dloader_size;

#define	AV_MAX	256

struct fcs_s {
	struct fcs_s *next;
	char *name;
	struct fctab_s *code;
};

static struct fcs_s *fcs_tab = NULL;

static struct fcs_s *load_force_code(char *file)
{
	struct fcs_s *p;
	for(p=fcs_tab;p!=NULL;p=p->next) {
		if (p->name == file)
			return(p);
	}
	if( (p = malloc(sizeof(struct fcs_s)+strlen(file)+1))==NULL )
	{	runtime(Out_of_memory);
		return(NULL);
	}
	if( (p->code=object_to_string(file))==NULL ) {
		runtime("load_force_code: Format of file \"%s\" is invalid.",file);
		free(p);
		return(NULL);
	}
	p->name = (char*)(p+1);
	strcpy(p->name,file);
	p->next = fcs_tab;
	fcs_tab = p;
	return(p);
}

static BUILDIN_FUNC(cmd_force_code_old)
{
  struct register_s r;
  char *dest,*dest_begin;
  int size,l,flags;
  struct fcs_s *f;
  struct fctab_s *head;
  uintptr_t d;

	*((uintptr_t*)(ret->data))=0;
	if( !getarg(e,&r) || (r.attr->type&0x0f)!=MED_TYPE_BITMAP || (r.data==r.buf) )
	{	runtime("force_code(): 1st argument must be bitmap attribute of kobject");
		return(-1);
	}
	if( !(r.flags&OBJECT_FLAG_LOCAL) && r.attr->type & MED_TYPE_READ_ONLY )
	{	runtime("force_code(): Changing read-only varible");
		return(-1);
	}
	dest=dest_begin=reg_data(r);
	size=r.attr->length;
	flags=r.flags;
	if( !getarg(e,&r) || (r.attr->type & 0x0f)!=MED_TYPE_STRING )
	{	runtime("force_code(): 2nd argument must be string");
		return(-1);
	}
	if( (f=load_force_code(r.data))==NULL )
	{	
		return(-1);
	}
	if( dloader_old_size > size )
	{
too_big:	runtime("force_code(): code is too big");
		return(-1);
	}
	memcpy(dest, dloader_old, dloader_old_size);
	dest+=dloader_old_size;
	size-=dloader_old_size;
	if( (l=f->code->total_len) > size )
		goto too_big;
	memcpy(dest, f->code, l);
	head=(struct fctab_s *)dest;
	dest+=l;
	size-=l;
	head->argc=0;
	head->argv=dest_begin - dest;

	while( getarg(e,&r) )
	{	switch( (r.attr->type&0x0f) )
		{	case MED_TYPE_SIGNED:
			case MED_TYPE_UNSIGNED:
				object_get_val(r2o(&r),r.attr,&d,sizeof(d));
				d=byte_reorder_put_int32(flags,d);
				if( sizeof(d) > size )
					goto too_big;
				memcpy(dest, &d, sizeof(d));
				dest+=sizeof(d);
				size-=sizeof(d);
				head->argc++;
				break;
			default:
				runtime("force_code(): invalid argument");
				return(-1);
		}
	}
	head->total_len=byte_reorder_put_int32(flags,dest - dest_begin);
	head->argc=byte_reorder_put_int32(flags,head->argc);
	head->argv=byte_reorder_put_int32(flags,head->argv);
	*((uintptr_t*)(ret->data))= dest - dest_begin;
	return(0);
}

static BUILDIN_FUNC(cmd_force_code)
{
  struct register_s r;
  char *dest,*dest_begin;
  int size,l,flags,rvarpos,rvarend;
  struct fcs_s *f;
  struct fctab_s *head;
  uintptr_t d;

	*((uintptr_t*)(ret->data))=0;
	if( !getarg(e,&r) || (r.attr->type&0x0f)!=MED_TYPE_BITMAP || (r.data==r.buf) )
	{	runtime("force_code(): 1st argument must be bitmap attribute of kobject");
		return(-1);
	}
	if( !(r.flags&OBJECT_FLAG_LOCAL) && r.attr->type & MED_TYPE_READ_ONLY )
	{	runtime("force_code(): Changing read-only varible");
		return(-1);
	}
	dest=dest_begin=reg_data(r);
	size=r.attr->length;
	flags=r.flags;
	rvarpos=rvarend=size;
	if( !getarg(e,&r) || (r.attr->type & 0x0f)!=MED_TYPE_STRING )
	{	runtime("force_code(): 2nd argument must be string");
		return(-1);
	}
	if( (f=load_force_code(r.data))==NULL )
	{	
		return(-1);
	}
	if( dloader_size > size )
	{
too_big:	runtime("force_code(): code is too big");
		return(-1);
	}
	memcpy(dest, dloader, dloader_size);
	dest+=dloader_size;
	size-=dloader_size;
	if( (l=f->code->total_len) > size )
		goto too_big;
	memcpy(dest, f->code, l);
	head=(struct fctab_s *)dest;
	dest+=l;
	size-=l;
	head->argc=0;
	head->argv=dest_begin - dest;

	d=0;	/* end(/begin) of variables */
	d=byte_reorder_put_int32(flags,d);
	if( sizeof(d) > size )
		goto too_big;
	memcpy(dest, &d, sizeof(d));
	dest+=sizeof(d);
	size-=sizeof(d);

	while( getarg(e,&r) )
	{	l=r.attr->length;
		switch( (r.attr->type&0x0f) )
		{	case MED_TYPE_SIGNED:
			case MED_TYPE_UNSIGNED:
				object_get_val(r2o(&r),r.attr,&d,sizeof(d));
				d=byte_reorder_put_int32(flags,d);
				if( 2*sizeof(d) > size )
					goto too_big;
				memcpy(dest, &d, sizeof(d));
				dest+=sizeof(d);
				size-=sizeof(d);
				d=1;	/* don't reloc this variable */
				d=byte_reorder_put_int32(flags,d);
				memcpy(dest, &d, sizeof(d));
				dest+=sizeof(d);
				size-=sizeof(d);
				head->argc++;
				break;
			case MED_TYPE_STRING:
				l=strnlen(reg_data(r),l);
			case MED_TYPE_BITMAP:
				if( l > size )
					goto too_big;
				rvarpos-=l;
				object_get_val(r2o(&r),r.attr,dest_begin+rvarpos,l);
				d=rvarpos;
				d=byte_reorder_put_int32(flags,d);
				size-=l;
				if( 2*sizeof(d) > size )
					goto too_big;
				memcpy(dest, &d, sizeof(d));
				dest+=sizeof(d);
				size-=sizeof(d);
				d=2;	/* reloc this variable */
				d=byte_reorder_put_int32(flags,d);
				memcpy(dest, &d, sizeof(d));
				dest+=sizeof(d);
				size-=sizeof(d);
				head->argc++;
				break;
			default:
				runtime("force_code(): invalid argument");
				return(-1);
		}
	}
	if( size>0 )
	{ uintptr_t *pi,i;
		memmove(dest,dest_begin+rvarpos,rvarend-rvarpos);
		pi=(uintptr_t*)(dest_begin+head->argv+((head->argc)<<3));
		while( (i=byte_reorder_get_int32(flags,*pi--))!=0 )
		{	if( i==2 )
			{	*pi=byte_reorder_put_int32(flags,
				   byte_reorder_get_int32(flags,*pi)-size);
			}
			pi--;
		}
	}
	dest+=rvarend-rvarpos;
	head->total_len=byte_reorder_put_int32(flags,dest - dest_begin);
	head->argc=byte_reorder_put_int32(flags,head->argc);
	head->argv=byte_reorder_put_int32(flags,head->argv);
	*((uintptr_t*)(ret->data))= dest - dest_begin;
	return(0);
}


int force_init(void)
{
/*
	{ int i;
		for(i=0;i<dloader_size;i++)
			printf("0x%02x ",(unsigned char)dloader[i]);
		printf("\n");
	}
*/
	lex_addkeyword("force_code_int",Tbuildin,(uintptr_t)cmd_force_code_old);
	lex_addkeyword("force_code",Tbuildin,(uintptr_t)cmd_force_code);
	return 0;
}

