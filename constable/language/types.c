// SPDX-License-Identifier: GPL-2.0
/**
 * @file types.c
 * @short Functions for data types
 *
 * (c)2004 by Marek Zelem <marek@terminus.sk>
 */

#include "language.h"
#include "execute.h"
#include "../constable.h"
#include "../object.h"
#include "../comm.h"

static struct medusa_class_s def_class_string = {
	0xfffffff0,	/* classid */
	MAX_REG_SIZE,	/* size */
	"string"	/* name */
};

static struct medusa_attribute_s def_attr_string[] = {
	{0, MAX_REG_SIZE, MED_TYPE_STRING, "string"},
	{0, 0, MED_TYPE_END, ""}
};

static struct medusa_class_s def_class_integer = {
	0xfffffff1,	/* classid */
	4,		/* size */
	"integer"	/* name */
};

static struct medusa_attribute_s def_attr_integer[] = {
	{0, MAX_REG_SIZE, MED_TYPE_UNSIGNED, "unsigned"},
	{0, MAX_REG_SIZE, MED_TYPE_SIGNED, "signed"},
	{0, 0, MED_TYPE_END, ""}
};

int language_init_comm_datatypes(struct comm_s *comm)
{
	if (add_class(comm, &def_class_string, def_attr_string) == NULL)
		comm_info("comm %s: Can't add default class 'string'", comm->name);
	if (add_class(comm, &def_class_integer, def_attr_integer) == NULL)
		comm_info("comm %s: Can't add default class 'integer'", comm->name);
	return 0;
}
