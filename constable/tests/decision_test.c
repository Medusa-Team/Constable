// SPDX-License-Identifier: GPL-2.0

#include <stdio.h>

#include "decision.h"

struct answer {
	int value;
	const char *name;
};

static const struct answer accumulated_answers[] = {
	{ RESULT_ERR, "ERR" },
	{ RESULT_FORCE_ALLOW, "FORCE_ALLOW" },
	{ RESULT_DENY, "DENY" },
	{ RESULT_FAKE_ALLOW, "FAKE_ALLOW" },
	{ RESULT_ALLOW, "ALLOW" },
};

static const struct answer candidate_answers[] = {
	{ RESULT_FORCE_ALLOW, "FORCE_ALLOW" },
	{ RESULT_DENY, "DENY" },
	{ RESULT_FAKE_ALLOW, "FAKE_ALLOW" },
	{ RESULT_ALLOW, "ALLOW" },
};

/*
 * Rows are accumulated_answers and columns are candidate_answers. This table
 * is the golden protocol-v3 policy behavior, not a proposed precedence model.
 */
static const int expected[][4] = {
	{ RESULT_FORCE_ALLOW, RESULT_DENY, RESULT_FAKE_ALLOW, RESULT_ALLOW },
	{ RESULT_FORCE_ALLOW, RESULT_DENY, RESULT_FAKE_ALLOW,
	  RESULT_FORCE_ALLOW },
	{ RESULT_DENY, RESULT_DENY, RESULT_DENY, RESULT_DENY },
	{ RESULT_FAKE_ALLOW, RESULT_DENY, RESULT_FAKE_ALLOW,
	  RESULT_FAKE_ALLOW },
	{ RESULT_FORCE_ALLOW, RESULT_DENY, RESULT_FAKE_ALLOW, RESULT_ALLOW },
};

static int failures;

static void expect_result(const char *context, int actual, int wanted)
{
	if (actual == wanted)
		return;

	fprintf(stderr, "%s: got %d, expected %d\n", context, actual, wanted);
	failures++;
}

static void test_golden_composition_table(void)
{
	size_t row;
	size_t column;
	char context[96];

	for (row = 0; row < sizeof(accumulated_answers) /
			      sizeof(accumulated_answers[0]); row++) {
		for (column = 0; column < sizeof(candidate_answers) /
					 sizeof(candidate_answers[0]); column++) {
			snprintf(context, sizeof(context), "%s then %s",
				 accumulated_answers[row].name,
				 candidate_answers[column].name);
			expect_result(
				context,
				evaluate_result(accumulated_answers[row].value,
						candidate_answers[column].value),
				expected[row][column]);
		}
	}
}

static void test_invalid_candidate_is_deny(void)
{
	static const int invalid_candidates[] = {
		RESULT_ERR,
		RESULT_RETRY,
		99,
	};
	size_t previous;
	size_t candidate;
	char context[96];

	for (previous = 0;
	     previous < sizeof(accumulated_answers) /
				sizeof(accumulated_answers[0]);
	     previous++) {
		for (candidate = 0;
		     candidate < sizeof(invalid_candidates) /
				     sizeof(invalid_candidates[0]);
		     candidate++) {
			snprintf(context, sizeof(context),
				 "%s then invalid candidate %d",
				 accumulated_answers[previous].name,
				 invalid_candidates[candidate]);
			expect_result(
				context,
				evaluate_result(
					accumulated_answers[previous].value,
					invalid_candidates[candidate]),
				RESULT_DENY);
		}
	}
}

int main(void)
{
	test_golden_composition_table();
	test_invalid_candidate_is_deny();

	if (failures) {
		fprintf(stderr, "decision semantics: %d failure(s)\n", failures);
		return 1;
	}

	puts("decision semantics: 35 checks passed");
	return 0;
}
