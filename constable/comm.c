// SPDX-License-Identifier: GPL-2.0
/*
 * Constable: comm.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <fcntl.h>

#include "constable.h"
#include "comm.h"
#include "language/execute.h"
#include "space.h"
#include "threading.h"
#include "mcp/mcp.h"

int comm_buf_to_queue_locked(struct comm_buffer_queue_s *q,
			     struct comm_buffer_s *b);
struct comm_buffer_s *comm_buf_from_queue_locked(struct comm_buffer_queue_s *q);
static pthread_t comm_workers[N_WORKER_THREADS];

extern struct event_handler_s *function_init;
int comm_nr_connections;
static struct comm_s *first_comm;
static struct comm_s *last_comm;

static int comm_var_data_size; /**< TODO: is manipulation with this global variable
				 * thread and/or per buffer safe?
				 */

void *comm_new_array(int size)
{
	void *v;

	v = calloc(comm_nr_connections, size);
	if (!v)
		return NULL;
	return v;
}

int comm_alloc_buf_var_data(int size)
{
	int r;

	r = comm_var_data_size;
	comm_var_data_size += size;

	return r;
}

static struct comm_buffer_s *comm_alloc_var_data(struct comm_buffer_s *b)
{
	int len = 0;

	if (b->p_comm_buf == b->comm_buf)
		len = b->len;
	b = comm_buf_resize(b, len + comm_var_data_size);
	if (!b)
		return NULL;
	b->var_data = b->comm_buf + len;

	return b;
}

struct comm_s *comm_new(char *name, int user_size)
{
	struct comm_s *c;

	c = calloc(1, sizeof(struct comm_s) + user_size);
	if (!c)
		return NULL;

	strncpy(c->name, name, sizeof(c->name) - 1);
	c->fd = -1;
	c->conn = comm_nr_connections++;
	c->state_lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
	c->init_buffer = NULL;
	sem_init(&c->output_sem, 0, 0);
	c->wait_for_answer.lock =
		(pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
	c->output.lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

	if (!last_comm)
		first_comm = c;
	else
		last_comm->next = c;
	last_comm = c;

	return c;
}

struct comm_s *comm_find(char *name)
{
	struct comm_s *c;

	for (c = first_comm; c; c = c->next)
		if (!strcmp(c->name, name))
			return c;

