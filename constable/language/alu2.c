// SPDX-License-Identifier: GPL-2.0
/**
 * @file alu2.c
 * @short Functions for manipulation with variables
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#include "execute.h"
#include "../constable.h"
#include "../object.h"
#include <sys/param.h>
#include <stdio.h>

struct object_s *r2o(const struct register_s *r, struct object_s *o)
{
	if (r->data == r->buf)
		o->data = r->data - r->attr->offset;
	else
		o->data = r->data;
	o->flags = r->flags;
	o->class = r->class;

	return o;
}

int r_int(struct register_s *r)
{
	int d = 0;
	struct object_s o;

	switch (r->attr->type & 0x0f) {
	case MED_TYPE_SIGNED:
	case MED_TYPE_UNSIGNED:
		object_get_val(r2o(r, &o), r->attr, &d, sizeof(d));
		return d;
	}
	return 0;
}

#define LEN_ALIGN(x)	(((x) + (sizeof(uintptr_t) - 1)) & (~(sizeof(uintptr_t) - 1)))
#define LEN_MAX		(MAX_REG_SIZE&(~(sizeof(uintptr_t) - 1)))

void r_imm(struct register_s *r)
{
	int n;

	if (r->data == r->buf)
		return;
	n = LEN_ALIGN(r->attr->length);
	if (n > MAX_REG_SIZE) {
		runtime("Variable too long");
		n = LEN_MAX;
	}
	if (r->attr->type == MED_TYPE_END && n > 0)
		memcpy(r->buf, r->data + r->attr->offset, n);
	else {
		struct object_s o;

		object_get_val(r2o(r, &o), r->attr, r->buf, n);
		r->flags &= ~OBJECT_FLAG_CHENDIAN;
	}
	r->data = r->buf;
	r->flags &= ~OBJECT_FLAG_LOCAL;
	r->object = NULL;
}

void r_sto(struct register_s *v, struct register_s *d)
{
	u_int8_t type, typed;
	struct medusa_attribute_s tmpa;
	struct object_s o;

	if (v->data == v->buf) {
		runtime("Invalid lvalue");
		//if ((d->attr->type & 0x0f) == MED_TYPE_STRING)
		printf("ZZZ: %s: str=\"%s\"\n", __func__, d->data);
		return;
	}
	if (!(v->flags & OBJECT_FLAG_LOCAL) && v->attr->type & MED_TYPE_READ_ONLY) {
		runtime("Changing read-only varible");
		return;
	}
	type = v->attr->type & 0x0f;
	typed = d->attr->type & 0x0f;
	if (type == MED_TYPE_UNSIGNED && typed == MED_TYPE_SIGNED)
		type = MED_TYPE_UNSIGNED;
	else if (type == MED_TYPE_SIGNED && typed == MED_TYPE_UNSIGNED)
		type = MED_TYPE_UNSIGNED;
	else if (type == MED_TYPE_BITMAP && typed == MED_TYPE_SIGNED)
		type = MED_TYPE_SIGNED;
	else if (type == MED_TYPE_BITMAP && typed == MED_TYPE_UNSIGNED)
		type = MED_TYPE_UNSIGNED;
	else if (type != typed) {
		runtime("Illegal type conversion [%1x->%1x]", typed, type);
		return;
	}
	if (type == MED_TYPE_END) {
		struct object_s o2;

		if (object_copy(r2o(v, &o), r2o(d, &o2)) < 0)
			runtime("Class of object don't match");
		return;
	}

	r_imm(d);	/* FIXME: spravit to krajsie - toto je iba kvoli byte orderu ;-( */
	memcpy(&tmpa, v->attr, sizeof(tmpa));
	tmpa.type = type;
	object_set_val(r2o(v, &o), &tmpa, d->data, d->attr->length);
}

void r_resize(struct register_s *v, int size)
{
	int nv;

	if (v->data != v->buf)
		r_imm(v);
	nv = v->attr->length;
	switch (v->attr->type & 0x0f) {
	case MED_TYPE_END:
		runtime("Object can't be resized");
		return;
	case MED_TYPE_UNSIGNED:
	case MED_TYPE_SIGNED:
		nv = LEN_ALIGN(nv);
	}
	if (nv >= size)
		return;
	object_resize_data(v->data, v->attr, size);
}
