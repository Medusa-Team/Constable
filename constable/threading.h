/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Constable: threading.h
 * (c)2018 by Roderik Ploszek
 */

#ifndef _THREADING_H
#define _THREADING_H

#include <stdlib.h>
#include <pthread.h>

#define N_WORKER_THREADS 2

static inline int tls_create(pthread_key_t *key)
{
	if (pthread_key_create(key, NULL))
		return -1;
	return 0;
}

static inline void *tls_alloc(pthread_key_t key, size_t size)
{
	void *data;

	data = malloc(size);
	if (!data)
		return NULL;

	// First byte is zeroed for errstr usage, otherwise it is optional
	*(char *)data = 0;

	if (pthread_setspecific(key, data)) {
		free(data);
		return NULL;
	}

	return data;
}

int tls_alloc_init(void);

#endif /* _THREADING_H */