	return NULL;
}

int comm_do(void)
{
	struct comm_s *c;
	pthread_attr_t attr;

	if (pthread_attr_init(&attr)) {
		puts("Cannot initialize thread attribute");
		return -1;
	}
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	// CREATE READ AND WRITE THREADS FOR EACH COMMUNICATION INTERFACE
	for (c = first_comm; c; c = c->next) {
		if (c->fd < 0)
			continue;

		if (mcp_receive_greeting(c)) {
			comm_error("Error when receiving greeting");
			c->close(c);
			continue;
		}
		if (pthread_create(&c->read_thread, NULL,
				   (void *(*)(void *)) c->read, c)) {
			puts("Cannot create read thread");
			return -1;
		}
		if (pthread_create(&c->write_thread, NULL, write_loop, c)) {
			puts("Cannot create write thread");
			return -1;
		}
	}

	// CREATE WORKER THREADS
	for (int i = 0; i < N_WORKER_THREADS; i++) {
		if (pthread_create(comm_workers + i, &attr, comm_worker, NULL)) {
			puts("Cannot create worker thread");
			return -1;
		}
	}

	// CALL JOIN
	for (c = first_comm; c; c = c->next) {
		if (c->fd >= 0) {
			for (int i = 0; i < N_WORKER_THREADS; i++) {
				if (pthread_join(c->read_thread, NULL)) {
					puts("Error when joining read thread");
					return -1;
				}
			}
			if (pthread_join(c->write_thread, NULL)) {
				puts("Error when joining write thread");
				return -1;
			}
		}
	}

	return 0;
}

void *comm_worker(void *arg)
{
	int r = 0;

	if (tls_alloc_init())
		return (void *)-1;

	if (execute_registers_init())
		return (void *)-1;

	while (1) {
		struct comm_buffer_s *b;

		b = comm_buf_get_todo();
		//printf("comm_worker: b->do_phase = %d, b->var_data = %p\n",
		//		b->do_phase, b->var_data);
		pthread_mutex_lock(&b->lock);
		if (!b->var_data) {
			b = comm_alloc_var_data(b);
			if (!b) {
				pthread_mutex_unlock(&b->lock);
				return (void *)-1;
			}
		}

		if (b->do_phase < 1000) {
			/*
			 * If do_event() finishes handler execution (instruction RET in
			 * virtual machine), it returns 0 and `b->do_phase` is set to 0 too.
			 * Decision result is stored in `b->context.result`.
			 *
			 * Result (answer to a decision request) is written to an output in
			 * the next iteration of `b` processing (via `comm_buf_todo`). In
			 * the next iteration `b->do_phase` should be >= 1000, so
			 * `do_event(b)` won't run. See comments of the code below, where
			 * result `r` from do_event() is examined.
			 */
			r = do_event(b);
		} else {
			/* mY : ak bola udalost vybavena, odosiela sa ANSWER
			 * mY : to vsak neplati pre umelo vyvolany event funkcie _init
			 */
			if (function_init && b->init_handler == function_init) {
				struct comm_s *comm;
				//printf("comm_worker: answer for function init, buf id = %u, b->init_handler = %p\n",
				//		b->id, b->init_handler);
				r = 0;
				pthread_mutex_lock(&b->comm->state_lock);
				pthread_mutex_unlock(&b->lock);
				comm = b->comm;
				b->free(b);
				/* _init() is processed and waiting requests from `b->to_wake`
				 * are already in `comm_todo`. After setting `init_buffer` to
				 * NULL, new requests are inserted into `comm_todo` (see
				 * mcp.c).
				 */
				comm->init_buffer = NULL;
				pthread_mutex_unlock(&comm->state_lock);
				continue;
			} else {
				//printf("comm_worker: answer buf id = %u, r = %d\n", b->id, r);
				r = b->comm->answer(b->comm, b);
			}
		}
		//printf("ZZZ: do_event()=%d\n",r);

		/*
		 * If a handler returns value 1, processing of the buffer should
		 * continue; the buffer is moved to the end of the `todo` queue.
		 * See comments in event.h.
		 *
		 * See documentation of answer() function in `struct comm_s`. answer()
		 * returns values -1, 0, 2 and 3. Return value `r == 1` doesn't
		 * originate from b->comm->answer().
		 */
		if (r == 1) {
			pthread_mutex_unlock(&b->lock);
			comm_buf_todo(b);
		} else if (r <= 0) {
			if (b->do_phase < 1000) {
				/*
				 * `r` is 0 and `b->do_phase` is 0 too if do_event() finished
				 * execution of a `b->init_handler` or a handler (i.e., the code
				 * of virtual machine executed RET instruction of the handler).
				 * Answer of buffer `b` should be handled in the next iteration of
				 * `comm_buf_todo` queue.
				 *
				 * As do_event() is executed if `b->do_phase` < 1000, the value of
				 * `do_phase` is changed to prevent its execution.
				 *
				 * `r` is -1 in case of an error during handler execution. If an
				 * error occurs, information about it should be reported to the
				 * kernel in the corresponding answer.
				 *
				 * TODO: I think `b` should precede all buffers in `comm_buf_todo`,
				 * so it has to be added at the head of the queue. But first, we
				 * have to analyse the code of `do_phase` and answer() function, if
				 * this case of setting `b->do_phase`=1000 is only for this case.
				 */
				b->do_phase = 1000;
				pthread_mutex_unlock(&b->lock);
				comm_buf_todo(b);
			} else {
				/*
				 * `r` is 0 or -1 and `b->do_phase` is 1000 after `b->comm->answer()`
				 * finished. -1 means an error (can't alloc answer buffer).
				 */
				pthread_mutex_unlock(&b->lock);
				b->free(b);
			}
		} else {
			/*
			 * If a handler in `do_event(b)` or `b->comm->answer()` returns 2 or
			 * 3, operation of the event or `answer` wasn't finished yet.
			 * Constable has to wait for an answer to the `update`/`fetch`
			 * request from the kernel. Buffer `b` is returned back to the queue
			 * using comm_buf_todo() in the comm_buf_free() function when buffer
			 * used for the `update` operation is freed.
			 */
			pthread_mutex_unlock(&b->lock);
		}
	}

	return 0;
}

void *write_loop(void *arg)
{
	struct comm_s *c = (struct comm_s *)arg;

	while (1) {
		/* c->write() returns 1 on success */
		if (c->write(c) <= 0) {
			c->close(c);
			break;
		}
	}

	return (void *)-1;
}

int comm_conn_init(struct comm_s *comm)
{
	//printf("ZZZ: comm_conn_init %s\n",comm->name);
	/* default kobjects fo internal constable use */
	language_init_comm_datatypes(comm);
	/* initialize event_masks and classes */
	if (space_init_event_mask(comm) < 0)
		return -1;
	if (class_comm_init(comm) < 0)
		return -1;

	/*
	 * If the configuration file defines _init(), it is inserted into the
	 * queue first and decision requests that came from the kernel are
	 * inserted into `init_buffer->to_wake` queue to be processed after
	 * _init() finishes.
	 */
	pthread_mutex_lock(&comm->state_lock);
	if (function_init) {
		struct comm_buffer_s *p;

		p = comm_buf_get(0, comm);
		if (unlikely(!p)) {
			comm_error("Can't get comm buffer for _init");
			pthread_mutex_unlock(&comm->state_lock);
			return -1;
		}
		get_empty_context(&p->context);
		p->event = NULL;
		p->init_handler = function_init;
		p->ehh_list = EHH_NOTIFY_ALLOW;
		comm->init_buffer = p;
		// _init() is inserted here
		comm_buf_todo(p);
	}
	comm->state = 1;
	pthread_mutex_unlock(&comm->state_lock);

	return 0;
}

int comm_error(const char *fmt, ...)
{
	va_list ap;
	char buf[4096];

	va_start(ap, fmt);
	vsnprintf(buf, 4000, fmt, ap);
	va_end(ap);
	sprintf(buf + strlen(buf), "\n");
	write(1, buf, strlen(buf));

	return -1;
}

int comm_info(const char *fmt, ...)
{
	va_list ap;
	char buf[4096];

	va_start(ap, fmt);
	vsnprintf(buf, 4000, fmt, ap);
	va_end(ap);
	sprintf(buf + strlen(buf), "\n");
	write(1, buf, strlen(buf));

	return -1;
}

/**
 * Opens a file descriptor for given @filename and @flags.
 * On success, returned file descriptor will be greather than 2,
 * i.e. descriptors 0, 1 and 2 are skipped.
 * On failure, return -1.
 *
 * This behavior prevents interleaving of writes (stdout and stderr
 * used by fprintf() with a communication device write, for example /dev/medusa).
 */
int comm_open_skip_stdfds(const char *filename, int flags, mode_t mode)
{
	int err;
	int fd = open(filename, flags, mode);

	if (unlikely(fd < 0)) {
		err = errno;
		goto bad_fd;
	}

	// skip STDIN, STDOUT, STDERR fds
	if (fd < 3) {
		int fd_orig = fd;

		fd = fcntl(fd_orig, F_DUPFD, 3);
		err = errno;
		close(fd_orig);
		if (fd < 0)
			goto bad_fd;
	}

	return fd;

bad_fd:
	comm_error("Can't open '%s': %s", filename, strerror(err));
	return -1;
}
