/*
 * Constable: threading.h
 * (c)2018 by Roderik Ploszek
 */

#ifndef _THREADING_H
#define _THREADING_H

#define N_WORKER_THREADS 2

#define TLS_CREATE(key, destructor)                     \
    if (pthread_key_create(key, destructor))            \
        return -1

// First byte is zeroed for errstr usage, otherwise it is optional
#define TLS_ALLOC(key, type)                            \
    if (!(data = malloc(sizeof(type))))                 \
        return -1;                                      \
    *(char*)data = 0;                                   \
    if (pthread_setspecific(key, data))                 \
        return -1

int tls_create(void);
int tls_alloc(void);

#endif /* _THREADING_H */
