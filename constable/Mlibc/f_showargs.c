/*
 * Example forcing code for use with MLIBC.
 * (C) 2000/02/06 Milan Pikula <www@fornax.sk>
 * $Id: f_showargs.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 *
 * Usage: force "f_showargs" [$argument [...]];
 *
 * This forces displaying of the arguments to the file
 * descriptor 1 of the process. If lucky,
 * the standard output points to your screen.
 *
 * For example, display some numbers after invocation of
 * "/bin/cat" binary.
 *  1. Hook the corresponding action:
 *       for exec "/bin/cat" {
 *               force "f_showargs" 5 6 7;
 *       }
 *  2. Run /bin/cat:
 *    # /bin/cat
 *    Force: argc=3 : 5 6 7
 */

#include "fc_std.h"
#include "mlibc.h"

void main(int argc, int *argv)
{
    int i;
    printf("Force: argc=%d :", argc);
    for (i = 0; i < argc; i++)
        printf(" %d", argv[i]);
    printf("\n");
}

