// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../comm.h"
#include "../constable.h"
#include "../event.h"
#include "../fallback_policy.h"
#include "../domain_rule.h"
#include "../approval.h"
#include "../medusa_object.h"
#include "../object.h"
#include "../string_utils.h"
#include "mcp.h"
#include "validate.h"

extern struct event_handler_s *function_init;

struct mcp_comm_s {
	uint64_t generation;
	uint64_t enabled_features;
	uint64_t next_request_id;
	pthread_mutex_t request_lock;
	struct v4_cancelled_request *cancelled;
	uint64_t replacement_generation;
	bool replacement_pending;
};

struct v4_cancelled_request {
	struct v4_cancelled_request *next;
	uint64_t request_id;
};

struct v4_tlv_view {
	uint16_t type;
	uint16_t flags;
	const uint8_t *value;
	size_t length;
};

struct v4_builder {
	struct comm_buffer_s *buffer;
	size_t capacity;
};

#define MCP_DATA(c) ((struct mcp_comm_s *)comm_user_data(c))

static uint16_t from_le16(uint16_t value)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	return __builtin_bswap16(value);
#else
	return value;
#endif
}

static uint32_t from_le32(uint32_t value)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	return __builtin_bswap32(value);
#else
	return value;
#endif
}

static uint64_t from_le64(uint64_t value)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	return __builtin_bswap64(value);
#else
	return value;
#endif
}

#define to_le16(value) from_le16(value)
#define to_le32(value) from_le32(value)
#define to_le64(value) from_le64(value)

static uint16_t load_le16(const void *source)
{
	uint16_t value;

	memcpy(&value, source, sizeof(value));
	return from_le16(value);
}

static uint32_t load_le32(const void *source)
{
	uint32_t value;

	memcpy(&value, source, sizeof(value));
	return from_le32(value);
}

static uint64_t load_le64(const void *source)
{
	uint64_t value;

	memcpy(&value, source, sizeof(value));
	return from_le64(value);
}

static void store_le16(void *destination, uint16_t value)
{
	value = to_le16(value);
	memcpy(destination, &value, sizeof(value));
}

static void store_le32(void *destination, uint32_t value)
{
	value = to_le32(value);
	memcpy(destination, &value, sizeof(value));
}

static void store_le64(void *destination, uint64_t value)
{
	value = to_le64(value);
	memcpy(destination, &value, sizeof(value));
}

static int v4_next_tlv(const uint8_t *frame, size_t length, size_t *offset,
		       struct v4_tlv_view *view)
{
	const struct medusa_tlv *tlv;
	size_t tlv_length;

	if (*offset == length)
		return 0;
	if (*offset < MEDUSA_FRAME_HEADER_SIZE ||
	    length - *offset < MEDUSA_TLV_HEADER_SIZE)
		return -EMSGSIZE;
	tlv = (const struct medusa_tlv *)(frame + *offset);
	tlv_length = load_le32(&tlv->length);
	view->type = load_le16(&tlv->type);
	view->flags = load_le16(&tlv->flags);
	view->value = (const uint8_t *)tlv + MEDUSA_TLV_HEADER_SIZE;
	view->length = tlv_length - MEDUSA_TLV_HEADER_SIZE;
	*offset += MEDUSA_TLV_ALIGN_UP(tlv_length);
	return 1;
}

static const uint8_t *v4_find_tlv(const uint8_t *frame, size_t length,
				  uint16_t type, size_t *value_length,
				  bool required)
{
	struct v4_tlv_view view;
	const uint8_t *found = NULL;
	size_t offset = MEDUSA_FRAME_HEADER_SIZE;
	int result;

	while ((result = v4_next_tlv(frame, length, &offset, &view)) > 0) {
		if (view.type != type)
			continue;
		if (found)
			return NULL;
		found = view.value;
		*value_length = view.length;
	}
	if (result < 0 || (!found && required))
		return NULL;
	return found;
}

static int v4_get_u8(const uint8_t *frame, size_t length, uint16_t type,
		     uint8_t *value)
{
	size_t value_length = 0;
	const uint8_t *wire =
		v4_find_tlv(frame, length, type, &value_length, true);

	if (!wire)
		return -ENOENT;
	if (value_length != sizeof(*value))
		return -EMSGSIZE;
	*value = *wire;
	return 0;
}

static int v4_get_u16(const uint8_t *frame, size_t length, uint16_t type,
		      uint16_t *value)
{
	size_t value_length = 0;
	const uint8_t *wire =
		v4_find_tlv(frame, length, type, &value_length, true);

	if (!wire)
		return -ENOENT;
	if (value_length != sizeof(*value))
		return -EMSGSIZE;
	*value = load_le16(wire);
	return 0;
}

static int v4_get_u32(const uint8_t *frame, size_t length, uint16_t type,
		      uint32_t *value)
{
	size_t value_length = 0;
	const uint8_t *wire =
		v4_find_tlv(frame, length, type, &value_length, true);

	if (!wire)
		return -ENOENT;
	if (value_length != sizeof(*value))
		return -EMSGSIZE;
	*value = load_le32(wire);
	return 0;
}

static int v4_get_u64(const uint8_t *frame, size_t length, uint16_t type,
		      uint64_t *value)
{
	size_t value_length = 0;
	const uint8_t *wire =
		v4_find_tlv(frame, length, type, &value_length, true);

	if (!wire)
		return -ENOENT;
	if (value_length != sizeof(*value))
		return -EMSGSIZE;
	*value = load_le64(wire);
	return 0;
}

