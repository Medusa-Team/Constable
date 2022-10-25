/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Constable: hash.h
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#ifndef _HASH_H
#define _HASH_H

#include "types.h"

struct hash_ent_s {
	struct hash_ent_s *next;
	uintptr_t key;
};

struct hash_s {
	struct hash_ent_s *table[256];
};

void hash_add(struct hash_s *h, struct hash_ent_s *e, uintptr_t key);
struct hash_ent_s *hash_find(struct hash_s *h, uintptr_t key);
struct hash_ent_s *hash_del(struct hash_s *h, uintptr_t key);

#endif /* _HASH_H */

