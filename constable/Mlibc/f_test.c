#include "fc_std.h"
#include "mlibc.h"

void main(int argc, int *argv)
{
	printf("%16d %8d\n", strtol("  +  -  45     ", NULL, 0), getuid());
	exit(0);

}
