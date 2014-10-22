/**
 * @file elf.c
 * @short Routines for parsing elf file format
 *
 * (c)2000 by Marek Zelem <marek@terminus.sk>
 * $Id: elf.c,v 1.3 2005/02/12 18:39:08 marek Exp $
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "elf.h"
#include <elf.h>
//#include <mcompiler/print.h>

#include <errno.h>
#include <string.h>

typedef struct {
	char *name;
	int num;
} p_stab_t;

#define S(x)    {#x,x}
#define SE      {NULL,-1}


int elf_errno;

elf_t *elf_open(const char *filename)
{
	elf_t *e;
	elf_errno = E_OTHER;
	if ((e = malloc(sizeof(elf_t))) == NULL)
		return NULL;
	if ((e->fd = open(filename, O_RDONLY)) < 0) {
		free(e);
		return NULL;
	}
	if (read(e->fd, &(e->head), sizeof(Elf32_Ehdr)) !=
	    sizeof(Elf32_Ehdr)) {
		free(e);
		return NULL;
	}
	if (e->head.e_ident[EI_MAG0] != ELFMAG0
	    || e->head.e_ident[EI_MAG1] != ELFMAG1
	    || e->head.e_ident[EI_MAG2] != ELFMAG2
	    || e->head.e_ident[EI_MAG3] != ELFMAG3
	    || e->head.e_ehsize != sizeof(Elf32_Ehdr)) {
		free(e);
		elf_errno = E_NOTELF;
		return NULL;
	}
	e->st_len = 0;
	e->st = NULL;
	if (e->head.e_shoff && e->head.e_shentsize == sizeof(Elf32_Shdr)) {
		e->st_len = e->head.e_shnum;
		if ((e->st = malloc(sizeof(Elf32_Shdr) * e->st_len)) ==
		    NULL) {
			free(e);
			return NULL;
		}
		if (lseek(e->fd, e->head.e_shoff, SEEK_SET) < 0) {
			free(e->st);
			free(e);
			return NULL;
		}
		if (read(e->fd, e->st, sizeof(Elf32_Shdr) * e->st_len)
		    != sizeof(Elf32_Shdr) * e->st_len) {
			free(e->st);
			free(e);
			return NULL;
		}
	}
	elf_errno = 0;
	return e;
}

int elf_close(elf_t * elf)
{
	if (elf == NULL)
		return 0;
	if (elf->fd >= 0)
		close(elf->fd);
	if (elf->st != NULL)
		free(elf->st);
	free(elf);
	return 0;
}

int elf_test(elf_t * elf)
{
	if (elf->head.e_ident[EI_CLASS] != ELFCLASS32)
		return E_BADMACHINE;
	if (elf->head.e_ident[EI_DATA] != ELFDATA2LSB)
		return E_BADMACHINE;
	if (elf->head.e_ident[EI_VERSION] != EV_CURRENT)
		return E_BADVERSION;
	if (elf->head.e_machine != EM_386)
		return E_BADMACHINE;
	if (elf->head.e_version != EV_CURRENT)
		return E_BADVERSION;
	return 0;
}

void *elf_read_section(elf_t * elf, int section)
{
	char *buf;
	long len;
	elf_errno = E_BADSECTION;
	if (elf->st == NULL || section >= elf->st_len)
		return NULL;
	if (elf->st[section].sh_type == SHT_NULL
	    || elf->st[section].sh_type == SHT_NOBITS)
		return NULL;
	elf_errno = E_OTHER;
	if (lseek(elf->fd, elf->st[section].sh_offset, SEEK_SET) < 0)
		return NULL;
	len = elf->st[section].sh_size;
	if ((buf = malloc(len)) == NULL)
		return NULL;
	if (read(elf->fd, buf, len) != len) {
		free(buf);
		return NULL;
	}
	elf_errno = 0;
	return buf;
}

void *elf_map_section(elf_t * elf, int section, elfmap_t * map, int prot)
{
	char *buf;
	long l, lp;
	long page;
	long offp, off;
	page = getpagesize();
	elf_errno = E_BADSECTION;
	if (elf->st == NULL || section >= elf->st_len)
		return NULL;
	if (elf->st[section].sh_type == SHT_NULL)
//          || elf->st[section].sh_type==SHT_NOBITS )
		return NULL;
	elf_errno = E_OTHER;
	off = elf->st[section].sh_offset;
	if (elf->st[section].sh_type == SHT_NOBITS)
		off = 0;
	offp = ((off) / page) * page;
	off -= offp;
	l = elf->st[section].sh_size;
	lp = (((l + off) + page - 1) / page) * page;
	if (elf->st[section].sh_type != SHT_NOBITS)
		buf = mmap(NULL, lp, prot, MAP_PRIVATE, elf->fd, offp);
	else {
		buf = mmap(NULL, lp, prot, MAP_PRIVATE | MAP_ANON, -1, 0);
		if (buf != NULL && buf != (char *) (-1))
			memset(buf, 0, lp);
	}
	if (buf == (char *) (-1)) {
//              printf("mmap (section=%d) error: %s (len=%ld off=%ld)\n",
		//                      section,strerror(errno),lp,offp);
		return NULL;
	}
	if (buf == NULL)
		return NULL;
	if (map != NULL) {
		map->map_addr = buf;
		map->map_len = lp;
		map->base = buf + off;
		map->len = l;
	}
	elf_errno = 0;
	return buf + off;
}
int elf_unmap(elfmap_t * map)
{
	int r;
	if (map->map_addr == NULL)
		return 0;
	r = munmap(map->map_addr, map->map_len);
	map->map_addr = NULL;
	map->map_len = 0;
	return r;
}

/*
   void *elf_map_section( elf_t *elf, int section, elfmap_t *map, int prot )
   { char *buf;
   long l,lp;
   long page;
   long offp,off;
   page=getpagesize();
   elf_errno=E_BADSECTION;
   if( elf->st==NULL || section >= elf->st_len )
   return(NULL);
   if(    elf->st[section].sh_type==SHT_NULL
   || elf->st[section].sh_type==SHT_NOBITS )
   return(NULL);
   elf_errno=E_OTHER;
   off=elf->st[section].sh_offset;
   offp=((off)/page)*page;
   off-=offp;
   if( lseek(elf->fd,offp,SEEK_SET)<0 )
   return(NULL);
   ..... dokoncit ....
   l=elf->st[section].sh_size;
   lp=((l+page-1)/page)*page;
   if( (buf=malloc(lp))==NULL )
   return(NULL);
   if( read(elf->fd,buf,l)!=l )
   {    free(buf);
   return(NULL);
   }
   elf_errno=0;
   if( len!=NULL )
   *len=l;
   if( mprotect(buf,lp,prot)<0 )
   {    printf("mprotect (addr=%p section=%d len=%ld) error: %s\n",
   buf,section,lp,strerror(errno));
   free(buf);
   return(NULL);
   }
   return(buf);
   }
   int elf_unmap( elfmap_t *map )
   {
   free(addr);
   return(0);
   }
 */

