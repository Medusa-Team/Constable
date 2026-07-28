/* SPDX-License-Identifier: GPL-2.0 */
#ifndef CONSTABLE_MEDUSA_OBJECT_H
#define CONSTABLE_MEDUSA_OBJECT_H

#include <stddef.h>
#include <stdint.h>

#include <linux/medusa.h>

/*
 * Constable's policy engine uses dense object images described at runtime.
 * These are internal descriptors, not wire structures.  Protocol v4 converts
 * every frame and TLV explicitly before constructing them.
 */
typedef uint64_t MCPptr_t;
typedef uint64_t Mptr_t;

#define MEDUSA_ATTRNAME_MAX	27
#define MEDUSA_CLASSNAME_MAX	30
#define MEDUSA_OPNAME_MAX	30

struct medusa_attribute_s {
	uint16_t offset;
	uint16_t length;
	uint8_t type;
	char name[MEDUSA_ATTRNAME_MAX];
};

#define MED_TYPE_END		MEDUSA_ATTR_END
#define MED_TYPE_UNSIGNED	MEDUSA_ATTR_UNSIGNED
#define MED_TYPE_SIGNED		MEDUSA_ATTR_SIGNED
#define MED_TYPE_STRING		MEDUSA_ATTR_STRING
#define MED_TYPE_BITMAP		MEDUSA_ATTR_BITMAP
#define MED_TYPE_BYTES		MEDUSA_ATTR_BYTES
#define MED_TYPE_READ_ONLY	0x80U
#define MED_TYPE_PRIMARY_KEY	0x40U
#define MED_TYPE_LITTLE_ENDIAN	0x30U
#define MED_TYPE_BIG_ENDIAN	0x20U

struct medusa_class_s {
	MCPptr_t classid;
	uint16_t size;
	char name[MEDUSA_CLASSNAME_MAX];
};

struct medusa_acctype_s {
	MCPptr_t opid;
	uint16_t size;
	uint16_t actbit;
	MCPptr_t op_class[2];
	char name[MEDUSA_OPNAME_MAX];
	char op_name[2][MEDUSA_ATTRNAME_MAX];
};

/* Historical internal source names; none of these are protocol structures. */
#define medusa_comm_attribute_s medusa_attribute_s
#define medusa_comm_class_s medusa_class_s
#define medusa_comm_acctype_s medusa_acctype_s

#define MED_ATTR_END { 0, 0, MED_TYPE_END, "" }
#define MED_ATTR_x(c, attr, attr_name, attr_type)			\
	{ (uint16_t)offsetof(c, attr), sizeof(((c *)0)->attr),		\
	  attr_type, attr_name }
#define MED_ATTR_UNSIGNED(c, attr, name)				\
	MED_ATTR_x(c, attr, name, MED_TYPE_UNSIGNED)
#define MED_ATTR_SIGNED(c, attr, name)					\
	MED_ATTR_x(c, attr, name, MED_TYPE_SIGNED)
#define MED_ATTR_STRING(c, attr, name)					\
	MED_ATTR_x(c, attr, name, MED_TYPE_STRING)
#define MED_ATTR_BITMAP(c, attr, name)					\
	MED_ATTR_x(c, attr, name, MED_TYPE_BITMAP)

#endif /* CONSTABLE_MEDUSA_OBJECT_H */
