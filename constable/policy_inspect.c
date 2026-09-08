// SPDX-License-Identifier: GPL-2.0

#include "policy_inspect.h"

#include "access_types.h"
#include "event.h"
#include "object.h"
#include "space.h"
#include "tree.h"
#include "vs.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *const handler_modes[EHH_LISTS] = {
	"vs_allow",
	"vs_deny",
	"notify_allow",
	"notify_deny",
};

struct inspect_output {
	FILE *file;
	int first;
};

static const char *space_label(const struct space_s *space, char *buffer,
			       size_t capacity)
{
	size_t bit;
	size_t word;

	if (strcmp(space->name, ANON_SPACE_NAME))
		return space->name;
	for (word = 0; word < VS_WORDS; word++) {
		for (bit = 0; bit < BITS_PER_VS_WORD; bit++) {
			if (space->vs_id[word] & ((vs_t)1U << bit)) {
				snprintf(buffer, capacity, "?@%zu",
					 word * BITS_PER_VS_WORD + bit);
				return buffer;
			}
		}
	}
	snprintf(buffer, capacity, "?@unassigned");
	return buffer;
}

static int json_string(FILE *file, const char *value)
{
	const unsigned char *cursor;

	if (fputc('"', file) == EOF)
		return -1;
	for (cursor = (const unsigned char *)(value ? value : "");
	     *cursor; cursor++) {
		switch (*cursor) {
		case '"':
			if (fputs("\\\"", file) == EOF)
				return -1;
			break;
		case '\\':
			if (fputs("\\\\", file) == EOF)
				return -1;
			break;
		case '\b':
			if (fputs("\\b", file) == EOF)
				return -1;
			break;
		case '\f':
			if (fputs("\\f", file) == EOF)
				return -1;
			break;
		case '\n':
			if (fputs("\\n", file) == EOF)
				return -1;
			break;
		case '\r':
			if (fputs("\\r", file) == EOF)
				return -1;
			break;
		case '\t':
			if (fputs("\\t", file) == EOF)
				return -1;
			break;
		default:
			if (*cursor < 0x20) {
				if (fprintf(file, "\\u%04x", *cursor) < 0)
					return -1;
			} else if (fputc(*cursor, file) == EOF) {
				return -1;
			}
		}
	}
	return fputc('"', file) == EOF ? -1 : 0;
}

static int write_space_set(FILE *file, const vs_t *set)
{
	struct space_s *space;
	char label[32];
	int first = 1;

	if (fputc('[', file) == EOF)
		return -1;
	for (space = global_spaces; space; space = space->next) {
		if (vs_isclear(space->vs_id) || !vs_test(space->vs_id, set))
			continue;
		if (!first && fputc(',', file) == EOF)
			return -1;
		if (json_string(file, space_label(space, label,
						 sizeof(label))))
			return -1;
		first = 0;
	}
	return fputc(']', file) == EOF ? -1 : 0;
}

static int write_access_sets(FILE *file, const vs_t *sets)
{
	int access;

	if (fputc('{', file) == EOF)
		return -1;
	for (access = 0; access < NR_ACCESS_TYPES; access++) {
		if (access && fputc(',', file) == EOF)
			return -1;
		if (json_string(file, access_names[access]) ||
		    fputc(':', file) == EOF ||
		    write_space_set(file, sets + access * VS_WORDS))
			return -1;
	}
	return fputc('}', file) == EOF ? -1 : 0;
}

static int write_handler(FILE *file, const char *role, int mode,
			 const struct event_hadler_hash_s *handler)
{
	if (fputs("{\"role\":", file) == EOF ||
	    json_string(file, role) ||
	    fputs(",\"mode\":", file) == EOF ||
	    json_string(file, handler_modes[mode]) ||
	    fputs(",\"event\":", file) == EOF ||
	    json_string(file, handler->evname ? handler->evname->name :
			handler->handler->op_name) ||
	    fputs(",\"function\":", file) == EOF ||
	    json_string(file, handler->handler->op_name) ||
	    fputc('}', file) == EOF)
		return -1;
	return 0;
}

