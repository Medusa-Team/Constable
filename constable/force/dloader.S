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
.dl0:	pop   %ebp
	add   $.fctab-.dl0,%ebp
	mov   %ebp,START(%ebp)
	mov   DREL_TAB(%ebp),%esi
	add   %ebp,%esi
	mov   %esi,DREL_TAB(%ebp)
.dl_rel:
	mov   (%esi),%edi
	cmp   $0xffffffffffffffff,%edi
	je     .dl_relocated
	add   %ebp,%edi
	add   %ebp,(%edi)
	add   $0x4,%esi
	jmp    .dl_rel
.dl_relocated:
	mov   ARGV(%ebp),%esi
	add   %ebp,%esi
	mov   %esi,ARGV(%ebp)
	mov   ARGC(%ebp),%eax
	shl   $3,%eax
	add   ARGC,%esi
.dl_do_arg:
	mov   (%esi),%eax
	sub   $0x4,%esi
	cmp   $0,%eax
	je     .dl_do_arg_end
	mov   (%esi),%edi
	sub   $0x4,%esi
	cmp   $1,%eax
	je     .dl_do_arg_noreloc
	add   %ebp,%edi
.dl_do_arg_noreloc:
	push   %edi
	jmp    .dl_do_arg
.dl_do_arg_end:
	mov   MAIN(%ebp),%eax
	add   %ebp,%eax
	call   *%eax
	mov   ARGC(%ebp),%eax
	shl    $2,%eax
	add   %eax,%esp
	ret
.align 4
.fctab:
.globl dloader_size;
dloader_size:
	.long    .fctab-dloader

