/**
 * @file mcp.c
 * @short Medusa Communication Protocol handler
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: mcp.c,v 1.3 2002/10/23 10:25:43 marek Exp $
 */

#include <stdio.h>

#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include "../comm.h"
#include "../constable.h"
#include "../medusa_object.h"
#include "../object.h"
#include "../event.h"
#include <sys/param.h>
#include <endian.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

extern struct event_handler_s *function_init;

static int get_event_context( struct comm_s *comm, struct event_context_s *c, struct event_type_s *t, void *data );
static int mcp_opened( struct comm_s *c );
static int mcp_accept( struct comm_s *c );
static int mcp_r_greeting( struct comm_buffer_s *b );
static int mcp_read( struct comm_s *c );
static int mcp_r_head( struct comm_buffer_s *b );
static int mcp_r_query( struct comm_buffer_s *b );
static int mcp_answer( struct comm_s *c, struct comm_buffer_s *b, int result );
static int mcp_r_classdef_attr( struct comm_buffer_s *b );
static int mcp_r_acctypedef_attr( struct comm_buffer_s *b );
static int mcp_r_discard( struct comm_buffer_s *b );
static int mcp_write( struct comm_s *c );
static int mcp_nowrite( struct comm_s *c );
static int mcp_close( struct comm_s *c );
static int mcp_fetch_object( struct comm_s *c, int cont, struct object_s *o, struct comm_buffer_s *wake );
static int mcp_fetch_object_wait( struct comm_buffer_s *b );
static int mcp_r_fetch_answer( struct comm_buffer_s *b );
static int mcp_r_fetch_answer_done( struct comm_buffer_s *b );
static int mcp_update_object( struct comm_s *c, int cont, struct object_s *o, struct comm_buffer_s *wake );
static int mcp_update_object_wait( struct comm_buffer_s *b );
static int mcp_r_update_answer( struct comm_buffer_s *b );
static int mcp_conf_error( struct comm_s *c, const char *fmt, ... );

static int get_event_context( struct comm_s *comm, struct event_context_s *c, struct event_type_s *t, void *data )
{
    c->operation.next=&(c->subject);
    c->operation.attr.offset=0;
    c->operation.attr.length=t->m.size;
    c->operation.attr.type=MED_TYPE_END;
    strncpy(c->operation.attr.name,t->m.name,MIN(MEDUSA_ATTRNAME_MAX,MEDUSA_OPNAME_MAX));
    c->operation.flags=comm->flags;
    c->operation.class=&(t->operation_class);
    c->operation.data=(char*)(data)+sizeof(MCPptr_t)+sizeof(unsigned int);

    c->subject.next=&(c->object);
    c->subject.attr.offset=c->subject.attr.length=0;
    c->subject.attr.type=MED_TYPE_END;
    strncpy(c->subject.attr.name,t->m.op_name[0],MEDUSA_ATTRNAME_MAX);
    c->subject.flags=comm->flags;
    c->subject.class=t->op[0];
    c->subject.data=(char*)(data)+sizeof(MCPptr_t)+sizeof(unsigned int)+(t->m.size);
    c->object.next=NULL;
    c->object.attr.offset=c->object.attr.length=0;
    c->object.attr.type=MED_TYPE_END;
    strncpy(c->object.attr.name,t->m.op_name[1],MEDUSA_ATTRNAME_MAX);
    c->object.flags=comm->flags;
    c->object.class=t->op[1];
    c->object.data=(char*)(data)+sizeof(MCPptr_t)+sizeof(unsigned int)+(t->m.size);
    if( c->subject.class!=NULL )
    {	c->object.data+= c->subject.class->m.size;
        c->subject.attr.length=c->subject.class->m.size;
    }
    if( c->object.class!=NULL )
    {
        c->object.attr.length=c->object.class->m.size;
    }
    c->local_vars=NULL;
    return(0);
}

struct mcp_comm_s {
    struct comm_s	*next;
    struct comm_s	*to_accept;
    in_addr_t	allow_ip;
    in_addr_t	allow_mask;
    in_port_t	allow_port;
};
#define	mcp_data(c)	((struct mcp_comm_s*)(comm_user_data(c)))