static int write_node_handlers(FILE *file, const struct tree_s *node)
{
	const struct event_hadler_hash_s *handler;
	int first = 1;
	int mode;

	if (fputc('[', file) == EOF)
		return -1;
	for (mode = 0; mode < EHH_LISTS; mode++) {
		for (handler = node->subject_handlers[mode]; handler;
		     handler = handler->next) {
			if (!first && fputc(',', file) == EOF)
				return -1;
			if (write_handler(file, "subject", mode, handler))
				return -1;
			first = 0;
		}
		for (handler = node->object_handlers[mode]; handler;
		     handler = handler->next) {
			if (!first && fputc(',', file) == EOF)
				return -1;
			if (write_handler(file, "object", mode, handler))
				return -1;
			first = 0;
		}
	}
	return fputc(']', file) == EOF ? -1 : 0;
}

static int write_namespace_node(FILE *file, struct tree_s *node, int *first)
{
	struct tree_s *child;
	const char *path;

	if (!*first && fputc(',', file) == EOF)
		return -1;
	*first = 0;
	path = tree_get_path(node);
	if (fputs("{\"path\":", file) == EOF ||
	    json_string(file, path) ||
	    fputs(",\"primary_space\":", file) == EOF)
		return -1;
	if (node->primary_space) {
		char label[32];

		if (json_string(file, space_label(node->primary_space, label,
						 sizeof(label))))
			return -1;
	} else if (fputs("null", file) == EOF) {
		return -1;
	}
	if (fputs(",\"access\":", file) == EOF ||
	    write_access_sets(file, &node->vs[0][0]) ||
	    fputs(",\"handlers\":", file) == EOF ||
	    write_node_handlers(file, node) ||
	    fputc('}', file) == EOF)
		return -1;

	for (child = node->child; child; child = child->next)
		if (write_namespace_node(file, child, first))
			return -1;
	for (child = node->regex_child; child; child = child->next)
		if (write_namespace_node(file, child, first))
			return -1;
	return 0;
}

static int write_space(FILE *file, const struct space_s *space)
{
	char label[32];
	size_t word;

	if (fputs("{\"name\":", file) == EOF ||
	    json_string(file, space_label(space, label, sizeof(label))) ||
	    fprintf(file,
		    ",\"primary\":%s,\"used\":%s,\"members\":%d,\"id\":[",
		    space->primary ? "true" : "false",
		    space->used ? "true" : "false", space->members.count) < 0)
		return -1;
	for (word = 0; word < VS_WORDS; word++) {
		if (word && fputc(',', file) == EOF)
			return -1;
		if (fprintf(file, "\"0x%08" PRIx32 "\"",
			    (uint32_t)space->vs_id[word]) < 0)
			return -1;
	}
	if (fputs("],\"access\":", file) == EOF ||
	    write_access_sets(file, &space->vs[0][0]) ||
	    fputc('}', file) == EOF)
		return -1;
	return 0;
}

static int write_event(const struct event_names_s *event, void *argument)
{
	struct inspect_output *output = argument;
	const struct event_hadler_hash_s *handler;
	int first_rule = 1;
	int mode;

	if (!output->first && fputc(',', output->file) == EOF)
		return -1;
	output->first = 0;
	if (fputs("{\"name\":", output->file) == EOF ||
	    json_string(output->file, event->name) ||
	    fputs(",\"rules\":[", output->file) == EOF)
		return -1;
	for (mode = 0; mode < EHH_LISTS; mode++) {
		for (handler = event->handlers_hash[mode]; handler;
		     handler = handler->next) {
			if (!first_rule && fputc(',', output->file) == EOF)
				return -1;
			if (fputs("{\"mode\":", output->file) == EOF ||
			    json_string(output->file, handler_modes[mode]) ||
			    fputs(",\"subject_spaces\":", output->file) == EOF ||
			    write_space_set(output->file, handler->subject_vs) ||
			    fputs(",\"object_spaces\":", output->file) == EOF ||
			    write_space_set(output->file, handler->object_vs) ||
			    fputc('}', output->file) == EOF)
				return -1;
			first_rule = 0;
		}
	}
	return fputs("]}", output->file) == EOF ? -1 : 0;
}

