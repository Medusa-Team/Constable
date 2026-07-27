// SPDX-License-Identifier: GPL-2.0

#include <mcompiler/c_language.h>
#include <mcompiler/lex.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct memory_preprocessor {
	struct compiler_preprocessor_class interface;
	const char *input;
	size_t length;
	size_t position;
};

static int failures;
static int checks;
static int preprocessors_destroyed;

#define EXPECT_TRUE(condition, description)					\
	do {									\
		checks++;							\
		if (!(condition)) {						\
			fprintf(stderr, "FAIL: %s\n", description);		\
			failures++;						\
		}								\
	} while (0)

static void destroy_preprocessor(struct compiler_preprocessor_class *interface)
{
	struct memory_preprocessor *preprocessor =
		(struct memory_preprocessor *)interface;

	preprocessors_destroyed++;
	free(preprocessor);
}

static int memory_get_char(struct compiler_preprocessor_class *interface,
			   char *character)
{
	struct memory_preprocessor *preprocessor =
		(struct memory_preprocessor *)interface;

	if (preprocessor->position == preprocessor->length)
		return 1;
	*character = preprocessor->input[preprocessor->position++];
	interface->col++;
	return 0;
}

static struct compiler_preprocessor_class *
memory_preprocessor_create(const char *input, size_t length)
{
	struct memory_preprocessor *preprocessor =
		calloc(1, sizeof(*preprocessor));

	if (!preprocessor)
		return NULL;
	preprocessor->interface.destroy = destroy_preprocessor;
	preprocessor->interface.get_char = memory_get_char;
	preprocessor->interface.filename = "memory";
	preprocessor->interface.row = 1;
	preprocessor->input = input;
	preprocessor->length = length;
	return &preprocessor->interface;
}

static void test_long_identifier(void)
{
	const size_t length = 65536;
	struct compiler_preprocessor_class *preprocessor;
	struct compiler_lex_class *lexer;
	struct lex_s token = { 0 };
	char *input = malloc(length);
	val_t *value;

	EXPECT_TRUE(input != NULL, "long lexer input allocation succeeds");
	if (!input)
		return;
	memset(input, 'a', length);
	preprocessor = memory_preprocessor_create(input, length);
	EXPECT_TRUE(preprocessor != NULL,
		    "memory preprocessor allocation succeeds");
	if (!preprocessor) {
		free(input);
		return;
	}
	lexer = lex_create(clex_states, preprocessor);
	EXPECT_TRUE(lexer != NULL, "lexer allocation succeeds");
	if (!lexer) {
		preprocessor->destroy(preprocessor);
		free(input);
		return;
	}

	lexer->lex(lexer, &token, END);
	value = (val_t *)token.data;
	EXPECT_TRUE(token.sym == CL_ID,
		    "a token spanning thousands of growth steps is recognized");
	EXPECT_TRUE(value && value->size == (int)length,
		    "the long token retains its exact length");
	EXPECT_TRUE(value && value->value[0] == 'a' &&
		    value->value[length - 1] == 'a' &&
		    value->value[length] == '\0',
		    "the long token remains bounded and terminated");

	free(value);
	lexer->destroy(lexer);
	free(input);
}

static void test_exponent_overflow(void)
{
	static const char input[] = "1e999999999999999999999999999999";
	struct compiler_preprocessor_class *preprocessor =
		memory_preprocessor_create(input, sizeof(input) - 1);
	struct compiler_lex_class *lexer;
	struct lex_s token = { 0 };

	EXPECT_TRUE(preprocessor != NULL,
		    "exponent preprocessor allocation succeeds");
	if (!preprocessor)
		return;
	lexer = lex_create(clex_states, preprocessor);
	EXPECT_TRUE(lexer != NULL, "exponent lexer allocation succeeds");
	if (!lexer) {
		preprocessor->destroy(preprocessor);
		return;
	}
	lexer->lex(lexer, &token, END);
	EXPECT_TRUE(token.sym == eLEXERR,
		    "an overflowing decimal exponent is rejected");
	lexer->destroy(lexer);
}

int main(void)
{
	test_long_identifier();
	test_exponent_overflow();
	EXPECT_TRUE(preprocessors_destroyed == 2,
		    "lexer destruction releases each preprocessor");

	if (failures) {
		fprintf(stderr, "lexer growth: %d failure(s)\n", failures);
		return 1;
	}
	printf("lexer growth: %d checks passed\n", checks);
	return 0;
}
