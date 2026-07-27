// SPDX-License-Identifier: GPL-2.0

#include "validate.h"

#include <stdint.h>
#include <string.h>

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
	case MCP_DEFINITION_ATTRIBUTE_OUT_OF_RANGE:
		return "attribute extends beyond its object";
	}
	return "unknown definition error";
}
