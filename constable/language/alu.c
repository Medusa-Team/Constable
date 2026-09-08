// SPDX-License-Identifier: GPL-2.0
/**
 * @file alu.c
 * @short functions of Arithmetic and Logic Unit
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#include <sys/types.h>
#include "../constable.h"
#include "../medusa_object.h"
#include "../string_utils.h"
#include "execute.h"
#include "language.h"
#include <sys/param.h>
#include <inttypes.h>

/* !!!!!!!!!!!!!!!!!!!!!! */
typedef	int32_t	s_int32_t;
typedef	int64_t	s_int64_t;
/*
 * Mixed signed/unsigned operations historically use the C usual arithmetic
 * conversions. Giving both macro operands that resulting type makes the
 * conversion explicit and keeps -Wsign-compare useful elsewhere.
 */
typedef u_int32_t uu_int32_t;
typedef u_int32_t us_int32_t;
typedef u_int32_t su_int32_t;
typedef s_int32_t ss_int32_t;
typedef u_int64_t uu_int64_t;
typedef u_int64_t us_int64_t;
typedef u_int64_t su_int64_t;
typedef s_int64_t ss_int64_t;

#define TYPE_u	MED_TYPE_UNSIGNED
#define TYPE_s	MED_TYPE_SIGNED
#define TYPE_b	MED_TYPE_BITMAP

#define OPbb(name, op) \
static int r_##name##_bb(struct register_s *v, struct register_s *d) \
{							\
	int i, n, nv, nd;				\
							\
	nv = v->attr->length;				\
	nd = d->attr->length;				\
	n = MAX((nv+3) >> 2, (nd+3) >> 2) << 2;	\
	r_resize(v, n);					\
	r_resize(d, n);					\
	v->tmp_attr.length = n;				\
	v->tmp_attr.type = TYPE_b;			\
	v->attr = &(v->tmp_attr);			\
							\
	for (i = 0; i < n; i++)				\
		((unsigned char *)(v->data))[i] op(((unsigned char *)(d->data))[i]); \
	return 0; \
}

OPbb(add, |=)
OPbb(sub, &= ~)
OPbb(and, &=)
OPbb(or, |=)
OPbb(xor, ^=)

