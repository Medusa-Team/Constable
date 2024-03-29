// SPDX-License-Identifier: GPL-2.0
/*
 * Constable: comm_buf.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#include "comm.h"
#include "constable.h"
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

extern int comm_nr_connections;

static pthread_mutex_t buffers_lock = PTHREAD_MUTEX_INITIALIZER;
static struct comm_buffer_s *buffers[2];
static unsigned int buf_id;	/**< global counter for `buf_id`, access
				  * under `buffers_lock`
				  */

#define ITEM_FREE_LIST_CAPACITY 10
static size_t ready_items;
static pthread_mutex_t item_free_list_lock = PTHREAD_MUTEX_INITIALIZER;
static struct queue_item_s *item_free_list;

static struct comm_buffer_s *malloc_buf(int size);

static inline void comm_buf_init(struct comm_buffer_s *b, struct comm_s *comm)
{
	b->comm = comm;
	b->open_counter = comm->open_counter;
	b->to_wake.lock = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
	b->lock = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
	b->execute.pos = 0;
	b->execute.base = 0;
	b->execute.h = NULL;
	b->execute.c = NULL;
	b->execute.keep_stack = 0;
	b->do_phase = 0;
	b->ehh_list = EHH_VS_ALLOW;
	b->context.cb = b;
	b->var_data = NULL;

	b->waiting.to = 0;
	b->waiting.cid = 0;
	b->waiting.seq = 0;
}

struct comm_buffer_s *comm_buf_get(int size, struct comm_s *comm)
{
	int i;
	struct comm_buffer_s *b;
	unsigned int id;

	pthread_mutex_lock(&buffers_lock);
	id = buf_id++;
	for (i = 0; i < ARRAY_SIZE(buffers); i++) {
		b = buffers[i];
		if (b && size <= b->size) {
			buffers[i] = b->next;
			pthread_mutex_unlock(&buffers_lock);
			b->len = 0;
			b->want = 0;
			b->p_comm_buf = b->comm_buf;
			b->completed = NULL;
			b->to_wake.first = b->to_wake.last = NULL;
			b->init_handler = NULL;
			b->id = id;
			comm_buf_init(b, comm);
			return b;
		}
	}

	pthread_mutex_unlock(&buffers_lock);
	b = malloc_buf(size);
	b->id = id;
	comm_buf_init(b, comm);
	return b;
}

struct comm_buffer_s *comm_buf_resize(struct comm_buffer_s *b, int size)
{
	if (size > b->size) {
		struct comm_buffer_s *n;
		int z__n, z_size;
		unsigned int z_id;
		struct comm_buffer_s *z_next, *z_context_cb;
		void (*z_free)(struct comm_buffer_s *cb);

		if (b->var_data != NULL) {
			fatal("Can't resize comm buffer with var_data!");
			return NULL;
		}

		n = comm_buf_get(size, b->comm);
		if (n == NULL) {
			fatal(Out_of_memory);
			return NULL;
		}

		z_id = n->id;
		z__n = n->_n;
		z_size = n->size;
		z_next = n->next;
		z_context_cb = n->context.cb;
		z_free = n->bfree;
		*n = *b;
		n->id = z_id;
		n->_n = z__n;
		n->size = z_size;
		n->next = z_next;
		n->context.cb = z_context_cb;
		n->bfree = z_free;
		memcpy(n->comm_buf, b->comm_buf, n->len);
		b->to_wake.first = b->to_wake.last = NULL;
		b->bfree(b);
		return n;
	}

	return b;
}

int mcp_ready_answer(struct comm_s *c);

static void comm_buf_free(struct comm_buffer_s *b)
{
	struct comm_buffer_s *q;

	pthread_mutex_lock(&(b->to_wake.lock));
	while ((q = comm_buf_from_queue(&(b->to_wake))) != NULL)
		comm_buf_todo(q);
	pthread_mutex_unlock(&(b->to_wake.lock));

	/* send READY cmd to the kernel after _init() finishes */
	if (function_init && b->comm && b->comm->version > 2 && b->comm->init_buffer == b
	    && mcp_ready_answer(b->comm) < 0)
		fatal("%s: MEDUSA_COMM_READY_ANSWER not send to the kernel", __func__);

	//printf("comm_buf_free: free buffer %u\n", b->id);
	if (b->_n >= 0) {
		pthread_mutex_lock(&buffers_lock);
		b->next = buffers[b->_n];
		buffers[b->_n] = b;
		pthread_mutex_unlock(&buffers_lock);
	} else {
		execute_put_stack(b->execute.stack);
		free(b);
	}
}

