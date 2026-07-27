// SPDX-License-Identifier: GPL-2.0

#include "../tree.h"
#include "../space.h"
#include "../comm.h"

#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>

pthread_key_t errstr_key;
char *access_names[NR_ACCESS_TYPES];
char *Out_of_memory = "Out of memory";

static int checks;
static int failures;

#define EXPECT_TRUE(condition, description)				\
	do {								\
		checks++;						\
		if (!(condition)) {					\
			fprintf(stderr, "FAIL: %s\n", description);	\
			failures++;					\
		}							\
	} while (0)

int error(const char *format, ...)
{
	(void)format;
	return -1;
}

int fatal(const char *format, ...)
{
	(void)format;
	return -1;
}

void *comm_new_array(int size)
{
	if (size <= 0)
		return NULL;
	return calloc(1, (size_t)size);
}

struct event_hadler_hash_s *
evhash_add(struct event_hadler_hash_s **hash,
	   struct event_handler_s *handler, struct event_names_s *event)
{
	(void)hash;
	(void)handler;
	(void)event;
	return NULL;
}

void vs_add(const vs_t *from, vs_t *to)
{
	(void)from;
	(void)to;
}

int vs_isclear(const vs_t *set)
{
	(void)set;
	return 1;
}

int space_vs_to_str(vs_t *set, char *output, int size)
{
	(void)set;
	if (size <= 0)
		return -1;
	output[0] = '\0';
	return 0;
}

static void free_children(struct tree_s *node)
{
	struct tree_s *child;
	struct tree_s *next;

	for (child = node->child; child; child = next) {
		next = child->next;
		free_children(child);
		free(child->events);
		free(child);
	}
	for (child = node->regex_child; child; child = next) {
		next = child->next;
		free_children(child);
		if (child->compiled_regex != (void *)1) {
			regfree(child->compiled_regex);
			free(child->compiled_regex);
		}
		free(child->events);
		free(child);
	}
}

int main(void)
{
	static const size_t component_length = 1024 * 1024;
	struct tree_type_s type = {
		.size = sizeof(struct tree_s),
	};
	struct tree_s root = {
		.type = &type,
	};
	struct tree_s *created;
	struct tree_s *matched;
	struct rlimit stack_limit = {
		.rlim_cur = 256 * 1024,
		.rlim_max = 256 * 1024,
	};
	char *path;
	char *query;

	EXPECT_TRUE(setrlimit(RLIMIT_STACK, &stack_limit) == 0,
		    "the regression test runs with a bounded stack");
	path = malloc(component_length + 1);
	query = malloc(component_length * 2 + 2);
	EXPECT_TRUE(path != NULL && query != NULL,
		    "long path inputs are allocated");
	if (!path || !query) {
		free(path);
		free(query);
		return EXIT_FAILURE;
	}
	memset(path, 'a', component_length);
	path[component_length] = '\0';
	memcpy(query, path, component_length);
	query[component_length] = '/';
	memset(query + component_length + 1, 'b', component_length);
	query[component_length * 2 + 1] = '\0';

	global_root = &root;
	EXPECT_TRUE(create_path(NULL) == NULL,
		    "path creation rejects a null input");
	EXPECT_TRUE(find_path(NULL) == NULL,
		    "path lookup rejects a null input");
	created = create_path(path);
	EXPECT_TRUE(created != NULL,
		    "a component larger than the stack limit is created");
	EXPECT_TRUE(created && strlen(created->name) == component_length,
		    "the created node retains the complete component");
	matched = find_path(query);
	EXPECT_TRUE(matched != NULL,
		    "long regex-fallback lookup avoids stack exhaustion");
	EXPECT_TRUE(matched && strcmp(matched->name, ".*") == 0,
		    "the fallback lookup selects the wildcard node");
	EXPECT_TRUE(created && strlen(tree_get_path(created)) == 4095,
		    "long rendered paths remain bounded and terminated");

	free_children(&root);
	global_root = NULL;
	free(path);
	free(query);

	if (failures) {
		fprintf(stderr, "tree paths: %d failure(s)\n", failures);
		return EXIT_FAILURE;
	}
	printf("tree paths: %d checks passed\n", checks);
	return EXIT_SUCCESS;
}