/* printouts */

p_stab_t t_e_type[] = {
	S(ET_NONE), S(ET_REL), S(ET_EXEC), S(ET_DYN),
	S(ET_CORE), SE
};
p_stab_t t_e_machine[] = {
	S(EM_NONE), S(EM_M32), S(EM_SPARC), S(EM_386),
	S(EM_68K), S(EM_88K), S(EM_486), S(EM_860),
	S(EM_MIPS), /* S(EM_MIPS_RS4_BE), */
#ifdef	EM_SPARC64
	S(EM_SPARC64),
#endif
	S(EM_PARISC),
#ifdef EM_SPARC32PLUS
	S(EM_SPARC32PLUS),
#endif
	S(EM_PPC), S(EM_ALPHA),
	SE
};

p_stab_t t_shn[] = {
	S(SHN_ABS), S(SHN_COMMON),
	SE
};
p_stab_t t_sh_type[] = {
	S(SHT_NULL), S(SHT_PROGBITS), S(SHT_SYMTAB), S(SHT_STRTAB),
	S(SHT_RELA), S(SHT_HASH), S(SHT_DYNAMIC), S(SHT_NOTE),
	S(SHT_NOBITS), S(SHT_REL), S(SHT_SHLIB), S(SHT_DYNSYM),
#ifdef SHT_NUM
	S(SHT_NUM),
#endif
	SE
};
p_stab_t t_sh_flags[] = {
	S(SHF_WRITE), S(SHF_ALLOC), S(SHF_EXECINSTR), SE
};
/*
   int elf_print_elfhead( elf_t *elf )
   {
   printf(      "ELF HEAD: ident: CLASS=%d DATA=%d VERSION=%d PAD=%d\n",
   elf->head.e_ident[EI_CLASS],
   elf->head.e_ident[EI_DATA],
   elf->head.e_ident[EI_VERSION],
   elf->head.e_ident[EI_PAD]);
   printf(      "          type=%s", print_num2sym(t_e_type,elf->head.e_type));
   printf(                        " machine=%s version=%ld\n",
   print_num2sym(t_e_machine,elf->head.e_machine),
   (long)(elf->head.e_version));
   printf(      "          entry=0x%x phoff=%d shoff=%d flags=0x%x\n"
   "          ehsize=%d phentsize=%d phnum=%d\n"
   "          shentsize=%d shnum=%d shstrndx=%d\n",
   elf->head.e_entry,
   elf->head.e_phoff,elf->head.e_shoff,elf->head.e_flags,
   elf->head.e_ehsize,elf->head.e_phentsize,elf->head.e_phnum,
   elf->head.e_shentsize,elf->head.e_shnum,elf->head.e_shstrndx);
   return(0);
   }

   int elf_print_sections( elf_t *elf )
   { int i;
   char *str;
   if( elf->st==NULL )  return(-1);
   str=elf_read_section(elf,elf->head.e_shstrndx);
   for(i=0;i<elf->st_len;i++)
   {
   printf(      "SECTION %2d name=\"%s\" type=%s flags=%s\n"
   "        addr=0x%x offset=%d size=%d link=%d "
   "info=%d addralign=%d entsize=%d\n",
   i,
   (str==NULL ? "???" : str+elf->st[i].sh_name ),
   print_num2sym(t_sh_type,elf->st[i].sh_type),
   print_num2orsym(t_sh_flags,elf->st[i].sh_flags),
   elf->st[i].sh_addr, elf->st[i].sh_offset,
   elf->st[i].sh_size, elf->st[i].sh_link,
   elf->st[i].sh_info, elf->st[i].sh_addralign,
   elf->st[i].sh_entsize );
   }
   if( str!=NULL )              free(str);
   return(0);
   }
 */