#define OPbx(name, t2, op) \
static int r_##name##_b##t2(struct register_s *v, struct register_s *d) \
{							\
	t2##_int32_t i, nv;				\
							\
	i = ((t2##_int32_t *)(d->data))[0];		\
	nv = 8 * v->attr->length;			\
	if (i > nv) {					\
		runtime("Cannot set bit %d of %d-bit long bitfield", i, nv); \
		return -1;					\
	}						\
							\
	op((char *)(v->data), i);			\
	return 0; \
}

OPbx(add, s, setbit)
OPbx(add, u, setbit)
OPbx(sub, s, clrbit)
OPbx(sub, u, clrbit)

/* Reject invalid arithmetic before evaluating a C expression. The overflow
 * builtins check the mathematical result against the declared result type,
 * including mixed signed/unsigned inputs and the signed result of subtraction.
 */
#define NEGATIVE_u(value) 0
#define NEGATIVE_s(value) ((value) < 0)
#define INTEGER_MAX_u32 UINT32_MAX
#define INTEGER_MAX_s32 INT32_MAX
#define INTEGER_MAX_u64 UINT64_MAX
#define INTEGER_MAX_s64 INT64_MAX
#define CHECK_add(t1, t2, t3, bits) \
	__builtin_add_overflow(x, y, &checked_result)
#define CHECK_sub(t1, t2, t3, bits) \
	__builtin_sub_overflow(x, y, &checked_result)
#define CHECK_mul(t1, t2, t3, bits) \
	__builtin_mul_overflow(x, y, &checked_result)
#define BAD_DIVISOR(t1, t2, bits) \
	(y == 0 || (TYPE_##t1 == MED_TYPE_SIGNED && \
	 TYPE_##t2 == MED_TYPE_SIGNED && \
	 x == (t1##_int##bits##_t)INT##bits##_MIN && \
	 y == (t2##_int##bits##_t)-1))
#define CHECK_div(t1, t2, t3, bits) \
	(BAD_DIVISOR(t1, t2, bits) || \
	 __builtin_add_overflow(x / y, 0, &checked_result))
#define CHECK_mod(t1, t2, t3, bits) \
	(BAD_DIVISOR(t1, t2, bits) || \
	 __builtin_add_overflow(x % y, 0, &checked_result))
#define BAD_SHIFT(t2, bits) \
	(NEGATIVE_##t2(y) || (uint64_t)y >= bits)
#define CHECK_shl(t1, t2, t3, bits) \
	(BAD_SHIFT(t2, bits) || NEGATIVE_##t1(x) || \
	 (uint64_t)x > ((uint64_t)INTEGER_MAX_##t3##bits >> y) || \
	 (checked_result = (t3##_int##bits##_t)(x << y), 0))
#define CHECK_shr(t1, t2, t3, bits) \
	(BAD_SHIFT(t2, bits) || \
	 (checked_result = (t3##_int##bits##_t)(x >> y), 0))
#define CHECK_and(t1, t2, t3, bits) \
	(checked_result = (t3##_int##bits##_t)(x & y), 0)
#define CHECK_or(t1, t2, t3, bits) \
	(checked_result = (t3##_int##bits##_t)(x | y), 0)
#define CHECK_xor(t1, t2, t3, bits) \
	(checked_result = (t3##_int##bits##_t)(x ^ y), 0)

#define OPi(name, t1, t2, t3) \
static int r_##name##_##t1##t2(struct register_s *v, struct register_s *d) \
{							\
	int nv, nd, n;					\
							\
	nv = v->attr->length;				\
	nd = d->attr->length;				\
	n = MAX((nv+3) >> 2, (nd+3) >> 2);		\
	r_resize(v, n << 2);				\
	r_resize(d, n << 2);				\
	v->tmp_attr.length = n << 2;			\
	v->tmp_attr.type = TYPE_##t3;			\
	v->attr = &(v->tmp_attr);			\
	if (n == 1) {					\
		t1##_int32_t x;				\
		t2##_int32_t y;				\
		t3##_int32_t checked_result; \
							\
		x = ((t1##_int32_t *)(v->data))[0];	\
		y = ((t2##_int32_t *)(d->data))[0];	\
		if (CHECK_##name(t1, t2, t3, 32)) { \
			runtime("Invalid integer operation " #name); \
			return -1; \
		} \
		((t3##_int32_t *)(v->data))[0] = checked_result; \
	} else {					\
		t1##_int64_t x;				\
		t2##_int64_t y;				\
		t3##_int64_t checked_result; \
							\
		x = ((t1##_int64_t *)(v->data))[0];	\
		y = ((t2##_int64_t *)(d->data))[0];	\
		if (CHECK_##name(t1, t2, t3, 64)) { \
			runtime("Invalid integer operation " #name); \
			return -1; \
		} \
		((t3##_int64_t *)(v->data))[0] = checked_result; \
	}						\
	return 0; \
}

/* Relational operators always produce a 32-bit unsigned Boolean (0 or 1).
 * Operand width selects the comparison width, not the result width. R_push()
 * serializes only tmp_attr.length bytes; later arithmetic extends that Boolean.
 */
#define ROPi(name, t1, t2, op)				\
static int r_##name##_##t1##t2(struct register_s *v, struct register_s *d) \
{							\
	int nv, nd, n;					\
							\
	nv = v->attr->length;				\
	nd = d->attr->length;				\
	n = MAX((nv+3) >> 2, (nd+3) >> 2);		\
	r_resize(v, n << 2);				\
	r_resize(d, n << 2);				\
	v->tmp_attr.length = 4;				\
	v->tmp_attr.type = MED_TYPE_UNSIGNED;		\
	v->attr = &(v->tmp_attr);			\
	if (n == 1) {					\
		t1##t2##_int32_t x;			\
		t1##t2##_int32_t y;			\
							\
		x = ((t1##_int32_t *)(v->data))[0];	\
		y = ((t2##_int32_t *)(d->data))[0];	\
		((u_int32_t *)(v->data))[0] = (u_int32_t)(op); \
	} else {					\
		t1##t2##_int64_t x;			\
		t1##t2##_int64_t y;			\
							\
		x = ((t1##_int64_t *)(v->data))[0];	\
		y = ((t2##_int64_t *)(d->data))[0];	\
		((u_int32_t *)(v->data))[0] = (u_int32_t)(op); \
	}						\
	return 0; \
}

#define ROPb(name, op) \
static int r_##name##_bb(struct register_s *v, struct register_s *d) \
{							\
	int nv, nd, n, i;				\
							\
	nv = v->attr->length;				\
	nd = d->attr->length;				\
	n = MAX((nv+3) >> 2, (nd+3) >> 2);		\
	r_resize(v, n << 2);				\
	r_resize(d, n << 2);				\
	v->tmp_attr.length = 4;				\
	v->tmp_attr.type = MED_TYPE_UNSIGNED;		\
	v->attr = &(v->tmp_attr);			\
	for (i = 0; i < n; i++) {			\
		if (!((((unsigned char *)(v->data))[i]) op(((unsigned char *)(d->data))[i]))) { \
			((u_int32_t *)(v->data))[0] = 0; \
			return 0;				\
		}					\
	}						\
	((u_int32_t *)(v->data))[0] = 1;		\
	return 0; \
}

#define ROPc(name, op) \
static int r_##name##_cc(struct register_s *v, struct register_s *d) \
{							\
	int nv, nd, i;					\
	char a, b;					\
							\
	nv = v->attr->length;				\
	nd = d->attr->length;				\
	v->tmp_attr.length = 4;				\
	v->tmp_attr.type = MED_TYPE_SIGNED;		\
	v->attr = &(v->tmp_attr);			\
	for (i = 0; i < nv && i < nd; i++) {		\
		b = ((char *)(d->data))[i];		\
		a = ((char *)(v->data))[i];		\
		if (a == 0 || b == 0 || a != b)		\
			break;				\
	}						\
	if (i >= nv)					\
		a = 0;					\
	if (i >= nd)					\
		b = 0;					\
	((u_int32_t *)(v->data))[0] = (u_int32_t)(a op b); \
	return 0; \
}

OPi(add, u, u, u)
OPi(add, u, s, s)
OPi(add, s, u, s)
OPi(add, s, s, s)

OPi(sub, u, u, s)
OPi(sub, u, s, s)
OPi(sub, s, u, s)
OPi(sub, s, s, s)

OPi(mul, u, u, u)
OPi(mul, u, s, s)
OPi(mul, s, u, s)
OPi(mul, s, s, s)

OPi(div, u, u, u)
OPi(div, u, s, s)
OPi(div, s, u, s)
OPi(div, s, s, s)

OPi(mod, u, u, u)
OPi(mod, u, s, s)
OPi(mod, s, u, s)
OPi(mod, s, s, s)

OPi(shl, u, u, u)
OPi(shl, u, s, u)
OPi(shl, s, u, s)
OPi(shl, s, s, s)

OPi(shr, u, u, u)
OPi(shr, u, s, u)
OPi(shr, s, u, s)
OPi(shr, s, s, s)

OPi(and, u, u, u)
OPi(and, u, s, s)
OPi(and, s, u, s)
OPi(and, s, s, s)

OPi(or, u, u, u)
OPi(or, u, s, s)
OPi(or, s, u, s)
OPi(or, s, s, s)

OPi(xor, u, u, u)
OPi(xor, u, s, s)
OPi(xor, s, u, s)
OPi(xor, s, s, s)

ROPi(lt, u, u, x < y)
ROPi(lt, u, s, x < y)
ROPi(lt, s, u, x < y)
ROPi(lt, s, s, x < y)
ROPc(lt, <)

ROPi(gt, u, u, x > y)
ROPi(gt, u, s, x > y)
ROPi(gt, s, u, x > y)
ROPi(gt, s, s, x > y)
ROPc(gt, >)

ROPi(le, u, u, x <= y)
ROPi(le, u, s, x <= y)
ROPi(le, s, u, x <= y)
ROPi(le, s, s, x <= y)
ROPc(le, <=)

ROPi(ge, u, u, x >= y)
ROPi(ge, u, s, x >= y)
ROPi(ge, s, u, x >= y)
ROPi(ge, s, s, x >= y)
ROPc(ge, >=)

ROPi(eq, u, u, x == y)
ROPi(eq, u, s, x == y)
ROPi(eq, s, u, x == y)
ROPi(eq, s, s, x == y)
ROPb(eq, ==)
ROPc(eq, ==)

ROPi(ne, u, u, x != y)
ROPi(ne, u, s, x != y)
ROPi(ne, s, u, x != y)
ROPi(ne, s, s, x != y)
ROPb(ne, !=)
ROPc(ne, !=)

void r_neg(struct register_s *v)
{
	int n, i;

	n = (v->attr->length+3) >> 2;
	for (i = 0; i < n; i++)
		((u_int32_t *)(v->data))[i] = ~((u_int32_t *)(v->data))[i];
}

void r_not(struct register_s *v)
{
	int n, i;

	n = (v->attr->length+3) >> 2;
	v->tmp_attr.length = 4;
	v->tmp_attr.type = MED_TYPE_UNSIGNED;
	v->attr = &(v->tmp_attr);
	for (i = 0; i < n; i++) {
		if (((u_int32_t *)(v->data))[i]) {
			((u_int32_t *)(v->data))[0] = 0;
			return;
		}
	}
	((u_int32_t *)(v->data))[0] = 1;
}

int r_nz(struct register_s *v)
{
	int n, i;

	n = (v->attr->length+3) >> 2;
	for (i = 0; i < n; i++) {
		if (((u_int32_t *)(v->data))[i])
			return 1;
	}
	return 0;
}

/* ----------- strings ----------- */

static int r_add_cc(struct register_s *v, struct register_s *d)
{
	int l, x;

	l = v->attr->length;
	l = strnlen(v->data, l);
	x = strnlen(d->data, d->attr->length);
	if (v->attr != &(v->tmp_attr)) {
		v->tmp_attr = *(v->attr);
		v->attr = &(v->tmp_attr);
	}
	v->attr->length = l + x + 1;
	if (v->attr->length < MAX_REG_SIZE) {
		memcpy(v->data + l, d->data, v->attr->length - l - 1);
		v->data[v->attr->length - 1] = 0;
	} else {
		memcpy(v->data + l, d->data, MAX_REG_SIZE - l);
		v->attr->length = MAX_REG_SIZE;
	}
	return 0;
}

#define OPci(t2, f) \
static int r_add_c##t2(struct register_s *v, struct register_s *d) \
{							\
	int l, nd, written;				\
	t2##_int64_t y;					\
							\
	l = v->attr->length;				\
	l = strnlen(v->data, l);			\
	if (v->attr != &(v->tmp_attr)) {		\
		v->tmp_attr = *(v->attr);		\
		v->attr = &(v->tmp_attr);		\
	}						\
	nd = (d->attr->length) >> 2;			\
	if (nd == 1)					\
		y = ((t2##_int32_t *)(d->data))[0];	\
	else						\
		y = ((t2##_int64_t *)(d->data))[0];	\
	written = snprintf(v->data + l, MAX_REG_SIZE - l, f, y);	\
	if (written < 0) {				\
		v->data[l] = '\0';			\
		v->attr->length = l + 1;			\
	} else if (written >= MAX_REG_SIZE - l) {	\
		v->attr->length = MAX_REG_SIZE;		\
	} else {					\
		v->attr->length = l + written + 1;	\
	}						\
	return 0; \
}

OPci(u, "%" PRIu64)
OPci(s, "%" PRId64)

static int r_add_cb(struct register_s *v, struct register_s *d)
{
	int l, n, j;

	l = v->attr->length;
	l = strnlen(v->data, l);
	if (v->attr != &(v->tmp_attr)) {
		v->tmp_attr = *(v->attr);
		v->attr = &(v->tmp_attr);
	}
	n = (d->attr->length);
	if ((l + n + n + (n >> 2) + 1) >= MAX_REG_SIZE)
		return 0;
#ifdef BITMAP_DIPLAY_LEFT_RIGHT
	for (j = 0; j < n; j++) {
		if (j > 0 && (j & 0x03) == 0)
			v->data[l++] = ':';
		string_hex_byte(v->data + l, (unsigned char)d->data[j]);
		l += 2;
	}
#else
	for (j = n - 1; j >= 0; j--) {
		string_hex_byte(v->data + l, (unsigned char)d->data[j]);
		l += 2;
		if (j > 0 && (j & 0x03) == 0)
			v->data[l++] = ':';
	}
#endif
	v->data[l++] = 0;
	v->attr->length = l;
	return 0;
}


/* ---------------- table -------------- */

/* op, [uscb] , [uscb] */

static int(*op_func[16][4][4])(struct register_s *v, struct register_s *d) = {
	{
		{r_add_uu, r_add_us, NULL, NULL},
		{r_add_su, r_add_ss, NULL, NULL},
		{r_add_cu, r_add_cs, r_add_cc, r_add_cb},
		{r_add_bu, r_add_bs, NULL, r_add_bb}
	}, {
		{r_sub_uu, r_sub_us, NULL, NULL},
		{r_sub_su, r_sub_ss, NULL, NULL},
		{NULL, NULL, NULL, NULL},
		{r_sub_bu, r_sub_bs, NULL, r_sub_bb}
	}, {
		{r_mul_uu, r_mul_us, NULL, NULL},
		{r_mul_su, r_mul_ss, NULL, NULL},
		{NULL, NULL, NULL, NULL},
		{NULL, NULL, NULL, NULL}
	}, {
		{r_div_uu, r_div_us, NULL, NULL},
		{r_div_su, r_div_ss, NULL, NULL},
		{NULL, NULL, NULL, NULL},
		{NULL, NULL, NULL, NULL}
	}, {
		{r_mod_uu, r_mod_us, NULL, NULL},
		{r_mod_su, r_mod_ss, NULL, NULL},
		{NULL, NULL, NULL, NULL},
		{NULL, NULL, NULL, NULL}
	}, {
		{r_shl_uu, r_shl_us, NULL, NULL},
		{r_shl_su, r_shl_ss, NULL, NULL},
		{NULL, NULL, NULL, NULL},
		{NULL, NULL, NULL, NULL}
	}, {
		{r_shr_uu, r_shr_us, NULL, NULL},
		{r_shr_su, r_shr_ss, NULL, NULL},
		{NULL, NULL, NULL, NULL},
		{NULL, NULL, NULL, NULL}
	}, {
		{r_lt_uu, r_lt_us, NULL, NULL},
		{r_lt_su, r_lt_ss, NULL, NULL},
		{NULL, NULL, r_lt_cc, NULL},
		{NULL, NULL, NULL, NULL}
	}, {
		{r_gt_uu, r_gt_us, NULL, NULL},
		{r_gt_su, r_gt_ss, NULL, NULL},
		{NULL, NULL, r_gt_cc, NULL},
		{NULL, NULL, NULL, NULL}
	}, {
		{r_le_uu, r_le_us, NULL, NULL},
		{r_le_su, r_le_ss, NULL, NULL},
		{NULL, NULL, r_le_cc, NULL},
		{NULL, NULL, NULL, NULL}
	}, {
		{r_ge_uu, r_ge_us, NULL, NULL},
		{r_ge_su, r_ge_ss, NULL, NULL},
		{NULL, NULL, r_ge_cc, NULL},
		{NULL, NULL, NULL, NULL}
	}, {
		{r_eq_uu, r_eq_us, NULL, NULL},
		{r_eq_su, r_eq_ss, NULL, NULL},
		{NULL, NULL, r_eq_cc, NULL},
		{NULL, NULL, NULL, r_eq_bb}
	}, {
		{r_ne_uu, r_ne_us, NULL, NULL},
		{r_ne_su, r_ne_ss, NULL, NULL},
		{NULL, NULL, r_ne_cc, NULL},
		{NULL, NULL, NULL, r_ne_bb}
	}, {
		{r_or_uu, r_or_us, NULL, NULL},
		{r_or_su, r_or_ss, NULL, NULL},
		{NULL, NULL, NULL, NULL},
		{NULL, NULL, NULL, r_or_bb}
	}, {
		{r_and_uu, r_and_us, NULL, NULL},
		{r_and_su, r_and_ss, NULL, NULL},
		{NULL, NULL, NULL, NULL},
		{NULL, NULL, NULL, r_and_bb}
	}, {
		{r_xor_uu, r_xor_us, NULL, NULL},
		{r_xor_su, r_xor_ss, NULL, NULL},
		{NULL, NULL, NULL, NULL},
		{NULL, NULL, NULL, r_xor_bb}
	},
};

int do_bin_op(int op, struct register_s *v, struct register_s *d)
{
	int t1, t2;
	static const char * const opname[] = {
		"+", "-", "*", "/", "%", "<<", ">>",
		"<", ">", "<=", ">=", "==", "!=", "|", "&", "^"
	};
	static const char * const typename[] = {
		"unsigned", "signed", "string", "bitmap"
	};

	if (v->data != v->buf)
		r_imm(v);
	if (d->data != d->buf)
		r_imm(d);
	t1 = (v->attr->type & 0x0f) - MED_TYPE_UNSIGNED;
	t2 = (d->attr->type & 0x0f) - MED_TYPE_UNSIGNED;
	if (t1 < 0 || t1 > 3 || t2 < 0 || t2 > 3) {
		runtime("Illegal operand type");
		return -1;
	}
	if (op_func[op - oADD][t1][t2] == NULL) {
		runtime("Illegal operation %s between %s and %s",
			opname[op-oADD], typename[t1], typename[t2]);
		return -1;
	}
	return op_func[op - oADD][t1][t2](v, d);
}
