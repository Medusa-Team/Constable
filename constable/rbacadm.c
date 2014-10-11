/*
 * Constable: rbacadm.c
 * (c)2002 by Marek Zelem <marek@terminus.sk>
 * $Id: rbacadm.c,v 1.3 2003/12/08 20:21:08 marek Exp $
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <string.h>
#include <errno.h>

#define __NR_rbac_adm              250
//static inline _syscall5(int,rbac_adm,char *,op,char *,s1,char *,s2,char *,s3,char *,out)

int main( int argc, char *argv[] )
{ char *op="";
  char *s1="",*s2="",*s3="";
  char buf[1024];

	if( argc<2 )
	{	printf("usage: %s <op> [<arg1> [<arg2> [<arg3>]]]\n",argv[0]);
		return(0);
	}
	op=argv[1];
	if( argc>2 )
		s1=argv[2];
	if( argc>3 )
		s2=argv[3];
	if( argc>4 )
		s3=argv[4];
	strcpy(buf,"Not supported by the kernel");
	//rbac_adm(op,s1,s2,s3,buf);
	syscall(__NR_rbac_adm, op, s1, s2, s3, buf);
	printf("Result: %s\n",buf);
	return(0);
}

