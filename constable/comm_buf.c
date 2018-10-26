/*
 * Constable: comm_buf.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: comm_buf.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include "comm.h"
#include "constable.h"
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

static pthread_mutex_t buffers_lock = PTHREAD_MUTEX_INITIALIZER;
static struct comm_buffer_s *buffers[2];

#define ITEM_FREE_LIST_CAPACITY 10
static size_t ready_items = 0;
static pthread_mutex_t item_free_list_lock = PTHREAD_MUTEX_INITIALIZER;
static struct queue_item_s *item_free_list = NULL;

#define	BN	(sizeof(buffers)/sizeof(buffers[0]))

static struct comm_buffer_s *malloc_buf( int size );

static inline void comm_buf_init(struct comm_buffer_s *b, struct comm_s *comm)
{
    b->comm=comm;
    b->open_counter=comm->open_counter;
    b->to_wake.lock = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    b->execute.pos=0;
    b->execute.base=0;
    b->execute.h=NULL;
    b->execute.c=NULL;
    b->execute.keep_stack=0;
    b->do_phase=0;
    b->ehh_list=EHH_VS_ALLOW;
    b->context.cb=b;
    b->temp=NULL;
}

struct comm_buffer_s *comm_buf_get( int size, struct comm_s *comm )
{ int i;
    struct comm_buffer_s *b;
    pthread_mutex_lock(&buffers_lock);
    for(i=0;i<BN;i++)
    {	if( (b=buffers[i]) && size<=b->size )
        {	buffers[i]=b->next;
            pthread_mutex_unlock(&buffers_lock);
            b->len=0;
            b->want=0;
            b->pbuf=b->buf;
            b->completed=NULL;
            b->to_wake.first=b->to_wake.last=NULL;
            b->handler=NULL;
            comm_buf_init(b, comm);
            return(b);
        }
    }
    pthread_mutex_unlock(&buffers_lock);
    b=malloc_buf(size);
    comm_buf_init(b, comm);
    return(b);
}

struct comm_buffer_s *comm_buf_resize( struct comm_buffer_s *b, int size )
{
    if( size > b->size )
    { struct comm_buffer_s *n;
        int z__n,z_size;
        struct comm_buffer_s *z_next,*z_context_cb;
        void(*z_free)(struct comm_buffer_s*);
        if( b->temp!=NULL )
        {	fatal("Can't resize comm buffer with temp!");
            return(NULL);
        }
        if( (n=comm_buf_get(size,b->comm))==NULL )
        {	fatal(Out_of_memory);
            return(NULL);
        }
        z__n=n->_n;
        z_size=n->size;
        z_next=n->next;
        z_context_cb=n->context.cb;
        z_free=n->free;
        *n=*b;
        n->_n=z__n;
        n->size=z_size;
        n->next=z_next;
        n->context.cb=z_context_cb;
        n->free=z_free;
        memcpy(n->buf,b->buf,n->len);
        b->to_wake.first=b->to_wake.last=NULL;
        b->free(b);
        return(n);
    }
    return(b);
}


static void comm_buf_free( struct comm_buffer_s *b )
{ struct comm_buffer_s *q;
    while( (q=comm_buf_from_queue(&(b->to_wake)))!=NULL )
        comm_buf_todo(q);
    if( b->_n >= 0 )
    {
        pthread_mutex_lock(&buffers_lock);
        b->next=buffers[b->_n];
        buffers[b->_n]=b;
        pthread_mutex_unlock(&buffers_lock);
    }
    else {
        execute_put_stack(b->execute.stack);
        free(b);
    }
}

static struct comm_buffer_s *malloc_buf( int size )
{ struct comm_buffer_s *b;
    if( (b=calloc(1,sizeof(struct comm_buffer_s)+size))==NULL )
    {	fatal(Out_of_memory);
        return(NULL);
    }
    b->free=comm_buf_free;
    b->_n= -1;
    b->size=size;

    b->execute.stack=execute_get_stack();
    b->len=0;
    b->want=0;
    b->pbuf=b->buf;
    b->completed=NULL;
    b->to_wake.first=b->to_wake.last=NULL;
    return(b);
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
        if ((item = (struct queue_item_s*)
                malloc(sizeof(struct queue_item_s))) == NULL) {
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

int comm_buf_to_queue( struct comm_buffer_queue_s *q, struct comm_buffer_s *b )
{
    struct queue_item_s *item = new_item(b);
    if( q->last ) {
        q->last->next=item;
        q->last=item;
    }
    else
        q->last=q->first=item;
    return(0);
}

struct comm_buffer_s *comm_buf_from_queue( struct comm_buffer_queue_s *q )
{
    struct queue_item_s *item;
    struct comm_buffer_s *b = NULL;
    if( (item=q->first) && (q->first=item->next)==NULL )
        q->last=NULL;
    if (item) {
        b = item->buffer;
        free_item(item);
    }
    return(b);
}

/*
 * Deletes item from queue.
 * @prev: item before the item to be deleted (since it's not doubly linked)
 */
struct comm_buffer_s *comm_buf_del(struct comm_buffer_queue_s *q,
                  struct queue_item_s* prev,
                  struct queue_item_s* item)
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

int buffers_init( void )
{
    // No need to lock comm_todo or buffers, since this is just one thread
    comm_todo.first=comm_todo.last=NULL;
    buffers[0]=buffers[1]=NULL;
    return sem_init(&comm_todo_sem, 0, 0);
}

int buffers_alloc( void )
{ struct comm_buffer_s *b;
    int i,n,num;
    num=comm_nr_connections;
    n=0;
    while(num>0)
    {
        for(i=0;i<1;i++)
            if( (b=malloc_buf(4096))!=NULL )
            {	b->_n=0;
                b->free(b);
                n++;
            }
        for(i=0;i<1;i++)
            if( (b=malloc_buf(8192))!=NULL )
            {	b->_n=1;
                b->free(b);
                n++;
            }
        num--;
    }
    if( n==0 )
        return(-1);
    return(n);
}


