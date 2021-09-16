/*
 * Constable: comm.h
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: comm.h,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#ifndef _COMM_H
#define	_COMM_H

#include "event.h"
#include "threading.h"
#include "language/execute.h"
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>

struct comm_s;
struct comm_buffer_s;
struct execute_s;
struct event_hadler_hash_s;
struct class_handler_s;

struct comm_buffer_queue_s {
    struct queue_item_s	*first;
    struct queue_item_s	*last;
    pthread_mutex_t lock;
};

struct queue_item_s {
    struct queue_item_s *next;
    struct comm_buffer_s *buffer;
};

struct comm_buffer_s {
    struct comm_buffer_s	*next; /**< Used only for the buffers free
                                        * list */
    int			_n;	/**< position in buffers[] */
    int			size;
    void(*free)(struct comm_buffer_s*);
    unsigned int	id;	/**< identification of the buffer for debug */

    struct comm_s		*comm;
    int			open_counter;
    void			*user1;
    void			*user2;
    int	    user_data; // Matus mozno to ma byt intptr_t
    pthread_mutex_t lock; /**< lock for comm_worker */
    pthread_mutex_t write_finished_lock; /**< lock for guarding the
                                           write_finished predicate */
    pthread_cond_t write_finished_condition; /**< condition variable for the
                                                write_finished predicate */
    bool write_finished; /**< Predicate used for fetch and update operations.
                            If false, write operation on this buffer has not
                            been completed. If true, write has already
                            finished. */
    /* for do_event & execute ... */
    struct execute_s    execute;
    int			do_phase;
    int			ehh_list;
    struct event_hadler_hash_s *hh;
    struct class_handler_s	*ch;
    struct event_context_s	context;
    struct event_type_s	*event;
    struct event_handler_s	*handler;

    struct comm_buffer_queue_s	to_wake;
    int			len;		/* for comm */
    int			want;		/* for comm */
    int(*completed)(struct comm_buffer_s*);	/* for comm */
    void			*var_data;
    char			*p_comm_buf;	/*  for read/write */
    char			comm_buf[0]; 	/* for comm */
};

struct comm_s {
    struct comm_s	*next;
    int		conn;		/**< connection number */
    pthread_t   read_thread;  /**< thread used for read operations */
    pthread_t   write_thread;    /**< thread used for write operation */
    char		name[64];
    int		fd;
    int		open_counter;
    pthread_mutex_t state_lock;	/**< Mutex for `state` and `init_buffer`
				 * manipulation. */
    int		state;		/**< Until comm is not initialized, holds zero
				 * value. When initialized (while the first
				 * decision request is received), value is set
				 * to 1. */
    struct comm_buffer_s *init_buffer; /**< Buffer for _init(). If not NULL,
                                        * _init() was not yet processed and all
                                        * requests received by Constable are
                                        * enqueued into `init_buffer->to_wake`
                                        * queue. */
    int		flags;
    struct hash_s	classes;
    struct hash_s	events;

    /* for read/write/... */
    struct comm_buffer_queue_s output;
    sem_t output_sem;
    struct comm_buffer_queue_s wait_for_answer;	/* fetch */
    pthread_mutex_t read_lock; /**< Lock for reading one buffer at a time. */
    int(*read)(struct comm_s*);
    int(*write)(struct comm_s*);
    int(*close)(struct comm_s*);
    /**
     * Answer a request from the kernel.
     *
     * \return
     * -1 when no memory is available for allocation
     *  0 when answer was succesfully sent
     *  2 when update_object() returns 2
     *  3 when update_object() was executed and Constable is waiting for the
     *    answer (success/failure of update operation) from the kernel.
     */
    int(*answer)(struct comm_s*,struct comm_buffer_s*);
    int(*fetch_object)(struct comm_s*,int cont,struct object_s *o,struct comm_buffer_s *wake);
    /**
     * Update object information in the kernel.
     *
     * \return
     * -1 when no memory is available for allocation or `update` failed
     *  0 when object was succesfully updated
     *  2 TODO
     *  3 after `update` request was sent to the kernel (first pass)
     */
    int(*update_object)(struct comm_s*,int cont,struct object_s *o,struct comm_buffer_s *wake);

