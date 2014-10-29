/**
 * @file save.c
 * @short 
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: save.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include "rbac.h"
#include "../space.h"
#include "../language/language.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

static char *at2str( int a )
{ lextab_t *l=keywords;
	while(l->keyword!=NULL)
	{	if( l->sym==Taccess && l->data==a )
			return(l->keyword);
		l++;
	}
	return("???");
}

static void rbac_save_role( FILE *f, struct role_s *r )
{ struct hierarchy_s *h;
  struct user_assignment_s *u;
  struct permission_assignment_s *p;
  int i;

//printf("ZZZ: role %s\n",r->name);
	if( r->flag )
		return;
//printf("ZZZ: saving\n");
	for(h=r->sub;h!=NULL;h=h->next_sub)
		rbac_save_role(f,h->sub_role);
	fprintf(f,"%s {\n",r->name);
	if( r->sub!=NULL )
		fprintf(f,"\t");
	for(h=r->sub;h!=NULL;h=h->next_sub)
	{	if( h->next_sub!=NULL )
			fprintf(f,"%s , ",h->sub_role->name);
		else
			fprintf(f,"%s;\n",h->sub_role->name);
	}
	if( r->ua!=NULL )
		fprintf(f,"\t");
	for(u=r->ua;u!=NULL;u=u->next_user)
	{	if( u->next_user!=NULL )
			fprintf(f,"\"%s\" , ",u->user->name);
		else
			fprintf(f,"\"%s\";\n",u->user->name);
	}
	for(p=r->perm;p!=NULL;p=p->next)
	{	for(i=0;i<p->n;i++)
			fprintf(f,"\t%s %s;\n",at2str(p->access[i]),
						p->space[i]->name);
	}
	fprintf(f,"}\n\n");
	r->flag=1;
}

static void rbac_save_roles( FILE *f )
{ struct role_s *r;
	for(r=rbac_roles;r!=NULL;r=r->next)
		r->flag=0;
	for(r=rbac_roles;r!=NULL;r=r->next)
		rbac_save_role(f,r);
}

static void file_rotate( char *filename, int limit )
{ char s[strlen(filename)+16];
  char t[strlen(filename)+16];
  	if( limit==0 )
	{	unlink(filename);
		return;
	}
	sprintf(s,"%s.%d",filename,limit);
	unlink(s);
	for(limit--;limit>0;limit--)
	{	sprintf(s,"%s.%d",filename,limit);
		sprintf(t,"%s.%d",filename,limit+1);
		rename(s,t);
	}
	rename(filename,s);
}

int rbac_save( char *file, int rotate )
{ FILE *f;
	file_rotate(file,rotate);
	if( (f=fopen(file,"w"))==NULL )
		return(-1);
	rbac_save_roles(f);
	fclose(f);
	return(0);
}

