// SPDX-License-Identifier: GPL-2.0
/*
 * Constable: object_endian.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#include "object.h"
#include <endian.h>
#include <sys/param.h>
#include <string.h>

void memrcpy(void *dest, const void *src, size_t n)
{
	while (n > 0) {
		*((char *)dest) = ((char *)src)[--n];
		dest++;
	}
}

int object_get_val(struct object_s *o, struct medusa_attribute_s *a, void *buf, int maxlen)
{
	int n;
	int s = 0;

	switch (a->type & 0x0f) {
	case MED_TYPE_STRING:
		n = MIN(a->length, maxlen);
		strncpy(buf, o->data+a->offset, n);
		((char *)(buf))[(maxlen > n ? n : n-1)] = 0;
		return 0;
	case MED_TYPE_BITMAP:
		n = MIN(a->length, maxlen);
		memcpy(buf, o->data+a->offset, n);
		if (maxlen > n)
			memset(buf+n, 0, maxlen-n);
		return 0;
	case MED_TYPE_SIGNED:
		s = 1;
	case MED_TYPE_UNSIGNED:
		n = MIN(a->length, maxlen);
#if __BYTE_ORDER == __LITTLE_ENDIAN
		if (o->flags & OBJECT_FLAG_CHENDIAN)
			memrcpy(buf, o->data+a->offset+a->length-n, n);
		else
			memcpy(buf, o->data+a->offset, n);
		if (maxlen > n) {
			if (s && ((char *)buf)[n-1] & 0x80)
				memset(buf+n, 0xff, maxlen-n);
			else
				memset(buf+n, 0, maxlen-n);
		}
#elif __BYTE_ORDER == __BIG_ENDIAN
		if (o->flags & OBJECT_FLAG_CHENDIAN)
			memrcpy(buf+maxlen-n, o->data+a->offset, n);
		else
			memcpy(buf+maxlen-n, o->data+a->offset+a->length-n, n);
		if (maxlen > n) {
			if (s && ((char *)buf)[maxlen-n] & 0x080)
				memset(buf, 0xff, maxlen-n);
			else
				memset(buf, 0, maxlen-n);
		}
#else
#error "Unsupported endian type"
#endif
		return 0;
	}

	return -1;
}

int object_set_val(struct object_s *o, struct medusa_attribute_s *a, void *buf, int maxlen)
{
	int n;
	int s = 0;

	switch (a->type & 0x0f) {
	case MED_TYPE_STRING:
		n = MIN(a->length, maxlen);
		strncpy(o->data+a->offset, buf, n);
		((char *)(o->data+a->offset))[(a->length > n?n:n-1)] = 0;
		return 0;
	case MED_TYPE_BITMAP:
		n = MIN(a->length, maxlen);
		memcpy(o->data+a->offset, buf, n);
		if (a->length > n)
			memset(o->data+a->offset+n, 0, a->length-n);
		return 0;
	case MED_TYPE_SIGNED:
		s = 1;
	case MED_TYPE_UNSIGNED:
		n = MIN(a->length, maxlen);
#if __BYTE_ORDER == __LITTLE_ENDIAN
		if (o->flags & OBJECT_FLAG_CHENDIAN) {
			memrcpy(o->data+a->offset+a->length-n, buf, n);
			if (a->length > n) {
				if (s && ((char *)buf)[maxlen-1] & 0x80)
					memset(o->data+a->offset, 0xff, a->length-n);
				else
					memset(o->data+a->offset, 0, a->length-n);
			}
		} else {
			memcpy(o->data+a->offset, buf, n);
			if (a->length > n) {
				if (s && ((char *)buf)[maxlen-1] & 0x80)
					memset(o->data+a->offset+n, 0xff, a->length-n);
				else
					memset(o->data+a->offset+n, 0, a->length-n);
			}
		}
#elif __BYTE_ORDER == __BIG_ENDIAN
		if (o->flags & OBJECT_FLAG_CHENDIAN) {
			memrcpy(o->data+a->offset, buf+maxlen-n, n);
			if (a->length > n) {
				if (s && ((char *)buf)[0] & 0x80)
					memset(o->data+a->offset+n, 0xff, a->length-n);
				else
					memset(o->data+a->offset+n, 0, a->length-n);
			}
		} else {
			memcpy(o->data+a->offset+a->length-n, buf+maxlen-n, n);
			if (a->length > n) {
				if (s && ((char *)buf)[0] & 0x80)
					memset(o->data+a->offset, 0xff, a->length-n);
				else
					memset(o->data+a->offset, 0, a->length-n);
			}
		}
#else
#error "Unsupported endian type"
#endif
		return 0;
	}

	return -1;
}

static void object_swap_endian(struct object_s *o, struct medusa_attribute_s *a)
{
	int n;
	char c, *d;

	switch (a->type & 0x0f) {
	case MED_TYPE_SIGNED:
	case MED_TYPE_UNSIGNED:
		d = o->data + a->offset;
		n = a->length-1;
		while (n > 0) {
			c = *d;
			*d = *(d+n);
			*(d+n) = c;
			d++;
			n -= 2;
		}
		break;
	}
}

int object_copy(struct object_s *d, struct object_s *s)
{
	struct medusa_attribute_s *a;

	if (d->class != s->class)
		return -1;

	memcpy(d->data, s->data, d->class->m.size);
	if ((d->flags&OBJECT_FLAG_CHENDIAN) != (s->flags&OBJECT_FLAG_CHENDIAN)) {
		for (a = d->class->attr; a->type != MED_TYPE_END; a++)
			object_swap_endian(d, a);
	}

	return 0;
}

int object_set_byte_order(struct object_s *o, int flags)
{
	struct medusa_attribute_s *a;

	if ((flags&OBJECT_FLAG_CHENDIAN) != (o->flags&OBJECT_FLAG_CHENDIAN)) {
		o->flags = (o->flags & ~OBJECT_FLAG_CHENDIAN) | (flags & OBJECT_FLAG_CHENDIAN);
		for (a = o->class->attr; a->type != MED_TYPE_END; a++)
			object_swap_endian(o, a);
	}

	return 0;
}

static int object_resize_data_short(void *buf, struct medusa_attribute_s *a, int newlen)
{
	switch (a->type & 0x0f) {
	case MED_TYPE_STRING:
		((char *)buf)[newlen-1] = 0;
		return 0;
	case MED_TYPE_BITMAP:
		return 0;
	case MED_TYPE_SIGNED:
	case MED_TYPE_UNSIGNED:
#if __BYTE_ORDER == __LITTLE_ENDIAN
#elif __BYTE_ORDER == __BIG_ENDIAN
		memmove(buf, buf+a->length-newlen, newlen);
#else
#error "Unsupported endian type"
#endif
		return 0;
	}

	return -1;
}

int object_resize_data(void *buf, struct medusa_attribute_s *a, int newlen)
{
	int s = 0;

	if (a->length == newlen)
		return 0;
	if (a->length > newlen)
		return object_resize_data_short(buf, a, newlen);
	switch (a->type & 0x0f) {
	case MED_TYPE_STRING:
		return 0;
	case MED_TYPE_BITMAP:
		memset(buf+a->length, 0, newlen-a->length);
		return 0;
	case MED_TYPE_SIGNED:
		s = 1;
	case MED_TYPE_UNSIGNED:
#if __BYTE_ORDER == __LITTLE_ENDIAN
		if (s && ((char *)buf)[a->length-1] & 0x80)
			s = 0xff;
		memset(buf+a->length, s, newlen-a->length);
#elif __BYTE_ORDER == __BIG_ENDIAN
		if (s && ((char *)buf)[0] & 0x80)
			s = 0xff;
		memmove(buf+newlen-a->length, buf, a->length);
		memset(buf, s, newlen-a->length);
#else
#error "Unsupported endian type"
#endif
		return 0;
	}

	return -1;
}

void byte_reorder_attrs(int flags, struct medusa_attribute_s *a)
{
	if (flags & OBJECT_FLAG_CHENDIAN) {
		while (a->type != MED_COMM_TYPE_END) {
			a->offset = bswap_16(a->offset);
			a->length = bswap_16(a->length);
			a++;
		}
	}
}

void byte_reorder_class(int flags, struct medusa_class_s *c)
{
	if (flags & OBJECT_FLAG_CHENDIAN)
		c->size = bswap_16(c->size);
}

void byte_reorder_acctype(int flags, struct medusa_acctype_s *a)
{
	if (flags & OBJECT_FLAG_CHENDIAN) {
		a->size = bswap_16(a->size);
		a->actbit = bswap_16(a->actbit);
	}
}
