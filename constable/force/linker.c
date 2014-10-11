/**
 * @file linker.c
 * @short simple elf linker. Converts elf file into raw data.
 *
 * (c)2000-2002 by Marek Zelem <marek@terminus.sk>
 * $Id: linker.c,v 1.3 2003/12/08 20:21:08 marek Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include "linker.h"

static int add_drel(modules_t * m, int pos)
{
	if (m->drel_tab == NULL)
		m->drel_len = 0;
	m->drel_len++;
	if ((m->drel_tab = realloc(m->drel_tab, m->drel_len * sizeof(u32))) == NULL)
		return (-1);
	m->drel_tab[m->drel_len - 1] = pos;
	return (0);
}

static int align16(int len)
{
	return ((len + 0x0f) & ~0x0f);
}
static int mem_alloc(modules_t * m, int size)
{
	int r;
	if (m->mem == NULL)
		m->memsize = sizeof(struct fctab_s);
	r = m->memsize;
	m->memsize += size;
	if ((m->mem = realloc(m->mem, m->memsize)) == NULL)
		return (-1);
	return (r);
}
#define mem_addr(m,pos)	((char*)((m)->mem+(pos)))

static int load_segments(modules_t * m, elf_t * elf)
{
	int s, n;
	char *sstr = NULL;
	elfmap_t map;
	n = 0;
	elf_for_each_section(elf, s) {
		if ((elf_sect(elf, s).sh_flags & SHF_ALLOC)
//                      && elf_sect(elf,s).sh_type!=SHT_NOBITS 
		    && elf_sect(elf, s).sh_size > 0
		    && elf_sect(elf, s).sh_addr == 0)
			n++;
	}
	if (n == 0)
		return (0);
	if ((m->segment = malloc(n * sizeof(struct s_modseg))) == NULL)
		 return (-1);
	m->nr_seg = n;
	sstr = elf_read_section(elf, elf->head.e_shstrndx);
	n = 0;
	elf_for_each_section(elf, s) {
		if ((elf_sect(elf, s).sh_flags & SHF_ALLOC)
//                      && elf_sect(elf,s).sh_type!=SHT_NOBITS
		    && elf_sect(elf, s).sh_size > 0
		    && elf_sect(elf, s).sh_addr == 0) {
			m->segment[n].name = strdup(
							   (sstr == NULL ? "???" : sstr + elf_sect(elf, s).sh_name));
			m->segment[n].prot = PROT_READ;
			if (elf_sect(elf, s).sh_flags & SHF_EXECINSTR)
				m->segment[n].prot |= PROT_EXEC;
			if (elf_sect(elf, s).sh_flags & SHF_WRITE)
				m->segment[n].prot |= PROT_WRITE;
			m->segment[n].section = s;
			map.map_addr = NULL;
			if (elf_map_section(elf, s, &map, PROT_READ) == NULL) {
				free(m->segment[n].name);
				if (sstr != NULL)
					free(sstr);
				m->nr_seg = n;
				printf("load_segments: cant elf_map_sect: sect=%d\n", s);
				return (-1);
			}
			m->segment[n].size = align16(map.len);
			m->segment[n].pos = mem_alloc(m, m->segment[n].size);
			memcpy(mem_addr(m, m->segment[n].pos), map.base, map.len);
			elf_unmap(&map);
			n++;
		}
	}
	if (sstr != NULL)
		free(sstr);
	return (0);
}

struct s_modsym *modules_get_symbol(modules_t * mod, char *name)
{
	static struct s_modsym ms;
	int i;
	if (name == NULL || name[0] == 0)
		return (NULL);
#define	exp_sym(x,y)	if( !strcmp(name,#x) ) {\
				ms.name=NULL;\
				ms.value=((long)(&(((struct fctab_s*)0)->y)));\
				ms.segment=0;\
				ms.type=0;\
				return(&ms);\
			}

	exp_sym(__version, version)
	    else
		exp_sym(__start, start)
		    else
		exp_sym(__drel_tab, drel_tab)
		    else
		exp_sym(__length, total_len)
		    else
		exp_sym(__argc, argc)
		    else
		exp_sym(__argv, argv)
	    if (mod->sym != NULL && mod->symnames != NULL) {
		for (i = 0; i < mod->nr_sym; i++) {
			if (!strcmp(name, mod->sym[i].name))
				return (&(mod->sym[i]));
		}
	}
	return (NULL);
}

static int link_symbols(modules_t * m, elf_t * elf,
			Elf32_Sym * sym, int nr_syms, int symtab)
{
	int s, a, si;
	struct s_modseg *seg;
	struct s_modsym *msym;
	sym[0].st_value = 0;	/* first symbol is not valid !!! */
	for (s = 1; s < nr_syms; s++) {
		a = sym[s].st_shndx;
		if (a > 0 && a < SHN_LORESERVE) {
			if ((seg = m->segment) == NULL)
				goto Undef;
			for (si = 0; si < m->nr_seg && seg[si].section != a;)
				si++;
			seg += si;
			if (si >= m->nr_seg || seg->section != a || seg->pos < 0)
				goto Undef;
			else {
				sym[s].st_value += seg->pos;
				if (ELF32_ST_BIND(sym[s].st_info) == STB_GLOBAL) {
					if (m->sym == NULL) {
						m->sym_size = 16;
						m->nr_sym = 0;
						if ((m->sym = malloc(m->sym_size
								     * sizeof(struct s_modsym))) == NULL)
							 return (E_OTHER);
					}
					if (m->nr_sym >= m->sym_size) {
						m->sym_size += 16;
						if ((m->sym = realloc(m->sym, m->sym_size
								      * sizeof(struct s_modsym))) == NULL)
							 return (E_OTHER);
					}
					m->sym[m->nr_sym].name = (m->symnames) +
					    (sym[s].st_name);
					m->sym[m->nr_sym].value = sym[s].st_value;
					m->sym[m->nr_sym].size = sym[s].st_size;
					m->sym[m->nr_sym].segment = si;
					m->sym[m->nr_sym].type = 0;
					m->nr_sym++;
				}
			}
		} else if (a == 0) {
			if ((msym = modules_get_symbol(m,
			      (m->symnames) + (sym[s].st_name))) == NULL)
				goto Undef;
			else
				sym[s].st_value = msym->value;
		} else
	      Undef:{
			sym[s].st_value = 0;	/* undefined */
			if (a == 0) {
				if (m->undef == NULL) {
					m->undef_size = 4;
					m->undef_len = 0;
					if ((m->undef = malloc(m->undef_size
							       * sizeof(struct s_modsym))) == NULL)
						 return (E_OTHER);
				}
				if (m->undef_len >= m->undef_size) {
					m->undef_size += 4;
					if ((m->undef = realloc(m->undef, m->undef_size
								* sizeof(struct s_modsym))) == NULL)
						 return (E_OTHER);
				}
				m->undef[m->undef_len].name = (m->symnames) +
				    (sym[s].st_name);
				m->undef[m->undef_len].value = 0;
				m->undef[m->undef_len].size = 0;
				m->undef[m->undef_len].segment = 0;
				m->undef[m->undef_len].type = 0;
				m->undef_len++;
			}
		}
	}
	return (0);
}

