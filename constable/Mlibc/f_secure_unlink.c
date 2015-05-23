#include "fc_std.h"
#include "mlibc.h"

#include <linux/fcntl.h>

/*
 * Example forcing code for use with MLIBC.
 * (C) 2000/02/06 Milan Pikula <www@fornax.sk>
 * $Id: f_secure_unlink.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 *
 * Usage: force "f_secure_unlink" $filename_pointer;
 *
 * This forces the rewrite of file with zeros and 0xff's on unlink,
 * which prevents them from being recovered from the hard drive.
 *
 *  1. Hook the corresponding action:
 *       for exec "/sbin/extra_critical_application" {
 *               flags = 0x1234; // mark the application
 *       }
 *       on syscall {
 *               // /usr/include/asm/unistd.h: __NR_unlink is 10
 *               if (action == 10 && flags == 0x1234)
 *                       force "f_secure_unlink" trace1;
 *       }
 *  2. Discussion:
 *     Is this some kind of a security hole? Not. Forced code runs
 *     with the privilegies of the process, which wanted to 'unlink'.
 *     If it CAN write to the file, but CANNOT unlink it, because
 *     of the write permissions to the directory, it COULD be
 *     abused to re-write contents of that file anyway. THAT is a
 *     security hole.
 */

void main(int argc, int *argv)
{
    int fd, size;
    struct stat st;
    char fillbuf[4096]; /* temporary data storage, from
                   which we write to the file */

    int i;

    if (argc != 1) /* we require one parameter - filename */
        return;

    /* open the given file */
    fd = open((char *)(argv[0]), O_WRONLY | O_SYNC, 0);
    if (fd == -1)
        return;

    /* get the file size */
    if (fstat(fd, &st) == -1) { /* strange... */
        close(fd);
        return;
    }

    /* prepare the fill-buffer */
    for (i=0; i<sizeof(fillbuf); i++)
        fillbuf[i] = '\0';

    /* rewrite the whole file */
    for (size = st.st_size; size > 0; size -= sizeof(fillbuf))
        write(fd, fillbuf, size > sizeof(fillbuf) ?
                  sizeof(fillbuf) : size);

    /* move to the beginning */
    lseek(fd, 0, 0);

    /* prepare new fill-buffer */
    for (i=0; i<sizeof(fillbuf); i++)
        fillbuf[i] = '\xff';

    /* and rewrite it again */
    for (size = st.st_size; size > 0; size -= sizeof(fillbuf))
        write(fd, fillbuf, size > sizeof(fillbuf) ?
                  sizeof(fillbuf) : size);
    close(fd);
}

