// SPDX-License-Identifier: GPL-2.0

#include "decision.h"

int evaluate_result(int previous, int candidate)
{
	if (candidate != RESULT_ALLOW && candidate != RESULT_DENY &&
	    candidate != RESULT_FAKE_ALLOW &&
	    candidate != RESULT_FORCE_ALLOW)
		candidate = RESULT_DENY;

	if (previous == RESULT_DENY || candidate == RESULT_DENY)
		return RESULT_DENY;
	if (previous == RESULT_ERR || candidate == RESULT_FAKE_ALLOW)
		return candidate;
	if (previous == RESULT_ALLOW && candidate == RESULT_FORCE_ALLOW)
		return candidate;

	return previous;
}