    int(*conf_error)(struct comm_s *,const char *fmt,...);
    char		user_data[0]; /**< TODO change type to `mcp/mcp.c: struct mcp_comm_s` */
};

#define	comm_user_data(c)	((void *)(&((c)->user_data[0])))

struct comm_buffer_s *comm_buf_get( int size, struct comm_s *comm );
struct comm_buffer_s *comm_buf_resize( struct comm_buffer_s *b, int size );

extern int comm_nr_connections;

extern struct comm_buffer_queue_s comm_todo;
extern sem_t comm_todo_sem;
int comm_buf_to_queue( struct comm_buffer_queue_s *q, struct comm_buffer_s *b );
inline int comm_buf_to_queue_locked(struct comm_buffer_queue_s *q,
                             struct comm_buffer_s *b)
{
    int ret;
    pthread_mutex_lock(&q->lock);
    ret = comm_buf_to_queue(q, b);
    pthread_mutex_unlock(&q->lock);
    return ret;
}
struct comm_buffer_s *comm_buf_from_queue( struct comm_buffer_queue_s *q );
inline struct comm_buffer_s *comm_buf_from_queue_locked(
        struct comm_buffer_queue_s *q)
{
    struct comm_buffer_s *ret;
    pthread_mutex_lock(&q->lock);
    ret = comm_buf_from_queue(q);
    pthread_mutex_unlock(&q->lock);
    return ret;
}
struct comm_buffer_s *comm_buf_del(struct comm_buffer_queue_s *q,
                                   struct queue_item_s* prev,
                                   struct queue_item_s* item);
struct comm_buffer_s *comm_buf_peek_first(struct comm_buffer_queue_s *q);
struct comm_buffer_s *comm_buf_peek_last(struct comm_buffer_queue_s *q);

static inline int comm_buf_output_enqueue(struct comm_s *c, struct comm_buffer_s *b)
{
    comm_buf_to_queue_locked(&c->output, b);
    printf("comm_buf_output_enqueue: add buffer %u to OUTPUT queue\n", b->id);
    sem_post(&c->output_sem);
    return 0;
}

static inline struct comm_buffer_s* comm_buf_output_dequeue(struct comm_s *c)
{
    struct comm_buffer_s *ret;
    sem_wait(&c->output_sem);
    ret = comm_buf_from_queue_locked(&c->output);
    printf("comm_buf_output_dequeue: get buffer %u from OUTPUT queue and write it\n", ret->id);
    return ret;
}

#define	comm_buf_todo(b)	do {                                \
    comm_buf_to_queue_locked(&comm_todo, (b));                  \
    printf("comm_buf_todo: add buffer %u to TODO queue\n", b->id); \
    sem_post(&comm_todo_sem);                                   \
} while (0)

static inline struct comm_buffer_s* comm_buf_get_todo(void) {
    struct comm_buffer_s *ret;
    sem_wait(&comm_todo_sem);
    ret = comm_buf_from_queue_locked(&comm_todo);
    printf("comm_buf_get_todo: get buffer %u from TODO queue\n", ret->id);
    return ret;
}

/*
 * For each construct for looping through locked queues.
 * Used in fetch_answer and update_answer.
 */
#define FOR_EACH_LOCKED(item, queue)                 \
    pthread_mutex_lock(&(queue)->lock);              \
    item = (queue)->first;                           \
    while (item)

#define NEXT_ITEM(item) item = item->next

#define END_FOR_EACH_LOCKED(queue) pthread_mutex_unlock(&(queue)->lock)

void *comm_new_array( int size );
int comm_alloc_buf_var_data( int size );
#define	P_COMM_BUF_VAR_DATA(cb,ofs)	((cb)->var_data+(ofs))
#define	PUPTR_COMM_BUF_VAR_DATA(cb,ofs)  ((uintptr_t*)P_COMM_BUF_VAR_DATA((cb),(ofs)))
struct comm_s *comm_new( char *name, int user_size );
struct comm_s *comm_find( char *name );

int comm_conn_init( struct comm_s *comm );

int buffers_init( void );
int buffers_alloc( void );

int comm_do( void );
void* comm_worker(void*);
void* write_loop(void*);

int comm_error( const char *fmt, ... );
int comm_info( const char *fmt, ... );

#endif /* _COMM_H */

