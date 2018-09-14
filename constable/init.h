/*
 * Constable: init.h
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: init.h,v 1.3 2003/12/08 20:21:08 marek Exp $
 */

#ifndef _INIT_H
#define _INIT_H

struct module_s {
    struct module_s *next;
    char name[64];
    char *filename;
    int(*create_comm)( struct module_s * );
    int(*init_comm)( struct module_s * );
    int(*init_namespace)( struct module_s * );
    int(*init_rules)( struct module_s * );
};

int add_module( struct module_s *module );
struct module_s *activate_module( char *name );

#endif /* _INIT_H */