static struct v4_builder v4_builder_new(struct comm_s *comm, uint16_t type,
					uint64_t request_id,
					uint64_t generation,
					size_t payload_capacity)
{
	struct v4_builder builder = { 0 };
	struct medusa_frame_header *header;
	size_t total;

	if (payload_capacity > MEDUSA_FRAME_MAX_PAYLOAD ||
	    payload_capacity > SIZE_MAX - MEDUSA_FRAME_HEADER_SIZE)
		return builder;
	total = MEDUSA_FRAME_HEADER_SIZE + payload_capacity;
	if (total > INT_MAX)
		return builder;
	builder.buffer = comm_buf_get((int)total, comm);
	if (!builder.buffer)
		return builder;
	memset(builder.buffer->comm_buf, 0, total);
	builder.buffer->len = MEDUSA_FRAME_HEADER_SIZE;
	builder.buffer->want = 0;
	builder.buffer->completed = NULL;
	builder.capacity = total;
	header = (struct medusa_frame_header *)builder.buffer->comm_buf;
	store_le16(&header->version, MEDUSA_PROTOCOL_VERSION);
	store_le16(&header->type, type);
	store_le64(&header->request_id, request_id);
	store_le64(&header->policy_generation, generation);
	return builder;
}

static int v4_builder_add(struct v4_builder *builder, uint16_t type,
			  uint16_t flags, const void *value,
			  size_t value_length)
{
	struct medusa_frame_header *header;
	struct medusa_tlv *tlv;
	size_t tlv_length;
	size_t aligned;

	if (!builder || !builder->buffer ||
	    value_length > SIZE_MAX - MEDUSA_TLV_HEADER_SIZE)
		return -EINVAL;
	tlv_length = MEDUSA_TLV_HEADER_SIZE + value_length;
	aligned = MEDUSA_TLV_ALIGN_UP(tlv_length);
	if (aligned < tlv_length ||
	    aligned > builder->capacity - (size_t)builder->buffer->len)
		return -EMSGSIZE;
	tlv = (struct medusa_tlv *)(builder->buffer->comm_buf +
				    builder->buffer->len);
	store_le16(&tlv->type, type);
	store_le16(&tlv->flags, flags);
	store_le32(&tlv->length, (uint32_t)tlv_length);
	if (value_length)
		memcpy((uint8_t *)tlv + MEDUSA_TLV_HEADER_SIZE, value,
		       value_length);
	builder->buffer->len += (int)aligned;
	header = (struct medusa_frame_header *)builder->buffer->comm_buf;
	store_le32(&header->payload_length,
		   (uint32_t)(builder->buffer->len -
			      (int)MEDUSA_FRAME_HEADER_SIZE));
	return 0;
}

static int v4_builder_add_u8(struct v4_builder *builder, uint16_t type,
			     uint8_t value)
{
	return v4_builder_add(builder, type, 0, &value, sizeof(value));
}

static int v4_builder_add_u16(struct v4_builder *builder, uint16_t type,
			      uint16_t value)
{
	uint16_t wire = to_le16(value);

	return v4_builder_add(builder, type, 0, &wire, sizeof(wire));
}

static int v4_builder_add_u32(struct v4_builder *builder, uint16_t type,
			      uint32_t value)
{
	uint32_t wire = to_le32(value);

	return v4_builder_add(builder, type, 0, &wire, sizeof(wire));
}

static int v4_builder_add_u64(struct v4_builder *builder, uint16_t type,
			      uint64_t value)
{
	uint64_t wire = to_le64(value);

	return v4_builder_add(builder, type, 0, &wire, sizeof(wire));
}

static int v4_write_buffer(struct comm_s *comm, struct comm_buffer_s *buffer)
{
	ssize_t written;

	written = write(comm->fd, buffer->comm_buf, (size_t)buffer->len);
	if (written != buffer->len) {
		if (written >= 0)
			errno = EIO;
		return -1;
	}
	return 0;
}

static int v4_send_now(struct comm_s *comm, struct v4_builder *builder)
{
	int result;

	if (!builder->buffer)
		return -1;
	result = v4_write_buffer(comm, builder->buffer);
	builder->buffer->bfree(builder->buffer);
	builder->buffer = NULL;
	return result;
}

static int v4_enqueue(struct comm_s *comm, struct v4_builder *builder)
{
	if (!builder->buffer)
		return -1;
	comm_buf_output_enqueue(comm, builder->buffer);
	builder->buffer = NULL;
	return 0;
}

static uint64_t mcp_next_request_id(struct comm_s *comm)
{
	uint64_t id;

	pthread_mutex_lock(&MCP_DATA(comm)->request_lock);
	id = ++MCP_DATA(comm)->next_request_id;
	if (!id)
		id = ++MCP_DATA(comm)->next_request_id;
	pthread_mutex_unlock(&MCP_DATA(comm)->request_lock);
	return id;
}

static int v4_cancel_add(struct comm_s *comm, uint64_t request_id)
{
	struct v4_cancelled_request *cancelled = malloc(sizeof(*cancelled));

	if (!cancelled)
		return -ENOMEM;
	cancelled->request_id = request_id;
	pthread_mutex_lock(&MCP_DATA(comm)->request_lock);
	cancelled->next = MCP_DATA(comm)->cancelled;
	MCP_DATA(comm)->cancelled = cancelled;
	pthread_mutex_unlock(&MCP_DATA(comm)->request_lock);
	return 0;
}

static bool v4_cancel_take(struct comm_s *comm, uint64_t request_id)
{
	struct v4_cancelled_request **link;
	struct v4_cancelled_request *cancelled = NULL;

	pthread_mutex_lock(&MCP_DATA(comm)->request_lock);
	for (link = &MCP_DATA(comm)->cancelled; *link; link = &(*link)->next) {
		if ((*link)->request_id != request_id)
			continue;
		cancelled = *link;
		*link = cancelled->next;
		break;
	}
	pthread_mutex_unlock(&MCP_DATA(comm)->request_lock);
	free(cancelled);
	return cancelled != NULL;
}

bool mcp_authrequest_cancelled(struct comm_buffer_s *request)
{
	struct v4_cancelled_request *cancelled;
	uint64_t request_id;
	bool found = false;

	if (!request || !request->comm ||
	    request->len < (int)(2 * sizeof(MCPptr_t)))
		return true;
	request_id = ((MCPptr_t *)request->comm_buf)[1];
	pthread_mutex_lock(&MCP_DATA(request->comm)->request_lock);
	for (cancelled = MCP_DATA(request->comm)->cancelled;
	     cancelled; cancelled = cancelled->next) {
		if (cancelled->request_id == request_id) {
			found = true;
			break;
		}
	}
	pthread_mutex_unlock(&MCP_DATA(request->comm)->request_lock);
	return found;
}

