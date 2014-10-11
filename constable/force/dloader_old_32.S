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
	movl   ARGV(%ebp),%eax
	addl   %ebp,%eax
	movl   %eax,ARGV(%ebp)
	pushl  %eax
	movl   ARGC(%ebp),%eax
	pushl  %eax
	movl   MAIN(%ebp),%eax
	addl   %ebp,%eax
	call   *%eax
	addl   $0x8,%esp
	ret
.align 4
.fctab:
.globl dloader_old_size;
dloader_old_size:
	.long    .fctab-dloader_old

