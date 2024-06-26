// SPDX-License-Identifier: GPL-2.0
/*
 * Constable: tree.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#include "constable.h"
#include "tree.h"
#include "comm.h"
#include <regex.h>
#include "space.h"

/*
 * FIXME: Ak niekto chce vytvorit .*, mala by sa pod nou vytvorit este jedna .*
 */

/* The root's (of the Unified Name Space Tree) name. */
#define GLOBAL_ROOT_NAME "/"

/*
 * Set in medusa config file(s) by primary tree definition:
 *	primary tree "name";
 *
 * Primary tree:
 * -------------
 * Medusa conf language uses strings "/a/b/c" or paths @"d/e/f"
 * as a path representation descending from root of the Unified
 * Name Space Tree (UNST). Very often the representation of the
 * file system in the UNST is needed and in that case the file
 * system should be connected to the root of UNST through its
 * own tree definition node. For example the config lines:
 *	tree "fs" of file;
 *	tree "domain" of process;
 * define two nodes of UNST; each node represents the root of
 * a subtree, which contains data (nodes) of its own type:
 * "fs" contains file objects and "domain" process objects.
 *
 *	/		root of the UNST
 *	|--"fs"		root of the "fs" subtree
 *	|
 *	 --"domain"	root of the "domain" subtree
 *
 * Implication: all objects in a subtree are reachable only
 * through string/path prefix (a name of the root of the
 * subtree). For example, in a space definition for objects
 * of fs one writes:
 *	space my_home = "fs/home/my";
 * For the sake of simplicity and portability of configuration file(s)
 * Medusa language proposes a possibility of defining one so-called
 * "primary" tree. If defined, the string/path prefix "/" will be prepended
 * with the name of defined primary tree. For example
 *	primary tree "fs";
 *	space my_home = "/home/my";
 * will be internally handled as path "fs/home/my" descending from the
 * root of UNST.
 */
static char *default_path;

static struct tree_type_s *global_root_type;
struct tree_s *global_root;

/* ----------------------------- */

/* Unified Name Space Tree (global root) initialization. */
int tree_init(void)
{
	global_root_type = calloc(1, sizeof(struct tree_type_s)+strlen(GLOBAL_ROOT_NAME)+1);
	if (!global_root_type)
		return -1;
	strncpy(global_root_type->name, GLOBAL_ROOT_NAME, strlen(GLOBAL_ROOT_NAME)+1);

	global_root = calloc(1, sizeof(struct tree_s)+strlen(GLOBAL_ROOT_NAME)+1);
	if (!global_root)
		goto free_global_root_type;

	global_root->type = global_root_type;
	strncpy(global_root->name, GLOBAL_ROOT_NAME, strlen(GLOBAL_ROOT_NAME)+1);

	/*
	 * Allocate ->events array only for dummy use in tree_comm_reinit()
	 * function. This array in the global root node shouldn't be used for
	 * real values. It's required only for executing the same code path for
	 * all nodes of Unified Name Space Tree in the above mentioned function.
	 */
	global_root->events = comm_new_array(sizeof(struct tree_event_s));
	if (global_root->events == NULL)
		goto free_global_root;

	return 0;

free_global_root:
	free(global_root);
	global_root = NULL;
free_global_root_type:
	free(global_root_type);
	global_root_type = NULL;

	return -1;
}

/*
 * Set @new tree name as default path. Called for medusa conf
 * primary tree definition:
 *	primary tree "new";
 *
 * @new: Name of the tree to set as primary.
 */
int tree_set_default_path(char *new)
{
	if (default_path != NULL)
		free(default_path);

	default_path = strdup(new);
	if (default_path == NULL) {
		error(Out_of_memory);
		return -1;
	}

	return 0;
}

/* ---------- regexps ---------- */

static int isreg(char *name)
{	int i, unbs = 0;
	char *n = name;

	while (*name) {
		switch (*name) {
		case '(':
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
				(name[1] == 0))
				return 1;
			unbs = 1;
			name++;
		}
		name++;
	}

	if (unbs) {
		i = 0;
		while (n[i]) {
			if (n[i] == '\\')
				i++;
			n[0] = n[i];
			n++;
		}
		n[0] = 0;
	}

	return 0;
}

static void *regcompile(char *reg)
{
	int l = strlen(reg);
	char tmp[l+3];
	regex_t *r;

	/* speed up .* */
	if (!strcmp(reg, ".*"))
		return (void *)1;

	r = malloc(sizeof(regex_t));
	if (r == NULL)
		return NULL;

	tmp[0] = '^';
	strcpy(tmp+1, reg);
	tmp[l+1] = '$';
	tmp[l+2] = 0;
	if (regcomp(r, tmp, REG_EXTENDED|REG_NOSUB) != 0) {
		error("Error in regexp '%s'", reg);
		free(r);
		return NULL;
	}

	return r;
}

