/*
 * Constable: medusa_object.h
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: medusa_object.h,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#ifndef _MEDUSA_COMM_H
#define _MEDUSA_COMM_H

//#include <medusa/l3/arch.h>
#include <stdint.h>

/*
 * the following constants and structures cover the standard
 * communication protocol.
 *
 * this means: DON'T TOUCH! Not only you will break the comm
 * protocol, but also the code which relies on particular
 * facts about them.
 */

typedef uint64_t MCPptr_t; // medusa common protocol pointer type this is here because we wanna have one protocol for all architectures JK March 2015
typedef uint64_t Mptr_t; // medusa pointer if you want to run effectivly medusa you should use something like coid* :) for debuggin purposes you should use mcptr_t :) JK March 2015

/* version of this communication protocol */
#define MEDUSA_COMM_VERSION	1

#define MEDUSA_COMM_ATTRNAME_MAX	(32-5)
#define MEDUSA_COMM_CLASSNAME_MAX	(32-2)
#define MEDUSA_COMM_OPNAME_MAX		(32-2)

/* comm protocol commands. 'k' stands for kernel, 'c' for constable. */

#define MEDUSA_COMM_AUTHREQUEST		0x01	/* k->c */
#define MEDUSA_COMM_AUTHANSWER		0x81	/* c->k */

#define MEDUSA_COMM_CLASSDEF		0x02	/* k->c */
#define MEDUSA_COMM_CLASSUNDEF		0x03	/* k->c */
#define MEDUSA_COMM_ACCTYPEDEF		0x04	/* k->c */
#define MEDUSA_COMM_ACCTYPEUNDEF	0x05	/* k->c */

#define MEDUSA_COMM_FETCH_REQUEST	0x88	/* c->k */
#define MEDUSA_COMM_FETCH_ANSWER	0x08	/* k->c */
#define MEDUSA_COMM_FETCH_ERROR		0x09	/* k->c */

#define MEDUSA_COMM_UPDATE_REQUEST	0x8a	/* c->k */
#define MEDUSA_COMM_UPDATE_ANSWER	0x0a	/* k->c */

#pragma pack(push, 1)
struct medusa_comm_attribute_s {
    u_int16_t offset;			/* offset of attribute in object */
    u_int16_t length;			/* bytes consumed by data */
    u_int8_t type;				/* data type (MED_COMM_TYPE_xxx) */
    char name[MEDUSA_COMM_ATTRNAME_MAX];	/* string: attribute name */
};
#pragma pack(pop)

#define	MED_COMM_TYPE_END		0x00	/* end of attribute list */
#define	MED_COMM_TYPE_UNSIGNED		0x01	/* unsigned integer attr */
#define	MED_COMM_TYPE_SIGNED		0x02	/* signed integer attr */
#define	MED_COMM_TYPE_STRING		0x03	/* string attr */
#define	MED_COMM_TYPE_BITMAP		0x04	/* bitmap attr */

#define	MED_COMM_TYPE_READ_ONLY		0x80	/* this attribute is read-only */
#define	MED_COMM_TYPE_PRIMARY_KEY	0x40	/* this attribute is used to lookup object */

#pragma pack(push, 1)
struct medusa_comm_class_s {
    MCPptr_t	classid;	/* (1,2,...): unique identifier of this class */
    u_int16_t	size;		/* size of object */
    char		name[MEDUSA_COMM_CLASSNAME_MAX];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct medusa_comm_acctype_s {
    MCPptr_t	opid;
    u_int16_t	size;
    u_int16_t	actbit;	/* 0x8000 - means subject */
    MCPptr_t	op_class[2];
    char		name[MEDUSA_COMM_OPNAME_MAX];
    char		op_name[2][MEDUSA_COMM_ATTRNAME_MAX];
};
#pragma pack(pop)

/* for Constable internal use */
#define	MEDUSA_ATTRNAME_MAX	MEDUSA_COMM_ATTRNAME_MAX
#define	MEDUSA_CLASSNAME_MAX	MEDUSA_COMM_CLASSNAME_MAX
#define	MEDUSA_OPNAME_MAX	MEDUSA_COMM_OPNAME_MAX
#define	medusa_attribute_s	medusa_comm_attribute_s	
#define	medusa_class_s		medusa_comm_class_s
#define	medusa_acctype_s	medusa_comm_acctype_s
#define	MED_TYPE_END		MED_COMM_TYPE_END
#define	MED_TYPE_UNSIGNED	MED_COMM_TYPE_UNSIGNED
#define	MED_TYPE_SIGNED		MED_COMM_TYPE_SIGNED
#define	MED_TYPE_STRING		MED_COMM_TYPE_STRING
#define	MED_TYPE_BITMAP		MED_COMM_TYPE_BITMAP

#define	MED_TYPE_READ_ONLY	MED_COMM_TYPE_READ_ONLY
#define	MED_TYPE_PRIMARY_KEY	MED_COMM_TYPE_PRIMARY_KEY

#define MED_ATTR_END			{0,0,MED_TYPE_END,""}
#define	MED_ATTR_x(c,attr,name,type)	{ (u_int16_t)(uintptr_t)(&(((c*)(0))->attr)), sizeof(((c*)(0))->attr), type, name }
#define	MED_ATTR_UNSIGNED(c,attr,name)	MED_ATTR_x(c,attr,name,MED_TYPE_UNSIGNED)
#define	MED_ATTR_SIGNED(c,attr,name)	MED_ATTR_x(c,attr,name,MED_TYPE_SIGNED)
#define	MED_ATTR_STRING(c,attr,name)	MED_ATTR_x(c,attr,name,MED_TYPE_STRING)
#define	MED_ATTR_BITMAP(c,attr,name)	MED_ATTR_x(c,attr,name,MED_TYPE_BITMAP)
/* end of Constable internal */

#endif
