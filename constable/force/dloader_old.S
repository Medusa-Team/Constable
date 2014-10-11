/** 
 * @file dloader_old.S 
 * @short old Force code dynamic loader
 * allows only integer arguments
 *
 * (c)5.10.1999 by Marek Zelem
 * $Id: dloader_old.S,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

/* struct fctab_s */
VERSION		= 0x00
MAIN		= 0x04
START		= 0x08
DREL_TAB	= 0x0C
TOTAL_LEN	= 0x10
ARGC		= 0x14
ARGV		= 0x18
RESERVED	= 0x1C

	.data
.globl dloader_old;
	.align 4;
dloader_old:
	call   .dl0
.dl0:	popq   %rbp
	addq   $.fctab-.dl0,%rbp
	movq   %rbp,START(%ebp)
	movq   DREL_TAB(%rbp),%rsi
	addq   %rbp,%rsi
	movq   %rsi,DREL_TAB(%rbp)
.dl_rel:
	movq   (%rsi),%rdi
	cmpq   $0xffffffffffffffff,%rdi
	je     .dl_relocated
	addq   %rbp,%rdi
	addq   %rbp,(%rdi)
	addq   $0x4,%rsi
	jmp    .dl_rel
.dl_relocated:
	movq   ARGV(%rbp),%rax
	addq   %rbp,%rax
	movq   %rax,ARGV(%rbp)
	pushq  %rax
	movq   ARGC(%rbp),%rax
	pushq  %rax
	movq   MAIN(%rbp),%rax
	addq   %rbp,%rax
	call   *%rax
	addq   $0x8,%rsp
	ret
.align 4
.fctab:
.globl dloader_old_size;
dloader_old_size:
	.long    .fctab-dloader_old