static int regcmp(regex_t *r, char *name)
{
	/* speed up .* */
	if (r == (void *)1)
		return 0;

	if (regexec(r, name, 0, NULL, 0) == 0)
		return 0;

	return -1;
}

/* ---------- tree ---------- */

static int pathcmp(char *name, char **path)
{
	char *b;

	b = *path;
	while (*name != 0 && *name == *b) {
		name++;
		b++;
	}

	if (*name == 0 && (*b == 0 || *b == '/')) {
		*path = b;
		return 0;
	}

	if (*b > *name)
		return 1;
	return -1;
}

static struct tree_s *create_one_i(struct tree_s *base, char *name, int regexp)
{
	struct tree_s *p;
	struct tree_type_s *type;
	int l;

	if (*name == 0)
		return base;

	l = strlen(name);

	if (!regexp) {
		for (p = base->child; p != NULL; p = p->next)
			if (!strcmp(p->name, name))
				return p;
	} else {
		for (p = base->regex_child; p != NULL; p = p->next)
			if (!strcmp(p->name, name))
				return p;
	}

	type = base->type->child_type;
	if (type == NULL)
		type = base->type;
	p = malloc(type->size+l+1);
	if (p == NULL)
		return NULL;

	memset(p, 0, type->size+l+1);
	p->type = type;
	strncpy(p->name, name, l);
	p->name[l] = 0;
	p->parent = base;
	p->child = NULL;
	p->regex_child = NULL;
	p->primary_space = NULL;
	p->visited = false;

	p->events = comm_new_array(sizeof(struct tree_event_s));
	if (p->events == NULL) {
		free(p);
		return NULL;
	}

	/* speed up .* */
	if (base->compiled_regex == (void *)1
			&& base->child == NULL && base->regex_child == NULL
			&& !(regexp && !strcmp(p->name, ".*"))) {
		if (create_one_i(base, ".*", 1) == NULL) {
			free(p);
			return NULL;
		}
	}

	if (!regexp) {
		p->compiled_regex = NULL; /* important */
		p->next = base->child;
		base->child = p;
	} else {
		p->compiled_regex = regcompile(p->name);
		p->next = base->regex_child;
		base->regex_child = p;
	}

	if (p->type->init)
		p->type->init(p);

	/* speed up .* */
	if (!regexp || p->compiled_regex != (void *)1) {
		if (create_one_i(p, ".*", 1) == NULL)
			return NULL;
	}

	return p;
}

static struct tree_s *create_one(struct tree_s *base, char **name)
{
	char *n;
	int l = 0;

	while (**name == '/')
		(*name)++;
	n = *name;
	while (*n != 0 && *n != '/') {
		l++;
		n++;
	}

	{
		char tmp[l+1];

		strncpy(tmp, *name, l);
		tmp[l] = 0;
		*name = n;
		return create_one_i(base, tmp, isreg(tmp));
	}
}

struct tree_s *register_tree_type(struct tree_type_s *type)
{
	char *path;
	struct tree_s *t;

	global_root->type->child_type = type;
	path = type->name;
	t = create_one(global_root, &path);
	global_root->type->child_type = NULL;

	return t;
}

struct tree_s *create_path(char *path)
{
	struct tree_s *p;
	char *d;

	p = global_root;
	if (*path == '/') {
		d = default_path;
		if (d == NULL) {
			error("Invalid path begining with '/' without primary tree definition");
			return NULL;
		}

		while (*d != 0 && (p = create_one(p, &d)) != NULL)
			;
		if (p == NULL)
			return p;
	}

	while (*path != 0 && (p = create_one(p, &path)) != NULL)
		;
	return p;
}

struct tree_s *find_one(struct tree_s *base, char *name)
{
	struct tree_s *p;

	while (*name == '/')
		(name)++;
	if (*name == 0)
		return base;

	for (p = base->child; p != NULL; p = p->next) {
		if (!strcmp(p->name, name))
			return p;
	}

	for (p = base->regex_child; p != NULL; p = p->next) {
		if (!regcmp(p->compiled_regex, name))
			return p;
	}

	/* ... zeby este cyklus base=base->alt ??? a lepsie to retazit cez alt */
	if (base->child == NULL && base->regex_child == NULL)
		return base;

	return NULL;
}

static struct tree_s *find_one2(struct tree_s *base, char **name)
{
	struct tree_s *p;
	char *b;

	while (**name == '/')
		(*name)++;
	if (**name == 0)
		return base;

