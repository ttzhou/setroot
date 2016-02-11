#include <stdlib.h>
#include <errno.h>

#define MEMORY_ERROR "Not enough memory for allocation."

#define hextoint(hh) (int) strtol(hh, NULL, 16)
#define streq(str, test) !strcmp(str, test)

void
die( unsigned int error_code, char* error_msg )
{
	fprintf(stderr, "Error in %s at line %d: %s\n", __FILE__, __LINE__, error_msg );
	exit(error_code);
}

void
verify( void *ptr )
{
	if (ptr == NULL) { die( 1, MEMORY_ERROR ); }
}

void
tfargs_error( const char* flag )
{
	fprintf(stderr, "Not enough arguments provided to '%s'. "
					"Call 'man setroot' to see proper invocation sequence.\n",
					flag);
	exit(1);
}

void
invalid_img_error( const char* path )
{
	fprintf(stderr, "Could not load image '%s'. "
					"Exiting with status 1.\n",
					path);
	exit(1);
}

int
parse_int( const char* intstring )
{
	char *invalid_digits;
	long int val = strtol(intstring, &invalid_digits, 10);

	/* if we don't get an integer */
	if (invalid_digits[0] != '\0') {
		fprintf(stderr, "'%s' is not a float. "
				"Exiting with status 1.\n",
				intstring);
		exit(1);
	} return val;
}

float
parse_float( const char* floatstring )
{
	char *invalid_digits;
	float val = strtof(floatstring, &invalid_digits);

	/* if we don't get an integer */
	if (invalid_digits[0] != '\0') {
		fprintf(stderr, "'%s' is not a float. "
				"Exiting with status 1.\n",
				floatstring);
		exit(1);
	} return val;
}
