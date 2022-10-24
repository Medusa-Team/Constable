// SPDX-License-Identifier: GPL-2.0
/*
 * Constable: access_types.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 */

#include "access_types.h"

char *access_names[NR_ACCESS_TYPES] = {
	"MEMBER",
	"RECEIVE",
	"SEND",
	"SEE",
	"CREATE",
	"ERASE",
	"ENTER",
	"CONTROL",
};
