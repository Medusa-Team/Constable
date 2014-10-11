/**
 * @file elf.c
 * @short Header file for parsing elf file format
 *
 * (c)2000 by Marek Zelem <marek@terminus.sk>
 * $Id: elf.h,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#ifndef __ELF_H_
#define __ELF_H_

#include <elf.h>

/* errors */
#define	E_NOTELF	-1
#define	E_BADMACHINE	-2
#define	E_BADVERSION	-3
#define	E_BADTYPE	-8
#define	E_BADSECTION	-10
#define	E_OTHER		-100

extern int elf_errno;

typedef struct {
	int fd;			/* filedes. of elf file */
	Elf32_Ehdr head;
	int st_len;		/* sections */
	Elf32_Shdr *st;		/* section table */
} elf_t;

typedef struct {
	void *base;
	long len;
	void *map_addr;
	long map_len;
} elfmap_t;

#define elf_for_each_section(elf,sect)	\
	for( (sect)=0 ; (elf)->st!=NULL && (sect)<((elf)->st_len) ; (sect)++ )

#define	elf_sect(elf,sect)	( ((elf)->st[sect]) )


elf_t *elf_open(const char *filename);
int elf_close(elf_t * elf);
int elf_test(elf_t * elf);
void *elf_read_section(elf_t * elf, int section);	/* free */
void *elf_map_section(elf_t * elf, int section, elfmap_t * map, int prot);
int elf_unmap(elfmap_t * map);

int elf_print_elfhead(elf_t * elf);
int elf_print_sections(elf_t * elf);
int elf_print_symbols(elf_t * elf);
int elf_print_reloc(elf_t * elf);


#endif				/* _ELF_H */