static int prereloc_module(modules_t * m, elf_t * elf,
			   Elf32_Sym * sym, int nr_syms, int symtab)
{
	int s, a, i;
	struct s_modseg *seg;
	Elf32_Rel *rel = NULL;
	Elf32_Rela *rela = NULL;
	elfmap_t map;
	int rels = 0;
	int pos;
	int sy, rtype;
	long addend;
	map.map_addr = NULL;
	elf_for_each_section(elf, s) {
		if (elf_sect(elf, s).sh_type == SHT_REL
		    || elf_sect(elf, s).sh_type == SHT_RELA) {
			if (elf_sect(elf, s).sh_link != symtab) {
				printf("Bad symbol table!\n");
				return (E_BADVERSION);
			}
			a = elf_sect(elf, s).sh_info;
			if ((seg = m->segment) == NULL)
				return (E_OTHER);
			for (i = 0; i < m->nr_seg && seg[i].section != a;)
				i++;
			seg += i;
			if (i >= m->nr_seg || seg->section != a) {
//                              printf("Segment not allocated!\n");
				continue;
			}
			if (elf_sect(elf, s).sh_type == SHT_REL) {
				if (elf_sect(elf, s).sh_entsize != sizeof(Elf32_Rel))
			      Bad_entsize:{
					printf("Bad rel(a) entsize!\n");
					continue;
				}
				if ((rel = elf_map_section(elf, s, &map, PROT_READ))
				    == NULL)
					return (E_OTHER);
				rels = map.len / sizeof(Elf32_Rel);
			} else {
				if (elf_sect(elf, s).sh_entsize != sizeof(Elf32_Rela))
					goto Bad_entsize;
				if ((rela = elf_map_section(elf, s, &map, PROT_READ))
				    == NULL)
					return (E_OTHER);
				rels = map.len / sizeof(Elf32_Rela);
			}
			for (i = 0; i < rels; i++) {
				if (rel != NULL) {
					pos = ((seg->pos) + (rel[i].r_offset));
					sy = ELF32_R_SYM(rel[i].r_info);
					rtype = ELF32_R_TYPE(rel[i].r_info);
					addend = *((long *) (m->mem + pos));
				} else {
					pos = ((seg->pos) + (rela[i].r_offset));
					sy = ELF32_R_SYM(rela[i].r_info);
					rtype = ELF32_R_TYPE(rela[i].r_info);
					addend = rela[i].r_addend;
				}
				if (sy >= nr_syms || sym[sy].st_value == 0) {
					int us;
					if (m->rel == NULL) {
						m->rel_size = 4;
						m->rel_len = 0;
						m->rel = malloc(m->rel_size
								* sizeof(struct s_modrel));
					}
					if (m->rel_len >= m->rel_size) {
						m->rel_size = 16 + m->rel_len;
						m->rel = realloc(m->rel, m->rel_size
								 * sizeof(struct s_modrel));
					}
					us = 0;
					if (m->undef == NULL)
						goto Undef;
					while (us < m->undef_len) {
						if (m->rel != NULL
						    && !strcmp(m->undef[us].name,
							       (m->symnames) + (sym[sy].st_name))) {
							m->rel[m->rel_len].pos = pos;
							m->rel[m->rel_len].addend = addend;
							m->rel[m->rel_len].symbol = us;
							m->rel[m->rel_len].type = rtype;
							*((long *) (m->mem + pos)) = 0;
							m->rel_len++;
							goto Next;
						}
						us++;
					}
				      Undef:
					printf("undefined symbol(%d): \"%s\"\n", sy, (m->symnames) + (sym[sy].st_name));
					goto Err;
				      Next:continue;
				}
				switch (rtype) {
				case R_386_32:
					*((long *) (m->mem + pos)) =
					    (sym[sy].st_value) + addend;
					if (add_drel(m, pos) < 0)
						goto Err;
					break;
				case R_386_PC32:
					*((long *) (m->mem + pos)) =
					    (sym[sy].st_value) + addend - pos;
					break;
				case R_386_GLOB_DAT:
				case R_386_JMP_SLOT:
					*((long *) (m->mem + pos)) =
					    (sym[sy].st_value);
					if (add_drel(m, pos) < 0)
						goto Err;
					break;
				case R_386_RELATIVE:
					*((long *) (m->mem + pos)) =
					    ((long) (seg->pos)) + addend;
					if (add_drel(m, pos) < 0)
						goto Err;
					break;
				default:
					printf("Unimplemented reloc type!\n");
				      Err:if (rel != NULL) {
						elf_unmap(&map);
						rel = NULL;
					}
					if (rela != NULL) {
						elf_unmap(&map);
						rela = NULL;
					}
					return (E_UNDEF);
				}
//printf("rel: %s = %lx\n",(m->symnames)+(sym[sy].st_name),*pos);
			}
			if (rel != NULL) {
				elf_unmap(&map);
				rel = NULL;
			}
			if (rela != NULL) {
				elf_unmap(&map);
				rela = NULL;
			}
		}
	}
	add_drel(m, -1);
	return (0);
}


