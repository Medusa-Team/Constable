/*
 * Constable: tree.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: tree.c,v 1.5 2002/12/16 11:51:17 marek Exp $
 */

#include "constable.h"
#include "tree.h"
#include "comm.h"
#include <regex.h>
#include "space.h"

/*
 * FIXME: Ak niekto chce vytvorit .*, mala by sa pod nou vytvorit este jedna .*
 */

static char *default_path=NULL;

static struct tree_type_s global_root_type={
		"/",
		sizeof(struct tree_s),
		NULL,
		NULL,
		NULL
};

/* FIXME: global_root by chcelo v ramci inicializacie nulovat */
struct tree_s global_root={NULL,NULL,NULL,NULL,NULL,&global_root_type,
				NULL,NULL,NULL,NULL};

/* ----------------------------- */

int tree_set_default_path( char *new )
{
	if( default_path!=NULL )
		free(default_path);
	if( (default_path=strdup(new))==NULL )
	{	error(Out_of_memory);
		return(-1);
	}
	return(0);
}

/* ---------- regexps ---------- */

static int isreg( char *name )
{ int unbs=0;
  char *n=name;
	while( *name )
	{	switch (*name)
		{	case '(':
			case ')':
			case '[':
			case ']':
			case '^':
			case '$':
			case '.':
			case '*':
			case '+':
			case '?':
			case '{':
			case '}':
			case '|':
				return 1;
			case '\\':
				if (
				    (name[1] >= 'A' && name[1] <= 'Z') ||
				    (name[1] >= 'a' && name[1] <= 'z') ||
				    name[1]==0
				    )
					return 1;
				unbs=1;
				name++;
		}
		name++;
	}
	if( unbs )
	{ int i=0;
		while( n[i] )
		{	if( n[i]=='\\' )
				i++;
			n[0]=n[i];
			n++;
		}
		n[0]=0;
	}
	return 0;
}

static void *regcompile( char *reg )
{ int l=strlen(reg);
  char tmp[l+3];
  regex_t *r;
	if( !strcmp(reg,".*") )
		return((void*)1);	/* speed up .* */
  	if( (r=malloc(sizeof(regex_t)))==NULL )
		return(NULL);
	tmp[0]='^';
	strcpy(tmp+1,reg);
	tmp[l+1]='$';
	tmp[l+2]=0;
	if( regcomp(r, tmp, REG_EXTENDED|REG_NOSUB )!=0 )
	{	error("Error in regexp '%s'", reg);
		free(r);
		return(NULL);
	}
	return(r);
}

static int regcmp( regex_t *r, char *name )
{
	if( r==(void*)1 )
		return(0);	/* speed up .* */
	if( regexec(r,name,0,NULL,0)==0 )
		return(0);
	return(-1);
}

/* ---------- tree ---------- */

static int pathcmp( char *name, char **path )
{ char *b;
	b=*path;
	while( *name!=0 && *name==*b )
	{	name++; b++;	}
	if( *name==0 && (*b==0 || *b=='/') )
	{	*path=b;
		return(0);
	}
	if( *b>*name )
		return(1);
	return(-1);
}

static struct tree_s *create_one_i( struct tree_s *base, char *name, int regexp )
{ struct tree_s *p;
  struct tree_type_s *type,*ctype;
  int l;
  
	if( *name==0 )		return(base);

	l=strlen(name);

