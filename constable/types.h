/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Constable: types.h
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#ifndef _TYPES_H
#define _TYPES_H

#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>

/* Force a compilation error if condition is true, but also produce a
 * result (of value 0 and type int), so the expression can be used
 * e.g. in a structure initializer (or where-ever else comma expressions
 * aren't permitted).
 */
#define BUILD_BUG_ON_ZERO(e) ((int)(sizeof(struct { int:(-!!(e)); })))

/* Are two types/vars the same type (ignoring qualifiers)? */
#define __same_type(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))

/* &a[0] degrades to a pointer: a different type from an array */
#define __must_be_array(a) BUILD_BUG_ON_ZERO(__same_type((a), &(a)[0]))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))

#endif /* _TYPES_H */
