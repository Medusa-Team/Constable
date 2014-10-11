/*
 * test_c_pre
 * $Id: test_c_pre.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include <stdio.h>
#include <unistd.h>
#include <mcompiler/compiler.h>
#include <mcompiler/c_language.h>

int main( int argc, char *argv[] )
{ struct compiler_preprocessor_class *pre;
  char c;
	pre=c_preprocessor_create(argv[1]);
	if( pre==NULL )
	{	printf("Bad file\n");
		return(0);
	}
	while( pre->get_char(pre,&c)>=0 )
	{	write(1,&c,1);
	}
	dec_use(pre);
	return(0);
}

