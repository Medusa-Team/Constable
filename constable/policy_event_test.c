// SPDX-License-Identifier: GPL-2.0

#include "policy_event_test.h"

#include "comm.h"
#include "constable.h"
#include "event.h"
#include "language/execute.h"
#include "object.h"
#include "string_utils.h"
#include "tree.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG_OBJECT_CLASS_ID ((MCPptr_t)0x7fff0001U)
#define DEBUG_EVENT_ID ((MCPptr_t)0x7fff0002U)
#define HISTORICAL_FILE_CLASS_ID ((MCPptr_t)0x7fff0011U)
#define HISTORICAL_PROCESS_CLASS_ID ((MCPptr_t)0x7fff0012U)
#define HISTORICAL_GETFILE_EVENT_ID ((MCPptr_t)0x7fff0013U)
#define HISTORICAL_CAP_NET_RAW 13U

struct debug_object_data {
	int32_t value;
};

struct debug_event_data {
	int32_t trace;
	int32_t mode;
};

struct historical_file_data {
	uintptr_t object_cinfo[2];
	uintptr_t subject_cinfo[2];
	unsigned char pcap[8];
};

struct historical_process_data {
	uintptr_t object_cinfo[2];
	uintptr_t subject_cinfo[2];
};

struct historical_getfile_data {
	char filename[256];
	int32_t pid;
};

struct expected_event_result {
	int status;
	int result;
	int32_t trace;
	int32_t subject;
	int32_t object;
};

static struct medusa_attribute_s debug_object_attributes[] = {
	MED_ATTR_SIGNED(struct debug_object_data, value, "value"),
	MED_ATTR_END,
};

static struct medusa_attribute_s debug_event_attributes[] = {
	MED_ATTR_SIGNED(struct debug_event_data, trace, "trace"),
	MED_ATTR_SIGNED(struct debug_event_data, mode, "mode"),
	MED_ATTR_END,
};

static struct medusa_attribute_s historical_file_attributes[] = {
	MED_ATTR_BITMAP(struct historical_file_data, object_cinfo, "o_cinfo"),
	MED_ATTR_BITMAP(struct historical_file_data, subject_cinfo, "s_cinfo"),
	MED_ATTR_BITMAP(struct historical_file_data, pcap, "pcap"),
	MED_ATTR_END,
};

static struct medusa_attribute_s historical_process_attributes[] = {
	MED_ATTR_BITMAP(struct historical_process_data, object_cinfo, "o_cinfo"),
	MED_ATTR_BITMAP(struct historical_process_data, subject_cinfo, "s_cinfo"),
	MED_ATTR_END,
};

static struct medusa_attribute_s historical_getfile_attributes[] = {
	MED_ATTR_STRING(struct historical_getfile_data, filename, "filename"),
	MED_ATTR_SIGNED(struct historical_getfile_data, pid, "pid"),
	MED_ATTR_END,
};

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

static struct event_type_s *register_debug_schema(struct comm_s *comm)
{
	struct medusa_class_s class_definition = {
		.classid = DEBUG_OBJECT_CLASS_ID,
		.size = sizeof(struct debug_object_data),
		.name = "_debug_object",
	};
	struct medusa_acctype_s event_definition = {
		.opid = DEBUG_EVENT_ID,
		.size = sizeof(struct debug_event_data),
		.actbit = 0,
		.op_class = {
			DEBUG_OBJECT_CLASS_ID,
			DEBUG_OBJECT_CLASS_ID,
		},
		.name = "_debug_event",
		.op_name = {
			"subject",
			"object",
		},
	};

	if (!add_class(comm, &class_definition, debug_object_attributes))
		return NULL;
	return event_type_add(comm, &event_definition,
			      debug_event_attributes);
}

static int initialize_context(struct comm_buffer_s *buffer,
			      struct event_type_s *event,
			      struct debug_event_data *operation,
			      struct debug_object_data *subject,
			      struct debug_object_data *object)
{
	struct event_context_s *context = &buffer->context;

