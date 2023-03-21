// SPDX-License-Identifier: GPL-2.0
/**
 * @file mcp.c
 * @short Medusa Communication Protocol handler
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
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
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "mcp.h"

extern struct event_handler_s *function_init;
static struct comm_s *all_comms;

/**
 * Enumeration of possible return values for mcp handlers
 */
enum read_result {
	READ_ERROR = -1,    /**< an error during read operation */
	READ_DONE,          /**< successful read operation */
	READ_FREE,          /**< successful read operation and caller should free the buffer */
};

static int get_event_context(struct comm_s *comm, struct event_context_s *c,
			     struct event_type_s *t, void *data);
static struct comm_buffer_s *mcp_opened(struct comm_s *c);
static int mcp_accept(struct comm_s *c);
static enum read_result mcp_r_greeting(struct comm_buffer_s *b);
static int mcp_read_worker(struct comm_s *c);
static enum read_result mcp_r_head(struct comm_buffer_s *b);
static enum read_result mcp_r_query(struct comm_buffer_s *b);
static int mcp_answer(struct comm_s *c, struct comm_buffer_s *b);
static enum read_result mcp_r_classdef_attr(struct comm_buffer_s *b);
static enum read_result mcp_r_acctypedef_attr(struct comm_buffer_s *b);
static enum read_result mcp_r_discard(struct comm_buffer_s *b);
static enum read_result mcp_r_ready_request(struct comm_buffer_s *b);
static int mcp_write(struct comm_s *c);
static int mcp_nowrite(struct comm_s *c);
static int mcp_close(struct comm_s *c);
static int mcp_fetch_object(struct comm_s *c, int cont, struct object_s *o,
			    struct comm_buffer_s *wake);
static enum read_result mcp_r_fetch_answer(struct comm_buffer_s *b);
static enum read_result mcp_r_fetch_answer_done(struct comm_buffer_s *b);
static int mcp_update_object(struct comm_s *c, int cont, struct object_s *o,
			     struct comm_buffer_s *wake);
static enum read_result mcp_r_update_answer(struct comm_buffer_s *b);
static int mcp_conf_error(struct comm_s *c, const char *fmt, ...);

static int get_event_context(struct comm_s *comm,
			     struct event_context_s *c,
			     struct event_type_s *t,
			     void *data)
{
	c->operation.next = &c->subject;
	c->operation.attr.offset = 0;
	c->operation.attr.length = t->acctype.size;
	c->operation.attr.type = MED_TYPE_END;
	strncpy(c->operation.attr.name, t->acctype.name,
		MIN(MEDUSA_ATTRNAME_MAX, MEDUSA_OPNAME_MAX));
	c->operation.flags = comm->flags;
	c->operation.class = &t->operation_class;
	c->operation.data = (char *)(data) + 2 * sizeof(MCPptr_t);

	c->subject.next = &c->object;
	c->subject.attr.offset = 0;
	c->subject.attr.length = 0;
	c->subject.attr.type = MED_TYPE_END;
	strncpy(c->subject.attr.name, t->acctype.op_name[0], MEDUSA_ATTRNAME_MAX);
	c->subject.flags = comm->flags;
	c->subject.class = t->op[0];
	c->subject.data = (char *)(data) + 2 * sizeof(MCPptr_t) + (t->acctype.size);
	c->object.next = NULL;
	c->object.attr.offset = 0;
	c->object.attr.length = 0;
	c->object.attr.type = MED_TYPE_END;
	strncpy(c->object.attr.name, t->acctype.op_name[1], MEDUSA_ATTRNAME_MAX);
	c->object.flags = comm->flags;
	c->object.class = t->op[1];
	c->object.data = (char *)(data) + 2 * sizeof(MCPptr_t) + (t->acctype.size);
	if (c->subject.class) {
		c->object.data += c->subject.class->m.size;
		c->subject.attr.length = c->subject.class->m.size;
	}
	if (c->object.class)
		c->object.attr.length = c->object.class->m.size;
	c->local_vars = NULL;

	return 0;
}

struct mcp_comm_s {
	struct comm_s	*next;
	struct comm_s	*to_accept;
	in_addr_t	allow_ip;
	in_addr_t	allow_mask;
	in_port_t	allow_port;
};

struct comm_s *mcp_alloc_comm(char *name)
{
	struct comm_s *c;

	c = comm_new(name, sizeof(struct mcp_comm_s));
	if (unlikely(!c))
		return NULL;

	c->state = 0;
	c->read = mcp_read_worker;
	c->write = mcp_write;
	c->close = mcp_close;
	c->answer = mcp_answer;
	c->fetch_object = mcp_fetch_object;
	c->update_object = mcp_update_object;
	c->conf_error = mcp_conf_error;

	return c;
}

/**
 * Prepares the communication interface for usage. Creates a buffer for the
 * greeting message. Called from comm_do() for each communication interface.
 */
