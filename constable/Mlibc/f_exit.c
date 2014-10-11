#include "fc_std.h"
#include "mlibc.h"

/*
 * Example forcing code for use with MLIBC.
 * (C) 2000/02/06 Milan Pikula <www@fornax.sk>
 * $Id: f_exit.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 *
 * Usage: force "f_exit" $retval;
 *
 * This forces the exit() syscall to the process. You can specify
 * the return value, with which it will die.
 * 
 * For example, your application does some nasty thing and you want to
 * terminate it.
 *  1. Hook the corresponding action:
 *       for unlink "/tmp/del_or_die" {
 *               force "f_exit" 0;
 *       }
 *  2. There's no step 2. Application suddenly died :)
 */

void main(int argc, int *argv)
{
	if (argc == 1) /* if user supplied an argument, use it */
		exit(argv[0]);
	/* otherwise return 0 */
	exit(0);
}
