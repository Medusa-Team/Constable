// SPDX-License-Identifier: GPL-2.0

#include "rotate.h"

#include <mcompiler/checked_math.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int format_generation(char *output, size_t output_size,
			     const char *filename, unsigned int generation)
{
	int length;

	length = snprintf(output, output_size, "%s.%u", filename, generation);
	if (length < 0 || (size_t)length >= output_size) {
		errno = EOVERFLOW;
		return -1;
	}
	return 0;
}

static int unlink_if_present(const char *path)
{
	if (unlink(path) == 0 || errno == ENOENT)
		return 0;
	return -1;
}

static int rename_if_present(const char *source, const char *destination)
{
	if (rename(source, destination) == 0 || errno == ENOENT)
		return 0;
	return -1;
}

int rbac_rotate_files(const char *filename, unsigned int generations)
{
	char *source;
	char *destination;
	size_t path_size;
	unsigned int generation;

	if (!filename || !filename[0]) {
		errno = EINVAL;
		return -1;
	}
	if (generations > RBAC_ROTATION_LIMIT) {
		errno = E2BIG;
		return -1;
	}
	if (!generations) {
		return unlink_if_present(filename);
	}
	if (!checked_size_add(strlen(filename),
			      sizeof(".4294967295"), &path_size)) {
		errno = EOVERFLOW;
		return -1;
	}
	source = malloc(path_size);
	destination = malloc(path_size);
	if (!source || !destination) {
		free(source);
		free(destination);
		errno = ENOMEM;
		return -1;
	}

	if (format_generation(destination, path_size, filename, generations))
		goto fail;
	if (unlink_if_present(destination))
		goto fail;
	for (generation = generations - 1; generation > 0; generation--) {
		if (format_generation(source, path_size, filename, generation) ||
		    format_generation(destination, path_size, filename,
				      generation + 1))
			goto fail;
		if (rename_if_present(source, destination))
			goto fail;
	}
	if (format_generation(destination, path_size, filename, 1))
		goto fail;
	if (rename_if_present(filename, destination))
		goto fail;
	free(source);
	free(destination);
	return 0;

fail:
	free(source);
	free(destination);
	return -1;
}
