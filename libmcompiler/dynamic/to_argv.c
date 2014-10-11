/* Library of dynamic structures  V1.0   11.2.1998	*/
/*	copyright (c)1998 by Marek Zelem		*/ 
/*	e-mail: marek@fornax.elf.stuba.sk		*/
/* $Id: to_argv.c,v 1.3 2002/10/23 10:25:44 marek Exp $	*/

#include <ctype.h>
#include <string.h>
#include <mcompiler/dynamic.h>
#include <mcompiler/backslash.h>

/* darg_argv2argv
 * n je maximalny pocet poloziek z argv. Ak n<0 tak ide az po NULL 
 */
darg_t *darg_argv2argv( char **argv, int n )
{ darg_t *da;
	if( (da=darg_alloc(0))==NULL )	return(NULL);
	while( *argv!=NULL && n!=0 )
	{	darg_append(da,*argv);
		argv++;
		if( n>0 )	n--;
	}
	return(da);
}

/*
 * darg_str2argv
 * if separator is 0 then separator is all white - space  characters
 *    and multiple separators are understand as one
 * else words separate only one separator.
 */
darg_t *darg_str2argv( char *line, char separator )
{ darg_t *da;
  char *p;
  int i;
  char c;
	if( (da=darg_alloc(0))==NULL )	return(NULL);
	p=line;
	while( *p!=0 )
	{	i=0;
		if( separator==0 )
		{	while( *p!=0 && isspace(*p) )	p++;
			while( p[i]!=0 && !isspace(p[i]) )	i++;
			if( i==0 )	continue;
		}
		else
		{	while( p[i]!=0 && p[i]!=separator )	i++;
		}
		c=p[i];
		p[i]=0;
		if( darg_append(da,p)<=0 )
		{	darg_free(da);
			return(NULL);
		}
		p[i]=c;
		p+=i;
		if( *p!=0 )	p++;
	}
	return(da);
}

darg_t *darg_line2argv( char *line )
{	return( darg_linevar2argv(line,NULL,NULL) );
}

/* darg_linevar2argv
 * skonvertuje riadok na pole argumentov darg_t
 * preklada backslash-e
 * preklada premenne pomovov getvar ak !=NULL inak nepreklada
 *	getvar vrati pointer na retazec,ktory predstavuje obsah premennej
 *		a do int* vrati pocet pouzitych znakov zo vstupneho char*
 *		alebo <0 ak je potrebne tuto funkciu este raz zavolat.
 *		pred prvym volanim musi byt *(int*)=0
 */
darg_t *darg_linevar2argv( char *line, char*(*getvar)(void*,char*,int*), void *gv_arg )
{ darg_t *da;
  ddata_t *dd;
  char *p;
  int i,m;
  char c;
  char *var;
  int lenvar;
/* !!! pozor tato funkcia je zavisla od formatov darg_t a ddata_t !!! */
	if( (da=darg_alloc(0))==NULL )	return(NULL);
	if( (dd=dd_alloc(1,128))==NULL )
	{	darg_free(da);	return(NULL);	}
	p=line;
	m= -1;
	while( *p!=0 )
	{
		c=*p;
		if( m<0 )
		{	while( *p!=0 && isspace(*p) )	p++;
			if( *p!=0 )	m=0;
			continue;
		}
		else if( m==0 && isspace(*p) )
		{	c=0;
			m= -1;
		}
		else if( m==0 && *p=='"' )
		{	m=2; p++; continue;	}
		else if( m==2 && *p=='"' )
		{	m=0; p++; continue;	}
		else if( m==0 && *p=='\\' )
		{ long l;
			c=backslash_parse(p,&l);
			p+=l-1;
		}
		else if( getvar!=NULL && *p=='$' )
		{	lenvar=0;
			while( (var=getvar(gv_arg,p,&lenvar))!=NULL )
			{	dd_append(dd,var,strlen(var));
				if( lenvar<0 )
				{	c=(m==0?0:' ');
					if( dd_append(dd,&c,1)<=0 )
					{	dd_free(dd);    darg_free(da);
						return(NULL);
					}
				}
				else
				{	p+=(lenvar>0?lenvar:1);
					break;
				}
			}
			if( var!=NULL )	continue;
		}
		if( dd_append(dd,&c,1)<=0 )
		{	dd_free(dd);	darg_free(da);
			return(NULL);
		}
		p++;
	}
	c=0;
	if( m>=0 && dd_append(dd,&c,1)<=0 )
	{	dd_free(dd);	darg_free(da);
		return(NULL);
	}
	da->bn=dd->n;
	da->blen=dd->len;
	da->buf=dd_extract(dd);
	for(i=0,p=da->buf;i < da->bn;i++)
		if( da->buf[i]==0 )
		{	if( darg_append(da,NULL)<=0 )
			{	darg_free(da);	return(NULL);	}
			darg_set(da,darg_argc(da)-1,p);
			p=&(da->buf[i])+1;
		}
	return(da);
}

