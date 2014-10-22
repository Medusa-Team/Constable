/**
 * @file elf.c
 * @short Header file for parsing elf file format
 *
 * (c)2000 by Marek Zelem <marek@terminus.sk>
 * $Id: elf.h,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#ifndef _ELF_H
#define _ELF_H

#ifdef linux
#include <linux/elf.h>
#else
#include <machine/elf.h>
#endif

/*
 *
 * start section appended by mY
 * taken from /usr/include/elf.h
 *
 */

/* Intel 80386 specific definitions.  */

/* i386 relocs.  */

#define R_386_NONE	   0		/* No reloc */
#define R_386_32	   1		/* Direct 32 bit  */
#define R_386_PC32	   2		/* PC relative 32 bit */
#define R_386_GOT32	   3		/* 32 bit GOT entry */
#define R_386_PLT32	   4		/* 32 bit PLT address */
#define R_386_COPY	   5		/* Copy symbol at runtime */
#define R_386_GLOB_DAT	   6		/* Create GOT entry */
#define R_386_JMP_SLOT	   7		/* Create PLT entry */
#define R_386_RELATIVE	   8		/* Adjust by program base */
#define R_386_GOTOFF	   9		/* 32 bit offset to GOT */
#define R_386_GOTPC	   10		/* 32 bit PC relative offset to GOT */

/*
 *
 * end section appended by mY
 *
 */


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
