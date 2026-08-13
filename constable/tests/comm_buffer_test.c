// SPDX-License-Identifier: GPL-2.0

#include "comm.h"
#include "constable.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int comm_nr_connections;
struct event_handler_s *function_init;
char *Out_of_memory = "Out of memory";

static int failures;
static int checks;
static int ready_answers;
static struct event_handler_s init_handler;

#define EXPECT_TRUE(condition, description)					\
	do {									\
		checks++;							\
		if (!(condition)) {						\
			fprintf(stderr, "FAIL: %s\n", description);		\
			failures++;						\
		}								\
	} while (0)

int fatal(const char *format, ...)
{
	(void)format;
	return -1;
}

int runtime(const char *format, ...)
{
	(void)format;
	return -1;
}

int mcp_ready_answer(struct comm_s *comm)
{
	(void)comm;
	ready_answers++;
	return 0;
}

static void test_allocation_validation(void)
{
	struct comm_s comm = {
		.open_counter = 1,
	};

	EXPECT_TRUE(comm_buf_get(-1, &comm) == NULL,
		    "a negative communication-buffer size is rejected");
	EXPECT_TRUE(comm_buf_get(8, NULL) == NULL,
		    "a communication buffer requires an owner");
	EXPECT_TRUE(comm_buf_to_queue(NULL, (struct comm_buffer_s *)1) ==
		    -EINVAL,
		    "queue insertion rejects a NULL queue");
	EXPECT_TRUE(comm_buf_to_queue(&comm_todo, NULL) == -EINVAL,
		    "queue insertion rejects a NULL buffer");
}

static void test_resize_state_transfer(void)
{
	struct comm_s comm = {
		.open_counter = 7,
	};
	struct comm_buffer_s *buffer;
	struct comm_buffer_s *resized;
	struct comm_buffer_s *another;
	struct comm_buffer_s *waiting;
	struct stack_s *original_stack;
	int lock_result;
	int mutex_result;

	mutex_result = pthread_mutex_init(&comm.state_lock, NULL);
	EXPECT_TRUE(mutex_result == 0,
		    "the connection state mutex is initialized");
	if (mutex_result)
		return;

	buffer = comm_buf_get(16, &comm);
	EXPECT_TRUE(buffer != NULL, "a communication buffer is allocated");
	if (!buffer)
		return;

	memcpy(buffer->comm_buf, "12345678", 8);
	buffer->len = 8;
	buffer->want = 12;
	buffer->context.operation.next = &buffer->context.subject;
	buffer->context.subject.next = &buffer->context.object;
	buffer->context.operation.data = buffer->comm_buf + 2;
	buffer->context.subject.data = buffer->comm_buf + 4;
	buffer->context.object.data = buffer->comm_buf + 6;
	buffer->execute.c = &buffer->context;
	buffer->execute.my_comm_buff = buffer;
	original_stack = buffer->execute.stack;
	function_init = &init_handler;
	buffer->init_handler = function_init;
	comm.version = 4;
	comm.init_buffer = buffer;
	waiting = comm_buf_get(8, &comm);
	EXPECT_TRUE(waiting != NULL, "a waiting request buffer is allocated");
	if (!waiting) {
		buffer->bfree(buffer);
		pthread_mutex_destroy(&comm.state_lock);
		function_init = NULL;
		return;
	}
	EXPECT_TRUE(comm_buf_to_queue(&buffer->to_wake, waiting) == 0,
		    "a request can wait for initialization");

	resized = comm_buf_resize(buffer, 64);
	EXPECT_TRUE(resized != NULL && resized->size >= 64,
		    "a communication buffer grows to the requested capacity");
	if (!resized)
		return;
	EXPECT_TRUE(resized->len == 8 &&
		    !memcmp(resized->comm_buf, "12345678", 8),
		    "resize preserves the received bytes and logical length");
	EXPECT_TRUE(resized->p_comm_buf == resized->comm_buf,
		    "resize rebases the active communication pointer");
	EXPECT_TRUE(resized->context.operation.data ==
		    resized->comm_buf + 2 &&
		    resized->context.subject.data == resized->comm_buf + 4 &&
		    resized->context.object.data == resized->comm_buf + 6,
		    "resize rebases object views into the message");
	EXPECT_TRUE(resized->context.operation.next ==
		    &resized->context.subject &&
		    resized->context.subject.next == &resized->context.object,
		    "resize rebases the context's internal object chain");
	EXPECT_TRUE(resized->context.cb == resized &&
		    resized->execute.c == &resized->context &&
		    resized->execute.my_comm_buff == resized,
		    "resize rebases execution ownership pointers");
	EXPECT_TRUE(resized->execute.stack == original_stack,
		    "resize transfers the in-progress execution stack");
	EXPECT_TRUE(comm.init_buffer == resized,
		    "resize updates the connection's initialization owner");
	EXPECT_TRUE(comm_buf_peek_first(&resized->to_wake) == waiting &&
		    comm_buf_peek_last(&resized->to_wake) == waiting,
		    "resize transfers each initialization waiter exactly once");
	EXPECT_TRUE(ready_answers == 0,
		    "releasing the replaced allocation does not complete initialization");
	lock_result = pthread_mutex_trylock(&resized->lock);
	EXPECT_TRUE(lock_result == 0,
		    "the resized buffer owns a valid unlocked mutex");
	if (!lock_result)
		pthread_mutex_unlock(&resized->lock);

	another = comm_buf_get(8, &comm);
	EXPECT_TRUE(another != NULL, "a second communication buffer is allocated");
	if (another) {
		EXPECT_TRUE(another->execute.stack != resized->execute.stack,
			    "resized and replacement buffers do not alias stacks");
		another->bfree(another);
	}
	resized->bfree(resized);
	EXPECT_TRUE(ready_answers == 1,
		    "releasing the final initialization owner sends READY once");
	EXPECT_TRUE(comm_buf_from_queue_locked(&comm_todo) == waiting &&
		    comm_buf_from_queue_locked(&comm_todo) == NULL,
		    "the transferred waiter is scheduled exactly once");
	waiting->bfree(waiting);
	comm.init_buffer = NULL;
	function_init = NULL;
	pthread_mutex_destroy(&comm.state_lock);
}

static void test_resize_validation(void)
{
	struct comm_s comm = {
		.open_counter = 9,
	};
	struct comm_buffer_s *buffer;

	EXPECT_TRUE(pthread_mutex_init(&comm.state_lock, NULL) == 0,
		    "the validation connection mutex is initialized");
	buffer = comm_buf_get(16, &comm);

	EXPECT_TRUE(buffer != NULL, "validation buffer is allocated");
	if (!buffer)
		return;
	buffer->len = 8;
	EXPECT_TRUE(comm_buf_resize(buffer, 7) == NULL,
		    "resize cannot request less than the live message");
	EXPECT_TRUE(comm_buf_resize(buffer, -1) == NULL,
		    "a negative resize is rejected");
	buffer->len = 17;
	EXPECT_TRUE(comm_buf_resize(buffer, 32) == NULL,
		    "resize rejects a corrupt length/capacity invariant");
	buffer->len = 8;
	buffer->bfree(buffer);
	pthread_mutex_destroy(&comm.state_lock);
}

int main(void)
{
	if (buffers_init()) {
		fprintf(stderr, "FAIL: cannot initialize communication buffers\n");
		return 1;
	}

	test_allocation_validation();
	test_resize_state_transfer();
	test_resize_validation();

	if (failures) {
		fprintf(stderr, "communication buffers: %d failure(s)\n",
			failures);
		return 1;
	}
	printf("communication buffers: %d checks passed\n", checks);
	return 0;
}