p_stab_t t_stb[] = {
	S(STB_LOCAL), S(STB_GLOBAL), S(STB_WEAK), SE
};
p_stab_t t_stt[] = {
	S(STT_NOTYPE), S(STT_OBJECT), S(STT_FUNC), S(STT_SECTION),
	S(STT_FILE), SE
};
/*
   int elf_print_symbols( elf_t *elf )
   { int sec,i,n;
   char *sstr=NULL;
   char *str=NULL;
   Elf32_Sym *sym=NULL;
   if( elf->st==NULL )  return(-1);
   sstr=elf_read_section(elf,elf->head.e_shstrndx);
   for(sec=0;sec<elf->st_len;sec++)
   {  if( elf->st[sec].sh_type==SHT_SYMTAB
   && elf->st[sec].sh_entsize==sizeof(Elf32_Sym)
   && (sym=(Elf32_Sym*)elf_read_section(elf,sec))!=NULL
   && (str=elf_read_section(elf,elf->st[sec].sh_link))!=NULL )
   {    n= (elf->st[sec].sh_size) / (elf->st[sec].sh_entsize);
   printf("SECTION %2d name=\"%s\" type=%s flags=%s\n", sec,
   (sstr==NULL ? "???" : sstr+elf->st[sec].sh_name ),
   print_num2sym(t_sh_type,elf->st[sec].sh_type),
   print_num2orsym(t_sh_flags,elf->st[sec].sh_flags) );
   for(i=0;i<n;i++)
   {    printf("\t%3d) %s 0x%08x size=%d info=%s",i,
   str+sym[i].st_name,  sym[i].st_value,
   sym[i].st_size,
   print_num2sym(t_stb,ELF32_ST_BIND(sym[i].st_info))
   );
   printf(",%s\n\t\tother=%d shndx=\"%s\"",
   print_num2sym(t_stt,ELF32_ST_TYPE(sym[i].st_info)),
   sym[i].st_other,
   ((sstr==NULL||sym[i].st_shndx>=SHN_LORESERVE)
   ? "???" : sstr+elf->st[sym[i].st_shndx].sh_name )
   );
   printf("(%s)\n",
   print_num2sym(t_shn,sym[i].st_shndx)
   );
   }
   }
   if( sym!=NULL )      { free(sym); sym=NULL; }
   if( str!=NULL )      { free(str); str=NULL; }
   }
   if( sstr!=NULL )             free(sstr);
   return(0);
   }
 */