static void v4_cancel_clear(struct comm_s *comm)
{
	struct v4_cancelled_request *cancelled;

	pthread_mutex_lock(&MCP_DATA(comm)->request_lock);
	cancelled = MCP_DATA(comm)->cancelled;
	MCP_DATA(comm)->cancelled = NULL;
	pthread_mutex_unlock(&MCP_DATA(comm)->request_lock);
	while (cancelled) {
		struct v4_cancelled_request *next = cancelled->next;

		free(cancelled);
		cancelled = next;
	}
}

static int get_event_context(struct comm_s *comm,
			     struct event_context_s *context,
			     struct event_type_s *event, void *data)
{
	context->operation.next = &context->subject;
	context->operation.attr.offset = 0;
	context->operation.attr.length = event->acctype.size;
	context->operation.attr.type = MED_TYPE_END;
	if (string_copy_field(context->operation.attr.name,
			      sizeof(context->operation.attr.name),
			      event->acctype.name,
			      sizeof(event->acctype.name)))
		return -EPROTO;
	context->operation.flags = comm->flags;
	context->operation.class = event->operation_class;
	context->operation.data = (char *)data + 2 * sizeof(MCPptr_t);

	context->subject.next = &context->object;
	context->subject.attr.offset = 0;
	context->subject.attr.length = event->op[0] ? event->op[0]->m.size : 0;
	context->subject.attr.type = MED_TYPE_END;
	if (string_copy_field(context->subject.attr.name,
			      sizeof(context->subject.attr.name),
			      event->acctype.op_name[0],
			      sizeof(event->acctype.op_name[0])))
		return -EPROTO;
	context->subject.flags = comm->flags;
	context->subject.class = event->op[0];
	context->subject.data = (char *)data + 2 * sizeof(MCPptr_t) +
				event->acctype.size;

	context->object.next = NULL;
	context->object.attr.offset = 0;
	context->object.attr.length = event->op[1] ? event->op[1]->m.size : 0;
	context->object.attr.type = MED_TYPE_END;
	if (string_copy_field(context->object.attr.name,
			      sizeof(context->object.attr.name),
			      event->acctype.op_name[1],
			      sizeof(event->acctype.op_name[1])))
		return -EPROTO;
	context->object.flags = comm->flags;
	context->object.class = event->op[1];
	context->object.data = context->subject.data +
			       context->subject.attr.length;
	context->local_vars = NULL;
	return 0;
}

static uint8_t v4_internal_attr_type(uint16_t type, uint16_t flags)
{
	uint8_t internal = (uint8_t)type;

	if (flags & MEDUSA_ATTR_F_READ_ONLY)
		internal |= MED_TYPE_READ_ONLY;
	if (flags & MEDUSA_ATTR_F_PRIMARY_KEY)
		internal |= MED_TYPE_PRIMARY_KEY;
	if (flags & MEDUSA_ATTR_F_BIG_ENDIAN)
		internal |= MED_TYPE_BIG_ENDIAN;
	if (flags & MEDUSA_ATTR_F_LITTLE_ENDIAN)
		internal |= MED_TYPE_LITTLE_ENDIAN;
	return internal;
}

static struct medusa_attribute_s *
v4_parse_attributes(const uint8_t *frame, size_t length, uint32_t object_size)
{
	struct medusa_attribute_s *attributes;
	struct v4_tlv_view view;
	size_t count = 0;
	size_t index = 0;
	size_t offset = MEDUSA_FRAME_HEADER_SIZE;
	int result;

	while ((result = v4_next_tlv(frame, length, &offset, &view)) > 0)
		if (view.type == MEDUSA_TLV_ATTRIBUTE)
			count++;
	if (result < 0 || count > MCP_DEFINITION_ATTRIBUTE_LIMIT)
		return NULL;
	attributes = calloc(count + 1, sizeof(*attributes));
	if (!attributes)
		return NULL;
	offset = MEDUSA_FRAME_HEADER_SIZE;
	while ((result = v4_next_tlv(frame, length, &offset, &view)) > 0) {
		const struct medusa_attribute_definition *definition;
		uint32_t attr_offset;
		uint32_t attr_length;
		uint16_t name_length;
		uint16_t type;
		uint16_t flags;

		if (view.type != MEDUSA_TLV_ATTRIBUTE)
			continue;
		if (view.length < sizeof(*definition))
			goto invalid;
		definition =
			(const struct medusa_attribute_definition *)view.value;
		attr_offset = load_le32(&definition->offset);
		attr_length = load_le32(&definition->length);
		name_length = load_le16(&definition->name_length);
		type = load_le16(&definition->type);
		flags = load_le16(&definition->flags);
		if (!attr_length || attr_offset > UINT16_MAX ||
		    attr_length > UINT16_MAX ||
		    attr_offset > object_size ||
		    attr_length > object_size - attr_offset ||
		    name_length == 0 ||
		    name_length >= sizeof(attributes[index].name) ||
		    view.length != sizeof(*definition) + name_length ||
		    type < MEDUSA_ATTR_UNSIGNED || type > MEDUSA_ATTR_BYTES)
			goto invalid;
		attributes[index].offset = (uint16_t)attr_offset;
		attributes[index].length = (uint16_t)attr_length;
		attributes[index].type = v4_internal_attr_type(type, flags);
		memcpy(attributes[index].name,
		       view.value + sizeof(*definition), name_length);
		attributes[index].name[name_length] = '\0';
		index++;
	}
	return attributes;
invalid:
	free(attributes);
	return NULL;
}

static int v4_copy_name(const uint8_t *frame, size_t length, uint16_t type,
			char *destination, size_t capacity, bool allow_empty)
{
	size_t name_length = 0;
	const uint8_t *name =
		v4_find_tlv(frame, length, type, &name_length, true);

	if (!name || name_length >= capacity ||
	    (!allow_empty && name_length == 0) ||
	    memchr(name, '\0', name_length))
		return -EINVAL;
	memcpy(destination, name, name_length);
	destination[name_length] = '\0';
	return 0;
}