	get_empty_context(context);
	context->operation.attr.length = sizeof(*operation);
	context->operation.class = event->operation_class;
	context->operation.data = (char *)operation;
	context->subject.attr.length = sizeof(*subject);
	context->subject.class = event->op[0];
	context->subject.data = (char *)subject;
	context->object.attr.length = sizeof(*object);
	context->object.class = event->op[1];
	context->object.data = (char *)object;
	context->cb = buffer;
	if (string_copy(context->operation.attr.name,
			sizeof(context->operation.attr.name), "_debug_event") ||
	    string_copy(context->subject.attr.name,
			sizeof(context->subject.attr.name), "subject") ||
	    string_copy(context->object.attr.name,
			sizeof(context->object.attr.name), "object"))
		return -1;
	return 0;
}

static int run_event_case(struct comm_s *comm, struct event_type_s *event,
			  int initial_list, int32_t mode,
			  const struct expected_event_result *expected,
			  const char *name, FILE *output)
{
	struct comm_buffer_s buffer = {
		.comm = comm,
		.event = event,
		.ehh_list = initial_list,
	};
	struct debug_event_data operation = {
		.mode = mode,
	};
	struct debug_object_data subject = {
		.value = 2,
	};
	struct debug_object_data object = {
		.value = 3,
	};
	int status;

	if (initialize_context(&buffer, event, &operation, &subject, &object))
		return init_error("Cannot initialize controlled event objects");
	buffer.execute.stack = execute_get_stack();
	if (!buffer.execute.stack)
		return init_error("Cannot allocate controlled event stack");

	status = do_event(&buffer);
	release_execute_stacks(buffer.execute.stack);
	fprintf(output,
		"Policy event self-test %s: status=%d result=%d trace=%d subject=%d object=%d\n",
		name, status, buffer.context.result, operation.trace,
		subject.value, object.value);
	if (status != expected->status ||
	    buffer.context.result != expected->result ||
	    operation.trace != expected->trace ||
	    subject.value != expected->subject ||
	    object.value != expected->object)
		return init_error("Controlled event '%s' semantics changed", name);
	return 0;
}

int policy_event_self_test(const char *comm_name, FILE *output)
{
	static const struct expected_event_result allow_expected = {
		.status = 0,
		.result = RESULT_DENY,
		.trace = 123456,
		.subject = 16,
		.object = 11,
	};
	static const struct expected_event_result deny_expected = {
		.status = 0,
		.result = RESULT_FORCE_ALLOW,
		.trace = 756,
		.subject = 15,
		.object = 4,
	};
	struct event_names_s *event_name;
	struct event_type_s *event;
	struct comm_s *comm;

	if (!comm_name || !output)
		return -1;
	comm = comm_find((char *)comm_name);
	if (!comm)
		return init_error("Policy event self-test cannot find comm '%s'",
				  comm_name);
	event_name = event_type_find_name("_debug_event", false);
	if (!event_name)
		return init_error("Policy event self-test requires event _debug_event");
	event = register_debug_schema(comm);
	if (!event)
		return init_error("Cannot register controlled event schema");
	if (comm_conn_init(comm, false) < 0)
		return init_error("Cannot initialize controlled event schema");
	if (execute_registers_init() < 0)
		return init_error("Cannot allocate controlled event registers");

	if (run_event_case(comm, event, EHH_VS_ALLOW, 1, &allow_expected,
			   "allow-phase", output) ||
	    run_event_case(comm, event, EHH_VS_DENY, 2, &deny_expected,
			   "deny-phase", output))
		return -1;
	return 0;
}

static int quiet_conf_error(struct comm_s *comm, const char *format, ...)
{
	(void)comm;
	(void)format;
	return 0;
}