	if( !regexp )
	{	for(p=base->child;p!=NULL;p=p->next)
			if( !strcmp(p->name,name) )
				return(p);
	}
	else
	{	for(p=base->reg;p!=NULL;p=p->next)
			if( !strcmp(p->name,name) )
				return(p);
	}
	if( (type=base->type->child_type)==NULL )
		type=base->type;
	if( (ctype=type->child_type)==NULL )
		ctype=type;
	if( (p=malloc(type->size+l+1))==NULL )
		return(NULL);
	memset(p,0,type->size+l+1);
	p->type=type;
	p->name=((char*)p)+(type->size);
	strncpy(p->name,name,l);
	p->name[l]=0;
	p->parent=base;
	p->child=NULL;
	p->reg=NULL;
	p->primary_space=NULL;
	if( (p->events=comm_new_array(sizeof(struct tree_event_s)))==NULL )
	{	free(p);
		return(NULL);
	}
	if( base->extra==(void*)1	/* speed up .* */
	    && base->child==NULL && base->reg==NULL
	    && !(regexp && !strcmp(p->name,".*"))
	    )
	{
		if( create_one_i(base,".*",1)==NULL )
		{	free(p);
			return(NULL);
		}
	}
	if( !regexp )
	{
		p->extra=NULL;	/* important */
		p->next=base->child;
		base->child=p;
	}
	else
	{
		p->extra=regcompile(p->name);
		p->next=base->reg;
		base->reg=p;
	}
	if( p->type->init )
		p->type->init(p);
	if( !regexp || p->extra!=(void*)1 )	/* speed up .* */
	{
		if( create_one_i(p,".*",1)==NULL )
			return(NULL);
	}
	return(p);
}

static struct tree_s *create_one( struct tree_s *base, char **name )
{ char *n;
  int l=0;
	while( **name=='/' )	(*name)++;
	n=*name;
	while( *n!=0 && *n!='/' )
	{	l++;
		n++;
	}
	{ char tmp[l+1];
		strncpy(tmp,*name,l);
		tmp[l]=0;
		*name=n;
		if( !isreg(tmp) )
			return(create_one_i(base,tmp,0));
		else
			return(create_one_i(base,tmp,1));
	}
}

struct tree_s *register_tree_type( tree_type_t *type )
{ char *path;
  struct tree_s *t;
	global_root.type->child_type=type;	/* !!! POZOR !!! */
	path=type->name;
	t=create_one(&global_root,&path);
	global_root.type->child_type=NULL;
	return(t);
}

struct tree_s *create_path( char *path )
{ struct tree_s *p;
	p=&global_root;
	if( *path=='/' )
	{ char *d;
		if( (d=default_path)==NULL )
		{	error("Invalid path begining with '/' without primary tree definition");
			return(NULL);
		}
		while( *d!=0 && (p=create_one(p,&d))!=NULL );
		if( p==NULL )
			return(p);
	}
	while( *path!=0 && (p=create_one(p,&path))!=NULL );
	return(p);
}

struct tree_s *find_one( struct tree_s *base, char *name )
{ struct tree_s *p;
	while( *name=='/' )	(name)++;
	if( *name==0 )		return(base);
	for(p=base->child;p!=NULL;p=p->next)
		if( !strcmp(p->name,name) )
			return(p);
	for(p=base->reg;p!=NULL;p=p->next)
		if( !regcmp(p->extra,name) )
			return(p);
/* ... zeby este cyklus base=base->alt ??? a lepsie to retazit cez alt */
	if( base->child==NULL && base->reg==NULL )
		return(base);
	return(NULL);
}

static struct tree_s *find_one2( struct tree_s *base, char **name )
{ struct tree_s *p;
  char *b;
  
	while( **name=='/' )	(*name)++;
	if( **name==0 )		return(base);
	for(p=base->child;p!=NULL;p=p->next)
		if( !pathcmp(p->name,name) )
			return(p);
	for(b=*name;*b!=0 && *b!='/';b++);
	{ char s[b-(*name)+1];
		strncpy(s,*name,b-(*name));
		s[b-(*name)]=0;
		for(p=base->reg;p!=NULL;p=p->next)
			if( !regcmp(p->extra,s) )
			{	*name=b;
				return(p);
			}
	}
	if( base->child==NULL && base->reg==NULL )
		return(base);
	return(NULL);
}
	
struct tree_s *find_path( char *path )
{ struct tree_s *p;
	p=&global_root;
	if( *path=='/' )
	{ char *d;
		if( (d=default_path)==NULL )
		{	error("Invalid path begining with '/' without primary tree definition");
			return(NULL);
		}
		while( *d!=0 && (p=find_one2(p,&d))!=NULL );
		if( p==NULL )
			return(p);
	}
	while( *path!=0 && (p=find_one2(p,&path))!=NULL );
	return(p);
}