static int v4_handle_class_definition(struct comm_s *comm,
				      const uint8_t *frame, size_t length)
{
	struct medusa_attribute_s *attributes;
	struct medusa_class_s definition = { 0 };
	uint32_t class_id;
	uint32_t object_size;
	int error;

	error = v4_get_u32(frame, length, MEDUSA_TLV_CLASS_ID, &class_id);
	if (!error)
		error = v4_get_u32(
			frame, length, MEDUSA_TLV_OBJECT_SIZE, &object_size);
	if (!error)
		error = v4_copy_name(
			frame, length, MEDUSA_TLV_NAME, definition.name,
			sizeof(definition.name), false);
	if (error || !class_id || object_size > UINT16_MAX)
		return -EPROTO;
	attributes = v4_parse_attributes(frame, length, object_size);
	if (!attributes)
		return -EPROTO;
	definition.classid = class_id;
	definition.size = (uint16_t)object_size;
	if (!add_class(comm, &definition, attributes))
		error = -ENOMEM;
	free(attributes);
	return error;
}

static int v4_handle_event_definition(struct comm_s *comm,
				      const uint8_t *frame, size_t length)
{
	struct medusa_attribute_s *attributes;
	struct medusa_acctype_s definition = { 0 };
	uint32_t event_id;
	uint32_t event_size;
	uint32_t subject_class;
	uint32_t object_class;
	uint16_t trigger;
	uint8_t kind;
	int error;

	error = v4_get_u32(frame, length, MEDUSA_TLV_EVENT_ID, &event_id);
	if (!error)
		error = v4_get_u32(
			frame, length, MEDUSA_TLV_EVENT_SIZE, &event_size);
	if (!error)
		error = v4_get_u32(
			frame, length, MEDUSA_TLV_SUBJECT_CLASS_ID,
			&subject_class);
	if (!error)
		error = v4_get_u32(
			frame, length, MEDUSA_TLV_OBJECT_CLASS_ID,
			&object_class);
	if (!error)
		error = v4_get_u16(
			frame, length, MEDUSA_TLV_TRIGGER, &trigger);
	if (!error)
		error = v4_get_u8(
			frame, length, MEDUSA_TLV_EVENT_KIND, &kind);
	if (!error)
		error = v4_copy_name(
			frame, length, MEDUSA_TLV_NAME, definition.name,
			sizeof(definition.name), false);
	if (!error)
		error = v4_copy_name(
			frame, length, MEDUSA_TLV_SUBJECT_NAME,
			definition.op_name[0], sizeof(definition.op_name[0]),
			true);
	if (!error)
		error = v4_copy_name(
			frame, length, MEDUSA_TLV_OBJECT_NAME,
			definition.op_name[1], sizeof(definition.op_name[1]),
			true);
	if (error || !mcp_validate_event_kind(kind) || !event_id ||
	    !subject_class || !object_class ||
	    event_size > UINT16_MAX)
		return -EPROTO;
	attributes = v4_parse_attributes(frame, length, event_size);
	if (!attributes)
		return -EPROTO;
	definition.opid = event_id;
	definition.size = (uint16_t)event_size;
	definition.actbit = trigger;
	definition.kind = kind;
	definition.op_class[0] = subject_class;
	definition.op_class[1] = object_class;
	if (!event_type_add(comm, &definition, attributes))
		error = -ENOMEM;
	free(attributes);
	return error;
}

static int v4_configured_events_announced(struct comm_s *comm)
{
	unsigned int index;

	for (index = 0; index < fallback_policy_count(); index++) {
		const struct fallback_policy_config *policy =
			fallback_policy_at(index);
		struct event_names_s *name =
			event_type_find_name((char *)policy->event, false);

		if (!name || !name->events[comm->conn]) {
			comm_error("comm %s: fallback event '%s' was not announced",
				   comm->name, policy->event);
			return -ENOENT;
		}
		if (name->events[comm->conn]->acctype.kind ==
		    MEDUSA_EVENT_OBJECT_NOTIFICATION) {
			comm_error("comm %s: fallback policy is not valid for object-notification event '%s'",
				   comm->name, policy->event);
			return -EOPNOTSUPP;
		}
	}
	for (index = 0; index < domain_rule_count(); index++) {
		const struct domain_rule_config *rule = domain_rule_at(index);
		struct event_names_s *name =
			event_type_find_name((char *)rule->event, false);

		if (!name || !name->events[comm->conn]) {
			comm_error("comm %s: domain-rule event '%s' was not announced",
				   comm->name, rule->event);
			return -ENOENT;
		}
		if (name->events[comm->conn]->acctype.kind ==
		    MEDUSA_EVENT_OBJECT_NOTIFICATION) {
			comm_error("comm %s: domain decision rule is not valid for object-notification event '%s'",
				   comm->name, rule->event);
			return -EOPNOTSUPP;
		}
	}
	if (domain_rule_count() &&
	    !(MCP_DATA(comm)->enabled_features &
	      MEDUSA_FEATURE_DOMAIN_DECISION_CACHE)) {
		comm_error("comm %s: kernel lacks domain decision cache support",
			   comm->name);
		return -EOPNOTSUPP;
	}
	return 0;
}

struct v4_policy_context {
	struct comm_s *comm;
	uint64_t generation;
	int error;
};

