
#include "machine.h"
#include "eon.h"

extern int system_running;


void eon_fault (unsigned int addr, unsigned char type)
{
	if (system_running)
	{
		sim_error (">>> Page fault: addr=%04X type=%02X PC=%04X\n", addr, type, get_pc ());
#if 0
		fault_addr = addr;
		fault_type = type;
		irq ();
#endif
	}
}

struct machine eon_machine =
{
	.fault = eon_fault,
};


/**
 * Initialize the EON machine.
 */
void eon_init (const char *boot_rom_file)
{
	struct hw_device *dev;

	machine = &eon_machine;

	/* The MMU must be defined first, as all other devices
	that are attached can try to hook into the MMU. */
	device_define ( mmu_create (), 0,
		MMU_ADDR, BUS_MAP_SIZE, MAP_READWRITE+MAP_FIXED );

	/* A 1MB RAM part is mapped into all of the allowable 64KB
	address space, until overriden by other devices. */
	device_define ( ram_create (RAM_SIZE), 0,
		0x0000, MAX_CPU_ADDR, MAP_READWRITE );

	device_define ( rom_create (boot_rom_file, BOOT_ROM_SIZE), 0,
		BOOT_ROM_ADDR, BOOT_ROM_SIZE, MAP_READONLY );

	device_define ( dev = console_create (), 0,
		CONSOLE_ADDR, BUS_MAP_SIZE, MAP_READWRITE );
	device_define (dev, 0, 
		0xFF00, BUS_MAP_SIZE, MAP_READWRITE );

	device_define ( disk_create ("disk.bin"), 0,
		DISK_ADDR(0), BUS_MAP_SIZE, MAP_READWRITE);
}


/**
 * Initialize the simple machine, which is the default
 * machine that has no bells or whistles.
 */
void simple_init (const char *boot_rom_file)
{
	machine = &eon_machine;
	device_define ( ram_create (RAM_SIZE), 0,
		0x0000, MAX_CPU_ADDR, MAP_READWRITE );
	device_define ( console_create (), 0,
		0xFF00, BUS_MAP_SIZE, MAP_READWRITE );
}

