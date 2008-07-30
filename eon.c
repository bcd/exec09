
#include "machine.h"
#include "eon.h"

/**
 * Initialize the EON machine.
 */
void eon_init (const char *boot_rom_file)
{
	/* The MMU must be initialized first, as all other devices
	that are attached can try to hook into the MMU. */
	mmu_init ();
	ram_create (0x100000); /* 1MB */

	if (boot_rom_file)
		rom_create (boot_rom_file);

	console_init ();
	disk_init ("disk.bin");
}

