// SPDX-License-Identifier: GPL-2.0

#include "mcp/validate.h"

#include <stdlib.h>
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
	EXPECT_EQ(mcp_validate_event_kind(MEDUSA_EVENT_ACCESS), 1,
		  "an access event kind is valid");
	EXPECT_EQ(mcp_validate_event_kind(MEDUSA_EVENT_OBJECT_NOTIFICATION), 1,
		  "an object-notification event kind is valid");
	EXPECT_EQ(mcp_validate_event_kind(2), 0,
		  "an unknown event kind is rejected");

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

static int hex_nibble(char value)
{
	if (value >= '0' && value <= '9')
		return value - '0';
	if (value >= 'a' && value <= 'f')
		return value - 'a' + 10;
	return -1;
}

static void test_protocol_corpus(void)
{
	const char *path = getenv("MEDUSA_PROTOCOL_CORPUS");
	unsigned char frame[MEDUSA_FRAME_MAX_SIZE];
	char line[4096];
	FILE *corpus;

	if (!path)
		path = "fixtures/protocol-v4-conformance.txt";
	corpus = fopen(path, "r");
	if (!corpus) {
		perror(path);
		failures++;
		return;
	}
	while (fgets(line, sizeof(line), corpus)) {
		char *first;
		char *second;
		char *hex;
		size_t hex_length;
		size_t index;
		int expected;

		if (line[0] == '#' || line[0] == '\n')
			continue;
		first = strchr(line, '|');
		second = first ? strchr(first + 1, '|') : NULL;
		if (!first || !second) {
			failures++;
			continue;
		}
		*first = '\0';
		*second = '\0';
		hex = second + 1;
		hex[strcspn(hex, "\r\n")] = '\0';
		hex_length = strlen(hex);
		if (hex_length % 2 || hex_length / 2 > sizeof(frame)) {
			failures++;
			continue;
		}
		for (index = 0; index < hex_length / 2; index++) {
			int high = hex_nibble(hex[index * 2]);
			int low = hex_nibble(hex[index * 2 + 1]);

			if (high < 0 || low < 0) {
				failures++;
				break;
			}
			frame[index] = (unsigned char)((high << 4) | low);
		}
		if (index != hex_length / 2)
			continue;
		expected = strcmp(line, "valid") == 0 ? 0 : -1;
		EXPECT_EQ(mcp_validate_v4_frame(frame, hex_length / 2),
			  expected, first + 1);
	}
	fclose(corpus);
}

int main(void)
{
	test_definition_extent();
	test_class_definition();
	test_acctype_definition();
	test_protocol_corpus();

	if (failures) {
		fprintf(stderr, "MCP validation: %d failure(s)\n", failures);
		return 1;
	}

	printf("MCP validation: %d checks passed\n", checks);
	return 0;
}