static void free_modules_t(modules_t * m);

modules_t *modules_load_module(const char *filename,
			       const char *name, int status)
{
	elf_t *elf = NULL;
	int r, symtab;
	modules_t *m = NULL;
	Elf32_Sym *sym = NULL;
	elfmap_t sym_map;
	if (name == NULL || name[0] == 0)
		name = filename;
	if (strrchr(name, '/') != NULL)
		name = strrchr(name, '/') + 1;
	sym_map.map_addr = NULL;
	if ((elf = elf_open(filename)) == NULL)
		return (NULL);
	if ((r = elf_test(elf)) < 0)
		goto Err;
	if (elf->head.e_type != ET_REL) {
		r = E_BADTYPE;
		goto Err;
	}
	if ((m = malloc(sizeof(modules_t))) == NULL) {
		r = E_OTHER;
		goto Err;
	}
	m->status = 0;		/* status & MS_SET_MASK; */
	m->next = NULL;
	m->prev = NULL;
	m->map_symnames.map_addr = NULL;
	m->name = NULL;
	m->symnames = NULL;
	m->sym = NULL;
	m->segment = NULL;
	m->mem = NULL;
	m->drel_tab = NULL;
	m->memsize = 0;
	m->drel_len = 0;
	m->undef = NULL;
	m->undef_len = 0;
	m->undef_size = 0;
	m->rel = NULL;
	m->rel_len = 0;
	m->rel_size = 0;
	if ((m->name = strdup(name)) == NULL) {
		r = E_OTHER;
		goto Err;
	}
	elf_for_each_section(elf, symtab) {
		if (elf_sect(elf, symtab).sh_type == SHT_SYMTAB)
			break;
	}
	if ((sym = elf_map_section(elf, symtab, &sym_map, PROT_READ | PROT_WRITE))
	    == NULL) {
		r = E_BADTYPE;
		goto Err;
	}
	if ((m->symnames = elf_map_section(elf, elf_sect(elf, symtab).sh_link,
			       &(m->map_symnames), PROT_READ)) == NULL) {
		r = E_BADTYPE;
		goto Err;
	}
	m->sym = NULL;
	m->nr_sym = 0;
	m->sym_size = 0;
	m->segment = NULL;
	m->nr_seg = 0;
	if (load_segments(m, elf) < 0) {
		r = E_OTHER;
		goto Err;
	}
	if ((r = link_symbols(m, elf, sym, sym_map.len / sizeof(Elf32_Sym), symtab)) < 0)
		goto Err;
	if ((r = prereloc_module(m, elf, sym, sym_map.len / sizeof(Elf32_Sym), symtab)) < 0)
		goto Err;
//segments_hexdump(m);
	r = 0;
	if (m->rel != NULL)
		goto Err;
	goto Koniec;

      Err:if (r == E_UNDEF)
		modules_print_undef(0, m);
	if (m != NULL)
		free_modules_t(m);
	m = NULL;
      Koniec:if (sym != NULL)
		elf_unmap(&(sym_map));
	if (elf != NULL)
		elf_close(elf);
	return (m);
}