p_stab_t t_r_type[] = {
	S(R_386_NONE), S(R_386_32), S(R_386_PC32), S(R_386_GOT32),
	S(R_386_PLT32), S(R_386_COPY), S(R_386_GLOB_DAT),
	    S(R_386_JMP_SLOT),
	S(R_386_RELATIVE), S(R_386_GOTOFF), S(R_386_GOTPC),
#ifdef R_386_NUM
	S(R_386_NUM),
#endif
	SE
};
/*
   int elf_print_reloc( elf_t *elf )
   { int sec,i,n;
   char *sstr=NULL;
   char *str=NULL;
   Elf32_Sym *sym=NULL;
   Elf32_Rel *rel=NULL;
   Elf32_Rela *rela=NULL;
   if( elf->st==NULL )  return(-1);
   sstr=elf_read_section(elf,elf->head.e_shstrndx);
   for(sec=0;sec<elf->st_len;sec++)
   {  if( elf->st[sec].sh_type!=SHT_REL && elf->st[sec].sh_type!=SHT_RELA )
   continue;
   if( elf->st[sec].sh_type==SHT_REL
   && elf->st[sec].sh_entsize!=sizeof(Elf32_Rel) )
   continue;
   if( elf->st[sec].sh_type==SHT_RELA
   && elf->st[sec].sh_entsize!=sizeof(Elf32_Rela) )
   continue;
   if( (rel=elf_read_section(elf,sec))!=NULL
   && (sym=elf_read_section(elf,elf->st[sec].sh_link))!=NULL
   && (str=elf_read_section(elf,elf->st[elf->st[sec].sh_link].sh_link))!=NULL )
   {    n= (elf->st[sec].sh_size) / (elf->st[sec].sh_entsize);
   if( elf->st[sec].sh_type==SHT_RELA )
   {    rela=(Elf32_Rela*)rel;  rel=NULL;       }
   printf("SECTION %2d name=\"%s\" type=%s flags=%s\n", sec,
   (sstr==NULL ? "???" : sstr+elf->st[sec].sh_name ),
   print_num2sym(t_sh_type,elf->st[sec].sh_type),
   print_num2orsym(t_sh_flags,elf->st[sec].sh_flags) );
   for(i=0;i<n;i++)
   {    printf("\t0x%x: type=%s sym=%d",
   (rel!=NULL?rel[i].r_offset:rela[i].r_offset),
   print_num2sym(t_r_type,
   ELF32_R_TYPE(rel!=NULL?rel[i].r_info:rela[i].r_info)),
   ELF32_R_SYM(rel!=NULL?rel[i].r_info:rela[i].r_info) );
   if( rela!=NULL )
   printf(" addend=%d",rela[i].r_addend);
   printf("\n");
   }
   }
   if( rel!=NULL )      { free(rel); rel=NULL; }
   if( rela!=NULL )     { free(rela); rela=NULL; }
   if( sym!=NULL )      { free(sym); sym=NULL; }
   if( str!=NULL )      { free(str); str=NULL; }
   }
   if( sstr!=NULL )             free(sstr);
   return(0);
   }
 */
