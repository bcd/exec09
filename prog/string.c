
#include <string.h>
#include <libsim.h>

int main (void)
{
	if (strlen ("Testing") != 7)
		return 1;

	if (strlen ("") != 0)
		return 2;

	write (1, "Test passed.\n", 13);
	return 0;
}

