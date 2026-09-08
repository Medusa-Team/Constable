// SPDX-License-Identifier: GPL-2.0

#include "validate.h"

#include <stdint.h>
#include <string.h>

static uint16_t load_le16(const void *source)
{
	const unsigned char *bytes = source;

	return (uint16_t)bytes[0] | (uint16_t)bytes[1] << 8;
}

static uint32_t load_le32(const void *source)
{
	const unsigned char *bytes = source;

	return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
	       (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static int v4_tlv_known(uint16_t type)
{
	return (type >= MEDUSA_TLV_MIN_VERSION &&
		type <= MEDUSA_TLV_STATE) ||
	       (type >= MEDUSA_TLV_CLASS_ID &&
		type <= MEDUSA_TLV_EVENT_KIND) ||
	       (type >= MEDUSA_TLV_FALLBACK_POLICY &&
		type <= MEDUSA_TLV_DOMAIN_RULE) ||
	       (type >= MEDUSA_TLV_ERROR_CODE &&
		type <= MEDUSA_TLV_OFFENDING_TYPE);
}

int mcp_validate_v4_frame(const void *wire, size_t length)
{
	const uint8_t *frame = wire;
	const struct medusa_frame_header *header;
	size_t payload_length;
	size_t offset;

	if (!frame || length < MEDUSA_FRAME_HEADER_SIZE)
		return -1;
	header = (const struct medusa_frame_header *)frame;
	if (load_le16(&header->version) != MEDUSA_PROTOCOL_VERSION ||
	    load_le32(&header->flags) != 0 ||
	    load_le32(&header->reserved) != 0)
		return -1;
	payload_length = load_le32(&header->payload_length);
	if (payload_length > MEDUSA_FRAME_MAX_PAYLOAD ||
	    payload_length != length - MEDUSA_FRAME_HEADER_SIZE)
		return -1;
	offset = MEDUSA_FRAME_HEADER_SIZE;
	while (offset < length) {
		const struct medusa_tlv *tlv;
		size_t tlv_length;
		size_t aligned;
		size_t index;
		uint16_t flags;
		uint16_t type;

		if (length - offset < MEDUSA_TLV_HEADER_SIZE)
			return -1;
		tlv = (const struct medusa_tlv *)(frame + offset);
		tlv_length = load_le32(&tlv->length);
		flags = load_le16(&tlv->flags);
		type = load_le16(&tlv->type);
		if (flags & ~(MEDUSA_TLV_F_REQUIRED | MEDUSA_TLV_F_ARRAY) ||
		    tlv_length < MEDUSA_TLV_HEADER_SIZE)
			return -1;
		aligned = MEDUSA_TLV_ALIGN_UP(tlv_length);
		if (aligned < tlv_length || aligned > length - offset)
			return -1;
		if (!v4_tlv_known(type) && (flags & MEDUSA_TLV_F_REQUIRED))
			return -1;
		for (index = tlv_length; index < aligned; index++)
			if (frame[offset + index] != 0)
				return -1;
		offset += aligned;
	}
	return offset == length ? 0 : -1;
}

int mcp_validate_event_kind(uint8_t kind)
{
	return kind == MEDUSA_EVENT_ACCESS ||
	       kind == MEDUSA_EVENT_OBJECT_NOTIFICATION;
}

static enum mcp_definition_validation
validate_name(const char *name, size_t capacity, int allow_empty)
{
	const char *terminator = memchr(name, '\0', capacity);

	if (!terminator)
		return MCP_DEFINITION_MISSING_TERMINATOR;
	if (!allow_empty && terminator == name)
		return MCP_DEFINITION_EMPTY_NAME;
	return MCP_DEFINITION_VALID;
}

enum mcp_definition_validation
mcp_validate_definition_extent(size_t message_length, size_t attributes_offset,
			       size_t attribute_size, size_t *attribute_count)
{
	size_t payload_length;
	size_t count;

	if (!attribute_count || !attribute_size ||
	    message_length < attributes_offset)
		return MCP_DEFINITION_INVALID_ATTRIBUTE_LIST;
	payload_length = message_length - attributes_offset;
	if (!payload_length || payload_length % attribute_size)
		return MCP_DEFINITION_INVALID_ATTRIBUTE_LIST;
	count = payload_length / attribute_size;
	if (count > MCP_DEFINITION_ATTRIBUTE_LIMIT)
		return MCP_DEFINITION_TOO_MANY_ATTRIBUTES;
	*attribute_count = count;
	return MCP_DEFINITION_VALID;
}

static enum mcp_definition_validation
validate_attributes(const struct medusa_attribute_s *attributes,
		    size_t attribute_count, uint16_t object_size)
{
	size_t index;

	if (!attributes || !attribute_count ||
	    attributes[attribute_count - 1].type != MED_TYPE_END)
		return MCP_DEFINITION_INVALID_ATTRIBUTE_LIST;

	for (index = 0; index + 1 < attribute_count; index++) {
		enum mcp_definition_validation result;
		uint32_t end;

		if (attributes[index].type == MED_TYPE_END)
			return MCP_DEFINITION_INVALID_ATTRIBUTE_LIST;
		if (!attributes[index].length)
			return MCP_DEFINITION_ATTRIBUTE_OUT_OF_RANGE;

		result = validate_name(attributes[index].name,
				       sizeof(attributes[index].name), 0);
		if (result != MCP_DEFINITION_VALID)
			return result;

		end = (uint32_t)attributes[index].offset +
		      (uint32_t)attributes[index].length;
		if (end > object_size)
			return MCP_DEFINITION_ATTRIBUTE_OUT_OF_RANGE;
	}

	return MCP_DEFINITION_VALID;
}

enum mcp_definition_validation
mcp_validate_class_definition(const struct medusa_class_s *definition,
			      const struct medusa_attribute_s *attributes,
			      size_t attribute_count)
{
	enum mcp_definition_validation result;

	if (!definition)
		return MCP_DEFINITION_INVALID_ATTRIBUTE_LIST;

	result = validate_name(definition->name, sizeof(definition->name), 0);
	if (result != MCP_DEFINITION_VALID)
		return result;

	return validate_attributes(attributes, attribute_count, definition->size);
}

enum mcp_definition_validation
mcp_validate_acctype_definition(const struct medusa_acctype_s *definition,
				const struct medusa_attribute_s *attributes,
				size_t attribute_count)
{
	enum mcp_definition_validation result;
	size_t index;

	if (!definition)
		return MCP_DEFINITION_INVALID_ATTRIBUTE_LIST;

	result = validate_name(definition->name, sizeof(definition->name), 0);
	if (result != MCP_DEFINITION_VALID)
		return result;
	if (strlen(definition->name) >= MEDUSA_ATTRNAME_MAX)
		return MCP_DEFINITION_NAME_TOO_LONG;

	for (index = 0; index < 2; index++) {
		result = validate_name(definition->op_name[index],
				       sizeof(definition->op_name[index]), 1);
		if (result != MCP_DEFINITION_VALID)
			return result;
	}

	return validate_attributes(attributes, attribute_count, definition->size);
}

const char *
mcp_definition_validation_message(enum mcp_definition_validation result)
{
	switch (result) {
	case MCP_DEFINITION_VALID:
		return "valid definition";
	case MCP_DEFINITION_MISSING_TERMINATOR:
		return "name is not NUL-terminated";
	case MCP_DEFINITION_EMPTY_NAME:
		return "name is empty";
	case MCP_DEFINITION_NAME_TOO_LONG:
		return "event name does not fit the operation attribute";
	case MCP_DEFINITION_INVALID_ATTRIBUTE_LIST:
		return "attribute list is malformed";
	case MCP_DEFINITION_TOO_MANY_ATTRIBUTES:
		return "attribute list exceeds the protocol limit";
	case MCP_DEFINITION_ATTRIBUTE_OUT_OF_RANGE:
		return "attribute extends beyond its object";
	}
	return "unknown definition error";
}
