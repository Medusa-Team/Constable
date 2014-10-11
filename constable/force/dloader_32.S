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
.dl0:	popl   %ebp
	addl   $.fctab-.dl0,%ebp
	movl   %ebp,START(%ebp)
	movl   DREL_TAB(%ebp),%esi
	addl   %ebp,%esi
	movl   %esi,DREL_TAB(%ebp)
.dl_rel:
	movl   (%esi),%edi
	cmpl   $0xffffffff,%edi
	je     .dl_relocated
	addl   %ebp,%edi
	addl   %ebp,(%edi)
	addl   $0x4,%esi
	jmp    .dl_rel
.dl_relocated:
	movl   ARGV(%ebp),%esi
	addl   %ebp,%esi
	movl   %esi,ARGV(%ebp)
	movl   ARGC(%ebp),%eax
	shll   $3,%eax
	addl   ARGC,%esi
.dl_do_arg:
	movl   (%esi),%eax
	subl   $0x4,%esi
	cmpl   $0,%eax
	je     .dl_do_arg_end
	movl   (%esi),%edi
	subl   $0x4,%esi
	cmpl   $1,%eax
	je     .dl_do_arg_noreloc
	addl   %ebp,%edi
.dl_do_arg_noreloc:
	push   %edi
	jmp    .dl_do_arg
.dl_do_arg_end:
	movl   MAIN(%ebp),%eax
	addl   %ebp,%eax
	call   *%eax
	movl   ARGC(%ebp),%eax
	shl    $2,%eax
	addl   %eax,%esp
	ret
.align 4
.fctab:
.globl dloader_size;
dloader_size:
	.long    .fctab-dloader