static void free_modules_t(modules_t * m)
{
	int i;
	if (m->name != NULL)
		free(m->name);
	if (m->symnames != NULL)
		elf_unmap(&(m->map_symnames));
	if (m->sym != NULL)
		free(m->sym);
	if (m->segment != NULL) {
		for (i = 0; i < m->nr_seg; i++) {
			if (m->segment[i].name != NULL)
				free(m->segment[i].name);
		}
		free(m->segment);
	}
	if (m->undef != NULL)
		free(m->undef);
	if (m->rel != NULL)
		free(m->rel);
	if (m->mem != NULL)
		free(m->mem);
	if (m->drel_tab != NULL)
		free(m->drel_tab);
	free(m);
}

struct fctab_s *object_to_string(const char *filename)
{
	modules_t *m;
	char *str;
	int len;
	struct s_modsym *msym;
	struct fctab_s *fc;
	if ((m = modules_load_module(filename, NULL, 0)) == NULL)
		return (NULL);
	if ((msym = modules_get_symbol(m, "main")) == NULL) {
		free_modules_t(m);
		return (NULL);
	}
	len = m->memsize + (m->drel_len * sizeof(u32));
	if ((str = malloc(len)) == NULL) {
		free_modules_t(m);
		return (NULL);
	}
	fc = (struct fctab_s *) str;
	memcpy(str, m->mem, m->memsize);
	memcpy(str + m->memsize, m->drel_tab, m->drel_len * sizeof(u32));
	fc->version = 0x0100;
	fc->main = msym->value;
	fc->start = 0;		/*sizeof(struct fctab_s); */
	fc->drel_tab = m->memsize;
	fc->total_len = len;
	fc->argc = 0;
	fc->argv = len;
	free_modules_t(m);
	return (fc);
}