static int v4_send_event_policy(const struct event_names_s *name,
				void *argument)
{
	struct v4_policy_context *context = argument;
	struct event_type_s *event;
	struct v4_builder builder;
	uint8_t policy;
	unsigned int rule_count;
	unsigned int index;

	if (context->error)
		return context->error;
	event = name->events[context->comm->conn];
	if (!event)
		return 0;
	policy = fallback_policy_for_event(name->name);
	rule_count = domain_rule_count_for_event(name->name);
	builder = v4_builder_new(
		context->comm, MEDUSA_MSG_POLICY_EVENT, 0,
		context->generation, 32 + rule_count * 40);
	if (!builder.buffer ||
	    v4_builder_add_u32(
		    &builder, MEDUSA_TLV_EVENT_ID,
		    (uint32_t)event->acctype.opid) ||
	    v4_builder_add_u8(
		    &builder, MEDUSA_TLV_FALLBACK_POLICY, policy)) {
		context->error = -EIO;
		goto out;
	}
	for (index = 0; index < domain_rule_count(); index++) {
		const struct domain_rule_config *rule = domain_rule_at(index);
		struct medusa_domain_rule wire = { 0 };

		if (strcmp(rule->event, name->name))
			continue;
		store_le64(&wire.subject_domain, rule->subject_domain);
		store_le64(&wire.object_domain, rule->object_domain);
		store_le64(&wire.selector, rule->selector);
		wire.answer = rule->answer;
		if (v4_builder_add(&builder, MEDUSA_TLV_DOMAIN_RULE,
				   MEDUSA_TLV_F_ARRAY, &wire, sizeof(wire))) {
			context->error = -EIO;
			goto out;
		}
	}
	if (v4_send_now(context->comm, &builder)) {
		context->error = -EIO;
out:
		if (builder.buffer)
			builder.buffer->bfree(builder.buffer);
	}
	return context->error;
}

static int v4_install_policy_generation(struct comm_s *comm,
					uint64_t generation)
{
	struct v4_policy_context context = {
		.comm = comm,
		.generation = generation,
	};
	struct v4_builder builder;

	/*
	 * The initial generation binds compiled policy handlers to the announced
	 * classes and events.  A live replacement reuses that immutable binding;
	 * rebuilding it would allocate duplicate per-tree communication metadata
	 * and could not be published atomically.
	 */
	if (generation == MCP_DATA(comm)->generation) {
		if (v4_configured_events_announced(comm))
			return -1;
		if (comm_conn_init(comm, true) < 0)
			return -1;
	}
	builder = v4_builder_new(
		comm, MEDUSA_MSG_POLICY_BEGIN, 0,
		generation, 0);
	if (v4_send_now(comm, &builder))
		return -1;
	if (event_names_visit(v4_send_event_policy, &context) ||
	    context.error)
		return -1;
	builder = v4_builder_new(
		comm, MEDUSA_MSG_POLICY_COMMIT, 0,
		generation, 0);
	return v4_send_now(comm, &builder);
}

static int v4_install_policy(struct comm_s *comm)
{
	return v4_install_policy_generation(
		comm, MCP_DATA(comm)->generation);
}

static int v4_send_hello(struct comm_s *comm)
{
	struct v4_builder builder =
		v4_builder_new(comm, MEDUSA_MSG_HELLO, 0, 0, 64);
	uint64_t optional = MEDUSA_FEATURE_DECISION_PROGRESS |
			    MEDUSA_FEATURE_OBJECT_FETCH_UPDATE |
			    MEDUSA_FEATURE_ATOMIC_POLICY_REPLACE |
			    MEDUSA_FEATURE_REPLY_CACHE_UPDATE |
			    MEDUSA_FEATURE_DOMAIN_DECISION_CACHE;

	if (!builder.buffer ||
	    v4_builder_add_u16(
		    &builder, MEDUSA_TLV_MIN_VERSION, MEDUSA_PROTOCOL_VERSION) ||
	    v4_builder_add_u16(
		    &builder, MEDUSA_TLV_MAX_VERSION, MEDUSA_PROTOCOL_VERSION) ||
	    v4_builder_add_u64(
		    &builder, MEDUSA_TLV_REQUIRED_FEATURES,
		    MEDUSA_REQUIRED_FEATURES) ||
	    v4_builder_add_u64(
		    &builder, MEDUSA_TLV_OPTIONAL_FEATURES, optional)) {
		if (builder.buffer)
			builder.buffer->bfree(builder.buffer);
		return -1;
	}
	return v4_send_now(comm, &builder);
}

static ssize_t v4_read_frame(struct comm_s *comm, uint8_t *frame)
{
	ssize_t length;

	do {
		length = read(comm->fd, frame, MEDUSA_FRAME_MAX_SIZE);
	} while (length < 0 && errno == EINTR);
	if (length <= 0)
		return -1;
	if (mcp_validate_v4_frame(frame, (size_t)length)) {
		errno = EPROTO;
		return -1;
	}
	return length;
}

static int mcp_receive_handshake(struct comm_s *comm)
{
	uint8_t *frame;
	bool definitions_done = false;
	bool hello_done = false;
	int result = -1;

	class_free_all_clases(comm);
	event_free_all_events(comm);
	comm->open_counter++;
	comm->flags = 0;
	comm->version = MEDUSA_PROTOCOL_VERSION;
	frame = malloc(MEDUSA_FRAME_MAX_SIZE);
	if (!frame)
		return -1;
	if (v4_send_hello(comm))
		goto out;
	for (;;) {
		const struct medusa_frame_header *header;
		ssize_t length = v4_read_frame(comm, frame);
		uint16_t type;

		if (length < 0)
			goto out;
		header = (const struct medusa_frame_header *)frame;
		type = load_le16(&header->type);
		if (type == MEDUSA_MSG_HELLO_ACK && !hello_done) {
			uint64_t enabled;

			if (v4_get_u64(
				    frame, (size_t)length,
				    MEDUSA_TLV_ENABLED_FEATURES, &enabled) ||
			    (enabled & MEDUSA_REQUIRED_FEATURES) !=
				    MEDUSA_REQUIRED_FEATURES)
				goto out;
			if (approval_is_configured() &&
			    !(enabled & MEDUSA_FEATURE_DECISION_PROGRESS))
				goto out;
			MCP_DATA(comm)->generation =
				load_le64(&header->policy_generation);
			MCP_DATA(comm)->enabled_features = enabled;
			hello_done = true;
		} else if (type == MEDUSA_MSG_CLASS_DEFINITION &&
			   hello_done && !definitions_done) {
			if (v4_handle_class_definition(
				    comm, frame, (size_t)length))
				goto out;
		} else if (type == MEDUSA_MSG_EVENT_DEFINITION &&
			   hello_done && !definitions_done) {
			if (v4_handle_event_definition(
				    comm, frame, (size_t)length))
				goto out;
		} else if (type == MEDUSA_MSG_DEFINITIONS_DONE &&
			   hello_done && !definitions_done) {
			definitions_done = true;
			if (v4_install_policy(comm))
				goto out;
		} else if (type == MEDUSA_MSG_POLICY_READY &&
			   definitions_done &&
			   load_le64(&header->policy_generation) ==
				   MCP_DATA(comm)->generation) {
			result = 0;
			goto out;
		} else {
			errno = EPROTO;
			goto out;
		}
	}
out:
	free(frame);
	return result;
}