	for (p = base->child; p != NULL; p = p->next) {
		if (!pathcmp(p->name, name))
			return p;
	}

	for (b = *name; *b != 0 && *b != '/'; b++)
		;
	{
		char s[b - (*name) + 1];

		strncpy(s, *name, b - (*name));
		s[b - (*name)] = 0;
		for (p = base->regex_child; p != NULL; p = p->next) {
			if (!regcmp(p->compiled_regex, s)) {
				*name = b;
				return p;
			}
		}
	}

	if (base->child == NULL && base->regex_child == NULL)
		return base;

	return NULL;
}

struct tree_s *find_path(char *path)
{
	struct tree_s *p;
	char *d;

	p = global_root;
	if (*path == '/') {
		d = default_path;
		if (d == NULL) {
			error("Invalid path begining with '/' without primary tree definition");
			return NULL;
		}
		while (*d != 0 && (p = find_one2(p, &d)) != NULL)
			;
		if (p == NULL)
			return p;
	}

	while (*path != 0 && (p = find_one2(p, &path)) != NULL)
		;
	return p;
}

/* ----------------------------------- */

/* tree_get_rnth: n=0: t n=1: t->parnt ... */
static struct tree_s *tree_get_rnth(struct tree_s *t, int n)
{
	struct tree_s *p;

	for (p = t; n > 0; p = p->parent)
		n--;

	return p;
}

static void tree_for_alt_i(struct tree_s *t, struct tree_s *p, int n, void(*func)(struct tree_s *t, void *arg), void *arg)
{
	struct tree_s *r, *a;

	if (n == 0) {
		func(t, arg);
		return;
	}

	n--;
	r = tree_get_rnth(p, n);
	if (r->compiled_regex == NULL) {
		for (a = t->child; a != NULL; a = a->next)
			if (!strcmp(r->name, a->name))
				tree_for_alt_i(a, p, n, func, arg);
	} else {
		for (a = t->child; a != NULL; a = a->next)
			if (!regcmp(r->compiled_regex, a->name))
				tree_for_alt_i(a, p, n, func, arg);

		/* speed up .* */
		if (r->compiled_regex == (void *)1) {
			for (a = t->regex_child; a != NULL; a = a->next)
				tree_for_alt_i(a, p, n, func, arg);
		} else {
			for (a = t->regex_child; a != NULL; a = a->next)
				if (!strcmp(r->name, a->name))
					tree_for_alt_i(a, p, n, func, arg);
		}
	}
}

void tree_for_alternatives(struct tree_s *p, void(*func)(struct tree_s *t, void *arg), void *arg)
{
	int n;
	struct tree_s *t;

	for (n = 0, t = p->parent; t != NULL; t = t->parent)
		n++;

	tree_for_alt_i(tree_get_rnth(p, n), p, n, func, arg);
}

static void tree_is_equal_i(struct tree_s *p, void *arg)
{
	if (p == *((struct tree_s **)arg))
		*((struct tree_s **)arg) = NULL;
}

int tree_is_equal(struct tree_s *test, struct tree_s *p)
{
	int n;
	struct tree_s *t;

	for (n = 0, t = p->parent; t != NULL; t = t->parent)
		n++;

	tree_for_alt_i(tree_get_rnth(p, n), p, n, tree_is_equal_i, &test);
	if (test == NULL)
		return 1;
	return 0;
}

static void tree_is_offspring_i(struct tree_s *ancestor, void *arg)
{
	struct tree_s *offspring;

	offspring = *((struct tree_s **)arg);
	while (offspring != NULL) {
		if (offspring == ancestor) {
			*((struct tree_s **)arg) = NULL;
			break;
		}
		offspring = offspring->parent;
	}
}

int tree_is_offspring(struct tree_s *offspring, struct tree_s *ancestor)
{
	int n;
	struct tree_s *t;

	for (n = 0, t = ancestor->parent; t != NULL; t = t->parent)
		n++;

	tree_for_alt_i(tree_get_rnth(ancestor, n), ancestor, n, tree_is_offspring_i, &offspring);
	if (offspring == NULL)
		return 1;
	return 0;
}


static void _add_event_handlers(struct event_hadler_hash_s **from, struct event_hadler_hash_s **to)
{
	struct event_hadler_hash_s *h, *n;
	int l;
	char **errstr;

	for (l = 0; l < EHH_LISTS; l++) {
		for (h = from[l]; h != NULL; h = h->next) {
			n = evhash_add(&(to[l]), h->handler, h->evname);
			if (n == NULL) {
				errstr = (char **) pthread_getspecific(errstr_key);
				fatal("%s", *errstr);
			}
			vs_add(h->subject_vs, n->subject_vs);
			vs_add(h->object_vs, n->object_vs);
		}
	}
}

