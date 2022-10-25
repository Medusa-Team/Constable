/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Constable: vs.h
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#ifndef _VS_H
#define _VS_H

#include <asm/types.h>

typedef __u32	vs_t;

#define MAX_NUM_OF_VS 64
#define BITS_PER_VS_WORD	(sizeof(vs_t) << 3)
#define VS_WORDS		(MAX_NUM_OF_VS/BITS_PER_VS_WORD)

int vs_init(void);
int vs_alloc(vs_t *id);
int vs_is_enough(int bites);

void vs_clear(vs_t *to);
void vs_fill(vs_t *to);
void vs_invert(vs_t *to);
void vs_set(const vs_t *from, vs_t *to);
void vs_add(const vs_t *from, vs_t *to);
void vs_sub(const vs_t *from, vs_t *to);
void vs_mask(const vs_t *from, vs_t *to);

int vs_test(const vs_t *test, const vs_t *vs);
int vs_issub(const vs_t *test, const vs_t *vs);
int vs_isclear(const vs_t *vs);
int vs_isfull(const vs_t *vs);

#endif