/* ----------------------------------- */

/* tree_get_rnth: n=0: t n=1: t->parnt ... */
static struct tree_s *tree_get_rnth( struct tree_s *t, int n )
{ struct tree_s *p;
	for(p=t;n>0;p=p->parent)
		n--;
	return(p);
}

static void tree_for_alt_i( struct tree_s *t, struct tree_s *p, int n, void(*func)(struct tree_s *t, int arg), int arg )
{ struct tree_s *r,*a;
	if( n==0 )
	{	func(t,arg);
		return;
	}
	n--;
	r=tree_get_rnth(p,n);
	if( r->extra==NULL )
	{	for(a=t->child;a!=NULL;a=a->next)
			if( !strcmp(r->name,a->name) )
				tree_for_alt_i(a,p,n,func,arg);
	}
	else
	{	for(a=t->child;a!=NULL;a=a->next)
			if( !regcmp(r->extra,a->name) )
				tree_for_alt_i(a,p,n,func,arg);
		if( r->extra==1 /* .* speed up */ )
		{	for(a=t->reg;a!=NULL;a=a->next)
				tree_for_alt_i(a,p,n,func,arg);
		}
		else
		{	for(a=t->reg;a!=NULL;a=a->next)
				if( !strcmp(r->name,a->name) )
					tree_for_alt_i(a,p,n,func,arg);
		}
	}
}

void tree_for_alternatives( struct tree_s *p, void(*func)(struct tree_s *t, int arg), int arg )
{ int n;
  struct tree_s *t;
	for(n=0,t=p->parent;t!=NULL;t=t->parent)
		n++;
	tree_for_alt_i(tree_get_rnth(p,n),p,n,func,arg);
}

static void tree_is_equal_i( struct tree_s *p, int arg )
{
	if( p==*((struct tree_s **)arg) )
		*((struct tree_s **)arg)=NULL;
}

int tree_is_equal( struct tree_s *test, struct tree_s *p )
{ int n;
  struct tree_s *t;
	for(n=0,t=p->parent;t!=NULL;t=t->parent)
		n++;
	tree_for_alt_i(tree_get_rnth(p,n),p,n,tree_is_equal_i,&test);
	if( test==NULL )
		return(1);
	return(0);
}

static void tree_is_offspring_i( struct tree_s *ancestor, int arg )
{ struct tree_s *offspring;
	offspring=*((struct tree_s **)arg);
	while( offspring!=NULL )
	{	if( offspring==ancestor )
		{	*((struct tree_s **)arg)=NULL;
			break;
		}
		offspring=offspring->parent;
	}
}

int tree_is_offspring( struct tree_s *offspring, struct tree_s *ancestor )
{ int n;
  struct tree_s *t;
	for(n=0,t=ancestor->parent;t!=NULL;t=t->parent)
		n++;
	tree_for_alt_i(tree_get_rnth(ancestor,n),ancestor,n,tree_is_offspring_i,&offspring);
	if( offspring==NULL )
		return(1);
	return(0);
}


static void _add_event_handlers( struct event_hadler_hash_s **from, struct event_hadler_hash_s **to )
{ struct event_hadler_hash_s *h,*n;
  int l;
  	for(l=0;l<EHH_LISTS;l++)
	{	for(h=from[l];h!=NULL;h=h->next)
		{	if( (n=evhash_add(&(to[l]),h->handler,h->evname))==NULL )
				fatal("%s",errstr);
			vs_add(h->subject_vs,n->subject_vs);
			vs_add(h->object_vs,n->object_vs);
		}
	}
}

static void tree_node_merge( struct tree_s *r, struct tree_s *t )
{ struct tree_s *rc,*tc;

	_add_event_handlers(r->subject_handlers,t->subject_handlers);
	_add_event_handlers(r->object_handlers,t->object_handlers);
	for(rc=r->child;rc!=NULL;rc=rc->next)
	{	if( (tc=create_one_i(t,rc->name,0))!=NULL )
			tree_node_merge(rc,tc);
	}
	for(rc=r->reg;rc!=NULL;rc=rc->next)
	{	if( (tc=create_one_i(t,rc->name,1))!=NULL )
			tree_node_merge(rc,tc);
	}
}

