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
.dl0:	pop    %ebp
	add    $.fctab-.dl0,%ebp
	mov    %ebp,START(%ebp)
	mov    DREL_TAB(%ebp),%esi
	add    %ebp,%esi
	mov    %esi,DREL_TAB(%ebp)
.dl_rel:
	mov    (%esi),%edi
	cmp    $0xffffffffffffffff,%edi
	je     .dl_relocated
	add    %ebp,%edi
	add    %ebp,(%edi)
	add    $0x4,%esi
	jmp    .dl_rel
.dl_relocated:
	mov    ARGV(%ebp),%eax
	add    %ebp,%eax
	mov    %eax,ARGV(%ebp)
	push   %eax
	mov    ARGC(%ebp),%eax
	push   %eax
	mov    MAIN(%ebp),%eax
	add    %ebp,%eax
	call   *%eax
	add    $0x8,%esp
	ret
.align 4
.fctab:
.globl dloader_old_size;
dloader_old_size:
	.long    .fctab-dloader_old

