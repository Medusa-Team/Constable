/*
 * Constable: comm.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: comm.c,v 1.3 2004/04/24 15:33:35 marek Exp $
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "comm.h"
extern int comm_buf_to_queue_locked(struct comm_buffer_queue_s *q,
                                    struct comm_buffer_s *b);
extern struct comm_buffer_s *comm_buf_from_queue_locked(
        struct comm_buffer_queue_s *q);
#include "language/execute.h"
#include "space.h"
#include "threading.h"
#include "mcp/mcp.h"

static pthread_t comm_workers[N_WORKER_THREADS];

extern struct event_handler_s *function_init; // mY

int comm_nr_connections=0;
static struct comm_s *first_comm=NULL;
static struct comm_s *last_comm=NULL;

static int comm_temp_size=0;

void *comm_new_array( int size )
{ void *v;
    if( (v=malloc(comm_nr_connections*size))==NULL )
        return(NULL);
    memset(v,0,comm_nr_connections*size);
    return(v);
}

int comm_alloc_buf_temp( int size )
{ int r;
    r=comm_temp_size;
    comm_temp_size+=size;
    return(r);
}

static struct comm_buffer_s *comm_malloc_temps( struct comm_buffer_s * b )
{
    if( b->pbuf == b->buf )
    {   if( (b=comm_buf_resize(b,b->len+comm_temp_size))==NULL )
            return(NULL);
        b->temp=b->buf+b->len;
    }
    else
    {   if( (b=comm_buf_resize(b,comm_temp_size))==NULL )
            return(NULL);
        b->temp=b->buf;
    }
    return(b);
}

struct comm_s *comm_new( char *name, int user_size )
{ struct comm_s *c;
    if( (c=malloc(sizeof(struct comm_s)+user_size))==NULL )
        return(NULL);
    memset(c,0,sizeof(struct comm_s)+user_size);
    strncpy(c->name,name,sizeof(c->name)-1);
    c->fd= -1;
    if( (c->execute=execute_alloc_execute())==NULL )
    {   free(c);
        return(NULL);
    }
    c->conn=comm_nr_connections++;
    c->state_lock = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    sem_init(&c->output_sem, 0, 0);
    c->wait_for_answer.lock =
        (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    c->output.lock = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    if( last_comm==NULL )
        first_comm=c;
    else    last_comm->next=c;
    last_comm=c;
    return(c);
}

struct comm_s *comm_find( char *name )
{ struct comm_s *c;
    for(c=first_comm;c!=NULL;c=c->next)
        if( !strcmp(c->name,name) )
            return(c);
    return(NULL);
}

int comm_do( void )
{ struct comm_s *c;
    pthread_attr_t attr;

    if (pthread_attr_init(&attr)) {
        puts("Cannot initialize thread attribute");
        return -1;
    }
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    // CREATE READ AND WRITE THREADS FOR EACH COMMUNICATION INTERFACE
    for (c = first_comm; c != NULL; c = c->next) {
        if (c->fd < 0)
            continue;
        if (mcp_receive_greeting(c)) {
            comm_error("Error when receiving greeting");
            c->close(c);
            continue;
        }
        for (int i = 0; i < N_WORKER_THREADS; i++) {
            if (pthread_create(c->read_workers + i, NULL,
                               (void *(*)(void *)) c->read, c)) {
                puts("Cannot create read thread");
                return -1;
            }
        }
        if (pthread_create(&c->write_thread, NULL, write_loop, c)) {
            puts("Cannot create read thread");
            return -1;
        }
    }

    // CREATE WORKER THREADS
    for (int i = 0; i < N_WORKER_THREADS; i++) {
        if (pthread_create(comm_workers + i, &attr, comm_worker, NULL)) {
            puts("Cannot create read thread");
            return -1;
        }
    }

    // CALL JOIN
    for (c = first_comm; c != NULL; c = c->next) {
        if (c->fd>=0) {
            for (int i = 0; i < N_WORKER_THREADS; i++) {
                if (pthread_join(c->read_workers[i], NULL)) {
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

void* comm_worker(void *arg)
{
    int r = 0;

    if (tls_alloc())
        return (void*) -1;

    if (execute_registers_init())
        return (void*) -1;

    while (1) {
        struct comm_buffer_s *b;
        b = comm_buf_get_todo();
        if(b->temp == NULL) {
            b = comm_malloc_temps(b);
            if( b==NULL )
                return (void*) -1;
        }
        if(b->do_phase < 1000)
            r=do_event(b);
        else { // mY : ak bola udalost vybavena, odosiela sa ANSWER
          // mY : to vsak neplati pre umelo vyvolany event funkcie _init  
            if( b->handler == function_init ) // mY
                r = 0; // mY
            else // mY
                r=b->comm->answer(b->comm,b,r);
        } // mY
        //printf("ZZZ: do_event()=%d\n",r);
        if(r == 1)
            comm_buf_todo(b);
        else if(r <= 0) {   
            if( b->do_phase<1000 ) {   
                b->do_phase=1000;
                comm_buf_todo(b);
            }
            else    
                b->free(b);
        }
    }

    return 0;
}

void* write_loop(void *arg) {
    struct comm_s *c = (struct comm_s*) arg;
    while (1) {
        if (c->write(c) < 0) {
            c->close(c);
            break;
        }
    }
    return (void*) -1;
}

int comm_conn_init( struct comm_s *comm )
{
    //printf("ZZZ: comm_conn_init %s\n",comm->name);
    /* default kobjects fo internal constable use */
    language_init_comm_datatypes(comm);
    /* initialize event_masks and classes */
    if( space_init_event_mask(comm)<0 )
        return(-1);
    if( class_comm_init(comm)<0 )
        return(-1);
    return(0);
}

int comm_error( const char *fmt, ... )
{ va_list ap;
    char buf[4096];

    va_start(ap,fmt);
    vsnprintf(buf, 4000, fmt, ap);
    va_end(ap);
    sprintf(buf+strlen(buf),"\n");
    write(1,buf,strlen(buf));
    return(-1);
}

int comm_info( const char *fmt, ... )
{ va_list ap;
    char buf[4096];

    va_start(ap,fmt);
    vsnprintf(buf, 4000, fmt, ap);
    va_end(ap);
    sprintf(buf+strlen(buf),"\n");
    write(1,buf,strlen(buf));
    return(-1);
}
