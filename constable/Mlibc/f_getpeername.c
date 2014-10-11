#include "fc_std.h"
#include "mlibc.h"

#include <linux/net.h>
#include <linux/in.h>

/*
 * Example forcing code for use with MLIBC.
 * (C) 2000/02/06 Milan Pikula <www@fornax.sk>
 * $Id: f_getpeername.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 *
 * Usage: force "f_getpeername" $fd;
 *
 * This forces the getpeername() syscall to get the IP address of
 * host on the remote side of opened socket. This can be used to
 * add some kind of network filter to the applications, which are
 * opened to the world. Value is returned via setup() syscall.
 * 
 * For example, your application accepts the network connections
 * on port 12345 and you want to restrict it, if connection
 * came from an unknown host.
 *  1. Find out, which file descriptor is used in that application
 *     for the incoming connections (in many cases this will be a
 *     constant for the compiled binary; it may vary for some
 *     applications - in that case use peek_user to get the
 *     real value). Type
 *       # strace -ff some_strange_daemon 2>&1 | egrep 'accept.*127\.0'
 *     then connect to it from the second window:
 *       # telnet 0 12345
 *       Trying 0.0.0.0...
 *       Connected to 0.
 *       Escape character is '^]'.
 *     look at the first window. Something like this should appear:
 *       accept(4, {sin_family=AF_INET, sin_port=htons(1029), sin_addr=
 *       inet_addr("127.0.0.1")}, [16]) = 5
 *     here we see, that the connection was accepted on the file descriptor
 *     5. This is our candidate. Great.
 *  2. Pass this file descriptor to this module and force it to the
 *     process, somewhere after accepting the connection. I personally prefer
 *     to hook the fork() call, if the server forks for each client, because
 *     then we will be able to set restrictions to the child only.
 *       trace_on 0; // will be explained later
 *       force "f_getpeername" 5; // actually force this code
 *  3. Catch the result. Because of the nature of code forcing, there is no
 *     return value from the code. How we can get the result? The solution
 *     is simple: we have to do something in our forced code, which can be
 *     hooked in constable. Let's take the system call setup(), which is
 *     used only once per the system lifetime, immediately after boot. We will
 *     use this syscall from our forced code and catch it in constable via
 *     'on syscall'. It's number is 0. Now you see the reason of the
 *     "trace_on 0" above. We will pass the IP and errno as an arguments to
 *     this fictive system  call.
 *     Well, now we can make some security decision. Remember, that
 *     the address is in the network byte order (most significiant byte first).
 *     Address 192.168.1.2 (C0.A8.01.02 hex) will be 0x0201A8C0 on Intel
 *     systems.
 *       on syscall {
 *               if (action == 0) {
 *                       if (trace2 == 0 && trace1 == 0x0201A8C0) {
				 // ERRNO is not set
 *                               // and the address matches
 *                               log "Granting access to the " pid;
 *                       } else {
 *                               log "Unauthorised access from IP " trace1;
 *                               force "f_exit"; // or do something less harmful
 *                       }
 *               }               
 *       }
 */

/* Uncomment this, if the application have tty at the standard output fd */
/* #define FD1_IS_TTY */
 
void main(int argc, int *argv)
{
	struct sockaddr_in addr;
	int addrlen = sizeof(addr);

	unsigned long socketcall_args[6];

	if (argc != 1) {
#ifdef FD1_IS_TTY
		printf("GETPEERNAME: requires file descriptor\n");
#endif
		return;
	}
	socketcall_args[0] = (unsigned long)(argv[0]);
	socketcall_args[1] = (unsigned long)(&addr);
	socketcall_args[2] = (unsigned long)(&addrlen);
	if (socketcall(SYS_GETPEERNAME, socketcall_args) == 0)
		setup((int)(addr.sin_addr.s_addr), 0, 0, 0);
	else
		setup(0, errno, 0, 0);
}
