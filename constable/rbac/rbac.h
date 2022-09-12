/**
 * @file rbac.h
 * @short
 *
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: rbac.h,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#ifndef _RBAC_H
#define _RBAC_H

#include "../tree.h"
#include "../object.h"
#include <sys/types.h>
#include <pthread.h>

struct user_assignment_s;
struct permission_assignment_s;
struct hierarchy_s;

struct role_s;
#define	USER_MAX_ROLES	32

struct user_s {				/* U - users */
    struct object_s object;
    struct user_s *next;
    struct user_assignment_s *ua;
    struct role_s *roles[USER_MAX_ROLES];
    int nr_roles;
    uid_t uid;
    vs_t vs[VS_WORDS];
    //	uintptr_t cinfo;
    struct tree_s *cinfo;
    char name[32];
};

struct perm_s {				/* P - permissions */
    //	struct object_s object;
    int access;
    char space[32];
    vs_t vs[VS_WORDS];
    //	uintptr_t cinfo;
    struct tree_s *cinfo;
};

struct role_s {				/* R - roles */
    struct object_s object;
    struct role_s *next;
    int flag;			/* for rbac_save */
    struct user_assignment_s *ua;
    struct permission_assignment_s *perm;
    struct hierarchy_s *sup;
    struct hierarchy_s *sub;
    vs_t vs_sub[VS_WORDS];		/* member for sup -> sub hier.*/
    vs_t vs[NR_ACCESS_TYPES][VS_WORDS]; /* PA - permission assignment */
    //	uintptr_t cinfo_sup;
    struct tree_s *cinfo_sup;
    //	uintptr_t cinfo_sub;
    struct tree_s *cinfo_sub;
    char name[64];
};


struct user_assignment_s {		/* UA - user assignment */
    struct user_assignment_s *next_user;
    struct user_assignment_s *next_role;
    struct user_s *user;
    struct role_s *role;
};

struct permission_assignment_s {
    struct permission_assignment_s *next;
    int n;
    int access[8];
    struct space_s *space[8];
};

struct hierarchy_s {			/* H - hierarchy */
    struct hierarchy_s *next_sup;
    struct hierarchy_s *next_sub;
    struct role_s *sup_role;
    struct role_s *sub_role;
};


/* Obmedzenia */


/* Funkcie */

extern struct class_s *rbac_user_class,*rbac_perm_class,*rbac_role_class,*rbac_ROLE_class;
extern struct tree_type_s *rbac_t_user,*rbac_t_perm,*rbac_t_role,*rbac_t_ROLE;
extern struct user_s *rbac_users;
extern pthread_rwlock_t rbac_roles_lock;
extern struct role_s *rbac_roles;
extern struct comm_s *rbac_comm;

extern int rbac_roles_need_reinit;
int rbac_roles_reinit( void );
#define VALIDATE_ROLES()                                        \
    do {                                                        \
        pthread_rwlock_rdlock(&rbac_roles_lock);                \
        if(rbac_roles_need_reinit) {                            \
            pthread_rwlock_unlock(&rbac_roles_lock);            \
            rbac_roles_reinit();                                \
        }                                                       \
        pthread_rwlock_unlock(&rbac_roles_lock);                \
    } while(0)

struct user_s * rbac_user_add( char *name, uid_t uid );
struct user_s * rbac_user_find( char *name );
struct user_s * rbac_user_find_by_uid( uid_t uid );

struct role_s * rbac_role_add( char *name );
int rbac_role_del( struct role_s *role );
struct role_s * rbac_role_find( char *name );
//int rbac_all_roles_add_vs( const vs_t *vs );
int rbac_add_ua( struct user_s *user, struct role_s *role );
int rbac_del_ua( struct user_s *user, struct role_s *role );
//void rbac_inherit_sub( struct role_s *r );
void rbac_inherit_sup( struct role_s *r );
int rbac_set_hierarchy( struct role_s *sup_role, struct role_s *sub_role );
int rbac_del_hierarchy( struct role_s *sup_role, struct role_s *sub_role );
int rbac_role_add_perm( struct role_s *role, int which, struct space_s *t );
int rbac_role_del_perm( struct role_s *role, int which, struct space_s *t );

/* ?????????????????????????????????
          +- users --- <user> \\  --- <role>
          |
          +- roles --- <role> \\ --- <user>
          |
    rbac -+- role --- <role> --- <role> ...
          |
          +- ROLE --- <role> --- <role> ...
          |
          +- permission --- <space> --- [RECEIVE,SEND,SEE,CREATE,ERASE,ENTER,CONTROL]


 rbac/role - rekurzivne clenstvo smerom k podrolam
 rbac/ROLE - rekurzivne clenstvo smerom k nadrolam (je to vobec potrebne )
 - schopnosti vzdy smerom k nadrolam.
 
Pravo: priradene roli v konf subore:
    dvojica: typ,space
    typ: AT_RECEIVE AT_SEND AT_SEE AT_CREATE AT_ERASE AT_ENTER AT_CONTROL

    AT_CONTROL rbac/user/<user>	- pravo priradovat pouzivatelovi role
    AT_CONTROL rbac/role/<rola>	- pravo pridavat role pouzivatelov
    AT_CONTROL rbac/role/<rola>	- pravo pridavat/uberat prava
    AT_CONTROL rbac/permission	- pravo pridavat/uberat prava

    AT_CREATE rbac/role		- pravo vytvarat role
    AT_CREATE rbac/ROLE		- pravo vytvarat role
    AT_CREATE rbac/role/<rola>/...	- pravo vytvarat podrole
    AT_CREATE rbac/ROLE/<rola>/...	- pravo vytvarat nadrole
    AT_ERASE - ako AT_CREATE ale rusit.
 */

int rbac_get_user_vs( vs_t *vs, struct user_s *user );
int rbac_get_role_vs( vs_t *vs, struct role_s *role );

//extern struct rbac_rusers_s *r_user;
//extern struct rbac_rrole_s *r_role,*r_ROLE;
//extern struct rbac_perm_s *r_perm;

int rbac_object_init( void );
int rbac_adm_perm_init( void );

extern char *rbac_conffile;
int rbac_save( char *file, int rotate );
int rbac_adm( struct comm_buffer_s *to_wait, char *op, char *s1, char *s2, char *s3 );

#endif /* _RBAC_H */