struct comm_s *mcp_alloc_comm( char *name )
{ struct comm_s *c;
    if( (c=comm_new(name,sizeof(struct mcp_comm_s)))==NULL )
        return(NULL);
    c->state=0;
    c->read=mcp_read;
    c->write=mcp_write;
    c->close=mcp_close;
    c->answer=mcp_answer;
    c->fetch_object=mcp_fetch_object;
    c->update_object=mcp_update_object;
    c->conf_error=mcp_conf_error;
    return(c);
}

static int mcp_opened( struct comm_s *c )
{
    class_free_all_clases(c);
    event_free_all_events(c);
    c->open_counter++;
    if( c->buf==NULL && (c->buf=comm_buf_get(2048,c))==NULL )
        return(-1);
    c->buf->want=1*sizeof(MCPptr_t);
    c->buf->completed=mcp_r_greeting;
    return(0);
}

int mcp_open( struct comm_s *c, char *filename )
{
    if( (c->fd=open(filename,O_RDWR))<0 )
    {	init_error("Can't open %s",filename);
        return(-1);
    }
    return(mcp_opened(c));
}

struct comm_s *mcp_listen( in_port_t port )
{ static struct comm_s *all=NULL;
    struct comm_s *p;
    struct sockaddr_in addr;
    for(p=all;p!=NULL;p=mcp_data(p)->next)
        if( mcp_data(p)->allow_port==port )
            return(p);
    if( (p=mcp_alloc_comm(retprintf("#%d",port)))==NULL )
        return(NULL);
    mcp_data(p)->allow_port=port;
    if( (p->fd=socket(PF_INET,SOCK_STREAM,0))<0 )
    {	init_error("Can't create socket");
        return(NULL);
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr=INADDR_ANY;
    addr.sin_port=htons(port);
    if( bind(p->fd,(struct sockaddr*)&addr,sizeof(struct sockaddr_in))<0 )
    {	close(p->fd);
        p->fd= -1;
        init_error("Can't bind to port %d",port);
        return(NULL);
    }
    listen(p->fd,8);
    p->open_counter++;
    p->read=mcp_accept;
    p->write=mcp_nowrite;
    mcp_data(p)->next=all;
    all=p;
    return(p);
}

int mcp_to_accept( struct comm_s *c, struct comm_s *listen, in_addr_t ip, in_addr_t mask, in_port_t port )
{
    if( listen==NULL )	return(-1);
    mcp_data(c)->allow_ip=ip;
    mcp_data(c)->allow_mask=mask;
    mcp_data(c)->allow_port=port;
    mcp_data(c)->next=mcp_data(listen)->to_accept;
    mcp_data(listen)->to_accept=c;
    return(0);
}

static int mcp_accept( struct comm_s *c )
{
    struct sockaddr_in addr;
    socklen_t len;
    int sock;
    struct comm_s *p;
    len=sizeof(addr);
    if( (sock=accept(c->fd,(struct sockaddr*)(&addr),&len))<0 )
    {	comm_error("comm %s: error in accept()",c->name);
        return(-1);
    }
    for(p=mcp_data(c)->to_accept;p!=NULL;p=mcp_data(p)->next)
        if( (mcp_data(p)->allow_ip & mcp_data(p)->allow_mask)
                == (addr.sin_addr.s_addr & mcp_data(p)->allow_mask)
                &&
                (mcp_data(p)->allow_port==0 ||
                 (mcp_data(p)->allow_port==ntohs(addr.sin_port)))
                )
        {	if( p->fd >= 0 )
            {	comm_error("comm %s: reconnect",p->name);
                p->close(p);
            }
            p->fd=sock;
            return(mcp_opened(p));
        }
    comm_error("comm %s: unauthorized access from [%s:%d] => close",c->name,inet_ntoa(addr.sin_addr),ntohs(addr.sin_port));
    close(sock);
    return(0);
}

static int mcp_r_greeting( struct comm_buffer_s *b )
{
    switch( ((MCPptr_t*)(b->buf))[0] )
    {	case 0x66007e5a:
        comm_info("comm %s: has native byte order ;-D",b->comm->name);
        b->comm->flags=0;
        break;
    case 0x5a7e0066:
        comm_info("comm %s: need to translate byte order ;-)",b->comm->name);
        b->comm->flags=OBJECT_FLAG_CHENDIAN;
        break;
    case 0x00000000:
        comm_error("comm %s: does not support greeting ;-(",b->comm->name);
        b->comm->flags=0;
        b->want=sizeof(MCPptr_t)+sizeof(unsigned int);
        b->completed=mcp_r_head;
        return(0);
#if __BYTE_ORDER == __BIG_ENDIAN
    case 0x00665a7e:
#elif __BYTE_ORDER == __LITTLE_ENDIAN
    case 0x7e5a6600:
#endif
        comm_error("comm %s: has PDP byte order ;-/ (if you realy need this please e-mail me to <marek@terminu.sk>)",b->comm->name);
        return(-1);
#if __BYTE_ORDER == __BIG_ENDIAN
    case 0x66001fcb:
#elif __BYTE_ORDER == __LITTLE_ENDIAN
    case 0xcb1f0066:
#endif
        comm_error("comm %s: is 9 bit big endian ;-S",b->comm->name);
        return(-1);
#if __BYTE_ORDER == __BIG_ENDIAN
    case 0x9b3f800c:
#elif __BYTE_ORDER == __LITTLE_ENDIAN
    case 0x0c803f9b:
#endif
        comm_error("comm %s: is 9 bit little endian ;-s",b->comm->name);
        return(-1);
    default:
        comm_error("comm %s: has some exotic byte order ;-|",b->comm->name);
        return(-1);
    }
    b->comm->buf=NULL;
    b->free(b);
    return(0);
}

static int mcp_read( struct comm_s *c )
{
    int r;
    if( c->buf==NULL && (c->buf=comm_buf_get(2048,c))==NULL )
        return(0);
    if( c->buf->want==0 )
    {	c->buf->want=sizeof(uint32_t)+sizeof(MCPptr_t);
        c->buf->completed=mcp_r_head;
    }
    if( c->buf->want > c->buf->size && c->buf->pbuf==c->buf->buf )
    { struct comm_buffer_s *b;
        if( (b=comm_buf_resize(c->buf,c->buf->want))==NULL )
        {	fatal(Out_of_memory);
            return(-1);
        }
        c->buf=b;
    }
    if( c->buf->len < c->buf->want )
    {	r=read(c->fd,c->buf->pbuf+c->buf->len,c->buf->want-c->buf->len);
        if( r<=0 )
        {	comm_error("medusa comm %s: Read error or EOF (%d)",c->name,r);
            return(-1);
        }
        c->buf->len+=r;
        if( c->buf->len < c->buf->want )
            return(0);
    }
    return(c->buf->completed(c->buf));
}

static int mcp_r_head( struct comm_buffer_s *b )
{
    MCPptr_t x;
    if( (x=((MCPptr_t*)(b->buf))[0])!=0 )
    {
        if( (b->event=(struct event_type_s*)hash_find(&(b->comm->events),x))==NULL )
        {	comm_error("comm %s: Unknown access type %p!",b->comm->name,x);
            return(-1);
        }
        b->want= b->len + (b->event->m.size);
        if( b->event->op[0]!=NULL )
            b->want += b->event->op[0]->m.size;
        if( b->event->op[1]!=NULL )
            b->want += b->event->op[1]->m.size;
        b->completed=mcp_r_query;
        return(0);
    }
    else switch( byte_reorder_get_int32(b->comm->flags,((unsigned int*)(b->buf+sizeof(MCPptr_t)))[0]) )
    {
    case MEDUSA_COMM_CLASSDEF:
        b->want = b->len + sizeof(struct medusa_comm_class_s)+sizeof(struct medusa_comm_attribute_s);
        b->completed=mcp_r_classdef_attr;
        break;
    case MEDUSA_COMM_ACCTYPEDEF:
        b->want = b->len + sizeof(struct medusa_comm_acctype_s)+sizeof(struct medusa_comm_attribute_s);
        b->completed=mcp_r_acctypedef_attr;
        break;
    case MEDUSA_COMM_CLASSUNDEF:
    case MEDUSA_COMM_ACCTYPEUNDEF:
        runtime("Huh, I don't know how to undef class or acctype ;-|");
        b->want = b->len + sizeof(unsigned int);
        b->completed=mcp_r_discard;
        break;
    case MEDUSA_COMM_FETCH_ERROR:
    case MEDUSA_COMM_FETCH_ANSWER:
        b->want = b->len + 2*sizeof(MCPptr_t); //+sizeof(unsigned int);
        b->completed=mcp_r_fetch_answer;
        break;
    case MEDUSA_COMM_UPDATE_ANSWER:
        b->want = b->len + 2*sizeof(MCPptr_t)+sizeof(unsigned int);
        b->completed=mcp_r_update_answer;
        break;
    default:
        comm_error("comm %s: Communication protocol error! (%d)",b->comm->name,((unsigned int*)(b->buf+sizeof(MCPptr_t)))[0]);
        return(-1);
    }
    return(0);
}

static int mcp_r_query( struct comm_buffer_s *b )
{
    get_event_context(b->comm, &(b->context), b->event, b->buf );
    b->ehh_list=EHH_VS_ALLOW;
    printf("ZZZ kim rychla %d\n",b->comm->state);
    //fflush(stdout);
    // THR LOCK COMM STATE
    if( b->comm->state==0 )
    {
        printf("ZZZ net slunicko\n");
        //fflush(stdout);
        if( comm_conn_init(b->comm)<0 )
            return(-1); // THR UNLOCK COMM STATE
        printf("ZZZ dan slanina\n");
        //fflush(stdout);
        if( function_init!=NULL )
        { struct comm_buffer_s *p;
            if( (p=comm_buf_get(0,b->comm))==NULL )
            {	comm_error("Can't get comm buffer for _init");
                // THR UNLOCK COMM STATE
                return(-1);
            }
            get_empty_context(&(p->context));
            p->event=NULL;
            p->handler=function_init;
            b->comm->buf=NULL;
            comm_buf_to_queue(&(p->to_wake),b);
            p->ehh_list=EHH_NOTIFY_ALLOW;
            comm_buf_todo(p);
            b->comm->state=1;
            // THR UNLOCK COMM STATE
            return(0);
        }
        b->comm->state=1;
    }
    // THR UNLOCK COMM STATE
    b->comm->buf=NULL;
    comm_buf_todo(b);
    return(0);
}

static int mcp_answer( struct comm_s *c, struct comm_buffer_s *b, int result )
{ struct comm_buffer_s *r;
    #pragma pack(push)
    #pragma pack(1)
    struct out_struct {
        MCPptr_t ans, id;
        uint16_t res;
    } * out;
    #pragma pack(pop)
    int i;
    /* TODO: only if changed */
    printf("ZZZ: answer\n");
    if( b->context.result >=0 && b->context.subject.class!=NULL )
    {	if( b->do_phase==0 )
            b->do_phase=1000;
        printf("ZZZ: snazim sa updatnut\n");
        i=c->update_object(c,b->do_phase-1000,&(b->context.subject),b);
        if( i>0 )
        {	b->do_phase=i+1000;
            return(i);
        }
        printf("ZZZ: updatnute\n");
    }
    else printf("ZZZ: b->context.result=%d b->context.subject.class=%p\n",b->context.result,b->context.subject.class);
    if( (r=comm_buf_get(sizeof(*out),c))==NULL )     //( povodne uintptr_t )Asi to ma byt takto inac dava bludy v mallocu - prepisuje hodnotu user_data, by Matus
    {	fatal("Can't alloc buffer for send answer!");
        return(-1);
    }
    out = (void*)&r->buf;
    // TODO TODO TODO: mY: 'ans' has 64 bits, NOT 32 !!!
    out->ans= byte_reorder_put_int64(c->flags,MEDUSA_COMM_AUTHANSWER);
    /* TODO TODO TODO: mY: 
        'id' has 32 bits within an 'authrequest' cmd
        but... big&little endians problem...
        'id' is ignored by kernel, is always set to 0
    */
    out->id = 0;
    out->id = ((uint32_t*)(b->buf+sizeof(MCPptr_t)))[0];
#ifdef TRANSLATE_RESULT
    /* TODO TODO TODO: mY:
        WHY access 'out->res' by 'r->buf'
        see '#else' statement
    */
    switch( b->context.result )
    {	case RESULT_ALLOW:
        *((uint16_t*)(r->buf+2*sizeof(MCPptr_t)))
                = MED_YES;
        break;
    case RESULT_DENY:
        *((uint16_t*)(r->buf+2*sizeof(MCPptr_t)))
                = MED_NO;
        break;
    case RESULT_SKIP:
        *((uint16_t*)(r->buf+2*sizeof(MCPptr_t)))
                = MED_SKIP;
        break;
    case RESULT_OK:
        *((uint16_t*)(r->buf+2*sizeof(MCPptr_t)))
                = MED_OK;
        break;
    default:
        *((uint16_t*)(r->buf+2*sizeof(MCPptr_t)))
                = MED_ERR;
    }
    *((uint16_t*)(r->buf+2*sizeof(MCPptr_t)))=
            byte_reorder_put_int16(c->flags,
                                   *((uint16_t*)(r->buf+2*sizeof(MCPptr_t))));
#else
    out->res =
            byte_reorder_put_int16(c->flags,b->context.result);
#endif
    r->len=sizeof(*out);
    r->want=0;
    r->completed=NULL;
    comm_buf_to_queue(&(c->output),r);
    return(0);
}

static void unify_bitmap_types(struct medusa_comm_attribute_s *a)
{
    while( a->type!=MED_COMM_TYPE_END ) {
        switch (a->type) {
        case MED_COMM_TYPE_BITMAP_8:
        case MED_COMM_TYPE_BITMAP_16:
        case MED_COMM_TYPE_BITMAP_32:
            a->type = MED_COMM_TYPE_BITMAP;
        }
        a++;
    }
}

static int mcp_r_classdef_attr( struct comm_buffer_s *b )
{
    struct class_s *cl;
    if( ((struct medusa_comm_attribute_s *)(b->buf+b->len-sizeof(struct medusa_comm_attribute_s)))->type!=MED_TYPE_END )
    {	b->want = b->len + sizeof(struct medusa_comm_attribute_s);
        return(0);
    }
    byte_reorder_class(b->comm->flags,(struct medusa_class_s*)(b->buf+sizeof(MCPptr_t)+sizeof(int)));
    byte_reorder_attrs(b->comm->flags,(struct medusa_attribute_s*)(b->buf+sizeof(MCPptr_t)+sizeof(int)+sizeof(struct medusa_comm_class_s)));
    unify_bitmap_types((struct medusa_attribute_s*)(b->buf+sizeof(MCPptr_t)+sizeof(int)+sizeof(struct medusa_comm_class_s)));
    if( (cl=add_class(b->comm,
                      (struct medusa_class_s*)(b->buf+sizeof(MCPptr_t)+sizeof(int)),
                      (struct medusa_attribute_s*)(b->buf+sizeof(MCPptr_t)+sizeof(int)+sizeof(struct medusa_comm_class_s))))==NULL )
        comm_error("comm %s: Can't add class",b->comm->name);
    b->comm->buf=NULL;
    b->free(b);
    return(0);
}

static int mcp_r_acctypedef_attr( struct comm_buffer_s *b )
{
    if( ((struct medusa_comm_attribute_s *)(b->buf+b->len-sizeof(struct medusa_comm_attribute_s)))->type!=MED_TYPE_END )
    {	b->want = b->len + sizeof(struct medusa_comm_attribute_s);
        return(0);
    }
    byte_reorder_acctype(b->comm->flags,(struct medusa_acctype_s*)(b->buf+sizeof(MCPptr_t)+sizeof(unsigned int)));
    byte_reorder_attrs(b->comm->flags,(struct medusa_attribute_s*)(b->buf+sizeof(MCPptr_t)+sizeof(unsigned int)+sizeof(struct medusa_comm_acctype_s)));
    unify_bitmap_types((struct medusa_attribute_s*)(b->buf+sizeof(MCPptr_t)+sizeof(unsigned int)+sizeof(struct medusa_comm_acctype_s)));
    if( event_type_add(b->comm,(struct medusa_acctype_s*)(b->buf+sizeof(MCPptr_t)+sizeof(unsigned int)),
                       (struct medusa_attribute_s*)(b->buf+sizeof(MCPptr_t)+sizeof(unsigned int)+sizeof(struct medusa_comm_acctype_s)))<0 )
        comm_error("comm %s: Can't add acctype",b->comm->name);
    b->comm->buf=NULL;
    b->free(b);
    return(0);
}

static int mcp_r_discard( struct comm_buffer_s *b )
{
    b->comm->buf=NULL;
    b->free(b);
    return(0);
}

static int mcp_write( struct comm_s *c )
{ int r;
    struct comm_buffer_s *b;
    printf("ZZZ mcp_write 1\n");
    if( (b=comm_buf_peek_first(&c->output))==NULL )
        return(0);
    printf("ZZZ mcp_write 2\n");
    if( b->open_counter != c->open_counter )
    {	b=comm_buf_from_queue(&(c->output));
        b->free(b);
        return(0);
    }
    printf("ZZZ mcp_write 3\n");
    if( b->want < b->len )
    {	r=write(c->fd,b->pbuf+b->want,b->len-b->want);
        printf("ZZZ mcp_write 4 r=%d\n",r);
        if( r<=0 )
        {	comm_error("medusa comm %s: Write error",c->name);
            return(-1);
        }
        b->want+=r;
        if( b->want < b->len )
            return(0);
    }
    b=comm_buf_from_queue(&(c->output));
    if( b->completed )
        r=b->completed(b);
    else
    {	r=0;
        b->free(b);
    }
    return(r);
}

static int mcp_nowrite( struct comm_s *c )
{ struct comm_buffer_s *b;
    while( (b=comm_buf_from_queue(&(c->output)))!=NULL )
    {	b->free(b);
        return(0);
    }
    return(0);
}

static int mcp_close( struct comm_s *c )
{ struct comm_buffer_s *b;
    close(c->fd);
    c->fd= -1;
    if( c->buf )
    {	c->buf->free(c->buf);
        c->buf=NULL;
    }
    while( (b=comm_buf_from_queue(&(c->output))) )
        b->free(b);
    while( (b=comm_buf_from_queue(&(c->wait_for_answer))) )
        b->free(b);
    c->open_counter--;
    return(0);
}

static int mcp_fetch_object( struct comm_s *c, int cont, struct object_s *o, struct comm_buffer_s *wake )
{ static MCPptr_t id=2;
    struct comm_buffer_s *r;
    printf("ZZZZ mcp_fetch_object 1\n");
    if( cont==3 )
    {
        if( debug_do_out!=NULL )
        {	
            pthread_mutex_lock(&debug_do_lock);
            debug_do_out(debug_do_arg,"fetch ");
            object_print(o,debug_do_out,debug_do_arg);
            pthread_mutex_unlock(&debug_do_lock);
        }
        return(wake->user_data);	/* done */
    }
    printf("ZZZZ mcp_fetch_object 2\n");
    wake->user_data = -1;
    if( c->wait_for_answer.last!=NULL )	/* lebo kernel ;-( */
    {
        comm_buf_to_queue(&(c->wait_for_answer.last->buffer->to_wake),wake);
        return(2);
    }
    printf("ZZZZ mcp_fetch_object 3\n");
    if( (r=comm_buf_get(3*sizeof(MCPptr_t) + o->class->m.size,c))==NULL )
    {	fatal("Can't alloc buffer for fetch!");
        return(-1);
    }
    printf("ZZZZ mcp_fetch_object 4\n");
    r->user1=(void*)o;
    object_set_byte_order(o,c->flags);
    r->user2=(void*)(&(wake->user_data));
    ((MCPptr_t*)(r->buf))[0]= byte_reorder_put_int32(c->flags,MEDUSA_COMM_FETCH_REQUEST);
    ((MCPptr_t*)(r->buf))[1]= o->class->m.classid;
    ((MCPptr_t*)(r->buf))[2]= id++;
    memcpy(((MCPptr_t*)(r->buf))+3, o->data, o->class->m.size);
    r->len=3*sizeof(MCPptr_t) + o->class->m.size;
    r->want=0;
    r->completed=mcp_fetch_object_wait;
    comm_buf_to_queue(&(r->to_wake),wake);
    comm_buf_to_queue(&(c->output),r);
    printf("ZZZZ mcp_fetch_object 5\n");
    return(3);
}

static int mcp_fetch_object_wait( struct comm_buffer_s *b )
{
    printf("ZZZZ mcp_fetch_object_wait\n");
    comm_buf_to_queue(&(b->comm->wait_for_answer),b);
    return(0);
}

static int mcp_r_fetch_answer( struct comm_buffer_s *b )
{ struct comm_buffer_s *f,*p;
    #pragma pack(push)
    #pragma pack(1)
    struct {
        MCPptr_t p1,p2;
    } * bmask = (void*)( b->buf + sizeof(uint32_t) + sizeof(MCPptr_t)), *pmask;
    #pragma pack(pop)
    f=comm_buf_peek_first(&b->comm->wait_for_answer);
    p=NULL;
    do {	p=comm_buf_from_queue(&(b->comm->wait_for_answer));
        pmask = (void*)(p->buf + sizeof(MCPptr_t));
        if( byte_reorder_get_int32(b->comm->flags,((MCPptr_t*)(p->buf))[0])==MEDUSA_COMM_FETCH_REQUEST
                && pmask->p1==bmask->p1
                && pmask->p2==bmask->p2
                )
            break;
        comm_buf_to_queue(&(b->comm->wait_for_answer),p);
        p=NULL;
    } while( f != b->comm->wait_for_answer.first->buffer );

    if( byte_reorder_get_int32(b->comm->flags,((unsigned int*)(b->buf + sizeof(MCPptr_t)))[0])==MEDUSA_COMM_FETCH_ERROR )
    {	if( p!=NULL )
            p->free(p);
        b->comm->buf=NULL;
        b->free(b);
        return(0);
    }

    if( p!=NULL )
    {
        b->len = 0;
        b->want=((struct object_s *)(p->user1))->class->m.size;
        b->pbuf=((struct object_s *)(p->user1))->data;
        b->user1=(void*)p;
        b->completed=mcp_r_fetch_answer_done;
    }
    else
    { struct class_s *cl;
        cl=(struct class_s*)hash_find(&(b->comm->classes),bmask->p1);
        if( cl==NULL )
        {	comm_error("comm %s: Can't find class by class id",b->comm->name);
            return(-1);
        }
        b->want = b->len + cl->m.size;
        b->completed=mcp_r_discard;
    }
    return(0);
}

static int mcp_r_fetch_answer_done( struct comm_buffer_s *b )
{ struct comm_buffer_s *p;
    p=(struct comm_buffer_s *)(b->user1);
    if( p!=NULL )
    {	*((uint32_t*)(p->user2))=0;	/* success */ // Zmenene uintptr_t z na uint32_t - prepisovanie do_phase, by Matus
        p->free(p);
    }
    b->comm->buf=NULL;
    b->free(b);
    return(0);
}

static int mcp_update_object( struct comm_s *c, int cont, struct object_s *o, struct comm_buffer_s *wake )
{ static MCPptr_t id=2;
    struct comm_buffer_s *r;
    if( cont==3 )
    {
#ifdef TRANSLATE_RESULT
        if( wake->user_data==MED_OK )
#else
        if( wake->user_data==RESULT_OK )/* ma byt vzdy len MED_OK !!! */
#endif
            return(0);	/* done */
        return(-1);		/* done */
    }
#ifdef TRANSLATE_RESULT
    wake->user_data = MED_ERR;
#else
    wake->user_data = RESULT_UNKNOWN;  /* ma byt vzdy len MED_ERR !!! */
#endif
    if( c->wait_for_answer.last!=NULL )	/* lebo kernel ;-( */
    {	comm_buf_to_queue(&(c->wait_for_answer.last->buffer->to_wake),wake);
        return(2);
    }
    if( (r=comm_buf_get(3*sizeof(MCPptr_t) + ((struct object_s *)((void*)o))->class->m.size,c))==NULL )
    {	fatal("Can't alloc buffer for update!");
        return(-1);
    }

    if( debug_do_out!=NULL )
    {	
        pthread_mutex_lock(&debug_do_lock);
        debug_do_out(debug_do_arg,"update ");
        object_print(o,debug_do_out,debug_do_arg);
        pthread_mutex_unlock(&debug_do_lock);
    }

    r->user1=(void*)o;
    r->user2=(void*)(&(wake->user_data));
    ((MCPptr_t*)(r->buf))[0]= byte_reorder_put_int32(c->flags,MEDUSA_COMM_UPDATE_REQUEST);
    ((MCPptr_t*)(r->buf))[1]= o->class->m.classid;
    ((MCPptr_t*)(r->buf))[2]= id++;
    memcpy(((MCPptr_t*)(r->buf))+3, o->data, o->class->m.size);

    object_set_byte_order(o,r->comm->flags);
    r->len=3*sizeof(MCPptr_t) + o->class->m.size;
    r->want=0;
    r->completed=mcp_update_object_wait;
    comm_buf_to_queue(&(r->to_wake),wake);
    comm_buf_to_queue(&(c->output),r);
    return(3);
}

static int mcp_update_object_wait( struct comm_buffer_s *b )
{
    printf("ZZZZ mcp_update_object_wait\n");
    comm_buf_to_queue(&(b->comm->wait_for_answer),b);
    return(0);
}

static int mcp_r_update_answer( struct comm_buffer_s *b )
{ struct comm_buffer_s *f,*p;
    #pragma pack(push)
    #pragma pack(1)
    struct {
        MCPptr_t p1,p2,user;
    } * bmask = (void*)(b->buf + sizeof(uint32_t) + sizeof(MCPptr_t)), *pmask;
    #pragma pack(pop)
    f=comm_buf_peek_first(&b->comm->wait_for_answer);
    p=NULL;
    do {	p=comm_buf_from_queue(&(b->comm->wait_for_answer));
        pmask = (void*) (p->buf + sizeof(MCPptr_t));
        if( byte_reorder_get_int32(b->comm->flags,*(MCPptr_t*)p->buf)==MEDUSA_COMM_UPDATE_REQUEST
                && pmask->p1==bmask->p1 && pmask->p2==bmask->p2)
            break;
        comm_buf_to_queue(&(b->comm->wait_for_answer),p);
        p=NULL;
    } while( f != b->comm->wait_for_answer.first->buffer );

    if( p!=NULL )
    {
        *((uint32_t*)(p->user2))=
                byte_reorder_put_int32(b->comm->flags,bmask->user);   // Zmenene uintptr_t z na uint32_t - prepisovanie do_phase, by Matus
        p->free(p);
    }
    b->comm->buf=NULL;
    b->free(b);
    return(0);
}

int mcp_language_do( char *filename );

int mcp_init( char *filename )
{
    return(mcp_language_do(filename));
}

static int mcp_conf_error( struct comm_s *comm, const char *fmt, ... )
{ va_list ap;
    char buf[4096];
    sprintf(buf,"comm %s: ",comm->name);
    va_start(ap,fmt);
    vsnprintf(buf+strlen(buf), 4000, fmt, ap);
    va_end(ap);
    sprintf(buf+strlen(buf),"\n");
    write(1,buf,strlen(buf));
    return(-1);
}


