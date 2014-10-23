/**
 * @file alu.c
 * @short functions of Arithmetic and Logic Unit
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: alu.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include <sys/types.h>
// #include <asm/bitops.h>
#include "../constable.h"
#include "../medusa_object.h"
#include "execute.h"
#include "language.h"
#include <sys/param.h>

/* !!!!!!!!!!!!!!!!!!!!!! */
typedef	int32_t	s_int32_t;
typedef	int64_t	s_int64_t;

#define TYPE_u	MED_TYPE_UNSIGNED
#define TYPE_s	MED_TYPE_SIGNED
#define TYPE_b	MED_TYPE_BITMAP



#define OPbb(name,op) \
static void r_##name##_bb(struct register_s * v, struct register_s * d) \
{						\
	int i,n,nv,nd;				\
						\
	nv = v->attr->length;			\
	nd = d->attr->length;			\
	n = MAX((nv+3)>>2, (nd+3)>>2)<<2;	\
	r_resize(v,n);				\
	r_resize(d,n);				\
	v->tmp_attr.length=n;			\
	v->tmp_attr.type=TYPE_b;		\
	v->attr = &(v->tmp_attr);		\
						\
	for (i=0; i < n; i++)			\
		((unsigned char *)(v->data))[i] op (((unsigned char *)(d->data))[i]);	\
}

OPbb(add,|=)
OPbb(sub,&=~)
OPbb(and,&=)
OPbb(or,|=)
OPbb(xor,^=)

