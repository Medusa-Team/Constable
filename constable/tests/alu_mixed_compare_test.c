// SPDX-License-Identifier: GPL-2.0

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../language/execute.h"
#include "../language/language.h"

static unsigned int checks;
static unsigned int runtime_errors;

int runtime(const char *fmt, ...)
{
	(void)fmt;
	runtime_errors++;
	return -1;
}

static void init_register(struct register_s *reg, uint8_t type,
			  const void *value, size_t size)
{
	memset(reg, 0, sizeof(*reg));
	reg->tmp_attr.type = type;
	reg->tmp_attr.length = size;
	reg->attr = &reg->tmp_attr;
	reg->data = reg->buf;
	memcpy(reg->data, value, size);
}

static int result(struct register_s *reg)
{
	uint32_t value;

	memcpy(&value, reg->data, sizeof(value));
	return value != 0;
}

static int check32(uint8_t left_type, uint32_t left_value, int op,
		   uint8_t right_type, uint32_t right_value, int expected)
{
	struct register_s left;
	struct register_s right;

	init_register(&left, left_type, &left_value, sizeof(left_value));
	init_register(&right, right_type, &right_value, sizeof(right_value));
	do_bin_op(op, &left, &right);
	checks++;
	if (result(&left) == expected && left.attr->length == 4 &&
	    left.attr->type == MED_TYPE_UNSIGNED)
		return 0;
	fprintf(stderr, "32-bit mixed comparison %u failed\n", checks);
	return -1;
}

static int check64(uint8_t left_type, uint64_t left_value, int op,
		   uint8_t right_type, uint64_t right_value, int expected)
{
	struct register_s left;
	struct register_s right;

	init_register(&left, left_type, &left_value, sizeof(left_value));
	init_register(&right, right_type, &right_value, sizeof(right_value));
	do_bin_op(op, &left, &right);
	checks++;
	if (result(&left) == expected && left.attr->length == 4 &&
	    left.attr->type == MED_TYPE_UNSIGNED)
		return 0;
	fprintf(stderr, "64-bit mixed comparison %u failed\n", checks);
	return -1;
}

static int check_string_append(const char *prefix, uint8_t right_type,
			       const void *right_value, size_t right_size,
			       const char *expected)
{
	struct register_s left;
	struct register_s right;

	init_register(&left, MED_TYPE_STRING, prefix, strlen(prefix) + 1);
	init_register(&right, right_type, right_value, right_size);
	do_bin_op(oADD, &left, &right);
	checks++;
	if (!strcmp(left.data, expected) &&
	    left.attr->length == strlen(expected) + 1)
		return 0;
	fprintf(stderr, "string append check %u failed: got '%s'\n",
		checks, left.data);
	return -1;
}

static int arithmetic(uint8_t left_type, uint64_t x, int op,
		      uint8_t right_type, uint64_t y, size_t width,
		      int error, uint64_t expected)
{
	struct register_s left, right;
	unsigned int before = runtime_errors;
	int status;
	uint64_t actual = 0;
	uint32_t x32 = x, y32 = y;

	init_register(&left, left_type, width == 4 ? (void *)&x32 : (void *)&x, width);
	init_register(&right, right_type, width == 4 ? (void *)&y32 : (void *)&y, width);
	status = do_bin_op(op, &left, &right);
	if (width == 4) {
		uint32_t value;
		memcpy(&value, left.data, sizeof(value));
		actual = value;
		expected = (uint32_t)expected;
	} else
		memcpy(&actual, left.data, sizeof(actual));
	checks++;
	if (error ? (status < 0 && runtime_errors == before + 1) :
	    (status == 0 && runtime_errors == before && actual == expected))
		return 0;
	fprintf(stderr, "arithmetic check %u failed (op %d, width %zu)\n",
		checks, op, width);
	return -1;
}

static int arithmetic_boundaries(void)
{
	int failures = 0;
	const uint8_t u = MED_TYPE_UNSIGNED, s = MED_TYPE_SIGNED;
	for (size_t w = 4; w <= 8; w += 4) {
		uint64_t umax = w == 4 ? UINT32_MAX : UINT64_MAX;
		uint64_t smax = umax >> 1, smin = smax + 1;
#define A(t1, x, op, t2, y, error, expected) \
	failures += arithmetic(t1, x, op, t2, y, w, error, expected)
		A(s, smax, oADD, s, 1, 1, 0);
		A(u, umax, oADD, u, 1, 1, 0);
		A(s, smin, oSUB, s, 1, 1, 0);
		A(s, smax, oMUL, s, 2, 1, 0);
		A(u, umax, oMUL, u, 2, 1, 0);
		A(s, smin, oDIV, s, umax, 1, 0);
		A(s, smin, oMOD, s, umax, 1, 0);
		for (uint8_t t1 = u; t1 <= s; t1++)
			for (uint8_t t2 = u; t2 <= s; t2++) {
				A(t1, 1, oDIV, t2, 0, 1, 0);
				A(t1, 1, oMOD, t2, 0, 1, 0);
				A(t1, 1, oSHL, t2, w * 8, 1, 0);
				A(t1, 1, oSHR, t2, w * 8, 1, 0);
				A(t1, 6, oDIV, t2, 2, 0, 3);
				A(t1, 7, oMOD, t2, 2, 0, 1);
			}
		A(s, 1, oSHL, s, umax, 1, 0);
		A(u, 1, oSHR, s, umax, 1, 0);
		A(s, umax, oSHL, u, 1, 1, 0);
		A(s, smax, oSHL, u, 1, 1, 0);
		A(u, umax, oSHL, u, 1, 1, 0);
		A(u, 1, oSHL, u, w * 8 - 1, 0, smin);
		A(u, smin, oSHR, u, w * 8 - 1, 0, 1);
		A(s, smax - 1, oADD, s, 1, 0, smax);
		A(s, smin, oMUL, s, 1, 0, smin);
		A(u, 0, oSUB, u, 1, 0, umax);
		A(u, umax, oSUB, u, 0, 1, 0);
		A(s, umax, oADD, u, 1, 0, 0);
		A(u, 1, oADD, s, umax, 0, 0);
		A(u, smax, oADD, s, 1, 1, 0);
		A(s, umax, oMUL, u, 1, 0, umax);
#undef A
	}
	return failures;
}

