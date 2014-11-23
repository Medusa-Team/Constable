/**
 * @file dloader.S
 * @short Force code dynamic loader
 *
 * (c)20.9.2002 by Marek Zelem
 * $Id: dloader.S,v 1.2 2002/10/23 10:25:43 marek Exp $
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
.globl dloader;
	.align 4;
dloader:
	call   .dl0
.dl0:	popq   %rbp
	addq   $.fctab-.dl0,%rbp
	movq   %rbp,START(%rbp)
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
	movq   ARGV(%rbp),%rsi
	addq   %rbp,%rsi
	movq   %rsi,ARGV(%rbp)
	movq   ARGC(%rbp),%rax
	shlq   $3,%rax
	addq   ARGC,%rsi
.dl_do_arg:
	movq   (%rsi),%rax
	subq   $0x4,%rsi
	cmpq   $0,%rax
	je     .dl_do_arg_end
	movq   (%rsi),%rdi
	subq   $0x4,%rsi
	cmpq   $1,%rax
	je     .dl_do_arg_noreloc
	addq   %rbp,%rdi
.dl_do_arg_noreloc:
	push   %rdi
	jmp    .dl_do_arg
.dl_do_arg_end:
	movq   MAIN(%rbp),%rax
	addq   %rbp,%rax
	call   *%rax
	movq   ARGC(%rbp),%rax
	shl    $2,%rax
	addq   %rax,%rsp
	ret
.align 4
.fctab:
.globl dloader_size;
dloader_size:
	.long    .fctab-dloader