static struct event_type_s *register_historical_schema(struct comm_s *comm)
{
	struct medusa_class_s file_definition = {
		.classid = HISTORICAL_FILE_CLASS_ID,
		.size = sizeof(struct historical_file_data),
		.name = "file",
	};
	struct medusa_class_s process_definition = {
		.classid = HISTORICAL_PROCESS_CLASS_ID,
		.size = sizeof(struct historical_process_data),
		.name = "process",
	};
	struct medusa_acctype_s event_definition = {
		.opid = HISTORICAL_GETFILE_EVENT_ID,
		.size = sizeof(struct historical_getfile_data),
		.actbit = 0,
		.op_class = {
			HISTORICAL_FILE_CLASS_ID,
			HISTORICAL_FILE_CLASS_ID,
		},
		.name = "getfile",
		.op_name = {
			"file",
			"parent",
		},
	};

	if (!add_class(comm, &file_definition, historical_file_attributes) ||
	    !add_class(comm, &process_definition,
		       historical_process_attributes))
		return NULL;
	return event_type_add(comm, &event_definition,
			      historical_getfile_attributes);
}

static struct class_handler_s *find_class_handler(struct class_s *class,
						  const char *tree_name)
{
	struct class_handler_s *handler;

	for (handler = class->classname->class_handler; handler;
	     handler = handler->next)
		if (handler->root && handler->root->type &&
		    !strcmp(handler->root->type->name, tree_name))
			return handler;
	return NULL;
}

static int initialize_historical_context(
	struct comm_buffer_s *buffer, struct event_type_s *event,
	struct historical_getfile_data *operation,
	struct historical_file_data *file,
	struct historical_file_data *parent)
{
	struct event_context_s *context = &buffer->context;

	get_empty_context(context);
	context->operation.attr.length = sizeof(*operation);
	context->operation.class = event->operation_class;
	context->operation.data = (char *)operation;
	context->subject.attr.length = sizeof(*file);
	context->subject.class = event->op[0];
	context->subject.data = (char *)file;
	context->object.attr.length = sizeof(*parent);
	context->object.class = event->op[1];
	context->object.data = (char *)parent;
	context->cb = buffer;
	if (string_copy(context->operation.attr.name,
			sizeof(context->operation.attr.name), "getfile") ||
	    string_copy(context->subject.attr.name,
			sizeof(context->subject.attr.name), "file") ||
	    string_copy(context->object.attr.name,
			sizeof(context->object.attr.name), "parent"))
		return -1;
	return 0;
}

static int run_historical_getfile(
	struct comm_s *comm, struct event_type_s *event, const char *component,
	struct historical_file_data *file,
	struct historical_file_data *parent, int *result)
{
	struct comm_buffer_s *buffer;
	struct comm_buffer_s *resized;
	struct historical_getfile_data operation = { 0 };
	int status;

	if (string_copy(operation.filename, sizeof(operation.filename),
			component))
		return -1;
	buffer = comm_buf_get(0, comm);
	if (!buffer)
		return -1;
	resized = comm_buf_alloc_var_data(buffer);
	if (!resized) {
		buffer->bfree(buffer);
		return -1;
	}
	buffer = resized;
	buffer->event = event;
	buffer->ehh_list = EHH_VS_ALLOW;
	if (initialize_historical_context(buffer, event, &operation, file,
					  parent)) {
		buffer->bfree(buffer);
		return -1;
	}
	status = do_event(buffer);
	*result = buffer->context.result;
	buffer->bfree(buffer);
	return status;
}

static int bitmap_has_only_bit(const unsigned char *bitmap, size_t length,
			       unsigned int bit)
{
	size_t index;

	if (bit >= length * 8U)
		return 0;
	for (index = 0; index < length; index++) {
		unsigned char expected = 0;

		if (index == bit / 8U)
			expected = (unsigned char)(1U << (bit % 8U));
		if (bitmap[index] != expected)
			return 0;
	}
	return 1;
}

static int bitmap_is_clear(const unsigned char *bitmap, size_t length)
{
	size_t index;

	for (index = 0; index < length; index++)
		if (bitmap[index])
			return 0;
	return 1;
}

