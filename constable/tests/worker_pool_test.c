/* SPDX-License-Identifier: GPL-2.0 */

#include <stdio.h>

#include "../threading.h"

static int failures;

#define EXPECT_TRUE(condition)                                                   \
	do {                                                                      \
		if (!(condition)) {                                                 \
			fprintf(stderr, "%s:%d: expected %s\n", __FILE__, __LINE__, \
				#condition);                                            \
			failures++;                                                   \
		}                                                                 \
	} while (0)

int main(void)
{
	EXPECT_TRUE(worker_pool_configure(1) == 0);
	EXPECT_TRUE(worker_pool_count() == 1);
	EXPECT_TRUE(worker_pool_configure(CONSTABLE_MAX_WORKERS) == 0);
	EXPECT_TRUE(worker_pool_count() == CONSTABLE_MAX_WORKERS);
	EXPECT_TRUE(worker_pool_configure(CONSTABLE_MAX_WORKERS + 1) < 0);
	EXPECT_TRUE(worker_pool_count() == CONSTABLE_MAX_WORKERS);

	EXPECT_TRUE(worker_pool_configure(CONSTABLE_WORKERS_AUTO) == 0);
	EXPECT_TRUE(worker_pool_count() >= CONSTABLE_MIN_AUTO_WORKERS);
	EXPECT_TRUE(worker_pool_count() <= CONSTABLE_MAX_WORKERS);

	if (failures) {
		fprintf(stderr, "worker pool: %d failure(s)\n", failures);
		return 1;
	}
	puts("worker pool: all checks passed");
	return 0;
}