static void tree_apply_alts( struct tree_s *dir )
{ struct tree_s *r,*t,*all=NULL;
	for(r=dir->reg;r!=NULL;r=r->next)
	{	if( r->extra==1 /* .* speed up */ )
			all=r;
		for(t=dir->child;t!=NULL;t=t->next)
		{	if( !regcmp(r->extra,t->name) )
				tree_node_merge(r,t);
		}
	}
	if( all!=NULL )
	{	for(t=dir->reg;t!=NULL;t=t->next)
		{	if( t!=all )
				tree_node_merge(all,t);
		}
	}
	for(t=dir->child;t!=NULL;t=t->next)
		tree_apply_alts(t);
	for(t=dir->reg;t!=NULL;t=t->next)
		tree_apply_alts(t);
}

int tree_expand_alternatives( void )
{
	tree_apply_alts( &global_root );
	return(0);
}

/* ----------------------------------- */

char *tree_get_path( struct tree_s *t )
{ static char buf[4096];
  static int pos=0;
	if( t->parent==NULL )
	{	buf[(pos=0)]=0;
		return(buf);
	}
	tree_get_path(t->parent);
	if( pos>0 )
		buf[pos++]='/';
	strncpy(buf+pos,t->name,4095-pos);
	pos+=strlen(t->name);
	if( pos>4095 )	pos=4095;
	buf[pos]=0;
	return(buf);
}

/* ----------------------------------- */

void tree_print_handlers(struct event_hadler_hash_s *h,char *name,void(*out)(int arg, char *),int arg)
{
	if( h!=NULL )
	{	out(arg,"  ");
		out(arg,name);
		out(arg,"=");
		while( h!=NULL )
		{	out(arg,"[");
			out(arg,h->handler->op_name);
#ifdef DEBUG_TRACE
			out(arg,":");
			out(arg,h->handler->op_name+MEDUSA_OPNAME_MAX);
#endif
			out(arg,"]");
			h=h->next;
		}
	}
}

void tree_print_node( struct tree_s *t, int level, void(*out)(int arg, char *), int arg )
{ char *s,buf[4096];
  int a,i;
  struct tree_s *p;
  static char *list_name_s[]={"s:vs_allov","s:vs_deny","s:notify_allow","s:notify_deny"};
  static char *list_name_o[]={"o:vs_allov","o:vs_deny","o:notify_allow","o:notify_deny"};
	s=tree_get_path(t);
	for(i=level;i>0;i--)
		out(arg,"    ");
	out(arg,"\"");
	out(arg,s);
	out(arg,"\"");
	if( t->primary_space!=NULL )
	{	out(arg," [");
		out(arg,t->primary_space->name);
		out(arg,"]");
	}
	out(arg," vs=");
	if( space_vs_to_str(t->vs[AT_MEMBER],buf,4096)<0 )
		out(arg,"???");
	else	out(arg,buf);
	for(a=1;a<NR_ACCESS_TYPES;a++)
	{	if( !vs_isclear(t->vs[a]) )
		{	out(arg," ");
			out(arg,access_names[a]);
			out(arg,"=");
			if( space_vs_to_str(t->vs[a],buf,4096)<0 )
				out(arg,"???");
			else	out(arg,buf);
		}
	}
	for(a=0;a<4;a++)
		tree_print_handlers(t->subject_handlers[a],list_name_s[a],out,arg);
	for(a=0;a<4;a++)
		tree_print_handlers(t->object_handlers[a],list_name_o[a],out,arg);
	out(arg,"\n");
	for(p=t->child;p!=NULL;p=p->next)
		tree_print_node(p,level+1,out,arg);
	for(p=t->reg;p!=NULL;p=p->next)
		tree_print_node(p,level+1,out,arg);
}

