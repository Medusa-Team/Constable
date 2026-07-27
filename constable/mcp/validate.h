/* SPDX-License-Identifier: GPL-2.0 */
#ifndef CONSTABLE_MCP_VALIDATE_H
#define CONSTABLE_MCP_VALIDATE_H

#include <stddef.h>

#include "../medusa_object.h"

enum mcp_definition_validation {
	MCP_DEFINITION_VALID = 0,
	MCP_DEFINITION_MISSING_TERMINATOR,
	MCP_DEFINITION_EMPTY_NAME,
	MCP_DEFINITION_NAME_TOO_LONG,
	MCP_DEFINITION_INVALID_ATTRIBUTE_LIST,
	MCP_DEFINITION_ATTRIBUTE_OUT_OF_RANGE,
};

enum mcp_definition_validation
mcp_validate_class_definition(const struct medusa_class_s *definition,
			      const struct medusa_attribute_s *attributes,
			      size_t attribute_count);

enum mcp_definition_validation
mcp_validate_acctype_definition(const struct medusa_acctype_s *definition,
				const struct medusa_attribute_s *attributes,
				size_t attribute_count);

const char *
mcp_definition_validation_message(enum mcp_definition_validation result);

#endif /* CONSTABLE_MCP_VALIDATE_H */
