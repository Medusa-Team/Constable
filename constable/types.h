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
#include <stdbool.h>

/* Produce zero in expressions, but fail compilation when condition is true. */
#define BUILD_BUG_ON_ZERO(condition) \
	((int)(sizeof(struct { int dummy; int : -!!(condition); }) - \
	       sizeof(int)))

/* Compiler type introspection keeps accidental pointer use from compiling. */
#define SAME_TYPE(left, right) \
	__builtin_types_compatible_p(__typeof__(left), __typeof__(right))
#define MUST_BE_ARRAY(array) \
	BUILD_BUG_ON_ZERO(SAME_TYPE((array), &(array)[0]))
#define ARRAY_SIZE(array) \
	(sizeof(array) / sizeof((array)[0]) + MUST_BE_ARRAY(array))

#endif /* _TYPES_H */
