// SPDX-License-Identifier: GPL-2.0

#include "policy_validate.h"

#include "event.h"
#include "object.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INVENTORY_LINE_LIMIT (64U * 1024U)

struct inventory_event {
	struct inventory_event *next;
	char *name;
	char *subject_class;
	char *object_class;
	bool active;
};

struct inventory_class {
	struct inventory_class *next;
	char *name;
	bool active;
};

struct validation_context {
	const struct inventory_event *inventory;
	const struct inventory_class *classes;
	FILE *diagnostics;
	struct {
		unsigned int referenced;
		unsigned int active;
		unsigned int announced;
		unsigned int missing;
	} events, class_names;
};

static void inventory_free(struct inventory_event *events)
{
	struct inventory_event *event;

	while (events) {
		event = events;
		events = events->next;
		free(event->name);
		free(event->subject_class);
		free(event->object_class);
		free(event);
	}
}

static void class_inventory_free(struct inventory_class *classes)
{
	struct inventory_class *class_name;

	while (classes) {
		class_name = classes;
		classes = classes->next;
		free(class_name->name);
		free(class_name);
	}
}

static const struct inventory_event *
inventory_find(const struct inventory_event *events, const char *name)
{
	for (; events; events = events->next)
		if (!strcmp(events->name, name))
			return events;
	return NULL;
}

static const struct inventory_class *
class_inventory_find(const struct inventory_class *classes, const char *name)
{
	for (; classes; classes = classes->next)
		if (!strcmp(classes->name, name))
			return classes;
	return NULL;
}

static char *field_value(char *token, const char *name)
{
	size_t length = strlen(name);

	if (strncmp(token, name, length) || token[length] != '=')
		return NULL;
	return token + length + 1;
}

static int inventory_add_line(struct inventory_event **events, char *line,
			      unsigned int line_number, FILE *diagnostics)
{
	struct inventory_event *event;
	char *enforcement = NULL;
	char *name = NULL;
	char *object_class = NULL;
	char *save = NULL;
	char *subject_class = NULL;
	char *token;

	for (token = strtok_r(line, " \t\r\n", &save); token;
	     token = strtok_r(NULL, " \t\r\n", &save)) {
		char *value;

		if ((value = field_value(token, "event")))
			name = value;
		else if ((value = field_value(token, "subject_class")))
			subject_class = value;
		else if ((value = field_value(token, "object_class")))
			object_class = value;
		else if ((value = field_value(token, "enforcement")))
			enforcement = value;
	}

	if (!name || !name[0] || !subject_class || !subject_class[0] ||
	    !object_class || !object_class[0] || !enforcement ||
	    (strcmp(enforcement, "active") &&
	     strcmp(enforcement, "announced"))) {
		fprintf(diagnostics,
			"Policy validation: malformed event inventory line %u\n",
			line_number);
		return -1;
	}
	if (inventory_find(*events, name)) {
		fprintf(diagnostics,
			"Policy validation: duplicate event '%s' on line %u\n",
			name, line_number);
		return -1;
	}

	event = calloc(1, sizeof(*event));
	if (!event)
		return -1;
	event->name = strdup(name);
	event->subject_class = strdup(subject_class);
	event->object_class = strdup(object_class);
	if (!event->name || !event->subject_class || !event->object_class) {
		inventory_free(event);
		return -1;
	}
	event->active = !strcmp(enforcement, "active");
	event->next = *events;
	*events = event;
	return 0;
}

static int inventory_read(FILE *file, struct inventory_event **events,
			  FILE *diagnostics)
{
	unsigned int line_number = 0;
	size_t length;
	char *line;

	line = malloc(INVENTORY_LINE_LIMIT + 2U);
	if (!line)
		return -1;
	while (fgets(line, INVENTORY_LINE_LIMIT + 2U, file)) {
		line_number++;
		length = strlen(line);
		if (length > INVENTORY_LINE_LIMIT ||
		    (length && line[length - 1] != '\n' && !feof(file))) {
			fprintf(diagnostics,
				"Policy validation: event inventory line %u exceeds %u bytes\n",
				line_number, INVENTORY_LINE_LIMIT);
			free(line);
			return -1;
		}
		if (!length || line[0] == '\n')
			continue;
		if (inventory_add_line(events, line, line_number, diagnostics)) {
			free(line);
			return -1;
		}
	}
	free(line);
	if (ferror(file)) {
		fprintf(diagnostics,
			"Policy validation: cannot read event inventory\n");
		return -1;
	}
	return 0;
}

static int class_inventory_add_line(struct inventory_class **classes,
				    char *line, unsigned int line_number,
				    FILE *diagnostics)
{
	struct inventory_class *class_name;
	char *enforcement = NULL;
	char *name = NULL;
	char *save = NULL;
	char *token;

	for (token = strtok_r(line, " \t\r\n", &save); token;
	     token = strtok_r(NULL, " \t\r\n", &save)) {
		char *value;

		if ((value = field_value(token, "class")))
			name = value;
		else if ((value = field_value(token, "enforcement")))
			enforcement = value;
	}
	if (!name || !name[0] || !enforcement ||
	    (strcmp(enforcement, "active") &&
	     strcmp(enforcement, "announced"))) {
		fprintf(diagnostics,
			"Policy validation: malformed class inventory line %u\n",
			line_number);
		return -1;
	}
	if (class_inventory_find(*classes, name)) {
		fprintf(diagnostics,
			"Policy validation: duplicate class '%s' on line %u\n",
			name, line_number);
		return -1;
	}
	class_name = calloc(1, sizeof(*class_name));
	if (!class_name)
		return -1;
	class_name->name = strdup(name);
	if (!class_name->name) {
		free(class_name);
		return -1;
	}
	class_name->active = !strcmp(enforcement, "active");
	class_name->next = *classes;
	*classes = class_name;
	return 0;
}

