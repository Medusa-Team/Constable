/**
 * @file execute.c
 * @short Runtime interpreter of internal pre-compiled handlers
 * from configuration file.
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: execute.c,v 1.3 2002/10/23 10:25:43 marek Exp $
 */

#include "execute.h"
#include "language.h"
#include "variables.h"
#include "../constable.h"
#include "../comm.h"
#include "../threading.h"

/* stack format:
e->pos->
		local data
		....
e->base->	local_vars
		e->base
        	return address
		nr_vars
		arguments
		.....
		.....
		local_vars
		e->base
		------BEGIN------ 
 */

#define	LTI	0
#define	LTS	1
#define	LTP	2
#define	LTO	3
#define	LTa	4

struct medusa_attribute_s execute_attr_int={ 0,4,MED_TYPE_SIGNED|MED_TYPE_READ_ONLY,"" };
struct medusa_attribute_s execute_attr_str={ 0,MAX_REG_SIZE,MED_TYPE_STRING|MED_TYPE_READ_ONLY,"" };
struct medusa_attribute_s execute_attr_pointer={ 0,sizeof(void*),MED_TYPE_UNSIGNED|MED_TYPE_READ_ONLY,"" };

void obj_to_reg( struct register_s *r, struct object_s *o, char *attr )
{
	/* init register */
	r->flags=o->flags;
	r->object=NULL;
	r->class=NULL;
	if( attr==NULL )
	{	r->attr=&(o->attr);
		r->object=o;
		r->class=o->class;
		r->data=o->data;
		return;
	}
	if( (r->attr=get_attribute(o->class,attr))==NULL )
	{	runtime("Variable %s has no attribute %s",o->attr.name,attr);
		r->attr=&execute_attr_int;
		r->data=r->buf;
		*((uintptr_t*)(r->buf))=0;
		return;
	}
	r->data=o->data;
	return;
}

#define	pop()	execute_pop(e)
#define	push(d)	execute_push(e,(d))
#define	r_pop(r)	R_pop(e,(r))
#define	r_push(r)	R_push(e,(r))

void R_pop( struct execute_s *e, struct register_s *r )
{ uintptr_t x;
  int i,n;
	/* init register */
	r->flags=OBJECT_FLAG_LOCAL;
	r->object=NULL;
	r->class=NULL;
	x=pop();
	switch( x )
	{	case LTI:
			r->attr=&execute_attr_int;
			r->data=r->buf;
			*((uintptr_t*)(r->data))=pop();
			return;
		case LTS:
//			r->attr=&execute_attr_str;
			r->tmp_attr=execute_attr_str;
			r->data=(char*)pop();
			r->tmp_attr.length=strlen( (r->data) )+1;
			r->attr=&(r->tmp_attr);
			return;
		case LTP:
			r->attr=&execute_attr_pointer;
			r->data=r->buf;
			*((uintptr_t*)(r->data))=pop();
			return;
		case LTO:
			obj_to_reg(r,(struct object_s*)(pop()),NULL);
			return;
		default:
			if( x==LTa )
			{	r->attr=&(r->tmp_attr);
				for(i=0;i<(sizeof(r->tmp_attr)+3)>>2;i++)
					((uint32_t*)(r->attr))[i]=pop();
			}
			else
				r->attr=(struct medusa_attribute_s *)x;
			if( r->attr->type==MED_TYPE_END )
				r->class=(struct class_s*)(pop());
			x=pop();
			if( x!=0 )
			{	r->data=(char*)x;
				return;
			}
			r->data=r->buf;
			n= (r->attr->length + 3) >> 2;
			for(i=0;i<n;i++)
				((uint32_t*)(r->data))[i]=pop();
			return;
	}
}

void R_push( struct execute_s *e, struct register_s *r )
{ int n;
	if( r->attr==&execute_attr_int )
	{	push( *((uintptr_t*)(r->data)) );
		push( LTI );
	}
	else if( r->attr==&execute_attr_pointer )
	{	push( *((uintptr_t*)(r->data)) );
		push( LTP );
	}
	else if( r->data!=r->buf && r->object!=NULL )
	{	push( (uintptr_t)r->object );
		push( LTO );
	}
	else if( r->attr==&execute_attr_str && r->data!=r->buf )
	{	push( (uintptr_t)(r->data) );
		push( LTS );
	}
	else
	{	if( r->data==r->buf )
		{	n= (r->attr->length + 3) >> 2;
			for(n--;n>=0;n--)
				push( ((uint32_t*)(r->data))[n] );
			push( 0 );
		}
		else
			push( (uintptr_t)(r->data) );
		if( r->attr->type==MED_TYPE_END )
			push((uintptr_t)(r->class));
		if( r->attr==&(r->tmp_attr) )
		{	for(n=((sizeof(r->tmp_attr)+3)>>2)-1;n>=0;n--)
				push( ((uint32_t*)(r->attr))[n] );
			push( LTa );
		}
		else
			push( (uintptr_t)(r->attr) );
	}
}

