#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main ( int argc, char** argv )
{
	if ( argc > 1 && !strcmp(argv[1], "0") )
	{
		printf("%s: not making an address error, exiting normally...\n", argv[0]);
		return 0;
	}

	char* s = (char*)malloc(100);
	free(s);
	strcpy(s, "Hello world!");
	printf("string is: %s\n", s);
	return 0;
}