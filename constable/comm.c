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
#include "comm.h"
#include "language/execute.h"
#include "space.h"

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
    struct comm_buffer_s *b;
    fd_set rd,wr;
    int max=0;
    int r;

    for(;;)
    {
        while( (b=comm_buf_get_todo())!=NULL )
        {
            if( b->temp==NULL )
            {   b=comm_malloc_temps(b);
                if( b==NULL )
                    return(-1);
            }
            if( b->do_phase<1000 )
                r=do_event(b);
            else
            { // mY : ak bola udalost vybavena, odosiela sa ANSWER
              // mY : to vsak neplati pre umelo vyvolany event funkcie _init  
                if( b->handler == function_init ) // mY
                    r = 0; // mY
                else // mY
                    r=b->comm->answer(b->comm,b,r);
            } // mY
            //printf("ZZZ: do_event()=%d\n",r);
            if( r==1 )
                comm_buf_todo(b);
            else if( r<=0 )
            {   
                if( b->do_phase<1000 )
                {   
                    b->do_phase=1000;
                    comm_buf_todo(b);
                }
                else    
                    b->free(b);
            }
        }
        FD_ZERO(&rd);
        FD_ZERO(&wr);
        for(c=first_comm;c!=NULL;c=c->next)
        {   if( c->fd>=0 )
            {   if( c->fd > max )
                    max=c->fd;
                FD_SET(c->fd,&rd);
                if( c->output.first )
                    FD_SET(c->fd,&wr);
            }
        }
        r=select(max+1,&rd,&wr,NULL,NULL);
        if( r<=0 )
            continue;
        for(c=first_comm;c!=NULL;c=c->next)
        {   if( c->fd>=0 )
            {
                if( FD_ISSET(c->fd,&rd) )
                    if( c->read(c)<0 )
                    {   c->close(c);
                        continue;
                    }
                if( FD_ISSET(c->fd,&wr) )
                    if( c->write(c)<0 )
                    {   c->close(c);
                        continue;
                    }
            }
        }
    }

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
