/* SPDX-License-Identifier: GPL-2.0 */
#ifndef CONSTABLE_DECISION_H
#define CONSTABLE_DECISION_H

/*
 * Policy-language answer values. These numeric values are observable in
 * protocol-v3 policies and must remain stable until an explicitly versioned
 * language or protocol change.
 */
#define RESULT_ERR		(-1)
#define RESULT_FORCE_ALLOW	0
#define RESULT_DENY		1
#define RESULT_FAKE_ALLOW	2
#define RESULT_ALLOW		3
#define RESULT_RETRY		4

/*
 * Combine a completed handler's candidate answer with the accumulated answer.
 * Only RESULT_FORCE_ALLOW, RESULT_DENY, RESULT_FAKE_ALLOW and RESULT_ALLOW are
 * valid candidate answers. Any other candidate is normalized to DENY.
 */
int evaluate_result(int previous, int candidate);

#endif /* CONSTABLE_DECISION_H */
