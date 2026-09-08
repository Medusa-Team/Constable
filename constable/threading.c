// SPDX-License-Identifier: GPL-2.0

#include <unistd.h>

#include "threading.h"

static unsigned int configured_worker_count = CONSTABLE_MIN_AUTO_WORKERS;

int worker_pool_configure(unsigned int requested_workers)
{
	long online_cpus;

	if (requested_workers > CONSTABLE_MAX_WORKERS)
		return -1;
	if (requested_workers != CONSTABLE_WORKERS_AUTO) {
		configured_worker_count = requested_workers;
		return configured_worker_count == 0 ? -1 : 0;
	}

	online_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (online_cpus < (long)CONSTABLE_MIN_AUTO_WORKERS)
		configured_worker_count = CONSTABLE_MIN_AUTO_WORKERS;
	else if (online_cpus > (long)CONSTABLE_MAX_WORKERS)
		configured_worker_count = CONSTABLE_MAX_WORKERS;
	else
		configured_worker_count = (unsigned int)online_cpus;
	return 0;
}

unsigned int worker_pool_count(void)
{
	return configured_worker_count;
}