static int v4_queue_decision(struct comm_s *comm, const uint8_t *frame,
			     size_t length)
{
	const struct medusa_frame_header *header =
		(const struct medusa_frame_header *)frame;
	struct comm_buffer_s *buffer;
	struct event_type_s *event;
	const uint8_t *event_data;
	const uint8_t *subject_data;
	const uint8_t *object_data;
	size_t event_length = 0;
	size_t subject_length = 0;
	size_t object_length = 0;
	size_t synthetic_length;
	uint32_t event_id;
	uint64_t request_id = load_le64(&header->request_id);

	if (!request_id ||
	    load_le64(&header->policy_generation) !=
		    MCP_DATA(comm)->generation ||
	    v4_get_u32(frame, length, MEDUSA_TLV_EVENT_ID, &event_id))
		return -EPROTO;
	event = (struct event_type_s *)hash_find(&comm->events, event_id);
	if (!event)
		return -ENOENT;
	event_data = v4_find_tlv(
		frame, length, MEDUSA_TLV_EVENT_DATA, &event_length, true);
	subject_data = v4_find_tlv(
		frame, length, MEDUSA_TLV_SUBJECT_DATA, &subject_length, true);
	object_data = v4_find_tlv(
		frame, length, MEDUSA_TLV_OBJECT_DATA, &object_length, true);
	if (!event_data || !subject_data || !object_data ||
	    event_length != event->acctype.size ||
	    !event->op[0] || subject_length != event->op[0]->m.size ||
	    (event->op[1] && object_length != event->op[1]->m.size))
		return -EMSGSIZE;
	synthetic_length = 2 * sizeof(MCPptr_t) + event_length +
			   subject_length +
			   (event->op[1] ? object_length : 0);
	if (synthetic_length > INT_MAX)
		return -EOVERFLOW;
	buffer = comm_buf_get((int)synthetic_length, comm);
	if (!buffer)
		return -ENOMEM;
	((MCPptr_t *)buffer->comm_buf)[0] = event_id;
	((MCPptr_t *)buffer->comm_buf)[1] = request_id;
	memcpy(buffer->comm_buf + 2 * sizeof(MCPptr_t),
	       event_data, event_length);
	memcpy(buffer->comm_buf + 2 * sizeof(MCPptr_t) + event_length,
	       subject_data, subject_length);
	if (event->op[1])
		memcpy(buffer->comm_buf + 2 * sizeof(MCPptr_t) + event_length +
			       subject_length,
		       object_data, object_length);
	buffer->len = (int)synthetic_length;
	buffer->event = event;
	if (get_event_context(
		    comm, &buffer->context, event, buffer->comm_buf)) {
		buffer->bfree(buffer);
		return -EPROTO;
	}
	buffer->ehh_list = EHH_VS_ALLOW;
	pthread_mutex_lock(&comm->state_lock);
	if (function_init && comm->init_buffer)
		comm_buf_to_queue(&comm->init_buffer->to_wake, buffer);
	else
		comm_buf_todo(buffer);
	pthread_mutex_unlock(&comm->state_lock);
	return 0;
}

static struct comm_buffer_s *
v4_take_waiter(struct comm_s *comm, uint32_t reply_type, uint64_t request_id)
{
	struct comm_buffer_s *found = NULL;
	struct queue_item_s *previous = NULL;
	struct queue_item_s *item;

	pthread_mutex_lock(&comm->wait_for_answer.lock);
	for (item = comm->wait_for_answer.first; item; item = item->next) {
		if (item->buffer->waiting.to == reply_type &&
		    item->buffer->waiting.seq == request_id) {
			found = comm_buf_del(
				&comm->wait_for_answer, previous, item);
			break;
		}
		previous = item;
	}
	pthread_mutex_unlock(&comm->wait_for_answer.lock);
	return found;
}

static int v4_handle_object_reply(struct comm_s *comm, const uint8_t *frame,
				  size_t length, uint32_t type)
{
	const struct medusa_frame_header *header =
		(const struct medusa_frame_header *)frame;
	struct comm_buffer_s *waiter;
	const uint8_t *object_data;
	size_t object_length = 0;
	uint64_t request_id = load_le64(&header->request_id);
	uint32_t wire_status;
	int32_t status;

	if (!request_id ||
	    load_le64(&header->policy_generation) !=
		    MCP_DATA(comm)->generation ||
	    v4_get_u32(frame, length, MEDUSA_TLV_STATUS, &wire_status))
		return -EPROTO;
	status = (int32_t)wire_status;
	waiter = v4_take_waiter(comm, type, request_id);
	if (!waiter)
		return -ENOENT;
	if (type == MEDUSA_MSG_OBJECT_FETCH_REPLY && status == 0) {
		struct object_s *object = waiter->user1;

		object_data = v4_find_tlv(
			frame, length, MEDUSA_TLV_OBJECT_DATA,
			&object_length, true);
		if (!object_data || !object ||
		    object_length != object->class->m.size) {
			waiter->user_data = -1;
		} else {
			memcpy(object->data, object_data, object_length);
			waiter->user_data = 0;
		}
	} else {
		waiter->user_data = status;
	}
	waiter->waiting.to = 0;
	comm_buf_todo(waiter);
	return 0;
}