#define OPbx(name,t2,op) \
static void r_##name##_b##t2(struct register_s * v, struct register_s * d) \
{						\
	t2##_int32_t i, nv;			\
						\
	i = ((t2##_int32_t *)(d->data))[0];	\
	nv = 8 * v->attr->length;		\
	if (i>nv) {				\
		runtime("Cannot set bit %d of %d-bit long bitfield", i, nv);	\
		return;				\
	}					\
						\
	op((char*)(v->data),i);		\
}

OPbx(add,s,setbit)
OPbx(add,u,setbit)
OPbx(sub,s,clrbit)
OPbx(sub,u,clrbit)

#define OPi(name,t1,t2,t3,op)	\
static void r_##name##_##t1##t2( struct register_s *v, struct register_s *d ) \
{ int nv,nd,n;							\
	nv=v->attr->length;					\
	nd=d->attr->length;					\
	n= MAX( (nv+3)>>2 , (nd+3)>>2 );			\
	r_resize(v,n<<2);					\
	r_resize(d,n<<2);					\
	v->tmp_attr.length=n<<2;				\
	v->tmp_attr.type=TYPE_##t3;				\
	v->attr=&(v->tmp_attr);					\
	if( n==1 )						\
	{  t1##_int32_t x;					\
	   t2##_int32_t y;					\
		x= ((t1##_int32_t*)(v->data))[0];		\
		y= ((t2##_int32_t*)(d->data))[0];		\
		((t3##_int32_t*)(v->data))[0]=(t3##_int32_t)(op);\
	}							\
	else							\
	{  t1##_int64_t x;					\
	   t2##_int64_t y;					\
		x= ((t1##_int64_t*)(v->data))[0];		\
		y= ((t2##_int64_t*)(d->data))[0];		\
		((t3##_int64_t*)(v->data))[0]=(t3##_int64_t)(op);\
	}							\
}

#define ROPi(name,t1,t2,op)	\
static void r_##name##_##t1##t2( struct register_s *v, struct register_s *d ) \
{ int nv,nd,n;							\
	nv=v->attr->length;					\
	nd=d->attr->length;					\
	n= MAX( (nv+3)>>2 , (nd+3)>>2 );			\
	r_resize(v,n<<2);					\
	r_resize(d,n<<2);					\
	v->tmp_attr.length=4;					\
	v->tmp_attr.type=MED_TYPE_UNSIGNED;			\
	v->attr=&(v->tmp_attr);					\
	if( n==1 )						\
	{  t1##_int32_t x;					\
  	   t2##_int32_t y;					\
		x= ((t1##_int32_t*)(v->data))[0];		\
		y= ((t2##_int32_t*)(d->data))[0];		\
		((u_int32_t*)(v->data))[0]=(u_int32_t)(op);	\
	}							\
	else							\
	{  t1##_int64_t x;					\
  	   t2##_int64_t y;					\
		x= ((t1##_int64_t*)(v->data))[0];		\
		y= ((t2##_int64_t*)(d->data))[0];		\
		((u_int32_t*)(v->data))[0]=(u_int32_t)(op);	\
	}							\
}

#define ROPb(name,op) \
static void r_##name##_bb(struct register_s * v, struct register_s * d)	\
{								\
	int nv, nd, n,i;					\
	nv=v->attr->length;					\
	nd=d->attr->length;					\
	n= MAX( (nv+3)>>2 , (nd+3)>>2 );			\
	r_resize(v,n<<2);					\
	r_resize(d,n<<2);					\
	v->tmp_attr.length=4;					\
	v->tmp_attr.type=MED_TYPE_UNSIGNED;			\
	v->attr=&(v->tmp_attr);					\
	for (i=0; i<n; i++)					\
		if (!((((unsigned char *)(v->data))[i]) op (((unsigned char *)(d->data))[i])))	\
		{						\
			((u_int32_t*)(v->data))[0]=0;		\
			return;					\
		}						\
	((u_int32_t*)(v->data))[0]=1;				\
}

#define ROPc(name,op) \
static void r_##name##_cc(struct register_s * v, struct register_s * d)	\
{								\
	int nv, nd, i;						\
	char a,b;						\
	nv=v->attr->length;					\
	nd=d->attr->length;					\
	v->tmp_attr.length=4;					\
	v->tmp_attr.type=MED_TYPE_SIGNED;			\
	v->attr=&(v->tmp_attr);					\
	for (i=0; i<nv && i<nd ; i++)				\
	{	b=((char*)(d->data))[i];			\
		if( (a=((char*)(v->data))[i])==0 || b==0 || a!=b )	\
			break;					\
	}							\
	if( i>=nv )	a=0;					\
	if( i>=nd )	b=0;					\
	((u_int32_t*)(v->data))[0]=(u_int32_t)(a op b);		\
}

OPi(add,u,u,u,x+y)
OPi(add,u,s,s,x+y)
OPi(add,s,u,s,x+y)
OPi(add,s,s,s,x+y)

OPi(sub,u,u,s,x-y)
OPi(sub,u,s,s,x-y)
OPi(sub,s,u,s,x-y)
OPi(sub,s,s,s,x-y)

OPi(mul,u,u,u,x*y)
OPi(mul,u,s,s,x*y)
OPi(mul,s,u,s,x*y)
OPi(mul,s,s,s,x*y)

OPi(div,u,u,u,x/y)
OPi(div,u,s,s,x/y)
OPi(div,s,u,s,x/y)
OPi(div,s,s,s,x/y)

OPi(mod,u,u,u,x%y)
OPi(mod,u,s,s,x%y)
OPi(mod,s,u,s,x%y)
OPi(mod,s,s,s,x%y)

OPi(shl,u,u,u,x<<y)
OPi(shl,u,s,u,x<<y)
OPi(shl,s,u,s,x<<y)
OPi(shl,s,s,s,x<<y)

OPi(shr,u,u,u,x>>y)
OPi(shr,u,s,u,x>>y)
OPi(shr,s,u,s,x>>y)
OPi(shr,s,s,s,x>>y)

OPi(and,u,u,u,x & y)
OPi(and,u,s,s,x & y)
OPi(and,s,u,s,x & y)
OPi(and,s,s,s,x & y)

OPi(or,u,u,u,x | y)
OPi(or,u,s,s,x | y)
OPi(or,s,u,s,x | y)
OPi(or,s,s,s,x | y)

OPi(xor,u,u,u,x ^ y)
OPi(xor,u,s,s,x ^ y)
OPi(xor,s,u,s,x ^ y)
OPi(xor,s,s,s,x ^ y)

ROPi(lt,u,u,x < y)
ROPi(lt,u,s,x < y)
ROPi(lt,s,u,x < y)
ROPi(lt,s,s,x < y)
ROPc(lt,<)

ROPi(gt,u,u,x > y)
ROPi(gt,u,s,x > y)
ROPi(gt,s,u,x > y)
ROPi(gt,s,s,x > y)
ROPc(gt,>)

ROPi(le,u,u,x <= y)
ROPi(le,u,s,x <= y)
ROPi(le,s,u,x <= y)
ROPi(le,s,s,x <= y)
ROPc(le,<=)

ROPi(ge,u,u,x >= y)
ROPi(ge,u,s,x >= y)
ROPi(ge,s,u,x >= y)
ROPi(ge,s,s,x >= y)
ROPc(ge,>=)

ROPi(eq,u,u,x == y)
ROPi(eq,u,s,x == y)
ROPi(eq,s,u,x == y)
ROPi(eq,s,s,x == y)
ROPb(eq,==)
ROPc(eq,==)

ROPi(ne,u,u,x != y)
ROPi(ne,u,s,x != y)
ROPi(ne,s,u,x != y)
ROPi(ne,s,s,x != y)
ROPb(ne,!=)
ROPc(ne,!=)

void r_neg( struct register_s *v )
{ int n,i;
	n= (v->attr->length+3)>>2;
	for(i=0;i<n;i++)
		((u_int32_t*)(v->data))[i]= ~ ((u_int32_t*)(v->data))[i];
}

void r_not( struct register_s *v )
{ int n,i;
	n= (v->attr->length+3)>>2;
	v->tmp_attr.length=4;
	v->tmp_attr.type=MED_TYPE_UNSIGNED;
	v->attr=&(v->tmp_attr);
	for(i=0;i<n;i++)
		if( ((u_int32_t*)(v->data))[i] )
		{	((u_int32_t*)(v->data))[0]=0;
			return;
		}
	((u_int32_t*)(v->data))[0]=1;
}

int r_nz( struct register_s *v )
{ int n,i;
	n= (v->attr->length+3)>>2;
	for(i=0;i<n;i++)
		if( ((u_int32_t*)(v->data))[i] )
			return(1);
	return(0);
}

/* ----------- strings ----------- */

static void r_add_cc( struct register_s *v, struct register_s *d )
{ int l,x;
	l=v->attr->length;
	l=strnlen(v->data,l);
	x=strnlen(d->data,d->attr->length);
	if( v->attr != &(v->tmp_attr) )
	{	v->tmp_attr = *(v->attr);
		v->attr=&(v->tmp_attr);
	}
	if( (v->attr->length=l+x+1) < MAX_REG_SIZE )
	{	memcpy(v->data+l,d->data,v->attr->length-l-1);
		v->data[v->attr->length-1]=0;
	}
	else
	{	memcpy(v->data+l,d->data,MAX_REG_SIZE-l);
		v->attr->length=MAX_REG_SIZE;
	}
}

#define OPci(t2,f)	\
static void r_add_c##t2( struct register_s *v, struct register_s *d )	\
{ int l,nd;							\
  t2##_int64_t y;						\
	l=v->attr->length;					\
	l=strnlen(v->data,l);					\
	if( v->attr != &(v->tmp_attr) )				\
	{	v->tmp_attr = *(v->attr);			\
		v->attr=&(v->tmp_attr);				\
	}							\
	nd=(d->attr->length)>>2;				\
	if( nd==1 )						\
		y= ((t2##_int32_t*)(d->data))[0];		\
	else							\
		y= ((t2##_int64_t*)(d->data))[0];		\
	if( (v->attr->length=l+nd*10+1) < MAX_REG_SIZE )	\
		sprintf(v->data+l,f,y);				\
	else							\
		v->attr->length=MAX_REG_SIZE;			\
}

OPci(u,"%Lu")
OPci(s,"%Ld")

static void r_add_cb( struct register_s *v, struct register_s *d )
{ int l,n,j;
	l=v->attr->length;
	l=strnlen(v->data,l);
	if( v->attr != &(v->tmp_attr) )
	{	v->tmp_attr = *(v->attr);
		v->attr=&(v->tmp_attr);	
	}
	n=(d->attr->length);
	if( (l+n+n+(n>>2)+1) >= MAX_REG_SIZE )
		return;
#ifdef BITMAP_DIPLAY_LEFT_RIGHT
	for(j=0;j<n;j++)
	{	if( j>0 && (j&0x03)==0 )
			v->data[l++]=':';
		sprintf(v->data+l,"%02x",d->data[j]);
		l+=2;
	}
#else
	for(j=n-1;j>=0;j--)
	{	sprintf(v->data+l,"%02x",d->data[j]);
		l+=2;
		if( j>0 && (j&0x03)==0 )
			v->data[l++]=':';
	}
#endif
	v->data[l++]=0;
	v->attr->length=l;
}


/* ---------------- table -------------- */

/* op, [uscb] , [uscb] */

static void(*op_func[16][4][4])(struct register_s *v, struct register_s *d)={
	{{r_add_uu,r_add_us,NULL,NULL},{r_add_su,r_add_ss,NULL,NULL},
		{r_add_cu,r_add_cs,r_add_cc,r_add_cb},{r_add_bu,r_add_bs,NULL,r_add_bb}},
	{{r_sub_uu,r_sub_us,NULL,NULL},{r_sub_su,r_sub_ss,NULL,NULL},
		{NULL,NULL,NULL,NULL},{r_sub_bu,r_sub_bs,NULL,r_sub_bb}},
	{{r_mul_uu,r_mul_us,NULL,NULL},{r_mul_su,r_mul_ss,NULL,NULL},
		{NULL,NULL,NULL,NULL},{NULL,NULL,NULL,NULL}},
	{{r_div_uu,r_div_us,NULL,NULL},{r_div_su,r_div_ss,NULL,NULL},
		{NULL,NULL,NULL,NULL},{NULL,NULL,NULL,NULL}},
	{{r_mod_uu,r_mod_us,NULL,NULL},{r_mod_su,r_mod_ss,NULL,NULL},
		{NULL,NULL,NULL,NULL},{NULL,NULL,NULL,NULL}},
	{{r_shl_uu,r_shl_us,NULL,NULL},{r_shl_su,r_shl_ss,NULL,NULL},
		{NULL,NULL,NULL,NULL},{NULL,NULL,NULL,NULL}},
	{{r_shr_uu,r_shr_us,NULL,NULL},{r_shr_su,r_shr_ss,NULL,NULL},
		{NULL,NULL,NULL,NULL},{NULL,NULL,NULL,NULL}},
	{{r_lt_uu,r_lt_us,NULL,NULL},{r_lt_su,r_lt_ss,NULL,NULL},
		{NULL,NULL,r_lt_cc,NULL},{NULL,NULL,NULL,NULL}},
	{{r_gt_uu,r_gt_us,NULL,NULL},{r_gt_su,r_gt_ss,NULL,NULL},
		{NULL,NULL,r_gt_cc,NULL},{NULL,NULL,NULL,NULL}},
	{{r_le_uu,r_le_us,NULL,NULL},{r_le_su,r_le_ss,NULL,NULL},
		{NULL,NULL,r_le_cc,NULL},{NULL,NULL,NULL,NULL}},
	{{r_ge_uu,r_ge_us,NULL,NULL},{r_ge_su,r_ge_ss,NULL,NULL},
		{NULL,NULL,r_ge_cc,NULL},{NULL,NULL,NULL,NULL}},
	{{r_eq_uu,r_eq_us,NULL,NULL},{r_eq_su,r_eq_ss,NULL,NULL},
		{NULL,NULL,r_eq_cc,NULL},{NULL,NULL,NULL,r_eq_bb}},
	{{r_ne_uu,r_ne_us,NULL,NULL},{r_ne_su,r_ne_ss,NULL,NULL},
		{NULL,NULL,r_ne_cc,NULL},{NULL,NULL,NULL,r_ne_bb}},
	{{r_or_uu,r_or_us,NULL,NULL},{r_or_su,r_or_ss,NULL,NULL},
		{NULL,NULL,NULL,NULL},{NULL,NULL,NULL,r_or_bb}},
	{{r_and_uu,r_and_us,NULL,NULL},{r_and_su,r_and_ss,NULL,NULL},
		{NULL,NULL,NULL,NULL},{NULL,NULL,NULL,r_and_bb}},
	{{r_xor_uu,r_xor_us,NULL,NULL},{r_xor_su,r_xor_ss,NULL,NULL},
		{NULL,NULL,NULL,NULL},{NULL,NULL,NULL,r_xor_bb}},
};

void do_bin_op( int op, struct register_s *v, struct register_s *d )
{ int t1,t2;
  static const char *opname[]={"+", "-", "*", "/", "%", "<<", ">>",
  			"<", ">", "<=", ">=", "==", "!=", "|", "&", "^"};
  static const char *typename[]={"unsigned", "signed", "string", "bitmap"};
	if( v->data!=v->buf )
		r_imm(v);
	if( d->data!=d->buf )
		r_imm(d);
	t1= (v->attr->type&0x0f) - MED_TYPE_UNSIGNED;
	t2= (d->attr->type&0x0f) - MED_TYPE_UNSIGNED;
	if( t1<0 || t1>3 || t2<0 || t2>3 )
	{	runtime("Illegal operand type");
		return;
	}
	if( op_func[op-oADD][t1][t2]==NULL )
	{	runtime("Illegal operation %s between %s and %s",opname[op-oADD],typename[t1],typename[t2]);
		return;
	}
	op_func[op-oADD][t1][t2](v,d);
}

