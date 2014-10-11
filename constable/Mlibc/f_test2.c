#include "fc_std.h"
#include "mlibc.h"

void main(int argc, int *argv)
{
	int i;
	printf("Force: argc=%d : ", argc);
	for (i = 0; i < argc; i++)
		printf("%d ", argv[i]);
	printf("\n");
}