static int fn_getsval( struct execute_s *e, struct register_s *r )
{ int z_stack_pos,i,n;
	e->keep_stack=1;
	z_stack_pos=e->pos;
	n=pop();
	if( r==NULL )
	{	e->pos=z_stack_pos;
		e->keep_stack=0;
		return(n);
	}
	if( e->fn_getsval_p>=n )
	{	e->pos=z_stack_pos;
		e->keep_stack=0;
		return(0);
	}
	for(i=n-e->fn_getsval_p-1;i>=0;i--)
	{	r_pop(r);
	}
	e->pos=z_stack_pos;
	e->fn_getsval_p++;
//	if( r->object==NULL )	/* lebo buildin funkcie potrebuju kobjekty */
//		r_imm(r);
	e->keep_stack=0;
	return(1);
}

static int fn_getargsval( struct execute_s *e, struct register_s *r, int i )
{ int z_stack_pos,n;
	if( i<=0 )
		return(0);
	e->keep_stack=1;
	z_stack_pos=e->pos;
	e->pos=e->base;
	pop();
	pop();
	n=pop();
	i=n-i;
	if( i<0 )
	{	e->pos=z_stack_pos;
		e->keep_stack=0;
		return(0);
	}
	for(;i>=0;i--)
	{	r_pop(r);
	}
	e->pos=z_stack_pos;
	r_imm(r);
	e->keep_stack=0;
	return(1);
}

static struct object_s *data_find_var( struct execute_s *e, char *name )
{ struct object_s *o;

	if( e->c->operation.class!=NULL
	    && !strncmp(e->c->operation.attr.name,name,MEDUSA_ATTRNAME_MAX) )
		return(&(e->c->operation));
	else if( e->c->subject.class!=NULL
	    && !strncmp(e->c->subject.attr.name,name,MEDUSA_ATTRNAME_MAX) )
		return(&(e->c->subject));
	else if( e->c->object.class!=NULL
	    && !strncmp(e->c->object.attr.name,name,MEDUSA_ATTRNAME_MAX) )
		return(&(e->c->object));
	else
	{ uintptr_t p;
#ifdef LOCAL_VARS_RECURSIVE_SEARCH
		for(p=e->base;p!=0;p=execute_readstack(e,p-1))
#else
		p=e->base;
#endif
		   if( (o=get_var((struct object_s*)(execute_readstack(e,p)),name))!=NULL )
			return(o);
	}
	if( (o=get_var(e->c->local_vars,name))!=NULL )
		return(o);
	runtime("Undefined variable %s",name);
	return(NULL);
}

void reg_load_var( struct register_s *r, struct execute_s *e, char *name, char *attr )
{ struct object_s *o=NULL;

	/* init register */
	r->flags=0;
	r->object=NULL;
	r->class=NULL;
	if( attr==NULL )
	{	if( e->c->operation.class!=NULL
		    && (r->attr=get_attribute(e->c->operation.class,name))!=NULL )
		{	r->data=e->c->operation.data;
			r->flags=e->c->operation.flags;
			return;
		}
		if( e->c->subject.class!=NULL
		    && (r->attr=get_attribute(e->c->subject.class,name))!=NULL )
		{	r->data=e->c->subject.data;
			r->flags=e->c->subject.flags;
			return;
		}
		if( e->c->object.class!=NULL
		    && (r->attr=get_attribute(e->c->object.class,name))!=NULL )
		{	r->data=e->c->object.data;
			r->flags=e->c->object.flags;
			return;
		}
	}
	if( (o=data_find_var(e,name))==NULL )
	{	r->attr=&execute_attr_int;
		r->data=r->buf;
		*((uintptr_t*)(r->buf))=0;
		return;
	}
	obj_to_reg(r,o,attr);
}

// static struct register_s r0,r1;
static pthread_key_t r0_key, r1_key;

