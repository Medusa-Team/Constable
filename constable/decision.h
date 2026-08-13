/* SPDX-License-Identifier: GPL-2.0 */
#ifndef CONSTABLE_DECISION_H
#define CONSTABLE_DECISION_H

/*
 * Policy-language answer values. These numeric values are observable in
 * protocol-v3 policies and must remain stable until an explicitly versioned
 * language or protocol change.
 */
#define RESULT_ERR		(-1) /* handler failure */
#define RESULT_FORCE_ALLOW	0    /* strong allow; later deny/fake may override */
#define RESULT_DENY		1    /* final deny */
#define RESULT_FAKE_ALLOW	2    /* provisional allow; a later handler may deny */
#define RESULT_ALLOW		3    /* provisional normal allow */
#define RESULT_RETRY		4    /* defer evaluation */

/*
 * Combine a completed handler's candidate answer with the accumulated answer.
 * Only RESULT_FORCE_ALLOW, RESULT_DENY, RESULT_FAKE_ALLOW and RESULT_ALLOW are
 * valid candidate answers. Any other candidate is normalized to DENY.
 *
 * State transitions:
 * ERR -> any result
 * FORCE_ALLOW -> DENY or FAKE_ALLOW
 * DENY -> DENY
 * FAKE_ALLOW -> DENY
 * ALLOW -> FORCE_ALLOW, DENY, or FAKE_ALLOW
 */
int evaluate_result(int previous, int candidate);

#endif /* CONSTABLE_DECISION_H */
