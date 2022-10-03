// SPDX-License-Identifier: GPL-2.0
/**
 * @file cmds.c
 * @short Buildin commands
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#include "execute.h"
#include "language.h"
#include "../constable.h"
#include "../tree.h"
#include "../space.h"
#include "../generic.h"
#include "../comm.h"
#include <unistd.h>

static BUILDIN_FUNC(cmd_constable_pid)
{
	*((uintptr_t *)(ret->data)) = (uintptr_t)getpid();
	return 0;
}

static BUILDIN_FUNC(cmd_hex)
{
	long long d;
	struct register_s r;
	struct object_s o;

	ret->attr = &(execute_attr_str);
	ret->data[0] = 0;
	if (!getarg(e, &r) || ((r.attr->type & 0x0f) != MED_TYPE_SIGNED && (r.attr->type & 0x0f) != MED_TYPE_UNSIGNED)) {
		runtime("hex(): 1st argument must be signed or unsigned int");
		return -1;
	}
	object_get_val(r2o(&r, &o), r.attr, &d, sizeof(d));
	sprintf(ret->data, "%llx", d);
	if (getarg(e, &r)) {
		runtime("hex(): too many arguments");
		return -1;
	}
	return 0;
}

static BUILDIN_FUNC(cmd_comm)
{
	ret->attr = &(execute_attr_str);
	strncpy(ret->data, e->my_comm_buff->comm->name, MAX_REG_SIZE);
	return 0;
}

static BUILDIN_FUNC(cmd_operation)
{
	obj_to_reg(ret, &(e->c->operation), NULL);
	return 0;
}

static BUILDIN_FUNC(cmd_subject)
{
	obj_to_reg(ret, &(e->c->subject), NULL);
	return 0;
}

static BUILDIN_FUNC(cmd_object)
{
	obj_to_reg(ret, &(e->c->object), NULL);
	return 0;
}

static BUILDIN_FUNC(cmd_nameof)
{
	struct register_s r;
	struct object_s *o;
	int tmp;

	ret->attr = &(execute_attr_str);
	ret->data[0] = 0;

	tmp = getarg(e, &r);
	o = r.object;
	if (!tmp || o == NULL) {
		runtime("nameof: argument must be kobject");
		return -1;
	}
	strncpy(ret->data, o->attr.name, MAX_REG_SIZE);
	return 0;
}

static BUILDIN_FUNC(cmd_str2path)
{
	struct register_s r;
	struct tree_s *t;

	ret->attr = &(execute_attr_int);
	ret->data[0] = 0;

	if (!getarg(e, &r) || (r.attr->type & 0x0f) != MED_TYPE_STRING) {
		runtime("str2path: argument must be string");
		return -1;
	}
	t = find_path((char *)(r.data));
	if (t == NULL) {
		runtime("str2path: can't find path %s", (char *)(r.data));
		return -1;
	}
	ret->attr = &(execute_attr_pointer);
	*(struct tree_s **)ret->data = t;
	return 0;
}

/* spaces(bitmap) */
static BUILDIN_FUNC(cmd_spaces)
{
	struct register_s r;
	struct object_s o;
	vs_t vs[VS_WORDS];

	ret->attr = &(execute_attr_str);
	ret->data[0] = 0;

	if (!getarg(e, &r) || ((r.attr->type & 0x0f) != MED_TYPE_BITMAP)) {
		runtime("spaces: 1st argument must be bitmap");
		return -1;
	}

	vs_clear(vs);
	object_get_val(r2o(&r, &o), r.attr, vs, sizeof(vs));
	if (getarg(e, &r)) {
		runtime("spaces: too many arguments");
		return -1;
	}
	if (space_vs_to_str(vs, ret->data, ret->attr->length) < 0) {
		runtime("spaces: output is too long");
		return -1;
	}
	return 0;
}

/* strshl(str,len) */
static BUILDIN_FUNC(cmd_strshl)
{
	struct register_s r;
	struct register_s r2;
	unsigned int len, slen;

	ret->attr = &(execute_attr_int);
	*((uintptr_t *)(ret->data)) = (uintptr_t)(0);

	if (!getarg(e, &r) || ((r.attr->type & 0x0f) != MED_TYPE_STRING)) {
		runtime("strshl: 1st argument must be string");
		return -1;
	}

	if (!getarg(e, &r2) || (((r2.attr->type & 0x0f) != MED_TYPE_UNSIGNED) && ((r2.attr->type & 0x0f) != MED_TYPE_SIGNED))) {
		runtime("strshl: 2nd argument must be signed or unsigned integer");
		return -1;
	}
	len = *((uintptr_t *)(r2.data));

	if (getarg(e, &r2)) {
		runtime("strshl: too many arguments");
		return -1;
	}

	slen = strnlen(r.data, r.attr->length);
	if (len > slen)
		len = slen;
	if (len < slen)
		memmove(r.data, r.data + len, slen-len);
	if ((slen - len) < r.attr->length)
		r.data[slen - len] = 0;
	*((uintptr_t *)(ret->data)) = (uintptr_t)(slen - len);
	return 0;
}

