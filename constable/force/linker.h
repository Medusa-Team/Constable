/**
 * @file linker.h
 * @short header file for simple elf linker.
 *
 * (c)2000-2002 by Marek Zelem <marek@terminus.sk>
 * $Id: linker.h,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#ifndef _MODULES_H
#define _MODULES_H

#include <linux/types.h>
#include "elf.h"

typedef       uintptr_t  u32;

#define	E_UNDEF		-200
#define	E_NOINIT	-201	/* init_module doesn't exist */
#define	E_NOMOD		-210
#define	E_MODEXIST	-211
#define	E_MODUNINIT	-212	/* uninitialized module */
#define	E_MODSYMUSED	-213	/* mod->used!=NULL */
#define	E_MODINUSE	-214	/* mod->usecount!=0 */

#define	E_IMPOSIBLE	-500	/* can't be ! (shouldn't) */

typedef struct {
    char *name;
    void *addr;
} mod_syms_t;

#define SF(x)	{#x,(x)}
#define	SD(x)	{#x,&(x)}
#ifndef SE
#define	SE	{NULL,NULL}
#endif				/* SE */

struct s_modsym {
    char *name;		/* none */
    long value;
    u_long size;
    u_short segment;
    u_short type;		/* unused */
};

struct s_modrel {
    int pos;
    long addend;
    int symbol;
    int type;
};

struct s_modseg {
    char *name;		/* free */
    int pos;
    long size;
    int prot;
    int section;
};

typedef struct s_modules_t {
    struct s_modules_t *next;
    struct s_modules_t *prev;
    char *mem;		/* free */
    int memsize;
    u32 *drel_tab;		/* free */
    int drel_len;
    int status;
    char *name;		/* free */
    char *symnames;
    elfmap_t map_symnames;	/* unmap */
    struct s_modsym *sym;	/* free */
    u_long nr_sym, sym_size;
    struct s_modseg *segment;	/* free */
    u_long nr_seg;
    struct s_modsym *undef;	/* free */
    long undef_len, undef_size;
    struct s_modrel *rel;	/* free */
    long rel_len, rel_size;
} modules_t;

struct s_modsym *modules_get_symbol(modules_t * me, char *name);

/* If exists function int __loaded_module(void),
 * then it calls it, if it returns < 0 then error
 * If function doesn't exist and exist undefined used symbols, then err
 */
modules_t *modules_load_module(const char *filename,
                               const char *name, int status);

struct fctab_s *object_to_string(const char *filename);

int modules_print_undef(int fd, modules_t * mod);

struct fctab_s {	/* also defined in dloader.S */
    /*+00*/	u32 version;
    /*+04*/	u32 main;
    /*+08*/	u32 start;		/* dloader set this to begin of this struct */
    /*+0c*/	u32 drel_tab;		/* reloc. table  (dloader relocate this) */
    /*+10*/	u32 total_len;
    /*+14*/	u32 argc;
    /*+18*/	u32 argv;		/* dloader relocate this */
    /*+1c*/	u32 reserved;
};

#endif				/* _MODULES_H */
