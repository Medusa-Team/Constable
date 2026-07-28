// SPDX-License-Identifier: GPL-2.0
/*
 * Constable: comm_buf.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#include "comm.h"
#include "constable.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
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
	struct stack_s *stack = b->execute.stack;

	b->comm = comm;
	b->open_counter = comm->open_counter;
	b->user1 = NULL;
	b->user2 = NULL;
	b->user_data = 0;
	memset(&b->execute, 0, sizeof(b->execute));
	b->execute.stack = stack;
	b->do_phase = 0;
	b->ehh_list = EHH_VS_ALLOW;
	b->hh = NULL;
	b->ch = NULL;
	memset(&b->context, 0, sizeof(b->context));
	b->context.cb = b;
	b->event = NULL;
	b->init_handler = NULL;
	b->var_data = NULL;

	b->to_wake.first = NULL;
	b->to_wake.last = NULL;
	b->waiting.to = 0;
	b->waiting.cid = 0;
	b->waiting.seq = 0;
}

struct comm_buffer_s *comm_buf_get(int size, struct comm_s *comm)
{
	size_t i;
	struct comm_buffer_s *b;
	unsigned int id;

	if (size < 0 || !comm)
		return NULL;

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
	if (!b)
		return NULL;
	b->id = id;
	comm_buf_init(b, comm);
	return b;
}

static void *rebase_buffer_pointer(const struct comm_buffer_s *source,
				   struct comm_buffer_s *destination,
				   void *pointer)
{
	uintptr_t address;
	uintptr_t base;
	size_t offset;

	if (!pointer)
		return NULL;
	address = (uintptr_t)pointer;
	base = (uintptr_t)source->comm_buf;
	if (address < base)
		return pointer;
	offset = (size_t)(address - base);
	if (offset > (size_t)source->size)
		return pointer;
	return destination->comm_buf + offset;
}

static void transfer_buffer_state(struct comm_buffer_s *destination,
				  struct comm_buffer_s *source)
{
	struct stack_s *replacement_stack = destination->execute.stack;

	destination->comm = source->comm;
	destination->open_counter = source->open_counter;
	if (source->comm && source->comm->init_buffer == source)
		source->comm->init_buffer = destination;
	destination->user1 = source->user1 == source ?
			     destination :
			     rebase_buffer_pointer(source, destination,
						   source->user1);
	destination->user2 = source->user2 == source ?
			     destination :
			     rebase_buffer_pointer(source, destination,
						   source->user2);
	destination->user_data = source->user_data;

	destination->execute = source->execute;
	source->execute.stack = replacement_stack;
	if (destination->execute.my_comm_buff == source)
		destination->execute.my_comm_buff = destination;

	destination->do_phase = source->do_phase;
	destination->ehh_list = source->ehh_list;
	destination->hh = source->hh;
	destination->ch = source->ch;
	destination->context = source->context;
	destination->context.cb = destination;
	if (destination->context.operation.next == &source->context.subject)
		destination->context.operation.next =
			&destination->context.subject;
	if (destination->context.subject.next == &source->context.object)
		destination->context.subject.next = &destination->context.object;
	destination->context.operation.data =
		rebase_buffer_pointer(source, destination,
				      source->context.operation.data);
	destination->context.subject.data =
		rebase_buffer_pointer(source, destination,
				      source->context.subject.data);
	destination->context.object.data =
		rebase_buffer_pointer(source, destination,
				      source->context.object.data);
	if (destination->execute.c == &source->context)
		destination->execute.c = &destination->context;

	destination->event = source->event;
	destination->init_handler = source->init_handler;
	destination->to_wake.first = source->to_wake.first;
	destination->to_wake.last = source->to_wake.last;
	source->to_wake.first = NULL;
	source->to_wake.last = NULL;
	destination->waiting = source->waiting;
	destination->len = source->len;
	destination->want = source->want;
	destination->completed = source->completed;
	destination->var_data =
		rebase_buffer_pointer(source, destination, source->var_data);
	destination->p_comm_buf =
		rebase_buffer_pointer(source, destination, source->p_comm_buf);
}

struct comm_buffer_s *comm_buf_resize(struct comm_buffer_s *b, int size)
{
	if (!b || size < 0 || b->size < 0 || b->len < 0 ||
	    b->len > b->size || size < b->len)
		return NULL;

	if (size > b->size) {
		struct comm_buffer_s *n;

		if (b->var_data != NULL)
			return NULL;

		n = comm_buf_get(size, b->comm);
		if (!n)
			return NULL;

		transfer_buffer_state(n, b);
		if (n->len)
			memcpy(n->comm_buf, b->comm_buf, (size_t)n->len);
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
		fatal("%s: protocol-v4 policy readiness was not completed",
		      __func__);

	//printf("comm_buf_free: free buffer %u\n", b->id);
	if (b->_n >= 0) {
		pthread_mutex_lock(&buffers_lock);
		b->next = buffers[b->_n];
		buffers[b->_n] = b;
		pthread_mutex_unlock(&buffers_lock);
	} else {
		execute_put_stack(b->execute.stack);
		pthread_mutex_destroy(&b->to_wake.lock);
		pthread_mutex_destroy(&b->lock);
		free(b);
	}
}

static struct comm_buffer_s *malloc_buf(int size)
{
	struct comm_buffer_s *b;
	size_t allocation;

	if (size < 0 ||
	    (size_t)size > SIZE_MAX - sizeof(struct comm_buffer_s))
		return NULL;
	allocation = sizeof(struct comm_buffer_s) + (size_t)size;
	b = calloc(1, allocation);
	if (!b)
		return NULL;

	b->bfree = comm_buf_free;
	b->_n = -1;
	b->size = size;

	if (pthread_mutex_init(&b->lock, NULL)) {
		free(b);
		return NULL;
	}
	if (pthread_mutex_init(&b->to_wake.lock, NULL)) {
		pthread_mutex_destroy(&b->lock);
		free(b);
		return NULL;
	}
	b->execute.stack = execute_get_stack();
	if (!b->execute.stack) {
		pthread_mutex_destroy(&b->to_wake.lock);
		pthread_mutex_destroy(&b->lock);
		free(b);
		return NULL;
	}
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
	struct queue_item_s *item;

	if (!q || !b)
		return -EINVAL;
	item = new_item(b);
	if (!item)
		return -ENOMEM;
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
