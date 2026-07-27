/* SPDX-License-Identifier: GPL-2.0 */
#ifndef CONSTABLE_POLICY_EVENT_TEST_H
#define CONSTABLE_POLICY_EVENT_TEST_H

#include <stdio.h>

int policy_event_self_test(const char *comm_name, FILE *output);
int policy_historical_event_self_test(const char *comm_name, FILE *output);

#endif /* CONSTABLE_POLICY_EVENT_TEST_H */