int modules_print_undef(int fd, modules_t * mod)
{
	long i;
	char buf[128];
	snprintf(buf, 128, "undefined symbols:");
	write(fd, buf, strlen(buf));
	if (mod->undef != NULL) {
		for (i = 0; i < mod->undef_len; i++) {
			snprintf(buf, 128, " %s", mod->undef[i].name);
			write(fd, buf, strlen(buf));
		}
	}
	write(fd, "\n", 1);
	return (0);
}

#ifdef TEST_LINKER
/*
   char dloader[]={
   0xe8,0x00,0x00,0x00,0x00, //call   5 <begin+5>
   0x5d,                  //popl   %ebp
   0x81,0xc5,0x24,0x00,0x00, //addl   $0x24,%ebp
   0x00,
   0x8b,0x75,0x0c,                //movl   0xc(%ebp),%esi
   0x01,0xee,             //addl   %ebp,%esi
   0x8b,0x3e,             //movl   (%esi),%edi
   0x83,0xff,0xff,        //cmpl   $0xffffffff,%edi
   0x74,0x09,             //je     21 <begin+21>
   0x01,0xef,             //addl   %ebp,%edi
   0x01,0x2f,             //addl   %ebp,(%edi)
   0x83,0xc6,0x04,        //addl   $0x4,%esi
   0xeb,0xf0,             //jmp    11 <begin+11>
   0x8b,0x45,0x04,        //movl   0x4(%ebp),%eax
   0x01,0xe8,             //addl   %ebp,%eax
   0xff,0xd0,             //call   *%eax
   0xc3                   //ret    
   };
 */
#include "dloader.c"
void call_buf(char *buf)
{
	((void (*)(void)) (buf)) ();

}
int exec_str(struct fctab_s *fc, int argc, int *argv)
{
	char *buf;
	int l;
	l = sizeof(dloader) + fc->total_len + argc * 4;
	l += PAGE_SIZE - 1;
	l &= ~(PAGE_SIZE - 1);
	buf = mmap(NULL, l,
		   PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (buf == NULL || buf == (char *) (-1))
		return (-1);
	memcpy(buf, dloader, sizeof(dloader));
	memcpy(buf + sizeof(dloader), fc, fc->total_len);
	((struct fctab_s *) (buf + sizeof(dloader)))->argc = (u32) argc;
	if (argv != NULL)
		memcpy(buf + sizeof(dloader) + fc->total_len, argv, argc * 4);
	printf("Runing...\n");
//      call_buf(buf);
	__asm__(
		       "pushl %%eax\n\t"
		       "pushl %%esi\n\t"
		       "pushl %%edi\n\t"
		       "pushl %%ebp\n\t"
		       "call %%eax\n\t"
		       "popl %%ebp\n\t"
		       "popl %%edi\n\t"
		       "popl %%esi\n\t"
		       "popl %%eax\n\t"
      : : "a"(buf):   "memory", "cc");
	munmap(buf, l);
	return (0);
}

int main(int argc, char *argv[])
{
	struct fctab_s *fc;
	int a[5];
	a[0] = 0xaa;
	a[1] = 0xbb;
	if ((fc = object_to_string(argv[1])) == NULL) {
		printf("ERROR\n");
		return (0);
	}
	printf("loaded.\n");
	if (exec_str(fc, 2, a) < 0) {
		printf("RUN ERROR\n");
		return (0);
	}
	free(fc);
	return (0);
}
#endif
