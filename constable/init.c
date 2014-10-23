/*
 * Constable: init.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: init.c,v 1.3 2002/10/23 10:25:43 marek Exp $
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sched.h>
#include <signal.h>
#include <string.h>

#include "event.h"
#include "comm.h"
#include "init.h"

#include "space.h"
#include "tree.h"

#ifndef MEDUSA_INITNAME
#define MEDUSA_INITNAME	"/sbin/init"
#endif

int procmem_init( void );
int vs_init( void );
int language_init( char *filename );
int execute_init( int n );
int cmds_init( void );
int force_init( void );
int mcp_init( char *filename );

int language_do( void );

char *medusa_config_file="/etc/medusa.conf";

static int test=0;

static struct module_s *first_module=NULL;
static struct module_s *active_modules=NULL;

int add_module( struct module_s *module )
{ struct module_s **p;
	for(p=&first_module;*p!=NULL;p=&((*p)->next))
		if( !strcmp((*p)->name,module->name) )
			return(-1);
	module->next=NULL;
	*p=module;
	return(0);
}

struct module_s *activate_module( char *name )
{ struct module_s *m,**p;
	for(p=&first_module;*p!=NULL;p=&((*p)->next))
		if( !strcmp((*p)->name,name) )
			break;
	if( (m=*p)==NULL )
		return(NULL);
	*p=m->next;
	for(p=&active_modules;*p!=NULL;p=&((*p)->next));
	m->next=NULL;
	*p=m;
	return(m);
}



int init_all( char *filename )
{ struct module_s *m;
#ifdef RBAC
	_rbac_init();
#endif
	if( comm_buf_init()<0 )
		return(-1);
	if( mcp_init(filename)<0 )
		return(-1);

	/* Here initialize all comm interfaces */
	for(m=active_modules;m!=NULL;m=m->next)
	{	if( m->create_comm )
		{	if( m->create_comm(m)<0 )
				return(-1);
		}
	}

	if( comm_buf_init2()<0 )
		return(-1);
	if( vs_init()<0 )
		return(-1);
	if( cmds_init()<0 )
		return(-1);
	if( force_init()<0 )
		return(-1);

	for(m=active_modules;m!=NULL;m=m->next)
	{	if( m->init_comm )
		{	if( m->init_comm(m)<0 )
				return(-1);
		}
	}

	/* Here initialize all modules */
	for(m=active_modules;m!=NULL;m=m->next)
	{	if( m->init_namespace )
		{	if( m->init_namespace(m)<0 )
				return(-1);
		}
	}

	if( execute_init(2)<0 )
		return(-1);
	if( language_init(medusa_config_file)<0 )
		return(-1);
	if( language_do()<0 )
		return(-1);
//printf("ZZZ: All initialized\n");

	for(m=active_modules;m!=NULL;m=m->next)
	{	if( m->init_rules )
		{	if( m->init_rules(m)<0 )
				return(-1);
		}
	}
	return(0);
}

int usage( char *me )
{
	fprintf(stderr,"usage: %s [-t] [-d <tree debug file>] [-D[D] <class/events debug file>] [<config. file>]"
		"\n",me);
	return(0);
}

void init_sig_handler( int signum )
{
	return;
}

static int run_init( int argc, char *argv[] )
{ int i;
//  int status;

	switch ((i = fork()))
	{
		case -1:
			return(0);
		case 0:
			return(1);
		default:
			signal(SIGHUP,init_sig_handler);
			pause();
//			waitpid(i, &status, 0);
//			if (!WIFEXITED(status))
//				exit(-1);
			argv[0] = MEDUSA_INITNAME;
			execvp(argv[0], argv);
			printf("Fatal error: Can't execute %s\n",argv[0]);
			exit(-1);
	}
	return(0);
}

void(*debug_def_out)( int arg, char *str )=NULL;
int debug_def_arg=NULL;
void(*debug_do_out)( int arg, char *str )=NULL;
int debug_do_arg=NULL;

static void debug_fd_write( int arg, char *s )
{
	write(arg,s,strlen(s));
}

int main( int argc, char *argv[] )
{ char *conf_name="/etc/constable.conf";
  // struct sched_param schedpar; 
  int a;
  int kill_init=0;
  int debug_fd= -1;

	if( getpid() <= 1 )
		kill_init=run_init(argc,argv);

	for(a=1;a<argc;a++)
	{
		if( argv[a][0]=='-' )
		{	if( argv[a][1]=='t' )
				test=1;
			else if( argv[a][1]=='d' && a+1<argc )
			{	a++;
				debug_fd=open(argv[a],O_WRONLY|O_CREAT|O_TRUNC,0600);
				test=1;
			}
			else if( argv[a][1]=='D' && a+1<argc )
			{	a++;
				debug_def_out=debug_fd_write;
				debug_def_arg=(open(argv[a],O_WRONLY|O_CREAT|O_TRUNC,0600));
				if( argv[a-1][2]=='D' )
				{	debug_do_out=debug_fd_write;
					debug_do_arg=debug_def_arg;
				}
			}
			else if( argv[a][1]=='c' && a+1<argc )
			{	a++;
				medusa_config_file=argv[a];
			}
			else
				return(usage(argv[0]));
		}
		else
		{	conf_name=argv[a];
		}
	}

	if( init_all(conf_name)<0 )
		return(-1);
		
	if( space_apply_all()<0 )
		return(-1);

	if( debug_fd>=0 )
		tree_print_node(&global_root,0,debug_fd_write,debug_fd);

	if( test )
		return(0);

printf("ZZZ: bude dobre\n");
//	setsid();
//	schedpar.sched_priority = sched_get_priority_max(SCHED_FIFO);
//	sched_setscheduler(0, SCHED_FIFO, &schedpar);
	if( kill_init )
		kill(1,SIGHUP);
	comm_do();
	return(-1);
}