int policy_historical_event_self_test(const char *comm_name, FILE *output)
{
	struct historical_file_data root = { 0 };
	struct historical_file_data bin = { 0 };
	struct historical_file_data ping = { 0 };
	struct historical_file_data other = { 0 };
	struct class_handler_s *file_tree;
	struct event_handler_s *saved_init;
	struct event_names_s *event_name;
	struct event_type_s *event;
	struct comm_s *comm;
	int (*saved_conf_error)(struct comm_s *, const char *, ...);
	int bin_result;
	int other_result;
	int ping_result;
	int bin_status;
	int other_status;
	int ping_status;
	char bin_path[64];
	char ping_path[64];
	struct tree_s *resolved_path;

	if (!comm_name || !output)
		return -1;
	comm = comm_find((char *)comm_name);
	if (!comm)
		return init_error("Historical event self-test cannot find comm '%s'",
				  comm_name);
	event_name = event_type_find_name("getfile", false);
	if (!event_name)
		return init_error("Historical event self-test requires event getfile");
	event = register_historical_schema(comm);
	if (!event)
		return init_error("Cannot register historical getfile schema");

	saved_conf_error = comm->conf_error;
	saved_init = function_init;
	comm->conf_error = quiet_conf_error;
	function_init = NULL;
	if (comm_conn_init(comm, false) < 0) {
		function_init = saved_init;
		comm->conf_error = saved_conf_error;
		return init_error("Cannot initialize historical getfile schema");
	}
	function_init = saved_init;
	comm->conf_error = saved_conf_error;
	if (execute_registers_init() < 0)
		return init_error("Cannot allocate historical event registers");

	file_tree = find_class_handler(event->op[0], "fs");
	if (!file_tree || file_tree->cinfo_offset[comm->conn] < 0)
		return init_error("Historical policy has no initialized fs tree");
	root.subject_cinfo[0] = (uintptr_t)file_tree->root;
	bin_status = run_historical_getfile(comm, event, "bin", &bin, &root,
					    &bin_result);
	resolved_path = (struct tree_s *)(bin.subject_cinfo[0]);
	if (!resolved_path ||
	    string_copy(bin_path, sizeof(bin_path),
			tree_get_path(resolved_path)))
		return init_error("Historical getfile /bin path is unavailable");
	if (bin_status != 0 || bin_result != RESULT_ALLOW ||
	    strcmp(bin_path, "fs/bin"))
		return init_error("Historical getfile /bin traversal changed");

	ping_status = run_historical_getfile(comm, event, "ping", &ping, &bin,
					     &ping_result);
	resolved_path = (struct tree_s *)(ping.subject_cinfo[0]);
	if (!resolved_path ||
	    string_copy(ping_path, sizeof(ping_path),
			tree_get_path(resolved_path)))
		return init_error("Historical getfile /bin/ping path is unavailable");
	other_status = run_historical_getfile(comm, event, "other", &other,
					      &bin, &other_result);
	fprintf(output,
		"Policy historical event self-test: bin=%s ping=%s status=%d/%d/%d result=%d/%d/%d pcap_net_raw=%d other_pcap_clear=%d\n",
		bin_path, ping_path, bin_status, ping_status, other_status,
		bin_result, ping_result, other_result,
		bitmap_has_only_bit(ping.pcap, sizeof(ping.pcap),
				    HISTORICAL_CAP_NET_RAW),
		bitmap_is_clear(other.pcap, sizeof(other.pcap)));
	if (ping_status != 0 || ping_result != RESULT_ALLOW ||
	    strcmp(ping_path, "fs/bin/ping") ||
	    !bitmap_has_only_bit(ping.pcap, sizeof(ping.pcap),
				 HISTORICAL_CAP_NET_RAW) ||
	    other_status != 0 || other_result != RESULT_ALLOW ||
	    !bitmap_is_clear(other.pcap, sizeof(other.pcap)))
		return init_error("Historical getfile callback semantics changed");
	return 0;
}
