/**
 * @file variables.h
 * @short Header file for variables handling
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: variables.h,v 1.3 2002/10/23 10:25:43 marek Exp $
 */

#ifndef _VARIABLES_H
#define _VARIABLES_H

#include "../object.h"
#include "../types.h"

struct object_s *alloc_var( char *name, struct medusa_attribute_s *attr, struct class_s * class );
struct object_s *data_alias_var( char *name, struct object_s *o );
struct object_s *free_var( struct object_s *v );
void free_vars( struct object_s **v );
struct object_s *var_link( struct object_s **v, struct object_s *var );
struct object_s *get_var( struct object_s *v, char *name );

#endif /* _VARIABLES_H */
