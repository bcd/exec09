
/* Needed for write() */
#include <unistd.h>

int main (void)
{
   /* We don't have printf() working yet, so we have to
	 * use the UNIX-like system calls. */
	write (1, "Hello, World\n", 13);
	return (0);
}