static void tree_node_merge(struct tree_s *r, struct tree_s *t)
{
	struct tree_s *rc, *tc;

	_add_event_handlers(r->subject_handlers, t->subject_handlers);
	_add_event_handlers(r->object_handlers, t->object_handlers);

	for (rc = r->child; rc != NULL; rc = rc->next) {
		tc = create_one_i(t, rc->name, 0);
		if (tc != NULL)
			tree_node_merge(rc, tc);
	}
	for (rc = r->regex_child; rc != NULL; rc = rc->next) {
		tc = create_one_i(t, rc->name, 1);
		if (tc != NULL)
			tree_node_merge(rc, tc);
	}
}

static void tree_apply_alts(struct tree_s *dir)
{
	struct tree_s *r, *t, *all = NULL;

	for (r = dir->regex_child; r != NULL; r = r->next) {
		/* speed up .* */
		if (r->compiled_regex == (void *)1)
			all = r;

		for (t = dir->child; t != NULL; t = t->next) {
			if (!regcmp(r->compiled_regex, t->name))
				tree_node_merge(r, t);
		}
	}

	if (all != NULL) {
		for (t = dir->regex_child; t != NULL; t = t->next) {
			if (t != all)
				tree_node_merge(all, t);
		}
	}

	for (t = dir->child; t != NULL; t = t->next)
		tree_apply_alts(t);
	for (t = dir->regex_child; t != NULL; t = t->next)
		tree_apply_alts(t);
}

int tree_expand_alternatives(void)
{
	tree_apply_alts(global_root);
	return 0;
}

/* ----------------------------------- */

char *tree_get_path(struct tree_s *t)
{
	static char buf[4096];
	static int pos;

	if (t->parent == NULL) {
		buf[(pos = 0)] = 0;
		return buf;
	}

	tree_get_path(t->parent);
	if (pos > 0)
		buf[pos++] = '/';

	strncpy(buf + pos, t->name, 4095 - pos);
	pos += strlen(t->name);

	if (pos > 4095)
		pos = 4095;
	buf[pos] = 0;

	return buf;
}

/* ----------------------------------- */

void tree_print_handlers(struct event_hadler_hash_s *h, char *name, void(*out)(int arg, char *), int arg)
{
	if (h != NULL) {
		out(arg, "  ");
		out(arg, name);
		out(arg, "=");

		while (h != NULL) {
			out(arg, "[");
			out(arg, h->handler->op_name);
#ifdef DEBUG_TRACE
			out(arg, ":");
			out(arg, h->handler->op_name + MEDUSA_OPNAME_MAX);
#endif
			out(arg, "]");
			h = h->next;
		}
	}
}

void tree_print_node(struct tree_s *t, int level, void(*out)(int arg, char *), int arg)
{
	char *s, buf[4096];
	int a, i;
	struct tree_s *p;
	static const char * const list_name_s[] = {"s:vs_allov", "s:vs_deny", "s:notify_allow", "s:notify_deny"};
	static const char * const list_name_o[] = {"o:vs_allov", "o:vs_deny", "o:notify_allow", "o:notify_deny"};

	s = tree_get_path(t);
	for (i = level; i > 0; i--)
		out(arg, "    ");
	out(arg, "\"");
	out(arg, s);
	out(arg, "\"");
	if (t->primary_space != NULL) {
		out(arg, " [");
		out(arg, t->primary_space->name);
		out(arg, "]");
	}
	out(arg, " vs=");
	if (space_vs_to_str(t->vs[AT_MEMBER], buf, 4096) < 0)
		out(arg, "???");
	else
		out(arg, buf);

	for (a = 1; a < NR_ACCESS_TYPES; a++) {
		if (!vs_isclear(t->vs[a])) {
			out(arg, " ");
			out(arg, access_names[a]);
			out(arg, "=");
			if (space_vs_to_str(t->vs[a], buf, 4096) < 0)
				out(arg, "???");
			else
				out(arg, buf);
		}
	}

	for (a = 0; a < EHH_LISTS; a++)
		tree_print_handlers(t->subject_handlers[a], (char *)list_name_s[a], out, arg);
	for (a = 0; a < EHH_LISTS; a++)
		tree_print_handlers(t->object_handlers[a], (char *)list_name_o[a], out, arg);
	out(arg, "\n");

	for (p = t->child; p != NULL; p = p->next)
		tree_print_node(p, level+1, out, arg);
	for (p = t->regex_child; p != NULL; p = p->next)
		tree_print_node(p, level+1, out, arg);
}
