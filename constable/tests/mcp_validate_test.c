// SPDX-License-Identifier: GPL-2.0

#include "mcp/validate.h"

#include <stdio.h>
#include <string.h>

static int failures;
static int checks;

#define EXPECT_EQ(actual, expected, description)				\
	do {									\
		int actual_value = (actual);					\
		checks++;							\
		if (actual_value != (expected)) {				\
			fprintf(stderr, "FAIL: %s (got %d, expected %d)\n",\
				description, actual_value, (expected));		\
			failures++;						\
		}								\
	} while (0)

static struct medusa_attribute_s valid_attributes[] = {
	{
		.offset = 0,
		.length = 4,
		.type = MED_TYPE_UNSIGNED,
		.name = "uid",
	},
	MED_ATTR_END,
};

static void test_definition_extent(void)
{
	size_t count = 0;
	const size_t offset = 24;
	const size_t attribute_size =
		sizeof(struct medusa_comm_attribute_s);

	EXPECT_EQ(mcp_validate_definition_extent(
			  offset + 2 * attribute_size, offset,
			  attribute_size, &count),
		  MCP_DEFINITION_VALID,
		  "a complete attribute extent is accepted");
	EXPECT_EQ((int)count, 2,
		  "the validated attribute count is returned");
	EXPECT_EQ(mcp_validate_definition_extent(
			  offset + attribute_size - 1, offset,
			  attribute_size, &count),
		  MCP_DEFINITION_INVALID_ATTRIBUTE_LIST,
		  "a partial wire attribute is rejected");
	EXPECT_EQ(mcp_validate_definition_extent(
			  offset + (MCP_DEFINITION_ATTRIBUTE_LIMIT + 1) *
			  attribute_size,
			  offset, attribute_size, &count),
		  MCP_DEFINITION_TOO_MANY_ATTRIBUTES,
		  "an oversized remote definition is rejected");
	EXPECT_EQ(mcp_validate_definition_extent(offset, offset,
						 attribute_size, &count),
		  MCP_DEFINITION_INVALID_ATTRIBUTE_LIST,
		  "an empty remote attribute list is rejected");
}

static void test_class_definition(void)
{
	struct medusa_class_s definition = {
		.classid = 1,
		.size = 4,
		.name = "process",
	};
	struct medusa_attribute_s attributes[2];

	memcpy(attributes, valid_attributes, sizeof(attributes));
	EXPECT_EQ(mcp_validate_class_definition(&definition, attributes, 2),
		  MCP_DEFINITION_VALID, "a bounded class definition is valid");

	memset(definition.name, 'x', sizeof(definition.name));
	EXPECT_EQ(mcp_validate_class_definition(&definition, attributes, 2),
		  MCP_DEFINITION_MISSING_TERMINATOR,
		  "an unterminated class name is rejected");

	definition.name[0] = '\0';
	EXPECT_EQ(mcp_validate_class_definition(&definition, attributes, 2),
		  MCP_DEFINITION_EMPTY_NAME, "an empty class name is rejected");

	memcpy(definition.name, "process", sizeof("process"));
	memset(attributes[0].name, 'a', sizeof(attributes[0].name));
	EXPECT_EQ(mcp_validate_class_definition(&definition, attributes, 2),
		  MCP_DEFINITION_MISSING_TERMINATOR,
		  "an unterminated attribute name is rejected");

	memcpy(attributes, valid_attributes, sizeof(attributes));
	attributes[0].offset = 3;
	EXPECT_EQ(mcp_validate_class_definition(&definition, attributes, 2),
		  MCP_DEFINITION_ATTRIBUTE_OUT_OF_RANGE,
		  "an attribute beyond the object is rejected");

	memcpy(attributes, valid_attributes, sizeof(attributes));
	attributes[0].length = 0;
	EXPECT_EQ(mcp_validate_class_definition(&definition, attributes, 2),
		  MCP_DEFINITION_ATTRIBUTE_OUT_OF_RANGE,
		  "a zero-width attribute is rejected");

	memcpy(attributes, valid_attributes, sizeof(attributes));
	attributes[1].type = MED_TYPE_UNSIGNED;
	EXPECT_EQ(mcp_validate_class_definition(&definition, attributes, 2),
		  MCP_DEFINITION_INVALID_ATTRIBUTE_LIST,
		  "a definition without a terminal attribute is rejected");
}

static void test_acctype_definition(void)
{
	struct medusa_acctype_s definition = {
		.opid = 1,
		.size = 4,
		.name = "open",
		.op_name = { "subject", "object" },
	};

	EXPECT_EQ(mcp_validate_acctype_definition(&definition, valid_attributes,
						  2),
		  MCP_DEFINITION_VALID, "a bounded event definition is valid");

	memset(definition.name, 'e', MEDUSA_ATTRNAME_MAX);
	definition.name[MEDUSA_ATTRNAME_MAX] = '\0';
	EXPECT_EQ(mcp_validate_acctype_definition(&definition, valid_attributes,
						  2),
		  MCP_DEFINITION_NAME_TOO_LONG,
		  "an event name that cannot fit internally is rejected");

	memcpy(definition.name, "open", sizeof("open"));
	memset(definition.op_name[1], 'o', sizeof(definition.op_name[1]));
	EXPECT_EQ(mcp_validate_acctype_definition(&definition, valid_attributes,
						  2),
		  MCP_DEFINITION_MISSING_TERMINATOR,
		  "an unterminated operand name is rejected");
}

int main(void)
{
	test_definition_extent();
	test_class_definition();
	test_acctype_definition();

	if (failures) {
		fprintf(stderr, "MCP validation: %d failure(s)\n", failures);
		return 1;
	}

	printf("MCP validation: %d checks passed\n", checks);
	return 0;
}