static int mcp_read_worker(struct comm_s *comm)
{
	uint8_t *frame = malloc(MEDUSA_FRAME_MAX_SIZE);

	if (!frame || tls_alloc_init()) {
		free(frame);
		return -1;
	}
	for (;;) {
		const struct medusa_frame_header *header;
		ssize_t length = v4_read_frame(comm, frame);
		uint16_t type;
		int error;

		if (length < 0)
			break;
		header = (const struct medusa_frame_header *)frame;
		type = load_le16(&header->type);
		if (type == MEDUSA_MSG_DECISION_REQUEST)
			error = v4_queue_decision(comm, frame, (size_t)length);
		else if (type == MEDUSA_MSG_OBJECT_FETCH_REPLY ||
			 type == MEDUSA_MSG_OBJECT_UPDATE_REPLY)
			error = v4_handle_object_reply(
				comm, frame, (size_t)length, type);
		else if (type == MEDUSA_MSG_DECISION_CANCEL) {
			uint64_t request_id = load_le64(&header->request_id);

			if (!request_id ||
			    load_le64(&header->policy_generation) !=
				    MCP_DATA(comm)->generation ||
			    (size_t)length != MEDUSA_FRAME_HEADER_SIZE)
				error = -EPROTO;
			else
				error = v4_cancel_add(comm, request_id);
		} else if (type == MEDUSA_MSG_AUDIT)
			error = 0;
		else if (type == MEDUSA_MSG_POLICY_READY) {
			uint64_t generation =
				load_le64(&header->policy_generation);

			pthread_mutex_lock(&MCP_DATA(comm)->request_lock);
			if ((size_t)length != MEDUSA_FRAME_HEADER_SIZE ||
			    load_le64(&header->request_id) ||
			    !MCP_DATA(comm)->replacement_pending ||
			    generation !=
				    MCP_DATA(comm)->replacement_generation) {
				error = -EPROTO;
			} else {
				MCP_DATA(comm)->generation = generation;
				MCP_DATA(comm)->replacement_pending = false;
				error = 0;
			}
			pthread_mutex_unlock(&MCP_DATA(comm)->request_lock);
		}
		else
			error = -EPROTO;
		if (error) {
			errno = EPROTO;
			break;
		}
	}
	free(frame);
	comm->close(comm);
	return -1;
}

static int mcp_write(struct comm_s *comm)
{
	struct comm_buffer_s *buffer = comm_buf_output_dequeue(comm);
	int result;

	if (buffer->open_counter != comm->open_counter) {
		buffer->bfree(buffer);
		return 1;
	}
	result = v4_write_buffer(comm, buffer);
	buffer->bfree(buffer);
	return result ? -1 : 1;
}

static int mcp_close(struct comm_s *comm)
{
	struct comm_buffer_s *buffer;

	if (comm->fd >= 0)
		close(comm->fd);
	comm->fd = -1;
	pthread_mutex_lock(&comm->output.lock);
	while ((buffer = comm_buf_from_queue(&comm->output)) != NULL)
		buffer->bfree(buffer);
	pthread_mutex_unlock(&comm->output.lock);
	pthread_mutex_lock(&comm->wait_for_answer.lock);
	while ((buffer = comm_buf_from_queue(&comm->wait_for_answer)) != NULL)
		comm_buf_todo(buffer);
	pthread_mutex_unlock(&comm->wait_for_answer.lock);
	comm->open_counter--;
	v4_cancel_clear(comm);
	return 0;
}

static int mcp_conf_error(struct comm_s *comm, const char *format, ...)
{
	va_list arguments;
	char message[4096];
	char prefix[96];

	snprintf(prefix, sizeof(prefix), "comm %.63s: ", comm->name);
	va_start(arguments, format);
	string_vformat_line(
		message, sizeof(message), prefix, format, arguments);
	va_end(arguments);
	write(STDOUT_FILENO, message, strlen(message));
	return -1;
}

static int mcp_answer(struct comm_s *comm, struct comm_buffer_s *request)
{
	struct v4_builder builder;
	uint64_t request_id;
	int16_t answer;
	uint8_t cache_update = MEDUSA_CACHE_UPDATE_NONE;

	request_id = ((MCPptr_t *)request->comm_buf)[1];
	if (!request->approval_done && request->event &&
	    request->event->acctype.kind == MEDUSA_EVENT_ACCESS &&
	    (request->context.result == MED_ALLOW ||
	     request->context.result == MED_DENY) &&
	    approval_enabled_for(request->event->evname->name)) {
		request->context.result = approval_decide(
			request, request_id, request->event->evname->name,
			request->context.result);
		request->approval_done = 1;
	}
	if (request->context.result >= 0 && request->context.subject.class) {
		int continuation = request->do_phase == 0 ?
			0 : request->do_phase - 1000;
		int result = comm->update_object(
			comm, continuation, &request->context.subject, request);

		if (result > 0) {
			request->do_phase = result + 1000;
			return result;
		}
	}
	if (v4_cancel_take(comm, request_id))
		return 0;
	if (request->event &&
	    request->event->acctype.kind == MEDUSA_EVENT_OBJECT_NOTIFICATION &&
	    (request->context.result == MED_ALLOW ||
	     request->context.result == MED_DENY))
		request->context.result = MED_ALLOW;
	answer = request->context.result;
	if (answer != MED_ERR && answer != MED_DENY && answer != MED_ALLOW)
		answer = MED_ERR;
	if (answer == MED_ALLOW && request->event &&
	    request->event->acctype.kind == MEDUSA_EVENT_ACCESS &&
	    (MCP_DATA(comm)->enabled_features &
	     MEDUSA_FEATURE_REPLY_CACHE_UPDATE)) {
		if (request->event->monitored_operand == request->event->op[0])
			cache_update = MEDUSA_CACHE_UPDATE_SUBJECT;
		else if (request->event->monitored_operand ==
			 request->event->op[1])
			cache_update = MEDUSA_CACHE_UPDATE_OBJECT;
	}
	builder = v4_builder_new(
		comm, MEDUSA_MSG_DECISION_REPLY, request_id,
		MCP_DATA(comm)->generation, 32);
	if (!builder.buffer ||
	    v4_builder_add_u16(
		    &builder, MEDUSA_TLV_ANSWER, (uint16_t)answer) ||
	    (cache_update != MEDUSA_CACHE_UPDATE_NONE &&
	     v4_builder_add_u8(
		     &builder, MEDUSA_TLV_CACHE_UPDATE, cache_update))) {
		if (builder.buffer)
			builder.buffer->bfree(builder.buffer);
		return -1;
	}
	v4_enqueue(comm, &builder);
	return 0;
}