static int write_class(const struct class_names_s *class_name, void *argument)
{
	struct inspect_output *output = argument;

	if (!output->first && fputc(',', output->file) == EOF)
		return -1;
	output->first = 0;
	return json_string(output->file, class_name->name);
}

static int set_has_reachable_member(const vs_t *set)
{
	struct space_s *space;

	if (vs_isfull(set))
		return 1;
	for (space = global_spaces; space; space = space->next)
		if (space->members.count > 0 &&
		    vs_test(space->vs_id, set))
			return 1;
	return 0;
}

static int write_unreachable(const struct event_names_s *event, void *argument)
{
	struct inspect_output *output = argument;
	const struct event_hadler_hash_s *handler;
	const char *reason;
	int mode;

	for (mode = 0; mode < EHH_LISTS; mode++) {
		for (handler = event->handlers_hash[mode]; handler;
		     handler = handler->next) {
			reason = NULL;
			if (!vs_isclear(handler->subject_vs) &&
			    !set_has_reachable_member(handler->subject_vs))
				reason = "empty_subject_space";
			else if (!vs_isclear(handler->object_vs) &&
				 !set_has_reachable_member(handler->object_vs))
				reason = "empty_object_space";
			if (!reason)
				continue;
			if (!output->first &&
			    fputc(',', output->file) == EOF)
				return -1;
			output->first = 0;
			if (fputs("{\"event\":", output->file) == EOF ||
			    json_string(output->file, event->name) ||
			    fputs(",\"mode\":", output->file) == EOF ||
			    json_string(output->file, handler_modes[mode]) ||
			    fputs(",\"reason\":", output->file) == EOF ||
			    json_string(output->file, reason) ||
			    fputc('}', output->file) == EOF)
				return -1;
		}
	}
	return 0;
}

static int policy_inspect_write(FILE *file)
{
	struct inspect_output output = {
		.file = file,
		.first = 1,
	};
	struct space_s *space;
	int first = 1;

	if (fputs("{\"format\":\"constable-policy-v1\",\"classes\":[",
		  file) == EOF)
		return -1;
	if (class_names_visit(write_class, &output) ||
	    fputs("],\"spaces\":[", file) == EOF)
		return -1;
	for (space = global_spaces; space; space = space->next) {
		if (!first && fputc(',', file) == EOF)
			return -1;
		if (write_space(file, space))
			return -1;
		first = 0;
	}
	if (fputs("],\"namespace\":[", file) == EOF)
		return -1;
	first = 1;
	if (write_namespace_node(file, global_root, &first) ||
	    fputs("],\"events\":[", file) == EOF)
		return -1;
	output.first = 1;
	if (event_names_visit(write_event, &output) ||
	    fputs("],\"unreachable_rules\":[", file) == EOF)
		return -1;
	output.first = 1;
	if (event_names_visit(write_unreachable, &output) ||
	    fputs("]}\n", file) == EOF)
		return -1;
	return ferror(file) ? -1 : 0;
}

int policy_inspect_path(const char *path)
{
	FILE *file;
	int result;

	if (!path)
		return -1;
	if (!strcmp(path, "-")) {
		file = stdout;
	} else {
		file = fopen(path, "w");
		if (!file)
			return -1;
	}
	result = policy_inspect_write(file);
	if (file != stdout && fclose(file))
		result = -1;
	else if (file == stdout && fflush(file))
		result = -1;
	return result;
}
