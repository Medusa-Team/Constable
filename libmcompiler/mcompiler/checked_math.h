/* SPDX-License-Identifier: GPL-2.0 */
#ifndef MCOMPILER_CHECKED_MATH_H
#define MCOMPILER_CHECKED_MATH_H

#include <stddef.h>
#include <stdint.h>

static inline int checked_size_add(size_t left, size_t right, size_t *result)
{
	if (!result || left > SIZE_MAX - right)
		return 0;
	*result = left + right;
	return 1;
}

static inline int checked_size_multiply(size_t left, size_t right,
					size_t *result)
{
	if (!result || (right && left > SIZE_MAX / right))
		return 0;
	*result = left * right;
	return 1;
}

#endif /* MCOMPILER_CHECKED_MATH_H */
