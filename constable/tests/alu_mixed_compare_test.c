// SPDX-License-Identifier: GPL-2.0

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../language/execute.h"
#include "../language/language.h"

static unsigned int checks;

int runtime(const char *fmt, ...)
{
	(void)fmt;
	return -1;
}

void r_imm(struct register_s *reg)
{
	(void)reg;
}

void r_resize(struct register_s *reg, int size)
{
	if (reg->attr != &reg->tmp_attr)
		reg->tmp_attr = *reg->attr;
	reg->attr = &reg->tmp_attr;
	reg->attr->length = size;
}

void do_bin_op(int op, struct register_s *left, struct register_s *right);

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
	if (result(&left) == expected)
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
	if (result(&left) == expected)
		return 0;
	fprintf(stderr, "64-bit mixed comparison %u failed\n", checks);
	return -1;
}

int main(void)
{
	int failures = 0;

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

	if (failures)
		return 1;
	printf("ALU mixed comparisons: %u checks passed\n", checks);
	return 0;
}
