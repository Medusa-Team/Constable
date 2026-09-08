/* SPDX-License-Identifier: GPL-2.0 */
#ifndef MCOMPILER_CHECKED_MATH_H
#define MCOMPILER_CHECKED_MATH_H

#include <stddef.h>
#include <stdint.h>

static inline int checked_size_add(size_t left, size_t right, size_t *result)
{
	return !__builtin_add_overflow(left, right, result);
}

static inline int checked_size_multiply(size_t left, size_t right,
					size_t *result)
{
	return !__builtin_mul_overflow(left, right, result);
}

#endif /* MCOMPILER_CHECKED_MATH_H */
