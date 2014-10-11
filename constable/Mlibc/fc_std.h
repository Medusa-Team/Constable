/* fc_std.h
 * Standard force code implicit library
 * by Marek Zelem (c)5.10.1999
 * $Id: fc_std.h,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

extern int __version;		/* version of fc start code */
extern int *__drel_tab;		/* relocation table */
extern char *__start;		/* pointer to begin of forced code */
extern int __length;		/* total length of forced code */
extern int __argc;		/* fc main argc */
extern int *__argv;		/* fc main argv */

/* main function of force code */
void main( int argc, int *argv );