static int comparison_then_arithmetic(void)
{
	struct register_s left, right;
	uint64_t x = UINT64_MAX, y = UINT64_MAX;
	uint64_t actual;
	init_register(&left, MED_TYPE_UNSIGNED, &x, sizeof(x));
	init_register(&right, MED_TYPE_UNSIGNED, &y, sizeof(y));
	if (do_bin_op(oEQ, &left, &right))
		return -1;
	y = UINT64_C(1) << 40;
	init_register(&right, MED_TYPE_UNSIGNED, &y, sizeof(y));
	if (do_bin_op(oADD, &left, &right))
		return -1;
	memcpy(&actual, left.data, sizeof(actual));
	checks++;
	return actual == y + 1 && left.attr->length == 8 ? 0 : -1;
}

static int widening(void)
{
	int failures = 0;
	for (size_t width = 1; width <= 4; width *= 2) {
		for (int negative = 0; negative <= 1; negative++) {
			struct register_s left, right;
			uint8_t x8 = negative ? UINT8_MAX : 1;
			uint16_t x16 = negative ? UINT16_MAX : 1;
			uint32_t x32 = negative ? UINT32_MAX : 1;
			uint64_t zero = 0, actual;
			const void *x = width == 1 ? (void *)&x8 :
				       width == 2 ? (void *)&x16 : (void *)&x32;
			init_register(&left, MED_TYPE_SIGNED, x, width);
			/* Bytes beyond the declared value must never affect widening. */
			memset(left.data + width, 0xa5, 8 - width);
			init_register(&right, MED_TYPE_SIGNED, &zero, 8);
			if (do_bin_op(oADD, &left, &right))
				failures--;
			memcpy(&actual, left.data, sizeof(actual));
			checks++;
			if (actual != (negative ? UINT64_MAX : 1))
				failures--;
		}
	}
	return failures;
}

int main(void)
{
	int failures = 0;
	const uint8_t high_byte = 0xff;
	const int32_t minimum = INT32_MIN;

	failures += check32(MED_TYPE_UNSIGNED, 0, oLT,
			    MED_TYPE_SIGNED, UINT32_MAX, 1);
	failures += check32(MED_TYPE_SIGNED, UINT32_MAX, oLT,
			    MED_TYPE_UNSIGNED, 0, 0);
	failures += check32(MED_TYPE_UNSIGNED, UINT32_MAX, oEQ,
			    MED_TYPE_SIGNED, UINT32_MAX, 1);
	failures += check32(MED_TYPE_SIGNED, UINT32_MAX, oGT,
			    MED_TYPE_UNSIGNED, 0, 1);
	failures += check32(MED_TYPE_SIGNED, UINT32_MAX - 1, oLT,
			    MED_TYPE_SIGNED, UINT32_MAX, 1);

	failures += check64(MED_TYPE_UNSIGNED, 0, oLT,
			    MED_TYPE_SIGNED, UINT64_MAX, 1);
	failures += check64(MED_TYPE_SIGNED, UINT64_MAX, oLT,
			    MED_TYPE_UNSIGNED, 0, 0);
	failures += check64(MED_TYPE_UNSIGNED, UINT64_MAX, oEQ,
			    MED_TYPE_SIGNED, UINT64_MAX, 1);
	failures += check64(MED_TYPE_SIGNED, UINT64_MAX, oGT,
			    MED_TYPE_UNSIGNED, 0, 1);
	failures += check64(MED_TYPE_SIGNED, UINT64_MAX - 1, oLT,
			    MED_TYPE_SIGNED, UINT64_MAX, 1);
	failures += check_string_append("x", MED_TYPE_BITMAP, &high_byte,
					sizeof(high_byte), "xff");
	failures += check_string_append("", MED_TYPE_SIGNED, &minimum,
					sizeof(minimum), "-2147483648");

	failures += arithmetic_boundaries();
	failures += comparison_then_arithmetic();
	failures += widening();
	if (failures)
		return 1;
	printf("ALU mixed comparisons: %u checks passed\n", checks);
	return 0;
}
