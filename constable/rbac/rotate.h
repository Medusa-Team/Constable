// SPDX-License-Identifier: GPL-2.0
#ifndef CONSTABLE_RBAC_ROTATE_H
#define CONSTABLE_RBAC_ROTATE_H

#define RBAC_ROTATION_LIMIT 1024U

int rbac_rotate_files(const char *filename, unsigned int generations);

#endif /* CONSTABLE_RBAC_ROTATE_H */