static struct comm_buffer_s *malloc_buf(int size)
{
	struct comm_buffer_s *b;

	b = calloc(1, sizeof(struct comm_buffer_s) + size);
	if (b == NULL) {
		fatal(Out_of_memory);
		return NULL;
	}

	b->bfree = comm_buf_free;
	b->_n = -1;
	b->size = size;

	b->execute.stack = execute_get_stack();
	b->len = 0;
	b->want = 0;
	b->p_comm_buf = b->comm_buf;
	b->completed = NULL;
	b->to_wake.first = b->to_wake.last = NULL;

	return b;
}

/**
 * Returns new queue item. Allocates a new queue item only if the
 * items_free_list is empty, otherwise returns queue item from the free list.
 */
static struct queue_item_s *new_item(struct comm_buffer_s *b)
{
	struct queue_item_s *item;

	pthread_mutex_lock(&item_free_list_lock);
	if (ready_items) {
		item = item_free_list;
		item_free_list = item->next;
		ready_items--;
		pthread_mutex_unlock(&item_free_list_lock);
	} else {
		pthread_mutex_unlock(&item_free_list_lock);
		item = (struct queue_item_s *) malloc(sizeof(struct queue_item_s));
		if (item == NULL) {
			fatal(Out_of_memory);
			return NULL;
		}
	}

	item->next = NULL;
	item->buffer = b;

	return item;
}

/**
 * If capacity of items_free_list allows it, the queue item is placed on the
 * free list for later use. Otherwise it gets deallocated.
 */
static void free_item(struct queue_item_s *item)
{
	pthread_mutex_lock(&item_free_list_lock);
	if (ready_items < ITEM_FREE_LIST_CAPACITY) {
		item->next = item_free_list;
		item_free_list = item;
		ready_items++;
		pthread_mutex_unlock(&item_free_list_lock);
	} else {
		pthread_mutex_unlock(&item_free_list_lock);
		free(item);
	}
}

struct comm_buffer_queue_s comm_todo = {NULL, NULL, PTHREAD_MUTEX_INITIALIZER};
sem_t comm_todo_sem;

int comm_buf_to_queue(struct comm_buffer_queue_s *q, struct comm_buffer_s *b)
{
	struct queue_item_s *item = new_item(b);

	if (q->last) {
		q->last->next = item;
		q->last = item;
	} else
		q->last = q->first = item;

	return 0;
}

struct comm_buffer_s *comm_buf_from_queue(struct comm_buffer_queue_s *q)
{
	struct queue_item_s *item = q->first;
	struct comm_buffer_s *b = NULL;

	if (item == NULL)
		return NULL;

	q->first = item->next;
	if (q->first == NULL)
		q->last = NULL;
	b = item->buffer;
	free_item(item);

	return b;
}

/*
 * Deletes item from queue.
 * @prev: item before the item to be deleted (since it's not doubly linked)
 */
struct comm_buffer_s *comm_buf_del(struct comm_buffer_queue_s *q,
				   struct queue_item_s *prev,
				   struct queue_item_s *item)
{
	struct comm_buffer_s *b = NULL;

	if (!prev && !item->next) {
		q->first = NULL;
		q->last = NULL;
	} else if (!prev)
		q->first = item->next;
	else if (!item->next) {
		q->last = prev;
		prev->next = NULL;
	} else
		prev->next = item->next;

	b = item->buffer;
	free_item(item);

	return b;
}

inline struct comm_buffer_s *comm_buf_peek_first(struct comm_buffer_queue_s *q)
{
	if (q->first)
		return q->first->buffer;
	return NULL;
}

inline struct comm_buffer_s *comm_buf_peek_last(struct comm_buffer_queue_s *q)
{
	if (q->last)
		return q->last->buffer;
	return NULL;
}

int buffers_init(void)
{
	// No need to lock comm_todo or buffers, since this is just one thread
	comm_todo.first = comm_todo.last = NULL;
	buffers[0] = buffers[1] = NULL;

	return sem_init(&comm_todo_sem, 0, 0);
}

int buffers_alloc(void)
{
	struct comm_buffer_s *b;
	int i, n, num;

	num = comm_nr_connections;
	n = 0;
	while (num > 0) {
		for (i = 0; i < 1; i++) {
			b = malloc_buf(4096);
			if (b != NULL) {
				b->_n = 0;
				b->bfree(b);
				n++;
			}
		}
		for (i = 0; i < 1; i++) {
			b = malloc_buf(8192);
			if (b != NULL) {
				b->_n = 1;
				b->bfree(b);
				n++;
			}
		}
		num--;
	}

	if (n == 0)
		return -1;
	return n;
}
