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
#include "policy_event_test.h"
#include "policy_inspect.h"
#include "policy_validate.h"
#include "cli_options.h"
#include "fallback_policy.h"
#include "domain_rule.h"
#include "approval.h"

#ifndef MEDUSA_INITNAME
#define MEDUSA_INITNAME "/sbin/init"
#endif

char *medusa_config_file = "/etc/medusa.conf";
int medusa_config_file_explicit;

static int test;
static int policy_self_test;
static char *policy_event_self_test_comm;
static char *policy_historical_event_test_comm;
static char *policy_inspection_file;
static char *policy_validation_file;

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

	if (execute_init((int)worker_pool_count()) < 0)
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
	comm_seal_buf_var_data();

	return 0;
}

int usage(const char *me)
{
	fprintf(stderr,
		"Usage: %s [options] [<constable config>]\n\n"
		"    -h, --help prints this help without loading a policy\n"
		"    -c <policy file> selects the Medusa policy source\n"
		"    -F, --fallback <event=policy> stages an event fallback before READY\n"
		"    -R, --domain-rule <event:subject:object:selector=allow|deny> installs a non-sleepable cached rule; use * as a wildcard\n"
		"    --approval-socket <path> asks a user-session approval agent\n"
		"    --approval-events <csv|*> selects events requiring approval\n"
		"    --approval-uid <uid> authenticates the approval agent owner\n"
		"    --approval-timeout <seconds> limits each prompt (default 60)\n"
		"    --workers <auto|1-32> sizes the decision worker pool (default auto)\n"
		"    -t and/or -d causes Constable to shut down before initiating communication\n"
		"    -T executes function _debug offline and succeeds only on FORCE_ALLOW\n"
		"    -E executes the controlled _debug_event policy self-test offline\n"
		"    -H executes preserved historical getfile handlers offline\n"
		"    -I writes non-mutating policy inspection JSON and implies -t\n"
		"    -V rejects policy events not actively enforced by a kernel inventory\n"
		"    -d <file> writes the compiled tree and implies -t\n"
		"    -D <file> writes class/event definitions; -DD also traces events\n"
		"    -- ends option processing\n",
		me);
	return 0;
}

static void release_execute_stacks(struct stack_s *stack)
{
	struct stack_s *next;

	if (!stack)
		return;
	while (stack->prev)
		stack = stack->prev;
	while (stack) {
		next = stack->next;
		stack->prev = NULL;
		stack->next = NULL;
		execute_put_stack(stack);
		stack = next;
	}
}

static int run_policy_self_test(void)
{
	struct comm_buffer_s buffer = { 0 };
	struct event_context_s context = { 0 };
	int status;
	int result;

	if (!function_debug)
		return init_error("Policy self-test requires function _debug");
	if (execute_registers_init() < 0)
		return init_error("Cannot allocate policy self-test registers");

	buffer.execute.stack = execute_get_stack();
	if (!buffer.execute.stack)
		return init_error("Cannot allocate policy self-test stack");
	context.cb = &buffer;

	status = function_debug->handler(&buffer, function_debug, &context);
	result = context.result;
	release_execute_stacks(buffer.execute.stack);

	if (status != 0)
		return init_error("Policy self-test attempted asynchronous work");
	printf("Policy self-test result: %d\n", result);
	if (result != RESULT_FORCE_ALLOW)
		return init_error("Policy self-test did not return FORCE_ALLOW");
	return 0;
}

void init_sig_handler(int signum)
{
	(void)signum;
}