static int class_inventory_read(FILE *file, struct inventory_class **classes,
				FILE *diagnostics)
{
	unsigned int line_number = 0;
	size_t length;
	char *line;

	line = malloc(INVENTORY_LINE_LIMIT + 2U);
	if (!line)
		return -1;
	while (fgets(line, INVENTORY_LINE_LIMIT + 2U, file)) {
		line_number++;
		length = strlen(line);
		if (length > INVENTORY_LINE_LIMIT ||
		    (length && line[length - 1] != '\n' && !feof(file))) {
			fprintf(diagnostics,
				"Policy validation: class inventory line %u exceeds %u bytes\n",
				line_number, INVENTORY_LINE_LIMIT);
			free(line);
			return -1;
		}
		if (!length || line[0] == '\n')
			continue;
		if (class_inventory_add_line(classes, line, line_number,
					     diagnostics)) {
			free(line);
			return -1;
		}
	}
	free(line);
	if (ferror(file)) {
		fprintf(diagnostics,
			"Policy validation: cannot read class inventory\n");
		return -1;
	}
	return 0;
}

static int validate_event(const struct event_names_s *policy_event,
			  void *argument)
{
	struct validation_context *context = argument;
	const struct inventory_event *event;

	context->events.referenced++;
	event = inventory_find(context->inventory, policy_event->name);
	if (!event) {
		context->events.missing++;
		fprintf(context->diagnostics,
			"Policy validation: event '%s' is missing from the kernel inventory\n",
			policy_event->name);
		return 0;
	}
	if (!event->active) {
		context->events.announced++;
		fprintf(context->diagnostics,
			"Policy validation: event '%s' is announced but not actively enforced (subject=%s object=%s)\n",
			policy_event->name, event->subject_class,
			event->object_class);
		return 0;
	}
	context->events.active++;
	return 0;
}

static int validate_class(const struct class_names_s *policy_class,
			  void *argument)
{
	struct validation_context *context = argument;
	const struct inventory_class *class_name;

	context->class_names.referenced++;
	class_name = class_inventory_find(context->classes, policy_class->name);
	if (!class_name) {
		context->class_names.missing++;
		fprintf(context->diagnostics,
			"Policy validation: class '%s' is missing from the kernel inventory\n",
			policy_class->name);
		return 0;
	}
	if (!class_name->active) {
		context->class_names.announced++;
		fprintf(context->diagnostics,
			"Policy validation: class '%s' is announced but has no actively enforced event\n",
			policy_class->name);
		return 0;
	}
	context->class_names.active++;
	return 0;
}

static char *inventory_path(const char *directory, const char *name)
{
	size_t directory_length;
	size_t name_length;
	size_t size;
	char *path;

	directory_length = strlen(directory);
	name_length = strlen(name);
	if (name_length > SIZE_MAX - 2U ||
	    directory_length > SIZE_MAX - name_length - 2U)
		return NULL;
	size = directory_length + name_length + 2U;
	path = malloc(size);
	if (!path)
		return NULL;
	snprintf(path, size, "%s/%s", directory, name);
	return path;
}

int policy_validate_inventory_path(const char *path, FILE *diagnostics)
{
	struct inventory_event *inventory = NULL;
	struct inventory_class *classes = NULL;
	struct validation_context context = {
		.diagnostics = diagnostics,
	};
	FILE *file;
	char *classes_path = NULL;
	char *events_path = NULL;
	int inventory_result;
	int result = -1;

	if (!path || !diagnostics)
		return -1;
	events_path = inventory_path(path, "events");
	classes_path = inventory_path(path, "classes");
	if (!events_path || !classes_path)
		goto out;
	file = fopen(events_path, "r");
	if (!file) {
		fprintf(diagnostics,
			"Policy validation: cannot open event inventory '%s'\n",
			events_path);
		goto out;
	}
	inventory_result = inventory_read(file, &inventory, diagnostics);
	if (fclose(file))
		inventory_result = -1;
	if (inventory_result)
		goto out;

	file = fopen(classes_path, "r");
	if (!file) {
		fprintf(diagnostics,
			"Policy validation: cannot open class inventory '%s'\n",
			classes_path);
		goto out;
	}
	inventory_result = class_inventory_read(file, &classes, diagnostics);
	if (fclose(file))
		inventory_result = -1;
	if (inventory_result)
		goto out;

	context.inventory = inventory;
	context.classes = classes;
	if (event_names_visit(validate_event, &context))
		goto out;
	fprintf(diagnostics,
		"Policy validation events: referenced=%u active=%u announced=%u missing=%u\n",
		context.events.referenced, context.events.active,
		context.events.announced, context.events.missing);
	if (class_names_visit(validate_class, &context))
		goto out;
	fprintf(diagnostics,
		"Policy validation classes: referenced=%u active=%u announced=%u missing=%u\n",
		context.class_names.referenced, context.class_names.active,
		context.class_names.announced, context.class_names.missing);
	result = (context.events.announced || context.events.missing ||
		  context.class_names.announced ||
		  context.class_names.missing) ? 1 : 0;

out:
	free(events_path);
	free(classes_path);
	inventory_free(inventory);
	class_inventory_free(classes);
	return result;
}