char *execute_get_last_data( void )
{
    struct register_s *r0 = pthread_getspecific(r0_key);
	return (char*)(r0->data);
}

struct medusa_attribute_s *execute_get_last_attr( void )
{
    struct register_s *r0 = pthread_getspecific(r0_key);
	return(r0->attr);
}

static int execute_handler_do( struct execute_s *e )
{ uintptr_t cmd=oNOP,x;
  struct object_s *v;
  int i;
  uintptr_t *cmd_p;
  struct register_s *r0 = pthread_getspecific(r0_key);
  struct register_s *r1 = pthread_getspecific(r1_key);

#ifdef DEBUG_TRACE
    char *runtime_file;
    runtime_file = (char*) pthread_getspecific(runtime_file_key);
	strncpy(runtime_file, e->h->op_name+MEDUSA_OPNAME_MAX, 
            sizeof(RUNTIME_FILE_TYPE));
	runtime_file[sizeof(RUNTIME_FILE_TYPE)-1]=0;
#endif
for(;;)
{	cmd_p=e->p;
	cmd=*(e->p)++;
#ifdef DEBUG_TRACE
	x=*(e->p)++;
	//snprintf(runtime_pos,sizeof(runtime_pos)-1,"%d:%d",((x>>16)&0x0000ffff),(x&0x0000ffff));
    char *runtime_pos = (char*) pthread_getspecific(runtime_pos_key);
	snprintf(runtime_pos,sizeof(RUNTIME_POS_TYPE)-1,"%p",(void*)x);
	runtime_pos[sizeof(RUNTIME_POS_TYPE)-1]=0;
//runtime("ZZZ %04x",cmd);
#endif
	switch( cmd )
	{
	case oNOP:	break;
	case oLTI:	push(LTI);
			break;
	case oLTS:	push(LTS);
			break;
	case oLTP:	push(LTP);
			break;
	case oLDI:	push(*(e->p)++);
			break;
	case oLD0:	push(0);
			push(LTI);
			break;
	case oLD1:	push(1);
			push(LTI);
			break;
	case oDUP:
            r_pop(r0);
			r_push(r0);
			r_push(r0);
			break;
	case oSWP:
            r_pop(r0);
			r_pop(r1);
			r_push(r0);
			r_push(r1);
			break;
	case oDEL:
            r_pop(r0);
			break;
	case oDEn:	r_pop(r1);
			x=pop();
			while( x )
			{	r_pop(r0);
				x--;
			}
			r_push(r1);
			break;
	case oIMM:
            r_pop(r0);
			r_imm(r0);
			r_push(r0);
			break;
	case oSTO:
            r_pop(r0);
			r_pop(r1);
			r_sto(r1,r0);
			r_push(r1);
			break;
	case oFET:	if( (v=data_find_var(e,(char *)(*(e->p)++)))!=NULL )
			{	if( (i=v->class->comm->fetch_object(v->class->comm,e->cont,v,e->my_comm_buff))<0 )
				{	runtime("Can't fetch object");
					push(0);
				}
				else if( i>0 )
				{	e->cont=i;
					e->p=cmd_p;
					return(i);
				}
				else	push(1);
				push(LTI);
			}
			else
			{	push(0);
				push(LTI);
			}
			break;
	case oUPD:	if( (v=data_find_var(e,(char *)(*(e->p)++)))!=NULL )
			{	if( (i=v->class->comm->update_object(v->class->comm,e->cont,v,e->my_comm_buff))<0 )
				{	runtime("Can't update object");
					push(0);
				}
				else if( i>0 )
				{	e->cont=i;
					e->p=cmd_p;
					return(i);
				}
				else	push(1);
				push(LTI);
			}
			else
			{	push(0);
				push(LTI);
			}
			break;
	case oLDC:	x=*(e->p)++;
			load_constant(r0,x,(char*)(*(e->p)++));
			r_push(r0);
			break;
	case oLDV:	reg_load_var(r0,e,(char*)(*(e->p)++),NULL);
			r_push(r0);
			break;
	case oLDS:	x=*(e->p)++;
			reg_load_var(r0,e,(char*)x,(char*)(*(e->p)++));
			r_push(r0);
			break;
	case oATR:	x=*(e->p)++;
			r_pop(r0);
			if( r0->object==NULL )
			{	runtime("Referencing attribute in non object");
				push(0);
				push(LTI);
				break;
			}
			obj_to_reg(r0,r0->object,(char*)x);
			r_push(r0);
			break;
	case oNEW:	x=*(e->p)++;
			r_pop(r0);
			if( r0->attr!=&execute_attr_pointer
				||  (*((uintptr_t*)(r0->data)))==0
				|| ((struct class_names_s*)(*((uintptr_t*)(r0->data))))->classes[e->comm->conn]==NULL )
			{	runtime("Invalid variable type");
				*((uintptr_t*)(r0->data))=0; /* NULL */
				r_push(r0); /* len aby nieco bolo */
				break;
			}
			if( (v=alloc_var((char*)(x),NULL,((struct class_names_s*)(*((uintptr_t*)(r0->data))))->classes[e->comm->conn]))==NULL )
				runtime("Can't allocate varible",((struct class_names_s*)((struct class_names_s*)(*((uintptr_t*)(r0->data)))))->name);
			*((uintptr_t*)(r0->data))=(uintptr_t)v;
			r_push(r0);
			break;
	case oALI:	x=*(e->p)++;
			r_pop(r0);
			if( (r0->attr->type & 0x0f)!=MED_TYPE_STRING
				||  r0->data==NULL )
			{	runtime("Invalid name of aliased variable");
				r0->attr=&execute_attr_pointer;
				r0->data=r0->buf;
				*((uintptr_t*)(r0->data))=0; /* NULL */
				r_push(r0); /* len aby nieco bolo */
				break;
			}
			if( (v=data_alias_var((char*)(x),data_find_var(e,(char *)(r0->data))))==NULL )
				runtime("Can't alias varible");
			/* init register */
			r0->flags=0;
			r0->object=NULL;
			r0->class=NULL;
			r0->attr=&execute_attr_pointer;
			r0->data=r0->buf;
			*((uintptr_t*)(r0->data))=(uintptr_t)v;
			r_push(r0);
			break;
	case oVRL:	r_pop(r0);
			if( r0->attr!=&execute_attr_pointer
			    || (v=(struct object_s*)(*((uintptr_t*)(r0->data))))==NULL )
			{	r_push(r0); /* len aby nieco bolo */
				break;
			}
			var_link((struct object_s**)(execute_stack_pointer(e,e->base)),v);
			obj_to_reg(r0,v,NULL);
			r_push(r0);
			break;
	case oVRT:	r_pop(r0);
			if( r0->attr!=&execute_attr_pointer
			    || (v=(struct object_s*)(*((uintptr_t*)(r0->data))))==NULL )
			{	r_push(r0); /* len aby nieco bolo */
				break;
			}
			var_link(&(e->c->local_vars),v);
			obj_to_reg(r0,v,NULL);
			r_push(r0);
			break;
	case oTOF:	r_pop(r0);
			if( r0->class==NULL )
			{	runtime("Invalid typeof() argument");
				push(0);
			}
			else	push((uintptr_t)(r0->class->classname->name));
			push(LTS);
			break;
	case oCOF:	r_pop(r0);
			if( r0->class==NULL )
			{	runtime("Invalid commof() argument");
				push(0);
			}
			else	push((uintptr_t)(r0->class->comm->name));
			push(LTS);
	case oS2C:	r_pop(r0);
			if( (r0->attr->type & 0x0f)!=MED_TYPE_STRING )
			{	runtime("Name of class must be identifier or string");
				push(0);
			}
			else if( (x=(uintptr_t)(get_class_by_name((char*)r0->data)))==0 )
			{	runtime("Class '%s' does not exist",r0->data);
				push(0);
			}
			else	push(x);
			push(LTP);
			break;
	case oSCS:	r_pop(r0);
			if( (r0->attr->type & 0x0f)!=MED_TYPE_STRING )
			{	runtime("Name of connection must be string");
				break;
			}
			if( (e->comm=comm_find((char*)r0->data))!=NULL )
				break;
	case oSCD:	e->comm=e->my_comm_buff->comm;
			break;
	case oADD:
	case oSUB:
	case oMUL:
	case oDIV:
	case oMOD:
	case oSHL:
	case oSHR:
	case oLT:
	case oGT:
	case oLE:
	case oGE:
	case oEQ:
	case oNE:
	case oOR:
	case oAND:
	case oXOR:
			r_pop(r0);
			r_pop(r1);
			do_bin_op(cmd,r1,r0);
			r_push(r1);
			break;
	case oNOT:	r_pop(r0);
			r_not(r0);
			r_push(r0);
			break;
	case oNEG:	r_pop(r0);
			r_imm(r0);
			r_neg(r0);
			r_push(r0);
			break;

	case oJR:	(e->p)+= *(e->p)+1;	/* (e->p)+= *(e->p)++; */
			break;
	case oJIZ:	r_pop(r0);
			if( ! r_nz(r0) )
				(e->p)+= *(e->p)+1; /* (e->p)+= *(e->p)++; */
			else	(e->p)++;
			break;
	case oJNZ:	r_pop(r0);
			if( r_nz(r0) )
				(e->p)+= *(e->p)+1; /* (e->p)+= *(e->p)++; */
			else	(e->p)++;
			break;
	case oJSR:	x=*(e->p)++;
			x=*((uintptr_t*)x);
			//printf("execute_handler_do: %p JSR %lx\n",(e->p)-2,x);
			if( x==0 )
			{	runtime("Call undefined function");
				push(0);
				push(LTI);
				break;
			}
			push((uintptr_t)(e->p));
			push(e->base);
			e->base=e->pos;
			push(0);	/* local_vars */
			(e->p)=(uintptr_t*)x;
			break;
	case oRET:	
			//printf("execute_handler_do: %p RET\n",(e->p)-1);
			r_pop(r0);
			r_imm(r0);	/* POZOR! mohla by byt z local_vars */
			free_vars((struct object_s**)(execute_stack_pointer(e,e->base)));
			pop(); /* local_vars */
			e->base=pop();
			if( e->pos <= e->start )
			{	
				e->c->result=r_int(r0);
				e->pos=e->start;
				free_vars(&(e->c->local_vars));
				return( 0 );
			}
			
			(e->p)= (uintptr_t*) pop();
			r_push(r0);
			break;
	case oARG:	x=*(e->p)++;
			if( !fn_getargsval(e,r0,x) )
				runtime("Invalid argument $%d",x);
			r_push(r0);
			break;
	case oBIN:	x=*(e->p)++;
			e->fn_getsval_p=0;
			/* init register */
			r0->flags=0;
			r0->object=NULL;
			r0->class=NULL;
			r0->attr=&execute_attr_int; r0->data=r0->buf;
			*((uintptr_t*)(r0->data))= -1;
			i=((buildin_t)(x))(e,r0,fn_getsval);
			if( i>0 )
			{	e->cont=i;
				e->p=cmd_p;
				return(i);
			}
			r_push(r0);
			break;
	default:	runtime("Illegal instruction 0x%08x 0x%08x 0x%08x",cmd,(e->p)[0],(e->p)[1]);
	}
	e->cont=0;
}
}