int mcp_renew_authrequest(struct comm_buffer_s *request)
{
	struct v4_builder builder;
	uint64_t request_id;

	if (!request || !request->comm ||
	    request->len < (int)(2 * sizeof(MCPptr_t)))
		return -EINVAL;
	request_id = ((MCPptr_t *)request->comm_buf)[1];
	builder = v4_builder_new(
		request->comm, MEDUSA_MSG_DECISION_PROGRESS, request_id,
		MCP_DATA(request->comm)->generation, 0);
	if (!builder.buffer)
		return -ENOMEM;
	return v4_enqueue(request->comm, &builder);
}

static int mcp_object_request(struct comm_s *comm, int continuation,
			      struct object_s *object,
			      struct comm_buffer_s *wake, bool update)
{
	struct v4_builder builder;
	uint64_t request_id;
	uint32_t reply_type = update ?
		MEDUSA_MSG_OBJECT_UPDATE_REPLY :
		MEDUSA_MSG_OBJECT_FETCH_REPLY;

	if (continuation == 3) {
		if (update)
			return wake->user_data == MED_ALLOW ||
			       wake->user_data == 0 ? 0 : -1;
		return wake->user_data;
	}
	if (!object || !object->class ||
	    object->class->m.classid > UINT32_MAX)
		return -EINVAL;
	request_id = mcp_next_request_id(comm);
	builder = v4_builder_new(
		comm, update ? MEDUSA_MSG_OBJECT_UPDATE :
			       MEDUSA_MSG_OBJECT_FETCH,
		request_id, MCP_DATA(comm)->generation,
		32 + object->class->m.size);
	if (!builder.buffer ||
	    v4_builder_add_u32(
		    &builder, MEDUSA_TLV_CLASS_ID,
		    (uint32_t)object->class->m.classid) ||
	    v4_builder_add(
		    &builder, MEDUSA_TLV_OBJECT_DATA, 0, object->data,
		    object->class->m.size)) {
		if (builder.buffer)
			builder.buffer->bfree(builder.buffer);
		return -ENOMEM;
	}
	wake->user_data = -1;
	wake->user1 = update ? NULL : object;
	wake->waiting.to = reply_type;
	wake->waiting.cid = object->class->m.classid;
	wake->waiting.seq = request_id;
	comm_buf_to_queue_locked(&comm->wait_for_answer, wake);
	return v4_enqueue(comm, &builder) ? -1 : 3;
}

static int mcp_fetch_object(struct comm_s *comm, int continuation,
			    struct object_s *object,
			    struct comm_buffer_s *wake)
{
	return mcp_object_request(
		comm, continuation, object, wake, false);
}

static int mcp_update_object(struct comm_s *comm, int continuation,
			     struct object_s *object,
			     struct comm_buffer_s *wake)
{
	return mcp_object_request(
		comm, continuation, object, wake, true);
}

struct comm_s *mcp_alloc_comm(char *name)
{
	struct comm_s *comm =
		comm_new(name, sizeof(struct mcp_comm_s));

	if (!comm)
		return NULL;
	if (pthread_mutex_init(&MCP_DATA(comm)->request_lock, NULL))
		return NULL;
	comm->read = mcp_read_worker;
	comm->write = mcp_write;
	comm->close = mcp_close;
	comm->answer = mcp_answer;
	comm->fetch_object = mcp_fetch_object;
	comm->update_object = mcp_update_object;
	comm->conf_error = mcp_conf_error;
	return comm;
}

int mcp_open(struct comm_s *comm, char *filename)
{
	comm->fd = comm_open_skip_stdfds(filename, O_RDWR, 0);
	return comm->fd < 0 ? -1 : 0;
}

int mcp_receive_greeting(struct comm_s *comm)
{
	return mcp_receive_handshake(comm);
}

int mcp_replace_policy(struct comm_s *comm)
{
	struct v4_builder abort;
	uint64_t generation;
	int error;

	if (!comm || comm->fd < 0)
		return -ENOTCONN;
	pthread_mutex_lock(&MCP_DATA(comm)->request_lock);
	if (!(MCP_DATA(comm)->enabled_features &
	      MEDUSA_FEATURE_ATOMIC_POLICY_REPLACE)) {
		error = -EOPNOTSUPP;
		goto out;
	}
	if (MCP_DATA(comm)->replacement_pending) {
		error = -EBUSY;
		goto out;
	}
	if (MCP_DATA(comm)->generation == UINT64_MAX) {
		error = -EOVERFLOW;
		goto out;
	}
	generation = MCP_DATA(comm)->generation + 1;
	MCP_DATA(comm)->replacement_generation = generation;
	MCP_DATA(comm)->replacement_pending = true;
	pthread_mutex_unlock(&MCP_DATA(comm)->request_lock);

	error = v4_install_policy_generation(comm, generation);
	if (!error)
		return 0;
	abort = v4_builder_new(
		comm, MEDUSA_MSG_POLICY_ABORT, 0, generation, 0);
	v4_send_now(comm, &abort);
	pthread_mutex_lock(&MCP_DATA(comm)->request_lock);
	MCP_DATA(comm)->replacement_pending = false;
out:
	pthread_mutex_unlock(&MCP_DATA(comm)->request_lock);
	return error;
}

struct comm_s *mcp_listen(in_port_t port)
{
	(void)port;
	errno = EOPNOTSUPP;
	return NULL;
}

int mcp_to_accept(struct comm_s *comm, struct comm_s *listener,
		  in_addr_t address, in_addr_t mask, in_port_t port)
{
	(void)comm;
	(void)listener;
	(void)address;
	(void)mask;
	(void)port;
	return -EOPNOTSUPP;
}

int mcp_ready_answer(struct comm_s *comm)
{
	(void)comm;
	return 0;
}

int mcp_init(char *filename)
{
	return mcp_language_do(filename);
}
