#include <stdlib.h>
#include <errno.h>

#define MEMORY_ERROR "Not enough memory for allocation."

#define hextoint(hh) (int) strtol(hh, NULL, 16)
#define streq(str, test) !strcmp(str, test)

void die( unsigned int error_code, char* error_msg )
{
	fprintf(stderr, "Error in %s at line %d: %s\n", __FILE__, __LINE__, error_msg );
	exit(error_code);
}

void verify( void *ptr )
{
	if (ptr == NULL) { die( 1, MEMORY_ERROR ); }
}
