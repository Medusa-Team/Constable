/*
 * Constable: hash.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: hash.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include "hash.h"

#define hash_func(x)	((((x)>>24)^((x)>>16)^((x)>>8)^x)&0x00ff)

void hash_add( struct hash_s *h, struct hash_ent_s *e, uintptr_t key )
{ int i;
    i=hash_func(key);
    e->next=h->table[i];
    e->key=key;
    h->table[i]=e;
}

struct hash_ent_s *hash_find( struct hash_s *h, uintptr_t key )
{ struct hash_ent_s *e;
    for(e=h->table[hash_func(key)];e!=NULL;e=e->next)
        if( e->key==key )
            return(e);
    return(NULL);
}