/* strcut(str,len) */
static BUILDIN_FUNC(cmd_strcut)
{
	struct register_s r;
	struct register_s r2;
	unsigned int len, slen;

	ret->attr = &(execute_attr_int);
	*((uintptr_t *)(ret->data)) = (uintptr_t)(0);

	if (!getarg(e, &r) || ((r.attr->type & 0x0f) != MED_TYPE_STRING)) {
		runtime("strcut: 1st argument must be string");
		return -1;
	}
	if (!getarg(e, &r2) || (((r2.attr->type & 0x0f) != MED_TYPE_UNSIGNED) && ((r2.attr->type & 0x0f) != MED_TYPE_SIGNED))) {
		runtime("strcut: 2nd argument must be signed or unsigned integer");
		return -1;
	}
	len = *((uintptr_t *)(r2.data));

	if (getarg(e, &r2)) {
		runtime("strcut: too many arguments");
		return -1;
	}

	slen = strnlen(r.data, r.attr->length);
	if (len > slen)
		len = slen;
	if (len < r.attr->length)
		r.data[len] = 0;
	*((uintptr_t *)(ret->data)) = (uintptr_t)(len);
	return 0;
}

/* sizeof(obj) */
static BUILDIN_FUNC(cmd_sizeof)
{
	struct register_s r;

	ret->attr = &(execute_attr_int);
	*((uintptr_t *)(ret->data)) = (uintptr_t)(0);

	if (!getarg(e, &r)) {
		runtime("sizeof: must have one argument");
		return -1;
	}
	*((uintptr_t *)(ret->data)) = (uintptr_t)(r.attr->length);
	if (getarg(e, &r)) {
		runtime("sizeof: too many arguments");
		return -1;
	}
	return 0;
}

/* primaryspace(process,@"strom") */
static BUILDIN_FUNC(cmd_primaryspace)
{
	struct register_s r;
	struct object_s *o;
	struct tree_s *t;
	struct space_s *space;
	int tmp;

	ret->attr = &(execute_attr_str);
	ret->data[0] = 0;

	tmp = getarg(e, &r);
	o = r.object;
	if (!tmp || o == NULL) {
		runtime("primaryspace: 1st argument must be kobject");
		return -1;
	}
	if (!getarg(e, &r) || r.attr != &execute_attr_pointer) {
		runtime("primaryspace: 2nd argument must be path");
		return -1;
	}
	t = *(struct tree_s **)r.data;
	if (getarg(e, &r)) {
		runtime("primaryspace: too many arguments");
		return -1;
	}
	if (t->type->class_handler == NULL || t->type->class_handler->get_primary_space == NULL) {
		runtime("primaryspace: Invalid path");
		return -1;
	}
	space = t->type->class_handler->get_primary_space(t->type->class_handler, e->my_comm_buff->comm, o);
	if (space  == NULL) {
		char **errstr = (char **) pthread_getspecific(errstr_key);

		if (*errstr == NULL)
			return 0;
		runtime("primaryspace: %s", *errstr);
		return -1;
	}
	strncpy(ret->data, space->name, MAX_REG_SIZE);
	return 0;
}

/* enter(process,@"kam") */
static BUILDIN_FUNC(cmd_enter)
{
	struct register_s r;
	struct object_s *o;
	struct tree_s *t;
	int i, tmp;

	tmp = getarg(e, &r);
	o = r.object;
	if (!tmp || o == NULL) {
		runtime("enter: 1st argument must be kobject");
		return -1;
	}
	if (!getarg(e, &r) || r.attr != &execute_attr_pointer) {
		runtime("enter: 2nd argument must be path");
		return -1;
	}
	t = *(struct tree_s **)r.data;
	if (getarg(e, &r)) {
		runtime("enter: too many arguments");
		return -1;
	}
	if (t->type->class_handler == NULL || t->type->class_handler->enter_tree_node == NULL) {
		runtime("enter: Invalid path");
		return -1;
	}
	i = t->type->class_handler->enter_tree_node(t->type->class_handler, e->my_comm_buff->comm, o, t);
	if (i < 0) {
		char **errstr = (char **) pthread_getspecific(errstr_key);

		runtime("enter: %s", *errstr);
		i = 0;
	}
	*((uintptr_t *)(ret->data)) = (uintptr_t)(i);
	return 0;
}

int cmds_init(void)
{
	lex_addkeyword("constable_pid", Tbuildin, (uintptr_t)cmd_constable_pid);
	lex_addkeyword("hex", Tbuildin, (uintptr_t)cmd_hex);
	lex_addkeyword("_comm", Tbuildin, (uintptr_t)cmd_comm);
	lex_addkeyword("_operation", Tbuildin, (uintptr_t)cmd_operation);
	lex_addkeyword("_subject", Tbuildin, (uintptr_t)cmd_subject);
	lex_addkeyword("_object", Tbuildin, (uintptr_t)cmd_object);
	lex_addkeyword("nameof", Tbuildin, (uintptr_t)cmd_nameof);
	lex_addkeyword("str2path", Tbuildin, (uintptr_t)cmd_str2path);
	lex_addkeyword("spaces", Tbuildin, (uintptr_t)cmd_spaces);
	lex_addkeyword("primaryspace", Tbuildin, (uintptr_t)cmd_primaryspace);
	lex_addkeyword("enter", Tbuildin, (uintptr_t)cmd_enter);
	//	lex_addkeyword("log", Tbuildin, (uintptr_t)cmd_log);
	lex_addkeyword("strshl", Tbuildin, (uintptr_t)cmd_strshl);
	lex_addkeyword("strcut", Tbuildin, (uintptr_t)cmd_strcut);
	lex_addkeyword("sizeof", Tbuildin, (uintptr_t)cmd_sizeof);
	return 0;
}