static struct comm_buffer_s *mcp_opened(struct comm_s *c)
{
	struct comm_buffer_s *b;

	class_free_all_clases(c);
	event_free_all_events(c);
	c->open_counter++;
	b = comm_buf_get(2048, c);
	if (unlikely(!b)) {
		fatal("Not enough memory for communication buffer");
		return NULL;
	}
	b->want = 2 * sizeof(MCPptr_t);
	b->completed = mcp_r_greeting;
	return b;
}

/**
 * Opens a file descriptor of the communication interface. Called from the
 * configuration file parser.
 */
int mcp_open(struct comm_s *c, char *filename)
{
	c->fd = comm_open_skip_stdfds(filename, O_RDWR, 0);
	if (unlikely(c->fd < 0))
		return -1;
	return 0;
}

#define MCP_DATA(c)	((struct mcp_comm_s *)(comm_user_data(c)))

struct comm_s *mcp_listen(in_port_t port)
{
	struct comm_s *p;
	struct sockaddr_in addr;

	for (p = all_comms; p; p = MCP_DATA(p)->next) {
		if (MCP_DATA(p)->allow_port == port)
			return p;
	}
	p = mcp_alloc_comm(retprintf("#%d", port));
	if (unlikely(!p))
		return NULL;
	MCP_DATA(p)->allow_port = port;
	p->fd = socket(PF_INET, SOCK_STREAM, 0);
	if (unlikely(p->fd < 0)) {
		init_error("Can't create socket");
		return NULL;
	}
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);
	if (unlikely(bind(p->fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0)) {
		close(p->fd);
		p->fd = -1;
		init_error("Can't bind to port %d", port);
		return NULL;
	}
	listen(p->fd, 8);
	p->open_counter++;
	p->read = mcp_accept;
	p->write = mcp_nowrite;
	MCP_DATA(p)->next = all_comms;
	all_comms = p;

	return p;
}

int mcp_to_accept(struct comm_s *c, struct comm_s *listen,
		  in_addr_t ip,	in_addr_t mask, in_port_t port)
{
	if (unlikely(!listen))
		return -1;

	MCP_DATA(c)->allow_ip = ip;
	MCP_DATA(c)->allow_mask = mask;
	MCP_DATA(c)->allow_port = port;
	MCP_DATA(c)->next = MCP_DATA(listen)->to_accept;
	MCP_DATA(listen)->to_accept = c;

	return 0;
}