int execute_handler( struct comm_buffer_s *comm_buff, struct event_handler_s *h, struct event_context_s *c )
{ int i;
	if( comm_buff->execute.h!=NULL && (comm_buff->execute.h!=h || comm_buff->execute.c!=c) )
	{	comm_buf_to_queue(&(comm_buff->execute.my_comm_buff->to_wake),comm_buff);
		return(2);
	}
//	//if( comm_buff->do_phase==0 )
	if( comm_buff->execute.h==NULL )
	{	comm_buff->execute.h=h;
		comm_buff->execute.c=c;
		comm_buff->execute.comm=comm_buff->comm;
		comm_buff->execute.my_comm_buff=comm_buff;

		comm_buff->execute.c->result=RESULT_OK;
		comm_buff->execute.p=(uintptr_t *)(comm_buff->execute.h->data);
		comm_buff->execute.cont=0;
		comm_buff->execute.keep_stack=0;
		comm_buff->execute.start=comm_buff->execute.pos;
		execute_push(&comm_buff->execute,comm_buff->execute.base);
		comm_buff->execute.base=comm_buff->execute.pos;
		execute_push(&comm_buff->execute,0);	/* local_vars */
		//printf("execute_handler: for %s\n",h->op_name);
	}
	if( (i=execute_handler_do(&comm_buff->execute))<=0 )
		comm_buff->execute.h=NULL;
	return(i);
}

int execute_init( int n )
{
	execute_init_stacks(n);
    TLS_CREATE(&r0_key, NULL);
    TLS_CREATE(&r1_key, NULL);
	return(0);
}

/**
 * Allocates thread-specific memory for registers used in the virtual machine.
 */
int execute_registers_init(void)
{
    void *data;
    TLS_ALLOC(r0_key, struct register_s);
    TLS_ALLOC(r1_key, struct register_s);
    return 0;
}