static int run_init(int argc, char *argv[])
{
	int i;

	(void)argc;
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
	struct constable_cli_options options;
	enum constable_cli_result parse_result;
	const char *problem_argument;
	char *conf_name;
	int kill_init = 0;
	int debug_fd = -1;
	//struct sched_param schedpar;

	parse_result = constable_cli_parse(argc, argv, &options,
					   &problem_argument);
	if (parse_result == CONSTABLE_CLI_HELP) {
		constable_cli_options_destroy(&options);
		return usage(argv[0]);
	}
	if (parse_result != CONSTABLE_CLI_OK) {
		if (parse_result == CONSTABLE_CLI_MISSING_ARGUMENT)
			fprintf(stderr, "Option %s requires an argument\n",
				problem_argument);
		else if (parse_result == CONSTABLE_CLI_OUT_OF_MEMORY)
			fprintf(stderr, "Cannot allocate command-line policy options\n");
		else if (parse_result == CONSTABLE_CLI_TOO_MANY_DOMAIN_RULES)
			fprintf(stderr, "Too many domain rules (maximum %u)\n",
				CONSTABLE_MAX_DOMAIN_RULES);
		else if (parse_result == CONSTABLE_CLI_INVALID_WORKER_COUNT)
			fprintf(stderr,
				"Invalid worker count: %s (expected auto or 1-%u)\n",
				problem_argument, CONSTABLE_MAX_WORKERS);
		else
			fprintf(stderr, "Unknown option: %s\n", problem_argument);
		usage(argv[0]);
		constable_cli_options_destroy(&options);
		return 2;
	}
	if (fallback_policy_configure(options.fallback_policy_specs,
				      options.fallback_policy_count) < 0) {
		fprintf(stderr,
			"Invalid fallback policy; expected event=baseline_allow, event=baseline_deny, or event=online_required without duplicate events\n");
		constable_cli_options_destroy(&options);
		return 2;
	}
	if (domain_rule_configure(options.domain_rule_specs,
				  options.domain_rule_count) < 0) {
		fprintf(stderr,
			"Invalid domain rule; expected event:subject:object:selector=allow|deny with numeric keys or * wildcards and no duplicate keys\n");
		constable_cli_options_destroy(&options);
		return 2;
	}
	if (approval_configure(options.approval_socket, options.approval_events,
			       options.approval_uid,
			       options.approval_timeout) < 0) {
		fprintf(stderr,
			"Invalid approval configuration; socket, events, and uid are required together\n");
		constable_cli_options_destroy(&options);
		return 2;
	}
	if (worker_pool_configure(options.worker_count) < 0) {
		fprintf(stderr, "Cannot configure worker pool\n");
		constable_cli_options_destroy(&options);
		return 2;
	}

	conf_name = options.config_file;
	medusa_config_file = options.medusa_config_file;
	medusa_config_file_explicit = options.medusa_config_file_explicit;
	test = options.test_only;
	policy_self_test = options.policy_self_test;
	policy_event_self_test_comm = options.policy_event_self_test_comm;
	policy_historical_event_test_comm =
		options.policy_historical_event_test_comm;
	policy_inspection_file = options.policy_inspection_file;
	policy_validation_file = options.policy_validation_file;
	constable_cli_options_destroy(&options);

	if (options.tree_debug_file)
		debug_fd = comm_open_skip_stdfds(options.tree_debug_file,
						 O_WRONLY | O_CREAT | O_TRUNC,
						 0600);
	if (options.definition_debug_file) {
		debug_def_out = debug_fd_write;
		debug_def_arg =
			comm_open_skip_stdfds(options.definition_debug_file,
					     O_WRONLY | O_CREAT | O_TRUNC, 0600);
		if (options.debug_events) {
			debug_do_out = debug_fd_write;
			debug_do_arg = debug_def_arg;
		}
	}

	if (getpid() <= 1)
		kill_init = run_init(argc, argv);

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

	if (policy_inspection_file &&
	    policy_inspect_path(policy_inspection_file) < 0)
		return init_error("Cannot write policy inspection output");

	if (policy_validation_file &&
	    policy_validate_inventory_path(policy_validation_file, stdout) != 0)
		return init_error("Policy references classes or events without active enforcement");

	if (policy_event_self_test_comm &&
	    policy_event_self_test(policy_event_self_test_comm, stdout) < 0)
		return -1;

	if (policy_historical_event_test_comm &&
	    policy_historical_event_self_test(policy_historical_event_test_comm,
					      stdout) < 0)
		return -1;

	if (policy_self_test && run_policy_self_test() < 0)
		return -1;

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