static int mcp_accept(struct comm_s *c)
{
	struct sockaddr_in addr;
	socklen_t len;
	int sock;
	struct comm_s *p;

	len = sizeof(addr);
	sock = accept(c->fd, (struct sockaddr *)(&addr), &len);
	if (unlikely(sock < 0)) {
		comm_error("comm %s: error in accept()", c->name);
		return -1;
	}

	for (p = MCP_DATA(c)->to_accept; p; p = MCP_DATA(p)->next) {
		if ((MCP_DATA(p)->allow_ip & MCP_DATA(p)->allow_mask)
		    == (addr.sin_addr.s_addr & MCP_DATA(p)->allow_mask) &&
		    (MCP_DATA(p)->allow_port == 0 ||
		    (MCP_DATA(p)->allow_port == ntohs(addr.sin_port)))
		   ) {
			if (p->fd >= 0) {
				comm_error("comm %s: reconnect", p->name);
				p->close(p);
			}
			p->fd = sock;
			return 0;
		}
	}

	comm_error("comm %s: unauthorized access from [%s:%d] => close",
		   c->name, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
	close(sock);
	return 0;
}

#undef MCP_DATA

/**
 * Reads the greeting message from the kernel and sets endianness used for
 * communication.
 */
static enum read_result mcp_r_greeting(struct comm_buffer_s *b)
{
	switch (((MCPptr_t *)(b->comm_buf))[0]) {
	case 0x66007e5a:
		comm_info("comm %s: has native byte order ;-D", b->comm->name);
		b->comm->flags = 0;
		break;
	case 0x5a7e0066:
		comm_info("comm %s: need to translate byte order ;-)", b->comm->name);
		b->comm->flags = OBJECT_FLAG_CHENDIAN;
		break;
	case 0x00000000:
		comm_error("comm %s: does not support greeting ;-(", b->comm->name);
		b->comm->flags = 0;
		b->want = sizeof(MCPptr_t) + sizeof(unsigned int);
		b->completed = mcp_r_head;
		return READ_DONE;
#if __BYTE_ORDER == __BIG_ENDIAN
	case 0x00665a7e:
#elif __BYTE_ORDER == __LITTLE_ENDIAN
	case 0x7e5a6600:
#endif
		comm_error("comm %s: has PDP byte order ;-/", b->comm->name);
		return READ_ERROR;
#if __BYTE_ORDER == __BIG_ENDIAN
	case 0x66001fcb:
#elif __BYTE_ORDER == __LITTLE_ENDIAN
	case 0xcb1f0066:
#endif
		comm_error("comm %s: is 9 bit big endian ;-S", b->comm->name);
		return READ_ERROR;
#if __BYTE_ORDER == __BIG_ENDIAN
	case 0x9b3f800c:
#elif __BYTE_ORDER == __LITTLE_ENDIAN
	case 0x0c803f9b:
#endif
		comm_error("comm %s: is 9 bit little endian ;-s", b->comm->name);
		return READ_ERROR;
	default:
		comm_error("comm %s: has some exotic byte order ;-|", b->comm->name);
		return READ_ERROR;
	}

	comm_info("comm %s: protocol version %llu", b->comm->name, ((MCPptr_t *)(b->comm_buf))[1]);
	b->completed = NULL;
	return READ_FREE;
}

static inline int mcp_check_size_and_read(struct comm_buffer_s **buf)
{
	fd_set rd;
	int r;

	if ((*buf)->want > (*buf)->size && (*buf)->p_comm_buf == (*buf)->comm_buf) {
		struct comm_buffer_s *b;

		b = comm_buf_resize(*buf, (*buf)->want);
		if (unlikely(!b)) {
			fatal(Out_of_memory);
			return -1;
		}
		*buf = b;
	}

	while ((*buf)->len < (*buf)->want) {
		FD_ZERO(&rd);
		FD_SET((*buf)->comm->fd, &rd);
		r = select((*buf)->comm->fd + 1, &rd, NULL, NULL, NULL);
		if (unlikely(r == -1)) {
			fprintf(stderr, "%s: select error %d", __func__, errno);
			return -1;
		}
		if (FD_ISSET((*buf)->comm->fd, &rd))
			r = read((*buf)->comm->fd,
				 (*buf)->p_comm_buf + (*buf)->len,
				 (*buf)->want - (*buf)->len);
		else
			continue;
		if (unlikely(r <= 0)) {
			comm_error("medusa comm %s: Read error or EOF (%d)",
				   (*buf)->comm->name, errno);
			return -1;
		}
		(*buf)->len += r;
	}
	return 0;
}

static inline int mcp_read_loop(struct comm_buffer_s **buf)
{
	enum read_result result;

	while ((*buf)->completed) {
		if (unlikely(mcp_check_size_and_read(buf) < 0))
			goto error;
		result = (*buf)->completed(*buf);
		if (unlikely(result < READ_DONE)) {
			goto error;
		} else if (result == READ_FREE) {
			(*buf)->free(*buf);
			return 0;
		}
	}
	return 0;
error:
	(*buf)->comm->close((*buf)->comm);
	(*buf)->free(*buf);
	return -1;
}

int mcp_receive_greeting(struct comm_s *c)
{
	struct comm_buffer_s *b;

	b = mcp_opened(c);
	return mcp_read_loop(&b);
}

/**
 * Last function in the function chain of completed functions *must* set the
 * completed attribute to NULL if it finishes successfully.
 */
int mcp_read_worker(struct comm_s *c)
{
	struct comm_buffer_s *buf;

	if (unlikely(tls_alloc_init()))
		return -1;

	while (1) {
		pthread_mutex_lock(&c->read_lock);
		buf = comm_buf_get(2048, c);
		buf->want = sizeof(uint32_t) + sizeof(MCPptr_t);
		buf->completed = mcp_r_head;
		if (unlikely(mcp_read_loop(&buf))) {
			comm_error("%s: read error", __func__);
			c->close(c);
			pthread_mutex_unlock(&c->read_lock);
			return -1;
		}
		pthread_mutex_unlock(&c->read_lock);
	}
}

/**
 *  Read header of the received buffer and determine number of bytes to be
 *  read in the next read call.
 */
static enum read_result mcp_r_head(struct comm_buffer_s *b)
{
	MCPptr_t x;

	x = ((MCPptr_t *)(b->comm_buf))[0];
	if (likely(x != 0)) {
		// This is a decision request
		b->event = (struct event_type_s *)hash_find(&b->comm->events, x);
		if (unlikely(!b->event)) {
			comm_error("comm %s: Unknown access type %p!", b->comm->name, x);
			return READ_ERROR;
		}
		b->want = sizeof(uint32_t) + b->len + (b->event->acctype.size);
		if (likely(b->event->op[0]))
			b->want += b->event->op[0]->m.size;
		if (b->event->op[1])
			b->want += b->event->op[1]->m.size;
		b->completed = mcp_r_query;

		return READ_DONE;
	}

	switch (byte_reorder_get_int32(b->comm->flags,
				       ((unsigned int *)(b->comm_buf + sizeof(MCPptr_t)))[0])) {
	case MEDUSA_COMM_CLASSDEF:
		b->want = b->len + sizeof(struct medusa_comm_class_s)
			  + sizeof(struct medusa_comm_attribute_s);
		b->completed = mcp_r_classdef_attr;
		break;
	case MEDUSA_COMM_ACCTYPEDEF:
		b->want = b->len + sizeof(struct medusa_comm_acctype_s)
			  + sizeof(struct medusa_comm_attribute_s);
		b->completed = mcp_r_acctypedef_attr;
		break;
	case MEDUSA_COMM_CLASSUNDEF:
	case MEDUSA_COMM_ACCTYPEUNDEF:
		runtime("Huh, I don't know how to undef class or acctype ;-|");
		b->want = b->len + sizeof(unsigned int);
		b->completed = mcp_r_discard;
		break;
	case MEDUSA_COMM_FETCH_ERROR:
	case MEDUSA_COMM_FETCH_ANSWER:
		b->want = b->len + 2 * sizeof(MCPptr_t); //+sizeof(unsigned int);
		b->completed = mcp_r_fetch_answer;
		break;
	case MEDUSA_COMM_UPDATE_ANSWER:
		b->want = b->len + 2 * sizeof(MCPptr_t) + sizeof(unsigned int);
		b->completed = mcp_r_update_answer;
		break;
	case MEDUSA_COMM_READY_REQUEST:
		/* only redirect to mcp_r_ready_request() */
		b->completed = mcp_r_ready_request;
		break;
	default:
		comm_error("comm %s: Communication protocol error! (%d)",
			   b->comm->name, ((unsigned int *)(b->comm_buf + sizeof(MCPptr_t)))[0]);
		return READ_ERROR;
	}

	return READ_DONE;
}

/**
 * Reads an authorization query from the kernel and saves it to a queue for
 * later processing.
 */
static enum read_result mcp_r_query(struct comm_buffer_s *b)
{
	get_event_context(b->comm, &b->context, b->event, b->comm_buf);
	b->ehh_list = EHH_VS_ALLOW;
	pthread_mutex_lock(&b->comm->state_lock);

	/*
	 * ParanoYa sanity check: `comm->init_buffer` is not allocated yet.
	 */
	if (unlikely(b->comm->state == 0)) {
		comm_error("Corrupted comm '%s' state: comm->init_buffer is not allocated yet!",
			   b->comm->name);
		pthread_mutex_unlock(&b->comm->state_lock);
		return READ_ERROR;
	}

	if (function_init && b->comm->init_buffer) {
		/* Enqueue incoming requests to be processed after _init() finishes. */
		comm_buf_to_queue(&b->comm->init_buffer->to_wake, b);
		/* `b->completed` should be set to NULL before `unlock(state_lock)`.
		 * After _init() finishes, comm_worker() calls `free(init_buffer)`,
		 * which inserts `b` into `comm_todo`. As free() is called with locked
		 * `state_lock`, it cannot be done until `state_lock` is not unlocked
		 * here. In this way is guaranteed that the state of `b` is properly
		 * initialized before scheduling it for execution by some worker.
		 */
		b->completed = NULL;
		pthread_mutex_unlock(&b->comm->state_lock);
		return READ_DONE;
	}

	/* In this case comes `unlock(state_lock)` before `b->completed = NULL`.
	 * Integrity of `b` is not damaged and the code path is more effective (in
	 * the terms of concurrency).
	 */
	pthread_mutex_unlock(&b->comm->state_lock);
	b->completed = NULL;
	/* After _init() was processed, new requests are always inserted here. */
	comm_buf_todo(b);
	return READ_DONE;
}

static int mcp_answer(struct comm_s *c, struct comm_buffer_s *b)
{
	int pid;
	struct medusa_attribute_s *apid;
	struct comm_buffer_s *r;
	#pragma pack(push)
	#pragma pack(1)
	struct out_struct {
		MCPptr_t ans, id;
		uint16_t res;
	} *out;
	#pragma pack(pop)
	int i;

	/* TODO: only if changed */

	if (b->context.result >= 0 && b->context.subject.class) {
		/*
		 * The value od `do_phase` can't be zero, because the function
		 * `mcp_answer()` is called only once from `comm_worker()` when
		 * `do_phase` >= 1000. So the following expression shouldn't be
		 * true.
		 */
		if (b->do_phase == 0)
			b->do_phase = 1000;
		i = c->update_object(c, b->do_phase - 1000, &b->context.subject, b);
		/*
		 * See documentation of update_object() in `struct comm_s`. Negative
		 * return values of update_object() are silently ignored (including
		 * FAILURE of update operation). Values greather than zero indicate
		 * unfinished operation (i.e. waiting for kernel response about success
		 * or failure of the update operation). Value 0 of the variable `i`
		 * means successfully finished update operation.
		 */
		if (i > 0) {
			b->do_phase = i + 1000;
			return i;
		}
		/* `b->do_phase` after finished update operation remains 1000 */
	}

	r = comm_buf_get(sizeof(*out), c);
	if (unlikely(!r)) {
		fatal("Can't alloc buffer for send answer!");
		return -1;
	}
	out = (void *)&r->comm_buf;
	out->ans = byte_reorder_put_int64(c->flags, MEDUSA_COMM_AUTHANSWER);
	out->id = ((MCPptr_t *)(b->comm_buf + sizeof(MCPptr_t)))[0];
#ifdef TRANSLATE_RESULT
	switch (b->context.result) {
	case RESULT_FORCE_ALLOW:
		out->res = MED_FORCE_ALLOW;
		break;
	case RESULT_DENY:
		out->res = MED_DENY;
		break;
	case RESULT_FAKE_ALLOW:
		out->res = MED_FAKE_ALLOW;
		break;
	case RESULT_ALLOW:
		out->res = MED_ALLOW;
		break;
	default:
		out->res = MED_ERR;
	}
#else
	out->res = b->context.result;
#endif
	out->res = byte_reorder_put_int16(c->flags, out->res);
	r->len = sizeof(*out);
	r->want = 0;
	r->completed = NULL;

	pid = -1;
	apid = get_attribute(b->context.subject.class, "pid");
	if (apid) {
		object_get_val(&b->context.subject, apid, &pid, sizeof(pid));
	} else {
		apid = get_attribute(b->context.operation.class, "pid");
		if (apid)
			object_get_val(&b->context.operation, apid, &pid, sizeof(pid));
	}
	printf("answer 0x%016lx %2d [%s:%d]\n",
	       out->id, (short)out->res, b->context.operation.class->m.name, pid);

	comm_buf_output_enqueue(c, r);

	/* `b->do_phase` after finished update operation remains 1000 */
	return 0;
}

static void unify_bitmap_types(struct medusa_comm_attribute_s *a)
{
	/*
	 * kernel bitmap is BITMAP_8 type, which is also default type BITMAP
	 */
	while (a->type != MED_COMM_TYPE_END) {
		switch (a->type) {
		case MED_COMM_TYPE_BITMAP_8:
		case MED_COMM_TYPE_BITMAP_16:
		case MED_COMM_TYPE_BITMAP_32:
			a->type = MED_COMM_TYPE_BITMAP;
		}
		a++;
	}
}

#define OFF_ATTR (sizeof(MCPptr_t) + sizeof(uint32_t))
#define OFF_ATTR_CLASS (OFF_ATTR + sizeof(struct medusa_comm_class_s))
#define OFF_ATTR_ACCTYPE (OFF_ATTR + sizeof(struct medusa_comm_acctype_s))

static enum read_result mcp_r_classdef_attr(struct comm_buffer_s *b)
{
	struct class_s *cl;
	char *last_attr = b->comm_buf + b->len - sizeof(struct medusa_comm_attribute_s);

	if (((struct medusa_comm_attribute_s *)(last_attr))->type != MED_TYPE_END) {
		b->want = b->len + sizeof(struct medusa_comm_attribute_s);
		return READ_DONE;
	}
	byte_reorder_class(b->comm->flags, (struct medusa_class_s *)(b->comm_buf + OFF_ATTR));
	byte_reorder_attrs(b->comm->flags,
			   (struct medusa_attribute_s *)(b->comm_buf + OFF_ATTR_CLASS));
	unify_bitmap_types((struct medusa_attribute_s *)(b->comm_buf + OFF_ATTR_CLASS));
	cl = add_class(b->comm,
		       (struct medusa_class_s *)(b->comm_buf + OFF_ATTR),
		       (struct medusa_attribute_s *)(b->comm_buf + OFF_ATTR_CLASS));
	if (unlikely(!cl))
		comm_error("comm %s: Can't add class", b->comm->name);
	b->completed = NULL;
	return READ_FREE;
}

static enum read_result mcp_r_acctypedef_attr(struct comm_buffer_s *b)
{
	struct event_type_s *ev;
	char *last_attr = b->comm_buf + b->len - sizeof(struct medusa_comm_attribute_s);

	if (((struct medusa_comm_attribute_s *)(last_attr))->type != MED_TYPE_END) {
		b->want = b->len + sizeof(struct medusa_comm_attribute_s);
		return READ_DONE;
	}
	byte_reorder_acctype(b->comm->flags, (struct medusa_acctype_s *)(b->comm_buf + OFF_ATTR));
	byte_reorder_attrs(b->comm->flags,
			   (struct medusa_attribute_s *)(b->comm_buf + OFF_ATTR_ACCTYPE));
	unify_bitmap_types((struct medusa_attribute_s *)(b->comm_buf + OFF_ATTR_ACCTYPE));
	ev = event_type_add(b->comm,
			    (struct medusa_acctype_s *)(b->comm_buf + OFF_ATTR),
			    (struct medusa_attribute_s *)(b->comm_buf + OFF_ATTR_ACCTYPE));
	if (unlikely(!ev))
		comm_error("comm %s: Can't add acctype", b->comm->name);
	b->completed = NULL;
	return READ_FREE;
}

#undef OFF_ATTR
#undef OFF_ATTR_CLASS
#undef OFF_ATTR_ACCTYPE

static enum read_result mcp_r_discard(struct comm_buffer_s *b)
{
	b->completed = NULL;
	return READ_FREE;
}

int mcp_ready_answer(struct comm_s *c)
{
	struct comm_buffer_s *r;
	MCPptr_t *out;

	r = comm_buf_get(sizeof(MCPptr_t), c);
	if (unlikely(!r)) {
		fatal("Can't alloc buffer for COMM_READY answer!");
		return -1;
	}
	out = (void *)&r->comm_buf;
	*out = byte_reorder_put_int64(c->flags, MEDUSA_COMM_READY_ANSWER);
	r->len = sizeof(*out);
	r->want = 0;
	r->completed = NULL;

	comm_buf_output_enqueue(c, r);
	return 0;
}

static enum read_result mcp_r_ready_request(struct comm_buffer_s *b)
{
	b->completed = NULL;

	/* initialize comm and allocate a buffer for _init(), if defined in med config file */
	if (comm_conn_init(b->comm) < 0)
		return READ_ERROR;

	/*
	 * If _init() is not defined, initialization is complete, send READY_ANSWER to kernel.
	 * If it's defined, READY_ANSWER is send after _init() finishes, from comm_buf_free()
	 * when the _init()'s buffer will be destroyed
	 */
	if (!function_init && mcp_ready_answer(b->comm) < 0) {
		fatal("%s: MEDUSA_COMM_READY_ANSWER not send to the kernel", __func__);
		return READ_ERROR;
	}

	return READ_FREE;
}

/*
 * Each comm has its own thread performing `mcp_write` function.
 * A buffer is inserted into output queue of the comm interface `c`
 * from four operations:
 * 1) answer (at the end of the processing of the kernel request)
 * 2) update (when updating an object on the kernel site)
 * 3) fetch (when requesting an object from the kernel site)
 * 4) ready answer (an answer to ready query from the kernel site after initialization)
 *
 * Using of this function requires that the buffers in the output queue are
 * not used from another threads of Constable. Why? This function always
 * destroyes the processed buffers.
 */
static int mcp_write(struct comm_s *c)
{
	int r;
	struct comm_buffer_s *b;

	b = comm_buf_output_dequeue(c);

	/* Ignore buffers incoming from another comm interfaces. */
	if (unlikely(b->open_counter != c->open_counter)) {
		b->free(b);
		return 1;
	}

	while (b->want < b->len) {
		r = write(c->fd, b->p_comm_buf + b->want, b->len - b->want);
		if (unlikely(r <= 0)) {
			comm_error("medusa comm %s: Write error %d", c->name, r);
			return r;
		}
		b->want += r;
		if (unlikely(b->want < b->len))
			comm_info("medusa comm %s: Non-atomic write", c->name);
	}

	b->free(b);
	return 1;
}

static int mcp_nowrite(struct comm_s *c)
{
	struct comm_buffer_s *b;

	while ((b = comm_buf_from_queue_locked(&c->output)) != NULL) {
		b->free(b);
		return 0;
	}
	return 0;
}

static int mcp_close(struct comm_s *c)
{
	struct comm_buffer_s *b;

	close(c->fd);
	c->fd = -1;
	pthread_mutex_lock(&c->output.lock);
	while ((b = comm_buf_from_queue(&c->output)) != NULL)
		b->free(b);
	pthread_mutex_unlock(&c->output.lock);
	pthread_mutex_lock(&c->wait_for_answer.lock);
	while ((b = comm_buf_from_queue(&c->wait_for_answer)) != NULL)
		b->free(b);
	pthread_mutex_unlock(&c->wait_for_answer.lock);
	c->open_counter--;
	return 0;
}

static int mcp_fetch_object(struct comm_s *c,
			    int cont,
			    struct object_s *o,
			    struct comm_buffer_s *wake)
{
	static MCPptr_t id = 2;
	static pthread_mutex_t id_lock = PTHREAD_MUTEX_INITIALIZER;
	struct comm_buffer_s *r;

	if (cont == 3) {
		if (unlikely(debug_do_out)) {
			pthread_mutex_lock(&debug_do_lock);
			debug_do_out(debug_do_arg, "fetch ");
			object_print(o, debug_do_out, debug_do_arg);
			pthread_mutex_unlock(&debug_do_lock);
		}
		return wake->user_data;	/* done */
	}

	wake->user_data = -1;
	r = comm_buf_get(3 * sizeof(MCPptr_t) + o->class->m.size, c);
	if (unlikely(!r)) {
		fatal("Can't alloc buffer for fetch!");
		return -1;
	}
	wake->user1 = (void *)o;
	object_set_byte_order(o, c->flags);
	((MCPptr_t *)(r->comm_buf))[0] = byte_reorder_put_int32(c->flags,
								MEDUSA_COMM_FETCH_REQUEST);
	((MCPptr_t *)(r->comm_buf))[1] = o->class->m.classid;
	pthread_mutex_lock(&id_lock);
	((MCPptr_t *)(r->comm_buf))[2] = id++;
	pthread_mutex_unlock(&id_lock);
	memcpy(((MCPptr_t *)(r->comm_buf)) + 3, o->data, o->class->m.size);
	r->len = 3 * sizeof(MCPptr_t) + o->class->m.size;
	r->want = 0;
	r->completed = NULL;

	/*
	 * Enqueue buffer `wake` to the queue of buffers waiting for an answer from the
	 * kernel. After the answer is received, the buffer is removed from the
	 * queue in mcp_r_fetch_answer().
	 */
	wake->waiting.to = MEDUSA_COMM_FETCH_REQUEST;
	wake->waiting.cid = ((MCPptr_t *)(r->comm_buf))[1];
	wake->waiting.seq = ((MCPptr_t *)(r->comm_buf))[2];
	comm_buf_to_queue_locked(&wake->comm->wait_for_answer, wake);
	/*
	 * Send buffer `r` to the output. Buffer is destroyed in the `mcp_write`
	 * function.
	 */
	comm_buf_output_enqueue(c, r);
	return 3;
}

static enum read_result mcp_r_fetch_answer(struct comm_buffer_s *b)
{
	struct comm_buffer_s *p = NULL;
	struct queue_item_s *prev = NULL, *item;
	#pragma pack(push)
	#pragma pack(1)
	struct {
		MCPptr_t cid, seq;
	} *bmask = (void *)(b->comm_buf + sizeof(uint32_t) + sizeof(MCPptr_t));
	#pragma pack(pop)

	pthread_mutex_lock(&b->comm->wait_for_answer.lock);
	item = b->comm->wait_for_answer.first;
	while (item) {
		if (item->buffer->waiting.to == MEDUSA_COMM_FETCH_REQUEST &&
		    item->buffer->waiting.cid == bmask->cid &&
		    item->buffer->waiting.seq == bmask->seq) {
			p = comm_buf_del(&b->comm->wait_for_answer, prev, item);
			break;
		}
		prev = item;
		item = item->next;
	}
	pthread_mutex_unlock(&b->comm->wait_for_answer.lock);

	if (byte_reorder_get_int32(b->comm->flags,
				   ((unsigned int *)(b->comm_buf + sizeof(MCPptr_t)))[0])
				    == MEDUSA_COMM_FETCH_ERROR) {
		/* the return value is preset to -1 (p->user_data) in `mcp_fetch_object()` */
		if (likely(p))
			comm_buf_todo(p);
		b->completed = NULL;
		return READ_FREE;
	}

	if (likely(p)) {
		b->len = 0;
		b->want = ((struct object_s *)(p->user1))->class->m.size;
		b->p_comm_buf = ((struct object_s *)(p->user1))->data;
		b->user1 = (void *)p;
		b->completed = mcp_r_fetch_answer_done;
	} else {
		/*
		 * Arrived answer to FETCH request, but there is nothing waiting for it;
		 * there is necessary to read arrived k-object and discard it.
		 */
		struct class_s *cl;
		char *errmsg = "Arrived answer to FETCH request, nobody is waiting for it!";

		cl = (struct class_s *)hash_find(&b->comm->classes, bmask->cid);
		if (unlikely(!cl)) {
			fatal("comm %s: Can't find class by class id. %s", b->comm->name, errmsg);
			return READ_ERROR;
		}
		fatal("comm %s: %s", b->comm->name, errmsg);
		b->want = b->len + cl->m.size;
		b->completed = mcp_r_discard;
	}
	return READ_DONE;
}

static enum read_result mcp_r_fetch_answer_done(struct comm_buffer_s *b)
{
	struct comm_buffer_s *p;
	/*
	 * `p` cannot be NULL, function is called only once from
	 * `mcp_r_fetch_answer()` if `p` != NULL.
	 */
	p = (struct comm_buffer_s *)(b->user1);
	p->user_data = 0;	/* success */
	p->waiting.to = 0;
	p->user1 = NULL;
	comm_buf_todo(p);

	b->completed = NULL;
	return READ_FREE;
}

static int mcp_update_object(struct comm_s *c,
			     int cont,
			     struct object_s *o,
			     struct comm_buffer_s *wake)
{
	static MCPptr_t id = 2;
	static pthread_mutex_t id_lock = PTHREAD_MUTEX_INITIALIZER;
	struct comm_buffer_s *r;

	/*
	 * It's the second pass of this function. Result of the `update` operation
	 * is available from the kernel. If successful, mcp_update_object() returns
	 * 0, -1 otherwise.
	 */
	if (cont == 3) {
#ifdef TRANSLATE_RESULT
		if (wake->user_data == MED_ALLOW)
#else
		if (wake->user_data == RESULT_ALLOW)
#endif
			return 0;	/* done */
		return -1;	/* done */
	}

	/*
	 * It's the first pass of this function. Moving on...
	 */

#ifdef TRANSLATE_RESULT
	wake->user_data = MED_ERR;
#else
	wake->user_data = RESULT_ERR;
#endif
	r = comm_buf_get(3 * sizeof(MCPptr_t) + ((struct object_s *)((void *)o))->class->m.size, c);
	if (unlikely(!r)) {
		fatal("Can't alloc buffer for update!");
		return -1;
	}

	if (unlikely(debug_do_out)) {
		pthread_mutex_lock(&debug_do_lock);
		debug_do_out(debug_do_arg, "update ");
		object_print(o, debug_do_out, debug_do_arg);
		pthread_mutex_unlock(&debug_do_lock);
	}

	((MCPptr_t *)(r->comm_buf))[0] = byte_reorder_put_int32(c->flags,
								MEDUSA_COMM_UPDATE_REQUEST);
	((MCPptr_t *)(r->comm_buf))[1] = o->class->m.classid;
	pthread_mutex_lock(&id_lock);
	((MCPptr_t *)(r->comm_buf))[2] = id++;
	pthread_mutex_unlock(&id_lock);
	memcpy(((MCPptr_t *)(r->comm_buf)) + 3, o->data, o->class->m.size);

	object_set_byte_order(o, r->comm->flags);
	r->len = 3 * sizeof(MCPptr_t) + o->class->m.size;
	r->want = 0;
	r->completed = NULL;

	/*
	 * Enqueue buffer `wake` to the queue of buffers waiting for an answer from the
	 * kernel. After the answer is received, the buffer is removed from the
	 * queue in mcp_r_update_answer().
	 */
	wake->waiting.to = MEDUSA_COMM_UPDATE_ANSWER;
	wake->waiting.cid = ((MCPptr_t *)(r->comm_buf))[1];
	wake->waiting.seq = ((MCPptr_t *)(r->comm_buf))[2];
	comm_buf_to_queue_locked(&wake->comm->wait_for_answer, wake);
	/*
	 * Send buffer `r` to the output. Buffer is destroyed in the `mcp_write`
	 * function.
	 */
	comm_buf_output_enqueue(c, r);
	return 3;
}

/**
 * Reads an update answer message from the kernel.
 *
 * \param b received buffer with the update answer message
 */
static enum read_result mcp_r_update_answer(struct comm_buffer_s *b)
{
	struct comm_buffer_s *p = NULL;
	struct queue_item_s *prev = NULL, *item;
	#pragma pack(push)
	#pragma pack(1)
	struct {
		MCPptr_t cid, seq;
		uint32_t update_result;
	} *bmask = (void *)(b->comm_buf + sizeof(uint32_t) + sizeof(MCPptr_t));
	#pragma pack(pop)

	pthread_mutex_lock(&b->comm->wait_for_answer.lock);
	item = b->comm->wait_for_answer.first;
	while (item) {
		if (item->buffer->waiting.to == MEDUSA_COMM_UPDATE_ANSWER &&
		    item->buffer->waiting.cid == bmask->cid &&
		    item->buffer->waiting.seq == bmask->seq) {
			p = comm_buf_del(&b->comm->wait_for_answer, prev, item);
			break;
		}
		prev = item;
		item = item->next;
	}
	pthread_mutex_unlock(&b->comm->wait_for_answer.lock);

	if (likely(p)) {
		p->user_data = byte_reorder_put_int32(b->comm->flags, bmask->update_result);
		p->waiting.to = 0;
		comm_buf_todo(p);
	} else {
		fatal("comm %s: Arrived answer to UPDATE request, nobody is waiting for it!",
		      b->comm->name);
	}

	b->completed = NULL;
	return READ_FREE;
}

int mcp_init(char *filename)
{
	return mcp_language_do(filename);
}

static int mcp_conf_error(struct comm_s *comm, const char *fmt, ...)
{
	va_list ap;
	char buf[4096];

	sprintf(buf, "comm %s: ", comm->name);
	va_start(ap, fmt);
	vsnprintf(buf + strlen(buf), 4000, fmt, ap);
	va_end(ap);
	sprintf(buf + strlen(buf), "\n");
	write(1, buf, strlen(buf));

	return -1;
}
