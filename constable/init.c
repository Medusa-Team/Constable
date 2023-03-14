// SPDX-License-Identifier: GPL-2.0
/*
 * Constable: init.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sched.h>
#include <signal.h>
#include "event.h"
#include "comm.h"
#include "init.h"

#include "space.h"
#include "tree.h"
#include "constable.h"

#ifndef MEDUSA_INITNAME
#define MEDUSA_INITNAME "/sbin/init"
#endif

char *medusa_config_file = "/etc/medusa.conf";

static int test;

static struct module_s *first_module;
static struct module_s *active_modules;

int add_module(struct module_s *module)
{
	struct module_s **p;

	for (p = &first_module; *p; p = &((*p)->next))
		if (!strcmp((*p)->name, module->name))
			return -1;

	module->next = NULL;
	*p = module;

	return 0;
}

struct module_s *activate_module(char *name)
{
	struct module_s *module, **p;

	for (p = &first_module; *p; p = &((*p)->next))
		if (!strcmp((*p)->name, name))
			break;
	module = *p;

	if (!module)
		return NULL;
	*p = module->next;

	for (p = &active_modules; *p; p = &((*p)->next))
		;
	module->next = NULL;
	*p = module;

	return module;
}

int init_all(char *filename)
{
	struct module_s *m;

#ifdef RBAC
	_rbac_init();
#endif

	if (buffers_init() < 0)
		return -1;
	if (mcp_init(filename) < 0)
		return -1;

	/* Here initialize all comm interfaces */
	for (m = active_modules; m; m = m->next) {
		if (m->create_comm && m->create_comm(m) < 0)
			return -1;
	}

	if (buffers_alloc() < 0)
		return -1;
	if (vs_init() < 0)
		return -1;
	if (tree_init() < 0)
		return -1;
	if (cmds_init() < 0)
		return -1;
	//if (force_init() < 0)
	//	return -1;

	for (m = active_modules; m; m = m->next) {
		if (m->init_comm && m->init_comm(m) < 0)
			return -1;
	}

	/* Here initialize all modules */
	for (m = active_modules; m; m = m->next) {
		if (m->init_namespace && m->init_namespace(m) < 0)
			return -1;
	}

	if (execute_init(2) < 0)
		return -1;
	if (language_init(medusa_config_file) < 0)
		return -1;
	if (language_do() < 0)
		return -1;
	//printf("ZZZ: All initialized\n");

	for (m = active_modules; m; m = m->next) {
		if (m->init_rules && m->init_rules(m) < 0)
			return -1;
	}

	return 0;
}

int usage(char *me)
{
	fprintf(stderr,
		"Usage: %s [-t] [-d <tree debug file>] [-D[D] <class/events debug file>] [<config. file>]\n\n"
		"    -t and/or -d causes Constable to shut down before initiating communication\n",
		me);
	return 0;
}

void init_sig_handler(int signum)
{
}

static int run_init(int argc, char *argv[])
{
	int i;

	switch ((i = fork())) {
	case -1:
		return 0;
	case 0:
		return 1;
	default:
		signal(SIGHUP, init_sig_handler);
		pause();
		argv[0] = MEDUSA_INITNAME;
		execvp(argv[0], argv);
		printf("Fatal error: Can't execute %s\n", argv[0]);
		exit(-1);
	}

	return 0;
}

void (*debug_def_out)(int arg, char *str) = NULL;
pthread_mutex_t debug_def_lock = PTHREAD_MUTEX_INITIALIZER;
int debug_def_arg;
void (*debug_do_out)(int arg, char *str) = NULL;
pthread_mutex_t debug_do_lock = PTHREAD_MUTEX_INITIALIZER;
int debug_do_arg;

static void debug_fd_write(int arg, char *s)
{
	write(arg, s, strlen(s));
}

int tls_create_init(void)
{
#ifdef DEBUG_TRACE
	tls_create(&runtime_file_key);
	tls_create(&runtime_pos_key);
#endif
	tls_create(&errstr_key);
	return 0;
}

int tls_alloc_init(void)
{
#ifdef DEBUG_TRACE
	if (!tls_alloc(runtime_file_key, sizeof(RUNTIME_FILE_TYPE)))
		return -1;
	if (!tls_alloc(runtime_pos_key, sizeof(RUNTIME_POS_TYPE)))
		return -1;
#endif
	if (!tls_alloc(errstr_key, sizeof(char **)))
		return -1;
	return 0;
}

int main(int argc, char *argv[])
{
	char *conf_name = "/etc/constable.conf";
	int a;
	int kill_init = 0;
	int debug_fd = -1;
	//struct sched_param schedpar;

	if (getpid() <= 1)
		kill_init = run_init(argc, argv);

	for (a = 1; a < argc; a++) {
		if (argv[a][0] == '-') {
			if (argv[a][1] == 't') {
				test = 1;
			} else if (argv[a][1] == 'd' && a + 1 < argc) {
				a++;
				debug_fd = comm_open_skip_stdfds(argv[a],
								 O_WRONLY | O_CREAT | O_TRUNC,
								 0600);
				test = 1;
			} else if (argv[a][1] == 'D' && a + 1 < argc) {
				a++;
				debug_def_out = debug_fd_write;
				debug_def_arg = comm_open_skip_stdfds(argv[a],
								      O_WRONLY | O_CREAT | O_TRUNC,
								      0600);
				if (argv[a - 1][2] == 'D') {
					debug_do_out = debug_fd_write;
					debug_do_arg = debug_def_arg;
				}
			} else if (argv[a][1] == 'c' && a + 1 < argc) {
				a++;
				medusa_config_file = argv[a];
			} else {
				return usage(argv[0]);
			}
		} else {
			conf_name = argv[a];
		}
	}

	if (tls_create_init())
		return -1;

	if (tls_alloc_init())
		return -1;

	if (init_all(conf_name) < 0)
		return -1;

	if (space_apply_all() < 0)
		return -1;

	if (debug_fd >= 0)
		tree_print_node(global_root, 0, debug_fd_write, debug_fd);

	if (test)
		return 0;

	//printf("ZZZ: bude dobre\n");
	//setsid();
	//schedpar.sched_priority = sched_get_priority_max(SCHED_FIFO);
	//sched_setscheduler(0, SCHED_FIFO, &schedpar);
	if (kill_init)
		kill(1, SIGHUP);

	comm_do();
	return 0;
}
